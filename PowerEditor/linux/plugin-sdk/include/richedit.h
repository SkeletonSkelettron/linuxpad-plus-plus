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

// Notepad++ Linux plugin SDK: rich edit controls are plain text edit boxes here.
#pragma once
#include <windows.h>
#ifdef RC_INVOKED
#define RICHEDIT_CLASS "RichEdit20W"
#define MSFTEDIT_CLASS "RICHEDIT50W"
#else
#define RICHEDIT_CLASS L"RichEdit20W"
#define RICHEDIT_CLASSW L"RichEdit20W"
#define MSFTEDIT_CLASS L"RICHEDIT50W"
#define EM_SETBKGNDCOLOR (WM_USER + 67)
#define EM_SETCHARFORMAT (WM_USER + 68)
#define EM_SETEVENTMASK (WM_USER + 69)
#define EM_EXLIMITTEXT (WM_USER + 53)
#define EM_EXSETSEL (WM_USER + 55)
#define EM_EXGETSEL (WM_USER + 52)
#define EM_SETTEXTEX (WM_USER + 97)
#define EM_GETTEXTEX (WM_USER + 94)
#define EM_GETTEXTLENGTHEX (WM_USER + 95)
#define EM_AUTOURLDETECT (WM_USER + 91)
#define ENM_CHANGE 0x00000001
#define ENM_SELCHANGE 0x00080000
#define ENM_LINK 0x04000000
#define SCF_SELECTION 0x0001
#define SCF_ALL 0x0004
#define SCF_DEFAULT 0x0000
#define CFM_COLOR 0x40000000
#define CFM_BOLD 0x00000001
#define CFE_BOLD 0x0001
#define CFE_AUTOCOLOR 0x40000000
struct CHARRANGE { LONG cpMin; LONG cpMax; };
struct CHARFORMATW { UINT cbSize; DWORD dwMask; DWORD dwEffects; LONG yHeight; LONG yOffset; COLORREF crTextColor; BYTE bCharSet; BYTE bPitchAndFamily; WCHAR szFaceName[LF_FACESIZE]; };
typedef CHARFORMATW CHARFORMAT;
struct CHARFORMAT2W : CHARFORMATW { WORD wWeight; SHORT sSpacing; COLORREF crBackColor; LCID lcid; DWORD dwReserved; SHORT sStyle; WORD wKerning; BYTE bUnderlineType; BYTE bAnimation; BYTE bRevAuthor; BYTE bUnderlineColor; };
typedef CHARFORMAT2W CHARFORMAT2;
#endif
