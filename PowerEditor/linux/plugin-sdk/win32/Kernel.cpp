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

// Notepad++ Linux plugin SDK, npp_win32 library: kernel32 and friends on POSIX and
// Qt (memory, strings, files, INI files, time, threads, synchronization, processes,
// environment, modules, registry in QSettings), the clipboard and system metrics.

#include <atomic>
#include <csignal>
#include <chrono>
#include <condition_variable>
#include <map>
#include <mutex>
#include <shared_mutex>
#include <thread>

#include <QApplication>
#include <QClipboard>
#include <QCollator>
#include <QDateTime>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QLocale>
#include <QStorageInfo>
#include <QMimeData>
#include <QUrl>
#include <QRegularExpression>
#include <QScreen>
#include <QSettings>
#include <QStandardPaths>
#include <QStringDecoder>
#include <QStringEncoder>
#include <QStyle>
#include <QTextStream>
#include <QUuid>

#include <dirent.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <link.h>
#include <signal.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/wait.h>
#include <unistd.h>

#include "W32Internal.h"
#include <objidl.h>

namespace w32
{
	QString qs(const wchar_t* s, int len)
	{
		if (!s)
			return QString();
		if (len < 0)
			return QString::fromWCharArray(s);
		return QString::fromWCharArray(s, len);
	}

	QString qsA(const char* s, int len)
	{
		if (!s)
			return QString();
		return QString::fromUtf8(s, len < 0 ? -1 : len);
	}

	std::wstring ws(const QString& s)
	{
		return s.toStdWString();
	}

	int copyOut(const QString& s, wchar_t* buf, int max)
	{
		if (!buf || max <= 0)
			return 0;
		const std::wstring w = s.toStdWString();
		const int n = std::min(static_cast<int>(w.size()), max - 1);
		std::wmemcpy(buf, w.data(), static_cast<size_t>(n));
		buf[n] = 0;
		return n;
	}

	int copyOutA(const QString& s, char* buf, int max)
	{
		if (!buf || max <= 0)
			return 0;
		const QByteArray u = s.toUtf8();
		const int n = std::min(static_cast<int>(u.size()), max - 1);
		std::memcpy(buf, u.constData(), static_cast<size_t>(n));
		buf[n] = 0;
		return n;
	}

	QString nativePath(const QString& windowsPath)
	{
		QString p = windowsPath;
		p.replace(QLatin1Char('\\'), QLatin1Char('/'));
		// "C:/x" of a path built like on Windows: the drive goes
		if (p.size() >= 2 && p[1] == QLatin1Char(':') && p[0].isLetter())
			p = p.mid(2);
		if (p.startsWith(QStringLiteral("//?/")))
			p = p.mid(3);
		return p;
	}

	QString nativePath(const wchar_t* windowsPath)
	{
		return nativePath(qs(windowsPath));
	}

	// ---- kernel objects ---------------------------------------------------------------------
	enum class KType { File, Pipe, Thread, Process, Event, Mutex, Semaphore, Find, Change, Module };

	struct KObject
	{
		unsigned int magic = 0x4E50504Bu; // "NPPK"
		KType type;
		int fd = -1;
		std::mutex m;
		std::condition_variable cv;
		bool signaled = false;
		bool manualReset = true;
		DWORD exitCode = STILL_ACTIVE;
		pid_t pid = 0;
		LONG count = 0;
		LONG maximum = 0;
		std::thread::id owner;
		int recursion = 0;
		// Find
		DIR* dir = nullptr;
		std::string dirPath;
		std::string pattern;
		// named events: the handles of CreateEvent/OpenEvent of a name are the same object
		std::wstring name;
		int refs = 1;
		explicit KObject(KType t) : type(t) {}
	};

	std::mutex& namedMutex()
	{
		static std::mutex m;
		return m;
	}

	std::map<std::wstring, KObject*>& namedObjects()
	{
		static std::map<std::wstring, KObject*> objects;
		return objects;
	}

	KObject* kobj(HANDLE h)
	{
		auto* k = static_cast<KObject*>(h);
		if (!k || h == INVALID_HANDLE_VALUE)
			return nullptr;
		return k->magic == 0x4E50504Bu ? k : nullptr;
	}

	thread_local DWORD g_lastError = 0;

	void setErrno()
	{
		switch (errno)
		{
			case ENOENT: g_lastError = ERROR_FILE_NOT_FOUND; break;
			case ENOTDIR: g_lastError = ERROR_PATH_NOT_FOUND; break;
			case EACCES: case EPERM: g_lastError = ERROR_ACCESS_DENIED; break;
			case EEXIST: g_lastError = ERROR_ALREADY_EXISTS; break;
			case ENOMEM: g_lastError = ERROR_NOT_ENOUGH_MEMORY; break;
			case EINVAL: g_lastError = ERROR_INVALID_PARAMETER; break;
			case EPIPE: g_lastError = ERROR_BROKEN_PIPE; break;
			default: g_lastError = static_cast<DWORD>(errno); break;
		}
	}

	std::string fsPath(const wchar_t* p)
	{
		return QFile::encodeName(nativePath(p)).toStdString();
	}

	// ---- modules ---------------------------------------------------------------------------
	namespace
	{
		void anchor() {}
	}

	QString moduleFile()
	{
		Dl_info info{};
		if (dladdr(reinterpret_cast<void*>(&anchor), &info) && info.dli_fname)
			return QFile::decodeName(info.dli_fname);
		return QString();
	}

	HINSTANCE moduleHandle()
	{
		static HMODULE module = []
		{
			const QString path = moduleFile();
			if (QFileInfo(path) == QFileInfo(QCoreApplication::applicationFilePath()))
				return dlopen(nullptr, RTLD_LAZY);
			return dlopen(QFile::encodeName(path).constData(), RTLD_LAZY | RTLD_NOLOAD);
		}();
		return module;
	}

	// ---- INI files --------------------------------------------------------------------------
	namespace
	{
		struct IniFile
		{
			struct Entry
			{
				QString key;
				QString value;
			};
			struct Section
			{
				QString name;
				std::vector<Entry> entries;
			};
			std::vector<Section> sections;
			QStringList header; // lines before the first section

			Section* find(const QString& name)
			{
				for (Section& s : sections)
				{
					if (s.name.compare(name, Qt::CaseInsensitive) == 0)
						return &s;
				}
				return nullptr;
			}
		};

		QString readText(const QString& path)
		{
			QFile f(path);
			if (!f.open(QIODevice::ReadOnly))
				return QString();
			const QByteArray data = f.readAll();
			if (data.startsWith("\xFF\xFE"))
				return QString::fromUtf16(reinterpret_cast<const char16_t*>(data.constData() + 2), (data.size() - 2) / 2);
			if (data.startsWith("\xFE\xFF"))
			{
				QStringDecoder dec(QStringDecoder::Utf16BE);
				return dec(data.mid(2));
			}
			if (data.startsWith("\xEF\xBB\xBF"))
				return QString::fromUtf8(data.mid(3));
			return QString::fromUtf8(data);
		}

		IniFile readIni(const QString& path)
		{
			IniFile ini;
			const QStringList lines = readText(path).split(QLatin1Char('\n'));
			IniFile::Section* cur = nullptr;
			for (QString line : lines)
			{
				if (line.endsWith(QLatin1Char('\r')))
					line.chop(1);
				const QString t = line.trimmed();
				if (t.startsWith(QLatin1Char('[')) && t.contains(QLatin1Char(']')))
				{
					ini.sections.push_back(IniFile::Section{t.mid(1, t.indexOf(QLatin1Char(']')) - 1).trimmed(), {}});
					cur = &ini.sections.back();
					continue;
				}
				if (t.isEmpty() || t.startsWith(QLatin1Char(';')))
				{
					if (!cur)
						ini.header << line;
					continue;
				}
				if (!cur)
					continue;
				const int eq = static_cast<int>(t.indexOf(QLatin1Char('=')));
				if (eq < 0)
					continue;
				cur->entries.push_back(IniFile::Entry{t.left(eq).trimmed(), t.mid(eq + 1).trimmed()});
			}
			return ini;
		}

		bool writeIni(const QString& path, const IniFile& ini)
		{
			QDir().mkpath(QFileInfo(path).absolutePath());
			QFile f(path);
			if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
				return false;
			QString out;
			for (const QString& h : ini.header)
				out += h + QLatin1Char('\n');
			for (const auto& s : ini.sections)
			{
				out += QLatin1Char('[') + s.name + QStringLiteral("]\n");
				for (const auto& e : s.entries)
					out += e.key + QLatin1Char('=') + e.value + QLatin1Char('\n');
			}
			f.write(out.toUtf8());
			return true;
		}

		// the settings folder of the host: $XDG_CONFIG_HOME/<application name> (~/.config/linuxpad++)
		QString hostConfigDir()
		{
			QString base = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
			if (base.isEmpty())
				base = QDir::homePath() + QStringLiteral("/.config");
			const QString app = QCoreApplication::applicationName();
			return base + QLatin1Char('/') + (app.isEmpty() ? QStringLiteral("linuxpad++") : app);
		}

		QString iniPath(const wchar_t* file)
		{
			QString p = nativePath(file);
			if (QFileInfo(p).isRelative())
				p = hostConfigDir() + QLatin1Char('/') + p;
			return p;
		}

		// "a\0b\0\0" lists of the profile functions
		DWORD copyList(const QStringList& items, wchar_t* out, DWORD max)
		{
			if (!out || max < 2)
				return 0;
			DWORD n = 0;
			for (const QString& s : items)
			{
				const std::wstring w = ws(s);
				if (n + w.size() + 2 > max)
				{
					out[n] = 0;
					out[n + 1 < max ? n + 1 : n] = 0;
					return max - 2;
				}
				std::wmemcpy(out + n, w.data(), w.size());
				n += static_cast<DWORD>(w.size());
				out[n++] = 0;
			}
			out[n] = 0;
			return n ? n : 0;
		}
	}

	// ---- registry: values in a QSettings file ---------------------------------------------
	namespace
	{
		struct RegKey
		{
			unsigned int magic = 0x4E505052u; // "NPPR"
			QString path;
		};

		QSettings& registry()
		{
			static QSettings s(hostConfigDir() + QStringLiteral("/plugins/config/registry.ini"), QSettings::IniFormat);
			return s;
		}

		QString rootName(HKEY key)
		{
			switch (reinterpret_cast<uintptr_t>(key))
			{
				case 0x80000000: return QStringLiteral("HKCR");
				case 0x80000001: return QStringLiteral("HKCU");
				case 0x80000002: return QStringLiteral("HKLM");
				case 0x80000003: return QStringLiteral("HKU");
				default: break;
			}
			auto* k = static_cast<RegKey*>(key);
			return (k && k->magic == 0x4E505052u) ? k->path : QString();
		}

		QString keyPath(HKEY key, LPCWSTR sub)
		{
			QString base = rootName(key);
			if (sub && *sub)
				base += QLatin1Char('/') + qs(sub).replace(QLatin1Char('\\'), QLatin1Char('/'));
			return base;
		}
	}
}

using namespace w32;

// ---- memory ------------------------------------------------------------------------------------
namespace
{
	struct GlobalBlock
	{
		unsigned int magic = 0x4E505047u; // "NPPG"
		size_t size = 0;
		alignas(16) unsigned char data[1];
	};

	GlobalBlock* blockOf(HGLOBAL h)
	{
		if (!h)
			return nullptr;
		auto* b = reinterpret_cast<GlobalBlock*>(static_cast<unsigned char*>(h) - offsetof(GlobalBlock, data));
		return b->magic == 0x4E505047u ? b : nullptr;
	}
}

HGLOBAL GlobalAlloc(UINT flags, SIZE_T bytes)
{
	auto* b = static_cast<GlobalBlock*>(std::malloc(offsetof(GlobalBlock, data) + bytes + sizeof(wchar_t)));
	if (!b)
		return nullptr;
	b->magic = 0x4E505047u;
	b->size = bytes;
	if (flags & GMEM_ZEROINIT)
		std::memset(b->data, 0, bytes + sizeof(wchar_t));
	else
		std::memset(b->data + bytes, 0, sizeof(wchar_t));
	return b->data;
}

HGLOBAL GlobalReAlloc(HGLOBAL mem, SIZE_T bytes, UINT flags)
{
	GlobalBlock* b = blockOf(mem);
	if (!b)
		return GlobalAlloc(flags, bytes);
	auto* n = static_cast<GlobalBlock*>(std::realloc(b, offsetof(GlobalBlock, data) + bytes + sizeof(wchar_t)));
	if (!n)
		return nullptr;
	if ((flags & GMEM_ZEROINIT) && bytes > n->size)
		std::memset(n->data + n->size, 0, bytes - n->size);
	n->size = bytes;
	return n->data;
}

LPVOID GlobalLock(HGLOBAL mem)
{
	return mem;
}

BOOL GlobalUnlock(HGLOBAL)
{
	return FALSE;
}

HGLOBAL GlobalFree(HGLOBAL mem)
{
	if (GlobalBlock* b = blockOf(mem))
	{
		b->magic = 0;
		std::free(b);
	}
	return nullptr;
}

SIZE_T GlobalSize(HGLOBAL mem)
{
	GlobalBlock* b = blockOf(mem);
	return b ? b->size : 0;
}

HLOCAL LocalAlloc(UINT flags, SIZE_T bytes)
{
	return GlobalAlloc(flags, bytes);
}

HLOCAL LocalReAlloc(HLOCAL mem, SIZE_T bytes, UINT flags)
{
	return GlobalReAlloc(mem, bytes, flags);
}

LPVOID LocalLock(HLOCAL mem)
{
	return mem;
}

BOOL LocalUnlock(HLOCAL)
{
	return FALSE;
}

HLOCAL LocalFree(HLOCAL mem)
{
	return GlobalFree(mem);
}

HANDLE GetProcessHeap()
{
	return reinterpret_cast<HANDLE>(1);
}

LPVOID HeapAlloc(HANDLE, DWORD flags, SIZE_T bytes)
{
	return GlobalAlloc((flags & HEAP_ZERO_MEMORY) ? GMEM_ZEROINIT : 0, bytes);
}

LPVOID HeapReAlloc(HANDLE, DWORD flags, LPVOID mem, SIZE_T bytes)
{
	return GlobalReAlloc(mem, bytes, (flags & HEAP_ZERO_MEMORY) ? GMEM_ZEROINIT : 0);
}

BOOL HeapFree(HANDLE, DWORD, LPVOID mem)
{
	GlobalFree(mem);
	return TRUE;
}

LPVOID VirtualAlloc(LPVOID, SIZE_T size, DWORD, DWORD)
{
	return std::calloc(1, size);
}

BOOL VirtualFree(LPVOID address, SIZE_T, DWORD)
{
	std::free(address);
	return TRUE;
}

// ---- strings -------------------------------------------------------------------------------------
namespace
{
	QString decode(UINT codePage, const char* src, int len)
	{
		const QByteArray bytes(src, len);
		switch (codePage)
		{
			case CP_UTF8:
			case CP_ACP:
			case CP_THREAD_ACP:
			case CP_OEMCP: // the code page of consoles: UTF-8 on Linux
				return QString::fromUtf8(bytes);
			case 437:
			{
				QStringDecoder dec("IBM437");
				return dec.isValid() ? QString(dec(bytes)) : QString::fromLatin1(bytes);
			}
			case 28591:
				return QString::fromLatin1(bytes);
			default:
			{
				const QByteArray name = (codePage >= 1250 && codePage <= 1258) ? ("windows-" + QByteArray::number(codePage))
					: ("CP" + QByteArray::number(codePage));
				QStringDecoder dec(name.constData());
				return dec.isValid() ? QString(dec(bytes)) : QString::fromLatin1(bytes);
			}
		}
	}

	QByteArray encode(UINT codePage, const QString& s)
	{
		switch (codePage)
		{
			case CP_UTF8:
			case CP_ACP:
			case CP_THREAD_ACP:
			case CP_OEMCP:
				return s.toUtf8();
			case 28591:
				return s.toLatin1();
			default:
			{
				const QByteArray name = (codePage >= 1250 && codePage <= 1258) ? ("windows-" + QByteArray::number(codePage))
					: ("CP" + QByteArray::number(codePage));
				QStringEncoder enc(name.constData());
				return enc.isValid() ? QByteArray(enc(s)) : s.toLatin1();
			}
		}
	}
}

int MultiByteToWideChar(UINT codePage, DWORD, LPCSTR src, int srcLen, LPWSTR dst, int dstLen)
{
	if (!src)
		return 0;
	const bool withNul = srcLen < 0;
	const int len = withNul ? static_cast<int>(std::strlen(src)) : srcLen;
	std::wstring w = decode(codePage, src, len).toStdWString();
	if (withNul)
		w.push_back(L'\0');
	if (dstLen == 0 || !dst)
		return static_cast<int>(w.size());
	if (static_cast<int>(w.size()) > dstLen)
	{
		g_lastError = ERROR_INSUFFICIENT_BUFFER;
		std::wmemcpy(dst, w.data(), static_cast<size_t>(dstLen));
		return 0;
	}
	std::wmemcpy(dst, w.data(), w.size());
	return static_cast<int>(w.size());
}

int WideCharToMultiByte(UINT codePage, DWORD, LPCWSTR src, int srcLen, LPSTR dst, int dstLen, LPCSTR, LPBOOL usedDefChar)
{
	if (!src)
		return 0;
	const bool withNul = srcLen < 0;
	const QString s = withNul ? QString::fromWCharArray(src) : QString::fromWCharArray(src, srcLen);
	QByteArray b = encode(codePage, s);
	if (withNul)
		b.append('\0');
	if (usedDefChar)
		*usedDefChar = FALSE;
	if (dstLen == 0 || !dst)
		return static_cast<int>(b.size());
	if (b.size() > dstLen)
	{
		g_lastError = ERROR_INSUFFICIENT_BUFFER;
		std::memcpy(dst, b.constData(), static_cast<size_t>(dstLen));
		return 0;
	}
	std::memcpy(dst, b.constData(), static_cast<size_t>(b.size()));
	return static_cast<int>(b.size());
}

UINT GetACP()
{
	return CP_UTF8;
}

