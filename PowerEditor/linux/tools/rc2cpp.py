# This file is part of Notepad++ project.
# SPDX-License-Identifier: GPL-3.0-or-later

"""Translate a plugin's preprocessed Win32 resources into the Linux SDK tables."""

import argparse
from collections import defaultdict
from pathlib import Path
import re
import subprocess
import sys


TOKEN = re.compile(r'L?"(?:""|\\.|[^"\\])*"|<<|>>|[\w.]+|[^\s]')
NUMBER = re.compile(r"(?:0[xX][0-9a-fA-F]+|[0-9]+)[uUlL]*")
STRING = re.compile(r'L?"(?:""|\\.|[^"\\])*"')
OPERATORS = set("()|&^~+-*/%") | {"<<", ">>"}
RESOURCE_FLAGS = {"DISCARDABLE", "FIXED", "MOVEABLE", "PRELOAD", "LOADONCALL", "PURE", "IMPURE"}
RESOURCE_METADATA = {"LANGUAGE", "CHARACTERISTICS", "VERSION"}

# Standard abbreviated controls have a class, an implicit style and,
# for Edit/ComboBox/ListBox/ScrollBar, no title argument.
CONTROL_DEFAULTS = {
    "LTEXT": ("Static", "SS_LEFT | WS_GROUP"),
    "CTEXT": ("Static", "SS_CENTER | WS_GROUP"),
    "RTEXT": ("Static", "SS_RIGHT | WS_GROUP"),
    "ICON": ("Static", "SS_ICON"),
    "EDITTEXT": ("Edit", "WS_BORDER | WS_TABSTOP | ES_LEFT"),
    "COMBOBOX": ("ComboBox", "WS_TABSTOP | CBS_SIMPLE"),
    "LISTBOX": ("ListBox", "WS_BORDER | LBS_NOTIFY"),
    "SCROLLBAR": ("ScrollBar", "SBS_HORZ"),
    "PUSHBUTTON": ("Button", "WS_TABSTOP | BS_PUSHBUTTON"),
    "DEFPUSHBUTTON": ("Button", "WS_TABSTOP | BS_DEFPUSHBUTTON"),
    "PUSHBOX": ("Button", "WS_TABSTOP | BS_PUSHBOX"),
    "GROUPBOX": ("Button", "BS_GROUPBOX"),
    "CHECKBOX": ("Button", "WS_TABSTOP | BS_CHECKBOX"),
    "AUTOCHECKBOX": ("Button", "WS_TABSTOP | BS_AUTOCHECKBOX"),
    "RADIOBUTTON": ("Button", "BS_RADIOBUTTON"),
    "AUTORADIOBUTTON": ("Button", "BS_AUTORADIOBUTTON"),
    "STATE3": ("Button", "WS_TABSTOP | BS_3STATE"),
    "AUTO3STATE": ("Button", "WS_TABSTOP | BS_AUTO3STATE"),
}
TITLELESS_CONTROLS = {"EDITTEXT", "COMBOBOX", "LISTBOX", "SCROLLBAR"}
IMAGE_TYPES = {"BITMAP": "IMAGE_BITMAP", "ICON": "IMAGE_ICON", "CURSOR": "IMAGE_CURSOR"}
FILE_TYPES = {"RCDATA": 10, "MESSAGETABLE": 11, "FONT": 8, "HTML": 23, "MANIFEST": 24}


def statements(source):
    """Join wrapped statements and split inline BEGIN/END into separate tokens."""
    pending = []
    line = 0
    continuation = {",", "|", "&", "^", "<<", ">>", "+"}
    for lineno, source_line in enumerate(source.splitlines(), 1):
        if source_line.lstrip().startswith("#"):
            continue
        for token in TOKEN.findall(source_line):
            if token.upper() in {"BEGIN", "END"} or token in ("{", "}"):
                if pending:
                    yield line, pending
                yield lineno, ["BEGIN" if token.upper() == "BEGIN" or token == "{" else "END"]
                pending = []
            elif pending and lineno != line and not (
                pending[-1] in continuation or token in continuation or pending.count("(") > pending.count(")")
            ):
                yield line, pending
                pending = [token]
            else:
                pending.append(token)
            line = lineno
    if pending:
        yield line, pending


