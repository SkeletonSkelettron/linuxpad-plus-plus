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

#include "EditView.h"

#include <algorithm>
#include <cstring>

#include <stack>

#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QRegularExpression>
#include <QScreen>

#include "LangStyles.h"
#include "Udl.h"
#include "XmlMatchedTags.h"
#include "rgba_icons.h"

namespace
{
	constexpr char g_ZWSP[] = "\xE2\x80\x8B";

	// ScintillaEditView.cpp: C0, C1 and Unicode EOL characters
	constexpr const char* g_ccUniEolChars[][3] = {
		{"\x00", "NUL", "U+0000"}, {"\x01", "SOH", "U+0001"}, {"\x02", "STX", "U+0002"}, {"\x03", "ETX", "U+0003"},
		{"\x04", "EOT", "U+0004"}, {"\x05", "ENQ", "U+0005"}, {"\x06", "ACK", "U+0006"}, {"\a", "BEL", "U+0007"},
		{"\b", "BS", "U+0008"}, {"\v", "VT", "U+000B"}, {"\f", "FF", "U+000C"}, {"\x0E", "SO", "U+000E"},
		{"\x0F", "SI", "U+000F"}, {"\x10", "DLE", "U+0010"}, {"\x11", "DC1", "U+0011"}, {"\x12", "DC2", "U+0012"},
		{"\x13", "DC3", "U+0013"}, {"\x14", "DC4", "U+0014"}, {"\x15", "NAK", "U+0015"}, {"\x16", "SYN", "U+0016"},
		{"\x17", "ETB", "U+0017"}, {"\x18", "CAN", "U+0018"}, {"\x19", "EM", "U+0019"}, {"\x1A", "SUB", "U+001A"},
		{"\x1B", "ESC", "U+001B"}, {"\x1C", "FS", "U+001C"}, {"\x1D", "GS", "U+001D"}, {"\x1E", "RS", "U+001E"},
		{"\x1F", "US", "U+001F"}, {"\x7F", "DEL", "U+007F"},
		{"\xC2\x80", "PAD", "U+0080"}, {"\xC2\x81", "HOP", "U+0081"}, {"\xC2\x82", "BPH", "U+0082"}, {"\xC2\x83", "NBH", "U+0083"},
		{"\xC2\x84", "IND", "U+0084"}, {"\xC2\x86", "SSA", "U+0086"}, {"\xC2\x87", "ESA", "U+0087"}, {"\xC2\x88", "HTS", "U+0088"},
		{"\xC2\x89", "HTJ", "U+0089"}, {"\xC2\x8A", "VTS", "U+008A"}, {"\xC2\x8B", "PLD", "U+008B"}, {"\xC2\x8C", "PLU", "U+008C"},
		{"\xC2\x8D", "RI", "U+008D"}, {"\xC2\x8E", "SS2", "U+008E"}, {"\xC2\x8F", "SS3", "U+008F"}, {"\xC2\x90", "DCS", "U+0090"},
		{"\xC2\x91", "PU1", "U+0091"}, {"\xC2\x92", "PU2", "U+0092"}, {"\xC2\x93", "STS", "U+0093"}, {"\xC2\x94", "CCH", "U+0094"},
		{"\xC2\x95", "MW", "U+0095"}, {"\xC2\x96", "SPA", "U+0096"}, {"\xC2\x97", "EPA", "U+0097"}, {"\xC2\x98", "SOS", "U+0098"},
		{"\xC2\x99", "SGCI", "U+0099"}, {"\xC2\x9A", "SCI", "U+009A"}, {"\xC2\x9B", "CSI", "U+009B"}, {"\xC2\x9C", "ST", "U+009C"},
		{"\xC2\x9D", "OSC", "U+009D"}, {"\xC2\x9E", "PM", "U+009E"}, {"\xC2\x9F", "APC", "U+009F"},
		{"\xC2\x85", "NEL", "U+0085"}, {"\xE2\x80\xA8", "LS", "U+2028"}, {"\xE2\x80\xA9", "PS", "U+2029"}};

	// ScintillaEditView.cpp: non-printing characters
	constexpr const char* g_nonPrintingChars[][3] = {
		{"\xC2\xA0", "NBSP", "U+00A0"}, {"\xC2\xAD", "SHY", "U+00AD"}, {"\xD8\x9C", "ALM", "U+061C"}, {"\xDC\x8F", "SAM", "U+070F"},
		{"\xE1\x9A\x80", "OSPM", "U+1680"}, {"\xE1\xA0\x8E", "MVS", "U+180E"}, {"\xE2\x80\x80", "NQSP", "U+2000"},
		{"\xE2\x80\x81", "MQSP", "U+2001"}, {"\xE2\x80\x82", "ENSP", "U+2002"}, {"\xE2\x80\x83", "EMSP", "U+2003"},
		{"\xE2\x80\x84", "3/MSP", "U+2004"}, {"\xE2\x80\x85", "4/MSP", "U+2005"}, {"\xE2\x80\x86", "6/MSP", "U+2006"},
		{"\xE2\x80\x87", "FSP", "U+2007"}, {"\xE2\x80\x88", "PSP", "U+2008"}, {"\xE2\x80\x89", "THSP", "U+2009"},
		{"\xE2\x80\x8A", "HSP", "U+200A"}, {"\xE2\x80\x8B", "ZWSP", "U+200B"}, {"\xE2\x80\x8C", "ZWNJ", "U+200C"},
		{"\xE2\x80\x8D", "ZWJ", "U+200D"}, {"\xE2\x80\x8E", "LRM", "U+200E"}, {"\xE2\x80\x8F", "RLM", "U+200F"},
		{"\xE2\x80\xAA", "LRE", "U+202A"}, {"\xE2\x80\xAB", "RLE", "U+202B"}, {"\xE2\x80\xAC", "PDF", "U+202C"},
		{"\xE2\x80\xAD", "LRO", "U+202D"}, {"\xE2\x80\xAE", "RLO", "U+202E"}, {"\xE2\x80\xAF", "NNBSP", "U+202F"},
		{"\xE2\x81\x9F", "MMSP", "U+205F"}, {"\xE2\x81\xA0", "WJ", "U+2060"}, {"\xE2\x81\xA1", "(FA)", "U+2061"},
		{"\xE2\x81\xA2", "(IT)", "U+2062"}, {"\xE2\x81\xA3", "(IS)", "U+2063"}, {"\xE2\x81\xA4", "(IP)", "U+2064"},
		{"\xE2\x81\xA6", "LRI", "U+2066"}, {"\xE2\x81\xA7", "RLI", "U+2067"}, {"\xE2\x81\xA8", "FSI", "U+2068"},
		{"\xE2\x81\xA9", "PDI", "U+2069"}, {"\xE2\x81\xAA", "ISS", "U+206A"}, {"\xE2\x81\xAB", "ASS", "U+206B"},
		{"\xE2\x81\xAC", "IAFS", "U+206C"}, {"\xE2\x81\xAD", "AAFS", "U+206D"}, {"\xE2\x81\xAE", "NADS", "U+206E"},
		{"\xE2\x81\xAF", "NODS", "U+206F"}, {"\xE3\x80\x80", "IDSP", "U+3000"}, {"\xEF\xBB\xBF", "ZWNBSP", "U+FEFF"},
		{"\xEF\xBF\xB9", "IAA", "U+FFF9"}, {"\xEF\xBF\xBA", "IAS", "U+FFFA"}, {"\xEF\xBF\xBB", "IAT", "U+FFFB"}};

	int keyDef(int key, int mods) { return key + (mods << 16); }
}

