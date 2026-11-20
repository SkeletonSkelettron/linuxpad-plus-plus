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

// Settings > Preferences: Qt counterpart of WinControls/Preference/preferenceDlg.cpp
// (24 pages) and MISC/RegExt/regExtDlg.cpp (File Association). Like Notepad++, every
// change applies at once; the dialog only has a Close button.

#include "Dialogs.h"

#include <algorithm>
#include <functional>
#include <memory>

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QMimeDatabase>
#include <QPainter>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSettings>
#include <QSlider>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>

#include "EncodingMapper.h"
#include "FileIO.h"
#include "LangStyles.h"
#include "Localization.h"
#include "MainWindow.h"

namespace
{
	QSettings& prefsSettings()
	{
		static QSettings s(Npp::configDir() + QStringLiteral("/config.ini"), QSettings::IniFormat);
		return s;
	}

	enum ApplyFlag
	{
		ApplyNormal = 0,
		ApplyLocalization = 1,
		ApplyDarkMode = 2,
		ApplyRecent = 4,
	};

	QIcon swatch(const QColor& c)
	{
		QPixmap pm(28, 14);
		pm.fill(c.isValid() ? c : QColor(Qt::transparent));
		QPainter p(&pm);
		p.setPen(Qt::gray);
		p.drawRect(0, 0, pm.width() - 1, pm.height() - 1);
		return QIcon(pm);
	}

	// ---------------------------------------------------------------------------
	// File Association (MISC/RegExt/regExtDlg.cpp) with the freedesktop MIME database
	// ---------------------------------------------------------------------------
	struct ExtGroup
	{
		const char* name;
		QStringList exts;
	};

	const std::vector<ExtGroup>& extGroups()
	{
		static const std::vector<ExtGroup> groups = {
			{"Notepad", {".txt", ".log"}},
			{"ms ini/inf", {".ini", ".inf"}},
			{"c, c++, objc", {".h", ".hh", ".hpp", ".hxx", ".c", ".cpp", ".cxx", ".cc", ".m", ".mm", ".vcxproj", ".vcproj", ".props", ".vsprops", ".manifest"}},
			{"java, c#, pascal", {".java", ".cs", ".pas", ".pp", ".inc"}},
			{"web script", {".html", ".htm", ".shtml", ".shtm", ".hta", ".asp", ".aspx", ".css", ".js", ".json", ".mjs", ".jsm", ".jsp", ".php", ".php3",
			                ".php4", ".php5", ".phps", ".phpt", ".phtml", ".xml", ".xhtml", ".xht", ".xul", ".kml", ".xaml", ".xsml"}},
			{"public script", {".sh", ".bsh", ".bash", ".bat", ".cmd", ".nsi", ".nsh", ".lua", ".pl", ".pm", ".py"}},
			{"property script", {".rc", ".as", ".mx", ".vb", ".vbs"}},
			{"fortran, TeX, SQL", {".f", ".for", ".f90", ".f95", ".f2k", ".tex", ".sql"}},
			{"misc", {".nfo", ".mak"}},
			{"customize", {}},
		};
		return groups;
	}

	const QString kDesktopFile = QStringLiteral("notepad-plus-plus.desktop");

	QString mimeAppsPath()
	{
		QString base = QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation);
		if (base.isEmpty())
			base = QDir::homePath() + QStringLiteral("/.config");
		return base + QStringLiteral("/mimeapps.list");
	}

	QString mimeForExt(const QString& ext, bool* custom)
	{
		static QMimeDatabase db;
		const QList<QMimeType> types = db.mimeTypesForFileName(QStringLiteral("x") + ext);
		*custom = types.isEmpty();
		if (types.isEmpty())
			return QStringLiteral("application/x-notepad-plus-plus-%1").arg(ext.mid(1).toLower());
		return types.first().name();
	}

	// MIME types associated with Notepad++ in mimeapps.list [Default Applications]
	QStringList registeredMimes()
	{
		QStringList mimes;
		QFile f(mimeAppsPath());
		if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
			return mimes;
		bool inDefaults = false;
		QTextStream in(&f);
		while (!in.atEnd())
		{
			const QString line = in.readLine().trimmed();
			if (line.startsWith(QLatin1Char('[')))
			{
				inDefaults = (line == QStringLiteral("[Default Applications]"));
				continue;
			}
			if (!inDefaults)
				continue;
			const int eq = line.indexOf(QLatin1Char('='));
			if (eq > 0 && line.mid(eq + 1).split(QLatin1Char(';'), Qt::SkipEmptyParts).value(0) == kDesktopFile)
				mimes << line.left(eq);
		}
		return mimes;
	}

	bool registerExt(const QString& ext, QString* error)
	{
		bool custom = false;
		const QString mime = mimeForExt(ext, &custom);
		if (custom)
		{
			// unknown extension: user MIME type so that it can be associated
			QString data = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation);
			const QString pkgDir = data + QStringLiteral("/mime/packages");
			QDir().mkpath(pkgDir);
			QFile pkg(pkgDir + QStringLiteral("/notepad-plus-plus-%1.xml").arg(ext.mid(1).toLower()));
			if (!pkg.open(QIODevice::WriteOnly | QIODevice::Truncate))
			{
				*error = pkg.errorString();
				return false;
			}
			QTextStream out(&pkg);
			out << "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
			    << "<mime-info xmlns=\"http://www.freedesktop.org/standards/shared-mime-info\">\n"
			    << "  <mime-type type=\"" << mime << "\">\n"
			    << "    <sub-class-of type=\"text/plain\"/>\n"
			    << "    <comment>" << ext.mid(1) << " file</comment>\n"
			    << "    <glob pattern=\"*" << ext << "\"/>\n"
			    << "  </mime-type>\n</mime-info>\n";
			pkg.close();
			QProcess::execute(QStringLiteral("update-mime-database"), {data + QStringLiteral("/mime")});
		}
		const int rc = QProcess::execute(QStringLiteral("xdg-mime"), {QStringLiteral("default"), kDesktopFile, mime});
		if (rc != 0)
		{
			*error = QStringLiteral("xdg-mime failed for %1").arg(mime);
			return false;
		}
		return true;
	}

	void unregisterMime(const QString& mime)
	{
		// remove the Notepad++ entries of this MIME type from mimeapps.list
		QFile f(mimeAppsPath());
		if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
			return;
		QStringList lines;
		QTextStream in(&f);
		while (!in.atEnd())
			lines << in.readLine();
		f.close();
		for (QString& line : lines)
		{
			if (!line.startsWith(mime + QLatin1Char('=')))
				continue;
			QStringList apps = line.mid(mime.size() + 1).split(QLatin1Char(';'), Qt::SkipEmptyParts);
			apps.removeAll(kDesktopFile);
			line = apps.isEmpty() ? QString() : mime + QLatin1Char('=') + apps.join(QLatin1Char(';')) + QLatin1Char(';');
		}
		lines.removeAll(QString());
		if (f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
		{
			QTextStream out(&f);
			for (const QString& l : lines)
				out << l << '\n';
		}
	}
}

