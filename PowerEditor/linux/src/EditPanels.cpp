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

#include "EditPanels.h"

#include <algorithm>

#include <QApplication>
#include <QClipboard>
#include <QHeaderView>
#include <QKeyEvent>
#include <QListWidget>
#include <QMessageBox>
#include <QMimeData>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "EditView.h"
#include "FileIO.h"
#include "MainWindow.h"

namespace
{
	// AsciiListView::getHtmlName()
	const char* htmlName(int value)
	{
		switch (value)
		{
			case 33: return "&excl;";
			case 34: return "&quot;";
			case 35: return "&num;";
			case 36: return "&dollar;";
			case 37: return "&percnt;";
			case 38: return "&amp;";
			case 39: return "&apos;";
			case 40: return "&lpar;";
			case 41: return "&rpar;";
			case 42: return "&ast;";
			case 43: return "&plus;";
			case 44: return "&comma;";
			case 45: return "&minus;";
			case 46: return "&period;";
			case 47: return "&sol;";
			case 58: return "&colon;";
			case 59: return "&semi;";
			case 60: return "&lt;";
			case 61: return "&equals;";
			case 62: return "&gt;";
			case 63: return "&quest;";
			case 64: return "&commat;";
			case 91: return "&lbrack;";
			case 92: return "&bsol;";
			case 93: return "&rbrack;";
			case 94: return "&Hat;";
			case 95: return "&lowbar;";
			case 96: return "&grave;";
			case 123: return "&lbrace;";
			case 124: return "&vert;";
			case 125: return "&rbrace;";
			case 126: return "";
			case 128: return "&euro;";
			case 130: return "&sbquo;";
			case 131: return "&fnof;";
			case 132: return "&bdquo;";
			case 133: return "&hellip;";
			case 134: return "&dagger;";
			case 135: return "&Dagger;";
			case 136: return "&circ;";
			case 137: return "&permil;";
			case 138: return "&Scaron;";
			case 139: return "&lsaquo;";
			case 140: return "&OElig;";
			case 142: return "&Zcaron;";
			case 145: return "&lsquo;";
			case 146: return "&rsquo;";
			case 147: return "&ldquo;";
			case 148: return "&rdquo;";
			case 149: return "&bull;";
			case 150: return "&ndash;";
			case 151: return "&mdash;";
			case 152: return "&tilde;";
			case 153: return "&trade;";
			case 154: return "&scaron;";
			case 155: return "&rsaquo;";
			case 156: return "&oelig;";
			case 158: return "&zcaron;";
			case 159: return "&Yuml;";
			case 160: return "&nbsp;";
			case 161: return "&iexcl;";
			case 162: return "&cent;";
			case 163: return "&pound;";
			case 164: return "&curren;";
			case 165: return "&yen;";
			case 166: return "&brvbar;";
			case 167: return "&sect;";
			case 168: return "&uml;";
			case 169: return "&copy;";
			case 170: return "&ordf;";
			case 171: return "&laquo;";
			case 172: return "&not;";
			case 173: return "&shy;";
			case 174: return "&reg;";
			case 175: return "&macr;";
			case 176: return "&deg;";
			case 177: return "&plusmn;";
			case 178: return "&sup2;";
			case 179: return "&sup3;";
			case 180: return "&acute;";
			case 181: return "&micro;";
			case 182: return "&para;";
			case 183: return "&middot;";
			case 184: return "&cedil;";
			case 185: return "&sup1;";
			case 186: return "&ordm;";
			case 187: return "&raquo;";
			case 188: return "&frac14;";
			case 189: return "&frac12;";
			case 190: return "&frac34;";
			case 191: return "&iquest;";
			case 192: return "&Agrave;";
			case 193: return "&Aacute;";
			case 194: return "&Acirc;";
			case 195: return "&Atilde;";
			case 196: return "&Auml;";
			case 197: return "&Aring;";
			case 198: return "&AElig;";
			case 199: return "&Ccedil;";
			case 200: return "&Egrave;";
			case 201: return "&Eacute;";
			case 202: return "&Ecirc;";
			case 203: return "&Euml;";
			case 204: return "&Igrave;";
			case 205: return "&Iacute;";
			case 206: return "&Icirc;";
			case 207: return "&Iuml;";
			case 208: return "&ETH;";
			case 209: return "&Ntilde;";
			case 210: return "&Ograve;";
			case 211: return "&Oacute;";
			case 212: return "&Ocirc;";
			case 213: return "&Otilde;";
			case 214: return "&Ouml;";
			case 215: return "&times;";
			case 216: return "&Oslash;";
			case 217: return "&Ugrave;";
			case 218: return "&Uacute;";
			case 219: return "&Ucirc;";
			case 220: return "&Uuml;";
			case 221: return "&Yacute;";
			case 222: return "&THORN;";
			case 223: return "&szlig;";
			case 224: return "&agrave;";
			case 225: return "&aacute;";
			case 226: return "&acirc;";
			case 227: return "&atilde;";
			case 228: return "&auml;";
			case 229: return "&aring;";
			case 230: return "&aelig;";
			case 231: return "&ccedil;";
			case 232: return "&egrave;";
			case 233: return "&eacute;";
			case 234: return "&ecirc;";
			case 235: return "&euml;";
			case 236: return "&igrave;";
			case 237: return "&iacute;";
			case 238: return "&icirc;";
			case 239: return "&iuml;";
			case 240: return "&eth;";
			case 241: return "&ntilde;";
			case 242: return "&ograve;";
			case 243: return "&oacute;";
			case 244: return "&ocirc;";
			case 245: return "&otilde;";
			case 246: return "&ouml;";
			case 247: return "&divide;";
			case 248: return "&oslash;";
			case 249: return "&ugrave;";
			case 250: return "&uacute;";
			case 251: return "&ucirc;";
			case 252: return "&uuml;";
			case 253: return "&yacute;";
			case 254: return "&thorn;";
			case 255: return "&yuml;";
			default: return "";
		}
	}

