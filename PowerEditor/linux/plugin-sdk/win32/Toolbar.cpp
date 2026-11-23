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

// Notepad++ Linux plugin SDK, npp_win32 library: toolbars (ToolbarWindow32) on a QToolBar and
// rebars (ReBarWindow32) as a row of their band windows.

#include <QAction>
#include <QEvent>
#include <QHBoxLayout>
#include <QHelpEvent>
#include <QIcon>
#include <QMenu>
#include <QPointer>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QToolTip>

#include "W32Internal.h"

using namespace w32;

namespace
{
	constexpr LRESULT kNotHandled = -0x7FFFFFFF;

	struct ToolbarButton
	{
		TBBUTTON b{};
		QPointer<QAction> action;
	};

	struct ToolbarData
	{
		std::vector<ToolbarButton> buttons;
		HIMAGELIST images = nullptr;
		HIMAGELIST hotImages = nullptr;
		HIMAGELIST disabledImages = nullptr;
		std::vector<QIcon> bitmapIcons; // TB_ADDBITMAP
		std::vector<QString> strings;   // TB_ADDSTRING
		QSize buttonSize{24, 22};
		DWORD exStyle = 0;
		DWORD padding = MAKELONG(7, 6);
	};

	struct RebarBand
	{
		REBARBANDINFOW info{};
		QPointer<QWidget> child;
	};

	struct RebarData
	{
		std::vector<RebarBand> bands;
	};

	ToolbarData* toolbarData(Window* w) { return static_cast<ToolbarData*>(w->ctl); }
	RebarData* rebarData(Window* w) { return static_cast<RebarData*>(w->ctl); }

	// the standard bitmaps of the common controls (HINST_COMMCTRL): icons of the theme
	QIcon standardIcon(UINT_PTR bitmapId, int index)
	{
		static const char* std[] = {"edit-cut", "edit-copy", "edit-paste", "edit-undo", "edit-redo", "edit-delete", "document-new",
			"document-open", "document-save", "document-print-preview", "document-properties", "help-contents", "edit-find",
			"edit-find-replace", "document-print"};
		static const char* view[] = {"view-list-icons", "view-list-icons", "view-list-text", "view-list-details", "view-sort-ascending",
			"view-sort-ascending", "view-sort-ascending", "view-sort-ascending", "go-up", "network-connect", "network-disconnect",
			"folder-new", "view-more"};
		const bool isView = bitmapId == IDB_VIEW_SMALL_COLOR || bitmapId == IDB_VIEW_LARGE_COLOR;
		const int count = isView ? static_cast<int>(std::size(view)) : static_cast<int>(std::size(std));
		if (index < 0 || index >= count)
			return QIcon();
		return QIcon::fromTheme(QString::fromLatin1(isView ? view[index] : std[index]));
	}

	QIcon buttonIcon(const ToolbarData* d, int image)
	{
		if (image < 0)
			return QIcon();
		if (d->images)
		{
			if (HICON h = ImageList_GetIcon(d->images, image, 0))
				return iconOf(h);
			return QIcon();
		}
		if (image < static_cast<int>(d->bitmapIcons.size()))
			return d->bitmapIcons[static_cast<size_t>(image)];
		return QIcon();
	}

	QString buttonText(const ToolbarData* d, INT_PTR iString)
	{
		if (iString >= 0 && iString < static_cast<INT_PTR>(d->strings.size()))
			return d->strings[static_cast<size_t>(iString)];
		if (iString > 0xFFFF)
			return qs(reinterpret_cast<LPCWSTR>(iString));
		return QString();
	}

	void applyState(Window* w, ToolbarButton& tb)
	{
		QAction* a = tb.action;
		if (!a)
			return;
		const BYTE st = tb.b.fsState;
		a->setEnabled((st & TBSTATE_ENABLED) != 0);
		a->setVisible(!(st & TBSTATE_HIDDEN));
		if (a->isCheckable())
			a->setChecked((st & TBSTATE_CHECKED) != 0);
		if (auto* bar = qobject_cast<QToolBar*>(w->widget))
		{
			if (auto* button = qobject_cast<QToolButton*>(bar->widgetForAction(a)))
				button->setDown((st & TBSTATE_PRESSED) != 0);
		}
	}

