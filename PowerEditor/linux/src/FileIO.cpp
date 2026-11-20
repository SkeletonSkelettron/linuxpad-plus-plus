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

#include "FileIO.h"

#include <algorithm>

#include <cerrno>
#include <cstring>
#include <iconv.h>

#include <QFile>
#include <QFileInfo>
#include <QStringDecoder>
#include <QStringEncoder>

#include "EncodingMapper.h"
#include "uchardet.h"

QString Buffer::displayName() const
{
	return isUntitled() ? untitledName : QFileInfo(path).fileName();
}

QString Buffer::fullName() const
{
	return isUntitled() ? untitledName : path;
}

namespace
{
	struct CodepageName
	{
		int codepage;
		const char* iconv;
		const char* display;
	};

	// Code pages of the Encoding menu (EncodingMapper.cpp) and their glibc iconv names
	const CodepageName kCodepages[] = {
		{1250, "WINDOWS-1250", "Windows-1250"}, {1251, "WINDOWS-1251", "Windows-1251"}, {1252, "WINDOWS-1252", "Windows-1252"},
		{1253, "WINDOWS-1253", "Windows-1253"}, {1254, "WINDOWS-1254", "Windows-1254"}, {1255, "WINDOWS-1255", "Windows-1255"},
		{1256, "WINDOWS-1256", "Windows-1256"}, {1257, "WINDOWS-1257", "Windows-1257"}, {1258, "WINDOWS-1258", "Windows-1258"},
		{28591, "ISO-8859-1", "ISO 8859-1"}, {28592, "ISO-8859-2", "ISO 8859-2"}, {28593, "ISO-8859-3", "ISO 8859-3"},
		{28594, "ISO-8859-4", "ISO 8859-4"}, {28595, "ISO-8859-5", "ISO 8859-5"}, {28596, "ISO-8859-6", "ISO 8859-6"},
		{28597, "ISO-8859-7", "ISO 8859-7"}, {28598, "ISO-8859-8", "ISO 8859-8"}, {28599, "ISO-8859-9", "ISO 8859-9"},
		{28603, "ISO-8859-13", "ISO 8859-13"}, {28604, "ISO-8859-14", "ISO 8859-14"}, {28605, "ISO-8859-15", "ISO 8859-15"},
		{437, "CP437", "OEM-US"}, {720, "CP720", "OEM 720"}, {737, "CP737", "OEM 737"}, {775, "CP775", "OEM 775"}, {850, "CP850", "OEM 850"},
		{852, "CP852", "OEM 852"}, {855, "CP855", "OEM 855"}, {857, "CP857", "OEM 857"}, {858, "CP858", "OEM 858"},
		{860, "CP860", "OEM 860"}, {861, "CP861", "OEM 861"}, {862, "CP862", "OEM 862"}, {863, "CP863", "OEM 863"},
		{865, "CP865", "OEM 865"}, {866, "CP866", "OEM 866"}, {869, "CP869", "OEM 869"},
		{950, "CP950", "Big5 (Traditional)"}, {936, "CP936", "GB2312 (Simplified)"}, {932, "CP932", "Shift-JIS"},
		{949, "CP949", "Windows 949"}, {51949, "EUC-KR", "EUC-KR"}, {874, "CP874", "TIS-620"},
		{10007, "MAC-CYRILLIC", "Mac Cyrillic"}, {21866, "KOI8-U", "KOI8-U"}, {20866, "KOI8-R", "KOI8-R"},
	};

