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

// Notepad++ Linux plugin SDK: the types and structures of the Win32 API.

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <cwchar>
#include <cwctype>

// ---- calling conventions, exports -----------------------------------------------
#ifndef __cdecl
#define __cdecl
#endif
#ifndef __stdcall
#define __stdcall
#endif
#ifndef _cdecl
#define _cdecl
#endif
#ifndef WINAPI
#define WINAPI
#ifndef FAR
#define FAR
#endif
#ifndef NEAR
#define NEAR
#endif
#ifndef CONST
#define CONST const
#endif
#define __RPC_FAR
#define __RPC_USER
#define __RPC_STUB
#endif
#ifndef WINAPIV
#define WINAPIV
#endif
#ifndef CALLBACK
#define CALLBACK
#endif
#ifndef APIENTRY
#define APIENTRY
#endif
#ifndef STDAPI
#define STDAPI extern "C" HRESULT
#endif
#ifndef STDMETHODCALLTYPE
#define STDMETHODCALLTYPE
#endif
#ifndef __declspec
#define __declspec(x) __attribute__((visibility("default")))
#endif
#ifndef __forceinline
#define __forceinline inline __attribute__((always_inline))
#endif
#ifndef UNREFERENCED_PARAMETER
#define UNREFERENCED_PARAMETER(p) (void)(p)
#endif
#ifndef _In_
#define _In_
#define _In_opt_
#define _Out_
#define _Out_opt_
#define _Inout_
#define _Inout_opt_
#define _In_z_
#define _Printf_format_string_
#define _Use_decl_annotations_
#define _Success_(x)
#define _In_reads_(x)
#define _Out_writes_(x)
#define _Out_writes_z_(x)
#endif

// ---- basic types ------------------------------------------------------------------
typedef int BOOL;
typedef int errno_t; // C run time of Microsoft
typedef struct __locale_struct* _locale_t; // locale_t of glibc
typedef unsigned char BOOLEAN;
typedef unsigned char BYTE;
typedef unsigned char UCHAR;
typedef unsigned char byte; // rpcndr.h of Windows
typedef char CHAR;
typedef char CCHAR;
typedef unsigned short WORD;
typedef unsigned short USHORT;
typedef short SHORT;
typedef unsigned int DWORD;
typedef unsigned int UINT;
typedef unsigned int UINT32;
typedef int INT;
typedef int INT32;
typedef int LONG;          // 32 bit, as on Windows
typedef unsigned int ULONG;
typedef long long LONGLONG;
typedef unsigned long long ULONGLONG;
typedef long long INT64;
typedef unsigned long long UINT64;
typedef unsigned long long DWORD64;
typedef unsigned long long DWORDLONG;
typedef float FLOAT;
typedef double DOUBLE;
typedef wchar_t WCHAR;
typedef wchar_t TCHAR;
typedef wchar_t _TCHAR;
typedef wchar_t OLECHAR;
typedef char* LPSTR;
typedef char* PSTR;
typedef const char* LPCSTR;
typedef const char* PCSTR;
typedef wchar_t* LPWSTR;
typedef wchar_t* PWSTR;
typedef const wchar_t* LPCWSTR;
typedef const wchar_t* PCWSTR;
typedef wchar_t* LPTSTR;
typedef wchar_t* PTSTR;
typedef const wchar_t* LPCTSTR;
typedef const wchar_t* PCTSTR;
typedef wchar_t* BSTR;
typedef void VOID;
typedef void* PVOID;
typedef void* LPVOID;
typedef const void* LPCVOID;
typedef BYTE* LPBYTE;
typedef BYTE* PBYTE;
typedef BOOL* LPBOOL;
typedef BOOL* PBOOL;
typedef DWORD* LPDWORD;
typedef DWORD* PDWORD;
typedef WORD* LPWORD;
typedef LONG* LPLONG;
typedef LONG* PLONG;
typedef int* LPINT;
typedef UINT* PUINT;
typedef UINT* LPUINT;
typedef intptr_t INT_PTR;
typedef intptr_t* PINT_PTR;
typedef uintptr_t UINT_PTR;
typedef uintptr_t* PUINT_PTR;
typedef intptr_t LONG_PTR;
typedef uintptr_t ULONG_PTR;
typedef uintptr_t DWORD_PTR;
typedef uintptr_t* PDWORD_PTR;
typedef size_t SIZE_T;
typedef intptr_t SSIZE_T;
typedef uintptr_t WPARAM;
typedef intptr_t LPARAM;
typedef intptr_t LRESULT;
typedef DWORD COLORREF;
typedef DWORD* LPCOLORREF;
typedef LONG HRESULT;
typedef DWORD LCID;
typedef DWORD LCTYPE;
typedef WORD LANGID;
typedef WORD ATOM;
typedef DWORD ACCESS_MASK;
typedef void* HANDLE;
typedef HANDLE* PHANDLE;
typedef HANDLE* LPHANDLE;

