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

#include "UdlDialog.h"

#include <algorithm>

#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QTabWidget>
#include <QTimer>
#include <QVBoxLayout>

#include "MainWindow.h"
#include "Udl.h"

namespace
{
	// convertTo(): the words of a field, each with the prefix of its kind ("00" ... "23");
	// a group in (( )) keeps its spaces
	void convertTo(std::string& dest, const std::string& toConvert, const char* prefix)
	{
		bool inGroup = false;
		if (!dest.empty())
			dest += ' ';
		dest += prefix[0];
		dest += prefix[1];
		const size_t len = toConvert.size();
		auto at = [&](size_t k) { return k < len ? toConvert[k] : '\0'; };
		for (size_t i = 0; i < len; ++i)
		{
			if (i == 0 && at(i) == '(' && at(i + 1) == '(')
			{
				inGroup = true;
			}
			else if (at(i) == ' ' && at(i + 1) == '(' && at(i + 2) == '(')
			{
				inGroup = true;
				dest += ' ';
				dest += prefix[0];
				dest += prefix[1];
				++i; // skip space
			}
			if (inGroup && i >= 2 && at(i - 1) == ')' && at(i - 2) == ')')
				inGroup = false;
			if (at(i) == ' ')
			{
				if (at(i + 1) != ' ' && at(i + 1) != '\0')
				{
					dest += ' ';
					if (!inGroup)
					{
						dest += prefix[0];
						dest += prefix[1];
					}
				}
			}
			else
			{
				dest += at(i);
			}
		}
	}

	// CommentStyleDialog::retrieve() / SymbolsStyleDialog::retrieve(): the words of one prefix
	std::string retrieveWords(const std::string& list, const char* prefix)
	{
		std::string dest;
		bool begin2Copy = false;
		bool inGroup = false;
		const size_t len = list.size();
		auto at = [&](size_t k) { return k < len ? list[k] : '\0'; };
		for (size_t i = 0; i < len; ++i)
		{
			if ((i == 0 || at(i - 1) == ' ') && at(i) == prefix[0] && at(i + 1) == prefix[1])
			{
				if (!dest.empty())
					dest += ' ';
				begin2Copy = true;
				++i;
				continue;
			}
			if (at(i) == '(' && at(i + 1) == '(' && !inGroup && begin2Copy)
				inGroup = true;
			if (at(i) != ')' && i >= 2 && at(i - 1) == ')' && at(i - 2) == ')' && inGroup)
				inGroup = false;
			if (at(i) == ' ' && begin2Copy)
				begin2Copy = false;
			if (begin2Copy || inGroup)
				dest += at(i);
		}
		return dest;
	}

	std::string prefixOf(int i)
	{
		return QStringLiteral("%1").arg(i, 2, 10, QLatin1Char('0')).toStdString();
	}

	QString str(const std::string& s)
	{
		return QString::fromUtf8(s);
	}

	std::string std8(const QString& s)
	{
		return s.toUtf8().toStdString();
	}

	const char* const kCommentFields[] = {"commentLineOpen", "commentLineContinue", "commentLineClose", "commentOpen", "commentClose"};
	struct NumberField
	{
		const char* name;
		const char* label;
		int list;
	};
	const NumberField kNumberFields[] = {
		{"numberPrefix1", "Prefix 1", SCE_USER_KWLIST_NUMBER_PREFIX1}, {"numberPrefix2", "Prefix 2", SCE_USER_KWLIST_NUMBER_PREFIX2},
		{"numberExtras1", "Extras 1", SCE_USER_KWLIST_NUMBER_EXTRAS1}, {"numberExtras2", "Extras 2", SCE_USER_KWLIST_NUMBER_EXTRAS2},
		{"numberSuffix1", "Suffix 1", SCE_USER_KWLIST_NUMBER_SUFFIX1}, {"numberSuffix2", "Suffix 2", SCE_USER_KWLIST_NUMBER_SUFFIX2},
		{"numberRange", "Range", SCE_USER_KWLIST_NUMBER_RANGE}};
	const int kFolderLists[9] = {SCE_USER_KWLIST_FOLDERS_IN_CODE1_OPEN, SCE_USER_KWLIST_FOLDERS_IN_CODE1_MIDDLE, SCE_USER_KWLIST_FOLDERS_IN_CODE1_CLOSE,
	                             SCE_USER_KWLIST_FOLDERS_IN_CODE2_OPEN, SCE_USER_KWLIST_FOLDERS_IN_CODE2_MIDDLE, SCE_USER_KWLIST_FOLDERS_IN_CODE2_CLOSE,
	                             SCE_USER_KWLIST_FOLDERS_IN_COMMENT_OPEN, SCE_USER_KWLIST_FOLDERS_IN_COMMENT_MIDDLE, SCE_USER_KWLIST_FOLDERS_IN_COMMENT_CLOSE};
}

