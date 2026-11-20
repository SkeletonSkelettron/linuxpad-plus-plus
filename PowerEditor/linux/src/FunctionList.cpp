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

#include "FunctionList.h"

#include <algorithm>
#include <functional>

#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLineEdit>
#include <QMenu>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "EditView.h"
#include "FileIO.h"
#include "MainWindow.h"

namespace
{
	constexpr int PosRole = Qt::UserRole;      // position of the function in the document, -1 for nodes
	constexpr int KindRole = Qt::UserRole + 1; // 0 root, 1 class node, 2 function

	QString iconDir()
	{
		return Npp::prefs().isDark() ? QStringLiteral(":/icons/dark/panels") : QStringLiteral(":/icons/standard/panels");
	}
}

FunctionListPanel::FunctionListPanel(MainWindow* mw, QWidget* parent)
	: QWidget(parent), _mw(mw)
{
	auto* root = new QVBoxLayout(this);
	root->setContentsMargins(2, 2, 2, 2);
	root->setSpacing(2);

	auto* bar = new QHBoxLayout;
	_search = new QLineEdit(this);
	_search->setPlaceholderText(QStringLiteral("Search"));
	_search->setClearButtonEnabled(true);
	_sort = new QToolButton(this);
	_sort->setCheckable(true);
	_sort->setAutoRaise(true);
	_sort->setToolTip(QStringLiteral("Sort"));
	_reload = new QToolButton(this);
	_reload->setAutoRaise(true);
	_reload->setToolTip(QStringLiteral("Reload"));
	_prefs = new QToolButton(this);
	_prefs->setAutoRaise(true);
	_prefs->setToolTip(QStringLiteral("Preferences"));
	_prefs->setPopupMode(QToolButton::InstantPopup);
	auto* prefsMenu = new QMenu(_prefs);
	QAction* sortDefault = prefsMenu->addAction(QStringLiteral("Sort functions (A to Z) by default"));
	sortDefault->setCheckable(true);
	sortDefault->setChecked(Npp::prefs().funcListSortByDefault);
	connect(sortDefault, &QAction::toggled, this, [](bool on)
	{
		Npp::prefs().funcListSortByDefault = on;
		Npp::prefs().save();
	});
	_prefs->setMenu(prefsMenu);
	bar->addWidget(_search, 1);
	bar->addWidget(_sort);
	bar->addWidget(_reload);
	bar->addWidget(_prefs);
	root->addLayout(bar);

	_tree = new QTreeWidget(this);
	_tree->setHeaderHidden(true);
	_tree->setColumnCount(1);
	_tree->setObjectName(QStringLiteral("functionListTree"));
	_tree->header()->setStretchLastSection(true);
	root->addWidget(_tree, 1);

	_parseView = new EditView(this, false);
	_parseView->hide();

	_tree->installEventFilter(this);
	_search->installEventFilter(this);
	connect(_search, &QLineEdit::textChanged, this, [this] { rebuildTree(); markEntry(); });
	connect(_sort, &QToolButton::toggled, this, [this] { rebuildTree(); });
	connect(_reload, &QToolButton::clicked, this, [this] { reload(); });
	connect(_tree, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem* item, int) { openItem(item); });
	connect(_tree, &QTreeWidget::itemCollapsed, this, [this](QTreeWidgetItem* item)
	{
		if (!_rebuilding && item->data(0, KindRole).toInt() == 1)
			_states[_key].collapsed.insert(item->text(0));
	});
	connect(_tree, &QTreeWidget::itemExpanded, this, [this](QTreeWidgetItem* item)
	{
		if (!_rebuilding && item->data(0, KindRole).toInt() == 1)
			_states[_key].collapsed.remove(item->text(0));
	});
	applyIcons();
}

