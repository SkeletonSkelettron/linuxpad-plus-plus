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

// Notepad++ Linux plugin SDK: rich edit controls (RichEdit20W, RICHEDIT50W) are QPlainTextEdit
// boxes with character formats (colors, bold, italic, underline, font, size), selections and
// positions of RichEdit (one character per line end), find, text ranges and the notifications
// EN_MSGFILTER, EN_SELCHANGE and EN_CHANGE of the event mask. No OLE objects, no RTF streams.
#pragma once
#include <windows.h>

// styles (also for .rc files)
#define ES_SAVESEL 0x00008000
#define ES_SUNKEN 0x00004000
#define ES_DISABLENOSCROLL 0x00002000
#define ES_SELECTIONBAR 0x01000000
#define ES_NOOLEDRAGDROP 0x00000008
#define ES_EX_NOCALLOLEINIT 0x00000000
#define ES_VERTICAL 0x00400000
#define ES_NOIME 0x00080000
#define ES_SELFIME 0x00040000
#ifdef RC_INVOKED
#define RICHEDIT_CLASS "RichEdit20W"
#define MSFTEDIT_CLASS "RICHEDIT50W"
#else
#define RICHEDIT_CLASS L"RichEdit20W"
#define RICHEDIT_CLASSW L"RichEdit20W"
#define RICHEDIT_CLASSA "RichEdit20A"
#define MSFTEDIT_CLASS L"RICHEDIT50W"

#define EM_CANPASTE (WM_USER + 50)
#define EM_DISPLAYBAND (WM_USER + 51)
#define EM_EXGETSEL (WM_USER + 52)
#define EM_EXLIMITTEXT (WM_USER + 53)
#define EM_EXLINEFROMCHAR (WM_USER + 54)
#define EM_EXSETSEL (WM_USER + 55)
#define EM_FINDTEXT (WM_USER + 56)
#define EM_FORMATRANGE (WM_USER + 57)
#define EM_GETCHARFORMAT (WM_USER + 58)
#define EM_GETEVENTMASK (WM_USER + 59)
#define EM_GETOLEINTERFACE (WM_USER + 60)
#define EM_GETPARAFORMAT (WM_USER + 61)
#define EM_GETSELTEXT (WM_USER + 62)
#define EM_HIDESELECTION (WM_USER + 63)
#define EM_PASTESPECIAL (WM_USER + 64)
#define EM_REQUESTRESIZE (WM_USER + 65)
#define EM_SELECTIONTYPE (WM_USER + 66)
#define EM_SETBKGNDCOLOR (WM_USER + 67)
#define EM_SETCHARFORMAT (WM_USER + 68)
#define EM_SETEVENTMASK (WM_USER + 69)
#define EM_SETOLECALLBACK (WM_USER + 70)
#define EM_SETPARAFORMAT (WM_USER + 71)
#define EM_SETTARGETDEVICE (WM_USER + 72)
#define EM_STREAMIN (WM_USER + 73)
#define EM_STREAMOUT (WM_USER + 74)
#define EM_GETTEXTRANGE (WM_USER + 75)
#define EM_FINDWORDBREAK (WM_USER + 76)
#define EM_SETOPTIONS (WM_USER + 77)
#define EM_GETOPTIONS (WM_USER + 78)
#define EM_FINDTEXTEX (WM_USER + 79)
#define EM_SETUNDOLIMIT (WM_USER + 82)
#define EM_REDO (WM_USER + 84)
#define EM_CANREDO (WM_USER + 85)
#define EM_STOPGROUPTYPING (WM_USER + 88)
#define EM_SETTEXTMODE (WM_USER + 89)
#define EM_GETTEXTMODE (WM_USER + 90)
#define EM_AUTOURLDETECT (WM_USER + 91)
#define EM_GETAUTOURLDETECT (WM_USER + 92)
#define EM_GETTEXTEX (WM_USER + 94)
#define EM_GETTEXTLENGTHEX (WM_USER + 95)
#define EM_SHOWSCROLLBAR (WM_USER + 96)
#define EM_SETTEXTEX (WM_USER + 97)
#define EM_SETWORDWRAPMODE (WM_USER + 102)
#define EM_GETWORDWRAPMODE (WM_USER + 103)
#define EM_SETLANGOPTIONS (WM_USER + 120)
#define EM_GETLANGOPTIONS (WM_USER + 121)
#define EM_FINDTEXTW (WM_USER + 123)
#define EM_FINDTEXTEXW (WM_USER + 124)
#define EM_SETEDITSTYLE (WM_USER + 204)
#define EM_GETEDITSTYLE (WM_USER + 205)
#define EM_GETSCROLLPOS (WM_USER + 221)
#define EM_SETSCROLLPOS (WM_USER + 222)
#define EM_SETFONTSIZE (WM_USER + 223)
#define EM_GETZOOM (WM_USER + 224)
#define EM_SETZOOM (WM_USER + 225)

