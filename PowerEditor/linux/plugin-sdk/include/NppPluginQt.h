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

// Notepad++ Linux plugin SDK: Qt helpers for the user interface of plugins, in
// place of the Win32 dialogs.
//
//   nppMainWindow()          the main window: parent of the dialogs of a plugin
//   nppWidget(hwnd)          the QWidget of a handle (Scintilla views, main window)
//   nppHandleOf(widget)      a handle for a widget of the plugin, e.g. hClient of
//                            NPPM_DMMREGASDCKDLG (the widget goes in a QDockWidget)
//   nppDockOf(widget)        that QDockWidget, to follow its visibility (DMN_CLOSE)
//   nppMenu(hmenu)           the QMenu of NPPM_GETMENUHANDLE
//   nppIcon(QIcon)           an HICON for NPPM_ADDTOOLBARICON_FORDARKMODE / tTbData

#pragma once

#include <string>

#include <windows.h>

#include <QApplication>
#include <QDockWidget>
#include <QIcon>
#include <QMenu>
#include <QString>
#include <QVariant>
#include <QWidget>

inline QWidget* nppWidget(HWND h)
{
	return (h && h->magic == NPP_HANDLE_MAGIC) ? static_cast<QWidget*>(h->widget) : nullptr;
}

inline HWND nppMainHandle()
{
	for (QWidget* w : QApplication::topLevelWidgets())
	{
		const QVariant v = w->property("_nppMainHandle");
		if (v.isValid())
			return static_cast<HWND>(v.value<void*>());
	}
	return nullptr;
}

inline QWidget* nppMainWindow()
{
	return nppWidget(nppMainHandle());
}

inline HWND nppHandleOf(QWidget* w)
{
	if (!w)
		return nullptr;
	const QVariant v = w->property("_nppHandle");
	if (v.isValid())
		return static_cast<HWND>(v.value<void*>());
	auto* h = new NppHandle;
	h->kind = NPP_HANDLE_WIDGET;
	h->widget = w;
	h->owner = nppMainHandle();
	w->setProperty("_nppHandle", QVariant::fromValue(static_cast<void*>(h)));
	QObject::connect(w, &QObject::destroyed, [h] { delete h; });
	return h;
}

inline QDockWidget* nppDockOf(QWidget* client)
{
	for (QWidget* p = client ? client->parentWidget() : nullptr; p; p = p->parentWidget())
	{
		if (auto* dock = qobject_cast<QDockWidget*>(p))
			return dock;
	}
	return nullptr;
}

inline QMenu* nppMenu(HMENU m)
{
	return qobject_cast<QMenu*>(nppWidget(m));
}

// the icon lives as long as the plugin: give it to the host once
inline HICON nppIcon(const QIcon& icon)
{
	return new QIcon(icon);
}

inline QString nppString(const wchar_t* s)
{
	return s ? QString::fromWCharArray(s) : QString();
}

inline std::wstring nppWString(const QString& s)
{
	return s.toStdWString();
}
