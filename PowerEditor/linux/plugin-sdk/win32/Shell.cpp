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

// Notepad++ Linux plugin SDK, npp_win32 library: shlwapi paths and strings,
// ShellExecute, special folders (the XDG folders) and the common dialogs.

#include <QApplication>
#include <QColorDialog>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QFontDialog>
#include <QMessageBox>
#include <QMimeDatabase>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUrl>

#include <fnmatch.h>
#include <map>

#include "W32Internal.h"
#include <shlwapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <commdlg.h>

using namespace w32;

namespace
{
	bool isSep(wchar_t c)
	{
		return c == L'/' || c == L'\\';
	}

	wchar_t* lastSep(const wchar_t* p)
	{
		const wchar_t* last = nullptr;
		for (const wchar_t* q = p; q && *q; ++q)
		{
			if (isSep(*q))
				last = q;
		}
		return const_cast<wchar_t*>(last);
	}

	QString unq(const QString& s)
	{
		if (s.size() >= 2 && s.startsWith(QLatin1Char('"')) && s.endsWith(QLatin1Char('"')))
			return s.mid(1, s.size() - 2);
		return s;
	}
}

BOOL PathFileExistsW(LPCWSTR path)
{
	return (path && *path && QFileInfo::exists(nativePath(path))) ? TRUE : FALSE;
}

BOOL PathFileExistsA(LPCSTR path)
{
	return (path && *path && QFileInfo::exists(nativePath(qsA(path)))) ? TRUE : FALSE;
}

BOOL PathIsDirectoryW(LPCWSTR path)
{
	return (path && QFileInfo(nativePath(path)).isDir()) ? FILE_ATTRIBUTE_DIRECTORY : FALSE;
}

BOOL PathIsDirectoryEmptyW(LPCWSTR path)
{
	const QDir d(nativePath(path));
	return (d.exists() && d.isEmpty(QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden | QDir::System)) ? TRUE : FALSE;
}

BOOL PathIsRelativeW(LPCWSTR path)
{
	if (!path || !*path)
		return TRUE;
	if (isSep(path[0]))
		return FALSE;
	if (path[0] && path[1] == L':')
		return FALSE;
	return TRUE;
}

BOOL PathIsRootW(LPCWSTR path)
{
	if (!path)
		return FALSE;
	const QString p = qs(path);
	return (p == QStringLiteral("/") || p == QStringLiteral("\\") || QRegularExpression(QStringLiteral("^[A-Za-z]:[\\\\/]?$")).match(p).hasMatch()) ? TRUE : FALSE;
}

BOOL PathIsUNCW(LPCWSTR path)
{
	return (path && path[0] == L'\\' && path[1] == L'\\') ? TRUE : FALSE;
}

BOOL PathIsURLW(LPCWSTR path)
{
	if (!path)
		return FALSE;
	const QUrl u(qs(path));
	return (u.isValid() && !u.scheme().isEmpty() && u.scheme().size() > 1) ? TRUE : FALSE;
}

BOOL PathIsFileSpecW(LPCWSTR path)
{
	for (const wchar_t* p = path; p && *p; ++p)
	{
		if (isSep(*p) || *p == L':')
			return FALSE;
	}
	return TRUE;
}

BOOL PathIsPrefixW(LPCWSTR prefix, LPCWSTR path)
{
	const QString a = nativePath(prefix);
	const QString b = nativePath(path);
	return (b.startsWith(a) && (b.size() == a.size() || a.endsWith(QLatin1Char('/')) || b[a.size()] == QLatin1Char('/'))) ? TRUE : FALSE;
}

BOOL PathIsSameRootW(LPCWSTR, LPCWSTR)
{
	return TRUE;
}

LPWSTR PathFindFileNameW(LPCWSTR path)
{
	if (!path)
		return nullptr;
	const wchar_t* s = lastSep(path);
	if (s && s[1] == 0)
	{
		// "a/b/": the last component is b/
		const wchar_t* prev = nullptr;
		for (const wchar_t* q = path; q < s; ++q)
		{
			if (isSep(*q))
				prev = q;
		}
		return const_cast<LPWSTR>(prev ? prev + 1 : path);
	}
	return const_cast<LPWSTR>(s ? s + 1 : path);
}

LPSTR PathFindFileNameA(LPCSTR path)
{
	if (!path)
		return nullptr;
	const char* last = nullptr;
	for (const char* q = path; *q; ++q)
	{
		if (*q == '/' || *q == '\\')
			last = q;
	}
	return const_cast<LPSTR>(last ? last + 1 : path);
}

LPWSTR PathFindExtensionW(LPCWSTR path)
{
	if (!path)
		return nullptr;
	const wchar_t* name = PathFindFileNameW(path);
	const wchar_t* dot = std::wcsrchr(name, L'.');
	return const_cast<LPWSTR>(dot ? dot : name + std::wcslen(name));
}

LPSTR PathFindExtensionA(LPCSTR path)
{
	if (!path)
		return nullptr;
	const char* name = PathFindFileNameA(path);
	const char* dot = std::strrchr(name, '.');
	return const_cast<LPSTR>(dot ? dot : name + std::strlen(name));
}

LPWSTR PathFindNextComponentW(LPCWSTR path)
{
	if (!path)
		return nullptr;
	const wchar_t* p = path;
	while (*p && !isSep(*p))
		++p;
	return const_cast<LPWSTR>(*p ? p + 1 : p);
}

LPWSTR PathGetArgsW(LPCWSTR path)
{
	if (!path)
		return nullptr;
	bool quoted = false;
	const wchar_t* p = path;
	for (; *p; ++p)
	{
		if (*p == L'"')
			quoted = !quoted;
		else if (*p == L' ' && !quoted)
			return const_cast<LPWSTR>(p + 1);
	}
	return const_cast<LPWSTR>(p);
}

BOOL PathRemoveFileSpecW(LPWSTR path)
{
	if (!path || !*path)
		return FALSE;
	wchar_t* s = lastSep(path);
	if (!s)
	{
		path[0] = 0;
		return TRUE;
	}
	if (s == path)
	{
		if (s[1] == 0)
			return FALSE;
		s[1] = 0; // keep the root
		return TRUE;
	}
	*s = 0;
	return TRUE;
}

BOOL PathRemoveFileSpecA(LPSTR path)
{
	if (!path || !*path)
		return FALSE;
	char* last = nullptr;
	for (char* q = path; *q; ++q)
	{
		if (*q == '/' || *q == '\\')
			last = q;
	}
	if (!last)
	{
		path[0] = 0;
		return TRUE;
	}
	if (last == path)
		last[1] = 0;
	else
		*last = 0;
	return TRUE;
}

void PathRemoveExtensionW(LPWSTR path)
{
	wchar_t* ext = PathFindExtensionW(path);
	if (ext)
		*ext = 0;
}

