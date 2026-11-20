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

#include "FindReplace.h"

#include <algorithm>
#include <cstdio>

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDirIterator>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QProgressDialog>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QSettings>
#include <QTabBar>
#include <QVBoxLayout>

#include "EditView.h"
#include "FileIO.h"
#include "MainWindow.h"

namespace
{
	constexpr sptr_t FIND_INVALID_REGULAR_EXPRESSION = -2;
	constexpr int kHistoryMax = 10;
	constexpr qsizetype kResultLineMax = 1024;

	QSettings& findSettings()
	{
		static QSettings s(Npp::configDir() + QStringLiteral("/config.ini"), QSettings::IniFormat);
		return s;
	}

	sptr_t searchInTarget(EditView* v, const QByteArray& text, sptr_t from, sptr_t to)
	{
		v->ex(SCI_SETTARGETRANGE, from, to);
		return v->ex(SCI_SEARCHINTARGET, text.size(), reinterpret_cast<sptr_t>(text.constData()));
	}

	QString hitsString(int n)
	{
		return n == 1 ? QStringLiteral("1 hit") : QStringLiteral("%1 hits").arg(QLocale::system().toString(n));
	}

	QByteArray truncateUtf8(const QByteArray& s, qsizetype maxLen)
	{
		if (s.size() <= maxLen)
			return s;
		qsizetype n = maxLen;
		while (n > 0 && (static_cast<unsigned char>(s[n]) & 0xC0) == 0x80)
			--n;
		return s.left(n);
	}
}

// ---------------------------------------------------------------------------
// Searching helpers
// ---------------------------------------------------------------------------
namespace Searching
{
	static bool readBase(const QString& str, int start, int* value, int base, int size)
	{
		int temp = 0;
		*value = 0;
		const QChar max = QChar(static_cast<char16_t>('0' + base - 1));
		for (int i = 0; i < size; ++i)
		{
			QChar current = str[start + i];
			char16_t c = current.unicode();
			if (c >= 'A')
			{
				c &= 0xdf;
				c -= ('A' - '0' - 10);
			}
			else if (c > '9')
				return false;

			if (c >= '0' && c <= max.unicode())
			{
				temp *= base;
				temp += (c - '0');
			}
			else
				return false;
		}
		*value = temp;
		return true;
	}

	QString convertExtendedToString(const QString& query)
	{
		// Port of Searching::convertExtendedToString()
		QString result;
		result.reserve(query.size());
		const int length = static_cast<int>(query.size());
		int i = 0;
		while (i < length)
		{
			QChar current = query[i];
			const int charLeft = length - i - 1;
			if (current == QLatin1Char('\\') && charLeft > 0)
			{
				++i;
				current = query[i];
				const int left = length - i - 1;
				switch (current.unicode())
				{
					case 'r': result += QLatin1Char('\r'); break;
					case 'n': result += QLatin1Char('\n'); break;
					case '0': result += QChar(0); break;
					case 't': result += QLatin1Char('\t'); break;
					case '\\': result += QLatin1Char('\\'); break;
					case 'b':
					case 'd':
					case 'o':
					case 'x':
					case 'u':
					{
						int size = 0, base = 0;
						switch (current.unicode())
						{
							case 'b': size = 8; base = 2; break;
							case 'o': size = 3; base = 8; break;
							case 'd': size = 3; base = 10; break;
							case 'x': size = 2; base = 16; break;
							default: size = 4; base = 16; break;
						}
						int res = 0;
						if (left >= size && readBase(query, i + 1, &res, base, size))
						{
							result += QChar(static_cast<char16_t>(res));
							i += size;
							break;
						}
						[[fallthrough]];
					}
					default:
						result += QLatin1Char('\\');
						result += current;
						break;
				}
			}
			else
			{
				result += current;
			}
			++i;
		}
		return result;
	}

	QByteArray prepareText(const QString& text, int searchType)
	{
		if (searchType == FindExtended)
			return convertExtendedToString(text).toUtf8();
		return text.toUtf8();
	}

	int buildSearchFlags(const FindOptions& opt)
	{
		return (opt.wholeWord ? SCFIND_WHOLEWORD : 0) |
		       (opt.matchCase ? SCFIND_MATCHCASE : 0) |
		       (opt.searchType == FindRegex ? SCFIND_REGEXP | SCFIND_POSIX : 0) |
		       ((opt.searchType == FindRegex && opt.dotMatchesNewline) ? SCFIND_REGEXP_DOTMATCHESNL : 0);
	}
}

// ---------------------------------------------------------------------------
// SearchResultsPanel
// ---------------------------------------------------------------------------
SearchResultsPanel::SearchResultsPanel(MainWindow* mw, QWidget* parent)
	: QWidget(parent), _mw(mw)
{
	auto* layout = new QVBoxLayout(this);
	layout->setContentsMargins(0, 0, 0, 0);
	_view = new EditView(this, false);
	layout->addWidget(_view);
	_view->ex(SCI_SETMARGINWIDTHN, MARGIN_LINENUMBER, 0);
	_view->ex(SCI_SETMARGINWIDTHN, MARGIN_SYMBOL, 0);
	_view->ex(SCI_SETMARGINWIDTHN, MARGIN_CHANGEHISTORY, 0);
	_view->ex(SCI_SETCARETLINEVISIBLE, true);
	_view->ex(SCI_SETCARETLINEVISIBLEALWAYS, true);
	_view->ex(SCI_SETREADONLY, true);
	applyStyle();

	connect(_view, &ScintillaEditBase::doubleClick, this, [this](Scintilla::Position, Scintilla::Position line)
	{
		openLine(line);
	});
	connect(_view, &EditView::contextMenuWanted, this, &SearchResultsPanel::showContextMenu);
}

void SearchResultsPanel::applyStyle()
{
	_view->defineDocType(L_SEARCHRESULT);
	char addr[32];
	std::snprintf(addr, sizeof(addr), "%p", static_cast<void*>(&_markingsStruct));
	_view->ex(SCI_SETPROPERTY, reinterpret_cast<uptr_t>("@MarkingsStruct"), reinterpret_cast<sptr_t>(addr));
	_view->ex(SCI_SETPROPERTY, reinterpret_cast<uptr_t>("fold.compact"), reinterpret_cast<sptr_t>("0"));
	_view->ex(SCI_COLOURISE, 0, -1);
}

SearchResultsPanel::FileResult SearchResultsPanel::collect(EditView* v, const std::vector<SearchHit>& hits, const QString& fileName)
{
	FileResult fr;
	fr.fileName = fileName;
	fr.hitCount = static_cast<int>(hits.size());
	size_t i = 0;
	while (i < hits.size())
	{
		FoundLine fl;
		fl.line = hits[i].line;
		fl.start = hits[i].start;
		fl.end = hits[i].end;
		fl.text = truncateUtf8(v->lineText(fl.line), kResultLineMax);
		const sptr_t lineStart = v->ex(SCI_POSITIONFROMLINE, fl.line);
		// Searching > "Search Result window: show only one entry per found line"
		const bool onePerLine = Npp::prefs().finderOneEntryPerLine;
		while (i < hits.size() && hits[i].line == fl.line)
		{
			const sptr_t s = std::min<sptr_t>(hits[i].start - lineStart, fl.text.size());
			const sptr_t e = std::min<sptr_t>(hits[i].end - lineStart, fl.text.size());
			if (e > s)
				fl.segments.emplace_back(s, e);
			++i;
			if (!onePerLine)
				break;
		}
		fr.lines.push_back(std::move(fl));
	}
	return fr;
}