	// AsciiListView::getHtmlNumber()
	int htmlNumber(int value)
	{
		switch (value)
		{
			case 45: return 8722;
			case 128: return 8364;
			case 130: return 8218;
			case 131: return 402;
			case 132: return 8222;
			case 133: return 8230;
			case 134: return 8224;
			case 135: return 8225;
			case 136: return 710;
			case 137: return 8240;
			case 138: return 352;
			case 139: return 8249;
			case 140: return 338;
			case 142: return 381;
			case 145: return 8216;
			case 146: return 8217;
			case 147: return 8220;
			case 148: return 8221;
			case 149: return 8226;
			case 150: return 8211;
			case 151: return 8212;
			case 152: return 732;
			case 153: return 8482;
			case 154: return 353;
			case 155: return 8250;
			case 156: return 339;
			case 158: return 382;
			case 159: return 376;
			default: return -1;
		}
	}

	// AsciiListView::getAscii(): names of the control characters
	const char* controlName(int value)
	{
		switch (value)
		{
			case 0: return "NULL";
			case 1: return "SOH";
			case 2: return "STX";
			case 3: return "ETX";
			case 4: return "EOT";
			case 5: return "ENQ";
			case 6: return "ACK";
			case 7: return "BEL";
			case 8: return "BS";
			case 9: return "TAB";
			case 10: return "LF";
			case 11: return "VT";
			case 12: return "FF";
			case 13: return "CR";
			case 14: return "SO";
			case 15: return "SI";
			case 16: return "DLE";
			case 17: return "DC1";
			case 18: return "DC2";
			case 19: return "DC3";
			case 20: return "DC4";
			case 21: return "NAK";
			case 22: return "SYN";
			case 23: return "ETB";
			case 24: return "CAN";
			case 25: return "EM";
			case 26: return "SUB";
			case 27: return "ESC";
			case 28: return "FS";
			case 29: return "GS";
			case 30: return "RS";
			case 31: return "US";
			case 32: return "Space";
			case 127: return "DEL";
			default: return nullptr;
		}
	}

	// code page of the characters: Buffer::getEncoding(), -1 (Unicode documents) is the ANSI code page
	int documentCodepage(const Buffer* b)
	{
		if (!b || b->unicode != UniMode::uni8Bit)
			return CP_ANSI;
		return b->codepage;
	}

	QByteArray charOf(int value, int codepage)
	{
		if (value < 128)
			return QByteArray(1, static_cast<char>(value));
		return FileIO::toUtf8(QByteArray(1, static_cast<char>(value)), codepage, nullptr);
	}
}

