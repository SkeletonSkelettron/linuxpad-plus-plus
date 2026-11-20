// This file is part of Notepad++ project
// Copyright (C)2026 Notepad++ Linux port contributors
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// at your option any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

// Notepad++ Linux plugin SDK, npp_win32 library: GDI on QPainter. A device context
// paints on the QPainter of WM_PAINT, or on the QImage of the bitmap selected in a
// memory DC. Icons, bitmaps and cursors come from the resources of the .rc file.

#include <QApplication>
#include <QBuffer>
#include <QCursor>
#include <QFile>
#include <QFontDatabase>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QImage>
#include <QImageReader>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QPixmap>
#include <QRegion>
#include <QScreen>
#include <QStyle>
#include <QTextLayout>

#include "W32Internal.h"

namespace w32
{
	namespace
	{
		struct DcState
		{
			HGDIOBJ pen, brush, font, bitmap;
			COLORREF text, bk;
			int bkMode;
			UINT align;
		};

		struct Dc
		{
			unsigned int magic = 0x4E505044u; // "NPPD"
			QPainter* painter = nullptr;
			bool ownsPainter = false;
			QImage scratch;                 // DCs with nothing to paint on: measures only
			QWidget* widget = nullptr;
			Window* window = nullptr;
			bool memory = false;
			HGDIOBJ pen = nullptr;
			HGDIOBJ brush = nullptr;
			HGDIOBJ font = nullptr;
			HGDIOBJ bitmap = nullptr;
			COLORREF textColor = 0;
			COLORREF bkColor = 0xFFFFFF;
			int bkMode = OPAQUE;
			COLORREF dcBrush = 0xFFFFFF;
			COLORREF dcPen = 0;
			UINT align = 0;
			QPoint pos;
			std::vector<DcState> saved;
		};

		Dc* dcOf(HDC h)
		{
			auto* d = static_cast<Dc*>(h);
			return (d && d->magic == 0x4E505044u) ? d : nullptr;
		}

		HGDIOBJ stockObject(int index);

		QFont defaultFont()
		{
			return QApplication::font();
		}

		QFont fixedFont()
		{
			return QFontDatabase::systemFont(QFontDatabase::FixedFont);
		}

		Gdi* newObject(GdiKind kind)
		{
			auto* g = new Gdi;
			g->kind = kind;
			return g;
		}

		void ensureIconHeader(Gdi* g)
		{
			if (g->header.qicon)
				return;
			if (g->icon)
				g->header.qicon = g->icon;
			else if (g->image)
			{
				g->icon = new QIcon(QPixmap::fromImage(*g->image));
				g->header.qicon = g->icon;
			}
			else if (g->pixmap)
			{
				g->icon = new QIcon(*g->pixmap);
				g->header.qicon = g->icon;
			}
		}

		QPainter* painterOf(Dc* d)
		{
			if (!d)
				return nullptr;
			if (d->painter && d->painter->isActive())
				return d->painter;
			if (d->memory)
				return nullptr;
			if (d->scratch.isNull())
				d->scratch = QImage(1, 1, QImage::Format_ARGB32_Premultiplied);
			if (!d->painter)
			{
				d->painter = new QPainter(&d->scratch);
				d->ownsPainter = true;
			}
			return d->painter;
		}

		QFont dcFont(Dc* d)
		{
			if (Gdi* g = gdi(d ? d->font : nullptr); g && g->kind == GdiKind::Font)
				return g->font;
			if (d && d->widget)
				return d->widget->font();
			return defaultFont();
		}

		QPen dcPenOf(Dc* d)
		{
			Gdi* g = gdi(d->pen);
			if (!g)
				return QPen(Qt::black);
			if (d->pen == stockObject(DC_PEN))
				return QPen(color(d->dcPen));
			if (g->penStyle == PS_NULL)
				return QPen(Qt::NoPen);
			QPen p(g->color, std::max(1, g->width));
			switch (g->penStyle)
			{
				case PS_DASH: p.setStyle(Qt::DashLine); break;
				case PS_DOT: p.setStyle(Qt::DotLine); break;
				case PS_DASHDOT: p.setStyle(Qt::DashDotLine); break;
				case PS_DASHDOTDOT: p.setStyle(Qt::DashDotDotLine); break;
				default: break;
			}
			return p;
		}

		QBrush brushOf(HBRUSH h, Dc* d = nullptr)
		{
			const intptr_t v = reinterpret_cast<intptr_t>(h);
			if (v > 0 && v <= COLOR_MENUBAR + 1)
				return QBrush(sysColor(static_cast<int>(v - 1)));
			if (d && h == stockObject(DC_BRUSH))
				return QBrush(color(d->dcBrush));
			Gdi* g = gdi(h);
			if (!g || g->nullBrush)
				return QBrush(Qt::NoBrush);
			if (g->image)
				return QBrush(*g->image);
			return QBrush(g->color);
		}

		QRect qrect(const RECT* rc)
		{
			return rc ? QRect(rc->left, rc->top, rc->right - rc->left, rc->bottom - rc->top) : QRect();
		}

		QString textOf(LPCWSTR text, int len)
		{
			return qs(text, len < 0 ? -1 : len);
		}

		Qt::CursorShape cursorShape(int id)
		{
			switch (id)
			{
				case 32513: return Qt::IBeamCursor;
				case 32514: return Qt::WaitCursor;
				case 32515: return Qt::CrossCursor;
				case 32516: return Qt::UpArrowCursor;
				case 32642: return Qt::SizeFDiagCursor;
				case 32643: return Qt::SizeBDiagCursor;
				case 32644: return Qt::SizeHorCursor;
				case 32645: return Qt::SizeVerCursor;
				case 32646: return Qt::SizeAllCursor;
				case 32648: return Qt::ForbiddenCursor;
				case 32649: return Qt::PointingHandCursor;
				case 32650: return Qt::BusyCursor;
				case 32651: return Qt::WhatsThisCursor;
				default: return Qt::ArrowCursor;
			}
		}

		QIcon standardIcon(int id)
		{
			QStyle* s = QApplication::style();
			switch (id)
			{
				case 32513: return s->standardIcon(QStyle::SP_MessageBoxCritical);
				case 32514: return s->standardIcon(QStyle::SP_MessageBoxQuestion);
				case 32515: return s->standardIcon(QStyle::SP_MessageBoxWarning);
				case 32516: return s->standardIcon(QStyle::SP_MessageBoxInformation);
				default: return QApplication::windowIcon().isNull() ? s->standardIcon(QStyle::SP_ComputerIcon) : QApplication::windowIcon();
			}
		}

		// all the images of an .ico file (or one image of another format)
		QIcon iconFromData(const QByteArray& data)
		{
			QBuffer buf;
			buf.setData(data);
			buf.open(QIODevice::ReadOnly);
			QImageReader reader(&buf);
			QIcon icon;
			do
			{
				const QImage img = reader.read();
				if (!img.isNull())
					icon.addPixmap(QPixmap::fromImage(img));
			} while (reader.jumpToNextImage());
			return icon;
		}

		QImage transparentBackground(QImage img, bool map3d)
		{
			// LR_LOADTRANSPARENT: the colour of the first pixel; LR_LOADMAP3DCOLORS: the greys of buttons
			img = img.convertToFormat(QImage::Format_ARGB32);
			if (img.isNull())
				return img;
			const QRgb key = img.pixel(0, 0) & 0xFFFFFF;
			for (int y = 0; y < img.height(); ++y)
			{
				auto* line = reinterpret_cast<QRgb*>(img.scanLine(y));
				for (int x = 0; x < img.width(); ++x)
				{
					const QRgb c = line[x] & 0xFFFFFF;
					if (c == key || (map3d && c == 0xC0C0C0))
						line[x] = 0;
				}
			}
			return img;
		}

