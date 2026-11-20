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

// Notepad++ Linux plugin SDK: the common controls of Windows (list view, tree view,
// tab, trackbar, up-down, progress bar, tooltips, image lists) on Qt widgets.

#pragma once

#include <windows.h>

// ---- class names (strings: the .rc files use them too) -------------------------------
#ifdef RC_INVOKED
#define WC_LISTVIEW "SysListView32"
#define WC_TREEVIEW "SysTreeView32"
#define WC_TABCONTROL "SysTabControl32"
#define WC_HEADER "SysHeader32"
#define WC_LINK "SysLink"
#define WC_BUTTON "Button"
#define WC_STATIC "Static"
#define WC_EDIT "Edit"
#define WC_COMBOBOX "ComboBox"
#define WC_COMBOBOXEX "ComboBoxEx32"
#define WC_LISTBOX "ListBox"
#define WC_SCROLLBAR "ScrollBar"
#define TRACKBAR_CLASS "msctls_trackbar32"
#define UPDOWN_CLASS "msctls_updown32"
#define PROGRESS_CLASS "msctls_progress32"
#define TOOLTIPS_CLASS "tooltips_class32"
#define STATUSCLASSNAME "msctls_statusbar32"
#define TOOLBARCLASSNAME "ToolbarWindow32"
#define REBARCLASSNAME "ReBarWindow32"
#define HOTKEY_CLASS "msctls_hotkey32"
#define DATETIMEPICK_CLASS "SysDateTimePick32"
#define MONTHCAL_CLASS "SysMonthCal32"
#define WC_IPADDRESS "SysIPAddress32"
#else
#define WC_LISTVIEW L"SysListView32"
#define WC_TREEVIEW L"SysTreeView32"
#define WC_TABCONTROL L"SysTabControl32"
#define WC_HEADER L"SysHeader32"
#define WC_LINK L"SysLink"
#define WC_BUTTON L"Button"
#define WC_STATIC L"Static"
#define WC_EDIT L"Edit"
#define WC_COMBOBOX L"ComboBox"
#define WC_COMBOBOXEX L"ComboBoxEx32"
#define WC_LISTBOX L"ListBox"
#define WC_SCROLLBAR L"ScrollBar"
#define TRACKBAR_CLASS L"msctls_trackbar32"
#define UPDOWN_CLASS L"msctls_updown32"
#define PROGRESS_CLASS L"msctls_progress32"
#define TOOLTIPS_CLASS L"tooltips_class32"
#define STATUSCLASSNAME L"msctls_statusbar32"
#define TOOLBARCLASSNAME L"ToolbarWindow32"
#define REBARCLASSNAME L"ReBarWindow32"
#define HOTKEY_CLASS L"msctls_hotkey32"
#define DATETIMEPICK_CLASS L"SysDateTimePick32"
#define MONTHCAL_CLASS L"SysMonthCal32"
#define WC_IPADDRESS L"SysIPAddress32"
#endif
#define WC_LISTVIEWW WC_LISTVIEW
#define WC_TREEVIEWW WC_TREEVIEW
#define WC_TABCONTROLW WC_TABCONTROL
#define TRACKBAR_CLASSW TRACKBAR_CLASS
#define UPDOWN_CLASSW UPDOWN_CLASS
#define PROGRESS_CLASSW PROGRESS_CLASS
#define TOOLTIPS_CLASSW TOOLTIPS_CLASS

// ---- common notifications (WM_NOTIFY) ------------------------------------------------
#define NM_FIRST            (0U - 0U)
#define NM_OUTOFMEMORY      (NM_FIRST - 1)
#define NM_CLICK            (NM_FIRST - 2)
#define NM_DBLCLK           (NM_FIRST - 3)
#define NM_RETURN           (NM_FIRST - 4)
#define NM_RCLICK           (NM_FIRST - 5)
#define NM_RDBLCLK          (NM_FIRST - 6)
#define NM_SETFOCUS         (NM_FIRST - 7)
#define NM_KILLFOCUS        (NM_FIRST - 8)
#define NM_CUSTOMDRAW       (NM_FIRST - 12)
#define NM_HOVER            (NM_FIRST - 13)
#define NM_KEYDOWN          (NM_FIRST - 15)
#define NM_RELEASEDCAPTURE  (NM_FIRST - 16)
#define LVN_FIRST           (0U - 100U)
#define HDN_FIRST           (0U - 300U)
#define TVN_FIRST           (0U - 400U)
#define TTN_FIRST           (0U - 520U)
#define TCN_FIRST           (0U - 550U)
#define UDN_FIRST           (0U - 721U)
#define TRBN_FIRST          (0U - 1501U)

#define CCM_FIRST           0x2000
#define CCM_SETUNICODEFORMAT (CCM_FIRST + 5)
#define CCM_SETVERSION      (CCM_FIRST + 0x7)

#ifndef RC_INVOKED

struct INITCOMMONCONTROLSEX { DWORD dwSize; DWORD dwICC; };
typedef INITCOMMONCONTROLSEX* LPINITCOMMONCONTROLSEX;
BOOL InitCommonControlsEx(const INITCOMMONCONTROLSEX* icc);
void InitCommonControls();
#define ICC_LISTVIEW_CLASSES 0x00000001
#define ICC_TREEVIEW_CLASSES 0x00000002
#define ICC_BAR_CLASSES 0x00000004
#define ICC_TAB_CLASSES 0x00000008
#define ICC_UPDOWN_CLASS 0x00000010
#define ICC_PROGRESS_CLASS 0x00000020
#define ICC_HOTKEY_CLASS 0x00000040
#define ICC_ANIMATE_CLASS 0x00000080
#define ICC_WIN95_CLASSES 0x000000FF
#define ICC_DATE_CLASSES 0x00000100
#define ICC_USEREX_CLASSES 0x00000200
#define ICC_COOL_CLASSES 0x00000400
#define ICC_STANDARD_CLASSES 0x00004000
#define ICC_LINK_CLASS 0x00008000

// ---- window subclassing ---------------------------------------------------------------
typedef LRESULT (CALLBACK* SUBCLASSPROC)(HWND, UINT, WPARAM, LPARAM, UINT_PTR idSubclass, DWORD_PTR refData);
BOOL SetWindowSubclass(HWND hWnd, SUBCLASSPROC proc, UINT_PTR id, DWORD_PTR refData);
BOOL GetWindowSubclass(HWND hWnd, SUBCLASSPROC proc, UINT_PTR id, DWORD_PTR* refData);
BOOL RemoveWindowSubclass(HWND hWnd, SUBCLASSPROC proc, UINT_PTR id);
LRESULT DefSubclassProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

// ---- image lists -------------------------------------------------------------------
struct _IMAGELIST;
typedef _IMAGELIST* HIMAGELIST;
#define ILC_MASK 0x00000001
#define ILC_COLOR 0x00000000
#define ILC_COLOR4 0x00000004
#define ILC_COLOR8 0x00000008
#define ILC_COLOR16 0x00000010
#define ILC_COLOR24 0x00000018
#define ILC_COLOR32 0x00000020
#define ILD_NORMAL 0x00000000
#define ILD_TRANSPARENT 0x00000001
#define ILD_MASK 0x00000010
#define ILD_SELECTED 0x00000004
#define ILD_FOCUS 0x00000002
HIMAGELIST ImageList_Create(int cx, int cy, UINT flags, int initial, int grow);
BOOL ImageList_Destroy(HIMAGELIST il);
int ImageList_Add(HIMAGELIST il, HBITMAP image, HBITMAP mask);
int ImageList_AddMasked(HIMAGELIST il, HBITMAP image, COLORREF mask);
int ImageList_ReplaceIcon(HIMAGELIST il, int index, HICON icon);
#define ImageList_AddIcon(il, icon) ImageList_ReplaceIcon(il, -1, icon)
BOOL ImageList_Replace(HIMAGELIST il, int index, HBITMAP image, HBITMAP mask);
BOOL ImageList_Remove(HIMAGELIST il, int index);
#define ImageList_RemoveAll(il) ImageList_Remove(il, -1)
int ImageList_GetImageCount(HIMAGELIST il);
BOOL ImageList_SetImageCount(HIMAGELIST il, UINT count);
BOOL ImageList_GetIconSize(HIMAGELIST il, int* cx, int* cy);
BOOL ImageList_SetIconSize(HIMAGELIST il, int cx, int cy);
HICON ImageList_GetIcon(HIMAGELIST il, int index, UINT flags);
BOOL ImageList_Draw(HIMAGELIST il, int index, HDC hdc, int x, int y, UINT style);
BOOL ImageList_DrawEx(HIMAGELIST il, int index, HDC hdc, int x, int y, int dx, int dy, COLORREF bk, COLORREF fg, UINT style);
COLORREF ImageList_SetBkColor(HIMAGELIST il, COLORREF color);
HIMAGELIST ImageList_LoadImageW(HINSTANCE hInst, LPCWSTR name, int cx, int grow, COLORREF mask, UINT type, UINT flags);
#define ImageList_LoadImage ImageList_LoadImageW
#define ImageList_LoadBitmap(h, b, cx, g, m) ImageList_LoadImageW(h, b, cx, g, m, IMAGE_BITMAP, 0)

