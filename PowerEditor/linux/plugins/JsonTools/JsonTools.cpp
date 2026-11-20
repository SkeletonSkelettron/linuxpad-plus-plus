// SPDX-License-Identifier: GPL-3.0-or-later

#include <algorithm>
#include <functional>
#include <optional>

#include <QClipboard>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QProcess>
#include <QPushButton>
#include <QStandardPaths>
#include <QStringDecoder>
#include <QTimer>
#include <QTreeWidget>
#include <QVBoxLayout>

#include "NppPluginQt.h"
#include "PluginInterface.h"
#include "menuCmdID.h"

namespace
{
	constexpr qsizetype maxInput = 8 * 1024 * 1024;
	constexpr qsizetype maxReply = 32 * 1024 * 1024;
	constexpr int engineTimeoutMs = 8000;
	NppData host;
	QString enginePath;
	bool commandRunning = false;

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

	void report(const QString& text, bool success = false)
	{
		auto* box = new QMessageBox(success ? QMessageBox::Information : QMessageBox::Warning,
			QStringLiteral("JSONTools"), text, QMessageBox::Ok, nppWidget(host._nppHandle));
		box->setTextFormat(Qt::PlainText);
		box->setAttribute(Qt::WA_DeleteOnClose);
		box->open();
	}

	QByteArray readRange(HWND editor, Sci_Position first, Sci_Position last)
	{
		if (first == last)
			return {};
		QByteArray bytes(last - first + 1, Qt::Uninitialized);
		Sci_TextRangeFull range{{first, last}, bytes.data()};
		SendMessage(editor, SCI_GETTEXTRANGEFULL, 0, reinterpret_cast<LPARAM>(&range));
		bytes.chop(1);
		return bytes;
	}

	struct Snapshot
	{
		WPARAM buffer = 0;
		Sci_Position first = 0;
		Sci_Position last = 0;
		Sci_Position documentLength = 0;
		Sci_Position caret = 0;
		QByteArray bytes;
		QString text;
		bool hasSelection = false;

		bool matches(HWND editor) const
		{
			return buffer == static_cast<WPARAM>(send(NPPM_GETCURRENTBUFFERID))
				&& documentLength == SendMessage(editor, SCI_GETLENGTH, 0, 0)
				&& bytes == readRange(editor, first, last);
		}
	};

	std::optional<Snapshot> capture()
	{
		Snapshot snapshot;
		snapshot.buffer = static_cast<WPARAM>(send(NPPM_GETCURRENTBUFFERID));
		const HWND editor = currentEditor();
		if (!snapshot.buffer || !editor)
			return {};
		snapshot.first = SendMessage(editor, SCI_GETSELECTIONSTART, 0, 0);
		snapshot.last = SendMessage(editor, SCI_GETSELECTIONEND, 0, 0);
		snapshot.caret = SendMessage(editor, SCI_GETCURRENTPOS, 0, 0);
		snapshot.documentLength = SendMessage(editor, SCI_GETLENGTH, 0, 0);
		snapshot.hasSelection = snapshot.first != snapshot.last;
		if (!snapshot.hasSelection)
		{
			snapshot.first = 0;
			snapshot.last = snapshot.documentLength;
		}
		if (snapshot.last - snapshot.first > maxInput)
		{
			report(QStringLiteral("Select up to 8 MiB of text."));
			return {};
		}
		snapshot.bytes = readRange(editor, snapshot.first, snapshot.last);
		QStringDecoder utf8(QStringDecoder::Utf8, QStringConverter::Flag::Stateless);
		snapshot.text = utf8(snapshot.bytes);
		if (utf8.hasError())
		{
			report(QStringLiteral("The selection contains invalid UTF-8."));
			return {};
		}
		return snapshot;
	}

