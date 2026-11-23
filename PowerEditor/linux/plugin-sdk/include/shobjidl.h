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

// Notepad++ Linux plugin SDK: the file dialogs of Windows Vista (IFileOpenDialog, IFileSaveDialog,
// IShellItem) made with QFileDialog, from CoCreateInstance(CLSID_FileOpenDialog / CLSID_FileSaveDialog).
#pragma once

#include <windows.h>
#include <unknwn.h>
#include <shtypes.h>
#include <shlobj.h>

inline const CLSID CLSID_FileOpenDialog = {0xDC1C5A9C, 0xE88A, 0x4DDE, {0xA5, 0xA1, 0x60, 0xF8, 0x2A, 0x20, 0xAE, 0xF7}};
inline const CLSID CLSID_FileSaveDialog = {0xC0B4E2F3, 0xBA21, 0x4773, {0x8D, 0xBA, 0x33, 0x5E, 0xC9, 0x46, 0xEB, 0x8B}};
inline const IID IID_IModalWindow = {0xB4DB1657, 0x70D7, 0x485E, {0x8E, 0x3E, 0x6F, 0xCB, 0x5A, 0x5C, 0x18, 0x02}};
inline const IID IID_IFileDialog = {0x42F85136, 0xDB7E, 0x439C, {0x85, 0xF1, 0xE4, 0x07, 0x5D, 0x13, 0x5F, 0xC8}};
inline const IID IID_IFileOpenDialog = {0xD57C7288, 0xD4AD, 0x4768, {0xBE, 0x02, 0x9D, 0x96, 0x95, 0x32, 0xD9, 0x60}};
inline const IID IID_IFileSaveDialog = {0x84BCCD23, 0x5FDE, 0x4CDB, {0xAE, 0xA4, 0xAF, 0x64, 0xB8, 0x3D, 0x78, 0xAB}};
inline const IID IID_IShellItem = {0x43826D1E, 0xE718, 0x42EE, {0xBC, 0x55, 0xA1, 0xE2, 0x61, 0xC3, 0x7B, 0xFE}};
inline const IID IID_IShellItemArray = {0xB63EA76D, 0x1F85, 0x456F, {0xA1, 0x9C, 0x48, 0x15, 0x9E, 0xFA, 0x85, 0x8B}};
inline const IID IID_IFileDialogEvents = {0x973510DB, 0x7D7F, 0x452B, {0x89, 0x75, 0x74, 0xA8, 0x58, 0x28, 0xD3, 0x54}};

typedef DWORD SFGAOF;
#ifndef SFGAO_FOLDER
#define SFGAO_FOLDER 0x20000000L
#define SFGAO_FILESYSTEM 0x40000000L
#define SFGAO_STREAM 0x00400000L
#endif

enum SIGDN : int
{
	SIGDN_NORMALDISPLAY = 0,
	SIGDN_PARENTRELATIVEPARSING = (int)0x80018001,
	SIGDN_DESKTOPABSOLUTEPARSING = (int)0x80028000,
	SIGDN_PARENTRELATIVEEDITING = (int)0x80031001,
	SIGDN_DESKTOPABSOLUTEEDITING = (int)0x8004C000,
	SIGDN_FILESYSPATH = (int)0x80058000,
	SIGDN_URL = (int)0x80068000,
	SIGDN_PARENTRELATIVEFORADDRESSBAR = (int)0x8007C001,
	SIGDN_PARENTRELATIVE = (int)0x80080001,
	SIGDN_PARENTRELATIVEFORUI = (int)0x80094001
};
typedef DWORD SICHINTF;
enum FDAP { FDAP_BOTTOM = 0, FDAP_TOP = 1 };

typedef DWORD FILEOPENDIALOGOPTIONS;
#define FOS_OVERWRITEPROMPT 0x00000002
#define FOS_STRICTFILETYPES 0x00000004
#define FOS_NOCHANGEDIR 0x00000008
#define FOS_PICKFOLDERS 0x00000020
#define FOS_FORCEFILESYSTEM 0x00000040
#define FOS_ALLNONSTORAGEITEMS 0x00000080
#define FOS_NOVALIDATE 0x00000100
#define FOS_ALLOWMULTISELECT 0x00000200
#define FOS_PATHMUSTEXIST 0x00000800
#define FOS_FILEMUSTEXIST 0x00001000
#define FOS_CREATEPROMPT 0x00002000
#define FOS_SHAREAWARE 0x00004000
#define FOS_NOREADONLYRETURN 0x00008000
#define FOS_NOTESTFILECREATE 0x00010000
#define FOS_HIDEMRUPLACES 0x00020000
#define FOS_HIDEPINNEDPLACES 0x00040000
#define FOS_NODEREFERENCELINKS 0x00100000
#define FOS_OKBUTTONNEEDSINTERACTION 0x00200000
#define FOS_DONTADDTORECENT 0x02000000
#define FOS_FORCESHOWHIDDEN 0x10000000
#define FOS_DEFAULTNOMINIMODE 0x20000000
#define FOS_FORCEPREVIEWPANEON 0x40000000
#define FOS_SUPPORTSTREAMABLEITEMS 0x80000000


