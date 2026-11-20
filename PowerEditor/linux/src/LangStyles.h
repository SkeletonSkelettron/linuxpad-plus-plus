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

// Languages (langs.xml), styles (stylers.xml / themes) and the code that
// applies them to a Scintilla view. Port of the lexer related parts of
// Parameters.cpp and ScintillaComponent/ScintillaEditView.cpp.

#pragma once

#include <map>
#include <string>
#include <vector>

#include <QMap>
#include <QString>

#include "NppCommon.h"

class ScintillaEditBase;
struct UserLang;

struct Style
{
	int styleID = STYLE_NOT_USED;
	QString name;
	COLORREF fgColor = static_cast<COLORREF>(STYLE_NOT_USED);
	COLORREF bgColor = static_cast<COLORREF>(STYLE_NOT_USED);
	int colorStyle = COLORSTYLE_ALL;
	QString fontName;
	int fontStyle = STYLE_NOT_USED;
	int fontSize = STYLE_NOT_USED;
	int keywordClass = STYLE_NOT_USED;
	std::string keywords;
	bool hasKeywordsNode = false;
};

struct StyleArray
{
	std::vector<Style> styles;

	const Style* findByID(int id) const;
	Style* findByID(int id);
	const Style* findByName(const QString& name) const;
	Style* findByName(const QString& name);
};

struct LexerStyler : StyleArray
{
	QString lexerName;
	QString lexerDesc;
	QString userExt;
};

struct LangDef
{
	QString name;
	QString ext;
	QString commentLine;
	QString commentStart;
	QString commentEnd;
	std::string keywords[NB_LIST];
	bool hasKeywords[NB_LIST] = {};
	int tabSize = -1;
	bool tabReplacedBySpace = false;
	bool backspaceUnindent = false;
};

class StyleManager
{
public:
	static StyleManager& instance();

	void loadLangs();
	bool loadTheme(const QString& path); // empty path: default stylers
	QString currentThemePath() const { return _themePath; }
	QString defaultStylersPath() const;
	QMap<QString, QString> themes() const; // display name -> file path
	bool saveCurrentTheme(QString* error);

	static const LanguageNameInfo& langInfo(LangType lang);
	static int langCount();
	LangType langFromName(const QString& langName) const;
	LangType langFromMenuId(int menuId) const;
	int menuIdFromLang(LangType lang) const;
	LangType langFromFileName(const QString& path) const;
	static LangType langFromContent(const QByteArray& head);

	const LangDef* langDef(LangType lang) const;
	const char* wordList(LangType lang, int index) const;

	StyleArray& globalStyles() { return _globalStyles; }
	std::vector<LexerStyler>& lexerStylers() { return _lexerStylers; }
	LexerStyler* lexerStylerByName(const QString& name);
	const LexerStyler* lexerStylerByName(const QString& name) const;

	// Port of ScintillaEditView::defineDocType() and performGlobalStyles()
	void defineDocType(ScintillaEditBase* sci, LangType typeDoc, const UserLang* udl = nullptr, sptr_t bufferId = 0) const;
	void performGlobalStyles(ScintillaEditBase* sci) const;
	void applyEolAppearance(ScintillaEditBase* sci) const; // ScintillaEditView::setCRLF()
	void setSpecialStyle(ScintillaEditBase* sci, const Style& style) const;

	QString defaultFontFamily() const;

private:
	StyleManager() = default;

	void setStyle(ScintillaEditBase* sci, const Style& style) const { setSpecialStyle(sci, style); }
	void makeStyle(ScintillaEditBase* sci, LangType langType, const char** keywordArray) const;
	void setKeywords(ScintillaEditBase* sci, LangType langType, const char* keywords, int index) const;
	void populateSubStyleKeywords(ScintillaEditBase* sci, LangType langType, int baseStyleID, int numSubStyles, int firstLangIndex, const char** pKwArray) const;
	void setLexer(ScintillaEditBase* sci, LangType langType, int whichList, int baseStyleID = STYLE_NOT_USED, int numSubStyles = 8) const;
	void setLexerFromLangID(ScintillaEditBase* sci, LangType langID) const;
	const char* concatToBuildKeywordList(std::string& kwl, LangType langType, int keywordIndex) const;

	void setCppLexer(ScintillaEditBase* sci, LangType langType) const;
	void setJsLexer(ScintillaEditBase* sci) const;
	void setTclLexer(ScintillaEditBase* sci) const;
	void setObjCLexer(ScintillaEditBase* sci, LangType langType) const;
	void setXmlLexer(ScintillaEditBase* sci, LangType type) const;
	void setHTMLLexer(ScintillaEditBase* sci) const;
	void setEmbeddedJSLexer(ScintillaEditBase* sci) const;
	void setEmbeddedPhpLexer(ScintillaEditBase* sci) const;
	void setEmbeddedAspLexer(ScintillaEditBase* sci) const;
	void setJsonLexer(ScintillaEditBase* sci, bool isJson5) const;
	void setTypeScriptLexer(ScintillaEditBase* sci) const;
	void setBashLexer(ScintillaEditBase* sci) const;
	void setSpecialIndicator(ScintillaEditBase* sci, int indic, COLORREF color) const;
	void setTabSettings(ScintillaEditBase* sci, LangType lang) const;

	std::vector<LangDef> _langs; // indexed by LangType
	StyleArray _globalStyles;
	std::vector<LexerStyler> _lexerStylers;
	QString _themePath;
	QString _loadedStylersPath;
	mutable std::string _kwBuffer[NB_LIST * 2];
};
