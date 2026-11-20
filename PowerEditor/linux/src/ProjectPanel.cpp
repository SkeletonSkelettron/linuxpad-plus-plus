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

#include "ProjectPanel.h"

#include <functional>

#include <QDir>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLineEdit>
#include <QRegularExpression>
#include <QMenu>
#include <QMessageBox>
#include <QToolButton>
#include <QVBoxLayout>

#include <pugixml.hpp>

#include "MainWindow.h"
#include "NppCommon.h"

namespace
{
	constexpr int TypeRole = Qt::UserRole;     // NodeType
	constexpr int PathRole = Qt::UserRole + 1; // full path of a file

	QString iconDir()
	{
		return Npp::prefs().isDark() ? QStringLiteral(":/icons/dark/panels/treeview/") : QStringLiteral(":/icons/standard/panels/treeview/");
	}
}

// ---------------------------------------------------------------------------
// tree with the drag and drop of TreeView::beginDrag() / dropItem(): files and folders go
// into projects and folders, projects stay under the workspace
// ---------------------------------------------------------------------------
bool ProjectTree::canDrop(QTreeWidgetItem* dragged, QTreeWidgetItem* target, QAbstractItemView::DropIndicatorPosition pos) const
{
	if (!dragged || !target || dragged == target)
		return false;
	for (QTreeWidgetItem* p = target; p; p = p->parent())
	{
		if (p == dragged)
			return false; // into itself
	}
	const ProjectPanel::NodeType dt = ProjectPanel::nodeType(dragged);
	QTreeWidgetItem* parent = target;
	if (pos == QAbstractItemView::AboveItem || pos == QAbstractItemView::BelowItem)
		parent = target->parent();
	if (!parent)
		return false;
	const ProjectPanel::NodeType pt = ProjectPanel::nodeType(parent);
	if (dt == ProjectPanel::nodeType_project)
		return pt == ProjectPanel::nodeType_root;
	if (dt == ProjectPanel::nodeType_folder || dt == ProjectPanel::nodeType_file)
		return pt == ProjectPanel::nodeType_project || pt == ProjectPanel::nodeType_folder;
	return false;
}

void ProjectTree::dragMoveEvent(QDragMoveEvent* event)
{
	QTreeWidget::dragMoveEvent(event);
	if (event->source() != this || !canDrop(currentItem(), itemAt(event->position().toPoint()), dropIndicatorPosition()))
		event->ignore();
}

void ProjectTree::dropEvent(QDropEvent* event)
{
	QTreeWidgetItem* dragged = currentItem();
	QTreeWidgetItem* target = itemAt(event->position().toPoint());
	if (event->source() != this || !canDrop(dragged, target, dropIndicatorPosition()))
	{
		event->ignore();
		return;
	}
	QTreeWidget::dropEvent(event);
	emit moved();
}

