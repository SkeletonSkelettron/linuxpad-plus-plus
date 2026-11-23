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

#include "MainWindow.h"
#include "PluginsManager.h"

#include <algorithm>

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QBoxLayout>
#include <QCloseEvent>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QDockWidget>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QFileSystemWatcher>
#include <QHeaderView>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QClipboard>
#include <QMimeData>
#include <QPainter>
#include <QPushButton>
#include <QRegularExpression>
#include <QResizeEvent>
#include <QSettings>
#include <QSplitter>
#include <QStandardPaths>
#include <QStatusBar>
#include <QStyleHints>
#include <QSystemTrayIcon>
#include <QTabBar>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QTreeView>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "DocumentMap.h"
#include "EditPanels.h"
#include "EditView.h"
#include "EncodingMapper.h"
#include "FindReplace.h"
#include "FunctionList.h"
#include "IncrementalFind.h"
#include "Macros.h"
#include "Udl.h"
#include "UdlDialog.h"
#include "ProjectPanel.h"
#include "LangStyles.h"
#include "Localization.h"
#include "pugixml.hpp"

enum class MenuEntryKind { Item, Separator, PopupBegin, PopupEnd };
struct MenuTableEntry
{
	MenuEntryKind kind;
	const char* label;
	int id;
	const char* l10nKey;
	bool grayed;
};
#include "MenuTable.inc"

struct ShortcutTableEntry
{
	int id;
	Qt::KeyboardModifiers mods;
	Qt::Key key;
};
#include "ShortcutTable.inc"
#include "ContextMenuDefault.inc"

namespace
{
	// NppParameters::purgeMenuItemString(): no '&', nothing after the tab
	QString purgeMenuItemString(QString s)
	{
		const qsizetype tab = s.indexOf(QLatin1Char('\t'));
		if (tab >= 0)
			s.truncate(tab);
		s.remove(QLatin1Char('&'));
		return s;
	}

	// NppParameters::getCmdIdFromMenuEntryItemName(): the English names of the menus
	int cmdIdFromMenuEntryItemName(const QString& entryName, const QString& itemName)
	{
		int depth = 0;
		bool inEntry = false;
		for (const MenuTableEntry& e : kMenuTable)
		{
			switch (e.kind)
			{
				case MenuEntryKind::PopupBegin:
					if (depth == 0)
						inEntry = purgeMenuItemString(QString::fromUtf8(e.label)).compare(entryName, Qt::CaseInsensitive) == 0;
					++depth;
					break;
				case MenuEntryKind::PopupEnd:
					--depth;
					break;
				case MenuEntryKind::Item:
					if (inEntry && purgeMenuItemString(QString::fromUtf8(e.label)).compare(itemName, Qt::CaseInsensitive) == 0)
						return e.id;
					break;
				default:
					break;
			}
		}
		return -1;
	}
}

namespace
{
	QSettings& uiSettings()
	{
		static QSettings s(Npp::configDir() + QStringLiteral("/config.ini"), QSettings::IniFormat);
		return s;
	}

	// Shortcuts handled by Scintilla or by the focused widget itself: shown in the
	// menu but not registered, so that they keep working in line edits and docks.
	bool isDisplayOnlyShortcut(int id)
	{
		switch (id)
		{
			case IDM_EDIT_UNDO:
			case IDM_EDIT_REDO:
			case IDM_EDIT_CUT:
			case IDM_EDIT_COPY:
			case IDM_EDIT_PASTE:
			case IDM_EDIT_DELETE:
			case IDM_EDIT_SELECTALL:
			case IDM_EDIT_INS_TAB:
			case IDM_EDIT_RMV_TAB:
				return true;
			default:
				return false;
		}
	}

	struct ToolbarButton
	{
		int id;
		const char* icon;     // Fluent icons: icons/<light|dark>/toolbar/<regular|filled>/<icon>_off.ico
		const char* standard; // standard icons: icons/standard/toolbar/<standard>.bmp
	};

	const ToolbarButton kToolbar[] = {
		{IDM_FILE_NEW, "new", "newFile"}, {IDM_FILE_OPEN, "open", "openFile"}, {IDM_FILE_SAVE, "save", "saveFile"},
		{IDM_FILE_SAVEALL, "saveall", "saveAll"}, {IDM_FILE_CLOSE, "close", "closeFile"}, {IDM_FILE_CLOSEALL, "closeall", "closeAll"},
		{IDM_FILE_PRINT, "print", "print"}, {0, nullptr, nullptr},
		{IDM_EDIT_CUT, "cut", "cut"}, {IDM_EDIT_COPY, "copy", "copy"}, {IDM_EDIT_PASTE, "paste", "paste"}, {0, nullptr, nullptr},
		{IDM_EDIT_UNDO, "undo", "undo"}, {IDM_EDIT_REDO, "redo", "redo"}, {0, nullptr, nullptr},
		{IDM_SEARCH_FIND, "find", "find"}, {IDM_SEARCH_REPLACE, "findrep", "findReplace"}, {0, nullptr, nullptr},
		{IDM_VIEW_ZOOMIN, "zoomIn", "zoomIn"}, {IDM_VIEW_ZOOMOUT, "zoomOut", "zoomOut"}, {0, nullptr, nullptr},
		{IDM_VIEW_SYNSCROLLV, "syncV", "syncV"}, {IDM_VIEW_SYNSCROLLH, "syncH", "syncH"}, {0, nullptr, nullptr},
		{IDM_VIEW_WRAP, "wrap", "wrap"}, {IDM_VIEW_ALL_CHARACTERS, "allChars", "allChars"}, {IDM_VIEW_INDENT_GUIDE, "indentGuide", "indentGuide"},
		{0, nullptr, nullptr},
		{IDM_LANG_USER_DLG, "udl", "udl"}, {IDM_VIEW_DOC_MAP, "docMap", "docMap"}, {IDM_VIEW_DOCLIST, "docList", "docList"},
		{IDM_VIEW_FUNC_LIST, "funcList", "funcList"}, {IDM_VIEW_FILEBROWSER, "fileBrowser", "fileBrowser"}, {0, nullptr, nullptr},
		{IDM_VIEW_MONITORING, "monitoring", "monitoring"}, {0, nullptr, nullptr},
		{IDM_MACRO_STARTRECORDINGMACRO, "startrecord", "startRecord"}, {IDM_MACRO_STOPRECORDINGMACRO, "stoprecord", "stopRecord"},
		{IDM_MACRO_PLAYBACKRECORDEDMACRO, "playrecord", "playRecord"}, {IDM_MACRO_RUNMULTIMACRODLG, "playrecord_m", "playRecord_m"},
		{IDM_MACRO_SAVECURRENTMACRO, "saverecord", "saveRecord"},
	};

	// ImageListSet.cpp IconList::changeFluentIconColor()
	QImage recolorFluent(QImage img, QColor from, QColor to, bool changeEverything)
	{
		img = img.convertToFormat(QImage::Format_ARGB32);
		for (int y = 0; y < img.height(); ++y)
		{
			auto* line = reinterpret_cast<QRgb*>(img.scanLine(y));
			for (int x = 0; x < img.width(); ++x)
			{
				const QRgb px = line[x];
				if (qAlpha(px) == 0)
					continue;
				const bool match = changeEverything || (std::abs(qRed(px) - from.red()) <= 3 && std::abs(qGreen(px) - from.green()) <= 3 &&
				                                         std::abs(qBlue(px) - from.blue()) <= 3);
				if (match)
					line[x] = qRgba(to.red(), to.green(), to.blue(), qAlpha(px));
			}
		}
		return img;
	}

	QColor fluentColor(int choice, int custom)
	{
		switch (static_cast<FluentColor>(choice))
		{
			case FluentColor::red: return QColor(0xE8, 0x11, 0x23);
			case FluentColor::green: return QColor(0x00, 0x8B, 0x00);
			case FluentColor::blue: return QColor(0x00, 0x78, 0xD4);
			case FluentColor::purple: return QColor(0xB1, 0x46, 0xC2);
			case FluentColor::cyan: return QColor(0x00, 0xB7, 0xC3);
			case FluentColor::olive: return QColor(0x49, 0x82, 0x05);
			case FluentColor::yellow: return QColor(0xFF, 0xB9, 0x00);
			case FluentColor::accent: return QGuiApplication::palette().color(QPalette::Accent);
			case FluentColor::custom: return custom ? toQColor(static_cast<COLORREF>(custom)) : QColor();
			default: return QColor();
		}
	}

	// NppDarkMode.cpp tones: background, softer, hot, pure, error, text, darker text,
	// disabled text, link, edge, hot edge, disabled edge (0xRRGGBB)
	std::array<QRgb, 12> darkToneColors(int tone, const QString& custom)
	{
		std::array<QRgb, 12> c = {0x202020, 0x383838, 0x454545, 0x202020, 0xB00000, 0xE0E0E0, 0xC0C0C0, 0x808080, 0xFFFF00, 0x646464, 0x9B9B9B, 0x484848};
		static const QRgb offsets[] = {0, 0x100000, 0x001000, 0x000020, 0x100020, 0x001020, 0x101000};
		if (tone >= 1 && tone <= 6)
		{
			const QRgb off = offsets[tone];
			for (int i : {0, 1, 2, 3, 10, 11})
				c[static_cast<size_t>(i)] += off;
			c[9] += 0x1C1C1C + off;
		}
		else if (tone == 7)
		{
			const QStringList parts = custom.split(QLatin1Char(','), Qt::SkipEmptyParts);
			for (int i = 0; i < 12 && i < parts.size(); ++i)
			{
				bool ok = false;
				const uint v = parts[i].trimmed().toUInt(&ok, 16);
				if (ok)
					c[static_cast<size_t>(i)] = v;
			}
		}
		return c;
	}

	QString eolName(int eol)
	{
		switch (eol)
		{
			case SC_EOL_CRLF: return QStringLiteral("Windows (CR LF)");
			case SC_EOL_CR: return QStringLiteral("Macintosh (CR)");
			default: return QStringLiteral("Unix (LF)");
		}
	}

	QString withThousands(sptr_t n)
	{
		return QLocale::system().toString(static_cast<qlonglong>(n));
	}
}

// ---------------------------------------------------------------------------
// construction
// ---------------------------------------------------------------------------
MainWindow::MainWindow()
{
	{
		QIcon icon;
		for (int size : {16, 24, 32, 48, 64, 128, 256})
			icon.addFile(QStringLiteral(":/linuxpad/linuxpad-plus-plus-%1.png").arg(size), QSize(size, size));
		setWindowIcon(icon);
	}
	setAcceptDrops(true);
	setDockOptions(QMainWindow::AnimatedDocks | QMainWindow::AllowTabbedDocks);

	// split view: main view and sub view side by side (or one above the other)
	_splitter = new QSplitter(Qt::Horizontal, this);
	_splitter->setChildrenCollapsible(false);
	_splitter->setHandleWidth(5);
	for (DocView& dv : _views)
	{
		dv.panel = new QWidget(_splitter);
		dv.layout = new QBoxLayout(QBoxLayout::TopToBottom, dv.panel);
		dv.layout->setContentsMargins(0, 0, 0, 0);
		dv.layout->setSpacing(0);
		dv.tabs = new NppTabBar(dv.panel);
		dv.editHolder = new QWidget(dv.panel);
		auto* holderLayout = new QVBoxLayout(dv.editHolder);
		holderLayout->setContentsMargins(0, 0, 0, 0);
		dv.edit = new EditView(dv.editHolder, true);
		holderLayout->addWidget(dv.edit);
		dv.layout->addWidget(dv.tabs);
		dv.layout->addWidget(dv.editHolder, 1);
		_splitter->addWidget(dv.panel);
	}
	_views[SUB_VIEW].panel->hide();
	if (!uiSettings().value(QStringLiteral("splitterSideBySide"), true).toBool())
		_splitter->setOrientation(Qt::Vertical);
	_splitter->handle(1)->installEventFilter(this); // right click: rotate
	_splitter->handle(1)->setToolTip(QStringLiteral("Right click to rotate"));
	_edit = _views[MAIN_VIEW].edit;
	_tabs = _views[MAIN_VIEW].tabs;
	_loader = new EditView(this, false);
	_loader->hide();
	// the views, with the Incremental Search bar under them (or above, FindHistory._incrementalSearchBarAtTop)
	auto* central = new QWidget(this);
	_centralLayout = new QVBoxLayout(central);
	_centralLayout->setContentsMargins(0, 0, 0, 0);
	_centralLayout->setSpacing(0);
	_centralLayout->addWidget(_splitter, 1);
	setCentralWidget(central);
	qApp->installEventFilter(this); // Alt / F10 with a hidden menu bar, document switcher, view focus

	buildTabBar(MAIN_VIEW);
	buildTabBar(SUB_VIEW);
	UdlManager::instance().load();
	buildMenus();
	buildToolbar();
	buildStatusBar();
	registerCommands();
	setupCheckable();
	applyShortcuts();
	rebuildUdlMenu();
	_savedMacros = Macros::load();
	rebuildMacroMenu();
	_savedCommands = Macros::loadCommands();
	rebuildRunMenu();
	applyScintillaKeys();

	for (int v = MAIN_VIEW; v <= SUB_VIEW; ++v)
	{
		EditView* e = _views[v].edit;
		connect(e, &ScintillaEditBase::savePointChanged, this, [this, v](bool dirty) { onSavePointChanged(v, dirty); });
		connect(e, &ScintillaEditBase::notify, this, [this, v](Scintilla::NotificationData* scn) { onSciNotify(scn, v); });
		connect(e, &ScintillaEditBase::macroRecord, this, [this, v](Scintilla::Message m, Scintilla::uptr_t w, Scintilla::sptr_t l)
		{
			if (v == _activeView)
				onMacroRecord(static_cast<unsigned int>(m), w, l);
		});
		connect(e, &ScintillaEditBase::uriDropped, this, [this, v](const QString& uri)
		{
			setActiveView(v);
			const QUrl url(uri);
			openFile(url.isLocalFile() ? url.toLocalFile() : uri);
		});
		connect(e, &EditView::contextMenuWanted, this, [this, v](const QPoint& pos)
		{
			setActiveView(v);
			// like Notepad++: WM_CONTEXTMENU to its window (plugins may see it first), which shows the menu
			if (_pluginsManager)
				_pluginsManager->editorContextMenu(v, pos);
			else
				showEditorContextMenu(pos);
		});
		connect(e, &EditView::hideLinesChanged, this, [this, v](bool isHide, sptr_t location)
		{
			// Buffer::setHideLineChanged(): every view showing the document, then the markers go once
			const sptr_t doc = _views[v].edit->send(SCI_GETDOCPOINTER);
			for (int u = MAIN_VIEW; u <= SUB_VIEW; ++u)
			{
				EditView* ue = _views[u].edit;
				if (ue->send(SCI_GETDOCPOINTER) != doc)
					continue;
				if (isHide)
					ue->hideMarkedLines(location, false);
				else
					ue->showHiddenLines(location, false, false);
			}
			if (!isHide)
				_views[v].edit->showHiddenLines(location, false, true);
		});
		connect(e, &EditView::folded, this, [this, v]
		{
			// Notepad_plus::fold(): the Document Map follows the folding of the active view
			if (v == _activeView && docMapShown() && !_docMap->isTemporary())
				_docMap->syncFolding();
		});
	}

	_watcher = new QFileSystemWatcher(this);
	connect(_watcher, &QFileSystemWatcher::fileChanged, this, &MainWindow::onFileChanged);

	_backupTimer = new QTimer(this);
	connect(_backupTimer, &QTimer::timeout, this, &MainWindow::backupSnapshot);

	_statusTimer = new QTimer(this);
	_statusTimer->setSingleShot(true);
	connect(_statusTimer, &QTimer::timeout, this, [this] { _statusMessage.clear(); updateStatusBar(); });

	_recent = uiSettings().value(QStringLiteral("recentFiles")).toStringList();
	rebuildRecentMenu();

	const QByteArray geom = uiSettings().value(QStringLiteral("windowGeometry")).toByteArray();
	if (!geom.isEmpty())
		restoreGeometry(geom);
	else
		resize(1100, 750);

	_pluginsManager = new PluginsManager(this);
	applyDarkMode();
	applyTheme(Npp::prefs().themeFile);
	applyPrefs();
}

MainWindow::~MainWindow()
{
	if (_pluginsManager)
		_pluginsManager->setDestroying();
	// QWidget hides its docks after the menu and document state has been destroyed.
	for (QDockWidget* dock : findChildren<QDockWidget*>())
		disconnect(dock, &QDockWidget::visibilityChanged, this, nullptr);

	for (Buffer* b : allBuffers())
	{
		if (b->doc)
			_loader->send(SCI_RELEASEDOCUMENT, 0, b->doc);
		delete b;
	}
}

void MainWindow::buildTabBar(int view)
{
	NppTabBar* tabs = _views[view].tabs;
	tabs->setDocumentMode(true);
	tabs->setMovable(true);
	tabs->setTabsClosable(Npp::prefs().tabCloseButton);
	tabs->setExpanding(false);
	tabs->setUsesScrollButtons(true);
	tabs->setElideMode(Qt::ElideNone);
	tabs->setContextMenuPolicy(Qt::CustomContextMenu);
	tabs->setFocusPolicy(Qt::NoFocus);
	tabs->setMouseTracking(true);
	tabs->setAcceptDrops(true);
	tabs->installEventFilter(this);
	tabs->setColorIndexProvider([this, view](int i) { Buffer* b = bufferAt(view, i); return b ? b->tabColor : -1; });

	connect(tabs, &QTabBar::currentChanged, this, [this, view](int index)
	{
		if (!_switching)
			activateBuffer(bufferAt(view, index), view);
	});
	connect(tabs, &QTabBar::tabCloseRequested, this, [this, view](int index)
	{
		if (Buffer* b = bufferAt(view, index))
			closeBuffer(b, true, view);
	});
	connect(tabs, &QTabBar::customContextMenuRequested, this, [this, view, tabs](const QPoint& pos)
	{
		const int index = tabs->tabAt(pos);
		if (index >= 0)
		{
			activateBuffer(bufferAt(view, index), view);
			showTabContextMenu(tabs->mapToGlobal(pos), index);
		}
	});
	connect(tabs, &QTabBar::tabMoved, this, [this](int, int) { refreshDocumentList(); });
	connect(tabs, &QTabBar::currentChanged, this, [this](int) { updateAllTabs(); });
	connect(tabs, &NppTabBar::tabDropped, this, [this, view](NppTabBar* source, int sourceIndex, int insertAt, bool copy)
	{
		const int from = viewOfTabBar(source);
		if (from >= 0 && from != view)
			transferBuffer(bufferAt(from, sourceIndex), from, view, insertAt, copy);
	});
}

void MainWindow::buildMenus()
{
	QMenuBar* mb = menuBar();
	std::vector<QMenu*> stack;

	for (const MenuTableEntry& e : kMenuTable)
	{
		const QString label = QString::fromUtf8(e.label);
		switch (e.kind)
		{
			case MenuEntryKind::PopupBegin:
			{
				QMenu* m = stack.empty() ? mb->addMenu(label) : stack.back()->addMenu(label);
				const QString key = QString::fromUtf8(e.l10nKey);
				_menuKeys.insert(m, key);
				_menuLabels.insert(m, label);
				if (stack.empty())
				{
					if (key == QStringLiteral("file"))
						_fileMenu = m;
					else if (key == QStringLiteral("language"))
						_languageMenu = m;
					else if (key == QStringLiteral("encoding"))
						_encodingMenu = m;
					else if (key == QStringLiteral("macro"))
						_macroMenu = m;
					else if (key == QStringLiteral("Window"))
						_windowMenu = m;
				}
				else if (key == QStringLiteral("edit-eolConversion"))
				{
					_eolMenu = m;
				}
				stack.push_back(m);
				break;
			}
			case MenuEntryKind::PopupEnd:
				if (!stack.empty())
					stack.pop_back();
				break;
			case MenuEntryKind::Separator:
				if (!stack.empty())
					stack.back()->addSeparator();
				break;
			case MenuEntryKind::Item:
			{
				if (stack.empty())
					break;
				if (e.id == IDM_WINDOW_MRU_FIRST)
					break; // placeholder for the recent windows list
				QAction* a = _actions.value(e.id);
				if (!a)
				{
					a = new QAction(label, this);
					a->setData(e.id);
					a->setIconVisibleInMenu(false);
					connect(a, &QAction::triggered, this, [this, id = e.id] { command(id); });
					_actions.insert(e.id, a);
					_rcLabels.insert(e.id, label);
					_commandTopMenu.insert(e.id, stack.front());
					_commandOrder.insert(e.id, static_cast<int>(_commandOrder.size()));
				}
				stack.back()->addAction(a);
				break;
			}
		}
	}

	// Linux names of Windows specific entries
	_rcLabels[IDM_FILE_OPEN_FOLDER] = QStringLiteral("File Manager");
	_rcLabels[IDM_FILE_OPEN_CMD] = QStringLiteral("Terminal");
	_rcLabels[IDM_FILE_DELETE] = QStringLiteral("Move to &Trash");

	// recent files section of the File menu, before Exit
	if (_fileMenu)
	{
		QAction* exitAction = _actions.value(IDM_FILE_EXIT);
		for (int id : {IDM_FILE_RESTORELASTCLOSEDFILE, IDM_OPEN_ALL_RECENT_FILE, IDM_CLEAN_RECENT_FILE_LIST})
		{
			auto* a = new QAction(this);
			a->setData(id);
			connect(a, &QAction::triggered, this, [this, id] { command(id); });
			_actions.insert(id, a);
			_fileMenu->insertAction(exitAction, a);
		}
		_rcLabels[IDM_FILE_RESTORELASTCLOSEDFILE] = QStringLiteral("Restore Recent Closed File");
		_rcLabels[IDM_OPEN_ALL_RECENT_FILE] = QStringLiteral("Open All Recent Files");
		_rcLabels[IDM_CLEAN_RECENT_FILE_LIST] = QStringLiteral("Empty Recent Files List");
		_fileMenu->insertSeparator(exitAction);
		_recentSeparator = _fileMenu->insertSeparator(exitAction);
	}

	// tab bar commands with no menu entry in the .rc
	for (int id : {IDM_PINTAB, IDM_VIEW_SWITCHTO_OTHER_VIEW})
	{
		if (_actions.contains(id))
			continue;
		auto* a = new QAction(this);
		a->setData(id);
		connect(a, &QAction::triggered, this, [this, id] { command(id); });
		_actions.insert(id, a);
	}
	_rcLabels[IDM_PINTAB] = QStringLiteral("Pin Tab");

	retranslateMenus();
}

