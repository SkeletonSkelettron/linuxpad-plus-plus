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

// Main window of the Linux front end: Qt counterpart of Notepad_plus.cpp,
// Notepad_plus_Window.cpp, NppIO.cpp and NppNotification.cpp.

#pragma once

#include <functional>
#include <vector>

#include <QDateTime>
#include <QHash>
#include <QMainWindow>
#include <QPointer>
#include <QSet>
#include <QStringList>

#include "FileIO.h"
#include "NppCommon.h"
#include "TabBar.h"

class CharacterPanel;
class ClipboardHistoryPanel;
class DocumentMap;
struct SavedMacro;
struct SavedCommand;
class EditView;
class FindReplaceDlg;
class FunctionListPanel;
class IncrementalFindBar;
class PluginsManager;
class ProjectPanel;
class UdlDialog;
class QBoxLayout;
class QFrame;
class QSplitter;
class QSystemTrayIcon;
class SearchResultsPanel;
class QAction;
class QActionGroup;
class QDockWidget;
class QFileSystemWatcher;
class QLabel;
class QMenu;
class QTabBar;
class QTimer;
class QToolBar;
class QListWidget;
class QTreeView;
class QFileSystemModel;

namespace Scintilla { struct NotificationData; }

struct MacroStep
{
	enum Kind { Scintilla, Menu } kind = Scintilla;
	unsigned int message = 0;
	uptr_t wParam = 0;
	sptr_t lParam = 0;
	QByteArray text;
	bool hasText = false;
};

struct OpenOptions
{
	sptr_t line = -1;
	sptr_t column = -1;
	LangType lang = L_EXTERNAL; // L_EXTERNAL: detect
	bool readOnly = false;
};

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	MainWindow();
	~MainWindow() override;

	void openFromCommandLine(const QStringList& paths, const OpenOptions& opts);
	bool restoreSession();

	// the two views of split view (Notepad++ MAIN_VIEW / SUB_VIEW); _edit, _tabs and
	// _current are the members of the active view, like _pEditView and _pDocTab
	enum { MAIN_VIEW = 0, SUB_VIEW = 1 };

	EditView* editView() const { return _edit; }
	EditView* scratchView() const { return _loader; }
	Buffer* currentBuffer() const { return _current; }
	std::vector<Buffer*> allBuffers() const; // documents of both views, each once
	Buffer* bufferAt(int tabIndex) const;    // active view
	int indexOf(const Buffer* buf) const;    // active view
	Buffer* bufferAt(int view, int tabIndex) const;
	int indexOf(int view, const Buffer* buf) const;
	std::vector<Buffer*> viewBuffers(int view) const;
	Buffer* findBuffer(const QString& path) const;
	int activeView() const { return _activeView; }
	int otherView() const { return 1 - _activeView; }
	bool viewVisible(int view) const;
	EditView* viewEdit(int view) const { return _views[view].edit; }
	NppTabBar* viewTabs(int view) const { return _views[view].tabs; }
	Buffer* viewCurrent(int view) const { return view == _activeView ? _current : _views[view].current; }
	void setActiveView(int view);
	void moveToOtherView(bool clone); // View > Move/Clone Current Document
	void transferBuffer(Buffer* buf, int fromView, int toView, int insertAt, bool clone);

	Buffer* openFile(const QString& path, bool activate = true, const OpenOptions& opts = OpenOptions());
	Buffer* newDocument(bool activate = true);
	void activateBuffer(Buffer* buf, int view = -1); // -1: the active view if it has the document
	bool saveBuffer(Buffer* buf, bool saveAs = false, bool saveCopy = false, const QString& targetPath = QString()); // targetPath: no file dialog
	bool closeBuffer(Buffer* buf, bool askToSave = true, int view = -1);
	void reloadBuffer(Buffer* buf, FileIO::Force force = FileIO::Force::Auto, int codepage = CP_ANSI, bool keepPosition = true);
	QByteArray bufferText(Buffer* buf) const;
	void withDocument(Buffer* buf, const std::function<void(EditView*)>& fn) const;

	void command(int id);
	void showFindDialog(int tab);
	SearchResultsPanel* resultsPanel();
	void gotoPosition(Buffer* buf, sptr_t start, sptr_t end);
	void applyTheme(const QString& path);
	void applyPrefs();
	void applyDialogFont();
	// Preferences applied to the window (Preferences.cpp has the dialog)
	void applyMenuBarPrefs();
	void applyToolbarIcons();
	void applyTabBarPrefs();
	void applyLanguageMenu();
	void applyDarkMode();
	void applyTray();
	void applyPanelsState();
	QString openSaveDir() const;
	void rememberOpenSaveDir(const QString& file);
	void backupOnSave(const QString& path);
	void toggleDistractionFree();
	void showDocumentsMenu(const QPoint& globalPos);
	void documentSwitcher(bool forward);
	void showTabPeek(int view, int index, const QPoint& globalPos);
	void hideTabPeek();
	void useFirstLineAsTabName(Buffer* buf);
	bool isLargeFileSize(qint64 size) const;

	void applyShortcuts();
	void applyScintillaKeys(); // ScintillaAccelerator::updateKeys()
	void setMultiInstance(bool on) { _multiInst = on; }
	QStringList recentFiles() const { return _recent; }
	void retranslateMenus();
	PluginsManager* pluginsManager() const { return _pluginsManager; }
	void loadPlugins(bool enabled);                  // -noPlugin: not loaded
	void pluginsReady(const QString& pluginMessage); // NPPN_READY, NPPN_CMDLINEPLUGINMSG
	void setStatusText(const QString& text, int msTimeout = 5000);
	QAction* action(int id) const { return _actions.value(id); }
	QList<int> commandIds() const { return _actions.keys(); }
	QString commandName(int id) const;
	QString commandCategory(int id) const;
	bool hasCommand(int id) const { return _handlers.contains(id); }
	int commandOrder(int id) const { return _commandOrder.value(id, 1 << 20); }

