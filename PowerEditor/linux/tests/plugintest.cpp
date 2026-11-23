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

// Exercise the ported plugins together in a separate profile, through their menu
// commands and key strokes. The editor self test loads a smaller plugin set.
// npp_plugintest [screenshot folder]

#include <cstdio>
#include <functional>

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDesktopServices>
#include <QDir>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QStatusBar>
#include <QTextEdit>
#include <QPlainTextEdit>
#include <QListWidget>
#include <QLineEdit>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QTreeWidget>
#include <QTreeWidgetItemIterator>
#include <QDockWidget>
#include <QPushButton>
#include <QProcess>
#include <QRegularExpression>
#include <QTemporaryDir>
#include <QThread>
#include <QTimer>
#include <QContextMenuEvent>
#include <QUrl>

#include "EditView.h"
#include "LangStyles.h"
#include "Localization.h"
#include "MainWindow.h"
#include "../src/PluginsManager.h"

namespace
{
	int g_failures = 0;
	int g_checks = 0;
	QString g_shotDir;

	void check(bool ok, const QString& what, const QString& detail = QString())
	{
		++g_checks;
		if (ok)
			std::printf("  ok    %s\n", qPrintable(what));
		else
		{
			++g_failures;
			std::printf("  FAIL  %s %s\n", qPrintable(what), qPrintable(detail));
		}
	}

	void settle(int rounds = 5)
	{
		for (int i = 0; i < rounds; ++i)
			QApplication::processEvents();
	}

	// events for ms milliseconds of real time (timers of plugins)
	void waitMs(int ms)
	{
		QElapsedTimer elapsed;
		elapsed.start();
		while (elapsed.elapsed() < ms)
			QApplication::processEvents(QEventLoop::AllEvents, 20);
	}

	void shot(QWidget* w, const QString& name)
	{
		if (g_shotDir.isEmpty() || !w)
			return;
		settle();
		w->grab().save(g_shotDir + QLatin1Char('/') + name + QStringLiteral(".png"));
	}

	QString docText(EditView* v)
	{
		return QString::fromUtf8(v->wholeText());
	}

	void setDoc(EditView* v, const QByteArray& text)
	{
		v->ex(SCI_SETTEXT, 0, reinterpret_cast<sptr_t>(text.constData()));
	}

	QString selection(EditView* v)
	{
		const sptr_t a = v->ex(SCI_GETSELECTIONSTART);
		const sptr_t b = v->ex(SCI_GETSELECTIONEND);
		return QString::fromUtf8(v->wholeText().mid(static_cast<int>(a), static_cast<int>(b - a)));
	}

	// a key typed in the editor, through the event filters of the application (keyboard hooks)
	void typeKey(QWidget* target, int key, const QString& text, Qt::KeyboardModifiers mods = Qt::NoModifier)
	{
		QKeyEvent press(QEvent::KeyPress, key, mods, text);
		QApplication::sendEvent(target, &press);
		QKeyEvent release(QEvent::KeyRelease, key, mods, text);
		QApplication::sendEvent(target, &release);
		settle(2);
	}

	QMenu* pluginsMenu(MainWindow* w)
	{
		for (QAction* a : w->menuBar()->actions())
		{
			if (a->menu() && a->text().remove(QLatin1Char('&')) == QStringLiteral("Plugins"))
				return a->menu();
		}
		return nullptr;
	}

	QMenu* pluginMenu(MainWindow* w, const QString& name)
	{
		QMenu* m = pluginsMenu(w);
		for (QAction* a : m ? m->actions() : QList<QAction*>())
		{
			if (a->menu() && a->text().remove(QLatin1Char('&')) == name)
				return a->menu();
		}
		return nullptr;
	}

	QAction* pluginAction(MainWindow* w, const QString& plugin, const QString& item)
	{
		QMenu* m = pluginMenu(w, plugin);
		for (QAction* a : m ? m->actions() : QList<QAction*>())
		{
			if (a->text().remove(QLatin1Char('&')) == item)
				return a;
		}
		return nullptr;
	}

	QStringList menuItems(QMenu* m)
	{
		QStringList items;
		for (QAction* a : m ? m->actions() : QList<QAction*>())
			items << (a->isSeparator() ? QStringLiteral("|") : a->text().remove(QLatin1Char('&')));
		return items;
	}

	// a top level window of a plugin (modeless dialogs): its title, then it is closed
	bool closeWindowTitled(const QString& title, const QString& shotName)
	{
		settle(10);
		for (QWidget* t : QApplication::topLevelWidgets())
		{
			if (t->isVisible() && t->windowTitle().contains(title, Qt::CaseInsensitive) && !qobject_cast<MainWindow*>(t))
			{
				shot(t, shotName);
				t->hide();
				return true;
			}
		}
		return false;
	}

	// a modal dialog opened by fn, grabbed and closed
	// the texts of the labels, edits and lists of a window
	QString windowText(QWidget* w)
	{
		QStringList texts;
		for (QWidget* c : w->findChildren<QWidget*>())
		{
			if (auto* l = qobject_cast<QLabel*>(c))
				texts << l->text();
			else if (auto* e = qobject_cast<QLineEdit*>(c))
				texts << e->text();
			else if (auto* t = qobject_cast<QTextEdit*>(c))
				texts << t->toPlainText();
			else if (auto* p = qobject_cast<QPlainTextEdit*>(c))
				texts << p->toPlainText();
			else if (auto* lw = qobject_cast<QListWidget*>(c))
			{
				for (int i = 0; i < lw->count(); ++i)
					texts << lw->item(i)->text();
			}
		}
		return texts.join(QLatin1Char('\n'));
	}

	bool modalShown(const QString& shotName, const std::function<void()>& fn, QString* title = nullptr, QString* text = nullptr)
	{
		bool shown = false;
		bool fired = false;
		int ticks = 0;
		// a QTimer of this function: it never fires after the function has returned. It looks for
		// the dialog every 100 ms (some plugins work a while before they show it), 5 s at most.
		QTimer timer;
		timer.setInterval(100);
		QObject::connect(&timer, &QTimer::timeout, [&]
		{
			QWidget* d = QApplication::activeModalWidget();
			if (!d)
			{
				// a dialog of a plugin that Qt does not count as modal: the top level dialog shown
				for (QWidget* t : QApplication::topLevelWidgets())
				{
					if (t->isVisible() && (t->windowFlags() & Qt::Dialog) == Qt::Dialog && !qobject_cast<MainWindow*>(t))
						d = t;
				}
			}
			if (!d && ++ticks < 50)
				return;
			fired = true;
			timer.stop();
			std::printf("  (modal: %s)\n", d ? qPrintable(d->metaObject()->className() + QStringLiteral(" ") + d->windowTitle()) : "none");
			if (d)
			{
				shown = true;
				if (title)
					*title = d->windowTitle();
				if (text)
					*text = windowText(d);
				shot(d, shotName);
				// the button of the dialog (the close button of the window does nothing to
				// dialogs that do not handle IDCANCEL, like on Windows)
				QList<QPushButton*> buttons = d->findChildren<QPushButton*>();
				QPushButton* button = nullptr;
				for (QPushButton* b : buttons)
				{
					if (b->isEnabled() && (b->isDefault() || !button))
						button = b;
				}
				std::printf("  (button: %s, enabled %d)\n", button ? qPrintable(button->text()) : "none", button ? button->isEnabled() : 0);
				if (button)
					button->click();
				else
					d->close();
			}
		});
		timer.start();
		fn();
		// a dialog that is not modal: fn has returned, wait for the timer
		QElapsedTimer waited;
		waited.start();
		while (!fired && waited.elapsed() < 2000)
			QApplication::processEvents(QEventLoop::AllEvents, 50);
		timer.stop();
		if (!fired)
			std::printf("  (modal: none)\n");
		settle();
		return shown;
	}

	struct PluginSpec
	{
		const char* folder;   // plugins/<folder>/<folder>.so
		const char* menu;     // name in the Plugins menu (getName)
		QStringList items;    // its menu
	};

	class FileOpenReceiver : public QObject
	{
		Q_OBJECT

	public:
		QString path;
		QByteArray contents;

	public slots:
		void open(const QUrl& url)
		{
			path = url.toLocalFile();
			QFile file(path);
			if (file.open(QIODevice::ReadOnly))
				contents = file.readAll();
		}
	};
}

