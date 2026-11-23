//this file is part of notepad++
//Copyright (C)2022 Don HO <don.h@free.fr>
//
//This program is free software; you can redistribute it and/or
//modify it under the terms of the GNU General Public License
//as published by the Free Software Foundation; either
//version 2 of the License, or (at your option) any later version.
//
//This program is distributed in the hope that it will be useful,
//but WITHOUT ANY WARRANTY; without even the implied warranty of
//MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//GNU General Public License for more details.
//
//You should have received a copy of the GNU General Public License
//along with this program; if not, write to the Free Software
//Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

#include "PluginDefinition.h"
#include "menuCmdID.h"
#include <string>
#include <regex>
#include <map>
#include "Scintilla.h"
#include "resource.h"

#include <algorithm>
#include <string_view>
#include <QHostAddress>

static HINSTANCE pluginInstance;

// Global variable to store the text state before stripping
static std::map<UINT_PTR, std::string> previousTextStates;

void forgetPreviousText(UINT_PTR buffer)
{
    previousTextStates.erase(buffer);
}

// Global variables for custom IPv4 settings
bool replaceOctet[4] = {true, true, true, true};
std::string replaceStr[4] = {"qqq", "xxx", "yyy", "zzz"};

bool replaceIpv6 = true;
std::string ipv6ReplaceStr = "2001:db8:0:0:0:0:0:1";

bool replaceHost = true;
std::string hostReplaceStr = "hostname.com";

FuncItem funcItem[nbFunc];

NppData nppData;

//
// Initialize your plugin data here
// It will be called while plugin loading   
void pluginInit(HANDLE hModule)
{
    pluginInstance = static_cast<HINSTANCE>(hModule);
}

//
// Here you can do the clean up, save the parameters (if any) for the next session
//
void pluginCleanUp()
{
	previousTextStates.clear();
}

static std::string replaceIpv6Addresses(const std::string& text, const std::string& replacement)
{
	static const std::regex candidates("[0-9A-Fa-f:.]+");
	std::string result;
	size_t copied = 0;
	for (std::sregex_iterator match(text.begin(), text.end(), candidates), end; match != end; ++match)
	{
		const size_t start = match->position();
		const size_t length = match->length();
		std::string_view address(text.data() + start, length);
		while (!address.empty() && address.back() == '.')
			address.remove_suffix(1);
		if (address.size() <= 2 || std::count(address.begin(), address.end(), ':') < 2)
			continue;
		const QHostAddress ip(QString::fromLatin1(address.data(), address.size()));
		if (ip.protocol() != QAbstractSocket::IPv6Protocol)
			continue;
		result.append(text, copied, start - copied);
		result += replacement;
		copied = start + address.size();
	}
	result.append(text, copied, std::string::npos);
	return result;
}

