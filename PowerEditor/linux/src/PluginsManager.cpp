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

#include "PluginsManager.h"

#include <algorithm>
#include <cstring>
#include <cwchar>

#include <QAction>
#include <QApplication>
#include <QCursor>
#include <QDir>
#include <QDockWidget>
#include <QFile>
#include <QFileInfo>
#include <QLabel>
#include <QLibrary>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPalette>
#include <QPushButton>
#include <QSaveFile>
#include <QScopedValueRollback>
#include <QStandardPaths>
#include <QStatusBar>
#include <QThread>
#include <QTimer>
#include <QToolBar>

#include "EditView.h"
#include "FindReplace.h"
#include "LangStyles.h"
#include "Localization.h"
#include "Macros.h"
#include "MainWindow.h"
#include "ScintillaEditBase.h"
#include "Udl.h"
#include "pugixml.hpp"

#define ILEXER_H_NO_IMPLEMENTATION
#include "ILexer.h"
#include "Lexilla.h"

// The plugin interface of Notepad++, with the <windows.h> of the plugin SDK.
// In a namespace: Notepad_plus_msgs.h has its own LangType.
#include <windows.h>
namespace sdk
{
#include "PluginInterface.h"
#include "Docking.h"
}
#undef MAIN_VIEW
#undef SUB_VIEW

static_assert(sizeof(SCNotification) == sizeof(Scintilla::NotificationData), "SCNotification layout");

namespace
{
	constexpr int kPluginsCmd = 22000;          // ID_PLUGINS_CMD
	constexpr int kPluginsCmdLimit = 22999;     // ID_PLUGINS_CMD_LIMIT
	constexpr int kPluginsCmdDynamic = 23000;   // ID_PLUGINS_CMD_DYNAMIC
	constexpr int kMaxExternalLexerName = 128;  // MAX_EXTERNAL_LEXER_NAME_LEN
	constexpr int kPluginsCmdDynamicLimit = 24999;
	constexpr int kMarkerPlugins = 1;           // MARKER_PLUGINS
	constexpr int kMarkerPluginsLimit = 15;
	constexpr int kIndicatorPlugins = 9;        // INDICATOR_PLUGINS
	constexpr int kIndicatorPluginsLimit = 20;

	QString fromW(const wchar_t* s)
	{
		return s ? QString::fromWCharArray(s) : QString();
	}

	QString fromW(intptr_t s)
	{
		return fromW(reinterpret_cast<const wchar_t*>(s));
	}

	// a path of a plugin: plugins written for Windows build them with '\\' (and "C:")
	QString pathW(const wchar_t* s)
	{
		QString p = fromW(s);
		if (p.isEmpty() || !p.contains(QLatin1Char('\\')) || QFileInfo::exists(p))
			return p;
		p.replace(QLatin1Char('\\'), QLatin1Char('/'));
		if (p.size() >= 2 && p[1] == QLatin1Char(':') && p[0].isLetter())
			p = p.mid(2);
		return p;
	}

	QString pathW(intptr_t s)
	{
		return pathW(reinterpret_cast<const wchar_t*>(s));
	}

	// copies s with its nul character into dst (room for cap wchar_t)
	void toW(const QString& s, wchar_t* dst, size_t cap)
	{
		if (!dst || cap == 0)
			return;
		const std::wstring w = s.toStdWString();
		const size_t n = std::min(w.size(), cap - 1);
		std::wmemcpy(dst, w.data(), n);
		dst[n] = 0;
	}

	size_t wlen(const QString& s)
	{
		return s.toStdWString().size();
	}

	COLORREF colorRef(const QColor& c)
	{
		return static_cast<COLORREF>(c.red() | (c.green() << 8) | (c.blue() << 16));
	}

	const QIcon* pluginIcon(HANDLE handle)
	{
		if (!handle)
			return nullptr;
		unsigned int magic = 0;
		std::memcpy(&magic, handle, sizeof(magic));
		if (magic == NPP_IMAGE_MAGIC)
			return static_cast<const QIcon*>(static_cast<const NppImageHeader*>(handle)->qicon);
		return static_cast<const QIcon*>(handle); // nppIcon() from the Qt SDK
	}

	// ShortcutKey::_key (Windows virtual key) of a Qt key
	int vkOf(int key)
	{
		if (key >= Qt::Key_A && key <= Qt::Key_Z)
			return 'A' + (key - Qt::Key_A);
		if (key >= Qt::Key_0 && key <= Qt::Key_9)
			return '0' + (key - Qt::Key_0);
		if (key >= Qt::Key_F1 && key <= Qt::Key_F24)
			return VK_F1 + (key - Qt::Key_F1);
		switch (key)
		{
			case Qt::Key_Backspace: return VK_BACK;
			case Qt::Key_Tab: return VK_TAB;
			case Qt::Key_Return: case Qt::Key_Enter: return VK_RETURN;
			case Qt::Key_Escape: return VK_ESCAPE;
			case Qt::Key_Space: return VK_SPACE;
			case Qt::Key_PageUp: return VK_PRIOR;
			case Qt::Key_PageDown: return VK_NEXT;
			case Qt::Key_End: return VK_END;
			case Qt::Key_Home: return VK_HOME;
			case Qt::Key_Left: return VK_LEFT;
			case Qt::Key_Up: return VK_UP;
			case Qt::Key_Right: return VK_RIGHT;
			case Qt::Key_Down: return VK_DOWN;
			case Qt::Key_Insert: return VK_INSERT;
			case Qt::Key_Delete: return VK_DELETE;
			case Qt::Key_Semicolon: return VK_OEM_1;
			case Qt::Key_Plus: case Qt::Key_Equal: return VK_OEM_PLUS;
			case Qt::Key_Comma: return VK_OEM_COMMA;
			case Qt::Key_Minus: return VK_OEM_MINUS;
			case Qt::Key_Period: return VK_OEM_PERIOD;
			case Qt::Key_Slash: return VK_OEM_2;
			case Qt::Key_QuoteLeft: return VK_OEM_3;
			case Qt::Key_BracketLeft: return VK_OEM_4;
			case Qt::Key_Backslash: return VK_OEM_5;
			case Qt::Key_BracketRight: return VK_OEM_6;
			case Qt::Key_Apostrophe: return VK_OEM_7;
			default: return 0;
		}
	}

	LRESULT nppSend(NppHandle* h, UINT msg, WPARAM w, LPARAM l)
	{
		return static_cast<PluginsManager*>(h->host)->message(msg, w, l);
	}

	BOOL nppPost(NppHandle* h, UINT msg, WPARAM w, LPARAM l)
	{
		auto* pm = static_cast<PluginsManager*>(h->host);
		QMetaObject::invokeMethod(pm, [pm, msg, w, l] { pm->message(msg, w, l); }, Qt::QueuedConnection);
		return TRUE;
	}

	LRESULT sciSend(NppHandle* h, UINT msg, WPARAM w, LPARAM l)
	{
		auto* sci = static_cast<ScintillaEditBase*>(h->widget);
		return sci ? sci->send(msg, w, l) : 0;
	}

	BOOL sciPost(NppHandle* h, UINT msg, WPARAM w, LPARAM l)
	{
		QPointer<ScintillaEditBase> sci = static_cast<ScintillaEditBase*>(h->widget);
		if (!sci)
			return FALSE;
		QMetaObject::invokeMethod(sci, [sci, msg, w, l] { if (sci) sci->send(msg, w, l); }, Qt::QueuedConnection);
		return TRUE;
	}

	LRESULT menuSend(NppHandle* h, UINT msg, WPARAM w, LPARAM l)
	{
		return h->owner ? SendMessage(h->owner, msg, w, l) : 0;
	}
}

struct PluginsManager::Plugin
{
	QString path;
	QString module; // file name, e.g. "mimeTools.so"
	QString folder; // e.g. "mimeTools"
	QString name;   // getName()
	QLibrary* lib = nullptr; // never unloaded: the widgets of the plugin live until the end
	sdk::PFUNCSETINFO setInfo = nullptr;
	sdk::PFUNCGETNAME getName = nullptr;
	sdk::PFUNCGETFUNCSARRAY getFuncsArray = nullptr;
	sdk::PBENOTIFIED beNotified = nullptr;
	sdk::PMESSAGEPROC messageProc = nullptr;
	sdk::FuncItem* items = nullptr;
	int nbItems = 0;
	QMenu* menu = nullptr;
	QList<QAction*> actions; // one per FuncItem (separators included)
};

PluginsManager::PluginsManager(MainWindow* mw)
	: QObject(mw), _mw(mw), _nextCmdId(kPluginsCmd), _nextDynamicId(kPluginsCmdDynamic),
	  _nextMarker(kMarkerPlugins), _nextIndicator(kIndicatorPlugins),
	  _scnModifiedFlags(SC_MOD_DELETETEXT | SC_MOD_INSERTTEXT | SC_PERFORMED_UNDO | SC_PERFORMED_REDO | SC_MOD_CHANGEINDICATOR)
{
	_nppHandle = createHandle(NPP_HANDLE_NOTEPAD, mw);
	_nppHandle->send = nppSend;
	_nppHandle->post = nppPost;
	for (int v = 0; v < 2; ++v)
		_sciHandles[v] = createHandle(NPP_HANDLE_SCINTILLA, mw->viewEdit(v));
	mw->setProperty("_nppMainHandle", QVariant::fromValue<void*>(_nppHandle));
}

PluginsManager::~PluginsManager() = default;

