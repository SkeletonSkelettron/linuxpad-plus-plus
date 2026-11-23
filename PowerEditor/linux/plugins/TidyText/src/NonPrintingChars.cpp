#include "PluginInterface.h"
#include "PluginDefinition.h"
#include "NonPrintingChars.h"
#include <vector>
#include <string>
#include <cstdint>

extern NppData nppData;

namespace {

struct CharMatch
{
    Sci_Position startByte;
    Sci_Position byteLength;
    std::string replacement; // empty means "delete this character"
};

HWND getCurrentScintilla()
{
    int which = -1;
    ::SendMessage(nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, (LPARAM)&which);
    if (which == -1)
        return NULL;
    return (which == 0) ? nppData._scintillaMainHandle : nppData._scintillaSecondHandle;
}

// Decodes one UTF-8 codepoint starting at buf[pos] (pos < len).
// Always advances by at least 1 byte, even on malformed input; outLen == 1 for a byte
// that looked like a multi-byte lead but didn't have valid continuation bytes signals failure.
uint32_t decodeUtf8(const unsigned char *buf, size_t len, size_t pos, size_t &outLen)
{
    unsigned char b0 = buf[pos];
    size_t remaining = len - pos;

    auto isCont = [buf, len](size_t i) { return i < len && (buf[i] & 0xC0) == 0x80; };

    if (b0 < 0x80)
    {
        outLen = 1;
        return (uint32_t)b0;
    }
    if ((b0 & 0xE0) == 0xC0 && remaining >= 2 && isCont(pos + 1))
    {
        outLen = 2;
        return (((uint32_t)b0 & 0x1F) << 6) | ((uint32_t)buf[pos + 1] & 0x3F);
    }
    if ((b0 & 0xF0) == 0xE0 && remaining >= 3 && isCont(pos + 1) && isCont(pos + 2))
    {
        outLen = 3;
        return (((uint32_t)b0 & 0x0F) << 12) |
               (((uint32_t)buf[pos + 1] & 0x3F) << 6) |
               ((uint32_t)buf[pos + 2] & 0x3F);
    }
    if ((b0 & 0xF8) == 0xF0 && remaining >= 4 && isCont(pos + 1) && isCont(pos + 2) && isCont(pos + 3))
    {
        outLen = 4;
        return (((uint32_t)b0 & 0x07) << 18) |
               (((uint32_t)buf[pos + 1] & 0x3F) << 12) |
               (((uint32_t)buf[pos + 2] & 0x3F) << 6) |
               ((uint32_t)buf[pos + 3] & 0x3F);
    }

    // Invalid/unsupported lead byte: consume it raw so scanning always progresses.
    outLen = 1;
    return (uint32_t)b0;
}

// Encodes a single codepoint back to UTF-8 bytes.
std::string encodeUtf8(uint32_t cp)
{
    std::string out;
    if (cp <= 0x7Fu)
    {
        out.push_back((char)cp);
    }
    else if (cp <= 0x7FFu)
    {
        out.push_back((char)(0xC0u | (cp >> 6)));
        out.push_back((char)(0x80u | (cp & 0x3Fu)));
    }
    else if (cp <= 0xFFFFu)
    {
        out.push_back((char)(0xE0u | (cp >> 12)));
        out.push_back((char)(0x80u | ((cp >> 6) & 0x3Fu)));
        out.push_back((char)(0x80u | (cp & 0x3Fu)));
    }
    else
    {
        out.push_back((char)(0xF0u | (cp >> 18)));
        out.push_back((char)(0x80u | ((cp >> 12) & 0x3Fu)));
        out.push_back((char)(0x80u | ((cp >> 6) & 0x3Fu)));
        out.push_back((char)(0x80u | (cp & 0x3Fu)));
    }
    return out;
}

// Returns true and fills replacement if this codepoint should be flagged on its own.
bool lookupReplacement(uint32_t cp, std::string &replacement)
{
    switch (cp)
    {
        case 0x00A0u: replacement = " ";   return true; // no-break space
        case 0x00ADu: replacement = "";    return true; // soft hyphen
        case 0x200Bu: replacement = "";    return true; // zero width space
        case 0x200Cu: replacement = "";    return true; // zero width non-joiner
        case 0x200Du: replacement = "";    return true; // zero width joiner
        case 0xFEFFu: replacement = "";    return true; // zero width no-break space / BOM
        case 0x2028u: replacement = "\n";  return true; // line separator
        case 0x2029u: replacement = "\n";  return true; // paragraph separator
        // Typographic characters (U+2013 etc.) whose high byte was chopped off, leaving only the
        // low byte as a C0 control character, e.g. "It<0x19>s" for "It's".
        case 0x13u: replacement = encodeUtf8(0x2013u); return true; // was en dash
        case 0x14u: replacement = encodeUtf8(0x2014u); return true; // was em dash
        case 0x18u: replacement = encodeUtf8(0x2018u); return true; // was left single quote
        case 0x19u: replacement = encodeUtf8(0x2019u); return true; // was right single quote / apostrophe
        case 0x1Cu: replacement = encodeUtf8(0x201Cu); return true; // was left double quote
        case 0x1Du: replacement = encodeUtf8(0x201Du); return true; // was right double quote
        default:
            if (cp <= 0x1Fu && cp != 0x09u && cp != 0x0Au && cp != 0x0Du)
            {
                replacement = ""; // stray C0 control char
                return true;
            }
            if (cp >= 0x80u && cp <= 0x9Fu)
            {
                replacement = ""; // stray C1 control char (PAD, CCH, ST, OSC, SGC, SCI, ...)
                return true;      // not part of a recoverable mojibake run, but still garbage
            }
            return false;
    }
}

// Byte range to scan: current selection if non-empty, else the whole document.
void getScanRange(HWND sci, bool wholeDocOnly, Sci_Position &start, Sci_Position &end)
{
    Sci_Position selStart = (Sci_Position)::SendMessage(sci, SCI_GETSELECTIONSTART, 0, 0);
    Sci_Position selEnd = (Sci_Position)::SendMessage(sci, SCI_GETSELECTIONEND, 0, 0);
    if (!wholeDocOnly && selEnd > selStart)
    {
        start = selStart;
        end = selEnd;
    }
    else
    {
        start = 0;
        end = (Sci_Position)::SendMessage(sci, SCI_GETLENGTH, 0, 0);
    }
}

// Mojibake repair: text that was originally valid UTF-8 (e.g. a curly quote, U+2019)
// but got byte-for-byte reinterpreted as Latin-1/Windows-1252 ends up as 2-4 separate
// "junk" codepoints (each one a former UTF-8 byte, now promoted to its own codepoint,
// e.g. U+00E2, U+0080, U+0099). Reinterpreting those codepoints' low bytes as a raw
// UTF-8 byte stream recovers the original character.
// Looks ahead from buf[pos] (whose first character is lead, already decoded, leadLen bytes).
// On success returns the number of source bytes consumed and the recovered codepoint; 0 if no match.
size_t tryMojibakeRepair(const unsigned char *buf, size_t len, size_t pos, uint32_t lead,
                         size_t leadLen, uint32_t &recoveredCp, size_t &srcBytes)
{
    // Only genuine multi-byte UTF-8 lead bytes (0xC2-0xF4) are candidates.
    if (lead < 0xC2u || lead > 0xF4u)
        return 0;

    unsigned char synth[4];
    size_t synthSrcLen[4];
    size_t available = 0;
    size_t p = pos;
    while (p < len && available < 4)
    {
        size_t cl = 1;
        uint32_t cp = (available == 0) ? lead : decodeUtf8(buf, len, p, cl);
        if (available == 0)
            cl = leadLen;
        if (cp > 0xFFu)
            break;
        synth[available] = (unsigned char)cp;
        synthSrcLen[available] = cl;
        ++available;
        p += cl;
    }
    if (available < 2)
        return 0;

    size_t consumed = 1;
    uint32_t cp = decodeUtf8(synth, available, 0, consumed);
    if (consumed < 2) // fell back to the "invalid lead byte" branch: not a real match
        return 0;

    srcBytes = 0;
    for (size_t k = 0; k < consumed; ++k)
        srcBytes += synthSrcLen[k];
    recoveredCp = cp;
    return consumed;
}

bool isLetter(unsigned char c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

// True if buf[p..] is a contraction ending (s, t, m, d, re, ve, ll) that is a whole word.
bool isContractionSuffixAt(const unsigned char *buf, size_t len, size_t p)
{
    if (p >= len)
        return false;
    size_t n = 0;
    while (p + n < len && isLetter(buf[p + n]) && n < 3)
        ++n;
    if (p + n < len && isLetter(buf[p + n]))
        return false; // longer word
    if (n == 1)
    {
        unsigned char c = buf[p] | 0x20;
        return c == 's' || c == 't' || c == 'm' || c == 'd';
    }
    if (n == 2)
    {
        unsigned char a = buf[p] | 0x20, b = buf[p + 1] | 0x20;
        return (a == 'r' && b == 'e') || (a == 'v' && b == 'e') || (a == 'l' && b == 'l');
    }
    return false;
}

// An apostrophe damaged by encoding often leaves a stray space ("armor' s", "it 's").
// Absorb that space into the match so the result is "armor's" / "it's".
void absorbApostropheSpace(const unsigned char *buf, size_t len, size_t pos, CharMatch &m, Sci_Position base)
{
    size_t end = pos + (size_t)m.byteLength;
    if (end + 1 < len && buf[end] == ' ' && isContractionSuffixAt(buf, len, end + 1))
    {
        m.byteLength += 1;
    }
    else if (pos >= 2 && buf[pos - 1] == ' ' && isLetter(buf[pos - 2]) && isContractionSuffixAt(buf, len, end))
    {
        m.startByte = base + (Sci_Position)(pos - 1);
        m.byteLength += 1;
    }
}

// Streams over the range without building a per-character list, so very large files stay cheap.
std::vector<CharMatch> scanForMatches(HWND sci, bool wholeDocOnly)
{
    std::vector<CharMatch> matches;

    Sci_Position start = 0, end = 0;
    getScanRange(sci, wholeDocOnly, start, end);
    if (end <= start)
        return matches;

    // Read a few bytes of context on each side so a selection-only scan can still see a
    // neighbouring space (needed to repair "it 's"); only matches inside [start,end) are reported.
    Sci_Position docLen = (Sci_Position)::SendMessage(sci, SCI_GETLENGTH, 0, 0);
    Sci_Position pre = (start < 4) ? start : 4;
    Sci_Position post = (docLen - end < 4) ? (docLen - end) : 4;
    Sci_Position bufStart = start - pre;
    Sci_Position bufEnd = end + post;

    size_t len = (size_t)(bufEnd - bufStart);
    std::vector<char> buf(len + 1);
    Sci_TextRangeFull tr;
    tr.chrg.cpMin = bufStart;
    tr.chrg.cpMax = bufEnd;
    tr.lpstrText = buf.data();
    ::SendMessage(sci, SCI_GETTEXTRANGEFULL, 0, (LPARAM)&tr);

    const unsigned char *ubuf = (const unsigned char *)buf.data();
    size_t scanEnd = (size_t)(pre + (end - start));
    size_t pos = (size_t)pre;
    while (pos < scanEnd)
    {
        size_t charLen = 1;
        uint32_t cp = decodeUtf8(ubuf, len, pos, charLen);

        // Fast path: plain ASCII that isn't a control character can never match.
        if (cp >= 0x20u && cp < 0x7Fu)
        {
            ++pos;
            continue;
        }

        uint32_t recoveredCp = 0;
        size_t srcBytes = 0;
        size_t consumed = tryMojibakeRepair(ubuf, len, pos, cp, charLen, recoveredCp, srcBytes);
        if (consumed > 0)
        {
            CharMatch m;
            if (!lookupReplacement(recoveredCp, m.replacement))
                m.replacement = encodeUtf8(recoveredCp);
            m.startByte = bufStart + (Sci_Position)pos;
            m.byteLength = (Sci_Position)srcBytes;
            if (recoveredCp == 0x2019u)
                absorbApostropheSpace(ubuf, len, pos, m, bufStart);
            matches.push_back(m);
            pos += srcBytes;
            continue;
        }

        CharMatch m;
        if (lookupReplacement(cp, m.replacement))
        {
            m.startByte = bufStart + (Sci_Position)pos;
            m.byteLength = (Sci_Position)charLen;
            if (cp == 0x19u || cp == 0x2019u)
                absorbApostropheSpace(ubuf, len, pos, m, bufStart);
            matches.push_back(m);
        }
        pos += charLen;
    }

    return matches;
}

int g_indicatorID = -1;

int getIndicator(HWND sci)
{
    if (g_indicatorID < 0)
    {
        int startNumber = -1;
        ::SendMessage(nppData._nppHandle, NPPM_ALLOCATEINDICATOR, 1, (LPARAM)&startNumber);
        g_indicatorID = startNumber;
    }
    if (g_indicatorID >= 0)
    {
        ::SendMessage(sci, SCI_INDICSETSTYLE, (WPARAM)g_indicatorID, (LPARAM)INDIC_SQUIGGLE);
        ::SendMessage(sci, SCI_INDICSETFORE, (WPARAM)g_indicatorID, (LPARAM)0x0000FF); // BGR red
    }
    return g_indicatorID;
}

void applyHighlights(HWND sci, int indicator, const std::vector<CharMatch> &matches)
{
    ::SendMessage(sci, SCI_SETINDICATORCURRENT, (WPARAM)indicator, 0);
    for (const CharMatch &m : matches)
        ::SendMessage(sci, SCI_INDICATORFILLRANGE, (WPARAM)m.startByte, (LPARAM)m.byteLength);
}

Sci_Position indValueAt(HWND sci, int ind, Sci_Position pos)
{
    return (Sci_Position)::SendMessage(sci, SCI_INDICATORVALUEAT, (WPARAM)ind, (LPARAM)pos);
}

Sci_Position indStart(HWND sci, int ind, Sci_Position pos)
{
    return (Sci_Position)::SendMessage(sci, SCI_INDICATORSTART, (WPARAM)ind, (LPARAM)pos);
}

Sci_Position indEnd(HWND sci, int ind, Sci_Position pos)
{
    return (Sci_Position)::SendMessage(sci, SCI_INDICATOREND, (WPARAM)ind, (LPARAM)pos);
}

bool hasAnyHighlight(HWND sci, int ind, Sci_Position len)
{
    if (len <= 0)
        return false;
    if (indValueAt(sci, ind, 0) != 0)
        return true;
    return indEnd(sci, ind, 0) < len;
}

// Finds the first highlighted range at/after 'from'. When includeAtStart is true, a range that
// begins exactly at 'from' counts; otherwise a range containing 'from' is skipped.
bool findNextRange(HWND sci, int ind, Sci_Position len, Sci_Position from, bool includeAtStart,
                   Sci_Position &s, Sci_Position &e)
{
    Sci_Position p = from;
    if (p >= len)
        return false;

    if (indValueAt(sci, ind, p) != 0)
    {
        Sci_Position rs = indStart(sci, ind, p);
        if (includeAtStart && p == rs)
        {
            s = rs;
            e = indEnd(sci, ind, rs);
            return true;
        }
        p = indEnd(sci, ind, p);
        if (p >= len)
            return false;
    }

    Sci_Position start = indEnd(sci, ind, p); // end of the unhighlighted gap = start of next range
    if (start >= len || indValueAt(sci, ind, start) == 0)
        return false;

    s = start;
    e = indEnd(sci, ind, start);
    return true;
}

// Finds the last highlighted range that ends at or before 'from'.
bool findPrevRange(HWND sci, int ind, Sci_Position len, Sci_Position from,
                   Sci_Position &s, Sci_Position &e)
{
    Sci_Position r = (from > len) ? len : from;
    if (r <= 0)
        return false;

    if (indValueAt(sci, ind, r) != 0)
        r = indStart(sci, ind, r);
    if (r <= 0)
        return false;

    Sci_Position t = r - 1;
    if (indValueAt(sci, ind, t) != 0)
    {
        s = indStart(sci, ind, t);
        e = indEnd(sci, ind, t);
        return true;
    }

    Sci_Position z = indStart(sci, ind, t); // start of the unhighlighted gap before r
    if (z <= 0)
        return false;

    s = indStart(sci, ind, z - 1);
    e = z;
    return true;
}

void selectRange(HWND sci, Sci_Position s, Sci_Position e)
{
    ::SendMessage(sci, SCI_SETSEL, (WPARAM)s, (LPARAM)e);
    Sci_Position line = (Sci_Position)::SendMessage(sci, SCI_LINEFROMPOSITION, (WPARAM)s, 0);
    ::SendMessage(sci, SCI_ENSUREVISIBLEENFORCEPOLICY, (WPARAM)line, 0);
    ::SendMessage(sci, SCI_SCROLLCARET, 0, 0);

    wchar_t msg[64];
    wsprintf(msg, L"TidyText: match on line %d", (int)(line + 1));
    tidySetStatus(msg);
}

void jumpToMatch(bool forward)
{
    HWND sci = getCurrentScintilla();
    if (!sci)
        return;

    int ind = getIndicator(sci);
    if (ind < 0)
        return;

    Sci_Position len = (Sci_Position)::SendMessage(sci, SCI_GETLENGTH, 0, 0);

    if (!hasAnyHighlight(sci, ind, len))
    {
        // Nothing highlighted yet: scan the whole document so Next/Previous work without a separate step.
        std::vector<CharMatch> matches = scanForMatches(sci, true);
        if (matches.empty())
        {
            tidySetStatus(L"TidyText: no non-printing or mis-encoded characters found");
            return;
        }
        applyHighlights(sci, ind, matches);
    }

    Sci_Position selStart = (Sci_Position)::SendMessage(sci, SCI_GETSELECTIONSTART, 0, 0);
    Sci_Position selEnd = (Sci_Position)::SendMessage(sci, SCI_GETSELECTIONEND, 0, 0);
    Sci_Position s = 0, e = 0;

    if (forward)
    {
        if (findNextRange(sci, ind, len, selEnd, selEnd == selStart, s, e))
        {
            selectRange(sci, s, e);
        }
        else if (findNextRange(sci, ind, len, 0, true, s, e))
        {
            selectRange(sci, s, e);
            tidySetStatus(L"TidyText: reached the end, wrapped to the first match");
        }
    }
    else
    {
        if (findPrevRange(sci, ind, len, selStart, s, e))
        {
            selectRange(sci, s, e);
        }
        else if (findPrevRange(sci, ind, len, len, s, e))
        {
            selectRange(sci, s, e);
            tidySetStatus(L"TidyText: reached the start, wrapped to the last match");
        }
    }
}

} // namespace

void highlightNonPrintingChars()
{
    HWND sci = getCurrentScintilla();
    if (!sci)
        return;

    int indicator = getIndicator(sci);
    if (indicator < 0)
        return;

    std::vector<CharMatch> matches = scanForMatches(sci, false);
    applyHighlights(sci, indicator, matches);

    TCHAR msg[192];
    wsprintf(msg, TEXT("Found and highlighted %d non-printing/mis-encoded character(s).\n\nUse Plugins > TidyText > Next / Previous Match to step through them."), (int)matches.size());
    ::MessageBox(nppData._nppHandle, msg, TEXT("TidyText"), MB_OK | MB_ICONINFORMATION);
}

void nextNonPrintingChar()
{
    jumpToMatch(true);
}

void prevNonPrintingChar()
{
    jumpToMatch(false);
}

void clearNonPrintingHighlights()
{
    HWND sci = getCurrentScintilla();
    if (!sci)
        return;

    int indicator = getIndicator(sci);
    if (indicator < 0)
        return;

    Sci_Position len = (Sci_Position)::SendMessage(sci, SCI_GETLENGTH, 0, 0);
    ::SendMessage(sci, SCI_SETINDICATORCURRENT, (WPARAM)indicator, 0);
    ::SendMessage(sci, SCI_INDICATORCLEARRANGE, 0, (LPARAM)len);
}

void replaceNonPrintingChars()
{
    HWND sci = getCurrentScintilla();
    if (!sci)
        return;

    std::vector<CharMatch> matches = scanForMatches(sci, false);
    if (matches.empty())
    {
        ::MessageBox(nppData._nppHandle, TEXT("No non-printing or mis-encoded characters found."), TEXT("TidyText"), MB_OK | MB_ICONINFORMATION);
        return;
    }

    // One undo step for the whole batch, so a single Ctrl+Z reverts every replacement.
    ::SendMessage(sci, SCI_BEGINUNDOACTION, 0, 0);

    // Replace back-to-front so earlier byte offsets stay valid as replacement lengths differ.
    for (std::vector<CharMatch>::reverse_iterator it = matches.rbegin(); it != matches.rend(); ++it)
    {
        ::SendMessage(sci, SCI_SETTARGETRANGE, (WPARAM)it->startByte, (LPARAM)(it->startByte + it->byteLength));
        ::SendMessage(sci, SCI_REPLACETARGET, (WPARAM)it->replacement.size(), (LPARAM)it->replacement.c_str());
    }

    ::SendMessage(sci, SCI_ENDUNDOACTION, 0, 0);

    TCHAR msg[128];
    wsprintf(msg, TEXT("Replaced %d non-printing/mis-encoded character(s)."), (int)matches.size());
    ::MessageBox(nppData._nppHandle, msg, TEXT("TidyText"), MB_OK | MB_ICONINFORMATION);
}
