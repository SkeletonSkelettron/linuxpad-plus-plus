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

// Notepad++ Linux plugin SDK: the integer conversions of intsafe.h that plugins use.
#pragma once

#include <windows.h>
#include <limits>
#include <type_traits>

#define INTSAFE_E_ARITHMETIC_OVERFLOW ((HRESULT)0x80070216L)

namespace npp_intsafe
{
	template <typename To, typename From> inline HRESULT convert(From from, To* to)
	{
		if constexpr (std::is_signed_v<From> == std::is_signed_v<To>)
		{
			if (from < std::numeric_limits<To>::min() || from > std::numeric_limits<To>::max())
				return *to = static_cast<To>(-1), INTSAFE_E_ARITHMETIC_OVERFLOW;
		}
		else if constexpr (std::is_signed_v<From>)
		{
			if (from < 0 || static_cast<std::make_unsigned_t<From>>(from) > std::numeric_limits<To>::max())
				return *to = static_cast<To>(-1), INTSAFE_E_ARITHMETIC_OVERFLOW;
		}
		else if (from > static_cast<std::make_unsigned_t<To>>(std::numeric_limits<To>::max()))
			return *to = static_cast<To>(-1), INTSAFE_E_ARITHMETIC_OVERFLOW;
		*to = static_cast<To>(from);
		return S_OK;
	}
}

inline HRESULT ULongPtrToInt(ULONG_PTR from, INT* to) { return npp_intsafe::convert(from, to); }
inline HRESULT ULongPtrToUInt(ULONG_PTR from, UINT* to) { return npp_intsafe::convert(from, to); }
inline HRESULT ULongPtrToULong(ULONG_PTR from, ULONG* to) { return npp_intsafe::convert(from, to); }
inline HRESULT LongPtrToInt(LONG_PTR from, INT* to) { return npp_intsafe::convert(from, to); }
inline HRESULT SizeTToInt(size_t from, INT* to) { return npp_intsafe::convert(from, to); }
inline HRESULT SizeTToUInt(size_t from, UINT* to) { return npp_intsafe::convert(from, to); }
inline HRESULT SizeTToDWord(size_t from, DWORD* to) { return npp_intsafe::convert(from, to); }
inline HRESULT SizeTToULong(size_t from, ULONG* to) { return npp_intsafe::convert(from, to); }
inline HRESULT IntToUInt(INT from, UINT* to) { return npp_intsafe::convert(from, to); }
inline HRESULT IntToSizeT(INT from, size_t* to) { return npp_intsafe::convert(from, to); }
inline HRESULT UIntToInt(UINT from, INT* to) { return npp_intsafe::convert(from, to); }
inline HRESULT LongLongToInt(LONGLONG from, INT* to) { return npp_intsafe::convert(from, to); }
inline HRESULT LongLongToLong(LONGLONG from, LONG* to) { return npp_intsafe::convert(from, to); }
inline HRESULT ULongLongToULong(ULONGLONG from, ULONG* to) { return npp_intsafe::convert(from, to); }
inline HRESULT ULongLongToUInt(ULONGLONG from, UINT* to) { return npp_intsafe::convert(from, to); }
inline HRESULT ULongLongToSizeT(ULONGLONG from, size_t* to) { return npp_intsafe::convert(from, to); }
