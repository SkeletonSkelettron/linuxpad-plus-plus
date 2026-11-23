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

// Plugins: Qt counterpart of MISC/PluginsManager/PluginsManager.cpp and of the
// NPPM_ part of NppBigSwitch.cpp. Plugins are shared libraries built with the
// Linux plugin SDK (PowerEditor/linux/plugin-sdk): plugins/<Name>/<Name>.so, with
// the exports of PluginInterface.h. They talk to Notepad++ with SendMessage on the
// handles of NppData, like on Windows.

#pragma once

#include <memory>
#include <vector>

#include <QHash>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QStringList>

#include "NppCommon.h"

// NPPN_ notification codes, as in Notepad_plus_msgs.h (which cannot be included
// with NppCommon.h: it has its own LangType)
#ifndef NPPN_FIRST
#define NPPN_FIRST 1000
#define NPPN_READY (NPPN_FIRST + 1)
#define NPPN_TBMODIFICATION (NPPN_FIRST + 2)
#define NPPN_FILEBEFORECLOSE (NPPN_FIRST + 3)
#define NPPN_FILEOPENED (NPPN_FIRST + 4)
#define NPPN_FILECLOSED (NPPN_FIRST + 5)
#define NPPN_FILEBEFOREOPEN (NPPN_FIRST + 6)
#define NPPN_FILEBEFORESAVE (NPPN_FIRST + 7)
#define NPPN_FILESAVED (NPPN_FIRST + 8)
#define NPPN_SHUTDOWN (NPPN_FIRST + 9)
#define NPPN_BUFFERACTIVATED (NPPN_FIRST + 10)
#define NPPN_LANGCHANGED (NPPN_FIRST + 11)
#define NPPN_WORDSTYLESUPDATED (NPPN_FIRST + 12)
#define NPPN_SHORTCUTREMAPPED (NPPN_FIRST + 13)
#define NPPN_FILEBEFORELOAD (NPPN_FIRST + 14)
#define NPPN_FILELOADFAILED (NPPN_FIRST + 15)
#define NPPN_READONLYCHANGED (NPPN_FIRST + 16)
#define NPPN_DOCORDERCHANGED (NPPN_FIRST + 17)
#define NPPN_SNAPSHOTDIRTYFILELOADED (NPPN_FIRST + 18)
#define NPPN_BEFORESHUTDOWN (NPPN_FIRST + 19)
#define NPPN_CANCELSHUTDOWN (NPPN_FIRST + 20)
#define NPPN_FILEBEFORERENAME (NPPN_FIRST + 21)
#define NPPN_FILERENAMECANCEL (NPPN_FIRST + 22)
#define NPPN_FILERENAMED (NPPN_FIRST + 23)
#define NPPN_FILEBEFOREDELETE (NPPN_FIRST + 24)
#define NPPN_FILEDELETEFAILED (NPPN_FIRST + 25)
#define NPPN_FILEDELETED (NPPN_FIRST + 26)
#define NPPN_DARKMODECHANGED (NPPN_FIRST + 27)
#define NPPN_CMDLINEPLUGINMSG (NPPN_FIRST + 28)
#define NPPN_EXTERNALLEXERBUFFER (NPPN_FIRST + 29)
#define NPPN_GLOBALMODIFIED (NPPN_FIRST + 30)
#define NPPN_NATIVELANGCHANGED (NPPN_FIRST + 31)
#define NPPN_TOOLBARICONSETCHANGED (NPPN_FIRST + 32)
#endif

class MainWindow;
class QAction;
class QDockWidget;
class QLibrary;
class QMenu;
class QPoint;
class QWidget;
struct Buffer;
struct NppHandle;
struct SCNotification;

namespace Scintilla { struct NotificationData; }

class PluginsManager : public QObject
{
	Q_OBJECT

public:
	explicit PluginsManager(MainWindow* mw);
	~PluginsManager() override;

	static QStringList pluginsDirs();  // where plugins are looked for, first the one of the installation
	struct AvailablePlugin
	{
		QString folder;     // plugins/<folder>/<folder>.so
		QString path;       // of the .so
		bool userFolder;    // in userPluginsDir() (imported), else bundled with LinuxPad++
	};
	static QList<AvailablePlugin> availablePlugins(); // the plugins of pluginsDirs(), each folder name once
	// the plugins loadPlugins() loads: the ones of [Plugins] enabled (Plugins Admin), or $NPP_PLUGINS_ONLY (tests)
	static QList<AvailablePlugin> pluginsToLoad();
	static QString userPluginsDir();   // Settings > Import > Import plugin(s)... copies there
	static QString pluginsConfigDir(); // NPPM_GETPLUGINSCONFIGDIR

