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

// Notepad++ Linux plugin SDK, npp_win32 library: OLE drag and drop and clipboard on the ones of Qt.
// - DoDragDrop: the files (CF_HDROP) or the text of the data object in a QDrag.
// - RegisterDragDrop: the drags over the window go to the IDropTarget, with a data object of their files and text.
// - OleSetClipboard / OleGetClipboard: files as URLs (and x-special/gnome-copied-files for cut and copy).

#include <QAbstractScrollArea>
#include <QApplication>
#include <QClipboard>
#include <QCursor>
#include <QDir>
#include <QDrag>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QUrl>

#include <cstring>
#include <map>

#include "W32Internal.h"
#include <shlobj.h>

using namespace w32;

namespace
{
	const QString kGnomeCopied = QStringLiteral("x-special/gnome-copied-files");
	const QString kKdeCut = QStringLiteral("application/x-kde-cutselection");

	UINT preferredEffectFormat()
	{
		static const UINT f = RegisterClipboardFormatW(CFSTR_PREFERREDDROPEFFECT);
		return f;
	}

	// data object of files and text (the data of a Qt drag or of the clipboard)
	class MimeDataObject : public IDataObject
	{
	public:
		MimeDataObject(QStringList files, QString text, DWORD effect) : _files(std::move(files)), _text(std::move(text)), _effect(effect) {}

		HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override
		{
			if (!out)
				return E_POINTER;
			if (iid == IID_IUnknown || iid == IID_IDataObject)
			{
				*out = static_cast<IDataObject*>(this);
				AddRef();
				return S_OK;
			}
			*out = nullptr;
			return E_NOINTERFACE;
		}
		ULONG STDMETHODCALLTYPE AddRef() override { return ++_ref; }
		ULONG STDMETHODCALLTYPE Release() override
		{
			const ULONG r = --_ref;
			if (!r)
				delete this;
			return r;
		}

		HRESULT STDMETHODCALLTYPE GetData(FORMATETC* f, STGMEDIUM* m) override
		{
			const HRESULT q = QueryGetData(f);
			if (q != S_OK)
				return q;
			std::memset(m, 0, sizeof(*m));
			m->tymed = TYMED_HGLOBAL;
			if (f->cfFormat == CF_HDROP)
				m->hGlobal = makeDropFiles(_files, POINT{});
			else if (f->cfFormat == CF_UNICODETEXT)
			{
				const std::wstring w = ws(_text);
				m->hGlobal = GlobalAlloc(GMEM_ZEROINIT, (w.size() + 1) * sizeof(wchar_t));
				std::wmemcpy(static_cast<wchar_t*>(m->hGlobal), w.c_str(), w.size() + 1);
			}
			else if (f->cfFormat == CF_TEXT || f->cfFormat == CF_OEMTEXT)
			{
				const QByteArray u = _text.toUtf8();
				m->hGlobal = GlobalAlloc(GMEM_ZEROINIT, static_cast<SIZE_T>(u.size()) + 1);
				std::memcpy(m->hGlobal, u.constData(), static_cast<size_t>(u.size()));
			}
			else
			{
				m->hGlobal = GlobalAlloc(GMEM_ZEROINIT, sizeof(DWORD));
				std::memcpy(m->hGlobal, &_effect, sizeof(DWORD));
			}
			return m->hGlobal ? S_OK : E_OUTOFMEMORY;
		}
		HRESULT STDMETHODCALLTYPE GetDataHere(FORMATETC*, STGMEDIUM*) override { return E_NOTIMPL; }
		HRESULT STDMETHODCALLTYPE QueryGetData(FORMATETC* f) override
		{
			if (!f)
				return E_INVALIDARG;
			if (!(f->tymed & TYMED_HGLOBAL))
				return DV_E_TYMED;
			for (CLIPFORMAT c : formats())
			{
				if (c == f->cfFormat)
					return S_OK;
			}
			return DV_E_FORMATETC;
		}
		HRESULT STDMETHODCALLTYPE GetCanonicalFormatEtc(FORMATETC*, FORMATETC* out) override
		{
			if (out)
				out->ptd = nullptr;
			return E_NOTIMPL;
		}
		HRESULT STDMETHODCALLTYPE SetData(FORMATETC*, STGMEDIUM*, BOOL) override { return E_NOTIMPL; }
		HRESULT STDMETHODCALLTYPE EnumFormatEtc(DWORD dir, IEnumFORMATETC** out) override;
		HRESULT STDMETHODCALLTYPE DAdvise(FORMATETC*, DWORD, IAdviseSink*, DWORD*) override { return OLE_E_ADVISENOTSUPPORTED; }
		HRESULT STDMETHODCALLTYPE DUnadvise(DWORD) override { return OLE_E_ADVISENOTSUPPORTED; }
		HRESULT STDMETHODCALLTYPE EnumDAdvise(IEnumSTATDATA**) override { return OLE_E_ADVISENOTSUPPORTED; }