// ---------------------------------------------------------------------------
// ProjectPanel
// ---------------------------------------------------------------------------
ProjectPanel::ProjectPanel(MainWindow* mw, int panelId, QWidget* parent)
	: QWidget(parent), _mw(mw), _panelId(panelId)
{
	setObjectName(QStringLiteral("projectPanel%1").arg(panelId + 1));
	auto* root = new QVBoxLayout(this);
	root->setContentsMargins(2, 2, 2, 2);
	root->setSpacing(2);
	auto* bar = new QHBoxLayout;
	_wsButton = new QToolButton(this);
	_wsButton->setText(QStringLiteral("Workspace"));
	_wsButton->setPopupMode(QToolButton::InstantPopup);
	_wsButton->setAutoRaise(true);
	_editButton = new QToolButton(this);
	_editButton->setText(QStringLiteral("Edit"));
	_editButton->setAutoRaise(true);
	bar->addWidget(_wsButton);
	bar->addWidget(_editButton);
	bar->addStretch();
	root->addLayout(bar);

	_tree = new ProjectTree(this);
	_tree->setObjectName(QStringLiteral("projectTree"));
	_tree->setHeaderHidden(true);
	_tree->setEditTriggers(QAbstractItemView::NoEditTriggers); // Rename / F2
	_tree->setDragDropMode(QAbstractItemView::InternalMove);
	_tree->setDefaultDropAction(Qt::MoveAction);
	_tree->setContextMenuPolicy(Qt::CustomContextMenu);
	_tree->installEventFilter(this);
	root->addWidget(_tree, 1);

	// ProjectPanel::initMenus()
	auto makeMenu = [this](std::initializer_list<std::pair<int, const char*>> items)
	{
		auto* m = new QMenu(this);
		for (const auto& [cmd, label] : items)
		{
			if (cmd == 0)
			{
				m->addSeparator();
				continue;
			}
			QAction* a = m->addAction(QString::fromUtf8(label));
			connect(a, &QAction::triggered, this, [this, cmd = cmd] { popupMenuCmd(cmd); });
		}
		return m;
	};
	_workSpaceMenu = makeMenu({{IDM_PROJECT_NEWWS, "New Workspace"}, {IDM_PROJECT_OPENWS, "Open Workspace"}, {IDM_PROJECT_RELOADWS, "Reload Workspace"},
	                           {IDM_PROJECT_SAVEWS, "Save"}, {IDM_PROJECT_SAVEASWS, "Save As..."}, {IDM_PROJECT_SAVEACOPYASWS, "Save a Copy As..."},
	                           {0, ""}, {IDM_PROJECT_NEWPROJECT, "Add New Project"}, {0, ""}, {IDM_PROJECT_FINDINPROJECTSWS, "Find in Projects..."}});
	_projectMenu = makeMenu({{IDM_PROJECT_MOVEUP, "Move Up\tCtrl+Up"}, {IDM_PROJECT_MOVEDOWN, "Move Down\tCtrl+Down"}, {0, ""},
	                         {IDM_PROJECT_RENAME, "Rename"}, {IDM_PROJECT_NEWFOLDER, "Add Folder"}, {IDM_PROJECT_ADDFILES, "Add Files..."},
	                         {IDM_PROJECT_ADDFILESRECUSIVELY, "Add Files from Directory..."}, {IDM_PROJECT_DELETEFOLDER, "Remove\tDEL"}});
	_folderMenu = makeMenu({{IDM_PROJECT_MOVEUP, "Move Up\tCtrl+Up"}, {IDM_PROJECT_MOVEDOWN, "Move Down\tCtrl+Down"}, {0, ""},
	                        {IDM_PROJECT_RENAME, "Rename"}, {IDM_PROJECT_NEWFOLDER, "Add Folder"}, {IDM_PROJECT_ADDFILES, "Add Files..."},
	                        {IDM_PROJECT_ADDFILESRECUSIVELY, "Add Files from Directory..."}, {IDM_PROJECT_DELETEFOLDER, "Remove\tDEL"}});
	_fileMenu = makeMenu({{IDM_PROJECT_MOVEUP, "Move Up\tCtrl+Up"}, {IDM_PROJECT_MOVEDOWN, "Move Down\tCtrl+Down"}, {0, ""},
	                      {IDM_PROJECT_RENAME, "Rename"}, {IDM_PROJECT_DELETEFILE, "Remove\tDEL"}, {IDM_PROJECT_MODIFYFILEPATH, "Modify File Path"}});
	_wsButton->setMenu(_workSpaceMenu);

	connect(_editButton, &QToolButton::clicked, this, [this]
	{
		// IDB_EDIT_BTN: the menu of the selected project, folder or file
		QTreeWidgetItem* item = _tree->currentItem();
		if (item && nodeType(item) != nodeType_root)
			menuFor(item)->exec(_editButton->mapToGlobal(QPoint(0, _editButton->height())));
	});
	connect(_tree, &QTreeWidget::customContextMenuRequested, this, &ProjectPanel::showContextMenu);
	connect(_tree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* item)
	{
		if (nodeType(item) == nodeType_file)
			openSelectFile();
	});
	connect(_tree, &QTreeWidget::itemExpanded, this, &ProjectPanel::updateItemIcon);
	connect(_tree, &QTreeWidget::itemCollapsed, this, &ProjectPanel::updateItemIcon);
	connect(_tree, &ProjectTree::moved, this, [this] { setWorkSpaceDirty(true); });
	connect(_tree, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem* item, int)
	{
		// TVN_ENDLABELEDIT
		if (_building || _renameOld.isNull() || item != _tree->currentItem())
			return;
		const QString oldLabel = _renameOld;
		_renameOld = QString();
		const QString newLabel = item->text(0);
		if (newLabel.isEmpty() || newLabel == oldLabel)
		{
			const bool b = _building;
			_building = true;
			item->setText(0, oldLabel);
			_building = b;
			return;
		}
		if (nodeType(item) == nodeType_file)
		{
			// the old label in the file path is replaced by the new one
			QString path = item->data(0, PathRole).toString();
			const qsizetype found = path.lastIndexOf(oldLabel);
			if (found >= 0)
				path.replace(found, oldLabel.size(), newLabel);
			_building = true;
			item->setData(0, PathRole, path);
			item->setToolTip(0, path);
			updateItemIcon(item);
			_building = false;
		}
		setWorkSpaceDirty(true);
	});
	newWorkSpace();
}