BOOL PathRenameExtensionW(LPWSTR path, LPCWSTR ext)
{
	if (!path || !ext)
		return FALSE;
	PathRemoveExtensionW(path);
	std::wcscat(path, ext);
	return TRUE;
}

BOOL PathAddExtensionW(LPWSTR path, LPCWSTR ext)
{
	if (!path)
		return FALSE;
	if (*PathFindExtensionW(path))
		return FALSE;
	std::wcscat(path, ext ? ext : L".exe");
	return TRUE;
}

void PathStripPathW(LPWSTR path)
{
	if (!path)
		return;
	wchar_t* name = PathFindFileNameW(path);
	if (name != path)
		std::wmemmove(path, name, std::wcslen(name) + 1);
}

BOOL PathStripToRootW(LPWSTR path)
{
	if (!path)
		return FALSE;
	if (isSep(path[0]))
	{
		path[1] = 0;
		return TRUE;
	}
	if (path[0] && path[1] == L':')
	{
		path[2] = L'\\';
		path[3] = 0;
		return TRUE;
	}
	return FALSE;
}

LPWSTR PathAddBackslashW(LPWSTR path)
{
	if (!path)
		return nullptr;
	const size_t n = std::wcslen(path);
	if (n && !isSep(path[n - 1]))
	{
		path[n] = L'/';
		path[n + 1] = 0;
		return path + n + 1;
	}
	return path + n;
}

LPWSTR PathRemoveBackslashW(LPWSTR path)
{
	if (!path)
		return nullptr;
	size_t n = std::wcslen(path);
	if (n > 1 && isSep(path[n - 1]))
		path[--n] = 0;
	return path + n;
}

void PathRemoveBlanksW(LPWSTR path)
{
	if (!path)
		return;
	const QString t = qs(path).trimmed();
	copyOut(t, path, static_cast<int>(std::wcslen(path)) + 1);
}

void PathRemoveArgsW(LPWSTR path)
{
	wchar_t* args = PathGetArgsW(path);
	if (args && args > path && *args)
		args[-1] = 0;
	else if (args && *args == 0 && args > path && args[-1] == L' ')
		args[-1] = 0;
}

BOOL PathAppendW(LPWSTR path, LPCWSTR more)
{
	if (!path)
		return FALSE;
	if (!more || !*more)
		return TRUE;
	while (isSep(*more))
		++more;
	if (*path)
		PathAddBackslashW(path);
	std::wcscat(path, more);
	return TRUE;
}

BOOL PathAppendA(LPSTR path, LPCSTR more)
{
	if (!path)
		return FALSE;
	if (!more || !*more)
		return TRUE;
	while (*more == '/' || *more == '\\')
		++more;
	const size_t n = std::strlen(path);
	if (n && path[n - 1] != '/' && path[n - 1] != '\\')
		std::strcat(path, "/");
	std::strcat(path, more);
	return TRUE;
}

LPWSTR PathCombineW(LPWSTR dst, LPCWSTR dir, LPCWSTR file)
{
	if (!dst)
		return nullptr;
	if (file && !PathIsRelativeW(file))
	{
		std::wcscpy(dst, file);
		return dst;
	}
	std::wstring tmp = dir ? dir : L"";
	std::vector<wchar_t> buf(tmp.size() + (file ? std::wcslen(file) : 0) + 4, 0);
	std::wcscpy(buf.data(), tmp.c_str());
	PathAppendW(buf.data(), file);
	wchar_t canon[MAX_PATH] = {};
	PathCanonicalizeW(canon, buf.data());
	std::wcscpy(dst, canon);
	return dst;
}

BOOL PathCanonicalizeW(LPWSTR dst, LPCWSTR src)
{
	if (!dst || !src)
		return FALSE;
	QString p = qs(src);
	const bool absolute = !p.isEmpty() && (isSep(p[0].unicode()));
	QStringList parts;
	for (const QString& c : p.split(QRegularExpression(QStringLiteral("[\\\\/]")), Qt::SkipEmptyParts))
	{
		if (c == QStringLiteral("."))
			continue;
		if (c == QStringLiteral(".."))
		{
			if (!parts.isEmpty())
				parts.removeLast();
			continue;
		}
		parts << c;
	}
	QString out = (absolute ? QStringLiteral("/") : QString()) + parts.join(QLatin1Char('/'));
	if (out.isEmpty())
		out = absolute ? QStringLiteral("/") : QStringLiteral("");
	copyOut(out, dst, MAX_PATH);
	return TRUE;
}

void PathQuoteSpacesW(LPWSTR path)
{
	if (!path || !std::wcschr(path, L' '))
		return;
	const size_t n = std::wcslen(path);
	std::wmemmove(path + 1, path, n + 1);
	path[0] = L'"';
	path[n + 1] = L'"';
	path[n + 2] = 0;
}

void PathUnquoteSpacesW(LPWSTR path)
{
	if (!path)
		return;
	const size_t n = std::wcslen(path);
	if (n >= 2 && path[0] == L'"' && path[n - 1] == L'"')
	{
		std::wmemmove(path, path + 1, n - 2);
		path[n - 2] = 0;
	}
}

BOOL PathMatchSpecA(LPCSTR file, LPCSTR spec)
{
	if (!file || !spec)
		return FALSE;
	const std::wstring f = ws(qsA(file));
	const std::wstring s = ws(qsA(spec));
	return PathMatchSpecW(f.c_str(), s.c_str());
}

BOOL PathMatchSpecW(LPCWSTR file, LPCWSTR spec)
{
	if (!file || !spec)
		return FALSE;
	const QString f = qs(PathFindFileNameW(file));
	for (const QString& pat : qs(spec).split(QLatin1Char(';'), Qt::SkipEmptyParts))
	{
		QString p = pat.trimmed();
		if (p == QStringLiteral("*.*"))
			return TRUE;
		if (fnmatch(p.toUtf8().constData(), f.toUtf8().constData(), FNM_CASEFOLD) == 0)
			return TRUE;
	}
	return FALSE;
}

BOOL PathCompactPathExW(LPWSTR out, LPCWSTR src, UINT max, DWORD)
{
	if (!out || !src || max == 0)
		return FALSE;
	QString s = qs(src);
	if (static_cast<UINT>(s.size()) >= max)
		s = QStringLiteral("...") + s.right(static_cast<int>(max) - 4);
	copyOut(s, out, static_cast<int>(max));
	return TRUE;
}

BOOL PathCompactPathW(HDC, LPWSTR path, UINT width)
{
	if (!path)
		return FALSE;
	const int chars = std::max(5, static_cast<int>(width / 7));
	QString s = qs(path);
	if (s.size() > chars)
		s = QStringLiteral("...") + s.right(chars - 3);
	copyOut(s, path, static_cast<int>(std::wcslen(path)) + 1);
	return TRUE;
}

