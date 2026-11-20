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

// Notepad++ Linux plugin SDK, npp_win32 library: the common controls (list view and
// tree view on QTreeWidget, tab control, trackbar, up-down, progress bar, tooltips,
// SysLink, status bar) and image lists.

#include <QApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QHelpEvent>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QProgressBar>
#include <QRegularExpression>
#include <QSlider>
#include <QStyledItemDelegate>
#include <QTabBar>
#include <QTimer>
#include <QToolButton>
#include <QToolTip>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "W32Internal.h"

struct _IMAGELIST
{
	int cx = 16;
	int cy = 16;
	std::vector<QIcon> icons;
};

namespace w32
{
	bool isCommonControl(Kind k)
	{
		switch (k)
		{
			case Kind::ListView: case Kind::TreeView: case Kind::Tab: case Kind::Trackbar: case Kind::UpDown:
			case Kind::Progress: case Kind::Tooltip: case Kind::Link: case Kind::Header: case Kind::StatusBar:
				return true;
			default:
				return false;
		}
	}

	namespace
	{
		constexpr int RoleParam = Qt::UserRole + 1;
		constexpr int RoleImage = Qt::UserRole + 2;
		constexpr int RoleSelImage = Qt::UserRole + 3;
		constexpr int RoleCallback = Qt::UserRole + 4; // text from LVN_GETDISPINFO / TVN_GETDISPINFO
		constexpr int RoleChildren = Qt::UserRole + 5;
		constexpr int RoleExpandedOnce = Qt::UserRole + 6;
		constexpr int RoleState = Qt::UserRole + 8;
		constexpr int RoleImageCallback = Qt::UserRole + 9;

		struct ViewData
		{
			HIMAGELIST images[3] = {};
			DWORD exStyle = 0;
			std::vector<int> lastSelection; // list views: LVN_ITEMCHANGED of the selection changes
			bool silent = false;            // changes made by messages: no notification
			QTreeWidgetItem* dropHilite = nullptr;
			Window* header = nullptr;
			QColor textColor;
			QColor bkColor;
		};

		ViewData* viewData(Window* w)
		{
			return static_cast<ViewData*>(w->ctl);
		}

		QTreeWidget* tree(Window* w)
		{
			return qobject_cast<QTreeWidget*>(w->widget);
		}

		QIcon imageAt(HIMAGELIST il, int index)
		{
			if (!il || index < 0 || index >= static_cast<int>(il->icons.size()))
				return QIcon();
			return il->icons[static_cast<size_t>(index)];
		}

		// text of callback items (LPSTR_TEXTCALLBACK), asked when the item is painted
		class CallbackDelegate : public QStyledItemDelegate
		{
		public:
			CallbackDelegate(Window* w, QObject* parent) : QStyledItemDelegate(parent), _w(w) {}

		protected:
			void initStyleOption(QStyleOptionViewItem* opt, const QModelIndex& index) const override
			{
				QStyledItemDelegate::initStyleOption(opt, index);
				if (!index.data(RoleCallback).toBool() && !index.data(RoleImageCallback).toBool())
					return;
				QTreeWidget* t = tree(_w);
				if (!t)
					return;
				QTreeWidgetItem* item = t->itemFromIndex(index.siblingAtColumn(0));
				if (!item)
					return;
				wchar_t buf[1024] = {};
				if (_w->kind == Kind::ListView)
				{
					NMLVDISPINFOW di{};
					di.item.mask = (index.data(RoleCallback).toBool() ? LVIF_TEXT : 0) | (index.data(RoleImageCallback).toBool() ? LVIF_IMAGE : 0);
					di.item.iItem = t->indexOfTopLevelItem(item);
					di.item.iSubItem = index.column();
					di.item.pszText = buf;
					di.item.cchTextMax = 1024;
					di.item.lParam = static_cast<LPARAM>(item->data(0, RoleParam).toLongLong());
					notify(_w, &di.hdr, LVN_GETDISPINFOW);
					if (di.item.mask & LVIF_TEXT)
						opt->text = qs(di.item.pszText ? di.item.pszText : buf);
					if ((di.item.mask & LVIF_IMAGE) && index.column() == 0)
					{
						opt->icon = imageAt(viewData(_w)->images[LVSIL_SMALL], di.item.iImage);
						if (!opt->icon.isNull())
							opt->features |= QStyleOptionViewItem::HasDecoration;
					}
				}
				else
				{
					NMTVDISPINFOW di{};
					di.item.mask = TVIF_TEXT;
					di.item.hItem = reinterpret_cast<HTREEITEM>(item);
					di.item.pszText = buf;
					di.item.cchTextMax = 1024;
					di.item.lParam = static_cast<LPARAM>(item->data(0, RoleParam).toLongLong());
					notify(_w, &di.hdr, TVN_GETDISPINFOW);
					opt->text = qs(di.item.pszText ? di.item.pszText : buf);
				}
			}

		private:
			Window* _w;
		};

		// ---- list view -------------------------------------------------------------------
		void lvUpdateIcon(Window* w, QTreeWidgetItem* item)
		{
			const int image = item->data(0, RoleImage).toInt();
			item->setIcon(0, imageAt(viewData(w)->images[LVSIL_SMALL], image));
		}

		std::vector<int> lvSelection(QTreeWidget* t)
		{
			std::vector<int> sel;
			for (int i = 0; i < t->topLevelItemCount(); ++i)
			{
				if (t->topLevelItem(i)->isSelected())
					sel.push_back(i);
			}
			return sel;
		}

		void lvNotifyChanged(Window* w, int item, UINT newState, UINT oldState, UINT changed)
		{
			QTreeWidget* t = tree(w);
			NMLISTVIEW nm{};
			nm.iItem = item;
			nm.iSubItem = 0;
			nm.uNewState = newState;
			nm.uOldState = oldState;
			nm.uChanged = changed;
			if (t && item >= 0 && t->topLevelItem(item))
				nm.lParam = static_cast<LPARAM>(t->topLevelItem(item)->data(0, RoleParam).toLongLong());
			notify(w, &nm.hdr, LVN_ITEMCHANGED);
		}

		void lvSelectionChanged(Window* w)
		{
			QTreeWidget* t = tree(w);
			ViewData* d = viewData(w);
			if (!t || !d)
				return;
			const std::vector<int> now = lvSelection(t);
			const int focused = t->currentItem() ? t->indexOfTopLevelItem(t->currentItem()) : -1;
			if (d->silent)
			{
				d->lastSelection = now;
				return;
			}
			const std::vector<int> before = d->lastSelection;
			d->lastSelection = now;
			for (int i : before)
			{
				if (std::find(now.begin(), now.end(), i) == now.end())
					lvNotifyChanged(w, i, 0, LVIS_SELECTED, LVIF_STATE);
			}
			for (int i : now)
			{
				if (std::find(before.begin(), before.end(), i) == before.end())
					lvNotifyChanged(w, i, LVIS_SELECTED | (i == focused ? LVIS_FOCUSED : 0), 0, LVIF_STATE);
			}
		}

		void lvItemActivate(Window* w, QTreeWidgetItem* item, int column, UINT code)
		{
			QTreeWidget* t = tree(w);
			NMITEMACTIVATE nm{};
			nm.iItem = item ? t->indexOfTopLevelItem(item) : -1;
			nm.iSubItem = column;
			const QPoint p = t->viewport()->mapTo(t, t->viewport()->mapFromGlobal(QCursor::pos()));
			nm.ptAction = POINT{p.x(), p.y()};
			if (item)
				nm.lParam = static_cast<LPARAM>(item->data(0, RoleParam).toLongLong());
			const Qt::KeyboardModifiers m = QGuiApplication::keyboardModifiers();
			nm.uKeyFlags = ((m & Qt::ControlModifier) ? 0x0001 : 0) | ((m & Qt::ShiftModifier) ? 0x0002 : 0) | ((m & Qt::AltModifier) ? 0x0004 : 0);
			notify(w, &nm.hdr, code);
		}

		QTreeWidgetItem* lvItem(Window* w, int i)
		{
			QTreeWidget* t = tree(w);
			return (t && i >= 0 && i < t->topLevelItemCount()) ? t->topLevelItem(i) : nullptr;
		}

		void lvSetText(Window* w, QTreeWidgetItem* item, int sub, LPCWSTR text)
		{
			if (text == LPSTR_TEXTCALLBACKW)
			{
				item->setData(sub, RoleCallback, true);
				item->setText(sub, QString());
			}
			else
			{
				item->setData(sub, RoleCallback, false);
				item->setText(sub, qs(text));
			}
			Q_UNUSED(w);
		}

		UINT lvState(Window* w, QTreeWidgetItem* item, UINT mask)
		{
			QTreeWidget* t = tree(w);
			UINT s = 0;
			if ((mask & LVIS_SELECTED) && item->isSelected())
				s |= LVIS_SELECTED;
			if ((mask & LVIS_FOCUSED) && t->currentItem() == item)
				s |= LVIS_FOCUSED;
			if (mask & LVIS_STATEIMAGEMASK)
			{
				if (viewData(w)->exStyle & LVS_EX_CHECKBOXES)
					s |= INDEXTOSTATEIMAGEMASK(item->checkState(0) == Qt::Checked ? 2 : 1);
				else
					s |= item->data(0, RoleState).toUInt() & LVIS_STATEIMAGEMASK;
			}
			s |= item->data(0, RoleState).toUInt() & mask & ~(LVIS_SELECTED | LVIS_FOCUSED | LVIS_STATEIMAGEMASK);
			return s;
		}

		void lvSetState(Window* w, QTreeWidgetItem* item, UINT state, UINT mask)
		{
			QTreeWidget* t = tree(w);
			if (mask & LVIS_SELECTED)
				item->setSelected(state & LVIS_SELECTED);
			if ((mask & LVIS_FOCUSED) && (state & LVIS_FOCUSED))
				t->setCurrentItem(item, 0, QItemSelectionModel::NoUpdate);
			if (mask & LVIS_STATEIMAGEMASK)
			{
				if (viewData(w)->exStyle & LVS_EX_CHECKBOXES)
					item->setCheckState(0, ((state & LVIS_STATEIMAGEMASK) >> 12) == 2 ? Qt::Checked : Qt::Unchecked);
			}
			const UINT other = mask & ~(LVIS_SELECTED | LVIS_FOCUSED);
			if (other)
			{
				const UINT s = item->data(0, RoleState).toUInt();
				item->setData(0, RoleState, (s & ~other) | (state & other));
			}
		}

