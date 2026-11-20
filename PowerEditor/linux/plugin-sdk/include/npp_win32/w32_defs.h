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

// Notepad++ Linux plugin SDK: the constants of the Win32 API (values of the Windows
// SDK). Macros only: the resource compiler of the SDK (tools/rc2cpp.py) preprocesses
// the .rc files of plugins with this file.

#pragma once

#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif

#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif
#ifndef NULL
#define NULL 0
#endif
#ifndef MAX_PATH
#define MAX_PATH 4096 // paths of Linux are longer than 260
#endif

#ifndef IDC_STATIC
#define IDC_STATIC (-1)
#endif

// DllMain notification reasons
#define DLL_PROCESS_DETACH 0
#define DLL_PROCESS_ATTACH 1
#define DLL_THREAD_ATTACH  2
#define DLL_THREAD_DETACH  3

// ---- window styles ----------------------------------------------------------------
#define WS_OVERLAPPED       0x00000000L
#define WS_POPUP            0x80000000L
#define WS_CHILD            0x40000000L
#define WS_MINIMIZE         0x20000000L
#define WS_VISIBLE          0x10000000L
#define WS_DISABLED         0x08000000L
#define WS_CLIPSIBLINGS     0x04000000L
#define WS_CLIPCHILDREN     0x02000000L
#define WS_MAXIMIZE         0x01000000L
#define WS_CAPTION          0x00C00000L
#define WS_BORDER           0x00800000L
#define WS_DLGFRAME         0x00400000L
#define WS_VSCROLL          0x00200000L
#define WS_HSCROLL          0x00100000L
#define WS_SYSMENU          0x00080000L
#define WS_THICKFRAME       0x00040000L
#define WS_GROUP            0x00020000L
#define WS_TABSTOP          0x00010000L
#define WS_MINIMIZEBOX      0x00020000L
#define WS_MAXIMIZEBOX      0x00010000L
#define WS_TILED            WS_OVERLAPPED
#define WS_ICONIC           WS_MINIMIZE
#define WS_SIZEBOX          WS_THICKFRAME
#define WS_OVERLAPPEDWINDOW (WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MINIMIZEBOX | WS_MAXIMIZEBOX)
#define WS_TILEDWINDOW      WS_OVERLAPPEDWINDOW
#define WS_POPUPWINDOW      (WS_POPUP | WS_BORDER | WS_SYSMENU)
#define WS_CHILDWINDOW      WS_CHILD

#define WS_EX_DLGMODALFRAME     0x00000001L
#define WS_EX_NOPARENTNOTIFY    0x00000004L
#define WS_EX_TOPMOST           0x00000008L
#define WS_EX_ACCEPTFILES       0x00000010L
#define WS_EX_TRANSPARENT       0x00000020L
#define WS_EX_MDICHILD          0x00000040L
#define WS_EX_TOOLWINDOW        0x00000080L
#define WS_EX_WINDOWEDGE        0x00000100L
#define WS_EX_CLIENTEDGE        0x00000200L
#define WS_EX_CONTEXTHELP       0x00000400L
#define WS_EX_RIGHT             0x00001000L
#define WS_EX_LEFT              0x00000000L
#define WS_EX_RTLREADING        0x00002000L
#define WS_EX_LTRREADING        0x00000000L
#define WS_EX_LEFTSCROLLBAR     0x00004000L
#define WS_EX_RIGHTSCROLLBAR    0x00000000L
#define WS_EX_CONTROLPARENT     0x00010000L
#define WS_EX_STATICEDGE        0x00020000L
#define WS_EX_APPWINDOW         0x00040000L
#define WS_EX_LAYERED           0x00080000L
#define WS_EX_NOINHERITLAYOUT   0x00100000L
#define WS_EX_LAYOUTRTL         0x00400000L
#define WS_EX_COMPOSITED        0x02000000L
#define WS_EX_NOACTIVATE        0x08000000L
#define WS_EX_OVERLAPPEDWINDOW  (WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE)
#define WS_EX_PALETTEWINDOW     (WS_EX_WINDOWEDGE | WS_EX_TOOLWINDOW | WS_EX_TOPMOST)

#define DS_ABSALIGN         0x01L
#define DS_SYSMODAL         0x02L
#define DS_3DLOOK           0x0004L
#define DS_FIXEDSYS         0x0008L
#define DS_NOFAILCREATE     0x0010L
#define DS_LOCALEDIT        0x20L
#define DS_SETFONT          0x40L
#define DS_MODALFRAME       0x80L
#define DS_NOIDLEMSG        0x100L
#define DS_SETFOREGROUND    0x200L
#define DS_CONTROL          0x0400L
#define DS_CENTER           0x0800L
#define DS_CENTERMOUSE      0x1000L
#define DS_CONTEXTHELP      0x2000L
#define DS_SHELLFONT        (DS_SETFONT | DS_FIXEDSYS)

#define BS_PUSHBUTTON       0x00000000L
#define BS_DEFPUSHBUTTON    0x00000001L
#define BS_CHECKBOX         0x00000002L
#define BS_AUTOCHECKBOX     0x00000003L
#define BS_RADIOBUTTON      0x00000004L
#define BS_3STATE           0x00000005L
#define BS_AUTO3STATE       0x00000006L
#define BS_GROUPBOX         0x00000007L
#define BS_USERBUTTON       0x00000008L
#define BS_AUTORADIOBUTTON  0x00000009L
#define BS_PUSHBOX          0x0000000AL
#define BS_OWNERDRAW        0x0000000BL
#define BS_SPLITBUTTON      0x0000000CL
#define BS_DEFSPLITBUTTON   0x0000000DL
#define BS_COMMANDLINK      0x0000000EL
#define BS_DEFCOMMANDLINK   0x0000000FL
#define BS_TYPEMASK         0x0000000FL
#define BS_LEFTTEXT         0x00000020L
#define BS_RIGHTBUTTON      BS_LEFTTEXT
#define BS_TEXT             0x00000000L
#define BS_ICON             0x00000040L
#define BS_BITMAP           0x00000080L
#define BS_LEFT             0x00000100L
#define BS_RIGHT            0x00000200L
#define BS_CENTER           0x00000300L
#define BS_TOP              0x00000400L
#define BS_BOTTOM           0x00000800L
#define BS_VCENTER          0x00000C00L
#define BS_PUSHLIKE         0x00001000L
#define BS_MULTILINE        0x00002000L
#define BS_NOTIFY           0x00004000L
#define BS_FLAT             0x00008000L

#define ES_LEFT             0x0000L
#define ES_CENTER           0x0001L
#define ES_RIGHT            0x0002L
#define ES_MULTILINE        0x0004L
#define ES_UPPERCASE        0x0008L
#define ES_LOWERCASE        0x0010L
#define ES_PASSWORD         0x0020L
#define ES_AUTOVSCROLL      0x0040L
#define ES_AUTOHSCROLL      0x0080L
#define ES_NOHIDESEL        0x0100L
#define ES_OEMCONVERT       0x0400L
#define ES_READONLY         0x0800L
#define ES_WANTRETURN       0x1000L
#define ES_NUMBER           0x2000L

