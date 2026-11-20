// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <atomic>
#include <vector>

#include <QAbstractTableModel>
#include <QByteArray>
#include <QStringList>

namespace DiffChecker
{
	inline constexpr qsizetype maxBytes = 8 * 1024 * 1024;

	struct Input
	{
		QString path;
		QByteArray utf8;
	};

	struct Row
	{
		int left = -1;
		int right = -1;
		bool changed = false;
	};

	struct Comparison
	{
		QString error;
		QStringList left;
		QStringList right;
		std::vector<Row> rows;
		std::vector<int> changeRows;
		int removed = 0;
		int added = 0;
	};

	Comparison compare(const Input& left, const Input& right, const std::atomic_bool& cancelled);

	class DiffModel final : public QAbstractTableModel
	{
	public:
		using QAbstractTableModel::QAbstractTableModel;
		int rowCount(const QModelIndex& parent = {}) const override;
		int columnCount(const QModelIndex& parent = {}) const override;
		QVariant data(const QModelIndex& index, int role) const override;
		QVariant headerData(int section, Qt::Orientation orientation, int role) const override;
		void setComparison(Comparison result);
		const Comparison& comparison() const { return _result; }

	private:
		Comparison _result;
	};
}
