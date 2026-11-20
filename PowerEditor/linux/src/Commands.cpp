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

// Menu commands of the Linux front end: Qt counterpart of NppCommands.cpp,
// the editing helpers of Notepad_plus.cpp / ScintillaEditView.cpp and
// AutoCompletion.cpp.

#include "MainWindow.h"
#include "PluginsManager.h"

#include <algorithm>
#include <climits>
#include <cctype>
#include <cstring>
#include <random>

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCollator>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QMenu>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QComboBox>
#include <QGroupBox>
#include <QDockWidget>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QPainter>
#include <QPrintDialog>
#include <QPrinter>
#include <QProcess>
#include <QPushButton>
#include <QRandomGenerator>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QStatusBar>
#include <QSysInfo>
#include <QTabBar>
#include <QToolBar>
#include <QUrl>

#include "Dialogs.h"
#include "EditView.h"
#include "EncodingMapper.h"
#include "FindReplace.h"
#include "LangStyles.h"
#include "Macros.h"
#include "Udl.h"
#include "Localization.h"
#include "pugixml.hpp"

namespace
{
	QSettings& cmdSettings()
	{
		static QSettings s(Npp::configDir() + QStringLiteral("/config.ini"), QSettings::IniFormat);
		return s;
	}

	// lines an operation applies to: the selected lines, or the whole document
	std::pair<sptr_t, sptr_t> operationLines(EditView* v, bool requireMultiLine)
	{
		if (!v->ex(SCI_GETSELECTIONEMPTY))
		{
			const auto range = v->selectionLinesRange();
			if (!requireMultiLine || range.second > range.first)
				return range;
		}
		return {0, v->ex(SCI_GETLINECOUNT) - 1};
	}

	void replaceLines(EditView* v, sptr_t first, sptr_t last, const QList<QByteArray>& lines)
	{
		const sptr_t start = v->ex(SCI_POSITIONFROMLINE, first);
		const sptr_t end = v->ex(SCI_GETLINEENDPOSITION, last);
		QByteArray text;
		const QByteArray eol = v->eolString();
		for (int i = 0; i < lines.size(); ++i)
		{
			if (i)
				text += eol;
			text += lines[i];
		}
		v->replaceRange(start, end, text);
	}

	int styleIndicator(int n) // 1..5, 0 for the "Find Mark Style"
	{
		static const int indics[] = {SCE_UNIVERSAL_FOUND_STYLE, SCE_UNIVERSAL_FOUND_STYLE_EXT1, SCE_UNIVERSAL_FOUND_STYLE_EXT2,
		                             SCE_UNIVERSAL_FOUND_STYLE_EXT3, SCE_UNIVERSAL_FOUND_STYLE_EXT4, SCE_UNIVERSAL_FOUND_STYLE_EXT5};
		return indics[std::clamp(n, 0, 5)];
	}

	QByteArray indicatorText(EditView* v, int indic)
	{
		QByteArray out;
		const QByteArray eol = v->eolString();
		sptr_t pos = 0;
		const sptr_t len = v->ex(SCI_GETLENGTH);
		while (pos < len)
		{
			const sptr_t runEnd = v->ex(SCI_INDICATOREND, indic, pos);
			if (v->ex(SCI_INDICATORVALUEAT, indic, pos))
			{
				if (!out.isEmpty())
					out += eol;
				out += v->textRange(pos, runEnd);
			}
			if (runEnd <= pos)
				break;
			pos = runEnd;
		}
		return out;
	}

	// ---------------------------------------------------------------------------
	// auto-completion data (installer/APIs)
	// ---------------------------------------------------------------------------
	struct ApiFunc
	{
		QString retVal;
		QString descr;
		QStringList params;
	};

	struct ApiData
	{
		bool loaded = false;
		QStringList keywords;
		QHash<QString, QList<ApiFunc>> funcs;
		bool ignoreCase = true;
		char startFunc = '(';
		char stopFunc = ')';
		char paramSeparator = ',';
	};

	ApiData& apiFor(LangType lang)
	{
		static QHash<int, ApiData> cache;
		ApiData& d = cache[static_cast<int>(lang)];
		if (d.loaded)
			return d;
		d.loaded = true;

		QString name = QString::fromUtf8(StyleManager::langInfo(lang).langName);
		if (lang == L_JAVASCRIPT || lang == L_JS_EMBEDDED)
			name = QStringLiteral("javascript");
		else if (lang == L_COFFEESCRIPT)
			name = QStringLiteral("coffee");

		QString path;
		const QDir dir(Npp::dataDir() + QStringLiteral("/autoCompletion"));
		for (const QString& f : dir.entryList({QStringLiteral("*.xml")}, QDir::Files))
		{
			if (QFileInfo(f).completeBaseName().compare(name, Qt::CaseInsensitive) == 0)
				path = dir.absoluteFilePath(f);
		}
		if (path.isEmpty())
			return d;

		pugi::xml_document doc;
		if (!doc.load_file(QFile::encodeName(path).constData()))
			return d;
		pugi::xml_node ac = doc.child("NotepadPlus").child("AutoComplete");
		pugi::xml_node env = ac.child("Environment");
		if (env)
		{
			d.ignoreCase = std::string(env.attribute("ignoreCase").as_string("yes")) != "no";
			auto ch = [](const pugi::xml_attribute& a, char def) { const char* s = a.as_string(); return (s && *s) ? s[0] : def; };
			d.startFunc = ch(env.attribute("startFunc"), '(');
			d.stopFunc = ch(env.attribute("stopFunc"), ')');
			d.paramSeparator = ch(env.attribute("paramSeparator"), ',');
		}
		for (pugi::xml_node kw : ac.children("KeyWord"))
		{
			const QString kwName = QString::fromUtf8(kw.attribute("name").as_string());
			if (kwName.isEmpty())
				continue;
			d.keywords << kwName;
			for (pugi::xml_node ov : kw.children("Overload"))
			{
				ApiFunc f;
				f.retVal = QString::fromUtf8(ov.attribute("retVal").as_string());
				f.descr = QString::fromUtf8(ov.attribute("descr").as_string());
				for (pugi::xml_node p : ov.children("Param"))
					f.params << QString::fromUtf8(p.attribute("name").as_string());
				d.funcs[kwName].append(f);
			}
		}
		return d;
	}

	QString findTerminal()
	{
		const QString env = qEnvironmentVariable("TERMINAL");
		if (!env.isEmpty() && !QStandardPaths::findExecutable(env).isEmpty())
			return env;
		for (const char* t : {"konsole", "gnome-terminal", "kgx", "ptyxis", "xfce4-terminal", "mate-terminal", "tilix", "alacritty", "kitty", "foot", "wezterm", "xterm"})
		{
			if (!QStandardPaths::findExecutable(QString::fromLatin1(t)).isEmpty())
				return QString::fromLatin1(t);
		}
		return {};
	}
}

