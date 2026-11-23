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

// Notepad++ Linux plugin SDK: OLE data objects (IDataObject, FORMATETC, STGMEDIUM). Win32/ole32.cpp has
// DoDragDrop, RegisterDragDrop and OleSetClipboard on the drag and drop and the clipboard of Qt (files: CF_HDROP).

#pragma once
#include <windows.h>
#include <unknwn.h>

#ifndef RC_INVOKED
typedef WORD CLIPFORMAT;
typedef CLIPFORMAT* LPCLIPFORMAT;
typedef wchar_t* LPOLESTR;
typedef const wchar_t* LPCOLESTR;
typedef void* HMETAFILEPICT;
typedef void* HENHMETAFILE;
struct STATSTG
{
	LPOLESTR pwcsName;
	DWORD type;
	ULARGE_INTEGER cbSize;
	FILETIME mtime;
	FILETIME ctime;
	FILETIME atime;
	DWORD grfMode;
	DWORD grfLocksSupported;
	CLSID clsid;
	DWORD grfStateBits;
	DWORD reserved;
};
inline const IID IID_ISequentialStream = {0x0c733a30, 0x2a1c, 0x11ce, {0xad, 0xe5, 0x00, 0xaa, 0x00, 0x44, 0x77, 0x3d}};
inline const IID IID_IStream = {0x0000000c, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
inline const IID IID_IStorage = {0x0000000b, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
struct ISequentialStream : public IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE Read(void* pv, ULONG cb, ULONG* pcbRead) = 0;
	virtual HRESULT STDMETHODCALLTYPE Write(const void* pv, ULONG cb, ULONG* pcbWritten) = 0;
	static REFIID iid() { return IID_ISequentialStream; }
};
struct IStream : public ISequentialStream
{
	virtual HRESULT STDMETHODCALLTYPE Seek(LARGE_INTEGER dlibMove, DWORD dwOrigin, ULARGE_INTEGER* plibNewPosition) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetSize(ULARGE_INTEGER libNewSize) = 0;
	virtual HRESULT STDMETHODCALLTYPE CopyTo(IStream* pstm, ULARGE_INTEGER cb, ULARGE_INTEGER* pcbRead, ULARGE_INTEGER* pcbWritten) = 0;
	virtual HRESULT STDMETHODCALLTYPE Commit(DWORD grfCommitFlags) = 0;
	virtual HRESULT STDMETHODCALLTYPE Revert() = 0;
	virtual HRESULT STDMETHODCALLTYPE LockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType) = 0;
	virtual HRESULT STDMETHODCALLTYPE UnlockRegion(ULARGE_INTEGER libOffset, ULARGE_INTEGER cb, DWORD dwLockType) = 0;
	virtual HRESULT STDMETHODCALLTYPE Stat(STATSTG* pstatstg, DWORD grfStatFlag) = 0;
	virtual HRESULT STDMETHODCALLTYPE Clone(IStream** ppstm) = 0;
	static REFIID iid() { return IID_IStream; }
};
// structured storage: no implementation here, only what passes a pointer around (AddRef, Release)
struct IStorage : public IUnknown
{
	static REFIID iid() { return IID_IStorage; }
};
typedef IStream* LPSTREAM;
typedef IStorage* LPSTORAGE;

struct DVTARGETDEVICE
{
	DWORD tdSize;
	WORD tdDriverNameOffset;
	WORD tdDeviceNameOffset;
	WORD tdPortNameOffset;
	WORD tdExtDevmodeOffset;
	BYTE tdData[1];
};

enum DVASPECT { DVASPECT_CONTENT = 1, DVASPECT_THUMBNAIL = 2, DVASPECT_ICON = 4, DVASPECT_DOCPRINT = 8 };
enum TYMED { TYMED_HGLOBAL = 1, TYMED_FILE = 2, TYMED_ISTREAM = 4, TYMED_ISTORAGE = 8, TYMED_GDI = 16, TYMED_MFPICT = 32,
	TYMED_ENHMF = 64, TYMED_NULL = 0 };
enum DATADIR { DATADIR_GET = 1, DATADIR_SET = 2 };
enum ADVF { ADVF_NODATA = 1, ADVF_PRIMEFIRST = 2, ADVF_ONLYONCE = 4, ADVF_DATAONSTOP = 64 };

struct FORMATETC
{
	CLIPFORMAT cfFormat;
	DVTARGETDEVICE* ptd;
	DWORD dwAspect;
	LONG lindex;
	DWORD tymed;
};
typedef FORMATETC* LPFORMATETC;

struct STGMEDIUM
{
	DWORD tymed;
	union
	{
		HBITMAP hBitmap;
		HMETAFILEPICT hMetaFilePict;
		HENHMETAFILE hEnhMetaFile;
		HGLOBAL hGlobal;
		LPOLESTR lpszFileName;
		IStream* pstm;
		IStorage* pstg;
	};
	IUnknown* pUnkForRelease;
};
typedef STGMEDIUM* LPSTGMEDIUM;

