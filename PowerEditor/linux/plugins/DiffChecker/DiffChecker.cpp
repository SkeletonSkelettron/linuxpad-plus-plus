// SPDX-License-Identifier: GPL-3.0-or-later

#include "DiffModel.h"

#include <algorithm>
#include <memory>
#include <string>

#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFutureWatcher>
#include <QGridLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPointer>
#include <QPushButton>
#include <QSet>
#include <QTableView>
#include <QtConcurrent/QtConcurrentRun>

#include "NppPluginQt.h"
#include "PluginInterface.h"

namespace
{
	NppData host;

	LRESULT send(UINT message, WPARAM w = 0, LPARAM l = 0)
	{
		return SendMessage(host._nppHandle, message, w, l);
	}

	class CompareDialog final : public QDialog
	{
	public:
		explicit CompareDialog(QWidget* parent) : QDialog(parent), _watcher(this)
		{
			setAttribute(Qt::WA_DeleteOnClose);
			setWindowTitle(QStringLiteral("Diff Checker"));
			setObjectName(QStringLiteral("diffCheckerDialog"));
			resize(1050, 660);

			auto* layout = new QVBoxLayout(this);
			auto* sources = new QGridLayout;
			for (int side : {0, 1})
			{
				auto* label = new QLabel(side == 0 ? QStringLiteral("&Left:") : QStringLiteral("&Right:"), this);
				_sources[side] = new QComboBox(this);
				_sources[side]->setObjectName(side == 0 ? QStringLiteral("diffLeft") : QStringLiteral("diffRight"));
				_sources[side]->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
				label->setBuddy(_sources[side]);
				_browse[side] = new QPushButton(QStringLiteral("Browse…"), this);
				connect(_browse[side], &QPushButton::clicked, this, [this, side] { browse(side); });
				sources->addWidget(label, side, 0);
				sources->addWidget(_sources[side], side, 1);
				sources->addWidget(_browse[side], side, 2);
			}
			sources->setColumnStretch(1, 1);
			layout->addLayout(sources);

			auto* commands = new QHBoxLayout;
			_compare = new QPushButton(QStringLiteral("&Compare"), this);
			_compare->setObjectName(QStringLiteral("diffCompare"));
			_previous = new QPushButton(QStringLiteral("Previous change"), this);
			_next = new QPushButton(QStringLiteral("Next change"), this);
			_next->setObjectName(QStringLiteral("diffNext"));
			_status = new QLabel(this);
			_status->setObjectName(QStringLiteral("diffStatus"));
			_status->setTextFormat(Qt::PlainText);
			_status->setWordWrap(true);
			for (QPushButton* command : {_compare, _previous, _next})
				commands->addWidget(command);
			commands->addWidget(_status, 1);
			layout->addLayout(commands);

			_model = new DiffChecker::DiffModel(this);
			_table = new QTableView(this);
			_table->setObjectName(QStringLiteral("diffRows"));
			_table->setModel(_model);
			_table->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
			_table->setWordWrap(false);
			_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
			_table->setSelectionBehavior(QAbstractItemView::SelectRows);
			_table->setSelectionMode(QAbstractItemView::SingleSelection);
			_table->verticalHeader()->hide();
			auto* header = _table->horizontalHeader();
			for (int column : {0, 2})
			{
				header->setSectionResizeMode(column, QHeaderView::Fixed);
				header->resizeSection(column, 62);
			}
			header->setSectionResizeMode(1, QHeaderView::Stretch);
			header->setSectionResizeMode(3, QHeaderView::Stretch);
			layout->addWidget(_table, 1);

			auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
			layout->addWidget(buttons);
			connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::close);
			connect(_compare, &QPushButton::clicked, this, [this] { startComparison(); });
			connect(_previous, &QPushButton::clicked, this, [this] { jump(false); });
			connect(_next, &QPushButton::clicked, this, [this] { jump(true); });
			connect(_table, &QTableView::doubleClicked, this, [this](const QModelIndex& index) { openLine(index); });
			connect(&_watcher, &QFutureWatcher<DiffChecker::Comparison>::finished, this, [this] { showComparison(); });
			refreshSources();
			setWorking(false);
		}

		~CompareDialog() override
		{
			if (_cancel)
				_cancel->store(true, std::memory_order_relaxed);
		}