bool FunctionListPanel::eventFilter(QObject* obj, QEvent* event)
{
	// TVN_KEYDOWN and funclstSearchEditProc: Tab between the search field and the tree,
	// Escape clears the search (and goes back to the editor from the tree), Enter on a node folds it
	if (event->type() != QEvent::KeyPress)
		return QWidget::eventFilter(obj, event);
	auto* ke = static_cast<QKeyEvent*>(event);
	if (obj == _search)
	{
		if (ke->key() == Qt::Key_Escape)
		{
			_search->clear();
			return true;
		}
		if (ke->key() == Qt::Key_Tab)
		{
			_tree->setFocus();
			return true;
		}
	}
	else if (obj == _tree)
	{
		if (ke->key() == Qt::Key_Tab)
		{
			_search->setFocus();
			return true;
		}
		if (ke->key() == Qt::Key_Escape)
		{
			_search->clear();
			_mw->editView()->setFocus();
			return true;
		}
		if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter)
		{
			QTreeWidgetItem* item = _tree->currentItem();
			if (item && item->data(0, KindRole).toInt() != 2)
				item->setExpanded(!item->isExpanded());
			else
				openItem(item);
			return true;
		}
	}
	return QWidget::eventFilter(obj, event);
}

void FunctionListPanel::applyIcons()
{
	const QString dir = iconDir();
	_sort->setIcon(QIcon(dir + QStringLiteral("/toolbar/funclstSort.ico")));
	_reload->setIcon(QIcon(dir + QStringLiteral("/toolbar/funclstReload.ico")));
	_prefs->setIcon(QIcon(dir + QStringLiteral("/toolbar/funclstPreferences.ico")));
	if (!_key.isEmpty())
		rebuildTree();
}

void FunctionListPanel::setSorted(bool sorted)
{
	_sort->setChecked(sorted);
}

void FunctionListPanel::saveState()
{
	if (_key.isEmpty())
		return;
	State& st = _states[_key];
	st.search = _search->text();
	st.sort = _sort->isChecked();
}

bool FunctionListPanel::needsReload() const
{
	const Buffer* b = _mw->currentBuffer();
	return b != _parsedBuf || (b && b->lang != _parsedLang) || (b && b->fullName() != _key);
}

void FunctionListPanel::reload()
{
	// FunctionListPanel::reload()
	saveState();
	if (!_parsersReady)
	{
		_parsers.init(Npp::configDir() + QStringLiteral("/functionList"), Npp::dataDir() + QStringLiteral("/functionList"), _parseView);
		_parsersReady = true;
	}
	_found.clear();
	_markLine = _markEndLine = -1;

	Buffer* b = _mw->currentBuffer();
	_parsedBuf = b;
	_parsedLang = b ? b->lang : L_TEXT;
	if (!b)
	{
		_key.clear();
		_hasParser = false;
		rebuildTree();
		return;
	}
	LangType lang = b->lang;
	if (lang == L_JS_EMBEDDED)
		lang = L_JAVASCRIPT;
	const QString ext = b->isUntitled() ? QString() : QStringLiteral(".") + QFileInfo(b->path).suffix();
	_parseView->send(SCI_SETDOCPOINTER, 0, b->doc);
	_hasParser = _parsers.parse(_found, AssociationInfo(-1, lang, ext, lang == L_USER ? b->udlName : QString()));
	_parseView->send(SCI_SETDOCPOINTER, 0, 0);

	_key = b->fullName();
	_fileName = b->displayName();
	const bool known = _states.contains(_key);
	const State st = _states.value(_key);
	const QSignalBlocker blockSearch(_search);
	const QSignalBlocker blockSort(_sort);
	_search->setText(known ? st.search : QString());
	_sort->setChecked(known ? st.sort : Npp::prefs().funcListSortByDefault);
	rebuildTree();
	markEntry();
}

