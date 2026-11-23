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

// Notepad++ Linux plugin SDK, npp_win32 library: windows. A window is a QWidget with
// a Window record; its messages go through the subclasses and the window procedure,
// and the events of Qt become the messages of Windows.

#include <set>

#include <QAbstractScrollArea>
#include <QApplication>
#include <QCloseEvent>
#include <QContextMenuEvent>
#include <QCoreApplication>
#include <QCursor>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPixmap>
#include <QScreen>
#include <QScrollBar>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QWheelEvent>

#include "W32Internal.h"
#include <shlobj.h>
#include <richedit.h>

namespace w32
{
	thread_local bool g_defaultReached = false;

	namespace
	{
		struct WindowClass
		{
			std::wstring name;
			WNDPROC proc = nullptr;
			UINT style = 0;
			int cbWndExtra = 0;
			HBRUSH background = nullptr;
			HCURSOR cursor = nullptr;
			HICON icon = nullptr;
		};

		std::map<std::wstring, WindowClass>& classes()
		{
			static std::map<std::wstring, WindowClass> c;
			return c;
		}

		std::wstring lower(const std::wstring& s)
		{
			std::wstring r = s;
			for (wchar_t& c : r)
				c = static_cast<wchar_t>(std::towlower(c));
			return r;
		}

		LRESULT windowSend(NppHandle* h, UINT msg, WPARAM wParam, LPARAM lParam)
		{
			Window* w = win(h);
			return w ? dispatch(w, msg, wParam, lParam) : 0;
		}

		int windowPost(NppHandle* h, UINT msg, WPARAM wParam, LPARAM lParam)
		{
			return PostMessageW(h, msg, wParam, lParam);
		}

		HCURSOR g_cursor = nullptr;
		HWND g_capture = nullptr;

	}

	// procedures of the windows made here: GWLP_WNDPROC gives them, CallWindowProc takes them
	LRESULT CALLBACK dialogWndProc(HWND h, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		Window* w = win(h);
		if (!w)
			return 0;
		if (w->dlgProc)
		{
			w->msgResult = 0;
			const INT_PTR r = w->dlgProc(h, msg, wParam, lParam);
			if (r)
			{
				switch (msg)
				{
					case WM_CTLCOLORMSGBOX: case WM_CTLCOLOREDIT: case WM_CTLCOLORLISTBOX: case WM_CTLCOLORBTN:
					case WM_CTLCOLORDLG: case WM_CTLCOLORSCROLLBAR: case WM_CTLCOLORSTATIC: case WM_INITDIALOG:
					case WM_COMPAREITEM: case WM_VKEYTOITEM: case WM_CHARTOITEM: case WM_QUERYDRAGICON:
						return r;
					default:
						return w->msgResult;
				}
			}
			if (w->destroyed)
				return 0;
		}
		return DefDlgProcW(h, msg, wParam, lParam);
	}

	LRESULT CALLBACK controlWndProc(HWND h, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		Window* w = win(h);
		return w ? controlProc(w, msg, wParam, lParam) : 0;
	}

	Window* win(HWND h)
	{
		if (!h || h->magic != NPP_HANDLE_MAGIC || h->kind != NPP_HANDLE_WIDGET || !h->host)
			return nullptr;
		auto* w = static_cast<Window*>(h->host);
		return (&w->handle == h && w->handle.send == windowSend) ? w : nullptr;
	}

	HWND hwnd(Window* w)
	{
		return w ? &w->handle : nullptr;
	}

	Window* winOf(QObject* obj)
	{
		for (QObject* o = obj; o; o = o->parent())
		{
			const QVariant v = o->property("_w32Window");
			if (v.isValid())
				return static_cast<Window*>(v.value<void*>());
		}
		return nullptr;
	}

	QWidget* widgetOf(HWND h)
	{
		if (!h || h->magic != NPP_HANDLE_MAGIC)
			return nullptr;
		if (Window* w = win(h))
			return w->widget;
		return static_cast<QWidget*>(h->widget);
	}

	QWidget* parentWidgetOf(HWND parent)
	{
		QWidget* p = widgetOf(parent);
		return p ? p : widgetOf(nppHandle());
	}

	HWND nppHandle()
	{
		for (QWidget* w : QApplication::topLevelWidgets())
		{
			const QVariant v = w->property("_nppMainHandle");
			if (v.isValid())
				return static_cast<HWND>(v.value<void*>());
		}
		return nullptr;
	}

	Window* newWindow(Kind kind, QWidget* widget, HWND parent, int id, DWORD style, DWORD exStyle, const std::wstring& className)
	{
		auto* w = new Window; // never freed: plugins may keep their handles after DestroyWindow
		w->kind = kind;
		w->widget = widget;
		w->parent = parent;
		w->id = id;
		w->style = style;
		w->exStyle = exStyle;
		w->className = className;
		w->handle.kind = NPP_HANDLE_WIDGET;
		w->handle.send = windowSend;
		w->handle.post = windowPost;
		w->handle.widget = widget;
		w->handle.host = w;
		w->handle.owner = nppHandle();
		if (widget)
		{
			widget->setProperty("_w32Window", QVariant::fromValue(static_cast<void*>(w)));
			widget->setProperty("_nppHandle", QVariant::fromValue(static_cast<void*>(&w->handle)));
			QObject::connect(widget, &QObject::destroyed, [w]
			{
				w->widget = nullptr;
				w->handle.widget = nullptr;
				w->destroyed = true;
			});
		}
		return w;
	}

	Window* dialogOf(Window* w)
	{
		for (Window* p = w; p; p = win(p->parent))
		{
			if (p->isDialog)
				return p;
		}
		return nullptr;
	}

	namespace
	{
		LRESULT callSubclasses(Window* w, size_t level, UINT msg, WPARAM wParam, LPARAM lParam)
		{
			// SetWindowSubclass: the last one added is called first; DefSubclassProc goes on
			if (level < w->subclasses.size())
			{
				const Subclass s = w->subclasses[w->subclasses.size() - 1 - level];
				const size_t saved = w->subclassDepth;
				w->subclassDepth = level;
				const LRESULT r = s.proc(hwnd(w), msg, wParam, lParam, s.id, s.ref);
				w->subclassDepth = saved;
				return r;
			}
			if (w->wndProc)
				return w->wndProc(hwnd(w), msg, wParam, lParam);
			return defaultProc(w, msg, wParam, lParam);
		}
	}

	LRESULT dispatch(Window* w, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		if (!w || (w->destroyed && msg != WM_NCDESTROY))
			return 0;
		return callSubclasses(w, 0, msg, wParam, lParam);
	}

	LRESULT defaultProc(Window* w, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		if (w->isDialog)
			return DefDlgProcW(hwnd(w), msg, wParam, lParam);
		if (w->kind != Kind::Custom && w->kind != Kind::Generic)
			return controlProc(w, msg, wParam, lParam);
		return DefWindowProcW(hwnd(w), msg, wParam, lParam);
	}

	void notifyCommand(Window* ctl, int code)
	{
		if (!ctl || ctl->destroyed)
			return;
		HWND parent = ctl->parent;
		if (!parent)
			return;
		SendMessageW(parent, WM_COMMAND, MAKEWPARAM(ctl->id, code), reinterpret_cast<LPARAM>(hwnd(ctl)));
	}

	LRESULT notify(Window* ctl, NMHDR* hdr, UINT code)
	{
		if (!ctl || ctl->destroyed || !ctl->parent)
			return 0;
		hdr->hwndFrom = hwnd(ctl);
		hdr->idFrom = static_cast<UINT_PTR>(ctl->id);
		hdr->code = code;
		return SendMessageW(ctl->parent, WM_NOTIFY, static_cast<WPARAM>(ctl->id), reinterpret_cast<LPARAM>(hdr));
	}

