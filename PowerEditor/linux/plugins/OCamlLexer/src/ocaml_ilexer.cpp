/*
 * ocaml_ilexer.cpp -- OCaml as a Lexilla ILexer5 lexer, plus the plugin
 * half that runs olint and draws the diagnostics.
 *
 * MinGW-w64, x64 only. One calling convention on 64-bit Windows, so the
 * GCC/MSVC vtable ABI lines up for a single-inheritance pure-virtual
 * interface like ILexer5.
 */

#include <stdint.h>

#include <ILexer.h>
#include <Lexilla.h>
#include "ocaml_core.h"

#include <cstring>

using namespace Scintilla;

namespace {

/* Style names handed to Notepad++. Order MUST match the S_* enum. */
static const char *kStyleNames[OCAML_NUM_STYLES] = {
    "default", "keyword", "comment", "string", "char",
    "number", "operator", "typevar", "module"
};

#define OCAML_LEXER_ID 9002
#define MAX_FOLD_DEPTH  0x0FF
#define SC_FOLDLEVELBASE        0x400
#define SC_FOLDLEVELWHITEFLAG   0x1000
#define SC_FOLDLEVELHEADERFLAG  0x2000
#define SC_FOLDLEVELNUMBERMASK  0x0FFF

static bool isFoldOpener(const char *w, int len) {
    return (len == 5 && memcmp(w, "begin",  5) == 0) ||
           (len == 6 && memcmp(w, "struct", 6) == 0) ||
           (len == 3 && memcmp(w, "sig",    3) == 0) ||
           (len == 6 && memcmp(w, "object", 6) == 0);
}

static int FoldDelta(IDocument *pAccess, Sci_Position line) {
    const Sci_Position ls = pAccess->LineStart(line);
    const Sci_Position le = pAccess->LineEnd(line);

    int delta = 0;
    Sci_Position p = ls;
    while (p < le) {
        if (pAccess->StyleAt(p) != S_KEYWORD) { p++; continue; }
        const Sci_Position start = p;
        while (p < le && pAccess->StyleAt(p) == S_KEYWORD) p++;
        const int len = (int)(p - start);
        if (len > 0 && len <= 6) {
            char w[8];
            pAccess->GetCharRange(w, start, len);
            if (isFoldOpener(w, len))                    delta++;
            else if (len == 3 && memcmp(w, "end", 3) == 0) delta--;
        }
    }

    /* Multi-line comments fold too, and Lex already worked out where they
     * run: a positive line state means the line ended inside one. */
    const int before = (line > 0) ? pAccess->GetLineState(line - 1) : ST_NORMAL;
    const int after  = pAccess->GetLineState(line);
    if      (before <= 0 && after >  0) delta++;
    else if (before >  0 && after <= 0) delta--;

    return delta;
}


class LexerOCaml : public ILexer5 {
public:
    LexerOCaml() {}
    virtual ~LexerOCaml() {}

    /* ---- ILexer4 ---- */
    int SCI_METHOD Version() const override { return lvRelease5; }
    void SCI_METHOD Release() override { delete this; }

    const char * SCI_METHOD PropertyNames() override { return ""; }
    int SCI_METHOD PropertyType(const char *) override { return 0; }
    const char * SCI_METHOD DescribeProperty(const char *) override { return ""; }
    Sci_Position SCI_METHOD PropertySet(const char *, const char *) override { return -1; }
    const char * SCI_METHOD DescribeWordListSets() override { return ""; }
    Sci_Position SCI_METHOD WordListSet(int, const char *) override { return -1; }