	void refreshIcons(Window* w)
	{
		ToolbarData* d = toolbarData(w);
		for (ToolbarButton& tb : d->buttons)
		{
			if (tb.action && !(tb.b.fsStyle & BTNS_SEP))
				tb.action->setIcon(buttonIcon(d, tb.b.iBitmap));
		}
	}

	class ToolTipFilter : public QObject
	{
	public:
		ToolTipFilter(Window* w, int id, QObject* parent) : QObject(parent), _w(w), _id(id) {}
		bool eventFilter(QObject* obj, QEvent* e) override
		{
			if (e->type() != QEvent::ToolTip || !(_w->style & TBSTYLE_TOOLTIPS))
				return false;
			// TTN_GETDISPINFO to the parent, idFrom the command of the button
			NMTTDISPINFOW di{};
			di.hdr.hwndFrom = hwnd(_w);
			di.hdr.idFrom = static_cast<UINT_PTR>(_id);
			di.hdr.code = TTN_GETDISPINFOW;
			SendMessageW(_w->parent, WM_NOTIFY, static_cast<WPARAM>(_id), reinterpret_cast<LPARAM>(&di));
			QString text;
			if (di.lpszText && isIntResource(di.lpszText))
			{
				wchar_t buf[512] = {};
				LoadStringW(di.hinst ? di.hinst : moduleHandle(), static_cast<UINT>(intResource(di.lpszText)), buf, 512);
				text = qs(buf);
			}
			else if (di.lpszText)
				text = qs(di.lpszText);
			else
				text = qs(di.szText);
			if (!text.isEmpty())
				QToolTip::showText(static_cast<QHelpEvent*>(e)->globalPos(), text, qobject_cast<QWidget*>(obj));
			return true;
		}

	private:
		Window* _w;
		int _id;
	};

	void insertButton(Window* w, int index, const TBBUTTON& b)
	{
		ToolbarData* d = toolbarData(w);
		auto* bar = qobject_cast<QToolBar*>(w->widget);
		if (!bar)
			return;
		if (index < 0 || index > static_cast<int>(d->buttons.size()))
			index = static_cast<int>(d->buttons.size());
		ToolbarButton tb;
		tb.b = b;
		QAction* before = index < static_cast<int>(d->buttons.size()) ? d->buttons[static_cast<size_t>(index)].action.data() : nullptr;
		if (b.fsStyle & BTNS_SEP)
		{
			tb.action = bar->insertSeparator(before);
		}
		else
		{
			auto* a = new QAction(buttonIcon(d, b.iBitmap), buttonText(d, b.iString), bar);
			a->setCheckable((b.fsStyle & BTNS_CHECK) != 0);
			const int id = b.idCommand;
			QObject::connect(a, &QAction::triggered, bar, [w, id]
			{
				// the state of a check button, then WM_COMMAND to the parent like a click of Windows
				ToolbarData* data = toolbarData(w);
				for (ToolbarButton& t : data->buttons)
				{
					if (t.b.idCommand == id && t.action && t.action->isCheckable())
						t.b.fsState = static_cast<BYTE>(t.action->isChecked() ? (t.b.fsState | TBSTATE_CHECKED) : (t.b.fsState & ~TBSTATE_CHECKED));
				}
				SendMessageW(w->parent, WM_COMMAND, MAKEWPARAM(id, BN_CLICKED), reinterpret_cast<LPARAM>(hwnd(w)));
			});
			bar->insertAction(before, a);
			tb.action = a;
			if (auto* button = qobject_cast<QToolButton*>(bar->widgetForAction(a)))
			{
				button->installEventFilter(new ToolTipFilter(w, id, button));
				if (b.fsStyle & (BTNS_DROPDOWN | BTNS_WHOLEDROPDOWN))
				{
					// the arrow: TBN_DROPDOWN, the parent shows its own menu (TrackPopupMenu)
					auto* menu = new QMenu(button);
					button->setMenu(menu);
					button->setPopupMode((b.fsStyle & BTNS_WHOLEDROPDOWN) ? QToolButton::InstantPopup : QToolButton::MenuButtonPopup);
					QObject::connect(menu, &QMenu::aboutToShow, menu, [w, id, menu, button]
					{
						QTimer::singleShot(0, menu, [menu] { menu->close(); });
						QTimer::singleShot(0, button, [w, id, button]
						{
							NMTOOLBARW nm{};
							nm.iItem = id;
							const QRect r = button->geometry();
							nm.rcButton = RECT{r.left(), r.top(), r.right() + 1, r.bottom() + 1};
							notify(w, &nm.hdr, TBN_DROPDOWN);
						});
					});
				}
			}
		}
		d->buttons.insert(d->buttons.begin() + index, tb);
		applyState(w, d->buttons[static_cast<size_t>(index)]);
	}

