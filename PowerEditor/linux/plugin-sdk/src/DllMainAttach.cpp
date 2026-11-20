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

// Called by the host after the module and its C++ globals have been initialized.
// DLL_PROCESS_DETACH is not sent: plugins are not unloaded, and at the exit of the
// process the widgets they would free are gone. Use NPPN_SHUTDOWN to clean up.

#include "../win32/W32Internal.h"

BOOL APIENTRY DllMain(HANDLE hModule, DWORD reasonForCall, LPVOID reserved);

extern "C" __declspec(dllexport) BOOL nppLinuxInitialize()
{
	static const BOOL initialized = DllMain(w32::moduleHandle(), DLL_PROCESS_ATTACH, nullptr);
	return initialized;
}
