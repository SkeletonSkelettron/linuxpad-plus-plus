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

#include "Dialogs.h"

#include <algorithm>

#include <QAction>
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QClipboard>
#include <QColorDialog>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QIntValidator>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPainter>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QRadioButton>
#include <QSettings>
#include <QSpinBox>
#include <QStackedWidget>
#include <QTableWidget>
#include <QTabWidget>
#include <QVBoxLayout>

#include "EditView.h"
#include "FileIO.h"
#include "LangStyles.h"
#include "Localization.h"
#include "MainWindow.h"
#include "Macros.h"

namespace
{
	QSettings& dlgSettings()
	{
		static QSettings s(Npp::configDir() + QStringLiteral("/config.ini"), QSettings::IniFormat);
		return s;
	}

	QIcon colorIcon(COLORREF c, bool valid)
	{
		QPixmap pm(28, 14);
		pm.fill(valid ? toQColor(c) : QColor(Qt::transparent));
		QPainter p(&pm);
		p.setPen(Qt::gray);
		p.drawRect(0, 0, pm.width() - 1, pm.height() - 1);
		if (!valid)
			p.drawLine(0, pm.height() - 1, pm.width() - 1, 0);
		return QIcon(pm);
	}
}

namespace NppDialogs
{

// ---------------------------------------------------------------------------
// Go To
// ---------------------------------------------------------------------------
void goToLine(QWidget* parent, EditView* v)
{
	QDialog dlg(parent);
	dlg.setWindowTitle(QStringLiteral("Go To..."));
	auto* layout = new QVBoxLayout(&dlg);
	auto* modes = new QHBoxLayout;
	auto* lineMode = new QRadioButton(QStringLiteral("&Line"), &dlg);
	auto* offsetMode = new QRadioButton(QStringLiteral("&Offset"), &dlg);
	lineMode->setChecked(true);
	modes->addWidget(lineMode);
	modes->addWidget(offsetMode);
	modes->addStretch();
	layout->addLayout(modes);

	auto* form = new QFormLayout;
	auto* here = new QLabel(&dlg);
	auto* target = new QLineEdit(&dlg);
	auto* max = new QLabel(&dlg);
	form->addRow(QStringLiteral("You are here :"), here);
	form->addRow(QStringLiteral("You want to go to :"), target);
	form->addRow(QStringLiteral("You can't go further than :"), max);
	layout->addLayout(form);

	auto refresh = [&]
	{
		const sptr_t pos = v->ex(SCI_GETCURRENTPOS);
		if (lineMode->isChecked())
		{
			here->setText(QString::number(v->ex(SCI_LINEFROMPOSITION, pos) + 1));
			max->setText(QString::number(v->ex(SCI_GETLINECOUNT)));
		}
		else
		{
			here->setText(QString::number(pos));
			max->setText(QString::number(v->ex(SCI_GETLENGTH)));
		}
	};
	QObject::connect(lineMode, &QRadioButton::toggled, &dlg, refresh);
	refresh();

	auto* buttons = new QDialogButtonBox(&dlg);
	buttons->addButton(QStringLiteral("&Go"), QDialogButtonBox::AcceptRole);
	buttons->addButton(QStringLiteral("I'm going nowhere"), QDialogButtonBox::RejectRole);
	layout->addWidget(buttons);
	QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
	QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
	target->setFocus();

	if (dlg.exec() != QDialog::Accepted)
		return;
	bool ok = false;
	const qlonglong n = target->text().trimmed().toLongLong(&ok);
	if (!ok)
		return;
	if (lineMode->isChecked())
	{
		const sptr_t line = std::clamp<sptr_t>(n - 1, 0, v->ex(SCI_GETLINECOUNT) - 1);
		v->gotoLine(line);
	}
	else
	{
		const sptr_t pos = std::clamp<sptr_t>(n, 0, v->ex(SCI_GETLENGTH));
		v->ex(SCI_ENSUREVISIBLE, v->ex(SCI_LINEFROMPOSITION, pos));
		v->scrollPosToCenter(pos);
	}
	v->setFocus();
}

// ---------------------------------------------------------------------------
// Column Editor (port of ColumnEditorDlg)
// ---------------------------------------------------------------------------
void columnEditor(QWidget* parent, EditView* v)
{
	QDialog dlg(parent);
	dlg.setWindowTitle(QStringLiteral("Column / Multi-Selection Editor"));
	auto* layout = new QVBoxLayout(&dlg);

	auto* textBox = new QGroupBox(&dlg);
	auto* textLayout = new QVBoxLayout(textBox);
	auto* textMode = new QRadioButton(QStringLiteral("&Text to Insert"), textBox);
	auto* textEdit = new QLineEdit(textBox);
	textLayout->addWidget(textMode);
	textLayout->addWidget(textEdit);
	layout->addWidget(textBox);

	auto* numBox = new QGroupBox(&dlg);
	auto* numLayout = new QGridLayout(numBox);
	auto* numMode = new QRadioButton(QStringLiteral("&Number to Insert"), numBox);
	numLayout->addWidget(numMode, 0, 0, 1, 4);
	auto* initial = new QLineEdit(QStringLiteral("1"), numBox);
	auto* increase = new QLineEdit(QStringLiteral("1"), numBox);
	auto* repeat = new QLineEdit(QStringLiteral("1"), numBox);
	auto* leading = new QComboBox(numBox);
	leading->addItems({QStringLiteral("None"), QStringLiteral("Zeros"), QStringLiteral("Spaces")});
	numLayout->addWidget(new QLabel(QStringLiteral("Initial number :"), numBox), 1, 0);
	numLayout->addWidget(initial, 1, 1);
	numLayout->addWidget(new QLabel(QStringLiteral("Increase by :"), numBox), 2, 0);
	numLayout->addWidget(increase, 2, 1);
	numLayout->addWidget(new QLabel(QStringLiteral("Repeat :"), numBox), 3, 0);
	numLayout->addWidget(repeat, 3, 1);
	numLayout->addWidget(new QLabel(QStringLiteral("Leading :"), numBox), 4, 0);
	numLayout->addWidget(leading, 4, 1);
	auto* formatBox = new QGroupBox(QStringLiteral("Format"), numBox);
	auto* formatLayout = new QGridLayout(formatBox);
	auto* fmtDec = new QRadioButton(QStringLiteral("&Dec"), formatBox);
	auto* fmtHex = new QRadioButton(QStringLiteral("&Hex"), formatBox);
	auto* fmtOct = new QRadioButton(QStringLiteral("&Oct"), formatBox);
	auto* fmtBin = new QRadioButton(QStringLiteral("&Bin"), formatBox);
	fmtDec->setChecked(true);
	formatLayout->addWidget(fmtDec, 0, 0);
	formatLayout->addWidget(fmtHex, 0, 1);
	formatLayout->addWidget(fmtOct, 1, 0);
	formatLayout->addWidget(fmtBin, 1, 1);
	numLayout->addWidget(formatBox, 1, 2, 4, 2);
	layout->addWidget(numBox);

	auto* modeGroup = new QButtonGroup(&dlg);
	modeGroup->addButton(textMode);
	modeGroup->addButton(numMode);
	textMode->setChecked(true);
	auto updateEnabled = [&]
	{
		textEdit->setEnabled(textMode->isChecked());
		for (QWidget* w : {static_cast<QWidget*>(initial), static_cast<QWidget*>(increase), static_cast<QWidget*>(repeat),
		                   static_cast<QWidget*>(leading), static_cast<QWidget*>(formatBox)})
			w->setEnabled(numMode->isChecked());
	};
	QObject::connect(textMode, &QRadioButton::toggled, &dlg, updateEnabled);
	updateEnabled();

	auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
	layout->addWidget(buttons);
	QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
	QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

	if (dlg.exec() != QDialog::Accepted)
		return;

	// the lines (and insertion points) to work on
	struct Slot
	{
		sptr_t start;
		sptr_t end;
		sptr_t virtualSpace;
	};
	std::vector<Slot> targets;
	const bool columnSelection = v->ex(SCI_SELECTIONISRECTANGLE) || v->ex(SCI_GETSELECTIONS) > 1;
	if (columnSelection)
	{
		const sptr_t n = v->ex(SCI_GETSELECTIONS);
		for (sptr_t i = 0; i < n; ++i)
			targets.push_back({v->ex(SCI_GETSELECTIONNSTART, i), v->ex(SCI_GETSELECTIONNEND, i), v->ex(SCI_GETSELECTIONNSTARTVIRTUALSPACE, i)});
		std::sort(targets.begin(), targets.end(), [](const Slot& a, const Slot& b) { return a.start < b.start; });
	}
	else
	{
		const sptr_t cursorPos = v->ex(SCI_GETCURRENTPOS);
		const sptr_t cursorCol = v->ex(SCI_GETCOLUMN, cursorPos);
		const sptr_t cursorLine = v->ex(SCI_LINEFROMPOSITION, cursorPos);
		const sptr_t endLine = v->ex(SCI_GETLINECOUNT) - 1;
		for (sptr_t line = cursorLine; line <= endLine; ++line)
		{
			const sptr_t lineEnd = v->ex(SCI_GETLINEENDPOSITION, line);
			const sptr_t lineEndCol = v->ex(SCI_GETCOLUMN, lineEnd);
			if (lineEndCol < cursorCol)
				targets.push_back({lineEnd, lineEnd, cursorCol - lineEndCol});
			else
			{
				const sptr_t p = v->ex(SCI_FINDCOLUMN, line, cursorCol);
				targets.push_back({p, p, 0});
			}
		}
	}
	if (targets.empty())
		return;

	// texts to insert
	std::vector<QByteArray> texts;
	if (textMode->isChecked())
	{
		const QByteArray t = textEdit->text().toUtf8();
		texts.assign(targets.size(), t);
	}
	else
	{
		const int base = fmtHex->isChecked() ? 16 : fmtOct->isChecked() ? 8 : fmtBin->isChecked() ? 2 : 10;
		bool ok1 = false, ok2 = false, ok3 = false;
		const qlonglong init = initial->text().toLongLong(&ok1);
		const qlonglong incr = increase->text().toLongLong(&ok2);
		qlonglong rep = repeat->text().toLongLong(&ok3);
		if (!ok1 || !ok2)
			return;
		if (!ok3 || rep < 1)
			rep = 1;
		std::vector<QString> numbers;
		int width = 0;
		for (size_t i = 0; i < targets.size(); ++i)
		{
			const qlonglong n = init + incr * static_cast<qlonglong>(i / static_cast<size_t>(rep));
			QString s = QString::number(n < 0 ? -n : n, base);
			if (base == 16)
				s = s.toUpper();
			if (n < 0)
				s.prepend(QLatin1Char('-'));
			width = std::max(width, static_cast<int>(s.size()));
			numbers.push_back(s);
		}
		for (QString& s : numbers)
		{
			if (leading->currentIndex() == 1)
			{
				const bool neg = s.startsWith(QLatin1Char('-'));
				QString digits = neg ? s.mid(1) : s;
				digits = digits.rightJustified(width - (neg ? 1 : 0), QLatin1Char('0'));
				s = neg ? QStringLiteral("-") + digits : digits;
			}
			else if (leading->currentIndex() == 2)
				s = s.rightJustified(width, QLatin1Char(' '));
			else
				s = s.leftJustified(width, QLatin1Char(' '));
			texts.push_back(s.toUtf8());
		}
	}

	v->ex(SCI_BEGINUNDOACTION);
	for (int i = static_cast<int>(targets.size()) - 1; i >= 0; --i)
	{
		const Slot& s = targets[static_cast<size_t>(i)];
		const QByteArray text = QByteArray(static_cast<qsizetype>(s.virtualSpace), ' ') + texts[static_cast<size_t>(i)];
		v->replaceRange(s.start, s.end, text);
	}
	v->ex(SCI_ENDUNDOACTION);
}

// ---------------------------------------------------------------------------
// Hash generators (HashFromTextDlg / HashFromFilesDlg)
// ---------------------------------------------------------------------------
void hashGenerate(QWidget* parent, QCryptographicHash::Algorithm algo, const QString& name)
{
	QDialog dlg(parent);
	dlg.setWindowTitle(QStringLiteral("Generate %1 digest").arg(name));
	dlg.resize(560, 420);
	auto* layout = new QVBoxLayout(&dlg);
	auto* input = new QPlainTextEdit(&dlg);
	auto* perLine = new QCheckBox(QStringLiteral("Treat each line as a separate string"), &dlg);
	auto* output = new QPlainTextEdit(&dlg);
	output->setReadOnly(true);
	output->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
	layout->addWidget(input, 1);
	layout->addWidget(perLine);
	layout->addWidget(output, 1);
	auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dlg);
	QPushButton* copy = buttons->addButton(QStringLiteral("Copy to clipboard"), QDialogButtonBox::ActionRole);
	layout->addWidget(buttons);