QString ProjectPanel::panelTitle() const
{
	return QStringLiteral("Project Panel %1").arg(_panelId + 1);
}

ProjectPanel::NodeType ProjectPanel::nodeType(const QTreeWidgetItem* item)
{
	return item ? static_cast<NodeType>(item->data(0, TypeRole).toInt()) : nodeType_root;
}

void ProjectPanel::updateItemIcon(QTreeWidgetItem* item)
{
	if (!item)
		return;
	QString name;
	switch (nodeType(item))
	{
		case nodeType_root: name = _dirty ? QStringLiteral("project_work_space_dirty.ico") : QStringLiteral("project_work_space.ico"); break;
		case nodeType_project: name = QStringLiteral("project_root.ico"); break;
		case nodeType_folder: name = item->isExpanded() ? QStringLiteral("project_folder_open.ico") : QStringLiteral("project_folder_close.ico"); break;
		case nodeType_file:
			name = QFileInfo::exists(item->data(0, PathRole).toString()) ? QStringLiteral("project_file.ico") : QStringLiteral("project_file_invalid.ico");
			break;
	}
	const bool b = _building;
	_building = true;
	item->setIcon(0, QIcon(iconDir() + name));
	_building = b;
}

void ProjectPanel::applyIcons()
{
	std::function<void(QTreeWidgetItem*)> walk = [&](QTreeWidgetItem* it)
	{
		updateItemIcon(it);
		for (int i = 0; i < it->childCount(); ++i)
			walk(it->child(i));
	};
	for (int i = 0; i < _tree->topLevelItemCount(); ++i)
		walk(_tree->topLevelItem(i));
}

QTreeWidgetItem* ProjectPanel::makeItem(QTreeWidgetItem* parent, const QString& label, NodeType type, const QString& path)
{
	const bool b = _building;
	_building = true;
	auto* item = parent ? new QTreeWidgetItem(parent, {label}) : new QTreeWidgetItem(_tree, {label});
	item->setData(0, TypeRole, static_cast<int>(type));
	Qt::ItemFlags flags = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
	if (type != nodeType_root)
		flags |= Qt::ItemIsEditable | Qt::ItemIsDragEnabled;
	if (type != nodeType_file)
		flags |= Qt::ItemIsDropEnabled;
	item->setFlags(flags);
	if (type == nodeType_file)
	{
		item->setData(0, PathRole, path);
		item->setToolTip(0, path);
	}
	_building = b;
	updateItemIcon(item);
	return item;
}

void ProjectPanel::setWorkSpaceDirty(bool dirty)
{
	_dirty = dirty;
	updateItemIcon(_tree->topLevelItem(0));
}