    void SCI_METHOD Lex(Sci_PositionU startPos, Sci_Position lengthDoc,
                        int initStyle, IDocument *pAccess) override;
    void SCI_METHOD Fold(Sci_PositionU startPos, Sci_Position lengthDoc,
                         int /*initStyle*/, IDocument *pAccess) override {
        const Sci_Position docLen = pAccess->Length();
        Sci_Position endPos = (Sci_Position)startPos + lengthDoc;
        if (endPos > docLen) endPos = docLen;

        const Sci_Position startLine = pAccess->LineFromPosition((Sci_Position)startPos);
        const Sci_Position endLine =
            pAccess->LineFromPosition(endPos > 0 ? endPos - 1 : 0);

        int depth = 0;
        if (startLine > 0) {
            const int prev = pAccess->GetLevel(startLine - 1);
            depth = (prev & SC_FOLDLEVELNUMBERMASK) - SC_FOLDLEVELBASE
                  + FoldDelta(pAccess, startLine - 1);
        }
        if (depth < 0) depth = 0;

        for (Sci_Position line = startLine; line <= endLine; line++) {
            const int delta = FoldDelta(pAccess, line);

            int n = depth;
            if (n > MAX_FOLD_DEPTH) n = MAX_FOLD_DEPTH;
            int level = SC_FOLDLEVELBASE + n;
            if (delta > 0) level |= SC_FOLDLEVELHEADERFLAG;
            if (pAccess->LineEnd(line) == pAccess->LineStart(line))
                level |= SC_FOLDLEVELWHITEFLAG;
            pAccess->SetLevel(line, level);

            depth += delta;
            if (depth < 0) depth = 0;
        }

        const Sci_Position lastLine = pAccess->LineFromPosition(docLen);
        if (endLine < lastLine) {
            int n = depth;
            if (n > MAX_FOLD_DEPTH) n = MAX_FOLD_DEPTH;
            const int flags = pAccess->GetLevel(endLine + 1) & ~SC_FOLDLEVELNUMBERMASK;
            pAccess->SetLevel(endLine + 1, (SC_FOLDLEVELBASE + n) | flags);
        }
    }

    void * SCI_METHOD PrivateCall(int, void *) override { return nullptr; }
    int SCI_METHOD LineEndTypesSupported() override { return 0; }

    int SCI_METHOD AllocateSubStyles(int, int) override { return -1; }
    int SCI_METHOD SubStylesStart(int) override { return -1; }
    int SCI_METHOD SubStylesLength(int) override { return 0; }
    int SCI_METHOD StyleFromSubStyle(int subStyle) override { return subStyle; }
    int SCI_METHOD PrimaryStyleFromStyle(int style) override { return style; }
    void SCI_METHOD FreeSubStyles() override {}
    void SCI_METHOD SetIdentifiers(int, const char *) override {}
    int SCI_METHOD DistanceToSecondaryStyles() override { return 0; }
    const char * SCI_METHOD GetSubStyleBases() override { return ""; }

    int SCI_METHOD NamedStyles() override { return OCAML_NUM_STYLES; }
    const char * SCI_METHOD NameOfStyle(int style) override {
        return (style >= 0 && style < OCAML_NUM_STYLES) ? kStyleNames[style] : "";
    }
    const char * SCI_METHOD TagsOfStyle(int) override { return ""; }
    const char * SCI_METHOD DescriptionOfStyle(int) override { return ""; }

