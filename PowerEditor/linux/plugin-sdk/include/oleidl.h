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

// Notepad++ Linux plugin SDK: OLE drag and drop (IDropSource, IDropTarget).

#pragma once
#include <windows.h>
#include <objidl.h>

#ifndef RC_INVOKED
#define DROPEFFECT_NONE 0
#define DROPEFFECT_COPY 1
#define DROPEFFECT_MOVE 2
#define DROPEFFECT_LINK 4
#define DROPEFFECT_SCROLL 0x80000000
typedef DWORD DROPEFFECT;

#define DRAGDROP_S_DROP ((HRESULT)0x00040100L)
#define DRAGDROP_S_CANCEL ((HRESULT)0x00040101L)
#define DRAGDROP_S_USEDEFAULTCURSORS ((HRESULT)0x00040102L)
#define DRAGDROP_E_NOTREGISTERED ((HRESULT)0x80040100L)
#define DRAGDROP_E_ALREADYREGISTERED ((HRESULT)0x80040101L)
#define DRAGDROP_E_INVALIDHWND ((HRESULT)0x80040102L)

inline const IID IID_IDropSource = {0x00000121, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
inline const IID IID_IDropTarget = {0x00000122, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};

struct IDropSource : public IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE QueryContinueDrag(BOOL fEscapePressed, DWORD grfKeyState) = 0;
	virtual HRESULT STDMETHODCALLTYPE GiveFeedback(DWORD dwEffect) = 0;
	static REFIID iid() { return IID_IDropSource; }
};
typedef IDropSource* LPDROPSOURCE;

struct IDropTarget : public IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE DragEnter(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) = 0;
	virtual HRESULT STDMETHODCALLTYPE DragOver(DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) = 0;
	virtual HRESULT STDMETHODCALLTYPE DragLeave() = 0;
	virtual HRESULT STDMETHODCALLTYPE Drop(IDataObject* pDataObj, DWORD grfKeyState, POINTL pt, DWORD* pdwEffect) = 0;
	static REFIID iid() { return IID_IDropTarget; }
};
typedef IDropTarget* LPDROPTARGET;
#endif
