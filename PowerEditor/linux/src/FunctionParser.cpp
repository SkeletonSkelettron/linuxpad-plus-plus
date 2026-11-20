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

#include "FunctionParser.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <QFile>

#include "EditView.h"
#include "LangStyles.h"

namespace
{
	const char* attr(const pugi::xml_node& node, const char* name)
	{
		const pugi::xml_attribute a = node.attribute(name);
		return a ? a.value() : nullptr;
	}

	// keep the new lines of the attributes: the (?x) expressions have comments ending at the end of line
	// (TinyXML of the Windows version does not normalise attribute values)
	constexpr unsigned int xmlParseOptions = pugi::parse_default & ~pugi::parse_wconv_attribute;

	// ScintillaEditView::searchInTarget()
	intptr_t searchInTarget(EditView* view, const std::string& expr, size_t begin, size_t end)
	{
		view->ex(SCI_SETTARGETRANGE, begin, end);
		return view->ex(SCI_SEARCHINTARGET, expr.size(), reinterpret_cast<sptr_t>(expr.c_str()));
	}

	intptr_t searchInTarget(EditView* view, const char* expr, size_t begin, size_t end)
	{
		return searchInTarget(view, std::string(expr), begin, end);
	}
}

bool FunctionParsersManager::init(const QString& xmlDirPath, const QString& xmlInstalledPath, EditView* view)
{
	_view = view;
	_xmlDirPath = xmlDirPath;
	_xmlDirInstalledPath = xmlInstalledPath;

	bool isOK = getOverrideMapFromXmlTree(_xmlDirPath);
	if (isOK)
		return true;
	else if (_xmlDirPath != _xmlDirInstalledPath && !_xmlDirInstalledPath.isEmpty())
		return getOverrideMapFromXmlTree(_xmlDirInstalledPath);
	else
		return false;
}

bool FunctionParsersManager::getZonePaserParameters(
	const pugi::xml_node& classRangeParser,
	std::string& mainExprStr,
	std::string& openSymboleStr,
	std::string& closeSymboleStr,
	std::vector<std::string>& classNameExprArray,
	std::string& functionExprStr,
	std::vector<std::string>& functionNameExprArray
)
{
	const char* mainExpr = attr(classRangeParser, "mainExpr");
	if (!mainExpr || !mainExpr[0])
		return false;
	mainExprStr = mainExpr;

	const char* openSymbole = attr(classRangeParser, "openSymbole");
	if (openSymbole && openSymbole[0])
		openSymboleStr = openSymbole;

	const char* closeSymbole = attr(classRangeParser, "closeSymbole");
	if (closeSymbole && closeSymbole[0])
		closeSymboleStr = closeSymbole;

	pugi::xml_node classNameParser = classRangeParser.child("className");
	if (classNameParser)
	{
		for (pugi::xml_node childNode2 = classNameParser.child("nameExpr");
			childNode2;
			childNode2 = childNode2.next_sibling("nameExpr"))
		{
			const char* expr = attr(childNode2, "expr");
			if (expr && expr[0])
				classNameExprArray.push_back(expr);
		}
	}

	pugi::xml_node functionParser = classRangeParser.child("function");
	if (!functionParser)
		return false;

	const char* functionExpr = attr(functionParser, "mainExpr");
	if (!functionExpr || !functionExpr[0])
		return false;
	functionExprStr = functionExpr;

	pugi::xml_node functionNameParser = functionParser.child("functionName");
	if (functionNameParser)
	{
		for (pugi::xml_node childNode3 = functionNameParser.child("funcNameExpr");
			childNode3;
			childNode3 = childNode3.next_sibling("funcNameExpr"))
		{
			const char* expr = attr(childNode3, "expr");
			if (expr && expr[0])
				functionNameExprArray.push_back(expr);
		}

	}
	return true;
}

