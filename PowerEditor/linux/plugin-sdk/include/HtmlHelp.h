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

// Notepad++ Linux plugin SDK: HtmlHelp.h. There is no viewer of compiled HTML help (.chm) here:
// HtmlHelp opens nothing and returns NULL, so plugins fall back to their other help.
#pragma once

#include <windows.h>

#define HH_DISPLAY_TOPIC 0x0000
#define HH_HELP_FINDER 0x0000
#define HH_DISPLAY_TOC 0x0001
#define HH_DISPLAY_INDEX 0x0002
#define HH_DISPLAY_SEARCH 0x0003
#define HH_KEYWORD_LOOKUP 0x000D
#define HH_DISPLAY_TEXT_POPUP 0x000E
#define HH_HELP_CONTEXT 0x000F
#define HH_CLOSE_ALL 0x0012
#define HH_INITIALIZE 0x001C
#define HH_UNINITIALIZE 0x001D

inline HWND HtmlHelpW(HWND, LPCWSTR, UINT, DWORD_PTR) { return nullptr; }
inline HWND HtmlHelpA(HWND, LPCSTR, UINT, DWORD_PTR) { return nullptr; }
#define HtmlHelp HtmlHelpW