	void destroyWindow(Window* w)
	{
		if (!w || w->destroyed || w->inDestroy)
			return;
		w->inDestroy = true;
		if (g_capture == hwnd(w))
			g_capture = nullptr;
		dispatch(w, WM_DESTROY, 0, 0);
		// the child windows, after their parent like Windows does
		if (w->widget)
		{
			for (QWidget* child : w->widget->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly))
			{
				Window* c = winOf(child);
				if (c && c != w && c->widget == child)
					destroyWindow(c);
			}
		}
		for (auto& t : w->timers)
			delete t.second;
		w->timers.clear();
		dispatch(w, WM_NCDESTROY, 0, 0);
		w->destroyed = true;
		if (w->ctl && w->ctlFree)
			w->ctlFree(w->ctl);
		w->ctl = nullptr;
		if (w->loop)
			w->loop->exit(0);
		if (QWidget* widget = w->widget)
		{
			widget->hide();
			widget->setProperty("_w32Window", QVariant());
			widget->deleteLater();
		}
		w->widget = nullptr;
		w->handle.widget = nullptr;
	}

	int vkFromQt(int key, quint32 nativeVk)
	{
		if (key >= Qt::Key_A && key <= Qt::Key_Z)
			return 'A' + (key - Qt::Key_A);
		if (key >= Qt::Key_0 && key <= Qt::Key_9)
			return '0' + (key - Qt::Key_0);
		if (key >= Qt::Key_F1 && key <= Qt::Key_F24)
			return VK_F1 + (key - Qt::Key_F1);
		switch (key)
		{
			case Qt::Key_Backspace: return VK_BACK;
			case Qt::Key_Tab: case Qt::Key_Backtab: return VK_TAB;
			case Qt::Key_Clear: return VK_CLEAR;
			case Qt::Key_Return: case Qt::Key_Enter: return VK_RETURN;
			case Qt::Key_Shift: return VK_SHIFT;
			case Qt::Key_Control: return VK_CONTROL;
			case Qt::Key_Alt: return VK_MENU;
			case Qt::Key_Pause: return VK_PAUSE;
			case Qt::Key_CapsLock: return VK_CAPITAL;
			case Qt::Key_Escape: return VK_ESCAPE;
			case Qt::Key_Space: return VK_SPACE;
			case Qt::Key_PageUp: return VK_PRIOR;
			case Qt::Key_PageDown: return VK_NEXT;
			case Qt::Key_End: return VK_END;
			case Qt::Key_Home: return VK_HOME;
			case Qt::Key_Left: return VK_LEFT;
			case Qt::Key_Up: return VK_UP;
			case Qt::Key_Right: return VK_RIGHT;
			case Qt::Key_Down: return VK_DOWN;
			case Qt::Key_Print: return VK_SNAPSHOT;
			case Qt::Key_Insert: return VK_INSERT;
			case Qt::Key_Delete: return VK_DELETE;
			case Qt::Key_Help: return VK_HELP;
			case Qt::Key_Meta: return VK_LWIN;
			case Qt::Key_Menu: return VK_APPS;
			case Qt::Key_NumLock: return VK_NUMLOCK;
			case Qt::Key_ScrollLock: return VK_SCROLL;
			case Qt::Key_Asterisk: return VK_MULTIPLY;
			case Qt::Key_Semicolon: case Qt::Key_Colon: return VK_OEM_1;
			case Qt::Key_Plus: case Qt::Key_Equal: return VK_OEM_PLUS;
			case Qt::Key_Comma: case Qt::Key_Less: return VK_OEM_COMMA;
			case Qt::Key_Minus: case Qt::Key_Underscore: return VK_OEM_MINUS;
			case Qt::Key_Period: case Qt::Key_Greater: return VK_OEM_PERIOD;
			case Qt::Key_Slash: case Qt::Key_Question: return VK_OEM_2;
			case Qt::Key_QuoteLeft: case Qt::Key_AsciiTilde: return VK_OEM_3;
			case Qt::Key_BracketLeft: case Qt::Key_BraceLeft: return VK_OEM_4;
			case Qt::Key_Backslash: case Qt::Key_Bar: return VK_OEM_5;
			case Qt::Key_BracketRight: case Qt::Key_BraceRight: return VK_OEM_6;
			case Qt::Key_Apostrophe: case Qt::Key_QuoteDbl: return VK_OEM_7;
			default: break;
		}
		// shifted digits of a US keyboard
		static const char shifted[] = ")!@#$%^&*(";
		for (int i = 0; i < 10; ++i)
		{
			if (key == shifted[i])
				return '0' + i;
		}
		return static_cast<int>(nativeVk & 0xFF);
	}

	int qtKeyFromVk(int vk)
	{
		if (vk >= 'A' && vk <= 'Z')
			return Qt::Key_A + (vk - 'A');
		if (vk >= '0' && vk <= '9')
			return Qt::Key_0 + (vk - '0');
		if (vk >= VK_F1 && vk <= VK_F24)
			return Qt::Key_F1 + (vk - VK_F1);
		switch (vk)
		{
			case VK_BACK: return Qt::Key_Backspace;
			case VK_TAB: return Qt::Key_Tab;
			case VK_RETURN: return Qt::Key_Return;
			case VK_ESCAPE: return Qt::Key_Escape;
			case VK_SPACE: return Qt::Key_Space;
			case VK_PRIOR: return Qt::Key_PageUp;
			case VK_NEXT: return Qt::Key_PageDown;
			case VK_END: return Qt::Key_End;
			case VK_HOME: return Qt::Key_Home;
			case VK_LEFT: return Qt::Key_Left;
			case VK_UP: return Qt::Key_Up;
			case VK_RIGHT: return Qt::Key_Right;
			case VK_DOWN: return Qt::Key_Down;
			case VK_INSERT: return Qt::Key_Insert;
			case VK_DELETE: return Qt::Key_Delete;
			case VK_OEM_1: return Qt::Key_Semicolon;
			case VK_OEM_PLUS: return Qt::Key_Equal;
			case VK_OEM_COMMA: return Qt::Key_Comma;
			case VK_OEM_MINUS: return Qt::Key_Minus;
			case VK_OEM_PERIOD: return Qt::Key_Period;
			case VK_OEM_2: return Qt::Key_Slash;
			case VK_OEM_3: return Qt::Key_QuoteLeft;
			case VK_OEM_4: return Qt::Key_BracketLeft;
			case VK_OEM_5: return Qt::Key_Backslash;
			case VK_OEM_6: return Qt::Key_BracketRight;
			case VK_OEM_7: return Qt::Key_Apostrophe;
			case VK_ADD: return Qt::Key_Plus;
			case VK_SUBTRACT: return Qt::Key_Minus;
			case VK_MULTIPLY: return Qt::Key_Asterisk;
			case VK_DIVIDE: return Qt::Key_Slash;
			default: return 0;
		}
	}

	LPARAM keyLParam(int repeat, bool up, bool extended)
	{
		DWORD l = static_cast<DWORD>(repeat & 0xFFFF);
		if (extended)
			l |= 1u << 24;
		if (up)
			l |= (1u << 30) | (1u << 31);
		return static_cast<LPARAM>(l);
	}

	namespace
	{
		WPARAM mouseKeys(Qt::MouseButtons buttons, Qt::KeyboardModifiers mods)
		{
			WPARAM k = 0;
			if (buttons & Qt::LeftButton)
				k |= MK_LBUTTON;
			if (buttons & Qt::RightButton)
				k |= MK_RBUTTON;
			if (buttons & Qt::MiddleButton)
				k |= MK_MBUTTON;
			if (mods & Qt::ShiftModifier)
				k |= MK_SHIFT;
			if (mods & Qt::ControlModifier)
				k |= MK_CONTROL;
			return k;
		}

		LPARAM pointParam(const QPoint& p)
		{
			return MAKELPARAM(static_cast<short>(p.x()), static_cast<short>(p.y()));
		}

		UINT mouseMessage(QEvent::Type type, Qt::MouseButton b)
		{
			const int base = (b == Qt::RightButton) ? WM_RBUTTONDOWN : (b == Qt::MiddleButton ? WM_MBUTTONDOWN : WM_LBUTTONDOWN);
			switch (type)
			{
				case QEvent::MouseButtonPress: return static_cast<UINT>(base);
				case QEvent::MouseButtonRelease: return static_cast<UINT>(base + 1);
				case QEvent::MouseButtonDblClick: return static_cast<UINT>(base + 2);
				default: return WM_MOUSEMOVE;
			}
		}
	}

	// Qt events of a window as messages; returns true when the event is used
	bool translateEvent(Window* w, QEvent* e, bool isControl)
	{
		if (!w || w->destroyed)
			return false;
		HWND h = hwnd(w);
		switch (e->type())
		{
			case QEvent::KeyPress:
			case QEvent::ShortcutOverride:
			{
				auto* ke = static_cast<QKeyEvent*>(e);
				const int vk = vkFromQt(ke->key(), ke->nativeVirtualKey());
				if (e->type() == QEvent::ShortcutOverride)
				{
					// keys a window wants before the shortcuts of Notepad++ (WM_GETDLGCODE)
					if (!isControl && !w->isDialog && vk)
					{
						e->accept();
						return true;
					}
					return false;
				}
				const bool alt = ke->modifiers() & Qt::AltModifier;
				g_defaultReached = false;
				if (vk)
					dispatch(w, alt ? WM_SYSKEYDOWN : WM_KEYDOWN, static_cast<WPARAM>(vk), keyLParam(ke->isAutoRepeat() ? 2 : 1, false, false));
				bool used = !g_defaultReached;
				if (w->destroyed)
					return true;
				if (!used || !vk)
				{
					const QString text = ke->text();
					if (!text.isEmpty() && (text[0].isPrint() || text[0] == QLatin1Char('\r') || text[0] == QLatin1Char('\t')
						|| text[0] == QLatin1Char('\b') || text[0] == QChar(27) || (ke->modifiers() & Qt::ControlModifier)))
					{
						g_defaultReached = false;
						for (const QChar c : text)
							dispatch(w, alt ? WM_SYSCHAR : WM_CHAR, c.unicode(), keyLParam(1, false, false));
						used = !g_defaultReached;
					}
				}
				return used && (isControl || !w->isDialog);
			}
			case QEvent::KeyRelease:
			{
				auto* ke = static_cast<QKeyEvent*>(e);
				const int vk = vkFromQt(ke->key(), ke->nativeVirtualKey());
				if (!vk)
					return false;
				g_defaultReached = false;
				dispatch(w, (ke->modifiers() & Qt::AltModifier) ? WM_SYSKEYUP : WM_KEYUP, static_cast<WPARAM>(vk), keyLParam(1, true, false));
				return !g_defaultReached && isControl;
			}
			case QEvent::MouseButtonPress:
			case QEvent::MouseButtonRelease:
			case QEvent::MouseButtonDblClick:
			case QEvent::MouseMove:
			{
				auto* me = static_cast<QMouseEvent*>(e);
				QPoint pos = me->position().toPoint();
				if (w->widget && me->type() != QEvent::MouseMove)
				{
					// events of a viewport or of an inner widget: client coordinates of the window
					if (auto* src = qobject_cast<QWidget*>(static_cast<QObject*>(nullptr)))
						Q_UNUSED(src);
				}
				const UINT msg = mouseMessage(me->type(), me->button());
				const WPARAM keys = mouseKeys(me->buttons() | me->button(), me->modifiers());
				if (me->type() == QEvent::MouseMove && w->trackLeave)
					; // WM_MOUSELEAVE comes with QEvent::Leave
				g_defaultReached = false;
				dispatch(w, msg, keys, pointParam(pos));
				if (w->destroyed)
					return true;
				if (!isControl && me->type() == QEvent::MouseButtonRelease && me->button() == Qt::RightButton)
					return false; // QEvent::ContextMenu follows
				return !g_defaultReached && isControl;
			}
			case QEvent::Wheel:
			{
				auto* we = static_cast<QWheelEvent*>(e);
				const QPoint g = we->globalPosition().toPoint();
				const int dy = we->angleDelta().y();
				const int dx = we->angleDelta().x();
				g_defaultReached = false;
				if (dy)
					dispatch(w, WM_MOUSEWHEEL, MAKEWPARAM(mouseKeys(we->buttons(), we->modifiers()), static_cast<short>(dy)), pointParam(g));
				if (dx)
					dispatch(w, WM_MOUSEHWHEEL, MAKEWPARAM(mouseKeys(we->buttons(), we->modifiers()), static_cast<short>(-dx)), pointParam(g));
				return !g_defaultReached && isControl;
			}
			case QEvent::ContextMenu:
			{
				auto* ce = static_cast<QContextMenuEvent*>(e);
				const QPoint g = (ce->reason() == QContextMenuEvent::Keyboard) ? QPoint(-1, -1) : ce->globalPos();
				g_defaultReached = false;
				dispatch(w, WM_CONTEXTMENU, reinterpret_cast<WPARAM>(h), pointParam(g));
				return !g_defaultReached;
			}
			case QEvent::FocusIn:
				dispatch(w, WM_SETFOCUS, 0, 0);
				return false;
			case QEvent::FocusOut:
				dispatch(w, WM_KILLFOCUS, 0, 0);
				return false;
			case QEvent::Leave:
				if (w->trackLeave)
				{
					w->trackLeave = false;
					dispatch(w, WM_MOUSELEAVE, 0, 0);
				}
				return false;
			case QEvent::EnabledChange:
				if (w->widget)
					dispatch(w, WM_ENABLE, w->widget->isEnabled(), 0);
				return false;
			default:
				return false;
		}
	}

	// The widget of dialogs and of windows of registered classes
	class W32Widget : public QWidget
	{
	public:
		explicit W32Widget(QWidget* parent, Qt::WindowFlags f) : QWidget(parent, f)
		{
			setFocusPolicy(Qt::StrongFocus);
			setMouseTracking(true);
		}

		Window* window() const { return winOf(const_cast<W32Widget*>(this)); }

	protected:
		bool event(QEvent* e) override
		{
			Window* w = window();
			if (!w || w->destroyed)
				return QWidget::event(e);
			switch (e->type())
			{
				case QEvent::Paint:
				{
					QPainter p(this);
					const QRect r = static_cast<QPaintEvent*>(e)->rect();
					if (w->isDialog)
					{
						// the dialog background of the palette, unless WM_ERASEBKGND paints
						HDC dc = paintDC(w, &p, r);
						if (!dispatch(w, WM_ERASEBKGND, reinterpret_cast<WPARAM>(dc), 0))
							p.fillRect(r, palette().window());
						releasePaintDC(dc);
					}
					w->painter = &p;
					dispatch(w, WM_PAINT, 0, 0);
					w->painter = nullptr;
					return true;
				}
				case QEvent::Resize:
				{
					const QSize s = static_cast<QResizeEvent*>(e)->size();
					// the resize of the first show of a dialog: Windows has sent WM_SIZE before the
					// controls were made (plugins lay them out from their size at WM_INITDIALOG)
					if (s == w->lastSize)
						return true;
					w->lastSize = s;
					WPARAM type = SIZE_RESTORED;
					if (isMinimized())
						type = SIZE_MINIMIZED;
					else if (isMaximized())
						type = SIZE_MAXIMIZED;
					dispatch(w, WM_SIZE, type, MAKELPARAM(s.width(), s.height()));
					return true;
				}
				case QEvent::Move:
				{
					const QPoint p = static_cast<QMoveEvent*>(e)->pos();
					dispatch(w, WM_MOVE, 0, MAKELPARAM(static_cast<short>(p.x()), static_cast<short>(p.y())));
					return true;
				}
				case QEvent::Show:
					dispatch(w, WM_SHOWWINDOW, TRUE, 0);
					return QWidget::event(e);
				case QEvent::Hide:
					dispatch(w, WM_SHOWWINDOW, FALSE, 0);
					return QWidget::event(e);
				case QEvent::Close:
					if (!w->inDestroy && !w->ended)
					{
						// the close button: WM_CLOSE, the procedure decides (DefDlgProc: IDCANCEL)
						e->ignore();
						dispatch(w, WM_CLOSE, 0, 0);
						return true;
					}
					return QWidget::event(e);
				case QEvent::WindowActivate:
				case QEvent::WindowDeactivate:
					dispatch(w, WM_ACTIVATE, e->type() == QEvent::WindowActivate ? WA_ACTIVE : WA_INACTIVE, 0);
					return QWidget::event(e);
				case QEvent::DragEnter:
					if ((w->exStyle & WS_EX_ACCEPTFILES) && static_cast<QDragEnterEvent*>(e)->mimeData()->hasUrls())
					{
						static_cast<QDragEnterEvent*>(e)->acceptProposedAction();
						return true;
					}
					return QWidget::event(e);
				case QEvent::Drop:
					if (w->exStyle & WS_EX_ACCEPTFILES)
					{
						auto* de = static_cast<QDropEvent*>(e);
						QStringList files;
						for (const QUrl& u : de->mimeData()->urls())
						{
							if (u.isLocalFile())
								files << u.toLocalFile();
						}
						// HDROP: a DROPFILES like the one of Windows (DragQueryFile, also of CF_HDROP)
						HGLOBAL drop = makeDropFiles(files, POINT{de->position().toPoint().x(), de->position().toPoint().y()});
						dispatch(w, WM_DROPFILES, reinterpret_cast<WPARAM>(drop), 0);
						de->acceptProposedAction();
						return true;
					}
					return QWidget::event(e);
				case QEvent::KeyPress:
				{
					if (translateEvent(w, e, false))
						return true;
					if (w->destroyed)
						return true;
					if (w->isDialog)
					{
						// IsDialogMessage: Escape is IDCANCEL, Enter the default button
						auto* ke = static_cast<QKeyEvent*>(e);
						if (ke->key() == Qt::Key_Escape)
						{
							SendMessageW(hwnd(w), WM_COMMAND, MAKEWPARAM(IDCANCEL, BN_CLICKED), reinterpret_cast<LPARAM>(GetDlgItem(hwnd(w), IDCANCEL)));
							return true;
						}
						if ((ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) && ke->modifiers() == Qt::NoModifier)
						{
							const int id = LOWORD(SendMessageW(hwnd(w), DM_GETDEFID, 0, 0));
							HWND button = GetDlgItem(hwnd(w), id);
							if (!button || IsWindowEnabled(button))
								SendMessageW(hwnd(w), WM_COMMAND, MAKEWPARAM(id, BN_CLICKED), reinterpret_cast<LPARAM>(button));
							return true;
						}
					}
					return QWidget::event(e);
				}
				case QEvent::KeyRelease:
				case QEvent::ShortcutOverride:
				case QEvent::MouseButtonPress:
				case QEvent::MouseButtonRelease:
				case QEvent::MouseButtonDblClick:
				case QEvent::MouseMove:
				case QEvent::Wheel:
				case QEvent::ContextMenu:
				case QEvent::FocusIn:
				case QEvent::FocusOut:
				case QEvent::Leave:
				case QEvent::EnabledChange:
					if (e->type() == QEvent::MouseButtonPress && g_capture == nullptr && !w->isDialog)
						setFocus(Qt::MouseFocusReason);
					if (translateEvent(w, e, false))
						return true;
					return QWidget::event(e);
				default:
					return QWidget::event(e);
			}
		}
	};

	QWidget* newW32Widget(QWidget* parent, Qt::WindowFlags f)
	{
		return new W32Widget(parent, f);
	}

	// Events of controls: the messages for their subclasses (plugins catch Enter in an
	// edit box like this), and the default behaviour of Qt when the default procedure
	// is reached.
	class ControlFilter : public QObject
	{
	public:
		explicit ControlFilter(QObject* parent) : QObject(parent) {}

		bool eventFilter(QObject* obj, QEvent* e) override
		{
			Window* w = winOf(obj);
			if (!w || w->destroyed)
				return false;
			const bool hooked = !w->subclasses.empty() || w->wndProc != controlWndProc;
			// rich edits: EN_MSGFILTER of ENM_KEYEVENTS and ENM_MOUSEEVENTS, before the procedures
			if (w->kind == Kind::Edit && (w->eventMask & (ENM_KEYEVENTS | ENM_MOUSEEVENTS)) && richEditFilter(w, obj, e))
				return true;
			switch (e->type())
			{
				case QEvent::KeyPress:
				case QEvent::KeyRelease:
					return hooked && translateEvent(w, e, true);
				case QEvent::MouseButtonPress:
				case QEvent::MouseButtonRelease:
				case QEvent::MouseButtonDblClick:
				case QEvent::MouseMove:
				case QEvent::Wheel:
					return hooked && translateEvent(w, e, true);
				case QEvent::ContextMenu:
				{
					// WM_CONTEXTMENU: the control, then its parent (DefWindowProc)
					auto* ce = static_cast<QContextMenuEvent*>(e);
					const QPoint g = (ce->reason() == QContextMenuEvent::Keyboard) ? QPoint(-1, -1) : ce->globalPos();
					g_defaultReached = false;
					dispatch(w, WM_CONTEXTMENU, reinterpret_cast<WPARAM>(hwnd(w)), MAKELPARAM(static_cast<short>(g.x()), static_cast<short>(g.y())));
					return !g_defaultReached;
				}
				case QEvent::FocusIn:
					if (hooked)
						dispatch(w, WM_SETFOCUS, 0, 0);
					switch (w->kind)
					{
						case Kind::Edit: notifyCommand(w, EN_SETFOCUS); break;
						case Kind::ComboBox: notifyCommand(w, CBN_SETFOCUS); break;
						case Kind::ListBox: if (w->style & LBS_NOTIFY) notifyCommand(w, LBN_SETFOCUS); break;
						case Kind::Button: if (w->style & BS_NOTIFY) notifyCommand(w, BN_SETFOCUS); break;
						case Kind::ListView: case Kind::TreeView: { NMHDR hdr{}; notify(w, &hdr, NM_SETFOCUS); break; }
						default: break;
					}
					return false;
				case QEvent::FocusOut:
					if (hooked)
						dispatch(w, WM_KILLFOCUS, 0, 0);
					switch (w->kind)
					{
						case Kind::Edit: notifyCommand(w, EN_KILLFOCUS); break;
						case Kind::ComboBox: notifyCommand(w, CBN_KILLFOCUS); break;
						case Kind::ListBox: if (w->style & LBS_NOTIFY) notifyCommand(w, LBN_KILLFOCUS); break;
						case Kind::Button: if (w->style & BS_NOTIFY) notifyCommand(w, BN_KILLFOCUS); break;
						case Kind::ListView: case Kind::TreeView: { NMHDR hdr{}; notify(w, &hdr, NM_KILLFOCUS); break; }
						default: break;
					}
					return false;
				default:
					return false;
			}
		}
	};

	void installControlFilter(Window* w, QObject* target)
	{
		if (!w || !w->widget)
			return;
		static ControlFilter* filter = new ControlFilter(qApp);
		QObject* t = target ? target : w->widget.data();
		t->installEventFilter(filter);
		if (!target)
		{
			if (auto* area = qobject_cast<QAbstractScrollArea*>(w->widget.data()))
				area->viewport()->installEventFilter(filter);
		}
	}
}

