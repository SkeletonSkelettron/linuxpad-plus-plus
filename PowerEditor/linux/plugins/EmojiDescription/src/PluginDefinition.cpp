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
#include "UnicodeNames.h"
#include "CharDetailsDialog.h"
#include <windows.h>
#include <tchar.h>
#include <shlwapi.h>
#include <string>
#include <sstream>
#include <iomanip>

//
// The plugin data that Notepad++ needs
//
FuncItem funcItem[nbFunc];

//
// The data of Notepad++ that you can use in your plugin commands
//
NppData nppData;

//
// Plugin state
//
static bool g_showCharInfo = true;

// Which fields are shown by default: code point, HTML entity and the character
// description; the three byte-sequence fields (hex / dec / oct) are off. The
// description is served from the embedded Unicode table (see UnicodeNames.cpp),
// so it always resolves and is shown by default.
// Order: { codePoint, html, hex, decimal, octal, name } -- display order.
static DisplayFields g_fields = { true, true, false, false, false, true };

// Full path to the plugin .ini, resolved once from the Notepad++ config dir.
static TCHAR g_iniPath[MAX_PATH] = { 0 };

// This DLL's own instance handle, needed to load the dialog resource.
static HINSTANCE g_hInst = NULL;

//
// Initialize your plugin data here
// It will be called while plugin loading
void pluginInit(HANDLE hModule)
{
    g_hInst = (HINSTANCE)hModule;
}

//
// Here you can do the clean up, save the parameters (if any) for the next session
//
void pluginCleanUp()
{
}

//
// Insert a plugin-menu separator (defined below).
static bool setSeparator(size_t index);

// Initialization of your plugin commands
// You should fill your plugins commands here
void commandMenuInit()
{
    // Restore persisted settings before the menu check states are initialized.
    loadConfig();

    // The item labels double as the documentation for each field: the plugin API
    // has no way to attach a tooltip to a menu entry (FuncItem carries only a
    // name, a callback and a shortcut), so each label carries a sample value for
    // "Т" instead. The HTML sample doubles its '&' -- Win32 menu strings read a
    // single one as a mnemonic prefix, so "&&#1058;" is what renders "&#1058;".
    setCommand(0,  TEXT("Show Character Info"),                       toggleShowCharInfo,   NULL, g_showCharInfo);
    setSeparator(1);   // the action below is neither the master switch nor a field option,
    setCommand(2,  TEXT("Copy character details..."),                 copyCharacterDetails, NULL, false);
    setSeparator(3);   // so it is fenced off from both groups
    setCommand(4,  TEXT("Show: code point  (U+0422)"),                toggleFieldCodePoint, NULL, g_fields.codePoint);
    setCommand(5,  TEXT("Show: (html) HTML entity  (&&#1058;)"),      toggleFieldHtml,      NULL, g_fields.html);
    setCommand(6,  TEXT("Show: (hex) UTF-8 bytes  (0xD0 0xA2)"),      toggleFieldHex,       NULL, g_fields.hex);
    setCommand(7,  TEXT("Show: (dec) UTF-8 bytes  (208 162)"),        toggleFieldDecimal,   NULL, g_fields.decimal);
    setCommand(8,  TEXT("Show: (oct) UTF-8 bytes, git  (\\320\\242)"), toggleFieldOctal,    NULL, g_fields.octal);
    setCommand(9,  TEXT("Show: description  (CYRILLIC CAPITAL ...)"), toggleFieldName,      NULL, g_fields.name);
    setCommand(10, TEXT("About"),                                     aboutDialog,          NULL, false);
}

//
// Resolve the .ini path inside the Notepad++ plugin config directory.
//
static void initConfigPath()
{
    TCHAR configDir[MAX_PATH] = { 0 };
    ::SendMessage(nppData._nppHandle, NPPM_GETPLUGINSCONFIGDIR, MAX_PATH, (LPARAM)configDir);
    ::PathAppend(configDir, TEXT("EmojiDescription.ini"));
    _tcsncpy_s(g_iniPath, MAX_PATH, configDir, _TRUNCATE);
}

