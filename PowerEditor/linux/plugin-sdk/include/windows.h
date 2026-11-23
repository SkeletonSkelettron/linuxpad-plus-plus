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

// Notepad++ Linux plugin SDK: <windows.h> for Linux.
//
// The plugin interface of Notepad++ (PluginInterface.h, Notepad_plus_msgs.h, Docking.h,
// menuCmdID.h, and the copies plugins carry of them) compiles unchanged with it.
// Window handles are NppHandle objects: SendMessage(nppData._nppHandle, NPPM_..., w, l)
// and SendMessage(scintillaHandle, SCI_..., w, l) work as on Windows.
// Strings are wchar_t, which is UTF-32 on Linux.
//
// The rest of the Win32 API that plugins use (dialogs of .rc files, controls, menus,
// GDI, files, INI files, threads, clipboard...) is in npp_win32/w32_api.h, implemented
// with Qt and POSIX by the npp_win32 library. NppPluginQt.h has helpers for plugins
// that write their user interface with Qt directly.

#pragma once

#ifndef _WIN32

#include "npp_win32/w32_defs.h"

#ifndef RC_INVOKED

// standard headers that use swprintf & co, before the Microsoft names are defined
#include <algorithm>
#include <climits>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <functional>
#include <iostream>
#include <locale>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <vector>
#include <strings.h>
#include <unistd.h>

#include "npp_win32/w32_types.h"
#include "npp_win32/w32_api.h"

// Messages of the Linux host that have no NPPM_ counterpart.
#define NPPM_LINUX_FIRST          (WM_USER + 6000)
#define NPPM_LINUX_ENABLEMENUITEM (NPPM_LINUX_FIRST + 1) // wParam: command id, lParam: TRUE to enable
#define NPPM_LINUX_ISMENUITEMCHECKED (NPPM_LINUX_FIRST + 2) // wParam: command id
#define NPPM_LINUX_MESSAGEBOX     (NPPM_LINUX_FIRST + 3) // wParam: NppMessageBoxArgs*
#define NPPM_LINUX_SETMENUITEMTEXT (NPPM_LINUX_FIRST + 4) // wParam: command id, lParam: const wchar_t*
#define NPPM_LINUX_SETSCINTILLANOTIFY (NPPM_LINUX_FIRST + 5) // wParam: Scintilla handle (NPPM_CREATESCINTILLAHANDLE), lParam: NppScintillaNotify*
#define NPPM_LINUX_PLUGINWIDGETPARENT (NPPM_LINUX_FIRST + 6) // returns the QWidget* to parent plugin dialogs to
// wParam: buffer ID, lParam: NppBufferText* (or 0 to query length).
// Copies UTF-8 bytes when capacity is sufficient. Returns length, or -1 for an invalid buffer.
#define NPPM_LINUX_GETBUFFERTEXT   (NPPM_LINUX_FIRST + 7)
// wParam: NppWndHook*, lParam: TRUE to add, FALSE to remove. The messages of the main window
// (WM_COMMAND of the menus, the NPPM_ messages of plugins) go to hook first: SetWindowLongPtr
// (GWLP_WNDPROC) and SetWindowSubclass of nppData._nppHandle use it.
#define NPPM_LINUX_SUBCLASSNPP     (NPPM_LINUX_FIRST + 8)
// wParam: buffer ID from NPPN_FILEBEFORESAVE. lParam: NppSaveFilter callback.
// Invokes the callback synchronously for a writable buffer. The editor and
// target path are borrowed only during that call, including for background tabs.
#define NPPM_LINUX_FILTERBEFORESAVE (NPPM_LINUX_FIRST + 9)
// wParam: NppWndMessage*. CallWindowProc of the procedure that a subclass of nppData._nppHandle
// replaced: the message goes on to the next subclass, then to Notepad++. Every other message to
// the main window starts again at the newest subclass, like SendMessage on Windows.
#define NPPM_LINUX_CALLNEXT        (NPPM_LINUX_FIRST + 10)

struct NppWndMessage
{
	unsigned int msg;
	uintptr_t wParam;
	intptr_t lParam;
};

struct NppSaveFormat
{
	enum Encoding { Legacy = 0, Utf8Bom, Utf16BigEndian, Utf16LittleEndian, Utf8 };
	int eol;
	int encoding;
	int codepage;
};

using NppSaveFilter = void (*)(HWND editor, const wchar_t* targetPath, NppSaveFormat* format);

struct NppWndHook
{
	intptr_t (*dispatch)(void* context, unsigned int msg, uintptr_t wParam, intptr_t lParam);
	void* context;
};

struct NppBufferText
{
	char* data;
	size_t capacity;
};

struct NppMessageBoxArgs
{
	HWND parent;
	const wchar_t* text;
	const wchar_t* caption;
	UINT type;
};

struct SCNotification;
// The notifications of a Scintilla made with NPPM_CREATESCINTILLAHANDLE (WM_NOTIFY to
// its parent on Windows) are also given to callback(context, notification).
struct NppScintillaNotify
{
	void (*callback)(void* context, SCNotification* notification);
	void* context;
};

// like <windows.h> of Windows without WIN32_LEAN_AND_MEAN
#ifndef WIN32_LEAN_AND_MEAN
#include <shellapi.h>
#include <commdlg.h>
#endif

#else
// like winresrc.h of Windows: the styles of the common controls
#include <commctrl.h>
#endif // RC_INVOKED
#endif // !_WIN32