// ---------------------------------------------------------------------------
// registration
// ---------------------------------------------------------------------------
void MainWindow::registerCommands()
{
	auto onFocused = [this](unsigned int sciMsg, auto lineEditFn)
	{
		QWidget* fw = QApplication::focusWidget();
		if (auto* sci = qobject_cast<ScintillaEditBase*>(fw))
			sci->send(sciMsg);
		else if (auto* le = qobject_cast<QLineEdit*>(fw))
			(le->*lineEditFn)();
		else
			_edit->ex(sciMsg);
	};

	// File
	on(IDM_FILE_NEW, [this] { newDocument(true); });
	on(IDM_FILE_OPEN, [this] { fileOpen(); });
	on(IDM_FILE_OPEN_FOLDER, [this] { openContainingFolder(); });
	on(IDM_FILE_OPEN_CMD, [this] { openTerminal(false); });
	on(IDM_FILE_OPEN_POWERSHELL, [this] { openTerminal(true); });
	on(IDM_FILE_CONTAININGFOLDERASWORKSPACE, [this]
	{
		if (_current && !_current->isUntitled())
			toggleWorkspace(QFileInfo(_current->path).absolutePath());
	});
	on(IDM_FILE_OPEN_DEFAULT_VIEWER, [this]
	{
		if (_current && !_current->isUntitled())
			QDesktopServices::openUrl(QUrl::fromLocalFile(_current->path));
	});
	on(IDM_FILE_OPENFOLDERASWORKSPACE, [this]
	{
		const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("Select a folder to add in Folder as Workspace panel"), QDir::homePath());
		if (!dir.isEmpty())
			toggleWorkspace(dir);
	});
	on(IDM_FILE_RELOAD, [this]
	{
		if (!_current || _current->isUntitled())
			return;
		if (_current->dirty)
		{
			const auto answer = QMessageBox::question(this, QStringLiteral("Reload"),
				QStringLiteral("Are you sure you want to reload the current file and lose the changes made in Notepad++?"));
			if (answer != QMessageBox::Yes)
				return;
		}
		reloadBuffer(_current);
	});
	on(IDM_FILE_SAVE, [this] { saveBuffer(_current); });
	on(IDM_FILE_SAVEAS, [this] { saveBuffer(_current, true); });
	on(IDM_FILE_SAVECOPYAS, [this] { saveBuffer(_current, false, true); });
	on(IDM_FILE_SAVEALL, [this] { fileSaveAll(); });
	on(IDM_FILE_RENAME, [this] { fileRename(); });
	on(IDM_FILE_CLOSE, [this] { closeBuffer(_current, true); });
	on(IDM_FILE_CLOSEALL, [this] { fileCloseAll(false, false, 0, false); });
	on(IDM_FILE_CLOSEALL_BUT_CURRENT, [this] { fileCloseAll(true, false, 0, false); });
	on(IDM_FILE_CLOSEALL_BUT_PINNED, [this] { fileCloseAll(false, true, 0, false); });
	on(IDM_FILE_CLOSEALL_TOLEFT, [this] { fileCloseAll(false, false, -1, false); });
	on(IDM_FILE_CLOSEALL_TORIGHT, [this] { fileCloseAll(false, false, 1, false); });
	on(IDM_FILE_CLOSEALL_UNCHANGED, [this] { fileCloseAll(false, false, 0, true); });
	on(IDM_FILE_DELETE, [this] { fileDelete(); });
	on(IDM_FILE_LOADSESSION, [this] { fileLoadSession(); });
	on(IDM_FILE_SAVESESSION, [this] { fileSaveSession(); });
	on(IDM_FILE_PRINT, [this] { filePrint(true); });
	on(IDM_FILE_PRINTNOW, [this] { filePrint(false); });
	on(IDM_FILE_EXIT, [this]
	{
		_reallyQuit = true; // File > Exit quits even with "Close to" system tray
		close();
	});
	on(IDM_FILE_RESTORELASTCLOSEDFILE, [this]
	{
		while (!_closedFiles.isEmpty())
		{
			const QString p = _closedFiles.takeFirst();
			if (QFileInfo::exists(p))
			{
				openFile(p);
				break;
			}
		}
		updateMenuState();
	});
	on(IDM_OPEN_ALL_RECENT_FILE, [this]
	{
		const QStringList files = _recent;
		for (const QString& p : files)
		{
			if (QFileInfo::exists(p))
				openFile(p);
		}
	});
	on(IDM_CLEAN_RECENT_FILE_LIST, [this]
	{
		_recent.clear();
		cmdSettings().setValue(QStringLiteral("recentFiles"), _recent);
		rebuildRecentMenu();
	});

	// Edit
	on(IDM_EDIT_UNDO, [onFocused] { onFocused(SCI_UNDO, &QLineEdit::undo); });
	on(IDM_EDIT_REDO, [onFocused] { onFocused(SCI_REDO, &QLineEdit::redo); });
	// Editing 1 > Enable Copy/Cut Line without selection
	on(IDM_EDIT_CUT, [onFocused] { onFocused(Npp::prefs().lineCopyCutWithoutSelection ? SCI_CUTALLOWLINE : SCI_CUT, &QLineEdit::cut); });
	on(IDM_EDIT_COPY, [onFocused] { onFocused(Npp::prefs().lineCopyCutWithoutSelection ? SCI_COPYALLOWLINE : SCI_COPY, &QLineEdit::copy); });
	on(IDM_EDIT_PASTE, [onFocused] { onFocused(SCI_PASTE, &QLineEdit::paste); });
	on(IDM_EDIT_DELETE, [onFocused] { onFocused(SCI_CLEAR, &QLineEdit::del); });
	on(IDM_EDIT_SELECTALL, [onFocused] { onFocused(SCI_SELECTALL, &QLineEdit::selectAll); });
	for (int id : {IDM_EDIT_BEGINENDSELECT, IDM_EDIT_BEGINENDSELECT_COLUMNMODE})
	{
		on(id, [this, id]
		{
			// ScintillaEditView::beginOrEndSelect()
			const bool column = (id == IDM_EDIT_BEGINENDSELECT_COLUMNMODE);
			if (_beginSelectPos < 0)
			{
				_beginSelectPos = _edit->ex(SCI_GETCURRENTPOS);
				_beginSelectColumn = column;
				setStatusText(QStringLiteral("Begin/End Select: start position set"));
			}
			else
			{
				const sptr_t pos = _edit->ex(SCI_GETCURRENTPOS);
				if (_beginSelectColumn)
				{
					_edit->ex(SCI_SETSELECTIONMODE, SC_SEL_RECTANGLE);
					_edit->ex(SCI_SETRECTANGULARSELECTIONANCHOR, _beginSelectPos);
					_edit->ex(SCI_SETRECTANGULARSELECTIONCARET, pos);
				}
				else
				{
					_edit->ex(SCI_SETSEL, _beginSelectPos, pos);
				}
				_beginSelectPos = -1;
			}
		});
	}
	for (int id : {IDM_EDIT_INSERT_DATETIME_SHORT, IDM_EDIT_INSERT_DATETIME_LONG, IDM_EDIT_INSERT_DATETIME_CUSTOMIZED})
		on(id, [this, id] { editInsertDateTime(id); });
	for (int id : {IDM_EDIT_FULLPATHTOCLIP, IDM_EDIT_FILENAMETOCLIP, IDM_EDIT_CURRENTDIRTOCLIP, IDM_EDIT_COPY_ALL_NAMES, IDM_EDIT_COPY_ALL_PATHS})
		on(id, [this, id] { editCopyToClipboard(id); });
	on(IDM_EDIT_INS_TAB, [this] { _edit->ex(SCI_TAB); });
	on(IDM_EDIT_RMV_TAB, [this] { _edit->ex(SCI_BACKTAB); });
	for (int id : {IDM_EDIT_UPPERCASE, IDM_EDIT_LOWERCASE, IDM_EDIT_PROPERCASE_FORCE, IDM_EDIT_PROPERCASE_BLEND, IDM_EDIT_SENTENCECASE_FORCE,
	               IDM_EDIT_SENTENCECASE_BLEND, IDM_EDIT_INVERTCASE, IDM_EDIT_RANDOMCASE})
		on(id, [this, id] { editConvertCase(id); });
	for (int id : {IDM_EDIT_DUP_LINE, IDM_EDIT_REMOVE_ANY_DUP_LINES, IDM_EDIT_REMOVE_CONSECUTIVE_DUP_LINES, IDM_EDIT_SPLIT_LINES,
	               IDM_EDIT_JOIN_LINES, IDM_EDIT_LINE_UP, IDM_EDIT_LINE_DOWN, IDM_EDIT_REMOVEEMPTYLINES, IDM_EDIT_REMOVEEMPTYLINESWITHBLANK,
	               IDM_EDIT_BLANKLINEABOVECURRENT, IDM_EDIT_BLANKLINEBELOWCURRENT, IDM_EDIT_TRANSPOSE_LINE})
		on(id, [this, id] { editLineOperation(id); });
	for (int id : {IDM_EDIT_SORTLINES_LEXICOGRAPHIC_ASCENDING, IDM_EDIT_SORTLINES_LEXICOGRAPHIC_DESCENDING,
	               IDM_EDIT_SORTLINES_LEXICO_CASE_INSENS_ASCENDING, IDM_EDIT_SORTLINES_LEXICO_CASE_INSENS_DESCENDING,
	               IDM_EDIT_SORTLINES_LOCALE_ASCENDING, IDM_EDIT_SORTLINES_LOCALE_DESCENDING,
	               IDM_EDIT_SORTLINES_INTEGER_ASCENDING, IDM_EDIT_SORTLINES_INTEGER_DESCENDING,
	               IDM_EDIT_SORTLINES_DECIMALCOMMA_ASCENDING, IDM_EDIT_SORTLINES_DECIMALCOMMA_DESCENDING,
	               IDM_EDIT_SORTLINES_DECIMALDOT_ASCENDING, IDM_EDIT_SORTLINES_DECIMALDOT_DESCENDING,
	               IDM_EDIT_SORTLINES_LENGTH_ASCENDING, IDM_EDIT_SORTLINES_LENGTH_DESCENDING,
	               IDM_EDIT_SORTLINES_RANDOMLY, IDM_EDIT_SORTLINES_REVERSE_ORDER})
		on(id, [this, id] { editSortLines(id); });
	for (int id : {IDM_EDIT_BLOCK_COMMENT, IDM_EDIT_BLOCK_COMMENT_SET, IDM_EDIT_BLOCK_UNCOMMENT, IDM_EDIT_STREAM_COMMENT, IDM_EDIT_STREAM_UNCOMMENT})
		on(id, [this, id] { editComment(id); });
	on(IDM_EDIT_AUTOCOMPLETE, [this] { editAutoComplete(false, true); });
	on(IDM_EDIT_AUTOCOMPLETE_CURRENTFILE, [this] { editAutoComplete(true, true); });
	on(IDM_EDIT_AUTOCOMPLETE_PATH, [this]
	{
		// complete a file path written before the caret
		const sptr_t pos = _edit->ex(SCI_GETCURRENTPOS);
		const sptr_t lineStart = _edit->ex(SCI_POSITIONFROMLINE, _edit->ex(SCI_LINEFROMPOSITION, pos));
		const QString before = _edit->textRangeQ(lineStart, pos);
		const int sp = static_cast<int>(before.lastIndexOf(QRegularExpression(QStringLiteral("[\\s\"'<>|]"))));
		QString typed = before.mid(sp + 1);
		if (typed.startsWith(QLatin1Char('~')))
			typed.replace(0, 1, QDir::homePath());
		const QFileInfo fi(typed.endsWith(QLatin1Char('/')) ? typed + QStringLiteral(".") : typed);
		const QString dir = typed.endsWith(QLatin1Char('/')) ? typed : fi.path();
		const QString prefix = typed.endsWith(QLatin1Char('/')) ? QString() : fi.fileName();
		QStringList items;
		for (const QString& e : QDir(dir).entryList(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden, QDir::Name))
		{
			if (e.startsWith(prefix))
				items << e;
		}
		if (!items.isEmpty())
		{
			const QByteArray list = items.join(QLatin1Char('\n')).toUtf8();
			_edit->ex(SCI_AUTOCSHOW, prefix.toUtf8().size(), reinterpret_cast<sptr_t>(list.constData()));
		}
	});
	on(IDM_EDIT_FUNCCALLTIP, [this] { editFunctionCallTip(0); });
	on(IDM_EDIT_FUNCCALLTIP_PREVIOUS, [this] { editFunctionCallTip(-1); });
	on(IDM_EDIT_FUNCCALLTIP_NEXT, [this] { editFunctionCallTip(1); });
	on(IDM_FORMAT_TODOS, [this] { editEol(SC_EOL_CRLF); });
	on(IDM_FORMAT_TOUNIX, [this] { editEol(SC_EOL_LF); });
	on(IDM_FORMAT_TOMAC, [this] { editEol(SC_EOL_CR); });
	for (int id : {IDM_EDIT_TRIMTRAILING, IDM_EDIT_TRIMLINEHEAD, IDM_EDIT_TRIM_BOTH, IDM_EDIT_EOL2WS, IDM_EDIT_TRIMALL,
	               IDM_EDIT_TAB2SW, IDM_EDIT_SW2TAB_LEADING, IDM_EDIT_SW2TAB_ALL})
		on(id, [this, id] { editBlankOperation(id); });
	for (int id : {IDM_EDIT_OPENSELECTEDFILETOEDIT, IDM_EDIT_OPENSELECTEDFILEFOLDERINEXPLORER, IDM_EDIT_SEARCHONINTERNET})
		on(id, [this, id] { editOpenSelection(id); });
	on(IDM_EDIT_CHANGESEARCHENGINE, [this] { settingsPreferences(); });
	for (int id : {IDM_EDIT_MULTISELECTALL, IDM_EDIT_MULTISELECTALLMATCHCASE, IDM_EDIT_MULTISELECTALLWHOLEWORD, IDM_EDIT_MULTISELECTALLMATCHCASEWHOLEWORD,
	               IDM_EDIT_MULTISELECTNEXT, IDM_EDIT_MULTISELECTNEXTMATCHCASE, IDM_EDIT_MULTISELECTNEXTWHOLEWORD, IDM_EDIT_MULTISELECTNEXTMATCHCASEWHOLEWORD,
	               IDM_EDIT_MULTISELECTUNDO, IDM_EDIT_MULTISELECTSSKIP})
		on(id, [this, id] { editMultiSelect(id); });
	on(IDM_EDIT_COLUMNMODETIP, [this]
	{
		QMessageBox::information(this, QStringLiteral("Column Mode Tip"),
			QStringLiteral("There are 3 ways to switch to column-select mode:\n\n"
			               "1. (Keyboard and Mouse)  Hold Alt while left-click dragging\n\n"
			               "2. (Keyboard only)  Hold Alt+Shift while using arrow keys\n\n"
			               "3. (Keyboard or Mouse)\n      Put caret at desired start of column block position, then\n"
			               "       execute \"Begin/End Select in Column Mode\" command;\n"
			               "      Move caret to desired end of column block position, then\n"
			               "       execute \"Begin/End Select in Column Mode\" command again"));
	});
	on(IDM_EDIT_COLUMNMODE, [this] { editColumnEditor(); });
	on(IDM_EDIT_TOGGLEREADONLY, [this]
	{
		if (!_current)
			return;
		_current->userReadOnly = !_current->userReadOnly;
		_edit->ex(SCI_SETREADONLY, _current->isReadOnly());
		updateTabLabel(indexOf(_current));
		updateMenuState();
	});
	on(IDM_EDIT_TOGGLESYSTEMREADONLY, [this]
	{
		if (!_current || _current->isUntitled())
			return;
		QFile f(_current->path);
		QFileDevice::Permissions perms = f.permissions();
		if (perms & QFileDevice::WriteOwner)
			perms &= ~(QFileDevice::WriteOwner | QFileDevice::WriteUser | QFileDevice::WriteGroup | QFileDevice::WriteOther);
		else
			perms |= QFileDevice::WriteOwner | QFileDevice::WriteUser;
		if (!f.setPermissions(perms))
			QMessageBox::warning(this, QStringLiteral("Read-Only"), QStringLiteral("Cannot change the permissions of \"%1\".").arg(_current->path));
		_current->fileReadOnly = !QFileInfo(_current->path).isWritable();
		_edit->ex(SCI_SETREADONLY, _current->isReadOnly());
		updateTabLabel(indexOf(_current));
	});
	on(IDM_EDIT_SETREADONLYFORALLDOCS, [this]
	{
		for (Buffer* b : allBuffers())
		{
			b->userReadOnly = true;
			withDocument(b, [b](EditView* v) { v->ex(SCI_SETREADONLY, b->isReadOnly()); });
		}
		updateAllTabs();
		updateMenuState();
	});
	on(IDM_EDIT_CLEARREADONLYFORALLDOCS, [this]
	{
		for (Buffer* b : allBuffers())
		{
			b->userReadOnly = false;
			withDocument(b, [b](EditView* v) { v->ex(SCI_SETREADONLY, b->isReadOnly()); });
		}
		updateAllTabs();
		updateMenuState();
	});
	on(IDM_EDIT_REDACT_SELECTION, [this]
	{
		if (_edit->ex(SCI_GETSELECTIONEMPTY))
			return;
		_edit->ex(SCI_BEGINUNDOACTION);
		const sptr_t n = _edit->ex(SCI_GETSELECTIONS);
		for (sptr_t i = n - 1; i >= 0; --i)
		{
			const sptr_t s = _edit->ex(SCI_GETSELECTIONNSTART, i);
			const sptr_t e = _edit->ex(SCI_GETSELECTIONNEND, i);
			QString t = _edit->textRangeQ(s, e);
			for (QChar& c : t)
			{
				if (c != QLatin1Char('\r') && c != QLatin1Char('\n') && !c.isSpace())
					c = QChar(0x2588);
			}
			_edit->replaceRange(s, e, t.toUtf8());
		}
		_edit->ex(SCI_ENDUNDOACTION);
	});

	// Search
	on(IDM_SEARCH_FIND, [this] { showFindDialog(FindReplaceDlg::FindTab); });
	on(IDM_SEARCH_REPLACE, [this] { showFindDialog(FindReplaceDlg::ReplaceTab); });
	on(IDM_SEARCH_FINDINFILES, [this] { showFindDialog(FindReplaceDlg::FindInFilesTab); });
	on(IDM_SEARCH_MARK, [this] { showFindDialog(FindReplaceDlg::MarkTab); });
	on(IDM_SEARCH_FINDNEXT, [this] { searchFindNext(true, false, false); });
	on(IDM_SEARCH_FINDPREV, [this] { searchFindNext(false, false, false); });
	on(IDM_SEARCH_SETANDFINDNEXT, [this] { searchFindNext(true, true, false); });
	on(IDM_SEARCH_SETANDFINDPREV, [this] { searchFindNext(false, true, false); });
	on(IDM_SEARCH_VOLATILE_FINDNEXT, [this] { searchFindNext(true, false, true); });
	on(IDM_SEARCH_VOLATILE_FINDPREV, [this] { searchFindNext(false, false, true); });
	on(IDM_SEARCH_GOTOLINE, [this] { searchGotoLine(); });
	on(IDM_SEARCH_GOTOMATCHINGBRACE, [this] { searchMatchingBrace(false); });
	on(IDM_SEARCH_SELECTMATCHINGBRACES, [this] { searchMatchingBrace(true); });
	on(IDM_FOCUS_ON_FOUND_RESULTS, [this]
	{
		if (_resultsDock && _resultsDock->isVisible() && _results->view()->hasFocus())
			_edit->setFocus();
		else
			resultsPanel()->view()->setFocus();
	});
	on(IDM_SEARCH_GOTONEXTFOUND, [this] { if (_results) _results->gotoNextResult(true); });
	on(IDM_SEARCH_GOTOPREVFOUND, [this] { if (_results) _results->gotoNextResult(false); });
	for (int id : {IDM_SEARCH_TOGGLE_BOOKMARK, IDM_SEARCH_NEXT_BOOKMARK, IDM_SEARCH_PREV_BOOKMARK, IDM_SEARCH_CLEAR_BOOKMARKS,
	               IDM_SEARCH_CUTMARKEDLINES, IDM_SEARCH_COPYMARKEDLINES, IDM_SEARCH_PASTEMARKEDLINES, IDM_SEARCH_DELETEMARKEDLINES,
	               IDM_SEARCH_DELETEUNMARKEDLINES, IDM_SEARCH_INVERSEMARKS})
		on(id, [this, id] { searchBookmark(id); });
	for (int id : {IDM_SEARCH_MARKALLEXT1, IDM_SEARCH_MARKALLEXT2, IDM_SEARCH_MARKALLEXT3, IDM_SEARCH_MARKALLEXT4, IDM_SEARCH_MARKALLEXT5,
	               IDM_SEARCH_MARKONEEXT1, IDM_SEARCH_MARKONEEXT2, IDM_SEARCH_MARKONEEXT3, IDM_SEARCH_MARKONEEXT4, IDM_SEARCH_MARKONEEXT5,
	               IDM_SEARCH_UNMARKALLEXT1, IDM_SEARCH_UNMARKALLEXT2, IDM_SEARCH_UNMARKALLEXT3, IDM_SEARCH_UNMARKALLEXT4, IDM_SEARCH_UNMARKALLEXT5,
	               IDM_SEARCH_CLEARALLMARKS})
		on(id, [this, id] { searchStyleToken(id); });
	for (int id : {IDM_SEARCH_GOPREVMARKER1, IDM_SEARCH_GOPREVMARKER2, IDM_SEARCH_GOPREVMARKER3, IDM_SEARCH_GOPREVMARKER4, IDM_SEARCH_GOPREVMARKER5,
	               IDM_SEARCH_GOPREVMARKER_DEF, IDM_SEARCH_GONEXTMARKER1, IDM_SEARCH_GONEXTMARKER2, IDM_SEARCH_GONEXTMARKER3,
	               IDM_SEARCH_GONEXTMARKER4, IDM_SEARCH_GONEXTMARKER5, IDM_SEARCH_GONEXTMARKER_DEF})
		on(id, [this, id] { searchJumpToStyle(id); });
	for (int id : {IDM_SEARCH_STYLE1TOCLIP, IDM_SEARCH_STYLE2TOCLIP, IDM_SEARCH_STYLE3TOCLIP, IDM_SEARCH_STYLE4TOCLIP, IDM_SEARCH_STYLE5TOCLIP,
	               IDM_SEARCH_ALLSTYLESTOCLIP, IDM_SEARCH_MARKEDTOCLIP})
		on(id, [this, id] { searchCopyStyled(id); });
	for (int id : {IDM_SEARCH_CHANGED_NEXT, IDM_SEARCH_CHANGED_PREV, IDM_SEARCH_CLEAR_CHANGE_HISTORY})
		on(id, [this, id] { searchChangeHistory(id); });

	// View
	on(IDM_VIEW_ALWAYSONTOP, [this]
	{
		Npp::prefs().alwaysOnTop = !Npp::prefs().alwaysOnTop;
		setWindowFlag(Qt::WindowStaysOnTopHint, Npp::prefs().alwaysOnTop);
		show();
		updateMenuState();
	});
	on(IDM_VIEW_FULLSCREENTOGGLE, [this] { viewFullScreen(false); });
	on(IDM_VIEW_POSTIT, [this] { viewFullScreen(true); });
	for (int id : {IDM_VIEW_IN_FIREFOX, IDM_VIEW_IN_CHROME, IDM_VIEW_IN_EDGE, IDM_VIEW_IN_IE})
		on(id, [this, id] { viewInBrowser(id); });
	for (int id : {IDM_VIEW_TAB_SPACE, IDM_VIEW_EOL, IDM_VIEW_ALL_CHARACTERS, IDM_VIEW_INDENT_GUIDE, IDM_VIEW_WRAP_SYMBOL, IDM_VIEW_WRAP})
		on(id, [this, id] { viewToggle(id); });
	for (int id : {IDM_VIEW_ZOOMIN, IDM_VIEW_ZOOMOUT, IDM_VIEW_ZOOMRESTORE})
		on(id, [this, id] { viewZoom(id); });
	for (int id : {IDM_VIEW_TAB1, IDM_VIEW_TAB2, IDM_VIEW_TAB3, IDM_VIEW_TAB4, IDM_VIEW_TAB5, IDM_VIEW_TAB6, IDM_VIEW_TAB7, IDM_VIEW_TAB8,
	               IDM_VIEW_TAB9, IDM_VIEW_TAB_MOVEFORWARD, IDM_VIEW_TAB_MOVEBACKWARD, IDM_VIEW_TAB_START, IDM_VIEW_TAB_END,
	               IDM_VIEW_GOTO_START, IDM_VIEW_GOTO_END})
		on(id, [this, id] { viewTab(id); });
	for (int id : {IDM_VIEW_TAB_NEXT, IDM_VIEW_TAB_PREV})
	{
		on(id, [this, id]
		{
			// Ctrl+Tab opens the document switcher (MISC. > Document Switcher)
			if (QApplication::keyboardModifiers() & Qt::ControlModifier)
				documentSwitcher(id == IDM_VIEW_TAB_NEXT);
			else
				viewTab(id);
		});
	}
	for (int id : {IDM_VIEW_TAB_COLOUR_NONE, IDM_VIEW_TAB_COLOUR_1, IDM_VIEW_TAB_COLOUR_2, IDM_VIEW_TAB_COLOUR_3, IDM_VIEW_TAB_COLOUR_4, IDM_VIEW_TAB_COLOUR_5})
	{
		on(id, [this, id]
		{
			if (!_current)
				return;
			_current->tabColor = (id == IDM_VIEW_TAB_COLOUR_NONE) ? -1 : id - IDM_VIEW_TAB_COLOUR_1;
			for (DocView& dv : _views)
				dv.tabs->update();
			updateMenuState();
		});
	}
	// split view
	on(IDM_VIEW_GOTO_ANOTHER_VIEW, [this] { moveToOtherView(false); });
	on(IDM_VIEW_CLONE_TO_ANOTHER_VIEW, [this] { moveToOtherView(true); });
	on(IDM_VIEW_SWITCHTO_OTHER_VIEW, [this]
	{
		if (viewVisible(otherView()))
			setActiveView(otherView());
	});
	on(IDM_VIEW_SYNSCROLLV, [this]
	{
		_sync.vertical = !_sync.vertical && viewVisible(MAIN_VIEW) && viewVisible(SUB_VIEW);
		if (_sync.vertical)
			_sync.line = _views[MAIN_VIEW].edit->ex(SCI_GETFIRSTVISIBLELINE) - _views[SUB_VIEW].edit->ex(SCI_GETFIRSTVISIBLELINE);
		updateMenuState();
	});
	on(IDM_VIEW_SYNSCROLLH, [this]
	{
		_sync.horizontal = !_sync.horizontal && viewVisible(MAIN_VIEW) && viewVisible(SUB_VIEW);
		if (_sync.horizontal)
		{
			auto column = [](EditView* e) { return e->ex(SCI_GETXOFFSET) / std::max<sptr_t>(1, e->exs(SCI_TEXTWIDTH, STYLE_DEFAULT, "P")); };
			_sync.column = column(_views[MAIN_VIEW].edit) - column(_views[SUB_VIEW].edit);
		}
		updateMenuState();
	});
	on(IDM_VIEW_ZOOM_SYNC, [this]
	{
		NppPrefs& p = Npp::prefs();
		p.zoomSync = !p.zoomSync;
		p.save();
		syncZoom(_activeView);
		updateMenuState();
	});
	on(IDM_VIEW_NPC, [this] { viewToggle(IDM_VIEW_NPC); });
	on(IDM_VIEW_NPC_CCUNIEOL, [this] { viewToggle(IDM_VIEW_NPC_CCUNIEOL); });
	on(IDM_VIEW_DISTRACTIONFREE, [this] { toggleDistractionFree(); });
	for (int id : {IDM_VIEW_FOLDALL, IDM_VIEW_UNFOLDALL, IDM_VIEW_FOLD_CURRENT, IDM_VIEW_UNFOLD_CURRENT, IDM_VIEW_FOLD_1, IDM_VIEW_FOLD_2,
	               IDM_VIEW_FOLD_3, IDM_VIEW_FOLD_4, IDM_VIEW_FOLD_5, IDM_VIEW_FOLD_6, IDM_VIEW_FOLD_7, IDM_VIEW_FOLD_8, IDM_VIEW_UNFOLD_1,
	               IDM_VIEW_UNFOLD_2, IDM_VIEW_UNFOLD_3, IDM_VIEW_UNFOLD_4, IDM_VIEW_UNFOLD_5, IDM_VIEW_UNFOLD_6, IDM_VIEW_UNFOLD_7, IDM_VIEW_UNFOLD_8})
		on(id, [this, id] { viewFold(id); });
	on(IDM_VIEW_SUMMARY, [this] { viewSummary(); });
	on(IDM_VIEW_FILEBROWSER, [this] { toggleWorkspace(); });
	on(IDM_VIEW_DOCLIST, [this] { toggleDocumentList(); });
	on(IDM_VIEW_DOC_MAP, [this] { toggleDocumentMap(); });
	on(IDM_EDIT_CHAR_PANEL, [this] { toggleCharacterPanel(); });
	for (int i = 0; i < 3; ++i)
	{
		on(IDM_VIEW_PROJECT_PANEL_1 + i, [this, i] { toggleProjectPanel(i); });
		on(IDM_VIEW_SWITCHTO_PROJECT_PANEL_1 + i, [this, i] { toggleProjectPanel(i, true); });
	}
	on(IDM_SEARCH_FINDCHARINRANGE, [this] { NppDialogs::findCharsInRange(this); });
	on(IDM_SEARCH_FINDINCREMENT, [this] { incrementalSearch(); });
	on(IDM_VIEW_HIDELINES, [this] { _edit->hideLines(); });
	on(IDM_EDIT_CLIPBOARDHISTORY_PANEL, [this] { toggleClipboardHistory(); });
	for (int id : {IDM_EDIT_COPY_BINARY, IDM_EDIT_CUT_BINARY, IDM_EDIT_PASTE_BINARY, IDM_EDIT_PASTE_AS_HTML, IDM_EDIT_PASTE_AS_RTF})
		on(id, [this, id] { pasteSpecial(id); });
	on(IDM_VIEW_FUNC_LIST, [this] { toggleFunctionList(); });
	on(IDM_VIEW_SWITCHTO_FUNC_LIST, [this] { toggleFunctionList(true); });
	on(IDM_VIEW_MONITORING, [this]
	{
		if (!_current || _current->isUntitled())
		{
			QMessageBox::information(this, QStringLiteral("Monitoring"), QStringLiteral("The document must be saved to a file to be monitored."));
			updateMenuState();
			return;
		}
		if (!_current->monitoring && _current->dirty)
		{
			const auto answer = QMessageBox::question(this, QStringLiteral("Monitoring"),
				QStringLiteral("The file \"%1\" has unsaved changes. Save them before monitoring?").arg(_current->path),
				QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
			if (answer == QMessageBox::Cancel || (answer == QMessageBox::Yes && !saveBuffer(_current)))
			{
				updateMenuState();
				return;
			}
		}
		_current->monitoring = !_current->monitoring;
		if (_current->monitoring)
		{
			reloadBuffer(_current, FileIO::Force::Auto, CP_ANSI, false);
			_edit->ex(SCI_DOCUMENTEND);
		}
		_edit->ex(SCI_SETREADONLY, _current->isReadOnly());
		updateTabLabel(indexOf(_current));
		updateMenuState();
	});
	on(IDM_VIEW_LOAD_IN_NEW_INSTANCE, [this]
	{
		if (_current && !_current->isUntitled())
			QProcess::startDetached(QApplication::applicationFilePath(), {QStringLiteral("-multiInst"), QStringLiteral("-nosession"), _current->path});
	});
	on(IDM_VIEW_GOTO_NEW_INSTANCE, [this]
	{
		if (!_current || _current->isUntitled())
			return;
		if (_current->dirty && !saveBuffer(_current))
			return;
		QProcess::startDetached(QApplication::applicationFilePath(), {QStringLiteral("-multiInst"), QStringLiteral("-nosession"), _current->path});
		closeBuffer(_current, false);
	});
	on(IDM_PINTAB, [this]
	{
		if (!_current)
			return;
		_current->pinned = !_current->pinned;
		updateTabLabel(indexOf(_current));
		updateMenuState();
	});

	// Encoding
	for (int id : {IDM_FORMAT_ANSI, IDM_FORMAT_AS_UTF_8, IDM_FORMAT_UTF_8, IDM_FORMAT_UTF_16BE, IDM_FORMAT_UTF_16LE})
		on(id, [this, id] { encodingSet(id); });
	for (int id = IDM_FORMAT_ENCODE; id <= IDM_FORMAT_ENCODE_END; ++id)
	{
		const int cp = EncodingMapper::getInstance().getEncodingFromIndex(id - IDM_FORMAT_ENCODE);
		if (_actions.contains(id) && cp > 0 && FileIO::isCodepageSupported(cp))
			on(id, [this, id] { encodingSet(id); });
	}
	for (int id : {IDM_FORMAT_CONV2_ANSI, IDM_FORMAT_CONV2_AS_UTF_8, IDM_FORMAT_CONV2_UTF_8, IDM_FORMAT_CONV2_UTF_16BE, IDM_FORMAT_CONV2_UTF_16LE})
		on(id, [this, id] { encodingConvert(id); });

	// Language
	for (auto it = _actions.constBegin(); it != _actions.constEnd(); ++it)
	{
		const int id = it.key();
		if (id == IDM_LANG_USER)
		{
			on(id, [this] { languageSet(L_USER, QString()); }); // User-Defined: the language of the dialog
			continue;
		}
		const LangType lang = StyleManager::instance().langFromMenuId(id);
		if (lang != L_TEXT || id == IDM_LANG_TEXT)
			on(id, [this, lang] { languageSet(lang); });
	}
	on(IDM_LANG_USER_DLG, [this] { showUdlDialog(); });
	on(IDM_LANG_OPENUDLDIR, []
	{
		// Open User Defined Language folder...
		QDir().mkpath(UdlManager::folder());
		QDesktopServices::openUrl(QUrl::fromLocalFile(UdlManager::folder()));
	});
	on(IDM_LANG_UDLCOLLECTION_PROJECT_SITE, [] { QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/notepad-plus-plus/userDefinedLanguages"))); });

	// Settings
	on(IDM_SETTING_PREFERENCE, [this] { settingsPreferences(); });
	on(IDM_LANGSTYLE_CONFIG_DLG, [this] { settingsStyleConfigurator(); });
	on(IDM_SETTING_SHORTCUT_MAPPER, [this] { settingsShortcutMapper(); });
	on(IDM_SETTING_IMPORTSTYLETHEMES, [this] { settingsImportTheme(); });
	on(IDM_SETTING_IMPORTPLUGIN, [this]
	{
		// Notepad_plus::importPlugins(): copied to the plugins folder, loaded at the next launch
		const QStringList files = QFileDialog::getOpenFileNames(this, QStringLiteral("Import plugin(s)"), QDir::homePath(),
			QStringLiteral("Notepad++ plugin (*.so)"));
		if (files.isEmpty())
			return;
		QString error;
		if (!_pluginsManager->importPlugins(files, &error))
			QMessageBox::warning(this, QStringLiteral("Import plugin(s)"), error);
		else
			QMessageBox::information(this, QStringLiteral("Import plugin(s)"),
				QStringLiteral("The plugin(s) will be loaded the next time Notepad++ starts."));
	});
	on(IDM_SETTING_OPENPLUGINSDIR, [this]
	{
		// the folder of the installed plugins, else the one of the user's plugins
		QString dir = PluginsManager::pluginsDirs().value(qEnvironmentVariableIsEmpty("NPP_PLUGINS_DIR") ? 0 : 1);
		if (!QFileInfo(dir).isDir())
		{
			dir = PluginsManager::userPluginsDir();
			QDir().mkpath(dir);
		}
		QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
	});

	// Tools
	for (int id : {IDM_TOOL_MD5_GENERATE, IDM_TOOL_MD5_GENERATEFROMFILE, IDM_TOOL_MD5_GENERATEINTOCLIPBOARD,
	               IDM_TOOL_SHA1_GENERATE, IDM_TOOL_SHA1_GENERATEFROMFILE, IDM_TOOL_SHA1_GENERATEINTOCLIPBOARD,
	               IDM_TOOL_SHA256_GENERATE, IDM_TOOL_SHA256_GENERATEFROMFILE, IDM_TOOL_SHA256_GENERATEINTOCLIPBOARD,
	               IDM_TOOL_SHA512_GENERATE, IDM_TOOL_SHA512_GENERATEFROMFILE, IDM_TOOL_SHA512_GENERATEINTOCLIPBOARD})
		on(id, [this, id] { toolHash(id); });

	// Macro
	on(IDM_MACRO_STARTRECORDINGMACRO, [this] { macroStart(); });
	on(IDM_MACRO_STOPRECORDINGMACRO, [this] { macroStop(); });
	on(IDM_MACRO_PLAYBACKRECORDEDMACRO, [this] { macroPlayback(1); });
	on(IDM_MACRO_RUNMULTIMACRODLG, [this] { macroRunMultiple(); });
	on(IDM_MACRO_SAVECURRENTMACRO, [this] { macroSave(); });
	on(IDM_SETTING_SHORTCUT_MAPPER_MACRO, [this] { macroManage(); });
	on(IDM_SETTING_SHORTCUT_MAPPER_RUN, [this] { commandsManage(); });
	on(IDM_EXECUTE_VALIDATE_SHORTCUTSXML, [] { Macros::validateShortcutsXml(); });
	on(IDM_SETTING_EDITCONTEXTMENU, [this] { editContextMenuFile(); });

	// Run
	on(IDM_EXECUTE, [this] { runDialog(); });

	// Window
	for (int id : {IDM_WINDOW_SORT_FN_ASC, IDM_WINDOW_SORT_FN_DSC, IDM_WINDOW_SORT_FP_ASC, IDM_WINDOW_SORT_FP_DSC, IDM_WINDOW_SORT_FT_ASC,
	               IDM_WINDOW_SORT_FT_DSC, IDM_WINDOW_SORT_FS_ASC, IDM_WINDOW_SORT_FS_DSC, IDM_WINDOW_SORT_FD_ASC, IDM_WINDOW_SORT_FD_DSC})
		on(id, [this, id] { windowSort(id); });
	on(IDM_WINDOW_WINDOWS, [this] { windowList(); });

	// ?
	on(IDM_CMDLINEARGUMENTS, [this] { helpCommandLine(); });
	on(IDM_HOMESWEETHOME, [] { QDesktopServices::openUrl(QUrl(QStringLiteral("https://notepad-plus-plus.org/"))); });
	on(IDM_PROJECTPAGE, [] { QDesktopServices::openUrl(QUrl(QStringLiteral("https://github.com/notepad-plus-plus/notepad-plus-plus/"))); });
	on(IDM_ONLINEDOCUMENT, [] { QDesktopServices::openUrl(QUrl(QStringLiteral("https://npp-user-manual.org/"))); });
	on(IDM_FORUM, [] { QDesktopServices::openUrl(QUrl(QStringLiteral("https://community.notepad-plus-plus.org/"))); });
	on(IDM_DEBUGINFO, [this] { helpDebugInfo(); });
	on(IDM_ABOUT, [this] { helpAbout(); });
}

bool MainWindow::isMacroRecordable(int id) const
{
	switch (id)
	{
		case IDM_MACRO_STARTRECORDINGMACRO:
		case IDM_MACRO_STOPRECORDINGMACRO:
		case IDM_MACRO_PLAYBACKRECORDEDMACRO:
		case IDM_MACRO_SAVECURRENTMACRO:
		case IDM_MACRO_RUNMULTIMACRODLG:
		case IDM_EDIT_COLUMNMODE:
		case IDM_EDIT_COLUMNMODETIP:
		case IDM_EDIT_CHANGESEARCHENGINE:
		case IDM_SEARCH_FIND:
		case IDM_SEARCH_REPLACE:
		case IDM_SEARCH_FINDINFILES:
		case IDM_SEARCH_MARK:
		case IDM_SEARCH_GOTOLINE:
		case IDM_FOCUS_ON_FOUND_RESULTS:
			return false;
		default:
			break;
	}
	return (id >= IDM_EDIT && id < IDM_EDIT + 1000) || (id >= IDM_SEARCH && id < IDM_SEARCH + 1000) ||
	       id == IDM_FORMAT_TODOS || id == IDM_FORMAT_TOUNIX || id == IDM_FORMAT_TOMAC;
}

void MainWindow::showFindDialog(int tab)
{
	if (!_findDlg)
		_findDlg = new FindReplaceDlg(this);
	_findDlg->showTab(tab);
}

void MainWindow::gotoPosition(Buffer* buf, sptr_t start, sptr_t end)
{
	activateBuffer(buf);
	const sptr_t len = _edit->ex(SCI_GETLENGTH);
	_edit->displaySection(std::min(start, len), std::min(end, len), true);
	_edit->setFocus();
}

// ---------------------------------------------------------------------------
// File
// ---------------------------------------------------------------------------
void MainWindow::fileOpen()
{
	const QStringList files = QFileDialog::getOpenFileNames(this, QStringLiteral("Open"), openSaveDir(), QStringLiteral("All types (*)"));
	if (files.isEmpty())
		return;
	rememberOpenSaveDir(files.first());
	for (const QString& f : files)
		openFile(f);
}

void MainWindow::fileSaveAll()
{
	NppPrefs& p = Npp::prefs();
	bool anyDirty = false;
	for (Buffer* b : allBuffers())
		anyDirty = anyDirty || (b->dirty && !b->isReadOnly());
	if (anyDirty && p.saveAllConfirm)
	{
		// MISC. > Enable Save All confirm dialog
		QMessageBox box(QMessageBox::Question, QStringLiteral("Save All Confirmation"),
			QStringLiteral("Are you sure you want to save all modified documents?\n\nChoose \"Always Yes\" if you don't want to see this dialog again.\n"
			               "You can re-activate this dialog in Preferences later."), QMessageBox::Yes | QMessageBox::No, this);
		QPushButton* always = box.addButton(QStringLiteral("Always Yes"), QMessageBox::AcceptRole);
		box.exec();
		if (box.clickedButton() == always)
		{
			p.saveAllConfirm = false;
			p.save();
		}
		else if (box.standardButton(box.clickedButton()) != QMessageBox::Yes)
		{
			return;
		}
	}
	for (Buffer* b : allBuffers())
	{
		if (b->dirty && !b->isReadOnly())
		{
			if (!saveBuffer(b))
				break;
		}
	}
}

void MainWindow::fileCloseAll(bool butCurrent, bool butPinned, int direction, bool unchangedOnly)
{
	// both views, except "to the left / to the right" which is about the tabs of the active view
	std::vector<std::pair<int, Buffer*>> toClose;
	const int currentIndex = indexOf(_current);
	for (int v : {otherView(), _activeView})
	{
		if (direction != 0 && v != _activeView)
			continue;
		const std::vector<Buffer*> buffers = viewBuffers(v);
		for (int i = 0; i < static_cast<int>(buffers.size()); ++i)
		{
			Buffer* b = buffers[static_cast<size_t>(i)];
			if (butCurrent && b == _current)
				continue;
			if (butPinned && b->pinned)
				continue;
			if (direction < 0 && i >= currentIndex)
				continue;
			if (direction > 0 && i <= currentIndex)
				continue;
			if (unchangedOnly && b->dirty)
				continue;
			toClose.emplace_back(v, b);
		}
	}

	// ask for the modified documents that really close (Yes / No / Cancel / Yes to all / No to all)
	auto fullyClosed = [&](Buffer* b)
	{
		int tabsOfB = 0;
		int closing = 0;
		for (int v = MAIN_VIEW; v <= SUB_VIEW; ++v)
			tabsOfB += indexOf(v, b) >= 0 ? 1 : 0;
		for (const auto& pr : toClose)
			closing += pr.second == b ? 1 : 0;
		return closing == tabsOfB;
	};
	bool yesToAll = false;
	bool noToAll = false;
	std::vector<Buffer*> asked;
	for (const auto& [v, b] : toClose)
	{
		if (!b->dirty || noToAll || !fullyClosed(b) || std::find(asked.begin(), asked.end(), b) != asked.end())
			continue;
		asked.push_back(b);
		if (!yesToAll)
		{
			activateBuffer(b, v);
			const auto answer = QMessageBox::question(this, QStringLiteral("Save"), QStringLiteral("Save file \"%1\" ?").arg(b->fullName()),
				QMessageBox::Yes | QMessageBox::YesToAll | QMessageBox::No | QMessageBox::NoToAll | QMessageBox::Cancel, QMessageBox::Yes);
			if (answer == QMessageBox::Cancel)
				return;
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
			return;
	}
	for (const auto& [v, b] : toClose)
		closeBuffer(b, false, v);
}

void MainWindow::fileRename()
{
	if (!_current)
		return;
	Buffer* b = _current;
	bool ok = false;
	if (b->isUntitled())
	{
		const QString name = QInputDialog::getText(this, QStringLiteral("Rename"), QStringLiteral("New name:"), QLineEdit::Normal, b->untitledName, &ok);
		if (ok && !name.trimmed().isEmpty())
		{
			b->untitledName = name.trimmed();
			b->untitledTabRenamed = true;
			updateTabLabel(indexOf(b));
			updateTitle();
		}
		return;
	}
	const QFileInfo fi(b->path);
	const QString name = QInputDialog::getText(this, QStringLiteral("Rename"), QStringLiteral("New name:"), QLineEdit::Normal, fi.fileName(), &ok);
	if (!ok || name.isEmpty() || name == fi.fileName())
		return;
	const QString newPath = fi.absoluteDir().absoluteFilePath(name);
	if (QFileInfo::exists(newPath))
	{
		QMessageBox::warning(this, QStringLiteral("Rename"), QStringLiteral("\"%1\" already exists.").arg(newPath));
		return;
	}
	_pluginsManager->notifyBuffer(NPPN_FILEBEFORERENAME, b);
	unwatch(b);
	if (!QFile::rename(b->path, newPath))
	{
		watch(b);
		_pluginsManager->notifyBuffer(NPPN_FILERENAMECANCEL, b);
		QMessageBox::warning(this, QStringLiteral("Rename"), QStringLiteral("Cannot rename \"%1\".").arg(b->path));
		return;
	}
	b->path = newPath;
	watch(b);
	addRecent(newPath);
	updateTabLabel(indexOf(b));
	updateTitle();
	refreshDocumentList();
	_pluginsManager->notifyBuffer(NPPN_FILERENAMED, b);
}

void MainWindow::fileDelete()
{
	if (!_current || _current->isUntitled())
		return;
	Buffer* b = _current;
	const auto answer = QMessageBox::question(this, QStringLiteral("Delete file"),
		QStringLiteral("Are you sure you want to move \"%1\" to the Trash?").arg(b->path));
	if (answer != QMessageBox::Yes)
		return;
	_pluginsManager->notifyBuffer(NPPN_FILEBEFOREDELETE, b);
	unwatch(b);
	if (!QFile::moveToTrash(b->path))
	{
		watch(b);
		_pluginsManager->notifyBuffer(NPPN_FILEDELETEFAILED, b);
		QMessageBox::warning(this, QStringLiteral("Delete file"), QStringLiteral("Cannot move \"%1\" to the Trash.").arg(b->path));
		return;
	}
	_pluginsManager->notifyBuffer(NPPN_FILEDELETED, b);
	closeBuffer(b, false);
}

static QString sessionFilter()
{
	// MISC. > Session file ext.
	const QString ext = Npp::prefs().sessionFileExt.section(QLatin1Char('.'), -1).trimmed();
	if (!ext.isEmpty())
		return QStringLiteral("Session file (*.%1);;All types (*)").arg(ext);
	return QStringLiteral("Session file (*.xml *.session);;All types (*)");
}

void MainWindow::fileLoadSession()
{
	const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Load Session"), Npp::localConfigDir(), sessionFilter());
	if (path.isEmpty())
		return;
	if (!readSession(path))
		QMessageBox::warning(this, QStringLiteral("Load Session"), QStringLiteral("Cannot load the session \"%1\".").arg(path));
}

void MainWindow::fileSaveSession()
{
	const QString ext = Npp::prefs().sessionFileExt.section(QLatin1Char('.'), -1).trimmed();
	const QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Save Session"),
		Npp::localConfigDir() + (ext.isEmpty() ? QStringLiteral("/session.xml") : QStringLiteral("/session.") + ext), sessionFilter());
	if (!path.isEmpty())
		writeSession(path, false);
}

void MainWindow::filePrint(bool showDialog)
{
	// Qt counterpart of ScintillaComponent/Printer.cpp with Preferences > Print
	if (!_current)
		return;
	const NppPrefs& p = Npp::prefs();
	QPrinter printer(QPrinter::HighResolution);
	printer.setDocName(_current->displayName());
	if (p.printMarginTop || p.printMarginBottom || p.printMarginLeft || p.printMarginRight)
		printer.setPageMargins(QMarginsF(p.printMarginLeft, p.printMarginTop, p.printMarginRight, p.printMarginBottom), QPageLayout::Millimeter);
	if (showDialog)
	{
		QPrintDialog dlg(&printer, this);
		if (!_edit->ex(SCI_GETSELECTIONEMPTY))
			dlg.setOption(QAbstractPrintDialog::PrintSelection, true);
		if (dlg.exec() != QDialog::Accepted)
			return;
	}

	QPainter painter;
	if (!painter.begin(&printer))
	{
		QMessageBox::warning(this, QStringLiteral("Print"), QStringLiteral("Cannot print the document."));
		return;
	}

	const QRect page = printer.pageLayout().paintRectPixels(printer.resolution());
	const QRect paper = printer.pageLayout().fullRectPixels(printer.resolution());

	// header and footer
	auto makeFont = [](const QString& name, int size, bool bold, bool italic)
	{
		QFont f(name.isEmpty() ? QApplication::font().family() : name);
		f.setPointSizeF(size > 0 ? size : 9);
		f.setBold(bold);
		f.setItalic(italic);
		return f;
	};
	const QFont headerFont = makeFont(p.printHeaderFont, p.printHeaderSize, p.printHeaderBold, p.printHeaderItalic);
	const QFont footerFont = makeFont(p.printFooterFont, p.printFooterSize, p.printFooterBold, p.printFooterItalic);
	const bool hasHeader = !(p.printHeaderLeft.isEmpty() && p.printHeaderMiddle.isEmpty() && p.printHeaderRight.isEmpty());
	const bool hasFooter = !(p.printFooterLeft.isEmpty() && p.printFooterMiddle.isEmpty() && p.printFooterRight.isEmpty());
	const int headerH = hasHeader ? QFontMetrics(headerFont, &printer).height() * 3 / 2 : 0;
	const int footerH = hasFooter ? QFontMetrics(footerFont, &printer).height() * 3 / 2 : 0;
	const QDateTime now = QDateTime::currentDateTime();
	auto expand = [&](QString text, int pageNo)
	{
		text.replace(QStringLiteral("$(FULL_CURRENT_PATH)"), _current->fullName());
		text.replace(QStringLiteral("$(FILE_NAME)"), _current->displayName());
		text.replace(QStringLiteral("$(CURRENT_DIRECTORY)"), _current->isUntitled() ? QString() : QFileInfo(_current->path).absolutePath());
		text.replace(QStringLiteral("$(CURRENT_PRINTING_PAGE)"), QString::number(pageNo));
		text.replace(QStringLiteral("$(SHORT_DATE)"), QLocale::system().toString(now.date(), QLocale::ShortFormat));
		text.replace(QStringLiteral("$(LONG_DATE)"), QLocale::system().toString(now.date(), QLocale::LongFormat));
		text.replace(QStringLiteral("$(TIME)"), QLocale::system().toString(now.time(), QLocale::ShortFormat));
		return text;
	};
	auto drawBand = [&](const QFont& font, const QRect& band, const QString& l, const QString& m, const QString& r, int pageNo, bool header)
	{
		painter.save();
		painter.setFont(font);
		painter.setPen(Qt::black);
		const QRect textRect = band.adjusted(0, 0, 0, header ? -band.height() / 4 : 0).adjusted(0, header ? 0 : band.height() / 4, 0, 0);
		painter.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter, expand(l, pageNo));
		painter.drawText(textRect, Qt::AlignHCenter | Qt::AlignVCenter, expand(m, pageNo));
		painter.drawText(textRect, Qt::AlignRight | Qt::AlignVCenter, expand(r, pageNo));
		const int y = header ? band.bottom() - 1 : band.top() + 1;
		painter.drawLine(band.left(), y, band.right(), y);
		painter.restore();
	};

	Sci_RangeToFormatFull frPrint;
	frPrint.hdc = &painter;
	frPrint.hdcTarget = &painter;
	frPrint.rc.left = 0;
	frPrint.rc.top = headerH;
	frPrint.rc.right = page.width();
	frPrint.rc.bottom = page.height() - footerH;
	frPrint.rcPage.left = 0;
	frPrint.rcPage.top = 0;
	frPrint.rcPage.right = paper.width();
	frPrint.rcPage.bottom = paper.height();

	sptr_t start = 0;
	sptr_t end = _edit->ex(SCI_GETLENGTH);
	if (printer.printRange() == QPrinter::Selection)
	{
		start = _edit->ex(SCI_GETSELECTIONSTART);
		end = _edit->ex(SCI_GETSELECTIONEND);
	}

	// Print form feed as page break: one range per form feed separated part
	std::vector<std::pair<sptr_t, sptr_t>> ranges;
	if (p.printFormFeed)
	{
		sptr_t from = start;
		for (sptr_t pos = start; pos < end; ++pos)
		{
			if (_edit->ex(SCI_GETCHARAT, pos) == '\f')
			{
				ranges.emplace_back(from, pos);
				from = pos + 1;
			}
		}
		ranges.emplace_back(from, end);
	}
	else
	{
		ranges.emplace_back(start, end);
	}

	const sptr_t lineNumberWidth = _edit->ex(SCI_GETMARGINWIDTHN, MARGIN_LINENUMBER);
	if (!p.printLineNumber)
		_edit->ex(SCI_SETMARGINWIDTHN, MARGIN_LINENUMBER, 0);
	_edit->ex(SCI_SETPRINTCOLOURMODE, p.printColorMode);
	_edit->ex(SCI_SETPRINTWRAPMODE, SC_WRAP_WORD);

	const bool pageRange = printer.printRange() == QPrinter::PageRange;
	const int fromPage = pageRange ? printer.fromPage() : 1;
	const int toPage = pageRange ? printer.toPage() : INT_MAX;
	int pageNo = 1;
	bool printedAny = false;
	for (const auto& range : ranges)
	{
		sptr_t pos = range.first;
		do
		{
			const bool draw = (pageNo >= fromPage && pageNo <= toPage);
			if (draw)
			{
				if (printedAny)
					printer.newPage();
				printedAny = true;
				if (hasHeader)
					drawBand(headerFont, QRect(0, 0, page.width(), headerH), p.printHeaderLeft, p.printHeaderMiddle, p.printHeaderRight, pageNo, true);
				if (hasFooter)
					drawBand(footerFont, QRect(0, page.height() - footerH, page.width(), footerH), p.printFooterLeft, p.printFooterMiddle, p.printFooterRight, pageNo, false);
			}
			frPrint.chrg.cpMin = pos;
			frPrint.chrg.cpMax = range.second;
			const sptr_t next = (range.second > pos) ? _edit->ex(SCI_FORMATRANGEFULL, draw, reinterpret_cast<sptr_t>(&frPrint)) : range.second;
			++pageNo;
			if (next <= pos)
				break;
			pos = next;
		} while (pos < range.second && pageNo <= toPage);
		if (pageNo > toPage)
			break;
	}
	_edit->ex(SCI_FORMATRANGEFULL, false, 0);
	if (!p.printLineNumber)
		_edit->ex(SCI_SETMARGINWIDTHN, MARGIN_LINENUMBER, lineNumberWidth);
	painter.end();
}

void MainWindow::openContainingFolder()
{
	if (!_current || _current->isUntitled())
		return;
	const QString dbus = QStandardPaths::findExecutable(QStringLiteral("dbus-send"));
	if (!dbus.isEmpty())
	{
		const QString uri = QUrl::fromLocalFile(_current->path).toString();
		const bool started = QProcess::startDetached(dbus, {QStringLiteral("--session"), QStringLiteral("--dest=org.freedesktop.FileManager1"),
			QStringLiteral("--type=method_call"), QStringLiteral("/org/freedesktop/FileManager1"),
			QStringLiteral("org.freedesktop.FileManager1.ShowItems"), QStringLiteral("array:string:") + uri, QStringLiteral("string:")});
		if (started)
			return;
	}
	QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(_current->path).absolutePath()));
}