// ---------------------------------------------------------------------------
// Character Panel
// ---------------------------------------------------------------------------
CharacterPanel::CharacterPanel(MainWindow* mw, QWidget* parent)
	: QWidget(parent), _mw(mw)
{
	setObjectName(QStringLiteral("characterPanel"));
	auto* l = new QVBoxLayout(this);
	l->setContentsMargins(0, 0, 0, 0);
	_list = new QTreeWidget(this);
	_list->setObjectName(QStringLiteral("asciiList"));
	_list->setRootIsDecorated(false);
	_list->setUniformRowHeights(true);
	_list->setAllColumnsShowFocus(true);
	_list->setHeaderLabels({QStringLiteral("Value"), QStringLiteral("Hex"), QStringLiteral("Character"), QStringLiteral("HTML Name"),
	                        QStringLiteral("HTML Decimal"), QStringLiteral("HTML Hexadecimal")});
	// kDefaultColBaseWidths: 6, 6, 10, 13, 14, 17 average character widths
	const int avg = fontMetrics().averageCharWidth();
	const int widths[] = {6, 6, 10, 13, 14, 17};
	for (int c = 0; c < 6; ++c)
		_list->setColumnWidth(c, avg * widths[c]);
	l->addWidget(_list);
	_list->installEventFilter(this);
	connect(_list, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem* item, int column)
	{
		insertCell(_list->indexOfTopLevelItem(item), column);
	});
	switchEncoding();
}

void CharacterPanel::switchEncoding()
{
	const int cp = documentCodepage(_mw->currentBuffer());
	if (cp != _codepage)
		setValues(cp);
}

void CharacterPanel::setValues(int codepage)
{
	// AsciiListView::setValues()
	_codepage = codepage;
	const int current = _list->indexOfTopLevelItem(_list->currentItem());
	_list->clear();
	const bool ansi = (codepage == CP_ANSI || codepage == 1252);
	QList<QTreeWidgetItem*> items;
	for (int i = 0; i < 256; ++i)
	{
		QString ch;
		if (const char* name = controlName(i))
			ch = QString::fromLatin1(name);
		else
			ch = QString::fromUtf8(charOf(i, codepage));
		QString numStr, hexStr, nameStr;
		if (ansi)
		{
			int n = -1;
			if ((i >= 32 && i <= 126 && i != 45) || (i >= 160 && i <= 255))
				n = i;
			else
				n = htmlNumber(i);
			if (n > -1)
			{
				numStr = QStringLiteral("&#%1;").arg(n);
				hexStr = QStringLiteral("&#x%1;").arg(n, 0, 16);
			}
			nameStr = QString::fromLatin1(htmlName(i));
		}
		items << new QTreeWidgetItem(QStringList{QString::number(i), QStringLiteral("%1").arg(i, 2, 16, QLatin1Char('0')).toUpper(), ch, nameStr,
		                                         numStr, hexStr});
	}
	_list->addTopLevelItems(items);
	if (current >= 0)
		_list->setCurrentItem(_list->topLevelItem(current));
}

void CharacterPanel::insertBytes(const QByteArray& utf8)
{
	EditView* e = _mw->editView();
	if (!e || utf8.isEmpty())
		return;
	e->ex(SCI_BEGINUNDOACTION);
	e->exs(SCI_REPLACESEL, 0, "");
	e->exs(SCI_ADDTEXT, static_cast<uptr_t>(utf8.size()), utf8.constData());
	e->ex(SCI_ENDUNDOACTION);
	e->setFocus();
}

void CharacterPanel::insertCell(int row, int column)
{
	// AnsiCharPanel NM_DBLCLK: insertChar() for the Character column, insertString() for the others
	if (row < 0 || row > 255)
		return;
	if (column != 2)
	{
		const QTreeWidgetItem* item = _list->topLevelItem(row);
		if (item && !item->text(column).isEmpty())
			insertBytes(item->text(column).toUtf8());
		return;
	}
	insertBytes(charOf(row, _codepage));
}

bool CharacterPanel::eventFilter(QObject* obj, QEvent* event)
{
	// LVN_KEYDOWN VK_RETURN: insert the character
	if (obj == _list && event->type() == QEvent::KeyPress)
	{
		const int key = static_cast<QKeyEvent*>(event)->key();
		if (key == Qt::Key_Return || key == Qt::Key_Enter)
		{
			insertCell(_list->indexOfTopLevelItem(_list->currentItem()), 2);
			return true;
		}
	}
	return QWidget::eventFilter(obj, event);
}

