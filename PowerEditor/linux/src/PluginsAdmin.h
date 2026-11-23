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

#pragma once

class MainWindow;
class PluginsManager;

namespace PluginsAdmin
{
	// Plugins > Plugins Admin...: the plugins of the plugins folders (bundled with LinuxPad++ or imported), each with
	// a check box. The ticked ones are saved in config.ini ([Plugins] enabled=) and loaded from the next start.
	// True when the choice changed.
	bool exec(MainWindow* mw, PluginsManager* pm);
}
