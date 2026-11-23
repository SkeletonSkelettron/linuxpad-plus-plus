// Modal "character details" dialog.
//
// Opened from the plugin menu ("Copy character details..."), it shows every
// field for the character at the caret -- regardless of which ones the status
// bar is currently configured to display -- and offers to put the whole block
// on the clipboard. The clipboard is only ever touched by an explicit button
// press, never as a side effect of moving around the editor.
//
// The resource IDs live here, next to the code that uses them, the same way
// IDR_UNICODE_NAMES lives in UnicodeNames.h. The dialog template itself is in
// EmojiDescription.rc, which includes this header for the IDs.

#define IDD_CHAR_DETAILS 1101
#define IDC_CHAR_GLYPH   1102
#define IDC_CHAR_INFO    1103
#define IDC_CHAR_COPY    1104

#ifndef RC_INVOKED

#include <windows.h>

// Show the dialog modally, parented to the Notepad++ main window.
//
// glyph    the character itself, for the large display field; empty for
//          control characters and when there is nothing at the caret
// details  the text block shown in the detail field and copied verbatim
// canCopy  false when there is no character to describe, which disables the
//          copy button and leaves the block as a plain explanatory message
void showCharDetailsDialog(HINSTANCE hInst, HWND nppHandle,
                           const wchar_t* glyph, const wchar_t* details, bool canCopy);

#endif // RC_INVOKED
