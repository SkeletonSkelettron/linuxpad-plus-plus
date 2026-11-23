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

// Notepad++ Linux plugin SDK, npp_win32 library: the C run time of Microsoft that
// glibc does differently (wide printf formats, _itow, _wfopen...).

#include <cstdarg>
#include <cstdio>
#include <cwchar>
#include <algorithm>
#include <cerrno>
#include <cstdlib>

#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <sys/stat.h>

// the real glibc functions, not the macros of the SDK
#include <fcntl.h>
#include <unistd.h>

#include "W32Internal.h"
#undef swprintf
#undef vswprintf
#undef wprintf
#undef fwprintf
#undef swscanf

namespace
{
	// "%s" of Windows wide functions is a wchar_t string, "%S" a char one; glibc wants
	// "%ls" and "%s". "%I64d" is "%lld", "%ld" of Windows is 32 bit.
	std::wstring convert(const wchar_t* f, bool scan)
	{
		std::wstring out;
		if (!f)
			return out;
		for (const wchar_t* p = f; *p; ++p)
		{
			if (*p != L'%')
			{
				out += *p;
				continue;
			}
			out += *p++;
			if (*p == L'%')
			{
				out += L'%';
				continue;
			}
			// flags, width, precision
			while (*p && std::wcschr(L"-+ #0'*", *p))
				out += *p++;
			if (scan && *p == L'*')
				out += *p++;
			while (*p && (std::iswdigit(*p) || *p == L'.' || *p == L'*'))
				out += *p++;
			// length
			std::wstring length;
			if (p[0] == L'I' && p[1] == L'6' && p[2] == L'4')
			{
				length = L"ll";
				p += 3;
			}
			else if (p[0] == L'I' && p[1] == L'3' && p[2] == L'2')
			{
				p += 3;
			}
			else if (p[0] == L'I')
			{
				length = L"z";
				p += 1;
			}
			else if (p[0] == L'h' && p[1] == L'h')
			{
				length = L"hh";
				p += 2;
			}
			else if (p[0] == L'l' && p[1] == L'l')
			{
				length = L"ll";
				p += 2;
			}
			else if (*p == L'h' || *p == L'l' || *p == L'w' || *p == L'L' || *p == L'z' || *p == L'j' || *p == L't')
			{
				length = (*p == L'w') ? L"l" : std::wstring(1, *p);
				++p;
			}
			if (!*p)
				break;
			const wchar_t c = *p;
			switch (c)
			{
				case L's':
				case L'c':
					// h: char, l/w: wchar_t, none: wchar_t (wide function)
					if (length == L"h")
						out += c;
					else
					{
						out += L'l';
						out += c;
					}
					break;
				case L'S':
				case L'C':
					// the other width: char here
					if (length == L"l")
					{
						out += L'l';
						out += static_cast<wchar_t>(std::towlower(c));
					}
					else
						out += static_cast<wchar_t>(std::towlower(c));
					break;
				case L'd': case L'i': case L'u': case L'x': case L'X': case L'o':
					if (length != L"l")
						out += length;
					out += c;
					break;
				default:
					out += length;
					out += c;
					break;
			}
		}
		return out;
	}
}

std::wstring npp_windows_wformat(const wchar_t* format)
{
	return convert(format, false);
}

int npp_vsnwprintf(wchar_t* buffer, size_t count, const wchar_t* format, va_list args)
{
	if (!buffer || count == 0)
	{
		// the length it would take
		std::wstring tmp(65536, L'\0');
		va_list copy;
		va_copy(copy, args);
		const int n = vswprintf(tmp.data(), tmp.size(), convert(format, false).c_str(), copy);
		va_end(copy);
		return n;
	}
	const int n = vswprintf(buffer, count, convert(format, false).c_str(), args);
	if (n < 0 && count > 0)
		buffer[count - 1] = 0;
	return n;
}

int npp_snwprintf(wchar_t* buffer, size_t count, const wchar_t* format, ...)
{
	va_list ap;
	va_start(ap, format);
	const int n = npp_vsnwprintf(buffer, count, format, ap);
	va_end(ap);
	return n;
}

int npp_vfwprintf(FILE* f, const wchar_t* format, va_list args)
{
	return vfwprintf(f, convert(format, false).c_str(), args);
}