UINT GetOEMCP()
{
	return CP_UTF8; // the console code page of Linux terminals
}

BOOL IsValidCodePage(UINT codePage)
{
	if (codePage == CP_UTF8 || codePage == CP_ACP || codePage == 28591)
		return TRUE;
	const QByteArray name = (codePage >= 1250 && codePage <= 1258) ? ("windows-" + QByteArray::number(codePage)) : ("CP" + QByteArray::number(codePage));
	return QStringDecoder(name.constData()).isValid() ? TRUE : FALSE;
}

BOOL IsDBCSLeadByteEx(UINT codePage, BYTE c)
{
	if (codePage == 932)
		return (c >= 0x81 && c <= 0x9F) || (c >= 0xE0 && c <= 0xFC);
	if (codePage == 936 || codePage == 949 || codePage == 950)
		return c >= 0x81 && c <= 0xFE;
	return FALSE;
}

int lstrlenW(LPCWSTR s) { return s ? static_cast<int>(std::wcslen(s)) : 0; }
int lstrlenA(LPCSTR s) { return s ? static_cast<int>(std::strlen(s)) : 0; }
LPWSTR lstrcpyW(LPWSTR dst, LPCWSTR src) { return std::wcscpy(dst, src ? src : L""); }
LPSTR lstrcpyA(LPSTR dst, LPCSTR src) { return std::strcpy(dst, src ? src : ""); }
LPWSTR lstrcpynW(LPWSTR dst, LPCWSTR src, int max)
{
	if (!dst || max <= 0)
		return dst;
	std::wcsncpy(dst, src ? src : L"", static_cast<size_t>(max - 1));
	dst[max - 1] = 0;
	return dst;
}
LPSTR lstrcpynA(LPSTR dst, LPCSTR src, int max)
{
	if (!dst || max <= 0)
		return dst;
	std::strncpy(dst, src ? src : "", static_cast<size_t>(max - 1));
	dst[max - 1] = 0;
	return dst;
}
LPWSTR lstrcatW(LPWSTR dst, LPCWSTR src) { return std::wcscat(dst, src ? src : L""); }
LPSTR lstrcatA(LPSTR dst, LPCSTR src) { return std::strcat(dst, src ? src : ""); }
int lstrcmpW(LPCWSTR a, LPCWSTR b) { return QString::compare(qs(a), qs(b), Qt::CaseSensitive) < 0 ? -1 : (qs(a) == qs(b) ? 0 : 1); }
int lstrcmpiW(LPCWSTR a, LPCWSTR b)
{
	const int c = QString::compare(qs(a), qs(b), Qt::CaseInsensitive);
	return c < 0 ? -1 : (c > 0 ? 1 : 0);
}
int lstrcmpA(LPCSTR a, LPCSTR b) { return std::strcmp(a ? a : "", b ? b : ""); }
int lstrcmpiA(LPCSTR a, LPCSTR b) { return strcasecmp(a ? a : "", b ? b : ""); }

int CompareStringW(LCID, DWORD flags, LPCWSTR a, int lenA, LPCWSTR b, int lenB)
{
	QCollator collator;
	collator.setCaseSensitivity((flags & NORM_IGNORECASE) ? Qt::CaseInsensitive : Qt::CaseSensitive);
	const int r = collator.compare(qs(a, lenA), qs(b, lenB));
	return r < 0 ? CSTR_LESS_THAN : (r > 0 ? CSTR_GREATER_THAN : CSTR_EQUAL);
}

int CompareStringEx(LPCWSTR, DWORD flags, LPCWSTR a, int lenA, LPCWSTR b, int lenB, void*, void*, LPARAM)
{
	return CompareStringW(0, flags, a, lenA, b, lenB);
}

int CompareStringOrdinal(LPCWSTR a, int lenA, LPCWSTR b, int lenB, BOOL ignoreCase)
{
	const int r = QString::compare(qs(a, lenA), qs(b, lenB), ignoreCase ? Qt::CaseInsensitive : Qt::CaseSensitive);
	return r < 0 ? CSTR_LESS_THAN : (r > 0 ? CSTR_GREATER_THAN : CSTR_EQUAL);
}

int LCMapStringW(LCID, DWORD flags, LPCWSTR src, int srcLen, LPWSTR dst, int dstLen)
{
	QString s = qs(src, srcLen);
	if (flags & LCMAP_LOWERCASE)
		s = s.toLower();
	else if (flags & LCMAP_UPPERCASE)
		s = s.toUpper();
	const std::wstring w = ws(s);
	const int n = static_cast<int>(w.size()) + (srcLen < 0 ? 1 : 0);
	if (!dst || dstLen == 0)
		return n;
	std::wmemcpy(dst, w.c_str(), static_cast<size_t>(std::min(n, dstLen)));
	return std::min(n, dstLen);
}

int LCMapStringEx(LPCWSTR, DWORD flags, LPCWSTR src, int srcLen, LPWSTR dst, int dstLen, void*, void*, LPARAM)
{
	return LCMapStringW(0, flags, src, srcLen, dst, dstLen);
}

BOOL GetStringTypeW(DWORD, LPCWSTR src, int len, LPWORD types)
{
	if (!src || !types)
		return FALSE;
	const int n = len < 0 ? static_cast<int>(std::wcslen(src)) + 1 : len;
	for (int i = 0; i < n; ++i)
	{
		const QChar c(static_cast<char32_t>(src[i]) > 0xFFFF ? QChar(0xFFFD) : QChar(static_cast<char16_t>(src[i])));
		WORD t = 0;
		if (c.isUpper()) t |= C1_UPPER;
		if (c.isLower()) t |= C1_LOWER;
		if (c.isDigit()) t |= C1_DIGIT;
		if (c.isSpace()) t |= C1_SPACE;
		if (c.isPunct() || c.isSymbol()) t |= C1_PUNCT;
		if (c.category() == QChar::Other_Control) t |= C1_CNTRL;
		if (c == QLatin1Char(' ') || c == QLatin1Char('\t')) t |= C1_BLANK;
		if (std::iswxdigit(src[i])) t |= C1_XDIGIT;
		if (c.isLetter()) t |= C1_ALPHA;
		types[i] = t;
	}
	return TRUE;
}

LCID GetUserDefaultLCID() { return 0x0409; }
LCID GetThreadLocale() { return 0x0409; }
LANGID GetUserDefaultUILanguage() { return 0x0409; }
LANGID GetUserDefaultLangID() { return 0x0409; }

int GetLocaleInfoW(LCID, DWORD type, LPWSTR data, int max)
{
	const QLocale l;
	if (type & LOCALE_RETURN_NUMBER)
	{
		// the value as a DWORD in data (2 wchar_t)
		DWORD n = 0;
		switch (type & 0xFFFF)
		{
			case LOCALE_IDIGITS: n = 2; break;
			case LOCALE_ILZERO: n = 1; break;
			case LOCALE_INEGNUMBER: n = 1; break; // "-1.1"
			default: break;
		}
		const int size = static_cast<int>(sizeof(DWORD) / sizeof(wchar_t));
		if (!data || max == 0)
			return size;
		if (max < size)
			return 0;
		std::memcpy(data, &n, sizeof(DWORD));
		return size;
	}
	QString v;
	switch (type & 0xFFFF)
	{
		case LOCALE_SGROUPING: v = QStringLiteral("3;0"); break;
		case LOCALE_IDIGITS: v = QStringLiteral("2"); break;
		case LOCALE_ILZERO: v = QStringLiteral("1"); break;
		case LOCALE_INEGNUMBER: v = QStringLiteral("1"); break;
		case LOCALE_SNEGATIVESIGN: v = l.negativeSign(); break;
		case LOCALE_SDECIMAL: v = l.decimalPoint(); break;
		case LOCALE_STHOUSAND: v = l.groupSeparator(); break;
		case LOCALE_SISO639LANGNAME: v = l.name().section(QLatin1Char('_'), 0, 0); break;
		case LOCALE_SISO3166CTRYNAME: v = l.name().section(QLatin1Char('_'), 1, 1); break;
		default: break;
	}
	if (!data || max == 0)
		return static_cast<int>(ws(v).size()) + 1;
	return copyOut(v, data, max) + 1;
}

BOOL IsValidLocaleName(LPCWSTR name)
{
	if (!name || !*name)
		return FALSE;
	const QString n = qs(name).replace(QLatin1Char('-'), QLatin1Char('_'));
	const QLocale l(n);
	if (l.language() == QLocale::C)
		return FALSE;
	// QLocale falls back to English for what it does not know
	return (l.language() != QLocale::English || n.startsWith(QLatin1String("en"), Qt::CaseInsensitive)) ? TRUE : FALSE;
}

int GetLocaleInfoEx(LPCWSTR name, DWORD type, LPWSTR data, int max)
{
	const DWORD what = type & 0xFFFF;
	if (name && *name && !(type & LOCALE_RETURN_NUMBER))
	{
		// the names of a locale ("en-US"): QLocale; an unknown locale is an error, like on Windows
		const QString n = qs(name).replace(QLatin1Char('-'), QLatin1Char('_'));
		const QLocale l(n);
		if (l.language() == QLocale::C || (l.language() == QLocale::English && !n.startsWith(QLatin1String("en"), Qt::CaseInsensitive)))
		{
			SetLastError(ERROR_INVALID_PARAMETER);
			return 0;
		}
		const bool hasTerritory = n.contains(QLatin1Char('_'));
		QString v;
		switch (what)
		{
			case LOCALE_SENGLISHDISPLAYNAME:
			case LOCALE_SLOCALIZEDDISPLAYNAME:
				v = QLocale::languageToString(l.language());
				if (hasTerritory)
					v += QStringLiteral(" (") + QLocale::territoryToString(l.territory()) + QLatin1Char(')');
				break;
			case LOCALE_SNATIVEDISPLAYNAME:
				v = l.nativeLanguageName();
				if (hasTerritory)
					v += QStringLiteral(" (") + l.nativeTerritoryName() + QLatin1Char(')');
				break;
			case LOCALE_SENGLISHLANGUAGENAME: v = QLocale::languageToString(l.language()); break;
			case LOCALE_SNATIVELANGUAGENAME: v = l.nativeLanguageName(); break;
			case LOCALE_SENGLISHCOUNTRYNAME: v = QLocale::territoryToString(l.territory()); break;
			case LOCALE_SNATIVECOUNTRYNAME: v = l.nativeTerritoryName(); break;
			case LOCALE_SNAME: v = l.name().replace(QLatin1Char('_'), QLatin1Char('-')); break;
			default: return GetLocaleInfoW(0, type, data, max);
		}
		if (!data || max == 0)
			return static_cast<int>(ws(v).size()) + 1;
		return copyOut(v, data, max) + 1;
	}
	return GetLocaleInfoW(0, type, data, max);
}

int GetNumberFormatW(LCID, DWORD, LPCWSTR value, const NUMBERFMTW* format, LPWSTR out, int max)
{
	// value: "-1234.5" (digits, '.', a leading '-'); out: with the separators of format or of the locale
	if (!value)
		return 0;
	const QLocale l;
	QString v = qs(value).trimmed();
	const bool negative = v.startsWith(QLatin1Char('-'));
	if (negative)
		v.remove(0, 1);
	QString intPart = v.section(QLatin1Char('.'), 0, 0);
	QString frac = v.section(QLatin1Char('.'), 1);
	for (const QChar ch : intPart + frac)
	{
		if (!ch.isDigit())
		{
			SetLastError(ERROR_INVALID_PARAMETER);
			return 0;
		}
	}
	const int digits = format ? static_cast<int>(format->NumDigits) : 2;
	const bool leadingZero = format ? format->LeadingZero != 0 : true;
	const int grouping = format ? static_cast<int>(format->Grouping) : 3;
	const QString dec = format && format->lpDecimalSep ? qs(format->lpDecimalSep) : QString(l.decimalPoint());
	const QString thou = format && format->lpThousandSep ? qs(format->lpThousandSep) : QString(l.groupSeparator());
	// rounding of the fraction to digits
	if (frac.size() > digits)
	{
		const bool up = frac.at(digits) >= QLatin1Char('5');
		frac.truncate(digits);
		if (up)
		{
			QString all = intPart + frac;
			int i = static_cast<int>(all.size()) - 1;
			for (; i >= 0; --i)
			{
				if (all[i] == QLatin1Char('9'))
					all[i] = QLatin1Char('0');
				else
				{
					all[i] = QChar(all[i].unicode() + 1);
					break;
				}
			}
			if (i < 0)
				all.prepend(QLatin1Char('1'));
			intPart = all.left(all.size() - frac.size());
			frac = all.right(frac.size());
		}
	}
	while (frac.size() < digits)
		frac += QLatin1Char('0');
	intPart.remove(QRegularExpression(QStringLiteral("^0+(?=\\d)")));
	if (intPart.isEmpty() || (intPart == QLatin1String("0") && !leadingZero && digits > 0))
		intPart = leadingZero || digits == 0 ? QStringLiteral("0") : QString();
	if (grouping > 0)
	{
		const int g = grouping >= 10 ? grouping / 10 : grouping; // 32: "3;2" (first group 3)
		for (int i = static_cast<int>(intPart.size()) - g; i > 0; i -= g)
			intPart.insert(i, thou);
	}
	QString r = intPart;
	if (digits > 0)
		r += dec + frac;
	if (negative)
	{
		switch (format ? format->NegativeOrder : 1)
		{
			case 0: r = QLatin1Char('(') + r + QLatin1Char(')'); break;
			case 2: r = QStringLiteral("- ") + r; break;
			case 3: r += QLatin1Char('-'); break;
			case 4: r += QStringLiteral(" -"); break;
			default: r.prepend(QLatin1Char('-')); break;
		}
	}
	const int needed = static_cast<int>(ws(r).size()) + 1;
	if (!out || max == 0)
		return needed;
	if (max < needed)
	{
		SetLastError(ERROR_INSUFFICIENT_BUFFER);
		return 0;
	}
	return copyOut(r, out, max) + 1;
}

int GetNumberFormatEx(LPCWSTR, DWORD flags, LPCWSTR value, const NUMBERFMTW* format, LPWSTR out, int max)
{
	return GetNumberFormatW(0, flags, value, format, out, max);
}

namespace
{
	QDateTime fromSystemTime(const SYSTEMTIME* st)
	{
		if (!st)
			return QDateTime::currentDateTime();
		return QDateTime(QDate(st->wYear, st->wMonth, st->wDay), QTime(st->wHour, st->wMinute, st->wSecond, st->wMilliseconds));
	}

	QString windowsDateFormat(const QString& f)
	{
		// the date and time pictures of Windows are those of Qt, but for tt (AM/PM)
		QString r = f;
		r.replace(QStringLiteral("tt"), QStringLiteral("AP"));
		return r;
	}
}

int GetDateFormatW(LCID, DWORD flags, const SYSTEMTIME* date, LPCWSTR format, LPWSTR out, int max)
{
	const QDateTime dt = fromSystemTime(date);
	const QString s = format ? QLocale().toString(dt.date(), windowsDateFormat(qs(format)))
		: QLocale().toString(dt.date(), (flags & DATE_LONGDATE) ? QLocale::LongFormat : QLocale::ShortFormat);
	if (!out || max == 0)
		return static_cast<int>(ws(s).size()) + 1;
	return copyOut(s, out, max) + 1;
}

int GetTimeFormatW(LCID, DWORD flags, const SYSTEMTIME* time, LPCWSTR format, LPWSTR out, int max)
{
	const QDateTime dt = fromSystemTime(time);
	const QString s = format ? QLocale().toString(dt.time(), windowsDateFormat(qs(format)))
		: QLocale().toString(dt.time(), (flags & TIME_NOSECONDS) ? QLocale::ShortFormat : QLocale::LongFormat);
	if (!out || max == 0)
		return static_cast<int>(ws(s).size()) + 1;
	return copyOut(s, out, max) + 1;
}

// ---- files ---------------------------------------------------------------------------------
HANDLE CreateFileW(LPCWSTR path, DWORD access, DWORD, LPSECURITY_ATTRIBUTES, DWORD disposition, DWORD flags, HANDLE)
{
	const std::string p = fsPath(path);
	int oflags = O_CLOEXEC;
	if ((access & GENERIC_READ) && (access & GENERIC_WRITE))
		oflags |= O_RDWR;
	else if (access & (GENERIC_WRITE | GENERIC_ALL))
		oflags |= O_WRONLY;
	else
		oflags |= O_RDONLY;
	struct stat st{};
	const bool exists = ::stat(p.c_str(), &st) == 0;
	switch (disposition)
	{
		case CREATE_NEW:
			oflags |= O_CREAT | O_EXCL;
			break;
		case CREATE_ALWAYS:
			oflags |= O_CREAT | O_TRUNC;
			break;
		case OPEN_ALWAYS:
			oflags |= O_CREAT;
			break;
		case TRUNCATE_EXISTING:
			oflags |= O_TRUNC;
			break;
		default:
			break;
	}
	if (exists && S_ISDIR(st.st_mode))
	{
		if (!(flags & FILE_FLAG_BACKUP_SEMANTICS))
		{
			g_lastError = ERROR_ACCESS_DENIED;
			return INVALID_HANDLE_VALUE;
		}
		oflags = O_RDONLY | O_DIRECTORY | O_CLOEXEC;
	}
	const int fd = ::open(p.c_str(), oflags, 0644);
	if (fd < 0)
	{
		setErrno();
		return INVALID_HANDLE_VALUE;
	}
	auto* k = new KObject(KType::File);
	k->fd = fd;
	g_lastError = ((disposition == CREATE_ALWAYS || disposition == OPEN_ALWAYS) && exists) ? ERROR_ALREADY_EXISTS : 0;
	return k;
}

HANDLE CreateFileA(LPCSTR path, DWORD access, DWORD share, LPSECURITY_ATTRIBUTES sa, DWORD disposition, DWORD flags, HANDLE templ)
{
	const std::wstring w = ws(qsA(path));
	return CreateFileW(w.c_str(), access, share, sa, disposition, flags, templ);
}