// ---- handles ------------------------------------------------------------------------
// One structure for every window and menu handle: the main window, the Scintilla
// views, menus and the windows of plugins. SendMessage() calls send().
enum NppHandleKind : unsigned int
{
	NPP_HANDLE_NOTEPAD = 1,   // nppData._nppHandle
	NPP_HANDLE_SCINTILLA = 2, // nppData._scintillaMainHandle / _scintillaSecondHandle, NPPM_CREATESCINTILLAHANDLE
	NPP_HANDLE_MENU = 3,      // NPPM_GETMENUHANDLE, CreateMenu(), CreatePopupMenu()
	NPP_HANDLE_WIDGET = 4     // a window of a plugin (a QWidget)
};

#define NPP_HANDLE_MAGIC 0x4E505048u // "NPPH"

struct NppHandle
{
	unsigned int magic = NPP_HANDLE_MAGIC;
	unsigned int kind = NPP_HANDLE_WIDGET;
	intptr_t (*send)(NppHandle* self, unsigned int msg, uintptr_t wParam, intptr_t lParam) = nullptr;
	int (*post)(NppHandle* self, unsigned int msg, uintptr_t wParam, intptr_t lParam) = nullptr;
	void* widget = nullptr; // QWidget* (QMenu* for menus)
	void* host = nullptr;   // data of the side that made the handle
	NppHandle* owner = nullptr; // menus and plugin windows: the main window
};

typedef NppHandle* HWND;
typedef NppHandle* HMENU;

// Images and other GDI objects of the plugin SDK start with this header; the host
// takes a pointer without it (nppIcon()) as a QIcon*.
#define NPP_IMAGE_MAGIC 0x4E505049u // "NPPI"
struct NppImageHeader
{
	unsigned int magic = NPP_IMAGE_MAGIC;
	int kind = 0;
	void* qicon = nullptr; // QIcon*
};

typedef void* HICON;
typedef void* HBITMAP;
typedef void* HCURSOR;
typedef void* HBRUSH;
typedef void* HPEN;
typedef void* HFONT;
typedef void* HRGN;
typedef void* HPALETTE;
typedef void* HGDIOBJ;
typedef void* HDC;
typedef void* HINSTANCE;
typedef void* HMODULE;
typedef void* HGLOBAL;
typedef void* HLOCAL;
typedef void* HRSRC;
typedef void* HACCEL;
typedef void* HHOOK;
typedef void* HKEY;
typedef void* HMONITOR;
typedef void* HDROP;
typedef void* HTHEME;
typedef void* HDWP;
typedef HKEY* PHKEY;
typedef int HFILE;

typedef LRESULT (CALLBACK* WNDPROC)(HWND, UINT, WPARAM, LPARAM);
typedef INT_PTR (CALLBACK* DLGPROC)(HWND, UINT, WPARAM, LPARAM);
typedef void (CALLBACK* TIMERPROC)(HWND, UINT, UINT_PTR, DWORD);
typedef LRESULT (CALLBACK* HOOKPROC)(int code, WPARAM wParam, LPARAM lParam);
typedef BOOL (CALLBACK* WNDENUMPROC)(HWND, LPARAM);
typedef DWORD (WINAPI* LPTHREAD_START_ROUTINE)(LPVOID);
typedef LPTHREAD_START_ROUTINE PTHREAD_START_ROUTINE;
typedef intptr_t (WINAPI* FARPROC)();