// ---- list view -------------------------------------------------------------------------
#define LVS_ICON 0x0000
#define LVS_REPORT 0x0001
#define LVS_SMALLICON 0x0002
#define LVS_LIST 0x0003
#define LVS_TYPEMASK 0x0003
#define LVS_SINGLESEL 0x0004
#define LVS_SHOWSELALWAYS 0x0008
#define LVS_SORTASCENDING 0x0010
#define LVS_SORTDESCENDING 0x0020
#define LVS_SHAREIMAGELISTS 0x0040
#define LVS_NOLABELWRAP 0x0080
#define LVS_AUTOARRANGE 0x0100
#define LVS_EDITLABELS 0x0200
#define LVS_OWNERDATA 0x1000
#define LVS_NOSCROLL 0x2000
#define LVS_ALIGNTOP 0x0000
#define LVS_OWNERDRAWFIXED 0x0400
#define LVS_NOCOLUMNHEADER 0x4000
#define LVS_NOSORTHEADER 0x8000
#define LVS_EX_GRIDLINES 0x00000001
#define LVS_EX_CHECKBOXES 0x00000004
#define LVS_EX_TRACKSELECT 0x00000008
#define LVS_EX_HEADERDRAGDROP 0x00000010
#define LVS_EX_FULLROWSELECT 0x00000020
#define LVS_EX_ONECLICKACTIVATE 0x00000040
#define LVS_EX_INFOTIP 0x00000400
#define LVS_EX_LABELTIP 0x00004000
#define LVS_EX_BORDERSELECT 0x00008000
#define LVS_EX_DOUBLEBUFFER 0x00010000
#define LVS_EX_HIDELABELS 0x00020000
#define LVS_EX_AUTOSIZECOLUMNS 0x10000000

#define LVM_FIRST 0x1000
#define LVM_GETBKCOLOR (LVM_FIRST + 0)
#define LVM_SETBKCOLOR (LVM_FIRST + 1)
#define LVM_GETIMAGELIST (LVM_FIRST + 2)
#define LVM_SETIMAGELIST (LVM_FIRST + 3)
#define LVM_GETITEMCOUNT (LVM_FIRST + 4)
#define LVM_DELETEITEM (LVM_FIRST + 8)
#define LVM_DELETEALLITEMS (LVM_FIRST + 9)
#define LVM_GETCALLBACKMASK (LVM_FIRST + 10)
#define LVM_SETCALLBACKMASK (LVM_FIRST + 11)
#define LVM_GETNEXTITEM (LVM_FIRST + 12)
#define LVM_SETITEMPOSITION (LVM_FIRST + 15)
#define LVM_GETITEMPOSITION (LVM_FIRST + 16)
#define LVM_HITTEST (LVM_FIRST + 18)
#define LVM_ENSUREVISIBLE (LVM_FIRST + 19)
#define LVM_SCROLL (LVM_FIRST + 20)
#define LVM_REDRAWITEMS (LVM_FIRST + 21)
#define LVM_ARRANGE (LVM_FIRST + 22)
#define LVM_GETEDITCONTROL (LVM_FIRST + 24)
#define LVM_DELETECOLUMN (LVM_FIRST + 28)
#define LVM_GETCOLUMNWIDTH (LVM_FIRST + 29)
#define LVM_SETCOLUMNWIDTH (LVM_FIRST + 30)
#define LVM_GETHEADER (LVM_FIRST + 31)
#define LVM_GETVIEWRECT (LVM_FIRST + 34)
#define LVM_GETTEXTCOLOR (LVM_FIRST + 35)
#define LVM_SETTEXTCOLOR (LVM_FIRST + 36)
#define LVM_GETTEXTBKCOLOR (LVM_FIRST + 37)
#define LVM_SETTEXTBKCOLOR (LVM_FIRST + 38)
#define LVM_GETTOPINDEX (LVM_FIRST + 39)
#define LVM_GETCOUNTPERPAGE (LVM_FIRST + 40)
#define LVM_GETORIGIN (LVM_FIRST + 41)
#define LVM_UPDATE (LVM_FIRST + 42)
#define LVM_SETITEMSTATE (LVM_FIRST + 43)
#define LVM_GETITEMSTATE (LVM_FIRST + 44)
#define LVM_SETITEMCOUNT (LVM_FIRST + 47)
#define LVM_SORTITEMS (LVM_FIRST + 48)
#define LVM_GETSELECTEDCOUNT (LVM_FIRST + 50)
#define LVM_GETITEMSPACING (LVM_FIRST + 51)
#define LVM_SETEXTENDEDLISTVIEWSTYLE (LVM_FIRST + 54)
#define LVM_GETEXTENDEDLISTVIEWSTYLE (LVM_FIRST + 55)
#define LVM_GETSUBITEMRECT (LVM_FIRST + 56)
#define LVM_SUBITEMHITTEST (LVM_FIRST + 57)
#define LVM_SETCOLUMNORDERARRAY (LVM_FIRST + 58)
#define LVM_GETCOLUMNORDERARRAY (LVM_FIRST + 59)
#define LVM_SETHOTITEM (LVM_FIRST + 60)
#define LVM_GETHOTITEM (LVM_FIRST + 61)
#define LVM_GETSELECTIONMARK (LVM_FIRST + 66)
#define LVM_SETSELECTIONMARK (LVM_FIRST + 67)
#define LVM_GETITEMRECT (LVM_FIRST + 14)
#define LVM_GETITEMW (LVM_FIRST + 75)
#define LVM_SETITEMW (LVM_FIRST + 76)
#define LVM_INSERTITEMW (LVM_FIRST + 77)
#define LVM_FINDITEMW (LVM_FIRST + 83)
#define LVM_GETSTRINGWIDTHW (LVM_FIRST + 87)
#define LVM_GETCOLUMNW (LVM_FIRST + 95)
#define LVM_SETCOLUMNW (LVM_FIRST + 96)
#define LVM_INSERTCOLUMNW (LVM_FIRST + 97)
#define LVM_GETITEMTEXTW (LVM_FIRST + 115)
#define LVM_SETITEMTEXTW (LVM_FIRST + 116)
#define LVM_EDITLABELW (LVM_FIRST + 118)
#define LVM_SORTITEMSEX (LVM_FIRST + 81)
#define LVM_ISITEMVISIBLE (LVM_FIRST + 182)
#define LVM_GETITEM LVM_GETITEMW
#define LVM_SETITEM LVM_SETITEMW
#define LVM_INSERTITEM LVM_INSERTITEMW
#define LVM_FINDITEM LVM_FINDITEMW
#define LVM_GETSTRINGWIDTH LVM_GETSTRINGWIDTHW
#define LVM_GETCOLUMN LVM_GETCOLUMNW
#define LVM_SETCOLUMN LVM_SETCOLUMNW
#define LVM_INSERTCOLUMN LVM_INSERTCOLUMNW
#define LVM_GETITEMTEXT LVM_GETITEMTEXTW
#define LVM_SETITEMTEXT LVM_SETITEMTEXTW
#define LVM_EDITLABEL LVM_EDITLABELW
#define LVSIL_NORMAL 0
#define LVSIL_SMALL 1
#define LVSIL_STATE 2
#define LVIF_TEXT 0x00000001
#define LVIF_IMAGE 0x00000002
#define LVIF_PARAM 0x00000004
#define LVIF_STATE 0x00000008
#define LVIF_INDENT 0x00000010
#define LVIF_NORECOMPUTE 0x00000800
#define LVIF_GROUPID 0x00000100
#define LVIF_COLUMNS 0x00000200
#define LVIS_FOCUSED 0x0001
#define LVIS_SELECTED 0x0002
#define LVIS_CUT 0x0004
#define LVIS_DROPHILITED 0x0008
#define LVIS_OVERLAYMASK 0x0F00
#define LVIS_STATEIMAGEMASK 0xF000
#define LVNI_ALL 0x0000
#define LVNI_FOCUSED 0x0001
#define LVNI_SELECTED 0x0002
#define LVNI_CUT 0x0004
#define LVNI_DROPHILITED 0x0008
#define LVNI_ABOVE 0x0100
#define LVNI_BELOW 0x0200
#define LVNI_TOLEFT 0x0400
#define LVNI_TORIGHT 0x0800
#define LVCF_FMT 0x0001
#define LVCF_WIDTH 0x0002
#define LVCF_TEXT 0x0004
#define LVCF_SUBITEM 0x0008
#define LVCF_IMAGE 0x0010
#define LVCF_ORDER 0x0020
#define LVCFMT_LEFT 0x0000
#define LVCFMT_RIGHT 0x0001
#define LVCFMT_CENTER 0x0002
#define LVCFMT_JUSTIFYMASK 0x0003
#define LVSCW_AUTOSIZE (-1)
#define LVSCW_AUTOSIZE_USEHEADER (-2)
#define LVHT_NOWHERE 0x00000001
#define LVHT_ONITEMICON 0x00000002
#define LVHT_ONITEMLABEL 0x00000004
#define LVHT_ONITEMSTATEICON 0x00000008
#define LVHT_ONITEM (LVHT_ONITEMICON | LVHT_ONITEMLABEL | LVHT_ONITEMSTATEICON)
#define LVIR_BOUNDS 0
#define LVIR_ICON 1
#define LVIR_LABEL 2
#define LVIR_SELECTBOUNDS 3
#define LVFI_PARAM 0x0001
#define LVFI_STRING 0x0002
#define LVFI_PARTIAL 0x0008
#define LVFI_WRAP 0x0020
#define LPSTR_TEXTCALLBACKW ((LPWSTR)(LONG_PTR)-1)
#define LPSTR_TEXTCALLBACK LPSTR_TEXTCALLBACKW
#define I_IMAGECALLBACK (-1)
#define I_IMAGENONE (-2)

