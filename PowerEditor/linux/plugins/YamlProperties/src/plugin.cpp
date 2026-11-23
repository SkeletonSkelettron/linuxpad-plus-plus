#include "NppPluginQt.h"
#include "PluginInterface.h"
#include "menuCmdID.h"
#include "converter.h"
#include "ui_text.h"
#include <QDir>
#include <QMessageBox>
#include <QSaveFile>
#include <QSettings>
#include <QStringConverter>
#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <vector>

namespace {
constexpr wchar_t plugin_name[] = L"YamlProperties";
constexpr std::size_t input_limit = 8 * 1024 * 1024;
NppData npp;
std::array<FuncItem, 7> commands;
constexpr std::size_t new_tab_command = 3;
constexpr std::size_t sort_keys_command = 4;
bool new_tab = true;
bool sort_keys = false;
QString settings_path;
unsigned output_number = 0;
const yp::UiText* ui = &yp::english_ui;

void refresh_language() {
    // The native-language filename is available after NPPN_READY, not in setInfo.
    auto length = SendMessageW(npp._nppHandle, NPPM_GETNATIVELANGFILENAME, 0, 0);
    std::string filename;
    if (length > 0 && length < 4096) {
        std::vector<char> buffer(static_cast<std::size_t>(length) + 1, '\0');
        SendMessageW(npp._nppHandle, NPPM_GETNATIVELANGFILENAME, buffer.size(), reinterpret_cast<LPARAM>(buffer.data()));
        filename = buffer.data();
    }
    ui = &yp::ui_text(filename);
    auto menu = reinterpret_cast<HMENU>(SendMessageW(npp._nppHandle, NPPM_GETMENUHANDLE, NPPPLUGINMENU, 0));
    for (std::size_t i = 0; i < commands.size(); ++i) {
        lstrcpynW(commands[i]._itemName, ui->menus[i], menuItemSize);
        if (!commands[i]._pFunc || !menu) continue;
        MENUITEMINFOW item = {};
        item.cbSize = sizeof(item);
        item.fMask = MIIM_STRING;
        item.dwTypeData = commands[i]._itemName;
        SetMenuItemInfoW(menu, commands[i]._cmdID, FALSE, &item);
    }
    SendMessageW(npp._nppHandle, NPPM_SETMENUITEMCHECK, commands[new_tab_command]._cmdID, new_tab);
    SendMessageW(npp._nppHandle, NPPM_SETMENUITEMCHECK, commands[sort_keys_command]._cmdID, sort_keys);
    DrawMenuBar(npp._nppHandle);
}

void report(const std::string& message) {
    QMessageBox dialog(QMessageBox::Critical, QString::fromWCharArray(plugin_name),
        QString::fromUtf8(message.data(), message.size()), QMessageBox::Ok, nppWidget(npp._nppHandle));
    dialog.setTextFormat(Qt::PlainText);
    dialog.exec();
}
HWND editor() {
    int view = 0;
    SendMessageW(npp._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, reinterpret_cast<LPARAM>(&view));
    return view == 0 ? npp._scintillaMainHandle : npp._scintillaSecondHandle;
}
std::string read_input(HWND sci) {
    auto start = SendMessageW(sci, SCI_GETSELECTIONSTART, 0, 0);
    auto end = SendMessageW(sci, SCI_GETSELECTIONEND, 0, 0);
    bool selected = end != start;
    if (SendMessageW(sci, SCI_GETSELECTIONS, 0, 0) > 1 ||
        (selected && SendMessageW(sci, SCI_GETSELECTIONMODE, 0, 0) != SC_SEL_STREAM))
        throw yp::ConversionError("Select one continuous text range, or clear the selection to convert the whole document.");
    auto length = selected ? end-start : SendMessageW(sci, SCI_GETLENGTH, 0, 0);
    if (length < 0 || static_cast<std::size_t>(length) > input_limit) throw yp::ConversionError("Input exceeds 8 MiB.");
    if (SendMessageW(sci, SCI_GETCODEPAGE, 0, 0) != SC_CP_UTF8)
        throw yp::ConversionError("Convert the source to UTF-8 first.");
    if (!selected) start = 0;
    std::string bytes(static_cast<std::size_t>(length)+1, '\0');
    Sci_TextRangeFull range{{start, start + length}, bytes.data()};
    if (SendMessageW(sci, SCI_GETTEXTRANGEFULL, 0, reinterpret_cast<LPARAM>(&range)) != length)
        throw yp::ConversionError("Could not read the selected text.");
    bytes.resize(static_cast<std::size_t>(length));
    return bytes;
}
std::optional<yp::Format> filename_format() {
    wchar_t extension[64] = {};
    SendMessageW(npp._nppHandle, NPPM_GETEXTPART, 64, reinterpret_cast<LPARAM>(extension));
    if (lstrcmpiW(extension, L".yaml") == 0 || lstrcmpiW(extension, L".yml") == 0) return yp::Format::Yaml;
    if (lstrcmpiW(extension, L".properties") == 0) return yp::Format::Properties;
    return std::nullopt;
}
void validate_for_save(const std::string& utf8, UINT codepage) {
    if (codepage == CP_UTF8) return;
    if (std::all_of(utf8.begin(), utf8.end(), [](unsigned char c) { return c < 128; })) return;
    QByteArray name;
    if ((codepage >= 28591 && codepage <= 28599) || (codepage >= 28603 && codepage <= 28605))
        name = "ISO-8859-" + QByteArray::number(codepage - 28590);
    else if (codepage == 51949) name = "EUC-KR";
    else if (codepage == 10007) name = "x-mac-cyrillic";
    else if (codepage == 21866) name = "KOI8-U";
    else if (codepage == 20866) name = "KOI8-R";
    else name = "CP" + QByteArray::number(codepage);
    QStringEncoder encode(name.constData(), QStringConverter::Flag::Stateless);
    QStringDecoder decode(name.constData(), QStringConverter::Flag::Stateless);
    const QString text = QString::fromUtf8(utf8.data(), utf8.size());
    if (encode.isValid() && decode.isValid()) {
        const QByteArray bytes = encode(text);
        if (!encode.hasError() && QString(decode(bytes)) == text && !decode.hasError()) return;
    }
    throw yp::ConversionError("The result needs UTF-8. Enable output in a new tab or change the document encoding to UTF-8.");
}
UINT document_codepage() {
    HMENU menu = GetMenu(npp._nppHandle);
    auto checked = [menu](UINT command) {
        UINT state = GetMenuState(menu, command, MF_BYCOMMAND);
        return state != static_cast<UINT>(-1) && (state & MF_CHECKED);
    };
    // Notepad++ keeps named legacy encodings in a UTF-8 Scintilla buffer.
    // Its Encoding menu exposes the save encoding; SCI_GETCODEPAGE alone does not.
    if (checked(IDM_FORMAT_ANSI)) return 1252;
    struct Charset { UINT command; UINT codepage; };
    constexpr Charset charsets[] = {
        {IDM_FORMAT_WIN_1250, 1250}, {IDM_FORMAT_WIN_1251, 1251}, {IDM_FORMAT_WIN_1252, 1252},
        {IDM_FORMAT_WIN_1253, 1253}, {IDM_FORMAT_WIN_1254, 1254}, {IDM_FORMAT_WIN_1255, 1255},
        {IDM_FORMAT_WIN_1256, 1256}, {IDM_FORMAT_WIN_1257, 1257}, {IDM_FORMAT_WIN_1258, 1258},
        {IDM_FORMAT_ISO_8859_1, 28591}, {IDM_FORMAT_ISO_8859_2, 28592}, {IDM_FORMAT_ISO_8859_3, 28593},
        {IDM_FORMAT_ISO_8859_4, 28594}, {IDM_FORMAT_ISO_8859_5, 28595}, {IDM_FORMAT_ISO_8859_6, 28596},
        {IDM_FORMAT_ISO_8859_7, 28597}, {IDM_FORMAT_ISO_8859_8, 28598}, {IDM_FORMAT_ISO_8859_9, 28599},
        {IDM_FORMAT_ISO_8859_13, 28603}, {IDM_FORMAT_ISO_8859_14, 28604}, {IDM_FORMAT_ISO_8859_15, 28605},
        {IDM_FORMAT_DOS_437, 437}, {IDM_FORMAT_DOS_720, 720}, {IDM_FORMAT_DOS_737, 737},
        {IDM_FORMAT_DOS_775, 775}, {IDM_FORMAT_DOS_850, 850}, {IDM_FORMAT_DOS_852, 852},
        {IDM_FORMAT_DOS_855, 855}, {IDM_FORMAT_DOS_857, 857}, {IDM_FORMAT_DOS_858, 858},
        {IDM_FORMAT_DOS_860, 860}, {IDM_FORMAT_DOS_861, 861}, {IDM_FORMAT_DOS_862, 862},
        {IDM_FORMAT_DOS_863, 863}, {IDM_FORMAT_DOS_865, 865}, {IDM_FORMAT_DOS_866, 866},
        {IDM_FORMAT_DOS_869, 869}, {IDM_FORMAT_BIG5, 950}, {IDM_FORMAT_GB2312, 936},
        {IDM_FORMAT_SHIFT_JIS, 932}, {IDM_FORMAT_KOREAN_WIN, 949}, {IDM_FORMAT_EUC_KR, 51949},
        {IDM_FORMAT_TIS_620, 874}, {IDM_FORMAT_MAC_CYRILLIC, 10007},
        {IDM_FORMAT_KOI8U_CYRILLIC, 21866}, {IDM_FORMAT_KOI8R_CYRILLIC, 20866},
    };
    for (const auto& charset : charsets)
        if (checked(charset.command)) return charset.codepage;
    return CP_UTF8; // UTF-8 and UTF-16 files can represent every emitted Unicode scalar.
}
void replace_in_place(HWND source, const std::string& output, bool yaml_output) {
    if (SendMessageW(source, SCI_GETREADONLY, 0, 0))
        throw yp::ConversionError("The document is read-only. Enable new-tab output or make the document writable first. Source text was preserved.");
    validate_for_save(output, document_codepage());
    auto start = SendMessageW(source, SCI_GETSELECTIONSTART, 0, 0);
    auto end = SendMessageW(source, SCI_GETSELECTIONEND, 0, 0);
    bool selected = start != end;
    if (!selected) {
        start = 0;
        end = SendMessageW(source, SCI_GETLENGTH, 0, 0);
    }
    SendMessageW(source, SCI_BEGINUNDOACTION, 0, 0);
    SendMessageW(source, SCI_SETTARGETSTART, start, 0);
    SendMessageW(source, SCI_SETTARGETEND, end, 0);
    SendMessageW(source, SCI_REPLACETARGET, output.size(), reinterpret_cast<LPARAM>(output.data()));
    SendMessageW(source, SCI_ENDUNDOACTION, 0, 0);
    if (selected) {
        SendMessageW(source, SCI_SETSEL, start, start + static_cast<LPARAM>(output.size()));
    } else {
        SendMessageW(source, SCI_GOTOPOS, 0, 0);
        SendMessageW(npp._nppHandle, NPPM_SETCURRENTLANGTYPE, 0, yaml_output ? L_YAML : L_PROPS);
    }
}
void convert(std::optional<bool> requested_direction) {
    try {
        HWND source = editor();
        auto input = read_input(source);
        std::string_view text(input);
        if (text.substr(0, 3) == "\xef\xbb\xbf") text.remove_prefix(3);
        if (text.find_first_not_of(" \t\r\n\f\v") == std::string_view::npos) {
            QMessageBox::information(nppWidget(npp._nppHandle), QString::fromWCharArray(plugin_name),
                QString::fromWCharArray(ui->empty_input));
            return;
        }
        bool yaml_output = requested_direction.value_or(false);
        std::string output;
        if (requested_direction) {
            output = yaml_output ? yp::to_yaml(input, sort_keys) : yp::to_properties(input, sort_keys);
        } else {
            auto converted = yp::auto_convert(input, filename_format(), sort_keys);
            yaml_output = converted.output_format == yp::Format::Yaml;
            output = std::move(converted.text);
        }
        auto eol = SendMessageW(source, SCI_GETEOLMODE, 0, 0);
        if (eol != SC_EOL_LF) {
            std::string converted;
            converted.reserve(output.size() + output.size()/20);
            for (char c : output) {
                if (c == '\n') converted += eol == SC_EOL_CRLF ? "\r\n" : "\r";
                else converted += c;
            }
            output = std::move(converted);
        }
        if (!new_tab) {
            replace_in_place(source, output, yaml_output);
            return;
        }
        auto old_buffer = SendMessageW(npp._nppHandle, NPPM_GETCURRENTBUFFERID, 0, 0);
        SendMessageW(npp._nppHandle, NPPM_MENUCOMMAND, 0, IDM_FILE_NEW);
        auto new_buffer = SendMessageW(npp._nppHandle, NPPM_GETCURRENTBUFFERID, 0, 0);
        if (!new_buffer || old_buffer == new_buffer) throw yp::ConversionError("Could not create an output tab. Source text was preserved.");
        SendMessageW(npp._nppHandle, NPPM_MENUCOMMAND, 0, IDM_FORMAT_AS_UTF_8);
        HWND target = editor();
        SendMessageW(target, SCI_SETCODEPAGE, SC_CP_UTF8, 0);
        SendMessageW(target, SCI_SETEOLMODE, eol, 0);
        SendMessageW(target, SCI_BEGINUNDOACTION, 0, 0);
        SendMessageW(target, SCI_ADDTEXT, output.size(), reinterpret_cast<LPARAM>(output.data()));
        SendMessageW(target, SCI_ENDUNDOACTION, 0, 0);
        SendMessageW(target, SCI_GOTOPOS, 0, 0);
        SendMessageW(npp._nppHandle, NPPM_SETCURRENTLANGTYPE, 0, yaml_output ? L_YAML : L_PROPS);
        auto title = L"converted-" + std::to_wstring(++output_number) + (yaml_output ? L".yaml" : L".properties");
        SendMessageW(npp._nppHandle, NPPM_SETUNTITLEDNAME, new_buffer, reinterpret_cast<LPARAM>(title.c_str()));
    } catch (const std::exception& error) { report(error.what()); }
    catch (...) { report("Unexpected conversion failure. Source text was preserved."); }
}
void yaml_to_properties() { convert(false); }
void properties_to_yaml() { convert(true); }
void auto_convert() { convert(std::nullopt); }

bool save_settings(bool new_tab_value, bool sort_keys_value) {
    if (settings_path.isEmpty()) return false;
    const std::string text = std::string("[Settings]\r\nNewTab=") + (new_tab_value ? "1\r\n" : "0\r\n") +
                             "SortKeys=" + (sort_keys_value ? "1\r\n" : "0\r\n");
    QSaveFile file(settings_path);
    return file.open(QIODevice::WriteOnly) && file.write(text.data(), text.size()) == static_cast<qint64>(text.size())
        && file.commit();
}
void toggle_new_tab() {
    if (!save_settings(!new_tab, sort_keys)) {
        report(ui->output_save_error);
        return;
    }
    new_tab = !new_tab;
    SendMessageW(npp._nppHandle, NPPM_SETMENUITEMCHECK, commands[new_tab_command]._cmdID, new_tab);
}
void toggle_sort_keys() {
    if (!save_settings(new_tab, !sort_keys)) {
        report(ui->sort_save_error);
        return;
    }
    sort_keys = !sort_keys;
    SendMessageW(npp._nppHandle, NPPM_SETMENUITEMCHECK, commands[sort_keys_command]._cmdID, sort_keys);
}
void about() {
    const QString rules = QString::fromWCharArray(ui->about).toHtmlEscaped().replace(QLatin1Char('\n'), QStringLiteral("<br>"));
    const QString homepage = QStringLiteral("https://github.com/oiltea94/npp-yaml-properties");
    QMessageBox::about(nppWidget(npp._nppHandle), QStringLiteral("YamlProperties " NPP_YAML_PROPERTIES_VERSION),
        rules + QStringLiteral("<p>%1 <a href=\"%2\">%2</a></p>").arg(QString::fromWCharArray(ui->homepage_label), homepage));
}
}

