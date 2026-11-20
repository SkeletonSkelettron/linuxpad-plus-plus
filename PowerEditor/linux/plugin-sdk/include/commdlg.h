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

// Notepad++ Linux plugin SDK: the common dialogs (file, colour, font) on Qt dialogs.

#pragma once

#include <windows.h>

#ifndef RC_INVOKED
typedef UINT_PTR (CALLBACK* LPOFNHOOKPROC)(HWND, UINT, WPARAM, LPARAM);
struct OPENFILENAMEW
{
	DWORD lStructSize;
	HWND hwndOwner;
	HINSTANCE hInstance;
	LPCWSTR lpstrFilter;
	LPWSTR lpstrCustomFilter;
	DWORD nMaxCustFilter;
	DWORD nFilterIndex;
	LPWSTR lpstrFile;
	DWORD nMaxFile;
	LPWSTR lpstrFileTitle;
	DWORD nMaxFileTitle;
	LPCWSTR lpstrInitialDir;
	LPCWSTR lpstrTitle;
	DWORD Flags;
	WORD nFileOffset;
	WORD nFileExtension;
	LPCWSTR lpstrDefExt;
	LPARAM lCustData;
	LPOFNHOOKPROC lpfnHook;
	LPCWSTR lpTemplateName;
	void* pvReserved;
	DWORD dwReserved;
	DWORD FlagsEx;
};
typedef OPENFILENAMEW OPENFILENAME;
typedef OPENFILENAMEW* LPOPENFILENAMEW;
typedef OPENFILENAMEW* LPOPENFILENAME;
#define OPENFILENAME_SIZE_VERSION_400 sizeof(OPENFILENAMEW)
#define OFN_READONLY 0x00000001
#define OFN_OVERWRITEPROMPT 0x00000002
#define OFN_HIDEREADONLY 0x00000004
#define OFN_NOCHANGEDIR 0x00000008
#define OFN_SHOWHELP 0x00000010
#define OFN_ENABLEHOOK 0x00000020
#define OFN_NOVALIDATE 0x00000100
#define OFN_ALLOWMULTISELECT 0x00000200
#define OFN_EXTENSIONDIFFERENT 0x00000400
#define OFN_PATHMUSTEXIST 0x00000800
#define OFN_FILEMUSTEXIST 0x00001000
#define OFN_CREATEPROMPT 0x00002000
#define OFN_NOREADONLYRETURN 0x00008000
#define OFN_EXPLORER 0x00080000
#define OFN_NODEREFERENCELINKS 0x00100000
#define OFN_LONGNAMES 0x00200000
#define OFN_ENABLESIZING 0x00800000
#define OFN_DONTADDTORECENT 0x02000000
#define OFN_FORCESHOWHIDDEN 0x10000000
BOOL GetOpenFileNameW(LPOPENFILENAMEW ofn);
BOOL GetSaveFileNameW(LPOPENFILENAMEW ofn);
#define GetOpenFileName GetOpenFileNameW
#define GetSaveFileName GetSaveFileNameW
DWORD CommDlgExtendedError();

typedef UINT_PTR (CALLBACK* LPCCHOOKPROC)(HWND, UINT, WPARAM, LPARAM);
struct CHOOSECOLORW
{
	DWORD lStructSize;
	HWND hwndOwner;
	HWND hInstance;
	COLORREF rgbResult;
	COLORREF* lpCustColors;
	DWORD Flags;
	LPARAM lCustData;
	LPCCHOOKPROC lpfnHook;
	LPCWSTR lpTemplateName;
};
typedef CHOOSECOLORW CHOOSECOLOR;
typedef CHOOSECOLORW* LPCHOOSECOLORW;
typedef CHOOSECOLORW* LPCHOOSECOLOR;
#define CC_RGBINIT 0x00000001
#define CC_FULLOPEN 0x00000002
#define CC_PREVENTFULLOPEN 0x00000004
#define CC_SHOWHELP 0x00000008
#define CC_ENABLEHOOK 0x00000010
#define CC_SOLIDCOLOR 0x00000080
#define CC_ANYCOLOR 0x00000100
BOOL ChooseColorW(LPCHOOSECOLORW cc);
#define ChooseColor ChooseColorW

typedef UINT_PTR (CALLBACK* LPCFHOOKPROC)(HWND, UINT, WPARAM, LPARAM);
struct CHOOSEFONTW
{
	DWORD lStructSize;
	HWND hwndOwner;
	HDC hDC;
	LPLOGFONTW lpLogFont;
	INT iPointSize;
	DWORD Flags;
	COLORREF rgbColors;
	LPARAM lCustData;
	LPCFHOOKPROC lpfnHook;
	LPCWSTR lpTemplateName;
	HINSTANCE hInstance;
	LPWSTR lpszStyle;
	WORD nFontType;
	WORD ___MISSING_ALIGNMENT__;
	INT nSizeMin;
	INT nSizeMax;
};
typedef CHOOSEFONTW CHOOSEFONT;
typedef CHOOSEFONTW* LPCHOOSEFONTW;
typedef CHOOSEFONTW* LPCHOOSEFONT;
#define CF_SCREENFONTS 0x00000001
#define CF_INITTOLOGFONTSTRUCT 0x00000040
#define CF_EFFECTS 0x00000100
#define CF_FIXEDPITCHONLY 0x00004000
#define CF_NOVERTFONTS 0x01000000
#define CF_FORCEFONTEXIST 0x00010000
BOOL ChooseFontW(LPCHOOSEFONTW cf);
#define ChooseFont ChooseFontW
#endif