// ---- geometry ----------------------------------------------------------------------
struct RECT { LONG left; LONG top; LONG right; LONG bottom; };
struct POINT { LONG x; LONG y; };
struct POINTS { SHORT x; SHORT y; };
struct POINTL { LONG x; LONG y; };
typedef POINTL* PPOINTL;
struct SIZE { LONG cx; LONG cy; };
typedef RECT* LPRECT;
typedef RECT* PRECT;
typedef const RECT* LPCRECT;
typedef POINT* LPPOINT;
typedef POINT* PPOINT;
typedef SIZE* LPSIZE;
typedef SIZE SIZEL;
typedef RECT RECTL;

// ---- 64 bit values, time -----------------------------------------------------------
union LARGE_INTEGER
{
	struct { DWORD LowPart; LONG HighPart; };
	struct { DWORD LowPart; LONG HighPart; } u;
	LONGLONG QuadPart;
};
typedef LARGE_INTEGER* PLARGE_INTEGER;
union ULARGE_INTEGER
{
	struct { DWORD LowPart; DWORD HighPart; };
	struct { DWORD LowPart; DWORD HighPart; } u;
	ULONGLONG QuadPart;
};
typedef ULARGE_INTEGER* PULARGE_INTEGER;

struct FILETIME { DWORD dwLowDateTime; DWORD dwHighDateTime; };
typedef FILETIME* LPFILETIME;
typedef FILETIME* PFILETIME;
struct SYSTEMTIME
{
	WORD wYear;
	WORD wMonth;
	WORD wDayOfWeek;
	WORD wDay;
	WORD wHour;
	WORD wMinute;
	WORD wSecond;
	WORD wMilliseconds;
};
typedef SYSTEMTIME* LPSYSTEMTIME;
typedef SYSTEMTIME* PSYSTEMTIME;

// ---- kernel objects ----------------------------------------------------------------
struct CRITICAL_SECTION { void* impl = nullptr; };
typedef CRITICAL_SECTION* LPCRITICAL_SECTION;
typedef CRITICAL_SECTION* PCRITICAL_SECTION;
struct SRWLOCK { void* impl = nullptr; };
typedef SRWLOCK* PSRWLOCK;
#define SRWLOCK_INIT {nullptr}

struct SECURITY_ATTRIBUTES
{
	DWORD nLength;
	LPVOID lpSecurityDescriptor;
	BOOL bInheritHandle;
};
typedef SECURITY_ATTRIBUTES* LPSECURITY_ATTRIBUTES;
typedef SECURITY_ATTRIBUTES* PSECURITY_ATTRIBUTES;

struct OVERLAPPED
{
	ULONG_PTR Internal;
	ULONG_PTR InternalHigh;
	DWORD Offset;
	DWORD OffsetHigh;
	HANDLE hEvent;
};
typedef OVERLAPPED* LPOVERLAPPED;