void MainWindow::retranslateMenus()
{
	Localization& l10n = Localization::instance();
	for (auto it = _menuKeys.constBegin(); it != _menuKeys.constEnd(); ++it)
		it.key()->setTitle(l10n.menu(it.value(), _menuLabels.value(it.key())));

	for (auto it = _actions.constBegin(); it != _actions.constEnd(); ++it)
	{
		const int id = it.key();
		QString text = _rcLabels.value(id);
		if (id != IDM_FILE_OPEN_FOLDER && id != IDM_FILE_OPEN_CMD && id != IDM_FILE_DELETE)
			text = l10n.command(id, text);
		if (id == IDM_ABOUT)
			text.replace(QStringLiteral("Notepad++"), Npp::appName()); // "About Notepad++" in every language
		if (isDisplayOnlyShortcut(id))
		{
			for (const ShortcutTableEntry& s : kShortcutTable)
			{
				if (s.id == id)
				{
					text += QLatin1Char('\t') + QKeySequence(QKeyCombination(s.mods, s.key)).toString(QKeySequence::NativeText);
					break;
				}
			}
		}
		it.value()->setText(text);
	}
	if (QAction* a = _actions.value(IDM_FILE_OPEN_POWERSHELL))
		a->setVisible(!QStandardPaths::findExecutable(QStringLiteral("pwsh")).isEmpty());
	rebuildRecentMenu();
	if (_pluginsManager && _pluginsManager->isReady())
		_pluginsManager->notify(NPPN_NATIVELANGCHANGED, 0);
}

QString MainWindow::commandName(int id) const
{
	QAction* a = _actions.value(id);
	if (!a)
		return {};
	QString t = a->text();
	const int tab = t.indexOf(QLatin1Char('\t'));
	if (tab >= 0)
		t.truncate(tab);
	return Npp::stripAmpersand(t);
}

QString MainWindow::commandCategory(int id) const
{
	if (QMenu* m = _commandTopMenu.value(id))
		return Npp::stripAmpersand(m->title());
	return _fileMenu && (id == IDM_FILE_RESTORELASTCLOSEDFILE || id == IDM_OPEN_ALL_RECENT_FILE || id == IDM_CLEAN_RECENT_FILE_LIST)
		? Npp::stripAmpersand(_fileMenu->title()) : QStringLiteral("Tab");
}

void MainWindow::applyShortcuts()
{
	QHash<int, QList<QKeySequence>> keys;
	for (const ShortcutTableEntry& s : kShortcutTable)
	{
		if (isDisplayOnlyShortcut(s.id))
			continue;
		keys[s.id].append(QKeySequence(QKeyCombination(s.mods, s.key)));
	}
	// user overrides from the Shortcut Mapper
	QSettings& st = uiSettings();
	st.beginGroup(QStringLiteral("Shortcuts"));
	for (const QString& k : st.childKeys())
	{
		const int id = k.toInt();
		const QString seq = st.value(k).toString();
		if (seq.isEmpty())
			keys[id].clear();
		else
			keys[id] = {QKeySequence(seq, QKeySequence::PortableText)};
	}
	st.endGroup();

	for (auto it = _actions.constBegin(); it != _actions.constEnd(); ++it)
	{
		if (isDisplayOnlyShortcut(it.key()))
			continue;
		it.value()->setShortcuts(keys.value(it.key()));
		it.value()->setShortcutContext(Qt::WindowShortcut);
		addAction(it.value()); // keep shortcuts active when the menu bar is hidden
	}
}

void MainWindow::buildToolbar()
{
	_toolbar = addToolBar(QStringLiteral("Toolbar"));
	_toolbar->setObjectName(QStringLiteral("mainToolbar"));
	_toolbar->setMovable(false);
	_toolbar->setFloatable(false);
	for (const ToolbarButton& b : kToolbar)
	{
		if (!b.icon)
		{
			_toolbar->addSeparator();
			continue;
		}
		if (QAction* a = _actions.value(b.id))
			_toolbar->addAction(a);
	}
	applyToolbarIcons();
}

void MainWindow::buildStatusBar()
{
	QStatusBar* sb = statusBar();
	auto makeLabel = [sb](int minWidth, bool stretch)
	{
		auto* l = new QLabel(sb);
		l->setMinimumWidth(minWidth);
		l->setContentsMargins(6, 0, 6, 0);
		if (stretch)
			sb->addWidget(l, 1);
		else
			sb->addPermanentWidget(l);
		return l;
	};
	_sbLang = makeLabel(120, true);
	_sbDocInfo = makeLabel(220, false);
	_sbPos = makeLabel(260, false);
	_sbEol = makeLabel(110, false);
	_sbEnc = makeLabel(100, false);
	_sbIns = makeLabel(40, false);
	for (QLabel* l : {_sbLang, _sbEol, _sbEnc, _sbIns})
		l->installEventFilter(this);
	_sbLang->setToolTip(QStringLiteral("Click to change the language"));
	_sbEol->setToolTip(QStringLiteral("Click to convert line endings"));
	_sbEnc->setToolTip(QStringLiteral("Click to change the encoding"));
}

void MainWindow::on(int id, std::function<void()> fn)
{
	_handlers.insert(id, std::move(fn));
}

void MainWindow::setupCheckable()
{
	static const int checkable[] = {
		IDM_VIEW_ALWAYSONTOP, IDM_VIEW_FULLSCREENTOGGLE, IDM_VIEW_POSTIT, IDM_VIEW_TAB_SPACE, IDM_VIEW_EOL,
		IDM_VIEW_ALL_CHARACTERS, IDM_VIEW_INDENT_GUIDE, IDM_VIEW_WRAP_SYMBOL, IDM_VIEW_WRAP, IDM_VIEW_MONITORING,
		IDM_VIEW_FILEBROWSER, IDM_VIEW_DOCLIST, IDM_EDIT_TOGGLEREADONLY, IDM_PINTAB, IDM_VIEW_DISTRACTIONFREE, IDM_VIEW_NPC,
		IDM_VIEW_NPC_CCUNIEOL, IDM_VIEW_SYNSCROLLV, IDM_VIEW_SYNSCROLLH, IDM_VIEW_ZOOM_SYNC, IDM_VIEW_DOC_MAP, IDM_VIEW_FUNC_LIST,
		IDM_EDIT_CHAR_PANEL, IDM_EDIT_CLIPBOARDHISTORY_PANEL, IDM_VIEW_PROJECT_PANEL_1, IDM_VIEW_PROJECT_PANEL_2, IDM_VIEW_PROJECT_PANEL_3};
	for (int id : checkable)
	{
		if (QAction* a = _actions.value(id))
			a->setCheckable(true);
	}

	_langGroup = new QActionGroup(this);
	_langGroup->setExclusive(true);
	_encGroup = new QActionGroup(this);
	_encGroup->setExclusive(true);
	_eolGroup = new QActionGroup(this);
	_eolGroup->setExclusive(true);

	for (auto it = _actions.constBegin(); it != _actions.constEnd(); ++it)
	{
		const int id = it.key();
		QAction* a = it.value();
		if (StyleManager::instance().langFromMenuId(id) != L_TEXT || id == IDM_LANG_TEXT)
		{
			a->setCheckable(true);
			_langGroup->addAction(a);
		}
		const bool encId = id == IDM_FORMAT_ANSI || id == IDM_FORMAT_AS_UTF_8 || id == IDM_FORMAT_UTF_8 || id == IDM_FORMAT_UTF_16BE ||
		                   id == IDM_FORMAT_UTF_16LE || (id >= IDM_FORMAT_ENCODE && id <= IDM_FORMAT_ENCODE_END);
		if (encId)
		{
			a->setCheckable(true);
			_encGroup->addAction(a);
		}
		if (id == IDM_FORMAT_TODOS || id == IDM_FORMAT_TOUNIX || id == IDM_FORMAT_TOMAC)
		{
			a->setCheckable(true);
			_eolGroup->addAction(a);
		}
	}

	// commands without implementation are greyed out
	for (auto it = _actions.constBegin(); it != _actions.constEnd(); ++it)
	{
		if (!_handlers.contains(it.key()))
			it.value()->setEnabled(false);
	}
}

// ---------------------------------------------------------------------------
// buffers and tabs
// ---------------------------------------------------------------------------
std::vector<Buffer*> MainWindow::viewBuffers(int view) const
{
	std::vector<Buffer*> v;
	NppTabBar* tabs = _views[view].tabs;
	v.reserve(static_cast<size_t>(tabs->count()));
	for (int i = 0; i < tabs->count(); ++i)
		v.push_back(bufferAt(view, i));
	return v;
}

std::vector<Buffer*> MainWindow::allBuffers() const
{
	std::vector<Buffer*> v = viewBuffers(MAIN_VIEW);
	for (Buffer* b : viewBuffers(SUB_VIEW))
	{
		if (std::find(v.begin(), v.end(), b) == v.end())
			v.push_back(b);
	}
	return v;
}

Buffer* MainWindow::bufferAt(int tabIndex) const
{
	return bufferAt(_activeView, tabIndex);
}

Buffer* MainWindow::bufferAt(int view, int tabIndex) const
{
	NppTabBar* tabs = _views[view].tabs;
	if (tabIndex < 0 || tabIndex >= tabs->count())
		return nullptr;
	return reinterpret_cast<Buffer*>(tabs->tabData(tabIndex).value<quintptr>());
}

int MainWindow::indexOf(const Buffer* buf) const
{
	return indexOf(_activeView, buf);
}

int MainWindow::indexOf(int view, const Buffer* buf) const
{
	NppTabBar* tabs = _views[view].tabs;
	for (int i = 0; i < tabs->count(); ++i)
	{
		if (bufferAt(view, i) == buf)
			return i;
	}
	return -1;
}

bool MainWindow::viewVisible(int view) const
{
	return !_views[view].panel->isHidden() && _views[view].tabs->count() > 0;
}

int MainWindow::viewOfTabBar(const QObject* obj) const
{
	for (int v = MAIN_VIEW; v <= SUB_VIEW; ++v)
	{
		if (obj == _views[v].tabs)
			return v;
	}
	return -1;
}

int MainWindow::viewOfWidget(const QObject* obj) const
{
	for (const QObject* o = obj; o; o = o->parent())
	{
		for (int v = MAIN_VIEW; v <= SUB_VIEW; ++v)
		{
			if (o == _views[v].panel)
				return v;
		}
		if (o == this)
			break;
	}
	return -1;
}

QIcon MainWindow::tabIconOf(const Buffer* buf) const
{
	for (int v : {_activeView, 1 - _activeView})
	{
		const int i = indexOf(v, buf);
		if (i >= 0)
			return _views[v].tabs->tabIcon(i);
	}
	return {};
}

Buffer* MainWindow::findBuffer(const QString& path) const
{
	const QString canon = QFileInfo(path).absoluteFilePath();
	for (Buffer* b : allBuffers())
	{
		if (!b->isUntitled() && QFileInfo(b->path).absoluteFilePath() == canon)
			return b;
	}
	return nullptr;
}

QString MainWindow::nextUntitledName() const
{
	QSet<int> used;
	for (Buffer* b : allBuffers())
	{
		if (b->isUntitled() && b->untitledName.startsWith(QStringLiteral("new ")))
			used.insert(b->untitledName.mid(4).toInt());
	}
	int n = 1;
	while (used.contains(n))
		++n;
	return QStringLiteral("new %1").arg(n);
}

bool MainWindow::isCleanEmptyUntitled(Buffer* buf) const
{
	if (!buf || !buf->isUntitled() || buf->dirty)
		return false;
	bool empty = true;
	withDocument(buf, [&](EditView* v) { empty = v->ex(SCI_GETLENGTH) == 0; });
	return empty;
}

void MainWindow::withDocument(Buffer* buf, const std::function<void(EditView*)>& fn) const
{
	if (buf == _current)
	{
		fn(_edit);
		return;
	}
	_loader->send(SCI_SETDOCPOINTER, 0, buf->doc);
	fn(_loader);
	_loader->send(SCI_SETDOCPOINTER, 0, 0);
}

QByteArray MainWindow::bufferText(Buffer* buf) const
{
	QByteArray text;
	withDocument(buf, [&](EditView* v) { text = v->wholeText(); });
	return text;
}

void MainWindow::loadIntoDocument(Buffer* buf, const QByteArray& utf8, bool resetUndo)
{
	const bool isCurrent = (buf == _current);
	EditView* v = isCurrent ? _edit : _loader;
	if (!isCurrent)
		_loader->send(SCI_SETDOCPOINTER, 0, buf->doc);

	v->ex(SCI_SETREADONLY, false);
	if (resetUndo)
		v->ex(SCI_SETUNDOCOLLECTION, false);
	v->ex(SCI_CLEARALL);
	v->ex(SCI_APPENDTEXT, utf8.size(), reinterpret_cast<sptr_t>(utf8.constData()));
	if (resetUndo)
	{
		v->ex(SCI_SETUNDOCOLLECTION, true);
		v->ex(SCI_EMPTYUNDOBUFFER);
		v->ex(SCI_SETSAVEPOINT);
		v->ex(SCI_SETCHANGEHISTORY, EditView::changeHistoryFlags());
	}
	v->ex(SCI_SETEOLMODE, buf->eol);
	v->ex(SCI_SETREADONLY, buf->isReadOnly());

	if (!isCurrent)
		_loader->send(SCI_SETDOCPOINTER, 0, 0);
}

Buffer* MainWindow::newDocument(bool activate)
{
	auto* buf = new Buffer;
	buf->doc = _loader->send(SCI_CREATEDOCUMENT, 0, SC_DOCUMENTOPTION_DEFAULT);
	buf->untitledName = nextUntitledName();
	const NppPrefs& p = Npp::prefs();
	buf->eol = p.newDocEol;
	buf->unicode = static_cast<UniMode>(p.newDocUnicode);
	if (buf->unicode == UniMode::uni8Bit && p.newDocCodepage > 0 && FileIO::isCodepageSupported(p.newDocCodepage))
		buf->codepage = p.newDocCodepage;
	buf->lang = static_cast<LangType>(p.newDocLang);
	loadIntoDocument(buf, QByteArray(), true);

	const int view = (_targetView >= 0) ? _targetView : _activeView;
	addTab(view, buf);
	if (activate)
		activateBuffer(buf, view);
	refreshDocumentList();
	return buf;
}

void MainWindow::addTab(int view, Buffer* buf, int insertAt)
{
	NppTabBar* tabs = _views[view].tabs;
	_switching = true;
	const int index = (insertAt >= 0 && insertAt < tabs->count()) ? tabs->insertTab(insertAt, buf->displayName()) : tabs->addTab(buf->displayName());
	tabs->setTabData(index, QVariant::fromValue(reinterpret_cast<quintptr>(buf)));
	_switching = false;
	updateTabLabel(view, index);
}