#define EN_MSGFILTER 0x0700
#define EN_REQUESTRESIZE 0x0701
#define EN_SELCHANGE 0x0702
#define EN_DROPFILES 0x0703
#define EN_PROTECTED 0x0704
#define EN_LINK 0x070b

#define ENM_NONE 0x00000000
#define ENM_CHANGE 0x00000001
#define ENM_UPDATE 0x00000002
#define ENM_SCROLL 0x00000004
#define ENM_SCROLLEVENTS 0x00000008
#define ENM_KEYEVENTS 0x00010000
#define ENM_MOUSEEVENTS 0x00020000
#define ENM_REQUESTRESIZE 0x00040000
#define ENM_SELCHANGE 0x00080000
#define ENM_DROPFILES 0x00100000
#define ENM_PROTECTED 0x00200000
#define ENM_LINK 0x04000000

#define CFM_BOLD 0x00000001
#define CFM_ITALIC 0x00000002
#define CFM_UNDERLINE 0x00000004
#define CFM_STRIKEOUT 0x00000008
#define CFM_PROTECTED 0x00000010
#define CFM_LINK 0x00000020
#define CFM_SIZE 0x80000000
#define CFM_COLOR 0x40000000
#define CFM_FACE 0x20000000
#define CFM_OFFSET 0x10000000
#define CFM_CHARSET 0x08000000
#define CFM_BACKCOLOR 0x04000000
#define CFM_WEIGHT 0x00400000
#define CFE_BOLD 0x00000001
#define CFE_ITALIC 0x00000002
#define CFE_UNDERLINE 0x00000004
#define CFE_STRIKEOUT 0x00000008
#define CFE_PROTECTED 0x00000010
#define CFE_LINK 0x00000020
#define CFE_AUTOCOLOR 0x40000000
#define CFE_AUTOBACKCOLOR CFM_BACKCOLOR
#define CFM_EFFECTS (CFM_BOLD | CFM_ITALIC | CFM_UNDERLINE | CFM_COLOR | CFM_STRIKEOUT | CFE_PROTECTED | CFM_LINK)
#define CFM_ALL (CFM_EFFECTS | CFM_SIZE | CFM_FACE | CFM_OFFSET | CFM_CHARSET)

#define SCF_DEFAULT 0x0000
#define SCF_SELECTION 0x0001
#define SCF_WORD 0x0002
#define SCF_ALL 0x0004
#define SCF_USEUIRULES 0x0008

#define GTL_DEFAULT 0
#define GTL_USECRLF 1
#define GTL_PRECISE 2
#define GTL_CLOSE 4
#define GTL_NUMCHARS 8
#define GTL_NUMBYTES 16
#define GT_DEFAULT 0
#define GT_USECRLF 1
#define GT_SELECTION 2
#define GT_RAWTEXT 4
#define GT_NOHIDDENTEXT 8
#define ST_DEFAULT 0
#define ST_KEEPUNDO 1
#define ST_SELECTION 2
#define ST_NEWCHARS 4
#define ST_UNICODE 8

