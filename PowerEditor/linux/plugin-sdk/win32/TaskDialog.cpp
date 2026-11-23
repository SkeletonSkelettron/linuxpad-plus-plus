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

// Notepad++ Linux plugin SDK, npp_win32 library: TaskDialog and TaskDialogIndirect as a Qt dialog
// (main instruction, content with links, custom and common buttons, radio buttons, verification
// check box, expanded information, footer, callback notifications).

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QStyle>
#include <QVBoxLayout>

#include "W32Internal.h"

using namespace w32;

namespace
{
	// Esc and the close button cancel only with a Cancel button or TDF_ALLOW_DIALOG_CANCELLATION
	class TaskQDialog : public QDialog
	{
	public:
		TaskQDialog(QWidget* parent, bool cancellable) : QDialog(parent), _cancellable(cancellable) {}
		void reject() override
		{
			if (_cancellable)
				QDialog::reject();
		}

	private:
		bool _cancellable;
	};

	// the text of a task dialog: <a href="...">...</a> links (TDF_ENABLE_HYPERLINKS), the rest plain
	QString taskText(LPCWSTR s, bool links, QStringList* hrefs)
	{
		if (!s || isIntResource(s))
			return QString();
		const QString text = qs(s);
		QString out;
		if (links)
		{
			static const QRegularExpression re(QStringLiteral("<a(\\s+[^>]*)?>(.*?)</a>"),
				QRegularExpression::CaseInsensitiveOption | QRegularExpression::DotMatchesEverythingOption);
			static const QRegularExpression hrefRe(QStringLiteral("href\\s*=\\s*\"([^\"]*)\""), QRegularExpression::CaseInsensitiveOption);
			qsizetype last = 0;
			auto it = re.globalMatch(text);
			while (it.hasNext())
			{
				const auto m = it.next();
				out += text.mid(last, m.capturedStart() - last).toHtmlEscaped();
				hrefs->append(hrefRe.match(m.captured(1)).captured(1));
				out += QStringLiteral("<a href=\"%1\">%2</a>").arg(hrefs->size() - 1).arg(m.captured(2).toHtmlEscaped());
				last = m.capturedEnd();
			}
			out += text.mid(last).toHtmlEscaped();
		}
		else
			out = text.toHtmlEscaped();
		return out.replace(QLatin1Char('\n'), QStringLiteral("<br>"));
	}

	QIcon taskIcon(LPCWSTR icon, HICON hicon, bool useHandle)
	{
		if (useHandle)
			return hicon ? iconOf(hicon) : QIcon();
		if (!icon)
			return QIcon();
		QStyle* style = QApplication::style();
		if (icon == TD_WARNING_ICON)
			return style->standardIcon(QStyle::SP_MessageBoxWarning);
		if (icon == TD_ERROR_ICON)
			return style->standardIcon(QStyle::SP_MessageBoxCritical);
		if (icon == TD_INFORMATION_ICON)
			return style->standardIcon(QStyle::SP_MessageBoxInformation);
		if (icon == TD_SHIELD_ICON)
			return style->standardIcon(QStyle::SP_VistaShield);
		return iconOf(LoadIconW(moduleHandle(), icon));
	}
}

