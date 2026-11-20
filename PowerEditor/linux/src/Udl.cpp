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

#include "Udl.h"

#include <algorithm>
#include <cstring>
#include <unordered_map>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

#include <pugixml.hpp>

#include "ILexer.h"
#include "Lexilla.h"
#include "ScintillaEditBase.h"

namespace
{
	constexpr COLORREF black = rgb(0, 0, 0);
	constexpr COLORREF white = rgb(0xFF, 0xFF, 0xFF);

	// GlobalMappers of UserDefineDialog.h: every name of the keyword lists and styles, old ones included
	struct Mappers
	{
		std::unordered_map<std::string, int> kwId;
		std::unordered_map<int, std::string> kwName;
		std::unordered_map<std::string, int> stId;
		std::unordered_map<int, std::string> stName;

		void addKeywords(std::initializer_list<std::pair<const char*, int>> group)
		{
			// newer names win for the id -> name direction, the first name of a key stays
			for (const auto& [n, id] : group)
				kwName[id] = n;
			for (const auto& [n, id] : group)
				kwId.insert({n, id});
		}
		void addStyles(std::initializer_list<std::pair<const char*, int>> group)
		{
			for (const auto& [n, id] : group)
				stName[id] = n;
			for (const auto& [n, id] : group)
				stId.insert({n, id});
		}