		HGDIOBJ stockObject(int index)
		{
			static std::map<int, Gdi*> stock;
			auto it = stock.find(index);
			if (it != stock.end())
				return it->second;
			Gdi* g = nullptr;
			switch (index)
			{
				case WHITE_BRUSH: g = newObject(GdiKind::Brush); g->color = Qt::white; break;
				case LTGRAY_BRUSH: g = newObject(GdiKind::Brush); g->color = QColor(192, 192, 192); break;
				case GRAY_BRUSH: g = newObject(GdiKind::Brush); g->color = QColor(128, 128, 128); break;
				case DKGRAY_BRUSH: g = newObject(GdiKind::Brush); g->color = QColor(64, 64, 64); break;
				case BLACK_BRUSH: g = newObject(GdiKind::Brush); g->color = Qt::black; break;
				case NULL_BRUSH: g = newObject(GdiKind::Brush); g->nullBrush = true; break;
				case DC_BRUSH: g = newObject(GdiKind::Brush); g->color = Qt::white; break;
				case WHITE_PEN: g = newObject(GdiKind::Pen); g->color = Qt::white; break;
				case BLACK_PEN: g = newObject(GdiKind::Pen); g->color = Qt::black; break;
				case NULL_PEN: g = newObject(GdiKind::Pen); g->penStyle = PS_NULL; break;
				case DC_PEN: g = newObject(GdiKind::Pen); g->color = Qt::black; break;
				case OEM_FIXED_FONT:
				case ANSI_FIXED_FONT:
				case SYSTEM_FIXED_FONT:
					g = newObject(GdiKind::Font);
					g->font = fixedFont();
					break;
				default:
					g = newObject(GdiKind::Font);
					g->font = defaultFont();
					break;
			}
			g->stock = true;
			if (g->kind == GdiKind::Font)
			{
				g->logFont.lfHeight = -QFontMetrics(g->font).ascent();
				g->logFont.lfWeight = g->font.bold() ? FW_BOLD : FW_NORMAL;
				copyOut(g->font.family(), g->logFont.lfFaceName, LF_FACESIZE);
			}
			stock[index] = g;
			return g;
		}

		void applyFont(QPainter* p, Dc* d)
		{
			p->setFont(dcFont(d));
		}
	}

	Gdi* gdi(HGDIOBJ h)
	{
		auto* g = static_cast<Gdi*>(h);
		if (!g || reinterpret_cast<uintptr_t>(h) <= COLOR_MENUBAR + 1)
			return nullptr;
		return (g->header.magic == NPP_IMAGE_MAGIC) ? g : nullptr;
	}

	HGDIOBJ newGdi(Gdi* g)
	{
		return g;
	}

	QColor color(COLORREF c)
	{
		return QColor(GetRValue(c), GetGValue(c), GetBValue(c));
	}

	COLORREF colorRef(const QColor& c)
	{
		return RGB(c.red(), c.green(), c.blue());
	}

	QColor sysColor(int index)
	{
		const QPalette p = QApplication::palette();
		switch (index)
		{
			case COLOR_WINDOW: return p.color(QPalette::Base);
			case COLOR_WINDOWTEXT: return p.color(QPalette::Text);
			case COLOR_MENU: case COLOR_MENUBAR: return p.color(QPalette::Window);
			case COLOR_MENUTEXT: return p.color(QPalette::WindowText);
			case COLOR_BTNFACE: return p.color(QPalette::Button);
			case COLOR_BTNTEXT: return p.color(QPalette::ButtonText);
			case COLOR_BTNSHADOW: return p.color(QPalette::Mid);
			case COLOR_BTNHIGHLIGHT: return p.color(QPalette::Light);
			case COLOR_3DDKSHADOW: return p.color(QPalette::Shadow);
			case COLOR_3DLIGHT: return p.color(QPalette::Midlight);
			case COLOR_HIGHLIGHT: case COLOR_MENUHILIGHT: case COLOR_ACTIVECAPTION: case COLOR_GRADIENTACTIVECAPTION:
				return p.color(QPalette::Highlight);
			case COLOR_HIGHLIGHTTEXT: case COLOR_CAPTIONTEXT: return p.color(QPalette::HighlightedText);
			case COLOR_GRAYTEXT: case COLOR_INACTIVECAPTIONTEXT: return p.color(QPalette::Disabled, QPalette::Text);
			case COLOR_INFOBK: return p.color(QPalette::ToolTipBase);
			case COLOR_INFOTEXT: return p.color(QPalette::ToolTipText);
			case COLOR_HOTLIGHT: return p.color(QPalette::Link);
			case COLOR_APPWORKSPACE: return p.color(QPalette::Dark);
			case COLOR_WINDOWFRAME: case COLOR_ACTIVEBORDER: case COLOR_INACTIVEBORDER: return p.color(QPalette::Mid);
			case COLOR_INACTIVECAPTION: case COLOR_GRADIENTINACTIVECAPTION: return p.color(QPalette::Inactive, QPalette::Window);
			case COLOR_SCROLLBAR: return p.color(QPalette::Midlight);
			case COLOR_BACKGROUND: return p.color(QPalette::Dark);
			default: return p.color(QPalette::Window);
		}
	}

	HICON iconHandle(const QIcon& icon)
	{
		Gdi* g = newObject(GdiKind::Icon);
		g->icon = new QIcon(icon);
		g->header.qicon = g->icon;
		return g;
	}

	HBITMAP bitmapHandle(const QPixmap& pixmap)
	{
		Gdi* g = newObject(GdiKind::Bitmap);
		g->image = new QImage(pixmap.toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied));
		ensureIconHeader(g);
		return g;
	}

	QIcon iconOf(HANDLE h)
	{
		if (!h)
			return QIcon();
		if (Gdi* g = gdi(h))
		{
			if (g->icon)
				return *g->icon;
			if (g->image)
				return QIcon(QPixmap::fromImage(*g->image));
			if (g->pixmap)
				return QIcon(*g->pixmap);
			return QIcon();
		}
		// a QIcon* of NppPluginQt.h
		return *static_cast<const QIcon*>(h);
	}

	QPixmap pixmapOf(HANDLE h, QSize size)
	{
		if (!h)
			return QPixmap();
		if (Gdi* g = gdi(h))
		{
			if (g->image)
				return QPixmap::fromImage(*g->image);
			if (g->pixmap)
				return *g->pixmap;
			if (g->icon)
			{
				const QList<QSize> sizes = g->icon->availableSizes();
				return g->icon->pixmap(size.isValid() && !size.isEmpty() ? size : sizes.value(sizes.size() - 1, QSize(32, 32)));
			}
			return QPixmap();
		}
		const QIcon icon = iconOf(h);
		return icon.pixmap(size.isValid() && !size.isEmpty() ? size : QSize(32, 32));
	}

	QFont fontOf(HFONT h)
	{
		Gdi* g = gdi(h);
		return (g && g->kind == GdiKind::Font) ? g->font : defaultFont();
	}

	HFONT fontHandle(const QFont& f)
	{
		Gdi* g = newObject(GdiKind::Font);
		g->font = f;
		g->logFont.lfHeight = -QFontMetrics(f).ascent();
		g->logFont.lfWeight = f.bold() ? FW_BOLD : FW_NORMAL;
		g->logFont.lfItalic = f.italic();
		copyOut(f.family(), g->logFont.lfFaceName, LF_FACESIZE);
		return g;
	}

	HDC paintDC(Window* w, QPainter* p, const QRect&)
	{
		auto* d = new Dc;
		d->painter = p;
		d->window = w;
		d->widget = w ? w->widget : nullptr;
		d->font = w ? w->font : nullptr;
		d->pen = stockObject(BLACK_PEN);
		d->brush = stockObject(WHITE_BRUSH);
		if (d->widget)
		{
			d->textColor = colorRef(d->widget->palette().color(QPalette::WindowText));
			d->bkColor = colorRef(d->widget->palette().color(QPalette::Window));
		}
		return d;
	}

	void releasePaintDC(HDC dc)
	{
		Dc* d = dcOf(dc);
		if (!d)
			return;
		if (d->ownsPainter)
		{
			d->painter->end();
			delete d->painter;
		}
		d->magic = 0;
		delete d;
	}

	HDC widgetDC(QWidget* w, QPainter* p)
	{
		Window* win32 = winOf(w);
		HDC dc = paintDC(win32, p, w ? w->rect() : QRect());
		if (Dc* d = dcOf(dc))
			d->widget = w;
		return dc;
	}

	void releaseDC(HDC dc)
	{
		releasePaintDC(dc);
	}
}

