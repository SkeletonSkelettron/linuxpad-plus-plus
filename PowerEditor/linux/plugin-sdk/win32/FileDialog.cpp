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

// Notepad++ Linux plugin SDK, npp_win32 library: IFileOpenDialog / IFileSaveDialog with QFileDialog,
// IShellItem and IShellItemArray of file system paths.

#include <atomic>

#include <QDir>
#include <QFileDialog>
#include <QFileInfo>

#include "W32Internal.h"
#include <shobjidl.h>

using namespace w32;

namespace
{
	LPWSTR taskMemString(const QString& s)
	{
		const std::wstring w = ws(s);
		auto* out = static_cast<LPWSTR>(CoTaskMemAlloc((w.size() + 1) * sizeof(wchar_t)));
		if (out)
			std::wmemcpy(out, w.c_str(), w.size() + 1);
		return out;
	}

	template <typename... Interfaces> class ComObject : public Interfaces...
	{
	public:
		ULONG STDMETHODCALLTYPE AddRef() override { return ++_refs; }
		ULONG STDMETHODCALLTYPE Release() override
		{
			const ULONG n = --_refs;
			if (n == 0)
				delete this;
			return n;
		}

	protected:
		virtual ~ComObject() = default;

	private:
		std::atomic<ULONG> _refs{1};
	};

	class ShellItem : public ComObject<IShellItem>
	{
	public:
		explicit ShellItem(const QString& path) : _path(QDir::cleanPath(path)) {}
		const QString& path() const { return _path; }

		HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override
		{
			if (!out)
				return E_POINTER;
			if (iid == IID_IUnknown || iid == IID_IShellItem)
			{
				*out = static_cast<IShellItem*>(this);
				AddRef();
				return S_OK;
			}
			*out = nullptr;
			return E_NOINTERFACE;
		}
		HRESULT STDMETHODCALLTYPE BindToHandler(void*, REFGUID, REFIID, void** out) override
		{
			if (out)
				*out = nullptr;
			return E_NOTIMPL;
		}
		HRESULT STDMETHODCALLTYPE GetParent(IShellItem** parent) override
		{
			if (!parent)
				return E_POINTER;
			const QString dir = QFileInfo(_path).absolutePath();
			if (dir == _path)
			{
				*parent = nullptr;
				return E_FAIL;
			}
			*parent = new ShellItem(dir);
			return S_OK;
		}
		HRESULT STDMETHODCALLTYPE GetDisplayName(SIGDN form, LPWSTR* name) override
		{
			if (!name)
				return E_POINTER;
			switch (form)
			{
				case SIGDN_FILESYSPATH:
				case SIGDN_DESKTOPABSOLUTEPARSING:
				case SIGDN_DESKTOPABSOLUTEEDITING:
					*name = taskMemString(QDir::toNativeSeparators(_path));
					break;
				case SIGDN_URL:
					*name = taskMemString(QStringLiteral("file://") + _path);
					break;
				default:
					*name = taskMemString(QFileInfo(_path).fileName().isEmpty() ? _path : QFileInfo(_path).fileName());
					break;
			}
			return *name ? S_OK : E_OUTOFMEMORY;
		}
		HRESULT STDMETHODCALLTYPE GetAttributes(SFGAOF mask, SFGAOF* attributes) override
		{
			SFGAOF a = SFGAO_FILESYSTEM;
			if (QFileInfo(_path).isDir())
				a |= SFGAO_FOLDER;
			else
				a |= SFGAO_STREAM;
			if (attributes)
				*attributes = a & mask;
			return (a & mask) == mask ? S_OK : S_FALSE;
		}
		HRESULT STDMETHODCALLTYPE Compare(IShellItem* other, SICHINTF, int* order) override
		{
			auto* o = dynamic_cast<ShellItem*>(other);
			const int c = o ? _path.compare(o->_path) : 1;
			if (order)
				*order = c;
			return c == 0 ? S_OK : S_FALSE;
		}

	private:
		QString _path;
	};