Buffer* MainWindow::openFile(const QString& rawPath, bool activate, const OpenOptions& opts)
{
	if (rawPath.isEmpty())
		return nullptr;
	const QString path = QFileInfo(rawPath).absoluteFilePath();

	if (Buffer* existing = findBuffer(path))
	{
		if (activate)
			activateBuffer(existing);
		if (opts.line >= 0)
			_edit->gotoLine(opts.line);
		return existing;
	}

	const QFileInfo fi(path);
	if (fi.isDir())
	{
		toggleWorkspace(path);
		return nullptr;
	}
	const NppPrefs& prefs = Npp::prefs();
	if (!prefs.sessionFileExt.isEmpty() && fi.suffix().compare(prefs.sessionFileExt.section(QLatin1Char('.'), -1), Qt::CaseInsensitive) == 0 && fi.isFile())
	{
		// Preferences > MISC. > Session file ext.: opening such a file loads the session
		if (!readSession(path))
			QMessageBox::warning(this, QStringLiteral("Load Session"), QStringLiteral("Cannot load the session \"%1\".").arg(path));
		return nullptr;
	}
	if (fi.exists() && fi.size() >= (qint64(2) * 1024 * 1024 * 1024) && !prefs.largeFileSuppress2GBWarning)
	{
		const auto answer = QMessageBox::warning(this, QStringLiteral("File size problem"),
			QStringLiteral("\"%1\" is %2 GB or bigger. Opening it needs a lot of memory.\nOpen it anyway?").arg(path).arg(2),
			QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
		if (answer != QMessageBox::Yes)
			return nullptr;
	}
	if (!fi.exists())
	{
		const auto answer = QMessageBox::question(this, QStringLiteral("Create new file"),
			QStringLiteral("\"%1\" doesn't exist. Create it?").arg(path));
		if (answer != QMessageBox::Yes)
			return nullptr;
		QFile f(path);
		if (!f.open(QIODevice::WriteOnly))
		{
			QMessageBox::warning(this, QStringLiteral("Create new file"), QStringLiteral("Cannot create the file \"%1\".").arg(path));
			return nullptr;
		}
		f.close();
	}

	_pluginsManager->notify(NPPN_FILEBEFORELOAD, 0);
	FileIO::Loaded loaded = FileIO::load(path);
	if (!loaded.ok)
	{
		_pluginsManager->notify(NPPN_FILELOADFAILED, 0);
		QMessageBox::warning(this, QStringLiteral("Open"), QStringLiteral("Can not open file \"%1\".\n%2").arg(path, loaded.error));
		return nullptr;
	}

	// replace a clean empty "new 1" document like Notepad++ does
	const int view = (_targetView >= 0) ? _targetView : _activeView;
	Buffer* toReplace = (_views[view].tabs->count() == 1 && isCleanEmptyUntitled(bufferAt(view, 0)) && indexOf(1 - view, bufferAt(view, 0)) < 0)
		? bufferAt(view, 0) : nullptr;

	auto* buf = new Buffer;
	buf->doc = _loader->send(SCI_CREATEDOCUMENT, 0, SC_DOCUMENTOPTION_DEFAULT);
	buf->path = path;
	buf->unicode = loaded.unicode;
	buf->codepage = loaded.codepage;
	buf->eol = loaded.eol >= 0 ? loaded.eol : Npp::prefs().newDocEol;
	buf->lastModified = fi.lastModified();
	buf->fileReadOnly = !QFileInfo(path).isWritable();
	buf->userReadOnly = opts.readOnly;
	buf->largeFile = isLargeFileSize(fi.size());
	_pluginsManager->notifyBuffer(NPPN_FILEBEFOREOPEN, buf);

	if (opts.lang != L_EXTERNAL)
		buf->lang = opts.lang;
	else
	{
		buf->lang = langOfFile(path, &buf->udlName);
		if (buf->lang == L_TEXT)
			buf->lang = StyleManager::langFromContent(loaded.head);
	}

	loadIntoDocument(buf, loaded.utf8, true);

	addTab(view, buf);
	watch(buf);
	addRecent(path);
	_closedFiles.removeAll(path);
	_pluginsManager->notifyBuffer(NPPN_FILEOPENED, buf);

	if (activate)
	{
		activateBuffer(buf, view);
		if (opts.line >= 0)
		{
			_edit->gotoLine(opts.line);
			if (opts.column >= 0)
				_edit->ex(SCI_GOTOPOS, _edit->ex(SCI_FINDCOLUMN, opts.line, opts.column));
		}
	}
	if (toReplace)
		closeBuffer(toReplace, false, view);
	refreshDocumentList();
	return buf;
}

void MainWindow::saveCurrentPosition()
{
	saveViewPosition(_activeView);
}

void MainWindow::saveViewPosition(int view)
{
	Buffer* b = viewCurrent(view);
	if (!b)
		return;
	EditView* e = _views[view].edit;
	Buffer::ViewPos& pos = b->pos[view];
	pos.firstVisibleLine = e->ex(SCI_DOCLINEFROMVISIBLE, e->ex(SCI_GETFIRSTVISIBLELINE));
	pos.anchor = e->ex(SCI_GETANCHOR);
	pos.caret = e->ex(SCI_GETCURRENTPOS);
	pos.xOffset = e->ex(SCI_GETXOFFSET);
}

void MainWindow::restorePosition(Buffer* buf)
{
	const Buffer::ViewPos& pos = buf->pos[_activeView];
	_edit->ex(SCI_SETSEL, pos.anchor, pos.caret);
	_edit->ex(SCI_SETFIRSTVISIBLELINE, _edit->ex(SCI_VISIBLEFROMDOCLINE, pos.firstVisibleLine));
	_edit->ex(SCI_SETXOFFSET, pos.xOffset);
	_edit->ex(SCI_CHOOSECARETX);
}

void MainWindow::setActiveView(int view)
{
	// Notepad_plus::switchEditViewTo()
	if (view == _activeView || view < MAIN_VIEW || view > SUB_VIEW || _views[view].tabs->count() == 0)
		return;
	hideTabPeek();
	saveCurrentPosition();
	_views[_activeView].current = _current;
	_activeView = view;
	_edit = _views[view].edit;
	_tabs = _views[view].tabs;
	_current = _views[view].current;
	updateTabBarFocus();
	if (!_current)
	{
		if (Buffer* b = bufferAt(view, std::max(0, _tabs->currentIndex())))
			activateBuffer(b, view);
		return;
	}
	if (!_edit->hasFocus())
		_edit->setFocus();
	updateTitle();
	updateStatusBar();
	updateMenuState();
	refreshDocumentList();
	refreshPanels();
	_pluginsManager->notifyBuffer(NPPN_BUFFERACTIVATED, _current);
}

void MainWindow::activateBuffer(Buffer* buf, int view)
{
	if (!buf)
		return;
	if (view < 0)
		view = (indexOf(_activeView, buf) >= 0) ? _activeView : (indexOf(1 - _activeView, buf) >= 0 ? 1 - _activeView : _activeView);
	if (view != _activeView)
	{
		if (_views[view].tabs->count() == 0)
			return;
		// switch to the other view; buf is loaded below if that view shows another document
		hideTabPeek();
		saveCurrentPosition();
		_views[_activeView].current = _current;
		_activeView = view;
		_edit = _views[view].edit;
		_tabs = _views[view].tabs;
		_current = _views[view].current;
		updateTabBarFocus();
	}
	const int index = indexOf(buf);
	if (index < 0)
		return;

	if (buf != _current)
	{
		saveCurrentPosition();
		_edit->send(SCI_SETDOCPOINTER, 0, buf->doc);
		_current = buf;
		_edit->setDocPath(buf->path);
		if (_edit->isLargeFile() != buf->largeFile)
		{
			_edit->setLargeFile(buf->largeFile);
			_edit->applyPrefs();
		}
		_edit->ex(SCI_SETCHANGEHISTORY, EditView::changeHistoryFlags());
		_edit->defineDocType(buf->lang, buf->udlName);
		_edit->ex(SCI_SETREADONLY, buf->isReadOnly());
		_edit->restoreHiddenLines(); // ScintillaEditView::activateBuffer(): before the position, kept in document lines
		restorePosition(buf);
	}
	_views[_activeView].current = buf;
	if (_tabs->currentIndex() != index)
	{
		_switching = true;
		_tabs->setCurrentIndex(index);
		_switching = false;
	}
	buf->dirty = buf->forcedDirty || _edit->ex(SCI_GETMODIFY);
	_mru.erase(std::remove(_mru.begin(), _mru.end(), buf), _mru.end());
	_mru.insert(_mru.begin(), buf);
	updateAllTabs();
	updateTitle();
	updateStatusBar();
	updateMenuState();
	refreshDocumentList();
	refreshPanels();
	_edit->setFocus();
	_edit->doSmartHighlight();
	_edit->highlightUrls();
	smartHighlightOtherView();
	if (Npp::prefs().fileAutoDetectMode == AUTODETECT_CURRENT && !buf->isUntitled() && !_checkingFiles && !_closing)
		QTimer::singleShot(0, this, [this] { checkModifiedFiles(false); });
	_pluginsManager->notifyBuffer(NPPN_BUFFERACTIVATED, buf);
}

void MainWindow::showView(int view)
{
	DocView& dv = _views[view];
	if (dv.panel->isHidden())
	{
		dv.panel->show();
		const int total = (_splitter->orientation() == Qt::Horizontal) ? _splitter->width() : _splitter->height();
		_splitter->setSizes({total / 2, total - total / 2});
	}
	checkSyncState();
	updateMenuState();
}

void MainWindow::hideView(int view)
{
	// Notepad_plus::hideView(): a view without documents disappears when the other has some
	if (view == _activeView)
		setActiveView(1 - view);
	DocView& dv = _views[view];
	dv.current = nullptr;
	dv.edit->send(SCI_SETDOCPOINTER, 0, 0);
	dv.panel->hide();
	checkSyncState();
	updateMenuState();
}

void MainWindow::removeTab(Buffer* buf, int view)
{
	NppTabBar* tabs = _views[view].tabs;
	const int index = indexOf(view, buf);
	if (index < 0)
		return;
	if (tabs->count() == 1 && _views[1 - view].tabs->count() == 0)
	{
		// last document of the last view: a new document takes its place, in the main view
		if (Npp::prefs().tabExitOnCloseLast && !_closing)
			QTimer::singleShot(0, this, [this] { close(); });
		const int saved = _targetView;
		_targetView = MAIN_VIEW;
		if (view == SUB_VIEW)
			_views[MAIN_VIEW].panel->show();
		newDocument(view == MAIN_VIEW && view == _activeView);
		_targetView = saved;
	}
	if (tabs->count() == 1 && _views[1 - view].tabs->count() > 0)
	{
		// the other view has documents: this one goes away
		if (view == _activeView)
			setActiveView(1 - view);
		_switching = true;
		tabs->removeTab(index);
		_switching = false;
		hideView(view);
		refreshDocumentList();
		return;
	}
	if (buf == viewCurrent(view))
	{
		const int next = (index + 1 < tabs->count()) ? index + 1 : index - 1;
		if (view == _activeView)
		{
			activateBuffer(bufferAt(view, next), view);
		}
		else
		{
			// inactive view: show the next document without switching views
			Buffer* n = bufferAt(view, next);
			saveViewPosition(view);
			_views[view].current = n;
			EditView* e = _views[view].edit;
			e->send(SCI_SETDOCPOINTER, 0, n->doc);
			e->setDocPath(n->path);
			e->setLargeFile(n->largeFile);
			e->defineDocType(n->lang, n->udlName);
			const Buffer::ViewPos& pos = n->pos[view];
			e->ex(SCI_SETSEL, pos.anchor, pos.caret);
			e->ex(SCI_SETFIRSTVISIBLELINE, e->ex(SCI_VISIBLEFROMDOCLINE, pos.firstVisibleLine));
			_switching = true;
			tabs->setCurrentIndex(next);
			_switching = false;
		}
	}
	_switching = true;
	tabs->removeTab(indexOf(view, buf));
	_switching = false;
	if (view == _activeView && _current)
		activateBuffer(_current, view);
	else
		updateAllTabs();
}

bool MainWindow::closeBuffer(Buffer* buf, bool askToSave, int view)
{
	if (!buf)
		return false;
	if (view < 0)
		view = (indexOf(_activeView, buf) >= 0) ? _activeView : 1 - _activeView;
	if (indexOf(view, buf) < 0)
		return false;

	// a document cloned in the other view only loses this tab (Notepad_plus::doClose)
	if (indexOf(1 - view, buf) >= 0)
	{
		removeTab(buf, view);
		refreshDocumentList();
		updateMenuState();
		return true;
	}

	if (askToSave && buf->dirty)
	{
		activateBuffer(buf, view);
		const auto answer = QMessageBox::question(this, QStringLiteral("Save"),
			QStringLiteral("Save file \"%1\" ?").arg(buf->fullName()),
			QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Yes);
		if (answer == QMessageBox::Cancel)
			return false;
		if (answer == QMessageBox::Yes && !saveBuffer(buf))
			return false;
	}

	_pluginsManager->notifyBuffer(NPPN_FILEBEFORECLOSE, buf);
	if (!buf->isUntitled())
	{
		_closedFiles.removeAll(buf->path);
		_closedFiles.prepend(buf->path);
		addRecent(buf->path);
	}
	unwatch(buf);
	removeBackup(buf);
	_mru.erase(std::remove(_mru.begin(), _mru.end(), buf), _mru.end());
	removeTab(buf, view);
	if (buf == _current)
		_current = nullptr;
	for (DocView& dv : _views)
	{
		if (dv.current == buf)
			dv.current = nullptr;
	}
	_loader->send(SCI_RELEASEDOCUMENT, 0, buf->doc);
	delete buf;
	_pluginsManager->notifyBuffer(NPPN_FILECLOSED, buf);
	refreshDocumentList();
	updateMenuState();
	return true;
}

bool MainWindow::saveBuffer(Buffer* buf, bool saveAs, bool saveCopy, const QString& targetPath)
{
	if (!buf)
		return false;

	QString target = targetPath.isEmpty() ? buf->path : QFileInfo(targetPath).absoluteFilePath();
	if (targetPath.isEmpty() && (buf->isUntitled() || saveAs || saveCopy))
	{
		QString dir = buf->isUntitled() ? openSaveDir() : QFileInfo(buf->path).absolutePath();
		const QString suggested = dir + QLatin1Char('/') + buf->displayName();
		QString filter = QStringLiteral("All types (*)");
		const LangDef* ld = StyleManager::instance().langDef(buf->lang);
		if (ld && !ld->ext.isEmpty() && buf->lang != L_TEXT)
		{
			QStringList pats;
			for (const QString& e : ld->ext.split(QLatin1Char(' '), Qt::SkipEmptyParts))
				pats << QStringLiteral("*.") + e;
			filter = QStringLiteral("%1 (%2);;%3").arg(QString::fromUtf8(StyleManager::langInfo(buf->lang).longName), pats.join(QLatin1Char(' ')), filter);
		}
		target = QFileDialog::getSaveFileName(this, saveCopy ? QStringLiteral("Save a Copy As") : QStringLiteral("Save As"), suggested, filter);
		if (target.isEmpty())
			return false;
		rememberOpenSaveDir(target);
		if (!saveCopy)
		{
			Buffer* other = findBuffer(target);
			if (other && other != buf)
			{
				QMessageBox::warning(this, QStringLiteral("Save"), QStringLiteral("The file \"%1\" is already opened in LinuxPad++.").arg(target));
				return false;
			}
		}
	}

	// A FILEBEFORESAVE listener may change the document or its save format.
	_pluginsManager->notifyBeforeSave(buf, saveCopy ? QString() : target);
	const QByteArray utf8 = bufferText(buf);
	bool lossy = false;
	const QByteArray data = FileIO::encode(utf8, buf->unicode, buf->codepage, &lossy);
	if (lossy)
	{
		const auto answer = QMessageBox::warning(this, QStringLiteral("Save"),
			QStringLiteral("Some characters of \"%1\" cannot be represented in %2 and will be replaced by \"?\".\nSave anyway?")
				.arg(buf->displayName(), FileIO::encodingName(buf->unicode, buf->codepage)),
			QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
		if (answer != QMessageBox::Yes)
			return false;
	}

	unwatch(buf);
	if (!saveCopy)
		backupOnSave(target);
	QFile f(target);
	bool ok = f.open(QIODevice::WriteOnly | QIODevice::Truncate);
	if (ok)
		ok = (f.write(data) == data.size()) && f.flush();
	const QString err = f.errorString();
	f.close();
	if (!ok)
	{
		watch(buf);
		const auto answer = QMessageBox::warning(this, QStringLiteral("Save failed"),
			QStringLiteral("Cannot save file \"%1\".\n%2\n\nDo you want to save it under another name?").arg(target, err),
			QMessageBox::Yes | QMessageBox::No);
		if (answer == QMessageBox::Yes)
			return saveBuffer(buf, true, saveCopy);
		return false;
	}

	if (saveCopy)
	{
		watch(buf);
		return true;
	}

	const bool wasUntitled = buf->isUntitled();
	const bool pathChanged = (target != buf->path);
	buf->path = target;
	buf->dirty = false;
	buf->forcedDirty = false;
	buf->missing = false;
	buf->lastModified = QFileInfo(target).lastModified();
	buf->fileReadOnly = !QFileInfo(target).isWritable();
	withDocument(buf, [](EditView* v) { v->ex(SCI_SETSAVEPOINT); });
	if ((wasUntitled || pathChanged) && buf->lang == L_TEXT)
	{
		buf->lang = langOfFile(target, &buf->udlName);
		if (buf == _current)
			_edit->defineDocType(buf->lang, buf->udlName);
		refreshOtherViews(buf);
	}
	watch(buf);
	removeBackup(buf);
	addRecent(target);
	updateBufferTabs(buf);
	if (buf == _current)
	{
		updateTitle();
		updateStatusBar();
		updateMenuState();
	}
	refreshDocumentList();
	if (buf == _current)
		refreshPanels(true); // NppIO.cpp: the Function List is parsed again after saving
	_pluginsManager->notifyBuffer(NPPN_FILESAVED, buf);
	return true;
}

void MainWindow::reloadBuffer(Buffer* buf, FileIO::Force force, int codepage, bool keepPosition)
{
	if (!buf || buf->isUntitled())
		return;
	FileIO::Loaded loaded = FileIO::load(buf->path, force, codepage);
	if (!loaded.ok)
	{
		QMessageBox::warning(this, QStringLiteral("Reload"), QStringLiteral("Can not reload file \"%1\".\n%2").arg(buf->path, loaded.error));
		return;
	}
	if (buf == _current && keepPosition)
		saveCurrentPosition();
	buf->unicode = loaded.unicode;
	buf->codepage = loaded.codepage;
	if (loaded.eol >= 0)
		buf->eol = loaded.eol;
	buf->lastModified = QFileInfo(buf->path).lastModified();
	buf->fileReadOnly = !QFileInfo(buf->path).isWritable();
	buf->dirty = false;
	buf->forcedDirty = false;
	buf->missing = false;
	loadIntoDocument(buf, loaded.utf8, true);
	removeBackup(buf);
	if (buf == _current)
	{
		_edit->defineDocType(buf->lang, buf->udlName);
		if (keepPosition)
			restorePosition(buf);
		updateStatusBar();
		updateMenuState();
		updateTitle();
	}
	refreshOtherViews(buf);
	updateBufferTabs(buf);
	if (buf == _current)
		refreshPanels(true);
}

// ---------------------------------------------------------------------------
// state updates
// ---------------------------------------------------------------------------
void MainWindow::updateTabLabel(int view, int index)
{
	NppTabBar* tabs = _views[view].tabs;
	Buffer* b = bufferAt(view, index);
	if (!b)
		return;
	QString iconName = QStringLiteral("saved");
	if (b->monitoring)
		iconName = QStringLiteral("monitoring");
	else if (b->isReadOnly())
		iconName = QStringLiteral("readonly");
	else if (b->dirty)
		iconName = QStringLiteral("unsaved");
	const NppPrefs& p = Npp::prefs();
	const bool dark = p.isDark();
	QString iconPath;
	if (p.tabAltIcons && iconName != QStringLiteral("monitoring"))
		iconPath = QStringLiteral(":/icons/standard/tabbar/%1_alt.ico").arg(iconName);
	else if (dark && iconName != QStringLiteral("saved"))
		iconPath = QStringLiteral(":/icons/dark/tabbar/%1.ico").arg(iconName);
	if (iconPath.isEmpty() || !QFileInfo::exists(iconPath))
		iconPath = QStringLiteral(":/icons/standard/tabbar/%1.ico").arg(iconName);
	tabs->setTabIcon(index, QIcon(iconPath));
	const bool pinMarkInText = b->pinned && !p.tabPinButton;
	tabs->setTabText(index, (pinMarkInText ? QStringLiteral("📌 ") : QString()) + tabLabel(b).replace(QLatin1Char('&'), QStringLiteral("&&")));
	tabs->setTabToolTip(index, p.docPeekOnTab ? QString() : b->fullName());

	// pin button (Tab Bar > Enable pin tab feature / Show only pinned button / Show buttons on inactive tabs)
	const QTabBar::ButtonPosition closeSide = static_cast<QTabBar::ButtonPosition>(
		tabs->style()->styleHint(QStyle::SH_TabBar_CloseButtonPosition, nullptr, tabs));
	const QTabBar::ButtonPosition pinSide = (closeSide == QTabBar::RightSide) ? QTabBar::LeftSide : QTabBar::RightSide;
	const bool isActive = (index == tabs->currentIndex());
	const bool showPin = p.tabPinButton && (!p.tabOnlyPinnedButton || b->pinned) && (isActive || p.tabInactiveButtons || b->pinned);
	auto* pin = qobject_cast<QToolButton*>(tabs->tabButton(index, pinSide));
	if (showPin)
	{
		if (!pin)
		{
			pin = new QToolButton(tabs);
			pin->setAutoRaise(true);
			pin->setFocusPolicy(Qt::NoFocus);
			pin->setIconSize(QSize(12, 12));
			pin->setFixedSize(16, 16);
			pin->setObjectName(QStringLiteral("pinButton"));
			connect(pin, &QToolButton::clicked, this, [this, pin, tabs, view]
			{
				for (int i = 0; i < tabs->count(); ++i)
				{
					for (auto side : {QTabBar::LeftSide, QTabBar::RightSide})
					{
						if (tabs->tabButton(i, side) == pin)
						{
							if (Buffer* t = bufferAt(view, i))
							{
								activateBuffer(t, view);
								command(IDM_PINTAB);
							}
							return;
						}
					}
				}
			});
			tabs->setTabButton(index, pinSide, pin);
		}
		const QString pinIcon = dark ? (b->pinned ? QStringLiteral(":/icons/dark/tabbar/pinTabButton_pinned.ico") : QStringLiteral(":/icons/dark/tabbar/pinTabButton.ico"))
		                             : (b->pinned ? QStringLiteral(":/icons/standard/tabbar/pinTabButton_pinned.ico") : QStringLiteral(":/icons/standard/tabbar/pinTabButton.ico"));
		pin->setIcon(QIcon(pinIcon));
		pin->setToolTip(b->pinned ? QStringLiteral("Unpin Tab") : QStringLiteral("Pin Tab"));
	}
	else if (pin)
	{
		tabs->setTabButton(index, pinSide, nullptr);
		pin->deleteLater();
	}
	if (QWidget* close = tabs->tabButton(index, closeSide))
		close->setVisible(isActive || p.tabInactiveButtons);
}

void MainWindow::updateBufferTabs(Buffer* buf)
{
	for (int v = MAIN_VIEW; v <= SUB_VIEW; ++v)
	{
		const int i = indexOf(v, buf);
		if (i >= 0)
			updateTabLabel(v, i);
	}
}

void MainWindow::updateAllTabs()
{
	for (int v = MAIN_VIEW; v <= SUB_VIEW; ++v)
	{
		for (int i = 0; i < _views[v].tabs->count(); ++i)
			updateTabLabel(v, i);
	}
}

void MainWindow::updateTabBarFocus()
{
	for (int v = MAIN_VIEW; v <= SUB_VIEW; ++v)
		_views[v].edit->setActiveView(v == _activeView);
	// the coloured bar of the active tab: "focused" colour in the active view only
	for (int v = MAIN_VIEW; v <= SUB_VIEW; ++v)
	{
		NppTabBar::Look look = _tabLook;
		if (v != _activeView && viewVisible(v) && viewVisible(1 - v) && _tabUnfocusedColor.isValid())
			look.activeBarColor = _tabUnfocusedColor;
		_views[v].tabs->setLook(look);
	}
}

void MainWindow::updateTitle()
{
	if (!_current)
	{
		setWindowTitle(Npp::appName());
		return;
	}
	// Preferences > MISC. > Show only filename in title bar
	const QString name = Npp::prefs().shortTitle ? _current->displayName() : _current->fullName();
	QString title = name + QStringLiteral(" - ") + Npp::appName();
	if (_current->dirty)
		title.prepend(QLatin1Char('*'));
	setWindowTitle(title);
}

void MainWindow::setStatusText(const QString& text, int msTimeout)
{
	_statusMessage = text;
	updateStatusBar();
	_statusTimer->start(msTimeout);
}

void MainWindow::updateStatusBar()
{
	if (!_current)
		return;
	EditView* v = _edit;
	// like Notepad++, the language part changes with the buffer or its language only: the text a
	// plugin sets there (NPPM_SETSTATUSBAR) stays while the caret moves
	const QString langText = _statusMessage.isEmpty() ? langDescription(_current) : _statusMessage;
	const QString langKey = QString::number(reinterpret_cast<quintptr>(_current)) + QLatin1Char('|') + langText;
	if (langKey != _sbLangKey)
	{
		_sbLangKey = langKey;
		_sbLang->setText(langText);
	}

	const sptr_t length = v->ex(SCI_GETLENGTH);
	const sptr_t lines = v->ex(SCI_GETLINECOUNT);
	_sbDocInfo->setText(QStringLiteral("length : %1    lines : %2").arg(withThousands(length), withThousands(lines)));

	const sptr_t pos = v->ex(SCI_GETCURRENTPOS);
	const sptr_t line = v->ex(SCI_LINEFROMPOSITION, pos);
	const sptr_t col = v->ex(SCI_GETCOLUMN, pos) + v->ex(SCI_GETSELECTIONNCARETVIRTUALSPACE, v->ex(SCI_GETMAINSELECTION));
	QString posText = QStringLiteral("Ln : %1    Col : %2    ").arg(withThousands(line + 1), withThousands(col + 1));
	const sptr_t nbSel = v->ex(SCI_GETSELECTIONS);
	if (v->ex(SCI_GETSELECTIONEMPTY))
	{
		posText += QStringLiteral("Pos : %1").arg(withThousands(pos + 1));
	}
	else if (nbSel > 1 || v->ex(SCI_GETSELECTIONMODE) != SC_SEL_STREAM)
	{
		sptr_t chars = 0;
		for (sptr_t i = 0; i < nbSel; ++i)
			chars += v->ex(SCI_COUNTCHARACTERS, v->ex(SCI_GETSELECTIONNSTART, i), v->ex(SCI_GETSELECTIONNEND, i));
		posText += QStringLiteral("Sel : %1 | %2").arg(withThousands(chars), withThousands(nbSel));
	}
	else
	{
		const sptr_t start = v->ex(SCI_GETSELECTIONSTART);
		const sptr_t end = v->ex(SCI_GETSELECTIONEND);
		const sptr_t chars = v->ex(SCI_COUNTCHARACTERS, start, end);
		const auto range = v->selectionLinesRange();
		posText += QStringLiteral("Sel : %1 | %2").arg(withThousands(chars), withThousands(range.second - range.first + 1));
	}
	_sbPos->setText(posText);
	_sbEol->setText(eolName(static_cast<int>(v->ex(SCI_GETEOLMODE))));
	_sbEnc->setText(FileIO::encodingName(_current->unicode, _current->codepage));
	_sbIns->setText(v->ex(SCI_GETOVERTYPE) ? QStringLiteral("OVR") : QStringLiteral("INS"));
}

void MainWindow::updateMenuState()
{
	if (!_current)
		return;
	Buffer* b = _current;
	const NppPrefs& p = Npp::prefs();
	auto check = [this](int id, bool on)
	{
		if (QAction* a = _actions.value(id))
			a->setChecked(on);
	};

	const int langId = (b->lang == L_TEXT) ? IDM_LANG_TEXT : StyleManager::instance().menuIdFromLang(b->lang);
	QAction* langAction = _actions.value(langId);
	if (b->lang == L_USER && !b->udlName.isEmpty())
	{
		for (QAction* a : _langGroup->actions())
		{
			if (a->data().toInt() > IDM_LANG_USER && a->data().toInt() < IDM_LANG_USER_LIMIT && a->text() == b->udlName)
				langAction = a;
		}
	}
	if (b->lang == L_EXTERNAL)
	{
		langAction = nullptr;
		const int index = StyleManager::instance().externalLexerIndex(b->udlName);
		if (index >= 0)
			langAction = _actions.value(IDM_LANG_EXTERNAL + index);
	}
	if (langAction)
		langAction->setChecked(true);

	int encId = IDM_FORMAT_AS_UTF_8;
	switch (b->unicode)
	{
		case UniMode::uniUTF8: encId = IDM_FORMAT_UTF_8; break;
		case UniMode::uni16BE:
		case UniMode::uni16BE_NoBOM: encId = IDM_FORMAT_UTF_16BE; break;
		case UniMode::uni16LE:
		case UniMode::uni16LE_NoBOM: encId = IDM_FORMAT_UTF_16LE; break;
		case UniMode::uni8Bit:
			encId = IDM_FORMAT_ANSI;
			if (b->codepage != CP_ANSI)
			{
				const int idx = EncodingMapper::getInstance().getIndexFromEncoding(b->codepage);
				if (idx >= 0)
					encId = IDM_FORMAT_ENCODE + idx;
			}
			break;
		default: break;
	}
	if (QAction* a = _actions.value(encId))
		a->setChecked(true);

	const int eol = static_cast<int>(_edit->ex(SCI_GETEOLMODE));
	check(eol == SC_EOL_CRLF ? IDM_FORMAT_TODOS : eol == SC_EOL_CR ? IDM_FORMAT_TOMAC : IDM_FORMAT_TOUNIX, true);

	check(IDM_VIEW_WRAP, p.wordWrap);
	check(IDM_VIEW_TAB_SPACE, p.showSpaceTab);
	check(IDM_VIEW_EOL, p.showEol);
	check(IDM_VIEW_ALL_CHARACTERS, p.showSpaceTab && p.showEol);
	check(IDM_VIEW_INDENT_GUIDE, p.showIndentGuide);
	check(IDM_VIEW_WRAP_SYMBOL, p.showWrapSymbol);
	check(IDM_VIEW_NPC, p.showNpc);
	// split view
	const bool both = viewVisible(MAIN_VIEW) && viewVisible(SUB_VIEW);
	check(IDM_VIEW_SYNSCROLLV, _sync.vertical);
	check(IDM_VIEW_SYNSCROLLH, _sync.horizontal);
	check(IDM_VIEW_ZOOM_SYNC, p.zoomSync);
	for (int id : {IDM_VIEW_SYNSCROLLV, IDM_VIEW_SYNSCROLLH, IDM_VIEW_ZOOM_SYNC, IDM_VIEW_SWITCHTO_OTHER_VIEW})
	{
		if (QAction* a = _actions.value(id))
			a->setEnabled(both);
	}
	if (QAction* a = _actions.value(IDM_VIEW_GOTO_ANOTHER_VIEW))
		a->setEnabled(_tabs->count() > 1 || viewVisible(otherView()));
	check(IDM_VIEW_NPC_CCUNIEOL, p.showCcUniEol);
	check(IDM_VIEW_DISTRACTIONFREE, _distractionFree);
	check(IDM_VIEW_ALWAYSONTOP, p.alwaysOnTop);
	check(IDM_VIEW_FULLSCREENTOGGLE, isFullScreen() && !_postIt);
	check(IDM_VIEW_POSTIT, _postIt);
	check(IDM_VIEW_MONITORING, b->monitoring);
	check(IDM_EDIT_TOGGLEREADONLY, b->userReadOnly);
	check(IDM_PINTAB, b->pinned);
	check(IDM_VIEW_FILEBROWSER, _workspaceDock && _workspaceDock->isVisible());
	check(IDM_VIEW_DOCLIST, _docListDock && _docListDock->isVisible());
	check(IDM_VIEW_DOC_MAP, docMapShown());
	check(IDM_VIEW_FUNC_LIST, funcListShown());
	check(IDM_EDIT_CHAR_PANEL, _charPanelDock && !_charPanelDock->isHidden());
	for (int i = 0; i < 3; ++i)
		check(IDM_VIEW_PROJECT_PANEL_1 + i, projectPanelShown(i));
	check(IDM_EDIT_CLIPBOARDHISTORY_PANEL, _clipboardDock && !_clipboardDock->isHidden());

	if (QAction* a = _actions.value(IDM_FILE_RESTORELASTCLOSEDFILE))
		a->setEnabled(!_closedFiles.isEmpty());
	if (QAction* a = _actions.value(IDM_MACRO_STARTRECORDINGMACRO))
		a->setEnabled(!_recording);
	if (QAction* a = _actions.value(IDM_MACRO_STOPRECORDINGMACRO))
		a->setEnabled(_recording);
	// Notepad_plus::checkMacroState()
	if (QAction* a = _actions.value(IDM_MACRO_PLAYBACKRECORDEDMACRO))
		a->setEnabled(!_recording && !_macro.empty());
	if (QAction* a = _actions.value(IDM_MACRO_SAVECURRENTMACRO))
		a->setEnabled(!_recording && !_macro.empty() && !_recordingSaved);
	if (QAction* a = _actions.value(IDM_MACRO_RUNMULTIMACRODLG))
		a->setEnabled(!_recording && (!_macro.empty() || !_savedMacros.empty()));
	if (QAction* a = _actions.value(IDM_FILE_SAVE))
		a->setEnabled(b->dirty || b->isUntitled());
	if (QAction* a = _actions.value(IDM_FILE_RELOAD))
		a->setEnabled(!b->isUntitled());
}

void MainWindow::onSavePointChanged(int view, bool dirty)
{
	Buffer* b = viewCurrent(view);
	if (!b)
		return;
	b->dirty = dirty || b->forcedDirty;
	updateBufferTabs(b);
	if (b == _current)
	{
		updateTitle();
		updateMenuState();
	}
	refreshDocumentList();
}

void MainWindow::onSciNotify(Scintilla::NotificationData* scn, int view)
{
	if (_pluginsManager)
		_pluginsManager->relayScintilla(scn, view);
	const int code = static_cast<int>(scn->nmhdr.code);
	if (code == SCN_UPDATEUI)
	{
		const int updated = static_cast<int>(scn->updated);
		if ((_sync.vertical || _sync.horizontal) && (updated & (SC_UPDATE_V_SCROLL | SC_UPDATE_H_SCROLL)))
			syncScroll(view);
		if (view == _activeView)
		{
			updateStatusBar();
			if (updated & (SC_UPDATE_SELECTION | SC_UPDATE_CONTENT))
				smartHighlightOtherView();
			// NppNotification.cpp SCN_UPDATEUI / SCN_PAINTED
			if (docMapShown() && !_docMap->isTemporary())
				_docMap->scrollMap();
			if (funcListShown() && (updated & (SC_UPDATE_SELECTION | SC_UPDATE_CONTENT)))
				_funcList->markEntry();
		}
		else if (updated & (SC_UPDATE_V_SCROLL | SC_UPDATE_CONTENT))
		{
			smartHighlightOtherView(); // the other view scrolled: highlight its visible lines
		}
		return;
	}
	if (code == SCN_ZOOM)
	{
		syncZoom(view);
		if (view == _activeView && docMapShown() && !_docMap->isTemporary())
		{
			_docMap->wrapMap();
			_docMap->scrollMap();
		}
		return;
	}
	if (code == SCN_PAINTED)
	{
		// a resized editor wraps at other columns: DocumentMap::wrapMap() on SCN_PAINTED
		if (view == _activeView && docMapShown() && !_docMap->isTemporary() && _docMap->needToRewrap())
		{
			_docMap->wrapMap();
			_docMap->scrollMap();
		}
		return;
	}
	if (view != _activeView)
	{
		// the other view: typing happens in the active one, only keep the backup state
		if (code == SCN_MODIFIED && (static_cast<int>(scn->modificationType) & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT)))
		{
			if (Buffer* b = viewCurrent(view))
				b->backupStale = true;
		}
		return;
	}
	switch (code)
	{
		case SCN_MODIFIED:
		{
			const int mod = static_cast<int>(scn->modificationType);
			if ((mod & (SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT)) && _current)
			{
				_current->backupStale = true;
				if (scn->linesAdded != 0)
					updateStatusBar();
				// New Document > Use the first line of document as untitled tab name
				if (Npp::prefs().useContentAsTabName && _current->isUntitled() && !_current->untitledTabRenamed &&
				    _edit->ex(SCI_LINEFROMPOSITION, scn->position) == 0)
				{
					Buffer* b = _current;
					QTimer::singleShot(0, this, [this, b] { if (b == _current) useFirstLineAsTabName(b); });
				}
			}
			break;
		}
		case SCN_CHARADDED:
		{
			const NppPrefs& p = Npp::prefs();
			const int ch = scn->ch;
			if (!_edit->allowAutoCompletion())
				break;
			if (p.funcParamsHint && (ch == '(' || ch == ',' || ch == ')'))
				editFunctionCallTip(ch == ')' ? -2 : 0);
			// AutoCompletion::update(): let Scintilla filter an open list unless the list is brief
			if (!p.autoCompletionBrief && _edit->ex(SCI_AUTOCACTIVE))
				break;
			if (p.autoCompletion && ch > 0 && (QChar(static_cast<char16_t>(ch)).isLetterOrNumber() || ch == '_' || ch >= 0x80))
				editAutoComplete(false, false);
			break;
		}
		case SCN_AUTOCSELECTION:
		{
			// Auto-Completion > Insert Selection: TAB / ENTER (NppNotification.cpp)
			const NppPrefs& p = Npp::prefs();
			if (static_cast<int>(scn->listCompletionMethod) == SC_AC_NEWLINE && !p.autocInsertEnter)
			{
				_edit->ex(SCI_AUTOCCANCEL);
				_edit->ex(SCI_NEWLINE);
			}
			else if (static_cast<int>(scn->listCompletionMethod) == SC_AC_TAB && !p.autocInsertTab)
			{
				_edit->ex(SCI_AUTOCCANCEL);
				_edit->ex(SCI_TAB);
			}
			break;
		}
		case SCN_INDICATORRELEASE:
		{
			if (Npp::prefs().urlHighlight && _edit->allowClickableLink() && !(static_cast<int>(scn->modifiers) & SCMOD_SHIFT))
			{
				const QString url = _edit->urlAt(scn->position);
				if (!url.isEmpty() && _edit->ex(SCI_GETSELECTIONEMPTY))
					QDesktopServices::openUrl(QUrl(url));
			}
			break;
		}
		case SCN_MODIFYATTEMPTRO:
			if (_current && _current->isReadOnly())
				setStatusText(QStringLiteral("The document is read-only"));
			break;
		default:
			break;
	}
}

// ---------------------------------------------------------------------------
// split view: synchronized scrolling and zoom (Notepad_plus::doSynScroll, syncZoom)
// ---------------------------------------------------------------------------
void MainWindow::checkSyncState()
{
	if (!viewVisible(MAIN_VIEW) || !viewVisible(SUB_VIEW))
	{
		_sync.vertical = false;
		_sync.horizontal = false;
	}
	else
	{
		syncZoom(_activeView);
	}
	updateTabBarFocus();
}

void MainWindow::syncScroll(int fromView)
{
	if (_syncingScroll || !viewVisible(MAIN_VIEW) || !viewVisible(SUB_VIEW))
		return;
	EditView* m = _views[MAIN_VIEW].edit;
	EditView* sv = _views[SUB_VIEW].edit;
	sptr_t line = 0;
	sptr_t column = 0;
	if (_sync.vertical)
	{
		const sptr_t mainLine = m->ex(SCI_GETFIRSTVISIBLELINE);
		const sptr_t subLine = sv->ex(SCI_GETFIRSTVISIBLELINE);
		line = (fromView == MAIN_VIEW) ? mainLine - _sync.line - subLine : subLine + _sync.line - mainLine;
	}
	if (_sync.horizontal)
	{
		const sptr_t mainColumn = m->ex(SCI_GETXOFFSET) / std::max<sptr_t>(1, m->exs(SCI_TEXTWIDTH, STYLE_DEFAULT, "P"));
		const sptr_t subColumn = sv->ex(SCI_GETXOFFSET) / std::max<sptr_t>(1, sv->exs(SCI_TEXTWIDTH, STYLE_DEFAULT, "P"));
		column = (fromView == MAIN_VIEW) ? mainColumn - _sync.column - subColumn : subColumn + _sync.column - mainColumn;
	}
	if (line == 0 && column == 0)
		return;
	_syncingScroll = true;
	(fromView == MAIN_VIEW ? sv : m)->ex(SCI_LINESCROLL, static_cast<uptr_t>(column), line);
	_syncingScroll = false;
}

void MainWindow::syncZoom(int fromView)
{
	if (_syncingZoom || !Npp::prefs().zoomSync || !viewVisible(MAIN_VIEW) || !viewVisible(SUB_VIEW))
		return;
	_syncingZoom = true;
	_views[1 - fromView].edit->ex(SCI_SETZOOM, _views[fromView].edit->ex(SCI_GETZOOM));
	_syncingZoom = false;
}

void MainWindow::smartHighlightOtherView()
{
	// Highlighting > Smart Highlighting > Highlight another view
	const int other = 1 - _activeView;
	if (!viewVisible(other) || viewCurrent(other) == _current)
		return; // same document: the indicators are shared
	EditView* ov = _views[other].edit;
	const NppPrefs& p = Npp::prefs();
	if (!p.smartHighlight || !p.smartHighlightAnotherView || !ov->allowSmartHilite())
		return;
	ov->clearIndicator(SCE_UNIVERSAL_FOUND_STYLE_SMART);
	QByteArray text;
	int flags = 0;
	if (_edit->smartHighlightTarget(&text, &flags))
		ov->highlightVisible(SCE_UNIVERSAL_FOUND_STYLE_SMART, text, flags);
}

void MainWindow::refreshOtherViews(Buffer* buf)
{
	// lexer and folding belong to the document, styles to each view
	for (int v = MAIN_VIEW; v <= SUB_VIEW; ++v)
	{
		if (v != _activeView && _views[v].current == buf && buf)
			_views[v].edit->defineDocType(buf->lang, buf->udlName);
	}
}

void MainWindow::moveToOtherView(bool clone)
{
	if (_current)
		transferBuffer(_current, _activeView, 1 - _activeView, -1, clone);
}

void MainWindow::transferBuffer(Buffer* buf, int fromView, int toView, int insertAt, bool clone)
{
	// Notepad_plus::docGotoAnotherEditView()
	if (!buf || fromView == toView || indexOf(fromView, buf) < 0)
		return;
	if (!clone && _views[fromView].tabs->count() == 1 && !viewVisible(toView))
		return; // the only document, nothing to move it next to
	if (buf == viewCurrent(fromView))
		saveViewPosition(fromView);
	if (indexOf(toView, buf) < 0)
	{
		buf->pos[toView] = buf->pos[fromView];
		addTab(toView, buf, insertAt);
	}
	showView(toView);
	if (!clone)
		removeTab(buf, fromView);
	activateBuffer(buf, toView);
	checkSyncState();
	refreshDocumentList();
}

void MainWindow::rotateSplitter()
{
	_splitter->setOrientation(_splitter->orientation() == Qt::Horizontal ? Qt::Vertical : Qt::Horizontal);
	uiSettings().setValue(QStringLiteral("splitterSideBySide"), _splitter->orientation() == Qt::Horizontal);
	const int total = (_splitter->orientation() == Qt::Horizontal) ? _splitter->width() : _splitter->height();
	if (viewVisible(MAIN_VIEW) && viewVisible(SUB_VIEW))
		_splitter->setSizes({total / 2, total - total / 2});
}

// ---------------------------------------------------------------------------
// commands dispatch
// ---------------------------------------------------------------------------
void MainWindow::command(int id)
{
	auto it = _handlers.constFind(id);
	if (it == _handlers.constEnd())
		return;
	// plugins that subclass the main window see WM_COMMAND first (XBrackets...)
	if (_pluginsManager && _pluginsManager->routeCommand(id))
		return;
	if (_recording && isMacroRecordable(id))
	{
		MacroStep step;
		step.kind = MacroStep::Menu;
		step.message = static_cast<unsigned int>(id);
		_macro.push_back(step);
	}
	it.value()();
}

// ---------------------------------------------------------------------------
// recent files
// ---------------------------------------------------------------------------
void MainWindow::addRecent(const QString& path)
{
	_recent.removeAll(path);
	_recent.prepend(path);
	const int maxCount = std::max(0, Npp::prefs().recentFilesMax);
	while (_recent.size() > maxCount)
		_recent.removeLast();
	uiSettings().setValue(QStringLiteral("recentFiles"), _recent);
	rebuildRecentMenu();
}

void MainWindow::rebuildRecentMenu()
{
	if (!_fileMenu || !_recentSeparator)
		return;
	for (QAction* a : _recentActions)
	{
		_fileMenu->removeAction(a);
		if (_recentSubMenu)
			_recentSubMenu->removeAction(a);
		a->deleteLater();
	}
	_recentActions.clear();

	// Preferences > Recent Files History > In Submenu
	const bool inSubmenu = Npp::prefs().recentInSubmenu;
	if (inSubmenu && !_recentSubMenu)
	{
		_recentSubMenu = new QMenu(this);
		_recentSubMenuAction = _fileMenu->insertMenu(_recentSeparator, _recentSubMenu);
	}
	if (_recentSubMenuAction)
	{
		_recentSubMenuAction->setText(Localization::instance().menu(QStringLiteral("file-recentFiles"), QStringLiteral("Recent Files")));
		_recentSubMenuAction->setVisible(inSubmenu);
	}
	QMenu* target = inSubmenu ? _recentSubMenu : _fileMenu;

	// files currently opened are not listed, like in Notepad++
	QStringList shown;
	for (const QString& p : _recent)
	{
		if (!findBuffer(p))
			shown << p;
	}
	int n = 1;
	for (const QString& p : shown)
	{
		QString shownName = recentLabel(p);
		shownName.replace(QLatin1Char('&'), QStringLiteral("&&"));
		const QString label = (n <= 9) ? QStringLiteral("&%1: %2").arg(n).arg(shownName) : QStringLiteral("%1: %2").arg(n).arg(shownName);
		auto* a = new QAction(label, this);
		a->setToolTip(p);
		connect(a, &QAction::triggered, this, [this, p]
		{
			if (!QFileInfo::exists(p))
			{
				const auto answer = QMessageBox::question(this, QStringLiteral("Recent file"),
					QStringLiteral("\"%1\" doesn't exist. Remove it from the recent files list?").arg(p));
				if (answer == QMessageBox::Yes)
				{
					_recent.removeAll(p);
					uiSettings().setValue(QStringLiteral("recentFiles"), _recent);
					rebuildRecentMenu();
				}
				return;
			}
			openFile(p);
		});
		if (inSubmenu)
			_recentSubMenu->addAction(a);
		else
			target->insertAction(_recentSeparator, a);
		_recentActions << a;
		++n;
	}
	if (_recentSubMenuAction)
		_recentSubMenuAction->setEnabled(!shown.isEmpty());
	if (QAction* a = _actions.value(IDM_OPEN_ALL_RECENT_FILE))
		a->setEnabled(!shown.isEmpty());
	if (QAction* a = _actions.value(IDM_CLEAN_RECENT_FILE_LIST))
		a->setEnabled(!_recent.isEmpty());
}

// ---------------------------------------------------------------------------
// file status auto-detection
// ---------------------------------------------------------------------------
void MainWindow::watch(Buffer* buf)
{
	if (buf && !buf->isUntitled() && QFileInfo::exists(buf->path))
		_watcher->addPath(buf->path);
}

void MainWindow::unwatch(Buffer* buf)
{
	if (buf && !buf->isUntitled())
		_watcher->removePath(buf->path);
}

void MainWindow::onFileChanged(const QString& path)
{
	if (Npp::prefs().fileAutoDetectMode == AUTODETECT_DISABLED)
		return;
	_pendingChanges.insert(path);
	Buffer* b = findBuffer(path);
	if (b && b->monitoring)
	{
		QTimer::singleShot(100, this, [this] { checkModifiedFiles(true); });
		return;
	}
	if (isActiveWindow())
		QTimer::singleShot(300, this, [this] { checkModifiedFiles(true); });
}

void MainWindow::checkModifiedFiles(bool onlyPending)
{
	const int detectMode = Npp::prefs().fileAutoDetectMode;
	if (_checkingFiles || _closing || detectMode == AUTODETECT_DISABLED)
		return;
	_checkingFiles = true;

	const QSet<QString> pending = _pendingChanges;
	_pendingChanges.clear();

	for (Buffer* b : allBuffers())
	{
		if (b->isUntitled())
			continue;
		if (onlyPending && !pending.contains(b->path))
			continue;
		if (indexOf(b) < 0)
			continue;
		// "Enable for current file(s)": the other documents are checked when they become active
		if (detectMode == AUTODETECT_CURRENT && b != _current && !b->monitoring)
		{
			if (pending.contains(b->path))
				_pendingChanges.insert(b->path);
			continue;
		}

		const QFileInfo fi(b->path);
		if (!fi.exists())
		{
			if (b->missing)
				continue;
			b->missing = true;
			activateBuffer(b);
			const auto answer = QMessageBox::question(this, QStringLiteral("Keep non existing file"),
				QStringLiteral("The file \"%1\" doesn't exist anymore.\nKeep this file in editor?").arg(b->path));
			if (answer == QMessageBox::Yes)
			{
				b->forcedDirty = true;
				b->dirty = true;
				updateBufferTabs(b);
				updateTitle();
			}
			else
			{
				closeBuffer(b, false);
			}
			continue;
		}

		b->missing = false;
		if (fi.lastModified() == b->lastModified)
		{
			watch(b);
			continue;
		}
		b->lastModified = fi.lastModified();
		b->fileReadOnly = !fi.isWritable();

		if (b->monitoring)
		{
			reloadBuffer(b, FileIO::Force::Auto, CP_ANSI, false);
			if (b == _current)
				_edit->ex(SCI_DOCUMENTEND);
		}
		else if (!b->dirty && Npp::prefs().updateSilently)
		{
			reloadBuffer(b);
			if (Npp::prefs().scrollToEndAfterUpdate && b == _current)
				_edit->ex(SCI_DOCUMENTEND);
		}
		else
		{
			activateBuffer(b);
			QString msg = QStringLiteral("%1\n\nThis file has been modified by another program.\nDo you want to reload it").arg(b->path);
			msg += b->dirty ? QStringLiteral(" and lose the changes made in LinuxPad++?") : QStringLiteral("?");
			const auto answer = QMessageBox::question(this, QStringLiteral("Reload"), msg);
			if (answer == QMessageBox::Yes)
			{
				reloadBuffer(b);
				if (Npp::prefs().scrollToEndAfterUpdate && b == _current)
					_edit->ex(SCI_DOCUMENTEND);
			}
		}
		watch(b);
	}
	_checkingFiles = false;
}

bool MainWindow::isLargeFileSize(qint64 size) const
{
	const NppPrefs& p = Npp::prefs();
	return p.largeFileRestriction && size >= p.largeFileSizeBytes();
}

// ---------------------------------------------------------------------------
// session & snapshot (session.xml uses the Notepad++ format)
// ---------------------------------------------------------------------------
void MainWindow::removeBackup(Buffer* buf)
{
	if (!buf->backupPath.isEmpty())
	{
		QFile::remove(buf->backupPath);
		buf->backupPath.clear();
	}
	buf->backupStale = true;
}

void MainWindow::backupSnapshot()
{
	if (!Npp::prefs().sessionSnapshot)
		return;
	bool changed = false;
	for (Buffer* b : allBuffers())
	{
		const bool needsBackup = b->dirty || (b->isUntitled() && bufferText(b).size() > 0);
		if (!needsBackup)
		{
			if (!b->backupPath.isEmpty())
			{
				removeBackup(b);
				changed = true;
			}
			continue;
		}
		if (!b->backupStale && !b->backupPath.isEmpty() && QFileInfo::exists(b->backupPath))
			continue;
		if (b->backupPath.isEmpty())
		{
			QString name = b->displayName();
			name.replace(QLatin1Char('/'), QLatin1Char('_'));
			b->backupPath = Npp::backupDir() + QLatin1Char('/') + name + QLatin1Char('@') +
			                QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_HHmmss_zzz"));
		}
		QFile f(b->backupPath);
		if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
		{
			f.write(bufferText(b));
			f.close();
			b->backupStale = false;
			changed = true;
		}
	}
	if (changed)
		writeSession(Npp::localConfigDir() + QStringLiteral("/session.xml"), true);
}

void MainWindow::writeSession(const QString& path, bool withBackups)
{
	// session.xml of Notepad++: mainView and subView, a cloned document is in both
	saveViewPosition(MAIN_VIEW);
	saveViewPosition(SUB_VIEW);
	pugi::xml_document doc;
	pugi::xml_node decl = doc.append_child(pugi::node_declaration);
	decl.append_attribute("version") = "1.0";
	decl.append_attribute("encoding") = "UTF-8";
	pugi::xml_node session = doc.append_child("NotepadPlus").append_child("Session");
	session.append_attribute("activeView") = _activeView;

	for (int v = MAIN_VIEW; v <= SUB_VIEW; ++v)
	{
		pugi::xml_node viewNode = session.append_child(v == MAIN_VIEW ? "mainView" : "subView");
		int activeIndex = 0;
		int written = 0;
		for (Buffer* b : viewBuffers(v))
		{
			if (b->isUntitled() && (!withBackups || b->backupPath.isEmpty()))
				continue;
			if (b == viewCurrent(v))
				activeIndex = written;
			++written;
			const Buffer::ViewPos& pos = b->pos[v];
			pugi::xml_node f = viewNode.append_child("File");
			f.append_attribute("firstVisibleLine") = static_cast<long long>(pos.firstVisibleLine);
			f.append_attribute("xOffset") = static_cast<long long>(pos.xOffset);
			f.append_attribute("startPos") = static_cast<long long>(pos.anchor);
			f.append_attribute("endPos") = static_cast<long long>(pos.caret);
			f.append_attribute("selMode") = 0;
			// the name of the User Defined Language for L_USER, of the lexer for L_EXTERNAL (Notepad_plus::getLangDesc)
			if ((b->lang == L_USER || b->lang == L_EXTERNAL) && !b->udlName.isEmpty())
				f.append_attribute("lang") = b->udlName.toUtf8().constData();
			else
				f.append_attribute("lang") = StyleManager::langInfo(b->lang).shortName;
			f.append_attribute("encoding") = (b->unicode == UniMode::uni8Bit) ? b->codepage : -1;
			f.append_attribute("unicodeMode") = static_cast<int>(b->unicode);
			f.append_attribute("eol") = b->eol;
			f.append_attribute("userReadOnly") = b->userReadOnly ? "yes" : "no";
			f.append_attribute("tabPinned") = b->pinned ? "yes" : "no";
			f.append_attribute("filename") = b->fullName().toUtf8().constData();
			f.append_attribute("backupFilePath") = withBackups ? b->backupPath.toUtf8().constData() : "";
			f.append_attribute("originalFileLastModifTimestamp") = static_cast<long long>(b->lastModified.toSecsSinceEpoch());
			if (b->tabColor >= 0)
				f.append_attribute("tabColourId") = b->tabColor;

			// bookmarks
			withDocument(b, [&](EditView* e)
			{
				sptr_t line = e->ex(SCI_MARKERNEXT, 0, 1 << MARK_BOOKMARK);
				while (line >= 0)
				{
					f.append_child("Mark").append_attribute("line") = static_cast<long long>(line);
					line = e->ex(SCI_MARKERNEXT, line + 1, 1 << MARK_BOOKMARK);
				}
			});
		}
		viewNode.append_attribute("activeIndex") = activeIndex;
	}
	doc.save_file(QFile::encodeName(path).constData(), "    ");
}

bool MainWindow::readSession(const QString& path)
{
	pugi::xml_document doc;
	if (!doc.load_file(QFile::encodeName(path).constData()))
		return false;
	pugi::xml_node session = doc.child("NotepadPlus").child("Session");
	if (!session.child("mainView") && !session.child("subView"))
		return false;

	auto langFromSession = [&](const QString& s) -> LangType
	{
		for (int i = 0; i < StyleManager::langCount(); ++i)
		{
			const LanguageNameInfo& li = StyleManager::langInfo(static_cast<LangType>(i));
			if (s == QString::fromUtf8(li.shortName) || s == QString::fromUtf8(li.langName))
				return static_cast<LangType>(i);
		}
		return L_TEXT;
	};

	std::vector<Buffer*> opened[2];
	for (int view = MAIN_VIEW; view <= SUB_VIEW; ++view)
	{
		pugi::xml_node viewNode = session.child(view == MAIN_VIEW ? "mainView" : "subView");
		for (pugi::xml_node f : viewNode.children("File"))
		{
			const QString filename = QString::fromUtf8(f.attribute("filename").as_string());
			const QString backup = QString::fromUtf8(f.attribute("backupFilePath").as_string());
			const int codepage = f.attribute("encoding").as_int(-1);
			const bool hasUnicodeAttr = !f.attribute("unicodeMode").empty();
			const UniMode unicode = static_cast<UniMode>(f.attribute("unicodeMode").as_int(static_cast<int>(UniMode::uniCookie)));
			Buffer* buf = nullptr;
			bool clone = false;

			// a document already loaded for the main view: clone it
			for (Buffer* b : opened[MAIN_VIEW])
			{
				if (view == SUB_VIEW && ((!backup.isEmpty() && b->backupPath == backup) || (backup.isEmpty() && b->fullName() == filename)))
				{
					buf = b;
					clone = true;
				}
			}
			if (clone)
			{
				if (indexOf(view, buf) < 0)
					addTab(view, buf);
			}
			else if (!backup.isEmpty() && QFileInfo::exists(backup))
			{
				QFile bf(backup);
				if (!bf.open(QIODevice::ReadOnly))
					continue;
				const QByteArray content = bf.readAll();
				buf = new Buffer;
				buf->doc = _loader->send(SCI_CREATEDOCUMENT, 0, SC_DOCUMENTOPTION_DEFAULT);
				if (QDir::isAbsolutePath(filename))
				{
					buf->path = filename;
					buf->lastModified = QFileInfo(filename).lastModified();
				}
				else
					buf->untitledName = filename;
				buf->unicode = hasUnicodeAttr ? unicode : (codepage == -1 ? UniMode::uniCookie : UniMode::uni8Bit);
				buf->codepage = codepage;
				buf->eol = f.attribute("eol").as_int(FileIO::detectEol(content) >= 0 ? FileIO::detectEol(content) : Npp::prefs().newDocEol);
				buf->backupPath = backup;
				buf->backupStale = false;
				buf->dirty = true;
				buf->forcedDirty = true;
				loadIntoDocument(buf, content, true);
				addTab(view, buf);
				watch(buf);
			}
			else if (QDir::isAbsolutePath(filename) && QFileInfo(filename).isFile())
			{
				if (Buffer* existing = findBuffer(filename))
				{
					if (indexOf(view, existing) >= 0)
						continue;
					buf = existing; // opened by another session entry: clone
					addTab(view, buf);
					clone = true;
				}
				else
				{
					const int saved = _targetView;
					_targetView = view;
					buf = openFile(filename, false);
					_targetView = saved;
					if (!buf)
						continue;
					if (codepage != -1 && buf->unicode == UniMode::uni8Bit && buf->codepage != codepage && FileIO::isCodepageSupported(codepage))
						reloadBuffer(buf, FileIO::Force::Codepage, codepage, false);
				}
			}
			else if (QDir::isAbsolutePath(filename) && Npp::prefs().keepAbsentFilesInSession && !findBuffer(filename))
			{
				// Backup > Remember inaccessible files from past session: placeholder document
				buf = new Buffer;
				buf->doc = _loader->send(SCI_CREATEDOCUMENT, 0, SC_DOCUMENTOPTION_DEFAULT);
				buf->path = filename;
				buf->missing = true;
				buf->unicode = hasUnicodeAttr ? unicode : UniMode::uniCookie;
				buf->codepage = codepage;
				buf->eol = f.attribute("eol").as_int(Npp::prefs().newDocEol);
				loadIntoDocument(buf, QByteArray(), true);
				addTab(view, buf);
			}
			else
			{
				continue;
			}

			Buffer::ViewPos& pos = buf->pos[view];
			pos.firstVisibleLine = f.attribute("firstVisibleLine").as_llong(0);
			pos.xOffset = f.attribute("xOffset").as_llong(0);
			pos.anchor = f.attribute("startPos").as_llong(0);
			pos.caret = f.attribute("endPos").as_llong(0);
			if (!clone)
			{
				const QString langName = QString::fromUtf8(f.attribute("lang").as_string());
				buf->lang = langFromSession(langName);
				if (buf->lang == L_TEXT && UdlManager::instance().find(langName))
				{
					buf->lang = L_USER;
					buf->udlName = langName;
				}
				else if (buf->lang == L_TEXT && StyleManager::instance().externalLexerIndex(langName) >= 0)
				{
					buf->lang = L_EXTERNAL; // a lexer plugin
					buf->udlName = langName;
				}
				else if (buf->lang == L_TEXT && !buf->isUntitled())
				{
					buf->lang = langOfFile(buf->path, &buf->udlName);
				}
				buf->userReadOnly = QString::fromUtf8(f.attribute("userReadOnly").as_string()) == QStringLiteral("yes");
				buf->pinned = QString::fromUtf8(f.attribute("tabPinned").as_string()) == QStringLiteral("yes");
				buf->tabColor = f.attribute("tabColourId").as_int(-1);
				if (buf->tabColor > 4)
					buf->tabColor = -1;
				withDocument(buf, [&](EditView* e)
				{
					e->ex(SCI_SETREADONLY, buf->isReadOnly());
					for (pugi::xml_node m : f.children("Mark"))
						e->ex(SCI_MARKERADD, m.attribute("line").as_llong(0), MARK_BOOKMARK);
				});
			}
			updateBufferTabs(buf);
			opened[view].push_back(buf);
		}
	}

	if (opened[MAIN_VIEW].empty() && opened[SUB_VIEW].empty())
		return false;

	auto activeOf = [&](int view) -> Buffer*
	{
		const int active = session.child(view == MAIN_VIEW ? "mainView" : "subView").attribute("activeIndex").as_int(0);
		const std::vector<Buffer*>& list = opened[view];
		if (list.empty())
			return nullptr;
		return (active >= 0 && active < static_cast<int>(list.size())) ? list[static_cast<size_t>(active)] : list.front();
	};
	if (_views[SUB_VIEW].tabs->count() > 0)
	{
		showView(SUB_VIEW);
		if (Buffer* b = activeOf(SUB_VIEW))
			activateBuffer(b, SUB_VIEW);
	}
	if (Buffer* b = activeOf(MAIN_VIEW))
		activateBuffer(b, MAIN_VIEW);
	if (_views[MAIN_VIEW].tabs->count() == 0 && _views[SUB_VIEW].tabs->count() > 0)
	{
		if (Buffer* b = activeOf(SUB_VIEW))
			activateBuffer(b, SUB_VIEW);
		hideView(MAIN_VIEW);
	}
	else if (session.attribute("activeView").as_int(0) == SUB_VIEW && viewVisible(SUB_VIEW))
	{
		setActiveView(SUB_VIEW);
	}
	checkSyncState();
	refreshDocumentList();
	return true;
}

bool MainWindow::restoreSession()
{
	bool restored = false;
	if (Npp::prefs().rememberSession || Npp::prefs().sessionSnapshot)
		restored = readSession(Npp::localConfigDir() + QStringLiteral("/session.xml"));
	const NppPrefs& p = Npp::prefs();
	if (_tabs->count() == 0)
		newDocument(true);
	else if (restored && p.addNewDocOnStartup && p.rememberSession && !(_tabs->count() == 1 && isCleanEmptyUntitled(bufferAt(0))))
		newDocument(true); // New Document > Always open a new document in addition at startup
	if (p.sessionSnapshot)
		_backupTimer->start(std::max(1, p.snapshotIntervalSec) * 1000);
	if (!p.recentDontCheck)
	{
		// Recent Files History: check at launch time
		_recent.erase(std::remove_if(_recent.begin(), _recent.end(), [](const QString& f) { return !QFileInfo::exists(f); }), _recent.end());
		uiSettings().setValue(QStringLiteral("recentFiles"), _recent);
	}
	rebuildRecentMenu();
	applyPanelsState();
	return restored;
}

void MainWindow::openFromCommandLine(const QStringList& paths, const OpenOptions& opts)
{
	Buffer* last = nullptr;
	for (const QString& p : paths)
	{
		if (Buffer* b = openFile(p, true, opts))
			last = b;
	}
	if (last)
		activateBuffer(last);
	if (_tabs->count() == 0)
		newDocument(true);
	raise();
	activateWindow();
}

// ---------------------------------------------------------------------------
// theme and preferences
// ---------------------------------------------------------------------------
void MainWindow::applyTheme(const QString& path)
{
	QString themePath = path;
	if (themePath.isEmpty() && Npp::prefs().isDark())
	{
		const QString dark = Npp::dataDir() + QStringLiteral("/themes/DarkModeDefault.xml");
		if (QFileInfo::exists(dark))
			themePath = dark;
	}
	StyleManager::instance().loadTheme(themePath);
	for (int v = MAIN_VIEW; v <= SUB_VIEW; ++v)
	{
		if (Buffer* b = viewCurrent(v))
		{
			_views[v].edit->defineDocType(b->lang, b->udlName);
			_views[v].edit->doSmartHighlight();
		}
	}
	if (_results)
		_results->applyStyle();
	if (_docMap)
	{
		_docMap->applyStyle();
		if (docMapShown())
			_docMap->reloadMap(true);
	}
	if (_funcList)
		_funcList->applyIcons();
	for (ProjectPanel* pp : _projectPanels)
	{
		if (pp)
			pp->applyIcons();
	}
	applyPanelColours();
	if (_pluginsManager && _pluginsManager->isReady())
		_pluginsManager->notifyBuffer(NPPN_WORDSTYLESUPDATED, _current);
}

void MainWindow::applyPanelColours()
{
	// NppBigSwitch.cpp WM_UPDATESCINTILLAS: the panels take the colours of the theme's Default Style
	const Style* def = StyleManager::instance().globalStyles().findByID(STYLE_DEFAULT);
	if (!def || def->fgColor == static_cast<COLORREF>(STYLE_NOT_USED) || def->bgColor == static_cast<COLORREF>(STYLE_NOT_USED))
		return;
	const QColor fg = toQColor(def->fgColor);
	const QColor bg = toQColor(def->bgColor);
	QList<QWidget*> panels{_docList, _workspaceTree};
	if (_funcList)
		panels << _funcList->tree();
	if (_charPanel)
		panels << _charPanel->list();
	if (_clipboardPanel)
		panels << _clipboardPanel->list();
	for (ProjectPanel* pp : _projectPanels)
	{
		if (pp)
			panels << pp->tree();
	}
	for (QWidget* w : panels)
	{
		if (!w)
			continue;
		QPalette pal = w->palette();
		pal.setColor(QPalette::Base, bg);
		pal.setColor(QPalette::AlternateBase, bg);
		pal.setColor(QPalette::Text, fg);
		w->setPalette(pal);
	}
}

void MainWindow::applyPrefs()
{
	const NppPrefs& p = Npp::prefs();
	for (int v = MAIN_VIEW; v <= SUB_VIEW; ++v)
	{
		EditView* e = _views[v].edit;
		e->applyPrefs();
		if (Buffer* b = viewCurrent(v))
		{
			e->ex(SCI_SETCHANGEHISTORY, EditView::changeHistoryFlags());
			e->defineDocType(b->lang, b->udlName);
		}
	}
	applyMenuBarPrefs();
	applyToolbarIcons();
	_toolbar->setVisible(p.toolbarVisible && !_postIt && !_distractionFree);
	statusBar()->setVisible(p.statusbarVisible && !_postIt && !_distractionFree);
	applyTabBarPrefs();
	applyLanguageMenu();
	applyTray();

	// Margins/Border/Edge > Border Width
	for (DocView& dv : _views)
	{
		if (auto* l = dv.editHolder->layout())
			l->setContentsMargins(p.borderWidth, p.borderWidth, p.borderWidth, p.borderWidth);
	}

	if (static_cast<bool>(windowFlags() & Qt::WindowStaysOnTopHint) != p.alwaysOnTop)
	{
		const bool visible = isVisible();
		setWindowFlag(Qt::WindowStaysOnTopHint, p.alwaysOnTop);
		if (visible)
			show();
	}
	if (p.sessionSnapshot)
		_backupTimer->start(std::max(1, p.snapshotIntervalSec) * 1000);
	else
		_backupTimer->stop();
	if (_workspaceModel)
		_workspaceModel->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::AllDirs | QDir::Hidden | (p.fawSymlinks ? QDir::Filters() : QDir::NoSymLinks));
	if (_distractionFree)
		resizeEvent(nullptr);
	const int maxRecent = std::max(0, p.recentFilesMax);
	if (_recent.size() > maxRecent)
	{
		_recent = _recent.mid(0, maxRecent);
		uiSettings().setValue(QStringLiteral("recentFiles"), _recent);
	}
	if (docMapShown())
		_docMap->reloadMap(true);
	if (_funcList)
		_funcList->applyIcons();
	updateAllTabs();
	updateTitle();
	rebuildRecentMenu();
	updateMenuState();
}

