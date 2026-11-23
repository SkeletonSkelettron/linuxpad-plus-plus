// Based on EditorConfig for Notepad++.
// Copyright (C) 2003 Don HO and 2011-2016 EditorConfig Team.
// SPDX-License-Identifier: GPL-2.0-or-later

#include "NppPluginQt.h"
#include "PluginInterface.h"

#include <editorconfig/editorconfig.h>
#include <editorconfig/editorconfig_handle.h>

#include <QFile>
#include <QFileInfo>
#include <QMap>
#include <QMessageBox>

#include <memory>
#include <vector>

namespace
{
	NppData host;
	using Rules = QMap<QByteArray, QByteArray>;

	LRESULT send(UINT message, WPARAM w = 0, LPARAM l = 0)
	{
		return SendMessage(host._nppHandle, message, w, l);
	}

	HWND currentEditor()
	{
		int view = 0;
		send(NPPM_GETCURRENTSCINTILLA, 0, reinterpret_cast<LPARAM>(&view));
		return view == 0 ? host._scintillaMainHandle : host._scintillaSecondHandle;
	}

	QString currentPath()
	{
		const auto buffer = send(NPPM_GETCURRENTBUFFERID);
		const auto length = send(NPPM_GETFULLPATHFROMBUFFERID, buffer);
		if (length <= 0)
			return {};
		std::vector<wchar_t> path(length + 1);
		send(NPPM_GETFULLPATHFROMBUFFERID, buffer, reinterpret_cast<LPARAM>(path.data()));
		return QString::fromWCharArray(path.data());
	}

	Rules readRules(const QString& path, QString* error = nullptr)
	{
		if (!path.startsWith(QLatin1Char('/')))
			return {};
		std::unique_ptr<void, decltype(&editorconfig_handle_destroy)> handle(editorconfig_handle_init(), editorconfig_handle_destroy);
		if (!handle)
			return {};
		const int status = editorconfig_parse(QFile::encodeName(path).constData(), handle.get());
		if (status != 0)
		{
			if (error)
			{
				*error = QString::fromUtf8(editorconfig_get_error_msg(status));
				if (const char* file = editorconfig_handle_get_err_file(handle.get()))
					*error += QStringLiteral("\n%1 · line %2").arg(QFile::decodeName(file)).arg(status);
			}
			return {};
		}

		Rules rules;
		const int count = editorconfig_handle_get_name_value_count(handle.get());
		for (int i = 0; i < count; ++i)
		{
			const char* name = nullptr;
			const char* value = nullptr;
			editorconfig_handle_get_name_value(handle.get(), i, &name, &value);
			if (value && qstrcmp(value, "unset") != 0)
				rules.insert(name, value);
		}
		return rules;
	}

	int lineEnding(const Rules& rules, int fallback)
	{
		const QByteArray mode = rules.value("end_of_line");
		if (mode == "lf") return SC_EOL_LF;
		if (mode == "crlf") return SC_EOL_CRLF;
		if (mode == "cr") return SC_EOL_CR;
		return fallback;
	}

	void showMessage(const QString& text)
	{
		QMessageBox dialog(QMessageBox::Information, QStringLiteral("EditorConfig"), text,
			QMessageBox::Ok, nppWidget(host._nppHandle));
		dialog.setTextFormat(Qt::PlainText);
		dialog.exec();
	}