		LRESULT listViewProc(Window* w, UINT msg, WPARAM wParam, LPARAM lParam)
		{
			QTreeWidget* t = tree(w);
			ViewData* d = viewData(w);
			if (!t || !d)
				return -0x7FFFFFFF;
			switch (msg)
			{
				case LVM_GETITEMCOUNT:
					return t->topLevelItemCount();
				case LVM_SETITEMCOUNT:
					while (t->topLevelItemCount() < static_cast<int>(wParam))
					{
						auto* item = new QTreeWidgetItem;
						for (int c = 0; c < std::max(1, t->columnCount()); ++c)
							item->setData(c, RoleCallback, true);
						t->addTopLevelItem(item);
					}
					while (t->topLevelItemCount() > static_cast<int>(wParam))
						delete t->takeTopLevelItem(t->topLevelItemCount() - 1);
					return TRUE;
				case LVM_INSERTCOLUMNW:
				{
					const auto* col = reinterpret_cast<const LVCOLUMNW*>(lParam);
					const int n = t->columnCount();
					int at = std::clamp(static_cast<int>(wParam), 0, n);
					QStringList labels;
					for (int c = 0; c < n; ++c)
						labels << t->headerItem()->text(c);
					// QTreeWidget adds columns at the end: move the texts of the columns after
					t->setColumnCount(n + 1);
					labels.insert(at, (col && (col->mask & LVCF_TEXT)) ? qs(col->pszText) : QString());
					t->setHeaderLabels(labels);
					if (at < n)
					{
						for (int i = 0; i < t->topLevelItemCount(); ++i)
						{
							QTreeWidgetItem* item = t->topLevelItem(i);
							for (int c = n; c > at; --c)
								item->setText(c, item->text(c - 1));
							item->setText(at, QString());
						}
					}
					if (col && (col->mask & LVCF_WIDTH))
						t->setColumnWidth(at, std::max(col->cx, 0));
					if (col && (col->mask & LVCF_FMT))
					{
						const int fmt = col->fmt & LVCFMT_JUSTIFYMASK;
						t->headerItem()->setTextAlignment(at, fmt == LVCFMT_RIGHT ? Qt::AlignRight : (fmt == LVCFMT_CENTER ? Qt::AlignHCenter : Qt::AlignLeft));
					}
					return at;
				}
				case LVM_SETCOLUMNW:
				case LVM_GETCOLUMNW:
				{
					auto* col = reinterpret_cast<LVCOLUMNW*>(lParam);
					const int c = static_cast<int>(wParam);
					if (!col || c < 0 || c >= t->columnCount())
						return FALSE;
					if (msg == LVM_SETCOLUMNW)
					{
						if (col->mask & LVCF_TEXT)
							t->headerItem()->setText(c, qs(col->pszText));
						if (col->mask & LVCF_WIDTH)
							t->setColumnWidth(c, col->cx);
					}
					else
					{
						if (col->mask & LVCF_TEXT)
							copyOut(t->headerItem()->text(c), col->pszText, col->cchTextMax);
						if (col->mask & LVCF_WIDTH)
							col->cx = t->columnWidth(c);
						if (col->mask & LVCF_FMT)
							col->fmt = LVCFMT_LEFT;
						if (col->mask & LVCF_SUBITEM)
							col->iSubItem = c;
					}
					return TRUE;
				}
				case LVM_DELETECOLUMN:
				{
					const int c = static_cast<int>(wParam);
					const int n = t->columnCount();
					if (c < 0 || c >= n)
						return FALSE;
					QStringList labels;
					for (int k = 0; k < n; ++k)
					{
						if (k != c)
							labels << t->headerItem()->text(k);
					}
					for (int i = 0; i < t->topLevelItemCount(); ++i)
					{
						QTreeWidgetItem* item = t->topLevelItem(i);
						for (int k = c; k < n - 1; ++k)
							item->setText(k, item->text(k + 1));
					}
					t->setColumnCount(std::max(1, n - 1));
					t->setHeaderLabels(labels);
					return TRUE;
				}
				case LVM_SETCOLUMNWIDTH:
				{
					const int c = static_cast<int>(wParam);
					const int cx = static_cast<short>(LOWORD(lParam));
					if (c < 0 || c >= std::max(1, t->columnCount()))
						return FALSE;
					if (cx == LVSCW_AUTOSIZE)
						t->resizeColumnToContents(c);
					else if (cx == LVSCW_AUTOSIZE_USEHEADER)
					{
						if (c == t->columnCount() - 1)
						{
							int used = 0;
							for (int k = 0; k < c; ++k)
								used += t->columnWidth(k);
							t->setColumnWidth(c, std::max(t->viewport()->width() - used, t->header()->sectionSizeHint(c)));
						}
						else
						{
							t->resizeColumnToContents(c);
							t->setColumnWidth(c, std::max(t->columnWidth(c), t->header()->sectionSizeHint(c)));
						}
					}
					else
						t->setColumnWidth(c, std::max(cx, 0));
					return TRUE;
				}
				case LVM_GETCOLUMNWIDTH:
					return t->columnWidth(static_cast<int>(wParam));
				case LVM_INSERTITEMW:
				{
					const auto* lv = reinterpret_cast<const LVITEMW*>(lParam);
					if (!lv)
						return -1;
					auto* item = new QTreeWidgetItem;
					if (d->exStyle & LVS_EX_CHECKBOXES)
						item->setCheckState(0, Qt::Unchecked);
					if (lv->mask & LVIF_TEXT)
						lvSetText(w, item, 0, lv->pszText);
					if (lv->mask & LVIF_PARAM)
						item->setData(0, RoleParam, static_cast<qlonglong>(lv->lParam));
					if (lv->mask & LVIF_IMAGE)
					{
						item->setData(0, RoleImageCallback, lv->iImage == I_IMAGECALLBACK);
						item->setData(0, RoleImage, lv->iImage);
						lvUpdateIcon(w, item);
					}
					int at = std::clamp(lv->iItem, 0, t->topLevelItemCount());
					if (w->style & (LVS_SORTASCENDING | LVS_SORTDESCENDING))
					{
						const QString text = item->text(0);
						at = 0;
						while (at < t->topLevelItemCount())
						{
							const int cmp = QString::compare(t->topLevelItem(at)->text(0), text, Qt::CaseInsensitive);
							if ((w->style & LVS_SORTASCENDING) ? cmp > 0 : cmp < 0)
								break;
							++at;
						}
					}
					d->silent = true;
					t->insertTopLevelItem(at, item);
					if (lv->mask & LVIF_STATE)
						lvSetState(w, item, lv->state, lv->stateMask);
					d->silent = false;
					d->lastSelection = lvSelection(t);
					return at;
				}
				case LVM_DELETEITEM:
				{
					QTreeWidgetItem* item = lvItem(w, static_cast<int>(wParam));
					if (!item)
						return FALSE;
					NMLISTVIEW nm{};
					nm.iItem = static_cast<int>(wParam);
					nm.lParam = static_cast<LPARAM>(item->data(0, RoleParam).toLongLong());
					notify(w, &nm.hdr, LVN_DELETEITEM);
					d->silent = true;
					delete t->takeTopLevelItem(static_cast<int>(wParam));
					d->silent = false;
					d->lastSelection = lvSelection(t);
					return TRUE;
				}
				case LVM_DELETEALLITEMS:
				{
					NMLISTVIEW nm{};
					nm.iItem = -1;
					const bool each = !notify(w, &nm.hdr, LVN_DELETEALLITEMS);
					if (each)
					{
						for (int i = 0; i < t->topLevelItemCount(); ++i)
						{
							NMLISTVIEW one{};
							one.iItem = i;
							one.lParam = static_cast<LPARAM>(t->topLevelItem(i)->data(0, RoleParam).toLongLong());
							notify(w, &one.hdr, LVN_DELETEITEM);
						}
					}
					d->silent = true;
					t->clear();
					d->silent = false;
					d->lastSelection.clear();
					return TRUE;
				}
				case LVM_SETITEMW:
				case LVM_GETITEMW:
				{
					auto* lv = reinterpret_cast<LVITEMW*>(lParam);
					QTreeWidgetItem* item = lv ? lvItem(w, lv->iItem) : nullptr;
					if (!item)
						return FALSE;
					const int sub = lv->iSubItem;
					if (msg == LVM_SETITEMW)
					{
						if (lv->mask & LVIF_TEXT)
						{
							if (sub >= t->columnCount())
								t->setColumnCount(sub + 1);
							lvSetText(w, item, sub, lv->pszText);
						}
						if ((lv->mask & LVIF_PARAM) && sub == 0)
							item->setData(0, RoleParam, static_cast<qlonglong>(lv->lParam));
						if ((lv->mask & LVIF_IMAGE) && sub == 0)
						{
							item->setData(0, RoleImage, lv->iImage);
							item->setData(0, RoleImageCallback, lv->iImage == I_IMAGECALLBACK);
							lvUpdateIcon(w, item);
						}
						if (lv->mask & LVIF_STATE)
							lvSetState(w, item, lv->state, lv->stateMask);
					}
					else
					{
						if (lv->mask & LVIF_TEXT)
							copyOut(item->text(sub), lv->pszText, lv->cchTextMax);
						if (lv->mask & LVIF_PARAM)
							lv->lParam = static_cast<LPARAM>(item->data(0, RoleParam).toLongLong());
						if (lv->mask & LVIF_IMAGE)
							lv->iImage = item->data(0, RoleImage).toInt();
						if (lv->mask & LVIF_STATE)
							lv->state = lvState(w, item, lv->stateMask);
						if (lv->mask & LVIF_INDENT)
							lv->iIndent = 0;
					}
					return TRUE;
				}
				case LVM_SETITEMTEXTW:
				{
					auto* lv = reinterpret_cast<LVITEMW*>(lParam);
					QTreeWidgetItem* item = lvItem(w, static_cast<int>(wParam));
					if (!item || !lv)
						return FALSE;
					if (lv->iSubItem >= t->columnCount())
						t->setColumnCount(lv->iSubItem + 1);
					lvSetText(w, item, lv->iSubItem, lv->pszText);
					return TRUE;
				}
				case LVM_GETITEMTEXTW:
				{
					auto* lv = reinterpret_cast<LVITEMW*>(lParam);
					QTreeWidgetItem* item = lvItem(w, static_cast<int>(wParam));
					if (!item || !lv)
						return 0;
					return copyOut(item->text(lv->iSubItem), lv->pszText, lv->cchTextMax);
				}
				case LVM_SETITEMSTATE:
				{
					const auto* lv = reinterpret_cast<const LVITEMW*>(lParam);
					if (!lv)
						return FALSE;
					const int i = static_cast<int>(static_cast<intptr_t>(wParam));
					d->silent = false;
					if (i < 0)
					{
						for (int k = 0; k < t->topLevelItemCount(); ++k)
							lvSetState(w, t->topLevelItem(k), lv->state, lv->stateMask);
					}
					else if (QTreeWidgetItem* item = lvItem(w, i))
					{
						lvSetState(w, item, lv->state, lv->stateMask);
					}
					return TRUE;
				}
				case LVM_GETITEMSTATE:
				{
					QTreeWidgetItem* item = lvItem(w, static_cast<int>(wParam));
					return item ? lvState(w, item, static_cast<UINT>(lParam)) : 0;
				}
				case LVM_GETNEXTITEM:
				{
					const int start = static_cast<int>(static_cast<intptr_t>(wParam));
					const UINT flags = LOWORD(lParam);
					const int n = t->topLevelItemCount();
					if (flags & LVNI_FOCUSED)
					{
						const int cur = t->currentItem() ? t->indexOfTopLevelItem(t->currentItem()) : -1;
						if (cur > start && (!(flags & LVNI_SELECTED) || t->currentItem()->isSelected()))
							return cur;
						if (start < 0 && cur >= 0)
							return cur;
						return -1;
					}
					if (flags & LVNI_ABOVE)
						return start > 0 ? start - 1 : -1;
					for (int i = start + 1; i < n; ++i)
					{
						if (!(flags & LVNI_SELECTED) || t->topLevelItem(i)->isSelected())
							return i;
					}
					return -1;
				}
				case LVM_GETSELECTEDCOUNT:
					return static_cast<LRESULT>(lvSelection(t).size());
				case LVM_GETSELECTIONMARK:
					return t->currentItem() ? t->indexOfTopLevelItem(t->currentItem()) : -1;
				case LVM_SETSELECTIONMARK:
				{
					const int old = t->currentItem() ? t->indexOfTopLevelItem(t->currentItem()) : -1;
					if (QTreeWidgetItem* item = lvItem(w, static_cast<int>(lParam)))
						t->setCurrentItem(item, 0, QItemSelectionModel::NoUpdate);
					return old;
				}
				case LVM_ENSUREVISIBLE:
					if (QTreeWidgetItem* item = lvItem(w, static_cast<int>(wParam)))
						t->scrollToItem(item);
					return TRUE;
				case LVM_SETEXTENDEDLISTVIEWSTYLE:
				{
					const DWORD old = d->exStyle;
					const DWORD mask = wParam ? static_cast<DWORD>(wParam) : 0xFFFFFFFF;
					d->exStyle = (d->exStyle & ~mask) | (static_cast<DWORD>(lParam) & mask);
					if ((d->exStyle & LVS_EX_CHECKBOXES) && !(old & LVS_EX_CHECKBOXES))
					{
						for (int i = 0; i < t->topLevelItemCount(); ++i)
							t->topLevelItem(i)->setCheckState(0, Qt::Unchecked);
					}
					if (d->exStyle & LVS_EX_GRIDLINES)
						t->setStyleSheet(QStringLiteral("QTreeView::item { border-bottom: 1px solid palette(midlight); border-right: 1px solid palette(midlight); }"));
					return old;
				}
				case LVM_GETEXTENDEDLISTVIEWSTYLE:
					return d->exStyle;
				case LVM_SETIMAGELIST:
				{
					const int type = static_cast<int>(wParam);
					if (type < 0 || type > 2)
						return 0;
					HIMAGELIST old = d->images[type];
					d->images[type] = reinterpret_cast<HIMAGELIST>(lParam);
					if (type == LVSIL_SMALL && d->images[type])
					{
						t->setIconSize(QSize(d->images[type]->cx, d->images[type]->cy));
						for (int i = 0; i < t->topLevelItemCount(); ++i)
							lvUpdateIcon(w, t->topLevelItem(i));
					}
					return reinterpret_cast<LRESULT>(old);
				}
				case LVM_GETIMAGELIST:
					return (wParam <= 2) ? reinterpret_cast<LRESULT>(d->images[wParam]) : 0;
				case LVM_GETTOPINDEX:
				{
					QTreeWidgetItem* item = t->itemAt(0, 1);
					return item ? t->indexOfTopLevelItem(item) : 0;
				}
				case LVM_GETCOUNTPERPAGE:
				{
					const int rowH = t->topLevelItemCount() ? std::max(1, t->visualItemRect(t->topLevelItem(0)).height()) : QFontMetrics(t->font()).height() + 4;
					return std::max(1, t->viewport()->height() / rowH);
				}
				case LVM_HITTEST:
				case LVM_SUBITEMHITTEST:
				{
					auto* hit = reinterpret_cast<LVHITTESTINFO*>(lParam);
					if (!hit)
						return -1;
					const QPoint vp = t->viewport()->mapFrom(t, QPoint(hit->pt.x, hit->pt.y));
					QTreeWidgetItem* item = t->itemAt(vp);
					hit->iItem = item ? t->indexOfTopLevelItem(item) : -1;
					hit->iSubItem = item ? std::max(0, t->columnAt(vp.x())) : 0;
					hit->flags = item ? LVHT_ONITEMLABEL : LVHT_NOWHERE;
					if (item && (d->exStyle & LVS_EX_CHECKBOXES) && vp.x() - t->visualItemRect(item).left() < 20)
						hit->flags = LVHT_ONITEMSTATEICON;
					return hit->iItem;
				}
				case LVM_GETITEMRECT:
				case LVM_GETSUBITEMRECT:
				{
					auto* rc = reinterpret_cast<RECT*>(lParam);
					QTreeWidgetItem* item = lvItem(w, static_cast<int>(wParam));
					if (!rc || !item)
						return FALSE;
					QRect r = t->visualItemRect(item);
					if (msg == LVM_GETSUBITEMRECT && rc->top > 0)
					{
						const int c = rc->top;
						r.setLeft(t->header()->sectionViewportPosition(c));
						r.setWidth(t->columnWidth(c));
					}
					r.translate(t->viewport()->mapTo(t, QPoint(0, 0)));
					*rc = RECT{r.left(), r.top(), r.left() + r.width(), r.top() + r.height()};
					return TRUE;
				}
				case LVM_REDRAWITEMS:
				case LVM_UPDATE:
					t->viewport()->update();
					return TRUE;
				case LVM_SORTITEMS:
				case LVM_SORTITEMSEX:
				{
					auto cmp = reinterpret_cast<PFNLVCOMPARE>(lParam);
					if (!cmp)
						return FALSE;
					d->silent = true;
					QList<QTreeWidgetItem*> items;
					while (t->topLevelItemCount())
						items << t->takeTopLevelItem(0);
					std::vector<std::pair<QTreeWidgetItem*, int>> order;
					for (int i = 0; i < items.size(); ++i)
						order.push_back({items[i], i});
					std::stable_sort(order.begin(), order.end(), [&](const auto& a, const auto& b)
					{
						const LPARAM la = (msg == LVM_SORTITEMS) ? static_cast<LPARAM>(a.first->data(0, RoleParam).toLongLong()) : a.second;
						const LPARAM lb = (msg == LVM_SORTITEMS) ? static_cast<LPARAM>(b.first->data(0, RoleParam).toLongLong()) : b.second;
						return cmp(la, lb, static_cast<LPARAM>(wParam)) < 0;
					});
					for (const auto& o : order)
						t->addTopLevelItem(o.first);
					d->silent = false;
					d->lastSelection = lvSelection(t);
					return TRUE;
				}
				case LVM_FINDITEMW:
				{
					const auto* fi = reinterpret_cast<const LVFINDINFOW*>(lParam);
					if (!fi)
						return -1;
					const int n = t->topLevelItemCount();
					const int start = static_cast<int>(static_cast<intptr_t>(wParam)) + 1;
					for (int k = 0; k < n; ++k)
					{
						const int i = start + k;
						if (i >= n && !(fi->flags & LVFI_WRAP))
							break;
						QTreeWidgetItem* item = t->topLevelItem(i % n);
						if ((fi->flags & LVFI_PARAM) && static_cast<LPARAM>(item->data(0, RoleParam).toLongLong()) == fi->lParam)
							return i % n;
						if (fi->flags & (LVFI_STRING | LVFI_PARTIAL))
						{
							const QString s = qs(fi->psz);
							if ((fi->flags & LVFI_PARTIAL) ? item->text(0).startsWith(s, Qt::CaseInsensitive) : item->text(0).compare(s, Qt::CaseInsensitive) == 0)
								return i % n;
						}
					}
					return -1;
				}
				case LVM_GETSTRINGWIDTHW:
					return QFontMetrics(t->font()).horizontalAdvance(qs(reinterpret_cast<LPCWSTR>(lParam)));
				case LVM_GETHEADER:
				{
					if (!d->header)
					{
						d->header = newWindow(Kind::Header, t->header(), hwnd(w), 0, 0, 0, L"SysHeader32");
						d->header->ctl = t;
					}
					return reinterpret_cast<LRESULT>(hwnd(d->header));
				}
				case LVM_SETBKCOLOR:
				case LVM_SETTEXTBKCOLOR:
				{
					QPalette p = t->palette();
					if (static_cast<COLORREF>(lParam) != CLR_NONE)
						p.setColor(QPalette::Base, color(static_cast<COLORREF>(lParam)));
					t->setPalette(p);
					return TRUE;
				}
				case LVM_SETTEXTCOLOR:
				{
					QPalette p = t->palette();
					p.setColor(QPalette::Text, color(static_cast<COLORREF>(lParam)));
					t->setPalette(p);
					return TRUE;
				}
				case LVM_GETBKCOLOR:
				case LVM_GETTEXTBKCOLOR:
					return colorRef(t->palette().color(QPalette::Base));
				case LVM_GETTEXTCOLOR:
					return colorRef(t->palette().color(QPalette::Text));
				case LVM_EDITLABELW:
				{
					QTreeWidgetItem* item = lvItem(w, static_cast<int>(static_cast<intptr_t>(wParam)));
					if (item)
					{
						item->setFlags(item->flags() | Qt::ItemIsEditable);
						t->editItem(item, 0);
					}
					return 0;
				}
				case LVM_GETEDITCONTROL:
					return 0;
				case LVM_ISITEMVISIBLE:
				{
					QTreeWidgetItem* item = lvItem(w, static_cast<int>(wParam));
					return (item && t->viewport()->rect().intersects(t->visualItemRect(item))) ? TRUE : FALSE;
				}
				case LVM_SETCALLBACKMASK:
				case LVM_ARRANGE:
				case LVM_SETHOTITEM:
				case CCM_SETVERSION:
				case CCM_SETUNICODEFORMAT:
					return TRUE;
				default:
					return -0x7FFFFFFF;
			}
		}

