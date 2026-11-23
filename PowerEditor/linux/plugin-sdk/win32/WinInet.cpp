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

// Notepad++ Linux plugin SDK, npp_win32 library: the synchronous WinInet functions on QNetworkAccessManager.
// A request runs in the thread that opens it: the calls wait in a local event loop of that thread (worker threads
// of plugins too, Qt gives them an event dispatcher), until the headers came (InternetOpenUrl, HttpSendRequest) or
// until there is data to read (InternetReadFile).

#include <QDeadlineTimer>
#include <QEventLoop>
#include <QNetworkAccessManager>
#include <QNetworkProxy>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPointer>
#include <QRegularExpression>
#include <QTimer>
#include <QUrl>

#include <cstring>
#include <memory>

#include "W32Internal.h"
#include <wininet.h>

using namespace w32;

namespace
{
	constexpr quint32 kInetMagic = 0x494E4554u; // "INET"

	struct InetHandle
	{
		quint32 magic = kInetMagic;
		enum class Type { Session, Connection, Request } type = Type::Session;
		InetHandle* parent = nullptr;
		virtual ~InetHandle() { magic = 0; }
	};

	struct Session : InetHandle
	{
		QString agent;
		DWORD access = INTERNET_OPEN_TYPE_PRECONFIG;
		QString proxy;
		QString proxyUser;
		QString proxyPassword;
		int connectTimeout = 60000;
		int receiveTimeout = 60000;
	};

	struct Connection : InetHandle
	{
		QString server;
		INTERNET_PORT port = 0;
		QString user;
		QString password;
		DWORD service = INTERNET_SERVICE_HTTP;
		DWORD flags = 0;
	};

	struct Request : InetHandle
	{
		std::unique_ptr<QNetworkAccessManager> manager;
		QPointer<QNetworkReply> reply;
		QNetworkRequest request;
		QByteArray verb = "GET";
		QByteArray buffer;   // received, not read yet
		bool headersDone = false;
		bool finished = false;
		QNetworkReply::NetworkError error = QNetworkReply::NoError;
		QString errorText;
		DWORD flags = 0;
		~Request() override
		{
			if (reply)
			{
				reply->abort();
				delete reply;
			}
		}
	};

	thread_local QString lastResponseInfo;

	InetHandle* handleOf(HINTERNET h)
	{
		auto* p = static_cast<InetHandle*>(h);
		return (p && p->magic == kInetMagic) ? p : nullptr;
	}

	Session* sessionOf(InetHandle* h)
	{
		while (h && h->type != InetHandle::Type::Session)
			h = h->parent;
		return static_cast<Session*>(h);
	}

	DWORD errorOf(QNetworkReply::NetworkError e)
	{
		switch (e)
		{
			case QNetworkReply::ConnectionRefusedError: return ERROR_INTERNET_CANNOT_CONNECT;
			case QNetworkReply::RemoteHostClosedError: return ERROR_INTERNET_CONNECTION_ABORTED;
			case QNetworkReply::HostNotFoundError: return ERROR_INTERNET_NAME_NOT_RESOLVED;
			case QNetworkReply::TimeoutError: return ERROR_INTERNET_TIMEOUT;
			case QNetworkReply::OperationCanceledError: return ERROR_INTERNET_OPERATION_CANCELLED;
			case QNetworkReply::SslHandshakeFailedError: return ERROR_INTERNET_SEC_CERT_ERRORS;
			case QNetworkReply::ProxyConnectionRefusedError:
			case QNetworkReply::ProxyNotFoundError: return ERROR_INTERNET_PROXY_SERVER_UNREACHABLE;
			case QNetworkReply::ProxyAuthenticationRequiredError:
			case QNetworkReply::AuthenticationRequiredError: return ERROR_INTERNET_LOGIN_FAILURE;
			case QNetworkReply::ProtocolUnknownError: return ERROR_INTERNET_UNRECOGNIZED_SCHEME;
			case QNetworkReply::ProtocolInvalidOperationError: return ERROR_INTERNET_INVALID_OPERATION;
			default: return ERROR_INTERNET_CONNECTION_RESET;
		}
	}