NppHandle* PluginsManager::createHandle(unsigned int kind, QWidget* widget)
{
	auto h = std::make_unique<NppHandle>();
	h->kind = kind;
	h->widget = widget;
	h->host = this;
	if (kind == NPP_HANDLE_SCINTILLA)
	{
		h->send = sciSend;
		h->post = sciPost;
	}
	h->owner = _nppHandle;
	NppHandle* raw = h.get();
	_handles.push_back(std::move(h));
	return raw;
}

NppHandle* PluginsManager::menuHandle(QMenu* menu)
{
	if (NppHandle* h = _menuHandles.value(menu))
		return h;
	NppHandle* h = createHandle(NPP_HANDLE_MENU, menu);
	h->send = menuSend;
	_menuHandles.insert(menu, h);
	return h;
}

QStringList PluginsManager::pluginsDirs()
{
	QStringList dirs;
	const QString env = qEnvironmentVariable("NPP_PLUGINS_DIR");
	if (!env.isEmpty())
		dirs << env.split(QLatin1Char(':'), Qt::SkipEmptyParts);
	dirs << QDir::cleanPath(QCoreApplication::applicationDirPath() + QStringLiteral("/../lib/linuxpad++/plugins"));
	dirs << userPluginsDir();
	dirs.removeDuplicates();
	return dirs;
}

QString PluginsManager::userPluginsDir()
{
	return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/linuxpad++/plugins");
}

QString PluginsManager::pluginsConfigDir()
{
	const QString dir = Npp::configDir() + QStringLiteral("/plugins/config");
	QDir().mkpath(dir);
	return dir;
}

void PluginsManager::loadPlugins()
{
	// PluginsManager::loadPlugins(): plugins/<Name>/<Name>.so, in the order of the names.
	// $NPP_PLUGINS_ONLY (folder names separated by ':') loads only those (self test).
	const QStringList only = qEnvironmentVariable("NPP_PLUGINS_ONLY").split(QLatin1Char(':'), Qt::SkipEmptyParts);
	QStringList seen;
	for (const QString& dir : pluginsDirs())
	{
		const QFileInfoList folders = QDir(dir).entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::IgnoreCase);
		for (const QFileInfo& folder : folders)
		{
			const QString so = folder.absoluteFilePath() + QLatin1Char('/') + folder.fileName() + QStringLiteral(".so");
			if (!QFileInfo::exists(so) || seen.contains(folder.fileName(), Qt::CaseInsensitive))
				continue;
			if (!only.isEmpty() && !only.contains(folder.fileName(), Qt::CaseInsensitive))
				continue;
			seen << folder.fileName();
			loadPlugin(so);
		}
	}
	std::sort(_plugins.begin(), _plugins.end(), [](const auto& a, const auto& b)
	{
		return a->name.compare(b->name, Qt::CaseInsensitive) < 0;
	});
}

bool PluginsManager::loadPlugin(const QString& path)
{
	auto p = std::make_unique<Plugin>();
	p->path = path;
	p->module = QFileInfo(path).fileName();
	p->folder = QFileInfo(path).completeBaseName();
	auto fail = [&](const QString& why)
	{
		_errors << QStringLiteral("%1: %2").arg(path, why);
		qWarning("LinuxPad++: plugin %s not loaded: %s", qPrintable(path), qPrintable(why));
		return false;
	};

	p->lib = new QLibrary(path, this);
	p->lib->setLoadHints(QLibrary::ResolveAllSymbolsHint);
	if (!p->lib->load())
		return fail(p->lib->errorString());
	auto isUnicode = reinterpret_cast<BOOL (*)()>(p->lib->resolve("isUnicode"));
	p->setInfo = reinterpret_cast<sdk::PFUNCSETINFO>(p->lib->resolve("setInfo"));
	if (!p->setInfo)
		return fail(QStringLiteral("Missing \"setInfo\" function"));
	p->getName = reinterpret_cast<sdk::PFUNCGETNAME>(p->lib->resolve("getName"));
	if (!p->getName)
		return fail(QStringLiteral("Missing \"getName\" function"));
	p->beNotified = reinterpret_cast<sdk::PBENOTIFIED>(p->lib->resolve("beNotified"));
	if (!p->beNotified)
		return fail(QStringLiteral("Missing \"beNotified\" function"));
	p->messageProc = reinterpret_cast<sdk::PMESSAGEPROC>(p->lib->resolve("messageProc"));
	if (!p->messageProc)
		return fail(QStringLiteral("Missing \"messageProc\" function"));
	p->getFuncsArray = reinterpret_cast<sdk::PFUNCGETFUNCSARRAY>(p->lib->resolve("getFuncsArray"));
	if (!p->getFuncsArray)
		return fail(QStringLiteral("Missing \"getFuncsArray\" function"));

	if (auto initialize = reinterpret_cast<BOOL (*)()>(p->lib->resolve("nppLinuxInitialize")); initialize && !initialize())
		return fail(QStringLiteral("Plugin initialization failed."));
	if (!isUnicode || !isUnicode())
		return fail(QStringLiteral("This ANSI plugin is not compatible with your Unicode LinuxPad++."));
	p->name = fromW(p->getName());

	sdk::NppData data;
	data._nppHandle = _nppHandle;
	data._scintillaMainHandle = _sciHandles[0];
	data._scintillaSecondHandle = _sciHandles[1];
	p->setInfo(data);

	p->items = p->getFuncsArray(&p->nbItems);
	if (!p->items || p->nbItems <= 0)
		return fail(QStringLiteral("Missing \"FuncItems\" array, or the nb of Function Item is not set correctly"));
	if (_nextCmdId + p->nbItems > kPluginsCmdLimit)
		return fail(QStringLiteral("Too many plugin commands"));

	// a lexer plugin (Lexilla protocol): its lexers become the languages L_EXTERNAL + n, styled by
	// plugins/Config/<module>.xml (here ~/.config/linuxpad++/plugins/config/<module>.xml, else the
	// folder of the plugin or its Config folder)
	using GetLexerCountFn = int (*)();
	using GetLexerNameFn = void (*)(unsigned int index, char* name, int length);
	if (auto getLexerCount = reinterpret_cast<GetLexerCountFn>(p->lib->resolve("GetLexerCount")))
	{
		auto getLexerName = reinterpret_cast<GetLexerNameFn>(p->lib->resolve("GetLexerName"));
		if (!getLexerName)
			return fail(QStringLiteral("Loading GetLexerName function failed."));
		auto createLexer = reinterpret_cast<ExternalLexer::CreateLexerFn>(p->lib->resolve("CreateLexer"));
		if (!createLexer)
			return fail(QStringLiteral("Loading CreateLexer function failed."));
		const QString xmlName = p->folder + QStringLiteral(".xml");
		const QString folder = QFileInfo(path).absolutePath();
		QString xmlPath;
		for (const QString& candidate : {pluginsConfigDir() + QLatin1Char('/') + xmlName, folder + QLatin1Char('/') + xmlName,
				folder + QStringLiteral("/Config/") + xmlName})
		{
			if (QFileInfo::exists(candidate))
			{
				xmlPath = candidate;
				break;
			}
		}
		if (xmlPath.isEmpty())
			return fail(QStringLiteral("plugins/Config/%1 is missing.").arg(xmlName));
		StyleManager& sm = StyleManager::instance();
		const int count = std::min(getLexerCount(), StyleManager::maxExternalLexers);
		for (int i = 0; i < count; ++i)
		{
			char lexerName[kMaxExternalLexerName] = {};
			getLexerName(static_cast<unsigned int>(i), lexerName, kMaxExternalLexerName);
			sm.addExternalLexer(QString::fromUtf8(lexerName), createLexer);
		}
		if (!sm.loadExternalLexerConfig(xmlPath))
			_errors << QStringLiteral("%1: %2 failed to load.").arg(path, xmlPath);
	}
	for (int i = 0; i < p->nbItems; ++i)
	{
		p->items[i]._cmdID = _nextCmdId++;
		_commands.insert(p->items[i]._cmdID, {p.get(), i});
	}
	_plugins.push_back(std::move(p));
	return true;
}

void PluginsManager::addMenus()
{
	// PluginsManager::initMenu() / setMenu(): a submenu per plugin, on top of the Plugins menu
	QAction* openDir = _mw->_actions.value(IDM_SETTING_OPENPLUGINSDIR);
	QMenu* pluginsMenu = nullptr;
	if (openDir)
	{
		for (QObject* o : openDir->associatedObjects())
		{
			if (auto* m = qobject_cast<QMenu*>(o))
				pluginsMenu = m;
		}
	}
	if (!pluginsMenu)
		return;
	QAction* before = pluginsMenu->actions().value(0);
	for (auto& p : _plugins)
	{
		p->menu = new QMenu(p->name, pluginsMenu);
		pluginsMenu->insertMenu(before, p->menu);
		for (int i = 0; i < p->nbItems; ++i)
		{
			sdk::FuncItem& item = p->items[i];
			if (!item._pFunc)
			{
				p->actions << p->menu->addSeparator();
				continue;
			}
			auto* a = new QAction(fromW(item._itemName), _mw);
			a->setData(item._cmdID);
			a->setProperty("nppChecked", item._init2Check);
			if (item._init2Check)
			{
				a->setCheckable(true);
				a->setChecked(true);
			}
			if (item._pShKey && item._pShKey->_key)
			{
				ShortcutSpec spec;
				spec.ctrl = item._pShKey->_isCtrl;
				spec.alt = item._pShKey->_isAlt;
				spec.shift = item._pShKey->_isShift;
				spec.key = item._pShKey->_key;
				a->setShortcut(spec.keySequence());
			}
			const int id = item._cmdID;
			connect(a, &QAction::triggered, this, [this, a, id]
			{
				// a checked item changes only when the plugin says so (NPPM_SETMENUITEMCHECK)
				if (a->isCheckable())
					a->setChecked(a->property("nppChecked").toBool());
				_mw->command(id);
			});
			_mw->_handlers.insert(id, [this, id] { runCommand(id); });
			_mw->addAction(a); // shortcut with a hidden menu bar
			p->menu->addAction(a);
			p->actions << a;
		}
	}
	if (!_plugins.empty())
		pluginsMenu->insertSeparator(before);
}