		// ---- tree view --------------------------------------------------------------------
		QTreeWidgetItem* tvItem(HTREEITEM h)
		{
			return reinterpret_cast<QTreeWidgetItem*>(h);
		}

		HTREEITEM tvHandle(QTreeWidgetItem* item)
		{
			return reinterpret_cast<HTREEITEM>(item);
		}

		void tvFill(Window* w, QTreeWidgetItem* item, TVITEMW& tv)
		{
			QTreeWidget* t = tree(w);
			tv.hItem = tvHandle(item);
			if (!item)
				return;
			tv.lParam = static_cast<LPARAM>(item->data(0, RoleParam).toLongLong());
			tv.state = (item->isExpanded() ? TVIS_EXPANDED : 0) | (item->isSelected() || t->currentItem() == item ? TVIS_SELECTED : 0)
				| (item->font(0).bold() ? TVIS_BOLD : 0) | (item->data(0, RoleExpandedOnce).toBool() ? TVIS_EXPANDEDONCE : 0);
			if (viewData(w)->exStyle & TVS_CHECKBOXES || w->style & TVS_CHECKBOXES)
				tv.state |= INDEXTOSTATEIMAGEMASK(item->checkState(0) == Qt::Checked ? 2 : 1);
			tv.iImage = item->data(0, RoleImage).toInt();
			tv.iSelectedImage = item->data(0, RoleSelImage).toInt();
			tv.cChildren = item->childCount() ? 1 : item->data(0, RoleChildren).toInt();
		}

		void tvUpdateIcon(Window* w, QTreeWidgetItem* item)
		{
			HIMAGELIST il = viewData(w)->images[TVSIL_NORMAL];
			if (!il)
				return;
			const bool selected = tree(w)->currentItem() == item;
			const int image = selected ? item->data(0, RoleSelImage).toInt() : item->data(0, RoleImage).toInt();
			item->setIcon(0, imageAt(il, image));
		}

		void tvSet(Window* w, QTreeWidgetItem* item, const TVITEMW& tv)
		{
			if (tv.mask & TVIF_TEXT)
			{
				if (tv.pszText == LPSTR_TEXTCALLBACKW)
				{
					item->setData(0, RoleCallback, true);
					item->setText(0, QString());
				}
				else
				{
					item->setData(0, RoleCallback, false);
					item->setText(0, qs(tv.pszText));
				}
			}
			if (tv.mask & TVIF_PARAM)
				item->setData(0, RoleParam, static_cast<qlonglong>(tv.lParam));
			if (tv.mask & TVIF_IMAGE)
				item->setData(0, RoleImage, tv.iImage);
			if (tv.mask & TVIF_SELECTEDIMAGE)
				item->setData(0, RoleSelImage, tv.iSelectedImage);
			if (!(tv.mask & TVIF_SELECTEDIMAGE) && (tv.mask & TVIF_IMAGE) && !item->data(0, RoleSelImage).isValid())
				item->setData(0, RoleSelImage, tv.iImage);
			if (tv.mask & TVIF_CHILDREN)
			{
				item->setData(0, RoleChildren, tv.cChildren);
				item->setChildIndicatorPolicy(tv.cChildren ? QTreeWidgetItem::ShowIndicator : QTreeWidgetItem::DontShowIndicatorWhenChildless);
			}
			if (tv.mask & TVIF_STATE)
			{
				if (tv.stateMask & TVIS_BOLD)
				{
					QFont f = item->font(0);
					f.setBold(tv.state & TVIS_BOLD);
					item->setFont(0, f);
				}
				if (tv.stateMask & TVIS_EXPANDED)
					item->setExpanded(tv.state & TVIS_EXPANDED);
				if (tv.stateMask & TVIS_SELECTED)
					item->setSelected(tv.state & TVIS_SELECTED);
				if (tv.stateMask & TVIS_STATEIMAGEMASK)
				{
					const UINT idx = (tv.state & TVIS_STATEIMAGEMASK) >> 12;
					if (idx)
						item->setCheckState(0, idx == 2 ? Qt::Checked : Qt::Unchecked);
					else
						item->setData(0, Qt::CheckStateRole, QVariant());
				}
				if (tv.stateMask & TVIS_DROPHILITED)
					item->setBackground(0, (tv.state & TVIS_DROPHILITED) ? QApplication::palette().highlight() : QBrush());
			}
			if (tv.mask & (TVIF_IMAGE | TVIF_SELECTEDIMAGE))
				tvUpdateIcon(w, item);
		}

		void tvNotifyDelete(Window* w, QTreeWidgetItem* item)
		{
			for (int i = 0; i < item->childCount(); ++i)
				tvNotifyDelete(w, item->child(i));
			NMTREEVIEWW nm{};
			nm.action = 0;
			nm.itemOld.mask = TVIF_HANDLE | TVIF_PARAM;
			nm.itemOld.hItem = tvHandle(item);
			nm.itemOld.lParam = static_cast<LPARAM>(item->data(0, RoleParam).toLongLong());
			notify(w, &nm.hdr, TVN_DELETEITEMW);
		}

		bool tvExpanding(Window* w, QTreeWidgetItem* item, UINT action)
		{
			// TVN_ITEMEXPANDING: TRUE stops it
			NMTREEVIEWW nm{};
			nm.action = action;
			tvFill(w, item, nm.itemNew);
			nm.itemNew.mask = TVIF_HANDLE | TVIF_PARAM | TVIF_STATE;
			return notify(w, &nm.hdr, TVN_ITEMEXPANDINGW) != 0;
		}

		void tvExpanded(Window* w, QTreeWidgetItem* item, UINT action)
		{
			NMTREEVIEWW nm{};
			nm.action = action;
			tvFill(w, item, nm.itemNew);
			nm.itemNew.mask = TVIF_HANDLE | TVIF_PARAM | TVIF_STATE;
			notify(w, &nm.hdr, TVN_ITEMEXPANDEDW);
		}

		bool tvAll(QTreeWidget* t, QTreeWidgetItem* item)
		{
			// is item still in the tree? (handles of deleted items)
			if (!item)
				return false;
			QTreeWidgetItem* top = item;
			while (top->parent())
				top = top->parent();
			return t->indexOfTopLevelItem(top) >= 0;
		}