EditView::EditView(QWidget* parent, bool isMainEditor)
	: ScintillaEditBase(parent), _isMain(isMainEditor)
{
	ex(SCI_SETCODEPAGE, SC_CP_UTF8);
	ex(SCI_USEPOPUP, SC_POPUP_NEVER);
	ex(SCI_SETIDLESTYLING, SC_IDLESTYLING_ALL);

	ex(SCI_SETMARGINMASKN, MARGIN_FOLDER, SC_MASK_FOLDERS);
	ex(SCI_SETMARGINMASKN, MARGIN_SYMBOL, (1 << MARK_BOOKMARK) | (1 << MARK_HIDELINESBEGIN) | (1 << MARK_HIDELINESEND));
	ex(SCI_SETMARGINMASKN, MARGIN_CHANGEHISTORY,
	   (1 << SC_MARKNUM_HISTORY_REVERTED_TO_ORIGIN) | (1 << SC_MARKNUM_HISTORY_SAVED) | (1 << SC_MARKNUM_HISTORY_MODIFIED) | (1 << SC_MARKNUM_HISTORY_REVERTED_TO_MODIFIED));
	ex(SCI_SETMARGINMASKN, MARGIN_LINENUMBER, 0);
	ex(SCI_MARKERSETALPHA, MARK_BOOKMARK, 70);

	const bool hiDpi = devicePixelRatioF() >= 1.5;
	ex(SCI_RGBAIMAGESETWIDTH, hiDpi ? 18 : 14);
	ex(SCI_RGBAIMAGESETHEIGHT, hiDpi ? 18 : 14);
	ex(SCI_MARKERDEFINERGBAIMAGE, MARK_BOOKMARK, reinterpret_cast<sptr_t>(hiDpi ? bookmark18 : bookmark14));
	ex(SCI_MARKERDEFINERGBAIMAGE, MARK_HIDELINESBEGIN, reinterpret_cast<sptr_t>(hiDpi ? hidelines_begin18 : hidelines_begin14));
	ex(SCI_MARKERDEFINERGBAIMAGE, MARK_HIDELINESEND, reinterpret_cast<sptr_t>(hiDpi ? hidelines_end18 : hidelines_end14));

	// the line drawn where lines are hidden (View > Hide Lines)
	ex(SCI_SETELEMENTCOLOUR, SC_ELEMENT_HIDDEN_LINE, static_cast<sptr_t>(rgb(0x77, 0xCC, 0x77) | 0xFF000000u));
	ex(SCI_SETMARGINSENSITIVEN, MARGIN_FOLDER, true);
	ex(SCI_SETMARGINSENSITIVEN, MARGIN_SYMBOL, true);
	ex(SCI_SETFOLDFLAGS, SC_FOLDFLAG_LINEAFTER_CONTRACTED);
	ex(SCI_SETSCROLLWIDTHTRACKING, true);
	ex(SCI_SETSCROLLWIDTH, 1);

	for (int indic : {SCE_UNIVERSAL_FOUND_STYLE_SMART, SCE_UNIVERSAL_FOUND_STYLE, SCE_UNIVERSAL_FOUND_STYLE_INC, SCE_UNIVERSAL_TAGMATCH,
	                  SCE_UNIVERSAL_TAGATTR, SCE_UNIVERSAL_FOUND_STYLE_EXT1, SCE_UNIVERSAL_FOUND_STYLE_EXT2, SCE_UNIVERSAL_FOUND_STYLE_EXT3,
	                  SCE_UNIVERSAL_FOUND_STYLE_EXT4, SCE_UNIVERSAL_FOUND_STYLE_EXT5, FINDDLG_HIGHLIGHT_INDIC})
	{
		ex(SCI_INDICSETSTYLE, indic, INDIC_ROUNDBOX);
		ex(SCI_INDICSETALPHA, indic, 100);
		ex(SCI_INDICSETUNDER, indic, true);
	}
	ex(SCI_INDICSETSTYLE, URL_INDIC, INDIC_PLAIN);
	ex(SCI_INDICSETHOVERSTYLE, URL_INDIC, INDIC_FULLBOX);
	ex(SCI_INDICSETALPHA, URL_INDIC, 70);
	ex(SCI_INDICSETFLAGS, URL_INDIC, SC_INDICFLAG_VALUEFORE);

	// Notepad_plus::init()
	ex(SCI_SETMULTIPLESELECTION, true);
	ex(SCI_SETADDITIONALSELECTIONTYPING, true);
	ex(SCI_SETMULTIPASTE, SC_MULTIPASTE_EACH);
	ex(SCI_SETMOUSESELECTIONRECTANGULARSWITCH, true);
	ex(SCI_SETRECTANGULARSELECTIONMODIFIER, SCMOD_ALT);
	ex(SCI_SETPASTECONVERTENDINGS, true);
	ex(SCI_SETLAYOUTCACHE, SC_CACHE_PAGE);
	ex(SCI_SETMOUSEDWELLTIME, 500);
	ex(SCI_SETWRAPINDENTMODE, SC_WRAPINDENT_FIXED);
	ex(SCI_AUTOCSETSEPARATOR, '\n');
	ex(SCI_AUTOCSETIGNORECASE, true);
	ex(SCI_AUTOCSETCASEINSENSITIVEBEHAVIOUR, SC_CASEINSENSITIVEBEHAVIOUR_IGNORECASE);
	ex(SCI_AUTOCSETMAXHEIGHT, 9);

	const sptr_t wcLen = ex(SCI_GETWORDCHARS, 0, 0);
	_defaultWordChars = QByteArray(static_cast<qsizetype>(wcLen + 1), '\0');
	ex(SCI_GETWORDCHARS, 0, reinterpret_cast<sptr_t>(_defaultWordChars.data()));
	_defaultWordChars.truncate(static_cast<qsizetype>(wcLen));

	connect(this, &ScintillaEditBase::notify, this, &EditView::onNotify);
}

int EditView::changeHistoryFlags()
{
	const NppPrefs& p = Npp::prefs();
	if (!p.changeHistoryMargin && !p.changeHistoryIndicator)
		return SC_CHANGE_HISTORY_DISABLED;
	return SC_CHANGE_HISTORY_ENABLED | (p.changeHistoryMargin ? SC_CHANGE_HISTORY_MARKERS : 0) | (p.changeHistoryIndicator ? SC_CHANGE_HISTORY_INDICATORS : 0);
}

void EditView::applyPrefs()
{
	const NppPrefs& p = Npp::prefs();

	showMargin(MARGIN_LINENUMBER, p.showLineNumber);
	showMargin(MARGIN_SYMBOL, p.showBookmarkMargin);
	showMargin(MARGIN_CHANGEHISTORY, p.changeHistoryMargin);
	ex(SCI_SETMARGINWIDTHN, MARGIN_FOLDER, (p.folderStyle != FOLDER_STYLE_NONE) ? 14 : 0);
	ex(SCI_SETMARGINLEFT, 0, p.paddingLeft);
	ex(SCI_SETMARGINRIGHT, 0, p.paddingRight);

	const bool wrap = p.wordWrap && !(_largeFile && p.largeFileDeactivateWrap);
	ex(SCI_SETWRAPMODE, wrap ? SC_WRAP_WORD : SC_WRAP_NONE);
	ex(SCI_SETWRAPVISUALFLAGS, p.showWrapSymbol ? SC_WRAPVISUALFLAG_END : SC_WRAPVISUALFLAG_NONE);
	ex(SCI_SETWRAPINDENTMODE, p.lineWrapMethod == LINEWRAP_ALIGNED ? SC_WRAPINDENT_SAME
	                        : p.lineWrapMethod == LINEWRAP_INDENT ? SC_WRAPINDENT_INDENT : SC_WRAPINDENT_FIXED);
	ex(SCI_SETVIEWWS, p.showSpaceTab ? SCWS_VISIBLEALWAYS : SCWS_INVISIBLE);
	ex(SCI_SETVIEWEOL, p.showEol);
	ex(SCI_SETINDENTATIONGUIDES, p.showIndentGuide ? SC_IV_LOOKBOTH : SC_IV_NONE);
	ex(SCI_SETCARETLINEVISIBLE, p.currentLineMode != LINEHILITE_NONE);
	ex(SCI_SETCARETLINEFRAME, p.currentLineMode == LINEHILITE_FRAME ? p.currentLineFrameWidth : 0);

	// NppBigSwitch.cpp NPPM_INTERNAL_SETCARETWIDTH
	if (p.caretWidth < 4)
	{
		ex(SCI_SETCARETSTYLE, CARETSTYLE_LINE);
		ex(SCI_SETCARETWIDTH, p.caretWidth);
	}
	else
	{
		ex(SCI_SETCARETWIDTH, 1);
		ex(SCI_SETCARETSTYLE, p.caretWidth == 4 ? CARETSTYLE_BLOCK : (CARETSTYLE_BLOCK | CARETSTYLE_BLOCK_AFTER));
	}
	ex(SCI_SETCARETPERIOD, p.caretBlinkRate);
	ex(SCI_SETFONTQUALITY, p.smoothFont ? SC_EFF_QUALITY_LCD_OPTIMIZED : SC_EFF_QUALITY_DEFAULT);

	applyEdges();
	ex(SCI_SETENDATLASTLINE, !p.scrollBeyondLastLine);
	ex(SCI_SETMULTIPLESELECTION, p.multiEditing);
	ex(SCI_SETVIRTUALSPACEOPTIONS, SCVS_RECTANGULARSELECTION | (p.virtualSpace ? SCVS_USERACCESSIBLE : 0));
	ex(SCI_SETDRAGDROPENABLED, !p.disableSelectedTextDragDrop);
	ex(SCI_SETTABWIDTH, p.tabSize);
	ex(SCI_SETUSETABS, !p.replaceTabBySpace);
	ex(SCI_SETBACKSPACEUNINDENTS, p.backspaceUnindent);

	// clickable links: Notepad_plus::addHotSpot()
	const bool noUnderline = p.urlNoUnderline;
	ex(SCI_INDICSETSTYLE, URL_INDIC, noUnderline ? INDIC_HIDDEN : INDIC_PLAIN);
	ex(SCI_INDICSETHOVERSTYLE, URL_INDIC, p.urlFullBox ? INDIC_FULLBOX : INDIC_EXPLORERLINK);

	// Preferences > Margins/Border/Edge > Border width and No edge
	setFrameShape(p.showBorderEdge ? QFrame::StyledPanel : QFrame::NoFrame);

	applyWordChars();
	applyKeyBindings();
	applyNonPrintingChars();
	updateLineNumberWidth();
}

void EditView::applyEdges()
{
	// ScintillaEditView::setEdgeMode(): one column, several columns or background mode
	const NppPrefs& p = Npp::prefs();
	const QList<int> cols = p.edgeColumnList();
	ex(SCI_MULTIEDGECLEARALL);
	if (cols.isEmpty())
	{
		ex(SCI_SETEDGEMODE, EDGE_NONE);
	}
	else if (cols.size() == 1)
	{
		ex(SCI_SETEDGECOLUMN, cols.first());
		ex(SCI_SETEDGEMODE, p.edgeBgMode ? EDGE_BACKGROUND : EDGE_LINE);
	}
	else
	{
		ex(SCI_SETEDGEMODE, EDGE_MULTILINE);
		const sptr_t edgeColour = ex(SCI_GETEDGECOLOUR);
		for (int c : cols)
			ex(SCI_MULTIEDGEADDLINE, c, edgeColour);
	}
}

void EditView::applyWordChars()
{
	// ScintillaEditView::setWordChars()
	const NppPrefs& p = Npp::prefs();
	QByteArray chars = _defaultWordChars;
	if (!p.wordCharsDefault)
	{
		for (char c : p.customWordChars.toLatin1())
		{
			if (c > ' ' && !chars.contains(c))
				chars += c;
		}
	}
	exs(SCI_SETWORDCHARS, 0, chars.constData());
}

