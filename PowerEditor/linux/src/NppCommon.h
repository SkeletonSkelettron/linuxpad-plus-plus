// This file is part of Notepad++ project
// Copyright (C)2026 Notepad++ Linux port contributors
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// at your option any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

// Shared definitions of the Linux (Qt) front end.
// Values mirror MISC/Common/NppConstants.h and ScintillaComponent/ScintillaEditView.h,
// which cannot be included here because they depend on <windows.h>.

#pragma once

#include <cstdint>
#include <utility>
#include <QColor>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

#include "Scintilla.h"
#include "SciLexer.h"
#include "BoostRegexSearch.h"
#include "LangType.h"
#include "menuCmdID.h"

using COLORREF = uint32_t; // 0x00BBGGRR, the layout used by Scintilla and Win32

constexpr COLORREF rgb(int r, int g, int b)
{
	return static_cast<COLORREF>(r) | (static_cast<COLORREF>(g) << 8) | (static_cast<COLORREF>(b) << 16);
}

inline QColor toQColor(COLORREF c)
{
	return QColor(c & 0xFF, (c >> 8) & 0xFF, (c >> 16) & 0xFF);
}

inline COLORREF fromQColor(const QColor& c)
{
	return rgb(c.red(), c.green(), c.blue());
}

enum LangIdxStyle
{
	LANG_INDEX_INSTR = 0,
	LANG_INDEX_INSTR2 = 1,
	LANG_INDEX_TYPE = 2,
	LANG_INDEX_TYPE2 = 3,
	LANG_INDEX_TYPE3 = 4,
	LANG_INDEX_TYPE4 = 5,
	LANG_INDEX_TYPE5 = 6,
	LANG_INDEX_TYPE6 = 7,
	LANG_INDEX_TYPE7 = 8,
	LANG_INDEX_SUBSTYLE1 = 9,
	LANG_INDEX_SUBSTYLE2 = 10,
	LANG_INDEX_SUBSTYLE3 = 11,
	LANG_INDEX_SUBSTYLE4 = 12,
	LANG_INDEX_SUBSTYLE5 = 13,
	LANG_INDEX_SUBSTYLE6 = 14,
	LANG_INDEX_SUBSTYLE7 = 15,
	LANG_INDEX_SUBSTYLE8 = 16
};

inline constexpr int NB_LIST = 20;
inline constexpr int STYLE_NOT_USED = -1;

enum FontStyle
{
	FONTSTYLE_NONE = 0,
	FONTSTYLE_BOLD = 1,
	FONTSTYLE_ITALIC = 2,
	FONTSTYLE_UNDERLINE = 4
};

inline constexpr int COLORSTYLE_FOREGROUND = 0x01;
inline constexpr int COLORSTYLE_BACKGROUND = 0x02;
inline constexpr int COLORSTYLE_ALL = COLORSTYLE_FOREGROUND | COLORSTYLE_BACKGROUND;

// Scintilla margins, markers and indicators used by Notepad++
inline constexpr int MARGIN_LINENUMBER = 0;
inline constexpr int MARGIN_SYMBOL = 1;
inline constexpr int MARGIN_CHANGEHISTORY = 2;
inline constexpr int MARGIN_FOLDER = 3;

inline constexpr int MARK_BOOKMARK = 20;
inline constexpr int MARK_HIDELINESBEGIN = 19;
inline constexpr int MARK_HIDELINESEND = 18;

inline constexpr int URL_INDIC = 8;
inline constexpr int FINDDLG_HIGHLIGHT_INDIC = 30;

enum FolderStyle
{
	FOLDER_STYLE_SIMPLE = 1,
	FOLDER_STYLE_ARROW = 2,
	FOLDER_STYLE_CIRCLE = 3,
	FOLDER_STYLE_BOX = 4,
	FOLDER_STYLE_NONE = 5
};

struct LanguageNameInfo
{
	const char* langName = nullptr;
	const char* shortName = nullptr;
	const char* longName = nullptr;
	LangType langID = L_TEXT;
	const char* lexerID = nullptr;
};

