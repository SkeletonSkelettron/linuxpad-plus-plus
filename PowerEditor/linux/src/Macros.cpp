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

#include "Macros.h"

#include <algorithm>

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QMessageAuthenticationCode>
#include <QSettings>
#include <QSysInfo>
#include <QFile>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include <pugixml.hpp>

namespace
{
	// macro step types of shortcuts.xml (recordedMacroStep::MacroTypeIndex)
	enum MacroType { mtUseLParameter = 0, mtUseSParameter = 1, mtMenuCommand = 2, mtSavedSnR = 3 };

	SavedMacro defaultMacro()
	{
		// SHORTCUTSXML_DEFAULT_CONTENT of NppConstants.h
		SavedMacro m;
		m.name = QStringLiteral("Trim Trailing Space and Save");
		m.alt = true;
		m.shift = true;
		m.key = 83; // S
		for (int cmd : {IDM_EDIT_TRIMTRAILING, IDM_FILE_SAVE})
		{
			MacroStep s;
			s.kind = MacroStep::Menu;
			s.message = static_cast<unsigned int>(cmd);
			m.steps.push_back(s);
		}
		return m;
	}

	const char* yesNo(bool b)
	{
		return b ? "yes" : "no";
	}
}

const std::vector<Macros::NamedKey>& Macros::namedKeys()
{
	static const std::vector<NamedKey> keys = []
	{
		std::vector<NamedKey> k{
			{QStringLiteral("None"), 0, 0, false},
			{QStringLiteral("Backspace"), 0x08, Qt::Key_Backspace, false},
			{QStringLiteral("Tab"), 0x09, Qt::Key_Tab, false},
			{QStringLiteral("Enter"), 0x0D, Qt::Key_Return, false},
			{QStringLiteral("Esc"), 0x1B, Qt::Key_Escape, false},
			{QStringLiteral("Spacebar"), 0x20, Qt::Key_Space, false},
			{QStringLiteral("Page up"), 0x21, Qt::Key_PageUp, false},
			{QStringLiteral("Page down"), 0x22, Qt::Key_PageDown, false},
			{QStringLiteral("End"), 0x23, Qt::Key_End, false},
			{QStringLiteral("Home"), 0x24, Qt::Key_Home, false},
			{QStringLiteral("Left"), 0x25, Qt::Key_Left, false},
			{QStringLiteral("Up"), 0x26, Qt::Key_Up, false},
			{QStringLiteral("Right"), 0x27, Qt::Key_Right, false},
			{QStringLiteral("Down"), 0x28, Qt::Key_Down, false},
			{QStringLiteral("INS"), 0x2D, Qt::Key_Insert, false},
			{QStringLiteral("DEL"), 0x2E, Qt::Key_Delete, false}};
		for (int i = 0; i < 10; ++i)
			k.push_back({QString::number(i), 0x30 + i, Qt::Key_0 + i, false});
		for (int i = 0; i < 26; ++i)
			k.push_back({QString(QChar('A' + i)), 0x41 + i, Qt::Key_A + i, false});
		for (int i = 0; i < 10; ++i)
			k.push_back({QStringLiteral("Numpad %1").arg(i), 0x60 + i, Qt::Key_0 + i, true});
		k.push_back({QStringLiteral("Num *"), 0x6A, Qt::Key_Asterisk, true});
		k.push_back({QStringLiteral("Num +"), 0x6B, Qt::Key_Plus, true});
		k.push_back({QStringLiteral("Num -"), 0x6D, Qt::Key_Minus, true});
		k.push_back({QStringLiteral("Num ."), 0x6E, Qt::Key_Period, true});
		k.push_back({QStringLiteral("Num /"), 0x6F, Qt::Key_Slash, true});
		for (int i = 0; i < 24; ++i)
			k.push_back({QStringLiteral("F%1").arg(i + 1), 0x70 + i, Qt::Key_F1 + i, false});
		k.push_back({QStringLiteral("~"), 0xC0, Qt::Key_QuoteLeft, false});
		k.push_back({QStringLiteral("-"), 0xBD, Qt::Key_Minus, false});
		k.push_back({QStringLiteral("="), 0xBB, Qt::Key_Equal, false});
		k.push_back({QStringLiteral("["), 0xDB, Qt::Key_BracketLeft, false});
		k.push_back({QStringLiteral("]"), 0xDD, Qt::Key_BracketRight, false});
		k.push_back({QStringLiteral(";"), 0xBA, Qt::Key_Semicolon, false});
		k.push_back({QStringLiteral("'"), 0xDE, Qt::Key_Apostrophe, false});
		k.push_back({QStringLiteral("\\"), 0xDC, Qt::Key_Backslash, false});
		k.push_back({QStringLiteral(","), 0xBC, Qt::Key_Comma, false});
		k.push_back({QStringLiteral("."), 0xBE, Qt::Key_Period, false});
		k.push_back({QStringLiteral("/"), 0xBF, Qt::Key_Slash, false});
		k.push_back({QStringLiteral("<>"), 0xE2, Qt::Key_Less, false});
		return k;
	}();
	return keys;
}