		LRESULT treeViewProc(Window* w, UINT msg, WPARAM wParam, LPARAM lParam)
		{
			QTreeWidget* t = tree(w);
			ViewData* d = viewData(w);
			if (!t || !d)
				return -0x7FFFFFFF;
			switch (msg)
			{
				case TVM_INSERTITEMW:
				{
					auto* ins = reinterpret_cast<const TVINSERTSTRUCTW*>(lParam);
					if (!ins)
						return 0;
					QTreeWidgetItem* parent = (ins->hParent == TVI_ROOT || !ins->hParent) ? nullptr : tvItem(ins->hParent);
					auto* item = new QTreeWidgetItem;
					if (w->style & TVS_CHECKBOXES)
						item->setCheckState(0, Qt::Unchecked);
					TVITEMW tv = ins->item;
					tvSet(w, item, tv);
					const int count = parent ? parent->childCount() : t->topLevelItemCount();
					int at = count;
					if (ins->hInsertAfter == TVI_FIRST)
						at = 0;
					else if (ins->hInsertAfter == TVI_SORT)
					{
						at = 0;
						while (at < count && QString::compare(parent ? parent->child(at)->text(0) : t->topLevelItem(at)->text(0), item->text(0), Qt::CaseInsensitive) <= 0)
							++at;
					}
					else if (ins->hInsertAfter && ins->hInsertAfter != TVI_LAST)
					{
						QTreeWidgetItem* after = tvItem(ins->hInsertAfter);
						const int idx = parent ? parent->indexOfChild(after) : t->indexOfTopLevelItem(after);
						if (idx >= 0)
							at = idx + 1;
					}
					d->silent = true;
					if (parent)
						parent->insertChild(at, item);
					else
						t->insertTopLevelItem(at, item);
					d->silent = false;
					tvUpdateIcon(w, item);
					return reinterpret_cast<LRESULT>(tvHandle(item));
				}
				case TVM_DELETEITEM:
				{
					d->silent = true;
					if (reinterpret_cast<HTREEITEM>(lParam) == TVI_ROOT || !lParam)
					{
						for (int i = 0; i < t->topLevelItemCount(); ++i)
							tvNotifyDelete(w, t->topLevelItem(i));
						t->clear();
					}
					else if (QTreeWidgetItem* item = tvItem(reinterpret_cast<HTREEITEM>(lParam)); tvAll(t, item))
					{
						tvNotifyDelete(w, item);
						if (d->dropHilite == item)
							d->dropHilite = nullptr;
						delete item;
					}
					d->silent = false;
					return TRUE;
				}
				case TVM_EXPAND:
				{
					QTreeWidgetItem* item = tvItem(reinterpret_cast<HTREEITEM>(lParam));
					if (!tvAll(t, item))
						return FALSE;
					UINT code = static_cast<UINT>(wParam) & 3;
					if (code == TVE_TOGGLE)
						code = item->isExpanded() ? TVE_COLLAPSE : TVE_EXPAND;
					const bool once = item->data(0, RoleExpandedOnce).toBool();
					if (code == TVE_EXPAND && !item->isExpanded())
					{
						// first expansion: the parent may fill the children (TVN_ITEMEXPANDING)
						if (!once && tvExpanding(w, item, TVE_EXPAND))
							return FALSE;
						d->silent = true;
						item->setData(0, RoleExpandedOnce, true);
						item->setExpanded(true);
						d->silent = false;
						if (!once)
							tvExpanded(w, item, TVE_EXPAND);
					}
					else if (code == TVE_COLLAPSE)
					{
						d->silent = true;
						item->setExpanded(false);
						if (wParam & TVE_COLLAPSERESET)
						{
							for (QTreeWidgetItem* c : item->takeChildren())
							{
								tvNotifyDelete(w, c);
								delete c;
							}
							item->setData(0, RoleExpandedOnce, false);
						}
						d->silent = false;
					}
					return TRUE;
				}
				case TVM_GETCOUNT:
				{
					int n = 0;
					std::function<void(QTreeWidgetItem*)> count = [&](QTreeWidgetItem* i)
					{
						++n;
						for (int k = 0; k < i->childCount(); ++k)
							count(i->child(k));
					};
					for (int k = 0; k < t->topLevelItemCount(); ++k)
						count(t->topLevelItem(k));
					return n;
				}
				case TVM_GETNEXTITEM:
				{
					QTreeWidgetItem* item = tvItem(reinterpret_cast<HTREEITEM>(lParam));
					if (item && reinterpret_cast<HTREEITEM>(lParam) == TVI_ROOT)
						item = nullptr;
					switch (wParam & 0xFF)
					{
						case TVGN_ROOT:
							return reinterpret_cast<LRESULT>(tvHandle(t->topLevelItem(0)));
						case TVGN_CHILD:
							if (!item)
								return reinterpret_cast<LRESULT>(tvHandle(t->topLevelItem(0)));
							return tvAll(t, item) ? reinterpret_cast<LRESULT>(tvHandle(item->child(0))) : 0;
						case TVGN_NEXT:
						case TVGN_PREVIOUS:
						{
							if (!tvAll(t, item))
								return 0;
							QTreeWidgetItem* p = item->parent();
							const int idx = p ? p->indexOfChild(item) : t->indexOfTopLevelItem(item);
							const int next = idx + ((wParam & 0xFF) == TVGN_NEXT ? 1 : -1);
							const int count = p ? p->childCount() : t->topLevelItemCount();
							if (next < 0 || next >= count)
								return 0;
							return reinterpret_cast<LRESULT>(tvHandle(p ? p->child(next) : t->topLevelItem(next)));
						}
						case TVGN_PARENT:
							return tvAll(t, item) ? reinterpret_cast<LRESULT>(tvHandle(item->parent())) : 0;
						case TVGN_CARET:
							return reinterpret_cast<LRESULT>(tvHandle(t->currentItem()));
						case TVGN_DROPHILITE:
							return reinterpret_cast<LRESULT>(tvHandle(d->dropHilite));
						case TVGN_FIRSTVISIBLE:
							return reinterpret_cast<LRESULT>(tvHandle(t->itemAt(1, 1)));
						case TVGN_NEXTVISIBLE:
							return tvAll(t, item) ? reinterpret_cast<LRESULT>(tvHandle(t->itemBelow(item))) : 0;
						case TVGN_PREVIOUSVISIBLE:
							return tvAll(t, item) ? reinterpret_cast<LRESULT>(tvHandle(t->itemAbove(item))) : 0;
						case TVGN_LASTVISIBLE:
						{
							QTreeWidgetItem* last = t->topLevelItem(t->topLevelItemCount() - 1);
							while (last && last->isExpanded() && last->childCount())
								last = last->child(last->childCount() - 1);
							return reinterpret_cast<LRESULT>(tvHandle(last));
						}
						default:
							return 0;
					}
				}
				case TVM_SELECTITEM:
				{
					QTreeWidgetItem* item = tvItem(reinterpret_cast<HTREEITEM>(lParam));
					if (item && !tvAll(t, item))
						return FALSE;
					switch (wParam & 0xFF)
					{
						case TVGN_CARET:
							if (item)
								t->setCurrentItem(item);
							else
							{
								t->setCurrentItem(nullptr);
								t->clearSelection();
							}
							return TRUE;
						case TVGN_DROPHILITE:
						{
							if (d->dropHilite && tvAll(t, d->dropHilite))
								d->dropHilite->setBackground(0, QBrush());
							d->dropHilite = item;
							if (item)
								item->setBackground(0, QApplication::palette().highlight());
							return TRUE;
						}
						case TVGN_FIRSTVISIBLE:
							if (item)
								t->scrollToItem(item, QAbstractItemView::PositionAtTop);
							return TRUE;
						default:
							return FALSE;
					}
				}
				case TVM_GETITEMW:
				case TVM_SETITEMW:
				{
					auto* tv = reinterpret_cast<TVITEMW*>(lParam);
					QTreeWidgetItem* item = tv ? tvItem(tv->hItem) : nullptr;
					if (!tvAll(t, item))
						return FALSE;
					if (msg == TVM_SETITEMW)
					{
						d->silent = true;
						tvSet(w, item, *tv);
						d->silent = false;
						return TRUE;
					}
					const UINT mask = tv->mask;
					LPWSTR text = tv->pszText;
					const int max = tv->cchTextMax;
					TVITEMW filled = *tv;
					tvFill(w, item, filled);
					if (mask & TVIF_PARAM)
						tv->lParam = filled.lParam;
					if (mask & TVIF_STATE)
						tv->state = filled.state & (tv->stateMask ? tv->stateMask : 0xFFFFFFFF);
					if (mask & TVIF_IMAGE)
						tv->iImage = filled.iImage;
					if (mask & TVIF_SELECTEDIMAGE)
						tv->iSelectedImage = filled.iSelectedImage;
					if (mask & TVIF_CHILDREN)
						tv->cChildren = filled.cChildren;
					if ((mask & TVIF_TEXT) && text)
						copyOut(item->text(0), text, max);
					return TRUE;
				}
				case TVM_GETITEMSTATE:
				{
					QTreeWidgetItem* item = tvItem(reinterpret_cast<HTREEITEM>(wParam));
					if (!tvAll(t, item))
						return 0;
					TVITEMW tv{};
					tvFill(w, item, tv);
					return tv.state & static_cast<UINT>(lParam);
				}
				case TVM_ENSUREVISIBLE:
					if (QTreeWidgetItem* item = tvItem(reinterpret_cast<HTREEITEM>(lParam)); tvAll(t, item))
					{
						for (QTreeWidgetItem* p = item->parent(); p; p = p->parent())
							p->setExpanded(true);
						t->scrollToItem(item);
					}
					return TRUE;
				case TVM_HITTEST:
				{
					auto* hit = reinterpret_cast<TVHITTESTINFO*>(lParam);
					if (!hit)
						return 0;
					const QPoint vp = t->viewport()->mapFrom(t, QPoint(hit->pt.x, hit->pt.y));
					QTreeWidgetItem* item = t->itemAt(vp);
					hit->hItem = tvHandle(item);
					if (!item)
						hit->flags = TVHT_NOWHERE;
					else
					{
						const QRect r = t->visualItemRect(item);
						if (vp.x() < r.left())
							hit->flags = (vp.x() >= r.left() - t->indentation()) ? TVHT_ONITEMBUTTON : TVHT_ONITEMINDENT;
						else if ((w->style & TVS_CHECKBOXES) && vp.x() < r.left() + 20)
							hit->flags = TVHT_ONITEMSTATEICON;
						else
							hit->flags = TVHT_ONITEMLABEL;
					}
					return reinterpret_cast<LRESULT>(hit->hItem);
				}
				case TVM_GETITEMRECT:
				{
					auto* rc = reinterpret_cast<RECT*>(lParam);
					if (!rc)
						return FALSE;
					QTreeWidgetItem* item = tvItem(*reinterpret_cast<HTREEITEM*>(rc));
					if (!tvAll(t, item))
						return FALSE;
					QRect r = t->visualItemRect(item);
					if (!wParam)
						r.setLeft(0);
					r.translate(t->viewport()->mapTo(t, QPoint(0, 0)));
					*rc = RECT{r.left(), r.top(), r.left() + r.width(), r.top() + r.height()};
					return r.isValid() ? TRUE : FALSE;
				}
				case TVM_SORTCHILDREN:
				{
					QTreeWidgetItem* item = tvItem(reinterpret_cast<HTREEITEM>(lParam));
					d->silent = true;
					if (!item || reinterpret_cast<HTREEITEM>(lParam) == TVI_ROOT)
						t->sortItems(0, Qt::AscendingOrder);
					else if (tvAll(t, item))
						item->sortChildren(0, Qt::AscendingOrder);
					t->setSortingEnabled(false);
					d->silent = false;
					return TRUE;
				}
				case TVM_SORTCHILDRENCB:
				{
					auto* scb = reinterpret_cast<const TVSORTCB*>(lParam);
					if (!scb || !scb->lpfnCompare)
						return FALSE;
					QTreeWidgetItem* parent = (scb->hParent == TVI_ROOT || !scb->hParent) ? nullptr : tvItem(scb->hParent);
					d->silent = true;
					QList<QTreeWidgetItem*> items = parent ? parent->takeChildren() : QList<QTreeWidgetItem*>();
					if (!parent)
					{
						while (t->topLevelItemCount())
							items << t->takeTopLevelItem(0);
					}
					std::vector<QTreeWidgetItem*> v(items.begin(), items.end());
					std::stable_sort(v.begin(), v.end(), [&](QTreeWidgetItem* a, QTreeWidgetItem* b)
					{
						return scb->lpfnCompare(static_cast<LPARAM>(a->data(0, RoleParam).toLongLong()), static_cast<LPARAM>(b->data(0, RoleParam).toLongLong()), scb->lParam) < 0;
					});
					for (QTreeWidgetItem* x : v)
					{
						const bool expanded = x->isExpanded();
						if (parent)
							parent->addChild(x);
						else
							t->addTopLevelItem(x);
						x->setExpanded(expanded);
					}
					d->silent = false;
					return TRUE;
				}
				case TVM_SETIMAGELIST:
				{
					const int type = static_cast<int>(wParam);
					if (type < 0 || type > 2)
						return 0;
					HIMAGELIST old = d->images[type];
					d->images[type] = reinterpret_cast<HIMAGELIST>(lParam);
					if (type == TVSIL_NORMAL && d->images[type])
						t->setIconSize(QSize(d->images[type]->cx, d->images[type]->cy));
					std::function<void(QTreeWidgetItem*)> all = [&](QTreeWidgetItem* i)
					{
						tvUpdateIcon(w, i);
						for (int k = 0; k < i->childCount(); ++k)
							all(i->child(k));
					};
					for (int k = 0; k < t->topLevelItemCount(); ++k)
						all(t->topLevelItem(k));
					return reinterpret_cast<LRESULT>(old);
				}
				case TVM_GETIMAGELIST:
					return (wParam <= 2) ? reinterpret_cast<LRESULT>(d->images[wParam]) : 0;
				case TVM_EDITLABELW:
				{
					QTreeWidgetItem* item = tvItem(reinterpret_cast<HTREEITEM>(lParam));
					if (!tvAll(t, item))
						return 0;
					NMTVDISPINFOW di{};
					tvFill(w, item, di.item);
					if (notify(w, &di.hdr, TVN_BEGINLABELEDITW))
						return 0;
					item->setFlags(item->flags() | Qt::ItemIsEditable);
					t->setProperty("_w32EditOld", item->text(0));
					t->editItem(item, 0);
					return 0;
				}
				case TVM_ENDEDITLABELNOW:
					if (QTreeWidgetItem* item = t->currentItem())
						t->closePersistentEditor(item, 0);
					return TRUE;
				case TVM_GETEDITCONTROL:
					return 0;
				case TVM_GETVISIBLECOUNT:
				{
					const int rowH = t->topLevelItemCount() ? std::max(1, t->visualItemRect(t->topLevelItem(0)).height()) : QFontMetrics(t->font()).height() + 4;
					return std::max(1, t->viewport()->height() / rowH);
				}
				case TVM_SETITEMHEIGHT:
				case TVM_GETITEMHEIGHT:
					return t->topLevelItemCount() ? t->visualItemRect(t->topLevelItem(0)).height() : QFontMetrics(t->font()).height() + 4;
				case TVM_GETINDENT:
					return t->indentation();
				case TVM_SETINDENT:
					t->setIndentation(static_cast<int>(wParam));
					return 0;
				case TVM_SETBKCOLOR:
				{
					QPalette p = t->palette();
					const COLORREF old = colorRef(p.color(QPalette::Base));
					if (static_cast<intptr_t>(lParam) != -1)
						p.setColor(QPalette::Base, color(static_cast<COLORREF>(lParam)));
					t->setPalette(p);
					return old;
				}
				case TVM_SETTEXTCOLOR:
				{
					QPalette p = t->palette();
					const COLORREF old = colorRef(p.color(QPalette::Text));
					if (static_cast<intptr_t>(lParam) != -1)
						p.setColor(QPalette::Text, color(static_cast<COLORREF>(lParam)));
					t->setPalette(p);
					return old;
				}
				case TVM_GETBKCOLOR:
					return colorRef(t->palette().color(QPalette::Base));
				case TVM_GETTEXTCOLOR:
					return colorRef(t->palette().color(QPalette::Text));
				case TVM_SETLINECOLOR:
				case TVM_SETEXTENDEDSTYLE:
				case TVM_GETEXTENDEDSTYLE:
				case CCM_SETVERSION:
				case CCM_SETUNICODEFORMAT:
					return 0;
				default:
					return -0x7FFFFFFF;
			}
		}