using namespace w32;

// ---- device contexts -------------------------------------------------------------------------
HDC GetDC(HWND hWnd)
{
	Window* w = win(hWnd);
	if (w && w->painter)
		return paintDC(w, w->painter, w->widget ? w->widget->rect() : QRect());
	HDC dc = paintDC(w, nullptr, QRect());
	if (Dc* d = dcOf(dc))
	{
		d->widget = widgetOf(hWnd);
		painterOf(d);
	}
	return dc;
}

HDC GetWindowDC(HWND hWnd)
{
	return GetDC(hWnd);
}

HDC GetDCEx(HWND hWnd, HRGN, DWORD)
{
	return GetDC(hWnd);
}

int ReleaseDC(HWND, HDC hdc)
{
	releasePaintDC(hdc);
	return 1;
}

HDC BeginPaint(HWND hWnd, LPPAINTSTRUCT ps)
{
	Window* w = win(hWnd);
	HDC dc = GetDC(hWnd);
	if (ps)
	{
		*ps = PAINTSTRUCT{};
		ps->hdc = dc;
		QWidget* widget = widgetOf(hWnd);
		ps->rcPaint = widget ? RECT{0, 0, widget->width(), widget->height()} : RECT{};
		ps->fErase = FALSE;
	}
	Q_UNUSED(w);
	return dc;
}

BOOL EndPaint(HWND, const PAINTSTRUCT* ps)
{
	if (ps)
		releasePaintDC(ps->hdc);
	return TRUE;
}

HDC CreateCompatibleDC(HDC)
{
	auto* d = new Dc;
	d->memory = true;
	d->pen = stockObject(BLACK_PEN);
	d->brush = stockObject(WHITE_BRUSH);
	d->font = stockObject(DEFAULT_GUI_FONT);
	return d;
}

BOOL DeleteDC(HDC hdc)
{
	Dc* d = dcOf(hdc);
	if (!d)
		return FALSE;
	if (d->ownsPainter && d->painter)
	{
		d->painter->end();
		delete d->painter;
	}
	d->magic = 0;
	delete d;
	return TRUE;
}

HBITMAP CreateCompatibleBitmap(HDC, int cx, int cy)
{
	Gdi* g = newObject(GdiKind::Bitmap);
	g->image = new QImage(std::max(cx, 1), std::max(cy, 1), QImage::Format_ARGB32_Premultiplied);
	g->image->fill(Qt::black);
	return g;
}

HBITMAP CreateBitmap(int cx, int cy, UINT, UINT bitCount, const void* bits)
{
	Gdi* g = newObject(GdiKind::Bitmap);
	g->image = new QImage(std::max(cx, 1), std::max(cy, 1), QImage::Format_ARGB32_Premultiplied);
	g->image->fill(Qt::black);
	if (bits && bitCount == 32)
		std::memcpy(g->image->bits(), bits, static_cast<size_t>(cx) * cy * 4);
	return g;
}

HBITMAP CreateDIBSection(HDC, const BITMAPINFO* bmi, UINT, void** bits, HANDLE, DWORD)
{
	if (!bmi)
		return nullptr;
	const int cx = bmi->bmiHeader.biWidth;
	const int cy = std::abs(bmi->bmiHeader.biHeight);
	Gdi* g = newObject(GdiKind::Bitmap);
	g->image = new QImage(std::max(cx, 1), std::max(cy, 1), QImage::Format_ARGB32_Premultiplied);
	g->image->fill(Qt::transparent);
	if (bits)
		*bits = g->image->bits();
	return g;
}

HGDIOBJ SelectObject(HDC hdc, HGDIOBJ obj)
{
	Dc* d = dcOf(hdc);
	if (!d)
		return nullptr;
	Gdi* g = gdi(obj);
	if (!g)
	{
		// a region (CreateRectRgn) selected as a clip: SelectClipRgn
		return nullptr;
	}
	HGDIOBJ old = nullptr;
	switch (g->kind)
	{
		case GdiKind::Pen:
			old = d->pen;
			d->pen = obj;
			break;
		case GdiKind::Brush:
			old = d->brush;
			d->brush = obj;
			break;
		case GdiKind::Font:
			old = d->font;
			d->font = obj;
			break;
		case GdiKind::Bitmap:
		{
			old = d->bitmap;
			if (d->painter && d->ownsPainter)
			{
				d->painter->end();
				delete d->painter;
				d->painter = nullptr;
				d->ownsPainter = false;
			}
			d->bitmap = obj;
			if (!g->image && g->pixmap)
				g->image = new QImage(g->pixmap->toImage().convertToFormat(QImage::Format_ARGB32_Premultiplied));
			if (g->image)
			{
				d->painter = new QPainter(g->image);
				d->ownsPainter = true;
			}
			// the icon of the bitmap is made again from the image when it is asked
			if (g->icon && g->header.qicon == g->icon && g->kind == GdiKind::Bitmap)
			{
				delete g->icon;
				g->icon = nullptr;
				g->header.qicon = nullptr;
			}
			break;
		}
		case GdiKind::Region:
			if (g->region && d->painter)
				d->painter->setClipRegion(*g->region);
			return reinterpret_cast<HGDIOBJ>(1); // SIMPLEREGION
		default:
			break;
	}
	return old;
}

BOOL DeleteObject(HGDIOBJ obj)
{
	Gdi* g = gdi(obj);
	if (!g || g->stock)
		return g ? TRUE : FALSE;
	if (g->header.qicon && g->header.qicon != g->icon)
		delete static_cast<QIcon*>(g->header.qicon);
	delete g->icon;
	delete g->pixmap;
	delete g->image;
	delete g->cursor;
	delete g->region;
	g->header.magic = 0;
	delete g;
	return TRUE;
}

HGDIOBJ GetStockObject(int index)
{
	return stockObject(index);
}

