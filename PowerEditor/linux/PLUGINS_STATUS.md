# Plugins on Linux

The **Linux plugin SDK** (`plugin-sdk/`) builds shared libraries
`plugins/<Name>/<Name>.so` with the exports of `PluginInterface.h`
(`setInfo`, `getName`, `getFuncsArray`, `beNotified`, `messageProc`, `isUnicode`).
Windows modules need a source port to use this interface on Linux.

## Bundled modules

| Module | Fedora coverage |
|---|---|
| MIME Tools | Conversions and clipboard commands |
| Converter | ASCII/hex commands and the resource-backed conversion panel |
| Auto Detect Indention | Tab and space detection for the active document |
| Diff Checker | Qt comparison of tabs and files, using the ComparePlus line diff engine |
| JSONTools | Upstream parser, RemesPath and schema engines with a Qt frontend and .NET 10 runtime |

## How it works

- **Host** (`src/PluginsManager.cpp`): loads the plugins of `<prefix>/lib/notepad++/plugins`,
  `~/.local/share/notepad++/plugins` and `$NPP_PLUGINS_DIR`, builds the Plugins menu
  (shortcuts of `FuncItem`, check marks), forwards the `NPPN_` and `SCN_` notifications
  and answers `NPPM_` messages, including docking
  (`NPPM_DMMREGASDCKDLG`), toolbar icons, `NPPM_MSGTOPLUGIN`, `NPPM_CREATESCINTILLAHANDLE`.
  `-noPlugin` and `-pluginMessage=` work; Settings > Import > Import plugin(s)... and
  Plugins > Open Plugins Folder... work.
- **SDK** (`plugin-sdk/include`): a `windows.h` for Linux, so the original
  `PluginInterface.h`, `Notepad_plus_msgs.h`, `Docking.h` and `menuCmdID.h` (and the copies
  plugins carry) compile unchanged. Handles are `NppHandle` objects; `SendMessage(nppData._nppHandle, NPPM_...)`
  and `SendMessage(scintilla, SCI_...)` work as on Windows. Strings are `wchar_t` (UTF-32
  on Linux). `NppPluginQt.h` has the Qt helpers for plugin user interfaces.
- **Build**: `npp_add_plugin(<Name> [DLLMAIN] sources...)` in `plugins/<Name>/CMakeLists.txt`.
  Add `RC <file.rc>...` after the C++ sources to compile resources with `tools/rc2cpp.py`.
  It supports standard dialogs and controls, menus, strings, accelerators, version
  information and file-backed resources from UTF-8 and BOM-marked UTF-16 scripts.
  Add `DLLMAIN` to call `DllMain` with `DLL_PROCESS_ATTACH` after the module is loaded.
  The compatibility library is private to each module and implements a subset of
  Win32 with Qt and POSIX. Use `NppPluginQt.h` for new plugin UI code.
- **Sources**: `plugins-src/` (not in the port tree) has shallow clones of the 203
  repositories of the plugin list (`nppPluginList/src/pl.x64.json`, 231 plugins).
  A ported plugin copies its sources into `plugins/<Name>/` with `#ifdef _WIN32` around
  what Linux does differently.
- **Self test**: `tests/TestPlugin.cpp` and the `[plugins]` section of `tests/selftest.cpp`.

## Summary

| | Plugins |
|---|---:|
| Ported | 3 |
| Core port with Qt frontend (JSONTools) | 1 |
| C/C++, to do | 135 |
| C# / .NET (WinForms): need a Qt frontend | 48 |
| Delphi / Free Pascal | 8 |
| Rust, D, Python | 6 |
| No source code available | 30 |

## Plugins of the list (x64)

