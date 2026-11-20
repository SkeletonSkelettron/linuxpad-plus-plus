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

// Minimal stand-in for <windows.h>, used only when compiling
// lexilla/lexers/LexUser.cxx (Notepad++ UDL lexer) on Linux.
// The lexer only needs _itoa() from the Windows CRT.

#pragma once

#include <cstdio>
#include <cstdlib>
#include <cstring>

inline char* _itoa(int value, char* buffer, int radix)
{
	if (radix == 10)
	{
		std::sprintf(buffer, "%d", value);
		return buffer;
	}

	char tmp[sizeof(int) * 8 + 1];
	unsigned int v = (value < 0 && radix == 10) ? static_cast<unsigned int>(-value) : static_cast<unsigned int>(value);
	int i = 0;
	do
	{
		const unsigned int digit = v % static_cast<unsigned int>(radix);
		tmp[i++] = static_cast<char>(digit < 10 ? '0' + digit : 'a' + digit - 10);
		v /= static_cast<unsigned int>(radix);
	} while (v);

	int j = 0;
	while (i > 0)
		buffer[j++] = tmp[--i];
	buffer[j] = '\0';
	return buffer;
}