void StripIPsAndHostnames() {
    // 1. Change cursor to hourglass and update the Notepad++ status bar
    HCURSOR hWaitCursor = LoadCursor(NULL, IDC_WAIT);
    HCURSOR hOldCursor = SetCursor(hWaitCursor);
    ::SendMessage(nppData._nppHandle, NPPM_SETSTATUSBAR, STATUSBAR_DOC_TYPE, (LPARAM)TEXT("Stripping IPs and Hostnames. Please wait..."));

    // 2. Get the handle to the current active Scintilla editor
    int currentEdit = 0;
    ::SendMessage(nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, (LPARAM)&currentEdit);
    HWND curScintilla = (currentEdit == 0) ? nppData._scintillaMainHandle : nppData._scintillaSecondHandle;

    // 3. Get the length of the text in the editor
    const auto textLength = ::SendMessage(curScintilla, SCI_GETLENGTH, 0, 0);
    if (textLength <= 0) {
        SetCursor(hOldCursor); // Restore cursor if we exit early
        ::SendMessage(nppData._nppHandle, NPPM_SETSTATUSBAR, STATUSBAR_DOC_TYPE, (LPARAM)TEXT("Ready"));
        return;
    }

    // 4. Retrieve the text into a buffer
    std::string textContent(static_cast<size_t>(textLength) + 1, '\0');
    ::SendMessage(curScintilla, SCI_GETTEXT, textContent.size(), reinterpret_cast<LPARAM>(textContent.data()));
    textContent.pop_back();

    // 6. Safely try to run the Regular Expressions
    try {
        std::regex hostRegex(R"(\b(?:[a-zA-Z0-9-]+\.)+[a-zA-Z]{2,6}\b)");
        std::regex ipv4Regex(R"(\b(\d{1,3})\.(\d{1,3})\.(\d{1,3})\.(\d{1,3})\b)");

        // Build the dynamic IPv4 format string based on your UI Settings
        std::string ipv4Format = "";
        ipv4Format += replaceOctet[0] ? replaceStr[0] : "$1";
        ipv4Format += ".";
        ipv4Format += replaceOctet[1] ? replaceStr[1] : "$2";
        ipv4Format += ".";
        ipv4Format += replaceOctet[2] ? replaceStr[2] : "$3";
        ipv4Format += ".";
        ipv4Format += replaceOctet[3] ? replaceStr[3] : "$4";

        // Execute Replacements
        std::string strippedText = textContent;

        // If the Hostname box is checked, replace it with your custom string
        if (replaceHost) {
            strippedText = std::regex_replace(strippedText, hostRegex, hostReplaceStr);
        }

		if (replaceIpv6)
			strippedText = replaceIpv6Addresses(strippedText, ipv6ReplaceStr);

		strippedText = std::regex_replace(strippedText, ipv4Regex, ipv4Format);

        // 7. Check if anything actually changed
        if (strippedText == textContent) {
            SetCursor(hOldCursor); // Restore cursor before popup
            ::SendMessage(nppData._nppHandle, NPPM_SETSTATUSBAR, STATUSBAR_DOC_TYPE, (LPARAM)TEXT("Ready"));
            ::MessageBox(NULL, TEXT("No IPs or Hostnames were found in this file!"), TEXT("Strip Complete"), MB_OK | MB_ICONINFORMATION);
            return;
        }

        // 8. Push the modified text back into the editor
        const auto buffer = static_cast<UINT_PTR>(::SendMessage(nppData._nppHandle, NPPM_GETCURRENTBUFFERID, 0, 0));
        previousTextStates[buffer] = std::move(textContent);
        ::SendMessage(curScintilla, SCI_SETTARGETRANGE, 0, textLength);
        ::SendMessage(curScintilla, SCI_REPLACETARGET, strippedText.size(), reinterpret_cast<LPARAM>(strippedText.data()));

        // Show success in the bottom status bar
        ::SendMessage(nppData._nppHandle, NPPM_SETSTATUSBAR, STATUSBAR_DOC_TYPE, (LPARAM)TEXT("Stripping Complete!"));

    }
    catch (const std::exception& e) {
        // If it fails, pop up an error box
        ::MessageBoxA(NULL, e.what(), "Regex Error", MB_OK | MB_ICONERROR);
        ::SendMessage(nppData._nppHandle, NPPM_SETSTATUSBAR, STATUSBAR_DOC_TYPE, (LPARAM)TEXT("Stripping Failed!"));
    }

    // 9. Always restore the normal mouse cursor at the very end
    SetCursor(hOldCursor);
}

void UndoLastStrip() {
    // 1. Get the handle to the current active Scintilla editor
    int currentEdit = 0;
    ::SendMessage(nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, (LPARAM)&currentEdit);
    HWND curScintilla = (currentEdit == 0) ? nppData._scintillaMainHandle : nppData._scintillaSecondHandle;

    // 2. Get the unique internal ID of the tab we are currently looking at
    const auto buffer = static_cast<UINT_PTR>(::SendMessage(nppData._nppHandle, NPPM_GETCURRENTBUFFERID, 0, 0));

    // 3. Check our dictionary to see if we have a backup for THIS specific file
    if (const auto previous = previousTextStates.find(buffer); previous != previousTextStates.end()) {

        // Push the backup text back into the editor
        ::SendMessage(curScintilla, SCI_SETTARGETRANGE, 0, ::SendMessage(curScintilla, SCI_GETLENGTH, 0, 0));
        ::SendMessage(curScintilla, SCI_REPLACETARGET, previous->second.size(), reinterpret_cast<LPARAM>(previous->second.data()));

        // Remove it from the dictionary so we don't undo multiple times to the same state
        previousTextStates.erase(previous);

        ::SendMessage(nppData._nppHandle, NPPM_SETSTATUSBAR, STATUSBAR_DOC_TYPE, (LPARAM)TEXT("Undo Successful!"));
    }
    else {
        // If there is no backup for this file, tell the user gracefully!
        ::MessageBox(NULL, TEXT("There is no previous strip action to undo for this specific file!"), TEXT("Cannot Undo"), MB_OK | MB_ICONWARNING);
    }
}

