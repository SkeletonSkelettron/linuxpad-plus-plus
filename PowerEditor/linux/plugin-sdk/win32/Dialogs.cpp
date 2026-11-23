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

// Notepad++ Linux plugin SDK, npp_win32 library: dialogs of .rc templates and the
// standard controls (button, edit, static, combo box, list box, scroll bar).

#include <QAbstractItemView>
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QEventLoop>
#include <QFontMetrics>
#include <QFrame>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QProgressBar>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollBar>
#include <QSlider>
#include <QSpinBox>
#include <QTextBlock>
#include <QTimer>
#include <QToolTip>

#include "W32Internal.h"
#include <richedit.h>

namespace w32
{
	LRESULT CALLBACK dialogWndProc(HWND h, UINT msg, WPARAM wParam, LPARAM lParam);
	LRESULT CALLBACK controlWndProc(HWND h, UINT msg, WPARAM wParam, LPARAM lParam);
	QWidget* newW32Widget(QWidget* parent, Qt::WindowFlags f);

	Kind kindOfClass(const std::wstring& clsIn)
	{
		std::wstring cls;
		for (wchar_t c : clsIn)
			cls += static_cast<wchar_t>(std::towlower(c));
		if (cls == L"button")
			return Kind::Button;
		if (cls == L"edit" || cls.rfind(L"richedit", 0) == 0 || cls == L"msftedit")
			return Kind::Edit;
		if (cls == L"static")
			return Kind::Static;
		if (cls == L"combobox" || cls == L"comboboxex32")
			return Kind::ComboBox;
		if (cls == L"listbox")
			return Kind::ListBox;
		if (cls == L"scrollbar")
			return Kind::ScrollBar;
		if (cls == L"syslistview32")
			return Kind::ListView;
		if (cls == L"systreeview32")
			return Kind::TreeView;
		if (cls == L"systabcontrol32")
			return Kind::Tab;
		if (cls == L"msctls_trackbar32")
			return Kind::Trackbar;
		if (cls == L"msctls_updown32")
			return Kind::UpDown;
		if (cls == L"msctls_progress32")
			return Kind::Progress;
		if (cls == L"tooltips_class32")
			return Kind::Tooltip;
		if (cls == L"syslink")
			return Kind::Link;
		if (cls == L"sysheader32")
			return Kind::Header;
		if (cls == L"msctls_statusbar32")
			return Kind::StatusBar;
		if (cls == L"msctls_hotkey32")
			return Kind::HotKey;
		if (cls == L"toolbarwindow32")
			return Kind::Toolbar;
		if (cls == L"rebarwindow32")
			return Kind::Rebar;
		if (cls == L"scintilla")
			return Kind::Scintilla;
		return Kind::Generic;
	}

	namespace
	{
		DWORD buttonType(DWORD style)
		{
			return style & BS_TYPEMASK;
		}

		QString stripAmpersandFor(const QString& t, bool keepMnemonic)
		{
			// Qt uses & for mnemonics too; "&&" is a literal '&' in both
			return keepMnemonic ? t : QString(t).replace(QStringLiteral("&&"), QStringLiteral("\x01")).remove(QLatin1Char('&')).replace(QChar(1), QLatin1Char('&'));
		}

		QString labelText(const QString& t, DWORD style)
		{
			if (style & SS_NOPREFIX)
				return QString(t).replace(QLatin1Char('&'), QStringLiteral("&&"));
			return t;
		}

		// Static controls: QLabel shows "&x" as is, so the mnemonic goes
		QString staticText(const QString& t, DWORD style)
		{
			if (style & SS_NOPREFIX)
				return t;
			return stripAmpersandFor(t, false);
		}

		class GroupFrame : public QGroupBox
		{
		public:
			using QGroupBox::QGroupBox;

		protected:
			void focusInEvent(QFocusEvent*) override {}
		};
	}