def fields(tokens):
    result = [[]]
    level = 0
    for token in tokens:
        if token == "," and level == 0:
            result.append([])
            continue
        level += (token == "(") - (token == ")")
        result[-1].append(token)
    if level:
        raise ValueError("unbalanced parentheses")
    return result


def expression(tokens, default="0"):
    if not tokens:
        return default
    if any(token not in OPERATORS and not NUMBER.fullmatch(token) for token in tokens):
        raise ValueError("expected a numeric expression, got " + " ".join(tokens))
    return "(" + " ".join(tokens) + ")"


def is_number(tokens):
    return bool(tokens) and all(token in OPERATORS or NUMBER.fullmatch(token) for token in tokens)


def rc_string(tokens):
    if not tokens or any(not STRING.fullmatch(token) for token in tokens):
        raise ValueError("expected a quoted string, got " + " ".join(tokens))
    escapes = {"n": "\n", "r": "\r", "t": "\t", "a": "\a", "\\": "\\", '"': '"'}

    def unescape(match):
        value = match[0]
        if value == '""':
            return '"'
        if value[1] in escapes:
            return escapes[value[1]]
        if value.startswith("\\x") and len(value) > 2:
            return chr(int(value[2:], 16))
        if value[1] in "01234567":
            return chr(int(value[1:], 8))
        return value

    return "".join(
        re.sub(r'""|\\(?:x[0-9a-fA-F]{1,4}|[0-7]{1,3}|.)', unescape, token.removeprefix("L")[1:-1])
        for token in tokens
    )


def wide(text):
    encoded = []
    for char in text:
        if char in {'\\', '"'}:
            encoded.append("\\" + char)
        elif ord(char) < 32:
            encoded.append(f"\\{ord(char):03o}")
        else:
            encoded.append(char)
    return 'L"' + "".join(encoded) + '"'


def style(default, tokens):
    # RC combines the default style with the explicit flags, clearing NOT bits.
    set_bits = []
    clear_bits = []
    part = []
    depth = 0
    for token in [*tokens, "|"]:
        if token == "|" and depth == 0:
            if part:
                if part[0].upper() == "NOT":
                    clear_bits.append(expression(part[1:]))
                else:
                    set_bits.append(expression(part))
            part = []
            continue
        part.append(token)
        depth += (token == "(") - (token == ")")
    if depth:
        raise ValueError("unbalanced style expression")
    result = "(" + " | ".join([default, *set_bits]) + ")"
    if clear_bits:
        result += " & ~(" + " | ".join(clear_bits) + ")"
    return f"static_cast<DWORD>({result})"


def resource_name(tokens):
    if is_number(tokens):
        return expression(tokens), "nullptr"
    if tokens and STRING.fullmatch(tokens[0]):
        return "-1", wide(rc_string(tokens))
    if len(tokens) == 1 and tokens[0].isidentifier():
        return "-1", wide(tokens[0])
    raise ValueError("invalid resource identifier " + " ".join(tokens))


