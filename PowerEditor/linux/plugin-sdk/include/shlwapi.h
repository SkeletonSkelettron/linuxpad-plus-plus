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

// Notepad++ Linux plugin SDK: the path and string functions of shlwapi. Paths may use
// '/' or '\\'; the functions add '/'.

#pragma once

#include <windows.h>
#include <cstring>

#ifndef RC_INVOKED
BOOL PathFileExistsW(LPCWSTR path);
BOOL PathFileExistsA(LPCSTR path);
BOOL PathIsDirectoryW(LPCWSTR path);
BOOL PathIsDirectoryEmptyW(LPCWSTR path);
BOOL PathIsRelativeW(LPCWSTR path);
BOOL PathIsRootW(LPCWSTR path);
BOOL PathIsUNCW(LPCWSTR path);
BOOL PathIsURLW(LPCWSTR path);
BOOL PathIsFileSpecW(LPCWSTR path);
BOOL PathIsPrefixW(LPCWSTR prefix, LPCWSTR path);
BOOL PathIsSameRootW(LPCWSTR a, LPCWSTR b);
LPWSTR PathFindFileNameW(LPCWSTR path);
LPSTR PathFindFileNameA(LPCSTR path);
LPWSTR PathFindExtensionW(LPCWSTR path);
LPSTR PathFindExtensionA(LPCSTR path);
LPWSTR PathFindNextComponentW(LPCWSTR path);
LPWSTR PathGetArgsW(LPCWSTR path);
BOOL PathRemoveFileSpecW(LPWSTR path);
BOOL PathRemoveFileSpecA(LPSTR path);
void PathRemoveExtensionW(LPWSTR path);
BOOL PathRenameExtensionW(LPWSTR path, LPCWSTR ext);
BOOL PathAddExtensionW(LPWSTR path, LPCWSTR ext);
void PathStripPathW(LPWSTR path);
BOOL PathStripToRootW(LPWSTR path);
LPWSTR PathAddBackslashW(LPWSTR path);
LPWSTR PathRemoveBackslashW(LPWSTR path);
void PathRemoveBlanksW(LPWSTR path);
void PathRemoveArgsW(LPWSTR path);
BOOL PathAppendW(LPWSTR path, LPCWSTR more);
BOOL PathAppendA(LPSTR path, LPCSTR more);
LPWSTR PathCombineW(LPWSTR dst, LPCWSTR dir, LPCWSTR file);
BOOL PathCanonicalizeW(LPWSTR dst, LPCWSTR src);
void PathQuoteSpacesW(LPWSTR path);
void PathUnquoteSpacesW(LPWSTR path);
BOOL PathMatchSpecW(LPCWSTR file, LPCWSTR spec);
BOOL PathMatchSpecA(LPCSTR file, LPCSTR spec);
BOOL PathCompactPathExW(LPWSTR out, LPCWSTR src, UINT max, DWORD flags);
BOOL PathCompactPathW(HDC hdc, LPWSTR path, UINT width);
int PathCommonPrefixW(LPCWSTR a, LPCWSTR b, LPWSTR out);
BOOL PathRelativePathToW(LPWSTR out, LPCWSTR from, DWORD attrFrom, LPCWSTR to, DWORD attrTo);
int PathGetDriveNumberW(LPCWSTR path);
BOOL PathSearchAndQualifyW(LPCWSTR path, LPWSTR out, UINT max);
BOOL PathYetAnotherMakeUniqueName(LPWSTR out, LPCWSTR path, LPCWSTR shortName, LPCWSTR longName);
HRESULT PathCchCombine(LPWSTR out, size_t max, LPCWSTR dir, LPCWSTR file);
HRESULT PathCchAppend(LPWSTR path, size_t max, LPCWSTR more);
HRESULT PathCchRemoveFileSpec(LPWSTR path, size_t max);
HRESULT PathCchAddBackslash(LPWSTR path, size_t max);
HRESULT PathCchRemoveBackslash(LPWSTR path, size_t max);
HRESULT PathCchFindExtension(LPCWSTR path, size_t max, LPCWSTR* ext);
HRESULT PathCchRenameExtension(LPWSTR path, size_t max, LPCWSTR ext);
HRESULT PathCchCanonicalize(LPWSTR out, size_t max, LPCWSTR in);
#define PathFileExists PathFileExistsW
#define PathIsDirectory PathIsDirectoryW
#define PathIsDirectoryEmpty PathIsDirectoryEmptyW
#define PathIsRelative PathIsRelativeW
#define PathIsRoot PathIsRootW
#define PathIsUNC PathIsUNCW
#define PathIsURL PathIsURLW
#define PathIsFileSpec PathIsFileSpecW
#define PathIsPrefix PathIsPrefixW
#define PathIsSameRoot PathIsSameRootW
#define PathFindFileName PathFindFileNameW
#define PathFindExtension PathFindExtensionW
#define PathFindNextComponent PathFindNextComponentW
#define PathGetArgs PathGetArgsW
#define PathRemoveFileSpec PathRemoveFileSpecW
#define PathRemoveExtension PathRemoveExtensionW
#define PathRenameExtension PathRenameExtensionW
#define PathAddExtension PathAddExtensionW
#define PathStripPath PathStripPathW
#define PathStripToRoot PathStripToRootW
#define PathAddBackslash PathAddBackslashW
#define PathRemoveBackslash PathRemoveBackslashW
#define PathRemoveBlanks PathRemoveBlanksW
#define PathRemoveArgs PathRemoveArgsW
#define PathAppend PathAppendW
#define PathCombine PathCombineW
#define PathCanonicalize PathCanonicalizeW
#define PathQuoteSpaces PathQuoteSpacesW
#define PathUnquoteSpaces PathUnquoteSpacesW
#define PathMatchSpec PathMatchSpecW
#define PathCompactPathEx PathCompactPathExW
#define PathCompactPath PathCompactPathW
#define PathCommonPrefix PathCommonPrefixW
#define PathRelativePathTo PathRelativePathToW
#define PathGetDriveNumber PathGetDriveNumberW
#define PathSearchAndQualify PathSearchAndQualifyW