		void refreshSources()
		{
			const LRESULT active = send(NPPM_GETCURRENTBUFFERID);
			QSet<LRESULT> seen;
			const QVariant previous[2] = {_sources[0]->currentData(), _sources[1]->currentData()};
			for (auto* list : _sources)
				list->clear();
			for (int view : {0, 1})
			{
				const LRESULT count = send(NPPM_GETNBOPENFILES, 0, view == 0 ? PRIMARY_VIEW : SECOND_VIEW);
				for (LRESULT tab = 0; tab < count; ++tab)
				{
					const LRESULT id = send(NPPM_GETBUFFERIDFROMPOS, static_cast<WPARAM>(tab), view);
					if (!id || seen.contains(id))
						continue;
					seen.insert(id);
					const LRESULT length = send(NPPM_GETFULLPATHFROMBUFFERID, id);
					if (length < 0)
						continue;
					std::wstring name(static_cast<size_t>(length) + 1, L'\0');
					send(NPPM_GETFULLPATHFROMBUFFERID, id, reinterpret_cast<LPARAM>(name.data()));
					const QString caption = QString::fromWCharArray(name.c_str());
					for (auto* list : _sources)
						list->addItem(caption, QVariant::fromValue<qlonglong>(id));
				}
			}
			for (const QString& path : _files)
			{
				for (auto* list : _sources)
					list->addItem(QStringLiteral("%1 (on disk)").arg(path), path);
			}
			const int activeIndex = _sources[1]->findData(QVariant::fromValue<qlonglong>(active));
			for (int side : {0, 1})
			{
				int index = _sources[side]->findData(previous[side]);
				if (index < 0)
				{
					if (side == 1 && activeIndex >= 0)
						index = activeIndex;
					else if (activeIndex > 0)
						index = activeIndex - 1;
					else
						index = activeIndex == 0 && _sources[side]->count() > 1 ? 1 : 0;
				}
				_sources[side]->setCurrentIndex(index);
			}
		}

	private:
		void browse(int side)
		{
			const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Choose a file to compare"));
			if (path.isEmpty())
				return;
			if (!_files.contains(path))
				_files.append(path);
			refreshSources();
			_sources[side]->setCurrentIndex(_sources[side]->findData(path));
		}

		bool takeInput(int side, DiffChecker::Input& input)
		{
			const QVariant selection = _sources[side]->currentData();
			if (!selection.isValid())
			{
				_status->setText(QStringLiteral("Choose a source on each side."));
				return false;
			}
			if (selection.typeId() == QMetaType::QString)
			{
				input.path = selection.toString();
				_bufferIds[side] = 0;
				return true;
			}
			const WPARAM id = static_cast<WPARAM>(selection.toLongLong());
			const LRESULT length = send(NPPM_LINUX_GETBUFFERTEXT, id);
			if (length < 0 || length > DiffChecker::maxBytes)
			{
				_status->setText(length < 0 ? QStringLiteral("The selected tab is no longer available.")
					: QStringLiteral("Each input can be up to 8 MiB."));
				return false;
			}
			input.utf8.resize(length);
			const NppBufferText copy{input.utf8.data(), static_cast<size_t>(length)};
			if (send(NPPM_LINUX_GETBUFFERTEXT, id, reinterpret_cast<LPARAM>(&copy)) != length)
			{
				_status->setText(QStringLiteral("The selected tab changed. Try comparing again."));
				return false;
			}
			_bufferIds[side] = id;
			return true;
		}

		void startComparison()
		{
			if (_watcher.isRunning())
				return;
			_model->setComparison({});
			setWorking(false);
			DiffChecker::Input left, right;
			if (!takeInput(0, left) || !takeInput(1, right))
				return;
			_status->setText(QStringLiteral("Comparing…"));
			setWorking(true);
			_cancel = std::make_shared<std::atomic_bool>(false);
			_watcher.setFuture(QtConcurrent::run([left = std::move(left), right = std::move(right), cancel = _cancel]
			{
				return DiffChecker::compare(left, right, *cancel);
			}));
		}