namespace w32
{
	bool isGuiThread()
	{
		return !qApp || QThread::currentThread() == qApp->thread();
	}
}

using namespace w32;

bool npp_isGuiThread()
{
	return isGuiThread();
}

LRESULT npp_sendFromThread(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	LRESULT r = 0;
	QMetaObject::invokeMethod(qApp, [&] { r = hWnd->send(hWnd, msg, wParam, lParam); }, Qt::BlockingQueuedConnection);
	return r;
}

// ---- messages ------------------------------------------------------------------------------
LRESULT SendMessageA(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
		case WM_SETTEXT:
		{
			const std::wstring text = ws(qsA(reinterpret_cast<LPCSTR>(lParam)));
			return SendMessageW(hWnd, msg, wParam, reinterpret_cast<LPARAM>(text.c_str()));
		}
		case WM_GETTEXT:
			return GetWindowTextA(hWnd, reinterpret_cast<LPSTR>(lParam), static_cast<int>(wParam));
		default:
			return SendMessageW(hWnd, msg, wParam, lParam);
	}
}

BOOL PostMessageW(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (!hWnd || hWnd->magic != NPP_HANDLE_MAGIC)
		return FALSE;
	if (!win(hWnd))
	{
		// a handle of the host: its own post
		if (hWnd->post)
			return hWnd->post(hWnd, msg, wParam, lParam);
		return FALSE;
	}
	QWidget* widget = widgetOf(hWnd);
	QPointer<QWidget> guard = widget;
	QMetaObject::invokeMethod(qApp, [hWnd, msg, wParam, lParam, guard]
	{
		if (guard && win(hWnd))
			SendMessageW(hWnd, msg, wParam, lParam);
	}, Qt::QueuedConnection);
	return TRUE;
}

LRESULT SendNotifyMessageW(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	return SendMessageW(hWnd, msg, wParam, lParam);
}

LRESULT SendMessageTimeoutW(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam, UINT, UINT, PDWORD_PTR result)
{
	const LRESULT r = SendMessageW(hWnd, msg, wParam, lParam);
	if (result)
		*result = static_cast<DWORD_PTR>(r);
	return TRUE;
}

LRESULT SendDlgItemMessageW(HWND hDlg, int id, UINT msg, WPARAM wParam, LPARAM lParam)
{
	return SendMessageW(GetDlgItem(hDlg, id), msg, wParam, lParam);
}

LRESULT SendDlgItemMessageA(HWND hDlg, int id, UINT msg, WPARAM wParam, LPARAM lParam)
{
	return SendMessageA(GetDlgItem(hDlg, id), msg, wParam, lParam);
}

BOOL PostThreadMessageW(DWORD, UINT, WPARAM, LPARAM)
{
	return FALSE;
}

void PostQuitMessage(int)
{
}

BOOL GetMessageW(LPMSG msg, HWND, UINT, UINT)
{
	// message loops of plugins: let Qt run, never a WM_QUIT
	QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents);
	if (msg)
		*msg = MSG{};
	return TRUE;
}

BOOL PeekMessageW(LPMSG msg, HWND, UINT, UINT, UINT)
{
	QCoreApplication::processEvents();
	if (msg)
		*msg = MSG{};
	return FALSE;
}

BOOL TranslateMessage(const MSG*)
{
	return FALSE;
}

LRESULT DispatchMessageW(const MSG* msg)
{
	return (msg && msg->hwnd) ? SendMessageW(msg->hwnd, msg->message, msg->wParam, msg->lParam) : 0;
}

BOOL IsDialogMessageW(HWND, LPMSG)
{
	return FALSE;
}

int TranslateAcceleratorW(HWND, HACCEL, LPMSG)
{
	return 0;
}

DWORD GetMessagePos()
{
	const QPoint p = QCursor::pos();
	return static_cast<DWORD>(MAKELONG(static_cast<short>(p.x()), static_cast<short>(p.y())));
}

LONG GetMessageTime()
{
	static QElapsedTimer t;
	if (!t.isValid())
		t.start();
	return static_cast<LONG>(t.elapsed());
}

BOOL WaitMessage()
{
	QCoreApplication::processEvents(QEventLoop::WaitForMoreEvents);
	return TRUE;
}

UINT RegisterWindowMessageW(LPCWSTR name)
{
	static std::map<std::wstring, UINT> messages;
	const std::wstring key = name ? name : L"";
	auto it = messages.find(key);
	if (it != messages.end())
		return it->second;
	const UINT id = 0xC000 + static_cast<UINT>(messages.size());
	messages[key] = id;
	return id;
}

// ---- classes and windows ---------------------------------------------------------------------
ATOM RegisterClassExW(const WNDCLASSEXW* wc)
{
	if (!wc || !wc->lpszClassName)
		return 0;
	std::wstring name = isIntResource(wc->lpszClassName) ? std::to_wstring(intResource(wc->lpszClassName)) : std::wstring(wc->lpszClassName);
	WindowClass c;
	c.name = name;
	c.proc = wc->lpfnWndProc;
	c.style = wc->style;
	c.cbWndExtra = wc->cbWndExtra;
	c.background = wc->hbrBackground;
	c.cursor = wc->hCursor;
	c.icon = wc->hIcon;
	classes()[lower(name)] = c;
	static ATOM next = 0xC100;
	return next++;
}

ATOM RegisterClassW(const WNDCLASSW* wc)
{
	if (!wc)
		return 0;
	WNDCLASSEXW ex{};
	ex.cbSize = sizeof(ex);
	ex.style = wc->style;
	ex.lpfnWndProc = wc->lpfnWndProc;
	ex.cbClsExtra = wc->cbClsExtra;
	ex.cbWndExtra = wc->cbWndExtra;
	ex.hInstance = wc->hInstance;
	ex.hIcon = wc->hIcon;
	ex.hCursor = wc->hCursor;
	ex.hbrBackground = wc->hbrBackground;
	ex.lpszMenuName = wc->lpszMenuName;
	ex.lpszClassName = wc->lpszClassName;
	return RegisterClassExW(&ex);
}

BOOL UnregisterClassW(LPCWSTR className, HINSTANCE)
{
	if (!className || isIntResource(className))
		return FALSE;
	return classes().erase(lower(className)) ? TRUE : FALSE;
}

BOOL GetClassInfoExW(HINSTANCE, LPCWSTR className, WNDCLASSEXW* wc)
{
	if (!className || isIntResource(className))
		return FALSE;
	auto it = classes().find(lower(className));
	if (it == classes().end())
		return FALSE;
	if (wc)
	{
		*wc = WNDCLASSEXW{};
		wc->cbSize = sizeof(*wc);
		wc->lpfnWndProc = it->second.proc;
		wc->style = it->second.style;
		wc->cbWndExtra = it->second.cbWndExtra;
		wc->hbrBackground = it->second.background;
		wc->hCursor = it->second.cursor;
		wc->lpszClassName = className;
	}
	return TRUE;
}

namespace w32
{
	QWidget* newW32Widget(QWidget* parent, Qt::WindowFlags f);
}

HWND CreateWindowExW(DWORD exStyle, LPCWSTR className, LPCWSTR windowName, DWORD style, int x, int y, int width, int height,
	HWND parent, HMENU menu, HINSTANCE, LPVOID param)
{
	const std::wstring cls = (!className) ? L"" : (isIntResource(className) ? std::to_wstring(intResource(className)) : std::wstring(className));
	const QString title = qs(windowName);
	const bool child = (style & WS_CHILD) != 0;
	QWidget* parentWidget = child ? parentWidgetOf(parent) : (parent ? widgetOf(parent) : widgetOf(nppHandle()));
	if (parentWidget && !child)
		parentWidget = parentWidget->window();
	const int id = child ? static_cast<int>(reinterpret_cast<intptr_t>(menu)) : 0;

	auto it = classes().find(lower(cls));
	Window* w = nullptr;
	if (it != classes().end())
	{
		// a class of the plugin: its procedure gets the messages
		Qt::WindowFlags f = child ? Qt::Widget : Qt::Window;
		if (!child && (style & WS_POPUP) && !(style & WS_CAPTION))
			f = Qt::Tool | Qt::FramelessWindowHint;
		else if (!child && (exStyle & WS_EX_TOOLWINDOW))
			f = Qt::Tool;
		QWidget* widget = newW32Widget(parentWidget, f);
		w = newWindow(Kind::Custom, widget, parent, id, style, exStyle, cls);
		w->wndProc = it->second.proc;
		w->classProc = it->second.proc;
		w->background = it->second.background;
		w->cursor = it->second.cursor;
		w->extra.assign(static_cast<size_t>(std::max(0, it->second.cbWndExtra)), 0);
		if (!child && menu)
			w->menu = menu;
		widget->setWindowTitle(title);
		if (exStyle & WS_EX_ACCEPTFILES)
			widget->setAcceptDrops(true);
	}
	else
	{
		Window* ctl = nullptr;
		QWidget* widget = createControl(kindOfClass(cls), cls, style, parentWidget, title, -1, &ctl);
		if (!widget)
		{
			SetLastError(1407); // ERROR_CANNOT_FIND_WND_CLASS
			return nullptr;
		}
		w = ctl;
		w->parent = parent;
		w->id = id;
		w->style = style;
		w->exStyle = exStyle;
	}
	QWidget* widget = w->widget;
	if (x == CW_USEDEFAULT)
		x = 0;
	if (y == CW_USEDEFAULT)
		y = 0;
	if (width == CW_USEDEFAULT || width <= 0)
		width = child ? 0 : 400;
	if (height == CW_USEDEFAULT || height <= 0)
		height = child ? 0 : 300;
	widget->setGeometry(x, y, std::max(width, 0), std::max(height, 0));
	if (style & WS_DISABLED)
		widget->setEnabled(false);

	CREATESTRUCTW cs{};
	cs.lpCreateParams = param;
	cs.hMenu = menu;
	cs.hwndParent = parent;
	cs.cx = width;
	cs.cy = height;
	cs.x = x;
	cs.y = y;
	cs.style = static_cast<LONG>(style);
	cs.lpszName = windowName;
	cs.lpszClass = className;
	cs.dwExStyle = exStyle;
	HWND h = hwnd(w);
	if (w->kind == Kind::Custom)
	{
		if (!dispatch(w, WM_NCCREATE, 0, reinterpret_cast<LPARAM>(&cs)) && w->wndProc != DefWindowProcW)
		{
			// a procedure that returns FALSE to WM_NCCREATE: no window (some return 0 from DefWindowProc too)
		}
		if (dispatch(w, WM_CREATE, 0, reinterpret_cast<LPARAM>(&cs)) == -1)
		{
			destroyWindow(w);
			return nullptr;
		}
	}
	if (w->destroyed)
		return nullptr;
	if (style & WS_VISIBLE)
		widget->show();
	else if (!widget->isWindow())
		widget->hide(); // explicitly: Qt shows the children of a parent that is shown later, Windows does not
	return h;
}