int PathCommonPrefixW(LPCWSTR a, LPCWSTR b, LPWSTR out)
{
	const QStringList pa = nativePath(a).split(QLatin1Char('/'));
	const QStringList pb = nativePath(b).split(QLatin1Char('/'));
	QStringList common;
	for (int i = 0; i < std::min(pa.size(), pb.size()) && pa[i] == pb[i]; ++i)
		common << pa[i];
	const QString c = common.join(QLatin1Char('/'));
	if (out)
		copyOut(c, out, MAX_PATH);
	return static_cast<int>(ws(c).size());
}

BOOL PathRelativePathToW(LPWSTR out, LPCWSTR from, DWORD attrFrom, LPCWSTR to, DWORD)
{
	if (!out || !from || !to)
		return FALSE;
	QString base = nativePath(from);
	if (!(attrFrom & FILE_ATTRIBUTE_DIRECTORY))
		base = QFileInfo(base).path();
	QString rel = QDir(base).relativeFilePath(nativePath(to));
	if (!rel.startsWith(QStringLiteral("..")))
		rel = QStringLiteral("./") + rel;
	copyOut(rel, out, MAX_PATH);
	return TRUE;
}

int PathGetDriveNumberW(LPCWSTR path)
{
	if (path && path[0] && path[1] == L':')
		return std::towupper(path[0]) - L'A';
	return -1;
}

BOOL PathSearchAndQualifyW(LPCWSTR path, LPWSTR out, UINT max)
{
	if (!path || !out)
		return FALSE;
	copyOut(QFileInfo(nativePath(path)).absoluteFilePath(), out, static_cast<int>(max));
	return TRUE;
}

BOOL PathYetAnotherMakeUniqueName(LPWSTR out, LPCWSTR path, LPCWSTR, LPCWSTR longName)
{
	const QString dir = nativePath(path);
	const QString name = qs(longName);
	const QFileInfo fi(name);
	for (int i = 1; i < 1000; ++i)
	{
		const QString candidate = QStringLiteral("%1/%2 (%3)%4").arg(dir, fi.completeBaseName()).arg(i).arg(fi.suffix().isEmpty() ? QString() : QStringLiteral(".") + fi.suffix());
		if (!QFileInfo::exists(candidate))
		{
			copyOut(candidate, out, MAX_PATH);
			return TRUE;
		}
	}
	return FALSE;
}

HRESULT PathCchCombine(LPWSTR out, size_t max, LPCWSTR dir, LPCWSTR file)
{
	std::vector<wchar_t> buf(MAX_PATH, 0);
	PathCombineW(buf.data(), dir, file);
	return copyOut(qs(buf.data()), out, static_cast<int>(max)) >= 0 ? S_OK : E_FAIL;
}

HRESULT PathCchAppend(LPWSTR path, size_t max, LPCWSTR more)
{
	if (!path)
		return E_INVALIDARG;
	std::vector<wchar_t> buf(max + std::wcslen(more ? more : L"") + 4, 0);
	std::wcscpy(buf.data(), path);
	PathAppendW(buf.data(), more);
	copyOut(qs(buf.data()), path, static_cast<int>(max));
	return S_OK;
}

HRESULT PathCchRemoveFileSpec(LPWSTR path, size_t)
{
	return PathRemoveFileSpecW(path) ? S_OK : S_FALSE;
}

HRESULT PathCchAddBackslash(LPWSTR path, size_t)
{
	PathAddBackslashW(path);
	return S_OK;
}

HRESULT PathCchRemoveBackslash(LPWSTR path, size_t)
{
	PathRemoveBackslashW(path);
	return S_OK;
}

HRESULT PathCchFindExtension(LPCWSTR path, size_t, LPCWSTR* ext)
{
	if (!ext)
		return E_INVALIDARG;
	*ext = PathFindExtensionW(path);
	return S_OK;
}

HRESULT PathCchRenameExtension(LPWSTR path, size_t, LPCWSTR ext)
{
	return PathRenameExtensionW(path, ext) ? S_OK : E_FAIL;
}

HRESULT PathCchCanonicalize(LPWSTR out, size_t, LPCWSTR in)
{
	return PathCanonicalizeW(out, in) ? S_OK : E_FAIL;
}

LPWSTR StrStrIW(LPCWSTR s, LPCWSTR find)
{
	if (!s || !find)
		return nullptr;
	const QString hay = qs(s);
	const int i = static_cast<int>(hay.indexOf(qs(find), 0, Qt::CaseInsensitive));
	return i < 0 ? nullptr : const_cast<LPWSTR>(s + i);
}

LPSTR StrStrIA(LPCSTR s, LPCSTR find)
{
	if (!s || !find)
		return nullptr;
	return const_cast<LPSTR>(strcasestr(s, find));
}

LPWSTR StrStrW(LPCWSTR s, LPCWSTR find)
{
	return (s && find) ? const_cast<LPWSTR>(std::wcsstr(s, find)) : nullptr;
}

LPWSTR StrRStrIW(LPCWSTR s, LPCWSTR end, LPCWSTR find)
{
	if (!s || !find)
		return nullptr;
	const QString hay = end ? qs(s, static_cast<int>(end - s)) : qs(s);
	const int i = static_cast<int>(hay.lastIndexOf(qs(find), -1, Qt::CaseInsensitive));
	return i < 0 ? nullptr : const_cast<LPWSTR>(s + i);
}

int StrCmpIW(LPCWSTR a, LPCWSTR b) { return QString::compare(qs(a), qs(b), Qt::CaseInsensitive); }
int StrCmpW(LPCWSTR a, LPCWSTR b) { return std::wcscmp(a ? a : L"", b ? b : L""); }
int StrCmpNIW(LPCWSTR a, LPCWSTR b, int n) { return wcsncasecmp(a ? a : L"", b ? b : L"", static_cast<size_t>(n)); }
int StrCmpNW(LPCWSTR a, LPCWSTR b, int n) { return std::wcsncmp(a ? a : L"", b ? b : L"", static_cast<size_t>(n)); }

int StrCmpLogicalW(LPCWSTR a, LPCWSTR b)
{
	// numbers by their value, like the Explorer of Windows
	const QString sa = qs(a);
	const QString sb = qs(b);
	int i = 0;
	int j = 0;
	while (i < sa.size() && j < sb.size())
	{
		if (sa[i].isDigit() && sb[j].isDigit())
		{
			int ei = i;
			int ej = j;
			while (ei < sa.size() && sa[ei].isDigit())
				++ei;
			while (ej < sb.size() && sb[ej].isDigit())
				++ej;
			const qulonglong na = sa.mid(i, ei - i).toULongLong();
			const qulonglong nb = sb.mid(j, ej - j).toULongLong();
			if (na != nb)
				return na < nb ? -1 : 1;
			i = ei;
			j = ej;
			continue;
		}
		const int c = QString::compare(sa.mid(i, 1), sb.mid(j, 1), Qt::CaseInsensitive);
		if (c)
			return c < 0 ? -1 : 1;
		++i;
		++j;
	}
	return (sa.size() - i) < (sb.size() - j) ? -1 : ((sa.size() - i) > (sb.size() - j) ? 1 : 0);
}

