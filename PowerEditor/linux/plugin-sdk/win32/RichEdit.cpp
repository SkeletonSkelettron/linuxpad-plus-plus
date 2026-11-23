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

// Notepad++ Linux plugin SDK, npp_win32 library: the messages of rich edit controls on a
// QPlainTextEdit. Positions are the ones of RichEdit 2.0 and later: one character per line end.

#include <QApplication>
#include <QClipboard>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPalette>
#include <QPlainTextEdit>
#include <QScrollBar>
#include <QTextBlock>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QWheelEvent>

#include "W32Internal.h"
#include <richedit.h>

using namespace w32;

namespace
{
	constexpr LRESULT kNotHandled = -0x7FFFFFFF;

	int textLength(QPlainTextEdit* e)
	{
		return e->document()->characterCount() - 1; // without the last paragraph separator
	}

	// the cursor of [cpMin, cpMax): cpMax -1 is the end, cpMin -1 nothing
	QTextCursor rangeCursor(QPlainTextEdit* e, LONG cpMin, LONG cpMax)
	{
		QTextCursor c(e->document());
		const int len = textLength(e);
		const int start = std::clamp(static_cast<int>(cpMin), 0, len);
		const int end = (cpMax < 0) ? len : std::clamp(static_cast<int>(cpMax), 0, len);
		c.setPosition(std::min(start, end));
		c.setPosition(std::max(start, end), QTextCursor::KeepAnchor);
		return c;
	}

	// the text of RichEdit: CR between the paragraphs (CR LF with useCrLf)
	QString richText(const QString& plain, bool useCrLf)
	{
		QString t = plain;
		t.replace(QChar::ParagraphSeparator, QLatin1Char('\n'));
		t.replace(QChar::LineSeparator, QLatin1Char('\n'));
		return t.replace(QLatin1Char('\n'), useCrLf ? QStringLiteral("\r\n") : QStringLiteral("\r"));
	}

	QString plainFromWindows(QString t)
	{
		return t.replace(QStringLiteral("\r\n"), QStringLiteral("\n")).replace(QLatin1Char('\r'), QLatin1Char('\n'));
	}

	QTextCharFormat toFormat(QPlainTextEdit* e, const CHARFORMATW* cf)
	{
		QTextCharFormat f;
		if (!cf)
			return f;
		const DWORD mask = cf->dwMask;
		const DWORD effects = cf->dwEffects;
		if (mask & CFM_COLOR)
		{
			if (effects & CFE_AUTOCOLOR)
				f.setForeground(e->palette().color(QPalette::Text));
			else
				f.setForeground(color(cf->crTextColor));
		}
		if (mask & CFM_BOLD)
			f.setFontWeight((effects & CFE_BOLD) ? QFont::Bold : QFont::Normal);
		if (mask & CFM_ITALIC)
			f.setFontItalic((effects & CFE_ITALIC) != 0);
		if (mask & CFM_UNDERLINE)
			f.setFontUnderline((effects & CFE_UNDERLINE) != 0);
		if (mask & CFM_STRIKEOUT)
			f.setFontStrikeOut((effects & CFE_STRIKEOUT) != 0);
		if ((mask & CFM_FACE) && cf->szFaceName[0])
			f.setFontFamilies({qs(cf->szFaceName)});
		if ((mask & CFM_SIZE) && cf->yHeight > 0)
			f.setFontPointSize(cf->yHeight / 20.0); // twips
		if (cf->cbSize >= sizeof(CHARFORMAT2W))
		{
			const auto* cf2 = static_cast<const CHARFORMAT2W*>(cf);
			if (mask & CFM_BACKCOLOR)
			{
				if (effects & CFE_AUTOBACKCOLOR)
					f.setBackground(Qt::NoBrush);
				else
					f.setBackground(color(cf2->crBackColor));
			}
			if ((mask & CFM_WEIGHT) && cf2->wWeight)
				f.setFontWeight(static_cast<int>(cf2->wWeight));
		}
		return f;
	}