HWND CreateWindowExA(DWORD exStyle, LPCSTR className, LPCSTR windowName, DWORD style, int x, int y, int width, int height,
	HWND parent, HMENU menu, HINSTANCE hInst, LPVOID param)
{
	const std::wstring cls = className ? (isIntResource(className) ? std::to_wstring(intResource(className)) : ws(qsA(className))) : std::wstring();
	const std::wstring name = ws(qsA(windowName));
	return CreateWindowExW(exStyle, cls.c_str(), name.c_str(), style, x, y, width, height, parent, menu, hInst, param);
}

BOOL DestroyWindow(HWND hWnd)
{
	if (!isGuiThread())
		return onGui([=] { return DestroyWindow(hWnd); });
	Window* w = win(hWnd);
	if (!w || w->destroyed)
		return FALSE;
	destroyWindow(w);
	return TRUE;
}

BOOL IsWindow(HWND hWnd)
{
	if (!hWnd || hWnd->magic != NPP_HANDLE_MAGIC)
		return FALSE;
	if (Window* w = win(hWnd))
		return !w->destroyed && w->widget;
	return hWnd->widget != nullptr;
}

BOOL IsWindowVisible(HWND hWnd)
{
	QWidget* w = widgetOf(hWnd);
	return (w && w->isVisible()) ? TRUE : FALSE;
}

BOOL IsWindowEnabled(HWND hWnd)
{
	QWidget* w = widgetOf(hWnd);
	return (w && w->isEnabled()) ? TRUE : FALSE;
}

BOOL IsIconic(HWND hWnd)
{
	QWidget* w = widgetOf(hWnd);
	return (w && w->window()->isMinimized()) ? TRUE : FALSE;
}

BOOL IsZoomed(HWND hWnd)
{
	QWidget* w = widgetOf(hWnd);
	return (w && w->window()->isMaximized()) ? TRUE : FALSE;
}

BOOL IsChild(HWND parent, HWND hWnd)
{
	QWidget* p = widgetOf(parent);
	QWidget* c = widgetOf(hWnd);
	return (p && c && p != c && p->isAncestorOf(c)) ? TRUE : FALSE;
}

BOOL ShowWindow(HWND hWnd, int cmd)
{
	if (!isGuiThread())
		return onGui([=] { return ShowWindow(hWnd, cmd); });
	QWidget* w = widgetOf(hWnd);
	if (!w)
		return FALSE;
	const bool was = w->isVisible();
	switch (cmd)
	{
		case SW_HIDE:
			w->hide();
			break;
		case SW_MINIMIZE:
		case SW_SHOWMINIMIZED:
		case SW_SHOWMINNOACTIVE:
		case SW_FORCEMINIMIZE:
			w->showMinimized();
			break;
		case SW_MAXIMIZE:
			w->showMaximized();
			break;
		case SW_RESTORE:
			w->showNormal();
			break;
		case SW_SHOWNOACTIVATE:
		case SW_SHOWNA:
			w->setAttribute(Qt::WA_ShowWithoutActivating, true);
			w->show();
			w->setAttribute(Qt::WA_ShowWithoutActivating, false);
			break;
		default:
			w->show();
			if (w->isWindow())
			{
				w->raise();
				w->activateWindow();
			}
			break;
	}
	return was ? TRUE : FALSE;
}

BOOL ShowWindowAsync(HWND hWnd, int cmd)
{
	return ShowWindow(hWnd, cmd);
}

BOOL EnableWindow(HWND hWnd, BOOL enable)
{
	if (!isGuiThread())
		return onGui([=] { return EnableWindow(hWnd, enable); });
	QWidget* w = widgetOf(hWnd);
	if (!w)
		return FALSE;
	const bool wasDisabled = !w->isEnabled();
	w->setEnabled(enable != FALSE);
	return wasDisabled ? TRUE : FALSE;
}

BOOL UpdateWindow(HWND hWnd)
{
	if (!isGuiThread())
		return onGui([=] { return UpdateWindow(hWnd); });
	if (QWidget* w = widgetOf(hWnd))
		w->repaint();
	return TRUE;
}

BOOL InvalidateRect(HWND hWnd, const RECT* rc, BOOL)
{
	if (!isGuiThread())
		return onGui([=] { return InvalidateRect(hWnd, rc, TRUE); });
	QWidget* w = widgetOf(hWnd);
	if (!w)
		return FALSE;
	if (Window* win32 = win(hWnd); win32 && !win32->redraw)
		return TRUE;
	if (rc)
		w->update(QRect(rc->left, rc->top, rc->right - rc->left, rc->bottom - rc->top));
	else
		w->update();
	if (auto* area = qobject_cast<QAbstractScrollArea*>(w))
		area->viewport()->update();
	return TRUE;
}

BOOL ValidateRect(HWND, const RECT*)
{
	return TRUE;
}

BOOL InvalidateRgn(HWND hWnd, HRGN, BOOL erase)
{
	return InvalidateRect(hWnd, nullptr, erase);
}

BOOL RedrawWindow(HWND hWnd, const RECT* rc, HRGN, UINT flags)
{
	if (!isGuiThread())
		return onGui([=] { return RedrawWindow(hWnd, rc, nullptr, flags); });
	QWidget* w = widgetOf(hWnd);
	if (!w)
		return FALSE;
	if (flags & (RDW_INVALIDATE | RDW_ERASE | RDW_FRAME))
		InvalidateRect(hWnd, rc, TRUE);
	if (flags & (RDW_UPDATENOW | RDW_ERASENOW))
		w->repaint();
	if (flags & RDW_ALLCHILDREN)
	{
		for (QWidget* c : w->findChildren<QWidget*>())
			c->update();
	}
	return TRUE;
}

namespace
{
	// rebars and toolbars without CCS_NORESIZE keep the height of their content (Windows: "auto height")
	int ownHeight(HWND hWnd, int height)
	{
		Window* w = win(hWnd);
		if (!w || !w->widget || (w->kind != Kind::Rebar && w->kind != Kind::Toolbar) || (w->style & CCS_NORESIZE))
			return height;
		return std::max(w->widget->sizeHint().height(), w->widget->minimumHeight());
	}
}

BOOL MoveWindow(HWND hWnd, int x, int y, int width, int height, BOOL)
{
	if (!isGuiThread())
		return onGui([=] { return MoveWindow(hWnd, x, y, width, height, TRUE); });
	QWidget* w = widgetOf(hWnd);
	if (!w)
		return FALSE;
	if (w->isWindow())
	{
		w->move(x, y);
		w->resize(std::max(width, 0), std::max(height, 0));
	}
	else
	{
		w->setGeometry(x, y, std::max(width, 0), std::max(ownHeight(hWnd, height), 0));
	}
	return TRUE;
}

BOOL SetWindowPos(HWND hWnd, HWND insertAfter, int x, int y, int cx, int cy, UINT flags)
{
	if (!isGuiThread())
		return onGui([=] { return SetWindowPos(hWnd, insertAfter, x, y, cx, cy, flags); });
	QWidget* w = widgetOf(hWnd);
	if (!w)
		return FALSE;
	if (!(flags & SWP_NOMOVE))
		w->move(x, y);
	if (!(flags & SWP_NOSIZE))
		w->resize(std::max(cx, 0), std::max(ownHeight(hWnd, cy), 0));
	if (!(flags & SWP_NOZORDER))
	{
		if (insertAfter == HWND_TOPMOST || insertAfter == HWND_NOTOPMOST)
		{
			if (w->isWindow())
			{
				const bool visible = w->isVisible();
				w->setWindowFlag(Qt::WindowStaysOnTopHint, insertAfter == HWND_TOPMOST);
				if (visible)
					w->show();
			}
		}
		else if (insertAfter == HWND_TOP)
		{
			w->raise();
		}
		else if (insertAfter == HWND_BOTTOM)
		{
			w->lower();
		}
		else if (QWidget* after = widgetOf(insertAfter))
		{
			w->stackUnder(after);
		}
	}
	if (flags & SWP_SHOWWINDOW)
		w->show();
	if (flags & SWP_HIDEWINDOW)
		w->hide();
	if (flags & SWP_FRAMECHANGED)
		w->update();
	return TRUE;
}

HDWP BeginDeferWindowPos(int)
{
	return reinterpret_cast<HDWP>(1);
}

HDWP DeferWindowPos(HDWP h, HWND hWnd, HWND insertAfter, int x, int y, int cx, int cy, UINT flags)
{
	SetWindowPos(hWnd, insertAfter, x, y, cx, cy, flags);
	return h;
}

BOOL EndDeferWindowPos(HDWP)
{
	return TRUE;
}

BOOL GetWindowRect(HWND hWnd, LPRECT rc)
{
	QWidget* w = widgetOf(hWnd);
	if (!w || !rc)
		return FALSE;
	QRect r;
	if (w->isWindow())
		r = w->frameGeometry();
	else
		r = QRect(w->mapToGlobal(QPoint(0, 0)), w->size());
	rc->left = r.left();
	rc->top = r.top();
	rc->right = r.left() + r.width();
	rc->bottom = r.top() + r.height();
	return TRUE;
}

BOOL GetClientRect(HWND hWnd, LPRECT rc)
{
	QWidget* w = widgetOf(hWnd);
	if (!w || !rc)
		return FALSE;
	rc->left = 0;
	rc->top = 0;
	rc->right = w->width();
	rc->bottom = w->height();
	return TRUE;
}

BOOL ClientToScreen(HWND hWnd, LPPOINT pt)
{
	QWidget* w = widgetOf(hWnd);
	if (!w || !pt)
		return FALSE;
	const QPoint g = w->mapToGlobal(QPoint(pt->x, pt->y));
	pt->x = g.x();
	pt->y = g.y();
	return TRUE;
}

BOOL ScreenToClient(HWND hWnd, LPPOINT pt)
{
	QWidget* w = widgetOf(hWnd);
	if (!w || !pt)
		return FALSE;
	const QPoint c = w->mapFromGlobal(QPoint(pt->x, pt->y));
	pt->x = c.x();
	pt->y = c.y();
	return TRUE;
}

int MapWindowPoints(HWND from, HWND to, LPPOINT pts, UINT count)
{
	QWidget* f = widgetOf(from);
	QWidget* t = widgetOf(to);
	QPoint offset(0, 0);
	if (f)
		offset = f->mapToGlobal(QPoint(0, 0));
	if (t)
		offset -= t->mapToGlobal(QPoint(0, 0));
	for (UINT i = 0; pts && i < count; ++i)
	{
		pts[i].x += offset.x();
		pts[i].y += offset.y();
	}
	return MAKELONG(static_cast<short>(offset.x()), static_cast<short>(offset.y()));
}

BOOL GetWindowPlacement(HWND hWnd, WINDOWPLACEMENT* wp)
{
	QWidget* w = widgetOf(hWnd);
	if (!w || !wp)
		return FALSE;
	const QRect r = w->isWindow() ? w->normalGeometry() : w->geometry();
	wp->flags = 0;
	wp->showCmd = w->isMinimized() ? SW_SHOWMINIMIZED : (w->isMaximized() ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL);
	wp->rcNormalPosition = RECT{r.left(), r.top(), r.left() + r.width(), r.top() + r.height()};
	return TRUE;
}

BOOL SetWindowPlacement(HWND hWnd, const WINDOWPLACEMENT* wp)
{
	QWidget* w = widgetOf(hWnd);
	if (!w || !wp)
		return FALSE;
	const RECT& r = wp->rcNormalPosition;
	w->setGeometry(r.left, r.top, r.right - r.left, r.bottom - r.top);
	ShowWindow(hWnd, static_cast<int>(wp->showCmd));
	return TRUE;
}

HWND GetParent(HWND hWnd)
{
	if (Window* w = win(hWnd))
		return (w->style & WS_CHILD) ? w->parent : (w->owner ? w->owner : w->parent);
	return nullptr;
}

HWND SetParent(HWND child, HWND parent)
{
	Window* w = win(child);
	QWidget* widget = widgetOf(child);
	if (!widget)
		return nullptr;
	HWND old = w ? w->parent : nullptr;
	QWidget* p = parent ? widgetOf(parent) : nullptr;
	const bool visible = widget->isVisible();
	widget->setParent(p, p ? Qt::Widget : Qt::Window);
	if (visible)
		widget->show();
	if (w)
	{
		w->parent = parent;
		if (p)
			w->style = (w->style & ~WS_POPUP) | WS_CHILD;
	}
	return old;
}

HWND GetAncestor(HWND hWnd, UINT flags)
{
	if (flags == GA_PARENT)
		return GetParent(hWnd);
	HWND h = hWnd;
	while (Window* w = win(h))
	{
		if (!(w->style & WS_CHILD) || !w->parent)
			break;
		h = w->parent;
	}
	return h;
}

