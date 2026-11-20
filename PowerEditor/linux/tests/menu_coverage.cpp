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

// Lists the menu commands (generated from Notepad_plus.rc) that have no
// implementation in the Linux port yet, and the coverage per top-level menu.
//
//   QT_QPA_PLATFORM=offscreen ./build-linux/bin/npp_menu_coverage

#include <cstdio>

#include <QApplication>
#include <QMap>
#include <QMenu>
#include <QMenuBar>
#include <QTemporaryDir>

#include "MainWindow.h"

struct MainWindowAccess
{
	static bool implemented(const MainWindow& w, int id) { return w._handlers.contains(id); }
	static int idOf(const MainWindow& w, QAction* a) { return w._actions.key(a, 0); }
};

namespace
{
	struct Count
	{
		int total = 0;
		int done = 0;
	};

	void walk(const MainWindow& w, QMenu* menu, const QString& path, Count& count, QStringList& missing)
	{
		for (QAction* a : menu->actions())
		{
			if (a->isSeparator())
				continue;
			const QString text = a->text().remove(QLatin1Char('&')).section(QLatin1Char('\t'), 0, 0);
			if (a->menu())
			{
				walk(w, a->menu(), path + QStringLiteral(" > ") + text, count, missing);
				continue;
			}
			const int id = MainWindowAccess::idOf(w, a);
			if (!id)
				continue; // recent files, languages added at run time, ...
			++count.total;
			if (MainWindowAccess::implemented(w, id))
				++count.done;
			else
				missing << path + QStringLiteral(" > ") + text;
		}
	}
}

int main(int argc, char** argv)
{
	QTemporaryDir config;
	qputenv("XDG_CONFIG_HOME", config.path().toLocal8Bit());
	QApplication app(argc, argv);
	auto* w = new MainWindow;

	QMap<QString, Count> perMenu;
	QStringList missing;
	Count all;
	for (QAction* a : w->menuBar()->actions())
	{
		if (!a->menu())
			continue;
		const QString top = a->text().remove(QLatin1Char('&'));
		Count c;
		walk(*w, a->menu(), top, c, missing);
		perMenu[top] = c;
		all.total += c.total;
		all.done += c.done;
	}

	std::printf("Implemented menu commands: %d of %d\n\n", all.done, all.total);
	for (auto it = perMenu.constBegin(); it != perMenu.constEnd(); ++it)
		std::printf("  %-10s %3d / %3d\n", qPrintable(it.key()), it.value().done, it.value().total);
	std::printf("\nNot implemented:\n");
	for (const QString& m : missing)
		std::printf("  %s\n", qPrintable(m));
	std::fflush(stdout);
	std::_Exit(0);
}
