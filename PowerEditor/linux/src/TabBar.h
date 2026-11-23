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

// Document tab bar: Qt counterpart of WinControls/TabBar/TabBar.cpp drawing
// (coloured bar on the active tab, inactive tab colour, tab colours 1-5, reduced height).

#pragma once

#include <array>
#include <functional>

#include <QColor>
#include <QTabBar>

class QMimeData;

class NppTabBar : public QTabBar
{
	Q_OBJECT

public:
	explicit NppTabBar(QWidget* parent = nullptr);

	struct Look
	{
		bool reduce = true;
		bool activeBar = true;
		bool inactiveColor = true;
		QColor activeBarColor = QColor(250, 170, 60);
		QColor inactiveBg;
		QColor inactiveText;
		std::array<QColor, 5> tabColors{};
	};
	void setLook(const Look& look);
	const Look& look() const { return _look; }

	// colour index of each tab (-1: none), read through this callback
	void setColorIndexProvider(std::function<int(int)> fn) { _colorIndex = std::move(fn); }

	// a tab dragged out of a tab bar (to the other view, like Notepad++)
	static const char* dragMimeType() { return "application/x-linuxpad-plus-plus-tab"; }
	static bool decodeDrag(const QMimeData* mime, NppTabBar** source, int* index);

signals:
	void tabDropped(NppTabBar* source, int sourceIndex, int insertIndex, bool copy);

protected:
	void paintEvent(QPaintEvent* event) override;
	QSize tabSizeHint(int index) const override;
	void mousePressEvent(QMouseEvent* event) override;
	void mouseMoveEvent(QMouseEvent* event) override;
	void mouseReleaseEvent(QMouseEvent* event) override;
	void dragEnterEvent(QDragEnterEvent* event) override;
	void dragMoveEvent(QDragMoveEvent* event) override;
	void dropEvent(QDropEvent* event) override;

private:
	Look _look;
	std::function<int(int)> _colorIndex;
	int _pressIndex = -1;
	QPoint _pressPos;
};