void EditView::applyKeyBindings()
{
	// "Enable Copy/Cut Line without selection" (NppCommands.cpp IDM_EDIT_CUT / IDM_EDIT_COPY)
	const bool line = Npp::prefs().lineCopyCutWithoutSelection;
	const int copy = line ? SCI_COPYALLOWLINE : SCI_COPY;
	const int cut = line ? SCI_CUTALLOWLINE : SCI_CUT;
	ex(SCI_ASSIGNCMDKEY, keyDef('C', SCMOD_CTRL), copy);
	ex(SCI_ASSIGNCMDKEY, keyDef(SCK_INSERT, SCMOD_CTRL), copy);
	ex(SCI_ASSIGNCMDKEY, keyDef('X', SCMOD_CTRL), cut);
	ex(SCI_ASSIGNCMDKEY, keyDef(SCK_DELETE, SCMOD_SHIFT), cut);
}

void EditView::applyNonPrintingChars()
{
	// ScintillaEditView::showNpc(), showCcUniEol(), setNpcAndCcUniEOL()
	const NppPrefs& p = Npp::prefs();
	ex(SCI_CLEARALLREPRESENTATIONS);

	COLORREF npcColor = rgb(0xC0, 0xC0, 0xC0);
	if (const Style* st = StyleManager::instance().globalStyles().findByName(QStringLiteral("Non-printing characters custom color")))
		npcColor = st->fgColor;
	const sptr_t alphaColor = static_cast<sptr_t>(npcColor | 0xFF000000);
	const long appearance = p.npcCustomColor ? (SC_REPRESENTATION_BLOB | SC_REPRESENTATION_COLOUR) : SC_REPRESENTATION_BLOB;
	const int mode = std::clamp(p.npcMode, 1, 2);

	if (p.showCcUniEol)
	{
		const int ccMode = p.npcIncludeCcUniEol ? mode : NPC_ABBREVIATION;
		for (const auto& c : g_ccUniEolChars)
		{
			ex(SCI_SETREPRESENTATION, reinterpret_cast<uptr_t>(c[0]), reinterpret_cast<sptr_t>(c[ccMode]));
			if (p.npcIncludeCcUniEol && p.npcCustomColor)
			{
				ex(SCI_SETREPRESENTATIONCOLOUR, reinterpret_cast<uptr_t>(c[0]), alphaColor);
				ex(SCI_SETREPRESENTATIONAPPEARANCE, reinterpret_cast<uptr_t>(c[0]), appearance);
			}
		}
	}
	else
	{
		for (const auto& c : g_ccUniEolChars)
		{
			ex(SCI_SETREPRESENTATION, reinterpret_cast<uptr_t>(c[0]), reinterpret_cast<sptr_t>(g_ZWSP));
			ex(SCI_SETREPRESENTATIONAPPEARANCE, reinterpret_cast<uptr_t>(c[0]), SC_REPRESENTATION_PLAIN);
		}
	}

	if (p.showNpc)
	{
		for (const auto& c : g_nonPrintingChars)
		{
			ex(SCI_SETREPRESENTATION, reinterpret_cast<uptr_t>(c[0]), reinterpret_cast<sptr_t>(c[mode]));
			if (p.npcCustomColor)
			{
				ex(SCI_SETREPRESENTATIONCOLOUR, reinterpret_cast<uptr_t>(c[0]), alphaColor);
				ex(SCI_SETREPRESENTATIONAPPEARANCE, reinterpret_cast<uptr_t>(c[0]), appearance);
			}
		}
	}

	// SCI_CLEARALLREPRESENTATIONS also reset the CR LF appearance (Editing 2 > EOL)
	StyleManager::instance().applyEolAppearance(this);
	ex(SCI_SETVIEWEOL, p.showEol);
}

bool EditView::allowSmartHilite() const
{
	return !_largeFile || Npp::prefs().largeFileAllowSmartHilite;
}

bool EditView::allowBraceMatch() const
{
	return !_largeFile || Npp::prefs().largeFileAllowBraceMatch;
}

bool EditView::allowClickableLink() const
{
	return !_largeFile || Npp::prefs().largeFileAllowClickableLink;
}

bool EditView::allowAutoCompletion() const
{
	return !_largeFile || Npp::prefs().largeFileAllowAutoCompletion;
}

void EditView::showMargin(int margin, bool show)
{
	if (margin == MARGIN_LINENUMBER)
	{
		if (show)
			updateLineNumberWidth();
		else
			ex(SCI_SETMARGINWIDTHN, MARGIN_LINENUMBER, 0);
		return;
	}
	int width = 0;
	if (show)
	{
		if (margin == MARGIN_SYMBOL)
			width = devicePixelRatioF() >= 1.5 ? 20 : 16;
		else if (margin == MARGIN_CHANGEHISTORY)
			width = 9;
		else if (margin == MARGIN_FOLDER)
			width = 14;
	}
	ex(SCI_SETMARGINWIDTHN, margin, width);
}

void EditView::defineDocType(LangType lang, const QString& udlName)
{
	_lang = lang;
	_udlName = (lang == L_USER || lang == L_EXTERNAL) ? udlName : QString();
	StyleManager& sm = StyleManager::instance();
	const UserLang* udl = nullptr;
	if (lang == L_USER)
	{
		UdlManager& um = UdlManager::instance();
		udl = um.find(udlName);
		if (!udl)
			udl = &um.scratch(); // User-Defined: the language of the dialog
	}
	// Large File Restriction: no syntax highlighting (Buffer::isLargeFile)
	sm.defineDocType(this, _largeFile ? L_TEXT : lang, udl, ex(SCI_GETDOCPOINTER), lang == L_EXTERNAL ? udlName : QString());
	sm.performGlobalStyles(this);
	if (_isMain)
	{
		const NppPrefs& p = Npp::prefs();
		if (!p.showIndentGuide)
			ex(SCI_SETINDENTATIONGUIDES, SC_IV_NONE);
		if (p.folderStyle == FOLDER_STYLE_NONE)
			ex(SCI_SETMARGINWIDTHN, MARGIN_FOLDER, 0);
	}
	if (_isMain)
		applyNonPrintingChars();
	ex(SCI_COLOURISE, 0, -1);
	updateLineNumberWidth();
}

void EditView::updateLineNumberWidth()
{
	// ScintillaEditView::updateLineNumberWidth(): dynamic or constant width
	const NppPrefs& p = Npp::prefs();
	if (!p.showLineNumber || !_isMain)
		return;
	auto digits = [](sptr_t n)
	{
		int d = 1;
		for (; n >= 10; n /= 10)
			++d;
		return d;
	};
	int nbDigits = 0;
	if (p.lineNumberDynamicWidth)
	{
		const sptr_t lastVisible = ex(SCI_DOCLINEFROMVISIBLE, ex(SCI_GETFIRSTVISIBLELINE) + ex(SCI_LINESONSCREEN) + 1);
		nbDigits = std::max(digits(lastVisible), 3);
	}
	else
	{
		nbDigits = std::max(digits(ex(SCI_GETLINECOUNT)), 4);
	}
	const sptr_t pixelWidth = 8 + nbDigits * exs(SCI_TEXTWIDTH, STYLE_LINENUMBER, "8");
	ex(SCI_SETMARGINWIDTHN, MARGIN_LINENUMBER, pixelWidth);
}

// ---------------------------------------------------------------------------
// text helpers
// ---------------------------------------------------------------------------
QByteArray EditView::textRange(sptr_t start, sptr_t end) const
{
	if (end <= start)
		return {};
	QByteArray buf(static_cast<qsizetype>(end - start), Qt::Uninitialized);
	Sci_TextRangeFull tr;
	tr.chrg.cpMin = start;
	tr.chrg.cpMax = end;
	QByteArray tmp(static_cast<qsizetype>(end - start + 1), '\0');
	tr.lpstrText = tmp.data();
	ex(SCI_GETTEXTRANGEFULL, 0, reinterpret_cast<sptr_t>(&tr));
	tmp.truncate(end - start);
	return tmp;
}

void EditView::getText(char* dest, sptr_t start, sptr_t end) const
{
	Sci_TextRangeFull tr;
	tr.chrg.cpMin = start;
	tr.chrg.cpMax = end;
	tr.lpstrText = dest;
	ex(SCI_GETTEXTRANGEFULL, 0, reinterpret_cast<sptr_t>(&tr));
}

bool EditView::isCurrentLineFolded() const
{
	sptr_t line = ex(SCI_LINEFROMPOSITION, ex(SCI_GETCURRENTPOS));
	if (!(ex(SCI_GETFOLDLEVEL, line) & SC_FOLDLEVELHEADERFLAG))
		line = ex(SCI_GETFOLDPARENT, line);
	return line >= 0 && !ex(SCI_GETFOLDEXPANDED, line);
}

QByteArray EditView::selectedTextUtf8() const
{
	const sptr_t len = ex(SCI_GETSELTEXT, 0, 0);
	if (len <= 0)
		return {};
	QByteArray buf(static_cast<qsizetype>(len + 1), '\0');
	ex(SCI_GETSELTEXT, 0, reinterpret_cast<sptr_t>(buf.data()));
	buf.truncate(static_cast<qsizetype>(std::strlen(buf.constData())));
	return buf;
}

QByteArray EditView::wholeText() const
{
	const sptr_t len = ex(SCI_GETLENGTH);
	const char* p = reinterpret_cast<const char*>(ex(SCI_GETCHARACTERPOINTER));
	return QByteArray(p, static_cast<qsizetype>(len));
}

