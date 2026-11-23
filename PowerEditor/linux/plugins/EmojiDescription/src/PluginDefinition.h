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

#ifndef PLUGINDEFINITION_H
#define PLUGINDEFINITION_H

//
// All difinitions of plugin interface
//
#include "PluginInterface.h"

//-------------------------------------//
//-- STEP 1. DEFINE YOUR PLUGIN NAME --//
//-------------------------------------//
// Here define your plugin name
//
const TCHAR NPP_PLUGIN_NAME[] = TEXT("Emoji Description");

//-----------------------------------------------//
//-- STEP 2. DEFINE YOUR PLUGIN COMMAND NUMBER --//
//-----------------------------------------------//
//
// Here define the number of your plugin commands
//
const int nbFunc = 11;

//
// Which information fields are shown in the status bar.
// Toggled individually from the plugin menu and persisted to the plugin config.
//
// The fields are organized along two axes, and the struct is declared in display
// order. First the code point (what character this is), then the same UTF-8 byte
// sequence in three bases (how it is stored), and finally the character
// description -- always last, because it is the only variable-length field.
//
// For "Т": U+0422 | (html) &#1058; | (hex) 0xD0 0xA2 | (dec) 208 162 |
//          (oct) \320\242 | CYRILLIC CAPITAL LETTER TE
//
struct DisplayFields
{
    bool codePoint;   // U+0422           -- code point, no tag (U+ is the tag)
    bool html;        // (html) &#1058;   -- HTML entity, code point in decimal
    bool hex;         // (hex) 0xD0 0xA2  -- UTF-8 bytes, hexadecimal
    bool decimal;     // (dec) 208 162    -- UTF-8 bytes, decimal
    bool octal;       // (oct) \320\242   -- UTF-8 bytes, octal (git notation)
    bool name;        // CYRILLIC CAPITAL LETTER TE -- description, no tag, last
};


//
// Initialization of your plugin data
// It will be called while plugin loading
//
void pluginInit(HANDLE hModule);

//
// Cleaning of your plugin
// It will be called while plugin unloading
//
void pluginCleanUp();

//
//Initialization of your plugin commands
//
void commandMenuInit();

//
//Clean up your plugin commands allocation (if any)
//
void commandMenuCleanUp();

//
// Function which sets your command
//
bool setCommand(size_t index, TCHAR *cmdName, PFUNCPLUGINCMD pFunc, ShortcutKey *sk = NULL, bool check0nInit = false);


//
// Your plugin command functions
//
void toggleShowCharInfo();
void copyCharacterDetails();
void toggleFieldCodePoint();
void toggleFieldHtml();
void toggleFieldHex();
void toggleFieldDecimal();
void toggleFieldOctal();
void toggleFieldName();
void aboutDialog();

//
// Notification handler
//
void pluginBeNotified(SCNotification *notifyCode);

//
// Message handler
//
LRESULT pluginMessageProc(UINT Message, WPARAM wParam, LPARAM lParam);

//
// Helper functions
//
void updateCharacterInfo();
uint32_t decodeUtf8Char(const unsigned char* text, int& bytesRead);
void formatCharacterCodes(uint32_t codepoint, TCHAR* output, size_t outputSize);
bool getUnicodeName(uint32_t codepoint, TCHAR* output, size_t outputSize);

//
// Configuration persistence (plugin config dir .ini)
//
void loadConfig();
void saveConfig();

#endif //PLUGINDEFINITION_H