// Values of the Preferences dialog, mirroring NppGUI / ScintillaViewParams (Parameters.h)
enum class FluentColor { defaultColor, red, green, blue, purple, cyan, olive, yellow, accent, custom };
enum ToolbarIconSet { TB_SMALL = 0, TB_LARGE = 1, TB_SMALL2 = 2, TB_LARGE2 = 3, TB_STANDARD = 4 };
enum LineHiliteMode { LINEHILITE_NONE = 0, LINEHILITE_HILITE = 1, LINEHILITE_FRAME = 2 };
enum LineWrapMethod { LINEWRAP_DEFAULT = 0, LINEWRAP_ALIGNED = 1, LINEWRAP_INDENT = 2 };
enum CrlfMode { CRLF_PLAINTEXT = 0, CRLF_ROUNDEDRECT = 1, CRLF_PLAINTEXT_CUSTOM = 2, CRLF_ROUNDEDRECT_CUSTOM = 3 };
enum NpcMode { NPC_IDENTITY = 0, NPC_ABBREVIATION = 1, NPC_CODEPOINT = 2 };
enum DarkModeChoice { DARKMODE_LIGHT = 0, DARKMODE_DARK = 1, DARKMODE_FOLLOW_SYSTEM = 2 };
enum OpenSaveDir { DIR_FOLLOW_CURRENT = 0, DIR_REMEMBER_LAST = 1, DIR_FIXED = 2 };
enum RecentDisplay { RECENT_FILENAME_ONLY = 0, RECENT_FULL_PATH = 1, RECENT_CUSTOM_LENGTH = 2 };
enum AutoIndentMode { AUTOINDENT_NONE = 0, AUTOINDENT_BASIC = 1, AUTOINDENT_ADVANCED = 2 };
enum BackupMode { BACKUP_NONE = 0, BACKUP_SIMPLE = 1, BACKUP_VERBOSE = 2 };
enum MultiInstMode { MONO_INSTANCE = 0, MULTI_INSTANCE = 1, SESSION_IN_NEW_INSTANCE = 2 };
enum FileAutoDetect { AUTODETECT_CURRENT = 0, AUTODETECT_ALL = 1, AUTODETECT_DISABLED = 2 };
enum SystrayAction { SYSTRAY_NONE = 0, SYSTRAY_MINIMIZE = 1, SYSTRAY_CLOSE = 2, SYSTRAY_BOTH = 3 };
enum SearchEngineChoice { SE_CUSTOM = 0, SE_DUCKDUCKGO = 1, SE_GOOGLE = 2, SE_BING = 3, SE_YAHOO = 4, SE_STACKOVERFLOW = 5 };