//
// Load persisted settings (falling back to defaults on first run).
//
void loadConfig()
{
    initConfigPath();
    g_showCharInfo     = ::GetPrivateProfileInt(TEXT("Display"), TEXT("Enabled"),   1, g_iniPath) != 0;
    g_fields.codePoint = ::GetPrivateProfileInt(TEXT("Fields"),  TEXT("CodePoint"), 1, g_iniPath) != 0;
    g_fields.html      = ::GetPrivateProfileInt(TEXT("Fields"),  TEXT("Html"),      1, g_iniPath) != 0;
    g_fields.hex       = ::GetPrivateProfileInt(TEXT("Fields"),  TEXT("Hex"),       0, g_iniPath) != 0;
    g_fields.decimal   = ::GetPrivateProfileInt(TEXT("Fields"),  TEXT("Decimal"),   0, g_iniPath) != 0;
    g_fields.octal     = ::GetPrivateProfileInt(TEXT("Fields"),  TEXT("Octal"),     0, g_iniPath) != 0;
    g_fields.name      = ::GetPrivateProfileInt(TEXT("Fields"),  TEXT("Name"),      1, g_iniPath) != 0;

    // Migrate configs written before "Hex" changed meaning. It used to hold the
    // code point in hex (redundant with U+XXXX) while the UTF-8 bytes lived in a
    // separate "Utf8" key; now "Hex" *is* the byte sequence. Anyone who had the
    // byte view on keeps it. The stale key is dropped by the next saveConfig().
    if (::GetPrivateProfileInt(TEXT("Fields"), TEXT("Utf8"), 0, g_iniPath) != 0)
        g_fields.hex = true;
}

//
// Persist the current settings.
//
void saveConfig()
{
    if (!g_iniPath[0])
        return;

    ::WritePrivateProfileString(TEXT("Display"), TEXT("Enabled"),   g_showCharInfo     ? TEXT("1") : TEXT("0"), g_iniPath);
    ::WritePrivateProfileString(TEXT("Fields"),  TEXT("CodePoint"), g_fields.codePoint ? TEXT("1") : TEXT("0"), g_iniPath);
    ::WritePrivateProfileString(TEXT("Fields"),  TEXT("Html"),      g_fields.html      ? TEXT("1") : TEXT("0"), g_iniPath);
    ::WritePrivateProfileString(TEXT("Fields"),  TEXT("Hex"),       g_fields.hex       ? TEXT("1") : TEXT("0"), g_iniPath);
    ::WritePrivateProfileString(TEXT("Fields"),  TEXT("Decimal"),   g_fields.decimal   ? TEXT("1") : TEXT("0"), g_iniPath);
    ::WritePrivateProfileString(TEXT("Fields"),  TEXT("Octal"),     g_fields.octal     ? TEXT("1") : TEXT("0"), g_iniPath);
    ::WritePrivateProfileString(TEXT("Fields"),  TEXT("Name"),      g_fields.name      ? TEXT("1") : TEXT("0"), g_iniPath);

    // Retire the pre-0.4 key now that "Hex" carries the byte sequence.
    ::WritePrivateProfileString(TEXT("Fields"),  TEXT("Utf8"),      NULL, g_iniPath);
}

//
// Here you can do the clean up (especially for the shortcut)
//
void commandMenuCleanUp()
{
}

//
// This function help you to initialize your plugin commands
//
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

//
// Insert a separator line in the plugin menu at the given index. Notepad++
// renders a FuncItem with a NULL function pointer as a menu separator.
//
static bool setSeparator(size_t index)
{
    if (index >= nbFunc)
        return false;

    funcItem[index]._itemName[0] = TEXT('\0');
    funcItem[index]._pFunc = NULL;
    funcItem[index]._init2Check = false;
    funcItem[index]._pShKey = NULL;

    return true;
}

//
// Decode UTF-8 character at given position
//
uint32_t decodeUtf8Char(const unsigned char* text, int& bytesRead)
{
    bytesRead = 0;
    if (!text || !text[0])
        return 0;

    unsigned char firstByte = text[0];

    // 1-byte character (ASCII)
    if ((firstByte & 0x80) == 0) {
        bytesRead = 1;
        return firstByte;
    }

    // 2-byte character
    if ((firstByte & 0xE0) == 0xC0) {
        if ((text[1] & 0xC0) != 0x80)
            return 0;
        bytesRead = 2;
        return ((firstByte & 0x1F) << 6) | (text[1] & 0x3F);
    }

    // 3-byte character
    if ((firstByte & 0xF0) == 0xE0) {
        if ((text[1] & 0xC0) != 0x80 || (text[2] & 0xC0) != 0x80)
            return 0;
        bytesRead = 3;
        return ((firstByte & 0x0F) << 12) | ((text[1] & 0x3F) << 6) | (text[2] & 0x3F);
    }

    // 4-byte character (includes emoji)
    if ((firstByte & 0xF8) == 0xF0) {
        if ((text[1] & 0xC0) != 0x80 || (text[2] & 0xC0) != 0x80 || (text[3] & 0xC0) != 0x80)
            return 0;
        bytesRead = 4;
        return ((firstByte & 0x07) << 18) | ((text[1] & 0x3F) << 12) |
               ((text[2] & 0x3F) << 6) | (text[3] & 0x3F);
    }

    return 0;
}