	auto update = [&]
	{
		const QString text = input->toPlainText();
		if (text.isEmpty())
		{
			output->clear();
			return;
		}
		QStringList results;
		if (perLine->isChecked())
		{
			for (const QString& line : text.split(QLatin1Char('\n')))
				results << (line.isEmpty() ? QString() : QString::fromLatin1(QCryptographicHash::hash(line.toUtf8(), algo).toHex()));
		}
		else
			results << QString::fromLatin1(QCryptographicHash::hash(text.toUtf8(), algo).toHex());
		output->setPlainText(results.join(QLatin1Char('\n')));
	};
	QObject::connect(input, &QPlainTextEdit::textChanged, &dlg, update);
	QObject::connect(perLine, &QCheckBox::toggled, &dlg, update);
	QObject::connect(copy, &QPushButton::clicked, &dlg, [&] { QApplication::clipboard()->setText(output->toPlainText()); });
	QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
	dlg.exec();
}

void hashFromFiles(QWidget* parent, QCryptographicHash::Algorithm algo, const QString& name)
{
	const QStringList files = QFileDialog::getOpenFileNames(parent, QStringLiteral("Choose files to generate %1").arg(name), QDir::homePath());
	if (files.isEmpty())
		return;
	QStringList lines;
	for (const QString& f : files)
	{
		QFile file(f);
		if (!file.open(QIODevice::ReadOnly))
			continue;
		QCryptographicHash h(algo);
		h.addData(&file);
		lines << QStringLiteral("%1  %2").arg(QString::fromLatin1(h.result().toHex()), f);
	}
	QDialog dlg(parent);
	dlg.setWindowTitle(QStringLiteral("Generate %1 digest from files").arg(name));
	dlg.resize(720, 300);
	auto* layout = new QVBoxLayout(&dlg);
	auto* output = new QPlainTextEdit(lines.join(QLatin1Char('\n')), &dlg);
	output->setReadOnly(true);
	output->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
	layout->addWidget(output);
	auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, &dlg);
	QPushButton* copy = buttons->addButton(QStringLiteral("Copy to clipboard"), QDialogButtonBox::ActionRole);
	layout->addWidget(buttons);
	QObject::connect(copy, &QPushButton::clicked, &dlg, [&] { QApplication::clipboard()->setText(output->toPlainText()); });
	QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
	dlg.exec();
}