#define SS_LEFT             0x00000000L
#define SS_CENTER           0x00000001L
#define SS_RIGHT            0x00000002L
#define SS_ICON             0x00000003L
#define SS_BLACKRECT        0x00000004L
#define SS_GRAYRECT         0x00000005L
#define SS_WHITERECT        0x00000006L
#define SS_BLACKFRAME       0x00000007L
#define SS_GRAYFRAME        0x00000008L
#define SS_WHITEFRAME       0x00000009L
#define SS_USERITEM         0x0000000AL
#define SS_SIMPLE           0x0000000BL
#define SS_LEFTNOWORDWRAP   0x0000000CL
#define SS_OWNERDRAW        0x0000000DL
#define SS_BITMAP           0x0000000EL
#define SS_ENHMETAFILE      0x0000000FL
#define SS_ETCHEDHORZ       0x00000010L
#define SS_ETCHEDVERT       0x00000011L
#define SS_ETCHEDFRAME      0x00000012L
#define SS_TYPEMASK         0x0000001FL
#define SS_REALSIZECONTROL  0x00000040L
#define SS_NOPREFIX         0x00000080L
#define SS_NOTIFY           0x00000100L
#define SS_CENTERIMAGE      0x00000200L
#define SS_RIGHTJUST        0x00000400L
#define SS_REALSIZEIMAGE    0x00000800L
#define SS_SUNKEN           0x00001000L
#define SS_EDITCONTROL      0x00002000L
#define SS_ENDELLIPSIS      0x00004000L
#define SS_PATHELLIPSIS     0x00008000L
#define SS_WORDELLIPSIS     0x0000C000L
#define SS_ELLIPSISMASK     0x0000C000L

#define CBS_SIMPLE            0x0001L
#define CBS_DROPDOWN          0x0002L
#define CBS_DROPDOWNLIST      0x0003L
#define CBS_OWNERDRAWFIXED    0x0010L
#define CBS_OWNERDRAWVARIABLE 0x0020L
#define CBS_AUTOHSCROLL       0x0040L
#define CBS_OEMCONVERT        0x0080L
#define CBS_SORT              0x0100L
#define CBS_HASSTRINGS        0x0200L
#define CBS_NOINTEGRALHEIGHT  0x0400L
#define CBS_DISABLENOSCROLL   0x0800L
#define CBS_UPPERCASE         0x2000L
#define CBS_LOWERCASE         0x4000L

#define LBS_NOTIFY            0x0001L
#define LBS_SORT              0x0002L
#define LBS_NOREDRAW          0x0004L
#define LBS_MULTIPLESEL       0x0008L
#define LBS_OWNERDRAWFIXED    0x0010L
#define LBS_OWNERDRAWVARIABLE 0x0020L
#define LBS_HASSTRINGS        0x0040L
#define LBS_USETABSTOPS       0x0080L
#define LBS_NOINTEGRALHEIGHT  0x0100L
#define LBS_MULTICOLUMN       0x0200L
#define LBS_WANTKEYBOARDINPUT 0x0400L
#define LBS_EXTENDEDSEL       0x0800L
#define LBS_DISABLENOSCROLL   0x1000L
#define LBS_NODATA            0x2000L
#define LBS_NOSEL             0x4000L
#define LBS_COMBOBOX          0x8000L
#define LBS_STANDARD          (LBS_NOTIFY | LBS_SORT | WS_VSCROLL | WS_BORDER)

#define SBS_HORZ 0x0000L
#define SBS_VERT 0x0001L
#define SBS_SIZEGRIP 0x0010L

// ---- messages ---------------------------------------------------------------------
#define WM_NULL             0x0000
#define WM_CREATE           0x0001
#define WM_DESTROY          0x0002
#define WM_MOVE             0x0003
#define WM_SIZE             0x0005
#define WM_ACTIVATE         0x0006
#define WM_SETFOCUS         0x0007
#define WM_KILLFOCUS        0x0008
#define WM_ENABLE           0x000A
#define WM_SETREDRAW        0x000B
#define WM_SETTEXT          0x000C
#define WM_GETTEXT          0x000D
#define WM_GETTEXTLENGTH    0x000E
#define WM_PAINT            0x000F
#define WM_CLOSE            0x0010
#define WM_QUIT             0x0012
#define WM_ERASEBKGND       0x0014
#define WM_SYSCOLORCHANGE   0x0015
#define WM_SHOWWINDOW       0x0018
#define WM_SETTINGCHANGE    0x001A
#define WM_WININICHANGE     0x001A
#define WM_ACTIVATEAPP      0x001C
#define WM_SETCURSOR        0x0020
#define WM_MOUSEACTIVATE    0x0021
#define WM_GETMINMAXINFO    0x0024
#define WM_NEXTDLGCTL       0x0028
#define WM_DRAWITEM         0x002B
#define WM_MEASUREITEM      0x002C
#define WM_DELETEITEM       0x002D
#define WM_VKEYTOITEM       0x002E
#define WM_CHARTOITEM       0x002F
#define WM_SETFONT          0x0030
#define WM_GETFONT          0x0031
#define WM_QUERYDRAGICON    0x0037
#define WM_COMPAREITEM      0x0039
#define WM_WINDOWPOSCHANGING 0x0046
#define WM_WINDOWPOSCHANGED 0x0047
#define WM_NOTIFY           0x004E
#define WM_HELP             0x0053
#define WM_NOTIFYFORMAT     0x0055
#define WM_CONTEXTMENU      0x007B
#define WM_STYLECHANGED     0x007D
#define WM_GETICON          0x007F
#define WM_SETICON          0x0080
#define WM_NCCREATE         0x0081
#define WM_NCDESTROY        0x0082
#define WM_NCCALCSIZE       0x0083
#define WM_NCHITTEST        0x0084
#define WM_NCPAINT          0x0085
#define WM_NCACTIVATE       0x0086
#define WM_GETDLGCODE       0x0087
#define WM_NCMOUSEMOVE      0x00A0
#define WM_NCLBUTTONDOWN    0x00A1
#define WM_NCLBUTTONUP      0x00A2
#define WM_NCLBUTTONDBLCLK  0x00A3
#define WM_KEYFIRST         0x0100
#define WM_KEYDOWN          0x0100
#define WM_KEYUP            0x0101
#define WM_CHAR             0x0102
#define WM_DEADCHAR         0x0103
#define WM_SYSKEYDOWN       0x0104
#define WM_SYSKEYUP         0x0105
#define WM_SYSCHAR          0x0106
#define WM_KEYLAST          0x0109
#define WM_INITDIALOG       0x0110
#define WM_COMMAND          0x0111
#define WM_SYSCOMMAND       0x0112
#define WM_TIMER            0x0113
#define WM_HSCROLL          0x0114
#define WM_VSCROLL          0x0115
#define WM_INITMENU         0x0116
#define WM_INITMENUPOPUP    0x0117
#define WM_MENUSELECT       0x011F
#define WM_MENUCHAR         0x0120
#define WM_ENTERIDLE        0x0121
#define WM_UNINITMENUPOPUP  0x0125
#define WM_CHANGEUISTATE    0x0127
#define WM_UPDATEUISTATE    0x0128
#define WM_QUERYUISTATE     0x0129
#define WM_CTLCOLORMSGBOX   0x0132
#define WM_CTLCOLOREDIT     0x0133
#define WM_CTLCOLORLISTBOX  0x0134
#define WM_CTLCOLORBTN      0x0135
#define WM_CTLCOLORDLG      0x0136
#define WM_CTLCOLORSCROLLBAR 0x0137
#define WM_CTLCOLORSTATIC   0x0138
#define WM_MOUSEFIRST       0x0200
#define WM_MOUSEMOVE        0x0200
#define WM_LBUTTONDOWN      0x0201
#define WM_LBUTTONUP        0x0202
#define WM_LBUTTONDBLCLK    0x0203
#define WM_RBUTTONDOWN      0x0204
#define WM_RBUTTONUP        0x0205
#define WM_RBUTTONDBLCLK    0x0206
#define WM_MBUTTONDOWN      0x0207
#define WM_MBUTTONUP        0x0208
#define WM_MBUTTONDBLCLK    0x0209
#define WM_MOUSEWHEEL       0x020A
#define WM_XBUTTONDOWN      0x020B
#define WM_XBUTTONUP        0x020C
#define WM_MOUSEHWHEEL      0x020E
#define WM_MOUSELAST        0x020E
#define WM_PARENTNOTIFY     0x0210
#define WM_ENTERMENULOOP    0x0211
#define WM_EXITMENULOOP     0x0212
#define WM_SIZING           0x0214
#define WM_CAPTURECHANGED   0x0215
#define WM_MOVING           0x0216
#define WM_ENTERSIZEMOVE    0x0231
#define WM_EXITSIZEMOVE     0x0232
#define WM_DROPFILES        0x0233
#define WM_DPICHANGED       0x02E0
#define WM_MOUSEHOVER       0x02A1
#define WM_MOUSELEAVE       0x02A3
#define WM_CUT              0x0300
#define WM_COPY             0x0301
#define WM_PASTE            0x0302
#define WM_CLEAR            0x0303
#define WM_UNDO             0x0304
#define WM_PRINT            0x0317
#define WM_PRINTCLIENT      0x0318
#define WM_THEMECHANGED     0x031A
#define WM_USER             0x0400
#define WM_APP              0x8000

