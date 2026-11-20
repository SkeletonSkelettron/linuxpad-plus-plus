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

#include "IncrementalFind.h"

#include <QCheckBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QSettings>
#include <QToolButton>

#include "EditView.h"
#include "MainWindow.h"

namespace
{
	QSettings& incSettings()
	{
		static QSettings s(Npp::configDir() + QStringLiteral("/config.ini"), QSettings::IniFormat);
		return s;
	}
}

IncrementalFindBar::IncrementalFindBar(MainWindow* mw, QWidget* parent)
	: QWidget(parent), _mw(mw)
{
	setObjectName(QStringLiteral("incrementalFindBar"));
	auto* l = new QHBoxLayout(this);
	l->setContentsMargins(4, 2, 4, 2);
	auto* close = new QToolButton(this);
	close->setText(QStringLiteral("✕"));
	close->setToolTip(QStringLiteral("Close"));
	close->setAutoRaise(true);
	_upDown = new QToolButton(this);
	_upDown->setAutoRaise(true);
	auto* label = new QLabel(QStringLiteral("Find:"), this);
	_edit = new QLineEdit(this);
	_edit->setObjectName(QStringLiteral("incFindText"));
	_edit->setMinimumWidth(_edit->fontMetrics().averageCharWidth() * 36);
	label->setBuddy(_edit);
	auto* prev = new QToolButton(this);
	prev->setText(QStringLiteral("<"));
	prev->setToolTip(QStringLiteral("Find Previous"));
	auto* next = new QToolButton(this);
	next->setText(QStringLiteral(">"));
	next->setToolTip(QStringLiteral("Find Next"));
	_matchCase = new QCheckBox(QStringLiteral("Match &case"), this);
	_hiliteAll = new QCheckBox(QStringLiteral("&Highlight all"), this);
	_count = new QCheckBox(QStringLiteral("Count"), this);
	_statusLabel = new QLabel(this);
	_statusLabel->setObjectName(QStringLiteral("incFindStatus"));
	l->addWidget(close);
	l->addWidget(_upDown);
	l->addWidget(label);
	l->addWidget(_edit);
	l->addWidget(prev);
	l->addWidget(next);
	l->addSpacing(12);
	l->addWidget(_matchCase);
	l->addWidget(_hiliteAll);
	l->addWidget(_count);
	l->addSpacing(12);
	l->addWidget(_statusLabel, 1);

	// WM_INITDIALOG: FindHistory._isIncrementalMatchCase / HighlightAll / Count / searchBarAtTop
	_matchCase->setChecked(incSettings().value(QStringLiteral("Find/incrementalMatchCase"), false).toBool());
	_hiliteAll->setChecked(incSettings().value(QStringLiteral("Find/incrementalHighlightAll"), false).toBool());
	_count->setChecked(incSettings().value(QStringLiteral("Find/incrementalCount"), false).toBool());
	_atTop = incSettings().value(QStringLiteral("Find/incrementalSearchBarAtTop"), false).toBool();
	_upDown->setText(_atTop ? QStringLiteral("˅") : QStringLiteral("˄"));

	_edit->installEventFilter(this);
	connect(close, &QToolButton::clicked, this, [this] { display(false); });
	connect(_upDown, &QToolButton::clicked, this, [this]
	{
		_atTop = !_atTop;
		incSettings().setValue(QStringLiteral("Find/incrementalSearchBarAtTop"), _atTop);
		_upDown->setText(_atTop ? QStringLiteral("˅") : QStringLiteral("˄"));
		emit positionToggled(_atTop);
	});
	connect(prev, &QToolButton::clicked, this, [this] { findNext(false); });
	connect(next, &QToolButton::clicked, this, [this] { findNext(true); });
	connect(_edit, &QLineEdit::textChanged, this, [this]
	{
		_matches.clear();
		update(true, false, true, _hiliteAll->isChecked(), _matchCase->isChecked());
	});
	connect(_matchCase, &QCheckBox::toggled, this, [this](bool on)
	{
		incSettings().setValue(QStringLiteral("Find/incrementalMatchCase"), on);
		_matches.clear();
		update(true, false, true, true, true);
	});
	connect(_hiliteAll, &QCheckBox::toggled, this, [this](bool on)
	{
		incSettings().setValue(QStringLiteral("Find/incrementalHighlightAll"), on);
		update(false, false, true, true, false);
	});
	connect(_count, &QCheckBox::toggled, this, [this](bool on)
	{
		incSettings().setValue(QStringLiteral("Find/incrementalCount"), on);
		_matches.clear();
	});
	hide();
}