int GetObjectW(HGDIOBJ obj, int size, LPVOID out)
{
	Gdi* g = gdi(obj);
	if (!g)
		return 0;
	switch (g->kind)
	{
		case GdiKind::Bitmap:
		case GdiKind::Icon:
		{
			if (!out || size < static_cast<int>(sizeof(BITMAP)))
				return sizeof(BITMAP);
			auto* bm = static_cast<BITMAP*>(out);
			*bm = BITMAP{};
			QSize s;
			if (g->image)
				s = g->image->size();
			else if (g->pixmap)
				s = g->pixmap->size();
			else if (g->icon)
				s = g->icon->availableSizes().value(0, QSize(32, 32));
			bm->bmWidth = s.width();
			bm->bmHeight = s.height();
			bm->bmWidthBytes = s.width() * 4;
			bm->bmPlanes = 1;
			bm->bmBitsPixel = 32;
			bm->bmBits = g->image ? g->image->bits() : nullptr;
			return sizeof(BITMAP);
		}
		case GdiKind::Font:
			if (!out || size < static_cast<int>(sizeof(LOGFONTW)))
				return sizeof(LOGFONTW);
			*static_cast<LOGFONTW*>(out) = g->logFont;
			return sizeof(LOGFONTW);
		case GdiKind::Brush:
			if (!out || size < static_cast<int>(sizeof(LOGBRUSH)))
				return sizeof(LOGBRUSH);
			*static_cast<LOGBRUSH*>(out) = LOGBRUSH{g->nullBrush ? static_cast<UINT>(BS_NULL) : static_cast<UINT>(BS_SOLID), colorRef(g->color), 0};
			return sizeof(LOGBRUSH);
		default:
			return 0;
	}
}

DWORD GetObjectType(HGDIOBJ obj)
{
	if (dcOf(obj))
		return OBJ_DC;
	Gdi* g = gdi(obj);
	if (!g)
		return 0;
	switch (g->kind)
	{
		case GdiKind::Pen: return OBJ_PEN;
		case GdiKind::Brush: return OBJ_BRUSH;
		case GdiKind::Font: return OBJ_FONT;
		case GdiKind::Bitmap: return OBJ_BITMAP;
		default: return 0;
	}
}

HGDIOBJ GetCurrentObject(HDC hdc, UINT type)
{
	Dc* d = dcOf(hdc);
	if (!d)
		return nullptr;
	switch (type)
	{
		case OBJ_PEN: return d->pen;
		case OBJ_BRUSH: return d->brush;
		case OBJ_FONT: return d->font ? d->font : stockObject(DEFAULT_GUI_FONT);
		case OBJ_BITMAP: return d->bitmap;
		default: return nullptr;
	}
}

HBRUSH CreateSolidBrush(COLORREF c)
{
	Gdi* g = newObject(GdiKind::Brush);
	g->color = color(c);
	return g;
}

HBRUSH CreateBrushIndirect(const LOGBRUSH* lb)
{
	if (!lb)
		return nullptr;
	Gdi* g = newObject(GdiKind::Brush);
	g->color = color(lb->lbColor);
	g->nullBrush = (lb->lbStyle == BS_NULL);
	return g;
}

HBRUSH CreatePatternBrush(HBITMAP hbm)
{
	Gdi* src = gdi(hbm);
	Gdi* g = newObject(GdiKind::Brush);
	if (src && src->image)
		g->image = new QImage(*src->image);
	return g;
}

HBRUSH CreateHatchBrush(int, COLORREF c)
{
	return CreateSolidBrush(c);
}

HPEN CreatePen(int style, int width, COLORREF c)
{
	Gdi* g = newObject(GdiKind::Pen);
	g->penStyle = style & 0x0F;
	g->width = width;
	g->color = color(c);
	return g;
}

HFONT CreateFontIndirectW(const LOGFONTW* lf)
{
	if (!lf)
		return nullptr;
	Gdi* g = newObject(GdiKind::Font);
	g->logFont = *lf;
	QFont f = defaultFont();
	const QString face = qs(lf->lfFaceName);
	static const QStringList uiFaces = {QStringLiteral("MS Shell Dlg"), QStringLiteral("MS Shell Dlg 2"), QStringLiteral("MS Sans Serif"),
		QStringLiteral("Segoe UI"), QStringLiteral("Tahoma"), QStringLiteral("Microsoft Sans Serif"), QStringLiteral("System")};
	static const QStringList fixedFaces = {QStringLiteral("Courier New"), QStringLiteral("Courier"), QStringLiteral("Consolas"),
		QStringLiteral("Lucida Console"), QStringLiteral("Fixedsys"), QStringLiteral("Terminal")};
	if (fixedFaces.contains(face, Qt::CaseInsensitive) || (!face.isEmpty() && !uiFaces.contains(face, Qt::CaseInsensitive)
		&& (lf->lfPitchAndFamily & 3) == FIXED_PITCH))
	{
		f = fixedFont();
		if (!fixedFaces.contains(face, Qt::CaseInsensitive))
			f.setFamily(face);
	}
	else if (!face.isEmpty() && !uiFaces.contains(face, Qt::CaseInsensitive))
	{
		f.setFamily(face);
	}
	if (lf->lfHeight < 0)
		f.setPixelSize(std::max(1, static_cast<int>(-lf->lfHeight)));
	else if (lf->lfHeight > 0)
		f.setPixelSize(std::max(1, static_cast<int>(lf->lfHeight * 4 / 5)));
	if (lf->lfWeight)
		f.setWeight(static_cast<QFont::Weight>(std::clamp(static_cast<int>(lf->lfWeight), 100, 900)));
	f.setItalic(lf->lfItalic);
	f.setUnderline(lf->lfUnderline);
	f.setStrikeOut(lf->lfStrikeOut);
	g->font = f;
	return g;
}

HFONT CreateFontW(int height, int width, int escapement, int orientation, int weight, DWORD italic, DWORD underline,
	DWORD strikeOut, DWORD charSet, DWORD outPrecision, DWORD clipPrecision, DWORD quality, DWORD pitchAndFamily, LPCWSTR face)
{
	LOGFONTW lf{};
	lf.lfHeight = height;
	lf.lfWidth = width;
	lf.lfEscapement = escapement;
	lf.lfOrientation = orientation;
	lf.lfWeight = weight;
	lf.lfItalic = static_cast<BYTE>(italic);
	lf.lfUnderline = static_cast<BYTE>(underline);
	lf.lfStrikeOut = static_cast<BYTE>(strikeOut);
	lf.lfCharSet = static_cast<BYTE>(charSet);
	lf.lfOutPrecision = static_cast<BYTE>(outPrecision);
	lf.lfClipPrecision = static_cast<BYTE>(clipPrecision);
	lf.lfQuality = static_cast<BYTE>(quality);
	lf.lfPitchAndFamily = static_cast<BYTE>(pitchAndFamily);
	if (face)
		wcsncpy(lf.lfFaceName, face, LF_FACESIZE - 1);
	return CreateFontIndirectW(&lf);
}

HRGN CreateRectRgn(int left, int top, int right, int bottom)
{
	Gdi* g = newObject(GdiKind::Region);
	g->region = new QRegion(left, top, right - left, bottom - top);
	return g;
}

HRGN CreateRectRgnIndirect(const RECT* rc)
{
	return rc ? CreateRectRgn(rc->left, rc->top, rc->right, rc->bottom) : nullptr;
}

HRGN CreateRoundRectRgn(int left, int top, int right, int bottom, int, int)
{
	return CreateRectRgn(left, top, right, bottom);
}

int SelectClipRgn(HDC hdc, HRGN rgn)
{
	QPainter* p = painterOf(dcOf(hdc));
	if (!p)
		return 0;
	Gdi* g = gdi(rgn);
	if (g && g->region)
		p->setClipRegion(*g->region);
	else
		p->setClipping(false);
	return 2;
}

int IntersectClipRect(HDC hdc, int left, int top, int right, int bottom)
{
	QPainter* p = painterOf(dcOf(hdc));
	if (!p)
		return 0;
	p->setClipRect(QRect(left, top, right - left, bottom - top), p->hasClipping() ? Qt::IntersectClip : Qt::ReplaceClip);
	return 2;
}