// notification codes of WM_COMMAND
#define BN_CLICKED          0
#define BN_PAINT            1
#define BN_DBLCLK           5
#define BN_DOUBLECLICKED    BN_DBLCLK
#define BN_SETFOCUS         6
#define BN_KILLFOCUS        7
#define EN_SETFOCUS         0x0100
#define EN_KILLFOCUS        0x0200
#define EN_CHANGE           0x0300
#define EN_UPDATE           0x0400
#define EN_ERRSPACE         0x0500
#define EN_MAXTEXT          0x0501
#define EN_HSCROLL          0x0601
#define EN_VSCROLL          0x0602
#define CBN_ERRSPACE        (-1)
#define CBN_SELCHANGE       1
#define CBN_DBLCLK          2
#define CBN_SETFOCUS        3
#define CBN_KILLFOCUS       4
#define CBN_EDITCHANGE      5
#define CBN_EDITUPDATE      6
#define CBN_DROPDOWN        7
#define CBN_CLOSEUP         8
#define CBN_SELENDOK        9
#define CBN_SELENDCANCEL    10
#define LBN_ERRSPACE        (-2)
#define LBN_SELCHANGE       1
#define LBN_DBLCLK          2
#define LBN_SELCANCEL       3
#define LBN_SETFOCUS        4
#define LBN_KILLFOCUS       5
#define STN_CLICKED         0
#define STN_DBLCLK          1
#define STN_ENABLE          2
#define STN_DISABLE         3

// button, edit, combo box, list box, static messages
#define BM_GETCHECK         0x00F0
#define BM_SETCHECK         0x00F1
#define BM_GETSTATE         0x00F2
#define BM_SETSTATE         0x00F3
#define BM_SETSTYLE         0x00F4
#define BM_CLICK            0x00F5
#define BM_GETIMAGE         0x00F6
#define BM_SETIMAGE         0x00F7
#define BM_SETDONTCLICK     0x00F8
#define BST_UNCHECKED       0x0000
#define BST_CHECKED         0x0001
#define BST_INDETERMINATE   0x0002
#define BST_PUSHED          0x0004
#define BST_FOCUS           0x0008

#define EM_GETSEL           0x00B0
#define EM_SETSEL           0x00B1
#define EM_GETRECT          0x00B2
#define EM_SETRECT          0x00B3
#define EM_SETRECTNP        0x00B4
#define EM_SCROLL           0x00B5
#define EM_LINESCROLL       0x00B6
#define EM_SCROLLCARET      0x00B7
#define EM_GETMODIFY        0x00B8
#define EM_SETMODIFY        0x00B9
#define EM_GETLINECOUNT     0x00BA
#define EM_LINEINDEX        0x00BB
#define EM_SETHANDLE        0x00BC
#define EM_GETHANDLE        0x00BD
#define EM_GETTHUMB         0x00BE
#define EM_LINELENGTH       0x00C1
#define EM_REPLACESEL       0x00C2
#define EM_GETLINE          0x00C4
#define EM_LIMITTEXT        0x00C5
#define EM_SETLIMITTEXT     EM_LIMITTEXT
#define EM_CANUNDO          0x00C6
#define EM_UNDO             0x00C7
#define EM_FMTLINES         0x00C8
#define EM_LINEFROMCHAR     0x00C9
#define EM_SETTABSTOPS      0x00CB
#define EM_SETPASSWORDCHAR  0x00CC
#define EM_EMPTYUNDOBUFFER  0x00CD
#define EM_GETFIRSTVISIBLELINE 0x00CE
#define EM_SETREADONLY      0x00CF
#define EM_SETWORDBREAKPROC 0x00D0
#define EM_GETPASSWORDCHAR  0x00D2
#define EM_SETMARGINS       0x00D3
#define EM_GETMARGINS       0x00D4
#define EM_GETLIMITTEXT     0x00D5
#define EM_POSFROMCHAR      0x00D6
#define EM_CHARFROMPOS      0x00D7
#define ECM_FIRST           0x1500
#define EM_SETCUEBANNER     (ECM_FIRST + 1)
#define EM_GETCUEBANNER     (ECM_FIRST + 2)
#define EM_SHOWBALLOONTIP   (ECM_FIRST + 3)
#define EM_HIDEBALLOONTIP   (ECM_FIRST + 4)
#define EC_LEFTMARGIN       0x0001
#define EC_RIGHTMARGIN      0x0002

#define CB_GETEDITSEL       0x0140
#define CB_LIMITTEXT        0x0141
#define CB_SETEDITSEL       0x0142
#define CB_ADDSTRING        0x0143
#define CB_DELETESTRING     0x0144
#define CB_DIR              0x0145
#define CB_GETCOUNT         0x0146
#define CB_GETCURSEL        0x0147
#define CB_GETLBTEXT        0x0148
#define CB_GETLBTEXTLEN     0x0149
#define CB_INSERTSTRING     0x014A
#define CB_RESETCONTENT     0x014B
#define CB_FINDSTRING       0x014C
#define CB_SELECTSTRING     0x014D
#define CB_SETCURSEL        0x014E
#define CB_SHOWDROPDOWN     0x014F
#define CB_GETITEMDATA      0x0150
#define CB_SETITEMDATA      0x0151
#define CB_GETDROPPEDCONTROLRECT 0x0152
#define CB_SETITEMHEIGHT    0x0153
#define CB_GETITEMHEIGHT    0x0154
#define CB_SETEXTENDEDUI    0x0155
#define CB_GETEXTENDEDUI    0x0156
#define CB_GETDROPPEDSTATE  0x0157
#define CB_FINDSTRINGEXACT  0x0158
#define CB_SETLOCALE        0x0159
#define CB_GETLOCALE        0x015A
#define CB_GETTOPINDEX      0x015B
#define CB_SETTOPINDEX      0x015C
#define CB_GETHORIZONTALEXTENT 0x015D
#define CB_SETHORIZONTALEXTENT 0x015E
#define CB_GETDROPPEDWIDTH  0x015F
#define CB_SETDROPPEDWIDTH  0x0160
#define CB_INITSTORAGE      0x0161
#define CB_GETCOMBOBOXINFO  0x0164
#define CBM_FIRST           0x1700
#define CB_SETMINVISIBLE    (CBM_FIRST + 1)
#define CB_GETMINVISIBLE    (CBM_FIRST + 2)
#define CB_SETCUEBANNER     (CBM_FIRST + 3)
#define CB_OKAY             0
#define CB_ERR              (-1)
#define CB_ERRSPACE         (-2)