struct STARTUPINFOW
{
	DWORD cb;
	LPWSTR lpReserved;
	LPWSTR lpDesktop;
	LPWSTR lpTitle;
	DWORD dwX, dwY, dwXSize, dwYSize, dwXCountChars, dwYCountChars, dwFillAttribute, dwFlags;
	WORD wShowWindow;
	WORD cbReserved2;
	LPBYTE lpReserved2;
	HANDLE hStdInput;
	HANDLE hStdOutput;
	HANDLE hStdError;
};
typedef STARTUPINFOW STARTUPINFO;
typedef SIZE_T* PSIZE_T;
struct COORD { SHORT X; SHORT Y; };
typedef COORD* PCOORD;
// process and thread attributes: InitializeProcThreadAttributeList fails here (no pseudo consoles)
typedef struct _PROC_THREAD_ATTRIBUTE_LIST* LPPROC_THREAD_ATTRIBUTE_LIST;
typedef LPPROC_THREAD_ATTRIBUTE_LIST PPROC_THREAD_ATTRIBUTE_LIST;
struct STARTUPINFOEXW { STARTUPINFOW StartupInfo; LPPROC_THREAD_ATTRIBUTE_LIST lpAttributeList; };
typedef STARTUPINFOEXW STARTUPINFOEX;
typedef STARTUPINFOEXW* LPSTARTUPINFOEXW;
typedef STARTUPINFOEXW* LPSTARTUPINFOEX;
// job objects: CreateJobObject fails here
struct JOBOBJECT_BASIC_LIMIT_INFORMATION
{
	LARGE_INTEGER PerProcessUserTimeLimit;
	LARGE_INTEGER PerJobUserTimeLimit;
	DWORD LimitFlags;
	SIZE_T MinimumWorkingSetSize;
	SIZE_T MaximumWorkingSetSize;
	DWORD ActiveProcessLimit;
	ULONG_PTR Affinity;
	DWORD PriorityClass;
	DWORD SchedulingClass;
};
struct IO_COUNTERS { ULONGLONG ReadOperationCount, WriteOperationCount, OtherOperationCount, ReadTransferCount, WriteTransferCount, OtherTransferCount; };
struct JOBOBJECT_EXTENDED_LIMIT_INFORMATION
{
	JOBOBJECT_BASIC_LIMIT_INFORMATION BasicLimitInformation;
	IO_COUNTERS IoInfo;
	SIZE_T ProcessMemoryLimit;
	SIZE_T JobMemoryLimit;
	SIZE_T PeakProcessMemoryUsed;
	SIZE_T PeakJobMemoryUsed;
};
enum JOBOBJECTINFOCLASS { JobObjectBasicLimitInformation = 2, JobObjectExtendedLimitInformation = 9 };
// security descriptors are accepted and ignored
struct SECURITY_DESCRIPTOR { BYTE Revision; BYTE Sbz1; WORD Control; PVOID Owner; PVOID Group; PVOID Sacl; PVOID Dacl; };
typedef PVOID PSECURITY_DESCRIPTOR;
typedef PVOID PACL;
struct GUITHREADINFO { DWORD cbSize; DWORD flags; HWND hwndActive; HWND hwndFocus; HWND hwndCapture; HWND hwndMenuOwner; HWND hwndMoveSize; HWND hwndCaret; RECT rcCaret; };
typedef GUITHREADINFO* PGUITHREADINFO;
typedef GUITHREADINFO* LPGUITHREADINFO;
typedef BOOL (*PHANDLER_ROUTINE)(DWORD ctrlType);
typedef STARTUPINFOW* LPSTARTUPINFOW;
typedef STARTUPINFOW* LPSTARTUPINFO;
struct PROCESS_INFORMATION
{
	HANDLE hProcess;
	HANDLE hThread;
	DWORD dwProcessId;
	DWORD dwThreadId;
};
typedef PROCESS_INFORMATION* LPPROCESS_INFORMATION;

struct WIN32_FIND_DATAW
{
	DWORD dwFileAttributes;
	FILETIME ftCreationTime;
	FILETIME ftLastAccessTime;
	FILETIME ftLastWriteTime;
	DWORD nFileSizeHigh;
	DWORD nFileSizeLow;
	DWORD dwReserved0;
	DWORD dwReserved1;
	WCHAR cFileName[MAX_PATH];
	WCHAR cAlternateFileName[14];
};
typedef WIN32_FIND_DATAW WIN32_FIND_DATA;
typedef WIN32_FIND_DATAW* LPWIN32_FIND_DATAW;
typedef WIN32_FIND_DATAW* LPWIN32_FIND_DATA;
typedef WIN32_FIND_DATAW* PWIN32_FIND_DATAW;
struct WIN32_FILE_ATTRIBUTE_DATA
{
	DWORD dwFileAttributes;
	FILETIME ftCreationTime;
	FILETIME ftLastAccessTime;
	FILETIME ftLastWriteTime;
	DWORD nFileSizeHigh;
	DWORD nFileSizeLow;
};
enum GET_FILEEX_INFO_LEVELS { GetFileExInfoStandard, GetFileExMaxInfoLevel };

// ---- windows -----------------------------------------------------------------------
struct MSG
{
	HWND hwnd;
	UINT message;
	WPARAM wParam;
	LPARAM lParam;
	DWORD time;
	POINT pt;
};
typedef MSG* LPMSG;
typedef MSG* PMSG;

struct WNDCLASSEXW
{
	UINT cbSize;
	UINT style;
	WNDPROC lpfnWndProc;
	int cbClsExtra;
	int cbWndExtra;
	HINSTANCE hInstance;
	HICON hIcon;
	HCURSOR hCursor;
	HBRUSH hbrBackground;
	LPCWSTR lpszMenuName;
	LPCWSTR lpszClassName;
	HICON hIconSm;
};
typedef WNDCLASSEXW WNDCLASSEX;
struct WNDCLASSW
{
	UINT style;
	WNDPROC lpfnWndProc;
	int cbClsExtra;
	int cbWndExtra;
	HINSTANCE hInstance;
	HICON hIcon;
	HCURSOR hCursor;
	HBRUSH hbrBackground;
	LPCWSTR lpszMenuName;
	LPCWSTR lpszClassName;
};
typedef WNDCLASSW WNDCLASS;