	int indexOfCommand(const ToolbarData* d, int id)
	{
		for (size_t i = 0; i < d->buttons.size(); ++i)
		{
			if (d->buttons[i].b.idCommand == id && !(d->buttons[i].b.fsStyle & BTNS_SEP))
				return static_cast<int>(i);
		}
		return -1;
	}

	RECT rectOf(Window* w, const ToolbarButton& tb)
	{
		auto* bar = qobject_cast<QToolBar*>(w->widget);
		if (!bar || !tb.action || !tb.action->isVisible())
			return RECT{};
		QWidget* button = bar->widgetForAction(tb.action);
		if (!button)
			return RECT{};
		const QRect r = button->geometry();
		return RECT{r.left(), r.top(), r.right() + 1, r.bottom() + 1};
	}

	void layoutRebar(Window* w)
	{
		auto* box = w->widget ? qobject_cast<QHBoxLayout*>(w->widget->layout()) : nullptr;
		if (!box)
			return;
		RebarData* d = rebarData(w);
		int height = 0;
		for (RebarBand& band : d->bands)
		{
			if (!band.child)
				continue;
			const bool hidden = (band.info.fStyle & RBBS_HIDDEN) != 0;
			band.child->setVisible(!hidden);
			if (!hidden)
				height = std::max({height, static_cast<int>(band.info.cyMinChild), static_cast<int>(band.info.cyChild), band.child->sizeHint().height()});
		}
		if (height > 0 && !(w->style & CCS_NORESIZE))
		{
			// the height of the bands (Windows: the rebar sizes itself)
			w->widget->setMinimumHeight(height);
			if (w->widget->height() != height)
				w->widget->resize(w->widget->width(), height);
		}
	}

	void setBand(Window* w, RebarBand& band, const REBARBANDINFOW* info)
	{
		const UINT mask = info->fMask;
		if (mask & RBBIM_STYLE)
			band.info.fStyle = info->fStyle;
		if (mask & RBBIM_ID)
			band.info.wID = info->wID;
		if (mask & RBBIM_LPARAM)
			band.info.lParam = info->lParam;
		if (mask & RBBIM_SIZE)
			band.info.cx = info->cx;
		if (mask & RBBIM_IDEALSIZE)
			band.info.cxIdeal = info->cxIdeal;
		if (mask & RBBIM_CHILDSIZE)
		{
			band.info.cxMinChild = info->cxMinChild;
			band.info.cyMinChild = info->cyMinChild;
			band.info.cyChild = info->cyChild;
			band.info.cyMaxChild = info->cyMaxChild;
		}
		if ((mask & RBBIM_CHILD) && info->hwndChild)
		{
			band.info.hwndChild = info->hwndChild;
			if (QWidget* child = widgetOf(info->hwndChild))
			{
				if (child->parentWidget() != w->widget)
					child->setParent(w->widget);
				band.child = child;
				if (auto* box = qobject_cast<QHBoxLayout*>(w->widget->layout()); box && box->indexOf(child) < 0)
					box->addWidget(child, (band.info.fStyle & RBBS_FIXEDSIZE) ? 0 : 1);
			}
		}
		if (band.child && (mask & RBBIM_CHILDSIZE) && band.info.cxMinChild)
			band.child->setMinimumWidth(static_cast<int>(band.info.cxMinChild));
	}

	void getBand(const RebarBand& band, REBARBANDINFOW* info)
	{
		const UINT mask = info->fMask;
		if (mask & RBBIM_STYLE)
			info->fStyle = band.info.fStyle;
		if (mask & RBBIM_ID)
			info->wID = band.info.wID;
		if (mask & RBBIM_LPARAM)
			info->lParam = band.info.lParam;
		if (mask & RBBIM_CHILD)
			info->hwndChild = band.info.hwndChild;
		if (mask & RBBIM_SIZE)
			info->cx = band.child ? static_cast<UINT>(band.child->width()) : band.info.cx;
		if (mask & RBBIM_IDEALSIZE)
			info->cxIdeal = band.info.cxIdeal;
		if (mask & RBBIM_CHILDSIZE)
		{
			info->cxMinChild = band.info.cxMinChild;
			info->cyMinChild = band.info.cyMinChild;
			info->cyChild = band.info.cyChild;
			info->cyMaxChild = band.info.cyMaxChild;
		}
		if (mask & RBBIM_HEADERSIZE)
			info->cxHeader = 0;
	}
}