void ProjectPanel::newWorkSpace()
{
	_building = true;
	_tree->clear();
	_building = false;
	QTreeWidgetItem* root = makeItem(nullptr, QStringLiteral("Workspace"), nodeType_root);
	root->setToolTip(0, QString());
	_workSpaceFilePath.clear();
	setWorkSpaceDirty(false);
}

QString ProjectPanel::absoluteFilePath(const QString& relative) const
{
	// Windows workspace files use backslashes
	QString path = relative;
	path.replace(QLatin1Char('\\'), QLatin1Char('/'));
	if (!QDir::isRelativePath(path))
		return path;
	return QDir::cleanPath(QFileInfo(_workSpaceFilePath).absolutePath() + QLatin1Char('/') + path);
}

QString ProjectPanel::relativePath(const QString& filePath, const QString& workspacePath)
{
	// ProjectPanel::getRelativePath(): files under the folder of the workspace file are relative
	const QString dir = QFileInfo(workspacePath).absolutePath();
	if (filePath.startsWith(dir + QLatin1Char('/')))
		return filePath.mid(dir.size() + 1);
	return filePath;
}

bool ProjectPanel::buildTreeFrom(const void* xmlNode, QTreeWidgetItem* parent)
{
	const pugi::xml_node& node = *static_cast<const pugi::xml_node*>(xmlNode);
	for (pugi::xml_node child = node.first_child(); child; child = child.next_sibling())
	{
		const QString name = QString::fromUtf8(child.name());
		if (name == QStringLiteral("Folder"))
		{
			QTreeWidgetItem* folder = makeItem(parent, QString::fromUtf8(child.attribute("name").value()), nodeType_folder);
			if (child.first_child() && !buildTreeFrom(&child, folder))
				return false;
		}
		else if (name == QStringLiteral("File"))
		{
			QString value = QString::fromUtf8(child.attribute("name").value());
			const QString fullPath = absoluteFilePath(value);
			value.replace(QLatin1Char('\\'), QLatin1Char('/'));
			makeItem(parent, value.section(QLatin1Char('/'), -1), nodeType_file, fullPath);
		}
	}
	return true;
}

bool ProjectPanel::openWorkSpace(const QString& path, bool force)
{
	// ProjectPanel::openWorkSpace()
	if (path.isEmpty())
		return false;
	if (!force && !_workSpaceFilePath.isEmpty())
	{
		if (QFileInfo(path).absoluteFilePath() == QFileInfo(_workSpaceFilePath).absoluteFilePath())
			return true;
		if (!saveWorkspaceRequest(QStringLiteral("Open Workspace")))
			return true;
	}
	if (!QFileInfo::exists(path))
		return false;
	pugi::xml_document doc;
	if (!doc.load_file(QFile::encodeName(path).constData()))
		return false;
	pugi::xml_node root = doc.child("NotepadPlus");
	pugi::xml_node project = root.child("Project");
	if (!project)
		return false;
	_building = true;
	_tree->clear();
	_building = false;
	_workSpaceFilePath = QFileInfo(path).absoluteFilePath();
	QTreeWidgetItem* rootItem = makeItem(nullptr, QFileInfo(path).fileName(), nodeType_root);
	rootItem->setToolTip(0, _workSpaceFilePath);
	for (; project; project = project.next_sibling("Project"))
	{
		QTreeWidgetItem* projectItem = makeItem(rootItem, QString::fromUtf8(project.attribute("name").value()), nodeType_project);
		buildTreeFrom(&project, projectItem);
	}
	setWorkSpaceDirty(false);
	rootItem->setExpanded(true);
	return true;
}

void ProjectPanel::buildProjectXml(void* xmlNode, QTreeWidgetItem* item, const QString& workspacePath) const
{
	pugi::xml_node& node = *static_cast<pugi::xml_node*>(xmlNode);
	for (int i = 0; i < item->childCount(); ++i)
	{
		QTreeWidgetItem* c = item->child(i);
		if (nodeType(c) == nodeType_file)
		{
			pugi::xml_node f = node.append_child("File");
			f.append_attribute("name") = relativePath(c->data(0, PathRole).toString(), workspacePath).toUtf8().constData();
		}
		else
		{
			pugi::xml_node folder = node.append_child("Folder");
			folder.append_attribute("name") = c->text(0).toUtf8().constData();
			buildProjectXml(&folder, c, workspacePath);
		}
	}
}