struct CREATESTRUCTW
{
	LPVOID lpCreateParams;
	HINSTANCE hInstance;
	HMENU hMenu;
	HWND hwndParent;
	int cy;
	int cx;
	int y;
	int x;
	LONG style;
	LPCWSTR lpszName;
	LPCWSTR lpszClass;
	DWORD dwExStyle;
};
typedef CREATESTRUCTW CREATESTRUCT;
typedef CREATESTRUCTW* LPCREATESTRUCTW;
typedef CREATESTRUCTW* LPCREATESTRUCT;

struct PAINTSTRUCT
{
	HDC hdc;
	BOOL fErase;
	RECT rcPaint;
	BOOL fRestore;
	BOOL fIncUpdate;
	BYTE rgbReserved[32];
};
typedef PAINTSTRUCT* LPPAINTSTRUCT;

struct MINMAXINFO
{
	POINT ptReserved;
	POINT ptMaxSize;
	POINT ptMaxPosition;
	POINT ptMinTrackSize;
	POINT ptMaxTrackSize;
};
typedef MINMAXINFO* LPMINMAXINFO;

struct WINDOWPOS
{
	HWND hwnd;
	HWND hwndInsertAfter;
	int x;
	int y;
	int cx;
	int cy;
	UINT flags;
};
typedef WINDOWPOS* LPWINDOWPOS;

struct WINDOWPLACEMENT
{
	UINT length;
	UINT flags;
	UINT showCmd;
	POINT ptMinPosition;
	POINT ptMaxPosition;
	RECT rcNormalPosition;
};

struct NMHDR
{
	HWND hwndFrom;
	UINT_PTR idFrom;
	UINT code;
};
typedef NMHDR* LPNMHDR;

struct DRAWITEMSTRUCT
{
	UINT CtlType;
	UINT CtlID;
	UINT itemID;
	UINT itemAction;
	UINT itemState;
	HWND hwndItem;
	HDC hDC;
	RECT rcItem;
	ULONG_PTR itemData;
};
typedef DRAWITEMSTRUCT* LPDRAWITEMSTRUCT;
struct MEASUREITEMSTRUCT
{
	UINT CtlType;
	UINT CtlID;
	UINT itemID;
	UINT itemWidth;
	UINT itemHeight;
	ULONG_PTR itemData;
};
typedef MEASUREITEMSTRUCT* LPMEASUREITEMSTRUCT;

struct MENUITEMINFOW
{
	UINT cbSize;
	UINT fMask;
	UINT fType;
	UINT fState;
	UINT wID;
	HMENU hSubMenu;
	HBITMAP hbmpChecked;
	HBITMAP hbmpUnchecked;
	ULONG_PTR dwItemData;
	LPWSTR dwTypeData;
	UINT cch;
	HBITMAP hbmpItem;
};
typedef MENUITEMINFOW MENUITEMINFO;
typedef MENUITEMINFOW* LPMENUITEMINFOW;
typedef MENUITEMINFOW* LPMENUITEMINFO;
typedef const MENUITEMINFOW* LPCMENUITEMINFOW;
typedef const MENUITEMINFOW* LPCMENUITEMINFO;

struct ACCEL
{
	BYTE fVirt;
	WORD key;
	WORD cmd;
};
typedef ACCEL* LPACCEL;

struct MONITORINFO
{
	DWORD cbSize;
	RECT rcMonitor;
	RECT rcWork;
	DWORD dwFlags;
};
typedef MONITORINFO* LPMONITORINFO;

