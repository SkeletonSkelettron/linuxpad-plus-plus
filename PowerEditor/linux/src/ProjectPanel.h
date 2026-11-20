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

// View > Project Panels > Project Panel 1-3: Qt counterpart of WinControls/ProjectPanel.
// Workspace files use the Notepad++ format:
// <NotepadPlus><Project name=""><Folder name=""><File name="relative/path"/></Folder></Project></NotepadPlus>

#pragma once

#include <QStringList>
#include <QTreeWidget>
#include <QWidget>

class MainWindow;
class QToolButton;
class QMenu;

class ProjectTree : public QTreeWidget
{
	Q_OBJECT

public:
	using QTreeWidget::QTreeWidget;
	bool isEditing() const { return state() == EditingState; }

signals:
	void moved();

protected:
	void dragMoveEvent(QDragMoveEvent* event) override;
	void dropEvent(QDropEvent* event) override;

private:
	bool canDrop(QTreeWidgetItem* dragged, QTreeWidgetItem* target, QAbstractItemView::DropIndicatorPosition pos) const;
};

class ProjectPanel : public QWidget
{
	Q_OBJECT

public:
	enum NodeType { nodeType_root = 0, nodeType_project, nodeType_folder, nodeType_file };
	enum Cmd
	{
		// ProjectPanel_rc.h
		IDM_PROJECT_RENAME = 1, IDM_PROJECT_NEWFOLDER, IDM_PROJECT_ADDFILES, IDM_PROJECT_ADDFILESRECUSIVELY, IDM_PROJECT_DELETEFOLDER,
		IDM_PROJECT_DELETEFILE, IDM_PROJECT_MODIFYFILEPATH, IDM_PROJECT_MOVEUP, IDM_PROJECT_MOVEDOWN, IDM_PROJECT_NEWWS, IDM_PROJECT_OPENWS,
		IDM_PROJECT_RELOADWS, IDM_PROJECT_SAVEWS, IDM_PROJECT_SAVEASWS, IDM_PROJECT_SAVEACOPYASWS, IDM_PROJECT_NEWPROJECT,
		IDM_PROJECT_FINDINPROJECTSWS
	};

	ProjectPanel(MainWindow* mw, int panelId, QWidget* parent);

	bool openWorkSpace(const QString& path, bool force);
	void newWorkSpace();
	bool saveWorkSpace();
	bool saveWorkSpaceAs(bool saveCopyAs, const QString& path = QString());
	bool checkIfNeedSave();     // asks to save a modified workspace: false when cancelled
	void popupMenuCmd(int cmd); // the commands of the menus, on the selected item
	bool isDirty() const { return _dirty; }
	QString workSpaceFilePath() const { return _workSpaceFilePath; }
	QStringList workSpaceFiles(const QStringList& patterns) const; // enumWorkSpaceFiles(): for Find in Projects
	QString panelTitle() const;
	QTreeWidget* tree() const { return _tree; }
	void applyIcons();

	// for scripts and tests: add without dialogs
	QTreeWidgetItem* addFile(QTreeWidgetItem* parent, const QString& path);
	QTreeWidgetItem* addFolder(QTreeWidgetItem* parent, const QString& name, bool edit = true);
	QTreeWidgetItem* addProject(const QString& name, bool edit = true);
	void addFilesFrom(const QString& folderPath, QTreeWidgetItem* parent); // recursiveAddFilesFrom()
	static NodeType nodeType(const QTreeWidgetItem* item);

signals:
	void openFileRequested(const QString& path);
	void findInProjectsRequested(int panelId);

protected:
	bool eventFilter(QObject* obj, QEvent* event) override;

private:
	bool writeWorkSpace(const QString& path, bool updateGui);
	void buildProjectXml(void* xmlNode, QTreeWidgetItem* item, const QString& workspacePath) const;
	bool buildTreeFrom(const void* xmlNode, QTreeWidgetItem* parent);
	QString absoluteFilePath(const QString& relative) const;
	static QString relativePath(const QString& filePath, const QString& workspacePath);
	void setWorkSpaceDirty(bool dirty);
	void openSelectFile();
	bool saveWorkspaceRequest(const QString& title);
	QMenu* menuFor(QTreeWidgetItem* item);
	void showContextMenu(const QPoint& pos);
	void updateItemIcon(QTreeWidgetItem* item);
	QTreeWidgetItem* makeItem(QTreeWidgetItem* parent, const QString& label, NodeType type, const QString& path = QString());
	QString fileFilter() const;

	MainWindow* _mw;
	int _panelId;
	ProjectTree* _tree = nullptr;
	QToolButton* _wsButton = nullptr;
	QToolButton* _editButton = nullptr;
	QMenu* _workSpaceMenu = nullptr;
	QMenu* _projectMenu = nullptr;
	QMenu* _folderMenu = nullptr;
	QMenu* _fileMenu = nullptr;
	QString _workSpaceFilePath;
	bool _dirty = false;
	bool _building = false;
	QString _renameOld; // label of the item being renamed
};
