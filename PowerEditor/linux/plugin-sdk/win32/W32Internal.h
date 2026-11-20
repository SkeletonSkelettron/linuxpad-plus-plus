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

// Notepad++ Linux plugin SDK, npp_win32 library: what its parts share.

#pragma once

#include <map>
#include <memory>
#include <string>
#include <vector>

#include <QColor>
#include <QFont>
#include <QIcon>
#include <QPointer>
#include <QString>
#include <QWidget>

class QEventLoop;
class QPainter;
class QTimer;

#include <windows.h>
#include <commctrl.h>
#include "npp_win32/w32_resources.h"

namespace w32
{
	// ---- strings -------------------------------------------------------------------
	QString qs(const wchar_t* s, int len = -1);
	QString qsA(const char* s, int len = -1); // UTF-8
	std::wstring ws(const QString& s);
	// copies s into buf (max wchar_t with the nul), returns the length copied
	int copyOut(const QString& s, wchar_t* buf, int max);
	int copyOutA(const QString& s, char* buf, int max);
	QString nativePath(const QString& windowsPath); // '\\' as '/', "C:" dropped
	QString nativePath(const wchar_t* windowsPath);
	inline bool isIntResource(const void* p) { return (reinterpret_cast<uintptr_t>(p) >> 16) == 0; }
	inline int intResource(const void* p) { return static_cast<int>(reinterpret_cast<uintptr_t>(p) & 0xFFFF); }

	// ---- windows -------------------------------------------------------------------
	enum class Kind
	{
		Dialog, Custom, Button, Edit, Static, ComboBox, ListBox, ScrollBar,
		ListView, TreeView, Tab, Trackbar, UpDown, Progress, Tooltip, Link, Header, StatusBar, Scintilla, Generic
	};

	struct Subclass
	{
		SUBCLASSPROC proc;
		UINT_PTR id;
		DWORD_PTR ref;
	};

	struct Window
	{
		NppHandle handle;
		QPointer<QWidget> widget;
		Kind kind = Kind::Generic;
		std::wstring className;
		WNDPROC wndProc = nullptr;   // GWLP_WNDPROC (class or subclass)
		WNDPROC classProc = nullptr; // procedure of the class
		DLGPROC dlgProc = nullptr;
		HWND parent = nullptr;
		HWND owner = nullptr;
		int id = 0;
		DWORD style = 0;
		DWORD exStyle = 0;
		LONG_PTR userData = 0;
		LONG_PTR dlgUser = 0;
		LONG_PTR msgResult = 0;
		std::vector<BYTE> extra; // cbWndExtra
		std::map<std::wstring, HANDLE> props;
		std::vector<Subclass> subclasses;
		size_t subclassDepth = 0; // DefSubclassProc: next subclass to call
		HMENU menu = nullptr;
		HFONT font = nullptr;
		HICON icon = nullptr;
		HBRUSH background = nullptr; // class background brush
		HCURSOR cursor = nullptr;
		// dialogs
		bool isDialog = false;
		bool modal = false;
		bool ended = false;
		INT_PTR result = 0;
		QEventLoop* loop = nullptr;
		int defId = IDOK;
		const NppRcDialog* tmpl = nullptr;
		// state of controls
		bool checkPending = false; // BS_CHECKBOX: Qt changed the state on a click
		int checkState = 0;
		HWND buddy = nullptr;      // up-down
		void* ctl = nullptr;       // data of list views, tree views, tooltips...
		void (*ctlFree)(void*) = nullptr;
		std::map<UINT_PTR, QTimer*> timers;
		bool destroyed = false;
		bool inDestroy = false;
		bool trackLeave = false;
		bool settingText = false;
		bool redraw = true;
		HRGN region = nullptr;
		QPainter* painter = nullptr; // during WM_PAINT
	};