	// HTTP status codes are answers, not errors of WinInet
	bool isTransportError(QNetworkReply::NetworkError e)
	{
		return e != QNetworkReply::NoError && (e < QNetworkReply::ContentAccessDenied || e > QNetworkReply::UnknownServerError);
	}

	QNetworkProxy proxyOf(const Session* s)
	{
		if (!s || s->access == INTERNET_OPEN_TYPE_DIRECT)
			return QNetworkProxy(QNetworkProxy::NoProxy);
		if (s->access == INTERNET_OPEN_TYPE_PROXY && !s->proxy.isEmpty())
		{
			// "host:port", "http=host:port", several separated by spaces or ';': the first one
			QString p = s->proxy.section(QRegularExpression(QStringLiteral("[ ;]")), 0, 0, QString::SectionSkipEmpty);
			if (p.contains(QLatin1Char('=')))
				p = p.section(QLatin1Char('='), 1);
			p.remove(QRegularExpression(QStringLiteral("^[a-z]+://")));
			const QString host = p.section(QLatin1Char(':'), 0, 0);
			const int port = p.section(QLatin1Char(':'), 1).toInt();
			return QNetworkProxy(QNetworkProxy::HttpProxy, host, static_cast<quint16>(port ? port : 8080), s->proxyUser, s->proxyPassword);
		}
		return QNetworkProxy(QNetworkProxy::DefaultProxy);
	}

	void addHeaders(QNetworkRequest& request, const QString& headers)
	{
		for (const QString& line : headers.split(QRegularExpression(QStringLiteral("\r?\n")), Qt::SkipEmptyParts))
		{
			const int colon = line.indexOf(QLatin1Char(':'));
			if (colon > 0)
				request.setRawHeader(line.left(colon).trimmed().toLatin1(), line.mid(colon + 1).trimmed().toUtf8());
		}
	}

	QString headersOf(LPCWSTR headers, DWORD length)
	{
		if (!headers)
			return QString();
		return length == static_cast<DWORD>(-1) || length == 0 ? qs(headers) : qs(headers, static_cast<int>(length));
	}

	// waits in an event loop of this thread for done() or the timeout; false: timeout
	template <typename Done>
	bool waitFor(Request* r, int timeoutMs, Done done)
	{
		if (done())
			return true;
		QEventLoop loop;
		QTimer timer;
		timer.setSingleShot(true);
		QObject::connect(&timer, &QTimer::timeout, &loop, &QEventLoop::quit);
		if (r->reply)
		{
			QObject::connect(r->reply, &QNetworkReply::readyRead, &loop, &QEventLoop::quit);
			QObject::connect(r->reply, &QNetworkReply::metaDataChanged, &loop, &QEventLoop::quit);
			QObject::connect(r->reply, &QNetworkReply::finished, &loop, &QEventLoop::quit);
		}
		const QDeadlineTimer deadline(timeoutMs > 0 ? timeoutMs : 60000);
		while (!done())
		{
			if (deadline.hasExpired() || !r->reply)
				return done();
			timer.start(static_cast<int>(std::max<qint64>(1, deadline.remainingTime())));
			loop.exec(QEventLoop::ExcludeUserInputEvents);
		}
		return true;
	}

	void watch(Request* r)
	{
		QNetworkReply* reply = r->reply;
		QObject::connect(reply, &QNetworkReply::metaDataChanged, reply, [r] { r->headersDone = true; });
		QObject::connect(reply, &QNetworkReply::readyRead, reply, [r]
		{
			r->headersDone = true;
			if (r->reply)
				r->buffer += r->reply->readAll();
		});
		QObject::connect(reply, &QNetworkReply::finished, reply, [r]
		{
			r->headersDone = true;
			r->finished = true;
			if (r->reply)
			{
				r->buffer += r->reply->readAll();
				r->error = r->reply->error();
				r->errorText = r->reply->errorString();
			}
		});
	}

