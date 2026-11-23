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

// Notepad++ Linux plugin SDK: the functions of the Win32 API that plugins use,
// implemented with Qt and POSIX in the npp_win32 library (plugin-sdk/win32).
//
// Windows of plugins are Qt widgets: dialogs are built from the DIALOGEX templates
// of the .rc file of the plugin (npp_add_plugin(... RC file.rc)), controls are the
// Qt widget of the same kind, and the dialog procedure of the plugin gets the
// messages of Windows (WM_INITDIALOG, WM_COMMAND with BN_CLICKED, EN_CHANGE...,
// WM_NOTIFY of list and tree views, WM_SIZE, WM_TIMER...). Paths may use '\\': the
// file functions take it as '/'.

#pragma once

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <string>
#include <type_traits>

// min and max of <windows.h> (macros there; functions here, not to break the
// standard library and Qt). NOMINMAX takes them out, as on Windows.
#ifndef NOMINMAX
template <typename A, typename B> constexpr auto min(A a, B b) -> std::common_type_t<A, B> { return b < a ? b : a; }
template <typename A, typename B> constexpr auto max(A a, B b) -> std::common_type_t<A, B> { return a < b ? b : a; }
#endif

// ---- helpers -------------------------------------------------------------------------
#define LOWORD(l) ((WORD)(((DWORD_PTR)(l)) & 0xffff))
#define HIWORD(l) ((WORD)((((DWORD_PTR)(l)) >> 16) & 0xffff))
#define LOBYTE(w) ((BYTE)(((DWORD_PTR)(w)) & 0xff))
#define HIBYTE(w) ((BYTE)((((DWORD_PTR)(w)) >> 8) & 0xff))
#define MAKEWORD(a, b) ((WORD)(((BYTE)(((DWORD_PTR)(a)) & 0xff)) | ((WORD)((BYTE)(((DWORD_PTR)(b)) & 0xff))) << 8))
#define MAKELONG(a, b) ((LONG)(((WORD)(((DWORD_PTR)(a)) & 0xffff)) | ((DWORD)((WORD)(((DWORD_PTR)(b)) & 0xffff))) << 16))
#define MAKEWPARAM(l, h) ((WPARAM)(DWORD)MAKELONG(l, h))
#define MAKELPARAM(l, h) ((LPARAM)(DWORD)MAKELONG(l, h))
#define MAKELRESULT(l, h) ((LRESULT)(DWORD)MAKELONG(l, h))
#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
#define GET_WHEEL_DELTA_WPARAM(wParam) ((short)HIWORD(wParam))
#define GET_KEYSTATE_WPARAM(wParam) (LOWORD(wParam))
#define RGB(r, g, b) ((COLORREF)(((BYTE)(r) | ((WORD)((BYTE)(g)) << 8)) | (((DWORD)(BYTE)(b)) << 16)))
#define GetRValue(rgb) (LOBYTE(rgb))
#define GetGValue(rgb) (LOBYTE(((WORD)(rgb)) >> 8))
#define GetBValue(rgb) (LOBYTE((rgb) >> 16))
#define MAKEINTRESOURCEW(i) ((LPWSTR)((ULONG_PTR)((WORD)(i))))
#define MAKEINTRESOURCEA(i) ((LPSTR)((ULONG_PTR)((WORD)(i))))
#define MAKEINTRESOURCE MAKEINTRESOURCEW
#define IS_INTRESOURCE(r) ((((ULONG_PTR)(r)) >> 16) == 0)
#define MAKEINTATOM(i) ((LPWSTR)((ULONG_PTR)((WORD)(i))))
#define NPP_WIDE_LITERAL(s) L##s
#define TEXT(s) NPP_WIDE_LITERAL(s)
#define __TEXT(s) NPP_WIDE_LITERAL(s)
#ifndef _T
#define _T(s) NPP_WIDE_LITERAL(s)
#endif
#define ZeroMemory(d, n) std::memset((d), 0, (n))
#define SecureZeroMemory(d, n) std::memset((d), 0, (n))
#define FillMemory(d, n, f) std::memset((d), (f), (n))
#define CopyMemory(d, s, n) std::memcpy((d), (s), (n))
#define MoveMemory(d, s, n) std::memmove((d), (s), (n))
#define _countof(a) (sizeof(a) / sizeof((a)[0]))
#define ARRAYSIZE(a) (sizeof(a) / sizeof((a)[0]))
#define INDEXTOSTATEIMAGEMASK(i) ((i) << 12)
#define HWND_TOP ((HWND)0)
#define HWND_BOTTOM ((HWND)(intptr_t)1)
#define HWND_TOPMOST ((HWND)(intptr_t)-1)
#define HWND_NOTOPMOST ((HWND)(intptr_t)-2)
#define HWND_DESKTOP ((HWND)0)
#define HWND_MESSAGE ((HWND)(intptr_t)-3)
#define HWND_BROADCAST ((HWND)(intptr_t)0xffff)
#define INVALID_HANDLE_VALUE ((HANDLE)(LONG_PTR)-1)
#define IDC_ARROW MAKEINTRESOURCE(32512)
#define IDC_IBEAM MAKEINTRESOURCE(32513)
#define IDC_WAIT MAKEINTRESOURCE(32514)
#define IDC_CROSS MAKEINTRESOURCE(32515)
#define IDC_UPARROW MAKEINTRESOURCE(32516)
#define IDC_SIZENWSE MAKEINTRESOURCE(32642)
#define IDC_SIZENESW MAKEINTRESOURCE(32643)
#define IDC_SIZEWE MAKEINTRESOURCE(32644)
#define IDC_SIZENS MAKEINTRESOURCE(32645)
#define IDC_SIZEALL MAKEINTRESOURCE(32646)
#define IDC_NO MAKEINTRESOURCE(32648)
#define IDC_HAND MAKEINTRESOURCE(32649)
#define IDC_APPSTARTING MAKEINTRESOURCE(32650)
#define IDC_HELP MAKEINTRESOURCE(32651)
#define IDI_APPLICATION MAKEINTRESOURCE(32512)
#define IDI_HAND MAKEINTRESOURCE(32513)
#define IDI_ERROR IDI_HAND
#define IDI_QUESTION MAKEINTRESOURCE(32514)
#define IDI_EXCLAMATION MAKEINTRESOURCE(32515)
#define IDI_WARNING IDI_EXCLAMATION
#define IDI_ASTERISK MAKEINTRESOURCE(32516)
#define IDI_INFORMATION IDI_ASTERISK
#define RT_CURSOR MAKEINTRESOURCE(1)
#define RT_BITMAP MAKEINTRESOURCE(2)
#define RT_ICON MAKEINTRESOURCE(3)
#define RT_MENU MAKEINTRESOURCE(4)
#define RT_DIALOG MAKEINTRESOURCE(5)
#define RT_STRING MAKEINTRESOURCE(6)
#define RT_RCDATA MAKEINTRESOURCE(10)
#define RT_GROUP_ICON MAKEINTRESOURCE(14)
#define RT_VERSION MAKEINTRESOURCE(16)
#define HKEY_CLASSES_ROOT ((HKEY)(ULONG_PTR)0x80000000)
#define HKEY_CURRENT_USER ((HKEY)(ULONG_PTR)0x80000001)
#define HKEY_LOCAL_MACHINE ((HKEY)(ULONG_PTR)0x80000002)
#define HKEY_USERS ((HKEY)(ULONG_PTR)0x80000003)