	void loadRules(bool reportError)
	{
		const QString path = currentPath();
		if (!path.startsWith(QLatin1Char('/')))
			return;
		if (QFileInfo(path).fileName() == QStringLiteral(".editorconfig"))
		{
			int language = L_TEXT;
			send(NPPM_GETCURRENTLANGTYPE, 0, reinterpret_cast<LPARAM>(&language));
			if (language != L_INI)
				send(NPPM_SETCURRENTLANGTYPE, 0, L_INI);
		}

		QString error;
		const Rules rules = readRules(path, reportError ? &error : nullptr);
		if (!error.isEmpty())
		{
			showMessage(error);
			return;
		}
		HWND editor = currentEditor();
		if (!editor || rules.isEmpty())
			return;
		const QByteArray style = rules.value("indent_style");
		if (style == "space" || style == "tab")
			SendMessage(editor, SCI_SETUSETABS, style == "tab", 0);
		const QByteArray size = rules.value("indent_size");
		const int indent = size.toInt();
		const int tab = rules.value("tab_width").toInt();
		const int width = tab > 0 ? tab : indent;
		if (width > 0)
			SendMessage(editor, SCI_SETTABWIDTH, width, 0);
		if (size == "tab")
			SendMessage(editor, SCI_SETINDENT, SendMessage(editor, SCI_GETTABWIDTH, 0, 0), 0);
		else if (indent > 0)
			SendMessage(editor, SCI_SETINDENT, indent, 0);
		const int eol = lineEnding(rules, -1);
		if (eol >= 0)
			SendMessage(editor, SCI_SETEOLMODE, eol, 0);
	}

	void reload() { loadRules(true); }

	void showRules()
	{
		QString error;
		const Rules rules = readRules(currentPath(), &error);
		if (!error.isEmpty())
		{
			showMessage(error);
			return;
		}
		QStringList lines;
		for (auto i = rules.cbegin(); i != rules.cend(); ++i)
			lines.append(QString::fromUtf8(i.key()) + QStringLiteral(" = ") + QString::fromUtf8(i.value()));
		showMessage(lines.isEmpty() ? QStringLiteral("No EditorConfig rules apply to this file.") : lines.join(QLatin1Char('\n')));
	}

	void fixIndentation()
	{
		HWND editor = currentEditor();
		if (!editor || SendMessage(editor, SCI_GETREADONLY, 0, 0))
			return;
		const bool tabs = SendMessage(editor, SCI_GETUSETABS, 0, 0);
		const auto width = qMax<LRESULT>(1, SendMessage(editor, SCI_GETTABWIDTH, 0, 0));
		SendMessage(editor, SCI_BEGINUNDOACTION, 0, 0);
		const auto lines = SendMessage(editor, SCI_GETLINECOUNT, 0, 0);
		for (Sci_Position line = 0; line < lines; ++line)
		{
			const auto start = SendMessage(editor, SCI_POSITIONFROMLINE, line, 0);
			const auto end = SendMessage(editor, SCI_GETLINEINDENTPOSITION, line, 0);
			if (end <= start)
				continue;
			const auto columns = SendMessage(editor, SCI_GETLINEINDENTATION, line, 0);
			const QByteArray indentation = tabs ? QByteArray(columns / width, '\t') + QByteArray(columns % width, ' ')
				: QByteArray(columns, ' ');
			const auto* old = reinterpret_cast<const char*>(SendMessage(editor, SCI_GETRANGEPOINTER, start, end - start));
			if (old && QByteArrayView(old, end - start) == QByteArrayView(indentation))
				continue;
			SendMessage(editor, SCI_SETTARGETRANGE, start, end);
			SendMessage(editor, SCI_REPLACETARGET, indentation.size(), reinterpret_cast<LPARAM>(indentation.constData()));
		}
		SendMessage(editor, SCI_ENDUNDOACTION, 0, 0);
	}

	void trimTrailing(HWND editor)
	{
		for (Sci_Position line = SendMessage(editor, SCI_GETLINECOUNT, 0, 0); line > 0;)
		{
			--line;
			const auto start = SendMessage(editor, SCI_POSITIONFROMLINE, line, 0);
			const auto end = SendMessage(editor, SCI_GETLINEENDPOSITION, line, 0);
			auto cut = end;
			while (cut > start)
			{
				const int ch = SendMessage(editor, SCI_GETCHARAT, cut - 1, 0);
				if (ch != ' ' && ch != '\t')
					break;
				--cut;
			}
			if (cut < end)
				SendMessage(editor, SCI_DELETERANGE, cut, end - cut);
		}
	}