namespace w32
{
	void setupToolbar(Window* w)
	{
		auto* bar = new QToolBar(static_cast<QWidget*>(w->handle.widget));
		bar->setFloatable(false);
		bar->setMovable(false);
		bar->setIconSize(QSize(16, 16));
		bar->setToolButtonStyle((w->style & TBSTYLE_LIST) ? Qt::ToolButtonTextBesideIcon : Qt::ToolButtonIconOnly);
		w->widget = bar;
		w->ctl = new ToolbarData;
		w->ctlFree = [](void* p) { delete static_cast<ToolbarData*>(p); };
	}

	void setupRebar(Window* w)
	{
		auto* rebar = new QWidget(static_cast<QWidget*>(w->handle.widget));
		auto* box = new QHBoxLayout(rebar);
		box->setContentsMargins(0, 0, 0, 0);
		box->setSpacing(2);
		w->widget = rebar;
		w->ctl = new RebarData;
		w->ctlFree = [](void* p) { delete static_cast<RebarData*>(p); };
	}

	LRESULT toolbarProc(Window* w, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		ToolbarData* d = toolbarData(w);
		auto* bar = qobject_cast<QToolBar*>(w->widget);
		if (!d || !bar)
			return kNotHandled;
		auto byId = [d](WPARAM id) -> ToolbarButton*
		{
			const int i = indexOfCommand(d, static_cast<int>(id));
			return i < 0 ? nullptr : &d->buttons[static_cast<size_t>(i)];
		};
		auto setFlag = [&](WPARAM id, BYTE flag, bool on) -> LRESULT
		{
			ToolbarButton* tb = byId(id);
			if (!tb)
				return FALSE;
			tb->b.fsState = static_cast<BYTE>(on ? (tb->b.fsState | flag) : (tb->b.fsState & ~flag));
			applyState(w, *tb);
			return TRUE;
		};
		switch (msg)
		{
			case TB_BUTTONSTRUCTSIZE:
				return 0;
			case TB_ADDBUTTONSW:
			{
				const auto* buttons = reinterpret_cast<const TBBUTTON*>(lParam);
				for (WPARAM i = 0; buttons && i < wParam; ++i)
					insertButton(w, -1, buttons[i]);
				return TRUE;
			}
			case TB_INSERTBUTTONW:
				if (const auto* b = reinterpret_cast<const TBBUTTON*>(lParam))
					insertButton(w, static_cast<int>(wParam), *b);
				return TRUE;
			case TB_DELETEBUTTON:
			{
				if (wParam >= d->buttons.size())
					return FALSE;
				if (QAction* a = d->buttons[wParam].action)
				{
					bar->removeAction(a);
					a->deleteLater();
				}
				d->buttons.erase(d->buttons.begin() + static_cast<long>(wParam));
				return TRUE;
			}
			case TB_GETBUTTON:
				if (wParam >= d->buttons.size() || !lParam)
					return FALSE;
				*reinterpret_cast<TBBUTTON*>(lParam) = d->buttons[wParam].b;
				return TRUE;
			case TB_BUTTONCOUNT:
				return static_cast<LRESULT>(d->buttons.size());
			case TB_COMMANDTOINDEX:
				return indexOfCommand(d, static_cast<int>(wParam));
			case TB_ENABLEBUTTON:
				return setFlag(wParam, TBSTATE_ENABLED, LOWORD(lParam) != 0);
			case TB_CHECKBUTTON:
				return setFlag(wParam, TBSTATE_CHECKED, LOWORD(lParam) != 0);
			case TB_PRESSBUTTON:
				return setFlag(wParam, TBSTATE_PRESSED, LOWORD(lParam) != 0);
			case TB_HIDEBUTTON:
				return setFlag(wParam, TBSTATE_HIDDEN, LOWORD(lParam) != 0);
			case TB_INDETERMINATE:
				return setFlag(wParam, TBSTATE_INDETERMINATE, LOWORD(lParam) != 0);
			case TB_ISBUTTONENABLED: { ToolbarButton* tb = byId(wParam); return tb && (tb->b.fsState & TBSTATE_ENABLED); }
			case TB_ISBUTTONCHECKED: { ToolbarButton* tb = byId(wParam); return tb && (tb->b.fsState & TBSTATE_CHECKED); }
			case TB_ISBUTTONPRESSED: { ToolbarButton* tb = byId(wParam); return tb && (tb->b.fsState & TBSTATE_PRESSED); }
			case TB_ISBUTTONHIDDEN: { ToolbarButton* tb = byId(wParam); return tb && (tb->b.fsState & TBSTATE_HIDDEN); }
			case TB_SETSTATE:
			{
				ToolbarButton* tb = byId(wParam);
				if (!tb)
					return FALSE;
				tb->b.fsState = static_cast<BYTE>(LOWORD(lParam));
				applyState(w, *tb);
				return TRUE;
			}
			case TB_GETSTATE:
			{
				ToolbarButton* tb = byId(wParam);
				return tb ? tb->b.fsState : -1;
			}
			case TB_GETBITMAP:
			{
				ToolbarButton* tb = byId(wParam);
				return tb ? tb->b.iBitmap : 0;
			}
			case TB_SETCMDID:
				if (wParam >= d->buttons.size())
					return FALSE;
				d->buttons[wParam].b.idCommand = static_cast<int>(lParam);
				return TRUE;
			case TB_ADDBITMAP:
			{
				const auto* ab = reinterpret_cast<const TBADDBITMAP*>(lParam);
				if (!ab)
					return -1;
				const int first = static_cast<int>(d->bitmapIcons.size());
				const int count = static_cast<int>(wParam);
				if (ab->hInst == HINST_COMMCTRL)
				{
					const int n = std::max(count, 15);
					for (int i = 0; i < n; ++i)
						d->bitmapIcons.push_back(standardIcon(ab->nID, i));
					return first;
				}
				HBITMAP bmp = ab->hInst ? LoadBitmapW(ab->hInst, MAKEINTRESOURCEW(ab->nID)) : reinterpret_cast<HBITMAP>(ab->nID);
				const QPixmap strip = pixmapOf(bmp);
				if (strip.isNull() || count <= 0)
					return -1;
				const int width = strip.width() / count;
				for (int i = 0; i < count; ++i)
					d->bitmapIcons.push_back(QIcon(strip.copy(i * width, 0, width, strip.height())));
				refreshIcons(w);
				return first;
			}
			case TB_ADDSTRINGW:
			{
				const int first = static_cast<int>(d->strings.size());
				if (!lParam)
					return -1;
				if (wParam && isIntResource(reinterpret_cast<LPCWSTR>(lParam)))
				{
					wchar_t buf[1024] = {};
					LoadStringW(reinterpret_cast<HINSTANCE>(wParam), static_cast<UINT>(intResource(reinterpret_cast<LPCWSTR>(lParam))), buf, 1024);
					d->strings.push_back(qs(buf));
					return first;
				}
				// strings separated by nuls, two nuls at the end
				for (auto* p = reinterpret_cast<LPCWSTR>(lParam); *p; p += wcslen(p) + 1)
					d->strings.push_back(qs(p));
				return first;
			}
			case TB_GETITEMRECT:
				if (wParam >= d->buttons.size() || !lParam)
					return FALSE;
				*reinterpret_cast<RECT*>(lParam) = rectOf(w, d->buttons[wParam]);
				return TRUE;
			case TB_GETRECT:
			{
				ToolbarButton* tb = byId(wParam);
				if (!tb || !lParam)
					return FALSE;
				*reinterpret_cast<RECT*>(lParam) = rectOf(w, *tb);
				return TRUE;
			}
			case TB_SETIMAGELIST:
			{
				HIMAGELIST old = d->images;
				d->images = reinterpret_cast<HIMAGELIST>(lParam);
				if (d->images)
				{
					int cx = 16, cy = 16;
					ImageList_GetIconSize(d->images, &cx, &cy);
					bar->setIconSize(QSize(cx, cy));
				}
				refreshIcons(w);
				return reinterpret_cast<LRESULT>(old);
			}
			case TB_GETIMAGELIST:
				return reinterpret_cast<LRESULT>(d->images);
			case TB_SETHOTIMAGELIST:
			{
				HIMAGELIST old = d->hotImages;
				d->hotImages = reinterpret_cast<HIMAGELIST>(lParam);
				return reinterpret_cast<LRESULT>(old);
			}
			case TB_GETHOTIMAGELIST:
				return reinterpret_cast<LRESULT>(d->hotImages);
			case TB_SETDISABLEDIMAGELIST:
			{
				HIMAGELIST old = d->disabledImages;
				d->disabledImages = reinterpret_cast<HIMAGELIST>(lParam);
				return reinterpret_cast<LRESULT>(old);
			}
			case TB_GETDISABLEDIMAGELIST:
				return reinterpret_cast<LRESULT>(d->disabledImages);
			case TB_SETBUTTONSIZE:
				d->buttonSize = QSize(LOWORD(lParam), HIWORD(lParam));
				return TRUE;
			case TB_SETBITMAPSIZE:
				bar->setIconSize(QSize(LOWORD(lParam), HIWORD(lParam)));
				return TRUE;
			case TB_GETBUTTONSIZE:
			{
				QSize s = d->buttonSize;
				for (const ToolbarButton& tb : d->buttons)
				{
					if (tb.action && !(tb.b.fsStyle & BTNS_SEP))
					{
						if (QWidget* b = bar->widgetForAction(tb.action))
							s = b->sizeHint();
						break;
					}
				}
				return MAKELONG(s.width(), s.height());
			}
			case TB_AUTOSIZE:
				bar->adjustSize();
				return 0;
			case TB_GETIDEALSIZE:
			case TB_GETMAXSIZE:
				if (auto* size = reinterpret_cast<SIZE*>(lParam))
				{
					const QSize s = bar->sizeHint();
					size->cx = s.width();
					size->cy = s.height();
				}
				return TRUE;
			case TB_SETEXTENDEDSTYLE:
			{
				const DWORD old = d->exStyle;
				d->exStyle = wParam ? ((d->exStyle & ~static_cast<DWORD>(wParam)) | (static_cast<DWORD>(lParam) & static_cast<DWORD>(wParam))) : static_cast<DWORD>(lParam);
				return old;
			}
			case TB_GETEXTENDEDSTYLE:
				return d->exStyle;
			case TB_GETPADDING:
				return d->padding;
			case TB_SETPADDING:
			{
				const DWORD old = d->padding;
				d->padding = static_cast<DWORD>(lParam);
				return old;
			}
			case TB_SETSTYLE:
				w->style = static_cast<DWORD>(lParam);
				return 0;
			case TB_GETSTYLE:
				return w->style;
			case TB_SETPARENT:
			{
				HWND old = w->parent;
				w->parent = reinterpret_cast<HWND>(wParam);
				return reinterpret_cast<LRESULT>(old);
			}
			case TB_GETBUTTONINFOW:
			case TB_SETBUTTONINFOW:
			{
				auto* info = reinterpret_cast<TBBUTTONINFOW*>(lParam);
				if (!info)
					return -1;
				const int i = (info->dwMask & TBIF_BYINDEX) ? static_cast<int>(wParam) : indexOfCommand(d, static_cast<int>(wParam));
				if (i < 0 || i >= static_cast<int>(d->buttons.size()))
					return -1;
				ToolbarButton& tb = d->buttons[static_cast<size_t>(i)];
				if (msg == TB_GETBUTTONINFOW)
				{
					if (info->dwMask & TBIF_COMMAND)
						info->idCommand = tb.b.idCommand;
					if (info->dwMask & TBIF_IMAGE)
						info->iImage = tb.b.iBitmap;
					if (info->dwMask & TBIF_STATE)
						info->fsState = tb.b.fsState;
					if (info->dwMask & TBIF_STYLE)
						info->fsStyle = tb.b.fsStyle;
					if (info->dwMask & TBIF_LPARAM)
						info->lParam = tb.b.dwData;
					if ((info->dwMask & TBIF_TEXT) && info->pszText && tb.action)
						copyOut(tb.action->text(), info->pszText, info->cchText);
					return i;
				}
				if (info->dwMask & TBIF_COMMAND)
					tb.b.idCommand = info->idCommand;
				if (info->dwMask & TBIF_IMAGE)
				{
					tb.b.iBitmap = info->iImage;
					if (tb.action)
						tb.action->setIcon(buttonIcon(d, info->iImage));
				}
				if (info->dwMask & TBIF_STATE)
				{
					tb.b.fsState = info->fsState;
					applyState(w, tb);
				}
				if (info->dwMask & TBIF_STYLE)
					tb.b.fsStyle = info->fsStyle;
				if (info->dwMask & TBIF_LPARAM)
					tb.b.dwData = info->lParam;
				if ((info->dwMask & TBIF_TEXT) && tb.action)
					tb.action->setText(info->pszText ? qs(info->pszText) : QString());
				return TRUE;
			}
			case TB_GETBUTTONTEXTW:
			{
				ToolbarButton* tb = byId(wParam);
				if (!tb || !tb->action)
					return -1;
				const QString t = tb->action->text();
				if (lParam)
					copyOut(t, reinterpret_cast<LPWSTR>(lParam), static_cast<int>(t.size()) + 1);
				return static_cast<LRESULT>(t.size());
			}
			case TB_GETTOOLTIPS:
				return 0;
			case TB_SETTOOLTIPS:
			case TB_CUSTOMIZE:
				return 0;
			case TB_SETMAXTEXTROWS:
			case TB_SETINDENT:
			case TB_SETBUTTONWIDTH:
				return TRUE;
			case TB_SETROWS:
				return 0;
			case TB_GETROWS:
			case TB_GETTEXTROWS:
				return 1;
			case TB_GETHOTITEM:
			case TB_SETHOTITEM:
				return -1;
			default:
				return kNotHandled;
		}
	}