#define LB_ADDSTRING        0x0180
#define LB_INSERTSTRING     0x0181
#define LB_DELETESTRING     0x0182
#define LB_SELITEMRANGEEX   0x0183
#define LB_RESETCONTENT     0x0184
#define LB_SETSEL           0x0185
#define LB_SETCURSEL        0x0186
#define LB_GETSEL           0x0187
#define LB_GETCURSEL        0x0188
#define LB_GETTEXT          0x0189
#define LB_GETTEXTLEN       0x018A
#define LB_GETCOUNT         0x018B
#define LB_SELECTSTRING     0x018C
#define LB_DIR              0x018D
#define LB_GETTOPINDEX      0x018E
#define LB_FINDSTRING       0x018F
#define LB_GETSELCOUNT      0x0190
#define LB_GETSELITEMS      0x0191
#define LB_SETTABSTOPS      0x0192
#define LB_GETHORIZONTALEXTENT 0x0193
#define LB_SETHORIZONTALEXTENT 0x0194
#define LB_SETCOLUMNWIDTH   0x0195
#define LB_ADDFILE          0x0196
#define LB_SETTOPINDEX      0x0197
#define LB_GETITEMRECT      0x0198
#define LB_GETITEMDATA      0x0199
#define LB_SETITEMDATA      0x019A
#define LB_SELITEMRANGE     0x019B
#define LB_SETANCHORINDEX   0x019C
#define LB_GETANCHORINDEX   0x019D
#define LB_SETCARETINDEX    0x019E
#define LB_GETCARETINDEX    0x019F
#define LB_SETITEMHEIGHT    0x01A0
#define LB_GETITEMHEIGHT    0x01A1
#define LB_FINDSTRINGEXACT  0x01A2
#define LB_SETLOCALE        0x01A5
#define LB_GETLOCALE        0x01A6
#define LB_SETCOUNT         0x01A7
#define LB_INITSTORAGE      0x01A8
#define LB_ITEMFROMPOINT    0x01A9
#define LB_OKAY             0
#define LB_ERR              (-1)
#define LB_ERRSPACE         (-2)

#define STM_SETICON         0x0170
#define STM_GETICON         0x0171
#define STM_SETIMAGE        0x0172
#define STM_GETIMAGE        0x0173

#define SBM_SETPOS          0x00E0
#define SBM_GETPOS          0x00E1
#define SBM_SETRANGE        0x00E2
#define SBM_GETRANGE        0x00E3
#define SBM_SETSCROLLINFO   0x00E9
#define SBM_GETSCROLLINFO   0x00EA

#define DM_GETDEFID         (WM_USER + 0)
#define DM_SETDEFID         (WM_USER + 1)
#define DM_REPOSITION       (WM_USER + 2)
#define DC_HASDEFID         0x534B

#define DLGC_WANTARROWS     0x0001
#define DLGC_WANTTAB        0x0002
#define DLGC_WANTALLKEYS    0x0004
#define DLGC_WANTMESSAGE    0x0004
#define DLGC_HASSETSEL      0x0008
#define DLGC_DEFPUSHBUTTON  0x0010
#define DLGC_UNDEFPUSHBUTTON 0x0020
#define DLGC_RADIOBUTTON    0x0040
#define DLGC_WANTCHARS      0x0080
#define DLGC_STATIC         0x0100
#define DLGC_BUTTON         0x2000

// ---- dialog box command ids --------------------------------------------------------
#define IDOK                1
#define IDCANCEL            2
#define IDABORT             3
#define IDRETRY             4
#define IDIGNORE            5
#define IDYES               6
#define IDNO                7
#define IDCLOSE             8
#define IDHELP              9
#define IDTRYAGAIN          10
#define IDCONTINUE          11

// ---- show window, window positions ------------------------------------------------
#define SW_HIDE             0
#define SW_SHOWNORMAL       1
#define SW_NORMAL           1
#define SW_SHOWMINIMIZED    2
#define SW_SHOWMAXIMIZED    3
#define SW_MAXIMIZE         3
#define SW_SHOWNOACTIVATE   4
#define SW_SHOW             5
#define SW_MINIMIZE         6
#define SW_SHOWMINNOACTIVE  7
#define SW_SHOWNA           8
#define SW_RESTORE          9
#define SW_SHOWDEFAULT      10
#define SW_FORCEMINIMIZE    11

#define SWP_NOSIZE          0x0001
#define SWP_NOMOVE          0x0002
#define SWP_NOZORDER        0x0004
#define SWP_NOREDRAW        0x0008
#define SWP_NOACTIVATE      0x0010
#define SWP_FRAMECHANGED    0x0020
#define SWP_SHOWWINDOW      0x0040
#define SWP_HIDEWINDOW      0x0080
#define SWP_NOCOPYBITS      0x0100
#define SWP_NOOWNERZORDER   0x0200
#define SWP_NOSENDCHANGING  0x0400
#define SWP_DRAWFRAME       SWP_FRAMECHANGED
#define SWP_NOREPOSITION    SWP_NOOWNERZORDER
#define SWP_DEFERERASE      0x2000
#define SWP_ASYNCWINDOWPOS  0x4000

#define GWL_WNDPROC         (-4)
#define GWLP_WNDPROC        (-4)
#define GWLP_HINSTANCE      (-6)
#define GWL_HINSTANCE       (-6)
#define GWLP_HWNDPARENT     (-8)
#define GWLP_ID             (-12)
#define GWL_ID              (-12)
#define GWL_STYLE           (-16)
#define GWL_EXSTYLE         (-20)
#define GWLP_USERDATA       (-21)
#define GWL_USERDATA        (-21)
#define DWLP_MSGRESULT      0
#define DWLP_DLGPROC        8
#define DWLP_USER           16
#define DWL_MSGRESULT       DWLP_MSGRESULT
#define DWL_DLGPROC         DWLP_DLGPROC
#define DWL_USER            DWLP_USER
#define GCLP_HBRBACKGROUND  (-10)
#define GCLP_HCURSOR        (-12)
#define GCLP_HICON          (-14)
#define GCL_STYLE           (-26)

#define RDW_INVALIDATE      0x0001
#define RDW_INTERNALPAINT   0x0002
#define RDW_ERASE           0x0004
#define RDW_VALIDATE        0x0008
#define RDW_NOINTERNALPAINT 0x0010
#define RDW_NOERASE         0x0020
#define RDW_NOCHILDREN      0x0040
#define RDW_ALLCHILDREN     0x0080
#define RDW_UPDATENOW       0x0100
#define RDW_ERASENOW        0x0200
#define RDW_FRAME           0x0400
#define RDW_NOFRAME         0x0800

#define GW_HWNDFIRST        0
#define GW_HWNDLAST         1
#define GW_HWNDNEXT         2
#define GW_HWNDPREV         3
#define GW_OWNER            4
#define GW_CHILD            5
#define GA_PARENT           1
#define GA_ROOT             2
#define GA_ROOTOWNER        3

#define SC_SIZE             0xF000
#define SC_MOVE             0xF010
#define SC_MINIMIZE         0xF020
#define SC_MAXIMIZE         0xF030
#define SC_CLOSE            0xF060
#define SC_KEYMENU          0xF100
#define SC_RESTORE          0xF120