// ---------------------------------------------------------------------------
// events
// ---------------------------------------------------------------------------
void MainWindow::closeEvent(QCloseEvent* event)
{
	const NppPrefs& p = Npp::prefs();
	if (!_reallyQuit && (p.systrayAction == SYSTRAY_CLOSE || p.systrayAction == SYSTRAY_BOTH) && QSystemTrayIcon::isSystemTrayAvailable())
	{
		// MISC. > "Close to" system tray: the close button hides the window, File > Exit quits
		applyTray();
		if (_tray)
			_tray->show();
		hide();
		event->ignore();
		return;
	}
	_reallyQuit = false;
	_pluginsManager->notify(NPPN_BEFORESHUTDOWN, 0);
	struct CancelShutdown // NPPN_CANCELSHUTDOWN when the exit is cancelled
	{
		MainWindow* mw;
		QCloseEvent* ev;
		~CancelShutdown() { if (!ev->isAccepted()) mw->_pluginsManager->notify(NPPN_CANCELSHUTDOWN, 0); }
	} cancelShutdown{this, event};
	event->ignore();
	{
		// NppParameters::writeNeed2SaveUDL()
		QString error;
		UdlManager::instance().saveAll(&error);
	}
	// Notepad_plus::saveProjectPanelsParams(): a modified workspace may cancel the exit
	for (int i = 0; i < 3; ++i)
	{
		if (!_projectPanels[i])
			continue;
		if (!_projectPanels[i]->checkIfNeedSave())
		{
			event->ignore();
			return;
		}
		uiSettings().setValue(QStringLiteral("projectPanel%1Workspace").arg(i + 1), _projectPanels[i]->workSpaceFilePath());
	}
	saveCurrentPosition();

	if (!p.sessionSnapshot)
	{
		bool yesToAll = false;
		bool noToAll = false;
		for (Buffer* b : allBuffers())
		{
			if (!b->dirty || noToAll)
				continue;
			if (!yesToAll)
			{
				activateBuffer(b);
				const auto answer = QMessageBox::question(this, QStringLiteral("Save"),
					QStringLiteral("Save file \"%1\" ?").arg(b->fullName()),
					QMessageBox::Yes | QMessageBox::YesToAll | QMessageBox::No | QMessageBox::NoToAll | QMessageBox::Cancel, QMessageBox::Yes);
				if (answer == QMessageBox::Cancel)
				{
					event->ignore();
					return;
				}
				if (answer == QMessageBox::No)
					continue;
				if (answer == QMessageBox::NoToAll)
				{
					noToAll = true;
					continue;
				}
				if (answer == QMessageBox::YesToAll)
					yesToAll = true;
			}
			if (!saveBuffer(b))
			{
				event->ignore();
				return;
			}
		}
	}

	_closing = true;
	const QString sessionPath = Npp::localConfigDir() + QStringLiteral("/session.xml");
	if (!_instanceSession.isEmpty())
		writeSession(_instanceSession, false);
	if (!_noSession)
	{
		if (p.sessionSnapshot)
		{
			for (Buffer* b : allBuffers())
				b->backupStale = true;
			backupSnapshot();
			writeSession(sessionPath, true);
		}
		else if (p.rememberSession)
			writeSession(sessionPath, false);
		else
			QFile::remove(sessionPath);
	}

	uiSettings().setValue(QStringLiteral("windowGeometry"), saveGeometry());
	uiSettings().setValue(QStringLiteral("docListVisible"), _docListDock && _docListDock->isVisible());
	uiSettings().setValue(QStringLiteral("workspaceVisible"), _workspaceDock && _workspaceDock->isVisible());
	uiSettings().setValue(QStringLiteral("docMapVisible"), docMapShown());
	uiSettings().setValue(QStringLiteral("funcListVisible"), funcListShown());
	uiSettings().setValue(QStringLiteral("charPanelVisible"), _charPanelDock && !_charPanelDock->isHidden());
	for (int i = 0; i < 3; ++i)
		uiSettings().setValue(QStringLiteral("projectPanel%1Visible").arg(i + 1), projectPanelShown(i));
	uiSettings().setValue(QStringLiteral("clipboardHistoryVisible"), _clipboardDock && !_clipboardDock->isHidden());
	uiSettings().setValue(QStringLiteral("recentFiles"), _recent);
	p.save();
	if (_findDlg)
		_findDlg->saveHistory();
	_pluginsManager->notify(NPPN_SHUTDOWN, 0);
	event->accept();
}