void SearchResultsPanel::addSearch(const QString& searchText, const std::vector<FileResult>& files, int filesSearched)
{
	Block block;
	int totalHits = 0;
	int filesWithHits = 0;
	for (const FileResult& f : files)
	{
		totalHits += f.hitCount;
		if (f.hitCount > 0)
			++filesWithHits;
	}

	QString oneLine = searchText;
	oneLine.remove(QLatin1Char('\r'));
	oneLine.remove(QLatin1Char('\n'));
	const QString header = QStringLiteral("Search \"%1\" (%2 in %3 of %4 searched)")
		.arg(oneLine, hitsString(totalHits),
		     filesWithHits == 1 ? QStringLiteral("1 file") : QStringLiteral("%1 files").arg(filesWithHits),
		     QString::number(filesSearched));
	block.text += header.toUtf8() + '\n';
	block.lines.push_back(LineInfo{0, QString(), 0, 0, 0});
	block.markings.emplace_back();

	for (const FileResult& f : files)
	{
		if (f.hitCount == 0)
			continue;
		block.text += ("  " + f.fileName + QStringLiteral(" (%1)").arg(hitsString(f.hitCount))).toUtf8() + '\n';
		block.lines.push_back(LineInfo{1, f.fileName, 0, 0, 0});
		block.markings.emplace_back();

		for (const FoundLine& fl : f.lines)
		{
			const QByteArray prefix = "\tLine " + QByteArray::number(static_cast<qlonglong>(fl.line + 1)) + ": ";
			SearchResultMarkingLine marking;
			for (const auto& seg : fl.segments)
				marking._segmentPostions.emplace_back(prefix.size() + seg.first, prefix.size() + seg.second);
			block.text += prefix + fl.text + '\n';
			block.lines.push_back(LineInfo{2, f.fileName, fl.line, fl.start, fl.end});
			block.markings.push_back(std::move(marking));
		}
	}

	_blocks.insert(_blocks.begin(), std::move(block));
	if (_blocks.size() > 30)
		_blocks.resize(30);
	_currentLine = 0;
	rebuild();
}

void SearchResultsPanel::rebuild()
{
	QByteArray all;
	_lines.clear();
	_markings.clear();
	for (const Block& b : _blocks)
	{
		all += b.text;
		_lines.insert(_lines.end(), b.lines.begin(), b.lines.end());
		_markings.insert(_markings.end(), b.markings.begin(), b.markings.end());
	}
	_markingsStruct._length = static_cast<intptr_t>(_markings.size());
	_markingsStruct._markings = _markings.empty() ? nullptr : _markings.data();

	_view->ex(SCI_SETREADONLY, false);
	_view->ex(SCI_CLEARALL);
	_view->ex(SCI_APPENDTEXT, all.size(), reinterpret_cast<sptr_t>(all.constData()));
	_view->ex(SCI_SETREADONLY, true);
	_view->ex(SCI_EMPTYUNDOBUFFER);
	applyStyle();

	// fold the older searches
	sptr_t lineNo = 0;
	bool first = true;
	for (const Block& b : _blocks)
	{
		if (!first)
			_view->ex(SCI_FOLDLINE, lineNo, SC_FOLDACTION_CONTRACT);
		first = false;
		lineNo += static_cast<sptr_t>(b.lines.size());
	}
	_view->ex(SCI_GOTOPOS, 0);
}

void SearchResultsPanel::clearAll()
{
	_blocks.clear();
	_currentLine = -1;
	rebuild();
}

void SearchResultsPanel::openLine(sptr_t line)
{
	if (line < 0 || line >= static_cast<sptr_t>(_lines.size()))
		return;
	const LineInfo& li = _lines[static_cast<size_t>(line)];
	if (li.kind != 2)
	{
		_view->ex(SCI_TOGGLEFOLD, line);
		return;
	}
	_currentLine = line;
	_view->ex(SCI_GOTOLINE, line);

	Buffer* buf = _mw->findBuffer(li.fileName);
	if (!buf)
	{
		for (Buffer* b : _mw->allBuffers())
		{
			if (b->isUntitled() && b->untitledName == li.fileName)
			{
				buf = b;
				break;
			}
		}
	}
	if (!buf)
		buf = _mw->openFile(li.fileName);
	if (buf)
		_mw->gotoPosition(buf, li.start, li.end);
}

void SearchResultsPanel::gotoNextResult(bool forward)
{
	if (_lines.empty())
		return;
	const sptr_t n = static_cast<sptr_t>(_lines.size());
	sptr_t line = _currentLine;
	for (sptr_t step = 0; step < n; ++step)
	{
		line = forward ? (line + 1) % n : (line - 1 + n) % n;
		if (_lines[static_cast<size_t>(line)].kind == 2)
		{
			_view->ex(SCI_ENSUREVISIBLE, line);
			openLine(line);
			return;
		}
	}
}

void SearchResultsPanel::showContextMenu(const QPoint& globalPos)
{
	QMenu menu(this);
	menu.addAction(QStringLiteral("Fold all"), this, [this] { _view->ex(SCI_FOLDALL, SC_FOLDACTION_CONTRACT); });
	menu.addAction(QStringLiteral("Unfold all"), this, [this] { _view->ex(SCI_FOLDALL, SC_FOLDACTION_EXPAND); });
	menu.addSeparator();
	menu.addAction(QStringLiteral("Copy"), this, [this] { _view->ex(SCI_COPY); });
	menu.addAction(QStringLiteral("Select all"), this, [this] { _view->ex(SCI_SELECTALL); });
	menu.addAction(QStringLiteral("Clear all"), this, [this] { clearAll(); });
	menu.addSeparator();
	menu.addAction(QStringLiteral("Open all"), this, [this]
	{
		QSet<QString> files;
		for (const LineInfo& li : _lines)
		{
			if (li.kind == 1 && !files.contains(li.fileName))
			{
				files.insert(li.fileName);
				if (QFileInfo::exists(li.fileName))
					_mw->openFile(li.fileName);
			}
		}
	});
	menu.exec(globalPos);
}

// ---------------------------------------------------------------------------
// FindReplaceDlg
// ---------------------------------------------------------------------------
FindReplaceDlg::FindReplaceDlg(MainWindow* mw)
	: QDialog(mw), _mw(mw)
{
	setWindowTitle(QStringLiteral("Find"));
	setModal(false);
	_searchView = new EditView(this, false);
	_searchView->hide();
	buildUi();
	loadHistory();
	updateForTab();
	// Smart Highlighting "Use Find dialog settings" follows these two options
	auto syncFindState = [this]
	{
		Npp::findState().matchCase = _matchCase->isChecked();
		Npp::findState().wholeWord = _wholeWord->isChecked() && _wholeWord->isEnabled();
	};
	connect(_matchCase, &QCheckBox::toggled, this, syncFindState);
	connect(_wholeWord, &QCheckBox::toggled, this, syncFindState);
	syncFindState();
}