struct SCROLLINFO
{
	UINT cbSize;
	UINT fMask;
	int nMin;
	int nMax;
	UINT nPage;
	int nPos;
	int nTrackPos;
};
typedef SCROLLINFO* LPSCROLLINFO;
typedef const SCROLLINFO* LPCSCROLLINFO;
struct SCROLLBARINFO
{
	DWORD cbSize;
	RECT rcScrollBar; // screen coordinates
	int dxyLineButton;
	int xyThumbTop;
	int xyThumbBottom;
	int reserved;
	DWORD rgstate[6]; // CCHILDREN_SCROLLBAR + 1
};
typedef SCROLLBARINFO* PSCROLLBARINFO;
typedef SCROLLBARINFO* LPSCROLLBARINFO;
#define OBJID_HSCROLL ((LONG)0xFFFFFFFA)
#define OBJID_VSCROLL ((LONG)0xFFFFFFFB)
#define OBJID_CLIENT ((LONG)0xFFFFFFFC)
#define STATE_SYSTEM_UNAVAILABLE 0x00000001
#define STATE_SYSTEM_PRESSED 0x00000008
#define STATE_SYSTEM_INVISIBLE 0x00008000
#define STATE_SYSTEM_OFFSCREEN 0x00010000
#define SIF_RANGE 0x0001
#define SIF_PAGE 0x0002
#define SIF_POS 0x0004
#define SIF_DISABLENOSCROLL 0x0008
#define SIF_TRACKPOS 0x0010
#define SIF_ALL (SIF_RANGE | SIF_PAGE | SIF_POS | SIF_TRACKPOS)
#define SB_HORZ 0
#define SB_VERT 1
#define SB_CTL 2
#define SB_BOTH 3
#define SB_LINEUP 0
#define SB_LINELEFT 0
#define SB_LINEDOWN 1
#define SB_LINERIGHT 1
#define SB_PAGEUP 2
#define SB_PAGELEFT 2
#define SB_PAGEDOWN 3
#define SB_PAGERIGHT 3
#define SB_THUMBPOSITION 4
#define SB_THUMBTRACK 5
#define SB_TOP 6
#define SB_LEFT 6
#define SB_BOTTOM 7
#define SB_RIGHT 7
#define SB_ENDSCROLL 8

struct TRACKMOUSEEVENT
{
	DWORD cbSize;
	DWORD dwFlags;
	HWND hwndTrack;
	DWORD dwHoverTime;
};
typedef TRACKMOUSEEVENT* LPTRACKMOUSEEVENT;
#define TME_HOVER 0x00000001
#define TME_LEAVE 0x00000002
#define TME_CANCEL 0x80000000
#define HOVER_DEFAULT 0xFFFFFFFF

struct KBDLLHOOKSTRUCT
{
	DWORD vkCode;
	DWORD scanCode;
	DWORD flags;
	DWORD time;
	ULONG_PTR dwExtraInfo;
};
struct MOUSEHOOKSTRUCT
{
	POINT pt;
	HWND hwnd;
	UINT wHitTestCode;
	ULONG_PTR dwExtraInfo;
};
struct CWPSTRUCT
{
	LPARAM lParam;
	WPARAM wParam;
	UINT message;
	HWND hwnd;
};
struct HELPINFO
{
	UINT cbSize;
	int iContextType;
	int iCtrlId;
	HANDLE hItemHandle;
	DWORD_PTR dwContextId;
	POINT MousePos;
};
typedef HELPINFO* LPHELPINFO;
struct COPYDATASTRUCT
{
	ULONG_PTR dwData;
	DWORD cbData;
	PVOID lpData;
};
typedef COPYDATASTRUCT* PCOPYDATASTRUCT;
#define WM_COPYDATA 0x004A

// dialog templates in memory (CreateDialogIndirectParam): the SDK makes them from the .rc
#pragma pack(push, 2)
struct DLGTEMPLATE
{
	DWORD style;
	DWORD dwExtendedStyle;
	WORD cdit;
	short x;
	short y;
	short cx;
	short cy;
};
#pragma pack(pop)
typedef DLGTEMPLATE* LPDLGTEMPLATE;
typedef DLGTEMPLATE* LPDLGTEMPLATEW;
typedef const DLGTEMPLATE* LPCDLGTEMPLATE;
typedef const DLGTEMPLATE* LPCDLGTEMPLATEW;