class Compiler:
    def __init__(self, source, script):
        self.input = iter(statements(source))
        self.script = script
        self.line = 0
        self.code = ['// Generated by rc2cpp.py', '#include "npp_win32/w32_resources.h"', "", "namespace", "{"]
        self.tables = defaultdict(list)
        self.dependencies = set()
        self.version = "nullptr"
        self.serial = 0

    def next_statement(self):
        self.line, tokens = next(self.input)
        return tokens

    def begin(self):
        if self.next_statement() != ["BEGIN"]:
            raise ValueError("expected BEGIN")

    def block(self):
        self.begin()
        while (tokens := self.next_statement()) != ["END"]:
            yield tokens

    def array(self, typename, rows):
        if not rows:
            return "nullptr", "0"
        self.serial += 1
        name = f"table{self.serial}"
        self.code.append(f"\tconst {typename} {name}[] = {{")
        self.code.extend("\t\t{" + ", ".join(row) + "}," for row in rows)
        self.code.append("\t};")
        return name, str(len(rows))

    def control(self, tokens):
        kind, args = tokens[0].upper(), fields(tokens[1:])
        if kind == "CONTROL":
            title, ctrl_id, cls, flags = args[:4]
            class_name = wide(rc_string(cls))
            coords = args[4:8]
            extra = args[8:]
            default = "0"
        elif kind in CONTROL_DEFAULTS:
            cls, default = CONTROL_DEFAULTS[kind]
            class_name = wide(cls)
            title = [] if kind in TITLELESS_CONTROLS else args.pop(0)
            ctrl_id, *coords = args[:5]
            flags = args[5] if len(args) > 5 else []
            extra = args[6:]
        else:
            raise ValueError("unsupported dialog control " + kind)
        if len(coords) != 4:
            raise ValueError("expected four dialog control coordinates")
        text_id = expression(title) if kind in {"ICON", "CONTROL"} and is_number(title) else "-1"
        text = wide(rc_string(title)) if title and text_id == "-1" else 'L""'
        if kind == "ICON" and is_number(title):
            text = 'L""'
        return [
            class_name, text, text_id, expression(ctrl_id),
            *(expression(coord) for coord in coords),
            style("WS_CHILD | WS_VISIBLE | " + default, flags), style("0", extra[0] if extra else []),
        ]

    def dialog(self, identifier, args):
        coords = fields(args)
        if len(coords) not in (4, 5):
            raise ValueError("expected four dialog coordinates")
        props = {}
        while (tokens := self.next_statement()) != ["BEGIN"]:
            props[tokens[0].upper()] = tokens[1:]
        controls = []
        while (tokens := self.next_statement()) != ["END"]:
            controls.append(self.control(tokens))
        font = fields(props["FONT"]) if "FONT" in props else []
        caption = wide(rc_string(props["CAPTION"])) if "CAPTION" in props else 'L""'
        self.tables["Dialog"].append([
            *identifier, *(expression(coord) for coord in coords[:4]),
            style("0" if "STYLE" in props else "WS_POPUP | WS_BORDER | WS_SYSMENU", props.get("STYLE", [])),
            style("0", props.get("EXSTYLE", [])), caption,
            wide(rc_string(font[1])) if font else 'L""',
            expression(font[0]) if font else "0", expression(font[2], "400") if len(font) > 2 else "400",
            f"bool({expression(font[3])})" if len(font) > 3 else "false",
            expression(props.get("MENU", []), "-1"),
            *self.array("NppRcControl", controls),
        ])

    def menus(self, depth, extended):
        flag_names = {"GRAYED": "MF_GRAYED", "INACTIVE": "MF_DISABLED", "CHECKED": "MF_CHECKED",
                      "MENUBREAK": "MF_MENUBREAK", "MENUBARBREAK": "MF_MENUBARBREAK", "HELP": "MF_HELP"}
        for kind, *tokens in self.block():
            kind = kind.upper()
            if kind == "MENUITEM" and [s.upper() for s in tokens] == ["SEPARATOR"]:
                yield [str(depth), "false", "true", 'L""', "0", "0"]
                continue
            if kind not in {"POPUP", "MENUITEM"}:
                raise ValueError("unsupported menu item " + kind)
            caption, *args = fields(tokens)
            popup = kind == "POPUP"
            item_id = expression(args.pop(0)) if args and (extended or not popup) else "0"
            if extended:
                flags = " | ".join(expression(arg) for arg in args[:2]) or "0"
            else:
                flags = " | ".join(flag_names[token.upper()] for arg in args for token in arg) or "0"
            yield [str(depth), str(popup).lower(), "false", wide(rc_string(caption)), item_id, flags]
            if popup:
                yield from self.menus(depth + 1, extended)

    def accelerators(self):
        options = {"VIRTKEY": "FVIRTKEY", "NOINVERT": "FNOINVERT", "SHIFT": "FSHIFT",
                   "CONTROL": "FCONTROL", "ALT": "FALT", "ASCII": "0"}
        for tokens in self.block():
            key, cmd, *flags = fields(tokens)
            values = [options[token.upper()] for flag in flags for token in flag]
            if key and STRING.fullmatch(key[0]):
                char = rc_string(key)
                if len(char) == 2 and char.startswith("^"):
                    char = char[1].upper()
                    values.extend(["FVIRTKEY", "FCONTROL"])
                if len(char) != 1:
                    raise ValueError("expected one accelerator character")
                key = [str(ord(char.upper() if "FVIRTKEY" in values else char))]
            yield [" | ".join(values) or "0", expression(key), expression(cmd)]

    def version_info(self, first):
        props = {}
        tokens = first or self.next_statement()
        while tokens != ["BEGIN"]:
            props[tokens[0].upper()] = tokens[1:]
            tokens = self.next_statement()

        def version(name):
            parts = fields(props[name]) if name in props else [["0"]] * 4
            if len(parts) != 4:
                raise ValueError("expected four version numbers")
            major, minor, build, revision = (expression(n) for n in parts)
            return [f"MAKELONG({minor}, {major})", f"MAKELONG({revision}, {build})"]

        depth = 1
        strings = []
        while depth:
            tokens = self.next_statement()
            if tokens == ["BEGIN"]:
                depth += 1
            elif tokens == ["END"]:
                depth -= 1
            elif tokens[0].upper() == "VALUE":
                key, *values = fields(tokens[1:])
                if values and values[0] and STRING.fullmatch(values[0][0]):
                    strings.append(["0", wide(rc_string(key) + "\0" + rc_string(values[0]))])
        row = [*(f"static_cast<DWORD>({v})" for v in version("FILEVERSION") + version("PRODUCTVERSION")),
               *self.array("NppRcString", strings)]
        if self.version != "nullptr":
            raise ValueError("multiple VERSIONINFO resources are unsupported")
        self.code.append("\tconst NppRcVersion version = {" + ", ".join(row) + "};")
        self.version = "&version"

    def file_resource(self, identifier, kind, args):
        if len(args) != 1 or not STRING.fullmatch(args[0]):
            raise ValueError(f"expected a file for the {kind} resource")
        path = args[0].removeprefix("L")[1:-1].replace("\\\\", "\\").replace("\\", "/")
        filename = (self.script.parent / path).resolve()
        self.dependencies.add(filename)
        data = filename.read_bytes()
        self.serial += 1
        name = f"bytes{self.serial}"
        self.code.append(f"\tconst unsigned char {name}[] = {{")
        for start in range(0, len(data), 20):
            self.code.append("\t\t" + ", ".join(str(b) for b in data[start:start + 20]) + ",")
        if not data:
            self.code.append("\t\t0,")
        self.code.append("\t};")
        type_name = "nullptr"
        if kind in IMAGE_TYPES:
            type_id = IMAGE_TYPES[kind]
        elif kind in FILE_TYPES:
            type_id = str(FILE_TYPES[kind])
        elif NUMBER.fullmatch(kind):
            type_id = kind
        else:
            type_id = "-1"
            type_name = wide(rc_string([kind]) if STRING.fullmatch(kind) else kind)
        self.tables["Data"].append([type_id, type_name, *identifier, name, str(len(data))])

    def compile(self):
        try:
            for self.line, tokens in self.input:
                if tokens[0].upper() in RESOURCE_METADATA:
                    continue
                if tokens[0].upper() == "STRINGTABLE":
                    for row in self.block():
                        split = next(i for i, token in enumerate(row) if STRING.fullmatch(token))
                        key = expression(row[:split - 1] if row[split - 1] == "," else row[:split])
                        self.tables["String"].append([key, wide(rc_string(row[split:]))])
                    continue

                end_id = 1
                if tokens[0] == "(":
                    depth = 1
                    while depth:
                        depth += (tokens[end_id] == "(") - (tokens[end_id] == ")")
                        end_id += 1
                identifier = resource_name(tokens[:end_id])
                kind = tokens[end_id].upper()
                args = tokens[end_id + 1:]
                while args and args[0].upper() in RESOURCE_FLAGS:
                    args.pop(0)
                if kind in {"DIALOG", "DIALOGEX"}:
                    self.dialog(identifier, args)
                elif kind in {"MENU", "MENUEX"}:
                    if kind == "MENUEX" and args:
                        raise ValueError("unexpected MENUEX header")
                    items = self.array("NppRcMenuItem", list(self.menus(0, kind == "MENUEX")))
                    self.tables["Menu"].append([*identifier, *items])
                elif kind == "ACCELERATORS":
                    keys = self.array("NppRcAccel", list(self.accelerators()))
                    self.tables["AccelTable"].append([*identifier, *keys])
                elif kind == "VERSIONINFO":
                    self.version_info(args)
                else:
                    self.file_resource(identifier, kind, args)
        except (IndexError, KeyError, OSError, StopIteration, ValueError) as exc:
            detail = str(exc) or "incomplete resource statement"
            raise ValueError(f"{self.script} (preprocessed line {self.line}): {detail}") from exc

        tables = []
        for kind in ("Dialog", "String", "Menu", "AccelTable", "Data"):
            tables.extend(self.array("NppRc" + kind, self.tables[kind]))
        self.code.extend([
            "\tconst NppRcResources resources = {" + ", ".join([*tables, self.version]) + "};",
            "\tstruct RegisterResources",
            "\t{",
            "\t\tRegisterResources() { nppRegisterResources(&resources); }",
            "\t};",
            "\tconst RegisterResources registerResources;",
            "}",
            "",
        ])
        return "\n".join(self.code)