	QByteArray iconvConvert(const QByteArray& input, const char* from, const char* to, bool* lossy, const char* replacement)
	{
		iconv_t cd = iconv_open(to, from);
		if (cd == reinterpret_cast<iconv_t>(-1))
		{
			if (lossy)
				*lossy = true;
			return input;
		}

		QByteArray out;
		out.reserve(input.size() * 2 + 16);
		char* inPtr = const_cast<char*>(input.constData());
		size_t inLeft = static_cast<size_t>(input.size());
		char buffer[16384];

		while (inLeft > 0)
		{
			char* outPtr = buffer;
			size_t outLeft = sizeof(buffer);
			const size_t r = iconv(cd, &inPtr, &inLeft, &outPtr, &outLeft);
			out.append(buffer, static_cast<qsizetype>(sizeof(buffer) - outLeft));
			if (r == static_cast<size_t>(-1))
			{
				if (errno == E2BIG)
					continue;
				// invalid or unrepresentable sequence: emit a replacement and skip one input char
				if (lossy)
					*lossy = true;
				out.append(replacement);
				if (std::strcmp(from, "UTF-8") == 0)
				{
					// skip a whole UTF-8 sequence
					unsigned char c = static_cast<unsigned char>(*inPtr);
					size_t n = (c < 0x80) ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : (c >> 3) == 0x1E ? 4 : 1;
					n = std::min(n, inLeft);
					inPtr += n;
					inLeft -= n;
				}
				else
				{
					++inPtr;
					--inLeft;
				}
				iconv(cd, nullptr, nullptr, nullptr, nullptr);
			}
		}
		char* outPtr = buffer;
		size_t outLeft = sizeof(buffer);
		iconv(cd, nullptr, nullptr, &outPtr, &outLeft);
		out.append(buffer, static_cast<qsizetype>(sizeof(buffer) - outLeft));
		iconv_close(cd);
		return out;
	}

	// OEM 720 (DOS Arabic): glibc iconv has no CP720, so the table is here (bytes 0x80-0xFF)
	const char16_t kCp720[128] = {
		0x0080, 0x0081, 0x00E9, 0x00E2, 0x0084, 0x00E0, 0x0086, 0x00E7,
		0x00EA, 0x00EB, 0x00E8, 0x00EF, 0x00EE, 0x008D, 0x008E, 0x008F,
		0x0090, 0x0651, 0x0652, 0x00F4, 0x00A4, 0x0640, 0x00FB, 0x00F9,
		0x0621, 0x0622, 0x0623, 0x0624, 0x00A3, 0x0625, 0x0626, 0x0627,
		0x0628, 0x0629, 0x062A, 0x062B, 0x062C, 0x062D, 0x062E, 0x062F,
		0x0630, 0x0631, 0x0632, 0x0633, 0x0634, 0x0635, 0x00AB, 0x00BB,
		0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556,
		0x2555, 0x2563, 0x2551, 0x2557, 0x255D, 0x255C, 0x255B, 0x2510,
		0x2514, 0x2534, 0x252C, 0x251C, 0x2500, 0x253C, 0x255E, 0x255F,
		0x255A, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256C, 0x2567,
		0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256B,
		0x256A, 0x2518, 0x250C, 0x2588, 0x2584, 0x258C, 0x2590, 0x2580,
		0x0636, 0x0637, 0x0638, 0x0639, 0x063A, 0x0641, 0x00B5, 0x0642,
		0x0643, 0x0644, 0x0645, 0x0646, 0x0647, 0x0648, 0x0649, 0x064A,
		0x2261, 0x064B, 0x064C, 0x064D, 0x064E, 0x064F, 0x0650, 0x2248,
		0x00B0, 0x2219, 0x00B7, 0x221A, 0x207F, 0x00B2, 0x25A0, 0x00A0,
	};

	QByteArray cp720ToUtf8(const QByteArray& bytes)
	{
		QString s;
		s.reserve(bytes.size());
		for (const char c : bytes)
		{
			const auto b = static_cast<unsigned char>(c);
			s.append(b < 0x80 ? QChar(b) : QChar(kCp720[b - 0x80]));
		}
		return s.toUtf8();
	}