void FindReplaceDlg::buildUi()
{
	auto* root = new QVBoxLayout(this);
	_tabBar = new QTabBar(this);
	_tabBar->addTab(QStringLiteral("Find"));
	_tabBar->addTab(QStringLiteral("Replace"));
	_tabBar->addTab(QStringLiteral("Find in Files"));
	_tabBar->addTab(QStringLiteral("Find in Projects"));
	_tabBar->addTab(QStringLiteral("Mark"));
	_tabBar->setExpanding(false);
	_tabBar->setDrawBase(true);
	root->addWidget(_tabBar);
	connect(_tabBar, &QTabBar::currentChanged, this, [this] { updateForTab(); });

	auto* body = new QHBoxLayout;
	root->addLayout(body, 1);

	auto* left = new QVBoxLayout;
	body->addLayout(left, 1);

	auto* fields = new QGridLayout;
	left->addLayout(fields);
	auto makeCombo = [this]
	{
		auto* c = new QComboBox(this);
		c->setEditable(true);
		c->setInsertPolicy(QComboBox::NoInsert);
		c->setMinimumWidth(320);
		c->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
		return c;
	};
	fields->addWidget(new QLabel(QStringLiteral("Find what :"), this), 0, 0, Qt::AlignRight);
	_findCombo = makeCombo();
	fields->addWidget(_findCombo, 0, 1, 1, 2);
	_replaceLabel = new QLabel(QStringLiteral("Replace with :"), this);
	fields->addWidget(_replaceLabel, 1, 0, Qt::AlignRight);
	_replaceCombo = makeCombo();
	fields->addWidget(_replaceCombo, 1, 1, 1, 2);
	_filtersLabel = new QLabel(QStringLiteral("Filters :"), this);
	fields->addWidget(_filtersLabel, 2, 0, Qt::AlignRight);
	_filtersCombo = makeCombo();
	fields->addWidget(_filtersCombo, 2, 1, 1, 2);
	_dirLabel = new QLabel(QStringLiteral("Directory :"), this);
	fields->addWidget(_dirLabel, 3, 0, Qt::AlignRight);
	_dirCombo = makeCombo();
	fields->addWidget(_dirCombo, 3, 1);
	_dirBrowse = new QPushButton(QStringLiteral("..."), this);
	_dirBrowse->setFixedWidth(32);
	fields->addWidget(_dirBrowse, 3, 2);
	connect(_dirBrowse, &QPushButton::clicked, this, [this]
	{
		const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("Select a folder to search from"), _dirCombo->currentText());
		if (!dir.isEmpty())
			_dirCombo->setCurrentText(dir);
	});

	// Find in Projects: the Project Panels to search in
	_projectRow = new QWidget(this);
	auto* projectLayout = new QHBoxLayout(_projectRow);
	projectLayout->setContentsMargins(0, 0, 0, 0);
	for (int i = 0; i < 3; ++i)
	{
		_projectChecks[i] = new QCheckBox(QStringLiteral("Project Panel %1").arg(i + 1), _projectRow);
		_projectChecks[i]->setObjectName(QStringLiteral("projectPanelCheck%1").arg(i + 1));
		projectLayout->addWidget(_projectChecks[i]);
	}
	projectLayout->addStretch();
	fields->addWidget(_projectRow, 4, 1, 1, 2);

	auto* optionsRow = new QHBoxLayout;
	left->addLayout(optionsRow);

	auto* opts = new QVBoxLayout;
	optionsRow->addLayout(opts);
	_backward = new QCheckBox(QStringLiteral("Backward direction"), this);
	_wholeWord = new QCheckBox(QStringLiteral("Match &whole word only"), this);
	_matchCase = new QCheckBox(QStringLiteral("Match &case"), this);
	_wrapAround = new QCheckBox(QStringLiteral("Wra&p around"), this);
	_wrapAround->setChecked(true);
	_inSelection = new QCheckBox(QStringLiteral("In select&ion"), this);
	_bookmarkLine = new QCheckBox(QStringLiteral("Bookmark line"), this);
	_purge = new QCheckBox(QStringLiteral("Purge for each search"), this);
	_recursive = new QCheckBox(QStringLiteral("In all su&b-folders"), this);
	_recursive->setChecked(true);
	_hidden = new QCheckBox(QStringLiteral("In &hidden folders"), this);
	for (QCheckBox* c : {_bookmarkLine, _purge, _backward, _wholeWord, _matchCase, _wrapAround, _inSelection, _recursive, _hidden})
		opts->addWidget(c);
	opts->addStretch();

	auto* modeBox = new QGroupBox(QStringLiteral("Search Mode"), this);
	auto* modeLayout = new QVBoxLayout(modeBox);
	_modeNormal = new QRadioButton(QStringLiteral("&Normal"), modeBox);
	_modeExtended = new QRadioButton(QStringLiteral("E&xtended (\\n, \\r, \\t, \\0, \\x...)"), modeBox);
	_modeRegex = new QRadioButton(QStringLiteral("Re&gular expression"), modeBox);
	_dotNewline = new QCheckBox(QStringLiteral("&. matches newline"), modeBox);
	_modeNormal->setChecked(true);
	modeLayout->addWidget(_modeNormal);
	modeLayout->addWidget(_modeExtended);
	auto* regexRow = new QHBoxLayout;
	regexRow->addWidget(_modeRegex);
	regexRow->addWidget(_dotNewline);
	modeLayout->addLayout(regexRow);
	optionsRow->addWidget(modeBox, 0, Qt::AlignTop);
	connect(_modeRegex, &QRadioButton::toggled, this, [this](bool on)
	{
		_dotNewline->setEnabled(on);
		if (on)
			_wholeWord->setChecked(false);
		_wholeWord->setEnabled(!on);
	});
	_dotNewline->setEnabled(false);

	auto* buttons = new QVBoxLayout;
	body->addLayout(buttons);
	auto makeButton = [this, buttons](const QString& text, void (FindReplaceDlg::*slot)())
	{
		auto* b = new QPushButton(text, this);
		b->setAutoDefault(false);
		buttons->addWidget(b);
		connect(b, &QPushButton::clicked, this, slot);
		return b;
	};
	_btnFindNext = makeButton(QStringLiteral("Find Next"), &FindReplaceDlg::onFindNext);
	_btnFindNext->setDefault(true);
	_btnCount = makeButton(QStringLiteral("Coun&t"), &FindReplaceDlg::onCount);
	_btnFindAllOpened = makeButton(QStringLiteral("Find All in All &Opened\nDocuments"), &FindReplaceDlg::onFindAllOpened);
	_btnFindAllCurrent = makeButton(QStringLiteral("Find All in Current\nDocument"), &FindReplaceDlg::onFindAllCurrent);
	_btnReplace = makeButton(QStringLiteral("&Replace"), &FindReplaceDlg::onReplace);
	_btnReplaceAll = makeButton(QStringLiteral("Replace &All"), &FindReplaceDlg::onReplaceAll);
	_btnReplaceAllOpened = makeButton(QStringLiteral("Replace All in All &Opened\nDocuments"), &FindReplaceDlg::onReplaceAllOpened);
	_btnFindInFiles = new QPushButton(QStringLiteral("Find All"), this);
	buttons->addWidget(_btnFindInFiles);
	connect(_btnFindInFiles, &QPushButton::clicked, this, [this] { onFindInFiles(false); });
	_btnReplaceInFiles = new QPushButton(QStringLiteral("Replace in Files"), this);
	buttons->addWidget(_btnReplaceInFiles);
	connect(_btnReplaceInFiles, &QPushButton::clicked, this, [this] { onFindInFiles(true); });
	_btnMarkAll = makeButton(QStringLiteral("Mark All"), &FindReplaceDlg::onMarkAll);
	_btnClearMarks = makeButton(QStringLiteral("Clear all marks"), &FindReplaceDlg::onClearMarks);
	_btnCopyMarked = makeButton(QStringLiteral("Copy Marked Text"), &FindReplaceDlg::onCopyMarked);
	auto* close = new QPushButton(QStringLiteral("Close"), this);
	close->setAutoDefault(false);
	buttons->addWidget(close);
	connect(close, &QPushButton::clicked, this, &QDialog::hide);
	buttons->addStretch();

	_status = new QLabel(this);
	_status->setFrameShape(QFrame::StyledPanel);
	_status->setMinimumHeight(22);
	root->addWidget(_status);
}