// ---------------------------------------------------------------------------
// Style Configurator (port of WordStyleDlg)
// ---------------------------------------------------------------------------
void styleConfigurator(MainWindow* mw)
{
	StyleManager& sm = StyleManager::instance();
	const QString originalTheme = sm.currentThemePath();

	QDialog dlg(mw);
	dlg.setWindowTitle(QStringLiteral("Style Configurator"));
	dlg.resize(820, 520);
	auto* root = new QVBoxLayout(&dlg);

	auto* themeRow = new QHBoxLayout;
	themeRow->addWidget(new QLabel(QStringLiteral("Select theme :"), &dlg));
	auto* themeCombo = new QComboBox(&dlg);
	themeCombo->addItem(QStringLiteral("Default (stylers.xml)"), QString());
	const QMap<QString, QString> themes = sm.themes();
	for (auto it = themes.constBegin(); it != themes.constEnd(); ++it)
		themeCombo->addItem(it.key(), it.value());
	for (int i = 0; i < themeCombo->count(); ++i)
	{
		if (QFileInfo(themeCombo->itemData(i).toString()).fileName() == QFileInfo(originalTheme).fileName() && !originalTheme.isEmpty())
			themeCombo->setCurrentIndex(i);
	}
	themeRow->addWidget(themeCombo, 1);
	root->addLayout(themeRow);

	auto* body = new QHBoxLayout;
	root->addLayout(body, 1);
	auto* langList = new QListWidget(&dlg);
	auto* styleList = new QListWidget(&dlg);
	langList->setMaximumWidth(200);
	styleList->setMaximumWidth(240);
	body->addWidget(langList);
	body->addWidget(styleList);

	auto* right = new QVBoxLayout;
	body->addLayout(right, 1);
	auto* colourBox = new QGroupBox(QStringLiteral("Colour Style"), &dlg);
	auto* colourLayout = new QFormLayout(colourBox);
	auto* fgButton = new QPushButton(colourBox);
	auto* bgButton = new QPushButton(colourBox);
	colourLayout->addRow(QStringLiteral("Foreground colour"), fgButton);
	colourLayout->addRow(QStringLiteral("Background colour"), bgButton);
	right->addWidget(colourBox);

	auto* fontBox = new QGroupBox(QStringLiteral("Font Style"), &dlg);
	auto* fontLayout = new QFormLayout(fontBox);
	auto* fontName = new QComboBox(fontBox);
	fontName->addItem(QString());
	fontName->addItems(QFontDatabase::families());
	auto* fontSize = new QComboBox(fontBox);
	fontSize->addItem(QString());
	for (int s : {5, 6, 7, 8, 9, 10, 11, 12, 14, 16, 18, 20, 22, 24, 26, 28})
		fontSize->addItem(QString::number(s));
	auto* bold = new QCheckBox(QStringLiteral("Bold"), fontBox);
	auto* italic = new QCheckBox(QStringLiteral("Italic"), fontBox);
	auto* underline = new QCheckBox(QStringLiteral("Underline"), fontBox);
	fontLayout->addRow(QStringLiteral("Font name"), fontName);
	fontLayout->addRow(QStringLiteral("Font size"), fontSize);
	auto* styleRow = new QHBoxLayout;
	styleRow->addWidget(bold);
	styleRow->addWidget(italic);
	styleRow->addWidget(underline);
	fontLayout->addRow(styleRow);
	right->addWidget(fontBox);

	auto* extRow = new QFormLayout;
	auto* defExt = new QLineEdit(&dlg);
	defExt->setReadOnly(true);
	auto* userExt = new QLineEdit(&dlg);
	extRow->addRow(QStringLiteral("Default ext. :"), defExt);
	extRow->addRow(QStringLiteral("User ext. :"), userExt);
	right->addLayout(extRow);
	auto* defKw = new QPlainTextEdit(&dlg);
	defKw->setReadOnly(true);
	auto* userKw = new QPlainTextEdit(&dlg);
	right->addWidget(new QLabel(QStringLiteral("Default keywords"), &dlg));
	right->addWidget(defKw, 1);
	right->addWidget(new QLabel(QStringLiteral("User-defined keywords"), &dlg));
	right->addWidget(userKw, 1);

	auto* buttons = new QDialogButtonBox(&dlg);
	QPushButton* saveClose = buttons->addButton(QStringLiteral("Save && Close"), QDialogButtonBox::AcceptRole);
	buttons->addButton(QDialogButtonBox::Cancel);
	root->addWidget(buttons);
	Q_UNUSED(saveClose);

	bool updating = false;

	auto currentStyle = [&]() -> Style*
	{
		const int langRow = langList->currentRow();
		const int styleRow = styleList->currentRow();
		if (langRow < 0 || styleRow < 0)
			return nullptr;
		if (langRow == 0)
		{
			auto& g = sm.globalStyles().styles;
			return styleRow < static_cast<int>(g.size()) ? &g[static_cast<size_t>(styleRow)] : nullptr;
		}
		auto& lexers = sm.lexerStylers();
		if (langRow - 1 >= static_cast<int>(lexers.size()))
			return nullptr;
		auto& styles = lexers[static_cast<size_t>(langRow - 1)].styles;
		return styleRow < static_cast<int>(styles.size()) ? &styles[static_cast<size_t>(styleRow)] : nullptr;
	};

	auto currentLexer = [&]() -> LexerStyler*
	{
		const int langRow = langList->currentRow();
		auto& lexers = sm.lexerStylers();
		if (langRow <= 0 || langRow - 1 >= static_cast<int>(lexers.size()))
			return nullptr;
		return &lexers[static_cast<size_t>(langRow - 1)];
	};

	auto preview = [&]
	{
		if (Buffer* b = mw->currentBuffer())
			mw->editView()->defineDocType(b->lang, b->udlName);
	};

	auto showStyle = [&]
	{
		updating = true;
		Style* s = currentStyle();
		const bool valid = (s != nullptr);
		for (QWidget* w : std::initializer_list<QWidget*>{fgButton, bgButton, fontName, fontSize, bold, italic, underline})
			w->setEnabled(valid);
		if (s)
		{
			fgButton->setIcon(colorIcon(s->fgColor, s->fgColor != static_cast<COLORREF>(STYLE_NOT_USED)));
			bgButton->setIcon(colorIcon(s->bgColor, s->bgColor != static_cast<COLORREF>(STYLE_NOT_USED)));
			if (!s->fontName.isEmpty() && fontName->findText(s->fontName) < 0)
				fontName->insertItem(1, s->fontName);
			fontName->setCurrentIndex(std::max(0, fontName->findText(s->fontName)));
			fontSize->setCurrentText(s->fontSize > 0 ? QString::number(s->fontSize) : QString());
			const int fs = s->fontStyle == STYLE_NOT_USED ? 0 : s->fontStyle;
			bold->setChecked(fs & FONTSTYLE_BOLD);
			italic->setChecked(fs & FONTSTYLE_ITALIC);
			underline->setChecked(fs & FONTSTYLE_UNDERLINE);
		}
		LexerStyler* ls = currentLexer();
		userExt->setEnabled(ls != nullptr);
		userExt->setText(ls ? ls->userExt : QString());
		const LangType lt = ls ? sm.langFromName(ls->lexerName) : L_TEXT;
		const LangDef* ld = ls ? sm.langDef(lt) : nullptr;
		defExt->setText(ld ? ld->ext : QString());
		const bool hasKw = s && s->keywordClass >= 0 && s->keywordClass < NB_LIST;
		defKw->setPlainText(hasKw && ld ? QString::fromUtf8(ld->keywords[s->keywordClass]) : QString());
		userKw->setEnabled(hasKw);
		userKw->setPlainText(hasKw ? QString::fromUtf8(s->keywords) : QString());
		updating = false;
	};

	auto fillStyles = [&]
	{
		styleList->clear();
		const int langRow = langList->currentRow();
		if (langRow == 0)
		{
			for (const Style& s : sm.globalStyles().styles)
				styleList->addItem(s.name);
		}
		else if (LexerStyler* ls = currentLexer())
		{
			for (const Style& s : ls->styles)
				styleList->addItem(s.name);
		}
		styleList->setCurrentRow(0);
		showStyle();
	};

	auto fillLanguages = [&]
	{
		langList->clear();
		langList->addItem(QStringLiteral("Global Styles"));
		for (const LexerStyler& ls : sm.lexerStylers())
			langList->addItem(ls.lexerDesc.isEmpty() ? ls.lexerName : ls.lexerDesc);
		langList->setCurrentRow(0);
		fillStyles();
	};

	auto applyFontStyle = [&]
	{
		if (updating)
			return;
		Style* s = currentStyle();
		if (!s)
			return;
		s->fontName = fontName->currentText();
		s->fontSize = fontSize->currentText().isEmpty() ? STYLE_NOT_USED : fontSize->currentText().toInt();
		s->fontStyle = (bold->isChecked() ? FONTSTYLE_BOLD : 0) | (italic->isChecked() ? FONTSTYLE_ITALIC : 0) | (underline->isChecked() ? FONTSTYLE_UNDERLINE : 0);
		preview();
	};

	auto pickColour = [&](bool fg)
	{
		Style* s = currentStyle();
		if (!s)
			return;
		COLORREF& c = fg ? s->fgColor : s->bgColor;
		const QColor initial = (c != static_cast<COLORREF>(STYLE_NOT_USED)) ? toQColor(c) : QColor(Qt::white);
		const QColor chosen = QColorDialog::getColor(initial, &dlg, fg ? QStringLiteral("Foreground colour") : QStringLiteral("Background colour"));
		if (!chosen.isValid())
			return;
		c = fromQColor(chosen);
		showStyle();
		preview();
	};

	QObject::connect(langList, &QListWidget::currentRowChanged, &dlg, [&] { fillStyles(); });
	QObject::connect(styleList, &QListWidget::currentRowChanged, &dlg, [&] { showStyle(); });
	QObject::connect(fgButton, &QPushButton::clicked, &dlg, [&] { pickColour(true); });
	QObject::connect(bgButton, &QPushButton::clicked, &dlg, [&] { pickColour(false); });
	QObject::connect(fontName, &QComboBox::currentTextChanged, &dlg, [&] { applyFontStyle(); });
	QObject::connect(fontSize, &QComboBox::currentTextChanged, &dlg, [&] { applyFontStyle(); });
	for (QCheckBox* c : {bold, italic, underline})
		QObject::connect(c, &QCheckBox::toggled, &dlg, [&] { applyFontStyle(); });
	QObject::connect(userExt, &QLineEdit::textEdited, &dlg, [&](const QString& t)
	{
		if (LexerStyler* ls = currentLexer())
			ls->userExt = t;
	});
	QObject::connect(userKw, &QPlainTextEdit::textChanged, &dlg, [&]
	{
		if (updating)
			return;
		Style* s = currentStyle();
		if (s && s->keywordClass >= 0)
		{
			s->keywords = userKw->toPlainText().toUtf8().toStdString();
			s->hasKeywordsNode = true;
			preview();
		}
	});
	QObject::connect(themeCombo, &QComboBox::currentIndexChanged, &dlg, [&]
	{
		sm.loadTheme(themeCombo->currentData().toString());
		fillLanguages();
		preview();
	});
	QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
	QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

	fillLanguages();

	if (dlg.exec() == QDialog::Accepted)
	{
		QString err;
		if (!sm.saveCurrentTheme(&err))
			QMessageBox::warning(mw, QStringLiteral("Style Configurator"), err);
		Npp::prefs().themeFile = sm.currentThemePath();
		Npp::prefs().save();
		mw->applyTheme(Npp::prefs().themeFile);
	}
	else
	{
		mw->applyTheme(originalTheme);
	}
}

