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

#include "LangStyles.h"

#include <cstring>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFontDatabase>

#include "ScintillaEditBase.h"
#include "ILexer.h"
#include "Lexilla.h"
#include "Udl.h"
#include "pugixml.hpp"

#include "LangTable.inc"

namespace
{
	constexpr int LIST_NONE = 0;
	constexpr int LIST_0 = 1;
	constexpr int LIST_1 = 2;
	constexpr int LIST_2 = 4;
	constexpr int LIST_3 = 8;
	constexpr int LIST_4 = 16;
	constexpr int LIST_5 = 32;
	constexpr int LIST_6 = 64;
	constexpr int LIST_7 = 128;
	constexpr int LIST_8 = 256;

	constexpr COLORREF red = rgb(0xFF, 0, 0);
	constexpr COLORREF liteGreen = rgb(0, 0xFF, 0);
	constexpr COLORREF blue = rgb(0, 0, 0xFF);
	constexpr COLORREF yellow = rgb(0xFF, 0xFF, 0);
	constexpr COLORREF cyan = rgb(0, 0xFF, 0xFF);
	constexpr COLORREF orange = rgb(0xFF, 0x80, 0x00);
	constexpr COLORREF purple = rgb(0x80, 0x00, 0xFF);
	constexpr COLORREF darkGreen = rgb(0x00, 0x80, 0x00);
	constexpr COLORREF grey = rgb(128, 128, 128);
	constexpr COLORREF liteGrey = rgb(192, 192, 192);
	constexpr COLORREF veryLiteGrey = rgb(224, 224, 224);
	constexpr COLORREF darkGrey = rgb(64, 64, 64);
	constexpr COLORREF white = rgb(0xFF, 0xFF, 0xFF);
	constexpr COLORREF black = rgb(0, 0, 0);
	constexpr COLORREF yellowGreen = rgb(154, 205, 50);
	constexpr COLORREF darkCyan = rgb(0, 139, 139);
	constexpr COLORREF midGreen = rgb(0, 160, 0);

	inline sptr_t ex(ScintillaEditBase* s, unsigned int m, uptr_t w = 0, sptr_t l = 0)
	{
		return s->send(m, w, l);
	}

	inline void prop(ScintillaEditBase* s, const char* key, const char* value)
	{
		s->send(SCI_SETPROPERTY, reinterpret_cast<uptr_t>(key), reinterpret_cast<sptr_t>(value));
	}

	int kwClassFromName(const char* str)
	{
		if (!str)
			return -1;
		static const char* names[] = {"instre1", "instre2", "type1", "type2", "type3", "type4", "type5", "type6", "type7",
		                              "substyle1", "substyle2", "substyle3", "substyle4", "substyle5", "substyle6", "substyle7", "substyle8"};
		for (int i = 0; i < static_cast<int>(std::size(names)); ++i)
		{
			if (std::strcmp(str, names[i]) == 0)
				return i;
		}
		if (str[0] >= '0' && str[0] <= '8' && str[1] == '\0')
			return str[0] - '0';
		return -1;
	}