int main(int argc, char** argv)
{
	QTemporaryDir config;
	QTemporaryDir work;
	qputenv("XDG_CONFIG_HOME", config.path().toLocal8Bit());
	qputenv("XDG_DATA_HOME", (config.path() + QStringLiteral("/data")).toLocal8Bit());
	qputenv("TMPDIR", work.path().toLocal8Bit());
	if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM"))
		qputenv("QT_QPA_PLATFORM", "offscreen");
	QApplication::setApplicationName(QStringLiteral("linuxpad++"));
	QApplication app(argc, argv);
	if (argc > 1)
	{
		g_shotDir = QString::fromLocal8Bit(argv[1]);
		QDir().mkpath(g_shotDir);
	}

	const std::vector<PluginSpec> specs = {
		{"SurroundSelection", "SurroundSelection", {QStringLiteral("Enable"), QStringLiteral("|"), QStringLiteral("About...")}},
		{"BetterMultiSelection", "BetterMultiSelection", {QStringLiteral("Enable"), QStringLiteral("|"), QStringLiteral("About...")}},
		{"SelectQuotedText", "SelectQuotedText", {QStringLiteral("Select quoted text"), QStringLiteral("|"), QStringLiteral("About...")}},
		{"IndentByFold", "Indent By Fold", {QStringLiteral("Reindent File"), QStringLiteral("|"), QStringLiteral("Open Online Manual"), QStringLiteral("|"), QStringLiteral("About...")}},
		{"XBrackets", "XBrackets Lite", {QStringLiteral("Autocomplete brackets"), QStringLiteral("Highlight brackets"), QStringLiteral("Settings..."), QStringLiteral("|"),
			QStringLiteral("Go To Matching Bracket"), QStringLiteral("Go To Nearest Bracket"), QStringLiteral("Select To Matching Bracket"),
			QStringLiteral("Select To Nearest Brackets"), QStringLiteral("|"), QStringLiteral("Help...")}},
		{"NPPJSONViewer", "JSON Viewer", {QStringLiteral("Show JSON Viewer"), QStringLiteral("Format JSON"), QStringLiteral("Format JSON Lines"),
			QStringLiteral("Compress JSON"), QStringLiteral("Sort by key (ascending)"), QStringLiteral("|"), QStringLiteral("Settings"), QStringLiteral("About")}},
		{"CommentToggler", "Comment Toggler", {QStringLiteral("Toggle comment"), QStringLiteral("Plug-in homepage")}},
		{"QuickText", "QuickText", {QStringLiteral("Replace Snip"), QStringLiteral("|"), QStringLiteral("Open Snips File"), QStringLiteral("Open Config File"),
			QStringLiteral("Refresh Configuration"), QStringLiteral("|"), QStringLiteral("Settings")}},
		{"SelectToClipboard", "Selection to Clipboard", {QStringLiteral("Auto copy selection to clipboard"), QStringLiteral("About")}},
		{"FoldingLineHider", "Folding Line Hider", {QStringLiteral("Hide Folding Line (Toggle)"), QStringLiteral("Collapse Current Level")}},
		{"ColumnTools", "Column Tools", {QStringLiteral("Enable all"), QStringLiteral("|"), QStringLiteral("Column highlight"), QStringLiteral("Ruler"),
			QStringLiteral("|"), QStringLiteral("Settings")}},
		{"HTMLTag", "HTML Tag", {QStringLiteral("Find matching tag"), QStringLiteral("Select matching tags"), QStringLiteral("Select tag and contents"),
			QStringLiteral("Select tag contents only")}},
		{"qkNppReverseLines", "Reverse Lines", {QStringLiteral("Selection"), QStringLiteral("Document"), QStringLiteral("About")}},
		{"urlPlugin", "URL Plugin", {QStringLiteral("URL Plugin Settings"), QStringLiteral("Encode URL"), QStringLiteral("Decode URL")}},
		{"NppExport", "NppExport", {QStringLiteral("Export to RTF"), QStringLiteral("Export to HTML"), QStringLiteral("Copy RTF to clipboard"),
			QStringLiteral("Copy HTML to clipboard"), QStringLiteral("Copy all formats to clipboard")}},
		{"TidyText", "TidyText", {QStringLiteral("Highlight Non-Printing Characters"), QStringLiteral("Clear Highlights"),
			QStringLiteral("Replace Non-Printing Characters"), QStringLiteral("Next Match"), QStringLiteral("Previous Match")}},
		{"NppTextViz", "TextViz", {QStringLiteral("Show selected or reset all lines"), QStringLiteral("Hide selected or hide all lines"),
			QStringLiteral("Invert selected visibility")}},
		{"selectNLaunch", "Select 'N' Launch", {QStringLiteral("Selection to PDF"), QStringLiteral("Base64 to PDF"),
			QStringLiteral("Selection to Certificate"), QStringLiteral("Selection to CRL"), QStringLiteral("|"), QStringLiteral("Edit User Commands")}},
		{"pork2sausage", "Pork to Sausage", {QStringLiteral("Base64 encode"), QStringLiteral("Base64 decode"),
			QStringLiteral("Sort lines"), QStringLiteral("|"), QStringLiteral("Edit User Commands")}},
		{"SqlColumnHighlighter", "SQL Column Highlighter", {QStringLiteral("Enable/Disable highlighting"),
			QStringLiteral("Highlight at caret"), QStringLiteral("About...")}},
		{"NppZenHankaku", "全角半角変換", {QStringLiteral("全角 → 半角（すべて）"), QStringLiteral("全角 → 半角（英数字）")}},
		{"NppRestoreCursorPosition", "NppRestoreCursorPosition", {QStringLiteral("Save cursor position"), QStringLiteral("Clear history")}},
		{"NppLdapFormatter", "LDAP Formatter", {QStringLiteral("Format LDAP (Pretty)"), QStringLiteral("Format LDAP (Compact)")}},
		{"NppStripIpAndHost", "NppStripIpAndHost", {QStringLiteral("Strip IPs and Hostnames"), QStringLiteral("Undo Last Strip"),
			QStringLiteral("Replace Settings...")}},
		{"NppImagePreview", "Image Preview", {QStringLiteral("Enable Image Preview"), QStringLiteral("Preview Image at Cursor"),
			QStringLiteral("Clear Thumbnail Cache"), QStringLiteral("About")}},
		{"YamlProperties", "YamlProperties", {QStringLiteral("YAML → Properties"), QStringLiteral("Properties → YAML"),
			QStringLiteral("Auto-detect and convert"), QStringLiteral("Open conversion result in a new tab"),
			QStringLiteral("Sort keys during conversion")}},
		{"SimplifyJson", "Simplify JSON", {QStringLiteral("Escape JSON String"), QStringLiteral("Unescape JSON String"),
			QStringLiteral("Beautify JSON"), QStringLiteral("Minify JSON")}},
		{"EmojiDescription", "Emoji Description", {QStringLiteral("Show Character Info"), QStringLiteral("|"), QStringLiteral("Copy character details...")}},
		{"NppPluginDemo", "Notepad++ plugin demo", {QStringLiteral("Hello Notepad++"), QStringLiteral("Hello (with FX)"), QStringLiteral("What is Notepad++?")}},
		{"NppTaskList", "Task List", {QStringLiteral("Show Task List"), QStringLiteral("Reload Task List Configuration"), QStringLiteral("About Task List")}},
		{"Language_Selector", "Language Selector", {QStringLiteral("Select Language"), QStringLiteral("Settings"), QStringLiteral("About")}},
		{"NPPColorPicker", "NPPColorPicker", {QStringLiteral("Color Picker - Enabled"), QStringLiteral("Color Picker - Insert Color"),
			QStringLiteral("Color Picker - Settings"), QStringLiteral("Color Picker - About")}},
		{"NppFavorites", "NppFavorites", {QStringLiteral("/etc/hosts")}},
		{"TCSyslogFinder", "TC Syslog Finder", {QStringLiteral("Open Latest TC Syslog"), QStringLiteral("|"), QStringLiteral("Open Syslog Folder")}},
		{"InputSequence", "Input Sequence", {}},
		{"OCamlLexer", "OCaml Lexer", {QStringLiteral("Lint now (olint)")}},
		{"HLASMLexer", "HLASM Lexer", {QStringLiteral("About HLASM Lexer")}},
		{"RAScript", "RAScript", {QStringLiteral("Hello Notepad++")}},
		{"HiddenLexers", "HiddenLexers", {QStringLiteral("Edit Config File: HiddenLexers.json")}},
		{"JSMinNPP", "JSTool", {QStringLiteral("JSMin"), QStringLiteral("JSMin (New file)"), QStringLiteral("|"), QStringLiteral("JSFormat")}},
		{"NppExec", "NppExec", {QStringLiteral("Execute NppExec Script..."), QStringLiteral("Execute Previous NppExec Script"),
			QStringLiteral("Execute Selected Text"), QStringLiteral("Execute Clipboard Text")}},
		{"ForeLexer", "Notepad++ Syntax Plugin for Foresight Platform Fore", {QStringLiteral("About Notepad++ Syntax Plugin for Foresight Platform Fore")}},
		{"Explorer", "Explorer", {QStringLiteral("Explorer..."), QStringLiteral("Favorites..."), QStringLiteral("|"),
			QStringLiteral("Go to Path..."), QStringLiteral("Clear Filter")}},
		{"DSpellCheck", "DSpellCheck", {QStringLiteral("Spell Check Document Automatically"), QStringLiteral("Find Next Misspelling"),
			QStringLiteral("Find Previous Misspelling"), QStringLiteral("Change Current Language")}},
		{"HexEditor", "HEX-Editor", {QStringLiteral("View in HEX"), QStringLiteral("Compare HEX"), QStringLiteral("Clear Compare Result"),
			QStringLiteral("|"), QStringLiteral("Insert Columns..."), QStringLiteral("Pattern Replace...")}},
		{"XMLTools", "XML Tools", {QStringLiteral("Enable XML syntax auto-check"), QStringLiteral("Check XML syntax now"), QStringLiteral("|"),
			QStringLiteral("Enable auto-validation"), QStringLiteral("Validate now")}},
		{"ComparePlus", "ComparePlus", {QStringLiteral("Set as First to Compare"), QStringLiteral("Compare"), QStringLiteral("Compare Selections"),
			QStringLiteral("Find Unique Lines"), QStringLiteral("Find Unique Lines in Selections"), QStringLiteral("|"),
			QStringLiteral("Diff since last Save"), QStringLiteral("Compare file/selection to Clipboard"), QStringLiteral("SVN Diff"),
			QStringLiteral("Git Diff"), QStringLiteral("Git Diff since Commit")}},
		{"NppEditorConfig", "EditorConfig", {QStringLiteral("Reload EditorConfig for this file"),
			QStringLiteral("Show EditorConfig settings for this file"), QStringLiteral("|"), QStringLiteral("Fix indentation")}},
	};
	QStringList folders;
	for (const PluginSpec& s : specs)
		folders << QString::fromLatin1(s.folder);
	// $NPP_PLUGINTEST_PLUGINS (folders separated by ':'): load only these (the checks of the others fail)
	if (!qEnvironmentVariableIsEmpty("NPP_PLUGINTEST_PLUGINS"))
		folders = qEnvironmentVariable("NPP_PLUGINTEST_PLUGINS").split(QLatin1Char(':'));
	qputenv("NPP_PLUGINS_ONLY", folders.join(QLatin1Char(':')).toLocal8Bit());

	NppPrefs& prefs = Npp::prefs();
	prefs.load();
	Localization::instance().load(prefs.localization);
	StyleManager::instance().loadLangs();

	// Language Selector asks for the language of every new tab: not in this test
	{
		const QString pluginsConfig = config.path() + QStringLiteral("/linuxpad++/plugins/config");
		QDir().mkpath(pluginsConfig);
		QFile ini(pluginsConfig + QStringLiteral("/LanguageSelector.ini"));
		if (ini.open(QIODevice::WriteOnly))
			ini.write("AutoShowOnNewTab=0\n");
	}
	// Explorer starts in a folder of the test (paths of Windows: "C:" is the root folder)
	const QString explorerDir = config.path() + QStringLiteral("/explore");
	{
		QDir().mkpath(explorerDir + QStringLiteral("/subfolder"));
		QFile f(explorerDir + QStringLiteral("/alpha.txt"));
		if (f.open(QIODevice::WriteOnly))
			f.write("explorer-open-ok\n");
		QFile ini(config.path() + QStringLiteral("/linuxpad++/plugins/config/Explorer.ini"));
		if (ini.open(QIODevice::WriteOnly))
		{
			const QString winPath = QStringLiteral("C:") + QString(explorerDir).replace(QLatin1Char('/'), QLatin1Char('\\')) + QLatin1Char('\\');
			ini.write("[Explorer]\nLastPath=" + winPath.toUtf8() + "\nAutoUpdate=0\n");
		}
	}

	auto* w = new MainWindow;
	w->resize(1100, 720);
	w->loadPlugins(true);
	w->show();
	w->openFromCommandLine({}, OpenOptions());
	w->pluginsReady(QString());
	settle();
	EditView* e = w->editView();

	std::printf("[loading]\n");
	{
		PluginsManager* pm = w->pluginsManager();
		const QStringList mods = pm->loadedModules();
		check(pm->loadErrors().isEmpty(), QStringLiteral("no load error"), pm->loadErrors().join(QLatin1Char('\n')));
		// $NPP_PLUGINTEST_MENUS: the menus of the plugins, to write the specs of new ones
		if (!qEnvironmentVariableIsEmpty("NPP_PLUGINTEST_MENUS"))
		{
			if (QMenu* plugins = pluginsMenu(w))
			{
				for (QAction* a : plugins->actions())
				{
					if (a->menu())
						std::printf("  menu %s: %s\n", qPrintable(a->text()), qPrintable(menuItems(a->menu()).join(QStringLiteral(" | "))));
				}
			}
		}
		for (const PluginSpec& s : specs)
		{
			const QString folder = QString::fromLatin1(s.folder);
			if (!folders.contains(folder))
				continue; // not loaded ($NPP_PLUGINTEST_PLUGINS)
			check(mods.contains(folder + QStringLiteral(".so")), folder + QStringLiteral(": loaded"), mods.join(QLatin1Char(' ')));
			const QStringList items = menuItems(pluginMenu(w, QString::fromUtf8(s.menu)));
			// the first items: some plugins add more after them
			check(items.mid(0, static_cast<int>(s.items.size())) == s.items, folder + QStringLiteral(": menu"), items.join(QStringLiteral(", ")));
		}
	}

	std::printf("[SurroundSelection]\n");
	{
		w->newDocument();
		e = w->editView();
		setDoc(e, "abc def");
		e->ex(SCI_SETSEL, 0, 3);
		typeKey(e, Qt::Key_BracketLeft, QStringLiteral("["));
		check(docText(e) == QStringLiteral("[abc] def"), QStringLiteral("SurroundSelection: [ around the selection"), docText(e));
		typeKey(e, Qt::Key_Apostrophe, QStringLiteral("'"));
		check(docText(e) == QStringLiteral("['abc'] def"), QStringLiteral("SurroundSelection: ' around the selection"), docText(e));
		e->ex(SCI_SETEMPTYSELECTION, 9);
		check(pluginAction(w, QStringLiteral("SurroundSelection"), QStringLiteral("Enable"))
			&& pluginAction(w, QStringLiteral("SurroundSelection"), QStringLiteral("Enable"))->isChecked(), QStringLiteral("SurroundSelection: enabled at start"));
		if (QAction* about = pluginAction(w, QStringLiteral("SurroundSelection"), QStringLiteral("About...")))
		{
			about->trigger();
			check(closeWindowTitled(QString(), QStringLiteral("p-surroundselection-about")), QStringLiteral("SurroundSelection: About dialog"));
		}
	}

	std::printf("[XBrackets]\n");
	{
		w->newDocument();
		e = w->editView();
		w->languageSet(L_CPP);
		setDoc(e, "");
		typeKey(e, Qt::Key_ParenLeft, QStringLiteral("("));
		check(docText(e) == QStringLiteral("()") && e->ex(SCI_GETCURRENTPOS) == 1, QStringLiteral("XBrackets: ( completed with )"),
			docText(e) + QStringLiteral(" pos ") + QString::number(e->ex(SCI_GETCURRENTPOS)));
		typeKey(e, Qt::Key_ParenRight, QStringLiteral(")"));
		check(docText(e) == QStringLiteral("()") && e->ex(SCI_GETCURRENTPOS) == 2, QStringLiteral("XBrackets: ) typed over the completed one"), docText(e));
		setDoc(e, "f(a[1], b)");
		e->ex(SCI_GOTOPOS, 1);
		if (QAction* a = pluginAction(w, QStringLiteral("XBrackets Lite"), QStringLiteral("Go To Matching Bracket")))
			a->trigger();
		check(e->ex(SCI_GETCURRENTPOS) == 9 || e->ex(SCI_GETCURRENTPOS) == 10, QStringLiteral("XBrackets: Go To Matching Bracket"), QString::number(e->ex(SCI_GETCURRENTPOS)));
		e->ex(SCI_GOTOPOS, 4);
		if (QAction* a = pluginAction(w, QStringLiteral("XBrackets Lite"), QStringLiteral("Select To Nearest Brackets")))
			a->trigger();
		check(selection(e).contains(QStringLiteral("1")), QStringLiteral("XBrackets: Select To Nearest Brackets"), selection(e));
		if (QAction* a = pluginAction(w, QStringLiteral("XBrackets Lite"), QStringLiteral("Settings...")))
		{
			// XBrackets Lite has no settings dialog: its user configuration is opened in Notepad++
			a->trigger();
			settle();
			const QString path = w->currentBuffer() ? w->currentBuffer()->path : QString();
			check(path.endsWith(QStringLiteral("/plugins/config/XBrackets_UserConfig.json")) && QFileInfo::exists(path),
				QStringLiteral("XBrackets: Settings opens its configuration file"), path);
		}
	}

	std::printf("[SelectQuotedText]\n");
	{
		w->newDocument();
		e = w->editView();
		w->languageSet(L_CPP);
		setDoc(e, "s = \"hello world\";");
		e->ex(SCI_COLOURISE, 0, -1);
		e->ex(SCI_GOTOPOS, 7);
		if (QAction* a = pluginAction(w, QStringLiteral("SelectQuotedText"), QStringLiteral("Select quoted text")))
			a->trigger();
		check(selection(e) == QStringLiteral("hello world"), QStringLiteral("SelectQuotedText: string of the lexer"), selection(e));
		w->languageSet(L_TEXT);
		setDoc(e, "x = 'single' + y");
		e->ex(SCI_GOTOPOS, 7);
		if (QAction* a = pluginAction(w, QStringLiteral("SelectQuotedText"), QStringLiteral("Select quoted text")))
			a->trigger();
		check(selection(e) == QStringLiteral("single"), QStringLiteral("SelectQuotedText: the word in plain text"), selection(e));
	}

	std::printf("[IndentByFold]\n");
	{
		w->newDocument();
		e = w->editView();
		w->languageSet(L_CPP);
		setDoc(e, "void f()\n{\nint x;\nif (x)\n{\nx++;\n}\n}\n");
		e->ex(SCI_COLOURISE, 0, -1);
		if (QAction* a = pluginAction(w, QStringLiteral("Indent By Fold"), QStringLiteral("Reindent File")))
			a->trigger();
		settle();
		const QString t = docText(e);
		const QStringList lines = t.split(QLatin1Char('\n'));
		check(lines.size() > 6 && lines[2].startsWith(QLatin1Char('\t')) && lines[5].startsWith(QStringLiteral("\t\t")) && !lines[1].startsWith(QLatin1Char('\t')),
			QStringLiteral("IndentByFold: Reindent File by the fold levels"), QString(t).replace(QLatin1Char('\n'), QStringLiteral("\\n")).replace(QLatin1Char('\t'), QStringLiteral("\\t")));
	}

	std::printf("[JSON Viewer]\n");
	{
		w->newDocument();
		e = w->editView();
		w->languageSet(L_JSON);
		auto run = [&](const char* item)
		{
			if (QAction* a = pluginAction(w, QStringLiteral("JSON Viewer"), QString::fromLatin1(item)))
				a->trigger();
			settle();
		};
		setDoc(e, "{\"b\":1,\"a\":[1,2]}");
		run("Format JSON");
		const QString formatted = docText(e);
		check(formatted.count(QLatin1Char('\n')) >= 4 && formatted.contains(QStringLiteral("\"b\": 1")), QStringLiteral("JSON Viewer: Format JSON"),
			QString(formatted).replace(QLatin1Char('\n'), QStringLiteral("\\n")));
		run("Compress JSON");
		check(docText(e) == QStringLiteral("{\"b\":1,\"a\":[1,2]}"), QStringLiteral("JSON Viewer: Compress JSON"), docText(e));
		run("Sort by key (ascending)");
		const QString sorted = docText(e);
		check(sorted.indexOf(QStringLiteral("\"a\"")) >= 0 && sorted.indexOf(QStringLiteral("\"a\"")) < sorted.indexOf(QStringLiteral("\"b\"")),
			QStringLiteral("JSON Viewer: Sort by key"), QString(sorted).replace(QLatin1Char('\n'), QStringLiteral("\\n")));
		run("Show JSON Viewer");
		settle(20);
		QTreeWidget* tree = nullptr;
		for (QDockWidget* d : w->findChildren<QDockWidget*>())
		{
			if (d->isVisible() && d->windowTitle().contains(QStringLiteral("JSON")))
				tree = d->findChild<QTreeWidget*>();
		}
		int items = 0;
		std::function<void(QTreeWidgetItem*)> count = [&](QTreeWidgetItem* it)
		{
			++items;
			for (int i = 0; i < it->childCount(); ++i)
				count(it->child(i));
		};
		for (int i = 0; tree && i < tree->topLevelItemCount(); ++i)
			count(tree->topLevelItem(i));
		check(tree && items >= 4, QStringLiteral("JSON Viewer: docked panel with the tree of the document"), QString::number(items));
		// the tool bar row of the panel (refresh, validate, format, search) above the tree
		QString layout;
		int buttonsAbove = 0;
		if (tree)
		{
			for (QWidget* c : tree->parentWidget()->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly))
			{
				layout += QStringLiteral("%1(%2,%3 %4x%5%6) ").arg(QString::fromLatin1(c->metaObject()->className())).arg(c->x()).arg(c->y())
					.arg(c->width()).arg(c->height()).arg(c->isVisible() ? QString() : QStringLiteral(" hidden"));
				if (qobject_cast<QPushButton*>(c) && c->isVisible() && c->geometry().bottom() <= tree->geometry().top())
					++buttonsAbove;
			}
		}
		check(buttonsAbove >= 3, QStringLiteral("JSON Viewer: tool bar buttons above the tree"), layout);
		shot(w, QStringLiteral("p-jsonviewer-panel"));
	}

	std::printf("[Comment Toggler]\n");
	{
		w->newDocument();
		e = w->editView();
		w->languageSet(L_CPP);
		setDoc(e, "int a;");
		e->ex(SCI_GOTOPOS, 2);
		QAction* a = pluginAction(w, QStringLiteral("Comment Toggler"), QStringLiteral("Toggle comment"));
		if (a)
			a->trigger();
		settle();
		check(docText(e).startsWith(QStringLiteral("//")) && docText(e).contains(QStringLiteral("int a;")), QStringLiteral("Comment Toggler: comments the line"), docText(e));
		if (a)
			a->trigger();
		settle();
		check(docText(e) == QStringLiteral("int a;"), QStringLiteral("Comment Toggler: uncomments the line"), docText(e));
	}

	std::printf("[QuickText]\n");
	{
		w->newDocument();
		e = w->editView();
		w->languageSet(L_C);
		e->ex(SCI_SETEOLMODE, SC_EOL_LF);
		setDoc(e, "if");
		e->ex(SCI_GOTOPOS, 2);
		auto* replace = pluginAction(w, QStringLiteral("QuickText"), QStringLiteral("Replace Snip"));
		if (replace)
			replace->trigger();
		settle();
		const QString snippet = docText(e);
		check(snippet.startsWith(QStringLiteral("if (")) && snippet.contains(QLatin1Char('{')) && !snippet.contains(QLatin1Char('\r')),
			QStringLiteral("QuickText: expands the C snippet with the editor's line endings"), snippet);
		w->languageSet(L_TEXT);
		setDoc(e, "copyright");
		e->ex(SCI_GOTOPOS, 9);
		if (replace)
			replace->trigger();
		check(docText(e).startsWith(QStringLiteral("Copyright (C)")), QStringLiteral("QuickText: expands a global snippet"), docText(e));
	}

	std::printf("[Selection to Clipboard]\n");
	{
		w->newDocument();
		e = w->editView();
		setDoc(e, "alpha beta");
		QAction* autoCopy = pluginAction(w, QStringLiteral("Selection to Clipboard"), QStringLiteral("Auto copy selection to clipboard"));
		if (autoCopy && !autoCopy->isChecked())
			autoCopy->trigger();
		QApplication::clipboard()->setText(QStringLiteral("-"));
		// the plugin copies a selection that changed since the last update of the same tab
		e->ex(SCI_GOTOPOS, 3);
		settle(20);
		e->ex(SCI_SETSEL, 6, 10);
		settle(20);
		check(QApplication::clipboard()->text() == QStringLiteral("beta"), QStringLiteral("Selection to Clipboard: the selection is copied"), QApplication::clipboard()->text());
		if (autoCopy && autoCopy->isChecked())
			autoCopy->trigger();
	}

	std::printf("[Folding Line Hider]\n");
	{
		e = w->editView();
		QAction* toggle = pluginAction(w, QStringLiteral("Folding Line Hider"), QStringLiteral("Hide Folding Line (Toggle)"));
		const bool hidden = toggle && toggle->isChecked();
		if (toggle)
			toggle->trigger();
		settle();
		const bool hiddenNow = toggle && toggle->isChecked();
		const bool colourSet = e->ex(SCI_GETELEMENTISSET, SC_ELEMENT_FOLD_LINE) != 0;
		check(toggle && hiddenNow != hidden && colourSet == hiddenNow, QStringLiteral("Folding Line Hider: toggles the colour of the fold line"),
			QStringLiteral("checked %1, colour set %2").arg(hiddenNow).arg(colourSet));
		if (toggle)
			toggle->trigger();
	}

	std::printf("[Column Tools]\n");
	{
		w->newDocument();
		e = w->editView();
		setDoc(e, "abcdef\nghijkl");
		e->ex(SCI_GOTOPOS, 3);
		const sptr_t mode = e->ex(SCI_GETEDGEMODE);
		QAction* hi = pluginAction(w, QStringLiteral("Column Tools"), QStringLiteral("Column highlight"));
		if (hi)
			hi->trigger();
		settle();
		check(e->ex(SCI_GETEDGEMODE) == EDGE_LINE && e->ex(SCI_GETEDGECOLUMN) == 3, QStringLiteral("Column Tools: column highlight at the caret"),
			QStringLiteral("mode %1 column %2").arg(e->ex(SCI_GETEDGEMODE)).arg(e->ex(SCI_GETEDGECOLUMN)));
		if (hi)
			hi->trigger();
		settle();
		check(e->ex(SCI_GETEDGEMODE) == mode, QStringLiteral("Column Tools: column highlight off"), QString::number(e->ex(SCI_GETEDGEMODE)));
		QAction* ruler = pluginAction(w, QStringLiteral("Column Tools"), QStringLiteral("Ruler"));
		if (ruler && !ruler->isChecked())
			ruler->trigger();
		settle(20);
		auto* rulerWidget = e->parentWidget()->findChild<QWidget*>(QStringLiteral("columnToolsRuler"));
		check(ruler && ruler->isChecked() && rulerWidget && rulerWidget->isVisible() && rulerWidget->geometry().bottom() < e->geometry().top(),
			QStringLiteral("Column Tools: ruler is visible above the active editor"));
		if (rulerWidget)
		{
			const int x = static_cast<int>(e->ex(SCI_POINTXFROMPOSITION, 0, 4)) + 1;
			const QPoint at = rulerWidget->mapFromGlobal(e->viewport()->mapToGlobal(QPoint(x, 0)));
			const QPoint local(at.x(), rulerWidget->height() / 2);
			QMouseEvent click(QEvent::MouseButtonDblClick, local, rulerWidget->mapToGlobal(local),
				Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
			QApplication::sendEvent(rulerWidget, &click);
		}
		check(e->ex(SCI_GETEDGEMODE) == EDGE_LINE && e->ex(SCI_GETEDGECOLUMN) == 4,
			QStringLiteral("Column Tools: double-click sets an edge line at the ruler column"), QString::number(e->ex(SCI_GETEDGECOLUMN)));
		shot(w, QStringLiteral("p-columntools-ruler"));
		if (ruler)
			ruler->trigger();
		settle();
		check(rulerWidget && !rulerWidget->isVisible(), QStringLiteral("Column Tools: turning the ruler off hides its row"));
	}

	std::printf("[HTML Tag]\n");
	{
		w->newDocument();
		e = w->editView();
		w->languageSet(L_HTML);
		setDoc(e, "<div><b>x</b></div>");
		e->ex(SCI_GOTOPOS, 2);
		if (QAction* a = pluginAction(w, QStringLiteral("HTML Tag"), QStringLiteral("Find matching tag")))
			a->trigger();
		settle();
		check(e->ex(SCI_GETCURRENTPOS) >= 13, QStringLiteral("HTML Tag: Find matching tag"), QString::number(e->ex(SCI_GETCURRENTPOS)));
		e->ex(SCI_GOTOPOS, 9);
		if (QAction* a = pluginAction(w, QStringLiteral("HTML Tag"), QStringLiteral("Select tag contents only")))
			a->trigger();
		settle();
		check(selection(e) == QStringLiteral("x"), QStringLiteral("HTML Tag: Select tag contents only"), selection(e));
		setDoc(e, "a<b>&c \xc3\xa9");
		e->ex(SCI_SELECTALL);
		if (QAction* a = pluginAction(w, QStringLiteral("HTML Tag"), QStringLiteral("Encode entities")))
			a->trigger();
		settle();
		check(docText(e) == QStringLiteral("a&lt;b&gt;&amp;c &eacute;"), QStringLiteral("HTML Tag: Encode entities"), docText(e));
		e->ex(SCI_SELECTALL);
		if (QAction* a = pluginAction(w, QStringLiteral("HTML Tag"), QStringLiteral("Decode entities")))
			a->trigger();
		settle();
		check(docText(e) == QString::fromUtf8("a<b>&c \xc3\xa9"), QStringLiteral("HTML Tag: Decode entities"), docText(e));
	}

	std::printf("[Reverse Lines]\n");
	{
		w->newDocument();
		e = w->editView();
		setDoc(e, "a\nb\nc");
		if (QAction* a = pluginAction(w, QStringLiteral("Reverse Lines"), QStringLiteral("Document")))
			a->trigger();
		settle();
		check(docText(e) == QStringLiteral("c\nb\na"), QStringLiteral("Reverse Lines: the lines of the document"), docText(e));
	}

	std::printf("[URL Plugin]\n");
	{
		w->newDocument();
		e = w->editView();
		setDoc(e, "a b&c=d");
		e->ex(SCI_SELECTALL);
		if (QAction* a = pluginAction(w, QStringLiteral("URL Plugin"), QStringLiteral("Encode URL")))
			a->trigger();
		settle();
		const QString encoded = docText(e);
		check(!encoded.contains(QLatin1Char(' ')) && encoded.contains(QStringLiteral("%20")), QStringLiteral("URL Plugin: Encode URL"), encoded);
		e->ex(SCI_SELECTALL);
		if (QAction* a = pluginAction(w, QStringLiteral("URL Plugin"), QStringLiteral("Decode URL")))
			a->trigger();
		settle();
		check(docText(e) == QStringLiteral("a b&c=d"), QStringLiteral("URL Plugin: Decode URL"), docText(e));
	}

	std::printf("[NppExport]\n");
	{
		w->newDocument();
		e = w->editView();
		w->languageSet(L_CPP);
		setDoc(e, "int main() { return 0; }");
		e->ex(SCI_COLOURISE, 0, -1);
		QApplication::clipboard()->clear();
		if (QAction* a = pluginAction(w, QStringLiteral("NppExport"), QStringLiteral("Copy HTML to clipboard")))
			a->trigger();
		settle();
		const QMimeData* md = QApplication::clipboard()->mimeData();
		QString html;
		if (md)
		{
			for (const QString& f : md->formats())
			{
				if (f.contains(QStringLiteral("html"), Qt::CaseInsensitive))
					html += QString::fromUtf8(md->data(f));
			}
		}
		check(html.contains(QStringLiteral("main")) && html.contains(QLatin1Char('<')), QStringLiteral("NppExport: Copy HTML to clipboard"),
			md ? md->formats().join(QLatin1Char(' ')) + QLatin1Char(' ') + html.left(80) : QString());
	}

	std::printf("[TidyText]\n");
	{
		w->newDocument();
		e = w->editView();
		setDoc(e, "a\xc2\xa0" "b\x01");
		QAction* replace = pluginAction(w, QStringLiteral("TidyText"), QStringLiteral("Replace Non-Printing Characters"));
		QString title;
		const bool shown = replace && modalShown(QStringLiteral("p-tidytext-replace"), [replace] { replace->trigger(); }, &title);
		const QString text = docText(e);
		check(shown && !text.contains(QChar(0x00A0)) && !text.contains(QChar(1)) && text.startsWith(QLatin1Char('a')),
			QStringLiteral("TidyText: Replace Non-Printing Characters"), title + QStringLiteral(": ") + text.toUtf8().toHex());
	}

	std::printf("[TextViz]\n");
	{
		w->newDocument();
		e = w->editView();
		setDoc(e, "one\ntwo\nthree\n");
		// TextViz hides the lines between the two selection endpoints.
		e->ex(SCI_SETSEL, 0, 8);
		if (QAction* a = pluginAction(w, QStringLiteral("TextViz"), QStringLiteral("Hide selected or hide all lines")))
			a->trigger();
		settle();
		check(e->ex(SCI_GETLINEVISIBLE, 1) == 0 && e->ex(SCI_GETLINEVISIBLE, 0) != 0 && e->ex(SCI_GETLINEVISIBLE, 2) != 0,
			QStringLiteral("TextViz: hides the selected interior line"),
			QStringLiteral("%1 %2 %3").arg(e->ex(SCI_GETLINEVISIBLE, 0)).arg(e->ex(SCI_GETLINEVISIBLE, 1)).arg(e->ex(SCI_GETLINEVISIBLE, 2)));
		e->ex(SCI_SETSEL, 0, 0);
		if (QAction* a = pluginAction(w, QStringLiteral("TextViz"), QStringLiteral("Show selected or reset all lines")))
			a->trigger();
		settle();
		check(e->ex(SCI_GETLINEVISIBLE, 1) != 0, QStringLiteral("TextViz: shows all lines again"));
	}

	std::printf("[Select 'N' Launch]\n");
	{
		auto* edit = pluginAction(w, QStringLiteral("Select 'N' Launch"), QStringLiteral("Edit User Commands"));
		if (edit)
			edit->trigger();
		const QString path = w->currentBuffer() ? w->currentBuffer()->path : QString();
		check(edit && path.endsWith(QStringLiteral("/plugins/config/SelectNLaunch.ini")) && QFileInfo::exists(path),
			QStringLiteral("Select 'N' Launch: opens its generated configuration"), path);

		w->newDocument();
		e = w->editView();
		const QByteArray pdfBytes("%PDF-1.7\n\0\x01", 11);
		setDoc(e, pdfBytes.toBase64());
		e->ex(SCI_SELECTALL);
		FileOpenReceiver receiver;
		QDesktopServices::setUrlHandler(QStringLiteral("file"), &receiver, "open");
		if (auto* open = pluginAction(w, QStringLiteral("Select 'N' Launch"), QStringLiteral("Base64 to PDF")))
			open->trigger();
		QDesktopServices::unsetUrlHandler(QStringLiteral("file"));
		check(receiver.path.startsWith(work.path() + QLatin1Char('/')) && receiver.path.endsWith(QStringLiteral(".pdf")) &&
			receiver.contents == pdfBytes, QStringLiteral("Select 'N' Launch: hands decoded bytes to the PDF viewer"), receiver.path);
	}

	std::printf("[Pork to Sausage]\n");
	{
		w->newDocument();
		e = w->editView();
		const QByteArray input("caf\xc3\xa9 and a quote '");
		setDoc(e, input);
		e->ex(SCI_SELECTALL);
		if (auto* encode = pluginAction(w, QStringLiteral("Pork to Sausage"), QStringLiteral("Base64 encode")))
			encode->trigger();
		check(e->wholeText() == input.toBase64(), QStringLiteral("Pork to Sausage: encodes the UTF-8 selection"), docText(e));
		e->ex(SCI_SELECTALL);
		if (auto* decode = pluginAction(w, QStringLiteral("Pork to Sausage"), QStringLiteral("Base64 decode")))
			decode->trigger();
		check(e->wholeText() == input, QStringLiteral("Pork to Sausage: decodes the UTF-8 selection"), docText(e));
	}

	std::printf("[SQL Column Highlighter]\n");
	{
		w->newDocument();
		e = w->editView();
		w->languageSet(L_SQL);
		const QByteArray sql("INSERT INTO users (id, name) VALUES (1, 'Alice'), (2, 'Bob');");
		setDoc(e, sql);
		const qsizetype name = sql.indexOf("name");
		const qsizetype alice = sql.indexOf("Alice");
		const qsizetype bob = sql.indexOf("Bob");
		// the indicators of the plugin (19: the misspellings of DSpellCheck, which checks the strings of code)
		auto indicators = [e](qsizetype pos) { return e->ex(SCI_INDICATORALLONFOR, pos) & ~(sptr_t(1) << 19); };
		e->ex(SCI_GOTOPOS, name + 1);
		settle();
		check(indicators(name) && indicators(alice) && indicators(bob) == indicators(alice) && !indicators(sql.indexOf("id")),
			QStringLiteral("SQL Column Highlighter: matches a column to values in both rows"),
			QStringLiteral("%1 %2 %3").arg(indicators(name)).arg(indicators(alice)).arg(indicators(bob)));
		e->ex(SCI_SETSEL, bob, bob + 3);
		settle();
		check(indicators(name) && !indicators(alice), QStringLiteral("SQL Column Highlighter: selecting a value finds its column"));
		e->ex(SCI_GOTOPOS, 0);
		settle();
		check(!indicators(name) && !indicators(bob), QStringLiteral("SQL Column Highlighter: clears highlights when the caret moves away"));
	}

	std::printf("[NppZenHankaku]\n");
	{
		w->newDocument();
		e = w->editView();
		const QByteArray wide = QStringLiteral("ＡＢ　ガ").toUtf8();
		const QByteArray narrow = QStringLiteral("AB ｶﾞ").toUtf8();
		setDoc(e, wide + '|' + wide);
		e->ex(SCI_SETSEL, 0, wide.size());
		if (auto* toNarrow = pluginAction(w, QStringLiteral("全角半角変換"), QStringLiteral("全角 → 半角（すべて）")))
			toNarrow->trigger();
		check(e->wholeText() == narrow + '|' + wide && selection(e).toUtf8() == narrow,
			QStringLiteral("NppZenHankaku: converts only the selected text to half-width"), docText(e));
		if (auto* toWide = pluginAction(w, QStringLiteral("全角半角変換"), QStringLiteral("半角 → 全角（すべて）")))
			toWide->trigger();
		check(e->wholeText() == wide + '|' + wide, QStringLiteral("NppZenHankaku: restores full-width text and voiced katakana"), docText(e));
	}

	std::printf("[NppRestoreCursorPosition]\n");
	{
		const QDir dir(work.path());
		const QString upper = dir.filePath(QStringLiteral("Case.txt"));
		const QString lower = dir.filePath(QStringLiteral("case.txt"));
		auto visit = [w](const QString& path, int pos)
		{
			QFile file(path);
			if (!file.open(QIODevice::WriteOnly) || file.write("0123456789\n") != 11)
				return false;
			file.close();
			Buffer* buffer = w->openFile(path);
			if (!buffer)
				return false;
			w->editView()->ex(SCI_GOTOPOS, pos);
			settle();
			return w->closeBuffer(buffer, false);
		};
		const bool saved = visit(upper, 3) && visit(lower, 7);
		settle();
		const bool reopenedUpper = w->openFile(upper) != nullptr;
		settle();
		const auto upperPosition = w->editView()->ex(SCI_GETCURRENTPOS);
		const bool reopenedLower = w->openFile(lower) != nullptr;
		settle();
		const auto lowerPosition = w->editView()->ex(SCI_GETCURRENTPOS);
		const QString historyPath = config.path() + QStringLiteral("/linuxpad++/plugins/config/NppRestoreCursorPosition/history.json");
		check(saved && reopenedUpper && reopenedLower && upperPosition == 3 && lowerPosition == 7 && QFileInfo::exists(historyPath),
			QStringLiteral("NppRestoreCursorPosition: restores distinct positions for case-sensitive paths"),
			QStringLiteral("positions %1 and %2").arg(upperPosition).arg(lowerPosition));
	}

	std::printf("[LDAP Formatter]\n");
	{
		w->newDocument();
		e = w->editView();
		e->ex(SCI_SETEOLMODE, SC_EOL_LF);
		const QByteArray input("(&(objectClass=person)(|(cn=Ana)(cn=Luis)))");
		const QByteArray formatted("(&\n  (objectClass=person)\n  (|\n    (cn=Ana)\n    (cn=Luis)\n  )\n)");
		setDoc(e, input);
		QString failure;
		QTimer watch;
		QObject::connect(&watch, &QTimer::timeout, [&]
		{
			if (auto* dialog = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
			{
				failure = dialog->text();
				dialog->accept();
			}
		});
		watch.start(20);
		auto command = [&](const QString& name, const QByteArray& expected)
		{
			auto* action = pluginAction(w, QStringLiteral("LDAP Formatter"), name);
			if (!action)
				return false;
			failure.clear();
			action->trigger();
			QElapsedTimer elapsed;
			elapsed.start();
			while (e->wholeText() != expected && failure.isEmpty() && elapsed.elapsed() < 1500)
			{
				settle(1);
				QThread::msleep(2);
			}
			return e->wholeText() == expected;
		};
		const bool prettyOk = command(QStringLiteral("Format LDAP (Pretty)"), formatted);
		check(prettyOk, QStringLiteral("LDAP Formatter: formats a nested filter"), failure + docText(e));
		const bool compactOk = command(QStringLiteral("Format LDAP (Compact)"), input);
		check(compactOk, QStringLiteral("LDAP Formatter: compacts the formatted filter"), failure + docText(e));
	}

	std::printf("[NppStripIpAndHost]\n");
	{
		w->newDocument();
		e = w->editView();
		const QByteArray input("host=example.test IPv4=192.0.2.42 IPv6=fe80::1234.");
		setDoc(e, input);
		if (auto* strip = pluginAction(w, QStringLiteral("NppStripIpAndHost"), QStringLiteral("Strip IPs and Hostnames")))
			strip->trigger();
		check(e->wholeText() == "host=hostname.com IPv4=qqq.xxx.yyy.zzz IPv6=2001:db8:0:0:0:0:0:1.",
			QStringLiteral("NppStripIpAndHost: replaces the hostname, IPv4 and compressed IPv6"), docText(e));
		if (auto* undo = pluginAction(w, QStringLiteral("NppStripIpAndHost"), QStringLiteral("Undo Last Strip")))
			undo->trigger();
		check(e->wholeText() == input, QStringLiteral("NppStripIpAndHost: restores the active document"), docText(e));
	}

	std::printf("[Image Preview]\n");
	{
		const QDir dir(work.path());
		const QString docPath = dir.filePath(QStringLiteral("preview.html"));
		QImage first(36, 20, QImage::Format_RGB32);
		first.fill(Qt::red);
		QImage second(42, 24, QImage::Format_RGB32);
		second.fill(Qt::blue);
		const QByteArray html = QStringLiteral(
			"<img src=\"first.png\" alt=\"სურათი\" style=\"background: url(second.png)\">\n").toUtf8();
		QFile file(docPath);
		bool ready = first.save(dir.filePath(QStringLiteral("first.png"))) &&
			second.save(dir.filePath(QStringLiteral("second.png"))) &&
			file.open(QIODevice::WriteOnly) && file.write(html) == html.size();
		file.close();
		ready = ready && w->openFile(docPath);
		auto* show = pluginAction(w, QStringLiteral("Image Preview"), QStringLiteral("Preview Image at Cursor"));
		auto* hover = pluginAction(w, QStringLiteral("Image Preview"), QStringLiteral("Enable Image Preview"));
		auto hasImage = [w](const QSize& size, const QColor& color)
		{
			for (auto* label : w->findChildren<QLabel*>(QStringLiteral("nppImagePreviewThumbnail")))
			{
				if (label->isVisible() && label->pixmap().size() == size && label->pixmap().toImage().pixelColor(0, 0) == color)
					return true;
			}
			return false;
		};
		e = w->editView();
		settle();
		if (ready && show)
		{
			e->ex(SCI_GOTOPOS, html.indexOf("second.png") + 3);
			settle(); // the scroll to the caret first: a scroll closes the thumbnail
			show->trigger();
			settle();
		}
		auto previewState = [&]
		{
			QStringList labels;
			for (auto* label : w->findChildren<QLabel*>(QStringLiteral("nppImagePreviewThumbnail")))
				labels << QStringLiteral("%1 %2x%3").arg(label->isVisible() ? QStringLiteral("shown") : QStringLiteral("hidden"))
					.arg(label->pixmap().width()).arg(label->pixmap().height());
			return QStringLiteral("ready %1, show %2, view %3 (sub visible %4, edit visible %5), buffer %6, thumbnails: %7").arg(ready).arg(show != nullptr)
				.arg(w->activeView()).arg(w->viewVisible(MainWindow::SUB_VIEW)).arg(e->isVisible())
				.arg(w->currentBuffer() ? w->currentBuffer()->path : QString(), labels.join(QStringLiteral(", ")));
		};
		check(hasImage(second.size(), Qt::blue), QStringLiteral("Image Preview: opens the relative image under the caret"), previewState());

		if (ready && hover)
		{
			hover->trigger();
			auto* viewport = e->viewport();
			const auto pos = html.indexOf("first.png") + 2;
			// the start of the line in view (the editor may be narrow, scrolled to the caret). The
			// paint after the scroll first: a change of the scroll bars cancels the dwell of Scintilla.
			e->ex(SCI_GOTOPOS, pos);
			e->ex(SCI_SETXOFFSET, 0);
			waitMs(150);
			const QPoint point(e->ex(SCI_POINTXFROMPOSITION, 0, pos), e->ex(SCI_POINTYFROMPOSITION, 0, pos) + 2);
			QMouseEvent move(QEvent::MouseMove, point, viewport->mapToGlobal(point), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
			QApplication::sendEvent(viewport, &move);
			QElapsedTimer elapsed;
			elapsed.start();
			bool nudged = false;
			while (!hasImage(first.size(), Qt::red) && elapsed.elapsed() < 2000)
			{
				settle(1);
				QThread::msleep(2);
				if (!nudged && elapsed.elapsed() > 800)
				{
					// like a hand on the mouse: a second move starts the dwell again
					nudged = true;
					const QPoint again = point + QPoint(1, 0);
					QMouseEvent move2(QEvent::MouseMove, again, viewport->mapToGlobal(again), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
					QApplication::sendEvent(viewport, &move2);
				}
			}
		}
		check(hasImage(first.size(), Qt::red), QStringLiteral("Image Preview: hovering displays the referenced thumbnail"),
			QStringLiteral("dwell time %1, point %2,%3 in viewport %4x%5, x offset %6").arg(e->ex(SCI_GETMOUSEDWELLTIME))
				.arg(e->ex(SCI_POINTXFROMPOSITION, 0, html.indexOf("first.png") + 2)).arg(e->ex(SCI_POINTYFROMPOSITION, 0, html.indexOf("first.png") + 2))
				.arg(e->viewport()->width()).arg(e->viewport()->height()).arg(e->ex(SCI_GETXOFFSET)));
		if (ready && hover)
		{
			hover->trigger();
			settle();
		}
		check(!hasImage(first.size(), Qt::red), QStringLiteral("Image Preview: disabling hover closes the thumbnail"));
	}

	std::printf("[YamlProperties]\n");
	{
		const QString name = QStringLiteral("YamlProperties");
		auto trigger = [w, &name](const QString& label)
		{
			QAction* action = pluginAction(w, name, label);
			if (!action)
				return false;
			QTimer watch;
			bool error = false;
			QObject::connect(&watch, &QTimer::timeout, [&error]
			{
				if (auto* dialog = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
				{
					error = true;
					std::printf("  (YamlProperties: %s)\n", qPrintable(dialog->text()));
					dialog->accept();
				}
			});
			watch.start(20);
			action->trigger();
			return !error;
		};
		const QByteArray yaml("server:\n  port: 8080\nlogging:\n  level:\n    com.example: off\n");
		const QString path = QDir(work.path()).filePath(QStringLiteral("example.yaml"));
		QFile file(path);
		const bool written = file.open(QIODevice::WriteOnly) && file.write(yaml) == yaml.size();
		file.close();
		const bool opened = written && w->openFile(path);
		const bool converted = opened && trigger(QStringLiteral("Auto-detect and convert"));
		check(converted && docText(w->editView()) == QStringLiteral("server.port=8080\nlogging.level.com.example=off\n"),
			QStringLiteral("YamlProperties: auto-detects YAML and writes Properties to a new tab"), docText(w->editView()));
		check(opened && w->openFile(path) && w->editView()->wholeText() == yaml,
			QStringLiteral("YamlProperties: preserves the source while opening a result tab"));

		const bool toggled = trigger(QStringLiteral("Open conversion result in a new tab")) &&
			trigger(QStringLiteral("Sort keys during conversion"));
		w->newDocument();
		e = w->editView();
		e->ex(SCI_SETEOLMODE, SC_EOL_LF);
		const QByteArray properties("before\nz=1\na=2\nafter\n");
		setDoc(e, properties);
		e->ex(SCI_SETSEL, 7, properties.indexOf("after"));
		const bool reversed = toggled && trigger(QStringLiteral("Properties → YAML"));
		check(reversed && e->wholeText() == "before\na: 2\nz: 1\nafter\n",
			QStringLiteral("YamlProperties: sorts converted keys and replaces only the selection"), docText(e));
		e->ex(SCI_UNDO);
		check(e->wholeText() == properties, QStringLiteral("YamlProperties: undoes the conversion in one step"), docText(e));
	}

	std::printf("[EditorConfig]\n");
	{
		const QDir project(QDir(work.path()).filePath(QStringLiteral("project")));
		const QString path = project.filePath(QStringLiteral("sub/example.ecdemo"));
		auto write = [](const QString& target, const QByteArray& data)
		{
			QFile f(target);
			return f.open(QIODevice::WriteOnly) && f.write(data) == data.size();
		};
		auto read = [](const QString& target)
		{
			QFile f(target);
			return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
		};
		const QByteArray original("\tfirst  \r\nsecond\t");
		const bool ready = QDir().mkpath(project.filePath(QStringLiteral("sub"))) &&
			write(project.filePath(QStringLiteral(".editorconfig")),
				"root = true\n\n[*.ecdemo]\nindent_style = space\nindent_size = 3\ntab_width = 4\n"
				"end_of_line = lf\ntrim_trailing_whitespace = true\ninsert_final_newline = true\ncharset = utf-8-bom\n"
				"\n[*.latin]\ncharset = latin1\ninsert_final_newline = true\nend_of_line = crlf\n") &&
			write(project.filePath(QStringLiteral("sub/.editorconfig")), "[*.ecdemo]\nindent_size = 2\n") &&
			write(path, original);
		Buffer* source = ready ? w->openFile(path) : nullptr;
		e = w->editView();
		check(source && e->ex(SCI_GETINDENT) == 2 && e->ex(SCI_GETTABWIDTH) == 4 &&
			!e->ex(SCI_GETUSETABS) && e->ex(SCI_GETEOLMODE) == SC_EOL_LF,
			QStringLiteral("EditorConfig: inherits parent rules and applies the nested indentation override"));
		if (auto* action = pluginAction(w, QStringLiteral("EditorConfig"), QStringLiteral("Fix indentation")); source && action)
			action->trigger();
		check(source && e->wholeText() == "    first  \r\nsecond\t",
			QStringLiteral("EditorConfig: normalizes indentation on the first line"), docText(e));

		Buffer* other = w->newDocument();
		setDoc(w->editView(), "active tab");
		const bool saved = source && w->saveBuffer(source);
		check(saved && read(path) == QByteArray("\xef\xbb\xbf" "    first\nsecond\n") &&
			w->currentBuffer() == other && docText(w->editView()) == QStringLiteral("active tab"),
			QStringLiteral("EditorConfig: applies save rules to a background tab without selecting it"), QString::fromUtf8(read(path).toHex()));

		Buffer* created = w->newDocument();
		setDoc(w->editView(), QStringLiteral("café").toUtf8());
		const QString target = project.filePath(QStringLiteral("created.latin"));
		const bool savedAs = ready && w->saveBuffer(created, true, false, target);
		check(savedAs && read(target) == QByteArray("caf\xe9\r\n") &&
			docText(w->editView()) == QStringLiteral("café\r\n"),
			QStringLiteral("EditorConfig: Save As matches the destination and writes its charset and EOL"), QString::fromUtf8(read(target).toHex()));
	}

	std::printf("[Simplify JSON]\n");
	{
		w->newDocument();
		e = w->editView();
		setDoc(e, "{\"a\":1,\"b\":[1,2]}");
		e->ex(SCI_SELECTALL);
		if (QAction* a = pluginAction(w, QStringLiteral("Simplify JSON"), QStringLiteral("Beautify JSON")))
			a->trigger();
		settle();
		const QString pretty = docText(e);
		check(pretty.count(QLatin1Char('\n')) >= 3 && pretty.contains(QStringLiteral("\"a\"")), QStringLiteral("Simplify JSON: Beautify JSON"), pretty);
		e->ex(SCI_SELECTALL);
		if (QAction* a = pluginAction(w, QStringLiteral("Simplify JSON"), QStringLiteral("Minify JSON")))
			a->trigger();
		settle();
		check(docText(e) == QStringLiteral("{\"a\":1,\"b\":[1,2]}"), QStringLiteral("Simplify JSON: Minify JSON"), docText(e));
		setDoc(e, "say \"hi\"");
		e->ex(SCI_SELECTALL);
		if (QAction* a = pluginAction(w, QStringLiteral("Simplify JSON"), QStringLiteral("Escape JSON String")))
			a->trigger();
		settle();
		check(docText(e).contains(QStringLiteral("\\\"hi\\\"")), QStringLiteral("Simplify JSON: Escape JSON String"), docText(e));
	}

	std::printf("[Emoji Description]\n");
	{
		w->newDocument();
		e = w->editView();
		setDoc(e, "a\xf0\x9f\x98\x80" "b");
		e->ex(SCI_GOTOPOS, 1);
		QAction* details = pluginAction(w, QStringLiteral("Emoji Description"), QStringLiteral("Copy character details..."));
		QString title, text;
		const bool shown = details && modalShown(QStringLiteral("p-emoji-details"), [details] { details->trigger(); }, &title, &text);
		check(shown && text.contains(QStringLiteral("GRINNING FACE"), Qt::CaseInsensitive) && text.contains(QStringLiteral("1F600"), Qt::CaseInsensitive),
			QStringLiteral("Emoji Description: the details of the character (name table)"), title + QStringLiteral(": ") + text.left(200));
		// the information of the character at the caret in the status bar
		QAction* info = pluginAction(w, QStringLiteral("Emoji Description"), QStringLiteral("Show Character Info"));
		const bool wasOn = info && info->isChecked();
		if (info && !wasOn)
			info->trigger();
		e->ex(SCI_GOTOPOS, 0);
		settle();
		e->ex(SCI_GOTOPOS, 1);
		waitMs(300);
		const QString status = w->statusBar() ? windowText(w->statusBar()) : QString();
		check(status.contains(QStringLiteral("U+1F600"), Qt::CaseInsensitive), QStringLiteral("Emoji Description: the character at the caret in the status bar"),
			status.simplified().left(200));
		if (info && info->isChecked() != wasOn)
			info->trigger();
	}

	std::printf("[Plugin Demo]\n");
	{
		const int before = static_cast<int>(w->allBuffers().size());
		if (QAction* a = pluginAction(w, QStringLiteral("Notepad++ plugin demo"), QStringLiteral("Hello Notepad++")))
			a->trigger();
		settle();
		e = w->editView();
		check(static_cast<int>(w->allBuffers().size()) == before + 1 && docText(e) == QStringLiteral("Hello, Notepad++!"),
			QStringLiteral("Plugin Demo: Hello Notepad++ in a new document"), docText(e));
	}

	std::printf("[Task List]\n");
	{
		w->newDocument();
		e = w->editView();
		w->languageSet(L_CPP);
		setDoc(e, "int a;\n// TODO: fix this\nint b;\n");
		if (QAction* a = pluginAction(w, QStringLiteral("Task List"), QStringLiteral("Show Task List")))
			a->trigger();
		waitMs(700); // the document is searched by a timer of 200 ms
		QString listed;
		for (QDockWidget* d : w->findChildren<QDockWidget*>())
		{
			if (d->isVisible())
				listed += windowText(d) + QLatin1Char('\n');
		}
		check(listed.contains(QStringLiteral("fix this")), QStringLiteral("Task List: the TODO comment in the docked list"), listed.right(200));
		shot(w, QStringLiteral("p-tasklist"));
	}

	std::printf("[Language Selector]\n");
	{
		QAction* select = pluginAction(w, QStringLiteral("Language Selector"), QStringLiteral("Select Language"));
		QString title;
		check(select && modalShown(QStringLiteral("p-languageselector"), [select] { select->trigger(); }, &title),
			QStringLiteral("Language Selector: the dialog of the languages"), title);
		// "show on new tabs": the plugin sends WM_APP + 100 to the Notepad++ window, which sets a timer of 300 ms on
		// it, killed when it fires and opens the dialog. The dialog comes once and Cancel closes it (KillTimer of the
		// SDK did not stop the timers of the Notepad++ window: the dialog came back after every Cancel)
		int shown = 0;
		QTimer closer;
		closer.setInterval(50);
		QObject::connect(&closer, &QTimer::timeout, [&shown]
		{
			for (QWidget* t : QApplication::topLevelWidgets())
			{
				if (!t->isVisible() || !t->windowTitle().contains(QStringLiteral("Select Language")))
					continue;
				for (QPushButton* b : t->findChildren<QPushButton*>())
				{
					if (b->text().remove(QLatin1Char('&')) == QStringLiteral("Cancel"))
					{
						++shown;
						b->click();
						return;
					}
				}
			}
		});
		closer.start();
		w->pluginsManager()->message(0x8000 + 100, 0, 0); // WM_APP + 100
		waitMs(2000);
		closer.stop();
		check(shown == 1, QStringLiteral("Language Selector: the dialog of a new tab comes once, Cancel closes it"),
			QStringLiteral("shown %1 times").arg(shown));
	}

	std::printf("[OCaml Lexer]\n");
	{
		// a lexer plugin: its lexer is a language of the Language menu, of the files of its extensions
		QAction* menuItem = nullptr;
		for (QAction* a : w->findChildren<QAction*>())
		{
			const int id = a->data().toInt();
			if (id >= IDM_LANG_EXTERNAL && id <= IDM_LANG_EXTERNAL_LIMIT && a->text() == QStringLiteral("OCaml"))
				menuItem = a;
		}
		check(menuItem != nullptr, QStringLiteral("OCaml Lexer: OCaml in the Language menu"));
		const QString path = QDir(work.path()).filePath(QStringLiteral("sample.ml"));
		{
			QFile f(path);
			if (f.open(QIODevice::WriteOnly))
				f.write("let x = 1 (* note *)\nlet s = \"text\"\n");
		}
		w->openFile(path);
		settle();
		e = w->editView();
		const Buffer* b = w->currentBuffer();
		const int index = StyleManager::instance().externalLexerIndex(QStringLiteral("OCaml"));
		check(b && b->lang == L_EXTERNAL && b->udlName == QStringLiteral("OCaml") && index >= 0 && PluginsManager::langTypeOf(b) == L_EXTERNAL + index,
			QStringLiteral("OCaml Lexer: .ml files use the lexer of the plugin (L_EXTERNAL + n)"),
			b ? QStringLiteral("lang %1 '%2' index %3").arg(static_cast<int>(b->lang)).arg(b->udlName).arg(index) : QString());
		e->ex(SCI_COLOURISE, 0, -1);
		char lexer[32] = {};
		e->ex(SCI_GETLEXERLANGUAGE, 0, reinterpret_cast<sptr_t>(lexer));
		check(QByteArray(lexer) == "OCaml" && e->ex(SCI_GETSTYLEAT, 0) == 1 && e->ex(SCI_GETSTYLEAT, 12) == 2 && e->ex(SCI_GETSTYLEAT, 30) == 3,
			QStringLiteral("OCaml Lexer: keywords, comments and strings styled by the plugin"),
			QStringLiteral("%1 %2 %3 %4").arg(QString::fromLatin1(lexer)).arg(e->ex(SCI_GETSTYLEAT, 0)).arg(e->ex(SCI_GETSTYLEAT, 12)).arg(e->ex(SCI_GETSTYLEAT, 30)));
		check(e->ex(SCI_STYLEGETFORE, 1) == 0xCC0000 && e->ex(SCI_STYLEGETBOLD, 1) != 0, QStringLiteral("OCaml Lexer: the styles of OCamlLexer.xml"),
			QString::number(e->ex(SCI_STYLEGETFORE, 1), 16));
		const QString status = w->statusBar() ? windowText(w->statusBar()) : QString();
		check(status.contains(QStringLiteral("OCaml")), QStringLiteral("OCaml Lexer: the language in the status bar"), status.simplified().left(120));
		w->newDocument();
		e = w->editView();
		setDoc(e, "let y = 2");
		if (menuItem)
			menuItem->trigger();
		settle();
		e->ex(SCI_COLOURISE, 0, -1);
		check(w->currentBuffer()->lang == L_EXTERNAL && menuItem && menuItem->isChecked() && e->ex(SCI_GETSTYLEAT, 0) == 1,
			QStringLiteral("OCaml Lexer: chosen in the Language menu"));
	}

	std::printf("[HLASM Lexer]\n");
	{
		const QString path = QDir(work.path()).filePath(QStringLiteral("sample.mlc"));
		{
			QFile f(path);
			if (f.open(QIODevice::WriteOnly))
				f.write("* a comment line\nLABEL    MVC   FIELD,=C'X'\n");
		}
		w->openFile(path);
		settle();
		e = w->editView();
		e->ex(SCI_COLOURISE, 0, -1);
		char lexer[32] = {};
		e->ex(SCI_GETLEXERLANGUAGE, 0, reinterpret_cast<sptr_t>(lexer));
		const sptr_t label = e->ex(SCI_POSITIONFROMLINE, 1);
		check(w->currentBuffer()->lang == L_EXTERNAL && QByteArray(lexer) == "HLASM" && e->ex(SCI_GETSTYLEAT, 0) == 4 && e->ex(SCI_GETSTYLEAT, label) == 1,
			QStringLiteral("HLASM Lexer: .mlc files styled by the lexer of the plugin (comment, label)"),
			QStringLiteral("%1 %2 %3").arg(QString::fromLatin1(lexer)).arg(e->ex(SCI_GETSTYLEAT, 0)).arg(e->ex(SCI_GETSTYLEAT, label)));
	}

	std::printf("[RAScript]\n");
	{
		const QString path = QDir(work.path()).filePath(QStringLiteral("achievements.rascript"));
		{
			QFile f(path);
			if (f.open(QIODevice::WriteOnly))
				f.write("// a note\nfunction main() { return 1 }\n");
		}
		w->openFile(path);
		settle();
		e = w->editView();
		e->ex(SCI_COLOURISE, 0, -1);
		char lexer[32] = {};
		e->ex(SCI_GETLEXERLANGUAGE, 0, reinterpret_cast<sptr_t>(lexer));
		const sptr_t function = e->ex(SCI_POSITIONFROMLINE, 1);
		check(w->currentBuffer()->lang == L_EXTERNAL && QByteArray(lexer) == "RAScript" && e->ex(SCI_GETSTYLEAT, 0) == 25 && e->ex(SCI_GETSTYLEAT, function) != 0,
			QStringLiteral("RAScript: .rascript files styled by the grammar of RAScript.xml (comment, keyword)"),
			QStringLiteral("%1 %2 %3").arg(QString::fromLatin1(lexer)).arg(e->ex(SCI_GETSTYLEAT, 0)).arg(e->ex(SCI_GETSTYLEAT, function)));
	}

	std::printf("[Fore]\n");
	{
		const QString path = QDir(work.path()).filePath(QStringLiteral("module.fore"));
		{
			QFile f(path);
			if (f.open(QIODevice::WriteOnly))
				f.write("// a note\nSub Main;\nBegin\nEnd Sub Main;\n");
		}
		w->openFile(path);
		settle();
		e = w->editView();
		e->ex(SCI_COLOURISE, 0, -1);
		char lexer[32] = {};
		e->ex(SCI_GETLEXERLANGUAGE, 0, reinterpret_cast<sptr_t>(lexer));
		const sptr_t sub = e->ex(SCI_POSITIONFROMLINE, 1);
		check(w->currentBuffer()->lang == L_EXTERNAL && QByteArray(lexer) == "Fore" && e->ex(SCI_GETSTYLEAT, 0) == 7 && e->ex(SCI_GETSTYLEAT, sub) == 1,
			QStringLiteral("Fore: .fore files styled by the lexer of the plugin (comment, keyword)"),
			QStringLiteral("%1 %2 %3").arg(QString::fromLatin1(lexer)).arg(e->ex(SCI_GETSTYLEAT, 0)).arg(e->ex(SCI_GETSTYLEAT, sub)));
	}

	std::printf("[HiddenLexers]\n");
	{
		// the default HiddenLexers.json: .adoc files with the asciidoc lexer of Lexilla (NPPM_CREATELEXER)
		const QString path = QDir(work.path()).filePath(QStringLiteral("readme.adoc"));
		{
			QFile f(path);
			if (f.open(QIODevice::WriteOnly))
				f.write("= Title\n\nSome *strong* text.\n");
		}
		w->openFile(path);
		settle(20);
		e = w->editView();
		e->ex(SCI_COLOURISE, 0, -1);
		char lexer[32] = {};
		e->ex(SCI_GETLEXERLANGUAGE, 0, reinterpret_cast<sptr_t>(lexer));
		check(QByteArray(lexer) == "asciidoc" && e->ex(SCI_GETSTYLEAT, 0) != 0, QStringLiteral("HiddenLexers: .adoc files with the hidden asciidoc lexer"),
			QStringLiteral("%1 %2").arg(QString::fromLatin1(lexer)).arg(e->ex(SCI_GETSTYLEAT, 0)));
	}

	std::printf("[NppExec]\n");
	{
		if (QAction* a = pluginAction(w, QStringLiteral("NppExec"), QStringLiteral("Show NppExec Console")))
			a->trigger();
		waitMs(300);
		QPlainTextEdit* console = nullptr;
		for (QDockWidget* d : w->findChildren<QDockWidget*>())
		{
			if (!d->isVisible())
				continue;
			for (QPlainTextEdit* pe : d->findChildren<QPlainTextEdit*>())
			{
				if (pe->property("nppRichEdit").toBool())
					console = pe;
			}
		}
		check(console != nullptr, QStringLiteral("NppExec: the console (a rich edit) in a docked panel"));
		if (console)
		{
			// a command typed at the end of the console, then Enter: NppExec runs it with a pipe
			console->setFocus();
			console->moveCursor(QTextCursor::End);
			settle();
			for (const QChar ch : QStringLiteral("sh -c \"echo external-ok && expr 40 + 2\""))
			{
				int key = Qt::Key_unknown;
				if (ch.isLetter())
					key = Qt::Key_A + (ch.toUpper().unicode() - 'A');
				else if (ch.isDigit())
					key = Qt::Key_0 + (ch.unicode() - '0');
				else if (ch == QLatin1Char(' '))
					key = Qt::Key_Space;
				typeKey(console, key, QString(ch));
			}
			typeKey(console, Qt::Key_Return, QStringLiteral("\r"));
			QElapsedTimer elapsed;
			elapsed.start();
			while (!console->toPlainText().contains(QStringLiteral("Exit code 0")) && elapsed.elapsed() < 5000)
				waitMs(100);
			const QString text = console->toPlainText();
			check(text.contains(QStringLiteral("\nexternal-ok")) && text.contains(QStringLiteral("\n42")) && text.contains(QStringLiteral("Exit code 0")), QStringLiteral("NppExec: a command typed in the console runs and prints its output"),
				text.right(300).simplified());
			shot(w, QStringLiteral("p-nppexec-console"));
		}
	}

	std::printf("[JSTool]\n");
	{
		w->newDocument();
		e = w->editView();
		w->languageSet(L_JAVASCRIPT);
		setDoc(e, "function f(){var a=1;if(a){return a}}");
		if (QAction* a = pluginAction(w, QStringLiteral("JSTool"), QStringLiteral("JSFormat")))
			a->trigger();
		settle();
		const QString formatted = docText(e);
		check(formatted.count(QLatin1Char('\n')) >= 3 && formatted.contains(QStringLiteral("return a")), QStringLiteral("JSTool: JSFormat"), formatted.left(120));
		setDoc(e, "var a = 1; // note\nvar b = 2;\n");
		if (QAction* a = pluginAction(w, QStringLiteral("JSTool"), QStringLiteral("JSMin")))
			a->trigger();
		settle();
		const QString minified = docText(e).trimmed();
		check(!minified.contains(QStringLiteral("note")) && minified.contains(QStringLiteral("var a=1")), QStringLiteral("JSTool: JSMin"), minified.left(120));
		w->languageSet(L_JSON);
		setDoc(e, "{\"b\": 1, \"a\": {\"d\": true, \"c\": [1, 2]}}");
		if (QAction* a = pluginAction(w, QStringLiteral("JSTool"), QStringLiteral("JSON Sort")))
			a->trigger();
		settle();
		const QString sorted = docText(e);
		check(sorted.indexOf(QStringLiteral("\"a\"")) >= 0 && sorted.indexOf(QStringLiteral("\"a\"")) < sorted.indexOf(QStringLiteral("\"b\""))
			&& sorted.indexOf(QStringLiteral("\"c\"")) < sorted.indexOf(QStringLiteral("\"d\"")), QStringLiteral("JSTool: JSON Sort"), sorted.simplified().left(120));
		if (QAction* a = pluginAction(w, QStringLiteral("JSTool"), QStringLiteral("JSON Viewer")))
			a->trigger();
		waitMs(300);
		QStringList items;
		for (QDockWidget* d : w->findChildren<QDockWidget*>())
		{
			if (!d->isVisible())
				continue;
			for (QTreeWidget* t : d->findChildren<QTreeWidget*>())
			{
				for (QTreeWidgetItemIterator it(t); *it; ++it)
					items << (*it)->text(0);
			}
		}
		check(items.join(QLatin1Char(' ')).contains(QStringLiteral("a")) && items.size() >= 4, QStringLiteral("JSTool: JSON Viewer tree"), items.join(QStringLiteral(" | ")).left(150));
		shot(w, QStringLiteral("p-jstool-json-viewer"));
	}

	std::printf("[Explorer]\n");
	{
		if (QAction* a = pluginAction(w, QStringLiteral("Explorer"), QStringLiteral("Explorer...")))
			a->trigger();
		// the files of the folder: listed by the worker thread of Explorer, in a virtual list (LVS_OWNERDATA: the texts
		// come from LVN_GETDISPINFO when the rows are painted)
		QTreeWidget* files = nullptr;
		QTreeWidget* folders = nullptr;
		for (int i = 0; i < 40 && !(files && files->topLevelItemCount() == 3); ++i)
		{
			waitMs(100);
			for (QDockWidget* d : w->findChildren<QDockWidget*>())
			{
				if (!d->isVisible())
					continue;
				for (QTreeWidget* t : d->findChildren<QTreeWidget*>())
				{
					// the list has a header (report view), the tree has none
					if (!t->isHeaderHidden())
						files = t;
					else
						folders = t;
				}
			}
		}
		// "..", "[subfolder]", "alpha" (the parent folder, the folders, then the files)
		check(files && files->topLevelItemCount() == 3, QStringLiteral("Explorer: the panel lists the files and folders of the current folder"),
			files ? QString::number(files->topLevelItemCount()) : QStringLiteral("no list"));
		QStringList tree;
		if (folders)
		{
			for (QTreeWidgetItemIterator it(folders); *it; ++it)
				tree << (*it)->text(0);
		}
		check(folders && !tree.isEmpty() && tree.join(QLatin1Char(' ')).contains(QStringLiteral("explore")),
			QStringLiteral("Explorer: the folder tree is open at the current folder"), tree.join(QStringLiteral(", ")).left(200));
		shot(w, QStringLiteral("p-explorer"));
		// a double click on the file (the last row) opens it
		QTreeWidgetItem* alpha = files && files->topLevelItemCount() ? files->topLevelItem(files->topLevelItemCount() - 1) : nullptr;
		if (alpha)
		{
			const QPoint at = files->visualItemRect(alpha).center();
			for (QEvent::Type type : {QEvent::MouseButtonPress, QEvent::MouseButtonRelease, QEvent::MouseButtonDblClick, QEvent::MouseButtonRelease})
			{
				QMouseEvent me(type, at, files->viewport()->mapToGlobal(at), Qt::LeftButton,
					type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton, Qt::NoModifier);
				QApplication::sendEvent(files->viewport(), &me);
			}
		}
		for (int i = 0; i < 20 && !(w->currentBuffer() && w->currentBuffer()->path.endsWith(QStringLiteral("alpha.txt"))); ++i)
			waitMs(100);
		const QString opened = w->currentBuffer() ? w->currentBuffer()->path : QString();
		check(opened == explorerDir + QStringLiteral("/alpha.txt") && docText(w->editView()).startsWith(QStringLiteral("explorer-open-ok")),
			QStringLiteral("Explorer: a double click on a file opens it"), opened);

		bool submitted = false;
		QTimer enterPath;
		QObject::connect(&enterPath, &QTimer::timeout, [&]
		{
			QWidget* dialog = QApplication::activeModalWidget();
			if (!dialog)
			{
				for (QWidget* widget : QApplication::topLevelWidgets())
				{
					if (widget->isVisible() && widget->windowTitle() == QStringLiteral("Explorer") && widget->isModal())
					{
						dialog = widget;
						break;
					}
				}
			}
			if (!dialog)
				return;
			if (auto* error = qobject_cast<QMessageBox*>(dialog))
			{
				error->reject();
				return;
			}
			auto* path = dialog->findChild<QLineEdit*>();
			if (!path)
				return;
			for (QPushButton* button : dialog->findChildren<QPushButton*>())
			{
				if (button->text().remove(QLatin1Char('&')).compare(QStringLiteral("OK"), Qt::CaseInsensitive) == 0)
				{
					path->setText(explorerDir + QStringLiteral("/subfolder"));
					submitted = true;
					button->click();
					break;
				}
			}
		});
		if (QAction* go = pluginAction(w, QStringLiteral("Explorer"), QStringLiteral("Go to Path...")))
		{
			enterPath.start(20);
			go->trigger();
			enterPath.stop();
		}
		const auto* folder = folders ? folders->currentItem() : nullptr;
		check(submitted && folder && folder->text(0) == QStringLiteral("subfolder"),
			QStringLiteral("Explorer: Go to Path accepts an absolute Linux directory"),
			folder ? folder->text(0) : QString());

		if (files && submitted)
		{
			auto* mime = new QMimeData;
			mime->setUrls({QUrl::fromLocalFile(explorerDir + QStringLiteral("/alpha.txt"))});
			QApplication::clipboard()->setMimeData(mime);
			QTimer confirm;
			QObject::connect(&confirm, &QTimer::timeout, []
			{
				if (auto* dialog = qobject_cast<QMessageBox*>(QApplication::activeModalWidget()))
				{
					if (auto* yes = dialog->button(QMessageBox::Yes))
						yes->click();
				}
			});
			confirm.start(20);
			typeKey(files, Qt::Key_V, QString(QChar(0x16)), Qt::ControlModifier);
		}
		QFile copied(explorerDir + QStringLiteral("/subfolder/alpha.txt"));
		check(copied.open(QIODevice::ReadOnly) && copied.readAll() == "explorer-open-ok\n",
			QStringLiteral("Explorer: pastes local file URLs from the Linux clipboard"));
	}

	std::printf("[HexEditor]\n");
	{
		w->newDocument();
		e = w->editView();
		setDoc(e, "ABC\nHex editor test line\n");
		settle();
		QAction* view = pluginAction(w, QStringLiteral("HEX-Editor"), QStringLiteral("View in HEX"));
		if (view)
			view->trigger();
		waitMs(300);
		// the hex view: a list (report view) shown in the place of the editor
		QTreeWidget* hex = nullptr;
		for (QTreeWidget* t : w->findChildren<QTreeWidget*>())
		{
			if (t->isVisible() && !t->isHeaderHidden() && !qobject_cast<QDockWidget*>(t->window()) && t->columnCount() >= 3)
			{
				QWidget* dock = t;
				while (dock && !qobject_cast<QDockWidget*>(dock))
					dock = dock->parentWidget();
				if (!dock)
					hex = t;
			}
		}
		QStringList headers;
		for (int c = 0; hex && c < hex->columnCount(); ++c)
			headers << hex->headerItem()->text(c);
		check(hex && view && view->isChecked() && !e->isVisible(), QStringLiteral("HexEditor: View in HEX shows the bytes in a list in the place of the editor"),
			QStringLiteral("%1 rows, columns: %2, editor visible %3").arg(hex ? hex->topLevelItemCount() : -1).arg(headers.join(QStringLiteral(" | "))).arg(e->isVisible()));
		// 25 bytes, 16 per row (the default): 2 rows
		check(hex && hex->topLevelItemCount() == 2, QStringLiteral("HexEditor: one row for 16 bytes"), QString::number(hex ? hex->topLevelItemCount() : -1));
		shot(w, QStringLiteral("p-hexeditor"));
		// typing a hex digit on the first byte (the cursor is there): its high half, 0x41 'A' -> 0x51 'Q'
		if (hex)
		{
			hex->setFocus();
			QKeyEvent press(QEvent::KeyPress, Qt::Key_5, Qt::NoModifier, QStringLiteral("5"));
			QApplication::sendEvent(hex, &press);
			QKeyEvent release(QEvent::KeyRelease, Qt::Key_5, Qt::NoModifier, QStringLiteral("5"));
			QApplication::sendEvent(hex, &release);
			settle();
		}
		check(docText(e).startsWith(QStringLiteral("QBC\n")), QStringLiteral("HexEditor: a hex digit typed in the view changes the byte in the document"),
			docText(e).left(20).toUtf8().toHex(' '));
		e->ex(SCI_UNDO);
		settle();
		if (view)
			view->trigger();
		waitMs(200);
		check(e->isVisible() && docText(e) == QStringLiteral("ABC\nHex editor test line\n"), QStringLiteral("HexEditor: View in HEX again shows the text again"),
			docText(e).left(40));

		e->ex(SCI_GOTOPOS, 16);
		if (view && hex)
		{
			view->trigger();
			typeKey(hex, Qt::Key_7, QStringLiteral("7"));
			view->trigger();
		}
		check(e->wholeText() == "ABC\nHex editor tust line\n",
			QStringLiteral("HexEditor: the caret at a row boundary edits the next row's first byte"), e->wholeText().toHex(' '));

		const QString path = work.filePath(QStringLiteral("utf16-hex.txt"));
		QFile encoded(path);
		const QByteArray utf16 = QByteArray::fromHex("fffe4100e9000a00");
		const bool written = encoded.open(QIODevice::WriteOnly) && encoded.write(utf16) == utf16.size();
		encoded.close();
		Buffer* buffer = written ? w->openFile(path) : nullptr;
		if (buffer && view && hex)
		{
			view->trigger();
			typeKey(hex, Qt::Key_5, QStringLiteral("5"));
			view->trigger();
		}
		const bool saved = buffer && buffer->unicode == UniMode::uni16LE && w->saveBuffer(buffer);
		check(saved && encoded.open(QIODevice::ReadOnly) && encoded.readAll() == QByteArray::fromHex("fffe5100e9000a00"),
			QStringLiteral("HexEditor: a byte edit preserves the document's UTF-16 save encoding"), docText(w->editView()));
	}

	std::printf("[DSpellCheck]\n");
	{
		w->newDocument();
		e = w->editView();
		setDoc(e, "This is a tset of the speling checker.\n");
		e->ex(SCI_GOTOPOS, 0);
		QAction* autoCheck = pluginAction(w, QStringLiteral("DSpellCheck"), QStringLiteral("Spell Check Document Automatically"));
		if (autoCheck && !autoCheck->isChecked())
			autoCheck->trigger();
		// the indicators of the misspelled words: set after a delay (the dictionary loads in a thread)
		auto marked = [e](sptr_t pos) { return e->ex(SCI_INDICATORALLONFOR, pos) != 0; };
		const sptr_t tset = 10, speling = 22, checker = 30;
		for (int i = 0; i < 50 && !(marked(tset) && marked(speling)); ++i)
			waitMs(100);
		check(autoCheck && autoCheck->isChecked() && marked(tset) && marked(speling) && !marked(0) && !marked(checker),
			QStringLiteral("DSpellCheck: the misspelled words are marked with the system Hunspell dictionary"),
			QStringLiteral("tset %1 speling %2 This %3 checker %4").arg(marked(tset)).arg(marked(speling)).arg(marked(0)).arg(marked(checker)));
		shot(w, QStringLiteral("p-dspellcheck"));
		e->ex(SCI_GOTOPOS, 0);
		if (QAction* next = pluginAction(w, QStringLiteral("DSpellCheck"), QStringLiteral("Find Next Misspelling")))
			next->trigger();
		settle();
		check(selection(e) == QStringLiteral("tset"), QStringLiteral("DSpellCheck: Find Next Misspelling selects the first misspelled word"), selection(e));
		QStringList languages;
		if (QAction* lang = pluginAction(w, QStringLiteral("DSpellCheck"), QStringLiteral("Change Current Language")))
		{
			if (QMenu* m = lang->menu())
			{
				emit m->aboutToShow();
				languages = menuItems(m);
			}
		}
		check(languages.join(QLatin1Char('|')).contains(QStringLiteral("English")), QStringLiteral("DSpellCheck: the languages of /usr/share/hunspell in Change Current Language"),
			languages.join(QStringLiteral(", ")).left(200));
		// a right click on a misspelled word: its suggestions at the top of the context menu, one replaces the word
		QStringList items;
		QTimer::singleShot(400, [&items]
		{
			auto* popup = qobject_cast<QMenu*>(QApplication::activePopupWidget());
			if (!popup)
				return;
			items = menuItems(popup);
			for (QAction* a : popup->actions())
			{
				if (a->text().remove(QLatin1Char('&')) == QStringLiteral("test"))
				{
					a->trigger();
					break;
				}
			}
			popup->close();
		});
		{
			const QPoint at(static_cast<int>(e->ex(SCI_POINTXFROMPOSITION, 0, tset + 1)), static_cast<int>(e->ex(SCI_POINTYFROMPOSITION, 0, tset)) + 4);
			QContextMenuEvent ce(QContextMenuEvent::Mouse, at, e->viewport()->mapToGlobal(at));
			QApplication::sendEvent(e->viewport(), &ce);
		}
		settle();
		check(items.mid(0, 6).contains(QStringLiteral("test")) && items.join(QLatin1Char('\n')).contains(QStringLiteral("\nCut")),
			QStringLiteral("DSpellCheck: the suggestions for the word under the mouse in the context menu"), items.join(QStringLiteral(", ")).left(300));
		check(docText(e).startsWith(QStringLiteral("This is a test of")), QStringLiteral("DSpellCheck: a suggestion of the context menu replaces the word"),
			docText(e).left(30));
		if (QAction* settingsAction = pluginAction(w, QStringLiteral("DSpellCheck"), QStringLiteral("Settings...")))
			settingsAction->trigger();
		check(closeWindowTitled(QStringLiteral("DSpellCheck Settings"), QStringLiteral("p-dspellcheck-settings")), QStringLiteral("DSpellCheck: the Settings dialog"));
	}

	std::printf("[XML Tools]\n");
	{
		const QString xmlDir = work.path() + QStringLiteral("/xmltools");
		QDir().mkpath(xmlDir);
		auto writeFile = [](const QString& path, const QByteArray& data)
		{
			QFile f(path);
			return f.open(QIODevice::WriteOnly) && f.write(data) == data.size();
		};
		auto run = [w](const QString& item)
		{
			QAction* a = pluginAction(w, QStringLiteral("XML Tools"), item);
			if (a)
				a->trigger();
			settle();
			return a != nullptr;
		};
		// the annotations of the errors under the lines
		auto annotations = [](EditView* v)
		{
			QString all;
			for (sptr_t line = 0; line < v->ex(SCI_GETLINECOUNT); ++line)
			{
				const sptr_t len = v->ex(SCI_ANNOTATIONGETTEXT, line, 0);
				if (len <= 0)
					continue;
				QByteArray text(static_cast<int>(len) + 1, '\0');
				v->ex(SCI_ANNOTATIONGETTEXT, line, reinterpret_cast<sptr_t>(text.data()));
				all += QStringLiteral("%1: %2\n").arg(line).arg(QString::fromUtf8(text.constData()));
			}
			return all;
		};
		auto topWindow = [](const QString& title) -> QWidget*
		{
			settle(10);
			for (QWidget* t : QApplication::topLevelWidgets())
			{
				if (t->isVisible() && t->windowTitle() == title)
					return t;
			}
			return nullptr;
		};

		w->newDocument();
		e = w->editView();
		setDoc(e, "<a><b x=\"1\">t</b><c/></a>");
		run(QStringLiteral("Pretty print"));
		const QString pretty = docText(e);
		check(pretty.contains(QRegularExpression(QStringLiteral("<a>\\r?\\n[ \\t]+<b x=\"1\">t</b>\\r?\\n[ \\t]+<c/>\\r?\\n</a>"))),
			QStringLiteral("XML Tools: Pretty print indents the elements"), pretty);
		run(QStringLiteral("Linearize"));
		check(docText(e).trimmed() == QStringLiteral("<a><b x=\"1\">t</b><c/></a>"), QStringLiteral("XML Tools: Linearize"), docText(e));

		// syntax check: the error of libxml2 as an annotation, at its line
		setDoc(e, "<?xml version=\"1.0\"?>\n<a>\n<b>\n</a>\n");
		run(QStringLiteral("Check XML syntax now"));
		const QString syntaxErrors = annotations(e);
		check(syntaxErrors.contains(QStringLiteral("mismatch")) && syntaxErrors.startsWith(QStringLiteral("3: ")),
			QStringLiteral("XML Tools: Check XML syntax annotates the error at its line"), syntaxErrors);
		shot(w, QStringLiteral("p-xmltools-syntax"));
		setDoc(e, "<?xml version=\"1.0\"?>\n<a><b/></a>\n");
		check(annotations(e).isEmpty(), QStringLiteral("XML Tools: an edit clears the annotations"), annotations(e));
		QString message;
		modalShown(QStringLiteral("p-xmltools-noerror"), [&] { run(QStringLiteral("Check XML syntax now")); }, nullptr, &message);
		check(message.contains(QStringLiteral("No error detected")), QStringLiteral("XML Tools: Check XML syntax of a well-formed document"), message);

		// validation by the schema of xsi:noNamespaceSchemaLocation, relative to the folder of the document
		const bool written = writeFile(xmlDir + QStringLiteral("/order.xsd"),
				"<?xml version=\"1.0\"?>\n"
				"<xs:schema xmlns:xs=\"http://www.w3.org/2001/XMLSchema\">\n"
				"  <xs:element name=\"order\"><xs:complexType><xs:sequence>\n"
				"    <xs:element name=\"qty\" type=\"xs:integer\"/>\n"
				"  </xs:sequence></xs:complexType></xs:element>\n"
				"</xs:schema>\n")
			&& writeFile(xmlDir + QStringLiteral("/order.xml"),
				"<?xml version=\"1.0\"?>\n"
				"<order xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xsi:noNamespaceSchemaLocation=\"order.xsd\">\n"
				"<qty>many</qty>\n"
				"</order>\n");
		Buffer* order = written ? w->openFile(xmlDir + QStringLiteral("/order.xml")) : nullptr;
		e = w->editView();
		run(QStringLiteral("Validate now"));
		const QString validationErrors = annotations(e);
		check(order && validationErrors.contains(QStringLiteral("integer")) && validationErrors.startsWith(QStringLiteral("2: ")),
			QStringLiteral("XML Tools: Validate now annotates the XSD error at its line"), validationErrors);
		shot(w, QStringLiteral("p-xmltools-validate"));
		message.clear();
		if (order)
		{
			const sptr_t at = static_cast<sptr_t>(e->wholeText().indexOf("many"));
			e->ex(SCI_SETSEL, at, at + 4);
			e->ex(SCI_REPLACESEL, 0, reinterpret_cast<sptr_t>("42"));
			modalShown(QStringLiteral("p-xmltools-valid"), [&] { run(QStringLiteral("Validate now")); }, nullptr, &message);
		}
		check(message.contains(QStringLiteral("No error detected")), QStringLiteral("XML Tools: Validate now of a valid document"), message);

		// XPath
		w->newDocument();
		e = w->editView();
		setDoc(e, "<?xml version=\"1.0\"?>\n<r><i n=\"1\">one</i><i n=\"2\">two</i></r>\n");
		// the dialog is modal: the expressions are evaluated from a timer of its event loop, then it is closed
		auto evaluate = [](QWidget* xpath, const QString& expression)
		{
			QStringList rows;
			auto* edit = xpath->findChild<QLineEdit*>(QStringLiteral("expression"));
			auto* button = xpath->findChild<QPushButton*>(QStringLiteral("evaluate"));
			auto* results = xpath->findChild<QTreeWidget*>(QStringLiteral("results"));
			if (!edit || !button || !results)
				return rows;
			edit->setText(expression);
			button->click();
			settle();
			for (int i = 0; i < results->topLevelItemCount(); ++i)
			{
				const QTreeWidgetItem* item = results->topLevelItem(i);
				rows << item->text(0) + QLatin1Char('|') + item->text(1) + QLatin1Char('|') + item->text(2);
			}
			return rows;
		};
		QStringList nodes, number;
		QString xpathTitle;
		QTimer::singleShot(300, [&]
		{
			QWidget* xpath = QApplication::activeModalWidget();
			if (!xpath)
				return;
			xpathTitle = xpath->windowTitle();
			nodes = evaluate(xpath, QStringLiteral("//i[@n='2']"));
			number = evaluate(xpath, QStringLiteral("count(//i)"));
			shot(xpath, QStringLiteral("p-xmltools-xpath"));
			if (auto* dialog = qobject_cast<QDialog*>(xpath))
				dialog->reject();
			else
				xpath->close();
		});
		run(QStringLiteral("Evaluate XPath expression..."));
		check(xpathTitle == QStringLiteral("XPath expression evaluation") && nodes == QStringList{QStringLiteral("element|i|two")},
			QStringLiteral("XML Tools: XPath evaluation lists the nodes"), xpathTitle + QStringLiteral(": ") + nodes.join(QStringLiteral(", ")));
		check(number == QStringList{QStringLiteral("number||2")}, QStringLiteral("XML Tools: XPath evaluation of a number"), number.join(QStringLiteral(", ")));

		// the path of the node at the caret, in a message and in the clipboard
		e->ex(SCI_GOTOPOS, e->wholeText().indexOf("two"));
		message.clear();
		modalShown(QStringLiteral("p-xmltools-path"), [&] { run(QStringLiteral("Current XML Path")); }, nullptr, &message);
		check(message.contains(QStringLiteral("/r/i")) && QApplication::clipboard()->text() == QStringLiteral("/r/i"),
			QStringLiteral("XML Tools: Current XML Path"), message + QStringLiteral(" / ") + QApplication::clipboard()->text());

		// XSL transformation (libxslt): the result in a new document, a parameter of the dialog
		const QString xsl = xmlDir + QStringLiteral("/list.xsl");
		writeFile(xsl,
			"<?xml version=\"1.0\"?>\n"
			"<xsl:stylesheet version=\"1.0\" xmlns:xsl=\"http://www.w3.org/1999/XSL/Transform\">\n"
			"<xsl:output method=\"text\"/>\n"
			"<xsl:param name=\"sep\" select=\"','\"/>\n"
			"<xsl:template match=\"/r\"><xsl:for-each select=\"i\"><xsl:value-of select=\".\"/><xsl:value-of select=\"$sep\"/></xsl:for-each></xsl:template>\n"
			"</xsl:stylesheet>\n");
		Buffer* source = w->currentBuffer();
		run(QStringLiteral("XSL Transformation..."));
		QWidget* xslt = topWindow(QStringLiteral("XSL Transformation"));
		if (xslt)
		{
			if (auto* f = xslt->findChild<QLineEdit*>(QStringLiteral("xslfile")))
				f->setText(xsl);
			if (auto* o = xslt->findChild<QLineEdit*>(QStringLiteral("xsltoptions")))
				o->setText(QStringLiteral("sep=';'"));
			if (auto* b = xslt->findChild<QPushButton*>(QStringLiteral("transform")))
				b->click();
			settle();
			shot(xslt, QStringLiteral("p-xmltools-xslt"));
			xslt->hide();
		}
		check(xslt && w->currentBuffer() != source && docText(w->editView()) == QStringLiteral("one;two;"),
			QStringLiteral("XML Tools: XSL Transformation with a parameter in a new document"), docText(w->editView()));

		// escaping, comments, the closing tag
		w->newDocument();
		e = w->editView();
		setDoc(e, "a <b> & c");
		e->ex(SCI_SELECTALL);
		run(QStringLiteral("Escape characters in selection (<> → lt;gt;)"));
		check(docText(e) == QStringLiteral("a &lt;b&gt; &amp; c"), QStringLiteral("XML Tools: Escape characters in selection"), docText(e));
		e->ex(SCI_SELECTALL);
		run(QStringLiteral("Unescape characters in selection (lt;gt; → <>)"));
		check(docText(e) == QStringLiteral("a <b> & c"), QStringLiteral("XML Tools: Unescape characters in selection"), docText(e));
		setDoc(e, "<a><b/></a>");
		e->ex(SCI_SETSEL, 3, 7);
		run(QStringLiteral("Comment selection"));
		const QString commented = docText(e);
		check(commented == QStringLiteral("<a><!--<b/>--></a>"), QStringLiteral("XML Tools: Comment selection"), commented);
		e->ex(SCI_SETSEL, 3, 14);
		run(QStringLiteral("Uncomment selection"));
		check(docText(e) == QStringLiteral("<a><b/></a>"), QStringLiteral("XML Tools: Uncomment selection"), docText(e));
		setDoc(e, "<?xml version=\"1.0\"?>\n<root");
		e->ex(SCI_DOCUMENTEND);
		typeKey(e, Qt::Key_Greater, QStringLiteral(">"));
		check(docText(e).endsWith(QStringLiteral("<root></root>")) && e->ex(SCI_GETCURRENTPOS) == e->ex(SCI_GETLENGTH) - 7,
			QStringLiteral("XML Tools: Tag auto-close"), docText(e));

		QString title;
		modalShown(QStringLiteral("p-xmltools-options"), [&] { run(QStringLiteral("Options...")); }, &title);
		check(title == QStringLiteral("XML Tools options"), QStringLiteral("XML Tools: the Options dialog"), title);
		title.clear();
		modalShown(QStringLiteral("p-xmltools-about"), [&] { run(QStringLiteral("About XML Tools / Donate...")); }, &title, &message);
		check(title == QStringLiteral("About XML Tools") && message.contains(QStringLiteral("libxml2")), QStringLiteral("XML Tools: the About dialog"), title);
	}

	std::printf("[ComparePlus]\n");
	{
		// one view with only the documents of this test
		for (Buffer* x : w->allBuffers())
			w->closeBuffer(x, false);
		settle();
		auto run = [w](const QString& item)
		{
			QAction* a = pluginAction(w, QStringLiteral("ComparePlus"), item);
			if (a)
				a->trigger();
			settle(10);
			return a != nullptr;
		};
		auto markers = [](EditView* v, int line) { return v ? static_cast<int>(v->ex(SCI_MARKERGET, line)) : -1; };
		const QByteArray oldText = "line one\nline two\nline three\nline four\n";
		const QByteArray newText = "line one\nline 2\nline three\nadded line\nline four\n";
		w->newDocument();
		setDoc(w->editView(), oldText);
		w->newDocument();
		setDoc(w->editView(), newText);
		run(QStringLiteral("Compare"));
		waitMs(300);
		EditView* v0 = w->viewEdit(0);
		EditView* v1 = w->viewEdit(1);
		EditView* oldView = (v0 && v0->wholeText() == oldText) ? v0 : ((v1 && v1->wholeText() == oldText) ? v1 : nullptr);
		EditView* newView = (v0 && v0->wholeText() == newText) ? v0 : ((v1 && v1->wholeText() == newText) ? v1 : nullptr);
		check(w->viewVisible(0) && w->viewVisible(1) && oldView && newView && oldView != newView,
			QStringLiteral("ComparePlus: Compare shows the two documents side by side"),
			QStringLiteral("visible %1 %2, old %3 new %4").arg(w->viewVisible(0)).arg(w->viewVisible(1)).arg(oldView != nullptr).arg(newView != nullptr));
		shot(w, QStringLiteral("p-compareplus"));
		const int changedMask = 1 << 0, addedMask = 1 << 1, removedMask = 1 << 2;
		QString markerDetail;
		for (int line = 0; line < 5; ++line)
			markerDetail += QStringLiteral("%1:%2/%3 ").arg(line).arg(markers(oldView, line), 0, 16).arg(markers(newView, line), 0, 16);
		check((markers(oldView, 1) & changedMask) && (markers(newView, 1) & changedMask) && !(markers(oldView, 0) & 0xF) && !(markers(newView, 0) & 0xF),
			QStringLiteral("ComparePlus: the changed line is marked in both documents"), markerDetail);
		check(markers(newView, 3) & (addedMask | removedMask), QStringLiteral("ComparePlus: the added line is marked"), markerDetail);
		// the blank lines of alignment: "line four" at the same height in both views
		auto yOf = [](EditView* v, const char* text)
		{
			const sptr_t pos = v->wholeText().indexOf(text);
			return v->ex(SCI_POINTYFROMPOSITION, 0, pos);
		};
		check(oldView && newView && yOf(oldView, "line four") == yOf(newView, "line four"),
			QStringLiteral("ComparePlus: the documents are aligned by blank lines"),
			oldView && newView ? QStringLiteral("%1 %2").arg(yOf(oldView, "line four")).arg(yOf(newView, "line four")) : QString());
		bool navBar = false;
		for (QDockWidget* dock : w->findChildren<QDockWidget*>())
			navBar = navBar || (dock->isVisible() && dock->windowTitle() == QStringLiteral("Navigation Bar"));
		check(navBar, QStringLiteral("ComparePlus: the Navigation Bar is docked during the compare"));
		// the caret lines of the old and the new document (ComparePlus moves the caret of the view of the diff and
		// synchronizes the other one after a delay)
		auto carets = [&]
		{
			waitMs(450); // past the alignment of ComparePlus (300 ms after the views move)
			auto line = [](EditView* v) { return v ? v->ex(SCI_LINEFROMPOSITION, v->ex(SCI_GETCURRENTPOS)) : -1; };
			return std::make_pair(line(oldView), line(newView));
		};
		// Compare goes to the first diff; the navigation from there (no caret moved by the test: ComparePlus would
		// synchronize the views again)
		const auto afterCompare = carets();
		run(QStringLiteral("Next Diff Block"));
		const auto nextDiff = carets();
		run(QStringLiteral("First Diff Block"));
		const auto firstDiff = carets();
		run(QStringLiteral("Last Diff Block"));
		const auto lastDiff = carets();
		check(afterCompare.second == 1 && nextDiff.second == 3 && firstDiff.second == 1 && lastDiff.second == 3,
			QStringLiteral("ComparePlus: Compare, Next, First and Last Diff Block go to the diffs"),
			QStringLiteral("new document caret lines: %1, %2, %3, %4").arg(afterCompare.second).arg(nextDiff.second)
				.arg(firstDiff.second).arg(lastDiff.second));
		QString message;
		modalShown(QStringLiteral("p-compareplus-summary"), [&] { run(QStringLiteral("Active Compare Summary")); }, nullptr, &message);
		check(message.contains(QLatin1Char('1')), QStringLiteral("ComparePlus: Active Compare Summary"), message.left(300));
		// Generate Patch: a unified diff, written where the save dialog says
		const QString patchPath = work.path() + QStringLiteral("/compare.diff");
		// a warning about the ignore options of the compare may come first: accepted, then the save dialog
		QStringList patchDialogs;
		QTimer patchTimer;
		patchTimer.setInterval(100);
		int patchTicks = 0;
		QObject::connect(&patchTimer, &QTimer::timeout, [&]
		{
			QWidget* modal = QApplication::activeModalWidget();
			if (auto* box = qobject_cast<QMessageBox*>(modal))
			{
				patchDialogs << box->text().left(80);
				box->accept();
			}
			else if (auto* dialog = qobject_cast<QFileDialog*>(modal))
			{
				// the folder, then the name in the edit of the dialog (an absolute path given to selectFile selects
				// only its folder here); again at the next tick while the dialog stays open
				dialog->setDirectory(QFileInfo(patchPath).absolutePath());
				if (auto* name = dialog->findChild<QLineEdit*>(QStringLiteral("fileNameEdit")))
					name->setText(QFileInfo(patchPath).fileName());
				patchDialogs << QStringLiteral("save dialog: ") + dialog->selectedFiles().join(QLatin1Char(','));
				static_cast<QDialog*>(dialog)->accept(); // public in QDialog
				if (++patchTicks > 100)
					patchTimer.stop();
			}
			else if (++patchTicks > 100)
				patchTimer.stop();
		});
		patchTimer.start();
		run(QStringLiteral("Generate Patch"));
		waitMs(200);
		QByteArray patch;
		{
			QFile f(patchPath);
			if (f.open(QIODevice::ReadOnly))
				patch = f.readAll();
		}
		check(patch.contains("-line two") && patch.contains("+line 2") && patch.contains("+added line") && patch.contains("@@"),
			QStringLiteral("ComparePlus: Generate Patch writes a unified diff"), patchDialogs.join(QStringLiteral(" / ")) + QStringLiteral(": ") + QString::fromUtf8(patch.left(300)));
		patchTimer.stop();
		run(QStringLiteral("Clear Active Compare"));
		waitMs(200);
		check(!w->viewVisible(1) && (markers(w->editView(), 1) & 0xFFFF) == 0, QStringLiteral("ComparePlus: Clear Active Compare"),
			QStringLiteral("sub view visible %1, markers %2").arg(w->viewVisible(1)).arg(markers(w->editView(), 1)));

		// the texts of the two views of a compare (empty when there is no second view)
		auto pairTexts = [w]()
		{
			QStringList texts;
			for (int view = 0; view <= 1; ++view)
				texts << ((w->viewVisible(view) && w->viewEdit(view)) ? QString::fromUtf8(w->viewEdit(view)->wholeText()) : QString());
			return texts;
		};
		// a diff marker (changed, added or removed: lines without a common character are not "changed") in both views
		auto changedLine = [w](int line)
		{
			bool both = w->viewVisible(0) && w->viewVisible(1);
			for (int view = 0; view <= 1 && both; ++view)
				both = (w->viewEdit(view)->ex(SCI_MARKERGET, line) & 0x7) != 0;
			return both;
		};

		// Set as First to Compare: the document compared with the active one later, not with the previous tab
		w->newDocument();
		setDoc(w->editView(), "alpha\nbeta\n");
		run(QStringLiteral("Set as First to Compare"));
		w->newDocument();
		setDoc(w->editView(), "unrelated\n");
		w->newDocument();
		setDoc(w->editView(), "alpha\nbeta\ngamma\n");
		run(QStringLiteral("Compare"));
		waitMs(200);
		QStringList texts = pairTexts();
		check(texts.contains(QStringLiteral("alpha\nbeta\n")) && texts.contains(QStringLiteral("alpha\nbeta\ngamma\n")),
			QStringLiteral("ComparePlus: Set as First to Compare, then Compare"), texts.join(QStringLiteral(" | ")));
		run(QStringLiteral("Clear All Compares"));
		waitMs(200);
		check(!w->viewVisible(1), QStringLiteral("ComparePlus: Clear All Compares"));

		// the clipboard in a temporary document
		QApplication::clipboard()->setText(QStringLiteral("alpha\nBETA\n"));
		w->newDocument();
		setDoc(w->editView(), "alpha\nbeta\n");
		run(QStringLiteral("Compare file/selection to Clipboard"));
		waitMs(200);
		texts = pairTexts();
		check(texts.contains(QStringLiteral("alpha\nBETA\n")) && texts.contains(QStringLiteral("alpha\nbeta\n")) && changedLine(1),
			QStringLiteral("ComparePlus: Compare file/selection to Clipboard"), texts.join(QStringLiteral(" | ")));
		shot(w, QStringLiteral("p-compareplus-clipboard"));
		run(QStringLiteral("Clear Active Compare"));
		waitMs(200);
		bool clipboardDocClosed = true;
		for (Buffer* x : w->allBuffers())
			clipboardDocClosed = clipboardDocClosed && !x->path.contains(QStringLiteral("Clipboard"), Qt::CaseInsensitive);
		check(!w->viewVisible(1) && clipboardDocClosed, QStringLiteral("ComparePlus: the temporary document of the clipboard is closed"));

		// the saved file against the edited document
		const QString compareDir = work.path() + QStringLiteral("/compareplus");
		QDir().mkpath(compareDir);
		{
			QFile f(compareDir + QStringLiteral("/saved.txt"));
			if (f.open(QIODevice::WriteOnly))
				f.write("one\ntwo\nthree\n");
		}
		Buffer* saved = w->openFile(compareDir + QStringLiteral("/saved.txt"));
		if (saved)
		{
			EditView* v = w->editView();
			v->ex(SCI_SETSEL, 4, 7);
			v->ex(SCI_REPLACESEL, 0, reinterpret_cast<sptr_t>("TWO"));
		}
		run(QStringLiteral("Diff since last Save"));
		waitMs(200);
		texts = pairTexts();
		check(saved && texts.contains(QStringLiteral("one\ntwo\nthree\n")) && texts.contains(QStringLiteral("one\nTWO\nthree\n")) && changedLine(1),
			QStringLiteral("ComparePlus: Diff since last Save"), texts.join(QStringLiteral(" | ")));
		run(QStringLiteral("Clear Active Compare"));
		waitMs(200);

		// Git: the file of the last commit (libgit2 of the distribution), a repository made with git
		const QString repo = compareDir + QStringLiteral("/repo");
		QDir().mkpath(repo);
		auto git = [&repo](const QStringList& args)
		{
			QProcess p;
			p.setWorkingDirectory(repo);
			p.start(QStringLiteral("git"), QStringList{QStringLiteral("-c"), QStringLiteral("user.name=test"), QStringLiteral("-c"),
				QStringLiteral("user.email=test@example.com"), QStringLiteral("-c"), QStringLiteral("init.defaultBranch=main")} + args);
			return p.waitForFinished(20000) && p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0;
		};
		auto writeRepoFile = [&repo](const QByteArray& text)
		{
			QFile f(repo + QStringLiteral("/tracked.txt"));
			return f.open(QIODevice::WriteOnly) && f.write(text) == text.size();
		};
		const bool committed = git({QStringLiteral("init"), QStringLiteral("-q")}) && writeRepoFile("a\nb\nc\n")
			&& git({QStringLiteral("add"), QStringLiteral("tracked.txt")}) && git({QStringLiteral("commit"), QStringLiteral("-q"), QStringLiteral("-m"), QStringLiteral("init")})
			&& writeRepoFile("a\nB\nc\n");
		Buffer* tracked = committed ? w->openFile(repo + QStringLiteral("/tracked.txt")) : nullptr;
		if (tracked)
			run(QStringLiteral("Git Diff"));
		waitMs(200);
		texts = pairTexts();
		check(tracked && texts.contains(QStringLiteral("a\nb\nc\n")) && texts.contains(QStringLiteral("a\nB\nc\n")) && changedLine(1),
			QStringLiteral("ComparePlus: Git Diff"), (committed ? QString() : QStringLiteral("(no repository) ")) + texts.join(QStringLiteral(" | ")));
		shot(w, QStringLiteral("p-compareplus-git"));
		run(QStringLiteral("Clear Active Compare"));
		waitMs(200);

		for (const char* item : {"Settings...", "Compare Options (ignore, etc.)...", "Diffs Visual Filters...", "Help / About..."})
		{
			QString title;
			const QString name = QString::fromLatin1(item);
			const bool shown = modalShown(QStringLiteral("p-compareplus-") + name.left(8).remove(QLatin1Char(' ')).remove(QLatin1Char('.')),
				[&] { run(name); }, &title);
			check(shown, QStringLiteral("ComparePlus: the dialog of ") + name, title);
		}
	}

	std::printf("[about dialogs]\n");
	{
		for (const char* plugin : {"SelectQuotedText", "Indent By Fold", "BetterMultiSelection"})
		{
			QAction* about = pluginAction(w, QString::fromLatin1(plugin), QStringLiteral("About..."));
			if (!about)
			{
				check(false, QString::fromLatin1(plugin) + QStringLiteral(": About in the menu"));
				continue;
			}
			QString title;
			bool shown = modalShown(QStringLiteral("p-about-") + QString::fromLatin1(plugin), [about] { about->trigger(); }, &title);
			if (!shown)
				shown = closeWindowTitled(QString(), QStringLiteral("p-about-") + QString::fromLatin1(plugin));
			check(shown, QString::fromLatin1(plugin) + QStringLiteral(": About dialog"), title);
		}
	}

	for (Buffer* x : w->allBuffers())
		w->closeBuffer(x, false);
	delete w;
	std::printf("\n%d checks, %d failed\n", g_checks, g_failures);
	return g_failures ? 1 : 0;
}

#include "plugintest.moc"