protected:
	void closeEvent(QCloseEvent* event) override;
	void changeEvent(QEvent* event) override;
	void dragEnterEvent(QDragEnterEvent* event) override;
	void dropEvent(QDropEvent* event) override;
	bool eventFilter(QObject* obj, QEvent* event) override;
	void resizeEvent(QResizeEvent* event) override;

private:
	// construction
	void buildMenus();
	void buildToolbar();
	void buildStatusBar();
	void buildTabBar(int view);
	void registerCommands();
	void on(int id, std::function<void()> fn);
	void setupCheckable();

	// state updates
	void updateTitle();
	void updateStatusBar();
	void updateMenuState();
	void updateTabLabel(int index) { updateTabLabel(_activeView, index); }
	void updateTabLabel(int view, int index);
	void updateBufferTabs(Buffer* buf); // tabs of this document in both views
	void updateAllTabs();
	void updateTabBarFocus();
	QString tabLabel(const Buffer* b) const;
	void onSavePointChanged(int view, bool dirty);
	void onSciNotify(Scintilla::NotificationData* scn, int view);
	void onMacroRecord(unsigned int message, uptr_t wParam, sptr_t lParam);
	void saveCurrentPosition();
	void saveViewPosition(int view);
	void restorePosition(Buffer* buf);
	void loadIntoDocument(Buffer* buf, const QByteArray& utf8, bool resetUndo);
	void addTab(int view, Buffer* buf, int insertAt = -1);
	void removeTab(Buffer* buf, int view);
	void showView(int view);
	void hideView(int view);
	void checkSyncState();
	void syncScroll(int fromView);
	void syncZoom(int fromView);
	void smartHighlightOtherView();
	void refreshOtherViews(Buffer* buf); // the other view shows the same document: restyle it
	void rotateSplitter();
	int viewOfTabBar(const QObject* obj) const;
	int viewOfWidget(const QObject* obj) const;
	QIcon tabIconOf(const Buffer* buf) const;
	QString nextUntitledName() const;
	bool isCleanEmptyUntitled(Buffer* buf) const;

	// recent files
	void addRecent(const QString& path);
	void rebuildRecentMenu();
	QString recentLabel(const QString& path) const;


	// file status detection
	void watch(Buffer* buf);
	void unwatch(Buffer* buf);
	void onFileChanged(const QString& path);
	void checkModifiedFiles(bool onlyPending);

	// session & snapshot
	void writeSession(const QString& path, bool withBackups);
	bool readSession(const QString& path);
	void backupSnapshot();
	void removeBackup(Buffer* buf);

	// docking panels
	void toggleSearchResults();
	void toggleWorkspace(const QString& folder = QString());
	void toggleDocumentList();
	void refreshDocumentList();
	void toggleDocumentMap();
	void toggleFunctionList(bool focus = false);
	void toggleCharacterPanel();
	void toggleProjectPanel(int index, bool focus = false); // View > Project Panels > Project Panel 1-3
	bool projectPanelShown(int index) const;
	QStringList projectFiles(int panelMask, const QStringList& patterns) const; // Find in Projects
	void toggleClipboardHistory();
	void addRightDock(QDockWidget* dock); // docked on the right, tabbed with the panels already there
	void pasteSpecial(int id); // Edit > Paste Special
	void incrementalSearch(); // Search > Incremental Search
	void placeIncrementalBar(bool atTop);
	void refreshPanels(bool forceFunctionList = false); // Document Map and Function List follow the active document
	bool docMapShown() const;
	void applyPanelColours(); // theme's Default Style colours on the panel lists
	bool funcListShown() const;

	// context menus
	void showEditorContextMenu(const QPoint& globalPos);
	static QString contextMenuPath();
	void loadContextMenu();      // contextMenu.xml, or the default of NppConstants.h
	void editContextMenuFile();  // Settings > Edit Popup ContextMenu
	void showTabContextMenu(const QPoint& globalPos, int index);

	// commands (Commands.cpp)
	void fileOpen();
	void fileSaveAll();
	void fileCloseAll(bool butCurrent, bool butPinned, int direction, bool unchangedOnly);
	void fileRename();
	void fileDelete();
	void fileLoadSession();
	void fileSaveSession();
	void filePrint(bool showDialog);
	void openContainingFolder();
	void openTerminal(bool powershell);
	void editConvertCase(int id);
	void editLineOperation(int id);
	void editSortLines(int id);
	void editComment(int id);
	void editEol(int eolMode);
	void editBlankOperation(int id);
	void editInsertDateTime(int id);
	void editCopyToClipboard(int id);
	void editMultiSelect(int id);
	void editOpenSelection(int id);
	void editColumnEditor();
	void editAutoComplete(bool wordsOnly, bool force);
	void editFunctionCallTip(int direction);
	void searchFindNext(bool forward, bool selectFirst, bool volatileSearch);
	void searchGotoLine();
	void searchBookmark(int id);
	void searchStyleToken(int id);
	void searchJumpToStyle(int id);
	void searchCopyStyled(int id);
	void searchMatchingBrace(bool select);
	void searchChangeHistory(int id);
	void viewToggle(int id);
	void viewZoom(int id);
	void viewTab(int id);
	void viewFold(int id);
	void viewInBrowser(int id);
	void viewSummary();
	void viewFullScreen(bool postIt);
	void encodingSet(int id);
	void encodingConvert(int id);
	void languageSet(LangType lang, const QString& udlName = QString());
	void rebuildUdlMenu();                       // the User Defined Languages at the end of the Language menu
	void showUdlDialog();                        // Language > User Defined Language > Define your language...
	void udlChanged(const QString& name);        // the documents in this language are styled again ("" = User-Defined)
	void udlRenamed(const QString& oldName, const QString& newName);
	void udlRemoved(const QString& name);        // its documents become User-Defined
	LangType langOfFile(const QString& path, QString* udlName) const; // UDL extensions first, like Buffer::setFileName()
	QString langDescription(const Buffer* b) const; // Notepad_plus::getLangDesc()
	void macroStart();
	void macroStop();
	void macroPlayback(int times = 1);
	void macroRunMultiple();
	void macroSave();        // Macro > Save Current Recorded Macro...
	void macroManage();      // Macro > Modify Shortcut/Delete Macro...
	void macroPlaySteps(const std::vector<MacroStep>& steps, int times);
	void rebuildMacroMenu(); // the saved macros of shortcuts.xml in the Macro menu
	void rebuildRunMenu();   // the saved commands of shortcuts.xml in the Run menu
	void runCommandLine(const QString& cmd); // Command::run()
	void commandsManage();   // Run > Modify Shortcut/Delete Command...
	bool checkShortcutsXml(); // shortcuts.xml security validation before a saved macro or command runs
	void runDialog();
	void toolHash(int id);
	void windowSort(int id);
	void windowList();
	void settingsPreferences();
	void settingsStyleConfigurator();
	void settingsShortcutMapper();
	void settingsImportTheme();
	void helpAbout();
	void helpDebugInfo();
	void helpCommandLine();
	QString expandRunVariables(const QString& cmd) const;
	bool isMacroRecordable(int id) const;

	struct DocView
	{
		EditView* edit = nullptr;
		NppTabBar* tabs = nullptr;
		QWidget* panel = nullptr;      // one side of the splitter: tab bar and editor
		QBoxLayout* layout = nullptr;  // direction follows Tab Bar > Vertical
		QWidget* editHolder = nullptr; // Margins/Border/Edge > Border Width
		Buffer* current = nullptr;     // current document when the view is not the active one
	};
	DocView _views[2];
	int _activeView = MAIN_VIEW;
	int _targetView = -1; // view that openFile / newDocument add to (-1: the active view)
	QSplitter* _splitter = nullptr;
	struct SyncInfo
	{
		bool vertical = false;
		bool horizontal = false;
		sptr_t line = 0;
		sptr_t column = 0;
	} _sync;
	bool _syncingScroll = false;
	bool _syncingZoom = false;
	NppTabBar::Look _tabLook;
	QColor _tabUnfocusedColor;

	EditView* _edit = nullptr;
	EditView* _loader = nullptr;
	NppTabBar* _tabs = nullptr;
	QWidget* _menuCorner = nullptr;
	QMenu* _recentSubMenu = nullptr;
	QAction* _recentSubMenuAction = nullptr;
	QList<QMenu*> _letterMenus;     // compact Language menu (A, B, C ...)
	QList<QAction*> _flatLangActions; // Language menu entries added for the non-compact menu
	QAction* _langMenuAnchor = nullptr;
	QSystemTrayIcon* _tray = nullptr;
	QFrame* _switcher = nullptr;
	QListWidget* _switcherList = nullptr;
	EditView* _peekView = nullptr;
	QFrame* _peek = nullptr;
	std::vector<Buffer*> _mru;      // most recently used documents first (Ctrl+Tab)
	bool _menuShownByAlt = false;
	bool _altAlone = false;
	bool _reallyQuit = false;
	bool _distractionFree = false;
	bool _dfWasFullScreen = false;
	bool _multiInst = false;
	QString _toolbarIconKey;
	QToolBar* _toolbar = nullptr;
	QLabel* _sbLang = nullptr;
	QLabel* _sbDocInfo = nullptr;
	QLabel* _sbPos = nullptr;
	QLabel* _sbEol = nullptr;
	QLabel* _sbEnc = nullptr;
	QLabel* _sbIns = nullptr;

	QHash<int, QAction*> _actions;
	QHash<int, std::function<void()>> _handlers;
	QHash<QMenu*, QString> _menuKeys;   // localization key of each menu
	QHash<QMenu*, QString> _menuLabels; // .rc caption of each menu
	QHash<int, QString> _rcLabels;      // .rc caption of each command
	QHash<int, QMenu*> _commandTopMenu; // top level menu of each command
	QHash<int, int> _commandOrder;      // position of each command in the menus
	QMenu* _fileMenu = nullptr;
	QMenu* _languageMenu = nullptr;
	QMenu* _encodingMenu = nullptr;
	QMenu* _eolMenu = nullptr;
	QMenu* _macroMenu = nullptr;
	QMenu* _windowMenu = nullptr;
	QAction* _recentSeparator = nullptr;
	QList<QAction*> _recentActions;
	QStringList _recent;
	QStringList _closedFiles;
	QActionGroup* _langGroup = nullptr;
	QActionGroup* _encGroup = nullptr;
	QActionGroup* _eolGroup = nullptr;

	QFileSystemWatcher* _watcher = nullptr;
	QSet<QString> _pendingChanges;
	bool _checkingFiles = false;
	QTimer* _backupTimer = nullptr;
	QTimer* _statusTimer = nullptr;

	FindReplaceDlg* _findDlg = nullptr;
	SearchResultsPanel* _results = nullptr;
	QDockWidget* _resultsDock = nullptr;
	QDockWidget* _workspaceDock = nullptr;
	QTreeView* _workspaceTree = nullptr;
	QFileSystemModel* _workspaceModel = nullptr;
	QDockWidget* _docListDock = nullptr;
	QListWidget* _docList = nullptr;
	QDockWidget* _docMapDock = nullptr;
	DocumentMap* _docMap = nullptr;
	QDockWidget* _funcListDock = nullptr;
	FunctionListPanel* _funcList = nullptr;
	QDockWidget* _charPanelDock = nullptr;
	CharacterPanel* _charPanel = nullptr;
	QDockWidget* _clipboardDock = nullptr;
	ClipboardHistoryPanel* _clipboardPanel = nullptr;
	Buffer* _mapPeekBuffer = nullptr;
	QDockWidget* _projectDocks[3] = {};
	ProjectPanel* _projectPanels[3] = {};
	IncrementalFindBar* _incFind = nullptr;
	UdlDialog* _udlDialog = nullptr;
	PluginsManager* _pluginsManager = nullptr;
	struct ContextMenuItem
	{
		int id = 0;     // 0: separator
		QString label;  // ItemNameAs
		QString folder; // FolderName
	};
	std::vector<ContextMenuItem> _contextMenuItems;
	QDateTime _contextMenuStamp;
	bool _contextMenuLoaded = false;
	QBoxLayout* _centralLayout = nullptr; // tab hovered with MISC. > Peek on document map

	bool _recording = false;
	std::vector<MacroStep> _macro;
	std::vector<SavedMacro> _savedMacros;
	QList<QAction*> _macroMenuItems;
	std::vector<SavedCommand> _savedCommands;
	QList<QAction*> _runMenuItems;
	bool _recordingSaved = false; // the current macro has been saved
	bool _playingMacro = false;

	Buffer* _current = nullptr;
	bool _switching = false;
	bool _closing = false;
	bool _postIt = false;
	bool _noSession = false;
	QString _instanceSession; // session opened in its own instance, saved on exit
	QString _statusMessage;
	int _callTipIndex = 0;
	sptr_t _beginSelectPos = -1;
	bool _beginSelectColumn = false;
	QByteArray _callTipFunc;

	friend class FindReplaceDlg;
	friend class SearchResultsPanel;
	friend class UdlDialog;
	friend class PluginsManager;
	friend struct MainWindowAccess;
	friend int main(int, char**);
};
