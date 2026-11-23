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

// Edit > Character Panel (WinControls/AnsiCharPanel) and Edit > Clipboard History
// (WinControls/ClipboardHistory): the two panels of the Edit menu.

#pragma once

#include <vector>

#include <QByteArray>
#include <QWidget>

class MainWindow;
class QListWidget;
class QTreeWidget;

// MIME type of Edit > Paste Special > Copy Binary Content (CF_NPPTEXTLEN on Windows):
// the exact bytes of the selection, NUL characters included
inline constexpr const char* kNppBinaryMime = "application/x-linuxpad-plus-plus-binary";

class CharacterPanel : public QWidget
{
	Q_OBJECT

public:
	CharacterPanel(MainWindow* mw, QWidget* parent);

	void switchEncoding(); // AnsiCharPanel::switchEncoding(): characters of the code page of the document
	void insertCell(int row, int column); // NM_DBLCLK: the character, or the text of the cell
	QTreeWidget* list() const { return _list; }
	int codepage() const { return _codepage; }

protected:
	bool eventFilter(QObject* obj, QEvent* event) override;

private:
	void setValues(int codepage);
	void insertBytes(const QByteArray& utf8);

	MainWindow* _mw;
	QTreeWidget* _list = nullptr;
	int _codepage = -2;
};

class ClipboardHistoryPanel : public QWidget
{
	Q_OBJECT

public:
	ClipboardHistoryPanel(MainWindow* mw, QWidget* parent);

	struct Entry
	{
		QByteArray data; // UTF-8 text, or the bytes of a binary copy
		bool binary = false;
	};
	void addEntry(const Entry& entry); // ClipboardHistoryPanel::addToClipboadHistory()
	void insertEntry(int row);         // ClipboardHistoryPanel::onDoubleClick()
	const std::vector<Entry>& entries() const { return _entries; }
	QListWidget* list() const { return _list; }

private:
	void onClipboardChanged();
	static QString displayString(const Entry& entry);

	MainWindow* _mw;
	QListWidget* _list = nullptr;
	std::vector<Entry> _entries; // most recent first
};