BOOL ReadFile(HANDLE h, LPVOID buffer, DWORD toRead, LPDWORD read, LPOVERLAPPED)
{
	KObject* k = kobj(h);
	if (read)
		*read = 0;
	if (!k || k->fd < 0)
	{
		g_lastError = ERROR_INVALID_HANDLE;
		return FALSE;
	}
	ssize_t n;
	do
		n = ::read(k->fd, buffer, toRead);
	while (n < 0 && errno == EINTR);
	if (n < 0)
	{
		setErrno();
		return FALSE;
	}
	if (read)
		*read = static_cast<DWORD>(n);
	if (n == 0 && k->type == KType::Pipe && toRead > 0)
	{
		g_lastError = ERROR_BROKEN_PIPE;
		return FALSE;
	}
	return TRUE;
}

BOOL WriteFile(HANDLE h, LPCVOID buffer, DWORD toWrite, LPDWORD written, LPOVERLAPPED)
{
	KObject* k = kobj(h);
	if (written)
		*written = 0;
	if (!k || k->fd < 0)
	{
		g_lastError = ERROR_INVALID_HANDLE;
		return FALSE;
	}
	const char* p = static_cast<const char*>(buffer);
	DWORD done = 0;
	while (done < toWrite)
	{
		const ssize_t n = ::write(k->fd, p + done, toWrite - done);
		if (n < 0)
		{
			if (errno == EINTR)
				continue;
			setErrno();
			if (written)
				*written = done;
			return FALSE;
		}
		done += static_cast<DWORD>(n);
	}
	if (written)
		*written = done;
	return TRUE;
}

BOOL FlushFileBuffers(HANDLE h)
{
	KObject* k = kobj(h);
	return (k && k->fd >= 0 && ::fsync(k->fd) == 0) ? TRUE : FALSE;
}

BOOL CloseHandle(HANDLE h)
{
	KObject* k = kobj(h);
	if (!k)
		return h == nullptr ? FALSE : TRUE;
	if (!k->name.empty())
	{
		std::lock_guard<std::mutex> lock(namedMutex());
		if (--k->refs > 0)
			return TRUE;
		namedObjects().erase(k->name);
	}
	switch (k->type)
	{
		case KType::File:
		case KType::Pipe:
			if (k->fd >= 0)
				::close(k->fd);
			k->fd = -1;
			break;
		case KType::Find:
			if (k->dir)
				closedir(k->dir);
			k->dir = nullptr;
			break;
		case KType::Thread:
		case KType::Process:
			// the thread or the process may still run: the object stays
			return TRUE;
		default:
			break;
	}
	k->magic = 0;
	delete k;
	return TRUE;
}

DWORD GetFileSize(HANDLE h, LPDWORD high)
{
	KObject* k = kobj(h);
	struct stat st{};
	if (!k || k->fd < 0 || ::fstat(k->fd, &st) != 0)
		return INVALID_FILE_SIZE;
	if (high)
		*high = static_cast<DWORD>(static_cast<unsigned long long>(st.st_size) >> 32);
	return static_cast<DWORD>(st.st_size & 0xFFFFFFFF);
}

BOOL GetFileInformationByHandle(HANDLE h, LPBY_HANDLE_FILE_INFORMATION info)
{
	KObject* k = kobj(h);
	struct stat st{};
	if (!k || k->fd < 0 || !info || ::fstat(k->fd, &st) != 0)
	{
		SetLastError(ERROR_INVALID_HANDLE);
		return FALSE;
	}
	*info = BY_HANDLE_FILE_INFORMATION{};
	info->dwFileAttributes = S_ISDIR(st.st_mode) ? FILE_ATTRIBUTE_DIRECTORY : FILE_ATTRIBUTE_NORMAL;
	auto fileTime = [](const timespec& t)
	{
		// 100 ns intervals since 1601
		const unsigned long long v = (static_cast<unsigned long long>(t.tv_sec) + 11644473600ULL) * 10000000ULL + static_cast<unsigned long long>(t.tv_nsec) / 100;
		return FILETIME{static_cast<DWORD>(v & 0xFFFFFFFF), static_cast<DWORD>(v >> 32)};
	};
	info->ftCreationTime = fileTime(st.st_ctim);
	info->ftLastAccessTime = fileTime(st.st_atim);
	info->ftLastWriteTime = fileTime(st.st_mtim);
	info->dwVolumeSerialNumber = static_cast<DWORD>(st.st_dev);
	info->nFileSizeHigh = static_cast<DWORD>(static_cast<unsigned long long>(st.st_size) >> 32);
	info->nFileSizeLow = static_cast<DWORD>(st.st_size & 0xFFFFFFFF);
	info->nNumberOfLinks = static_cast<DWORD>(st.st_nlink);
	info->nFileIndexHigh = static_cast<DWORD>(static_cast<unsigned long long>(st.st_ino) >> 32);
	info->nFileIndexLow = static_cast<DWORD>(st.st_ino & 0xFFFFFFFF);
	return TRUE;
}

wchar_t* _wtmpnam(wchar_t* buffer)
{
	static thread_local wchar_t name[MAX_PATH];
	wchar_t* out = buffer ? buffer : name;
	static std::atomic<unsigned> serial{0};
	const QString path = QDir::tempPath() + QStringLiteral("/npp-%1-%2.tmp").arg(QCoreApplication::applicationPid()).arg(++serial);
	copyOut(path, out, MAX_PATH);
	return out;
}

BOOL GetFileSizeEx(HANDLE h, PLARGE_INTEGER size)
{
	KObject* k = kobj(h);
	struct stat st{};
	if (!k || k->fd < 0 || !size || ::fstat(k->fd, &st) != 0)
		return FALSE;
	size->QuadPart = st.st_size;
	return TRUE;
}

DWORD SetFilePointer(HANDLE h, LONG distance, PLONG high, DWORD method)
{
	KObject* k = kobj(h);
	if (!k || k->fd < 0)
		return INVALID_SET_FILE_POINTER;
	off_t off = distance;
	if (high)
		off = static_cast<off_t>((static_cast<unsigned long long>(static_cast<DWORD>(*high)) << 32) | static_cast<DWORD>(distance));
	const int whence = method == FILE_CURRENT ? SEEK_CUR : (method == FILE_END ? SEEK_END : SEEK_SET);
	const off_t r = ::lseek(k->fd, off, whence);
	if (r < 0)
		return INVALID_SET_FILE_POINTER;
	if (high)
		*high = static_cast<LONG>(static_cast<unsigned long long>(r) >> 32);
	return static_cast<DWORD>(r & 0xFFFFFFFF);
}

BOOL SetFilePointerEx(HANDLE h, LARGE_INTEGER distance, PLARGE_INTEGER newPos, DWORD method)
{
	KObject* k = kobj(h);
	if (!k || k->fd < 0)
		return FALSE;
	const int whence = method == FILE_CURRENT ? SEEK_CUR : (method == FILE_END ? SEEK_END : SEEK_SET);
	const off_t r = ::lseek(k->fd, static_cast<off_t>(distance.QuadPart), whence);
	if (r < 0)
		return FALSE;
	if (newPos)
		newPos->QuadPart = r;
	return TRUE;
}

BOOL SetEndOfFile(HANDLE h)
{
	KObject* k = kobj(h);
	if (!k || k->fd < 0)
		return FALSE;
	const off_t pos = ::lseek(k->fd, 0, SEEK_CUR);
	return ::ftruncate(k->fd, pos) == 0 ? TRUE : FALSE;
}

namespace
{
	FILETIME toFileTime(time_t t, long nsec)
	{
		const unsigned long long v = (static_cast<unsigned long long>(t) + 11644473600ULL) * 10000000ULL + static_cast<unsigned long long>(nsec / 100);
		return FILETIME{static_cast<DWORD>(v & 0xFFFFFFFF), static_cast<DWORD>(v >> 32)};
	}

	DWORD attributesOf(const struct stat& st, const std::string& name)
	{
		DWORD a = 0;
		if (S_ISDIR(st.st_mode))
			a |= FILE_ATTRIBUTE_DIRECTORY;
		if (!(st.st_mode & S_IWUSR))
			a |= FILE_ATTRIBUTE_READONLY;
		const size_t slash = name.find_last_of('/');
		const std::string base = slash == std::string::npos ? name : name.substr(slash + 1);
		if (!base.empty() && base[0] == '.' && base != "." && base != "..")
			a |= FILE_ATTRIBUTE_HIDDEN;
		if (!a)
			a = FILE_ATTRIBUTE_NORMAL;
		else if (!(a & FILE_ATTRIBUTE_DIRECTORY))
			a |= FILE_ATTRIBUTE_ARCHIVE;
		return a;
	}
}

BOOL GetFileTime(HANDLE h, LPFILETIME created, LPFILETIME accessed, LPFILETIME written)
{
	KObject* k = kobj(h);
	struct stat st{};
	if (!k || k->fd < 0 || ::fstat(k->fd, &st) != 0)
		return FALSE;
	if (created)
		*created = toFileTime(st.st_ctim.tv_sec, st.st_ctim.tv_nsec);
	if (accessed)
		*accessed = toFileTime(st.st_atim.tv_sec, st.st_atim.tv_nsec);
	if (written)
		*written = toFileTime(st.st_mtim.tv_sec, st.st_mtim.tv_nsec);
	return TRUE;
}

BOOL SetFileTime(HANDLE h, const FILETIME* created, const FILETIME* accessed, const FILETIME* written)
{
	(void)created; // Linux files have no creation time to set
	KObject* k = kobj(h);
	if (!k || k->fd < 0)
		return FALSE;
	auto spec = [](const FILETIME* ft)
	{
		timespec ts{};
		if (!ft || (ft->dwLowDateTime == 0 && ft->dwHighDateTime == 0))
		{
			ts.tv_nsec = UTIME_OMIT;
			return ts;
		}
		const unsigned long long v = (static_cast<unsigned long long>(ft->dwHighDateTime) << 32) | ft->dwLowDateTime;
		ts.tv_sec = static_cast<time_t>(v / 10000000ULL - 11644473600ULL);
		ts.tv_nsec = static_cast<long>((v % 10000000ULL) * 100);
		return ts;
	};
	const timespec times[2] = {spec(accessed), spec(written)};
	if (::futimens(k->fd, times) != 0)
	{
		setErrno();
		return FALSE;
	}
	return TRUE;
}

DWORD SearchPathW(LPCWSTR path, LPCWSTR fileName, LPCWSTR extension, DWORD bufferLength, LPWSTR buffer, LPWSTR* filePart)
{
	if (!fileName || !*fileName)
		return 0;
	QString name = nativePath(fileName);
	if (extension && *extension && QFileInfo(name).suffix().isEmpty())
		name += qs(extension);
	QStringList dirs;
	if (QFileInfo(name).isAbsolute())
		dirs << QString();
	else if (path && *path)
		dirs = nativePath(path).split(QRegularExpression(QStringLiteral("[;:]")), Qt::SkipEmptyParts);
	else
		dirs = QString::fromLocal8Bit(qgetenv("PATH")).split(QLatin1Char(':'), Qt::SkipEmptyParts);
	for (const QString& dir : dirs)
	{
		const QString candidate = dir.isEmpty() ? name : QDir(dir).filePath(name);
		const QFileInfo fi(candidate);
		if (!fi.exists() || fi.isDir())
			continue;
		const QString full = fi.absoluteFilePath();
		const DWORD needed = static_cast<DWORD>(full.size()) + 1;
		if (!buffer || bufferLength < needed)
			return needed;
		copyOut(full, buffer, static_cast<int>(bufferLength));
		if (filePart)
			*filePart = buffer + (full.size() - fi.fileName().size());
		return needed - 1;
	}
	g_lastError = ERROR_FILE_NOT_FOUND;
	return 0;
}

// no pseudo consoles and no job objects here: the callers use their other ways (pipes, the process)
BOOL InitializeProcThreadAttributeList(LPPROC_THREAD_ATTRIBUTE_LIST, DWORD, DWORD, PSIZE_T size)
{
	if (size)
		*size = 0;
	g_lastError = ERROR_NOT_SUPPORTED;
	return FALSE;
}

BOOL UpdateProcThreadAttribute(LPPROC_THREAD_ATTRIBUTE_LIST, DWORD, DWORD_PTR, PVOID, SIZE_T, PVOID, PSIZE_T)
{
	g_lastError = ERROR_NOT_SUPPORTED;
	return FALSE;
}

void DeleteProcThreadAttributeList(LPPROC_THREAD_ATTRIBUTE_LIST)
{
}

HANDLE CreateJobObjectW(LPSECURITY_ATTRIBUTES, LPCWSTR)
{
	g_lastError = ERROR_NOT_SUPPORTED;
	return nullptr;
}

BOOL SetInformationJobObject(HANDLE, JOBOBJECTINFOCLASS, LPVOID, DWORD)
{
	g_lastError = ERROR_NOT_SUPPORTED;
	return FALSE;
}

BOOL AssignProcessToJobObject(HANDLE, HANDLE)
{
	g_lastError = ERROR_NOT_SUPPORTED;
	return FALSE;
}

BOOL IsProcessInJob(HANDLE, HANDLE, PBOOL result)
{
	if (result)
		*result = FALSE;
	return TRUE;
}

BOOL TerminateJobObject(HANDLE, UINT)
{
	g_lastError = ERROR_NOT_SUPPORTED;
	return FALSE;
}

BOOL SetConsoleCtrlHandler(PHANDLER_ROUTINE, BOOL)
{
	return TRUE;
}

BOOL GenerateConsoleCtrlEvent(DWORD ctrlEvent, DWORD processGroupId)
{
	// Ctrl+C / Ctrl+Break of a console: SIGINT to the process group of the process (its own group
	// with CREATE_NEW_PROCESS_GROUP), else to the process; 0 (all of this console) signals nothing
	if (processGroupId == 0)
		return TRUE;
	const int sig = (ctrlEvent == CTRL_C_EVENT || ctrlEvent == CTRL_BREAK_EVENT) ? SIGINT : SIGTERM;
	const pid_t pid = static_cast<pid_t>(processGroupId);
	if (::kill(-pid, sig) == 0 || ::kill(pid, sig) == 0)
		return TRUE;
	setErrno();
	return FALSE;
}

BOOL AllocConsole()
{
	return FALSE;
}

BOOL FreeConsole()
{
	return TRUE;
}

BOOL AttachConsole(DWORD)
{
	g_lastError = ERROR_NOT_SUPPORTED;
	return FALSE;
}

DWORD GetFileType(HANDLE h)
{
	KObject* k = kobj(h);
	if (!k)
		return FILE_TYPE_UNKNOWN;
	if (k->type == KType::Pipe)
		return FILE_TYPE_PIPE;
	struct stat st{};
	if (k->fd >= 0 && ::fstat(k->fd, &st) == 0)
	{
		if (S_ISCHR(st.st_mode))
			return FILE_TYPE_CHAR;
		if (S_ISFIFO(st.st_mode) || S_ISSOCK(st.st_mode))
			return FILE_TYPE_PIPE;
	}
	return FILE_TYPE_DISK;
}

DWORD GetFileAttributesW(LPCWSTR path)
{
	const std::string p = fsPath(path);
	struct stat st{};
	if (::stat(p.c_str(), &st) != 0)
	{
		setErrno();
		return INVALID_FILE_ATTRIBUTES;
	}
	return attributesOf(st, p);
}

DWORD GetFileAttributesA(LPCSTR path)
{
	const std::wstring w = ws(qsA(path));
	return GetFileAttributesW(w.c_str());
}

BOOL GetFileAttributesExW(LPCWSTR path, GET_FILEEX_INFO_LEVELS, LPVOID info)
{
	const std::string p = fsPath(path);
	struct stat st{};
	if (::stat(p.c_str(), &st) != 0 || !info)
	{
		setErrno();
		return FALSE;
	}
	auto* d = static_cast<WIN32_FILE_ATTRIBUTE_DATA*>(info);
	d->dwFileAttributes = attributesOf(st, p);
	d->ftCreationTime = toFileTime(st.st_ctim.tv_sec, st.st_ctim.tv_nsec);
	d->ftLastAccessTime = toFileTime(st.st_atim.tv_sec, st.st_atim.tv_nsec);
	d->ftLastWriteTime = toFileTime(st.st_mtim.tv_sec, st.st_mtim.tv_nsec);
	d->nFileSizeHigh = static_cast<DWORD>(static_cast<unsigned long long>(st.st_size) >> 32);
	d->nFileSizeLow = static_cast<DWORD>(st.st_size & 0xFFFFFFFF);
	return TRUE;
}

BOOL SetFileAttributesW(LPCWSTR path, DWORD attributes)
{
	const std::string p = fsPath(path);
	struct stat st{};
	if (::stat(p.c_str(), &st) != 0)
		return FALSE;
	mode_t mode = st.st_mode;
	if (attributes & FILE_ATTRIBUTE_READONLY)
		mode &= ~(S_IWUSR | S_IWGRP | S_IWOTH);
	else
		mode |= S_IWUSR;
	return ::chmod(p.c_str(), mode) == 0 ? TRUE : FALSE;
}

BOOL DeleteFileW(LPCWSTR path)
{
	if (::unlink(fsPath(path).c_str()) != 0)
	{
		setErrno();
		return FALSE;
	}
	return TRUE;
}

BOOL DeleteFileA(LPCSTR path)
{
	const std::wstring w = ws(qsA(path));
	return DeleteFileW(w.c_str());
}

BOOL CopyFileW(LPCWSTR from, LPCWSTR to, BOOL failIfExists)
{
	const QString f = nativePath(from);
	const QString t = nativePath(to);
	if (QFileInfo::exists(t))
	{
		if (failIfExists)
		{
			g_lastError = ERROR_FILE_EXISTS;
			return FALSE;
		}
		QFile::remove(t);
	}
	if (!QFile::copy(f, t))
	{
		g_lastError = QFileInfo::exists(f) ? ERROR_ACCESS_DENIED : ERROR_FILE_NOT_FOUND;
		return FALSE;
	}
	return TRUE;
}

BOOL MoveFileW(LPCWSTR from, LPCWSTR to)
{
	return MoveFileExW(from, to, 0);
}