struct LVITEMW
{
	UINT mask;
	int iItem;
	int iSubItem;
	UINT state;
	UINT stateMask;
	LPWSTR pszText;
	int cchTextMax;
	int iImage;
	LPARAM lParam;
	int iIndent;
	int iGroupId;
	UINT cColumns;
	PUINT puColumns;
	int* piColFmt;
	int iGroup;
};
typedef LVITEMW LVITEM;
typedef LVITEMW LV_ITEM;
typedef LVITEMW* LPLVITEMW;
typedef LVITEMW* LPLVITEM;
struct LVCOLUMNW
{
	UINT mask;
	int fmt;
	int cx;
	LPWSTR pszText;
	int cchTextMax;
	int iSubItem;
	int iImage;
	int iOrder;
	int cxMin;
	int cxDefault;
	int cxIdeal;
};
typedef LVCOLUMNW LVCOLUMN;
typedef LVCOLUMNW LV_COLUMN;
typedef LVCOLUMNW* LPLVCOLUMNW;
typedef LVCOLUMNW* LPLVCOLUMN;
struct LVHITTESTINFO
{
	POINT pt;
	UINT flags;
	int iItem;
	int iSubItem;
	int iGroup;
};
typedef LVHITTESTINFO* LPLVHITTESTINFO;
struct LVFINDINFOW
{
	UINT flags;
	LPCWSTR psz;
	LPARAM lParam;
	POINT pt;
	UINT vkDirection;
};
typedef LVFINDINFOW LVFINDINFO;
struct NMLISTVIEW
{
	NMHDR hdr;
	int iItem;
	int iSubItem;
	UINT uNewState;
	UINT uOldState;
	UINT uChanged;
	POINT ptAction;
	LPARAM lParam;
};
typedef NMLISTVIEW NM_LISTVIEW;
typedef NMLISTVIEW* LPNMLISTVIEW;
struct NMITEMACTIVATE
{
	NMHDR hdr;
	int iItem;
	int iSubItem;
	UINT uNewState;
	UINT uOldState;
	UINT uChanged;
	POINT ptAction;
	LPARAM lParam;
	UINT uKeyFlags;
};
typedef NMITEMACTIVATE* LPNMITEMACTIVATE;
struct NMLVKEYDOWN
{
	NMHDR hdr;
	WORD wVKey;
	UINT flags;
};
typedef NMLVKEYDOWN* LPNMLVKEYDOWN;
struct NMLVDISPINFOW
{
	NMHDR hdr;
	LVITEMW item;
};
typedef NMLVDISPINFOW NMLVDISPINFO;
typedef NMLVDISPINFOW LV_DISPINFO;
struct NMLVGETINFOTIPW
{
	NMHDR hdr;
	DWORD dwFlags;
	LPWSTR pszText;
	int cchTextMax;
	int iItem;
	int iSubItem;
	LPARAM lParam;
};
typedef NMLVGETINFOTIPW NMLVGETINFOTIP;
typedef NMLVGETINFOTIPW* LPNMLVGETINFOTIP;
#define LVN_ITEMCHANGING (LVN_FIRST - 0)
#define LVN_ITEMCHANGED (LVN_FIRST - 1)
#define LVN_INSERTITEM (LVN_FIRST - 2)
#define LVN_DELETEITEM (LVN_FIRST - 3)
#define LVN_DELETEALLITEMS (LVN_FIRST - 4)
#define LVN_COLUMNCLICK (LVN_FIRST - 8)
#define LVN_BEGINDRAG (LVN_FIRST - 9)
#define LVN_BEGINRDRAG (LVN_FIRST - 11)
#define LVN_ITEMACTIVATE (LVN_FIRST - 14)
#define LVN_ODCACHEHINT (LVN_FIRST - 13)
#define LVN_HOTTRACK (LVN_FIRST - 21)
#define LVN_KEYDOWN (LVN_FIRST - 55)
#define LVN_MARQUEEBEGIN (LVN_FIRST - 56)
#define LVN_GETINFOTIPW (LVN_FIRST - 58)
#define LVN_GETINFOTIP LVN_GETINFOTIPW
#define LVN_BEGINLABELEDITW (LVN_FIRST - 75)
#define LVN_ENDLABELEDITW (LVN_FIRST - 76)
#define LVN_GETDISPINFOW (LVN_FIRST - 77)
#define LVN_SETDISPINFOW (LVN_FIRST - 78)
#define LVN_BEGINLABELEDIT LVN_BEGINLABELEDITW
#define LVN_ENDLABELEDIT LVN_ENDLABELEDITW
#define LVN_GETDISPINFO LVN_GETDISPINFOW
#define LVN_SETDISPINFO LVN_SETDISPINFOW
typedef int (CALLBACK* PFNLVCOMPARE)(LPARAM, LPARAM, LPARAM);

