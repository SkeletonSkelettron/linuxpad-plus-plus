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

// Notepad++ Linux plugin SDK: special folders (XDG folders of Linux) and the folder dialog.

#pragma once

#include <windows.h>
#include <shellapi.h>
#include <commctrl.h> // like shlobj_core.h of Windows
#include <ole2.h>

#ifndef RC_INVOKED
#define CSIDL_DESKTOP 0x0000
#define CSIDL_PROGRAMS 0x0002
#define CSIDL_PERSONAL 0x0005
#define CSIDL_MYDOCUMENTS CSIDL_PERSONAL
#define CSIDL_STARTUP 0x0007
#define CSIDL_DESKTOPDIRECTORY 0x0010
#define CSIDL_FONTS 0x0014
#define CSIDL_APPDATA 0x001a
#define CSIDL_LOCAL_APPDATA 0x001c
#define CSIDL_COMMON_APPDATA 0x0023
#define CSIDL_WINDOWS 0x0024
#define CSIDL_SYSTEM 0x0025
#define CSIDL_PROGRAM_FILES 0x0026
#define CSIDL_MYPICTURES 0x0027
#define CSIDL_PROFILE 0x0028
#define CSIDL_PROGRAM_FILESX86 0x002a
#define CSIDL_COMMON_DOCUMENTS 0x002e
#define CSIDL_FLAG_CREATE 0x8000
#define SHGFP_TYPE_CURRENT 0
HRESULT SHGetFolderPathW(HWND hWnd, int csidl, HANDLE token, DWORD flags, LPWSTR path);
HRESULT SHGetMalloc(LPMALLOC* malloc); // frees the PIDLs of SHBrowseForFolder
BOOL SHGetSpecialFolderPathW(HWND hWnd, LPWSTR path, int csidl, BOOL create);
#define SHGetFolderPath SHGetFolderPathW
#define SHGetSpecialFolderPath SHGetSpecialFolderPathW
typedef GUID KNOWNFOLDERID;
typedef const GUID& REFKNOWNFOLDERID;
extern const GUID FOLDERID_RoamingAppData;
extern const GUID FOLDERID_LocalAppData;
extern const GUID FOLDERID_Documents;
extern const GUID FOLDERID_Desktop;
extern const GUID FOLDERID_ProgramFiles;
extern const GUID FOLDERID_Profile;
extern const GUID FOLDERID_Downloads;
HRESULT SHGetKnownFolderPath(REFKNOWNFOLDERID id, DWORD flags, HANDLE token, PWSTR* path);
int SHCreateDirectoryExW(HWND hWnd, LPCWSTR path, const SECURITY_ATTRIBUTES* sa);
#define SHCreateDirectoryEx SHCreateDirectoryExW
int SHCreateDirectory(HWND hWnd, LPCWSTR path);
void SHChangeNotify(LONG eventId, UINT flags, LPCVOID item1, LPCVOID item2);
#define SHCNE_ASSOCCHANGED 0x08000000L
#define SHCNF_IDLIST 0x0000