BOOL MoveFileExW(LPCWSTR from, LPCWSTR to, DWORD flags)
{
	const std::string f = fsPath(from);
	if (!to)
		return ::unlink(f.c_str()) == 0 ? TRUE : FALSE;
	const std::string t = fsPath(to);
	struct stat st{};
	if (!(flags & MOVEFILE_REPLACE_EXISTING) && ::stat(t.c_str(), &st) == 0)
	{
		g_lastError = ERROR_ALREADY_EXISTS;
		return FALSE;
	}
	if (::rename(f.c_str(), t.c_str()) != 0)
	{
		if (errno == EXDEV && (flags & MOVEFILE_COPY_ALLOWED) && CopyFileW(from, to, FALSE))
			return DeleteFileW(from);
		setErrno();
		return FALSE;
	}
	return TRUE;
}

BOOL ReplaceFileW(LPCWSTR replaced, LPCWSTR replacement, LPCWSTR backup, DWORD, LPVOID, LPVOID)
{
	if (backup)
		CopyFileW(replaced, backup, FALSE);
	return MoveFileExW(replacement, replaced, MOVEFILE_REPLACE_EXISTING | MOVEFILE_COPY_ALLOWED);
}

BOOL CreateDirectoryW(LPCWSTR path, LPSECURITY_ATTRIBUTES)
{
	if (::mkdir(fsPath(path).c_str(), 0755) != 0)
	{
		setErrno();
		return FALSE;
	}
	return TRUE;
}

BOOL CreateDirectoryA(LPCSTR path, LPSECURITY_ATTRIBUTES sa)
{
	const std::wstring w = ws(qsA(path));
	return CreateDirectoryW(w.c_str(), sa);
}

BOOL RemoveDirectoryW(LPCWSTR path)
{
	if (::rmdir(fsPath(path).c_str()) != 0)
	{
		setErrno();
		return FALSE;
	}
	return TRUE;
}

namespace
{
	bool nextMatch(KObject* k, LPWIN32_FIND_DATAW data)
	{
		while (dirent* e = readdir(k->dir))
		{
			if (fnmatch(k->pattern.c_str(), e->d_name, FNM_CASEFOLD) != 0 && !(k->pattern == "*.*"))
				continue;
			const std::string full = k->dirPath + "/" + e->d_name;
			struct stat st{};
			if (::lstat(full.c_str(), &st) != 0)
				continue;
			*data = WIN32_FIND_DATAW{};
			data->dwFileAttributes = attributesOf(st, full);
			data->ftCreationTime = toFileTime(st.st_ctim.tv_sec, st.st_ctim.tv_nsec);
			data->ftLastAccessTime = toFileTime(st.st_atim.tv_sec, st.st_atim.tv_nsec);
			data->ftLastWriteTime = toFileTime(st.st_mtim.tv_sec, st.st_mtim.tv_nsec);
			data->nFileSizeHigh = static_cast<DWORD>(static_cast<unsigned long long>(st.st_size) >> 32);
			data->nFileSizeLow = static_cast<DWORD>(st.st_size & 0xFFFFFFFF);
			copyOut(QFile::decodeName(e->d_name), data->cFileName, MAX_PATH);
			return true;
		}
		return false;
	}
}

HANDLE FindFirstFileW(LPCWSTR pattern, LPWIN32_FIND_DATAW data)
{
	if (!pattern || !data)
		return INVALID_HANDLE_VALUE;
	const QString p = nativePath(pattern);
	QFileInfo fi(p);
	QString dirPath = fi.path();
	QString name = fi.fileName();
	if (!name.contains(QLatin1Char('*')) && !name.contains(QLatin1Char('?')))
	{
		// a file or folder of its own name
		dirPath = fi.absolutePath();
	}
	auto* k = new KObject(KType::Find);
	k->dirPath = QFile::encodeName(dirPath).toStdString();
	k->pattern = QFile::encodeName(name).toStdString();
	k->dir = opendir(k->dirPath.c_str());
	if (!k->dir || !nextMatch(k, data))
	{
		if (k->dir)
			closedir(k->dir);
		k->magic = 0;
		delete k;
		g_lastError = ERROR_FILE_NOT_FOUND;
		return INVALID_HANDLE_VALUE;
	}
	return k;
}

BOOL FindNextFileW(HANDLE h, LPWIN32_FIND_DATAW data)
{
	KObject* k = kobj(h);
	if (!k || k->type != KType::Find || !k->dir || !data)
		return FALSE;
	if (!nextMatch(k, data))
	{
		g_lastError = ERROR_NO_MORE_FILES;
		return FALSE;
	}
	return TRUE;
}

BOOL FindClose(HANDLE h)
{
	return CloseHandle(h);
}

DWORD GetCurrentDirectoryW(DWORD max, LPWSTR buffer)
{
	const QString d = QDir::currentPath();
	if (!buffer || max == 0)
		return static_cast<DWORD>(ws(d).size()) + 1;
	return static_cast<DWORD>(copyOut(d, buffer, static_cast<int>(max)));
}

BOOL SetCurrentDirectoryW(LPCWSTR path)
{
	return QDir::setCurrent(nativePath(path)) ? TRUE : FALSE;
}

DWORD GetTempPathW(DWORD max, LPWSTR buffer)
{
	QString d = QDir::tempPath();
	if (!d.endsWith(QLatin1Char('/')))
		d += QLatin1Char('/');
	if (!buffer || max == 0)
		return static_cast<DWORD>(ws(d).size()) + 1;
	return static_cast<DWORD>(copyOut(d, buffer, static_cast<int>(max)));
}

UINT GetTempFileNameW(LPCWSTR path, LPCWSTR prefix, UINT unique, LPWSTR out)
{
	const QString dir = nativePath(path);
	UINT n = unique ? unique : static_cast<UINT>(QDateTime::currentMSecsSinceEpoch() & 0xFFFF);
	QString name;
	for (int tries = 0; tries < 65536; ++tries, ++n)
	{
		name = QStringLiteral("%1/%2%3.tmp").arg(dir, qs(prefix).left(3)).arg(n & 0xFFFF, 4, 16, QLatin1Char('0'));
		if (unique || !QFileInfo::exists(name))
			break;
	}
	if (!unique)
	{
		QFile f(name);
		if (!f.open(QIODevice::WriteOnly | QIODevice::NewOnly))
			return 0;
	}
	copyOut(name, out, MAX_PATH);
	return n & 0xFFFF;
}

DWORD GetFullPathNameW(LPCWSTR path, DWORD max, LPWSTR buffer, LPWSTR* filePart)
{
	const QString p = QFileInfo(nativePath(path)).absoluteFilePath();
	if (!buffer || max == 0)
		return static_cast<DWORD>(ws(p).size()) + 1;
	const int n = copyOut(p, buffer, static_cast<int>(max));
	if (filePart)
	{
		wchar_t* slash = std::wcsrchr(buffer, L'/');
		*filePart = slash ? slash + 1 : buffer;
	}
	return static_cast<DWORD>(n);
}

DWORD GetLongPathNameW(LPCWSTR path, LPWSTR buffer, DWORD max)
{
	const QString p = qs(path);
	if (!buffer || max == 0)
		return static_cast<DWORD>(ws(p).size()) + 1;
	return static_cast<DWORD>(copyOut(p, buffer, static_cast<int>(max)));
}

DWORD GetShortPathNameW(LPCWSTR path, LPWSTR buffer, DWORD max)
{
	return GetLongPathNameW(path, buffer, max);
}

UINT GetWindowsDirectoryW(LPWSTR buffer, UINT max)
{
	return static_cast<UINT>(copyOut(QStringLiteral("/usr"), buffer, static_cast<int>(max)));
}

UINT GetSystemDirectoryW(LPWSTR buffer, UINT max)
{
	return static_cast<UINT>(copyOut(QStringLiteral("/usr/bin"), buffer, static_cast<int>(max)));
}

BOOL GetDiskFreeSpaceExW(LPCWSTR dir, PULARGE_INTEGER freeToCaller, PULARGE_INTEGER total, PULARGE_INTEGER totalFree)
{
	struct statvfs s{};
	const std::string p = dir ? fsPath(dir) : std::string("/");
	if (::statvfs(p.c_str(), &s) != 0)
		return FALSE;
	if (freeToCaller)
		freeToCaller->QuadPart = static_cast<ULONGLONG>(s.f_bavail) * s.f_frsize;
	if (total)
		total->QuadPart = static_cast<ULONGLONG>(s.f_blocks) * s.f_frsize;
	if (totalFree)
		totalFree->QuadPart = static_cast<ULONGLONG>(s.f_bfree) * s.f_frsize;
	return TRUE;
}

UINT GetDriveTypeW(LPCWSTR root)
{
	return (root && QFileInfo(nativePath(root)).isDir()) ? DRIVE_FIXED : DRIVE_NO_ROOT_DIR;
}

BOOL GetVolumeInformationW(LPCWSTR root, LPWSTR volumeName, DWORD volumeNameSize, LPDWORD serial, LPDWORD maxComponent,
	LPDWORD fsFlags, LPWSTR fsName, DWORD fsNameSize)
{
	const QStorageInfo info(root ? nativePath(root) : QStringLiteral("/"));
	if (!info.isValid())
	{
		SetLastError(ERROR_PATH_NOT_FOUND);
		return FALSE;
	}
	if (volumeName && volumeNameSize)
		copyOut(info.name(), volumeName, static_cast<int>(volumeNameSize));
	if (serial)
		*serial = static_cast<DWORD>(qHash(info.device()));
	if (maxComponent)
		*maxComponent = 255;
	if (fsFlags)
		*fsFlags = 0x00000003; // FILE_CASE_SENSITIVE_SEARCH | FILE_CASE_PRESERVED_NAMES
	if (fsName && fsNameSize)
		copyOut(QString::fromUtf8(info.fileSystemType()), fsName, static_cast<int>(fsNameSize));
	return TRUE;
}

DWORD GetLogicalDrives()
{
	return 1u << 2; // "C:" is the root folder
}

DWORD GetLogicalDriveStringsW(DWORD max, LPWSTR buffer)
{
	static const wchar_t drives[] = L"C:\\\0";
	if (!buffer || max < 5)
		return 5;
	std::wmemcpy(buffer, drives, 5);
	return 4;
}

HANDLE FindFirstChangeNotificationW(LPCWSTR, BOOL, DWORD)
{
	return new KObject(KType::Change); // never signaled: waits time out
}

BOOL FindNextChangeNotification(HANDLE h)
{
	return kobj(h) ? TRUE : FALSE;
}

BOOL FindCloseChangeNotification(HANDLE h)
{
	return CloseHandle(h);
}

BOOL FileTimeToSystemTime(const FILETIME* ft, LPSYSTEMTIME st)
{
	if (!ft || !st)
		return FALSE;
	const unsigned long long v = (static_cast<unsigned long long>(ft->dwHighDateTime) << 32) | ft->dwLowDateTime;
	const long long ms = static_cast<long long>(v / 10000ULL) - 11644473600000LL;
	const QDateTime dt = QDateTime::fromMSecsSinceEpoch(ms, QTimeZone::UTC);
	st->wYear = static_cast<WORD>(dt.date().year());
	st->wMonth = static_cast<WORD>(dt.date().month());
	st->wDayOfWeek = static_cast<WORD>(dt.date().dayOfWeek() % 7);
	st->wDay = static_cast<WORD>(dt.date().day());
	st->wHour = static_cast<WORD>(dt.time().hour());
	st->wMinute = static_cast<WORD>(dt.time().minute());
	st->wSecond = static_cast<WORD>(dt.time().second());
	st->wMilliseconds = static_cast<WORD>(dt.time().msec());
	return TRUE;
}

BOOL SystemTimeToFileTime(const SYSTEMTIME* st, LPFILETIME ft)
{
	if (!st || !ft)
		return FALSE;
	const QDateTime dt(QDate(st->wYear, st->wMonth, st->wDay), QTime(st->wHour, st->wMinute, st->wSecond, st->wMilliseconds), QTimeZone::UTC);
	const unsigned long long v = (static_cast<unsigned long long>(dt.toMSecsSinceEpoch()) + 11644473600000ULL) * 10000ULL;
	ft->dwLowDateTime = static_cast<DWORD>(v & 0xFFFFFFFF);
	ft->dwHighDateTime = static_cast<DWORD>(v >> 32);
	return TRUE;
}

BOOL FileTimeToLocalFileTime(const FILETIME* ft, LPFILETIME local)
{
	if (!ft || !local)
		return FALSE;
	const long long offset = static_cast<long long>(QDateTime::currentDateTime().offsetFromUtc()) * 10000000LL;
	const unsigned long long v = ((static_cast<unsigned long long>(ft->dwHighDateTime) << 32) | ft->dwLowDateTime) + static_cast<unsigned long long>(offset);
	local->dwLowDateTime = static_cast<DWORD>(v & 0xFFFFFFFF);
	local->dwHighDateTime = static_cast<DWORD>(v >> 32);
	return TRUE;
}

BOOL LocalFileTimeToFileTime(const FILETIME* local, LPFILETIME ft)
{
	if (!ft || !local)
		return FALSE;
	const long long offset = static_cast<long long>(QDateTime::currentDateTime().offsetFromUtc()) * 10000000LL;
	const unsigned long long v = ((static_cast<unsigned long long>(local->dwHighDateTime) << 32) | local->dwLowDateTime) - static_cast<unsigned long long>(offset);
	ft->dwLowDateTime = static_cast<DWORD>(v & 0xFFFFFFFF);
	ft->dwHighDateTime = static_cast<DWORD>(v >> 32);
	return TRUE;
}

LONG CompareFileTime(const FILETIME* a, const FILETIME* b)
{
	const unsigned long long va = (static_cast<unsigned long long>(a->dwHighDateTime) << 32) | a->dwLowDateTime;
	const unsigned long long vb = (static_cast<unsigned long long>(b->dwHighDateTime) << 32) | b->dwLowDateTime;
	return va < vb ? -1 : (va > vb ? 1 : 0);
}

BOOL SystemTimeToTzSpecificLocalTime(const void*, const SYSTEMTIME* universal, LPSYSTEMTIME local)
{
	FILETIME ft, lft;
	if (!SystemTimeToFileTime(universal, &ft) || !FileTimeToLocalFileTime(&ft, &lft))
		return FALSE;
	return FileTimeToSystemTime(&lft, local);
}

// ---- INI files -------------------------------------------------------------------------------
DWORD GetPrivateProfileStringW(LPCWSTR section, LPCWSTR key, LPCWSTR def, LPWSTR out, DWORD max, LPCWSTR file)
{
	IniFile ini = readIni(iniPath(file));
	if (!section)
	{
		QStringList names;
		for (const auto& s : ini.sections)
			names << s.name;
		return copyList(names, out, max);
	}
	IniFile::Section* s = ini.find(qs(section));
	if (!key)
	{
		QStringList keys;
		if (s)
		{
			for (const auto& e : s->entries)
				keys << e.key;
		}
		return copyList(keys, out, max);
	}
	QString value = def ? qs(def) : QString();
	if (s)
	{
		for (const auto& e : s->entries)
		{
			if (e.key.compare(qs(key), Qt::CaseInsensitive) == 0)
			{
				value = e.value;
				if (value.size() >= 2 && ((value.startsWith(QLatin1Char('"')) && value.endsWith(QLatin1Char('"')))
					|| (value.startsWith(QLatin1Char('\'')) && value.endsWith(QLatin1Char('\'')))))
					value = value.mid(1, value.size() - 2);
				break;
			}
		}
	}
	return static_cast<DWORD>(copyOut(value, out, static_cast<int>(max)));
}

DWORD GetPrivateProfileStringA(LPCSTR section, LPCSTR key, LPCSTR def, LPSTR out, DWORD max, LPCSTR file)
{
	const std::wstring s = ws(qsA(section));
	const std::wstring k = ws(qsA(key));
	const std::wstring d = ws(qsA(def));
	const std::wstring f = ws(qsA(file));
	std::wstring buf(max + 1, L'\0');
	const DWORD n = GetPrivateProfileStringW(section ? s.c_str() : nullptr, key ? k.c_str() : nullptr, def ? d.c_str() : nullptr, buf.data(), max, f.c_str());
	return static_cast<DWORD>(copyOutA(qs(buf.c_str(), static_cast<int>(n)), out, static_cast<int>(max)));
}

UINT GetPrivateProfileIntW(LPCWSTR section, LPCWSTR key, INT def, LPCWSTR file)
{
	wchar_t buf[64] = {};
	GetPrivateProfileStringW(section, key, L"", buf, 64, file);
	const QString s = qs(buf).trimmed();
	if (s.isEmpty())
		return static_cast<UINT>(def);
	bool ok = false;
	int v = s.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive) ? s.mid(2).toInt(&ok, 16) : s.toInt(&ok);
	if (!ok)
	{
		// leading digits, like Windows
		QString digits;
		for (QChar c : s)
		{
			if (c.isDigit() || (digits.isEmpty() && c == QLatin1Char('-')))
				digits += c;
			else
				break;
		}
		v = digits.toInt(&ok);
		if (!ok)
			return 0;
	}
	return static_cast<UINT>(v);
}

UINT GetPrivateProfileIntA(LPCSTR section, LPCSTR key, INT def, LPCSTR file)
{
	const std::wstring s = ws(qsA(section));
	const std::wstring k = ws(qsA(key));
	const std::wstring f = ws(qsA(file));
	return GetPrivateProfileIntW(s.c_str(), k.c_str(), def, f.c_str());
}

BOOL WritePrivateProfileStringW(LPCWSTR section, LPCWSTR key, LPCWSTR value, LPCWSTR file)
{
	if (!section)
		return FALSE;
	const QString path = iniPath(file);
	IniFile ini = readIni(path);
	IniFile::Section* s = ini.find(qs(section));
	if (!key)
	{
		// remove the section
		if (s)
			ini.sections.erase(ini.sections.begin() + (s - ini.sections.data()));
		return writeIni(path, ini) ? TRUE : FALSE;
	}
	if (!s)
	{
		if (!value)
			return TRUE;
		ini.sections.push_back(IniFile::Section{qs(section), {}});
		s = &ini.sections.back();
	}
	auto it = std::find_if(s->entries.begin(), s->entries.end(), [&](const IniFile::Entry& e) { return e.key.compare(qs(key), Qt::CaseInsensitive) == 0; });
	if (!value)
	{
		if (it != s->entries.end())
			s->entries.erase(it);
	}
	else if (it != s->entries.end())
	{
		it->value = qs(value);
	}
	else
	{
		s->entries.push_back(IniFile::Entry{qs(key), qs(value)});
	}
	return writeIni(path, ini) ? TRUE : FALSE;
}

