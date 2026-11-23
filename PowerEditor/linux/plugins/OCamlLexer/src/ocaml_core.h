/*
 * ocaml_core.h -- OCaml lexing logic. No Scintilla, no Notepad++, no
 * Windows. Takes a line, fills a style array, returns the continuation
 * state for the next one.
 */

#ifndef OCAML_CORE_H
#define OCAML_CORE_H

#include <cstring>
#include <cctype>

/* ---- Style IDs ---- */
enum {
    S_DEFAULT = 0, S_KEYWORD, S_COMMENT, S_STRING, S_CHAR,
    S_NUMBER, S_OPERATOR, S_TYPEVAR, S_MODULE
};
#define OCAML_NUM_STYLES 9

/* ---- Line state (returned by LexLine, fed to the next line) ----
 *   0   normal
 *  >0   inside nested comment, value = nesting depth
 *  -1   inside a {| ... |} quoted string
 *  -2   inside a "..." string
 */
#define ST_NORMAL    0
#define ST_QSTRING  -1
#define ST_STRING   -2

#define MAX_LINE 8192

/* ---- Keywords (OCaml manual keyword table) ---- */
static const char *kKeywords[] = {
    "and","as","assert","asr","begin","class","constraint","do","done","downto",
    "else","end","exception","external","false","for","fun","function","functor",
    "if","in","include","inherit","initializer","land","lazy","let","lor","lsl",
    "lsr","lxor","match","method","mod","module","mutable","new","nonrec","object",
    "of","open","or","private","rec","sig","struct","then","to","true","try",
    "type","val","virtual","when","while","with"
};
static const int kNumKeywords = (int)(sizeof(kKeywords) / sizeof(kKeywords[0]));

static inline bool isKeyword(const char *s, int len) {
    for (int i = 0; i < kNumKeywords; i++) {
        if ((int)strlen(kKeywords[i]) == len && strncmp(kKeywords[i], s, len) == 0)
            return true;
    }
    return false;
}

static inline bool isIdentStart(char c) { unsigned char u = (unsigned char)c; return isalpha(u) || c == '_'; }
static inline bool isIdentChar(char c)  { unsigned char u = (unsigned char)c; return isalnum(u) || c == '_' || c == '\''; }
static inline bool isOpChar(char c)     { return c != '\0' && strchr("!$%&*+-./:<=>?@^|~", c) != NULL; }

/* ---- Line lexer: fills sty[0..n-1], returns the state for the next line ---- */