int ExcludeClipRect(HDC hdc, int left, int top, int right, int bottom)
{
	QPainter* p = painterOf(dcOf(hdc));
	if (!p)
		return 0;
	QRegion r = p->hasClipping() ? p->clipRegion() : QRegion(p->viewport());
	r -= QRegion(left, top, right - left, bottom - top);
	p->setClipRegion(r);
	return 2;
}

int GetClipBox(HDC hdc, LPRECT rc)
{
	QPainter* p = painterOf(dcOf(hdc));
	if (!p || !rc)
		return 0;
	const QRect r = p->hasClipping() ? p->clipBoundingRect().toRect() : p->viewport();
	*rc = RECT{r.left(), r.top(), r.left() + r.width(), r.top() + r.height()};
	return 2;
}

int SetWindowRgn(HWND hWnd, HRGN rgn, BOOL)
{
	QWidget* w = widgetOf(hWnd);
	Gdi* g = gdi(rgn);
	if (w && g && g->region)
		w->setMask(*g->region);
	else if (w)
		w->clearMask();
	return 1;
}

COLORREF SetTextColor(HDC hdc, COLORREF c)
{
	Dc* d = dcOf(hdc);
	if (!d)
		return CLR_INVALID;
	const COLORREF old = d->textColor;
	d->textColor = c;
	return old;
}

COLORREF GetTextColor(HDC hdc)
{
	Dc* d = dcOf(hdc);
	return d ? d->textColor : CLR_INVALID;
}

COLORREF SetBkColor(HDC hdc, COLORREF c)
{
	Dc* d = dcOf(hdc);
	if (!d)
		return CLR_INVALID;
	const COLORREF old = d->bkColor;
	d->bkColor = c;
	return old;
}

COLORREF GetBkColor(HDC hdc)
{
	Dc* d = dcOf(hdc);
	return d ? d->bkColor : CLR_INVALID;
}

int SetBkMode(HDC hdc, int mode)
{
	Dc* d = dcOf(hdc);
	if (!d)
		return 0;
	const int old = d->bkMode;
	d->bkMode = mode;
	return old;
}

int GetBkMode(HDC hdc)
{
	Dc* d = dcOf(hdc);
	return d ? d->bkMode : 0;
}

COLORREF SetDCBrushColor(HDC hdc, COLORREF c)
{
	Dc* d = dcOf(hdc);
	if (!d)
		return CLR_INVALID;
	const COLORREF old = d->dcBrush;
	d->dcBrush = c;
	return old;
}

COLORREF SetDCPenColor(HDC hdc, COLORREF c)
{
	Dc* d = dcOf(hdc);
	if (!d)
		return CLR_INVALID;
	const COLORREF old = d->dcPen;
	d->dcPen = c;
	return old;
}

UINT SetTextAlign(HDC hdc, UINT align)
{
	Dc* d = dcOf(hdc);
	if (!d)
		return 0;
	const UINT old = d->align;
	d->align = align;
	return old;
}

int SetMapMode(HDC, int)
{
	return 1; // MM_TEXT
}

int SaveDC(HDC hdc)
{
	Dc* d = dcOf(hdc);
	if (!d)
		return 0;
	d->saved.push_back(DcState{d->pen, d->brush, d->font, d->bitmap, d->textColor, d->bkColor, d->bkMode, d->align});
	if (QPainter* p = painterOf(d))
		p->save();
	return static_cast<int>(d->saved.size());
}

BOOL RestoreDC(HDC hdc, int saved)
{
	Dc* d = dcOf(hdc);
	if (!d || d->saved.empty())
		return FALSE;
	const size_t target = saved < 0 ? d->saved.size() + static_cast<size_t>(saved) : static_cast<size_t>(saved - 1);
	while (d->saved.size() > target)
	{
		const DcState s = d->saved.back();
		d->saved.pop_back();
		d->pen = s.pen;
		d->brush = s.brush;
		d->font = s.font;
		d->textColor = s.text;
		d->bkColor = s.bk;
		d->bkMode = s.bkMode;
		d->align = s.align;
		if (QPainter* p = painterOf(d))
			p->restore();
	}
	return TRUE;
}

int FillRect(HDC hdc, const RECT* rc, HBRUSH brush)
{
	Dc* d = dcOf(hdc);
	QPainter* p = painterOf(d);
	if (!p || !rc)
		return 0;
	p->fillRect(qrect(rc), brushOf(brush, d));
	return 1;
}

int FrameRect(HDC hdc, const RECT* rc, HBRUSH brush)
{
	Dc* d = dcOf(hdc);
	QPainter* p = painterOf(d);
	if (!p || !rc)
		return 0;
	const QRect r = qrect(rc);
	const QBrush b = brushOf(brush, d);
	p->fillRect(QRect(r.left(), r.top(), r.width(), 1), b);
	p->fillRect(QRect(r.left(), r.bottom(), r.width(), 1), b);
	p->fillRect(QRect(r.left(), r.top(), 1, r.height()), b);
	p->fillRect(QRect(r.right(), r.top(), 1, r.height()), b);
	return 1;
}

BOOL InvertRect(HDC hdc, const RECT* rc)
{
	QPainter* p = painterOf(dcOf(hdc));
	if (!p || !rc)
		return FALSE;
	p->save();
	p->setCompositionMode(QPainter::RasterOp_SourceXorDestination);
	p->fillRect(qrect(rc), Qt::white);
	p->restore();
	return TRUE;
}

namespace
{
	template <typename F>
	BOOL shape(HDC hdc, F draw)
	{
		Dc* d = dcOf(hdc);
		QPainter* p = painterOf(d);
		if (!p)
			return FALSE;
		p->save();
		p->setPen(dcPenOf(d));
		p->setBrush(brushOf(d->brush, d));
		draw(p);
		p->restore();
		return TRUE;
	}
}

BOOL Rectangle(HDC hdc, int left, int top, int right, int bottom)
{
	return shape(hdc, [&](QPainter* p) { p->drawRect(QRect(left, top, right - left - 1, bottom - top - 1)); });
}

BOOL RoundRect(HDC hdc, int left, int top, int right, int bottom, int w, int h)
{
	return shape(hdc, [&](QPainter* p) { p->drawRoundedRect(QRect(left, top, right - left - 1, bottom - top - 1), w / 2.0, h / 2.0); });
}

BOOL Ellipse(HDC hdc, int left, int top, int right, int bottom)
{
	return shape(hdc, [&](QPainter* p) { p->drawEllipse(QRect(left, top, right - left - 1, bottom - top - 1)); });
}

BOOL Polygon(HDC hdc, const POINT* pts, int count)
{
	return shape(hdc, [&](QPainter* p)
	{
		QPolygon poly;
		for (int i = 0; i < count; ++i)
			poly << QPoint(pts[i].x, pts[i].y);
		p->drawPolygon(poly);
	});
}

BOOL Polyline(HDC hdc, const POINT* pts, int count)
{
	return shape(hdc, [&](QPainter* p)
	{
		QPolygon poly;
		for (int i = 0; i < count; ++i)
			poly << QPoint(pts[i].x, pts[i].y);
		p->drawPolyline(poly);
	});
}

BOOL MoveToEx(HDC hdc, int x, int y, LPPOINT old)
{
	Dc* d = dcOf(hdc);
	if (!d)
		return FALSE;
	if (old)
		*old = POINT{d->pos.x(), d->pos.y()};
	d->pos = QPoint(x, y);
	return TRUE;
}

BOOL LineTo(HDC hdc, int x, int y)
{
	Dc* d = dcOf(hdc);
	QPainter* p = painterOf(d);
	if (!p)
		return FALSE;
	p->save();
	p->setPen(dcPenOf(d));
	// GDI does not paint the last point
	const QPoint from = d->pos;
	const QPoint to(x, y);
	QLineF line(from, to);
	if (line.length() > 0)
		line.setLength(line.length() - 1);
	p->drawLine(line);
	p->restore();
	d->pos = to;
	return TRUE;
}