		void tvSelChanged(Window* w, QTreeWidgetItem* now, QTreeWidgetItem* before)
		{
			ViewData* d = viewData(w);
			if (before)
				tvUpdateIcon(w, before);
			if (now)
				tvUpdateIcon(w, now);
			NMTREEVIEWW nm{};
			nm.action = (QApplication::mouseButtons() != Qt::NoButton) ? TVC_BYMOUSE : TVC_UNKNOWN;
			tvFill(w, before, nm.itemOld);
			nm.itemOld.mask = TVIF_HANDLE | TVIF_PARAM | TVIF_STATE;
			tvFill(w, now, nm.itemNew);
			nm.itemNew.mask = TVIF_HANDLE | TVIF_PARAM | TVIF_STATE;
			Q_UNUSED(d);
			notify(w, &nm.hdr, TVN_SELCHANGINGW);
			notify(w, &nm.hdr, TVN_SELCHANGEDW);
		}

		// keys and clicks of list and tree views
		class ViewFilter : public QObject
		{
		public:
			explicit ViewFilter(Window* w, QObject* parent) : QObject(parent), _w(w) {}

			bool eventFilter(QObject* obj, QEvent* e) override
			{
				QTreeWidget* t = tree(_w);
				if (!t || _w->destroyed)
					return false;
				if (e->type() == QEvent::KeyPress && obj == t)
				{
					auto* ke = static_cast<QKeyEvent*>(e);
					const int vk = vkFromQt(ke->key(), ke->nativeVirtualKey());
					if (_w->kind == Kind::ListView)
					{
						NMLVKEYDOWN nm{};
						nm.wVKey = static_cast<WORD>(vk);
						notify(_w, &nm.hdr, LVN_KEYDOWN);
					}
					else
					{
						NMTVKEYDOWN nm{};
						nm.wVKey = static_cast<WORD>(vk);
						if (notify(_w, &nm.hdr, TVN_KEYDOWN))
							return true;
					}
					if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter)
					{
						NMHDR hdr{};
						if (notify(_w, &hdr, NM_RETURN))
							return true;
						if (_w->kind == Kind::ListView)
							lvItemActivate(_w, t->currentItem(), 0, LVN_ITEMACTIVATE);
					}
					return false;
				}
				if (obj == t->viewport())
				{
					if (e->type() == QEvent::MouseButtonPress)
					{
						auto* me = static_cast<QMouseEvent*>(e);
						QTreeWidgetItem* item = t->itemAt(me->position().toPoint());
						if (me->button() == Qt::RightButton)
						{
							// NM_RCLICK: the parent shows its menu (TVM_HITTEST of the cursor)
							_rightItem = item;
							return false;
						}
						if (me->button() == Qt::LeftButton && !item && _w->kind == Kind::ListView)
							lvItemActivate(_w, nullptr, 0, NM_CLICK);
					}
					else if (e->type() == QEvent::MouseButtonRelease)
					{
						auto* me = static_cast<QMouseEvent*>(e);
						QTreeWidgetItem* item = t->itemAt(me->position().toPoint());
						const int column = t->columnAt(me->position().toPoint().x());
						if (me->button() == Qt::LeftButton && item)
						{
							if (_w->kind == Kind::ListView)
								lvItemActivate(_w, item, std::max(column, 0), NM_CLICK);
							else
							{
								NMHDR hdr{};
								notify(_w, &hdr, NM_CLICK);
							}
						}
						else if (me->button() == Qt::RightButton)
						{
							LRESULT handled = 0;
							if (_w->kind == Kind::ListView)
							{
								NMITEMACTIVATE nm{};
								nm.iItem = item ? t->indexOfTopLevelItem(item) : -1;
								nm.iSubItem = std::max(column, 0);
								const QPoint p = t->viewport()->mapTo(t, me->position().toPoint());
								nm.ptAction = POINT{p.x(), p.y()};
								handled = notify(_w, &nm.hdr, NM_RCLICK);
							}
							else
							{
								NMHDR hdr{};
								handled = notify(_w, &hdr, NM_RCLICK);
							}
							if (!handled && _w->parent)
							{
								const QPoint g = me->globalPosition().toPoint();
								SendMessageW(_w->parent, WM_CONTEXTMENU, reinterpret_cast<WPARAM>(hwnd(_w)),
									MAKELPARAM(static_cast<short>(g.x()), static_cast<short>(g.y())));
							}
							return true;
						}
					}
					else if (e->type() == QEvent::MouseButtonDblClick)
					{
						auto* me = static_cast<QMouseEvent*>(e);
						QTreeWidgetItem* item = t->itemAt(me->position().toPoint());
						if (me->button() == Qt::LeftButton)
						{
							if (_w->kind == Kind::ListView)
							{
								lvItemActivate(_w, item, std::max(t->columnAt(me->position().toPoint().x()), 0), NM_DBLCLK);
								if (item)
									lvItemActivate(_w, item, 0, LVN_ITEMACTIVATE);
								return false;
							}
							NMHDR hdr{};
							if (notify(_w, &hdr, NM_DBLCLK))
								return true;
						}
					}
				}
				return false;
			}

		private:
			Window* _w;
			QTreeWidgetItem* _rightItem = nullptr;
		};

		// ---- tab control -----------------------------------------------------------------
		struct TabData
		{
			QTabBar* bar = nullptr;
			HIMAGELIST images = nullptr;
			bool silent = false;
		};

		LRESULT tabProc(Window* w, UINT msg, WPARAM wParam, LPARAM lParam)
		{
			auto* d = static_cast<TabData*>(w->ctl);
			QTabBar* bar = d ? d->bar : nullptr;
			if (!bar)
				return -0x7FFFFFFF;
			switch (msg)
			{
				case TCM_GETITEMCOUNT:
					return bar->count();
				case TCM_INSERTITEMW:
				{
					const auto* ti = reinterpret_cast<const TCITEMW*>(lParam);
					if (!ti)
						return -1;
					d->silent = true;
					const int at = bar->insertTab(std::clamp(static_cast<int>(wParam), 0, bar->count()), (ti->mask & TCIF_TEXT) ? qs(ti->pszText) : QString());
					if (ti->mask & TCIF_PARAM)
						bar->setTabData(at, static_cast<qlonglong>(ti->lParam));
					if ((ti->mask & TCIF_IMAGE) && d->images)
						bar->setTabIcon(at, imageAt(d->images, ti->iImage));
					d->silent = false;
					return at;
				}
				case TCM_SETITEMW:
				case TCM_GETITEMW:
				{
					auto* ti = reinterpret_cast<TCITEMW*>(lParam);
					const int i = static_cast<int>(wParam);
					if (!ti || i < 0 || i >= bar->count())
						return FALSE;
					if (msg == TCM_SETITEMW)
					{
						if (ti->mask & TCIF_TEXT)
							bar->setTabText(i, qs(ti->pszText));
						if (ti->mask & TCIF_PARAM)
							bar->setTabData(i, static_cast<qlonglong>(ti->lParam));
						if ((ti->mask & TCIF_IMAGE) && d->images)
							bar->setTabIcon(i, imageAt(d->images, ti->iImage));
					}
					else
					{
						if (ti->mask & TCIF_TEXT)
							copyOut(bar->tabText(i), ti->pszText, ti->cchTextMax);
						if (ti->mask & TCIF_PARAM)
							ti->lParam = static_cast<LPARAM>(bar->tabData(i).toLongLong());
					}
					return TRUE;
				}
				case TCM_DELETEITEM:
					d->silent = true;
					bar->removeTab(static_cast<int>(wParam));
					d->silent = false;
					return TRUE;
				case TCM_DELETEALLITEMS:
					d->silent = true;
					while (bar->count())
						bar->removeTab(0);
					d->silent = false;
					return TRUE;
				case TCM_GETCURSEL:
				case TCM_GETCURFOCUS:
					return bar->currentIndex();
				case TCM_SETCURSEL:
				case TCM_SETCURFOCUS:
				{
					const int old = bar->currentIndex();
					d->silent = true;
					bar->setCurrentIndex(static_cast<int>(wParam));
					d->silent = false;
					return old;
				}
				case TCM_GETITEMRECT:
				{
					auto* rc = reinterpret_cast<RECT*>(lParam);
					if (!rc)
						return FALSE;
					const QRect r = bar->tabRect(static_cast<int>(wParam)).translated(bar->pos());
					*rc = RECT{r.left(), r.top(), r.left() + r.width(), r.top() + r.height()};
					return TRUE;
				}
				case TCM_ADJUSTRECT:
				{
					auto* rc = reinterpret_cast<RECT*>(lParam);
					if (!rc)
						return 0;
					const int h = bar->sizeHint().height();
					if (wParam)
					{
						rc->top -= h + 2;
						rc->left -= 2;
						rc->right += 2;
						rc->bottom += 2;
					}
					else
					{
						rc->top += h + 2;
						rc->left += 2;
						rc->right -= 2;
						rc->bottom -= 2;
					}
					return 0;
				}
				case TCM_HITTEST:
				{
					auto* hit = reinterpret_cast<TCHITTESTINFO*>(lParam);
					return hit ? bar->tabAt(bar->mapFrom(w->widget, QPoint(hit->pt.x, hit->pt.y))) : -1;
				}
				case TCM_SETIMAGELIST:
				{
					HIMAGELIST old = d->images;
					d->images = reinterpret_cast<HIMAGELIST>(lParam);
					return reinterpret_cast<LRESULT>(old);
				}
				case TCM_GETIMAGELIST:
					return reinterpret_cast<LRESULT>(d->images);
				case TCM_GETROWCOUNT:
					return 1;
				case TCM_SETITEMSIZE:
				case TCM_SETPADDING:
					return 0;
				default:
					return -0x7FFFFFFF;
			}
		}

