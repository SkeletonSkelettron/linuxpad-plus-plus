/*
 * This file is part of ComparePlus plugin for Notepad++
 * Copyright (C)2013 Jean-Sebastien Leroy (jean.sebastien.leroy@gmail.com)
 * Copyright (C)2017-2025 Pavel Nedev (pg.nedev@gmail.com)
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdlib.h>
#include <windows.h>
#include <wchar.h>
#include <shlwapi.h>
#include <string>

#include "SqliteHelper.h"
#include "NppHelpers.h"
#include "Tools.h"


PSQLVERSION			sqlite3_libversion;
PSQLOPEN16			sqlite3_open16;
PSQLPREPARE16V2		sqlite3_prepare16_v2;
PSQLSTEP			sqlite3_step;
PSQLCOLUMNTEXT16	sqlite3_column_text16;
PSQLFINALZE			sqlite3_finalize;
PSQLCLOSE			sqlite3_close;


#ifdef NPP_PLUGIN_SDK_LINUX

#include <dlfcn.h>
#include <cstring>

// The SQLite of the distribution in the place of the sqlite3.dll of the plugin. Its UTF-16 functions take the
// wchar_t strings of Windows: here wchar_t is UTF-32, so its UTF-8 functions are called through these.
namespace
{

typedef int (*PSQLOPEN8) (const char *filename, sqlite3 **ppDb);
typedef int (*PSQLPREPARE8V2) (sqlite3 *db, const char *zSql, int nByte, sqlite3_stmt **ppStmt, const char **pzTail);
typedef const unsigned char * (*PSQLCOLUMNTEXT8) (sqlite3_stmt *pStmt, int iCol);

PSQLOPEN8		sqlite3_open8;
PSQLPREPARE8V2	sqlite3_prepare8_v2;
PSQLCOLUMNTEXT8	sqlite3_column_text8;

HMODULE openSQLite()
{
	static void* lib = []
	{
		void* h = dlopen("libsqlite3.so.0", RTLD_NOW | RTLD_LOCAL);

		if (!h)
			h = dlopen("libsqlite3.so", RTLD_NOW | RTLD_LOCAL);

		return h;
	}();

	return static_cast<HMODULE>(lib);
}

std::string toUtf8(const void* text)
{
	const wchar_t* w = static_cast<const wchar_t*>(text);
	if (!w || !*w)
		return std::string();

	const int len = ::WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);
	if (len <= 0)
		return std::string();

	std::string s(len, '\0');
	::WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), len, NULL, NULL);
	s.resize(std::strlen(s.c_str()));

	return s;
}

int open16(const void *filename, sqlite3 **ppDb)
{
	return sqlite3_open8(toUtf8(filename).c_str(), ppDb);
}

int prepare16_v2(sqlite3 *db, const void *zSql, int, sqlite3_stmt **ppStmt, const char **pzTail)
{
	if (pzTail)
		*pzTail = nullptr;

	return sqlite3_prepare8_v2(db, toUtf8(zSql).c_str(), -1, ppStmt, nullptr);
}

const void * column_text16(sqlite3_stmt *pStmt, int iCol)
{
	static thread_local std::wstring value;

	const char* text = reinterpret_cast<const char*>(sqlite3_column_text8(pStmt, iCol));
	value.clear();

	if (text && *text)
	{
		const int len = ::MultiByteToWideChar(CP_UTF8, 0, text, -1, NULL, 0);
		if (len > 0)
		{
			value.resize(len);
			::MultiByteToWideChar(CP_UTF8, 0, text, -1, value.data(), len);
			value.resize(wcslen(value.c_str()));
		}
	}

	return value.c_str();
}

}


bool isSQLlibFound()
{
	return openSQLite() != nullptr;
}


bool InitSQLite()
{
	static bool isInit = false;

	if (!isInit)
	{
		HMODULE ligSQLite = openSQLite();
		if (!ligSQLite)
			return false;

		sqlite3_open8 = (PSQLOPEN8)::GetProcAddress(ligSQLite, "sqlite3_open");
		sqlite3_prepare8_v2 = (PSQLPREPARE8V2)::GetProcAddress(ligSQLite, "sqlite3_prepare_v2");
		sqlite3_column_text8 = (PSQLCOLUMNTEXT8)::GetProcAddress(ligSQLite, "sqlite3_column_text");
		if (!sqlite3_open8 || !sqlite3_prepare8_v2 || !sqlite3_column_text8)
			return false;

		sqlite3_libversion = (PSQLVERSION)::GetProcAddress(ligSQLite, "sqlite3_libversion");
		if (!sqlite3_libversion)
			return false;
		sqlite3_step = (PSQLSTEP)::GetProcAddress(ligSQLite, "sqlite3_step");
		if (!sqlite3_step)
			return false;
		sqlite3_finalize = (PSQLFINALZE)::GetProcAddress(ligSQLite, "sqlite3_finalize");
		if (!sqlite3_finalize)
			return false;
		sqlite3_close = (PSQLCLOSE)::GetProcAddress(ligSQLite, "sqlite3_close");
		if (!sqlite3_close)
			return false;

		sqlite3_open16 = open16;
		sqlite3_prepare16_v2 = prepare16_v2;
		sqlite3_column_text16 = column_text16;

		isInit = true;
	}

	return true;
}

#else

namespace
{

inline std::wstring getDllPath()
{
	std::wstring dll = getPluginsHomePath();
	if (dll.empty())
		return dll;

	dll += L"\\ComparePlus\\libs\\sqlite3.dll";

	return dll;
}

}


bool isSQLlibFound()
{
	const std::wstring dllPath = getDllPath();

	if (dllPath.size())
		return fileExists(dllPath.c_str());

	return false;
}


bool InitSQLite()
{
	static bool isInit = false;

	if (!isInit)
	{
		const std::wstring dllPath = getDllPath();

		if (dllPath.empty())
			return false;

		HMODULE ligSQLite = ::LoadLibraryW(dllPath.c_str());
		if (!ligSQLite)
			return false;

		sqlite3_libversion = (PSQLVERSION)::GetProcAddress(ligSQLite, "sqlite3_libversion");
		if (!sqlite3_libversion)
			return false;
		sqlite3_open16 = (PSQLOPEN16)::GetProcAddress(ligSQLite, "sqlite3_open16");
		if (!sqlite3_open16)
			return false;
		sqlite3_prepare16_v2 = (PSQLPREPARE16V2)::GetProcAddress(ligSQLite, "sqlite3_prepare16_v2");
		if (!sqlite3_prepare16_v2)
			return false;
		sqlite3_step = (PSQLSTEP)::GetProcAddress(ligSQLite, "sqlite3_step");
		if (!sqlite3_step)
			return false;
		sqlite3_column_text16 = (PSQLCOLUMNTEXT16)::GetProcAddress(ligSQLite, "sqlite3_column_text16");
		if (!sqlite3_column_text16)
			return false;
		sqlite3_finalize = (PSQLFINALZE)::GetProcAddress(ligSQLite, "sqlite3_finalize");
		if (!sqlite3_finalize)
			return false;
		sqlite3_close = (PSQLCLOSE)::GetProcAddress(ligSQLite, "sqlite3_close");
		if (!sqlite3_close)
			return false;

		isInit = true;
	}

	return true;
}

#endif