BOOL WritePrivateProfileStringA(LPCSTR section, LPCSTR key, LPCSTR value, LPCSTR file)
{
	const std::wstring s = ws(qsA(section));
	const std::wstring k = ws(qsA(key));
	const std::wstring v = ws(qsA(value));
	const std::wstring f = ws(qsA(file));
	return WritePrivateProfileStringW(section ? s.c_str() : nullptr, key ? k.c_str() : nullptr, value ? v.c_str() : nullptr, f.c_str());
}

DWORD GetPrivateProfileSectionW(LPCWSTR section, LPWSTR out, DWORD max, LPCWSTR file)
{
	IniFile ini = readIni(iniPath(file));
	QStringList lines;
	if (IniFile::Section* s = ini.find(qs(section)))
	{
		for (const auto& e : s->entries)
			lines << e.key + QLatin1Char('=') + e.value;
	}
	return copyList(lines, out, max);
}

DWORD GetPrivateProfileSectionNamesW(LPWSTR out, DWORD max, LPCWSTR file)
{
	return GetPrivateProfileStringW(nullptr, nullptr, nullptr, out, max, file);
}

BOOL WritePrivateProfileSectionW(LPCWSTR section, LPCWSTR data, LPCWSTR file)
{
	if (!section)
		return FALSE;
	const QString path = iniPath(file);
	IniFile ini = readIni(path);
	IniFile::Section* s = ini.find(qs(section));
	if (!s)
	{
		ini.sections.push_back(IniFile::Section{qs(section), {}});
		s = &ini.sections.back();
	}
	s->entries.clear();
	for (const wchar_t* p = data; p && *p; p += std::wcslen(p) + 1)
	{
		const QString line = qs(p);
		const int eq = static_cast<int>(line.indexOf(QLatin1Char('=')));
		if (eq > 0)
			s->entries.push_back(IniFile::Entry{line.left(eq), line.mid(eq + 1)});
	}
	return writeIni(path, ini) ? TRUE : FALSE;
}

BOOL GetPrivateProfileStructW(LPCWSTR section, LPCWSTR key, LPVOID data, UINT size, LPCWSTR file)
{
	std::wstring buf(size * 2 + 8, L'\0');
	GetPrivateProfileStringW(section, key, L"", buf.data(), static_cast<DWORD>(buf.size()), file);
	const QByteArray bytes = QByteArray::fromHex(qs(buf.c_str()).toLatin1());
	if (static_cast<UINT>(bytes.size()) < size)
		return FALSE;
	std::memcpy(data, bytes.constData(), size);
	return TRUE;
}

BOOL WritePrivateProfileStructW(LPCWSTR section, LPCWSTR key, LPVOID data, UINT size, LPCWSTR file)
{
	const QByteArray hex = QByteArray(static_cast<const char*>(data), static_cast<int>(size)).toHex().toUpper();
	const std::wstring w = ws(QString::fromLatin1(hex));
	return WritePrivateProfileStringW(section, key, w.c_str(), file);
}

// ---- modules ---------------------------------------------------------------------------------
DWORD GetModuleFileNameW(HMODULE hModule, LPWSTR path, DWORD max)
{
	QString filename = QCoreApplication::applicationFilePath();
	if (hModule)
	{
		link_map* module = nullptr;
		if (dlinfo(hModule, RTLD_DI_LINKMAP, &module) != 0 || !module)
			return 0;
		if (module->l_name && *module->l_name)
			filename = QFile::decodeName(module->l_name);
	}
	return static_cast<DWORD>(copyOut(filename, path, static_cast<int>(max)));
}

DWORD GetModuleFileNameA(HMODULE hModule, LPSTR path, DWORD max)
{
	wchar_t buf[MAX_PATH] = {};
	GetModuleFileNameW(hModule, buf, MAX_PATH);
	return static_cast<DWORD>(copyOutA(qs(buf), path, static_cast<int>(max)));
}

HMODULE GetModuleHandleW(LPCWSTR name)
{
	if (!name)
		return dlopen(nullptr, RTLD_LAZY);
	QString n = nativePath(name);
	if (n.endsWith(QStringLiteral(".dll"), Qt::CaseInsensitive))
		n = n.left(n.size() - 4) + QStringLiteral(".so");
	if (QFileInfo(moduleFile()).fileName().compare(QFileInfo(n).fileName(), Qt::CaseInsensitive) == 0)
		return moduleHandle();
	return dlopen(QFile::encodeName(n).constData(), RTLD_LAZY | RTLD_NOLOAD);
}

HMODULE GetModuleHandleA(LPCSTR name)
{
	if (!name)
		return GetModuleHandleW(nullptr);
	const std::wstring w = ws(qsA(name));
	return GetModuleHandleW(w.c_str());
}

BOOL GetModuleHandleExW(DWORD flags, LPCWSTR name, HMODULE* hModule)
{
	if (!hModule)
		return FALSE;
	if (flags & GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS)
	{
		Dl_info info{};
		if (!dladdr(reinterpret_cast<const void*>(name), &info) || !info.dli_fname)
			return FALSE;
		const QString path = QFile::decodeName(info.dli_fname);
		const bool executable = QFileInfo(path) == QFileInfo(QCoreApplication::applicationFilePath());
		*hModule = dlopen(executable ? nullptr : info.dli_fname, RTLD_LAZY | RTLD_NOLOAD);
		return *hModule ? TRUE : FALSE;
	}
	*hModule = GetModuleHandleW(name);
	return *hModule ? TRUE : FALSE;
}

HMODULE LoadLibraryW(LPCWSTR path)
{
	QString p = nativePath(path);
	if (p.endsWith(QStringLiteral(".dll"), Qt::CaseInsensitive))
		p = p.left(p.size() - 4) + QStringLiteral(".so");
	void* h = dlopen(QFile::encodeName(p).constData(), RTLD_NOW | RTLD_LOCAL);
	if (!h && !p.contains(QLatin1Char('/')))
		h = dlopen(QFile::encodeName(QStringLiteral("lib") + p).constData(), RTLD_NOW | RTLD_LOCAL);
	if (!h)
		g_lastError = ERROR_FILE_NOT_FOUND;
	return h;
}

HMODULE LoadLibraryA(LPCSTR path)
{
	const std::wstring w = ws(qsA(path));
	return LoadLibraryW(w.c_str());
}

HMODULE LoadLibraryExW(LPCWSTR path, HANDLE, DWORD)
{
	return LoadLibraryW(path);
}

FARPROC GetProcAddress(HMODULE hModule, LPCSTR name)
{
	if (!hModule || !name || isIntResource(name))
		return nullptr;
	return reinterpret_cast<FARPROC>(dlsym(hModule, name));
}

BOOL FreeLibrary(HMODULE hModule)
{
	if (!hModule || hModule == moduleHandle())
		return FALSE;
	return dlclose(hModule) == 0 ? TRUE : FALSE;
}

BOOL DisableThreadLibraryCalls(HMODULE)
{
	return TRUE;
}

DWORD GetLastError()
{
	return g_lastError;
}

void SetLastError(DWORD error)
{
	g_lastError = error;
}

namespace
{
	QString errorText(DWORD id)
	{
		switch (id)
		{
			case ERROR_SUCCESS: return QStringLiteral("The operation completed successfully.");
			case ERROR_FILE_NOT_FOUND: return QStringLiteral("The system cannot find the file specified.");
			case ERROR_PATH_NOT_FOUND: return QStringLiteral("The system cannot find the path specified.");
			case ERROR_ACCESS_DENIED: return QStringLiteral("Access is denied.");
			case ERROR_INVALID_HANDLE: return QStringLiteral("The handle is invalid.");
			case ERROR_NOT_ENOUGH_MEMORY: return QStringLiteral("Not enough memory resources are available to process this command.");
			case ERROR_ALREADY_EXISTS: return QStringLiteral("Cannot create a file when that file already exists.");
			case ERROR_INVALID_PARAMETER: return QStringLiteral("The parameter is incorrect.");
			default: return QStringLiteral("Error %1: %2").arg(id).arg(QString::fromLocal8Bit(std::strerror(static_cast<int>(id))));
		}
	}

	DWORD formatOut(DWORD flags, const QString& text, LPWSTR buffer, DWORD size)
	{
		const std::wstring w = ws(text + QStringLiteral("\r\n"));
		if (flags & FORMAT_MESSAGE_ALLOCATE_BUFFER)
		{
			auto* out = static_cast<wchar_t*>(LocalAlloc(LMEM_ZEROINIT, (w.size() + 1) * sizeof(wchar_t)));
			std::wmemcpy(out, w.c_str(), w.size() + 1);
			*reinterpret_cast<wchar_t**>(buffer) = out;
			return static_cast<DWORD>(w.size());
		}
		return static_cast<DWORD>(copyOut(qs(w.c_str()), buffer, static_cast<int>(size)));
	}
}

DWORD FormatMessageW(DWORD flags, LPCVOID source, DWORD id, DWORD, LPWSTR buffer, DWORD size, va_list*)
{
	if (flags & FORMAT_MESSAGE_FROM_STRING)
		return formatOut(flags, qs(static_cast<LPCWSTR>(source)), buffer, size);
	return formatOut(flags, errorText(id), buffer, size);
}

DWORD FormatMessageA(DWORD flags, LPCVOID source, DWORD id, DWORD lang, LPSTR buffer, DWORD size, va_list* args)
{
	if (flags & FORMAT_MESSAGE_ALLOCATE_BUFFER)
	{
		const QString t = (flags & FORMAT_MESSAGE_FROM_STRING) ? qsA(static_cast<LPCSTR>(source)) : errorText(id);
		const QByteArray u = (t + QStringLiteral("\r\n")).toUtf8();
		auto* out = static_cast<char*>(LocalAlloc(LMEM_ZEROINIT, static_cast<SIZE_T>(u.size()) + 1));
		std::memcpy(out, u.constData(), static_cast<size_t>(u.size()));
		*reinterpret_cast<char**>(buffer) = out;
		return static_cast<DWORD>(u.size());
	}
	std::wstring w(size + 1, L'\0');
	FormatMessageW(flags & ~FORMAT_MESSAGE_FROM_STRING, source, id, lang, w.data(), size, args);
	return static_cast<DWORD>(copyOutA(qs(w.c_str()), buffer, static_cast<int>(size)));
}

void OutputDebugStringW(LPCWSTR s)
{
	qDebug("%s", qPrintable(qs(s)));
}

void OutputDebugStringA(LPCSTR s)
{
	qDebug("%s", s ? s : "");
}

BOOL IsDebuggerPresent()
{
	return FALSE;
}

void DebugBreak()
{
	raise(SIGTRAP);
}

// ---- time -------------------------------------------------------------------------------------
DWORD GetTickCount()
{
	return static_cast<DWORD>(GetTickCount64());
}

ULONGLONG GetTickCount64()
{
	return static_cast<ULONGLONG>(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());
}

BOOL QueryPerformanceCounter(LARGE_INTEGER* counter)
{
	if (!counter)
		return FALSE;
	counter->QuadPart = std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
	return TRUE;
}

BOOL QueryPerformanceFrequency(LARGE_INTEGER* frequency)
{
	if (!frequency)
		return FALSE;
	frequency->QuadPart = 1000000000LL;
	return TRUE;
}

namespace
{
	void toSystemTime(const QDateTime& dt, LPSYSTEMTIME st)
	{
		st->wYear = static_cast<WORD>(dt.date().year());
		st->wMonth = static_cast<WORD>(dt.date().month());
		st->wDayOfWeek = static_cast<WORD>(dt.date().dayOfWeek() % 7);
		st->wDay = static_cast<WORD>(dt.date().day());
		st->wHour = static_cast<WORD>(dt.time().hour());
		st->wMinute = static_cast<WORD>(dt.time().minute());
		st->wSecond = static_cast<WORD>(dt.time().second());
		st->wMilliseconds = static_cast<WORD>(dt.time().msec());
	}
}

void GetLocalTime(LPSYSTEMTIME st)
{
	if (st)
		toSystemTime(QDateTime::currentDateTime(), st);
}

void GetSystemTime(LPSYSTEMTIME st)
{
	if (st)
		toSystemTime(QDateTime::currentDateTimeUtc(), st);
}

void GetSystemTimeAsFileTime(LPFILETIME ft)
{
	SYSTEMTIME st;
	GetSystemTime(&st);
	SystemTimeToFileTime(&st, ft);
}

void Sleep(DWORD ms)
{
	std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

DWORD SleepEx(DWORD ms, BOOL)
{
	Sleep(ms);
	return 0;
}

// ---- threads and synchronization ---------------------------------------------------------------
DWORD GetCurrentProcessId()
{
	return static_cast<DWORD>(getpid());
}

DWORD GetCurrentThreadId()
{
	return static_cast<DWORD>(std::hash<std::thread::id>{}(std::this_thread::get_id()) & 0xFFFFFFFF);
}

HANDLE GetCurrentProcess()
{
	return reinterpret_cast<HANDLE>(static_cast<intptr_t>(-1));
}

HANDLE GetCurrentThread()
{
	return reinterpret_cast<HANDLE>(static_cast<intptr_t>(-2));
}

namespace
{
	void signal(KObject* k)
	{
		std::lock_guard<std::mutex> lock(k->m);
		k->signaled = true;
		k->cv.notify_all();
	}
}

HANDLE CreateThread(LPSECURITY_ATTRIBUTES, SIZE_T, LPTHREAD_START_ROUTINE start, LPVOID param, DWORD, LPDWORD id)
{
	if (!start)
		return nullptr;
	auto* k = new KObject(KType::Thread);
	std::thread t([k, start, param]
	{
		const DWORD code = start(param);
		{
			std::lock_guard<std::mutex> lock(k->m);
			k->exitCode = code;
		}
		signal(k);
	});
	if (id)
		*id = static_cast<DWORD>(std::hash<std::thread::id>{}(t.get_id()) & 0xFFFFFFFF);
	t.detach();
	return k;
}

void ExitThread(DWORD)
{
	pthread_exit(nullptr);
}

BOOL TerminateThread(HANDLE, DWORD)
{
	return FALSE;
}

BOOL GetExitCodeThread(HANDLE h, LPDWORD code)
{
	KObject* k = kobj(h);
	if (!k || !code)
		return FALSE;
	std::lock_guard<std::mutex> lock(k->m);
	*code = k->exitCode;
	return TRUE;
}

DWORD ResumeThread(HANDLE)
{
	return 0;
}

DWORD SuspendThread(HANDLE)
{
	return 0;
}

BOOL SetThreadPriority(HANDLE, int)
{
	return TRUE;
}

int GetThreadPriority(HANDLE)
{
	return THREAD_PRIORITY_NORMAL;
}

uintptr_t _beginthreadex(void*, unsigned, unsigned (*start)(void*), void* arg, unsigned, unsigned* id)
{
	if (!start)
		return 0;
	auto* k = new KObject(KType::Thread);
	std::thread t([k, start, arg]
	{
		const unsigned code = start(arg);
		{
			std::lock_guard<std::mutex> lock(k->m);
			k->exitCode = code;
		}
		signal(k);
	});
	if (id)
		*id = static_cast<unsigned>(std::hash<std::thread::id>{}(t.get_id()) & 0xFFFFFFFF);
	t.detach();
	return reinterpret_cast<uintptr_t>(k);
}

uintptr_t _beginthread(void (*start)(void*), unsigned, void* arg)
{
	if (!start)
		return static_cast<uintptr_t>(-1);
	std::thread(start, arg).detach();
	return 1;
}

void _endthreadex(unsigned)
{
	pthread_exit(nullptr);
}

namespace
{
	bool processDone(KObject* k)
	{
		if (k->signaled)
			return true;
		int status = 0;
		const pid_t r = waitpid(k->pid, &status, WNOHANG);
		if (r == k->pid)
		{
			k->exitCode = WIFEXITED(status) ? static_cast<DWORD>(WEXITSTATUS(status)) : static_cast<DWORD>(128 + WTERMSIG(status));
			k->signaled = true;
			return true;
		}
		if (r < 0)
		{
			k->signaled = true;
			if (k->exitCode == STILL_ACTIVE)
				k->exitCode = 0;
			return true;
		}
		return false;
	}

	bool tryAcquire(KObject* k)
	{
		// called with k->m locked
		switch (k->type)
		{
			case KType::Event:
				if (!k->signaled)
					return false;
				if (!k->manualReset)
					k->signaled = false;
				return true;
			case KType::Mutex:
				if (k->recursion == 0 || k->owner == std::this_thread::get_id())
				{
					k->owner = std::this_thread::get_id();
					++k->recursion;
					return true;
				}
				return false;
			case KType::Semaphore:
				if (k->count > 0)
				{
					--k->count;
					return true;
				}
				return false;
			default:
				return k->signaled;
		}
	}
}

DWORD WaitForSingleObject(HANDLE h, DWORD ms)
{
	KObject* k = kobj(h);
	if (!k)
		return WAIT_FAILED;
	const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms == INFINITE ? 0 : ms);
	if (k->type == KType::Process)
	{
		while (!processDone(k))
		{
			if (ms != INFINITE && std::chrono::steady_clock::now() >= deadline)
				return WAIT_TIMEOUT;
			std::this_thread::sleep_for(std::chrono::milliseconds(5));
		}
		return WAIT_OBJECT_0;
	}
	if (k->type == KType::Change)
	{
		if (ms != INFINITE)
			std::this_thread::sleep_for(std::chrono::milliseconds(std::min<DWORD>(ms, 1000)));
		return WAIT_TIMEOUT;
	}
	std::unique_lock<std::mutex> lock(k->m);
	if (ms == INFINITE)
	{
		k->cv.wait(lock, [k] { return tryAcquire(k); });
		return WAIT_OBJECT_0;
	}
	return k->cv.wait_until(lock, deadline, [k] { return tryAcquire(k); }) ? WAIT_OBJECT_0 : WAIT_TIMEOUT;
}