	// sends the request and waits for the answer's headers
	bool start(Request* r, const QByteArray& body)
	{
		Session* s = sessionOf(r);
		r->manager = std::make_unique<QNetworkAccessManager>();
		r->manager->setProxy(proxyOf(s));
		r->manager->setRedirectPolicy((r->flags & INTERNET_FLAG_NO_AUTO_REDIRECT) ? QNetworkRequest::ManualRedirectPolicy
			: QNetworkRequest::NoLessSafeRedirectPolicy);
		if (s && !s->agent.isEmpty() && !r->request.hasRawHeader("User-Agent"))
			r->request.setHeader(QNetworkRequest::UserAgentHeader, s->agent);
		if (r->flags & (INTERNET_FLAG_RELOAD | INTERNET_FLAG_PRAGMA_NOCACHE | INTERNET_FLAG_NO_CACHE_WRITE))
			r->request.setAttribute(QNetworkRequest::CacheLoadControlAttribute, QNetworkRequest::AlwaysNetwork);
		r->request.setTransferTimeout(s ? s->receiveTimeout : 60000);
		r->reply = r->manager->sendCustomRequest(r->request, r->verb, body);
		watch(r);
		waitFor(r, s ? s->connectTimeout + s->receiveTimeout : 120000, [r] { return r->headersDone || r->finished; });
		if (!r->headersDone && !r->finished)
		{
			SetLastError(ERROR_INTERNET_TIMEOUT);
			return false;
		}
		if (r->finished && isTransportError(r->error)
			&& !r->reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).isValid())
		{
			lastResponseInfo = r->errorText;
			SetLastError(errorOf(r->error));
			return false;
		}
		return true;
	}

	QString headerValue(Request* r, DWORD query)
	{
		QNetworkReply* reply = r->reply;
		if (!reply)
			return QString();
		const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
		switch (query)
		{
			case HTTP_QUERY_STATUS_CODE:
				return status ? QString::number(status) : (r->finished && r->error == QNetworkReply::NoError ? QStringLiteral("200") : QString());
			case HTTP_QUERY_STATUS_TEXT:
				return reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString();
			case HTTP_QUERY_RAW_HEADERS_CRLF:
			case HTTP_QUERY_RAW_HEADERS:
			{
				const QString sep = query == HTTP_QUERY_RAW_HEADERS_CRLF ? QStringLiteral("\r\n") : QString(QChar(0));
				QString all = QStringLiteral("HTTP/1.1 %1 %2").arg(status).arg(reply->attribute(QNetworkRequest::HttpReasonPhraseAttribute).toString()) + sep;
				for (const auto& pair : reply->rawHeaderPairs())
					all += QString::fromUtf8(pair.first) + QStringLiteral(": ") + QString::fromUtf8(pair.second) + sep;
				return all + sep;
			}
			default:
				break;
		}
		static const struct { DWORD query; const char* name; } names[] = {
			{HTTP_QUERY_CONTENT_TYPE, "Content-Type"}, {HTTP_QUERY_CONTENT_LENGTH, "Content-Length"},
			{HTTP_QUERY_CONTENT_LANGUAGE, "Content-Language"}, {HTTP_QUERY_DATE, "Date"}, {HTTP_QUERY_EXPIRES, "Expires"},
			{HTTP_QUERY_LAST_MODIFIED, "Last-Modified"}, {HTTP_QUERY_CONTENT_ENCODING, "Content-Encoding"},
			{HTTP_QUERY_LOCATION, "Location"}, {HTTP_QUERY_SERVER, "Server"}, {HTTP_QUERY_ETAG, "ETag"},
			{HTTP_QUERY_CONTENT_TRANSFER_ENCODING, "Content-Transfer-Encoding"}, {HTTP_QUERY_MIME_VERSION, "MIME-Version"},
		};
		for (const auto& n : names)
		{
			if (n.query == query && reply->hasRawHeader(n.name))
				return QString::fromUtf8(reply->rawHeader(n.name));
		}
		// FTP and files: the size of the data received
		if (query == HTTP_QUERY_CONTENT_LENGTH)
		{
			const QVariant length = reply->header(QNetworkRequest::ContentLengthHeader);
			if (length.isValid())
				return QString::number(length.toLongLong());
		}
		return QString();
	}

	BOOL queryInfo(HINTERNET h, DWORD level, LPVOID buffer, LPDWORD length, bool wide)
	{
		auto* r = static_cast<Request*>(handleOf(h));
		if (!r || r->type != InetHandle::Type::Request || !length)
		{
			SetLastError(ERROR_INVALID_HANDLE);
			return FALSE;
		}
		const DWORD query = level & 0xFFFF;
		const QString value = headerValue(r, query);
		if (value.isEmpty())
		{
			SetLastError(ERROR_HTTP_HEADER_NOT_FOUND);
			return FALSE;
		}
		if (level & (HTTP_QUERY_FLAG_NUMBER | HTTP_QUERY_FLAG_NUMBER64))
		{
			const bool is64 = (level & HTTP_QUERY_FLAG_NUMBER64) != 0;
			const DWORD size = is64 ? sizeof(ULONGLONG) : sizeof(DWORD);
			if (*length < size || !buffer)
			{
				*length = size;
				SetLastError(ERROR_INSUFFICIENT_BUFFER);
				return FALSE;
			}
			if (is64)
				*static_cast<ULONGLONG*>(buffer) = value.toULongLong();
			else
				*static_cast<DWORD*>(buffer) = value.toUInt();
			*length = size;
			return TRUE;
		}
		if (wide)
		{
			const std::wstring w = ws(value);
			const DWORD needed = static_cast<DWORD>((w.size() + 1) * sizeof(wchar_t));
			if (*length < needed || !buffer)
			{
				*length = needed;
				SetLastError(ERROR_INSUFFICIENT_BUFFER);
				return FALSE;
			}
			std::memcpy(buffer, w.c_str(), needed);
			*length = needed - sizeof(wchar_t);
			return TRUE;
		}
		const QByteArray a = value.toUtf8();
		const DWORD needed = static_cast<DWORD>(a.size() + 1);
		if (*length < needed || !buffer)
		{
			*length = needed;
			SetLastError(ERROR_INSUFFICIENT_BUFFER);
			return FALSE;
		}
		std::memcpy(buffer, a.constData(), needed);
		*length = needed - 1;
		return TRUE;
	}
}