#define ListView_GetItemCount(h) (int)SendMessage((h), LVM_GETITEMCOUNT, 0, 0)
#define ListView_InsertItem(h, pi) (int)SendMessage((h), LVM_INSERTITEM, 0, (LPARAM)(pi))
#define ListView_SetItem(h, pi) (BOOL)SendMessage((h), LVM_SETITEM, 0, (LPARAM)(pi))
#define ListView_GetItem(h, pi) (BOOL)SendMessage((h), LVM_GETITEM, 0, (LPARAM)(pi))
#define ListView_DeleteItem(h, i) (BOOL)SendMessage((h), LVM_DELETEITEM, (WPARAM)(int)(i), 0)
#define ListView_DeleteAllItems(h) (BOOL)SendMessage((h), LVM_DELETEALLITEMS, 0, 0)
#define ListView_InsertColumn(h, i, pc) (int)SendMessage((h), LVM_INSERTCOLUMN, (WPARAM)(int)(i), (LPARAM)(pc))
#define ListView_DeleteColumn(h, i) (BOOL)SendMessage((h), LVM_DELETECOLUMN, (WPARAM)(int)(i), 0)
#define ListView_SetColumn(h, i, pc) (BOOL)SendMessage((h), LVM_SETCOLUMN, (WPARAM)(int)(i), (LPARAM)(pc))
#define ListView_GetColumn(h, i, pc) (BOOL)SendMessage((h), LVM_GETCOLUMN, (WPARAM)(int)(i), (LPARAM)(pc))
#define ListView_SetColumnWidth(h, i, cx) (BOOL)SendMessage((h), LVM_SETCOLUMNWIDTH, (WPARAM)(int)(i), MAKELPARAM((cx), 0))
#define ListView_GetColumnWidth(h, i) (int)SendMessage((h), LVM_GETCOLUMNWIDTH, (WPARAM)(int)(i), 0)
#define ListView_GetNextItem(h, i, f) (int)SendMessage((h), LVM_GETNEXTITEM, (WPARAM)(int)(i), MAKELPARAM((f), 0))
#define ListView_GetSelectedCount(h) (UINT)SendMessage((h), LVM_GETSELECTEDCOUNT, 0, 0)
#define ListView_GetSelectionMark(h) (int)SendMessage((h), LVM_GETSELECTIONMARK, 0, 0)
#define ListView_SetSelectionMark(h, i) (int)SendMessage((h), LVM_SETSELECTIONMARK, 0, (LPARAM)(i))
#define ListView_EnsureVisible(h, i, p) (BOOL)SendMessage((h), LVM_ENSUREVISIBLE, (WPARAM)(int)(i), MAKELPARAM((p), 0))
#define ListView_SetExtendedListViewStyle(h, s) (DWORD)SendMessage((h), LVM_SETEXTENDEDLISTVIEWSTYLE, 0, (LPARAM)(s))
#define ListView_SetExtendedListViewStyleEx(h, m, s) (DWORD)SendMessage((h), LVM_SETEXTENDEDLISTVIEWSTYLE, (WPARAM)(m), (LPARAM)(s))
#define ListView_GetExtendedListViewStyle(h) (DWORD)SendMessage((h), LVM_GETEXTENDEDLISTVIEWSTYLE, 0, 0)
#define ListView_SetImageList(h, il, t) (HIMAGELIST)SendMessage((h), LVM_SETIMAGELIST, (WPARAM)(t), (LPARAM)(il))
#define ListView_GetImageList(h, t) (HIMAGELIST)SendMessage((h), LVM_GETIMAGELIST, (WPARAM)(t), 0)
#define ListView_GetTopIndex(h) (int)SendMessage((h), LVM_GETTOPINDEX, 0, 0)
#define ListView_GetCountPerPage(h) (int)SendMessage((h), LVM_GETCOUNTPERPAGE, 0, 0)
#define ListView_RedrawItems(h, f, l) (BOOL)SendMessage((h), LVM_REDRAWITEMS, (WPARAM)(int)(f), (LPARAM)(int)(l))
#define ListView_Update(h, i) (BOOL)SendMessage((h), LVM_UPDATE, (WPARAM)(i), 0)
#define ListView_HitTest(h, p) (int)SendMessage((h), LVM_HITTEST, 0, (LPARAM)(LPLVHITTESTINFO)(p))
#define ListView_SubItemHitTest(h, p) (int)SendMessage((h), LVM_SUBITEMHITTEST, 0, (LPARAM)(LPLVHITTESTINFO)(p))
#define ListView_SetItemCount(h, n) SendMessage((h), LVM_SETITEMCOUNT, (WPARAM)(n), 0)
#define ListView_SetItemCountEx(h, n, f) SendMessage((h), LVM_SETITEMCOUNT, (WPARAM)(n), (LPARAM)(f))
#define ListView_SortItems(h, f, l) (BOOL)SendMessage((h), LVM_SORTITEMS, (WPARAM)(LPARAM)(l), (LPARAM)(PFNLVCOMPARE)(f))
#define ListView_SortItemsEx(h, f, l) (BOOL)SendMessage((h), LVM_SORTITEMSEX, (WPARAM)(LPARAM)(l), (LPARAM)(PFNLVCOMPARE)(f))
#define ListView_GetHeader(h) (HWND)SendMessage((h), LVM_GETHEADER, 0, 0)
#define ListView_SetBkColor(h, c) (BOOL)SendMessage((h), LVM_SETBKCOLOR, 0, (LPARAM)(COLORREF)(c))
#define ListView_SetTextColor(h, c) (BOOL)SendMessage((h), LVM_SETTEXTCOLOR, 0, (LPARAM)(COLORREF)(c))
#define ListView_SetTextBkColor(h, c) (BOOL)SendMessage((h), LVM_SETTEXTBKCOLOR, 0, (LPARAM)(COLORREF)(c))
#define ListView_GetEditControl(h) (HWND)SendMessage((h), LVM_GETEDITCONTROL, 0, 0)
#define ListView_EditLabel(h, i) (HWND)SendMessage((h), LVM_EDITLABEL, (WPARAM)(int)(i), 0)
#define ListView_FindItem(h, s, p) (int)SendMessage((h), LVM_FINDITEM, (WPARAM)(int)(s), (LPARAM)(p))
#define ListView_GetStringWidth(h, s) (int)SendMessage((h), LVM_GETSTRINGWIDTH, 0, (LPARAM)(s))
inline void ListView_SetItemState(HWND h, int i, UINT data, UINT mask)
{
	LVITEMW item{};
	item.stateMask = mask;
	item.state = data;
	SendMessageW(h, LVM_SETITEMSTATE, (WPARAM)i, (LPARAM)&item);
}
#define ListView_GetItemState(h, i, m) (UINT)SendMessage((h), LVM_GETITEMSTATE, (WPARAM)(i), (LPARAM)(m))
inline void ListView_SetItemText(HWND h, int i, int sub, LPCWSTR text)
{
	LVITEMW item{};
	item.iSubItem = sub;
	item.pszText = const_cast<LPWSTR>(text);
	SendMessageW(h, LVM_SETITEMTEXT, (WPARAM)i, (LPARAM)&item);
}
inline void ListView_GetItemText(HWND h, int i, int sub, LPWSTR text, int max)
{
	LVITEMW item{};
	item.iSubItem = sub;
	item.cchTextMax = max;
	item.pszText = text;
	SendMessageW(h, LVM_GETITEMTEXT, (WPARAM)i, (LPARAM)&item);
}
inline BOOL ListView_GetItemRect(HWND h, int i, RECT* rc, int code)
{
	if (rc)
		rc->left = code;
	return (BOOL)SendMessageW(h, LVM_GETITEMRECT, (WPARAM)i, (LPARAM)rc);
}
inline BOOL ListView_GetSubItemRect(HWND h, int i, int sub, int code, RECT* rc)
{
	if (rc)
	{
		rc->top = sub;
		rc->left = code;
	}
	return (BOOL)SendMessageW(h, LVM_GETSUBITEMRECT, (WPARAM)i, (LPARAM)rc);
}
#define ListView_SetCheckState(h, i, c) ListView_SetItemState(h, i, INDEXTOSTATEIMAGEMASK((c) ? 2 : 1), LVIS_STATEIMAGEMASK)
#define ListView_GetCheckState(h, i) ((((UINT)(SendMessage((h), LVM_GETITEMSTATE, (WPARAM)(i), LVIS_STATEIMAGEMASK))) >> 12) - 1)

// ---- header -----------------------------------------------------------------------------
#define HDM_FIRST 0x1200
#define HDM_GETITEMCOUNT (HDM_FIRST + 0)
#define HDM_GETITEMRECT (HDM_FIRST + 7)
#define HDN_ITEMCLICKW (HDN_FIRST - 22)
#define HDN_ITEMCLICK HDN_ITEMCLICKW
#define Header_GetItemCount(h) (int)SendMessage((h), HDM_GETITEMCOUNT, 0, 0)

