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

// Search > Incremental Search: Qt counterpart of FindIncrementDlg (ScintillaComponent/FindReplaceDlg.cpp),
// a bar under the editor (or above it, with its ˄ / ˅ button).

#pragma once

#include <utility>
#include <vector>

#include <QWidget>

#include "NppCommon.h"

class MainWindow;
class QCheckBox;
class QLabel;
class QLineEdit;
class QToolButton;

class IncrementalFindBar : public QWidget
{
	Q_OBJECT

public:
	enum FindStatus { FSFound, FSNotFound, FSTopReached, FSEndReached, FSResetCount };

	IncrementalFindBar(MainWindow* mw, QWidget* parent);

	void display(bool show);                    // FindIncrementDlg::display()
	void setSearchText(const QString& text);    // the selection when the bar is opened
	void findNext(bool forward);                // IDC_INCFINDNXTOK / IDC_INCFINDPREVOK, Enter, F3
	bool atTop() const { return _atTop; }
	FindStatus findStatus() const { return _status; }
	QLineEdit* edit() const { return _edit; }
	QLabel* statusLabel() const { return _statusLabel; }
	QCheckBox* matchCase() const { return _matchCase; }
	QCheckBox* highlightAll() const { return _hiliteAll; }
	QCheckBox* count() const { return _count; }

signals:
	void positionToggled(bool atTop); // NPPM_INTERNAL_INCSEARCHBARPOSITION

protected:
	bool eventFilter(QObject* obj, QEvent* event) override;

private:
	void update(bool updateSearch, bool advance, bool forward, bool updateHiLight, bool updateCase);
	bool processFindNext(const QByteArray& text, bool advance, bool forward, FindStatus* status, sptr_t* matchStart, sptr_t* matchEnd);
	void countMatches(const QByteArray& text);
	void markSelectedTextInc(bool enable);
	void setFindStatus(FindStatus status, size_t nbCounted, int nth);
	int searchFlags() const;

	MainWindow* _mw;
	QLineEdit* _edit = nullptr;
	QToolButton* _upDown = nullptr;
	QCheckBox* _matchCase = nullptr;
	QCheckBox* _hiliteAll = nullptr;
	QCheckBox* _count = nullptr;
	QLabel* _statusLabel = nullptr;
	FindStatus _status = FSFound;
	bool _atTop = false;
	std::vector<std::pair<sptr_t, sptr_t>> _matches;
};