void FindReplaceDlg::updateForTab()
{
	const int tab = _tabBar->currentIndex();
	const bool replace = (tab == ReplaceTab);
	const bool fif = (tab == FindInFilesTab);
	const bool fip = (tab == FindInProjectsTab);
	const bool files = fif || fip;
	const bool mark = (tab == MarkTab);
	const bool find = (tab == FindTab);

	static const char* titles[] = {"Find", "Replace", "Find in Files", "Find in Projects", "Mark"};
	setWindowTitle(QString::fromLatin1(titles[tab]));

	_replaceLabel->setVisible(replace || files);
	_replaceCombo->setVisible(replace || files);
	_filtersLabel->setVisible(files);
	_filtersCombo->setVisible(files);
	_dirLabel->setVisible(fif);
	_dirCombo->setVisible(fif);
	_dirBrowse->setVisible(fif);
	_projectRow->setVisible(fip);
	_btnReplaceInFiles->setText(fip ? QStringLiteral("Replace in Projects") : QStringLiteral("Replace in Files"));

	_backward->setVisible(!files);
	_wrapAround->setVisible(!files);
	_inSelection->setVisible(replace || mark || find);
	_bookmarkLine->setVisible(mark);
	_purge->setVisible(mark);
	_recursive->setVisible(fif);
	_hidden->setVisible(fif);

	_btnFindNext->setVisible(find || replace);
	_btnCount->setVisible(find);
	_btnFindAllOpened->setVisible(find);
	_btnFindAllCurrent->setVisible(find);
	_btnReplace->setVisible(replace);
	_btnReplaceAll->setVisible(replace);
	_btnReplaceAllOpened->setVisible(replace);
	_btnFindInFiles->setVisible(files);
	_btnReplaceInFiles->setVisible(files);
	_btnMarkAll->setVisible(mark);
	_btnClearMarks->setVisible(mark);
	_btnCopyMarked->setVisible(mark);

	_btnFindNext->setDefault(find || replace);
	_btnFindInFiles->setDefault(files);
	_btnMarkAll->setDefault(mark);
	adjustSize();
}

void FindReplaceDlg::showTab(int tab)
{
	// Preferences > Searching > When Find Dialog is Invoked
	const NppPrefs& p = Npp::prefs();
	EditView* v = _mw->editView();
	QString sel = v->selectedText();
	const qsizetype selLen = v->ex(SCI_GETSELECTIONEND) - v->ex(SCI_GETSELECTIONSTART);
	if (_inSelection)
		_inSelection->setChecked(p.inSelectionThreshold > 0 && selLen >= p.inSelectionThreshold);
	if (p.fillFindFieldWithSelected)
	{
		if (sel.isEmpty() && p.fillFindFieldSelectCaret)
		{
			const sptr_t pos = v->ex(SCI_GETCURRENTPOS);
			const sptr_t ws = v->ex(SCI_WORDSTARTPOSITION, pos, true);
			const sptr_t we = v->ex(SCI_WORDENDPOSITION, pos, true);
			if (we > ws)
			{
				v->ex(SCI_SETSEL, ws, we);
				sel = v->selectedText();
			}
		}
		if (!sel.isEmpty() && !sel.contains(QLatin1Char('\n')) && !sel.contains(QLatin1Char('\r')) && sel.size() <= p.fillFindWhatThreshold &&
		    !(_inSelection && _inSelection->isChecked()))
			_findCombo->setCurrentText(sel);
	}
	if (tab == FindInFilesTab && (_dirCombo->currentText().isEmpty() || p.fillDirFromActiveDoc))
	{
		Buffer* b = _mw->currentBuffer();
		if (b && !b->isUntitled())
			_dirCombo->setCurrentText(QFileInfo(b->path).absolutePath());
		else if (_dirCombo->currentText().isEmpty())
			_dirCombo->setCurrentText(QDir::homePath());
	}
	// Use Monospaced font in Find dialog
	const QFont comboFont = p.monospacedFindDlg ? QFontDatabase::systemFont(QFontDatabase::FixedFont) : font();
	for (QComboBox* c : {_findCombo, _replaceCombo})
		c->setFont(comboFont);
	_tabBar->setCurrentIndex(tab);
	updateForTab();
	_status->clear();
	show();
	raise();
	activateWindow();
	_findCombo->setFocus();
	_findCombo->lineEdit()->selectAll();
}

void FindReplaceDlg::setFindText(const QString& text)
{
	_findCombo->setCurrentText(text);
}

QString FindReplaceDlg::findText() const
{
	return _findCombo->currentText();
}

void FindReplaceDlg::setDirectory(const QString& dir)
{
	_dirCombo->setCurrentText(dir);
}

void FindReplaceDlg::setFilters(const QString& filters)
{
	_filtersCombo->setCurrentText(filters);
}

void FindReplaceDlg::setProjectPanels(int mask)
{
	for (int i = 0; i < 3; ++i)
		_projectChecks[i]->setChecked(mask & (1 << i));
}

FindOptions FindReplaceDlg::options() const
{
	FindOptions o;
	o.findText = _findCombo->currentText();
	o.replaceText = _replaceCombo->currentText();
	o.filters = _filtersCombo->currentText();
	o.directory = _dirCombo->currentText();
	o.wholeWord = _wholeWord->isChecked() && _wholeWord->isEnabled();
	o.matchCase = _matchCase->isChecked();
	o.wrapAround = _wrapAround->isChecked();
	o.backward = _backward->isChecked();
	o.inSelection = _inSelection->isChecked() && _inSelection->isVisible();
	o.searchType = _modeRegex->isChecked() ? FindRegex : _modeExtended->isChecked() ? FindExtended : FindNormal;
	o.dotMatchesNewline = _dotNewline->isChecked();
	o.recursive = _recursive->isChecked();
	o.hiddenFolders = _hidden->isChecked();
	o.bookmarkLine = _bookmarkLine->isChecked();
	o.purge = _purge->isChecked();
	Npp::findState().matchCase = o.matchCase;
	Npp::findState().wholeWord = o.wholeWord;
	return o;
}

void FindReplaceDlg::setStatus(const QString& text, int kind)
{
	static const char* colors[] = {"#0000c0", "#e00000", "#008000"};
	_status->setStyleSheet(QStringLiteral("QLabel { color: %1; }").arg(QString::fromLatin1(colors[std::clamp(kind, 0, 2)])));
	_status->setText(text);
	if (!isVisible() && !text.isEmpty())
		_mw->setStatusText(text);
}

void FindReplaceDlg::addToHistory(QComboBox* combo, const QString& text)
{
	if (text.isEmpty())
		return;
	const int idx = combo->findText(text);
	if (idx >= 0)
		combo->removeItem(idx);
	combo->insertItem(0, text);
	while (combo->count() > kHistoryMax)
		combo->removeItem(combo->count() - 1);
	combo->setCurrentIndex(0);
}

