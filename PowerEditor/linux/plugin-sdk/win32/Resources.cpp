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

// Notepad++ Linux plugin SDK, npp_win32 library: the resources of the .rc file of
// the plugin (tools/rc2cpp.py). FindResource / LoadResource / LockResource of a
// dialog give a DLGTEMPLATE that CreateDialogIndirectParam takes back (the RTL dialogs
// of the plugin template).

#include <vector>

#include "W32Internal.h"

namespace w32
{
	namespace
	{
		std::vector<const NppRcResources*>& registry()
		{
			static std::vector<const NppRcResources*> r;
			return r;
		}

		bool sameName(const wchar_t* a, LPCWSTR name, int id)
		{
			if (isIntResource(name))
				return id >= 0 && id == intResource(name);
			if (!name)
				return false;
			// "#123" names an id too
			if (name[0] == L'#')
				return id == static_cast<int>(std::wcstol(name + 1, nullptr, 10));
			return a && wcscasecmp(a, name) == 0;
		}

		// a template in memory: DLGTEMPLATE and the dialog it stands for
		struct MemTemplate
		{
			DLGTEMPLATE header;
			unsigned int magic;
			const NppRcDialog* dialog;
		};

		struct ResHandle
		{
			unsigned int magic = 0x4E505053u; // "NPPS"
			const NppRcDialog* dialog = nullptr;
			const NppRcData* data = nullptr;
			MemTemplate tmpl{};
		};
	}

	const NppRcDialog* findDialog(LPCWSTR name)
	{
		for (const NppRcResources* r : registry())
		{
			for (int i = 0; i < r->dialogCount; ++i)
			{
				if (sameName(r->dialogs[i].name, name, r->dialogs[i].id))
					return &r->dialogs[i];
			}
		}
		return nullptr;
	}

	const NppRcDialog* dialogOfTemplate(LPCDLGTEMPLATE t)
	{
		if (!t)
			return nullptr;
		const auto* m = reinterpret_cast<const MemTemplate*>(t);
		if (m->magic == 0x4E505054u) // "NPPT"
			return m->dialog;
		return nullptr;
	}

	const NppRcMenu* findMenu(LPCWSTR name)
	{
		for (const NppRcResources* r : registry())
		{
			for (int i = 0; i < r->menuCount; ++i)
			{
				if (sameName(r->menus[i].name, name, r->menus[i].id))
					return &r->menus[i];
			}
		}
		return nullptr;
	}

	const NppRcAccelTable* findAccel(LPCWSTR name)
	{
		for (const NppRcResources* r : registry())
		{
			for (int i = 0; i < r->accelCount; ++i)
			{
				if (sameName(r->accels[i].name, name, r->accels[i].id))
					return &r->accels[i];
			}
		}
		return nullptr;
	}

	const NppRcData* findData(LPCWSTR name, LPCWSTR type)
	{
		for (const NppRcResources* r : registry())
		{
			for (int i = 0; i < r->dataCount; ++i)
			{
				const NppRcData& d = r->data[i];
				if (!sameName(d.name, name, d.id))
					continue;
				if (type && isIntResource(type))
				{
					const int t = intResource(type);
					const int dt = (d.type == IMAGE_ICON) ? 14 : (d.type == IMAGE_BITMAP ? 2 : (d.type == IMAGE_CURSOR ? 12 : d.type));
					if (t == dt || (t == 3 && d.type == IMAGE_ICON) || (t == 1 && d.type == IMAGE_CURSOR))
						return &d;
					continue;
				}
				if (type && (!d.typeName || wcscasecmp(d.typeName, type) != 0))
					continue;
				return &d;
			}
		}
		return nullptr;
	}

	const NppRcData* findImage(LPCWSTR name, int type)
	{
		const NppRcData* fallback = nullptr;
		for (const NppRcResources* r : registry())
		{
			for (int i = 0; i < r->dataCount; ++i)
			{
				const NppRcData& d = r->data[i];
				if (!sameName(d.name, name, d.id))
					continue;
				if (d.type == type)
					return &d;
				if (!fallback && (d.typeName != nullptr || d.type == IMAGE_ICON || d.type == IMAGE_BITMAP))
					fallback = &d; // a PNG resource loaded as an icon or a bitmap
			}
		}
		return fallback;
	}

	const NppRcString* findString(int id)
	{
		for (const NppRcResources* r : registry())
		{
			for (int i = 0; i < r->stringCount; ++i)
			{
				if (r->strings[i].id == id)
					return &r->strings[i];
			}
		}
		return nullptr;
	}

	const NppRcVersion* versionInfo()
	{
		for (const NppRcResources* r : registry())
		{
			if (r->version)
				return r->version;
		}
		return nullptr;
	}
}

void nppRegisterResources(const NppRcResources* res)
{
	if (res)
		w32::registry().push_back(res);
}

using namespace w32;

HRSRC FindResourceW(HMODULE, LPCWSTR name, LPCWSTR type)
{
	if (isIntResource(type) && intResource(type) == 5) // RT_DIALOG
	{
		const NppRcDialog* d = findDialog(name);
		if (!d)
			return nullptr;
		auto* h = new ResHandle;
		h->dialog = d;
		h->tmpl.header.style = d->style;
		h->tmpl.header.dwExtendedStyle = d->exStyle;
		h->tmpl.header.cdit = static_cast<WORD>(d->count);
		h->tmpl.header.x = d->x;
		h->tmpl.header.y = d->y;
		h->tmpl.header.cx = d->cx;
		h->tmpl.header.cy = d->cy;
		h->tmpl.magic = 0x4E505054u;
		h->tmpl.dialog = d;
		return h;
	}
	const NppRcData* d = findData(name, type);
	if (!d)
		return nullptr;
	auto* h = new ResHandle;
	h->data = d;
	return h;
}

HRSRC FindResourceExW(HMODULE hModule, LPCWSTR type, LPCWSTR name, WORD)
{
	return FindResourceW(hModule, name, type);
}

HGLOBAL LoadResource(HMODULE, HRSRC res)
{
	return res;
}

LPVOID LockResource(HGLOBAL res)
{
	auto* h = static_cast<ResHandle*>(res);
	if (!h || h->magic != 0x4E505053u)
		return nullptr;
	if (h->dialog)
		return &h->tmpl;
	return const_cast<unsigned char*>(h->data->data);
}

DWORD SizeofResource(HMODULE, HRSRC res)
{
	auto* h = static_cast<ResHandle*>(res);
	if (!h || h->magic != 0x4E505053u)
		return 0;
	if (h->dialog)
		return sizeof(MemTemplate);
	return h->data->size;
}

BOOL FreeResource(HGLOBAL)
{
	return TRUE;
}