void MainWindow::changeEvent(QEvent* event)
{
	if (event->type() == QEvent::ActivationChange && isActiveWindow())
		QTimer::singleShot(0, this, [this] { checkModifiedFiles(false); });
	if (event->type() == QEvent::WindowStateChange && isMinimized())
	{
		const int action = Npp::prefs().systrayAction;
		if ((action == SYSTRAY_MINIMIZE || action == SYSTRAY_BOTH) && QSystemTrayIcon::isSystemTrayAvailable())
		{
			applyTray();
			if (_tray)
				_tray->show();
			QTimer::singleShot(0, this, [this] { hide(); });
		}
	}
	QMainWindow::changeEvent(event);
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
	NppTabBar* source = nullptr;
	int index = -1;
	if (event->mimeData()->hasUrls() || NppTabBar::decodeDrag(event->mimeData(), &source, &index))
		event->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent* event)
{
	NppTabBar* source = nullptr;
	int index = -1;
	if (NppTabBar::decodeDrag(event->mimeData(), &source, &index))
	{
		// a tab dropped on the other view: move it there (Ctrl: clone)
		const int from = viewOfTabBar(source);
		const int to = viewOfWidget(childAt(event->position().toPoint()));
		if (from >= 0 && to >= 0 && from != to)
			transferBuffer(bufferAt(from, index), from, to, -1, (event->modifiers() & Qt::ControlModifier) != 0);
		event->acceptProposedAction();
		return;
	}
	for (const QUrl& url : event->mimeData()->urls())
	{
		if (!url.isLocalFile())
			continue;
		const QString path = url.toLocalFile();
		if (QFileInfo(path).isDir() && Npp::prefs().dropFolderOpensFiles)
		{
			// Default Directory > open all files of a dropped folder instead of Folder as Workspace
			QDirIterator it(path, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
			while (it.hasNext())
				openFile(it.next());
			continue;
		}
		openFile(path);
	}
	event->acceptProposedAction();
}

bool MainWindow::eventFilter(QObject* obj, QEvent* event)
{
	// General > Menu > Hide: Alt or F10 shows the menu bar (Notepad_plus_Window)
	if (Npp::prefs().hideMenuBar && !_postIt && !_distractionFree && isActiveWindow())
	{
		if (event->type() == QEvent::KeyPress)
		{
			auto* ke = static_cast<QKeyEvent*>(event);
			_altAlone = (ke->key() == Qt::Key_Alt && ke->modifiers() == Qt::AltModifier);
			if (ke->key() == Qt::Key_F10 && ke->modifiers() == Qt::NoModifier)
			{
				const bool show = !menuBar()->isVisible();
				menuBar()->setVisible(show);
				_menuShownByAlt = show;
				if (show && !menuBar()->actions().isEmpty())
					menuBar()->setActiveAction(menuBar()->actions().first());
				return true;
			}
		}
		else if (event->type() == QEvent::KeyRelease)
		{
			auto* ke = static_cast<QKeyEvent*>(event);
			if (ke->key() == Qt::Key_Alt && _altAlone && !ke->isAutoRepeat())
			{
				_altAlone = false;
				const bool show = !menuBar()->isVisible();
				menuBar()->setVisible(show);
				_menuShownByAlt = show;
				if (show && !menuBar()->actions().isEmpty())
					menuBar()->setActiveAction(menuBar()->actions().first());
			}
		}
		else if (event->type() == QEvent::MouseButtonPress && _menuShownByAlt && obj != menuBar())
		{
			QWidget* w = qobject_cast<QWidget*>(obj);
			if (!w || !qobject_cast<QMenu*>(w->window()))
			{
				menuBar()->hide();
				_menuShownByAlt = false;
			}
		}
	}

	// document switcher (Ctrl+Tab): activate the selected document when Ctrl is released
	if (_switcher && _switcher->isVisible() && event->type() == QEvent::KeyRelease)
	{
		auto* ke = static_cast<QKeyEvent*>(event);
		if (ke->key() == Qt::Key_Control)
		{
			const int row = _switcherList->currentRow();
			const quintptr ptr = row >= 0 ? _switcherList->item(row)->data(Qt::UserRole).value<quintptr>() : 0;
			_switcher->hide();
			if (Buffer* b = reinterpret_cast<Buffer*>(ptr); b && indexOf(b) >= 0)
				activateBuffer(b);
			return true;
		}
	}
	if (_switcher && _switcher->isVisible() && event->type() == QEvent::KeyPress)
	{
		auto* ke = static_cast<QKeyEvent*>(event);
		if (ke->key() == Qt::Key_Tab || ke->key() == Qt::Key_Backtab)
		{
			documentSwitcher(ke->key() == Qt::Key_Tab && !(ke->modifiers() & Qt::ShiftModifier));
			return true;
		}
		if (ke->key() == Qt::Key_Escape)
		{
			_switcher->hide();
			return true;
		}
	}

	// split view: the view that gets the keyboard focus becomes the active one
	if (event->type() == QEvent::FocusIn)
	{
		const int v = viewOfWidget(obj);
		if (v >= 0 && v != _activeView && qobject_cast<EditView*>(obj))
			setActiveView(v);
	}
	if (_splitter && obj == _splitter->handle(1) && event->type() == QEvent::ContextMenu)
	{
		// Notepad++ splitter: right click to rotate the views
		QMenu menu(this);
		menu.addAction(QStringLiteral("Rotate to left"), this, [this] { rotateSplitter(); });
		menu.addAction(QStringLiteral("Rotate to right"), this, [this] { rotateSplitter(); });
		menu.exec(static_cast<QContextMenuEvent*>(event)->globalPos());
		return true;
	}

	if (const int tv = viewOfTabBar(obj); tv >= 0)
	{
		NppTabBar* tabs = _views[tv].tabs;
		if (event->type() == QEvent::ToolTip && Npp::prefs().docPeekOnTab)
		{
			auto* he = static_cast<QHelpEvent*>(event);
			const int index = tabs->tabAt(he->pos());
			if (index >= 0 && index != tabs->currentIndex())
				showTabPeek(tv, index, he->globalPos());
			else
				hideTabPeek();
			return true;
		}
		if (event->type() == QEvent::MouseMove && Npp::prefs().docPeekOnMap && docMapShown())
		{
			// NppNotification.cpp TCN_MOUSEHOVERING: MISC. > Peek on document map
			const int index = tabs->tabAt(static_cast<QMouseEvent*>(event)->position().toPoint());
			Buffer* b = bufferAt(tv, index);
			if (b && b != viewCurrent(MAIN_VIEW) && b != viewCurrent(SUB_VIEW))
			{
				if (b != _mapPeekBuffer)
					_docMap->showTemporarily(b, tv);
				_mapPeekBuffer = b;
			}
			else if (_docMap->isTemporary())
			{
				_mapPeekBuffer = nullptr;
				_docMap->reloadMap();
			}
		}
		if (event->type() == QEvent::Leave)
		{
			hideTabPeek();
			if (_docMap && _docMap->isTemporary())
			{
				// TCN_MOUSELEAVING
				_mapPeekBuffer = nullptr;
				_docMap->reloadMap();
			}
		}
		if (event->type() == QEvent::MouseButtonPress)
		{
			hideTabPeek();
			if (_docMap && _docMap->isTemporary())
			{
				_mapPeekBuffer = nullptr;
				_docMap->reloadMap();
			}
			if (tv != _activeView)
				setActiveView(tv); // a click on the tab bar of the other view activates it
		}
		if (event->type() == QEvent::MouseButtonDblClick)
		{
			auto* me = static_cast<QMouseEvent*>(event);
			const int index = tabs->tabAt(me->position().toPoint());
			if (index < 0)
			{
				setActiveView(tv);
				newDocument(true);
				return true;
			}
			if (Npp::prefs().doubleClickTabCloses)
			{
				closeBuffer(bufferAt(tv, index), true, tv);
				return true;
			}
		}
		else if (event->type() == QEvent::MouseButtonRelease)
		{
			auto* me = static_cast<QMouseEvent*>(event);
			if (me->button() == Qt::MiddleButton)
			{
				const int index = tabs->tabAt(me->position().toPoint());
				if (index >= 0)
				{
					closeBuffer(bufferAt(tv, index), true, tv);
					return true;
				}
			}
		}
	}
	else if (event->type() == QEvent::MouseButtonRelease)
	{
		auto* me = static_cast<QMouseEvent*>(event);
		QMenu* menu = nullptr;
		if (obj == _sbLang)
			menu = _languageMenu;
		else if (obj == _sbEol)
			menu = _eolMenu;
		else if (obj == _sbEnc)
			menu = _encodingMenu;
		else if (obj == _sbIns)
		{
			_edit->ex(SCI_EDITTOGGLEOVERTYPE);
			updateStatusBar();
			return true;
		}
		if (menu)
		{
			menu->exec(static_cast<QWidget*>(obj)->mapToGlobal(me->position().toPoint()));
			return true;
		}
	}
	return QMainWindow::eventFilter(obj, event);
}

// ---------------------------------------------------------------------------
// context menus
// ---------------------------------------------------------------------------
QString MainWindow::contextMenuPath()
{
	return Npp::configDir() + QStringLiteral("/contextMenu.xml");
}

void MainWindow::loadContextMenu()
{
	// NppParameters::getContextMenuFromXmlTree(): ScintillaContextMenu of contextMenu.xml, read again when the file changes
	const QString path = contextMenuPath();
	const QFileInfo fi(path);
	const QDateTime stamp = fi.exists() ? fi.lastModified() : QDateTime();
	if (_contextMenuLoaded && stamp == _contextMenuStamp)
		return;
	_contextMenuLoaded = true;
	_contextMenuStamp = stamp;
	_contextMenuItems.clear();
	pugi::xml_document doc;
	bool ok = fi.exists() && doc.load_file(QFile::encodeName(path).constData());
	if (!ok)
		ok = doc.load_string(kContextMenuXmlDefault);
	if (!ok)
		return;
	const Localization& l10n = Localization::instance();
	for (pugi::xml_node n = doc.child("NotepadPlus").child("ScintillaContextMenu").child("Item"); n; n = n.next_sibling("Item"))
	{
		ContextMenuItem item;
		item.folder = QString::fromUtf8(n.attribute("FolderName").value());
		const QString translateId = QString::fromUtf8(n.attribute("TranslateID").value());
		if (!translateId.isEmpty() && !item.folder.isEmpty())
			item.folder = l10n.misc(translateId, item.folder);
		item.label = QString::fromUtf8(n.attribute("ItemNameAs").value());
		const int id = n.attribute("id").as_int(-1);
		if (id >= 0)
		{
			item.id = id;
		}
		else
		{
			const QString entry = QString::fromUtf8(n.attribute("MenuEntryName").value());
			const QString name = QString::fromUtf8(n.attribute("MenuItemName").value());
			if (entry.isEmpty() || name.isEmpty())
				continue; // PluginEntryName / PluginCommandItemName: no plugins in this port
			item.id = cmdIdFromMenuEntryItemName(entry, name);
			if (item.id == -1)
				continue;
		}
		_contextMenuItems.push_back(item);
	}
}

void MainWindow::showEditorContextMenu(const QPoint& globalPos)
{
	// ContextMenu::create(): the items of contextMenu.xml, FolderName as submenus, id 0 as separator
	loadContextMenu();
	QMenu menu(this);
	QHash<QString, QMenu*> folders;
	QString lastFolder;
	for (const ContextMenuItem& item : _contextMenuItems)
	{
		QMenu* target = &menu;
		if (!item.folder.isEmpty())
		{
			target = folders.value(item.folder);
			if (!target)
			{
				target = menu.addMenu(item.folder);
				folders.insert(item.folder, target);
			}
		}
		if (item.id == 0)
		{
			target->addSeparator();
			continue;
		}
		QAction* a = _actions.value(item.id);
		if (!a && item.id >= ID_MACRO && item.id < ID_MACRO_LIMIT)
		{
			for (QAction* m : std::as_const(_macroMenuItems))
			{
				if (m->data().toInt() == item.id)
					a = m;
			}
		}
		if (!a)
			continue;
		if (item.label.isEmpty())
		{
			target->addAction(a);
		}
		else
		{
			// ItemNameAs: the same command under another name
			QAction* named = target->addAction(item.label);
			named->setEnabled(a->isEnabled());
			named->setCheckable(a->isCheckable());
			named->setChecked(a->isChecked());
			connect(named, &QAction::triggered, a, &QAction::trigger);
		}
	}
	// WM_INITMENUPOPUP: plugins (spell checkers) add their items
	if (_pluginsManager)
		_pluginsManager->initMenuPopup(&menu);
	menu.exec(globalPos);
}

void MainWindow::editContextMenuFile()
{
	// IDM_SETTING_EDITCONTEXTMENU
	QMessageBox::information(this, QStringLiteral("Editing contextMenu"),
	                         QStringLiteral("Editing contextMenu.xml allows you to modify your Notepad++ popup context menu on edit zone.\n"
	                                        "The changes are used the next time the context menu opens."));
	const QString path = contextMenuPath();
	if (!QFileInfo::exists(path))
	{
		QDir().mkpath(QFileInfo(path).absolutePath());
		QFile f(path);
		if (f.open(QIODevice::WriteOnly))
			f.write(QByteArray(kContextMenuXmlDefault).replace("\r\n", "\n"));
	}
	if (Buffer* b = openFile(path))
		activateBuffer(b);
}

void MainWindow::showTabContextMenu(const QPoint& globalPos, int index)
{
	Q_UNUSED(index);
	QMenu menu(this);
	auto add = [&](int id)
	{
		if (QAction* a = _actions.value(id))
			menu.addAction(a);
	};
	add(IDM_FILE_CLOSE);
	add(IDM_FILE_CLOSEALL_BUT_CURRENT);
	add(IDM_FILE_CLOSEALL_TOLEFT);
	add(IDM_FILE_CLOSEALL_TORIGHT);
	add(IDM_FILE_CLOSEALL_UNCHANGED);
	add(IDM_FILE_SAVE);
	add(IDM_FILE_SAVEAS);
	menu.addSeparator();
	add(IDM_FILE_OPEN_FOLDER);
	add(IDM_FILE_OPEN_CMD);
	add(IDM_FILE_CONTAININGFOLDERASWORKSPACE);
	menu.addSeparator();
	add(IDM_FILE_RENAME);
	add(IDM_FILE_DELETE);
	add(IDM_FILE_RELOAD);
	add(IDM_FILE_PRINT);
	menu.addSeparator();
	add(IDM_EDIT_TOGGLEREADONLY);
	add(IDM_PINTAB);
	menu.addSeparator();
	add(IDM_EDIT_FULLPATHTOCLIP);
	add(IDM_EDIT_FILENAMETOCLIP);
	add(IDM_EDIT_CURRENTDIRTOCLIP);
	menu.addSeparator();
	add(IDM_VIEW_GOTO_ANOTHER_VIEW);
	add(IDM_VIEW_CLONE_TO_ANOTHER_VIEW);
	add(IDM_VIEW_GOTO_NEW_INSTANCE);
	add(IDM_VIEW_LOAD_IN_NEW_INSTANCE);
	menu.addSeparator();
	QMenu* colors = menu.addMenu(QStringLiteral("Apply Color to Tab"));
	for (int id : {IDM_VIEW_TAB_COLOUR_1, IDM_VIEW_TAB_COLOUR_2, IDM_VIEW_TAB_COLOUR_3, IDM_VIEW_TAB_COLOUR_4, IDM_VIEW_TAB_COLOUR_5, IDM_VIEW_TAB_COLOUR_NONE})
	{
		if (QAction* a = _actions.value(id))
			colors->addAction(a);
	}
	menu.exec(globalPos);
}

// ---------------------------------------------------------------------------
// docking panels
// ---------------------------------------------------------------------------
SearchResultsPanel* MainWindow::resultsPanel()
{
	if (!_results)
	{
		_resultsDock = new QDockWidget(QStringLiteral("Search results"), this);
		_resultsDock->setObjectName(QStringLiteral("searchResultsDock"));
		_results = new SearchResultsPanel(this, _resultsDock);
		_resultsDock->setWidget(_results);
		addDockWidget(Qt::BottomDockWidgetArea, _resultsDock);
	}
	_resultsDock->show();
	return _results;
}

void MainWindow::toggleSearchResults()
{
	if (_resultsDock && _resultsDock->isVisible())
		_resultsDock->hide();
	else
		resultsPanel();
}

void MainWindow::toggleWorkspace(const QString& folder)
{
	if (!_workspaceDock)
	{
		_workspaceDock = new QDockWidget(QStringLiteral("Folder as Workspace"), this);
		_workspaceDock->setObjectName(QStringLiteral("workspaceDock"));
		_workspaceModel = new QFileSystemModel(_workspaceDock);
		_workspaceModel->setReadOnly(true);
		_workspaceModel->setFilter(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::AllDirs | QDir::Hidden |
		                           (Npp::prefs().fawSymlinks ? QDir::Filters() : QDir::NoSymLinks));
		_workspaceTree = new QTreeView(_workspaceDock);
		_workspaceTree->setModel(_workspaceModel);
		for (int c = 1; c < _workspaceModel->columnCount(); ++c)
			_workspaceTree->hideColumn(c);
		_workspaceTree->header()->hide();
		applyPanelColours();
		_workspaceDock->setWidget(_workspaceTree);
		addDockWidget(Qt::LeftDockWidgetArea, _workspaceDock);
		connect(_workspaceTree, &QTreeView::activated, this, [this](const QModelIndex& idx)
		{
			if (!_workspaceModel->isDir(idx))
				openFile(_workspaceModel->filePath(idx));
		});
		connect(_workspaceDock, &QDockWidget::visibilityChanged, this, [this] { updateMenuState(); });
		const QString last = uiSettings().value(QStringLiteral("workspaceFolder")).toString();
		if (folder.isEmpty() && !last.isEmpty())
		{
			_workspaceModel->setRootPath(last);
			_workspaceTree->setRootIndex(_workspaceModel->index(last));
			_workspaceDock->setWindowTitle(QFileInfo(last).fileName());
		}
		_workspaceDock->hide();
	}
	if (!folder.isEmpty())
	{
		_workspaceModel->setRootPath(folder);
		_workspaceTree->setRootIndex(_workspaceModel->index(folder));
		_workspaceDock->setWindowTitle(QFileInfo(folder).fileName());
		uiSettings().setValue(QStringLiteral("workspaceFolder"), folder);
		_workspaceDock->show();
	}
	else
	{
		_workspaceDock->setVisible(!_workspaceDock->isVisible());
		if (_workspaceDock->isVisible() && _workspaceModel->rootPath().isEmpty())
		{
			const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("Select a folder to add in Folder as Workspace panel"), QDir::homePath());
			if (dir.isEmpty())
				_workspaceDock->hide();
			else
				toggleWorkspace(dir);
		}
	}
	updateMenuState();
}