QByteArray EditView::lineText(sptr_t line) const
{
	return textRange(ex(SCI_POSITIONFROMLINE, line), ex(SCI_GETLINEENDPOSITION, line));
}

QByteArray EditView::wordAt(sptr_t pos, sptr_t* start, sptr_t* end) const
{
	const sptr_t s = ex(SCI_WORDSTARTPOSITION, pos, true);
	const sptr_t e = ex(SCI_WORDENDPOSITION, pos, true);
	if (start)
		*start = s;
	if (end)
		*end = e;
	return textRange(s, e);
}

QString EditView::currentWordOrSelection() const
{
	if (!ex(SCI_GETSELECTIONEMPTY))
		return selectedText();
	return QString::fromUtf8(wordAt(ex(SCI_GETCURRENTPOS)));
}

std::pair<sptr_t, sptr_t> EditView::selectionLinesRange() const
{
	const sptr_t selStart = ex(SCI_GETSELECTIONSTART);
	const sptr_t selEnd = ex(SCI_GETSELECTIONEND);
	const sptr_t lineStart = ex(SCI_LINEFROMPOSITION, selStart);
	sptr_t lineEnd = ex(SCI_LINEFROMPOSITION, selEnd);
	if (lineEnd > lineStart && selEnd == ex(SCI_POSITIONFROMLINE, lineEnd))
		--lineEnd;
	return {lineStart, lineEnd};
}

void EditView::replaceRange(sptr_t start, sptr_t end, const QByteArray& text)
{
	ex(SCI_SETTARGETRANGE, start, end);
	ex(SCI_REPLACETARGET, text.size(), reinterpret_cast<sptr_t>(text.constData()));
}

void EditView::scrollPosToCenter(sptr_t pos)
{
	ex(SCI_GOTOPOS, pos);
	const sptr_t line = ex(SCI_LINEFROMPOSITION, pos);
	const sptr_t firstVisible = ex(SCI_GETFIRSTVISIBLELINE);
	const sptr_t onScreen = ex(SCI_LINESONSCREEN);
	const sptr_t visLine = ex(SCI_VISIBLEFROMDOCLINE, line);
	if (visLine < firstVisible || visLine > firstVisible + onScreen - 1)
		ex(SCI_SETFIRSTVISIBLELINE, std::max<sptr_t>(0, visLine - onScreen / 2));
}

void EditView::gotoLine(sptr_t line)
{
	ex(SCI_ENSUREVISIBLE, line);
	scrollPosToCenter(ex(SCI_POSITIONFROMLINE, line));
}

void EditView::displaySection(sptr_t start, sptr_t end, bool forward)
{
	// Searching::displaySectionCentered()
	const sptr_t lineStart = ex(SCI_LINEFROMPOSITION, start);
	const sptr_t lineEnd = ex(SCI_LINEFROMPOSITION, end);
	for (sptr_t l = lineStart; l <= lineEnd; ++l)
		ex(SCI_ENSUREVISIBLE, l);
	const sptr_t firstVisible = ex(SCI_GETFIRSTVISIBLELINE);
	const sptr_t onScreen = ex(SCI_LINESONSCREEN);
	const sptr_t visStart = ex(SCI_VISIBLEFROMDOCLINE, lineStart);
	if (visStart < firstVisible || visStart >= firstVisible + onScreen)
		ex(SCI_SETFIRSTVISIBLELINE, std::max<sptr_t>(0, visStart - onScreen / 2));
	if (forward)
		ex(SCI_SETSEL, start, end);
	else
		ex(SCI_SETSEL, end, start);
	ex(SCI_SCROLLRANGE, end, start);
	ex(SCI_CHOOSECARETX);
}

QByteArray EditView::eolString() const
{
	switch (ex(SCI_GETEOLMODE))
	{
		case SC_EOL_CRLF: return QByteArrayLiteral("\r\n");
		case SC_EOL_CR: return QByteArrayLiteral("\r");
		default: return QByteArrayLiteral("\n");
	}
}

sptr_t EditView::lineLength(sptr_t line) const
{
	return ex(SCI_GETLINEENDPOSITION, line) - ex(SCI_POSITIONFROMLINE, line);
}

void EditView::setLineIndent(sptr_t line, sptr_t indent) const
{
	if (indent < 0)
		return;
	sptr_t cpMin = ex(SCI_GETSELECTIONSTART);
	sptr_t cpMax = ex(SCI_GETSELECTIONEND);
	const sptr_t posBefore = ex(SCI_GETLINEINDENTPOSITION, line);
	ex(SCI_SETLINEINDENTATION, line, indent);
	const sptr_t posAfter = ex(SCI_GETLINEINDENTPOSITION, line);
	const sptr_t diff = posAfter - posBefore;
	if (posAfter > posBefore)
	{
		if (cpMin >= posBefore)
			cpMin += diff;
		if (cpMax >= posBefore)
			cpMax += diff;
	}
	else if (posAfter < posBefore)
	{
		if (cpMin >= posAfter)
			cpMin = (cpMin >= posBefore) ? cpMin + diff : posAfter;
		if (cpMax >= posAfter)
			cpMax = (cpMax >= posBefore) ? cpMax + diff : posAfter;
	}
	ex(SCI_SETSEL, cpMin, cpMax);
}

// ---------------------------------------------------------------------------
// features
// ---------------------------------------------------------------------------
void EditView::clearIndicator(int indic)
{
	ex(SCI_SETINDICATORCURRENT, indic);
	ex(SCI_INDICATORCLEARRANGE, 0, ex(SCI_GETLENGTH));
}

void EditView::markAllOccurrences(int indic, const QByteArray& text, int flags)
{
	if (text.isEmpty())
		return;
	ex(SCI_SETINDICATORCURRENT, indic);
	ex(SCI_SETSEARCHFLAGS, flags);
	sptr_t pos = 0;
	const sptr_t docEnd = ex(SCI_GETLENGTH);
	while (pos <= docEnd)
	{
		ex(SCI_SETTARGETRANGE, pos, docEnd);
		const sptr_t found = ex(SCI_SEARCHINTARGET, text.size(), reinterpret_cast<sptr_t>(text.constData()));
		if (found < 0)
			break;
		const sptr_t end = ex(SCI_GETTARGETEND);
		if (end > found)
			ex(SCI_INDICATORFILLRANGE, found, end - found);
		pos = (end > found) ? end : found + 1;
	}
}

void EditView::doSmartHighlight()
{
	// SmartHighlighter::highlightView()
	clearIndicator(SCE_UNIVERSAL_FOUND_STYLE_SMART);
	QByteArray text;
	int flags = 0;
	if (smartHighlightTarget(&text, &flags))
		highlightVisible(SCE_UNIVERSAL_FOUND_STYLE_SMART, text, flags);
}

bool EditView::smartHighlightTarget(QByteArray* text, int* flags) const
{
	const NppPrefs& p = Npp::prefs();
	if (!p.smartHighlight || !allowSmartHilite() || ex(SCI_GETSELECTIONEMPTY) || ex(SCI_GETSELECTIONS) > 1)
		return false;
	// "Use Find dialog settings": match case and whole word of the Find dialog
	const bool matchCase = p.smartHighlightUseFindSettings ? Npp::findState().matchCase : p.smartHighlightMatchCase;
	const bool wholeWord = p.smartHighlightUseFindSettings ? Npp::findState().wholeWord : p.smartHighlightWholeWord;

	const sptr_t selStart = ex(SCI_GETSELECTIONSTART);
	const sptr_t selEnd = ex(SCI_GETSELECTIONEND);
	const sptr_t curPos = ex(SCI_GETCURRENTPOS);
	const sptr_t wordStart = ex(SCI_WORDSTARTPOSITION, curPos, true);
	const sptr_t wordEnd = ex(SCI_WORDENDPOSITION, wordStart, true);
	if (wholeWord && (wordStart != selStart || wordEnd != selEnd))
		return false;

	*text = textRange(selStart, selEnd);
	if (text->isEmpty() || text->contains('\n') || text->contains('\r') || text->size() > 1024)
		return false;
	*flags = (matchCase ? SCFIND_MATCHCASE : 0) | (wholeWord ? SCFIND_WHOLEWORD : 0);
	return true;
}

void EditView::highlightVisible(int indic, const QByteArray& text, int flags)
{
	ex(SCI_SETINDICATORCURRENT, indic);
	ex(SCI_SETSEARCHFLAGS, flags);
	const sptr_t firstLine = ex(SCI_GETFIRSTVISIBLELINE);
	const sptr_t nbLines = std::min<sptr_t>(ex(SCI_LINESONSCREEN), 400) + 1;
	sptr_t prevDocLine = -1;
	for (sptr_t i = firstLine; i <= firstLine + nbLines; ++i)
	{
		const sptr_t docLine = ex(SCI_DOCLINEFROMVISIBLE, i);
		if (docLine == prevDocLine)
			continue;
		prevDocLine = docLine;
		sptr_t start = ex(SCI_POSITIONFROMLINE, docLine);
		const sptr_t end = ex(SCI_GETLINEENDPOSITION, docLine);
		if (start < 0)
			break;
		while (start < end)
		{
			ex(SCI_SETTARGETRANGE, start, end);
			const sptr_t found = ex(SCI_SEARCHINTARGET, text.size(), reinterpret_cast<sptr_t>(text.constData()));
			if (found < 0)
				break;
			const sptr_t fEnd = ex(SCI_GETTARGETEND);
			ex(SCI_INDICATORFILLRANGE, found, fEnd - found);
			start = std::max(fEnd, found + 1);
		}
	}
}

