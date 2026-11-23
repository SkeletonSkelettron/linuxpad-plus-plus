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

// Notepad++ Linux plugin SDK, npp_win32 library: menus. Menus of plugins are QMenus;
// menus of Notepad++ (NPPM_GETMENUHANDLE) are changed through the host so that the
// check marks of plugin commands stay right.

#include <QAction>
#include <QMenu>
#include <QMenuBar>

#include "W32Internal.h"

#define W32_NPPM_GETMENUHANDLE (WM_USER + 1000 + 25)
#define W32_NPPM_SETMENUITEMCHECK (WM_USER + 1000 + 40)

namespace w32
{
	namespace
	{
		struct MenuData
		{
			NppHandle handle;
			QPointer<QMenu> menu;
		};

		intptr_t menuSend(NppHandle*, unsigned int, uintptr_t, intptr_t)
		{
			return 0;
		}

		bool isHostMenu(HMENU h)
		{
			return h && h->magic == NPP_HANDLE_MAGIC && h->kind == NPP_HANDLE_MENU && h->send != menuSend;
		}

		QList<QAction*> actionsOf(QWidget* m)
		{
			return m ? m->actions() : QList<QAction*>();
		}

		QWidget* widgetOfMenu(HMENU h)
		{
			if (!h || h->magic != NPP_HANDLE_MAGIC || h->kind != NPP_HANDLE_MENU)
				return nullptr;
			return static_cast<QWidget*>(h->widget);
		}

		QAction* findById(QWidget* m, UINT id, QWidget** owner)
		{
			for (QAction* a : actionsOf(m))
			{
				if (a->menu())
				{
					if (static_cast<UINT>(a->data().toInt()) == id && a->data().isValid())
					{
						if (owner)
							*owner = m;
						return a;
					}
					if (QAction* x = findById(a->menu(), id, owner))
						return x;
				}
				else if (!a->isSeparator() && a->data().isValid() && static_cast<UINT>(a->data().toInt()) == id)
				{
					if (owner)
						*owner = m;
					return a;
				}
			}
			return nullptr;
		}

		QAction* findItem(HMENU h, UINT item, bool byPosition, QWidget** owner = nullptr)
		{
			QWidget* m = widgetOfMenu(h);
			if (!m)
				return nullptr;
			if (byPosition)
			{
				if (owner)
					*owner = m;
				return actionsOf(m).value(static_cast<int>(item));
			}
			return findById(m, item, owner);
		}

		QString menuText(const QString& t)
		{
			// "Item\tCtrl+X": the shortcut text is shown by Qt from the text after a tab too
			return t;
		}

		QAction* makeAction(QWidget* m, UINT flags, UINT_PTR id, LPCWSTR text, bool host)
		{
			QAction* a = nullptr;
			if (flags & MF_SEPARATOR)
			{
				a = new QAction(m);
				a->setSeparator(true);
				return a;
			}
			if (flags & MF_POPUP)
			{
				QMenu* sub = menuOf(reinterpret_cast<HMENU>(id));
				if (!sub)
					sub = new QMenu(m);
				sub->setTitle(menuText(qs(text)));
				a = sub->menuAction();
			}
			else
			{
				a = new QAction(menuText((flags & MF_BITMAP) ? QString() : qs(text)), m);
				a->setData(static_cast<int>(id));
				if (host)
				{
					// an item of a plugin in a menu of Notepad++: WM_COMMAND to Notepad++, which
					// gives the dynamic ids (NPPM_ALLOCATECMDID) to the plugins
					const int cmd = static_cast<int>(id);
					QObject::connect(a, &QAction::triggered, a, [cmd]
					{
						SendMessageW(nppHandle(), WM_COMMAND, static_cast<WPARAM>(cmd), 0);
					});
				}
			}
			if (flags & MF_CHECKED)
			{
				a->setCheckable(true);
				a->setChecked(true);
			}
			a->setEnabled(!(flags & (MF_GRAYED | MF_DISABLED)));
			return a;
		}
	}