void MainWindow::toggleDocumentList()
{
	if (!_docListDock)
	{
		_docListDock = new QDockWidget(QStringLiteral("Document List"), this);
		_docListDock->setObjectName(QStringLiteral("docListDock"));
		_docList = new QListWidget(_docListDock);
		_docListDock->setWidget(_docList);
		applyPanelColours();
		addDockWidget(Qt::LeftDockWidgetArea, _docListDock);
		connect(_docList, &QListWidget::currentRowChanged, this, [this](int row)
		{
			QListWidgetItem* item = _docList->item(row);
			if (!item)
				return;
			const int view = item->data(Qt::UserRole + 1).toInt();
			Buffer* b = reinterpret_cast<Buffer*>(item->data(Qt::UserRole).value<quintptr>());
			if (indexOf(view, b) >= 0 && (b != _current || view != _activeView))
				activateBuffer(b, view);
		});
		connect(_docListDock, &QDockWidget::visibilityChanged, this, [this] { updateMenuState(); });
		refreshDocumentList();
		_docListDock->hide();
	}
	_docListDock->setVisible(!_docListDock->isVisible());
	updateMenuState();
}

bool MainWindow::docMapShown() const
{
	return _docMapDock && _docMap && !_docMapDock->isHidden();
}

bool MainWindow::funcListShown() const
{
	return _funcListDock && _funcList && !_funcListDock->isHidden();
}

void MainWindow::toggleDocumentMap()
{
	// IDM_VIEW_DOC_MAP: Notepad_plus::launchDocMap()
	if (!_docMapDock)
	{
		_docMapDock = new QDockWidget(QStringLiteral("Document Map"), this);
		_docMapDock->setObjectName(QStringLiteral("docMapDock"));
		_docMap = new DocumentMap(this, _docMapDock);
		_docMapDock->setWidget(_docMap);
		addRightDock(_docMapDock);
		resizeDocks({_docMapDock}, {180}, Qt::Horizontal);
		connect(_docMapDock, &QDockWidget::visibilityChanged, this, [this](bool visible)
		{
			// DMN_SWITCHIN
			if (visible)
				_docMap->reloadMap(true);
			updateMenuState();
		});
		_docMapDock->hide();
	}
	if (docMapShown())
	{
		_docMapDock->hide();
	}
	else
	{
		_docMapDock->show();
		_docMapDock->raise();
		_docMap->reloadMap(true);
	}
	updateMenuState();
}

