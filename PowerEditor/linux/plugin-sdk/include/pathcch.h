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

// Notepad++ Linux plugin SDK: pathcch.h, on top of the shlwapi.h path functions.
#pragma once

#include <windows.h>
#include <shlwapi.h>

#define PATHCCH_NONE 0x00
#define PATHCCH_ALLOW_LONG_PATHS 0x01
#define PATHCCH_MAX_CCH 0x8000

inline HRESULT PathCchCombine(PWSTR out, size_t cch, PCWSTR dir, PCWSTR file)
{
	wchar_t tmp[MAX_PATH * 4] = {};
	if (!out || !PathCombineW(tmp, dir, file) || wcslen(tmp) + 1 > cch)
		return E_INVALIDARG;
	wcscpy(out, tmp);
	return S_OK;
}
inline HRESULT PathCchCombineEx(PWSTR out, size_t cch, PCWSTR dir, PCWSTR file, ULONG) { return PathCchCombine(out, cch, dir, file); }
inline HRESULT PathCchAppend(PWSTR path, size_t cch, PCWSTR more)
{
	if (!path || !more || wcslen(path) + wcslen(more) + 2 > cch)
		return E_INVALIDARG;
	return PathAppendW(path, more) ? S_OK : E_INVALIDARG;
}
inline HRESULT PathCchAppendEx(PWSTR path, size_t cch, PCWSTR more, ULONG) { return PathCchAppend(path, cch, more); }
inline HRESULT PathCchRemoveFileSpec(PWSTR path, size_t) { return (path && PathRemoveFileSpecW(path)) ? S_OK : S_FALSE; }
inline HRESULT PathCchAddBackslash(PWSTR path, size_t cch)
{
	if (!path || wcslen(path) + 2 > cch)
		return E_INVALIDARG;
	return PathAddBackslashW(path) ? S_OK : S_FALSE;
}
inline HRESULT PathCchRemoveBackslash(PWSTR path, size_t) { return (path && PathRemoveBackslashW(path)) ? S_OK : S_FALSE; }
inline HRESULT PathCchRemoveExtension(PWSTR path, size_t) { if (path) PathRemoveExtensionW(path); return S_OK; }
inline HRESULT PathCchRenameExtension(PWSTR path, size_t, PCWSTR ext) { return (path && PathRenameExtensionW(path, ext)) ? S_OK : E_INVALIDARG; }