DWORD WaitForMultipleObjects(DWORD count, const HANDLE* handles, BOOL all, DWORD ms)
{
	const auto start = std::chrono::steady_clock::now();
	for (;;)
	{
		DWORD done = 0;
		for (DWORD i = 0; i < count; ++i)
		{
			if (WaitForSingleObject(handles[i], 0) == WAIT_OBJECT_0)
			{
				if (!all)
					return WAIT_OBJECT_0 + i;
				++done;
			}
		}
		if (all && done == count)
			return WAIT_OBJECT_0;
		if (ms != INFINITE && std::chrono::steady_clock::now() - start >= std::chrono::milliseconds(ms))
			return WAIT_TIMEOUT;
		std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}
}

DWORD MsgWaitForMultipleObjects(DWORD count, const HANDLE* handles, BOOL all, DWORD ms, DWORD)
{
	QCoreApplication::processEvents();
	const DWORD r = WaitForMultipleObjects(count, handles, all, std::min<DWORD>(ms, 50));
	return (r == WAIT_TIMEOUT && ms != 0) ? WAIT_OBJECT_0 + count : r;
}

HANDLE CreateEventW(LPSECURITY_ATTRIBUTES, BOOL manualReset, BOOL initialState, LPCWSTR name)
{
	std::lock_guard<std::mutex> lock(namedMutex());
	if (name && *name)
	{
		auto it = namedObjects().find(name);
		if (it != namedObjects().end())
		{
			++it->second->refs;
			g_lastError = ERROR_ALREADY_EXISTS;
			return it->second;
		}
	}
	auto* k = new KObject(KType::Event);
	k->manualReset = manualReset != FALSE;
	k->signaled = initialState != FALSE;
	if (name && *name)
	{
		k->name = name;
		namedObjects()[k->name] = k;
	}
	g_lastError = ERROR_SUCCESS;
	return k;
}

HANDLE OpenEventW(DWORD, BOOL, LPCWSTR name)
{
	std::lock_guard<std::mutex> lock(namedMutex());
	auto it = name ? namedObjects().find(name) : namedObjects().end();
	if (it == namedObjects().end() || it->second->type != KType::Event)
	{
		g_lastError = ERROR_FILE_NOT_FOUND;
		return nullptr;
	}
	++it->second->refs;
	return it->second;
}

BOOL SetEvent(HANDLE h)
{
	KObject* k = kobj(h);
	if (!k)
		return FALSE;
	signal(k);
	return TRUE;
}

BOOL ResetEvent(HANDLE h)
{
	KObject* k = kobj(h);
	if (!k)
		return FALSE;
	std::lock_guard<std::mutex> lock(k->m);
	k->signaled = false;
	return TRUE;
}

HANDLE CreateMutexW(LPSECURITY_ATTRIBUTES, BOOL initialOwner, LPCWSTR)
{
	auto* k = new KObject(KType::Mutex);
	if (initialOwner)
	{
		k->owner = std::this_thread::get_id();
		k->recursion = 1;
	}
	return k;
}

BOOL ReleaseMutex(HANDLE h)
{
	KObject* k = kobj(h);
	if (!k)
		return FALSE;
	std::lock_guard<std::mutex> lock(k->m);
	if (k->recursion > 0)
		--k->recursion;
	k->cv.notify_all();
	return TRUE;
}

HANDLE CreateSemaphoreW(LPSECURITY_ATTRIBUTES, LONG initial, LONG maximum, LPCWSTR)
{
	auto* k = new KObject(KType::Semaphore);
	k->count = initial;
	k->maximum = maximum;
	return k;
}

BOOL ReleaseSemaphore(HANDLE h, LONG count, LPLONG previous)
{
	KObject* k = kobj(h);
	if (!k)
		return FALSE;
	std::lock_guard<std::mutex> lock(k->m);
	if (previous)
		*previous = k->count;
	k->count = std::min(k->count + count, k->maximum);
	k->cv.notify_all();
	return TRUE;
}

void InitializeCriticalSection(LPCRITICAL_SECTION cs)
{
	if (cs)
		cs->impl = new std::recursive_mutex;
}

BOOL InitializeCriticalSectionAndSpinCount(LPCRITICAL_SECTION cs, DWORD)
{
	InitializeCriticalSection(cs);
	return TRUE;
}

BOOL InitializeCriticalSectionEx(LPCRITICAL_SECTION cs, DWORD, DWORD)
{
	InitializeCriticalSection(cs);
	return TRUE;
}

void DeleteCriticalSection(LPCRITICAL_SECTION cs)
{
	if (cs)
	{
		delete static_cast<std::recursive_mutex*>(cs->impl);
		cs->impl = nullptr;
	}
}

void EnterCriticalSection(LPCRITICAL_SECTION cs)
{
	if (cs && !cs->impl)
		cs->impl = new std::recursive_mutex;
	if (cs)
		static_cast<std::recursive_mutex*>(cs->impl)->lock();
}

BOOL TryEnterCriticalSection(LPCRITICAL_SECTION cs)
{
	if (cs && !cs->impl)
		cs->impl = new std::recursive_mutex;
	return (cs && static_cast<std::recursive_mutex*>(cs->impl)->try_lock()) ? TRUE : FALSE;
}

void LeaveCriticalSection(LPCRITICAL_SECTION cs)
{
	if (cs && cs->impl)
		static_cast<std::recursive_mutex*>(cs->impl)->unlock();
}

namespace
{
	std::shared_mutex* srw(PSRWLOCK lock)
	{
		static std::mutex init;
		std::lock_guard<std::mutex> g(init);
		if (!lock->impl)
			lock->impl = new std::shared_mutex;
		return static_cast<std::shared_mutex*>(lock->impl);
	}
}

void InitializeSRWLock(PSRWLOCK lock)
{
	if (lock)
		lock->impl = new std::shared_mutex;
}

void AcquireSRWLockExclusive(PSRWLOCK lock) { srw(lock)->lock(); }
void ReleaseSRWLockExclusive(PSRWLOCK lock) { srw(lock)->unlock(); }
void AcquireSRWLockShared(PSRWLOCK lock) { srw(lock)->lock_shared(); }
void ReleaseSRWLockShared(PSRWLOCK lock) { srw(lock)->unlock_shared(); }