// Every persisted preference: X(type, name, default value).
// Stored in ~/.config/linuxpad++/config.ini under the same names.
#define NPP_PREF_FIELDS(X) \
	/* View menu */ \
	X(bool, wordWrap, false) \
	X(bool, showSpaceTab, false) \
	X(bool, showEol, false) \
	X(bool, showIndentGuide, true) \
	X(bool, showWrapSymbol, false) \
	X(bool, showNpc, false) \
	X(bool, showCcUniEol, true) \
	X(bool, alwaysOnTop, false) \
	X(bool, zoomSync, false) /* View > Zoom > Synchronize Across Views */ \
	X(bool, funcListSortByDefault, false) /* Function List > Preferences */ \
	/* General */ \
	X(QString, localization, QStringLiteral("english.xml")) \
	X(bool, hideMenuBar, false) \
	X(bool, hideMenuRightShortcuts, false) \
	X(bool, statusbarVisible, true) \
	X(int, dialogFontSize, 0) /* 0: desktop default */ \
	X(QString, dialogFontName, QString()) \
	/* Toolbar */ \
	X(bool, toolbarVisible, true) \
	X(int, toolbarIconSet, TB_SMALL) \
	X(int, toolbarColor, static_cast<int>(FluentColor::defaultColor)) \
	X(int, toolbarCustomColor, 0) /* COLORREF */ \
	X(bool, toolbarMono, false) /* colorization: complete (true) or partial */ \
	/* Tab bar */ \
	X(bool, tabbarVisible, true) \
	X(bool, tabVertical, false) \
	X(bool, tabMultiLine, false) \
	X(bool, tabLock, false) \
	X(bool, doubleClickTabCloses, false) \
	X(bool, tabExitOnCloseLast, false) \
	X(int, tabMaxLabelLen, 0) /* 0: no compacting */ \
	X(bool, tabReduce, true) \
	X(bool, tabAltIcons, false) \
	X(bool, tabInactiveColor, true) \
	X(bool, tabActiveBar, true) \
	X(bool, tabCloseButton, true) \
	X(bool, tabPinButton, true) \
	X(bool, tabOnlyPinnedButton, false) \
	X(bool, tabInactiveButtons, false) \
	/* Editing 1 */ \
	X(int, currentLineMode, LINEHILITE_HILITE) \
	X(int, currentLineFrameWidth, 1) \
	X(int, caretWidth, 1) /* 0-3, 4 block, 5 block after */ \
	X(int, caretBlinkRate, 600) \
	X(int, lineWrapMethod, LINEWRAP_ALIGNED) \
	X(bool, smoothFont, false) \
	X(bool, virtualSpace, false) \
	X(bool, foldToggleable, false) \
	X(bool, rightClickKeepsSelection, false) \
	X(bool, lineCopyCutWithoutSelection, true) \
	X(bool, selectedTextForegroundSingleColor, false) \
	X(bool, scrollBeyondLastLine, true) \
	X(bool, disableAdvancedScrolling, false) \
	X(bool, disableSelectedTextDragDrop, false) \
	/* Editing 2 */ \
	X(bool, multiEditing, true) \
	X(bool, columnSel2MultiEdit, true) \
	X(int, eolMode, CRLF_ROUNDEDRECT) \
	X(int, npcMode, NPC_ABBREVIATION) \
	X(bool, npcCustomColor, false) \
	X(bool, npcIncludeCcUniEol, false) \
	X(bool, npcNoInputC0, true) \
	/* Dark mode */ \
	X(int, darkMode, DARKMODE_FOLLOW_SYSTEM) \
	X(int, darkTone, 0) /* 0 black ... 6 olive, 7 customized */ \
	X(QString, darkCustomColors, QString()) \
	/* Margins / border / edge */ \
	X(int, folderStyle, FOLDER_STYLE_BOX) \
	X(int, borderWidth, 2) \
	X(bool, showBorderEdge, true) \
	X(QString, edgeColumns, QString()) \
	X(bool, edgeBgMode, false) \
	X(bool, changeHistoryMargin, true) \
	X(bool, changeHistoryIndicator, false) \
	X(bool, showLineNumber, true) \
	X(bool, lineNumberDynamicWidth, true) \
	X(int, paddingLeft, 0) \
	X(int, paddingRight, 0) \
	X(int, distractionFreeDivPart, 4) \
	X(bool, showBookmarkMargin, true) \
	/* New document */ \
	X(int, newDocEol, SC_EOL_LF) \
	X(int, newDocUnicode, 4) /* UniMode::uniCookie (UTF-8) */ \
	X(int, newDocCodepage, -1) \
	X(bool, newDocOpenAnsiAsUtf8, true) \
	X(int, newDocLang, L_TEXT) \
	X(bool, addNewDocOnStartup, false) \
	X(bool, useContentAsTabName, false) \
	/* Default directory */ \
	X(int, openSaveDir, DIR_FOLLOW_CURRENT) \
	X(QString, openSaveDirPath, QString()) \
	X(bool, dropFolderOpensFiles, false) \
	/* Recent files history */ \
	X(bool, recentDontCheck, true) \
	X(int, recentFilesMax, 10) \
	X(bool, recentInSubmenu, false) \
	X(int, recentDisplay, RECENT_FULL_PATH) \
	X(int, recentCustomLength, 100) \
	/* Language */ \
	X(bool, langMenuCompact, true) \
	X(QString, disabledLangs, QString()) /* space separated short names */ \
	X(bool, backslashIsEscapeForSql, true) \
	/* Indentation */ \
	X(int, tabSize, 4) \
	X(bool, replaceTabBySpace, false) \
	X(QString, langTabSettings, QString()) /* "lang:size:spaces:bsunindent;..." */ \
	X(bool, backspaceUnindent, false) \
	X(int, autoIndentMode, AUTOINDENT_ADVANCED) \
	/* Highlighting */ \
	X(bool, markAllMatchCase, false) \
	X(bool, markAllWholeWord, true) \
	X(bool, tagMatchHilite, true) \
	X(bool, tagAttrHilite, true) \
	X(bool, hiliteNonHtmlZone, false) \
	X(bool, smartHighlight, true) \
	X(bool, smartHighlightAnotherView, false) \
	X(bool, smartHighlightMatchCase, false) \
	X(bool, smartHighlightWholeWord, true) \
	X(bool, smartHighlightUseFindSettings, false) \
	X(bool, braceHighlight, true) \
	/* Print */ \
	X(bool, printLineNumber, true) \
	X(bool, printFormFeed, false) \
	X(int, printColorMode, SC_PRINT_COLOURONWHITE) \
	X(int, printMarginTop, 0) \
	X(int, printMarginBottom, 0) \
	X(int, printMarginLeft, 0) \
	X(int, printMarginRight, 0) \
	X(QString, printHeaderLeft, QString()) \
	X(QString, printHeaderMiddle, QString()) \
	X(QString, printHeaderRight, QString()) \
	X(QString, printHeaderFont, QString()) \
	X(int, printHeaderSize, 0) \
	X(bool, printHeaderBold, false) \
	X(bool, printHeaderItalic, false) \
	X(QString, printFooterLeft, QString()) \
	X(QString, printFooterMiddle, QString()) \
	X(QString, printFooterRight, QString()) \
	X(QString, printFooterFont, QString()) \
	X(int, printFooterSize, 0) \
	X(bool, printFooterBold, false) \
	X(bool, printFooterItalic, false) \
	/* Searching */ \
	X(int, inSelectionThreshold, 1024) \
	X(bool, fillDirFromActiveDoc, false) \
	X(bool, fillFindFieldWithSelected, true) \
	X(int, fillFindWhatThreshold, 1024) \
	X(bool, fillFindFieldSelectCaret, true) \
	X(bool, monospacedFindDlg, false) \
	X(bool, findDlgAlwaysVisible, false) \
	X(bool, finderOneEntryPerLine, true) \
	X(bool, fifPreferDisk, false) \
	/* Backup */ \
	X(bool, rememberSession, true) \
	X(bool, sessionSnapshot, true) \
	X(int, snapshotIntervalSec, 7) \
	X(bool, keepAbsentFilesInSession, false) \
	X(int, backupMode, BACKUP_NONE) \
	X(bool, backupUseCustomDir, false) \
	X(QString, backupDir, QString()) \
	/* Auto-completion */ \
	X(bool, autoCompletion, true) \
	X(int, autoCompletionMode, 3) /* 1 function, 2 word, 3 both */ \
	X(int, autoCompletionThreshold, 1) \
	X(bool, autoCompletionBrief, false) \
	X(bool, funcParamsHint, true) \
	X(bool, autocInsertTab, true) \
	X(bool, autocInsertEnter, true) \
	X(bool, autocIgnoreNumbers, true) \
	X(bool, autoPairParentheses, false) \
	X(bool, autoPairBrackets, false) \
	X(bool, autoPairBraces, false) \
	X(bool, autoPairDoubleQuotes, false) \
	X(bool, autoPairQuotes, false) \
	X(bool, autoPairHtmlXmlTag, false) \
	X(QString, matchedPairs, QString()) /* up to 3 "oc" pairs */ \
	/* Multi-instance & date */ \
	X(int, multiInstMode, MONO_INSTANCE) \
	X(bool, dateTimeReverse, false) \
	X(QString, dateTimeFormat, QStringLiteral("yyyy-MM-dd HH:mm:ss")) \
	X(bool, keepStateClipboardHistory, false) \
	X(bool, keepStateDocList, false) \
	X(bool, keepStateCharPanel, false) \
	X(bool, keepStateFileBrowser, false) \
	X(bool, keepStateProjectPanel, false) \
	X(bool, keepStateDocMap, false) \
	X(bool, keepStateFuncList, false) \
	X(bool, keepStatePluginPanel, false) \
	/* Delimiter */ \
	X(bool, wordCharsDefault, true) \
	X(QString, customWordChars, QString()) \
	X(QString, delimiterOpen, QStringLiteral("(")) \
	X(QString, delimiterClose, QStringLiteral(")")) \
	X(bool, delimiterSeveralLines, false) \
	/* Performance */ \
	X(bool, largeFileRestriction, true) \
	X(int, largeFileSizeMB, 200) \
	X(bool, largeFileDeactivateWrap, true) \
	X(bool, largeFileAllowAutoCompletion, false) \
	X(bool, largeFileAllowSmartHilite, false) \
	X(bool, largeFileAllowBraceMatch, false) \
	X(bool, largeFileAllowClickableLink, false) \
	X(bool, largeFileSuppress2GBWarning, false) \
	/* Cloud & link */ \
	X(bool, urlHighlight, true) \
	X(bool, urlNoUnderline, false) \
	X(bool, urlFullBox, false) \
	X(QString, uriSchemes, QStringLiteral("svn:// cvs:// git:// imap:// irc:// irc6:// ircs:// ldap:// ldaps:// news: telnet:// gopher:// ssh:// sftp:// smb:// skype: snmp:// spotify: steam:// sms: slack:// chrome:// bitcoin:")) \
	/* Search engine */ \
	X(int, searchEngineChoice, SE_GOOGLE) \
	X(QString, searchEngine, QStringLiteral("https://www.google.com/search?q=$(CURRENT_WORD)")) \
	/* MISC. */ \
	X(int, fileAutoDetectMode, AUTODETECT_CURRENT) \
	X(bool, updateSilently, false) \
	X(bool, scrollToEndAfterUpdate, false) \
	X(bool, docSwitcher, true) \
	X(bool, docSwitcherMru, true) \
	X(bool, docPeekOnTab, false) \
	X(bool, docPeekOnMap, false) \
	X(int, systrayAction, SYSTRAY_NONE) \
	X(bool, muteSounds, false) \
	X(bool, detectEncoding, true) \
	X(bool, shortTitle, false) \
	X(bool, saveAllConfirm, true) \
	X(bool, fawSymlinks, false) \
	X(QString, sessionFileExt, QString()) \
	X(QString, workspaceFileExt, QString()) \
	/* Style configurator */ \
	X(QString, themeFile, QString()) /* empty: stylers.xml / stylers.model.xml (or dark default) */

