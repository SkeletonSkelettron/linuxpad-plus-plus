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

#include "NppCommon.h"

#include <algorithm>

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QGuiApplication>
#include <QPalette>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QStyleHints>

namespace
{
	QSettings& settingsFile()
	{
		static QSettings s(Npp::configDir() + QStringLiteral("/config.ini"), QSettings::IniFormat);
		return s;
	}
}

void NppPrefs::load()
{
	QSettings& s = settingsFile();
	const NppPrefs d;
#define NPP_PREF_LOAD(type, name, def) name = s.value(QStringLiteral(#name), d.name).value<type>();
	NPP_PREF_FIELDS(NPP_PREF_LOAD)
#undef NPP_PREF_LOAD

	// options of the Find dialog (Smart Highlighting "Use Find dialog settings")
	Npp::findState().matchCase = s.value(QStringLiteral("Find/matchCase"), false).toBool();
	Npp::findState().wholeWord = s.value(QStringLiteral("Find/wholeWord"), false).toBool();

	// settings of older versions of the Linux port
	auto has = [&s](const char* key) { return s.contains(QString::fromLatin1(key)); };
	if (!has("currentLineMode") && has("currentLineHilite"))
		currentLineMode = s.value(QStringLiteral("currentLineHilite")).toBool() ? LINEHILITE_HILITE : LINEHILITE_NONE;
	if (!has("toolbarIconSet") && has("largeToolbarIcons"))
		toolbarIconSet = s.value(QStringLiteral("largeToolbarIcons")).toBool() ? TB_LARGE : TB_SMALL;
	if (!has("changeHistoryMargin") && has("showChangeHistory"))
		changeHistoryMargin = s.value(QStringLiteral("showChangeHistory")).toBool();
	if (!has("edgeColumns") && has("edgeMode"))
	{
		const int mode = s.value(QStringLiteral("edgeMode")).toInt();
		if (mode != 0)
			edgeColumns = QString::number(s.value(QStringLiteral("edgeColumn"), 80).toInt());
		edgeBgMode = (mode == 2);
	}
	if (!has("autoIndentMode") && has("autoIndent"))
		autoIndentMode = s.value(QStringLiteral("autoIndent")).toBool() ? AUTOINDENT_ADVANCED : AUTOINDENT_NONE;
	if (!has("multiInstMode") && has("multiInstance"))
		multiInstMode = s.value(QStringLiteral("multiInstance")).toBool() ? MULTI_INSTANCE : MONO_INSTANCE;
	if (!has("fileAutoDetectMode") && has("fileAutoDetect"))
		fileAutoDetectMode = s.value(QStringLiteral("fileAutoDetect")).toBool() ? AUTODETECT_CURRENT : AUTODETECT_DISABLED;

	// ranges of the Windows dialog
	if (tabSize < 1 || tabSize > 64)
		tabSize = 4;
	if (snapshotIntervalSec < 1)
		snapshotIntervalSec = 7;
	currentLineMode = std::clamp(currentLineMode, 0, 2);
	currentLineFrameWidth = std::clamp(currentLineFrameWidth, 1, 6);
	caretWidth = std::clamp(caretWidth, 0, 5);
	if (caretBlinkRate != 0)
		caretBlinkRate = std::clamp(caretBlinkRate, 50, 2500);
	toolbarIconSet = std::clamp(toolbarIconSet, 0, 4);
	toolbarColor = std::clamp(toolbarColor, 0, static_cast<int>(FluentColor::custom));
	tabMaxLabelLen = std::clamp(tabMaxLabelLen, 0, 99);
	borderWidth = std::clamp(borderWidth, 0, 30);
	paddingLeft = std::clamp(paddingLeft, 0, 9);
	paddingRight = std::clamp(paddingRight, 0, 9);
	distractionFreeDivPart = std::clamp(distractionFreeDivPart, 3, 9);
	recentFilesMax = std::clamp(recentFilesMax, 0, 30);
	recentCustomLength = std::clamp(recentCustomLength, 1, 259);
	largeFileSizeMB = std::clamp(largeFileSizeMB, 1, 2046);
	autoCompletionThreshold = std::clamp(autoCompletionThreshold, 1, 9);
	folderStyle = std::clamp(folderStyle, 1, 5);
	darkTone = std::clamp(darkTone, 0, 7);
}

void NppPrefs::save() const
{
	QSettings& s = settingsFile();
#define NPP_PREF_SAVE(type, name, def) s.setValue(QStringLiteral(#name), name);
	NPP_PREF_FIELDS(NPP_PREF_SAVE)
#undef NPP_PREF_SAVE
	for (const char* old : {"currentLineHilite", "largeToolbarIcons", "showChangeHistory", "edgeMode", "edgeColumn", "autoIndent", "multiInstance", "fileAutoDetect"})
		s.remove(QString::fromLatin1(old));
	s.sync();
}

QList<int> NppPrefs::edgeColumnList() const
{
	QList<int> cols;
	for (const QString& t : edgeColumns.split(QRegularExpression(QStringLiteral("[\\s,;]+")), Qt::SkipEmptyParts))
	{
		bool ok = false;
		const int c = t.toInt(&ok);
		if (ok && c > 0 && !cols.contains(c))
			cols << c;
	}
	std::sort(cols.begin(), cols.end());
	return cols;
}