COLORREF SetPixel(HDC hdc, int x, int y, COLORREF c)
{
	QPainter* p = painterOf(dcOf(hdc));
	if (!p)
		return CLR_INVALID;
	p->save();
	p->setPen(color(c));
	p->drawPoint(x, y);
	p->restore();
	return c;
}

COLORREF GetPixel(HDC hdc, int x, int y)
{
	Dc* d = dcOf(hdc);
	Gdi* g = d ? gdi(d->bitmap) : nullptr;
	if (!g || !g->image || !g->image->rect().contains(x, y))
		return CLR_INVALID;
	return colorRef(QColor(g->image->pixel(x, y)));
}

BOOL DrawFocusRect(HDC hdc, const RECT* rc)
{
	QPainter* p = painterOf(dcOf(hdc));
	if (!p || !rc)
		return FALSE;
	p->save();
	QPen pen(QApplication::palette().color(QPalette::Text));
	pen.setStyle(Qt::DotLine);
	p->setPen(pen);
	p->setBrush(Qt::NoBrush);
	p->drawRect(qrect(rc).adjusted(0, 0, -1, -1));
	p->restore();
	return TRUE;
}

BOOL DrawEdge(HDC hdc, LPRECT rc, UINT edge, UINT flags)
{
	QPainter* p = painterOf(dcOf(hdc));
	if (!p || !rc)
		return FALSE;
	const QRect r = qrect(rc).adjusted(0, 0, -1, -1);
	const QPalette pal = QApplication::palette();
	const bool sunken = edge & (BDR_SUNKENOUTER | BDR_SUNKENINNER);
	const QColor tl = sunken ? pal.color(QPalette::Mid) : pal.color(QPalette::Light);
	const QColor br = sunken ? pal.color(QPalette::Light) : pal.color(QPalette::Mid);
	p->save();
	if (flags & BF_TOP)
	{
		p->setPen(tl);
		p->drawLine(r.topLeft(), r.topRight());
	}
	if (flags & BF_LEFT)
	{
		p->setPen(tl);
		p->drawLine(r.topLeft(), r.bottomLeft());
	}
	if (flags & BF_BOTTOM)
	{
		p->setPen(br);
		p->drawLine(r.bottomLeft(), r.bottomRight());
	}
	if (flags & BF_RIGHT)
	{
		p->setPen(br);
		p->drawLine(r.topRight(), r.bottomRight());
	}
	p->restore();
	return TRUE;
}

BOOL DrawFrameControl(HDC hdc, LPRECT rc, UINT type, UINT state)
{
	QPainter* p = painterOf(dcOf(hdc));
	if (!p || !rc)
		return FALSE;
	const QRect r = qrect(rc);
	p->save();
	p->setPen(QApplication::palette().color(QPalette::Mid));
	p->setBrush((state & DFCS_PUSHED) ? QApplication::palette().mid() : QApplication::palette().button());
	p->drawRect(r.adjusted(0, 0, -1, -1));
	if (type == DFC_BUTTON && (state & DFCS_CHECKED))
	{
		p->setPen(QPen(QApplication::palette().color(QPalette::Text), 2));
		p->drawLine(r.left() + 3, r.center().y(), r.center().x(), r.bottom() - 3);
		p->drawLine(r.center().x(), r.bottom() - 3, r.right() - 3, r.top() + 3);
	}
	p->restore();
	return TRUE;
}

BOOL DrawIcon(HDC hdc, int x, int y, HICON icon)
{
	return DrawIconEx(hdc, x, y, icon, 32, 32, 0, nullptr, DI_NORMAL);
}

BOOL DrawIconEx(HDC hdc, int x, int y, HICON icon, int cx, int cy, UINT, HBRUSH, UINT)
{
	QPainter* p = painterOf(dcOf(hdc));
	if (!p || !icon)
		return FALSE;
	const QIcon q = iconOf(icon);
	const QSize s(cx ? cx : 32, cy ? cy : 32);
	q.paint(p, QRect(QPoint(x, y), s));
	return TRUE;
}

BOOL GradientFill(HDC, void*, ULONG, void*, ULONG, ULONG)
{
	return FALSE;
}

namespace
{
	QString prefixText(const QString& t, UINT format)
	{
		if (format & DT_NOPREFIX)
			return t;
		QString out;
		for (int i = 0; i < t.size(); ++i)
		{
			if (t[i] == QLatin1Char('&'))
			{
				if (i + 1 < t.size() && t[i + 1] == QLatin1Char('&'))
				{
					out += QLatin1Char('&');
					++i;
				}
				continue;
			}
			out += t[i];
		}
		return out;
	}

	int drawText(Dc* d, const QString& text, LPRECT rc, UINT format)
	{
		QPainter* p = painterOf(d);
		if (!p || !rc)
			return 0;
		applyFont(p, d);
		const QFontMetrics fm(p->font());
		QString t = prefixText(text, format);
		if (format & DT_EXPANDTABS)
			t.replace(QLatin1Char('\t'), QStringLiteral("        "));
		QRect r = qrect(rc);
		int flags = 0;
		if (format & DT_CENTER)
			flags |= Qt::AlignHCenter;
		else if (format & DT_RIGHT)
			flags |= Qt::AlignRight;
		else
			flags |= Qt::AlignLeft;
		if (format & DT_SINGLELINE)
		{
			flags |= Qt::TextSingleLine;
			if (format & DT_VCENTER)
				flags |= Qt::AlignVCenter;
			else if (format & DT_BOTTOM)
				flags |= Qt::AlignBottom;
			else
				flags |= Qt::AlignTop;
		}
		else
		{
			flags |= Qt::AlignTop;
			if (format & DT_WORDBREAK)
				flags |= Qt::TextWordWrap;
		}
		if (format & DT_CALCRECT)
		{
			const QRect bound = fm.boundingRect(QRect(r.left(), r.top(), (format & DT_WORDBREAK) && r.width() > 0 ? r.width() : 100000, 100000), flags, t);
			rc->right = rc->left + bound.width();
			rc->bottom = rc->top + bound.height();
			return bound.height();
		}
		if (format & (DT_END_ELLIPSIS | DT_PATH_ELLIPSIS | DT_WORD_ELLIPSIS))
			t = fm.elidedText(t, (format & DT_PATH_ELLIPSIS) ? Qt::ElideMiddle : Qt::ElideRight, r.width());
		p->save();
		if (!(format & DT_NOCLIP))
			p->setClipRect(r, p->hasClipping() ? Qt::IntersectClip : Qt::ReplaceClip);
		QRect bound;
		if (d->bkMode == OPAQUE)
		{
			bound = fm.boundingRect(r, flags, t);
			p->fillRect(bound, color(d->bkColor));
		}
		p->setPen(color(d->textColor));
		p->drawText(r, flags, t, &bound);
		p->restore();
		return bound.height();
	}
}

int DrawTextW(HDC hdc, LPCWSTR text, int len, LPRECT rc, UINT format)
{
	return drawText(dcOf(hdc), textOf(text, len), rc, format);
}

int DrawTextA(HDC hdc, LPCSTR text, int len, LPRECT rc, UINT format)
{
	return drawText(dcOf(hdc), qsA(text, len), rc, format);
}

int DrawTextExW(HDC hdc, LPWSTR text, int len, LPRECT rc, UINT format, void*)
{
	return drawText(dcOf(hdc), textOf(text, len), rc, format);
}

