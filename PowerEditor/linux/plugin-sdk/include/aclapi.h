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

// Notepad++ Linux plugin SDK: aclapi.h. Linux files have POSIX permissions, not ACLs: the ACL functions succeed
// and change nothing (a moved file keeps its permissions).

#pragma once
#include <windows.h>

#ifndef RC_INVOKED
struct ACL
{
	BYTE AclRevision;
	BYTE Sbz1;
	WORD AclSize;
	WORD AceCount;
	WORD Sbz2;
};
typedef void* PSID;
typedef DWORD SECURITY_INFORMATION;
enum SE_OBJECT_TYPE { SE_UNKNOWN_OBJECT_TYPE = 0, SE_FILE_OBJECT, SE_SERVICE, SE_PRINTER, SE_REGISTRY_KEY };
#define ACL_REVISION 2
#define OWNER_SECURITY_INFORMATION 0x00000001L
#define GROUP_SECURITY_INFORMATION 0x00000002L
#define DACL_SECURITY_INFORMATION 0x00000004L
#define SACL_SECURITY_INFORMATION 0x00000008L
#define PROTECTED_DACL_SECURITY_INFORMATION 0x80000000L
#define UNPROTECTED_DACL_SECURITY_INFORMATION 0x20000000L
inline BOOL InitializeAcl(PACL acl, DWORD size, DWORD revision) // PACL: void* (windows.h of the SDK)
{
	auto* a = static_cast<ACL*>(acl);
	if (!a)
		return FALSE;
	a->AclRevision = static_cast<BYTE>(revision);
	a->AclSize = static_cast<WORD>(size);
	a->AceCount = 0;
	return TRUE;
}
inline DWORD SetNamedSecurityInfoW(LPWSTR, SE_OBJECT_TYPE, SECURITY_INFORMATION, PSID, PSID, PACL, PACL) { return ERROR_SUCCESS; }
#define SetNamedSecurityInfo SetNamedSecurityInfoW
#endif