HINTERNET InternetOpenW(LPCWSTR agent, DWORD accessType, LPCWSTR proxy, LPCWSTR proxyBypass, DWORD)
{
	Q_UNUSED(proxyBypass);
	auto* s = new Session;
	s->type = InetHandle::Type::Session;
	s->agent = agent ? qs(agent) : QString();
	s->access = accessType;
	s->proxy = proxy ? qs(proxy) : QString();
	return s;
}

HINTERNET InternetOpenA(LPCSTR agent, DWORD accessType, LPCSTR proxy, LPCSTR proxyBypass, DWORD flags)
{
	const std::wstring a = agent ? ws(qsA(agent)) : std::wstring();
	const std::wstring p = proxy ? ws(qsA(proxy)) : std::wstring();
	const std::wstring b = proxyBypass ? ws(qsA(proxyBypass)) : std::wstring();
	return InternetOpenW(agent ? a.c_str() : nullptr, accessType, proxy ? p.c_str() : nullptr, proxyBypass ? b.c_str() : nullptr, flags);
}

HINTERNET InternetOpenUrlW(HINTERNET session, LPCWSTR url, LPCWSTR headers, DWORD headersLength, DWORD flags, DWORD_PTR)
{
	Session* s = static_cast<Session*>(handleOf(session));
	if (!s || s->type != InetHandle::Type::Session || !url)
	{
		SetLastError(ERROR_INVALID_HANDLE);
		return nullptr;
	}
	const QUrl u(qs(url));
	if (!u.isValid() || u.scheme().isEmpty())
	{
		SetLastError(ERROR_INTERNET_INVALID_URL);
		return nullptr;
	}
	auto* r = new Request;
	r->type = InetHandle::Type::Request;
	r->parent = s;
	r->flags = flags;
	r->request = QNetworkRequest(u);
	addHeaders(r->request, headersOf(headers, headersLength));
	if (!start(r, QByteArray()))
	{
		const DWORD e = GetLastError();
		delete r;
		SetLastError(e);
		return nullptr;
	}
	return r;
}

