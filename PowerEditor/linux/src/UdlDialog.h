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

// Language > User Defined Language > Define your language...: Qt counterpart of
// ScintillaComponent/UserDefineDialog.cpp (the four tabs, the language list and the Styler dialog).

#pragma once

#include <QDialog>
#include <QHash>
#include <QString>

class MainWindow;
class QCheckBox;
class QComboBox;
class QLineEdit;
class QPlainTextEdit;
class QRadioButton;
class QTabWidget;
class QTimer;
struct UserLang;

class UdlDialog : public QDialog
{
	Q_OBJECT

public:
	explicit UdlDialog(MainWindow* mw);

	void showFor(const QString& udlName); // the language of the current document, or "User Defined Language"
	UserLang* current() const { return _lang; }
	QComboBox* langCombo() const { return _langCombo; }
	QTabWidget* tabs() const { return _tabs; }

	// for the self test: the field of a keyword list
	QPlainTextEdit* keywordsEdit(int group) const { return _keywords[group]; }
	QLineEdit* field(const QString& name) const { return _fields.value(name); }

	// the actions of the buttons, without their dialogs
	bool createLang(const QString& name, bool copyCurrent);
	bool renameLang(const QString& name);
	bool removeLang();

	// Styler dialog (StylerDlg): true when the style changed
	static bool editStyle(QWidget* parent, UserLang* lang, int styleId, bool nesting);

protected:
	void closeEvent(QCloseEvent* event) override;
	void hideEvent(QHideEvent* event) override;

private:
	void buildUi();
	void fillCombo(const QString& select);
	void selectLang(int index);
	void updateDlg();       // the fields from the language (updateDlg() of the four dialogs)
	void retrieve();        // the language from the fields (setKeywords2List())
	void changed();         // apply to the documents, mark the language for saving
	void apply();

	MainWindow* _mw;
	UserLang* _lang = nullptr;
	bool _updating = false;
	QComboBox* _langCombo = nullptr;
	QLineEdit* _ext = nullptr;
	QCheckBox* _ignoreCase = nullptr;
	QTabWidget* _tabs = nullptr;
	QCheckBox* _foldCompact = nullptr;
	QCheckBox* _foldComments = nullptr;
	QPlainTextEdit* _keywords[8] = {};
	QCheckBox* _prefix[8] = {};
	QRadioButton* _lcPos[3] = {};
	QRadioButton* _decSep[3] = {};
	QPlainTextEdit* _operators1 = nullptr;
	QPlainTextEdit* _operators2 = nullptr;
	QPlainTextEdit* _folders[9] = {}; // code1 open/middle/close, code2, comment
	QHash<QString, QLineEdit*> _fields; // comments, numbers, delimiters
	QTimer* _applyTimer = nullptr;
};