int npp_vswscanf(const wchar_t* s, const wchar_t* format, va_list args)
{
	return vswscanf(s, convert(format, true).c_str(), args);
}

int wsprintfW(LPWSTR buffer, LPCWSTR format, ...)
{
	va_list ap;
	va_start(ap, format);
	const int n = npp_vsnwprintf(buffer, 1024, format, ap); // wsprintf writes at most 1024 characters
	va_end(ap);
	return n;
}

int wsprintfA(LPSTR buffer, LPCSTR format, ...)
{
	va_list ap;
	va_start(ap, format);
	const int n = vsnprintf(buffer, 1024, format, ap);
	va_end(ap);
	return n;
}

int wvsprintfW(LPWSTR buffer, LPCWSTR format, va_list args)
{
	return npp_vsnwprintf(buffer, 1024, format, args);
}

char* npp_strlwr(char* s)
{
	for (char* p = s; p && *p; ++p)
		*p = static_cast<char>(std::tolower(static_cast<unsigned char>(*p)));
	return s;
}

char* npp_strupr(char* s)
{
	for (char* p = s; p && *p; ++p)
		*p = static_cast<char>(std::toupper(static_cast<unsigned char>(*p)));
	return s;
}

wchar_t* npp_wcslwr(wchar_t* s)
{
	for (wchar_t* p = s; p && *p; ++p)
		*p = static_cast<wchar_t>(std::towlower(*p));
	return s;
}

wchar_t* npp_wcsupr(wchar_t* s)
{
	for (wchar_t* p = s; p && *p; ++p)
		*p = static_cast<wchar_t>(std::towupper(*p));
	return s;
}

wchar_t* npp_wcsset(wchar_t* s, wchar_t c)
{
	for (wchar_t* p = s; p && *p; ++p)
		*p = c;
	return s;
}

namespace
{
	template <typename Char, typename T>
	Char* toText(T value, Char* buffer, int radix, bool isSigned)
	{
		if (!buffer)
			return buffer;
		if (radix < 2 || radix > 36)
			radix = 10;
		const bool neg = isSigned && radix == 10 && static_cast<long long>(value) < 0;
		unsigned long long v = neg ? static_cast<unsigned long long>(-static_cast<long long>(value)) : static_cast<unsigned long long>(value);
		if (isSigned && radix != 10 && sizeof(T) == 4)
			v &= 0xFFFFFFFFULL;
		Char tmp[72];
		int n = 0;
		do
		{
			const int d = static_cast<int>(v % static_cast<unsigned>(radix));
			tmp[n++] = static_cast<Char>(d < 10 ? '0' + d : 'a' + d - 10);
			v /= static_cast<unsigned>(radix);
		} while (v);
		int i = 0;
		if (neg)
			buffer[i++] = '-';
		while (n)
			buffer[i++] = tmp[--n];
		buffer[i] = 0;
		return buffer;
	}
}

wchar_t* npp_itow(int value, wchar_t* buffer, int radix) { return toText(value, buffer, radix, true); }
char* npp_itoa(int value, char* buffer, int radix) { return toText(value, buffer, radix, true); }
wchar_t* npp_ultow(unsigned long value, wchar_t* buffer, int radix) { return toText(value, buffer, radix, false); }
wchar_t* npp_i64tow(long long value, wchar_t* buffer, int radix) { return toText(value, buffer, radix, true); }
wchar_t* npp_ui64tow(unsigned long long value, wchar_t* buffer, int radix) { return toText(value, buffer, radix, false); }

std::string npp_path(const wchar_t* windowsPath)
{
	return QFile::encodeName(w32::nativePath(windowsPath)).toStdString();
}

std::string npp_path(const char* windowsPath)
{
	return QFile::encodeName(w32::nativePath(QString::fromUtf8(windowsPath ? windowsPath : ""))).toStdString();
}

FILE* npp_wfopen(const wchar_t* path, const wchar_t* mode)
{
	// "ccs=UTF-8" and the like of Microsoft: the file is opened as is
	QString m = w32::qs(mode);
	const int comma = static_cast<int>(m.indexOf(QLatin1Char(',')));
	if (comma >= 0)
		m = m.left(comma);
	m.remove(QLatin1Char('t'));
	return std::fopen(npp_path(path).c_str(), m.toLatin1().constData());
}