void PluginsManager::notifyToolbar()
{
	notify(NPPN_TBMODIFICATION, 0);
}

void PluginsManager::notifyReady()
{
	_ready = true;
	notify(NPPN_READY, 0);
}

void PluginsManager::notify(unsigned int code, uptr_t idFrom, void* hwndFrom)
{
	if (_plugins.empty() || _destroying)
		return;
	SCNotification scn{};
	scn.nmhdr.code = code;
	scn.nmhdr.hwndFrom = hwndFrom ? hwndFrom : _nppHandle;
	scn.nmhdr.idFrom = idFrom;
	for (auto& p : _plugins)
		p->beNotified(&scn);
}

void PluginsManager::notifyBuffer(unsigned int code, const Buffer* buf)
{
	notify(code, reinterpret_cast<uptr_t>(buf));
}

void PluginsManager::notifyBeforeSave(Buffer* buf, const QString& target)
{
	const QScopedValueRollback bufferScope(_savingBuffer, buf);
	const QScopedValueRollback targetScope(_saveTarget, target);
	notifyBuffer(NPPN_FILEBEFORESAVE, buf);
}

void PluginsManager::relayScintilla(const Scintilla::NotificationData* data, int view)
{
	if (_plugins.empty() || _destroying)
		return;
	SCNotification scn;
	std::memcpy(&scn, data, sizeof(scn));
	if (scn.nmhdr.code == SCN_MODIFIED && !(static_cast<unsigned int>(scn.modificationType) & _scnModifiedFlags))
		return;
	scn.nmhdr.hwndFrom = _sciHandles[view];
	scn.nmhdr.idFrom = 0;
	for (auto& p : _plugins)
		p->beNotified(&scn);
}

void PluginsManager::runCommand(int id)
{
	if (id >= kPluginsCmdDynamic && id <= kPluginsCmdDynamicLimit)
	{
		// menu items made by plugins with NPPM_ALLOCATECMDID: WM_COMMAND to every plugin
		for (auto& p : _plugins)
			p->messageProc(WM_COMMAND, static_cast<WPARAM>(id), 0);
		return;
	}
	auto it = _commands.constFind(id);
	if (it == _commands.constEnd())
		return;
	Plugin* p = it->first;
	const int index = it->second;
	if (p->items[index]._pFunc)
		p->items[index]._pFunc();
}

QStringList PluginsManager::loadedModules() const
{
	QStringList list;
	for (const auto& p : _plugins)
		list << p->module;
	return list;
}

QString PluginsManager::commandModule(int id) const
{
	auto it = _commands.constFind(id);
	return it == _commands.constEnd() ? QString() : it->first->module;
}

int PluginsManager::commandIndex(int id) const
{
	auto it = _commands.constFind(id);
	return it == _commands.constEnd() ? -1 : it->second;
}

QList<int> PluginsManager::commandIds() const
{
	QList<int> ids;
	for (const auto& p : _plugins)
	{
		for (int i = 0; i < p->nbItems; ++i)
		{
			if (p->items[i]._pFunc)
				ids << p->items[i]._cmdID;
		}
	}
	return ids;
}

QAction* PluginsManager::commandAction(int id) const
{
	auto it = _commands.constFind(id);
	if (it == _commands.constEnd())
		return nullptr;
	const Plugin* p = it->first;
	QAction* a = p->actions.value(it->second);
	return (a && !a->isSeparator()) ? a : nullptr;
}

PluginsManager::Plugin* PluginsManager::pluginOfModule(const QString& module) const
{
	// "NppExec.dll" of a Windows plugin is "NppExec.so" here
	const QString base = QFileInfo(module).completeBaseName();
	for (const auto& p : _plugins)
	{
		if (p->module.compare(module, Qt::CaseInsensitive) == 0 || p->folder.compare(base, Qt::CaseInsensitive) == 0)
			return p.get();
	}
	return nullptr;
}

bool PluginsManager::importPlugins(const QStringList& files, QString* error)
{
	// Notepad_plus::importPlugins(): each one to its own folder of the plugins directory
	for (const QString& file : files)
	{
		const QFileInfo fi(file);
		if (fi.suffix() != QStringLiteral("so") || fi.completeBaseName().isEmpty())
		{
			if (error)
				*error = QStringLiteral("\"%1\" is not a plugin (a .so file).").arg(file);
			return false;
		}
		const QString destDir = userPluginsDir() + QLatin1Char('/') + fi.completeBaseName();
		const QString dest = destDir + QLatin1Char('/') + fi.fileName();
		if (fi.isFile() && fi.canonicalFilePath() == QFileInfo(dest).canonicalFilePath())
			continue;
		auto failed = [&]
		{
			if (error)
				*error = QStringLiteral("Cannot copy \"%1\" to \"%2\".").arg(file, dest);
			return false;
		};
		QFile input(file);
		QSaveFile output(dest);
		if (!fi.isFile() || !input.open(QIODevice::ReadOnly) || !QDir().mkpath(destDir) || !output.open(QIODevice::WriteOnly))
			return failed();

		QByteArray buffer(64 * 1024, Qt::Uninitialized);
		for (;;)
		{
			const qint64 bytes = input.read(buffer.data(), buffer.size());
			if (bytes == 0)
				break;
			if (bytes < 0 || output.write(buffer.constData(), bytes) != bytes)
				return failed();
		}
		if (!output.commit())
			return failed();
	}
	return true;
}

QWidget* PluginsManager::widgetOf(intptr_t handle) const
{
	auto* h = reinterpret_cast<NppHandle*>(handle);
	if (!h || h->magic != NPP_HANDLE_MAGIC)
		return nullptr;
	return static_cast<QWidget*>(h->widget);
}

PluginsManager::Dock* PluginsManager::dockOfClient(QWidget* client)
{
	for (Dock& d : _docks)
	{
		if (client && d.client == client)
			return &d;
	}
	return nullptr;
}

intptr_t PluginsManager::registerDock(const void* raw)
{
	// DockingManager::createDockableDlg(): the client widget in a dock of its default container
	const auto* data = static_cast<const sdk::tTbData*>(raw);
	if (!data)
		return FALSE;
	QWidget* client = widgetOf(reinterpret_cast<intptr_t>(data->hClient));
	if (!client)
		return FALSE;
	if (dockOfClient(client))
		return TRUE;
	QString title = fromW(data->pszName);
	if ((data->uMask & DWS_ADDINFO) && data->pszAddInfo && *data->pszAddInfo)
		title += QStringLiteral(" - ") + fromW(data->pszAddInfo);
	auto* dock = new QDockWidget(title, _mw);
	const QString module = fromW(data->pszModuleName);
	dock->setObjectName(QStringLiteral("plugin:%1:%2").arg(QFileInfo(module).completeBaseName()).arg(data->dlgID));
	if (data->uMask & DWS_ICONTAB)
	{
		if (const QIcon* icon = pluginIcon(data->hIconTab))
			dock->setWindowIcon(*icon);
	}
	dock->setWidget(client);
	reinterpret_cast<NppHandle*>(data->hClient)->owner = _nppHandle;
	Qt::DockWidgetArea area = Qt::LeftDockWidgetArea;
	switch ((data->uMask >> 28) & 0x3)
	{
		case CONT_RIGHT: area = Qt::RightDockWidgetArea; break;
		case CONT_TOP: area = Qt::TopDockWidgetArea; break;
		case CONT_BOTTOM: area = Qt::BottomDockWidgetArea; break;
		default: break;
	}
	if (area == Qt::RightDockWidgetArea)
		_mw->addRightDock(dock);
	else
		_mw->addDockWidget(area, dock);
	if (data->uMask & DWS_DF_FLOATING)
		dock->setFloating(true);
	_mw->restoreDockWidget(dock);
	dock->hide();
	Dock d;
	d.dock = dock;
	d.client = client;
	d.name = fromW(data->pszName);
	d.module = module;
	d.dlgID = data->dlgID;
	_docks.push_back(d);
	return TRUE;
}

void PluginsManager::setChecked(int cmdId, bool checked)
{
	QAction* a = commandAction(cmdId);
	if (!a)
		return;
	a->setProperty("nppChecked", checked);
	if (!a->isCheckable())
		a->setCheckable(true);
	a->setChecked(checked);
}

intptr_t PluginsManager::copyString(const QString& s, uintptr_t bufLen, intptr_t buf, bool lengthIfNull) const
{
	// the "call twice" convention: buffer NULL gives the length, then the copy
	const size_t len = wlen(s);
	if (!buf)
		return lengthIfNull ? static_cast<intptr_t>(len) : 0;
	if (bufLen < len + 1)
		return 0;
	toW(s, reinterpret_cast<wchar_t*>(buf), bufLen);
	return static_cast<intptr_t>(len);
}

Buffer* PluginsManager::bufferOf(uintptr_t id) const
{
	if (!id)
		return nullptr;
	for (Buffer* b : _mw->allBuffers())
	{
		if (reinterpret_cast<uintptr_t>(b) == id)
			return b;
	}
	return nullptr;
}