void EditView::doBraceMatch()
{
	// Notepad_plus::braceMatch() / findMatchingBracePos()
	if (!Npp::prefs().braceHighlight || !allowBraceMatch())
	{
		ex(SCI_BRACEHIGHLIGHT, -1, -1);
		return;
	}
	const sptr_t caretPos = ex(SCI_GETCURRENTPOS);
	const sptr_t lengthDoc = ex(SCI_GETLENGTH);
	sptr_t braceAtCaret = -1;
	sptr_t braceOpposite = -1;
	auto isBrace = [](int c) { return c && std::strchr("[](){}", c) != nullptr; };

	if (lengthDoc > 0 && caretPos > 0)
	{
		const int charBefore = static_cast<int>(ex(SCI_GETCHARAT, caretPos - 1));
		if (isBrace(charBefore))
			braceAtCaret = caretPos - 1;
	}
	if (lengthDoc > 0 && braceAtCaret < 0)
	{
		const int charAfter = static_cast<int>(ex(SCI_GETCHARAT, caretPos));
		if (isBrace(charAfter))
			braceAtCaret = caretPos;
	}
	if (braceAtCaret >= 0)
		braceOpposite = ex(SCI_BRACEMATCH, braceAtCaret, 0);

	if (braceAtCaret != -1 && braceOpposite == -1)
	{
		ex(SCI_BRACEBADLIGHT, braceAtCaret);
		ex(SCI_SETHIGHLIGHTGUIDE, 0);
	}
	else
	{
		ex(SCI_BRACEHIGHLIGHT, braceAtCaret, braceOpposite);
		if (Npp::prefs().showIndentGuide && braceAtCaret >= 0)
		{
			const sptr_t c1 = ex(SCI_GETCOLUMN, braceAtCaret);
			const sptr_t c2 = ex(SCI_GETCOLUMN, braceOpposite);
			ex(SCI_SETHIGHLIGHTGUIDE, std::min(c1, c2));
		}
		else
		{
			ex(SCI_SETHIGHLIGHTGUIDE, 0);
		}
	}
}

void EditView::doTagMatch()
{
	const NppPrefs& p = Npp::prefs();
	if (!p.tagMatchHilite)
	{
		clearIndicator(SCE_UNIVERSAL_TAGMATCH);
		clearIndicator(SCE_UNIVERSAL_TAGATTR);
		return;
	}
	XmlMatchedTagsHighlighter highlighter(this);
	highlighter.tagMatch(p.tagAttrHilite);
}

bool EditView::isConditionExprLine(sptr_t line) const
{
	if (line < 0 || line >= ex(SCI_GETLINECOUNT))
		return false;
	const sptr_t startPos = ex(SCI_POSITIONFROMLINE, line);
	const sptr_t endPos = ex(SCI_GETLINEENDPOSITION, line);
	ex(SCI_SETSEARCHFLAGS, SCFIND_REGEXP | SCFIND_POSIX);
	ex(SCI_SETTARGETRANGE, startPos, endPos);
	const char ifElseForWhileExpr[] = "((else[ \t]+)?if|for|while)[ \t]*[(].*[)][ \t]*|else[ \t]*";
	const sptr_t posFound = exs(SCI_SEARCHINTARGET, std::strlen(ifElseForWhileExpr), ifElseForWhileExpr);
	if (posFound >= 0)
	{
		const sptr_t end = ex(SCI_GETTARGETEND);
		if (end == endPos)
			return true;
	}
	return false;
}

void EditView::maintainIndentation(int ch)
{
	// Port of Notepad_plus::maintainIndentation()
	const int mode = Npp::prefs().autoIndentMode;
	if (mode == AUTOINDENT_NONE)
		return;

	const sptr_t eolMode = ex(SCI_GETEOLMODE);
	const sptr_t curLine = ex(SCI_LINEFROMPOSITION, ex(SCI_GETCURRENTPOS));
	sptr_t prevLine = curLine - 1;
	sptr_t indentAmountPrevLine = 0;
	const sptr_t tabWidth = ex(SCI_GETTABWIDTH);
	const LangType type = _lang;
	const bool isNewLine = ((eolMode == SC_EOL_CRLF || eolMode == SC_EOL_LF) && ch == '\n') || (eolMode == SC_EOL_CR && ch == '\r');

	if (mode == AUTOINDENT_BASIC)
	{
		if (isNewLine)
		{
			while (prevLine >= 0 && lineLength(prevLine) == 0)
				--prevLine;
			if (prevLine >= 0)
				indentAmountPrevLine = ex(SCI_GETLINEINDENTATION, prevLine);
			if (indentAmountPrevLine > 0)
				setLineIndent(curLine, indentAmountPrevLine);
		}
		return;
	}

	if (isNewLine && prevLine >= 0 && lineLength(prevLine) == 0)
		return;

	const bool cLike = type == L_C || type == L_CPP || type == L_JAVA || type == L_CS || type == L_OBJC || type == L_PHP ||
	                   type == L_JS_EMBEDDED || type == L_JAVASCRIPT || type == L_JSP || type == L_CSS || type == L_PERL ||
	                   type == L_RUST || type == L_POWERSHELL || type == L_JSON || type == L_JSON5 || type == L_GOLANG ||
	                   type == L_SWIFT || type == L_TYPESCRIPT;

	if (cLike)
	{
		if (isNewLine)
		{
			while (prevLine >= 0 && lineLength(prevLine) == 0)
				--prevLine;
			if (prevLine >= 0)
				indentAmountPrevLine = ex(SCI_GETLINEINDENTATION, prevLine);

			const sptr_t prevPos = ex(SCI_GETCURRENTPOS) - (eolMode == SC_EOL_CRLF ? 3 : 2);
			const unsigned char prevChar = static_cast<unsigned char>(ex(SCI_GETCHARAT, prevPos));
			const sptr_t curPos = ex(SCI_GETCURRENTPOS);
			const unsigned char nextChar = static_cast<unsigned char>(ex(SCI_GETCHARAT, curPos));

			if (prevChar == '{')
			{
				if (nextChar == '}')
				{
					const char* eolChars = (eolMode == SC_EOL_CRLF) ? "\r\n" : (eolMode == SC_EOL_LF) ? "\n" : "\r";
					exs(SCI_INSERTTEXT, ex(SCI_GETCURRENTPOS), eolChars);
					setLineIndent(curLine + 1, indentAmountPrevLine);
				}
				setLineIndent(curLine, indentAmountPrevLine + tabWidth);
			}
			else if (nextChar == '{')
			{
				setLineIndent(curLine, indentAmountPrevLine);
			}
			else if (type == L_PERL || type == L_RUST || type == L_POWERSHELL || type == L_JSON || type == L_JSON5)
			{
				setLineIndent(curLine, indentAmountPrevLine);
			}
			else if (isConditionExprLine(prevLine))
			{
				setLineIndent(curLine, indentAmountPrevLine + tabWidth);
			}
			else if (indentAmountPrevLine > 0)
			{
				if (prevLine > 0 && isConditionExprLine(prevLine - 1))
					setLineIndent(curLine, indentAmountPrevLine - tabWidth);
				else
					setLineIndent(curLine, indentAmountPrevLine);
			}
		}
		else if (ch == '{')
		{
			const sptr_t startPos = ex(SCI_POSITIONFROMLINE, curLine);
			const sptr_t endPos = ex(SCI_GETCURRENTPOS);
			for (sptr_t i = endPos - 2; i > 0 && i > startPos; --i)
			{
				const unsigned char aChar = static_cast<unsigned char>(ex(SCI_GETCHARAT, i));
				if (aChar != ' ' && aChar != '\t')
					return;
			}
			while (prevLine >= 0 && lineLength(prevLine) == 0)
				--prevLine;
			if (prevLine >= 0)
			{
				indentAmountPrevLine = ex(SCI_GETLINEINDENTATION, prevLine);
				const sptr_t startPos2 = ex(SCI_POSITIONFROMLINE, prevLine);
				const sptr_t endPos2 = ex(SCI_GETLINEENDPOSITION, prevLine);
				ex(SCI_SETSEARCHFLAGS, SCFIND_REGEXP | SCFIND_POSIX);
				ex(SCI_SETTARGETRANGE, startPos2, endPos2);
				const char braceExpr[] = "[ \t]*\\{.*";
				const sptr_t posFound = exs(SCI_SEARCHINTARGET, std::strlen(braceExpr), braceExpr);
				if (posFound >= 0 && ex(SCI_GETTARGETEND) == endPos2)
					indentAmountPrevLine += tabWidth;
			}
			setLineIndent(curLine, indentAmountPrevLine);
		}
		else if (ch == '}')
		{
			sptr_t startPos = ex(SCI_GETCURRENTPOS);
			if (startPos != 0)
				startPos -= 1;
			const sptr_t posFound = ex(SCI_BRACEMATCH, startPos, 0);
			if (posFound == -1)
				return;
			const sptr_t matchedPairLine = ex(SCI_LINEFROMPOSITION, posFound);
			if (matchedPairLine == curLine)
				return;
			setLineIndent(curLine, ex(SCI_GETLINEINDENTATION, matchedPairLine));
		}
	}
	else if (type == L_PYTHON)
	{
		if (isNewLine)
		{
			while (prevLine >= 0 && lineLength(prevLine) == 0)
				--prevLine;
			if (prevLine < 0)
				return;
			indentAmountPrevLine = ex(SCI_GETLINEINDENTATION, prevLine);
			ex(SCI_SETSEARCHFLAGS, SCFIND_REGEXP | SCFIND_POSIX);
			ex(SCI_SETTARGETRANGE, ex(SCI_POSITIONFROMLINE, prevLine), ex(SCI_GETLINEENDPOSITION, prevLine));
			const char colonExpr[] = ":[ \t]*(#|$)";
			if (exs(SCI_SEARCHINTARGET, std::strlen(colonExpr), colonExpr) >= 0)
				setLineIndent(curLine, indentAmountPrevLine + tabWidth);
			else
				setLineIndent(curLine, indentAmountPrevLine);
		}
	}
	else if (isNewLine)
	{
		while (prevLine >= 0 && lineLength(prevLine) == 0)
			--prevLine;
		if (prevLine >= 0)
			indentAmountPrevLine = ex(SCI_GETLINEINDENTATION, prevLine);
		if (indentAmountPrevLine > 0)
			setLineIndent(curLine, indentAmountPrevLine);
	}
}