struct IShellItem : IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE BindToHandler(void* bindContext, REFGUID handler, REFIID iid, void** out) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetParent(IShellItem** parent) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetDisplayName(SIGDN form, LPWSTR* name) = 0; // free name with CoTaskMemFree
	virtual HRESULT STDMETHODCALLTYPE GetAttributes(SFGAOF mask, SFGAOF* attributes) = 0;
	virtual HRESULT STDMETHODCALLTYPE Compare(IShellItem* other, SICHINTF hint, int* order) = 0;
	static REFIID iid() { return IID_IShellItem; }
};

struct IShellItemArray : IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE BindToHandler(void* bindContext, REFGUID handler, REFIID iid, void** out) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetPropertyStore(int flags, REFIID iid, void** out) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetPropertyDescriptionList(const void* key, REFIID iid, void** out) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetAttributes(int attribFlags, SFGAOF mask, SFGAOF* attributes) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetCount(DWORD* count) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetItemAt(DWORD index, IShellItem** item) = 0;
	virtual HRESULT STDMETHODCALLTYPE EnumItems(void** items) = 0;
	static REFIID iid() { return IID_IShellItemArray; }
};

struct IFileDialog;
struct IFileDialogEvents : IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE OnFileOk(IFileDialog* dialog) = 0;
	virtual HRESULT STDMETHODCALLTYPE OnFolderChanging(IFileDialog* dialog, IShellItem* folder) = 0;
	virtual HRESULT STDMETHODCALLTYPE OnFolderChange(IFileDialog* dialog) = 0;
	virtual HRESULT STDMETHODCALLTYPE OnSelectionChange(IFileDialog* dialog) = 0;
	virtual HRESULT STDMETHODCALLTYPE OnShareViolation(IFileDialog* dialog, IShellItem* item, int* response) = 0;
	virtual HRESULT STDMETHODCALLTYPE OnTypeChange(IFileDialog* dialog) = 0;
	virtual HRESULT STDMETHODCALLTYPE OnOverwrite(IFileDialog* dialog, IShellItem* item, int* response) = 0;
	static REFIID iid() { return IID_IFileDialogEvents; }
};

struct IModalWindow : IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE Show(HWND owner) = 0; // HRESULT_FROM_WIN32(ERROR_CANCELLED) when cancelled
	static REFIID iid() { return IID_IModalWindow; }
};

struct IFileDialog : IModalWindow
{
	virtual HRESULT STDMETHODCALLTYPE SetFileTypes(UINT count, const COMDLG_FILTERSPEC* types) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetFileTypeIndex(UINT index) = 0; // 1 based
	virtual HRESULT STDMETHODCALLTYPE GetFileTypeIndex(UINT* index) = 0;
	virtual HRESULT STDMETHODCALLTYPE Advise(IFileDialogEvents* events, DWORD* cookie) = 0;
	virtual HRESULT STDMETHODCALLTYPE Unadvise(DWORD cookie) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetOptions(FILEOPENDIALOGOPTIONS options) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetOptions(FILEOPENDIALOGOPTIONS* options) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetDefaultFolder(IShellItem* folder) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetFolder(IShellItem* folder) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetFolder(IShellItem** folder) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetCurrentSelection(IShellItem** item) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetFileName(LPCWSTR name) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetFileName(LPWSTR* name) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetTitle(LPCWSTR title) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetOkButtonLabel(LPCWSTR text) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetFileNameLabel(LPCWSTR text) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetResult(IShellItem** item) = 0;
	virtual HRESULT STDMETHODCALLTYPE AddPlace(IShellItem* item, FDAP where) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetDefaultExtension(LPCWSTR extension) = 0;
	virtual HRESULT STDMETHODCALLTYPE Close(HRESULT result) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetClientGuid(REFGUID guid) = 0;
	virtual HRESULT STDMETHODCALLTYPE ClearClientData() = 0;
	virtual HRESULT STDMETHODCALLTYPE SetFilter(void* filter) = 0;
	static REFIID iid() { return IID_IFileDialog; }
};

struct IFileOpenDialog : IFileDialog
{
	virtual HRESULT STDMETHODCALLTYPE GetResults(IShellItemArray** items) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetSelectedItems(IShellItemArray** items) = 0;
	static REFIID iid() { return IID_IFileOpenDialog; }
};

struct IFileSaveDialog : IFileDialog
{
	virtual HRESULT STDMETHODCALLTYPE SetSaveAsItem(IShellItem* item) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetProperties(void* store) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetCollectedProperties(void* list, BOOL appendDefault) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetProperties(void** store) = 0;
	virtual HRESULT STDMETHODCALLTYPE ApplyProperties(IShellItem* item, void* store, HWND owner, void* sink) = 0;
	static REFIID iid() { return IID_IFileSaveDialog; }
};

HRESULT SHCreateItemFromParsingName(PCWSTR path, void* bindContext, REFIID iid, void** out);
