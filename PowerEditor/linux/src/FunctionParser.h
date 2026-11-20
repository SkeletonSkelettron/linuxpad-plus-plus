// This file is part of Notepad++ project
// Copyright (C)2021 Don HO <don.h@free.fr>

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

// Linux copy of WinControls/FunctionList/functionParser.{h,cpp}: the same parsers on
// the Qt EditView, with pugixml for the rule files. Only the accessors differ.

#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <QString>

#include "pugixml.hpp"
#include "LangType.h"

class EditView;

struct foundInfo final
{
	std::string _data;
	std::string _data2;
	intptr_t _pos = -1;
	intptr_t _pos2 = -1;
};

class FunctionParser
{
friend class FunctionParsersManager;
public:
	FunctionParser(const char* id, const char* displayName, const std::string& commentExpr, const std::string& functionExpr, const std::vector<std::string>& functionNameExprArray, const std::vector<std::string>& classNameExprArray) noexcept
		: _id(id), _displayName(displayName), _commentExpr(commentExpr), _functionExpr(functionExpr), _functionNameExprArray(functionNameExprArray), _classNameExprArray(classNameExprArray)
	{}

	virtual void parse(std::vector<foundInfo>& foundInfos, size_t begin, size_t end, EditView* view, const std::string& classStructName = "") = 0;
	void funcParse(std::vector<foundInfo>& foundInfos, size_t begin, size_t end, EditView* view, const std::string& classStructName = "", const std::vector<std::pair<size_t, size_t>>* commentZones = nullptr);
	static bool isInZones(size_t pos2Test, const std::vector<std::pair<size_t, size_t>>& zones);
	virtual ~FunctionParser() = default;

protected:
	std::string _id;
	std::string _displayName;
	std::string _commentExpr;
	std::string _functionExpr;
	std::vector<std::string> _functionNameExprArray;
	std::vector<std::string> _classNameExprArray;
	void getCommentZones(std::vector<std::pair<size_t, size_t>>& commentZone, size_t begin, size_t end, EditView* view);
	void getInvertZones(std::vector<std::pair<size_t, size_t>>& destZones, const std::vector<std::pair<size_t, size_t>>& sourceZones, size_t begin, size_t end);
	static std::string parseSubLevel(size_t begin, size_t end, std::vector<std::string> dataToSearch, intptr_t& foundPos, EditView* view);
};


class FunctionZoneParser : public FunctionParser
{
public:
	FunctionZoneParser() = delete;
	FunctionZoneParser(const char* id, const char* displayName, const std::string& commentExpr, const std::string& rangeExpr, const std::string& openSymbole, const std::string& closeSymbole,
		const std::vector<std::string>& classNameExprArray, const std::string& functionExpr, const std::vector<std::string>& functionNameExprArray) noexcept
		: FunctionParser(id, displayName, commentExpr, functionExpr, functionNameExprArray, classNameExprArray), _rangeExpr(rangeExpr), _openSymbole(openSymbole), _closeSymbole(closeSymbole)
	{}

	void parse(std::vector<foundInfo>& foundInfos, size_t begin, size_t end, EditView* view, const std::string& classStructName = "") override;

protected:
	void classParse(std::vector<foundInfo>& foundInfos, std::vector<std::pair<size_t, size_t>>& scannedZones, const std::vector<std::pair<size_t, size_t>>& commentZones, size_t begin, size_t end, EditView* view, const std::string& classStructName = "");

private:
	std::string _rangeExpr;
	std::string _openSymbole;
	std::string _closeSymbole;

	static size_t getBodyClosePos(size_t begin, const char* bodyOpenSymbol, const char* bodyCloseSymbol, const std::vector<std::pair<size_t, size_t>>& commentZones, EditView* view);
};