HINTERNET InternetOpenUrlA(HINTERNET session, LPCSTR url, LPCSTR headers, DWORD headersLength, DWORD flags, DWORD_PTR context)
{
	const std::wstring u = url ? ws(qsA(url)) : std::wstring();
	const std::wstring h = headers ? ws(headersLength && headersLength != static_cast<DWORD>(-1) ? qsA(headers, static_cast<int>(headersLength)) : qsA(headers)) : std::wstring();
	return InternetOpenUrlW(session, url ? u.c_str() : nullptr, headers ? h.c_str() : nullptr, static_cast<DWORD>(-1), flags, context);
}

HINTERNET InternetConnectW(HINTERNET session, LPCWSTR server, INTERNET_PORT port, LPCWSTR user, LPCWSTR password, DWORD service, DWORD flags, DWORD_PTR)
{
	Session* s = static_cast<Session*>(handleOf(session));
	if (!s || s->type != InetHandle::Type::Session || !server)
	{
		SetLastError(ERROR_INVALID_HANDLE);
		return nullptr;
	}
	auto* c = new Connection;
	c->type = InetHandle::Type::Connection;
	c->parent = s;
	c->server = qs(server);
	c->port = port;
	c->user = user ? qs(user) : QString();
	c->password = password ? qs(password) : QString();
	c->service = service;
	c->flags = flags;
	return c;
}

HINTERNET HttpOpenRequestW(HINTERNET connect, LPCWSTR verb, LPCWSTR object, LPCWSTR, LPCWSTR referrer, LPCWSTR*, DWORD flags, DWORD_PTR)
{
	auto* c = static_cast<Connection*>(handleOf(connect));
	if (!c || c->type != InetHandle::Type::Connection)
	{
		SetLastError(ERROR_INVALID_HANDLE);
		return nullptr;
	}
	const bool secure = (flags & INTERNET_FLAG_SECURE) || c->port == INTERNET_DEFAULT_HTTPS_PORT;
	QUrl u;
	u.setScheme(c->service == INTERNET_SERVICE_FTP ? QStringLiteral("ftp") : (secure ? QStringLiteral("https") : QStringLiteral("http")));
	u.setHost(c->server);
	if (c->port && c->port != (secure ? INTERNET_DEFAULT_HTTPS_PORT : INTERNET_DEFAULT_HTTP_PORT))
		u.setPort(c->port);
	if (!c->user.isEmpty())
	{
		u.setUserName(c->user);
		u.setPassword(c->password);
	}
	const QString path = object ? qs(object) : QStringLiteral("/");
	const QUrl full = u.resolved(QUrl(path.startsWith(QLatin1Char('/')) ? path : QLatin1Char('/') + path));
	auto* r = new Request;
	r->type = InetHandle::Type::Request;
	r->parent = c;
	r->flags = flags;
	r->verb = verb && *verb ? qs(verb).toLatin1() : QByteArray("GET");
	r->request = QNetworkRequest(full);
	if (referrer)
		r->request.setRawHeader("Referer", qs(referrer).toUtf8());
	return r;
}

BOOL HttpAddRequestHeadersW(HINTERNET request, LPCWSTR headers, DWORD headersLength, DWORD)
{
	auto* r = static_cast<Request*>(handleOf(request));
	if (!r || r->type != InetHandle::Type::Request)
	{
		SetLastError(ERROR_INVALID_HANDLE);
		return FALSE;
	}
	addHeaders(r->request, headersOf(headers, headersLength));
	return TRUE;
}