		// ---- up-down ---------------------------------------------------------------------
		struct UpDownData
		{
			int min = 0;
			int max = 100;
			int pos = 0;
			int base = 10;
		};

		void upDownApply(Window* w, int delta)
		{
			auto* d = static_cast<UpDownData*>(w->ctl);
			if (!d)
				return;
			NMUPDOWN nm{};
			nm.iPos = d->pos;
			nm.iDelta = delta;
			if (notify(w, &nm.hdr, UDN_DELTAPOS))
				return;
			const int lo = std::min(d->min, d->max);
			const int hi = std::max(d->min, d->max);
			int next = d->pos + ((d->min > d->max) ? -nm.iDelta : nm.iDelta);
			if (next > hi)
				next = (w->style & UDS_WRAP) ? lo : hi;
			if (next < lo)
				next = (w->style & UDS_WRAP) ? hi : lo;
			d->pos = next;
			if (w->buddy && (w->style & UDS_SETBUDDYINT))
			{
				const std::wstring t = ws(QString::number(d->pos, d->base));
				SetWindowTextW(w->buddy, t.c_str());
			}
			SendMessageW(w->parent, (w->style & UDS_HORZ) ? WM_HSCROLL : WM_VSCROLL, MAKEWPARAM(SB_THUMBPOSITION, d->pos), reinterpret_cast<LPARAM>(hwnd(w)));
			SendMessageW(w->parent, (w->style & UDS_HORZ) ? WM_HSCROLL : WM_VSCROLL, MAKEWPARAM(SB_ENDSCROLL, d->pos), reinterpret_cast<LPARAM>(hwnd(w)));
		}

		LRESULT upDownProc(Window* w, UINT msg, WPARAM wParam, LPARAM lParam)
		{
			auto* d = static_cast<UpDownData*>(w->ctl);
			if (!d)
				return -0x7FFFFFFF;
			auto readBuddy = [&]
			{
				if (w->buddy && (w->style & UDS_SETBUDDYINT))
				{
					wchar_t buf[64] = {};
					GetWindowTextW(w->buddy, buf, 64);
					bool ok = false;
					const int v = qs(buf).trimmed().remove(QLatin1Char(',')).toInt(&ok, d->base);
					if (ok)
						d->pos = v;
				}
			};
			switch (msg)
			{
				case UDM_SETRANGE:
					d->min = static_cast<short>(HIWORD(lParam));
					d->max = static_cast<short>(LOWORD(lParam));
					return 0;
				case UDM_SETRANGE32:
					d->min = static_cast<int>(wParam);
					d->max = static_cast<int>(lParam);
					return 0;
				case UDM_GETRANGE:
					return MAKELONG(static_cast<short>(d->max), static_cast<short>(d->min));
				case UDM_GETRANGE32:
					if (wParam)
						*reinterpret_cast<int*>(wParam) = d->min;
					if (lParam)
						*reinterpret_cast<int*>(lParam) = d->max;
					return 0;
				case UDM_SETPOS:
				case UDM_SETPOS32:
				{
					const int old = d->pos;
					d->pos = (msg == UDM_SETPOS) ? static_cast<short>(LOWORD(lParam)) : static_cast<int>(lParam);
					if (w->buddy && (w->style & UDS_SETBUDDYINT))
					{
						const std::wstring t = ws(QString::number(d->pos, d->base));
						SetWindowTextW(w->buddy, t.c_str());
					}
					return old;
				}
				case UDM_GETPOS:
					readBuddy();
					return MAKELONG(static_cast<short>(d->pos), 0);
				case UDM_GETPOS32:
					readBuddy();
					if (lParam)
						*reinterpret_cast<BOOL*>(lParam) = FALSE;
					return d->pos;
				case UDM_SETBUDDY:
				{
					HWND old = w->buddy;
					w->buddy = reinterpret_cast<HWND>(wParam);
					return reinterpret_cast<LRESULT>(old);
				}
				case UDM_GETBUDDY:
					return reinterpret_cast<LRESULT>(w->buddy);
				case UDM_SETBASE:
				{
					const int old = d->base;
					d->base = (wParam == 16) ? 16 : 10;
					return old;
				}
				case UDM_SETACCEL:
					return TRUE;
				default:
					return -0x7FFFFFFF;
			}
		}

		// ---- tooltips ----------------------------------------------------------------------
		struct Tool
		{
			HWND owner = nullptr;
			UINT_PTR id = 0;
			UINT flags = 0;
			RECT rect{};
			QString text;
			bool callback = false;
			LPARAM lParam = 0;
		};

		struct TooltipData
		{
			std::vector<Tool> tools;
			bool active = true;
			QString title;
			POINT trackPos{};
			bool tracking = false;
		};

		QString toolText(Window* tip, const Tool& tool)
		{
			if (!tool.callback)
				return tool.text;
			NMTTDISPINFOW di{};
			di.lParam = tool.lParam;
			di.uFlags = tool.flags;
			NMHDR& hdr = di.hdr;
			hdr.hwndFrom = hwnd(tip);
			hdr.idFrom = tool.id;
			hdr.code = TTN_GETDISPINFOW;
			HWND target = tool.owner;
			SendMessageW(target, WM_NOTIFY, tool.id, reinterpret_cast<LPARAM>(&di));
			if (di.lpszText && di.lpszText != LPSTR_TEXTCALLBACKW)
			{
				if (isIntResource(di.lpszText))
				{
					wchar_t buf[512] = {};
					LoadStringW(nullptr, static_cast<UINT>(intResource(di.lpszText)), buf, 512);
					return qs(buf);
				}
				return qs(di.lpszText);
			}
			return qs(di.szText);
		}

		class TooltipFilter : public QObject
		{
		public:
			explicit TooltipFilter(Window* tip, QObject* parent) : QObject(parent), _tip(tip) {}

			bool eventFilter(QObject* obj, QEvent* e) override
			{
				if (e->type() != QEvent::ToolTip || _tip->destroyed)
					return false;
				auto* d = static_cast<TooltipData*>(_tip->ctl);
				if (!d || !d->active)
					return false;
				auto* widget = qobject_cast<QWidget*>(obj);
				auto* he = static_cast<QHelpEvent*>(e);
				for (const Tool& t : d->tools)
				{
					QWidget* toolWidget = (t.flags & TTF_IDISHWND) ? widgetOf(reinterpret_cast<HWND>(t.id)) : widgetOf(t.owner);
					if (toolWidget != widget)
						continue;
					if (!(t.flags & TTF_IDISHWND) && !(t.rect.right == 0 && t.rect.bottom == 0))
					{
						const QRect r(t.rect.left, t.rect.top, t.rect.right - t.rect.left, t.rect.bottom - t.rect.top);
						if (!r.contains(he->pos()))
							continue;
					}
					QString text = toolText(_tip, t);
					if (text.isEmpty())
						return true;
					if (!d->title.isEmpty())
						text = QStringLiteral("<b>%1</b><br>%2").arg(d->title.toHtmlEscaped(), text.toHtmlEscaped().replace(QLatin1Char('\n'), QStringLiteral("<br>")));
					QToolTip::showText(he->globalPos(), text, widget);
					return true;
				}
				return false;
			}

		private:
			Window* _tip;
		};

		LRESULT tooltipProc(Window* w, UINT msg, WPARAM wParam, LPARAM lParam)
		{
			auto* d = static_cast<TooltipData*>(w->ctl);
			if (!d)
				return -0x7FFFFFFF;
			static TooltipFilter* filterProto = nullptr;
			Q_UNUSED(filterProto);
			switch (msg)
			{
				case TTM_ADDTOOLW:
				case TTM_SETTOOLINFOW:
				case TTM_UPDATETIPTEXTW:
				case TTM_NEWTOOLRECTW:
				{
					const auto* ti = reinterpret_cast<const TTTOOLINFOW*>(lParam);
					if (!ti)
						return FALSE;
					auto it = std::find_if(d->tools.begin(), d->tools.end(), [&](const Tool& t) { return t.owner == ti->hwnd && t.id == ti->uId; });
					if (it == d->tools.end())
					{
						if (msg != TTM_ADDTOOLW)
							return FALSE;
						d->tools.push_back(Tool{});
						it = d->tools.end() - 1;
						it->owner = ti->hwnd;
						it->id = ti->uId;
						it->flags = ti->uFlags;
						QWidget* toolWidget = (ti->uFlags & TTF_IDISHWND) ? widgetOf(reinterpret_cast<HWND>(ti->uId)) : widgetOf(ti->hwnd);
						if (toolWidget)
						{
							auto* f = new TooltipFilter(w, w->widget);
							toolWidget->installEventFilter(f);
							toolWidget->setAttribute(Qt::WA_AlwaysShowToolTips);
						}
					}
					if (msg != TTM_NEWTOOLRECTW && ti->lpszText)
					{
						it->callback = (ti->lpszText == LPSTR_TEXTCALLBACKW);
						if (!it->callback)
						{
							if (isIntResource(ti->lpszText))
							{
								wchar_t buf[512] = {};
								LoadStringW(nullptr, static_cast<UINT>(intResource(ti->lpszText)), buf, 512);
								it->text = qs(buf);
							}
							else
								it->text = qs(ti->lpszText);
						}
					}
					if (msg != TTM_UPDATETIPTEXTW)
						it->rect = ti->rect;
					it->lParam = ti->lParam;
					return TRUE;
				}
				case TTM_DELTOOLW:
				{
					const auto* ti = reinterpret_cast<const TTTOOLINFOW*>(lParam);
					if (ti)
						d->tools.erase(std::remove_if(d->tools.begin(), d->tools.end(), [&](const Tool& t) { return t.owner == ti->hwnd && t.id == ti->uId; }), d->tools.end());
					return 0;
				}
				case TTM_GETTOOLINFOW:
				{
					auto* ti = reinterpret_cast<TTTOOLINFOW*>(lParam);
					if (!ti)
						return FALSE;
					for (const Tool& t : d->tools)
					{
						if (t.owner == ti->hwnd && t.id == ti->uId)
						{
							ti->uFlags = t.flags;
							ti->rect = t.rect;
							ti->lParam = t.lParam;
							if (ti->lpszText && !t.callback)
								copyOut(t.text, ti->lpszText, 80);
							return TRUE;
						}
					}
					return FALSE;
				}
				case TTM_ACTIVATE:
					d->active = wParam != 0;
					if (!d->active)
						QToolTip::hideText();
					return 0;
				case TTM_SETTITLEW:
					d->title = qs(reinterpret_cast<LPCWSTR>(lParam));
					return TRUE;
				case TTM_TRACKPOSITION:
					d->trackPos = POINT{static_cast<short>(LOWORD(lParam)), static_cast<short>(HIWORD(lParam))};
					if (d->tracking)
						tooltipProc(w, TTM_TRACKACTIVATE, TRUE, 0);
					return 0;
				case TTM_TRACKACTIVATE:
				{
					d->tracking = wParam != 0;
					if (!d->tracking)
					{
						QToolTip::hideText();
						return 0;
					}
					const auto* ti = reinterpret_cast<const TTTOOLINFOW*>(lParam);
					for (const Tool& t : d->tools)
					{
						if (!ti || (t.owner == ti->hwnd && t.id == ti->uId))
						{
							QString text = toolText(w, t);
							if (!d->title.isEmpty())
								text = QStringLiteral("<b>%1</b><br>%2").arg(d->title.toHtmlEscaped(), text.toHtmlEscaped());
							QToolTip::showText(QPoint(d->trackPos.x, d->trackPos.y), text, widgetOf(t.owner));
							break;
						}
					}
					return 0;
				}
				case TTM_POP:
					QToolTip::hideText();
					return 0;
				case TTM_SETMAXTIPWIDTH:
					return -1;
				case TTM_SETDELAYTIME:
					return 0;
				default:
					return -0x7FFFFFFF;
			}
		}

