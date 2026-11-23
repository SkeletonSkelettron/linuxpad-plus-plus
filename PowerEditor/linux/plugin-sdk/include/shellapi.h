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

// Notepad++ Linux plugin SDK: ShellExecute and file icons.

#pragma once

#include <windows.h>

#ifndef RC_INVOKED
HINSTANCE ShellExecuteW(HWND hWnd, LPCWSTR operation, LPCWSTR file, LPCWSTR parameters, LPCWSTR directory, INT show);
HINSTANCE ShellExecuteA(HWND hWnd, LPCSTR operation, LPCSTR file, LPCSTR parameters, LPCSTR directory, INT show);
#define ShellExecute ShellExecuteW
struct SHELLEXECUTEINFOW
{
	DWORD cbSize;
	ULONG fMask;
	HWND hwnd;
	LPCWSTR lpVerb;
	LPCWSTR lpFile;
	LPCWSTR lpParameters;
	LPCWSTR lpDirectory;
	int nShow;
	HINSTANCE hInstApp;
	void* lpIDList;
	LPCWSTR lpClass;
	HKEY hkeyClass;
	DWORD dwHotKey;
	HANDLE hIcon;
	HANDLE hProcess;
};
typedef SHELLEXECUTEINFOW SHELLEXECUTEINFO;
typedef SHELLEXECUTEINFOW* LPSHELLEXECUTEINFOW;
typedef SHELLEXECUTEINFOW* LPSHELLEXECUTEINFO;
BOOL ShellExecuteExW(SHELLEXECUTEINFOW* info);
#define ShellExecuteEx ShellExecuteExW
#define SEE_MASK_NOCLOSEPROCESS 0x00000040
#define SEE_MASK_FLAG_NO_UI 0x00000400
#define SEE_MASK_INVOKEIDLIST 0x0000000C
#define SE_ERR_FNF 2
#define SE_ERR_PNF 3
#define SE_ERR_ACCESSDENIED 5
#define SE_ERR_OOM 8
#define SE_ERR_SHARE 26
#define SE_ERR_ASSOCINCOMPLETE 27
#define SE_ERR_DDETIMEOUT 28
#define SE_ERR_DDEFAIL 29
#define SE_ERR_DDEBUSY 30
#define SE_ERR_DLLNOTFOUND 32
#define SE_ERR_NOASSOC 31

struct SHFILEINFOW
{
	HICON hIcon;
	int iIcon;
	DWORD dwAttributes;
	WCHAR szDisplayName[MAX_PATH];
	WCHAR szTypeName[80];
};
typedef SHFILEINFOW SHFILEINFO;
DWORD_PTR SHGetFileInfoW(LPCWSTR path, DWORD attributes, SHFILEINFOW* info, UINT size, UINT flags);
#define SHGetFileInfo SHGetFileInfoW
#define SHGFI_ICON 0x000000100
#define SHGFI_DISPLAYNAME 0x000000200
#define SHGFI_TYPENAME 0x000000400
#define SHGFI_ATTRIBUTES 0x000000800
#define SHGFI_SYSICONINDEX 0x000004000
#define SHGFI_LARGEICON 0x000000000
#define SHGFI_SMALLICON 0x000000001
#define SHGFI_OPENICON 0x000000002
#define SHGFI_USEFILEATTRIBUTES 0x000000010
#define SHGFI_OVERLAYINDEX 0x000000040

struct SHFILEOPSTRUCTW
{
	HWND hwnd;
	UINT wFunc;
	LPCWSTR pFrom;
	LPCWSTR pTo;
	WORD fFlags;
	BOOL fAnyOperationsAborted;
	LPVOID hNameMappings;
	LPCWSTR lpszProgressTitle;
};
typedef SHFILEOPSTRUCTW SHFILEOPSTRUCT;
int SHFileOperationW(SHFILEOPSTRUCTW* op);
#define SHFileOperation SHFileOperationW
#define FO_MOVE 0x0001
#define FO_COPY 0x0002
#define FO_DELETE 0x0003
#define FO_RENAME 0x0004
#define FOF_SILENT 0x0004
#define FOF_RENAMEONCOLLISION 0x0008
#define FOF_MULTIDESTFILES 0x0001
#define FOF_NOCONFIRMATION 0x0010
#define FOF_ALLOWUNDO 0x0040
#define FOF_FILESONLY 0x0080
#define FOF_NOCONFIRMMKDIR 0x0200
#define FOF_NOERRORUI 0x0400
#define FOF_NO_UI (FOF_SILENT | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_NOCONFIRMMKDIR)
LPWSTR* CommandLineToArgvW(LPCWSTR cmdLine, int* argc);
#endif