    /* ---- ILexer5 ---- */
    const char * SCI_METHOD GetName() override { return "OCaml"; }
    int SCI_METHOD GetIdentifier() override { return OCAML_LEXER_ID; }
    const char * SCI_METHOD PropertyGet(const char *) override { return ""; }
};

void SCI_METHOD LexerOCaml::Lex(Sci_PositionU startPos, Sci_Position lengthDoc,
                                int /*initStyle*/, IDocument *pAccess) {
    const Sci_Position docLen = pAccess->Length();
    const Sci_Position startLine = pAccess->LineFromPosition((Sci_Position)startPos);

    Sci_Position endPos = (Sci_Position)startPos + lengthDoc;
    if (endPos > docLen) endPos = docLen;
    const Sci_Position endLine =
        pAccess->LineFromPosition(endPos > 0 ? endPos - 1 : 0);

    /* Resume from the previous line, so re-lexing a slice mid-document
     * picks up mid-comment or mid-string. Negative means string. */
    int prevState = (startLine > 0) ? pAccess->GetLineState(startLine - 1) : ST_NORMAL;

    for (Sci_Position line = startLine; line <= endLine; line++) {
        const Sci_Position ls = pAccess->LineStart(line);
        const Sci_Position lnext = pAccess->LineStart(line + 1);
        int rawLen = (int)(lnext - ls);

        if (rawLen <= 0) {
            pAccess->StartStyling(ls);
            pAccess->SetLineState(line, ST_NORMAL);
            prevState = ST_NORMAL;
            continue;
        }
        if (rawLen > MAX_LINE - 1) rawLen = MAX_LINE - 1;

        char buf[MAX_LINE];
        char sty[MAX_LINE];
        pAccess->GetCharRange(buf, ls, rawLen);
        buf[rawLen] = '\0';

        int n = rawLen;
        while (n > 0 && (buf[n - 1] == '\r' || buf[n - 1] == '\n')) n--;

        memset(sty, S_DEFAULT, rawLen);
        const int newState = LexLine(buf, sty, n, prevState);

        pAccess->StartStyling(ls);
        pAccess->SetStyles(rawLen, sty);

        pAccess->SetLineState(line, newState);
        prevState = newState;
    }
}

LexerOCaml *FactoryOCaml() { return new LexerOCaml(); }

} /* anonymous namespace */

/* ---- Lexilla exports (Notepad++ loads these) ---- */

extern "C" {

ILexer5 * LEXILLA_CALL CreateLexer(const char *name) {
    if (name && _stricmp(name, "OCaml") == 0) return FactoryOCaml();
    return nullptr;
}

int LEXILLA_CALL GetLexerCount(void) { return 1; }

void LEXILLA_CALL GetLexerName(unsigned int index, char *name, int buflength) {
    if (buflength <= 0) return;
    name[0] = '\0';
    if (index == 0) {
        strncpy(name, "OCaml", buflength - 1);
        name[buflength - 1] = '\0';
    }
}

LexerFactoryFunction LEXILLA_CALL GetLexerFactory(unsigned int index) {
    return (index == 0) ? reinterpret_cast<LexerFactoryFunction>(FactoryOCaml) : nullptr;
}

const char * LEXILLA_CALL GetNameSpace(void) { return ""; }
const char * LEXILLA_CALL GetLibraryPropertyNames(void) { return ""; }
void LEXILLA_CALL SetLibraryProperty(const char *, const char *) {}

} /* extern "C" */

/* ---- Notepad++ plugin glue ---- */
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cstdio>
#include <cwchar>
#include <cstdarg>

/* Fixed path, since attaching a debugger to Notepad++ is no fun. */
static void dbg(const char *fmt, ...) {
    FILE *f = fopen("C:\\dev\\ocaml_lexer.log", "a");
    if (!f) return;
    va_list ap; va_start(ap, fmt); vfprintf(f, fmt, ap); va_end(ap);
    fputc('\n', f); fclose(f);
}

struct NppData { HWND _nppHandle; HWND _scintillaMainHandle; HWND _scintillaSecondHandle; };
typedef void (*PFUNCPLUGINCMD)();
struct ShortcutKey { bool _isCtrl; bool _isAlt; bool _isShift; UCHAR _key; };
struct FuncItem { TCHAR _itemName[64]; PFUNCPLUGINCMD _pFunc; int _cmdID; bool _init2Check; ShortcutKey *_pShKey; };
struct SciNotify { NMHDR nmhdr; char _pad[256]; };
struct SciNotifDwell { NMHDR nmhdr; LONG_PTR position; };