// ---------------------------------------------------------------------------
// Clipboard History
// ---------------------------------------------------------------------------
ClipboardHistoryPanel::ClipboardHistoryPanel(MainWindow* mw, QWidget* parent)
	: QWidget(parent), _mw(mw)
{
	setObjectName(QStringLiteral("clipboardHistoryPanel"));
	auto* l = new QVBoxLayout(this);
	l->setContentsMargins(0, 0, 0, 0);
	_list = new QListWidget(this);
	_list->setObjectName(QStringLiteral("clipboardHistoryList"));
	_list->setUniformItemSizes(true);
	_list->setTextElideMode(Qt::ElideRight);
	l->addWidget(_list);
	connect(_list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem* item) { insertEntry(_list->row(item)); });
	connect(_list, &QListWidget::itemActivated, this, [this](QListWidgetItem* item)
	{
		if (QApplication::mouseButtons() == Qt::NoButton)
			insertEntry(_list->row(item)); // Enter
	});
	// SetClipboardViewer(): watched from the first opening of the panel
	connect(QApplication::clipboard(), &QClipboard::dataChanged, this, &ClipboardHistoryPanel::onClipboardChanged);
	onClipboardChanged();
}

void ClipboardHistoryPanel::onClipboardChanged()
{
	// ClipboardHistoryPanel::getClipboadData()
	const QMimeData* mime = QApplication::clipboard()->mimeData();
	if (!mime)
		return;
	Entry entry;
	if (mime->hasFormat(QString::fromLatin1(kNppBinaryMime)))
	{
		entry.data = mime->data(QString::fromLatin1(kNppBinaryMime));
		entry.binary = true;
	}
	else if (mime->hasText())
	{
		entry.data = mime->text().toUtf8();
	}
	if (!entry.data.isEmpty())
		addEntry(entry);
}

QString ClipboardHistoryPanel::displayString(const Entry& entry)
{
	// getDisplayStr(): MAX_PATH - 3 characters, then "..."
	constexpr int maxLen = 257;
	QString s = entry.binary ? QString::fromLatin1(entry.data.left(maxLen)) : QString::fromUtf8(entry.data);
	const bool truncated = entry.binary ? entry.data.size() > maxLen : s.size() > maxLen - 1;
	if (!entry.binary && truncated)
	{
		int len = maxLen - 1;
		if (s.at(len - 1).isHighSurrogate())
			--len;
		s.truncate(len);
	}
	// the list shows one line per entry
	for (QChar& c : s)
	{
		if (c == QLatin1Char('\r') || c == QLatin1Char('\n') || c == QLatin1Char('\t') || c.unicode() == 0)
			c = QLatin1Char(' ');
	}
	if (truncated)
		s += QStringLiteral("...");
	return s;
}

void ClipboardHistoryPanel::addEntry(const Entry& entry)
{
	auto same = [&](const Entry& e) { return e.binary == entry.binary && e.data == entry.data; };
	const auto it = std::find_if(_entries.begin(), _entries.end(), same);
	const int idx = (it == _entries.end()) ? -1 : static_cast<int>(it - _entries.begin());
	if (idx == 0)
		return;
	if (idx > 0)
	{
		// already in the history: moved to the top
		std::rotate(_entries.begin(), _entries.begin() + idx, _entries.begin() + idx + 1);
		delete _list->takeItem(idx);
	}
	else
	{
		_entries.insert(_entries.begin(), entry);
	}
	auto* item = new QListWidgetItem(displayString(entry));
	_list->insertItem(0, item);
}

void ClipboardHistoryPanel::insertEntry(int row)
{
	// ClipboardHistoryPanel::onDoubleClick()
	if (row < 0 || row >= static_cast<int>(_entries.size()))
		return;
	EditView* e = _mw->editView();
	if (!e)
		return;
	const Entry& entry = _entries[static_cast<size_t>(row)];
	e->ex(SCI_BEGINUNDOACTION);
	e->exs(SCI_REPLACESEL, 0, "");
	e->exs(SCI_ADDTEXT, static_cast<uptr_t>(entry.data.size()), entry.data.constData());
	e->ex(SCI_ENDUNDOACTION);
	e->setFocus();
}