void IncrementalFindBar::display(bool show)
{
	if (show)
	{
		setVisible(true);
		_edit->setFocus();
		_edit->selectAll();
		return;
	}
	// IDCANCEL
	if (EditView* e = _mw->editView())
	{
		e->clearIndicator(SCE_UNIVERSAL_FOUND_STYLE_INC);
		e->setFocus();
	}
	setVisible(false);
	_matches.clear();
}

void IncrementalFindBar::setSearchText(const QString& text)
{
	_edit->setText(text);
}

void IncrementalFindBar::findNext(bool forward)
{
	update(true, true, forward, false, false);
}

bool IncrementalFindBar::eventFilter(QObject* obj, QEvent* event)
{
	if (obj == _edit && event->type() == QEvent::ShortcutOverride)
	{
		// the accelerators of the bar come before the ones of the main window (F3 searches in the bar)
		const int key = static_cast<QKeyEvent*>(event)->key();
		if (key == Qt::Key_F3 || key == Qt::Key_Escape || key == Qt::Key_Return || key == Qt::Key_Enter)
		{
			event->accept();
			return true;
		}
	}
	if (obj == _edit && event->type() == QEvent::KeyPress)
	{
		auto* ke = static_cast<QKeyEvent*>(event);
		const bool shift = ke->modifiers() & Qt::ShiftModifier;
		switch (ke->key())
		{
			case Qt::Key_Escape:
				display(false);
				return true;
			case Qt::Key_Return:
			case Qt::Key_Enter:
				findNext(!shift); // IDOK, Shift+Enter backwards
				return true;
			case Qt::Key_F3:
				findNext(!shift); // accelerators IDM_SEARCH_FINDNEXT / FINDPREV
				return true;
			default:
				break;
		}
	}
	return QWidget::eventFilter(obj, event);
}

int IncrementalFindBar::searchFlags() const
{
	return _matchCase->isChecked() ? SCFIND_MATCHCASE : 0;
}

bool IncrementalFindBar::processFindNext(const QByteArray& text, bool advance, bool forward, FindStatus* status, sptr_t* matchStart, sptr_t* matchEnd)
{
	// FindReplaceDlg::processFindNext() with FirstIncremental / NextIncremental, wrap around
	EditView* v = _mw->editView();
	*status = FSFound;
	*matchStart = *matchEnd = -1;
	if (!v || text.isEmpty())
		return false;
	const sptr_t docLength = v->ex(SCI_GETLENGTH);
	const sptr_t cpMin = v->ex(SCI_GETSELECTIONSTART);
	const sptr_t cpMax = v->ex(SCI_GETSELECTIONEND);
	sptr_t startPosition = advance ? cpMin + 1 : cpMin;
	sptr_t endPosition = docLength;
	if (!forward)
	{
		startPosition = advance ? cpMax - 1 : cpMax;
		endPosition = 0;
	}
	v->ex(SCI_SETSEARCHFLAGS, searchFlags());
	auto search = [&]
	{
		v->ex(SCI_SETTARGETRANGE, startPosition, endPosition);
		return v->ex(SCI_SEARCHINTARGET, static_cast<uptr_t>(text.size()), reinterpret_cast<sptr_t>(text.constData()));
	};
	sptr_t posFind = search();
	if (posFind == -1)
	{
		if (forward)
		{
			startPosition = 0;
			endPosition = docLength;
			*status = FSEndReached;
		}
		else
		{
			startPosition = docLength;
			endPosition = 0;
			*status = FSTopReached;
		}
		posFind = search();
		if (posFind == -1)
		{
			*status = FSNotFound;
			return false;
		}
	}
	const sptr_t start = posFind;
	const sptr_t end = v->ex(SCI_GETTARGETEND);
	v->displaySection(start, end, forward);
	*matchStart = start;
	*matchEnd = end;
	return true;
}