	QWidget* createControl(Kind kind, const std::wstring& cls, DWORD style, QWidget* parent, const QString& text, int textId, Window** out)
	{
		QWidget* widget = nullptr;
		switch (kind)
		{
			case Kind::Button:
			{
				switch (buttonType(style))
				{
					case BS_CHECKBOX:
					case BS_AUTOCHECKBOX:
					case BS_3STATE:
					case BS_AUTO3STATE:
					{
						auto* b = new QCheckBox(text, parent);
						b->setTristate(buttonType(style) == BS_3STATE || buttonType(style) == BS_AUTO3STATE);
						if (style & BS_LEFTTEXT)
							b->setLayoutDirection(Qt::RightToLeft);
						widget = b;
						break;
					}
					case BS_RADIOBUTTON:
					case BS_AUTORADIOBUTTON:
					{
						auto* b = new QRadioButton(text, parent);
						b->setAutoExclusive(false); // groups of WS_GROUP: see groupRadios()
						if (style & BS_LEFTTEXT)
							b->setLayoutDirection(Qt::RightToLeft);
						widget = b;
						break;
					}
					case BS_GROUPBOX:
					{
						auto* g = new GroupFrame(stripAmpersandFor(text, false), parent);
						g->setFocusPolicy(Qt::NoFocus);
						g->setAttribute(Qt::WA_TransparentForMouseEvents);
						widget = g;
						break;
					}
					default:
					{
						auto* b = new QPushButton(text, parent);
						b->setAutoDefault(false);
						if (buttonType(style) == BS_DEFPUSHBUTTON)
							b->setDefault(true);
						if (style & BS_PUSHLIKE)
							b->setCheckable(true);
						if (style & BS_FLAT)
							b->setFlat(true);
						widget = b;
						break;
					}
				}
				break;
			}
			case Kind::Edit:
			{
				const bool rich = !cls.empty() && (std::towlower(cls[0]) == L'r' || std::towlower(cls[0]) == L'm');
				if ((style & ES_MULTILINE) || rich)
				{
					auto* e = new QPlainTextEdit(parent);
					if (rich)
						e->setProperty("nppRichEdit", true); // positions with one character per line end
					e->setPlainText(text);
					e->setReadOnly(style & ES_READONLY);
					e->setLineWrapMode((style & (WS_HSCROLL | ES_AUTOHSCROLL)) ? QPlainTextEdit::NoWrap : QPlainTextEdit::WidgetWidth);
					e->setVerticalScrollBarPolicy((style & WS_VSCROLL) ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
					e->setHorizontalScrollBarPolicy((style & WS_HSCROLL) ? Qt::ScrollBarAsNeeded : Qt::ScrollBarAlwaysOff);
					e->setTabChangesFocus(!(style & ES_WANTRETURN));
					widget = e;
				}
				else
				{
					auto* e = new QLineEdit(text, parent);
					e->setReadOnly(style & ES_READONLY);
					if (style & ES_PASSWORD)
						e->setEchoMode(QLineEdit::Password);
					if (style & ES_CENTER)
						e->setAlignment(Qt::AlignCenter);
					else if (style & ES_RIGHT)
						e->setAlignment(Qt::AlignRight);
					if (!(style & WS_BORDER))
						e->setFrame(true);
					widget = e;
				}
				break;
			}
			case Kind::Static:
			{
				auto* l = new QLabel(parent);
				const DWORD type = style & SS_TYPEMASK;
				switch (type)
				{
					case SS_ICON:
					case SS_BITMAP:
						if (textId >= 0)
						{
							const QPixmap px = pixmapOf(type == SS_ICON ? LoadImageW(moduleHandle(), MAKEINTRESOURCEW(textId), IMAGE_ICON, 32, 32, 0)
								: LoadImageW(moduleHandle(), MAKEINTRESOURCEW(textId), IMAGE_BITMAP, 0, 0, 0));
							l->setPixmap(px);
						}
						else if (!text.isEmpty())
						{
							const std::wstring name = ws(text);
							l->setPixmap(pixmapOf(LoadImageW(moduleHandle(), name.c_str(), type == SS_ICON ? IMAGE_ICON : IMAGE_BITMAP, 32, 32, 0)));
						}
						if (style & SS_CENTERIMAGE)
							l->setAlignment(Qt::AlignCenter);
						break;
					case SS_ETCHEDHORZ:
						l->setFrameStyle(QFrame::HLine | QFrame::Sunken);
						break;
					case SS_ETCHEDVERT:
						l->setFrameStyle(QFrame::VLine | QFrame::Sunken);
						break;
					case SS_ETCHEDFRAME:
					case SS_BLACKFRAME:
					case SS_GRAYFRAME:
					case SS_WHITEFRAME:
						l->setFrameStyle(QFrame::Box | QFrame::Sunken);
						break;
					case SS_BLACKRECT:
					case SS_GRAYRECT:
					case SS_WHITERECT:
						l->setAutoFillBackground(true);
						break;
					default:
						l->setText(staticText(text, style));
						l->setWordWrap(type != SS_LEFTNOWORDWRAP && type != SS_SIMPLE && !(style & SS_ELLIPSISMASK));
						if (type == SS_CENTER)
							l->setAlignment(Qt::AlignHCenter | Qt::AlignTop);
						else if (type == SS_RIGHT)
							l->setAlignment(Qt::AlignRight | Qt::AlignTop);
						else
							l->setAlignment(Qt::AlignLeft | Qt::AlignTop);
						if (style & SS_CENTERIMAGE)
							l->setAlignment((l->alignment() & Qt::AlignHorizontal_Mask) | Qt::AlignVCenter);
						break;
				}
				if (style & SS_SUNKEN)
					l->setFrameStyle(QFrame::Panel | QFrame::Sunken);
				if (style & WS_BORDER)
					l->setFrameStyle(QFrame::Box | QFrame::Plain);
				l->setTextInteractionFlags(Qt::NoTextInteraction);
				widget = l;
				break;
			}
			case Kind::ComboBox:
			{
				auto* c = new QComboBox(parent);
				const DWORD type = style & 3;
				c->setEditable(type == CBS_DROPDOWN || type == CBS_SIMPLE);
				c->setInsertPolicy(QComboBox::NoInsert);
				c->setMaxVisibleItems(20);
				widget = c;
				break;
			}
			case Kind::ListBox:
			{
				auto* l = new QListWidget(parent);
				if (style & LBS_EXTENDEDSEL)
					l->setSelectionMode(QAbstractItemView::ExtendedSelection);
				else if (style & LBS_MULTIPLESEL)
					l->setSelectionMode(QAbstractItemView::MultiSelection);
				else if (style & LBS_NOSEL)
					l->setSelectionMode(QAbstractItemView::NoSelection);
				else
					l->setSelectionMode(QAbstractItemView::SingleSelection);
				l->setSortingEnabled(style & LBS_SORT);
				l->setUniformItemSizes(true);
				widget = l;
				break;
			}
			case Kind::ScrollBar:
				widget = new QScrollBar((style & SBS_VERT) ? Qt::Vertical : Qt::Horizontal, parent);
				break;
			case Kind::Scintilla:
			{
				// a Scintilla of the plugin: Notepad++ makes it (NPPM_CREATESCINTILLAHANDLE)
				HWND npp = nppHandle();
				Window* parentWin = winOf(parent);
				HWND sci = reinterpret_cast<HWND>(SendMessageW(npp, WM_USER + 1000 + 20 /* NPPM_CREATESCINTILLAHANDLE */, 0,
					reinterpret_cast<LPARAM>(parentWin ? hwnd(parentWin) : nullptr)));
				widget = widgetOf(sci);
				if (!widget)
					return nullptr;
				// keep the host's handle for SendMessage: the window record forwards to it
				Window* w = newWindow(kind, widget, nullptr, 0, style, 0, cls);
				w->wndProc = controlWndProc;
				w->classProc = controlWndProc;
				w->ctl = sci;
				*out = w;
				return widget;
			}
			default:
			{
				if (isCommonControl(kind))
				{
					Window* w = newWindow(kind, nullptr, nullptr, 0, style, 0, cls);
					w->wndProc = controlWndProc;
					w->classProc = controlWndProc;
					w->widget = nullptr;
					// the common controls make their widget in setupCommonControl()
					w->handle.widget = parent; // temporary parent
					setupCommonControl(w);
					if (!w->widget)
						return nullptr;
					w->widget->setProperty("_w32Window", QVariant::fromValue(static_cast<void*>(w)));
					w->widget->setProperty("_nppHandle", QVariant::fromValue(static_cast<void*>(&w->handle)));
					w->handle.widget = w->widget;
					QObject::connect(w->widget, &QObject::destroyed, [w]
					{
						w->widget = nullptr;
						w->handle.widget = nullptr;
						w->destroyed = true;
					});
					installControlFilter(w);
					// the text of the template or of CreateWindowEx (the markup of a SysLink)
					if (kind == Kind::Link && !text.isEmpty())
					{
						const std::wstring t = ws(text);
						commonControlProc(w, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(t.c_str()));
					}
					*out = w;
					return w->widget;
				}
				// a class nobody knows: an empty child window
				widget = newW32Widget(parent, Qt::Widget);
				Window* w = newWindow(Kind::Generic, widget, nullptr, 0, style, 0, cls);
				*out = w;
				return widget;
			}
		}
		Window* w = newWindow(kind, widget, nullptr, 0, style, 0, cls);
		w->wndProc = controlWndProc;
		w->classProc = controlWndProc;
		*out = w;
		installControlFilter(w);
		if (auto* combo = qobject_cast<QComboBox*>(widget); combo && combo->lineEdit())
			installControlFilter(w, combo->lineEdit());

		// notifications of the controls
		switch (kind)
		{
			case Kind::Button:
				if (auto* ab = qobject_cast<QAbstractButton*>(widget))
				{
					QObject::connect(ab, &QAbstractButton::clicked, ab, [w, ab]
					{
						const DWORD type = buttonType(w->style);
						if (type == BS_CHECKBOX || type == BS_3STATE || type == BS_RADIOBUTTON)
						{
							// not automatic: the state changes only with BM_SETCHECK
							auto* cb = qobject_cast<QCheckBox*>(ab);
							if (cb)
								cb->setCheckState(w->checkState == BST_CHECKED ? Qt::Checked : (w->checkState == BST_INDETERMINATE ? Qt::PartiallyChecked : Qt::Unchecked));
							else
								ab->setChecked(w->checkState == BST_CHECKED);
						}
						else if (type == BS_AUTORADIOBUTTON)
						{
							// the other radio buttons of the group
							ab->setChecked(true);
							Window* dlg = winOf(ab->parentWidget());
							if (dlg && dlg->widget)
							{
								QList<QWidget*> order;
								for (QObject* o : dlg->widget->children())
								{
									if (auto* x = qobject_cast<QWidget*>(o))
										order << x;
								}
								const int me = static_cast<int>(order.indexOf(ab));
								auto isRadio = [](QWidget* x)
								{
									Window* xw = winOf(x);
									return xw && xw->widget == x && xw->kind == Kind::Button
										&& (buttonType(xw->style) == BS_AUTORADIOBUTTON || buttonType(xw->style) == BS_RADIOBUTTON);
								};
								// back to the start of the group (WS_GROUP), then forward to the next group
								int start = me;
								while (start > 0)
								{
									Window* sw = winOf(order[start]);
									if (sw && (sw->style & WS_GROUP))
										break;
									if (!isRadio(order[start - 1]))
										break;
									--start;
								}
								for (int i = start; i < order.size(); ++i)
								{
									if (i > start)
									{
										Window* xw = winOf(order[i]);
										if (!isRadio(order[i]) || (xw && (xw->style & WS_GROUP)))
											break;
									}
									if (order[i] != ab)
										if (auto* rb = qobject_cast<QAbstractButton*>(order[i]))
											rb->setChecked(false);
								}
							}
						}
						notifyCommand(w, BN_CLICKED);
					});
				}
				break;
			case Kind::Edit:
				if (auto* le = qobject_cast<QLineEdit*>(widget))
				{
					QObject::connect(le, &QLineEdit::textChanged, le, [w] { notifyCommand(w, EN_UPDATE); notifyCommand(w, EN_CHANGE); });
				}
				else if (auto* pe = qobject_cast<QPlainTextEdit*>(widget); pe && isRichEdit(pe))
				{
					// a rich edit notifies what its event mask asks for
					QObject::connect(pe, &QPlainTextEdit::textChanged, pe, [w]
					{
						if (w->eventMask & ENM_UPDATE)
							notifyCommand(w, EN_UPDATE);
						if (w->eventMask & ENM_CHANGE)
							notifyCommand(w, EN_CHANGE);
					});
					QObject::connect(pe, &QPlainTextEdit::cursorPositionChanged, pe, [w] { richEditSelChanged(w); });
					QObject::connect(pe, &QPlainTextEdit::selectionChanged, pe, [w] { richEditSelChanged(w); });
				}
				else if (auto* pe = qobject_cast<QPlainTextEdit*>(widget))
				{
					QObject::connect(pe, &QPlainTextEdit::textChanged, pe, [w] { notifyCommand(w, EN_UPDATE); notifyCommand(w, EN_CHANGE); });
				}
				break;
			case Kind::ComboBox:
				if (auto* c = qobject_cast<QComboBox*>(widget))
				{
					QObject::connect(c, qOverload<int>(&QComboBox::activated), c, [w]
					{
						notifyCommand(w, CBN_SELCHANGE);
						notifyCommand(w, CBN_SELENDOK);
					});
					if (QLineEdit* le = c->lineEdit())
					{
						QObject::connect(le, &QLineEdit::textEdited, le, [w]
						{
							notifyCommand(w, CBN_EDITUPDATE);
							notifyCommand(w, CBN_EDITCHANGE);
						});
					}
				}
				break;
			case Kind::ListBox:
				if (auto* l = qobject_cast<QListWidget*>(widget))
				{
					QObject::connect(l, &QListWidget::itemSelectionChanged, l, [w]
					{
						if ((w->style & LBS_NOTIFY) && !w->settingText)
							notifyCommand(w, LBN_SELCHANGE);
					});
					QObject::connect(l, &QListWidget::itemDoubleClicked, l, [w]
					{
						if (w->style & LBS_NOTIFY)
							notifyCommand(w, LBN_DBLCLK);
					});
				}
				break;
			case Kind::Static:
				if (w->style & SS_NOTIFY)
				{
					auto* l = static_cast<QLabel*>(widget);
					l->setTextInteractionFlags(Qt::LinksAccessibleByMouse);
					QObject::connect(l, &QLabel::linkActivated, l, [w] { notifyCommand(w, STN_CLICKED); });
				}
				break;
			case Kind::ScrollBar:
				if (auto* sb = qobject_cast<QScrollBar*>(widget))
				{
					QObject::connect(sb, &QScrollBar::actionTriggered, sb, [w, sb](int action)
					{
						int code = SB_ENDSCROLL;
						switch (action)
						{
							case QAbstractSlider::SliderSingleStepAdd: code = SB_LINEDOWN; break;
							case QAbstractSlider::SliderSingleStepSub: code = SB_LINEUP; break;
							case QAbstractSlider::SliderPageStepAdd: code = SB_PAGEDOWN; break;
							case QAbstractSlider::SliderPageStepSub: code = SB_PAGEUP; break;
							case QAbstractSlider::SliderToMinimum: code = SB_TOP; break;
							case QAbstractSlider::SliderToMaximum: code = SB_BOTTOM; break;
							case QAbstractSlider::SliderMove: code = SB_THUMBTRACK; break;
							default: break;
						}
						SendMessageW(w->parent, sb->orientation() == Qt::Vertical ? WM_VSCROLL : WM_HSCROLL,
							MAKEWPARAM(code, sb->sliderPosition()), reinterpret_cast<LPARAM>(hwnd(w)));
					});
				}
				break;
			default:
				break;
		}
		return widget;
	}

	// ---- control procedures ----------------------------------------------------------------
	namespace
	{
		QString textOf(QWidget* widget)
		{
			if (auto* le = qobject_cast<QLineEdit*>(widget))
				return le->text();
			if (auto* pe = qobject_cast<QPlainTextEdit*>(widget))
				return pe->toPlainText().replace(QLatin1Char('\n'), QStringLiteral("\r\n"));
			if (auto* ab = qobject_cast<QAbstractButton*>(widget))
				return ab->text();
			if (auto* gb = qobject_cast<QGroupBox*>(widget))
				return gb->title();
			if (auto* l = qobject_cast<QLabel*>(widget))
				return l->text();
			if (auto* c = qobject_cast<QComboBox*>(widget))
				return c->isEditable() ? c->currentText() : (c->currentIndex() >= 0 ? c->currentText() : QString());
			return widget ? widget->windowTitle() : QString();
		}

		void setTextOf(Window* w, const QString& text)
		{
			QWidget* widget = w->widget;
			if (auto* le = qobject_cast<QLineEdit*>(widget))
				le->setText(text);
			else if (auto* pe = qobject_cast<QPlainTextEdit*>(widget))
				pe->setPlainText(QString(text).replace(QStringLiteral("\r\n"), QStringLiteral("\n")).replace(QLatin1Char('\r'), QLatin1Char('\n')));
			else if (auto* ab = qobject_cast<QAbstractButton*>(widget))
				ab->setText(text);
			else if (auto* gb = qobject_cast<QGroupBox*>(widget))
				gb->setTitle(stripAmpersandFor(text, false));
			else if (auto* l = qobject_cast<QLabel*>(widget))
				l->setText(staticText(text, w->style));
			else if (auto* c = qobject_cast<QComboBox*>(widget))
			{
				if (c->isEditable())
					c->setEditText(text);
				else
					c->setCurrentIndex(c->findText(text));
			}
			else if (widget)
				widget->setWindowTitle(text);
		}

		// position of a Windows edit box (CRLF line ends) in a Qt text (LF)
		int qtPos(QPlainTextEdit* e, int winPos)
		{
			if (isRichEdit(e))
				return winPos; // RichEdit: one character per line end, like Qt
			const QString t = e->toPlainText();
			int win = 0;
			for (int i = 0; i < t.size(); ++i)
			{
				if (win >= winPos)
					return i;
				win += (t[i] == QLatin1Char('\n')) ? 2 : 1;
			}
			return static_cast<int>(t.size());
		}

		int winPos(QPlainTextEdit* e, int qtPosition)
		{
			if (isRichEdit(e))
				return qtPosition;
			const QString t = e->toPlainText().left(qtPosition);
			return static_cast<int>(t.size() + t.count(QLatin1Char('\n')));
		}

		LRESULT editProc(Window* w, UINT msg, WPARAM wParam, LPARAM lParam)
		{
			auto* le = qobject_cast<QLineEdit*>(w->widget);
			auto* pe = qobject_cast<QPlainTextEdit*>(w->widget);
			if (pe && isRichEdit(pe))
			{
				const LRESULT r = richEditProc(w, msg, wParam, lParam);
				if (r != -0x7FFFFFFF)
					return r;
			}
			switch (msg)
			{
				case EM_GETSEL:
				{
					int start = 0;
					int end = 0;
					if (le)
					{
						start = le->hasSelectedText() ? le->selectionStart() : le->cursorPosition();
						end = le->hasSelectedText() ? le->selectionEnd() : le->cursorPosition();
					}
					else if (pe)
					{
						const QTextCursor c = pe->textCursor();
						start = winPos(pe, c.selectionStart());
						end = winPos(pe, c.selectionEnd());
					}
					if (wParam)
						*reinterpret_cast<DWORD*>(wParam) = static_cast<DWORD>(start);
					if (lParam)
						*reinterpret_cast<DWORD*>(lParam) = static_cast<DWORD>(end);
					return MAKELRESULT(std::min(start, 0xFFFF), std::min(end, 0xFFFF));
				}
				case EM_SETSEL:
				{
					int start = static_cast<int>(static_cast<intptr_t>(wParam));
					int end = static_cast<int>(lParam);
					if (le)
					{
						const int len = static_cast<int>(le->text().size());
						if (start < 0)
						{
							le->deselect();
							return 0;
						}
						if (end < 0 || end > len)
							end = len;
						start = std::min(start, len);
						le->setSelection(start, end - start);
					}
					else if (pe)
					{
						QTextCursor c = pe->textCursor();
						const int len = static_cast<int>(pe->toPlainText().size());
						if (start < 0)
						{
							c.clearSelection();
							pe->setTextCursor(c);
							return 0;
						}
						const int qs0 = qtPos(pe, start);
						const int qe = end < 0 ? len : qtPos(pe, end);
						c.setPosition(qs0);
						c.setPosition(qe, QTextCursor::KeepAnchor);
						pe->setTextCursor(c);
					}
					return 0;
				}
				case EM_REPLACESEL:
				{
					const QString t = qs(reinterpret_cast<LPCWSTR>(lParam));
					if (le)
						le->insert(t);
					else if (pe)
						pe->insertPlainText(QString(t).replace(QStringLiteral("\r\n"), QStringLiteral("\n")));
					return 0;
				}
				case EM_LIMITTEXT:
					if (le)
						le->setMaxLength(wParam ? static_cast<int>(wParam) : 32767 * 1024);
					return 0;
				case EM_GETLIMITTEXT:
					return le ? le->maxLength() : 0x7FFFFFFE;
				case EM_SETREADONLY:
					if (le)
						le->setReadOnly(wParam != 0);
					else if (pe)
						pe->setReadOnly(wParam != 0);
					w->style = wParam ? (w->style | ES_READONLY) : (w->style & ~ES_READONLY);
					return TRUE;
				case EM_GETLINECOUNT:
					return pe ? pe->document()->blockCount() : 1;
				case EM_LINEINDEX:
				{
					if (!pe)
						return 0;
					int line = static_cast<int>(static_cast<intptr_t>(wParam));
					if (line < 0)
						line = pe->textCursor().blockNumber();
					const QTextBlock b = pe->document()->findBlockByNumber(line);
					return b.isValid() ? winPos(pe, b.position()) : -1;
				}
				case EM_LINELENGTH:
				{
					if (le)
						return le->text().size();
					if (!pe)
						return 0;
					const QTextBlock b = pe->document()->findBlock(qtPos(pe, static_cast<int>(wParam)));
					return b.isValid() ? b.length() - 1 : 0;
				}
				case EM_LINEFROMCHAR:
				{
					if (!pe)
						return 0;
					const int pos = static_cast<intptr_t>(wParam) < 0 ? pe->textCursor().position() : qtPos(pe, static_cast<int>(wParam));
					return pe->document()->findBlock(pos).blockNumber();
				}
				case EM_GETLINE:
				{
					QString line;
					if (le)
						line = le->text();
					else if (pe)
						line = pe->document()->findBlockByNumber(static_cast<int>(wParam)).text();
					auto* buf = reinterpret_cast<wchar_t*>(lParam);
					if (!buf)
						return 0;
					const int max = *reinterpret_cast<WORD*>(buf);
					const std::wstring s = ws(line);
					const int n = std::min(max, static_cast<int>(s.size()));
					std::wmemcpy(buf, s.data(), static_cast<size_t>(n));
					return n;
				}
				case EM_GETFIRSTVISIBLELINE:
					return pe ? pe->cursorForPosition(QPoint(0, 0)).blockNumber() : 0;
				case EM_LINESCROLL:
					if (pe)
						pe->verticalScrollBar()->setValue(pe->verticalScrollBar()->value() + static_cast<int>(lParam));
					return TRUE;
				case EM_SCROLL:
					if (pe)
					{
						QScrollBar* sb = pe->verticalScrollBar();
						switch (wParam)
						{
							case SB_LINEDOWN: sb->triggerAction(QAbstractSlider::SliderSingleStepAdd); break;
							case SB_LINEUP: sb->triggerAction(QAbstractSlider::SliderSingleStepSub); break;
							case SB_PAGEDOWN: sb->triggerAction(QAbstractSlider::SliderPageStepAdd); break;
							case SB_PAGEUP: sb->triggerAction(QAbstractSlider::SliderPageStepSub); break;
							default: break;
						}
					}
					return TRUE;
				case EM_SCROLLCARET:
					if (pe)
						pe->ensureCursorVisible();
					return TRUE;
				case EM_GETMODIFY:
					return le ? le->isModified() : (pe ? pe->document()->isModified() : 0);
				case EM_SETMODIFY:
					if (le)
						le->setModified(wParam != 0);
					else if (pe)
						pe->document()->setModified(wParam != 0);
					return 0;
				case EM_CANUNDO:
					return le ? le->isUndoAvailable() : (pe ? pe->document()->isUndoAvailable() : 0);
				case EM_UNDO:
				case WM_UNDO:
					if (le)
						le->undo();
					else if (pe)
						pe->undo();
					return TRUE;
				case EM_EMPTYUNDOBUFFER:
					if (pe)
						pe->document()->clearUndoRedoStacks();
					return 0;
				case EM_SETPASSWORDCHAR:
					if (le)
						le->setEchoMode(wParam ? QLineEdit::Password : QLineEdit::Normal);
					return 0;
				case EM_SETCUEBANNER:
					if (le)
						le->setPlaceholderText(qs(reinterpret_cast<LPCWSTR>(lParam)));
					else if (pe)
						pe->setPlaceholderText(qs(reinterpret_cast<LPCWSTR>(lParam)));
					return TRUE;
				case EM_SHOWBALLOONTIP:
				{
					const auto* tip = reinterpret_cast<const EDITBALLOONTIP*>(lParam);
					if (tip && w->widget)
					{
						QString t = qs(tip->pszText);
						if (tip->pszTitle && *tip->pszTitle)
							t = QStringLiteral("<b>%1</b><br>%2").arg(qs(tip->pszTitle).toHtmlEscaped(), t.toHtmlEscaped());
						QToolTip::showText(w->widget->mapToGlobal(QPoint(0, w->widget->height())), t, w->widget);
					}
					return TRUE;
				}
				case EM_HIDEBALLOONTIP:
					QToolTip::hideText();
					return TRUE;
				case EM_SETMARGINS:
					if (le)
						le->setTextMargins((wParam & EC_LEFTMARGIN) ? LOWORD(lParam) : 0, 0, (wParam & EC_RIGHTMARGIN) ? HIWORD(lParam) : 0, 0);
					return 0;
				case EM_SETTABSTOPS:
					return TRUE;
				case WM_CUT:
					if (le)
						le->cut();
					else if (pe)
						pe->cut();
					return 0;
				case WM_COPY:
					if (le)
						le->copy();
					else if (pe)
						pe->copy();
					return 0;
				case WM_PASTE:
					if (le)
						le->paste();
					else if (pe)
						pe->paste();
					return 0;
				case WM_CLEAR:
					if (le)
						le->del();
					else if (pe)
						pe->textCursor().removeSelectedText();
					return 0;
				default:
					return -0x7FFFFFFF;
			}
		}

		QListWidget* listOf(Window* w)
		{
			return qobject_cast<QListWidget*>(w->widget);
		}

		LRESULT listProc(Window* w, UINT msg, WPARAM wParam, LPARAM lParam, bool combo)
		{
			// the LB_ messages, and the CB_ ones for the list of a combo box
			auto* c = combo ? qobject_cast<QComboBox*>(w->widget) : nullptr;
			QListWidget* l = combo ? nullptr : listOf(w);
			if (!c && !l)
				return -0x7FFFFFFF;
			auto count = [&] { return c ? c->count() : l->count(); };
			auto textAt = [&](int i) { return c ? c->itemText(i) : (l->item(i) ? l->item(i)->text() : QString()); };
			auto dataAt = [&](int i) -> LPARAM { return static_cast<LPARAM>(c ? c->itemData(i, Qt::UserRole + 7).toLongLong() : (l->item(i) ? l->item(i)->data(Qt::UserRole + 7).toLongLong() : 0)); };
			auto insertAt = [&](int i, const QString& t) -> int
			{
				const bool sorted = combo ? (w->style & CBS_SORT) : (w->style & LBS_SORT);
				if (sorted)
				{
					i = 0;
					while (i < count() && QString::compare(textAt(i), t, Qt::CaseInsensitive) <= 0)
						++i;
				}
				else if (i < 0 || i > count())
				{
					i = count();
				}
				w->settingText = true;
				if (c)
					c->insertItem(i, t);
				else
				{
					l->setSortingEnabled(false);
					l->insertItem(i, t);
				}
				w->settingText = false;
				return i;
			};
			auto find = [&](int start, const QString& t, bool exact) -> int
			{
				const int n = count();
				for (int k = 0; k < n; ++k)
				{
					const int i = (start + 1 + k) % std::max(n, 1);
					const QString s = textAt(i);
					if (exact ? (s.compare(t, Qt::CaseInsensitive) == 0) : s.startsWith(t, Qt::CaseInsensitive))
						return i;
				}
				return -1;
			};
			const int err = -1;
			switch (msg)
			{
				case LB_ADDSTRING: case CB_ADDSTRING:
					return insertAt(-1, qs(reinterpret_cast<LPCWSTR>(lParam)));
				case LB_INSERTSTRING: case CB_INSERTSTRING:
					return insertAt(static_cast<int>(static_cast<intptr_t>(wParam)), qs(reinterpret_cast<LPCWSTR>(lParam)));
				case LB_DELETESTRING: case CB_DELETESTRING:
				{
					const int i = static_cast<int>(wParam);
					if (i < 0 || i >= count())
						return err;
					w->settingText = true;
					if (c)
						c->removeItem(i);
					else
						delete l->takeItem(i);
					w->settingText = false;
					return count();
				}
				case LB_RESETCONTENT: case CB_RESETCONTENT:
					w->settingText = true;
					if (c)
					{
						const QString edit = c->isEditable() ? c->currentText() : QString();
						c->clear();
						if (c->isEditable())
							c->setEditText(QString());
						Q_UNUSED(edit);
					}
					else
						l->clear();
					w->settingText = false;
					return 0;
				case LB_GETCOUNT: case CB_GETCOUNT:
					return count();
				case CBEM_GETCOMBOCONTROL: // a ComboBoxEx32 is the combo box itself
					return c ? reinterpret_cast<LRESULT>(hwnd(w)) : 0;
				case CB_GETCOMBOBOXINFO:
				{
					auto* info = reinterpret_cast<COMBOBOXINFO*>(lParam);
					if (!c || !info)
						return FALSE;
					// the edit and the list are the combo box: their messages go to it
					const QRect r = c->rect();
					info->rcItem = RECT{2, 2, r.width() - 20, r.height() - 2};
					info->rcButton = RECT{r.width() - 18, 2, r.width() - 2, r.height() - 2};
					info->stateButton = 0;
					info->hwndCombo = hwnd(w);
					info->hwndItem = hwnd(w);
					info->hwndList = hwnd(w);
					return TRUE;
				}
				case CBEM_GETEDITCONTROL:
					return 0;
				case LB_GETCURSEL: case CB_GETCURSEL:
					if (c)
						return c->currentIndex();
					return l->currentRow() >= 0 && l->currentItem() && l->currentItem()->isSelected() ? l->currentRow() : (l->selectedItems().isEmpty() ? -1 : l->row(l->selectedItems().first()));
				case LB_SETCURSEL: case CB_SETCURSEL:
				{
					const int i = static_cast<int>(static_cast<intptr_t>(wParam));
					w->settingText = true;
					if (c)
					{
						c->setCurrentIndex(i >= 0 && i < count() ? i : -1);
						if (i < 0 && c->isEditable())
							c->setEditText(QString());
					}
					else
					{
						l->clearSelection();
						if (i >= 0 && i < count())
						{
							l->setCurrentRow(i);
							l->item(i)->setSelected(true);
							l->scrollToItem(l->item(i));
						}
					}
					w->settingText = false;
					return (i >= 0 && i < count()) ? i : err;
				}
				case LB_GETTEXT: case CB_GETLBTEXT:
				{
					const int i = static_cast<int>(wParam);
					if (i < 0 || i >= count())
						return err;
					const std::wstring s = ws(textAt(i));
					if (lParam)
						std::wcscpy(reinterpret_cast<wchar_t*>(lParam), s.c_str());
					return static_cast<LRESULT>(s.size());
				}
				case LB_GETTEXTLEN: case CB_GETLBTEXTLEN:
				{
					const int i = static_cast<int>(wParam);
					if (i < 0 || i >= count())
						return err;
					return static_cast<LRESULT>(ws(textAt(i)).size());
				}
				case LB_FINDSTRING: case CB_FINDSTRING:
					return find(static_cast<int>(static_cast<intptr_t>(wParam)), qs(reinterpret_cast<LPCWSTR>(lParam)), false);
				case LB_FINDSTRINGEXACT: case CB_FINDSTRINGEXACT:
					return find(static_cast<int>(static_cast<intptr_t>(wParam)), qs(reinterpret_cast<LPCWSTR>(lParam)), true);
				case LB_SELECTSTRING: case CB_SELECTSTRING:
				{
					const int i = find(static_cast<int>(static_cast<intptr_t>(wParam)), qs(reinterpret_cast<LPCWSTR>(lParam)), false);
					if (i >= 0)
						listProc(w, combo ? CB_SETCURSEL : LB_SETCURSEL, static_cast<WPARAM>(i), 0, combo);
					return i;
				}
				case LB_GETITEMDATA: case CB_GETITEMDATA:
				{
					const int i = static_cast<int>(wParam);
					return (i >= 0 && i < count()) ? dataAt(i) : err;
				}
				case LB_SETITEMDATA: case CB_SETITEMDATA:
				{
					const int i = static_cast<int>(wParam);
					if (i < 0 || i >= count())
						return err;
					if (c)
						c->setItemData(i, static_cast<qlonglong>(lParam), Qt::UserRole + 7);
					else
						l->item(i)->setData(Qt::UserRole + 7, static_cast<qlonglong>(lParam));
					return TRUE;
				}
				case LB_GETTOPINDEX: case CB_GETTOPINDEX:
					if (l)
					{
						QListWidgetItem* it = l->itemAt(0, 0);
						return it ? l->row(it) : 0;
					}
					return 0;
				case LB_SETTOPINDEX: case CB_SETTOPINDEX:
					if (l && l->item(static_cast<int>(wParam)))
						l->scrollToItem(l->item(static_cast<int>(wParam)), QAbstractItemView::PositionAtTop);
					return 0;
				case LB_GETSEL:
				{
					QListWidgetItem* it = l ? l->item(static_cast<int>(wParam)) : nullptr;
					return it ? (it->isSelected() ? 1 : 0) : err;
				}
				case LB_SETSEL:
				{
					if (!l)
						return err;
					const int i = static_cast<int>(static_cast<intptr_t>(lParam));
					w->settingText = true;
					if (i < 0)
					{
						for (int k = 0; k < l->count(); ++k)
							l->item(k)->setSelected(wParam != 0);
					}
					else if (QListWidgetItem* it = l->item(i))
					{
						it->setSelected(wParam != 0);
					}
					w->settingText = false;
					return 0;
				}
				case LB_SELITEMRANGE:
				case LB_SELITEMRANGEEX:
					if (l)
					{
						int a = (msg == LB_SELITEMRANGE) ? LOWORD(lParam) : static_cast<int>(wParam);
						int b = (msg == LB_SELITEMRANGE) ? HIWORD(lParam) : static_cast<int>(lParam);
						const bool sel = (msg == LB_SELITEMRANGE) ? (wParam != 0) : (a <= b);
						if (a > b)
							std::swap(a, b);
						for (int k = a; k <= b && k < l->count(); ++k)
							l->item(k)->setSelected(sel);
					}
					return 0;
				case LB_GETSELCOUNT:
					return l ? l->selectedItems().size() : err;
				case LB_GETSELITEMS:
				{
					if (!l)
						return err;
					auto* out = reinterpret_cast<int*>(lParam);
					int n = 0;
					for (int k = 0; k < l->count() && n < static_cast<int>(wParam); ++k)
					{
						if (l->item(k)->isSelected())
							out[n++] = k;
					}
					return n;
				}
				case LB_SETCARETINDEX:
					if (l && l->item(static_cast<int>(wParam)))
						l->setCurrentRow(static_cast<int>(wParam), QItemSelectionModel::NoUpdate);
					return 0;
				case LB_GETCARETINDEX:
					return l ? l->currentRow() : 0;
				case LB_SETANCHORINDEX:
				case LB_GETANCHORINDEX:
					return 0;
				case LB_GETITEMHEIGHT: case CB_GETITEMHEIGHT:
					return QFontMetrics(w->widget->font()).height() + 2;
				case LB_SETITEMHEIGHT: case CB_SETITEMHEIGHT:
				case LB_SETHORIZONTALEXTENT: case CB_SETHORIZONTALEXTENT:
				case LB_SETCOLUMNWIDTH: case LB_SETTABSTOPS:
				case LB_INITSTORAGE: case CB_INITSTORAGE:
				case CB_SETEXTENDEDUI: case CB_SETLOCALE: case LB_SETLOCALE:
					return 0;
				case LB_GETITEMRECT:
				{
					auto* rc = reinterpret_cast<RECT*>(lParam);
					if (!l || !rc || !l->item(static_cast<int>(wParam)))
						return err;
					const QRect r = l->visualItemRect(l->item(static_cast<int>(wParam)));
					*rc = RECT{r.left(), r.top(), r.left() + r.width(), r.top() + r.height()};
					return 0;
				}
				case LB_ITEMFROMPOINT:
				{
					if (!l)
						return 0;
					QListWidgetItem* it = l->itemAt(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
					return it ? l->row(it) : MAKELONG(l->count(), 1);
				}
				case CB_SHOWDROPDOWN:
					if (c)
					{
						if (wParam)
							c->showPopup();
						else
							c->hidePopup();
					}
					return TRUE;
				case CB_GETDROPPEDSTATE:
					return (c && c->view() && c->view()->isVisible()) ? TRUE : FALSE;
				case CB_LIMITTEXT:
					if (c && c->lineEdit())
						c->lineEdit()->setMaxLength(wParam ? static_cast<int>(wParam) : 32767);
					return TRUE;
				case CB_GETEDITSEL:
					if (c && c->lineEdit())
					{
						QLineEdit* le = c->lineEdit();
						const int s = le->hasSelectedText() ? le->selectionStart() : le->cursorPosition();
						const int e = le->hasSelectedText() ? le->selectionEnd() : le->cursorPosition();
						if (wParam)
							*reinterpret_cast<DWORD*>(wParam) = static_cast<DWORD>(s);
						if (lParam)
							*reinterpret_cast<DWORD*>(lParam) = static_cast<DWORD>(e);
						return MAKELRESULT(s, e);
					}
					return 0;
				case CB_SETEDITSEL:
					if (c && c->lineEdit())
					{
						const int s = static_cast<short>(LOWORD(lParam));
						const int e = static_cast<short>(HIWORD(lParam));
						QLineEdit* le = c->lineEdit();
						if (s < 0)
							le->deselect();
						else
							le->setSelection(s, (e < 0 ? static_cast<int>(le->text().size()) : e) - s);
					}
					return TRUE;
				case CB_SETDROPPEDWIDTH:
					if (c && c->view())
						c->view()->setMinimumWidth(static_cast<int>(wParam));
					return static_cast<LRESULT>(wParam);
				case CB_GETDROPPEDWIDTH:
					return c && c->view() ? c->view()->minimumWidth() : 0;
				case CB_SETMINVISIBLE:
					if (c)
						c->setMaxVisibleItems(static_cast<int>(wParam));
					return TRUE;
				case CB_SETCUEBANNER:
					if (c && c->lineEdit())
						c->lineEdit()->setPlaceholderText(qs(reinterpret_cast<LPCWSTR>(lParam)));
					else if (c)
						c->setPlaceholderText(qs(reinterpret_cast<LPCWSTR>(lParam)));
					return TRUE;
				default:
					return -0x7FFFFFFF;
			}
		}

		int buttonCheck(Window* w)
		{
			if (auto* cb = qobject_cast<QCheckBox*>(w->widget))
				return cb->checkState() == Qt::Checked ? BST_CHECKED : (cb->checkState() == Qt::PartiallyChecked ? BST_INDETERMINATE : BST_UNCHECKED);
			if (auto* ab = qobject_cast<QAbstractButton*>(w->widget))
				return ab->isChecked() ? BST_CHECKED : BST_UNCHECKED;
			return BST_UNCHECKED;
		}
	}

	LRESULT controlProc(Window* w, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		if (!w || w->destroyed)
			return 0;
		if (w->kind == Kind::Scintilla)
		{
			// the Scintilla of the host: its own handle answers
			HWND sci = static_cast<HWND>(w->ctl);
			return SendMessageW(sci, msg, wParam, lParam);
		}
		if (isCommonControl(w->kind))
		{
			const LRESULT r = commonControlProc(w, msg, wParam, lParam);
			if (r != -0x7FFFFFFF)
				return r;
		}
		QWidget* widget = w->widget;
		switch (w->kind)
		{
			case Kind::Edit:
			{
				const LRESULT r = editProc(w, msg, wParam, lParam);
				if (r != -0x7FFFFFFF)
					return r;
				break;
			}
			case Kind::ComboBox:
			{
				const LRESULT r = listProc(w, msg, wParam, lParam, true);
				if (r != -0x7FFFFFFF)
					return r;
				break;
			}
			case Kind::ListBox:
			{
				const LRESULT r = listProc(w, msg, wParam, lParam, false);
				if (r != -0x7FFFFFFF)
					return r;
				break;
			}
			case Kind::Button:
				switch (msg)
				{
					case BM_GETCHECK:
						return buttonCheck(w);
					case BM_SETCHECK:
						w->checkState = static_cast<int>(wParam);
						if (auto* cb = qobject_cast<QCheckBox*>(widget))
							cb->setCheckState(wParam == BST_CHECKED ? Qt::Checked : (wParam == BST_INDETERMINATE ? Qt::PartiallyChecked : Qt::Unchecked));
						else if (auto* ab = qobject_cast<QAbstractButton*>(widget))
						{
							if (ab->isCheckable() || qobject_cast<QRadioButton*>(ab))
								ab->setChecked(wParam == BST_CHECKED);
						}
						return 0;
					case BM_GETSTATE:
					{
						LRESULT s = buttonCheck(w);
						if (auto* ab = qobject_cast<QAbstractButton*>(widget); ab && ab->isDown())
							s |= BST_PUSHED;
						if (widget && widget->hasFocus())
							s |= BST_FOCUS;
						return s;
					}
					case BM_SETSTATE:
						if (auto* ab = qobject_cast<QAbstractButton*>(widget))
							ab->setDown(wParam != 0);
						return 0;
					case BM_CLICK:
						if (auto* ab = qobject_cast<QAbstractButton*>(widget))
							ab->click();
						return 0;
					case BM_SETSTYLE:
						w->style = (w->style & ~BS_TYPEMASK) | (static_cast<DWORD>(wParam) & BS_TYPEMASK);
						if (auto* pb = qobject_cast<QPushButton*>(widget))
							pb->setDefault(buttonType(w->style) == BS_DEFPUSHBUTTON);
						return 0;
					case BM_SETIMAGE:
						if (auto* ab = qobject_cast<QAbstractButton*>(widget))
						{
							const QIcon icon = iconOf(reinterpret_cast<HANDLE>(lParam));
							ab->setIcon(icon);
							if (!icon.isNull())
								ab->setIconSize(icon.availableSizes().value(0, QSize(16, 16)));
						}
						return 0;
					case BM_GETIMAGE:
						return 0;
					default:
						break;
				}
				break;
			case Kind::Static:
				if (auto* l = qobject_cast<QLabel*>(widget))
				{
					if (msg == STM_SETICON || msg == STM_SETIMAGE)
					{
						const HANDLE h = reinterpret_cast<HANDLE>(msg == STM_SETICON ? wParam : lParam);
						l->setPixmap(pixmapOf(h, l->size()));
						return 0;
					}
					if (msg == STM_GETICON || msg == STM_GETIMAGE)
						return 0;
				}
				break;
			case Kind::ScrollBar:
				if (auto* sb = qobject_cast<QScrollBar*>(widget))
				{
					switch (msg)
					{
						case SBM_SETPOS: { const int old = sb->value(); sb->setValue(static_cast<int>(wParam)); return old; }
						case SBM_GETPOS: return sb->value();
						case SBM_SETRANGE: sb->setRange(static_cast<int>(wParam), static_cast<int>(lParam)); return 0;
						case SBM_GETRANGE:
							if (wParam)
								*reinterpret_cast<int*>(wParam) = sb->minimum();
							if (lParam)
								*reinterpret_cast<int*>(lParam) = sb->maximum();
							return 0;
						case SBM_SETSCROLLINFO:
						{
							const auto* si = reinterpret_cast<const SCROLLINFO*>(lParam);
							if (si)
							{
								if (si->fMask & SIF_RANGE)
									sb->setRange(si->nMin, std::max(si->nMin, si->nMax - static_cast<int>((si->fMask & SIF_PAGE) ? si->nPage : 0) + 1));
								if (si->fMask & SIF_PAGE)
									sb->setPageStep(static_cast<int>(si->nPage));
								if (si->fMask & SIF_POS)
									sb->setValue(si->nPos);
							}
							return sb->value();
						}
						case SBM_GETSCROLLINFO:
						{
							auto* si = reinterpret_cast<SCROLLINFO*>(lParam);
							if (si)
							{
								si->nMin = sb->minimum();
								si->nMax = sb->maximum() + sb->pageStep() - 1;
								si->nPage = static_cast<UINT>(sb->pageStep());
								si->nPos = sb->value();
								si->nTrackPos = sb->sliderPosition();
							}
							return TRUE;
						}
						default:
							break;
					}
				}
				break;
			default:
				break;
		}

		// messages of every control
		switch (msg)
		{
			case WM_SETTEXT:
				w->settingText = true;
				setTextOf(w, qs(reinterpret_cast<LPCWSTR>(lParam)));
				w->settingText = false;
				return TRUE;
			case WM_GETTEXT:
				return copyOut(textOf(widget), reinterpret_cast<LPWSTR>(lParam), static_cast<int>(wParam));
			case WM_GETTEXTLENGTH:
				return static_cast<LRESULT>(ws(textOf(widget)).size());
			case WM_SETFONT:
				w->font = reinterpret_cast<HFONT>(wParam);
				if (widget && wParam)
					widget->setFont(fontOf(w->font));
				return 0;
			case WM_GETFONT:
				return reinterpret_cast<LRESULT>(w->font);
			case WM_SETREDRAW:
				w->redraw = wParam != 0;
				if (widget)
				{
					widget->setUpdatesEnabled(wParam != 0);
					if (wParam)
						widget->update();
				}
				return 0;
			case WM_GETDLGCODE:
				return (w->kind == Kind::Edit) ? (DLGC_WANTCHARS | DLGC_HASSETSEL | DLGC_WANTARROWS) : 0;
			case WM_STYLECHANGED:
				if (w->kind == Kind::Edit)
				{
					if (auto* le = qobject_cast<QLineEdit*>(widget))
						le->setReadOnly(w->style & ES_READONLY);
				}
				return 0;
			case WM_CONTEXTMENU:
				// edit boxes have their own menu; the others give it to the parent
				if (w->kind == Kind::Edit || w->kind == Kind::ComboBox)
				{
					g_defaultReached = true;
					return 0;
				}
				return DefWindowProcW(hwnd(w), msg, wParam, lParam);
			default:
				return DefWindowProcW(hwnd(w), msg, wParam, lParam);
		}
	}

	// ---- dialog units ------------------------------------------------------------------------
	QSize baseUnits(QWidget* w)
	{
		const QFontMetrics fm(w ? w->font() : QApplication::font());
		const int cx = (fm.horizontalAdvance(QStringLiteral("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz")) / 26 + 1) / 2;
		return QSize(std::max(cx, 4), std::max(fm.height(), 8));
	}

	QRect dluRect(QWidget* dlg, int x, int y, int cx, int cy)
	{
		const QSize b = baseUnits(dlg);
		return QRect(x * b.width() / 4, y * b.height() / 8, cx * b.width() / 4, cy * b.height() / 8);
	}

	HWND createDialog(const NppRcDialog* t, HWND parent, DLGPROC proc, LPARAM init, bool modal)
	{
		if (!t)
		{
			SetLastError(1813); // ERROR_RESOURCE_TYPE_NOT_FOUND
			return nullptr;
		}
		const bool child = (t->style & WS_CHILD) != 0;
		QWidget* parentWidget = parentWidgetOf(parent);
		Qt::WindowFlags flags;
		if (child)
		{
			flags = Qt::Widget;
		}
		else
		{
			parentWidget = parentWidget ? parentWidget->window() : nullptr;
			flags = Qt::Dialog;
			if (t->exStyle & WS_EX_TOOLWINDOW)
				flags = Qt::Tool;
			if (!(t->style & WS_CAPTION))
				flags |= Qt::FramelessWindowHint;
			if (!(t->style & WS_SYSMENU))
				flags |= Qt::CustomizeWindowHint | Qt::WindowTitleHint;
			else
				flags |= Qt::WindowCloseButtonHint | Qt::WindowTitleHint;
			if (t->style & WS_MAXIMIZEBOX)
				flags |= Qt::WindowMaximizeButtonHint;
			if (t->style & WS_MINIMIZEBOX)
				flags |= Qt::WindowMinimizeButtonHint;
			if (t->exStyle & WS_EX_TOPMOST)
				flags |= Qt::WindowStaysOnTopHint;
		}
		QWidget* dlg = newW32Widget(parentWidget, flags);
		dlg->setAttribute(Qt::WA_DeleteOnClose, false);
		dlg->setFocusPolicy(Qt::NoFocus);
		dlg->setAutoFillBackground(true);
		if (t->fontSize > 0 && (t->style & DS_SETFONT))
		{
			QFont font = dlg->font();
			if (t->fontName && *t->fontName)
				font.setFamily(qs(t->fontName));
			font.setPointSize(t->fontSize);
			if (t->fontWeight > 0)
				font.setWeight(static_cast<QFont::Weight>(t->fontWeight));
			font.setItalic(t->fontItalic);
			dlg->setFont(font);
		}
		if (t->exStyle & WS_EX_LAYOUTRTL)
			dlg->setLayoutDirection(Qt::RightToLeft);
		Window* w = newWindow(Kind::Dialog, dlg, parent, 0, t->style, t->exStyle, L"#32770");
		w->isDialog = true;
		w->modal = modal;
		w->dlgProc = proc;
		w->wndProc = dialogWndProc;
		w->classProc = dialogWndProc;
		w->tmpl = t;
		w->extra.assign(32, 0);
		dlg->setWindowTitle(qs(t->caption));
		if (Window* pw = win(parent))
		{
			if (!child)
				w->owner = parent;
		}
		else if (!child)
		{
			w->owner = parent;
		}

		const QRect client = dluRect(dlg, t->x, t->y, t->cx, t->cy);
		// A dialog laid out in resource units needs to keep space for its fixed controls
		// when it becomes the content of a new dock widget.
		dlg->setMinimumSize(client.size());
		dlg->resize(client.size());
		w->lastSize = client.size();
		if (child)
			dlg->move(client.topLeft());

		// the controls, in the order of the template (it is the tab order too)
		QWidget* prevFocus = nullptr;
		std::vector<std::pair<QWidget*, Window*>> groups;
		for (int i = 0; i < t->count; ++i)
		{
			const NppRcControl& c = t->controls[i];
			const DWORD style = c.style;
			Window* ctl = nullptr;
			const QString text = qs(c.text);
			QWidget* cw = createControl(kindOfClass(c.className ? c.className : L""), c.className ? c.className : L"", style, dlg, text, c.textId, &ctl);
			if (!cw || !ctl)
				continue;
			ctl->parent = hwnd(w);
			ctl->id = c.id;
			ctl->style = style;
			ctl->exStyle = c.exStyle;
			QRect r = dluRect(dlg, c.x, c.y, c.cx, c.cy);
			if (ctl->kind == Kind::ComboBox)
				r.setHeight(std::max(cw->sizeHint().height(), std::min(r.height(), cw->sizeHint().height())));
			if (ctl->kind == Kind::Edit && !(style & ES_MULTILINE) && r.height() < cw->sizeHint().height())
			{
				const int h = std::min(cw->sizeHint().height(), r.height() + 6);
				r.adjust(0, -(h - r.height()) / 2, 0, (h - r.height()) - (h - r.height()) / 2);
			}
			if (ctl->kind == Kind::Button && (buttonType(style) == BS_PUSHBUTTON || buttonType(style) == BS_DEFPUSHBUTTON)
				&& r.height() < cw->sizeHint().height())
			{
				const int h = std::min(cw->sizeHint().height(), r.height() + 6);
				r.adjust(0, -(h - r.height()) / 2, 0, (h - r.height()) - (h - r.height()) / 2);
			}
			cw->setGeometry(r);
			if (ctl->kind == Kind::Button && buttonType(style) == BS_DEFPUSHBUTTON)
				w->defId = c.id;
			if (ctl->kind == Kind::Button && buttonType(style) == BS_GROUPBOX)
				cw->lower();
			if (style & WS_DISABLED)
				cw->setEnabled(false);
			if (!(style & WS_VISIBLE))
				cw->hide();
			if (!(style & WS_TABSTOP) && ctl->kind != Kind::Edit && ctl->kind != Kind::ComboBox && ctl->kind != Kind::ListBox
				&& !isCommonControl(ctl->kind))
				cw->setFocusPolicy(cw->focusPolicy() == Qt::NoFocus ? Qt::NoFocus : Qt::ClickFocus);
			if (cw->focusPolicy() != Qt::NoFocus)
			{
				if (prevFocus)
					QWidget::setTabOrder(prevFocus, cw);
				prevFocus = cw;
			}
		}
		// Expose the dialog font via WM_GETFONT, as well as to the Qt children.
		w->font = fontHandle(dlg->font());

		// WM_INITDIALOG: TRUE gives the focus to the first control that takes it
		const LRESULT focus = dispatch(w, WM_INITDIALOG, 0, init);
		if (w->destroyed)
			return nullptr;
		if (focus)
		{
			for (QWidget* x : dlg->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly))
			{
				if (x->isVisibleTo(dlg) && x->isEnabled() && x->focusPolicy() & Qt::TabFocus)
				{
					x->setFocus(Qt::OtherFocusReason);
					break;
				}
			}
		}
		if (!child && (t->style & DS_CENTER) && parentWidget)
		{
			const QRect pr = parentWidget->window()->frameGeometry();
			dlg->move(pr.center() - QPoint(dlg->width() / 2, dlg->height() / 2));
		}
		else if (!child && parentWidget && !modal)
		{
			const QRect pr = parentWidget->window()->geometry();
			dlg->move(pr.topLeft() + client.topLeft());
		}
		if ((t->style & WS_VISIBLE) && !modal)
			dlg->show();
		else if (child)
			dlg->hide(); // explicitly: Qt shows the children of a parent that is shown later, Windows does not
		return hwnd(w);
	}
}

using namespace w32;

// ---- dialogs ---------------------------------------------------------------------------------
HWND CreateDialogParamW(HINSTANCE, LPCWSTR templateName, HWND parent, DLGPROC proc, LPARAM init)
{
	return createDialog(findDialog(templateName), parent, proc, init, false);
}

HWND CreateDialogIndirectParamW(HINSTANCE, LPCDLGTEMPLATE tmpl, HWND parent, DLGPROC proc, LPARAM init)
{
	return createDialog(dialogOfTemplate(tmpl), parent, proc, init, false);
}

namespace
{
	INT_PTR runModal(const NppRcDialog* t, HWND parent, DLGPROC proc, LPARAM init)
	{
		HWND h = createDialog(t, parent, proc, init, true);
		Window* w = win(h);
		if (!w)
			return -1;
		if (w->ended)
		{
			const INT_PTR r = w->result;
			destroyWindow(w);
			return r;
		}
		QWidget* dlg = w->widget;
		dlg->setWindowModality(Qt::ApplicationModal);
		if (!(t->style & DS_CENTER) || true)
		{
			if (QWidget* p = dlg->parentWidget())
			{
				const QRect pr = p->window()->frameGeometry();
				dlg->move(pr.center() - QPoint(dlg->width() / 2, dlg->height() / 2));
			}
		}
		dlg->show();
		QEventLoop loop;
		w->loop = &loop;
		if (!w->ended)
			loop.exec(QEventLoop::DialogExec);
		w->loop = nullptr;
		const INT_PTR r = w->result;
		destroyWindow(w);
		return r;
	}
}

INT_PTR DialogBoxParamW(HINSTANCE, LPCWSTR templateName, HWND parent, DLGPROC proc, LPARAM init)
{
	return runModal(findDialog(templateName), parent, proc, init);
}

INT_PTR DialogBoxIndirectParamW(HINSTANCE, LPCDLGTEMPLATE tmpl, HWND parent, DLGPROC proc, LPARAM init)
{
	return runModal(dialogOfTemplate(tmpl), parent, proc, init);
}

BOOL EndDialog(HWND hDlg, INT_PTR result)
{
	Window* w = win(hDlg);
	if (!w)
		return FALSE;
	w->ended = true;
	w->result = result;
	if (w->loop)
	{
		w->loop->exit(0);
	}
	else if (!w->modal)
	{
		// a modeless dialog: EndDialog hides it (like Windows, the window stays)
		if (w->widget)
			w->widget->hide();
		w->ended = false;
	}
	return TRUE;
}

HWND GetDlgItem(HWND hDlg, int id)
{
	QWidget* dlg = widgetOf(hDlg);
	if (!dlg)
		return nullptr;
	for (QWidget* c : dlg->findChildren<QWidget*>())
	{
		Window* w = winOf(c);
		if (w && w->widget == c && w->id == id && w->parent == hDlg && !w->destroyed)
			return hwnd(w);
	}
	return nullptr;
}

int GetDlgCtrlID(HWND hWnd)
{
	Window* w = win(hWnd);
	return w ? w->id : 0;
}

BOOL SetDlgItemTextW(HWND hDlg, int id, LPCWSTR text)
{
	HWND h = GetDlgItem(hDlg, id);
	if (!h)
		return FALSE;
	SendMessageW(h, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(text ? text : L""));
	return TRUE;
}

BOOL SetDlgItemTextA(HWND hDlg, int id, LPCSTR text)
{
	const std::wstring t = ws(qsA(text));
	return SetDlgItemTextW(hDlg, id, t.c_str());
}

UINT GetDlgItemTextW(HWND hDlg, int id, LPWSTR text, int max)
{
	HWND h = GetDlgItem(hDlg, id);
	if (!h)
	{
		if (text && max > 0)
			text[0] = 0;
		return 0;
	}
	return static_cast<UINT>(SendMessageW(h, WM_GETTEXT, static_cast<WPARAM>(max), reinterpret_cast<LPARAM>(text)));
}

UINT GetDlgItemTextA(HWND hDlg, int id, LPSTR text, int max)
{
	HWND h = GetDlgItem(hDlg, id);
	const int len = h ? static_cast<int>(SendMessageW(h, WM_GETTEXTLENGTH, 0, 0)) : 0;
	std::wstring buf(static_cast<size_t>(len) + 1, L'\0');
	if (h)
		SendMessageW(h, WM_GETTEXT, static_cast<WPARAM>(len + 1), reinterpret_cast<LPARAM>(buf.data()));
	return static_cast<UINT>(copyOutA(qs(buf.c_str()), text, max));
}

BOOL SetDlgItemInt(HWND hDlg, int id, UINT value, BOOL isSigned)
{
	const QString s = isSigned ? QString::number(static_cast<int>(value)) : QString::number(value);
	const std::wstring t = ws(s);
	return SetDlgItemTextW(hDlg, id, t.c_str());
}

UINT GetDlgItemInt(HWND hDlg, int id, BOOL* translated, BOOL isSigned)
{
	wchar_t buf[64] = {};
	GetDlgItemTextW(hDlg, id, buf, 64);
	bool ok = false;
	const QString s = qs(buf).trimmed();
	const qlonglong v = s.toLongLong(&ok);
	if (translated)
		*translated = ok && (isSigned || v >= 0) ? TRUE : FALSE;
	return ok ? static_cast<UINT>(v) : 0;
}

BOOL CheckDlgButton(HWND hDlg, int id, UINT check)
{
	HWND h = GetDlgItem(hDlg, id);
	if (!h)
		return FALSE;
	SendMessageW(h, BM_SETCHECK, check, 0);
	return TRUE;
}

UINT IsDlgButtonChecked(HWND hDlg, int id)
{
	return static_cast<UINT>(SendMessageW(GetDlgItem(hDlg, id), BM_GETCHECK, 0, 0));
}

BOOL CheckRadioButton(HWND hDlg, int first, int last, int check)
{
	for (int id = first; id <= last; ++id)
	{
		if (HWND h = GetDlgItem(hDlg, id))
			SendMessageW(h, BM_SETCHECK, id == check ? BST_CHECKED : BST_UNCHECKED, 0);
	}
	return TRUE;
}

HWND GetNextDlgTabItem(HWND hDlg, HWND ctl, BOOL previous)
{
	QWidget* c = widgetOf(ctl);
	QWidget* d = widgetOf(hDlg);
	if (!d)
		return nullptr;
	QWidget* n = c ? (previous ? c->previousInFocusChain() : c->nextInFocusChain()) : d->nextInFocusChain();
	for (int guard = 0; n && guard < 500; ++guard)
	{
		Window* w = winOf(n);
		if (w && w->widget == n && w->parent == hDlg && n->isEnabled() && n->isVisible())
			return hwnd(w);
		n = previous ? n->previousInFocusChain() : n->nextInFocusChain();
	}
	return nullptr;
}

HWND GetNextDlgGroupItem(HWND hDlg, HWND ctl, BOOL previous)
{
	return GetNextDlgTabItem(hDlg, ctl, previous);
}

BOOL MapDialogRect(HWND hDlg, LPRECT rc)
{
	if (!rc)
		return FALSE;
	const QSize b = baseUnits(widgetOf(hDlg));
	rc->left = rc->left * b.width() / 4;
	rc->right = rc->right * b.width() / 4;
	rc->top = rc->top * b.height() / 8;
	rc->bottom = rc->bottom * b.height() / 8;
	return TRUE;
}

LONG GetDialogBaseUnits()
{
	const QSize b = baseUnits(nullptr);
	return MAKELONG(b.width(), b.height());
}

// ---- message boxes ---------------------------------------------------------------------------
int MessageBoxW(HWND parent, LPCWSTR text, LPCWSTR caption, UINT type)
{
	QWidget* p = widgetOf(parent);
	if (!p)
		p = widgetOf(nppHandle());
	QMessageBox box(p ? p->window() : nullptr);
	box.setWindowTitle(caption ? qs(caption) : QStringLiteral("Error"));
	box.setText(qs(text));
	switch (type & 0xF0)
	{
		case MB_ICONHAND: box.setIcon(QMessageBox::Critical); break;
		case MB_ICONQUESTION: box.setIcon(QMessageBox::Question); break;
		case MB_ICONEXCLAMATION: box.setIcon(QMessageBox::Warning); break;
		case MB_ICONASTERISK: box.setIcon(QMessageBox::Information); break;
		default: break;
	}
	QList<std::pair<QMessageBox::StandardButton, int>> buttons;
	switch (type & 0x0F)
	{
		case MB_OKCANCEL: buttons = {{QMessageBox::Ok, IDOK}, {QMessageBox::Cancel, IDCANCEL}}; break;
		case MB_ABORTRETRYIGNORE: buttons = {{QMessageBox::Abort, IDABORT}, {QMessageBox::Retry, IDRETRY}, {QMessageBox::Ignore, IDIGNORE}}; break;
		case MB_YESNOCANCEL: buttons = {{QMessageBox::Yes, IDYES}, {QMessageBox::No, IDNO}, {QMessageBox::Cancel, IDCANCEL}}; break;
		case MB_YESNO: buttons = {{QMessageBox::Yes, IDYES}, {QMessageBox::No, IDNO}}; break;
		case MB_RETRYCANCEL: buttons = {{QMessageBox::Retry, IDRETRY}, {QMessageBox::Cancel, IDCANCEL}}; break;
		case MB_CANCELTRYCONTINUE: buttons = {{QMessageBox::Cancel, IDCANCEL}, {QMessageBox::Retry, IDTRYAGAIN}, {QMessageBox::Ignore, IDCONTINUE}}; break;
		default: buttons = {{QMessageBox::Ok, IDOK}}; break;
	}
	QMessageBox::StandardButtons all;
	for (const auto& b : buttons)
		all |= b.first;
	box.setStandardButtons(all);
	const int def = (type & MB_DEFBUTTON3) ? 2 : ((type & MB_DEFBUTTON2) ? 1 : 0);
	if (def < buttons.size())
		box.setDefaultButton(buttons[def].first);
	if (type & MB_TOPMOST)
		box.setWindowFlag(Qt::WindowStaysOnTopHint);
	const auto clicked = static_cast<QMessageBox::StandardButton>(box.exec());
	for (const auto& b : buttons)
	{
		if (b.first == clicked)
			return b.second;
	}
	return (buttons.size() == 1) ? buttons[0].second : IDCANCEL;
}

int MessageBoxA(HWND parent, LPCSTR text, LPCSTR caption, UINT type)
{
	const std::wstring t = ws(qsA(text));
	const std::wstring c = ws(qsA(caption));
	return MessageBoxW(parent, t.c_str(), caption ? c.c_str() : nullptr, type);
}

BOOL MessageBeep(UINT)
{
	QApplication::beep();
	return TRUE;
}

BOOL GetComboBoxInfo(HWND hwndCombo, PCOMBOBOXINFO info)
{
	return SendMessageW(hwndCombo, CB_GETCOMBOBOXINFO, 0, reinterpret_cast<LPARAM>(info)) ? TRUE : FALSE;
}