namespace
{
	BOOL textOut(Dc* d, int x, int y, const QString& t, const RECT* opaque)
	{
		QPainter* p = painterOf(d);
		if (!p)
			return FALSE;
		applyFont(p, d);
		const QFontMetrics fm(p->font());
		if (opaque)
			p->fillRect(qrect(opaque), color(d->bkColor));
		int w = fm.horizontalAdvance(t);
		int left = x;
		if ((d->align & 6) == 6) // TA_CENTER
			left -= w / 2;
		else if (d->align & 2) // TA_RIGHT
			left -= w;
		const int top = (d->align & 8) ? y - fm.height() : ((d->align & 24) == 24 ? y - fm.ascent() : y); // TA_BOTTOM / TA_BASELINE
		if (d->bkMode == OPAQUE && !opaque)
			p->fillRect(QRect(left, top, w, fm.height()), color(d->bkColor));
		p->setPen(color(d->textColor));
		p->drawText(QPoint(left, top + fm.ascent()), t);
		return TRUE;
	}
}

BOOL TextOutW(HDC hdc, int x, int y, LPCWSTR text, int len)
{
	return textOut(dcOf(hdc), x, y, textOf(text, len), nullptr);
}

BOOL TextOutA(HDC hdc, int x, int y, LPCSTR text, int len)
{
	return textOut(dcOf(hdc), x, y, qsA(text, len), nullptr);
}

BOOL ExtTextOutW(HDC hdc, int x, int y, UINT options, const RECT* rc, LPCWSTR text, UINT len, const int*)
{
	Dc* d = dcOf(hdc);
	QPainter* p = painterOf(d);
	if (!p)
		return FALSE;
	if (rc && (options & ETO_CLIPPED))
	{
		p->save();
		p->setClipRect(qrect(rc), p->hasClipping() ? Qt::IntersectClip : Qt::ReplaceClip);
	}
	const BOOL r = textOut(d, x, y, text ? textOf(text, static_cast<int>(len)) : QString(), (options & ETO_OPAQUE) ? rc : nullptr);
	if (rc && (options & ETO_CLIPPED))
		p->restore();
	return r;
}

BOOL GetTextExtentPoint32W(HDC hdc, LPCWSTR text, int len, LPSIZE size)
{
	if (!size)
		return FALSE;
	Dc* d = dcOf(hdc);
	const QFontMetrics fm(dcFont(d));
	const QString t = textOf(text, len);
	size->cx = fm.horizontalAdvance(t);
	size->cy = fm.height();
	return TRUE;
}

BOOL GetTextExtentPoint32A(HDC hdc, LPCSTR text, int len, LPSIZE size)
{
	const std::wstring t = ws(qsA(text, len));
	return GetTextExtentPoint32W(hdc, t.c_str(), static_cast<int>(t.size()), size);
}

BOOL GetTextMetricsW(HDC hdc, LPTEXTMETRICW tm)
{
	if (!tm)
		return FALSE;
	const QFont f = dcFont(dcOf(hdc));
	const QFontMetrics fm(f);
	*tm = TEXTMETRICW{};
	tm->tmHeight = fm.height();
	tm->tmAscent = fm.ascent();
	tm->tmDescent = fm.descent();
	tm->tmInternalLeading = std::max(0, fm.height() - fm.ascent() - fm.descent());
	tm->tmExternalLeading = fm.leading();
	tm->tmAveCharWidth = fm.averageCharWidth();
	tm->tmMaxCharWidth = fm.maxWidth();
	tm->tmWeight = f.bold() ? FW_BOLD : FW_NORMAL;
	tm->tmItalic = f.italic();
	tm->tmUnderlined = f.underline();
	tm->tmFirstChar = 32;
	tm->tmLastChar = 0xFFFD;
	tm->tmDefaultChar = L'?';
	tm->tmBreakChar = L' ';
	tm->tmPitchAndFamily = f.fixedPitch() ? 0 : 1; // TMPF_FIXED_PITCH means variable
	tm->tmDigitizedAspectX = 96;
	tm->tmDigitizedAspectY = 96;
	return TRUE;
}

int GetTextFaceW(HDC hdc, int max, LPWSTR face)
{
	return copyOut(dcFont(dcOf(hdc)).family(), face, max);
}

int GetDeviceCaps(HDC, int index)
{
	QScreen* s = QGuiApplication::primaryScreen();
	switch (index)
	{
		case LOGPIXELSX: return s ? static_cast<int>(s->logicalDotsPerInchX()) : 96;
		case LOGPIXELSY: return s ? static_cast<int>(s->logicalDotsPerInchY()) : 96;
		case HORZRES: return s ? s->geometry().width() : 1920;
		case VERTRES: return s ? s->geometry().height() : 1080;
		case BITSPIXEL: return 32;
		case PLANES: return 1;
		default: return 0;
	}
}

namespace
{
	QImage* imageOfDC(Dc* d)
	{
		Gdi* g = d ? gdi(d->bitmap) : nullptr;
		return g ? g->image : nullptr;
	}
}

BOOL BitBlt(HDC dst, int x, int y, int cx, int cy, HDC src, int sx, int sy, DWORD rop)
{
	return StretchBlt(dst, x, y, cx, cy, src, sx, sy, cx, cy, rop);
}

BOOL StretchBlt(HDC dst, int x, int y, int cx, int cy, HDC src, int sx, int sy, int scx, int scy, DWORD rop)
{
	Dc* d = dcOf(dst);
	QPainter* p = painterOf(d);
	if (!p)
		return FALSE;
	if (rop == BLACKNESS || rop == WHITENESS)
	{
		p->fillRect(QRect(x, y, cx, cy), rop == BLACKNESS ? Qt::black : Qt::white);
		return TRUE;
	}
	QImage* img = imageOfDC(dcOf(src));
	if (!img)
		return FALSE;
	p->save();
	if (rop == SRCAND)
		p->setCompositionMode(QPainter::RasterOp_SourceAndDestination);
	else if (rop == SRCPAINT)
		p->setCompositionMode(QPainter::RasterOp_SourceOrDestination);
	else if (rop == SRCINVERT)
		p->setCompositionMode(QPainter::RasterOp_SourceXorDestination);
	else
		p->setCompositionMode(QPainter::CompositionMode_Source);
	p->drawImage(QRect(x, y, cx, cy), *img, QRect(sx, sy, scx, scy));
	p->restore();
	return TRUE;
}

BOOL PatBlt(HDC hdc, int x, int y, int cx, int cy, DWORD rop)
{
	Dc* d = dcOf(hdc);
	QPainter* p = painterOf(d);
	if (!p)
		return FALSE;
	if (rop == BLACKNESS)
		p->fillRect(QRect(x, y, cx, cy), Qt::black);
	else if (rop == WHITENESS)
		p->fillRect(QRect(x, y, cx, cy), Qt::white);
	else
		p->fillRect(QRect(x, y, cx, cy), brushOf(d->brush, d));
	return TRUE;
}

BOOL TransparentBlt(HDC dst, int x, int y, int cx, int cy, HDC src, int sx, int sy, int scx, int scy, UINT transparent)
{
	Dc* d = dcOf(dst);
	QPainter* p = painterOf(d);
	QImage* img = imageOfDC(dcOf(src));
	if (!p || !img)
		return FALSE;
	QImage part = img->copy(sx, sy, scx, scy).convertToFormat(QImage::Format_ARGB32);
	const QRgb key = colorRef(QColor(0, 0, 0)) == transparent ? 0 : qRgb(GetRValue(transparent), GetGValue(transparent), GetBValue(transparent));
	for (int yy = 0; yy < part.height(); ++yy)
	{
		auto* line = reinterpret_cast<QRgb*>(part.scanLine(yy));
		for (int xx = 0; xx < part.width(); ++xx)
		{
			if ((line[xx] & 0xFFFFFF) == (key & 0xFFFFFF))
				line[xx] = 0;
		}
	}
	p->drawImage(QRect(x, y, cx, cy), part);
	return TRUE;
}

