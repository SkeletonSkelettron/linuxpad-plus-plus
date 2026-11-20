// SPDX-License-Identifier: GPL-3.0-or-later

#include "DiffModel.h"

#include <algorithm>

#include <QBrush>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QStringDecoder>

#include "engine/diff.h"

namespace DiffChecker
{
	namespace
	{
		constexpr qsizetype maxLines = 100000;
		struct TooComplex {};

		bool readLines(const Input& input, QStringList& lines, QString& error)
		{
			QByteArray bytes = input.utf8;
			if (!input.path.isEmpty())
			{
				QFile file(input.path);
				if (!QFileInfo(input.path).isFile() || !file.open(QIODevice::ReadOnly))
				{
					error = QStringLiteral("Could not open %1.").arg(input.path);
					return false;
				}
				bytes = file.read(maxBytes + 1);
				if (file.error() != QFileDevice::NoError)
				{
					error = file.errorString();
					return false;
				}
			}
			if (bytes.size() > maxBytes)
			{
				error = QStringLiteral("Each input can be up to 8 MiB.");
				return false;
			}
			const auto encoding = QStringConverter::encodingForData(bytes).value_or(QStringConverter::Utf8);
			QStringDecoder decoder(encoding, QStringConverter::Flag::Stateless);
			QString text = decoder(bytes);
			if (decoder.hasError() || text.contains(QChar::Null))
			{
				error = QStringLiteral("Select a Unicode text file, or open it in the editor with its encoding first.");
				return false;
			}
			text.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
			text.replace(QLatin1Char('\r'), QLatin1Char('\n'));
			if (text.count(QLatin1Char('\n')) >= maxLines)
			{
				error = QStringLiteral("Each input can have up to 100,000 lines.");
				return false;
			}
			if (!text.isEmpty())
				lines = text.split(QLatin1Char('\n'));
			return true;
		}
	}

	Comparison compare(const Input& left, const Input& right, const std::atomic_bool& cancelled)
	{
		Comparison result;
		if (!readLines(left, result.left, result.error) || !readLines(right, result.right, result.error))
			return result;

		using LineKey = hash_type<unsigned int>;
		QHash<QStringView, unsigned int> lineKeys;
		lineKeys.reserve(result.left.size() + result.right.size());
		auto keysFor = [&](const QStringList& lines)
		{
			std::vector<LineKey> keys;
			keys.reserve(static_cast<size_t>(lines.size()));
			for (const QString& line : lines)
			{
				// QHash verifies equality before a key is reused. The diff sees no hash collisions.
				auto existing = lineKeys.constFind(QStringView(line));
				const auto key = existing == lineKeys.cend()
					? lineKeys.insert(QStringView(line), static_cast<unsigned int>(lineKeys.size())).value()
					: existing.value();
				keys.emplace_back(key);
			}
			return keys;
		};
		const auto leftKeys = keysFor(result.left);
		const auto rightKeys = keysFor(result.right);
		QElapsedTimer timer;
		timer.start();
		auto checkBudget = [&]
		{
			if (cancelled.load(std::memory_order_relaxed) || timer.hasExpired(2500))
				throw TooComplex{};
		};
		diff_results differences;
		try {
			checkBudget();
			differences = DiffCalc<LineKey>(leftKeys, rightKeys, checkBudget)(DiffAlg::MIXED);
		}
		catch (const TooComplex&)
		{
			result.error = QStringLiteral("The comparison exceeded its time limit. Try smaller inputs.");
			return result;
		}

		int a = 0;
		int b = 0;
		result.rows.reserve(std::max(leftKeys.size(), rightKeys.size()));
		for (const diff_info& change : differences)
		{
			while (a < change.a.s && b < change.b.s)
				result.rows.push_back({a++, b++, false});
			result.changeRows.push_back(static_cast<int>(result.rows.size()));
			result.removed += static_cast<int>(change.a.len());
			result.added += static_cast<int>(change.b.len());
			while (a < change.a.e || b < change.b.e)
				result.rows.push_back({a < change.a.e ? a++ : -1, b < change.b.e ? b++ : -1, true});
		}
		while (a < result.left.size() && b < result.right.size())
			result.rows.push_back({a++, b++, false});
		return result;
	}

	int DiffModel::rowCount(const QModelIndex& parent) const
	{
		return parent.isValid() ? 0 : static_cast<int>(_result.rows.size());
	}

	int DiffModel::columnCount(const QModelIndex& parent) const
	{
		return parent.isValid() ? 0 : 4;
	}

	QVariant DiffModel::data(const QModelIndex& index, int role) const
	{
		if (!index.isValid())
			return {};
		const Row& row = _result.rows[static_cast<size_t>(index.row())];
		const bool onLeft = index.column() < 2;
		const bool lineNumber = index.column() % 2 == 0;
		const int line = onLeft ? row.left : row.right;
		if (role == Qt::TextAlignmentRole && lineNumber)
			return static_cast<int>(Qt::AlignRight | Qt::AlignVCenter);
		if (role == Qt::BackgroundRole && row.changed && line >= 0)
			return QBrush(onLeft ? QColor(220, 64, 58, 52) : QColor(44, 160, 90, 58));
		if (line < 0)
			return {};
		const QString& text = (onLeft ? _result.left : _result.right).at(line);
		if (role == Qt::ToolTipRole && !lineNumber)
			return QStringLiteral("<pre>%1</pre>").arg(text.toHtmlEscaped());
		if (role == Qt::DisplayRole)
			return lineNumber ? QVariant(line + 1) : QVariant(text);
		return {};
	}

	QVariant DiffModel::headerData(int section, Qt::Orientation orientation, int role) const
	{
		if (orientation != Qt::Horizontal || role != Qt::DisplayRole)
			return {};
		if (section == 1)
			return QStringLiteral("Left");
		if (section == 3)
			return QStringLiteral("Right");
		return QStringLiteral("Line");
	}

	void DiffModel::setComparison(Comparison result)
	{
		beginResetModel();
		_result = std::move(result);
		endResetModel();
	}
}
