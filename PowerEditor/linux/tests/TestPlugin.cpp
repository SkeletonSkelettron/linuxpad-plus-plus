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

// Plugin of the self test (not installed): records the notifications it gets and
// answers NPPM_MSGTOPLUGIN with them, registers a docked panel and a toolbar icon.

#include <map>

#include <QLabel>
#include <QStyle>

#include "PluginInterface.h"
#include "Docking.h"
#include "NppPluginQt.h"

#include <wininet.h>

#include "TestPlugin.h"

namespace
{
	NppData nppData;
	FuncItem funcItems[5];
	ShortcutKey aboutKey{true, true, true, 'T'};
	TestPluginReport report;
	QLabel* panel = nullptr;
	bool panelShown = false;
	tTbData dockData;
	toolbarIconsWithDarkMode icons;

	void commandA()
	{
		++report.commandA;
		// a command that edits the current document through Scintilla
		int view = 0;
		::SendMessage(nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, reinterpret_cast<LPARAM>(&view));
		HWND sci = view == 0 ? nppData._scintillaMainHandle : nppData._scintillaSecondHandle;
		::SendMessage(sci, SCI_APPENDTEXT, 7, reinterpret_cast<LPARAM>("plugin!"));
		::SendMessage(nppData._nppHandle, NPPM_SETMENUITEMCHECK, funcItems[0]._cmdID, report.commandA % 2);
	}

	void showPanel()
	{
		if (!panel)
		{
			panel = new QLabel(QStringLiteral("Test plugin panel"));
			panel->setObjectName(QStringLiteral("testPluginPanel"));
			dockData.hClient = nppHandleOf(panel);
			dockData.pszName = L"Test Panel";
			dockData.dlgID = 2;
			dockData.uMask = DWS_DF_CONT_RIGHT | DWS_ADDINFO;
			dockData.pszAddInfo = L"info";
			dockData.pszModuleName = L"NppTestPlugin.so";
			::SendMessage(nppData._nppHandle, NPPM_DMMREGASDCKDLG, 0, reinterpret_cast<LPARAM>(&dockData));
		}
		panelShown = !panelShown;
		::SendMessage(nppData._nppHandle, panelShown ? NPPM_DMMSHOW : NPPM_DMMHIDE, 0, reinterpret_cast<LPARAM>(dockData.hClient));
		::SendMessage(nppData._nppHandle, NPPM_SETMENUITEMCHECK, funcItems[2]._cmdID, panelShown);
	}

	void about()
	{
		++report.about;
	}
}

extern "C" __declspec(dllexport) void setInfo(NppData data)
{
	nppData = data;
	wcscpy(funcItems[0]._itemName, L"Command A");
	funcItems[0]._pFunc = commandA;
	wcscpy(funcItems[1]._itemName, L"-SEPARATOR-");
	wcscpy(funcItems[2]._itemName, L"Show Panel");
	funcItems[2]._pFunc = showPanel;
	wcscpy(funcItems[3]._itemName, L"Checked at start");
	funcItems[3]._pFunc = about;
	funcItems[3]._init2Check = true;
	wcscpy(funcItems[4]._itemName, L"About");
	funcItems[4]._pFunc = about;
	funcItems[4]._pShKey = &aboutKey;
}

extern "C" __declspec(dllexport) const wchar_t* getName()
{
	return L"Test Plugin";
}

extern "C" __declspec(dllexport) FuncItem* getFuncsArray(int* nb)
{
	*nb = 5;
	return funcItems;
}

extern "C" __declspec(dllexport) void beNotified(SCNotification* scn)
{
	const unsigned int code = scn->nmhdr.code;
	if (code >= NPPN_FIRST && code < NPPN_FIRST + 64)
	{
		++report.nppn[code - NPPN_FIRST];
		report.lastIdFrom[code - NPPN_FIRST] = scn->nmhdr.idFrom;
		if (scn->nmhdr.hwndFrom != nppData._nppHandle && code != NPPN_READONLYCHANGED && code != NPPN_DOCORDERCHANGED
			&& code != NPPN_GLOBALMODIFIED && code != NPPN_SHORTCUTREMAPPED)
			++report.badHwndFrom;
	}
	else if (code == SCN_MODIFIED)
	{
		++report.scnModified;
		if (scn->nmhdr.hwndFrom != nppData._scintillaMainHandle && scn->nmhdr.hwndFrom != nppData._scintillaSecondHandle)
			++report.badHwndFrom;
	}
	else if (code == SCN_UPDATEUI)
	{
		++report.scnUpdateUi;
	}
	if (code == NPPN_TBMODIFICATION)
	{
		icons.hToolbarIcon = LoadIcon(nullptr, IDI_INFORMATION);
		icons.hToolbarIconDarkMode = nppIcon(QApplication::style()->standardIcon(QStyle::SP_FileIcon));
		icons.hToolbarBmp = icons.hToolbarIcon;
		::SendMessage(nppData._nppHandle, NPPM_ADDTOOLBARICON_FORDARKMODE, funcItems[0]._cmdID, reinterpret_cast<LPARAM>(&icons));
	}
}

extern "C" __declspec(dllexport) LRESULT messageProc(UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (msg == NPPM_MSGTOPLUGIN)
	{
		auto* info = reinterpret_cast<CommunicationInfo*>(lParam);
		if (info && info->internalMsg == TestPluginReport::kGetReport && info->info)
			*static_cast<TestPluginReport*>(info->info) = report;
		if (info && info->internalMsg == TestPluginInet::kGet && info->info)
		{
			// the WinInet of the SDK, like a plugin that downloads something
			auto* get = static_cast<TestPluginInet*>(info->info);
			HINTERNET session = InternetOpen(L"NppTestPlugin", INTERNET_OPEN_TYPE_DIRECT, nullptr, nullptr, 0);
			HINTERNET url = session ? InternetOpenUrl(session, get->url, nullptr, 0, INTERNET_FLAG_RELOAD, 0) : nullptr;
			get->opened = url != nullptr;
			if (url)
			{
				DWORD value = 0;
				DWORD size = sizeof(value);
				if (HttpQueryInfo(url, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER, &value, &size, nullptr))
					get->status = value;
				size = sizeof(value);
				if (HttpQueryInfo(url, HTTP_QUERY_CONTENT_LENGTH | HTTP_QUERY_FLAG_NUMBER, &value, &size, nullptr))
					get->contentLength = value;
				DWORD read = 0;
				size_t at = 0;
				while (at < sizeof(get->body) - 1 && InternetReadFile(url, get->body + at, static_cast<DWORD>(sizeof(get->body) - 1 - at), &read) && read)
					at += read;
				InternetCloseHandle(url);
			}
			if (session)
				InternetCloseHandle(session);
		}
		return TRUE;
	}
	if (msg == WM_COMMAND)
		++report.dynamicCommands;
	return TRUE;
}

extern "C" __declspec(dllexport) BOOL isUnicode()
{
	return TRUE;
}
