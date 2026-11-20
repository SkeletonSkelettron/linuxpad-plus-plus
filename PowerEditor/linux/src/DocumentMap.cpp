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

#include "DocumentMap.h"

#include <algorithm>
#include <vector>

#include <QApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

#include "EditView.h"
#include "FileIO.h"
#include "LangStyles.h"
#include "MainWindow.h"

// ViewZoneDlg: the panel over the map, frosted, with the visible part of the editor
// in the focus colour (both drawn at 50/256 opacity, NppParameters::setTransparent(50))
class ZoneOverlay : public QWidget
{
public:
	explicit ZoneOverlay(DocumentMap* map)
		: QWidget(map), _dm(map)
	{
		setObjectName(QStringLiteral("documentMapZone"));
		setFocusPolicy(Qt::ClickFocus);
		setAttribute(Qt::WA_NoSystemBackground);
		setMouseTracking(false);
	}

protected:
	void paintEvent(QPaintEvent*) override
	{
		QPainter p(this);
		QColor frost = _dm->_frost;
		QColor focus = _dm->_focus;
		frost.setAlpha(50);
		focus.setAlpha(50);
		p.fillRect(rect(), frost);
		const int top = std::max(0, _dm->_zoneTop);
		const int bottom = std::min(height(), _dm->_zoneBottom);
		if (bottom > top)
			p.fillRect(QRect(0, top, width(), bottom - top), focus);
	}
	void mousePressEvent(QMouseEvent* event) override
	{
		if (event->button() == Qt::LeftButton)
			_dm->scrollEditorTo(static_cast<int>(event->position().y()));
		event->accept();
	}
	void mouseMoveEvent(QMouseEvent* event) override
	{
		if (event->buttons() & Qt::LeftButton)
			_dm->scrollEditorTo(static_cast<int>(event->position().y()));
		event->accept();
	}
	void mouseDoubleClickEvent(QMouseEvent* event) override { event->accept(); }
	void wheelEvent(QWheelEvent* event) override
	{
		_dm->wheelEditor(event->angleDelta().y(), event->modifiers() & Qt::ControlModifier);
		event->accept();
	}
	void keyPressEvent(QKeyEvent* event) override
	{
		// ViewZoneDlg::CanvasProc WM_KEYDOWN
		switch (event->key())
		{
			case Qt::Key_Up: _dm->scrollEditor(false, false); break;
			case Qt::Key_Down: _dm->scrollEditor(true, false); break;
			case Qt::Key_PageUp: _dm->scrollEditor(false, true); break;
			case Qt::Key_PageDown: _dm->scrollEditor(true, true); break;
			default: QWidget::keyPressEvent(event); return;
		}
		event->accept();
	}

private:
	DocumentMap* _dm;
};

DocumentMap::DocumentMap(MainWindow* mw, QWidget* parent)
	: QWidget(parent), _mw(mw)
{
	setObjectName(QStringLiteral("documentMap"));
	setMinimumWidth(40);

	// WM_INITDIALOG: zoom -10, no scroll bars, no modification events, no margins
	_map = new EditView(this, false);
	_map->setObjectName(QStringLiteral("documentMapView"));
	_map->setFrameShape(QFrame::NoFrame);
	_map->setFocusPolicy(Qt::NoFocus);
	_map->viewport()->setFocusPolicy(Qt::NoFocus);
	_map->ex(SCI_SETZOOM, static_cast<uptr_t>(-10));
	_map->ex(SCI_SETVSCROLLBAR, false);
	_map->ex(SCI_SETHSCROLLBAR, false);
	_map->ex(SCI_SETMODEVENTMASK, 0);
	_map->ex(SCI_SETCARETSTYLE, CARETSTYLE_INVISIBLE);
	_map->ex(SCI_SETCARETLINEVISIBLE, false);

	_overlay = new ZoneOverlay(this);
	_overlay->raise();
	applyStyle();
}