	QMenu* menuOf(HMENU h)
	{
		return qobject_cast<QMenu*>(widgetOfMenu(h));
	}

	HMENU menuHandle(QMenu* m)
	{
		if (!m)
			return nullptr;
		const QVariant v = m->property("_w32Menu");
		if (v.isValid())
			return static_cast<HMENU>(v.value<void*>());
		auto* d = new MenuData; // lives as long as the menu
		d->menu = m;
		d->handle.kind = NPP_HANDLE_MENU;
		d->handle.send = menuSend;
		d->handle.widget = m;
		d->handle.host = d;
		d->handle.owner = nullptr;
		m->setProperty("_w32Menu", QVariant::fromValue(static_cast<void*>(&d->handle)));
		QObject::connect(m, &QObject::destroyed, [d]
		{
			d->handle.widget = nullptr;
		});
		return &d->handle;
	}

	HMENU buildMenu(const NppRcMenu* rc)
	{
		if (!rc)
			return nullptr;
		auto* root = new QMenu;
		std::vector<QMenu*> stack{root};
		for (int i = 0; i < rc->count; ++i)
		{
			const NppRcMenuItem& it = rc->items[i];
			while (static_cast<int>(stack.size()) > it.depth + 1)
				stack.pop_back();
			QMenu* parent = stack.back();
			if (it.separator)
			{
				parent->addSeparator();
			}
			else if (it.popup)
			{
				QMenu* sub = parent->addMenu(qs(it.text));
				sub->setEnabled(!(it.flags & (MF_GRAYED | MF_DISABLED)));
				stack.push_back(sub);
			}
			else
			{
				QAction* a = parent->addAction(qs(it.text));
				a->setData(it.id);
				if (it.flags & MF_CHECKED)
				{
					a->setCheckable(true);
					a->setChecked(true);
				}
				a->setEnabled(!(it.flags & (MF_GRAYED | MF_DISABLED)));
			}
		}
		return menuHandle(root);
	}
}

using namespace w32;

HMENU CreateMenu()
{
	return menuHandle(new QMenu);
}

HMENU CreatePopupMenu()
{
	return menuHandle(new QMenu);
}

BOOL DestroyMenu(HMENU hMenu)
{
	if (isHostMenu(hMenu))
		return FALSE;
	QMenu* m = menuOf(hMenu);
	if (!m)
		return FALSE;
	m->deleteLater();
	return TRUE;
}

HMENU GetMenu(HWND hWnd)
{
	if (hWnd && hWnd->magic == NPP_HANDLE_MAGIC && hWnd->kind == NPP_HANDLE_NOTEPAD)
		return reinterpret_cast<HMENU>(SendMessageW(hWnd, W32_NPPM_GETMENUHANDLE, 1 /* NPPMAINMENU */, 0));
	Window* w = win(hWnd);
	return w ? w->menu : nullptr;
}

BOOL SetMenu(HWND hWnd, HMENU hMenu)
{
	Window* w = win(hWnd);
	if (!w)
		return FALSE;
	w->menu = hMenu;
	return TRUE;
}

HMENU GetSubMenu(HMENU hMenu, int pos)
{
	QAction* a = actionsOf(widgetOfMenu(hMenu)).value(pos);
	if (!a || !a->menu())
		return nullptr;
	HMENU sub = menuHandle(a->menu());
	if (isHostMenu(hMenu))
		sub->owner = hMenu->owner; // a menu of Notepad++ too
	return sub;
}

HMENU GetSystemMenu(HWND, BOOL)
{
	return nullptr;
}

int GetMenuItemCount(HMENU hMenu)
{
	QWidget* m = widgetOfMenu(hMenu);
	return m ? static_cast<int>(m->actions().size()) : -1;
}