// ---- tree view --------------------------------------------------------------------------
struct _TREEITEM;
typedef _TREEITEM* HTREEITEM;
#define TVI_ROOT ((HTREEITEM)(ULONG_PTR)-0x10000)
#define TVI_FIRST ((HTREEITEM)(ULONG_PTR)-0x0FFFF)
#define TVI_LAST ((HTREEITEM)(ULONG_PTR)-0x0FFFE)
#define TVI_SORT ((HTREEITEM)(ULONG_PTR)-0x0FFFD)
#define TVS_HASBUTTONS 0x0001
#define TVS_HASLINES 0x0002
#define TVS_LINESATROOT 0x0004
#define TVS_EDITLABELS 0x0008
#define TVS_DISABLEDRAGDROP 0x0010
#define TVS_SHOWSELALWAYS 0x0020
#define TVS_RTLREADING 0x0040
#define TVS_NOTOOLTIPS 0x0080
#define TVS_CHECKBOXES 0x0100
#define TVS_TRACKSELECT 0x0200
#define TVS_SINGLEEXPAND 0x0400
#define TVS_INFOTIP 0x0800
#define TVS_FULLROWSELECT 0x1000
#define TVS_NOSCROLL 0x2000
#define TVS_NONEVENHEIGHT 0x4000
#define TVS_NOHSCROLL 0x8000
#define TVS_EX_DOUBLEBUFFER 0x0004
#define TVIF_TEXT 0x0001
#define TVIF_IMAGE 0x0002
#define TVIF_PARAM 0x0004
#define TVIF_STATE 0x0008
#define TVIF_HANDLE 0x0010
#define TVIF_SELECTEDIMAGE 0x0020
#define TVIF_CHILDREN 0x0040
#define TVIF_INTEGRAL 0x0080
#define TVIS_SELECTED 0x0002
#define TVIS_CUT 0x0004
#define TVIS_DROPHILITED 0x0008
#define TVIS_BOLD 0x0010
#define TVIS_EXPANDED 0x0020
#define TVIS_EXPANDEDONCE 0x0040
#define TVIS_EXPANDPARTIAL 0x0080
#define TVIS_OVERLAYMASK 0x0F00
#define TVIS_STATEIMAGEMASK 0xF000
#define TVIS_USERMASK 0xF000
#define I_CHILDRENCALLBACK (-1)
#define TVGN_ROOT 0x0000
#define TVGN_NEXT 0x0001
#define TVGN_PREVIOUS 0x0002
#define TVGN_PARENT 0x0003
#define TVGN_CHILD 0x0004
#define TVGN_FIRSTVISIBLE 0x0005
#define TVGN_NEXTVISIBLE 0x0006
#define TVGN_PREVIOUSVISIBLE 0x0007
#define TVGN_DROPHILITE 0x0008
#define TVGN_CARET 0x0009
#define TVGN_LASTVISIBLE 0x000A
#define TVE_COLLAPSE 0x0001
#define TVE_EXPAND 0x0002
#define TVE_TOGGLE 0x0003
#define TVE_EXPANDPARTIAL 0x4000
#define TVE_COLLAPSERESET 0x8000
#define TVSIL_NORMAL 0
#define TVSIL_STATE 2
#define TVHT_NOWHERE 0x0001
#define TVHT_ONITEMICON 0x0002
#define TVHT_ONITEMLABEL 0x0004
#define TVHT_ONITEM (TVHT_ONITEMICON | TVHT_ONITEMLABEL | TVHT_ONITEMSTATEICON)
#define TVHT_ONITEMINDENT 0x0008
#define TVHT_ONITEMBUTTON 0x0010
#define TVHT_ONITEMRIGHT 0x0020
#define TVHT_ONITEMSTATEICON 0x0040
#define TVC_UNKNOWN 0x0000
#define TVC_BYMOUSE 0x0001
#define TVC_BYKEYBOARD 0x0002
#define TV_FIRST 0x1100
#define TVM_INSERTITEMW (TV_FIRST + 50)
#define TVM_DELETEITEM (TV_FIRST + 1)
#define TVM_EXPAND (TV_FIRST + 2)
#define TVM_GETITEMRECT (TV_FIRST + 4)
#define TVM_GETCOUNT (TV_FIRST + 5)
#define TVM_GETINDENT (TV_FIRST + 6)
#define TVM_SETINDENT (TV_FIRST + 7)
#define TVM_GETIMAGELIST (TV_FIRST + 8)
#define TVM_SETIMAGELIST (TV_FIRST + 9)
#define TVM_GETNEXTITEM (TV_FIRST + 10)
#define TVM_SELECTITEM (TV_FIRST + 11)
#define TVM_GETITEMW (TV_FIRST + 62)
#define TVM_SETITEMW (TV_FIRST + 63)
#define TVM_EDITLABELW (TV_FIRST + 65)
#define TVM_GETEDITCONTROL (TV_FIRST + 15)
#define TVM_GETVISIBLECOUNT (TV_FIRST + 16)
#define TVM_HITTEST (TV_FIRST + 17)
#define TVM_CREATEDRAGIMAGE (TV_FIRST + 18)
#define TVM_SORTCHILDREN (TV_FIRST + 19)
#define TVM_ENSUREVISIBLE (TV_FIRST + 20)
#define TVM_SORTCHILDRENCB (TV_FIRST + 21)
#define TVM_ENDEDITLABELNOW (TV_FIRST + 22)
#define TVM_SETITEMHEIGHT (TV_FIRST + 27)
#define TVM_GETITEMHEIGHT (TV_FIRST + 28)
#define TVM_SETBKCOLOR (TV_FIRST + 29)
#define TVM_SETTEXTCOLOR (TV_FIRST + 30)
#define TVM_GETBKCOLOR (TV_FIRST + 31)
#define TVM_GETTEXTCOLOR (TV_FIRST + 32)
#define TVM_SETLINECOLOR (TV_FIRST + 40)
#define TVM_GETITEMSTATE (TV_FIRST + 39)
#define TVM_SETEXTENDEDSTYLE (TV_FIRST + 44)
#define TVM_GETEXTENDEDSTYLE (TV_FIRST + 45)
#define TVM_INSERTITEM TVM_INSERTITEMW
#define TVM_GETITEM TVM_GETITEMW
#define TVM_SETITEM TVM_SETITEMW
#define TVM_EDITLABEL TVM_EDITLABELW

struct TVITEMW
{
	UINT mask;
	HTREEITEM hItem;
	UINT state;
	UINT stateMask;
	LPWSTR pszText;
	int cchTextMax;
	int iImage;
	int iSelectedImage;
	int cChildren;
	LPARAM lParam;
};
typedef TVITEMW TVITEM;
typedef TVITEMW TV_ITEM;
typedef TVITEMW* LPTVITEMW;
typedef TVITEMW* LPTVITEM;
struct TVITEMEXW
{
	UINT mask;
	HTREEITEM hItem;
	UINT state;
	UINT stateMask;
	LPWSTR pszText;
	int cchTextMax;
	int iImage;
	int iSelectedImage;
	int cChildren;
	LPARAM lParam;
	int iIntegral;
	UINT uStateEx;
	HWND hwnd;
	int iExpandedImage;
	int iReserved;
};
typedef TVITEMEXW TVITEMEX;
struct TVINSERTSTRUCTW
{
	HTREEITEM hParent;
	HTREEITEM hInsertAfter;
	union
	{
		TVITEMEXW itemex;
		TVITEMW item;
	};
};
typedef TVINSERTSTRUCTW TVINSERTSTRUCT;
typedef TVINSERTSTRUCTW TV_INSERTSTRUCT;
typedef TVINSERTSTRUCTW* LPTVINSERTSTRUCTW;
typedef TVINSERTSTRUCTW* LPTVINSERTSTRUCT;
struct TVHITTESTINFO
{
	POINT pt;
	UINT flags;
	HTREEITEM hItem;
};
typedef TVHITTESTINFO* LPTVHITTESTINFO;
struct NMTREEVIEWW
{
	NMHDR hdr;
	UINT action;
	TVITEMW itemOld;
	TVITEMW itemNew;
	POINT ptDrag;
};
typedef NMTREEVIEWW NMTREEVIEW;
typedef NMTREEVIEWW NM_TREEVIEW;
typedef NMTREEVIEWW* LPNMTREEVIEWW;
typedef NMTREEVIEWW* LPNMTREEVIEW;
struct NMTVDISPINFOW
{
	NMHDR hdr;
	TVITEMW item;
};
typedef NMTVDISPINFOW NMTVDISPINFO;
typedef NMTVDISPINFOW TV_DISPINFO;
typedef NMTVDISPINFOW* LPNMTVDISPINFOW;
typedef NMTVDISPINFOW* LPNMTVDISPINFO;
struct NMTVKEYDOWN
{
	NMHDR hdr;
	WORD wVKey;
	UINT flags;
};
typedef NMTVKEYDOWN* LPNMTVKEYDOWN;
struct NMTVGETINFOTIPW
{
	NMHDR hdr;
	LPWSTR pszText;
	int cchTextMax;
	HTREEITEM hItem;
	LPARAM lParam;
};
typedef NMTVGETINFOTIPW NMTVGETINFOTIP;
typedef NMTVGETINFOTIPW* LPNMTVGETINFOTIP;
struct TVSORTCB
{
	HTREEITEM hParent;
	int (CALLBACK* lpfnCompare)(LPARAM, LPARAM, LPARAM);
	LPARAM lParam;
};
typedef TVSORTCB* LPTVSORTCB;
#define TVN_SELCHANGINGW (TVN_FIRST - 50)
#define TVN_SELCHANGEDW (TVN_FIRST - 51)
#define TVN_GETDISPINFOW (TVN_FIRST - 52)
#define TVN_SETDISPINFOW (TVN_FIRST - 53)
#define TVN_ITEMEXPANDINGW (TVN_FIRST - 54)
#define TVN_ITEMEXPANDEDW (TVN_FIRST - 55)
#define TVN_BEGINDRAGW (TVN_FIRST - 56)
#define TVN_BEGINRDRAGW (TVN_FIRST - 57)
#define TVN_DELETEITEMW (TVN_FIRST - 58)
#define TVN_BEGINLABELEDITW (TVN_FIRST - 59)
#define TVN_ENDLABELEDITW (TVN_FIRST - 60)
#define TVN_KEYDOWN (TVN_FIRST - 12)
#define TVN_GETINFOTIPW (TVN_FIRST - 14)
#define TVN_ITEMCHANGINGW (TVN_FIRST - 17)
#define TVN_ITEMCHANGEDW (TVN_FIRST - 19)
#define TVN_SELCHANGING TVN_SELCHANGINGW
#define TVN_SELCHANGED TVN_SELCHANGEDW
#define TVN_GETDISPINFO TVN_GETDISPINFOW
#define TVN_SETDISPINFO TVN_SETDISPINFOW
#define TVN_ITEMEXPANDING TVN_ITEMEXPANDINGW
#define TVN_ITEMEXPANDED TVN_ITEMEXPANDEDW
#define TVN_BEGINDRAG TVN_BEGINDRAGW
#define TVN_BEGINRDRAG TVN_BEGINRDRAGW
#define TVN_DELETEITEM TVN_DELETEITEMW
#define TVN_BEGINLABELEDIT TVN_BEGINLABELEDITW
#define TVN_ENDLABELEDIT TVN_ENDLABELEDITW
#define TVN_GETINFOTIP TVN_GETINFOTIPW
#define TVN_ITEMCHANGING TVN_ITEMCHANGINGW
#define TVN_ITEMCHANGED TVN_ITEMCHANGEDW

