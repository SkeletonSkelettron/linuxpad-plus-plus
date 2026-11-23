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

// Notepad++ Linux plugin SDK: WM_DEVICECHANGE events (never sent here).

#pragma once
#include <windows.h>

#ifndef RC_INVOKED
#define DBT_DEVNODES_CHANGED 0x0007
#define DBT_QUERYCHANGECONFIG 0x0017
#define DBT_CONFIGCHANGED 0x0018
#define DBT_DEVICEARRIVAL 0x8000
#define DBT_DEVICEQUERYREMOVE 0x8001
#define DBT_DEVICEQUERYREMOVEFAILED 0x8002
#define DBT_DEVICEREMOVEPENDING 0x8003
#define DBT_DEVICEREMOVECOMPLETE 0x8004
#define DBT_DEVICETYPESPECIFIC 0x8005
#define DBT_DEVTYP_OEM 0x00000000
#define DBT_DEVTYP_VOLUME 0x00000002
#define DBT_DEVTYP_PORT 0x00000003
#define DBT_DEVTYP_DEVICEINTERFACE 0x00000005
#define DBT_DEVTYP_HANDLE 0x00000006
#define DBTF_MEDIA 0x0001
#define DBTF_NET 0x0002
struct DEV_BROADCAST_HDR
{
	DWORD dbch_size;
	DWORD dbch_devicetype;
	DWORD dbch_reserved;
};
typedef DEV_BROADCAST_HDR* PDEV_BROADCAST_HDR;
struct DEV_BROADCAST_VOLUME
{
	DWORD dbcv_size;
	DWORD dbcv_devicetype;
	DWORD dbcv_reserved;
	DWORD dbcv_unitmask;
	WORD dbcv_flags;
};
typedef DEV_BROADCAST_VOLUME* PDEV_BROADCAST_VOLUME;
#endif