bool FunctionParsersManager::getUnitPaserParameters(
	const pugi::xml_node& functionParser,
	std::string& mainExprStr,
	std::vector<std::string>& functionNameExprArray,
	std::vector<std::string>& classNameExprArray
)
{
	const char* mainExpr = attr(functionParser, "mainExpr");
	if (!mainExpr || !mainExpr[0])
		return false;
	mainExprStr = mainExpr;

	pugi::xml_node functionNameParser = functionParser.child("functionName");
	if (functionNameParser)
	{
		for (pugi::xml_node childNode = functionNameParser.child("nameExpr");
			childNode;
			childNode = childNode.next_sibling("nameExpr"))
		{
			const char* expr = attr(childNode, "expr");
			if (expr && expr[0])
				functionNameExprArray.push_back(expr);
		}
	}

	pugi::xml_node classNameParser = functionParser.child("className");
	if (classNameParser)
	{
		for (pugi::xml_node childNode = classNameParser.child("nameExpr");
			childNode;
			childNode = childNode.next_sibling("nameExpr"))
		{
			const char* expr = attr(childNode, "expr");
			if (expr && expr[0])
				classNameExprArray.push_back(expr);
		}
	}
	return true;
}

bool FunctionParsersManager::loadFuncListFromXmlTree(const QString& xmlDirPath, LangType lType, const QString& overrideId, int udlIndex)
{
	QString funcListRulePath = xmlDirPath + QLatin1Char('/');
	int index = -1;
	if (lType == L_USER) // UDL
	{
		if (overrideId.isEmpty())
			return false;

		if (udlIndex == -1)
			return false;

		index = udlIndex;
		funcListRulePath += overrideId;
	}
	else // Supported Language
	{
		index = lType;
		if (overrideId.isEmpty())
		{
			funcListRulePath += QString::fromUtf8(StyleManager::langInfo(lType).langName) + QStringLiteral(".xml");
		}
		else
		{
			funcListRulePath += overrideId;
		}
	}

	if (index > _currentUDIndex)
		return false;

	if (_parsers[index] == nullptr)
		return false;

	pugi::xml_document xmlFuncListDoc;
	if (!xmlFuncListDoc.load_file(QFile::encodeName(funcListRulePath).constData(), xmlParseOptions))
		return false;

	pugi::xml_node root = xmlFuncListDoc.child("NotepadPlus");
	if (!root)
		return false;

	root = root.child("functionList");
	if (!root)
		return false;

	pugi::xml_node parserRoot = root.child("parser");
	if (!parserRoot)
		return false;

	const char* id = attr(parserRoot, "id");
	if (!id || !id[0])
		return false;

	std::string commentExpr;
	const char* pCommentExpr = attr(parserRoot, "commentExpr");
	if (pCommentExpr && pCommentExpr[0])
		commentExpr = pCommentExpr;

	std::vector<std::string> classNameExprArray;
	std::vector<std::string> functionNameExprArray;

	const char* displayName = attr(parserRoot, "displayName");
	if (!displayName || !displayName[0])
		displayName = id;

	pugi::xml_node classRangeParser = parserRoot.child("classRange");
	pugi::xml_node functionParser = parserRoot.child("function");
	if (classRangeParser && functionParser)
	{
		std::string mainExpr, openSymbole, closeSymbole, functionExpr;
		getZonePaserParameters(classRangeParser, mainExpr, openSymbole, closeSymbole, classNameExprArray, functionExpr, functionNameExprArray);

		std::string mainExpr2;
		std::vector<std::string> classNameExprArray2;
		std::vector<std::string> functionNameExprArray2;
		getUnitPaserParameters(functionParser, mainExpr2, functionNameExprArray2, classNameExprArray2);
		auto funcUnitPaser = std::make_unique<FunctionUnitParser>(id, displayName, commentExpr, mainExpr2, functionNameExprArray2, classNameExprArray2);

		_parsers[index]->_parser = std::make_unique<FunctionMixParser>(id, displayName, commentExpr, mainExpr, openSymbole, closeSymbole, classNameExprArray, functionExpr, functionNameExprArray, std::move(funcUnitPaser));
	}
	else if (classRangeParser)
	{
		std::string mainExpr, openSymbole, closeSymbole, functionExpr;
		getZonePaserParameters(classRangeParser, mainExpr, openSymbole, closeSymbole, classNameExprArray, functionExpr, functionNameExprArray);
		_parsers[index]->_parser = std::make_unique<FunctionZoneParser>(id, displayName, commentExpr, mainExpr, openSymbole, closeSymbole, classNameExprArray, functionExpr, functionNameExprArray);
	}
	else if (functionParser)
	{
		std::string mainExpr;
		getUnitPaserParameters(functionParser, mainExpr, functionNameExprArray, classNameExprArray);
		_parsers[index]->_parser = std::make_unique<FunctionUnitParser>(id, displayName, commentExpr, mainExpr, functionNameExprArray, classNameExprArray);
	}

	return true;
}

