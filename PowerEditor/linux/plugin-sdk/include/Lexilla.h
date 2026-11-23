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

// Notepad++ Linux plugin SDK: Lexilla.h of Lexilla. The Lexilla functions that a lexer plugin
// defines (CreateLexer, GetLexerCount, GetLexerName...) are its exports, which exports.def says
// on Windows: declared here with the default visibility, their definitions are exported too.
#pragma once

#include <ILexer.h>

#pragma GCC visibility push(default)
#include_next <Lexilla.h>
#pragma GCC visibility pop
