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

// Notepad++ Linux plugin SDK: a header of the C run time of Microsoft (see npp_win32/w32_api.h).
#pragma once
#include <windows.h>
#include <sys/stat.h>
#include <fcntl.h>
#ifndef _ASSERTE
#define _ASSERTE(x) ((void)0)
#define _ASSERT(x) ((void)0)
#endif
