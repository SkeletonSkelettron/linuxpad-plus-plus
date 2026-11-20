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

// Notepad++ Linux plugin SDK: themes of Windows (Qt widgets follow the palette).
#pragma once
#include <windows.h>
#ifndef RC_INVOKED
#define ETDT_DISABLE 0x00000001
#define ETDT_ENABLE 0x00000002
#define ETDT_USETABTEXTURE 0x00000004
#define ETDT_ENABLETAB (ETDT_ENABLE | ETDT_USETABTEXTURE)
inline HRESULT EnableThemeDialogTexture(HWND, DWORD) { return S_OK; }
inline HRESULT SetWindowTheme(HWND, LPCWSTR, LPCWSTR) { return S_OK; }
inline BOOL IsThemeActive() { return TRUE; }
inline BOOL IsAppThemed() { return TRUE; }
inline HTHEME OpenThemeData(HWND, LPCWSTR) { return nullptr; }
inline HRESULT CloseThemeData(HTHEME) { return S_OK; }
inline HRESULT DrawThemeBackground(HTHEME, HDC, int, int, LPCRECT, LPCRECT) { return E_NOTIMPL; }
inline HRESULT DrawThemeText(HTHEME, HDC, int, int, LPCWSTR, int, DWORD, DWORD, LPCRECT) { return E_NOTIMPL; }
inline HRESULT GetThemeColor(HTHEME, int, int, int, COLORREF*) { return E_NOTIMPL; }
inline HRESULT GetThemePartSize(HTHEME, HDC, int, int, LPCRECT, int, SIZE*) { return E_NOTIMPL; }
inline HRESULT DrawThemeParentBackground(HWND, HDC, const RECT*) { return E_NOTIMPL; }
inline BOOL IsThemeBackgroundPartiallyTransparent(HTHEME, int, int) { return FALSE; }
#endif
