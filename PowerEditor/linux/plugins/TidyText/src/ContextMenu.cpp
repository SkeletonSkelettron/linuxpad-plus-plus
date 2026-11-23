#include "PluginInterface.h"
#include "PluginDefinition.h"
#include "ContextMenu.h"
#include <string>
#include <vector>

extern NppData nppData;

namespace {

const char *kEntryNames[] = {
    "Highlight Non-Printing Characters",
    "Clear Highlights",
    "Replace Non-Printing Characters",
    "Next Match",
    "Previous Match",
    "Send Selection to ChatGPT",
    "Send Selection to Claude",
    "Send Selection to Gemini",
};

std::wstring npp(UINT msg)
{
    wchar_t buf[MAX_PATH] = { 0 };
    ::SendMessage(nppData._nppHandle, msg, MAX_PATH, (LPARAM)buf);
    return buf;
}

std::wstring parentDir(std::wstring p)
{
    size_t i = p.find_last_of(L"\\/");
    return (i == std::wstring::npos) ? p : p.substr(0, i);
}

bool fileExists(const std::wstring &p)
{
    return ::GetFileAttributesW(p.c_str()) != INVALID_FILE_ATTRIBUTES;
}

bool readFile(const std::wstring &p, std::string &out)
{
    HANDLE h = ::CreateFileW(p.c_str(), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return false;
    DWORD size = ::GetFileSize(h, NULL);
    out.assign(size, '\0');
    DWORD got = 0;
    bool ok = size == 0 || (::ReadFile(h, &out[0], size, &got, NULL) && got == size);
    ::CloseHandle(h);
    return ok;
}

bool writeFile(const std::wstring &p, const std::string &data)
{
    HANDLE h = ::CreateFileW(p.c_str(), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return false;
    DWORD put = 0;
    bool ok = data.empty() || (::WriteFile(h, data.data(), (DWORD)data.size(), &put, NULL) && put == data.size());
    ::CloseHandle(h);
    return ok;
}

// Removes every line that mentions the TidyText plugin entry or our marker comment.
std::string removeOldEntries(const std::string &xml)
{
    std::string out;
    size_t pos = 0;
    while (pos < xml.size())
    {
        size_t nl = xml.find('\n', pos);
        size_t end = (nl == std::string::npos) ? xml.size() : nl + 1;
        std::string line = xml.substr(pos, end - pos);
        if (line.find("PluginEntryName=\"TidyText\"") == std::string::npos &&
            line.find("TidyText plugin commands") == std::string::npos)
            out += line;
        pos = end;
    }
    return out;
}

} // namespace

void installContextMenuEntries()
{
    std::wstring cfgDir = parentDir(parentDir(npp(NPPM_GETPLUGINSCONFIGDIR))); // ...\Notepad++
    std::wstring target = cfgDir + L"\\contextMenu.xml";

    if (!fileExists(target))
    {
        // No customised copy yet: start from Notepad++'s default so the standard items stay.
        std::wstring dflt = npp(NPPM_GETNPPDIRECTORY) + L"\\contextMenu.xml";
        if (!fileExists(dflt) || !::CopyFileW(dflt.c_str(), target.c_str(), TRUE))
        {
            ::MessageBox(nppData._nppHandle, TEXT("Could not find or create contextMenu.xml."), TEXT("TidyText"), MB_OK | MB_ICONWARNING);
            return;
        }
    }

    std::string xml;
    if (!readFile(target, xml))
    {
        ::MessageBox(nppData._nppHandle, TEXT("Could not read contextMenu.xml."), TEXT("TidyText"), MB_OK | MB_ICONWARNING);
        return;
    }

    const char *closeTag = "</ScintillaContextMenu>";
    if (xml.find(closeTag) == std::string::npos)
    {
        ::MessageBox(nppData._nppHandle, TEXT("contextMenu.xml has no <ScintillaContextMenu> section; left it unchanged."), TEXT("TidyText"), MB_OK | MB_ICONWARNING);
        return;
    }

    ::CopyFileW(target.c_str(), (target + L".tidytext.bak").c_str(), FALSE);

    xml = removeOldEntries(xml);
    const char *eol = (xml.find("\r\n") != std::string::npos) ? "\r\n" : "\n";

    std::string block = std::string("        <!-- TidyText plugin commands -->") + eol;
    for (const char *name : kEntryNames)
        block += std::string("        <Item FolderName=\"TidyText\" PluginEntryName=\"TidyText\" PluginCommandItemName=\"") + name + "\" />" + eol;

    // Put it after the last existing plugin entry (next to "Plugin commands"); else before the closing tag.
    size_t insertAt = std::string::npos;
    size_t last = xml.rfind("PluginEntryName=");
    if (last != std::string::npos)
    {
        size_t nl = xml.find('\n', last);
        if (nl != std::string::npos)
            insertAt = nl + 1;
    }
    if (insertAt == std::string::npos)
    {
        size_t close = xml.find(closeTag);
        insertAt = xml.rfind('\n', close);
        insertAt = (insertAt == std::string::npos) ? close : insertAt + 1;
    }
    xml.insert(insertAt, block);

    if (!writeFile(target, xml))
    {
        ::MessageBox(nppData._nppHandle, TEXT("Could not write contextMenu.xml (is it read-only?)."), TEXT("TidyText"), MB_OK | MB_ICONWARNING);
        return;
    }

    ::MessageBox(nppData._nppHandle,
        TEXT("TidyText entries were added to the right-click menu.\n\nRestart Notepad++ to see them. A backup of the previous file was saved as contextMenu.xml.tidytext.bak."),
        TEXT("TidyText"), MB_OK | MB_ICONINFORMATION);
}
