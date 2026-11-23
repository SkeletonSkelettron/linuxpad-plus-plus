#include "PluginInterface.h"
#include "PluginDefinition.h"
#include "SendToAI.h"
#include <shellapi.h>
#include <vector>
#include <string>
#include <cstring>

extern NppData nppData;

namespace {

// Windows' ShellExecute is unreliable for URLs much beyond ~2000 characters, so longer
// selections fall back to the clipboard.
const size_t kMaxUrlChars = 2000;

HWND getCurrentScintilla()
{
    int which = -1;
    ::SendMessage(nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, (LPARAM)&which);
    if (which == -1)
        return NULL;
    return (which == 0) ? nppData._scintillaMainHandle : nppData._scintillaSecondHandle;
}

void copyToClipboard(const std::vector<wchar_t> &wideBuf)
{
    if (!::OpenClipboard(nppData._nppHandle))
        return;

    ::EmptyClipboard();

    HGLOBAL hMem = ::GlobalAlloc(GMEM_MOVEABLE, wideBuf.size() * sizeof(wchar_t));
    if (hMem)
    {
        void *dest = ::GlobalLock(hMem);
        if (dest)
        {
            memcpy(dest, wideBuf.data(), wideBuf.size() * sizeof(wchar_t));
            ::GlobalUnlock(hMem);
            ::SetClipboardData(CF_UNICODETEXT, hMem);
        }
        else
        {
            ::GlobalFree(hMem);
        }
    }

    ::CloseClipboard();
}

// Percent-encodes UTF-8 bytes for use in a URL query value.
std::wstring urlEncode(const char *utf8, size_t len)
{
    static const wchar_t hex[] = L"0123456789ABCDEF";
    std::wstring out;
    out.reserve(len * 3);
    for (size_t i = 0; i < len; ++i)
    {
        unsigned char c = (unsigned char)utf8[i];
        bool unreserved = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
                          (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.' || c == '~';
        if (unreserved)
        {
            out.push_back((wchar_t)c);
        }
        else
        {
            out.push_back(L'%');
            out.push_back(hex[(c >> 4) & 0x0F]);
            out.push_back(hex[c & 0x0F]);
        }
    }
    return out;
}

bool openUrl(const std::wstring &url)
{
    HINSTANCE r = ::ShellExecuteW(NULL, L"open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
    return (INT_PTR)r > 32;
}

// Always copies the selection to the clipboard. If promptUrlPrefix is given and the text fits,
// opens the chat with the text already in the prompt box; otherwise opens the plain site and
// the user pastes with Ctrl+V.
void sendSelection(const wchar_t *baseUrl, const wchar_t *promptUrlPrefix)
{
    HWND sci = getCurrentScintilla();
    if (!sci)
        return;

    Sci_Position len = (Sci_Position)::SendMessage(sci, SCI_GETSELTEXT, 0, (LPARAM)NULL);
    if (len <= 1) // just the terminating NUL, i.e. empty selection
    {
        ::MessageBox(nppData._nppHandle, TEXT("No text selected."), TEXT("TidyText"), MB_OK | MB_ICONINFORMATION);
        return;
    }

    std::vector<char> utf8Buf((size_t)len);
    ::SendMessage(sci, SCI_GETSELTEXT, 0, (LPARAM)utf8Buf.data());

    int wideLen = ::MultiByteToWideChar(CP_UTF8, 0, utf8Buf.data(), -1, NULL, 0);
    if (wideLen <= 0)
        return;

    std::vector<wchar_t> wideBuf((size_t)wideLen);
    ::MultiByteToWideChar(CP_UTF8, 0, utf8Buf.data(), -1, wideBuf.data(), wideLen);
    copyToClipboard(wideBuf);

    if (promptUrlPrefix)
    {
        std::wstring url = promptUrlPrefix;
        url += urlEncode(utf8Buf.data(), utf8Buf.size() - 1); // exclude the terminating NUL
        if (url.size() <= kMaxUrlChars)
        {
            if (openUrl(url))
                tidySetStatus(L"TidyText: opened the chat with your text (also copied to the clipboard)");
            else
                tidySetStatus(L"TidyText: could not open the browser");
            return;
        }

        openUrl(baseUrl);
        tidySetStatus(L"TidyText: selection too long to put in the link - it is on the clipboard, press Ctrl+V in the chat");
        return;
    }

    openUrl(baseUrl);
    tidySetStatus(L"TidyText: selection copied - press Ctrl+V in the chat box");
}

} // namespace

void sendSelectionToChatGPT()
{
    sendSelection(L"https://chatgpt.com/", L"https://chatgpt.com/?q=");
}

void sendSelectionToClaude()
{
    sendSelection(L"https://claude.ai/new", L"https://claude.ai/new?q=");
}

void sendSelectionToGemini()
{
    // Gemini has no supported way to pre-fill a prompt from a link, so this one stays clipboard-based.
    sendSelection(L"https://gemini.google.com/app", NULL);
}