// ---------------------------------------------------------------------------
// Shortcut Mapper
// ---------------------------------------------------------------------------
void shortcutMapper(MainWindow* mw)
{
	QDialog dlg(mw);
	dlg.setWindowTitle(QStringLiteral("Shortcut Mapper"));
	dlg.resize(720, 560);
	auto* root = new QVBoxLayout(&dlg);
	auto* filter = new QLineEdit(&dlg);
	filter->setPlaceholderText(QStringLiteral("Filter"));
	root->addWidget(filter);
	auto* table = new QTableWidget(&dlg);
	table->setColumnCount(3);
	table->setHorizontalHeaderLabels({QStringLiteral("Name"), QStringLiteral("Shortcut"), QStringLiteral("Category")});
	table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
	table->setSelectionBehavior(QAbstractItemView::SelectRows);
	table->setSelectionMode(QAbstractItemView::SingleSelection);
	table->setEditTriggers(QAbstractItemView::NoEditTriggers);
	table->verticalHeader()->hide();

	// Scintilla commands (ScintillaKeyMap): the keys of the editor commands without menu item
	auto* sciTable = new QTableWidget(&dlg);
	sciTable->setObjectName(QStringLiteral("scintillaKeysTable"));
	sciTable->setColumnCount(2);
	sciTable->setHorizontalHeaderLabels({QStringLiteral("Name"), QStringLiteral("Shortcut")});
	sciTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
	sciTable->setSelectionBehavior(QAbstractItemView::SelectRows);
	sciTable->setSelectionMode(QAbstractItemView::SingleSelection);
	sciTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
	sciTable->verticalHeader()->hide();
	auto* tabs = new QTabWidget(&dlg);
	tabs->setObjectName(QStringLiteral("shortcutMapperTabs"));
	tabs->addTab(table, QStringLiteral("Main menu"));
	tabs->addTab(sciTable, QStringLiteral("Scintilla commands"));
	root->addWidget(tabs, 1);
	std::vector<ScintillaKeyCmd>& sciCmds = Macros::scintillaKeys();
	std::vector<std::vector<ScintillaKeyCombo>> sciKeys;
	for (const ScintillaKeyCmd& c : sciCmds)
		sciKeys.push_back(c.keys);
	auto sciText = [&](size_t i)
	{
		QStringList parts;
		for (const ScintillaKeyCombo& k : sciKeys[i])
		{
			if (k.vk)
				parts << Macros::keyText(k);
		}
		return parts.join(QStringLiteral(" or "));
	};
	auto fillSci = [&]
	{
		sciTable->setRowCount(0);
		const QString f = filter->text();
		for (size_t i = 0; i < sciCmds.size(); ++i)
		{
			if (sciCmds[i].menuCmd != 0)
				continue; // with the menu commands of the Main menu tab
			if (!f.isEmpty() && !sciCmds[i].name.contains(f, Qt::CaseInsensitive))
				continue;
			const int row = sciTable->rowCount();
			sciTable->insertRow(row);
			auto* item = new QTableWidgetItem(sciCmds[i].name);
			item->setData(Qt::UserRole, static_cast<int>(i));
			sciTable->setItem(row, 0, item);
			sciTable->setItem(row, 1, new QTableWidgetItem(sciText(i)));
		}
	};
	auto selectedSci = [&]() -> int
	{
		const int row = sciTable->currentRow();
		return row >= 0 ? sciTable->item(row, 0)->data(Qt::UserRole).toInt() : -1;
	};


	QSettings& s = dlgSettings();
	QHash<int, QString> pending; // id -> new portable key sequence ("" = none), "<reset>" = default
	QList<int> ids = mw->commandIds();
	std::sort(ids.begin(), ids.end(), [mw](int a, int b) { return mw->commandOrder(a) < mw->commandOrder(b); });

	auto shortcutText = [&](int id)
	{
		if (pending.contains(id))
		{
			const QString v = pending.value(id);
			if (v != QStringLiteral("<reset>"))
				return QKeySequence(v, QKeySequence::PortableText).toString(QKeySequence::NativeText);
		}
		QAction* a = mw->action(id);
		if (!a)
			return QString();
		if (!a->shortcuts().isEmpty())
			return a->shortcut().toString(QKeySequence::NativeText);
		const int tab = static_cast<int>(a->text().indexOf(QLatin1Char('\t')));
		return tab >= 0 ? a->text().mid(tab + 1) : QString();
	};

	auto fill = [&]
	{
		table->setRowCount(0);
		const QString f = filter->text();
		for (int id : ids)
		{
			const QString name = mw->commandName(id);
			if (name.isEmpty() || !mw->hasCommand(id))
				continue;
			if (!f.isEmpty() && !name.contains(f, Qt::CaseInsensitive))
				continue;
			const int row = table->rowCount();
			table->insertRow(row);
			auto* item = new QTableWidgetItem(name);
			item->setData(Qt::UserRole, id);
			table->setItem(row, 0, item);
			table->setItem(row, 1, new QTableWidgetItem(shortcutText(id)));
			table->setItem(row, 2, new QTableWidgetItem(mw->commandCategory(id)));
		}
	};
	fill();
	fillSci();
	QObject::connect(filter, &QLineEdit::textChanged, &dlg, fill);
	QObject::connect(filter, &QLineEdit::textChanged, &dlg, fillSci);

	auto selectedId = [&]() -> int
	{
		const int row = table->currentRow();
		return row >= 0 ? table->item(row, 0)->data(Qt::UserRole).toInt() : 0;
	};

	auto modifySci = [&]
	{
		const int i = selectedSci();
		if (i < 0)
			return;
		QDialog edit(&dlg);
		edit.setWindowTitle(sciCmds[static_cast<size_t>(i)].name);
		auto* l = new QVBoxLayout(&edit);
		auto* keyEdit = new QKeySequenceEdit(&edit);
		keyEdit->setMaximumSequenceLength(1);
		l->addWidget(new QLabel(QStringLiteral("Press the new shortcut:"), &edit));
		l->addWidget(keyEdit);
		auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &edit);
		l->addWidget(bb);
		QObject::connect(bb, &QDialogButtonBox::accepted, &edit, &QDialog::accept);
		QObject::connect(bb, &QDialogButtonBox::rejected, &edit, &QDialog::reject);
		keyEdit->setFocus();
		if (edit.exec() != QDialog::Accepted)
			return;
		ScintillaKeyCombo k;
		if (!Macros::keyComboFromSequence(keyEdit->keySequence(), &k))
			return;
		for (size_t other = 0; other < sciKeys.size(); ++other)
		{
			if (static_cast<int>(other) == i)
				continue;
			auto& ok = sciKeys[other];
			if (std::find(ok.begin(), ok.end(), k) == ok.end())
				continue;
			const auto answer = QMessageBox::question(&dlg, QStringLiteral("Conflict"),
				QStringLiteral("This shortcut is already used by \"%1\". Remove it from there?").arg(sciCmds[other].name));
			if (answer != QMessageBox::Yes)
				return;
			ok.erase(std::remove(ok.begin(), ok.end(), k), ok.end());
		}
		sciKeys[static_cast<size_t>(i)] = {k};
		fillSci();
	};
	QObject::connect(sciTable, &QTableWidget::cellDoubleClicked, &dlg, modifySci);

	auto modify = [&]
	{
		if (tabs->currentIndex() == 1)
		{
			modifySci();
			return;
		}
		const int id = selectedId();
		if (!id)
			return;
		QDialog edit(&dlg);
		edit.setWindowTitle(mw->commandName(id));
		auto* l = new QVBoxLayout(&edit);
		auto* keyEdit = new QKeySequenceEdit(&edit);
		keyEdit->setMaximumSequenceLength(1);
		l->addWidget(new QLabel(QStringLiteral("Press the new shortcut:"), &edit));
		l->addWidget(keyEdit);
		auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &edit);
		l->addWidget(bb);
		QObject::connect(bb, &QDialogButtonBox::accepted, &edit, &QDialog::accept);
		QObject::connect(bb, &QDialogButtonBox::rejected, &edit, &QDialog::reject);
		keyEdit->setFocus();
		if (edit.exec() != QDialog::Accepted || keyEdit->keySequence().isEmpty())
			return;
		const QString seq = keyEdit->keySequence().toString(QKeySequence::PortableText);
		for (int other : ids)
		{
			if (other != id && shortcutText(other) == keyEdit->keySequence().toString(QKeySequence::NativeText))
			{
				const auto answer = QMessageBox::question(&dlg, QStringLiteral("Conflict"),
					QStringLiteral("This shortcut is already used by \"%1\". Remove it from there?").arg(mw->commandName(other)));
				if (answer != QMessageBox::Yes)
					return;
				pending[other] = QString();
			}
		}
		pending[id] = seq;
		fill();
	};
	QObject::connect(table, &QTableWidget::cellDoubleClicked, &dlg, modify);

	auto* buttons = new QDialogButtonBox(&dlg);
	QPushButton* modifyBtn = buttons->addButton(QStringLiteral("Modify"), QDialogButtonBox::ActionRole);
	QPushButton* clearBtn = buttons->addButton(QStringLiteral("Clear"), QDialogButtonBox::ActionRole);
	QPushButton* resetBtn = buttons->addButton(QStringLiteral("Reset to default"), QDialogButtonBox::ActionRole);
	buttons->addButton(QDialogButtonBox::Ok);
	buttons->addButton(QDialogButtonBox::Cancel);
	root->addWidget(buttons);
	QObject::connect(modifyBtn, &QPushButton::clicked, &dlg, modify);
	QObject::connect(clearBtn, &QPushButton::clicked, &dlg, [&]
	{
		if (tabs->currentIndex() == 1)
		{
			if (const int i = selectedSci(); i >= 0)
			{
				sciKeys[static_cast<size_t>(i)].clear();
				fillSci();
			}
			return;
		}
		if (const int id = selectedId())
		{
			pending[id] = QString();
			fill();
		}
	});
	QObject::connect(resetBtn, &QPushButton::clicked, &dlg, [&]
	{
		if (tabs->currentIndex() == 1)
		{
			if (const int i = selectedSci(); i >= 0)
			{
				sciKeys[static_cast<size_t>(i)] = sciCmds[static_cast<size_t>(i)].defaults;
				fillSci();
			}
			return;
		}
		if (const int id = selectedId())
		{
			pending[id] = QStringLiteral("<reset>");
			fill();
		}
	});
	QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
	QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

	if (dlg.exec() != QDialog::Accepted)
		return;
	s.beginGroup(QStringLiteral("Shortcuts"));
	for (auto it = pending.constBegin(); it != pending.constEnd(); ++it)
	{
		if (it.value() == QStringLiteral("<reset>"))
			s.remove(QString::number(it.key()));
		else
			s.setValue(QString::number(it.key()), it.value());
	}
	s.endGroup();
	s.sync();
	mw->applyShortcuts();
	bool sciChanged = false;
	for (size_t i = 0; i < sciCmds.size(); ++i)
	{
		if (sciCmds[i].keys != sciKeys[i])
		{
			sciCmds[i].keys = sciKeys[i];
			sciChanged = true;
		}
	}
	if (sciChanged)
	{
		QString error;
		if (!Macros::saveScintillaKeys(&error))
			QMessageBox::warning(mw, QStringLiteral("Shortcut Mapper"), error);
	}
	mw->applyScintillaKeys();
}