INT_PTR CALLBACK SettingsDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM /*lParam*/) {
    switch (uMsg) {
    case WM_INITDIALOG: {
        // Load IPv4 (Your existing code)
        CheckDlgButton(hwndDlg, IDC_CHK_OCT1, replaceOctet[0] ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwndDlg, IDC_CHK_OCT2, replaceOctet[1] ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwndDlg, IDC_CHK_OCT3, replaceOctet[2] ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwndDlg, IDC_CHK_OCT4, replaceOctet[3] ? BST_CHECKED : BST_UNCHECKED);
        SetDlgItemTextA(hwndDlg, IDC_EDIT_OCT1, replaceStr[0].c_str());
        SetDlgItemTextA(hwndDlg, IDC_EDIT_OCT2, replaceStr[1].c_str());
        SetDlgItemTextA(hwndDlg, IDC_EDIT_OCT3, replaceStr[2].c_str());
        SetDlgItemTextA(hwndDlg, IDC_EDIT_OCT4, replaceStr[3].c_str());

        // --- NEW: Load IPv6 and Hostnames ---
        CheckDlgButton(hwndDlg, IDC_CHK_IPV6, replaceIpv6 ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hwndDlg, IDC_CHK_HOST, replaceHost ? BST_CHECKED : BST_UNCHECKED);
        SetDlgItemTextA(hwndDlg, IDC_EDIT_IPV6, ipv6ReplaceStr.c_str());
        SetDlgItemTextA(hwndDlg, IDC_EDIT_HOST, hostReplaceStr.c_str());

        return TRUE;
    }
    case WM_COMMAND: {
        switch (LOWORD(wParam)) {
        case IDOK: {
            char buffer[256];
            // Save IPv4 (Your existing code)
            replaceOctet[0] = (IsDlgButtonChecked(hwndDlg, IDC_CHK_OCT1) == BST_CHECKED);
            replaceOctet[1] = (IsDlgButtonChecked(hwndDlg, IDC_CHK_OCT2) == BST_CHECKED);
            replaceOctet[2] = (IsDlgButtonChecked(hwndDlg, IDC_CHK_OCT3) == BST_CHECKED);
            replaceOctet[3] = (IsDlgButtonChecked(hwndDlg, IDC_CHK_OCT4) == BST_CHECKED);
            GetDlgItemTextA(hwndDlg, IDC_EDIT_OCT1, buffer, 256); replaceStr[0] = buffer;
            GetDlgItemTextA(hwndDlg, IDC_EDIT_OCT2, buffer, 256); replaceStr[1] = buffer;
            GetDlgItemTextA(hwndDlg, IDC_EDIT_OCT3, buffer, 256); replaceStr[2] = buffer;
            GetDlgItemTextA(hwndDlg, IDC_EDIT_OCT4, buffer, 256); replaceStr[3] = buffer;

            // --- NEW: Save IPv6 and Hostnames ---
            replaceIpv6 = (IsDlgButtonChecked(hwndDlg, IDC_CHK_IPV6) == BST_CHECKED);
            replaceHost = (IsDlgButtonChecked(hwndDlg, IDC_CHK_HOST) == BST_CHECKED);
            GetDlgItemTextA(hwndDlg, IDC_EDIT_IPV6, buffer, 256); ipv6ReplaceStr = buffer;
            GetDlgItemTextA(hwndDlg, IDC_EDIT_HOST, buffer, 256); hostReplaceStr = buffer;

            EndDialog(hwndDlg, IDOK);
            return TRUE;
        }

        case IDCANCEL: {
            // User clicked Cancel, just close without saving
            EndDialog(hwndDlg, IDCANCEL);
            return TRUE;
        }
        }
        break;
    }
    }
    return FALSE;
}

void OpenReplaceSettings() {
    // Launch the custom dialog window using our new dynamic Handle!
    DialogBoxParam(pluginInstance, MAKEINTRESOURCE(IDD_SETTINGS_DLG), nppData._nppHandle, SettingsDlgProc, 0);
}

void commandMenuInit()
{
    // Index 0: The main stripping tool
    setCommand(0, TEXT("Strip IPs and Hostnames"), StripIPsAndHostnames, NULL, false);

    // Index 1: The undo tool
    setCommand(1, TEXT("Undo Last Strip"), UndoLastStrip, NULL, false);

    // Index 2: The settings window
    setCommand(2, TEXT("Replace Settings..."), OpenReplaceSettings, NULL, false);
}

void commandMenuCleanUp()
{
	// Don't forget to deallocate your shortcut here
}

bool setCommand(size_t index, TCHAR *cmdName, PFUNCPLUGINCMD pFunc, ShortcutKey *sk, bool check0nInit) 
{
    if (index >= nbFunc)
        return false;

    if (!pFunc)
        return false;

    lstrcpy(funcItem[index]._itemName, cmdName);
    funcItem[index]._pFunc = pFunc;
    funcItem[index]._init2Check = check0nInit;
    funcItem[index]._pShKey = sk;

    return true;
}