	COLORREF hexColor(const char* str)
	{
		if (!str || !*str)
			return static_cast<COLORREF>(STYLE_NOT_USED);
		bool ok = false;
		const unsigned long v = QString::fromLatin1(str).toULong(&ok, 16);
		if (!ok)
			return static_cast<COLORREF>(STYLE_NOT_USED);
		return rgb((v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF);
	}

	QString colorHex(COLORREF c)
	{
		return QStringLiteral("%1%2%3")
			.arg(c & 0xFF, 2, 16, QLatin1Char('0'))
			.arg((c >> 8) & 0xFF, 2, 16, QLatin1Char('0'))
			.arg((c >> 16) & 0xFF, 2, 16, QLatin1Char('0'))
			.toUpper();
	}

	void readStyle(Style& s, const pugi::xml_node& node)
	{
		s.styleID = node.attribute("styleID").as_int(STYLE_NOT_USED);
		s.name = QString::fromUtf8(node.attribute("name").as_string());
		if (node.attribute("fgColor"))
			s.fgColor = hexColor(node.attribute("fgColor").as_string());
		if (node.attribute("bgColor"))
			s.bgColor = hexColor(node.attribute("bgColor").as_string());
		s.colorStyle = node.attribute("colorStyle").as_int(COLORSTYLE_ALL);
		s.fontName = QString::fromUtf8(node.attribute("fontName").as_string());
		const char* fs = node.attribute("fontStyle").as_string();
		s.fontStyle = (fs && *fs) ? node.attribute("fontStyle").as_int(STYLE_NOT_USED) : STYLE_NOT_USED;
		const char* fz = node.attribute("fontSize").as_string();
		s.fontSize = (fz && *fz) ? node.attribute("fontSize").as_int(STYLE_NOT_USED) : STYLE_NOT_USED;
		if (node.attribute("keywordClass"))
			s.keywordClass = kwClassFromName(node.attribute("keywordClass").as_string());
		const char* kw = node.child_value();
		s.keywords = kw ? kw : "";
		s.hasKeywordsNode = (s.keywordClass != STYLE_NOT_USED);
	}

	void writeStyle(pugi::xml_node node, const Style& s)
	{
		auto setOrAdd = [&](const char* name, const char* value)
		{
			pugi::xml_attribute a = node.attribute(name);
			if (!a)
				a = node.append_attribute(name);
			a.set_value(value);
		};
		if (s.fgColor != static_cast<COLORREF>(STYLE_NOT_USED))
			setOrAdd("fgColor", colorHex(s.fgColor).toUtf8().constData());
		if (s.bgColor != static_cast<COLORREF>(STYLE_NOT_USED))
			setOrAdd("bgColor", colorHex(s.bgColor).toUtf8().constData());
		if (node.attribute("fontName") || !s.fontName.isEmpty())
			setOrAdd("fontName", s.fontName.toUtf8().constData());
		if (node.attribute("fontStyle") || s.fontStyle != STYLE_NOT_USED)
			setOrAdd("fontStyle", s.fontStyle == STYLE_NOT_USED ? "" : QByteArray::number(s.fontStyle).constData());
		if (node.attribute("fontSize") || s.fontSize != STYLE_NOT_USED)
			setOrAdd("fontSize", s.fontSize == STYLE_NOT_USED ? "" : QByteArray::number(s.fontSize).constData());
		if (s.hasKeywordsNode)
		{
			node.remove_children();
			if (!s.keywords.empty())
				node.append_child(pugi::node_pcdata).set_value(s.keywords.c_str());
		}
	}

	bool loadStylers(const QString& path, StyleArray& global, std::vector<LexerStyler>& lexers)
	{
		pugi::xml_document doc;
		if (!doc.load_file(QFile::encodeName(path).constData()))
			return false;
		pugi::xml_node root = doc.child("NotepadPlus");
		if (!root)
			return false;

		for (pugi::xml_node lex : root.child("LexerStyles").children("LexerType"))
		{
			LexerStyler ls;
			ls.lexerName = QString::fromUtf8(lex.attribute("name").as_string());
			ls.lexerDesc = QString::fromUtf8(lex.attribute("desc").as_string());
			ls.userExt = QString::fromUtf8(lex.attribute("ext").as_string());
			for (pugi::xml_node ws : lex.children("WordsStyle"))
			{
				Style s;
				readStyle(s, ws);
				ls.styles.push_back(std::move(s));
			}
			lexers.push_back(std::move(ls));
		}
		for (pugi::xml_node ws : root.child("GlobalStyles").children("WidgetStyle"))
		{
			Style s;
			readStyle(s, ws);
			global.styles.push_back(std::move(s));
		}
		return true;
	}

	bool isInList(const QString& token, const QString& list)
	{
		if (token.isEmpty())
			return false;
		const QStringList items = list.split(QLatin1Char(' '), Qt::SkipEmptyParts);
		for (const QString& item : items)
		{
			if (item.compare(token, Qt::CaseInsensitive) == 0)
				return true;
		}
		return false;
	}
}

// ---------------------------------------------------------------------------
// StyleArray
// ---------------------------------------------------------------------------
const Style* StyleArray::findByID(int id) const
{
	for (const Style& s : styles)
	{
		if (s.styleID == id)
			return &s;
	}
	return nullptr;
}

Style* StyleArray::findByID(int id)
{
	for (Style& s : styles)
	{
		if (s.styleID == id)
			return &s;
	}
	return nullptr;
}

const Style* StyleArray::findByName(const QString& name) const
{
	for (const Style& s : styles)
	{
		if (s.name == name)
			return &s;
	}
	return nullptr;
}

Style* StyleArray::findByName(const QString& name)
{
	for (Style& s : styles)
	{
		if (s.name == name)
			return &s;
	}
	return nullptr;
}

// ---------------------------------------------------------------------------
// StyleManager: loading
// ---------------------------------------------------------------------------
StyleManager& StyleManager::instance()
{
	static StyleManager sm;
	return sm;
}

const LanguageNameInfo& StyleManager::langInfo(LangType lang)
{
	const int n = langCount();
	int i = static_cast<int>(lang);
	if (i < 0 || i >= n)
		i = 0;
	return kLangNameInfo[i];
}

int StyleManager::langCount()
{
	return static_cast<int>(std::size(kLangNameInfo));
}

void StyleManager::loadLangs()
{
	_langs.clear();
	_langs.resize(langCount());
	for (int i = 0; i < langCount(); ++i)
		_langs[i].name = QString::fromUtf8(kLangNameInfo[i].langName);

	QString path = Npp::configDir() + QStringLiteral("/langs.xml");
	if (!QFileInfo::exists(path))
		path = Npp::dataDir() + QStringLiteral("/langs.model.xml");

	pugi::xml_document doc;
	if (!doc.load_file(QFile::encodeName(path).constData()))
		return;

	for (pugi::xml_node lang : doc.child("NotepadPlus").child("Languages").children("Language"))
	{
		const LangType lt = langFromName(QString::fromUtf8(lang.attribute("name").as_string()));
		if (lt == L_TEXT && std::strcmp(lang.attribute("name").as_string(), "normal") != 0)
			continue;
		LangDef& d = _langs[lt];
		d.ext = QString::fromUtf8(lang.attribute("ext").as_string());
		d.commentLine = QString::fromUtf8(lang.attribute("commentLine").as_string());
		d.commentStart = QString::fromUtf8(lang.attribute("commentStart").as_string());
		d.commentEnd = QString::fromUtf8(lang.attribute("commentEnd").as_string());
		const int tabSettings = lang.attribute("tabSettings").as_int(-1);
		if (tabSettings != -1)
		{
			d.tabSize = tabSettings & 0x7F;
			d.tabReplacedBySpace = (tabSettings & 0x80) != 0;
		}
		d.backspaceUnindent = std::strcmp(lang.attribute("backspaceUnindent").as_string(), "yes") == 0;
		for (pugi::xml_node kw : lang.children("Keywords"))
		{
			const int idx = kwClassFromName(kw.attribute("name").as_string());
			if (idx >= 0 && idx < NB_LIST)
			{
				d.keywords[idx] = kw.child_value();
				d.hasKeywords[idx] = true;
			}
		}
	}
}

QString StyleManager::defaultStylersPath() const
{
	const QString user = Npp::configDir() + QStringLiteral("/stylers.xml");
	if (QFileInfo::exists(user))
		return user;
	return Npp::dataDir() + QStringLiteral("/stylers.model.xml");
}

QMap<QString, QString> StyleManager::themes() const
{
	QMap<QString, QString> result;
	for (const QString& dir : {Npp::dataDir() + QStringLiteral("/themes"), Npp::configDir() + QStringLiteral("/themes")})
	{
		const QFileInfoList files = QDir(dir).entryInfoList({QStringLiteral("*.xml")}, QDir::Files, QDir::Name);
		for (const QFileInfo& fi : files)
			result.insert(fi.completeBaseName(), fi.absoluteFilePath());
	}
	return result;
}

bool StyleManager::loadTheme(const QString& path)
{
	StyleArray modelGlobal;
	std::vector<LexerStyler> modelLexers;
	const QString modelPath = defaultStylersPath();
	loadStylers(modelPath, modelGlobal, modelLexers);

	if (path.isEmpty() || !QFileInfo::exists(path))
	{
		_globalStyles = std::move(modelGlobal);
		_lexerStylers = std::move(modelLexers);
		_themePath.clear();
		_loadedStylersPath = modelPath;
		return true;
	}

	StyleArray themeGlobal;
	std::vector<LexerStyler> themeLexers;
	if (!loadStylers(path, themeGlobal, themeLexers))
	{
		_globalStyles = std::move(modelGlobal);
		_lexerStylers = std::move(modelLexers);
		_themePath.clear();
		_loadedStylersPath = modelPath;
		return false;
	}

	// A theme may miss lexers or global styles added after it was written:
	// complete it from the model, using the theme's default colours.
	const Style* themeDefault = themeGlobal.findByID(STYLE_DEFAULT);
	const Style* modelDefault = modelGlobal.findByID(STYLE_DEFAULT);
	for (const Style& s : modelGlobal.styles)
	{
		const bool exists = (s.styleID != 0) ? (themeGlobal.findByID(s.styleID) && themeGlobal.findByName(s.name)) : (themeGlobal.findByName(s.name) != nullptr);
		if (!exists)
			themeGlobal.styles.push_back(s);
	}
	for (const LexerStyler& ml : modelLexers)
	{
		bool found = false;
		for (const LexerStyler& tl : themeLexers)
		{
			if (tl.lexerName == ml.lexerName)
			{
				found = true;
				break;
			}
		}
		if (found)
			continue;
		LexerStyler copy = ml;
		if (themeDefault && modelDefault)
		{
			for (Style& s : copy.styles)
			{
				if (s.bgColor == modelDefault->bgColor)
					s.bgColor = themeDefault->bgColor;
				if (s.fgColor == modelDefault->fgColor)
					s.fgColor = themeDefault->fgColor;
			}
		}
		themeLexers.push_back(std::move(copy));
	}

	_globalStyles = std::move(themeGlobal);
	_lexerStylers = std::move(themeLexers);
	_themePath = path;
	_loadedStylersPath = path;
	return true;
}

bool StyleManager::saveCurrentTheme(QString* error)
{
	// Themes shipped in the data dir and the model are saved into the user config dir.
	QString target;
	if (_themePath.isEmpty())
		target = Npp::configDir() + QStringLiteral("/stylers.xml");
	else if (_themePath.startsWith(Npp::configDir()))
		target = _themePath;
	else
	{
		QDir().mkpath(Npp::configDir() + QStringLiteral("/themes"));
		target = Npp::configDir() + QStringLiteral("/themes/") + QFileInfo(_themePath).fileName();
	}

	pugi::xml_document doc;
	if (!doc.load_file(QFile::encodeName(_loadedStylersPath).constData()))
	{
		if (error)
			*error = QStringLiteral("Cannot read %1").arg(_loadedStylersPath);
		return false;
	}
	pugi::xml_node root = doc.child("NotepadPlus");
	for (pugi::xml_node lex : root.child("LexerStyles").children("LexerType"))
	{
		const LexerStyler* ls = lexerStylerByName(QString::fromUtf8(lex.attribute("name").as_string()));
		if (!ls)
			continue;
		for (pugi::xml_node ws : lex.children("WordsStyle"))
		{
			const Style* s = ls->findByID(ws.attribute("styleID").as_int(STYLE_NOT_USED));
			if (s)
				writeStyle(ws, *s);
		}
	}
	for (pugi::xml_node ws : root.child("GlobalStyles").children("WidgetStyle"))
	{
		const Style* s = _globalStyles.findByName(QString::fromUtf8(ws.attribute("name").as_string()));
		if (s)
			writeStyle(ws, *s);
	}
	if (!doc.save_file(QFile::encodeName(target).constData(), "    "))
	{
		if (error)
			*error = QStringLiteral("Cannot write %1").arg(target);
		return false;
	}
	if (!_themePath.isEmpty())
		_themePath = target;
	_loadedStylersPath = target;
	return true;
}

LangType StyleManager::langFromName(const QString& langName) const
{
	const QByteArray name = langName.toUtf8();
	for (int i = 0; i < langCount(); ++i)
	{
		if (std::strcmp(kLangNameInfo[i].langName, name.constData()) == 0)
			return static_cast<LangType>(i);
	}
	return L_TEXT;
}

LangType StyleManager::langFromMenuId(int menuId) const
{
	for (const auto& m : kMenuIdToLang)
	{
		if (m.menuId == menuId)
			return m.lang;
	}
	return L_TEXT;
}

int StyleManager::menuIdFromLang(LangType lang) const
{
	for (const auto& m : kMenuIdToLang)
	{
		if (m.lang == lang)
			return m.menuId;
	}
	return 0;
}

LangType StyleManager::langFromFileName(const QString& path) const
{
	const QFileInfo fi(path);
	const QString name = fi.fileName();

	// special file names (Buffer::setFileName)
	if (name.compare(QStringLiteral("makefile"), Qt::CaseInsensitive) == 0 || name.compare(QStringLiteral("GNUmakefile"), Qt::CaseInsensitive) == 0)
		return L_MAKEFILE;
	if (name.compare(QStringLiteral("CmakeLists.txt"), Qt::CaseInsensitive) == 0)
		return L_CMAKE;
	if (name == QStringLiteral("SConstruct") || name == QStringLiteral("SConscript") || name == QStringLiteral("wscript"))
		return L_PYTHON;
	if (name == QStringLiteral("Rakefile") || name == QStringLiteral("Vagrantfile"))
		return L_RUBY;
	if (name == QStringLiteral("PKGBUILD") || name == QStringLiteral(".bashrc") || name == QStringLiteral(".bash_profile") ||
	    name == QStringLiteral(".profile") || name == QStringLiteral(".zshrc"))
		return L_BASH;

	QString ext = fi.suffix();
	if (ext.isEmpty())
		ext = name.startsWith(QLatin1Char('.')) ? name.mid(1) : QString();
	if (ext.isEmpty())
		return L_TEXT;

	// user extensions from the styler, then langs.xml (scanned from the end like Notepad++)
	for (const LexerStyler& ls : _lexerStylers)
	{
		if (isInList(ext, ls.userExt))
			return langFromName(ls.lexerName);
	}
	for (int i = static_cast<int>(_langs.size()) - 1; i >= 0; --i)
	{
		if (!_langs[i].ext.isEmpty() && isInList(ext, _langs[i].ext))
			return static_cast<LangType>(i);
	}
	return L_TEXT;
}

LangType StyleManager::langFromContent(const QByteArray& head)
{
	// Port of FileManager::detectLanguageFromTextBeginning()
	const auto* data = reinterpret_cast<const unsigned char*>(head.constData());
	const size_t dataLen = static_cast<size_t>(head.size());
	if (dataLen <= 3)
		return L_TEXT;

	size_t i = 0;
	if ((data[0] == 0xEF && data[1] == 0xBB && data[2] == 0xBF) || (data[0] == 0xFE && data[1] == 0xFF && data[2] == 0x00) ||
	    (data[0] == 0xFF && data[1] == 0xFE && data[2] == 0x00))
		i += 3;

	for (; i < dataLen; ++i)
	{
		if (data[i] != ' ' && data[i] != '\t' && data[i] != '\n' && data[i] != '\r')
			break;
	}
	if (i == dataLen)
		return L_TEXT;

	const size_t longestLength = std::min<size_t>(40, dataLen - i);
	std::string buf2Test(reinterpret_cast<const char*>(data) + i, longestLength);
	const size_t crnl = std::min(buf2Test.find('\r'), buf2Test.find('\n'));
	if (crnl != std::string::npos && crnl < longestLength)
		buf2Test.resize(crnl);

	if (buf2Test.rfind("#!", 0) == 0)
	{
		static const std::pair<const char*, LangType> shebangLangs[] = {
			{"sh", L_BASH}, {"python", L_PYTHON}, {"perl", L_PERL}, {"php", L_PHP}, {"ruby", L_RUBY}, {"node", L_JAVASCRIPT}};
		for (const auto& sl : shebangLangs)
		{
			if (buf2Test.find(sl.first) != std::string::npos)
				return sl.second;
		}
		return L_TEXT;
	}

	static const std::pair<const char*, LangType> firstLine[] = {
		{"<?xml", L_XML}, {"<?php", L_PHP}, {"<html", L_HTML}, {"<!DOCTYPE html", L_HTML}, {"<?", L_PHP}};
	for (const auto& fl : firstLine)
	{
		if (buf2Test.rfind(fl.first, 0) == 0)
			return fl.second;
	}
	return L_TEXT;
}

const LangDef* StyleManager::langDef(LangType lang) const
{
	const int i = static_cast<int>(lang);
	if (i < 0 || i >= static_cast<int>(_langs.size()))
		return nullptr;
	return &_langs[i];
}

const char* StyleManager::wordList(LangType lang, int index) const
{
	const LangDef* d = langDef(lang);
	if (!d || index < 0 || index >= NB_LIST || !d->hasKeywords[index])
		return nullptr;
	return d->keywords[index].c_str();
}

LexerStyler* StyleManager::lexerStylerByName(const QString& name)
{
	for (LexerStyler& ls : _lexerStylers)
	{
		if (ls.lexerName == name)
			return &ls;
	}
	return nullptr;
}

const LexerStyler* StyleManager::lexerStylerByName(const QString& name) const
{
	for (const LexerStyler& ls : _lexerStylers)
	{
		if (ls.lexerName == name)
			return &ls;
	}
	return nullptr;
}

QString StyleManager::defaultFontFamily() const
{
	const Style* def = _globalStyles.findByID(STYLE_DEFAULT);
	if (def && !def->fontName.isEmpty() && QFontDatabase::hasFamily(def->fontName))
		return def->fontName;
	return QFontDatabase::systemFont(QFontDatabase::FixedFont).family();
}

// ---------------------------------------------------------------------------
// StyleManager: applying styles (ScintillaEditView port)
// ---------------------------------------------------------------------------
void StyleManager::setSpecialStyle(ScintillaEditBase* sci, const Style& styleToSet) const
{
	const int styleID = styleToSet.styleID;
	if ((styleToSet.colorStyle & COLORSTYLE_FOREGROUND) && styleToSet.fgColor != static_cast<COLORREF>(STYLE_NOT_USED))
		ex(sci, SCI_STYLESETFORE, styleID, styleToSet.fgColor);

	if ((styleToSet.colorStyle & COLORSTYLE_BACKGROUND) && styleToSet.bgColor != static_cast<COLORREF>(STYLE_NOT_USED))
		ex(sci, SCI_STYLESETBACK, styleID, styleToSet.bgColor);

	if (!styleToSet.fontName.isEmpty())
	{
		QString family = styleToSet.fontName;
		if (!QFontDatabase::hasFamily(family))
			family = (styleID == STYLE_DEFAULT) ? defaultFontFamily() : QString();
		if (!family.isEmpty())
		{
			const QByteArray fontNameA = family.toUtf8();
			ex(sci, SCI_STYLESETFONT, styleID, reinterpret_cast<sptr_t>(fontNameA.constData()));
		}
	}
	else if (styleID == STYLE_DEFAULT)
	{
		const QByteArray fontNameA = defaultFontFamily().toUtf8();
		ex(sci, SCI_STYLESETFONT, styleID, reinterpret_cast<sptr_t>(fontNameA.constData()));
	}

	const int fontStyle = styleToSet.fontStyle;
	if (fontStyle != STYLE_NOT_USED)
	{
		ex(sci, SCI_STYLESETBOLD, styleID, fontStyle & FONTSTYLE_BOLD);
		ex(sci, SCI_STYLESETITALIC, styleID, fontStyle & FONTSTYLE_ITALIC);
		ex(sci, SCI_STYLESETUNDERLINE, styleID, fontStyle & FONTSTYLE_UNDERLINE);
	}

	if (styleToSet.fontSize > 0)
		ex(sci, SCI_STYLESETSIZE, styleID, styleToSet.fontSize);
}

void StyleManager::setLexerFromLangID(ScintillaEditBase* sci, LangType langID) const
{
	const char* lexerName = langInfo(langID).lexerID;
	ex(sci, SCI_SETILEXER, 0, reinterpret_cast<sptr_t>(CreateLexer(lexerName)));
}

const char* StyleManager::concatToBuildKeywordList(std::string& kwl, LangType langType, int keywordIndex) const
{
	kwl += " ";
	const char* defKwl = wordList(langType, keywordIndex);
	kwl += defKwl ? defKwl : "";
	return kwl.c_str();
}

void StyleManager::makeStyle(ScintillaEditBase* sci, LangType langType, const char** keywordArray) const
{
	const LexerStyler* pStyler = lexerStylerByName(QString::fromUtf8(langInfo(langType).langName));
	if (!pStyler)
		return;
	for (const Style& style : pStyler->styles)
	{
		setStyle(sci, style);
		if (keywordArray && style.keywordClass != STYLE_NOT_USED && style.keywordClass < NB_LIST && !style.keywords.empty())
			keywordArray[style.keywordClass] = style.keywords.c_str();
	}
}

void StyleManager::setKeywords(ScintillaEditBase* sci, LangType langType, const char* keywords, int index) const
{
	std::string wordList = keywords ? keywords : "";
	ex(sci, SCI_SETKEYWORDS, index, reinterpret_cast<sptr_t>(concatToBuildKeywordList(wordList, langType, index)));
}

void StyleManager::populateSubStyleKeywords(ScintillaEditBase* sci, LangType langType, int baseStyleID, int numSubStyles, int firstLangIndex, const char** pKwArray) const
{
	const int firstID = static_cast<int>(ex(sci, SCI_ALLOCATESUBSTYLES, baseStyleID, numSubStyles) & 0xFF);
	if (pKwArray && firstID >= 0)
	{
		for (int i = 0; i < numSubStyles; ++i)
		{
			const int ss = firstLangIndex + i;
			std::string userWords = pKwArray[ss] ? pKwArray[ss] : "";
			ex(sci, SCI_SETIDENTIFIERS, firstID + i, reinterpret_cast<sptr_t>(concatToBuildKeywordList(userWords, langType, ss)));
		}
	}
}

void StyleManager::setLexer(ScintillaEditBase* sci, LangType langType, int whichList, int baseStyleID, int numSubStyles) const
{
	setLexerFromLangID(sci, langType);

	const char* pKwArray[NB_LIST]{};
	makeStyle(sci, langType, pKwArray);

	static const int lists[] = {LIST_0, LIST_1, LIST_2, LIST_3, LIST_4, LIST_5, LIST_6, LIST_7, LIST_8};
	for (int i = 0; i < 9; ++i)
	{
		if (whichList & lists[i])
			setKeywords(sci, langType, pKwArray[i], i);
	}

	if (baseStyleID != STYLE_NOT_USED)
		populateSubStyleKeywords(sci, langType, baseStyleID, numSubStyles, LANG_INDEX_SUBSTYLE1, pKwArray);

	prop(sci, "fold", "1");
	prop(sci, "fold.compact", "0");
	prop(sci, "fold.comment", "1");
}

void StyleManager::setCppLexer(ScintillaEditBase* sci, LangType langType) const
{
	setLexerFromLangID(sci, L_CPP);

	if (langType == L_GOLANG)
		prop(sci, "lexer.cpp.backquoted.strings", "1");

	if (langType != L_RC)
	{
		const char* doxygenKeyWords = wordList(L_CPP, LANG_INDEX_TYPE2);
		if (doxygenKeyWords)
			ex(sci, SCI_SETKEYWORDS, 2, reinterpret_cast<sptr_t>(doxygenKeyWords));
	}

	const char* pKwArray[NB_LIST]{};
	makeStyle(sci, langType, pKwArray);

	std::string instr = pKwArray[LANG_INDEX_INSTR] ? pKwArray[LANG_INDEX_INSTR] : "";
	std::string types = pKwArray[LANG_INDEX_TYPE] ? pKwArray[LANG_INDEX_TYPE] : "";
	std::string global = pKwArray[LANG_INDEX_INSTR2] ? pKwArray[LANG_INDEX_INSTR2] : "";

	ex(sci, SCI_SETKEYWORDS, 0, reinterpret_cast<sptr_t>(concatToBuildKeywordList(instr, langType, LANG_INDEX_INSTR)));
	ex(sci, SCI_SETKEYWORDS, 1, reinterpret_cast<sptr_t>(concatToBuildKeywordList(types, langType, LANG_INDEX_TYPE)));
	ex(sci, SCI_SETKEYWORDS, 3, reinterpret_cast<sptr_t>(concatToBuildKeywordList(global, langType, LANG_INDEX_INSTR2)));

	populateSubStyleKeywords(sci, langType, SCE_C_IDENTIFIER, 8, LANG_INDEX_SUBSTYLE1, pKwArray);

	prop(sci, "fold", "1");
	prop(sci, "fold.compact", "0");
	prop(sci, "fold.comment", "1");
	prop(sci, "fold.cpp.comment.explicit", "0");
	prop(sci, "fold.preprocessor", "1");
	prop(sci, "lexer.cpp.track.preprocessor", "0");
}

void StyleManager::setJsLexer(ScintillaEditBase* sci) const
{
	const char* doxygenKeyWords = wordList(L_CPP, LANG_INDEX_TYPE2);

	setLexerFromLangID(sci, L_JAVASCRIPT);
	const char* pKwArray[NB_LIST]{};
	makeStyle(sci, L_JAVASCRIPT, pKwArray);

	if (doxygenKeyWords)
		ex(sci, SCI_SETKEYWORDS, 2, reinterpret_cast<sptr_t>(doxygenKeyWords));

	const LexerStyler* pNewStyler = lexerStylerByName(QString::fromUtf8(langInfo(L_JAVASCRIPT).langName));
	if (pNewStyler)
	{
		for (const Style& style : pNewStyler->styles)
			setStyle(sci, style);

		std::string instr = pKwArray[LANG_INDEX_INSTR] ? pKwArray[LANG_INDEX_INSTR] : "";
		std::string types = pKwArray[LANG_INDEX_TYPE] ? pKwArray[LANG_INDEX_TYPE] : "";
		std::string instr2 = pKwArray[LANG_INDEX_INSTR2] ? pKwArray[LANG_INDEX_INSTR2] : "";

		ex(sci, SCI_SETKEYWORDS, 0, reinterpret_cast<sptr_t>(concatToBuildKeywordList(instr, L_JAVASCRIPT, LANG_INDEX_INSTR)));
		ex(sci, SCI_SETKEYWORDS, 1, reinterpret_cast<sptr_t>(concatToBuildKeywordList(types, L_JAVASCRIPT, LANG_INDEX_TYPE)));
		ex(sci, SCI_SETKEYWORDS, 3, reinterpret_cast<sptr_t>(concatToBuildKeywordList(instr2, L_JAVASCRIPT, LANG_INDEX_INSTR2)));

		populateSubStyleKeywords(sci, L_JAVASCRIPT, SCE_C_IDENTIFIER, 8, LANG_INDEX_SUBSTYLE1, pKwArray);
	}
	else
	{
		const LexerStyler* pOldStyler = lexerStylerByName(QString::fromUtf8(langInfo(L_JS_EMBEDDED).langName));
		if (pOldStyler)
		{
			for (Style style : pOldStyler->styles)
			{
				int cppID = style.styleID;
				switch (style.styleID)
				{
					case SCE_HJ_DEFAULT: cppID = SCE_C_DEFAULT; break;
					case SCE_HJ_WORD: cppID = SCE_C_IDENTIFIER; break;
					case SCE_HJ_SYMBOLS: cppID = SCE_C_OPERATOR; break;
					case SCE_HJ_COMMENT: cppID = SCE_C_COMMENT; break;
					case SCE_HJ_COMMENTLINE: cppID = SCE_C_COMMENTLINE; break;
					case SCE_HJ_COMMENTDOC: cppID = SCE_C_COMMENTDOC; break;
					case SCE_HJ_NUMBER: cppID = SCE_C_NUMBER; break;
					case SCE_HJ_KEYWORD: cppID = SCE_C_WORD; break;
					case SCE_HJ_DOUBLESTRING: cppID = SCE_C_STRING; break;
					case SCE_HJ_SINGLESTRING: cppID = SCE_C_CHARACTER; break;
					case SCE_HJ_REGEX: cppID = SCE_C_REGEX; break;
				}
				style.styleID = cppID;
				setStyle(sci, style);
			}
		}
		ex(sci, SCI_STYLESETEOLFILLED, SCE_C_DEFAULT, true);
		ex(sci, SCI_STYLESETEOLFILLED, SCE_C_COMMENTLINE, true);
		ex(sci, SCI_STYLESETEOLFILLED, SCE_C_COMMENT, true);
		ex(sci, SCI_STYLESETEOLFILLED, SCE_C_COMMENTDOC, true);

		makeStyle(sci, L_JS_EMBEDDED, pKwArray);
		std::string instr = pKwArray[LANG_INDEX_INSTR] ? pKwArray[LANG_INDEX_INSTR] : "";
		ex(sci, SCI_SETKEYWORDS, 0, reinterpret_cast<sptr_t>(concatToBuildKeywordList(instr, L_JS_EMBEDDED, LANG_INDEX_INSTR)));
	}

	prop(sci, "fold", "1");
	prop(sci, "fold.compact", "0");
	prop(sci, "fold.comment", "1");
	prop(sci, "fold.cpp.comment.explicit", "0");
	prop(sci, "fold.preprocessor", "1");
	prop(sci, "lexer.cpp.track.preprocessor", "0");
	prop(sci, "lexer.cpp.backquoted.strings", "2");
}

void StyleManager::setTclLexer(ScintillaEditBase* sci) const
{
	setLexerFromLangID(sci, L_TCL);

	const char* pKwArray[NB_LIST]{};
	makeStyle(sci, L_TCL, pKwArray);

	// Scintilla keyword set index -> Notepad++ keyword class
	static const std::pair<int, int> map[] = {
		{0, LANG_INDEX_INSTR}, {1, LANG_INDEX_TYPE}, {2, LANG_INDEX_INSTR2}, {3, LANG_INDEX_TYPE2}, {4, LANG_INDEX_TYPE3},
		{5, LANG_INDEX_TYPE4}, {6, LANG_INDEX_TYPE5}, {7, LANG_INDEX_TYPE6}, {8, LANG_INDEX_TYPE7}};
	for (const auto& m : map)
	{
		std::string kw = pKwArray[m.second] ? pKwArray[m.second] : "";
		ex(sci, SCI_SETKEYWORDS, m.first, reinterpret_cast<sptr_t>(concatToBuildKeywordList(kw, L_TCL, m.second)));
	}
}

void StyleManager::setObjCLexer(ScintillaEditBase* sci, LangType langType) const
{
	setLexerFromLangID(sci, L_OBJC);

	const char* pKwArray[NB_LIST]{};
	makeStyle(sci, langType, pKwArray);

	std::string instr = pKwArray[LANG_INDEX_INSTR] ? pKwArray[LANG_INDEX_INSTR] : "";
	std::string directive = pKwArray[LANG_INDEX_INSTR2] ? pKwArray[LANG_INDEX_INSTR2] : "";
	std::string types = pKwArray[LANG_INDEX_TYPE] ? pKwArray[LANG_INDEX_TYPE] : "";
	std::string qualifier = pKwArray[LANG_INDEX_TYPE2] ? pKwArray[LANG_INDEX_TYPE2] : "";
	const char* doxygenKeyWords = wordList(L_CPP, LANG_INDEX_TYPE2);

	ex(sci, SCI_SETKEYWORDS, 0, reinterpret_cast<sptr_t>(concatToBuildKeywordList(instr, langType, LANG_INDEX_INSTR)));
	ex(sci, SCI_SETKEYWORDS, 1, reinterpret_cast<sptr_t>(concatToBuildKeywordList(types, langType, LANG_INDEX_TYPE)));
	ex(sci, SCI_SETKEYWORDS, 2, reinterpret_cast<sptr_t>(doxygenKeyWords ? doxygenKeyWords : ""));
	ex(sci, SCI_SETKEYWORDS, 3, reinterpret_cast<sptr_t>(concatToBuildKeywordList(directive, langType, LANG_INDEX_INSTR2)));
	ex(sci, SCI_SETKEYWORDS, 4, reinterpret_cast<sptr_t>(concatToBuildKeywordList(qualifier, langType, LANG_INDEX_TYPE2)));

	prop(sci, "fold", "1");
	prop(sci, "fold.compact", "0");
	prop(sci, "fold.comment", "1");
	prop(sci, "fold.cpp.comment.explicit", "0");
	prop(sci, "fold.preprocessor", "1");
}

void StyleManager::setTypeScriptLexer(ScintillaEditBase* sci) const
{
	const char* doxygenKeyWords = wordList(L_CPP, LANG_INDEX_TYPE2);
	setLexerFromLangID(sci, L_TYPESCRIPT);
	if (doxygenKeyWords)
		ex(sci, SCI_SETKEYWORDS, 2, reinterpret_cast<sptr_t>(doxygenKeyWords));

	const char* pKwArray[NB_LIST]{};
	makeStyle(sci, L_TYPESCRIPT, pKwArray);

	std::string instr = pKwArray[LANG_INDEX_INSTR] ? pKwArray[LANG_INDEX_INSTR] : "";
	std::string types = pKwArray[LANG_INDEX_TYPE] ? pKwArray[LANG_INDEX_TYPE] : "";
	ex(sci, SCI_SETKEYWORDS, 0, reinterpret_cast<sptr_t>(concatToBuildKeywordList(instr, L_TYPESCRIPT, LANG_INDEX_INSTR)));
	ex(sci, SCI_SETKEYWORDS, 1, reinterpret_cast<sptr_t>(concatToBuildKeywordList(types, L_TYPESCRIPT, LANG_INDEX_TYPE)));

	populateSubStyleKeywords(sci, L_TYPESCRIPT, SCE_C_IDENTIFIER, 8, LANG_INDEX_SUBSTYLE1, pKwArray);

	prop(sci, "fold", "1");
	prop(sci, "fold.compact", "0");
	prop(sci, "fold.comment", "1");
	prop(sci, "fold.cpp.comment.explicit", "0");
	prop(sci, "fold.preprocessor", "1");
	prop(sci, "lexer.cpp.track.preprocessor", "0");
	prop(sci, "lexer.cpp.backquoted.strings", "1");
}

void StyleManager::setXmlLexer(ScintillaEditBase* sci, LangType type) const
{
	if (type == L_XML)
	{
		const char* pKwArray[NB_LIST]{};
		setLexerFromLangID(sci, L_XML);
		makeStyle(sci, type, pKwArray);

		std::string keywordList = pKwArray[LANG_INDEX_INSTR] ? pKwArray[LANG_INDEX_INSTR] : "";
		ex(sci, SCI_SETKEYWORDS, 5, reinterpret_cast<sptr_t>(concatToBuildKeywordList(keywordList, L_XML, LANG_INDEX_INSTR)));
		populateSubStyleKeywords(sci, type, SCE_H_ATTRIBUTE, 8, LANG_INDEX_SUBSTYLE1, pKwArray);
		prop(sci, "lexer.xml.allow.scripts", "0");
	}
	else if (type == L_HTML || type == L_PHP || type == L_ASP || type == L_JSP)
	{
		setLexerFromLangID(sci, L_HTML);
		setHTMLLexer(sci);
		setEmbeddedJSLexer(sci);
		setEmbeddedPhpLexer(sci);
		setEmbeddedAspLexer(sci);
	}
	prop(sci, "fold", "1");
	prop(sci, "fold.compact", "0");
	prop(sci, "fold.html", "1");
	prop(sci, "fold.hypertext.comment", "1");
}

void StyleManager::setHTMLLexer(ScintillaEditBase* sci) const
{
	const char* pKwArray[NB_LIST]{};
	makeStyle(sci, L_HTML, pKwArray);

	std::string kw = pKwArray[LANG_INDEX_INSTR] ? pKwArray[LANG_INDEX_INSTR] : "";
	ex(sci, SCI_SETKEYWORDS, 0, reinterpret_cast<sptr_t>(concatToBuildKeywordList(kw, L_HTML, LANG_INDEX_INSTR)));
	std::string kw2 = pKwArray[LANG_INDEX_INSTR2] ? pKwArray[LANG_INDEX_INSTR2] : "";
	ex(sci, SCI_SETKEYWORDS, 5, reinterpret_cast<sptr_t>(concatToBuildKeywordList(kw2, L_HTML, LANG_INDEX_INSTR2)));

	populateSubStyleKeywords(sci, L_HTML, SCE_H_TAG, 4, LANG_INDEX_SUBSTYLE1, pKwArray);
	populateSubStyleKeywords(sci, L_HTML, SCE_H_ATTRIBUTE, 4, LANG_INDEX_SUBSTYLE5, pKwArray);
}

void StyleManager::setEmbeddedJSLexer(ScintillaEditBase* sci) const
{
	const char* pKwArray[NB_LIST]{};
	makeStyle(sci, L_JS_EMBEDDED, pKwArray);

	std::string kw = pKwArray[LANG_INDEX_INSTR] ? pKwArray[LANG_INDEX_INSTR] : "";
	ex(sci, SCI_SETKEYWORDS, 1, reinterpret_cast<sptr_t>(concatToBuildKeywordList(kw, L_JS_EMBEDDED, LANG_INDEX_INSTR)));
	populateSubStyleKeywords(sci, L_JS_EMBEDDED, SCE_HJ_WORD, 8, LANG_INDEX_SUBSTYLE1, pKwArray);
	ex(sci, SCI_STYLESETEOLFILLED, SCE_HJ_DEFAULT, true);
	ex(sci, SCI_STYLESETEOLFILLED, SCE_HJ_COMMENT, true);
	ex(sci, SCI_STYLESETEOLFILLED, SCE_HJ_COMMENTDOC, true);
	ex(sci, SCI_STYLESETEOLFILLED, SCE_HJ_TEMPLATELITERAL, true);
	ex(sci, SCI_STYLESETEOLFILLED, SCE_HJA_TEMPLATELITERAL, true);
}

void StyleManager::setEmbeddedPhpLexer(ScintillaEditBase* sci) const
{
	const char* pKwArray[NB_LIST]{};
	makeStyle(sci, L_PHP, pKwArray);

	std::string kw = pKwArray[LANG_INDEX_INSTR] ? pKwArray[LANG_INDEX_INSTR] : "";
	ex(sci, SCI_SETKEYWORDS, 4, reinterpret_cast<sptr_t>(concatToBuildKeywordList(kw, L_PHP, LANG_INDEX_INSTR)));
	populateSubStyleKeywords(sci, L_PHP, SCE_HPHP_WORD, 8, LANG_INDEX_SUBSTYLE1, pKwArray);
	ex(sci, SCI_STYLESETEOLFILLED, SCE_HPHP_DEFAULT, true);
	ex(sci, SCI_STYLESETEOLFILLED, SCE_HPHP_COMMENT, true);
}

void StyleManager::setEmbeddedAspLexer(ScintillaEditBase* sci) const
{
	const char* pKwArray[NB_LIST]{};
	makeStyle(sci, L_ASP, pKwArray);

	std::string kw = pKwArray[LANG_INDEX_INSTR] ? pKwArray[LANG_INDEX_INSTR] : "";
	prop(sci, "asp.default.language", "2");
	ex(sci, SCI_SETKEYWORDS, 2, reinterpret_cast<sptr_t>(concatToBuildKeywordList(kw, L_VB, LANG_INDEX_INSTR)));
	populateSubStyleKeywords(sci, L_ASP, SCE_HB_WORD, 8, LANG_INDEX_SUBSTYLE1, pKwArray);
	ex(sci, SCI_STYLESETEOLFILLED, SCE_HBA_DEFAULT, true);
}

void StyleManager::setJsonLexer(ScintillaEditBase* sci, bool isJson5) const
{
	setLexerFromLangID(sci, isJson5 ? L_JSON5 : L_JSON);

	const char* pKwArray[NB_LIST]{};
	makeStyle(sci, L_JSON, pKwArray);

	std::string kw = pKwArray[LANG_INDEX_INSTR] ? pKwArray[LANG_INDEX_INSTR] : "";
	std::string kw2 = pKwArray[LANG_INDEX_INSTR2] ? pKwArray[LANG_INDEX_INSTR2] : "";
	ex(sci, SCI_SETKEYWORDS, 0, reinterpret_cast<sptr_t>(concatToBuildKeywordList(kw, L_JSON, LANG_INDEX_INSTR)));
	ex(sci, SCI_SETKEYWORDS, 1, reinterpret_cast<sptr_t>(concatToBuildKeywordList(kw2, L_JSON, LANG_INDEX_INSTR2)));

	prop(sci, "fold", "1");
	prop(sci, "fold.compact", "0");
	prop(sci, "lexer.json.escape.sequence", "1");
	if (isJson5)
		prop(sci, "lexer.json.allow.comments", "1");
}

void StyleManager::setBashLexer(ScintillaEditBase* sci) const
{
	setLexerFromLangID(sci, L_BASH);

	const char* pKwArray[NB_LIST]{};
	makeStyle(sci, L_BASH, pKwArray);
	setKeywords(sci, L_BASH, pKwArray[LANG_INDEX_INSTR], LANG_INDEX_INSTR);

	populateSubStyleKeywords(sci, L_BASH, SCE_SH_IDENTIFIER, 4, LANG_INDEX_SUBSTYLE1, pKwArray);
	populateSubStyleKeywords(sci, L_BASH, SCE_SH_SCALAR, 4, LANG_INDEX_SUBSTYLE5, pKwArray);

	prop(sci, "fold", "1");
	prop(sci, "fold.compact", "0");
	prop(sci, "fold.comment", "1");
}

void StyleManager::setSpecialIndicator(ScintillaEditBase* sci, int indic, COLORREF color) const
{
	ex(sci, SCI_INDICSETFORE, indic, color);
}

void StyleManager::setTabSettings(ScintillaEditBase* sci, LangType lang) const
{
	const LangDef* d = langDef(lang);
	const NppPrefs& p = Npp::prefs();
	LangTabSetting user;
	if (p.langTab(QString::fromUtf8(langInfo(lang).langName), &user))
	{
		// Preferences > Indentation: value set for this language, or "Use default value"
		ex(sci, SCI_SETTABWIDTH, user.size > 0 ? user.size : (p.tabSize > 0 ? p.tabSize : 4));
		ex(sci, SCI_SETUSETABS, user.size > 0 ? !user.spaces : !p.replaceTabBySpace);
		ex(sci, SCI_SETBACKSPACEUNINDENTS, user.size > 0 ? user.backspaceUnindent : p.backspaceUnindent);
	}
	else if (d && d->tabSize > 0)
	{
		ex(sci, SCI_SETTABWIDTH, d->tabSize);
		ex(sci, SCI_SETUSETABS, !d->tabReplacedBySpace);
		ex(sci, SCI_SETBACKSPACEUNINDENTS, d->backspaceUnindent);
	}
	else
	{
		ex(sci, SCI_SETTABWIDTH, p.tabSize > 0 ? p.tabSize : 4);
		ex(sci, SCI_SETUSETABS, !p.replaceTabBySpace);
		ex(sci, SCI_SETBACKSPACEUNINDENTS, p.backspaceUnindent);
	}
}

static bool isNeededFolderMargin(LangType typeDoc)
{
	switch (typeDoc)
	{
		case L_ASCII:
		case L_BATCH:
		case L_TEXT:
		case L_MAKEFILE:
		case L_HASKELL:
		case L_SMALLTALK:
		case L_KIX:
		case L_ADA:
			return false;
		default:
			return true;
	}
}

static bool isPythonStyleIndentation(LangType typeDoc)
{
	return (typeDoc == L_PYTHON || typeDoc == L_COFFEESCRIPT || typeDoc == L_HASKELL ||
		typeDoc == L_C || typeDoc == L_CPP || typeDoc == L_OBJC || typeDoc == L_CS || typeDoc == L_JAVA ||
		typeDoc == L_PHP || typeDoc == L_JS_EMBEDDED || typeDoc == L_JAVASCRIPT || typeDoc == L_MAKEFILE ||
		typeDoc == L_ASN1 || typeDoc == L_GDSCRIPT);
}

void StyleManager::defineDocType(ScintillaEditBase* sci, LangType typeDoc, const UserLang* udl, sptr_t bufferId) const
{
	const Style* pStyleDefault = _globalStyles.findByID(STYLE_DEFAULT);
	if (pStyleDefault)
	{
		Style s = *pStyleDefault;
		s.colorStyle = COLORSTYLE_ALL;
		setStyle(sci, s);
	}
	else
	{
		Style s;
		s.styleID = STYLE_DEFAULT;
		s.fgColor = black;
		s.bgColor = white;
		s.fontSize = 10;
		setStyle(sci, s);
	}

	ex(sci, SCI_STYLECLEARALL);

	struct IndicDef { int id; COLORREF color; };
	const IndicDef indics[] = {
		{SCE_UNIVERSAL_FOUND_STYLE, red},           {SCE_UNIVERSAL_FOUND_STYLE_SMART, liteGreen},
		{SCE_UNIVERSAL_FOUND_STYLE_INC, blue},      {SCE_UNIVERSAL_TAGMATCH, rgb(0x80, 0x00, 0xFF)},
		{SCE_UNIVERSAL_TAGATTR, yellow},            {SCE_UNIVERSAL_FOUND_STYLE_EXT1, cyan},
		{SCE_UNIVERSAL_FOUND_STYLE_EXT2, orange},   {SCE_UNIVERSAL_FOUND_STYLE_EXT3, yellow},
		{SCE_UNIVERSAL_FOUND_STYLE_EXT4, purple},   {SCE_UNIVERSAL_FOUND_STYLE_EXT5, darkGreen},
	};
	for (const IndicDef& d : indics)
	{
		const Style* pStyle = _globalStyles.findByID(d.id);
		const COLORREF c = (pStyle && pStyle->bgColor != static_cast<COLORREF>(STYLE_NOT_USED)) ? pStyle->bgColor : d.color;
		setSpecialIndicator(sci, d.id, c);
		if (d.id == SCE_UNIVERSAL_FOUND_STYLE_INC)
			ex(sci, SCI_INDICSETFORE, FINDDLG_HIGHLIGHT_INDIC, c);
	}

	const NppPrefs& p = Npp::prefs();
	ex(sci, SCI_SETMARGINWIDTHN, MARGIN_FOLDER, (p.folderStyle != FOLDER_STYLE_NONE && isNeededFolderMargin(typeDoc)) ? 14 : 0);

	switch (typeDoc)
	{
		case L_C:
		case L_CPP:
		case L_JAVA:
		case L_RC:
		case L_CS:
		case L_FLASH:
		case L_SWIFT:
		case L_GOLANG:
			setCppLexer(sci, typeDoc);
			break;

		case L_JS_EMBEDDED:
		case L_JAVASCRIPT:
			setJsLexer(sci);
			break;

		case L_TCL: setTclLexer(sci); break;
		case L_OBJC: setObjCLexer(sci, typeDoc); break;

		case L_PHP:
		case L_ASP:
		case L_JSP:
		case L_HTML:
		case L_XML:
			setXmlLexer(sci, typeDoc);
			break;

		case L_JSON: setJsonLexer(sci, false); break;
		case L_JSON5: setJsonLexer(sci, true); break;
		case L_CSS: setLexer(sci, L_CSS, LIST_0 | LIST_1 | LIST_4 | LIST_6); break;
		case L_LUA: setLexer(sci, L_LUA, LIST_0 | LIST_1 | LIST_2 | LIST_3 | LIST_4 | LIST_5 | LIST_6 | LIST_7, SCE_LUA_IDENTIFIER, 4); break;
		case L_MAKEFILE: setLexer(sci, L_MAKEFILE, LIST_NONE); break;
		case L_INI:
			setLexer(sci, L_INI, LIST_NONE);
			ex(sci, SCI_STYLESETEOLFILLED, SCE_PROPS_SECTION, true);
			break;
		case L_PROPS:
			setLexer(sci, L_PROPS, LIST_NONE);
			ex(sci, SCI_STYLESETEOLFILLED, SCE_PROPS_SECTION, true);
			break;

		case L_ASCII:
		{
			const LexerStyler* pStyler = lexerStylerByName(QStringLiteral("nfo"));
			Style nfoStyle;
			nfoStyle.styleID = STYLE_DEFAULT;
			nfoStyle.fontName = defaultFontFamily();
			nfoStyle.fontSize = 10;
			if (pStyler)
			{
				const Style* pDefStyle = pStyler->findByName(QStringLiteral("DEFAULT"));
				if (pDefStyle)
				{
					nfoStyle.bgColor = pDefStyle->bgColor;
					nfoStyle.fgColor = pDefStyle->fgColor;
					if (!pDefStyle->fontName.isEmpty())
					{
						nfoStyle.fontName = pDefStyle->fontName;
						if (pDefStyle->fontSize != STYLE_NOT_USED)
							nfoStyle.fontSize = pDefStyle->fontSize;
						if (pDefStyle->fontStyle != STYLE_NOT_USED)
							nfoStyle.fontStyle = pDefStyle->fontStyle;
					}
					nfoStyle.colorStyle = pDefStyle->colorStyle;
				}
			}
			setSpecialStyle(sci, nfoStyle);
			ex(sci, SCI_STYLECLEARALL);
			setLexerFromLangID(sci, L_TEXT);
			break;
		}

		case L_SQL:
			setLexer(sci, L_SQL, LIST_0 | LIST_1 | LIST_4);
			prop(sci, "sql.backslash.escapes", p.backslashIsEscapeForSql ? "1" : "0");
			break;
		case L_MSSQL: setLexer(sci, L_MSSQL, LIST_0 | LIST_1 | LIST_2 | LIST_3 | LIST_4 | LIST_5); break;
		case L_VB: setLexer(sci, L_VB, LIST_0); break;
		case L_PASCAL:
			setLexer(sci, L_PASCAL, LIST_0);
			prop(sci, "fold.preprocessor", "1");
			break;
		case L_PERL: setLexer(sci, L_PERL, LIST_0); break;
		case L_PYTHON:
			setLexer(sci, L_PYTHON, LIST_0 | LIST_1, SCE_P_IDENTIFIER);
			prop(sci, "fold.quotes.python", "1");
			prop(sci, "lexer.python.decorator.attributes", "1");
			prop(sci, "lexer.python.identifier.attributes", "1");
			break;
		case L_GDSCRIPT:
			setLexer(sci, L_GDSCRIPT, LIST_0 | LIST_1, SCE_GD_IDENTIFIER);
			prop(sci, "lexer.gdscript.keywords2.no.sub.identifiers", "1");
			prop(sci, "lexer.gdscript.whinge.level", "1");
			break;
		case L_BATCH: setLexer(sci, L_BATCH, LIST_0); break;
		case L_TEX:
			for (int i = 0; i < 4; ++i)
				ex(sci, SCI_SETKEYWORDS, i, reinterpret_cast<sptr_t>(""));
			setLexer(sci, L_TEX, LIST_NONE);
			break;
		case L_NSIS: setLexer(sci, L_NSIS, LIST_0 | LIST_1 | LIST_2 | LIST_3); break;
		case L_BASH: setBashLexer(sci); break;
		case L_FORTRAN: setLexer(sci, L_FORTRAN, LIST_0 | LIST_1 | LIST_2); break;
		case L_FORTRAN_77: setLexer(sci, L_FORTRAN_77, LIST_0 | LIST_1 | LIST_2); break;
		case L_LISP: setLexer(sci, L_LISP, LIST_0 | LIST_1); break;
		case L_SCHEME: setLexer(sci, L_SCHEME, LIST_0 | LIST_1); break;
		case L_ASM:
			setLexer(sci, L_ASM, LIST_0 | LIST_1 | LIST_2 | LIST_3 | LIST_4 | LIST_5 | LIST_6 | LIST_7);
			prop(sci, "fold.asm.syntax.based", "1");
			prop(sci, "fold.asm.comment.multiline", "1");
			prop(sci, "fold.asm.comment.explicit", "1");
			break;
		case L_DIFF: setLexer(sci, L_DIFF, LIST_NONE); break;
		case L_PS: setLexer(sci, L_PS, LIST_0 | LIST_1 | LIST_2 | LIST_3); break;
		case L_RUBY: setLexer(sci, L_RUBY, LIST_0); break;
		case L_SMALLTALK: setLexer(sci, L_SMALLTALK, LIST_0); break;
		case L_VHDL: setLexer(sci, L_VHDL, LIST_0 | LIST_1 | LIST_2 | LIST_3 | LIST_4 | LIST_5 | LIST_6); break;
		case L_KIX: setLexer(sci, L_KIX, LIST_0 | LIST_1 | LIST_2); break;
		case L_CAML: setLexer(sci, L_CAML, LIST_0 | LIST_1 | LIST_2); break;
		case L_ADA: setLexer(sci, L_ADA, LIST_0); break;
		case L_VERILOG:
			setLexer(sci, L_VERILOG, LIST_0 | LIST_1);
			prop(sci, "fold.preprocessor", "1");
			break;
		case L_AU3:
			setLexer(sci, L_AU3, LIST_0 | LIST_1 | LIST_2 | LIST_3 | LIST_4 | LIST_5 | LIST_6);
			prop(sci, "fold.preprocessor", "1");
			break;
		case L_MATLAB: setLexer(sci, L_MATLAB, LIST_0); break;
		case L_HASKELL: setLexer(sci, L_HASKELL, LIST_0); break;
		case L_INNO: setLexer(sci, L_INNO, LIST_0 | LIST_1 | LIST_2 | LIST_3 | LIST_4 | LIST_5); break;
		case L_CMAKE: setLexer(sci, L_CMAKE, LIST_0 | LIST_1 | LIST_2); break;
		case L_YAML: setLexer(sci, L_YAML, LIST_0); break;
		case L_COBOL:
			setLexer(sci, L_COBOL, LIST_0 | LIST_1 | LIST_2);
			prop(sci, "fold.preprocessor", "1");
			break;
		case L_GUI4CLI: setLexer(sci, L_GUI4CLI, LIST_0 | LIST_1 | LIST_2 | LIST_3 | LIST_4); break;
		case L_D: setLexer(sci, L_D, LIST_0 | LIST_1 | LIST_2 | LIST_3 | LIST_4 | LIST_5 | LIST_6); break;
		case L_POWERSHELL: setLexer(sci, L_POWERSHELL, LIST_0 | LIST_1 | LIST_2 | LIST_3 | LIST_4 | LIST_5); break;
		case L_R:
			setLexer(sci, L_R, LIST_0 | LIST_1 | LIST_2);
			prop(sci, "lexer.r.escape.sequence", "1");
			break;
		case L_COFFEESCRIPT: setLexer(sci, L_COFFEESCRIPT, LIST_0 | LIST_1 | LIST_2 | LIST_3); break;
		case L_BAANC:
			setLexer(sci, L_BAANC, LIST_0 | LIST_1 | LIST_2 | LIST_3 | LIST_4 | LIST_5 | LIST_6 | LIST_7 | LIST_8);
			prop(sci, "lexer.baan.styling.within.preprocessor", "1");
			prop(sci, "fold.preprocessor", "1");
			prop(sci, "fold.baan.syntax.based", "1");
			prop(sci, "fold.baan.keywords.based", "1");
			prop(sci, "fold.baan.sections", "1");
			prop(sci, "fold.baan.inner.level", "1");
			break;
		case L_SREC: setLexer(sci, L_SREC, LIST_NONE); break;
		case L_IHEX: setLexer(sci, L_IHEX, LIST_NONE); break;
		case L_TEHEX: setLexer(sci, L_TEHEX, LIST_NONE); break;
		case L_ASN1: setLexer(sci, L_ASN1, LIST_0 | LIST_1 | LIST_2 | LIST_3); break;
		case L_AVS: setLexer(sci, L_AVS, LIST_0 | LIST_1 | LIST_2 | LIST_3 | LIST_4 | LIST_5); break;
		case L_BLITZBASIC: setLexer(sci, L_BLITZBASIC, LIST_0 | LIST_1 | LIST_2 | LIST_3); break;
		case L_PUREBASIC: setLexer(sci, L_PUREBASIC, LIST_0 | LIST_1 | LIST_2 | LIST_3); break;
		case L_FREEBASIC: setLexer(sci, L_FREEBASIC, LIST_0 | LIST_1 | LIST_2 | LIST_3); break;
		case L_CSOUND: setLexer(sci, L_CSOUND, LIST_0 | LIST_1 | LIST_2); break;
		case L_ERLANG: setLexer(sci, L_ERLANG, LIST_0 | LIST_1 | LIST_2 | LIST_3 | LIST_4 | LIST_5); break;
		case L_ESCRIPT: setLexer(sci, L_ESCRIPT, LIST_0 | LIST_1 | LIST_2); break;
		case L_FORTH: setLexer(sci, L_FORTH, LIST_0 | LIST_1 | LIST_2 | LIST_3 | LIST_4 | LIST_5); break;
		case L_LATEX: setLexer(sci, L_LATEX, LIST_NONE); break;
		case L_MMIXAL: setLexer(sci, L_MMIXAL, LIST_0 | LIST_1 | LIST_2); break;
		case L_NIM: setLexer(sci, L_NIM, LIST_0); break;
		case L_NNCRONTAB: setLexer(sci, L_NNCRONTAB, LIST_0 | LIST_1 | LIST_2); break;
		case L_OSCRIPT: setLexer(sci, L_OSCRIPT, LIST_0 | LIST_1 | LIST_2 | LIST_3 | LIST_4 | LIST_5); break;
		case L_REBOL: setLexer(sci, L_REBOL, LIST_0 | LIST_1 | LIST_2 | LIST_3 | LIST_4 | LIST_5 | LIST_6); break;
		case L_REGISTRY: setLexer(sci, L_REGISTRY, LIST_NONE); break;
		case L_RUST: setLexer(sci, L_RUST, LIST_0 | LIST_1 | LIST_2 | LIST_3 | LIST_4 | LIST_5 | LIST_6); break;
		case L_SPICE: setLexer(sci, L_SPICE, LIST_0 | LIST_1 | LIST_2); break;
		case L_TXT2TAGS: setLexer(sci, L_TXT2TAGS, LIST_NONE); break;
		case L_VISUALPROLOG: setLexer(sci, L_VISUALPROLOG, LIST_0 | LIST_1 | LIST_2 | LIST_3); break;
		case L_TYPESCRIPT: setTypeScriptLexer(sci); break;
		case L_HOLLYWOOD: setLexer(sci, L_HOLLYWOOD, LIST_0 | LIST_1 | LIST_2 | LIST_3); break;
		case L_RAKU:
			setLexer(sci, L_RAKU, LIST_0 | LIST_1 | LIST_2 | LIST_3 | LIST_4 | LIST_5 | LIST_6);
			prop(sci, "fold.raku.comment.multiline", "1");
			prop(sci, "fold.raku.comment.pod", "1");
			break;
		case L_TOML: setLexer(sci, L_TOML, LIST_0); break;
		case L_SAS: setLexer(sci, L_SAS, LIST_0 | LIST_1 | LIST_2 | LIST_3); break;
		case L_STTXT:
			setLexer(sci, L_STTXT, LIST_0 | LIST_1 | LIST_2 | LIST_3 | LIST_4 | LIST_5);
			prop(sci, "fold.preprocessor", "1");
			break;
		case L_ERRORLIST:
			setLexer(sci, L_ERRORLIST, LIST_NONE);
			ex(sci, SCI_STYLESETVISIBLE, SCE_ERR_ESCSEQ, false);
			ex(sci, SCI_STYLESETVISIBLE, SCE_ERR_ESCSEQ_UNKNOWN, false);
			prop(sci, "lexer.errorlist.value.separate", "0");
			prop(sci, "lexer.errorlist.escape.sequences", "1");
			break;
		case L_ESCSEQ:
			setLexer(sci, L_ESCSEQ, LIST_NONE);
			ex(sci, SCI_STYLESETVISIBLE, SCE_ESCSEQ_IDENTIFIER, false);
			ex(sci, SCI_STYLESETVISIBLE, SCE_ESCSEQ_UNKNOWN, false);
			prop(sci, "lexer.escseq.colour.text", "1");
			break;
		case L_SEARCHRESULT:
			ex(sci, SCI_STYLESETEOLFILLED, SCE_SEARCHRESULT_FILE_HEADER, true);
			ex(sci, SCI_STYLESETEOLFILLED, SCE_SEARCHRESULT_SEARCH_HEADER, true);
			setLexer(sci, L_SEARCHRESULT, LIST_NONE);
			break;

		case L_USER:
			// ScintillaEditView::setUserLexer()
			if (udl)
				UdlManager::setUserLexer(sci, *udl, bufferId);
			else
				setLexerFromLangID(sci, L_TEXT);
			break;

		case L_TEXT:
		default:
			setLexerFromLangID(sci, L_TEXT);
			break;
	}

	// global styles that apply on top of the lexer styles
	for (int id : {STYLE_INDENTGUIDE, STYLE_BRACELIGHT, STYLE_BRACEBAD})
	{
		const Style* pStyle = _globalStyles.findByID(id);
		if (pStyle)
			setStyle(sci, *pStyle);
	}
	if (const Style* pStyle = _globalStyles.findByID(STYLE_LINENUMBER))
		setSpecialStyle(sci, *pStyle);

	setTabSettings(sci, typeDoc);

	if (p.showIndentGuide)
		ex(sci, SCI_SETINDENTATIONGUIDES, isPythonStyleIndentation(typeDoc) ? SC_IV_LOOKFORWARD : SC_IV_LOOKBOTH);
}

void StyleManager::performGlobalStyles(ScintillaEditBase* sci) const
{
	const NppPrefs& p = Npp::prefs();
	const StyleArray& stylers = _globalStyles;
	const Style* pStyle = nullptr;

	auto element = [sci](int el, COLORREF c) { ex(sci, SCI_SETELEMENTCOLOUR, el, static_cast<sptr_t>(c | 0xFF000000)); };

	if (p.currentLineMode != LINEHILITE_NONE)
	{
		pStyle = stylers.findByName(QStringLiteral("Current line background colour"));
		if (pStyle)
			element(SC_ELEMENT_CARET_LINE_BACK, pStyle->bgColor);
		ex(sci, SCI_SETCARETLINEVISIBLEALWAYS, true);
	}
	else
	{
		ex(sci, SCI_RESETELEMENTCOLOUR, SC_ELEMENT_CARET_LINE_BACK);
	}
	ex(sci, SCI_SETCARETLINEFRAME, (p.currentLineMode == LINEHILITE_FRAME) ? p.currentLineFrameWidth : 0);

	COLORREF selectColorBack = grey;
	COLORREF selectColorFore = black;
	pStyle = stylers.findByName(QStringLiteral("Selected text colour"));
	if (pStyle)
	{
		selectColorBack = pStyle->bgColor;
		selectColorFore = pStyle->fgColor;
	}
	element(SC_ELEMENT_SELECTION_BACK, selectColorBack);
	element(SC_ELEMENT_SELECTION_INACTIVE_BACK, selectColorBack);
	for (int el : {SC_ELEMENT_SELECTION_TEXT, SC_ELEMENT_SELECTION_INACTIVE_TEXT, SC_ELEMENT_SELECTION_ADDITIONAL_TEXT})
	{
		if (p.selectedTextForegroundSingleColor)
			element(el, selectColorFore);
		else
			ex(sci, SCI_RESETELEMENTCOLOUR, el);
	}

	COLORREF multiSelBack = liteGrey;
	pStyle = stylers.findByName(QStringLiteral("Multi-selected text color"));
	if (pStyle)
		multiSelBack = pStyle->bgColor;
	element(SC_ELEMENT_SELECTION_ADDITIONAL_BACK, multiSelBack);

	COLORREF caretColor = black;
	pStyle = stylers.findByName(QStringLiteral("Caret colour"));
	if (pStyle)
		caretColor = pStyle->fgColor;
	element(SC_ELEMENT_CARET, caretColor);

	COLORREF multiEditCaretColor = darkGrey;
	pStyle = stylers.findByName(QStringLiteral("Multi-edit carets color"));
	if (pStyle)
		multiEditCaretColor = pStyle->fgColor;
	element(SC_ELEMENT_CARET_ADDITIONAL, multiEditCaretColor);

	COLORREF edgeColor = liteGrey;
	pStyle = stylers.findByName(QStringLiteral("Edge colour"));
	if (pStyle)
		edgeColor = pStyle->fgColor;
	ex(sci, SCI_SETEDGECOLOUR, edgeColor);

	COLORREF foldMarginColor = grey;
	COLORREF foldMarginHiColor = white;
	pStyle = stylers.findByName(QStringLiteral("Fold margin"));
	if (pStyle)
	{
		foldMarginHiColor = pStyle->fgColor;
		foldMarginColor = pStyle->bgColor;
	}
	ex(sci, SCI_SETFOLDMARGINCOLOUR, true, foldMarginColor);
	ex(sci, SCI_SETFOLDMARGINHICOLOUR, true, foldMarginHiColor);

	COLORREF bookmarkMarginColor = veryLiteGrey;
	pStyle = stylers.findByName(QStringLiteral("Bookmark margin"));
	if (!pStyle)
		pStyle = stylers.findByName(QStringLiteral("Line number margin"));
	if (pStyle)
		bookmarkMarginColor = pStyle->bgColor;
	ex(sci, SCI_SETMARGINTYPEN, MARGIN_SYMBOL, SC_MARGIN_COLOUR);
	ex(sci, SCI_SETMARGINBACKN, MARGIN_SYMBOL, bookmarkMarginColor);

	COLORREF changeHistoryMarginColor = veryLiteGrey;
	pStyle = stylers.findByName(QStringLiteral("Change History margin"));
	if (!pStyle)
		pStyle = stylers.findByName(QStringLiteral("Line number margin"));
	if (pStyle)
		changeHistoryMarginColor = pStyle->bgColor;
	ex(sci, SCI_SETMARGINTYPEN, MARGIN_CHANGEHISTORY, SC_MARGIN_COLOUR);
	ex(sci, SCI_SETMARGINBACKN, MARGIN_CHANGEHISTORY, changeHistoryMarginColor);

	struct HistoryDef { const char* name; COLORREF def; int marker; int indicIns; int indicDel; };
	const HistoryDef history[] = {
		{"Change History modified", orange, SC_MARKNUM_HISTORY_MODIFIED, INDICATOR_HISTORY_MODIFIED_INSERTION, INDICATOR_HISTORY_MODIFIED_DELETION},
		{"Change History revert modified", yellowGreen, SC_MARKNUM_HISTORY_REVERTED_TO_MODIFIED, INDICATOR_HISTORY_REVERTED_TO_MODIFIED_INSERTION, INDICATOR_HISTORY_REVERTED_TO_MODIFIED_DELETION},
		{"Change History revert origin", darkCyan, SC_MARKNUM_HISTORY_REVERTED_TO_ORIGIN, INDICATOR_HISTORY_REVERTED_TO_ORIGIN_INSERTION, INDICATOR_HISTORY_REVERTED_TO_ORIGIN_DELETION},
		{"Change History saved", midGreen, SC_MARKNUM_HISTORY_SAVED, INDICATOR_HISTORY_SAVED_INSERTION, INDICATOR_HISTORY_SAVED_DELETION},
	};
	for (const HistoryDef& h : history)
	{
		COLORREF fg = h.def;
		COLORREF bg = h.def;
		pStyle = stylers.findByName(QString::fromLatin1(h.name));
		if (pStyle)
		{
			fg = pStyle->fgColor;
			bg = pStyle->bgColor;
		}
		ex(sci, SCI_MARKERSETFORE, h.marker, fg);
		ex(sci, SCI_MARKERSETBACK, h.marker, bg);
		ex(sci, SCI_INDICSETFORE, h.indicIns, fg);
		ex(sci, SCI_INDICSETFORE, h.indicDel, fg);
	}

	COLORREF urlHoveredFG = grey;
	pStyle = stylers.findByName(QStringLiteral("URL hovered"));
	if (pStyle)
		urlHoveredFG = pStyle->fgColor;
	ex(sci, SCI_INDICSETHOVERFORE, URL_INDIC, urlHoveredFG);

	// fold markers
	COLORREF foldfgColor = white, foldbgColor = grey, activeFoldFgColor = red;
	pStyle = stylers.findByName(QStringLiteral("Fold"));
	if (pStyle)
	{
		foldfgColor = pStyle->bgColor;
		foldbgColor = pStyle->fgColor;
	}
	pStyle = stylers.findByName(QStringLiteral("Fold active"));
	if (pStyle)
		activeFoldFgColor = pStyle->fgColor;

	static const int markersArray[][7] = {
		{SC_MARKNUM_FOLDEROPEN, SC_MARKNUM_FOLDER, SC_MARKNUM_FOLDERSUB, SC_MARKNUM_FOLDERTAIL, SC_MARKNUM_FOLDEREND, SC_MARKNUM_FOLDEROPENMID, SC_MARKNUM_FOLDERMIDTAIL},
		{SC_MARK_MINUS, SC_MARK_PLUS, SC_MARK_EMPTY, SC_MARK_EMPTY, SC_MARK_EMPTY, SC_MARK_EMPTY, SC_MARK_EMPTY},
		{SC_MARK_ARROWDOWN, SC_MARK_ARROW, SC_MARK_EMPTY, SC_MARK_EMPTY, SC_MARK_EMPTY, SC_MARK_EMPTY, SC_MARK_EMPTY},
		{SC_MARK_CIRCLEMINUS, SC_MARK_CIRCLEPLUS, SC_MARK_VLINE, SC_MARK_LCORNERCURVE, SC_MARK_CIRCLEPLUSCONNECTED, SC_MARK_CIRCLEMINUSCONNECTED, SC_MARK_TCORNERCURVE},
		{SC_MARK_BOXMINUS, SC_MARK_BOXPLUS, SC_MARK_VLINE, SC_MARK_LCORNER, SC_MARK_BOXPLUSCONNECTED, SC_MARK_BOXMINUSCONNECTED, SC_MARK_TCORNER}};
	const int style = (p.folderStyle >= FOLDER_STYLE_SIMPLE && p.folderStyle <= FOLDER_STYLE_BOX) ? p.folderStyle : FOLDER_STYLE_BOX;
	for (int j = 0; j < 7; ++j)
	{
		ex(sci, SCI_MARKERDEFINE, markersArray[0][j], markersArray[style][j]);
		ex(sci, SCI_MARKERSETFORE, markersArray[0][j], foldfgColor);
		ex(sci, SCI_MARKERSETBACK, markersArray[0][j], foldbgColor);
		ex(sci, SCI_MARKERSETBACKSELECTED, markersArray[0][j], activeFoldFgColor);
	}
	ex(sci, SCI_MARKERENABLEHIGHLIGHT, true);

	COLORREF wsSymbolFgColor = black;
	pStyle = stylers.findByName(QStringLiteral("White space symbol"));
	if (pStyle)
		wsSymbolFgColor = pStyle->fgColor;
	ex(sci, SCI_SETWHITESPACEFORE, true, wsSymbolFgColor);

	applyEolAppearance(sci);
}

void StyleManager::applyEolAppearance(ScintillaEditBase* sci) const
{
	const NppPrefs& p = Npp::prefs();
	const StyleArray& stylers = _globalStyles;
	const Style* pStyle = nullptr;
	COLORREF eolCustomColor = liteGrey;
	pStyle = stylers.findByName(QStringLiteral("EOL custom color"));
	if (pStyle)
		eolCustomColor = pStyle->fgColor;
	long appearance = SC_REPRESENTATION_BLOB;
	switch (p.eolMode)
	{
		case CRLF_PLAINTEXT: appearance = SC_REPRESENTATION_PLAIN; break;
		case CRLF_PLAINTEXT_CUSTOM: appearance = SC_REPRESENTATION_PLAIN | SC_REPRESENTATION_COLOUR; break;
		case CRLF_ROUNDEDRECT_CUSTOM: appearance = SC_REPRESENTATION_BLOB | SC_REPRESENTATION_COLOUR; break;
		default: appearance = SC_REPRESENTATION_BLOB; break;
	}
	const char* cr = "\x0d";
	const char* lf = "\x0a";
	ex(sci, SCI_SETREPRESENTATIONCOLOUR, reinterpret_cast<uptr_t>(cr), static_cast<sptr_t>(eolCustomColor | 0xFF000000));
	ex(sci, SCI_SETREPRESENTATIONCOLOUR, reinterpret_cast<uptr_t>(lf), static_cast<sptr_t>(eolCustomColor | 0xFF000000));
	ex(sci, SCI_SETREPRESENTATIONAPPEARANCE, reinterpret_cast<uptr_t>(cr), appearance);
	ex(sci, SCI_SETREPRESENTATIONAPPEARANCE, reinterpret_cast<uptr_t>(lf), appearance);
}