HWND GetWindow(HWND hWnd, UINT cmd)
{
	QWidget* w = widgetOf(hWnd);
	if (!w)
		return nullptr;
	auto handleOf = [](QWidget* x) -> HWND
	{
		if (!x)
			return nullptr;
		const QVariant v = x->property("_nppHandle");
		return v.isValid() ? static_cast<HWND>(v.value<void*>()) : nullptr;
	};
	if (cmd == GW_CHILD)
	{
		for (QObject* o : w->children())
		{
			if (HWND c = handleOf(qobject_cast<QWidget*>(o)))
				return c;
		}
		return nullptr;
	}
	if (cmd == GW_OWNER)
		return GetParent(hWnd);
	QWidget* p = w->parentWidget();
	if (!p)
		return nullptr;
	QList<QWidget*> siblings;
	for (QObject* o : p->children())
	{
		if (auto* s = qobject_cast<QWidget*>(o); s && handleOf(s))
			siblings << s;
	}
	const int i = static_cast<int>(siblings.indexOf(w));
	switch (cmd)
	{
		case GW_HWNDFIRST: return siblings.isEmpty() ? nullptr : handleOf(siblings.first());
		case GW_HWNDLAST: return siblings.isEmpty() ? nullptr : handleOf(siblings.last());
		case GW_HWNDNEXT: return (i >= 0 && i + 1 < siblings.size()) ? handleOf(siblings[i + 1]) : nullptr;
		case GW_HWNDPREV: return (i > 0) ? handleOf(siblings[i - 1]) : nullptr;
		default: return nullptr;
	}
}

HWND GetTopWindow(HWND hWnd)
{
	return GetWindow(hWnd, GW_CHILD);
}

HWND GetDesktopWindow()
{
	return nullptr;
}

HWND GetForegroundWindow()
{
	QWidget* w = QApplication::activeWindow();
	if (!w)
		return nullptr;
	const QVariant v = w->property("_nppHandle");
	if (v.isValid())
		return static_cast<HWND>(v.value<void*>());
	const QVariant m = w->property("_nppMainHandle");
	return m.isValid() ? static_cast<HWND>(m.value<void*>()) : nullptr;
}

BOOL SetForegroundWindow(HWND hWnd)
{
	QWidget* w = widgetOf(hWnd);
	if (!w)
		return FALSE;
	w->window()->raise();
	w->window()->activateWindow();
	return TRUE;
}

HWND GetActiveWindow()
{
	return GetForegroundWindow();
}

HWND SetActiveWindow(HWND hWnd)
{
	HWND old = GetActiveWindow();
	SetForegroundWindow(hWnd);
	return old;
}

BOOL GetGUIThreadInfo(DWORD, PGUITHREADINFO info)
{
	if (!info)
		return FALSE;
	const DWORD size = info->cbSize;
	*info = GUITHREADINFO{};
	info->cbSize = size;
	info->hwndActive = GetActiveWindow();
	info->hwndFocus = GetFocus();
	info->hwndCaret = info->hwndFocus;
	POINT p{};
	if (GetCaretPos(&p))
		info->rcCaret = RECT{p.x, p.y, p.x + 1, p.y + 16};
	return TRUE;
}

BOOL GetCaretPos(LPPOINT point)
{
	// the caret of the focused text box, in its client coordinates
	if (!point)
		return FALSE;
	*point = POINT{};
	QWidget* f = QApplication::focusWidget();
	QRect r;
	if (auto* pe = qobject_cast<QPlainTextEdit*>(f))
		r = pe->cursorRect().translated(pe->viewport()->mapTo(pe, QPoint()));
	else if (auto* le = qobject_cast<QLineEdit*>(f))
		r = le->inputMethodQuery(Qt::ImCursorRectangle).toRect();
	else
		return FALSE;
	point->x = r.left();
	point->y = r.top();
	return TRUE;
}

HWND GetFocus()
{
	for (QWidget* w = QApplication::focusWidget(); w; w = w->parentWidget())
	{
		const QVariant v = w->property("_nppHandle");
		if (v.isValid())
			return static_cast<HWND>(v.value<void*>());
		const QVariant m = w->property("_nppMainHandle");
		if (m.isValid())
			return static_cast<HWND>(m.value<void*>());
	}
	return nullptr;
}

HWND SetFocus(HWND hWnd)
{
	if (!isGuiThread())
		return onGui([=] { return SetFocus(hWnd); });
	HWND old = GetFocus();
	if (QWidget* w = widgetOf(hWnd))
	{
		if (auto* proxy = w->focusProxy())
			proxy->setFocus(Qt::OtherFocusReason);
		else
			w->setFocus(Qt::OtherFocusReason);
	}
	return old;
}

HWND GetCapture()
{
	return g_capture;
}

HWND SetCapture(HWND hWnd)
{
	HWND old = g_capture;
	g_capture = hWnd;
	if (QWidget* w = widgetOf(hWnd))
		w->grabMouse();
	return old;
}

BOOL ReleaseCapture()
{
	if (QWidget* w = widgetOf(g_capture))
		w->releaseMouse();
	HWND old = g_capture;
	g_capture = nullptr;
	if (Window* w = win(old))
		dispatch(w, WM_CAPTURECHANGED, 0, 0);
	return TRUE;
}

BOOL BringWindowToTop(HWND hWnd)
{
	return SetForegroundWindow(hWnd);
}

HWND WindowFromPoint(POINT pt)
{
	for (QWidget* w = QApplication::widgetAt(QPoint(pt.x, pt.y)); w; w = w->parentWidget())
	{
		const QVariant v = w->property("_nppHandle");
		if (v.isValid())
			return static_cast<HWND>(v.value<void*>());
	}
	return nullptr;
}

HWND ChildWindowFromPoint(HWND parent, POINT pt)
{
	QWidget* p = widgetOf(parent);
	if (!p)
		return nullptr;
	for (QWidget* w = p->childAt(QPoint(pt.x, pt.y)); w && w != p; w = w->parentWidget())
	{
		const QVariant v = w->property("_nppHandle");
		if (v.isValid())
			return static_cast<HWND>(v.value<void*>());
	}
	return parent;
}

HWND FindWindowW(LPCWSTR className, LPCWSTR windowName)
{
	return FindWindowExW(nullptr, nullptr, className, windowName);
}

HWND FindWindowExW(HWND parent, HWND after, LPCWSTR className, LPCWSTR windowName)
{
	QList<QWidget*> list;
	if (QWidget* p = widgetOf(parent))
	{
		for (QObject* o : p->children())
		{
			if (auto* w = qobject_cast<QWidget*>(o))
				list << w;
		}
	}
	else
	{
		list = QApplication::topLevelWidgets();
	}
	bool past = (after == nullptr);
	for (QWidget* w : list)
	{
		const QVariant v = w->property("_nppHandle");
		if (!v.isValid())
			continue;
		HWND h = static_cast<HWND>(v.value<void*>());
		if (!past)
		{
			past = (h == after);
			continue;
		}
		Window* x = win(h);
		if (className && !isIntResource(className) && (!x || lower(x->className) != lower(className)))
			continue;
		if (windowName && w->windowTitle() != qs(windowName))
			continue;
		return h;
	}
	return nullptr;
}

BOOL EnumChildWindows(HWND parent, WNDENUMPROC proc, LPARAM lParam)
{
	QWidget* p = widgetOf(parent);
	if (!p || !proc)
		return FALSE;
	for (QWidget* w : p->findChildren<QWidget*>())
	{
		const QVariant v = w->property("_nppHandle");
		if (!v.isValid() || winOf(w) == nullptr || winOf(w)->widget != w)
			continue;
		if (!proc(static_cast<HWND>(v.value<void*>()), lParam))
			break;
	}
	return TRUE;
}

BOOL EnumWindows(WNDENUMPROC proc, LPARAM lParam)
{
	if (!proc)
		return FALSE;
	for (QWidget* w : QApplication::topLevelWidgets())
	{
		QVariant v = w->property("_nppHandle");
		if (!v.isValid())
			v = w->property("_nppMainHandle");
		if (v.isValid() && !proc(static_cast<HWND>(v.value<void*>()), lParam))
			break;
	}
	return TRUE;
}

BOOL EnumThreadWindows(DWORD, WNDENUMPROC proc, LPARAM lParam)
{
	return EnumWindows(proc, lParam);
}

DWORD GetWindowThreadProcessId(HWND, LPDWORD pid)
{
	if (pid)
		*pid = static_cast<DWORD>(getpid());
	return GetCurrentThreadId();
}

BOOL SetWindowTextW(HWND hWnd, LPCWSTR text)
{
	if (!isGuiThread())
		return onGui([=] { return SetWindowTextW(hWnd, text); });
	if (win(hWnd))
		return static_cast<BOOL>(SendMessageW(hWnd, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(text)));
	if (QWidget* w = widgetOf(hWnd))
		w->setWindowTitle(qs(text));
	return TRUE;
}

BOOL SetWindowTextA(HWND hWnd, LPCSTR text)
{
	const std::wstring t = ws(qsA(text));
	return SetWindowTextW(hWnd, t.c_str());
}

int GetWindowTextW(HWND hWnd, LPWSTR text, int max)
{
	if (win(hWnd))
		return static_cast<int>(SendMessageW(hWnd, WM_GETTEXT, static_cast<WPARAM>(max), reinterpret_cast<LPARAM>(text)));
	QWidget* w = widgetOf(hWnd);
	return copyOut(w ? w->windowTitle() : QString(), text, max);
}

int GetWindowTextA(HWND hWnd, LPSTR text, int max)
{
	const int len = GetWindowTextLengthW(hWnd);
	std::wstring buf(static_cast<size_t>(len) + 1, L'\0');
	GetWindowTextW(hWnd, buf.data(), len + 1);
	return copyOutA(qs(buf.c_str()), text, max);
}

int GetWindowTextLengthW(HWND hWnd)
{
	if (win(hWnd))
		return static_cast<int>(SendMessageW(hWnd, WM_GETTEXTLENGTH, 0, 0));
	QWidget* w = widgetOf(hWnd);
	return w ? static_cast<int>(ws(w->windowTitle()).size()) : 0;
}

int GetClassNameW(HWND hWnd, LPWSTR name, int max)
{
	Window* w = win(hWnd);
	QString cls;
	if (w)
		cls = qs(w->className.c_str());
	else if (hWnd && hWnd->kind == NPP_HANDLE_SCINTILLA)
		cls = QStringLiteral("Scintilla");
	else if (hWnd && hWnd->kind == NPP_HANDLE_NOTEPAD)
		cls = QStringLiteral("Notepad++");
	return copyOut(cls, name, max);
}

// ---- subclasses of the windows of Notepad++ (main window, Scintilla views) --------------------
namespace
{
	struct HostWindow;

	// the key or mouse event being given to the procedures of a Scintilla view
	struct HostEvent
	{
		QObject* target = nullptr;
		QEvent* event = nullptr;
		HostWindow* window = nullptr;
		bool delivered = false;
		bool hasText = false;
	};
	thread_local HostEvent* g_hostEvent = nullptr;

	class HostFilter;

	struct HostWindow
	{
		HWND h = nullptr;
		WNDPROC proc = nullptr; // GWLP_WNDPROC, nullptr: the procedure of Notepad++
		std::vector<Subclass> subclasses;
		size_t depth = 0;
		bool hooked = false;
		HostFilter* filter = nullptr;
		NppWndHook hook{};
		bool passing = false; // an event given back to Scintilla
	};

	std::map<HWND, HostWindow*>& hostWindows()
	{
		static std::map<HWND, HostWindow*> w;
		return w;
	}

	bool isHostHandle(HWND h)
	{
		return h && h->magic == NPP_HANDLE_MAGIC && !win(h) && (h->kind == NPP_HANDLE_NOTEPAD || h->kind == NPP_HANDLE_SCINTILLA);
	}

	void deliver(HostEvent* he)
	{
		if (!he || he->delivered || !he->target)
			return;
		he->delivered = true;
		he->window->passing = true;
		QCoreApplication::sendEvent(he->target, he->event);
		he->window->passing = false;
	}

	LRESULT CALLBACK hostDefaultProc(HWND h, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		// what Notepad++ does with the message: Scintilla gets the event, the main window the message
		HostEvent* he = g_hostEvent;
		if (he && he->window && he->window->h == h)
		{
			switch (msg)
			{
				case WM_KEYDOWN:
				case WM_SYSKEYDOWN:
					if (!he->hasText)
						deliver(he);
					return 0;
				case WM_CHAR:
				case WM_SYSCHAR:
				case WM_KEYUP:
				case WM_SYSKEYUP:
				case WM_LBUTTONDOWN: case WM_LBUTTONUP: case WM_LBUTTONDBLCLK:
				case WM_RBUTTONDOWN: case WM_RBUTTONUP: case WM_RBUTTONDBLCLK:
				case WM_MBUTTONDOWN: case WM_MBUTTONUP: case WM_MBUTTONDBLCLK:
				case WM_MOUSEMOVE: case WM_MOUSEWHEEL: case WM_MOUSEHWHEEL:
					deliver(he);
					return 0;
				default:
					break;
			}
		}
		if (msg == WM_SETFOCUS || msg == WM_KILLFOCUS)
			return 0;
		if (h && h->send && h->kind == NPP_HANDLE_NOTEPAD)
		{
			// the procedure before this subclass: the next subclass of the main window, then Notepad++
			NppWndMessage next{msg, wParam, lParam};
			return h->send(h, NPPM_LINUX_CALLNEXT, reinterpret_cast<WPARAM>(&next), 0);
		}
		if (h && h->send)
			return h->send(h, msg, wParam, lParam);
		return 0;
	}

	LRESULT hostDispatch(HostWindow* hw, size_t level, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		if (level < hw->subclasses.size())
		{
			const Subclass s = hw->subclasses[hw->subclasses.size() - 1 - level];
			const size_t saved = hw->depth;
			hw->depth = level;
			const LRESULT r = s.proc(hw->h, msg, wParam, lParam, s.id, s.ref);
			hw->depth = saved;
			return r;
		}
		return (hw->proc ? hw->proc : hostDefaultProc)(hw->h, msg, wParam, lParam);
	}

	intptr_t nppHookDispatch(void* context, unsigned int msg, uintptr_t wParam, intptr_t lParam)
	{
		return hostDispatch(static_cast<HostWindow*>(context), 0, msg, wParam, lParam);
	}