#define TreeView_InsertItem(h, l) (HTREEITEM)SendMessage((h), TVM_INSERTITEM, 0, (LPARAM)(LPTVINSERTSTRUCT)(l))
#define TreeView_DeleteItem(h, i) (BOOL)SendMessage((h), TVM_DELETEITEM, 0, (LPARAM)(HTREEITEM)(i))
#define TreeView_DeleteAllItems(h) (BOOL)SendMessage((h), TVM_DELETEITEM, 0, (LPARAM)TVI_ROOT)
#define TreeView_Expand(h, i, c) (BOOL)SendMessage((h), TVM_EXPAND, (WPARAM)(c), (LPARAM)(HTREEITEM)(i))
#define TreeView_GetCount(h) (UINT)SendMessage((h), TVM_GETCOUNT, 0, 0)
#define TreeView_GetNextItem(h, i, c) (HTREEITEM)SendMessage((h), TVM_GETNEXTITEM, (WPARAM)(c), (LPARAM)(HTREEITEM)(i))
#define TreeView_GetChild(h, i) TreeView_GetNextItem(h, i, TVGN_CHILD)
#define TreeView_GetNextSibling(h, i) TreeView_GetNextItem(h, i, TVGN_NEXT)
#define TreeView_GetPrevSibling(h, i) TreeView_GetNextItem(h, i, TVGN_PREVIOUS)
#define TreeView_GetParent(h, i) TreeView_GetNextItem(h, i, TVGN_PARENT)
#define TreeView_GetFirstVisible(h) TreeView_GetNextItem(h, NULL, TVGN_FIRSTVISIBLE)
#define TreeView_GetNextVisible(h, i) TreeView_GetNextItem(h, i, TVGN_NEXTVISIBLE)
#define TreeView_GetPrevVisible(h, i) TreeView_GetNextItem(h, i, TVGN_PREVIOUSVISIBLE)
#define TreeView_GetSelection(h) TreeView_GetNextItem(h, NULL, TVGN_CARET)
#define TreeView_GetDropHilight(h) TreeView_GetNextItem(h, NULL, TVGN_DROPHILITE)
#define TreeView_GetRoot(h) TreeView_GetNextItem(h, NULL, TVGN_ROOT)
#define TreeView_GetLastVisible(h) TreeView_GetNextItem(h, NULL, TVGN_LASTVISIBLE)
#define TreeView_Select(h, i, c) (BOOL)SendMessage((h), TVM_SELECTITEM, (WPARAM)(c), (LPARAM)(HTREEITEM)(i))
#define TreeView_SelectItem(h, i) TreeView_Select(h, i, TVGN_CARET)
#define TreeView_SelectDropTarget(h, i) TreeView_Select(h, i, TVGN_DROPHILITE)
#define TreeView_SelectSetFirstVisible(h, i) TreeView_Select(h, i, TVGN_FIRSTVISIBLE)
#define TreeView_GetItem(h, p) (BOOL)SendMessage((h), TVM_GETITEM, 0, (LPARAM)(TV_ITEM*)(p))
#define TreeView_SetItem(h, p) (BOOL)SendMessage((h), TVM_SETITEM, 0, (LPARAM)(const TV_ITEM*)(p))
#define TreeView_EditLabel(h, i) (HWND)SendMessage((h), TVM_EDITLABEL, 0, (LPARAM)(HTREEITEM)(i))
#define TreeView_GetEditControl(h) (HWND)SendMessage((h), TVM_GETEDITCONTROL, 0, 0)
#define TreeView_EndEditLabelNow(h, c) (BOOL)SendMessage((h), TVM_ENDEDITLABELNOW, (WPARAM)(c), 0)
#define TreeView_HitTest(h, p) (HTREEITEM)SendMessage((h), TVM_HITTEST, 0, (LPARAM)(LPTVHITTESTINFO)(p))
#define TreeView_EnsureVisible(h, i) (BOOL)SendMessage((h), TVM_ENSUREVISIBLE, 0, (LPARAM)(HTREEITEM)(i))
#define TreeView_SortChildren(h, i, r) (BOOL)SendMessage((h), TVM_SORTCHILDREN, (WPARAM)(r), (LPARAM)(HTREEITEM)(i))
#define TreeView_SortChildrenCB(h, p, r) (BOOL)SendMessage((h), TVM_SORTCHILDRENCB, (WPARAM)(r), (LPARAM)(LPTVSORTCB)(p))
#define TreeView_SetImageList(h, il, t) (HIMAGELIST)SendMessage((h), TVM_SETIMAGELIST, (WPARAM)(t), (LPARAM)(HIMAGELIST)(il))
#define TreeView_GetImageList(h, t) (HIMAGELIST)SendMessage((h), TVM_GETIMAGELIST, (WPARAM)(t), 0)
#define TreeView_GetItemState(h, i, m) (UINT)SendMessage((h), TVM_GETITEMSTATE, (WPARAM)(i), (LPARAM)(m))
#define TreeView_SetItemHeight(h, cy) (int)SendMessage((h), TVM_SETITEMHEIGHT, (WPARAM)(cy), 0)
#define TreeView_GetItemHeight(h) (int)SendMessage((h), TVM_GETITEMHEIGHT, 0, 0)
#define TreeView_SetBkColor(h, c) (COLORREF)SendMessage((h), TVM_SETBKCOLOR, 0, (LPARAM)(c))
#define TreeView_SetTextColor(h, c) (COLORREF)SendMessage((h), TVM_SETTEXTCOLOR, 0, (LPARAM)(c))
#define TreeView_SetLineColor(h, c) (COLORREF)SendMessage((h), TVM_SETLINECOLOR, 0, (LPARAM)(c))
#define TreeView_GetVisibleCount(h) (UINT)SendMessage((h), TVM_GETVISIBLECOUNT, 0, 0)
#define TreeView_SetExtendedStyle(h, s, m) (DWORD)SendMessage((h), TVM_SETEXTENDEDSTYLE, (WPARAM)(m), (LPARAM)(s))
inline BOOL TreeView_GetItemRect(HWND h, HTREEITEM item, RECT* rc, BOOL textOnly)
{
	if (rc)
		*reinterpret_cast<HTREEITEM*>(rc) = item;
	return (BOOL)SendMessageW(h, TVM_GETITEMRECT, (WPARAM)textOnly, (LPARAM)rc);
}
#define TreeView_SetCheckState(h, i, c) do { TVITEMW _tvi{}; _tvi.mask = TVIF_HANDLE | TVIF_STATE; _tvi.hItem = (i); _tvi.stateMask = TVIS_STATEIMAGEMASK; _tvi.state = INDEXTOSTATEIMAGEMASK((c) ? 2 : 1); TreeView_SetItem(h, &_tvi); } while (0)
#define TreeView_GetCheckState(h, i) ((((UINT)(SendMessage((h), TVM_GETITEMSTATE, (WPARAM)(i), TVIS_STATEIMAGEMASK))) >> 12) - 1)

