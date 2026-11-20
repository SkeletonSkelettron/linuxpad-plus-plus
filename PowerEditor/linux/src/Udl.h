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

// User Defined Languages: userDefineLang.xml and userDefineLangs/*.xml of the settings
// folder, applied with the Notepad++ UDL lexer (lexilla/lexers/LexUser.cxx).
// Port of UserLangContainer / NppParameters::feedUserLang() and ScintillaEditView::setUserLexer().

#pragma once

#include <memory>
#include <string>
#include <vector>

#include <QString>

#include "LangStyles.h"
#include "NppCommon.h"

class ScintillaEditBase;

struct UserLang
{
	QString name = QStringLiteral("User Defined Language");
	QString ext;
	QString udlVersion;
	QString file; // the XML file it was read from (empty: not saved yet)
	bool dirty = false; // changed in the dialog, written by UdlManager::saveAll()
	bool caseIgnored = false;
	bool allowFoldOfComments = false;
	bool foldCompact = false;
	bool darkModeTheme = false;
	int forcePureLC = 0;
	int decimalSeparator = 0;
	bool isPrefix[SCE_USER_TOTAL_KEYWORD_GROUPS] = {};
	std::string keywordLists[SCE_USER_KWLIST_TOTAL];
	Style styles[SCE_USER_STYLE_TOTAL_STYLES];
	int nesting[SCE_USER_STYLE_TOTAL_STYLES] = {};

	UserLang();
};

class UdlManager
{
public:
	static UdlManager& instance();

	void load(); // the settings folder, then the installed userDefineLangs folder
	const std::vector<std::unique_ptr<UserLang>>& langs() const { return _langs; }
	const UserLang* find(const QString& name) const;
	UserLang* find(const QString& name);
	UserLang& scratch() { return _scratch; } // IDM_LANG_USER: the language of the dialog not saved yet
	QString langNameFromExt(const QString& ext, const QString& fileName) const; // getUserDefinedLangNameFromExt()
	static QString folder(); // ~/.config/notepad++/userDefineLangs

	// ScintillaEditView::setUserLexer(): bufferId identifies the document for the lexer
	static void setUserLexer(ScintillaEditBase* sci, const UserLang& lang, sptr_t bufferId);

	// GlobalMappers
	static const char* keywordName(int id);
	static int keywordId(const std::string& name);
	static const char* styleName(int id);
	static int styleId(const std::string& name);

	// write languages to a file (Export, and the files of the settings folder)
	static bool saveToFile(const UserLang& lang, const QString& path, QString* error);
	static bool saveToFile(const std::vector<const UserLang*>& langs, const QString& path, QString* error);
	bool saveAll(QString* error); // NppParameters::writeNeed2SaveUDL(): the changed languages, each in its file
	bool addLang(std::unique_ptr<UserLang> lang); // false: the name exists
	bool removeLang(const QString& name);
	int importFile(const QString& path); // the languages of a file, copied into userDefineLangs: number added

private:
	UdlManager() = default;
	void loadFile(const QString& path, bool userFolder);

	std::vector<std::unique_ptr<UserLang>> _langs;
	UserLang _scratch;
};
