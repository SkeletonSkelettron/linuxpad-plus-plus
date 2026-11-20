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

// Reads the nativeLang XML files of the Windows build (installer/nativeLang)
// for menu captions.

#pragma once

#include <QHash>
#include <QMap>
#include <QString>

class Localization
{
public:
	static Localization& instance();

	bool load(const QString& fileName); // e.g. "english.xml"
	QString command(int id, const QString& fallback) const;
	QString menu(const QString& key, const QString& fallback) const; // main or sub menu id
	QString misc(const QString& id, const QString& fallback) const;  // MiscStrings (NativeLangSpeaker::getLocalizedStrFromID)
	QString languageName() const { return _languageName; }
	bool isRTL() const { return _rtl; }
	QMap<QString, QString> available() const; // display name -> file name

private:
	Localization() = default;
	QHash<int, QString> _commands;
	QHash<QString, QString> _menus;
	QHash<QString, QString> _misc;
	QString _languageName;
	bool _rtl = false;
};