UINT GetMenuItemID(HMENU hMenu, int pos)
{
	QAction* a = actionsOf(widgetOfMenu(hMenu)).value(pos);
	if (!a)
		return static_cast<UINT>(-1);
	if (a->menu())
		return static_cast<UINT>(-1);
	return a->isSeparator() ? 0 : static_cast<UINT>(a->data().toInt());
}

UINT GetMenuState(HMENU hMenu, UINT id, UINT flags)
{
	QAction* a = findItem(hMenu, id, flags & MF_BYPOSITION);
	if (!a)
		return static_cast<UINT>(-1);
	UINT s = 0;
	if (a->isSeparator())
		s |= MF_SEPARATOR;
	if (a->isChecked())
		s |= MF_CHECKED;
	if (!a->isEnabled())
		s |= MF_GRAYED;
	if (a->menu())
		s |= MF_POPUP | (static_cast<UINT>(a->menu()->actions().size()) << 8);
	if (isHostMenu(hMenu) && !a->menu())
	{
		if (SendMessageW(hMenu->owner, NPPM_LINUX_ISMENUITEMCHECKED, static_cast<WPARAM>(a->data().toInt()), 0))
			s |= MF_CHECKED;
		else
			s &= ~MF_CHECKED;
	}
	return s;
}

int GetMenuStringW(HMENU hMenu, UINT id, LPWSTR text, int max, UINT flags)
{
	QAction* a = findItem(hMenu, id, flags & MF_BYPOSITION);
	if (!a)
		return 0;
	const QString t = a->text();
	if (!text || max <= 0)
		return static_cast<int>(ws(t).size());
	return copyOut(t, text, max);
}

BOOL AppendMenuW(HMENU hMenu, UINT flags, UINT_PTR id, LPCWSTR text)
{
	QWidget* m = widgetOfMenu(hMenu);
	if (!m)
		return FALSE;
	QAction* a = makeAction(m, flags, id, text, isHostMenu(hMenu));
	m->addAction(a);
	return TRUE;
}

BOOL InsertMenuW(HMENU hMenu, UINT pos, UINT flags, UINT_PTR id, LPCWSTR text)
{
	QWidget* owner = nullptr;
	QAction* before = (pos == 0xFFFFFFFF) ? nullptr : findItem(hMenu, pos, flags & MF_BYPOSITION, &owner);
	QWidget* m = owner ? owner : widgetOfMenu(hMenu);
	if (!m)
		return FALSE;
	QAction* a = makeAction(m, flags, id, text, isHostMenu(hMenu));
	m->insertAction(before, a);
	return TRUE;
}

BOOL ModifyMenuW(HMENU hMenu, UINT pos, UINT flags, UINT_PTR id, LPCWSTR text)
{
	QAction* a = findItem(hMenu, pos, flags & MF_BYPOSITION);
	if (!a)
		return FALSE;
	if (!(flags & MF_SEPARATOR) && text && !(flags & MF_BITMAP))
		a->setText(qs(text));
	if (!(flags & MF_POPUP))
		a->setData(static_cast<int>(id));
	a->setEnabled(!(flags & (MF_GRAYED | MF_DISABLED)));
	if (flags & MF_CHECKED)
	{
		a->setCheckable(true);
		a->setChecked(true);
	}
	return TRUE;
}