bool FunctionParsersManager::getOverrideMapFromXmlTree(const QString& xmlDirPath)
{
	const QString funcListRulePath = xmlDirPath + QStringLiteral("/overrideMap.xml");

	pugi::xml_document xmlFuncListDoc;
	if (!xmlFuncListDoc.load_file(QFile::encodeName(funcListRulePath).constData(), xmlParseOptions))
		return false;

	pugi::xml_node root = xmlFuncListDoc.child("NotepadPlus");
	if (!root)
		return false;

	root = root.child("functionList");
	if (!root)
		return false;

	pugi::xml_node associationMapRoot = root.child("associationMap");
	if (associationMapRoot)
	{
		for (pugi::xml_node childNode = associationMapRoot.child("association");
			childNode;
			childNode = childNode.next_sibling("association"))
		{
			const char* id = attr(childNode, "id");
			if (!id || !id[0])
				continue;

			const int langID = childNode.attribute("langID").as_int(-1);
			const char* userDefinedLangName = attr(childNode, "userDefinedLangName");
			int parserSize = static_cast<int>(std::size(_parsers));

			if (langID >= 0 && langID < parserSize)
			{
				_parsers[langID] = std::make_unique<ParserInfo>(QString::fromUtf8(id));
			}
			else if (userDefinedLangName && userDefinedLangName[0])
			{
				++_currentUDIndex;

				if (_currentUDIndex < parserSize)
				{
					_parsers[_currentUDIndex] = std::make_unique<ParserInfo>(QString::fromUtf8(id), QString::fromUtf8(userDefinedLangName));
				}
			}
		}
	}

	return true;
}

FunctionParser* FunctionParsersManager::getParser(const AssociationInfo& assoInfo)
{
	enum ParserType : unsigned char
	{
		doNothing,
		checkLangID,
		checkUserDefined
	};

	unsigned char choice = doNothing;
	// langID != -1 && langID != L_USER
	if (assoInfo._langID != -1 && assoInfo._langID != L_USER)
		choice = checkLangID;
	// langID == L_USER, we chack the userDefinedLangName
	else if (assoInfo._langID == L_USER && !assoInfo._userDefinedLangName.isEmpty())
		choice = checkUserDefined;
	else
		return nullptr;

	switch (choice)
	{
		case checkLangID:
		{
			if (_parsers[assoInfo._langID] != nullptr)
			{
				if (_parsers[assoInfo._langID]->_parser != nullptr)
					return _parsers[assoInfo._langID]->_parser.get();

				// load it
				if (loadFuncListFromXmlTree(_xmlDirPath, static_cast<LangType>(assoInfo._langID), _parsers[assoInfo._langID]->_id))
					return _parsers[assoInfo._langID]->_parser.get();
				else if (_xmlDirPath != _xmlDirInstalledPath && !_xmlDirInstalledPath.isEmpty() && loadFuncListFromXmlTree(_xmlDirInstalledPath, static_cast<LangType>(assoInfo._langID), _parsers[assoInfo._langID]->_id))
					return _parsers[assoInfo._langID]->_parser.get();
			}
			else
			{
				_parsers[assoInfo._langID] = std::make_unique<ParserInfo>();
				// load it
				if (loadFuncListFromXmlTree(_xmlDirPath, static_cast<LangType>(assoInfo._langID), _parsers[assoInfo._langID]->_id))
					return _parsers[assoInfo._langID]->_parser.get();
				else if (_xmlDirPath != _xmlDirInstalledPath && !_xmlDirInstalledPath.isEmpty() && loadFuncListFromXmlTree(_xmlDirInstalledPath, static_cast<LangType>(assoInfo._langID), _parsers[assoInfo._langID]->_id))
					return _parsers[assoInfo._langID]->_parser.get();

				return nullptr;
			}
		}
		break;

		case checkUserDefined:
		{
			if (_currentUDIndex == L_EXTERNAL) // no User Defined Language parser
				return nullptr;

			for (int i = L_EXTERNAL + 1; i <= _currentUDIndex; ++i)
			{
				if (_parsers[i]->_userDefinedLangName == assoInfo._userDefinedLangName)
				{
					if (_parsers[i]->_parser)
					{
						return _parsers[i]->_parser.get();
					}

					// load it
					if (loadFuncListFromXmlTree(_xmlDirPath, static_cast<LangType>(assoInfo._langID), _parsers[i]->_id, i))
						return _parsers[i]->_parser.get();
					if (_xmlDirPath != _xmlDirInstalledPath && !_xmlDirInstalledPath.isEmpty() && loadFuncListFromXmlTree(_xmlDirInstalledPath, static_cast<LangType>(assoInfo._langID), _parsers[i]->_id, i))
						return _parsers[i]->_parser.get();
					break;
				}
			}
			return nullptr;
		}
	}
	return nullptr;
}