void FindReplaceDlg::loadHistory()
{
	QSettings& s = findSettings();
	s.beginGroup(QStringLiteral("Find"));
	_findCombo->addItems(s.value(QStringLiteral("findHistory")).toStringList());
	_replaceCombo->addItems(s.value(QStringLiteral("replaceHistory")).toStringList());
	QStringList filters = s.value(QStringLiteral("filtersHistory")).toStringList();
	if (filters.isEmpty())
		filters << QStringLiteral("*.*");
	_filtersCombo->addItems(filters);
	_dirCombo->addItems(s.value(QStringLiteral("dirHistory")).toStringList());
	_matchCase->setChecked(s.value(QStringLiteral("matchCase"), false).toBool());
	_wholeWord->setChecked(s.value(QStringLiteral("wholeWord"), false).toBool());
	_wrapAround->setChecked(s.value(QStringLiteral("wrapAround"), true).toBool());
	_recursive->setChecked(s.value(QStringLiteral("recursive"), true).toBool());
	_hidden->setChecked(s.value(QStringLiteral("hidden"), false).toBool());
	const int mode = s.value(QStringLiteral("searchMode"), 0).toInt();
	(mode == FindRegex ? _modeRegex : mode == FindExtended ? _modeExtended : _modeNormal)->setChecked(true);
	_dotNewline->setChecked(s.value(QStringLiteral("dotMatchesNewline"), false).toBool());
	s.endGroup();
	_findCombo->setCurrentText(QString());
	_replaceCombo->setCurrentText(QString());
}

void FindReplaceDlg::saveHistory()
{
	auto items = [](QComboBox* c)
	{
		QStringList l;
		for (int i = 0; i < c->count(); ++i)
			l << c->itemText(i);
		return l;
	};
	QSettings& s = findSettings();
	s.beginGroup(QStringLiteral("Find"));
	s.setValue(QStringLiteral("findHistory"), items(_findCombo));
	s.setValue(QStringLiteral("replaceHistory"), items(_replaceCombo));
	s.setValue(QStringLiteral("filtersHistory"), items(_filtersCombo));
	s.setValue(QStringLiteral("dirHistory"), items(_dirCombo));
	const FindOptions o = options();
	s.setValue(QStringLiteral("matchCase"), o.matchCase);
	s.setValue(QStringLiteral("wholeWord"), _wholeWord->isChecked());
	s.setValue(QStringLiteral("wrapAround"), o.wrapAround);
	s.setValue(QStringLiteral("recursive"), o.recursive);
	s.setValue(QStringLiteral("hidden"), o.hiddenFolders);
	s.setValue(QStringLiteral("searchMode"), o.searchType);
	s.setValue(QStringLiteral("dotMatchesNewline"), o.dotMatchesNewline);
	s.endGroup();
}

// ---------------------------------------------------------------------------
// find / replace in the current view
// ---------------------------------------------------------------------------
bool FindReplaceDlg::find(const FindOptions& opt, bool forward, bool showMessages)
{
	// Port of FindReplaceDlg::processFindNext()
	EditView* v = _mw->editView();
	const QByteArray text = Searching::prepareText(opt.findText, opt.searchType);
	if (text.isEmpty())
		return false;

	const sptr_t docLength = v->ex(SCI_GETLENGTH);
	const sptr_t selMin = v->ex(SCI_GETSELECTIONSTART);
	const sptr_t selMax = v->ex(SCI_GETSELECTIONEND);
	sptr_t startPosition = forward ? selMax : selMax - 1;
	sptr_t endPosition = forward ? docLength : 0;
	if (!forward && selMin == selMax)
		startPosition = selMax;

	int flags = Searching::buildSearchFlags(opt) | SCFIND_REGEXP_EMPTYMATCH_ALL | SCFIND_REGEXP_SKIPCRLFASONE;
	if (v->ex(SCI_GETCHARAT, startPosition - 1) == '\r' && v->ex(SCI_GETCHARAT, startPosition) == '\n')
		flags = (flags & ~SCFIND_REGEXP_EMPTYMATCH_MASK) | SCFIND_REGEXP_EMPTYMATCH_NONE;
	v->ex(SCI_SETSEARCHFLAGS, flags);

	int status = 0;
	sptr_t posFind = searchInTarget(v, text, startPosition, endPosition);
	if (posFind == -1 && opt.wrapAround)
	{
		if (forward)
		{
			startPosition = 0;
			endPosition = docLength;
			status = 1;
		}
		else
		{
			startPosition = docLength;
			endPosition = 0;
			status = 2;
		}
		posFind = searchInTarget(v, text, startPosition, endPosition);
	}

	if (posFind == FIND_INVALID_REGULAR_EXPRESSION)
	{
		if (showMessages)
			setStatus(QStringLiteral("Find: Invalid regular expression"), 1);
		return false;
	}
	if (posFind < 0)
	{
		if (showMessages)
		{
			QString msg = QStringLiteral("Find: Can't find the text \"%1\"").arg(opt.findText);
			if (!(opt.wrapAround && !opt.matchCase && !opt.wholeWord))
				msg += QStringLiteral(" (try with less restrictive options: wrap around, no match case, no whole word)");
			setStatus(msg, 1);
		}
		return false;
	}

	const sptr_t start = posFind;
	const sptr_t end = v->ex(SCI_GETTARGETEND);
	v->displaySection(start, end, forward);
	if (start == end)
		v->exs(SCI_CALLTIPSHOW, start, "^ zero length match");

	if (showMessages)
	{
		if (status == 1)
			setStatus(QStringLiteral("Find: Found the 1st occurrence from the top. The end of the document has been reached."), 2);
		else if (status == 2)
			setStatus(QStringLiteral("Find: Found the 1st occurrence from the bottom. The beginning of the document has been reached."), 2);
		else
			setStatus(QString(), 0);
	}
	return true;
}

bool FindReplaceDlg::findNext(bool forward)
{
	FindOptions opt = options();
	if (opt.findText.isEmpty())
	{
		showTab(FindTab);
		return false;
	}
	if (opt.backward)
		forward = !forward;
	return find(opt, forward, true);
}

void FindReplaceDlg::onFindNext()
{
	addToHistory(_findCombo, _findCombo->currentText());
	FindOptions opt = options();
	find(opt, !opt.backward, true);
}

int FindReplaceDlg::countInView(EditView* v, const FindOptions& opt, bool inSelection)
{
	const QByteArray text = Searching::prepareText(opt.findText, opt.searchType);
	if (text.isEmpty())
		return 0;
	sptr_t start = inSelection ? v->ex(SCI_GETSELECTIONSTART) : 0;
	sptr_t end = inSelection ? v->ex(SCI_GETSELECTIONEND) : v->ex(SCI_GETLENGTH);
	v->ex(SCI_SETSEARCHFLAGS, Searching::buildSearchFlags(opt) | SCFIND_REGEXP_SKIPCRLFASONE | SCFIND_REGEXP_EMPTYMATCH_NOTAFTERMATCH);
	int count = 0;
	while (true)
	{
		const sptr_t pos = searchInTarget(v, text, start, end);
		if (pos == FIND_INVALID_REGULAR_EXPRESSION)
			return -1;
		if (pos < 0)
			break;
		const sptr_t tEnd = v->ex(SCI_GETTARGETEND);
		if (tEnd > end)
			break;
		++count;
		if (tEnd == end)
			break;
		start = tEnd;
	}
	return count;
}