// ---- tab control ------------------------------------------------------------------------
#define TCS_SCROLLOPPOSITE 0x0001
#define TCS_BOTTOM 0x0002
#define TCS_RIGHT 0x0002
#define TCS_MULTISELECT 0x0004
#define TCS_FLATBUTTONS 0x0008
#define TCS_FORCEICONLEFT 0x0010
#define TCS_FORCELABELLEFT 0x0020
#define TCS_HOTTRACK 0x0040
#define TCS_VERTICAL 0x0080
#define TCS_TABS 0x0000
#define TCS_BUTTONS 0x0100
#define TCS_SINGLELINE 0x0000
#define TCS_MULTILINE 0x0200
#define TCS_FIXEDWIDTH 0x0400
#define TCS_FOCUSNEVER 0x8000
#define TCS_OWNERDRAWFIXED 0x2000
#define TCS_TOOLTIPS 0x4000
#define TCM_FIRST 0x1300
#define TCM_GETIMAGELIST (TCM_FIRST + 2)
#define TCM_SETIMAGELIST (TCM_FIRST + 3)
#define TCM_GETITEMCOUNT (TCM_FIRST + 4)
#define TCM_GETITEMW (TCM_FIRST + 60)
#define TCM_SETITEMW (TCM_FIRST + 61)
#define TCM_INSERTITEMW (TCM_FIRST + 62)
#define TCM_DELETEITEM (TCM_FIRST + 8)
#define TCM_DELETEALLITEMS (TCM_FIRST + 9)
#define TCM_GETITEMRECT (TCM_FIRST + 10)
#define TCM_GETCURSEL (TCM_FIRST + 11)
#define TCM_SETCURSEL (TCM_FIRST + 12)
#define TCM_HITTEST (TCM_FIRST + 13)
#define TCM_ADJUSTRECT (TCM_FIRST + 40)
#define TCM_SETITEMSIZE (TCM_FIRST + 41)
#define TCM_SETPADDING (TCM_FIRST + 43)
#define TCM_GETROWCOUNT (TCM_FIRST + 44)
#define TCM_GETCURFOCUS (TCM_FIRST + 47)
#define TCM_SETCURFOCUS (TCM_FIRST + 48)
#define TCM_GETITEM TCM_GETITEMW
#define TCM_SETITEM TCM_SETITEMW
#define TCM_INSERTITEM TCM_INSERTITEMW
#define TCIF_TEXT 0x0001
#define TCIF_IMAGE 0x0002
#define TCIF_RTLREADING 0x0004
#define TCIF_PARAM 0x0008
#define TCIF_STATE 0x0010
struct TCITEMW
{
	UINT mask;
	DWORD dwState;
	DWORD dwStateMask;
	LPWSTR pszText;
	int cchTextMax;
	int iImage;
	LPARAM lParam;
};
typedef TCITEMW TCITEM;
typedef TCITEMW TC_ITEM;
typedef TCITEMW* LPTCITEMW;
typedef TCITEMW* LPTCITEM;
struct TCHITTESTINFO
{
	POINT pt;
	UINT flags;
};
#define TCN_KEYDOWN (TCN_FIRST - 0)
#define TCN_SELCHANGE (TCN_FIRST - 1)
#define TCN_SELCHANGING (TCN_FIRST - 2)
#define TabCtrl_GetItemCount(h) (int)SendMessage((h), TCM_GETITEMCOUNT, 0, 0)
#define TabCtrl_InsertItem(h, i, p) (int)SendMessage((h), TCM_INSERTITEM, (WPARAM)(int)(i), (LPARAM)(const TC_ITEM*)(p))
#define TabCtrl_SetItem(h, i, p) (BOOL)SendMessage((h), TCM_SETITEM, (WPARAM)(int)(i), (LPARAM)(TC_ITEM*)(p))
#define TabCtrl_GetItem(h, i, p) (BOOL)SendMessage((h), TCM_GETITEM, (WPARAM)(int)(i), (LPARAM)(TC_ITEM*)(p))
#define TabCtrl_DeleteItem(h, i) (BOOL)SendMessage((h), TCM_DELETEITEM, (WPARAM)(int)(i), 0)
#define TabCtrl_DeleteAllItems(h) (BOOL)SendMessage((h), TCM_DELETEALLITEMS, 0, 0)
#define TabCtrl_GetCurSel(h) (int)SendMessage((h), TCM_GETCURSEL, 0, 0)
#define TabCtrl_SetCurSel(h, i) (int)SendMessage((h), TCM_SETCURSEL, (WPARAM)(i), 0)
#define TabCtrl_GetItemRect(h, i, p) (BOOL)SendMessage((h), TCM_GETITEMRECT, (WPARAM)(int)(i), (LPARAM)(RECT*)(p))
#define TabCtrl_AdjustRect(h, b, p) (int)SendMessage((h), TCM_ADJUSTRECT, (WPARAM)(BOOL)(b), (LPARAM)(RECT*)(p))
#define TabCtrl_SetImageList(h, il) (HIMAGELIST)SendMessage((h), TCM_SETIMAGELIST, 0, (LPARAM)(HIMAGELIST)(il))
#define TabCtrl_HitTest(h, p) (int)SendMessage((h), TCM_HITTEST, 0, (LPARAM)(TCHITTESTINFO*)(p))
#define TabCtrl_GetRowCount(h) (int)SendMessage((h), TCM_GETROWCOUNT, 0, 0)

// ---- trackbar, up-down, progress bar -------------------------------------------------------
#define TBS_AUTOTICKS 0x0001
#define TBS_VERT 0x0002
#define TBS_HORZ 0x0000
#define TBS_TOP 0x0004
#define TBS_BOTTOM 0x0000
#define TBS_LEFT 0x0004
#define TBS_RIGHT 0x0000
#define TBS_BOTH 0x0008
#define TBS_NOTICKS 0x0010
#define TBS_ENABLESELRANGE 0x0020
#define TBS_FIXEDLENGTH 0x0040
#define TBS_NOTHUMB 0x0080
#define TBS_TOOLTIPS 0x0100
#define TBM_GETPOS (WM_USER)
#define TBM_GETRANGEMIN (WM_USER + 1)
#define TBM_GETRANGEMAX (WM_USER + 2)
#define TBM_GETTIC (WM_USER + 3)
#define TBM_SETTIC (WM_USER + 4)
#define TBM_SETPOS (WM_USER + 5)
#define TBM_SETRANGE (WM_USER + 6)
#define TBM_SETRANGEMIN (WM_USER + 7)
#define TBM_SETRANGEMAX (WM_USER + 8)
#define TBM_CLEARTICS (WM_USER + 9)
#define TBM_SETSEL (WM_USER + 10)
#define TBM_SETPAGESIZE (WM_USER + 21)
#define TBM_GETPAGESIZE (WM_USER + 22)
#define TBM_SETLINESIZE (WM_USER + 23)
#define TBM_GETLINESIZE (WM_USER + 24)
#define TBM_SETTICFREQ (WM_USER + 20)
#define TBM_SETPOSNOTIFY (WM_USER + 34)
#define TB_LINEUP 0
#define TB_LINEDOWN 1
#define TB_PAGEUP 2
#define TB_PAGEDOWN 3
#define TB_THUMBPOSITION 4
#define TB_THUMBTRACK 5
#define TB_TOP 6
#define TB_BOTTOM 7
#define TB_ENDTRACK 8
#define UDS_WRAP 0x0001
#define UDS_SETBUDDYINT 0x0002
#define UDS_ALIGNRIGHT 0x0004
#define UDS_ALIGNLEFT 0x0008
#define UDS_AUTOBUDDY 0x0010
#define UDS_ARROWKEYS 0x0020
#define UDS_HORZ 0x0040
#define UDS_NOTHOUSANDS 0x0080
#define UDS_HOTTRACK 0x0100
#define UDM_SETRANGE (WM_USER + 101)
#define UDM_GETRANGE (WM_USER + 102)
#define UDM_SETPOS (WM_USER + 103)
#define UDM_GETPOS (WM_USER + 104)
#define UDM_SETBUDDY (WM_USER + 105)
#define UDM_GETBUDDY (WM_USER + 106)
#define UDM_SETACCEL (WM_USER + 107)
#define UDM_SETBASE (WM_USER + 109)
#define UDM_SETRANGE32 (WM_USER + 111)
#define UDM_GETRANGE32 (WM_USER + 112)
#define UDM_SETPOS32 (WM_USER + 113)
#define UDM_GETPOS32 (WM_USER + 114)
#define UD_MAXVAL 0x7fff
#define UD_MINVAL (-UD_MAXVAL)
struct NMUPDOWN
{
	NMHDR hdr;
	int iPos;
	int iDelta;
};
typedef NMUPDOWN NM_UPDOWN;
typedef NMUPDOWN* LPNMUPDOWN;
#define UDN_DELTAPOS (UDN_FIRST - 1)
#define PBS_SMOOTH 0x01
#define PBS_VERTICAL 0x04
#define PBS_MARQUEE 0x08
#define PBM_SETRANGE (WM_USER + 1)
#define PBM_SETPOS (WM_USER + 2)
#define PBM_DELTAPOS (WM_USER + 3)
#define PBM_SETSTEP (WM_USER + 4)
#define PBM_STEPIT (WM_USER + 5)
#define PBM_SETRANGE32 (WM_USER + 6)
#define PBM_GETRANGE (WM_USER + 7)
#define PBM_GETPOS (WM_USER + 8)
#define PBM_SETBARCOLOR (WM_USER + 9)
#define PBM_SETMARQUEE (WM_USER + 10)
#define PBM_SETBKCOLOR CCM_SETBKCOLOR
#define CCM_SETBKCOLOR (CCM_FIRST + 1)
#define PBM_SETSTATE (WM_USER + 16)
#define PBST_NORMAL 0x0001
#define PBST_ERROR 0x0002
#define PBST_PAUSED 0x0003