def make_escape(path):
    return str(path).replace("$", "$$").replace("#", "\\#").replace(" ", "\\ ")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", required=True, help="GCC- or Clang-compatible C compiler")
    parser.add_argument("--include", type=Path, action="append", default=[])
    parser.add_argument("--depfile", type=Path, required=True)
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    command = [
        args.compiler, "-E", "-P", "-x", "c", "-DRC_INVOKED",
        "-MMD", "-MF", str(args.depfile), "-MQ", str(args.output),
        *(f"-I{directory}" for directory in args.include), str(args.input),
    ]
    try:
        stdin = None
        with args.input.open("rb") as source:
            if source.read(2) in (b"\xff\xfe", b"\xfe\xff"):
                source.seek(0)
                stdin = source.read().decode("utf-16")
                command[-1:] = ["-iquote", str(args.input.resolve().parent), "-"]
        preprocessed = subprocess.run(command, input=stdin, check=True, stdout=subprocess.PIPE, encoding="utf-8").stdout
        compiler = Compiler(preprocessed, args.input)
        code = compiler.compile()
        if compiler.dependencies:
            with args.depfile.open("a", encoding="utf-8") as depfile:
                assets = " ".join(make_escape(path) for path in sorted(compiler.dependencies))
                depfile.write(f"{make_escape(args.output)}: {assets}\n")
        args.output.write_text(code, encoding="utf-8")
    except (subprocess.CalledProcessError, UnicodeError, ValueError, OSError) as exc:
        parser.exit(1, f"rc2cpp: {exc}\n")


if __name__ == "__main__":
    main()