void FunctionParser::funcParse(
	std::vector<foundInfo>& foundInfos,
	size_t begin,
	size_t end,
	EditView* view,
	const std::string& classStructName,
	const std::vector<std::pair<size_t, size_t>>* commentZones
)
{
	if (begin >= end)
		return;

	if (_functionExpr.length() == 0)
		return;

	static constexpr int flags = SCFIND_REGEXP | SCFIND_POSIX | SCFIND_REGEXP_DOTMATCHESNL;

	view->ex(SCI_SETSEARCHFLAGS, flags);
	intptr_t targetStart = searchInTarget(view, _functionExpr, begin, end);

	//foundInfos.clear();
	while (targetStart >= 0)
	{
		targetStart = view->ex(SCI_GETTARGETSTART);
		intptr_t targetEnd = view->ex(SCI_GETTARGETEND);
		if (targetEnd > static_cast<intptr_t>(end)) //we found a result but outside our range, therefore do not process it
		{
			break;
		}
		size_t foundTextLen = targetEnd - targetStart;
		if (targetStart + foundTextLen == end)
			break;

		foundInfo fi;

		// dataToSearch & data2ToSearch are optional
		if (!_functionNameExprArray.size() && !_classNameExprArray.size())
		{
			fi._data = view->textRange(targetStart, targetEnd).toStdString(); // whole found data
			fi._pos = targetStart;

		}
		else
		{
			intptr_t foundPos = -1;
			if (_functionNameExprArray.size())
			{
				fi._data = parseSubLevel(targetStart, targetEnd, _functionNameExprArray, foundPos, view);
				fi._pos = foundPos;
			}

			if (!classStructName.empty())
			{
				fi._data2 = classStructName;
				fi._pos2 = -1; // change -1 valeur for validated data2
			}
			else if (_classNameExprArray.size())
			{
				fi._data2 = parseSubLevel(targetStart, targetEnd, _classNameExprArray, foundPos, view);
				fi._pos2 = foundPos;
			}
		}

		if (fi._pos != -1 || fi._pos2 != -1) // at least one should be found
		{
			if (commentZones != nullptr)
			{
				if (!isInZones(fi._pos, *commentZones) && !isInZones(fi._pos2, *commentZones))
					foundInfos.push_back(fi);
			}
			else
				foundInfos.push_back(fi);
		}

		begin = targetStart + foundTextLen;
		targetStart = searchInTarget(view, _functionExpr, begin, end);
	}
}


std::string FunctionParser::parseSubLevel(size_t begin, size_t end, std::vector<std::string> dataToSearch, intptr_t& foundPos, EditView* view)
{
	if (begin >= end)
	{
		foundPos = -1;
		return "";
	}

	if (!dataToSearch.size())
		return "";

	static constexpr int flags = SCFIND_REGEXP | SCFIND_POSIX  | SCFIND_REGEXP_DOTMATCHESNL;

	view->ex(SCI_SETSEARCHFLAGS, flags);
	intptr_t targetStart = searchInTarget(view, dataToSearch[0], begin, end);

	if (targetStart < 0)
	{
		foundPos = -1;
		return "";
	}
	intptr_t targetEnd = view->ex(SCI_GETTARGETEND);

	if (dataToSearch.size() >= 2)
	{
		dataToSearch.erase(dataToSearch.begin());
		return parseSubLevel(targetStart, targetEnd, dataToSearch, foundPos, view);
	}

	// only one processed element, so we conclude the result
	foundPos = targetStart;
	return view->textRange(targetStart, targetEnd).toStdString();
}


bool FunctionParsersManager::parse(std::vector<foundInfo>& foundInfos, const AssociationInfo& assoInfo)
{
	// Serch the right parser from the given ext in the map
	FunctionParser *fp = getParser(assoInfo);
	if (!fp)
		return false;

	// parse
	size_t docLen = static_cast<size_t>(_view->ex(SCI_GETLENGTH));
	fp->parse(foundInfos, 0, docLen, _view);

	return true;
}


