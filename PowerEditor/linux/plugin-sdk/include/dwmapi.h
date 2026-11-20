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

// Notepad++ Linux plugin SDK: the desktop window manager of Windows (nothing to do here).
#pragma once
#include <windows.h>
#ifndef RC_INVOKED
inline HRESULT DwmSetWindowAttribute(HWND, DWORD, LPCVOID, DWORD) { return S_OK; }
inline HRESULT DwmGetWindowAttribute(HWND, DWORD, PVOID, DWORD) { return E_NOTIMPL; }
inline HRESULT DwmIsCompositionEnabled(BOOL* on) { if (on) *on = TRUE; return S_OK; }
#endif
