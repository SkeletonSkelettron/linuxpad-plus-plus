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

// Notepad++ Linux plugin SDK: Notepad_plus_msgs.h of Notepad++, with the names of the messages
// that it renamed (*_DEPRECATED) for the plugins written with an older copy.
#pragma once

#include_next <Notepad_plus_msgs.h>

#ifndef NPPM_GETOPENFILENAMES
#define NPPM_GETOPENFILENAMES NPPM_GETOPENFILENAMES_DEPRECATED
#endif
#ifndef NPPM_GETOPENFILENAMESPRIMARY
#define NPPM_GETOPENFILENAMESPRIMARY NPPM_GETOPENFILENAMESPRIMARY_DEPRECATED
#endif
#ifndef NPPM_GETOPENFILENAMESSECOND
#define NPPM_GETOPENFILENAMESSECOND NPPM_GETOPENFILENAMESSECOND_DEPRECATED
#endif
#ifndef NPPM_DESTROYSCINTILLAHANDLE
#define NPPM_DESTROYSCINTILLAHANDLE NPPM_DESTROYSCINTILLAHANDLE_DEPRECATED
#endif
#ifndef NPPM_ADDTOOLBARICON
#define NPPM_ADDTOOLBARICON NPPM_ADDTOOLBARICON_DEPRECATED
#endif
#ifndef NPPM_GETENABLETHEMETEXTUREFUNC
#define NPPM_GETENABLETHEMETEXTUREFUNC NPPM_GETENABLETHEMETEXTUREFUNC_DEPRECATED
#endif
#ifndef NPPM_ALLOCATESUPPORTED
#define NPPM_ALLOCATESUPPORTED NPPM_ALLOCATESUPPORTED_DEPRECATED
#endif