	class ShellItemArray : public ComObject<IShellItemArray>
	{
	public:
		explicit ShellItemArray(const QStringList& paths) : _paths(paths) {}
		HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override
		{
			if (!out)
				return E_POINTER;
			if (iid == IID_IUnknown || iid == IID_IShellItemArray)
			{
				*out = static_cast<IShellItemArray*>(this);
				AddRef();
				return S_OK;
			}
			*out = nullptr;
			return E_NOINTERFACE;
		}
		HRESULT STDMETHODCALLTYPE BindToHandler(void*, REFGUID, REFIID, void** out) override { if (out) *out = nullptr; return E_NOTIMPL; }
		HRESULT STDMETHODCALLTYPE GetPropertyStore(int, REFIID, void** out) override { if (out) *out = nullptr; return E_NOTIMPL; }
		HRESULT STDMETHODCALLTYPE GetPropertyDescriptionList(const void*, REFIID, void** out) override { if (out) *out = nullptr; return E_NOTIMPL; }
		HRESULT STDMETHODCALLTYPE GetAttributes(int, SFGAOF, SFGAOF* attributes) override { if (attributes) *attributes = 0; return E_NOTIMPL; }
		HRESULT STDMETHODCALLTYPE GetCount(DWORD* count) override
		{
			if (!count)
				return E_POINTER;
			*count = static_cast<DWORD>(_paths.size());
			return S_OK;
		}
		HRESULT STDMETHODCALLTYPE GetItemAt(DWORD index, IShellItem** item) override
		{
			if (!item)
				return E_POINTER;
			if (index >= static_cast<DWORD>(_paths.size()))
			{
				*item = nullptr;
				return E_INVALIDARG;
			}
			*item = new ShellItem(_paths.at(static_cast<qsizetype>(index)));
			return S_OK;
		}
		HRESULT STDMETHODCALLTYPE EnumItems(void** items) override { if (items) *items = nullptr; return E_NOTIMPL; }

	private:
		QStringList _paths;
	};

	class FileDialog : public ComObject<IFileOpenDialog, IFileSaveDialog>
	{
	public:
		explicit FileDialog(bool save) : _save(save)
		{
			_options = save ? (FOS_OVERWRITEPROMPT | FOS_NOREADONLYRETURN | FOS_PATHMUSTEXIST)
				: (FOS_PATHMUSTEXIST | FOS_FILEMUSTEXIST);
		}

		HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override
		{
			if (!out)
				return E_POINTER;
			*out = nullptr;
			if (iid == IID_IUnknown || iid == IID_IModalWindow || iid == IID_IFileDialog)
				*out = _save ? static_cast<IFileDialog*>(static_cast<IFileSaveDialog*>(this)) : static_cast<IFileDialog*>(static_cast<IFileOpenDialog*>(this));
			else if (iid == IID_IFileOpenDialog && !_save)
				*out = static_cast<IFileOpenDialog*>(this);
			else if (iid == IID_IFileSaveDialog && _save)
				*out = static_cast<IFileSaveDialog*>(this);
			if (!*out)
				return E_NOINTERFACE;
			AddRef();
			return S_OK;
		}

		HRESULT STDMETHODCALLTYPE Show(HWND owner) override
		{
			QFileDialog dlg(widgetOf(owner));
			dlg.setWindowTitle(_title.isEmpty() ? (_save ? QFileDialog::tr("Save As") : QFileDialog::tr("Open")) : _title);
			dlg.setAcceptMode(_save ? QFileDialog::AcceptSave : QFileDialog::AcceptOpen);
			if (_options & FOS_PICKFOLDERS)
			{
				dlg.setFileMode(QFileDialog::Directory);
				dlg.setOption(QFileDialog::ShowDirsOnly, true);
			}
			else if (_save)
				dlg.setFileMode(QFileDialog::AnyFile);
			else
				dlg.setFileMode((_options & FOS_ALLOWMULTISELECT) ? QFileDialog::ExistingFiles : QFileDialog::ExistingFile);
			dlg.setOption(QFileDialog::DontConfirmOverwrite, !(_options & FOS_OVERWRITEPROMPT));
			dlg.setOption(QFileDialog::DontResolveSymlinks, (_options & FOS_NODEREFERENCELINKS) != 0);
			if (_options & FOS_FORCESHOWHIDDEN)
				dlg.setFilter(dlg.filter() | QDir::Hidden);
			if (!_filters.isEmpty())
			{
				dlg.setNameFilters(_filters);
				if (_filterIndex >= 1 && _filterIndex <= static_cast<UINT>(_filters.size()))
					dlg.selectNameFilter(_filters.at(static_cast<qsizetype>(_filterIndex - 1)));
			}
			if (!_extension.isEmpty())
				dlg.setDefaultSuffix(_extension);
			const QString folder = !_folder.isEmpty() ? _folder : _defaultFolder;
			if (!folder.isEmpty())
				dlg.setDirectory(folder);
			if (!_fileName.isEmpty())
				dlg.selectFile(_fileName);
			if (!_okLabel.isEmpty())
				dlg.setLabelText(QFileDialog::Accept, _okLabel);
			if (!_fileNameLabel.isEmpty())
				dlg.setLabelText(QFileDialog::FileName, _fileNameLabel);
			if (dlg.exec() != QDialog::Accepted || dlg.selectedFiles().isEmpty())
				return HRESULT_FROM_WIN32(ERROR_CANCELLED);
			_results = dlg.selectedFiles();
			const qsizetype filter = _filters.indexOf(dlg.selectedNameFilter());
			if (filter >= 0)
				_filterIndex = static_cast<UINT>(filter + 1);
			return S_OK;
		}