HRESULT TaskDialogIndirect(const TASKDIALOGCONFIG* config, int* button, int* radioButton, BOOL* verificationChecked)
{
	if (!config)
		return E_INVALIDARG;
	const bool cancellable = (config->dwFlags & TDF_ALLOW_DIALOG_CANCELLATION) || (config->dwCommonButtons & TDCBF_CANCEL_BUTTON);
	TaskQDialog dlg(widgetOf(config->hwndParent), cancellable);
	dlg.setWindowTitle(config->pszWindowTitle && !isIntResource(config->pszWindowTitle) ? qs(config->pszWindowTitle) : QCoreApplication::applicationName());
	const bool links = (config->dwFlags & TDF_ENABLE_HYPERLINKS) != 0;
	QStringList hrefs;
	auto callback = [&](UINT msg, WPARAM wParam, LPARAM lParam) -> HRESULT
	{
		return config->pfCallback ? config->pfCallback(nullptr, msg, wParam, lParam, config->lpCallbackData) : S_OK;
	};

	auto* layout = new QVBoxLayout(&dlg);
	auto* top = new QHBoxLayout;
	layout->addLayout(top);
	const QIcon mainIcon = taskIcon(config->pszMainIcon, config->hMainIcon, (config->dwFlags & TDF_USE_HICON_MAIN) != 0);
	if (!mainIcon.isNull())
	{
		auto* icon = new QLabel(&dlg);
		icon->setPixmap(mainIcon.pixmap(32, 32));
		icon->setAlignment(Qt::AlignTop);
		top->addWidget(icon);
	}
	auto* texts = new QVBoxLayout;
	top->addLayout(texts, 1);
	auto addLabel = [&](LPCWSTR s, QBoxLayout* into, bool bold) -> QLabel*
	{
		const QString html = taskText(s, links, &hrefs);
		if (html.isEmpty())
			return nullptr;
		auto* label = new QLabel(bold ? QStringLiteral("<b>%1</b>").arg(html) : html, &dlg);
		label->setTextFormat(Qt::RichText);
		label->setWordWrap(true);
		label->setTextInteractionFlags(Qt::TextBrowserInteraction);
		label->setOpenExternalLinks(false);
		QObject::connect(label, &QLabel::linkActivated, &dlg, [&](const QString& link)
		{
			const std::wstring href = ws(hrefs.value(link.toInt()));
			callback(TDN_HYPERLINK_CLICKED, 0, reinterpret_cast<LPARAM>(href.c_str()));
		});
		if (bold)
		{
			QFont f = label->font();
			f.setPointSizeF(f.pointSizeF() * 1.2);
			label->setFont(f);
		}
		into->addWidget(label);
		return label;
	};
	addLabel(config->pszMainInstruction, texts, true);
	addLabel(config->pszContent, texts, false);

	// radio buttons
	QButtonGroup radios(&dlg);
	for (UINT i = 0; config->pRadioButtons && i < config->cRadioButtons; ++i)
	{
		auto* r = new QRadioButton(qs(config->pRadioButtons[i].pszButtonText).remove(QLatin1Char('&')), &dlg);
		radios.addButton(r, config->pRadioButtons[i].nButtonID);
		texts->addWidget(r);
		if (!(config->dwFlags & TDF_NO_DEFAULT_RADIO_BUTTON)
			&& (config->pRadioButtons[i].nButtonID == config->nDefaultRadioButton || (i == 0 && !config->nDefaultRadioButton)))
			r->setChecked(true);
	}
	QObject::connect(&radios, &QButtonGroup::idClicked, &dlg, [&](int id) { callback(TDN_RADIO_BUTTON_CLICKED, static_cast<WPARAM>(id), 0); });

	// expanded information, behind a button
	QLabel* expanded = nullptr;
	if (config->pszExpandedInformation)
	{
		const QString more = config->pszCollapsedControlText ? qs(config->pszCollapsedControlText) : QCoreApplication::translate("TaskDialog", "See details");
		const QString less = config->pszExpandedControlText ? qs(config->pszExpandedControlText) : QCoreApplication::translate("TaskDialog", "Hide details");
		auto* toggle = new QPushButton(&dlg);
		toggle->setFlat(true);
		texts->addWidget(toggle, 0, Qt::AlignLeft);
		expanded = addLabel(config->pszExpandedInformation, texts, false);
		const bool open = (config->dwFlags & TDF_EXPANDED_BY_DEFAULT) != 0;
		if (expanded)
			expanded->setVisible(open);
		toggle->setText(open ? less : more);
		QObject::connect(toggle, &QPushButton::clicked, &dlg, [=, &callback]
		{
			const bool show = expanded && !expanded->isVisible();
			if (expanded)
				expanded->setVisible(show);
			toggle->setText(show ? less : more);
			callback(TDN_EXPANDO_BUTTON_CLICKED, show, 0);
		});
	}

	// verification check box and buttons
	auto* bottom = new QHBoxLayout;
	layout->addLayout(bottom);
	QCheckBox* verification = nullptr;
	if (config->pszVerificationText)
	{
		verification = new QCheckBox(qs(config->pszVerificationText).remove(QLatin1Char('&')), &dlg);
		verification->setChecked((config->dwFlags & TDF_VERIFICATION_FLAG_CHECKED) != 0);
		QObject::connect(verification, &QCheckBox::toggled, &dlg, [&](bool on) { callback(TDN_VERIFICATION_CLICKED, on, 0); });
		bottom->addWidget(verification);
	}
	bottom->addStretch(1);
	int result = IDCANCEL;
	QPushButton* defaultButton = nullptr;
	auto addButton = [&](int id, const QString& text)
	{
		auto* b = new QPushButton(text, &dlg);
		bottom->addWidget(b);
		if (id == config->nDefaultButton || (!config->nDefaultButton && !defaultButton))
			defaultButton = b;
		QObject::connect(b, &QPushButton::clicked, &dlg, [&, id]
		{
			// S_FALSE from the callback: the dialog stays
			if (callback(TDN_BUTTON_CLICKED, static_cast<WPARAM>(id), 0) == S_FALSE)
				return;
			result = id;
			dlg.accept();
		});
	};
	for (UINT i = 0; config->pButtons && i < config->cButtons; ++i)
		addButton(config->pButtons[i].nButtonID, qs(config->pButtons[i].pszButtonText).section(QLatin1Char('\n'), 0, 0));
	const struct { int flag; int id; QDialogButtonBox::StandardButton standard; } common[] = {
		{TDCBF_OK_BUTTON, IDOK, QDialogButtonBox::Ok}, {TDCBF_YES_BUTTON, IDYES, QDialogButtonBox::Yes},
		{TDCBF_NO_BUTTON, IDNO, QDialogButtonBox::No}, {TDCBF_RETRY_BUTTON, IDRETRY, QDialogButtonBox::Retry},
		{TDCBF_CANCEL_BUTTON, IDCANCEL, QDialogButtonBox::Cancel}, {TDCBF_CLOSE_BUTTON, IDCLOSE, QDialogButtonBox::Close},
	};
	QDialogButtonBox names;
	for (const auto& c : common)
	{
		if (config->dwCommonButtons & c.flag)
			addButton(c.id, names.addButton(c.standard)->text());
	}
	if (!config->dwCommonButtons && !config->cButtons)
		addButton(IDOK, names.addButton(QDialogButtonBox::Ok)->text());
	if (defaultButton)
	{
		defaultButton->setDefault(true);
		defaultButton->setFocus();
	}

	// footer
	if (config->pszFooter)
	{
		auto* footer = new QHBoxLayout;
		layout->addLayout(footer);
		const QIcon footerIcon = taskIcon(config->pszFooterIcon, config->hFooterIcon, (config->dwFlags & TDF_USE_HICON_FOOTER) != 0);
		if (!footerIcon.isNull())
		{
			auto* icon = new QLabel(&dlg);
			icon->setPixmap(footerIcon.pixmap(16, 16));
			footer->addWidget(icon);
		}
		auto* f = new QVBoxLayout;
		footer->addLayout(f, 1);
		addLabel(config->pszFooter, f, false);
	}

	if (!cancellable)
		dlg.setWindowFlag(Qt::WindowCloseButtonHint, false);
	if (config->cxWidth)
		dlg.setMinimumWidth(static_cast<int>(config->cxWidth) * baseUnits(&dlg).width() / 4);

	callback(TDN_DIALOG_CONSTRUCTED, 0, 0);
	callback(TDN_CREATED, 0, 0);
	dlg.exec();
	callback(TDN_DESTROYED, 0, 0);
	if (button)
		*button = result;
	if (radioButton)
		*radioButton = radios.checkedId() == -1 ? 0 : radios.checkedId();
	if (verificationChecked)
		*verificationChecked = verification && verification->isChecked() ? TRUE : FALSE;
	return S_OK;
}

HRESULT TaskDialog(HWND parent, HINSTANCE instance, PCWSTR title, PCWSTR mainInstruction, PCWSTR content,
	TASKDIALOG_COMMON_BUTTON_FLAGS buttons, PCWSTR icon, int* button)
{
	TASKDIALOGCONFIG config{};
	config.cbSize = sizeof(config);
	config.hwndParent = parent;
	config.hInstance = instance;
	config.dwCommonButtons = buttons;
	config.pszWindowTitle = title;
	config.pszMainIcon = icon;
	config.pszMainInstruction = mainInstruction;
	config.pszContent = content;
	return TaskDialogIndirect(&config, button, nullptr, nullptr);
}