LPWSTR StrStrIW(LPCWSTR s, LPCWSTR find);
LPSTR StrStrIA(LPCSTR s, LPCSTR find);
LPWSTR StrStrW(LPCWSTR s, LPCWSTR find);
LPWSTR StrRStrIW(LPCWSTR s, LPCWSTR end, LPCWSTR find);
int StrCmpIW(LPCWSTR a, LPCWSTR b);
int StrCmpW(LPCWSTR a, LPCWSTR b);
int StrCmpNIW(LPCWSTR a, LPCWSTR b, int n);
int StrCmpNW(LPCWSTR a, LPCWSTR b, int n);
int StrCmpLogicalW(LPCWSTR a, LPCWSTR b);
LPWSTR StrChrW(LPCWSTR s, WCHAR c);
LPWSTR StrChrIW(LPCWSTR s, WCHAR c);
LPWSTR StrRChrW(LPCWSTR s, LPCWSTR end, WCHAR c);
LPWSTR StrCpyW(LPWSTR dst, LPCWSTR src);
LPWSTR StrCpyNW(LPWSTR dst, LPCWSTR src, int max);
LPWSTR StrCatW(LPWSTR dst, LPCWSTR src);
LPWSTR StrCatBuffW(LPWSTR dst, LPCWSTR src, int max);
LPWSTR StrDupW(LPCWSTR s);
int StrToIntW(LPCWSTR s);
BOOL StrToIntExW(LPCWSTR s, DWORD flags, int* out);
BOOL StrTrimW(LPWSTR s, LPCWSTR chars);
LPWSTR StrFormatByteSizeW(LONGLONG size, LPWSTR buffer, UINT max);
LPWSTR StrPBrkW(LPCWSTR s, LPCWSTR set);
int StrSpnW(LPCWSTR s, LPCWSTR set);
int StrCSpnW(LPCWSTR s, LPCWSTR set);
// the char versions (UTF-8 strings: bytes are compared, ASCII case folding)
inline LPSTR StrChrA(LPCSTR s, WORD c) { return s ? const_cast<LPSTR>(strchr(s, static_cast<char>(c))) : nullptr; }
inline LPSTR StrChrIA(LPCSTR s, WORD c)
{
	for (; s && *s; ++s)
		if (tolower(static_cast<unsigned char>(*s)) == tolower(static_cast<unsigned char>(c)))
			return const_cast<LPSTR>(s);
	return nullptr;
}
inline LPSTR StrRChrA(LPCSTR s, LPCSTR end, WORD c)
{
	LPCSTR found = nullptr;
	for (; s && *s && (!end || s < end); ++s)
		if (*s == static_cast<char>(c))
			found = s;
	return const_cast<LPSTR>(found);
}
inline LPSTR StrStrA(LPCSTR s, LPCSTR find) { return (s && find) ? const_cast<LPSTR>(strstr(s, find)) : nullptr; }
inline int StrCmpIA(LPCSTR a, LPCSTR b) { return strcasecmp(a, b); }
inline int StrCmpNIA(LPCSTR a, LPCSTR b, int n) { return strncasecmp(a, b, static_cast<size_t>(n)); }
inline int StrCmpNA(LPCSTR a, LPCSTR b, int n) { return strncmp(a, b, static_cast<size_t>(n)); }
inline int StrToIntA(LPCSTR s) { return s ? atoi(s) : 0; }
#define STIF_DEFAULT 0x00000000
#define STIF_SUPPORT_HEX 0x00000001
#define StrStrI StrStrIW
#define StrStr StrStrW
#define StrRStrI StrRStrIW
#define StrCmpI StrCmpIW
#define StrCmp StrCmpW
#define StrCmpNI StrCmpNIW
#define StrCmpN StrCmpNW
#define StrCmpLogical StrCmpLogicalW
#define StrChr StrChrW
#define StrChrI StrChrIW
#define StrRChr StrRChrW
#define StrCpy StrCpyW
#define StrCpyN StrCpyNW
#define StrCat StrCatW
#define StrCatBuff StrCatBuffW
#define StrDup StrDupW
#define StrToInt StrToIntW
#define StrToIntEx StrToIntExW
#define StrTrim StrTrimW
#define StrFormatByteSize StrFormatByteSizeW
#define StrFormatByteSize64 StrFormatByteSizeW
#define StrPBrk StrPBrkW
#define StrSpn StrSpnW
#define StrCSpn StrCSpnW
#define wnsprintf npp_snwprintf
#define wnsprintfW npp_snwprintf
HRESULT UrlEscapeW(LPCWSTR url, LPWSTR out, LPDWORD outLen, DWORD flags);
HRESULT UrlUnescapeW(LPWSTR url, LPWSTR out, LPDWORD outLen, DWORD flags);
HRESULT UrlCreateFromPathW(LPCWSTR path, LPWSTR url, LPDWORD urlLen, DWORD reserved);
HRESULT PathCreateFromUrlW(LPCWSTR url, LPWSTR path, LPDWORD pathLen, DWORD reserved);
#define UrlEscape UrlEscapeW
#define UrlUnescape UrlUnescapeW
#define UrlCreateFromPath UrlCreateFromPathW
#define PathCreateFromUrl PathCreateFromUrlW
#define URL_ESCAPE_SEGMENT_ONLY 0x00002000
#define URL_ESCAPE_PERCENT 0x00001000
#define URL_UNESCAPE_INPLACE 0x00100000
HRESULT SHLoadIndirectString(LPCWSTR source, LPWSTR out, UINT max, void** reserved);
#endif