extern "C" __declspec(dllexport) void setInfo(NppData data) {
    npp = data;
    try {
        const auto length = SendMessageW(npp._nppHandle, NPPM_GETPLUGINSCONFIGDIR, 0, 0);
        if (length > 0) {
            std::vector<wchar_t> path(length + 1, L'\0');
            if (SendMessageW(npp._nppHandle, NPPM_GETPLUGINSCONFIGDIR, path.size(), reinterpret_cast<LPARAM>(path.data()))) {
                const QDir config(QString::fromWCharArray(path.data()));
                if (config.exists() || QDir().mkpath(config.path()))
                    settings_path = config.filePath(QStringLiteral("YamlProperties.ini"));
            }
        }
        if (!settings_path.isEmpty()) {
            const QSettings settings(settings_path, QSettings::IniFormat);
            new_tab = settings.value(QStringLiteral("Settings/NewTab"), true).toBool();
            sort_keys = settings.value(QStringLiteral("Settings/SortKeys"), false).toBool();
        }
        PFUNCPLUGINCMD handlers[] = {yaml_to_properties, properties_to_yaml, auto_convert, toggle_new_tab, toggle_sort_keys, nullptr, about};
        for (std::size_t i = 0; i < commands.size(); ++i) {
            lstrcpynW(commands[i]._itemName, ui->menus[i], menuItemSize);
            commands[i]._pFunc = handlers[i];
        }
        commands[new_tab_command]._init2Check = new_tab;
        commands[sort_keys_command]._init2Check = sort_keys;
    } catch (...) { report("Plugin initialization failed."); }
}
extern "C" __declspec(dllexport) const wchar_t* getName() { return plugin_name; }
extern "C" __declspec(dllexport) FuncItem* getFuncsArray(int* count) {
    *count = static_cast<int>(commands.size());
    return commands.data();
}
extern "C" __declspec(dllexport) void beNotified(SCNotification* notification) {
    if (notification && (notification->nmhdr.code == NPPN_READY || notification->nmhdr.code == NPPN_NATIVELANGCHANGED))
        try { refresh_language(); }
        catch (...) { report("Could not update the plugin interface language."); }
}
extern "C" __declspec(dllexport) LRESULT messageProc(UINT, WPARAM, LPARAM) { return TRUE; }
extern "C" __declspec(dllexport) BOOL isUnicode() { return TRUE; }