		std::vector<CLIPFORMAT> formats() const
		{
			std::vector<CLIPFORMAT> f;
			if (!_files.isEmpty())
			{
				f.push_back(CF_HDROP);
				f.push_back(static_cast<CLIPFORMAT>(preferredEffectFormat()));
			}
			if (!_text.isEmpty())
			{
				f.push_back(CF_UNICODETEXT);
				f.push_back(CF_TEXT);
			}
			return f;
		}

	private:
		virtual ~MimeDataObject() = default;
		ULONG _ref = 1;
		QStringList _files;
		QString _text;
		DWORD _effect;
	};

	class FormatEnum : public IEnumFORMATETC
	{
	public:
		explicit FormatEnum(std::vector<CLIPFORMAT> formats, size_t at = 0) : _formats(std::move(formats)), _at(at) {}
		HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override
		{
			if (!out)
				return E_POINTER;
			if (iid == IID_IUnknown || iid == IID_IEnumFORMATETC)
			{
				*out = static_cast<IEnumFORMATETC*>(this);
				AddRef();
				return S_OK;
			}
			*out = nullptr;
			return E_NOINTERFACE;
		}
		ULONG STDMETHODCALLTYPE AddRef() override { return ++_ref; }
		ULONG STDMETHODCALLTYPE Release() override
		{
			const ULONG r = --_ref;
			if (!r)
				delete this;
			return r;
		}
		HRESULT STDMETHODCALLTYPE Next(ULONG n, FORMATETC* out, ULONG* fetched) override
		{
			ULONG i = 0;
			for (; i < n && _at < _formats.size(); ++i, ++_at)
				out[i] = FORMATETC{_formats[_at], nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
			if (fetched)
				*fetched = i;
			return i == n ? S_OK : S_FALSE;
		}
		HRESULT STDMETHODCALLTYPE Skip(ULONG n) override
		{
			_at += n;
			return _at <= _formats.size() ? S_OK : S_FALSE;
		}
		HRESULT STDMETHODCALLTYPE Reset() override
		{
			_at = 0;
			return S_OK;
		}
		HRESULT STDMETHODCALLTYPE Clone(IEnumFORMATETC** out) override
		{
			if (!out)
				return E_POINTER;
			*out = new FormatEnum(_formats, _at);
			return S_OK;
		}

	private:
		virtual ~FormatEnum() = default;
		ULONG _ref = 1;
		std::vector<CLIPFORMAT> _formats;
		size_t _at;
	};

	HRESULT MimeDataObject::EnumFormatEtc(DWORD dir, IEnumFORMATETC** out)
	{
		if (!out)
			return E_POINTER;
		if (dir != DATADIR_GET)
		{
			*out = nullptr;
			return E_NOTIMPL;
		}
		*out = new FormatEnum(formats());
		return S_OK;
	}

	QStringList localFiles(const QMimeData* md)
	{
		QStringList files;
		if (!md)
			return files;
		for (const QUrl& u : md->urls())
		{
			if (u.isLocalFile())
				files << u.toLocalFile();
		}
		return files;
	}

	IDataObject* dataObjectOf(const QMimeData* md)
	{
		const QStringList files = localFiles(md);
		const QString text = md && md->hasText() && files.isEmpty() ? md->text() : QString();
		return new MimeDataObject(files, text, filesEffectOf(md));
	}

