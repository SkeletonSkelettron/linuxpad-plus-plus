// Based on NppImagePreview, Copyright (c) 2026 Mashkawat Ahsan.
// SPDX-License-Identifier: MIT

#include "NppPluginQt.h"
#include "PluginInterface.h"

#include <QAbstractScrollArea>
#include <QCache>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFrame>
#include <QGuiApplication>
#include <QImageReader>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QPointer>
#include <QRegularExpression>
#include <QScreen>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#include <array>
#include <vector>

namespace
{
	constexpr qint64 maxFileBytes = 24 * 1024 * 1024;
	constexpr qint64 maxPixels = 36000000;
	const QSize maxThumbnail(240, 200);
	constexpr int hoverDelay = 500;

	struct Thumbnail
	{
		QImage image;
		QString description;
		QDateTime modified;
		qint64 bytes = 0;
	};

	NppData host;
	QCache<QString, Thumbnail> thumbnails(50);
	QPointer<QFrame> popup;
	HWND popupView = nullptr;
	bool hoverEnabled = false;
	bool popupFromHover = false;
	std::array<LRESULT, 2> previousDwell{};

	HWND viewHandle(int view)
	{
		return view == 0 ? host._scintillaMainHandle : host._scintillaSecondHandle;
	}

	QString imagePathAt(HWND editor, Sci_Position position, bool manual)
	{
		const auto line = SendMessage(editor, SCI_LINEFROMPOSITION, position, 0);
		const auto start = SendMessage(editor, SCI_POSITIONFROMLINE, line, 0);
		const auto length = SendMessage(editor, SCI_GETLINEENDPOSITION, line, 0) - start;
		if (length <= 0 || length > 16384)
			return {};
		QByteArray bytes(length + 1, Qt::Uninitialized);
		Sci_TextRangeFull range{{start, start + length}, bytes.data()};
		if (SendMessage(editor, SCI_GETTEXTRANGEFULL, 0, reinterpret_cast<LPARAM>(&range)) != length)
			return {};
		const QString text = QString::fromUtf8(bytes.constData(), length);
		const auto column = QString::fromUtf8(bytes.constData(), qBound<Sci_Position>(0, position - start, length)).size();

		// The upstream parser recognizes quoted paths, CSS url() and Markdown
		// image links. Capture each path's span, including on multilingual lines.
		static const QRegularExpression reference(QStringLiteral(
			R"re("([^"\r\n]+%1)"|'([^'\r\n]+%1)'|\burl\s*\(\s*([^"')\r\n]+%1)\s*\)|!\[[^\]\r\n]*\]\(([^)\r\n]+%1)\))re")
			.arg(QStringLiteral(R"(\.(?:png|jpe?g|webp|gif|svg|bmp|ico|avif|tiff?))")),
			QRegularExpression::CaseInsensitiveOption);
		QString firstPath;
		auto matches = reference.globalMatch(text);
		while (matches.hasNext())
		{
			const auto match = matches.next();
			const int group = match.lastCapturedIndex();
			if (match.capturedStart(group) <= column && column <= match.capturedEnd(group))
				return match.captured(group);
			if (firstPath.isEmpty())
				firstPath = match.captured(group);
		}
		return manual ? firstPath : QString();
	}

	QString resolvePath(const QString& path, int view)
	{
		if (path.isEmpty())
			return {};
		const QUrl url(path);
		if (path.startsWith(QStringLiteral("//")) || !url.host().isEmpty())
			return {};
		if (url.isLocalFile())
			return url.toLocalFile();
		if (!url.scheme().isEmpty())
			return {};
		if (path.startsWith(QLatin1Char('/')))
			return path;

		const auto index = SendMessage(host._nppHandle, NPPM_GETCURRENTDOCINDEX, 0, view);
		if (index < 0)
			return {};
		const auto buffer = SendMessage(host._nppHandle, NPPM_GETBUFFERIDFROMPOS, index, view);
		const auto length = SendMessage(host._nppHandle, NPPM_GETFULLPATHFROMBUFFERID, buffer, 0);
		if (length <= 0)
			return {};
		std::vector<wchar_t> fullPath(length + 1);
		SendMessage(host._nppHandle, NPPM_GETFULLPATHFROMBUFFERID, buffer, reinterpret_cast<LPARAM>(fullPath.data()));
		const QString document = QString::fromWCharArray(fullPath.data());
		return document.startsWith(QLatin1Char('/')) ? QFileInfo(document).dir().filePath(path) : QString();
	}

	const Thumbnail* loadThumbnail(const QFileInfo& file)
	{
		if (!file.isFile() || file.size() > maxFileBytes)
			return nullptr;
		const QString path = file.canonicalFilePath();
		if (path.isEmpty())
			return nullptr;
		if (const auto* cached = thumbnails.object(path); cached &&
			cached->modified == file.lastModified() && cached->bytes == file.size())
			return cached;

		QImageReader reader(path);
		reader.setAutoTransform(true);
		const QSize size = reader.size();
		if (size.isEmpty() || qint64(size.width()) * size.height() > maxPixels)
			return nullptr;
		if (size.width() > maxThumbnail.width() || size.height() > maxThumbnail.height())
			reader.setScaledSize(size.scaled(maxThumbnail, Qt::KeepAspectRatio));
		const QByteArray format = reader.format();
		QImage image = reader.read();
		if (image.isNull())
			return nullptr;
		if (image.width() > maxThumbnail.width() || image.height() > maxThumbnail.height())
			image = image.scaled(maxThumbnail, Qt::KeepAspectRatio, Qt::SmoothTransformation);
		const QString description = QStringLiteral("%1 × %2 px · %3 · %4")
			.arg(size.width()).arg(size.height()).arg(QString::fromLatin1(format.toUpper()),
				QLocale().formattedDataSize(file.size()));
		auto* loaded = new Thumbnail{std::move(image), description, file.lastModified(), file.size()};
		thumbnails.insert(path, loaded);
		return loaded;
	}

	void hidePreview()
	{
		if (popup)
		{
			popup->hide();
			popup->deleteLater();
			popup.clear();
		}
		popupView = nullptr;
	}

	void preview(HWND editor, Sci_Position position, QPoint where, bool manual)
	{
		if (!editor || position < 0)
			return;
		auto* widget = nppWidget(editor);
		if (!widget || !widget->isVisible())
			return;
		auto* scrollArea = qobject_cast<QAbstractScrollArea*>(widget);
		QWidget* viewport = scrollArea ? scrollArea->viewport() : widget;

		const QString rawPath = imagePathAt(editor, position, manual);
		if (rawPath.isEmpty() && !manual)
			return;
		const QString localPath = resolvePath(rawPath, editor == viewHandle(0) ? 0 : 1);
		const QFileInfo info(localPath);
		const Thumbnail* thumbnail = loadThumbnail(info);

		hidePreview();
		auto* frame = new QFrame(viewport, Qt::ToolTip | Qt::WindowDoesNotAcceptFocus);
		frame->setObjectName(QStringLiteral("nppImagePreviewPopup"));
		frame->setAttribute(Qt::WA_ShowWithoutActivating);
		frame->setAttribute(Qt::WA_TransparentForMouseEvents);
		frame->setFrameShape(QFrame::StyledPanel);
		auto* layout = new QVBoxLayout(frame);
		if (thumbnail)
		{
			auto* image = new QLabel(frame);
			image->setObjectName(QStringLiteral("nppImagePreviewThumbnail"));
			image->setPixmap(QPixmap::fromImage(thumbnail->image));
			image->setAlignment(Qt::AlignCenter);
			layout->addWidget(image);
		}
		QString description;
		if (thumbnail)
			description = info.fileName() + QLatin1Char('\n') + thumbnail->description;
		else if (rawPath.isEmpty())
			description = QStringLiteral("Place the cursor on an image path.");
		else if (localPath.isEmpty())
			description = QStringLiteral("Preview requires a local path and a saved document for relative paths.");
		else
			description = info.isFile() ? QStringLiteral("This image could not be previewed.") : QStringLiteral("Image file not found.");
		auto* label = new QLabel(description, frame);
		label->setTextFormat(Qt::PlainText);
		label->setWordWrap(true);
		label->setMaximumWidth(300);
		layout->addWidget(label);
		frame->adjustSize();

		QPoint location = viewport->mapToGlobal(where + QPoint(12, 20));
		auto* screen = QGuiApplication::screenAt(location);
		const auto bounds = (screen ? screen : viewport->screen())->availableGeometry();
		location.setX(qBound(bounds.left(), location.x(), qMax(bounds.left(), bounds.right() + 1 - frame->width())));
		if (location.y() + frame->height() > bounds.bottom())
			location.setY(qMax(bounds.top(), location.y() - frame->height() - 36));
		frame->move(location);
		popup = frame;
		popupView = editor;
		popupFromHover = !manual;
		frame->show();
		if (manual)
		{
			QTimer::singleShot(5000, frame, [frame]
			{
				if (popup == frame)
					hidePreview();
			});
		}
	}

	void setHover(bool enabled)
	{
		for (int view : {0, 1})
		{
			HWND editor = viewHandle(view);
			const auto delay = SendMessage(editor, SCI_GETMOUSEDWELLTIME, 0, 0);
			if (enabled)
			{
				previousDwell[view] = delay;
				if (delay > hoverDelay)
					SendMessage(editor, SCI_SETMOUSEDWELLTIME, hoverDelay, 0);
			}
			else if (delay == hoverDelay && previousDwell[view] > hoverDelay)
				SendMessage(editor, SCI_SETMOUSEDWELLTIME, previousDwell[view], 0);
		}
		hoverEnabled = enabled;
	}

	void toggleHover();

	void showAtCaret()
	{
		int view = 0;
		SendMessage(host._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, reinterpret_cast<LPARAM>(&view));
		const HWND editor = viewHandle(view);
		const auto position = SendMessage(editor, SCI_GETCURRENTPOS, 0, 0);
		const QPoint where(SendMessage(editor, SCI_POINTXFROMPOSITION, 0, position),
			SendMessage(editor, SCI_POINTYFROMPOSITION, 0, position));
		preview(editor, position, where, true);
	}

	void clearCache()
	{
		thumbnails.clear();
	}

	void about()
	{
		QMessageBox::about(nppWidget(host._nppHandle), QStringLiteral("Image Preview"),
			QStringLiteral("NppImagePreview <br> by Mashkawat Ahsan"));
	}

	FuncItem commands[] = {
		{L"Enable Image Preview", toggleHover},
		{L"Preview Image at Cursor", showAtCaret},
		{L"Clear Thumbnail Cache", clearCache},
		{L"About", about},
	};

	void toggleHover()
	{
		setHover(!hoverEnabled);
		SendMessage(host._nppHandle, NPPM_SETMENUITEMCHECK, commands[0]._cmdID, hoverEnabled);
		if (!hoverEnabled)
			hidePreview();
	}
}

