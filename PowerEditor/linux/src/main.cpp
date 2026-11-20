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

// Entry point of the Linux front end: Qt counterpart of winmain.cpp.
// Parses the Notepad++ command line and hands files over to an already
// running instance (single instance mode) through a local socket.

#include <cstdio>

#include <QApplication>
#include <QDataStream>
#include <QDir>
#include <QFileInfo>
#include <QLocalServer>
#include <QLocalSocket>

#include "LangStyles.h"
#include "Localization.h"
#include "MainWindow.h"
#include "NppCommon.h"

namespace
{
	struct CmdLine
	{
		QStringList files;
		OpenOptions opts;
		bool multiInst = false;
		bool noSession = false;
		bool noPlugin = false;  // -noPlugin
		QString pluginMessage;  // -pluginMessage=...
		bool alwaysOnTop = false;
		bool help = false;
		QString langName;
	};

	CmdLine parse(const QStringList& args)
	{
		CmdLine c;
		for (int i = 1; i < args.size(); ++i)
		{
			const QString& a = args[i];
			if (a == QStringLiteral("--help") || a == QStringLiteral("-h"))
				c.help = true;
			else if (a.compare(QStringLiteral("-multiInst"), Qt::CaseInsensitive) == 0)
				c.multiInst = true;
			else if (a.compare(QStringLiteral("-nosession"), Qt::CaseInsensitive) == 0)
				c.noSession = true;
			else if (a.compare(QStringLiteral("-noPlugin"), Qt::CaseInsensitive) == 0)
				c.noPlugin = true;
			else if (a.startsWith(QStringLiteral("-pluginMessage="), Qt::CaseInsensitive))
				c.pluginMessage = a.mid(15);
			else if (a.compare(QStringLiteral("-ro"), Qt::CaseInsensitive) == 0)
				c.opts.readOnly = true;
			else if (a.compare(QStringLiteral("-alwaysOnTop"), Qt::CaseInsensitive) == 0)
				c.alwaysOnTop = true;
			else if (a.startsWith(QStringLiteral("-n")) && a.size() > 2 && a[2].isDigit())
				c.opts.line = a.mid(2).toLongLong() - 1;
			else if (a.startsWith(QStringLiteral("-c")) && a.size() > 2 && a[2].isDigit())
				c.opts.column = a.mid(2).toLongLong() - 1;
			else if (a.startsWith(QStringLiteral("-l")) && a.size() > 2)
				c.langName = a.mid(2);
			else if (a.startsWith(QLatin1Char('-')) && !QFileInfo::exists(a))
				continue; // Ignore unsupported switches.
			else
				c.files << QFileInfo(a).absoluteFilePath();
		}
		return c;
	}

	QString serverName()
	{
		return QStringLiteral("notepad++-%1").arg(QString::fromLocal8Bit(qgetenv("USER")));
	}

	QByteArray encodeRequest(const CmdLine& c)
	{
		QByteArray data;
		QDataStream out(&data, QIODevice::WriteOnly);
		out << c.files << static_cast<qint64>(c.opts.line) << static_cast<qint64>(c.opts.column) << c.langName << c.opts.readOnly;
		return data;
	}

	CmdLine decodeRequest(const QByteArray& data)
	{
		CmdLine c;
		QDataStream in(data);
		qint64 line = -1;
		qint64 column = -1;
		in >> c.files >> line >> column >> c.langName >> c.opts.readOnly;
		c.opts.line = line;
		c.opts.column = column;
		return c;
	}

	bool sendToRunningInstance(const CmdLine& c)
	{
		QLocalSocket socket;
		socket.connectToServer(serverName());
		if (!socket.waitForConnected(500))
			return false;
		const QByteArray payload = encodeRequest(c);
		QDataStream out(&socket);
		out << payload;
		socket.flush();
		socket.waitForBytesWritten(1000);
		socket.disconnectFromServer();
		return true;
	}

	void applyLang(CmdLine& c)
	{
		if (!c.langName.isEmpty())
			c.opts.lang = StyleManager::instance().langFromName(c.langName);
	}
}

int main(int argc, char** argv)
{
	QApplication::setApplicationName(QStringLiteral("notepad++"));
	QApplication::setApplicationDisplayName(QStringLiteral("Notepad++"));
	QApplication::setApplicationVersion(Npp::version());
	QApplication::setDesktopFileName(qEnvironmentVariable("FLATPAK_ID", QStringLiteral("notepad-plus-plus")));
	QApplication app(argc, argv);

	CmdLine cmd = parse(QApplication::arguments());
	if (cmd.help)
	{
		std::printf("Usage: notepad++ [--help] [-multiInst] [-nosession] [-noPlugin] [-pluginMessage=message]\n"
			"                 [-lLanguage] [-nLineNumber] [-cColumnNumber] [-ro] [-alwaysOnTop] [filePath...]\n");
		return 0;
	}

	NppPrefs& prefs = Npp::prefs();
	prefs.load();

	// Preferences > Multi-Instance & Date
	QString instanceSession;
	if (prefs.multiInstMode == SESSION_IN_NEW_INSTANCE && !prefs.sessionFileExt.isEmpty())
	{
		// "Open session in a new instance (and save session automatically on exit)"
		const QString ext = prefs.sessionFileExt.section(QLatin1Char('.'), -1);
		for (const QString& f : cmd.files)
		{
			if (QFileInfo(f).suffix().compare(ext, Qt::CaseInsensitive) == 0 && QFileInfo(f).isFile())
			{
				instanceSession = QFileInfo(f).absoluteFilePath();
				break;
			}
		}
	}
	const bool multiInst = cmd.multiInst || prefs.multiInstMode == MULTI_INSTANCE || !instanceSession.isEmpty();
	const bool singleInstance = !multiInst;
	if (singleInstance && sendToRunningInstance(cmd))
		return 0;

	Localization::instance().load(prefs.localization);
	StyleManager::instance().loadLangs();
	applyLang(cmd);

	if (cmd.alwaysOnTop)
		prefs.alwaysOnTop = true;

	MainWindow w;
	w._noSession = cmd.noSession || !instanceSession.isEmpty();
	w._instanceSession = instanceSession;
	w.setMultiInstance(multiInst);
	w.applyDialogFont();

	QLocalServer server;
	if (singleInstance)
	{
		QLocalServer::removeServer(serverName());
		server.setSocketOptions(QLocalServer::UserAccessOption);
		server.listen(serverName());
		QObject::connect(&server, &QLocalServer::newConnection, &w, [&server, &w]
		{
			while (QLocalSocket* sock = server.nextPendingConnection())
			{
				QObject::connect(sock, &QLocalSocket::readyRead, &w, [sock, &w]
				{
					QDataStream in(sock);
					in.startTransaction();
					QByteArray payload;
					in >> payload;
					if (!in.commitTransaction())
						return;
					CmdLine c = decodeRequest(payload);
					applyLang(c);
					w.openFromCommandLine(c.files, c.opts);
					sock->disconnectFromServer();
				});
				QObject::connect(sock, &QLocalSocket::disconnected, sock, &QObject::deleteLater);
			}
		});
	}

	w.loadPlugins(!cmd.noPlugin);
	if (!w._noSession)
		w.restoreSession();
	else
		w.applyPanelsState();
	w.show();
	if (!cmd.files.isEmpty())
		w.openFromCommandLine(cmd.files, cmd.opts);
	else if (w._noSession)
		w.openFromCommandLine({}, cmd.opts);
	w.pluginsReady(cmd.pluginMessage);

	return app.exec();
}