	void finalNewline(HWND editor, const QByteArray& setting, int eol)
	{
		if (setting != "true" && setting != "false")
			return;
		const auto length = SendMessage(editor, SCI_GETLENGTH, 0, 0);
		auto cut = length;
		while (cut > 0)
		{
			const auto ch = SendMessage(editor, SCI_GETCHARAT, cut - 1, 0);
			if (ch != '\n' && ch != '\r')
				break;
			--cut;
		}
		if (setting == "false" && cut < length)
			SendMessage(editor, SCI_DELETERANGE, cut, length - cut);
		if (setting == "true" && length > 0 && cut == length)
		{
			const char* ending = eol == SC_EOL_CRLF ? "\r\n" : eol == SC_EOL_CR ? "\r" : "\n";
			SendMessage(editor, SCI_APPENDTEXT, eol == SC_EOL_CRLF ? 2 : 1, reinterpret_cast<LPARAM>(ending));
		}
	}

	void formatSavedFile(HWND editor, const wchar_t* path, NppSaveFormat* format)
	{
		const Rules rules = readRules(QString::fromWCharArray(path));
		if (rules.isEmpty())
			return;
		const int eol = lineEnding(rules, -1);
		const QByteArray newline = rules.value("insert_final_newline");
		const bool trim = rules.value("trim_trailing_whitespace") == "true";
		if (trim || !newline.isEmpty() || eol >= 0)
		{
			SendMessage(editor, SCI_BEGINUNDOACTION, 0, 0);
			if (trim)
				trimTrailing(editor);
			if (eol >= 0)
			{
				SendMessage(editor, SCI_CONVERTEOLS, eol, 0);
				format->eol = eol;
			}
			finalNewline(editor, newline, format->eol);
			SendMessage(editor, SCI_ENDUNDOACTION, 0, 0);
		}
		const QByteArray charset = rules.value("charset");
		if (charset == "utf-8") format->encoding = NppSaveFormat::Utf8;
		else if (charset == "utf-8-bom") format->encoding = NppSaveFormat::Utf8Bom;
		else if (charset == "utf-16be") format->encoding = NppSaveFormat::Utf16BigEndian;
		else if (charset == "utf-16le") format->encoding = NppSaveFormat::Utf16LittleEndian;
		else if (charset == "latin1")
		{
			format->encoding = NppSaveFormat::Legacy;
			format->codepage = 28591;
		}
	}

	void about()
	{
		int major, minor, patch;
		editorconfig_get_version(&major, &minor, &patch);
		showMessage(QStringLiteral("EditorConfig for Notepad++\n\nEditorConfig Core C %1.%2.%3%4\neditorconfig.org")
			.arg(major).arg(minor).arg(patch).arg(QString::fromLatin1(editorconfig_get_version_suffix())));
	}

	FuncItem commands[] = {
		{L"Reload EditorConfig for this file", reload},
		{L"Show EditorConfig settings for this file", showRules},
		{},
		{L"Fix indentation", fixIndentation},
		{},
		{L"About...", about},
	};
}

extern "C" void setInfo(NppData data) { host = data; }
extern "C" const wchar_t* getName() { return L"EditorConfig"; }
extern "C" FuncItem* getFuncsArray(int* count)
{
	*count = std::size(commands);
	return commands;
}
extern "C" LRESULT messageProc(UINT, WPARAM, LPARAM) { return TRUE; }
extern "C" BOOL isUnicode() { return TRUE; }

extern "C" void beNotified(SCNotification* notification)
{
	switch (notification->nmhdr.code)
	{
		case NPPN_READY:
		case NPPN_BUFFERACTIVATED:
			loadRules(false);
			break;
		case NPPN_FILESAVED:
			if (notification->nmhdr.idFrom == static_cast<uptr_t>(send(NPPM_GETCURRENTBUFFERID)))
				loadRules(false);
			break;
		case NPPN_FILEBEFORESAVE:
			send(NPPM_LINUX_FILTERBEFORESAVE, notification->nmhdr.idFrom, reinterpret_cast<LPARAM>(&formatSavedFile));
			break;
	}
}