namespace NppDialogs
{

void preferences(MainWindow* mw)
{
	NppPrefs& p = Npp::prefs();
	QDialog dlg(mw);
	dlg.setWindowTitle(QStringLiteral("Preferences"));
	dlg.resize(940, 560);
	auto* root = new QVBoxLayout(&dlg);
	auto* body = new QHBoxLayout;
	root->addLayout(body, 1);
	auto* list = new QListWidget(&dlg);
	list->setMaximumWidth(200);
	list->setObjectName(QStringLiteral("prefsPageList"));
	auto* stack = new QStackedWidget(&dlg);
	stack->setObjectName(QStringLiteral("prefsPages"));
	body->addWidget(list);
	body->addWidget(stack, 1);

	// ---------------------------------------------------------------------
	// live apply
	// ---------------------------------------------------------------------
	int pendingFlags = 0;
	QTimer applyTimer;
	applyTimer.setSingleShot(true);
	applyTimer.setInterval(150);
	auto applyNow = [&]
	{
		applyTimer.stop();
		const int f = pendingFlags;
		pendingFlags = 0;
		p.save();
		if (f & ApplyLocalization)
		{
			Localization::instance().load(p.localization);
			mw->retranslateMenus();
		}
		if (f & ApplyDarkMode)
		{
			mw->applyDarkMode();
			mw->applyTheme(p.themeFile);
		}
		mw->applyPrefs();
	};
	QObject::connect(&applyTimer, &QTimer::timeout, &dlg, applyNow);
	auto changed = [&pendingFlags, &applyTimer](int flags) { pendingFlags |= flags; applyTimer.start(); };

	// ---------------------------------------------------------------------
	// builders
	// ---------------------------------------------------------------------
	auto addPage = [&](const QString& title, const QString& key) -> QVBoxLayout*
	{
		auto* page = new QWidget;
		page->setObjectName(QStringLiteral("prefsPage_") + key);
		auto* l = new QVBoxLayout(page);
		auto* scroll = new QScrollArea;
		scroll->setWidget(page);
		scroll->setWidgetResizable(true);
		scroll->setFrameShape(QFrame::NoFrame);
		stack->addWidget(scroll);
		auto* item = new QListWidgetItem(title, list);
		item->setData(Qt::UserRole, key);
		return l;
	};
	auto check = [&](const QString& text, bool& field, int flags = ApplyNormal)
	{
		auto* c = new QCheckBox(text);
		c->setChecked(field);
		QObject::connect(c, &QCheckBox::toggled, &dlg, [&field, changed, flags](bool on) { field = on; changed(flags); });
		return c;
	};
	auto checkInv = [&](const QString& text, bool& field, int flags = ApplyNormal)
	{
		auto* c = new QCheckBox(text);
		c->setChecked(!field);
		QObject::connect(c, &QCheckBox::toggled, &dlg, [&field, changed, flags](bool on) { field = !on; changed(flags); });
		return c;
	};
	auto radio = [](QButtonGroup* g, const QString& text, int id)
	{
		auto* r = new QRadioButton(text);
		g->addButton(r, id);
		return r;
	};
	auto bindGroup = [&](QButtonGroup* g, int& field, int flags = ApplyNormal)
	{
		if (QAbstractButton* b = g->button(field))
			b->setChecked(true);
		QObject::connect(g, &QButtonGroup::idToggled, &dlg, [&field, changed, flags](int id, bool on)
		{
			if (on)
			{
				field = id;
				changed(flags);
			}
		});
	};
	auto bindBoolGroup = [&](QButtonGroup* g, bool& field, int flags = ApplyNormal) // ids 1 (true) / 0 (false)
	{
		if (QAbstractButton* b = g->button(field ? 1 : 0))
			b->setChecked(true);
		QObject::connect(g, &QButtonGroup::idToggled, &dlg, [&field, changed, flags](int id, bool on)
		{
			if (on)
			{
				field = (id == 1);
				changed(flags);
			}
		});
	};
	auto spin = [&](int& field, int min, int max, int flags = ApplyNormal)
	{
		auto* s = new QSpinBox;
		s->setRange(min, max);
		s->setValue(std::clamp(field, min, max));
		QObject::connect(s, &QSpinBox::valueChanged, &dlg, [&field, changed, flags](int v) { field = v; changed(flags); });
		return s;
	};
	auto slider = [&](int& field, int min, int max, int step, QLabel* valueLabel, int flags = ApplyNormal)
	{
		auto* s = new QSlider(Qt::Horizontal);
		s->setRange(min, max);
		s->setSingleStep(step);
		s->setPageStep(step);
		s->setValue(std::clamp(field, min, max));
		if (valueLabel)
			valueLabel->setText(QString::number(s->value()));
		QObject::connect(s, &QSlider::valueChanged, &dlg, [&field, changed, flags, valueLabel, step, min](int v)
		{
			if (step > 1)
				v = min + ((v - min) / step) * step;
			field = v;
			if (valueLabel)
				valueLabel->setText(QString::number(v));
			changed(flags);
		});
		return s;
	};
	auto line = [&](QString& field, int flags = ApplyNormal)
	{
		auto* e = new QLineEdit(field);
		QObject::connect(e, &QLineEdit::textChanged, &dlg, [&field, changed, flags](const QString& t) { field = t; changed(flags); });
		return e;
	};
	auto comboIndex = [&](const QStringList& items, int& field, int flags = ApplyNormal)
	{
		auto* c = new QComboBox;
		c->addItems(items);
		c->setCurrentIndex(std::clamp(field, 0, static_cast<int>(items.size()) - 1));
		QObject::connect(c, &QComboBox::currentIndexChanged, &dlg, [&field, changed, flags](int i) { field = i; changed(flags); });
		return c;
	};
	auto group = [](const QString& title, QLayout* l)
	{
		auto* g = new QGroupBox(title);
		g->setLayout(l);
		return g;
	};
	auto vbox = [](std::initializer_list<QWidget*> ws)
	{
		auto* l = new QVBoxLayout;
		for (QWidget* w : ws)
			l->addWidget(w);
		return l;
	};
	auto hbox = [](std::initializer_list<QWidget*> ws, bool stretch = true)
	{
		auto* l = new QHBoxLayout;
		for (QWidget* w : ws)
			l->addWidget(w);
		if (stretch)
			l->addStretch(1);
		return l;
	};
	auto hboxWidget = [&hbox](std::initializer_list<QWidget*> ws)
	{
		auto* w = new QWidget;
		auto* l = hbox(ws);
		l->setContentsMargins(0, 0, 0, 0);
		w->setLayout(l);
		return w;
	};
	auto indented = [](QWidget* w)
	{
		auto* holder = new QWidget;
		auto* l = new QHBoxLayout(holder);
		l->setContentsMargins(20, 0, 0, 0);
		l->addWidget(w);
		return holder;
	};
	auto label = [](const QString& text)
	{
		auto* l = new QLabel(text);
		l->setWordWrap(true);
		return l;
	};
	auto dependsOn = [&dlg](QAbstractButton* master, std::initializer_list<QWidget*> slaves, bool invert = false)
	{
		std::vector<QWidget*> v(slaves);
		auto update = [master, v, invert] { for (QWidget* w : v) w->setEnabled(master->isChecked() != invert && master->isEnabled()); };
		QObject::connect(master, &QAbstractButton::toggled, &dlg, update);
		update();
	};
	auto notOnLinux = [](QWidget* w, const QString& why)
	{
		w->setEnabled(false);
		w->setToolTip(why);
	};
	auto browseDir = [&dlg](QLineEdit* edit, const QString& title)
	{
		auto* b = new QPushButton(QStringLiteral("..."));
		b->setFixedWidth(32);
		QObject::connect(b, &QPushButton::clicked, &dlg, [&dlg, edit, title]
		{
			const QString dir = QFileDialog::getExistingDirectory(&dlg, title, edit->text().isEmpty() ? QDir::homePath() : edit->text());
			if (!dir.isEmpty())
				edit->setText(dir);
		});
		return b;
	};
	const QString needRestart = QStringLiteral("(Need to restart Notepad++)");

	// =====================================================================
	// General
	// =====================================================================
	{
		QVBoxLayout* page = addPage(QStringLiteral("General"), QStringLiteral("Global"));
		auto* localization = new QComboBox;
		const QMap<QString, QString> langs = Localization::instance().available();
		for (auto it = langs.constBegin(); it != langs.constEnd(); ++it)
		{
			localization->addItem(it.key(), it.value());
			if (it.value() == p.localization)
				localization->setCurrentIndex(localization->count() - 1);
		}
		QObject::connect(localization, &QComboBox::currentIndexChanged, &dlg, [&, localization](int)
		{
			p.localization = localization->currentData().toString();
			changed(ApplyLocalization);
		});
		page->addWidget(group(QStringLiteral("Localization"), vbox({localization})));
		page->addWidget(group(QStringLiteral("Menu"), vbox({check(QStringLiteral("Hide (use Alt or F10 key to toggle)"), p.hideMenuBar),
		                                                     check(QStringLiteral("Hide right shortcuts ＋ ▼ ✕"), p.hideMenuRightShortcuts)})));
		page->addWidget(group(QStringLiteral("Status Bar"), vbox({checkInv(QStringLiteral("Hide"), p.statusbarVisible)})));

		auto* fontSize = new QComboBox;
		fontSize->addItem(QStringLiteral("Default"), 0);
		for (int sz = 7; sz <= 16; ++sz)
			fontSize->addItem(QString::number(sz), sz);
		fontSize->setCurrentIndex(std::max(0, fontSize->findData(p.dialogFontSize)));
		QObject::connect(fontSize, &QComboBox::currentIndexChanged, &dlg, [&, fontSize](int) { p.dialogFontSize = fontSize->currentData().toInt(); changed(ApplyNormal); });
		auto* fontName = new QComboBox;
		fontName->addItem(QStringLiteral("Default"), QString());
		for (const QString& fam : QFontDatabase::families())
			fontName->addItem(fam, fam);
		fontName->setCurrentIndex(std::max(0, fontName->findData(p.dialogFontName)));
		QObject::connect(fontName, &QComboBox::currentIndexChanged, &dlg, [&, fontName](int) { p.dialogFontName = fontName->currentData().toString(); changed(ApplyNormal); });
		auto* fontForm = new QGridLayout;
		fontForm->addWidget(new QLabel(QStringLiteral("Size:")), 0, 0, Qt::AlignRight);
		fontForm->addWidget(fontSize, 0, 1, Qt::AlignLeft);
		fontForm->addWidget(new QLabel(QStringLiteral("Name:")), 1, 0, Qt::AlignRight);
		fontForm->addWidget(fontName, 1, 1);
		page->addWidget(group(QStringLiteral("Dialog Font ") + needRestart, fontForm));
		page->addStretch(1);
	}

	// =====================================================================
	// Toolbar
	// =====================================================================
	{
		QVBoxLayout* page = addPage(QStringLiteral("Toolbar"), QStringLiteral("Toolbar"));
		page->addWidget(checkInv(QStringLiteral("Hide"), p.toolbarVisible));
		auto* sets = new QButtonGroup(&dlg);
		auto* setBox = vbox({radio(sets, QStringLiteral("Fluent UI: small"), TB_SMALL), radio(sets, QStringLiteral("Fluent UI: large"), TB_LARGE),
		                     radio(sets, QStringLiteral("Filled Fluent UI: small"), TB_SMALL2), radio(sets, QStringLiteral("Filled Fluent UI: large"), TB_LARGE2),
		                     radio(sets, QStringLiteral("Standard icons: small"), TB_STANDARD)});
		bindGroup(sets, p.toolbarIconSet);

		auto* coloring = new QButtonGroup(&dlg);
		QRadioButton* complete = radio(coloring, QStringLiteral("Complete"), 1);
		QRadioButton* partial = radio(coloring, QStringLiteral("Partial"), 0);
		bindBoolGroup(coloring, p.toolbarMono);

		auto* colors = new QButtonGroup(&dlg);
		auto* colorGrid = new QGridLayout;
		const std::pair<const char*, FluentColor> leftColors[] = {{"Red", FluentColor::red}, {"Green", FluentColor::green}, {"Blue", FluentColor::blue},
		                                                          {"Purple", FluentColor::purple}, {"Cyan", FluentColor::cyan}, {"Olive", FluentColor::olive},
		                                                          {"Yellow", FluentColor::yellow}};
		int row = 0;
		for (const auto& [name, fc] : leftColors)
			colorGrid->addWidget(radio(colors, QString::fromLatin1(name), static_cast<int>(fc)), row++, 0);
		colorGrid->addWidget(radio(colors, QStringLiteral("Default"), static_cast<int>(FluentColor::defaultColor)), 0, 1);
		QRadioButton* accent = radio(colors, QStringLiteral("System Accent"), static_cast<int>(FluentColor::accent));
		accent->setToolTip(QStringLiteral("The accent colour of the desktop theme"));
		colorGrid->addWidget(accent, 1, 1);
		QRadioButton* customRadio = radio(colors, QStringLiteral("Custom"), static_cast<int>(FluentColor::custom));
		auto* customColor = new QPushButton;
		customColor->setIcon(swatch(p.toolbarCustomColor ? toQColor(static_cast<COLORREF>(p.toolbarCustomColor)) : QColor()));
		customColor->setFixedWidth(48);
		colorGrid->addWidget(hboxWidget({customRadio, customColor}), 2, 1);
		bindGroup(colors, p.toolbarColor);
		QObject::connect(customColor, &QPushButton::clicked, &dlg, [&, customColor, customRadio]
		{
			const QColor c = QColorDialog::getColor(p.toolbarCustomColor ? toQColor(static_cast<COLORREF>(p.toolbarCustomColor)) : QColor(0, 0x80, 0), &dlg);
			if (!c.isValid())
				return;
			p.toolbarCustomColor = static_cast<int>(fromQColor(c));
			customColor->setIcon(swatch(c));
			customRadio->setChecked(true);
			p.toolbarColor = static_cast<int>(FluentColor::custom);
			changed(ApplyNormal);
		});
		auto* colorChoice = group(QStringLiteral("Color choice"), colorGrid);
		auto* colorization = group(QStringLiteral("Colorization"), vbox({complete, partial, colorChoice}));
		auto updateColorization = [sets, colorization] { colorization->setEnabled(sets->checkedId() != TB_STANDARD); };
		QObject::connect(sets, &QButtonGroup::idToggled, &dlg, updateColorization);
		updateColorization();

		auto* row1 = new QHBoxLayout;
		auto* setWidget = new QWidget;
		setWidget->setLayout(setBox);
		row1->addWidget(setWidget, 0, Qt::AlignTop);
		row1->addWidget(colorization, 1);
		page->addLayout(row1);
		page->addStretch(1);
	}

	// =====================================================================
	// Tab Bar
	// =====================================================================
	{
		QVBoxLayout* page = addPage(QStringLiteral("Tab Bar"), QStringLiteral("Tabbar"));
		QCheckBox* hide = checkInv(QStringLiteral("Hide"), p.tabbarVisible);
		page->addWidget(hide);
		QCheckBox* multiLine = check(QStringLiteral("Multi-line"), p.tabMultiLine);
		notOnLinux(multiLine, QStringLiteral("Not available on Linux: the Qt tab bar has a single row of tabs (scroll buttons are shown instead)"));
		auto* maxLen = spin(p.tabMaxLabelLen, 0, 99);
		maxLen->setToolTip(QStringLiteral("0: no compacting. Longer names are shortened with \"...\""));
		auto* behavior = group(QStringLiteral("Behavior"), vbox({check(QStringLiteral("Vertical"), p.tabVertical), multiLine,
			check(QStringLiteral("Lock (no drag and drop)"), p.tabLock), check(QStringLiteral("Double click to close document"), p.doubleClickTabCloses),
			check(QStringLiteral("Exit on close the last tab"), p.tabExitOnCloseLast),
			hboxWidget({new QLabel(QStringLiteral("Max. tab label length:")), maxLen})}));
		QCheckBox* pin = check(QStringLiteral("Enable pin tab feature"), p.tabPinButton);
		QCheckBox* onlyPinned = check(QStringLiteral("Show only pinned button"), p.tabOnlyPinnedButton);
		auto* look = group(QStringLiteral("Look && feel"), vbox({check(QStringLiteral("Reduce"), p.tabReduce), check(QStringLiteral("Alternate icons"), p.tabAltIcons),
			check(QStringLiteral("Change inactive tab color"), p.tabInactiveColor), check(QStringLiteral("Draw a coloured bar on active tab"), p.tabActiveBar),
			check(QStringLiteral("Show close button"), p.tabCloseButton), pin, onlyPinned,
			check(QStringLiteral("Show buttons on inactive tabs"), p.tabInactiveButtons)}));
		dependsOn(pin, {onlyPinned});
		auto* row = new QHBoxLayout;
		row->addWidget(behavior);
		row->addWidget(look);
		page->addLayout(row);
		dependsOn(hide, {behavior, look}, true);
		page->addStretch(1);
	}

	// =====================================================================
	// Editing 1
	// =====================================================================
	{
		QVBoxLayout* page = addPage(QStringLiteral("Editing 1"), QStringLiteral("Scintillas"));
		auto* lineModes = new QButtonGroup(&dlg);
		auto* frameValue = new QLabel;
		auto* frameWidth = slider(p.currentLineFrameWidth, 1, 6, 1, frameValue);
		QRadioButton* frame = radio(lineModes, QStringLiteral("Frame"), LINEHILITE_FRAME);
		auto* lineBox = vbox({radio(lineModes, QStringLiteral("None"), LINEHILITE_NONE), radio(lineModes, QStringLiteral("Highlight Background"), LINEHILITE_HILITE),
		                      frame, hboxWidget({new QLabel(QStringLiteral("Width:")), frameWidth, frameValue})});
		bindGroup(lineModes, p.currentLineMode);
		dependsOn(frame, {frameWidth, frameValue});

		auto* caretWidth = comboIndex({QStringLiteral("0"), QStringLiteral("1"), QStringLiteral("2"), QStringLiteral("3"), QStringLiteral("Block"), QStringLiteral("Block After")}, p.caretWidth);
		if (p.caretBlinkRate == 0)
			p.caretBlinkRate = 2500;
		auto* blink = slider(p.caretBlinkRate, 50, 2500, 50, nullptr);
		blink->setToolTip(QStringLiteral("Left: fast, right: slow"));
		auto* caretGrid = new QGridLayout;
		caretGrid->addWidget(new QLabel(QStringLiteral("Width:")), 0, 0, Qt::AlignRight);
		caretGrid->addWidget(caretWidth, 0, 1, 1, 3, Qt::AlignLeft);
		caretGrid->addWidget(new QLabel(QStringLiteral("Blink rate:")), 1, 0, Qt::AlignRight);
		caretGrid->addWidget(new QLabel(QStringLiteral("F")), 1, 1);
		caretGrid->addWidget(blink, 1, 2);
		caretGrid->addWidget(new QLabel(QStringLiteral("S")), 1, 3);

		auto* wraps = new QButtonGroup(&dlg);
		auto* wrapBox = vbox({radio(wraps, QStringLiteral("Default"), LINEWRAP_DEFAULT), radio(wraps, QStringLiteral("Aligned"), LINEWRAP_ALIGNED),
		                      radio(wraps, QStringLiteral("Indent"), LINEWRAP_INDENT)});
		bindGroup(wraps, p.lineWrapMethod);

		auto* leftCol = new QVBoxLayout;
		leftCol->addWidget(group(QStringLiteral("Current Line Indicator"), lineBox));
		leftCol->addWidget(group(QStringLiteral("Caret Settings"), caretGrid));
		leftCol->addWidget(group(QStringLiteral("Line Wrap"), wrapBox));
		leftCol->addStretch(1);

		QCheckBox* advScroll = check(QStringLiteral("Disable advanced scrolling feature due to touchpad issue"), p.disableAdvancedScrolling);
		notOnLinux(advScroll, QStringLiteral("Workaround for a Windows touchpad driver issue, not needed on Linux"));
		auto* rightCol = vbox({check(QStringLiteral("Enable smooth font"), p.smoothFont), check(QStringLiteral("Enable virtual space"), p.virtualSpace),
			check(QStringLiteral("Make current level folding/unfolding commands toggleable"), p.foldToggleable),
			check(QStringLiteral("Keep selection when right-click outside of selection"), p.rightClickKeepsSelection),
			check(QStringLiteral("Enable Copy/Cut Line without selection"), p.lineCopyCutWithoutSelection),
			check(QStringLiteral("Apply custom color to selected text foreground"), p.selectedTextForegroundSingleColor),
			check(QStringLiteral("Enable scrolling beyond last line"), p.scrollBeyondLastLine), advScroll,
			check(QStringLiteral("Disable selected text drag-and-drop"), p.disableSelectedTextDragDrop)});
		rightCol->addStretch(1);
		auto* row = new QHBoxLayout;
		row->addLayout(leftCol);
		row->addLayout(rightCol, 1);
		page->addLayout(row);
	}

	// =====================================================================
	// Editing 2
	// =====================================================================
	{
		QVBoxLayout* page = addPage(QStringLiteral("Editing 2"), QStringLiteral("Scintillas2"));
		QCheckBox* multi = check(QStringLiteral("Enable Multi-Editing (Ctrl+Mouse click/selection)"), p.multiEditing);
		QCheckBox* col2multi = check(QStringLiteral("Enable Column Selection to Multi-Editing"), p.columnSel2MultiEdit);
		dependsOn(multi, {col2multi});
		auto* multiBox = vbox({multi, indented(col2multi)});

		// EOL (CRLF): Default / Plain Text, Custom Color
		auto* eolGroup = new QButtonGroup(&dlg);
		QRadioButton* eolDefault = radio(eolGroup, QStringLiteral("Default"), 1);
		QRadioButton* eolPlain = radio(eolGroup, QStringLiteral("Plain Text"), 0);
		auto* eolCustom = new QCheckBox(QStringLiteral("Custom Color"));
		const bool rounded = (p.eolMode == CRLF_ROUNDEDRECT || p.eolMode == CRLF_ROUNDEDRECT_CUSTOM);
		(rounded ? eolDefault : eolPlain)->setChecked(true);
		eolCustom->setChecked(p.eolMode == CRLF_PLAINTEXT_CUSTOM || p.eolMode == CRLF_ROUNDEDRECT_CUSTOM);
		auto updateEol = [&, eolDefault, eolCustom]
		{
			p.eolMode = (eolCustom->isChecked() ? 2 : 0) + (eolDefault->isChecked() ? 1 : 0);
			changed(ApplyNormal);
		};
		QObject::connect(eolGroup, &QButtonGroup::idToggled, &dlg, [updateEol](int, bool on) { if (on) updateEol(); });
		QObject::connect(eolCustom, &QCheckBox::toggled, &dlg, updateEol);
		auto styleButton = [&dlg, mw]
		{
			auto* b = new QPushButton(QStringLiteral("..."));
			b->setFixedWidth(32);
			b->setToolTip(QStringLiteral("Set the colour in Style Configurator (Global Styles)"));
			QObject::connect(b, &QPushButton::clicked, &dlg, [mw] { NppDialogs::styleConfigurator(mw); });
			return b;
		};
		auto* eolBox = vbox({eolDefault, eolPlain, hboxWidget({eolCustom, styleButton()})});

		auto* npcGroup = new QButtonGroup(&dlg);
		auto* npcNote = new QPushButton(QStringLiteral("?"));
		npcNote->setFixedWidth(28);
		QObject::connect(npcNote, &QPushButton::clicked, &dlg, [&dlg]
		{
			QMessageBox::information(&dlg, QStringLiteral("Non-Printing Characters"),
				QStringLiteral("Abbreviation: name of the character (NBSP, ZWSP ...).\nCodepoint: Unicode code point (U+00A0 ...).\n\n"
				               "Shown with View > Show Symbol > Show Non-Printing Characters."));
		});
		auto* npcAppearance = vbox({hboxWidget({radio(npcGroup, QStringLiteral("Abbreviation"), NPC_ABBREVIATION), npcNote}),
		                            radio(npcGroup, QStringLiteral("Codepoint"), NPC_CODEPOINT),
		                            hboxWidget({check(QStringLiteral("Custom Color"), p.npcCustomColor), styleButton()})});
		bindGroup(npcGroup, p.npcMode);
		auto* npcBox = new QVBoxLayout;
		npcBox->addWidget(group(QStringLiteral("Appearance"), npcAppearance));
		npcBox->addWidget(check(QStringLiteral("Apply Appearance settings to C0, C1 && Unicode EOL"), p.npcIncludeCcUniEol));
		npcBox->addWidget(check(QStringLiteral("Prevent control character (C0 code) typing into document"), p.npcNoInputC0));

		auto* row = new QHBoxLayout;
		row->addWidget(group(QStringLiteral("Multi-Editing"), multiBox), 1);
		row->addWidget(group(QStringLiteral("EOL (CRLF)"), eolBox));
		page->addLayout(row);
		page->addWidget(group(QStringLiteral("Non-Printing Characters"), npcBox));
		page->addStretch(1);
	}

	// =====================================================================
	// Dark Mode
	// =====================================================================
	{
		QVBoxLayout* page = addPage(QStringLiteral("Dark Mode"), QStringLiteral("DarkMode"));
		auto* modes = new QButtonGroup(&dlg);
		page->addWidget(radio(modes, QStringLiteral("Light mode"), DARKMODE_LIGHT));
		page->addWidget(radio(modes, QStringLiteral("Dark mode"), DARKMODE_DARK));
		page->addWidget(radio(modes, QStringLiteral("Follow system"), DARKMODE_FOLLOW_SYSTEM));
		if (QAbstractButton* b = modes->button(p.darkMode))
			b->setChecked(true);

		auto* tones = new QButtonGroup(&dlg);
		auto* toneGrid = new QGridLayout;
		const char* toneNames[] = {"Black", "Red", "Green", "Blue", "Purple", "Cyan", "Olive"};
		for (int i = 0; i < 7; ++i)
			toneGrid->addWidget(radio(tones, QString::fromLatin1(toneNames[i]), i), i, 0);
		QRadioButton* customized = radio(tones, QStringLiteral("Customized"), 7);
		toneGrid->addWidget(customized, 0, 1, 1, 6);

		// NppDarkMode::Colors, in the order of the Windows dialog
		static const char* colorNames[] = {"Content background", "Hot track item", "Control background", "Dialog background", "Error", "Text",
		                                   "Darker text", "Disabled text", "Link", "Edge", "Edge highlight", "Edge disabled"};
		static const int colorIndex[] = {0, 2, 1, 3, 4, 5, 6, 7, 8, 9, 10, 11}; // into the Colors struct order
		static const QRgb defaults[] = {0x202020, 0x383838, 0x454545, 0x202020, 0xB00000, 0xE0E0E0, 0xC0C0C0, 0x808080, 0xFFFF00, 0x646464, 0x9B9B9B, 0x484848};
		auto customList = [&p]
		{
			QStringList parts = p.darkCustomColors.split(QLatin1Char(','), Qt::SkipEmptyParts);
			while (parts.size() < 12)
				parts << QString::number(defaults[parts.size()], 16);
			return parts;
		};
		auto* customBox = new QWidget;
		auto* customGrid = new QGridLayout(customBox);
		std::vector<QPushButton*> colorButtons;
		for (int i = 0; i < 12; ++i)
		{
			const int idx = colorIndex[i];
			auto* b = new QPushButton;
			b->setFixedWidth(40);
			b->setIcon(swatch(QColor(customList()[idx].toUInt(nullptr, 16))));
			QObject::connect(b, &QPushButton::clicked, &dlg, [&, b, idx, customList]
			{
				QStringList parts = customList();
				const QColor c = QColorDialog::getColor(QColor(parts[idx].toUInt(nullptr, 16)), &dlg);
				if (!c.isValid())
					return;
				parts[idx] = QString::number(c.rgb() & 0xFFFFFF, 16);
				p.darkCustomColors = parts.join(QLatin1Char(','));
				b->setIcon(swatch(c));
				changed(ApplyDarkMode);
			});
			colorButtons.push_back(b);
			customGrid->addWidget(b, i % 5, (i / 5) * 2);
			customGrid->addWidget(new QLabel(QString::fromLatin1(colorNames[i])), i % 5, (i / 5) * 2 + 1);
		}
		auto* reset = new QPushButton(QStringLiteral("Reset"));
		QObject::connect(reset, &QPushButton::clicked, &dlg, [&, colorButtons]
		{
			p.darkCustomColors.clear();
			for (int i = 0; i < 12; ++i)
				colorButtons[static_cast<size_t>(i)]->setIcon(swatch(QColor(defaults[colorIndex[i]])));
			changed(ApplyDarkMode);
		});
		customGrid->addWidget(reset, 4, 4, 1, 2, Qt::AlignRight);
		toneGrid->addWidget(customBox, 1, 1, 6, 6);
		auto* tonesBox = group(QStringLiteral("Tones"), toneGrid);
		page->addWidget(tonesBox);
		page->addStretch(1);

		bindGroup(tones, p.darkTone, ApplyDarkMode);
		auto updateTones = [&p, tonesBox, customBox, customized]
		{
			tonesBox->setEnabled(p.isDark());
			customBox->setEnabled(customized->isChecked());
		};
		QObject::connect(tones, &QButtonGroup::idToggled, &dlg, updateTones);
		QObject::connect(modes, &QButtonGroup::idToggled, &dlg, [&, updateTones](int id, bool on)
		{
			if (!on)
				return;
			p.darkMode = id;
			p.themeFile.clear(); // like Notepad++: the dark or light default theme follows the mode
			updateTones();
			changed(ApplyDarkMode);
		});
		updateTones();
	}

	// =====================================================================
	// Margins/Border/Edge
	// =====================================================================
	{
		QVBoxLayout* page = addPage(QStringLiteral("Margins/Border/Edge"), QStringLiteral("MarginsBorderEdge"));
		auto* folds = new QButtonGroup(&dlg);
		auto* foldBox = vbox({radio(folds, QStringLiteral("Simple"), FOLDER_STYLE_SIMPLE), radio(folds, QStringLiteral("Arrow"), FOLDER_STYLE_ARROW),
		                      radio(folds, QStringLiteral("Circle tree"), FOLDER_STYLE_CIRCLE), radio(folds, QStringLiteral("Box tree"), FOLDER_STYLE_BOX),
		                      radio(folds, QStringLiteral("None"), FOLDER_STYLE_NONE)});
		bindGroup(folds, p.folderStyle);

		auto* borderValue = new QLabel;
		auto* borderBox = vbox({hboxWidget({slider(p.borderWidth, 0, 30, 1, borderValue), borderValue}), checkInv(QStringLiteral("No edge"), p.showBorderEdge)});

		auto* edges = new QPlainTextEdit(p.edgeColumns);
		edges->setPlaceholderText(QStringLiteral("80 120"));
		edges->setToolTip(QStringLiteral("Add one or more column numbers, separated by spaces, to show vertical lines at these columns.\n"
		                                 "With a single number, \"Background mode\" colours the text beyond it instead."));
		edges->setMaximumHeight(70);
		QCheckBox* bgMode = check(QStringLiteral("Background mode"), p.edgeBgMode);
		auto updateBg = [&p, bgMode] { bgMode->setEnabled(p.edgeColumnList().size() == 1); };
		QObject::connect(edges, &QPlainTextEdit::textChanged, &dlg, [&, edges, updateBg]
		{
			p.edgeColumns = edges->toPlainText().simplified();
			updateBg();
			changed(ApplyNormal);
		});
		updateBg();
		auto* edgeBox = vbox({edges, bgMode});

		auto* historyBox = vbox({check(QStringLiteral("Show in the margin"), p.changeHistoryMargin), check(QStringLiteral("Show in the text"), p.changeHistoryIndicator)});

		QCheckBox* lineNumbers = check(QStringLiteral("Display"), p.showLineNumber);
		auto* widths = new QButtonGroup(&dlg);
		QRadioButton* dyn = radio(widths, QStringLiteral("Dynamic width"), 1);
		QRadioButton* cst = radio(widths, QStringLiteral("Constant width"), 0);
		bindBoolGroup(widths, p.lineNumberDynamicWidth);
		dependsOn(lineNumbers, {dyn, cst});
		auto* lineNumberBox = vbox({lineNumbers, dyn, cst});

		auto* padLeftValue = new QLabel;
		auto* padRightValue = new QLabel;
		auto* dfValue = new QLabel;
		auto* padGrid = new QGridLayout;
		padGrid->addWidget(new QLabel(QStringLiteral("Left")), 0, 0, Qt::AlignRight);
		padGrid->addWidget(slider(p.paddingLeft, 0, 9, 1, padLeftValue), 0, 1);
		padGrid->addWidget(padLeftValue, 0, 2);
		padGrid->addWidget(new QLabel(QStringLiteral("Right")), 1, 0, Qt::AlignRight);
		padGrid->addWidget(slider(p.paddingRight, 0, 9, 1, padRightValue), 1, 1);
		padGrid->addWidget(padRightValue, 1, 2);
		auto* dfSlider = slider(p.distractionFreeDivPart, 3, 9, 1, dfValue);
		dfSlider->setToolTip(QStringLiteral("Distraction Free Mode: the padding on each side is the editor width divided by this value"));
		padGrid->addWidget(new QLabel(QStringLiteral("Distraction Free")), 2, 0, Qt::AlignRight);
		padGrid->addWidget(dfSlider, 2, 1);
		padGrid->addWidget(dfValue, 2, 2);

		auto* grid = new QGridLayout;
		grid->addWidget(group(QStringLiteral("Fold Margin Style"), foldBox), 0, 0);
		grid->addWidget(group(QStringLiteral("Vertical Edge Settings"), edgeBox), 0, 1);
		grid->addWidget(group(QStringLiteral("Line Number"), lineNumberBox), 0, 2);
		grid->addWidget(group(QStringLiteral("Border Width"), borderBox), 1, 0);
		grid->addWidget(group(QStringLiteral("Change History"), historyBox), 1, 1);
		grid->addWidget(group(QStringLiteral("Padding"), padGrid), 1, 2);
		grid->addWidget(check(QStringLiteral("Display bookmark"), p.showBookmarkMargin), 2, 2);
		page->addLayout(grid);
		page->addStretch(1);
	}

	// =====================================================================
	// New Document
	// =====================================================================
	{
		QVBoxLayout* page = addPage(QStringLiteral("New Document"), QStringLiteral("NewDoc"));
		auto* eols = new QButtonGroup(&dlg);
		auto* eolBox = vbox({radio(eols, QStringLiteral("Windows (CR LF)"), SC_EOL_CRLF), radio(eols, QStringLiteral("Unix (LF)"), SC_EOL_LF),
		                     radio(eols, QStringLiteral("Macintosh (CR)"), SC_EOL_CR)});
		bindGroup(eols, p.newDocEol);

		constexpr int otherId = 100;
		auto* encs = new QButtonGroup(&dlg);
		QRadioButton* utf8 = radio(encs, QStringLiteral("UTF-8"), static_cast<int>(UniMode::uniCookie));
		QCheckBox* ansiAsUtf8 = check(QStringLiteral("Apply to opened ANSI files"), p.newDocOpenAnsiAsUtf8);
		QRadioButton* other = radio(encs, QString(), otherId);
		auto* cpCombo = new QComboBox;
		for (int i = 0;; ++i)
		{
			const int cp = EncodingMapper::getInstance().getEncodingFromIndex(i);
			if (cp == -1)
				break;
			if (cp > 0 && FileIO::isCodepageSupported(cp))
				cpCombo->addItem(FileIO::encodingName(UniMode::uni8Bit, cp), cp);
		}
		auto* encBox = vbox({radio(encs, QStringLiteral("ANSI"), static_cast<int>(UniMode::uni8Bit)), utf8, indented(ansiAsUtf8),
		                     radio(encs, QStringLiteral("UTF-8 with BOM"), static_cast<int>(UniMode::uniUTF8)),
		                     radio(encs, QStringLiteral("UTF-16 Big Endian with BOM"), static_cast<int>(UniMode::uni16BE)),
		                     radio(encs, QStringLiteral("UTF-16 Little Endian with BOM"), static_cast<int>(UniMode::uni16LE)), hboxWidget({other, cpCombo})});
		const bool isOther = (p.newDocUnicode == static_cast<int>(UniMode::uni8Bit) && p.newDocCodepage > 0);
		if (QAbstractButton* b = encs->button(isOther ? otherId : p.newDocUnicode))
			b->setChecked(true);
		if (isOther)
			cpCombo->setCurrentIndex(std::max(0, cpCombo->findData(p.newDocCodepage)));
		auto updateEnc = [&, encs, cpCombo]
		{
			const int id = encs->checkedId();
			if (id == otherId)
			{
				p.newDocUnicode = static_cast<int>(UniMode::uni8Bit);
				p.newDocCodepage = cpCombo->currentData().toInt();
			}
			else
			{
				p.newDocUnicode = id;
				p.newDocCodepage = -1;
			}
			cpCombo->setEnabled(id == otherId);
			changed(ApplyNormal);
		};
		QObject::connect(encs, &QButtonGroup::idToggled, &dlg, [updateEnc](int, bool on) { if (on) updateEnc(); });
		QObject::connect(cpCombo, &QComboBox::currentIndexChanged, &dlg, [encs, updateEnc](int) { if (encs->checkedId() == otherId) updateEnc(); });
		cpCombo->setEnabled(isOther);
		dependsOn(utf8, {ansiAsUtf8});

		auto* defLang = new QComboBox;
		for (int i = 0; i < StyleManager::langCount(); ++i)
		{
			const LangType lt = static_cast<LangType>(i);
			if (lt == L_SEARCHRESULT || lt == L_USER || lt == L_JS_EMBEDDED || lt == L_EXTERNAL)
				continue;
			defLang->addItem(QString::fromUtf8(StyleManager::langInfo(lt).shortName), i);
		}
		defLang->model()->sort(0);
		defLang->setCurrentIndex(std::max(0, defLang->findData(p.newDocLang)));
		QObject::connect(defLang, &QComboBox::currentIndexChanged, &dlg, [&, defLang](int) { p.newDocLang = defLang->currentData().toInt(); changed(ApplyNormal); });

		auto* row = new QHBoxLayout;
		auto* leftCol = new QVBoxLayout;
		leftCol->addWidget(group(QStringLiteral("Format (Line ending)"), eolBox));
		leftCol->addLayout(hbox({new QLabel(QStringLiteral("Default language:")), defLang}));
		leftCol->addStretch(1);
		row->addLayout(leftCol);
		row->addWidget(group(QStringLiteral("Encoding"), encBox), 1);
		auto* newDocBox = new QVBoxLayout;
		newDocBox->addLayout(row);
		newDocBox->addWidget(check(QStringLiteral("Always open a new document in addition at startup"), p.addNewDocOnStartup));
		newDocBox->addWidget(check(QStringLiteral("Use the first line of document as untitled tab name"), p.useContentAsTabName));
		page->addWidget(group(QStringLiteral("New Document"), newDocBox));
		page->addStretch(1);
	}

	// =====================================================================
	// Default Directory
	// =====================================================================
	{
		QVBoxLayout* page = addPage(QStringLiteral("Default Directory"), QStringLiteral("DefaultDir"));
		auto* dirs = new QButtonGroup(&dlg);
		QRadioButton* fixed = radio(dirs, QString(), DIR_FIXED);
		QLineEdit* path = line(p.openSaveDirPath);
		QPushButton* browse = browseDir(path, QStringLiteral("Default Open/Save file Directory"));
		auto* box = vbox({radio(dirs, QStringLiteral("Follow current document"), DIR_FOLLOW_CURRENT),
		                  radio(dirs, QStringLiteral("Remember last used directory"), DIR_REMEMBER_LAST), hboxWidget({fixed, path, browse})});
		path->setMinimumWidth(320);
		bindGroup(dirs, p.openSaveDir);
		dependsOn(fixed, {path, browse});
		page->addWidget(group(QStringLiteral("Default Open/Save file Directory"), box));
		page->addWidget(check(QStringLiteral("Open all files of folder instead of launching Folder as Workspace on folder dropping"), p.dropFolderOpensFiles));
		page->addStretch(1);
	}

	// =====================================================================
	// Recent Files History
	// =====================================================================
	{
		QVBoxLayout* page = addPage(QStringLiteral("Recent Files History"), QStringLiteral("RecentFilesHistory"));
		auto* displays = new QButtonGroup(&dlg);
		QRadioButton* customLen = radio(displays, QStringLiteral("Customize Maximum Length:"), RECENT_CUSTOM_LENGTH);
		QSpinBox* lenSpin = spin(p.recentCustomLength, 1, 259);
		auto* displayBox = vbox({check(QStringLiteral("In Submenu"), p.recentInSubmenu), radio(displays, QStringLiteral("Only File Name"), RECENT_FILENAME_ONLY),
		                         radio(displays, QStringLiteral("Full File Name Path"), RECENT_FULL_PATH), hboxWidget({customLen, lenSpin, new QLabel(QStringLiteral("(1 – 259)"))})});
		bindGroup(displays, p.recentDisplay);
		dependsOn(customLen, {lenSpin});
		auto* box = vbox({check(QStringLiteral("Don't check at launch time"), p.recentDontCheck),
		                  hboxWidget({new QLabel(QStringLiteral("Max. number of entries:")), spin(p.recentFilesMax, 0, 30, ApplyRecent), new QLabel(QStringLiteral("(0 – 30)"))}),
		                  group(QStringLiteral("Display"), displayBox)});
		page->addWidget(group(QStringLiteral("Recent Files History"), box));
		page->addStretch(1);
	}

	// =====================================================================
	// File Association
	// =====================================================================
	{
		QVBoxLayout* page = addPage(QStringLiteral("File Association"), QStringLiteral("FileAssoc"));
		page->addWidget(label(QStringLiteral("Make Notepad++ the default application of these file types for your user "
		                                     "(xdg-mime, ~/.config/mimeapps.list).")));
		auto* groupsList = new QListWidget;
		for (const ExtGroup& g : extGroups())
			groupsList->addItem(QString::fromLatin1(g.name));
		auto* extList = new QListWidget;
		auto* customExt = new QLineEdit;
		customExt->setPlaceholderText(QStringLiteral(".ext"));
		customExt->setVisible(false);
		auto* registered = new QListWidget;
		auto* add = new QPushButton(QStringLiteral("->"));
		auto* remove = new QPushButton(QStringLiteral("<-"));
		auto refreshRegistered = [registered]
		{
			registered->clear();
			static QMimeDatabase db;
			for (const QString& mime : registeredMimes())
			{
				const QMimeType t = db.mimeTypeForName(mime);
				const QString suffix = t.isValid() ? t.preferredSuffix() : QString();
				auto* item = new QListWidgetItem(suffix.isEmpty() ? mime : QStringLiteral(".%1  (%2)").arg(suffix, mime), registered);
				item->setData(Qt::UserRole, mime);
			}
		};
		QObject::connect(groupsList, &QListWidget::currentRowChanged, &dlg, [extList, customExt](int row)
		{
			extList->clear();
			if (row < 0)
				return;
			const ExtGroup& g = extGroups()[static_cast<size_t>(row)];
			extList->addItems(g.exts);
			customExt->setVisible(g.exts.isEmpty());
		});
		QObject::connect(add, &QPushButton::clicked, &dlg, [&dlg, extList, customExt, refreshRegistered]
		{
			QString ext = customExt->isVisible() ? customExt->text().trimmed() : (extList->currentItem() ? extList->currentItem()->text() : QString());
			if (ext.isEmpty())
				return;
			if (!ext.startsWith(QLatin1Char('.')))
				ext.prepend(QLatin1Char('.'));
			QString error;
			if (!registerExt(ext, &error))
				QMessageBox::warning(&dlg, QStringLiteral("File Association"), error);
			refreshRegistered();
		});
		QObject::connect(remove, &QPushButton::clicked, &dlg, [registered, refreshRegistered]
		{
			if (QListWidgetItem* item = registered->currentItem())
				unregisterMime(item->data(Qt::UserRole).toString());
			refreshRegistered();
		});
		auto* buttons = new QVBoxLayout;
		buttons->addStretch(1);
		buttons->addWidget(add);
		buttons->addWidget(remove);
		buttons->addStretch(1);
		auto* row = new QHBoxLayout;
		row->addWidget(group(QStringLiteral("Supported exts :"), vbox({groupsList})));
		auto* extBox = vbox({extList, customExt});
		row->addWidget(group(QString(), extBox));
		row->addLayout(buttons);
		row->addWidget(group(QStringLiteral("Registered exts :"), vbox({registered})));
		page->addLayout(row, 1);
		if (QStandardPaths::findExecutable(QStringLiteral("xdg-mime")).isEmpty())
		{
			page->addWidget(label(QStringLiteral("xdg-mime is not installed (package xdg-utils): file association is not available.")));
			add->setEnabled(false);
			remove->setEnabled(false);
		}
		refreshRegistered();
		groupsList->setCurrentRow(0);
	}

	// =====================================================================
	// Language
	// =====================================================================
	{
		QVBoxLayout* page = addPage(QStringLiteral("Language"), QStringLiteral("Language"));
		auto* available = new QListWidget;
		auto* disabledList = new QListWidget;
		available->setObjectName(QStringLiteral("langAvailable"));
		disabledList->setObjectName(QStringLiteral("langDisabled"));
		const QStringList disabled = p.disabledLangList();
		struct LangItem { QString display; QString name; };
		std::vector<LangItem> items;
		for (int i = 0; i < StyleManager::langCount(); ++i)
		{
			const LangType lt = static_cast<LangType>(i);
			if (lt == L_TEXT || lt == L_SEARCHRESULT || lt == L_USER || lt == L_JS_EMBEDDED || lt == L_EXTERNAL)
				continue;
			if (StyleManager::instance().menuIdFromLang(lt) <= 0)
				continue;
			const LanguageNameInfo& li = StyleManager::langInfo(lt);
			items.push_back({QString::fromUtf8(li.shortName), QString::fromUtf8(li.langName)});
		}
		std::sort(items.begin(), items.end(), [](const LangItem& a, const LangItem& b) { return a.display.compare(b.display, Qt::CaseInsensitive) < 0; });
		for (const LangItem& it : items)
		{
			auto* item = new QListWidgetItem(it.display, disabled.contains(it.name) ? disabledList : available);
			item->setData(Qt::UserRole, it.name);
		}
		auto move = [&p, changed, disabledList](QListWidget* from, QListWidget* to)
		{
			QListWidgetItem* item = from->takeItem(from->currentRow());
			if (!item)
				return;
			to->addItem(item);
			to->sortItems();
			QStringList names;
			for (int i = 0; i < disabledList->count(); ++i)
				names << disabledList->item(i)->data(Qt::UserRole).toString();
			p.disabledLangs = names.join(QLatin1Char(' '));
			changed(ApplyNormal);
		};
		auto* toDisabled = new QPushButton(QStringLiteral("->"));
		auto* toAvailable = new QPushButton(QStringLiteral("<-"));
		QObject::connect(toDisabled, &QPushButton::clicked, &dlg, [move, available, disabledList] { move(available, disabledList); });
		QObject::connect(toAvailable, &QPushButton::clicked, &dlg, [move, available, disabledList] { move(disabledList, available); });
		auto* buttons = new QVBoxLayout;
		buttons->addStretch(1);
		buttons->addWidget(toDisabled);
		buttons->addWidget(toAvailable);
		buttons->addStretch(1);
		auto* lists = new QGridLayout;
		lists->addWidget(new QLabel(QStringLiteral("Available items")), 0, 0, Qt::AlignHCenter);
		lists->addWidget(new QLabel(QStringLiteral("Disabled items")), 0, 2, Qt::AlignHCenter);
		lists->addWidget(available, 1, 0);
		lists->addLayout(buttons, 1, 1);
		lists->addWidget(disabledList, 1, 2);
		auto* menuBox = new QVBoxLayout;
		menuBox->addWidget(check(QStringLiteral("Make language menu compact"), p.langMenuCompact));
		menuBox->addLayout(lists);
		page->addWidget(group(QStringLiteral("Language Menu"), menuBox), 1);
		page->addWidget(check(QStringLiteral("Treat backslash as escape character for SQL"), p.backslashIsEscapeForSql));
	}

	// =====================================================================
	// Indentation
	// =====================================================================
	{
		QVBoxLayout* page = addPage(QStringLiteral("Indentation"), QStringLiteral("Indentation"));
		auto* langList = new QListWidget;
		langList->addItem(QStringLiteral("[Default]"));
		struct LangItem { QString display; QString name; int lang; };
		std::vector<LangItem> items;
		for (int i = 0; i < StyleManager::langCount(); ++i)
		{
			const LangType lt = static_cast<LangType>(i);
			if (lt == L_SEARCHRESULT || lt == L_USER || lt == L_JS_EMBEDDED || lt == L_EXTERNAL)
				continue;
			const LanguageNameInfo& li = StyleManager::langInfo(lt);
			items.push_back({QString::fromUtf8(li.shortName), QString::fromUtf8(li.langName), i});
		}
		std::sort(items.begin(), items.end(), [](const LangItem& a, const LangItem& b) { return a.display.compare(b.display, Qt::CaseInsensitive) < 0; });
		for (const LangItem& it : items)
		{
			auto* item = new QListWidgetItem(it.display, langList);
			item->setData(Qt::UserRole, it.name);
			item->setData(Qt::UserRole + 1, it.lang);
		}
		auto* useDefault = new QCheckBox(QStringLiteral("Use default value"));
		auto* size = new QSpinBox;
		size->setRange(1, 64);
		auto* using_ = new QButtonGroup(&dlg);
		QRadioButton* tabChar = radio(using_, QStringLiteral("Tab character"), 0);
		QRadioButton* spaces = radio(using_, QStringLiteral("Space character(s)"), 1);
		auto* bsUnindent = new QCheckBox(QStringLiteral("Backspace key unindents instead of removing single space"));
		auto* valueBox = new QGroupBox;
		auto* valueGrid = new QGridLayout(valueBox);
		valueGrid->addWidget(useDefault, 0, 0, 1, 2);
		valueGrid->addWidget(new QLabel(QStringLiteral("Indent size:")), 1, 0, Qt::AlignRight);
		valueGrid->addWidget(size, 1, 1, Qt::AlignLeft);
		valueGrid->addWidget(new QLabel(QStringLiteral("Indent using:")), 2, 0, 1, 2);
		valueGrid->addWidget(tabChar, 3, 0, 1, 2);
		valueGrid->addWidget(spaces, 4, 0, 1, 2);
		valueGrid->addWidget(bsUnindent, 5, 0, 1, 2);

		auto loading = std::make_shared<bool>(false);
		auto load = [&, langList, useDefault, size, tabChar, spaces, bsUnindent, loading]
		{
			*loading = true;
			const int row = langList->currentRow();
			const bool isDefault = (row <= 0);
			useDefault->setVisible(!isDefault);
			int sz = p.tabSize;
			bool sp = p.replaceTabBySpace;
			bool bs = p.backspaceUnindent;
			bool def = false;
			if (!isDefault)
			{
				const QString name = langList->item(row)->data(Qt::UserRole).toString();
				LangTabSetting t;
				const LangDef* ld = StyleManager::instance().langDef(static_cast<LangType>(langList->item(row)->data(Qt::UserRole + 1).toInt()));
				if (p.langTab(name, &t))
				{
					def = (t.size <= 0);
					if (!def)
					{
						sz = t.size;
						sp = t.spaces;
						bs = t.backspaceUnindent;
					}
				}
				else if (ld && ld->tabSize > 0)
				{
					sz = ld->tabSize;
					sp = ld->tabReplacedBySpace;
					bs = ld->backspaceUnindent;
				}
				else
				{
					def = true;
				}
			}
			useDefault->setChecked(def);
			size->setValue(sz);
			(sp ? spaces : tabChar)->setChecked(true);
			bsUnindent->setChecked(bs);
			for (QWidget* w : std::initializer_list<QWidget*>{size, tabChar, spaces, bsUnindent})
				w->setEnabled(!def);
			*loading = false;
		};
		auto store = [&, langList, useDefault, size, spaces, bsUnindent, loading, load]
		{
			if (*loading)
				return;
			const int row = langList->currentRow();
			if (row <= 0)
			{
				p.tabSize = size->value();
				p.replaceTabBySpace = spaces->isChecked();
				p.backspaceUnindent = bsUnindent->isChecked();
			}
			else
			{
				auto tabs = p.langTabs();
				LangTabSetting t;
				t.size = useDefault->isChecked() ? 0 : size->value();
				t.spaces = spaces->isChecked();
				t.backspaceUnindent = bsUnindent->isChecked();
				tabs.insert(langList->item(row)->data(Qt::UserRole).toString(), t);
				p.setLangTabs(tabs);
			}
			load();
			changed(ApplyNormal);
		};
		QObject::connect(langList, &QListWidget::currentRowChanged, &dlg, load);
		QObject::connect(useDefault, &QCheckBox::toggled, &dlg, store);
		QObject::connect(size, &QSpinBox::valueChanged, &dlg, store);
		QObject::connect(using_, &QButtonGroup::idToggled, &dlg, [store](int, bool on) { if (on) store(); });
		QObject::connect(bsUnindent, &QCheckBox::toggled, &dlg, store);
		langList->setCurrentRow(0);

		auto* autoIndents = new QButtonGroup(&dlg);
		auto* autoBox = vbox({radio(autoIndents, QStringLiteral("None"), AUTOINDENT_NONE), radio(autoIndents, QStringLiteral("Basic"), AUTOINDENT_BASIC),
		                      radio(autoIndents, QStringLiteral("Advanced"), AUTOINDENT_ADVANCED)});
		bindGroup(autoIndents, p.autoIndentMode);

		auto* indentBox = new QVBoxLayout;
		indentBox->addWidget(langList);
		indentBox->addWidget(valueBox);
		auto* row = new QHBoxLayout;
		row->addWidget(group(QStringLiteral("Indent Settings"), indentBox), 1);
		auto* autoGroup = group(QStringLiteral("Auto-indent"), autoBox);
		row->addWidget(autoGroup, 0, Qt::AlignTop);
		page->addLayout(row, 1);
	}

	// =====================================================================
	// Highlighting
	// =====================================================================
	{
		QVBoxLayout* page = addPage(QStringLiteral("Highlighting"), QStringLiteral("Highlighting"));
		auto* markBox = vbox({check(QStringLiteral("Match case"), p.markAllMatchCase), check(QStringLiteral("Match whole word only"), p.markAllWholeWord)});
		QCheckBox* tags = check(QStringLiteral("Enable"), p.tagMatchHilite);
		QCheckBox* tagAttr = check(QStringLiteral("Highlight tag attributes"), p.tagAttrHilite);
		QCheckBox* tagZone = check(QStringLiteral("Highlight comment/php/asp zone"), p.hiliteNonHtmlZone);
		dependsOn(tags, {tagAttr, tagZone});
		auto* tagBox = vbox({tags, tagAttr, tagZone});

		QCheckBox* smart = check(QStringLiteral("Enable"), p.smartHighlight);
		QCheckBox* another = check(QStringLiteral("Highlight another view"), p.smartHighlightAnotherView);
		QCheckBox* smartCase = check(QStringLiteral("Match case"), p.smartHighlightMatchCase);
		QCheckBox* smartWord = check(QStringLiteral("Match whole word only"), p.smartHighlightWholeWord);
		QCheckBox* useFind = check(QStringLiteral("Use Find dialog settings"), p.smartHighlightUseFindSettings);
		auto* matching = group(QStringLiteral("Matching"), vbox({smartCase, smartWord, useFind}));
		dependsOn(useFind, {smartCase, smartWord}, true);
		dependsOn(smart, {matching});
		auto* smartBox = vbox({smart, another, matching});

		auto* leftCol = new QVBoxLayout;
		leftCol->addWidget(group(QStringLiteral("Style All Occurrences of Token"), markBox));
		leftCol->addWidget(group(QStringLiteral("Highlight Matching Tags"), tagBox));
		leftCol->addStretch(1);
		auto* row = new QHBoxLayout;
		row->addLayout(leftCol);
		row->addWidget(group(QStringLiteral("Smart Highlighting"), smartBox), 0, Qt::AlignTop);
		page->addLayout(row);
		page->addStretch(1);
	}

	// =====================================================================
	// Print
	// =====================================================================
	{
		QVBoxLayout* page = addPage(QStringLiteral("Print"), QStringLiteral("Print"));
		auto* colorModes = new QButtonGroup(&dlg);
		auto* colorBox = vbox({radio(colorModes, QStringLiteral("WYSIWYG"), SC_PRINT_NORMAL), radio(colorModes, QStringLiteral("Invert"), SC_PRINT_INVERTLIGHT),
		                       radio(colorModes, QStringLiteral("Black on white"), SC_PRINT_BLACKONWHITE),
		                       radio(colorModes, QStringLiteral("No background color"), SC_PRINT_COLOURONWHITE)});
		bindGroup(colorModes, p.printColorMode);
		auto* marginGrid = new QGridLayout;
		marginGrid->addWidget(new QLabel(QStringLiteral("Top")), 0, 1, Qt::AlignHCenter);
		marginGrid->addWidget(spin(p.printMarginTop, 0, 100), 1, 1);
		marginGrid->addWidget(new QLabel(QStringLiteral("Left")), 2, 0, Qt::AlignRight);
		marginGrid->addWidget(spin(p.printMarginLeft, 0, 100), 2, 1);
		marginGrid->addWidget(spin(p.printMarginRight, 0, 100), 2, 2);
		marginGrid->addWidget(new QLabel(QStringLiteral("Right")), 2, 3);
		marginGrid->addWidget(spin(p.printMarginBottom, 0, 100), 3, 1);
		marginGrid->addWidget(new QLabel(QStringLiteral("Bottom")), 4, 1, Qt::AlignHCenter);

		// header and footer
		auto* preview = new QLineEdit;
		preview->setReadOnly(true);
		auto lastEditPtr = std::make_shared<QLineEdit*>(nullptr); // header/footer field that gets "Add"
		const Buffer* cur = mw->currentBuffer();
		auto expandSample = [cur](QString t)
		{
			const QDateTime now = QDateTime::currentDateTime();
			t.replace(QStringLiteral("$(FULL_CURRENT_PATH)"), cur ? cur->fullName() : QString());
			t.replace(QStringLiteral("$(FILE_NAME)"), cur ? cur->displayName() : QString());
			t.replace(QStringLiteral("$(CURRENT_DIRECTORY)"), (cur && !cur->isUntitled()) ? QFileInfo(cur->path).absolutePath() : QString());
			t.replace(QStringLiteral("$(CURRENT_PRINTING_PAGE)"), QStringLiteral("1"));
			t.replace(QStringLiteral("$(SHORT_DATE)"), QLocale::system().toString(now.date(), QLocale::ShortFormat));
			t.replace(QStringLiteral("$(LONG_DATE)"), QLocale::system().toString(now.date(), QLocale::LongFormat));
			t.replace(QStringLiteral("$(TIME)"), QLocale::system().toString(now.time(), QLocale::ShortFormat));
			return t;
		};
		auto hfEdit = [&, preview, lastEditPtr, expandSample](QString& field, Qt::Alignment align)
		{
			QLineEdit* e = line(field);
			e->setAlignment(align);
			QObject::connect(e, &QLineEdit::textChanged, &dlg, [preview, expandSample](const QString& t) { preview->setText(expandSample(t)); });
			QObject::connect(e, &QLineEdit::cursorPositionChanged, &dlg, [e, lastEditPtr, preview, expandSample]
			{
				*lastEditPtr = e;
				preview->setText(expandSample(e->text()));
			});
			return e;
		};
		auto fontCombo = [&](QString& field)
		{
			auto* c = new QComboBox;
			c->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
			c->setMinimumContentsLength(14);
			c->addItem(QString(), QString());
			for (const QString& fam : QFontDatabase::families())
				c->addItem(fam, fam);
			c->setCurrentIndex(std::max(0, c->findData(field)));
			QObject::connect(c, &QComboBox::currentIndexChanged, &dlg, [&field, c, changed](int) { field = c->currentData().toString(); changed(ApplyNormal); });
			return c;
		};
		auto sizeCombo = [&](int& field)
		{
			auto* c = new QComboBox;
			c->addItem(QString(), 0);
			for (int sz : {5, 6, 7, 8, 9, 10, 11, 12, 14, 16, 18, 20, 22, 24, 26, 28, 36, 48, 72})
				c->addItem(QString::number(sz), sz);
			c->setCurrentIndex(std::max(0, c->findData(field)));
			QObject::connect(c, &QComboBox::currentIndexChanged, &dlg, [&field, c, changed](int) { field = c->currentData().toInt(); changed(ApplyNormal); });
			return c;
		};
		auto hfBox = [&](QString& l, QString& m, QString& r, QString& font, int& size, bool& bold, bool& italic)
		{
			auto* g = new QGridLayout;
			g->addWidget(new QLabel(QStringLiteral("Left part")), 0, 0, Qt::AlignHCenter);
			g->addWidget(new QLabel(QStringLiteral("Middle part")), 0, 1, Qt::AlignHCenter);
			g->addWidget(new QLabel(QStringLiteral("Right part")), 0, 2, Qt::AlignHCenter);
			g->addWidget(hfEdit(l, Qt::AlignLeft), 1, 0);
			g->addWidget(hfEdit(m, Qt::AlignHCenter), 1, 1);
			g->addWidget(hfEdit(r, Qt::AlignRight), 1, 2);
			g->addWidget(hboxWidget({fontCombo(font), sizeCombo(size), check(QStringLiteral("Bold"), bold), check(QStringLiteral("Italic"), italic)}), 2, 0, 1, 3);
			return g;
		};
		static const std::pair<const char*, const char*> vars[] = {
			{"Full file name path", "$(FULL_CURRENT_PATH)"}, {"File name", "$(FILE_NAME)"}, {"File directory", "$(CURRENT_DIRECTORY)"},
			{"Page", "$(CURRENT_PRINTING_PAGE)"}, {"Short date format", "$(SHORT_DATE)"}, {"Long date format", "$(LONG_DATE)"}, {"Time", "$(TIME)"}};
		auto* varCombo = new QComboBox;
		for (const auto& [name, var] : vars)
			varCombo->addItem(QString::fromLatin1(name), QString::fromLatin1(var));
		auto* addVar = new QPushButton(QStringLiteral("Add"));
		QObject::connect(addVar, &QPushButton::clicked, &dlg, [varCombo, lastEditPtr]
		{
			if (QLineEdit* e = *lastEditPtr)
			{
				e->insert(varCombo->currentData().toString());
				e->setFocus();
			}
		});
		auto* hf = new QVBoxLayout;
		hf->addLayout(hbox({new QLabel(QStringLiteral("Variable:")), varCombo, addVar}));
		hf->addWidget(group(QStringLiteral("Header"), hfBox(p.printHeaderLeft, p.printHeaderMiddle, p.printHeaderRight, p.printHeaderFont, p.printHeaderSize,
		                                                     p.printHeaderBold, p.printHeaderItalic)));
		hf->addWidget(group(QStringLiteral("Footer"), hfBox(p.printFooterLeft, p.printFooterMiddle, p.printFooterRight, p.printFooterFont, p.printFooterSize,
		                                                     p.printFooterBold, p.printFooterItalic)));
		hf->addWidget(new QLabel(QStringLiteral("Here display your variable settings")));
		hf->addWidget(preview);

		auto* leftCol = new QVBoxLayout;
		leftCol->addWidget(group(QStringLiteral("Color Options"), colorBox));
		leftCol->addWidget(group(QStringLiteral("Margin Setting (Unit:mm)"), marginGrid));
		leftCol->addStretch(1);
		auto* row = new QHBoxLayout;
		row->addLayout(leftCol);
		row->addWidget(group(QStringLiteral("Header and Footer"), hf), 1);
		page->addLayout(row);
		page->addWidget(check(QStringLiteral("Print line number"), p.printLineNumber));
		page->addWidget(check(QStringLiteral("Print formfeed as page break"), p.printFormFeed));
		page->addStretch(1);
	}

	// =====================================================================
	// Searching
	// =====================================================================
	{
		QVBoxLayout* page = addPage(QStringLiteral("Searching"), QStringLiteral("Searching"));
		auto* thresh = spin(p.inSelectionThreshold, 0, 99999);
		thresh->setToolTip(QStringLiteral("Number of selected characters from which \"In selection\" is checked when the Find dialog opens (0: never)"));
		QCheckBox* fillSel = check(QStringLiteral("Fill Find Field with Selected Text"), p.fillFindFieldWithSelected);
		auto* maxFill = spin(p.fillFindWhatThreshold, 1, 9999);
		QCheckBox* selCaret = check(QStringLiteral("Select Word Under Caret when Nothing Selected"), p.fillFindFieldSelectCaret);
		auto* maxFillRow = hboxWidget({maxFill, new QLabel(QStringLiteral(": Max Characters to Auto-Fill Find Field from Selection"))});
		dependsOn(fillSel, {maxFillRow, selCaret});
		auto* invoked = vbox({hboxWidget({new QLabel(QStringLiteral("Minimum Size for Auto-Checking \"In selection\":")), thresh}),
		                      check(QStringLiteral("Fill Find in Files Directory Field Based On Active Document"), p.fillDirFromActiveDoc), fillSel, maxFillRow, selCaret});
		page->addWidget(group(QStringLiteral("When Find Dialog is Invoked"), invoked));
		page->addWidget(check(QStringLiteral("Use Monospaced font in Find dialog"), p.monospacedFindDlg));
		page->addWidget(check(QStringLiteral("Find dialog remains open after search that outputs to results window"), p.findDlgAlwaysVisible));
		page->addWidget(check(QStringLiteral("Search Result window: show only one entry per found line if possible"), p.finderOneEntryPerLine));
		page->addWidget(check(QStringLiteral("Find in Files: Prefer on-disk file content over open buffers"), p.fifPreferDisk));
		page->addStretch(1);
	}

	// =====================================================================
	// Backup
	// =====================================================================
	{
		QVBoxLayout* page = addPage(QStringLiteral("Backup"), QStringLiteral("Backup"));
		QCheckBox* remember = check(QStringLiteral("Remember current session for next launch"), p.rememberSession);
		QCheckBox* snapshot = check(QStringLiteral("Enable session snapshot and periodic backup"), p.sessionSnapshot);
		auto* interval = spin(p.snapshotIntervalSec, 1, 9999);
		auto* intervalRow = hboxWidget({new QLabel(QStringLiteral("Trigger backup on modification in every")), interval, new QLabel(QStringLiteral("seconds"))});
		auto* backupPath = new QLineEdit(Npp::backupDir());
		backupPath->setReadOnly(true);
		auto* pathRow = hboxWidget({new QLabel(QStringLiteral("Backup path:")), backupPath});
		backupPath->setMinimumWidth(360);
		dependsOn(remember, {snapshot});
		dependsOn(snapshot, {intervalRow, pathRow});
		auto* snapBox = vbox({remember, snapshot, intervalRow, pathRow, check(QStringLiteral("Remember inaccessible files from past session"), p.keepAbsentFilesInSession)});
		page->addWidget(group(QStringLiteral("Session snapshot and periodic backup"), snapBox));

		auto* modes = new QButtonGroup(&dlg);
		QRadioButton* none = radio(modes, QStringLiteral("None"), BACKUP_NONE);
		QCheckBox* customDir = check(QStringLiteral("Custom Backup Directory"), p.backupUseCustomDir);
		QLineEdit* dir = line(p.backupDir);
		QPushButton* browse = browseDir(dir, QStringLiteral("Custom Backup Directory"));
		auto* dirRow = hboxWidget({new QLabel(QStringLiteral("Directory:")), dir, browse});
		dir->setMinimumWidth(300);
		dependsOn(customDir, {dirRow});
		auto* customBox = group(QString(), vbox({customDir, dirRow}));
		dependsOn(none, {customBox}, true);
		auto* saveBox = vbox({none, radio(modes, QStringLiteral("Simple backup"), BACKUP_SIMPLE), radio(modes, QStringLiteral("Verbose backup"), BACKUP_VERBOSE), customBox});
		bindGroup(modes, p.backupMode);
		page->addWidget(group(QStringLiteral("Backup on save"), saveBox));
		page->addStretch(1);
	}

	// =====================================================================
	// Auto-Completion
	// =====================================================================
	{
		QVBoxLayout* page = addPage(QStringLiteral("Auto-Completion"), QStringLiteral("AutoCompletion"));
		QCheckBox* enable = check(QStringLiteral("Enable auto-completion on each input"), p.autoCompletion);
		auto* modes = new QButtonGroup(&dlg);
		QRadioButton* func = radio(modes, QStringLiteral("Function completion"), 1);
		QRadioButton* word = radio(modes, QStringLiteral("Word completion"), 2);
		QRadioButton* both = radio(modes, QStringLiteral("Function and word completion"), 3);
		bindGroup(modes, p.autoCompletionMode);
		auto* nValue = new QLabel;
		auto* fromN = slider(p.autoCompletionThreshold, 1, 9, 1, nValue);
		auto* fromRow = hboxWidget({new QLabel(QStringLiteral("From")), nValue, new QLabel(QStringLiteral("th character")), new QLabel(QStringLiteral("1")), fromN,
		                            new QLabel(QStringLiteral("9"))});
		dependsOn(enable, {func, word, both, fromRow});
		auto* insertBox = group(QStringLiteral("Insert Selection"), vbox({check(QStringLiteral("TAB"), p.autocInsertTab), check(QStringLiteral("ENTER"), p.autocInsertEnter)}));
		auto* acGrid = new QGridLayout;
		acGrid->addWidget(enable, 0, 0);
		acGrid->addWidget(fromRow, 0, 1);
		acGrid->addWidget(func, 1, 0);
		acGrid->addWidget(word, 2, 0);
		acGrid->addWidget(both, 3, 0);
		acGrid->addWidget(insertBox, 1, 1, 3, 1);
		acGrid->addWidget(check(QStringLiteral("Make auto-completion list brief"), p.autoCompletionBrief), 4, 0);
		acGrid->addWidget(check(QStringLiteral("Ignore numbers"), p.autocIgnoreNumbers), 4, 1);
		acGrid->addWidget(check(QStringLiteral("Function parameters hint on input"), p.funcParamsHint), 5, 0);
		page->addWidget(group(QStringLiteral("Auto-Completion"), acGrid));

		// Auto-Insert and matched pairs
		auto pairEdit = [&](int index, bool close)
		{
			auto* e = new QLineEdit;
			e->setMaxLength(1);
			e->setFixedWidth(32);
			e->setAlignment(Qt::AlignCenter);
			QString pairs = p.matchedPairs.leftJustified(6, QLatin1Char(' '));
			const QChar c = pairs.at(index * 2 + (close ? 1 : 0));
			if (c != QLatin1Char(' '))
				e->setText(c);
			QObject::connect(e, &QLineEdit::textChanged, &dlg, [&p, changed, index, close](const QString& t)
			{
				QString pairs = p.matchedPairs.leftJustified(6, QLatin1Char(' '));
				pairs[index * 2 + (close ? 1 : 0)] = t.isEmpty() ? QLatin1Char(' ') : t.at(0);
				p.matchedPairs = pairs.trimmed().isEmpty() ? QString() : pairs;
				changed(ApplyNormal);
			});
			return e;
		};
		auto* insGrid = new QGridLayout;
		insGrid->addWidget(check(QStringLiteral(" ("), p.autoPairParentheses), 1, 0);
		insGrid->addWidget(check(QStringLiteral(" ["), p.autoPairBrackets), 2, 0);
		insGrid->addWidget(check(QStringLiteral(" {"), p.autoPairBraces), 3, 0);
		insGrid->addWidget(check(QStringLiteral(" \""), p.autoPairDoubleQuotes), 1, 1);
		insGrid->addWidget(check(QStringLiteral(" '"), p.autoPairQuotes), 2, 1);
		insGrid->addWidget(check(QStringLiteral(" html/xml close tag"), p.autoPairHtmlXmlTag), 3, 1);
		insGrid->addWidget(new QLabel(QStringLiteral("Open")), 0, 3, Qt::AlignHCenter);
		insGrid->addWidget(new QLabel(QStringLiteral("Close")), 0, 4, Qt::AlignHCenter);
		for (int i = 0; i < 3; ++i)
		{
			insGrid->addWidget(new QLabel(QStringLiteral("Matched pair %1:").arg(i + 1)), i + 1, 2, Qt::AlignRight);
			insGrid->addWidget(pairEdit(i, false), i + 1, 3);
			insGrid->addWidget(pairEdit(i, true), i + 1, 4);
		}
		insGrid->setColumnStretch(5, 1);
		page->addWidget(group(QStringLiteral("Auto-Insert"), insGrid));
		page->addStretch(1);
	}

	// =====================================================================
	// Multi-Instance & Date
	// =====================================================================
	{
		QVBoxLayout* page = addPage(QStringLiteral("Multi-Instance & Date"), QStringLiteral("MultiInstance"));
		auto* modes = new QButtonGroup(&dlg);
		auto* instBox = vbox({radio(modes, QStringLiteral("Default (mono-instance)"), MONO_INSTANCE),
		                      radio(modes, QStringLiteral("Always in multi-instance mode"), MULTI_INSTANCE),
		                      radio(modes, QStringLiteral("Open session in a new instance (and save session automatically on exit)"), SESSION_IN_NEW_INSTANCE)});
		bindGroup(modes, p.multiInstMode);

		auto* result = new QLabel;
		QLineEdit* format = line(p.dateTimeFormat);
		auto updateResult = [result, format] { result->setText(QDateTime::currentDateTime().toString(Npp::windowsDateTimeFormat(format->text()))); };
		QObject::connect(format, &QLineEdit::textChanged, &dlg, updateResult);
		updateResult();
		auto* examples = new QGridLayout;
		examples->addWidget(new QLabel(QStringLiteral("yyyy-MM-dd HH:mm:ss\nH:m d/M/yyyy\nMMM d, yyyy  tt h:m")), 0, 0, Qt::AlignRight);
		examples->addWidget(new QLabel(QStringLiteral("1985-10-26 16:24:42\n16:24 26/10/1985\nOct 26, 1985  PM 4:24")), 0, 1);
		auto* dateBox = new QVBoxLayout;
		dateBox->addWidget(check(QStringLiteral("Reverse default date time order (short && long formats)"), p.dateTimeReverse));
		dateBox->addLayout(examples);
		dateBox->addLayout(hbox({new QLabel(QStringLiteral("Custom format:")), format}, false));
		dateBox->addWidget(result);

		auto noPanel = [&](QCheckBox* c) { notOnLinux(c, QStringLiteral("This panel is not available on Linux yet")); return c; };
		auto* panelBox = vbox({label(QStringLiteral("Remember panel state (panel is open) in other instances (multi-instance mode) or when using command line parameter [-nosession]")),
		                       check(QStringLiteral("Clipboard History"), p.keepStateClipboardHistory), check(QStringLiteral("Document List"), p.keepStateDocList),
		                       check(QStringLiteral("Character Panel"), p.keepStateCharPanel), check(QStringLiteral("Folder as Workspace"), p.keepStateFileBrowser),
		                       check(QStringLiteral("Project Panels"), p.keepStateProjectPanel), check(QStringLiteral("Document Map"), p.keepStateDocMap),
		                       check(QStringLiteral("Function List"), p.keepStateFuncList), noPanel(check(QStringLiteral("Plugin Panels"), p.keepStatePluginPanel))});

		auto* leftCol = new QVBoxLayout;
		leftCol->addWidget(group(QStringLiteral("Multi-instance settings *"), instBox));
		leftCol->addWidget(group(QStringLiteral("Customize insert Date Time"), dateBox));
		leftCol->addStretch(1);
		auto* row = new QHBoxLayout;
		row->addLayout(leftCol, 1);
		row->addWidget(group(QStringLiteral("Panel State and [-nosession] *"), panelBox), 0, Qt::AlignTop);
		page->addLayout(row);
		page->addWidget(new QLabel(QStringLiteral("* The modification of this setting needs to restart Notepad++")));
		page->addStretch(1);
	}

	// =====================================================================
	// Delimiter
	// =====================================================================
	{
		QVBoxLayout* page = addPage(QStringLiteral("Delimiter"), QStringLiteral("Delimiter"));
		auto* wc = new QButtonGroup(&dlg);
		QRadioButton* custom = radio(wc, QStringLiteral("Add your character as part of word\n(don't choose it unless you know what you're doing)"), 0);
		QLineEdit* chars = line(p.customWordChars);
		chars->setToolTip(QStringLiteral("This allows you to include additional character into current word characters while double clicking for selection or searching with \"Match whole word only\" option checked."));
		auto* warning = new QLabel;
		auto updateWarning = [chars, warning]
		{
			warning->setText(chars->text().contains(QLatin1Char(' ')) ? QStringLiteral("Be careful: your character list contains white space.") : QString());
		};
		QObject::connect(chars, &QLineEdit::textChanged, &dlg, updateWarning);
		updateWarning();
		auto* wcBox = vbox({radio(wc, QStringLiteral("Use default Word character list as it is"), 1), custom, chars, warning});
		bindBoolGroup(wc, p.wordCharsDefault);
		dependsOn(custom, {chars, warning});
		page->addWidget(group(QStringLiteral("Word character list"), wcBox));

		QLineEdit* open = line(p.delimiterOpen);
		QLineEdit* close = line(p.delimiterClose);
		for (QLineEdit* e : {open, close})
		{
			e->setMaxLength(1);
			e->setFixedWidth(32);
			e->setAlignment(Qt::AlignCenter);
		}
		auto* demo = new QGridLayout;
		demo->addWidget(new QLabel(QStringLiteral("Open")), 0, 0, Qt::AlignRight);
		demo->addWidget(open, 0, 1);
		demo->addWidget(new QLabel(QStringLiteral("bla bla bla bla bla bla")), 0, 2);
		demo->addWidget(close, 0, 3);
		demo->addWidget(new QLabel(QStringLiteral("Close")), 0, 4);
		demo->addWidget(new QLabel(QStringLiteral("bla bla bla bla bla bla bla bla bla bla bla bla")), 1, 1, 1, 3);
		demo->setColumnStretch(5, 1);
		auto* delimBox = new QVBoxLayout;
		delimBox->addLayout(demo);
		delimBox->addWidget(check(QStringLiteral("Allow on several lines"), p.delimiterSeveralLines));
		page->addWidget(group(QStringLiteral("Delimiter selection settings (Ctrl + Mouse double click)"), delimBox));
		page->addStretch(1);
	}

	// =====================================================================
	// Performance
	// =====================================================================
	{
		QVBoxLayout* page = addPage(QStringLiteral("Performance"), QStringLiteral("Performance"));
		QCheckBox* enable = check(QStringLiteral("Enable Large File Restriction (no syntax highlighting)"), p.largeFileRestriction);
		enable->setToolTip(QStringLiteral("Files of the defined size or bigger are opened without syntax highlighting and the features below. "
		                                  "The change applies to the files opened afterwards."));
		auto* sizeRow = hboxWidget({new QLabel(QStringLiteral("Define Large File Size:")), spin(p.largeFileSizeMB, 1, 2046), new QLabel(QStringLiteral("MB   (1 - 2046)"))});
		QCheckBox* wrap = check(QStringLiteral("Deactivate Word Wrap globally"), p.largeFileDeactivateWrap);
		QCheckBox* ac = check(QStringLiteral("Allow Auto-Completion"), p.largeFileAllowAutoCompletion);
		QCheckBox* sh = check(QStringLiteral("Allow Smart Highlighting"), p.largeFileAllowSmartHilite);
		QCheckBox* bm = check(QStringLiteral("Allow Brace Match"), p.largeFileAllowBraceMatch);
		QCheckBox* link = check(QStringLiteral("Allow URL Clickable Link"), p.largeFileAllowClickableLink);
		dependsOn(enable, {sizeRow, wrap, ac, sh, bm, link});
		auto* box = vbox({enable, sizeRow, wrap, ac, sh, bm, link, check(QStringLiteral("Suppress warning when opening ≥2GB files"), p.largeFileSuppress2GBWarning)});
		page->addWidget(group(QStringLiteral("Large File Restriction"), box));
		page->addStretch(1);
	}

	// =====================================================================
	// Cloud & Link
	// =====================================================================
	{
		QVBoxLayout* page = addPage(QStringLiteral("Cloud & Link"), QStringLiteral("Cloud"));
		QString cloudPath;
		{
			QFile f(Npp::cloudChoiceFile());
			if (f.open(QIODevice::ReadOnly))
				cloudPath = QString::fromUtf8(f.readAll()).trimmed();
		}
		auto* clouds = new QButtonGroup(&dlg);
		QRadioButton* noCloud = radio(clouds, QStringLiteral("No Cloud"), 0);
		QRadioButton* withCloud = radio(clouds, QStringLiteral("Set your cloud location path here:"), 1);
		auto* path = new QLineEdit(cloudPath);
		QPushButton* browse = browseDir(path, QStringLiteral("Select a folder from/to where Notepad++ reads/writes its settings"));
		auto* warning = new QLabel;
		warning->setWordWrap(true);
		(cloudPath.isEmpty() ? noCloud : withCloud)->setChecked(true);
		auto applyCloud = [&dlg, withCloud, path, warning]
		{
			const QString choice = Npp::cloudChoiceFile();
			if (!withCloud->isChecked() || path->text().trimmed().isEmpty())
			{
				QFile::remove(choice);
				warning->setText(withCloud->isChecked() ? QString() : QStringLiteral("Please restart Notepad++ to take effect."));
				return;
			}
			const QString dir = path->text().trimmed();
			if (!QFileInfo(dir).isDir() || !QFileInfo(dir).isWritable())
			{
				warning->setText(QStringLiteral("Invalid path: the folder must exist and be writable."));
				return;
			}
			QDir().mkpath(QFileInfo(choice).absolutePath());
			QFile f(choice);
			if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
				f.write(dir.toUtf8());
			// settings that are not there yet are copied, so that they are not lost
			for (const char* name : {"config.ini", "langs.xml", "stylers.xml"})
			{
				const QString src = Npp::configDir() + QLatin1Char('/') + QLatin1String(name);
				const QString dst = dir + QLatin1Char('/') + QLatin1String(name);
				if (QFileInfo::exists(src) && !QFileInfo::exists(dst))
					QFile::copy(src, dst);
			}
			warning->setText(QStringLiteral("Please restart Notepad++ to take effect."));
			Q_UNUSED(dlg);
		};
		QObject::connect(clouds, &QButtonGroup::idToggled, &dlg, [applyCloud](int, bool on) { if (on) applyCloud(); });
		QObject::connect(path, &QLineEdit::editingFinished, &dlg, applyCloud);
		QObject::connect(path, &QLineEdit::textChanged, &dlg, [withCloud, applyCloud](const QString& t) { if (withCloud->isChecked() && QFileInfo(t).isDir()) applyCloud(); });
		dependsOn(withCloud, {path, browse});
		page->addWidget(group(QStringLiteral("Settings on cloud"), vbox({noCloud, withCloud, hboxWidget({path, browse}), warning})));
		path->setMinimumWidth(320);

		QCheckBox* enable = check(QStringLiteral("Enable"), p.urlHighlight);
		QCheckBox* noUnderline = check(QStringLiteral("No underline"), p.urlNoUnderline);
		QCheckBox* fullBox = check(QStringLiteral("Enable fullbox mode"), p.urlFullBox);
		auto* schemes = new QPlainTextEdit(p.uriSchemes);
		schemes->setMaximumHeight(70);
		QObject::connect(schemes, &QPlainTextEdit::textChanged, &dlg, [&, schemes]
		{
			p.uriSchemes = schemes->toPlainText().simplified();
			changed(ApplyNormal);
		});
		dependsOn(enable, {noUnderline, fullBox});
		auto* linkGrid = new QGridLayout;
		linkGrid->addWidget(enable, 0, 0);
		linkGrid->addWidget(noUnderline, 0, 1);
		linkGrid->addWidget(fullBox, 1, 1);
		linkGrid->addWidget(new QLabel(QStringLiteral("URI customized schemes:")), 2, 0, 1, 2);
		linkGrid->addWidget(schemes, 3, 0, 1, 2);
		page->addWidget(group(QStringLiteral("Clickable Link Settings"), linkGrid));
		page->addStretch(1);
	}

	// =====================================================================
	// Search Engine
	// =====================================================================
	{
		QVBoxLayout* page = addPage(QStringLiteral("Search Engine"), QStringLiteral("SearchEngine"));
		auto* engines = new QButtonGroup(&dlg);
		QRadioButton* custom = radio(engines, QStringLiteral("Set your search engine here:"), SE_CUSTOM);
		QLineEdit* url = line(p.searchEngine);
		auto* box = vbox({radio(engines, QStringLiteral("DuckDuckGo"), SE_DUCKDUCKGO), radio(engines, QStringLiteral("Google"), SE_GOOGLE),
		                  radio(engines, QStringLiteral("Yahoo!"), SE_YAHOO), radio(engines, QStringLiteral("Stack Overflow"), SE_STACKOVERFLOW), custom, url,
		                  new QLabel(QStringLiteral("Example: https://www.google.com/search?q=$(CURRENT_WORD)"))});
		if (p.searchEngineChoice == SE_BING)
			p.searchEngineChoice = SE_DUCKDUCKGO;
		bindGroup(engines, p.searchEngineChoice);
		dependsOn(custom, {url});
		page->addWidget(group(QStringLiteral("Search Engine (for command \"Search on Internet\")"), box));
		page->addStretch(1);
	}

	// =====================================================================
	// MISC.
	// =====================================================================
	{
		QVBoxLayout* page = addPage(QStringLiteral("MISC."), QStringLiteral("MISC"));
		auto* detect = comboIndex({QStringLiteral("Enable for current file(s)"), QStringLiteral("Enable for all opened files"), QStringLiteral("Disable")}, p.fileAutoDetectMode);
		QCheckBox* silent = check(QStringLiteral("Update silently"), p.updateSilently);
		QCheckBox* toEnd = check(QStringLiteral("Scroll to the last line after update"), p.scrollToEndAfterUpdate);
		auto updateDetect = [detect, silent, toEnd]
		{
			const bool on = detect->currentIndex() != AUTODETECT_DISABLED;
			silent->setEnabled(on);
			toEnd->setEnabled(on);
		};
		QObject::connect(detect, &QComboBox::currentIndexChanged, &dlg, updateDetect);
		updateDetect();

		QCheckBox* switcher = check(QStringLiteral("Enable"), p.docSwitcher);
		QCheckBox* mru = check(QStringLiteral("Enable MRU behaviour"), p.docSwitcherMru);
		dependsOn(switcher, {mru});
		QCheckBox* peekMap = check(QStringLiteral("Peek on document map"), p.docPeekOnMap);

		auto* tray = comboIndex({QStringLiteral("No action on"), QStringLiteral("Minimize to"), QStringLiteral("Close to"), QStringLiteral("Minimize / Close to")}, p.systrayAction);
		auto* rendering = new QComboBox;
		rendering->addItem(QStringLiteral("Qt (default)"));
		notOnLinux(rendering, QStringLiteral("GDI and DirectWrite are Windows rendering engines; Scintilla draws with Qt on Linux"));

		auto* sessionExt = line(p.sessionFileExt);
		auto* workspaceExt = line(p.workspaceFileExt);
		for (QLineEdit* e : {sessionExt, workspaceExt})
			e->setFixedWidth(80);

		auto* leftCol = new QVBoxLayout;
		leftCol->addWidget(group(QStringLiteral("File Status Auto-Detection"), vbox({detect, silent, toEnd})));
		leftCol->addLayout(hbox({tray, new QLabel(QStringLiteral("system tray"))}));
		leftCol->addLayout(hbox({rendering, new QLabel(QStringLiteral("rendering mode"))}));
		auto* rightCol = new QVBoxLayout;
		rightCol->addWidget(group(QStringLiteral("Document Switcher (Ctrl+TAB)"), vbox({switcher, mru})));
		rightCol->addWidget(group(QStringLiteral("Document Peeker"), vbox({check(QStringLiteral("Peek on tab"), p.docPeekOnTab), peekMap})));
		auto* exts = new QGridLayout;
		exts->addWidget(new QLabel(QStringLiteral("Session file ext.:")), 0, 0, Qt::AlignRight);
		exts->addWidget(sessionExt, 0, 1);
		exts->addWidget(new QLabel(QStringLiteral("Workspace file ext.:")), 1, 0, Qt::AlignRight);
		exts->addWidget(workspaceExt, 1, 1);
		rightCol->addLayout(exts);
		rightCol->addStretch(1);
		auto* row = new QHBoxLayout;
		row->addLayout(leftCol, 1);
		row->addLayout(rightCol, 1);
		page->addLayout(row);
		page->addWidget(check(QStringLiteral("Mute all sounds"), p.muteSounds));
		page->addWidget(check(QStringLiteral("Autodetect character encoding"), p.detectEncoding));
		page->addWidget(check(QStringLiteral("Show only filename in title bar"), p.shortTitle));
		page->addWidget(check(QStringLiteral("Enable Save All confirm dialog"), p.saveAllConfirm));
		page->addWidget(check(QStringLiteral("Allow loading symlinks in Folder as Workspace panel"), p.fawSymlinks));
		page->addStretch(1);
	}

	// ---------------------------------------------------------------------
	QObject::connect(list, &QListWidget::currentRowChanged, stack, &QStackedWidget::setCurrentIndex);
	const QString lastPage = prefsSettings().value(QStringLiteral("prefsLastPage")).toString();
	int startRow = 0;
	for (int i = 0; i < list->count(); ++i)
	{
		if (list->item(i)->data(Qt::UserRole).toString() == lastPage)
			startRow = i;
	}
	list->setCurrentRow(startRow);

	auto* buttons = new QHBoxLayout;
	buttons->addStretch(1);
	auto* close = new QPushButton(QStringLiteral("Close"));
	close->setDefault(true);
	buttons->addWidget(close);
	buttons->addStretch(1);
	root->addLayout(buttons);
	QObject::connect(close, &QPushButton::clicked, &dlg, &QDialog::accept);

	dlg.exec();
	if (applyTimer.isActive() || pendingFlags)
		applyNow();
	if (list->currentItem())
		prefsSettings().setValue(QStringLiteral("prefsLastPage"), list->currentItem()->data(Qt::UserRole).toString());
}

} // namespace NppDialogs