LPWSTR StrChrW(LPCWSTR s, WCHAR c) { return s ? const_cast<LPWSTR>(std::wcschr(s, c)) : nullptr; }
LPWSTR StrChrIW(LPCWSTR s, WCHAR c)
{
	for (const wchar_t* p = s; p && *p; ++p)
	{
		if (std::towlower(*p) == std::towlower(c))
			return const_cast<LPWSTR>(p);
	}
	return nullptr;
}
LPWSTR StrRChrW(LPCWSTR s, LPCWSTR end, WCHAR c)
{
	if (!s)
		return nullptr;
	const wchar_t* last = nullptr;
	for (const wchar_t* p = s; *p && (!end || p < end); ++p)
	{
		if (*p == c)
			last = p;
	}
	return const_cast<LPWSTR>(last);
}
LPWSTR StrCpyW(LPWSTR dst, LPCWSTR src) { return std::wcscpy(dst, src ? src : L""); }
LPWSTR StrCpyNW(LPWSTR dst, LPCWSTR src, int max) { return lstrcpynW(dst, src, max); }
LPWSTR StrCatW(LPWSTR dst, LPCWSTR src) { return std::wcscat(dst, src ? src : L""); }
LPWSTR StrCatBuffW(LPWSTR dst, LPCWSTR src, int max)
{
	wcscat_s(dst, static_cast<size_t>(max), src);
	return dst;
}
LPWSTR StrDupW(LPCWSTR s)
{
	const size_t n = s ? std::wcslen(s) : 0;
	auto* out = static_cast<wchar_t*>(LocalAlloc(0, (n + 1) * sizeof(wchar_t)));
	if (s)
		std::wmemcpy(out, s, n);
	out[n] = 0;
	return out;
}
int StrToIntW(LPCWSTR s) { return s ? static_cast<int>(std::wcstol(s, nullptr, 10)) : 0; }
BOOL StrToIntExW(LPCWSTR s, DWORD flags, int* out)
{
	if (!s || !out)
		return FALSE;
	QString t = qs(s).trimmed();
	bool ok = false;
	if ((flags & STIF_SUPPORT_HEX) && t.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive))
		*out = static_cast<int>(t.mid(2).toUInt(&ok, 16));
	else
		*out = t.toInt(&ok);
	return ok ? TRUE : FALSE;
}
BOOL StrTrimW(LPWSTR s, LPCWSTR chars)
{
	if (!s || !chars)
		return FALSE;
	const QString set = qs(chars);
	QString t = qs(s);
	const int before = static_cast<int>(t.size());
	while (!t.isEmpty() && set.contains(t[0]))
		t.remove(0, 1);
	while (!t.isEmpty() && set.contains(t[t.size() - 1]))
		t.chop(1);
	copyOut(t, s, before + 1);
	return before != t.size() ? TRUE : FALSE;
}
LPWSTR StrFormatByteSizeW(LONGLONG size, LPWSTR buffer, UINT max)
{
	copyOut(QLocale().formattedDataSize(size, 1, QLocale::DataSizeTraditionalFormat), buffer, static_cast<int>(max));
	return buffer;
}
LPWSTR StrPBrkW(LPCWSTR s, LPCWSTR set) { return (s && set) ? const_cast<LPWSTR>(std::wcspbrk(s, set)) : nullptr; }
int StrSpnW(LPCWSTR s, LPCWSTR set) { return (s && set) ? static_cast<int>(std::wcsspn(s, set)) : 0; }
int StrCSpnW(LPCWSTR s, LPCWSTR set) { return (s && set) ? static_cast<int>(std::wcscspn(s, set)) : 0; }

HRESULT UrlEscapeW(LPCWSTR url, LPWSTR out, LPDWORD outLen, DWORD)
{
	const QString e = QString::fromUtf8(QUrl::toPercentEncoding(qs(url), "/:?=&#"));
	const DWORD n = static_cast<DWORD>(ws(e).size());
	if (!out || !outLen || *outLen <= n)
	{
		if (outLen)
			*outLen = n + 1;
		return E_POINTER;
	}
	copyOut(e, out, static_cast<int>(*outLen));
	*outLen = n;
	return S_OK;
}

HRESULT UrlUnescapeW(LPWSTR url, LPWSTR out, LPDWORD outLen, DWORD flags)
{
	const QString d = QUrl::fromPercentEncoding(qs(url).toUtf8());
	if (flags & URL_UNESCAPE_INPLACE)
	{
		copyOut(d, url, static_cast<int>(std::wcslen(url)) + 1);
		return S_OK;
	}
	const DWORD n = static_cast<DWORD>(ws(d).size());
	if (!out || !outLen || *outLen <= n)
	{
		if (outLen)
			*outLen = n + 1;
		return E_POINTER;
	}
	copyOut(d, out, static_cast<int>(*outLen));
	*outLen = n;
	return S_OK;
}

HRESULT UrlCreateFromPathW(LPCWSTR path, LPWSTR url, LPDWORD urlLen, DWORD)
{
	const QString u = QUrl::fromLocalFile(nativePath(path)).toString();
	if (!url || !urlLen)
		return E_POINTER;
	*urlLen = static_cast<DWORD>(copyOut(u, url, static_cast<int>(*urlLen)));
	return S_OK;
}

HRESULT PathCreateFromUrlW(LPCWSTR url, LPWSTR path, LPDWORD pathLen, DWORD)
{
	const QString p = QUrl(qs(url)).toLocalFile();
	if (!path || !pathLen)
		return E_POINTER;
	*pathLen = static_cast<DWORD>(copyOut(p, path, static_cast<int>(*pathLen)));
	return S_OK;
}

HRESULT SHLoadIndirectString(LPCWSTR source, LPWSTR out, UINT max, void**)
{
	copyOut(qs(source), out, static_cast<int>(max));
	return S_OK;
}

// ---- ShellExecute ----------------------------------------------------------------------------
namespace
{
	bool shellOpen(const QString& operation, const QString& fileIn, const QString& params, const QString& dir)
	{
		const QString file = unq(fileIn);
		const QUrl url(file);
		if (!url.scheme().isEmpty() && url.scheme().size() > 1 && !QFileInfo::exists(nativePath(file)))
			return QDesktopServices::openUrl(url);
		const QString path = nativePath(file);
		const QFileInfo fi(path);
		if (operation.compare(QStringLiteral("explore"), Qt::CaseInsensitive) == 0 || fi.isDir())
			return QDesktopServices::openUrl(QUrl::fromLocalFile(fi.isDir() ? path : fi.absolutePath()));
		if (fi.exists() && fi.isExecutable() && !fi.isDir())
		{
			QProcess p;
			p.setProgram(path);
			p.setArguments(QProcess::splitCommand(params));
			if (!dir.isEmpty())
				p.setWorkingDirectory(nativePath(dir));
			return p.startDetached();
		}
		if (fi.exists())
			return QDesktopServices::openUrl(QUrl::fromLocalFile(fi.absoluteFilePath()));
		// a program of the PATH
		const QString exe = QStandardPaths::findExecutable(file);
		if (!exe.isEmpty())
		{
			QProcess p;
			p.setProgram(exe);
			p.setArguments(QProcess::splitCommand(params));
			if (!dir.isEmpty())
				p.setWorkingDirectory(nativePath(dir));
			return p.startDetached();
		}
		return false;
	}
}

