# Linux port: feature parity with Windows Notepad++

What the Qt port does and does not do yet, compared with the Windows build of
the same sources. Update this file when a gap is closed.

Regenerate the menu numbers with (needs `-DNPP_BUILD_TESTS=ON`):

```bash
cmake --build build-linux --target npp_menu_coverage
```

```bash
QT_QPA_PLATFORM=offscreen ./build-linux/bin/npp_menu_coverage
```

"Implemented" means the menu command has a handler. The self test
(`npp_selftest`) covers the main editor and plugin paths.

## Same as Windows

The editing core is the same code: Scintilla, Lexilla, the Notepad++ lexers and
the boost regex search engine. Highlighting, folding and regex behaviour match.
Menus, default shortcuts and the language list are generated from the Windows
sources at build time. Languages, themes, translations and auto-completion APIs
are the shipped XML files.

## Menu commands (2026-11-21)

478 of 482 implemented.

| Menu | Implemented |
|---|---|
| File | 30 / 30 |
| Edit | 104 / 104 |
| Search | 66 / 66 |
| View | 80 / 82 |
| Encoding | 56 / 56 |
| Language | 98 / 98 |
| Settings | 6 / 6 |
| Tools | 12 / 12 |
| Macro | 5 / 5 |
| Run | 2 / 2 |
| Plugins | 1 / 1 |
| Window | 11 / 11 |
| ? (Help) | 7 / 9 |

Not implemented (greyed out in the menus):

- **View:** Text Direction RTL / LTR (Scintilla has no bidirectional text on Qt)
- **? (Help):** Update Notepad++, Set Updater Proxy

## Dialogs and other gaps

- **Preferences:** all 24 pages, see the next section.
- **Shortcut Mapper:** Main menu and Scintilla commands tabs; the saved macros and
  Run commands are modified or deleted from Macro > Modify Shortcut/Delete Macro...
  and Run > Modify Shortcut/Delete Command.... The Scintilla commands that have a
  menu item are set in the Main menu tab (Windows lists them in both).
- **Translations:** the menus are translated (`nativeLang` files); the dialogs are English only.
- **Settings files:** `session.xml`, `langs.xml`, `stylers.xml` and themes use the
  Windows format. Other settings live in `~/.config/notepad++/config.ini`
  (QSettings), so the Windows `config.xml` is not read; of `shortcuts.xml` the
  `<Macros>`, `<UserDefinedCommands>` and `<ScintillaKeys>` parts are used (the menu
  shortcuts of the Main menu tab are in config.ini). `contextMenu.xml` works like on Windows.
- **Plugins:** the Linux SDK loads native modules. Bundled plugins and the
  compatibility coverage are listed in [PLUGINS_STATUS.md](PLUGINS_STATUS.md).
- **Windows only, not planned:** shell extension, updater, running as administrator.

## Preferences (Windows: 24 pages, about 260 controls): done

All 24 pages are there (`src/Preferences.cpp`) with the Windows options, and like
Notepad++ every change applies at once (the dialog only has Close). Settings are
stored in `~/.config/notepad++/config.ini`; the keys of older port versions are
migrated. Checked by the `[preferences]` part of the self test (one check per
area, screenshots of every page).