QHash<QString, LangTabSetting> NppPrefs::langTabs() const
{
	QHash<QString, LangTabSetting> out;
	for (const QString& entry : langTabSettings.split(QLatin1Char(';'), Qt::SkipEmptyParts))
	{
		const QStringList parts = entry.split(QLatin1Char(':'));
		if (parts.size() < 3)
			continue;
		LangTabSetting t;
		t.size = std::clamp(parts[1].toInt(), 0, 64);
		t.spaces = parts[2] == QStringLiteral("1");
		t.backspaceUnindent = parts.size() > 3 && parts[3] == QStringLiteral("1");
		out.insert(parts[0], t);
	}
	return out;
}

void NppPrefs::setLangTabs(const QHash<QString, LangTabSetting>& tabs)
{
	QStringList entries;
	for (auto it = tabs.constBegin(); it != tabs.constEnd(); ++it)
		entries << QStringLiteral("%1:%2:%3:%4").arg(it.key()).arg(it.value().size).arg(it.value().spaces ? 1 : 0).arg(it.value().backspaceUnindent ? 1 : 0);
	entries.sort();
	langTabSettings = entries.join(QLatin1Char(';'));
}

bool NppPrefs::langTab(const QString& langName, LangTabSetting* out) const
{
	const auto tabs = langTabs();
	auto it = tabs.constFind(langName);
	if (it == tabs.constEnd())
		return false;
	if (out)
		*out = it.value();
	return true;
}

QList<std::pair<char, char>> NppPrefs::matchedPairList() const
{
	QList<std::pair<char, char>> pairs;
	const QByteArray b = matchedPairs.toLatin1();
	for (qsizetype i = 0; i + 1 < b.size(); i += 2)
	{
		if (b[i] != ' ' && b[i + 1] != ' ')
			pairs.append({b[i], b[i + 1]});
	}
	return pairs;
}

QString NppPrefs::searchEngineUrl() const
{
	switch (searchEngineChoice)
	{
		case SE_DUCKDUCKGO:
		case SE_BING:
			return QStringLiteral("https://duckduckgo.com/?q=$(CURRENT_WORD)");
		case SE_YAHOO:
			return QStringLiteral("https://search.yahoo.com/search?q=$(CURRENT_WORD)");
		case SE_STACKOVERFLOW:
			return QStringLiteral("https://stackoverflow.com/search?q=$(CURRENT_WORD)");
		case SE_CUSTOM:
		{
			QString url = searchEngine;
			url.remove(QRegularExpression(QStringLiteral("\\s")));
			if (url.startsWith(QStringLiteral("http://")) || url.startsWith(QStringLiteral("https://")))
				return url;
			return QStringLiteral("https://www.google.com/search?q=$(CURRENT_WORD)");
		}
		default:
			return QStringLiteral("https://www.google.com/search?q=$(CURRENT_WORD)");
	}
}

bool NppPrefs::isDark() const
{
	if (darkMode == DARKMODE_DARK)
		return true;
	if (darkMode == DARKMODE_LIGHT)
		return false;
	return Npp::isDarkDesktop();
}

namespace Npp
{
	NppPrefs& prefs()
	{
		static NppPrefs p;
		return p;
	}

	FindState& findState()
	{
		static FindState f;
		return f;
	}

	QString dataDir()
	{
		static QString dir = []
		{
			const QByteArray env = qgetenv("NPP_DATA_DIR");
			if (!env.isEmpty() && QFileInfo(QString::fromLocal8Bit(env) + QStringLiteral("/langs.model.xml")).exists())
				return QString::fromLocal8Bit(env);

			const QString exeDir = QCoreApplication::applicationDirPath();
			const QStringList candidates = {
				exeDir + QStringLiteral("/../share/linuxpad++"),
				exeDir + QStringLiteral("/share/linuxpad++"),
				exeDir,
				QStringLiteral("/usr/local/share/linuxpad++"),
				QStringLiteral("/usr/share/linuxpad++"),
			};
			for (const QString& c : candidates)
			{
				if (QFileInfo(c + QStringLiteral("/langs.model.xml")).exists())
					return QDir(c).canonicalPath();
			}
			return exeDir;
		}();
		return dir;
	}