void MainWindow::openTerminal(bool powershell)
{
	const QString dir = (_current && !_current->isUntitled()) ? QFileInfo(_current->path).absolutePath() : QDir::homePath();
	const QString term = findTerminal();
	if (term.isEmpty())
	{
		QMessageBox::warning(this, QStringLiteral("Terminal"), QStringLiteral("No terminal emulator found. Set the TERMINAL environment variable."));
		return;
	}
	QStringList args;
	const QString name = QFileInfo(term).fileName();
	if (name == QStringLiteral("konsole"))
		args << QStringLiteral("--workdir") << dir;
	else if (name == QStringLiteral("gnome-terminal") || name == QStringLiteral("kgx") || name == QStringLiteral("ptyxis") ||
	         name == QStringLiteral("xfce4-terminal") || name == QStringLiteral("mate-terminal") || name == QStringLiteral("tilix"))
		args << QStringLiteral("--working-directory=") + dir;
	if (powershell)
	{
		if (name == QStringLiteral("gnome-terminal") || name == QStringLiteral("kgx") || name == QStringLiteral("ptyxis"))
			args << QStringLiteral("--") << QStringLiteral("pwsh");
		else if (name == QStringLiteral("kitty"))
			args << QStringLiteral("pwsh");
		else
			args << QStringLiteral("-e") << QStringLiteral("pwsh");
	}
	QProcess::startDetached(term, args, dir);
}

