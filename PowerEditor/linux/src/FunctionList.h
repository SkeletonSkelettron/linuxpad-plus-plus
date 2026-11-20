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

// View > Function List: Qt counterpart of WinControls/FunctionList/functionListPanel.cpp.
// The rules are the functionList/*.xml files of Notepad++ (FunctionParser.cpp).

#pragma once

#include <vector>

#include <QHash>
#include <QSet>
#include <QWidget>

#include "FunctionParser.h"
#include "NppCommon.h"

class EditView;
class MainWindow;
struct Buffer;
class QLineEdit;
class QToolButton;
class QTreeWidget;
class QTreeWidgetItem;

class FunctionListPanel : public QWidget
{
	Q_OBJECT

public:
	FunctionListPanel(MainWindow* mw, QWidget* parent);

	void reload();    // parse the document of the active view
	bool needsReload() const; // another document or language than the parsed one
	void markEntry(); // select the function the caret is in
	void applyIcons(); // light or dark icons
	QTreeWidget* tree() const { return _tree; }
	QLineEdit* searchEdit() const { return _search; }
	void setSorted(bool sorted);

protected:
	bool eventFilter(QObject* obj, QEvent* event) override;

private:
	struct State
	{
		QString search;
		bool sort = false;
		QSet<QString> collapsed; // class nodes folded by the user
	};

	void rebuildTree();
	void openItem(QTreeWidgetItem* item);
	void saveState();

	MainWindow* _mw;
	EditView* _parseView = nullptr; // hidden view the parsers search in
	FunctionParsersManager _parsers;
	bool _parsersReady = false;
	bool _hasParser = false;
	QLineEdit* _search = nullptr;
	QToolButton* _sort = nullptr;
	QToolButton* _reload = nullptr;
	QToolButton* _prefs = nullptr;
	QTreeWidget* _tree = nullptr;
	std::vector<foundInfo> _found;
	const Buffer* _parsedBuf = nullptr;
	LangType _parsedLang = L_TEXT;
	QString _key;      // full name of the parsed document
	QString _fileName; // root node label
	QHash<QString, State> _states;
	sptr_t _markLine = -1;
	sptr_t _markEndLine = -1;
	bool _rebuilding = false;
};