| Page | Status | Notes |
|---|---|---|
| General | ✅ | Hidden menu bar shows with Alt or F10; ＋ ▼ ✕ buttons in the menu bar corner; dialog font after restart |
| Toolbar | ✅ | Fluent small/large, filled small/large, standard icons; complete/partial colorization in 9 colours, system accent, custom |
| Tab Bar | 🟡 | All but **Multi-line**: the Qt tab bar has one row (scroll buttons instead), the option is greyed out |
| Editing 1 | 🟡 | All but **Disable advanced scrolling** (a Windows touchpad workaround, greyed out) |
| Editing 2 | ✅ | Multi-editing, CR LF appearance, non-printing characters, C0 typing |
| Dark Mode | ✅ | Light, dark, follow system; 7 tones and 12 customized colours as the application palette; switches to the dark or light default theme |
| Margins/Border/Edge | ✅ | Several vertical edges, background mode, padding, distraction free width, border width and edge |
| New Document | ✅ | Line ending, encoding (also other code pages), ANSI as UTF-8, default language, first line as tab name |
| Default Directory | ✅ | |
| Recent Files History | ✅ | Check at launch, submenu, file name / full path / custom length |
| File Association | ✅ | Linux equivalent: xdg-mime default for your user (`~/.config/mimeapps.list`); unknown extensions get a user MIME type |
| Language | ✅ | Compact or flat Language menu, hidden languages, SQL backslash |
| Indentation | ✅ | Per language (stored in config.ini, over langs.xml), backspace unindent, auto-indent none/basic/advanced |
| Highlighting | ✅ | Matching tags (port of xmlMatchedTagsHighlighter), mark all, smart highlighting, also in the other view of split view. "Highlight comment/php/asp zone" is stored but, as in the Windows code, has no effect |
| Print | ✅ | Colour mode, margins, header and footer with variables and fonts, line numbers, form feed page breaks |
| Searching | ✅ | |
| Backup | ✅ | Remember inaccessible files, backup on save (simple, verbose, custom folder) |
| Auto-Completion | ✅ | Brief list, TAB/ENTER, ignore numbers, matched pairs, html/xml close tag (port of AutoCompletion::insertMatchedChars) |
| Multi-Instance & Date | 🟡 | Modes and date/time formats work. Plugin panel state remains pending for this mode. |
| Delimiter | ✅ | Word characters, Ctrl+double click selection |
| Performance | ✅ | Large file restriction and its options, 2 GB warning |
| Cloud & Link | ✅ | Settings folder from cloud/choice like Notepad++ (restart needed; session and backups stay local); link styles and URI schemes |
| Search Engine | ✅ | |
| MISC. | 🟡 | All but **rendering mode** (Windows GDI/DirectWrite), greyed out. System tray, Ctrl+Tab switcher with MRU, peek on tab and peek on document map, workspace file extension work |

The options still greyed out are Windows-only or need the Qt tab bar to have
several rows.

## Split view: done (2026-11-20)

Two views side by side or one above the other (right click on the splitter to
rotate; the orientation is remembered), each with its own tab bar, like Notepad++:

- View > Move/Clone Current Document > Move to Other View / Clone to Other View,
  also in the tab context menu, and by dragging a tab to the other view (Ctrl: clone).
  A clone is the same document: edits show in both views, each view keeps its own
  caret and scroll position. Closing a clone only removes that tab.
- A view without documents disappears; the last document of the last view is
  replaced by a new document in the main view.
- View > Focus on Another View (F8); clicking in a view or on its tab bar makes it
  the active one (title, status bar, menus and commands follow it). The coloured
  bar of the active tab uses the "focused" colour in the active view.
- View > Synchronize Vertical / Horizontal Scrolling, View > Zoom > Synchronize
  Across Views (remembered), Smart Highlighting > Highlight another view.
- session.xml has mainView and subView like Notepad++ (clones are restored as one
  document); the Document List shows both views ("1" / "2"); Close All closes both
  views, Close All to the Left / Right only the active view.

Not done: dropping a tab outside the window to move it to a new instance (the tab
context menu has Move to New Instance).

## Document Map and Function List: done (2026-11-20)

Both are dock panels on the right (tabbed together like the Windows default
container), opened from View or the toolbar, remembered at exit (and in
-nosession / multi-instance mode with Preferences > Multi-Instance > Panel State).

- **Document Map** (`src/DocumentMap.cpp`, port of documentMap.cpp): the document
  of the active view at zoom -10, the visible part of the editor marked with the
  "Document map" colours of the theme at 50/256 opacity. Click or drag to move the
  marked zone, the wheel and Up / Down / Page Up / Page Down scroll the editor.
  Follows scrolling, folding, word wrap (wraps at the same columns), zoom, the
  active view of split view, language and theme changes. MISC. > Peek on document
  map shows the document of a hovered tab.
- **Function List** (`src/FunctionParser.cpp` is the Windows functionParser.cpp,
  `src/FunctionList.cpp` the panel): the shipped `functionList/*.xml` rules and
  `overrideMap.xml`; a user `~/.config/notepad++/functionList/` folder comes first.
  Class nodes, search filter, sort (and "sort by default" in its menu), reload,
  the entry of the caret is selected, double click / Enter goes to the function.
  Parsed again on document switch, language change and after saving (like
  Windows, not while typing). The search, sort and folded nodes are kept per file.