		// ---- SysLink -------------------------------------------------------------------------
		QString linkMarkup(const QString& text)
		{
			// <a>x</a> and <a href="u">x</a> of SysLink: Qt rich text with the link index
			QString out;
			static const QRegularExpression re(QStringLiteral("<a(\\s+[^>]*)?>(.*?)</a>"), QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
			static const QRegularExpression hrefRe(QStringLiteral("href\\s*=\\s*\"([^\"]*)\""), QRegularExpression::CaseInsensitiveOption);
			static const QRegularExpression idRe(QStringLiteral("id\\s*=\\s*\"([^\"]*)\""), QRegularExpression::CaseInsensitiveOption);
			int last = 0;
			int index = 0;
			auto it = re.globalMatch(text);
			while (it.hasNext())
			{
				const auto m = it.next();
				out += text.mid(last, m.capturedStart() - last).toHtmlEscaped();
				const QString attrs = m.captured(1);
				const QString href = hrefRe.match(attrs).captured(1);
				const QString id = idRe.match(attrs).captured(1);
				out += QStringLiteral("<a href=\"%1|%2|%3\">%4</a>").arg(index).arg(QString(href).replace(QLatin1Char('"'), QStringLiteral("&quot;")),
					id, m.captured(2).toHtmlEscaped());
				last = static_cast<int>(m.capturedEnd());
				++index;
			}
			out += text.mid(last).toHtmlEscaped();
			return out;
		}
	}

	void setupCommonControl(Window* w)
	{
		QWidget* parent = static_cast<QWidget*>(w->handle.widget);
		switch (w->kind)
		{
			case Kind::ListView:
			case Kind::TreeView:
			{
				auto* t = new QTreeWidget(parent);
				auto* d = new ViewData;
				w->ctl = d;
				w->ctlFree = [](void* p) { delete static_cast<ViewData*>(p); };
				w->widget = t;
				t->setItemDelegate(new CallbackDelegate(w, t));
				t->setContextMenuPolicy(Qt::PreventContextMenu);
				if (w->kind == Kind::ListView)
				{
					t->setRootIsDecorated(false);
					t->setItemsExpandable(false);
					t->setUniformRowHeights(true);
					t->setAllColumnsShowFocus(true);
					t->setColumnCount(1);
					const DWORD view = w->style & LVS_TYPEMASK;
					t->setHeaderHidden(view != LVS_REPORT || (w->style & LVS_NOCOLUMNHEADER));
					t->header()->setStretchLastSection(false);
					t->header()->setSectionsClickable(!(w->style & LVS_NOSORTHEADER));
					t->setSelectionMode((w->style & LVS_SINGLESEL) ? QAbstractItemView::SingleSelection : QAbstractItemView::ExtendedSelection);
					t->setEditTriggers(QAbstractItemView::NoEditTriggers);
					QObject::connect(t, &QTreeWidget::itemSelectionChanged, t, [w] { lvSelectionChanged(w); });
					QObject::connect(t, &QTreeWidget::itemChanged, t, [w, t](QTreeWidgetItem* item, int column)
					{
						ViewData* vd = viewData(w);
						if (vd->silent || column != 0 || !(vd->exStyle & LVS_EX_CHECKBOXES))
							return;
						const int i = t->indexOfTopLevelItem(item);
						const UINT nowState = INDEXTOSTATEIMAGEMASK(item->checkState(0) == Qt::Checked ? 2 : 1);
						const UINT oldState = INDEXTOSTATEIMAGEMASK(item->checkState(0) == Qt::Checked ? 1 : 2);
						lvNotifyChanged(w, i, nowState, oldState, LVIF_STATE);
					});
					QObject::connect(t->header(), &QHeaderView::sectionClicked, t, [w](int section)
					{
						NMLISTVIEW nm{};
						nm.iItem = -1;
						nm.iSubItem = section;
						notify(w, &nm.hdr, LVN_COLUMNCLICK);
					});
					QObject::connect(t->itemDelegate(), &QAbstractItemDelegate::commitData, t, [w, t](QWidget* editor)
					{
						Q_UNUSED(editor);
						QTreeWidgetItem* item = t->currentItem();
						if (!item)
							return;
						NMLVDISPINFOW di{};
						di.item.iItem = t->indexOfTopLevelItem(item);
						const std::wstring text = ws(item->text(0));
						di.item.pszText = const_cast<wchar_t*>(text.c_str());
						di.item.mask = LVIF_TEXT;
						notify(w, &di.hdr, LVN_ENDLABELEDITW);
					});
				}
				else
				{
					t->setHeaderHidden(true);
					t->setRootIsDecorated(w->style & (TVS_LINESATROOT | TVS_HASBUTTONS));
					t->setSelectionMode(QAbstractItemView::SingleSelection);
					t->setEditTriggers(QAbstractItemView::NoEditTriggers);
					t->setExpandsOnDoubleClick(true);
					QObject::connect(t, &QTreeWidget::currentItemChanged, t, [w](QTreeWidgetItem* now, QTreeWidgetItem* before)
					{
						if (!viewData(w)->silent)
							tvSelChanged(w, now, before);
					});
					QObject::connect(t, &QTreeWidget::itemExpanded, t, [w, t](QTreeWidgetItem* item)
					{
						ViewData* vd = viewData(w);
						if (vd->silent)
							return;
						if (tvExpanding(w, item, TVE_EXPAND))
						{
							vd->silent = true;
							item->setExpanded(false);
							vd->silent = false;
							return;
						}
						item->setData(0, RoleExpandedOnce, true);
						tvExpanded(w, item, TVE_EXPAND);
						Q_UNUSED(t);
					});
					QObject::connect(t, &QTreeWidget::itemCollapsed, t, [w](QTreeWidgetItem* item)
					{
						ViewData* vd = viewData(w);
						if (vd->silent)
							return;
						if (tvExpanding(w, item, TVE_COLLAPSE))
						{
							vd->silent = true;
							item->setExpanded(true);
							vd->silent = false;
							return;
						}
						tvExpanded(w, item, TVE_COLLAPSE);
					});
					QObject::connect(t, &QTreeWidget::itemChanged, t, [w](QTreeWidgetItem* item, int)
					{
						ViewData* vd = viewData(w);
						if (vd->silent)
							return;
						NMTVDISPINFOW di{};
						tvFill(w, item, di.item);
						// a label edit ends (TVN_ENDLABELEDIT); a check box changes (TVN_ITEMCHANGED)
						QTreeWidget* tw = tree(w);
						const QVariant old = tw->property("_w32EditOld");
						if (old.isValid())
						{
							tw->setProperty("_w32EditOld", QVariant());
							const std::wstring text = ws(item->text(0));
							di.item.mask = TVIF_TEXT | TVIF_HANDLE | TVIF_PARAM;
							di.item.pszText = const_cast<wchar_t*>(text.c_str());
							if (!notify(w, &di.hdr, TVN_ENDLABELEDITW))
							{
								vd->silent = true;
								item->setText(0, old.toString());
								vd->silent = false;
							}
							item->setFlags(item->flags() & ~Qt::ItemIsEditable);
							return;
						}
						notify(w, &di.hdr, TVN_ITEMCHANGEDW);
					});
				}
				auto* filter = new ViewFilter(w, t);
				t->installEventFilter(filter);
				t->viewport()->installEventFilter(filter);
				if (w->style & WS_BORDER || w->exStyle & WS_EX_CLIENTEDGE)
					t->setFrameShape(QFrame::StyledPanel);
				break;
			}
			case Kind::Tab:
			{
				auto* box = new QWidget(parent);
				auto* d = new TabData;
				w->ctl = d;
				w->ctlFree = [](void* p) { delete static_cast<TabData*>(p); };
				d->bar = new QTabBar(box);
				d->bar->setDrawBase(true);
				d->bar->setExpanding(false);
				d->bar->setUsesScrollButtons(true);
				auto* frame = new QFrame(box);
				frame->setFrameShape(QFrame::StyledPanel);
				auto* layout = new QVBoxLayout(box);
				layout->setContentsMargins(0, 0, 0, 0);
				layout->setSpacing(0);
				layout->addWidget(d->bar);
				layout->addWidget(frame, 1);
				frame->lower();
				w->widget = box;
				QObject::connect(d->bar, &QTabBar::currentChanged, box, [w, d](int)
				{
					if (d->silent)
						return;
					NMHDR hdr{};
					notify(w, &hdr, TCN_SELCHANGING);
					NMHDR hdr2{};
					notify(w, &hdr2, TCN_SELCHANGE);
				});
				break;
			}
			case Kind::Trackbar:
			{
				auto* s = new QSlider((w->style & TBS_VERT) ? Qt::Vertical : Qt::Horizontal, parent);
				s->setRange(0, 100);
				s->setTickPosition((w->style & TBS_NOTICKS) ? QSlider::NoTicks : ((w->style & TBS_BOTH) ? QSlider::TicksBothSides
					: ((w->style & TBS_TOP) ? QSlider::TicksAbove : QSlider::TicksBelow)));
				w->widget = s;
				QObject::connect(s, &QSlider::actionTriggered, s, [w, s](int action)
				{
					int code = TB_THUMBTRACK;
					switch (action)
					{
						case QAbstractSlider::SliderSingleStepAdd: code = TB_LINEDOWN; break;
						case QAbstractSlider::SliderSingleStepSub: code = TB_LINEUP; break;
						case QAbstractSlider::SliderPageStepAdd: code = TB_PAGEDOWN; break;
						case QAbstractSlider::SliderPageStepSub: code = TB_PAGEUP; break;
						case QAbstractSlider::SliderToMinimum: code = TB_TOP; break;
						case QAbstractSlider::SliderToMaximum: code = TB_BOTTOM; break;
						default: break;
					}
					QTimer::singleShot(0, s, [w, s, code]
					{
						SendMessageW(w->parent, s->orientation() == Qt::Vertical ? WM_VSCROLL : WM_HSCROLL, MAKEWPARAM(code, s->value()), reinterpret_cast<LPARAM>(hwnd(w)));
					});
				});
				QObject::connect(s, &QSlider::sliderReleased, s, [w, s]
				{
					SendMessageW(w->parent, s->orientation() == Qt::Vertical ? WM_VSCROLL : WM_HSCROLL, MAKEWPARAM(TB_ENDTRACK, s->value()), reinterpret_cast<LPARAM>(hwnd(w)));
				});
				break;
			}
			case Kind::UpDown:
			{
				auto* box = new QWidget(parent);
				auto* layout = (w->style & UDS_HORZ) ? static_cast<QBoxLayout*>(new QHBoxLayout(box)) : static_cast<QBoxLayout*>(new QVBoxLayout(box));
				layout->setContentsMargins(0, 0, 0, 0);
				layout->setSpacing(0);
				auto* up = new QToolButton(box);
				auto* down = new QToolButton(box);
				up->setArrowType((w->style & UDS_HORZ) ? Qt::RightArrow : Qt::UpArrow);
				down->setArrowType((w->style & UDS_HORZ) ? Qt::LeftArrow : Qt::DownArrow);
				up->setAutoRepeat(true);
				down->setAutoRepeat(true);
				up->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
				down->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
				layout->addWidget(up);
				layout->addWidget(down);
				auto* d = new UpDownData;
				w->ctl = d;
				w->ctlFree = [](void* p) { delete static_cast<UpDownData*>(p); };
				w->widget = box;
				QObject::connect(up, &QToolButton::clicked, box, [w] { upDownApply(w, 1); });
				QObject::connect(down, &QToolButton::clicked, box, [w] { upDownApply(w, -1); });
				if (w->style & UDS_AUTOBUDDY)
				{
					// the control before it in the dialog is its buddy
					QTimer::singleShot(0, box, [w, box]
					{
						if (!w->parent)
							return;
						HWND prev = GetWindow(hwnd(w), GW_HWNDPREV);
						if (prev && !w->buddy)
						{
							w->buddy = prev;
							if (QWidget* bw = widgetOf(prev); bw && (w->style & (UDS_ALIGNRIGHT | UDS_ALIGNLEFT)))
							{
								const QRect r = bw->geometry();
								const int width = std::max(box->width(), 16);
								if (w->style & UDS_ALIGNRIGHT)
								{
									bw->setGeometry(r.adjusted(0, 0, -width, 0));
									box->setGeometry(r.right() - width + 1, r.top(), width, r.height());
								}
								else
								{
									bw->setGeometry(r.adjusted(width, 0, 0, 0));
									box->setGeometry(r.left(), r.top(), width, r.height());
								}
							}
						}
					});
				}
				break;
			}
			case Kind::Progress:
			{
				auto* p = new QProgressBar(parent);
				p->setRange(0, 100);
				p->setValue(0);
				p->setTextVisible(false);
				p->setOrientation((w->style & PBS_VERTICAL) ? Qt::Vertical : Qt::Horizontal);
				if (w->style & PBS_MARQUEE)
					p->setRange(0, 0);
				w->widget = p;
				break;
			}
			case Kind::Tooltip:
			{
				auto* t = new QWidget(parent);
				t->hide();
				auto* d = new TooltipData;
				w->ctl = d;
				w->ctlFree = [](void* p) { delete static_cast<TooltipData*>(p); };
				w->widget = t;
				break;
			}
			case Kind::Link:
			{
				auto* l = new QLabel(parent);
				l->setTextFormat(Qt::RichText);
				l->setWordWrap(true);
				l->setTextInteractionFlags(Qt::LinksAccessibleByMouse | Qt::LinksAccessibleByKeyboard);
				w->widget = l;
				QObject::connect(l, &QLabel::linkActivated, l, [w](const QString& link)
				{
					const QStringList parts = link.split(QLatin1Char('|'));
					NMLINK nm{};
					nm.item.mask = LIF_ITEMINDEX | LIF_URL | LIF_ITEMID;
					nm.item.iLink = parts.value(0).toInt();
					copyOut(parts.value(1), nm.item.szUrl, L_MAX_URL_LENGTH);
					copyOut(parts.value(2), nm.item.szID, MAX_LINKID_TEXT);
					notify(w, &nm.hdr, NM_CLICK);
				});
				break;
			}
			case Kind::StatusBar:
			{
				auto* box = new QFrame(parent);
				box->setFrameShape(QFrame::NoFrame);
				auto* layout = new QHBoxLayout(box);
				layout->setContentsMargins(4, 1, 4, 1);
				layout->setSpacing(8);
				auto* label = new QLabel(box);
				layout->addWidget(label, 1);
				w->widget = box;
				break;
			}
			default:
				break;
		}
	}