namespace
{
	void applyItemInfo(QAction* a, HMENU hMenu, const MENUITEMINFOW* mii)
	{
		if (mii->fMask & (MIIM_STRING | MIIM_TYPE))
		{
			if (!((mii->fMask & MIIM_TYPE) && (mii->fType & MFT_SEPARATOR)) && mii->dwTypeData)
				a->setText(qs(mii->dwTypeData));
		}
		if (mii->fMask & MIIM_ID)
			a->setData(static_cast<int>(mii->wID));
		if (mii->fMask & MIIM_DATA)
			a->setProperty("_w32ItemData", static_cast<qulonglong>(mii->dwItemData));
		if (mii->fMask & MIIM_STATE)
		{
			a->setEnabled(!(mii->fState & MFS_GRAYED));
			if (isHostMenu(hMenu) && !a->menu())
				SendMessageW(hMenu->owner, W32_NPPM_SETMENUITEMCHECK, static_cast<WPARAM>(a->data().toInt()), (mii->fState & MFS_CHECKED) ? TRUE : FALSE);
			else
			{
				if (mii->fState & MFS_CHECKED)
					a->setCheckable(true);
				a->setChecked(mii->fState & MFS_CHECKED);
			}
		}
		if ((mii->fMask & MIIM_SUBMENU) && mii->hSubMenu)
		{
			if (QMenu* sub = menuOf(mii->hSubMenu))
				a->setMenu(sub);
		}
	}
}

BOOL InsertMenuItemW(HMENU hMenu, UINT item, BOOL byPosition, LPCMENUITEMINFOW mii)
{
	QWidget* owner = nullptr;
	QAction* before = (item == 0xFFFFFFFF) ? nullptr : findItem(hMenu, item, byPosition, &owner);
	QWidget* m = owner ? owner : widgetOfMenu(hMenu);
	if (!m || !mii)
		return FALSE;
	// an item without text, bitmap or submenu (fMask 0, as plugins insert separators) is drawn as a separator
	const bool separator = (((mii->fMask & MIIM_FTYPE) || (mii->fMask & MIIM_TYPE)) && (mii->fType & MFT_SEPARATOR))
		|| !(mii->fMask & (MIIM_STRING | MIIM_TYPE | MIIM_BITMAP | MIIM_SUBMENU));
	UINT flags = separator ? MF_SEPARATOR : MF_STRING;
	if ((mii->fMask & MIIM_SUBMENU) && mii->hSubMenu)
		flags |= MF_POPUP;
	QAction* a = makeAction(m, flags, (flags & MF_POPUP) ? reinterpret_cast<UINT_PTR>(mii->hSubMenu) : mii->wID,
		(mii->fMask & (MIIM_STRING | MIIM_TYPE)) ? mii->dwTypeData : L"", isHostMenu(hMenu));
	m->insertAction(before, a);
	applyItemInfo(a, hMenu, mii);
	return TRUE;
}

BOOL SetMenuItemInfoW(HMENU hMenu, UINT item, BOOL byPosition, LPCMENUITEMINFOW mii)
{
	QAction* a = findItem(hMenu, item, byPosition);
	if (!a || !mii)
		return FALSE;
	applyItemInfo(a, hMenu, mii);
	return TRUE;
}

BOOL GetMenuItemInfoW(HMENU hMenu, UINT item, BOOL byPosition, LPMENUITEMINFOW mii)
{
	QAction* a = findItem(hMenu, item, byPosition);
	if (!a || !mii)
		return FALSE;
	if (mii->fMask & MIIM_ID)
		mii->wID = a->menu() ? 0 : static_cast<UINT>(a->data().toInt());
	if (mii->fMask & MIIM_SUBMENU)
		mii->hSubMenu = a->menu() ? menuHandle(a->menu()) : nullptr;
	if (mii->fMask & MIIM_DATA)
		mii->dwItemData = static_cast<ULONG_PTR>(a->property("_w32ItemData").toULongLong());
	if (mii->fMask & (MIIM_FTYPE | MIIM_TYPE))
		mii->fType = a->isSeparator() ? MFT_SEPARATOR : MFT_STRING;
	if (mii->fMask & MIIM_STATE)
	{
		mii->fState = (a->isEnabled() ? 0 : MFS_GRAYED) | (a->isChecked() ? MFS_CHECKED : 0);
		if (isHostMenu(hMenu) && !a->menu())
			mii->fState = (mii->fState & ~MFS_CHECKED) | (SendMessageW(hMenu->owner, NPPM_LINUX_ISMENUITEMCHECKED, static_cast<WPARAM>(a->data().toInt()), 0) ? MFS_CHECKED : 0);
	}
	if (mii->fMask & (MIIM_STRING | MIIM_TYPE))
	{
		const std::wstring t = ws(a->text());
		if (mii->dwTypeData && mii->cch > 0)
			copyOut(a->text(), mii->dwTypeData, static_cast<int>(mii->cch) + 1);
		mii->cch = static_cast<UINT>(t.size());
	}
	return TRUE;
}