QKeySequence ShortcutSpec::keySequence() const
{
	if (!key)
		return {};
	for (const Macros::NamedKey& k : Macros::namedKeys())
	{
		if (k.vk != key)
			continue;
		int combo = k.qtKey;
		if (ctrl)
			combo |= Qt::ControlModifier;
		if (alt)
			combo |= Qt::AltModifier;
		if (shift)
			combo |= Qt::ShiftModifier;
		if (k.keypad)
			combo |= Qt::KeypadModifier;
		return QKeySequence(combo);
	}
	return {};
}

QString Macros::shortcutsPath()
{
	return Npp::configDir() + QStringLiteral("/shortcuts.xml");
}

std::vector<SavedMacro> Macros::load()
{
	// NppParameters::getMacrosFromXmlTree() / getActions()
	std::vector<SavedMacro> macros;
	pugi::xml_document doc;
	if (!QFileInfo::exists(shortcutsPath()) || !doc.load_file(QFile::encodeName(shortcutsPath()).constData()))
	{
		macros.push_back(defaultMacro());
		return macros;
	}
	for (pugi::xml_node m = doc.child("NotepadPlus").child("Macros").child("Macro"); m; m = m.next_sibling("Macro"))
	{
		SavedMacro sm;
		sm.name = QString::fromUtf8(m.attribute("name").value());
		if (sm.name.isEmpty())
			continue;
		sm.folder = QString::fromUtf8(m.attribute("FolderName").value());
		sm.ctrl = QString::fromLatin1(m.attribute("Ctrl").value()) == QStringLiteral("yes");
		sm.alt = QString::fromLatin1(m.attribute("Alt").value()) == QStringLiteral("yes");
		sm.shift = QString::fromLatin1(m.attribute("Shift").value()) == QStringLiteral("yes");
		sm.key = m.attribute("Key").as_int(0);
		for (pugi::xml_node a = m.child("Action"); a; a = a.next_sibling("Action"))
		{
			MacroStep s;
			const int type = a.attribute("type").as_int(-1);
			if (type == mtMenuCommand)
			{
				s.kind = MacroStep::Menu;
				s.message = static_cast<unsigned int>(a.attribute("wParam").as_ullong(0));
			}
			else if (type == mtUseLParameter || type == mtUseSParameter)
			{
				s.kind = MacroStep::Scintilla;
				s.message = a.attribute("message").as_uint(0);
				s.wParam = static_cast<uptr_t>(a.attribute("wParam").as_ullong(0));
				s.lParam = static_cast<sptr_t>(a.attribute("lParam").as_llong(0));
				if (type == mtUseSParameter)
				{
					s.text = QByteArray(a.attribute("sParam").value());
					s.hasText = true;
				}
			}
			else
			{
				continue; // mtSavedSnR: Find / Replace steps are not recorded by this port
			}
			sm.steps.push_back(s);
		}
		macros.push_back(sm);
	}
	return macros;
}