#define ECOOP_SET 0x0001
#define ECOOP_OR 0x0002
#define ECOOP_AND 0x0003
#define ECOOP_XOR 0x0004
#define ECO_AUTOWORDSELECTION 0x00000001
#define ECO_AUTOVSCROLL 0x00000040
#define ECO_AUTOHSCROLL 0x00000080
#define ECO_NOHIDESEL 0x00000100
#define ECO_READONLY 0x00000800
#define ECO_WANTRETURN 0x00001000
#define ECO_SAVESEL 0x00008000
#define ECO_SELECTIONBAR 0x01000000

#define SEL_EMPTY 0x0000
#define SEL_TEXT 0x0001
#define SEL_MULTICHAR 0x0004

#define SF_TEXT 0x0001
#define SF_RTF 0x0002
#define SF_UNICODE 0x0010
#define SFF_SELECTION 0x8000

#ifndef FR_DOWN
#define FR_DOWN 0x00000001
#define FR_WHOLEWORD 0x00000002
#define FR_MATCHCASE 0x00000004
#endif

struct CHARRANGE { LONG cpMin; LONG cpMax; };
struct CHARFORMATW { UINT cbSize; DWORD dwMask; DWORD dwEffects; LONG yHeight; LONG yOffset; COLORREF crTextColor; BYTE bCharSet; BYTE bPitchAndFamily; WCHAR szFaceName[LF_FACESIZE]; };
typedef CHARFORMATW CHARFORMAT;
struct CHARFORMAT2W : CHARFORMATW { WORD wWeight; SHORT sSpacing; COLORREF crBackColor; LCID lcid; DWORD dwReserved; SHORT sStyle; WORD wKerning; BYTE bUnderlineType; BYTE bAnimation; BYTE bRevAuthor; BYTE bUnderlineColor; };
typedef CHARFORMAT2W CHARFORMAT2;
struct TEXTRANGEW { CHARRANGE chrg; LPWSTR lpstrText; };
struct TEXTRANGEA { CHARRANGE chrg; LPSTR lpstrText; };
typedef TEXTRANGEW TEXTRANGE;
struct FINDTEXTW { CHARRANGE chrg; LPCWSTR lpstrText; };
struct FINDTEXTA { CHARRANGE chrg; LPCSTR lpstrText; };
typedef FINDTEXTW FINDTEXT;
struct FINDTEXTEXW { CHARRANGE chrg; LPCWSTR lpstrText; CHARRANGE chrgText; };
struct FINDTEXTEXA { CHARRANGE chrg; LPCSTR lpstrText; CHARRANGE chrgText; };
typedef FINDTEXTEXW FINDTEXTEX;
struct GETTEXTLENGTHEX { DWORD flags; UINT codepage; };
struct GETTEXTEX { DWORD cb; DWORD flags; UINT codepage; LPCSTR lpDefaultChar; LPBOOL lpUsedDefChar; };
struct SETTEXTEX { DWORD flags; UINT codepage; };
struct MSGFILTER { NMHDR nmhdr; UINT msg; WPARAM wParam; LPARAM lParam; };
struct SELCHANGE { NMHDR nmhdr; CHARRANGE chrg; WORD seltyp; };
struct ENLINK { NMHDR nmhdr; UINT msg; WPARAM wParam; LPARAM lParam; CHARRANGE chrg; };
typedef DWORD (CALLBACK* EDITSTREAMCALLBACK)(DWORD_PTR cookie, LPBYTE buffer, LONG cb, LONG* pcb);
struct EDITSTREAM { DWORD_PTR dwCookie; DWORD dwError; EDITSTREAMCALLBACK pfnCallback; };
struct PARAFORMAT { UINT cbSize; DWORD dwMask; WORD wNumbering; WORD wEffects; LONG dxStartIndent; LONG dxRightIndent; LONG dxOffset; WORD wAlignment; SHORT cTabCount; LONG rgxTabs[32]; };
#endif
