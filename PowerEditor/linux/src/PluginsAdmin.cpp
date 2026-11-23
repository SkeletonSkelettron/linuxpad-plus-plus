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

#include "PluginsAdmin.h"

#include "MainWindow.h"
#include "NppCommon.h"
#include "PluginsManager.h"

#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTreeWidget>
#include <QUrl>
#include <QVBoxLayout>

bool PluginsAdmin::exec(MainWindow* mw, PluginsManager* pm)
{
	QDialog dlg(mw);
	dlg.setObjectName(QStringLiteral("pluginsAdmin"));
	dlg.setWindowTitle(QStringLiteral("Plugins Admin"));
	auto* layout = new QVBoxLayout(&dlg);

	const QString configFile = QDir::toNativeSeparators(Npp::configDir() + QStringLiteral("/config.ini"));
	auto* info = new QLabel(QStringLiteral("These plugins come with LinuxPad++ or were imported into your plugins folder. "
		"LinuxPad++ loads only the ones you tick. Your choice is saved in %1 and applies the next time LinuxPad++ starts.")
		.arg(configFile), &dlg);
	info->setWordWrap(true);
	layout->addWidget(info);

	auto* filter = new QLineEdit(&dlg);
	filter->setObjectName(QStringLiteral("filter"));
	filter->setPlaceholderText(QStringLiteral("Search"));
	filter->setClearButtonEnabled(true);
	layout->addWidget(filter);

	auto* list = new QTreeWidget(&dlg);
	list->setObjectName(QStringLiteral("plugins"));
	list->setRootIsDecorated(false);
	list->setHeaderLabels({QStringLiteral("Plugin"), QStringLiteral("Comes from"), QStringLiteral("Now")});
	const QStringList loaded = pm ? pm->loadedModules() : QStringList(); // "<folder>.so"
	const QStringList chosen = Npp::prefs().enabledPlugins;
	for (const PluginsManager::AvailablePlugin& p : PluginsManager::availablePlugins())
	{
		auto* item = new QTreeWidgetItem(list);
		item->setText(0, p.folder);
		item->setData(0, Qt::UserRole, p.folder);
		item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
		item->setCheckState(0, chosen.contains(p.folder, Qt::CaseInsensitive) ? Qt::Checked : Qt::Unchecked);
		item->setText(1, p.userFolder ? QStringLiteral("your plugins folder") : QStringLiteral("LinuxPad++"));
		item->setText(2, loaded.contains(p.folder + QStringLiteral(".so"), Qt::CaseInsensitive) ? QStringLiteral("loaded") : QString());
		const QString folder = QDir::toNativeSeparators(QFileInfo(p.path).absolutePath());
		for (int column = 0; column < 3; ++column)
			item->setToolTip(column, folder);
	}
	list->header()->setStretchLastSection(false);
	list->header()->setSectionResizeMode(0, QHeaderView::Stretch);
	list->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
	list->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
	layout->addWidget(list, 1);
	if (list->topLevelItemCount() == 0)
		info->setText(info->text() + QStringLiteral("\n\nNo plugin was found in the plugins folders."));

	QObject::connect(filter, &QLineEdit::textChanged, list, [list](const QString& text)
	{
		for (int i = 0; i < list->topLevelItemCount(); ++i)
			list->topLevelItem(i)->setHidden(!list->topLevelItem(i)->text(0).contains(text.trimmed(), Qt::CaseInsensitive));
	});

	auto* row = new QHBoxLayout;
	auto setAll = [list](Qt::CheckState state)
	{
		for (int i = 0; i < list->topLevelItemCount(); ++i)
		{
			if (!list->topLevelItem(i)->isHidden())
				list->topLevelItem(i)->setCheckState(0, state);
		}
	};
	auto* all = new QPushButton(QStringLiteral("Select all"), &dlg);
	auto* none = new QPushButton(QStringLiteral("Select none"), &dlg);
	auto* openFolder = new QPushButton(QStringLiteral("Open Plugins Folder"), &dlg);
	QObject::connect(all, &QPushButton::clicked, &dlg, [setAll] { setAll(Qt::Checked); });
	QObject::connect(none, &QPushButton::clicked, &dlg, [setAll] { setAll(Qt::Unchecked); });
	QObject::connect(openFolder, &QPushButton::clicked, &dlg, []
	{
		// the folder for plugins of the user (Settings > Import > Import plugin(s)... copies there)
		const QString dir = PluginsManager::userPluginsDir();
		QDir().mkpath(dir);
		QDesktopServices::openUrl(QUrl::fromLocalFile(dir));
	});
	row->addWidget(all);
	row->addWidget(none);
	row->addStretch(1);
	row->addWidget(openFolder);
	layout->addLayout(row);

	auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
	QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
	QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
	layout->addWidget(buttons);
	dlg.resize(560, 560);

	if (dlg.exec() != QDialog::Accepted)
		return false;

	QStringList newChoice;
	for (int i = 0; i < list->topLevelItemCount(); ++i)
	{
		const QTreeWidgetItem* item = list->topLevelItem(i);
		if (item->checkState(0) == Qt::Checked)
			newChoice << item->data(0, Qt::UserRole).toString();
	}
	QStringList before = chosen;
	QStringList after = newChoice;
	before.sort(Qt::CaseInsensitive);
	after.sort(Qt::CaseInsensitive);
	if (before.join(QLatin1Char('\n')).compare(after.join(QLatin1Char('\n')), Qt::CaseInsensitive) == 0)
		return false;

	NppPrefs& prefs = Npp::prefs();
	prefs.enabledPlugins = newChoice;
	prefs.save();
	QMessageBox::information(mw, QStringLiteral("Plugins Admin"),
		QStringLiteral("Your choice of plugins is saved. It applies the next time LinuxPad++ starts."));
	return true;
}
