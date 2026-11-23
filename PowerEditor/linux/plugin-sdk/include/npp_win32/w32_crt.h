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

// Notepad++ Linux plugin SDK: the names of the C run time of Microsoft that Windows code has from
// the standard C headers, without <windows.h> (included in every plugin source by npp_add_plugin).
#pragma once

#ifdef __cplusplus
#include <cstdint>
#endif
#include <string.h>
#include <strings.h>
#include <wchar.h>

#define _wcsicmp wcscasecmp
#define _wcsnicmp wcsncasecmp
#define _stricmp strcasecmp
#define _strnicmp strncasecmp
#define _wcsdup wcsdup
#define _strdup strdup

// the sized integer keywords of MSVC
#ifndef __int64
#define __int64 long long
#define __int32 int
#define __int16 short
#define __int8 char
#endif

// _inline of MSVC (__inline is known to GCC)
#ifndef _inline
#define _inline __inline
#endif