HINSTANCE ShellExecuteW(HWND, LPCWSTR operation, LPCWSTR file, LPCWSTR parameters, LPCWSTR directory, INT)
{
	const bool ok = shellOpen(qs(operation), qs(file), qs(parameters), qs(directory));
	return reinterpret_cast<HINSTANCE>(static_cast<intptr_t>(ok ? 42 : SE_ERR_FNF));
}

HINSTANCE ShellExecuteA(HWND, LPCSTR operation, LPCSTR file, LPCSTR parameters, LPCSTR directory, INT)
{
	const bool ok = shellOpen(qsA(operation), qsA(file), qsA(parameters), qsA(directory));
	return reinterpret_cast<HINSTANCE>(static_cast<intptr_t>(ok ? 42 : SE_ERR_FNF));
}

BOOL ShellExecuteExW(SHELLEXECUTEINFOW* info)
{
	if (!info)
		return FALSE;
	const bool ok = shellOpen(qs(info->lpVerb), qs(info->lpFile), qs(info->lpParameters), qs(info->lpDirectory));
	info->hInstApp = reinterpret_cast<HINSTANCE>(static_cast<intptr_t>(ok ? 42 : SE_ERR_FNF));
	info->hProcess = nullptr;
	return ok ? TRUE : FALSE;
}

namespace
{
	// the "system image list" of Windows: one list (small, large) of the icons of the file types, filled when asked
	struct SystemImages
	{
		HIMAGELIST list = nullptr;
		std::map<QString, int> indexes; // key of the icon -> its index
	};

	SystemImages& systemImages(bool small)
	{
		static SystemImages lists[2];
		SystemImages& s = lists[small ? 1 : 0];
		if (!s.list)
		{
			const int size = small ? 16 : 32;
			s.list = ImageList_Create(size, size, ILC_COLOR32 | ILC_MASK, 0, 64);
		}
		return s;
	}

	QIcon fileIcon(const QFileInfo& fi, bool useAttributes, bool open, QString* key)
	{
		static QFileIconProvider provider;
		if (fi.isRoot())
		{
			*key = QStringLiteral("drive");
			return provider.icon(QFileIconProvider::Drive);
		}
		if (fi.isDir() || (useAttributes && !fi.exists() && fi.suffix().isEmpty() && fi.fileName().isEmpty()))
		{
			*key = open ? QStringLiteral("folder-open") : QStringLiteral("folder");
			const QIcon theme = QIcon::fromTheme(*key);
			return !theme.isNull() ? theme : provider.icon(QFileIconProvider::Folder);
		}
		static QMimeDatabase mimes;
		const QMimeType mime = useAttributes && !fi.exists() ? mimes.mimeTypeForFile(fi.fileName(), QMimeDatabase::MatchExtension) : mimes.mimeTypeForFile(fi);
		*key = mime.name();
		QIcon icon = QIcon::fromTheme(mime.iconName());
		if (icon.isNull())
			icon = QIcon::fromTheme(mime.genericIconName());
		if (icon.isNull())
			icon = provider.icon(QFileIconProvider::File);
		return icon;
	}
}

DWORD_PTR SHGetFileInfoW(LPCWSTR path, DWORD, SHFILEINFOW* info, UINT, UINT flags)
{
	if (!info)
		return 0;
	*info = SHFILEINFOW{};
	const QString p = nativePath(path);
	const QFileInfo fi(p.isEmpty() ? QStringLiteral("/") : p);
	const bool useAttributes = (flags & SHGFI_USEFILEATTRIBUTES) != 0;
	static QFileIconProvider provider;
	if (flags & (SHGFI_ICON | SHGFI_SYSICONINDEX))
	{
		QString key;
		const QIcon icon = fileIcon(fi, useAttributes, (flags & SHGFI_OPENICON) != 0, &key);
		if (flags & SHGFI_ICON)
			info->hIcon = iconHandle(icon);
		if (flags & SHGFI_SYSICONINDEX)
		{
			SystemImages& images = systemImages((flags & SHGFI_SMALLICON) != 0);
			auto it = images.indexes.find(key);
			if (it == images.indexes.end())
			{
				HICON h = iconHandle(icon);
				it = images.indexes.emplace(key, ImageList_AddIcon(images.list, h)).first;
				DestroyIcon(h);
			}
			info->iIcon = it->second; // no overlay (SHGFI_OVERLAYINDEX: the bits 24..31 stay 0)
		}
	}
	if (flags & SHGFI_DISPLAYNAME)
		copyOut(fi.fileName().isEmpty() ? p : fi.fileName(), info->szDisplayName, MAX_PATH);
	if (flags & SHGFI_TYPENAME)
		copyOut(fi.isDir() ? QStringLiteral("File folder") : QMimeDatabase().mimeTypeForFile(fi).comment(), info->szTypeName, 80);
	if (flags & SHGFI_ATTRIBUTES)
		info->dwAttributes = fi.isDir() ? 0x20000000 : 0; // SFGAO_FOLDER
	// SHGFI_SYSICONINDEX returns the image list (not to be destroyed)
	if (flags & SHGFI_SYSICONINDEX)
		return reinterpret_cast<DWORD_PTR>(systemImages((flags & SHGFI_SMALLICON) != 0).list);
	return 1;
}

namespace
{
	bool copyRecursively(const QString& from, const QString& to)
	{
		const QFileInfo fi(from);
		if (fi.isSymLink())
			return QFile::link(fi.symLinkTarget(), to);
		if (!fi.isDir())
			return QFile::copy(from, to);
		// a folder into itself: never
		if (QDir::cleanPath(to).startsWith(QDir::cleanPath(from) + QLatin1Char('/')))
			return false;
		if (!QDir().mkpath(to))
			return false;
		bool ok = true;
		for (const QFileInfo& child : QDir(from).entryInfoList(QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot))
			ok = copyRecursively(child.filePath(), to + QLatin1Char('/') + child.fileName()) && ok;
		return ok;
	}

	// "name (2).ext": a name that is not used
	QString uniqueName(const QString& path)
	{
		const QFileInfo fi(path);
		const QString dir = fi.path();
		const bool folder = fi.isDir();
		const QString base = folder ? fi.fileName() : fi.completeBaseName();
		const QString ext = folder || fi.suffix().isEmpty() ? QString() : QLatin1Char('.') + fi.suffix();
		for (int n = 2;; ++n)
		{
			const QString candidate = QStringLiteral("%1/%2 (%3)%4").arg(dir, base).arg(n).arg(ext);
			if (!QFileInfo::exists(candidate))
				return candidate;
		}
	}

