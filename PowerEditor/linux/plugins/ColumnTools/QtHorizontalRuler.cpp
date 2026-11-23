// SPDX-License-Identifier: GPL-2.0-or-later

#include "NppHorizontalRuler.h"

#include <cmath>

#include <QAbstractScrollArea>
#include <QBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QPointer>

#include <NppPluginQt.h>

extern NppData nppData;
extern int g_iEdgeModeOrig;

HorizontalRuler mainHRuler;
HorizontalRuler subHRuler;

namespace
{
	class ColumnRuler : public QWidget
	{
	public:
		ColumnRuler(HWND editor, QAbstractScrollArea* view, const HorizontalRuler& settings)
			: QWidget(view->parentWidget()), _editor(editor), _view(view), _settings(settings)
		{
			setObjectName(QStringLiteral("columnToolsRuler"));
			setAccessibleName(tr("Column ruler"));
			setToolTip(tr("Double-click to toggle an edge line. Shift adds a line. Ctrl clears multiple lines."));
		}

		void refreshFont()
		{
			QFont labelFont = parentWidget()->font();
			const auto editorPoints = send(SCI_STYLEGETSIZEFRACTIONAL, STYLE_DEFAULT) / qreal(SC_FONT_SIZE_MULTIPLIER);
			const auto points = _settings.bFontFix ? _settings.nFontSize : editorPoints + send(SCI_GETZOOM);
			labelFont.setPointSizeF(qMax(1.0, points));
			setFont(labelFont);
			setFixedHeight(fontMetrics().height() + 7);
			update();
		}

	protected:
		void paintEvent(QPaintEvent*) override
		{
			const Scale scale = rulerScale();
			QPainter paint(this);
			paint.fillRect(rect(), palette().window());
			paint.setPen(palette().mid().color());
			paint.drawLine(0, height() - 1, width() - 1, height() - 1);
			paint.setClipRect(scale.left, 0, scale.right - scale.left, height() - 1);
			paint.setPen(palette().windowText().color());

			const auto first = qMax<qintptr>(0, std::floor((scale.left - scale.origin) / scale.advance));
			const auto caret = caretColumn();
			for (qintptr column = first; ; ++column)
			{
				const int x = qRound(scale.origin + column * scale.advance);
				if (x >= scale.right)
					break;
				const bool numbered = column % 10 == 0;
				paint.drawLine(x, height() - (numbered ? 7 : 4), x, height() - 2);
				if (numbered)
					paint.drawText(x + 2, fontMetrics().ascent(), QString::number(column + _settings.bRulerStart));
				if (column == caret)
					paint.fillRect(x + 1, height() - 6, qMax(1, qRound(scale.advance) - 1), 4, palette().highlight());
			}
		}

		void mouseDoubleClickEvent(QMouseEvent* event) override
		{
			const Scale scale = rulerScale();
			const qreal x = event->position().x();
			if (event->button() != Qt::LeftButton || x < scale.left || x >= scale.right)
				return;
			const auto column = qMax<qintptr>(0, std::floor((x - scale.origin) / scale.advance));
			if (event->modifiers() & Qt::ShiftModifier)
			{
				send(SCI_SETEDGEMODE, EDGE_MULTILINE);
				send(SCI_MULTIEDGEADDLINE, column, send(SCI_GETEDGECOLOUR));
			}
			else if (event->modifiers() & Qt::ControlModifier)
			{
				send(SCI_MULTIEDGECLEARALL);
				send(SCI_SETEDGEMODE, g_iEdgeModeOrig);
			}
			else if (send(SCI_GETEDGECOLUMN) == column && send(SCI_GETEDGEMODE) == EDGE_LINE)
			{
				send(SCI_SETEDGEMODE, EDGE_NONE);
			}
			else
			{
				send(SCI_SETEDGECOLUMN, column);
				send(SCI_SETEDGEMODE, EDGE_LINE);
			}
			event->accept();
		}

	private:
		struct Scale
		{
			int left;
			int right;
			int origin;
			qreal advance;
		};

		LRESULT send(UINT message, WPARAM wParam = 0, LPARAM lParam = 0) const
		{
			return SendMessage(_editor, message, wParam, lParam);
		}

		Scale rulerScale() const
		{
			const QWidget* viewport = _view->viewport();
			const int frameLeft = mapFromGlobal(viewport->mapToGlobal(QPoint())).x();
			int left = frameLeft + send(SCI_GETMARGINLEFT);
			const auto margins = send(SCI_GETMARGINS);
			for (int index = 0; index < margins; ++index)
				left += send(SCI_GETMARGINWIDTHN, index);
			// Measure several spaces so a fractional advance stays aligned after scrolling.
			static const char spaces[] = "                                ";
			const qreal advance = send(SCI_TEXTWIDTH, STYLE_DEFAULT, reinterpret_cast<LPARAM>(spaces)) / qreal(sizeof(spaces) - 1);
			return {left, frameLeft + viewport->width(), left - static_cast<int>(send(SCI_GETXOFFSET)), qMax(1.0, advance)};
		}

		qintptr caretColumn() const
		{
			const auto column = send(SCI_GETCOLUMN, send(SCI_GETCURRENTPOS));
			return column + (send(SCI_SELECTIONISRECTANGLE) ? send(SCI_GETRECTANGULARSELECTIONCARETVIRTUALSPACE)
				: send(SCI_GETSELECTIONNCARETVIRTUALSPACE, send(SCI_GETMAINSELECTION)));
		}

		HWND _editor;
		QAbstractScrollArea* _view;
		const HorizontalRuler& _settings;
	};

	QPointer<ColumnRuler> mainRuler;
	QPointer<ColumnRuler> subRuler;

	void setRulerVisible(QPointer<ColumnRuler>& widget, HorizontalRuler& settings, HWND editor, bool visible)
	{
		settings.SetEnable(visible);
		if (visible && !widget)
		{
			auto* view = qobject_cast<QAbstractScrollArea*>(nppWidget(editor));
			auto* layout = view && view->parentWidget() ? qobject_cast<QBoxLayout*>(view->parentWidget()->layout()) : nullptr;
			if (!layout)
				return;
			widget = new ColumnRuler(editor, view, settings);
			layout->insertWidget(0, widget);
		}
		if (widget)
		{
			if (visible)
				widget->refreshFont();
			widget->setVisible(visible);
		}
	}
}

void ruler(bool enable)
{
	setRulerVisible(mainRuler, mainHRuler, nppData._scintillaMainHandle, enable);
	setRulerVisible(subRuler, subHRuler, nppData._scintillaSecondHandle, enable);
}

void refreshRulers(HWND editor)
{
	if (editor)
	{
		if (editor == nppData._scintillaMainHandle && mainRuler)
			mainRuler->update();
		if (editor == nppData._scintillaSecondHandle && subRuler)
			subRuler->update();
	}
	else
	{
		if (mainRuler)
			mainRuler->refreshFont();
		if (subRuler)
			subRuler->refreshFont();
	}
}

void removeRulers()
{
	delete mainRuler.data();
	delete subRuler.data();
}

LRESULT onHorizontalRulerMessageProc(UINT message, WPARAM, LPARAM)
{
	if (message == WM_SIZE)
		refreshRulers();
	return TRUE;
}