// ---------------------------------------------------------------------------
// Windows (WindowsDlg)
// ---------------------------------------------------------------------------
void windowsList(MainWindow* mw)
{
	QDialog dlg(mw);
	dlg.setWindowTitle(QStringLiteral("Windows"));
	dlg.resize(760, 420);
	auto* root = new QHBoxLayout(&dlg);
	auto* table = new QTableWidget(&dlg);
	table->setColumnCount(4);
	table->setHorizontalHeaderLabels({QStringLiteral("Name"), QStringLiteral("Path"), QStringLiteral("Type"), QStringLiteral("Size")});
	table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
	table->setSelectionBehavior(QAbstractItemView::SelectRows);
	table->setEditTriggers(QAbstractItemView::NoEditTriggers);
	table->verticalHeader()->hide();
	table->setSortingEnabled(false);
	root->addWidget(table, 1);

	std::vector<Buffer*> bufs;
	auto fill = [&]
	{
		bufs = mw->allBuffers();
		table->setRowCount(static_cast<int>(bufs.size()));
		for (int i = 0; i < static_cast<int>(bufs.size()); ++i)
		{
			Buffer* b = bufs[static_cast<size_t>(i)];
			qlonglong len = 0;
			mw->withDocument(b, [&](EditView* v) { len = v->ex(SCI_GETLENGTH); });
			table->setItem(i, 0, new QTableWidgetItem((b->dirty ? QStringLiteral("*") : QString()) + b->displayName()));
			table->setItem(i, 1, new QTableWidgetItem(b->isUntitled() ? QString() : QFileInfo(b->path).absolutePath()));
			table->setItem(i, 2, new QTableWidgetItem(QString::fromUtf8(StyleManager::langInfo(b->lang).shortName)));
			table->setItem(i, 3, new QTableWidgetItem(QLocale::system().toString(len)));
		}
	};
	fill();

	auto selected = [&]()
	{
		std::vector<Buffer*> out;
		for (const QModelIndex& idx : table->selectionModel()->selectedRows())
			out.push_back(bufs[static_cast<size_t>(idx.row())]);
		return out;
	};

	auto* buttons = new QVBoxLayout;
	root->addLayout(buttons);
	auto* activate = new QPushButton(QStringLiteral("&Activate"), &dlg);
	auto* save = new QPushButton(QStringLiteral("&Save"), &dlg);
	auto* closeBtn = new QPushButton(QStringLiteral("&Close window(s)"), &dlg);
	auto* ok = new QPushButton(QStringLiteral("&OK"), &dlg);
	for (QPushButton* b : {activate, save, closeBtn, ok})
		buttons->addWidget(b);
	buttons->addStretch();

	auto doActivate = [&]
	{
		const auto sel = selected();
		if (!sel.empty())
		{
			mw->activateBuffer(sel.front());
			dlg.accept();
		}
	};
	QObject::connect(activate, &QPushButton::clicked, &dlg, doActivate);
	QObject::connect(table, &QTableWidget::cellDoubleClicked, &dlg, doActivate);
	QObject::connect(save, &QPushButton::clicked, &dlg, [&]
	{
		for (Buffer* b : selected())
			mw->saveBuffer(b);
		fill();
	});
	QObject::connect(closeBtn, &QPushButton::clicked, &dlg, [&]
	{
		for (Buffer* b : selected())
		{
			if (!mw->closeBuffer(b, true))
				break;
		}
		fill();
	});
	QObject::connect(ok, &QPushButton::clicked, &dlg, &QDialog::accept);
	dlg.exec();
}