void MainWindow::toggleFunctionList(bool focus)
{
	// IDM_VIEW_FUNC_LIST / IDM_VIEW_SWITCHTO_FUNC_LIST: Notepad_plus::launchFunctionList()
	if (!_funcListDock)
	{
		_funcListDock = new QDockWidget(QStringLiteral("Function List"), this);
		_funcListDock->setObjectName(QStringLiteral("funcListDock"));
		_funcList = new FunctionListPanel(this, _funcListDock);
		_funcListDock->setWidget(_funcList);
		applyPanelColours();
		addRightDock(_funcListDock);
		resizeDocks({_funcListDock}, {220}, Qt::Horizontal);
		connect(_funcListDock, &QDockWidget::visibilityChanged, this, [this](bool visible)
		{
			if (visible)
				_funcList->reload();
			updateMenuState();
		});
		_funcListDock->hide();
	}
	if (funcListShown() && !focus)
	{
		_funcListDock->hide();
	}
	else
	{
		const bool wasShown = funcListShown();
		_funcListDock->show();
		_funcListDock->raise();
		if (!wasShown)
			_funcList->reload();
		if (focus)
			_funcList->tree()->setFocus();
	}
	updateMenuState();
}

void MainWindow::addRightDock(QDockWidget* dock)
{
	// the Notepad++ default container of the panels is on the right, the panels are its tabs
	addDockWidget(Qt::RightDockWidgetArea, dock);
	for (QDockWidget* other : {_funcListDock, _docMapDock, _charPanelDock, _clipboardDock})
	{
		if (other && other != dock && dockWidgetArea(other) == Qt::RightDockWidgetArea && !other->isFloating())
		{
			tabifyDockWidget(other, dock);
			break;
		}
	}
}

bool MainWindow::projectPanelShown(int index) const
{
	return index >= 0 && index < 3 && _projectDocks[index] && !_projectDocks[index]->isHidden();
}

void MainWindow::toggleProjectPanel(int index, bool focus)
{
	// IDM_VIEW_PROJECT_PANEL_1-3: Notepad_plus::launchProjectPanel()
	if (index < 0 || index > 2)
		return;
	const QString wsKey = QStringLiteral("projectPanel%1Workspace").arg(index + 1);
	if (!_projectDocks[index])
	{
		auto* dock = new QDockWidget(QStringLiteral("Project Panel %1").arg(index + 1), this);
		dock->setObjectName(QStringLiteral("projectPanel%1Dock").arg(index + 1));
		auto* panel = new ProjectPanel(this, index, dock);
		dock->setWidget(panel);
		addDockWidget(Qt::LeftDockWidgetArea, dock); // DWS_DF_CONT_LEFT
		for (QDockWidget* other : {_projectDocks[0], _projectDocks[1], _projectDocks[2]})
		{
			if (other && dockWidgetArea(other) == Qt::LeftDockWidgetArea && !other->isFloating())
			{
				tabifyDockWidget(other, dock);
				break;
			}
		}
		_projectDocks[index] = dock;
		_projectPanels[index] = panel;
		connect(panel, &ProjectPanel::openFileRequested, this, [this](const QString& path)
		{
			// NPPM_DOOPEN
			if (Buffer* b = openFile(path))
				activateBuffer(b);
		});
		connect(panel, &ProjectPanel::findInProjectsRequested, this, [this](int id)
		{
			// NPPM_INTERNAL_FINDINPROJECTS
			showFindDialog(FindReplaceDlg::FindInProjectsTab);
			if (_findDlg)
				_findDlg->setProjectPanels(1 << id);
		});
		connect(dock, &QDockWidget::visibilityChanged, this, [this] { updateMenuState(); });
		applyPanelColours();
		const QString ws = uiSettings().value(wsKey).toString();
		if (!ws.isEmpty() && !panel->openWorkSpace(ws, true))
			panel->newWorkSpace();
		dock->hide();
	}
	ProjectPanel* panel = _projectPanels[index];
	QDockWidget* dock = _projectDocks[index];
	if (!dock->isHidden() && !focus)
	{
		if (!panel->checkIfNeedSave())
			return;
		uiSettings().setValue(wsKey, panel->workSpaceFilePath());
		if (panel->isAncestorOf(QApplication::focusWidget()))
			_edit->setFocus();
		dock->hide();
	}
	else
	{
		if (dock->isHidden())
		{
			const QString ws = uiSettings().value(wsKey).toString();
			if (!ws.isEmpty())
				panel->openWorkSpace(ws, false);
		}
		dock->show();
		dock->raise();
		if (focus)
			panel->tree()->setFocus();
	}
	updateMenuState();
}

QStringList MainWindow::projectFiles(int panelMask, const QStringList& patterns) const
{
	QStringList files;
	for (int i = 0; i < 3; ++i)
	{
		if ((panelMask & (1 << i)) && _projectPanels[i])
			files << _projectPanels[i]->workSpaceFiles(patterns);
	}
	files.removeDuplicates();
	return files;
}

void MainWindow::toggleCharacterPanel()
{
	// IDM_EDIT_CHAR_PANEL: Notepad_plus::launchAnsiCharPanel()
	if (!_charPanelDock)
	{
		_charPanelDock = new QDockWidget(QStringLiteral("ASCII Codes Insertion Panel"), this);
		_charPanelDock->setObjectName(QStringLiteral("charPanelDock"));
		_charPanel = new CharacterPanel(this, _charPanelDock);
		_charPanelDock->setWidget(_charPanel);
		addRightDock(_charPanelDock);
		resizeDocks({_charPanelDock}, {360}, Qt::Horizontal);
		applyPanelColours();
		connect(_charPanelDock, &QDockWidget::visibilityChanged, this, [this](bool visible)
		{
			if (visible)
				_charPanel->switchEncoding();
			updateMenuState();
		});
		_charPanelDock->hide();
	}
	if (!_charPanelDock->isHidden())
	{
		_charPanelDock->hide();
	}
	else
	{
		_charPanelDock->show();
		_charPanelDock->raise();
		_charPanel->switchEncoding();
	}
	updateMenuState();
}

void MainWindow::toggleClipboardHistory()
{
	// IDM_EDIT_CLIPBOARDHISTORY_PANEL: Notepad_plus::launchClipboardHistoryPanel()
	if (!_clipboardDock)
	{
		_clipboardDock = new QDockWidget(QStringLiteral("Clipboard History"), this);
		_clipboardDock->setObjectName(QStringLiteral("clipboardHistoryDock"));
		_clipboardPanel = new ClipboardHistoryPanel(this, _clipboardDock);
		_clipboardDock->setWidget(_clipboardPanel);
		addRightDock(_clipboardDock);
		applyPanelColours();
		connect(_clipboardDock, &QDockWidget::visibilityChanged, this, [this] { updateMenuState(); });
		_clipboardDock->hide();
	}
	_clipboardDock->setVisible(_clipboardDock->isHidden());
	if (!_clipboardDock->isHidden())
		_clipboardDock->raise();
	updateMenuState();
}

void MainWindow::pasteSpecial(int id)
{
	EditView* e = _edit;
	QClipboard* cb = QApplication::clipboard();
	switch (id)
	{
		case IDM_EDIT_COPY_BINARY:
		case IDM_EDIT_CUT_BINARY:
		{
			// the selection with its NUL characters, and its length (CF_TEXT + CF_NPPTEXTLEN);
			// getSelectedTextToMultiChar() reads a text range: SCI_GETSELTEXT turns NUL into spaces
			if (e->ex(SCI_GETSELTEXT, 0, 0) <= 0)
				return;
			const QByteArray bytes = e->textRange(e->ex(SCI_GETSELECTIONSTART), e->ex(SCI_GETSELECTIONEND));
			if (bytes.isEmpty())
				return;
			auto* mime = new QMimeData;
			const qsizetype nul = bytes.indexOf('\0');
			mime->setText(QString::fromUtf8(nul >= 0 ? bytes.left(nul) : bytes));
			mime->setData(QString::fromLatin1(kNppBinaryMime), bytes);
			cb->setMimeData(mime);
			if (id == IDM_EDIT_CUT_BINARY)
				e->exs(SCI_REPLACESEL, 0, "");
			return;
		}
		case IDM_EDIT_PASTE_BINARY:
		{
			const QMimeData* mime = cb->mimeData();
			if (!mime)
				return;
			if (mime->hasFormat(QString::fromLatin1(kNppBinaryMime)))
			{
				const QByteArray bytes = mime->data(QString::fromLatin1(kNppBinaryMime));
				e->ex(SCI_BEGINUNDOACTION);
				e->exs(SCI_REPLACESEL, 0, "");
				e->exs(SCI_ADDTEXT, static_cast<uptr_t>(bytes.size()), bytes.constData());
				e->ex(SCI_ENDUNDOACTION);
			}
			else if (mime->hasText())
			{
				e->exs(SCI_REPLACESEL, 0, mime->text().toUtf8().constData());
			}
			return;
		}
		case IDM_EDIT_PASTE_AS_HTML:
		case IDM_EDIT_PASTE_AS_RTF:
		{
			// the HTML or RTF source of the clipboard (CF_HTML / CF_RTF)
			const QMimeData* mime = cb->mimeData();
			if (!mime)
				return;
			QByteArray data;
			if (id == IDM_EDIT_PASTE_AS_HTML)
			{
				if (mime->hasHtml())
					data = mime->html().toUtf8();
			}
			else
			{
				for (const char* type : {"text/rtf", "application/rtf", "text/richtext"})
				{
					if (mime->hasFormat(QString::fromLatin1(type)))
					{
						data = mime->data(QString::fromLatin1(type));
						break;
					}
				}
			}
			if (!data.isEmpty())
				e->exs(SCI_REPLACESEL, 0, data.constData());
			return;
		}
		default:
			return;
	}
}

void MainWindow::incrementalSearch()
{
	// IDM_SEARCH_FINDINCREMENT
	if (!_incFind)
	{
		_incFind = new IncrementalFindBar(this, centralWidget());
		placeIncrementalBar(_incFind->atTop());
		connect(_incFind, &IncrementalFindBar::positionToggled, this, &MainWindow::placeIncrementalBar);
	}
	QWidget* focus = QApplication::focusWidget();
	if (_incFind->isVisible() && focus && _incFind->isAncestorOf(focus))
	{
		// the accelerator of the bar: back to the field, or search again
		if (focus != _incFind->edit())
			_incFind->edit()->setFocus();
		else
			_incFind->findNext(true);
		return;
	}
	const QString sel = _edit->selectedText();
	if (!sel.isEmpty())
		_incFind->setSearchText(sel);
	_incFind->display(true);
}

void MainWindow::placeIncrementalBar(bool atTop)
{
	// NPPM_INTERNAL_INCSEARCHBARPOSITION
	_centralLayout->removeWidget(_incFind);
	_centralLayout->insertWidget(atTop ? 0 : _centralLayout->count(), _incFind);
}

void MainWindow::refreshPanels(bool forceFunctionList)
{
	if (_charPanel && !_charPanelDock->isHidden())
		_charPanel->switchEncoding(); // AnsiCharPanel::switchEncoding() on activation and encoding change
	// Notepad_plus::activateBuffer() / BufferChangeLanguage / after saving
	if (docMapShown())
		_docMap->reloadMap();
	if (funcListShown())
	{
		if (forceFunctionList || _funcList->needsReload())
			_funcList->reload();
		else
			_funcList->markEntry();
	}
}

void MainWindow::refreshDocumentList()
{
	if (!_docList)
		return;
	QSignalBlocker blocker(_docList);
	_docList->clear();
	// both views; "2" marks the documents of the second view, like the Document List of Notepad++
	const bool both = viewVisible(MAIN_VIEW) && viewVisible(SUB_VIEW);
	for (int v = MAIN_VIEW; v <= SUB_VIEW; ++v)
	{
		NppTabBar* tabs = _views[v].tabs;
		for (int i = 0; i < tabs->count(); ++i)
		{
			Buffer* b = bufferAt(v, i);
			const QString name = (both ? QStringLiteral("%1   ").arg(v + 1) : QString()) + b->displayName();
			auto* item = new QListWidgetItem(tabs->tabIcon(i), name, _docList);
			item->setToolTip(b->fullName());
			item->setData(Qt::UserRole, QVariant::fromValue(reinterpret_cast<quintptr>(b)));
			item->setData(Qt::UserRole + 1, v);
			if (v == _activeView && b == _current)
				_docList->setCurrentItem(item);
		}
	}
}

// ---------------------------------------------------------------------------
// Preferences applied to the window
// ---------------------------------------------------------------------------
void MainWindow::applyDialogFont()
{
	// General > Dialog Font (Notepad++ needs a restart too)
	const NppPrefs& p = Npp::prefs();
	if (p.dialogFontName.isEmpty() && p.dialogFontSize <= 0)
		return;
	QFont f = QApplication::font();
	if (!p.dialogFontName.isEmpty())
		f.setFamily(p.dialogFontName);
	if (p.dialogFontSize > 0)
		f.setPointSize(p.dialogFontSize);
	QApplication::setFont(f, "QDialog");
}

void MainWindow::applyMenuBarPrefs()
{
	const NppPrefs& p = Npp::prefs();
	if (!_menuShownByAlt || !p.hideMenuBar)
	{
		_menuShownByAlt = false;
		menuBar()->setVisible(!p.hideMenuBar && !_postIt && !_distractionFree);
	}

	// General > Menu > Hide right shortcuts + ▼ ✕
	if (!_menuCorner)
	{
		_menuCorner = new QWidget(menuBar());
		auto* l = new QHBoxLayout(_menuCorner);
		l->setContentsMargins(0, 0, 4, 0);
		l->setSpacing(0);
		auto make = [this, l](const QString& text, const QString& tip)
		{
			auto* b = new QToolButton(_menuCorner);
			b->setText(text);
			b->setToolTip(tip);
			b->setAutoRaise(true);
			b->setFocusPolicy(Qt::NoFocus);
			l->addWidget(b);
			return b;
		};
		QToolButton* plus = make(QStringLiteral("＋"), QStringLiteral("New"));
		QToolButton* list = make(QStringLiteral("▼"), QStringLiteral("List all documents"));
		QToolButton* cross = make(QStringLiteral("✕"), QStringLiteral("Close"));
		connect(plus, &QToolButton::clicked, this, [this] { newDocument(true); });
		connect(list, &QToolButton::clicked, this, [this, list] { showDocumentsMenu(list->mapToGlobal(QPoint(0, list->height()))); });
		connect(cross, &QToolButton::clicked, this, [this] { closeBuffer(_current, true); });
		menuBar()->setCornerWidget(_menuCorner, Qt::TopRightCorner);
	}
	_menuCorner->setVisible(!p.hideMenuRightShortcuts);
}

void MainWindow::applyToolbarIcons()
{
	// Preferences > Toolbar: icon set and Fluent icon colorization
	const NppPrefs& p = Npp::prefs();
	const bool dark = p.isDark();
	const int set = p.toolbarIconSet;
	const bool large = (set == TB_LARGE || set == TB_LARGE2);
	const QColor chosen = fluentColor(p.toolbarColor, p.toolbarCustomColor);
	const QString key = QStringLiteral("%1/%2/%3/%4").arg(dark).arg(set).arg(chosen.isValid() ? chosen.name() : QStringLiteral("-")).arg(p.toolbarMono);
	_toolbar->setIconSize(large ? QSize(32, 32) : QSize(16, 16));
	if (key == _toolbarIconKey)
		return;
	_toolbarIconKey = key;

	const QString theme = dark ? QStringLiteral("dark") : QStringLiteral("light");
	const QString variant = (set == TB_SMALL2 || set == TB_LARGE2) ? QStringLiteral("filled") : QStringLiteral("regular");
	const QColor mainColor = dark ? QColor(0xDE, 0xDE, 0xDE) : QColor(0x21, 0x21, 0x21);
	const QColor secondary = dark ? QColor(0x4C, 0xC2, 0xFF) : QColor(0x00, 0x78, 0xD4);
	QColor target = chosen;
	if (!target.isValid() && p.toolbarMono)
		target = mainColor; // default colour with complete colorization: monochrome

	for (const ToolbarButton& b : kToolbar)
	{
		if (!b.icon)
			continue;
		QAction* a = _actions.value(b.id);
		if (!a)
			continue;
		QIcon icon;
		if (set == TB_STANDARD)
		{
			QImage img(QStringLiteral(":/icons/standard/toolbar/%1.bmp").arg(QString::fromLatin1(b.standard)));
			if (!img.isNull())
			{
				img = img.convertToFormat(QImage::Format_ARGB32);
				const QRgb key3d = qRgb(0xC0, 0xC0, 0xC0); // ImageList mask colour
				for (int y = 0; y < img.height(); ++y)
				{
					auto* line = reinterpret_cast<QRgb*>(img.scanLine(y));
					for (int x = 0; x < img.width(); ++x)
					{
						if ((line[x] & 0xFFFFFF) == (key3d & 0xFFFFFF))
							line[x] = 0;
					}
				}
				icon.addPixmap(QPixmap::fromImage(img));
			}
		}
		else
		{
			const QString base = QStringLiteral(":/icons/%1/toolbar/%2/%3").arg(theme, variant, QString::fromLatin1(b.icon));
			const QIcon src(base + QStringLiteral("_off.ico"));
			if (target.isValid())
			{
				for (int sz : {16, 20, 24, 32, 40, 48, 64})
				{
					const QImage img = src.pixmap(QSize(sz, sz), 1.0).toImage();
					if (!img.isNull())
						icon.addPixmap(QPixmap::fromImage(recolorFluent(img, secondary, target, p.toolbarMono)));
				}
			}
			else
			{
				icon = src;
			}
			if (QFileInfo::exists(base + QStringLiteral("_dis.ico")))
				icon.addFile(base + QStringLiteral("_dis.ico"), QSize(), QIcon::Disabled);
		}
		a->setIcon(icon);
	}
}

void MainWindow::applyTabBarPrefs()
{
	const NppPrefs& p = Npp::prefs();
	for (DocView& dv : _views)
	{
		dv.tabs->setVisible(p.tabbarVisible && !_postIt && !_distractionFree);
		dv.tabs->setTabsClosable(p.tabCloseButton);
		dv.tabs->setMovable(!p.tabLock);
		dv.tabs->setShape(p.tabVertical ? QTabBar::RoundedWest : QTabBar::RoundedNorth);
		dv.layout->setDirection(p.tabVertical ? QBoxLayout::LeftToRight : QBoxLayout::TopToBottom);
		dv.tabs->setExpanding(false);
		dv.tabs->setUsesScrollButtons(true);
	}

	NppTabBar::Look look;
	look.reduce = p.tabReduce;
	look.activeBar = p.tabActiveBar;
	look.inactiveColor = p.tabInactiveColor;
	const bool dark = p.isDark();
	StyleArray& st = StyleManager::instance().globalStyles();
	auto valid = [](COLORREF c) { return c != static_cast<COLORREF>(STYLE_NOT_USED); };
	if (const Style* s = st.findByName(QStringLiteral("Active tab focused indicator")); s && valid(s->fgColor))
		look.activeBarColor = toQColor(s->fgColor);
	if (dark)
		look.inactiveBg = QColor(0, 0, 0);
	else if (const Style* s = st.findByName(QStringLiteral("Inactive tabs")); s && valid(s->bgColor))
		look.inactiveBg = toQColor(s->bgColor);
	static const QRgb lightDefaults[] = {0xF3F0CB, 0xDBF3CB, 0xCBDBF3, 0xF3DBCB, 0xF3CBEE};
	static const QRgb darkDefaults[] = {0x807848, 0x568048, 0x507094, 0x805048, 0x784880};
	for (int i = 0; i < 5; ++i)
	{
		const QString name = dark ? QStringLiteral("Tab color dark mode %1").arg(i + 1) : QStringLiteral("Tab color %1").arg(i + 1);
		const Style* s = st.findByName(name);
		look.tabColors[static_cast<size_t>(i)] = (s && valid(s->bgColor)) ? toQColor(s->bgColor) : QColor(dark ? darkDefaults[i] : lightDefaults[i]);
	}
	_tabLook = look;
	_tabUnfocusedColor = QColor(0xFF, 0xCA, 0xB0);
	if (const Style* s = st.findByName(QStringLiteral("Active tab unfocused indicator")); s && valid(s->fgColor))
		_tabUnfocusedColor = toQColor(s->fgColor);
	updateTabBarFocus();
}

LangType MainWindow::langOfFile(const QString& path, QString* udlName) const
{
	// Buffer::setFileName(): the extensions of the User Defined Languages come first
	const QFileInfo fi(path);
	if (udlName)
		udlName->clear();
	if (!fi.suffix().isEmpty())
	{
		const QString udl = UdlManager::instance().langNameFromExt(fi.suffix(), fi.fileName());
		if (!udl.isEmpty())
		{
			if (udlName)
				*udlName = udl;
			return L_USER;
		}
	}
	QString external;
	const LangType lang = StyleManager::instance().langFromFileName(path, &external);
	if (lang == L_EXTERNAL && udlName)
		*udlName = external; // the lexer of a lexer plugin
	return lang;
}

QString MainWindow::langDescription(const Buffer* b) const
{
	// Notepad_plus::getLangDesc()
	if (b->lang == L_USER && !b->udlName.isEmpty())
		return QStringLiteral("User Defined language file - ") + b->udlName;
	if (b->lang == L_EXTERNAL && !b->udlName.isEmpty())
		return b->udlName; // the name of the lexer plugin, like Notepad++
	return QString::fromUtf8(StyleManager::langInfo(b->lang).longName);
}

void MainWindow::showUdlDialog()
{
	if (!_udlDialog)
		_udlDialog = new UdlDialog(this);
	_udlDialog->showFor((_current && _current->lang == L_USER) ? _current->udlName : QString());
}

void MainWindow::udlChanged(const QString& name)
{
	for (int v = MAIN_VIEW; v <= SUB_VIEW; ++v)
	{
		Buffer* b = viewCurrent(v);
		if (b && b->lang == L_USER && b->udlName == name)
			_views[v].edit->defineDocType(L_USER, name);
	}
	if (docMapShown())
		_docMap->reloadMap(true);
}