	void fromFormat(QPlainTextEdit* e, const QTextCharFormat& f, CHARFORMATW* cf)
	{
		const UINT size = cf->cbSize;
		const QFont font = f.font();
		cf->dwMask = CFM_ALL;
		cf->dwEffects = 0;
		if (font.bold())
			cf->dwEffects |= CFE_BOLD;
		if (font.italic())
			cf->dwEffects |= CFE_ITALIC;
		if (font.underline())
			cf->dwEffects |= CFE_UNDERLINE;
		if (font.strikeOut())
			cf->dwEffects |= CFE_STRIKEOUT;
		if (f.hasProperty(QTextFormat::ForegroundBrush))
			cf->crTextColor = colorRef(f.foreground().color());
		else
		{
			cf->crTextColor = colorRef(e->palette().color(QPalette::Text));
			cf->dwEffects |= CFE_AUTOCOLOR;
		}
		const qreal points = font.pointSizeF() > 0 ? font.pointSizeF() : e->font().pointSizeF();
		cf->yHeight = static_cast<LONG>(points * 20);
		copyOut(font.family().isEmpty() ? e->font().family() : font.family(), cf->szFaceName, LF_FACESIZE);
		if (size >= sizeof(CHARFORMAT2W))
		{
			auto* cf2 = static_cast<CHARFORMAT2W*>(cf);
			cf2->dwMask |= CFM_BACKCOLOR | CFM_WEIGHT;
			cf2->wWeight = static_cast<WORD>(font.weight());
			if (f.hasProperty(QTextFormat::BackgroundBrush) && f.background().style() != Qt::NoBrush)
				cf2->crBackColor = colorRef(f.background().color());
			else
			{
				cf2->crBackColor = colorRef(e->palette().color(QPalette::Base));
				cf2->dwEffects |= CFE_AUTOBACKCOLOR;
			}
		}
	}

	LRESULT findText(QPlainTextEdit* e, WPARAM flags, const CHARRANGE& range, LPCWSTR what, CHARRANGE* found)
	{
		if (!what || !*what)
			return -1;
		QTextDocument::FindFlags qf;
		if (flags & FR_MATCHCASE)
			qf |= QTextDocument::FindCaseSensitively;
		if (flags & FR_WHOLEWORD)
			qf |= QTextDocument::FindWholeWords;
		const bool down = (flags & FR_DOWN) != 0;
		const int len = textLength(e);
		int from = std::clamp(static_cast<int>(range.cpMin), 0, len);
		int to = (range.cpMax < 0) ? (down ? len : 0) : std::clamp(static_cast<int>(range.cpMax), 0, len);
		if (!down)
			qf |= QTextDocument::FindBackward;
		const QString needle = plainFromWindows(qs(what));
		const QTextCursor c = e->document()->find(needle, from, qf);
		if (c.isNull())
			return -1;
		const int start = c.selectionStart();
		const int end = c.selectionEnd();
		if (down ? end > std::max(from, to) : start < std::min(from, to))
			return -1;
		if (found)
		{
			found->cpMin = start;
			found->cpMax = end;
		}
		return start;
	}

	LRESULT copyText(const QString& text, UINT codepage, void* buffer, DWORD bytes)
	{
		if (!buffer || !bytes)
			return 0;
		if (codepage == 1200 || codepage == 0xFFFFFFFF) // Unicode (wchar_t)
			return copyOut(text, static_cast<wchar_t*>(buffer), static_cast<int>(bytes / sizeof(wchar_t)));
		const std::wstring w = ws(text);
		const int n = WideCharToMultiByte(codepage, 0, w.c_str(), -1, static_cast<char*>(buffer), static_cast<int>(bytes), nullptr, nullptr);
		if (n <= 0)
		{
			static_cast<char*>(buffer)[bytes - 1] = '\0';
			return static_cast<LRESULT>(strlen(static_cast<char*>(buffer)));
		}
		return n - 1;
	}
}