namespace
{
	void writeMacros(pugi::xml_node macrosNode, const std::vector<SavedMacro>& macros)
	{
		for (const SavedMacro& m : macros)
		{
			pugi::xml_node mn = macrosNode.append_child("Macro");
			mn.append_attribute("name") = m.name.toUtf8().constData();
			mn.append_attribute("Ctrl") = yesNo(m.ctrl);
			mn.append_attribute("Alt") = yesNo(m.alt);
			mn.append_attribute("Shift") = yesNo(m.shift);
			mn.append_attribute("Key") = m.key;
			if (!m.folder.isEmpty())
				mn.append_attribute("FolderName") = m.folder.toUtf8().constData();
			for (const MacroStep& s : m.steps)
			{
				pugi::xml_node a = mn.append_child("Action");
				if (s.kind == MacroStep::Menu)
				{
					a.append_attribute("type") = static_cast<int>(mtMenuCommand);
					a.append_attribute("message") = 0;
					a.append_attribute("wParam") = s.message;
					a.append_attribute("lParam") = 0;
					a.append_attribute("sParam") = "";
				}
				else
				{
					a.append_attribute("type") = static_cast<int>(s.hasText ? mtUseSParameter : mtUseLParameter);
					a.append_attribute("message") = s.message;
					a.append_attribute("wParam") = static_cast<unsigned long long>(s.wParam);
					a.append_attribute("lParam") = s.hasText ? 0LL : static_cast<long long>(s.lParam);
					a.append_attribute("sParam") = s.hasText ? s.text.constData() : "";
				}
			}
		}
	}

	void writeCommands(pugi::xml_node node, const std::vector<SavedCommand>& commands)
	{
		// NppParameters::insertUserCmd()
		for (const SavedCommand& c : commands)
		{
			pugi::xml_node cn = node.append_child("Command");
			cn.append_attribute("name") = c.name.toUtf8().constData();
			cn.append_attribute("Ctrl") = yesNo(c.ctrl);
			cn.append_attribute("Alt") = yesNo(c.alt);
			cn.append_attribute("Shift") = yesNo(c.shift);
			cn.append_attribute("Key") = c.key;
			if (!c.folder.isEmpty())
				cn.append_attribute("FolderName") = c.folder.toUtf8().constData();
			cn.append_child(pugi::node_pcdata).set_value(c.command.toUtf8().constData());
		}
	}

	std::vector<SavedCommand> defaultCommands()
	{
		// SHORTCUTSXML_DEFAULT_CONTENT of NppConstants.h
		auto make = [](const char* name, int key, const char* cmd)
		{
			SavedCommand c;
			c.name = QString::fromUtf8(name);
			c.alt = true;
			c.key = key;
			c.command = QString::fromUtf8(cmd);
			return c;
		};
		return {make("Get PHP help", 112, "https://www.php.net/$(CURRENT_WORD)"),
		        make("Wikipedia Search", 114, "https://en.wikipedia.org/wiki/Special:Search?search=$(CURRENT_WORD)"),
		        make("Open selected file path in new instance", 117, "$(NPP_FULL_FILE_PATH) $(CURRENT_WORD) -nosession -multiInst")};
	}

	// shortcuts.xml, or a new one with the default macros and commands of Notepad++
	void openShortcuts(pugi::xml_document& doc)
	{
		const QString path = Macros::shortcutsPath();
		if (QFileInfo::exists(path) && doc.load_file(QFile::encodeName(path).constData()) && doc.child("NotepadPlus"))
			return;
		doc.reset();
		pugi::xml_node decl = doc.append_child(pugi::node_declaration);
		decl.append_attribute("version") = "1.0";
		decl.append_attribute("encoding") = "UTF-8";
		pugi::xml_node root = doc.append_child("NotepadPlus");
		root.append_child("InternalCommands");
		writeMacros(root.append_child("Macros"), {defaultMacro()});
		writeCommands(root.append_child("UserDefinedCommands"), defaultCommands());
		root.append_child("PluginCommands");
		root.append_child("ScintillaKeys");
	}

	pugi::xml_node clearedSection(pugi::xml_document& doc, const char* name)
	{
		pugi::xml_node root = doc.child("NotepadPlus");
		pugi::xml_node node = root.child(name);
		if (!node)
			return root.append_child(name);
		while (pugi::xml_node c = node.first_child())
			node.remove_child(c);
		return node;
	}