	QJsonObject failure(const QString& why)
	{
		return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), why}};
	}

	void request(QWidget* owner, const Snapshot& snapshot, QString operation, const QString& argument,
		bool jsonLines, std::function<void(const QJsonObject&)> finished)
	{
		const QString dotnet = QStandardPaths::findExecutable(QStringLiteral("dotnet"));
		if (dotnet.isEmpty() || !QFileInfo::exists(enginePath))
		{
			finished(failure(QStringLiteral("JSONTools requires the .NET 10 runtime and its companion engine files.")));
			return;
		}
		const QJsonObject input{
			{QStringLiteral("operation"), operation},
			{QStringLiteral("text"), snapshot.text},
			{QStringLiteral("argument"), argument},
			{QStringLiteral("jsonLines"), jsonLines},
			{QStringLiteral("position"), static_cast<int>(snapshot.caret - snapshot.first)},
		};
		auto* worker = new QProcess(owner);
		worker->setProgram(dotnet);
		worker->setArguments({enginePath});
		QObject::connect(worker, &QProcess::started, owner, [worker, input]
		{
			worker->write(QJsonDocument(input).toJson(QJsonDocument::Compact));
			worker->closeWriteChannel();
		});

		auto done = [worker, finished = std::move(finished)](const QJsonObject& output)
		{
			if (worker->property("reported").toBool())
				return;
			worker->setProperty("reported", true);
			worker->deleteLater();
			finished(output);
		};
		QObject::connect(worker, &QProcess::errorOccurred, owner, [worker, done](QProcess::ProcessError error)
		{
			if (error == QProcess::FailedToStart)
				done(failure(worker->errorString()));
		});
		QObject::connect(worker, &QProcess::finished, owner, [worker, done](int code, QProcess::ExitStatus status)
		{
			if (worker->property("timeLimit").toBool())
			{
				done(failure(QStringLiteral("The operation exceeded its time limit. Try a smaller selection.")));
				return;
			}
			if (status != QProcess::NormalExit || code != 0)
			{
				const QString errors = QString::fromUtf8(worker->readAllStandardError().left(2048)).trimmed();
				done(failure(errors.isEmpty() ? QStringLiteral("The JSONTools engine stopped unexpectedly.") : errors));
				return;
			}
			const QByteArray output = worker->readAllStandardOutput();
			const QJsonDocument reply = output.size() <= maxReply ? QJsonDocument::fromJson(output) : QJsonDocument();
			done(reply.isObject() ? reply.object() : failure(QStringLiteral("The JSONTools result could not be displayed.")));
		});
		QTimer::singleShot(engineTimeoutMs, worker, [worker]
		{
			if (worker->state() != QProcess::NotRunning)
			{
				worker->setProperty("timeLimit", true);
				worker->kill();
			}
		});
		worker->start();
	}

	void selectNode(const Snapshot& snapshot, int position)
	{
		if (position < 0 || position > snapshot.bytes.size())
			return;
		const HWND editor = currentEditor();
		if (!snapshot.matches(editor))
			return;
		const Sci_Position target = snapshot.first + position;
		const LRESULT line = SendMessage(editor, SCI_LINEFROMPOSITION, target, 0);
		SendMessage(editor, SCI_ENSUREVISIBLE, line, 0);
		SendMessage(editor, SCI_GOTOPOS, target, 0);
	}

	enum class Output { ReplaceFormatted, ReplaceRaw, Clipboard, Notice, NewJson, NewYaml };

	void replace(const Snapshot& source, QString text, bool normalizeLineEnds)
	{
		const HWND editor = currentEditor();
		if (!source.matches(editor))
		{
			report(QStringLiteral("The document changed while JSONTools was running. Try the command again."));
			return;
		}
		if (SendMessage(editor, SCI_GETREADONLY, 0, 0))
		{
			report(QStringLiteral("The document is read-only."));
			return;
		}
		if (normalizeLineEnds)
		{
			const LRESULT mode = SendMessage(editor, SCI_GETEOLMODE, 0, 0);
			text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
			if (mode == SC_EOL_CRLF)
				text.replace(QLatin1Char('\n'), QStringLiteral("\r\n"));
			else if (mode == SC_EOL_CR)
				text.replace(QLatin1Char('\n'), QLatin1Char('\r'));
		}
		const QByteArray bytes = text.toUtf8();
		if (bytes == source.bytes)
			return;
		SendMessage(editor, SCI_BEGINUNDOACTION, 0, 0);
		SendMessage(editor, SCI_SETTARGETRANGE, source.first, source.last);
		SendMessage(editor, SCI_REPLACETARGET, bytes.size(), reinterpret_cast<LPARAM>(bytes.constData()));
		SendMessage(editor, SCI_ENDUNDOACTION, 0, 0);
		if (source.hasSelection)
			SendMessage(editor, SCI_SETSEL, source.first, source.first + bytes.size());
		else
			SendMessage(editor, SCI_GOTOPOS, std::min<Sci_Position>(source.caret, bytes.size()), 0);
	}

	void run(const char* operation, Output output, const QString& argument = {})
	{
		if (commandRunning)
			return;
		const auto source = capture();
		if (!source)
			return;
		commandRunning = true;
		request(nppWidget(host._nppHandle), *source, QString::fromLatin1(operation), argument, false,
			[source = *source, output](const QJsonObject& reply)
			{
				commandRunning = false;
				if (!reply.value(QStringLiteral("ok")).toBool())
				{
					selectNode(source, reply.value(QStringLiteral("position")).toInt(-1));
					report(reply.value(QStringLiteral("error")).toString());
					return;
				}
				const QString result = reply.value(QStringLiteral("text")).toString();
				switch (output)
				{
					case Output::ReplaceFormatted:
					case Output::ReplaceRaw:
						replace(source, result, output == Output::ReplaceFormatted);
						break;
					case Output::Clipboard:
						QApplication::clipboard()->setText(result);
						break;
					case Output::Notice:
						report(result, true);
						break;
					case Output::NewJson:
					case Output::NewYaml:
					{
						send(NPPM_MENUCOMMAND, 0, IDM_FILE_NEW);
						const QByteArray utf8 = result.toUtf8();
						SendMessage(currentEditor(), SCI_ADDTEXT, utf8.size(), reinterpret_cast<LPARAM>(utf8.constData()));
						send(NPPM_SETCURRENTLANGTYPE, 0, output == Output::NewYaml ? L_YAML : L_JSON);
						break;
					}
				}
			});
	}

	class TreeDialog final : public QDialog
	{
	public:
		TreeDialog(QWidget* parent, bool jsonLines) : QDialog(parent), _jsonLines(jsonLines)
		{
			setAttribute(Qt::WA_DeleteOnClose);
			setWindowTitle(jsonLines ? QStringLiteral("JSONTools · JSON Lines") : QStringLiteral("JSONTools · Tree Viewer"));
			setObjectName(QStringLiteral("jsonToolsTree"));
			resize(780, 610);
			auto* layout = new QVBoxLayout(this);
			auto* controls = new QHBoxLayout;
			_query = new QLineEdit(this);
			_query->setObjectName(QStringLiteral("jsonToolsQuery"));
			_query->setPlaceholderText(QStringLiteral("RemesPath query"));
			_run = new QPushButton(QStringLiteral("&Run Query"), this);
			_run->setObjectName(QStringLiteral("jsonToolsRun"));
			_refresh = new QPushButton(QStringLiteral("&Refresh From Editor"), this);
			_copy = new QPushButton(QStringLiteral("&Copy Result"), this);
			_copy->setObjectName(QStringLiteral("jsonToolsCopy"));
			_copy->setEnabled(false);
			controls->addWidget(_query, 1);
			for (QPushButton* button : {_run, _refresh, _copy})
				controls->addWidget(button);
			layout->addLayout(controls);

			_status = new QLabel(this);
			_status->setTextFormat(Qt::PlainText);
			_status->setWordWrap(true);
			layout->addWidget(_status);
			_tree = new QTreeWidget(this);
			_tree->setObjectName(QStringLiteral("jsonToolsNodes"));
			_tree->setHeaderLabels({QStringLiteral("Key"), QStringLiteral("Value")});
			_tree->setAlternatingRowColors(true);
			_tree->header()->resizeSection(0, 270);
			layout->addWidget(_tree, 1);
			auto* controlsBottom = new QDialogButtonBox(QDialogButtonBox::Close, this);
			layout->addWidget(controlsBottom);
			connect(controlsBottom, &QDialogButtonBox::rejected, this, &QDialog::close);
			connect(_refresh, &QPushButton::clicked, this, [this] { reload(); });
			connect(_run, &QPushButton::clicked, this, [this] { populate(true); });
			connect(_query, &QLineEdit::returnPressed, this, [this] { populate(true); });
			connect(_copy, &QPushButton::clicked, this, [this] { QApplication::clipboard()->setText(_queryResult); });
			connect(_tree, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem* item)
			{
				if (_source)
					selectNode(*_source, item->data(0, Qt::UserRole).toInt());
			});
		}

		void reload()
		{
			_source = capture();
			populate(false);
		}

	private:
		void append(const QJsonObject& node, QTreeWidgetItem* parent)
		{
			const QStringList labels{node.value(QStringLiteral("key")).toString(), node.value(QStringLiteral("value")).toString()};
			auto* item = parent ? new QTreeWidgetItem(parent, labels) : new QTreeWidgetItem(_tree, labels);
			item->setData(0, Qt::UserRole, node.value(QStringLiteral("position")).toInt(-1));
			for (const auto& child : node.value(QStringLiteral("children")).toArray())
				append(child.toObject(), item);
		}

		void populate(bool query)
		{
			if (!_source || _working || (query && _query->text().trimmed().isEmpty()))
				return;
			_working = true;
			_run->setEnabled(false);
			_refresh->setEnabled(false);
			_copy->setEnabled(false);
			_tree->clear();
			_status->setText(QStringLiteral("Working…"));
			request(this, *_source, query ? QStringLiteral("query") : QStringLiteral("tree"), _query->text(), _jsonLines,
				[this, query](const QJsonObject& reply)
				{
					_working = false;
					_run->setEnabled(true);
					_refresh->setEnabled(true);
					if (!reply.value(QStringLiteral("ok")).toBool())
					{
						_status->setText(reply.value(QStringLiteral("error")).toString());
						return;
					}
					_queryResult = reply.value(QStringLiteral("text")).toString();
					append(reply.value(QStringLiteral("tree")).toObject(), nullptr);
					_tree->expandToDepth(1);
					_copy->setEnabled(query);
					_status->setText(reply.value(QStringLiteral("truncated")).toBool()
						? QStringLiteral("Showing the first part of the result tree.") : QString());
				});
		}

		QLineEdit* _query = nullptr;
		QPushButton* _run = nullptr;
		QPushButton* _refresh = nullptr;
		QPushButton* _copy = nullptr;
		QLabel* _status = nullptr;
		QTreeWidget* _tree = nullptr;
		std::optional<Snapshot> _source;
		QString _queryResult;
		bool _jsonLines = false;
		bool _working = false;
	};

	void openTree(bool jsonLines)
	{
		auto* viewer = new TreeDialog(nppWidget(host._nppHandle), jsonLines);
		viewer->show();
		viewer->reload();
	}

	void validate()
	{
		const QString name = QFileDialog::getOpenFileName(nppWidget(host._nppHandle), QStringLiteral("Open JSON Schema"),
			QString(), QStringLiteral("JSON files (*.json *.jsonc *.json5);;All files (*)"));
		if (name.isEmpty())
			return;
		QFile file(name);
		if (!QFileInfo(name).isFile() || !file.open(QIODevice::ReadOnly))
		{
			report(QStringLiteral("Could not open schema %1.").arg(name));
			return;
		}
		const QByteArray bytes = file.read(maxInput + 1);
		if (bytes.size() > maxInput || file.error() != QFileDevice::NoError)
		{
			report(QStringLiteral("Could not read the schema. The size limit is 8 MiB."));
			return;
		}
		QStringDecoder decoder(QStringConverter::encodingForData(bytes).value_or(QStringConverter::Utf8),
			QStringConverter::Flag::Stateless);
		const QString schema = decoder(bytes);
		if (decoder.hasError())
			report(QStringLiteral("The schema is not Unicode text."));
		else
			run("validate", Output::Notice, schema);
	}

	FuncItem commands[] = {
		{L"Pretty-print current JSON file", [] { run("pretty", Output::ReplaceFormatted); }},
		{L"Compress current JSON file", [] { run("compact", Output::ReplaceFormatted); }},
		{L"Check JSON syntax now", [] { run("check", Output::Notice); }},
		{L"Path to current position", [] { run("path", Output::Clipboard); }},
		{L"---", nullptr},
		{L"Open JSON tree viewer", [] { openTree(false); }},
		{L"Parse JSON Lines document", [] { openTree(true); }},
		{L"Array to JSON Lines", [] { run("jsonl", Output::ReplaceFormatted); }},
		{L"Sort array", [] { run("sort", Output::ReplaceFormatted); }},
		{L"---", nullptr},
		{L"Validate JSON against JSON schema", validate},
		{L"Generate schema from JSON", [] { run("schema", Output::NewJson); }},
		{L"Generate random JSON from schema", [] { run("random", Output::NewJson); }},
		{L"JSON to YAML", [] { run("yaml", Output::NewYaml); }},
		{L"---", nullptr},
		{L"Quote text as a JSON string", [] { run("quote", Output::ReplaceRaw); }},
		{L"Unquote JSON string", [] { run("unquote", Output::ReplaceRaw); }},
	};
}

BOOL APIENTRY DllMain(HANDLE module, DWORD reason, LPVOID)
{
	if (reason == DLL_PROCESS_ATTACH)
	{
		wchar_t path[MAX_PATH] = {};
		if (!GetModuleFileName(module, path, MAX_PATH))
			return FALSE;
		enginePath = QFileInfo(QString::fromWCharArray(path)).dir().filePath(QStringLiteral("JsonToolsEngine.dll"));
	}
	return TRUE;
}

extern "C" __declspec(dllexport) void setInfo(NppData data) { host = data; }
extern "C" __declspec(dllexport) const wchar_t* getName() { return L"JSONTools"; }
extern "C" __declspec(dllexport) BOOL isUnicode() { return TRUE; }
extern "C" __declspec(dllexport) FuncItem* getFuncsArray(int* count)
{
	*count = static_cast<int>(std::size(commands));
	return commands;
}
extern "C" __declspec(dllexport) void beNotified(SCNotification*) {}
extern "C" __declspec(dllexport) LRESULT messageProc(UINT, WPARAM, LPARAM) { return TRUE; }