	// key and mouse events of a Scintilla view as messages for the procedures of plugins
	class HostFilter : public QObject
	{
	public:
		explicit HostFilter(HostWindow* hw) : QObject(qApp), _hw(hw) {}

		bool eventFilter(QObject* obj, QEvent* e) override
		{
			if (_hw->passing)
				return false;
			switch (e->type())
			{
				case QEvent::KeyPress:
				case QEvent::KeyRelease:
				{
					auto* ke = static_cast<QKeyEvent*>(e);
					const int vk = vkFromQt(ke->key(), ke->nativeVirtualKey());
					const QString text = ke->text();
					HostEvent he;
					he.target = obj;
					he.event = e;
					he.window = _hw;
					he.hasText = !text.isEmpty() && (text[0].isPrint() || text[0] == QLatin1Char('\t') || text[0] == QLatin1Char('\r')
						|| text[0] == QLatin1Char('\b') || text[0] == QChar(27));
					HostEvent* saved = g_hostEvent;
					g_hostEvent = &he;
					const bool alt = ke->modifiers() & Qt::AltModifier;
					if (e->type() == QEvent::KeyPress)
					{
						if (vk)
							hostDispatch(_hw, 0, alt ? WM_SYSKEYDOWN : WM_KEYDOWN, static_cast<WPARAM>(vk), keyLParam(ke->isAutoRepeat() ? 2 : 1, false, false));
						if (he.hasText)
						{
							for (const QChar c : text)
								hostDispatch(_hw, 0, alt ? WM_SYSCHAR : WM_CHAR, c.unicode(), keyLParam(1, false, false));
						}
						else if (!vk)
							deliver(&he);
					}
					else
					{
						if (vk)
							hostDispatch(_hw, 0, alt ? WM_SYSKEYUP : WM_KEYUP, static_cast<WPARAM>(vk), keyLParam(1, true, false));
						else
							deliver(&he);
					}
					g_hostEvent = saved;
					return true; // given to Scintilla by hostDefaultProc, or kept by a plugin
				}
				case QEvent::MouseButtonPress:
				case QEvent::MouseButtonRelease:
				case QEvent::MouseButtonDblClick:
				case QEvent::Wheel:
				{
					HostEvent he;
					he.target = obj;
					he.event = e;
					he.window = _hw;
					HostEvent* saved = g_hostEvent;
					g_hostEvent = &he;
					if (e->type() == QEvent::Wheel)
					{
						auto* we = static_cast<QWheelEvent*>(e);
						const QPoint g = we->globalPosition().toPoint();
						const int dy = we->angleDelta().y();
						hostDispatch(_hw, 0, dy ? WM_MOUSEWHEEL : WM_MOUSEHWHEEL, MAKEWPARAM(mouseKeys(we->buttons(), we->modifiers()),
							static_cast<short>(dy ? dy : -we->angleDelta().x())), pointParam(g));
					}
					else
					{
						auto* me = static_cast<QMouseEvent*>(e);
						hostDispatch(_hw, 0, mouseMessage(me->type(), me->button()), mouseKeys(me->buttons() | me->button(), me->modifiers()),
							pointParam(me->position().toPoint()));
					}
					g_hostEvent = saved;
					return true;
				}
				case QEvent::FocusIn:
					hostDispatch(_hw, 0, WM_SETFOCUS, 0, 0);
					return false;
				case QEvent::FocusOut:
					hostDispatch(_hw, 0, WM_KILLFOCUS, 0, 0);
					return false;
				default:
					return false;
			}
		}

	private:
		HostWindow* _hw;
	};

	HostWindow* hostWindow(HWND h)
	{
		auto& all = hostWindows();
		auto it = all.find(h);
		if (it != all.end())
			return it->second;
		auto* hw = new HostWindow;
		hw->h = h;
		all[h] = hw;
		return hw;
	}

	void hookHost(HostWindow* hw)
	{
		if (hw->hooked)
			return;
		hw->hooked = true;
		if (hw->h->kind == NPP_HANDLE_NOTEPAD)
		{
			hw->hook.dispatch = nppHookDispatch;
			hw->hook.context = hw;
			hw->h->send(hw->h, NPPM_LINUX_SUBCLASSNPP, reinterpret_cast<WPARAM>(&hw->hook), TRUE);
		}
		else if (auto* widget = static_cast<QWidget*>(hw->h->widget))
		{
			hw->filter = new HostFilter(hw);
			widget->installEventFilter(hw->filter);
			if (auto* area = qobject_cast<QAbstractScrollArea*>(widget))
				area->viewport()->installEventFilter(hw->filter);
		}
	}
}

LONG_PTR GetWindowLongPtrW(HWND hWnd, int index)
{
	Window* w = win(hWnd);
	if (!w && isHostHandle(hWnd) && index == GWLP_WNDPROC)
	{
		HostWindow* hw = hostWindow(hWnd);
		return reinterpret_cast<LONG_PTR>(hw->proc ? hw->proc : hostDefaultProc);
	}
	if (!w)
		return 0;
	switch (index)
	{
		case GWLP_WNDPROC: return reinterpret_cast<LONG_PTR>(w->wndProc);
		case GWLP_HINSTANCE: return reinterpret_cast<LONG_PTR>(moduleHandle());
		case GWLP_HWNDPARENT: return reinterpret_cast<LONG_PTR>(w->parent);
		case GWLP_ID: return w->id;
		case GWL_STYLE:
		{
			DWORD s = w->style;
			if (w->widget)
			{
				s = w->widget->isVisible() ? (s | WS_VISIBLE) : (s & ~WS_VISIBLE);
				s = w->widget->isEnabled() ? (s & ~WS_DISABLED) : (s | WS_DISABLED);
			}
			return static_cast<LONG_PTR>(s);
		}
		case GWL_EXSTYLE: return static_cast<LONG_PTR>(w->exStyle);
		case GWLP_USERDATA: return w->userData;
		default:
			break;
	}
	if (w->isDialog)
	{
		if (index == DWLP_MSGRESULT)
			return w->msgResult;
		if (index == DWLP_DLGPROC)
			return reinterpret_cast<LONG_PTR>(w->dlgProc);
		if (index == DWLP_USER)
			return w->dlgUser;
	}
	if (index >= 0 && static_cast<size_t>(index) + sizeof(LONG_PTR) <= w->extra.size())
	{
		LONG_PTR v = 0;
		std::memcpy(&v, w->extra.data() + index, sizeof(v));
		return v;
	}
	return 0;
}

LONG_PTR SetWindowLongPtrW(HWND hWnd, int index, LONG_PTR value)
{
	Window* w = win(hWnd);
	if (!w && isHostHandle(hWnd) && index == GWLP_WNDPROC)
	{
		// a plugin subclasses a window of Notepad++
		HostWindow* hw = hostWindow(hWnd);
		const LONG_PTR old = reinterpret_cast<LONG_PTR>(hw->proc ? hw->proc : hostDefaultProc);
		auto proc = reinterpret_cast<WNDPROC>(value);
		hw->proc = (proc == hostDefaultProc) ? nullptr : proc;
		hookHost(hw);
		return old;
	}
	if (!w)
		return 0;
	const LONG_PTR old = GetWindowLongPtrW(hWnd, index);
	switch (index)
	{
		case GWLP_WNDPROC:
			w->wndProc = reinterpret_cast<WNDPROC>(value);
			return old;
		case GWLP_ID:
			w->id = static_cast<int>(value);
			return old;
		case GWL_STYLE:
		{
			const DWORD s = static_cast<DWORD>(value);
			w->style = s;
			if (w->widget)
			{
				w->widget->setEnabled(!(s & WS_DISABLED));
				if (w->kind != Kind::Dialog && !w->isDialog)
					controlProc(w, WM_STYLECHANGED, GWL_STYLE, 0);
			}
			return old;
		}
		case GWL_EXSTYLE:
			w->exStyle = static_cast<DWORD>(value);
			if (w->widget && w->widget->isWindow())
			{
				const bool visible = w->widget->isVisible();
				w->widget->setWindowFlag(Qt::WindowStaysOnTopHint, (w->exStyle & WS_EX_TOPMOST) != 0);
				if (visible)
					w->widget->show();
			}
			return old;
		case GWLP_USERDATA:
			w->userData = value;
			return old;
		case GWLP_HWNDPARENT:
			w->owner = reinterpret_cast<HWND>(value);
			return old;
		default:
			break;
	}
	if (w->isDialog)
	{
		if (index == DWLP_MSGRESULT)
		{
			w->msgResult = value;
			return old;
		}
		if (index == DWLP_DLGPROC)
		{
			w->dlgProc = reinterpret_cast<DLGPROC>(value);
			return old;
		}
		if (index == DWLP_USER)
		{
			w->dlgUser = value;
			return old;
		}
	}
	if (index >= 0)
	{
		if (static_cast<size_t>(index) + sizeof(LONG_PTR) > w->extra.size())
			w->extra.resize(static_cast<size_t>(index) + sizeof(LONG_PTR), 0);
		std::memcpy(w->extra.data() + index, &value, sizeof(value));
	}
	return old;
}

ULONG_PTR GetClassLongPtrW(HWND hWnd, int index)
{
	Window* w = win(hWnd);
	if (!w)
		return 0;
	switch (index)
	{
		case GCLP_HBRBACKGROUND: return reinterpret_cast<ULONG_PTR>(w->background);
		case GCLP_HCURSOR: return reinterpret_cast<ULONG_PTR>(w->cursor);
		case GCLP_HICON: return reinterpret_cast<ULONG_PTR>(w->icon);
		default: return 0;
	}
}

ULONG_PTR SetClassLongPtrW(HWND hWnd, int index, LONG_PTR value)
{
	Window* w = win(hWnd);
	if (!w)
		return 0;
	const ULONG_PTR old = GetClassLongPtrW(hWnd, index);
	switch (index)
	{
		case GCLP_HBRBACKGROUND: w->background = reinterpret_cast<HBRUSH>(value); break;
		case GCLP_HCURSOR: w->cursor = reinterpret_cast<HCURSOR>(value); break;
		case GCLP_HICON:
			w->icon = reinterpret_cast<HICON>(value);
			if (w->widget)
				w->widget->setWindowIcon(iconOf(w->icon));
			break;
		default: break;
	}
	return old;
}

BOOL SetPropW(HWND hWnd, LPCWSTR name, HANDLE data)
{
	Window* w = win(hWnd);
	if (!w || !name)
		return FALSE;
	const std::wstring key = isIntResource(name) ? std::to_wstring(intResource(name)) : std::wstring(name);
	w->props[key] = data;
	return TRUE;
}

HANDLE GetPropW(HWND hWnd, LPCWSTR name)
{
	Window* w = win(hWnd);
	if (!w || !name)
		return nullptr;
	const std::wstring key = isIntResource(name) ? std::to_wstring(intResource(name)) : std::wstring(name);
	auto it = w->props.find(key);
	return it == w->props.end() ? nullptr : it->second;
}

HANDLE RemovePropW(HWND hWnd, LPCWSTR name)
{
	Window* w = win(hWnd);
	if (!w || !name)
		return nullptr;
	const std::wstring key = isIntResource(name) ? std::to_wstring(intResource(name)) : std::wstring(name);
	auto it = w->props.find(key);
	if (it == w->props.end())
		return nullptr;
	HANDLE h = it->second;
	w->props.erase(it);
	return h;
}

LRESULT CallWindowProcW(WNDPROC proc, HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (proc)
		return proc(hWnd, msg, wParam, lParam);
	if (Window* w = win(hWnd))
		return defaultProc(w, msg, wParam, lParam);
	if (isHostHandle(hWnd))
		return hostDefaultProc(hWnd, msg, wParam, lParam);
	return 0;
}