	bool saveShortcuts(pugi::xml_document& doc, QString* error)
	{
		const QString path = Macros::shortcutsPath();
		QDir().mkpath(QFileInfo(path).absolutePath());
		if (!doc.save_file(QFile::encodeName(path).constData(), "    ", pugi::format_default, pugi::encoding_utf8))
		{
			if (error)
				*error = QStringLiteral("Can not write %1").arg(path);
			return false;
		}
		Macros::validateShortcutsXml(); // written by Notepad++: trusted
		return true;
	}
}

namespace
{
	QByteArray shortcutsHmac()
	{
		// computeHMAC(getMachineGUID(), file content)
		QFile f(Macros::shortcutsPath());
		if (!f.open(QIODevice::ReadOnly))
			return {};
		QByteArray key;
		for (const char* idFile : {"/etc/machine-id", "/var/lib/dbus/machine-id"})
		{
			QFile id(QString::fromLatin1(idFile));
			if (id.open(QIODevice::ReadOnly))
			{
				key = id.readAll().trimmed();
				break;
			}
		}
		if (key.isEmpty())
			key = QSysInfo::machineUniqueId();
		return QMessageAuthenticationCode::hash(f.readAll(), key, QCryptographicHash::Sha256).toHex();
	}

	QSettings& hmacSettings()
	{
		static QSettings s(Npp::configDir() + QStringLiteral("/config.ini"), QSettings::IniFormat);
		return s;
	}
}

bool Macros::isShortcutsXmlTrusted()
{
	if (!QFileInfo::exists(shortcutsPath()))
		return true; // the defaults of Notepad++
	hmacSettings().sync();
	const QByteArray stored = hmacSettings().value(QStringLiteral("shortcutsXmlHmac")).toByteArray();
	return !stored.isEmpty() && stored == shortcutsHmac();
}

void Macros::validateShortcutsXml()
{
	if (!QFileInfo::exists(shortcutsPath()))
		return;
	hmacSettings().setValue(QStringLiteral("shortcutsXmlHmac"), shortcutsHmac());
	hmacSettings().sync();
}

namespace
{
	struct ScintillaKeyTableEntry
	{
		const char* name;
		int sciCmd;
		bool ctrl;
		bool alt;
		bool shift;
		int vk;
		int menuCmd;
	};
#include "ScintillaKeyTable.inc"

	std::vector<ScintillaKeyCmd> g_sciKeys;
	bool g_sciKeysLoaded = false;
}

void Macros::reloadScintillaKeys()
{
	// NppParameters::initScintillaKeys() + getScintKeysFromXmlTree()
	g_sciKeys.clear();
	for (const ScintillaKeyTableEntry& e : kScintillaKeyTable)
	{
		const ScintillaKeyCombo k{e.ctrl, e.alt, e.shift, e.vk};
		if (e.name[0] == '\0' && !g_sciKeys.empty() && g_sciKeys.back().sciCmd == e.sciCmd)
		{
			g_sciKeys.back().defaults.push_back(k); // another key of the command above
			continue;
		}
		ScintillaKeyCmd c;
		c.name = QString::fromLatin1(e.name);
		c.sciCmd = e.sciCmd;
		c.menuCmd = e.menuCmd;
		c.defaults.push_back(k);
		g_sciKeys.push_back(c);
	}
	for (ScintillaKeyCmd& c : g_sciKeys)
		c.keys = c.defaults;
	g_sciKeysLoaded = true;

	pugi::xml_document doc;
	if (!QFileInfo::exists(shortcutsPath()) || !doc.load_file(QFile::encodeName(shortcutsPath()).constData()))
		return;
	auto combo = [](const pugi::xml_node& n)
	{
		ScintillaKeyCombo k;
		k.ctrl = QString::fromLatin1(n.attribute("Ctrl").value()) == QStringLiteral("yes");
		k.alt = QString::fromLatin1(n.attribute("Alt").value()) == QStringLiteral("yes");
		k.shift = QString::fromLatin1(n.attribute("Shift").value()) == QStringLiteral("yes");
		k.vk = n.attribute("Key").as_int(0);
		return k;
	};
	for (pugi::xml_node n = doc.child("NotepadPlus").child("ScintillaKeys").child("ScintKey"); n; n = n.next_sibling("ScintKey"))
	{
		const int sci = n.attribute("ScintID").as_int(0);
		const int menu = n.attribute("menuCmdID").as_int(0);
		for (ScintillaKeyCmd& c : g_sciKeys)
		{
			if (c.sciCmd != sci || c.menuCmd != menu)
				continue;
			c.keys = {combo(n)};
			for (pugi::xml_node next = n.child("NextKey"); next; next = next.next_sibling("NextKey"))
				c.keys.push_back(combo(next));
			break;
		}
	}
}

