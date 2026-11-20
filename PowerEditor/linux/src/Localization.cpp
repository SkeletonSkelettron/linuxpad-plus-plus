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

#include "Localization.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include "NppCommon.h"
#include "pugixml.hpp"

Localization& Localization::instance()
{
	static Localization l;
	return l;
}

bool Localization::load(const QString& fileName)
{
	_commands.clear();
	_menus.clear();
	_misc.clear();
	_languageName.clear();
	_rtl = false;

	QString path = Npp::configDir() + QStringLiteral("/localization/") + fileName;
	if (!QFileInfo::exists(path))
		path = Npp::dataDir() + QStringLiteral("/localization/") + fileName;

	pugi::xml_document doc;
	if (!doc.load_file(QFile::encodeName(path).constData()))
		return false;

	pugi::xml_node lang = doc.child("NotepadPlus").child("Native-Langue");
	_languageName = QString::fromUtf8(lang.attribute("name").as_string());
	_rtl = QString::fromUtf8(lang.attribute("RTL").as_string()) == QStringLiteral("yes");

	pugi::xml_node mainMenu = lang.child("Menu").child("Main");
	for (pugi::xml_node it : mainMenu.child("Entries").children("Item"))
		_menus.insert(QString::fromUtf8(it.attribute("menuId").as_string()), QString::fromUtf8(it.attribute("name").as_string()));
	for (pugi::xml_node it : mainMenu.child("SubEntries").children("Item"))
		_menus.insert(QString::fromUtf8(it.attribute("subMenuId").as_string()), QString::fromUtf8(it.attribute("name").as_string()));
	for (pugi::xml_node it : mainMenu.child("Commands").children("Item"))
		_commands.insert(it.attribute("id").as_int(), QString::fromUtf8(it.attribute("name").as_string()));
	for (pugi::xml_node it : lang.child("MiscStrings").children())
		_misc.insert(QString::fromUtf8(it.name()), QString::fromUtf8(it.attribute("value").as_string()));
	return true;
}

QString Localization::misc(const QString& id, const QString& fallback) const
{
	auto it = _misc.constFind(id);
	return (it != _misc.constEnd() && !it.value().isEmpty()) ? it.value() : fallback;
}

QString Localization::command(int id, const QString& fallback) const
{
	auto it = _commands.constFind(id);
	return (it != _commands.constEnd() && !it.value().isEmpty()) ? it.value() : fallback;
}

QString Localization::menu(const QString& key, const QString& fallback) const
{
	if (key.isEmpty())
		return fallback;
	auto it = _menus.constFind(key);
	return (it != _menus.constEnd() && !it.value().isEmpty()) ? it.value() : fallback;
}

QMap<QString, QString> Localization::available() const
{
	QMap<QString, QString> result;
	const QFileInfoList files = QDir(Npp::dataDir() + QStringLiteral("/localization")).entryInfoList({QStringLiteral("*.xml")}, QDir::Files, QDir::Name);
	for (const QFileInfo& fi : files)
	{
		pugi::xml_document doc;
		QString name = fi.completeBaseName();
		if (doc.load_file(QFile::encodeName(fi.absoluteFilePath()).constData()))
		{
			const char* n = doc.child("NotepadPlus").child("Native-Langue").attribute("name").as_string();
			if (n && *n)
				name = QString::fromUtf8(n);
		}
		if (result.contains(name))
			name += QStringLiteral(" (%1)").arg(fi.completeBaseName());
		result.insert(name, fi.fileName());
	}
	return result;
}