std::vector<SearchHit> FindReplaceDlg::findAllInView(EditView* v, const FindOptions& opt)
{
	std::vector<SearchHit> hits;
	const QByteArray text = Searching::prepareText(opt.findText, opt.searchType);
	if (text.isEmpty())
		return hits;
	sptr_t start = 0;
	const sptr_t end = v->ex(SCI_GETLENGTH);
	v->ex(SCI_SETSEARCHFLAGS, Searching::buildSearchFlags(opt) | SCFIND_REGEXP_SKIPCRLFASONE | SCFIND_REGEXP_EMPTYMATCH_NOTAFTERMATCH);
	while (true)
	{
		const sptr_t pos = searchInTarget(v, text, start, end);
		if (pos < 0)
			break;
		const sptr_t tEnd = v->ex(SCI_GETTARGETEND);
		if (tEnd > end)
			break;
		hits.push_back(SearchHit{v->ex(SCI_LINEFROMPOSITION, pos), pos, tEnd});
		if (tEnd == end)
			break;
		start = tEnd;
	}
	return hits;
}

int FindReplaceDlg::replaceAllInView(EditView* v, const FindOptions& opt, bool inSelection)
{
	const QByteArray text = Searching::prepareText(opt.findText, opt.searchType);
	if (text.isEmpty())
		return 0;
	const bool isRegex = (opt.searchType == FindRegex);
	const QByteArray replacement = isRegex ? opt.replaceText.toUtf8() : Searching::prepareText(opt.replaceText, opt.searchType);

	sptr_t startRange = inSelection ? v->ex(SCI_GETSELECTIONSTART) : 0;
	sptr_t endRange = inSelection ? v->ex(SCI_GETSELECTIONEND) : v->ex(SCI_GETLENGTH);
	v->ex(SCI_SETSEARCHFLAGS, Searching::buildSearchFlags(opt) | SCFIND_REGEXP_SKIPCRLFASONE | SCFIND_REGEXP_EMPTYMATCH_NOTAFTERMATCH);

	int count = 0;
	v->ex(SCI_BEGINUNDOACTION);
	while (true)
	{
		const sptr_t targetStart = searchInTarget(v, text, startRange, endRange);
		if (targetStart == FIND_INVALID_REGULAR_EXPRESSION)
		{
			count = -1;
			break;
		}
		if (targetStart < 0)
			break;
		const sptr_t targetEnd = v->ex(SCI_GETTARGETEND);
		if (targetEnd > endRange)
			break;
		const sptr_t foundTextLen = targetEnd - targetStart;
		const sptr_t replacedLen = isRegex
			? v->ex(SCI_REPLACETARGETRE, replacement.size(), reinterpret_cast<sptr_t>(replacement.constData()))
			: v->ex(SCI_REPLACETARGET, replacement.size(), reinterpret_cast<sptr_t>(replacement.constData()));
		const sptr_t replaceDelta = replacedLen - foundTextLen;
		++count;
		if (targetStart + foundTextLen == endRange)
			break;
		startRange = targetStart + foundTextLen + replaceDelta;
		endRange += replaceDelta;
	}
	v->ex(SCI_ENDUNDOACTION);
	return count;
}

int FindReplaceDlg::markAllInView(EditView* v, const FindOptions& opt, bool inSelection)
{
	const QByteArray text = Searching::prepareText(opt.findText, opt.searchType);
	if (text.isEmpty())
		return 0;
	if (opt.purge)
	{
		v->clearIndicator(SCE_UNIVERSAL_FOUND_STYLE);
		v->ex(SCI_MARKERDELETEALL, MARK_BOOKMARK);
	}
	sptr_t start = inSelection ? v->ex(SCI_GETSELECTIONSTART) : 0;
	const sptr_t end = inSelection ? v->ex(SCI_GETSELECTIONEND) : v->ex(SCI_GETLENGTH);
	v->ex(SCI_SETSEARCHFLAGS, Searching::buildSearchFlags(opt) | SCFIND_REGEXP_SKIPCRLFASONE | SCFIND_REGEXP_EMPTYMATCH_NOTAFTERMATCH);
	v->ex(SCI_SETINDICATORCURRENT, SCE_UNIVERSAL_FOUND_STYLE);
	int count = 0;
	while (true)
	{
		const sptr_t pos = searchInTarget(v, text, start, end);
		if (pos == FIND_INVALID_REGULAR_EXPRESSION)
			return -1;
		if (pos < 0)
			break;
		const sptr_t tEnd = v->ex(SCI_GETTARGETEND);
		if (tEnd > end)
			break;
		if (tEnd > pos)
			v->ex(SCI_INDICATORFILLRANGE, pos, tEnd - pos);
		if (opt.bookmarkLine)
		{
			const sptr_t line = v->ex(SCI_LINEFROMPOSITION, pos);
			if (!(v->ex(SCI_MARKERGET, line) & (1 << MARK_BOOKMARK)))
				v->ex(SCI_MARKERADD, line, MARK_BOOKMARK);
		}
		++count;
		if (tEnd == end)
			break;
		start = tEnd;
	}
	return count;
}

void FindReplaceDlg::onCount()
{
	addToHistory(_findCombo, _findCombo->currentText());
	const FindOptions opt = options();
	const int n = countInView(_mw->editView(), opt, opt.inSelection);
	if (n < 0)
		setStatus(QStringLiteral("Count: Invalid regular expression"), 1);
	else
		setStatus(QStringLiteral("Count: %1 match%2.").arg(n).arg(n == 1 ? QString() : QStringLiteral("es")), n ? 0 : 1);
}

void FindReplaceDlg::onFindAllCurrent()
{
	addToHistory(_findCombo, _findCombo->currentText());
	const FindOptions opt = options();
	Buffer* b = _mw->currentBuffer();
	if (!b)
		return;
	EditView* v = _mw->editView();
	const SearchResultsPanel::FileResult fr = SearchResultsPanel::collect(v, findAllInView(v, opt), b->fullName());
	_mw->resultsPanel()->addSearch(opt.findText, {fr}, 1);
	setStatus(QStringLiteral("Find All in Current Document: %1").arg(hitsString(fr.hitCount)), fr.hitCount ? 0 : 1);
	if (!Npp::prefs().findDlgAlwaysVisible)
		hide(); // Searching > Find dialog remains open after search that outputs to results window
}

void FindReplaceDlg::onFindAllOpened()
{
	addToHistory(_findCombo, _findCombo->currentText());
	const FindOptions opt = options();
	const std::vector<Buffer*> buffers = _mw->allBuffers();
	std::vector<SearchResultsPanel::FileResult> results;
	int total = 0;
	for (Buffer* b : buffers)
	{
		_mw->withDocument(b, [&](EditView* v)
		{
			SearchResultsPanel::FileResult fr = SearchResultsPanel::collect(v, findAllInView(v, opt), b->fullName());
			total += fr.hitCount;
			if (fr.hitCount)
				results.push_back(std::move(fr));
		});
	}
	_mw->resultsPanel()->addSearch(opt.findText, results, static_cast<int>(buffers.size()));
	setStatus(QStringLiteral("Find All in All Opened Documents: %1").arg(hitsString(total)), total ? 0 : 1);
	if (!Npp::prefs().findDlgAlwaysVisible)
		hide();
}