BOOL RemoveMenu(HMENU hMenu, UINT pos, UINT flags)
{
	QWidget* owner = nullptr;
	QAction* a = findItem(hMenu, pos, flags & MF_BYPOSITION, &owner);
	if (!a || !owner)
		return FALSE;
	owner->removeAction(a);
	return TRUE;
}

BOOL DeleteMenu(HMENU hMenu, UINT pos, UINT flags)
{
	QWidget* owner = nullptr;
	QAction* a = findItem(hMenu, pos, flags & MF_BYPOSITION, &owner);
	if (!a || !owner)
		return FALSE;
	owner->removeAction(a);
	if (a->menu())
		a->menu()->deleteLater();
	else if (a->parent() == owner)
		a->deleteLater();
	return TRUE;
}

DWORD CheckMenuItem(HMENU hMenu, UINT id, UINT flags)
{
	QAction* a = findItem(hMenu, id, flags & MF_BYPOSITION);
	if (isHostMenu(hMenu))
	{
		// a plugin command (NPPM_SETMENUITEMCHECK keeps the check mark)
		const UINT cmd = a ? static_cast<UINT>(a->data().toInt()) : id;
		if ((flags & MF_BYPOSITION) && !a)
			return static_cast<DWORD>(-1);
		const BOOL was = static_cast<BOOL>(SendMessageW(hMenu->owner, NPPM_LINUX_ISMENUITEMCHECKED, cmd, 0));
		SendMessageW(hMenu->owner, W32_NPPM_SETMENUITEMCHECK, cmd, (flags & MF_CHECKED) ? TRUE : FALSE);
		return was ? MF_CHECKED : MF_UNCHECKED;
	}
	if (!a)
		return static_cast<DWORD>(-1);
	const bool was = a->isChecked();
	if (flags & MF_CHECKED)
		a->setCheckable(true);
	a->setChecked(flags & MF_CHECKED);
	return was ? MF_CHECKED : MF_UNCHECKED;
}

BOOL EnableMenuItem(HMENU hMenu, UINT id, UINT flags)
{
	QAction* a = findItem(hMenu, id, flags & MF_BYPOSITION);
	if (isHostMenu(hMenu) && !(flags & MF_BYPOSITION))
	{
		SendMessageW(hMenu->owner, NPPM_LINUX_ENABLEMENUITEM, id, (flags & (MF_GRAYED | MF_DISABLED)) ? FALSE : TRUE);
		return TRUE;
	}
	if (!a)
		return -1;
	const bool was = a->isEnabled();
	a->setEnabled(!(flags & (MF_GRAYED | MF_DISABLED)));
	return was ? MF_ENABLED : MF_GRAYED;
}

BOOL CheckMenuRadioItem(HMENU hMenu, UINT first, UINT last, UINT check, UINT flags)
{
	for (UINT i = first; i <= last; ++i)
		CheckMenuItem(hMenu, i, (flags & MF_BYPOSITION) | (i == check ? MF_CHECKED : MF_UNCHECKED));
	return TRUE;
}

BOOL SetMenuDefaultItem(HMENU hMenu, UINT item, UINT byPos)
{
	QAction* a = findItem(hMenu, item, byPos != 0);
	QMenu* m = menuOf(hMenu);
	if (!a || !m)
		return FALSE;
	m->setDefaultAction(a);
	return TRUE;
}