// ---------------------------------------------------------------------------
// AutoCompletion::insertMatchedChars() and InsertedMatchedChars
// ---------------------------------------------------------------------------
void EditView::removeInvalidMatchedChars(const MatchedCharInserted& mci)
{
	if (mci.c == '\n' || mci.c == '\r')
	{
		_insertedMatchedChars.clear();
		return;
	}
	for (int i = static_cast<int>(_insertedMatchedChars.size()) - 1; i >= 0; --i)
	{
		const MatchedCharInserted& m = _insertedMatchedChars[static_cast<size_t>(i)];
		if (m.pos < mci.pos && ex(SCI_LINEFROMPOSITION, mci.pos) == ex(SCI_LINEFROMPOSITION, m.pos))
			continue;
		_insertedMatchedChars.erase(_insertedMatchedChars.begin() + i);
	}
}

sptr_t EditView::searchMatchedChar(char startChar, char endChar, sptr_t posToDetect)
{
	if (_insertedMatchedChars.empty())
		return -1;
	const sptr_t posToDetectLine = ex(SCI_LINEFROMPOSITION, posToDetect);
	for (int i = static_cast<int>(_insertedMatchedChars.size()) - 1; i >= 0; --i)
	{
		const MatchedCharInserted m = _insertedMatchedChars[static_cast<size_t>(i)];
		if (m.c != startChar)
			continue;
		_insertedMatchedChars.erase(_insertedMatchedChars.begin() + i);
		if (m.pos >= posToDetect || ex(SCI_LINEFROMPOSITION, m.pos) != posToDetectLine)
			continue;
		const sptr_t endPos = ex(SCI_GETLINEENDPOSITION, posToDetectLine);
		for (sptr_t j = posToDetect; j <= endPos; ++j)
		{
			const char aChar = static_cast<char>(ex(SCI_GETCHARAT, j));
			if (aChar != ' ')
				return (aChar == endChar) ? j : -1;
		}
	}
	return -1;
}

QByteArray EditView::closeTag(sptr_t caretPos, bool isHtml) const
{
	// AutoCompletion::getCloseTag()
	if (isHtml && ex(SCI_GETSTYLEAT, caretPos) >= SCE_HJ_START)
		return {};
	const char prev = static_cast<char>(ex(SCI_GETCHARAT, caretPos - 2));
	const char prevprev = static_cast<char>(ex(SCI_GETCHARAT, caretPos - 3));
	if ((prevprev == '-' && prev == '-') || prev == '/')
		return {};

	ex(SCI_SETSEARCHFLAGS, SCFIND_REGEXP | SCFIND_POSIX);
	ex(SCI_SETTARGETRANGE, caretPos, 0);
	const char tag2find[] = "<[^\\s>]*";
	const sptr_t targetStart = exs(SCI_SEARCHINTARGET, std::strlen(tag2find), tag2find);
	if (targetStart < 0)
		return {};
	const sptr_t targetEnd = ex(SCI_GETTARGETEND);
	if (targetEnd - targetStart < 2 || targetEnd - targetStart > 254)
		return {};
	const QByteArray tagHead = textRange(targetStart, targetEnd);
	if (tagHead.size() < 2 || tagHead[1] == '/' || tagHead[1] == '?' || tagHead.startsWith("<!--"))
		return {};
	if (isHtml)
	{
		static const char* const voidTags[] = {"area", "base", "br", "col", "embed", "hr", "img", "input", "keygen", "link", "meta",
		                                       "param", "source", "track", "wbr", "!doctype"};
		const QByteArray name = tagHead.mid(1).toLower();
		for (const char* t : voidTags)
		{
			if (name.startsWith(t))
				return {};
		}
	}
	return "</" + tagHead.mid(1) + ">";
}

void EditView::insertMatchedChars(int character)
{
	const NppPrefs& p = Npp::prefs();
	const sptr_t caretPos = ex(SCI_GETCURRENTPOS);
	if (character > 127 || character < 0)
	{
		if (!_insertedMatchedChars.empty())
			removeInvalidMatchedChars({'\0', caretPos - 1});
		return;
	}
	if (ex(SCI_GETSELECTIONS) > 1)
		return;

	const char ch = static_cast<char>(character);
	const char charPrev = static_cast<char>(ex(SCI_GETCHARAT, caretPos - 2));
	const char charNext = static_cast<char>(ex(SCI_GETCHARAT, caretPos));
	const bool isCharPrevBlank = (charPrev == ' ' || charPrev == '\t' || charPrev == '\n' || charPrev == '\r' || charPrev == '\0');
	const bool isCharNextBlank = (charNext == ' ' || charNext == '\t' || charNext == '\n' || charNext == '\r' || caretPos == ex(SCI_GETLENGTH));
	const bool isCharNextCloseSymbol = (charNext == ')' || charNext == ']' || charNext == '}');
	const bool isInSandwich = (charPrev == '(' && charNext == ')') || (charPrev == '[' && charNext == ']') || (charPrev == '{' && charNext == '}');

	// user defined matched pairs first
	for (const auto& pair : p.matchedPairList())
	{
		if (pair.first == ch && isCharNextBlank)
		{
			const char userMatchedChar[2] = {pair.second, '\0'};
			exs(SCI_INSERTTEXT, caretPos, userMatchedChar);
			return;
		}
	}

	QByteArray matchedChars;
	auto quoteAllowed = [&]
	{
		return (isCharPrevBlank && isCharNextBlank) || isInSandwich || (charPrev == '(' && isCharNextBlank) || (isCharPrevBlank && charNext == ')') ||
		       (charPrev == '[' && isCharNextBlank) || (isCharPrevBlank && charNext == ']') || (charPrev == '{' && isCharNextBlank) ||
		       (isCharPrevBlank && charNext == '}');
	};
	switch (ch)
	{
		case '(':
		case '[':
		case '{':
		{
			const bool enabled = (ch == '(') ? p.autoPairParentheses : (ch == '[') ? p.autoPairBrackets : p.autoPairBraces;
			if (enabled && (isCharNextBlank || isCharNextCloseSymbol))
			{
				matchedChars = (ch == '(') ? ")" : (ch == '[') ? "]" : "}";
				removeInvalidMatchedChars({ch, caretPos - 1});
				_insertedMatchedChars.push_back({ch, caretPos - 1});
			}
			break;
		}
		case '"':
		case '\'':
		{
			const bool enabled = (ch == '"') ? p.autoPairDoubleQuotes : p.autoPairQuotes;
			if (!enabled)
				break;
			if (!_insertedMatchedChars.empty())
			{
				const sptr_t pos = searchMatchedChar(ch, ch, caretPos);
				if (pos != -1)
				{
					ex(SCI_DELETERANGE, pos, 1);
					ex(SCI_GOTOPOS, pos);
					return;
				}
			}
			if (quoteAllowed())
			{
				matchedChars = QByteArray(1, ch);
				removeInvalidMatchedChars({ch, caretPos - 1});
				_insertedMatchedChars.push_back({ch, caretPos - 1});
			}
			break;
		}
		case '>':
			if (p.autoPairHtmlXmlTag && (_lang == L_HTML || _lang == L_XML))
				matchedChars = closeTag(caretPos, _lang == L_HTML);
			break;
		case ')':
		case ']':
		case '}':
		{
			if (_insertedMatchedChars.empty())
				break;
			const bool enabled = (ch == ')') ? p.autoPairParentheses : (ch == ']') ? p.autoPairBrackets : p.autoPairBraces;
			if (!enabled)
				return;
			const char startChar = (ch == ')') ? '(' : (ch == ']') ? '[' : '{';
			const sptr_t pos = searchMatchedChar(startChar, ch, caretPos);
			if (pos != -1)
			{
				ex(SCI_DELETERANGE, pos, 1);
				ex(SCI_GOTOPOS, pos);
			}
			return;
		}
		default:
			if (!_insertedMatchedChars.empty())
				removeInvalidMatchedChars({ch, caretPos - 1});
			break;
	}
	if (!matchedChars.isEmpty())
		exs(SCI_INSERTTEXT, caretPos, matchedChars.constData());
}

void EditView::foldAll(bool fold)
{
	ex(SCI_COLOURISE, 0, -1);
	ex(SCI_FOLDALL, fold ? SC_FOLDACTION_CONTRACT : SC_FOLDACTION_EXPAND);
	emit folded();
}

void EditView::foldLevel(int level, bool fold)
{
	ex(SCI_COLOURISE, 0, -1);
	const sptr_t maxLine = ex(SCI_GETLINECOUNT);
	for (sptr_t line = 0; line < maxLine; ++line)
	{
		const sptr_t lev = ex(SCI_GETFOLDLEVEL, line);
		if (!(lev & SC_FOLDLEVELHEADERFLAG))
			continue;
		const int levNum = static_cast<int>((lev & SC_FOLDLEVELNUMBERMASK) - SC_FOLDLEVELBASE);
		if (levNum != level)
			continue;
		const bool expanded = ex(SCI_GETFOLDEXPANDED, line) != 0;
		if (fold == expanded)
			ex(SCI_TOGGLEFOLD, line);
	}
	emit folded();
}