		Mappers()
		{
			// pre 2.0
			addKeywords({{"Operators", SCE_USER_KWLIST_OPERATORS1}, {"Folder+", SCE_USER_KWLIST_FOLDERS_IN_CODE1_OPEN},
			             {"Folder-", SCE_USER_KWLIST_FOLDERS_IN_CODE1_CLOSE}, {"Words1", SCE_USER_KWLIST_KEYWORDS1},
			             {"Words2", SCE_USER_KWLIST_KEYWORDS2}, {"Words3", SCE_USER_KWLIST_KEYWORDS3}, {"Words4", SCE_USER_KWLIST_KEYWORDS4}});
			// 2.0
			addKeywords({{"Comments", SCE_USER_KWLIST_COMMENTS}, {"Numbers, additional", SCE_USER_KWLIST_NUMBER_RANGE},
			             {"Numbers, prefixes", SCE_USER_KWLIST_NUMBER_PREFIX2}, {"Numbers, extras with prefixes", SCE_USER_KWLIST_NUMBER_EXTRAS2},
			             {"Numbers, suffixes", SCE_USER_KWLIST_NUMBER_SUFFIX2}, {"Operators1", SCE_USER_KWLIST_OPERATORS1},
			             {"Operators2", SCE_USER_KWLIST_OPERATORS2}, {"Folders in code1, open", SCE_USER_KWLIST_FOLDERS_IN_CODE1_OPEN},
			             {"Folders in code1, middle", SCE_USER_KWLIST_FOLDERS_IN_CODE1_MIDDLE}, {"Folders in code1, close", SCE_USER_KWLIST_FOLDERS_IN_CODE1_CLOSE},
			             {"Folders in code2, open", SCE_USER_KWLIST_FOLDERS_IN_CODE2_OPEN}, {"Folders in code2, middle", SCE_USER_KWLIST_FOLDERS_IN_CODE2_MIDDLE},
			             {"Folders in code2, close", SCE_USER_KWLIST_FOLDERS_IN_CODE2_CLOSE}, {"Folders in comment, open", SCE_USER_KWLIST_FOLDERS_IN_COMMENT_OPEN},
			             {"Folders in comment, middle", SCE_USER_KWLIST_FOLDERS_IN_COMMENT_MIDDLE},
			             {"Folders in comment, close", SCE_USER_KWLIST_FOLDERS_IN_COMMENT_CLOSE}, {"Keywords1", SCE_USER_KWLIST_KEYWORDS1},
			             {"Keywords2", SCE_USER_KWLIST_KEYWORDS2}, {"Keywords3", SCE_USER_KWLIST_KEYWORDS3}, {"Keywords4", SCE_USER_KWLIST_KEYWORDS4},
			             {"Keywords5", SCE_USER_KWLIST_KEYWORDS5}, {"Keywords6", SCE_USER_KWLIST_KEYWORDS6}, {"Keywords7", SCE_USER_KWLIST_KEYWORDS7},
			             {"Keywords8", SCE_USER_KWLIST_KEYWORDS8}, {"Delimiters", SCE_USER_KWLIST_DELIMITERS}});
			// 2.1
			addKeywords({{"Numbers, prefix1", SCE_USER_KWLIST_NUMBER_PREFIX1}, {"Numbers, prefix2", SCE_USER_KWLIST_NUMBER_PREFIX2},
			             {"Numbers, extras1", SCE_USER_KWLIST_NUMBER_EXTRAS1}, {"Numbers, extras2", SCE_USER_KWLIST_NUMBER_EXTRAS2},
			             {"Numbers, suffix1", SCE_USER_KWLIST_NUMBER_SUFFIX1}, {"Numbers, suffix2", SCE_USER_KWLIST_NUMBER_SUFFIX2},
			             {"Numbers, range", SCE_USER_KWLIST_NUMBER_RANGE}});
			// pre 2.0
			addStyles({{"FOLDEROPEN", SCE_USER_STYLE_FOLDER_IN_CODE1}, {"FOLDERCLOSE", SCE_USER_STYLE_FOLDER_IN_CODE1},
			           {"KEYWORD1", SCE_USER_STYLE_KEYWORD1}, {"KEYWORD2", SCE_USER_STYLE_KEYWORD2}, {"KEYWORD3", SCE_USER_STYLE_KEYWORD3},
			           {"KEYWORD4", SCE_USER_STYLE_KEYWORD4}, {"COMMENT", SCE_USER_STYLE_COMMENT}, {"COMMENT LINE", SCE_USER_STYLE_COMMENTLINE},
			           {"NUMBER", SCE_USER_STYLE_NUMBER}, {"OPERATOR", SCE_USER_STYLE_OPERATOR}, {"DELIMINER1", SCE_USER_STYLE_DELIMITER1},
			           {"DELIMINER2", SCE_USER_STYLE_DELIMITER2}, {"DELIMINER3", SCE_USER_STYLE_DELIMITER3}});
			// post 2.0
			addStyles({{"DEFAULT", SCE_USER_STYLE_DEFAULT}, {"COMMENTS", SCE_USER_STYLE_COMMENT}, {"LINE COMMENTS", SCE_USER_STYLE_COMMENTLINE},
			           {"NUMBERS", SCE_USER_STYLE_NUMBER}, {"KEYWORDS1", SCE_USER_STYLE_KEYWORD1}, {"KEYWORDS2", SCE_USER_STYLE_KEYWORD2},
			           {"KEYWORDS3", SCE_USER_STYLE_KEYWORD3}, {"KEYWORDS4", SCE_USER_STYLE_KEYWORD4}, {"KEYWORDS5", SCE_USER_STYLE_KEYWORD5},
			           {"KEYWORDS6", SCE_USER_STYLE_KEYWORD6}, {"KEYWORDS7", SCE_USER_STYLE_KEYWORD7}, {"KEYWORDS8", SCE_USER_STYLE_KEYWORD8},
			           {"OPERATORS", SCE_USER_STYLE_OPERATOR}, {"FOLDER IN CODE1", SCE_USER_STYLE_FOLDER_IN_CODE1},
			           {"FOLDER IN CODE2", SCE_USER_STYLE_FOLDER_IN_CODE2}, {"FOLDER IN COMMENT", SCE_USER_STYLE_FOLDER_IN_COMMENT},
			           {"DELIMITERS1", SCE_USER_STYLE_DELIMITER1}, {"DELIMITERS2", SCE_USER_STYLE_DELIMITER2}, {"DELIMITERS3", SCE_USER_STYLE_DELIMITER3},
			           {"DELIMITERS4", SCE_USER_STYLE_DELIMITER4}, {"DELIMITERS5", SCE_USER_STYLE_DELIMITER5}, {"DELIMITERS6", SCE_USER_STYLE_DELIMITER6},
			           {"DELIMITERS7", SCE_USER_STYLE_DELIMITER7}, {"DELIMITERS8", SCE_USER_STYLE_DELIMITER8}});
		}
	};

	const Mappers& mappers()
	{
		static const Mappers m;
		return m;
	}