Like Windows, the Function List, Document List and Folder as Workspace lists
take the colours of the theme's Default Style.

## Edit panels, Paste Special, Find characters in range, Incremental Search: done (2026-11-20)

- **Edit > Character Panel** (`src/EditPanels.cpp`, port of ansiCharPanel / asciiListView):
  the 256 characters of the code page of the document with the HTML name, decimal
  and hexadecimal columns (for ANSI / Windows-1252); double click inserts the
  character or the text of the cell, Enter the character.
- **Edit > Clipboard History** (clipboardHistoryPanel): every text copied while the
  panel exists (in Notepad++ or another program), newest first, a copy already in
  the list moves to the top; double click or Enter inserts it. Binary copies are
  kept with their NUL characters.
- **Edit > Paste Special:** Copy / Cut / Paste Binary Content use a private MIME
  type with the exact bytes (CF_NPPTEXTLEN on Windows), with the text before the
  first NUL as plain text for other programs. Paste HTML / RTF Content insert the
  HTML or RTF source of the clipboard.
- **Search > Find characters in range...** (FindCharsInRange): modeless dialog,
  non-ASCII, ASCII or a custom byte range, up / down, wrap around. Like Windows it
  looks at the bytes of the document (UTF-8 here).
- **Search > Incremental Search** (FindIncrementDlg): a bar under the editor (˄ / ˅
  moves it above), searches while typing from the start of the selection, < / >,
  Enter / Shift+Enter, F3 / Shift+F3, Match case, Highlight all, Count ("2/5"),
  wrap messages, red field when the phrase is not found, Escape closes it. The
  options and the position are remembered.

## Hide Lines: done (2026-11-20)

View > Hide Lines hides the selected lines (not the first and last line of the
document) between two markers in the bookmark margin, like ScintillaEditView::hideLines():
a section touching another one's marker lines joins it, a click on a marker shows
the section again and removes its markers. The markers belong to the document,
so the other view of a clone and a document shown again restore the hidden lines.

## Saved macros: done (2026-11-20)

Macro > Save Current Recorded Macro... asks for a name and a shortcut (Ctrl / Alt
/ Shift + key, the Windows Shortcut dialog) and stores the macro in `<Macros>` of
`~/.config/notepad++/shortcuts.xml`, in the Windows format (Action types 0, 1 and
2, Windows key codes), so the macros of a Windows `shortcuts.xml` work when the
file is copied there; the other parts of the file are kept. Without the file the
default "Trim Trailing Space and Save" (Alt+Shift+S) is there, as in Notepad++.
Saved macros are in the Macro menu (FolderName gives a submenu) with their
shortcuts, in Run a Macro Multiple Times, and Modify Shortcut/Delete Macro...
renames, changes the shortcut or deletes them. A recorded new line is stored as
SCI_NEWLINE like Windows, so it inserts the line ending of the document.
Find / Replace steps (Action type 3) are skipped: the port does not record the
Find dialog.

OEM 720 (Encoding > Character sets > Arabic): glibc iconv has no CP720, so the
port converts it with its own table (`src/FileIO.cpp`).

## Editor context menu from contextMenu.xml: done (2026-11-20)

The right click menu of the editor is built from `<ScintillaContextMenu>` of
`~/.config/notepad++/contextMenu.xml`, or from the default of Notepad++
(CONTEXTMENU_XML_CONTENT of NppConstants.h, extracted at build time): `id`,
`MenuEntryName` + `MenuItemName` (English menu names), `ItemNameAs`, `FolderName`
(with `TranslateID`), `id="0"` separators; plugin items are skipped. Settings >
Edit Popup ContextMenu creates the file from the default and opens it; the menu
follows the file the next time it opens (Windows needs a restart).

## Project Panels: done (2026-11-20)