	QByteArray utf8ToCp720(const QByteArray& utf8, bool* lossy)
	{
		QByteArray out;
		out.reserve(utf8.size());
		for (const char32_t u : QString::fromUtf8(utf8).toUcs4())
		{
			if (u < 0x80)
			{
				out.append(static_cast<char>(u));
				continue;
			}
			const char16_t* found = (u <= 0xFFFF) ? std::find(std::begin(kCp720), std::end(kCp720), static_cast<char16_t>(u)) : std::end(kCp720);
			if (found != std::end(kCp720))
			{
				out.append(static_cast<char>(0x80 + (found - std::begin(kCp720))));
			}
			else
			{
				out.append('?');
				if (lossy)
					*lossy = true;
			}
		}
		return out;
	}

	int detectCodepage(const QByteArray& data)
	{
		// Port of FileManager::detectCodepage()
		int codepage = -1;
		uchardet_t ud = uchardet_new();
		uchardet_handle_data(ud, data.constData(), static_cast<size_t>(data.size()));
		uchardet_data_end(ud);
		const char* cs = uchardet_get_charset(ud);
		if (cs && *cs && strcasecmp(cs, "TIS-620") != 0)
			codepage = EncodingMapper::getInstance().getEncodingFromString(cs);
		uchardet_delete(ud);
		return codepage;
	}
}

namespace FileIO
{
	const char* iconvName(int codepage)
	{
		if (codepage == CP_ANSI)
			return "WINDOWS-1252";
		for (const CodepageName& c : kCodepages)
		{
			if (c.codepage == codepage)
				return c.iconv;
		}
		return nullptr;
	}

	bool isCodepageSupported(int codepage)
	{
		if (codepage == 720)
			return true; // kCp720
		const char* name = iconvName(codepage);
		if (!name)
			return false;
		iconv_t cd = iconv_open("UTF-8", name);
		if (cd == reinterpret_cast<iconv_t>(-1))
			return false;
		iconv_close(cd);
		return true;
	}

	QString encodingName(UniMode unicode, int codepage)
	{
		switch (unicode)
		{
			case UniMode::uniUTF8: return QStringLiteral("UTF-8-BOM");
			case UniMode::uniCookie:
			case UniMode::uni7Bit: return QStringLiteral("UTF-8");
			case UniMode::uni16BE: return QStringLiteral("UTF-16 BE BOM");
			case UniMode::uni16LE: return QStringLiteral("UTF-16 LE BOM");
			case UniMode::uni16BE_NoBOM: return QStringLiteral("UTF-16 Big Endian");
			case UniMode::uni16LE_NoBOM: return QStringLiteral("UTF-16 Little Endian");
			case UniMode::uni8Bit:
			default:
				break;
		}
		if (codepage == CP_ANSI)
			return QStringLiteral("ANSI");
		for (const CodepageName& c : kCodepages)
		{
			if (c.codepage == codepage)
				return QString::fromLatin1(c.display);
		}
		return QStringLiteral("ANSI");
	}

	int detectEol(const QByteArray& data)
	{
		const char* p = data.constData();
		const qsizetype n = data.size();
		for (qsizetype i = 0; i < n; ++i)
		{
			if (p[i] == '\r')
				return (i + 1 < n && p[i + 1] == '\n') ? SC_EOL_CRLF : SC_EOL_CR;
			if (p[i] == '\n')
				return SC_EOL_LF;
		}
		return -1;
	}