		HRESULT STDMETHODCALLTYPE SetFileTypes(UINT count, const COMDLG_FILTERSPEC* types) override
		{
			_filters.clear();
			for (UINT i = 0; types && i < count; ++i)
			{
				const QString spec = qs(types[i].pszSpec).replace(QLatin1Char(';'), QLatin1Char(' ')).trimmed();
				_filters << QStringLiteral("%1 (%2)").arg(qs(types[i].pszName), spec.isEmpty() ? QStringLiteral("*") : spec);
			}
			return S_OK;
		}
		HRESULT STDMETHODCALLTYPE SetFileTypeIndex(UINT index) override { _filterIndex = index; return S_OK; }
		HRESULT STDMETHODCALLTYPE GetFileTypeIndex(UINT* index) override
		{
			if (!index)
				return E_POINTER;
			*index = _filterIndex ? _filterIndex : 1;
			return S_OK;
		}
		HRESULT STDMETHODCALLTYPE Advise(IFileDialogEvents*, DWORD* cookie) override
		{
			// the events are not sent: the result is checked after Show
			if (cookie)
				*cookie = 1;
			return S_OK;
		}
		HRESULT STDMETHODCALLTYPE Unadvise(DWORD) override { return S_OK; }
		HRESULT STDMETHODCALLTYPE SetOptions(FILEOPENDIALOGOPTIONS options) override { _options = options; return S_OK; }
		HRESULT STDMETHODCALLTYPE GetOptions(FILEOPENDIALOGOPTIONS* options) override
		{
			if (!options)
				return E_POINTER;
			*options = _options;
			return S_OK;
		}
		HRESULT STDMETHODCALLTYPE SetDefaultFolder(IShellItem* folder) override { _defaultFolder = pathOf(folder); return S_OK; }
		HRESULT STDMETHODCALLTYPE SetFolder(IShellItem* folder) override { _folder = pathOf(folder); return S_OK; }
		HRESULT STDMETHODCALLTYPE GetFolder(IShellItem** folder) override
		{
			if (!folder)
				return E_POINTER;
			const QString f = !_results.isEmpty() ? QFileInfo(_results.first()).absolutePath() : (!_folder.isEmpty() ? _folder : _defaultFolder);
			*folder = f.isEmpty() ? nullptr : new ShellItem(f);
			return *folder ? S_OK : E_FAIL;
		}
		HRESULT STDMETHODCALLTYPE GetCurrentSelection(IShellItem** item) override { return GetResult(item); }
		HRESULT STDMETHODCALLTYPE SetFileName(LPCWSTR name) override { _fileName = name ? nativePath(name) : QString(); return S_OK; }
		HRESULT STDMETHODCALLTYPE GetFileName(LPWSTR* name) override
		{
			if (!name)
				return E_POINTER;
			*name = taskMemString(!_results.isEmpty() ? QFileInfo(_results.first()).fileName() : _fileName);
			return S_OK;
		}
		HRESULT STDMETHODCALLTYPE SetTitle(LPCWSTR title) override { _title = title ? qs(title) : QString(); return S_OK; }
		HRESULT STDMETHODCALLTYPE SetOkButtonLabel(LPCWSTR text) override { _okLabel = text ? qs(text) : QString(); return S_OK; }
		HRESULT STDMETHODCALLTYPE SetFileNameLabel(LPCWSTR text) override { _fileNameLabel = text ? qs(text) : QString(); return S_OK; }
		HRESULT STDMETHODCALLTYPE GetResult(IShellItem** item) override
		{
			if (!item)
				return E_POINTER;
			if (_results.isEmpty())
			{
				*item = nullptr;
				return E_UNEXPECTED;
			}
			*item = new ShellItem(_results.first());
			return S_OK;
		}
		HRESULT STDMETHODCALLTYPE AddPlace(IShellItem*, FDAP) override { return S_OK; }
		HRESULT STDMETHODCALLTYPE SetDefaultExtension(LPCWSTR extension) override
		{
			_extension = extension ? qs(extension) : QString();
			if (_extension.startsWith(QLatin1Char('.')))
				_extension.remove(0, 1);
			return S_OK;
		}
		HRESULT STDMETHODCALLTYPE Close(HRESULT) override { return S_OK; }
		HRESULT STDMETHODCALLTYPE SetClientGuid(REFGUID) override { return S_OK; }
		HRESULT STDMETHODCALLTYPE ClearClientData() override { return S_OK; }
		HRESULT STDMETHODCALLTYPE SetFilter(void*) override { return S_OK; }