// ---------------------------------------------------------------------------
// Run a Macro Multiple Times
// ---------------------------------------------------------------------------
int macroRunMultiple(QWidget* parent, const QStringList& names, int* macroIndex)
{
	QDialog dlg(parent);
	dlg.setObjectName(QStringLiteral("runMacroDlg"));
	dlg.setWindowTitle(QStringLiteral("Run a Macro Multiple Times"));
	auto* layout = new QVBoxLayout(&dlg);
	auto* macroRow = new QHBoxLayout;
	auto* macroCombo = new QComboBox(&dlg);
	macroCombo->setObjectName(QStringLiteral("macroCombo"));
	macroCombo->addItems(names);
	macroRow->addWidget(new QLabel(QStringLiteral("Macro to run"), &dlg));
	macroRow->addWidget(macroCombo, 1);
	layout->addLayout(macroRow);
	auto* timesRow = new QHBoxLayout;
	auto* runTimes = new QRadioButton(QStringLiteral("Run"), &dlg);
	auto* times = new QSpinBox(&dlg);
	times->setRange(1, 100000);
	timesRow->addWidget(runTimes);
	timesRow->addWidget(times);
	timesRow->addWidget(new QLabel(QStringLiteral("times"), &dlg));
	timesRow->addStretch();
	layout->addLayout(timesRow);
	auto* untilEnd = new QRadioButton(QStringLiteral("Run until the end of file"), &dlg);
	layout->addWidget(untilEnd);
	runTimes->setChecked(true);
	auto* buttons = new QDialogButtonBox(&dlg);
	buttons->addButton(QStringLiteral("Run"), QDialogButtonBox::AcceptRole);
	buttons->addButton(QDialogButtonBox::Cancel);
	layout->addWidget(buttons);
	QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
	QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
	if (dlg.exec() != QDialog::Accepted)
		return -1;
	if (macroIndex)
		*macroIndex = macroCombo->currentIndex();
	return untilEnd->isChecked() ? 0 : times->value();
}