namespace w32
{
	bool isRichEdit(QWidget* w)
	{
		return w && w->property("nppRichEdit").toBool();
	}

	LRESULT richEditProc(Window* w, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		auto* e = qobject_cast<QPlainTextEdit*>(w->widget);
		if (!e)
			return kNotHandled;
		switch (msg)
		{
			case EM_EXGETSEL:
				if (auto* r = reinterpret_cast<CHARRANGE*>(lParam))
				{
					const QTextCursor c = e->textCursor();
					r->cpMin = c.selectionStart();
					r->cpMax = c.selectionEnd();
				}
				return 0;
			case EM_EXSETSEL:
			{
				const auto* r = reinterpret_cast<const CHARRANGE*>(lParam);
				if (!r)
					return 0;
				QTextCursor c = e->textCursor();
				if (r->cpMin == -1)
				{
					c.clearSelection(); // the selection goes, the caret stays
					e->setTextCursor(c);
					return c.position();
				}
				// cpMax first: the caret is at cpMax, like RichEdit
				QTextCursor range = rangeCursor(e, r->cpMin, r->cpMax);
				const int anchor = std::min(static_cast<int>(r->cpMin), textLength(e));
				const int caret = range.position() == anchor ? range.anchor() : range.position();
				c.setPosition(std::clamp(anchor, 0, textLength(e)));
				c.setPosition(caret, QTextCursor::KeepAnchor);
				e->setTextCursor(c);
				return caret;
			}
			case EM_EXLINEFROMCHAR:
			{
				const int pos = static_cast<int>(lParam) < 0 ? e->textCursor().position() : static_cast<int>(lParam);
				return e->document()->findBlock(std::clamp(pos, 0, textLength(e))).blockNumber();
			}
			case EM_SETCHARFORMAT:
			{
				const auto* cf = reinterpret_cast<const CHARFORMATW*>(lParam);
				if (!cf)
					return FALSE;
				const QTextCharFormat f = toFormat(e, cf);
				if (wParam & SCF_ALL)
				{
					QTextCursor all(e->document());
					all.select(QTextCursor::Document);
					all.mergeCharFormat(f);
					e->mergeCurrentCharFormat(f);
				}
				else if (wParam & (SCF_SELECTION | SCF_WORD))
				{
					QTextCursor c = e->textCursor();
					if ((wParam & SCF_WORD) && !c.hasSelection())
						c.select(QTextCursor::WordUnderCursor);
					if (c.hasSelection())
						c.mergeCharFormat(f);
					else
						e->mergeCurrentCharFormat(f); // the insertion point: the text typed or inserted next
				}
				else
				{
					// the default format: the font and the color of the text without its own format
					if (cf->dwMask & (CFM_FACE | CFM_SIZE | CFM_BOLD | CFM_ITALIC))
					{
						QFont font = e->font();
						if ((cf->dwMask & CFM_FACE) && cf->szFaceName[0])
							font.setFamily(qs(cf->szFaceName));
						if ((cf->dwMask & CFM_SIZE) && cf->yHeight > 0)
							font.setPointSizeF(cf->yHeight / 20.0);
						if (cf->dwMask & CFM_BOLD)
							font.setBold((cf->dwEffects & CFE_BOLD) != 0);
						if (cf->dwMask & CFM_ITALIC)
							font.setItalic((cf->dwEffects & CFE_ITALIC) != 0);
						e->setFont(font);
						e->document()->setDefaultFont(font);
					}
					if ((cf->dwMask & CFM_COLOR) && !(cf->dwEffects & CFE_AUTOCOLOR))
					{
						QPalette p = e->palette();
						p.setColor(QPalette::Text, color(cf->crTextColor));
						e->setPalette(p);
					}
				}
				return TRUE;
			}
			case EM_GETCHARFORMAT:
			{
				auto* cf = reinterpret_cast<CHARFORMATW*>(lParam);
				if (!cf)
					return 0;
				const QTextCharFormat f = (wParam == SCF_DEFAULT) ? QTextCharFormat() : e->textCursor().charFormat();
				fromFormat(e, f, cf);
				return static_cast<LRESULT>(cf->dwMask);
			}
			case EM_SETBKGNDCOLOR:
			{
				QPalette p = e->palette();
				const COLORREF old = colorRef(p.color(QPalette::Base));
				if (wParam)
					p.setColor(QPalette::Base, QApplication::palette().color(QPalette::Base));
				else
					p.setColor(QPalette::Base, color(static_cast<COLORREF>(lParam)));
				e->setPalette(p);
				return old;
			}
			case EM_SETEVENTMASK:
			{
				const DWORD old = w->eventMask;
				w->eventMask = static_cast<DWORD>(lParam);
				return old;
			}
			case EM_GETEVENTMASK:
				return w->eventMask;
			case EM_GETTEXTRANGE:
			{
				auto* tr = reinterpret_cast<TEXTRANGEW*>(lParam);
				if (!tr || !tr->lpstrText)
					return 0;
				const QString t = richText(rangeCursor(e, tr->chrg.cpMin, tr->chrg.cpMax).selectedText(), false);
				return copyOut(t, tr->lpstrText, static_cast<int>(t.size()) + 1);
			}
			case EM_GETSELTEXT:
			{
				const QString t = richText(e->textCursor().selectedText(), false);
				if (lParam)
					copyOut(t, reinterpret_cast<LPWSTR>(lParam), static_cast<int>(t.size()) + 1);
				return static_cast<LRESULT>(t.size());
			}
			case EM_FINDTEXT:
			case EM_FINDTEXTW:
			{
				const auto* ft = reinterpret_cast<const FINDTEXTW*>(lParam);
				return ft ? findText(e, wParam, ft->chrg, ft->lpstrText, nullptr) : -1;
			}
			case EM_FINDTEXTEX:
			case EM_FINDTEXTEXW:
			{
				auto* ft = reinterpret_cast<FINDTEXTEXW*>(lParam);
				return ft ? findText(e, wParam, ft->chrg, ft->lpstrText, &ft->chrgText) : -1;
			}
			case EM_GETTEXTLENGTHEX:
			{
				const auto* gtl = reinterpret_cast<const GETTEXTLENGTHEX*>(lParam);
				const DWORD flags = gtl ? gtl->flags : GTL_DEFAULT;
				const QString t = richText(e->toPlainText(), (flags & GTL_USECRLF) != 0);
				if ((flags & GTL_NUMBYTES) && gtl && gtl->codepage != 1200)
					return static_cast<LRESULT>(t.toUtf8().size());
				if ((flags & GTL_NUMBYTES) && gtl)
					return static_cast<LRESULT>(t.size() * sizeof(wchar_t));
				return static_cast<LRESULT>(t.size());
			}
			case EM_GETTEXTEX:
			{
				const auto* gt = reinterpret_cast<const GETTEXTEX*>(wParam);
				if (!gt)
					return 0;
				const QString source = (gt->flags & GT_SELECTION) ? e->textCursor().selectedText() : e->toPlainText();
				return copyText(richText(source, (gt->flags & GT_USECRLF) != 0), gt->codepage, reinterpret_cast<void*>(lParam), gt->cb);
			}
			case EM_SETTEXTEX:
			{
				const auto* st = reinterpret_cast<const SETTEXTEX*>(wParam);
				QString t;
				if (lParam)
					t = (st && st->codepage != 1200 && st->codepage != 0xFFFFFFFF) ? qsA(reinterpret_cast<LPCSTR>(lParam)) : qs(reinterpret_cast<LPCWSTR>(lParam));
				t = plainFromWindows(t);
				if (st && (st->flags & ST_SELECTION))
					e->insertPlainText(t);
				else
					e->setPlainText(t);
				return 1;
			}
			case EM_HIDESELECTION:
				return 0;
			case EM_SETTARGETDEVICE:
				e->setLineWrapMode(lParam == 0 ? QPlainTextEdit::WidgetWidth : QPlainTextEdit::NoWrap);
				return TRUE;
			case EM_SETOPTIONS:
			{
				DWORD options = w->richOptions | (e->isReadOnly() ? ECO_READONLY : 0);
				const DWORD value = static_cast<DWORD>(lParam);
				switch (wParam)
				{
					case ECOOP_SET: options = value; break;
					case ECOOP_OR: options |= value; break;
					case ECOOP_AND: options &= value; break;
					case ECOOP_XOR: options ^= value; break;
					default: break;
				}
				e->setReadOnly((options & ECO_READONLY) != 0);
				w->richOptions = options & ~static_cast<DWORD>(ECO_READONLY);
				return options;
			}
			case EM_GETOPTIONS:
				return w->richOptions | (e->isReadOnly() ? ECO_READONLY : 0);
			case EM_SETUNDOLIMIT:
				e->setUndoRedoEnabled(lParam != 0);
				return lParam;
			case EM_REDO:
				e->redo();
				return TRUE;
			case EM_CANREDO:
				return e->document()->isRedoAvailable() ? TRUE : FALSE;
			case EM_CANPASTE:
				return QApplication::clipboard()->text().isEmpty() ? FALSE : TRUE;
			case EM_SELECTIONTYPE:
			{
				const int n = static_cast<int>(e->textCursor().selectedText().size());
				return n == 0 ? SEL_EMPTY : (SEL_TEXT | (n > 1 ? SEL_MULTICHAR : 0));
			}
			case EM_POSFROMCHAR:
			{
				// RichEdit: wParam the POINTL to fill, lParam the character
				auto* pt = reinterpret_cast<POINTL*>(wParam);
				QTextCursor c(e->document());
				c.setPosition(std::clamp(static_cast<int>(lParam), 0, textLength(e)));
				const QRect r = e->cursorRect(c);
				const QPoint p = e->viewport()->mapTo(e, r.topLeft());
				if (pt)
				{
					pt->x = p.x();
					pt->y = p.y();
				}
				return 0;
			}
			case EM_CHARFROMPOS:
			{
				const auto* pt = reinterpret_cast<const POINTL*>(lParam);
				if (!pt)
					return 0;
				const QPoint p = e->viewport()->mapFrom(e, QPoint(static_cast<int>(pt->x), static_cast<int>(pt->y)));
				return e->cursorForPosition(p).position();
			}
			case EM_GETSCROLLPOS:
				if (auto* pt = reinterpret_cast<POINT*>(lParam))
				{
					pt->x = e->horizontalScrollBar()->value();
					pt->y = e->verticalScrollBar()->value() * e->fontMetrics().lineSpacing();
				}
				return 1;
			case EM_SETSCROLLPOS:
				if (const auto* pt = reinterpret_cast<const POINT*>(lParam))
				{
					e->horizontalScrollBar()->setValue(pt->x);
					e->verticalScrollBar()->setValue(pt->y / std::max(1, e->fontMetrics().lineSpacing()));
				}
				return 1;
			case EM_EXLIMITTEXT:
			case EM_STOPGROUPTYPING:
			case EM_SETLANGOPTIONS:
			case EM_GETLANGOPTIONS:
			case EM_SETEDITSTYLE:
			case EM_GETEDITSTYLE:
			case EM_AUTOURLDETECT:
			case EM_GETAUTOURLDETECT:
			case EM_SETTEXTMODE:
			case EM_SETWORDWRAPMODE:
				return 0;
			case EM_GETTEXTMODE:
				return 1; // TM_PLAINTEXT
			case EM_SETZOOM:
			case EM_SETFONTSIZE:
			case EM_SETPARAFORMAT:
			case EM_REQUESTRESIZE:
				return TRUE;
			case EM_GETZOOM:
				if (wParam)
					*reinterpret_cast<int*>(wParam) = 0;
				if (lParam)
					*reinterpret_cast<int*>(lParam) = 0;
				return TRUE;
			case EM_GETPARAFORMAT:
				return 0;
			case EM_STREAMIN:
			{
				auto* es = reinterpret_cast<EDITSTREAM*>(lParam);
				if (!es || !es->pfnCallback)
					return 0;
				QByteArray data;
				BYTE chunk[4096];
				LONG got = 0;
				while (es->pfnCallback(es->dwCookie, chunk, sizeof(chunk), &got) == 0 && got > 0)
					data.append(reinterpret_cast<const char*>(chunk), got);
				QString t = (wParam & SF_UNICODE)
					? QString::fromWCharArray(reinterpret_cast<const wchar_t*>(data.constData()), static_cast<qsizetype>(data.size() / sizeof(wchar_t)))
					: QString::fromUtf8(data);
				t = plainFromWindows(t);
				if (wParam & SFF_SELECTION)
					e->insertPlainText(t);
				else
					e->setPlainText(t);
				return static_cast<LRESULT>(t.size());
			}
			case EM_STREAMOUT:
			{
				auto* es = reinterpret_cast<EDITSTREAM*>(lParam);
				if (!es || !es->pfnCallback)
					return 0;
				const QString t = richText((wParam & SFF_SELECTION) ? e->textCursor().selectedText() : e->toPlainText(), true);
				QByteArray data;
				if (wParam & SF_UNICODE)
				{
					const std::wstring w = ws(t);
					data = QByteArray(reinterpret_cast<const char*>(w.data()), static_cast<qsizetype>(w.size() * sizeof(wchar_t)));
				}
				else
					data = t.toUtf8();
				qsizetype done = 0;
				while (done < data.size())
				{
					LONG written = 0;
					const LONG n = static_cast<LONG>(std::min<qsizetype>(data.size() - done, 4096));
					if (es->pfnCallback(es->dwCookie, reinterpret_cast<LPBYTE>(data.data() + done), n, &written) != 0 || written <= 0)
						break;
					done += written;
				}
				return static_cast<LRESULT>(done);
			}
			default:
				return kNotHandled;
		}
	}