bool ProjectPanel::writeWorkSpace(const QString& path, bool updateGui)
{
	// ProjectPanel::writeWorkSpace()
	QTreeWidgetItem* root = _tree->topLevelItem(0);
	if (!root)
		return false;
	pugi::xml_document doc;
	pugi::xml_node decl = doc.append_child(pugi::node_declaration);
	decl.append_attribute("version") = "1.0";
	decl.append_attribute("encoding") = "UTF-8";
	pugi::xml_node npp = doc.append_child("NotepadPlus");
	const QString absPath = QFileInfo(path).absoluteFilePath();
	for (int i = 0; i < root->childCount(); ++i)
	{
		QTreeWidgetItem* project = root->child(i);
		pugi::xml_node p = npp.append_child("Project");
		p.append_attribute("name") = project->text(0).toUtf8().constData();
		buildProjectXml(&p, project, absPath);
	}
	if (!doc.save_file(QFile::encodeName(absPath).constData(), "    ", pugi::format_default, pugi::encoding_utf8))
	{
		QMessageBox::critical(this, _workSpaceFilePath.isEmpty() ? panelTitle() : QFileInfo(_workSpaceFilePath).fileName(),
		                      QStringLiteral("An error occurred while writing your workspace file.\nYour workspace has not been saved."));
		return false;
	}
	if (updateGui)
	{
		const bool b = _building;
		_building = true;
		root->setText(0, QFileInfo(absPath).fileName());
		root->setToolTip(0, absPath);
		_building = b;
	}
	return true;
}

QString ProjectPanel::fileFilter() const
{
	// ProjectPanel::setFileExtFilter(): MISC. > Workspace file ext.
	QString ext = Npp::prefs().workspaceFileExt.trimmed();
	QStringList filters;
	if (!ext.isEmpty())
	{
		if (ext.startsWith(QLatin1Char('.')))
			ext.remove(0, 1);
		filters << QStringLiteral("Workspace file (*.%1)").arg(ext);
	}
	filters << QStringLiteral("All types (*)");
	return filters.join(QStringLiteral(";;"));
}

bool ProjectPanel::saveWorkSpace()
{
	if (_workSpaceFilePath.isEmpty())
		return saveWorkSpaceAs(false);
	if (!writeWorkSpace(_workSpaceFilePath, true))
		return false;
	setWorkSpaceDirty(false);
	return true;
}

bool ProjectPanel::saveWorkSpaceAs(bool saveCopyAs, const QString& givenPath)
{
	QString fn = givenPath;
	if (fn.isEmpty())
	{
		QString ext = Npp::prefs().workspaceFileExt.trimmed();
		fn = QFileDialog::getSaveFileName(this, saveCopyAs ? QStringLiteral("Save a Copy As") : QStringLiteral("Save As"),
		                                  _workSpaceFilePath.isEmpty() ? QDir::homePath() : _workSpaceFilePath, fileFilter());
		if (fn.isEmpty())
			return false;
		if (!ext.isEmpty() && QFileInfo(fn).suffix().isEmpty())
			fn += ext.startsWith(QLatin1Char('.')) ? ext : QLatin1Char('.') + ext;
	}
	if (!writeWorkSpace(fn, !saveCopyAs))
		return false;
	if (!saveCopyAs)
	{
		_workSpaceFilePath = QFileInfo(fn).absoluteFilePath();
		setWorkSpaceDirty(false);
	}
	return true;
}