bool findCharInRange(EditView* view, unsigned char beginRange, unsigned char endRange, long long startPos, bool down, bool wrap)
{
	// FindCharsInRangeDlg::findCharInRange(): the bytes of the document (UTF-8 here)
	const sptr_t totalSize = view->ex(SCI_GETLENGTH);
	if (startPos == -1)
		startPos = down ? 0 : totalSize - 1;
	if (startPos > totalSize)
		return false;
	const QByteArray content = view->textRange(0, totalSize);
	auto inRange = [&](long long i)
	{
		const auto c = static_cast<unsigned char>(content.at(static_cast<qsizetype>(i)));
		return c >= beginRange && c <= endRange;
	};
	long long found = -1;
	for (long long i = startPos - (down ? 0 : 1); down ? i < totalSize : i >= 0; down ? ++i : --i)
	{
		if (inRange(i))
		{
			found = i;
			break;
		}
	}
	if (found < 0 && wrap)
	{
		for (long long i = down ? 0 : totalSize - 1; down ? i < totalSize : i >= 0; down ? ++i : --i)
		{
			if (inRange(i))
			{
				found = i;
				break;
			}
		}
	}
	if (found < 0)
		return false;
	view->ex(SCI_ENSUREVISIBLE, view->ex(SCI_LINEFROMPOSITION, found));
	view->ex(SCI_GOTOPOS, found);
	view->ex(SCI_SETSEL, down ? found : found + 1, down ? found + 1 : found);
	return true;
}