size_t FunctionZoneParser::getBodyClosePos(
	size_t begin,
	const char* bodyOpenSymbol,
	const char* bodyCloseSymbol,
	const std::vector<std::pair<size_t, size_t>>& commentZones,
	EditView* view
)
{
	size_t cntOpen = 1;

	size_t docLen = static_cast<size_t>(view->ex(SCI_GETLENGTH));

	if (begin >= docLen)
		return docLen;

	std::string exprToSearch = "(";
	exprToSearch += bodyOpenSymbol;
	exprToSearch += "|";
	exprToSearch += bodyCloseSymbol;
	exprToSearch += ")";


	static constexpr int flags = SCFIND_REGEXP | SCFIND_POSIX | SCFIND_REGEXP_DOTMATCHESNL;

	view->ex(SCI_SETSEARCHFLAGS, flags);
	intptr_t targetStart = searchInTarget(view, exprToSearch, begin, docLen);
	intptr_t targetEnd = 0;

	do
	{
		if (targetStart >= 0) // found open or close symbol
		{
			targetEnd = view->ex(SCI_GETTARGETEND);

			// Treat it only if it's NOT in the comment zone
			if (!isInZones(targetStart, commentZones))
			{
				// Now we determine the symbol (open or close)
				const intptr_t tmpStart = searchInTarget(view, bodyOpenSymbol, targetStart, targetEnd);
				if (tmpStart >= 0) // open symbol found
				{
					++cntOpen;
				}
				else // if it's not open symbol, then it must be the close one
				{
					--cntOpen;
				}
			}
		}
		else // nothing found
		{
			cntOpen = 0; // get me out of here
			targetEnd = begin;
		}

		targetStart = searchInTarget(view, exprToSearch, targetEnd, docLen);

	} while (cntOpen);

	return targetEnd;
}

void FunctionZoneParser::classParse(
	std::vector<foundInfo>& foundInfos,
	std::vector<std::pair<size_t, size_t>>& scannedZones,
	const std::vector<std::pair<size_t, size_t>>& commentZones,
	size_t begin, size_t end, EditView* view,
	const std::string& /*classStructName*/
)
{
	if (begin >= end)
		return;

	static constexpr int flags = SCFIND_REGEXP | SCFIND_POSIX | SCFIND_REGEXP_DOTMATCHESNL;

	view->ex(SCI_SETSEARCHFLAGS, flags);
	intptr_t targetStart = searchInTarget(view, _rangeExpr, begin, end);

	while (targetStart >= 0)
	{
		intptr_t targetEnd = view->ex(SCI_GETTARGETEND);

		// Get class name
		intptr_t foundPos = 0;
		std::string subLevelClassStructName = parseSubLevel(targetStart, targetEnd, _classNameExprArray, foundPos, view);


		if (!_openSymbole.empty() && !_closeSymbole.empty())
		{
			targetEnd = getBodyClosePos(targetEnd, _openSymbole.c_str(), _closeSymbole.c_str(), commentZones, view);
		}

		if (targetEnd > static_cast<intptr_t>(end)) //we found a result but outside our range, therefore do not process it
			break;

		scannedZones.push_back(std::pair<size_t, size_t>(targetStart, targetEnd));

		size_t foundTextLen = targetEnd - targetStart;
		if (targetStart + foundTextLen == end)
			break;

		// Begin to search all method inside
		// std::vector<QString> emptyArray;
		if (!isInZones(targetStart, commentZones))
		{
			funcParse(foundInfos, targetStart, targetEnd, view, subLevelClassStructName, &commentZones);
		}
		begin = targetStart + (targetEnd - targetStart);
		targetStart = searchInTarget(view, _rangeExpr, begin, end);
	}
}