LONG InterlockedIncrement(LONG volatile* v) { return __atomic_add_fetch(v, 1, __ATOMIC_SEQ_CST); }
LONG InterlockedDecrement(LONG volatile* v) { return __atomic_sub_fetch(v, 1, __ATOMIC_SEQ_CST); }
LONG InterlockedExchange(LONG volatile* v, LONG value) { return __atomic_exchange_n(v, value, __ATOMIC_SEQ_CST); }
LONG InterlockedCompareExchange(LONG volatile* v, LONG exchange, LONG comparand)
{
	__atomic_compare_exchange_n(v, &comparand, exchange, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
	return comparand;
}
LONG InterlockedExchangeAdd(LONG volatile* v, LONG value) { return __atomic_fetch_add(v, value, __ATOMIC_SEQ_CST); }
void* InterlockedExchangePointer(void* volatile* v, void* value) { return __atomic_exchange_n(v, value, __ATOMIC_SEQ_CST); }
void* InterlockedCompareExchangePointer(void* volatile* v, void* exchange, void* comparand)
{
	__atomic_compare_exchange_n(v, &comparand, exchange, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
	return comparand;
}

// ---- processes and pipes --------------------------------------------------------------------
BOOL CreateProcessW(LPCWSTR app, LPWSTR cmdLine, LPSECURITY_ATTRIBUTES, LPSECURITY_ATTRIBUTES, BOOL, DWORD creationFlags, LPVOID,
	LPCWSTR dir, LPSTARTUPINFOW si, LPPROCESS_INFORMATION pi)
{
	// the command line runs in the shell (Windows programs of plugins: the user sets Linux ones)
	QString command = cmdLine ? qs(cmdLine) : qs(app);
	if (app && cmdLine && !command.contains(qs(app)))
		command = QStringLiteral("\"%1\" %2").arg(nativePath(app), command);
	const QByteArray cmd = command.toLocal8Bit();
	const QByteArray workDir = dir ? QFile::encodeName(nativePath(dir)) : QByteArray();
	int in = -1, out = -1, err = -1;
	if (si && (si->dwFlags & STARTF_USESTDHANDLES))
	{
		if (KObject* k = kobj(si->hStdInput))
			in = k->fd;
		if (KObject* k = kobj(si->hStdOutput))
			out = k->fd;
		if (KObject* k = kobj(si->hStdError))
			err = k->fd;
	}
	const pid_t pid = fork();
	if (pid < 0)
	{
		setErrno();
		return FALSE;
	}
	if (pid == 0)
	{
		if (in >= 0)
			dup2(in, 0);
		if (out >= 0)
			dup2(out, 1);
		if (err >= 0)
			dup2(err, 2);
		if (!workDir.isEmpty() && chdir(workDir.constData()) != 0)
			_exit(127);
		// CREATE_NEW_PROCESS_GROUP: GenerateConsoleCtrlEvent reaches the shell and what it runs
		if (creationFlags & CREATE_NEW_PROCESS_GROUP)
			setpgid(0, 0);
		execl("/bin/sh", "sh", "-c", cmd.constData(), static_cast<char*>(nullptr));
		_exit(127);
	}
	auto* k = new KObject(KType::Process);
	k->pid = pid;
	if (pi)
	{
		pi->hProcess = k;
		pi->hThread = new KObject(KType::Thread);
		pi->dwProcessId = static_cast<DWORD>(pid);
		pi->dwThreadId = static_cast<DWORD>(pid);
	}
	return TRUE;
}

BOOL GetExitCodeProcess(HANDLE h, LPDWORD code)
{
	KObject* k = kobj(h);
	if (!k || !code || k->type != KType::Process)
		return FALSE;
	processDone(k);
	*code = k->exitCode;
	return TRUE;
}

BOOL TerminateProcess(HANDLE h, UINT code)
{
	KObject* k = kobj(h);
	if (!k || k->type != KType::Process)
		return FALSE;
	kill(k->pid, SIGKILL);
	processDone(k);
	k->exitCode = code;
	return TRUE;
}

DWORD GetProcessId(HANDLE h)
{
	KObject* k = kobj(h);
	return k ? static_cast<DWORD>(k->pid) : 0;
}

HANDLE OpenProcess(DWORD, BOOL, DWORD pid)
{
	auto* k = new KObject(KType::Process);
	k->pid = static_cast<pid_t>(pid);
	return k;
}

BOOL CreatePipe(PHANDLE readPipe, PHANDLE writePipe, LPSECURITY_ATTRIBUTES, DWORD)
{
	int fds[2];
	if (pipe(fds) != 0)
	{
		setErrno();
		return FALSE;
	}
	auto* r = new KObject(KType::Pipe);
	r->fd = fds[0];
	auto* w = new KObject(KType::Pipe);
	w->fd = fds[1];
	*readPipe = r;
	*writePipe = w;
	return TRUE;
}

BOOL PeekNamedPipe(HANDLE h, LPVOID, DWORD, LPDWORD read, LPDWORD available, LPDWORD left)
{
	KObject* k = kobj(h);
	if (!k || k->fd < 0)
		return FALSE;
	int n = 0;
	if (ioctl(k->fd, FIONREAD, &n) != 0)
		return FALSE;
	if (read)
		*read = 0;
	if (available)
		*available = static_cast<DWORD>(n);
	if (left)
		*left = 0;
	return TRUE;
}

BOOL SetHandleInformation(HANDLE h, DWORD mask, DWORD flags)
{
	KObject* k = kobj(h);
	if (!k || k->fd < 0)
		return TRUE;
	if (mask & HANDLE_FLAG_INHERIT)
	{
		const int fl = fcntl(k->fd, F_GETFD);
		fcntl(k->fd, F_SETFD, (flags & HANDLE_FLAG_INHERIT) ? (fl & ~FD_CLOEXEC) : (fl | FD_CLOEXEC));
	}
	return TRUE;
}

BOOL DuplicateHandle(HANDLE, HANDLE src, HANDLE, LPHANDLE dst, DWORD, BOOL, DWORD)
{
	KObject* k = kobj(src);
	if (!k || !dst)
		return FALSE;
	if (k->fd >= 0)
	{
		auto* d = new KObject(k->type);
		d->fd = dup(k->fd);
		*dst = d;
		return TRUE;
	}
	*dst = src;
	return TRUE;
}

HANDLE GetStdHandle(DWORD which)
{
	static KObject* handles[3] = {};
	const int fd = (which == STD_INPUT_HANDLE) ? 0 : (which == STD_OUTPUT_HANDLE ? 1 : 2);
	if (!handles[fd])
	{
		handles[fd] = new KObject(KType::File);
		handles[fd]->fd = fd;
	}
	return handles[fd];
}

DWORD GetEnvironmentVariableW(LPCWSTR name, LPWSTR buffer, DWORD max)
{
	const QByteArray n = qs(name).toLocal8Bit();
	const char* v = getenv(n.constData());
	if (!v)
	{
		g_lastError = 203; // ERROR_ENVVAR_NOT_FOUND
		return 0;
	}
	const QString value = QString::fromLocal8Bit(v);
	const DWORD len = static_cast<DWORD>(ws(value).size());
	if (!buffer || max <= len)
		return len + 1;
	return static_cast<DWORD>(copyOut(value, buffer, static_cast<int>(max)));
}

BOOL SetEnvironmentVariableW(LPCWSTR name, LPCWSTR value)
{
	const QByteArray n = qs(name).toLocal8Bit();
	if (!value)
		return unsetenv(n.constData()) == 0 ? TRUE : FALSE;
	return setenv(n.constData(), qs(value).toLocal8Bit().constData(), 1) == 0 ? TRUE : FALSE;
}

DWORD ExpandEnvironmentStringsW(LPCWSTR src, LPWSTR dst, DWORD max)
{
	QString s = qs(src);
	static const QRegularExpression re(QStringLiteral("%([^%]+)%"));
	QString out;
	int last = 0;
	auto it = re.globalMatch(s);
	while (it.hasNext())
	{
		const auto m = it.next();
		out += s.mid(last, m.capturedStart() - last);
		QString name = m.captured(1);
		QString value;
		if (name.compare(QStringLiteral("APPDATA"), Qt::CaseInsensitive) == 0)
			value = QDir::homePath() + QStringLiteral("/.config");
		else if (name.compare(QStringLiteral("LOCALAPPDATA"), Qt::CaseInsensitive) == 0)
			value = QDir::homePath() + QStringLiteral("/.local/share");
		else if (name.compare(QStringLiteral("USERPROFILE"), Qt::CaseInsensitive) == 0)
			value = QDir::homePath();
		else if (name.compare(QStringLiteral("TEMP"), Qt::CaseInsensitive) == 0 || name.compare(QStringLiteral("TMP"), Qt::CaseInsensitive) == 0)
			value = QDir::tempPath();
		else
		{
			const char* v = getenv(name.toLocal8Bit().constData());
			value = v ? QString::fromLocal8Bit(v) : m.captured(0);
		}
		out += value;
		last = static_cast<int>(m.capturedEnd());
	}
	out += s.mid(last);
	const DWORD len = static_cast<DWORD>(ws(out).size()) + 1;
	if (!dst || max < len)
		return len;
	copyOut(out, dst, static_cast<int>(max));
	return len;
}

LPWSTR GetCommandLineW()
{
	static std::wstring line = ws(QCoreApplication::arguments().join(QLatin1Char(' ')));
	return line.data();
}

LPWSTR GetEnvironmentStringsW()
{
	QString all;
	for (char** e = environ; e && *e; ++e)
		all += QString::fromLocal8Bit(*e) + QChar(0);
	all += QChar(0);
	const std::wstring w = ws(all);
	auto* out = static_cast<wchar_t*>(LocalAlloc(0, (w.size() + 1) * sizeof(wchar_t)));
	std::wmemcpy(out, w.data(), w.size());
	out[w.size()] = 0;
	return out;
}

BOOL FreeEnvironmentStringsW(LPWSTR env)
{
	LocalFree(env);
	return TRUE;
}

BOOL GetUserNameW(LPWSTR buffer, LPDWORD size)
{
	const char* u = getenv("USER");
	const QString name = u ? QString::fromLocal8Bit(u) : QStringLiteral("user");
	if (!size)
		return FALSE;
	const DWORD len = static_cast<DWORD>(ws(name).size()) + 1;
	if (!buffer || *size < len)
	{
		*size = len;
		g_lastError = ERROR_INSUFFICIENT_BUFFER;
		return FALSE;
	}
	copyOut(name, buffer, static_cast<int>(*size));
	*size = len;
	return TRUE;
}

BOOL GetComputerNameW(LPWSTR buffer, LPDWORD size)
{
	char host[256] = {};
	gethostname(host, sizeof(host) - 1);
	const QString name = QString::fromLocal8Bit(host);
	if (!size)
		return FALSE;
	const DWORD len = static_cast<DWORD>(ws(name).size());
	if (!buffer || *size <= len)
	{
		*size = len + 1;
		return FALSE;
	}
	copyOut(name, buffer, static_cast<int>(*size));
	*size = len;
	return TRUE;
}

void GetSystemInfo(SYSTEM_INFO* si)
{
	if (!si)
		return;
	*si = SYSTEM_INFO{};
	si->wProcessorArchitecture = 9; // PROCESSOR_ARCHITECTURE_AMD64
	si->dwPageSize = static_cast<DWORD>(sysconf(_SC_PAGESIZE));
	si->dwNumberOfProcessors = static_cast<DWORD>(std::max(1u, std::thread::hardware_concurrency()));
	si->dwAllocationGranularity = 65536;
}

void GetNativeSystemInfo(SYSTEM_INFO* si)
{
	GetSystemInfo(si);
}

BOOL GetVersionExW(OSVERSIONINFOW* vi)
{
	if (!vi)
		return FALSE;
	vi->dwMajorVersion = 10;
	vi->dwMinorVersion = 0;
	vi->dwBuildNumber = 19045;
	vi->dwPlatformId = 2;
	vi->szCSDVersion[0] = 0;
	if (vi->dwOSVersionInfoSize >= sizeof(OSVERSIONINFOEXW))
	{
		auto* ex = reinterpret_cast<OSVERSIONINFOEXW*>(vi);
		ex->wServicePackMajor = 0;
		ex->wServicePackMinor = 0;
		ex->wSuiteMask = 0;
		ex->wProductType = VER_NT_WORKSTATION;
		ex->wReserved = 0;
	}
	return TRUE;
}

BOOL GetProductInfo(DWORD, DWORD, DWORD, DWORD, PDWORD productType)
{
	if (!productType)
		return FALSE;
	*productType = PRODUCT_PROFESSIONAL;
	return TRUE;
}

BOOL IsTextUnicode(const void* buffer, int size, LPINT result)
{
	const auto* b = static_cast<const unsigned char*>(buffer);
	const int asked = result ? *result : 0xFFFF;
	int found = 0;
	if (!b || size < 2)
	{
		if (result)
			*result = 0;
		return FALSE;
	}
	if (size % 2)
		found |= IS_TEXT_UNICODE_ODD_LENGTH;
	if (b[0] == 0xFF && b[1] == 0xFE)
		found |= IS_TEXT_UNICODE_SIGNATURE;
	if (b[0] == 0xFE && b[1] == 0xFF)
		found |= IS_TEXT_UNICODE_REVERSE_SIGNATURE;
	// statistics: ASCII text in UTF-16 LE has nuls in the odd bytes, not in the even ones
	int evenNul = 0, oddNul = 0, controls = 0;
	const int pairs = size / 2;
	for (int i = 0; i < pairs; ++i)
	{
		const unsigned lo = b[2 * i], hi = b[2 * i + 1];
		if (!lo)
			++evenNul;
		if (!hi)
			++oddNul;
		const unsigned ch = lo | (hi << 8);
		if (ch == '\r' || ch == '\n' || ch == '\t' || ch == ' ')
			++controls;
		if (ch == 0xFFFF || ch == 0xFFFE)
			found |= IS_TEXT_UNICODE_ILLEGAL_CHARS;
	}
	if (oddNul > pairs / 2 && evenNul < pairs / 8)
		found |= IS_TEXT_UNICODE_STATISTICS | IS_TEXT_UNICODE_ASCII16;
	if (evenNul > pairs / 2 && oddNul < pairs / 8)
		found |= IS_TEXT_UNICODE_REVERSE_STATISTICS | IS_TEXT_UNICODE_REVERSE_ASCII16;
	if (controls)
		found |= IS_TEXT_UNICODE_CONTROLS;
	// single bytes text: a nul byte is not text
	for (int i = 0; i < size; ++i)
	{
		if (!b[i])
		{
			found |= IS_TEXT_UNICODE_NULL_BYTES;
			break;
		}
	}
	if (result)
		*result = found & asked;
	const bool unicode = (found & (IS_TEXT_UNICODE_SIGNATURE | IS_TEXT_UNICODE_STATISTICS | IS_TEXT_UNICODE_ASCII16))
		&& !(found & (IS_TEXT_UNICODE_ODD_LENGTH | IS_TEXT_UNICODE_ILLEGAL_CHARS | IS_TEXT_UNICODE_REVERSE_SIGNATURE));
	return unicode ? TRUE : FALSE;
}

DWORD GetVersion()
{
	return 10 | (0 << 8) | (19045u << 16);
}

// ---- version resources ------------------------------------------------------------------------
namespace
{
	struct VersionBlock
	{
		unsigned int magic = 0x4E505056u; // "NPPV"
		VS_FIXEDFILEINFO fixed{};
		std::map<std::wstring, std::wstring> strings;
	};
}

DWORD GetFileVersionInfoSizeW(LPCWSTR file, LPDWORD handle)
{
	if (handle)
		*handle = 0;
	const QString f = QFileInfo(nativePath(file)).fileName();
	const QString me = QFileInfo(moduleFile()).fileName();
	if (!versionInfo() || (f.compare(me, Qt::CaseInsensitive) != 0 && QFileInfo(f).completeBaseName().compare(QFileInfo(me).completeBaseName(), Qt::CaseInsensitive) != 0))
		return 0;
	return sizeof(VersionBlock);
}

BOOL GetFileVersionInfoW(LPCWSTR, DWORD, DWORD size, LPVOID data)
{
	const NppRcVersion* v = versionInfo();
	if (!v || !data || size < sizeof(VersionBlock))
		return FALSE;
	auto* b = new (data) VersionBlock;
	b->fixed.dwSignature = 0xFEEF04BD;
	b->fixed.dwFileVersionMS = v->fileVersionMS;
	b->fixed.dwFileVersionLS = v->fileVersionLS;
	b->fixed.dwProductVersionMS = v->productVersionMS;
	b->fixed.dwProductVersionLS = v->productVersionLS;
	for (int i = 0; i < v->count; ++i)
	{
		const wchar_t* key = v->strings[i].text;
		const wchar_t* value = key + std::wcslen(key) + 1;
		b->strings[key] = value;
	}
	return TRUE;
}

BOOL VerQueryValueW(LPCVOID block, LPCWSTR subBlock, LPVOID* buffer, PUINT len)
{
	auto* b = static_cast<const VersionBlock*>(block);
	if (!b || b->magic != 0x4E505056u || !subBlock || !buffer)
		return FALSE;
	const QString sub = qs(subBlock);
	if (sub == QStringLiteral("\\") || sub.isEmpty())
	{
		*buffer = const_cast<VS_FIXEDFILEINFO*>(&b->fixed);
		if (len)
			*len = sizeof(VS_FIXEDFILEINFO);
		return TRUE;
	}
	if (sub.startsWith(QStringLiteral("\\VarFileInfo\\Translation")))
	{
		static DWORD translation = MAKELONG(0x0409, 1200);
		*buffer = &translation;
		if (len)
			*len = sizeof(translation);
		return TRUE;
	}
	const std::wstring key = ws(sub.section(QLatin1Char('\\'), -1));
	auto it = b->strings.find(key);
	if (it == b->strings.end())
		return FALSE;
	*buffer = const_cast<wchar_t*>(it->second.c_str());
	if (len)
		*len = static_cast<UINT>(it->second.size()) + 1;
	return TRUE;
}

// ---- registry -----------------------------------------------------------------------------------
LONG RegOpenKeyExW(HKEY key, LPCWSTR subKey, DWORD, ACCESS_MASK, PHKEY result)
{
	if (!result)
		return ERROR_INVALID_PARAMETER;
	const QString path = keyPath(key, subKey);
	QSettings& reg = registry();
	if (!reg.childGroups().isEmpty() || true)
	{
		const QStringList all = reg.allKeys();
		bool exists = false;
		for (const QString& k : all)
		{
			if (k.startsWith(path + QLatin1Char('/'), Qt::CaseInsensitive) || k.compare(path, Qt::CaseInsensitive) == 0)
			{
				exists = true;
				break;
			}
		}
		if (!exists)
			return ERROR_FILE_NOT_FOUND;
	}
	auto* k = new RegKey;
	k->path = path;
	*result = k;
	return ERROR_SUCCESS;
}

LONG RegCreateKeyExW(HKEY key, LPCWSTR subKey, DWORD, LPWSTR, DWORD, ACCESS_MASK, LPSECURITY_ATTRIBUTES, PHKEY result, LPDWORD disposition)
{
	if (!result)
		return ERROR_INVALID_PARAMETER;
	auto* k = new RegKey;
	k->path = keyPath(key, subKey);
	*result = k;
	if (disposition)
		*disposition = REG_OPENED_EXISTING_KEY;
	return ERROR_SUCCESS;
}

LONG RegQueryValueExW(HKEY key, LPCWSTR name, LPDWORD, LPDWORD type, LPBYTE data, LPDWORD size)
{
	QSettings& reg = registry();
	const QString k = keyPath(key, nullptr) + QLatin1Char('/') + (name && *name ? qs(name) : QStringLiteral("(Default)"));
	if (!reg.contains(k))
		return ERROR_FILE_NOT_FOUND;
	const QVariant v = reg.value(k);
	if (v.typeId() == QMetaType::Int || v.typeId() == QMetaType::UInt || v.typeId() == QMetaType::LongLong)
	{
		if (type)
			*type = REG_DWORD;
		if (size)
		{
			if (data && *size >= 4)
			{
				const DWORD d = static_cast<DWORD>(v.toLongLong());
				std::memcpy(data, &d, 4);
			}
			else if (data)
			{
				*size = 4;
				return ERROR_MORE_DATA;
			}
			*size = 4;
		}
		return ERROR_SUCCESS;
	}
	if (v.typeId() == QMetaType::QByteArray)
	{
		const QByteArray b = v.toByteArray();
		if (type)
			*type = REG_BINARY;
		if (size)
		{
			if (data && *size >= static_cast<DWORD>(b.size()))
				std::memcpy(data, b.constData(), static_cast<size_t>(b.size()));
			else if (data)
			{
				*size = static_cast<DWORD>(b.size());
				return ERROR_MORE_DATA;
			}
			*size = static_cast<DWORD>(b.size());
		}
		return ERROR_SUCCESS;
	}
	const std::wstring s = ws(v.toString());
	const DWORD bytes = static_cast<DWORD>((s.size() + 1) * sizeof(wchar_t));
	if (type)
		*type = REG_SZ;
	if (size)
	{
		if (data && *size >= bytes)
			std::memcpy(data, s.c_str(), bytes);
		else if (data)
		{
			*size = bytes;
			return ERROR_MORE_DATA;
		}
		*size = bytes;
	}
	return ERROR_SUCCESS;
}

LONG RegSetValueExW(HKEY key, LPCWSTR name, DWORD, DWORD type, const BYTE* data, DWORD size)
{
	QSettings& reg = registry();
	const QString k = keyPath(key, nullptr) + QLatin1Char('/') + (name && *name ? qs(name) : QStringLiteral("(Default)"));
	switch (type)
	{
		case REG_DWORD:
		{
			DWORD d = 0;
			if (data && size >= 4)
				std::memcpy(&d, data, 4);
			reg.setValue(k, static_cast<qlonglong>(d));
			break;
		}
		case REG_SZ:
		case REG_EXPAND_SZ:
			reg.setValue(k, qs(reinterpret_cast<const wchar_t*>(data)));
			break;
		default:
			reg.setValue(k, QByteArray(reinterpret_cast<const char*>(data), static_cast<int>(size)));
			break;
	}
	reg.sync();
	return ERROR_SUCCESS;
}

LONG RegDeleteValueW(HKEY key, LPCWSTR name)
{
	QSettings& reg = registry();
	const QString k = keyPath(key, nullptr) + QLatin1Char('/') + (name && *name ? qs(name) : QStringLiteral("(Default)"));
	if (!reg.contains(k))
		return ERROR_FILE_NOT_FOUND;
	reg.remove(k);
	return ERROR_SUCCESS;
}

LONG RegDeleteKeyW(HKEY key, LPCWSTR subKey)
{
	registry().remove(keyPath(key, subKey));
	return ERROR_SUCCESS;
}

LONG RegCloseKey(HKEY key)
{
	auto* k = static_cast<RegKey*>(key);
	if (k && reinterpret_cast<uintptr_t>(key) > 0x80000010 && k->magic == 0x4E505052u)
	{
		k->magic = 0;
		delete k;
	}
	return ERROR_SUCCESS;
}

LONG RegEnumKeyExW(HKEY key, DWORD index, LPWSTR name, LPDWORD nameLen, LPDWORD, LPWSTR, LPDWORD, LPFILETIME)
{
	QSettings& reg = registry();
	reg.beginGroup(keyPath(key, nullptr));
	const QStringList groups = reg.childGroups();
	reg.endGroup();
	if (index >= static_cast<DWORD>(groups.size()))
		return ERROR_NO_MORE_ITEMS;
	if (name && nameLen)
		*nameLen = static_cast<DWORD>(copyOut(groups[static_cast<int>(index)], name, static_cast<int>(*nameLen)));
	return ERROR_SUCCESS;
}

LONG RegEnumValueW(HKEY key, DWORD index, LPWSTR name, LPDWORD nameLen, LPDWORD reserved, LPDWORD type, LPBYTE data, LPDWORD size)
{
	QSettings& reg = registry();
	reg.beginGroup(keyPath(key, nullptr));
	const QStringList keys = reg.childKeys();
	reg.endGroup();
	if (index >= static_cast<DWORD>(keys.size()))
		return ERROR_NO_MORE_ITEMS;
	const QString k = keys[static_cast<int>(index)];
	if (name && nameLen)
		*nameLen = static_cast<DWORD>(copyOut(k, name, static_cast<int>(*nameLen)));
	const std::wstring wk = ws(k);
	return RegQueryValueExW(key, wk.c_str(), reserved, type, data, size);
}

LONG RegGetValueW(HKEY key, LPCWSTR subKey, LPCWSTR value, DWORD, LPDWORD type, PVOID data, LPDWORD size)
{
	HKEY k = nullptr;
	if (RegOpenKeyExW(key, subKey, 0, KEY_READ, &k) != ERROR_SUCCESS)
		return ERROR_FILE_NOT_FOUND;
	const LONG r = RegQueryValueExW(k, value, nullptr, type, static_cast<LPBYTE>(data), size);
	RegCloseKey(k);
	return r;
}

// ---- COM ------------------------------------------------------------------------------------------
HRESULT CoInitialize(LPVOID) { return S_OK; }
HRESULT CoInitializeEx(LPVOID, DWORD) { return S_OK; }
void CoUninitialize() {}
HRESULT CoCreateInstance(REFCLSID clsid, void*, DWORD, REFIID iid, LPVOID* out)
{
	if (!out)
		return E_POINTER;
	*out = nullptr;
	// the file dialogs; other classes are not registered
	return w32::createShellObject(clsid, iid, out);
}
void CoTaskMemFree(LPVOID p) { std::free(p); }

namespace
{
	// IMalloc of CoGetMalloc / SHGetMalloc: CoTaskMemAlloc and CoTaskMemFree
	class TaskMalloc : public IMalloc
	{
	public:
		HRESULT STDMETHODCALLTYPE QueryInterface(REFIID iid, void** out) override
		{
			if (!out)
				return E_POINTER;
			*out = (iid == IID_IUnknown || iid == IID_IMalloc) ? static_cast<IMalloc*>(this) : nullptr;
			return *out ? S_OK : E_NOINTERFACE;
		}
		ULONG STDMETHODCALLTYPE AddRef() override { return 2; }
		ULONG STDMETHODCALLTYPE Release() override { return 1; }
		void* STDMETHODCALLTYPE Alloc(SIZE_T cb) override { return CoTaskMemAlloc(cb); }
		void* STDMETHODCALLTYPE Realloc(void* pv, SIZE_T cb) override { return std::realloc(pv, cb); }
		void STDMETHODCALLTYPE Free(void* pv) override { CoTaskMemFree(pv); }
		SIZE_T STDMETHODCALLTYPE GetSize(void*) override { return static_cast<SIZE_T>(-1); }
		int STDMETHODCALLTYPE DidAlloc(void*) override { return -1; }
		void STDMETHODCALLTYPE HeapMinimize() override {}
	};
}

HRESULT CoGetMalloc(DWORD, LPMALLOC* malloc)
{
	static TaskMalloc m;
	if (!malloc)
		return E_INVALIDARG;
	*malloc = &m;
	return S_OK;
}
LPVOID CoTaskMemAlloc(SIZE_T size) { return std::malloc(size); }
HRESULT CoCreateGuid(GUID* guid)
{
	if (!guid)
		return E_POINTER;
	const QByteArray r = QByteArray::fromHex(QUuid::createUuid().toRfc4122().toHex());
	std::memcpy(guid, r.constData(), std::min<size_t>(sizeof(GUID), static_cast<size_t>(r.size())));
	return S_OK;
}
HRESULT OleInitialize(LPVOID) { return S_OK; }
void OleUninitialize() {}
BSTR SysAllocString(const OLECHAR* s)
{
	const size_t n = s ? std::wcslen(s) : 0;
	auto* out = static_cast<wchar_t*>(std::malloc((n + 1) * sizeof(wchar_t)));
	if (s)
		std::wmemcpy(out, s, n);
	out[n] = 0;
	return out;
}
void SysFreeString(BSTR s) { std::free(s); }
UINT SysStringLen(BSTR s) { return s ? static_cast<UINT>(std::wcslen(s)) : 0; }

// ---- clipboard --------------------------------------------------------------------------------------
namespace
{
	struct ClipboardState
	{
		bool open = false;
		bool emptied = false;
		QMimeData* pending = nullptr;
		std::vector<HGLOBAL> owned;
	};

	ClipboardState& clip()
	{
		static ClipboardState s;
		return s;
	}

	std::map<UINT, QString>& customFormats()
	{
		static std::map<UINT, QString> f;
		return f;
	}

	UINT preferredDropEffect()
	{
		static const UINT f = RegisterClipboardFormatW(L"Preferred DropEffect");
		return f;
	}

	QString mimeOf(UINT format)
	{
		if (format == CF_TEXT || format == CF_UNICODETEXT || format == CF_OEMTEXT)
			return QStringLiteral("text/plain");
		auto it = customFormats().find(format);
		if (it != customFormats().end())
			return QStringLiteral("application/x-qt-windows-mime;value=\"%1\"").arg(it->second);
		return QStringLiteral("application/x-npp-format-%1").arg(format);
	}
}

BOOL OpenClipboard(HWND)
{
	clip().open = true;
	return TRUE;
}

BOOL CloseClipboard()
{
	ClipboardState& c = clip();
	if (c.pending)
	{
		QApplication::clipboard()->setMimeData(c.pending);
		c.pending = nullptr;
	}
	else if (c.emptied)
	{
		QApplication::clipboard()->clear();
	}
	c.emptied = false;
	c.open = false;
	return TRUE;
}

BOOL EmptyClipboard()
{
	ClipboardState& c = clip();
	delete c.pending;
	c.pending = nullptr;
	c.emptied = true;
	return TRUE;
}

HANDLE SetClipboardData(UINT format, HANDLE data)
{
	ClipboardState& c = clip();
	if (!c.pending)
		c.pending = new QMimeData;
	if (!data)
		return nullptr;
	if (format == CF_HDROP)
	{
		// files: URLs (and cut or copy, CFSTR_PREFERREDDROPEFFECT, when it comes)
		const DWORD effect = filesEffectOf(c.pending);
		setFilesMime(c.pending, dropFilesList(data), c.pending->hasFormat(QStringLiteral("x-special/gnome-copied-files")) ? effect : 0);
	}
	else if (format == preferredDropEffect())
	{
		DWORD effect = 0;
		std::memcpy(&effect, data, std::min<size_t>(sizeof(effect), GlobalSize(data)));
		setFilesEffect(c.pending, effect);
	}
	else if (format == CF_UNICODETEXT)
		c.pending->setText(qs(static_cast<const wchar_t*>(data)));
	else if (format == CF_TEXT || format == CF_OEMTEXT)
		c.pending->setText(qsA(static_cast<const char*>(data)));
	else
	{
		const SIZE_T size = GlobalSize(data);
		c.pending->setData(mimeOf(format), QByteArray(static_cast<const char*>(data), static_cast<int>(size)));
	}
	GlobalFree(data); // the clipboard owns it now
	return data;
}

HANDLE GetClipboardData(UINT format)
{
	ClipboardState& c = clip();
	for (HGLOBAL h : c.owned)
		GlobalFree(h);
	c.owned.clear();
	const QMimeData* md = QApplication::clipboard()->mimeData();
	if (!md)
		return nullptr;
	HGLOBAL h = nullptr;
	if (format == CF_HDROP)
	{
		QStringList files;
		for (const QUrl& u : md->urls())
		{
			if (u.isLocalFile())
				files << u.toLocalFile();
		}
		if (files.isEmpty())
			return nullptr;
		h = makeDropFiles(files, POINT{});
	}
	else if (format == preferredDropEffect() && md->hasUrls())
	{
		const DWORD effect = filesEffectOf(md);
		h = GlobalAlloc(GMEM_ZEROINIT, sizeof(DWORD));
		std::memcpy(h, &effect, sizeof(DWORD));
	}
	else if (format == CF_UNICODETEXT)
	{
		if (!md->hasText())
			return nullptr;
		const std::wstring w = ws(md->text());
		h = GlobalAlloc(GMEM_ZEROINIT, (w.size() + 1) * sizeof(wchar_t));
		std::wmemcpy(static_cast<wchar_t*>(h), w.c_str(), w.size() + 1);
	}
	else if (format == CF_TEXT || format == CF_OEMTEXT)
	{
		if (!md->hasText())
			return nullptr;
		const QByteArray u = md->text().toUtf8();
		h = GlobalAlloc(GMEM_ZEROINIT, static_cast<SIZE_T>(u.size()) + 1);
		std::memcpy(h, u.constData(), static_cast<size_t>(u.size()));
	}
	else
	{
		const QString mime = mimeOf(format);
		if (!md->hasFormat(mime))
			return nullptr;
		const QByteArray b = md->data(mime);
		h = GlobalAlloc(GMEM_ZEROINIT, static_cast<SIZE_T>(b.size()));
		std::memcpy(h, b.constData(), static_cast<size_t>(b.size()));
	}
	c.owned.push_back(h);
	return h;
}

BOOL IsClipboardFormatAvailable(UINT format)
{
	const QMimeData* md = QApplication::clipboard()->mimeData();
	if (!md)
		return FALSE;
	if (format == CF_TEXT || format == CF_UNICODETEXT || format == CF_OEMTEXT)
		return md->hasText() ? TRUE : FALSE;
	if (format == CF_HDROP || format == preferredDropEffect())
	{
		for (const QUrl& u : md->urls())
		{
			if (u.isLocalFile())
				return TRUE;
		}
		return format == CF_HDROP ? FALSE : (md->hasFormat(mimeOf(format)) ? TRUE : FALSE);
	}
	return md->hasFormat(mimeOf(format)) ? TRUE : FALSE;
}

UINT RegisterClipboardFormatW(LPCWSTR name)
{
	const QString n = qs(name);
	for (const auto& f : customFormats())
	{
		if (f.second.compare(n, Qt::CaseInsensitive) == 0)
			return f.first;
	}
	const UINT id = 0xC000 + static_cast<UINT>(customFormats().size()) + 0x100;
	customFormats()[id] = n;
	return id;
}

UINT EnumClipboardFormats(UINT format)
{
	return (format == 0 && IsClipboardFormatAvailable(CF_UNICODETEXT)) ? CF_UNICODETEXT : 0;
}

int CountClipboardFormats()
{
	const QMimeData* md = QApplication::clipboard()->mimeData();
	return md ? static_cast<int>(md->formats().size()) : 0;
}

// ---- system metrics, monitors, rectangles --------------------------------------------------------
int GetSystemMetrics(int index)
{
	QScreen* s = QGuiApplication::primaryScreen();
	const QRect g = s ? s->geometry() : QRect(0, 0, 1920, 1080);
	const QRect v = s ? s->virtualGeometry() : g;
	QStyle* st = QApplication::style();
	switch (index)
	{
		case SM_CXSCREEN: return g.width();
		case SM_CYSCREEN: return g.height();
		case SM_CXVIRTUALSCREEN: return v.width();
		case SM_CYVIRTUALSCREEN: return v.height();
		case SM_XVIRTUALSCREEN: return v.x();
		case SM_YVIRTUALSCREEN: return v.y();
		case SM_CXFULLSCREEN: return s ? s->availableGeometry().width() : g.width();
		case SM_CYFULLSCREEN: return s ? s->availableGeometry().height() : g.height();
		case SM_CXVSCROLL: case SM_CXHSCROLL: case SM_CYHSCROLL: case SM_CYVSCROLL:
			return st ? st->pixelMetric(QStyle::PM_ScrollBarExtent) : 16;
		case SM_CYCAPTION: case SM_CYSMCAPTION: return st ? st->pixelMetric(QStyle::PM_TitleBarHeight) : 23;
		case SM_CXBORDER: case SM_CYBORDER: return 1;
		case SM_CXEDGE: case SM_CYEDGE: return 2;
		case SM_CXDLGFRAME: case SM_CYDLGFRAME: case SM_CXFRAME: case SM_CYFRAME: return 4;
		case SM_CXPADDEDBORDER: return 4;
		case SM_CXICON: case SM_CYICON: case SM_CXCURSOR: case SM_CYCURSOR: return 32;
		case SM_CXSMICON: case SM_CYSMICON: return st ? st->pixelMetric(QStyle::PM_SmallIconSize) : 16;
		case SM_CYMENU: return st ? st->pixelMetric(QStyle::PM_MenuBarItemSpacing) + 20 : 20;
		case SM_CXMENUCHECK: case SM_CYMENUCHECK: return st ? st->pixelMetric(QStyle::PM_IndicatorWidth) : 13;
		case SM_CXDOUBLECLK: case SM_CYDOUBLECLK: return 4;
		case SM_CXDRAG: case SM_CYDRAG: return QApplication::startDragDistance();
		case SM_CMONITORS: return static_cast<int>(QGuiApplication::screens().size());
		case SM_SWAPBUTTON: return 0;
		case SM_CXMIN: return 112;
		case SM_CYMIN: return 27;
		case SM_CXSIZE: case SM_CYSIZE: return 24;
		default: return 0;
	}
}

BOOL SystemParametersInfoW(UINT action, UINT, PVOID pvParam, UINT)
{
	switch (action)
	{
		case SPI_GETWORKAREA:
		{
			QScreen* s = QGuiApplication::primaryScreen();
			const QRect r = s ? s->availableGeometry() : QRect(0, 0, 1920, 1040);
			if (pvParam)
				*static_cast<RECT*>(pvParam) = RECT{r.left(), r.top(), r.left() + r.width(), r.top() + r.height()};
			return TRUE;
		}
		case SPI_GETNONCLIENTMETRICS:
		{
			auto* m = static_cast<NONCLIENTMETRICSW*>(pvParam);
			if (!m)
				return FALSE;
			const UINT cb = m->cbSize;
			*m = NONCLIENTMETRICSW{};
			m->cbSize = cb;
			m->iBorderWidth = 1;
			m->iScrollWidth = m->iScrollHeight = GetSystemMetrics(SM_CXVSCROLL);
			m->iCaptionHeight = GetSystemMetrics(SM_CYCAPTION);
			m->iMenuHeight = GetSystemMetrics(SM_CYMENU);
			Gdi* g = gdi(GetStockObject(DEFAULT_GUI_FONT));
			if (g)
				m->lfMessageFont = m->lfMenuFont = m->lfStatusFont = m->lfCaptionFont = m->lfSmCaptionFont = g->logFont;
			return TRUE;
		}
		case SPI_GETICONTITLELOGFONT:
			if (Gdi* g = gdi(GetStockObject(DEFAULT_GUI_FONT)); g && pvParam)
				*static_cast<LOGFONTW*>(pvParam) = g->logFont;
			return TRUE;
		case SPI_GETWHEELSCROLLLINES:
			if (pvParam)
				*static_cast<UINT*>(pvParam) = static_cast<UINT>(QApplication::wheelScrollLines());
			return TRUE;
		case SPI_GETKEYBOARDCUES:
			if (pvParam)
				*static_cast<BOOL*>(pvParam) = TRUE;
			return TRUE;
		case SPI_GETHIGHCONTRAST:
			return FALSE;
		default:
			return FALSE;
	}
}

namespace
{
	HMONITOR monitorOf(QScreen* s)
	{
		return s ? static_cast<HMONITOR>(s) : static_cast<HMONITOR>(QGuiApplication::primaryScreen());
	}
}

HMONITOR MonitorFromWindow(HWND hWnd, DWORD)
{
	QWidget* w = widgetOf(hWnd);
	return monitorOf(w ? w->screen() : nullptr);
}

HMONITOR MonitorFromRect(LPCRECT rc, DWORD)
{
	if (!rc)
		return monitorOf(nullptr);
	return monitorOf(QGuiApplication::screenAt(QPoint((rc->left + rc->right) / 2, (rc->top + rc->bottom) / 2)));
}

BOOL EnumDisplayMonitors(HDC, LPCRECT clip, MONITORENUMPROC proc, LPARAM data)
{
	if (!proc)
		return FALSE;
	for (QScreen* s : QGuiApplication::screens())
	{
		const QRect g = s->geometry();
		RECT rc{g.left(), g.top(), g.left() + g.width(), g.top() + g.height()};
		if (clip)
		{
			RECT part{};
			if (!IntersectRect(&part, &rc, clip))
				continue;
			rc = part;
		}
		if (!proc(static_cast<HMONITOR>(s), nullptr, &rc, data))
			break;
	}
	return TRUE;
}

HMONITOR MonitorFromPoint(POINT pt, DWORD)
{
	return monitorOf(QGuiApplication::screenAt(QPoint(pt.x, pt.y)));
}

BOOL GetMonitorInfoW(HMONITOR hMonitor, LPMONITORINFO info)
{
	auto* s = static_cast<QScreen*>(hMonitor);
	if (!info)
		return FALSE;
	if (!QGuiApplication::screens().contains(s))
		s = QGuiApplication::primaryScreen();
	if (!s)
		return FALSE;
	const QRect g = s->geometry();
	const QRect a = s->availableGeometry();
	info->rcMonitor = RECT{g.left(), g.top(), g.left() + g.width(), g.top() + g.height()};
	info->rcWork = RECT{a.left(), a.top(), a.left() + a.width(), a.top() + a.height()};
	info->dwFlags = (s == QGuiApplication::primaryScreen()) ? 1 : 0;
	return TRUE;
}

BOOL SetRect(LPRECT rc, int left, int top, int right, int bottom)
{
	if (!rc)
		return FALSE;
	*rc = RECT{left, top, right, bottom};
	return TRUE;
}

BOOL SetRectEmpty(LPRECT rc)
{
	return SetRect(rc, 0, 0, 0, 0);
}

BOOL CopyRect(LPRECT dst, const RECT* src)
{
	if (!dst || !src)
		return FALSE;
	*dst = *src;
	return TRUE;
}

BOOL InflateRect(LPRECT rc, int dx, int dy)
{
	if (!rc)
		return FALSE;
	rc->left -= dx;
	rc->right += dx;
	rc->top -= dy;
	rc->bottom += dy;
	return TRUE;
}

BOOL OffsetRect(LPRECT rc, int dx, int dy)
{
	if (!rc)
		return FALSE;
	rc->left += dx;
	rc->right += dx;
	rc->top += dy;
	rc->bottom += dy;
	return TRUE;
}

BOOL IsRectEmpty(const RECT* rc)
{
	return (!rc || rc->right <= rc->left || rc->bottom <= rc->top) ? TRUE : FALSE;
}

BOOL PtInRect(const RECT* rc, POINT pt)
{
	return (rc && pt.x >= rc->left && pt.x < rc->right && pt.y >= rc->top && pt.y < rc->bottom) ? TRUE : FALSE;
}

BOOL IntersectRect(LPRECT dst, const RECT* a, const RECT* b)
{
	if (!dst || !a || !b)
		return FALSE;
	RECT r{std::max(a->left, b->left), std::max(a->top, b->top), std::min(a->right, b->right), std::min(a->bottom, b->bottom)};
	if (r.right <= r.left || r.bottom <= r.top)
	{
		*dst = RECT{0, 0, 0, 0};
		return FALSE;
	}
	*dst = r;
	return TRUE;
}

BOOL UnionRect(LPRECT dst, const RECT* a, const RECT* b)
{
	if (!dst || !a || !b)
		return FALSE;
	if (IsRectEmpty(a))
	{
		*dst = *b;
		return !IsRectEmpty(b);
	}
	if (IsRectEmpty(b))
	{
		*dst = *a;
		return TRUE;
	}
	*dst = RECT{std::min(a->left, b->left), std::min(a->top, b->top), std::max(a->right, b->right), std::max(a->bottom, b->bottom)};
	return TRUE;
}

BOOL EqualRect(const RECT* a, const RECT* b)
{
	return (a && b && a->left == b->left && a->top == b->top && a->right == b->right && a->bottom == b->bottom) ? TRUE : FALSE;
}

int MulDiv(int number, int numerator, int denominator)
{
	if (denominator == 0)
		return -1;
	const long long v = static_cast<long long>(number) * numerator;
	const long long half = (v < 0) == (denominator < 0) ? denominator / 2 : -denominator / 2;
	return static_cast<int>((v + half) / denominator);
}

// ---- strings of user32 ----------------------------------------------------------------------------
int LoadStringW(HINSTANCE, UINT id, LPWSTR buffer, int max)
{
	const NppRcString* s = findString(static_cast<int>(id));
	if (!s)
	{
		if (buffer && max > 0)
			buffer[0] = 0;
		return 0;
	}
	if (max == 0 && buffer)
	{
		// a read-only pointer to the string
		*reinterpret_cast<const wchar_t**>(buffer) = s->text;
		return static_cast<int>(std::wcslen(s->text));
	}
	return copyOut(qs(s->text), buffer, max);
}

int LoadStringA(HINSTANCE hInst, UINT id, LPSTR buffer, int max)
{
	wchar_t buf[4096] = {};
	LoadStringW(hInst, id, buf, 4096);
	return copyOutA(qs(buf), buffer, max);
}

LPWSTR CharUpperW(LPWSTR s)
{
	if (isIntResource(s))
		return reinterpret_cast<LPWSTR>(static_cast<uintptr_t>(std::towupper(static_cast<wint_t>(reinterpret_cast<uintptr_t>(s)))));
	for (wchar_t* p = s; p && *p; ++p)
		*p = static_cast<wchar_t>(std::towupper(*p));
	return s;
}

LPWSTR CharLowerW(LPWSTR s)
{
	if (isIntResource(s))
		return reinterpret_cast<LPWSTR>(static_cast<uintptr_t>(std::towlower(static_cast<wint_t>(reinterpret_cast<uintptr_t>(s)))));
	for (wchar_t* p = s; p && *p; ++p)
		*p = static_cast<wchar_t>(std::towlower(*p));
	return s;
}

LPSTR CharUpperA(LPSTR s)
{
	for (char* p = s; p && *p; ++p)
		*p = static_cast<char>(std::toupper(static_cast<unsigned char>(*p)));
	return s;
}

LPSTR CharLowerA(LPSTR s)
{
	for (char* p = s; p && *p; ++p)
		*p = static_cast<char>(std::tolower(static_cast<unsigned char>(*p)));
	return s;
}

DWORD CharUpperBuffW(LPWSTR s, DWORD n)
{
	for (DWORD i = 0; s && i < n; ++i)
		s[i] = static_cast<wchar_t>(std::towupper(s[i]));
	return n;
}

DWORD CharLowerBuffW(LPWSTR s, DWORD n)
{
	for (DWORD i = 0; s && i < n; ++i)
		s[i] = static_cast<wchar_t>(std::towlower(s[i]));
	return n;
}

LPWSTR CharNextW(LPCWSTR s)
{
	return const_cast<LPWSTR>((s && *s) ? s + 1 : s);
}

LPWSTR CharPrevW(LPCWSTR start, LPCWSTR s)
{
	return const_cast<LPWSTR>((s && s > start) ? s - 1 : start);
}

LPSTR CharNextA(LPCSTR s)
{
	if (!s || !*s)
		return const_cast<LPSTR>(s);
	// UTF-8: past the continuation bytes
	const char* p = s + 1;
	while ((static_cast<unsigned char>(*p) & 0xC0) == 0x80)
		++p;
	return const_cast<LPSTR>(p);
}

BOOL IsCharAlphaW(WCHAR c) { return std::iswalpha(c) ? TRUE : FALSE; }
BOOL IsCharAlphaNumericW(WCHAR c) { return std::iswalnum(c) ? TRUE : FALSE; }
BOOL IsCharUpperW(WCHAR c) { return std::iswupper(c) ? TRUE : FALSE; }
BOOL IsCharLowerW(WCHAR c) { return std::iswlower(c) ? TRUE : FALSE; }