/* ---- NPP / Scintilla messages ---- */
#define NPPMSG                   (WM_USER + 1000)
#define NPPM_GETCURRENTSCINTILLA (NPPMSG + 4)
#define NPPM_GETFULLCURRENTPATH  (NPPMSG + 41)
#define NPPM_GETPLUGINSCONFIGDIR (NPPMSG + 46)
#define NPPN_FIRST               1000
#define NPPN_FILEOPENED          (NPPN_FIRST + 4)
#define NPPN_FILESAVED           (NPPN_FIRST + 8)
#define NPPN_BUFFERACTIVATED     (NPPN_FIRST + 10)
#define SCN_DWELLSTART           2016
#define SCN_DWELLEND             2017
#define SCI_GETLEXERLANGUAGE     4012
#define SCI_STYLESETFORE         2051
#define SCI_STYLESETBACK         2052
#define SCI_STYLESETBOLD         2053
#define SCI_STYLESETITALIC       2054
#define SCI_GETLENGTH            2006
#define SCI_POSITIONFROMLINE     2167
#define SCI_INDICSETSTYLE        2080
#define SCI_INDICSETFORE         2083
#define SCI_SETINDICATORCURRENT  2500
#define SCI_INDICATORFILLRANGE   2504
#define SCI_INDICATORCLEARRANGE  2505
#define SCI_CALLTIPSHOW          2200
#define SCI_CALLTIPCANCEL        2201
#define SCI_SETMOUSEDWELLTIME    2264
#define INDIC_SQUIGGLE           1
#define IND_ERR                  20
#define IND_WARN                 21
#define IND_INFO                 22

static NppData g_nppData;

static HWND CurrentSci() {
    int which = -1;
    SendMessage(g_nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, (LPARAM)&which);
    return (which == 0) ? g_nppData._scintillaMainHandle : g_nppData._scintillaSecondHandle;
}

static bool CurrentIsOCaml(HWND sci) {
    char name[32] = { 0 };
    SendMessageA(sci, SCI_GETLEXERLANGUAGE, 0, (LPARAM)name);
    return _stricmp(name, "OCaml") == 0;
}

/* Default colours (BGR), so it highlights before anyone edits the XML. */
static void SetStyleColours(HWND sci) {
    SendMessage(sci, SCI_STYLESETFORE,   S_DEFAULT,  0x000000);
    SendMessage(sci, SCI_STYLESETFORE,   S_KEYWORD,  0xCC0000);
    SendMessage(sci, SCI_STYLESETBOLD,   S_KEYWORD,  1);
    SendMessage(sci, SCI_STYLESETFORE,   S_COMMENT,  0x208020);
    SendMessage(sci, SCI_STYLESETITALIC, S_COMMENT,  1);
    SendMessage(sci, SCI_STYLESETFORE,   S_STRING,   0x2222C0);
    SendMessage(sci, SCI_STYLESETFORE,   S_CHAR,     0x9000A0);
    SendMessage(sci, SCI_STYLESETFORE,   S_NUMBER,   0x008CFF);
    SendMessage(sci, SCI_STYLESETFORE,   S_OPERATOR, 0x806000);
    SendMessage(sci, SCI_STYLESETFORE,   S_TYPEVAR,  0x808000);
    SendMessage(sci, SCI_STYLESETFORE,   S_MODULE,   0x905010);
}

/* ---- olint diagnostics ---- */

typedef struct {
    int  line, col, eline, ecol;
    int  startPos, endPos;
    char sev;
    char msg[320];
} Diag;

#define MAX_DIAGS 512
static Diag g_diags[MAX_DIAGS];
static int  g_nDiags = 0;
static char g_json[262144];