struct ITEMIDLIST;
typedef ITEMIDLIST* LPITEMIDLIST;
typedef const ITEMIDLIST* LPCITEMIDLIST;
typedef ITEMIDLIST* PIDLIST_ABSOLUTE;
typedef const ITEMIDLIST* PCIDLIST_ABSOLUTE;
typedef int (CALLBACK* BFFCALLBACK)(HWND hWnd, UINT msg, LPARAM lParam, LPARAM data);
struct BROWSEINFOW
{
	HWND hwndOwner;
	LPCITEMIDLIST pidlRoot;
	LPWSTR pszDisplayName;
	LPCWSTR lpszTitle;
	UINT ulFlags;
	BFFCALLBACK lpfn;
	LPARAM lParam;
	int iImage;
};
typedef BROWSEINFOW BROWSEINFO;
typedef BROWSEINFOW* LPBROWSEINFOW;
typedef BROWSEINFOW* LPBROWSEINFO;
#define BIF_RETURNONLYFSDIRS 0x00000001
#define BIF_DONTGOBELOWDOMAIN 0x00000002
#define BIF_STATUSTEXT 0x00000004
#define BIF_EDITBOX 0x00000010
#define BIF_VALIDATE 0x00000020
#define BIF_NEWDIALOGSTYLE 0x00000040
#define BIF_USENEWUI (BIF_NEWDIALOGSTYLE | BIF_EDITBOX)
#define BIF_NONEWFOLDERBUTTON 0x00000200
#define BIF_BROWSEINCLUDEFILES 0x00004000
#define BFFM_INITIALIZED 1
#define BFFM_SELCHANGED 2
#define BFFM_SETSELECTIONW (WM_USER + 103)
#define BFFM_SETSELECTION BFFM_SETSELECTIONW
#define BFFM_SETSTATUSTEXTW (WM_USER + 104)
#define BFFM_SETSTATUSTEXT BFFM_SETSTATUSTEXTW
LPITEMIDLIST SHBrowseForFolderW(LPBROWSEINFOW bi);
BOOL SHGetPathFromIDListW(LPCITEMIDLIST pidl, LPWSTR path);
void ILFree(LPITEMIDLIST pidl);
HRESULT SHILCreateFromPath(PCWSTR path, LPITEMIDLIST* pidl, DWORD* attributes);
HRESULT SHGetFolderLocation(HWND hWnd, int csidl, HANDLE token, DWORD reserved, LPITEMIDLIST* pidl);
HRESULT SHParseDisplayName(PCWSTR name, void* bc, LPITEMIDLIST* pidl, ULONG sfgaoIn, ULONG* sfgaoOut);
HRESULT SHOpenFolderAndSelectItems(LPCITEMIDLIST folder, UINT count, LPCITEMIDLIST* items, DWORD flags);
#define SHBrowseForFolder SHBrowseForFolderW
#define SHGetPathFromIDList SHGetPathFromIDListW

// files of drag and drop and of the clipboard (CF_HDROP): a DROPFILES then the paths, each ended by a nul, and a nul
struct DROPFILES
{
	DWORD pFiles; // offset of the paths
	POINT pt;
	BOOL fNC;
	BOOL fWide;
};
typedef DROPFILES* LPDROPFILES;
#define CFSTR_PREFERREDDROPEFFECT L"Preferred DropEffect"
#define CFSTR_PERFORMEDDROPEFFECT L"Performed DropEffect"
#define CFSTR_PASTESUCCEEDED L"Paste Succeeded"
#define CFSTR_SHELLIDLIST L"Shell IDList Array"
#define CFSTR_FILENAMEW L"FileNameW"
#define CFSTR_FILENAME CFSTR_FILENAMEW

// the drag images of the shell: none here (CoCreateInstance(CLSID_DragDropHelper) fails)
inline const CLSID CLSID_DragDropHelper = {0x4657278A, 0x411B, 0x11D2, {0x83, 0x9A, 0x00, 0xC0, 0x4F, 0xD9, 0x18, 0xD0}};
inline const IID IID_IDropTargetHelper = {0x4657278B, 0x411B, 0x11D2, {0x83, 0x9A, 0x00, 0xC0, 0x4F, 0xD9, 0x18, 0xD0}};
inline const IID IID_IDragSourceHelper = {0xDE5BF786, 0x477A, 0x11D2, {0x83, 0x9D, 0x00, 0xC0, 0x4F, 0xD9, 0x18, 0xD0}};
struct SHDRAGIMAGE
{
	SIZE sizeDragImage;
	POINT ptOffset;
	HBITMAP hbmpDragImage;
	COLORREF crColorKey;
};
typedef SHDRAGIMAGE* LPSHDRAGIMAGE;
struct IDropTargetHelper : public IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE DragEnter(HWND hwndTarget, IDataObject* pDataObject, POINT* ppt, DWORD dwEffect) = 0;
	virtual HRESULT STDMETHODCALLTYPE DragLeave() = 0;
	virtual HRESULT STDMETHODCALLTYPE DragOver(POINT* ppt, DWORD dwEffect) = 0;
	virtual HRESULT STDMETHODCALLTYPE Drop(IDataObject* pDataObject, POINT* ppt, DWORD dwEffect) = 0;
	virtual HRESULT STDMETHODCALLTYPE Show(BOOL fShow) = 0;
	static REFIID iid() { return IID_IDropTargetHelper; }
};
struct IDragSourceHelper : public IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE InitializeFromBitmap(LPSHDRAGIMAGE pshdi, IDataObject* pDataObject) = 0;
	virtual HRESULT STDMETHODCALLTYPE InitializeFromWindow(HWND hwnd, POINT* ppt, IDataObject* pDataObject) = 0;
	static REFIID iid() { return IID_IDragSourceHelper; }
};
#endif

// like shlobj_core.h of Windows: IShellItem, IFileDialog...
#include <shobjidl.h>