	bool removePath(const QString& path)
	{
		const QFileInfo fi(path);
		return (fi.isDir() && !fi.isSymLink()) ? QDir(path).removeRecursively() : QFile::remove(path);
	}
}

int SHFileOperationW(SHFILEOPSTRUCTW* op)
{
	if (!op || !op->pFrom || !*op->pFrom)
		return 1;
	if (op->wFunc == FO_DELETE && !(op->fFlags & FOF_NOCONFIRMATION))
	{
		const bool trash = op->fFlags & FOF_ALLOWUNDO;
		const QString question = trash ? QStringLiteral("Move the selected items to the trash?")
			: QStringLiteral("Permanently delete the selected items?");
		if (QMessageBox::question(widgetOf(op->hwnd), QStringLiteral("Delete"), question,
			QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes)
		{
			op->fAnyOperationsAborted = TRUE;
			return 1;
		}
	}
	int failed = 0;
	// pTo: one folder for all, or one name per file (FOF_MULTIDESTFILES)
	QStringList targets;
	if (op->fFlags & FOF_MULTIDESTFILES)
	{
		for (const wchar_t* t = op->pTo; t && *t; t += std::wcslen(t) + 1)
			targets << nativePath(t);
	}
	else if (op->pTo && *op->pTo)
		targets << nativePath(op->pTo);
	int index = 0;
	for (const wchar_t* f = op->pFrom; *f; f += std::wcslen(f) + 1, ++index)
	{
		const QString from = nativePath(f);
		if (from.isEmpty() || QDir(from).isRoot())
		{
			++failed;
			continue;
		}
		switch (op->wFunc)
		{
			case FO_DELETE:
				if (op->fFlags & FOF_ALLOWUNDO)
					failed += QFile::moveToTrash(from) ? 0 : 1;
				else
					failed += removePath(from) ? 0 : 1;
				break;
			case FO_COPY:
			case FO_MOVE:
			case FO_RENAME:
			{
				QString to = targets.value(targets.size() > 1 ? index : 0);
				if (to.isEmpty())
				{
					++failed;
					break;
				}
				if (op->wFunc != FO_RENAME && QFileInfo(to).isDir() && targets.size() <= 1)
					to += QLatin1Char('/') + QFileInfo(from).fileName();
				if (QDir::cleanPath(to) == QDir::cleanPath(from))
				{
					// copied onto itself: a copy beside it (Windows: "- Copy"), moved onto itself: nothing to do
					if (op->wFunc != FO_COPY)
						break;
					to = uniqueName(to);
				}
				else if (QFileInfo::exists(to) || QFileInfo(to).isSymLink())
				{
					if (op->fFlags & FOF_RENAMEONCOLLISION)
						to = uniqueName(to);
					else
					{
						const bool replace = (op->fFlags & FOF_NOCONFIRMATION)
							|| QMessageBox::question(widgetOf(op->hwnd), QStringLiteral("Replace"),
								QStringLiteral("\"%1\" exists already. Replace it?").arg(to)) == QMessageBox::Yes;
						if (!replace || !removePath(to))
						{
							++failed;
							break;
						}
					}
				}
				if (op->wFunc == FO_COPY)
					failed += copyRecursively(from, to) ? 0 : 1;
				else if (QFileInfo(from).isDir() && !QFileInfo(from).isSymLink())
				{
					// folders: renamed, or copied then removed (other file system)
					failed += (QDir().rename(from, to) || (copyRecursively(from, to) && removePath(from))) ? 0 : 1;
				}
				else
					failed += QFile::rename(from, to) ? 0 : 1;
				break;
			}
			default:
				break;
		}
	}
	if (failed)
		op->fAnyOperationsAborted = TRUE;
	return failed ? 1 : 0;
}

LPWSTR* CommandLineToArgvW(LPCWSTR cmdLine, int* argc)
{
	const QStringList args = QProcess::splitCommand(qs(cmdLine));
	size_t total = 0;
	for (const QString& a : args)
		total += ws(a).size() + 1;
	auto* block = static_cast<unsigned char*>(LocalAlloc(LMEM_ZEROINIT, (args.size() + 1) * sizeof(wchar_t*) + total * sizeof(wchar_t)));
	auto** argv = reinterpret_cast<wchar_t**>(block);
	auto* text = reinterpret_cast<wchar_t*>(block + (args.size() + 1) * sizeof(wchar_t*));
	for (int i = 0; i < args.size(); ++i)
	{
		const std::wstring w = ws(args[i]);
		argv[i] = text;
		std::wmemcpy(text, w.c_str(), w.size() + 1);
		text += w.size() + 1;
	}
	if (argc)
		*argc = static_cast<int>(args.size());
	return argv;
}

// ---- special folders ----------------------------------------------------------------------------
namespace
{
	QString folderOf(int csidl)
	{
		switch (csidl & 0xFF)
		{
			case CSIDL_APPDATA: return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
			case CSIDL_LOCAL_APPDATA: return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
			case CSIDL_COMMON_APPDATA: return QStringLiteral("/usr/share");
			case CSIDL_PERSONAL: return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
			case CSIDL_DESKTOP: case CSIDL_DESKTOPDIRECTORY: return QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
			case CSIDL_MYPICTURES: return QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
			case CSIDL_PROFILE: return QDir::homePath();
			case CSIDL_FONTS: return QStandardPaths::writableLocation(QStandardPaths::FontsLocation);
			case CSIDL_PROGRAM_FILES: case CSIDL_PROGRAM_FILESX86: return QStringLiteral("/usr");
			case CSIDL_WINDOWS: return QStringLiteral("/usr");
			case CSIDL_SYSTEM: return QStringLiteral("/usr/bin");
			case CSIDL_COMMON_DOCUMENTS: return QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
			default: return QDir::homePath();
		}
	}
}

HRESULT SHGetFolderLocation(HWND hWnd, int csidl, HANDLE token, DWORD, LPITEMIDLIST* pidl)
{
	if (!pidl)
		return E_INVALIDARG;
	*pidl = nullptr;
	wchar_t path[MAX_PATH] = {};
	const HRESULT hr = SHGetFolderPathW(hWnd, csidl, token, 0, path);
	if (FAILED(hr))
		return hr;
	return SHILCreateFromPath(path, pidl, nullptr);
}

HRESULT SHGetMalloc(LPMALLOC* malloc)
{
	return CoGetMalloc(1, malloc);
}

HRESULT SHGetFolderPathW(HWND, int csidl, HANDLE, DWORD, LPWSTR path)
{
	const QString f = folderOf(csidl);
	if (csidl & CSIDL_FLAG_CREATE)
		QDir().mkpath(f);
	copyOut(f, path, MAX_PATH);
	return S_OK;
}

BOOL SHGetSpecialFolderPathW(HWND, LPWSTR path, int csidl, BOOL create)
{
	const QString f = folderOf(csidl);
	if (create)
		QDir().mkpath(f);
	copyOut(f, path, MAX_PATH);
	return TRUE;
}

const GUID FOLDERID_RoamingAppData = {0x3EB685DB, 0x65F9, 0x4CF6, {0xA0, 0x3A, 0xE3, 0xEF, 0x65, 0x72, 0x9F, 0x3D}};
const GUID FOLDERID_LocalAppData = {0xF1B32785, 0x6FBA, 0x4FCF, {0x9D, 0x55, 0x7B, 0x8E, 0x7F, 0x15, 0x70, 0x91}};
const GUID FOLDERID_Documents = {0xFDD39AD0, 0x238F, 0x46AF, {0xAD, 0xB4, 0x6C, 0x85, 0x48, 0x03, 0x69, 0xC7}};
const GUID FOLDERID_Desktop = {0xB4BFCC3A, 0xDB2C, 0x424C, {0xB0, 0x29, 0x7F, 0xE9, 0x9A, 0x87, 0xC6, 0x41}};
const GUID FOLDERID_ProgramFiles = {0x905E63B6, 0xC1BF, 0x494E, {0xB2, 0x9C, 0x65, 0xB7, 0x32, 0xD3, 0xD2, 0x1A}};
const GUID FOLDERID_Profile = {0x5E6C858F, 0x0E22, 0x4760, {0x9A, 0xFE, 0xEA, 0x33, 0x17, 0xB6, 0x71, 0x73}};
const GUID FOLDERID_Downloads = {0x374DE290, 0x123F, 0x4565, {0x91, 0x64, 0x39, 0xC4, 0x92, 0x5E, 0x46, 0x7B}};

HRESULT SHGetKnownFolderPath(REFKNOWNFOLDERID id, DWORD, HANDLE, PWSTR* path)
{
	if (!path)
		return E_POINTER;
	QString f = QDir::homePath();
	auto same = [&](const GUID& g) { return std::memcmp(&id, &g, sizeof(GUID)) == 0; };
	if (same(FOLDERID_RoamingAppData))
		f = folderOf(CSIDL_APPDATA);
	else if (same(FOLDERID_LocalAppData))
		f = folderOf(CSIDL_LOCAL_APPDATA);
	else if (same(FOLDERID_Documents))
		f = folderOf(CSIDL_PERSONAL);
	else if (same(FOLDERID_Desktop))
		f = folderOf(CSIDL_DESKTOP);
	else if (same(FOLDERID_ProgramFiles))
		f = folderOf(CSIDL_PROGRAM_FILES);
	else if (same(FOLDERID_Downloads))
		f = QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
	const std::wstring w = ws(f);
	*path = static_cast<PWSTR>(CoTaskMemAlloc((w.size() + 1) * sizeof(wchar_t)));
	std::wmemcpy(*path, w.c_str(), w.size() + 1);
	return S_OK;
}

int SHCreateDirectoryExW(HWND, LPCWSTR path, const SECURITY_ATTRIBUTES*)
{
	const QString p = nativePath(path);
	if (QFileInfo(p).isDir())
		return ERROR_ALREADY_EXISTS;
	return QDir().mkpath(p) ? ERROR_SUCCESS : ERROR_PATH_NOT_FOUND;
}

int SHCreateDirectory(HWND hWnd, LPCWSTR path)
{
	return SHCreateDirectoryExW(hWnd, path, nullptr);
}

void SHChangeNotify(LONG, UINT, LPCVOID, LPCVOID)
{
}

// PIDLs here are malloc'ed wchar_t paths
LPITEMIDLIST SHBrowseForFolderW(LPBROWSEINFOW bi)
{
	QWidget* parent = bi ? widgetOf(bi->hwndOwner) : nullptr;
	if (!parent)
		parent = widgetOf(nppHandle());
	const QString title = bi ? qs(bi->lpszTitle) : QString();
	QString start = QDir::homePath();
	const QString dir = QFileDialog::getExistingDirectory(parent, title.isEmpty() ? QStringLiteral("Browse For Folder") : title, start);
	if (dir.isEmpty())
		return nullptr;
	if (bi && bi->pszDisplayName)
		copyOut(QFileInfo(dir).fileName(), bi->pszDisplayName, MAX_PATH);
	const std::wstring w = ws(dir);
	auto* p = static_cast<wchar_t*>(CoTaskMemAlloc((w.size() + 1) * sizeof(wchar_t)));
	std::wmemcpy(p, w.c_str(), w.size() + 1);
	return reinterpret_cast<LPITEMIDLIST>(p);
}

BOOL SHGetPathFromIDListW(LPCITEMIDLIST pidl, LPWSTR path)
{
	if (!pidl || !path)
		return FALSE;
	copyOut(qs(reinterpret_cast<const wchar_t*>(pidl)), path, MAX_PATH);
	return TRUE;
}

void ILFree(LPITEMIDLIST pidl)
{
	CoTaskMemFree(pidl);
}

HRESULT SHILCreateFromPath(PCWSTR path, LPITEMIDLIST* pidl, DWORD*)
{
	if (!pidl)
		return E_POINTER;
	const std::wstring w = ws(nativePath(path));
	auto* p = static_cast<wchar_t*>(CoTaskMemAlloc((w.size() + 1) * sizeof(wchar_t)));
	std::wmemcpy(p, w.c_str(), w.size() + 1);
	*pidl = reinterpret_cast<LPITEMIDLIST>(p);
	return S_OK;
}

HRESULT SHParseDisplayName(PCWSTR name, void*, LPITEMIDLIST* pidl, ULONG, ULONG*)
{
	return SHILCreateFromPath(name, pidl, nullptr);
}

HRESULT SHOpenFolderAndSelectItems(LPCITEMIDLIST folder, UINT, LPCITEMIDLIST*, DWORD)
{
	if (!folder)
		return E_INVALIDARG;
	const QString p = qs(reinterpret_cast<const wchar_t*>(folder));
	QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(p).isDir() ? p : QFileInfo(p).absolutePath()));
	return S_OK;
}