//
// Look up the Unicode character name (e.g. "EM DASH", "GRINNING FACE") for a
// code point. Backed by the name table embedded in the DLL (see UnicodeNames.h
// / UnicodeNames.cpp), so it covers the entire Unicode code space -- including
// supplementary planes and emoji -- with no dependency on the OS. Returns false
// when the code point has no formal Unicode name.
//
bool getUnicodeName(uint32_t codepoint, TCHAR* output, size_t outputSize)
{
    return lookupUnicodeName(codepoint, output, outputSize);
}

//
// Encode a code point back into UTF-8. Returns the number of bytes written to
// `bytes` (1-4).
//
static int encodeUtf8(uint32_t codepoint, unsigned char bytes[4])
{
    int count;

    if (codepoint < 0x80) {
        bytes[0] = static_cast<unsigned char>(codepoint);
        count = 1;
    }
    else if (codepoint < 0x800) {
        bytes[0] = static_cast<unsigned char>(0xC0 | (codepoint >> 6));
        bytes[1] = static_cast<unsigned char>(0x80 | (codepoint & 0x3F));
        count = 2;
    }
    else if (codepoint < 0x10000) {
        bytes[0] = static_cast<unsigned char>(0xE0 | (codepoint >> 12));
        bytes[1] = static_cast<unsigned char>(0x80 | ((codepoint >> 6) & 0x3F));
        bytes[2] = static_cast<unsigned char>(0x80 | (codepoint & 0x3F));
        count = 3;
    }
    else {
        bytes[0] = static_cast<unsigned char>(0xF0 | (codepoint >> 18));
        bytes[1] = static_cast<unsigned char>(0x80 | ((codepoint >> 12) & 0x3F));
        bytes[2] = static_cast<unsigned char>(0x80 | ((codepoint >> 6) & 0x3F));
        bytes[3] = static_cast<unsigned char>(0x80 | (codepoint & 0x3F));
        count = 4;
    }

    return count;
}

//
// Append the UTF-8 byte sequence in hexadecimal, e.g. "Т" -> "0xD0 0xA2".
//
static void appendUtf8Hex(std::wstringstream& ss, uint32_t codepoint)
{
    unsigned char bytes[4];
    int count = encodeUtf8(codepoint, bytes);

    for (int i = 0; i < count; ++i) {
        if (i > 0)
            ss << L" ";
        ss << L"0x" << std::uppercase << std::hex << std::setfill(L'0') << std::setw(2) << (int)bytes[i];
    }

    ss << std::dec;
}

//
// Append the UTF-8 byte sequence in decimal, e.g. "Т" -> "208 162".
//
static void appendUtf8Dec(std::wstringstream& ss, uint32_t codepoint)
{
    unsigned char bytes[4];
    int count = encodeUtf8(codepoint, bytes);

    for (int i = 0; i < count; ++i) {
        if (i > 0)
            ss << L" ";
        ss << std::dec << (int)bytes[i];
    }
}

//
// Append the UTF-8 byte sequence as backslash-escaped octal, the notation git
// uses for non-ASCII bytes in paths, e.g. "Т" -> "\320\242". Every byte is
// escaped (including ASCII ones) so the field is unambiguous on its own.
//
static void appendUtf8Octal(std::wstringstream& ss, uint32_t codepoint)
{
    unsigned char bytes[4];
    int count = encodeUtf8(codepoint, bytes);

    for (int i = 0; i < count; ++i)
        ss << L"\\" << std::oct << std::setfill(L'0') << std::setw(3) << (int)bytes[i];

    ss << std::dec;
}