void IncrementalFindBar::countMatches(const QByteArray& text)
{
	// processAll(ProcessCountAll, ..., &_matches)
	_matches.clear();
	EditView* v = _mw->editView();
	if (!v || text.isEmpty())
		return;
	v->ex(SCI_SETSEARCHFLAGS, searchFlags());
	const sptr_t docEnd = v->ex(SCI_GETLENGTH);
	sptr_t pos = 0;
	while (pos <= docEnd)
	{
		v->ex(SCI_SETTARGETRANGE, pos, docEnd);
		const sptr_t found = v->ex(SCI_SEARCHINTARGET, static_cast<uptr_t>(text.size()), reinterpret_cast<sptr_t>(text.constData()));
		if (found < 0)
			break;
		const sptr_t end = v->ex(SCI_GETTARGETEND);
		_matches.emplace_back(found, end);
		pos = (end > found) ? end : found + 1;
	}
}

void IncrementalFindBar::update(bool updateSearch, bool advance, bool forward, bool updateHiLight, bool updateCase)
{
	// FindIncrementDlg::run_dlgProc() WM_COMMAND
	EditView* v = _mw->editView();
	if (!v)
		return;
	const QByteArray text = _edit->text().toUtf8();
	if (updateSearch)
	{
		FindStatus status = FSFound;
		sptr_t start = -1, end = -1;
		const bool found = processFindNext(text, advance, forward, &status, &start, &end);
		int nth = 0;
		if (_count->isChecked())
		{
			if (_matches.empty())
				countMatches(text);
			for (size_t i = 0; i < _matches.size(); ++i)
			{
				if (_matches[i].first == start && _matches[i].second == end)
				{
					nth = static_cast<int>(i) + 1;
					break;
				}
			}
		}
		setFindStatus(text.isEmpty() ? FSResetCount : status, _count->isChecked() ? _matches.size() : 0, nth);
		// case sensitivity changed and the selection does not match any more: caret at its start
		if (updateCase && !found)
		{
			const sptr_t cpMin = v->ex(SCI_GETSELECTIONSTART);
			v->ex(SCI_SETSEL, static_cast<uptr_t>(-1), cpMin);
		}
	}
	if (updateHiLight)
		markSelectedTextInc(!text.isEmpty() && _hiliteAll->isChecked());
}

void IncrementalFindBar::markSelectedTextInc(bool enable)
{
	// FindIncrementDlg::markSelectedTextInc(): every occurrence of the selected text
	EditView* v = _mw->editView();
	v->clearIndicator(SCE_UNIVERSAL_FOUND_STYLE_INC);
	if (!enable)
		return;
	const QByteArray sel = v->selectedTextUtf8();
	if (sel.isEmpty())
		return;
	v->markAllOccurrences(SCE_UNIVERSAL_FOUND_STYLE_INC, sel, searchFlags());
}

void IncrementalFindBar::setFindStatus(FindStatus status, size_t nbCounted, int nth)
{
	// FindIncrementDlg::setFindStatus()
	QString str;
	const QLocale loc;
	if (nbCounted > 0)
		str = loc.toString(nth) + QLatin1Char('/') + loc.toString(static_cast<qulonglong>(nbCounted));
	switch (status)
	{
		case FSNotFound:
			str = QStringLiteral("Phrase not found");
			break;
		case FSTopReached:
			str = nbCounted > 0 ? str + QStringLiteral(" - Reached top of page, continued from bottom")
			                    : QStringLiteral("Reached top of page, continued from bottom");
			break;
		case FSEndReached:
			str = nbCounted > 0 ? str + QStringLiteral(" - Reached end of page, continued from top")
			                    : QStringLiteral("Reached end of page, continued from top");
			break;
		case FSFound:
			break;
		case FSResetCount:
			str.clear();
			break;
	}
	_status = status;
	// WM_CTLCOLOREDIT: the field turns red when the text is not found
	_edit->setStyleSheet(status == FSNotFound ? QStringLiteral("QLineEdit { background: #FF6666; color: white; }") : QString());
	_statusLabel->setText(str);
}