View > Project Panels > Project Panel 1-3 (`src/ProjectPanel.cpp`, port of
WinControls/ProjectPanel): docked on the left, tabbed together. Workspace and Edit
menus and the context menus of the workspace, project, folder and file nodes:
New / Open / Reload / Save / Save As / Save a Copy As workspace, Add New Project,
Find in Projects, Move Up / Down (Ctrl+Up / Down), Rename (F2), Add Folder, Add
Files, Add Files from Directory (hidden folders skipped), Remove (DEL), Modify File
Path. Drag and drop moves files and folders into projects and folders. Double click
or Enter opens a file; missing files have the "invalid" icon; the workspace icon
shows unsaved changes, and a modified workspace is offered for saving when the
panel or Notepad++ closes. Workspace files are the Notepad++ XML (paths relative to
the workspace file; the backslashes of Windows workspace files are understood) and
MISC. > Workspace file ext. is the file dialog filter. Each panel reopens its
workspace. The Find dialog has the Find in Projects tab (Project Panel 1-3 check
boxes and the filters), also opened by Find in Projects... of a panel.

## User Defined Languages: done (2026-11-20)

`src/Udl.cpp` reads `userDefineLang.xml` and `userDefineLangs/*.xml` of the settings
folder (and an installed `userDefineLangs` folder), all UDL versions, and applies
them with the Notepad++ UDL lexer like ScintillaEditView::setUserLexer(): keywords
(quoted words too), comments, delimiters, folding, colours, nesting. The languages
are at the end of the Language menu, chosen by their extensions first (dark/light
UDL of the current mode preferred), kept in session.xml by name, shown in the
status bar, and the Function List uses their overrideMap.xml parsers. User-Defined
and Open User Defined Language folder work.

Define your language... (`src/UdlDialog.cpp`, port of UserDefineDialog.cpp): the
language list (User Defined Language and the saved ones), Create new, Save as,
Rename, Remove, Import, Export, extensions, Ignore case, and the four tabs: Folder &
Default (folding in code 1 / 2 / comment, fold compact), Keywords Lists (8 groups,
prefix mode), Comment & Number (line comment position, comment folding, comment
line and block delimiters, number prefixes / extras / suffixes / range, decimal
separator), Operators & Delimiters (operators 1 / 2, 8 delimiters with escape).
Each Styler button opens the Styler dialog (font, size, bold / italic / underline,
colours or transparent, nesting for comments and delimiters). The documents in the
language follow every change; the comment and delimiter lists use the Notepad++
encoding ("00//", (( )) groups). Changed languages are written when the dialog
closes and at exit, each into the file it came from (new ones get a file in
userDefineLangs; an installed one is copied there). Not ported: docking the dialog
and its transparency slider.

## Run commands and shortcuts.xml validation: done (2026-11-20)

Run > Run... is the Windows dialog: history, "..." to browse, "+" for the
$(VARIABLES), Run and Save.... Save... stores the command with a name and shortcut
in `<UserDefinedCommands>` of shortcuts.xml; the commands are in the Run menu (the
Notepad++ defaults "Get PHP help", "Wikipedia Search", "Open selected file path in
new instance" when there is no file), with Modify Shortcut/Delete Command....
Like ShellExecute, an address opens in the browser, a document with its program,
anything else runs in /bin/sh, in the folder of the current document.

As on Windows, the saved macros and commands run only while shortcuts.xml is the
file Notepad++ wrote or validated: its HMAC (keyed with /etc/machine-id) is kept in
config.ini. A file changed by hand is opened read-only for review with a warning,
and Run > Validate shortcuts.xml accepts it.

## Scintilla commands: done (2026-11-20)

The editor keys are the Notepad++ list (scintKeyDefs of Parameters.cpp, extracted
at build time), applied like ScintillaAccelerator::updateKeys(): the key map of
both views is cleared and rebuilt from the list. Settings > Shortcut Mapper >
Scintilla commands modifies, clears or resets them (with conflict check); the
changed ones are written to `<ScintillaKeys>` of shortcuts.xml in the Windows
format (ScintKey / NextKey), so a Windows shortcuts.xml brings its key changes.

## What is left (2026-11-21)

- Menu: View > Text Direction RTL / LTR (Windows mirrors the edit window through
  GDI; Scintilla's Qt platform has no equivalent without changing scintilla/)
  and the Windows updater commands.
- The dialogs are not translated (the menus are).
- Multi-line tab bar, docking of the UDL dialog and its transparency.