#define SIZE_RESTORED       0
#define SIZE_MINIMIZED      1
#define SIZE_MAXIMIZED      2
#define WA_INACTIVE         0
#define WA_ACTIVE           1
#define WA_CLICKACTIVE      2
#define HTERROR             (-2)
#define HTTRANSPARENT       (-1)
#define HTNOWHERE           0
#define HTCLIENT            1
#define HTCAPTION           2
#define HTLEFT              10
#define HTRIGHT             11
#define HTTOP               12
#define HTBOTTOM            15

#define CS_VREDRAW          0x0001
#define CS_HREDRAW          0x0002
#define CS_DBLCLKS          0x0008
#define CS_OWNDC            0x0020
#define CS_CLASSDC          0x0040
#define CS_PARENTDC         0x0080
#define CS_NOCLOSE          0x0200
#define CS_SAVEBITS         0x0800
#define CS_GLOBALCLASS      0x4000
#define CS_DROPSHADOW       0x00020000

#define CW_USEDEFAULT       ((int)0x80000000)

// ---- system metrics and colours ----------------------------------------------------
#define SM_CXSCREEN         0
#define SM_CYSCREEN         1
#define SM_CXVSCROLL        2
#define SM_CYHSCROLL        3
#define SM_CYCAPTION        4
#define SM_CXBORDER         5
#define SM_CYBORDER         6
#define SM_CXDLGFRAME       7
#define SM_CYDLGFRAME       8
#define SM_CYVTHUMB         9
#define SM_CXHTHUMB         10
#define SM_CXICON           11
#define SM_CYICON           12
#define SM_CXCURSOR         13
#define SM_CYCURSOR         14
#define SM_CYMENU           15
#define SM_CXFULLSCREEN     16
#define SM_CYFULLSCREEN     17
#define SM_CYVSCROLL        20
#define SM_CXHSCROLL        21
#define SM_SWAPBUTTON       23
#define SM_CXMIN            28
#define SM_CYMIN            29
#define SM_CXSIZE           30
#define SM_CYSIZE           31
#define SM_CXFRAME          32
#define SM_CYFRAME          33
#define SM_CXSIZEFRAME      SM_CXFRAME
#define SM_CYSIZEFRAME      SM_CYFRAME
#define SM_CXDOUBLECLK      36
#define SM_CYDOUBLECLK      37
#define SM_CXEDGE           45
#define SM_CYEDGE           46
#define SM_CXSMICON         49
#define SM_CYSMICON         50
#define SM_CYSMCAPTION      51
#define SM_CXDRAG           68
#define SM_CYDRAG           69
#define SM_CXMENUCHECK      71
#define SM_CYMENUCHECK      72
#define SM_XVIRTUALSCREEN   76
#define SM_YVIRTUALSCREEN   77
#define SM_CXVIRTUALSCREEN  78
#define SM_CYVIRTUALSCREEN  79
#define SM_CMONITORS        80
#define SM_CXPADDEDBORDER   92

#define COLOR_SCROLLBAR         0
#define COLOR_BACKGROUND        1
#define COLOR_DESKTOP           1
#define COLOR_ACTIVECAPTION     2
#define COLOR_INACTIVECAPTION   3
#define COLOR_MENU              4
#define COLOR_WINDOW            5
#define COLOR_WINDOWFRAME       6
#define COLOR_MENUTEXT          7
#define COLOR_WINDOWTEXT        8
#define COLOR_CAPTIONTEXT       9
#define COLOR_ACTIVEBORDER      10
#define COLOR_INACTIVEBORDER    11
#define COLOR_APPWORKSPACE      12
#define COLOR_HIGHLIGHT         13
#define COLOR_HIGHLIGHTTEXT     14
#define COLOR_BTNFACE           15
#define COLOR_3DFACE            15
#define COLOR_BTNSHADOW         16
#define COLOR_3DSHADOW          16
#define COLOR_GRAYTEXT          17
#define COLOR_BTNTEXT           18
#define COLOR_INACTIVECAPTIONTEXT 19
#define COLOR_BTNHIGHLIGHT      20
#define COLOR_BTNHILIGHT        20
#define COLOR_3DHIGHLIGHT       20
#define COLOR_3DHILIGHT         20
#define COLOR_3DDKSHADOW        21
#define COLOR_3DLIGHT           22
#define COLOR_INFOTEXT          23
#define COLOR_INFOBK            24
#define COLOR_HOTLIGHT          26
#define COLOR_GRADIENTACTIVECAPTION 27
#define COLOR_GRADIENTINACTIVECAPTION 28
#define COLOR_MENUHILIGHT       29
#define COLOR_MENUBAR           30

#define SPI_GETNONCLIENTMETRICS 0x0029
#define SPI_GETWORKAREA         0x0030
#define SPI_GETICONTITLELOGFONT 0x001F
#define SPI_GETHIGHCONTRAST     0x0042
#define SPI_GETKEYBOARDCUES     0x100A
#define SPI_GETWHEELSCROLLLINES 0x0068
#define SPIF_UPDATEINIFILE      0x0001
#define SPIF_SENDCHANGE         0x0002

// ---- menus -------------------------------------------------------------------------
#define MF_INSERT           0x00000000L
#define MF_CHANGE           0x00000080L
#define MF_APPEND           0x00000100L
#define MF_DELETE           0x00000200L
#define MF_REMOVE           0x00001000L
#define MF_BYCOMMAND        0x00000000L
#define MF_BYPOSITION       0x00000400L
#define MF_SEPARATOR        0x00000800L
#define MF_ENABLED          0x00000000L
#define MF_GRAYED           0x00000001L
#define MF_DISABLED         0x00000002L
#define MF_UNCHECKED        0x00000000L
#define MF_CHECKED          0x00000008L
#define MF_USECHECKBITMAPS  0x00000200L
#define MF_STRING           0x00000000L
#define MF_BITMAP           0x00000004L
#define MF_OWNERDRAW        0x00000100L
#define MF_POPUP            0x00000010L
#define MF_MENUBARBREAK     0x00000020L
#define MF_MENUBREAK        0x00000040L
#define MF_UNHILITE         0x00000000L
#define MF_HILITE           0x00000080L
#define MF_DEFAULT          0x00001000L
#define MF_SYSMENU          0x00002000L
#define MF_HELP             0x00004000L
#define MF_RIGHTJUSTIFY     0x00004000L
#define MF_MOUSESELECT      0x00008000L
#define MFT_STRING          MF_STRING
#define MFT_BITMAP          MF_BITMAP
#define MFT_MENUBARBREAK    MF_MENUBARBREAK
#define MFT_MENUBREAK       MF_MENUBREAK
#define MFT_OWNERDRAW       MF_OWNERDRAW
#define MFT_RADIOCHECK      0x00000200L
#define MFT_SEPARATOR       MF_SEPARATOR
#define MFT_RIGHTORDER      0x00002000L
#define MFT_RIGHTJUSTIFY    MF_RIGHTJUSTIFY
#define MFS_GRAYED          0x00000003L
#define MFS_DISABLED        MFS_GRAYED
#define MFS_CHECKED         MF_CHECKED
#define MFS_HILITE          MF_HILITE
#define MFS_ENABLED         MF_ENABLED
#define MFS_UNCHECKED       MF_UNCHECKED
#define MFS_UNHILITE        MF_UNHILITE
#define MFS_DEFAULT         MF_DEFAULT
#define MIIM_STATE          0x00000001
#define MIIM_ID             0x00000002
#define MIIM_SUBMENU        0x00000004
#define MIIM_CHECKMARKS     0x00000008
#define MIIM_TYPE           0x00000010
#define MIIM_DATA           0x00000020
#define MIIM_STRING         0x00000040
#define MIIM_BITMAP         0x00000080
#define MIIM_FTYPE          0x00000100
#define TPM_LEFTBUTTON      0x0000L
#define TPM_RIGHTBUTTON     0x0002L
#define TPM_LEFTALIGN       0x0000L
#define TPM_CENTERALIGN     0x0004L
#define TPM_RIGHTALIGN      0x0008L
#define TPM_TOPALIGN        0x0000L
#define TPM_VCENTERALIGN    0x0010L
#define TPM_BOTTOMALIGN     0x0020L
#define TPM_HORIZONTAL      0x0000L
#define TPM_VERTICAL        0x0040L
#define TPM_NONOTIFY        0x0080L
#define TPM_RETURNCMD       0x0100L