std::vector<ScintillaKeyCmd>& Macros::scintillaKeys()
{
	if (!g_sciKeysLoaded)
		reloadScintillaKeys();
	return g_sciKeys;
}

sptr_t Macros::scintillaKeyDef(const ScintillaKeyCombo& k)
{
	// keyTranslate() of shortcut.h, with the other punctuation keys of a Qt keyboard
	int key = k.vk;
	switch (k.vk)
	{
		case 0x28: key = SCK_DOWN; break;
		case 0x26: key = SCK_UP; break;
		case 0x25: key = SCK_LEFT; break;
		case 0x27: key = SCK_RIGHT; break;
		case 0x24: key = SCK_HOME; break;
		case 0x23: key = SCK_END; break;
		case 0x21: key = SCK_PRIOR; break;
		case 0x22: key = SCK_NEXT; break;
		case 0x2E: key = SCK_DELETE; break;
		case 0x2D: key = SCK_INSERT; break;
		case 0x1B: key = SCK_ESCAPE; break;
		case 0x08: key = SCK_BACK; break;
		case 0x09: key = SCK_TAB; break;
		case 0x0D: key = SCK_RETURN; break;
		case 0x6B: key = SCK_ADD; break;
		case 0x6D: key = SCK_SUBTRACT; break;
		case 0x6F: key = SCK_DIVIDE; break;
		case 0xBF: key = '/'; break;
		case 0xC0: key = '`'; break;
		case 0xDB: key = '['; break;
		case 0xDC: key = '\\'; break;
		case 0xDD: key = ']'; break;
		case 0xBA: key = ';'; break;
		case 0xBB: key = '='; break;
		case 0xBC: key = ','; break;
		case 0xBD: key = '-'; break;
		case 0xBE: key = '.'; break;
		case 0xDE: key = '\''; break;
		default: break;
	}
	const int mods = (k.ctrl ? SCMOD_CTRL : 0) | (k.alt ? SCMOD_ALT : 0) | (k.shift ? SCMOD_SHIFT : 0);
	return key + (static_cast<sptr_t>(mods) << 16);
}

QString Macros::keyText(const ScintillaKeyCombo& k)
{
	if (!k.vk)
		return QString();
	QString name;
	for (const NamedKey& nk : namedKeys())
	{
		if (nk.vk == k.vk)
			name = nk.name;
	}
	QString s;
	if (k.ctrl)
		s += QStringLiteral("Ctrl+");
	if (k.alt)
		s += QStringLiteral("Alt+");
	if (k.shift)
		s += QStringLiteral("Shift+");
	return s + name;
}

bool Macros::keyComboFromSequence(const QKeySequence& seq, ScintillaKeyCombo* k)
{
	if (seq.isEmpty())
		return false;
	const QKeyCombination c = seq[0];
	const bool keypad = c.keyboardModifiers() & Qt::KeypadModifier;
	for (const NamedKey& nk : namedKeys())
	{
		if (nk.vk && nk.qtKey == c.key() && nk.keypad == keypad)
		{
			k->ctrl = c.keyboardModifiers() & Qt::ControlModifier;
			k->alt = c.keyboardModifiers() & Qt::AltModifier;
			k->shift = c.keyboardModifiers() & Qt::ShiftModifier;
			k->vk = nk.vk;
			return true;
		}
	}
	return false;
}

bool Macros::save(const std::vector<SavedMacro>& macros, QString* error)
{
	// NppParameters::insertMacro(): the other parts of shortcuts.xml are kept
	pugi::xml_document doc;
	openShortcuts(doc);
	writeMacros(clearedSection(doc, "Macros"), macros);
	return saveShortcuts(doc, error);
}