LRESULT DefWindowProcW(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	Window* w = win(hWnd);
	switch (msg)
	{
		case WM_NCCREATE:
			return TRUE;
		case WM_CLOSE:
			if (w)
				destroyWindow(w);
			return 0;
		case WM_SETTEXT:
			if (QWidget* widget = widgetOf(hWnd))
				widget->setWindowTitle(qs(reinterpret_cast<LPCWSTR>(lParam)));
			return TRUE;
		case WM_GETTEXT:
		{
			QWidget* widget = widgetOf(hWnd);
			return copyOut(widget ? widget->windowTitle() : QString(), reinterpret_cast<LPWSTR>(lParam), static_cast<int>(wParam));
		}
		case WM_GETTEXTLENGTH:
		{
			QWidget* widget = widgetOf(hWnd);
			return widget ? static_cast<LRESULT>(ws(widget->windowTitle()).size()) : 0;
		}
		case WM_ERASEBKGND:
			if (w && w->painter && w->widget)
			{
				QBrush b = w->widget->palette().window();
				if (w->background)
				{
					const intptr_t v = reinterpret_cast<intptr_t>(w->background);
					if (v > 0 && v <= COLOR_MENUBAR + 1)
						b = QBrush(sysColor(static_cast<int>(v - 1)));
					else if (Gdi* g = gdi(w->background); g && !g->nullBrush)
						b = QBrush(g->color);
				}
				w->painter->fillRect(w->widget->rect(), b);
				return 1;
			}
			return 0;
		case WM_SETFONT:
			if (w)
			{
				w->font = reinterpret_cast<HFONT>(wParam);
				if (w->widget && wParam)
					w->widget->setFont(fontOf(w->font));
			}
			return 0;
		case WM_GETFONT:
			return w ? reinterpret_cast<LRESULT>(w->font) : 0;
		case WM_SETICON:
			if (w)
			{
				HICON old = w->icon;
				w->icon = reinterpret_cast<HICON>(lParam);
				if (w->widget && lParam)
					w->widget->setWindowIcon(iconOf(w->icon));
				return reinterpret_cast<LRESULT>(old);
			}
			return 0;
		case WM_GETICON:
			return w ? reinterpret_cast<LRESULT>(w->icon) : 0;
		case WM_CTLCOLORMSGBOX:
		case WM_CTLCOLORDLG:
		case WM_CTLCOLORBTN:
		case WM_CTLCOLORSTATIC:
		case WM_CTLCOLORSCROLLBAR:
			return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_BTNFACE));
		case WM_CTLCOLOREDIT:
		case WM_CTLCOLORLISTBOX:
			return reinterpret_cast<LRESULT>(GetSysColorBrush(COLOR_WINDOW));
		case WM_NCHITTEST:
			return HTCLIENT;
		case WM_MOUSEACTIVATE:
			return 1; // MA_ACTIVATE
		case WM_NOTIFYFORMAT:
			return 2; // NFR_UNICODE
		case WM_SYSCOMMAND:
			if ((wParam & 0xFFF0) == SC_CLOSE)
				SendMessageW(hWnd, WM_CLOSE, 0, 0);
			else if ((wParam & 0xFFF0) == SC_MINIMIZE)
				ShowWindow(hWnd, SW_MINIMIZE);
			else if ((wParam & 0xFFF0) == SC_MAXIMIZE)
				ShowWindow(hWnd, SW_MAXIMIZE);
			else if ((wParam & 0xFFF0) == SC_RESTORE)
				ShowWindow(hWnd, SW_RESTORE);
			return 0;
		case WM_CONTEXTMENU:
			// child windows give it to their parent
			if (w && (w->style & WS_CHILD) && w->parent && !w->destroyed)
			{
				g_defaultReached = false;
				SendMessageW(w->parent, WM_CONTEXTMENU, wParam, lParam);
				return 0;
			}
			g_defaultReached = true;
			return 0;
		case WM_MOUSEWHEEL:
		case WM_MOUSEHWHEEL:
			if (w && (w->style & WS_CHILD) && w->parent)
				return SendMessageW(w->parent, msg, wParam, lParam);
			g_defaultReached = true;
			return 0;
		case WM_KEYDOWN:
		case WM_KEYUP:
		case WM_CHAR:
		case WM_SYSKEYDOWN:
		case WM_SYSKEYUP:
		case WM_SYSCHAR:
		case WM_MOUSEMOVE:
		case WM_LBUTTONDOWN:
		case WM_LBUTTONUP:
		case WM_LBUTTONDBLCLK:
		case WM_RBUTTONDOWN:
		case WM_RBUTTONUP:
		case WM_RBUTTONDBLCLK:
		case WM_MBUTTONDOWN:
		case WM_MBUTTONUP:
		case WM_MBUTTONDBLCLK:
			g_defaultReached = true;
			return 0;
		case WM_SETCURSOR:
			if (w && w->widget && w->cursor)
			{
				if (Gdi* g = gdi(w->cursor); g && g->cursor)
					w->widget->setCursor(*g->cursor);
			}
			return 0;
		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			BeginPaint(hWnd, &ps);
			EndPaint(hWnd, &ps);
			return 0;
		}
		default:
			return 0;
	}
}

LRESULT DefDlgProcW(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	Window* w = win(hDlg);
	if (!w)
		return 0;
	switch (msg)
	{
		case WM_CLOSE:
			SendMessageW(hDlg, WM_COMMAND, MAKEWPARAM(IDCANCEL, BN_CLICKED), reinterpret_cast<LPARAM>(GetDlgItem(hDlg, IDCANCEL)));
			return 0;
		case DM_GETDEFID:
			return MAKELONG(w->defId, DC_HASDEFID);
		case DM_SETDEFID:
			w->defId = static_cast<int>(wParam);
			return TRUE;
		case WM_ERASEBKGND:
			return 0; // the palette background (W32Widget)
		case WM_NEXTDLGCTL:
			if (lParam)
				SetFocus(reinterpret_cast<HWND>(wParam));
			else if (HWND next = GetNextDlgTabItem(hDlg, GetFocus(), wParam != 0))
				SetFocus(next);
			return 0;
		case WM_GETDLGCODE:
			return 0;
		case WM_COMMAND:
		case WM_INITDIALOG:
		case WM_NOTIFY:
			return 0;
		default:
			return DefWindowProcW(hDlg, msg, wParam, lParam);
	}
}

BOOL FlashWindow(HWND hWnd, BOOL)
{
	if (QWidget* w = widgetOf(hWnd))
		QApplication::alert(w->window());
	return TRUE;
}

BOOL FlashWindowEx(PFLASHWINFO info)
{
	if (!info || info->dwFlags == FLASHW_STOP)
		return FALSE;
	return FlashWindow(info->hwnd, TRUE);
}

UINT SendInput(UINT count, LPINPUT inputs, int size)
{
	if (!inputs || size != static_cast<int>(sizeof(INPUT)))
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		return 0;
	}
	UINT sent = 0;
	for (UINT i = 0; i < count; ++i)
	{
		const INPUT& in = inputs[i];
		if (in.type != INPUT_MOUSE)
			break;
		const DWORD f = in.mi.dwFlags;
		const QPoint global = QCursor::pos();
		struct Button { DWORD down, up; Qt::MouseButton button; };
		for (const Button& b : {Button{MOUSEEVENTF_LEFTDOWN, MOUSEEVENTF_LEFTUP, Qt::LeftButton},
			Button{MOUSEEVENTF_RIGHTDOWN, MOUSEEVENTF_RIGHTUP, Qt::RightButton},
			Button{MOUSEEVENTF_MIDDLEDOWN, MOUSEEVENTF_MIDDLEUP, Qt::MiddleButton}})
		{
			if (!(f & (b.down | b.up)))
				continue;
			const bool down = (f & b.down) != 0;
			const Qt::MouseButtons held = QGuiApplication::mouseButtons();
			// a release of a button that is not held: nothing to release (like Windows, no effect)
			if (!down && !(held & b.button))
				continue;
			QWidget* target = QWidget::mouseGrabber();
			if (!target)
				target = QApplication::widgetAt(global);
			if (!target)
				continue;
			const QPointF local = target->mapFromGlobal(QPointF(global));
			QMouseEvent e(down ? QEvent::MouseButtonPress : QEvent::MouseButtonRelease, local, QPointF(global), b.button,
				down ? (held | b.button) : (held & ~b.button), QGuiApplication::keyboardModifiers());
			QApplication::sendEvent(target, &e);
		}
		++sent;
	}
	return sent;
}

UINT GetDpiForWindow(HWND hWnd)
{
	QWidget* w = widgetOf(hWnd);
	QScreen* s = w && w->screen() ? w->screen() : QGuiApplication::primaryScreen();
	return s ? static_cast<UINT>(s->logicalDotsPerInch()) : 96;
}

UINT GetDpiForSystem()
{
	QScreen* s = QGuiApplication::primaryScreen();
	return s ? static_cast<UINT>(s->logicalDotsPerInch()) : 96;
}

BOOL AdjustWindowRectEx(LPRECT, DWORD, BOOL, DWORD)
{
	return TRUE; // Qt sizes are client sizes
}

BOOL AnimateWindow(HWND hWnd, DWORD, DWORD flags)
{
	return ShowWindow(hWnd, (flags & 0x00010000) ? SW_HIDE : SW_SHOW); // AW_HIDE
}

BOOL SetLayeredWindowAttributes(HWND hWnd, COLORREF, BYTE alpha, DWORD flags)
{
	QWidget* w = widgetOf(hWnd);
	if (!w)
		return FALSE;
	if (flags & LWA_ALPHA)
		w->window()->setWindowOpacity(alpha / 255.0);
	return TRUE;
}

BOOL LockWindowUpdate(HWND)
{
	return TRUE;
}

BOOL DrawMenuBar(HWND)
{
	return TRUE;
}

BOOL ShowScrollBar(HWND hWnd, int bar, BOOL show)
{
	if (auto* area = qobject_cast<QAbstractScrollArea*>(widgetOf(hWnd)))
	{
		const auto policy = show ? Qt::ScrollBarAlwaysOn : Qt::ScrollBarAlwaysOff;
		if (bar == SB_HORZ || bar == SB_BOTH)
			area->setHorizontalScrollBarPolicy(policy);
		if (bar == SB_VERT || bar == SB_BOTH)
			area->setVerticalScrollBarPolicy(policy);
	}
	return TRUE;
}

int SetScrollInfo(HWND, int, LPCSCROLLINFO si, BOOL)
{
	return si ? si->nPos : 0;
}

BOOL GetScrollInfo(HWND, int, LPSCROLLINFO si)
{
	if (si)
	{
		si->nMin = 0;
		si->nMax = 0;
		si->nPage = 0;
		si->nPos = 0;
		si->nTrackPos = 0;
	}
	return FALSE;
}

// the scroll bars of a Qt scroll area (also the Scintilla of Notepad++)
BOOL GetScrollBarInfo(HWND hWnd, LONG idObject, PSCROLLBARINFO info)
{
	if (!info || (idObject != OBJID_VSCROLL && idObject != OBJID_HSCROLL))
		return FALSE;
	const DWORD size = info->cbSize;
	*info = SCROLLBARINFO{};
	info->cbSize = size;
	info->rgstate[0] = STATE_SYSTEM_INVISIBLE;
	auto* area = qobject_cast<QAbstractScrollArea*>(w32::widgetOf(hWnd));
	if (!area)
		return TRUE;
	QScrollBar* bar = idObject == OBJID_VSCROLL ? area->verticalScrollBar() : area->horizontalScrollBar();
	if (!bar || !bar->isVisible())
		return TRUE;
	const QPoint topLeft = bar->mapToGlobal(QPoint(0, 0));
	info->rcScrollBar = RECT{topLeft.x(), topLeft.y(), topLeft.x() + bar->width(), topLeft.y() + bar->height()};
	info->dxyLineButton = 0;
	info->rgstate[0] = bar->isEnabled() ? 0 : STATE_SYSTEM_UNAVAILABLE;
	return TRUE;
}

int SetScrollPos(HWND, int, int pos, BOOL)
{
	return pos;
}

int GetScrollPos(HWND, int)
{
	return 0;
}

BOOL SetScrollRange(HWND, int, int, int, BOOL)
{
	return TRUE;
}

BOOL TrackMouseEvent(LPTRACKMOUSEEVENT tme)
{
	if (!tme)
		return FALSE;
	if (Window* w = win(tme->hwndTrack))
		w->trackLeave = (tme->dwFlags & TME_LEAVE) && !(tme->dwFlags & TME_CANCEL);
	return TRUE;
}

BOOL DragDetect(HWND, POINT)
{
	return FALSE;
}

void DragAcceptFiles(HWND hWnd, BOOL accept)
{
	if (Window* w = win(hWnd))
	{
		w->exStyle = accept ? (w->exStyle | WS_EX_ACCEPTFILES) : (w->exStyle & ~WS_EX_ACCEPTFILES);
		if (w->widget)
			w->widget->setAcceptDrops(accept != FALSE);
	}
}

UINT DragQueryFileW(HDROP drop, UINT index, LPWSTR file, UINT max)
{
	if (!drop)
		return 0;
	const QStringList files = dropFilesList(drop);
	if (index == 0xFFFFFFFF)
		return static_cast<UINT>(files.size());
	if (index >= static_cast<UINT>(files.size()))
		return 0;
	const QString f = files[static_cast<int>(index)];
	if (!file)
		return static_cast<UINT>(ws(f).size());
	return static_cast<UINT>(copyOut(f, file, static_cast<int>(max)));
}

BOOL DragQueryPoint(HDROP drop, POINT* pt)
{
	const auto* df = static_cast<const DROPFILES*>(drop);
	if (!df || !pt)
		return FALSE;
	*pt = df->pt;
	return !df->fNC;
}

void DragFinish(HDROP drop)
{
	GlobalFree(drop);
}

// ---- timers --------------------------------------------------------------------------------
namespace
{
	// timers of the handles of the host (Notepad++ window, Scintilla views) and of no window (SetTimer(NULL)):
	// not windows of the SDK, so the plugin keeps them, by handle and id like Windows
	// Never destroyed: plugins kill their timers in the destructors of their global objects, at the exit of the
	// process, after the destruction of the static objects made later (a static map would be gone by then).
	std::map<std::pair<HWND, UINT_PTR>, QTimer*>& hostTimers()
	{
		static auto* timers = new std::map<std::pair<HWND, UINT_PTR>, QTimer*>;
		return *timers;
	}
}

UINT_PTR SetTimer(HWND hWnd, UINT_PTR id, UINT elapse, TIMERPROC proc)
{
	static UINT_PTR nextId = 0x7000;
	Window* w = win(hWnd);
	if (!w && (!hWnd || !id))
		id = ++nextId; // SetTimer(NULL, ...) gives the id of the timer
	QTimer*& t = w ? w->timers[id] : hostTimers()[{hWnd, id}];
	if (!t)
		t = new QTimer;
	else
		QObject::disconnect(t, &QTimer::timeout, nullptr, nullptr); // the same timer again: its new procedure, like Windows
	const UINT_PTR timerId = id;
	QObject::connect(t, &QTimer::timeout, [hWnd, timerId, proc]
	{
		const DWORD now = GetTickCount();
		if (proc)
			proc(hWnd, WM_TIMER, timerId, now);
		else if (hWnd)
			SendMessageW(hWnd, WM_TIMER, timerId, 0);
	});
	t->start(static_cast<int>(std::max<UINT>(elapse, 1)));
	return id;
}

BOOL KillTimer(HWND hWnd, UINT_PTR id)
{
	if (Window* w = win(hWnd))
	{
		auto it = w->timers.find(id);
		if (it == w->timers.end())
			return FALSE;
		it->second->stop();
		it->second->deleteLater();
		w->timers.erase(it);
		return TRUE;
	}
	auto it = hostTimers().find({hWnd, id});
	if (it == hostTimers().end())
		return FALSE;
	if (QCoreApplication::instance()) // after the application (exit of the process): nothing runs any more
	{
		it->second->stop();
		it->second->deleteLater();
	}
	hostTimers().erase(it);
	return TRUE;
}

