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

// Notepad++ Linux plugin SDK: the resources of a .rc file, as tools/rc2cpp.py
// writes them (dialogs, string tables, menus, accelerators, icons, bitmaps and
// other files). Each plugin has its own copy of the npp_win32 library, so the
// resource ids of different plugins do not meet.

#pragma once

#include <windows.h>

struct NppRcControl
{
	const wchar_t* className; // "Button", "Edit", "Static", "ComboBox", "SysListView32"...
	const wchar_t* text;
	int textId;               // ICON/CONTROL with a resource id as text (or -1)
	int id;
	short x, y, cx, cy;       // dialog units
	DWORD style;
	DWORD exStyle;
};

struct NppRcDialog
{
	int id;                   // -1: by name
	const wchar_t* name;
	short x, y, cx, cy;
	DWORD style;
	DWORD exStyle;
	const wchar_t* caption;
	const wchar_t* fontName;
	int fontSize;
	int fontWeight;
	bool fontItalic;
	int menuId;
	const NppRcControl* controls;
	int count;
};

struct NppRcString
{
	int id;
	const wchar_t* text;
};

struct NppRcMenuItem
{
	int depth;               // 0: items of the menu, 1: items of a POPUP of depth 0...
	bool popup;
	bool separator;
	const wchar_t* text;
	int id;
	UINT flags;              // MF_GRAYED, MF_CHECKED...
};

struct NppRcMenu
{
	int id;
	const wchar_t* name;
	const NppRcMenuItem* items;
	int count;
};

struct NppRcAccel
{
	BYTE fVirt;
	WORD key;
	WORD cmd;
};

struct NppRcAccelTable
{
	int id;
	const wchar_t* name;
	const NppRcAccel* items;
	int count;
};

struct NppRcData // ICON, BITMAP, PNG, RCDATA, CURSOR... with the bytes of the file
{
	int type;                // IMAGE_ICON, IMAGE_BITMAP, IMAGE_CURSOR, or 10 (RT_RCDATA) / other
	const wchar_t* typeName; // custom resource type ("PNG", "TEXT"...)
	int id;
	const wchar_t* name;
	const unsigned char* data;
	unsigned int size;
};

struct NppRcVersion
{
	DWORD fileVersionMS, fileVersionLS, productVersionMS, productVersionLS;
	const NppRcString* strings; // id unused: text is "Key\0Value"
	int count;
};

struct NppRcResources
{
	const NppRcDialog* dialogs;
	int dialogCount;
	const NppRcString* strings;
	int stringCount;
	const NppRcMenu* menus;
	int menuCount;
	const NppRcAccelTable* accels;
	int accelCount;
	const NppRcData* data;
	int dataCount;
	const NppRcVersion* version;
};

// called by the generated code at load time
void nppRegisterResources(const NppRcResources* res);
