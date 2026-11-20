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

// Macro > Save Current Recorded Macro and the saved macros of the Macro menu.
// Stored like Notepad++ in <Macros> of shortcuts.xml in the settings folder (the
// Windows file can be copied there), with the Windows key codes (shortcut.cpp).

#pragma once

#include <vector>

#include <QKeySequence>
#include <QString>

#include "MainWindow.h"

// command ids of the saved macros and of the saved Run commands (resource.h)
inline constexpr int ID_MACRO = 20000;
inline constexpr int ID_MACRO_LIMIT = 20499;
inline constexpr int ID_USER_CMD = 21000;
inline constexpr int ID_USER_CMD_LIMIT = 21499;

// name and shortcut of a saved macro or Run command (Shortcut of shortcut.h)
struct ShortcutSpec
{
	QString name;
	QString folder; // FolderName: submenu of the Macro / Run menu
	bool ctrl = false;
	bool alt = false;
	bool shift = false;
	int key = 0; // Windows virtual key code, 0: no shortcut

	QKeySequence keySequence() const;
};

struct SavedMacro : ShortcutSpec
{
	std::vector<MacroStep> steps;
};

// Run > Run... > Save...: <UserDefinedCommands> of shortcuts.xml
struct SavedCommand : ShortcutSpec
{
	QString command;
};

// Settings > Shortcut Mapper > Scintilla commands (ScintillaKeyMap): <ScintillaKeys> of shortcuts.xml
struct ScintillaKeyCombo
{
	bool ctrl = false;
	bool alt = false;
	bool shift = false;
	int vk = 0; // Windows virtual key code, 0: none
	bool operator==(const ScintillaKeyCombo& o) const { return ctrl == o.ctrl && alt == o.alt && shift == o.shift && vk == o.vk; }
};

struct ScintillaKeyCmd
{
	QString name;
	int sciCmd = 0;
	int menuCmd = 0; // the menu command sharing the shortcut (its key is set in the Main menu tab)
	std::vector<ScintillaKeyCombo> defaults;
	std::vector<ScintillaKeyCombo> keys;
};

namespace Macros
{
	std::vector<ScintillaKeyCmd>& scintillaKeys(); // scintKeyDefs of Parameters.cpp, changed by shortcuts.xml
	void reloadScintillaKeys();
	bool saveScintillaKeys(QString* error);           // only the changed commands, like Windows
	sptr_t scintillaKeyDef(const ScintillaKeyCombo& k); // ScintillaKeyMap::toKeyDef()
	QString keyText(const ScintillaKeyCombo& k);
	bool keyComboFromSequence(const QKeySequence& seq, ScintillaKeyCombo* k);

	QString shortcutsPath();
	std::vector<SavedMacro> load();                       // the default "Trim Trailing Space and Save" without a file
	bool save(const std::vector<SavedMacro>& macros, QString* error); // only <Macros> is rewritten
	std::vector<SavedCommand> loadCommands(); // the default commands of Notepad++ without a file
	bool saveCommands(const std::vector<SavedCommand>& commands, QString* error); // only <UserDefinedCommands>

	// shortcuts.xml security validation: the HMAC of the file (key: the machine id) is kept in
	// config.ini; the saved macros and commands run only while the file matches it
	bool isShortcutsXmlTrusted();
	void validateShortcutsXml(); // Run > Validate shortcuts.xml, and after Notepad++ wrote the file

	// the Shortcut dialog of the Windows version: name, Ctrl / Alt / Shift and a key
	bool editDialog(QWidget* parent, ShortcutSpec* spec, const QString& title);
	// Modify Shortcut/Delete Macro... / Modify Shortcut/Delete Command...: true when something changed
	bool manageDialog(QWidget* parent, std::vector<SavedMacro>* macros);
	bool manageDialog(QWidget* parent, std::vector<SavedCommand>* commands);

	struct NamedKey
	{
		QString name;
		int vk;
		int qtKey;
		bool keypad;
	};
	const std::vector<NamedKey>& namedKeys(); // namedKeyArray of shortcut.cpp
}
