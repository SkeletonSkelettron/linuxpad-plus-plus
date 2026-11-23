// Implementation of the modal character-details dialog. See CharDetailsDialog.h
// for what it is for; the dialog template is in EmojiDescription.rc.

#include "CharDetailsDialog.h"
#include "Notepad_plus_msgs.h"
#include <tchar.h>
#include <cstring>

namespace {

//
// Everything the dialog procedure needs. Lives on the caller's stack for the
// lifetime of the modal loop, reached through DWLP_USER.
//
struct DialogState
{
    const wchar_t* glyph;
    const wchar_t* details;
    bool           canCopy;
    HWND           nppHandle;
    HFONT          glyphFont;
    HFONT          infoFont;
};

//
// Put a wide string on the clipboard as CF_UNICODETEXT. On success the memory
// block belongs to the clipboard and must not be freed by us.
//
bool copyToClipboard(HWND owner, const wchar_t* text)
{
    if (!text)
        return false;

    const size_t bytes = (::wcslen(text) + 1) * sizeof(wchar_t);
    HGLOBAL block = ::GlobalAlloc(GMEM_MOVEABLE, bytes);
    if (!block)
        return false;

    void* dest = ::GlobalLock(block);
    if (!dest) {
        ::GlobalFree(block);
        return false;
    }
    ::memcpy(dest, text, bytes);
    ::GlobalUnlock(block);

    if (!::OpenClipboard(owner)) {
        ::GlobalFree(block);
        return false;
    }
    ::EmptyClipboard();
    if (!::SetClipboardData(CF_UNICODETEXT, block)) {
        ::CloseClipboard();
        ::GlobalFree(block);
        return false;
    }
    ::CloseClipboard();
    return true;
}

//
// Create a font at a point size scaled for the dialog's DPI.
//
HFONT makeFont(HWND hDlg, int pointSize, const wchar_t* face)
{
    HDC dc = ::GetDC(hDlg);
    const int dpi = dc ? ::GetDeviceCaps(dc, LOGPIXELSY) : 96;
    if (dc)
        ::ReleaseDC(hDlg, dc);

    return ::CreateFont(-::MulDiv(pointSize, dpi, 72), 0, 0, 0, FW_NORMAL,
                        FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                        OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, face);
}

//
// Center the dialog over the Notepad++ window rather than the screen, so it
// lands on the right monitor in a multi-display setup.
//
void centerOnParent(HWND hDlg, HWND parent)
{
    RECT dlgRect, parentRect;
    if (!parent || !::GetWindowRect(hDlg, &dlgRect) || !::GetWindowRect(parent, &parentRect))
        return;

    const int dlgWidth  = dlgRect.right - dlgRect.left;
    const int dlgHeight = dlgRect.bottom - dlgRect.top;
    const int x = parentRect.left + ((parentRect.right - parentRect.left) - dlgWidth) / 2;
    const int y = parentRect.top + ((parentRect.bottom - parentRect.top) - dlgHeight) / 2;

    ::SetWindowPos(hDlg, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
}

INT_PTR CALLBACK dialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
    DialogState* state = reinterpret_cast<DialogState*>(::GetWindowLongPtr(hDlg, DWLP_USER));

    switch (message)
    {
        case WM_INITDIALOG:
        {
            state = reinterpret_cast<DialogState*>(lParam);
            ::SetWindowLongPtr(hDlg, DWLP_USER, static_cast<LONG_PTR>(lParam));

            // The character gets a large font -- it is the one thing here you
            // may be squinting at. The detail block gets a monospace one so the
            // value column lines up.
            state->glyphFont = makeFont(hDlg, 22, TEXT("Segoe UI"));
            state->infoFont  = makeFont(hDlg, 9,  TEXT("Consolas"));

            HWND glyphCtl = ::GetDlgItem(hDlg, IDC_CHAR_GLYPH);
            HWND infoCtl  = ::GetDlgItem(hDlg, IDC_CHAR_INFO);

            if (state->glyphFont)
                ::SendMessage(glyphCtl, WM_SETFONT, (WPARAM)state->glyphFont, TRUE);
            if (state->infoFont)
                ::SendMessage(infoCtl, WM_SETFONT, (WPARAM)state->infoFont, TRUE);

            ::SetDlgItemText(hDlg, IDC_CHAR_GLYPH, state->glyph);
            ::SetDlgItemText(hDlg, IDC_CHAR_INFO, state->details);

            if (!state->canCopy)
                ::EnableWindow(::GetDlgItem(hDlg, IDC_CHAR_COPY), FALSE);

            // Adopt the Notepad++ theme, dark mode included. Nothing to undo:
            // the dialog is modal and short-lived, so it cannot outlive a theme
            // change and never needs dmfHandleChange.
            ::SendMessage(state->nppHandle, NPPM_DARKMODESUBCLASSANDTHEME,
                          static_cast<WPARAM>(NppDarkMode::dmfInit),
                          reinterpret_cast<LPARAM>(hDlg));

            centerOnParent(hDlg, state->nppHandle);

            // Preselect the block so Ctrl+C works the moment the dialog opens,
            // and so it is visibly selectable text rather than a static label.
            ::SetFocus(infoCtl);
            ::SendMessage(infoCtl, EM_SETSEL, 0, -1);
            return FALSE;  // focus was set explicitly
        }

        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
                case IDC_CHAR_COPY:
                    if (state && state->canCopy)
                        copyToClipboard(hDlg, state->details);
                    ::EndDialog(hDlg, IDC_CHAR_COPY);
                    return TRUE;

                case IDCANCEL:
                    ::EndDialog(hDlg, IDCANCEL);
                    return TRUE;
            }
            break;

        case WM_DESTROY:
            if (state) {
                if (state->glyphFont)
                    ::DeleteObject(state->glyphFont);
                if (state->infoFont)
                    ::DeleteObject(state->infoFont);
                state->glyphFont = NULL;
                state->infoFont = NULL;
            }
            break;
    }

    return FALSE;
}

} // namespace

void showCharDetailsDialog(HINSTANCE hInst, HWND nppHandle,
                           const wchar_t* glyph, const wchar_t* details, bool canCopy)
{
    DialogState state = {};
    state.glyph     = glyph ? glyph : TEXT("");
    state.details   = details ? details : TEXT("");
    state.canCopy   = canCopy;
    state.nppHandle = nppHandle;

    ::DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_CHAR_DETAILS), nppHandle,
                     dialogProc, reinterpret_cast<LPARAM>(&state));
}