class FunctionUnitParser : public FunctionParser
{
public:
	FunctionUnitParser(const char* id, const char* displayName, const std::string& commentExpr,
		const std::string& mainExpr, const std::vector<std::string>& functionNameExprArray,
		const std::vector<std::string>& classNameExprArray) noexcept
		: FunctionParser(id, displayName, commentExpr, mainExpr, functionNameExprArray, classNameExprArray)
	{}

	void parse(std::vector<foundInfo>& foundInfos, size_t begin, size_t end, EditView* view, const std::string& classStructName = "") override;
};

class FunctionMixParser : public FunctionZoneParser
{
public:
	FunctionMixParser(const char* id, const char* displayName, const std::string& commentExpr, const std::string& rangeExpr, const std::string& openSymbole, const std::string& closeSymbole,
		const std::vector<std::string>& classNameExprArray, const std::string& functionExpr, const std::vector<std::string>& functionNameExprArray, std::unique_ptr<FunctionUnitParser> funcUnitPaser) noexcept
		: FunctionZoneParser(id, displayName, commentExpr, rangeExpr, openSymbole, closeSymbole, classNameExprArray, functionExpr, functionNameExprArray), _funcUnitPaser(std::move(funcUnitPaser))
	{}

	void parse(std::vector<foundInfo>& foundInfos, size_t begin, size_t end, EditView* view, const std::string& classStructName = "") override;

private:
	std::unique_ptr<FunctionUnitParser> _funcUnitPaser = nullptr;
};


struct AssociationInfo final
{
	int _id;
	int _langID;
	QString _ext;
	QString _userDefinedLangName;

	AssociationInfo(int id, int langID, const QString& ext, const QString& userDefinedLangName) noexcept
		: _id(id), _langID(langID), _ext(ext), _userDefinedLangName(userDefinedLangName)
	{
	}
};

inline constexpr int nbMaxUserDefined = 25;

struct ParserInfo
{
	QString _id; // xml parser rule file name - if empty, then we use default name. Mandatory if _userDefinedLangName is not empty
	std::unique_ptr<FunctionParser> _parser = nullptr;
	QString _userDefinedLangName;

	ParserInfo() {}
	explicit ParserInfo(const QString& id) noexcept : _id(id) {}
	explicit ParserInfo(const QString& id, const QString& userDefinedLangName) noexcept : _id(id), _userDefinedLangName(userDefinedLangName) {}
};

class FunctionParsersManager final
{
public:
	bool init(const QString& xmlDirPath, const QString& xmlInstalledPath, EditView* view);
	bool parse(std::vector<foundInfo>& foundInfos, const AssociationInfo& assoInfo);
	bool hasParser(const AssociationInfo& assoInfo) { return getParser(assoInfo) != nullptr; }


private:
	EditView* _view = nullptr;
	QString _xmlDirPath; // The 1st place to load function list files. Usually it's "%APPDATA%\Notepad++\functionList\"
	QString _xmlDirInstalledPath; // Where Notepad++ is installed. The 2nd place to load function list files. Usually it's "%PROGRAMFILES%\Notepad++\functionList\"

	std::unique_ptr<ParserInfo> _parsers[L_EXTERNAL + nbMaxUserDefined] = { nullptr };
	int _currentUDIndex = L_EXTERNAL;

	bool getOverrideMapFromXmlTree(const QString& xmlDirPath);
	bool loadFuncListFromXmlTree(const QString& xmlDirPath, LangType lType, const QString& overrideId, int udlIndex = -1);
	static bool getZonePaserParameters(const pugi::xml_node& classRangeParser, std::string& mainExprStr, std::string& openSymboleStr, std::string& closeSymboleStr, std::vector<std::string>& classNameExprArray, std::string& functionExprStr, std::vector<std::string>& functionNameExprArray);
	static bool getUnitPaserParameters(const pugi::xml_node& functionParser, std::string& mainExprStr, std::vector<std::string>& functionNameExprArray, std::vector<std::string>& classNameExprArray);
	FunctionParser* getParser(const AssociationInfo& assoInfo);
};