void MainWindow::udlRenamed(const QString& oldName, const QString& newName)
{
	for (Buffer* b : allBuffers())
	{
		if (b->lang == L_USER && b->udlName == oldName)
			b->udlName = newName;
	}
	udlChanged(newName);
	updateStatusBar();
}

void MainWindow::udlRemoved(const QString& name)
{
	// WM_REMOVE_USERLANG
	for (Buffer* b : allBuffers())
	{
		if (b->lang == L_USER && b->udlName == name)
			b->udlName.clear();
	}
	udlChanged(QString());
	updateStatusBar();
}

void MainWindow::rebuildUdlMenu()
{
	// Notepad_plus::init(): the User Defined Languages before "User-Defined" in the Language menu
	if (!_languageMenu)
		return;
	for (QAction* a : _languageMenu->actions())
	{
		const int id = a->data().toInt();
		if (id > IDM_LANG_USER && id < IDM_LANG_USER_LIMIT)
		{
			_languageMenu->removeAction(a);
			_langGroup->removeAction(a);
			_handlers.remove(id);
			a->deleteLater();
		}
	}
	QAction* userDefined = _actions.value(IDM_LANG_USER);
	const auto& langs = UdlManager::instance().langs();
	const int count = std::min(static_cast<int>(langs.size()), IDM_LANG_USER_LIMIT - IDM_LANG_USER - 1);
	for (int i = 0; i < count; ++i)
	{
		const int id = IDM_LANG_USER + 1 + i;
		const QString name = langs[static_cast<size_t>(i)]->name;
		auto* a = new QAction(name, this);
		a->setData(id);
		a->setCheckable(true);
		_langGroup->addAction(a);
		_languageMenu->insertAction(userDefined, a);
		_handlers.insert(id, [this, name] { languageSet(L_USER, name); });
		connect(a, &QAction::triggered, this, [this, id] { command(id); });
	}
	if (_current)
		updateMenuState();
}

void MainWindow::addExternalLexerMenu()
{
	// Notepad_plus::init(): each lexer of the lexer plugins in the Language menu, in the
	// alphabetical order of its top level items
	if (!_languageMenu)
		return;
	const auto& lexers = StyleManager::instance().externalLexers();
	for (int i = 0; i < static_cast<int>(lexers.size()); ++i)
	{
		const int id = IDM_LANG_EXTERNAL + i;
		if (id > IDM_LANG_EXTERNAL_LIMIT || _actions.contains(id))
			continue;
		const QString name = lexers[static_cast<size_t>(i)].name;
		auto* a = new QAction(name, this);
		a->setData(id);
		a->setCheckable(true);
		_langGroup->addAction(a);
		QAction* before = nullptr;
		bool first = true;
		for (QAction* item : _languageMenu->actions())
		{
			const QString text = Npp::stripAmpersand(item->menu() ? item->menu()->title() : item->text());
			if (!first && !item->isSeparator() && text.compare(name, Qt::CaseInsensitive) > 0)
			{
				before = item;
				break;
			}
			first = false;
		}
		_languageMenu->insertAction(before, a);
		_actions.insert(id, a);
		_handlers.insert(id, [this, name] { languageSet(L_EXTERNAL, name); });
		connect(a, &QAction::triggered, this, [this, id] { command(id); });
	}
	if (_current)
		updateMenuState();
}

void MainWindow::applyLanguageMenu()
{
	// Preferences > Language: compact menu and hidden languages
	if (!_languageMenu)
		return;
	const NppPrefs& p = Npp::prefs();
	StyleManager& sm = StyleManager::instance();
	if (_letterMenus.isEmpty())
	{
		bool seenLetters = false;
		for (QAction* a : _languageMenu->actions())
		{
			if (a->menu() && Npp::stripAmpersand(a->menu()->title()).size() == 1)
			{
				_letterMenus << a->menu();
				seenLetters = true;
			}
			else if (seenLetters && a->isSeparator() && !_langMenuAnchor)
			{
				_langMenuAnchor = a;
			}
		}
	}

	auto langNameOf = [&sm](QAction* a) { return QString::fromUtf8(StyleManager::langInfo(sm.langFromMenuId(a->data().toInt())).langName); };
	QList<QAction*> lettered;
	for (QMenu* m : _letterMenus)
	{
		for (QAction* a : m->actions())
		{
			if (!a->menu() && !a->isSeparator())
				lettered << a;
		}
	}
	QList<QAction*> all = lettered;
	for (QAction* a : _languageMenu->actions())
	{
		const int id = a->data().toInt();
		if (!a->menu() && !a->isSeparator() && id != IDM_LANG_TEXT && sm.langFromMenuId(id) != L_TEXT && !all.contains(a))
			all << a;
	}
	const QStringList disabled = p.disabledLangList();
	for (QAction* a : all)
		a->setVisible(!disabled.contains(langNameOf(a)));

	for (QAction* a : _flatLangActions)
		_languageMenu->removeAction(a);
	_flatLangActions.clear();
	if (p.langMenuCompact)
	{
		for (QMenu* m : _letterMenus)
		{
			bool any = false;
			for (QAction* a : m->actions())
				any = any || a->isVisible();
			m->menuAction()->setVisible(any);
		}
	}
	else
	{
		for (QMenu* m : _letterMenus)
			m->menuAction()->setVisible(false);
		std::sort(lettered.begin(), lettered.end(), [](QAction* a, QAction* b)
		{
			return Npp::stripAmpersand(a->text()).compare(Npp::stripAmpersand(b->text()), Qt::CaseInsensitive) < 0;
		});
		for (QAction* a : lettered)
		{
			_languageMenu->insertAction(_langMenuAnchor, a);
			_flatLangActions << a;
		}
	}
}

void MainWindow::applyDarkMode()
{
	// Preferences > Dark Mode: palette of the chosen tone (NppDarkMode.cpp colours)
	static const QPalette systemPalette = QApplication::palette();
	const NppPrefs& p = Npp::prefs();
	const bool desktopDark = Npp::isDarkDesktop();
	QPalette palette;

	if (p.darkMode == DARKMODE_FOLLOW_SYSTEM && (!desktopDark || p.darkTone == 0))
	{
		palette = systemPalette;
	}
	else if (p.isDark())
	{
		const auto c = darkToneColors(p.darkTone, p.darkCustomColors);
		auto col = [&c](int i) { return QColor(c[static_cast<size_t>(i)]); };
		palette.setColor(QPalette::Window, col(3));
		palette.setColor(QPalette::WindowText, col(5));
		palette.setColor(QPalette::Base, col(1));
		palette.setColor(QPalette::AlternateBase, col(0));
		palette.setColor(QPalette::Text, col(5));
		palette.setColor(QPalette::PlaceholderText, col(7));
		palette.setColor(QPalette::Button, col(1));
		palette.setColor(QPalette::ButtonText, col(5));
		palette.setColor(QPalette::BrightText, col(4));
		palette.setColor(QPalette::Highlight, col(2).lighter(130));
		palette.setColor(QPalette::HighlightedText, col(5));
		palette.setColor(QPalette::Link, col(8));
		palette.setColor(QPalette::LinkVisited, col(8));
		palette.setColor(QPalette::ToolTipBase, col(1));
		palette.setColor(QPalette::ToolTipText, col(5));
		palette.setColor(QPalette::Light, col(10));
		palette.setColor(QPalette::Midlight, col(9));
		palette.setColor(QPalette::Mid, col(9));
		palette.setColor(QPalette::Dark, col(11));
		palette.setColor(QPalette::Shadow, col(0).darker(150));
		palette.setColor(QPalette::Accent, col(10));
		for (auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
			palette.setColor(QPalette::Disabled, role, col(7));
	}
	else
	{
		// On KDE, the active style's standard palette can also be dark. Keep
		// the desktop colours when they're light and use a light palette otherwise.
		const bool lightDesktopPalette = systemPalette.color(QPalette::Window).lightness() >= 128 &&
			systemPalette.color(QPalette::Base).lightness() >= 128;
		const QColor lightBackground(0xf0, 0xf0, 0xf0);
		palette = lightDesktopPalette ? systemPalette : QPalette(lightBackground, lightBackground);
	}

	QApplication::setPalette(palette);
	setPalette(palette);
	// Breeze applies the desktop header colours while polishing the main window.
	ensurePolished();
	QWidget* chrome[] = {menuBar(), _toolbar, statusBar(), _views[MAIN_VIEW].tabs, _views[SUB_VIEW].tabs};
	for (QWidget* widget : chrome)
	{
		if (widget)
			widget->setPalette(palette);
	}
	for (QMenu* menu : findChildren<QMenu*>())
		menu->setPalette(palette);
	for (QDockWidget* dock : findChildren<QDockWidget*>())
		dock->setPalette(palette);
	_toolbarIconKey.clear();
	if (_pluginsManager && _pluginsManager->isReady())
		_pluginsManager->notify(NPPN_DARKMODECHANGED, 0);
}

void MainWindow::applyTray()
{
	// MISC. > system tray
	const int action = Npp::prefs().systrayAction;
	if (action == SYSTRAY_NONE || !QSystemTrayIcon::isSystemTrayAvailable())
	{
		if (_tray)
			_tray->hide();
		return;
	}
	if (!_tray)
	{
		_tray = new QSystemTrayIcon(windowIcon(), this);
		_tray->setToolTip(Npp::appName());
		auto restore = [this]
		{
			showNormal();
			raise();
			activateWindow();
			if (_tray)
				_tray->hide();
		};
		auto* menu = new QMenu(this);
		menu->addAction(QStringLiteral("Activate"), this, restore);
		menu->addAction(QStringLiteral("New"), this, [this, restore] { restore(); newDocument(true); });
		menu->addAction(QStringLiteral("Open..."), this, [this, restore] { restore(); fileOpen(); });
		menu->addSeparator();
		menu->addAction(QStringLiteral("Exit"), this, [this]
		{
			_reallyQuit = true;
			close();
		});
		_tray->setContextMenu(menu);
		connect(_tray, &QSystemTrayIcon::activated, this, [restore](QSystemTrayIcon::ActivationReason reason)
		{
			if (reason == QSystemTrayIcon::Trigger || reason == QSystemTrayIcon::DoubleClick)
				restore();
		});
	}
	if (isVisible() && !isMinimized())
		_tray->hide();
}

void MainWindow::applyPanelsState()
{
	// panels open at the last exit; with -nosession or in multi-instance mode only the
	// ones of Preferences > Multi-Instance & Date > Panel State
	const NppPrefs& p = Npp::prefs();
	const bool normal = !_noSession && !_multiInst;
	if (uiSettings().value(QStringLiteral("docListVisible")).toBool() && (normal || p.keepStateDocList) && !(_docListDock && _docListDock->isVisible()))
		toggleDocumentList();
	const QString folder = uiSettings().value(QStringLiteral("workspaceFolder")).toString();
	if (uiSettings().value(QStringLiteral("workspaceVisible")).toBool() && (normal || p.keepStateFileBrowser) && !folder.isEmpty() &&
	    QFileInfo(folder).isDir() && !(_workspaceDock && _workspaceDock->isVisible()))
		toggleWorkspace(folder);
	if (uiSettings().value(QStringLiteral("funcListVisible")).toBool() && (normal || p.keepStateFuncList) && !funcListShown())
		toggleFunctionList();
	if (uiSettings().value(QStringLiteral("docMapVisible")).toBool() && (normal || p.keepStateDocMap) && !docMapShown())
		toggleDocumentMap();
	if (uiSettings().value(QStringLiteral("charPanelVisible")).toBool() && (normal || p.keepStateCharPanel) && !(_charPanelDock && !_charPanelDock->isHidden()))
		toggleCharacterPanel();
	for (int i = 0; i < 3; ++i)
	{
		if (uiSettings().value(QStringLiteral("projectPanel%1Visible").arg(i + 1)).toBool() && (normal || p.keepStateProjectPanel) && !projectPanelShown(i))
			toggleProjectPanel(i);
	}
	if (uiSettings().value(QStringLiteral("clipboardHistoryVisible")).toBool() && (normal || p.keepStateClipboardHistory) &&
	    !(_clipboardDock && !_clipboardDock->isHidden()))
		toggleClipboardHistory();
}

QString MainWindow::openSaveDir() const
{
	// Preferences > Default Directory
	const NppPrefs& p = Npp::prefs();
	const QString last = uiSettings().value(QStringLiteral("lastDir")).toString();
	switch (p.openSaveDir)
	{
		case DIR_FIXED:
		{
			QString dir = p.openSaveDirPath;
			if (dir.startsWith(QLatin1Char('~')))
				dir.replace(0, 1, QDir::homePath());
			if (!dir.isEmpty() && QFileInfo(dir).isDir())
				return dir;
			break;
		}
		case DIR_REMEMBER_LAST:
			if (!last.isEmpty() && QFileInfo(last).isDir())
				return last;
			break;
		default:
			if (_current && !_current->isUntitled())
				return QFileInfo(_current->path).absolutePath();
			break;
	}
	return (!last.isEmpty() && QFileInfo(last).isDir()) ? last : QDir::homePath();
}

void MainWindow::rememberOpenSaveDir(const QString& file)
{
	uiSettings().setValue(QStringLiteral("lastDir"), QFileInfo(file).absolutePath());
}

void MainWindow::backupOnSave(const QString& path)
{
	// Preferences > Backup > Backup on save (NppIO.cpp Notepad_plus::doSave)
	const NppPrefs& p = Npp::prefs();
	if (p.backupMode == BACKUP_NONE || !QFileInfo(path).isFile())
		return;
	const QFileInfo fi(path);
	QString dir;
	if (p.backupUseCustomDir && !p.backupDir.trimmed().isEmpty())
	{
		dir = p.backupDir.trimmed();
		if (dir.startsWith(QLatin1Char('~')))
			dir.replace(0, 1, QDir::homePath());
	}
	else
	{
		dir = fi.absolutePath();
		if (p.backupMode == BACKUP_VERBOSE)
			dir += QStringLiteral("/nppBackup");
	}
	QDir().mkpath(dir);
	QString target = dir + QLatin1Char('/') + fi.fileName();
	if (p.backupMode == BACKUP_SIMPLE)
		target += QStringLiteral(".bak");
	else
		target += QLatin1Char('.') + QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd_HHmmss")) + QStringLiteral(".bak");
	QFile::remove(target);
	QFile::copy(path, target);
}

void MainWindow::toggleDistractionFree()
{
	// View > Distraction Free Mode: full screen, no bars, text centred
	_distractionFree = !_distractionFree;
	if (_distractionFree)
	{
		_dfWasFullScreen = isFullScreen();
		if (!_dfWasFullScreen)
			showFullScreen();
	}
	else if (!_dfWasFullScreen)
	{
		showNormal();
	}
	applyPrefs();
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
	if (event)
		QMainWindow::resizeEvent(event);
	if (_distractionFree)
	{
		// ScintillaViewParams::getDistractionFreePadding()
		const int div = std::max(3, Npp::prefs().distractionFreeDivPart);
		for (DocView& dv : _views)
		{
			const int padding = dv.edit->width() / div;
			dv.edit->ex(SCI_SETMARGINLEFT, 0, padding);
			dv.edit->ex(SCI_SETMARGINRIGHT, 0, padding);
		}
	}
}

void MainWindow::showDocumentsMenu(const QPoint& globalPos)
{
	QMenu menu(this);
	for (int v = MAIN_VIEW; v <= SUB_VIEW; ++v)
	{
		if (v == SUB_VIEW && _views[v].tabs->count() > 0 && !menu.isEmpty())
			menu.addSeparator();
		for (int i = 0; i < _views[v].tabs->count(); ++i)
		{
			Buffer* b = bufferAt(v, i);
			QAction* a = menu.addAction(_views[v].tabs->tabIcon(i), b->displayName());
			a->setCheckable(true);
			a->setChecked(b == _current && v == _activeView);
			connect(a, &QAction::triggered, this, [this, b, v] { if (indexOf(v, b) >= 0) activateBuffer(b, v); });
		}
	}
	menu.exec(globalPos);
}

void MainWindow::documentSwitcher(bool forward)
{
	// MISC. > Document Switcher (Ctrl+Tab), MRU order when asked
	const NppPrefs& p = Npp::prefs();
	const int count = _tabs->count();
	if (count < 2)
		return;
	if (!p.docSwitcher)
	{
		const int cur = _tabs->currentIndex();
		activateBuffer(bufferAt((cur + (forward ? 1 : -1) + count) % count));
		return;
	}
	if (!_switcher)
	{
		_switcher = new QFrame(this, Qt::ToolTip | Qt::FramelessWindowHint);
		_switcher->setFrameShape(QFrame::StyledPanel);
		auto* l = new QVBoxLayout(_switcher);
		l->setContentsMargins(2, 2, 2, 2);
		_switcherList = new QListWidget(_switcher);
		_switcherList->setFocusPolicy(Qt::NoFocus);
		l->addWidget(_switcherList);
	}
	if (!_switcher->isVisible())
	{
		_switcherList->clear();
		std::vector<Buffer*> order;
		if (p.docSwitcherMru)
		{
			for (Buffer* b : _mru)
			{
				if (indexOf(b) >= 0)
					order.push_back(b);
			}
		}
		for (Buffer* b : allBuffers())
		{
			if (std::find(order.begin(), order.end(), b) == order.end())
				order.push_back(b);
		}
		if (!p.docSwitcherMru)
			std::rotate(order.begin(), order.begin() + std::max(0, _tabs->currentIndex()), order.end());
		for (Buffer* b : order)
		{
			auto* item = new QListWidgetItem(tabIconOf(b), b->displayName(), _switcherList);
			item->setData(Qt::UserRole, QVariant::fromValue(reinterpret_cast<quintptr>(b)));
			item->setToolTip(b->fullName());
		}
		const int rows = std::min(_switcherList->count(), 15);
		_switcher->resize(360, rows * (_switcherList->sizeHintForRow(0) + 2) + 12);
		_switcher->move(mapToGlobal(rect().center()) - QPoint(_switcher->width() / 2, _switcher->height() / 2));
		_switcherList->setCurrentRow(0);
		_switcher->show();
	}
	const int n = _switcherList->count();
	_switcherList->setCurrentRow((_switcherList->currentRow() + (forward ? 1 : -1) + n) % n);
}

void MainWindow::showTabPeek(int view, int index, const QPoint& globalPos)
{
	// MISC. > Document Peeker > Peek on tab
	Buffer* b = bufferAt(view, index);
	if (!b)
		return;
	if (!_peek)
	{
		_peek = new QFrame(this, Qt::ToolTip | Qt::FramelessWindowHint);
		_peek->setFrameShape(QFrame::Box);
		auto* l = new QVBoxLayout(_peek);
		l->setContentsMargins(1, 1, 1, 1);
		_peekView = new EditView(_peek, false);
		_peekView->setFocusPolicy(Qt::NoFocus);
		l->addWidget(_peekView);
		_peek->resize(480, 300);
	}
	_peekView->send(SCI_SETDOCPOINTER, 0, b->doc);
	_peekView->defineDocType(b->largeFile ? L_TEXT : b->lang, b->udlName);
	for (int m = 0; m < 5; ++m)
		_peekView->ex(SCI_SETMARGINWIDTHN, m, 0);
	_peekView->ex(SCI_SETZOOM, -2);
	_peekView->ex(SCI_SETHSCROLLBAR, false);
	_peekView->ex(SCI_SETVSCROLLBAR, false);
	_peekView->ex(SCI_SETFIRSTVISIBLELINE, _peekView->ex(SCI_VISIBLEFROMDOCLINE, b->pos[view].firstVisibleLine));
	_peek->move(globalPos + QPoint(8, 16));
	_peek->show();
}

void MainWindow::hideTabPeek()
{
	if (_peek && _peek->isVisible())
	{
		_peek->hide();
		_peekView->send(SCI_SETDOCPOINTER, 0, 0);
	}
}

void MainWindow::useFirstLineAsTabName(Buffer* buf)
{
	// NppIO.cpp Notepad_plus::useFirstLineAsTabName()
	if (!buf || buf != _current)
		return;
	QString name = QString::fromUtf8(_edit->lineText(0)).trimmed();
	static const QRegularExpression forbidden(QStringLiteral(R"([\\/:*?"<>|\t])"));
	name.remove(forbidden);
	name = name.left(255).trimmed();
	if (name.isEmpty() || name == buf->untitledName)
		return;
	for (Buffer* o : allBuffers())
	{
		if (o != buf && o->displayName() == name)
			return;
	}
	buf->untitledName = name;
	updateBufferTabs(buf);
	updateTitle();
	refreshDocumentList();
}

QString MainWindow::tabLabel(const Buffer* b) const
{
	// Tab Bar > Max. tab label length (PathCompactPathEx on the file name)
	QString name = b->displayName();
	const int maxLen = Npp::prefs().tabMaxLabelLen;
	if (maxLen > 0 && name.size() > maxLen)
		name = name.left(std::max(1, maxLen - 3)) + QStringLiteral("...");
	return name;
}

QString MainWindow::recentLabel(const QString& path) const
{
	// Recent Files History > Display
	const NppPrefs& p = Npp::prefs();
	switch (p.recentDisplay)
	{
		case RECENT_FILENAME_ONLY:
			return QFileInfo(path).fileName();
		case RECENT_CUSTOM_LENGTH:
		{
			const int len = p.recentCustomLength;
			if (path.size() <= len)
				return path;
			if (len <= 4)
				return path.left(len);
			const int right = (len - 3) / 2 + ((len - 3) % 2);
			const int left = len - 3 - right;
			return path.left(left) + QStringLiteral("...") + path.right(right);
		}
		default:
			return path;
	}
}

void MainWindow::loadPlugins(bool enabled)
{
	// Notepad_plus::init(): the plugins, their menus, then NPPN_TBMODIFICATION
	if (enabled)
	{
		_pluginsManager->loadPlugins();
		_pluginsManager->addMenus();
		addExternalLexerMenu();
	}
	_pluginsManager->notifyToolbar();
}

void MainWindow::pluginsReady(const QString& pluginMessage)
{
	_pluginsManager->notifyReady();
	if (!pluginMessage.isEmpty())
	{
		const std::wstring msg = pluginMessage.toStdWString();
		_pluginsManager->notify(NPPN_CMDLINEPLUGINMSG, reinterpret_cast<uptr_t>(msg.c_str()));
	}
}