//
// Format the enabled character-info fields for display.
//
void formatCharacterCodes(uint32_t codepoint, TCHAR* output, size_t outputSize)
{
    if (!codepoint) {
        _sntprintf_s(output, outputSize, _TRUNCATE, TEXT("No character"));
        return;
    }

    std::wstringstream ss;
    bool first = true;

    // Separator helper: " | " between fields, nothing before the first one.
    auto sep = [&]() {
        if (!first)
            ss << L" | ";
        first = false;
    };

    // Fields are emitted in struct order. The code point and the description
    // carry no "(tag)" -- the U+ prefix and the plain-text name identify
    // themselves -- while every other field is tagged so the base it is printed
    // in is never ambiguous.
    if (g_fields.codePoint) {
        sep();
        ss << L"U+" << std::uppercase << std::hex << std::setfill(L'0') << std::setw(4) << codepoint << std::dec;
    }

    if (g_fields.html) {
        sep();
        ss << L"(html) &#" << std::dec << codepoint << L";";
    }

    if (g_fields.hex) {
        sep();
        ss << L"(hex) ";
        appendUtf8Hex(ss, codepoint);
    }

    if (g_fields.decimal) {
        sep();
        ss << L"(dec) ";
        appendUtf8Dec(ss, codepoint);
    }

    if (g_fields.octal) {
        sep();
        ss << L"(oct) ";
        appendUtf8Octal(ss, codepoint);
    }

    // Always last: the only variable-length field, so the short technical ones
    // stay at stable positions as the caret moves.
    if (g_fields.name) {
        TCHAR nameBuf[256];
        if (getUnicodeName(codepoint, nameBuf, 256)) {
            sep();
            ss << nameBuf;
        }
    }

    if (first) {
        // No fields enabled — give the user a hint rather than a blank bar.
        _sntprintf_s(output, outputSize, _TRUNCATE, TEXT("Emoji Description: no fields enabled (see Plugins menu)"));
        return;
    }

    std::wstring result = ss.str();
    _tcsncpy_s(output, outputSize, result.c_str(), _TRUNCATE);
}

//
// Fetch the text Notepad++ normally shows in the DOC_TYPE segment (the current
// language description, e.g. "Normal text file", "C++ source file"). We prepend
// it to our character info instead of overwriting it, so installing the plugin
// no longer hides the file's language (see issue #9).
//
static std::wstring getCurrentLangDesc()
{
    int langType = 0;
    ::SendMessage(nppData._nppHandle, NPPM_GETCURRENTLANGTYPE, 0, (LPARAM)&langType);

    // Two-call convention: first call returns the character count (excluding NUL).
    int len = (int)::SendMessage(nppData._nppHandle, NPPM_GETLANGUAGEDESC, langType, (LPARAM)NULL);
    if (len <= 0)
        return std::wstring();

    std::wstring desc(len, L'\0');
    ::SendMessage(nppData._nppHandle, NPPM_GETLANGUAGEDESC, langType, (LPARAM)&desc[0]);
    return desc;
}

//
// What sits under the caret. Shared by the status bar and the details dialog so
// the two can never disagree about which character is being described.
//
enum class CaretChar
{
    Ok,             // decoded; the code point is returned (0 if the bytes are invalid)
    EndOfDocument,  // the caret is past the last byte
    NoView,         // no active Scintilla view
};

//
// Read and decode the UTF-8 character at the caret in the active view.
//
static CaretChar readCaretCodePoint(uint32_t& codepoint)
{
    codepoint = 0;

    int which = -1;
    ::SendMessage(nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, (LPARAM)&which);
    if (which == -1)
        return CaretChar::NoView;

    HWND curScintilla = (which == 0) ? nppData._scintillaMainHandle : nppData._scintillaSecondHandle;

    Sci_Position pos = ::SendMessage(curScintilla, SCI_GETCURRENTPOS, 0, 0);
    Sci_Position docLength = ::SendMessage(curScintilla, SCI_GETLENGTH, 0, 0);
    if (pos >= docLength)
        return CaretChar::EndOfDocument;

    // Read up to 4 bytes -- the longest UTF-8 sequence.
    unsigned char buffer[5] = { 0 };
    Sci_TextRangeFull tr;
    tr.chrg.cpMin = pos;
    tr.chrg.cpMax = (pos + 4 > docLength) ? docLength : pos + 4;
    tr.lpstrText = (char*)buffer;
    ::SendMessage(curScintilla, SCI_GETTEXTRANGEFULL, 0, (LPARAM)&tr);

    int bytesRead = 0;
    codepoint = decodeUtf8Char(buffer, bytesRead);
    return CaretChar::Ok;
}

