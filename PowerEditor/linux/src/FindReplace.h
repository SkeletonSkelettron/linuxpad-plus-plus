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

// Find / Replace / Find in Files / Mark dialog and the "Search results" panel.
// Qt counterpart of ScintillaComponent/FindReplaceDlg.cpp.

#pragma once

#include <vector>

#include <QDialog>
#include <QWidget>

#include "NppCommon.h"

class EditView;
class MainWindow;
struct Buffer;
class QCheckBox;
class QComboBox;
class QGroupBox;
class QLabel;
class QPushButton;
class QRadioButton;
class QTabBar;

enum SearchType
{
	FindNormal = 0,
	FindExtended = 1,
	FindRegex = 2
};

struct FindOptions
{
	QString findText;
	QString replaceText;
	QString filters;
	QString directory;
	bool wholeWord = false;
	bool matchCase = false;
	bool wrapAround = true;
	bool backward = false;
	bool inSelection = false;
	int searchType = FindNormal;
	bool dotMatchesNewline = false;
	bool recursive = true;
	bool hiddenFolders = false;
	bool bookmarkLine = false;
	bool purge = false;
};

namespace Searching
{
	QByteArray prepareText(const QString& text, int searchType);
	int buildSearchFlags(const FindOptions& opt);
	QString convertExtendedToString(const QString& query);
}

struct SearchHit
{
	sptr_t line = 0;
	sptr_t start = 0;
	sptr_t end = 0;
};

class SearchResultsPanel : public QWidget
{
	Q_OBJECT

public:
	SearchResultsPanel(MainWindow* mw, QWidget* parent);

	struct FoundLine
	{
		sptr_t line = 0;    // line number in the document
		QByteArray text;    // line text (UTF-8, truncated)
		std::vector<std::pair<sptr_t, sptr_t>> segments; // match offsets inside text
		sptr_t start = 0;   // document position of the first match on the line
		sptr_t end = 0;
	};
	struct FileResult
	{
		QString fileName; // path, or the name of an untitled document
		std::vector<FoundLine> lines;
		int hitCount = 0;
	};

	static FileResult collect(EditView* view, const std::vector<SearchHit>& hits, const QString& fileName);
	void addSearch(const QString& searchText, const std::vector<FileResult>& files, int filesSearched);
	void clearAll();
	void gotoNextResult(bool forward);
	void applyStyle();
	EditView* view() const { return _view; }

private:
	struct LineInfo
	{
		int kind = 0; // 0 search header, 1 file header, 2 hit
		QString fileName;
		sptr_t line = 0;
		sptr_t start = 0;
		sptr_t end = 0;
	};
	struct Block
	{
		QByteArray text;
		std::vector<LineInfo> lines;
		std::vector<SearchResultMarkingLine> markings;
	};

	void rebuild();
	void openLine(sptr_t line);
	void showContextMenu(const QPoint& globalPos);

	MainWindow* _mw;
	EditView* _view;
	std::vector<Block> _blocks;
	std::vector<LineInfo> _lines;
	std::vector<SearchResultMarkingLine> _markings;
	SearchResultMarkings _markingsStruct{0, nullptr};
	sptr_t _currentLine = -1;
};

class FindReplaceDlg : public QDialog
{
	Q_OBJECT

public:
	enum Tab
	{
		FindTab = 0,
		ReplaceTab = 1,
		FindInFilesTab = 2,
		FindInProjectsTab = 3,
		MarkTab = 4
	};

	explicit FindReplaceDlg(MainWindow* mw);

	void showTab(int tab);
	void setFindText(const QString& text);
	QString findText() const;
	FindOptions options() const;
	bool findNext(bool forward);
	bool find(const FindOptions& opt, bool forward, bool showMessages);
	void saveHistory();
	void setDirectory(const QString& dir);
	void setFilters(const QString& filters); // NPPM_LAUNCHFINDINFILESDLG
	void setProjectPanels(int mask); // Find in Projects: Project Panel 1-3 check boxes

private:
	void buildUi();
	void updateForTab();
	void setStatus(const QString& text, int kind); // 0 info, 1 not found, 2 end reached
	void addToHistory(QComboBox* combo, const QString& text);
	void loadHistory();

	void onFindNext();
	void onCount();
	void onFindAllCurrent();
	void onFindAllOpened();
	void onReplace();
	void onReplaceAll();
	void onReplaceAllOpened();
	void onFindInFiles(bool replace);
	void onMarkAll();
	void onClearMarks();
	void onCopyMarked();

	int replaceAllInView(EditView* view, const FindOptions& opt, bool inSelection);
	int countInView(EditView* view, const FindOptions& opt, bool inSelection);
	int markAllInView(EditView* view, const FindOptions& opt, bool inSelection);
	std::vector<SearchHit> findAllInView(EditView* view, const FindOptions& opt);
	QStringList collectFiles(const FindOptions& opt, bool* cancelled);

	MainWindow* _mw;
	EditView* _searchView = nullptr;
	QTabBar* _tabBar = nullptr;
	QComboBox* _findCombo = nullptr;
	QComboBox* _replaceCombo = nullptr;
	QComboBox* _filtersCombo = nullptr;
	QComboBox* _dirCombo = nullptr;
	QLabel* _replaceLabel = nullptr;
	QLabel* _filtersLabel = nullptr;
	QLabel* _dirLabel = nullptr;
	QPushButton* _dirBrowse = nullptr;
	QWidget* _projectRow = nullptr;
	QCheckBox* _projectChecks[3] = {};
	QCheckBox* _backward = nullptr;
	QCheckBox* _wholeWord = nullptr;
	QCheckBox* _matchCase = nullptr;
	QCheckBox* _wrapAround = nullptr;
	QCheckBox* _inSelection = nullptr;
	QCheckBox* _bookmarkLine = nullptr;
	QCheckBox* _purge = nullptr;
	QCheckBox* _recursive = nullptr;
	QCheckBox* _hidden = nullptr;
	QRadioButton* _modeNormal = nullptr;
	QRadioButton* _modeExtended = nullptr;
	QRadioButton* _modeRegex = nullptr;
	QCheckBox* _dotNewline = nullptr;
	QPushButton* _btnFindNext = nullptr;
	QPushButton* _btnCount = nullptr;
	QPushButton* _btnFindAllOpened = nullptr;
	QPushButton* _btnFindAllCurrent = nullptr;
	QPushButton* _btnReplace = nullptr;
	QPushButton* _btnReplaceAll = nullptr;
	QPushButton* _btnReplaceAllOpened = nullptr;
	QPushButton* _btnFindInFiles = nullptr;
	QPushButton* _btnReplaceInFiles = nullptr;
	QPushButton* _btnMarkAll = nullptr;
	QPushButton* _btnClearMarks = nullptr;
	QPushButton* _btnCopyMarked = nullptr;
	QLabel* _status = nullptr;

	friend struct MainWindowAccess;
};