// ---------------------------------------------------------------------------
// Edit
// ---------------------------------------------------------------------------
void MainWindow::editInsertDateTime(int id)
{
	const QDateTime now = QDateTime::currentDateTime();
	const QLocale loc = QLocale::system();
	const NppPrefs& p = Npp::prefs();
	QString text;
	if (id == IDM_EDIT_INSERT_DATETIME_SHORT || id == IDM_EDIT_INSERT_DATETIME_LONG)
	{
		const QString date = loc.toString(now.date(), id == IDM_EDIT_INSERT_DATETIME_SHORT ? QLocale::ShortFormat : QLocale::LongFormat);
		const QString time = loc.toString(now.time(), QLocale::ShortFormat);
		// Multi-Instance & Date > Reverse default date time order: date + time
		text = p.dateTimeReverse ? date + QLatin1Char(' ') + time : time + QLatin1Char(' ') + date;
	}
	else
	{
		text = now.toString(Npp::windowsDateTimeFormat(p.dateTimeFormat));
	}
	const QByteArray utf8 = text.toUtf8();
	_edit->exs(SCI_REPLACESEL, 0, utf8.constData());
}

void MainWindow::editCopyToClipboard(int id)
{
	if (!_current)
		return;
	QString text;
	switch (id)
	{
		case IDM_EDIT_FULLPATHTOCLIP:
			text = _current->fullName();
			break;
		case IDM_EDIT_FILENAMETOCLIP:
			text = _current->displayName();
			break;
		case IDM_EDIT_CURRENTDIRTOCLIP:
			text = _current->isUntitled() ? QString() : QFileInfo(_current->path).absolutePath();
			break;
		case IDM_EDIT_COPY_ALL_NAMES:
		case IDM_EDIT_COPY_ALL_PATHS:
		{
			QStringList l;
			for (Buffer* b : allBuffers())
				l << (id == IDM_EDIT_COPY_ALL_NAMES ? b->displayName() : b->fullName());
			text = l.join(QLatin1Char('\n'));
			break;
		}
		default:
			break;
	}
	QApplication::clipboard()->setText(text);
}

void MainWindow::editConvertCase(int id)
{
	if (_edit->ex(SCI_GETSELECTIONEMPTY))
		return;
	if (id == IDM_EDIT_UPPERCASE && _edit->ex(SCI_GETSELECTIONS) == 1)
	{
		_edit->ex(SCI_UPPERCASE);
		return;
	}
	if (id == IDM_EDIT_LOWERCASE && _edit->ex(SCI_GETSELECTIONS) == 1)
	{
		_edit->ex(SCI_LOWERCASE);
		return;
	}

	auto convert = [id](const QString& in)
	{
		QString s = in;
		switch (id)
		{
			case IDM_EDIT_UPPERCASE:
				return s.toUpper();
			case IDM_EDIT_LOWERCASE:
				return s.toLower();
			case IDM_EDIT_PROPERCASE_FORCE:
			case IDM_EDIT_PROPERCASE_BLEND:
			{
				bool wordStart = true;
				for (int i = 0; i < s.size(); ++i)
				{
					const QChar c = s[i];
					if (c.isLetter())
					{
						if (wordStart)
							s[i] = c.toUpper();
						else if (id == IDM_EDIT_PROPERCASE_FORCE)
							s[i] = c.toLower();
						wordStart = false;
					}
					else
					{
						// an apostrophe inside a word does not start a new word ("don't")
						wordStart = !(c.isNumber() || (c == QLatin1Char('\'') && i > 0 && s[i - 1].isLetter()));
					}
				}
				return s;
			}
			case IDM_EDIT_SENTENCECASE_FORCE:
			case IDM_EDIT_SENTENCECASE_BLEND:
			{
				bool sentenceStart = true;
				bool afterTerminator = false;
				for (int i = 0; i < s.size(); ++i)
				{
					const QChar c = s[i];
					if (c.isLetter())
					{
						if (sentenceStart)
							s[i] = c.toUpper();
						else if (id == IDM_EDIT_SENTENCECASE_FORCE)
							s[i] = c.toLower();
						sentenceStart = false;
						afterTerminator = false;
					}
					else if (c == QLatin1Char('.') || c == QLatin1Char('!') || c == QLatin1Char('?'))
					{
						afterTerminator = true;
					}
					else if (c.isSpace())
					{
						if (afterTerminator || c == QLatin1Char('\n') || c == QLatin1Char('\r'))
							sentenceStart = sentenceStart || afterTerminator;
					}
					else
					{
						afterTerminator = false;
					}
				}
				return s;
			}
			case IDM_EDIT_INVERTCASE:
				for (QChar& c : s)
					c = c.isUpper() ? c.toLower() : c.toUpper();
				return s;
			case IDM_EDIT_RANDOMCASE:
				for (QChar& c : s)
					c = (QRandomGenerator::global()->bounded(2) == 0) ? c.toLower() : c.toUpper();
				return s;
			default:
				return s;
		}
	};

	_edit->ex(SCI_BEGINUNDOACTION);
	const sptr_t n = _edit->ex(SCI_GETSELECTIONS);
	for (sptr_t i = 0; i < n; ++i)
	{
		const sptr_t s = _edit->ex(SCI_GETSELECTIONNSTART, i);
		const sptr_t e = _edit->ex(SCI_GETSELECTIONNEND, i);
		if (e <= s)
			continue;
		const QByteArray newText = convert(_edit->textRangeQ(s, e)).toUtf8();
		_edit->ex(SCI_SETTARGETRANGE, s, e);
		_edit->ex(SCI_REPLACETARGETMINIMAL, newText.size(), reinterpret_cast<sptr_t>(newText.constData()));
	}
	_edit->ex(SCI_ENDUNDOACTION);
}

void MainWindow::editLineOperation(int id)
{
	EditView* v = _edit;
	switch (id)
	{
		case IDM_EDIT_DUP_LINE:
			v->ex(v->ex(SCI_GETSELECTIONEMPTY) ? SCI_LINEDUPLICATE : SCI_SELECTIONDUPLICATE);
			return;
		case IDM_EDIT_TRANSPOSE_LINE:
			v->ex(SCI_LINETRANSPOSE);
			return;
		case IDM_EDIT_LINE_UP:
			v->ex(SCI_MOVESELECTEDLINESUP);
			return;
		case IDM_EDIT_LINE_DOWN:
			v->ex(SCI_MOVESELECTEDLINESDOWN);
			return;
		case IDM_EDIT_SPLIT_LINES:
			if (v->ex(SCI_GETSELECTIONEMPTY))
				return;
			v->ex(SCI_TARGETFROMSELECTION);
			v->ex(SCI_LINESSPLIT, 0);
			return;
		case IDM_EDIT_JOIN_LINES:
		{
			const auto range = v->selectionLinesRange();
			if (range.second <= range.first)
				return;
			v->ex(SCI_SETTARGETRANGE, v->ex(SCI_POSITIONFROMLINE, range.first), v->ex(SCI_GETLINEENDPOSITION, range.second));
			v->ex(SCI_LINESJOIN);
			return;
		}
		case IDM_EDIT_BLANKLINEABOVECURRENT:
		case IDM_EDIT_BLANKLINEBELOWCURRENT:
		{
			const sptr_t line = v->ex(SCI_LINEFROMPOSITION, v->ex(SCI_GETCURRENTPOS));
			const QByteArray eol = v->eolString();
			if (id == IDM_EDIT_BLANKLINEABOVECURRENT)
			{
				const sptr_t pos = v->ex(SCI_POSITIONFROMLINE, line);
				v->ex(SCI_INSERTTEXT, pos, reinterpret_cast<sptr_t>(eol.constData()));
				v->ex(SCI_GOTOPOS, pos);
			}
			else
			{
				const sptr_t pos = v->ex(SCI_GETLINEENDPOSITION, line);
				v->ex(SCI_INSERTTEXT, pos, reinterpret_cast<sptr_t>(eol.constData()));
				v->ex(SCI_GOTOPOS, pos + eol.size());
			}
			return;
		}
		default:
			break;
	}

	// operations on whole lines of the selection / document
	const auto range = operationLines(v, false);
	QList<QByteArray> lines;
	for (sptr_t l = range.first; l <= range.second; ++l)
		lines << v->lineText(l);

	QList<QByteArray> result;
	if (id == IDM_EDIT_REMOVE_ANY_DUP_LINES)
	{
		QSet<QByteArray> seen;
		for (const QByteArray& l : lines)
		{
			if (!seen.contains(l))
			{
				seen.insert(l);
				result << l;
			}
		}
	}
	else if (id == IDM_EDIT_REMOVE_CONSECUTIVE_DUP_LINES)
	{
		for (const QByteArray& l : lines)
		{
			if (result.isEmpty() || result.last() != l)
				result << l;
		}
	}
	else if (id == IDM_EDIT_REMOVEEMPTYLINES || id == IDM_EDIT_REMOVEEMPTYLINESWITHBLANK)
	{
		for (const QByteArray& l : lines)
		{
			const bool empty = (id == IDM_EDIT_REMOVEEMPTYLINES) ? l.isEmpty() : l.trimmed().isEmpty();
			if (!empty)
				result << l;
		}
		// keep the final line end of the document
		if (range.second == v->ex(SCI_GETLINECOUNT) - 1 && lines.last().isEmpty() && !result.isEmpty())
			result << QByteArray();
	}
	if (result == lines)
		return;

	v->ex(SCI_BEGINUNDOACTION);
	if (result.isEmpty())
	{
		// remove the whole range including its line ends
		const sptr_t start = v->ex(SCI_POSITIONFROMLINE, range.first);
		const sptr_t end = (range.second + 1 < v->ex(SCI_GETLINECOUNT)) ? v->ex(SCI_POSITIONFROMLINE, range.second + 1) : v->ex(SCI_GETLENGTH);
		v->ex(SCI_DELETERANGE, start, end - start);
	}
	else
	{
		replaceLines(v, range.first, range.second, result);
	}
	v->ex(SCI_ENDUNDOACTION);
}

void MainWindow::editSortLines(int id)
{
	// Port of Notepad_plus::sortLines(): selected lines, or the whole document
	EditView* v = _edit;
	auto range = operationLines(v, true);
	const sptr_t lineCount = v->ex(SCI_GETLINECOUNT);
	if (range.second == lineCount - 1 && range.second > range.first && v->lineLength(range.second) == 0)
		--range.second; // don't sort the empty last line
	if (range.second <= range.first)
		return;

	// column mode: sort on the selected columns
	const bool columnMode = v->ex(SCI_SELECTIONISRECTANGLE) != 0;
	sptr_t fromCol = 0;
	sptr_t toCol = 0;
	if (columnMode)
	{
		const sptr_t c1 = v->ex(SCI_GETCOLUMN, v->ex(SCI_GETRECTANGULARSELECTIONANCHOR)) + v->ex(SCI_GETRECTANGULARSELECTIONANCHORVIRTUALSPACE);
		const sptr_t c2 = v->ex(SCI_GETCOLUMN, v->ex(SCI_GETRECTANGULARSELECTIONCARET)) + v->ex(SCI_GETRECTANGULARSELECTIONCARETVIRTUALSPACE);
		fromCol = std::min(c1, c2);
		toCol = std::max(c1, c2);
	}

	struct Entry
	{
		QByteArray line;
		QString key;
		int index;
	};
	std::vector<Entry> entries;
	for (sptr_t l = range.first; l <= range.second; ++l)
	{
		Entry e;
		e.line = v->lineText(l);
		if (columnMode)
		{
			const sptr_t s = v->ex(SCI_FINDCOLUMN, l, fromCol);
			const sptr_t t = v->ex(SCI_FINDCOLUMN, l, toCol);
			e.key = v->textRangeQ(s, t);
		}
		else
		{
			e.key = QString::fromUtf8(e.line);
		}
		e.index = static_cast<int>(entries.size());
		entries.push_back(std::move(e));
	}

	const bool descending = (id == IDM_EDIT_SORTLINES_LEXICOGRAPHIC_DESCENDING || id == IDM_EDIT_SORTLINES_LEXICO_CASE_INSENS_DESCENDING ||
	                         id == IDM_EDIT_SORTLINES_LOCALE_DESCENDING || id == IDM_EDIT_SORTLINES_INTEGER_DESCENDING ||
	                         id == IDM_EDIT_SORTLINES_DECIMALCOMMA_DESCENDING || id == IDM_EDIT_SORTLINES_DECIMALDOT_DESCENDING ||
	                         id == IDM_EDIT_SORTLINES_LENGTH_DESCENDING);

	if (id == IDM_EDIT_SORTLINES_RANDOMLY)
	{
		std::shuffle(entries.begin(), entries.end(), std::mt19937(std::random_device{}()));
	}
	else if (id == IDM_EDIT_SORTLINES_REVERSE_ORDER)
	{
		std::reverse(entries.begin(), entries.end());
	}
	else if (id == IDM_EDIT_SORTLINES_INTEGER_ASCENDING || id == IDM_EDIT_SORTLINES_INTEGER_DESCENDING ||
	         id == IDM_EDIT_SORTLINES_DECIMALCOMMA_ASCENDING || id == IDM_EDIT_SORTLINES_DECIMALCOMMA_DESCENDING ||
	         id == IDM_EDIT_SORTLINES_DECIMALDOT_ASCENDING || id == IDM_EDIT_SORTLINES_DECIMALDOT_DESCENDING)
	{
		const bool integer = (id == IDM_EDIT_SORTLINES_INTEGER_ASCENDING || id == IDM_EDIT_SORTLINES_INTEGER_DESCENDING);
		const bool comma = (id == IDM_EDIT_SORTLINES_DECIMALCOMMA_ASCENDING || id == IDM_EDIT_SORTLINES_DECIMALCOMMA_DESCENDING);
		const QRegularExpression re(integer ? QStringLiteral("^\\s*([+-]?\\d+)")
		                            : comma ? QStringLiteral("^\\s*([+-]?(?:\\d+(?:,\\d*)?|,\\d+)(?:[eE][+-]?\\d+)?)")
		                                    : QStringLiteral("^\\s*([+-]?(?:\\d+(?:\\.\\d*)?|\\.\\d+)(?:[eE][+-]?\\d+)?)"));
		std::vector<std::pair<double, Entry>> numeric;
		std::vector<Entry> others;
		for (Entry& e : entries)
		{
			const QRegularExpressionMatch m = re.match(e.key);
			if (m.hasMatch())
			{
				QString num = m.captured(1);
				if (comma)
					num.replace(QLatin1Char(','), QLatin1Char('.'));
				numeric.emplace_back(num.toDouble(), std::move(e));
			}
			else
				others.push_back(std::move(e));
		}
		std::stable_sort(numeric.begin(), numeric.end(), [descending](const auto& a, const auto& b)
		{
			return descending ? a.first > b.first : a.first < b.first;
		});
		entries.clear();
		if (!descending)
			entries.insert(entries.end(), others.begin(), others.end());
		for (auto& n : numeric)
			entries.push_back(std::move(n.second));
		if (descending)
			entries.insert(entries.end(), others.begin(), others.end());
	}
	else
	{
		QCollator collator;
		collator.setNumericMode(false);
		auto less = [&](const Entry& a, const Entry& b) -> bool
		{
			int c = 0;
			switch (id)
			{
				case IDM_EDIT_SORTLINES_LEXICO_CASE_INSENS_ASCENDING:
				case IDM_EDIT_SORTLINES_LEXICO_CASE_INSENS_DESCENDING:
					c = a.key.compare(b.key, Qt::CaseInsensitive);
					break;
				case IDM_EDIT_SORTLINES_LOCALE_ASCENDING:
				case IDM_EDIT_SORTLINES_LOCALE_DESCENDING:
					c = collator.compare(a.key, b.key);
					break;
				case IDM_EDIT_SORTLINES_LENGTH_ASCENDING:
				case IDM_EDIT_SORTLINES_LENGTH_DESCENDING:
					c = (a.key.size() < b.key.size()) ? -1 : (a.key.size() > b.key.size()) ? 1 : 0;
					break;
				default:
					c = a.key.compare(b.key, Qt::CaseSensitive);
					break;
			}
			return descending ? c > 0 : c < 0;
		};
		std::stable_sort(entries.begin(), entries.end(), less);
	}

	QList<QByteArray> sorted;
	for (const Entry& e : entries)
		sorted << e.line;
	v->ex(SCI_BEGINUNDOACTION);
	replaceLines(v, range.first, range.second, sorted);
	v->ex(SCI_ENDUNDOACTION);
	v->ex(SCI_SETSEL, v->ex(SCI_POSITIONFROMLINE, range.first), v->ex(SCI_GETLINEENDPOSITION, range.second));
}

