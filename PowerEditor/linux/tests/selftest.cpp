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

// Self test of the Linux front end. Drives the real MainWindow (offscreen
// platform) through file I/O in several encodings, editing commands, search,
// macros, sessions and themes. Uses a temporary XDG_CONFIG_HOME, so the user's
// settings are untouched.
//
//   QT_QPA_PLATFORM=offscreen ./build-linux/bin/npp_selftest [screenshot dir]

#include <algorithm>
#include <cstdio>
#include <functional>

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDialog>
#include <QDateTime>
#include <QDir>
#include <QEventLoop>
#include <QElapsedTimer>
#include <QPushButton>
#include <QFile>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QPlainTextEdit>
#include <QProxyStyle>
#include <QRadioButton>
#include <QSpinBox>
#include <QStatusBar>
#include <QStringEncoder>
#include <QTabBar>
#include <QTableWidget>
#include <QTableView>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <QTreeWidget>

#include "Dialogs.h"
#include "DocumentMap.h"
#include "EditPanels.h"
#include "EditView.h"
#include "FileIO.h"
#include "FindReplace.h"
#include "FunctionList.h"
#include "IncrementalFind.h"
#include "LangStyles.h"
#include "Localization.h"
#include "Macros.h"
#include "ProjectPanel.h"
#include "Udl.h"
#include "UdlDialog.h"
#include "MainWindow.h"
#include "../src/PluginsManager.h" // not the Windows one of the plugin SDK include path
#include "../plugins/NppConverter/resource.h"
#include "TestPlugin.h"

// the plugin interface, as plugins see it (Notepad_plus_msgs.h has its own LangType)
#include <QDockWidget>
#include <QToolBar>
#include <windows.h>
namespace sdk
{
#include "PluginInterface.h"
}
#undef MAIN_VIEW
#undef SUB_VIEW

struct MainWindowAccess
{
	static void backup(MainWindow& w) { w.backupSnapshot(); }
	static bool readSession(MainWindow& w, const QString& p) { return w.readSession(p); }
	static FindReplaceDlg* findDlg(MainWindow& w)
	{
		if (!w._findDlg)
			w._findDlg = new FindReplaceDlg(&w);
		return w._findDlg;
	}
	static int replaceAll(FindReplaceDlg* d, EditView* v, const FindOptions& o) { return d->replaceAllInView(v, o, false); }
	static int count(FindReplaceDlg* d, EditView* v, const FindOptions& o) { return d->countInView(v, o, false); }
	static void findInFiles(FindReplaceDlg* d, const QString& text, const QString& dir, const QString& filters)
	{
		d->_findCombo->setCurrentText(text);
		d->_dirCombo->setCurrentText(dir);
		d->_filtersCombo->setCurrentText(filters);
		d->onFindInFiles(false);
	}
	static QString resultsText(MainWindow& w) { return w._results ? QString::fromUtf8(w._results->view()->wholeText()) : QString(); }
	static QString statusEncoding(MainWindow& w) { return w._sbEnc->text(); }
	static QString statusEol(MainWindow& w) { return w._sbEol->text(); }
	static QString statusLang(MainWindow& w) { return w._sbLang->text(); }
	static void editAutoComplete(MainWindow& w) { w.editAutoComplete(false, true); }
	static void macroStart(MainWindow& w) { w.macroStart(); }
	static void macroStop(MainWindow& w) { w.macroStop(); }
	static void macroPlay(MainWindow& w, int n) { w.macroPlayback(n); }
	static QTabBar* tabs(MainWindow& w) { return w._tabs; }
	static QMenu* languageMenu(MainWindow& w) { return w._languageMenu; }
	static QString recentLabel(MainWindow& w, const QString& path) { return w.recentLabel(path); }
	static bool distractionFree(MainWindow& w) { return w._distractionFree; }
	static void writeSession(MainWindow& w, const QString& path) { w.writeSession(path, false); }
	static void syncScroll(MainWindow& w, int view) { w.syncScroll(view); }
	static void closeAll(MainWindow& w) { w.fileCloseAll(false, false, 0, false); }
	static QWidget* panel(MainWindow& w, int view) { return w._views[view].panel; }
	static DocumentMap* docMap(MainWindow& w) { return w._docMap; }
	static FunctionListPanel* funcList(MainWindow& w) { return w._funcList; }
	static CharacterPanel* charPanel(MainWindow& w) { return w._charPanel; }
	static ClipboardHistoryPanel* clipboardPanel(MainWindow& w) { return w._clipboardPanel; }
	static IncrementalFindBar* incFind(MainWindow& w) { return w._incFind; }
	static void reloadMacros(MainWindow& w)
	{
		w._savedMacros = Macros::load();
		w.rebuildMacroMenu();
	}
	static ProjectPanel* projectPanel(MainWindow& w, int i) { return w._projectPanels[i]; }
	static bool docMapShown(MainWindow& w) { return w.docMapShown(); }
	static bool funcListShown(MainWindow& w) { return w.funcListShown(); }
};

namespace
{
	int g_failures = 0;
	int g_checks = 0;
	QString g_shotDir;

	void check(bool ok, const char* what, const QString& detail = QString())
	{
		++g_checks;
		if (ok)
			std::printf("  ok    %s\n", what);
		else
		{
			++g_failures;
			std::printf("  FAIL  %s %s\n", what, qPrintable(detail));
		}
	}

	QByteArray readAll(const QString& path)
	{
		QFile f(path);
		if (!f.open(QIODevice::ReadOnly))
			return {};
		return f.readAll();
	}

	void writeAll(const QString& path, const QByteArray& data)
	{
		QFile f(path);
		if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
			f.write(data);
	}

	void settle()
	{
		for (int i = 0; i < 5; ++i)
			QApplication::processEvents();
	}

	bool waitFor(const std::function<bool()>& ready, int timeoutMs = 5000)
	{
		QElapsedTimer timer;
		timer.start();
		while (!ready() && !timer.hasExpired(timeoutMs))
		{
			QApplication::processEvents(QEventLoop::AllEvents, 20);
			QThread::msleep(1);
		}
		return ready();
	}

	void shot(QWidget* w, const QString& name)
	{
		if (g_shotDir.isEmpty() || !w)
			return;
		settle();
		w->grab().save(g_shotDir + QLatin1Char('/') + name + QStringLiteral(".png"));
	}

	// opens a modal dialog, grabs it and closes it
	void shotModal(const QString& name, const std::function<void()>& open)
	{
		bool grabbed = false;
		QTimer::singleShot(400, [&]
		{
			QWidget* w = QApplication::activeModalWidget();
			if (w)
			{
				shot(w, name);
				grabbed = true;
				if (auto* d = qobject_cast<QDialog*>(w))
					d->reject();
				else
					w->close();
			}
		});
		open();
		check(grabbed, qPrintable(QStringLiteral("dialog shown: ") + name));
	}

	QString docText(EditView* v)
	{
		return QString::fromUtf8(v->wholeText());
	}

	void setDoc(EditView* v, const QByteArray& text)
	{
		v->ex(SCI_SETTEXT, 0, reinterpret_cast<sptr_t>(text.constData()));
	}

	class DarkDesktopStyle : public QProxyStyle
	{
	public:
		QPalette standardPalette() const override
		{
			const QColor background(0x3b, 0x42, 0x52);
			return QPalette(background, background);
		}

		void polish(QWidget* widget) override
		{
			QProxyStyle::polish(widget);
			if (auto* window = qobject_cast<QMainWindow*>(widget))
			{
				auto* bar = window->menuBar();
				const QPalette desktop = standardPalette();
				QPalette menu = bar->palette();
				menu.setColor(QPalette::Window, desktop.color(QPalette::Window));
				menu.setColor(QPalette::WindowText, desktop.color(QPalette::WindowText));
				bar->setPalette(menu);
			}
		}
	};

	int testLightModeOnDarkDesktop()
	{
		if (QApplication::platformName() == QStringLiteral("offscreen") ||
			QApplication::palette().color(QPalette::Window).lightness() >= 128)
		{
			// Emulate Breeze on a dark desktop on the Qt offscreen platform.
			QApplication::setStyle(new DarkDesktopStyle);
			QApplication::setPalette(QApplication::style()->standardPalette());
		}
		const QPalette desktop = QApplication::palette();
		check(QApplication::style()->standardPalette().color(QPalette::Window).lightness() < 128,
			"appearance: the desktop style offers a dark standard palette");

		NppPrefs& prefs = Npp::prefs();
		prefs.darkMode = DARKMODE_FOLLOW_SYSTEM;
		MainWindow window;
		window.newDocument(true);
		window.show();
		window.command(IDM_VIEW_DOC_MAP);
		settle();
		check(window.menuBar()->palette().color(QPalette::Window).lightness() < 128, "appearance: Follow system uses the dark desktop palette");

		bool selectedLight = false;
		bool preferencesTurnedLight = false;
		QTimer::singleShot(0, &window, [&]
		{
			auto* dialog = qobject_cast<QDialog*>(QApplication::activeModalWidget());
			if (!dialog)
				return;
			if (auto* pages = dialog->findChild<QListWidget*>(QStringLiteral("prefsPageList")))
			{
				const auto matches = pages->findItems(QStringLiteral("Dark Mode"), Qt::MatchExactly);
				if (!matches.isEmpty())
					pages->setCurrentItem(matches.front());
			}
			for (auto* button : dialog->findChildren<QRadioButton*>())
			{
				if (button->text() == QStringLiteral("Light mode"))
				{
					button->click();
					selectedLight = button->isChecked();
					break;
				}
			}
			QTimer::singleShot(250, dialog, [&, dialog]
			{
				preferencesTurnedLight = dialog->palette().color(QPalette::Window).lightness() >= 128;
				dialog->accept();
			});
		});
		NppDialogs::preferences(&window);
		settle();
		check(selectedLight && prefs.darkMode == DARKMODE_LIGHT, "appearance: selecting Light mode updates preferences");
		check(preferencesTurnedLight, "appearance: the open Preferences dialog switches to light");
		const QPalette menu = window.menuBar()->palette();
		check(menu.color(QPalette::Window).lightness() >= 128 && menu.color(QPalette::WindowText).lightness() < 128,
			"appearance: Light mode gives menus a light background and dark text",
			menu.color(QPalette::Window).name() + QLatin1Char('/') + menu.color(QPalette::WindowText).name());
		auto* firstMenu = window.menuBar()->actions().value(0);
		if (firstMenu && firstMenu->menu())
			firstMenu->menu()->ensurePolished();
		check(firstMenu && firstMenu->menu() && firstMenu->menu()->palette().color(QPalette::Window).lightness() >= 128,
			"appearance: an existing popup menu uses the light palette");
		auto* toolbar = window.findChild<QToolBar*>();
		check(toolbar && toolbar->palette().color(QPalette::Window).lightness() >= 128,
			"appearance: the toolbar uses the light palette");
		auto* documentMap = window.findChild<DocumentMap*>();
		auto* dock = documentMap ? qobject_cast<QDockWidget*>(documentMap->parentWidget()) : nullptr;
		check(dock && dock->palette().color(QPalette::Window).lightness() >= 128 &&
			window.statusBar()->palette().color(QPalette::Window).lightness() >= 128 &&
			MainWindowAccess::tabs(window)->palette().color(QPalette::Window).lightness() >= 128,
			"appearance: existing tabs, the status bar and the dock switch to light");
		check(QApplication::palette().color(QPalette::Base).lightness() >= 128 && !prefs.isDark(),
			"appearance: Light mode updates the application palette");
		const COLORREF editorBackground = static_cast<COLORREF>(window.editView()->ex(SCI_STYLEGETBACK, STYLE_DEFAULT));
		check(toQColor(editorBackground).lightness() >= 128, "appearance: Light mode selects a light default editor theme");

		prefs.load();
		QApplication::setPalette(desktop);
		MainWindow reopened;
		reopened.newDocument(true);
		reopened.show();
		settle();
		auto* menuBar = reopened.menuBar();
		const QPalette startupMenu = menuBar->palette();
		check(prefs.darkMode == DARKMODE_LIGHT &&
			startupMenu.color(QPalette::Window).lightness() >= 128 &&
			startupMenu.color(QPalette::WindowText).lightness() < 128,
			"appearance: Light mode survives the first show of the menu bar",
			startupMenu.color(QPalette::Window).name() + QLatin1Char('/') + startupMenu.color(QPalette::WindowText).name());

		auto* fileMenu = menuBar->actions().value(0);
		const QRect labelRect = fileMenu ? menuBar->actionGeometry(fileMenu).adjusted(2, 3, -2, -3) : QRect();
		const QImage label = labelRect.isValid() ? menuBar->grab(labelRect).toImage() : QImage();
		int darkPixels = 0;
		int lightPixels = 0;
		for (int y = 0; y < label.height(); ++y)
			for (int x = 0; x < label.width(); ++x)
			{
				const int lightness = qGray(label.pixel(x, y));
				darkPixels += lightness < 100;
				lightPixels += lightness >= 200;
			}
		check(menuBar->isVisible() && darkPixels >= 5 && lightPixels > label.width() * label.height() / 2,
			"appearance: the File menu label renders in dark text on a light background after startup");

		prefs.darkMode = DARKMODE_FOLLOW_SYSTEM;
		reopened.applyDarkMode();
		check(QApplication::palette().color(QPalette::Window) == desktop.color(QPalette::Window),
			"appearance: switching back restores the desktop palette");
		std::printf("\n%d checks, %d failed\n", g_checks, g_failures);
		return g_failures ? 1 : 0;
	}
}