QWidget* DocumentMap::zoneWidget() const
{
	return _overlay;
}

void DocumentMap::applyStyle()
{
	// ViewZoneDlg::setColour() from the "Document map" global style
	if (const Style* s = StyleManager::instance().globalStyles().findByName(QStringLiteral("Document map")))
	{
		if (s->fgColor != static_cast<COLORREF>(STYLE_NOT_USED))
			_focus = toQColor(s->fgColor);
		if (s->bgColor != static_cast<COLORREF>(STYLE_NOT_USED))
			_frost = toQColor(s->bgColor);
	}
	_overlay->update();
}

void DocumentMap::reloadMap(bool restyle)
{
	// DocumentMap::reloadMap() + setSyntaxHiliting()
	const bool wasTemporary = _temporary;
	_temporary = false;
	EditView* e = _mw->editView();
	Buffer* b = _mw->currentBuffer();
	if (!e || !b)
	{
		_map->send(SCI_SETDOCPOINTER, 0, 0);
		setZone(0, 0);
		return;
	}
	const sptr_t doc = e->send(SCI_GETDOCPOINTER);
	if (_map->send(SCI_GETDOCPOINTER) != doc)
	{
		_map->send(SCI_SETDOCPOINTER, 0, doc);
		restyle = true;
	}
	if (restyle || wasTemporary || _map->currentLang() != e->currentLang() || _map->isLargeFile() != e->isLargeFile() || _map->udlName() != e->udlName())
		setSyntaxHiliting(e->currentLang(), e->isLargeFile(), e->udlName());
	syncFolding();
	wrapMap();
	scrollMap();
}

void DocumentMap::setSyntaxHiliting(LangType lang, bool largeFile, const QString& udlName)
{
	// the lexer belongs to the document, the styles to each view
	_map->setLargeFile(largeFile);
	_map->defineDocType(lang, udlName);
	for (int m = 0; m < 5; ++m)
		_map->ex(SCI_SETMARGINWIDTHN, m, 0);
	_map->ex(SCI_SETINDENTATIONGUIDES, SC_IV_NONE);
	_map->ex(SCI_SETCARETLINEVISIBLE, false);
	_map->ex(SCI_SETCARETSTYLE, CARETSTYLE_INVISIBLE);
	_map->ex(SCI_SETZOOM, static_cast<uptr_t>(-10));
}

void DocumentMap::showTemporarily(Buffer* buf, int view)
{
	// DocumentMap::showInMapTemporarily() + scrollMapWith()
	if (!buf)
		return;
	_temporary = true;
	EditView* e = _mw->viewEdit(view);
	_map->send(SCI_SETDOCPOINTER, 0, buf->doc);
	setSyntaxHiliting(buf->lang, buf->largeFile, buf->udlName);
	wrapMap();

	const sptr_t first = buf->pos[view].firstVisibleLine;
	const sptr_t nbLine = e->ex(SCI_LINESONSCREEN);
	const sptr_t last = first + nbLine;
	const sptr_t lastVisibleMap = _map->ex(SCI_DOCLINEFROMVISIBLE, _map->ex(SCI_GETFIRSTVISIBLELINE) + _map->ex(SCI_LINESONSCREEN));
	_map->ex(SCI_GOTOLINE, lastVisibleMap < last ? last : first);
	_map->ex(SCI_GOTOLINE, first);
	const int higherY = static_cast<int>(_map->ex(SCI_POINTYFROMPOSITION, 0, _map->ex(SCI_POSITIONFROMLINE, first)));
	const int lineHeight = static_cast<int>(_map->ex(SCI_TEXTHEIGHT, first));
	setZone(higherY, higherY + static_cast<int>(nbLine) * lineHeight);
}