UdlDialog::UdlDialog(MainWindow* mw)
	: QDialog(mw), _mw(mw)
{
	setObjectName(QStringLiteral("udlDialog"));
	setWindowTitle(QStringLiteral("User Defined Language v2.1"));
	_applyTimer = new QTimer(this);
	_applyTimer->setSingleShot(true);
	_applyTimer->setInterval(250);
	connect(_applyTimer, &QTimer::timeout, this, &UdlDialog::apply);
	buildUi();
	resize(820, 720);
}

void UdlDialog::buildUi()
{
	auto* root = new QVBoxLayout(this);

	// language list and its buttons
	auto* top = new QGridLayout;
	top->addWidget(new QLabel(QStringLiteral("User language:"), this), 0, 0);
	_langCombo = new QComboBox(this);
	_langCombo->setObjectName(QStringLiteral("udlLangCombo"));
	_langCombo->setMinimumWidth(240);
	top->addWidget(_langCombo, 0, 1);
	auto* createBtn = new QPushButton(QStringLiteral("Create new..."), this);
	auto* saveAsBtn = new QPushButton(QStringLiteral("Save as..."), this);
	auto* renameBtn = new QPushButton(QStringLiteral("Rename"), this);
	auto* removeBtn = new QPushButton(QStringLiteral("Remove"), this);
	auto* importBtn = new QPushButton(QStringLiteral("Import..."), this);
	auto* exportBtn = new QPushButton(QStringLiteral("Export..."), this);
	top->addWidget(createBtn, 0, 2);
	top->addWidget(saveAsBtn, 0, 3);
	top->addWidget(renameBtn, 0, 4);
	top->addWidget(removeBtn, 0, 5);
	top->addWidget(importBtn, 1, 4);
	top->addWidget(exportBtn, 1, 5);
	top->addWidget(new QLabel(QStringLiteral("Ext.:"), this), 1, 0);
	_ext = new QLineEdit(this);
	_ext->setObjectName(QStringLiteral("udlExt"));
	top->addWidget(_ext, 1, 1);
	_ignoreCase = new QCheckBox(QStringLiteral("Ignore case"), this);
	top->addWidget(_ignoreCase, 1, 2, 1, 2);
	root->addLayout(top);

	_tabs = new QTabWidget(this);
	root->addWidget(_tabs, 1);
	auto addTab = [this](const QString& title)
	{
		auto* scroll = new QScrollArea(_tabs);
		scroll->setWidgetResizable(true);
		scroll->setFrameShape(QFrame::NoFrame);
		auto* page = new QWidget(scroll);
		scroll->setWidget(page);
		_tabs->addTab(scroll, title);
		return page;
	};
	auto styler = [this](QWidget* parent, int styleId, bool nesting)
	{
		auto* b = new QPushButton(QStringLiteral("Styler"), parent);
		b->setObjectName(QStringLiteral("styler%1").arg(styleId));
		connect(b, &QPushButton::clicked, this, [this, styleId, nesting]
		{
			if (_lang && editStyle(this, _lang, styleId, nesting))
				changed();
		});
		return b;
	};
	auto smallEdit = [this](QWidget* parent, int lines)
	{
		auto* e = new QPlainTextEdit(parent);
		e->setFixedHeight(e->fontMetrics().lineSpacing() * lines + 12);
		connect(e, &QPlainTextEdit::textChanged, this, &UdlDialog::changed);
		return e;
	};
	auto lineEdit = [this](QWidget* parent, const QString& name)
	{
		auto* e = new QLineEdit(parent);
		e->setObjectName(name);
		_fields.insert(name, e);
		connect(e, &QLineEdit::textChanged, this, &UdlDialog::changed);
		return e;
	};

	// Folder & Default (FolderStyleDialog)
	{
		QWidget* page = addTab(QStringLiteral("Folder && Default"));
		auto* l = new QVBoxLayout(page);
		auto* defBox = new QGroupBox(QStringLiteral("Default style"), page);
		auto* defLayout = new QHBoxLayout(defBox);
		defLayout->addWidget(styler(defBox, SCE_USER_STYLE_DEFAULT, false));
		defLayout->addStretch();
		_foldCompact = new QCheckBox(QStringLiteral("Fold compact (fold empty lines too)"), defBox);
		defLayout->addWidget(_foldCompact);
		l->addWidget(defBox);
		const char* titles[] = {"Folding in code 1 style:", "Folding in code 2 style (separators needed):", "Folding in comment style:"};
		const int styles[] = {SCE_USER_STYLE_FOLDER_IN_CODE1, SCE_USER_STYLE_FOLDER_IN_CODE2, SCE_USER_STYLE_FOLDER_IN_COMMENT};
		for (int g = 0; g < 3; ++g)
		{
			auto* box = new QGroupBox(QString::fromLatin1(titles[g]), page);
			auto* grid = new QGridLayout(box);
			grid->addWidget(styler(box, styles[g], false), 0, 3, Qt::AlignTop);
			const char* labels[] = {"Open:", "Middle:", "Close:"};
			for (int k = 0; k < 3; ++k)
			{
				grid->addWidget(new QLabel(QString::fromLatin1(labels[k]), box), 0, k * 1, Qt::AlignLeft);
				_folders[g * 3 + k] = smallEdit(box, 3);
				grid->addWidget(_folders[g * 3 + k], 1, k);
			}
			l->addWidget(box);
		}
		l->addStretch();
	}

	// Keywords Lists (KeyWordsStyleDialog)
	{
		QWidget* page = addTab(QStringLiteral("Keywords Lists"));
		auto* grid = new QGridLayout(page);
		const char* ordinals[] = {"1st", "2nd", "3rd", "4th", "5th", "6th", "7th", "8th"};
		for (int g = 0; g < 8; ++g)
		{
			auto* box = new QGroupBox(QStringLiteral("%1 group").arg(QString::fromLatin1(ordinals[g])), page);
			auto* bl = new QVBoxLayout(box);
			auto* row = new QHBoxLayout;
			row->addWidget(styler(box, SCE_USER_STYLE_KEYWORD1 + g, false));
			_prefix[g] = new QCheckBox(QStringLiteral("Prefix mode"), box);
			row->addWidget(_prefix[g]);
			row->addStretch();
			bl->addLayout(row);
			_keywords[g] = smallEdit(box, 4);
			_keywords[g]->setObjectName(QStringLiteral("keywords%1").arg(g + 1));
			bl->addWidget(_keywords[g]);
			grid->addWidget(box, g / 2, g % 2);
		}
	}

	// Comment & Number (CommentStyleDialog)
	{
		QWidget* page = addTab(QStringLiteral("Comment && Number"));
		auto* l = new QGridLayout(page);
		auto* posBox = new QGroupBox(QStringLiteral("Line comment position"), page);
		auto* posLayout = new QVBoxLayout(posBox);
		const char* pos[] = {"Allow anywhere", "Force at beginning of line", "Allow preceding whitespace"};
		for (int i = 0; i < 3; ++i)
		{
			_lcPos[i] = new QRadioButton(QString::fromLatin1(pos[i]), posBox);
			posLayout->addWidget(_lcPos[i]);
		}
		_foldComments = new QCheckBox(QStringLiteral("Allow folding of comments"), page);
		auto* lineBox = new QGroupBox(QStringLiteral("Comment line style"), page);
		auto* lineForm = new QFormLayout(lineBox);
		lineForm->addRow(styler(lineBox, SCE_USER_STYLE_COMMENTLINE, true));
		lineForm->addRow(QStringLiteral("Open:"), lineEdit(lineBox, QString::fromLatin1(kCommentFields[0])));
		lineForm->addRow(QStringLiteral("Continue character:"), lineEdit(lineBox, QString::fromLatin1(kCommentFields[1])));
		lineForm->addRow(QStringLiteral("Close:"), lineEdit(lineBox, QString::fromLatin1(kCommentFields[2])));
		auto* commentBox = new QGroupBox(QStringLiteral("Comment style"), page);
		auto* commentForm = new QFormLayout(commentBox);
		commentForm->addRow(styler(commentBox, SCE_USER_STYLE_COMMENT, true));
		commentForm->addRow(QStringLiteral("Open:"), lineEdit(commentBox, QString::fromLatin1(kCommentFields[3])));
		commentForm->addRow(QStringLiteral("Close:"), lineEdit(commentBox, QString::fromLatin1(kCommentFields[4])));
		auto* numberBox = new QGroupBox(QStringLiteral("Number style"), page);
		auto* numberForm = new QFormLayout(numberBox);
		numberForm->addRow(styler(numberBox, SCE_USER_STYLE_NUMBER, false));
		for (const NumberField& f : kNumberFields)
			numberForm->addRow(QString::fromLatin1(f.label) + QLatin1Char(':'), lineEdit(numberBox, QString::fromLatin1(f.name)));
		auto* sepRow = new QHBoxLayout;
		const char* seps[] = {"Dot", "Comma", "Both"};
		for (int i = 0; i < 3; ++i)
		{
			_decSep[i] = new QRadioButton(QString::fromLatin1(seps[i]), numberBox);
			sepRow->addWidget(_decSep[i]);
		}
		numberForm->addRow(QStringLiteral("Decimal separator:"), sepRow);
		l->addWidget(posBox, 0, 0);
		l->addWidget(_foldComments, 1, 0);
		l->addWidget(lineBox, 2, 0);
		l->addWidget(commentBox, 3, 0);
		l->addWidget(numberBox, 0, 1, 4, 1);
		l->setRowStretch(4, 1);
	}

	// Operators & Delimiters (SymbolsStyleDialog)
	{
		QWidget* page = addTab(QStringLiteral("Operators && Delimiters"));
		auto* l = new QGridLayout(page);
		auto* opBox = new QGroupBox(QStringLiteral("Operators style"), page);
		auto* opLayout = new QGridLayout(opBox);
		opLayout->addWidget(styler(opBox, SCE_USER_STYLE_OPERATOR, false), 0, 0);
		opLayout->addWidget(new QLabel(QStringLiteral("Operators 1"), opBox), 1, 0);
		opLayout->addWidget(new QLabel(QStringLiteral("Operators 2 (separators required)"), opBox), 1, 1);
		_operators1 = smallEdit(opBox, 2);
		_operators2 = smallEdit(opBox, 2);
		opLayout->addWidget(_operators1, 2, 0);
		opLayout->addWidget(_operators2, 2, 1);
		l->addWidget(opBox, 0, 0, 1, 2);
		for (int d = 0; d < 8; ++d)
		{
			auto* box = new QGroupBox(QStringLiteral("Delimiter %1 style").arg(d + 1), page);
			auto* form = new QFormLayout(box);
			form->addRow(styler(box, SCE_USER_STYLE_DELIMITER1 + d, true));
			form->addRow(QStringLiteral("Open:"), lineEdit(box, QStringLiteral("delimiter%1Open").arg(d + 1)));
			form->addRow(QStringLiteral("Escape:"), lineEdit(box, QStringLiteral("delimiter%1Escape").arg(d + 1)));
			form->addRow(QStringLiteral("Close:"), lineEdit(box, QStringLiteral("delimiter%1Close").arg(d + 1)));
			l->addWidget(box, 1 + d / 2, d % 2);
		}
	}

	for (QCheckBox* c : {_ignoreCase, _foldCompact, _foldComments})
		connect(c, &QCheckBox::toggled, this, &UdlDialog::changed);
	for (QCheckBox* c : _prefix)
		connect(c, &QCheckBox::toggled, this, &UdlDialog::changed);
	for (QRadioButton* r : _lcPos)
		connect(r, &QRadioButton::toggled, this, &UdlDialog::changed);
	for (QRadioButton* r : _decSep)
		connect(r, &QRadioButton::toggled, this, &UdlDialog::changed);
	connect(_ext, &QLineEdit::textChanged, this, &UdlDialog::changed);
	connect(_langCombo, &QComboBox::currentIndexChanged, this, &UdlDialog::selectLang);

	connect(createBtn, &QPushButton::clicked, this, [this]
	{
		bool ok = false;
		const QString name = QInputDialog::getText(this, QStringLiteral("Create New Language..."), QStringLiteral("Name:"), QLineEdit::Normal, QString(), &ok).trimmed();
		if (ok && !name.isEmpty())
			createLang(name, false);
	});
	connect(saveAsBtn, &QPushButton::clicked, this, [this]
	{
		bool ok = false;
		const QString name = QInputDialog::getText(this, QStringLiteral("Save Current Language Name As..."), QStringLiteral("Name:"), QLineEdit::Normal, QString(), &ok).trimmed();
		if (ok && !name.isEmpty())
			createLang(name, true);
	});
	connect(renameBtn, &QPushButton::clicked, this, [this]
	{
		if (_langCombo->currentIndex() <= 0)
			return;
		bool ok = false;
		const QString name = QInputDialog::getText(this, QStringLiteral("Rename Current Language Name"), QStringLiteral("Name:"), QLineEdit::Normal, _lang->name, &ok).trimmed();
		if (ok && !name.isEmpty() && name != _lang->name)
			renameLang(name);
	});
	connect(removeBtn, &QPushButton::clicked, this, [this]
	{
		if (_langCombo->currentIndex() <= 0)
			return;
		if (QMessageBox::question(this, QStringLiteral("Remove the current language"), QStringLiteral("Are you sure?")) == QMessageBox::Yes)
			removeLang();
	});
	connect(importBtn, &QPushButton::clicked, this, [this]
	{
		const QString fn = QFileDialog::getOpenFileName(this, QStringLiteral("Import"), QString(), QStringLiteral("UDL (*.xml);;All types (*)"));
		if (fn.isEmpty())
			return;
		const int n = UdlManager::instance().importFile(fn);
		QString error;
		UdlManager::instance().saveAll(&error);
		_mw->rebuildUdlMenu();
		fillCombo(_lang ? _lang->name : QString());
		if (n > 0)
			QMessageBox::information(this, QStringLiteral("Import successful"), QStringLiteral("Import successful."));
		else
			QMessageBox::warning(this, QStringLiteral("Import"), QStringLiteral("Fail to import."));
	});
	connect(exportBtn, &QPushButton::clicked, this, [this]
	{
		if (!_lang)
			return;
		const QString fn = QFileDialog::getSaveFileName(this, QStringLiteral("Export"), _lang->name + QStringLiteral(".xml"), QStringLiteral("UDL (*.xml);;All types (*)"));
		if (fn.isEmpty())
			return;
		QString error;
		if (UdlManager::saveToFile(*_lang, fn, &error))
			QMessageBox::information(this, QStringLiteral("Export successful"), QStringLiteral("Export successful."));
		else
			QMessageBox::warning(this, QStringLiteral("Export"), error);
	});
	auto updateButtons = [this, renameBtn, removeBtn]
	{
		const bool named = _langCombo->currentIndex() > 0;
		renameBtn->setEnabled(named);
		removeBtn->setEnabled(named);
	};
	connect(_langCombo, &QComboBox::currentIndexChanged, this, updateButtons);
	auto* close = new QDialogButtonBox(QDialogButtonBox::Close, this);
	connect(close, &QDialogButtonBox::rejected, this, &QDialog::hide);
	root->addWidget(close);
}