std::vector<SavedCommand> Macros::loadCommands()
{
	// NppParameters::getUserCmdsFromXmlTree()
	pugi::xml_document doc;
	if (!QFileInfo::exists(shortcutsPath()) || !doc.load_file(QFile::encodeName(shortcutsPath()).constData()))
		return defaultCommands();
	std::vector<SavedCommand> commands;
	for (pugi::xml_node n = doc.child("NotepadPlus").child("UserDefinedCommands").child("Command"); n; n = n.next_sibling("Command"))
	{
		SavedCommand c;
		c.name = QString::fromUtf8(n.attribute("name").value());
		c.command = QString::fromUtf8(n.child_value());
		if (c.name.isEmpty() || c.command.isEmpty())
			continue;
		c.folder = QString::fromUtf8(n.attribute("FolderName").value());
		c.ctrl = QString::fromLatin1(n.attribute("Ctrl").value()) == QStringLiteral("yes");
		c.alt = QString::fromLatin1(n.attribute("Alt").value()) == QStringLiteral("yes");
		c.shift = QString::fromLatin1(n.attribute("Shift").value()) == QStringLiteral("yes");
		c.key = n.attribute("Key").as_int(0);
		commands.push_back(c);
	}
	return commands;
}

bool Macros::saveScintillaKeys(QString* error)
{
	// NppParameters::insertScintKey(): the commands whose keys are not the default ones
	pugi::xml_document doc;
	openShortcuts(doc);
	pugi::xml_node node = clearedSection(doc, "ScintillaKeys");
	auto setKey = [](pugi::xml_node n, const ScintillaKeyCombo& k)
	{
		n.append_attribute("Ctrl") = yesNo(k.ctrl);
		n.append_attribute("Alt") = yesNo(k.alt);
		n.append_attribute("Shift") = yesNo(k.shift);
		n.append_attribute("Key") = k.vk;
	};
	for (const ScintillaKeyCmd& c : scintillaKeys())
	{
		if (c.keys == c.defaults)
			continue;
		pugi::xml_node k = node.append_child("ScintKey");
		k.append_attribute("ScintID") = c.sciCmd;
		k.append_attribute("menuCmdID") = c.menuCmd;
		setKey(k, c.keys.empty() ? ScintillaKeyCombo{} : c.keys.front());
		for (size_t i = 1; i < c.keys.size(); ++i)
			setKey(k.append_child("NextKey"), c.keys[i]);
	}
	return saveShortcuts(doc, error);
}

bool Macros::saveCommands(const std::vector<SavedCommand>& commands, QString* error)
{
	pugi::xml_document doc;
	openShortcuts(doc);
	writeCommands(clearedSection(doc, "UserDefinedCommands"), commands);
	return saveShortcuts(doc, error);
}

bool Macros::editDialog(QWidget* parent, ShortcutSpec* macro, const QString& title)
{
	// Shortcut::run_dlgProc(): IDD_SHORTCUT_DLG
	QDialog dlg(parent);
	dlg.setObjectName(QStringLiteral("macroShortcutDlg"));
	dlg.setWindowTitle(title);
	auto* form = new QFormLayout;
	auto* name = new QLineEdit(macro->name, &dlg);
	name->setObjectName(QStringLiteral("macroName"));
	form->addRow(QStringLiteral("Name:"), name);
	auto* mods = new QHBoxLayout;
	auto* ctrl = new QCheckBox(QStringLiteral("CTRL"), &dlg);
	auto* alt = new QCheckBox(QStringLiteral("ALT"), &dlg);
	auto* shift = new QCheckBox(QStringLiteral("SHIFT"), &dlg);
	ctrl->setChecked(macro->ctrl);
	alt->setChecked(macro->alt);
	shift->setChecked(macro->shift);
	auto* key = new QComboBox(&dlg);
	for (const NamedKey& k : namedKeys())
		key->addItem(k.name, k.vk);
	key->setCurrentIndex(std::max(0, key->findData(macro->key)));
	mods->addWidget(ctrl);
	mods->addWidget(alt);
	mods->addWidget(shift);
	mods->addWidget(new QLabel(QStringLiteral("+"), &dlg));
	mods->addWidget(key, 1);
	form->addRow(mods);
	auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
	auto* root = new QVBoxLayout(&dlg);
	root->addLayout(form);
	root->addWidget(buttons);
	auto updateOk = [&] { buttons->button(QDialogButtonBox::Ok)->setEnabled(!name->text().trimmed().isEmpty()); };
	QObject::connect(name, &QLineEdit::textChanged, &dlg, updateOk);
	QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
	QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
	updateOk();
	if (dlg.exec() != QDialog::Accepted)
		return false;
	macro->name = name->text().trimmed();
	macro->ctrl = ctrl->isChecked();
	macro->alt = alt->isChecked();
	macro->shift = shift->isChecked();
	macro->key = key->currentData().toInt();
	return true;
}