void DocumentMap::syncFolding()
{
	// Buffer::getHeaderLineState() / ScintillaEditView::syncFoldStateWith(): only the contracted headers
	EditView* e = _mw->editView();
	if (!e || _map->send(SCI_GETDOCPOINTER) != e->send(SCI_GETDOCPOINTER))
		return;
	auto contracted = [](EditView* v)
	{
		std::vector<sptr_t> lines;
		for (sptr_t l = v->ex(SCI_CONTRACTEDFOLDNEXT, 0); l >= 0; l = v->ex(SCI_CONTRACTEDFOLDNEXT, l + 1))
			lines.push_back(l);
		return lines;
	};
	const std::vector<sptr_t> inEditor = contracted(e);
	const std::vector<sptr_t> inMap = contracted(_map);
	for (sptr_t l : inMap)
	{
		if (!std::binary_search(inEditor.begin(), inEditor.end(), l))
			_map->ex(SCI_FOLDLINE, l, SC_FOLDACTION_EXPAND);
	}
	for (sptr_t l : inEditor)
	{
		if (!std::binary_search(inMap.begin(), inMap.end(), l))
			_map->ex(SCI_FOLDLINE, l, SC_FOLDACTION_CONTRACT);
	}
	if (!_temporary)
		scrollMap();
}

bool DocumentMap::needToRewrap() const
{
	const EditView* e = _mw->editView();
	if (!e)
		return false;
	const int mode = static_cast<int>(e->ex(SCI_GETWRAPMODE));
	if (mode != _wrapMode)
		return true;
	return mode != SC_WRAP_NONE && (e->ex(SCI_GETZOOM) != _wrapZoom || e->viewport()->width() != _wrapEditWidth);
}