// Indentation page, per language. size 0: "Use default value" (also over langs.xml)
struct LangTabSetting
{
	int size = 0;
	bool spaces = false;
	bool backspaceUnindent = false;
};

// User preferences, persisted in ~/.config/linuxpad++/config.ini
struct NppPrefs
{
#define NPP_PREF_DECLARE(type, name, def) type name = def;
	NPP_PREF_FIELDS(NPP_PREF_DECLARE)
#undef NPP_PREF_DECLARE
	// [Plugins] enabled= of config.ini: the folders of the plugins LinuxPad++ loads, chosen by the user in
	// Plugins > Plugins Admin... (or imported); none at the first start, though the bundled plugins are installed
	QStringList enabledPlugins;

	void load();
	void save() const;

	// helpers over the stored values
	QList<int> edgeColumnList() const;
	QStringList disabledLangList() const { return disabledLangs.split(QLatin1Char(' '), Qt::SkipEmptyParts); }
	QHash<QString, LangTabSetting> langTabs() const; // by language short name (Indentation page)
	void setLangTabs(const QHash<QString, LangTabSetting>& tabs);
	bool langTab(const QString& langName, LangTabSetting* out) const;
	QList<std::pair<char, char>> matchedPairList() const;
	QString searchEngineUrl() const;
	bool isDark() const; // dark mode on, following the desktop when asked to
	qint64 largeFileSizeBytes() const { return static_cast<qint64>(largeFileSizeMB) * 1024 * 1024; }
};

// Options of the Find dialog that other parts follow (Smart Highlighting "Use Find dialog settings")
struct FindState
{
	bool matchCase = false;
	bool wholeWord = false;
};

namespace Npp
{
	NppPrefs& prefs();
	FindState& findState();
	QString dataDir();   // <prefix>/share/linuxpad++
	QString configDir(); // ~/.config/linuxpad++ (the settings of ~/.config/notepad++ are copied there once)
	QString backupDir(); // ~/.config/linuxpad++/backup
	QString localConfigDir(); // ~/.config/linuxpad++, even when the settings are on a cloud folder
	QString cloudChoiceFile(); // ~/.config/linuxpad++/cloud/choice, like Notepad++
	void beep(); // QApplication::beep() unless "Mute all sounds"
	QString windowsDateTimeFormat(const QString& fmt); // Windows date/time picture ("tt": AM/PM) to Qt
	QString version();   // the Notepad++ version LinuxPad++ is based on, e.g. "8.9.9" (NPPM_GETNPPVERSION)
	QString linuxPadVersion(); // the version of LinuxPad++, e.g. "0.0.1"
	QString appName();   // "LinuxPad++"
	bool isDarkDesktop();
	QString stripAmpersand(const QString& s);
}