// ---- message boxes -----------------------------------------------------------------
#define MB_OK                0x00000000L
#define MB_OKCANCEL          0x00000001L
#define MB_ABORTRETRYIGNORE  0x00000002L
#define MB_YESNOCANCEL       0x00000003L
#define MB_YESNO             0x00000004L
#define MB_RETRYCANCEL       0x00000005L
#define MB_CANCELTRYCONTINUE 0x00000006L
#define MB_ICONHAND          0x00000010L
#define MB_ICONQUESTION      0x00000020L
#define MB_ICONEXCLAMATION   0x00000030L
#define MB_ICONASTERISK      0x00000040L
#define MB_USERICON          0x00000080L
#define MB_ICONWARNING       MB_ICONEXCLAMATION
#define MB_ICONERROR         MB_ICONHAND
#define MB_ICONINFORMATION   MB_ICONASTERISK
#define MB_ICONSTOP          MB_ICONHAND
#define MB_DEFBUTTON1        0x00000000L
#define MB_DEFBUTTON2        0x00000100L
#define MB_DEFBUTTON3        0x00000200L
#define MB_APPLMODAL         0x00000000L
#define MB_SYSTEMMODAL       0x00001000L
#define MB_TASKMODAL         0x00002000L
#define MB_HELP              0x00004000L
#define MB_SETFOREGROUND     0x00010000L
#define MB_TOPMOST           0x00040000L
#define MB_RIGHT             0x00080000L
#define MB_RTLREADING        0x00100000L

// ---- virtual keys ------------------------------------------------------------------
#define VK_LBUTTON    0x01
#define VK_RBUTTON    0x02
#define VK_CANCEL     0x03
#define VK_MBUTTON    0x04
#define VK_BACK       0x08
#define VK_TAB        0x09
#define VK_CLEAR      0x0C
#define VK_RETURN     0x0D
#define VK_SHIFT      0x10
#define VK_CONTROL    0x11
#define VK_MENU       0x12
#define VK_PAUSE      0x13
#define VK_CAPITAL    0x14
#define VK_ESCAPE     0x1B
#define VK_SPACE      0x20
#define VK_PRIOR      0x21
#define VK_NEXT       0x22
#define VK_END        0x23
#define VK_HOME       0x24
#define VK_LEFT       0x25
#define VK_UP         0x26
#define VK_RIGHT      0x27
#define VK_DOWN       0x28
#define VK_SELECT     0x29
#define VK_PRINT      0x2A
#define VK_EXECUTE    0x2B
#define VK_SNAPSHOT   0x2C
#define VK_INSERT     0x2D
#define VK_DELETE     0x2E
#define VK_HELP       0x2F
#define VK_LWIN       0x5B
#define VK_RWIN       0x5C
#define VK_APPS       0x5D
#define VK_NUMPAD0    0x60
#define VK_NUMPAD1    0x61
#define VK_NUMPAD2    0x62
#define VK_NUMPAD3    0x63
#define VK_NUMPAD4    0x64
#define VK_NUMPAD5    0x65
#define VK_NUMPAD6    0x66
#define VK_NUMPAD7    0x67
#define VK_NUMPAD8    0x68
#define VK_NUMPAD9    0x69
#define VK_MULTIPLY   0x6A
#define VK_ADD        0x6B
#define VK_SEPARATOR  0x6C
#define VK_SUBTRACT   0x6D
#define VK_DECIMAL    0x6E
#define VK_DIVIDE     0x6F
#define VK_F1         0x70
#define VK_F2         0x71
#define VK_F3         0x72
#define VK_F4         0x73
#define VK_F5         0x74
#define VK_F6         0x75
#define VK_F7         0x76
#define VK_F8         0x77
#define VK_F9         0x78
#define VK_F10        0x79
#define VK_F11        0x7A
#define VK_F12        0x7B
#define VK_F13        0x7C
#define VK_F14        0x7D
#define VK_F15        0x7E
#define VK_F16        0x7F
#define VK_F17        0x80
#define VK_F18        0x81
#define VK_F19        0x82
#define VK_F20        0x83
#define VK_F21        0x84
#define VK_F22        0x85
#define VK_F23        0x86
#define VK_F24        0x87
#define VK_NUMLOCK    0x90
#define VK_SCROLL     0x91
#define VK_LSHIFT     0xA0
#define VK_RSHIFT     0xA1
#define VK_LCONTROL   0xA2
#define VK_RCONTROL   0xA3
#define VK_LMENU      0xA4
#define VK_RMENU      0xA5
#define VK_OEM_1      0xBA
#define VK_OEM_PLUS   0xBB
#define VK_OEM_COMMA  0xBC
#define VK_OEM_MINUS  0xBD
#define VK_OEM_PERIOD 0xBE
#define VK_OEM_2      0xBF
#define VK_OEM_3      0xC0
#define VK_OEM_4      0xDB
#define VK_OEM_5      0xDC
#define VK_OEM_6      0xDD
#define VK_OEM_7      0xDE
#define VK_OEM_8      0xDF
#define VK_OEM_102    0xE2

#define MK_LBUTTON    0x0001
#define MK_RBUTTON    0x0002
#define MK_SHIFT      0x0004
#define MK_CONTROL    0x0008
#define MK_MBUTTON    0x0010
#define WHEEL_DELTA   120

#define MOD_ALT       0x0001
#define MOD_CONTROL   0x0002
#define MOD_SHIFT     0x0004
#define FVIRTKEY      TRUE
#define FNOINVERT     0x02
#define FSHIFT        0x04
#define FCONTROL      0x08
#define FALT          0x10

// ---- images, cursors, resources ----------------------------------------------------
#define IMAGE_BITMAP        0
#define IMAGE_ICON          1
#define IMAGE_CURSOR        2
#define IMAGE_ENHMETAFILE   3
#define LR_DEFAULTCOLOR     0x00000000
#define LR_MONOCHROME       0x00000001
#define LR_LOADFROMFILE     0x00000010
#define LR_LOADTRANSPARENT  0x00000020
#define LR_DEFAULTSIZE      0x00000040
#define LR_VGACOLOR         0x00000080
#define LR_LOADMAP3DCOLORS  0x00001000
#define LR_CREATEDIBSECTION 0x00002000
#define LR_SHARED           0x00008000

// ---- code pages, clipboard, memory ---------------------------------------------------
#define CP_ACP              0
#define CP_OEMCP            1
#define CP_MACCP            2
#define CP_THREAD_ACP       3
#define CP_UTF7             65000
#define CP_UTF8             65001
#define MB_PRECOMPOSED      0x00000001
#define MB_COMPOSITE        0x00000002
#define MB_USEGLYPHCHARS    0x00000004
#define MB_ERR_INVALID_CHARS 0x00000008
#define WC_COMPOSITECHECK   0x00000200
#define WC_NO_BEST_FIT_CHARS 0x00000400
#define WC_ERR_INVALID_CHARS 0x00000080