void FunctionParser::getCommentZones(std::vector<std::pair<size_t, size_t>>& commentZone, size_t begin, size_t end, EditView* view)
{
	if ((begin >= end) || (_commentExpr.empty()))
		return;

	static constexpr int flags = SCFIND_REGEXP | SCFIND_POSIX | SCFIND_REGEXP_DOTMATCHESNL;

	view->ex(SCI_SETSEARCHFLAGS, flags);
	intptr_t targetStart = searchInTarget(view, _commentExpr, begin, end);

	while (targetStart >= 0)
	{
		targetStart = view->ex(SCI_GETTARGETSTART);
		intptr_t targetEnd = view->ex(SCI_GETTARGETEND);
		if (targetEnd > static_cast<intptr_t>(end)) //we found a result but outside our range, therefore do not process it
			break;

		commentZone.push_back(std::pair<size_t, size_t>(targetStart, targetEnd));

		intptr_t foundTextLen = targetEnd - targetStart;
		if (targetStart + foundTextLen == static_cast<intptr_t>(end))
			break;

		begin = targetStart + foundTextLen;
		targetStart = searchInTarget(view, _commentExpr, begin, end);
	}
}


bool FunctionParser::isInZones(size_t pos2Test, const std::vector<std::pair<size_t, size_t>>& zones)
{
	for (size_t i = 0, len = zones.size(); i < len; ++i)
	{
		if (pos2Test >= zones[i].first && pos2Test < zones[i].second)
			return true;
	}
	return false;
}


void FunctionParser::getInvertZones(std::vector<std::pair<size_t, size_t>>& destZones, const std::vector<std::pair<size_t, size_t>>& sourceZones, size_t begin, size_t end)
{
	if (sourceZones.size() == 0)
	{
		destZones.push_back(std::pair<size_t, size_t>(begin, end));
	}
	else
	{
		// check the begin
		if (begin < sourceZones[0].first)
		{
			destZones.push_back(std::pair<size_t, size_t>(begin, sourceZones[0].first - 1));
		}

		size_t i = 0;
		for (size_t len = sourceZones.size() - 1; i < len; ++i)
		{
			size_t newBegin = sourceZones[i].second + 1;
			size_t newEnd = sourceZones[i + 1].first - 1;
			if (newBegin < newEnd)
				destZones.push_back(std::pair<size_t, size_t>(newBegin, newEnd));
		}
		size_t lastBegin = sourceZones[i].second + 1;
		if (lastBegin < end)
			destZones.push_back(std::pair<size_t, size_t>(lastBegin, end));
	}
}


void FunctionZoneParser::parse(std::vector<foundInfo>& foundInfos, size_t begin, size_t end, EditView* view, const std::string& classStructName)
{
	std::vector<std::pair<size_t, size_t>> classZones, commentZones, nonCommentZones;
	getCommentZones(commentZones, begin, end, view);
	getInvertZones(nonCommentZones, commentZones, begin, end);
	for (size_t i = 0, len = nonCommentZones.size(); i < len; ++i)
	{
		classParse(foundInfos, classZones, commentZones, nonCommentZones[i].first, nonCommentZones[i].second, view, classStructName);
	}
}

void FunctionUnitParser::parse(std::vector<foundInfo>& foundInfos, size_t begin, size_t end, EditView* view, const std::string& classStructName)
{
	std::vector<std::pair<size_t, size_t>> commentZones, nonCommentZones;
	getCommentZones(commentZones, begin, end, view);
	getInvertZones(nonCommentZones, commentZones, begin, end);
	for (size_t i = 0, len = nonCommentZones.size(); i < len; ++i)
	{
		funcParse(foundInfos, nonCommentZones[i].first, nonCommentZones[i].second, view, classStructName);
	}
}

void FunctionMixParser::parse(std::vector<foundInfo>& foundInfos, size_t begin, size_t end, EditView* view, const std::string& classStructName)
{
	std::vector<std::pair<size_t, size_t>> commentZones, scannedZones, nonScannedZones;
	getCommentZones(commentZones, begin, end, view);

	classParse(foundInfos, scannedZones, commentZones, begin, end, view, classStructName);

	// the second level
	for (size_t i = 0, len = scannedZones.size(); i < len; ++i)
	{
		std::vector<std::pair<size_t, size_t>> temp;
		classParse(foundInfos, temp, commentZones, scannedZones[i].first, scannedZones[i].second, view, classStructName);
	}
	// invert scannedZones
	getInvertZones(nonScannedZones, scannedZones, begin, end);

	// for each nonScannedZones, search functions
	if (_funcUnitPaser)
	{
		for (size_t i = 0, len = nonScannedZones.size(); i < len; ++i)
		{
			_funcUnitPaser->funcParse(foundInfos, nonScannedZones[i].first, nonScannedZones[i].second, view, classStructName, &commentZones);
		}
	}
}
