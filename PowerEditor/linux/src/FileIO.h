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

// Documents (buffers) and the reading / writing of files in their encodings.
// Linux counterpart of ScintillaComponent/Buffer.cpp and Utf8_16.cpp:
// Scintilla always holds UTF-8, conversion happens at load and save time.

#pragma once

#include <QByteArray>
#include <QDateTime>
#include <QString>

#include "NppCommon.h"

enum class UniMode
{
	uni8Bit = 0,   // a code page (ANSI or a character set)
	uniUTF8 = 1,   // UTF-8 with BOM
	uni16BE = 2,   // UTF-16 BE with BOM
	uni16LE = 3,   // UTF-16 LE with BOM
	uniCookie = 4, // UTF-8 without BOM
	uni7Bit = 5,
	uni16BE_NoBOM = 6,
	uni16LE_NoBOM = 7
};

inline constexpr int CP_ANSI = -1; // "ANSI": Windows-1252 on Linux

struct Buffer
{
	sptr_t doc = 0; // Scintilla document
	QString path;   // empty for "new 1" documents
	QString untitledName;
	UniMode unicode = UniMode::uniCookie;
	int codepage = CP_ANSI; // used when unicode == uni8Bit
	int eol = SC_EOL_LF;
	LangType lang = L_TEXT;
	QString udlName; // L_USER: name of the User Defined Language
	bool dirty = false;
	bool forcedDirty = false; // restored from a session snapshot
	bool userReadOnly = false;
	bool fileReadOnly = false;
	bool monitoring = false;
	bool pinned = false;
	bool missing = false;
	bool largeFile = false;          // Preferences > Performance > Large File Restriction
	bool untitledTabRenamed = false; // renamed by the user: no first line tab name
	int tabColor = -1;               // View > Tab > Apply Color 1-5
	QDateTime lastModified;
	QString backupPath;
	bool backupStale = true;

	// position in each view (main / sub), saved when the document is hidden
	struct ViewPos
	{
		sptr_t firstVisibleLine = 0;
		sptr_t anchor = 0;
		sptr_t caret = 0;
		sptr_t xOffset = 0;
	};
	ViewPos pos[2];

	bool isUntitled() const { return path.isEmpty(); }
	bool isReadOnly() const { return userReadOnly || fileReadOnly || monitoring; }
	QString displayName() const;
	QString fullName() const;
};

namespace FileIO
{
	struct Loaded
	{
		bool ok = false;
		QString error;
		QByteArray utf8;
		UniMode unicode = UniMode::uniCookie;
		int codepage = CP_ANSI;
		int eol = -1; // -1: no line ending found
		QByteArray head; // first bytes of the raw file, for language detection
	};

	enum class Force
	{
		Auto,
		Utf8,      // reinterpret as UTF-8 (BOM kept if present)
		Utf16BE,
		Utf16LE,
		Codepage   // reinterpret in the given code page
	};

	Loaded load(const QString& path, Force force = Force::Auto, int codepage = CP_ANSI);
	Loaded decode(const QByteArray& raw, Force force = Force::Auto, int codepage = CP_ANSI);

	QByteArray encode(const QByteArray& utf8, UniMode unicode, int codepage, bool* lossy);
	bool save(const QString& path, const QByteArray& utf8, UniMode unicode, int codepage, QString* error, bool* lossy);

	QByteArray toUtf8(const QByteArray& bytes, int codepage, bool* lossy);
	QByteArray fromUtf8(const QByteArray& utf8, int codepage, bool* lossy);

	const char* iconvName(int codepage);
	bool isCodepageSupported(int codepage);
	QString encodingName(UniMode unicode, int codepage);
	int detectEol(const QByteArray& data);
	bool isValidUtf8(const QByteArray& data);
}