void UdlDialog::fillCombo(const QString& select)
{
	const QSignalBlocker blocker(_langCombo);
	_langCombo->clear();
	_langCombo->addItem(QStringLiteral("User Defined Language"));
	int index = 0;
	for (const auto& l : UdlManager::instance().langs())
	{
		_langCombo->addItem(l->name);
		if (l->name == select)
			index = _langCombo->count() - 1;
	}
	_langCombo->setCurrentIndex(index);
	selectLang(index);
	emit _langCombo->currentIndexChanged(index); // the buttons
}

void UdlDialog::showFor(const QString& udlName)
{
	fillCombo(udlName);
	show();
	raise();
	activateWindow();
}

void UdlDialog::selectLang(int index)
{
	UdlManager& um = UdlManager::instance();
	_lang = (index <= 0) ? &um.scratch() : um.find(_langCombo->itemText(index));
	if (!_lang)
		_lang = &um.scratch();
	updateDlg();
}

void UdlDialog::updateDlg()
{
	if (!_lang)
		return;
	_updating = true;
	const UserLang& ul = *_lang;
	_ext->setText(ul.ext);
	_ignoreCase->setChecked(ul.caseIgnored);
	_foldCompact->setChecked(ul.foldCompact);
	_foldComments->setChecked(ul.allowFoldOfComments);
	for (int i = 0; i < 9; ++i)
		_folders[i]->setPlainText(str(ul.keywordLists[kFolderLists[i]]));
	for (int g = 0; g < 8; ++g)
	{
		_keywords[g]->setPlainText(str(ul.keywordLists[SCE_USER_KWLIST_KEYWORDS1 + g]));
		_prefix[g]->setChecked(ul.isPrefix[g]);
	}
	_lcPos[std::clamp(ul.forcePureLC, 0, 2)]->setChecked(true);
	_decSep[std::clamp(ul.decimalSeparator, 0, 2)]->setChecked(true);
	for (int i = 0; i < 5; ++i)
		_fields.value(QString::fromLatin1(kCommentFields[i]))->setText(str(retrieveWords(ul.keywordLists[SCE_USER_KWLIST_COMMENTS], prefixOf(i).c_str())));
	for (const NumberField& f : kNumberFields)
		_fields.value(QString::fromLatin1(f.name))->setText(str(ul.keywordLists[f.list]));
	_operators1->setPlainText(str(ul.keywordLists[SCE_USER_KWLIST_OPERATORS1]));
	_operators2->setPlainText(str(ul.keywordLists[SCE_USER_KWLIST_OPERATORS2]));
	const char* parts[] = {"Open", "Escape", "Close"};
	for (int d = 0; d < 8; ++d)
	{
		for (int k = 0; k < 3; ++k)
		{
			const std::string words = retrieveWords(ul.keywordLists[SCE_USER_KWLIST_DELIMITERS], prefixOf(d * 3 + k).c_str());
			_fields.value(QStringLiteral("delimiter%1%2").arg(d + 1).arg(QString::fromLatin1(parts[k])))->setText(str(words));
		}
	}
	_updating = false;
}