static inline int LexLine(const char *buf, char *sty, int n, int prevState) {
    if (n <= 0) return (prevState > 0 || prevState == ST_QSTRING) ? prevState : ST_NORMAL;

    memset(sty, S_DEFAULT, n);

    int pos = 0;
    int commentDepth = (prevState > 0) ? prevState : 0;
    bool inStr  = (prevState == ST_STRING);
    bool inQStr = (prevState == ST_QSTRING);

    while (pos < n) {
        if (commentDepth > 0) {
            int start = pos;
            while (pos < n) {
                if (buf[pos] == '(' && pos + 1 < n && buf[pos + 1] == '*') { commentDepth++; pos += 2; }
                else if (buf[pos] == '*' && pos + 1 < n && buf[pos + 1] == ')') {
                    commentDepth--; pos += 2;
                    if (commentDepth == 0) break;
                } else pos++;
            }
            memset(sty + start, S_COMMENT, pos - start);
            continue;
        }
        if (inStr) {
            int start = pos;
            while (pos < n) {
                if (buf[pos] == '\\' && pos + 1 < n) pos += 2;
                else if (buf[pos] == '"') { pos++; inStr = false; break; }
                else pos++;
            }
            memset(sty + start, S_STRING, pos - start);
            continue;
        }
        if (inQStr) {
            int start = pos;
            while (pos < n) {
                if (buf[pos] == '|' && pos + 1 < n && buf[pos + 1] == '}') { pos += 2; inQStr = false; break; }
                else pos++;
            }
            memset(sty + start, S_STRING, pos - start);
            continue;
        }

        char ch = buf[pos];

        /* comment open */
        if (ch == '(' && pos + 1 < n && buf[pos + 1] == '*') {
            sty[pos] = S_COMMENT; sty[pos + 1] = S_COMMENT;
            pos += 2; commentDepth = 1;
            continue;
        }

        /* "..." string */
        if (ch == '"') { sty[pos] = S_STRING; pos++; inStr = true; continue; }

        /* {| ... |} and {id| ... |id} quoted strings */
        if (ch == '{') {
            if (pos + 1 < n && buf[pos + 1] == '|') {
                sty[pos] = S_STRING; sty[pos + 1] = S_STRING; pos += 2; inQStr = true; continue;
            }
            int p = pos + 1;
            while (p < n && (islower((unsigned char)buf[p]) || buf[p] == '_')) p++;
            if (p < n && buf[p] == '|') {
                int idlen = p - (pos + 1);
                int q = p + 1; bool closed = false;
                while (q < n) {
                    if (buf[q] == '|' && q + 1 + idlen < n &&
                        strncmp(buf + q + 1, buf + pos + 1, (size_t)idlen) == 0 &&
                        buf[q + 1 + idlen] == '}') { q = q + 1 + idlen + 1; closed = true; break; }
                    q++;
                }
                int end = closed ? q : n;
                memset(sty + pos, S_STRING, end - pos);
                pos = end; continue;
            }
            sty[pos] = S_DEFAULT; pos++; continue;
        }

        /* 'c' char literal vs 'a type variable */
        if (ch == '\'') {
            if (pos + 1 < n && buf[pos + 1] == '\\') {
                int start = pos; int q = pos + 2;
                if (q < n) {
                    if (buf[q] == 'x' || buf[q] == 'X') { q++; while (q < n && isxdigit((unsigned char)buf[q])) q++; }
                    else if (isdigit((unsigned char)buf[q])) { while (q < n && isdigit((unsigned char)buf[q])) q++; }
                    else q++;
                }
                if (q < n && buf[q] == '\'') q++;
                memset(sty + start, S_CHAR, q - start); pos = q; continue;
            }
            if (pos + 2 < n && buf[pos + 2] == '\'') {
                memset(sty + pos, S_CHAR, 3); pos += 3; continue;
            }
            int start = pos; pos++;
            while (pos < n && isIdentChar(buf[pos])) pos++;
            memset(sty + start, S_TYPEVAR, pos - start); continue;
        }

        /* numbers */
        if (isdigit((unsigned char)ch)) {
            int start = pos;
            if (ch == '0' && pos + 1 < n && strchr("xXoObB", buf[pos + 1])) {
                pos += 2;
                while (pos < n && (isalnum((unsigned char)buf[pos]) || buf[pos] == '_' || buf[pos] == '.')) pos++;
            } else {
                while (pos < n) {
                    char c = buf[pos];
                    if (isdigit((unsigned char)c) || c == '_' || c == '.' || c == 'e' || c == 'E') { pos++; continue; }
                    if ((c == '+' || c == '-') && pos > start && (buf[pos - 1] == 'e' || buf[pos - 1] == 'E')) { pos++; continue; }
                    break;
                }
            }
            memset(sty + start, S_NUMBER, pos - start); continue;
        }

        /* identifiers / keywords / modules */
        if (isIdentStart(ch)) {
            int start = pos;
            while (pos < n && isIdentChar(buf[pos])) pos++;
            int len = pos - start;
            if (isupper((unsigned char)ch))            memset(sty + start, S_MODULE, len);
            else if (isKeyword(buf + start, len))      memset(sty + start, S_KEYWORD, len);
            continue;
        }

        /* symbolic operators */
        if (isOpChar(ch)) {
            int start = pos;
            while (pos < n && isOpChar(buf[pos])) pos++;
            memset(sty + start, S_OPERATOR, pos - start); continue;
        }

        sty[pos] = S_DEFAULT; pos++;
    }

    if (commentDepth > 0) return commentDepth;
    if (inQStr) return ST_QSTRING;
    if (inStr)  return ST_STRING;
    return ST_NORMAL;
}

#endif /* OCAML_CORE_H */