	// LinuxPad++ was called Notepad++ (~/.config/notepad++): the first start copies those settings, once. The
	// folder of Notepad++ is left as it is. The copy is made under another name and renamed at the end, so an
	// interrupted copy is made again. The settings files that store the old path get the new one (not the backups,
	// which are the documents of the user).
	void migrateSettings(const QString& from, const QString& to)
	{
		if (QFileInfo::exists(to) || !QFileInfo(from).isDir())
			return;
		const QString tmp = to + QStringLiteral(".migrating");
		QDir(tmp).removeRecursively();
		bool ok = QDir().mkpath(tmp);
		const QString fromPrefix = QDir(from).absolutePath() + QLatin1Char('/');
		const QString toPrefix = QDir(to).absolutePath() + QLatin1Char('/');
		QDirIterator it(from, QDir::AllEntries | QDir::Hidden | QDir::System | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
		while (ok && it.hasNext())
		{
			const QString src = it.next();
			const QFileInfo info = it.fileInfo();
			const QString rel = QDir(from).relativeFilePath(src);
			const QString dst = tmp + QLatin1Char('/') + rel;
			if (info.isSymLink())
				ok = QFile::link(info.symLinkTarget(), dst);
			else if (info.isDir())
				ok = QDir().mkpath(dst);
			else
			{
				const QString suffix = info.suffix().toLower();
				const bool text = !rel.startsWith(QStringLiteral("backup/")) && info.size() < 4 * 1024 * 1024
					&& (suffix == QStringLiteral("xml") || suffix == QStringLiteral("ini") || suffix == QStringLiteral("json")
						|| suffix == QStringLiteral("cfg") || suffix == QStringLiteral("txt") || suffix.isEmpty());
				QFile in(src);
				QByteArray data;
				if (text && in.open(QIODevice::ReadOnly))
				{
					data = in.readAll();
					in.close();
					data.replace(fromPrefix.toUtf8(), toPrefix.toUtf8());
					QFile out(dst);
					ok = out.open(QIODevice::WriteOnly) && out.write(data) == data.size();
					if (ok)
						out.setPermissions(info.permissions());
				}
				else
					ok = QFile::copy(src, dst);
			}
		}
		if (ok)
			ok = QDir().rename(tmp, to);
		if (!ok)
		{
			qWarning("LinuxPad++: the settings of %s were not copied to %s", qPrintable(from), qPrintable(to));
			QDir(tmp).removeRecursively();
		}
	}

	QString localConfigDir()
	{
		static QString dir = []
		{
			QString base = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
			if (base.isEmpty())
				base = QDir::homePath() + QStringLiteral("/.config");
			const QString d = base + QStringLiteral("/linuxpad++");
			migrateSettings(base + QStringLiteral("/notepad++"), d);
			QDir().mkpath(d);
			return d;
		}();
		return dir;
	}

	QString cloudChoiceFile()
	{
		return localConfigDir() + QStringLiteral("/cloud/choice");
	}

	QString configDir()
	{
		// Settings on cloud (Preferences > Cloud & Link): like Notepad++, the path is
		// read from cloud/choice at launch, so a change needs a restart.
		static QString dir = []
		{
			QFile f(cloudChoiceFile());
			if (f.open(QIODevice::ReadOnly))
			{
				const QString cloud = QString::fromUtf8(f.readAll()).trimmed();
				if (!cloud.isEmpty() && QFileInfo(cloud).isDir() && QFileInfo(cloud).isWritable())
					return QDir(cloud).absolutePath();
			}
			return localConfigDir();
		}();
		return dir;
	}

	QString backupDir()
	{
		// the session and its backups store absolute paths: never on the cloud folder
		const QString d = localConfigDir() + QStringLiteral("/backup");
		QDir().mkpath(d);
		return d;
	}

	QString version()
	{
		return QStringLiteral(NPP_VERSION_STRING);
	}

	QString appName()
	{
		return QStringLiteral("LinuxPad++");
	}

	bool isDarkDesktop()
	{
		if (auto* hints = QGuiApplication::styleHints())
		{
			if (hints->colorScheme() == Qt::ColorScheme::Dark)
				return true;
			if (hints->colorScheme() == Qt::ColorScheme::Light)
				return false;
		}
		const QPalette pal = QGuiApplication::palette();
		return pal.color(QPalette::Window).lightness() < 128;
	}

	void beep()
	{
		if (!prefs().muteSounds)
			QApplication::beep();
	}

	QString windowsDateTimeFormat(const QString& fmt)
	{
		// GetDateFormat / GetTimeFormat pictures use "tt" / "t" for the AM/PM designator
		QString out;
		out.reserve(fmt.size());
		for (int i = 0; i < fmt.size(); ++i)
		{
			if (fmt[i] == QLatin1Char('t'))
			{
				if (i + 1 < fmt.size() && fmt[i + 1] == QLatin1Char('t'))
				{
					out += QStringLiteral("AP");
					++i;
				}
				else
				{
					out += QStringLiteral("A");
				}
				continue;
			}
			if (fmt[i] == QLatin1Char('\''))
			{
				const int close = fmt.indexOf(QLatin1Char('\''), i + 1);
				const int stop = close < 0 ? static_cast<int>(fmt.size()) - 1 : close;
				out += fmt.mid(i, stop - i + 1);
				i = stop;
				continue;
			}
			out += fmt[i];
		}
		return out;
	}

	QString stripAmpersand(const QString& s)
	{
		QString out;
		out.reserve(s.size());
		for (int i = 0; i < s.size(); ++i)
		{
			if (s[i] == QLatin1Char('&'))
			{
				if (i + 1 < s.size() && s[i + 1] == QLatin1Char('&'))
				{
					out += QLatin1Char('&');
					++i;
				}
				continue;
			}
			out += s[i];
		}
		return out;
	}
}