void UdlDialog::retrieve()
{
	if (!_lang)
		return;
	UserLang& ul = *_lang;
	ul.ext = _ext->text().trimmed();
	ul.caseIgnored = _ignoreCase->isChecked();
	ul.foldCompact = _foldCompact->isChecked();
	ul.allowFoldOfComments = _foldComments->isChecked();
	for (int i = 0; i < 9; ++i)
		ul.keywordLists[kFolderLists[i]] = std8(_folders[i]->toPlainText());
	for (int g = 0; g < 8; ++g)
	{
		ul.keywordLists[SCE_USER_KWLIST_KEYWORDS1 + g] = std8(_keywords[g]->toPlainText());
		ul.isPrefix[g] = _prefix[g]->isChecked();
	}
	for (int i = 0; i < 3; ++i)
	{
		if (_lcPos[i]->isChecked())
			ul.forcePureLC = i;
		if (_decSep[i]->isChecked())
			ul.decimalSeparator = i;
	}
	std::string comments;
	for (int i = 0; i < 5; ++i)
		convertTo(comments, std8(_fields.value(QString::fromLatin1(kCommentFields[i]))->text()), prefixOf(i).c_str());
	ul.keywordLists[SCE_USER_KWLIST_COMMENTS] = comments;
	for (const NumberField& f : kNumberFields)
		ul.keywordLists[f.list] = std8(_fields.value(QString::fromLatin1(f.name))->text());
	ul.keywordLists[SCE_USER_KWLIST_OPERATORS1] = std8(_operators1->toPlainText());
	ul.keywordLists[SCE_USER_KWLIST_OPERATORS2] = std8(_operators2->toPlainText());
	std::string delimiters;
	const char* parts[] = {"Open", "Escape", "Close"};
	for (int d = 0; d < 8; ++d)
	{
		for (int k = 0; k < 3; ++k)
			convertTo(delimiters, std8(_fields.value(QStringLiteral("delimiter%1%2").arg(d + 1).arg(QString::fromLatin1(parts[k])))->text()), prefixOf(d * 3 + k).c_str());
	}
	ul.keywordLists[SCE_USER_KWLIST_DELIMITERS] = delimiters;
}