intptr_t PluginsManager::message(unsigned int msg, uintptr_t wParam, intptr_t lParam)
{
	// windows of plugins hidden while the main window is destroyed: nothing to answer with
	if (_destroying)
		return 0;
	if (msg == NPPM_LINUX_SUBCLASSNPP)
	{
		// a plugin subclasses the main window (SetWindowLongPtr / SetWindowSubclass)
		const auto* hook = reinterpret_cast<const NppWndHook*>(wParam);
		if (!hook || !hook->dispatch)
			return FALSE;
		auto it = std::find_if(_wndHooks.begin(), _wndHooks.end(), [hook](const WndHook& h) { return h.context == hook->context; });
		if (lParam && it == _wndHooks.end())
			_wndHooks.push_back(WndHook{hook->dispatch, hook->context});
		else if (!lParam && it != _wndHooks.end())
			_wndHooks.erase(it);
		return TRUE;
	}
	if (msg == NPPM_LINUX_CALLNEXT)
	{
		// CallWindowProc of the original procedure in a subclass: the next subclass, then Notepad++
		const auto* next = reinterpret_cast<const NppWndMessage*>(wParam);
		if (!next)
			return 0;
		if (_wndHookLevel == 0) // not called from a subclass procedure: Notepad++ itself
			return handleMessage(next->msg, next->wParam, next->lParam);
		return dispatchHooks(next->msg, next->wParam, next->lParam);
	}
	// any other message (also one that a subclass procedure sends while it handles another one)
	// starts at the newest subclass, like SendMessage on Windows
	const size_t saved = _wndHookLevel;
	_wndHookLevel = 0;
	const intptr_t r = dispatchHooks(msg, wParam, lParam);
	_wndHookLevel = saved;
	return r;
}

// SendMessageW of the plugin SDK (windows.h), which this file uses too: a message sent from
// another thread is handled on the GUI thread (weak: the npp_win32 library has the same ones)
__attribute__((weak)) bool npp_isGuiThread()
{
	return !qApp || QThread::currentThread() == qApp->thread();
}

__attribute__((weak)) LRESULT npp_sendFromThread(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	LRESULT r = 0;
	QMetaObject::invokeMethod(qApp, [&] { r = hWnd->send(hWnd, msg, wParam, lParam); }, Qt::BlockingQueuedConnection);
	return r;
}

int PluginsManager::langTypeOf(const Buffer* b)
{
	// the lexers of the lexer plugins are L_EXTERNAL + their index for the plugins, like Notepad++
	if (b->lang == L_EXTERNAL)
	{
		const int index = StyleManager::instance().externalLexerIndex(b->udlName);
		return L_EXTERNAL + std::max(index, 0);
	}
	return static_cast<int>(b->lang);
}

QString PluginsManager::externalLexerName(intptr_t langType)
{
	const auto& lexers = StyleManager::instance().externalLexers();
	const intptr_t index = langType - L_EXTERNAL;
	if (index < 0 || index >= static_cast<intptr_t>(lexers.size()))
		return QString();
	return lexers[static_cast<size_t>(index)].name;
}

intptr_t PluginsManager::dispatchHooks(unsigned int msg, uintptr_t wParam, intptr_t lParam)
{
	// the procedures of the subclasses, newest first, then Notepad++
	if (_wndHookLevel < _wndHooks.size())
	{
		const WndHook hook = _wndHooks[_wndHooks.size() - 1 - _wndHookLevel];
		++_wndHookLevel;
		const intptr_t r = hook.dispatch(hook.context, msg, wParam, lParam);
		--_wndHookLevel;
		return r;
	}
	return handleMessage(msg, wParam, lParam);
}

void PluginsManager::editorContextMenu(int view, const QPoint& globalPos)
{
	NppHandle* sci = scintillaHandle(view);
	message(WM_CONTEXTMENU, reinterpret_cast<uintptr_t>(sci),
		static_cast<intptr_t>(MAKELPARAM(static_cast<short>(globalPos.x()), static_cast<short>(globalPos.y()))));
}

void PluginsManager::initMenuPopup(QMenu* menu)
{
	if (_wndHooks.empty() || !menu)
		return;
	// one handle for the temporary menus: it points to the menu while it opens, then to nothing
	if (!_popupMenuHandle)
	{
		_popupMenuHandle = createHandle(NPP_HANDLE_MENU, nullptr);
		_popupMenuHandle->send = menuSend;
	}
	_popupMenuHandle->widget = menu;
	QObject::connect(menu, &QObject::destroyed, this, [this, menu]
	{
		if (_popupMenuHandle && _popupMenuHandle->widget == menu)
			_popupMenuHandle->widget = nullptr;
	});
	message(WM_INITMENUPOPUP, reinterpret_cast<uintptr_t>(_popupMenuHandle), 0); // position 0, not the window menu
}

bool PluginsManager::routeCommand(int id)
{
	if (_wndHooks.empty() || _wndHookLevel > 0 || _routing)
		return false;
	_routing = true;
	message(WM_COMMAND, static_cast<uintptr_t>(id), 0);
	_routing = false;
	return true;
}