	void richEditSelChanged(Window* w)
	{
		auto* e = qobject_cast<QPlainTextEdit*>(w->widget);
		if (!e || !(w->eventMask & ENM_SELCHANGE))
			return;
		SELCHANGE sc{};
		const QTextCursor c = e->textCursor();
		sc.chrg.cpMin = c.selectionStart();
		sc.chrg.cpMax = c.selectionEnd();
		const int n = sc.chrg.cpMax - sc.chrg.cpMin;
		sc.seltyp = static_cast<WORD>(n == 0 ? SEL_EMPTY : (SEL_TEXT | (n > 1 ? SEL_MULTICHAR : 0)));
		notify(w, &sc.nmhdr, EN_SELCHANGE);
	}

	// EN_MSGFILTER: the keys and the mouse of the control go to its parent first (ENM_KEYEVENTS,
	// ENM_MOUSEEVENTS); a nonzero answer eats the event
	bool richEditFilter(Window* w, QObject* obj, QEvent* ev)
	{
		auto* e = qobject_cast<QPlainTextEdit*>(w->widget);
		if (!e || !isRichEdit(e))
			return false;
		auto filter = [w](UINT msg, WPARAM wParam, LPARAM lParam)
		{
			MSGFILTER mf{};
			mf.msg = msg;
			mf.wParam = wParam;
			mf.lParam = lParam;
			return notify(w, &mf.nmhdr, EN_MSGFILTER) != 0;
		};
		switch (ev->type())
		{
			case QEvent::KeyPress:
			case QEvent::KeyRelease:
			{
				if (!(w->eventMask & ENM_KEYEVENTS))
					return false;
				auto* k = static_cast<QKeyEvent*>(ev);
				const int vk = vkFromQt(k->key(), k->nativeVirtualKey());
				const bool alt = (k->modifiers() & Qt::AltModifier) != 0;
				const bool extended = (k->modifiers() & Qt::KeypadModifier) != 0;
				if (ev->type() == QEvent::KeyRelease)
					return vk && filter(alt ? WM_SYSKEYUP : WM_KEYUP, static_cast<WPARAM>(vk), keyLParam(1, true, extended));
				if (vk && filter(alt ? WM_SYSKEYDOWN : WM_KEYDOWN, static_cast<WPARAM>(vk), keyLParam(k->isAutoRepeat() ? 2 : 1, false, extended)))
					return true;
				const QString text = k->text();
				for (QChar ch : text)
				{
					if (ch.unicode() >= 0x20 || ch == QLatin1Char('\r') || ch == QLatin1Char('\t') || ch == QLatin1Char('\b'))
					{
						if (filter(alt ? WM_SYSCHAR : WM_CHAR, ch.unicode(), keyLParam(1, false, false)))
							return true;
					}
				}
				return false;
			}
			case QEvent::MouseButtonPress:
			case QEvent::MouseButtonRelease:
			case QEvent::MouseButtonDblClick:
			case QEvent::MouseMove:
			{
				if (!(w->eventMask & ENM_MOUSEEVENTS))
					return false;
				auto* m = static_cast<QMouseEvent*>(ev);
				QWidget* source = qobject_cast<QWidget*>(obj);
				const QPoint p = source ? source->mapTo(e, m->position().toPoint()) : m->position().toPoint();
				const Qt::MouseButton b = m->button();
				UINT msg = WM_MOUSEMOVE;
				if (ev->type() != QEvent::MouseMove)
				{
					const int i = (b == Qt::RightButton) ? 1 : (b == Qt::MiddleButton) ? 2 : 0;
					static const UINT down[] = {WM_LBUTTONDOWN, WM_RBUTTONDOWN, WM_MBUTTONDOWN};
					static const UINT up[] = {WM_LBUTTONUP, WM_RBUTTONUP, WM_MBUTTONUP};
					static const UINT dbl[] = {WM_LBUTTONDBLCLK, WM_RBUTTONDBLCLK, WM_MBUTTONDBLCLK};
					msg = (ev->type() == QEvent::MouseButtonPress) ? down[i] : (ev->type() == QEvent::MouseButtonRelease) ? up[i] : dbl[i];
				}
				WPARAM keys = 0;
				if (m->buttons() & Qt::LeftButton)
					keys |= MK_LBUTTON;
				if (m->buttons() & Qt::RightButton)
					keys |= MK_RBUTTON;
				if (m->modifiers() & Qt::ShiftModifier)
					keys |= MK_SHIFT;
				if (m->modifiers() & Qt::ControlModifier)
					keys |= MK_CONTROL;
				return filter(msg, keys, MAKELPARAM(static_cast<short>(p.x()), static_cast<short>(p.y())));
			}
			case QEvent::Wheel:
			{
				if (!(w->eventMask & ENM_MOUSEEVENTS))
					return false;
				auto* wh = static_cast<QWheelEvent*>(ev);
				const QPoint g = wh->globalPosition().toPoint();
				return filter(WM_MOUSEWHEEL, MAKEWPARAM(0, static_cast<short>(wh->angleDelta().y())), MAKELPARAM(static_cast<short>(g.x()), static_cast<short>(g.y())));
			}
			default:
				return false;
		}
	}
}