void UdlDialog::changed()
{
	if (_updating || !_lang)
		return;
	retrieve();
	if (_lang != &UdlManager::instance().scratch())
		_lang->dirty = true;
	_applyTimer->start();
}

void UdlDialog::apply()
{
	// SharedParametersDialog: the documents in this language follow the changes
	if (_lang)
		_mw->udlChanged(_lang == &UdlManager::instance().scratch() ? QString() : _lang->name);
}

bool UdlDialog::createLang(const QString& name, bool copyCurrent)
{
	// IDC_ADD_BUTTON / IDC_SAVEAS_BUTTON
	UdlManager& um = UdlManager::instance();
	if (um.find(name) || name == QStringLiteral("User Defined Language"))
	{
		QMessageBox::warning(this, QStringLiteral("UDL Error"), QStringLiteral("This name is used by another language,\nplease give another one."));
		return false;
	}
	auto ul = std::make_unique<UserLang>();
	if (copyCurrent && _lang)
		*ul = *_lang;
	ul->name = name;
	ul->file.clear();
	ul->dirty = true;
	um.addLang(std::move(ul));
	QString error;
	um.saveAll(&error);
	_mw->rebuildUdlMenu();
	fillCombo(name);
	return true;
}

bool UdlDialog::renameLang(const QString& name)
{
	// IDC_RENAME_BUTTON
	UdlManager& um = UdlManager::instance();
	if (!_lang || _lang == &um.scratch())
		return false;
	if (um.find(name))
	{
		QMessageBox::warning(this, QStringLiteral("UDL Error"), QStringLiteral("This name is used by another language,\nplease give another one."));
		return false;
	}
	const QString old = _lang->name;
	_lang->name = name;
	_lang->dirty = true;
	QString error;
	um.saveAll(&error);
	_mw->udlRenamed(old, name);
	_mw->rebuildUdlMenu();
	fillCombo(name);
	return true;
}