	void loadPlugins();            // PluginsManager::loadPlugins()
	void addMenus();               // PluginsManager::initMenu(): before Open Plugins Folder...
	void notifyToolbar();          // NPPN_TBMODIFICATION
	void notifyReady();            // NPPN_READY
	void notify(unsigned int code, uptr_t idFrom, void* hwndFrom = nullptr); // NPPN_ notifications from the main window
	void notifyBuffer(unsigned int code, const Buffer* buf);
	void notifyBeforeSave(Buffer* buf, const QString& target);
	void relayScintilla(const Scintilla::NotificationData* scn, int view); // SCN_ notifications of both views
	void runCommand(int id);       // plugin menu commands
	bool hasPlugins() const { return !_plugins.empty(); }
	bool isReady() const { return _ready; }
	void setDestroying() { _destroying = true; } // ~MainWindow: no more messages nor notifications
	int count() const { return static_cast<int>(_plugins.size()); }
	QStringList loadedModules() const;   // "mimeTools.so" ...
	QStringList loadErrors() const { return _errors; }
	bool isPluginCommand(int id) const { return _commands.contains(id); }
	QString commandModule(int id) const;  // module name of a plugin command
	int commandIndex(int id) const;       // index in the FuncItem array of its plugin
	QList<int> commandIds() const;        // the plugin commands, in the order of the menus
	QAction* commandAction(int id) const;
	bool importPlugins(const QStringList& files, QString* error); // IDM_SETTING_IMPORTPLUGIN

	NppHandle* nppHandle() const { return _nppHandle; }
	NppHandle* scintillaHandle(int view) const { return _sciHandles[view]; }
	intptr_t message(unsigned int msg, uintptr_t wParam, intptr_t lParam); // SendMessage to nppData._nppHandle
	static int langTypeOf(const Buffer* b);              // L_EXTERNAL + n for the lexer n of the lexer plugins
	static QString externalLexerName(intptr_t langType); // the lexer of L_EXTERNAL + n, empty otherwise
	// a menu command through the procedures of plugins that subclass the main window
	// (NPPM_LINUX_SUBCLASSNPP); false: no such plugin, or the command is being run
	bool routeCommand(int id);
	// the context menu of an editing view, like Notepad++: WM_CONTEXTMENU through the subclasses of plugins, then
	// Notepad++ shows the menu; initMenuPopup sends WM_INITMENUPOPUP for a menu about to open (plugins add items)
	void editorContextMenu(int view, const QPoint& globalPos);
	void initMenuPopup(QMenu* menu);

private:
	struct Plugin;
	struct Dock
	{
		QPointer<QDockWidget> dock;
		QPointer<QWidget> client;
		QString name;
		QString module;
		int dlgID = -1;
	};

	bool loadPlugin(const QString& path);
	Plugin* pluginOfModule(const QString& module) const;
	Dock* dockOfClient(QWidget* client);
	QWidget* widgetOf(intptr_t handle) const;
	NppHandle* createHandle(unsigned int kind, QWidget* widget);
	NppHandle* menuHandle(QMenu* menu);
	intptr_t scintillaMessage(NppHandle* h, unsigned int msg, uintptr_t wParam, intptr_t lParam);
	intptr_t registerDock(const void* data);
	void setChecked(int cmdId, bool checked);
	intptr_t copyString(const QString& s, uintptr_t bufLen, intptr_t buf, bool lengthIfNull) const;
	Buffer* bufferOf(uintptr_t id) const;
	static int vkFromQtKey(int key);

	MainWindow* _mw;
	std::vector<std::unique_ptr<Plugin>> _plugins;
	QHash<int, std::pair<Plugin*, int>> _commands; // command id -> plugin, index of FuncItem
	std::vector<Dock> _docks;
	std::vector<std::unique_ptr<NppHandle>> _handles;
	QHash<QMenu*, NppHandle*> _menuHandles;
	NppHandle* _popupMenuHandle = nullptr; // the menu of initMenuPopup (a temporary QMenu)
	NppHandle* _nppHandle = nullptr;
	NppHandle* _sciHandles[2] = {};
	QStringList _errors;
	Buffer* _savingBuffer = nullptr;
	QString _saveTarget;
	int _nextCmdId;
	int _nextDynamicId;
	int _nextMarker;
	int _nextIndicator;
	unsigned int _scnModifiedFlags;
	bool _ready = false;
	QHash<QString, int> _externalAutoIndent; // NPPM_SETEXTERNALLEXERAUTOINDENTMODE
	struct WndHook
	{
		intptr_t (*dispatch)(void*, unsigned int, uintptr_t, intptr_t);
		void* context;
	};
	std::vector<WndHook> _wndHooks; // subclasses of the main window, the last one first
	size_t _wndHookLevel = 0;
	bool _routing = false;
	bool _destroying = false;
	intptr_t handleMessage(unsigned int msg, uintptr_t wParam, intptr_t lParam);
	intptr_t dispatchHooks(unsigned int msg, uintptr_t wParam, intptr_t lParam); // the subclasses from _wndHookLevel on, then handleMessage
	QList<QAction*> _toolbarActions;
};