BOOL HttpSendRequestW(HINTERNET request, LPCWSTR headers, DWORD headersLength, LPVOID optional, DWORD optionalLength)
{
	auto* r = static_cast<Request*>(handleOf(request));
	if (!r || r->type != InetHandle::Type::Request || r->reply)
	{
		SetLastError(ERROR_INVALID_HANDLE);
		return FALSE;
	}
	addHeaders(r->request, headersOf(headers, headersLength));
	const QByteArray body = optional ? QByteArray(static_cast<const char*>(optional), static_cast<int>(optionalLength)) : QByteArray();
	return start(r, body) ? TRUE : FALSE;
}

BOOL HttpSendRequestA(HINTERNET request, LPCSTR headers, DWORD headersLength, LPVOID optional, DWORD optionalLength)
{
	const std::wstring h = headers ? ws(headersLength && headersLength != static_cast<DWORD>(-1) ? qsA(headers, static_cast<int>(headersLength)) : qsA(headers)) : std::wstring();
	return HttpSendRequestW(request, headers ? h.c_str() : nullptr, static_cast<DWORD>(-1), optional, optionalLength);
}

BOOL InternetReadFile(HINTERNET file, LPVOID buffer, DWORD toRead, LPDWORD read)
{
	auto* r = static_cast<Request*>(handleOf(file));
	if (!r || r->type != InetHandle::Type::Request || !read)
	{
		SetLastError(ERROR_INVALID_HANDLE);
		return FALSE;
	}
	*read = 0;
	Session* s = sessionOf(r);
	if (!waitFor(r, s ? s->receiveTimeout : 60000, [r] { return !r->buffer.isEmpty() || r->finished; }))
	{
		SetLastError(ERROR_INTERNET_TIMEOUT);
		return FALSE;
	}
	if (r->buffer.isEmpty() && r->finished && isTransportError(r->error) && r->error != QNetworkReply::OperationCanceledError)
	{
		lastResponseInfo = r->errorText;
		SetLastError(errorOf(r->error));
		return FALSE;
	}
	const DWORD n = std::min<DWORD>(toRead, static_cast<DWORD>(r->buffer.size()));
	if (n && buffer)
	{
		std::memcpy(buffer, r->buffer.constData(), n);
		r->buffer.remove(0, static_cast<int>(n));
	}
	*read = n;
	return TRUE;
}

BOOL InternetQueryDataAvailable(HINTERNET file, LPDWORD available, DWORD, DWORD_PTR)
{
	auto* r = static_cast<Request*>(handleOf(file));
	if (!r || r->type != InetHandle::Type::Request)
	{
		SetLastError(ERROR_INVALID_HANDLE);
		return FALSE;
	}
	Session* s = sessionOf(r);
	waitFor(r, s ? s->receiveTimeout : 60000, [r] { return !r->buffer.isEmpty() || r->finished; });
	if (available)
		*available = static_cast<DWORD>(r->buffer.size());
	return TRUE;
}

BOOL HttpQueryInfoW(HINTERNET request, DWORD level, LPVOID buffer, LPDWORD length, LPDWORD)
{
	return queryInfo(request, level, buffer, length, true);
}

BOOL HttpQueryInfoA(HINTERNET request, DWORD level, LPVOID buffer, LPDWORD length, LPDWORD)
{
	return queryInfo(request, level, buffer, length, false);
}

