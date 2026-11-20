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

#include "TabBar.h"

#include <algorithm>

#include <QApplication>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>

NppTabBar::NppTabBar(QWidget* parent)
	: QTabBar(parent)
{
}

void NppTabBar::setLook(const Look& look)
{
	_look = look;
	updateGeometry();
	update();
}

QSize NppTabBar::tabSizeHint(int index) const
{
	QSize sz = QTabBar::tabSizeHint(index);
	if (!_look.reduce)
	{
		// "Reduce" unchecked: taller tabs, like the Windows tab bar
		const bool vertical = (shape() == QTabBar::RoundedWest || shape() == QTabBar::RoundedEast ||
		                       shape() == QTabBar::TriangularWest || shape() == QTabBar::TriangularEast);
		if (vertical)
			sz.setWidth(sz.width() + 8);
		else
			sz.setHeight(sz.height() + 8);
	}
	return sz;
}

void NppTabBar::paintEvent(QPaintEvent* event)
{
	QTabBar::paintEvent(event);

	QPainter p(this);
	const bool vertical = (shape() == QTabBar::RoundedWest || shape() == QTabBar::RoundedEast ||
	                       shape() == QTabBar::TriangularWest || shape() == QTabBar::TriangularEast);
	const int current = currentIndex();
	for (int i = 0; i < count(); ++i)
	{
		const QRect r = tabRect(i);
		if (!r.isValid() || !event->rect().intersects(r))
			continue;

		const int colorIndex = _colorIndex ? _colorIndex(i) : -1;
		if (colorIndex >= 0 && colorIndex < 5 && _look.tabColors[static_cast<size_t>(colorIndex)].isValid())
		{
			QColor c = _look.tabColors[static_cast<size_t>(colorIndex)];
			c.setAlpha(i == current ? 110 : 80);
			p.fillRect(r.adjusted(1, 1, -1, -1), c);
		}
		else if (i != current && _look.inactiveColor && _look.inactiveBg.isValid())
		{
			QColor c = _look.inactiveBg;
			c.setAlpha(70);
			p.fillRect(r.adjusted(1, 1, -1, -1), c);
		}

		if (i == current && _look.activeBar)
		{
			const int thickness = 3;
			const QRect bar = vertical ? QRect(r.left(), r.top(), thickness, r.height()) : QRect(r.left(), r.top(), r.width(), thickness);
			p.fillRect(bar, _look.activeBarColor);
		}
	}
}

bool NppTabBar::decodeDrag(const QMimeData* mime, NppTabBar** source, int* index)
{
	if (!mime || !mime->hasFormat(QString::fromLatin1(dragMimeType())))
		return false;
	const QList<QByteArray> parts = mime->data(QString::fromLatin1(dragMimeType())).split(' ');
	if (parts.size() != 2)
		return false;
	*source = reinterpret_cast<NppTabBar*>(parts[0].toULongLong());
	*index = parts[1].toInt();
	return true;
}

void NppTabBar::mousePressEvent(QMouseEvent* event)
{
	if (event->button() == Qt::LeftButton)
	{
		_pressIndex = tabAt(event->position().toPoint());
		_pressPos = event->position().toPoint();
	}
	QTabBar::mousePressEvent(event);
}

void NppTabBar::mouseMoveEvent(QMouseEvent* event)
{
	// dragged away from the bar: start a drag that the other view accepts
	if (_pressIndex >= 0 && (event->buttons() & Qt::LeftButton))
	{
		const QPoint pos = event->position().toPoint();
		const bool vertical = (shape() == QTabBar::RoundedWest || shape() == QTabBar::RoundedEast);
		const int away = vertical ? std::max(-pos.x(), pos.x() - width()) : std::max(-pos.y(), pos.y() - height());
		if (away > 2 * QApplication::startDragDistance() + 8)
		{
			const int index = _pressIndex;
			_pressIndex = -1;
			// end the move of QTabBar itself first
			QMouseEvent release(QEvent::MouseButtonRelease, event->position(), event->globalPosition(), Qt::LeftButton, Qt::NoButton, event->modifiers());
			QTabBar::mouseReleaseEvent(&release);

			auto* mime = new QMimeData;
			mime->setData(QString::fromLatin1(dragMimeType()), QByteArray::number(reinterpret_cast<quintptr>(this)) + ' ' + QByteArray::number(index));
			auto* drag = new QDrag(this);
			drag->setMimeData(mime);
			drag->setPixmap(grab(tabRect(index)));
			drag->exec(Qt::MoveAction | Qt::CopyAction, Qt::MoveAction);
			return;
		}
	}
	QTabBar::mouseMoveEvent(event);
}

void NppTabBar::mouseReleaseEvent(QMouseEvent* event)
{
	_pressIndex = -1;
	QTabBar::mouseReleaseEvent(event);
}

void NppTabBar::dragEnterEvent(QDragEnterEvent* event)
{
	NppTabBar* source = nullptr;
	int index = -1;
	if (decodeDrag(event->mimeData(), &source, &index) && source != this)
	{
		event->setDropAction((event->modifiers() & Qt::ControlModifier) ? Qt::CopyAction : Qt::MoveAction);
		event->accept();
		return;
	}
	QTabBar::dragEnterEvent(event);
}

void NppTabBar::dragMoveEvent(QDragMoveEvent* event)
{
	NppTabBar* source = nullptr;
	int index = -1;
	if (decodeDrag(event->mimeData(), &source, &index) && source != this)
	{
		event->setDropAction((event->modifiers() & Qt::ControlModifier) ? Qt::CopyAction : Qt::MoveAction);
		event->accept();
		return;
	}
	QTabBar::dragMoveEvent(event);
}

void NppTabBar::dropEvent(QDropEvent* event)
{
	NppTabBar* source = nullptr;
	int index = -1;
	if (decodeDrag(event->mimeData(), &source, &index) && source != this)
	{
		int insertAt = tabAt(event->position().toPoint());
		if (insertAt < 0)
			insertAt = count();
		const bool copy = (event->modifiers() & Qt::ControlModifier) != 0;
		event->setDropAction(copy ? Qt::CopyAction : Qt::MoveAction);
		event->accept();
		emit tabDropped(source, index, insertAt, copy);
		return;
	}
	QTabBar::dropEvent(event);
}