#define CF_TEXT             1
#define CF_BITMAP           2
#define CF_OEMTEXT          7
#define CF_DIB              8
#define CF_UNICODETEXT      13
#define CF_HDROP            15
#define CF_LOCALE           16

#define GMEM_FIXED          0x0000
#define GMEM_MOVEABLE       0x0002
#define GMEM_ZEROINIT       0x0040
#define GMEM_DDESHARE       0x2000
#define GMEM_SHARE          0x2000
#define GHND                (GMEM_MOVEABLE | GMEM_ZEROINIT)
#define GPTR                (GMEM_FIXED | GMEM_ZEROINIT)
#define LMEM_FIXED          0x0000
#define LMEM_MOVEABLE       0x0002
#define LMEM_ZEROINIT       0x0040
#define LHND                (LMEM_MOVEABLE | LMEM_ZEROINIT)
#define LPTR                (LMEM_FIXED | LMEM_ZEROINIT)
#define HEAP_ZERO_MEMORY    0x00000008

// ---- files ---------------------------------------------------------------------------
#define GENERIC_READ            0x80000000L
#define GENERIC_WRITE           0x40000000L
#define GENERIC_EXECUTE         0x20000000L
#define GENERIC_ALL             0x10000000L
#define FILE_SHARE_READ         0x00000001
#define FILE_SHARE_WRITE        0x00000002
#define FILE_SHARE_DELETE       0x00000004
#define CREATE_NEW              1
#define CREATE_ALWAYS           2
#define OPEN_EXISTING           3
#define OPEN_ALWAYS             4
#define TRUNCATE_EXISTING       5
#define FILE_ATTRIBUTE_READONLY  0x00000001
#define FILE_ATTRIBUTE_HIDDEN    0x00000002
#define FILE_ATTRIBUTE_SYSTEM    0x00000004
#define FILE_ATTRIBUTE_DIRECTORY 0x00000010
#define FILE_ATTRIBUTE_ARCHIVE   0x00000020
#define FILE_ATTRIBUTE_DEVICE    0x00000040
#define FILE_ATTRIBUTE_NORMAL    0x00000080
#define FILE_ATTRIBUTE_TEMPORARY 0x00000100
#define FILE_ATTRIBUTE_REPARSE_POINT 0x00000400
#define FILE_FLAG_WRITE_THROUGH  0x80000000
#define FILE_FLAG_OVERLAPPED     0x40000000
#define FILE_FLAG_NO_BUFFERING   0x20000000
#define FILE_FLAG_RANDOM_ACCESS  0x10000000
#define FILE_FLAG_SEQUENTIAL_SCAN 0x08000000
#define FILE_FLAG_DELETE_ON_CLOSE 0x04000000
#define FILE_FLAG_BACKUP_SEMANTICS 0x02000000
#define FILE_BEGIN              0
#define FILE_CURRENT            1
#define FILE_END                2
#define INVALID_FILE_SIZE       ((DWORD)0xFFFFFFFF)
#define INVALID_FILE_ATTRIBUTES ((DWORD)-1)
#define INVALID_SET_FILE_POINTER ((DWORD)-1)
#define MOVEFILE_REPLACE_EXISTING 0x00000001
#define MOVEFILE_COPY_ALLOWED     0x00000002

// ---- errors, results ---------------------------------------------------------------
#define ERROR_SUCCESS           0L
#define NO_ERROR                0L
#define ERROR_INVALID_FUNCTION  1L
#define ERROR_FILE_NOT_FOUND    2L
#define ERROR_PATH_NOT_FOUND    3L
#define ERROR_ACCESS_DENIED     5L
#define ERROR_INVALID_HANDLE    6L
#define ERROR_NOT_ENOUGH_MEMORY 8L
#define ERROR_NO_MORE_FILES     18L
#define ERROR_SHARING_VIOLATION 32L
#define ERROR_FILE_EXISTS       80L
#define ERROR_INVALID_PARAMETER 87L
#define ERROR_BROKEN_PIPE       109L
#define ERROR_INSUFFICIENT_BUFFER 122L
#define ERROR_ALREADY_EXISTS    183L
#define ERROR_MORE_DATA         234L
#define ERROR_NO_MORE_ITEMS     259L
#define ERROR_CANCELLED         1223L
#define S_OK                    ((HRESULT)0L)
#define S_FALSE                 ((HRESULT)1L)
#define E_NOTIMPL               ((HRESULT)0x80004001L)
#define E_NOINTERFACE           ((HRESULT)0x80004002L)
#define E_POINTER               ((HRESULT)0x80004003L)
#define E_FAIL                  ((HRESULT)0x80004005L)
#define E_UNEXPECTED            ((HRESULT)0x8000FFFFL)
#define E_OUTOFMEMORY           ((HRESULT)0x8007000EL)
#define E_INVALIDARG            ((HRESULT)0x80070057L)
#define E_ACCESSDENIED          ((HRESULT)0x80070005L)
#define SUCCEEDED(hr)           (((HRESULT)(hr)) >= 0)
#define FAILED(hr)              (((HRESULT)(hr)) < 0)

// ---- synchronization, processes --------------------------------------------------
#define INFINITE                0xFFFFFFFF
#define WAIT_OBJECT_0           0x00000000L
#define WAIT_ABANDONED          0x00000080L
#define WAIT_TIMEOUT            258L
#define WAIT_FAILED             ((DWORD)0xFFFFFFFF)
#define STILL_ACTIVE            259
#define CREATE_SUSPENDED        0x00000004
#define CREATE_NEW_CONSOLE      0x00000010
#define CREATE_UNICODE_ENVIRONMENT 0x00000400
#define CREATE_NO_WINDOW        0x08000000
#define DETACHED_PROCESS        0x00000008
#define NORMAL_PRIORITY_CLASS   0x00000020
#define STARTF_USESHOWWINDOW    0x00000001
#define STARTF_USESTDHANDLES    0x00000100
#define HANDLE_FLAG_INHERIT     0x00000001
#define STD_INPUT_HANDLE        ((DWORD)-10)
#define STD_OUTPUT_HANDLE       ((DWORD)-11)
#define STD_ERROR_HANDLE        ((DWORD)-12)
#define THREAD_PRIORITY_NORMAL  0
#define THREAD_PRIORITY_LOWEST  (-2)
#define THREAD_PRIORITY_BELOW_NORMAL (-1)
#define THREAD_PRIORITY_ABOVE_NORMAL 1
#define THREAD_PRIORITY_HIGHEST 2
#define FORMAT_MESSAGE_ALLOCATE_BUFFER 0x00000100
#define FORMAT_MESSAGE_IGNORE_INSERTS  0x00000200
#define FORMAT_MESSAGE_FROM_STRING     0x00000400
#define FORMAT_MESSAGE_FROM_HMODULE    0x00000800
#define FORMAT_MESSAGE_FROM_SYSTEM     0x00001000
#define FORMAT_MESSAGE_ARGUMENT_ARRAY  0x00002000
#define LANG_NEUTRAL            0x00
#define LANG_ENGLISH            0x09
#define SUBLANG_DEFAULT         0x01
#define SUBLANG_NEUTRAL         0x00
#define SUBLANG_ENGLISH_US      0x01
#define MAKELANGID(p, s)        ((((WORD)(s)) << 10) | (WORD)(p))
#define LOCALE_USER_DEFAULT     0x0400
#define LOCALE_SYSTEM_DEFAULT   0x0800
#define LOCALE_NAME_USER_DEFAULT nullptr
#define NORM_IGNORECASE         0x00000001
#define CSTR_LESS_THAN          1
#define CSTR_EQUAL              2
#define CSTR_GREATER_THAN       3
#define LCMAP_LOWERCASE         0x00000100
#define LCMAP_UPPERCASE         0x00000200