BOOL TrackPopupMenu(HMENU hMenu, UINT flags, int x, int y, int, HWND hWnd, const RECT*)
{
	QMenu* m = menuOf(hMenu);
	if (!m)
		return FALSE;
	if (hWnd)
		SendMessageW(hWnd, WM_INITMENUPOPUP, reinterpret_cast<WPARAM>(hMenu), 0);
	QPoint pos(x, y);
	if (flags & TPM_RIGHTALIGN)
		pos.rx() -= m->sizeHint().width();
	else if (flags & TPM_CENTERALIGN)
		pos.rx() -= m->sizeHint().width() / 2;
	if (flags & TPM_BOTTOMALIGN)
		pos.ry() -= m->sizeHint().height();
	else if (flags & TPM_VCENTERALIGN)
		pos.ry() -= m->sizeHint().height() / 2;
	QAction* chosen = m->exec(pos);
	const int id = (chosen && !chosen->menu() && chosen->data().isValid()) ? chosen->data().toInt() : 0;
	if (flags & TPM_RETURNCMD)
		return id;
	if (id && hWnd && !(flags & TPM_NONOTIFY))
		PostMessageW(hWnd, WM_COMMAND, MAKEWPARAM(id, 0), 0);
	return id ? TRUE : FALSE;
}

BOOL TrackPopupMenuEx(HMENU hMenu, UINT flags, int x, int y, HWND hWnd, void*)
{
	return TrackPopupMenu(hMenu, flags, x, y, 0, hWnd, nullptr);
}

HMENU LoadMenuW(HINSTANCE, LPCWSTR name)
{
	return buildMenu(findMenu(name));
}

namespace
{
	struct AccelTable
	{
		std::vector<ACCEL> items;
	};
}

HACCEL CreateAcceleratorTableW(LPACCEL accels, int count)
{
	auto* t = new AccelTable;
	for (int i = 0; accels && i < count; ++i)
		t->items.push_back(accels[i]);
	return t;
}

BOOL DestroyAcceleratorTable(HACCEL hAccel)
{
	delete static_cast<AccelTable*>(hAccel);
	return TRUE;
}

HACCEL LoadAcceleratorsW(HINSTANCE, LPCWSTR name)
{
	const NppRcAccelTable* rc = findAccel(name);
	if (!rc)
		return nullptr;
	auto* t = new AccelTable;
	for (int i = 0; i < rc->count; ++i)
		t->items.push_back(ACCEL{rc->items[i].fVirt, rc->items[i].key, rc->items[i].cmd});
	return t;
}

BOOL IsMenu(HMENU menu)
{
	return w32::menuOf(menu) ? TRUE : FALSE;
}

BOOL GetMenuBarInfo(HWND hWnd, LONG idObject, LONG idItem, PMENUBARINFO info)
{
	if (!info || idObject != OBJID_MENU)
		return FALSE;
	QWidget* w = widgetOf(hWnd);
	QMenuBar* bar = w ? w->window()->findChild<QMenuBar*>(QString(), Qt::FindDirectChildrenOnly) : nullptr;
	if (!bar)
		bar = w ? w->window()->findChild<QMenuBar*>() : nullptr;
	info->hMenu = GetMenu(hWnd);
	info->hwndMenu = nullptr;
	info->fBarFocused = FALSE;
	info->fFocused = FALSE;
	if (!bar)
	{
		info->rcBar = RECT{};
		return info->hMenu != nullptr;
	}
	QRect r = bar->rect();
	if (idItem > 0)
	{
		const QList<QAction*> actions = bar->actions();
		if (idItem > actions.size())
			return FALSE;
		r = bar->actionGeometry(actions[idItem - 1]);
	}
	const QPoint g = bar->mapToGlobal(r.topLeft());
	info->rcBar = RECT{g.x(), g.y(), g.x() + r.width(), g.y() + r.height()};
	return TRUE;
}