	LRESULT commonControlProc(Window* w, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		switch (w->kind)
		{
			case Kind::ListView:
				return listViewProc(w, msg, wParam, lParam);
			case Kind::TreeView:
				return treeViewProc(w, msg, wParam, lParam);
			case Kind::Tab:
				return tabProc(w, msg, wParam, lParam);
			case Kind::UpDown:
				return upDownProc(w, msg, wParam, lParam);
			case Kind::Tooltip:
				return tooltipProc(w, msg, wParam, lParam);
			case Kind::Trackbar:
				if (auto* s = qobject_cast<QSlider*>(w->widget))
				{
					const QSignalBlocker block(s);
					switch (msg)
					{
						case TBM_GETPOS: return s->value();
						case TBM_SETPOS: case TBM_SETPOSNOTIFY: s->setValue(static_cast<int>(lParam)); return 0;
						case TBM_SETRANGE: s->setRange(static_cast<short>(LOWORD(lParam)), static_cast<short>(HIWORD(lParam))); return 0;
						case TBM_SETRANGEMIN: s->setMinimum(static_cast<int>(lParam)); return 0;
						case TBM_SETRANGEMAX: s->setMaximum(static_cast<int>(lParam)); return 0;
						case TBM_GETRANGEMIN: return s->minimum();
						case TBM_GETRANGEMAX: return s->maximum();
						case TBM_SETPAGESIZE: { const int old = s->pageStep(); s->setPageStep(static_cast<int>(lParam)); return old; }
						case TBM_GETPAGESIZE: return s->pageStep();
						case TBM_SETLINESIZE: { const int old = s->singleStep(); s->setSingleStep(static_cast<int>(lParam)); return old; }
						case TBM_GETLINESIZE: return s->singleStep();
						case TBM_SETTICFREQ: s->setTickInterval(static_cast<int>(wParam)); return 0;
						case TBM_CLEARTICS: case TBM_SETTIC: case TBM_SETSEL: return TRUE;
						default: break;
					}
				}
				return -0x7FFFFFFF;
			case Kind::Progress:
				if (auto* p = qobject_cast<QProgressBar*>(w->widget))
				{
					switch (msg)
					{
						case PBM_SETRANGE: { const LRESULT old = MAKELONG(p->minimum(), p->maximum()); p->setRange(LOWORD(lParam), HIWORD(lParam)); return old; }
						case PBM_SETRANGE32: { const LRESULT old = MAKELONG(p->minimum(), p->maximum()); p->setRange(static_cast<int>(wParam), static_cast<int>(lParam)); return old; }
						case PBM_GETRANGE:
						{
							struct PBRANGE { int iLow; int iHigh; };
							if (auto* r = reinterpret_cast<PBRANGE*>(lParam))
							{
								r->iLow = p->minimum();
								r->iHigh = p->maximum();
							}
							return wParam ? p->minimum() : p->maximum();
						}
						case PBM_SETPOS: { const int old = p->value(); p->setValue(static_cast<int>(wParam)); return old; }
						case PBM_DELTAPOS: { const int old = p->value(); p->setValue(old + static_cast<int>(wParam)); return old; }
						case PBM_GETPOS: return p->value();
						case PBM_SETSTEP: { const int old = p->property("_w32Step").toInt(); p->setProperty("_w32Step", static_cast<int>(wParam)); return old ? old : 10; }
						case PBM_STEPIT:
						{
							const int old = p->value();
							const int step = p->property("_w32Step").isValid() ? p->property("_w32Step").toInt() : 10;
							int next = old + step;
							if (next > p->maximum())
								next = p->minimum() + (next - p->maximum());
							p->setValue(next);
							return old;
						}
						case PBM_SETMARQUEE: if (wParam) p->setRange(0, 0); else p->setRange(0, 100); return TRUE;
						case PBM_SETBARCOLOR:
						{
							QPalette pal = p->palette();
							pal.setColor(QPalette::Highlight, color(static_cast<COLORREF>(lParam)));
							p->setPalette(pal);
							return CLR_DEFAULT;
						}
						case CCM_SETBKCOLOR: return CLR_DEFAULT;
						case PBM_SETSTATE: return PBST_NORMAL;
						default: break;
					}
				}
				return -0x7FFFFFFF;
			case Kind::Link:
				if (auto* l = qobject_cast<QLabel*>(w->widget))
				{
					if (msg == WM_SETTEXT)
					{
						l->setText(linkMarkup(qs(reinterpret_cast<LPCWSTR>(lParam))));
						return TRUE;
					}
					if (msg == LM_SETITEM)
						return TRUE;
				}
				return -0x7FFFFFFF;
			case Kind::Header:
			{
				auto* t = static_cast<QTreeWidget*>(w->ctl);
				if (!t)
					return -0x7FFFFFFF;
				if (msg == HDM_GETITEMCOUNT)
					return t->columnCount();
				if (msg == HDM_GETITEMRECT)
				{
					auto* rc = reinterpret_cast<RECT*>(lParam);
					if (!rc)
						return FALSE;
					const int c = static_cast<int>(wParam);
					const int x = t->header()->sectionViewportPosition(c);
					*rc = RECT{x, 0, x + t->columnWidth(c), t->header()->height()};
					return TRUE;
				}
				return -0x7FFFFFFF;
			}
			case Kind::StatusBar:
				if (msg == SB_SETTEXTW)
				{
					if (QLabel* l = w->widget ? w->widget->findChild<QLabel*>() : nullptr)
						l->setText(qs(reinterpret_cast<LPCWSTR>(lParam)));
					return TRUE;
				}
				if (msg == SB_SETPARTS || msg == SB_SIMPLE)
					return TRUE;
				if (msg == WM_SETTEXT)
					return commonControlProc(w, SB_SETTEXTW, 0, lParam);
				return -0x7FFFFFFF;
			default:
				return -0x7FFFFFFF;
		}
	}
}

using namespace w32;

BOOL InitCommonControlsEx(const INITCOMMONCONTROLSEX*)
{
	return TRUE;
}

void InitCommonControls()
{
}

// ---- image lists ----------------------------------------------------------------------------
HIMAGELIST ImageList_Create(int cx, int cy, UINT, int, int)
{
	auto* il = new _IMAGELIST;
	il->cx = cx;
	il->cy = cy;
	return il;
}

BOOL ImageList_Destroy(HIMAGELIST il)
{
	delete il;
	return TRUE;
}

namespace
{
	// a strip of images of a bitmap, each cx wide (ImageList_Add of a toolbar bitmap)
	void addStrip(HIMAGELIST il, const QPixmap& strip, const QColor* mask)
	{
		if (strip.isNull())
			return;
		QImage img = strip.toImage().convertToFormat(QImage::Format_ARGB32);
		if (mask)
		{
			for (int y = 0; y < img.height(); ++y)
			{
				auto* line = reinterpret_cast<QRgb*>(img.scanLine(y));
				for (int x = 0; x < img.width(); ++x)
				{
					if ((line[x] & 0xFFFFFF) == (mask->rgb() & 0xFFFFFF))
						line[x] = 0;
				}
			}
		}
		const int n = std::max(1, img.width() / std::max(1, il->cx));
		for (int i = 0; i < n; ++i)
			il->icons.push_back(QIcon(QPixmap::fromImage(img.copy(i * il->cx, 0, std::min(il->cx, img.width()), img.height()))));
	}
}

int ImageList_Add(HIMAGELIST il, HBITMAP image, HBITMAP)
{
	if (!il)
		return -1;
	const int first = static_cast<int>(il->icons.size());
	addStrip(il, pixmapOf(image), nullptr);
	return first;
}

int ImageList_AddMasked(HIMAGELIST il, HBITMAP image, COLORREF mask)
{
	if (!il)
		return -1;
	const int first = static_cast<int>(il->icons.size());
	const QColor c = color(mask);
	addStrip(il, pixmapOf(image), &c);
	return first;
}

int ImageList_ReplaceIcon(HIMAGELIST il, int index, HICON icon)
{
	if (!il)
		return -1;
	const QIcon q = iconOf(icon);
	if (index < 0 || index >= static_cast<int>(il->icons.size()))
	{
		il->icons.push_back(q);
		return static_cast<int>(il->icons.size()) - 1;
	}
	il->icons[static_cast<size_t>(index)] = q;
	return index;
}

BOOL ImageList_Replace(HIMAGELIST il, int index, HBITMAP image, HBITMAP)
{
	if (!il || index < 0 || index >= static_cast<int>(il->icons.size()))
		return FALSE;
	il->icons[static_cast<size_t>(index)] = QIcon(pixmapOf(image));
	return TRUE;
}

BOOL ImageList_Remove(HIMAGELIST il, int index)
{
	if (!il)
		return FALSE;
	if (index < 0)
		il->icons.clear();
	else if (index < static_cast<int>(il->icons.size()))
		il->icons.erase(il->icons.begin() + index);
	return TRUE;
}

int ImageList_GetImageCount(HIMAGELIST il)
{
	return il ? static_cast<int>(il->icons.size()) : 0;
}

BOOL ImageList_SetImageCount(HIMAGELIST il, UINT count)
{
	if (!il)
		return FALSE;
	il->icons.resize(count);
	return TRUE;
}

BOOL ImageList_GetIconSize(HIMAGELIST il, int* cx, int* cy)
{
	if (!il)
		return FALSE;
	if (cx)
		*cx = il->cx;
	if (cy)
		*cy = il->cy;
	return TRUE;
}

BOOL ImageList_SetIconSize(HIMAGELIST il, int cx, int cy)
{
	if (!il)
		return FALSE;
	il->cx = cx;
	il->cy = cy;
	il->icons.clear();
	return TRUE;
}

HICON ImageList_GetIcon(HIMAGELIST il, int index, UINT)
{
	if (!il || index < 0 || index >= static_cast<int>(il->icons.size()))
		return nullptr;
	return iconHandle(il->icons[static_cast<size_t>(index)]);
}

BOOL ImageList_Draw(HIMAGELIST il, int index, HDC hdc, int x, int y, UINT)
{
	if (!il || index < 0 || index >= static_cast<int>(il->icons.size()))
		return FALSE;
	HICON icon = iconHandle(il->icons[static_cast<size_t>(index)]);
	DrawIconEx(hdc, x, y, icon, il->cx, il->cy, 0, nullptr, DI_NORMAL);
	DestroyIcon(icon);
	return TRUE;
}

BOOL ImageList_DrawEx(HIMAGELIST il, int index, HDC hdc, int x, int y, int dx, int dy, COLORREF, COLORREF, UINT)
{
	if (!il || index < 0 || index >= static_cast<int>(il->icons.size()))
		return FALSE;
	HICON icon = iconHandle(il->icons[static_cast<size_t>(index)]);
	DrawIconEx(hdc, x, y, icon, dx ? dx : il->cx, dy ? dy : il->cy, 0, nullptr, DI_NORMAL);
	DestroyIcon(icon);
	return TRUE;
}

COLORREF ImageList_SetBkColor(HIMAGELIST, COLORREF)
{
	return CLR_NONE;
}

HIMAGELIST ImageList_LoadImageW(HINSTANCE hInst, LPCWSTR name, int cx, int grow, COLORREF mask, UINT type, UINT flags)
{
	HANDLE image = LoadImageW(hInst, name, type, 0, 0, flags);
	if (!image)
		return nullptr;
	const QPixmap px = pixmapOf(image);
	HIMAGELIST il = ImageList_Create(cx, px.height(), ILC_COLOR32 | ILC_MASK, 0, grow);
	if (type == IMAGE_ICON)
		ImageList_ReplaceIcon(il, -1, image);
	else if (mask == CLR_NONE)
		ImageList_Add(il, image, nullptr);
	else
		ImageList_AddMasked(il, image, mask);
	DeleteObject(image);
	return il;
}