void MainWindow::editComment(int id)
{
	if (!_current)
		return;
	const LangDef* ld = StyleManager::instance().langDef(_current->lang);
	if (!ld)
		return;
	EditView* v = _edit;
	const QByteArray lineSymbol = ld->commentLine.toUtf8();
	const QByteArray streamStart = ld->commentStart.toUtf8();
	const QByteArray streamEnd = ld->commentEnd.toUtf8();

	const bool streamCommand = (id == IDM_EDIT_STREAM_COMMENT || id == IDM_EDIT_STREAM_UNCOMMENT);
	const bool useStream = streamCommand || (lineSymbol.isEmpty() && !streamStart.isEmpty() && !streamEnd.isEmpty());

	if (!useStream)
	{
		// Port of Notepad_plus::doBlockComment()
		if (lineSymbol.isEmpty())
			return;
		const QByteArray comment = lineSymbol + ' ';
		const sptr_t selStart = v->ex(SCI_GETSELECTIONSTART);
		const sptr_t selEnd = v->ex(SCI_GETSELECTIONEND);
		const bool hadSelection = selStart != selEnd;
		const sptr_t caretLine = v->ex(SCI_LINEFROMPOSITION, v->ex(SCI_GETCURRENTPOS));
		const sptr_t caretCol = v->ex(SCI_GETCURRENTPOS) - v->ex(SCI_POSITIONFROMLINE, caretLine);
		const auto range = v->selectionLinesRange();
		sptr_t caretDelta = 0;

		v->ex(SCI_BEGINUNDOACTION);
		for (sptr_t i = range.first; i <= range.second; ++i)
		{
			const sptr_t lineIndent = v->ex(SCI_GETLINEINDENTPOSITION, i);
			const sptr_t lineEnd = v->ex(SCI_GETLINEENDPOSITION, i);
			if (lineIndent == lineEnd && hadSelection)
				continue; // empty lines are left alone
			const QByteArray lineBuf = v->textRange(lineIndent, lineEnd);
			if (id != IDM_EDIT_BLOCK_COMMENT_SET && lineBuf.startsWith(lineSymbol))
			{
				sptr_t len = lineSymbol.size();
				if (lineBuf.size() > len && lineBuf[static_cast<qsizetype>(len)] == ' ')
					++len;
				v->ex(SCI_DELETERANGE, lineIndent, len);
				if (i == caretLine)
					caretDelta = -len;
				continue;
			}
			if (id == IDM_EDIT_BLOCK_UNCOMMENT)
				continue;
			v->ex(SCI_INSERTTEXT, lineIndent, reinterpret_cast<sptr_t>(comment.constData()));
			if (i == caretLine)
				caretDelta = comment.size();
		}
		if (hadSelection)
			v->ex(SCI_SETSEL, v->ex(SCI_POSITIONFROMLINE, range.first), v->ex(SCI_GETLINEENDPOSITION, range.second));
		else
			v->ex(SCI_GOTOPOS, v->ex(SCI_POSITIONFROMLINE, caretLine) + std::max<sptr_t>(0, caretCol + caretDelta));
		v->ex(SCI_ENDUNDOACTION);
		return;
	}

	if (streamStart.isEmpty() || streamEnd.isEmpty())
		return;

	const QByteArray startMark = streamStart + ' ';
	const QByteArray endMark = ' ' + streamEnd;
	sptr_t selStart = v->ex(SCI_GETSELECTIONSTART);
	sptr_t selEnd = v->ex(SCI_GETSELECTIONEND);

	if (id == IDM_EDIT_STREAM_UNCOMMENT)
	{
		// find the comment around the selection / caret
		v->ex(SCI_SETSEARCHFLAGS, 0);
		v->ex(SCI_SETTARGETRANGE, selStart, 0);
		const sptr_t openPos = v->ex(SCI_SEARCHINTARGET, streamStart.size(), reinterpret_cast<sptr_t>(streamStart.constData()));
		if (openPos < 0)
			return;
		v->ex(SCI_SETTARGETRANGE, std::max(selEnd, openPos + static_cast<sptr_t>(streamStart.size())), v->ex(SCI_GETLENGTH));
		const sptr_t closePos = v->ex(SCI_SEARCHINTARGET, streamEnd.size(), reinterpret_cast<sptr_t>(streamEnd.constData()));
		if (closePos < 0)
			return;
		v->ex(SCI_BEGINUNDOACTION);
		sptr_t closeStart = closePos;
		sptr_t closeLen = streamEnd.size();
		if (closeStart > 0 && v->ex(SCI_GETCHARAT, closeStart - 1) == ' ')
		{
			--closeStart;
			++closeLen;
		}
		v->ex(SCI_DELETERANGE, closeStart, closeLen);
		sptr_t openLen = streamStart.size();
		if (v->ex(SCI_GETCHARAT, openPos + openLen) == ' ')
			++openLen;
		v->ex(SCI_DELETERANGE, openPos, openLen);
		v->ex(SCI_ENDUNDOACTION);
		return;
	}

	if (selStart == selEnd)
	{
		const sptr_t pos = v->ex(SCI_GETCURRENTPOS);
		selStart = v->ex(SCI_WORDSTARTPOSITION, pos, true);
		selEnd = v->ex(SCI_WORDENDPOSITION, pos, true);
		if (selStart == selEnd)
			return;
	}
	v->ex(SCI_BEGINUNDOACTION);
	v->ex(SCI_INSERTTEXT, selEnd, reinterpret_cast<sptr_t>(endMark.constData()));
	v->ex(SCI_INSERTTEXT, selStart, reinterpret_cast<sptr_t>(startMark.constData()));
	v->ex(SCI_SETSEL, selStart, selEnd + startMark.size() + endMark.size());
	v->ex(SCI_ENDUNDOACTION);
}

void MainWindow::editEol(int eolMode)
{
	if (!_current || _current->isReadOnly())
		return;
	_edit->ex(SCI_CONVERTEOLS, eolMode);
	_edit->ex(SCI_SETEOLMODE, eolMode);
	_current->eol = eolMode;
	updateStatusBar();
	updateMenuState();
}

void MainWindow::editBlankOperation(int id)
{
	EditView* v = _edit;
	const auto range = operationLines(v, false);
	const int tabWidth = static_cast<int>(v->ex(SCI_GETTABWIDTH));

	auto expandTabs = [tabWidth](const QString& line)
	{
		QString out;
		int col = 0;
		for (const QChar c : line)
		{
			if (c == QLatin1Char('\t'))
			{
				const int n = tabWidth - (col % tabWidth);
				out += QString(n, QLatin1Char(' '));
				col += n;
			}
			else
			{
				out += c;
				++col;
			}
		}
		return out;
	};

	auto spacesToTabs = [tabWidth, &expandTabs](const QString& line, bool leadingOnly)
	{
		const QString expanded = expandTabs(line);
		int limit = static_cast<int>(expanded.size());
		if (leadingOnly)
		{
			limit = 0;
			while (limit < expanded.size() && expanded[limit] == QLatin1Char(' '))
				++limit;
		}
		QString out;
		int col = 0;
		int i = 0;
		while (i < expanded.size())
		{
			if (i < limit && expanded[i] == QLatin1Char(' '))
			{
				int j = i;
				while (j < limit && expanded[j] == QLatin1Char(' '))
					++j;
				// emit tabs for each tab stop crossed by the run of spaces
				int c = col;
				while (true)
				{
					const int nextStop = (c / tabWidth + 1) * tabWidth;
					if (nextStop <= col + (j - i) && nextStop - c > 1)
					{
						out += QLatin1Char('\t');
						c = nextStop;
					}
					else if (nextStop <= col + (j - i) && nextStop - c == 1)
					{
						out += QLatin1Char(' ');
						c = nextStop;
					}
					else
						break;
				}
				out += QString(col + (j - i) - c, QLatin1Char(' '));
				col += (j - i);
				i = j;
				continue;
			}
			out += expanded[i];
			++col;
			++i;
		}
		return out;
	};

	v->ex(SCI_BEGINUNDOACTION);
	if (id == IDM_EDIT_EOL2WS || id == IDM_EDIT_TRIMALL)
	{
		QList<QByteArray> parts;
		for (sptr_t l = range.first; l <= range.second; ++l)
		{
			QByteArray t = v->lineText(l);
			if (id == IDM_EDIT_TRIMALL)
				t = t.trimmed();
			if (id == IDM_EDIT_TRIMALL && t.isEmpty())
				continue;
			parts << t;
		}
		const sptr_t start = v->ex(SCI_POSITIONFROMLINE, range.first);
		const sptr_t end = v->ex(SCI_GETLINEENDPOSITION, range.second);
		v->replaceRange(start, end, parts.join(' '));
	}
	else
	{
		for (sptr_t l = range.second; l >= range.first; --l)
		{
			const QString line = QString::fromUtf8(v->lineText(l));
			QString out = line;
			switch (id)
			{
				case IDM_EDIT_TRIMTRAILING:
				{
					int n = static_cast<int>(out.size());
					while (n > 0 && (out[n - 1] == QLatin1Char(' ') || out[n - 1] == QLatin1Char('\t')))
						--n;
					out.truncate(n);
					break;
				}
				case IDM_EDIT_TRIMLINEHEAD:
				{
					int n = 0;
					while (n < out.size() && (out[n] == QLatin1Char(' ') || out[n] == QLatin1Char('\t')))
						++n;
					out = out.mid(n);
					break;
				}
				case IDM_EDIT_TRIM_BOTH:
				{
					int a = 0;
					int b = static_cast<int>(out.size());
					while (a < b && (out[a] == QLatin1Char(' ') || out[a] == QLatin1Char('\t')))
						++a;
					while (b > a && (out[b - 1] == QLatin1Char(' ') || out[b - 1] == QLatin1Char('\t')))
						--b;
					out = out.mid(a, b - a);
					break;
				}
				case IDM_EDIT_TAB2SW:
					out = expandTabs(line);
					break;
				case IDM_EDIT_SW2TAB_LEADING:
					out = spacesToTabs(line, true);
					break;
				case IDM_EDIT_SW2TAB_ALL:
					out = spacesToTabs(line, false);
					break;
				default:
					break;
			}
			if (out != line)
			{
				const sptr_t s = v->ex(SCI_POSITIONFROMLINE, l);
				const sptr_t e = v->ex(SCI_GETLINEENDPOSITION, l);
				v->replaceRange(s, e, out.toUtf8());
			}
		}
	}
	v->ex(SCI_ENDUNDOACTION);
}

void MainWindow::editMultiSelect(int id)
{
	EditView* v = _edit;
	if (!Npp::prefs().multiEditing)
		v->ex(SCI_SETMULTIPLESELECTION, true);

	auto flagsFor = [](int i) -> int
	{
		switch (i)
		{
			case IDM_EDIT_MULTISELECTALLMATCHCASE:
			case IDM_EDIT_MULTISELECTNEXTMATCHCASE:
				return SCFIND_MATCHCASE;
			case IDM_EDIT_MULTISELECTALLWHOLEWORD:
			case IDM_EDIT_MULTISELECTNEXTWHOLEWORD:
				return SCFIND_WHOLEWORD;
			case IDM_EDIT_MULTISELECTALLMATCHCASEWHOLEWORD:
			case IDM_EDIT_MULTISELECTNEXTMATCHCASEWHOLEWORD:
				return SCFIND_MATCHCASE | SCFIND_WHOLEWORD;
			default:
				return 0;
		}
	};

	if (id == IDM_EDIT_MULTISELECTUNDO)
	{
		if (v->ex(SCI_GETSELECTIONS) > 1)
			v->ex(SCI_DROPSELECTIONN, v->ex(SCI_GETMAINSELECTION));
		v->ex(SCI_SCROLLCARET);
		return;
	}
	if (id == IDM_EDIT_MULTISELECTSSKIP)
	{
		if (v->ex(SCI_GETSELECTIONS) > 1)
		{
			v->ex(SCI_DROPSELECTIONN, v->ex(SCI_GETMAINSELECTION));
			v->ex(SCI_TARGETWHOLEDOCUMENT);
			v->ex(SCI_MULTIPLESELECTADDNEXT);
			v->ex(SCI_SCROLLCARET);
		}
		return;
	}

	const bool all = (id == IDM_EDIT_MULTISELECTALL || id == IDM_EDIT_MULTISELECTALLMATCHCASE ||
	                  id == IDM_EDIT_MULTISELECTALLWHOLEWORD || id == IDM_EDIT_MULTISELECTALLMATCHCASEWHOLEWORD);
	if (v->ex(SCI_GETSELECTIONEMPTY))
	{
		const sptr_t pos = v->ex(SCI_GETCURRENTPOS);
		v->ex(SCI_SETSEL, v->ex(SCI_WORDSTARTPOSITION, pos, true), v->ex(SCI_WORDENDPOSITION, pos, true));
		if (v->ex(SCI_GETSELECTIONEMPTY))
			return;
	}
	v->ex(SCI_SETSEARCHFLAGS, flagsFor(id));
	v->ex(SCI_TARGETWHOLEDOCUMENT);
	v->ex(all ? SCI_MULTIPLESELECTADDEACH : SCI_MULTIPLESELECTADDNEXT);
	v->ex(SCI_SCROLLCARET);
}

void MainWindow::editOpenSelection(int id)
{
	QString text = _edit->selectedText().trimmed();
	if (text.isEmpty())
	{
		// take the path-like word under the caret
		const sptr_t pos = _edit->ex(SCI_GETCURRENTPOS);
		const sptr_t line = _edit->ex(SCI_LINEFROMPOSITION, pos);
		const QString lineText = QString::fromUtf8(_edit->lineText(line));
		const int col = static_cast<int>(_edit->textRangeQ(_edit->ex(SCI_POSITIONFROMLINE, line), pos).size());
		int a = col;
		int b = col;
		auto isPathChar = [](QChar c) { return !c.isSpace() && c != QLatin1Char('"') && c != QLatin1Char('\'') && c != QLatin1Char('<') && c != QLatin1Char('>'); };
		while (a > 0 && isPathChar(lineText[a - 1]))
			--a;
		while (b < lineText.size() && isPathChar(lineText[b]))
			++b;
		text = lineText.mid(a, b - a);
	}
	if (text.isEmpty())
		return;

	if (id == IDM_EDIT_SEARCHONINTERNET)
	{
		QString url = Npp::prefs().searchEngineUrl();
		url.replace(QStringLiteral("$(CURRENT_WORD)"), QString::fromUtf8(QUrl::toPercentEncoding(text)));
		QDesktopServices::openUrl(QUrl(url));
		return;
	}

	QString path = text;
	if (path.startsWith(QStringLiteral("file://")))
		path = QUrl(path).toLocalFile();
	if (path.startsWith(QLatin1Char('~')))
		path.replace(0, 1, QDir::homePath());
	if (QDir::isRelativePath(path) && _current && !_current->isUntitled())
		path = QFileInfo(_current->path).absoluteDir().absoluteFilePath(path);

	if (!QFileInfo::exists(path))
	{
		QMessageBox::information(this, QStringLiteral("Open"), QStringLiteral("\"%1\" doesn't exist.").arg(path));
		return;
	}
	if (id == IDM_EDIT_OPENSELECTEDFILETOEDIT)
		openFile(path);
	else
		QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).isDir() ? path : QFileInfo(path).absolutePath()));
}

void MainWindow::editColumnEditor()
{
	NppDialogs::columnEditor(this, _edit);
}

void MainWindow::editAutoComplete(bool wordsOnly, bool force)
{
	// Port of the essentials of AutoCompletion::showApiComplete / showWordComplete
	if (!_current)
		return;
	EditView* v = _edit;
	const sptr_t pos = v->ex(SCI_GETCURRENTPOS);
	const sptr_t wordStart = v->ex(SCI_WORDSTARTPOSITION, pos, true);
	const QByteArray prefix = v->textRange(wordStart, pos);
	const NppPrefs& p = Npp::prefs();
	if (prefix.isEmpty() && !force)
		return;
	if (!force && QString::fromUtf8(prefix).size() < p.autoCompletionThreshold)
		return;
	// Auto-Completion > Ignore numbers
	if (!force && p.autocIgnoreNumbers && std::all_of(prefix.begin(), prefix.end(), [](char c) { return c >= '0' && c <= '9'; }))
		return;

	const ApiData& api = apiFor(_current->lang);
	const bool ignoreCase = api.ignoreCase;
	const QString qprefix = QString::fromUtf8(prefix);
	QStringList items;

	const bool useApi = !wordsOnly && (force || p.autoCompletionMode == 1 || p.autoCompletionMode == 3);
	const bool useWords = wordsOnly || (!force && (p.autoCompletionMode == 2 || p.autoCompletionMode == 3)) || (force && api.keywords.isEmpty());
	if (useApi)
	{
		for (const QString& k : api.keywords)
		{
			if (k.startsWith(qprefix, ignoreCase ? Qt::CaseInsensitive : Qt::CaseSensitive))
				items << k;
		}
	}
	if (useWords && !prefix.isEmpty())
	{
		QSet<QString> words;
		v->ex(SCI_SETSEARCHFLAGS, SCFIND_WORDSTART | SCFIND_MATCHCASE);
		const sptr_t docEnd = v->ex(SCI_GETLENGTH);
		sptr_t from = 0;
		while (from < docEnd && words.size() < 2000)
		{
			v->ex(SCI_SETTARGETRANGE, from, docEnd);
			const sptr_t found = v->ex(SCI_SEARCHINTARGET, prefix.size(), reinterpret_cast<sptr_t>(prefix.constData()));
			if (found < 0)
				break;
			const sptr_t wEnd = v->ex(SCI_WORDENDPOSITION, found, true);
			if (found != wordStart && wEnd > found + static_cast<sptr_t>(prefix.size()))
				words.insert(v->textRangeQ(found, wEnd));
			from = std::max(wEnd, found + 1);
		}
		for (const QString& w : words)
			items << w;
	}
	items.removeDuplicates();
	if (items.isEmpty() || (items.size() == 1 && items.first() == qprefix))
	{
		if (v->ex(SCI_AUTOCACTIVE))
			v->ex(SCI_AUTOCCANCEL);
		return;
	}
	std::sort(items.begin(), items.end(), [](const QString& a, const QString& b) { return a.compare(b, Qt::CaseInsensitive) < 0; });
	v->ex(SCI_AUTOCSETIGNORECASE, ignoreCase);
	v->ex(SCI_AUTOCSETORDER, SC_ORDER_PERFORMSORT);
	const QByteArray list = items.join(QLatin1Char('\n')).toUtf8();
	v->ex(SCI_AUTOCSHOW, prefix.size(), reinterpret_cast<sptr_t>(list.constData()));
}

void MainWindow::editFunctionCallTip(int direction)
{
	// Port of the essentials of FunctionCallTip (AutoCompletion.cpp)
	if (!_current)
		return;
	const ApiData& api = apiFor(_current->lang);
	EditView* v = _edit;
	if (api.funcs.isEmpty())
		return;

	const sptr_t pos = v->ex(SCI_GETCURRENTPOS);
	const sptr_t from = std::max<sptr_t>(0, pos - 1024);
	const QByteArray text = v->textRange(from, pos);
	int depth = 0;
	int paramIndex = 0;
	int openPos = -1;
	for (int i = static_cast<int>(text.size()) - 1; i >= 0; --i)
	{
		const char c = text[i];
		if (c == api.stopFunc)
			++depth;
		else if (c == api.startFunc)
		{
			if (depth == 0)
			{
				openPos = i;
				break;
			}
			--depth;
		}
		else if (c == api.paramSeparator && depth == 0)
			++paramIndex;
		else if ((c == ';' || c == '{' || c == '}') && depth == 0)
			break;
	}
	if (openPos < 0)
	{
		v->ex(SCI_CALLTIPCANCEL);
		return;
	}
	int j = openPos - 1;
	while (j >= 0 && (text[j] == ' ' || text[j] == '\t'))
		--j;
	const int nameEnd = j + 1;
	while (j >= 0 && (std::isalnum(static_cast<unsigned char>(text[j])) || text[j] == '_' || static_cast<unsigned char>(text[j]) >= 0x80))
		--j;
	const QString name = QString::fromUtf8(text.mid(j + 1, nameEnd - j - 1));
	if (name.isEmpty())
	{
		v->ex(SCI_CALLTIPCANCEL);
		return;
	}

	QList<ApiFunc> overloads;
	for (auto it = api.funcs.constBegin(); it != api.funcs.constEnd(); ++it)
	{
		if (it.key().compare(name, api.ignoreCase ? Qt::CaseInsensitive : Qt::CaseSensitive) == 0)
		{
			overloads = it.value();
			break;
		}
	}
	if (overloads.isEmpty())
	{
		v->ex(SCI_CALLTIPCANCEL);
		return;
	}
	const QByteArray nameUtf8 = name.toUtf8();
	if (nameUtf8 != _callTipFunc)
		_callTipIndex = 0;
	_callTipFunc = nameUtf8;
	const int n = static_cast<int>(overloads.size());
	if (direction == 1 || direction == -1)
		_callTipIndex = (_callTipIndex + direction + n) % n;
	_callTipIndex = std::clamp(_callTipIndex, 0, n - 1);
	const ApiFunc& f = overloads[_callTipIndex];

	QByteArray tip;
	if (n > 1)
		tip += "\001 " + QByteArray::number(_callTipIndex + 1) + " of " + QByteArray::number(n) + " \002";
	if (!f.retVal.isEmpty())
		tip += f.retVal.toUtf8() + ' ';
	tip += nameUtf8 + api.startFunc;
	sptr_t hlStart = 0;
	sptr_t hlEnd = 0;
	for (int i = 0; i < f.params.size(); ++i)
	{
		if (i)
			tip += QByteArray(1, api.paramSeparator) + ' ';
		if (i == paramIndex)
			hlStart = tip.size();
		tip += f.params[i].toUtf8();
		if (i == paramIndex)
			hlEnd = tip.size();
	}
	tip += api.stopFunc;
	if (!f.descr.isEmpty())
		tip += '\n' + f.descr.toUtf8();

	const sptr_t showPos = from + j + 1;
	v->ex(SCI_CALLTIPSHOW, showPos, reinterpret_cast<sptr_t>(tip.constData()));
	v->ex(SCI_CALLTIPSETHLT, hlStart, hlEnd);
}