bool UdlDialog::removeLang()
{
	// IDC_REMOVELANG_BUTTON: the documents in this language become User-Defined (WM_REMOVE_USERLANG)
	UdlManager& um = UdlManager::instance();
	if (!_lang || _lang == &um.scratch())
		return false;
	const QString name = _lang->name;
	_lang = &um.scratch();
	um.removeLang(name);
	QString error;
	um.saveAll(&error);
	_mw->udlRemoved(name);
	_mw->rebuildUdlMenu();
	fillCombo(QString());
	return true;
}

void UdlDialog::closeEvent(QCloseEvent* event)
{
	QDialog::closeEvent(event);
}

void UdlDialog::hideEvent(QHideEvent* event)
{
	// the changed languages are written when the dialog closes (and when Notepad++ exits)
	if (_applyTimer->isActive())
	{
		_applyTimer->stop();
		apply();
	}
	QString error;
	if (!UdlManager::instance().saveAll(&error))
		QMessageBox::warning(this, windowTitle(), error);
	QDialog::hideEvent(event);
}

bool UdlDialog::editStyle(QWidget* parent, UserLang* lang, int styleId, bool nesting)
{
	// StylerDlg: font, colours (or transparent) and the nesting of other styles
	Style& st = lang->styles[styleId];
	QDialog dlg(parent);
	dlg.setObjectName(QStringLiteral("udlStylerDlg"));
	dlg.setWindowTitle(QStringLiteral("Styler Dialog - %1").arg(QString::fromUtf8(UdlManager::styleName(styleId))));
	auto* root = new QVBoxLayout(&dlg);

	auto* fontBox = new QGroupBox(QStringLiteral("Font options"), &dlg);
	auto* fontForm = new QFormLayout(fontBox);
	auto* name = new QComboBox(fontBox);
	name->addItem(QString());
	name->addItems(QFontDatabase::families());
	name->setCurrentIndex(std::max(0, name->findText(st.fontName)));
	auto* size = new QComboBox(fontBox);
	size->addItem(QString());
	for (int s : {5, 6, 7, 8, 9, 10, 11, 12, 14, 16, 18, 20, 22, 24, 26, 28})
		size->addItem(QString::number(s));
	size->setCurrentIndex(st.fontSize > 0 ? std::max(0, size->findText(QString::number(st.fontSize))) : 0);
	auto* bold = new QCheckBox(QStringLiteral("Bold"), fontBox);
	auto* italic = new QCheckBox(QStringLiteral("Italic"), fontBox);
	auto* underline = new QCheckBox(QStringLiteral("Underline"), fontBox);
	const int fs = st.fontStyle == STYLE_NOT_USED ? 0 : st.fontStyle;
	bold->setChecked(fs & FONTSTYLE_BOLD);
	italic->setChecked(fs & FONTSTYLE_ITALIC);
	underline->setChecked(fs & FONTSTYLE_UNDERLINE);
	fontForm->addRow(QStringLiteral("Name:"), name);
	fontForm->addRow(QStringLiteral("Size:"), size);
	auto* styleRow = new QHBoxLayout;
	styleRow->addWidget(bold);
	styleRow->addWidget(italic);
	styleRow->addWidget(underline);
	fontForm->addRow(styleRow);
	root->addWidget(fontBox);

	auto* colorBox = new QGroupBox(QStringLiteral("Colour options"), &dlg);
	auto* colorGrid = new QGridLayout(colorBox);
	QColor fg = toQColor(st.fgColor == static_cast<COLORREF>(STYLE_NOT_USED) ? rgb(0, 0, 0) : st.fgColor);
	QColor bg = toQColor(st.bgColor == static_cast<COLORREF>(STYLE_NOT_USED) ? rgb(0xFF, 0xFF, 0xFF) : st.bgColor);
	auto* fgBtn = new QPushButton(colorBox);
	auto* bgBtn = new QPushButton(colorBox);
	auto paint = [](QPushButton* b, const QColor& c) { b->setStyleSheet(QStringLiteral("background-color: %1; min-width: 48px;").arg(c.name())); };
	paint(fgBtn, fg);
	paint(bgBtn, bg);
	auto* fgTransparent = new QCheckBox(QStringLiteral("Transparent"), colorBox);
	auto* bgTransparent = new QCheckBox(QStringLiteral("Transparent"), colorBox);
	fgTransparent->setChecked(!(st.colorStyle & COLORSTYLE_FOREGROUND));
	bgTransparent->setChecked(!(st.colorStyle & COLORSTYLE_BACKGROUND));
	colorGrid->addWidget(new QLabel(QStringLiteral("Foreground colour"), colorBox), 0, 0);
	colorGrid->addWidget(fgBtn, 0, 1);
	colorGrid->addWidget(fgTransparent, 0, 2);
	colorGrid->addWidget(new QLabel(QStringLiteral("Background colour"), colorBox), 1, 0);
	colorGrid->addWidget(bgBtn, 1, 1);
	colorGrid->addWidget(bgTransparent, 1, 2);
	QObject::connect(fgBtn, &QPushButton::clicked, &dlg, [&]
	{
		const QColor c = QColorDialog::getColor(fg, &dlg);
		if (c.isValid())
			paint(fgBtn, fg = c);
	});
	QObject::connect(bgBtn, &QPushButton::clicked, &dlg, [&]
	{
		const QColor c = QColorDialog::getColor(bg, &dlg);
		if (c.isValid())
			paint(bgBtn, bg = c);
	});
	root->addWidget(colorBox);

	struct NestItem
	{
		const char* label;
		int mask;
	};
	static const NestItem nestItems[] = {
		{"Delimiter 1", SCE_USER_MASK_NESTING_DELIMITER1}, {"Delimiter 2", SCE_USER_MASK_NESTING_DELIMITER2},
		{"Delimiter 3", SCE_USER_MASK_NESTING_DELIMITER3}, {"Delimiter 4", SCE_USER_MASK_NESTING_DELIMITER4},
		{"Delimiter 5", SCE_USER_MASK_NESTING_DELIMITER5}, {"Delimiter 6", SCE_USER_MASK_NESTING_DELIMITER6},
		{"Delimiter 7", SCE_USER_MASK_NESTING_DELIMITER7}, {"Delimiter 8", SCE_USER_MASK_NESTING_DELIMITER8},
		{"Comment", SCE_USER_MASK_NESTING_COMMENT}, {"Comment line", SCE_USER_MASK_NESTING_COMMENT_LINE},
		{"Keyword 1", SCE_USER_MASK_NESTING_KEYWORD1}, {"Keyword 2", SCE_USER_MASK_NESTING_KEYWORD2},
		{"Keyword 3", SCE_USER_MASK_NESTING_KEYWORD3}, {"Keyword 4", SCE_USER_MASK_NESTING_KEYWORD4},
		{"Keyword 5", SCE_USER_MASK_NESTING_KEYWORD5}, {"Keyword 6", SCE_USER_MASK_NESTING_KEYWORD6},
		{"Keyword 7", SCE_USER_MASK_NESTING_KEYWORD7}, {"Keyword 8", SCE_USER_MASK_NESTING_KEYWORD8},
		{"Operators 1", SCE_USER_MASK_NESTING_OPERATORS1}, {"Operators 2", SCE_USER_MASK_NESTING_OPERATORS2},
		{"Numbers", SCE_USER_MASK_NESTING_NUMBERS}};
	std::vector<std::pair<QCheckBox*, int>> nestChecks;
	if (nesting)
	{
		auto* nestBox = new QGroupBox(QStringLiteral("Nesting"), &dlg);
		auto* nestGrid = new QGridLayout(nestBox);
		int i = 0;
		for (const NestItem& n : nestItems)
		{
			auto* c = new QCheckBox(QString::fromLatin1(n.label), nestBox);
			c->setChecked(lang->nesting[styleId] & n.mask);
			nestGrid->addWidget(c, i % 7, i / 7);
			nestChecks.emplace_back(c, n.mask);
			++i;
		}
		root->addWidget(nestBox);
	}
	auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
	QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
	QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
	root->addWidget(buttons);
	if (dlg.exec() != QDialog::Accepted)
		return false;

	st.fontName = name->currentText();
	st.fontSize = size->currentText().isEmpty() ? STYLE_NOT_USED : size->currentText().toInt();
	st.fontStyle = (bold->isChecked() ? FONTSTYLE_BOLD : 0) | (italic->isChecked() ? FONTSTYLE_ITALIC : 0) | (underline->isChecked() ? FONTSTYLE_UNDERLINE : 0);
	st.fgColor = fromQColor(fg);
	st.bgColor = fromQColor(bg);
	st.colorStyle = (fgTransparent->isChecked() ? 0 : COLORSTYLE_FOREGROUND) | (bgTransparent->isChecked() ? 0 : COLORSTYLE_BACKGROUND);
	if (nesting)
	{
		int mask = 0;
		for (const auto& [c, m] : nestChecks)
			mask |= c->isChecked() ? m : 0;
		lang->nesting[styleId] = mask;
	}
	return true;
}