	// keyword lists given to the lexer as properties, the others go with SCI_SETKEYWORDS
	const char* lexerProperty(int kwList)
	{
		switch (kwList)
		{
			case SCE_USER_KWLIST_COMMENTS: return "userDefine.comments";
			case SCE_USER_KWLIST_DELIMITERS: return "userDefine.delimiters";
			case SCE_USER_KWLIST_OPERATORS1: return "userDefine.operators1";
			case SCE_USER_KWLIST_NUMBER_PREFIX1: return "userDefine.numberPrefix1";
			case SCE_USER_KWLIST_NUMBER_PREFIX2: return "userDefine.numberPrefix2";
			case SCE_USER_KWLIST_NUMBER_EXTRAS1: return "userDefine.numberExtras1";
			case SCE_USER_KWLIST_NUMBER_EXTRAS2: return "userDefine.numberExtras2";
			case SCE_USER_KWLIST_NUMBER_SUFFIX1: return "userDefine.numberSuffix1";
			case SCE_USER_KWLIST_NUMBER_SUFFIX2: return "userDefine.numberSuffix2";
			case SCE_USER_KWLIST_NUMBER_RANGE: return "userDefine.numberRange";
			case SCE_USER_KWLIST_FOLDERS_IN_CODE1_OPEN: return "userDefine.foldersInCode1Open";
			case SCE_USER_KWLIST_FOLDERS_IN_CODE1_MIDDLE: return "userDefine.foldersInCode1Middle";
			case SCE_USER_KWLIST_FOLDERS_IN_CODE1_CLOSE: return "userDefine.foldersInCode1Close";
			default: return nullptr;
		}
	}