inline const IID IID_IEnumFORMATETC = {0x00000103, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
inline const IID IID_IDataObject = {0x0000010e, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
inline const IID IID_IAdviseSink = {0x0000010f, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
inline const IID IID_IEnumSTATDATA = {0x00000105, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};

struct IEnumFORMATETC : public IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE Next(ULONG celt, FORMATETC* rgelt, ULONG* pceltFetched) = 0;
	virtual HRESULT STDMETHODCALLTYPE Skip(ULONG celt) = 0;
	virtual HRESULT STDMETHODCALLTYPE Reset() = 0;
	virtual HRESULT STDMETHODCALLTYPE Clone(IEnumFORMATETC** ppenum) = 0;
	static REFIID iid() { return IID_IEnumFORMATETC; }
};
typedef IEnumFORMATETC* LPENUMFORMATETC;

struct IAdviseSink : public IUnknown
{
	virtual void STDMETHODCALLTYPE OnDataChange(FORMATETC* pFormatetc, STGMEDIUM* pStgmed) = 0;
	virtual void STDMETHODCALLTYPE OnViewChange(DWORD dwAspect, LONG lindex) = 0;
	virtual void STDMETHODCALLTYPE OnRename(void* pmk) = 0;
	virtual void STDMETHODCALLTYPE OnSave() = 0;
	virtual void STDMETHODCALLTYPE OnClose() = 0;
	static REFIID iid() { return IID_IAdviseSink; }
};

struct STATDATA
{
	FORMATETC formatetc;
	DWORD advf;
	IAdviseSink* pAdvSink;
	DWORD dwConnection;
};

struct IEnumSTATDATA : public IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE Next(ULONG celt, STATDATA* rgelt, ULONG* pceltFetched) = 0;
	virtual HRESULT STDMETHODCALLTYPE Skip(ULONG celt) = 0;
	virtual HRESULT STDMETHODCALLTYPE Reset() = 0;
	virtual HRESULT STDMETHODCALLTYPE Clone(IEnumSTATDATA** ppenum) = 0;
	static REFIID iid() { return IID_IEnumSTATDATA; }
};

struct IDataObject : public IUnknown
{
	virtual HRESULT STDMETHODCALLTYPE GetData(FORMATETC* pformatetcIn, STGMEDIUM* pmedium) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetDataHere(FORMATETC* pformatetc, STGMEDIUM* pmedium) = 0;
	virtual HRESULT STDMETHODCALLTYPE QueryGetData(FORMATETC* pformatetc) = 0;
	virtual HRESULT STDMETHODCALLTYPE GetCanonicalFormatEtc(FORMATETC* pformatectIn, FORMATETC* pformatetcOut) = 0;
	virtual HRESULT STDMETHODCALLTYPE SetData(FORMATETC* pformatetc, STGMEDIUM* pmedium, BOOL fRelease) = 0;
	virtual HRESULT STDMETHODCALLTYPE EnumFormatEtc(DWORD dwDirection, IEnumFORMATETC** ppenumFormatEtc) = 0;
	virtual HRESULT STDMETHODCALLTYPE DAdvise(FORMATETC* pformatetc, DWORD advf, IAdviseSink* pAdvSink, DWORD* pdwConnection) = 0;
	virtual HRESULT STDMETHODCALLTYPE DUnadvise(DWORD dwConnection) = 0;
	virtual HRESULT STDMETHODCALLTYPE EnumDAdvise(IEnumSTATDATA** ppenumAdvise) = 0;
	static REFIID iid() { return IID_IDataObject; }
};
typedef IDataObject* LPDATAOBJECT;

#define DV_E_FORMATETC ((HRESULT)0x80040064L)
#define DV_E_CLIPFORMAT ((HRESULT)0x8004006AL)
#define DV_E_STGMEDIUM ((HRESULT)0x80040066L)
#define DV_E_TYMED ((HRESULT)0x80040069L)
#define DV_E_LINDEX ((HRESULT)0x80040068L)
#define DV_E_DVASPECT ((HRESULT)0x8004006BL)
#define OLE_E_ADVISENOTSUPPORTED ((HRESULT)0x80040003L)
#define DATA_S_SAMEFORMATETC ((HRESULT)0x00040130L)
#define CLIPBRD_E_CANT_OPEN ((HRESULT)0x800401D0L)

inline const IID IID_IMalloc = {0x00000002, 0x0000, 0x0000, {0xC0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x46}};
struct IMalloc : public IUnknown
{
	virtual void* STDMETHODCALLTYPE Alloc(SIZE_T cb) = 0;
	virtual void* STDMETHODCALLTYPE Realloc(void* pv, SIZE_T cb) = 0;
	virtual void STDMETHODCALLTYPE Free(void* pv) = 0;
	virtual SIZE_T STDMETHODCALLTYPE GetSize(void* pv) = 0;
	virtual int STDMETHODCALLTYPE DidAlloc(void* pv) = 0;
	virtual void STDMETHODCALLTYPE HeapMinimize() = 0;
	static REFIID iid() { return IID_IMalloc; }
};
typedef IMalloc* LPMALLOC;
HRESULT CoGetMalloc(DWORD context, LPMALLOC* malloc); // CoTaskMemAlloc / CoTaskMemFree

void ReleaseStgMedium(STGMEDIUM* medium);
HANDLE OleDuplicateData(HANDLE src, CLIPFORMAT format, UINT flags);
#endif