void EditView::foldCurrent(bool fold)
{
	sptr_t line = ex(SCI_LINEFROMPOSITION, ex(SCI_GETCURRENTPOS));
	if (!(ex(SCI_GETFOLDLEVEL, line) & SC_FOLDLEVELHEADERFLAG))
		line = ex(SCI_GETFOLDPARENT, line);
	if (line < 0)
		return;
	ex(SCI_FOLDLINE, line, fold ? SC_FOLDACTION_CONTRACT : SC_FOLDACTION_EXPAND);
	emit folded();
}

void EditView::marginClick(sptr_t position, int modifiers, int margin)
{
	const sptr_t lineClick = ex(SCI_LINEFROMPOSITION, position);
	if (margin == MARGIN_FOLDER)
	{
		const sptr_t levelClick = ex(SCI_GETFOLDLEVEL, lineClick);
		if (levelClick & SC_FOLDLEVELHEADERFLAG)
		{
			if (modifiers & SCMOD_SHIFT)
				ex(SCI_FOLDCHILDREN, lineClick, SC_FOLDACTION_EXPAND);
			else if (modifiers & SCMOD_CTRL)
				ex(SCI_FOLDCHILDREN, lineClick, ex(SCI_GETFOLDEXPANDED, lineClick) ? SC_FOLDACTION_CONTRACT : SC_FOLDACTION_EXPAND);
			else
				ex(SCI_TOGGLEFOLD, lineClick);
			emit folded();
		}
	}
	else if (margin == MARGIN_SYMBOL && !modifiers)
	{
		// NppNotification.cpp SCN_MARGINCLICK: a hidden lines marker shows the lines, otherwise bookmark
		if (hidelineMarkerClicked(lineClick))
			return;
		if (ex(SCI_MARKERGET, lineClick) & (1 << MARK_BOOKMARK))
			ex(SCI_MARKERDELETE, lineClick, MARK_BOOKMARK);
		else
			ex(SCI_MARKERADD, lineClick, MARK_BOOKMARK);
	}
}

void EditView::hideLines()
{
	// ScintillaEditView::hideLines()
	sptr_t startLine = ex(SCI_LINEFROMPOSITION, ex(SCI_GETSELECTIONSTART));
	sptr_t endLine = ex(SCI_LINEFROMPOSITION, ex(SCI_GETSELECTIONEND));

	// the very first and very last lines can not be hidden
	const sptr_t nbLines = ex(SCI_GETLINECOUNT);
	if (nbLines < 3)
		return;
	if (!startLine)
		++startLine;
	if (endLine == nbLines - 1)
		--endLine;
	if (startLine > endLine)
		return;

	int scope = 0;
	bool recentMarkerWasOpen = false;
	auto removeMarker = [this, &scope, &recentMarkerWasOpen](sptr_t line, int markerMask)
	{
		const sptr_t state = ex(SCI_MARKERGET, line) & markerMask;
		if (state & (1 << MARK_HIDELINESEND))
		{
			ex(SCI_MARKERDELETE, line, MARK_HIDELINESEND);
			recentMarkerWasOpen = false;
			--scope;
		}
		if (state & (1 << MARK_HIDELINESBEGIN))
		{
			ex(SCI_MARKERDELETE, line, MARK_HIDELINESBEGIN);
			recentMarkerWasOpen = true;
			++scope;
		}
	};

	sptr_t startMarker = startLine - 1;
	sptr_t endMarker = endLine + 1;

	// previous markers are removed in the selected region
	removeMarker(startMarker, 1 << MARK_HIDELINESBEGIN);
	for (sptr_t i = startLine; i <= endLine; ++i)
		removeMarker(i, (1 << MARK_HIDELINESBEGIN) | (1 << MARK_HIDELINESEND));
	removeMarker(endMarker, 1 << MARK_HIDELINESEND);

	// lines next to another hidden section: one hidden section
	if (scope == 0 && recentMarkerWasOpen)
	{
		while (scope == 0 && startMarker >= 0)
			removeMarker(--startMarker, 1 << MARK_HIDELINESBEGIN);
		while (scope != 0 && endMarker < nbLines)
			removeMarker(++endMarker, 1 << MARK_HIDELINESEND);
	}
	else
	{
		while (scope < 0 && startMarker >= 0)
			removeMarker(--startMarker, 1 << MARK_HIDELINESBEGIN);
		while (scope > 0 && endMarker < nbLines)
			removeMarker(++endMarker, 1 << MARK_HIDELINESEND);
	}

	ex(SCI_MARKERADD, startMarker, MARK_HIDELINESBEGIN);
	ex(SCI_MARKERADD, endMarker, MARK_HIDELINESEND);
	emit hideLinesChanged(true, startMarker);
}

bool EditView::hidelineMarkerClicked(sptr_t lineNumber)
{
	// ScintillaEditView::hidelineMarkerClicked()
	sptr_t state = ex(SCI_MARKERGET, lineNumber);
	bool openPresent = (state & (1 << MARK_HIDELINESBEGIN)) != 0;
	bool closePresent = (state & (1 << MARK_HIDELINESEND)) != 0;
	if (!openPresent && !closePresent)
		return false;
	if (openPresent)
	{
		// two markers on the line: the lower section opens
		emit hideLinesChanged(false, lineNumber);
		return true;
	}
	openPresent = false;
	sptr_t i = lineNumber - 1;
	for (; i >= 0 && !openPresent; --i)
	{
		state = ex(SCI_MARKERGET, i);
		openPresent = (state & (1 << MARK_HIDELINESBEGIN)) != 0;
	}
	if (openPresent)
		emit hideLinesChanged(false, i + 1);
	else
		ex(SCI_MARKERDELETE, lineNumber, MARK_HIDELINESEND); // a close marker without open marker
	return true;
}

void EditView::hideMarkedLines(sptr_t searchStart, bool toEndOfDoc)
{
	// ScintillaEditView::hideMarkedLines()
	const sptr_t maxLines = ex(SCI_GETLINECOUNT);
	sptr_t startHiding = searchStart;
	bool isInSection = false;
	for (sptr_t i = searchStart; i < maxLines; ++i)
	{
		const sptr_t state = ex(SCI_MARKERGET, i);
		if (state & (1 << MARK_HIDELINESEND))
		{
			if (isInSection)
			{
				ex(SCI_HIDELINES, startHiding, i - 1);
				if (!toEndOfDoc)
					return;
			}
			isInSection = false;
		}
		if (state & (1 << MARK_HIDELINESBEGIN))
		{
			isInSection = true;
			startHiding = i + 1;
		}
	}
}

void EditView::showHiddenLines(sptr_t searchStart, bool toEndOfDoc, bool doDelete)
{
	// ScintillaEditView::showHiddenLines()
	const sptr_t maxLines = ex(SCI_GETLINECOUNT);
	sptr_t startShowing = searchStart;
	bool isInSection = false;
	for (sptr_t i = searchStart; i < maxLines; ++i)
	{
		const sptr_t state = ex(SCI_MARKERGET, i);
		if ((state & (1 << MARK_HIDELINESBEGIN)) && !isInSection)
		{
			isInSection = true;
			if (doDelete)
				ex(SCI_MARKERDELETE, i, MARK_HIDELINESBEGIN);
			else
				startShowing = i + 1;
		}
		else if (state & (1 << MARK_HIDELINESEND))
		{
			if (doDelete)
			{
				ex(SCI_MARKERDELETE, i, MARK_HIDELINESEND);
				if (!toEndOfDoc)
					return;
				isInSection = false;
			}
			else if (isInSection)
			{
				if (startShowing >= i)
				{
					// passed the close marker because of a folded section
					if (!toEndOfDoc)
						return;
					isInSection = false;
					continue;
				}
				ex(SCI_SHOWLINES, startShowing, i - 1);
				if (!toEndOfDoc)
					return;
				isInSection = false;
			}
		}
		const sptr_t levelLine = ex(SCI_GETFOLDLEVEL, i);
		if ((levelLine & SC_FOLDLEVELHEADERFLAG) && isInSection && ex(SCI_GETFOLDEXPANDED, i))
			ex(SCI_SHOWLINES, startShowing, i); // a folded section stays hidden
	}
}

void EditView::restoreHiddenLines()
{
	// ScintillaEditView::restoreHiddenLines(): the hidden sections of a document shown again in this view
	sptr_t line = 0;
	while (line != -1)
	{
		sptr_t lineBegin = ex(SCI_MARKERNEXT, line, 1 << MARK_HIDELINESBEGIN);
		sptr_t lineEnd = ex(SCI_MARKERNEXT, line, 1 << MARK_HIDELINESEND);
		if (lineBegin != -1 && lineEnd != -1 && lineBegin != lineEnd)
		{
			ex(SCI_HIDELINES, lineBegin + 1, lineEnd - 1);
			// the end marker and the next begin marker may be on the same line
			lineBegin = ex(SCI_MARKERNEXT, lineEnd, 1 << MARK_HIDELINESBEGIN);
			lineEnd = ex(SCI_MARKERNEXT, lineEnd, 1 << MARK_HIDELINESEND);
			line = lineEnd + ((lineBegin == lineEnd) ? 0 : 1);
		}
		else if (lineBegin != -1 && lineEnd != -1 && lineBegin == lineEnd)
		{
			lineEnd = ex(SCI_MARKERNEXT, line + 1, 1 << MARK_HIDELINESEND);
			if (lineEnd != -1)
				ex(SCI_HIDELINES, lineBegin + 1, lineEnd - 1);
			line = lineEnd;
		}
		else
		{
			line = -1;
		}
	}
}