static bool runCapture(const wchar_t *cmdline, char *out, int outCap, int *outLen) {
    SECURITY_ATTRIBUTES sa; sa.nLength = sizeof(sa); sa.bInheritHandle = TRUE; sa.lpSecurityDescriptor = NULL;
    HANDLE rd = NULL, wr = NULL;
    if (!CreatePipe(&rd, &wr, &sa, 0)) return false;
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW si; ZeroMemory(&si, sizeof(si)); si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = wr; si.hStdError = wr; si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION pi; ZeroMemory(&pi, sizeof(pi));
    wchar_t cmd[2048]; lstrcpynW(cmd, cmdline, 2048);
    BOOL ok = CreateProcessW(NULL, cmd, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    CloseHandle(wr);
    if (!ok) { CloseHandle(rd); return false; }
    int total = 0; DWORD n = 0;
    while (total < outCap - 1 && ReadFile(rd, out + total, (DWORD)(outCap - 1 - total), &n, NULL) && n > 0)
        total += (int)n;
    out[total] = '\0'; *outLen = total;
    CloseHandle(rd);
    WaitForSingleObject(pi.hProcess, 5000);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    return true;
}

static bool objInt(const char *s, int len, const char *key, int *out) {
    int kl = (int)strlen(key);
    for (int i = 0; i + kl + 3 < len; i++) {
        if (s[i] == '"' && strncmp(s + i + 1, key, (size_t)kl) == 0 &&
            s[i + 1 + kl] == '"' && s[i + 2 + kl] == ':') {
            int j = i + 3 + kl; while (j < len && s[j] == ' ') j++;
            int sign = 1; if (j < len && s[j] == '-') { sign = -1; j++; }
            int v = 0; bool any = false;
            while (j < len && s[j] >= '0' && s[j] <= '9') { v = v * 10 + (s[j] - '0'); j++; any = true; }
            if (any) { *out = v * sign; return true; }
        }
    }
    return false;
}

static bool objStr(const char *s, int len, const char *key, char *out, int cap) {
    int kl = (int)strlen(key);
    for (int i = 0; i + kl + 3 < len; i++) {
        if (s[i] == '"' && strncmp(s + i + 1, key, (size_t)kl) == 0 &&
            s[i + 1 + kl] == '"' && s[i + 2 + kl] == ':') {
            int j = i + 3 + kl; while (j < len && s[j] == ' ') j++;
            if (j >= len || s[j] != '"') return false;
            j++; int o = 0;
            while (j < len && s[j] != '"' && o < cap - 1) {
                if (s[j] == '\\' && j + 1 < len) {
                    char c = s[j + 1];
                    out[o++] = (c == 'n') ? '\n' : (c == 't') ? '\t' : c; j += 2;
                } else out[o++] = s[j++];
            }
            out[o] = '\0'; return true;
        }
    }
    return false;
}

static void parseDiagnostics(const char *json, int n) {
    g_nDiags = 0;
    bool inStr = false; int depth = 0, objStart = -1;
    for (int i = 0; i < n; i++) {
        char c = json[i];
        if (inStr) { if (c == '\\') i++; else if (c == '"') inStr = false; continue; }
        if (c == '"') { inStr = true; continue; }
        if (c == '{') { if (depth == 0) objStart = i; depth++; }
        else if (c == '}') {
            depth--;
            if (depth == 0 && objStart >= 0 && g_nDiags < MAX_DIAGS) {
                const char *s = json + objStart; int len = i - objStart + 1;
                Diag *d = &g_diags[g_nDiags]; memset(d, 0, sizeof(*d));
                int v;
                if (objInt(s, len, "line", &v)) d->line = v;
                if (objInt(s, len, "col",  &v)) d->col  = v;
                d->eline = d->line; d->ecol = d->col + 1;
                if (objInt(s, len, "endline", &v)) d->eline = v;
                if (objInt(s, len, "endcol",  &v)) d->ecol  = v;
                char sevb[8] = ""; objStr(s, len, "severity", sevb, sizeof(sevb));
                d->sev = sevb[0] ? sevb[0] : 'W';
                char rule[16] = "", msg[256] = "", hint[200] = "";
                objStr(s, len, "rule", rule, sizeof(rule));
                objStr(s, len, "message", msg, sizeof(msg));
                bool hasHint = objStr(s, len, "hint", hint, sizeof(hint));
                snprintf(d->msg, sizeof(d->msg), "[%s] %s%s%s",
                         rule, msg, hasHint ? "\n  hint: " : "", hasHint ? hint : "");
                g_nDiags++;
            }
            objStart = -1;
        }
    }
}

static void configureIndicators(HWND sci) {
    SendMessage(sci, SCI_INDICSETSTYLE, IND_ERR,  INDIC_SQUIGGLE);
    SendMessage(sci, SCI_INDICSETFORE,  IND_ERR,  0x0000FF);
    SendMessage(sci, SCI_INDICSETSTYLE, IND_WARN, INDIC_SQUIGGLE);
    SendMessage(sci, SCI_INDICSETFORE,  IND_WARN, 0x0080FF);
    SendMessage(sci, SCI_INDICSETSTYLE, IND_INFO, INDIC_SQUIGGLE);
    SendMessage(sci, SCI_INDICSETFORE,  IND_INFO, 0xC08030);
    SendMessage(sci, SCI_SETMOUSEDWELLTIME, 400, 0);
}

static int docPos(HWND sci, int line, int col) {
    int lsp = (int)SendMessage(sci, SCI_POSITIONFROMLINE, line - 1, 0);
    return lsp + (col - 1);
}

static void applyDiagnostics(HWND sci) {
    int len = (int)SendMessage(sci, SCI_GETLENGTH, 0, 0);
    int inds[3] = { IND_ERR, IND_WARN, IND_INFO };
    for (int k = 0; k < 3; k++) {
        SendMessage(sci, SCI_SETINDICATORCURRENT, inds[k], 0);
        SendMessage(sci, SCI_INDICATORCLEARRANGE, 0, len);
    }
    for (int k = 0; k < g_nDiags; k++) {
        Diag *d = &g_diags[k];
        d->startPos = docPos(sci, d->line, d->col);
        d->endPos   = docPos(sci, d->eline, d->ecol);
        if (d->endPos <= d->startPos) d->endPos = d->startPos + 1;
        int ind = (d->sev == 'E') ? IND_ERR : (d->sev == 'I') ? IND_INFO : IND_WARN;
        SendMessage(sci, SCI_SETINDICATORCURRENT, ind, 0);
        SendMessage(sci, SCI_INDICATORFILLRANGE, d->startPos, d->endPos - d->startPos);
    }
}

#ifndef GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
#define GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS 0x00000004
#endif
#ifndef GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT
#define GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT 0x00000002
#endif

/* Address inside this DLL, for GetModuleHandleEx. */
static const char g_anchor = 0;

/* Prefer an olint.exe beside the DLL, so a release is one zip and no PATH
 * setup. Otherwise fall back to bare `olint`. */
static void getOlintExe(wchar_t *out, int cap) {
    HMODULE self = NULL;
    if (GetModuleHandleExW(
            GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            (LPCWSTR)&g_anchor, &self) && self) {
        wchar_t dll[1024]; dll[0] = 0;
        DWORD n = GetModuleFileNameW(self, dll, 1024);
        if (n > 0 && n < 1024) {
            wchar_t *slash = wcsrchr(dll, L'\\');
            if (slash) {
                *slash = L'\0';
                wchar_t cand[1100];
                _snwprintf(cand, 1100, L"%s\\olint.exe", dll);
                cand[1099] = L'\0';
                if (GetFileAttributesW(cand) != INVALID_FILE_ATTRIBUTES) {
                    lstrcpynW(out, cand, cap);
                    return;
                }
            }
        }
    }
    lstrcpynW(out, L"olint", cap);   /* not bundled: fall back to PATH */
}

static void lintWithOlint(HWND sci) {
    wchar_t path[1024]; path[0] = 0;
    SendMessage(g_nppData._nppHandle, NPPM_GETFULLCURRENTPATH, 1024, (LPARAM)path);
    if (!path[0]) { dbg("lint: no current path"); return; }
    wchar_t exe[1100]; getOlintExe(exe, 1100);
    char exeA[1200] = ""; WideCharToMultiByte(CP_UTF8, 0, exe, -1, exeA, sizeof(exeA), NULL, NULL);
    dbg("lint: olint='%s'", exeA);
    wchar_t cmd[2300];
    _snwprintf(cmd, 2300, L"\"%s\" -f json \"%s\"", exe, path);
    cmd[2299] = L'\0';
    int outLen = 0;
    if (!runCapture(cmd, g_json, sizeof(g_json), &outLen)) { dbg("lint: runCapture FAILED (olint not launchable)"); return; }
    dbg("lint: captured %d bytes", outLen);
    parseDiagnostics(g_json, outLen);
    dbg("lint: parsed %d diagnostics", g_nDiags);
    configureIndicators(sci);
    applyDiagnostics(sci);
    dbg("lint: drew indicators");
}

static void onDwell(HWND sci, int pos) {
    for (int k = 0; k < g_nDiags; k++) {
        Diag *d = &g_diags[k];
        if (pos >= d->startPos && pos < d->endPos) {
            SendMessage(sci, SCI_CALLTIPSHOW, pos, (LPARAM)d->msg);
            return;
        }
    }
}

/* ---- plugin command + exports ---- */

static FuncItem g_funcItems[1];
static void cmdLintNow() {
    HWND sci = CurrentSci();
    if (sci && CurrentIsOCaml(sci)) lintWithOlint(sci);
}

/* ---- Config XML ----
 * NPP won't load an external lexer without plugins\Config\<name>.xml, and
 * blames the DLL for being incompatible. Plugins Admin never writes there,
 * so do it here. setInfo runs before NPP looks. */
#include "ocaml_config_xml.h"

/* Only ever creates a missing file, so hand-edited colours survive. */
static void EnsureConfigXml() {
    wchar_t dir[MAX_PATH] = { 0 };
    SendMessage(g_nppData._nppHandle, NPPM_GETPLUGINSCONFIGDIR,
                MAX_PATH, (LPARAM)dir);
    if (!dir[0]) return;

    static const wchar_t leaf[] = L"\\OCamlLexer.xml";
    wchar_t path[MAX_PATH];
    if (wcslen(dir) + wcslen(leaf) + 1 > MAX_PATH) return;
    wcscpy(path, dir);
    wcscat(path, leaf);
    if (GetFileAttributesW(path) != INVALID_FILE_ATTRIBUTES) return;

    CreateDirectoryW(dir, NULL);   /* usually already there */
    HANDLE h = CreateFileW(path, GENERIC_WRITE, 0, NULL, CREATE_NEW,
                           FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE) return;
    DWORD written = 0;
    WriteFile(h, kConfigXml, (DWORD)(sizeof(kConfigXml) - 1), &written, NULL);
    CloseHandle(h);
}


extern "C" {

BOOL          isUnicode() { return TRUE; }
void          setInfo(NppData nd) { g_nppData = nd; dbg("== setInfo: plugin loaded =="); EnsureConfigXml(); }
const TCHAR * getName() { return TEXT("OCaml Lexer"); }
FuncItem *    getFuncsArray(int *n) {
    dbg("== getFuncsArray: menu built ==");
    lstrcpy(g_funcItems[0]._itemName, TEXT("Lint now (olint)"));
    g_funcItems[0]._pFunc = cmdLintNow;
    g_funcItems[0]._cmdID = 0;
    g_funcItems[0]._init2Check = false;
    g_funcItems[0]._pShKey = NULL;
    *n = 1;
    return g_funcItems;
}
void          beNotified(SciNotify *nt) {
    if (!nt) return;
    unsigned int code = nt->nmhdr.code;
    HWND sci = CurrentSci();
    if (!sci) return;

    if (code == NPPN_BUFFERACTIVATED || code == NPPN_FILEOPENED) {
        bool isml = CurrentIsOCaml(sci);
        dbg("notify: code=%u isOCaml=%d", code, (int)isml);
        if (isml) {
            SetStyleColours(sci);
            lintWithOlint(sci);
        }
    } else if (code == NPPN_FILESAVED) {
        bool isml = CurrentIsOCaml(sci);
        dbg("notify: SAVED isOCaml=%d", (int)isml);
        if (isml) lintWithOlint(sci);
    } else if (code == SCN_DWELLSTART) {
        if (CurrentIsOCaml(sci))
            onDwell(sci, (int)((SciNotifDwell *)nt)->position);
    } else if (code == SCN_DWELLEND) {
        SendMessage(sci, SCI_CALLTIPCANCEL, 0, 0);
    }
}
LRESULT       messageProc(UINT, WPARAM, LPARAM) { return TRUE; }

} /* extern "C" */