extern "C" void setInfo(NppData data) { host = data; }
extern "C" const wchar_t* getName() { return L"Image Preview"; }
extern "C" FuncItem* getFuncsArray(int* count)
{
	*count = std::size(commands);
	return commands;
}
extern "C" LRESULT messageProc(UINT, WPARAM, LPARAM) { return TRUE; }
extern "C" BOOL isUnicode() { return TRUE; }

extern "C" void beNotified(SCNotification* notification)
{
	const auto editor = static_cast<HWND>(notification->nmhdr.hwndFrom);
	switch (notification->nmhdr.code)
	{
		case NPPN_SHUTDOWN:
			if (hoverEnabled)
				setHover(false);
			clearCache();
			hidePreview();
			break;
		case NPPN_BUFFERACTIVATED:
		case NPPN_FILEBEFORECLOSE:
			hidePreview();
			break;
		case SCN_DWELLSTART:
			if (hoverEnabled && (editor == viewHandle(0) || editor == viewHandle(1)))
				preview(editor, notification->position, QPoint(notification->x, notification->y), false);
			break;
		case SCN_DWELLEND:
			if (popupFromHover && editor == popupView)
				hidePreview();
			break;
		case SCN_UPDATEUI:
			if (editor == popupView && (notification->updated & (SC_UPDATE_CONTENT | SC_UPDATE_H_SCROLL | SC_UPDATE_V_SCROLL)))
				hidePreview();
			break;
	}
}