int main(int argc, char** argv)
{
	QTemporaryDir config;
	QTemporaryDir work;
	qputenv("XDG_CONFIG_HOME", config.path().toLocal8Bit());
	qputenv("XDG_DATA_HOME", (config.path() + QStringLiteral("/data")).toLocal8Bit()); // no plugin of the user
	if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
		qputenv("QT_QPA_PLATFORM", "offscreen");

	QApplication::setApplicationName(QStringLiteral("notepad++"));
	QApplication app(argc, argv);
	const bool lightModeTest = app.arguments().contains(QStringLiteral("--test-light-mode"));
	if (argc > 1 && !lightModeTest)
	{
		g_shotDir = QString::fromLocal8Bit(argv[1]);
		QDir().mkpath(g_shotDir);
	}

	NppPrefs& prefs = Npp::prefs();
	prefs.load();
	Localization::instance().load(prefs.localization);
	StyleManager::instance().loadLangs();
	if (lightModeTest)
		return testLightModeOnDarkDesktop();
	std::printf("data dir: %s\nconfig dir: %s\n", qPrintable(Npp::dataDir()), qPrintable(Npp::configDir()));

	// the plugins of the build tree and the plugin of the self test
	qputenv("NPP_PLUGINS_DIR", QDir::cleanPath(QCoreApplication::applicationDirPath() + QStringLiteral("/../test-plugins")).toLocal8Bit());
	auto* w = new MainWindow;
	w->resize(1100, 720);
	w->loadPlugins(true);
	w->restoreSession();
	w->show();
	w->pluginsReady(QString());
	settle();
	EditView* v = w->editView();

	std::printf("[open & save UTF-8 C++ file]\n");
	{
		const QString path = work.filePath(QStringLiteral("hello.cpp"));
		writeAll(path, "int main()\n{\n\treturn 0;\n}\n");
		Buffer* b = w->openFile(path);
		check(b != nullptr, "file opened");
		check(b && b->lang == L_CPP, "language detected as C++");
		check(b && b->unicode == UniMode::uniCookie, "encoding UTF-8");
		check(MainWindowAccess::statusEol(*w) == QStringLiteral("Unix (LF)"), "status bar EOL", MainWindowAccess::statusEol(*w));
		check(MainWindowAccess::statusLang(*w) == QStringLiteral("C++ source file"), "status bar language", MainWindowAccess::statusLang(*w));
		char lexer[64] = {};
		v->ex(SCI_GETLEXERLANGUAGE, 0, reinterpret_cast<sptr_t>(lexer));
		check(QByteArray(lexer) == "cpp", "C++ lexer active", QString::fromLatin1(lexer));
		check(w->bufferAt(0) == b, "replaced the empty new 1 document");
		v->ex(SCI_INSERTTEXT, 0, reinterpret_cast<sptr_t>("// hi\n"));
		check(b->dirty, "dirty after edit");
		w->saveBuffer(b);
		check(!b->dirty, "clean after save");
		check(readAll(path) == "// hi\nint main()\n{\n\treturn 0;\n}\n", "saved bytes");
		shot(w, QStringLiteral("01-main-cpp"));

		// comment toggle (Ctrl+Q) on line 2
		v->ex(SCI_GOTOLINE, 1);
		w->command(IDM_EDIT_BLOCK_COMMENT);
		check(QString::fromUtf8(v->lineText(1)) == QStringLiteral("// int main()"), "toggle comment adds //", QString::fromUtf8(v->lineText(1)));
		w->command(IDM_EDIT_BLOCK_COMMENT);
		check(QString::fromUtf8(v->lineText(1)) == QStringLiteral("int main()"), "toggle comment removes //", QString::fromUtf8(v->lineText(1)));

		// bookmarks
		v->ex(SCI_GOTOLINE, 3);
		w->command(IDM_SEARCH_TOGGLE_BOOKMARK);
		v->ex(SCI_GOTOLINE, 0);
		w->command(IDM_SEARCH_NEXT_BOOKMARK);
		check(v->ex(SCI_LINEFROMPOSITION, v->ex(SCI_GETCURRENTPOS)) == 3, "next bookmark");

		// language menu
		w->command(StyleManager::instance().menuIdFromLang(L_PYTHON));
		check(b->lang == L_PYTHON, "language changed to Python");
		w->command(StyleManager::instance().menuIdFromLang(L_CPP));
		w->closeBuffer(b, false);
	}

	std::printf("[Windows-1251 file]\n");
	{
		const QString text = QString::fromUtf8(
			"Съешь же ещё этих мягких французских булок, да выпей чаю.\n"
			"В чащах юга жил бы цитрус? Да, но фальшивый экземпляр!\n"
			"Широкая электрификация южных губерний даст мощный толчок подъёму сельского хозяйства.\n");
		const QByteArray raw = FileIO::fromUtf8(text.toUtf8(), 1251, nullptr);
		const QString path = work.filePath(QStringLiteral("cyr.txt"));
		writeAll(path, raw);
		Buffer* b = w->openFile(path);
		check(b && b->unicode == UniMode::uni8Bit && b->codepage == 1251, "detected Windows-1251",
		      b ? QStringLiteral("unicode=%1 cp=%2").arg(static_cast<int>(b->unicode)).arg(b->codepage) : QString());
		check(docText(v) == text, "decoded text");
		check(MainWindowAccess::statusEncoding(*w) == QStringLiteral("Windows-1251"), "status bar encoding", MainWindowAccess::statusEncoding(*w));
		w->saveBuffer(b);
		check(readAll(path) == raw, "saved back unchanged");
		w->closeBuffer(b, false);
	}

	std::printf("[UTF-16 LE BOM file]\n");
	{
		const QString text = QString::fromUtf8("ăé€ 😀 Ἀλφα\r\nsecond line\r\n");
		QStringEncoder enc(QStringEncoder::Utf16LE);
		const QByteArray raw = QByteArray("\xFF\xFE", 2) + QByteArray(enc.encode(text));
		const QString path = work.filePath(QStringLiteral("u16.txt"));
		writeAll(path, raw);
		Buffer* b = w->openFile(path);
		check(b && b->unicode == UniMode::uni16LE, "detected UTF-16 LE BOM");
		check(docText(v) == text, "decoded text");
		check(b && b->eol == SC_EOL_CRLF, "EOL detected CRLF");
		w->saveBuffer(b);
		check(readAll(path) == raw, "saved back unchanged");

		w->command(IDM_FORMAT_CONV2_UTF_8);
		check(b->dirty, "convert marks dirty");
		w->saveBuffer(b);
		check(readAll(path) == QByteArray("\xEF\xBB\xBF", 3) + text.toUtf8(), "converted to UTF-8 BOM");

		w->command(IDM_FORMAT_TOUNIX);
		w->saveBuffer(b);
		check(!readAll(path).contains('\r'), "EOL converted to LF");
		w->closeBuffer(b, false);
	}

	std::printf("[editing commands]\n");
	{
		Buffer* b = w->newDocument(true);
		setDoc(v, "pear\napple   \nfig\napple   \n");
		w->command(IDM_EDIT_SORTLINES_LEXICOGRAPHIC_ASCENDING);
		check(docText(v) == QStringLiteral("apple   \napple   \nfig\npear\n"), "sort lines", docText(v));
		w->command(IDM_EDIT_REMOVE_ANY_DUP_LINES);
		check(docText(v) == QStringLiteral("apple   \nfig\npear\n"), "remove duplicate lines", docText(v));
		w->command(IDM_EDIT_TRIMTRAILING);
		check(docText(v) == QStringLiteral("apple\nfig\npear\n"), "trim trailing space", docText(v));
		w->command(IDM_EDIT_SORTLINES_LEXICOGRAPHIC_DESCENDING);
		check(docText(v) == QStringLiteral("pear\nfig\napple\n"), "sort descending", docText(v));

		setDoc(v, "10\n9\n100\nx\n");
		w->command(IDM_EDIT_SORTLINES_INTEGER_ASCENDING);
		check(docText(v) == QStringLiteral("x\n9\n10\n100\n"), "integer sort", docText(v));

		setDoc(v, "hello wORLD");
		v->ex(SCI_SELECTALL);
		w->command(IDM_EDIT_PROPERCASE_FORCE);
		check(docText(v) == QStringLiteral("Hello World"), "proper case", docText(v));
		v->ex(SCI_SELECTALL);
		w->command(IDM_EDIT_UPPERCASE);
		check(docText(v) == QStringLiteral("HELLO WORLD"), "upper case", docText(v));

		setDoc(v, "a\tb\n");
		w->command(IDM_EDIT_TAB2SW);
		check(docText(v) == QStringLiteral("a   b\n"), "tab to space", docText(v));

		setDoc(v, "line\n");
		v->ex(SCI_GOTOPOS, 0);
		w->command(IDM_EDIT_DUP_LINE);
		check(docText(v) == QStringLiteral("line\nline\n"), "duplicate line", docText(v));

		// macro: record typing "ab", play it back once
		setDoc(v, "");
		MainWindowAccess::macroStart(*w);
		v->ex(SCI_REPLACESEL, 0, reinterpret_cast<sptr_t>("ab"));
		MainWindowAccess::macroStop(*w);
		MainWindowAccess::macroPlay(*w, 2);
		check(docText(v) == QStringLiteral("ababab"), "macro playback", docText(v));

		// auto-completion from the C++ API file
		w->command(StyleManager::instance().menuIdFromLang(L_CPP));
		setDoc(v, "pri");
		v->ex(SCI_DOCUMENTEND);
		MainWindowAccess::editAutoComplete(*w);
		check(v->ex(SCI_AUTOCACTIVE) != 0, "auto-completion list shown");
		shot(w, QStringLiteral("02-autocomplete"));
		v->ex(SCI_AUTOCCANCEL);
		w->closeBuffer(b, false);
	}

	std::printf("[search & replace]\n");
	{
		Buffer* b = w->newDocument(true);
		FindReplaceDlg* dlg = MainWindowAccess::findDlg(*w);
		setDoc(v, "foo bar foo\njohn@example alice@site\nTAB\there\n");
		FindOptions o;
		o.findText = QStringLiteral("foo");
		check(MainWindowAccess::count(dlg, v, o) == 2, "count");
		o.replaceText = QStringLiteral("baz");
		check(MainWindowAccess::replaceAll(dlg, v, o) == 2, "replace all normal");
		o = FindOptions();
		o.searchType = FindRegex;
		o.findText = QStringLiteral("(\\w+)@(\\w+)");
		o.replaceText = QStringLiteral("$2:$1");
		check(MainWindowAccess::replaceAll(dlg, v, o) == 2, "replace all regex");
		check(docText(v).contains(QStringLiteral("example:john site:alice")), "regex back-references", docText(v));
		o = FindOptions();
		o.searchType = FindExtended;
		o.findText = QStringLiteral("TAB\\t");
		o.replaceText = QStringLiteral("T\\n");
		check(MainWindowAccess::replaceAll(dlg, v, o) == 1, "replace all extended");
		check(docText(v).contains(QStringLiteral("T\nhere")), "extended escapes", docText(v));

		v->ex(SCI_GOTOPOS, 0);
		o = FindOptions();
		o.findText = QStringLiteral("baz");
		check(dlg->find(o, true, false), "find next");
		check(v->ex(SCI_GETSELECTIONSTART) == 0 && v->ex(SCI_GETSELECTIONEND) == 3, "find selects the match");
		check(dlg->find(o, true, false) && v->ex(SCI_GETSELECTIONSTART) == 8, "find next again");
		check(dlg->find(o, true, false) && v->ex(SCI_GETSELECTIONSTART) == 0, "wrap around");

		dlg->showTab(FindReplaceDlg::FindTab);
		shot(dlg, QStringLiteral("10-find"));
		dlg->showTab(FindReplaceDlg::ReplaceTab);
		shot(dlg, QStringLiteral("11-replace"));
		dlg->showTab(FindReplaceDlg::FindInFilesTab);
		shot(dlg, QStringLiteral("12-find-in-files"));
		dlg->showTab(FindReplaceDlg::MarkTab);
		shot(dlg, QStringLiteral("13-mark"));
		dlg->hide();

		// find in files
		QDir(work.path()).mkpath(QStringLiteral("fif/sub"));
		writeAll(work.filePath(QStringLiteral("fif/a.txt")), "one needle\ntwo\n");
		writeAll(work.filePath(QStringLiteral("fif/sub/b.cpp")), "needle needle\n");
		writeAll(work.filePath(QStringLiteral("fif/sub/c.bin")), QByteArray("needle\0\0\0", 9));
		MainWindowAccess::findInFiles(dlg, QStringLiteral("needle"), work.filePath(QStringLiteral("fif")), QStringLiteral("*.*"));
		const QString results = MainWindowAccess::resultsText(*w);
		check(results.contains(QStringLiteral("Search \"needle\" (3 hits in 2 files of 3 searched)")), "find in files header", results.section(QLatin1Char('\n'), 0, 0));
		check(results.contains(QStringLiteral("\tLine 1: needle needle")), "find in files result line");
		shot(w, QStringLiteral("03-search-results"));
		w->closeBuffer(b, false);
	}

	std::printf("[themes]\n");
	{
		const QString monokai = Npp::dataDir() + QStringLiteral("/themes/Monokai.xml");
		w->applyTheme(monokai);
		const Style* def = StyleManager::instance().globalStyles().findByID(STYLE_DEFAULT);
		check(def && static_cast<COLORREF>(v->ex(SCI_STYLEGETBACK, STYLE_DEFAULT)) == def->bgColor, "theme default background applied");
		shot(w, QStringLiteral("04-monokai"));
		w->applyTheme(QString());
	}

	std::printf("[session snapshot]\n");
	{
		Buffer* b = w->newDocument(true);
		setDoc(v, "unsaved stuff");
		const QString name = b->untitledName;
		MainWindowAccess::backup(*w);
		auto* w2 = new MainWindow;
		const bool restored = MainWindowAccess::readSession(*w2, Npp::configDir() + QStringLiteral("/session.xml"));
		check(restored, "session read");
		Buffer* found = nullptr;
		for (Buffer* x : w2->allBuffers())
		{
			if (x->untitledName == name)
				found = x;
		}
		check(found != nullptr, "unsaved document restored");
		check(found && w2->bufferText(found) == "unsaved stuff", "restored content");
		check(found && found->dirty, "restored document is dirty");
		for (Buffer* x : w2->allBuffers())
			w2->closeBuffer(x, false);
		delete w2;
		w->closeBuffer(b, false);
	}

	std::printf("[preferences]\n");
	{
		// every page of the dialog, and a change applied while the dialog is open
		int pages = 0;
		bool statusHidden = false;
		QTimer::singleShot(400, [&]
		{
			QWidget* d = QApplication::activeModalWidget();
			auto* pageList = d ? d->findChild<QListWidget*>(QStringLiteral("prefsPageList")) : nullptr;
			if (!pageList)
				return;
			pages = pageList->count();
			for (int i = 0; i < pageList->count(); ++i)
			{
				pageList->setCurrentRow(i);
				shot(d, QStringLiteral("30-pref-%1-%2").arg(i + 1, 2, 10, QLatin1Char('0')).arg(pageList->item(i)->data(Qt::UserRole).toString()));
			}
			if (QWidget* general = d->findChild<QWidget*>(QStringLiteral("prefsPage_Global")))
			{
				for (QCheckBox* c : general->findChildren<QCheckBox*>())
				{
					if (c->text() == QStringLiteral("Hide") && !c->isChecked())
					{
						c->setChecked(true); // Status Bar > Hide
						break;
					}
				}
			}
			QTimer::singleShot(400, [&, d]
			{
				statusHidden = !w->statusBar()->isVisible();
				static_cast<QDialog*>(d)->reject();
			});
		});
		NppDialogs::preferences(w);
		check(pages == 24, "preferences: 24 pages", QString::number(pages));
		check(statusHidden, "preferences: change applied while the dialog is open");
		prefs.statusbarVisible = true;

		Buffer* b = w->openFile(work.filePath(QStringLiteral("hello.cpp")));
		auto apply = [&] { w->applyPrefs(); settle(); };

		prefs.caretWidth = 4;
		prefs.currentLineMode = LINEHILITE_FRAME;
		prefs.currentLineFrameWidth = 3;
		prefs.lineWrapMethod = LINEWRAP_ALIGNED;
		prefs.paddingLeft = 5;
		apply();
		check(v->ex(SCI_GETCARETSTYLE) == CARETSTYLE_BLOCK, "editing 1: block caret");
		check(v->ex(SCI_GETCARETLINEFRAME) == 3, "editing 1: current line frame width");
		check(v->ex(SCI_GETWRAPINDENTMODE) == SC_WRAPINDENT_SAME, "editing 1: aligned line wrap");
		check(v->ex(SCI_GETMARGINLEFT) == 5, "margins: left padding");

		prefs.edgeColumns = QStringLiteral("80 120");
		apply();
		check(v->ex(SCI_GETEDGEMODE) == EDGE_MULTILINE, "margins: several vertical edges");
		prefs.edgeColumns = QStringLiteral("100");
		prefs.edgeBgMode = true;
		apply();
		check(v->ex(SCI_GETEDGEMODE) == EDGE_BACKGROUND && v->ex(SCI_GETEDGECOLUMN) == 100, "margins: edge background mode");

		prefs.wordCharsDefault = false;
		prefs.customWordChars = QStringLiteral("-");
		apply();
		setDoc(v, "foo-bar baz");
		check(v->ex(SCI_WORDENDPOSITION, 0, true) == 7, "delimiter: custom word character");
		setDoc(v, "call(a, (b), c) end");
		v->selectBetweenDelimiters(6);
		check(v->selectedText() == QStringLiteral("a, (b), c"), "delimiter: Ctrl+double click selection", v->selectedText());

		prefs.showNpc = true;
		apply();
		char rep[16] = {};
		v->ex(SCI_GETREPRESENTATION, reinterpret_cast<uptr_t>("\xC2\xA0"), reinterpret_cast<sptr_t>(rep));
		check(QByteArray(rep) == "NBSP", "view: non-printing characters", QString::fromLatin1(rep));
		prefs.npcMode = NPC_CODEPOINT;
		apply();
		std::fill(std::begin(rep), std::end(rep), '\0');
		v->ex(SCI_GETREPRESENTATION, reinterpret_cast<uptr_t>("\xC2\xA0"), reinterpret_cast<sptr_t>(rep));
		check(QByteArray(rep) == "U+00A0", "editing 2: codepoint appearance", QString::fromLatin1(rep));

		prefs.autoPairParentheses = true;
		setDoc(v, "f(");
		v->ex(SCI_GOTOPOS, 2);
		v->insertMatchedChars('(');
		check(docText(v) == QStringLiteral("f()"), "auto-insert: parenthesis", docText(v));
		prefs.matchedPairs = QStringLiteral("<>");
		setDoc(v, "a<");
		v->ex(SCI_GOTOPOS, 2);
		v->insertMatchedChars('<');
		check(docText(v) == QStringLiteral("a<>"), "auto-insert: matched pair 1", docText(v));

		Buffer* html = w->newDocument(true);
		w->command(IDM_LANG_HTML);
		prefs.autoPairHtmlXmlTag = true;
		setDoc(v, "<div>");
		v->ex(SCI_COLOURISE, 0, -1);
		v->ex(SCI_GOTOPOS, 5);
		v->insertMatchedChars('>');
		check(docText(v) == QStringLiteral("<div></div>"), "auto-insert: html close tag", docText(v));
		setDoc(v, "<a><b>x</b></a>");
		v->ex(SCI_COLOURISE, 0, -1);
		v->ex(SCI_GOTOPOS, 4);
		v->doTagMatch();
		check(v->ex(SCI_INDICATORVALUEAT, SCE_UNIVERSAL_TAGMATCH, 3) != 0 && v->ex(SCI_INDICATORVALUEAT, SCE_UNIVERSAL_TAGMATCH, 8) != 0,
		      "highlighting: matching tags");
		w->closeBuffer(html, false);
		w->activateBuffer(b);

		prefs.tabVertical = true;
		prefs.tabMaxLabelLen = 5;
		apply();
		QTabBar* tabBar = MainWindowAccess::tabs(*w);
		check(tabBar->shape() == QTabBar::RoundedWest, "tab bar: vertical");
		check(tabBar->tabText(w->indexOf(b)) == QStringLiteral("he..."), "tab bar: max. label length", tabBar->tabText(w->indexOf(b)));
		prefs.tabVertical = false;
		prefs.tabMaxLabelLen = 0;

		prefs.hideMenuBar = true;
		apply();
		check(!w->menuBar()->isVisible(), "general: hidden menu bar");
		prefs.hideMenuBar = false;

		prefs.langMenuCompact = false;
		prefs.disabledLangs = QStringLiteral("cobol");
		apply();
		QMenu* langMenu = MainWindowAccess::languageMenu(*w);
		check(langMenu && langMenu->actions().contains(w->action(IDM_LANG_CPP)), "language: menu not compact");
		check(!w->action(IDM_LANG_COBOL)->isVisible(), "language: disabled language hidden");
		prefs.langMenuCompact = true;
		prefs.disabledLangs.clear();
		apply();
		check(!langMenu->actions().contains(w->action(IDM_LANG_CPP)) && w->action(IDM_LANG_COBOL)->isVisible(), "language: compact menu restored");

		prefs.toolbarIconSet = TB_STANDARD;
		apply();
		check(!w->action(IDM_FILE_NEW)->icon().isNull(), "toolbar: standard icons");
		prefs.toolbarIconSet = TB_SMALL;
		prefs.toolbarColor = static_cast<int>(FluentColor::red);
		prefs.toolbarMono = true;
		apply();
		const QImage icon = w->action(IDM_FILE_NEW)->icon().pixmap(QSize(16, 16), 1.0).toImage().convertToFormat(QImage::Format_ARGB32);
		// opaque pixels only: semi-transparent edges lose precision through premultiplied alpha
		int opaque = 0;
		int red = 0;
		QString sample;
		for (int y = 0; y < icon.height(); ++y)
		{
			for (int x = 0; x < icon.width(); ++x)
			{
				const QRgb px = icon.pixel(x, y);
				if (qAlpha(px) < 250)
					continue;
				++opaque;
				if (std::abs(qRed(px) - 0xE8) <= 4 && std::abs(qGreen(px) - 0x11) <= 4 && std::abs(qBlue(px) - 0x23) <= 4)
					++red;
				else if (sample.isEmpty())
					sample = QColor(px).name();
			}
		}
		check(opaque > 0 && red == opaque, "toolbar: complete colorization", QStringLiteral("%1/%2 %3").arg(red).arg(opaque).arg(sample));
		prefs.toolbarColor = 0;
		prefs.toolbarMono = false;

		prefs.recentDisplay = RECENT_FILENAME_ONLY;
		check(MainWindowAccess::recentLabel(*w, QStringLiteral("/a/b/c.txt")) == QStringLiteral("c.txt"), "recent files: only file name");
		prefs.recentDisplay = RECENT_CUSTOM_LENGTH;
		prefs.recentCustomLength = 10;
		check(MainWindowAccess::recentLabel(*w, QStringLiteral("/home/user/some/file.txt")).size() == 10, "recent files: custom length");
		prefs.recentDisplay = RECENT_FULL_PATH;

		check(Npp::windowsDateTimeFormat(QStringLiteral("MMM d, yyyy  tt h:m")) == QStringLiteral("MMM d, yyyy  AP h:m"), "date: Windows AM/PM picture");
		prefs.searchEngineChoice = SE_DUCKDUCKGO;
		check(prefs.searchEngineUrl().contains(QStringLiteral("duckduckgo")), "search engine: DuckDuckGo");
		prefs.searchEngineChoice = SE_CUSTOM;
		prefs.searchEngine = QStringLiteral("not a url");
		check(prefs.searchEngineUrl().startsWith(QStringLiteral("https://www.google.com")), "search engine: invalid custom URL falls back to Google");
		prefs.searchEngineChoice = SE_GOOGLE;

		prefs.newDocOpenAnsiAsUtf8 = false;
		check(FileIO::decode("plain ascii").unicode == UniMode::uni8Bit, "new document: ASCII file opened as ANSI");
		prefs.newDocOpenAnsiAsUtf8 = true;
		check(FileIO::decode("plain ascii").unicode == UniMode::uniCookie, "new document: ASCII file opened as UTF-8");

		// Indentation per language
		auto tabs = prefs.langTabs();
		tabs.insert(QStringLiteral("python"), LangTabSetting{2, true, false});
		prefs.setLangTabs(tabs);
		writeAll(work.filePath(QStringLiteral("t.py")), "def f():\n    pass\n");
		Buffer* py = w->openFile(work.filePath(QStringLiteral("t.py")));
		check(v->ex(SCI_GETTABWIDTH) == 2 && !v->ex(SCI_GETUSETABS), "indentation: per language setting");
		prefs.langTabSettings.clear();
		w->closeBuffer(py, false);

		// Backup on save
		prefs.backupMode = BACKUP_SIMPLE;
		w->activateBuffer(b);
		v->ex(SCI_APPENDTEXT, 2, reinterpret_cast<sptr_t>("//"));
		w->saveBuffer(b);
		check(QFileInfo::exists(work.filePath(QStringLiteral("hello.cpp.bak"))), "backup: simple backup on save");
		prefs.backupMode = BACKUP_NONE;

		// Large File Restriction
		prefs.largeFileSizeMB = 1;
		writeAll(work.filePath(QStringLiteral("big.cpp")), QByteArray(1100 * 1024, 'x'));
		Buffer* big = w->openFile(work.filePath(QStringLiteral("big.cpp")));
		check(big && big->largeFile && v->isLargeFile(), "performance: large file restriction");
		check(v->ex(SCI_GETLEXER) == SCLEX_NULL || v->ex(SCI_GETLEXER) == 1, "performance: large file not styled", QString::number(v->ex(SCI_GETLEXER)));
		w->closeBuffer(big, false);
		prefs.largeFileSizeMB = 200;

		// Distraction Free Mode
		w->command(IDM_VIEW_DISTRACTIONFREE);
		settle();
		check(MainWindowAccess::distractionFree(*w) && !w->menuBar()->isVisible() && v->ex(SCI_GETMARGINLEFT) > 50, "view: distraction free mode");
		w->command(IDM_VIEW_DISTRACTIONFREE);
		settle();

		// Dark mode forced on
		prefs.darkMode = DARKMODE_DARK;
		prefs.darkTone = 3;
		w->applyDarkMode();
		w->applyTheme(QString());
		apply();
		check(QApplication::palette().color(QPalette::Window).lightness() < 128, "dark mode: dark palette");
		check(StyleManager::instance().currentThemePath().contains(QStringLiteral("DarkModeDefault")), "dark mode: dark theme");
		shot(w, QStringLiteral("31-dark-blue-tone"));
		prefs.darkMode = DARKMODE_LIGHT;
		w->applyDarkMode();
		w->applyTheme(QString());
		apply();
		check(QApplication::palette().color(QPalette::Window).lightness() >= 128, "dark mode: light palette");

		// back to the defaults
		Npp::prefs() = NppPrefs();
		prefs.save();
		w->applyDarkMode();
		w->applyTheme(QString());
		apply();
		w->closeBuffer(b, false);
	}

	std::printf("[split view]\n");
	{
		QByteArray lines;
		for (int i = 0; i < 300; ++i)
			lines += "line " + QByteArray::number(i) + " needle\n";
		writeAll(work.filePath(QStringLiteral("a.txt")), lines);
		writeAll(work.filePath(QStringLiteral("b.txt")), "needle in b\n" + lines);
		Buffer* a = w->openFile(work.filePath(QStringLiteral("a.txt")));
		Buffer* b = w->openFile(work.filePath(QStringLiteral("b.txt")));
		check(!w->viewVisible(MainWindow::SUB_VIEW), "split: one view at start");

		w->activateBuffer(a);
		w->command(IDM_VIEW_CLONE_TO_ANOTHER_VIEW);
		settle();
		check(w->viewVisible(MainWindow::SUB_VIEW) && w->activeView() == MainWindow::SUB_VIEW, "split: clone opens the second view");
		check(w->indexOf(MainWindow::MAIN_VIEW, a) >= 0 && w->indexOf(MainWindow::SUB_VIEW, a) >= 0, "split: cloned document in both views");
		w->editView()->ex(SCI_DOCUMENTSTART);
		w->editView()->exs(SCI_REPLACESEL, 0, "X");
		check(w->viewEdit(MainWindow::MAIN_VIEW)->lineText(0).startsWith("X"), "split: clone shares the text");
		w->editView()->ex(SCI_UNDO);
		shot(w, QStringLiteral("40-split-clone"));

		w->command(IDM_FILE_CLOSE);
		settle();
		check(!w->viewVisible(MainWindow::SUB_VIEW) && w->indexOf(MainWindow::MAIN_VIEW, a) >= 0 && w->activeView() == MainWindow::MAIN_VIEW,
		      "split: closing the clone hides the empty view, the document stays");

		w->activateBuffer(b);
		w->command(IDM_VIEW_GOTO_ANOTHER_VIEW);
		settle();
		check(w->indexOf(MainWindow::SUB_VIEW, b) >= 0 && w->indexOf(MainWindow::MAIN_VIEW, b) < 0 && w->currentBuffer() == b, "split: move to other view");
		w->command(IDM_VIEW_SWITCHTO_OTHER_VIEW);
		check(w->activeView() == MainWindow::MAIN_VIEW && w->currentBuffer() == a, "split: focus on another view");
		w->viewEdit(MainWindow::SUB_VIEW)->setFocus();
		settle();
		check(w->activeView() == MainWindow::SUB_VIEW && w->currentBuffer() == b, "split: focusing the other editor activates its view");
		w->command(IDM_VIEW_SWITCHTO_OTHER_VIEW);

		// synchronized scrolling and zoom
		EditView* mv = w->viewEdit(MainWindow::MAIN_VIEW);
		EditView* sv = w->viewEdit(MainWindow::SUB_VIEW);
		mv->ex(SCI_SETFIRSTVISIBLELINE, 0);
		sv->ex(SCI_SETFIRSTVISIBLELINE, 10);
		w->command(IDM_VIEW_SYNSCROLLV);
		mv->ex(SCI_LINESCROLL, 0, 40);
		MainWindowAccess::syncScroll(*w, MainWindow::MAIN_VIEW);
		check(sv->ex(SCI_GETFIRSTVISIBLELINE) == 50, "split: synchronized vertical scrolling", QString::number(sv->ex(SCI_GETFIRSTVISIBLELINE)));
		w->command(IDM_VIEW_SYNSCROLLV);
		w->command(IDM_VIEW_ZOOM_SYNC);
		mv->ex(SCI_SETZOOM, 3);
		settle();
		check(sv->ex(SCI_GETZOOM) == 3, "split: zoom synchronized across views");
		w->command(IDM_VIEW_ZOOM_SYNC);
		mv->ex(SCI_SETZOOM, 0);
		sv->ex(SCI_SETZOOM, 0);

		// smart highlighting in the other view
		prefs.smartHighlightAnotherView = true;
		w->activateBuffer(a, MainWindow::MAIN_VIEW);
		mv->ex(SCI_SETFIRSTVISIBLELINE, 0);
		sv->ex(SCI_SETFIRSTVISIBLELINE, 0);
		const sptr_t ws = mv->ex(SCI_POSITIONFROMLINE, 0) + 7;
		mv->ex(SCI_SETSEL, ws, ws + 6); // "needle"
		settle();
		w->activateBuffer(a, MainWindow::MAIN_VIEW);
		mv->ex(SCI_SETSEL, ws, ws + 6);
		settle();
		check(sv->ex(SCI_INDICATORVALUEAT, SCE_UNIVERSAL_FOUND_STYLE_SMART, 0) != 0, "split: smart highlighting in the other view");
		prefs.smartHighlightAnotherView = false;
		mv->ex(SCI_SETSEL, 0, 0);

		// session with both views
		w->activateBuffer(a, MainWindow::MAIN_VIEW);
		w->command(IDM_VIEW_CLONE_TO_ANOTHER_VIEW);
		const QString sessionPath = work.filePath(QStringLiteral("split-session.xml"));
		MainWindowAccess::writeSession(*w, sessionPath);
		shot(w, QStringLiteral("41-split-two-documents"));
		auto* w2 = new MainWindow;
		w2->resize(1000, 600);
		w2->show();
		const bool read = MainWindowAccess::readSession(*w2, sessionPath);
		settle();
		Buffer* a2 = w2->findBuffer(work.filePath(QStringLiteral("a.txt")));
		Buffer* b2 = w2->findBuffer(work.filePath(QStringLiteral("b.txt")));
		check(read && w2->viewVisible(MainWindow::SUB_VIEW), "split session: second view restored");
		check(a2 && w2->indexOf(MainWindow::MAIN_VIEW, a2) >= 0 && w2->indexOf(MainWindow::SUB_VIEW, a2) >= 0, "split session: clone restored as one document");
		check(b2 && w2->indexOf(MainWindow::SUB_VIEW, b2) >= 0 && w2->indexOf(MainWindow::MAIN_VIEW, b2) < 0, "split session: document of the second view");
		check(w2->activeView() == MainWindow::SUB_VIEW, "split session: active view");
		MainWindowAccess::closeAll(*w2);
		check(!w2->viewVisible(MainWindow::SUB_VIEW) && w2->allBuffers().size() == 1, "split: close all closes both views",
		      QString::number(w2->allBuffers().size()));
		delete w2;

		// tab moved by drag and drop (same code path as the drop handler)
		w->transferBuffer(a, MainWindow::SUB_VIEW, MainWindow::MAIN_VIEW, 0, false);
		check(w->indexOf(MainWindow::SUB_VIEW, a) < 0 && w->indexOf(MainWindow::MAIN_VIEW, a) == 0, "split: dropped tab moved to the other view");
		for (Buffer* x : w->allBuffers())
			w->closeBuffer(x, false);
		check(!w->viewVisible(MainWindow::SUB_VIEW) && w->activeView() == MainWindow::MAIN_VIEW && w->currentBuffer(), "split: back to one view");
	}

	std::printf("[document map & function list]\n");
	{
		QByteArray src =
			"#include <cstdio>\n"
			"\n"
			"class Foo\n"
			"{\n"
			"public:\n"
			"\tint bar(int x);\n"
			"\tvoid baz();\n"
			"};\n"
			"\n"
			"int Foo::bar(int x)\n"
			"{\n"
			"\treturn x * 2;\n"
			"}\n"
			"\n"
			"void Foo::baz()\n"
			"{\n"
			"}\n"
			"\n"
			"static int helper(int a, int b)\n"
			"{\n"
			"\treturn a + b;\n"
			"}\n"
			"\n";
		for (int i = 0; i < 200; ++i)
			src += "// filler line " + QByteArray::number(i) + "\n";
		src += "int main()\n{\n\treturn helper(1, 2);\n}\n";
		const QString path = work.filePath(QStringLiteral("funcs.cpp"));
		writeAll(path, src);
		Buffer* fb = w->openFile(path);
		w->activateBuffer(fb);
		EditView* e = w->editView();

		w->command(IDM_VIEW_FUNC_LIST);
		settle();
		FunctionListPanel* fl = MainWindowAccess::funcList(*w);
		check(fl && MainWindowAccess::funcListShown(*w), "function list: shown by the View command");
		check(w->action(IDM_VIEW_FUNC_LIST) && w->action(IDM_VIEW_FUNC_LIST)->isChecked(), "function list: menu item checked");
		QTreeWidget* tree = fl ? fl->tree() : nullptr;
		auto leaves = [&]
		{
			QStringList names;
			std::function<void(QTreeWidgetItem*, const QString&)> walk = [&](QTreeWidgetItem* it, const QString& prefix)
			{
				for (int i = 0; i < it->childCount(); ++i)
				{
					QTreeWidgetItem* c = it->child(i);
					if (c->childCount() > 0)
						walk(c, c->text(0) + QStringLiteral("::"));
					else if (!c->isHidden())
						names << prefix + c->text(0);
				}
			};
			if (tree && tree->topLevelItem(0))
				walk(tree->topLevelItem(0), QString());
			return names;
		};
		const QStringList found = leaves();
		check(tree && tree->topLevelItem(0) && tree->topLevelItem(0)->text(0) == QStringLiteral("funcs.cpp"), "function list: root is the file name");
		check(found.contains(QStringLiteral("Foo::bar")) && found.contains(QStringLiteral("Foo::baz")), "function list: methods under their class node",
		      found.join(QLatin1Char(',')));
		check(found.contains(QStringLiteral("helper")) && found.contains(QStringLiteral("main")), "function list: free functions", found.join(QLatin1Char(',')));

		fl->searchEdit()->setText(QStringLiteral("hel"));
		check(leaves() == QStringList{QStringLiteral("helper")}, "function list: search filter", leaves().join(QLatin1Char(',')));
		fl->searchEdit()->clear();

		fl->setSorted(true);
		{
			QTreeWidgetItem* root = tree->topLevelItem(0);
			QStringList order;
			for (int i = 0; i < root->childCount(); ++i)
				order << root->child(i)->text(0);
			QStringList sorted = order;
			std::sort(sorted.begin(), sorted.end());
			check(order == sorted && order.size() >= 3, "function list: sort A to Z", order.join(QLatin1Char(',')));
		}
		fl->setSorted(false);

		e->ex(SCI_GOTOLINE, 20); // inside helper()
		fl->markEntry();
		check(tree->currentItem() && tree->currentItem()->text(0) == QStringLiteral("helper"), "function list: entry of the caret marked",
		      tree->currentItem() ? tree->currentItem()->text(0) : QString());

		QTreeWidgetItem* mainItem = nullptr;
		for (QTreeWidgetItem* it : tree->findItems(QStringLiteral("main"), Qt::MatchExactly | Qt::MatchRecursive))
			mainItem = it;
		if (mainItem)
			emit tree->itemActivated(mainItem, 0);
		check(mainItem && QString::fromUtf8(e->lineText(e->ex(SCI_LINEFROMPOSITION, e->ex(SCI_GETCURRENTPOS)))).startsWith(QStringLiteral("int main")),
		      "function list: activating an entry goes to the function");

		e->ex(SCI_DOCUMENTEND);
		e->exs(SCI_REPLACESEL, 0, "\nvoid added()\n{\n}\n");
		w->command(IDM_FILE_SAVE);
		settle();
		check(leaves().contains(QStringLiteral("added")), "function list: parsed again after saving", leaves().join(QLatin1Char(',')));

		w->command(IDM_LANG_TEXT);
		check(tree->topLevelItemCount() == 0, "function list: no parser for normal text");
		w->command(IDM_LANG_CPP);
		check(leaves().contains(QStringLiteral("main")), "function list: follows the language change");

		// Document Map
		e->ex(SCI_DOCUMENTSTART);
		w->command(IDM_VIEW_DOC_MAP);
		settle();
		DocumentMap* dm = MainWindowAccess::docMap(*w);
		check(dm && MainWindowAccess::docMapShown(*w), "document map: shown by the View command");
		check(w->action(IDM_VIEW_DOC_MAP) && w->action(IDM_VIEW_DOC_MAP)->isChecked(), "document map: menu item checked");
		check(dm && dm->mapView()->send(SCI_GETDOCPOINTER) == e->send(SCI_GETDOCPOINTER), "document map: shows the document of the editor");
		check(dm && dm->mapView()->ex(SCI_GETZOOM) == -10, "document map: zoomed out");
		dm->scrollMap();
		const int top0 = dm->zoneTop();
		const int height0 = dm->zoneBottom() - dm->zoneTop();
		check(height0 > 0 && top0 <= 2, "document map: zone marks the top of the document", QStringLiteral("%1 %2").arg(top0).arg(height0));
		e->ex(SCI_SETFIRSTVISIBLELINE, 100);
		settle();
		dm->scrollMap();
		check(dm->zoneTop() > top0, "document map: zone follows the editor", QString::number(dm->zoneTop()));
		const sptr_t first1 = e->ex(SCI_GETFIRSTVISIBLELINE);
		dm->scrollEditorTo(dm->zoneBottom() + 40);
		check(e->ex(SCI_GETFIRSTVISIBLELINE) > first1, "document map: click below the zone scrolls the editor down",
		      QString::number(e->ex(SCI_GETFIRSTVISIBLELINE)));
		const sptr_t first2 = e->ex(SCI_GETFIRSTVISIBLELINE);
		dm->scrollEditor(false, true);
		check(e->ex(SCI_GETFIRSTVISIBLELINE) < first2, "document map: Page Up scrolls the editor up");
		e->ex(SCI_SETFIRSTVISIBLELINE, 0);

		e->foldAll(true);
		check(dm->mapView()->ex(SCI_CONTRACTEDFOLDNEXT, 0) >= 0, "document map: follows Fold All");
		e->foldAll(false);
		check(dm->mapView()->ex(SCI_CONTRACTEDFOLDNEXT, 0) < 0, "document map: follows Unfold All");

		w->command(IDM_VIEW_WRAP);
		settle();
		check(dm->mapView()->ex(SCI_GETWRAPMODE) != SC_WRAP_NONE, "document map: wraps with the editor");
		w->command(IDM_VIEW_WRAP);
		settle();
		check(dm->mapView()->ex(SCI_GETWRAPMODE) == SC_WRAP_NONE, "document map: unwraps with the editor");

		Buffer* other = w->openFile(work.filePath(QStringLiteral("hello.cpp")));
		w->activateBuffer(fb);
		dm->showTemporarily(other, w->activeView());
		check(dm->isTemporary() && dm->mapView()->send(SCI_GETDOCPOINTER) == other->doc, "document map: peek shows the hovered document");
		dm->reloadMap();
		check(!dm->isTemporary() && dm->mapView()->send(SCI_GETDOCPOINTER) == fb->doc, "document map: back to the current document");
		w->activateBuffer(other);
		check(dm->mapView()->send(SCI_GETDOCPOINTER) == other->doc && tree->topLevelItem(0) && tree->topLevelItem(0)->text(0) == QStringLiteral("hello.cpp"),
		      "panels: follow the activated document");
		w->activateBuffer(fb);
		shot(w, QStringLiteral("45-docmap-funclist"));

		w->command(IDM_VIEW_DOC_MAP);
		w->command(IDM_VIEW_FUNC_LIST);
		settle();
		check(!MainWindowAccess::docMapShown(*w) && !MainWindowAccess::funcListShown(*w), "panels: hidden by the View commands again");
		w->closeBuffer(other, false);
		w->closeBuffer(fb, false);
	}

	std::printf("[character panel, clipboard history, paste special]\n");
	{
		w->newDocument();
		EditView* e = w->editView();
		w->command(IDM_EDIT_CHAR_PANEL);
		settle();
		CharacterPanel* cp = MainWindowAccess::charPanel(*w);
		check(cp && cp->list()->topLevelItemCount() == 256, "character panel: 256 characters");
		check(cp && cp->list()->topLevelItem(65)->text(2) == QStringLiteral("A") && cp->list()->topLevelItem(128)->text(2) == QString(QChar(0x20AC)) &&
		      cp->list()->topLevelItem(10)->text(2) == QStringLiteral("LF"), "character panel: characters of the ANSI code page");
		check(cp && cp->list()->topLevelItem(60)->text(3) == QStringLiteral("&lt;") && cp->list()->topLevelItem(128)->text(4) == QStringLiteral("&#8364;") &&
		      cp->list()->topLevelItem(233)->text(5) == QStringLiteral("&#xe9;"), "character panel: HTML columns");
		cp->insertCell(65, 2);
		cp->insertCell(233, 2);
		cp->insertCell(60, 3);
		check(e->wholeText() == QByteArray("A\xC3\xA9&lt;"), "character panel: double click inserts the character or the cell", QString::fromUtf8(e->wholeText()));
		shot(w, QStringLiteral("46-character-panel"));
		w->command(IDM_EDIT_CHAR_PANEL);

		w->command(IDM_EDIT_CLIPBOARDHISTORY_PANEL);
		settle();
		ClipboardHistoryPanel* ch = MainWindowAccess::clipboardPanel(*w);
		QApplication::clipboard()->setText(QStringLiteral("first entry"));
		settle();
		QApplication::clipboard()->setText(QStringLiteral("second entry"));
		settle();
		check(ch && ch->entries().size() >= 2 && ch->entries()[0].data == "second entry" && ch->entries()[1].data == "first entry",
		      "clipboard history: newest copy first", ch ? QString::number(ch->entries().size()) : QString());
		const size_t count = ch ? ch->entries().size() : 0;
		QApplication::clipboard()->setText(QStringLiteral("first entry"));
		settle();
		check(ch && ch->entries().size() == count && ch->entries()[0].data == "first entry", "clipboard history: copied again moves to the top");
		e->ex(SCI_CLEARALL);
		ch->insertEntry(1);
		check(e->wholeText() == "second entry", "clipboard history: double click inserts the entry", QString::fromUtf8(e->wholeText()));

		// Paste Special: binary content keeps the NUL characters
		e->ex(SCI_CLEARALL);
		e->exs(SCI_ADDTEXT, 5, "a\0b\0c");
		e->ex(SCI_SELECTALL);
		w->command(IDM_EDIT_COPY_BINARY);
		settle();
		const QMimeData* mime = QApplication::clipboard()->mimeData();
		check(mime && mime->data(QString::fromLatin1(kNppBinaryMime)) == QByteArray("a\0b\0c", 5), "paste special: copy binary content",
		      mime ? QString::fromLatin1(mime->data(QString::fromLatin1(kNppBinaryMime)).toHex()) + QStringLiteral(" ") + mime->formats().join(QLatin1Char(',')) : QString());
		check(ch->entries()[0].binary && ch->entries()[0].data.size() == 5, "clipboard history: binary entry");
		e->ex(SCI_DOCUMENTEND);
		w->command(IDM_EDIT_PASTE_BINARY);
		check(e->ex(SCI_GETLENGTH) == 10 && e->ex(SCI_GETCHARAT, 6) == 0, "paste special: paste binary content",
		      QString::fromLatin1(e->textRange(0, e->ex(SCI_GETLENGTH)).toHex()));
		e->ex(SCI_SELECTALL);
		w->command(IDM_EDIT_CUT_BINARY);
		check(e->ex(SCI_GETLENGTH) == 0 && QApplication::clipboard()->mimeData()->data(QString::fromLatin1(kNppBinaryMime)).size() == 10,
		      "paste special: cut binary content");
		auto* html = new QMimeData;
		html->setHtml(QStringLiteral("<b>bold</b>"));
		html->setData(QStringLiteral("text/rtf"), QByteArray("{\\rtf1 bold}"));
		QApplication::clipboard()->setMimeData(html);
		w->command(IDM_EDIT_PASTE_AS_HTML);
		check(e->wholeText() == "<b>bold</b>", "paste special: paste HTML content", QString::fromUtf8(e->wholeText()));
		e->ex(SCI_CLEARALL);
		w->command(IDM_EDIT_PASTE_AS_RTF);
		check(e->wholeText() == "{\\rtf1 bold}", "paste special: paste RTF content", QString::fromUtf8(e->wholeText()));
		shot(w, QStringLiteral("47-clipboard-history"));
		w->command(IDM_EDIT_CLIPBOARDHISTORY_PANEL);
		e->ex(SCI_SETSAVEPOINT);
		w->closeBuffer(w->currentBuffer(), false);
	}

	std::printf("[find characters in range, incremental search]\n");
	{
		w->newDocument();
		EditView* e = w->editView();
		e->exs(SCI_SETTEXT, 0, "abc \xC3\xA9 def \xC3\xBC\nneedle one Needle two needle three\n");
		e->ex(SCI_GOTOPOS, 0);
		bool found = NppDialogs::findCharInRange(e, 128, 255, e->ex(SCI_GETCURRENTPOS), true, false);
		check(found && e->ex(SCI_GETSELECTIONSTART) == 4 && e->ex(SCI_GETSELECTIONEND) == 5, "find characters in range: first non-ASCII byte");
		e->ex(SCI_GOTOPOS, 0);
		found = NppDialogs::findCharInRange(e, 128, 255, 0, false, true);
		check(found && e->ex(SCI_GETSELECTIONSTART) == 12, "find characters in range: upwards with wrap around",
		      QString::number(e->ex(SCI_GETSELECTIONSTART)));
		e->ex(SCI_GOTOPOS, 0);
		check(!NppDialogs::findCharInRange(e, '0', '9', 0, true, true), "find characters in range: custom range without match");
		NppDialogs::findCharsInRange(w);
		settle();
		QWidget* charsDlg = w->findChild<QDialog*>(QStringLiteral("findCharsInRangeDlg"));
		check(charsDlg && charsDlg->isVisible(), "find characters in range: dialog shown");
		shot(charsDlg, QStringLiteral("48-find-chars-in-range"));
		if (charsDlg)
			charsDlg->hide();

		e->ex(SCI_GOTOPOS, 0);
		w->command(IDM_SEARCH_FINDINCREMENT);
		settle();
		IncrementalFindBar* inc = MainWindowAccess::incFind(*w);
		check(inc && inc->isVisible() && (inc->edit()->hasFocus() || !w->isActiveWindow()), "incremental search: bar shown with the focus",
		      inc ? QStringLiteral("%1 %2 %3").arg(inc->isVisible()).arg(inc->edit()->hasFocus()).arg(w->isActiveWindow()) : QString());
		inc->matchCase()->setChecked(false);
		inc->count()->setChecked(true);
		inc->highlightAll()->setChecked(true);
		inc->edit()->setText(QStringLiteral("nee"));
		inc->edit()->setText(QStringLiteral("needle"));
		const sptr_t line1 = e->ex(SCI_POSITIONFROMLINE, 1);
		check(e->ex(SCI_GETSELECTIONSTART) == line1 && e->ex(SCI_GETSELECTIONEND) == line1 + 6, "incremental search: selects while typing");
		check(inc->statusLabel()->text() == QStringLiteral("1/3"), "incremental search: count", inc->statusLabel()->text());
		inc->findNext(true);
		check(e->textRangeQ(e->ex(SCI_GETSELECTIONSTART), e->ex(SCI_GETSELECTIONEND)) == QStringLiteral("Needle") &&
		      inc->statusLabel()->text() == QStringLiteral("2/3"), "incremental search: next occurrence", inc->statusLabel()->text());
		auto indicatorRanges = [&](int indic)
		{
			int n = 0;
			for (sptr_t pos = 0; pos < e->ex(SCI_GETLENGTH);)
			{
				const sptr_t end = e->ex(SCI_INDICATOREND, indic, pos);
				if (e->ex(SCI_INDICATORVALUEAT, indic, pos))
					++n;
				if (end <= pos)
					break;
				pos = end;
			}
			return n;
		};
		inc->findNext(true);
		inc->findNext(true);
		check(inc->statusLabel()->text().startsWith(QStringLiteral("1/3")) && inc->findStatus() == IncrementalFindBar::FSEndReached,
		      "incremental search: wraps at the end", inc->statusLabel()->text());
		inc->highlightAll()->setChecked(false);
		inc->highlightAll()->setChecked(true);
		check(indicatorRanges(SCE_UNIVERSAL_FOUND_STYLE_INC) == 3, "incremental search: highlight all", QString::number(indicatorRanges(SCE_UNIVERSAL_FOUND_STYLE_INC)));
		inc->matchCase()->setChecked(true);
		inc->findNext(true);
		check(e->textRangeQ(e->ex(SCI_GETSELECTIONSTART), e->ex(SCI_GETSELECTIONEND)) == QStringLiteral("needle") &&
		      inc->statusLabel()->text().startsWith(QStringLiteral("2/2")), "incremental search: match case", inc->statusLabel()->text());
		inc->edit()->setText(QStringLiteral("needlex"));
		check(inc->findStatus() == IncrementalFindBar::FSNotFound && inc->statusLabel()->text() == QStringLiteral("Phrase not found"),
		      "incremental search: phrase not found");
		inc->edit()->setText(QStringLiteral("needle"));
		shot(w, QStringLiteral("49-incremental-search"));
		QKeyEvent esc(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
		QApplication::sendEvent(inc->edit(), &esc);
		check(!inc->isVisible() && indicatorRanges(SCE_UNIVERSAL_FOUND_STYLE_INC) == 0, "incremental search: Escape closes the bar and clears the marks");
		inc->matchCase()->setChecked(false);
		inc->count()->setChecked(false);
		inc->highlightAll()->setChecked(false);
		e->ex(SCI_SETSAVEPOINT);
		w->closeBuffer(w->currentBuffer(), false);
	}

	std::printf("[hide lines]\n");
	{
		w->newDocument();
		EditView* e = w->editView();
		QByteArray text;
		for (int i = 0; i < 20; ++i)
			text += "line " + QByteArray::number(i) + "\n";
		e->exs(SCI_SETTEXT, 0, text.constData());
		e->ex(SCI_SETSEL, e->ex(SCI_POSITIONFROMLINE, 3), e->ex(SCI_POSITIONFROMLINE, 5) + 2);
		w->command(IDM_VIEW_HIDELINES);
		check(!e->ex(SCI_GETLINEVISIBLE, 3) && !e->ex(SCI_GETLINEVISIBLE, 5) && e->ex(SCI_GETLINEVISIBLE, 2) && e->ex(SCI_GETLINEVISIBLE, 6),
		      "hide lines: selected lines hidden");
		check((e->ex(SCI_MARKERGET, 2) & (1 << MARK_HIDELINESBEGIN)) && (e->ex(SCI_MARKERGET, 6) & (1 << MARK_HIDELINESEND)), "hide lines: markers around the section");
		e->ex(SCI_SETSEL, e->ex(SCI_POSITIONFROMLINE, 7), e->ex(SCI_POSITIONFROMLINE, 7));
		w->command(IDM_VIEW_HIDELINES);
		check(!e->ex(SCI_GETLINEVISIBLE, 7) && e->ex(SCI_GETLINEVISIBLE, 6) && (e->ex(SCI_MARKERGET, 6) & (1 << MARK_HIDELINESEND)) &&
		      (e->ex(SCI_MARKERGET, 6) & (1 << MARK_HIDELINESBEGIN)) && (e->ex(SCI_MARKERGET, 8) & (1 << MARK_HIDELINESEND)),
		      "hide lines: a second section next to the first shares its marker line");
		w->command(IDM_VIEW_CLONE_TO_ANOTHER_VIEW);
		settle();
		EditView* sub = w->viewEdit(MainWindow::SUB_VIEW);
		check(!sub->ex(SCI_GETLINEVISIBLE, 4) && !sub->ex(SCI_GETLINEVISIBLE, 7), "hide lines: restored in the other view of a clone");
		check(sub->hidelineMarkerClicked(2), "hide lines: click on the marker");
		check(e->ex(SCI_GETLINEVISIBLE, 4) && sub->ex(SCI_GETLINEVISIBLE, 4) && !e->ex(SCI_GETLINEVISIBLE, 7) &&
		      !(e->ex(SCI_MARKERGET, 2) & (1 << MARK_HIDELINESBEGIN)), "hide lines: the click shows that section in both views and removes its markers");
		check(sub->hidelineMarkerClicked(6) && e->ex(SCI_GETLINEVISIBLE, 7) && sub->ex(SCI_GETLINEVISIBLE, 7) &&
		      e->ex(SCI_MARKERNEXT, 0, (1 << MARK_HIDELINESBEGIN) | (1 << MARK_HIDELINESEND)) == -1, "hide lines: then the next section");
		check(!sub->hidelineMarkerClicked(2), "hide lines: no marker left, the click toggles a bookmark instead");
		w->command(IDM_FILE_CLOSE);
		settle();
		e = w->editView();
		e->ex(SCI_SETSAVEPOINT);
		w->closeBuffer(w->currentBuffer(), false);
	}

	std::printf("[saved macros]\n");
	{
		w->newDocument();
		EditView* e = w->editView();
		auto macroAction = [&](const QString& name) -> QAction*
		{
			for (QAction* a : w->findChildren<QAction*>())
			{
				if (a->text() == name && a->data().toInt() >= ID_MACRO)
					return a;
			}
			return nullptr;
		};
		QAction* trim = macroAction(QStringLiteral("Trim Trailing Space and Save"));
		check(trim && trim->shortcut() == QKeySequence(Qt::ALT | Qt::SHIFT | Qt::Key_S), "saved macros: default macro in the Macro menu");

		MainWindowAccess::macroStart(*w);
		e->exs(SCI_REPLACESEL, 0, "xy");
		e->ex(SCI_NEWLINE);
		MainWindowAccess::macroStop(*w);
		w->updateMenuState();
		check(w->action(IDM_MACRO_SAVECURRENTMACRO)->isEnabled(), "saved macros: Save Current Recorded Macro enabled after recording");
		QTimer::singleShot(300, [&]
		{
			if (auto* d = qobject_cast<QDialog*>(QApplication::activeModalWidget()))
			{
				shot(d, QStringLiteral("50-save-macro"));
				d->findChild<QLineEdit*>(QStringLiteral("macroName"))->setText(QStringLiteral("Type xy"));
				const QList<QCheckBox*> boxes = d->findChildren<QCheckBox*>();
				for (QCheckBox* c : boxes)
					c->setChecked(c->text() == QStringLiteral("CTRL") || c->text() == QStringLiteral("SHIFT"));
				auto* key = d->findChild<QComboBox*>();
				key->setCurrentIndex(key->findText(QStringLiteral("F9")));
				d->accept();
			}
		});
		w->command(IDM_MACRO_SAVECURRENTMACRO);
		settle();
		const QByteArray xml = readAll(Macros::shortcutsPath());
		check(xml.contains("<Macro name=\"Type xy\" Ctrl=\"yes\" Alt=\"no\" Shift=\"yes\" Key=\"120\">") &&
		      xml.contains("type=\"1\" message=\"2170\"") && xml.contains("sParam=\"xy\"") && xml.contains("type=\"0\" message=\"2329\"") &&
		      xml.contains("type=\"2\" message=\"0\" wParam=\"42024\"") && xml.contains("Trim Trailing Space and Save"),
		      "saved macros: written to shortcuts.xml like Notepad++", QString::fromUtf8(xml.left(400)));
		QAction* typed = macroAction(QStringLiteral("Type xy"));
		check(typed && typed->shortcut() == QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_F9) && !w->action(IDM_MACRO_SAVECURRENTMACRO)->isEnabled(),
		      "saved macros: in the Macro menu with its shortcut");
		e->ex(SCI_CLEARALL);
		if (typed)
			typed->trigger();
		check(docText(e) == QStringLiteral("xy\n"), "saved macros: menu item plays the macro", docText(e).replace(QLatin1Char('\n'), QStringLiteral("\\n")));

		const std::vector<SavedMacro> loaded = Macros::load();
		QString loadedInfo = QString::number(loaded.size());
		for (const SavedMacro& m : loaded)
		{
			loadedInfo += QStringLiteral(" [") + m.name;
			for (const MacroStep& st : m.steps)
				loadedInfo += QStringLiteral(" %1:%2:%3").arg(int(st.kind)).arg(st.message).arg(QString::fromUtf8(st.text));
			loadedInfo += QStringLiteral("]");
		}
		check(loaded.size() == 2 && loaded[1].steps.size() == 2 && loaded[1].steps[1].message == SCI_NEWLINE && loaded[0].steps.size() == 2 &&
		      loaded[0].steps[0].kind == MacroStep::Menu && loaded[0].steps[0].message == IDM_EDIT_TRIMTRAILING, "saved macros: read back from shortcuts.xml",
		      loadedInfo);

		MainWindowAccess::macroStart(*w);
		e->exs(SCI_REPLACESEL, 0, "z");
		MainWindowAccess::macroStop(*w);
		e->ex(SCI_CLEARALL);
		QTimer::singleShot(300, [&]
		{
			if (auto* d = qobject_cast<QDialog*>(QApplication::activeModalWidget()))
			{
				auto* combo = d->findChild<QComboBox*>(QStringLiteral("macroCombo"));
				combo->setCurrentIndex(combo->findText(QStringLiteral("Type xy")));
				d->findChild<QSpinBox*>()->setValue(2);
				d->accept();
			}
		});
		w->command(IDM_MACRO_RUNMULTIMACRODLG);
		check(docText(e) == QStringLiteral("xy\nxy\n"), "saved macros: Run a Macro Multiple Times with a saved macro",
		      docText(e).replace(QLatin1Char('\n'), QStringLiteral("\\n")));

		// Modify Shortcut/Delete Macro: back to the default macro only
		std::vector<SavedMacro> one{loaded[0]};
		Macros::save(one, nullptr);
		MainWindowAccess::reloadMacros(*w);
		QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
		check(!macroAction(QStringLiteral("Type xy")) && macroAction(QStringLiteral("Trim Trailing Space and Save")), "saved macros: menu rebuilt after a deletion");
		e->ex(SCI_SETSAVEPOINT);
		w->closeBuffer(w->currentBuffer(), false);
	}

	std::printf("[OEM 720]\n");
	{
		const QByteArray raw("\x98\x99 \xA1\x8C", 5);
		const QByteArray utf8 = FileIO::toUtf8(raw, 720, nullptr);
		check(QString::fromUtf8(utf8) == QStringLiteral("\u0621\u0622 \u0629\u00EE"), "OEM 720: decoded with the built-in table", QString::fromUtf8(utf8));
		bool lossy = false;
		check(FileIO::fromUtf8(utf8, 720, &lossy) == raw && !lossy && FileIO::isCodepageSupported(720), "OEM 720: encoded back");
		FileIO::fromUtf8(QByteArray("\xE4\xB8\xAD"), 720, &lossy);
		check(lossy && FileIO::encodingName(UniMode::uni8Bit, 720) == QStringLiteral("OEM 720"), "OEM 720: characters outside the code page");
	}

	std::printf("[context menu]\n");
	{
		w->newDocument();
		auto popupLabels = [&]
		{
			QStringList labels;
			QTimer::singleShot(300, [&]
			{
				if (auto* m = qobject_cast<QMenu*>(QApplication::activePopupWidget()))
				{
					for (QAction* a : m->actions())
						labels << (a->isSeparator() ? QStringLiteral("|") : a->text().remove(QLatin1Char('&')).section(QLatin1Char('\t'), 0, 0));
					m->close();
				}
			});
			w->showEditorContextMenu(w->mapToGlobal(QPoint(200, 200)));
			return labels;
		};
		const QStringList def = popupLabels();
		check(def.size() > 10 && def.at(0) == QStringLiteral("Cut") && def.at(2) == QStringLiteral("Paste") &&
		      def.contains(QStringLiteral("Style all occurrences of token")), "context menu: default contextMenu.xml of Notepad++",
		      def.join(QLatin1Char(',')));

		QTimer::singleShot(300, [] { if (auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) box->accept(); });
		w->command(IDM_SETTING_EDITCONTEXTMENU);
		settle();
		const QString cmPath = MainWindow::contextMenuPath();
		check(QFileInfo::exists(cmPath) && w->currentBuffer() && w->currentBuffer()->path == cmPath && readAll(cmPath).contains("<ScintillaContextMenu>"),
		      "context menu: Edit Popup ContextMenu opens contextMenu.xml");
		w->closeBuffer(w->currentBuffer(), false);
		writeAll(cmPath, QStringLiteral("<NotepadPlus><ScintillaContextMenu>"
		                                "<Item MenuEntryName=\"Edit\" MenuItemName=\"Paste\"/><Item id=\"0\"/>"
		                                "<Item id=\"%1\" ItemNameAs=\"BIG\"/><Item FolderName=\"Mine\" id=\"%2\"/>"
		                                "<Item PluginEntryName=\"X\" PluginCommandItemName=\"Y\"/>"
		                                "</ScintillaContextMenu></NotepadPlus>").arg(IDM_EDIT_UPPERCASE).arg(IDM_EDIT_LOWERCASE).toUtf8());
		{
			QFile f(cmPath);
			if (f.open(QIODevice::ReadWrite))
				f.setFileTime(QDateTime::currentDateTime().addSecs(5), QFileDevice::FileModificationTime);
		}
		const QStringList custom = popupLabels();
		check(custom == QStringList({QStringLiteral("Paste"), QStringLiteral("|"), QStringLiteral("BIG"), QStringLiteral("Mine")}),
		      "context menu: items, separator, ItemNameAs and FolderName of the user's file", custom.join(QLatin1Char(',')));
		QFile::remove(cmPath);
		w->closeBuffer(w->currentBuffer(), false);
	}

	std::printf("[project panels]\n");
	{
		QDir().mkpath(work.filePath(QStringLiteral("proj/sub")));
		writeAll(work.filePath(QStringLiteral("proj/a.txt")), "one projneedle\n");
		writeAll(work.filePath(QStringLiteral("proj/sub/b.cpp")), "int projneedle;\nprojneedle++;\n");
		w->command(IDM_VIEW_PROJECT_PANEL_1);
		settle();
		ProjectPanel* pp = MainWindowAccess::projectPanel(*w, 0);
		check(pp && w->projectPanelShown(0) && w->action(IDM_VIEW_PROJECT_PANEL_1)->isChecked(), "project panel: shown by the View command");
		QTreeWidget* tree = pp->tree();
		check(tree->topLevelItemCount() == 1 && tree->topLevelItem(0)->text(0) == QStringLiteral("Workspace") && !pp->isDirty(), "project panel: new workspace");
		QTreeWidgetItem* proj = pp->addProject(QStringLiteral("Proj"), false);
		QTreeWidgetItem* folder = pp->addFolder(proj, QStringLiteral("src"), false);
		pp->addFile(folder, work.filePath(QStringLiteral("proj/sub/b.cpp")));
		QTreeWidgetItem* aItem = pp->addFile(proj, work.filePath(QStringLiteral("proj/a.txt")));
		pp->addFile(proj, work.filePath(QStringLiteral("proj/missing.txt")));
		check(pp->isDirty(), "project panel: modified workspace");
		const QString wsPath = work.filePath(QStringLiteral("proj/test.workspace"));
		check(pp->saveWorkSpaceAs(false, wsPath) && !pp->isDirty() && tree->topLevelItem(0)->text(0) == QStringLiteral("test.workspace"),
		      "project panel: Save As");
		const QByteArray ws = readAll(wsPath);
		check(ws.contains("<Project name=\"Proj\">") && ws.contains("<Folder name=\"src\">") && ws.contains("<File name=\"sub/b.cpp\"") &&
		      ws.contains("<File name=\"a.txt\""), "project panel: workspace file in the Notepad++ format, relative paths", QString::fromUtf8(ws));

		tree->setCurrentItem(aItem);
		pp->popupMenuCmd(ProjectPanel::IDM_PROJECT_MOVEUP);
		check(proj->indexOfChild(aItem) == 0 && pp->isDirty(), "project panel: Move Up");
		pp->popupMenuCmd(ProjectPanel::IDM_PROJECT_MOVEDOWN);
		check(proj->indexOfChild(aItem) == 1, "project panel: Move Down");
		check(pp->workSpaceFiles({QStringLiteral("*.txt")}).size() == 2 && pp->workSpaceFiles({}).size() == 3, "project panel: files of the workspace");

		emit tree->itemDoubleClicked(aItem, 0);
		settle();
		check(w->currentBuffer() && w->currentBuffer()->path == work.filePath(QStringLiteral("proj/a.txt")), "project panel: double click opens the file");

		// Find in Projects
		FindReplaceDlg* fd = MainWindowAccess::findDlg(*w);
		w->showFindDialog(FindReplaceDlg::FindInProjectsTab);
		fd->setProjectPanels(1);
		MainWindowAccess::findInFiles(fd, QStringLiteral("projneedle"), QString(), QStringLiteral("*.*"));
		const QString res = MainWindowAccess::resultsText(*w);
		check(res.contains(QStringLiteral("Search \"projneedle\" (3 hits in 2 files of 2 searched)")), "project panel: Find in Projects",
		      res.section(QLatin1Char('\n'), 0, 0));
		fd->hide();

		// a Windows workspace file: backslashes in the paths
		const QString winWs = work.filePath(QStringLiteral("proj/windows.xml"));
		writeAll(winWs, "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\r\n<NotepadPlus>\r\n    <Project name=\"Win\">\r\n        <Folder name=\"code\">\r\n"
		                "            <File name=\"sub\\b.cpp\" />\r\n        </Folder>\r\n    </Project>\r\n</NotepadPlus>\r\n");
		w->command(IDM_VIEW_PROJECT_PANEL_2);
		settle();
		ProjectPanel* pp2 = MainWindowAccess::projectPanel(*w, 1);
		check(pp2 && pp2->openWorkSpace(winWs, true) && pp2->workSpaceFiles({}) == QStringList{work.filePath(QStringLiteral("proj/sub/b.cpp"))},
		      "project panel: Windows workspace file with backslashes");
		shot(w, QStringLiteral("51-project-panels"));
		check(pp2->openWorkSpace(wsPath, true) && pp2->tree()->topLevelItem(0)->child(0)->childCount() == 3, "project panel: workspace read back");

		pp->saveWorkSpace();
		w->command(IDM_VIEW_PROJECT_PANEL_1);
		w->command(IDM_VIEW_PROJECT_PANEL_2);
		settle();
		check(!w->projectPanelShown(0) && !w->projectPanelShown(1), "project panel: hidden by the View commands again");
		for (Buffer* x : w->allBuffers())
			w->closeBuffer(x, false);
	}

	std::printf("[user defined languages]\n");
	{
		QDir().mkpath(UdlManager::folder());
		writeAll(UdlManager::folder() + QStringLiteral("/mylang.xml"),
		         "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n<NotepadPlus>\n"
		         "  <UserLang name=\"MyLang\" ext=\"myl mylang\" udlVersion=\"2.1\">\n"
		         "    <Settings><Global caseIgnored=\"no\" allowFoldOfComments=\"no\" foldCompact=\"no\" forcePureLC=\"0\" decimalSeparator=\"0\" />\n"
		         "      <Prefix Keywords1=\"no\" Keywords2=\"no\" Keywords3=\"no\" Keywords4=\"no\" Keywords5=\"no\" Keywords6=\"no\" Keywords7=\"no\" Keywords8=\"no\" /></Settings>\n"
		         "    <KeywordLists>\n"
		         "      <Keywords name=\"Comments\">00# 01 02 03 04</Keywords>\n"
		         "      <Keywords name=\"Folders in code1, open\">begin</Keywords>\n"
		         "      <Keywords name=\"Folders in code1, close\">end</Keywords>\n"
		         "      <Keywords name=\"Keywords1\">print &quot;two words&quot;</Keywords>\n"
		         "      <Keywords name=\"Delimiters\">00&quot; 01 02&quot; 03 04 05 06 07 08 09 10 11 12 13 14 15 16 17 18 19 20 21 22 23</Keywords>\n"
		         "    </KeywordLists>\n"
		         "    <Styles>\n"
		         "      <WordsStyle name=\"DEFAULT\" fgColor=\"000000\" bgColor=\"FFFFFF\" fontStyle=\"0\" nesting=\"0\" />\n"
		         "      <WordsStyle name=\"LINE COMMENTS\" fgColor=\"008000\" bgColor=\"FFFFFF\" fontStyle=\"2\" nesting=\"0\" />\n"
		         "      <WordsStyle name=\"KEYWORDS1\" fgColor=\"0000FF\" bgColor=\"FFFFFF\" fontStyle=\"1\" nesting=\"0\" />\n"
		         "      <WordsStyle name=\"FOLDER IN CODE1\" fgColor=\"FF0000\" bgColor=\"FFFFFF\" fontStyle=\"1\" nesting=\"0\" />\n"
		         "      <WordsStyle name=\"DELIMITERS1\" fgColor=\"808080\" bgColor=\"FFFFFF\" fontStyle=\"0\" nesting=\"0\" />\n"
		         "    </Styles>\n  </UserLang>\n</NotepadPlus>\n");
		UdlManager::instance().load();
		w->rebuildUdlMenu();
		QAction* udlAction = nullptr;
		for (QAction* a : MainWindowAccess::languageMenu(*w)->actions())
		{
			if (a->text() == QStringLiteral("MyLang"))
				udlAction = a;
		}
		check(udlAction && UdlManager::instance().find(QStringLiteral("MyLang")), "UDL: user defined language in the Language menu");

		const QString path = work.filePath(QStringLiteral("test.myl"));
		writeAll(path, "begin\n  print two words # a comment\n  x = \"text\"\nend\n");
		Buffer* b = w->openFile(path);
		w->activateBuffer(b);
		EditView* e = w->editView();
		e->ex(SCI_COLOURISE, 0, -1);
		w->_statusMessage.clear(); // a status message of an earlier test
		w->updateStatusBar();
		check(b->lang == L_USER && b->udlName == QStringLiteral("MyLang"), "UDL: language from the extension");
		check(MainWindowAccess::statusLang(*w) == QStringLiteral("User Defined language file - MyLang"), "UDL: status bar", MainWindowAccess::statusLang(*w));
		const sptr_t printPos = e->ex(SCI_POSITIONFROMLINE, 1) + 2;
		const sptr_t commentPos = e->ex(SCI_POSITIONFROMLINE, 1) + 23;
		const sptr_t textPos = e->ex(SCI_POSITIONFROMLINE, 2) + 7;
		check(e->ex(SCI_GETSTYLEAT, 0) == SCE_USER_STYLE_FOLDER_IN_CODE1 && e->ex(SCI_GETSTYLEAT, printPos) == SCE_USER_STYLE_KEYWORD1 &&
		      e->ex(SCI_GETSTYLEAT, printPos + 7) == SCE_USER_STYLE_KEYWORD1 && e->ex(SCI_GETSTYLEAT, commentPos) == SCE_USER_STYLE_COMMENTLINE &&
		      e->ex(SCI_GETSTYLEAT, textPos) == SCE_USER_STYLE_DELIMITER1,
		      "UDL: keywords (also quoted words), comments, delimiters and folders styled by the UDL lexer",
		      QStringLiteral("%1 %2 %3 %4 %5").arg(e->ex(SCI_GETSTYLEAT, 0)).arg(e->ex(SCI_GETSTYLEAT, printPos)).arg(e->ex(SCI_GETSTYLEAT, printPos + 7))
		          .arg(e->ex(SCI_GETSTYLEAT, commentPos)).arg(e->ex(SCI_GETSTYLEAT, textPos)));
		check(e->ex(SCI_STYLEGETFORE, SCE_USER_STYLE_KEYWORD1) == rgb(0, 0, 0xFF) && e->ex(SCI_STYLEGETBOLD, SCE_USER_STYLE_KEYWORD1) &&
		      e->ex(SCI_STYLEGETITALIC, SCE_USER_STYLE_COMMENTLINE), "UDL: colours and font styles of the language");
		check((e->ex(SCI_GETFOLDLEVEL, 0) & SC_FOLDLEVELHEADERFLAG) != 0, "UDL: folding with the folder keywords");
		check(w->action(IDM_LANG_USER) && udlAction->isChecked(), "UDL: its menu item checked");

		const QString sessionPath = work.filePath(QStringLiteral("udl-session.xml"));
		MainWindowAccess::writeSession(*w, sessionPath);
		check(readAll(sessionPath).contains("lang=\"MyLang\""), "UDL: session keeps the language name");

		w->command(IDM_LANG_TEXT);
		check(b->lang == L_TEXT && b->udlName.isEmpty(), "UDL: back to Normal Text");
		udlAction->trigger();
		check(b->lang == L_USER && b->udlName == QStringLiteral("MyLang") && e->ex(SCI_GETSTYLEAT, printPos) == SCE_USER_STYLE_KEYWORD1,
		      "UDL: chosen in the Language menu");
		w->command(IDM_LANG_USER);
		check(b->lang == L_USER && b->udlName.isEmpty(), "UDL: User-Defined");

		const QString copy = work.filePath(QStringLiteral("copy.udl.xml"));
		UserLang saved = *UdlManager::instance().find(QStringLiteral("MyLang"));
		saved.name = QStringLiteral("MyLangCopy");
		check(UdlManager::saveToFile(saved, copy, nullptr), "UDL: written in the udlVersion 2.1 format");
		const QByteArray xml = readAll(copy);
		check(xml.contains("<UserLang name=\"MyLangCopy\" ext=\"myl mylang\" udlVersion=\"2.1\">") && xml.contains("<Keywords name=\"Keywords1\">print \"two words\"</Keywords>") &&
		      xml.contains("<WordsStyle name=\"KEYWORDS1\" fgColor=\"0000FF\""), "UDL: keywords and styles written back", QString::fromUtf8(xml.left(300)));
		e->ex(SCI_SETSAVEPOINT);
		w->closeBuffer(b, false);
	}

	std::printf("[UDL dialog]\n");
	{
		auto wait = [](int ms)
		{
			QEventLoop loop;
			QTimer::singleShot(ms, &loop, &QEventLoop::quit);
			loop.exec();
		};
		Buffer* b = w->openFile(work.filePath(QStringLiteral("test.myl")));
		w->activateBuffer(b);
		EditView* e = w->editView();
		w->command(IDM_LANG_USER_DLG);
		settle();
		UdlDialog* dlg = w->findChild<UdlDialog*>();
		check(dlg && dlg->isVisible() && dlg->langCombo()->currentText() == QStringLiteral("MyLang"), "UDL dialog: opens on the language of the document");
		check(dlg->keywordsEdit(0)->toPlainText() == QStringLiteral("print \"two words\"") && dlg->field(QStringLiteral("commentLineOpen"))->text() == QStringLiteral("#") &&
		      dlg->field(QStringLiteral("delimiter1Open"))->text() == QStringLiteral("\"") && dlg->field(QStringLiteral("delimiter1Close"))->text() == QStringLiteral("\""),
		      "UDL dialog: keywords, comments and delimiters of the language");
		shot(dlg, QStringLiteral("52-udl-dialog"));

		e->ex(SCI_SETTEXT, 0, reinterpret_cast<sptr_t>("begin\n  zork // note\nend\n"));
		dlg->keywordsEdit(1)->setPlainText(QStringLiteral("zork"));
		dlg->field(QStringLiteral("commentLineOpen"))->setText(QStringLiteral("# //"));
		wait(500);
		e->ex(SCI_COLOURISE, 0, -1);
		const sptr_t zork = e->ex(SCI_POSITIONFROMLINE, 1) + 2;
		UserLang* ul = UdlManager::instance().find(QStringLiteral("MyLang"));
		check(ul && ul->dirty && e->ex(SCI_GETSTYLEAT, zork) == SCE_USER_STYLE_KEYWORD2 && e->ex(SCI_GETSTYLEAT, zork + 5) == SCE_USER_STYLE_COMMENTLINE,
		      "UDL dialog: the document follows the changes", QStringLiteral("%1 %2").arg(e->ex(SCI_GETSTYLEAT, zork)).arg(e->ex(SCI_GETSTYLEAT, zork + 5)));
		check(ul->keywordLists[SCE_USER_KWLIST_COMMENTS].rfind("00# 00// 01 02 03 04", 0) == 0, "UDL dialog: comment list in the Notepad++ format",
		      QString::fromStdString(ul->keywordLists[SCE_USER_KWLIST_COMMENTS]));
		dlg->field(QStringLiteral("delimiter2Open"))->setText(QStringLiteral("((begin of)) x"));
		dlg->langCombo()->setCurrentIndex(0);
		dlg->langCombo()->setCurrentIndex(dlg->langCombo()->findText(QStringLiteral("MyLang")));
		check(dlg->field(QStringLiteral("delimiter2Open"))->text() == QStringLiteral("((begin of)) x"), "UDL dialog: (( )) groups kept",
		      QString::fromStdString(ul->keywordLists[SCE_USER_KWLIST_DELIMITERS]));

		QTimer::singleShot(300, []
		{
			if (auto* d = qobject_cast<QDialog*>(QApplication::activeModalWidget()))
			{
				for (QCheckBox* c : d->findChildren<QCheckBox*>())
				{
					if (c->text() == QStringLiteral("Italic"))
						c->setChecked(true);
				}
				d->accept();
			}
		});
		check(UdlDialog::editStyle(dlg, ul, SCE_USER_STYLE_KEYWORD2, false) && (ul->styles[SCE_USER_STYLE_KEYWORD2].fontStyle & FONTSTYLE_ITALIC),
		      "UDL dialog: Styler");

		check(dlg->createLang(QStringLiteral("Second"), true) && UdlManager::instance().find(QStringLiteral("Second")) &&
		      QFileInfo::exists(UdlManager::folder() + QStringLiteral("/Second.xml")), "UDL dialog: Save As a new language, in its own file");
		bool inMenu = false;
		for (QAction* a : MainWindowAccess::languageMenu(*w)->actions())
			inMenu = inMenu || a->text() == QStringLiteral("Second");
		check(inMenu, "UDL dialog: the new language in the Language menu");
		check(dlg->renameLang(QStringLiteral("Second2")) && readAll(UdlManager::folder() + QStringLiteral("/Second.xml")).contains("name=\"Second2\""),
		      "UDL dialog: Rename");
		check(dlg->removeLang() && !UdlManager::instance().find(QStringLiteral("Second2")) && !QFileInfo::exists(UdlManager::folder() + QStringLiteral("/Second.xml")),
		      "UDL dialog: Remove");
		dlg->langCombo()->setCurrentIndex(dlg->langCombo()->findText(QStringLiteral("MyLang")));
		dlg->hide();
		const QByteArray xml = readAll(UdlManager::folder() + QStringLiteral("/mylang.xml"));
		check(xml.contains("<Keywords name=\"Keywords2\">zork</Keywords>") && !ul->dirty, "UDL dialog: changes written when the dialog closes");
		e->ex(SCI_SETSAVEPOINT);
		w->closeBuffer(b, false);
	}

	std::printf("[run commands]\n");
	{
		auto waitFor = [](const std::function<bool()>& cond, int ms)
		{
			QElapsedTimer t;
			t.start();
			while (!cond() && t.elapsed() < ms)
			{
				QEventLoop loop;
				QTimer::singleShot(50, &loop, &QEventLoop::quit);
				loop.exec();
			}
			return cond();
		};
		auto runAction = [&](const QString& name) -> QAction*
		{
			for (QAction* a : w->findChildren<QAction*>())
			{
				if (a->text() == name && a->data().toInt() >= ID_USER_CMD && a->data().toInt() < ID_USER_CMD_LIMIT)
					return a;
			}
			return nullptr;
		};
		QAction* php = runAction(QStringLiteral("Get PHP help"));
		check(php && php->shortcut() == QKeySequence(Qt::ALT | Qt::Key_F1) && runAction(QStringLiteral("Wikipedia Search")),
		      "run commands: default commands of Notepad++ in the Run menu");

		const QString outFile = work.filePath(QStringLiteral("run-out.txt"));
		const QString cmdText = QStringLiteral("echo $(FILE_NAME) > \"%1\"").arg(outFile);
		w->newDocument();
		QTimer::singleShot(300, [&]
		{
			auto* d = qobject_cast<QDialog*>(QApplication::activeModalWidget());
			if (!d)
				return;
			shot(d, QStringLiteral("53-run-dialog"));
			d->findChild<QComboBox*>(QStringLiteral("runCombo"))->setCurrentText(cmdText);
			QTimer::singleShot(300, []
			{
				if (auto* s = qobject_cast<QDialog*>(QApplication::activeModalWidget()))
				{
					s->findChild<QLineEdit*>(QStringLiteral("macroName"))->setText(QStringLiteral("Write file"));
					s->accept();
				}
			});
			for (QPushButton* b : d->findChildren<QPushButton*>())
			{
				if (b->text() == QStringLiteral("&Save..."))
					b->click();
			}
			for (QPushButton* b : d->findChildren<QPushButton*>())
			{
				if (b->text() == QStringLiteral("&Run"))
					b->click();
			}
		});
		w->command(IDM_EXECUTE);
		check(waitFor([&] { return QFileInfo::exists(outFile); }, 3000) && readAll(outFile).trimmed() == w->currentBuffer()->displayName().toUtf8(),
		      "run commands: Run with the variables of Notepad++", QString::fromUtf8(readAll(outFile)));
		QAction* saved = runAction(QStringLiteral("Write file"));
		check(saved && readAll(Macros::shortcutsPath()).contains("<Command name=\"Write file\""), "run commands: Save... adds the command to shortcuts.xml and the Run menu");

		QFile::remove(outFile);
		if (saved)
			saved->trigger();
		check(waitFor([&] { return QFileInfo::exists(outFile); }, 3000), "run commands: saved command runs");

		// shortcuts.xml changed outside Notepad++: the commands do not run until it is validated
		QFile::remove(outFile);
		{
			QFile f(Macros::shortcutsPath());
			if (f.open(QIODevice::Append))
				f.write("<!-- edited by hand -->\n");
		}
		QTimer::singleShot(300, [] { if (auto* box = qobject_cast<QMessageBox*>(QApplication::activeModalWidget())) box->accept(); });
		if (saved)
			saved->trigger();
		settle();
		check(!waitFor([&] { return QFileInfo::exists(outFile); }, 800) && w->currentBuffer() && w->currentBuffer()->path == Macros::shortcutsPath() &&
		      w->currentBuffer()->isReadOnly(), "run commands: modified shortcuts.xml is opened for review instead of running");
		w->closeBuffer(w->currentBuffer(), false);
		w->command(IDM_EXECUTE_VALIDATE_SHORTCUTSXML);
		if (saved)
			saved->trigger();
		check(waitFor([&] { return QFileInfo::exists(outFile); }, 3000), "run commands: Validate shortcuts.xml");
		for (Buffer* x : w->allBuffers())
			w->closeBuffer(x, false);
	}

	std::printf("[scintilla commands]\n");
	{
		w->newDocument();
		EditView* e = w->editView();
		auto findCmd = [](int sci) -> ScintillaKeyCmd*
		{
			for (ScintillaKeyCmd& c : Macros::scintillaKeys())
			{
				if (c.sciCmd == sci && c.menuCmd == 0)
					return &c;
			}
			return nullptr;
		};
		ScintillaKeyCmd* del = findCmd(SCI_LINEDELETE);
		check(Macros::scintillaKeys().size() > 80 && del && del->keys.size() == 1 && del->keys[0] == ScintillaKeyCombo{true, false, true, 'L'},
		      "scintilla commands: the Notepad++ list with its default keys");
		auto press = [&](Qt::Key key, Qt::KeyboardModifiers mods)
		{
			QKeyEvent ev(QEvent::KeyPress, key, mods);
			QApplication::sendEvent(e, &ev);
		};
		e->ex(SCI_SETTEXT, 0, reinterpret_cast<sptr_t>("one\ntwo\nthree\n"));
		e->ex(SCI_GOTOLINE, 1);
		press(Qt::Key_L, Qt::ControlModifier | Qt::ShiftModifier);
		check(docText(e) == QStringLiteral("one\nthree\n"), "scintilla commands: default key of a command", docText(e));

		del->keys = {ScintillaKeyCombo{true, true, false, 'K'}};
		check(Macros::saveScintillaKeys(nullptr) &&
		      readAll(Macros::shortcutsPath()).contains("<ScintKey ScintID=\"2338\" menuCmdID=\"0\" Ctrl=\"yes\" Alt=\"yes\" Shift=\"no\" Key=\"75\""),
		      "scintilla commands: changed key in <ScintillaKeys> of shortcuts.xml");
		w->applyScintillaKeys();
		e->ex(SCI_SETTEXT, 0, reinterpret_cast<sptr_t>("one\ntwo\nthree\n"));
		e->ex(SCI_GOTOLINE, 1);
		press(Qt::Key_L, Qt::ControlModifier | Qt::ShiftModifier);
		const bool oldKeyGone = docText(e) == QStringLiteral("one\ntwo\nthree\n");
		press(Qt::Key_K, Qt::ControlModifier | Qt::AltModifier);
		check(oldKeyGone && docText(e) == QStringLiteral("one\nthree\n"), "scintilla commands: the new key replaces the old one", docText(e));
		Macros::reloadScintillaKeys();
		del = findCmd(SCI_LINEDELETE);
		check(del && del->keys.size() == 1 && del->keys[0] == ScintillaKeyCombo{true, true, false, 'K'}, "scintilla commands: read back from shortcuts.xml");

		QStringList tabNames;
		int sciRows = 0;
		QTimer::singleShot(400, [&]
		{
			if (auto* d = qobject_cast<QDialog*>(QApplication::activeModalWidget()))
			{
				if (auto* t = d->findChild<QTabWidget*>(QStringLiteral("shortcutMapperTabs")))
				{
					for (int i = 0; i < t->count(); ++i)
						tabNames << t->tabText(i);
					t->setCurrentIndex(1);
					shot(d, QStringLiteral("54-shortcut-mapper-scintilla"));
				}
				if (auto* st = d->findChild<QTableWidget*>(QStringLiteral("scintillaKeysTable")))
					sciRows = st->rowCount();
				d->reject();
			}
		});
		NppDialogs::shortcutMapper(w);
		check(tabNames == QStringList({QStringLiteral("Main menu"), QStringLiteral("Scintilla commands")}) && sciRows > 50,
		      "scintilla commands: Shortcut Mapper tab", QString::number(sciRows));

		del->keys = del->defaults;
		Macros::saveScintillaKeys(nullptr);
		w->applyScintillaKeys();
		e->ex(SCI_SETSAVEPOINT);
		w->closeBuffer(w->currentBuffer(), false);
	}

	std::printf("[plugins]\n");
	{
		PluginsManager* pm = w->pluginsManager();
		const QStringList mods = pm->loadedModules();
		if (NPP_TEST_BUNDLED_PLUGINS)
		{
			check(mods.contains(QStringLiteral("mimeTools.so")), "plugins: MIME Tools loaded", mods.join(QLatin1Char(' ')));
			check(mods.contains(QStringLiteral("NppConverter.so")), "plugins: Converter loaded", mods.join(QLatin1Char(' ')));
			check(mods.contains(QStringLiteral("nppAutoDetectIndent.so")), "plugins: Auto Detect Indention loaded", mods.join(QLatin1Char(' ')));
			check(mods.contains(QStringLiteral("DiffChecker.so")), "plugins: Diff Checker loaded", mods.join(QLatin1Char(' ')));
		}
		if (NPP_TEST_JSONTOOLS)
			check(mods.contains(QStringLiteral("JsonTools.so")), "plugins: JSONTools loaded", mods.join(QLatin1Char(' ')));
		check(mods.contains(QStringLiteral("NppTestPlugin.so")), "plugins: test plugin loaded", mods.join(QLatin1Char(' ')));
		check(pm->loadErrors().isEmpty(), "plugins: no load error", pm->loadErrors().join(QLatin1Char('\n')));
		auto send = [&](unsigned int msg, uintptr_t wp = 0, intptr_t lp = 0)
		{
			return SendMessage(pm->nppHandle(), msg, wp, lp);
		};
		auto report = [&]
		{
			TestPluginReport r;
			sdk::CommunicationInfo ci{TestPluginReport::kGetReport, L"selftest", &r};
			send(NPPM_MSGTOPLUGIN, reinterpret_cast<uintptr_t>(L"NppTestPlugin.dll"), reinterpret_cast<intptr_t>(&ci));
			return r;
		};
		auto nppn = [](const TestPluginReport& r, unsigned int code) { return r.nppn[code - NPPN_FIRST]; };
		auto idFrom = [](const TestPluginReport& r, unsigned int code) { return r.lastIdFrom[code - NPPN_FIRST]; };

		// Plugins menu: a submenu per plugin, then Open Plugins Folder...
		QMenu* pluginsMenu = nullptr;
		for (QAction* a : w->menuBar()->actions())
		{
			if (a->menu() && a->text().remove(QLatin1Char('&')) == QStringLiteral("Plugins"))
				pluginsMenu = a->menu();
		}
		QStringList top;
		for (QAction* a : pluginsMenu ? pluginsMenu->actions() : QList<QAction*>())
			top << (a->isSeparator() ? QStringLiteral("|") : a->text().remove(QLatin1Char('&')));
		check(top.size() >= 3 && top.contains(QStringLiteral("Test Plugin"))
			&& (!NPP_TEST_BUNDLED_PLUGINS || (top.contains(QStringLiteral("Converter"))
				&& top.indexOf(QStringLiteral("MIME Tools")) >= 0
				&& top.indexOf(QStringLiteral("Test Plugin")) > top.indexOf(QStringLiteral("MIME Tools"))))
			&& top.last().startsWith(QStringLiteral("Open Plugins Folder")) && top[top.size() - 2] == QStringLiteral("|"),
			"plugins: Plugins menu", top.join(QStringLiteral(", ")));
		auto pluginAction = [&](const QString& plugin, const QString& item) -> QAction*
		{
			for (QAction* a : pluginsMenu ? pluginsMenu->actions() : QList<QAction*>())
			{
				if (a->menu() && a->text() == plugin)
				{
					for (QAction* b : a->menu()->actions())
					{
						if (b->text() == item)
							return b;
					}
				}
			}
			return nullptr;
		};
		QStringList mimeItems;
		for (QAction* m : pluginsMenu ? pluginsMenu->actions() : QList<QAction*>())
		{
			if (m->menu() && m->text() == QStringLiteral("MIME Tools"))
			{
				for (QAction* a : m->menu()->actions())
					mimeItems << (a->isSeparator() ? QStringLiteral("|") : a->text());
			}
		}
		w->newDocument();
		EditView* e = w->editView();
		if (NPP_TEST_BUNDLED_PLUGINS)
		{
			check(mimeItems.size() == 22 && mimeItems[7] == QStringLiteral("|") && mimeItems[19] == QStringLiteral("SAML Decode"),
				"plugins: MIME Tools menu (22 items, separators)", mimeItems.join(QStringLiteral(", ")));
			auto runPlugin = [&](const QString& plugin, const char* item, const QByteArray& in)
			{
				setDoc(e, in);
				e->ex(SCI_SELECTALL);
				if (QAction* a = pluginAction(plugin, QString::fromLatin1(item)))
					a->trigger();
				return QString::fromUtf8(e->wholeText());
			};
			const QString mimeTools = QStringLiteral("MIME Tools");
			check(runPlugin(mimeTools, "Base64 Encode", "Hello world") == QStringLiteral("SGVsbG8gd29ybGQ"), "plugins: MIME Tools Base64 Encode", docText(e));
			check(runPlugin(mimeTools, "Base64 Encode with padding", "Hello world") == QStringLiteral("SGVsbG8gd29ybGQ="), "plugins: MIME Tools Base64 Encode with padding", docText(e));
			check(runPlugin(mimeTools, "Base64 Decode", "SGVsbG8gd29ybGQ=") == QStringLiteral("Hello world"), "plugins: MIME Tools Base64 Decode", docText(e));
			check(runPlugin(mimeTools, "URL Encode (RFC1738)", "a b") == QStringLiteral("a%20b"), "plugins: MIME Tools URL Encode", docText(e));
			check(runPlugin(mimeTools, "URL Decode", "a%20b%41") == QStringLiteral("a bA"), "plugins: MIME Tools URL Decode", docText(e));
			check(runPlugin(mimeTools, "Quoted-printable Encode", "caf\xC3\xA9") == QStringLiteral("caf=C3=A9"), "plugins: MIME Tools Quoted-printable Encode", docText(e));
			check(runPlugin(mimeTools, "Quoted-printable Decode", "caf=C3=A9") == QStringLiteral("café"), "plugins: MIME Tools Quoted-printable Decode", docText(e));

			const QString converter = QStringLiteral("Converter");
			check(runPlugin(converter, "ASCII -> HEX", "Az") == QStringLiteral("417A"), "plugins: Converter ASCII to hex", docText(e));
			check(runPlugin(converter, "HEX -> ASCII", "417A") == QStringLiteral("Az"), "plugins: Converter hex to ASCII", docText(e));
			QAction* panel = pluginAction(converter, QStringLiteral("Conversion Panel"));
			check(panel != nullptr, "plugins: Converter panel command");
			if (panel)
			{
				panel->trigger();
				settle();
				auto* dock = w->findChild<QDockWidget*>(QStringLiteral("plugin:NppConverter:3"));
				check(dock && dock->isVisible(), "plugins: Converter dock built from the .rc template");
				HWND dialog = reinterpret_cast<HWND>(send(NPPM_DMMGETPLUGINHWNDBYNAME,
					reinterpret_cast<uintptr_t>(L"Conversion"), reinterpret_cast<intptr_t>(L"NppConverter.so")));
				auto widget = [dialog](int id) -> QWidget*
				{
					HWND control = GetDlgItem(dialog, id);
					return control ? static_cast<QWidget*>(control->widget) : nullptr;
				};
				auto* ascii = qobject_cast<QLineEdit*>(widget(ID_ASCII_EDIT));
				auto* hex = qobject_cast<QLineEdit*>(widget(ID_HEX_EDIT));
				auto* binary = qobject_cast<QLineEdit*>(widget(ID_BIN_EDIT));
				auto* insertHex = qobject_cast<QPushButton*>(widget(ID_HEX_INSERT_BUTTON));
				auto* copyAscii = qobject_cast<QPushButton*>(widget(ID_ASCII_BUTTON));
				check(ascii && hex && binary && insertHex && copyAscii, "plugins: Converter control IDs from its resource");
				if (ascii && hex && binary && insertHex && copyAscii)
				{
					check(dock && dock->widget() && insertHex->isVisible()
						&& dock->widget()->rect().contains(insertHex->geometry()),
						"plugins: Converter dock fits the resource controls");
					ascii->setText(QStringLiteral("A"));
					check(hex->text() == QStringLiteral("41") && binary->text() == QStringLiteral("1000001"),
						"plugins: Converter panel EN_CHANGE and ANSI WM_SETTEXT");
					setDoc(e, "");
					insertHex->click();
					check(docText(e) == QStringLiteral("41"), "plugins: Converter panel button and ANSI WM_GETTEXT");
					hex->setText(QStringLiteral("00"));
					copyAscii->click();
					const QMimeData* clipboard = QApplication::clipboard()->mimeData();
					check(clipboard && clipboard->data(QString::fromLatin1(kNppBinaryMime)) == QByteArray(1, '\0'),
						"plugins: Converter panel copies a NUL byte");
					w->command(IDM_EDIT_PASTE_BINARY);
					check(e->wholeText() == QByteArray("41\0", 3), "plugins: binary paste of Converter output");
					shot(dock, QStringLiteral("71-converter-panel"));
				}
				if (dock)
					dock->close();
			}

			Buffer* original = w->currentBuffer();
			const QString spacesPath = work.filePath(QStringLiteral("two-spaces.txt"));
			const QString tabsPath = work.filePath(QStringLiteral("tabs.txt"));
			writeAll(spacesPath, "a\n  b\n  c\n    d\n  e\n" + QByteArray(54, ' ') + "alignment\n");
			writeAll(tabsPath, "a\n\tb\n\tc\n\t\td\n");
			Buffer* spaces = w->openFile(spacesPath);
			check(spaces && e->ex(SCI_GETUSETABS) == 0 && e->ex(SCI_GETINDENT) == 2,
				"plugins: Auto Detect Indention applies two spaces on activation");
			Buffer* tabs = w->openFile(tabsPath);
			check(tabs && e->ex(SCI_GETUSETABS) == 1, "plugins: Auto Detect Indention detects tabs");
			w->activateBuffer(spaces);
			check(spaces && e->ex(SCI_GETUSETABS) == 0 && e->ex(SCI_GETINDENT) == 2,
				"plugins: Auto Detect Indention restores a switched tab's settings");
			if (QAction* disable = pluginAction(QStringLiteral("Auto Detect Indention"), QStringLiteral("Disable plugin")))
			{
				disable->trigger();
				const bool disabled = disable->isChecked();
				disable->trigger();
				check(disabled && !disable->isChecked() && e->ex(SCI_GETINDENT) == 2,
					"plugins: Auto Detect Indention can be toggled");
			}
			else
				check(false, "plugins: Auto Detect Indention toggle is in the menu");
			w->closeBuffer(tabs, false);
			w->closeBuffer(spaces, false);
			w->activateBuffer(original);

			const QString leftPath = work.filePath(QStringLiteral("diff-left.txt"));
			const QString rightPath = work.filePath(QStringLiteral("diff-right.txt"));
			writeAll(leftPath, "same\r\nold\r\nkeep\r\nremove\r\nlast\r\n");
			writeAll(rightPath, "same\nnew\nkeep\nlast\n");
			Buffer* left = w->openFile(leftPath);
			Buffer* right = w->openFile(rightPath);
			check(left && right, "plugins: Diff Checker sample tabs");
			if (left && right)
			{
				setDoc(e, "same\nnew\nkeep\nlast\nadded\n");
				QAction* compareMenu = pluginAction(QStringLiteral("Diff Checker"), QStringLiteral("Compare…"));
				if (compareMenu)
					compareMenu->trigger();
				auto* dialog = w->findChild<QDialog*>(QStringLiteral("diffCheckerDialog"));
				check(dialog && dialog->isVisible(), "plugins: Diff Checker opens from the menu");
				if (dialog)
				{
					auto* leftCombo = dialog->findChild<QComboBox*>(QStringLiteral("diffLeft"));
					auto* rightCombo = dialog->findChild<QComboBox*>(QStringLiteral("diffRight"));
					auto* compare = dialog->findChild<QPushButton*>(QStringLiteral("diffCompare"));
					auto* next = dialog->findChild<QPushButton*>(QStringLiteral("diffNext"));
					auto* status = dialog->findChild<QLabel*>(QStringLiteral("diffStatus"));
					auto* table = dialog->findChild<QTableView*>(QStringLiteral("diffRows"));
					check(leftCombo && rightCombo && compare && next && status && table, "plugins: Diff Checker controls");
					if (leftCombo && rightCombo && compare && next && status && table)
					{
						auto choose = [](QComboBox* combo, Buffer* buffer)
						{
							combo->setCurrentIndex(combo->findData(QVariant::fromValue<qlonglong>(reinterpret_cast<intptr_t>(buffer))));
						};
						auto compareAndWait = [&]
						{
							compare->click();
							return waitFor([&] { return compare->isEnabled(); });
						};
						choose(leftCombo, left);
						choose(rightCombo, right);
						check(compareAndWait() && status->text().contains(QStringLiteral("3 change blocks"))
							&& table->model()->index(1, 1).data().toString() == QStringLiteral("old")
							&& table->model()->index(1, 3).data().toString() == QStringLiteral("new")
							&& w->currentBuffer() == right,
							"plugins: Diff Checker compares live text, CRLF, and LF", status->text());
						next->click();
						check(table->currentIndex().row() == 1, "plugins: Diff Checker jumps to the first change");
						shot(dialog, QStringLiteral("72-diff-checker"));
						choose(rightCombo, left);
						check(compareAndWait() && status->text() == QStringLiteral("No differences"),
							"plugins: Diff Checker identical tabs");
					}
					dialog->close();
					settle();
				}
				e->ex(SCI_SETSAVEPOINT);
			}
			w->closeBuffer(right, false);
			w->closeBuffer(left, false);
			w->activateBuffer(original);
		}

		if (NPP_TEST_JSONTOOLS)
		{
			const QString plugin = QStringLiteral("JSONTools");
			auto invoke = [&](const QString& item)
			{
				if (QAction* action = pluginAction(plugin, item))
				{
					action->trigger();
					return true;
				}
				return false;
			};
			const QByteArray json5 = "{name: 'value', nums: [3,1,2],}";
			const QByteArray compact = "{\"name\":\"value\",\"nums\":[3,1,2]}";
			setDoc(e, json5);
			check(invoke(QStringLiteral("Pretty-print current JSON file")) && waitFor([&] { return e->wholeText().contains('\n'); })
				&& e->wholeText().contains("\"name\": \"value\""), "plugins: JSONTools formats JSON5");
			e->ex(SCI_UNDO);
			check(e->wholeText() == json5, "plugins: JSONTools formatting is one undo step");
			check(invoke(QStringLiteral("Compress current JSON file")) && waitFor([&] { return e->wholeText() == compact; }),
				"plugins: JSONTools compresses JSON in the editor", docText(e));
			e->ex(SCI_GOTOPOS, 0);

			check(invoke(QStringLiteral("Open JSON tree viewer")), "plugins: JSONTools tree viewer menu");
			auto* viewer = w->findChild<QDialog*>(QStringLiteral("jsonToolsTree"));
			check(viewer && viewer->isVisible(), "plugins: JSONTools opens the Qt tree viewer");
			if (viewer)
			{
				auto* tree = viewer->findChild<QTreeWidget*>(QStringLiteral("jsonToolsNodes"));
				auto* query = viewer->findChild<QLineEdit*>(QStringLiteral("jsonToolsQuery"));
				auto* run = viewer->findChild<QPushButton*>(QStringLiteral("jsonToolsRun"));
				auto* copy = viewer->findChild<QPushButton*>(QStringLiteral("jsonToolsCopy"));
				check(tree && query && run && copy && waitFor([&] { return tree->topLevelItemCount() == 1; }),
					"plugins: JSONTools displays parsed nodes");
				if (tree && query && run && copy && tree->topLevelItemCount() == 1)
				{
					const auto* root = tree->topLevelItem(0);
					check(root && root->childCount() == 2 && root->child(1)->text(0) == QStringLiteral("nums"),
						"plugins: JSONTools tree shows object keys");
					shot(viewer, QStringLiteral("73-jsontools-tree"));
					query->setText(QStringLiteral("@.nums[1]"));
					run->click();
					const bool solved = waitFor([&] { return run->isEnabled(); }) && copy->isEnabled()
						&& tree->topLevelItemCount() == 1 && tree->topLevelItem(0)->text(1) == QStringLiteral("1");
					check(solved, "plugins: JSONTools queries with the upstream RemesPath engine");
					if (solved)
						copy->click();
					check(solved && QApplication::clipboard()->text() == QStringLiteral("1"), "plugins: JSONTools copies query results");
				}
				viewer->close();
				settle();
			}

			const QByteArray prefix = "prefix ";
			setDoc(e, prefix + json5 + " suffix");
			e->ex(SCI_SETSEL, prefix.size(), prefix.size() + json5.size());
			check(invoke(QStringLiteral("Compress current JSON file"))
				&& waitFor([&] { return e->wholeText() == prefix + compact + " suffix"; }),
				"plugins: JSONTools formats only selected JSON");
		}

		const QString pluginFixture = QCoreApplication::applicationDirPath()
			+ QStringLiteral("/../test-plugins/NppTestPlugin/NppTestPlugin.so");
		const QString importSource = work.filePath(QStringLiteral("ImportCheck.so"));
		const QString imported = PluginsManager::userPluginsDir() + QStringLiteral("/ImportCheck/ImportCheck.so");
		const QByteArray moduleBytes = readAll(pluginFixture);
		QString importError;
		check(!moduleBytes.isEmpty() && QFile::copy(pluginFixture, importSource)
			&& pm->importPlugins({importSource}, &importError) && readAll(imported) == moduleBytes,
			"plugins: import copies a native module", importError);
		check(pm->importPlugins({imported}, &importError) && readAll(imported) == moduleBytes,
			"plugins: importing an installed module preserves it", importError);
		check(!pm->importPlugins({work.filePath(QStringLiteral("missing/ImportCheck.so"))}, &importError)
			&& readAll(imported) == moduleBytes, "plugins: failed import preserves the installed module");

		// notifications
		TestPluginReport r = report();
		check(nppn(r, NPPN_TBMODIFICATION) == 1 && nppn(r, NPPN_READY) == 1, "plugins: NPPN_TBMODIFICATION and NPPN_READY once",
			QStringLiteral("%1 %2").arg(nppn(r, NPPN_TBMODIFICATION)).arg(nppn(r, NPPN_READY)));
		check(r.scnModified > 0 && r.scnUpdateUi > 0, "plugins: SCN_MODIFIED and SCN_UPDATEUI relayed",
			QStringLiteral("%1 %2").arg(r.scnModified).arg(r.scnUpdateUi));
		check(idFrom(r, NPPN_BUFFERACTIVATED) == reinterpret_cast<uintptr_t>(w->currentBuffer()), "plugins: NPPN_BUFFERACTIVATED of the new document");
		const QString pluginFile = work.filePath(QStringLiteral("plugin-notify.txt"));
		writeAll(pluginFile, "first\n");
		const int opened = nppn(r, NPPN_FILEOPENED);
		Buffer* pb = w->openFile(pluginFile);
		r = report();
		check(pb && nppn(r, NPPN_FILEOPENED) == opened + 1 && idFrom(r, NPPN_FILEOPENED) == reinterpret_cast<uintptr_t>(pb)
			&& nppn(r, NPPN_FILEBEFOREOPEN) > 0 && nppn(r, NPPN_FILEBEFORELOAD) > 0, "plugins: NPPN_FILEBEFORELOAD/BEFOREOPEN/FILEOPENED");
		w->languageSet(L_CPP);
		r = report();
		check(nppn(r, NPPN_LANGCHANGED) > 0 && idFrom(r, NPPN_LANGCHANGED) == reinterpret_cast<uintptr_t>(pb), "plugins: NPPN_LANGCHANGED");
		check(send(NPPM_GETBUFFERLANGTYPE, reinterpret_cast<uintptr_t>(pb)) == L_CPP, "plugins: NPPM_GETBUFFERLANGTYPE");

		// commands, check marks, shortcuts, toolbar
		QAction* cmdA = pluginAction(QStringLiteral("Test Plugin"), QStringLiteral("Command A"));
		check(cmdA != nullptr, "plugins: test plugin command in the menu");
		if (cmdA)
		{
			cmdA->trigger();
			r = report();
			check(r.commandA == 1 && docText(e).endsWith(QStringLiteral("plugin!")), "plugins: command edits through Scintilla", docText(e));
			check(cmdA->isCheckable() && cmdA->isChecked(), "plugins: NPPM_SETMENUITEMCHECK checks the item");
			send(WM_COMMAND, static_cast<uintptr_t>(cmdA->data().toInt()));
			r = report();
			check(r.commandA == 2 && !cmdA->isChecked(), "plugins: WM_COMMAND runs a plugin command, unchecked again");
			check(w->findChild<QToolBar*>() && w->findChild<QToolBar*>()->actions().contains(cmdA) && !cmdA->icon().isNull(),
				"plugins: NPPM_ADDTOOLBARICON_FORDARKMODE");
			sdk::ShortcutKey sk;
			check(!send(NPPM_GETSHORTCUTBYCMDID, static_cast<uintptr_t>(cmdA->data().toInt()), reinterpret_cast<intptr_t>(&sk)), "plugins: no shortcut for Command A");
		}
		QAction* aboutA = pluginAction(QStringLiteral("Test Plugin"), QStringLiteral("About"));
		check(aboutA && aboutA->shortcut() == QKeySequence(QStringLiteral("Ctrl+Alt+Shift+T")), "plugins: ShortcutKey of FuncItem",
			aboutA ? aboutA->shortcut().toString() : QString());
		if (aboutA)
		{
			sdk::ShortcutKey sk;
			check(send(NPPM_GETSHORTCUTBYCMDID, static_cast<uintptr_t>(aboutA->data().toInt()), reinterpret_cast<intptr_t>(&sk)) && sk._isCtrl && sk._isAlt && sk._isShift && sk._key == 'T',
				"plugins: NPPM_GETSHORTCUTBYCMDID");
		}
		QAction* checkedA = pluginAction(QStringLiteral("Test Plugin"), QStringLiteral("Checked at start"));
		check(checkedA && checkedA->isChecked(), "plugins: _init2Check");
		if (checkedA)
		{
			HMENU menu = GetMenu(pm->nppHandle());
			CheckMenuItem(menu, static_cast<UINT>(checkedA->data().toInt()), MF_BYCOMMAND | MF_UNCHECKED);
			check(!checkedA->isChecked(), "plugins: CheckMenuItem(GetMenu()) of the SDK");
		}

		// docked panel
		if (QAction* showA = pluginAction(QStringLiteral("Test Plugin"), QStringLiteral("Show Panel")))
		{
			showA->trigger();
			settle();
			QDockWidget* dock = w->findChild<QDockWidget*>(QStringLiteral("plugin:NppTestPlugin:2"));
			check(dock && dock->isVisible() && dock->windowTitle() == QStringLiteral("Test Panel - info") && dock->widget()
				&& dock->widget()->objectName() == QStringLiteral("testPluginPanel"), "plugins: NPPM_DMMREGASDCKDLG + NPPM_DMMSHOW",
				dock ? dock->windowTitle() : QString());
			check(dock && w->dockWidgetArea(dock) == Qt::RightDockWidgetArea, "plugins: DWS_DF_CONT_RIGHT");
			shot(w, QStringLiteral("70-plugin-panel"));
			showA->trigger();
			settle();
			check(dock && !dock->isVisible() && !showA->isChecked(), "plugins: NPPM_DMMHIDE");
		}

		// NPPM_ messages
		check(send(NPPM_GETCURRENTBUFFERID) == reinterpret_cast<intptr_t>(pb), "plugins: NPPM_GETCURRENTBUFFERID");
		{
			const intptr_t len = send(NPPM_GETFULLPATHFROMBUFFERID, reinterpret_cast<uintptr_t>(pb), 0);
			std::wstring path(static_cast<size_t>(len), L'\0');
			send(NPPM_GETFULLPATHFROMBUFFERID, reinterpret_cast<uintptr_t>(pb), reinterpret_cast<intptr_t>(path.data()));
			check(QString::fromStdWString(path) == pluginFile, "plugins: NPPM_GETFULLPATHFROMBUFFERID", QString::fromStdWString(path));
			wchar_t name[MAX_PATH];
			check(send(NPPM_GETFILENAME, MAX_PATH, reinterpret_cast<intptr_t>(name)) && QString::fromWCharArray(name) == QStringLiteral("plugin-notify.txt"),
				"plugins: NPPM_GETFILENAME");
			check(send(NPPM_GETNAMEPART, MAX_PATH, reinterpret_cast<intptr_t>(name)) && QString::fromWCharArray(name) == QStringLiteral("plugin-notify"),
				"plugins: NPPM_GETNAMEPART");
			check(!send(NPPM_GETFILENAME, 3, reinterpret_cast<intptr_t>(name)), "plugins: NPPM_GETFILENAME with a small buffer fails");
			check(send(NPPM_GETLANGUAGENAME, L_CPP, 0) == 3, "plugins: NPPM_GETLANGUAGENAME length");
			send(NPPM_GETLANGUAGENAME, L_CPP, reinterpret_cast<intptr_t>(name));
			check(QString::fromWCharArray(name) == QStringLiteral("C++"), "plugins: NPPM_GETLANGUAGENAME", QString::fromWCharArray(name));
			const intptr_t clen = send(NPPM_GETPLUGINSCONFIGDIR, 0, 0);
			std::wstring cfg(static_cast<size_t>(clen) + 1, L'\0');
			check(send(NPPM_GETPLUGINSCONFIGDIR, cfg.size(), reinterpret_cast<intptr_t>(cfg.data())) == TRUE
				&& QString::fromWCharArray(cfg.c_str()) == Npp::configDir() + QStringLiteral("/plugins/config"), "plugins: NPPM_GETPLUGINSCONFIGDIR",
				QString::fromWCharArray(cfg.c_str()));
		}
		{
			const intptr_t ver = send(NPPM_GETNPPVERSION, TRUE);
			check(HIWORD(ver) == Npp::version().section(QLatin1Char('.'), 0, 0).toInt() && LOWORD(ver) >= 100, "plugins: NPPM_GETNPPVERSION",
				QStringLiteral("%1.%2").arg(HIWORD(ver)).arg(LOWORD(ver)));
		}
		{
			int start = 0;
			check(send(NPPM_ALLOCATEMARKER, 3, reinterpret_cast<intptr_t>(&start)) && start == 1, "plugins: NPPM_ALLOCATEMARKER", QString::number(start));
			check(send(NPPM_ALLOCATEINDICATOR, 1, reinterpret_cast<intptr_t>(&start)) && start == 9, "plugins: NPPM_ALLOCATEINDICATOR", QString::number(start));
			check(send(NPPM_ALLOCATECMDID, 2, reinterpret_cast<intptr_t>(&start)) && start == 23000, "plugins: NPPM_ALLOCATECMDID", QString::number(start));
			send(NPPM_MENUCOMMAND, 0, start);
			check(report().dynamicCommands == 1, "plugins: WM_COMMAND of an allocated id goes to messageProc");
		}
		check(send(NPPM_GETNBOPENFILES, 0, PRIMARY_VIEW) == w->viewTabs(0)->count(), "plugins: NPPM_GETNBOPENFILES");
		check(send(NPPM_GETBUFFERIDFROMPOS, static_cast<uintptr_t>(w->indexOf(0, pb)), 0) == reinterpret_cast<intptr_t>(pb), "plugins: NPPM_GETBUFFERIDFROMPOS");
		check(send(NPPM_GETPOSFROMBUFFERID, reinterpret_cast<uintptr_t>(pb), 0) == w->indexOf(0, pb), "plugins: NPPM_GETPOSFROMBUFFERID");
		check(send(NPPM_GETCURRENTDOCINDEX, 0, 0) == w->indexOf(0, pb) && send(NPPM_GETCURRENTDOCINDEX, 0, 1) == -1, "plugins: NPPM_GETCURRENTDOCINDEX");
		check(send(NPPM_GETBUFFERFORMAT, reinterpret_cast<uintptr_t>(pb)) == SC_EOL_LF && send(NPPM_GETBUFFERENCODING, reinterpret_cast<uintptr_t>(pb)) == 4,
			"plugins: NPPM_GETBUFFERFORMAT / NPPM_GETBUFFERENCODING");
		{
			e->ex(SCI_GOTOLINE, 0);
			check(send(NPPM_GETCURRENTLINE) == 0 && send(NPPM_GETCURRENTCOLUMN) == 0, "plugins: NPPM_GETCURRENTLINE / COLUMN");
		}
		{
			HWND sci = reinterpret_cast<HWND>(send(NPPM_CREATESCINTILLAHANDLE, 0, 0));
			SendMessage(sci, SCI_SETTEXT, 0, reinterpret_cast<LPARAM>("abc"));
			check(sci && SendMessage(sci, SCI_GETLENGTH, 0, 0) == 3, "plugins: NPPM_CREATESCINTILLAHANDLE");
			auto fn = reinterpret_cast<sptr_t (*)(sptr_t, unsigned int, uptr_t, sptr_t)>(SendMessage(pm->scintillaHandle(0), SCI_GETDIRECTFUNCTION, 0, 0));
			const sptr_t ptr = SendMessage(pm->scintillaHandle(0), SCI_GETDIRECTPOINTER, 0, 0);
			check(fn && ptr && fn(ptr, SCI_GETLENGTH, 0, 0) == w->viewEdit(0)->ex(SCI_GETLENGTH), "plugins: SCI_GETDIRECTFUNCTION through the handle");
		}
		send(NPPM_SETSTATUSBAR, STATUSBAR_DOC_TYPE, reinterpret_cast<intptr_t>(L"From plugin"));
		check(MainWindowAccess::statusLang(*w) == QStringLiteral("From plugin"), "plugins: NPPM_SETSTATUSBAR", MainWindowAccess::statusLang(*w));
		{
			const QString copy = work.filePath(QStringLiteral("plugin-copy.txt"));
			const std::wstring wcopy = copy.toStdWString();
			check(send(NPPM_SAVECURRENTFILEAS, TRUE, reinterpret_cast<intptr_t>(wcopy.c_str())) && QFileInfo::exists(copy) && w->currentBuffer()->path == pluginFile,
				"plugins: NPPM_SAVECURRENTFILEAS (copy)");
		}
		r = report();
		const int saved = nppn(r, NPPN_FILESAVED);
		e->ex(SCI_APPENDTEXT, 1, reinterpret_cast<sptr_t>("x"));
		check(send(NPPM_SAVECURRENTFILE) == TRUE, "plugins: NPPM_SAVECURRENTFILE");
		r = report();
		check(nppn(r, NPPN_FILESAVED) == saved + 1 && nppn(r, NPPN_FILEBEFORESAVE) >= saved + 1, "plugins: NPPN_FILEBEFORESAVE / NPPN_FILESAVED");
		const int closed = nppn(r, NPPN_FILECLOSED);
		w->closeBuffer(pb, false);
		r = report();
		check(nppn(r, NPPN_FILECLOSED) == closed + 1 && idFrom(r, NPPN_FILECLOSED) == reinterpret_cast<uintptr_t>(pb)
			&& idFrom(r, NPPN_FILEBEFORECLOSE) == reinterpret_cast<uintptr_t>(pb), "plugins: NPPN_FILEBEFORECLOSE / NPPN_FILECLOSED");
		check(r.badHwndFrom == 0, "plugins: hwndFrom of the notifications", QString::number(r.badHwndFrom));
		e->ex(SCI_SETSAVEPOINT);
		w->closeBuffer(w->currentBuffer(), false);
	}

	std::printf("[dialogs]\n");
	{
		w->openFile(work.filePath(QStringLiteral("hello.cpp")));
		shotModal(QStringLiteral("20-preferences"), [&] { NppDialogs::preferences(w); });
		shotModal(QStringLiteral("21-style-configurator"), [&] { NppDialogs::styleConfigurator(w); });
		shotModal(QStringLiteral("22-shortcut-mapper"), [&] { NppDialogs::shortcutMapper(w); });
		shotModal(QStringLiteral("23-goto"), [&] { NppDialogs::goToLine(w, v); });
		shotModal(QStringLiteral("24-column-editor"), [&] { NppDialogs::columnEditor(w, v); });
		shotModal(QStringLiteral("25-about"), [&] { w->command(IDM_ABOUT); });
		shotModal(QStringLiteral("26-debug-info"), [&] { w->command(IDM_DEBUGINFO); });
		shotModal(QStringLiteral("27-summary"), [&] { w->command(IDM_VIEW_SUMMARY); });
		shotModal(QStringLiteral("28-windows"), [&] { w->command(IDM_WINDOW_WINDOWS); });
	}

	for (Buffer* x : w->allBuffers())
		w->closeBuffer(x, false);
	delete w;

	std::printf("\n%d checks, %d failed\n", g_checks, g_failures);
	return g_failures ? 1 : 0;
}