	bool isValidUtf8(const QByteArray& data)
	{
		const auto* s = reinterpret_cast<const unsigned char*>(data.constData());
		const qsizetype n = data.size();
		qsizetype i = 0;
		while (i < n)
		{
			const unsigned char c = s[i];
			if (c < 0x80)
			{
				++i;
				continue;
			}
			int len = 0;
			if ((c & 0xE0) == 0xC0 && c >= 0xC2)
				len = 2;
			else if ((c & 0xF0) == 0xE0)
				len = 3;
			else if ((c & 0xF8) == 0xF0 && c <= 0xF4)
				len = 4;
			else
				return false;
			if (i + len > n)
				return false; // truncated sequence
			for (int k = 1; k < len; ++k)
			{
				if ((s[i + k] & 0xC0) != 0x80)
					return false;
			}
			if (len == 3)
			{
				const unsigned int cp = ((c & 0x0F) << 12) | ((s[i + 1] & 0x3F) << 6) | (s[i + 2] & 0x3F);
				if (cp < 0x800 || (cp >= 0xD800 && cp <= 0xDFFF))
					return false;
			}
			else if (len == 4)
			{
				const unsigned int cp = ((c & 0x07) << 18) | ((s[i + 1] & 0x3F) << 12) | ((s[i + 2] & 0x3F) << 6) | (s[i + 3] & 0x3F);
				if (cp < 0x10000 || cp > 0x10FFFF)
					return false;
			}
			i += len;
		}
		return true;
	}

	QByteArray toUtf8(const QByteArray& bytes, int codepage, bool* lossy)
	{
		if (codepage == 720)
			return cp720ToUtf8(bytes);
		const char* name = iconvName(codepage);
		if (!name)
			name = "WINDOWS-1252";
		return iconvConvert(bytes, name, "UTF-8", lossy, "\xEF\xBF\xBD");
	}

	QByteArray fromUtf8(const QByteArray& utf8, int codepage, bool* lossy)
	{
		if (codepage == 720)
			return utf8ToCp720(utf8, lossy);
		const char* name = iconvName(codepage);
		if (!name)
			name = "WINDOWS-1252";
		return iconvConvert(utf8, "UTF-8", name, lossy, "?");
	}

	Loaded decode(const QByteArray& raw, Force force, int codepage)
	{
		Loaded r;
		r.ok = true;
		r.head = raw.left(1024);

		const auto* d = reinterpret_cast<const unsigned char*>(raw.constData());
		const qsizetype n = raw.size();
		const bool bomUtf8 = n >= 3 && d[0] == 0xEF && d[1] == 0xBB && d[2] == 0xBF;
		const bool bom16BE = n >= 2 && d[0] == 0xFE && d[1] == 0xFF;
		const bool bom16LE = n >= 2 && d[0] == 0xFF && d[1] == 0xFE;

		auto decode16 = [&](bool bigEndian, bool hasBom)
		{
			QStringDecoder dec(bigEndian ? QStringDecoder::Utf16BE : QStringDecoder::Utf16LE, QStringDecoder::Flag::Stateless);
			const QByteArray payload = hasBom ? raw.mid(2) : raw;
			const QString text = dec.decode(payload);
			r.utf8 = text.toUtf8();
		};

		switch (force)
		{
			case Force::Utf8:
				r.unicode = bomUtf8 ? UniMode::uniUTF8 : UniMode::uniCookie;
				r.utf8 = bomUtf8 ? raw.mid(3) : raw;
				break;
			case Force::Utf16BE:
				r.unicode = bom16BE ? UniMode::uni16BE : UniMode::uni16BE_NoBOM;
				decode16(true, bom16BE);
				break;
			case Force::Utf16LE:
				r.unicode = bom16LE ? UniMode::uni16LE : UniMode::uni16LE_NoBOM;
				decode16(false, bom16LE);
				break;
			case Force::Codepage:
				r.unicode = UniMode::uni8Bit;
				r.codepage = codepage;
				r.utf8 = toUtf8(raw, codepage, nullptr);
				break;
			case Force::Auto:
			default:
				if (bomUtf8)
				{
					r.unicode = UniMode::uniUTF8;
					r.utf8 = raw.mid(3);
				}
				else if (bom16LE)
				{
					r.unicode = UniMode::uni16LE;
					decode16(false, true);
				}
				else if (bom16BE)
				{
					r.unicode = UniMode::uni16BE;
					decode16(true, true);
				}
				else if (isValidUtf8(raw))
				{
					// a pure ASCII file is "ANSI" unless New Document > UTF-8 > Apply to opened ANSI files
					const NppPrefs& p = Npp::prefs();
					const bool ascii = std::all_of(raw.begin(), raw.end(), [](char c) { return static_cast<unsigned char>(c) < 0x80; });
					const bool ansiAsUtf8 = p.newDocOpenAnsiAsUtf8 && p.newDocUnicode == static_cast<int>(UniMode::uniCookie);
					r.unicode = (ascii && !ansiAsUtf8) ? UniMode::uni8Bit : UniMode::uniCookie;
					r.utf8 = raw;
				}
				else
				{
					// MISC. > Autodetect character encoding
					int cp = Npp::prefs().detectEncoding ? detectCodepage(raw) : CP_ANSI;
					if (cp == 10007)
					{
						// uchardet often reports Mac Cyrillic for Windows-1251 text: both share the
						// lower case letters, but Windows-1251 upper case letters are symbols in Mac
						// Cyrillic. Keep the decoding that gives more letters.
						auto letters = [](const QByteArray& utf8)
						{
							int n = 0;
							for (const QChar c : QString::fromUtf8(utf8))
								n += c.isLetter() ? 1 : 0;
							return n;
						};
						if (letters(toUtf8(raw, 1251, nullptr)) >= letters(toUtf8(raw, 10007, nullptr)))
							cp = 1251;
					}
					r.unicode = UniMode::uni8Bit;
					r.codepage = (cp > 0 && isCodepageSupported(cp)) ? cp : CP_ANSI;
					r.utf8 = toUtf8(raw, r.codepage, nullptr);
				}
				break;
		}

		r.eol = detectEol(r.utf8);
		return r;
	}