//
// Update character information in status bar
//
void updateCharacterInfo()
{
    if (!g_showCharInfo)
        return;

    uint32_t codepoint = 0;
    const CaretChar caret = readCaretCodePoint(codepoint);
    if (caret == CaretChar::NoView)
        return;

    // Keep the native language text and prepend it, rather than clobbering it.
    std::wstring lang = getCurrentLangDesc();
    std::wstring prefix = lang.empty() ? std::wstring() : lang + L" | ";

    if (caret == CaretChar::EndOfDocument) {
        std::wstring eod = prefix + L"End of document";
        ::SendMessage(nppData._nppHandle, NPPM_SETSTATUSBAR, STATUSBAR_DOC_TYPE, (LPARAM)eod.c_str());
        return;
    }

    TCHAR statusText[512];
    formatCharacterCodes(codepoint, statusText, 512);

    // Display in status bar, keeping the native language text in front of it.
    std::wstring out = prefix + statusText;
    ::SendMessage(nppData._nppHandle, NPPM_SETSTATUSBAR, STATUSBAR_DOC_TYPE, (LPARAM)out.c_str());
}

//
// Notification handler
//
void pluginBeNotified(SCNotification *notifyCode)
{
    switch (notifyCode->nmhdr.code)
    {
        case SCN_UPDATEUI:
            // Update when cursor moves or selection changes
            if (notifyCode->updated & (SC_UPDATE_SELECTION | SC_UPDATE_CONTENT))
            {
                updateCharacterInfo();
            }
            break;

        case NPPN_BUFFERACTIVATED:
            // Update when switching between documents
            updateCharacterInfo();
            break;
    }
}

//
// Message handler
//
LRESULT pluginMessageProc(UINT /*Message*/, WPARAM /*wParam*/, LPARAM /*lParam*/)
{
    return TRUE;
}

//----------------------------------------------//
//-- STEP 4. DEFINE YOUR ASSOCIATED FUNCTIONS --//
//----------------------------------------------//

void toggleShowCharInfo()
{
    g_showCharInfo = !g_showCharInfo;

    // Update menu check state
    ::SendMessage(nppData._nppHandle, NPPM_SETMENUITEMCHECK, funcItem[0]._cmdID, g_showCharInfo);
    saveConfig();

    if (g_showCharInfo) {
        updateCharacterInfo();
    } else {
        // Writing "" is a no-op for the status bar (Notepad++ ignores empty
        // strings), so the last char-info would just freeze. Restore the native
        // language text instead so the DOC_TYPE segment returns to normal at once.
        std::wstring lang = getCurrentLangDesc();
        ::SendMessage(nppData._nppHandle, NPPM_SETSTATUSBAR, STATUSBAR_DOC_TYPE, (LPARAM)lang.c_str());
    }
}

//
// Flip a single field flag: update its menu check, persist, and refresh the bar.
//
static void toggleField(bool& flag, int menuIndex)
{
    flag = !flag;
    ::SendMessage(nppData._nppHandle, NPPM_SETMENUITEMCHECK, funcItem[menuIndex]._cmdID, flag);
    saveConfig();
    if (g_showCharInfo)
        updateCharacterInfo();
}

// menuIndex values match the setCommand() indices in commandMenuInit(), which
// are offset by the separators at indices 1 and 3.
void toggleFieldCodePoint() { toggleField(g_fields.codePoint, 4); }
void toggleFieldHtml()      { toggleField(g_fields.html,      5); }
void toggleFieldHex()       { toggleField(g_fields.hex,       6); }
void toggleFieldDecimal()   { toggleField(g_fields.decimal,   7); }
void toggleFieldOctal()     { toggleField(g_fields.octal,     8); }
void toggleFieldName()      { toggleField(g_fields.name,      9); }