// ---- common dialogs --------------------------------------------------------------------------------
namespace
{
	QString filterOf(LPCWSTR filter, DWORD index, QString* selected)
	{
		// "Text files\0*.txt;*.log\0All\0*.*\0\0" -> "Text files (*.txt *.log);;All (*)"
		QStringList parts;
		int n = 0;
		for (const wchar_t* p = filter; p && *p;)
		{
			const QString name = qs(p);
			p += std::wcslen(p) + 1;
			if (!*p)
				break;
			QString pattern = qs(p).replace(QLatin1Char(';'), QLatin1Char(' '));
			p += std::wcslen(p) + 1;
			pattern.replace(QStringLiteral("*.*"), QStringLiteral("*"));
			const QString entry = name.contains(QLatin1Char('(')) ? QStringLiteral("%1 (%2)").arg(name.section(QLatin1Char('('), 0, 0).trimmed(), pattern)
				: QStringLiteral("%1 (%2)").arg(name, pattern);
			parts << entry;
			if (++n == static_cast<int>(index) && selected)
				*selected = entry;
		}
		return parts.join(QStringLiteral(";;"));
	}

	BOOL fileDialog(LPOPENFILENAMEW ofn, bool save)
	{
		if (!ofn || !ofn->lpstrFile)
			return FALSE;
		QWidget* parent = widgetOf(ofn->hwndOwner);
		if (!parent)
			parent = widgetOf(nppHandle());
		QString selectedFilter;
		const QString filter = filterOf(ofn->lpstrFilter, ofn->nFilterIndex ? ofn->nFilterIndex : 1, &selectedFilter);
		QString start = ofn->lpstrInitialDir ? nativePath(ofn->lpstrInitialDir) : QString();
		if (ofn->lpstrFile[0])
		{
			const QString initial = nativePath(ofn->lpstrFile);
			start = start.isEmpty() || QFileInfo(initial).isAbsolute() ? initial : start + QLatin1Char('/') + initial;
		}
		const QString title = qs(ofn->lpstrTitle);
		QFileDialog::Options options;
		if (!(ofn->Flags & OFN_OVERWRITEPROMPT) && save)
			options |= QFileDialog::DontConfirmOverwrite;
		QStringList files;
		if (save)
		{
			QString f = QFileDialog::getSaveFileName(parent, title.isEmpty() ? QStringLiteral("Save As") : title, start, filter, &selectedFilter, options);
			if (!f.isEmpty())
			{
				if (ofn->lpstrDefExt && *ofn->lpstrDefExt && QFileInfo(f).suffix().isEmpty())
					f += QLatin1Char('.') + qs(ofn->lpstrDefExt);
				files << f;
			}
		}
		else if (ofn->Flags & OFN_ALLOWMULTISELECT)
			files = QFileDialog::getOpenFileNames(parent, title.isEmpty() ? QStringLiteral("Open") : title, start, filter, &selectedFilter, options);
		else
		{
			const QString f = QFileDialog::getOpenFileName(parent, title.isEmpty() ? QStringLiteral("Open") : title, start, filter, &selectedFilter, options);
			if (!f.isEmpty())
				files << f;
		}
		if (files.isEmpty())
			return FALSE;
		const QStringList filters = filter.split(QStringLiteral(";;"));
		ofn->nFilterIndex = static_cast<DWORD>(std::max<qsizetype>(0, filters.indexOf(selectedFilter)) + 1);
		if (files.size() == 1)
		{
			copyOut(files.first(), ofn->lpstrFile, static_cast<int>(ofn->nMaxFile));
			const std::wstring w = ws(files.first());
			const size_t slash = w.find_last_of(L'/');
			ofn->nFileOffset = static_cast<WORD>(slash == std::wstring::npos ? 0 : slash + 1);
			const size_t dot = w.find_last_of(L'.');
			ofn->nFileExtension = static_cast<WORD>((dot == std::wstring::npos || dot < ofn->nFileOffset) ? w.size() : dot + 1);
			if (ofn->lpstrFileTitle)
				copyOut(QFileInfo(files.first()).fileName(), ofn->lpstrFileTitle, static_cast<int>(ofn->nMaxFileTitle));
			return TRUE;
		}
		// "folder\0file1\0file2\0\0"
		std::wstring out = ws(QFileInfo(files.first()).absolutePath());
		out.push_back(L'\0');
		for (const QString& f : files)
		{
			out += ws(QFileInfo(f).fileName());
			out.push_back(L'\0');
		}
		out.push_back(L'\0');
		if (out.size() > ofn->nMaxFile)
			return FALSE;
		std::wmemcpy(ofn->lpstrFile, out.data(), out.size());
		ofn->nFileOffset = static_cast<WORD>(ws(QFileInfo(files.first()).absolutePath()).size() + 1);
		return TRUE;
	}
}

