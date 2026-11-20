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

// Dialogs of the Linux front end: Go To, Column Editor, hash generators,
// Preferences, Style Configurator, Shortcut Mapper, Windows, Run Macro.
// Qt counterparts of the dialogs under PowerEditor/src/WinControls.

#pragma once

#include <QCryptographicHash>
#include <QString>
#include <QStringList>

class EditView;
class MainWindow;
class QWidget;

namespace NppDialogs
{
	void goToLine(QWidget* parent, EditView* view);
	void columnEditor(QWidget* parent, EditView* view);
	void hashGenerate(QWidget* parent, QCryptographicHash::Algorithm algo, const QString& name);
	void hashFromFiles(QWidget* parent, QCryptographicHash::Algorithm algo, const QString& name);
	void preferences(MainWindow* mw);
	void styleConfigurator(MainWindow* mw);
	void shortcutMapper(MainWindow* mw);
	void windowsList(MainWindow* mw);
	// -1: cancelled, 0: until end of file, n: times; macroIndex: the macro chosen in names
	int macroRunMultiple(QWidget* parent, const QStringList& names, int* macroIndex);

	// Search > Find characters in range... (WinControls/FindCharsInRange), modeless
	void findCharsInRange(MainWindow* mw);
	bool findCharInRange(EditView* view, unsigned char beginRange, unsigned char endRange, long long startPos, bool down, bool wrap);
}
