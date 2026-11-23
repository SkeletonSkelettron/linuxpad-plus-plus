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

// Notepad++ Linux plugin SDK: IUnknown and the COM helpers that plugins use.
#pragma once

#include <windows.h>
#include <cstring>

#ifndef STDMETHOD
#define STDMETHOD(method) virtual HRESULT STDMETHODCALLTYPE method
#define STDMETHOD_(type, method) virtual type STDMETHODCALLTYPE method
#define STDMETHODIMP HRESULT STDMETHODCALLTYPE
#define STDMETHODIMP_(type) type STDMETHODCALLTYPE
#define IFACEMETHODIMP STDMETHODIMP
#define IFACEMETHODIMP_(type) STDMETHODIMP_(type)
#define PURE = 0
#endif

#ifndef HRESULT_FROM_WIN32
#define HRESULT_FROM_WIN32(x) ((HRESULT)(x) <= 0 ? ((HRESULT)(x)) : ((HRESULT)(((x) & 0x0000FFFF) | (7 << 16) | 0x80000000)))
#endif

inline bool IsEqualGUID(REFGUID a, REFGUID b) { return std::memcmp(&a, &b, sizeof(GUID)) == 0; }
#define IsEqualIID IsEqualGUID
#define IsEqualCLSID IsEqualGUID
inline bool operator==(REFGUID a, REFGUID b) { return IsEqualGUID(a, b); }
inline bool operator!=(REFGUID a, REFGUID b) { return !IsEqualGUID(a, b); }

inline const IID IID_IUnknown = {0x00000000, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};

struct IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) = 0;
	virtual ULONG STDMETHODCALLTYPE AddRef() = 0;
	virtual ULONG STDMETHODCALLTYPE Release() = 0;
	static REFIID iid() { return IID_IUnknown; }
protected:
	virtual ~IUnknown() = default;
};
typedef IUnknown* LPUNKNOWN;

// IID_PPV_ARGS(&p): the IID of the interface of p, and p
template <typename T> inline REFIID npp_iid_of(T**) { return T::iid(); }
#define IID_PPV_ARGS(pp) npp_iid_of(pp), reinterpret_cast<void**>(pp)
