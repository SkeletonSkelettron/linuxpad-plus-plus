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

// Notepad++ Linux plugin SDK: the message crackers of windowsx.h that plugins use.

#pragma once

#include <windows.h>

#ifndef RC_INVOKED
#define Button_GetCheck(h) ((int)(DWORD)SendMessage((h), BM_GETCHECK, 0, 0))
#define Button_SetCheck(h, c) ((void)SendMessage((h), BM_SETCHECK, (WPARAM)(int)(c), 0))
#define Button_Enable(h, e) EnableWindow((h), (e))
#define Button_SetText(h, t) SetWindowText((h), (t))
#define Edit_GetText(h, t, n) GetWindowText((h), (t), (n))
#define Edit_SetText(h, t) SetWindowText((h), (t))
#define Edit_GetTextLength(h) GetWindowTextLength(h)
#define Edit_LimitText(h, n) ((void)SendMessage((h), EM_LIMITTEXT, (WPARAM)(n), 0))
#define Edit_SetSel(h, s, e) ((void)SendMessage((h), EM_SETSEL, (s), (e)))
#define Edit_ReplaceSel(h, t) ((void)SendMessage((h), EM_REPLACESEL, 0, (LPARAM)(LPCTSTR)(t)))
#define Edit_SetReadOnly(h, r) ((BOOL)SendMessage((h), EM_SETREADONLY, (WPARAM)(BOOL)(r), 0))
#define Edit_Enable(h, e) EnableWindow((h), (e))
#define ComboBox_AddString(h, s) ((int)(DWORD)SendMessage((h), CB_ADDSTRING, 0, (LPARAM)(LPCTSTR)(s)))
#define ComboBox_InsertString(h, i, s) ((int)(DWORD)SendMessage((h), CB_INSERTSTRING, (WPARAM)(int)(i), (LPARAM)(LPCTSTR)(s)))
#define ComboBox_DeleteString(h, i) ((int)(DWORD)SendMessage((h), CB_DELETESTRING, (WPARAM)(int)(i), 0))
#define ComboBox_ResetContent(h) ((int)(DWORD)SendMessage((h), CB_RESETCONTENT, 0, 0))
#define ComboBox_GetCount(h) ((int)(DWORD)SendMessage((h), CB_GETCOUNT, 0, 0))
#define ComboBox_GetCurSel(h) ((int)(DWORD)SendMessage((h), CB_GETCURSEL, 0, 0))
#define ComboBox_SetCurSel(h, i) ((int)(DWORD)SendMessage((h), CB_SETCURSEL, (WPARAM)(int)(i), 0))
#define ComboBox_GetLBText(h, i, s) ((int)(DWORD)SendMessage((h), CB_GETLBTEXT, (WPARAM)(int)(i), (LPARAM)(LPCTSTR)(s)))
#define ComboBox_GetLBTextLen(h, i) ((int)(DWORD)SendMessage((h), CB_GETLBTEXTLEN, (WPARAM)(int)(i), 0))
#define ComboBox_GetItemData(h, i) ((LRESULT)(ULONG_PTR)SendMessage((h), CB_GETITEMDATA, (WPARAM)(int)(i), 0))
#define ComboBox_SetItemData(h, i, d) ((int)(DWORD)SendMessage((h), CB_SETITEMDATA, (WPARAM)(int)(i), (LPARAM)(d)))
#define ComboBox_FindString(h, i, s) ((int)(DWORD)SendMessage((h), CB_FINDSTRING, (WPARAM)(int)(i), (LPARAM)(LPCTSTR)(s)))
#define ComboBox_FindStringExact(h, i, s) ((int)(DWORD)SendMessage((h), CB_FINDSTRINGEXACT, (WPARAM)(int)(i), (LPARAM)(LPCTSTR)(s)))
#define ComboBox_SelectString(h, i, s) ((int)(DWORD)SendMessage((h), CB_SELECTSTRING, (WPARAM)(int)(i), (LPARAM)(LPCTSTR)(s)))
#define ComboBox_GetText(h, t, n) GetWindowText((h), (t), (n))
#define ComboBox_SetText(h, t) SetWindowText((h), (t))
#define ComboBox_GetTextLength(h) GetWindowTextLength(h)
#define ComboBox_Enable(h, e) EnableWindow((h), (e))
#define ComboBox_ShowDropdown(h, s) ((BOOL)(DWORD)SendMessage((h), CB_SHOWDROPDOWN, (WPARAM)(BOOL)(s), 0))
#define ComboBox_LimitText(h, n) ((int)(DWORD)SendMessage((h), CB_LIMITTEXT, (WPARAM)(int)(n), 0))
#define ListBox_AddString(h, s) ((int)(DWORD)SendMessage((h), LB_ADDSTRING, 0, (LPARAM)(LPCTSTR)(s)))
#define ListBox_InsertString(h, i, s) ((int)(DWORD)SendMessage((h), LB_INSERTSTRING, (WPARAM)(int)(i), (LPARAM)(LPCTSTR)(s)))
#define ListBox_DeleteString(h, i) ((int)(DWORD)SendMessage((h), LB_DELETESTRING, (WPARAM)(int)(i), 0))
#define ListBox_ResetContent(h) ((BOOL)(DWORD)SendMessage((h), LB_RESETCONTENT, 0, 0))
#define ListBox_GetCount(h) ((int)(DWORD)SendMessage((h), LB_GETCOUNT, 0, 0))
#define ListBox_GetCurSel(h) ((int)(DWORD)SendMessage((h), LB_GETCURSEL, 0, 0))
#define ListBox_SetCurSel(h, i) ((int)(DWORD)SendMessage((h), LB_SETCURSEL, (WPARAM)(int)(i), 0))
#define ListBox_GetText(h, i, s) ((int)(DWORD)SendMessage((h), LB_GETTEXT, (WPARAM)(int)(i), (LPARAM)(LPCTSTR)(s)))
#define ListBox_GetTextLen(h, i) ((int)(DWORD)SendMessage((h), LB_GETTEXTLEN, (WPARAM)(int)(i), 0))
#define ListBox_GetItemData(h, i) ((LRESULT)(ULONG_PTR)SendMessage((h), LB_GETITEMDATA, (WPARAM)(int)(i), 0))
#define ListBox_SetItemData(h, i, d) ((int)(DWORD)SendMessage((h), LB_SETITEMDATA, (WPARAM)(int)(i), (LPARAM)(d)))
#define ListBox_GetSel(h, i) ((int)(DWORD)SendMessage((h), LB_GETSEL, (WPARAM)(int)(i), 0))
#define ListBox_SetSel(h, s, i) ((int)(DWORD)SendMessage((h), LB_SETSEL, (WPARAM)(BOOL)(s), (LPARAM)(i)))
#define ListBox_GetSelCount(h) ((int)(DWORD)SendMessage((h), LB_GETSELCOUNT, 0, 0))
#define ListBox_GetSelItems(h, n, p) ((int)(DWORD)SendMessage((h), LB_GETSELITEMS, (WPARAM)(int)(n), (LPARAM)(int*)(p)))
#define ListBox_FindString(h, i, s) ((int)(DWORD)SendMessage((h), LB_FINDSTRING, (WPARAM)(int)(i), (LPARAM)(LPCTSTR)(s)))
#define ListBox_FindStringExact(h, i, s) ((int)(DWORD)SendMessage((h), LB_FINDSTRINGEXACT, (WPARAM)(int)(i), (LPARAM)(LPCTSTR)(s)))
#define ListBox_SetTopIndex(h, i) ((int)(DWORD)SendMessage((h), LB_SETTOPINDEX, (WPARAM)(int)(i), 0))
#define ListBox_GetTopIndex(h) ((int)(DWORD)SendMessage((h), LB_GETTOPINDEX, 0, 0))
#define Static_SetText(h, t) SetWindowText((h), (t))
#define Static_GetText(h, t, n) GetWindowText((h), (t), (n))
#define Static_SetIcon(h, i) ((HICON)(UINT_PTR)SendMessage((h), STM_SETICON, (WPARAM)(HICON)(i), 0))
#define Static_Enable(h, e) EnableWindow((h), (e))
#define ScrollBar_SetPos(h, p, r) SetScrollPos((h), SB_CTL, (p), (r))
#define ScrollBar_GetPos(h) GetScrollPos((h), SB_CTL)
#define SetWindowFont(h, f, r) ((void)SendMessage((h), WM_SETFONT, (WPARAM)(HFONT)(f), (LPARAM)(BOOL)(r)))
#define GetWindowFont(h) ((HFONT)(UINT_PTR)SendMessage((h), WM_GETFONT, 0, 0))
#define GetWindowStyle(h) ((DWORD)GetWindowLong((h), GWL_STYLE))
#define GetWindowExStyle(h) ((DWORD)GetWindowLong((h), GWL_EXSTYLE))
#define GetWindowID(h) GetDlgCtrlID(h)
#define SubclassWindow(h, p) ((WNDPROC)SetWindowLongPtr((h), GWLP_WNDPROC, (LPARAM)(WNDPROC)(p)))
#define SelectFont(dc, f) ((HFONT)SelectObject((dc), (HGDIOBJ)(HFONT)(f)))
#define SelectBrush(dc, b) ((HBRUSH)SelectObject((dc), (HGDIOBJ)(HBRUSH)(b)))
#define SelectPen(dc, p) ((HPEN)SelectObject((dc), (HGDIOBJ)(HPEN)(p)))
#define SelectBitmap(dc, b) ((HBITMAP)SelectObject((dc), (HGDIOBJ)(HBITMAP)(b)))
#define DeleteFont(f) DeleteObject((HGDIOBJ)(HFONT)(f))
#define DeleteBrush(b) DeleteObject((HGDIOBJ)(HBRUSH)(b))
#define DeletePen(p) DeleteObject((HGDIOBJ)(HPEN)(p))
#define DeleteBitmap(b) DeleteObject((HGDIOBJ)(HBITMAP)(b))
#define GetStockFont(i) ((HFONT)GetStockObject(i))
#define GetStockBrush(i) ((HBRUSH)GetStockObject(i))
#define HANDLE_MSG(hwnd, message, fn) case (message): return HANDLE_##message((hwnd), (wParam), (lParam), (fn))
#endif