// ---- messages to windows ---------------------------------------------------------------
// like Windows, a message sent from another thread is handled on the thread of the windows (the
// GUI thread), and the sender waits for the result
bool npp_isGuiThread();
LRESULT npp_sendFromThread(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
inline LRESULT SendMessageW(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (!hWnd || hWnd->magic != NPP_HANDLE_MAGIC || !hWnd->send)
		return 0;
	if (!npp_isGuiThread())
		return npp_sendFromThread(hWnd, msg, wParam, lParam);
	return hWnd->send(hWnd, msg, wParam, lParam);
}
LRESULT SendMessageA(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
BOOL PostMessageW(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
inline BOOL PostMessageA(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) { return PostMessageW(hWnd, msg, wParam, lParam); }
LRESULT SendNotifyMessageW(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT SendMessageTimeoutW(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT flags, UINT timeout, PDWORD_PTR result);
#define SendMessage SendMessageW
#define PostMessage PostMessageW
#define SendNotifyMessage SendNotifyMessageW
#define SendMessageTimeout SendMessageTimeoutW
#define SMTO_NORMAL 0x0000
#define SMTO_ABORTIFHUNG 0x0002
LRESULT SendDlgItemMessageW(HWND hDlg, int id, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT SendDlgItemMessageA(HWND hDlg, int id, UINT msg, WPARAM wParam, LPARAM lParam);
#define SendDlgItemMessage SendDlgItemMessageW
BOOL PostThreadMessageW(DWORD threadId, UINT msg, WPARAM wParam, LPARAM lParam);
#define PostThreadMessage PostThreadMessageW
void PostQuitMessage(int exitCode);
BOOL GetMessageW(LPMSG msg, HWND hWnd, UINT min, UINT max);
BOOL PeekMessageW(LPMSG msg, HWND hWnd, UINT min, UINT max, UINT remove);
BOOL TranslateMessage(const MSG* msg);
LRESULT DispatchMessageW(const MSG* msg);
BOOL IsDialogMessageW(HWND hDlg, LPMSG msg);
int TranslateAcceleratorW(HWND hWnd, HACCEL hAcc, LPMSG msg);
#define GetMessage GetMessageW
#define PeekMessage PeekMessageW
#define DispatchMessage DispatchMessageW
#define IsDialogMessage IsDialogMessageW
#define TranslateAccelerator TranslateAcceleratorW
DWORD GetMessagePos();
LONG GetMessageTime();
BOOL WaitMessage();
UINT RegisterWindowMessageW(LPCWSTR name);
#define RegisterWindowMessage RegisterWindowMessageW

// ---- window classes and windows ----------------------------------------------------
ATOM RegisterClassExW(const WNDCLASSEXW* wc);
ATOM RegisterClassW(const WNDCLASSW* wc);
BOOL UnregisterClassW(LPCWSTR className, HINSTANCE hInst);
BOOL GetClassInfoExW(HINSTANCE hInst, LPCWSTR className, WNDCLASSEXW* wc);
#define RegisterClassEx RegisterClassExW
#define RegisterClass RegisterClassW
#define UnregisterClass UnregisterClassW
#define GetClassInfoEx GetClassInfoExW
HWND CreateWindowExW(DWORD exStyle, LPCWSTR className, LPCWSTR windowName, DWORD style, int x, int y, int width, int height,
	HWND parent, HMENU menu, HINSTANCE hInst, LPVOID param);
HWND CreateWindowExA(DWORD exStyle, LPCSTR className, LPCSTR windowName, DWORD style, int x, int y, int width, int height,
	HWND parent, HMENU menu, HINSTANCE hInst, LPVOID param);
#define CreateWindowEx CreateWindowExW
#define CreateWindowW(c, n, s, x, y, w, h, p, m, i, a) CreateWindowExW(0, c, n, s, x, y, w, h, p, m, i, a)
#define CreateWindow CreateWindowW
BOOL DestroyWindow(HWND hWnd);
BOOL IsWindow(HWND hWnd);
BOOL IsWindowVisible(HWND hWnd);
BOOL IsWindowEnabled(HWND hWnd);
BOOL IsIconic(HWND hWnd);
BOOL IsZoomed(HWND hWnd);
BOOL IsChild(HWND parent, HWND hWnd);
inline BOOL IsWindowUnicode(HWND) { return TRUE; }
BOOL ShowWindow(HWND hWnd, int cmd);
BOOL ShowWindowAsync(HWND hWnd, int cmd);
BOOL EnableWindow(HWND hWnd, BOOL enable);
BOOL UpdateWindow(HWND hWnd);
BOOL InvalidateRect(HWND hWnd, const RECT* rect, BOOL erase);
BOOL ValidateRect(HWND hWnd, const RECT* rect);
BOOL InvalidateRgn(HWND hWnd, HRGN rgn, BOOL erase);
BOOL RedrawWindow(HWND hWnd, const RECT* rect, HRGN rgn, UINT flags);
BOOL MoveWindow(HWND hWnd, int x, int y, int width, int height, BOOL repaint);
BOOL SetWindowPos(HWND hWnd, HWND insertAfter, int x, int y, int cx, int cy, UINT flags);
HDWP BeginDeferWindowPos(int n);
HDWP DeferWindowPos(HDWP h, HWND hWnd, HWND insertAfter, int x, int y, int cx, int cy, UINT flags);
BOOL EndDeferWindowPos(HDWP h);
BOOL GetWindowRect(HWND hWnd, LPRECT rect);
BOOL GetClientRect(HWND hWnd, LPRECT rect);
BOOL ClientToScreen(HWND hWnd, LPPOINT pt);
BOOL ScreenToClient(HWND hWnd, LPPOINT pt);
int MapWindowPoints(HWND from, HWND to, LPPOINT pts, UINT count);
BOOL GetWindowPlacement(HWND hWnd, WINDOWPLACEMENT* wp);
BOOL SetWindowPlacement(HWND hWnd, const WINDOWPLACEMENT* wp);
HWND GetParent(HWND hWnd);
HWND SetParent(HWND child, HWND parent);
HWND GetAncestor(HWND hWnd, UINT flags);
HWND GetWindow(HWND hWnd, UINT cmd);
#define GetNextWindow(hWnd, cmd) GetWindow((hWnd), (cmd))
UINT GetWindowModuleFileNameW(HWND hWnd, LPWSTR path, UINT max);
#define GetWindowModuleFileName GetWindowModuleFileNameW
HWND GetTopWindow(HWND hWnd);
HWND GetDesktopWindow();
HWND GetForegroundWindow();
BOOL SetForegroundWindow(HWND hWnd);
HWND GetActiveWindow();
HWND SetActiveWindow(HWND hWnd);
HWND GetFocus();
HWND SetFocus(HWND hWnd);
HWND GetCapture();
HWND SetCapture(HWND hWnd);
BOOL ReleaseCapture();
BOOL BringWindowToTop(HWND hWnd);
HWND WindowFromPoint(POINT pt);
HWND ChildWindowFromPoint(HWND parent, POINT pt);
HWND FindWindowW(LPCWSTR className, LPCWSTR windowName);
HWND FindWindowExW(HWND parent, HWND after, LPCWSTR className, LPCWSTR windowName);
#define FindWindow FindWindowW
#define FindWindowEx FindWindowExW
BOOL EnumChildWindows(HWND parent, WNDENUMPROC proc, LPARAM lParam);
BOOL EnumWindows(WNDENUMPROC proc, LPARAM lParam);
BOOL EnumThreadWindows(DWORD threadId, WNDENUMPROC proc, LPARAM lParam);
DWORD GetWindowThreadProcessId(HWND hWnd, LPDWORD pid);
BOOL SetWindowTextW(HWND hWnd, LPCWSTR text);
BOOL SetWindowTextA(HWND hWnd, LPCSTR text);
int GetWindowTextW(HWND hWnd, LPWSTR text, int max);
int GetWindowTextA(HWND hWnd, LPSTR text, int max);
int GetWindowTextLengthW(HWND hWnd);
inline int GetWindowTextLengthA(HWND hWnd) { return GetWindowTextLengthW(hWnd); }
#define SetWindowText SetWindowTextW
#define GetWindowText GetWindowTextW
#define GetWindowTextLength GetWindowTextLengthW
int GetClassNameW(HWND hWnd, LPWSTR name, int max);
#define GetClassName GetClassNameW
LONG_PTR GetWindowLongPtrW(HWND hWnd, int index);
LONG_PTR SetWindowLongPtrW(HWND hWnd, int index, LONG_PTR value);
inline LONG GetWindowLongW(HWND hWnd, int index) { return static_cast<LONG>(GetWindowLongPtrW(hWnd, index)); }
inline LONG SetWindowLongW(HWND hWnd, int index, LONG value) { return static_cast<LONG>(SetWindowLongPtrW(hWnd, index, value)); }
#define GetWindowLongPtr GetWindowLongPtrW
#define SetWindowLongPtr SetWindowLongPtrW
#define GetWindowLongPtrA GetWindowLongPtrW
#define SetWindowLongPtrA SetWindowLongPtrW
#define GetWindowLong GetWindowLongW
#define SetWindowLong SetWindowLongW
#define GetWindowLongA GetWindowLongW
#define SetWindowLongA SetWindowLongW
ULONG_PTR GetClassLongPtrW(HWND hWnd, int index);
ULONG_PTR SetClassLongPtrW(HWND hWnd, int index, LONG_PTR value);
#define GetClassLongPtr GetClassLongPtrW
#define SetClassLongPtr SetClassLongPtrW
BOOL SetPropW(HWND hWnd, LPCWSTR name, HANDLE data);
HANDLE GetPropW(HWND hWnd, LPCWSTR name);
HANDLE RemovePropW(HWND hWnd, LPCWSTR name);
#define SetProp SetPropW
#define GetProp GetPropW
#define RemoveProp RemovePropW
LRESULT CallWindowProcW(WNDPROC proc, HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT DefWindowProcW(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT DefDlgProcW(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);
#define CallWindowProc CallWindowProcW
#define CallWindowProcA CallWindowProcW
#define DefWindowProc DefWindowProcW
#define DefWindowProcA DefWindowProcW
#define DefDlgProc DefDlgProcW
BOOL FlashWindow(HWND hWnd, BOOL invert);
typedef struct {
	UINT cbSize;
	HWND hwnd;
	DWORD dwFlags;
	UINT uCount;
	DWORD dwTimeout;
} FLASHWINFO, *PFLASHWINFO;
#define FLASHW_STOP 0
#define FLASHW_CAPTION 0x00000001
#define FLASHW_TRAY 0x00000002
#define FLASHW_ALL (FLASHW_CAPTION | FLASHW_TRAY)
#define FLASHW_TIMER 0x00000004
#define FLASHW_TIMERNOFG 0x0000000C
BOOL FlashWindowEx(PFLASHWINFO info); // QApplication::alert (FLASHW_STOP: nothing)
// SendInput: the mouse buttons (MOUSEEVENTF_*DOWN/UP) at the mouse position, to the widget that has the mouse
// (grabbed) or the one under it; other input is not sent
typedef struct {
	LONG dx;
	LONG dy;
	DWORD mouseData;
	DWORD dwFlags;
	DWORD time;
	ULONG_PTR dwExtraInfo;
} MOUSEINPUT;
typedef struct {
	WORD wVk;
	WORD wScan;
	DWORD dwFlags;
	DWORD time;
	ULONG_PTR dwExtraInfo;
} KEYBDINPUT;
typedef struct {
	DWORD uMsg;
	WORD wParamL;
	WORD wParamH;
} HARDWAREINPUT;
typedef struct {
	DWORD type;
	union {
		MOUSEINPUT mi;
		KEYBDINPUT ki;
		HARDWAREINPUT hi;
	};
} INPUT, *LPINPUT;
#define INPUT_MOUSE 0
#define INPUT_KEYBOARD 1
#define INPUT_HARDWARE 2
#define MOUSEEVENTF_MOVE 0x0001
#define MOUSEEVENTF_LEFTDOWN 0x0002
#define MOUSEEVENTF_LEFTUP 0x0004
#define MOUSEEVENTF_RIGHTDOWN 0x0008
#define MOUSEEVENTF_RIGHTUP 0x0010
#define MOUSEEVENTF_MIDDLEDOWN 0x0020
#define MOUSEEVENTF_MIDDLEUP 0x0040
#define MOUSEEVENTF_WHEEL 0x0800
#define MOUSEEVENTF_ABSOLUTE 0x8000
#define KEYEVENTF_EXTENDEDKEY 0x0001
#define KEYEVENTF_KEYUP 0x0002
#define KEYEVENTF_UNICODE 0x0004
#define KEYEVENTF_SCANCODE 0x0008
UINT SendInput(UINT count, LPINPUT inputs, int size); // the number of inputs sent
UINT GetDpiForWindow(HWND hWnd);
UINT GetDpiForSystem();
// DPI awareness: Qt scales the windows, the context is only remembered
#define USER_DEFAULT_SCREEN_DPI 96
typedef struct DPI_AWARENESS_CONTEXT__* DPI_AWARENESS_CONTEXT;
#define DPI_AWARENESS_CONTEXT_UNAWARE ((DPI_AWARENESS_CONTEXT)-1)
#define DPI_AWARENESS_CONTEXT_SYSTEM_AWARE ((DPI_AWARENESS_CONTEXT)-2)
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE ((DPI_AWARENESS_CONTEXT)-3)
#define DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2 ((DPI_AWARENESS_CONTEXT)-4)
#define DPI_AWARENESS_CONTEXT_UNAWARE_GDISCALED ((DPI_AWARENESS_CONTEXT)-5)
inline DPI_AWARENESS_CONTEXT& npp_threadDpiContext()
{
	static thread_local DPI_AWARENESS_CONTEXT context = DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2;
	return context;
}
inline DPI_AWARENESS_CONTEXT SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT context)
{
	const DPI_AWARENESS_CONTEXT old = npp_threadDpiContext();
	npp_threadDpiContext() = context;
	return old;
}
inline DPI_AWARENESS_CONTEXT GetThreadDpiAwarenessContext() { return npp_threadDpiContext(); }
inline BOOL AreDpiAwarenessContextsEqual(DPI_AWARENESS_CONTEXT a, DPI_AWARENESS_CONTEXT b) { return a == b; }
int GetSystemMetrics(int index);
int MulDiv(int number, int numerator, int denominator);
inline int GetSystemMetricsForDpi(int index, UINT dpi) { return MulDiv(GetSystemMetrics(index), static_cast<int>(dpi), USER_DEFAULT_SCREEN_DPI); }
BOOL AdjustWindowRectEx(LPRECT rect, DWORD style, BOOL menu, DWORD exStyle);
BOOL AnimateWindow(HWND hWnd, DWORD time, DWORD flags);
BOOL SetLayeredWindowAttributes(HWND hWnd, COLORREF key, BYTE alpha, DWORD flags);
#define LWA_COLORKEY 0x00000001
#define LWA_ALPHA 0x00000002
BOOL LockWindowUpdate(HWND hWnd);
BOOL DrawMenuBar(HWND hWnd);
BOOL ShowScrollBar(HWND hWnd, int bar, BOOL show);
int SetScrollInfo(HWND hWnd, int bar, LPCSCROLLINFO si, BOOL redraw);
BOOL GetScrollInfo(HWND hWnd, int bar, LPSCROLLINFO si);
BOOL GetScrollBarInfo(HWND hWnd, LONG idObject, PSCROLLBARINFO info);
int SetScrollPos(HWND hWnd, int bar, int pos, BOOL redraw);
int GetScrollPos(HWND hWnd, int bar);
BOOL SetScrollRange(HWND hWnd, int bar, int min, int max, BOOL redraw);
BOOL TrackMouseEvent(LPTRACKMOUSEEVENT tme);
BOOL DragDetect(HWND hWnd, POINT pt);
void DragAcceptFiles(HWND hWnd, BOOL accept);
UINT DragQueryFileW(HDROP drop, UINT index, LPWSTR file, UINT max);
void DragFinish(HDROP drop);
BOOL DragQueryPoint(HDROP drop, POINT* pt);
#define DragQueryFile DragQueryFileW

// ---- dialogs -------------------------------------------------------------------------
HWND CreateDialogParamW(HINSTANCE hInst, LPCWSTR templateName, HWND parent, DLGPROC proc, LPARAM init);
HWND CreateDialogIndirectParamW(HINSTANCE hInst, LPCDLGTEMPLATE tmpl, HWND parent, DLGPROC proc, LPARAM init);
INT_PTR DialogBoxParamW(HINSTANCE hInst, LPCWSTR templateName, HWND parent, DLGPROC proc, LPARAM init);
INT_PTR DialogBoxIndirectParamW(HINSTANCE hInst, LPCDLGTEMPLATE tmpl, HWND parent, DLGPROC proc, LPARAM init);
#define CreateDialogParam CreateDialogParamW
#define CreateDialogIndirectParam CreateDialogIndirectParamW
#define DialogBoxParam DialogBoxParamW
#define DialogBoxIndirectParam DialogBoxIndirectParamW
#define CreateDialog(i, t, p, f) CreateDialogParamW(i, t, p, f, 0)
#define CreateDialogW CreateDialog
#define DialogBox(i, t, p, f) DialogBoxParamW(i, t, p, f, 0)
#define DialogBoxW DialogBox
BOOL EndDialog(HWND hDlg, INT_PTR result);
HWND GetDlgItem(HWND hDlg, int id);
int GetDlgCtrlID(HWND hWnd);
BOOL SetDlgItemTextW(HWND hDlg, int id, LPCWSTR text);
BOOL SetDlgItemTextA(HWND hDlg, int id, LPCSTR text);
UINT GetDlgItemTextW(HWND hDlg, int id, LPWSTR text, int max);
UINT GetDlgItemTextA(HWND hDlg, int id, LPSTR text, int max);
#define SetDlgItemText SetDlgItemTextW
#define GetDlgItemText GetDlgItemTextW
BOOL SetDlgItemInt(HWND hDlg, int id, UINT value, BOOL isSigned);
UINT GetDlgItemInt(HWND hDlg, int id, BOOL* translated, BOOL isSigned);
BOOL CheckDlgButton(HWND hDlg, int id, UINT check);
UINT IsDlgButtonChecked(HWND hDlg, int id);
BOOL CheckRadioButton(HWND hDlg, int first, int last, int check);
HWND GetNextDlgTabItem(HWND hDlg, HWND ctl, BOOL previous);
HWND GetNextDlgGroupItem(HWND hDlg, HWND ctl, BOOL previous);
BOOL MapDialogRect(HWND hDlg, LPRECT rect);
LONG GetDialogBaseUnits();

int MessageBoxW(HWND parent, LPCWSTR text, LPCWSTR caption, UINT type);
int MessageBoxA(HWND parent, LPCSTR text, LPCSTR caption, UINT type);
inline int MessageBoxExW(HWND parent, LPCWSTR text, LPCWSTR caption, UINT type, WORD) { return MessageBoxW(parent, text, caption, type); }
#define MessageBox MessageBoxW
#define MessageBoxEx MessageBoxExW
BOOL MessageBeep(UINT type);

// ---- menus ---------------------------------------------------------------------------
HMENU CreateMenu();
HMENU CreatePopupMenu();
BOOL DestroyMenu(HMENU hMenu);
HMENU GetMenu(HWND hWnd);
struct MENUBARINFO
{
	DWORD cbSize;
	RECT rcBar;
	HMENU hMenu;
	HWND hwndMenu;
	BOOL fBarFocused : 1;
	BOOL fFocused : 1;
	BOOL fUnused : 30;
};
typedef MENUBARINFO* PMENUBARINFO;
typedef MENUBARINFO* LPMENUBARINFO;
#define OBJID_WINDOW ((LONG)0x00000000)
#define OBJID_SYSMENU ((LONG)0xFFFFFFFF)
#define OBJID_MENU ((LONG)0xFFFFFFFD)
// the menu bar of the window (idItem 0) or one of its menus (1...): screen rectangle and menu
BOOL GetMenuBarInfo(HWND hWnd, LONG idObject, LONG idItem, PMENUBARINFO info);
BOOL SetMenu(HWND hWnd, HMENU hMenu);
HMENU GetSubMenu(HMENU hMenu, int pos);
HMENU GetSystemMenu(HWND hWnd, BOOL revert);
int GetMenuItemCount(HMENU hMenu);
UINT GetMenuItemID(HMENU hMenu, int pos);
UINT GetMenuState(HMENU hMenu, UINT id, UINT flags);
int GetMenuStringW(HMENU hMenu, UINT id, LPWSTR text, int max, UINT flags);
#define GetMenuString GetMenuStringW
BOOL AppendMenuW(HMENU hMenu, UINT flags, UINT_PTR id, LPCWSTR text);
BOOL InsertMenuW(HMENU hMenu, UINT pos, UINT flags, UINT_PTR id, LPCWSTR text);
BOOL ModifyMenuW(HMENU hMenu, UINT pos, UINT flags, UINT_PTR id, LPCWSTR text);
BOOL InsertMenuItemW(HMENU hMenu, UINT item, BOOL byPosition, LPCMENUITEMINFOW mii);
BOOL SetMenuItemInfoW(HMENU hMenu, UINT item, BOOL byPosition, LPCMENUITEMINFOW mii);
BOOL GetMenuItemInfoW(HMENU hMenu, UINT item, BOOL byPosition, LPMENUITEMINFOW mii);
#define AppendMenu AppendMenuW
#define InsertMenu InsertMenuW
#define ModifyMenu ModifyMenuW
#define InsertMenuItem InsertMenuItemW
#define SetMenuItemInfo SetMenuItemInfoW
#define GetMenuItemInfo GetMenuItemInfoW
BOOL RemoveMenu(HMENU hMenu, UINT pos, UINT flags);
BOOL DeleteMenu(HMENU hMenu, UINT pos, UINT flags);
DWORD CheckMenuItem(HMENU hMenu, UINT id, UINT flags);
BOOL EnableMenuItem(HMENU hMenu, UINT id, UINT flags);
BOOL CheckMenuRadioItem(HMENU hMenu, UINT first, UINT last, UINT check, UINT flags);
BOOL SetMenuDefaultItem(HMENU hMenu, UINT item, UINT byPos);
BOOL TrackPopupMenu(HMENU hMenu, UINT flags, int x, int y, int reserved, HWND hWnd, const RECT* rect);
BOOL TrackPopupMenuEx(HMENU hMenu, UINT flags, int x, int y, HWND hWnd, void* params);
struct TPMPARAMS
{
	UINT cbSize;
	RECT rcExclude;
};
typedef TPMPARAMS tagTPMPARAMS;
typedef TPMPARAMS* LPTPMPARAMS;
HMENU LoadMenuW(HINSTANCE hInst, LPCWSTR name);
#define LoadMenu LoadMenuW
HACCEL CreateAcceleratorTableW(LPACCEL accels, int count);
BOOL DestroyAcceleratorTable(HACCEL hAccel);
HACCEL LoadAcceleratorsW(HINSTANCE hInst, LPCWSTR name);
#define CreateAcceleratorTable CreateAcceleratorTableW
#define LoadAccelerators LoadAcceleratorsW

// ---- timers, keyboard, mouse, cursor -----------------------------------------------
UINT_PTR SetTimer(HWND hWnd, UINT_PTR id, UINT elapse, TIMERPROC proc);
BOOL KillTimer(HWND hWnd, UINT_PTR id);
SHORT GetKeyState(int vKey);
SHORT GetAsyncKeyState(int vKey);
BOOL GetKeyboardState(PBYTE state);
UINT MapVirtualKeyW(UINT code, UINT mapType);
#define MapVirtualKey MapVirtualKeyW
SHORT VkKeyScanW(WCHAR ch);
#define VkKeyScan VkKeyScanW
typedef void* HKL;
HKL GetKeyboardLayout(DWORD threadId);
SHORT VkKeyScanExW(WCHAR ch, HKL layout);
#define VkKeyScanEx VkKeyScanExW
int GetKeyNameTextW(LONG lParam, LPWSTR text, int max);
#define GetKeyNameText GetKeyNameTextW
UINT GetDoubleClickTime();
BOOL GetCursorPos(LPPOINT pt);
BOOL SetCursorPos(int x, int y);
HCURSOR LoadCursorW(HINSTANCE hInst, LPCWSTR name);
HCURSOR SetCursor(HCURSOR cursor);
HCURSOR GetCursor();
int ShowCursor(BOOL show);
BOOL DestroyCursor(HCURSOR cursor);
#define LoadCursor LoadCursorW
BOOL RegisterHotKey(HWND hWnd, int id, UINT modifiers, UINT vk);
BOOL UnregisterHotKey(HWND hWnd, int id);
HHOOK SetWindowsHookExW(int idHook, HOOKPROC proc, HINSTANCE hMod, DWORD threadId);
BOOL UnhookWindowsHookEx(HHOOK hHook);
LRESULT CallNextHookEx(HHOOK hHook, int code, WPARAM wParam, LPARAM lParam);
#define SetWindowsHookEx SetWindowsHookExW
void keybd_event(BYTE vk, BYTE scan, DWORD flags, ULONG_PTR extra);
#define KEYEVENTF_KEYUP 0x0002

// ---- system ----------------------------------------------------------------------------
int GetSystemMetrics(int index);
DWORD GetSysColor(int index);
HBRUSH GetSysColorBrush(int index);
BOOL SystemParametersInfoW(UINT action, UINT param, PVOID pvParam, UINT winIni);
#define SystemParametersInfo SystemParametersInfoW
HMONITOR MonitorFromWindow(HWND hWnd, DWORD flags);
HMONITOR MonitorFromRect(LPCRECT rect, DWORD flags);
HMONITOR MonitorFromPoint(POINT pt, DWORD flags);
BOOL GetMonitorInfoW(HMONITOR hMonitor, LPMONITORINFO info);
typedef BOOL (CALLBACK* MONITORENUMPROC)(HMONITOR hMonitor, HDC hdc, LPRECT rect, LPARAM data);
BOOL EnumDisplayMonitors(HDC hdc, LPCRECT clip, MONITORENUMPROC proc, LPARAM data);
#define GetMonitorInfo GetMonitorInfoW

// ---- rectangles --------------------------------------------------------------------------
BOOL SetRect(LPRECT rc, int left, int top, int right, int bottom);
BOOL SetRectEmpty(LPRECT rc);
BOOL CopyRect(LPRECT dst, const RECT* src);
BOOL InflateRect(LPRECT rc, int dx, int dy);
BOOL OffsetRect(LPRECT rc, int dx, int dy);
BOOL IsRectEmpty(const RECT* rc);
BOOL PtInRect(const RECT* rc, POINT pt);
BOOL IntersectRect(LPRECT dst, const RECT* a, const RECT* b);
BOOL UnionRect(LPRECT dst, const RECT* a, const RECT* b);
BOOL EqualRect(const RECT* a, const RECT* b);
int MulDiv(int number, int numerator, int denominator);

// ---- clipboard ---------------------------------------------------------------------------
BOOL OpenClipboard(HWND owner);
BOOL CloseClipboard();
BOOL EmptyClipboard();
HANDLE SetClipboardData(UINT format, HANDLE data);
HANDLE GetClipboardData(UINT format);
BOOL IsClipboardFormatAvailable(UINT format);
UINT RegisterClipboardFormatW(LPCWSTR name);
#define RegisterClipboardFormat RegisterClipboardFormatW
UINT EnumClipboardFormats(UINT format);
int CountClipboardFormats();

// ---- strings (user32) --------------------------------------------------------------------
int LoadStringW(HINSTANCE hInst, UINT id, LPWSTR buffer, int max);
int LoadStringA(HINSTANCE hInst, UINT id, LPSTR buffer, int max);
#define LoadString LoadStringW
LPWSTR CharUpperW(LPWSTR s);
LPWSTR CharLowerW(LPWSTR s);
LPSTR CharUpperA(LPSTR s);
LPSTR CharLowerA(LPSTR s);
DWORD CharUpperBuffW(LPWSTR s, DWORD n);
DWORD CharLowerBuffW(LPWSTR s, DWORD n);
LPWSTR CharNextW(LPCWSTR s);
LPWSTR CharPrevW(LPCWSTR start, LPCWSTR s);
LPSTR CharNextA(LPCSTR s);
BOOL IsCharAlphaW(WCHAR c);
BOOL IsCharAlphaNumericW(WCHAR c);
BOOL IsCharUpperW(WCHAR c);
BOOL IsCharLowerW(WCHAR c);
#define CharUpper CharUpperW
#define CharLower CharLowerW
#define CharUpperBuff CharUpperBuffW
#define CharLowerBuff CharLowerBuffW
#define CharNext CharNextW
#define CharPrev CharPrevW
#define IsCharAlpha IsCharAlphaW
#define IsCharAlphaNumeric IsCharAlphaNumericW
#define IsCharUpper IsCharUpperW
#define IsCharLower IsCharLowerW
int wsprintfW(LPWSTR buffer, LPCWSTR format, ...);
int wsprintfA(LPSTR buffer, LPCSTR format, ...);
int wvsprintfW(LPWSTR buffer, LPCWSTR format, va_list args);
inline int wvsprintfA(LPSTR buffer, LPCSTR format, va_list args) { return vsprintf(buffer, format, args); }
#define wsprintf wsprintfW
#define wvsprintf wvsprintfW

// ---- GDI -------------------------------------------------------------------------------
HDC GetDC(HWND hWnd);
HDC GetWindowDC(HWND hWnd);
HDC GetDCEx(HWND hWnd, HRGN clip, DWORD flags);
int ReleaseDC(HWND hWnd, HDC hdc);
HDC BeginPaint(HWND hWnd, LPPAINTSTRUCT ps);
BOOL EndPaint(HWND hWnd, const PAINTSTRUCT* ps);
HDC CreateCompatibleDC(HDC hdc);
BOOL DeleteDC(HDC hdc);
HBITMAP CreateCompatibleBitmap(HDC hdc, int cx, int cy);
HBITMAP CreateBitmap(int cx, int cy, UINT planes, UINT bitCount, const void* bits);
HBITMAP CreateDIBSection(HDC hdc, const BITMAPINFO* bmi, UINT usage, void** bits, HANDLE section, DWORD offset);
HGDIOBJ SelectObject(HDC hdc, HGDIOBJ obj);
BOOL DeleteObject(HGDIOBJ obj);
HGDIOBJ GetStockObject(int index);
int GetObjectW(HGDIOBJ obj, int size, LPVOID out);
#define GetObject GetObjectW
DWORD GetObjectType(HGDIOBJ obj);
HGDIOBJ GetCurrentObject(HDC hdc, UINT type);
#define OBJ_PEN 1
#define OBJ_BRUSH 2
#define OBJ_DC 3
#define OBJ_FONT 6
#define OBJ_BITMAP 7
HBRUSH CreateSolidBrush(COLORREF color);
HBRUSH CreateBrushIndirect(const LOGBRUSH* lb);
HBRUSH CreatePatternBrush(HBITMAP hbm);
HBRUSH CreateHatchBrush(int hatch, COLORREF color);

// ---- processes, consoles, jobs, security (Kernel.cpp) ------------------------------------
BOOL InitializeProcThreadAttributeList(LPPROC_THREAD_ATTRIBUTE_LIST list, DWORD count, DWORD flags, PSIZE_T size); // fails
BOOL UpdateProcThreadAttribute(LPPROC_THREAD_ATTRIBUTE_LIST list, DWORD flags, DWORD_PTR attribute, PVOID value, SIZE_T size, PVOID previous, PSIZE_T returnSize);
void DeleteProcThreadAttributeList(LPPROC_THREAD_ATTRIBUTE_LIST list);
HANDLE CreateJobObjectW(LPSECURITY_ATTRIBUTES sa, LPCWSTR name); // NULL: no job objects
#define CreateJobObject CreateJobObjectW
BOOL SetInformationJobObject(HANDLE job, JOBOBJECTINFOCLASS infoClass, LPVOID info, DWORD size);
BOOL AssignProcessToJobObject(HANDLE job, HANDLE process);
BOOL IsProcessInJob(HANDLE process, HANDLE job, PBOOL result);
BOOL TerminateJobObject(HANDLE job, UINT exitCode);
BOOL SetConsoleCtrlHandler(PHANDLER_ROUTINE handler, BOOL add);
BOOL GenerateConsoleCtrlEvent(DWORD ctrlEvent, DWORD processGroupId); // SIGINT to the process (group)
BOOL AllocConsole();
BOOL FreeConsole();
BOOL AttachConsole(DWORD processId);
inline BOOL InitializeSecurityDescriptor(PSECURITY_DESCRIPTOR, DWORD) { return TRUE; }
inline BOOL SetSecurityDescriptorDacl(PSECURITY_DESCRIPTOR, BOOL, PACL, BOOL) { return TRUE; }
BOOL GetGUIThreadInfo(DWORD threadId, PGUITHREADINFO info);
BOOL GetCaretPos(LPPOINT point);
BOOL IsMenu(HMENU menu);
DWORD SearchPathW(LPCWSTR path, LPCWSTR fileName, LPCWSTR extension, DWORD bufferLength, LPWSTR buffer, LPWSTR* filePart);
#define SearchPath SearchPathW
BOOL SetFileTime(HANDLE file, const FILETIME* created, const FILETIME* accessed, const FILETIME* written);
HPEN CreatePen(int style, int width, COLORREF color);
HFONT CreateFontIndirectW(const LOGFONTW* lf);
HFONT CreateFontW(int height, int width, int escapement, int orientation, int weight, DWORD italic, DWORD underline,
	DWORD strikeOut, DWORD charSet, DWORD outPrecision, DWORD clipPrecision, DWORD quality, DWORD pitchAndFamily, LPCWSTR face);
#define CreateFontIndirect CreateFontIndirectW
#define CreateFont CreateFontW
HRGN CreateRectRgn(int left, int top, int right, int bottom);
HRGN CreateRectRgnIndirect(const RECT* rc);
HRGN CreateRoundRectRgn(int left, int top, int right, int bottom, int w, int h);
int SelectClipRgn(HDC hdc, HRGN rgn);
int IntersectClipRect(HDC hdc, int left, int top, int right, int bottom);
int ExcludeClipRect(HDC hdc, int left, int top, int right, int bottom);
int GetClipBox(HDC hdc, LPRECT rc);
int SetWindowRgn(HWND hWnd, HRGN rgn, BOOL redraw);
COLORREF SetTextColor(HDC hdc, COLORREF color);
COLORREF GetTextColor(HDC hdc);
COLORREF SetBkColor(HDC hdc, COLORREF color);
COLORREF GetBkColor(HDC hdc);
int SetBkMode(HDC hdc, int mode);
int GetBkMode(HDC hdc);
COLORREF SetDCBrushColor(HDC hdc, COLORREF color);
COLORREF SetDCPenColor(HDC hdc, COLORREF color);
UINT SetTextAlign(HDC hdc, UINT align);
int SetMapMode(HDC hdc, int mode);
int SaveDC(HDC hdc);
BOOL RestoreDC(HDC hdc, int saved);
int FillRect(HDC hdc, const RECT* rc, HBRUSH brush);
int FrameRect(HDC hdc, const RECT* rc, HBRUSH brush);
BOOL InvertRect(HDC hdc, const RECT* rc);
BOOL Rectangle(HDC hdc, int left, int top, int right, int bottom);
BOOL RoundRect(HDC hdc, int left, int top, int right, int bottom, int w, int h);
BOOL Ellipse(HDC hdc, int left, int top, int right, int bottom);
BOOL Polygon(HDC hdc, const POINT* pts, int count);
BOOL Polyline(HDC hdc, const POINT* pts, int count);
BOOL MoveToEx(HDC hdc, int x, int y, LPPOINT old);
BOOL LineTo(HDC hdc, int x, int y);
COLORREF SetPixel(HDC hdc, int x, int y, COLORREF color);
inline BOOL SetPixelV(HDC hdc, int x, int y, COLORREF color) { return SetPixel(hdc, x, y, color) != CLR_INVALID; }
COLORREF GetPixel(HDC hdc, int x, int y);
BOOL DrawFocusRect(HDC hdc, const RECT* rc);
BOOL DrawEdge(HDC hdc, LPRECT rc, UINT edge, UINT flags);
BOOL DrawFrameControl(HDC hdc, LPRECT rc, UINT type, UINT state);
BOOL DrawIcon(HDC hdc, int x, int y, HICON icon);
BOOL DrawIconEx(HDC hdc, int x, int y, HICON icon, int cx, int cy, UINT step, HBRUSH flicker, UINT flags);
#define DI_NORMAL 0x0003
BOOL GradientFill(HDC hdc, void* vertices, ULONG nVertex, void* mesh, ULONG nMesh, ULONG mode);
int DrawTextW(HDC hdc, LPCWSTR text, int len, LPRECT rc, UINT format);
int DrawTextA(HDC hdc, LPCSTR text, int len, LPRECT rc, UINT format);
int DrawTextExW(HDC hdc, LPWSTR text, int len, LPRECT rc, UINT format, void* params);
#define DrawText DrawTextW
#define DrawTextEx DrawTextExW
BOOL TextOutW(HDC hdc, int x, int y, LPCWSTR text, int len);
BOOL TextOutA(HDC hdc, int x, int y, LPCSTR text, int len);
BOOL ExtTextOutW(HDC hdc, int x, int y, UINT options, const RECT* rc, LPCWSTR text, UINT len, const int* dx);
#define TextOut TextOutW
#define ExtTextOut ExtTextOutW
BOOL GetTextExtentPoint32W(HDC hdc, LPCWSTR text, int len, LPSIZE size);
BOOL GetTextExtentPoint32A(HDC hdc, LPCSTR text, int len, LPSIZE size);
#define GetTextExtentPoint32 GetTextExtentPoint32W
#define GetTextExtentPoint GetTextExtentPoint32W
BOOL GetTextMetricsW(HDC hdc, LPTEXTMETRICW tm);
#define GetTextMetrics GetTextMetricsW
int GetTextFaceW(HDC hdc, int max, LPWSTR face);
#define GetTextFace GetTextFaceW
int GetDeviceCaps(HDC hdc, int index);
BOOL BitBlt(HDC dst, int x, int y, int cx, int cy, HDC src, int sx, int sy, DWORD rop);
BOOL StretchBlt(HDC dst, int x, int y, int cx, int cy, HDC src, int sx, int sy, int scx, int scy, DWORD rop);
BOOL PatBlt(HDC hdc, int x, int y, int cx, int cy, DWORD rop);
#define LF_FULLFACESIZE 64
#define RASTER_FONTTYPE 0x0001
#define DEVICE_FONTTYPE 0x0002
#define TRUETYPE_FONTTYPE 0x0004
struct ENUMLOGFONTEXW
{
	LOGFONTW elfLogFont;
	WCHAR elfFullName[LF_FULLFACESIZE];
	WCHAR elfStyle[LF_FACESIZE];
	WCHAR elfScript[LF_FACESIZE];
};
typedef ENUMLOGFONTEXW ENUMLOGFONTEX;
typedef ENUMLOGFONTEXW* LPENUMLOGFONTEXW;
typedef ENUMLOGFONTEXW* LPENUMLOGFONTEX;
struct FONTSIGNATURE
{
	DWORD fsUsb[4];
	DWORD fsCsb[2];
};
struct NEWTEXTMETRICW
{
	LONG tmHeight, tmAscent, tmDescent, tmInternalLeading, tmExternalLeading, tmAveCharWidth, tmMaxCharWidth, tmWeight,
		tmOverhang, tmDigitizedAspectX, tmDigitizedAspectY;
	WCHAR tmFirstChar, tmLastChar, tmDefaultChar, tmBreakChar;
	BYTE tmItalic, tmUnderlined, tmStruckOut, tmPitchAndFamily, tmCharSet;
	DWORD ntmFlags;
	UINT ntmSizeEM, ntmCellHeight, ntmAvgWidth;
};
struct NEWTEXTMETRICEXW
{
	NEWTEXTMETRICW ntmTm;
	FONTSIGNATURE ntmFontSig;
};
typedef NEWTEXTMETRICW NEWTEXTMETRIC;
typedef NEWTEXTMETRICEXW NEWTEXTMETRICEX;
typedef int (CALLBACK* FONTENUMPROCW)(const LOGFONTW* logFont, const TEXTMETRICW* metric, DWORD fontType, LPARAM data);
typedef FONTENUMPROCW FONTENUMPROC;
// the families of the font database (lfFaceName empty: all, else that one), once each
int EnumFontFamiliesExW(HDC hdc, LPLOGFONTW logFont, FONTENUMPROCW proc, LPARAM data, DWORD flags);
int EnumFontFamiliesW(HDC hdc, LPCWSTR family, FONTENUMPROCW proc, LPARAM data);
#define EnumFontFamiliesEx EnumFontFamiliesExW
#define EnumFontFamilies EnumFontFamiliesW
BOOL SetBrushOrgEx(HDC hdc, int x, int y, LPPOINT old);
BOOL GetBrushOrgEx(HDC hdc, LPPOINT pt);
BOOL TransparentBlt(HDC dst, int x, int y, int cx, int cy, HDC src, int sx, int sy, int scx, int scy, UINT transparent);
typedef struct {
	BYTE BlendOp;
	BYTE BlendFlags;
	BYTE SourceConstantAlpha;
	BYTE AlphaFormat;
} BLENDFUNCTION, *PBLENDFUNCTION;
#define AC_SRC_OVER 0x00
#define AC_SRC_ALPHA 0x01
// SourceConstantAlpha over the alpha of the bitmap (QImage), the formats of Qt with alpha are premultiplied
BOOL AlphaBlend(HDC dst, int x, int y, int cx, int cy, HDC src, int sx, int sy, int scx, int scy, BLENDFUNCTION blend);
#define GdiAlphaBlend AlphaBlend
int SetStretchBltMode(HDC hdc, int mode);
BOOL GdiFlush();

// ---- images, icons, resources ------------------------------------------------------------
HANDLE LoadImageW(HINSTANCE hInst, LPCWSTR name, UINT type, int cx, int cy, UINT flags);
HANDLE LoadImageA(HINSTANCE hInst, LPCSTR name, UINT type, int cx, int cy, UINT flags);
#define LoadImage LoadImageW
HICON LoadIconW(HINSTANCE hInst, LPCWSTR name);
#define LoadIcon LoadIconW
HBITMAP LoadBitmapW(HINSTANCE hInst, LPCWSTR name);
#define LoadBitmap LoadBitmapW
BOOL DestroyIcon(HICON icon);
HICON CopyIcon(HICON icon);
HANDLE CopyImage(HANDLE h, UINT type, int cx, int cy, UINT flags);
HICON CreateIconIndirect(PICONINFO info);
BOOL GetIconInfo(HICON icon, PICONINFO info);
HRESULT LoadIconMetric(HINSTANCE hInst, PCWSTR name, int metric, HICON* icon);
HRESULT LoadIconWithScaleDown(HINSTANCE hInst, PCWSTR name, int cx, int cy, HICON* icon);
HRSRC FindResourceW(HMODULE hModule, LPCWSTR name, LPCWSTR type);
HRSRC FindResourceExW(HMODULE hModule, LPCWSTR type, LPCWSTR name, WORD lang);
HGLOBAL LoadResource(HMODULE hModule, HRSRC res);
LPVOID LockResource(HGLOBAL res);
DWORD SizeofResource(HMODULE hModule, HRSRC res);
BOOL FreeResource(HGLOBAL res);
#define FindResource FindResourceW
#define FindResourceEx FindResourceExW

// ---- kernel: memory --------------------------------------------------------------------
HGLOBAL GlobalAlloc(UINT flags, SIZE_T bytes);
HGLOBAL GlobalReAlloc(HGLOBAL mem, SIZE_T bytes, UINT flags);
LPVOID GlobalLock(HGLOBAL mem);
BOOL GlobalUnlock(HGLOBAL mem);
HGLOBAL GlobalFree(HGLOBAL mem);
SIZE_T GlobalSize(HGLOBAL mem);
HLOCAL LocalAlloc(UINT flags, SIZE_T bytes);
HLOCAL LocalReAlloc(HLOCAL mem, SIZE_T bytes, UINT flags);
LPVOID LocalLock(HLOCAL mem);
BOOL LocalUnlock(HLOCAL mem);
HLOCAL LocalFree(HLOCAL mem);
HANDLE GetProcessHeap();
LPVOID HeapAlloc(HANDLE heap, DWORD flags, SIZE_T bytes);
LPVOID HeapReAlloc(HANDLE heap, DWORD flags, LPVOID mem, SIZE_T bytes);
BOOL HeapFree(HANDLE heap, DWORD flags, LPVOID mem);
LPVOID VirtualAlloc(LPVOID address, SIZE_T size, DWORD type, DWORD protect);
BOOL VirtualFree(LPVOID address, SIZE_T size, DWORD type);
#define MEM_COMMIT 0x00001000
#define MEM_RESERVE 0x00002000
#define MEM_RELEASE 0x00008000
#define PAGE_READWRITE 0x04

// ---- kernel: strings ---------------------------------------------------------------------
int MultiByteToWideChar(UINT codePage, DWORD flags, LPCSTR src, int srcLen, LPWSTR dst, int dstLen);
int WideCharToMultiByte(UINT codePage, DWORD flags, LPCWSTR src, int srcLen, LPSTR dst, int dstLen, LPCSTR defChar, LPBOOL usedDefChar);
UINT GetACP();
UINT GetOEMCP();
BOOL IsValidCodePage(UINT codePage);
BOOL IsDBCSLeadByteEx(UINT codePage, BYTE c);
int lstrlenW(LPCWSTR s);
int lstrlenA(LPCSTR s);
LPWSTR lstrcpyW(LPWSTR dst, LPCWSTR src);
LPSTR lstrcpyA(LPSTR dst, LPCSTR src);
LPWSTR lstrcpynW(LPWSTR dst, LPCWSTR src, int max);
LPSTR lstrcpynA(LPSTR dst, LPCSTR src, int max);
LPWSTR lstrcatW(LPWSTR dst, LPCWSTR src);
LPSTR lstrcatA(LPSTR dst, LPCSTR src);
int lstrcmpW(LPCWSTR a, LPCWSTR b);
int lstrcmpiW(LPCWSTR a, LPCWSTR b);
int lstrcmpA(LPCSTR a, LPCSTR b);
int lstrcmpiA(LPCSTR a, LPCSTR b);
#define lstrlen lstrlenW
#define lstrcpy lstrcpyW
#define lstrcpyn lstrcpynW
#define lstrcat lstrcatW
#define lstrcmp lstrcmpW
#define lstrcmpi lstrcmpiW
int CompareStringW(LCID locale, DWORD flags, LPCWSTR a, int lenA, LPCWSTR b, int lenB);
int CompareStringEx(LPCWSTR locale, DWORD flags, LPCWSTR a, int lenA, LPCWSTR b, int lenB, void*, void*, LPARAM);
int CompareStringOrdinal(LPCWSTR a, int lenA, LPCWSTR b, int lenB, BOOL ignoreCase);
#define CompareString CompareStringW
int LCMapStringW(LCID locale, DWORD flags, LPCWSTR src, int srcLen, LPWSTR dst, int dstLen);
int LCMapStringEx(LPCWSTR locale, DWORD flags, LPCWSTR src, int srcLen, LPWSTR dst, int dstLen, void*, void*, LPARAM);
#define LCMapString LCMapStringW
BOOL GetStringTypeW(DWORD infoType, LPCWSTR src, int len, LPWORD charType);
#define CT_CTYPE1 0x00000001
#define C1_UPPER 0x0001
#define C1_LOWER 0x0002
#define C1_DIGIT 0x0004
#define C1_SPACE 0x0008
#define C1_PUNCT 0x0010
#define C1_CNTRL 0x0020
#define C1_BLANK 0x0040
#define C1_XDIGIT 0x0080
#define C1_ALPHA 0x0100
LCID GetUserDefaultLCID();
LCID GetThreadLocale();
LANGID GetUserDefaultUILanguage();
LANGID GetUserDefaultLangID();
int GetLocaleInfoW(LCID locale, DWORD type, LPWSTR data, int max);
int GetLocaleInfoEx(LPCWSTR locale, DWORD type, LPWSTR data, int max);
BOOL IsValidLocaleName(LPCWSTR name); // a language QLocale knows ("en-US", "de")
#define LOCALE_RETURN_NUMBER 0x20000000
#define LOCALE_SLOCALIZEDDISPLAYNAME 0x00000002
#define LOCALE_SENGLISHLANGUAGENAME 0x00001001
#define LOCALE_SNATIVELANGUAGENAME 0x00000004
#define LOCALE_SENGLISHCOUNTRYNAME 0x00001002
#define LOCALE_SNATIVECOUNTRYNAME 0x00000008
#define LOCALE_SNAME 0x0000005c
#define LOCALE_SENGLISHDISPLAYNAME 0x00000072
#define LOCALE_SNATIVEDISPLAYNAME 0x00000073
#define LOCALE_NOUSEROVERRIDE 0x80000000
#define LOCALE_SGROUPING 0x00000010
#define LOCALE_IDIGITS 0x00000011
#define LOCALE_ILZERO 0x00000012
#define LOCALE_INEGNUMBER 0x00001010
#define LOCALE_SNEGATIVESIGN 0x00000051
struct NUMBERFMTW
{
	UINT NumDigits;
	UINT LeadingZero;
	UINT Grouping;
	LPWSTR lpDecimalSep;
	LPWSTR lpThousandSep;
	UINT NegativeOrder;
};
typedef NUMBERFMTW NUMBERFMT;
typedef NUMBERFMTW* LPNUMBERFMTW;
typedef NUMBERFMTW* LPNUMBERFMT;
int GetNumberFormatW(LCID locale, DWORD flags, LPCWSTR value, const NUMBERFMTW* format, LPWSTR out, int max);
int GetNumberFormatEx(LPCWSTR locale, DWORD flags, LPCWSTR value, const NUMBERFMTW* format, LPWSTR out, int max);
#define GetNumberFormat GetNumberFormatW
#define GetLocaleInfo GetLocaleInfoW
#define LOCALE_SDECIMAL 0x0000000E
#define LOCALE_STHOUSAND 0x0000000F
#define LOCALE_SISO639LANGNAME 0x00000059
#define LOCALE_SISO3166CTRYNAME 0x0000005A
int GetDateFormatW(LCID locale, DWORD flags, const SYSTEMTIME* date, LPCWSTR format, LPWSTR out, int max);
int GetTimeFormatW(LCID locale, DWORD flags, const SYSTEMTIME* time, LPCWSTR format, LPWSTR out, int max);
#define GetDateFormat GetDateFormatW
#define GetTimeFormat GetTimeFormatW
#define DATE_SHORTDATE 0x00000001
#define DATE_LONGDATE 0x00000002
#define TIME_NOSECONDS 0x00000002

// ---- kernel: files and folders -------------------------------------------------------------
HANDLE CreateFileW(LPCWSTR path, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES sa, DWORD disposition, DWORD flags, HANDLE templ);
HANDLE CreateFileA(LPCSTR path, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES sa, DWORD disposition, DWORD flags, HANDLE templ);
#define CreateFile CreateFileW
BOOL ReadFile(HANDLE h, LPVOID buffer, DWORD toRead, LPDWORD read, LPOVERLAPPED ov);
BOOL WriteFile(HANDLE h, LPCVOID buffer, DWORD toWrite, LPDWORD written, LPOVERLAPPED ov);
BOOL FlushFileBuffers(HANDLE h);
BOOL CloseHandle(HANDLE h);
DWORD GetFileSize(HANDLE h, LPDWORD high);
struct BY_HANDLE_FILE_INFORMATION
{
	DWORD dwFileAttributes;
	FILETIME ftCreationTime;
	FILETIME ftLastAccessTime;
	FILETIME ftLastWriteTime;
	DWORD dwVolumeSerialNumber;
	DWORD nFileSizeHigh;
	DWORD nFileSizeLow;
	DWORD nNumberOfLinks;
	DWORD nFileIndexHigh; // the inode
	DWORD nFileIndexLow;
};
typedef BY_HANDLE_FILE_INFORMATION* LPBY_HANDLE_FILE_INFORMATION;
BOOL GetFileInformationByHandle(HANDLE h, LPBY_HANDLE_FILE_INFORMATION info);
BOOL GetFileSizeEx(HANDLE h, PLARGE_INTEGER size);
DWORD SetFilePointer(HANDLE h, LONG distance, PLONG high, DWORD method);
BOOL SetFilePointerEx(HANDLE h, LARGE_INTEGER distance, PLARGE_INTEGER newPos, DWORD method);
BOOL SetEndOfFile(HANDLE h);
BOOL GetFileTime(HANDLE h, LPFILETIME created, LPFILETIME accessed, LPFILETIME written);
DWORD GetFileType(HANDLE h);
#define FILE_TYPE_UNKNOWN 0x0000
#define FILE_TYPE_DISK 0x0001
#define FILE_TYPE_CHAR 0x0002
#define FILE_TYPE_PIPE 0x0003
DWORD GetFileAttributesW(LPCWSTR path);
DWORD GetFileAttributesA(LPCSTR path);
BOOL GetFileAttributesExW(LPCWSTR path, GET_FILEEX_INFO_LEVELS level, LPVOID info);
BOOL SetFileAttributesW(LPCWSTR path, DWORD attributes);
#define GetFileAttributes GetFileAttributesW
#define GetFileAttributesEx GetFileAttributesExW
#define SetFileAttributes SetFileAttributesW
BOOL DeleteFileW(LPCWSTR path);
BOOL DeleteFileA(LPCSTR path);
BOOL CopyFileW(LPCWSTR from, LPCWSTR to, BOOL failIfExists);
BOOL MoveFileW(LPCWSTR from, LPCWSTR to);
BOOL MoveFileExW(LPCWSTR from, LPCWSTR to, DWORD flags);
BOOL ReplaceFileW(LPCWSTR replaced, LPCWSTR replacement, LPCWSTR backup, DWORD flags, LPVOID, LPVOID);
#define DeleteFile DeleteFileW
#define CopyFile CopyFileW
#define MoveFile MoveFileW
#define MoveFileEx MoveFileExW
#define ReplaceFile ReplaceFileW
BOOL CreateDirectoryW(LPCWSTR path, LPSECURITY_ATTRIBUTES sa);
BOOL CreateDirectoryA(LPCSTR path, LPSECURITY_ATTRIBUTES sa);
BOOL RemoveDirectoryW(LPCWSTR path);
#define CreateDirectory CreateDirectoryW
#define RemoveDirectory RemoveDirectoryW
HANDLE FindFirstFileW(LPCWSTR pattern, LPWIN32_FIND_DATAW data);
BOOL FindNextFileW(HANDLE h, LPWIN32_FIND_DATAW data);
BOOL FindClose(HANDLE h);
#define FindFirstFile FindFirstFileW
#define FindNextFile FindNextFileW
DWORD GetCurrentDirectoryW(DWORD max, LPWSTR buffer);
BOOL SetCurrentDirectoryW(LPCWSTR path);
#define GetCurrentDirectory GetCurrentDirectoryW
#define SetCurrentDirectory SetCurrentDirectoryW
DWORD GetTempPathW(DWORD max, LPWSTR buffer);
UINT GetTempFileNameW(LPCWSTR path, LPCWSTR prefix, UINT unique, LPWSTR out);
#define GetTempPath GetTempPathW
#define GetTempFileName GetTempFileNameW
DWORD GetFullPathNameW(LPCWSTR path, DWORD max, LPWSTR buffer, LPWSTR* filePart);
DWORD GetLongPathNameW(LPCWSTR path, LPWSTR buffer, DWORD max);
DWORD GetShortPathNameW(LPCWSTR path, LPWSTR buffer, DWORD max);
#define GetFullPathName GetFullPathNameW
#define GetLongPathName GetLongPathNameW
#define GetShortPathName GetShortPathNameW
UINT GetWindowsDirectoryW(LPWSTR buffer, UINT max);
UINT GetSystemDirectoryW(LPWSTR buffer, UINT max);
#define GetWindowsDirectory GetWindowsDirectoryW
#define GetSystemDirectory GetSystemDirectoryW
BOOL GetDiskFreeSpaceExW(LPCWSTR dir, PULARGE_INTEGER freeToCaller, PULARGE_INTEGER total, PULARGE_INTEGER totalFree);
#define GetDiskFreeSpaceEx GetDiskFreeSpaceExW
UINT GetDriveTypeW(LPCWSTR root);
#define GetDriveType GetDriveTypeW
DWORD GetLogicalDrives();
BOOL GetVolumeInformationW(LPCWSTR root, LPWSTR volumeName, DWORD volumeNameSize, LPDWORD serial, LPDWORD maxComponent,
	LPDWORD fsFlags, LPWSTR fsName, DWORD fsNameSize);
#define GetVolumeInformation GetVolumeInformationW
struct COMBOBOXINFO
{
	DWORD cbSize;
	RECT rcItem;
	RECT rcButton;
	DWORD stateButton;
	HWND hwndCombo;
	HWND hwndItem; // the combo box itself: the messages of its edit go to it
	HWND hwndList;
};
typedef COMBOBOXINFO* PCOMBOBOXINFO;
typedef COMBOBOXINFO* LPCOMBOBOXINFO;
BOOL GetComboBoxInfo(HWND hwndCombo, PCOMBOBOXINFO info);
DWORD GetLogicalDriveStringsW(DWORD max, LPWSTR buffer);
#define GetLogicalDriveStrings GetLogicalDriveStringsW
#define DRIVE_UNKNOWN 0
#define DRIVE_NO_ROOT_DIR 1
#define DRIVE_REMOVABLE 2
#define DRIVE_FIXED 3
#define DRIVE_REMOTE 4
#define DRIVE_CDROM 5
HANDLE FindFirstChangeNotificationW(LPCWSTR path, BOOL subtree, DWORD filter);
BOOL FindNextChangeNotification(HANDLE h);
BOOL FindCloseChangeNotification(HANDLE h);
#define FindFirstChangeNotification FindFirstChangeNotificationW
#define FILE_NOTIFY_CHANGE_FILE_NAME 0x00000001
#define FILE_NOTIFY_CHANGE_DIR_NAME 0x00000002
#define FILE_NOTIFY_CHANGE_ATTRIBUTES 0x00000004
#define FILE_NOTIFY_CHANGE_SIZE 0x00000008
#define FILE_NOTIFY_CHANGE_LAST_WRITE 0x00000010
BOOL FileTimeToSystemTime(const FILETIME* ft, LPSYSTEMTIME st);
BOOL SystemTimeToFileTime(const SYSTEMTIME* st, LPFILETIME ft);
BOOL FileTimeToLocalFileTime(const FILETIME* ft, LPFILETIME local);
BOOL LocalFileTimeToFileTime(const FILETIME* local, LPFILETIME ft);
LONG CompareFileTime(const FILETIME* a, const FILETIME* b);
BOOL SystemTimeToTzSpecificLocalTime(const void* tz, const SYSTEMTIME* universal, LPSYSTEMTIME local);

// ---- kernel: INI files -------------------------------------------------------------------
DWORD GetPrivateProfileStringW(LPCWSTR section, LPCWSTR key, LPCWSTR def, LPWSTR out, DWORD max, LPCWSTR file);
DWORD GetPrivateProfileStringA(LPCSTR section, LPCSTR key, LPCSTR def, LPSTR out, DWORD max, LPCSTR file);
UINT GetPrivateProfileIntW(LPCWSTR section, LPCWSTR key, INT def, LPCWSTR file);
UINT GetPrivateProfileIntA(LPCSTR section, LPCSTR key, INT def, LPCSTR file);
BOOL WritePrivateProfileStringW(LPCWSTR section, LPCWSTR key, LPCWSTR value, LPCWSTR file);
BOOL WritePrivateProfileStringA(LPCSTR section, LPCSTR key, LPCSTR value, LPCSTR file);
DWORD GetPrivateProfileSectionW(LPCWSTR section, LPWSTR out, DWORD max, LPCWSTR file);
DWORD GetPrivateProfileSectionNamesW(LPWSTR out, DWORD max, LPCWSTR file);
BOOL WritePrivateProfileSectionW(LPCWSTR section, LPCWSTR data, LPCWSTR file);
BOOL GetPrivateProfileStructW(LPCWSTR section, LPCWSTR key, LPVOID data, UINT size, LPCWSTR file);
BOOL WritePrivateProfileStructW(LPCWSTR section, LPCWSTR key, LPVOID data, UINT size, LPCWSTR file);
#define GetPrivateProfileString GetPrivateProfileStringW
#define GetPrivateProfileInt GetPrivateProfileIntW
#define WritePrivateProfileString WritePrivateProfileStringW
#define GetPrivateProfileSection GetPrivateProfileSectionW
#define GetPrivateProfileSectionNames GetPrivateProfileSectionNamesW
#define WritePrivateProfileSection WritePrivateProfileSectionW
#define GetPrivateProfileStruct GetPrivateProfileStructW
#define WritePrivateProfileStruct WritePrivateProfileStructW

// ---- kernel: modules, processes, threads, synchronization ----------------------------------
DWORD GetModuleFileNameW(HMODULE hModule, LPWSTR path, DWORD max);
DWORD GetModuleFileNameA(HMODULE hModule, LPSTR path, DWORD max);
#define GetModuleFileName GetModuleFileNameW
HMODULE GetModuleHandleW(LPCWSTR name);
HMODULE GetModuleHandleA(LPCSTR name);
BOOL GetModuleHandleExW(DWORD flags, LPCWSTR name, HMODULE* hModule);
#define GetModuleHandle GetModuleHandleW
#define GetModuleHandleEx GetModuleHandleExW
#define GET_MODULE_HANDLE_EX_FLAG_PIN 0x00000001 // modules of the plugins stay loaded until Notepad++ exits
#define GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS 0x00000004
#define GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT 0x00000002
HMODULE LoadLibraryW(LPCWSTR path);
HMODULE LoadLibraryA(LPCSTR path);
HMODULE LoadLibraryExW(LPCWSTR path, HANDLE file, DWORD flags);
#define LoadLibrary LoadLibraryW
#define LoadLibraryEx LoadLibraryExW
#define LOAD_LIBRARY_AS_DATAFILE 0x00000002
#define LOAD_LIBRARY_SEARCH_SYSTEM32 0x00000800
FARPROC GetProcAddress(HMODULE hModule, LPCSTR name);
BOOL FreeLibrary(HMODULE hModule);
BOOL DisableThreadLibraryCalls(HMODULE hModule);
DWORD GetLastError();
void SetLastError(DWORD error);
DWORD FormatMessageW(DWORD flags, LPCVOID source, DWORD id, DWORD lang, LPWSTR buffer, DWORD size, va_list* args);
DWORD FormatMessageA(DWORD flags, LPCVOID source, DWORD id, DWORD lang, LPSTR buffer, DWORD size, va_list* args);
#define FormatMessage FormatMessageW
void OutputDebugStringW(LPCWSTR s);
void OutputDebugStringA(LPCSTR s);
#define OutputDebugString OutputDebugStringW
BOOL IsDebuggerPresent();
void DebugBreak();
DWORD GetTickCount();
ULONGLONG GetTickCount64();
BOOL QueryPerformanceCounter(LARGE_INTEGER* counter);
BOOL QueryPerformanceFrequency(LARGE_INTEGER* frequency);
void GetLocalTime(LPSYSTEMTIME st);
void GetSystemTime(LPSYSTEMTIME st);
void GetSystemTimeAsFileTime(LPFILETIME ft);
void Sleep(DWORD ms);
DWORD SleepEx(DWORD ms, BOOL alertable);
DWORD GetCurrentProcessId();
DWORD GetCurrentThreadId();
HANDLE GetCurrentProcess();
HANDLE GetCurrentThread();
HANDLE CreateThread(LPSECURITY_ATTRIBUTES sa, SIZE_T stack, LPTHREAD_START_ROUTINE start, LPVOID param, DWORD flags, LPDWORD id);
void ExitThread(DWORD code);
BOOL TerminateThread(HANDLE h, DWORD code);
BOOL GetExitCodeThread(HANDLE h, LPDWORD code);
DWORD ResumeThread(HANDLE h);
DWORD SuspendThread(HANDLE h);
BOOL SetThreadPriority(HANDLE h, int priority);
int GetThreadPriority(HANDLE h);
uintptr_t _beginthreadex(void* security, unsigned stack, unsigned (*start)(void*), void* arg, unsigned flags, unsigned* id);
uintptr_t _beginthread(void (*start)(void*), unsigned stack, void* arg);
void _endthreadex(unsigned code);
DWORD WaitForSingleObject(HANDLE h, DWORD ms);
DWORD WaitForMultipleObjects(DWORD count, const HANDLE* handles, BOOL all, DWORD ms);
DWORD MsgWaitForMultipleObjects(DWORD count, const HANDLE* handles, BOOL all, DWORD ms, DWORD wakeMask);
#define QS_ALLINPUT 0x04FF
HANDLE CreateEventW(LPSECURITY_ATTRIBUTES sa, BOOL manualReset, BOOL initialState, LPCWSTR name);
#define CreateEvent CreateEventW
HANDLE OpenEventW(DWORD access, BOOL inherit, LPCWSTR name); // events of CreateEvent with a name in this process
#define OpenEvent OpenEventW
BOOL SetEvent(HANDLE h);
BOOL ResetEvent(HANDLE h);
HANDLE CreateMutexW(LPSECURITY_ATTRIBUTES sa, BOOL initialOwner, LPCWSTR name);
#define CreateMutex CreateMutexW
BOOL ReleaseMutex(HANDLE h);
HANDLE CreateSemaphoreW(LPSECURITY_ATTRIBUTES sa, LONG initial, LONG maximum, LPCWSTR name);
#define CreateSemaphore CreateSemaphoreW
BOOL ReleaseSemaphore(HANDLE h, LONG count, LPLONG previous);
void InitializeCriticalSection(LPCRITICAL_SECTION cs);
BOOL InitializeCriticalSectionAndSpinCount(LPCRITICAL_SECTION cs, DWORD spin);
BOOL InitializeCriticalSectionEx(LPCRITICAL_SECTION cs, DWORD spin, DWORD flags);
void DeleteCriticalSection(LPCRITICAL_SECTION cs);
void EnterCriticalSection(LPCRITICAL_SECTION cs);
BOOL TryEnterCriticalSection(LPCRITICAL_SECTION cs);
void LeaveCriticalSection(LPCRITICAL_SECTION cs);
void InitializeSRWLock(PSRWLOCK lock);
void AcquireSRWLockExclusive(PSRWLOCK lock);
void ReleaseSRWLockExclusive(PSRWLOCK lock);
void AcquireSRWLockShared(PSRWLOCK lock);
void ReleaseSRWLockShared(PSRWLOCK lock);
LONG InterlockedIncrement(LONG volatile* v);
LONG InterlockedDecrement(LONG volatile* v);
LONG InterlockedExchange(LONG volatile* v, LONG value);
LONG InterlockedCompareExchange(LONG volatile* v, LONG exchange, LONG comparand);
LONG InterlockedExchangeAdd(LONG volatile* v, LONG value);
void* InterlockedExchangePointer(void* volatile* v, void* value);
void* InterlockedCompareExchangePointer(void* volatile* v, void* exchange, void* comparand);
BOOL CreateProcessW(LPCWSTR app, LPWSTR cmdLine, LPSECURITY_ATTRIBUTES psa, LPSECURITY_ATTRIBUTES tsa, BOOL inherit, DWORD flags,
	LPVOID env, LPCWSTR dir, LPSTARTUPINFOW si, LPPROCESS_INFORMATION pi);
#define CreateProcess CreateProcessW
BOOL GetExitCodeProcess(HANDLE h, LPDWORD code);
BOOL TerminateProcess(HANDLE h, UINT code);
DWORD GetProcessId(HANDLE h);
HANDLE OpenProcess(DWORD access, BOOL inherit, DWORD pid);
BOOL CreatePipe(PHANDLE readPipe, PHANDLE writePipe, LPSECURITY_ATTRIBUTES sa, DWORD size);
BOOL PeekNamedPipe(HANDLE h, LPVOID buffer, DWORD size, LPDWORD read, LPDWORD available, LPDWORD left);
BOOL SetHandleInformation(HANDLE h, DWORD mask, DWORD flags);
BOOL DuplicateHandle(HANDLE srcProcess, HANDLE src, HANDLE dstProcess, LPHANDLE dst, DWORD access, BOOL inherit, DWORD options);
#define DUPLICATE_SAME_ACCESS 0x00000002
HANDLE GetStdHandle(DWORD which);
DWORD GetEnvironmentVariableW(LPCWSTR name, LPWSTR buffer, DWORD max);
BOOL SetEnvironmentVariableW(LPCWSTR name, LPCWSTR value);
DWORD ExpandEnvironmentStringsW(LPCWSTR src, LPWSTR dst, DWORD max);
LPWSTR GetCommandLineW();
LPWSTR GetEnvironmentStringsW();
BOOL FreeEnvironmentStringsW(LPWSTR env);
#define GetEnvironmentVariable GetEnvironmentVariableW
#define SetEnvironmentVariable SetEnvironmentVariableW
#define ExpandEnvironmentStrings ExpandEnvironmentStringsW
#define GetCommandLine GetCommandLineW
#define GetEnvironmentStrings GetEnvironmentStringsW
#define FreeEnvironmentStrings FreeEnvironmentStringsW
BOOL GetUserNameW(LPWSTR buffer, LPDWORD size);
BOOL GetComputerNameW(LPWSTR buffer, LPDWORD size);
#define GetUserName GetUserNameW
#define GetComputerName GetComputerNameW
struct SYSTEM_INFO
{
	WORD wProcessorArchitecture;
	WORD wReserved;
	DWORD dwPageSize;
	LPVOID lpMinimumApplicationAddress;
	LPVOID lpMaximumApplicationAddress;
	DWORD_PTR dwActiveProcessorMask;
	DWORD dwNumberOfProcessors;
	DWORD dwProcessorType;
	DWORD dwAllocationGranularity;
	WORD wProcessorLevel;
	WORD wProcessorRevision;
};
void GetSystemInfo(SYSTEM_INFO* si);
void GetNativeSystemInfo(SYSTEM_INFO* si);
struct OSVERSIONINFOW
{
	DWORD dwOSVersionInfoSize;
	DWORD dwMajorVersion;
	DWORD dwMinorVersion;
	DWORD dwBuildNumber;
	DWORD dwPlatformId;
	WCHAR szCSDVersion[128];
};
typedef OSVERSIONINFOW OSVERSIONINFO;
typedef OSVERSIONINFOW* LPOSVERSIONINFOW;
typedef OSVERSIONINFOW* LPOSVERSIONINFO;
struct OSVERSIONINFOEXW
{
	DWORD dwOSVersionInfoSize;
	DWORD dwMajorVersion;
	DWORD dwMinorVersion;
	DWORD dwBuildNumber;
	DWORD dwPlatformId;
	WCHAR szCSDVersion[128];
	WORD wServicePackMajor;
	WORD wServicePackMinor;
	WORD wSuiteMask;
	BYTE wProductType;
	BYTE wReserved;
};
typedef OSVERSIONINFOEXW OSVERSIONINFOEX;
typedef OSVERSIONINFOEXW* LPOSVERSIONINFOEXW;
typedef OSVERSIONINFOEXW* LPOSVERSIONINFOEX;
#define VER_NT_WORKSTATION 0x0000001
#define VER_NT_DOMAIN_CONTROLLER 0x0000002
#define VER_NT_SERVER 0x0000003
// GetProductInfo: a desktop edition here (PRODUCT_PROFESSIONAL), never a "server core"
BOOL GetProductInfo(DWORD major, DWORD minor, DWORD spMajor, DWORD spMinor, PDWORD productType);
#define PRODUCT_UNDEFINED 0x00000000
#define PRODUCT_ULTIMATE 0x00000001
#define PRODUCT_HOME_BASIC 0x00000002
#define PRODUCT_HOME_PREMIUM 0x00000003
#define PRODUCT_ENTERPRISE 0x00000004
#define PRODUCT_BUSINESS 0x00000006
#define PRODUCT_STANDARD_SERVER 0x00000007
#define PRODUCT_DATACENTER_SERVER 0x00000008
#define PRODUCT_ENTERPRISE_SERVER 0x0000000A
#define PRODUCT_STARTER 0x0000000B
#define PRODUCT_DATACENTER_SERVER_CORE 0x0000000C
#define PRODUCT_STANDARD_SERVER_CORE 0x0000000D
#define PRODUCT_ENTERPRISE_SERVER_CORE 0x0000000E
#define PRODUCT_WEB_SERVER_CORE 0x0000001D
#define PRODUCT_STANDARD_SERVER_CORE_V 0x00000028
#define PRODUCT_DATACENTER_SERVER_CORE_V 0x00000027
#define PRODUCT_ENTERPRISE_SERVER_CORE_V 0x00000029
#define PRODUCT_STORAGE_EXPRESS_SERVER_CORE 0x0000002B
#define PRODUCT_STORAGE_STANDARD_SERVER_CORE 0x0000002C
#define PRODUCT_STORAGE_WORKGROUP_SERVER_CORE 0x0000002D
#define PRODUCT_STORAGE_ENTERPRISE_SERVER_CORE 0x0000002E
#define PRODUCT_PROFESSIONAL 0x00000030
#define PRODUCT_STANDARD_SERVER_SOLUTIONS_CORE 0x00000035
#define PRODUCT_SMALLBUSINESS_SERVER_PREMIUM_CORE 0x0000003F
#define PRODUCT_DATACENTER_A_SERVER_CORE 0x00000091
#define PRODUCT_STANDARD_A_SERVER_CORE 0x00000092
BOOL GetVersionExW(OSVERSIONINFOW* vi);
#define IS_TEXT_UNICODE_ASCII16 0x0001
#define IS_TEXT_UNICODE_REVERSE_ASCII16 0x0010
#define IS_TEXT_UNICODE_STATISTICS 0x0002
#define IS_TEXT_UNICODE_REVERSE_STATISTICS 0x0020
#define IS_TEXT_UNICODE_CONTROLS 0x0004
#define IS_TEXT_UNICODE_REVERSE_CONTROLS 0x0040
#define IS_TEXT_UNICODE_SIGNATURE 0x0008
#define IS_TEXT_UNICODE_REVERSE_SIGNATURE 0x0080
#define IS_TEXT_UNICODE_ILLEGAL_CHARS 0x0100
#define IS_TEXT_UNICODE_ODD_LENGTH 0x0200
#define IS_TEXT_UNICODE_DBCS_LEADBYTE 0x0400
#define IS_TEXT_UNICODE_NULL_BYTES 0x1000
#define IS_TEXT_UNICODE_UNICODE_MASK 0x000F
#define IS_TEXT_UNICODE_REVERSE_MASK 0x00F0
#define IS_TEXT_UNICODE_NOT_UNICODE_MASK 0x0F00
#define IS_TEXT_UNICODE_NOT_ASCII_MASK 0xF000
// is the buffer UTF-16 (little endian)? the tests of the flags asked, the results in *result
BOOL IsTextUnicode(const void* buffer, int size, LPINT result);
#define GetVersionEx GetVersionExW
DWORD GetVersion();
inline bool IsWindowsVistaOrGreater() { return true; }
inline bool IsWindows7OrGreater() { return true; }
inline bool IsWindows8OrGreater() { return true; }
inline bool IsWindows10OrGreater() { return true; }
DWORD GetFileVersionInfoSizeW(LPCWSTR file, LPDWORD handle);
BOOL GetFileVersionInfoW(LPCWSTR file, DWORD handle, DWORD size, LPVOID data);
BOOL VerQueryValueW(LPCVOID block, LPCWSTR subBlock, LPVOID* buffer, PUINT len);
#define GetFileVersionInfoSize GetFileVersionInfoSizeW
#define GetFileVersionInfo GetFileVersionInfoW
#define VerQueryValue VerQueryValueW
struct VS_FIXEDFILEINFO
{
	DWORD dwSignature;
	DWORD dwStrucVersion;
	DWORD dwFileVersionMS;
	DWORD dwFileVersionLS;
	DWORD dwProductVersionMS;
	DWORD dwProductVersionLS;
	DWORD dwFileFlagsMask;
	DWORD dwFileFlags;
	DWORD dwFileOS;
	DWORD dwFileType;
	DWORD dwFileSubtype;
	DWORD dwFileDateMS;
	DWORD dwFileDateLS;
};

// ---- registry (settings of plugins in QSettings) -------------------------------------
LONG RegOpenKeyExW(HKEY key, LPCWSTR subKey, DWORD options, ACCESS_MASK sam, PHKEY result);
LONG RegCreateKeyExW(HKEY key, LPCWSTR subKey, DWORD reserved, LPWSTR cls, DWORD options, ACCESS_MASK sam,
	LPSECURITY_ATTRIBUTES sa, PHKEY result, LPDWORD disposition);
LONG RegQueryValueExW(HKEY key, LPCWSTR name, LPDWORD reserved, LPDWORD type, LPBYTE data, LPDWORD size);
LONG RegSetValueExW(HKEY key, LPCWSTR name, DWORD reserved, DWORD type, const BYTE* data, DWORD size);
LONG RegDeleteValueW(HKEY key, LPCWSTR name);
LONG RegDeleteKeyW(HKEY key, LPCWSTR subKey);
LONG RegCloseKey(HKEY key);
LONG RegEnumKeyExW(HKEY key, DWORD index, LPWSTR name, LPDWORD nameLen, LPDWORD reserved, LPWSTR cls, LPDWORD clsLen, LPFILETIME ft);
LONG RegEnumValueW(HKEY key, DWORD index, LPWSTR name, LPDWORD nameLen, LPDWORD reserved, LPDWORD type, LPBYTE data, LPDWORD size);
LONG RegGetValueW(HKEY key, LPCWSTR subKey, LPCWSTR value, DWORD flags, LPDWORD type, PVOID data, LPDWORD size);
#define RegOpenKeyEx RegOpenKeyExW
#define RegCreateKeyEx RegCreateKeyExW
#define RegQueryValueEx RegQueryValueExW
#define RegSetValueEx RegSetValueExW
#define RegDeleteValue RegDeleteValueW
#define RegDeleteKey RegDeleteKeyW
#define RegEnumKeyEx RegEnumKeyExW
#define RegEnumValue RegEnumValueW
#define RegGetValue RegGetValueW
#define KEY_QUERY_VALUE 0x0001
#define KEY_SET_VALUE 0x0002
#define KEY_CREATE_SUB_KEY 0x0004
#define KEY_ENUMERATE_SUB_KEYS 0x0008
#define KEY_READ 0x20019
#define KEY_WRITE 0x20006
#define KEY_ALL_ACCESS 0xF003F
#define KEY_WOW64_64KEY 0x0100
#define KEY_WOW64_32KEY 0x0200
#define REG_NONE 0
#define REG_SZ 1
#define REG_EXPAND_SZ 2
#define REG_BINARY 3
#define REG_DWORD 4
#define REG_MULTI_SZ 7
#define REG_QWORD 11
#define REG_OPTION_NON_VOLATILE 0x00000000
#define REG_CREATED_NEW_KEY 0x00000001
#define REG_OPENED_EXISTING_KEY 0x00000002
#define RRF_RT_REG_SZ 0x00000002
#define RRF_RT_REG_DWORD 0x00000010
#define RRF_RT_ANY 0x0000ffff

// ---- COM (no COM on Linux: the calls fail politely) -----------------------------------
HRESULT CoInitialize(LPVOID reserved);
HRESULT CoInitializeEx(LPVOID reserved, DWORD coInit);
void CoUninitialize();
HRESULT CoCreateInstance(REFCLSID clsid, void* outer, DWORD context, REFIID iid, LPVOID* out);
void CoTaskMemFree(LPVOID p);
LPVOID CoTaskMemAlloc(SIZE_T size);
HRESULT CoCreateGuid(GUID* guid);
HRESULT OleInitialize(LPVOID reserved);
void OleUninitialize();
#define COINIT_APARTMENTTHREADED 0x2
#define COINIT_MULTITHREADED 0x0
#define COINIT_DISABLE_OLE1DDE 0x4
#define CLSCTX_INPROC_SERVER 0x1
#define CLSCTX_ALL 0x17
BSTR SysAllocString(const OLECHAR* s);
void SysFreeString(BSTR s);
UINT SysStringLen(BSTR s);

// ---- C run time of Microsoft ------------------------------------------------------------
// The wide printf functions of Windows take %s for a wchar_t string and %S for a char
// string, the ones of glibc the other way round: these versions follow Windows.
int npp_vsnwprintf(wchar_t* buffer, size_t count, const wchar_t* format, va_list args);
int npp_snwprintf(wchar_t* buffer, size_t count, const wchar_t* format, ...);
int npp_vfwprintf(FILE* f, const wchar_t* format, va_list args);
int npp_vswscanf(const wchar_t* s, const wchar_t* format, va_list args);
std::wstring npp_windows_wformat(const wchar_t* format); // the format with the glibc conversions
inline int npp_swprintf(wchar_t* buffer, size_t count, const wchar_t* format, ...)
{
	va_list ap;
	va_start(ap, format);
	const int n = npp_vsnwprintf(buffer, count, format, ap);
	va_end(ap);
	return n;
}
template <size_t N, typename... Args> inline int npp_swprintf(wchar_t (&buffer)[N], const wchar_t* format, Args... args)
{
	return npp_snwprintf(buffer, N, format, args...);
}
inline int npp_swprintf(wchar_t* buffer, const wchar_t* format, ...)
{
	va_list ap;
	va_start(ap, format);
	const int n = npp_vsnwprintf(buffer, 1 << 20, format, ap);
	va_end(ap);
	return n;
}
inline int npp_wprintf(const wchar_t* format, ...)
{
	va_list ap;
	va_start(ap, format);
	const int n = npp_vfwprintf(stdout, format, ap);
	va_end(ap);
	return n;
}
inline int npp_fwprintf(FILE* f, const wchar_t* format, ...)
{
	va_list ap;
	va_start(ap, format);
	const int n = npp_vfwprintf(f, format, ap);
	va_end(ap);
	return n;
}
inline int npp_swscanf(const wchar_t* s, const wchar_t* format, ...)
{
	va_list ap;
	va_start(ap, format);
	const int n = npp_vswscanf(s, format, ap);
	va_end(ap);
	return n;
}
inline int npp_vswprintf(wchar_t* buffer, size_t count, const wchar_t* format, va_list args)
{
	return npp_vsnwprintf(buffer, count, format, args);
}
template <size_t N> inline int npp_vswprintf(wchar_t (&buffer)[N], const wchar_t* format, va_list args)
{
	return npp_vsnwprintf(buffer, N, format, args);
}
#define swprintf npp_swprintf
#define vswprintf npp_vswprintf
#define wprintf npp_wprintf
#define fwprintf npp_fwprintf
#define swscanf npp_swscanf
#define swprintf_s npp_swprintf
#define _swprintf npp_swprintf
#define _snwprintf npp_snwprintf
#define _snwprintf_s(b, n, c, ...) npp_snwprintf(b, n, __VA_ARGS__)
#define _vsnwprintf npp_vsnwprintf
#define _vsnwprintf_s(b, n, c, f, a) npp_vsnwprintf(b, n, f, a)
#define vswprintf_s npp_vswprintf
#define _stprintf npp_swprintf
#define _stprintf_s npp_swprintf
#define _sntprintf npp_snwprintf
#define _vstprintf_s npp_vswprintf
// sprintf_s(buffer, size, format, ...) and sprintf_s(char (&buffer)[N], format, ...) of MSVC
inline int npp_sprintf_s(char* buffer, size_t size, const char* format, ...)
{
	va_list ap;
	va_start(ap, format);
	const int n = vsnprintf(buffer, size, format, ap);
	va_end(ap);
	return n;
}
template <size_t N> inline int npp_sprintf_s(char (&buffer)[N], const char* format, ...)
{
	va_list ap;
	va_start(ap, format);
	const int n = vsnprintf(buffer, N, format, ap);
	va_end(ap);
	return n;
}
#define sprintf_s npp_sprintf_s
#define _snprintf snprintf
#define _snprintf_s(b, n, c, ...) snprintf(b, n, __VA_ARGS__)
#define _vsnprintf vsnprintf
#define vsprintf_s vsnprintf
#define vsnprintf_s(b, n, c, f, a) vsnprintf(b, n, f, a)
#define strncpy_s(d, n, s, c) npp_strncpy_s(d, n, s, c)
#define _wcsicmp wcscasecmp
#define _wcsnicmp wcsncasecmp
#define _stricmp strcasecmp
#define _strnicmp strncasecmp
#define stricmp strcasecmp
#define strnicmp strncasecmp
#define wcsicmp wcscasecmp
#define _wcsdup wcsdup
#define _strdup strdup
#define _wtoi(s) ((int)std::wcstol((s), nullptr, 10))
#define _wtol(s) std::wcstol((s), nullptr, 10)
#define _wtoi64(s) std::wcstoll((s), nullptr, 10)
#define _wtof(s) std::wcstod((s), nullptr)
#define _atoi64(s) std::strtoll((s), nullptr, 10)
#define _wcstoi64 std::wcstoll
#define _wcstoui64 std::wcstoull
#define _strtoi64 std::strtoll
#define _strtoui64 std::strtoull
#define _tcslen wcslen
#define _tcscpy wcscpy
#define _tcsncpy wcsncpy
#define _tcscat wcscat
#define _tcscmp wcscmp
#define _tcsicmp wcscasecmp
#define _tcsnicmp wcsncasecmp
#define _tcsncmp wcsncmp
#define _tcschr wcschr
#define _tcsrchr wcsrchr
#define _tcsstr wcsstr
#define _tcstok wcstok
#define _tcsncat wcsncat
// wcstok of MSVC: two arguments, the position kept between the calls (per thread)
inline wchar_t* wcstok(wchar_t* s, const wchar_t* delim)
{
	static thread_local wchar_t* context = nullptr;
	return ::wcstok(s, delim, &context);
}
#define _tcstol std::wcstol
#define _tcstoul std::wcstoul
#define _tcsdup wcsdup
#define _tcscpy_s wcscpy_s
#define _tcsncpy_s wcsncpy_s
#define _tcscat_s wcscat_s
#define _tcsncat_s wcsncat_s
#define _tcsnlen wcsnlen
#define _tcsspn wcsspn
#define _tcscspn wcscspn
#define _tcspbrk wcspbrk
#define _tcsncicmp wcsncasecmp
#define _tcsnccmp wcsncmp
#define _tcscoll wcscoll
#define _tcsicoll wcscasecmp
#define _tcstod std::wcstod
#define _tcstok_s wcstok
#define _tcslwr npp_wcslwr
#define _tcsupr npp_wcsupr
#define _tcslwr_s(s, n) npp_wcslwr(s)
#define _tcsupr_s(s, n) npp_wcsupr(s)
#define _tfopen npp_wfopen
#define _tfopen_s _wfopen_s
#define _tremove npp_wremove
#define _trename npp_wrename
#define _tgetenv npp_wgetenv
#define _tsystem npp_wsystem
#define _itot npp_itow
#define _itot_s _itow_s
#define _ttoi64 _wtoi64
#define _tstoi _wtoi
#define _tstof _wtof
#define _ftprintf npp_fwprintf
#define _tprintf npp_wprintf
#define _stscanf npp_swscanf
#define _sntprintf_s _snwprintf_s
#define _vsntprintf npp_vsnwprintf
#define _tsplitpath_s npp_wsplitpath_s
#define _tmakepath_s npp_wmakepath_s
#define _ttoi _wtoi
#define _ttol _wtol
#define _totupper towupper
#define _totlower towlower
#define _istspace iswspace
#define _istdigit iswdigit
#define _istalpha iswalpha
#define _istalnum iswalnum
#define _istxdigit iswxdigit
#define _istupper iswupper
#define _istlower iswlower
#define _istprint iswprint
#define _tcsftime wcsftime
#define _getcwd getcwd
#define _access access
#define _unlink unlink
#define _fileno fileno
#define _isatty isatty
#define _getpid getpid
#define _close close
#define _read read
#define _write write
#define _lseek lseek
#define _strlwr npp_strlwr
#define _strupr npp_strupr
#define _wcslwr npp_wcslwr
#define _wcsupr npp_wcsupr
#define _wcslwr_s(s, n) npp_wcslwr(s)
#define _wcsupr_s(s, n) npp_wcsupr(s)
#define _strlwr_s(s, n) npp_strlwr(s)
#define _strupr_s(s, n) npp_strupr(s)
#define _itow npp_itow
#define _itoa npp_itoa
#define _ltow npp_itow
#define _ltoa npp_itoa
#define _ultow npp_ultow
#define _i64tow npp_i64tow
#define _ui64tow npp_ui64tow
wchar_t* npp_itow(int value, wchar_t* buffer, int radix);
char* npp_itoa(int value, char* buffer, int radix);
wchar_t* npp_i64tow(long long value, wchar_t* buffer, int radix);
wchar_t* npp_ui64tow(unsigned long long value, wchar_t* buffer, int radix);
// the _s conversions of MSVC: with the buffer size, or a buffer array (template overload)
#define NPP_CONVERT_S(name, type, ch, impl) \
	inline int name(type v, ch* b, size_t, int r) { impl(v, b, r); return 0; } \
	template <size_t N> inline int name(type v, ch (&b)[N], int r) { impl(v, b, r); return 0; }
NPP_CONVERT_S(_itow_s, int, wchar_t, npp_itow)
NPP_CONVERT_S(_itoa_s, int, char, npp_itoa)
NPP_CONVERT_S(_ltow_s, long, wchar_t, npp_itow)
NPP_CONVERT_S(_i64tow_s, long long, wchar_t, npp_i64tow)
NPP_CONVERT_S(_ui64tow_s, unsigned long long, wchar_t, npp_ui64tow)
#undef NPP_CONVERT_S
#define _fwprintf_p fwprintf // glibc knows the positional arguments (%1$s)
#define _wprintf_p wprintf
#define _swprintf_p swprintf
// _wopen: open() with a wide path
#define _O_RDONLY O_RDONLY
#define _O_WRONLY O_WRONLY
#define _O_RDWR O_RDWR
#define _O_APPEND O_APPEND
#define _O_CREAT O_CREAT
#define _O_TRUNC O_TRUNC
#define _O_EXCL O_EXCL
#define _O_BINARY 0
#define _O_TEXT 0
#define _O_NOINHERIT O_CLOEXEC
#define _S_IREAD 0400
#define _S_IWRITE 0200
int _wopen(const wchar_t* path, int flags, int mode = 0644);
#define _wfopen npp_wfopen
#define _wfopen_s(f, p, m) ((*(f) = npp_wfopen(p, m)) ? 0 : 2)
#define fopen_s(f, p, m) ((*(f) = std::fopen(p, m)) ? 0 : 2)
#define _wremove npp_wremove
#define _wrename npp_wrename
#define _wmkdir npp_wmkdir
#define _wchdir npp_wchdir
#define _wgetcwd npp_wgetcwd
#define _wgetdcwd(drive, buffer, len) npp_wgetcwd(buffer, len)
#define _getdrive() 0 // no drives: the current folder of _wgetdcwd is the current folder
#define _wgetenv npp_wgetenv
#define _wsystem npp_wsystem
#define _wcsset npp_wcsset
#define wcstok_s wcstok
#define strtok_s strtok_r
#define _wsplitpath_s npp_wsplitpath_s
#define _wmakepath_s npp_wmakepath_s
#define _MAX_PATH MAX_PATH
#define _MAX_DRIVE 3
#define _MAX_DIR MAX_PATH
#define _MAX_FNAME MAX_PATH
#define _MAX_EXT MAX_PATH
#define _TRUNCATE ((size_t)-1)
#define __max(a, b) (((a) > (b)) ? (a) : (b))
#define __min(a, b) (((a) < (b)) ? (a) : (b))
char* npp_strlwr(char* s);
char* npp_strupr(char* s);
wchar_t* npp_wcslwr(wchar_t* s);
wchar_t* npp_wcsupr(wchar_t* s);
wchar_t* npp_wcsset(wchar_t* s, wchar_t c);
wchar_t* npp_itow(int value, wchar_t* buffer, int radix);
char* npp_itoa(int value, char* buffer, int radix);
wchar_t* npp_ultow(unsigned long value, wchar_t* buffer, int radix);
wchar_t* npp_i64tow(long long value, wchar_t* buffer, int radix);
wchar_t* npp_ui64tow(unsigned long long value, wchar_t* buffer, int radix);
FILE* npp_wfopen(const wchar_t* path, const wchar_t* mode);
#define _fseeki64 fseeko
#define _ftelli64 ftello
// a name for a temporary file (Windows: in the current folder; here: in the temporary folder)
wchar_t* _wtmpnam(wchar_t* buffer);
int npp_wremove(const wchar_t* path);
int npp_wrename(const wchar_t* from, const wchar_t* to);
int npp_wmkdir(const wchar_t* path);
int npp_wchdir(const wchar_t* path);
wchar_t* npp_wgetcwd(wchar_t* buffer, int len); // buffer NULL: malloc'ed (free)
wchar_t* npp_wgetenv(const wchar_t* name);
int npp_wsystem(const wchar_t* command);
int npp_wsplitpath_s(const wchar_t* path, wchar_t* drive, size_t driveLen, wchar_t* dir, size_t dirLen, wchar_t* fname, size_t fnameLen, wchar_t* ext, size_t extLen);
int npp_wmakepath_s(wchar_t* path, size_t len, const wchar_t* drive, const wchar_t* dir, const wchar_t* fname, const wchar_t* ext);
int npp_strncpy_s(char* dst, size_t dstLen, const char* src, size_t count);
std::string npp_path(const wchar_t* windowsPath); // UTF-8 path, '\\' as '/'
std::string npp_path(const char* windowsPath);

template <size_t N> inline int wcscpy_s(wchar_t (&dst)[N], const wchar_t* src)
{
	std::wcsncpy(dst, src ? src : L"", N - 1);
	dst[N - 1] = 0;
	return 0;
}
inline int wcscpy_s(wchar_t* dst, size_t n, const wchar_t* src)
{
	if (!dst || !n)
		return 22;
	std::wcsncpy(dst, src ? src : L"", n - 1);
	dst[n - 1] = 0;
	return 0;
}
template <size_t N> inline int wcscat_s(wchar_t (&dst)[N], const wchar_t* src)
{
	const size_t len = std::wcslen(dst);
	if (len + 1 < N)
		std::wcsncat(dst, src ? src : L"", N - len - 1);
	return 0;
}
inline int wcscat_s(wchar_t* dst, size_t n, const wchar_t* src)
{
	const size_t len = std::wcslen(dst);
	if (len + 1 < n)
		std::wcsncat(dst, src ? src : L"", n - len - 1);
	return 0;
}
template <size_t N> inline int wcsncpy_s(wchar_t (&dst)[N], const wchar_t* src, size_t count)
{
	const size_t n = count < N - 1 ? count : N - 1;
	std::wcsncpy(dst, src ? src : L"", n);
	dst[n] = 0;
	return 0;
}
inline int wcsncpy_s(wchar_t* dst, size_t dstLen, const wchar_t* src, size_t count)
{
	if (!dst || !dstLen)
		return 22;
	const size_t n = count < dstLen - 1 ? count : dstLen - 1;
	std::wcsncpy(dst, src ? src : L"", n);
	dst[n] = 0;
	return 0;
}
template <size_t N> inline int wcsncat_s(wchar_t (&dst)[N], const wchar_t* src, size_t count)
{
	const size_t len = std::wcslen(dst);
	if (len + 1 < N)
		std::wcsncat(dst, src ? src : L"", count < N - len - 1 ? count : N - len - 1);
	return 0;
}
template <size_t N> inline int strcpy_s(char (&dst)[N], const char* src)
{
	std::strncpy(dst, src ? src : "", N - 1);
	dst[N - 1] = 0;
	return 0;
}
inline int strcpy_s(char* dst, size_t n, const char* src)
{
	if (!dst || !n)
		return 22;
	std::strncpy(dst, src ? src : "", n - 1);
	dst[n - 1] = 0;
	return 0;
}
template <size_t N> inline int strcat_s(char (&dst)[N], const char* src)
{
	const size_t len = std::strlen(dst);
	if (len + 1 < N)
		std::strncat(dst, src ? src : "", N - len - 1);
	return 0;
}
inline int strcat_s(char* dst, size_t n, const char* src)
{
	const size_t len = std::strlen(dst);
	if (len + 1 < n)
		std::strncat(dst, src ? src : "", n - len - 1);
	return 0;
}
inline size_t wcsnlen_s(const wchar_t* s, size_t n) { return s ? wcsnlen(s, n) : 0; }
inline size_t strnlen_s(const char* s, size_t n) { return s ? strnlen(s, n) : 0; }
inline int memcpy_s(void* dst, size_t dstLen, const void* src, size_t count)
{
	if (count > dstLen)
		return 34;
	std::memcpy(dst, src, count);
	return 0;
}
inline int wmemcpy_s(wchar_t* dst, size_t dstLen, const wchar_t* src, size_t count)
{
	if (count > dstLen)
		return 34;
	std::wmemcpy(dst, src, count);
	return 0;
}
inline int memmove_s(void* dst, size_t dstLen, const void* src, size_t count)
{
	if (count > dstLen)
		return 34;
	std::memmove(dst, src, count);
	return 0;
}