	COLORREF hexColor(const char* str)
	{
		bool ok = false;
		const unsigned long v = QString::fromLatin1(str).toULong(&ok, 16);
		if (!ok)
			return static_cast<COLORREF>(STYLE_NOT_USED);
		return rgb((v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF);
	}

	QString colorHex(COLORREF c)
	{
		return QStringLiteral("%1%2%3").arg(c & 0xFF, 2, 16, QLatin1Char('0')).arg((c >> 8) & 0xFF, 2, 16, QLatin1Char('0'))
			.arg((c >> 16) & 0xFF, 2, 16, QLatin1Char('0')).toUpper();
	}

	bool boolAttr(const pugi::xml_node& n, const char* name)
	{
		return QString::fromLatin1(n.attribute(name).as_string()).compare(QStringLiteral("yes"), Qt::CaseInsensitive) == 0;
	}
}

UserLang::UserLang()
{
	for (int i = 0; i < SCE_USER_STYLE_TOTAL_STYLES; ++i)
	{
		styles[i].styleID = i;
		styles[i].name = QString::fromUtf8(UdlManager::styleName(i));
	}
}

const char* UdlManager::keywordName(int id)
{
	auto it = mappers().kwName.find(id);
	return it == mappers().kwName.end() ? "" : it->second.c_str();
}

int UdlManager::keywordId(const std::string& name)
{
	auto it = mappers().kwId.find(name);
	return it == mappers().kwId.end() ? -1 : it->second;
}

const char* UdlManager::styleName(int id)
{
	auto it = mappers().stName.find(id);
	return it == mappers().stName.end() ? "" : it->second.c_str();
}

int UdlManager::styleId(const std::string& name)
{
	auto it = mappers().stId.find(name);
	return it == mappers().stId.end() ? -1 : it->second;
}

UdlManager& UdlManager::instance()
{
	static UdlManager m;
	return m;
}

QString UdlManager::folder()
{
	return Npp::configDir() + QStringLiteral("/userDefineLangs");
}

void UdlManager::load()
{
	// NppParameters::load(): userDefineLang.xml, then the files of userDefineLangs
	_langs.clear();
	loadFile(Npp::configDir() + QStringLiteral("/userDefineLang.xml"), true);
	for (const QFileInfo& fi : QDir(folder()).entryInfoList({QStringLiteral("*.xml")}, QDir::Files, QDir::Name))
		loadFile(fi.absoluteFilePath(), true);
	for (const QFileInfo& fi : QDir(Npp::dataDir() + QStringLiteral("/userDefineLangs")).entryInfoList({QStringLiteral("*.xml")}, QDir::Files, QDir::Name))
		loadFile(fi.absoluteFilePath(), false);
}

void UdlManager::loadFile(const QString& path, bool userFolder)
{
	// NppParameters::feedUserLang()
	pugi::xml_document doc;
	if (!QFileInfo::exists(path) || !doc.load_file(QFile::encodeName(path).constData()))
		return;
	for (pugi::xml_node n = doc.child("NotepadPlus").child("UserLang"); n; n = n.next_sibling("UserLang"))
	{
		auto ul = std::make_unique<UserLang>();
		ul->name = QString::fromUtf8(n.attribute("name").value());
		if (ul->name.isEmpty() || find(ul->name))
			continue;
		ul->ext = QString::fromUtf8(n.attribute("ext").value());
		ul->udlVersion = QString::fromUtf8(n.attribute("udlVersion").value());
		ul->darkModeTheme = boolAttr(n, "darkModeTheme");
		ul->file = path;
		Q_UNUSED(userFolder);
		pugi::xml_node settings = n.child("Settings");
		pugi::xml_node keywordLists = n.child("KeywordLists");
		pugi::xml_node stylesNode = n.child("Styles");
		if (!settings || !keywordLists || !stylesNode)
			continue;

		// feedUserSettings()
		if (pugi::xml_node g = settings.child("Global"))
		{
			ul->caseIgnored = boolAttr(g, "caseIgnored");
			ul->allowFoldOfComments = boolAttr(g, "allowFoldOfComments");
			ul->forcePureLC = g.attribute("forcePureLC").as_int(0);
			ul->decimalSeparator = g.attribute("decimalSeparator").as_int(0);
			ul->foldCompact = boolAttr(g, "foldCompact");
		}
		if (pugi::xml_node prefix = settings.child("Prefix"))
		{
			if (ul->udlVersion == QStringLiteral("2.1") || ul->udlVersion == QStringLiteral("2.0"))
			{
				for (int i = 0; i < SCE_USER_TOTAL_KEYWORD_GROUPS; ++i)
					ul->isPrefix[i] = boolAttr(prefix, keywordName(i + SCE_USER_KWLIST_KEYWORDS1));
			}
			else
			{
				static const char* const names[] = {"words1", "words2", "words3", "words4"};
				for (int i = 0; i < 4; ++i)
					ul->isPrefix[i] = boolAttr(prefix, names[i]);
			}
		}

		// feedUserKeywordList()
		for (pugi::xml_node k = keywordLists.child("Keywords"); k; k = k.next_sibling("Keywords"))
		{
			const std::string kname = k.attribute("name").as_string();
			const std::string kwl = k.child_value();
			if (kwl.empty())
				continue;
			if (ul->udlVersion.isEmpty() && kname == "Delimiters" && kwl.size() >= 6)
			{
				// pre 2.0
				std::string temp;
				temp += "00"; if (kwl[0] != '0') temp += kwl[0]; temp += " 01";
				temp += " 02"; if (kwl[3] != '0') temp += kwl[3];
				temp += " 03"; if (kwl[1] != '0') temp += kwl[1]; temp += " 04";
				temp += " 05"; if (kwl[4] != '0') temp += kwl[4];
				temp += " 06"; if (kwl[2] != '0') temp += kwl[2]; temp += " 07";
				temp += " 08"; if (kwl[5] != '0') temp += kwl[5];
				temp += " 09 10 11 12 13 14 15 16 17 18 19 20 21 22 23";
				ul->keywordLists[SCE_USER_KWLIST_DELIMITERS] = temp;
			}
			else if (kname == "Comment")
			{
				// pre 2.0
				std::string temp = " " + kwl;
				for (size_t pos = temp.find(" 0"); pos != std::string::npos; pos = temp.find(" 0", pos + 1))
					temp.replace(pos, 2, " 00");
				for (size_t pos = temp.find(" 1"); pos != std::string::npos; pos = temp.find(" 1"))
					temp.replace(pos, 2, " 03");
				for (size_t pos = temp.find(" 2"); pos != std::string::npos; pos = temp.find(" 2"))
					temp.replace(pos, 2, " 04");
				temp += " 01 02";
				if (temp[0] == ' ')
					temp.erase(0, 1);
				ul->keywordLists[SCE_USER_KWLIST_COMMENTS] = temp;
			}
			else
			{
				const int id = keywordId(kname);
				if (id >= 0)
					ul->keywordLists[id] = kwl;
			}
		}

		// feedUserStyles()
		bool seen[SCE_USER_STYLE_TOTAL_STYLES] = {};
		for (pugi::xml_node ws = stylesNode.child("WordsStyle"); ws; ws = ws.next_sibling("WordsStyle"))
		{
			const int id = styleId(ws.attribute("name").as_string());
			if (id < 0 || id >= SCE_USER_STYLE_TOTAL_STYLES || seen[id])
				continue;
			seen[id] = true;
			Style& s = ul->styles[id];
			if (ws.attribute("fgColor"))
				s.fgColor = hexColor(ws.attribute("fgColor").as_string());
			if (ws.attribute("bgColor"))
				s.bgColor = hexColor(ws.attribute("bgColor").as_string());
			s.colorStyle = ws.attribute("colorStyle").as_int(COLORSTYLE_ALL);
			s.fontName = QString::fromUtf8(ws.attribute("fontName").as_string());
			s.fontStyle = ws.attribute("fontStyle").as_int(STYLE_NOT_USED);
			s.fontSize = ws.attribute("fontSize").as_int(STYLE_NOT_USED);
			ul->nesting[id] = ws.attribute("nesting").as_int(0);
		}
		_langs.push_back(std::move(ul));
	}
}

const UserLang* UdlManager::find(const QString& name) const
{
	for (const auto& l : _langs)
	{
		if (l->name == name)
			return l.get();
	}
	return nullptr;
}

UserLang* UdlManager::find(const QString& name)
{
	for (auto& l : _langs)
	{
		if (l->name == name)
			return l.get();
	}
	return nullptr;
}

QString UdlManager::langNameFromExt(const QString& ext, const QString& fileName) const
{
	// NppParameters::getUserDefinedLangNameFromExt(): a UDL of the current light / dark mode first
	if (ext.isEmpty())
		return QString();
	const bool dark = Npp::prefs().isDark();
	const UserLang* matched = nullptr;
	for (const auto& l : _langs)
	{
		for (const QString& e : l->ext.split(QLatin1Char(' '), Qt::SkipEmptyParts))
		{
			if (e.compare(ext, Qt::CaseInsensitive) == 0 || (fileName.contains(QLatin1Char('.')) && e.compare(fileName, Qt::CaseInsensitive) == 0))
			{
				if (!matched)
					matched = l.get();
				if (l->darkModeTheme == dark)
					return l->name;
			}
		}
	}
	return matched ? matched->name : QString();
}

void UdlManager::setUserLexer(ScintillaEditBase* sci, const UserLang& ul, sptr_t bufferId)
{
	// ScintillaEditView::setUserLexer()
	auto prop = [sci](const char* key, const QByteArray& value)
	{
		sci->send(SCI_SETPROPERTY, reinterpret_cast<uptr_t>(key), reinterpret_cast<sptr_t>(value.constData()));
	};
	sci->send(SCI_SETILEXER, 0, reinterpret_cast<sptr_t>(CreateLexer("user")));
	prop("fold", "1");
	prop("userDefine.isCaseIgnored", ul.caseIgnored ? "1" : "0");
	prop("userDefine.allowFoldOfComments", ul.allowFoldOfComments ? "1" : "0");
	prop("userDefine.foldCompact", ul.foldCompact ? "1" : "0");
	for (int i = 0; i < SCE_USER_TOTAL_KEYWORD_GROUPS; ++i)
		prop(QStringLiteral("userDefine.prefixKeywords%1").arg(i + 1).toLatin1().constData(), ul.isPrefix[i] ? "1" : "0");

	int setKeywordsCounter = 0;
	for (int i = 0; i < SCE_USER_KWLIST_TOTAL; ++i)
	{
		const std::string& kw = ul.keywordLists[i];
		if (const char* p = lexerProperty(i))
		{
			prop(p, QByteArray::fromStdString(kw));
			continue;
		}
		// OPERATORS2, FOLDERS_IN_CODE2, FOLDERS_IN_COMMENT, KEYWORDS1-8: quoted words keep their spaces
		std::string temp;
		bool inDoubleQuote = false;
		bool inSingleQuote = false;
		bool nonWSFound = false;
		const size_t len = kw.size();
		for (size_t j = 0; j < len; ++j)
		{
			const char c = kw[j];
			const char next = (j + 1 < len) ? kw[j + 1] : '\0';
			if (!inSingleQuote && c == '"')
			{
				inDoubleQuote = !inDoubleQuote;
				continue;
			}
			if (!inDoubleQuote && c == '\'')
			{
				inSingleQuote = !inSingleQuote;
				continue;
			}
			if (c == '\\' && (next == '"' || next == '\'' || next == '\\'))
			{
				++j;
				temp += kw[j];
				continue;
			}
			if (inDoubleQuote || inSingleQuote)
			{
				if (static_cast<unsigned char>(c) > ' ')
				{
					temp += c;
					nonWSFound = true;
				}
				else if (nonWSFound && j > 0 && kw[j - 1] != '"' && next != '"' && static_cast<unsigned char>(next) > ' ')
				{
					temp += inDoubleQuote ? '\v' : '\b';
				}
			}
			else
			{
				temp += c;
			}
		}
		sci->send(SCI_SETKEYWORDS, static_cast<uptr_t>(setKeywordsCounter++), reinterpret_cast<sptr_t>(temp.c_str()));
	}

	prop("userDefine.forcePureLC", QByteArray::number(ul.forcePureLC));
	prop("userDefine.decimalSeparator", QByteArray::number(ul.decimalSeparator));
	// ids of the language and of the document for the caches of the lexer
	prop("userDefine.udlName", QByteArray::number(static_cast<qulonglong>(reinterpret_cast<uintptr_t>(&ul))));
	prop("userDefine.currentBufferID", QByteArray::number(static_cast<qulonglong>(bufferId)));

	StyleManager& sm = StyleManager::instance();
	for (int i = 0; i < SCE_USER_STYLE_TOTAL_STYLES; ++i)
	{
		prop(QStringLiteral("userDefine.nesting.%1").arg(i, 2, 10, QLatin1Char('0')).toLatin1().constData(), QByteArray::number(ul.nesting[i]));
		Style s = ul.styles[i];
		s.styleID = i;
		sm.setSpecialStyle(sci, s);
	}
}

bool UdlManager::addLang(std::unique_ptr<UserLang> lang)
{
	if (!lang || lang->name.isEmpty() || find(lang->name))
		return false;
	_langs.push_back(std::move(lang));
	return true;
}

namespace
{
	bool inSettings(const QString& path)
	{
		return QFileInfo(path).absoluteFilePath().startsWith(QFileInfo(Npp::configDir()).absoluteFilePath() + QLatin1Char('/'));
	}