		// IFileOpenDialog
		HRESULT STDMETHODCALLTYPE GetResults(IShellItemArray** items) override
		{
			if (!items)
				return E_POINTER;
			*items = new ShellItemArray(_results);
			return S_OK;
		}
		HRESULT STDMETHODCALLTYPE GetSelectedItems(IShellItemArray** items) override { return GetResults(items); }

		// IFileSaveDialog
		HRESULT STDMETHODCALLTYPE SetSaveAsItem(IShellItem* item) override
		{
			const QString p = pathOf(item);
			if (!p.isEmpty())
			{
				_folder = QFileInfo(p).absolutePath();
				_fileName = QFileInfo(p).fileName();
			}
			return S_OK;
		}
		HRESULT STDMETHODCALLTYPE SetProperties(void*) override { return S_OK; }
		HRESULT STDMETHODCALLTYPE SetCollectedProperties(void*, BOOL) override { return S_OK; }
		HRESULT STDMETHODCALLTYPE GetProperties(void** store) override { if (store) *store = nullptr; return E_NOTIMPL; }
		HRESULT STDMETHODCALLTYPE ApplyProperties(IShellItem*, void*, HWND, void*) override { return E_NOTIMPL; }

	private:
		static QString pathOf(IShellItem* item)
		{
			if (auto* s = dynamic_cast<ShellItem*>(item))
				return s->path();
			LPWSTR name = nullptr;
			QString p;
			if (item && SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &name)) && name)
			{
				p = nativePath(name);
				CoTaskMemFree(name);
			}
			return p;
		}

		bool _save;
		FILEOPENDIALOGOPTIONS _options = 0;
		QStringList _filters;
		UINT _filterIndex = 1;
		QString _title, _okLabel, _fileNameLabel, _fileName, _folder, _defaultFolder, _extension;
		QStringList _results;
	};
}

namespace w32
{
	// CoCreateInstance of the objects made here
	HRESULT createShellObject(REFCLSID clsid, REFIID iid, void** out)
	{
		if (clsid != CLSID_FileOpenDialog && clsid != CLSID_FileSaveDialog)
			return REGDB_E_CLASSNOTREG;
		auto* dialog = new FileDialog(clsid == CLSID_FileSaveDialog);
		const HRESULT hr = dialog->QueryInterface(iid, out);
		static_cast<IFileOpenDialog*>(dialog)->Release();
		return hr;
	}
}

HRESULT SHCreateItemFromParsingName(PCWSTR path, void*, REFIID iid, void** out)
{
	if (!out)
		return E_POINTER;
	*out = nullptr;
	if (!path)
		return E_INVALIDARG;
	const QString p = nativePath(path);
	if (!QFileInfo::exists(p))
		return HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND);
	auto* item = new ShellItem(QFileInfo(p).absoluteFilePath());
	const HRESULT hr = item->QueryInterface(iid, out);
	item->Release();
	return hr;
}
