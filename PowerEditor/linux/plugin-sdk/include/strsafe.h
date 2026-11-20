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

// Notepad++ Linux plugin SDK: the StringCch functions of strsafe.h.
#pragma once
#include <windows.h>
#ifndef RC_INVOKED
inline HRESULT StringCchCopyW(LPWSTR dst, size_t max, LPCWSTR src) { if (!dst || !max) return E_INVALIDARG; wcscpy_s(dst, max, src ? src : L""); return (src && std::wcslen(src) >= max) ? (HRESULT)0x8007007AL : S_OK; }
inline HRESULT StringCchCopyNW(LPWSTR dst, size_t max, LPCWSTR src, size_t n) { if (!dst || !max) return E_INVALIDARG; wcsncpy_s(dst, max, src ? src : L"", n); return S_OK; }
inline HRESULT StringCchCatW(LPWSTR dst, size_t max, LPCWSTR src) { if (!dst || !max) return E_INVALIDARG; wcscat_s(dst, max, src ? src : L""); return S_OK; }
inline HRESULT StringCchLengthW(LPCWSTR s, size_t max, size_t* len) { if (!s) return E_INVALIDARG; const size_t n = wcsnlen(s, max); if (len) *len = n; return n < max ? S_OK : E_INVALIDARG; }
inline HRESULT StringCchPrintfW(LPWSTR dst, size_t max, LPCWSTR format, ...) { va_list ap; va_start(ap, format); const int n = npp_vsnwprintf(dst, max, format, ap); va_end(ap); return n < 0 ? (HRESULT)0x8007007AL : S_OK; }
inline HRESULT StringCchVPrintfW(LPWSTR dst, size_t max, LPCWSTR format, va_list ap) { return npp_vsnwprintf(dst, max, format, ap) < 0 ? (HRESULT)0x8007007AL : S_OK; }
inline HRESULT StringCbCopyW(LPWSTR dst, size_t bytes, LPCWSTR src) { return StringCchCopyW(dst, bytes / sizeof(wchar_t), src); }
inline HRESULT StringCbCatW(LPWSTR dst, size_t bytes, LPCWSTR src) { return StringCchCatW(dst, bytes / sizeof(wchar_t), src); }
inline HRESULT StringCchCopyA(LPSTR dst, size_t max, LPCSTR src) { if (!dst || !max) return E_INVALIDARG; strcpy_s(dst, max, src ? src : ""); return S_OK; }
inline HRESULT StringCchCatA(LPSTR dst, size_t max, LPCSTR src) { if (!dst || !max) return E_INVALIDARG; strcat_s(dst, max, src ? src : ""); return S_OK; }
inline HRESULT StringCchPrintfA(LPSTR dst, size_t max, LPCSTR format, ...) { va_list ap; va_start(ap, format); const int n = vsnprintf(dst, max, format, ap); va_end(ap); return n < 0 ? (HRESULT)0x8007007AL : S_OK; }
#define StringCchCopy StringCchCopyW
#define StringCchCopyN StringCchCopyNW
#define StringCchCat StringCchCatW
#define StringCchLength StringCchLengthW
#define StringCchPrintf StringCchPrintfW
#define StringCchVPrintf StringCchVPrintfW
#define StringCbCopy StringCbCopyW
#define StringCbCat StringCbCatW
#endif