void findCharsInRange(MainWindow* mw)
{
	static QPointer<QDialog> dlg;
	if (!dlg)
	{
		dlg = new QDialog(mw);
		dlg->setObjectName(QStringLiteral("findCharsInRangeDlg"));
		dlg->setWindowTitle(QStringLiteral("Find Characters in Range..."));
		auto* nonAscii = new QRadioButton(QStringLiteral("&Non-ASCII characters (128–255)"), dlg);
		auto* ascii = new QRadioButton(QStringLiteral("&ASCII characters (0–127)"), dlg);
		auto* custom = new QRadioButton(QStringLiteral("Custom &range (0–255):"), dlg);
		auto* startEdit = new QLineEdit(QStringLiteral("0"), dlg);
		auto* endEdit = new QLineEdit(QStringLiteral("255"), dlg);
		for (QLineEdit* le : {startEdit, endEdit})
		{
			le->setMaxLength(3);
			le->setAlignment(Qt::AlignCenter);
			le->setFixedWidth(le->fontMetrics().horizontalAdvance(QStringLiteral("0000")) + 12);
			le->setValidator(new QIntValidator(0, 999, le));
			le->setEnabled(false);
		}
		auto* dash = new QLabel(QStringLiteral("–"), dlg);
		dash->setEnabled(false);
		nonAscii->setChecked(true);
		auto* rangeGroup = new QButtonGroup(dlg);
		rangeGroup->addButton(nonAscii);
		rangeGroup->addButton(ascii);
		rangeGroup->addButton(custom);
		QObject::connect(custom, &QRadioButton::toggled, dlg, [startEdit, endEdit, dash](bool on)
		{
			startEdit->setEnabled(on);
			endEdit->setEnabled(on);
			dash->setEnabled(on);
		});

		auto* dirBox = new QGroupBox(QStringLiteral("Direction"), dlg);
		auto* up = new QRadioButton(QStringLiteral("&Up"), dirBox);
		auto* downBtn = new QRadioButton(QStringLiteral("&Down"), dirBox);
		downBtn->setChecked(true);
		auto* dirLayout = new QVBoxLayout(dirBox);
		dirLayout->addWidget(up);
		dirLayout->addWidget(downBtn);
		auto* wrapBox = new QCheckBox(QStringLiteral("Wra&p around"), dlg);

		auto* findBtn = new QPushButton(QStringLiteral("&Find"), dlg);
		findBtn->setDefault(true);
		auto* closeBtn = new QPushButton(QStringLiteral("&Close"), dlg);

		auto* customRow = new QHBoxLayout;
		customRow->addWidget(custom);
		customRow->addWidget(startEdit);
		customRow->addWidget(dash);
		customRow->addWidget(endEdit);
		customRow->addStretch();
		auto* bottom = new QHBoxLayout;
		bottom->addWidget(dirBox);
		bottom->addWidget(wrapBox, 0, Qt::AlignBottom);
		bottom->addStretch();
		auto* left = new QVBoxLayout;
		left->addWidget(nonAscii);
		left->addWidget(ascii);
		left->addLayout(customRow);
		left->addLayout(bottom);
		auto* right = new QVBoxLayout;
		right->addWidget(findBtn);
		right->addWidget(closeBtn);
		right->addStretch();
		auto* root = new QHBoxLayout(dlg);
		root->addLayout(left, 1);
		root->addLayout(right);

		QObject::connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::hide);
		QObject::connect(findBtn, &QPushButton::clicked, dlg, [mw, nonAscii, ascii, startEdit, endEdit, up, wrapBox]
		{
			// ID_FINDCHAR_NEXT: getRangeFromUI() / getDirectionFromUI()
			int start = 128, end = 255;
			if (ascii->isChecked())
			{
				start = 0;
				end = 127;
			}
			else if (!nonAscii->isChecked())
			{
				bool okStart = false, okEnd = false;
				start = startEdit->text().toInt(&okStart);
				end = endEdit->text().toInt(&okEnd);
				if (!okStart || !okEnd || start > 255 || end > 255 || start > end)
				{
					QMessageBox::warning(dlg, QStringLiteral("Range Value problem"), QStringLiteral("You should type between 0 and 255."));
					return;
				}
			}
			EditView* v = mw->editView();
			findCharInRange(v, static_cast<unsigned char>(start), static_cast<unsigned char>(end), v->ex(SCI_GETCURRENTPOS), !up->isChecked(),
			                wrapBox->isChecked());
		});
	}
	dlg->show();
	dlg->raise();
	dlg->activateWindow();
}

} // namespace NppDialogs