// ---------------------------------------------------------------------------
// Search
// ---------------------------------------------------------------------------
void MainWindow::searchFindNext(bool forward, bool selectFirst, bool volatileSearch)
{
	if (!_findDlg)
		_findDlg = new FindReplaceDlg(this);

	if (selectFirst || volatileSearch)
	{
		if (_edit->ex(SCI_GETSELECTIONEMPTY))
		{
			const sptr_t pos = _edit->ex(SCI_GETCURRENTPOS);
			_edit->ex(SCI_SETSEL, _edit->ex(SCI_WORDSTARTPOSITION, pos, true), _edit->ex(SCI_WORDENDPOSITION, pos, true));
		}
		const QString text = _edit->selectedText();
		if (text.isEmpty())
			return;
		if (selectFirst)
		{
			_findDlg->setFindText(text);
			_findDlg->findNext(forward);
			return;
		}
		FindOptions opt;
		opt.findText = text;
		opt.wrapAround = true;
		opt.matchCase = false;
		opt.wholeWord = false;
		if (!_findDlg->find(opt, forward, false))
			setStatusText(QStringLiteral("Find: Can't find the text \"%1\"").arg(text));
		return;
	}
	if (_findDlg->findText().isEmpty())
	{
		showFindDialog(FindReplaceDlg::FindTab);
		return;
	}
	_findDlg->findNext(forward);
}

void MainWindow::searchGotoLine()
{
	NppDialogs::goToLine(this, _edit);
}

void MainWindow::searchMatchingBrace(bool select)
{
	EditView* v = _edit;
	const sptr_t caretPos = v->ex(SCI_GETCURRENTPOS);
	const sptr_t lengthDoc = v->ex(SCI_GETLENGTH);
	auto isBrace = [](int c) { return c && std::strchr("[](){}", c) != nullptr; };
	sptr_t braceAtCaret = -1;
	if (lengthDoc > 0 && caretPos > 0 && isBrace(static_cast<int>(v->ex(SCI_GETCHARAT, caretPos - 1))))
		braceAtCaret = caretPos - 1;
	if (lengthDoc > 0 && braceAtCaret < 0 && isBrace(static_cast<int>(v->ex(SCI_GETCHARAT, caretPos))))
		braceAtCaret = caretPos;
	if (braceAtCaret < 0)
		return;
	const sptr_t braceOpposite = v->ex(SCI_BRACEMATCH, braceAtCaret, 0);
	if (braceOpposite < 0)
		return;
	if (select)
		v->ex(SCI_SETSEL, std::min(braceAtCaret, braceOpposite), std::max(braceAtCaret, braceOpposite) + 1);
	else
		v->ex(SCI_GOTOPOS, braceOpposite);
}

void MainWindow::searchBookmark(int id)
{
	EditView* v = _edit;
	const int mask = 1 << MARK_BOOKMARK;
	const sptr_t curLine = v->ex(SCI_LINEFROMPOSITION, v->ex(SCI_GETCURRENTPOS));
	const sptr_t lineCount = v->ex(SCI_GETLINECOUNT);

	auto bookmarkedLines = [&]()
	{
		std::vector<sptr_t> lines;
		sptr_t l = v->ex(SCI_MARKERNEXT, 0, mask);
		while (l >= 0)
		{
			lines.push_back(l);
			l = v->ex(SCI_MARKERNEXT, l + 1, mask);
		}
		return lines;
	};
	auto lineWithEol = [&](sptr_t l)
	{
		const sptr_t s = v->ex(SCI_POSITIONFROMLINE, l);
		const sptr_t e = (l + 1 < lineCount) ? v->ex(SCI_POSITIONFROMLINE, l + 1) : v->ex(SCI_GETLENGTH);
		return std::make_pair(s, e);
	};

	switch (id)
	{
		case IDM_SEARCH_TOGGLE_BOOKMARK:
			if (v->ex(SCI_MARKERGET, curLine) & mask)
				v->ex(SCI_MARKERDELETE, curLine, MARK_BOOKMARK);
			else
				v->ex(SCI_MARKERADD, curLine, MARK_BOOKMARK);
			break;
		case IDM_SEARCH_NEXT_BOOKMARK:
		{
			sptr_t l = v->ex(SCI_MARKERNEXT, curLine + 1, mask);
			if (l < 0)
				l = v->ex(SCI_MARKERNEXT, 0, mask);
			if (l >= 0)
				v->gotoLine(l);
			break;
		}
		case IDM_SEARCH_PREV_BOOKMARK:
		{
			sptr_t l = v->ex(SCI_MARKERPREVIOUS, curLine - 1, mask);
			if (l < 0)
				l = v->ex(SCI_MARKERPREVIOUS, lineCount - 1, mask);
			if (l >= 0)
				v->gotoLine(l);
			break;
		}
		case IDM_SEARCH_CLEAR_BOOKMARKS:
			v->ex(SCI_MARKERDELETEALL, MARK_BOOKMARK);
			break;
		case IDM_SEARCH_COPYMARKEDLINES:
		case IDM_SEARCH_CUTMARKEDLINES:
		case IDM_SEARCH_DELETEMARKEDLINES:
		{
			const std::vector<sptr_t> lines = bookmarkedLines();
			if (lines.empty())
				return;
			if (id != IDM_SEARCH_DELETEMARKEDLINES)
			{
				QByteArray text;
				for (sptr_t l : lines)
				{
					const auto r = lineWithEol(l);
					text += v->textRange(r.first, r.second);
				}
				QApplication::clipboard()->setText(QString::fromUtf8(text));
			}
			if (id != IDM_SEARCH_COPYMARKEDLINES)
			{
				v->ex(SCI_BEGINUNDOACTION);
				for (auto it = lines.rbegin(); it != lines.rend(); ++it)
				{
					const auto r = lineWithEol(*it);
					v->ex(SCI_DELETERANGE, r.first, r.second - r.first);
				}
				v->ex(SCI_ENDUNDOACTION);
			}
			break;
		}
		case IDM_SEARCH_PASTEMARKEDLINES:
		{
			const std::vector<sptr_t> lines = bookmarkedLines();
			if (lines.empty())
				return;
			const QByteArray clip = QApplication::clipboard()->text().toUtf8();
			v->ex(SCI_BEGINUNDOACTION);
			for (auto it = lines.rbegin(); it != lines.rend(); ++it)
			{
				const auto r = lineWithEol(*it);
				v->ex(SCI_DELETERANGE, r.first, r.second - r.first);
			}
			v->ex(SCI_INSERTTEXT, v->ex(SCI_POSITIONFROMLINE, lines.front()), reinterpret_cast<sptr_t>(clip.constData()));
			v->ex(SCI_ENDUNDOACTION);
			break;
		}
		case IDM_SEARCH_DELETEUNMARKEDLINES:
		{
			v->ex(SCI_BEGINUNDOACTION);
			for (sptr_t l = lineCount - 1; l >= 0; --l)
			{
				if (v->ex(SCI_MARKERGET, l) & mask)
					continue;
				const auto r = lineWithEol(l);
				v->ex(SCI_DELETERANGE, r.first, r.second - r.first);
			}
			v->ex(SCI_ENDUNDOACTION);
			break;
		}
		case IDM_SEARCH_INVERSEMARKS:
			for (sptr_t l = 0; l < lineCount; ++l)
			{
				if (v->ex(SCI_MARKERGET, l) & mask)
					v->ex(SCI_MARKERDELETE, l, MARK_BOOKMARK);
				else
					v->ex(SCI_MARKERADD, l, MARK_BOOKMARK);
			}
			break;
		default:
			break;
	}
}

void MainWindow::searchStyleToken(int id)
{
	EditView* v = _edit;
	if (id == IDM_SEARCH_CLEARALLMARKS)
	{
		for (int i = 1; i <= 5; ++i)
			v->clearIndicator(styleIndicator(i));
		return;
	}
	static const int unmark[] = {IDM_SEARCH_UNMARKALLEXT1, IDM_SEARCH_UNMARKALLEXT2, IDM_SEARCH_UNMARKALLEXT3, IDM_SEARCH_UNMARKALLEXT4, IDM_SEARCH_UNMARKALLEXT5};
	static const int markAll[] = {IDM_SEARCH_MARKALLEXT1, IDM_SEARCH_MARKALLEXT2, IDM_SEARCH_MARKALLEXT3, IDM_SEARCH_MARKALLEXT4, IDM_SEARCH_MARKALLEXT5};
	static const int markOne[] = {IDM_SEARCH_MARKONEEXT1, IDM_SEARCH_MARKONEEXT2, IDM_SEARCH_MARKONEEXT3, IDM_SEARCH_MARKONEEXT4, IDM_SEARCH_MARKONEEXT5};
	for (int i = 0; i < 5; ++i)
	{
		const int indic = styleIndicator(i + 1);
		if (id == unmark[i])
		{
			v->clearIndicator(indic);
			return;
		}
		if (id == markAll[i] || id == markOne[i])
		{
			if (v->ex(SCI_GETSELECTIONEMPTY))
			{
				const sptr_t pos = v->ex(SCI_GETCURRENTPOS);
				v->ex(SCI_SETSEL, v->ex(SCI_WORDSTARTPOSITION, pos, true), v->ex(SCI_WORDENDPOSITION, pos, true));
			}
			// Highlighting > Style All Occurrences of Token
			const NppPrefs& p = Npp::prefs();
			const int markFlags = (p.markAllMatchCase ? SCFIND_MATCHCASE : 0) | (p.markAllWholeWord ? SCFIND_WHOLEWORD : 0);
			const sptr_t s = v->ex(SCI_GETSELECTIONSTART);
			const sptr_t e = v->ex(SCI_GETSELECTIONEND);
			if (e <= s)
				return;
			if (id == markOne[i])
			{
				v->ex(SCI_SETINDICATORCURRENT, indic);
				v->ex(SCI_INDICATORFILLRANGE, s, e - s);
			}
			else
			{
				const QByteArray text = v->textRange(s, e);
				v->markAllOccurrences(indic, text, markFlags);
			}
			return;
		}
	}
}

void MainWindow::searchJumpToStyle(int id)
{
	int n = 0;
	bool forward = true;
	switch (id)
	{
		case IDM_SEARCH_GONEXTMARKER1: n = 1; break;
		case IDM_SEARCH_GONEXTMARKER2: n = 2; break;
		case IDM_SEARCH_GONEXTMARKER3: n = 3; break;
		case IDM_SEARCH_GONEXTMARKER4: n = 4; break;
		case IDM_SEARCH_GONEXTMARKER5: n = 5; break;
		case IDM_SEARCH_GONEXTMARKER_DEF: n = 0; break;
		case IDM_SEARCH_GOPREVMARKER1: n = 1; forward = false; break;
		case IDM_SEARCH_GOPREVMARKER2: n = 2; forward = false; break;
		case IDM_SEARCH_GOPREVMARKER3: n = 3; forward = false; break;
		case IDM_SEARCH_GOPREVMARKER4: n = 4; forward = false; break;
		case IDM_SEARCH_GOPREVMARKER5: n = 5; forward = false; break;
		case IDM_SEARCH_GOPREVMARKER_DEF: n = 0; forward = false; break;
		default: return;
	}
	EditView* v = _edit;
	const int indic = styleIndicator(n);
	const sptr_t len = v->ex(SCI_GETLENGTH);
	auto isOn = [&](sptr_t pos) { return pos >= 0 && pos < len && v->ex(SCI_INDICATORVALUEAT, indic, pos) != 0; };

	if (forward)
	{
		sptr_t pos = v->ex(SCI_GETSELECTIONEND);
		if (isOn(pos))
			pos = v->ex(SCI_INDICATOREND, indic, pos);
		sptr_t start = isOn(pos) ? pos : v->ex(SCI_INDICATOREND, indic, pos);
		if (start >= len || !isOn(start))
		{
			start = isOn(0) ? 0 : v->ex(SCI_INDICATOREND, indic, 0);
			if (start >= len || !isOn(start))
				return;
		}
		v->displaySection(start, v->ex(SCI_INDICATOREND, indic, start), true);
	}
	else
	{
		sptr_t pos = v->ex(SCI_GETSELECTIONSTART) - 1;
		if (isOn(pos))
			pos = v->ex(SCI_INDICATORSTART, indic, pos) - 1;
		while (pos >= 0 && !isOn(pos))
			pos = v->ex(SCI_INDICATORSTART, indic, pos) - 1;
		if (pos < 0)
		{
			pos = len - 1;
			while (pos >= 0 && !isOn(pos))
				pos = v->ex(SCI_INDICATORSTART, indic, pos) - 1;
			if (pos < 0)
				return;
		}
		const sptr_t start = v->ex(SCI_INDICATORSTART, indic, pos);
		v->displaySection(start, v->ex(SCI_INDICATOREND, indic, pos), false);
	}
}

void MainWindow::searchCopyStyled(int id)
{
	EditView* v = _edit;
	QByteArray text;
	switch (id)
	{
		case IDM_SEARCH_STYLE1TOCLIP: text = indicatorText(v, styleIndicator(1)); break;
		case IDM_SEARCH_STYLE2TOCLIP: text = indicatorText(v, styleIndicator(2)); break;
		case IDM_SEARCH_STYLE3TOCLIP: text = indicatorText(v, styleIndicator(3)); break;
		case IDM_SEARCH_STYLE4TOCLIP: text = indicatorText(v, styleIndicator(4)); break;
		case IDM_SEARCH_STYLE5TOCLIP: text = indicatorText(v, styleIndicator(5)); break;
		case IDM_SEARCH_MARKEDTOCLIP: text = indicatorText(v, styleIndicator(0)); break;
		case IDM_SEARCH_ALLSTYLESTOCLIP:
		{
			// ranges of the 5 styles, in document order
			std::vector<std::pair<sptr_t, sptr_t>> ranges;
			const sptr_t len = v->ex(SCI_GETLENGTH);
			for (int i = 1; i <= 5; ++i)
			{
				const int indic = styleIndicator(i);
				sptr_t pos = 0;
				while (pos < len)
				{
					const sptr_t end = v->ex(SCI_INDICATOREND, indic, pos);
					if (v->ex(SCI_INDICATORVALUEAT, indic, pos))
						ranges.emplace_back(pos, end);
					if (end <= pos)
						break;
					pos = end;
				}
			}
			std::sort(ranges.begin(), ranges.end());
			const QByteArray eol = v->eolString();
			for (const auto& r : ranges)
			{
				if (!text.isEmpty())
					text += eol;
				text += v->textRange(r.first, r.second);
			}
			break;
		}
		default:
			break;
	}
	if (!text.isEmpty())
		QApplication::clipboard()->setText(QString::fromUtf8(text));
}

void MainWindow::searchChangeHistory(int id)
{
	EditView* v = _edit;
	const int mask = (1 << SC_MARKNUM_HISTORY_MODIFIED) | (1 << SC_MARKNUM_HISTORY_SAVED) |
	                 (1 << SC_MARKNUM_HISTORY_REVERTED_TO_MODIFIED) | (1 << SC_MARKNUM_HISTORY_REVERTED_TO_ORIGIN);
	if (id == IDM_SEARCH_CLEAR_CHANGE_HISTORY)
	{
		const auto answer = QMessageBox::question(this, QStringLiteral("Clear Change History"),
			QStringLiteral("Clearing the change history also clears the undo history of the document. Continue?"));
		if (answer != QMessageBox::Yes)
			return;
		v->ex(SCI_EMPTYUNDOBUFFER);
		v->ex(SCI_SETCHANGEHISTORY, SC_CHANGE_HISTORY_DISABLED);
		v->ex(SCI_SETCHANGEHISTORY, EditView::changeHistoryFlags());
		return;
	}
	const sptr_t curLine = v->ex(SCI_LINEFROMPOSITION, v->ex(SCI_GETCURRENTPOS));
	const sptr_t lineCount = v->ex(SCI_GETLINECOUNT);
	const bool forward = (id == IDM_SEARCH_CHANGED_NEXT);
	sptr_t l = curLine;
	if (forward)
	{
		while (l < lineCount && (v->ex(SCI_MARKERGET, l) & mask))
			++l; // leave the current block of changes
		l = v->ex(SCI_MARKERNEXT, l, mask);
		if (l < 0)
			l = v->ex(SCI_MARKERNEXT, 0, mask);
	}
	else
	{
		while (l >= 0 && (v->ex(SCI_MARKERGET, l) & mask))
			--l;
		l = v->ex(SCI_MARKERPREVIOUS, l, mask);
		if (l < 0)
			l = v->ex(SCI_MARKERPREVIOUS, lineCount - 1, mask);
		while (l > 0 && (v->ex(SCI_MARKERGET, l - 1) & mask))
			--l; // go to the beginning of the block
	}
	if (l >= 0)
		v->gotoLine(l);
}

// ---------------------------------------------------------------------------
// View
// ---------------------------------------------------------------------------
void MainWindow::viewToggle(int id)
{
	NppPrefs& p = Npp::prefs();
	switch (id)
	{
		case IDM_VIEW_TAB_SPACE: p.showSpaceTab = !p.showSpaceTab; break;
		case IDM_VIEW_EOL: p.showEol = !p.showEol; break;
		case IDM_VIEW_ALL_CHARACTERS:
		{
			const bool on = !(p.showSpaceTab && p.showEol);
			p.showSpaceTab = on;
			p.showEol = on;
			break;
		}
		case IDM_VIEW_INDENT_GUIDE: p.showIndentGuide = !p.showIndentGuide; break;
		case IDM_VIEW_WRAP_SYMBOL: p.showWrapSymbol = !p.showWrapSymbol; break;
		case IDM_VIEW_WRAP: p.wordWrap = !p.wordWrap; break;
		case IDM_VIEW_NPC: p.showNpc = !p.showNpc; break;
		case IDM_VIEW_NPC_CCUNIEOL: p.showCcUniEol = !p.showCcUniEol; break;
		default: break;
	}
	_edit->applyPrefs();
	if (id == IDM_VIEW_INDENT_GUIDE && _current)
		_edit->defineDocType(_current->lang, _current->udlName);
	p.save();
	updateMenuState();
}

void MainWindow::viewZoom(int id)
{
	if (id == IDM_VIEW_ZOOMIN)
		_edit->ex(SCI_ZOOMIN);
	else if (id == IDM_VIEW_ZOOMOUT)
		_edit->ex(SCI_ZOOMOUT);
	else
		_edit->ex(SCI_SETZOOM, 0);
}

void MainWindow::viewTab(int id)
{
	const int count = _tabs->count();
	const int cur = _tabs->currentIndex();
	if (count == 0)
		return;
	static const int tabIds[] = {IDM_VIEW_TAB1, IDM_VIEW_TAB2, IDM_VIEW_TAB3, IDM_VIEW_TAB4, IDM_VIEW_TAB5, IDM_VIEW_TAB6, IDM_VIEW_TAB7, IDM_VIEW_TAB8, IDM_VIEW_TAB9};
	for (int i = 0; i < 9; ++i)
	{
		if (id == tabIds[i])
		{
			const int target = (i == 8) ? count - 1 : i;
			if (target < count)
				activateBuffer(bufferAt(target));
			return;
		}
	}
	switch (id)
	{
		case IDM_VIEW_TAB_NEXT:
			activateBuffer(bufferAt((cur + 1) % count));
			break;
		case IDM_VIEW_TAB_PREV:
			activateBuffer(bufferAt((cur - 1 + count) % count));
			break;
		case IDM_VIEW_TAB_MOVEFORWARD:
			if (cur + 1 < count)
				_tabs->moveTab(cur, cur + 1);
			break;
		case IDM_VIEW_TAB_MOVEBACKWARD:
			if (cur > 0)
				_tabs->moveTab(cur, cur - 1);
			break;
		case IDM_VIEW_TAB_START: // First Tab
			activateBuffer(bufferAt(0));
			break;
		case IDM_VIEW_TAB_END: // Last Tab
			activateBuffer(bufferAt(count - 1));
			break;
		case IDM_VIEW_GOTO_START: // Move to Start
			if (cur > 0)
				_tabs->moveTab(cur, 0);
			break;
		case IDM_VIEW_GOTO_END: // Move to End
			if (cur + 1 < count)
				_tabs->moveTab(cur, count - 1);
			break;
		default:
			break;
	}
	refreshDocumentList();
}