	// files and text of a data object
	void readDataObject(IDataObject* data, QStringList& files, QString& text, DWORD& effect)
	{
		FORMATETC f{CF_HDROP, nullptr, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
		STGMEDIUM m{};
		if (data->GetData(&f, &m) == S_OK)
		{
			files = dropFilesList(m.hGlobal);
			ReleaseStgMedium(&m);
		}
		f.cfFormat = static_cast<CLIPFORMAT>(preferredEffectFormat());
		m = STGMEDIUM{};
		if (data->GetData(&f, &m) == S_OK)
		{
			if (m.hGlobal && GlobalSize(m.hGlobal) >= 1)
			{
				DWORD e = 0;
				std::memcpy(&e, m.hGlobal, std::min<size_t>(sizeof(DWORD), GlobalSize(m.hGlobal)));
				effect = e;
			}
			ReleaseStgMedium(&m);
		}
		f.cfFormat = CF_UNICODETEXT;
		m = STGMEDIUM{};
		if (data->GetData(&f, &m) == S_OK)
		{
			if (m.hGlobal)
				text = qs(static_cast<const wchar_t*>(m.hGlobal));
			ReleaseStgMedium(&m);
		}
		else
		{
			f.cfFormat = CF_TEXT;
			m = STGMEDIUM{};
			if (data->GetData(&f, &m) == S_OK)
			{
				if (m.hGlobal)
					text = qsA(static_cast<const char*>(m.hGlobal));
				ReleaseStgMedium(&m);
			}
		}
	}

	Qt::DropActions actionsOf(DWORD effects)
	{
		Qt::DropActions a;
		if (effects & DROPEFFECT_COPY)
			a |= Qt::CopyAction;
		if (effects & DROPEFFECT_MOVE)
			a |= Qt::MoveAction;
		if (effects & DROPEFFECT_LINK)
			a |= Qt::LinkAction;
		return a;
	}

	DWORD effectsOf(Qt::DropActions a)
	{
		DWORD e = DROPEFFECT_NONE;
		if (a & Qt::CopyAction)
			e |= DROPEFFECT_COPY;
		if (a & Qt::MoveAction)
			e |= DROPEFFECT_MOVE;
		if (a & Qt::LinkAction)
			e |= DROPEFFECT_LINK;
		return e;
	}

	Qt::DropAction actionOf(DWORD effect)
	{
		if (effect & DROPEFFECT_MOVE)
			return Qt::MoveAction;
		if (effect & DROPEFFECT_COPY)
			return Qt::CopyAction;
		if (effect & DROPEFFECT_LINK)
			return Qt::LinkAction;
		return Qt::IgnoreAction;
	}

	DWORD keyStateOf(Qt::MouseButtons buttons, Qt::KeyboardModifiers mods)
	{
		DWORD k = 0;
		if (buttons & Qt::LeftButton)
			k |= MK_LBUTTON;
		if (buttons & Qt::RightButton)
			k |= MK_RBUTTON;
		if (buttons & Qt::MiddleButton)
			k |= MK_MBUTTON;
		if (mods & Qt::ShiftModifier)
			k |= MK_SHIFT;
		if (mods & Qt::ControlModifier)
			k |= MK_CONTROL;
		if (mods & Qt::AltModifier)
			k |= MK_ALT;
		return k;
	}

	// the drops on a window of RegisterDragDrop
	class DropFilter : public QObject
	{
	public:
		DropFilter(IDropTarget* target, QWidget* widget) : QObject(widget), _target(target), _widget(widget)
		{
			_target->AddRef();
		}
		~DropFilter() override
		{
			// the target is released by RevokeDragDrop only: like Windows, a window destroyed without it keeps its
			// reference (plugins register objects that must not be deleted, Explorer: a member of a global)
			if (_data)
				_data->Release();
		}

		bool eventFilter(QObject*, QEvent* e) override
		{
			switch (e->type())
			{
				case QEvent::DragEnter:
				{
					auto* de = static_cast<QDragEnterEvent*>(e);
					if (_data)
						_data->Release();
					_data = dataObjectOf(de->mimeData());
					DWORD effect = effectsOf(de->possibleActions());
					_target->DragEnter(_data, keyStateOf(de->buttons(), de->modifiers()), pointOf(de->position()), &effect);
					// accepted in any case: DragOver can allow the drop later (the keys pressed change the effect)
					de->setDropAction(effect ? actionOf(effect) : de->proposedAction());
					de->accept();
					return true;
				}
				case QEvent::DragMove:
				{
					auto* de = static_cast<QDragMoveEvent*>(e);
					DWORD effect = effectsOf(de->possibleActions());
					_target->DragOver(keyStateOf(de->buttons(), de->modifiers()), pointOf(de->position()), &effect);
					if (effect)
					{
						de->setDropAction(actionOf(effect));
						de->accept();
					}
					else
						de->ignore();
					return true;
				}
				case QEvent::DragLeave:
					_target->DragLeave();
					if (_data)
					{
						_data->Release();
						_data = nullptr;
					}
					return true;
				case QEvent::Drop:
				{
					auto* de = static_cast<QDropEvent*>(e);
					IDataObject* data = _data ? _data : dataObjectOf(de->mimeData());
					_data = nullptr;
					DWORD effect = effectsOf(de->possibleActions());
					_target->Drop(data, keyStateOf(de->buttons(), de->modifiers()), pointOf(de->position()), &effect);
					data->Release();
					// DROPEFFECT_NONE after a drop: the target moved the files itself (Windows: "optimized move"),
					// the source must not delete them, so it is told they were copied
					de->setDropAction(effect ? actionOf(effect) : Qt::CopyAction);
					de->accept();
					return true;
				}
				default:
					return false;
			}
		}

		IDropTarget* target() const { return _target; }

	private:
		POINTL pointOf(const QPointF& p) const
		{
			const QPoint g = _widget ? _widget->mapToGlobal(p.toPoint()) : p.toPoint();
			return POINTL{g.x(), g.y()};
		}

		IDropTarget* _target;
		QPointer<QWidget> _widget;
		IDataObject* _data = nullptr;
	};

	std::map<HWND, QPointer<DropFilter>>& dropTargets()
	{
		static std::map<HWND, QPointer<DropFilter>> m;
		return m;
	}

	struct OleClipboard
	{
		IDataObject* data = nullptr;
	};

	OleClipboard& oleClipboard()
	{
		static OleClipboard c;
		return c;
	}
}

namespace w32
{
	bool& dragDone()
	{
		static bool done = false;
		return done;
	}