void FindReplaceDlg::onReplace()
{
	// Port of FindReplaceDlg::processReplace()
	addToHistory(_findCombo, _findCombo->currentText());
	addToHistory(_replaceCombo, _replaceCombo->currentText());
	FindOptions opt = options();
	EditView* v = _mw->editView();
	Buffer* b = _mw->currentBuffer();
	if (b && b->isReadOnly())
	{
		setStatus(QStringLiteral("Replace: Cannot replace text. The current document is read only."), 1);
		return;
	}
	const QByteArray text = Searching::prepareText(opt.findText, opt.searchType);
	if (text.isEmpty())
		return;
	const bool isRegex = (opt.searchType == FindRegex);
	const QByteArray replacement = isRegex ? opt.replaceText.toUtf8() : Searching::prepareText(opt.replaceText, opt.searchType);
	const bool forward = !opt.backward;

	const sptr_t selStart = v->ex(SCI_GETSELECTIONSTART);
	const sptr_t selEnd = v->ex(SCI_GETSELECTIONEND);
	v->ex(SCI_SETSEARCHFLAGS, Searching::buildSearchFlags(opt) | SCFIND_REGEXP_EMPTYMATCH_ALL | SCFIND_REGEXP_EMPTYMATCH_ALLOWATSTART | SCFIND_REGEXP_SKIPCRLFASONE);
	const sptr_t pos = searchInTarget(v, text, selStart, v->ex(SCI_GETLENGTH));
	const bool selectionMatches = (pos == selStart && v->ex(SCI_GETTARGETEND) == selEnd);

	bool replaced = false;
	if (selectionMatches)
	{
		v->ex(SCI_SETTARGETRANGE, selStart, selEnd);
		const sptr_t len = isRegex
			? v->ex(SCI_REPLACETARGETRE, replacement.size(), reinterpret_cast<sptr_t>(replacement.constData()))
			: v->ex(SCI_REPLACETARGET, replacement.size(), reinterpret_cast<sptr_t>(replacement.constData()));
		const sptr_t caret = forward ? selStart + len : selStart;
		v->ex(SCI_SETSEL, caret, caret);
		replaced = true;
	}
	const bool found = find(opt, forward, false);
	if (replaced)
		setStatus(found ? QStringLiteral("Replace: 1 occurrence was replaced. The next occurrence found.")
		                : QStringLiteral("Replace: 1 occurrence was replaced. No more occurrences were found."), found ? 0 : 2);
	else
		setStatus(found ? QString() : QStringLiteral("Replace: no occurrence was found."), found ? 0 : 1);
}

void FindReplaceDlg::onReplaceAll()
{
	addToHistory(_findCombo, _findCombo->currentText());
	addToHistory(_replaceCombo, _replaceCombo->currentText());
	const FindOptions opt = options();
	Buffer* b = _mw->currentBuffer();
	if (b && b->isReadOnly())
	{
		setStatus(QStringLiteral("Replace All: Cannot replace text. The current document is read only."), 1);
		return;
	}
	const int n = replaceAllInView(_mw->editView(), opt, opt.inSelection);
	if (n < 0)
		setStatus(QStringLiteral("Replace All: Invalid regular expression"), 1);
	else
		setStatus(QStringLiteral("Replace All: %1 occurrence%2 replaced in %3.")
			.arg(n).arg(n == 1 ? QStringLiteral(" was") : QStringLiteral("s were"))
			.arg(opt.inSelection ? QStringLiteral("selected text") : QStringLiteral("entire file")), n ? 0 : 1);
}

void FindReplaceDlg::onReplaceAllOpened()
{
	addToHistory(_findCombo, _findCombo->currentText());
	addToHistory(_replaceCombo, _replaceCombo->currentText());
	const FindOptions opt = options();
	const auto answer = QMessageBox::question(this, QStringLiteral("Replace All in All Opened Documents"),
		QStringLiteral("Are you sure you want to replace all occurrences in all opened documents?"));
	if (answer != QMessageBox::Yes)
		return;
	int total = 0;
	for (Buffer* b : _mw->allBuffers())
	{
		if (b->isReadOnly())
			continue;
		_mw->withDocument(b, [&](EditView* v)
		{
			const int n = replaceAllInView(v, opt, false);
			if (n > 0)
				total += n;
		});
	}
	setStatus(QStringLiteral("Replace in Opened Files: %1 occurrence%2 replaced.").arg(total).arg(total == 1 ? QStringLiteral(" was") : QStringLiteral("s were")), total ? 0 : 1);
}

// ---------------------------------------------------------------------------
// Find in Files
// ---------------------------------------------------------------------------
QStringList FindReplaceDlg::collectFiles(const FindOptions& opt, bool* cancelled)
{
	QStringList include;
	QStringList excludeFiles;
	QStringList excludeDirs;
	QString filters = opt.filters.trimmed();
	for (QString f : filters.split(QRegularExpression(QStringLiteral("[\\s;]+")), Qt::SkipEmptyParts))
	{
		if (f.startsWith(QStringLiteral("!+\\")) || f.startsWith(QStringLiteral("!+/")))
			excludeDirs << f.mid(3);
		else if (f.startsWith(QStringLiteral("!\\")) || f.startsWith(QStringLiteral("!/")))
			excludeDirs << f.mid(2);
		else if (f.startsWith(QLatin1Char('!')))
			excludeFiles << f.mid(1);
		else
			include << (f == QStringLiteral("*.*") ? QStringLiteral("*") : f);
	}
	if (include.isEmpty())
		include << QStringLiteral("*");

	auto matches = [](const QString& name, const QStringList& patterns)
	{
		for (const QString& p : patterns)
		{
			const QRegularExpression re(QRegularExpression::wildcardToRegularExpression(p, QRegularExpression::UnanchoredWildcardConversion).prepend(QLatin1Char('^')).append(QLatin1Char('$')));
			if (re.match(name).hasMatch())
				return true;
		}
		return false;
	};

	QStringList files;
	QDir::Filters dirFilters = QDir::Files | QDir::NoDotAndDotDot | QDir::Readable;
	if (opt.hiddenFolders)
		dirFilters |= QDir::Hidden;
	QDirIterator::IteratorFlags flags = opt.recursive ? QDirIterator::Subdirectories : QDirIterator::NoIteratorFlags;
	QDirIterator it(opt.directory, dirFilters, flags);
	int n = 0;
	while (it.hasNext())
	{
		const QString path = it.next();
		if ((++n & 0x3FF) == 0)
		{
			QApplication::processEvents();
			if (cancelled && *cancelled)
				break;
		}
		const QString rel = QDir(opt.directory).relativeFilePath(QFileInfo(path).absolutePath());
		bool skip = false;
		if (!opt.hiddenFolders)
		{
			for (const QString& part : rel.split(QLatin1Char('/'), Qt::SkipEmptyParts))
			{
				if (part.startsWith(QLatin1Char('.')) && part != QStringLiteral(".") && part != QStringLiteral(".."))
				{
					skip = true;
					break;
				}
			}
		}
		for (const QString& d : excludeDirs)
		{
			if (rel.split(QLatin1Char('/')).contains(d))
				skip = true;
		}
		if (skip)
			continue;
		const QString name = it.fileName();
		if (!matches(name, include) || matches(name, excludeFiles))
			continue;
		files << path;
	}
	return files;
}