template <typename T>
static bool manageShortcuts(QWidget* parent, std::vector<T>* macros, const QString& title, const QString& deleteTitle)
{
	// Shortcut Mapper, Macros tab: Modify and Delete
	QDialog dlg(parent);
	dlg.setObjectName(QStringLiteral("macroManagerDlg"));
	dlg.setWindowTitle(title);
	dlg.resize(560, 360);
	auto* table = new QTableWidget(&dlg);
	table->setColumnCount(2);
	table->setHorizontalHeaderLabels({QStringLiteral("Name"), QStringLiteral("Shortcut")});
	table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
	table->setSelectionBehavior(QAbstractItemView::SelectRows);
	table->setSelectionMode(QAbstractItemView::SingleSelection);
	table->setEditTriggers(QAbstractItemView::NoEditTriggers);
	table->verticalHeader()->hide();
	std::vector<T> edited = *macros;
	auto fill = [&]
	{
		table->setRowCount(static_cast<int>(edited.size()));
		for (int r = 0; r < static_cast<int>(edited.size()); ++r)
		{
			const T& m = edited[static_cast<size_t>(r)];
			table->setItem(r, 0, new QTableWidgetItem(m.folder.isEmpty() ? m.name : m.folder + QStringLiteral(" / ") + m.name));
			table->setItem(r, 1, new QTableWidgetItem(m.keySequence().toString(QKeySequence::NativeText)));
		}
	};
	fill();
	auto* modify = new QPushButton(QStringLiteral("Modify"), &dlg);
	auto* remove = new QPushButton(QStringLiteral("Delete"), &dlg);
	auto* close = new QPushButton(QStringLiteral("Close"), &dlg);
	auto* buttons = new QHBoxLayout;
	buttons->addWidget(modify);
	buttons->addWidget(remove);
	buttons->addStretch();
	buttons->addWidget(close);
	auto* root = new QVBoxLayout(&dlg);
	root->addWidget(table, 1);
	root->addLayout(buttons);
	bool changed = false;
	QObject::connect(modify, &QPushButton::clicked, &dlg, [&]
	{
		const int r = table->currentRow();
		if (r < 0)
			return;
		if (Macros::editDialog(&dlg, &edited[static_cast<size_t>(r)], QStringLiteral("Shortcut")))
		{
			changed = true;
			fill();
			table->selectRow(r);
		}
	});
	QObject::connect(table, &QTableWidget::cellDoubleClicked, modify, &QPushButton::click);
	QObject::connect(remove, &QPushButton::clicked, &dlg, [&]
	{
		const int r = table->currentRow();
		if (r < 0)
			return;
		if (QMessageBox::question(&dlg, deleteTitle, QStringLiteral("Are you sure you want to delete this shortcut?")) != QMessageBox::Yes)
			return;
		edited.erase(edited.begin() + r);
		changed = true;
		fill();
	});
	QObject::connect(close, &QPushButton::clicked, &dlg, &QDialog::accept);
	dlg.exec();
	if (changed)
		*macros = edited;
	return changed;
}

bool Macros::manageDialog(QWidget* parent, std::vector<SavedMacro>* macros)
{
	// Shortcut Mapper, Macros tab
	return manageShortcuts(parent, macros, QStringLiteral("Shortcut Mapper - Macros"), QStringLiteral("Delete Macro"));
}

bool Macros::manageDialog(QWidget* parent, std::vector<SavedCommand>* commands)
{
	// Shortcut Mapper, Run commands tab
	return manageShortcuts(parent, commands, QStringLiteral("Shortcut Mapper - Run commands"), QStringLiteral("Delete Command"));
}