// ---- keyboard, mouse, cursor ---------------------------------------------------------------
namespace
{
	// keys down now, for GetKeyState of other keys than the modifiers (keyboard hooks)
	std::set<int>& keysDown()
	{
		static std::set<int> k;
		return k;
	}

	void trackKey(QKeyEvent* ke)
	{
		const int vk = vkFromQt(ke->key(), ke->nativeVirtualKey());
		if (!vk)
			return;
		if (ke->type() == QEvent::KeyPress)
			keysDown().insert(vk);
		else if (!ke->isAutoRepeat())
			keysDown().erase(vk);
	}
}

SHORT GetKeyState(int vk)
{
	const Qt::KeyboardModifiers m = QGuiApplication::queryKeyboardModifiers();
	const Qt::MouseButtons b = QGuiApplication::mouseButtons();
	bool down = false;
	switch (vk)
	{
		case VK_SHIFT: case VK_LSHIFT: case VK_RSHIFT: down = m & Qt::ShiftModifier; break;
		case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL: down = m & Qt::ControlModifier; break;
		case VK_MENU: case VK_LMENU: case VK_RMENU: down = m & Qt::AltModifier; break;
		case VK_LWIN: case VK_RWIN: down = m & Qt::MetaModifier; break;
		case VK_LBUTTON: down = b & Qt::LeftButton; break;
		case VK_RBUTTON: down = b & Qt::RightButton; break;
		case VK_MBUTTON: down = b & Qt::MiddleButton; break;
		default: down = keysDown().count(vk) != 0; break;
	}
	return down ? static_cast<SHORT>(0x8000) : 0;
}

SHORT GetAsyncKeyState(int vk)
{
	return GetKeyState(vk);
}

BOOL GetKeyboardState(PBYTE state)
{
	if (!state)
		return FALSE;
	std::memset(state, 0, 256);
	for (int vk : {VK_SHIFT, VK_CONTROL, VK_MENU, VK_LBUTTON, VK_RBUTTON, VK_MBUTTON})
	{
		if (GetKeyState(vk) & 0x8000)
			state[vk] = 0x80;
	}
	for (int vk : keysDown())
	{
		if (vk > 0 && vk < 256)
			state[vk] = 0x80;
	}
	return TRUE;
}

UINT MapVirtualKeyW(UINT code, UINT mapType)
{
	if (mapType == 2) // MAPVK_VK_TO_CHAR
	{
		if ((code >= 'A' && code <= 'Z') || (code >= '0' && code <= '9'))
			return code;
		const char* oem = ";=,-./`";
		if (code >= VK_OEM_1 && code <= VK_OEM_3)
			return static_cast<UINT>(oem[code - VK_OEM_1]);
		switch (code)
		{
			case VK_OEM_4: return '[';
			case VK_OEM_5: return '\\';
			case VK_OEM_6: return ']';
			case VK_OEM_7: return '\'';
			case VK_SPACE: return ' ';
			default: return 0;
		}
	}
	return code;
}

SHORT VkKeyScanW(WCHAR ch)
{
	// the keys of a US keyboard: virtual key, 0x100 for Shift
	const QChar c(static_cast<char16_t>(ch));
	if (c.isLetter() && c.toUpper().unicode() < 128)
		return static_cast<SHORT>(c.toUpper().unicode() | (c.isUpper() ? 0x100 : 0));
	if (c.isDigit())
		return static_cast<SHORT>(ch);
	static const struct { wchar_t ch; int vk; bool shift; } keys[] = {
		{L' ', VK_SPACE, false}, {L'\t', VK_TAB, false}, {L'\r', VK_RETURN, false}, {L'\b', VK_BACK, false},
		{L')', '0', true}, {L'!', '1', true}, {L'@', '2', true}, {L'#', '3', true}, {L'$', '4', true},
		{L'%', '5', true}, {L'^', '6', true}, {L'&', '7', true}, {L'*', '8', true}, {L'(', '9', true},
		{L';', VK_OEM_1, false}, {L':', VK_OEM_1, true}, {L'=', VK_OEM_PLUS, false}, {L'+', VK_OEM_PLUS, true},
		{L',', VK_OEM_COMMA, false}, {L'<', VK_OEM_COMMA, true}, {L'-', VK_OEM_MINUS, false}, {L'_', VK_OEM_MINUS, true},
		{L'.', VK_OEM_PERIOD, false}, {L'>', VK_OEM_PERIOD, true}, {L'/', VK_OEM_2, false}, {L'?', VK_OEM_2, true},
		{L'`', VK_OEM_3, false}, {L'~', VK_OEM_3, true}, {L'[', VK_OEM_4, false}, {L'{', VK_OEM_4, true},
		{L'\\', VK_OEM_5, false}, {L'|', VK_OEM_5, true}, {L']', VK_OEM_6, false}, {L'}', VK_OEM_6, true},
		{L'\'', VK_OEM_7, false}, {L'"', VK_OEM_7, true}};
	for (const auto& k : keys)
	{
		if (k.ch == ch)
			return static_cast<SHORT>(k.vk | (k.shift ? 0x100 : 0));
	}
	return -1;
}

SHORT VkKeyScanExW(WCHAR ch, HKL)
{
	return VkKeyScanW(ch);
}

HKL GetKeyboardLayout(DWORD)
{
	return reinterpret_cast<HKL>(static_cast<uintptr_t>(0x04090409));
}

int GetKeyNameTextW(LONG lParam, LPWSTR text, int max)
{
	const int vk = (lParam >> 16) & 0xFF;
	const int key = qtKeyFromVk(vk);
	return copyOut(key ? QKeySequence(key).toString() : QString(), text, max);
}

UINT GetDoubleClickTime()
{
	return static_cast<UINT>(QApplication::doubleClickInterval());
}

BOOL GetCursorPos(LPPOINT pt)
{
	if (!pt)
		return FALSE;
	const QPoint p = QCursor::pos();
	pt->x = p.x();
	pt->y = p.y();
	return TRUE;
}

BOOL SetCursorPos(int x, int y)
{
	QCursor::setPos(x, y);
	return TRUE;
}

HCURSOR SetCursor(HCURSOR cursor)
{
	HCURSOR old = g_cursor;
	g_cursor = cursor;
	Gdi* g = gdi(cursor);
	if (g && g->cursor)
	{
		if (QWidget* w = QApplication::widgetAt(QCursor::pos()))
			w->setCursor(*g->cursor);
	}
	else if (!cursor)
	{
		if (QWidget* w = QApplication::widgetAt(QCursor::pos()))
			w->unsetCursor();
	}
	return old;
}

HCURSOR GetCursor()
{
	return g_cursor;
}

int ShowCursor(BOOL show)
{
	return show ? 0 : -1;
}

BOOL DestroyCursor(HCURSOR)
{
	return TRUE;
}

BOOL RegisterHotKey(HWND, int, UINT, UINT)
{
	return FALSE;
}

BOOL UnregisterHotKey(HWND, int)
{
	return FALSE;
}

namespace
{
	// SetWindowsHookEx: keyboard and mouse hooks of the thread, from an application event filter
	struct Hook
	{
		int type;
		HOOKPROC proc;
	};

	std::vector<Hook*>& hooks()
	{
		static std::vector<Hook*> h;
		return h;
	}

	class HookFilter : public QObject
	{
	public:
		bool eventFilter(QObject* obj, QEvent* e) override
		{
			if (hooks().empty() || !obj->isWidgetType())
				return false;
			if (e->type() == QEvent::KeyPress || e->type() == QEvent::KeyRelease)
			{
				auto* ke = static_cast<QKeyEvent*>(e);
				// a key event goes to the parents too when the focus widget ignores it: once
				static QEvent* last = nullptr;
				static quint64 lastTime = 0;
				if (e == last && ke->timestamp() == lastTime)
					return false;
				last = e;
				lastTime = ke->timestamp();
				trackKey(ke);
				const int vk = vkFromQt(ke->key(), ke->nativeVirtualKey());
				if (!vk)
					return false;
				const LPARAM l = keyLParam(1, e->type() == QEvent::KeyRelease, false);
				for (Hook* h : std::vector<Hook*>(hooks()))
				{
					if (h->type == WH_KEYBOARD && h->proc(HC_ACTION, static_cast<WPARAM>(vk), l))
						return true;
				}
			}
			else if (e->type() == QEvent::MouseButtonPress || e->type() == QEvent::MouseButtonRelease
				|| e->type() == QEvent::MouseButtonDblClick || e->type() == QEvent::MouseMove)
			{
				auto* me = static_cast<QMouseEvent*>(e);
				MOUSEHOOKSTRUCT mh{};
				const QPoint g = me->globalPosition().toPoint();
				mh.pt = POINT{g.x(), g.y()};
				mh.hwnd = nullptr;
				mh.wHitTestCode = HTCLIENT;
				const UINT msg = mouseMessage(me->type(), me->button());
				for (Hook* h : std::vector<Hook*>(hooks()))
				{
					if (h->type == WH_MOUSE && h->proc(HC_ACTION, msg, reinterpret_cast<LPARAM>(&mh)))
						return true;
				}
			}
			return false;
		}
	};
}

HHOOK SetWindowsHookExW(int idHook, HOOKPROC proc, HINSTANCE, DWORD)
{
	if (!proc || (idHook != WH_KEYBOARD && idHook != WH_MOUSE && idHook != WH_GETMESSAGE && idHook != WH_CALLWNDPROC))
		return nullptr;
	static HookFilter* filter = nullptr;
	if (!filter)
	{
		filter = new HookFilter;
		qApp->installEventFilter(filter);
	}
	auto* h = new Hook{idHook, proc};
	hooks().push_back(h);
	return h;
}

BOOL UnhookWindowsHookEx(HHOOK hHook)
{
	auto& list = hooks();
	auto it = std::find(list.begin(), list.end(), static_cast<Hook*>(hHook));
	if (it == list.end())
		return FALSE;
	delete *it;
	list.erase(it);
	return TRUE;
}

LRESULT CallNextHookEx(HHOOK, int, WPARAM, LPARAM)
{
	return 0;
}

void keybd_event(BYTE vk, BYTE, DWORD flags, ULONG_PTR)
{
	QWidget* target = QApplication::focusWidget();
	if (!target)
		return;
	const int key = qtKeyFromVk(vk);
	if (!key)
		return;
	QKeyEvent ev((flags & KEYEVENTF_KEYUP) ? QEvent::KeyRelease : QEvent::KeyPress, key, Qt::NoModifier);
	QCoreApplication::sendEvent(target, &ev);
}

// ---- window subclassing (comctl32) ---------------------------------------------------------
BOOL SetWindowSubclass(HWND hWnd, SUBCLASSPROC proc, UINT_PTR id, DWORD_PTR refData)
{
	if (isHostHandle(hWnd) && proc)
	{
		HostWindow* hw = hostWindow(hWnd);
		for (Subclass& s : hw->subclasses)
		{
			if (s.proc == proc && s.id == id)
			{
				s.ref = refData;
				return TRUE;
			}
		}
		hw->subclasses.push_back(Subclass{proc, id, refData});
		hookHost(hw);
		return TRUE;
	}
	Window* w = win(hWnd);
	if (!w || !proc)
		return FALSE;
	for (Subclass& s : w->subclasses)
	{
		if (s.proc == proc && s.id == id)
		{
			s.ref = refData;
			return TRUE;
		}
	}
	w->subclasses.push_back(Subclass{proc, id, refData});
	return TRUE;
}

BOOL GetWindowSubclass(HWND hWnd, SUBCLASSPROC proc, UINT_PTR id, DWORD_PTR* refData)
{
	if (isHostHandle(hWnd))
	{
		for (const Subclass& s : hostWindow(hWnd)->subclasses)
		{
			if (s.proc == proc && s.id == id)
			{
				if (refData)
					*refData = s.ref;
				return TRUE;
			}
		}
		return FALSE;
	}
	Window* w = win(hWnd);
	if (!w)
		return FALSE;
	for (const Subclass& s : w->subclasses)
	{
		if (s.proc == proc && s.id == id)
		{
			if (refData)
				*refData = s.ref;
			return TRUE;
		}
	}
	return FALSE;
}

BOOL RemoveWindowSubclass(HWND hWnd, SUBCLASSPROC proc, UINT_PTR id)
{
	if (isHostHandle(hWnd))
	{
		HostWindow* hw = hostWindow(hWnd);
		for (auto it = hw->subclasses.begin(); it != hw->subclasses.end(); ++it)
		{
			if (it->proc == proc && it->id == id)
			{
				hw->subclasses.erase(it);
				return TRUE;
			}
		}
		return FALSE;
	}
	Window* w = win(hWnd);
	if (!w)
		return FALSE;
	for (auto it = w->subclasses.begin(); it != w->subclasses.end(); ++it)
	{
		if (it->proc == proc && it->id == id)
		{
			w->subclasses.erase(it);
			return TRUE;
		}
	}
	return FALSE;
}

LRESULT DefSubclassProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (isHostHandle(hWnd))
	{
		HostWindow* hw = hostWindow(hWnd);
		return hostDispatch(hw, hw->depth + 1, msg, wParam, lParam);
	}
	Window* w = win(hWnd);
	if (!w)
		return 0;
	return callSubclasses(w, w->subclassDepth + 1, msg, wParam, lParam);
}

UINT GetWindowModuleFileNameW(HWND hWnd, LPWSTR path, UINT max)
{
	// the windows of this plugin: its .so; the others: Notepad++
	const QString file = win(hWnd) ? moduleFile() : QCoreApplication::applicationFilePath();
	if (!path || !max)
		return 0;
	return static_cast<UINT>(copyOut(file, path, static_cast<int>(max)));
}