void MainWindow::viewFold(int id)
{
	switch (id)
	{
		case IDM_VIEW_FOLDALL: _edit->foldAll(true); return;
		case IDM_VIEW_UNFOLDALL: _edit->foldAll(false); return;
		case IDM_VIEW_FOLD_CURRENT:
		case IDM_VIEW_UNFOLD_CURRENT:
		{
			// Editing 1 > Make current level folding/unfolding commands toggleable
			bool fold = (id == IDM_VIEW_FOLD_CURRENT);
			if (Npp::prefs().foldToggleable)
				fold = !_edit->isCurrentLineFolded();
			_edit->foldCurrent(fold);
			return;
		}
		default: break;
	}
	if (id > IDM_VIEW_FOLD && id <= IDM_VIEW_FOLD_8)
		_edit->foldLevel(id - IDM_VIEW_FOLD - 1, true);
	else if (id > IDM_VIEW_UNFOLD && id <= IDM_VIEW_UNFOLD_8)
		_edit->foldLevel(id - IDM_VIEW_UNFOLD - 1, false);
}

void MainWindow::viewInBrowser(int id)
{
	if (!_current || _current->isUntitled())
	{
		QMessageBox::information(this, QStringLiteral("View in browser"), QStringLiteral("Please save the document first."));
		return;
	}
	QStringList candidates;
	if (id == IDM_VIEW_IN_FIREFOX)
		candidates = {QStringLiteral("firefox")};
	else if (id == IDM_VIEW_IN_CHROME)
		candidates = {QStringLiteral("google-chrome"), QStringLiteral("google-chrome-stable"), QStringLiteral("chromium"), QStringLiteral("chromium-browser")};
	else if (id == IDM_VIEW_IN_EDGE)
		candidates = {QStringLiteral("microsoft-edge"), QStringLiteral("microsoft-edge-stable")};
	for (const QString& c : candidates)
	{
		const QString exe = QStandardPaths::findExecutable(c);
		if (!exe.isEmpty())
		{
			QProcess::startDetached(exe, {_current->path});
			return;
		}
	}
	QDesktopServices::openUrl(QUrl::fromLocalFile(_current->path));
}

void MainWindow::viewSummary()
{
	if (!_current)
		return;
	EditView* v = _edit;
	const QByteArray text = v->wholeText();
	const QString s = QString::fromUtf8(text);
	qsizetype chars = 0;
	for (const QChar c : s)
	{
		if (c != QLatin1Char('\r') && c != QLatin1Char('\n') && !c.isLowSurrogate())
			++chars;
	}
	static const QRegularExpression wordRe(QStringLiteral("\\w+"), QRegularExpression::UseUnicodePropertiesOption);
	qsizetype words = 0;
	auto it = wordRe.globalMatch(s);
	while (it.hasNext())
	{
		it.next();
		++words;
	}
	QString msg;
	if (!_current->isUntitled())
	{
		const QFileInfo fi(_current->path);
		msg += QStringLiteral("Full file path: %1\nCreated: %2\nModified: %3\n\n")
			.arg(_current->path, fi.birthTime().isValid() ? QLocale::system().toString(fi.birthTime(), QLocale::ShortFormat) : QStringLiteral("-"),
			     QLocale::system().toString(fi.lastModified(), QLocale::ShortFormat));
	}
	const QLocale loc = QLocale::system();
	msg += QStringLiteral("Characters (without line endings): %1\nWords: %2\nLines: %3\nDocument length: %4\n")
		.arg(loc.toString(chars), loc.toString(words), loc.toString(static_cast<qlonglong>(v->ex(SCI_GETLINECOUNT))),
		     loc.toString(static_cast<qlonglong>(text.size())));
	const sptr_t nbSel = v->ex(SCI_GETSELECTIONS);
	sptr_t selChars = 0;
	sptr_t selBytes = 0;
	for (sptr_t i = 0; i < nbSel; ++i)
	{
		const sptr_t a = v->ex(SCI_GETSELECTIONNSTART, i);
		const sptr_t b = v->ex(SCI_GETSELECTIONNEND, i);
		selChars += v->ex(SCI_COUNTCHARACTERS, a, b);
		selBytes += b - a;
	}
	msg += QStringLiteral("%1 selected characters (%2 bytes) in %3 ranges").arg(selChars).arg(selBytes).arg(v->ex(SCI_GETSELECTIONEMPTY) ? 0 : nbSel);
	QMessageBox::information(this, QStringLiteral("Summary"), msg);
}

void MainWindow::viewFullScreen(bool postIt)
{
	const NppPrefs& p = Npp::prefs();
	if (postIt)
	{
		_postIt = !_postIt;
		menuBar()->setVisible(!_postIt && !p.hideMenuBar);
		_toolbar->setVisible(p.toolbarVisible && !_postIt);
		statusBar()->setVisible(p.statusbarVisible && !_postIt);
		for (DocView& dv : _views)
			dv.tabs->setVisible(p.tabbarVisible && !_postIt);
		setWindowFlag(Qt::FramelessWindowHint, _postIt);
		show();
	}
	else
	{
		if (isFullScreen())
			showNormal();
		else
			showFullScreen();
	}
	updateMenuState();
}

// ---------------------------------------------------------------------------
// Encoding & language
// ---------------------------------------------------------------------------
void MainWindow::encodingSet(int id)
{
	Buffer* b = _current;
	if (!b)
		return;

	FileIO::Force force = FileIO::Force::Auto;
	int codepage = CP_ANSI;
	UniMode wanted = UniMode::uniCookie;
	switch (id)
	{
		case IDM_FORMAT_ANSI: force = FileIO::Force::Codepage; wanted = UniMode::uni8Bit; break;
		case IDM_FORMAT_AS_UTF_8: force = FileIO::Force::Utf8; wanted = UniMode::uniCookie; break;
		case IDM_FORMAT_UTF_8: force = FileIO::Force::Utf8; wanted = UniMode::uniUTF8; break;
		case IDM_FORMAT_UTF_16BE: force = FileIO::Force::Utf16BE; wanted = UniMode::uni16BE; break;
		case IDM_FORMAT_UTF_16LE: force = FileIO::Force::Utf16LE; wanted = UniMode::uni16LE; break;
		default:
			force = FileIO::Force::Codepage;
			wanted = UniMode::uni8Bit;
			codepage = EncodingMapper::getInstance().getEncodingFromIndex(id - IDM_FORMAT_ENCODE);
			break;
	}

	if (b->isUntitled())
	{
		b->unicode = wanted;
		b->codepage = codepage;
	}
	else
	{
		if (b->dirty)
		{
			const auto answer = QMessageBox::question(this, QStringLiteral("Save Current Modification"),
				QStringLiteral("You should save the current modification.\nAll the saved modifications can not be undone.\n\nContinue?"));
			if (answer != QMessageBox::Yes || !saveBuffer(b))
			{
				updateMenuState();
				return;
			}
		}
		reloadBuffer(b, force, codepage, true);
		if ((id == IDM_FORMAT_UTF_8 && b->unicode != UniMode::uniUTF8) || (id == IDM_FORMAT_AS_UTF_8 && b->unicode != UniMode::uniCookie))
		{
			// adding / removing a BOM changes the file
			b->unicode = wanted;
			b->forcedDirty = true;
			b->dirty = true;
		}
	}
	updateTabLabel(indexOf(b));
	updateTitle();
	updateStatusBar();
	updateMenuState();
	refreshPanels(); // the Character Panel shows the characters of the new code page
}

void MainWindow::encodingConvert(int id)
{
	Buffer* b = _current;
	if (!b || b->isReadOnly())
		return;
	UniMode target = UniMode::uniCookie;
	int codepage = CP_ANSI;
	switch (id)
	{
		case IDM_FORMAT_CONV2_ANSI: target = UniMode::uni8Bit; break;
		case IDM_FORMAT_CONV2_AS_UTF_8: target = UniMode::uniCookie; break;
		case IDM_FORMAT_CONV2_UTF_8: target = UniMode::uniUTF8; break;
		case IDM_FORMAT_CONV2_UTF_16BE: target = UniMode::uni16BE; break;
		case IDM_FORMAT_CONV2_UTF_16LE: target = UniMode::uni16LE; break;
		default: return;
	}
	if (target == b->unicode && (target != UniMode::uni8Bit || codepage == b->codepage))
		return;
	b->unicode = target;
	b->codepage = codepage;
	b->forcedDirty = true;
	b->dirty = true;
	updateTabLabel(indexOf(b));
	updateTitle();
	updateStatusBar();
	updateMenuState();
	refreshPanels();
}

void MainWindow::languageSet(LangType lang, const QString& udlName)
{
	if (!_current)
		return;
	_current->lang = lang;
	_current->udlName = (lang == L_USER) ? udlName : QString();
	_edit->defineDocType(lang, _current->udlName);
	_edit->restoreHiddenLines(); // Notepad_plus::setLanguage()
	refreshOtherViews(_current);
	refreshPanels(); // BufferChangeLanguage: the Function List follows the language
	updateStatusBar();
	updateMenuState();
	_pluginsManager->notifyBuffer(NPPN_LANGCHANGED, _current);
}

// ---------------------------------------------------------------------------
// Macro
// ---------------------------------------------------------------------------
void MainWindow::onMacroRecord(unsigned int message, uptr_t wParam, sptr_t lParam)
{
	if (!_recording || _playingMacro)
		return;
	// NppNotification.cpp SCN_MACRORECORD: SCI_REPLACESEL of one EOL character is recorded as
	// SCI_NEWLINE, which inserts the EOL of the document
	if (message == SCI_REPLACESEL && lParam)
	{
		const char* ch = reinterpret_cast<const char*>(lParam);
		if (ch[0] != '\0' && ch[1] == '\0' && (ch[0] == '\n' || ch[0] == '\r'))
		{
			// CR LF document: the CR was already recorded as SCI_NEWLINE
			if (_current && _current->eol == SC_EOL_CRLF && ch[0] == '\n' && !_macro.empty() && _macro.back().message == SCI_NEWLINE)
				return;
			message = SCI_NEWLINE;
			wParam = 0;
			lParam = 0;
		}
	}
	MacroStep step;
	step.kind = MacroStep::Scintilla;
	step.message = message;
	step.wParam = wParam;
	step.lParam = lParam;
	switch (message)
	{
		case SCI_REPLACESEL:
		case SCI_INSERTTEXT:
		case SCI_SEARCHNEXT:
		case SCI_SEARCHPREV:
			if (lParam)
			{
				step.text = QByteArray(reinterpret_cast<const char*>(lParam));
				step.hasText = true;
			}
			break;
		case SCI_ADDTEXT:
		case SCI_APPENDTEXT:
			if (lParam)
			{
				step.text = QByteArray(reinterpret_cast<const char*>(lParam), static_cast<qsizetype>(wParam));
				step.hasText = true;
			}
			break;
		default:
			break;
	}
	_macro.push_back(step);
}

void MainWindow::macroStart()
{
	_macro.clear();
	_recording = true;
	_recordingSaved = false;
	_edit->ex(SCI_STARTRECORD);
	setStatusText(QStringLiteral("Recording macro..."), 3000);
	updateMenuState();
}

void MainWindow::macroStop()
{
	_edit->ex(SCI_STOPRECORD);
	_recording = false;
	updateMenuState();
}

void MainWindow::macroPlayback(int times)
{
	macroPlaySteps(_macro, times);
}

void MainWindow::macroPlaySteps(const std::vector<MacroStep>& steps, int times)
{
	if (steps.empty())
		return;
	_playingMacro = true;
	_edit->ex(SCI_BEGINUNDOACTION);
	const bool untilEnd = (times <= 0);
	int run = 0;
	sptr_t lastPos = -1;
	while (untilEnd || run < times)
	{
		for (const MacroStep& s : steps)
		{
			if (s.kind == MacroStep::Menu)
				command(static_cast<int>(s.message));
			else
				_edit->ex(s.message, s.wParam, s.hasText ? reinterpret_cast<sptr_t>(s.text.constData()) : s.lParam);
		}
		++run;
		if (untilEnd)
		{
			const sptr_t pos = _edit->ex(SCI_GETCURRENTPOS);
			if (pos == lastPos || pos >= _edit->ex(SCI_GETLENGTH) || run > 100000)
				break;
			lastPos = pos;
		}
	}
	_edit->ex(SCI_ENDUNDOACTION);
	_playingMacro = false;
}

void MainWindow::macroRunMultiple()
{
	// RunMacroDlg: the current recorded macro and the saved macros
	QStringList names;
	if (!_macro.empty())
		names << QStringLiteral("Current recorded macro");
	for (const SavedMacro& m : _savedMacros)
		names << m.name;
	int index = 0;
	const int times = NppDialogs::macroRunMultiple(this, names, &index);
	if (times < 0 || index < 0)
		return;
	if (!_macro.empty())
	{
		if (index == 0)
		{
			macroPlayback(times);
			return;
		}
		--index;
	}
	if (index < static_cast<int>(_savedMacros.size()) && checkShortcutsXml())
		macroPlaySteps(_savedMacros[static_cast<size_t>(index)].steps, times);
}

bool MainWindow::checkShortcutsXml()
{
	// NppCommands.cpp: the commands of shortcuts.xml run only if the file is the one Notepad++ wrote or validated
	if (Macros::isShortcutsXmlTrusted())
		return true;
	OpenOptions opts;
	opts.readOnly = true;
	if (Buffer* b = openFile(Macros::shortcutsPath(), true, opts))
		activateBuffer(b);
	QMessageBox::warning(this, QStringLiteral("Security Warning"),
	                     QStringLiteral("The shortcuts.xml file appears to have been modified manually.\n\nFor security reasons, please review the opened "
	                                    "shortcuts.xml. If the file content is OK, use \"Validate shortcuts.xml\" from the \"Run\" menu to confirm it."));
	return false;
}

void MainWindow::runCommandLine(const QString& cmd)
{
	// Command::run(): the variables of Notepad++, then like ShellExecute "open": an address in the
	// browser, a document with its program, a command line in the shell
	const QString expanded = expandRunVariables(cmd).trimmed();
	if (expanded.isEmpty())
		return;
	static const QRegularExpression urlRe(QStringLiteral("^([A-Za-z][A-Za-z0-9+.-]*://|mailto:)"));
	if (urlRe.match(expanded).hasMatch())
	{
		QDesktopServices::openUrl(QUrl(expanded, QUrl::TolerantMode));
		return;
	}
	QString program = expanded;
	QString rest;
	if (program.startsWith(QLatin1Char('"')))
	{
		const qsizetype end = program.indexOf(QLatin1Char('"'), 1);
		rest = end > 0 ? program.mid(end + 1) : QString();
		program = end > 0 ? program.mid(1, end - 1) : program.mid(1);
	}
	else if (const qsizetype sp = program.indexOf(QLatin1Char(' ')); sp > 0)
	{
		rest = program.mid(sp + 1);
		program.truncate(sp);
	}
	const QFileInfo fi(program);
	if (fi.isFile() && !fi.isExecutable() && rest.trimmed().isEmpty())
	{
		QDesktopServices::openUrl(QUrl::fromLocalFile(fi.absoluteFilePath()));
		return;
	}
	const QString workDir = (_current && !_current->isUntitled()) ? QFileInfo(_current->path).absolutePath() : QDir::homePath();
	if (!QProcess::startDetached(QStringLiteral("/bin/sh"), {QStringLiteral("-c"), expanded}, workDir))
		QMessageBox::warning(this, QStringLiteral("Run"), QStringLiteral("An attempt was made to execute the below command.\n\nCommand: %1").arg(expanded));
}

void MainWindow::applyScintillaKeys()
{
	// ScintillaAccelerator::updateKeys(): the key map of both views is the Notepad++ list,
	// the top of the list first; a command whose menu shortcut was changed keeps only the menu key
	std::vector<ScintillaKeyCmd>& cmds = Macros::scintillaKeys();
	QSettings& st = cmdSettings();
	st.sync();
	for (int v = MAIN_VIEW; v <= SUB_VIEW; ++v)
	{
		EditView* e = _views[v].edit;
		e->ex(SCI_CLEARALLCMDKEYS);
		for (auto it = cmds.rbegin(); it != cmds.rend(); ++it)
		{
			if (it->menuCmd != 0 && st.contains(QStringLiteral("Shortcuts/%1").arg(it->menuCmd)))
				continue;
			for (const ScintillaKeyCombo& k : it->keys)
			{
				if (k.vk)
					e->ex(SCI_ASSIGNCMDKEY, static_cast<uptr_t>(Macros::scintillaKeyDef(k)), it->sciCmd);
			}
		}
		e->applyKeyBindings();
	}
}

void MainWindow::commandsManage()
{
	if (!Macros::manageDialog(this, &_savedCommands))
		return;
	QString error;
	if (!Macros::saveCommands(_savedCommands, &error))
		QMessageBox::warning(this, QStringLiteral("Shortcut Mapper"), error);
	rebuildRunMenu();
}

void MainWindow::rebuildRunMenu()
{
	// Notepad_plus::init() Run Menu: the commands after "Validate shortcuts.xml", then
	// "Modify Shortcut/Delete Command..."
	QMenu* runMenu = nullptr;
	if (QAction* exec = _actions.value(IDM_EXECUTE))
	{
		for (QObject* o : exec->associatedObjects())
		{
			if (auto* m = qobject_cast<QMenu*>(o))
				runMenu = m;
		}
	}
	for (QAction* a : std::as_const(_runMenuItems))
	{
		removeAction(a);
		if (runMenu)
			runMenu->removeAction(a);
		if (QMenu* sub = a->menu())
			sub->deleteLater();
		a->deleteLater();
	}
	_runMenuItems.clear();
	for (int id = ID_USER_CMD; id < ID_USER_CMD_LIMIT; ++id)
		_handlers.remove(id);
	if (!runMenu || _savedCommands.empty())
		return;
	auto add = [&](QAction* a)
	{
		runMenu->addAction(a);
		_runMenuItems << a;
	};
	auto* sep = new QAction(runMenu);
	sep->setSeparator(true);
	add(sep);
	QHash<QString, QMenu*> folders;
	const int count = std::min(static_cast<int>(_savedCommands.size()), ID_USER_CMD_LIMIT - ID_USER_CMD);
	for (int i = 0; i < count; ++i)
	{
		const SavedCommand& c = _savedCommands[static_cast<size_t>(i)];
		auto* a = new QAction(c.name, this);
		a->setShortcut(c.keySequence());
		a->setData(ID_USER_CMD + i);
		connect(a, &QAction::triggered, this, [this, i] { command(ID_USER_CMD + i); });
		_handlers.insert(ID_USER_CMD + i, [this, i]
		{
			if (i < static_cast<int>(_savedCommands.size()) && checkShortcutsXml())
				runCommandLine(_savedCommands[static_cast<size_t>(i)].command);
		});
		addAction(a);
		if (c.folder.isEmpty())
		{
			add(a);
			continue;
		}
		QMenu* sub = folders.value(c.folder);
		if (!sub)
		{
			sub = new QMenu(c.folder, runMenu);
			folders.insert(c.folder, sub);
			add(sub->menuAction());
		}
		sub->addAction(a);
		_runMenuItems << a;
	}
	auto* sep2 = new QAction(runMenu);
	sep2->setSeparator(true);
	add(sep2);
	auto* modify = new QAction(QStringLiteral("Modify Shortcut/Delete Command..."), this);
	connect(modify, &QAction::triggered, this, [this] { command(IDM_SETTING_SHORTCUT_MAPPER_RUN); });
	add(modify);
}

void MainWindow::macroSave()
{
	// IDM_MACRO_SAVECURRENTMACRO: Notepad_plus::addCurrentMacro()
	if (_macro.empty() || _recording)
		return;
	SavedMacro m;
	m.steps = _macro;
	if (!Macros::editDialog(this, &m, QStringLiteral("Shortcut")))
		return;
	_savedMacros.push_back(m);
	QString error;
	if (!Macros::save(_savedMacros, &error))
		QMessageBox::warning(this, QStringLiteral("Save Current Recorded Macro"), error);
	_recordingSaved = true;
	rebuildMacroMenu();
	updateMenuState();
}