// ---- GDI -----------------------------------------------------------------------------
#define TRANSPARENT         1
#define OPAQUE              2
#define PS_SOLID            0
#define PS_DASH             1
#define PS_DOT              2
#define PS_DASHDOT          3
#define PS_DASHDOTDOT       4
#define PS_NULL             5
#define PS_INSIDEFRAME      6
#define BS_SOLID            0
#define BS_NULL             1
#define BS_HOLLOW           BS_NULL
#define WHITE_BRUSH         0
#define LTGRAY_BRUSH        1
#define GRAY_BRUSH          2
#define DKGRAY_BRUSH        3
#define BLACK_BRUSH         4
#define NULL_BRUSH          5
#define HOLLOW_BRUSH        NULL_BRUSH
#define WHITE_PEN           6
#define BLACK_PEN           7
#define NULL_PEN            8
#define OEM_FIXED_FONT      10
#define ANSI_FIXED_FONT     11
#define ANSI_VAR_FONT       12
#define SYSTEM_FONT         13
#define DEVICE_DEFAULT_FONT 14
#define SYSTEM_FIXED_FONT   16
#define DEFAULT_GUI_FONT    17
#define DC_BRUSH            18
#define DC_PEN              19
#define FW_DONTCARE         0
#define FW_THIN             100
#define FW_EXTRALIGHT       200
#define FW_LIGHT            300
#define FW_NORMAL           400
#define FW_REGULAR          400
#define FW_MEDIUM           500
#define FW_SEMIBOLD         600
#define FW_DEMIBOLD         600
#define FW_BOLD             700
#define FW_EXTRABOLD        800
#define FW_HEAVY            900
#define ANSI_CHARSET        0
#define DEFAULT_CHARSET     1
#define SYMBOL_CHARSET      2
#define OEM_CHARSET         255
#define OUT_DEFAULT_PRECIS  0
#define OUT_TT_PRECIS       4
#define OUT_OUTLINE_PRECIS  8
#define CLIP_DEFAULT_PRECIS 0
#define DEFAULT_QUALITY     0
#define DRAFT_QUALITY       1
#define PROOF_QUALITY       2
#define NONANTIALIASED_QUALITY 3
#define ANTIALIASED_QUALITY 4
#define CLEARTYPE_QUALITY   5
#define DEFAULT_PITCH       0
#define FIXED_PITCH         1
#define VARIABLE_PITCH      2
#define FF_DONTCARE         (0 << 4)
#define FF_ROMAN            (1 << 4)
#define FF_SWISS            (2 << 4)
#define FF_MODERN           (3 << 4)
#define FF_SCRIPT           (4 << 4)
#define FF_DECORATIVE       (5 << 4)
#define LF_FACESIZE         32
#define DT_TOP              0x00000000
#define DT_LEFT             0x00000000
#define DT_CENTER           0x00000001
#define DT_RIGHT            0x00000002
#define DT_VCENTER          0x00000004
#define DT_BOTTOM           0x00000008
#define DT_WORDBREAK        0x00000010
#define DT_SINGLELINE       0x00000020
#define DT_EXPANDTABS       0x00000040
#define DT_TABSTOP          0x00000080
#define DT_NOCLIP           0x00000100
#define DT_EXTERNALLEADING  0x00000200
#define DT_CALCRECT         0x00000400
#define DT_NOPREFIX         0x00000800
#define DT_INTERNAL         0x00001000
#define DT_EDITCONTROL      0x00002000
#define DT_PATH_ELLIPSIS    0x00004000
#define DT_END_ELLIPSIS     0x00008000
#define DT_MODIFYSTRING     0x00010000
#define DT_RTLREADING       0x00020000
#define DT_WORD_ELLIPSIS    0x00040000
#define DT_NOFULLWIDTHCHARBREAK 0x00080000
#define DT_HIDEPREFIX       0x00100000
#define DT_PREFIXONLY       0x00200000
#define SRCCOPY             0x00CC0020
#define SRCPAINT            0x00EE0086
#define SRCAND              0x008800C6
#define SRCINVERT           0x00660046
#define PATCOPY             0x00F00021
#define BLACKNESS           0x00000042
#define WHITENESS           0x00FF0062
#define HORZRES             8
#define VERTRES             10
#define BITSPIXEL           12
#define PLANES              14
#define LOGPIXELSX          88
#define LOGPIXELSY          90
#define ETO_OPAQUE          0x0002
#define ETO_CLIPPED         0x0004
#define BDR_RAISEDOUTER     0x0001
#define BDR_SUNKENOUTER     0x0002
#define BDR_RAISEDINNER     0x0004
#define BDR_SUNKENINNER     0x0008
#define EDGE_RAISED         (BDR_RAISEDOUTER | BDR_RAISEDINNER)
#define EDGE_SUNKEN         (BDR_SUNKENOUTER | BDR_SUNKENINNER)
#define EDGE_ETCHED         (BDR_SUNKENOUTER | BDR_RAISEDINNER)
#define EDGE_BUMP           (BDR_RAISEDOUTER | BDR_SUNKENINNER)
#define BF_LEFT             0x0001
#define BF_TOP              0x0002
#define BF_RIGHT            0x0004
#define BF_BOTTOM           0x0008
#define BF_RECT             (BF_LEFT | BF_TOP | BF_RIGHT | BF_BOTTOM)
#define BF_FLAT             0x4000
#define BF_MONO             0x8000
#define DFC_CAPTION         1
#define DFC_MENU            2
#define DFC_SCROLL          3
#define DFC_BUTTON          4
#define DFCS_BUTTONCHECK    0x0000
#define DFCS_BUTTONPUSH     0x0010
#define DFCS_CHECKED        0x0400
#define DFCS_INACTIVE       0x0100
#define DFCS_PUSHED         0x0200
#define ODT_MENU            1
#define ODT_LISTBOX         2
#define ODT_COMBOBOX        3
#define ODT_BUTTON          4
#define ODT_STATIC          5
#define ODA_DRAWENTIRE      0x0001
#define ODA_SELECT          0x0002
#define ODA_FOCUS           0x0004
#define ODS_SELECTED        0x0001
#define ODS_GRAYED          0x0002
#define ODS_DISABLED        0x0004
#define ODS_CHECKED         0x0008
#define ODS_FOCUS           0x0010
#define ODS_DEFAULT         0x0020
#define ODS_HOTLIGHT        0x0040
#define ODS_NOFOCUSRECT     0x0200
#define CLR_INVALID         0xFFFFFFFF
#define CLR_NONE            0xFFFFFFFFL
#define CLR_DEFAULT         0xFF000000L
#define MONITOR_DEFAULTTONULL    0x00000000
#define MONITOR_DEFAULTTOPRIMARY 0x00000001
#define MONITOR_DEFAULTTONEAREST 0x00000002

// ---- hooks ---------------------------------------------------------------------------
#define WH_KEYBOARD         2
#define WH_GETMESSAGE       3
#define WH_CALLWNDPROC      4
#define WH_MOUSE            7
#define WH_KEYBOARD_LL      13
#define WH_MOUSE_LL         14
#define HC_ACTION           0
#define PM_NOREMOVE         0x0000
#define PM_REMOVE           0x0001