intptr_t PluginsManager::handleMessage(unsigned int msg, uintptr_t wParam, intptr_t lParam)
{
	// NppBigSwitch.cpp, the NPPM_ messages
	MainWindow* mw = _mw;
	switch (msg)
	{
		case WM_COMMAND:
		case NPPM_MENUCOMMAND:
		{
			const int id = (msg == WM_COMMAND) ? LOWORD(wParam) : static_cast<int>(lParam);
			if (id >= kPluginsCmdDynamic && id <= kPluginsCmdDynamicLimit)
				runCommand(id);
			else
			{
				// the subclasses have had it already (message())
				const bool routing = _routing;
				_routing = true;
				mw->command(id);
				_routing = routing;
			}
			return TRUE;
		}

		case NPPM_GETCURRENTSCINTILLA:
			if (lParam)
				*reinterpret_cast<int*>(lParam) = mw->activeView();
			return TRUE;

		case NPPM_GETCURRENTLANGTYPE:
			if (lParam)
				*reinterpret_cast<int*>(lParam) = mw->currentBuffer() ? langTypeOf(mw->currentBuffer()) : L_TEXT;
			return TRUE;

		case NPPM_SETCURRENTLANGTYPE:
			if (const QString external = externalLexerName(lParam); !external.isEmpty())
				mw->languageSet(L_EXTERNAL, external); // L_EXTERNAL + n: the lexer n of the lexer plugins
			else if (lParam >= 0 && lParam < StyleManager::langCount() && lParam != L_USER && lParam != L_EXTERNAL)
				mw->languageSet(static_cast<LangType>(lParam));
			return TRUE;

		case NPPM_GETNBOPENFILES:
		{
			const int main = mw->viewTabs(0)->count();
			const int sub = mw->viewVisible(1) ? mw->viewTabs(1)->count() : 0;
			if (lParam == PRIMARY_VIEW)
				return main;
			if (lParam == SECOND_VIEW)
				return sub;
			return main + sub;
		}

		case NPPM_GETOPENFILENAMES_DEPRECATED:
		case NPPM_GETOPENFILENAMESPRIMARY_DEPRECATED:
		case NPPM_GETOPENFILENAMESSECOND_DEPRECATED:
		{
			auto** names = reinterpret_cast<wchar_t**>(wParam);
			if (!names)
				return 0;
			std::vector<Buffer*> list;
			if (msg != NPPM_GETOPENFILENAMESSECOND_DEPRECATED)
				list = mw->viewBuffers(0);
			if (msg != NPPM_GETOPENFILENAMESPRIMARY_DEPRECATED)
			{
				const std::vector<Buffer*> sub = mw->viewBuffers(1);
				list.insert(list.end(), sub.begin(), sub.end());
			}
			int n = 0;
			for (Buffer* b : list)
			{
				if (n >= lParam)
					break;
				toW(b->fullName(), names[n++], MAX_PATH);
			}
			return n;
		}

		case NPPM_MODELESSDIALOG:
		{
			// IsDialogMessage of Windows: Qt gives the keys to the dialogs already
			auto* h = reinterpret_cast<NppHandle*>(lParam);
			if (h && h->magic == NPP_HANDLE_MAGIC && !h->owner)
				h->owner = _nppHandle;
			return lParam;
		}

		case NPPM_GETNBSESSIONFILES:
		case NPPM_GETSESSIONFILES:
		{
			const QString path = pathW(lParam);
			pugi::xml_document doc;
			const bool ok = !path.isEmpty() && doc.load_file(QFile::encodeName(path).constData());
			if (msg == NPPM_GETNBSESSIONFILES && wParam)
				*reinterpret_cast<BOOL*>(wParam) = ok ? TRUE : FALSE;
			if (!ok)
				return msg == NPPM_GETNBSESSIONFILES ? 0 : FALSE;
			QStringList files;
			pugi::xml_node session = doc.child("NotepadPlus").child("Session");
			for (const char* view : {"mainView", "subView"})
			{
				for (pugi::xml_node f : session.child(view).children("File"))
					files << QString::fromUtf8(f.attribute("filename").as_string());
			}
			if (msg == NPPM_GETNBSESSIONFILES)
				return files.size();
			auto** out = reinterpret_cast<wchar_t**>(wParam);
			if (!out)
				return FALSE;
			for (int i = 0; i < files.size(); ++i)
				toW(files[i], out[i], MAX_PATH);
			return TRUE;
		}

		case NPPM_SAVESESSION:
		{
			const auto* si = reinterpret_cast<const sdk::sessionInfo*>(lParam);
			if (!si || !si->sessionFilePathName)
				return 0;
			pugi::xml_document doc;
			pugi::xml_node session = doc.append_child("NotepadPlus").append_child("Session");
			session.append_attribute("activeView") = 0;
			pugi::xml_node mainView = session.append_child("mainView");
			mainView.append_attribute("activeIndex") = 0;
			for (int i = 0; i < si->nbFile; ++i)
				mainView.append_child("File").append_attribute("filename") = pathW(si->files[i]).toUtf8().constData();
			session.append_child("subView").append_attribute("activeIndex") = 0;
			if (!doc.save_file(QFile::encodeName(pathW(si->sessionFilePathName)).constData()))
				return 0;
			return reinterpret_cast<intptr_t>(si->sessionFilePathName);
		}

		case NPPM_SAVECURRENTSESSION:
		{
			const QString path = pathW(lParam);
			if (path.isEmpty())
				return 0;
			mw->writeSession(path, false);
			return QFileInfo::exists(path) ? lParam : 0;
		}

		case NPPM_LOADSESSION:
			return mw->readSession(pathW(lParam)) ? TRUE : FALSE;

		case NPPM_CREATESCINTILLAHANDLE:
		{
			QWidget* parent = widgetOf(lParam);
			auto* sci = new ScintillaEditBase(parent ? parent : mw);
			if (!parent)
				sci->hide();
			NppHandle* h = createHandle(NPP_HANDLE_SCINTILLA, sci);
			connect(sci, &QObject::destroyed, this, [h] { h->widget = nullptr; });
			return reinterpret_cast<intptr_t>(h);
		}

		case NPPM_DESTROYSCINTILLAHANDLE_DEPRECATED:
			return TRUE;

		case NPPM_GETNBUSERLANG:
			if (lParam)
				*reinterpret_cast<int*>(lParam) = IDM_LANG_USER + 1;
			return static_cast<intptr_t>(UdlManager::instance().langs().size());

		case NPPM_GETCURRENTDOCINDEX:
		{
			const int view = static_cast<int>(lParam);
			if (view < 0 || view > 1 || !mw->viewVisible(view))
				return -1;
			return mw->indexOf(view, mw->viewCurrent(view));
		}

		case NPPM_SETSTATUSBAR:
		{
			QLabel* parts[] = {mw->_sbLang, mw->_sbDocInfo, mw->_sbPos, mw->_sbEol, mw->_sbEnc, mw->_sbIns};
			if (wParam > 5 || !parts[wParam])
				return FALSE;
			parts[wParam]->setText(fromW(lParam));
			return TRUE;
		}

		case NPPM_GETMENUHANDLE:
		{
			if (wParam == NPPMAINMENU)
			{
				NppHandle* h = _menuHandles.value(nullptr);
				if (!h)
				{
					h = createHandle(NPP_HANDLE_MENU, mw->menuBar());
					h->send = menuSend;
					_menuHandles.insert(nullptr, h);
				}
				return reinterpret_cast<intptr_t>(h);
			}
			QAction* openDir = mw->_actions.value(IDM_SETTING_OPENPLUGINSDIR);
			for (QObject* o : openDir ? openDir->associatedObjects() : QObjectList())
			{
				if (auto* m = qobject_cast<QMenu*>(o))
					return reinterpret_cast<intptr_t>(menuHandle(m));
			}
			return 0;
		}

		case NPPM_ENCODESCI:
		case NPPM_DECODESCI:
		{
			const int view = static_cast<int>(wParam);
			Buffer* b = (view == 0 || view == 1) ? mw->viewCurrent(view) : nullptr;
			if (!b)
				return -1;
			const int old = static_cast<int>(b->unicode);
			if (view != mw->activeView())
				mw->setActiveView(view);
			mw->encodingConvert(msg == NPPM_ENCODESCI ? IDM_FORMAT_CONV2_UTF_8 : IDM_FORMAT_CONV2_ANSI);
			return msg == NPPM_ENCODESCI ? static_cast<int>(b->unicode) : old;
		}

		case NPPM_ACTIVATEDOC:
		case NPPM_TRIGGERTABBARCONTEXTMENU:
		{
			const int view = static_cast<int>(wParam);
			if (view != 0 && view != 1)
				return FALSE;
			Buffer* b = mw->bufferAt(view, static_cast<int>(lParam));
			if (!b)
				return FALSE;
			mw->activateBuffer(b, view);
			if (msg == NPPM_TRIGGERTABBARCONTEXTMENU)
				mw->showTabContextMenu(QCursor::pos(), static_cast<int>(lParam));
			return TRUE;
		}

		case NPPM_LAUNCHFINDINFILESDLG:
		{
			mw->showFindDialog(FindReplaceDlg::FindInFilesTab);
			if (mw->_findDlg)
			{
				if (wParam)
					mw->_findDlg->setDirectory(pathW(static_cast<intptr_t>(wParam)));
				if (lParam)
					mw->_findDlg->setFilters(fromW(lParam));
			}
			return TRUE;
		}

		case WM_CONTEXTMENU:
		{
			// Notepad++ (NppBigSwitch.cpp): the context menu of the editing view the message comes from
			for (int view = 0; view <= 1; ++view) // MAIN_VIEW, SUB_VIEW
			{
				if (reinterpret_cast<uintptr_t>(scintillaHandle(view)) == wParam)
				{
					mw->showEditorContextMenu(QPoint(static_cast<short>(LOWORD(lParam)), static_cast<short>(HIWORD(lParam))));
					return TRUE;
				}
			}
			return FALSE;
		}

		case NPPM_DMMREGASDCKDLG:
			return registerDock(reinterpret_cast<const void*>(lParam));

		case NPPM_DMMSHOW:
		case NPPM_DMMHIDE:
		case NPPM_DMMUPDATEDISPINFO:
		{
			Dock* d = dockOfClient(widgetOf(lParam));
			if (!d || !d->dock)
				return FALSE;
			if (msg == NPPM_DMMSHOW)
			{
				d->dock->show();
				d->dock->raise();
			}
			else if (msg == NPPM_DMMHIDE)
			{
				d->dock->hide();
			}
			else
			{
				d->dock->update();
			}
			return TRUE;
		}

		case NPPM_DMMVIEWOTHERTAB:
		{
			const QString name = fromW(lParam);
			for (Dock& d : _docks)
			{
				if (d.dock && d.name == name)
				{
					d.dock->show();
					d.dock->raise();
					return TRUE;
				}
			}
			return FALSE;
		}

		case NPPM_DMMGETPLUGINHWNDBYNAME:
		{
			if (!lParam)
				return 0;
			const QString window = fromW(static_cast<intptr_t>(wParam));
			const QString module = QFileInfo(fromW(lParam)).completeBaseName();
			for (Dock& d : _docks)
			{
				if (d.client && QFileInfo(d.module).completeBaseName().compare(module, Qt::CaseInsensitive) == 0
					&& (window.isEmpty() || d.name == window))
				{
					const QVariant v = d.client->property("_nppHandle");
					return v.isValid() ? reinterpret_cast<intptr_t>(v.value<void*>()) : 0;
				}
			}
			return 0;
		}

		case NPPM_RELOADFILE:
		{
			Buffer* b = mw->findBuffer(pathW(lParam));
			if (!b)
				return FALSE;
			mw->reloadBuffer(b);
			return TRUE;
		}

		case NPPM_SWITCHTOFILE:
		{
			Buffer* b = mw->findBuffer(pathW(lParam));
			if (!b)
				return FALSE;
			mw->activateBuffer(b);
			return TRUE;
		}

		case NPPM_SAVECURRENTFILE:
			return (mw->currentBuffer() && mw->currentBuffer()->dirty) ? (mw->saveBuffer(mw->currentBuffer()) ? TRUE : FALSE) : FALSE;

		case NPPM_SAVEALLFILES:
		{
			bool any = false;
			for (Buffer* b : mw->allBuffers())
				any = any || b->dirty;
			mw->fileSaveAll();
			return any ? TRUE : FALSE;
		}

		case NPPM_SAVEFILE:
		{
			Buffer* b = mw->findBuffer(pathW(lParam));
			if (!b)
				return FALSE;
			return mw->saveBuffer(b) ? TRUE : FALSE;
		}

		case NPPM_SAVECURRENTFILEAS:
		{
			Buffer* b = mw->currentBuffer();
			const QString path = pathW(lParam);
			if (!b || path.isEmpty())
				return FALSE;
			return mw->saveBuffer(b, false, wParam != 0, path) ? TRUE : FALSE;
		}

		case NPPM_SETMENUITEMCHECK:
			setChecked(static_cast<int>(wParam), lParam != 0);
			if (QAction* a = mw->_actions.value(static_cast<int>(wParam)))
			{
				a->setCheckable(true);
				a->setChecked(lParam != 0);
			}
			return TRUE;

		case NPPM_LINUX_ISMENUITEMCHECKED:
		{
			if (QAction* a = commandAction(static_cast<int>(wParam)))
				return a->property("nppChecked").toBool() ? TRUE : FALSE;
			QAction* a = mw->_actions.value(static_cast<int>(wParam));
			return (a && a->isChecked()) ? TRUE : FALSE;
		}

		case NPPM_LINUX_ENABLEMENUITEM:
		{
			QAction* a = commandAction(static_cast<int>(wParam));
			if (!a)
				a = mw->_actions.value(static_cast<int>(wParam));
			if (a)
				a->setEnabled(lParam != 0);
			return TRUE;
		}

		case NPPM_LINUX_SETMENUITEMTEXT:
		{
			if (QAction* a = commandAction(static_cast<int>(wParam)))
				a->setText(fromW(lParam));
			return TRUE;
		}

		case NPPM_LINUX_MESSAGEBOX:
		{
			const auto* args = reinterpret_cast<const NppMessageBoxArgs*>(wParam);
			if (!args)
				return IDCANCEL;
			QWidget* parent = widgetOf(reinterpret_cast<intptr_t>(args->parent));
			QMessageBox box(parent ? parent->window() : mw);
			box.setWindowTitle(fromW(args->caption));
			box.setText(fromW(args->text));
			switch (args->type & 0xF0)
			{
				case MB_ICONHAND: box.setIcon(QMessageBox::Critical); break;
				case MB_ICONQUESTION: box.setIcon(QMessageBox::Question); break;
				case MB_ICONEXCLAMATION: box.setIcon(QMessageBox::Warning); break;
				case MB_ICONASTERISK: box.setIcon(QMessageBox::Information); break;
				default: break;
			}
			QList<std::pair<QMessageBox::StandardButton, int>> buttons;
			switch (args->type & 0x0F)
			{
				case MB_OKCANCEL: buttons = {{QMessageBox::Ok, IDOK}, {QMessageBox::Cancel, IDCANCEL}}; break;
				case MB_ABORTRETRYIGNORE: buttons = {{QMessageBox::Abort, IDABORT}, {QMessageBox::Retry, IDRETRY}, {QMessageBox::Ignore, IDIGNORE}}; break;
				case MB_YESNOCANCEL: buttons = {{QMessageBox::Yes, IDYES}, {QMessageBox::No, IDNO}, {QMessageBox::Cancel, IDCANCEL}}; break;
				case MB_YESNO: buttons = {{QMessageBox::Yes, IDYES}, {QMessageBox::No, IDNO}}; break;
				case MB_RETRYCANCEL: buttons = {{QMessageBox::Retry, IDRETRY}, {QMessageBox::Cancel, IDCANCEL}}; break;
				default: buttons = {{QMessageBox::Ok, IDOK}}; break;
			}
			QMessageBox::StandardButtons all;
			for (const auto& b : buttons)
				all |= b.first;
			box.setStandardButtons(all);
			const int defIndex = (args->type & MB_DEFBUTTON2) ? 1 : 0;
			if (defIndex < buttons.size())
				box.setDefaultButton(buttons[defIndex].first);
			const auto clicked = static_cast<QMessageBox::StandardButton>(box.exec());
			for (const auto& b : buttons)
			{
				if (b.first == clicked)
					return b.second;
			}
			return IDCANCEL;
		}

		case NPPM_LINUX_SETSCINTILLANOTIFY:
		{
			auto* h = reinterpret_cast<NppHandle*>(wParam);
			const auto* cb = reinterpret_cast<const NppScintillaNotify*>(lParam);
			auto* sci = (h && h->magic == NPP_HANDLE_MAGIC && h->kind == NPP_HANDLE_SCINTILLA) ? static_cast<ScintillaEditBase*>(h->widget) : nullptr;
			if (!sci || !cb || !cb->callback)
				return FALSE;
			const NppScintillaNotify target = *cb;
			connect(sci, &ScintillaEditBase::notify, this, [target, h](Scintilla::NotificationData* data)
			{
				SCNotification scn;
				std::memcpy(&scn, data, sizeof(scn));
				scn.nmhdr.hwndFrom = h;
				target.callback(target.context, &scn);
			});
			return TRUE;
		}

		case NPPM_LINUX_PLUGINWIDGETPARENT:
			return reinterpret_cast<intptr_t>(static_cast<QWidget*>(mw));

		case NPPM_LINUX_FILTERBEFORESAVE:
		{
			Buffer* b = bufferOf(wParam);
			const auto filter = reinterpret_cast<NppSaveFilter>(lParam);
			if (!filter || !b || b != _savingBuffer || _saveTarget.isEmpty() || b->isReadOnly())
				return FALSE;
			const std::wstring target = _saveTarget.toStdWString();
			NppSaveFormat format{b->eol, static_cast<int>(b->unicode), b->codepage};
			// The callback can modify the saved document without activating a tab.
			// A filter may still dispatch ordinary Scintilla notifications while editing.
			const QScopedValueRollback bufferScope(_savingBuffer, static_cast<Buffer*>(nullptr));
			mw->withDocument(b, [&](EditView* editor)
			{
				NppHandle editorHandle;
				editorHandle.kind = NPP_HANDLE_SCINTILLA;
				editorHandle.widget = editor;
				editorHandle.owner = _nppHandle;
				editorHandle.send = sciSend;
				filter(&editorHandle, target.c_str(), &format);
				b->dirty = b->forcedDirty || editor->ex(SCI_GETMODIFY);
			});
			if (format.eol >= SC_EOL_CRLF && format.eol <= SC_EOL_LF)
			{
				b->eol = format.eol;
				for (int view : {0, 1})
				{
					if (mw->viewCurrent(view) == b)
						mw->viewEdit(view)->ex(SCI_SETEOLMODE, format.eol);
				}
			}
			if (format.encoding >= 0 && format.encoding <= static_cast<int>(UniMode::uni16LE_NoBOM)
				&& (format.encoding != NppSaveFormat::Legacy || FileIO::isCodepageSupported(format.codepage))
				&& (format.encoding != static_cast<int>(b->unicode) || format.codepage != b->codepage))
			{
				b->unicode = static_cast<UniMode>(format.encoding);
				b->codepage = format.codepage;
				b->dirty = b->forcedDirty = true;
			}
			mw->updateBufferTabs(b);
			if (b == mw->currentBuffer())
			{
				mw->updateTitle();
				mw->updateStatusBar();
				mw->updateMenuState();
			}
			return TRUE;
		}

		case NPPM_LINUX_GETBUFFERTEXT:
		{
			Buffer* buffer = bufferOf(wParam);
			if (!buffer)
				return -1;
			const auto* destination = reinterpret_cast<const NppBufferText*>(lParam);
			sptr_t length = 0;
			mw->withDocument(buffer, [&](EditView* editor)
			{
				length = editor->ex(SCI_GETLENGTH);
				if (length > 0 && destination && destination->data && destination->capacity >= static_cast<size_t>(length))
				{
					const auto* text = reinterpret_cast<const char*>(editor->ex(SCI_GETCHARACTERPOINTER));
					std::memcpy(destination->data, text, static_cast<size_t>(length));
				}
			});
			return length;
		}

		case NPPM_ADDTOOLBARICON_DEPRECATED:
		case NPPM_ADDTOOLBARICON_FORDARKMODE:
		{
			QAction* a = commandAction(static_cast<int>(wParam));
			if (!a || !lParam || !mw->_toolbar)
				return FALSE;
			const QIcon* icon = nullptr;
			if (msg == NPPM_ADDTOOLBARICON_FORDARKMODE)
			{
				const auto* icons = reinterpret_cast<const sdk::toolbarIconsWithDarkMode*>(lParam);
				icon = pluginIcon(Npp::prefs().isDark() && icons->hToolbarIconDarkMode ? icons->hToolbarIconDarkMode
					: (icons->hToolbarIcon ? icons->hToolbarIcon : icons->hToolbarBmp));
			}
			else
			{
				const auto* icons = reinterpret_cast<const sdk::toolbarIcons*>(lParam);
				icon = pluginIcon(icons->hToolbarIcon ? icons->hToolbarIcon : icons->hToolbarBmp);
			}
			if (icon)
				a->setIcon(*icon);
			if (_toolbarActions.isEmpty())
				_toolbarActions << mw->_toolbar->addSeparator();
			mw->_toolbar->addAction(a);
			_toolbarActions << a;
			return TRUE;
		}

		case NPPM_GETWINDOWSVERSION:
			return sdk::WV_WIN10;

		case NPPM_MAKECURRENTBUFFERDIRTY:
			if (Buffer* b = mw->currentBuffer())
			{
				b->forcedDirty = true;
				b->dirty = true;
				mw->updateBufferTabs(b);
				mw->updateTitle();
			}
			return TRUE;

		case NPPM_GETENABLETHEMETEXTUREFUNC_DEPRECATED:
			return 0;

		case NPPM_GETPLUGINSCONFIGDIR:
		{
			const QString dir = pluginsConfigDir();
			if (!lParam)
				return static_cast<intptr_t>(wlen(dir));
			if (wParam < wlen(dir) + 1)
				return FALSE;
			toW(dir, reinterpret_cast<wchar_t*>(lParam), wParam);
			return TRUE;
		}

		case NPPM_MSGTOPLUGIN:
		{
			Plugin* p = pluginOfModule(fromW(static_cast<intptr_t>(wParam)));
			if (!p)
				return FALSE;
			p->messageProc(msg, wParam, lParam);
			return TRUE;
		}

		case NPPM_GETNPPVERSION:
		{
			const QStringList parts = Npp::version().split(QLatin1Char('.'));
			const int major = parts.value(0).toInt();
			QString minor;
			for (int i = 1; i < parts.size() && i < 4; ++i)
				minor += parts[i].left(1);
			if (wParam)
				minor = minor.leftJustified(3, QLatin1Char('0'));
			return MAKELONG(minor.toInt(), major);
		}

		case NPPM_HIDETABBAR:
		case NPPM_ISTABBARHIDDEN:
		{
			const bool hidden = mw->viewTabs(0)->isHidden();
			if (msg == NPPM_HIDETABBAR)
			{
				for (int v = 0; v < 2; ++v)
					mw->viewTabs(v)->setVisible(lParam == 0);
			}
			return hidden ? TRUE : FALSE;
		}

		case NPPM_HIDETOOLBAR:
		case NPPM_ISTOOLBARHIDDEN:
		{
			const bool hidden = !mw->_toolbar || mw->_toolbar->isHidden();
			if (msg == NPPM_HIDETOOLBAR && mw->_toolbar)
				mw->_toolbar->setVisible(lParam == 0);
			return hidden ? TRUE : FALSE;
		}

		case NPPM_HIDEMENU:
		case NPPM_ISMENUHIDDEN:
		{
			const bool hidden = mw->menuBar()->isHidden();
			if (msg == NPPM_HIDEMENU)
				mw->menuBar()->setVisible(lParam == 0);
			return hidden ? TRUE : FALSE;
		}

		case NPPM_HIDESTATUSBAR:
		case NPPM_ISSTATUSBARHIDDEN:
		{
			const bool hidden = mw->statusBar()->isHidden();
			if (msg == NPPM_HIDESTATUSBAR)
				mw->statusBar()->setVisible(lParam == 0);
			return hidden ? TRUE : FALSE;
		}

		case NPPM_GETPOSFROMBUFFERID:
		{
			const Buffer* b = bufferOf(wParam);
			if (!b)
				return -1;
			const int first = (lParam == 1) ? 1 : 0;
			for (int view : {first, 1 - first})
			{
				const int index = mw->indexOf(view, b);
				if (index >= 0)
					return (static_cast<intptr_t>(view) << 30) | index;
			}
			return -1;
		}

		case NPPM_GETFULLPATHFROMBUFFERID:
		{
			const Buffer* b = bufferOf(wParam);
			if (!b)
				return -1;
			const QString path = b->fullName();
			if (lParam)
				toW(path, reinterpret_cast<wchar_t*>(lParam), wlen(path) + 1);
			return static_cast<intptr_t>(wlen(path));
		}

		case NPPM_GETBUFFERIDFROMPOS:
		{
			const int view = static_cast<int>(lParam);
			if (view != 0 && view != 1)
				return 0;
			return reinterpret_cast<intptr_t>(mw->bufferAt(view, static_cast<int>(wParam)));
		}

		case NPPM_GETCURRENTBUFFERID:
			return reinterpret_cast<intptr_t>(mw->currentBuffer());

		case NPPM_RELOADBUFFERID:
		{
			Buffer* b = bufferOf(wParam);
			if (!b || b->isUntitled())
				return FALSE;
			if (lParam && b->dirty && QMessageBox::question(mw, QStringLiteral("Reload"),
				QStringLiteral("Are you sure you want to reload the current file and lose the changes made in LinuxPad++?")) != QMessageBox::Yes)
				return FALSE;
			mw->reloadBuffer(b);
			return TRUE;
		}

		case NPPM_GETBUFFERLANGTYPE:
		{
			const Buffer* b = bufferOf(wParam);
			return b ? static_cast<intptr_t>(langTypeOf(b)) : -1;
		}

		case NPPM_SETBUFFERLANGTYPE:
		{
			Buffer* b = bufferOf(wParam);
			if (!b || lParam < 0 || lParam >= StyleManager::langCount() || lParam == L_USER || lParam == L_EXTERNAL)
				return FALSE;
			if (b == mw->currentBuffer())
			{
				mw->languageSet(static_cast<LangType>(lParam));
			}
			else
			{
				b->lang = static_cast<LangType>(lParam);
				b->udlName.clear();
				for (int v = 0; v < 2; ++v)
				{
					if (mw->viewCurrent(v) == b)
						mw->viewEdit(v)->defineDocType(b->lang);
				}
				notifyBuffer(NPPN_LANGCHANGED, b);
			}
			return TRUE;
		}

		case NPPM_GETBUFFERENCODING:
		{
			const Buffer* b = bufferOf(wParam);
			return b ? static_cast<intptr_t>(b->unicode) : -1;
		}

		case NPPM_SETBUFFERENCODING:
		{
			// only on new, unedited documents
			Buffer* b = bufferOf(wParam);
			if (!b || lParam < 0 || lParam > 7 || b->dirty || !b->isUntitled())
				return FALSE;
			b->unicode = static_cast<UniMode>(lParam);
			mw->updateStatusBar();
			mw->updateMenuState();
			return TRUE;
		}

		case NPPM_GETBUFFERFORMAT:
		{
			const Buffer* b = bufferOf(wParam);
			return b ? b->eol : -1;
		}

		case NPPM_SETBUFFERFORMAT:
		{
			Buffer* b = bufferOf(wParam);
			if (!b || lParam < SC_EOL_CRLF || lParam > SC_EOL_LF)
				return FALSE;
			if (b == mw->currentBuffer())
			{
				mw->editEol(static_cast<int>(lParam));
			}
			else
			{
				mw->withDocument(b, [lParam](EditView* e)
				{
					e->ex(SCI_SETEOLMODE, static_cast<uptr_t>(lParam));
					e->ex(SCI_CONVERTEOLS, static_cast<uptr_t>(lParam));
				});
				b->eol = static_cast<int>(lParam);
			}
			return TRUE;
		}

		case NPPM_GETSHORTCUTBYCMDID:
		{
			auto* sk = reinterpret_cast<sdk::ShortcutKey*>(lParam);
			QAction* a = commandAction(static_cast<int>(wParam));
			if (!sk || !a || a->shortcut().isEmpty())
				return FALSE;
			const QKeyCombination kc = a->shortcut()[0];
			sk->_isCtrl = kc.keyboardModifiers() & Qt::ControlModifier;
			sk->_isAlt = kc.keyboardModifiers() & Qt::AltModifier;
			sk->_isShift = kc.keyboardModifiers() & Qt::ShiftModifier;
			sk->_key = static_cast<UCHAR>(vkOf(kc.key()));
			return sk->_key ? TRUE : FALSE;
		}

		case NPPM_REMOVESHORTCUTBYCMDID:
		{
			QAction* a = commandAction(static_cast<int>(wParam));
			if (!a)
				return FALSE;
			a->setShortcut(QKeySequence());
			return TRUE;
		}

		case NPPM_DOOPEN:
			return mw->openFile(pathW(lParam)) ? TRUE : FALSE;

		case NPPM_GETCURRENTNATIVELANGENCODING:
			return 65001; // CP_UTF8: the nativeLang files are UTF-8

		case NPPM_ALLOCATESUPPORTED_DEPRECATED:
			return TRUE;

		case NPPM_ALLOCATECMDID:
		case NPPM_ALLOCATEMARKER:
		case NPPM_ALLOCATEINDICATOR:
		{
			auto* start = reinterpret_cast<int*>(lParam);
			int* next = (msg == NPPM_ALLOCATECMDID) ? &_nextDynamicId : (msg == NPPM_ALLOCATEMARKER ? &_nextMarker : &_nextIndicator);
			const int limit = (msg == NPPM_ALLOCATECMDID) ? kPluginsCmdDynamicLimit : (msg == NPPM_ALLOCATEMARKER ? kMarkerPluginsLimit : kIndicatorPluginsLimit);
			const int n = static_cast<int>(wParam);
			if (!start)
				return FALSE;
			if (n <= 0 || *next + n - 1 > limit)
			{
				*start = 0;
				return FALSE;
			}
			*start = *next;
			if (msg == NPPM_ALLOCATECMDID)
			{
				for (int id = *next; id < *next + n; ++id)
					mw->_handlers.insert(id, [this, id] { runCommand(id); });
			}
			*next += n;
			return TRUE;
		}

		case NPPM_GETLANGUAGENAME:
		case NPPM_GETLANGUAGEDESC:
		{
			const LangType lang = static_cast<LangType>(wParam);
			QString name = externalLexerName(static_cast<intptr_t>(wParam)); // Notepad_plus::getLangDesc(): the name of the lexer
			if (lang == L_USER && mw->currentBuffer() && mw->currentBuffer()->lang == L_USER)
				name = mw->currentBuffer()->udlName;
			if (name.isEmpty())
			{
				const LanguageNameInfo& info = StyleManager::langInfo(lang);
				name = QString::fromUtf8(msg == NPPM_GETLANGUAGENAME ? info.shortName : info.longName);
			}
			if (lParam)
				toW(name, reinterpret_cast<wchar_t*>(lParam), wlen(name) + 1);
			return static_cast<intptr_t>(wlen(name));
		}

		case NPPM_SHOWDOCLIST:
		{
			const bool shown = mw->_docListDock && !mw->_docListDock->isHidden();
			if (shown != (lParam != 0))
				mw->toggleDocumentList();
			return TRUE;
		}

		case NPPM_ISDOCLISTSHOWN:
			return (mw->_docListDock && !mw->_docListDock->isHidden()) ? TRUE : FALSE;

		case NPPM_GETAPPDATAPLUGINSALLOWED:
			return TRUE; // ~/.local/share/linuxpad++/plugins is always looked at

		case NPPM_GETCURRENTVIEW:
			return mw->activeView();

		case NPPM_DOCLISTDISABLEEXTCOLUMN:
		case NPPM_DOCLISTDISABLEPATHCOLUMN:
		case NPPM_SETEDITORBORDEREDGE:
		case NPPM_DISABLEAUTOUPDATE:
			return TRUE;

		case NPPM_GETEDITORDEFAULTFOREGROUNDCOLOR:
			return mw->editView()->ex(SCI_STYLEGETFORE, STYLE_DEFAULT);

		case NPPM_GETEDITORDEFAULTBACKGROUNDCOLOR:
			return mw->editView()->ex(SCI_STYLEGETBACK, STYLE_DEFAULT);

		case NPPM_SETSMOOTHFONT:
			for (int v = 0; v < 2; ++v)
				mw->viewEdit(v)->ex(SCI_SETFONTQUALITY, lParam ? SC_EFF_QUALITY_LCD_OPTIMIZED : SC_EFF_QUALITY_DEFAULT);
			return TRUE;

		case NPPM_GETPLUGINHOMEPATH:
			return copyString(pluginsDirs().value(0), wParam, lParam, true); // plugins/<folder name> is the folder of a plugin

		case NPPM_GETSETTINGSONCLOUDPATH:
		{
			QFile f(Npp::cloudChoiceFile());
			QString cloud;
			if (f.open(QIODevice::ReadOnly))
				cloud = QString::fromUtf8(f.readAll()).trimmed();
			return cloud.isEmpty() ? 0 : copyString(cloud, wParam, lParam, true);
		}

		case NPPM_GETNPPSETTINGSDIRPATH:
			return copyString(Npp::configDir(), wParam, lParam, true);

		case NPPM_SETLINENUMBERWIDTHMODE:
		{
			if (lParam != LINENUMWIDTH_DYNAMIC && lParam != LINENUMWIDTH_CONSTANT)
				return FALSE;
			Npp::prefs().lineNumberDynamicWidth = (lParam == LINENUMWIDTH_DYNAMIC);
			for (int v = 0; v < 2; ++v)
				mw->viewEdit(v)->updateLineNumberWidth();
			return TRUE;
		}

		case NPPM_GETLINENUMBERWIDTHMODE:
			return Npp::prefs().lineNumberDynamicWidth ? LINENUMWIDTH_DYNAMIC : LINENUMWIDTH_CONSTANT;

		case NPPM_GETEXTERNALLEXERAUTOINDENTMODE:
		{
			const QString name = fromW(static_cast<intptr_t>(wParam));
			if (!lParam || !_externalAutoIndent.contains(name))
				return FALSE;
			*reinterpret_cast<int*>(lParam) = _externalAutoIndent.value(name);
			return TRUE;
		}

		case NPPM_SETEXTERNALLEXERAUTOINDENTMODE:
		{
			const QString name = fromW(static_cast<intptr_t>(wParam));
			if (name.isEmpty())
				return FALSE;
			_externalAutoIndent.insert(name, static_cast<int>(lParam));
			return TRUE;
		}

		case NPPM_ISAUTOINDENTON:
			return Npp::prefs().autoIndentMode != AUTOINDENT_NONE ? TRUE : FALSE;

		case NPPM_GETCURRENTMACROSTATUS:
			if (mw->_recording)
				return static_cast<int>(sdk::MacroStatus::RecordInProgress);
			if (mw->_playingMacro)
				return static_cast<int>(sdk::MacroStatus::PlayingBack);
			return static_cast<int>(mw->_macro.empty() ? sdk::MacroStatus::Idle : sdk::MacroStatus::RecordingStopped);

		case NPPM_ISDARKMODEENABLED:
			return Npp::prefs().isDark() ? TRUE : FALSE;

		case NPPM_GETDARKMODECOLORS:
		{
			// NppDarkMode::Colors: 12 COLORREF
			constexpr size_t kColors = 12;
			if (wParam != kColors * sizeof(COLORREF) || !lParam)
				return FALSE;
			const QPalette pal = QApplication::palette();
			const COLORREF colors[kColors] = {
				colorRef(pal.color(QPalette::Window)), colorRef(pal.color(QPalette::Button)),
				colorRef(pal.color(QPalette::Midlight)), colorRef(pal.color(QPalette::Base)),
				colorRef(QColor(0xB0, 0x00, 0x00)), colorRef(pal.color(QPalette::WindowText)),
				colorRef(pal.color(QPalette::Text).darker(115)), colorRef(pal.color(QPalette::Disabled, QPalette::Text)),
				colorRef(pal.color(QPalette::Link)), colorRef(pal.color(QPalette::Mid)),
				colorRef(pal.color(QPalette::Light)), colorRef(pal.color(QPalette::Disabled, QPalette::Mid))};
			std::memcpy(reinterpret_cast<void*>(lParam), colors, sizeof(colors));
			return TRUE;
		}

		case NPPM_GETCURRENTCMDLINE:
		{
			QStringList args = QCoreApplication::arguments();
			if (!args.isEmpty())
				args.removeFirst();
			return copyString(args.join(QLatin1Char(' ')), wParam, lParam, true);
		}

		case NPPM_CREATELEXER:
			return reinterpret_cast<intptr_t>(CreateLexer(fromW(lParam).toUtf8().constData()));

		case NPPM_GETBOOKMARKID:
			return MARK_BOOKMARK;

		case NPPM_DARKMODESUBCLASSANDTHEME:
			return static_cast<intptr_t>(wParam); // Qt widgets follow the palette of the application

		case NPPM_GETTABCOLORID:
		{
			const int view = (static_cast<intptr_t>(wParam) == -1) ? mw->activeView() : static_cast<int>(wParam);
			if (view != 0 && view != 1)
				return -1;
			const Buffer* b = (lParam == -1) ? mw->viewCurrent(view) : mw->bufferAt(view, static_cast<int>(lParam));
			return b ? b->tabColor : -1;
		}

		case NPPM_SETUNTITLEDNAME:
		{
			Buffer* b = (static_cast<intptr_t>(wParam) == -1) ? mw->currentBuffer() : bufferOf(wParam);
			const QString name = fromW(lParam);
			if (!b || !b->isUntitled() || name.isEmpty())
				return FALSE;
			b->untitledName = name;
			b->untitledTabRenamed = true;
			mw->updateBufferTabs(b);
			mw->updateTitle();
			return TRUE;
		}

		case NPPM_GETNATIVELANGFILENAME:
		{
			const QByteArray file = Npp::prefs().localization.toUtf8();
			if (file.isEmpty() || file == "english.xml")
				return 0;
			if (lParam)
			{
				if (wParam < static_cast<uintptr_t>(file.size()) + 1)
					return 0;
				std::memcpy(reinterpret_cast<char*>(lParam), file.constData(), static_cast<size_t>(file.size()) + 1);
			}
			return file.size();
		}

		case NPPM_ADDSCNMODIFIEDFLAGS:
			_scnModifiedFlags |= static_cast<unsigned int>(lParam);
			return TRUE;

		case NPPM_GETTOOLBARICONSETCHOICE:
			return Npp::prefs().toolbarIconSet;

		// RUNCOMMAND_USER: the Run variables
		case NPPM_GETFULLCURRENTPATH:
		case NPPM_GETCURRENTDIRECTORY:
		case NPPM_GETFILENAME:
		case NPPM_GETNAMEPART:
		case NPPM_GETEXTPART:
		case NPPM_GETCURRENTWORD:
		case NPPM_GETNPPDIRECTORY:
		case NPPM_GETNPPFULLFILEPATH:
		case NPPM_GETCURRENTLINESTR:
		case NPPM_GETFILENAMEATCURSOR:
		{
			QString s;
			switch (msg)
			{
				case NPPM_GETFULLCURRENTPATH: s = mw->expandRunVariables(QStringLiteral("$(FULL_CURRENT_PATH)")); break;
				case NPPM_GETCURRENTDIRECTORY:
					s = (mw->currentBuffer() && !mw->currentBuffer()->isUntitled()) ? QFileInfo(mw->currentBuffer()->path).absolutePath() : QString();
					break;
				case NPPM_GETFILENAME: s = mw->expandRunVariables(QStringLiteral("$(FILE_NAME)")); break;
				case NPPM_GETNAMEPART: s = mw->expandRunVariables(QStringLiteral("$(NAME_PART)")); break;
				case NPPM_GETEXTPART: s = mw->expandRunVariables(QStringLiteral("$(EXT_PART)")); break;
				case NPPM_GETCURRENTWORD: s = mw->expandRunVariables(QStringLiteral("$(CURRENT_WORD)")); break;
				case NPPM_GETNPPDIRECTORY: s = QCoreApplication::applicationDirPath(); break;
				case NPPM_GETNPPFULLFILEPATH: s = QCoreApplication::applicationFilePath(); break;
				case NPPM_GETCURRENTLINESTR:
				{
					EditView* e = mw->editView();
					s = QString::fromUtf8(e->lineText(e->ex(SCI_LINEFROMPOSITION, e->ex(SCI_GETCURRENTPOS))));
					while (s.endsWith(QLatin1Char('\n')) || s.endsWith(QLatin1Char('\r')))
						s.chop(1);
					break;
				}
				case NPPM_GETFILENAMEATCURSOR:
				{
					// the characters of a file name around the caret
					EditView* e = mw->editView();
					const sptr_t line = e->ex(SCI_LINEFROMPOSITION, e->ex(SCI_GETCURRENTPOS));
					const QString text = QString::fromUtf8(e->lineText(line));
					const sptr_t col = e->ex(SCI_GETCURRENTPOS) - e->ex(SCI_POSITIONFROMLINE, line);
					const int pos = QString::fromUtf8(e->lineText(line).left(static_cast<int>(col))).size();
					auto isPathChar = [](QChar c) { return c.isLetterOrNumber() || QStringLiteral("_./\\:~-+%#@").contains(c); };
					int start = pos;
					int end = pos;
					while (start > 0 && isPathChar(text[start - 1]))
						--start;
					while (end < text.size() && isPathChar(text[end]))
						++end;
					s = text.mid(start, end - start);
					break;
				}
				default: break;
			}
			if (!lParam || wParam < wlen(s) + 1)
				return FALSE;
			toW(s, reinterpret_cast<wchar_t*>(lParam), wParam);
			return TRUE;
		}

		case NPPM_GETCURRENTLINE:
		{
			EditView* e = mw->editView();
			return e->ex(SCI_LINEFROMPOSITION, e->ex(SCI_GETCURRENTPOS));
		}

		case NPPM_GETCURRENTCOLUMN:
		{
			EditView* e = mw->editView();
			return e->ex(SCI_GETCOLUMN, e->ex(SCI_GETCURRENTPOS));
		}

		default:
			break;
	}
	return 0;
}

int PluginsManager::vkFromQtKey(int key)
{
	return vkOf(key);
}
