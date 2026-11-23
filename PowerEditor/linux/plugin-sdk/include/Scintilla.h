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

// Notepad++ Linux plugin SDK: Scintilla.h of Notepad++, with the API that Notepad++
// took out of it in 8.4.3 and that older plugins still use. Scintilla still answers
// these messages; Sci_PositionCR is long, 64 bit on Linux like Sci_Position.

#pragma once

#include_next <Scintilla.h>

#ifndef NPP_SCINTILLA_OLD_API
#define NPP_SCINTILLA_OLD_API

#ifndef SCI_GETSTYLEDTEXT
#define SCI_GETSTYLEDTEXT 2015
#endif
#ifndef SCI_FINDTEXT
#define SCI_FINDTEXT 2150
#endif
#ifndef SCI_FORMATRANGE
#define SCI_FORMATRANGE 2151
#endif
#ifndef SCI_GETTEXTRANGE
#define SCI_GETTEXTRANGE 2162
#endif
// lexers are set by Notepad++ (Lexilla): these do nothing in Scintilla 5
#ifndef SCI_SETLEXER
#define SCI_SETLEXER 4001
#endif
#ifndef SCI_SETLEXERLANGUAGE
#define SCI_SETLEXERLANGUAGE 4006
#endif
#ifndef SCI_LOADLEXERLIBRARY
#define SCI_LOADLEXERLIBRARY 4007
#endif

typedef long Sci_PositionCR;

struct Sci_CharacterRange
{
	Sci_PositionCR cpMin;
	Sci_PositionCR cpMax;
};

struct Sci_TextRange
{
	struct Sci_CharacterRange chrg;
	char* lpstrText;
};

struct Sci_TextToFind
{
	struct Sci_CharacterRange chrg;
	const char* lpstrText;
	struct Sci_CharacterRange chrgText;
};

struct Sci_RangeToFormat
{
	Sci_SurfaceID hdc;
	Sci_SurfaceID hdcTarget;
	struct Sci_Rectangle rc;
	struct Sci_Rectangle rcPage;
	struct Sci_CharacterRange chrg;
};

// the names of Scintilla 3, still used by older plugins
#ifndef NPP_NO_SCINTILLA_OLD_NAMES
#define CharacterRange Sci_CharacterRange
#define TextRange Sci_TextRange
#define TextToFind Sci_TextToFind
#define RangeToFormat Sci_RangeToFormat
#endif

#endif