void MainWindow::macroManage()
{
	// IDM_SETTING_SHORTCUT_MAPPER_MACRO
	if (!Macros::manageDialog(this, &_savedMacros))
		return;
	QString error;
	if (!Macros::save(_savedMacros, &error))
		QMessageBox::warning(this, QStringLiteral("Shortcut Mapper"), error);
	rebuildMacroMenu();
	updateMenuState();
}

void MainWindow::rebuildMacroMenu()
{
	// Notepad_plus::init() Macro Menu: DynamicMenu after "Run a Macro Multiple Times...",
	// then "Modify Shortcut/Delete Macro..."
	for (QAction* a : std::as_const(_macroMenuItems))
	{
		removeAction(a);
		if (_macroMenu)
			_macroMenu->removeAction(a);
		if (QMenu* sub = a->menu())
			sub->deleteLater();
		a->deleteLater();
	}
	_macroMenuItems.clear();
	for (int id = ID_MACRO; id < ID_MACRO_LIMIT; ++id)
		_handlers.remove(id);
	if (!_macroMenu || _savedMacros.empty())
		return;

	const QList<QAction*> actions = _macroMenu->actions();
	const qsizetype anchor = actions.indexOf(_actions.value(IDM_MACRO_RUNMULTIMACRODLG));
	QAction* before = (anchor >= 0 && anchor + 1 < actions.size()) ? actions.at(anchor + 1) : nullptr;
	auto insert = [&](QAction* a)
	{
		_macroMenu->insertAction(before, a);
		_macroMenuItems << a;
	};
	auto separator = [&]
	{
		auto* sep = new QAction(_macroMenu);
		sep->setSeparator(true);
		insert(sep);
	};
	separator();
	QHash<QString, QMenu*> folders;
	const int count = std::min(static_cast<int>(_savedMacros.size()), ID_MACRO_LIMIT - ID_MACRO);
	for (int i = 0; i < count; ++i)
	{
		const SavedMacro& m = _savedMacros[static_cast<size_t>(i)];
		auto* a = new QAction(m.name, this);
		a->setShortcut(m.keySequence());
		a->setData(ID_MACRO + i);
		connect(a, &QAction::triggered, this, [this, i] { command(ID_MACRO + i); });
		_handlers.insert(ID_MACRO + i, [this, i]
		{
			if (i < static_cast<int>(_savedMacros.size()) && checkShortcutsXml())
				macroPlaySteps(_savedMacros[static_cast<size_t>(i)].steps, 1);
		});
		addAction(a); // the shortcut works with a hidden menu bar too
		if (m.folder.isEmpty())
		{
			insert(a);
			continue;
		}
		QMenu* sub = folders.value(m.folder);
		if (!sub)
		{
			sub = new QMenu(m.folder, _macroMenu);
			folders.insert(m.folder, sub);
			insert(sub->menuAction());
		}
		sub->addAction(a);
		_macroMenuItems << a;
	}
	separator();
	auto* modify = new QAction(QStringLiteral("Modify Shortcut/Delete Macro..."), this);
	connect(modify, &QAction::triggered, this, [this] { command(IDM_SETTING_SHORTCUT_MAPPER_MACRO); });
	insert(modify);
}

// ---------------------------------------------------------------------------
// Run, Tools, Window, Settings, ?
// ---------------------------------------------------------------------------
QString MainWindow::expandRunVariables(const QString& cmd) const
{
	QString out = cmd;
	const QString path = _current ? _current->fullName() : QString();
	const QFileInfo fi(path);
	const sptr_t pos = _edit->ex(SCI_GETCURRENTPOS);
	const sptr_t line = _edit->ex(SCI_LINEFROMPOSITION, pos);
	out.replace(QStringLiteral("$(FULL_CURRENT_PATH)"), path);
	out.replace(QStringLiteral("$(CURRENT_DIRECTORY)"), (_current && !_current->isUntitled()) ? fi.absolutePath() : QDir::homePath());
	out.replace(QStringLiteral("$(FILE_NAME)"), fi.fileName());
	out.replace(QStringLiteral("$(NAME_PART)"), fi.completeBaseName());
	out.replace(QStringLiteral("$(EXT_PART)"), fi.suffix().isEmpty() ? QString() : QStringLiteral(".") + fi.suffix());
	out.replace(QStringLiteral("$(NPP_DIRECTORY)"), QApplication::applicationDirPath());
	out.replace(QStringLiteral("$(NPP_FULL_FILE_PATH)"), QApplication::applicationFilePath());
	out.replace(QStringLiteral("$(CURRENT_WORD)"), _edit->currentWordOrSelection());
	out.replace(QStringLiteral("$(CURRENT_LINE)"), QString::number(line + 1));
	out.replace(QStringLiteral("$(CURRENT_COLUMN)"), QString::number(_edit->ex(SCI_GETCOLUMN, pos) + 1));
	out.replace(QStringLiteral("$(CURRENT_LINESTR)"), QString::fromUtf8(_edit->lineText(line)));
	static const QRegularExpression sysVar(QStringLiteral("\\$\\(SYS\\.([A-Za-z0-9_]+)\\)"));
	QRegularExpressionMatch m;
	while ((m = sysVar.match(out)).hasMatch())
		out.replace(m.capturedStart(), m.capturedLength(), qEnvironmentVariable(m.captured(1).toLatin1().constData()));
	return out;
}

void MainWindow::runDialog()
{
	// RunDlg: the program to run, its history, "..." to browse, "+" for the variables, Save...
	QDialog dlg(this);
	dlg.setObjectName(QStringLiteral("runDlg"));
	dlg.setWindowTitle(QStringLiteral("Run..."));
	auto* root = new QVBoxLayout(&dlg);
	auto* box = new QGroupBox(QStringLiteral("The Program to Run"), &dlg);
	auto* row = new QHBoxLayout(box);
	auto* combo = new QComboBox(box);
	combo->setObjectName(QStringLiteral("runCombo"));
	combo->setEditable(true);
	combo->setInsertPolicy(QComboBox::NoInsert);
	combo->setMinimumWidth(460);
	QStringList history = cmdSettings().value(QStringLiteral("runHistory")).toStringList();
	combo->addItems(history);
	auto* browse = new QPushButton(QStringLiteral("..."), box);
	browse->setFixedWidth(32);
	auto* vars = new QPushButton(QStringLiteral("+"), box);
	vars->setFixedWidth(32);
	row->addWidget(combo, 1);
	row->addWidget(browse);
	row->addWidget(vars);
	root->addWidget(box);
	auto* buttons = new QHBoxLayout;
	auto* runBtn = new QPushButton(QStringLiteral("&Run"), &dlg);
	runBtn->setDefault(true);
	auto* saveBtn = new QPushButton(QStringLiteral("&Save..."), &dlg);
	auto* cancelBtn = new QPushButton(QStringLiteral("&Cancel"), &dlg);
	buttons->addStretch();
	buttons->addWidget(runBtn);
	buttons->addWidget(saveBtn);
	buttons->addWidget(cancelBtn);
	buttons->addStretch();
	root->addLayout(buttons);

	connect(browse, &QPushButton::clicked, &dlg, [&]
	{
		const QString fn = QFileDialog::getOpenFileName(&dlg, QStringLiteral("Program to run"));
		if (!fn.isEmpty())
			combo->setCurrentText(fn.contains(QLatin1Char(' ')) ? QLatin1Char('"') + fn + QLatin1Char('"') : fn);
	});
	connect(vars, &QPushButton::clicked, &dlg, [&]
	{
		// IDR_RUN_DLG_MENU_VARIABLES
		QMenu menu(&dlg);
		const std::pair<const char*, const char*> items[] = {
			{"FULL_CURRENT_PATH", "Full path to active file"}, {"CURRENT_DIRECTORY", "Active file's directory"},
			{"FILE_NAME", "Active file's name"}, {"NAME_PART", "File name without extension"}, {"EXT_PART", "File extension (with .)"},
			{"CURRENT_WORD", "Selected word or word under caret"}, {"NPP_DIRECTORY", "notepad++ directory"},
			{"NPP_FULL_FILE_PATH", "Full path of notepad++"}, {"CURRENT_LINE", "Line number of caret"},
			{"CURRENT_COLUMN", "Column number of caret"}, {"CURRENT_LINESTR", "Current line text"}};
		for (const auto& [var, desc] : items)
		{
			const QString v = QStringLiteral("$(%1)").arg(QString::fromLatin1(var));
			menu.addAction(QStringLiteral("%1\t%2").arg(QString::fromLatin1(var), QString::fromLatin1(desc)), &dlg, [combo, v] { combo->lineEdit()->insert(v); });
		}
		menu.exec(vars->mapToGlobal(QPoint(0, vars->height())));
	});
	connect(cancelBtn, &QPushButton::clicked, &dlg, &QDialog::reject);
	connect(saveBtn, &QPushButton::clicked, &dlg, [&]
	{
		// IDC_BUTTON_SAVE: a command of the Run menu
		const QString cmd = combo->currentText().trimmed();
		if (cmd.isEmpty())
			return;
		SavedCommand c;
		c.command = cmd;
		if (!Macros::editDialog(&dlg, &c, QStringLiteral("Shortcut")))
			return;
		_savedCommands.push_back(c);
		QString error;
		if (!Macros::saveCommands(_savedCommands, &error))
			QMessageBox::warning(&dlg, QStringLiteral("Run"), error);
		rebuildRunMenu();
	});
	connect(runBtn, &QPushButton::clicked, &dlg, [&]
	{
		const QString cmd = combo->currentText().trimmed();
		if (cmd.isEmpty())
			return;
		history.removeAll(cmd);
		history.prepend(cmd);
		while (history.size() > 20)
			history.removeLast();
		cmdSettings().setValue(QStringLiteral("runHistory"), history);
		dlg.accept();
		runCommandLine(cmd);
	});
	combo->setCurrentText(history.isEmpty() ? QString() : history.first());
	combo->lineEdit()->selectAll();
	dlg.exec();
}

void MainWindow::toolHash(int id)
{
	QCryptographicHash::Algorithm algo = QCryptographicHash::Md5;
	QString name = QStringLiteral("MD5");
	if (id == IDM_TOOL_SHA1_GENERATE || id == IDM_TOOL_SHA1_GENERATEFROMFILE || id == IDM_TOOL_SHA1_GENERATEINTOCLIPBOARD)
	{
		algo = QCryptographicHash::Sha1;
		name = QStringLiteral("SHA-1");
	}
	else if (id == IDM_TOOL_SHA256_GENERATE || id == IDM_TOOL_SHA256_GENERATEFROMFILE || id == IDM_TOOL_SHA256_GENERATEINTOCLIPBOARD)
	{
		algo = QCryptographicHash::Sha256;
		name = QStringLiteral("SHA-256");
	}
	else if (id == IDM_TOOL_SHA512_GENERATE || id == IDM_TOOL_SHA512_GENERATEFROMFILE || id == IDM_TOOL_SHA512_GENERATEINTOCLIPBOARD)
	{
		algo = QCryptographicHash::Sha512;
		name = QStringLiteral("SHA-512");
	}

	if (id == IDM_TOOL_MD5_GENERATE || id == IDM_TOOL_SHA1_GENERATE || id == IDM_TOOL_SHA256_GENERATE || id == IDM_TOOL_SHA512_GENERATE)
		NppDialogs::hashGenerate(this, algo, name);
	else if (id == IDM_TOOL_MD5_GENERATEFROMFILE || id == IDM_TOOL_SHA1_GENERATEFROMFILE || id == IDM_TOOL_SHA256_GENERATEFROMFILE || id == IDM_TOOL_SHA512_GENERATEFROMFILE)
		NppDialogs::hashFromFiles(this, algo, name);
	else
	{
		const QByteArray sel = _edit->selectedTextUtf8();
		if (sel.isEmpty())
			return;
		QApplication::clipboard()->setText(QString::fromLatin1(QCryptographicHash::hash(sel, algo).toHex()));
	}
}

void MainWindow::windowSort(int id)
{
	std::vector<Buffer*> bufs = allBuffers();
	auto key = [id, this](Buffer* b) -> QVariant
	{
		switch (id)
		{
			case IDM_WINDOW_SORT_FN_ASC:
			case IDM_WINDOW_SORT_FN_DSC:
				return b->displayName().toLower();
			case IDM_WINDOW_SORT_FP_ASC:
			case IDM_WINDOW_SORT_FP_DSC:
				return b->fullName().toLower();
			case IDM_WINDOW_SORT_FT_ASC:
			case IDM_WINDOW_SORT_FT_DSC:
				return QString::fromUtf8(StyleManager::langInfo(b->lang).shortName).toLower();
			case IDM_WINDOW_SORT_FS_ASC:
			case IDM_WINDOW_SORT_FS_DSC:
			{
				qlonglong len = 0;
				withDocument(b, [&](EditView* v) { len = v->ex(SCI_GETLENGTH); });
				return len;
			}
			default:
				return b->isUntitled() ? QDateTime::currentDateTime() : QFileInfo(b->path).lastModified();
		}
	};
	const bool desc = (id == IDM_WINDOW_SORT_FN_DSC || id == IDM_WINDOW_SORT_FP_DSC || id == IDM_WINDOW_SORT_FT_DSC ||
	                   id == IDM_WINDOW_SORT_FS_DSC || id == IDM_WINDOW_SORT_FD_DSC);
	std::vector<std::pair<QVariant, Buffer*>> keyed;
	for (Buffer* b : bufs)
		keyed.emplace_back(key(b), b);
	std::stable_sort(keyed.begin(), keyed.end(), [desc](const auto& a, const auto& b)
	{
		const auto c = QVariant::compare(a.first, b.first);
		return desc ? c == QPartialOrdering::Greater : c == QPartialOrdering::Less;
	});
	for (int target = 0; target < static_cast<int>(keyed.size()); ++target)
	{
		const int from = indexOf(keyed[static_cast<size_t>(target)].second);
		if (from != target)
			_tabs->moveTab(from, target);
	}
	refreshDocumentList();
}

void MainWindow::windowList()
{
	NppDialogs::windowsList(this);
}

void MainWindow::settingsPreferences()
{
	NppDialogs::preferences(this);
}

void MainWindow::settingsStyleConfigurator()
{
	NppDialogs::styleConfigurator(this);
}

void MainWindow::settingsShortcutMapper()
{
	NppDialogs::shortcutMapper(this);
}

void MainWindow::settingsImportTheme()
{
	const QStringList files = QFileDialog::getOpenFileNames(this, QStringLiteral("Import style theme(s)"), QDir::homePath(),
		QStringLiteral("Theme files (*.xml)"));
	if (files.isEmpty())
		return;
	const QString dir = Npp::configDir() + QStringLiteral("/themes");
	QDir().mkpath(dir);
	int n = 0;
	for (const QString& f : files)
	{
		const QString target = dir + QLatin1Char('/') + QFileInfo(f).fileName();
		QFile::remove(target);
		if (QFile::copy(f, target))
			++n;
	}
	QMessageBox::information(this, QStringLiteral("Import style theme(s)"),
		QStringLiteral("%1 theme(s) imported. Select them in Settings > Style Configurator.").arg(n));
}

void MainWindow::helpCommandLine()
{
	QMessageBox::information(this, QStringLiteral("Command Line Arguments"),
		QStringLiteral("Usage :\n\n"
		               "notepad++ [--help] [-multiInst] [-nosession] [-lLanguage] [-nLineNumber] [-cColumnNumber]\n"
		               "          [-ro] [-alwaysOnTop] [filePath...]\n\n"
		               "--help : This help message\n"
		               "-multiInst : Launch another Notepad++ instance\n"
		               "-nosession : Launch Notepad++ without previous session\n"
		               "-l : Open file or display ghost typing with syntax highlighting of choice (e.g. -lcpp)\n"
		               "-n : Scroll to indicated line on filePath\n"
		               "-c : Scroll to indicated column on filePath\n"
		               "-ro : Make the filePath read-only\n"
		               "-alwaysOnTop : Make Notepad++ always on top\n"
		               "filePath : file or folder name to open (absolute or relative path name)"));
}

void MainWindow::helpAbout()
{
	QMessageBox box(this);
	box.setWindowTitle(QStringLiteral("About Notepad++"));
	box.setIconPixmap(QIcon(QStringLiteral(":/icons/npp.ico")).pixmap(64, 64));
	box.setTextFormat(Qt::RichText);
	box.setText(QStringLiteral("<h3>Notepad++ v%1</h3>"
	                           "<p>Native Linux build (Qt %2)<br>Build time: %3 - %4</p>"
	                           "<p>Home: <a href=\"https://notepad-plus-plus.org/\">https://notepad-plus-plus.org/</a></p>"
	                           "<p>GNU General Public Licence</p>"
	                           "<p>This program is free software; you can redistribute it and/or modify it under the terms of the "
	                           "GNU General Public License as published by the Free Software Foundation; either version 3 of the "
	                           "License, or (at your option) any later version.</p>")
		.arg(Npp::version(), QString::fromLatin1(qVersion()), QStringLiteral(__DATE__), QStringLiteral(__TIME__)));
	box.exec();
}

void MainWindow::helpDebugInfo()
{
	const NppPrefs& p = Npp::prefs();
	QString info;
	info += QStringLiteral("Notepad++ v%1   (%2-bit)\n").arg(Npp::version()).arg(QSysInfo::WordSize);
	info += QStringLiteral("Build time: %1 - %2\n").arg(QStringLiteral(__DATE__), QStringLiteral(__TIME__));
	info += QStringLiteral("Front end: Qt %1 (native Linux)\n").arg(QString::fromLatin1(qVersion()));
	info += QStringLiteral("Scintilla/Lexilla included: %1/%2\n").arg(QStringLiteral(NPP_SCINTILLA_VERSION), QStringLiteral(NPP_LEXILLA_VERSION));
	info += QStringLiteral("Boost Regex included: %1\n").arg(QStringLiteral(NPP_BOOST_VERSION));
	info += QStringLiteral("Path: %1\n").arg(QApplication::applicationFilePath());
	info += QStringLiteral("Command Line: %1\n").arg(QApplication::arguments().mid(1).join(QLatin1Char(' ')));
	info += QStringLiteral("Data dir: %1\n").arg(Npp::dataDir());
	info += QStringLiteral("Settings dir: %1\n").arg(Npp::configDir());
	info += QStringLiteral("Periodic Backup: %1\n").arg(p.sessionSnapshot ? QStringLiteral("ON") : QStringLiteral("OFF"));
	info += QStringLiteral("Multi-instance Mode: %1\n").arg(p.multiInstMode == MULTI_INSTANCE ? QStringLiteral("ON") : p.multiInstMode == SESSION_IN_NEW_INSTANCE ? QStringLiteral("SESSION") : QStringLiteral("OFF"));
	QString detect = p.fileAutoDetectMode == AUTODETECT_DISABLED ? QStringLiteral("cdDisabled") : p.fileAutoDetectMode == AUTODETECT_ALL ? QStringLiteral("cdEnabledNew") : QStringLiteral("cdEnabledOld");
	if (p.fileAutoDetectMode != AUTODETECT_DISABLED && p.updateSilently)
		detect += QStringLiteral(" | cdAutoUpdate");
	if (p.fileAutoDetectMode != AUTODETECT_DISABLED && p.scrollToEndAfterUpdate)
		detect += QStringLiteral(" | cdGo2end");
	info += QStringLiteral("File Status Auto-Detection: %1\n").arg(detect);
	info += QStringLiteral("Dark Mode: %1\n").arg(Npp::isDarkDesktop() ? QStringLiteral("ON") : QStringLiteral("OFF"));
	info += QStringLiteral("Display Info: %1\n").arg(QGuiApplication::platformName());
	info += QStringLiteral("OS Name: %1\n").arg(QSysInfo::prettyProductName());
	info += QStringLiteral("OS Kernel: %1 %2\n").arg(QSysInfo::kernelType(), QSysInfo::kernelVersion());
	{
		const QStringList modules = _pluginsManager->loadedModules();
		info += QStringLiteral("Plugins: %1\n").arg(modules.isEmpty() ? QStringLiteral("none") : modules.join(QLatin1Char(' ')));
	}

	QMessageBox box(this);
	box.setWindowTitle(QStringLiteral("Debug Info"));
	box.setText(info);
	box.setTextInteractionFlags(Qt::TextSelectableByMouse);
	QPushButton* copy = box.addButton(QStringLiteral("Copy debug info into clipboard"), QMessageBox::ActionRole);
	box.addButton(QMessageBox::Close);
	box.exec();
	if (box.clickedButton() == copy)
		QApplication::clipboard()->setText(info);
}