bool ProjectPanel::checkIfNeedSave()
{
	// ProjectPanel::checkIfNeedSave()
	if (!_dirty)
		return true;
	const QString title = _workSpaceFilePath.isEmpty() ? panelTitle() : QFileInfo(_workSpaceFilePath).fileName();
	const auto res = QMessageBox::question(this, title, QStringLiteral("The workspace was modified. Do you want to save it?"),
	                                       QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
	if (res == QMessageBox::Yes)
		return saveWorkSpace();
	return res == QMessageBox::No;
}

bool ProjectPanel::saveWorkspaceRequest(const QString& title)
{
	// returns true to go on, false to stop
	if (!_dirty)
		return true;
	const auto res = QMessageBox::question(this, title, QStringLiteral("The current workspace was modified. Do you want to save the current project?"),
	                                       QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
	if (res == QMessageBox::Yes)
		return saveWorkSpace();
	return res == QMessageBox::No;
}

QStringList ProjectPanel::workSpaceFiles(const QStringList& patterns) const
{
	// ProjectPanel::enumWorkSpaceFiles()
	QStringList files;
	QList<QRegularExpression> res;
	for (const QString& p : patterns)
		res << QRegularExpression(QRegularExpression::wildcardToRegularExpression(p), QRegularExpression::CaseInsensitiveOption);
	std::function<void(QTreeWidgetItem*)> walk = [&](QTreeWidgetItem* it)
	{
		for (int i = 0; i < it->childCount(); ++i)
		{
			QTreeWidgetItem* c = it->child(i);
			if (nodeType(c) == nodeType_file)
			{
				bool match = res.isEmpty();
				for (const QRegularExpression& re : res)
					match = match || re.match(c->text(0)).hasMatch();
				if (match)
					files << c->data(0, PathRole).toString();
			}
			else
			{
				walk(c);
			}
		}
	};
	if (QTreeWidgetItem* root = _tree->topLevelItem(0))
		walk(root);
	return files;
}

void ProjectPanel::openSelectFile()
{
	QTreeWidgetItem* item = _tree->currentItem();
	if (nodeType(item) != nodeType_file)
		return;
	const QString path = item->data(0, PathRole).toString();
	if (QFileInfo::exists(path))
		emit openFileRequested(path);
	updateItemIcon(item);
}

QTreeWidgetItem* ProjectPanel::addFile(QTreeWidgetItem* parent, const QString& path)
{
	QTreeWidgetItem* item = makeItem(parent, QFileInfo(path).fileName(), nodeType_file, QFileInfo(path).absoluteFilePath());
	setWorkSpaceDirty(true);
	return item;
}

QTreeWidgetItem* ProjectPanel::addFolder(QTreeWidgetItem* parent, const QString& name, bool edit)
{
	// ProjectPanel::addFolder()
	QTreeWidgetItem* item = makeItem(parent, name, nodeType_folder);
	parent->setExpanded(true);
	if (edit)
	{
		_tree->setCurrentItem(item);
		_renameOld = item->text(0);
		_tree->editItem(item);
	}
	return item;
}

QTreeWidgetItem* ProjectPanel::addProject(const QString& name, bool edit)
{
	QTreeWidgetItem* root = _tree->topLevelItem(0);
	QTreeWidgetItem* item = makeItem(root, name, nodeType_project);
	setWorkSpaceDirty(true);
	root->setExpanded(true);
	if (edit)
	{
		_tree->setCurrentItem(item);
		_renameOld = item->text(0);
		_tree->editItem(item);
	}
	return item;
}

void ProjectPanel::addFilesFrom(const QString& folderPath, QTreeWidgetItem* parent)
{
	// ProjectPanel::recursiveAddFilesFrom(): hidden directories are ignored, folders first
	const QDir dir(folderPath);
	for (const QFileInfo& d : dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name))
	{
		QTreeWidgetItem* folder = makeItem(parent, d.fileName(), nodeType_folder);
		addFilesFrom(d.absoluteFilePath(), folder);
	}
	for (const QFileInfo& f : dir.entryInfoList(QDir::Files | QDir::Hidden | QDir::System, QDir::Name))
		makeItem(parent, f.fileName(), nodeType_file, f.absoluteFilePath());
	parent->setExpanded(true);
}

QMenu* ProjectPanel::menuFor(QTreeWidgetItem* item)
{
	switch (nodeType(item))
	{
		case nodeType_root: return _workSpaceMenu;
		case nodeType_project: return _projectMenu;
		case nodeType_folder: return _folderMenu;
		case nodeType_file: return _fileMenu;
	}
	return _workSpaceMenu;
}

void ProjectPanel::showContextMenu(const QPoint& pos)
{
	QTreeWidgetItem* item = _tree->itemAt(pos);
	if (!item)
		return;
	_tree->setCurrentItem(item);
	menuFor(item)->exec(_tree->viewport()->mapToGlobal(pos));
}

bool ProjectPanel::eventFilter(QObject* obj, QEvent* event)
{
	// TVN_KEYDOWN
	if (obj == _tree && event->type() == QEvent::KeyPress && !_tree->isEditing())
	{
		auto* ke = static_cast<QKeyEvent*>(event);
		QTreeWidgetItem* item = _tree->currentItem();
		const NodeType type = nodeType(item);
		const bool ctrl = ke->modifiers() & Qt::ControlModifier;
		switch (ke->key())
		{
			case Qt::Key_Delete:
				if (type == nodeType_project || type == nodeType_folder)
					popupMenuCmd(IDM_PROJECT_DELETEFOLDER);
				else if (type == nodeType_file)
					popupMenuCmd(IDM_PROJECT_DELETEFILE);
				return true;
			case Qt::Key_Return:
			case Qt::Key_Enter:
				if (type == nodeType_file)
					openSelectFile();
				else if (item)
					item->setExpanded(!item->isExpanded());
				return true;
			case Qt::Key_Up:
				if (ctrl)
				{
					popupMenuCmd(IDM_PROJECT_MOVEUP);
					return true;
				}
				break;
			case Qt::Key_Down:
				if (ctrl)
				{
					popupMenuCmd(IDM_PROJECT_MOVEDOWN);
					return true;
				}
				break;
			case Qt::Key_F2:
				popupMenuCmd(IDM_PROJECT_RENAME);
				return true;
			default:
				break;
		}
	}
	return QWidget::eventFilter(obj, event);
}

void ProjectPanel::popupMenuCmd(int cmd)
{
	// ProjectPanel::popupMenuCmd()
	QTreeWidgetItem* item = _tree->currentItem();
	if (!item)
		item = _tree->topLevelItem(0);
	if (!item)
		return;
	switch (cmd)
	{
		case IDM_PROJECT_NEWPROJECT:
			addProject(QStringLiteral("Project Name"));
			break;
		case IDM_PROJECT_NEWWS:
			if (_dirty)
			{
				const auto res = QMessageBox::question(this, QStringLiteral("New Workspace"),
				                                       QStringLiteral("The current workspace was modified. Do you want to save the current project?"),
				                                       QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
				if (res == QMessageBox::Cancel || (res == QMessageBox::Yes && !saveWorkSpace()))
					return;
			}
			newWorkSpace();
			break;
		case IDM_PROJECT_RENAME:
			if (nodeType(item) != nodeType_root)
			{
				_renameOld = item->text(0);
				_tree->editItem(item);
			}
			break;
		case IDM_PROJECT_NEWFOLDER:
			if (nodeType(item) == nodeType_project || nodeType(item) == nodeType_folder)
			{
				addFolder(item, QStringLiteral("Folder Name"));
				setWorkSpaceDirty(true);
			}
			break;
		case IDM_PROJECT_MOVEUP:
		case IDM_PROJECT_MOVEDOWN:
		{
			QTreeWidgetItem* parent = item->parent();
			if (!parent)
				return;
			const int index = parent->indexOfChild(item);
			const int to = index + (cmd == IDM_PROJECT_MOVEUP ? -1 : 1);
			if (to < 0 || to >= parent->childCount())
				return;
			const bool expanded = item->isExpanded();
			parent->takeChild(index);
			parent->insertChild(to, item);
			item->setExpanded(expanded);
			_tree->setCurrentItem(item);
			setWorkSpaceDirty(true);
			break;
		}
		case IDM_PROJECT_ADDFILES:
		{
			const QStringList files = QFileDialog::getOpenFileNames(this, QStringLiteral("Add Files"), QString(), QStringLiteral("All types (*)"));
			if (files.isEmpty())
				return;
			for (const QString& f : files)
				makeItem(item, QFileInfo(f).fileName(), nodeType_file, QFileInfo(f).absoluteFilePath());
			item->setExpanded(true);
			setWorkSpaceDirty(true);
			break;
		}
		case IDM_PROJECT_ADDFILESRECUSIVELY:
		{
			const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("Select a folder to add in your project"));
			if (dir.isEmpty())
				return;
			addFilesFrom(dir, item);
			setWorkSpaceDirty(true);
			break;
		}
		case IDM_PROJECT_OPENWS:
		{
			if (!saveWorkspaceRequest(QStringLiteral("Open Workspace")))
				return;
			const QString fn = QFileDialog::getOpenFileName(this, QStringLiteral("Open Workspace"), QString(), fileFilter());
			if (!fn.isEmpty() && !openWorkSpace(fn, true))
				QMessageBox::warning(this, QStringLiteral("Open Workspace"),
				                     QStringLiteral("The workspace could not be opened.\nIt seems the file to open is not a valid project file."));
			break;
		}
		case IDM_PROJECT_RELOADWS:
			if (_dirty && QMessageBox::question(this, QStringLiteral("Reload Workspace"),
			                                    QStringLiteral("The current workspace was modified. Reloading will discard all modifications.\nDo you want to continue?")) !=
			                  QMessageBox::Yes)
				return;
			if (QFileInfo::exists(_workSpaceFilePath))
				openWorkSpace(_workSpaceFilePath, true);
			else
				QMessageBox::information(this, QStringLiteral("Reload Workspace"), QStringLiteral("Cannot find the file to reload."));
			break;
		case IDM_PROJECT_SAVEWS:
			saveWorkSpace();
			break;
		case IDM_PROJECT_SAVEASWS:
		case IDM_PROJECT_SAVEACOPYASWS:
			saveWorkSpaceAs(cmd == IDM_PROJECT_SAVEACOPYASWS);
			break;
		case IDM_PROJECT_FINDINPROJECTSWS:
			emit findInProjectsRequested(_panelId);
			break;
		case IDM_PROJECT_DELETEFOLDER:
		{
			if (nodeType(item) != nodeType_project && nodeType(item) != nodeType_folder)
				return;
			if (item->childCount() > 0 &&
			    QMessageBox::question(this, QStringLiteral("Remove folder from project"),
			                          QStringLiteral("All the sub-items will be removed.\nAre you sure you want to remove this folder from the project?")) !=
			        QMessageBox::Yes)
				return;
			delete item;
			setWorkSpaceDirty(true);
			break;
		}
		case IDM_PROJECT_DELETEFILE:
			if (nodeType(item) != nodeType_file)
				return;
			if (QMessageBox::question(this, QStringLiteral("Remove file from project"),
			                          QStringLiteral("Are you sure you want to remove this file from the project?")) != QMessageBox::Yes)
				return;
			delete item;
			setWorkSpaceDirty(true);
			break;
		case IDM_PROJECT_MODIFYFILEPATH:
		{
			// FileRelocalizerDlg
			if (nodeType(item) != nodeType_file)
				return;
			const QString old = item->data(0, PathRole).toString();
			bool ok = false;
			const QString fn = QInputDialog::getText(this, QStringLiteral("Change file full path name"), QStringLiteral("New full file path:"),
			                                         QLineEdit::Normal, old, &ok);
			if (!ok || fn.isEmpty() || fn == old)
				return;
			_building = true;
			item->setData(0, PathRole, fn);
			item->setToolTip(0, fn);
			item->setText(0, QFileInfo(fn).fileName());
			_building = false;
			updateItemIcon(item);
			setWorkSpaceDirty(true);
			break;
		}
		default:
			break;
	}
}