void FindReplaceDlg::onFindInFiles(bool replace)
{
	addToHistory(_findCombo, _findCombo->currentText());
	addToHistory(_filtersCombo, _filtersCombo->currentText());
	addToHistory(_dirCombo, _dirCombo->currentText());
	if (replace)
		addToHistory(_replaceCombo, _replaceCombo->currentText());
	const FindOptions opt = options();
	if (opt.findText.isEmpty())
		return;
	const bool inProjects = (_tabBar->currentIndex() == FindInProjectsTab);
	const QString what = inProjects ? (replace ? QStringLiteral("Replace in Projects") : QStringLiteral("Find in Projects"))
	                                : (replace ? QStringLiteral("Replace in Files") : QStringLiteral("Find in Files"));
	int projectMask = 0;
	for (int i = 0; i < 3; ++i)
		projectMask |= _projectChecks[i]->isChecked() ? (1 << i) : 0;
	if (inProjects && !projectMask)
	{
		setStatus(QStringLiteral("%1: no Project Panel is checked.").arg(what), 1);
		return;
	}
	if (!inProjects && !QFileInfo(opt.directory).isDir())
	{
		setStatus(QStringLiteral("Find in Files: The directory doesn't exist."), 1);
		return;
	}
	if (replace)
	{
		const auto answer = QMessageBox::question(this, QStringLiteral("Are you sure?"),
			inProjects ? QStringLiteral("Are you sure you want to replace all occurrences of:\n%1\nwith:\n%2\nin all the files of the selected projects?")
			                 .arg(opt.findText, opt.replaceText)
			           : QStringLiteral("Are you sure you want to replace all occurrences of:\n%1\nwith:\n%2\nin all files of\n%3 ?")
				.arg(opt.findText, opt.replaceText, opt.directory));
		if (answer != QMessageBox::Yes)
			return;
	}

	bool cancelled = false;
	QProgressDialog progress(replace ? QStringLiteral("Replace in Files") : QStringLiteral("Find in Files"), QStringLiteral("Cancel"), 0, 0, this);
	progress.setWindowModality(Qt::WindowModal);
	progress.setMinimumDuration(400);
	connect(&progress, &QProgressDialog::canceled, this, [&cancelled] { cancelled = true; });
	progress.setLabelText(QStringLiteral("Collecting files..."));

	QStringList files;
	if (inProjects)
	{
		// NPPM_INTERNAL_FINDINPROJECTS: the files of the checked panels that match the filters
		QStringList include, exclude;
		for (const QString& f : opt.filters.trimmed().split(QRegularExpression(QStringLiteral("[\\s;]+")), Qt::SkipEmptyParts))
		{
			if (f.startsWith(QLatin1Char('!')))
				exclude << f.mid(1);
			else
				include << (f == QStringLiteral("*.*") ? QStringLiteral("*") : f);
		}
		auto matches = [](const QString& name, const QStringList& patterns)
		{
			for (const QString& pat : patterns)
			{
				if (QRegularExpression(QRegularExpression::wildcardToRegularExpression(pat), QRegularExpression::CaseInsensitiveOption).match(name).hasMatch())
					return true;
			}
			return false;
		};
		for (const QString& f : _mw->projectFiles(projectMask, {}))
		{
			const QString name = QFileInfo(f).fileName();
			if ((include.isEmpty() || matches(name, include)) && !matches(name, exclude) && QFileInfo(f).isFile())
				files << f;
		}
	}
	else
	{
		files = collectFiles(opt, &cancelled);
	}
	progress.setMaximum(static_cast<int>(files.size()));

	std::vector<SearchResultsPanel::FileResult> results;
	int totalHits = 0;
	int searched = 0;
	int replacedFiles = 0;

	auto handleView = [&](EditView* v, const QString& path) -> int
	{
		if (replace)
		{
			const int n = replaceAllInView(v, opt, false);
			if (n > 0)
			{
				totalHits += n;
				++replacedFiles;
			}
			return n;
		}
		SearchResultsPanel::FileResult fr = SearchResultsPanel::collect(v, findAllInView(v, opt), path);
		totalHits += fr.hitCount;
		if (fr.hitCount)
			results.push_back(std::move(fr));
		return 0;
	};

	for (int i = 0; i < files.size() && !cancelled; ++i)
	{
		const QString& path = files[i];
		progress.setValue(i);
		if ((i & 0xF) == 0)
		{
			progress.setLabelText(path);
			QApplication::processEvents();
		}
		++searched;

		// Searching > Find in Files: Prefer on-disk file content over open buffers
		Buffer* open = _mw->findBuffer(path);
		if (open && (replace || !Npp::prefs().fifPreferDisk))
		{
			if (replace && open->isReadOnly())
				continue;
			_mw->withDocument(open, [&](EditView* v) { handleView(v, path); });
			continue;
		}

		QFile f(path);
		if (!f.open(QIODevice::ReadOnly))
			continue;
		const QByteArray raw = f.readAll();
		f.close();
		if (raw.left(8000).contains('\0'))
			continue; // binary file
		const FileIO::Loaded loaded = FileIO::decode(raw);
		_searchView->ex(SCI_SETDOCPOINTER, 0, 0);
		_searchView->ex(SCI_SETUNDOCOLLECTION, false);
		_searchView->ex(SCI_APPENDTEXT, loaded.utf8.size(), reinterpret_cast<sptr_t>(loaded.utf8.constData()));
		const int n = handleView(_searchView, path);
		if (replace && n > 0)
		{
			QString err;
			bool lossy = false;
			if (!FileIO::save(path, _searchView->wholeText(), loaded.unicode, loaded.codepage, &err, &lossy))
				setStatus(QStringLiteral("Replace in Files: cannot write %1: %2").arg(path, err), 1);
		}
	}
	_searchView->ex(SCI_SETDOCPOINTER, 0, 0);
	progress.setValue(progress.maximum());

	if (replace)
	{
		setStatus(QStringLiteral("%5: %1 occurrence%2 replaced in %3 file%4.")
			.arg(totalHits).arg(totalHits == 1 ? QStringLiteral(" was") : QStringLiteral("s were"))
			.arg(replacedFiles).arg(replacedFiles == 1 ? QString() : QStringLiteral("s")).arg(what), totalHits ? 0 : 1);
		return;
	}
	_mw->resultsPanel()->addSearch(opt.findText, results, searched);
	setStatus(QStringLiteral("%4: %1 in %2 file%3 searched.").arg(hitsString(totalHits)).arg(searched).arg(searched == 1 ? QString() : QStringLiteral("s")).arg(what), totalHits ? 0 : 1);
	if (!Npp::prefs().findDlgAlwaysVisible)
		hide();
}

// ---------------------------------------------------------------------------
// Mark
// ---------------------------------------------------------------------------
void FindReplaceDlg::onMarkAll()
{
	addToHistory(_findCombo, _findCombo->currentText());
	const FindOptions opt = options();
	const int n = markAllInView(_mw->editView(), opt, opt.inSelection);
	if (n < 0)
		setStatus(QStringLiteral("Mark: Invalid regular expression"), 1);
	else
		setStatus(QStringLiteral("Mark: %1 match%2.").arg(n).arg(n == 1 ? QString() : QStringLiteral("es")), n ? 0 : 1);
}

void FindReplaceDlg::onClearMarks()
{
	EditView* v = _mw->editView();
	v->clearIndicator(SCE_UNIVERSAL_FOUND_STYLE);
	v->ex(SCI_MARKERDELETEALL, MARK_BOOKMARK);
	setStatus(QString(), 0);
}

void FindReplaceDlg::onCopyMarked()
{
	EditView* v = _mw->editView();
	QByteArray out;
	const QByteArray eol = v->eolString();
	sptr_t pos = 0;
	const sptr_t len = v->ex(SCI_GETLENGTH);
	while (pos < len)
	{
		if (v->ex(SCI_INDICATORVALUEAT, SCE_UNIVERSAL_FOUND_STYLE, pos))
		{
			const sptr_t end = v->ex(SCI_INDICATOREND, SCE_UNIVERSAL_FOUND_STYLE, pos);
			if (!out.isEmpty())
				out += eol;
			out += v->textRange(pos, end);
			pos = std::max(end, pos + 1);
		}
		else
		{
			const sptr_t next = v->ex(SCI_INDICATOREND, SCE_UNIVERSAL_FOUND_STYLE, pos);
			if (next <= pos)
				break;
			pos = next;
		}
	}
	QApplication::clipboard()->setText(QString::fromUtf8(out));
}