void FunctionListPanel::rebuildTree()
{
	_rebuilding = true;
	_tree->clear();
	_markLine = _markEndLine = -1;
	if (!_hasParser)
	{
		_rebuilding = false;
		return;
	}
	const QString dir = iconDir();
	const QIcon rootIcon(dir + QStringLiteral("/treeview/project_file.ico"));
	const QIcon nodeIcon(dir + QStringLiteral("/treeview/funcList_node.ico"));
	const QIcon leafIcon(dir + QStringLiteral("/treeview/funcList_leaf.ico"));

	auto* root = new QTreeWidgetItem(_tree, {_fileName});
	root->setIcon(0, rootIcon);
	root->setData(0, PosRole, -1);
	root->setData(0, KindRole, 0);
	root->setToolTip(0, _key);

	// search: only the functions whose name contains the text
	const QString filter = _search->text().trimmed();
	std::vector<const foundInfo*> items;
	for (const foundInfo& fi : _found)
	{
		if (filter.isEmpty() || QString::fromStdString(fi._data).contains(filter, Qt::CaseInsensitive))
			items.push_back(&fi);
	}
	// not sorted: in the order of the document (categorySortFunc), nodes at their first function
	std::stable_sort(items.begin(), items.end(), [](const foundInfo* a, const foundInfo* b) { return a->_pos < b->_pos; });

	QHash<QString, QTreeWidgetItem*> nodes;
	const State st = _states.value(_key);
	for (const foundInfo* fi : items)
	{
		QTreeWidgetItem* parent = root;
		// search: TreeView::searchLeafAndBuildTree(), the matching functions without their class node
		if (filter.isEmpty() && !fi->_data2.empty())
		{
			const QString nodeName = QString::fromStdString(fi->_data2);
			parent = nodes.value(nodeName);
			if (!parent)
			{
				parent = new QTreeWidgetItem(root, {nodeName});
				parent->setIcon(0, nodeIcon);
				parent->setData(0, PosRole, -1);
				parent->setData(0, KindRole, 1);
				nodes.insert(nodeName, parent);
			}
		}
		auto* leaf = new QTreeWidgetItem(parent, {QString::fromStdString(fi->_data)});
		leaf->setIcon(0, leafIcon);
		leaf->setData(0, PosRole, static_cast<qlonglong>(fi->_pos));
		leaf->setData(0, KindRole, 2);
	}
	if (_sort->isChecked())
		root->sortChildren(0, Qt::AscendingOrder);
	root->setExpanded(true);
	for (QTreeWidgetItem* node : nodes)
		node->setExpanded(!st.collapsed.contains(node->text(0)));
	_rebuilding = false;
	saveState();
}

void FunctionListPanel::openItem(QTreeWidgetItem* item)
{
	// FunctionListPanel::openSelection()
	if (!item || item->data(0, KindRole).toInt() != 2)
		return;
	const sptr_t pos = item->data(0, PosRole).toLongLong();
	if (pos < 0)
		return;
	EditView* e = _mw->editView();
	e->ex(SCI_ENSUREVISIBLE, e->ex(SCI_LINEFROMPOSITION, pos));
	e->scrollPosToCenter(pos);
	e->setFocus();
}

void FunctionListPanel::markEntry()
{
	// FunctionListPanel::markEntry() / findMarkEntry(): the function the caret is in
	QTreeWidgetItem* root = _tree->topLevelItem(0);
	if (!root)
		return;
	EditView* e = _mw->editView();
	const sptr_t line = e->ex(SCI_LINEFROMPOSITION, e->ex(SCI_GETCURRENTPOS));
	if (_markLine != -1 && line >= _markLine && line < _markEndLine)
		return;
	_markLine = -1;
	_markEndLine = e->ex(SCI_GETLINECOUNT) + 1;
	QTreeWidgetItem* found = nullptr;
	std::function<void(QTreeWidgetItem*)> walk = [&](QTreeWidgetItem* it)
	{
		for (int i = 0; i < it->childCount(); ++i)
		{
			QTreeWidgetItem* c = it->child(i);
			if (c->childCount() > 0)
			{
				walk(c);
				continue;
			}
			const sptr_t pos = c->data(0, PosRole).toLongLong();
			if (pos < 0)
				continue;
			const sptr_t l = e->ex(SCI_LINEFROMPOSITION, pos);
			if (line >= l)
			{
				if (l > _markLine)
				{
					_markLine = l;
					found = c;
				}
			}
			else if (l < _markEndLine)
			{
				_markEndLine = l;
			}
		}
	};
	walk(root);
	const QSignalBlocker blocker(_tree);
	_tree->setCurrentItem(found ? found : root);
	if (found)
		_tree->scrollToItem(found);
	if (_markLine == -1)
		_markEndLine = -1;
}