//
// Render a code point as UTF-16 text for display. Control characters come back
// empty: a literal CR, LF or TAB dropped into the dialog would break the layout
// of the very block meant to describe it, and the code point and description
// rows identify the character on their own.
//
static std::wstring codePointToText(uint32_t codepoint)
{
    if (codepoint < 0x20 || (codepoint >= 0x7F && codepoint <= 0x9F))
        return std::wstring();

    std::wstring text;
    if (codepoint < 0x10000) {
        text.push_back(static_cast<wchar_t>(codepoint));
    }
    else {
        const uint32_t offset = codepoint - 0x10000;
        text.push_back(static_cast<wchar_t>(0xD800 + (offset >> 10)));
        text.push_back(static_cast<wchar_t>(0xDC00 + (offset & 0x3FF)));
    }
    return text;
}

//
// Start a detail row: the label, padded so every value begins in the same column.
//
static void detailLabel(std::wstringstream& ss, const wchar_t* label)
{
    ss << std::left << std::setw(19) << std::setfill(L' ') << label << std::right;
}

//
// Compose the details block for the dialog. Every field is present regardless of
// the status-bar toggles -- those exist to keep that one line short during
// everyday work, while this is where you come when you actually need the data.
//
static std::wstring buildCharacterDetails(uint32_t codepoint, const std::wstring& glyph)
{
    std::wstringstream ss;

    detailLabel(ss, TEXT("Character"));
    ss << glyph << L"\r\n";

    detailLabel(ss, TEXT("Code point"));
    ss << L"U+" << std::uppercase << std::hex << std::setfill(L'0') << std::setw(4)
       << codepoint << std::dec << std::setfill(L' ') << L"\r\n";

    detailLabel(ss, TEXT("HTML entity"));
    ss << L"&#" << std::dec << codepoint << L";\r\n";

    detailLabel(ss, TEXT("UTF-8 bytes (hex)"));
    appendUtf8Hex(ss, codepoint);
    ss << L"\r\n";

    detailLabel(ss, TEXT("UTF-8 bytes (dec)"));
    appendUtf8Dec(ss, codepoint);
    ss << L"\r\n";

    detailLabel(ss, TEXT("UTF-8 bytes (oct)"));
    appendUtf8Octal(ss, codepoint);
    ss << L"\r\n";

    detailLabel(ss, TEXT("Description"));
    TCHAR nameBuf[256];
    if (getUnicodeName(codepoint, nameBuf, 256))
        ss << nameBuf;
    else
        ss << L"(no formal Unicode name)";
    ss << L"\r\n";

    return ss.str();
}

//
// Menu command: open the details dialog for the character at the caret. The
// clipboard is only touched from there, by an explicit button press.
//
void copyCharacterDetails()
{
    uint32_t codepoint = 0;
    const CaretChar caret = readCaretCodePoint(codepoint);

    // A zero code point means either a real NUL or bytes that did not decode;
    // neither is something we can describe, and the status bar says the same.
    if (caret != CaretChar::Ok || codepoint == 0) {
        showCharDetailsDialog(g_hInst, nppData._nppHandle, TEXT(""),
                              TEXT("No character at the caret."), false);
        return;
    }

    const std::wstring glyph = codePointToText(codepoint);
    const std::wstring details = buildCharacterDetails(codepoint, glyph);
    showCharDetailsDialog(g_hInst, nppData._nppHandle, glyph.c_str(), details.c_str(), true);
}

void aboutDialog()
{
    ::MessageBox(NULL,
        TEXT("Emoji Description v0.4.17\n\n")
        TEXT("Displays detailed character encoding information in the status bar.\n\n")
        TEXT("Shows for any character, e.g. for CYRILLIC CAPITAL LETTER TE:\n")
        TEXT("- U+0422 - Unicode code point\n")
        TEXT("- (html) &#1058; - HTML entity\n")
        TEXT("- (hex) 0xD0 0xA2 - UTF-8 bytes, hexadecimal\n")
        TEXT("- (dec) 208 162 - UTF-8 bytes, decimal\n")
        TEXT("- (oct) \\320\\242 - UTF-8 bytes, octal (git notation)\n")
        TEXT("- CYRILLIC CAPITAL LETTER TE - description\n\n")
        TEXT("Each field can be toggled from the Plugins menu.\n")
        TEXT("\"Copy character details...\" opens a dialog with every field\n")
        TEXT("for the character at the caret, ready to copy.\n")
        TEXT("Supports all Unicode characters including emoji!\n\n")
        TEXT("https://github.com/Ruberoid/npp_emoji_description"),
        TEXT("About Emoji Description"),
        MB_OK | MB_ICONINFORMATION);
}