BOOL InternetSetOptionW(HINTERNET handle, DWORD option, LPVOID buffer, DWORD length)
{
	InetHandle* h = handleOf(handle);
	Session* s = sessionOf(h);
	auto number = [&]() -> int { return (buffer && length >= sizeof(DWORD)) ? static_cast<int>(*static_cast<DWORD*>(buffer)) : 0; };
	auto text = [&]() -> QString { return buffer ? qs(static_cast<LPCWSTR>(buffer)) : QString(); };
	switch (option)
	{
		case INTERNET_OPTION_SETTINGS_CHANGED:
		case INTERNET_OPTION_REFRESH:
		case INTERNET_OPTION_PROXY_SETTINGS_CHANGED:
			return TRUE; // the proxy of the system is read for each request
		case INTERNET_OPTION_CONNECT_TIMEOUT:
			if (s)
				s->connectTimeout = number();
			return s ? TRUE : FALSE;
		case INTERNET_OPTION_RECEIVE_TIMEOUT:
		case INTERNET_OPTION_DATA_RECEIVE_TIMEOUT:
			if (s)
				s->receiveTimeout = number();
			return s ? TRUE : FALSE;
		case INTERNET_OPTION_PROXY_USERNAME:
			if (s)
				s->proxyUser = text();
			return s ? TRUE : FALSE;
		case INTERNET_OPTION_PROXY_PASSWORD:
			if (s)
				s->proxyPassword = text();
			return s ? TRUE : FALSE;
		case INTERNET_OPTION_USER_AGENT:
			if (s)
				s->agent = text();
			return s ? TRUE : FALSE;
		case INTERNET_OPTION_PROXY:
			if (s && buffer)
			{
				const auto* info = static_cast<const INTERNET_PROXY_INFO*>(buffer);
				s->access = info->dwAccessType;
				s->proxy = info->lpszProxy ? qs(info->lpszProxy) : QString();
			}
			return s ? TRUE : FALSE;
		default:
			return h ? TRUE : FALSE; // send timeouts, buffer sizes, connect retries: Qt's own
	}
}

BOOL InternetQueryOptionW(HINTERNET handle, DWORD option, LPVOID buffer, LPDWORD length)
{
	InetHandle* h = handleOf(handle);
	Session* s = sessionOf(h);
	if (!s || !length)
	{
		SetLastError(ERROR_INVALID_HANDLE);
		return FALSE;
	}
	if (option == INTERNET_OPTION_CONNECT_TIMEOUT || option == INTERNET_OPTION_RECEIVE_TIMEOUT)
	{
		if (*length < sizeof(DWORD) || !buffer)
		{
			*length = sizeof(DWORD);
			SetLastError(ERROR_INSUFFICIENT_BUFFER);
			return FALSE;
		}
		*static_cast<DWORD*>(buffer) = static_cast<DWORD>(option == INTERNET_OPTION_CONNECT_TIMEOUT ? s->connectTimeout : s->receiveTimeout);
		*length = sizeof(DWORD);
		return TRUE;
	}
	SetLastError(ERROR_INTERNET_INVALID_OPTION);
	return FALSE;
}

BOOL InternetCloseHandle(HINTERNET handle)
{
	InetHandle* h = handleOf(handle);
	if (!h)
	{
		SetLastError(ERROR_INVALID_HANDLE);
		return FALSE;
	}
	delete h;
	return TRUE;
}

BOOL InternetGetLastResponseInfoW(LPDWORD error, LPWSTR buffer, LPDWORD length)
{
	if (error)
		*error = 0;
	if (!length)
		return FALSE;
	const std::wstring w = ws(lastResponseInfo);
	if (!buffer || *length <= w.size())
	{
		*length = static_cast<DWORD>(w.size() + 1);
		SetLastError(ERROR_INSUFFICIENT_BUFFER);
		return FALSE;
	}
	std::wmemcpy(buffer, w.c_str(), w.size() + 1);
	*length = static_cast<DWORD>(w.size());
	return TRUE;
}

BOOL InternetGetConnectedState(LPDWORD flags, DWORD)
{
	if (flags)
		*flags = INTERNET_CONNECTION_LAN | INTERNET_CONNECTION_CONFIGURED;
	return TRUE;
}

BOOL InternetCheckConnectionW(LPCWSTR, DWORD, DWORD)
{
	return TRUE;
}

BOOL InternetCanonicalizeUrlW(LPCWSTR url, LPWSTR buffer, LPDWORD length, DWORD)
{
	if (!url || !length)
		return FALSE;
	const QString canonical = QUrl::fromUserInput(qs(url)).toString(QUrl::FullyEncoded);
	const std::wstring w = ws(canonical);
	if (!buffer || *length <= w.size())
	{
		*length = static_cast<DWORD>(w.size() + 1);
		SetLastError(ERROR_INSUFFICIENT_BUFFER);
		return FALSE;
	}
	std::wmemcpy(buffer, w.c_str(), w.size() + 1);
	*length = static_cast<DWORD>(w.size());
	return TRUE;
}