		void showComparison()
		{
			try {
				_model->setComparison(_watcher.result());
				const auto& result = _model->comparison();
				if (!result.error.isEmpty())
					_status->setText(result.error);
				else if (result.changeRows.empty())
					_status->setText(QStringLiteral("No differences"));
				else
					_status->setText(QStringLiteral("%1 change blocks · %2 removed · %3 added")
						.arg(result.changeRows.size()).arg(result.removed).arg(result.added));
			}
			catch (const std::exception&)
			{
				_status->setText(QStringLiteral("The comparison could not be completed."));
			}
			setWorking(false);
		}

		void setWorking(bool working)
		{
			for (int side : {0, 1})
			{
				_sources[side]->setEnabled(!working);
				_browse[side]->setEnabled(!working);
			}
			_compare->setEnabled(!working);
			const bool canJump = !working && !_model->comparison().changeRows.empty();
			_next->setEnabled(canJump);
			_previous->setEnabled(canJump);
		}

		void jump(bool forward)
		{
			const auto& changes = _model->comparison().changeRows;
			if (changes.empty())
				return;
			const int current = _table->currentIndex().row();
			int row = 0;
			if (forward)
			{
				const auto next = std::upper_bound(changes.begin(), changes.end(), current);
				row = next != changes.end() ? *next : changes.front();
			}
			else
			{
				const auto previous = std::lower_bound(changes.begin(), changes.end(), current);
				row = previous != changes.begin() ? *std::prev(previous) : changes.back();
			}
			const QModelIndex index = _model->index(row, 1);
			_table->setCurrentIndex(index);
			_table->scrollTo(index, QAbstractItemView::PositionAtCenter);
		}

		void openLine(const QModelIndex& index)
		{
			const int side = index.column() < 2 ? 0 : 1;
			const auto& row = _model->comparison().rows.at(static_cast<size_t>(index.row()));
			const int line = side == 0 ? row.left : row.right;
			if (line < 0 || !_bufferIds[side])
				return;
			const LRESULT position = send(NPPM_GETPOSFROMBUFFERID, _bufferIds[side]);
			if (position < 0)
				return;
			const int view = static_cast<int>(position >> 30);
			const LRESULT tab = position & ((1L << 30) - 1);
			send(NPPM_ACTIVATEDOC, view, tab);
			const HWND editor = view == 0 ? host._scintillaMainHandle : host._scintillaSecondHandle;
			SendMessage(editor, SCI_ENSUREVISIBLE, line, 0);
			SendMessage(editor, SCI_GOTOLINE, line, 0);
		}

		QComboBox* _sources[2]{};
		QPushButton* _browse[2]{};
		QPushButton* _compare = nullptr;
		QPushButton* _previous = nullptr;
		QPushButton* _next = nullptr;
		QLabel* _status = nullptr;
		QTableView* _table = nullptr;
		DiffChecker::DiffModel* _model = nullptr;
		QStringList _files;
		WPARAM _bufferIds[2]{};
		std::shared_ptr<std::atomic_bool> _cancel;
		QFutureWatcher<DiffChecker::Comparison> _watcher;
	};

	QPointer<CompareDialog> dialog;

	void showDialog()
	{
		if (!dialog)
			dialog = new CompareDialog(nppWidget(host._nppHandle));
		dialog->show();
		dialog->raise();
		dialog->activateWindow();
	}

	FuncItem commands[] = {{L"Compare…", showDialog}};
}

extern "C" __declspec(dllexport) void setInfo(NppData data) { host = data; }
extern "C" __declspec(dllexport) const wchar_t* getName() { return L"Diff Checker"; }
extern "C" __declspec(dllexport) BOOL isUnicode() { return TRUE; }
extern "C" __declspec(dllexport) FuncItem* getFuncsArray(int* count)
{
	*count = static_cast<int>(std::size(commands));
	return commands;
}

extern "C" __declspec(dllexport) void beNotified(SCNotification* notification)
{
	if (!dialog)
		return;
	switch (notification->nmhdr.code)
	{
		case NPPN_BUFFERACTIVATED:
		case NPPN_FILECLOSED:
		case NPPN_FILESAVED:
			dialog->refreshSources();
			break;
		case NPPN_SHUTDOWN:
			dialog->close();
			break;
		default:
			break;
	}
}

extern "C" __declspec(dllexport) LRESULT messageProc(UINT, WPARAM, LPARAM) { return TRUE; }