	HGLOBAL makeDropFiles(const QStringList& files, POINT pt)
	{
		std::wstring paths;
		for (const QString& f : files)
		{
			paths += ws(f);
			paths += L'\0';
		}
		paths += L'\0';
		const SIZE_T size = sizeof(DROPFILES) + paths.size() * sizeof(wchar_t);
		HGLOBAL h = GlobalAlloc(GMEM_ZEROINIT, size);
		if (!h)
			return nullptr;
		auto* df = static_cast<DROPFILES*>(h);
		df->pFiles = sizeof(DROPFILES);
		df->pt = pt;
		df->fNC = FALSE;
		df->fWide = TRUE;
		std::memcpy(static_cast<BYTE*>(h) + sizeof(DROPFILES), paths.data(), paths.size() * sizeof(wchar_t));
		return h;
	}

	QStringList dropFilesList(const void* drop)
	{
		QStringList files;
		const auto* df = static_cast<const DROPFILES*>(drop);
		if (!df || df->pFiles < sizeof(DWORD))
			return files;
		const SIZE_T size = GlobalSize(const_cast<void*>(drop));
		const BYTE* base = static_cast<const BYTE*>(drop);
		if (df->fWide)
		{
			const auto* p = reinterpret_cast<const wchar_t*>(base + df->pFiles);
			const auto* end = reinterpret_cast<const wchar_t*>(base + (size ? size : df->pFiles + 65536));
			while (p < end && *p)
			{
				const size_t n = wcsnlen(p, static_cast<size_t>(end - p));
				files << nativePath(QString::fromWCharArray(p, static_cast<int>(n)));
				p += n + 1;
			}
		}
		else
		{
			const auto* p = reinterpret_cast<const char*>(base + df->pFiles);
			const auto* end = reinterpret_cast<const char*>(base + (size ? size : df->pFiles + 65536));
			while (p < end && *p)
			{
				const size_t n = strnlen(p, static_cast<size_t>(end - p));
				files << nativePath(QString::fromUtf8(p, static_cast<int>(n)));
				p += n + 1;
			}
		}
		return files;
	}