// ---- GDI -----------------------------------------------------------------------------
struct LOGFONTW
{
	LONG lfHeight;
	LONG lfWidth;
	LONG lfEscapement;
	LONG lfOrientation;
	LONG lfWeight;
	BYTE lfItalic;
	BYTE lfUnderline;
	BYTE lfStrikeOut;
	BYTE lfCharSet;
	BYTE lfOutPrecision;
	BYTE lfClipPrecision;
	BYTE lfQuality;
	BYTE lfPitchAndFamily;
	WCHAR lfFaceName[LF_FACESIZE];
};
typedef LOGFONTW LOGFONT;
typedef LOGFONTW* LPLOGFONTW;
typedef LOGFONTW* LPLOGFONT;
typedef LOGFONTW* PLOGFONT;
struct LOGFONTA
{
	LONG lfHeight;
	LONG lfWidth;
	LONG lfEscapement;
	LONG lfOrientation;
	LONG lfWeight;
	BYTE lfItalic;
	BYTE lfUnderline;
	BYTE lfStrikeOut;
	BYTE lfCharSet;
	BYTE lfOutPrecision;
	BYTE lfClipPrecision;
	BYTE lfQuality;
	BYTE lfPitchAndFamily;
	CHAR lfFaceName[LF_FACESIZE];
};
struct TEXTMETRICW
{
	LONG tmHeight;
	LONG tmAscent;
	LONG tmDescent;
	LONG tmInternalLeading;
	LONG tmExternalLeading;
	LONG tmAveCharWidth;
	LONG tmMaxCharWidth;
	LONG tmWeight;
	LONG tmOverhang;
	LONG tmDigitizedAspectX;
	LONG tmDigitizedAspectY;
	WCHAR tmFirstChar;
	WCHAR tmLastChar;
	WCHAR tmDefaultChar;
	WCHAR tmBreakChar;
	BYTE tmItalic;
	BYTE tmUnderlined;
	BYTE tmStruckOut;
	BYTE tmPitchAndFamily;
	BYTE tmCharSet;
};
typedef TEXTMETRICW TEXTMETRIC;
typedef TEXTMETRICW* LPTEXTMETRICW;
typedef TEXTMETRICW* LPTEXTMETRIC;
struct NONCLIENTMETRICSW
{
	UINT cbSize;
	int iBorderWidth;
	int iScrollWidth;
	int iScrollHeight;
	int iCaptionWidth;
	int iCaptionHeight;
	LOGFONTW lfCaptionFont;
	int iSmCaptionWidth;
	int iSmCaptionHeight;
	LOGFONTW lfSmCaptionFont;
	int iMenuWidth;
	int iMenuHeight;
	LOGFONTW lfMenuFont;
	LOGFONTW lfStatusFont;
	LOGFONTW lfMessageFont;
	int iPaddedBorderWidth;
};
typedef NONCLIENTMETRICSW NONCLIENTMETRICS;
struct LOGBRUSH
{
	UINT lbStyle;
	COLORREF lbColor;
	ULONG_PTR lbHatch;
};
struct BITMAP
{
	LONG bmType;
	LONG bmWidth;
	LONG bmHeight;
	LONG bmWidthBytes;
	WORD bmPlanes;
	WORD bmBitsPixel;
	LPVOID bmBits;
};
typedef BITMAP* LPBITMAP;
struct RGBQUAD
{
	BYTE rgbBlue;
	BYTE rgbGreen;
	BYTE rgbRed;
	BYTE rgbReserved;
};
struct BITMAPINFOHEADER
{
	DWORD biSize;
	LONG biWidth;
	LONG biHeight;
	WORD biPlanes;
	WORD biBitCount;
	DWORD biCompression;
	DWORD biSizeImage;
	LONG biXPelsPerMeter;
	LONG biYPelsPerMeter;
	DWORD biClrUsed;
	DWORD biClrImportant;
};
struct BITMAPINFO
{
	BITMAPINFOHEADER bmiHeader;
	RGBQUAD bmiColors[1];
};
typedef BITMAPINFO* LPBITMAPINFO;
#define BI_RGB 0L
#define DIB_RGB_COLORS 0
struct ICONINFO
{
	BOOL fIcon;
	DWORD xHotspot;
	DWORD yHotspot;
	HBITMAP hbmMask;
	HBITMAP hbmColor;
};
typedef ICONINFO* PICONINFO;

// ---- COM -----------------------------------------------------------------------------
struct GUID
{
	unsigned int Data1;
	unsigned short Data2;
	unsigned short Data3;
	unsigned char Data4[8];
};
typedef GUID IID;
typedef GUID CLSID;
typedef const GUID& REFGUID;
typedef const IID& REFIID;
typedef const CLSID& REFCLSID;