BOOL AlphaBlend(HDC dst, int x, int y, int cx, int cy, HDC src, int sx, int sy, int scx, int scy, DWORD blend)
{
	Dc* d = dcOf(dst);
	QPainter* p = painterOf(d);
	QImage* img = imageOfDC(dcOf(src));
	if (!p || !img)
		return FALSE;
	p->save();
	p->setOpacity(((blend >> 16) & 0xFF) / 255.0);
	p->drawImage(QRect(x, y, cx, cy), *img, QRect(sx, sy, scx, scy));
	p->restore();
	return TRUE;
}

int SetStretchBltMode(HDC, int)
{
	return 1;
}

BOOL GdiFlush()
{
	return TRUE;
}

// ---- system colours ----------------------------------------------------------------------------
DWORD GetSysColor(int index)
{
	return colorRef(sysColor(index));
}

HBRUSH GetSysColorBrush(int index)
{
	static std::map<int, Gdi*> brushes;
	Gdi*& g = brushes[index];
	if (!g)
	{
		g = new Gdi;
		g->kind = GdiKind::Brush;
		g->stock = true;
	}
	g->color = sysColor(index);
	return g;
}

// ---- images and resources --------------------------------------------------------------------
HANDLE LoadImageW(HINSTANCE, LPCWSTR name, UINT type, int cx, int cy, UINT flags)
{
	if (!name)
		return nullptr;
	QByteArray data;
	bool standard = false;
	int standardId = 0;
	if (flags & LR_LOADFROMFILE)
	{
		QFile f(nativePath(name));
		if (!f.open(QIODevice::ReadOnly))
			return nullptr;
		data = f.readAll();
	}
	else
	{
		const NppRcData* res = findImage(name, static_cast<int>(type));
		if (res)
			data = QByteArray::fromRawData(reinterpret_cast<const char*>(res->data), static_cast<int>(res->size));
		else if (isIntResource(name) && intResource(name) >= 32512)
		{
			standard = true;
			standardId = intResource(name);
		}
		else
			return nullptr;
	}
	switch (type)
	{
		case IMAGE_ICON:
		{
			Gdi* g = newObject(GdiKind::Icon);
			g->icon = new QIcon(standard ? standardIcon(standardId) : iconFromData(data));
			if (g->icon->isNull())
			{
				DeleteObject(g);
				return nullptr;
			}
			if (cx > 0 && cy > 0)
			{
				// the size asked for: an icon of that size only (some plugins draw it as is)
				const QPixmap px = g->icon->pixmap(QSize(cx, cy));
				QIcon sized;
				sized.addPixmap(px.size() == QSize(cx, cy) ? px : px.scaled(cx, cy, Qt::KeepAspectRatio, Qt::SmoothTransformation));
				*g->icon = sized;
			}
			g->header.qicon = g->icon;
			return g;
		}
		case IMAGE_CURSOR:
		{
			Gdi* g = newObject(GdiKind::Cursor);
			if (standard)
				g->cursor = new QCursor(cursorShape(standardId));
			else
			{
				QImage img = QImage::fromData(data);
				g->cursor = new QCursor(img.isNull() ? QCursor(Qt::ArrowCursor) : QCursor(QPixmap::fromImage(img)));
			}
			return g;
		}
		default:
		{
			if (standard)
				return nullptr;
			QImage img = QImage::fromData(data);
			if (img.isNull())
				return nullptr;
			if (flags & (LR_LOADTRANSPARENT | LR_LOADMAP3DCOLORS))
				img = transparentBackground(img, flags & LR_LOADMAP3DCOLORS);
			if (cx > 0 && cy > 0 && img.size() != QSize(cx, cy))
				img = img.scaled(cx, cy, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
			Gdi* g = newObject(GdiKind::Bitmap);
			g->image = new QImage(img.convertToFormat(QImage::Format_ARGB32_Premultiplied));
			ensureIconHeader(g);
			return g;
		}
	}
}

HANDLE LoadImageA(HINSTANCE hInst, LPCSTR name, UINT type, int cx, int cy, UINT flags)
{
	if (isIntResource(name))
		return LoadImageW(hInst, reinterpret_cast<LPCWSTR>(name), type, cx, cy, flags);
	const std::wstring n = ws(qsA(name));
	return LoadImageW(hInst, n.c_str(), type, cx, cy, flags);
}

HICON LoadIconW(HINSTANCE hInst, LPCWSTR name)
{
	return LoadImageW(hInst, name, IMAGE_ICON, 0, 0, LR_SHARED);
}

HBITMAP LoadBitmapW(HINSTANCE hInst, LPCWSTR name)
{
	return LoadImageW(hInst, name, IMAGE_BITMAP, 0, 0, 0);
}

HCURSOR LoadCursorW(HINSTANCE hInst, LPCWSTR name)
{
	if (!hInst && isIntResource(name))
	{
		static std::map<int, Gdi*> cursors;
		Gdi*& g = cursors[intResource(name)];
		if (!g)
		{
			g = newObject(GdiKind::Cursor);
			g->cursor = new QCursor(cursorShape(intResource(name)));
			g->stock = true;
		}
		return g;
	}
	HANDLE h = LoadImageW(hInst, name, IMAGE_CURSOR, 0, 0, 0);
	return h ? h : LoadCursorW(nullptr, IDC_ARROW);
}

BOOL DestroyIcon(HICON icon)
{
	return DeleteObject(icon);
}

HICON CopyIcon(HICON icon)
{
	return iconHandle(iconOf(icon));
}

HANDLE CopyImage(HANDLE h, UINT type, int cx, int cy, UINT)
{
	Gdi* g = gdi(h);
	if (!g)
		return nullptr;
	if (type == IMAGE_ICON || g->kind == GdiKind::Icon)
	{
		QIcon icon = iconOf(h);
		if (cx > 0 && cy > 0)
		{
			QIcon sized;
			sized.addPixmap(icon.pixmap(QSize(cx, cy)));
			icon = sized;
		}
		return iconHandle(icon);
	}
	QPixmap px = pixmapOf(h);
	if (cx > 0 && cy > 0)
		px = px.scaled(cx, cy, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
	return bitmapHandle(px);
}

HICON CreateIconIndirect(PICONINFO info)
{
	if (!info)
		return nullptr;
	const QPixmap px = pixmapOf(info->hbmColor);
	return px.isNull() ? nullptr : iconHandle(QIcon(px));
}

BOOL GetIconInfo(HICON icon, PICONINFO info)
{
	if (!info || !icon)
		return FALSE;
	*info = ICONINFO{};
	info->fIcon = TRUE;
	info->hbmColor = bitmapHandle(pixmapOf(icon));
	info->hbmMask = nullptr;
	return TRUE;
}

HRESULT LoadIconMetric(HINSTANCE hInst, PCWSTR name, int metric, HICON* icon)
{
	if (!icon)
		return E_POINTER;
	const int s = metric ? 32 : 16;
	*icon = LoadImageW(hInst, name, IMAGE_ICON, s, s, 0);
	return *icon ? S_OK : E_FAIL;
}

HRESULT LoadIconWithScaleDown(HINSTANCE hInst, PCWSTR name, int cx, int cy, HICON* icon)
{
	if (!icon)
		return E_POINTER;
	*icon = LoadImageW(hInst, name, IMAGE_ICON, cx, cy, 0);
	return *icon ? S_OK : E_FAIL;
}