	void setFilesMime(QMimeData* md, const QStringList& files, DWORD effect)
	{
		QList<QUrl> urls;
		for (const QString& f : files)
			urls << QUrl::fromLocalFile(f);
		md->setUrls(urls);
		if (effect)
			setFilesEffect(md, effect);
	}

	void setFilesEffect(QMimeData* md, DWORD effect)
	{
		// cut (move) or copy, for Explorer-like plugins and for the file managers
		const bool cut = (effect & DROPEFFECT_MOVE) && !(effect & DROPEFFECT_COPY);
		QByteArray gnome = cut ? "cut" : "copy";
		for (const QUrl& u : md->urls())
			gnome += '\n' + u.toEncoded();
		md->setData(kGnomeCopied, gnome);
		md->setData(kKdeCut, cut ? "1" : "0");
		const DWORD e = effect;
		md->setData(QStringLiteral("application/x-qt-windows-mime;value=\"%1\"").arg(qs(CFSTR_PREFERREDDROPEFFECT)),
			QByteArray(reinterpret_cast<const char*>(&e), sizeof(e)));
	}

	DWORD filesEffectOf(const QMimeData* md)
	{
		if (!md)
			return DROPEFFECT_NONE;
		const QString mime = QStringLiteral("application/x-qt-windows-mime;value=\"%1\"").arg(qs(CFSTR_PREFERREDDROPEFFECT));
		if (md->hasFormat(mime))
		{
			const QByteArray b = md->data(mime);
			DWORD e = 0;
			std::memcpy(&e, b.constData(), std::min<size_t>(sizeof(e), static_cast<size_t>(b.size())));
			return e;
		}
		if (md->hasFormat(kGnomeCopied))
			return md->data(kGnomeCopied).startsWith("cut") ? DROPEFFECT_MOVE : (DROPEFFECT_COPY | DROPEFFECT_LINK);
		if (md->hasFormat(kKdeCut))
			return md->data(kKdeCut).startsWith('1') ? DROPEFFECT_MOVE : (DROPEFFECT_COPY | DROPEFFECT_LINK);
		return md->hasUrls() ? (DROPEFFECT_COPY | DROPEFFECT_LINK) : DROPEFFECT_NONE;
	}
}

// ---- STGMEDIUM ------------------------------------------------------------------------------
void ReleaseStgMedium(STGMEDIUM* m)
{
	if (!m)
		return;
	if (m->pUnkForRelease)
	{
		m->pUnkForRelease->Release();
		m->pUnkForRelease = nullptr;
	}
	else
	{
		switch (m->tymed)
		{
			case TYMED_HGLOBAL:
				GlobalFree(m->hGlobal);
				break;
			case TYMED_GDI:
				DeleteObject(m->hBitmap);
				break;
			case TYMED_FILE:
				CoTaskMemFree(m->lpszFileName);
				break;
			case TYMED_ISTREAM:
			case TYMED_ISTORAGE:
				if (m->pstm)
					reinterpret_cast<IUnknown*>(m->pstm)->Release();
				break;
			default:
				break;
		}
	}
	m->tymed = TYMED_NULL;
	m->hGlobal = nullptr;
}

HANDLE OleDuplicateData(HANDLE src, CLIPFORMAT format, UINT flags)
{
	Q_UNUSED(flags);
	if (!src)
		return nullptr;
	if (format == CF_BITMAP)
		return CopyImage(src, IMAGE_BITMAP, 0, 0, 0);
	const SIZE_T size = GlobalSize(src);
	if (!size)
		return nullptr;
	HGLOBAL h = GlobalAlloc(GMEM_ZEROINIT, size);
	if (h)
		std::memcpy(h, src, size);
	return h;
}

// ---- drag and drop ------------------------------------------------------------------------------
HRESULT RegisterDragDrop(HWND hwnd, IDropTarget* target)
{
	if (!target)
		return E_INVALIDARG;
	QWidget* w = widgetOf(hwnd);
	if (!w)
		return DRAGDROP_E_INVALIDHWND;
	auto& targets = dropTargets();
	if (auto it = targets.find(hwnd); it != targets.end() && it->second)
		return DRAGDROP_E_ALREADYREGISTERED;
	// the drops of a list or a tree arrive on its viewport
	QWidget* receiver = w;
	if (auto* area = qobject_cast<QAbstractScrollArea*>(w))
		receiver = area->viewport();
	auto* filter = new DropFilter(target, receiver);
	receiver->setAcceptDrops(true);
	receiver->installEventFilter(filter);
	targets[hwnd] = filter;
	return S_OK;
}

HRESULT RevokeDragDrop(HWND hwnd)
{
	auto& targets = dropTargets();
	auto it = targets.find(hwnd);
	if (it == targets.end())
		return DRAGDROP_E_NOTREGISTERED;
	if (DropFilter* f = it->second)
	{
		if (auto* receiver = qobject_cast<QWidget*>(f->parent()))
			receiver->setAcceptDrops(false);
		IDropTarget* target = f->target();
		delete f;
		target->Release();
	}
	targets.erase(it);
	return S_OK;
}

HRESULT DoDragDrop(IDataObject* data, IDropSource* source, DWORD okEffects, DWORD* effect)
{
	if (effect)
		*effect = DROPEFFECT_NONE;
	if (!data)
		return E_INVALIDARG;
	QStringList files;
	QString text;
	DWORD preferred = DROPEFFECT_NONE;
	readDataObject(data, files, text, preferred);
	if (files.isEmpty() && text.isEmpty())
		return E_UNEXPECTED;
	auto* md = new QMimeData;
	if (!files.isEmpty())
		setFilesMime(md, files, DROPEFFECT_NONE);
	else
		md->setText(text);
	QWidget* from = QApplication::widgetAt(QCursor::pos());
	if (!from)
		from = QApplication::activeWindow();
	if (!from)
	{
		delete md;
		return E_UNEXPECTED;
	}
	dragDone() = true;
	auto* drag = new QDrag(from);
	drag->setMimeData(md);
	const Qt::DropActions actions = actionsOf(okEffects);
	Qt::DropAction def = Qt::IgnoreAction;
	if (actions & Qt::MoveAction)
		def = Qt::MoveAction;
	else if (actions & Qt::CopyAction)
		def = Qt::CopyAction;
	const Qt::DropAction done = drag->exec(actions ? actions : Qt::CopyAction, def);
	drag->deleteLater();
	if (source)
		source->QueryContinueDrag(FALSE, 0);
	if (done == Qt::IgnoreAction)
		return DRAGDROP_S_CANCEL;
	if (effect)
		*effect = effectsOf(done);
	return DRAGDROP_S_DROP;
}

// ---- clipboard ------------------------------------------------------------------------------------
HRESULT OleSetClipboard(IDataObject* data)
{
	OleClipboard& c = oleClipboard();
	if (c.data)
	{
		c.data->Release();
		c.data = nullptr;
	}
	if (!data)
	{
		QApplication::clipboard()->clear();
		return S_OK;
	}
	QStringList files;
	QString text;
	DWORD effect = DROPEFFECT_NONE;
	readDataObject(data, files, text, effect);
	auto* md = new QMimeData;
	if (!files.isEmpty())
	{
		setFilesMime(md, files, effect ? effect : (DROPEFFECT_COPY | DROPEFFECT_LINK));
		md->setText(files.join(QLatin1Char('\n')));
	}
	else if (!text.isEmpty())
		md->setText(text);
	QApplication::clipboard()->setMimeData(md);
	data->AddRef();
	c.data = data;
	return S_OK;
}

HRESULT OleFlushClipboard()
{
	// the data is already in the clipboard of Qt
	OleClipboard& c = oleClipboard();
	if (c.data)
	{
		c.data->Release();
		c.data = nullptr;
	}
	return S_OK;
}

HRESULT OleIsCurrentClipboard(IDataObject* data)
{
	return data && data == oleClipboard().data ? S_OK : S_FALSE;
}

HRESULT OleGetClipboard(IDataObject** data)
{
	if (!data)
		return E_INVALIDARG;
	*data = dataObjectOf(QApplication::clipboard()->mimeData());
	return S_OK;
}