int _wopen(const wchar_t* path, int flags, int mode)
{
	return ::open(npp_path(path).c_str(), flags, mode);
}

int npp_wremove(const wchar_t* path)
{
	return std::remove(npp_path(path).c_str());
}

int npp_wrename(const wchar_t* from, const wchar_t* to)
{
	return std::rename(npp_path(from).c_str(), npp_path(to).c_str());
}

int npp_wmkdir(const wchar_t* path)
{
	return ::mkdir(npp_path(path).c_str(), 0755);
}

int npp_wchdir(const wchar_t* path)
{
	return ::chdir(npp_path(path).c_str());
}

wchar_t* npp_wgetcwd(wchar_t* buffer, int len)
{
	const std::wstring dir = QDir::currentPath().toStdWString();
	if (!buffer) {
		const size_t size = std::max<size_t>(dir.size() + 1, len > 0 ? static_cast<size_t>(len) : 0);
		buffer = static_cast<wchar_t*>(std::malloc(size * sizeof(wchar_t)));
		if (!buffer)
			return nullptr;
		wcscpy(buffer, dir.c_str());
		return buffer;
	}
	if (len <= 0 || dir.size() + 1 > static_cast<size_t>(len)) {
		errno = ERANGE;
		return nullptr;
	}
	wcscpy(buffer, dir.c_str());
	return buffer;
}

wchar_t* npp_wgetenv(const wchar_t* name)
{
	static thread_local std::wstring value;
	const char* v = getenv(w32::qs(name).toLocal8Bit().constData());
	if (!v)
		return nullptr;
	value = QString::fromLocal8Bit(v).toStdWString();
	return value.data();
}

int npp_wsystem(const wchar_t* command)
{
	return std::system(w32::qs(command).toLocal8Bit().constData());
}

int npp_wsplitpath_s(const wchar_t* path, wchar_t* drive, size_t driveLen, wchar_t* dir, size_t dirLen, wchar_t* fname, size_t fnameLen, wchar_t* ext, size_t extLen)
{
	QString p = w32::qs(path);
	QString drv;
	if (p.size() >= 2 && p[1] == QLatin1Char(':'))
	{
		drv = p.left(2);
		p = p.mid(2);
	}
	const int slash = static_cast<int>(std::max(p.lastIndexOf(QLatin1Char('/')), p.lastIndexOf(QLatin1Char('\\'))));
	const QString d = p.left(slash + 1);
	const QString file = p.mid(slash + 1);
	const int dot = static_cast<int>(file.lastIndexOf(QLatin1Char('.')));
	const QString name = dot >= 0 ? file.left(dot) : file;
	const QString e = dot >= 0 ? file.mid(dot) : QString();
	if (drive)
		w32::copyOut(drv, drive, static_cast<int>(driveLen));
	if (dir)
		w32::copyOut(d, dir, static_cast<int>(dirLen));
	if (fname)
		w32::copyOut(name, fname, static_cast<int>(fnameLen));
	if (ext)
		w32::copyOut(e, ext, static_cast<int>(extLen));
	return 0;
}

int npp_wmakepath_s(wchar_t* path, size_t len, const wchar_t* drive, const wchar_t* dir, const wchar_t* fname, const wchar_t* ext)
{
	QString p;
	if (drive && *drive)
		p += w32::qs(drive);
	if (dir && *dir)
	{
		p += w32::qs(dir);
		if (!p.endsWith(QLatin1Char('/')) && !p.endsWith(QLatin1Char('\\')))
			p += QLatin1Char('/');
	}
	if (fname)
		p += w32::qs(fname);
	if (ext && *ext)
	{
		if (*ext != L'.')
			p += QLatin1Char('.');
		p += w32::qs(ext);
	}
	w32::copyOut(p, path, static_cast<int>(len));
	return 0;
}

int npp_strncpy_s(char* dst, size_t dstLen, const char* src, size_t count)
{
	if (!dst || dstLen == 0)
		return 22;
	size_t n = (count == static_cast<size_t>(-1)) ? dstLen - 1 : std::min(count, dstLen - 1);
	n = std::min(n, src ? std::strlen(src) : 0);
	if (src)
		std::memcpy(dst, src, n);
	dst[n] = 0;
	return 0;
}