| Plugin | Folder | Source | Language | Lines | Status |
|---|---|---|---|---:|---|
| 3P - Progress Programmers Pal | 3P | [jcaillon/3P](https://github.com/jcaillon/3P) | C# | 112490 | C# / .NET WinForms: needs a C++/Qt rewrite |
| ActiveX Plugin | ActiveX | https://sourceforge.net/projects/nppactivexplugin/ | - | 0 | No source on GitHub (binary on SourceForge or a web site) |
| ActivityWatch | AwWatcherNPP | [pytlus93/aw-watcher-notepadpp](https://github.com/pytlus93/aw-watcher-notepadpp) | C# | 890 | C# / .NET WinForms: needs a C++/Qt rewrite |
| AnalysePlugin | AnalysePlugin | https://sourceforge.net/projects/analyseplugin | - | 0 | No source on GitHub (binary on SourceForge or a web site) |
| AndroidLogger | AndroidLogger | [glandon/AndroidLogger](https://github.com/glandon/AndroidLogger) | C++ | 4854 | To do |
| Another Markdown | AnotherMarkdown | [ezyuzin/NppAnotherMarkdown](https://github.com/ezyuzin/NppAnotherMarkdown) | C# | 20447 | C# / .NET WinForms: needs a C++/Qt rewrite |
| Auto Detect Indention | nppAutoDetectIndent | [Chocobo1/nppAutoDetectIndent](https://github.com/Chocobo1/nppAutoDetectIndent) | C++ | 768 | **Ported** |
| AutoCodepage | AutoCodepage | https://sourceforge.net/projects/autocodepage | - | 0 | No source on GitHub (binary on SourceForge or a web site) |
| AutoEolFormat | AutoEolFormat | https://sourceforge.net/projects/autoeolformat | - | 0 | No source on GitHub (binary on SourceForge or a web site) |
| Automation Scripts | NppScripts | [oleg-shilo/scripts.npp](https://github.com/oleg-shilo/scripts.npp) | C# | 24215 | C# / .NET WinForms: needs a C++/Qt rewrite |
| AutoSave | AutoSave | [francostellari/NppPlugins](https://github.com/francostellari/NppPlugins) | ? | 0 | No source in the repository |
| BAT Path IntelliSense | BatPathIntelliSense | [nicehero/NppBatPathIntelliSense](https://github.com/nicehero/NppBatPathIntelliSense) | C++ | 4987 | To do |
| BetterMultiSelection | BetterMultiSelection | [dail8859/BetterMultiSelection](https://github.com/dail8859/BetterMultiSelection) | C++ | 5073 | To do |
| BigFiles - Open Very Large Files | BigFiles | [superolmo/BigFiles](https://github.com/superolmo/BigFiles) | C++ | 6200 | To do |
| Blitz Search | BlitzSearch | [Natestah/BlitsNppPlugin](https://github.com/Natestah/BlitsNppPlugin) | C++ | 412 | To do |
| Bookmarks@Dook | BookmarksDook | [Dook1/Bookmarks-Dook](https://github.com/Dook1/Bookmarks-Dook) | ? | 0 | No source in the repository |
| BracketsCheck | BracketsCheck | [niccord/BracketsCheck](https://github.com/niccord/BracketsCheck) | C# | 3041 | C# / .NET WinForms: needs a C++/Qt rewrite |
| Bunyan Log Viewer | BunyanLogViewer | [jayapalb/notepad-bunyan-plugin](https://github.com/jayapalb/notepad-bunyan-plugin) | C++ | 1411 | To do |
| CADdyTools | CADdyTools | [MarioRosi/CADdyTools](https://github.com/MarioRosi/CADdyTools) | C# | 26094 | C# / .NET WinForms: needs a C++/Qt rewrite |
| CalcStudio for Oracle EPM | CalcStudioEPM | [rlorenzoni/CalcStudioEPM](https://github.com/rlorenzoni/CalcStudioEPM) | C# | 24514 | C# / .NET WinForms: needs a C++/Qt rewrite |
| Chinese Converter | NppChineseConverter | [lkk9898969/Npp_ChineseConverter](https://github.com/lkk9898969/Npp_ChineseConverter) | C++ | 6091 | To do |
| Code Alignment | CodeAlignmentNpp | [cpmcgrath/codealignment](https://github.com/cpmcgrath/codealignment) | C# | 6600 | C# / .NET WinForms: needs a C++/Qt rewrite |
| Code::Stats | CodeStats | [p0358/notepadpp-CodeStats](https://github.com/p0358/notepadpp-CodeStats) | C# | 17786 | C# / .NET WinForms: needs a C++/Qt rewrite |
| CollectionInterface | CollectionInterface | [pryrt/NppPlugin-CollectionInterface](https://github.com/pryrt/NppPlugin-CollectionInterface) | C++ | 32725 | To do |
| Color Picker | NPPColorPicker | [dasu88/NPPColorPicker](https://github.com/dasu88/NPPColorPicker) | C++ | 647 | To do |
| Columns++ | ColumnsPlusPlus | [Coises/ColumnsPlusPlus](https://github.com/Coises/ColumnsPlusPlus) | C++ | 264868 | To do |
| ColumnTools | ColumnTools | [vinsworldcom/nppColumnTools](https://github.com/vinsworldcom/nppColumnTools) | C++ | 4284 | To do |
| Comment Toggler | CommentToggler | [ScienceDiscoverer/CommentToggler](https://github.com/ScienceDiscoverer/CommentToggler) | C++ | 1461 | To do |
| Comment Wrap | Comment Wrap | https://sourceforge.net/projects/kered13-notepad-plugins/ | - | 0 | No source on GitHub (binary on SourceForge or a web site) |
| Compare | ComparePlugin | [pnedev/compare-plugin](https://github.com/pnedev/compare-plugin) | C++ | 54344 | To do |
| ComparePlus | ComparePlus | [pnedev/comparePlus](https://github.com/pnedev/comparePlus) | C++ | 54344 | To do |
| Compose | Compose | [Coises/Compose-for-NotepadPlusPlus](https://github.com/Coises/Compose-for-NotepadPlusPlus) | C++ | 38371 | To do |
| CompressedFileViewer | CompressedFileViewer | [Pascal-Krenckel/CompressedFileViewer](https://github.com/Pascal-Krenckel/CompressedFileViewer) | C# | 15873 | C# / .NET WinForms: needs a C++/Qt rewrite |
| CS-Script - C# Intellisense | CSScriptNpp | [oleg-shilo/cs-script.npp](https://github.com/oleg-shilo/cs-script.npp) | C# | 114270 | C# / .NET WinForms: needs a C++/Qt rewrite |
| CSV Lint | CSVLint | [BdR76/CSVLint](https://github.com/BdR76/CSVLint) | C# | 32180 | C# / .NET WinForms: needs a C++/Qt rewrite |
| CSV Visual Editor | CsvVisualEditor | [zzsolt/notepadplusplus-csv-visual-editor](https://github.com/zzsolt/notepadplusplus-csv-visual-editor) | C# | 16990 | C# / .NET WinForms: needs a C++/Qt rewrite |
| CsvQuery | CsvQuery | [jokedst/CsvQuery](https://github.com/jokedst/CsvQuery) | C# | 141124 | C# / .NET WinForms: needs a C++/Qt rewrite |
| Customize Toolbar | _CustomizeToolbar | https://sourceforge.net/projects/npp-customize | - | 0 | No source on GitHub (binary on SourceForge or a web site) |
| CustomLineNumbers | CustomLineNumbers | https://sourceforge.net/projects/customlinenumbers | - | 0 | No source on GitHub (binary on SourceForge or a web site) |
| Dark Mode C | dark_mode_C | [pullitall/Notepadpp-light-dark-switch-and-automatic-switch-based-on-location](https://github.com/pullitall/Notepadpp-light-dark-switch-and-automatic-switch-based-on-location) | C++ | 3550 | To do |
| Datamodder File Tools | FileTools | [dm-utils/filetools](https://github.com/dm-utils/filetools) | C++ | 2973 | To do |
| Datamodder Query Files | QueryFiles | [dm-utils/queryfiles](https://github.com/dm-utils/queryfiles) | C++ | 2907 | To do |
| Datamodder Regex Viewer | RegexViewer | [dm-utils/regexviewer](https://github.com/dm-utils/regexviewer) | C++ | 4437 | To do |
| Datamodder SQL Formatter | FormatSQL | [dm-utils/formatsql](https://github.com/dm-utils/formatsql) | C++ | 5156 | To do |
| DBGp | dbgpPlugin | [rdipardo/dbgpPlugin](https://github.com/rdipardo/dbgpPlugin) | Pascal | 1510 | Delphi / Free Pascal: needs a rewrite |
| Discord Rich Presence | DiscordRPC | [Zukaritasu/notepadpp_rpc](https://github.com/Zukaritasu/notepadpp_rpc) | C++ | 31119 | To do |
| DoxyIt | DoxyIt | [dail8859/DoxyIt](https://github.com/dail8859/DoxyIt) | C++ | 7417 | To do |
| DraftSync | DraftSync | [belarrem/DraftSync](https://github.com/belarrem/DraftSync) | C# | 17329 | C# / .NET WinForms: needs a C++/Qt rewrite |
| DSpellCheck | DSpellCheck | [Predelnik/DSpellCheck](https://github.com/Predelnik/DSpellCheck) | C++ | 12798 | To do |
| Dual Scrollbars | DualScrollbars | [SaiyajinK/Dual-Scrollbars-for-Notepad-Plus-Plus](https://github.com/SaiyajinK/Dual-Scrollbars-for-Notepad-Plus-Plus) | C++ | 2529 | To do |
| EditorConfig | NppEditorConfig | [editorconfig/editorconfig-notepad-plus-plus](https://github.com/editorconfig/editorconfig-notepad-plus-plus) | C++ | 3844 | To do |
| ElasticTabstops | ElasticTabstops | [mariusv-github/ElasticTabstops](https://github.com/mariusv-github/ElasticTabstops) | C++ | 10068 | To do |
| Emoji Description | EmojiDescription | [Ruberoid/npp_emoji_description](https://github.com/Ruberoid/npp_emoji_description) | C++ | 6200 | To do |
| EnhanceAnyLexer | EnhanceAnyLexer | [Ekopalypse/EnhanceAnyLexer](https://github.com/Ekopalypse/EnhanceAnyLexer) | C++ | 9 | To do |
| ERP Helper | ERPHelper | [swhitley/ERPHelper](https://github.com/swhitley/ERPHelper) | C# | 20824 | C# / .NET WinForms: needs a C++/Qt rewrite |
| Explorer | Explorer | [oviradoi/npp-explorer-plugin](https://github.com/oviradoi/npp-explorer-plugin) | C++ | 17155 | To do |
| Expression calculator | PlanetCNCNpp64 | [PlanetCNC/PlanetCNCNpp](https://github.com/PlanetCNC/PlanetCNCNpp) | ? | 0 | No source in the repository |
| ExtSettings | ExtSettings | https://sourceforge.net/projects/extsettings | - | 0 | No source on GitHub (binary on SourceForge or a web site) |
| F# Interactive | NPPFSIPlugin | [rdipardo/nppFSIPlugin](https://github.com/rdipardo/nppFSIPlugin) | Pascal | 8056 | Delphi / Free Pascal: needs a rewrite |
| FileFinder | FileFinder | [ufo/filefinder](https://github.com/ufo/filefinder) | C# | 5745 | C# / .NET WinForms: needs a C++/Qt rewrite |
| FingerText2 | FingerText2 | [ultimatejimmy/FingerText2](https://github.com/ultimatejimmy/FingerText2) | C++ | 227288 | To do |
| Fixed-width Data Visualizer | FWDataViz | [shriprem/FWDataViz](https://github.com/shriprem/FWDataViz) | C++ | 17073 | To do |
| FixParser | FixParser | [chtaylo3/fix-parser](https://github.com/chtaylo3/fix-parser) | C++ | 2283 | To do |
| folderpad++ | folderpad++ | [jks-liu/folderpad-plus-plus](https://github.com/jks-liu/folderpad-plus-plus) | Python | 1256 | Python: needs a rewrite or a Python binding of the SDK |
| Folding Line Hider | FoldingLineHider | [leonardchai/FoldingLineHider](https://github.com/leonardchai/FoldingLineHider) | C++ | 3364 | To do |
| GDI Emoji | NppGdiEmoji | [seantw/NppGdiEmoji](https://github.com/seantw/NppGdiEmoji) | C++ | 1580 | To do |
| GEDCOM Lexer | GedcomLexer | https://sourceforge.net/projects/gedcomlexer/ | - | 0 | No source on GitHub (binary on SourceForge or a web site) |
| GitHub Repo Viewer | GitHubRepoViewer | [ufoptg/GitHubRepoViewer](https://github.com/ufoptg/GitHubRepoViewer) | C++ | 4568 | To do |
| GitSCM | GitSCM | [vinsworldcom/nppGitSCM](https://github.com/vinsworldcom/nppGitSCM) | C++ | 6905 | To do |
| GotoLineCol | GotoLineCol | [shriprem/Goto-Line-Col-NPP-Plugin](https://github.com/shriprem/Goto-Line-Col-NPP-Plugin) | C++ | 7338 | To do |
| Graph Builder | GraphBuilderLaz | [pisarev/graphbuilder-npp](https://github.com/pisarev/graphbuilder-npp) | Pascal | 10036 | Delphi / Free Pascal: needs a rewrite |
| HEX-Editor | HexEditor | [chcg/NPP_HexEditor](https://github.com/chcg/NPP_HexEditor) | C++ | 21926 | To do |
| HiddenLexers | HiddenLexers | [pryrt/NppPlugin-HiddenLexers](https://github.com/pryrt/NppPlugin-HiddenLexers) | C++ | 30713 | To do |
| HLASM Lexer - Column-Aware Syntax Highlighting | HLASMLexer | [Zaneham/hlasm-npp](https://github.com/Zaneham/hlasm-npp) | C++ | 535 | To do |
| HTML Tag | HTMLTag | [rdipardo/nppHTMLTag](https://github.com/rdipardo/nppHTMLTag) | C++ | 4239 | To do |
| HugeFiles | HugeFiles | [molsonkiko/HugeFiles](https://github.com/molsonkiko/HugeFiles) | C# | 18225 | C# / .NET WinForms: needs a C++/Qt rewrite |
| ImgTag | ImgTag | [chcg/ImgTag](https://github.com/chcg/ImgTag) | C# | 4702 | C# / .NET WinForms: needs a C++/Qt rewrite |
| Indent By Fold | IndentByFold | [ffes/indentbyfold](https://github.com/ffes/indentbyfold) | C++ | 1513 | To do |
| Input Sequence | InputSequence | [dasu88/NPPInputSequence](https://github.com/dasu88/NPPInputSequence) | C++ | 410 | To do |
| Java Plugin | NppJavaPlugin | [dominikcebula/npp-java-plugin](https://github.com/dominikcebula/npp-java-plugin) | C++ | 7035 | To do |
| JavaScript Map Parser | JsMapParser.NppPlugin | [megaboich/js-map-parser](https://github.com/megaboich/js-map-parser) | C# | 18231 | C# / .NET WinForms: needs a C++/Qt rewrite |
| jN Notepad++ Plugin | jN | [sieukrem/jn-npp-plugin](https://github.com/sieukrem/jn-npp-plugin) | C++ | 33639 | To do |
| JSFunctionViewer | JSFunctionViewer | [davidsover/nppJSFunctionViewer](https://github.com/davidsover/nppJSFunctionViewer) | C# | 14552 | C# / .NET WinForms: needs a C++/Qt rewrite |
| JSON Tools | JsonTools | [molsonkiko/JsonToolsNppPlugin](https://github.com/molsonkiko/JsonToolsNppPlugin) | C# | 41702 | **Core port with Qt frontend** |
| JSON Viewer | NPPJSONViewer | [NPP-JSONViewer/JSON-Viewer](https://github.com/NPP-JSONViewer/JSON-Viewer) | C++ | 5172 | To do |
| JsonL Simple Formatter | JsonLSimpleFormatter | [davikokar/JsonLSimpleFormatter](https://github.com/davikokar/JsonLSimpleFormatter) | C# | 1212 | C# / .NET WinForms: needs a C++/Qt rewrite |
| JSTool | JSMinNPP | [sunjw/jstoolnpp](https://github.com/sunjw/jstoolnpp) | C++ | 11895 | To do |
| Language Selector | Language_Selector | [hassaine-abdellah/npp-language-selector](https://github.com/hassaine-abdellah/npp-language-selector) | C++ | 4655 | To do |
| LanguageHelp | LanguageHelp | [francostellari/NppPlugins](https://github.com/francostellari/NppPlugins) | ? | 0 | No source in the repository |
| LDAP Formatter | NppLdapFormatter | [ignaciojulio/npp-ldap-formatter](https://github.com/ignaciojulio/npp-ldap-formatter) | C++ | 3652 | To do |
| Linefilter3 | Linefilter3 | https://www.seelisoft.net/Linefilter3/ | - | 0 | No source on GitHub (binary on SourceForge or a web site) |
| Linter | Linter | [deadem/notepad-pp-linter](https://github.com/deadem/notepad-pp-linter) | C++ | 6024 | To do |
| Linter++ | Linter++ | [ThosRTanner/notepad-pp-linter](https://github.com/ThosRTanner/notepad-pp-linter) | C++ | 5848 | To do |
| LLM Assistant | LLMAssistant | [Gvo87/NotepadPlusPlus-LLM-Plugin](https://github.com/Gvo87/NotepadPlusPlus-LLM-Plugin) | C++ | 2116 | To do |
| Location Navigate | LocationNavigate | https://sourceforge.net/projects/locationnav/ | - | 0 | No source on GitHub (binary on SourceForge or a web site) |
| LogRAM | LogRAM.Npp | [HBZHSN/LogRAM](https://github.com/HBZHSN/LogRAM) | C# | 11579 | C# / .NET WinForms: needs a C++/Qt rewrite |
| LuaScript | LuaScript | [dail8859/LuaScript](https://github.com/dail8859/LuaScript) | C++ | 35218 | To do |
| MahiMarkD++ | MahiMarkD++ | [Mahiron-hq/MahiMarkD-PlusPlus](https://github.com/Mahiron-hq/MahiMarkD-PlusPlus) | Rust | 5001 | Rust: needs a rewrite or a Rust binding of the SDK |
| Markdown Features | NppMarkdownFeatures | [0langa/npp-markdown-features](https://github.com/0langa/npp-markdown-features) | C++ | 6798 | To do |
| Markdown Panel | NppMarkdownPanel | [mohzy83/NppMarkdownPanel](https://github.com/mohzy83/NppMarkdownPanel) | C# | 20151 | C# / .NET WinForms: needs a C++/Qt rewrite |
| Markdown Table Editor | MarkdownTableEditor | [krotname/NppMarkdownTableEditor](https://github.com/krotname/NppMarkdownTableEditor) | C++ | 12372 | To do |
| Markdown with Mermaid | Markdown_with_Mermaid | [rhyots/Markdown_with_Mermaid](https://github.com/rhyots/Markdown_with_Mermaid) | C++ | 1809 | To do |
| MarkdownReader | MarkdownReader | [rsalinger/NotepadPlusPlus-Markdown](https://github.com/rsalinger/NotepadPlusPlus-Markdown) | C++ | 2909 | To do |
| MarkdownViewer++ | MarkdownViewerPlusPlus | [nea/MarkdownViewerPlusPlus](https://github.com/nea/MarkdownViewerPlusPlus) | C# | 17308 | C# / .NET WinForms: needs a C++/Qt rewrite |
| MarkLens | MarkLens | [maarten-grassmid/marklens](https://github.com/maarten-grassmid/marklens) | C# | 1899 | C# / .NET WinForms: needs a C++/Qt rewrite |
| MenuIcons | MenuIcons | [francostellari/NppPlugins](https://github.com/francostellari/NppPlugins) | ? | 0 | No source in the repository |
| Merge files in one | Merge files in one | [gurikbal/Merge-files-in-one](https://github.com/gurikbal/Merge-files-in-one) | C# | 2850 | C# / .NET WinForms: needs a C++/Qt rewrite |
| Mermaid Viewer | MermaidViewer | [Lithe-Lab/MermaidViewerNpp](https://github.com/Lithe-Lab/MermaidViewerNpp) | C# | 17949 | C# / .NET WinForms: needs a C++/Qt rewrite |
| Mime tools | mimeTools | [npp-plugins/mimetools](https://github.com/npp-plugins/mimetools) | C++ | 5435 | **Ported** |
| MoarFavorites | MoarFavorites | [ERMtn/MoarFavorites](https://github.com/ERMtn/MoarFavorites) | ? | 0 | No source in the repository |
| Modern Markdown viewer | NMD | [VioletTape/notepad_md_plugin](https://github.com/VioletTape/notepad_md_plugin) | C++ | 878 | To do |
| Multi-Search | NppMultiSearch | [NachoMarengo/NppMultiSearch](https://github.com/NachoMarengo/NppMultiSearch) | C# | 1495 | C# / .NET WinForms: needs a C++/Qt rewrite |
| MultiReplace | MultiReplace | [daddel80/notepadpp-multireplace](https://github.com/daddel80/notepadpp-multireplace) | C++ | 85587 | To do |
| MusicPlayer | MusicPlayer_1.0.11x64 | [gallettube/MusicPlayer](https://github.com/gallettube/MusicPlayer) | C# | 2718 | C# / .NET WinForms: needs a C++/Qt rewrite |
| MZC8051 | MZC8051 | [Jiangshan00001/npp_MZC8051](https://github.com/Jiangshan00001/npp_MZC8051) | C++ | 5927 | To do |
| NavigateTo | NavigateTo | [young-developer/nppNavigateTo](https://github.com/young-developer/nppNavigateTo) | C# | 19516 | C# / .NET WinForms: needs a C++/Qt rewrite |
| NestedDSV Data Visualizer | NestedDSV | [shriprem/NestedDSV](https://github.com/shriprem/NestedDSV) | C++ | 16305 | To do |
| NewFileBrowser | NewFileBrowser | https://sourceforge.net/projects/locationnav/ | - | 0 | No source on GitHub (binary on SourceForge or a web site) |
| Notepad++ bplist plugin | NppBplistPlugin | [azerg/NppBplistPlugin](https://github.com/azerg/NppBplistPlugin) | C++ | 2544 | To do |
| Notepad++ Plugin Demo | NppPluginDemo | [npp-plugins/plugindemo](https://github.com/npp-plugins/plugindemo) | C++ | 5306 | To do |
| Notepad++ Plugin Template | NppPluginTemplate | [npp-plugins/plugintemplate](https://github.com/npp-plugins/plugintemplate) | C++ | 4811 | To do |
| Notepad++ Syntax Plugin for Foresight Platform Fore | ForeLexer | [rhyots/notepad-plus-plus-syntax-plugin-foresight-platform-fore](https://github.com/rhyots/notepad-plus-plus-syntax-plugin-foresight-platform-fore) | C++ | 2189 | To do |
| NotepadStarterPlugin | NotepadStarterPlugin | [lygstate/NotepadStarter](https://github.com/lygstate/NotepadStarter) | C++ | 5371 | To do |
| Npp Converter | NppConverter | [npp-plugins/converter](https://github.com/npp-plugins/converter) | C++ | 5548 | **Ported** |
| Npp Highlighter | Npp-Highlighter | [michaelxzhang/Npp-Highlighter](https://github.com/michaelxzhang/Npp-Highlighter) | C++ | 35944 | To do |
| npp Random String Generator | nppRandomStringGenerator | [cmbsolutions/nppRandomStringGenerator](https://github.com/cmbsolutions/nppRandomStringGenerator) | C# | 23451 | C# / .NET WinForms: needs a C++/Qt rewrite |
| Npp Xml Treeview | NppXmlTreeviewPlugin | [joaoasrosa/nppxmltreeview](https://github.com/joaoasrosa/nppxmltreeview) | C# | 3923 | C# / .NET WinForms: needs a C++/Qt rewrite |
| npp.Connections | npp.connections | [chcg/npp.connections](https://github.com/chcg/npp.connections) | Pascal | 8313 | Delphi / Free Pascal: needs a rewrite |
| NppAIAssistant | NppAIAssistant | [pingqLIN/NppAIAssistant](https://github.com/pingqLIN/NppAIAssistant) | C++ | 13056 | To do |
| NppCrossCheck | NppCrossCheck | [pablo-code14/NppCrossCheck](https://github.com/pablo-code14/NppCrossCheck) | C++ | 6964 | To do |
| NppCrypt | nppcrypt | [chcg/nppcrypt](https://github.com/chcg/nppcrypt) | C++ | 167727 | To do |
| NppESPHome | NppESPHome | [atiburzi/NppESPHome-Plugin](https://github.com/atiburzi/NppESPHome-Plugin) | Pascal | 10387 | Delphi / Free Pascal: needs a rewrite |
| NppEventExec | NppEventExec | [MIvanchev/NppEventExec](https://github.com/MIvanchev/NppEventExec) | C++ | 9512 | To do |
| NppExec | NppExec | [d0vgan/nppexec](https://github.com/d0vgan/nppexec) | C++ | 84359 | To do |
| NppExport | NppExport | [chcg/NPP_ExportPlugin](https://github.com/chcg/NPP_ExportPlugin) | C++ | 3566 | To do |
| NppFavorites | NppFavorites | [heldersepu/nppfavorites](https://github.com/heldersepu/nppfavorites) | C++ | 3382 | To do |
| NppFoldTail | NppFoldTail | [seantw/NppFoldTail](https://github.com/seantw/NppFoldTail) | C++ | 1717 | To do |
| NppFormatter | NppFormatter | [oiltea94/npp-formatter](https://github.com/oiltea94/npp-formatter) | C++ | 2143 | To do |
| NppFTP | NppFTP | [ashkulz/NppFTP](https://github.com/ashkulz/NppFTP) | C++ | 37766 | To do |
| NppGist | NppGist | [KvanTTT/NppGist](https://github.com/KvanTTT/NppGist) | C# | 5416 | C# / .NET WinForms: needs a C++/Qt rewrite |
| NppGTags | NppGTags | [pnedev/nppgtags](https://github.com/pnedev/nppgtags) | C++ | 15895 | To do |
| NppGZipFileViewer [Deprecated] | NppGZipFileViewer | [Pascal-Krenckel/NppGZipFileViewer](https://github.com/Pascal-Krenckel/NppGZipFileViewer) | C# | 15686 | C# / .NET WinForms: needs a C++/Qt rewrite |
| NppImagePreview | NppImagePreview | [bdgeek/NppImagePreview](https://github.com/bdgeek/NppImagePreview) | C++ | 2862 | To do |
| NppJumpList | NppJumpList | [chcg/JumpList](https://github.com/chcg/JumpList) | C++ | 5463 | To do |
| NppMarkdownNext | NppMarkdownNext | [theneet0/NppMarkdownNext](https://github.com/theneet0/NppMarkdownNext) | C++ | 5732 | To do |
| NppMenuScripts | NppMenuScripts | [seantw/NppMenuScripts](https://github.com/seantw/NppMenuScripts) | C++ | 10803 | To do |
| NppMenuSearch | NppMenuSearch | [peter-frentrup/NppMenuSearch](https://github.com/peter-frentrup/NppMenuSearch) | C# | 7833 | C# / .NET WinForms: needs a C++/Qt rewrite |
| NppNetNote | NppDocShare | [chcg/NppDocShare](https://github.com/chcg/NppDocShare) | C++ | 8721 | To do |
| NppOpenAI | NppOpenAI | [Krazal/nppopenai](https://github.com/Krazal/nppopenai) | C++ | 39415 | To do |
| NppPluginOpenHost | NppPluginOpenHost | [jejemorg/NppPluginOpenHost](https://github.com/jejemorg/NppPluginOpenHost) | C++ | 2687 | To do |
| NppQrCode | NppQrCode64 | [rdipardo/nppQrCode](https://github.com/rdipardo/nppQrCode) | Pascal | 1559 | Delphi / Free Pascal: needs a rewrite |
| NppRegExTractor | NppRegExTractorPlugin | [viper3400/NppRegExTractor](https://github.com/viper3400/NppRegExTractor) | C# | 2659 | C# / .NET WinForms: needs a C++/Qt rewrite |
| NppRestoreCursorPosition | NppRestoreCursorPosition | [RivieraSystems/NppRestoreCursorPosition](https://github.com/RivieraSystems/NppRestoreCursorPosition) | C++ | 3665 | To do |
| NppS3 | NppS3 | [bsy0318/NppS3](https://github.com/bsy0318/NppS3) | C++ | 10652 | To do |
| NppStripIpAndHost | NppStripIpAndHost | [larreblarsson/NppStripIpAndHost](https://github.com/larreblarsson/NppStripIpAndHost) | C++ | 5632 | To do |
| NppTags | NppTags | [ffes/npptags](https://github.com/ffes/npptags) | C++ | 253724 | To do |
| NppTextFX2 | NppTextFX | [rainman74/NPPTextFX2](https://github.com/rainman74/NPPTextFX2) | C++ | 23182 | To do |
| NppTextViz | NppTextViz | [chcg/NppTextViz](https://github.com/chcg/NppTextViz) | C++ | 4688 | To do |
| NppTranslatePanel | NppTranslatePanel | [Jerryk133/NppTranslatePanel](https://github.com/Jerryk133/NppTranslatePanel) | C# | 22610 | C# / .NET WinForms: needs a C++/Qt rewrite |
| NppTranslator | NppTranslator | [AndyDL24/PluginTranslateNPP](https://github.com/AndyDL24/PluginTranslateNPP) | C++ | 2918 | To do |
| NppUISpy | NppUISpy | [dinkumoil/NppUISpy](https://github.com/dinkumoil/NppUISpy) | Pascal | 48248 | Delphi / Free Pascal: needs a rewrite |
| NppVim | NppVim | [h-jangra/NppVim](https://github.com/h-jangra/NppVim) | C++ | 20666 | To do |
| NppVoicePlugin | NppVoicePlugin | [bms-prog/NppVoicePlugin](https://github.com/bms-prog/NppVoicePlugin) | ? | 0 | No source in the repository |
| NppZenHankaku | NppZenHankaku | [RivieraSystems/NppZenHankaku](https://github.com/RivieraSystems/NppZenHankaku) | C++ | 4651 | To do |
| NWScript Tools | NWScript-Npp | [Leonard-The-Wise/NWScript-Npp](https://github.com/Leonard-The-Wise/NWScript-Npp) | C++ | 69242 | To do |
| NX ASCII Database Helper | NX_ASCII_Database_Helper | [ZhuXuefengGitHub/NX_ASCII_Database_Helper](https://github.com/ZhuXuefengGitHub/NX_ASCII_Database_Helper) | C++ | 6424 | To do |
| OCaml Lexer | OCamlLexer | [Zaneham/ocaml-npp](https://github.com/Zaneham/ocaml-npp) | C++ | 807 | To do |
| Open File In Solution | nppplugin_ofis2 | [incrediblejr/nppplugins](https://github.com/incrediblejr/nppplugins) | C++ | 40561 | To do |
| OpenSelection | OpenSelection | [francostellari/NppPlugins](https://github.com/francostellari/NppPlugins) | ? | 0 | No source in the repository |
| Papyrus Script Lexer | Papyrus | [blu3mania/npp-papyrus](https://github.com/blu3mania/npp-papyrus) | C++ | 8509 | To do |
| PlantUML Viewer | PlantUmlViewer | [Fruchtzwerg94/PlantUmlViewer](https://github.com/Fruchtzwerg94/PlantUmlViewer) | C# | 19810 | C# / .NET WinForms: needs a C++/Qt rewrite |
| Poor Man's T-Sql Formatter | PoorMansTSqlFormatterNppPlugin | [TaoK/PoorMansTSqlFormatter](https://github.com/TaoK/PoorMansTSqlFormatter) | C# | 20188 | C# / .NET WinForms: needs a C++/Qt rewrite |
| Pork to Sausage | pork2sausage | [npp-plugins/pork2sausage](https://github.com/npp-plugins/pork2sausage) | C++ | 4539 | To do |
| Preview HTML | PreviewHTML | [rdipardo/npp_preview](https://github.com/rdipardo/npp_preview) | Pascal | 9308 | Delphi / Free Pascal: needs a rewrite |
| pycalc | pycalc | [pycalc-plugin/notepad-plus-plus](https://github.com/pycalc-plugin/notepad-plus-plus) | C++ | 3412 | To do |
| Python Indent | Python Indent | https://sourceforge.net/projects/kered13-notepad-plugins/ | - | 0 | No source on GitHub (binary on SourceForge or a web site) |
| PythonScript | PythonScript | [bruderstein/PythonScript](https://github.com/bruderstein/PythonScript) | Python | 538515 | Python: needs a rewrite or a Python binding of the SDK |
| QuickText | QuickText | [vinsworldcom/nppQuickText](https://github.com/vinsworldcom/nppQuickText) | C++ | 4163 | To do |
| Random Values | RandomValuesNppPlugin | [BdR76/RandomValuesNPP](https://github.com/BdR76/RandomValuesNPP) | C# | 18798 | C# / .NET WinForms: needs a C++/Qt rewrite |
| RAScript | RAScript | [joshraphael/npp-rascript](https://github.com/joshraphael/npp-rascript) | C++ | 1045 | To do |
| RDMD for Notepad++ (English) | rdmd-en-x64 | [dokutoku/rdmd-for-npp](https://gitlab.com/dokutoku/rdmd-for-npp) | D | 5523 | D: needs a rewrite or a D binding of the SDK |
| RDMD for Notepad++ (Japanese) | rdmd-ja-x64 | [dokutoku/rdmd-for-npp](https://gitlab.com/dokutoku/rdmd-for-npp) | D | 5523 | D: needs a rewrite or a D binding of the SDK |
| Redis | RedisPlugin | [bakingam1983/Notepad.plus.plus-RedisPlugin](https://github.com/bakingam1983/Notepad.plus.plus-RedisPlugin) | C# | 1156 | C# / .NET WinForms: needs a C++/Qt rewrite |
| Regex Trainer | RegexTrainer | [ahmoylaw/RegexTrainer-Descriptions](https://github.com/ahmoylaw/RegexTrainer-Descriptions) | ? | 0 | No source in the repository |
| Remove Duplicate Lines | Remove Duplicate Lines | [gurikbal/Remove_dup_lines](https://github.com/gurikbal/Remove_dup_lines) | C# | 2625 | C# / .NET WinForms: needs a C++/Qt rewrite |
| RestApiToText | RestApiToText | [eljefe7000/RestApiToText](https://github.com/eljefe7000/RestApiToText) | C++ | 3475 | To do |
| Reverse Lines | qkNppReverseLines | [querykuma/qkNppReverseLines](https://github.com/querykuma/qkNppReverseLines) | C++ | 4417 | To do |
| RGA ISA Compiler | NppRgaIsaPlugin | [DIMOSUS/NppRgaIsaPlugin](https://github.com/DIMOSUS/NppRgaIsaPlugin) | C++ | 7812 | To do |
| RunMe | RunMe | [francostellari/NppPlugins](https://github.com/francostellari/NppPlugins) | ? | 0 | No source in the repository |
| rustnpp | rustnpp | [chcg/rustnpp](https://github.com/chcg/rustnpp) | Rust | 274 | Rust: needs a rewrite or a Rust binding of the SDK |
| Save as admin | NppSaveAsAdmin | [Hsilgos/nppsaveasadmin](https://github.com/Hsilgos/nppsaveasadmin) | C++ | 6407 | To do |
| ScrollMarks | ScrollMarks | [Hong-Seungmin/ScrollMarks](https://github.com/Hong-Seungmin/ScrollMarks) | C++ | 5046 | To do |
| SecurePad | SecurePad | [DominicTobias/SecurePad](https://github.com/DominicTobias/SecurePad) | C++ | 4930 | To do |
| Select N' Launch | selectNLaunch | [npp-plugins/selectnlaunch](https://github.com/npp-plugins/selectnlaunch) | C++ | 4157 | To do |
| Select to Clipboard | SelectToClipboard | [KubaDee/SelectToClipboard](https://github.com/KubaDee/SelectToClipboard) | C++ | 3196 | To do |
| SelectQuotedText | SelectQuotedText | [ffes/selectquotedtext](https://github.com/ffes/selectquotedtext) | C++ | 1136 | To do |
| Session Manager | SessionMgr | [chcg/npp-session-manager](https://github.com/chcg/npp-session-manager) | C++ | 10910 | To do |
| Shtirlitz | ShtirlitzNppPlugin | [shtirlitz-dev/notepadpp-plugin](https://github.com/shtirlitz-dev/notepadpp-plugin) | ? | 0 | No source in the repository |
| Simplify Json | SimplifyJson | [analystvipul/SimplifyJson](https://github.com/analystvipul/SimplifyJson) | C++ | 3756 | To do |
| Snippets | NppSnippets | [ffes/nppsnippets](https://github.com/ffes/nppsnippets) | C++ | 253730 | To do |
| Solution Hub | nppplugin_solutionhub | [incrediblejr/nppplugins](https://github.com/incrediblejr/nppplugins) | C++ | 40561 | To do |
| Solution Hub UI | nppplugin_solutionhub_ui | [incrediblejr/nppplugins](https://github.com/incrediblejr/nppplugins) | C++ | 40561 | To do |
| Solution Tools | nppplugin_solutiontools | [incrediblejr/nppplugins](https://github.com/incrediblejr/nppplugins) | C++ | 40561 | To do |
| Source Cookifier | SourceCookifier | [ufo/sourcecookifier](https://github.com/ufo/sourcecookifier) | C | 55437 | To do |
| SpeechPlugin | SpeechPlugin | [chcg/SpeechPlugin](https://github.com/chcg/SpeechPlugin) | C++ | 2700 | To do |
| SQL Column Highlighter | SqlColumnHighlighter | [zhgao/npp-sql-column-highlighter](https://github.com/zhgao/npp-sql-column-highlighter) | C++ | 993 | To do |
| SQLinForm | SQLinFormNpp64 | https://www.sqlinform.com | - | 0 | No source on GitHub (binary on SourceForge or a web site) |
| StayAwake | StayAwake_NPP | [shriprem/StayAwake_NPP_Plugin](https://github.com/shriprem/StayAwake_NPP_Plugin) | C++ | 3086 | To do |
| SurroundSelection | SurroundSelection | [dail8859/SurroundSelection](https://github.com/dail8859/SurroundSelection) | C++ | 4206 | To do |
| TagLEET | TagLEET | https://sourceforge.net/projects/tagleet/ | - | 0 | No source on GitHub (binary on SourceForge or a web site) |
| TakeNotes | TakeNotes | [francostellari/NppPlugins](https://github.com/francostellari/NppPlugins) | ? | 0 | No source in the repository |
| Task List | NppTaskList | [Megabyteceer/npp-task-list](https://github.com/Megabyteceer/npp-task-list) | C++ | 6718 | To do |
| TC Syslog Finder | TCSyslogFinder | [bsagarzazu/npp-tc-syslog-finder](https://github.com/bsagarzazu/npp-tc-syslog-finder) | C++ | 4170 | To do |
| TidyText | TidyText | [gonzalu/TidyText](https://github.com/gonzalu/TidyText) | C++ | 4730 | To do |
| ToolBucket | NppToolBucket | https://phdesign.com.au/npptoolbucket/ | - | 0 | No source on GitHub (binary on SourceForge or a web site) |
| TopMost | TopMost | [francostellari/NppPlugins](https://github.com/francostellari/NppPlugins) | ? | 0 | No source in the repository |
| Tortoise SVN | nppplugin_svn | [incrediblejr/nppplugins](https://github.com/incrediblejr/nppplugins) | C++ | 40561 | To do |
| TreeSitter | TreeSitterLexer | [Thorium/NppTreeSitter](https://github.com/Thorium/NppTreeSitter) | C++ | 4195 | To do |
| TroiaLens | TroiaLens | [ufukt51/TroiaLensPlugin](https://github.com/ufukt51/TroiaLensPlugin) | C# | 3258 | C# / .NET WinForms: needs a C++/Qt rewrite |
| URL Encode/Decode Plugin | urlPlugin | [SinghRajenM/nppURLPlugin](https://github.com/SinghRajenM/nppURLPlugin) | C++ | 2137 | To do |
| Virtual Folders | VirtualFolders | [FatihC/VirtualFolders](https://github.com/FatihC/VirtualFolders) | C++ | 49306 | To do |
| Virtual Tab Groups | VirtualTabGroups | [JamesShaver/VirtualTabGroups](https://github.com/JamesShaver/VirtualTabGroups) | C# | 2987 | C# / .NET WinForms: needs a C++/Qt rewrite |
| Visual Studio Line Copy | VisualStudioLineCopy | https://sourceforge.net/projects/notepad-visualstudiolinecopy/ | - | 0 | No source on GitHub (binary on SourceForge or a web site) |
| VSCode Keymap NPP | VSCodeKeymapNpp | [14ag/vscode-to-notepadpp](https://github.com/14ag/vscode-to-notepadpp) | C++ | 4084 | To do |
| WakaTime | WakaTime | [wakatime/notepadpp-wakatime](https://github.com/wakatime/notepadpp-wakatime) | C# | 16643 | C# / .NET WinForms: needs a C++/Qt rewrite |
| WebEdit | WebEdit | [Krazal/WebEdit](https://github.com/Krazal/WebEdit) | C# | 1654 | C# / .NET WinForms: needs a C++/Qt rewrite |
| XBrackets Lite | XBrackets | [d0vgan/npp-XBracketsLite](https://github.com/d0vgan/npp-XBracketsLite) | C++ | 12182 | To do |
| XML Navigator | XMLNavigator | [chcg/XmlNavigator](https://github.com/chcg/XmlNavigator) | C# | 3882 | C# / .NET WinForms: needs a C++/Qt rewrite |
| XML Tools | XMLTools | [morbac/xmltools](https://github.com/morbac/xmltools) | C++ | 15394 | To do |
| xsvViewer++ | xsvViewer | [mgalhu/xsvViewer](https://github.com/mgalhu/xsvViewer) | C++ | 6408 | To do |
| YamlProperties | YamlProperties | [oiltea94/npp-yaml-properties](https://github.com/oiltea94/npp-yaml-properties) | C++ | 1701 | To do |