	// the file of the settings folder a language is written to
	QString targetFile(const UserLang& ul)
	{
		if (!ul.file.isEmpty() && inSettings(ul.file))
			return ul.file;
		QString base = ul.file.isEmpty() ? QString() : QFileInfo(ul.file).fileName();
		if (base.isEmpty())
		{
			base = ul.name;
			static const QRegularExpression bad(QStringLiteral("[\\\\/:*?\"<>|]"));
			base.replace(bad, QStringLiteral("_"));
			base += QStringLiteral(".xml");
		}
		return UdlManager::folder() + QLatin1Char('/') + base;
	}
}

bool UdlManager::removeLang(const QString& name)
{
	auto it = std::find_if(_langs.begin(), _langs.end(), [&](const auto& l) { return l->name == name; });
	if (it == _langs.end())
		return false;
	const QString file = (*it)->file;
	_langs.erase(it);
	if (file.isEmpty() || !inSettings(file))
		return true;
	// the other languages of the same file are written again, an empty file is removed
	bool others = false;
	for (auto& l : _langs)
	{
		if (l->file == file)
		{
			l->dirty = true;
			others = true;
		}
	}
	if (!others)
		QFile::remove(file);
	return true;
}

int UdlManager::importFile(const QString& path)
{
	// UserDefineDialog IDC_IMPORT_BUTTON: the languages of the file, the names that exist are skipped
	const size_t before = _langs.size();
	loadFile(path, false);
	int added = 0;
	for (size_t i = before; i < _langs.size(); ++i)
	{
		_langs[i]->file.clear();
		_langs[i]->dirty = true;
		++added;
	}
	return added;
}

bool UdlManager::saveAll(QString* error)
{
	std::vector<QString> files;
	for (auto& l : _langs)
	{
		if (!l->dirty)
			continue;
		l->file = targetFile(*l);
		if (std::find(files.begin(), files.end(), l->file) == files.end())
			files.push_back(l->file);
	}
	bool ok = true;
	for (const QString& f : files)
	{
		std::vector<const UserLang*> group;
		for (auto& l : _langs)
		{
			if (l->file == f)
				group.push_back(l.get());
		}
		ok = saveToFile(group, f, error) && ok;
	}
	if (ok)
	{
		for (auto& l : _langs)
			l->dirty = false;
	}
	return ok;
}

bool UdlManager::saveToFile(const UserLang& ul, const QString& path, QString* error)
{
	return saveToFile(std::vector<const UserLang*>{&ul}, path, error);
}

bool UdlManager::saveToFile(const std::vector<const UserLang*>& langs, const QString& path, QString* error)
{
	// NppParameters::insertUserLang2Tree(): udlVersion 2.1
	pugi::xml_document doc;
	pugi::xml_node decl = doc.append_child(pugi::node_declaration);
	decl.append_attribute("version") = "1.0";
	decl.append_attribute("encoding") = "UTF-8";
	pugi::xml_node root = doc.append_child("NotepadPlus");
	for (const UserLang* pul : langs)
	{
	const UserLang& ul = *pul;
	pugi::xml_node n = root.append_child("UserLang");
	n.append_attribute("name") = ul.name.toUtf8().constData();
	n.append_attribute("ext") = ul.ext.toUtf8().constData();
	n.append_attribute("udlVersion") = "2.1";
	if (ul.darkModeTheme)
		n.append_attribute("darkModeTheme") = "yes";
	pugi::xml_node settings = n.append_child("Settings");
	pugi::xml_node g = settings.append_child("Global");
	g.append_attribute("caseIgnored") = ul.caseIgnored ? "yes" : "no";
	g.append_attribute("allowFoldOfComments") = ul.allowFoldOfComments ? "yes" : "no";
	g.append_attribute("foldCompact") = ul.foldCompact ? "yes" : "no";
	g.append_attribute("forcePureLC") = ul.forcePureLC;
	g.append_attribute("decimalSeparator") = ul.decimalSeparator;
	pugi::xml_node prefix = settings.append_child("Prefix");
	for (int i = 0; i < SCE_USER_TOTAL_KEYWORD_GROUPS; ++i)
		prefix.append_attribute(keywordName(i + SCE_USER_KWLIST_KEYWORDS1)) = ul.isPrefix[i] ? "yes" : "no";
	pugi::xml_node kwls = n.append_child("KeywordLists");
	for (int i = 0; i < SCE_USER_KWLIST_TOTAL; ++i)
	{
		pugi::xml_node k = kwls.append_child("Keywords");
		k.append_attribute("name") = keywordName(i);
		if (!ul.keywordLists[i].empty())
			k.append_child(pugi::node_pcdata).set_value(ul.keywordLists[i].c_str());
	}
	pugi::xml_node styles = n.append_child("Styles");
	for (int i = 0; i < SCE_USER_STYLE_TOTAL_STYLES; ++i)
	{
		const Style& s = ul.styles[i];
		pugi::xml_node ws = styles.append_child("WordsStyle");
		ws.append_attribute("name") = styleName(i);
		ws.append_attribute("fgColor") = colorHex(s.fgColor == static_cast<COLORREF>(STYLE_NOT_USED) ? black : s.fgColor).toLatin1().constData();
		ws.append_attribute("bgColor") = colorHex(s.bgColor == static_cast<COLORREF>(STYLE_NOT_USED) ? white : s.bgColor).toLatin1().constData();
		ws.append_attribute("colorStyle") = s.colorStyle;
		ws.append_attribute("fontName") = s.fontName.toUtf8().constData();
		ws.append_attribute("fontStyle") = s.fontStyle == STYLE_NOT_USED ? 0 : s.fontStyle;
		if (s.fontSize != STYLE_NOT_USED)
			ws.append_attribute("fontSize") = s.fontSize;
		ws.append_attribute("nesting") = ul.nesting[i];
	}
	}
	QDir().mkpath(QFileInfo(path).absolutePath());
	if (!doc.save_file(QFile::encodeName(path).constData(), "    ", pugi::format_default, pugi::encoding_utf8))
	{
		if (error)
			*error = QStringLiteral("Can not write %1").arg(path);
		return false;
	}
	return true;
}