void DocumentMap::wrapMap()
{
	// DocumentMap::wrapMap(): a wrapped editor wraps the map at the same columns
	EditView* e = _mw->editView();
	if (!e)
		return;
	_wrapMode = static_cast<int>(e->ex(SCI_GETWRAPMODE));
	_wrapZoom = e->ex(SCI_GETZOOM);
	_wrapEditWidth = e->viewport()->width();
	if (e->ex(SCI_GETWRAPMODE) == SC_WRAP_NONE)
	{
		_mapWidth = -1;
		_map->ex(SCI_SETWRAPMODE, SC_WRAP_NONE);
		_map->ex(SCI_SETMARGINLEFT, 0, 0);
		_map->ex(SCI_SETMARGINRIGHT, 0, 0);
	}
	else
	{
		int marginsWidth = 0;
		for (int m = 0; m < 5; ++m)
			marginsWidth += static_cast<int>(e->ex(SCI_GETMARGINWIDTHN, m));
		const int paddingLeft = static_cast<int>(e->ex(SCI_GETMARGINLEFT));
		const int paddingRight = static_cast<int>(e->ex(SCI_GETMARGINRIGHT));
		const int editZoneWidth = e->viewport()->width() - marginsWidth - paddingLeft - paddingRight;
		// zoomRatio[] of documentMap.cpp, measured on the fonts in use
		static const char sample[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
		const double editText = static_cast<double>(e->exs(SCI_TEXTWIDTH, STYLE_DEFAULT, sample));
		const double mapText = static_cast<double>(_map->exs(SCI_TEXTWIDTH, STYLE_DEFAULT, sample));
		const double ratio = (mapText > 0 && editText > 0) ? editText / mapText : 4.0;
		const double docMapWidth = editZoneWidth / ratio;
		_mapWidth = std::max(8, static_cast<int>(docMapWidth));
		_map->ex(SCI_SETWRAPMODE, e->ex(SCI_GETWRAPMODE));
		_map->ex(SCI_SETWRAPINDENTMODE, e->ex(SCI_GETWRAPINDENTMODE));
		_map->ex(SCI_SETMARGINLEFT, 0, static_cast<sptr_t>(paddingLeft / ratio));
		_map->ex(SCI_SETMARGINRIGHT, 0, static_cast<sptr_t>(paddingRight / ratio));
	}
	layoutMap();
}

void DocumentMap::layoutMap()
{
	const int w = (_mapWidth > 0) ? std::min(_mapWidth, width()) : width();
	_map->setGeometry(0, 0, w, height());
	_overlay->setGeometry(0, 0, w, height());
	_overlay->raise();
}

void DocumentMap::resizeEvent(QResizeEvent* event)
{
	QWidget::resizeEvent(event);
	layoutMap();
	if (!_temporary)
		scrollMap();
}

void DocumentMap::setZone(int top, int bottom)
{
	if (top == _zoneTop && bottom == _zoneBottom)
		return;
	_zoneTop = top;
	_zoneBottom = bottom;
	_overlay->update();
}

void DocumentMap::scrollMap()
{
	// DocumentMap::scrollMap()
	EditView* e = _mw->editView();
	if (!e || _temporary || _map->send(SCI_GETDOCPOINTER) != e->send(SCI_GETDOCPOINTER))
		return;
	const QSize editSize = e->viewport()->size();
	const sptr_t higherPos = e->ex(SCI_POSITIONFROMPOINT, 0, 0);
	const sptr_t lowerPos = e->ex(SCI_POSITIONFROMPOINT, editSize.width(), editSize.height());

	// let Scintilla scroll the map
	_map->ex(SCI_GOTOPOS, higherPos);
	_map->ex(SCI_GOTOPOS, lowerPos);
	if (_mapWidth < 0)
		_map->ex(SCI_SETXOFFSET, 0);

	const sptr_t higherY = _map->ex(SCI_POINTYFROMPOSITION, 0, higherPos);
	const sptr_t lineHeightMap = _map->ex(SCI_TEXTHEIGHT, 0);
	sptr_t lowerY = 0;
	if (e->ex(SCI_GETWRAPMODE) == SC_WRAP_NONE)
	{
		// not wrapped: mimic the height of the edit view
		const sptr_t lineHeightEdit = std::max<sptr_t>(1, e->ex(SCI_TEXTHEIGHT, 0));
		lowerY = higherY + lineHeightMap * editSize.height() / lineHeightEdit;
	}
	else
	{
		// wrapped: ask Scintilla, the range may be wrapped differently in the map
		lowerY = _map->ex(SCI_POINTYFROMPOSITION, 0, lowerPos) + lineHeightMap;
	}
	setZone(static_cast<int>(higherY), static_cast<int>(lowerY));
}

void DocumentMap::scrollEditorTo(int y)
{
	// DOCUMENTMAP_MOUSECLICKED
	EditView* e = _mw->editView();
	if (!e || _temporary)
		return;
	const int center = (_zoneTop + _zoneBottom) / 2;
	const sptr_t pixelPerLine = _map->ex(SCI_TEXTHEIGHT, 0);
	if (pixelPerLine <= 0)
		return;
	const sptr_t nbLine2jump = (y - center) / pixelPerLine;
	if (nbLine2jump != 0)
		e->ex(SCI_LINESCROLL, 0, nbLine2jump);
	scrollMap();
}

void DocumentMap::scrollEditor(bool down, bool page)
{
	// DocumentMap::scrollMap(bool direction, moveMode whichMode)
	EditView* e = _mw->editView();
	if (!e)
		return;
	const sptr_t nbLine = page ? e->ex(SCI_LINESONSCREEN) : 1;
	e->ex(SCI_LINESCROLL, 0, down ? nbLine : -nbLine);
	scrollMap();
}

void DocumentMap::wheelEditor(int angleDelta, bool zoom)
{
	// DOCUMENTMAP_MOUSEWHEEL: the wheel acts on the editor
	EditView* e = _mw->editView();
	if (!e || angleDelta == 0)
		return;
	if (zoom)
	{
		e->ex(angleDelta > 0 ? SCI_ZOOMIN : SCI_ZOOMOUT);
		return;
	}
	const int lines = -angleDelta * QApplication::wheelScrollLines() / 120;
	e->ex(SCI_LINESCROLL, 0, lines != 0 ? lines : (angleDelta > 0 ? -1 : 1));
	scrollMap();
}
