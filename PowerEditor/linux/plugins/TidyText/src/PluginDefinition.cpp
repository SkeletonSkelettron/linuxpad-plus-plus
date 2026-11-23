#include "PluginDefinition.h"
#include "menuCmdID.h"
#include "NonPrintingChars.h"
#include "SendToAI.h"
#include "ContextMenu.h"

//
// The plugin data that Notepad++ needs
//
FuncItem funcItem[nbFunc];

//
// The data of Notepad++ that you can use in your plugin commands
//
NppData nppData;

//
// Initialize your plugin data here
// It will be called while plugin loading
void pluginInit(HANDLE /*hModule*/)
{
}

//
// Here you can do the clean up, save the parameters (if any) for the next session
//
void pluginCleanUp()
{
}

//
// Initialization of your plugin commands
// You should fill your plugins commands here
void commandMenuInit()
{
    setCommand(0, TEXT("Highlight Non-Printing Characters"), highlightNonPrintingChars, NULL, false);
    setCommand(1, TEXT("Clear Highlights"), clearNonPrintingHighlights, NULL, false);
    setCommand(2, TEXT("Replace Non-Printing Characters"), replaceNonPrintingChars, NULL, false);

    // Default shortcuts (changeable in Settings > Shortcut Mapper > Plugin commands).
    static ShortcutKey skNext;
    skNext._isCtrl = false;
    skNext._isAlt = true;
    skNext._isShift = false;
    skNext._key = VK_F7;

    static ShortcutKey skPrev;
    skPrev._isCtrl = false;
    skPrev._isAlt = true;
    skPrev._isShift = true;
    skPrev._key = VK_F7;

    setCommand(3, TEXT("Next Match"), nextNonPrintingChar, &skNext, false);
    setCommand(4, TEXT("Previous Match"), prevNonPrintingChar, &skPrev, false);

    // separator: a FuncItem with a NULL function pointer renders as a menu separator
    lstrcpy(funcItem[5]._itemName, TEXT("-"));
    funcItem[5]._pFunc = NULL;
    funcItem[5]._init2Check = false;
    funcItem[5]._pShKey = NULL;

    setCommand(6, TEXT("Send Selection to ChatGPT"), sendSelectionToChatGPT, NULL, false);
    setCommand(7, TEXT("Send Selection to Claude"), sendSelectionToClaude, NULL, false);
    setCommand(8, TEXT("Send Selection to Gemini"), sendSelectionToGemini, NULL, false);

    lstrcpy(funcItem[9]._itemName, TEXT("-"));
    funcItem[9]._pFunc = NULL;
    funcItem[9]._init2Check = false;
    funcItem[9]._pShKey = NULL;

    setCommand(10, TEXT("Add TidyText to Right-Click Menu"), installContextMenuEntries, NULL, false);
}

void tidySetStatus(const wchar_t *text)
{
    ::SendMessage(nppData._nppHandle, NPPM_SETSTATUSBAR, STATUSBAR_DOC_TYPE, (LPARAM)text);
}

//
// Here you can do the clean up (especially for the shortcut)
//
void commandMenuCleanUp()
{
	// Don't forget to deallocate your shortcut here
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
