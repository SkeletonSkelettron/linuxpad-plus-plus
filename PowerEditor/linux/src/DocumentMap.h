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

// View > Document Map: Qt counterpart of WinControls/DocumentMap/documentMap.cpp.
// A zoomed out view of the document of the active view, with the visible part of
// the editor marked (ViewZoneDlg). Click, drag, wheel and arrow keys scroll the editor.

#pragma once

#include <QColor>
#include <QWidget>

#include "NppCommon.h"

class EditView;
class MainWindow;
class ZoneOverlay;
struct Buffer;

class DocumentMap : public QWidget
{
	Q_OBJECT

public:
	DocumentMap(MainWindow* mw, QWidget* parent);

	void reloadMap(bool restyle = false); // show the document of the active view (reloadMap() + setSyntaxHiliting())
	void scrollMap();   // follow the editor, mark its visible part
	void wrapMap();     // wrap like the editor (wrapMap() / initWrapMap())
	bool needToRewrap() const; // DocumentMap::needToRecomputeWith()
	void syncFolding(); // contracted folds of the editor
	void showTemporarily(Buffer* buf, int view); // MISC. > Peek on document map
	bool isTemporary() const { return _temporary; }
	void applyStyle();  // "Document map" colours of the theme

	EditView* mapView() const { return _map; }
	QWidget* zoneWidget() const;
	int zoneTop() const { return _zoneTop; }
	int zoneBottom() const { return _zoneBottom; }
	void scrollEditorTo(int y); // DOCUMENTMAP_MOUSECLICKED: centre of the zone moves to y
	void scrollEditor(bool down, bool page); // DOCUMENTMAP_SCROLL
	void wheelEditor(int angleDelta, bool zoom); // DOCUMENTMAP_MOUSEWHEEL

protected:
	void resizeEvent(QResizeEvent* event) override;

private:
	friend class ZoneOverlay;
	void layoutMap();
	void setZone(int top, int bottom);
	void setSyntaxHiliting(LangType lang, bool largeFile, const QString& udlName);

	MainWindow* _mw;
	EditView* _map = nullptr;
	ZoneOverlay* _overlay = nullptr;
	int _zoneTop = 0;
	int _zoneBottom = 0;
	int _mapWidth = -1; // wrapped: width of the map view, -1 the whole panel
	int _wrapEditWidth = -1; // editor width and zoom the map was wrapped for
	sptr_t _wrapZoom = 0;
	int _wrapMode = SC_WRAP_NONE;
	QColor _focus = QColor(0xFF, 0x80, 0x00);
	QColor _frost = QColor(0xFF, 0xFF, 0xFF);
	bool _temporary = false;
};
