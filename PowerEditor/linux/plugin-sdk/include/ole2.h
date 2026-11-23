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

// Notepad++ Linux plugin SDK: OLE functions (Win32/Ole.cpp).

#pragma once
#include <windows.h>
#include <objidl.h>
#include <oleidl.h>

#ifndef RC_INVOKED
HRESULT OleInitialize(LPVOID reserved);
void OleUninitialize();
HRESULT RegisterDragDrop(HWND hwnd, IDropTarget* target);
HRESULT RevokeDragDrop(HWND hwnd);
HRESULT DoDragDrop(IDataObject* data, IDropSource* source, DWORD okEffects, DWORD* effect);
HRESULT OleSetClipboard(IDataObject* data);
HRESULT OleGetClipboard(IDataObject** data);
HRESULT OleFlushClipboard();
HRESULT OleIsCurrentClipboard(IDataObject* data);
#endif
