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

// Scintilla editing view with the Notepad++ behaviour.
// Qt counterpart of ScintillaComponent/ScintillaEditView.cpp.

#pragma once

#include <utility>
#include <vector>

#include <QByteArray>
#include <QString>

#include "ScintillaEditBase.h"
#include "NppCommon.h"

class EditView : public ScintillaEditBase
{
	Q_OBJECT

public:
	explicit EditView(QWidget* parent = nullptr, bool isMainEditor = true);

	sptr_t ex(unsigned int msg, uptr_t w = 0, sptr_t l = 0) const { return send(msg, w, l); }
	sptr_t exs(unsigned int msg, uptr_t w, const char* s) const { return send(msg, w, reinterpret_cast<sptr_t>(s)); }
	sptr_t execute(unsigned int msg, uptr_t w = 0, sptr_t l = 0) const { return send(msg, w, l); }

	void applyPrefs();
	void defineDocType(LangType lang, const QString& udlName = QString()); // udlName: the User Defined Language of L_USER, the lexer of L_EXTERNAL
	const QString& udlName() const { return _udlName; }
	LangType currentLang() const { return _lang; }
	void updateLineNumberWidth();
	void showMargin(int margin, bool show);
	static int changeHistoryFlags(); // SCI_SETCHANGEHISTORY value of Preferences > Margins

	// document information set by the main window
	void setDocPath(const QString& path) { _docPath = path; }
	const QString& docPath() const { return _docPath; }
	void setLargeFile(bool large) { _largeFile = large; }
	void setActiveView(bool active) { _activeView = active; } // split view: the focused view runs smart highlighting
	bool isLargeFile() const { return _largeFile; }
	bool allowSmartHilite() const;
	bool allowBraceMatch() const;
	bool allowClickableLink() const;
	bool allowAutoCompletion() const;

	// text helpers, UTF-8
	QByteArray textRange(sptr_t start, sptr_t end) const;
	QString textRangeQ(sptr_t start, sptr_t end) const { return QString::fromUtf8(textRange(start, end)); }
	void getText(char* dest, sptr_t start, sptr_t end) const;
	QByteArray selectedTextUtf8() const;
	QString selectedText() const { return QString::fromUtf8(selectedTextUtf8()); }
	QByteArray wholeText() const;
	QByteArray lineText(sptr_t line) const;
	QByteArray wordAt(sptr_t pos, sptr_t* start = nullptr, sptr_t* end = nullptr) const;
	QString currentWordOrSelection() const;
	std::pair<sptr_t, sptr_t> selectionLinesRange() const;
	void replaceRange(sptr_t start, sptr_t end, const QByteArray& text);
	void scrollPosToCenter(sptr_t pos);
	void gotoLine(sptr_t line);
	void displaySection(sptr_t start, sptr_t end, bool forward = true);
	QByteArray eolString() const;
	sptr_t lineLength(sptr_t line) const;
	void setLineIndent(sptr_t line, sptr_t indent) const;
	bool isShownIndentGuide() const { return ex(SCI_GETINDENTATIONGUIDES) != SC_IV_NONE; }
	bool isCurrentLineFolded() const;

	// Notepad++ features
	void doSmartHighlight();
	bool smartHighlightTarget(QByteArray* text, int* flags) const; // selection that smart highlighting looks for
	void highlightVisible(int indic, const QByteArray& text, int flags); // occurrences in the visible lines
	void doBraceMatch();
	void doTagMatch();
	void maintainIndentation(int ch);
	void insertMatchedChars(int ch);
	void foldAll(bool fold);
	void foldLevel(int level, bool fold);
	void foldCurrent(bool fold);
	void clearIndicator(int indic);
	void markAllOccurrences(int indic, const QByteArray& text, int flags);
	void highlightUrls();
	QString urlAt(sptr_t pos) const;
	void applyNonPrintingChars(); // ScintillaEditView::showNpc() / showCcUniEol()
	void selectBetweenDelimiters(sptr_t position); // Ctrl + double click (Preferences > Delimiter)
	void applyKeyBindings(); // the copy / cut keys of "Copy/Cut Line without selection"

	// View > Hide Lines: MARK_HIDELINESBEGIN / END markers belong to the document, the hidden lines to each view
	void hideLines();
	bool hidelineMarkerClicked(sptr_t lineNumber);
	void hideMarkedLines(sptr_t searchStart, bool toEndOfDoc);
	void showHiddenLines(sptr_t searchStart, bool toEndOfDoc, bool doDelete);
	void restoreHiddenLines();

signals:
	void contextMenuWanted(const QPoint& globalPos);
	void folded(); // fold state changed by a command or the fold margin (Document Map follows it)
	void hideLinesChanged(bool isHide, sptr_t location); // Buffer::setHideLineChanged(): every view of the document follows

protected:
	void contextMenuEvent(QContextMenuEvent* event) override;
	void keyPressEvent(QKeyEvent* event) override;

private:
	void onNotify(Scintilla::NotificationData* scn);
	void marginClick(sptr_t position, int modifiers, int margin);
	bool isConditionExprLine(sptr_t line) const;
	void applyWordChars();
	void applyEdges();
	QByteArray closeTag(sptr_t caretPos, bool isHtml) const;

	// AutoCompletion.cpp: InsertedMatchedChars
	struct MatchedCharInserted
	{
		char c = 0;
		sptr_t pos = -1;
	};
	void removeInvalidMatchedChars(const MatchedCharInserted& mci);
	sptr_t searchMatchedChar(char startChar, char endChar, sptr_t posToDetect);

	LangType _lang = L_TEXT;
	bool _isMain = true;
	bool _largeFile = false;
	bool _activeView = true;
	QString _docPath;
	QString _udlName;
	QByteArray _defaultWordChars;
	std::vector<MatchedCharInserted> _insertedMatchedChars;
};