	Loaded load(const QString& path, Force force, int codepage)
	{
		QFile f(path);
		if (!f.open(QIODevice::ReadOnly))
		{
			Loaded r;
			r.error = f.errorString();
			return r;
		}
		const QByteArray raw = f.readAll();
		if (f.error() != QFileDevice::NoError)
		{
			Loaded r;
			r.error = f.errorString();
			return r;
		}
		return decode(raw, force, codepage);
	}

	QByteArray encode(const QByteArray& utf8, UniMode unicode, int codepage, bool* lossy)
	{
		if (lossy)
			*lossy = false;
		switch (unicode)
		{
			case UniMode::uniUTF8:
				return QByteArray("\xEF\xBB\xBF", 3) + utf8;
			case UniMode::uniCookie:
			case UniMode::uni7Bit:
				return utf8;
			case UniMode::uni16BE:
			case UniMode::uni16BE_NoBOM:
			case UniMode::uni16LE:
			case UniMode::uni16LE_NoBOM:
			{
				const bool be = (unicode == UniMode::uni16BE || unicode == UniMode::uni16BE_NoBOM);
				const bool bom = (unicode == UniMode::uni16BE || unicode == UniMode::uni16LE);
				QStringEncoder enc(be ? QStringEncoder::Utf16BE : QStringEncoder::Utf16LE, QStringEncoder::Flag::Stateless);
				QByteArray out = enc.encode(QString::fromUtf8(utf8));
				if (bom)
					out.prepend(be ? QByteArray("\xFE\xFF", 2) : QByteArray("\xFF\xFE", 2));
				return out;
			}
			case UniMode::uni8Bit:
			default:
				return fromUtf8(utf8, codepage, lossy);
		}
	}

	bool save(const QString& path, const QByteArray& utf8, UniMode unicode, int codepage, QString* error, bool* lossy)
	{
		const QByteArray data = encode(utf8, unicode, codepage, lossy);

		// Write in place like Notepad++ does: keeps permissions, owner, links.
		QFile f(path);
		if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
		{
			if (error)
				*error = f.errorString();
			return false;
		}
		const qint64 written = f.write(data);
		const bool flushed = f.flush();
		f.close();
		if (written != data.size() || !flushed)
		{
			if (error)
				*error = f.errorString();
			return false;
		}
		return true;
	}
}
