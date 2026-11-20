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

// What the plugin of the self test reports (NPPM_MSGTOPLUGIN, internalMsg kGetReport).

#pragma once

#include <cstdint>

struct TestPluginReport
{
	static constexpr long kGetReport = 1;
	int nppn[64] = {};             // count of each NPPN_ notification (code - NPPN_FIRST)
	uintptr_t lastIdFrom[64] = {}; // idFrom of the last one
	int scnModified = 0;
	int scnUpdateUi = 0;
	int badHwndFrom = 0;
	int commandA = 0;
	int about = 0;
	int dynamicCommands = 0;
};