static const QRegularExpression& urlRegex()
{
	// Notepad_plus.cpp isUrlSchemeSupported(): built-in schemes and "URI customized schemes"
	static QString builtFor;
	static QRegularExpression re;
	const QString schemes = Npp::prefs().uriSchemes;
	if (builtFor != schemes || !re.isValid() || re.pattern().isEmpty())
	{
		QStringList alternatives;
		for (const QString& sc : (QStringLiteral("ftp:// http:// https:// mailto: file:// ") + schemes).split(QLatin1Char(' '), Qt::SkipEmptyParts))
			alternatives << QRegularExpression::escape(sc);
		re = QRegularExpression(QStringLiteral(R"((?<![\w])(?:%1)[^\s<>"'`(){}\[\]]*[^\s<>"'`(){}\[\].,;:!?])").arg(alternatives.join(QLatin1Char('|'))),
		                        QRegularExpression::CaseInsensitiveOption);
		builtFor = schemes;
	}
	return re;
}

void EditView::highlightUrls()
{
	if (!_isMain)
		return;
	ex(SCI_SETINDICATORCURRENT, URL_INDIC);
	const sptr_t firstLine = ex(SCI_DOCLINEFROMVISIBLE, ex(SCI_GETFIRSTVISIBLELINE));
	const sptr_t lastLine = ex(SCI_DOCLINEFROMVISIBLE, ex(SCI_GETFIRSTVISIBLELINE) + ex(SCI_LINESONSCREEN) + 1);
	const sptr_t start = ex(SCI_POSITIONFROMLINE, firstLine);
	const sptr_t end = ex(SCI_GETLINEENDPOSITION, std::min(lastLine, ex(SCI_GETLINECOUNT) - 1));
	ex(SCI_INDICATORCLEARRANGE, start, end - start);
	if (!Npp::prefs().urlHighlight || !allowClickableLink() || end - start > 2 * 1024 * 1024)
		return;
	const QByteArray text = textRange(start, end);
	const QString s = QString::fromUtf8(text);
	auto it = urlRegex().globalMatch(s);
	while (it.hasNext())
	{
		const QRegularExpressionMatch m = it.next();
		const sptr_t bStart = start + s.left(m.capturedStart()).toUtf8().size();
		const sptr_t bLen = m.captured().toUtf8().size();
		ex(SCI_INDICATORFILLRANGE, bStart, bLen);
	}
}

QString EditView::urlAt(sptr_t pos) const
{
	if (!(ex(SCI_INDICATORALLONFOR, pos) & (1 << URL_INDIC)))
		return {};
	const sptr_t start = ex(SCI_INDICATORSTART, URL_INDIC, pos);
	const sptr_t end = ex(SCI_INDICATOREND, URL_INDIC, pos);
	return textRangeQ(start, end);
}

void EditView::selectBetweenDelimiters(sptr_t position)
{
	// NppNotification.cpp SCN_DOUBLECLICK with Ctrl: Preferences > Delimiter
	const NppPrefs& p = Npp::prefs();
	const QByteArray openB = p.delimiterOpen.toLatin1();
	const QByteArray closeB = p.delimiterClose.toLatin1();
	if (openB.isEmpty() || closeB.isEmpty())
		return;
	const char left = openB[0];
	const char right = closeB[0];
	const bool wholeDoc = p.delimiterSeveralLines;

	sptr_t click = (position != -1) ? position : ex(SCI_GETCURRENTPOS);
	QByteArray buf;
	sptr_t base = 0;
	if (wholeDoc)
	{
		buf = wholeText();
	}
	else
	{
		const sptr_t line = ex(SCI_LINEFROMPOSITION, ex(SCI_GETCURRENTPOS));
		base = ex(SCI_POSITIONFROMLINE, line);
		buf = textRange(base, ex(SCI_GETLINEENDPOSITION, line));
		click -= base;
	}
	if (click < 0 || click > buf.size())
		return;

	sptr_t leftmost = -1;
	sptr_t rightmost = -1;
	if (left == right)
	{
		for (sptr_t i = std::min<sptr_t>(click, buf.size() - 1); i >= 0; --i)
		{
			if (buf.at(i) == left && !(left == '"' && i > 0 && buf.at(i - 1) == '\\'))
			{
				leftmost = i;
				break;
			}
		}
		if (leftmost == -1)
			return;
		for (sptr_t i = click; i < buf.size(); ++i)
		{
			if (buf.at(i) == right && !(right == '"' && i > 0 && buf.at(i - 1) == '\\'))
			{
				rightmost = i;
				break;
			}
		}
	}
	else
	{
		std::stack<sptr_t> lefts;
		for (sptr_t i = 0; i < buf.size(); ++i)
		{
			if (buf.at(i) == left)
				lefts.push(i);
			else if (buf.at(i) == right && !lefts.empty())
			{
				const sptr_t matching = lefts.top();
				lefts.pop();
				if (matching <= click && i >= click && (leftmost == -1 || matching > leftmost))
				{
					leftmost = matching;
					rightmost = i;
				}
			}
		}
	}
	if (leftmost != -1 && rightmost != -1)
	{
		ex(SCI_SETCURRENTPOS, base + rightmost);
		ex(SCI_SETANCHOR, base + leftmost + 1);
	}
}

void EditView::onNotify(Scintilla::NotificationData* scn)
{
	const int code = static_cast<int>(scn->nmhdr.code);
	switch (code)
	{
		case SCN_MARGINCLICK:
			marginClick(scn->position, static_cast<int>(scn->modifiers), scn->margin);
			break;
		case SCN_UPDATEUI:
		{
			const int updated = static_cast<int>(scn->updated);
			if (_isMain && (updated & (SC_UPDATE_SELECTION | SC_UPDATE_CONTENT)))
			{
				doBraceMatch();
				doTagMatch();
			}
			if (_isMain && _activeView && (updated & (SC_UPDATE_SELECTION | SC_UPDATE_V_SCROLL | SC_UPDATE_CONTENT)))
				doSmartHighlight();
			if (_isMain && (updated & (SC_UPDATE_V_SCROLL | SC_UPDATE_CONTENT)))
				highlightUrls();
			break;
		}
		case SCN_MODIFIED:
			if (scn->linesAdded != 0)
				updateLineNumberWidth();
			break;
		case SCN_PAINTED:
			if (_isMain && Npp::prefs().lineNumberDynamicWidth)
				updateLineNumberWidth();
			break;
		case SCN_ZOOM:
			updateLineNumberWidth();
			break;
		case SCN_CHARADDED:
			if (_isMain)
			{
				maintainIndentation(scn->ch);
				insertMatchedChars(scn->ch);
			}
			break;
		case SCN_DOUBLECLICK:
			if (_isMain && static_cast<int>(scn->modifiers) == SCMOD_CTRL)
				selectBetweenDelimiters(scn->position);
			break;
		default:
			break;
	}
}

void EditView::contextMenuEvent(QContextMenuEvent* event)
{
	// Like Notepad++ (ScintillaWin WM_RBUTTONDOWN): a right click outside the selection
	// moves the caret there, unless "Keep selection when right-click outside of selection"
	if (event->reason() == QContextMenuEvent::Mouse && !Npp::prefs().rightClickKeepsSelection)
	{
		const QPoint p = event->pos();
		const sptr_t pos = ex(SCI_POSITIONFROMPOINTCLOSE, p.x(), p.y());
		const sptr_t selStart = ex(SCI_GETSELECTIONSTART);
		const sptr_t selEnd = ex(SCI_GETSELECTIONEND);
		if (pos >= 0 && (pos < selStart || pos > selEnd) && ex(SCI_GETSELECTIONS) == 1)
			ex(SCI_GOTOPOS, pos);
	}
	emit contextMenuWanted(event->globalPos());
	event->accept();
}

void EditView::keyPressEvent(QKeyEvent* event)
{
	const NppPrefs& p = Npp::prefs();
	if (_isMain && p.columnSel2MultiEdit)
	{
		// ScintillaEditView::scintillaNew_Proc WM_KEYDOWN: column selection to multi-editing
		const sptr_t selMode = ex(SCI_GETSELECTIONMODE);
		if (selMode == SC_SEL_RECTANGLE || selMode == SC_SEL_THIN)
		{
			switch (event->key())
			{
				case Qt::Key_Left:
				case Qt::Key_Right:
				case Qt::Key_Up:
				case Qt::Key_Down:
				case Qt::Key_Home:
				case Qt::Key_End:
				case Qt::Key_Return:
				case Qt::Key_Enter:
				case Qt::Key_Backspace:
					ex(SCI_SETSELECTIONMODE, SC_SEL_STREAM);
					ex(SCI_SETSELECTIONMODE, SC_SEL_STREAM);
					break;
				case Qt::Key_Escape:
				{
					const sptr_t caret = ex(SCI_GETSELECTIONNCARET, ex(SCI_GETMAINSELECTION));
					ex(SCI_SETSELECTION, caret, caret);
					ex(SCI_SETSELECTIONMODE, SC_SEL_STREAM);
					break;
				}
				default:
					break;
			}
		}
	}

	ScintillaEditBase::keyPressEvent(event);

	// The Qt platform layer never types control characters. Notepad++ lets them in
	// when "Prevent control character (C0 code) typing into document" is unchecked.
	if (_isMain && !p.npcNoInputC0 && !event->isAccepted())
	{
		const QString text = event->text();
		if (text.size() == 1)
		{
			const char16_t c = text.at(0).unicode();
			const bool handledKey = (c == '\t' || c == '\r' || c == '\n' || c == '\b' || c == 0x1B);
			if ((c < 32 || c == 127) && !handledKey && !ex(SCI_GETREADONLY))
			{
				const char buf[2] = {static_cast<char>(c), '\0'};
				ex(SCI_REPLACESEL, 0, reinterpret_cast<sptr_t>(buf));
				event->accept();
			}
		}
	}
}