BOOL GetOpenFileNameW(LPOPENFILENAMEW ofn)
{
	return fileDialog(ofn, false);
}

BOOL GetSaveFileNameW(LPOPENFILENAMEW ofn)
{
	return fileDialog(ofn, true);
}

DWORD CommDlgExtendedError()
{
	return 0;
}

BOOL ChooseColorW(LPCHOOSECOLORW cc)
{
	if (!cc)
		return FALSE;
	QWidget* parent = widgetOf(cc->hwndOwner);
	if (!parent)
		parent = widgetOf(nppHandle());
	if (cc->lpCustColors)
	{
		for (int i = 0; i < 16 && i < QColorDialog::customCount(); ++i)
			QColorDialog::setCustomColor(i, color(cc->lpCustColors[i]));
	}
	const QColor c = QColorDialog::getColor((cc->Flags & CC_RGBINIT) ? color(cc->rgbResult) : QColor(Qt::white), parent);
	if (cc->lpCustColors)
	{
		for (int i = 0; i < 16 && i < QColorDialog::customCount(); ++i)
			cc->lpCustColors[i] = colorRef(QColorDialog::customColor(i));
	}
	if (!c.isValid())
		return FALSE;
	cc->rgbResult = colorRef(c);
	return TRUE;
}

BOOL ChooseFontW(LPCHOOSEFONTW cf)
{
	if (!cf || !cf->lpLogFont)
		return FALSE;
	QWidget* parent = widgetOf(cf->hwndOwner);
	if (!parent)
		parent = widgetOf(nppHandle());
	QFont initial = QApplication::font();
	if (cf->Flags & CF_INITTOLOGFONTSTRUCT)
	{
		HFONT h = CreateFontIndirectW(cf->lpLogFont);
		initial = fontOf(h);
		DeleteObject(h);
	}
	bool ok = false;
	QFontDialog::FontDialogOptions options;
	if (cf->Flags & CF_FIXEDPITCHONLY)
		options |= QFontDialog::MonospacedFonts;
	const QFont f = QFontDialog::getFont(&ok, initial, parent, QString(), options);
	if (!ok)
		return FALSE;
	LOGFONTW& lf = *cf->lpLogFont;
	lf = LOGFONTW{};
	const int px = f.pixelSize() > 0 ? f.pixelSize() : static_cast<int>(f.pointSizeF() * 96.0 / 72.0 + 0.5);
	lf.lfHeight = -px;
	lf.lfWeight = f.weight();
	lf.lfItalic = f.italic();
	lf.lfUnderline = f.underline();
	lf.lfStrikeOut = f.strikeOut();
	copyOut(f.family(), lf.lfFaceName, LF_FACESIZE);
	cf->iPointSize = static_cast<INT>(f.pointSizeF() * 10);
	return TRUE;
}