// ---- tooltips -----------------------------------------------------------------------------
#define TTS_ALWAYSTIP 0x01
#define TTS_NOPREFIX 0x02
#define TTS_NOANIMATE 0x10
#define TTS_NOFADE 0x20
#define TTS_BALLOON 0x40
#define TTS_CLOSE 0x80
#define TTF_IDISHWND 0x0001
#define TTF_CENTERTIP 0x0002
#define TTF_RTLREADING 0x0004
#define TTF_SUBCLASS 0x0010
#define TTF_TRACK 0x0020
#define TTF_ABSOLUTE 0x0080
#define TTF_TRANSPARENT 0x0100
#define TTF_PARSELINKS 0x1000
#define TTM_ACTIVATE (WM_USER + 1)
#define TTM_SETDELAYTIME (WM_USER + 3)
#define TTM_ADDTOOLW (WM_USER + 50)
#define TTM_DELTOOLW (WM_USER + 51)
#define TTM_NEWTOOLRECTW (WM_USER + 52)
#define TTM_GETTOOLINFOW (WM_USER + 53)
#define TTM_SETTOOLINFOW (WM_USER + 54)
#define TTM_UPDATETIPTEXTW (WM_USER + 57)
#define TTM_TRACKACTIVATE (WM_USER + 17)
#define TTM_TRACKPOSITION (WM_USER + 18)
#define TTM_SETMAXTIPWIDTH (WM_USER + 24)
#define TTM_POP (WM_USER + 28)
#define TTM_SETTITLEW (WM_USER + 33)
#define TTM_ADDTOOL TTM_ADDTOOLW
#define TTM_DELTOOL TTM_DELTOOLW
#define TTM_NEWTOOLRECT TTM_NEWTOOLRECTW
#define TTM_GETTOOLINFO TTM_GETTOOLINFOW
#define TTM_SETTOOLINFO TTM_SETTOOLINFOW
#define TTM_UPDATETIPTEXT TTM_UPDATETIPTEXTW
#define TTM_SETTITLE TTM_SETTITLEW
#define TTDT_AUTOMATIC 0
#define TTDT_RESHOW 1
#define TTDT_AUTOPOP 2
#define TTDT_INITIAL 3
#define TTI_NONE 0
#define TTI_INFO 1
#define TTI_WARNING 2
#define TTI_ERROR 3
struct TTTOOLINFOW
{
	UINT cbSize;
	UINT uFlags;
	HWND hwnd;
	UINT_PTR uId;
	RECT rect;
	HINSTANCE hinst;
	LPWSTR lpszText;
	LPARAM lParam;
	void* lpReserved;
};
typedef TTTOOLINFOW TOOLINFO;
typedef TTTOOLINFOW TOOLINFOW;
typedef TTTOOLINFOW TTTOOLINFO;
typedef TTTOOLINFOW* LPTOOLINFO;
struct NMTTDISPINFOW
{
	NMHDR hdr;
	LPWSTR lpszText;
	WCHAR szText[80];
	HINSTANCE hinst;
	UINT uFlags;
	LPARAM lParam;
};
typedef NMTTDISPINFOW NMTTDISPINFO;
typedef NMTTDISPINFOW TOOLTIPTEXT;
typedef NMTTDISPINFOW* LPNMTTDISPINFO;
typedef NMTTDISPINFOW* LPTOOLTIPTEXT;
#define TTN_GETDISPINFOW (TTN_FIRST - 10)
#define TTN_GETDISPINFO TTN_GETDISPINFOW
#define TTN_NEEDTEXT TTN_GETDISPINFOW
#define TTN_SHOW (TTN_FIRST - 1)
#define TTN_POP (TTN_FIRST - 2)
#define TTN_LINKCLICK (TTN_FIRST - 3)

// ---- SysLink, edit balloon, custom draw ----------------------------------------------------
#define LM_SETITEM (WM_USER + 0x302)
#define LIF_ITEMINDEX 0x00000001
#define LIF_STATE 0x00000002
#define LIF_ITEMID 0x00000004
#define LIF_URL 0x00000008
#define MAX_LINKID_TEXT 48
#define L_MAX_URL_LENGTH (2048 + 32 + 4)
struct LITEM
{
	UINT mask;
	int iLink;
	UINT state;
	UINT stateMask;
	WCHAR szID[MAX_LINKID_TEXT];
	WCHAR szUrl[L_MAX_URL_LENGTH];
};
struct NMLINK
{
	NMHDR hdr;
	LITEM item;
};
typedef NMLINK* PNMLINK;
#define NM_LDOWN NM_CLICK
struct EDITBALLOONTIP
{
	DWORD cbStruct;
	LPCWSTR pszTitle;
	LPCWSTR pszText;
	INT ttiIcon;
};
#define Edit_ShowBalloonTip(h, p) (BOOL)SendMessage((h), EM_SHOWBALLOONTIP, 0, (LPARAM)(p))
#define Edit_SetCueBannerText(h, t) (BOOL)SendMessage((h), EM_SETCUEBANNER, 0, (LPARAM)(t))
#define ComboBox_SetCueBannerText(h, t) (BOOL)SendMessage((h), CB_SETCUEBANNER, 0, (LPARAM)(t))
#define CDDS_PREPAINT 0x00000001
#define CDDS_POSTPAINT 0x00000002
#define CDDS_ITEM 0x00010000
#define CDDS_SUBITEM 0x00020000
#define CDDS_ITEMPREPAINT (CDDS_ITEM | CDDS_PREPAINT)
#define CDDS_ITEMPOSTPAINT (CDDS_ITEM | CDDS_POSTPAINT)
#define CDRF_DODEFAULT 0x00000000
#define CDRF_NEWFONT 0x00000002
#define CDRF_SKIPDEFAULT 0x00000004
#define CDRF_NOTIFYPOSTPAINT 0x00000010
#define CDRF_NOTIFYITEMDRAW 0x00000020
#define CDRF_NOTIFYSUBITEMDRAW 0x00000020
#define CDIS_SELECTED 0x0001
#define CDIS_FOCUS 0x0010
#define CDIS_HOT 0x0040
struct NMCUSTOMDRAW
{
	NMHDR hdr;
	DWORD dwDrawStage;
	HDC hdc;
	RECT rc;
	DWORD_PTR dwItemSpec;
	UINT uItemState;
	LPARAM lItemlParam;
};
typedef NMCUSTOMDRAW* LPNMCUSTOMDRAW;
struct NMLVCUSTOMDRAW
{
	NMCUSTOMDRAW nmcd;
	COLORREF clrText;
	COLORREF clrTextBk;
	int iSubItem;
};
typedef NMLVCUSTOMDRAW* LPNMLVCUSTOMDRAW;
struct NMTVCUSTOMDRAW
{
	NMCUSTOMDRAW nmcd;
	COLORREF clrText;
	COLORREF clrTextBk;
	int iLevel;
};
typedef NMTVCUSTOMDRAW* LPNMTVCUSTOMDRAW;

// ---- status bar (minimal) ------------------------------------------------------------------
#define SB_SETTEXTW (WM_USER + 11)
#define SB_GETTEXTW (WM_USER + 13)
#define SB_SETPARTS (WM_USER + 4)
#define SB_SIMPLE (WM_USER + 9)
#define SB_SETTEXT SB_SETTEXTW
#define SB_GETTEXT SB_GETTEXTW
#define SBARS_SIZEGRIP 0x0100

#endif // RC_INVOKED