	LRESULT rebarProc(Window* w, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		RebarData* d = rebarData(w);
		if (!d || !w->widget)
			return kNotHandled;
		switch (msg)
		{
			case RB_INSERTBANDW:
			{
				const auto* info = reinterpret_cast<const REBARBANDINFOW*>(lParam);
				if (!info)
					return FALSE;
				RebarBand band;
				int index = static_cast<int>(static_cast<intptr_t>(wParam));
				if (index < 0 || index > static_cast<int>(d->bands.size()))
					index = static_cast<int>(d->bands.size());
				d->bands.insert(d->bands.begin() + index, band);
				setBand(w, d->bands[static_cast<size_t>(index)], info);
				layoutRebar(w);
				return TRUE;
			}
			case RB_DELETEBAND:
			{
				if (wParam >= d->bands.size())
					return FALSE;
				if (QWidget* child = d->bands[wParam].child)
				{
					if (auto* box = qobject_cast<QHBoxLayout*>(w->widget->layout()))
						box->removeWidget(child);
					child->hide();
				}
				d->bands.erase(d->bands.begin() + static_cast<long>(wParam));
				layoutRebar(w);
				return TRUE;
			}
			case RB_SETBANDINFOW:
				if (wParam >= d->bands.size() || !lParam)
					return FALSE;
				setBand(w, d->bands[wParam], reinterpret_cast<const REBARBANDINFOW*>(lParam));
				layoutRebar(w);
				return TRUE;
			case RB_GETBANDINFOW:
				if (wParam >= d->bands.size() || !lParam)
					return FALSE;
				getBand(d->bands[wParam], reinterpret_cast<REBARBANDINFOW*>(lParam));
				return TRUE;
			case RB_SHOWBAND:
				if (wParam >= d->bands.size())
					return FALSE;
				d->bands[wParam].info.fStyle = lParam ? (d->bands[wParam].info.fStyle & ~static_cast<UINT>(RBBS_HIDDEN))
					: (d->bands[wParam].info.fStyle | RBBS_HIDDEN);
				layoutRebar(w);
				return TRUE;
			case RB_IDTOINDEX:
				for (size_t i = 0; i < d->bands.size(); ++i)
				{
					if (d->bands[i].info.wID == static_cast<UINT>(wParam))
						return static_cast<LRESULT>(i);
				}
				return -1;
			case RB_GETBANDCOUNT:
				return static_cast<LRESULT>(d->bands.size());
			case RB_GETROWCOUNT:
				return d->bands.empty() ? 0 : 1;
			case RB_GETBARHEIGHT:
			case RB_GETROWHEIGHT:
				return std::max(w->widget->height(), w->widget->minimumHeight());
			case RB_SETBARINFO:
				return TRUE;
			case RB_GETBARINFO:
				return TRUE;
			case RB_SETPARENT:
			{
				HWND old = w->parent;
				w->parent = reinterpret_cast<HWND>(wParam);
				return reinterpret_cast<LRESULT>(old);
			}
			case RB_MAXIMIZEBAND:
			case RB_MINIMIZEBAND:
				return 0;
			default:
				return kNotHandled;
		}
	}
}