	Window* win(HWND h);         // the window of a handle made here, nullptr otherwise
	Window* winOf(QObject* obj); // the window of a widget (or of the nearest ancestor)
	HWND hwnd(Window* w);
	QWidget* widgetOf(HWND h);    // widget of any handle (also the host's)
	QWidget* parentWidgetOf(HWND parent); // where a child window of parent goes
	Window* newWindow(Kind kind, QWidget* widget, HWND parent, int id, DWORD style, DWORD exStyle, const std::wstring& className);
	void destroyWindow(Window* w);
	LRESULT dispatch(Window* w, UINT msg, WPARAM wParam, LPARAM lParam); // subclasses, then GWLP_WNDPROC
	LRESULT defaultProc(Window* w, UINT msg, WPARAM wParam, LPARAM lParam); // DefWindowProc / DefDlgProc of the window
	LRESULT controlProc(Window* w, UINT msg, WPARAM wParam, LPARAM lParam); // built-in control procedures
	LRESULT commonControlProc(Window* w, UINT msg, WPARAM wParam, LPARAM lParam); // list view, tree view...
	bool isCommonControl(Kind k);
	void notifyCommand(Window* ctl, int code); // WM_COMMAND to the parent
	LRESULT notify(Window* ctl, NMHDR* hdr, UINT code); // WM_NOTIFY to the parent
	Window* dialogOf(Window* w); // the dialog a control belongs to
	void installControlFilter(Window* w, QObject* target = nullptr);
	QWidget* createControl(Kind kind, const std::wstring& cls, DWORD style, QWidget* parent, const QString& text, int textId, Window** out);
	Kind kindOfClass(const std::wstring& cls);
	void setupCommonControl(Window* w);

	// was the default procedure of a control reached for a key or mouse message?
	extern thread_local bool g_defaultReached;

	// ---- dialogs ---------------------------------------------------------------------
	QSize baseUnits(QWidget* w);
	QRect dluRect(QWidget* dlg, int x, int y, int cx, int cy);
	HWND createDialog(const NppRcDialog* t, HWND parent, DLGPROC proc, LPARAM init, bool modal);

	// ---- resources -------------------------------------------------------------------
	const NppRcDialog* findDialog(LPCWSTR name);
	const NppRcDialog* dialogOfTemplate(LPCDLGTEMPLATE t);
	const NppRcMenu* findMenu(LPCWSTR name);
	const NppRcAccelTable* findAccel(LPCWSTR name);
	const NppRcData* findData(LPCWSTR name, LPCWSTR type);
	const NppRcData* findImage(LPCWSTR name, int type);
	const NppRcString* findString(int id);
	const NppRcVersion* versionInfo();
	QString moduleFile(); // path of the plugin .so
	HINSTANCE moduleHandle();

	// ---- menus -----------------------------------------------------------------------
	QMenu* menuOf(HMENU h);
	HMENU menuHandle(QMenu* m);
	HMENU buildMenu(const NppRcMenu* m);

	// ---- GDI ---------------------------------------------------------------------------
	enum class GdiKind { Pen = 1, Brush = 2, Font = 6, Bitmap = 7, Icon = 20, Cursor = 21, Region = 22 };
	struct Gdi
	{
		NppImageHeader header; // qicon: icons and bitmaps
		GdiKind kind = GdiKind::Brush;
		bool stock = false;
		QColor color;
		int penStyle = PS_SOLID;
		int width = 1;
		bool nullBrush = false;
		QFont font;
		LOGFONTW logFont{};
		QPixmap* pixmap = nullptr; // bitmaps: QPixmap (or image of a memory DC)
		QImage* image = nullptr;   // DIB sections and bitmaps selected in memory DCs
		QIcon* icon = nullptr;
		QCursor* cursor = nullptr;
		QRegion* region = nullptr;
	};
	Gdi* gdi(HGDIOBJ h);
	HGDIOBJ newGdi(Gdi* g);
	HICON iconHandle(const QIcon& icon);
	HBITMAP bitmapHandle(const QPixmap& pixmap);
	QIcon iconOf(HANDLE h);   // icons and bitmaps (also a plain QIcon* of NppPluginQt.h)
	QPixmap pixmapOf(HANDLE h, QSize size = QSize());
	QFont fontOf(HFONT h);
	HFONT fontHandle(const QFont& f);
	QColor color(COLORREF c);
	COLORREF colorRef(const QColor& c);
	QColor sysColor(int index);
	HDC paintDC(Window* w, QPainter* p, const QRect& rect); // DC of WM_PAINT
	void releasePaintDC(HDC dc);
	HDC widgetDC(QWidget* w, QPainter* p); // DC that paints on p (WM_DRAWITEM, custom draw)
	void releaseDC(HDC dc);

	// ---- keys ------------------------------------------------------------------------------
	int vkFromQt(int qtKey, quint32 nativeVk = 0);
	int qtKeyFromVk(int vk);
	LPARAM keyLParam(int repeat, bool up, bool extended);

	// ---- the plugin's main window handle (for the owners of plugin windows) ------------
	HWND nppHandle();
}
