// This file is part of the Linux port of XML Tools for Notepad++ (GPL v3, see LICENSE).
// The dialogs of XML Tools with Qt (the MFC ones of the Windows version: AboutBoxDlg.cpp, DebugDlg.cpp,
// HowtoUseDlg.cpp, InputDlg.cpp, MessageDlg.cpp, OptionsDlg.cpp, SelectFileDlg.cpp, XPathEvalDlg.cpp,
// XSLTransformDlg.cpp). The behavior is the one of these files.

#include "StdAfx.h"

#include "XMLTools.h"
#include "PluginInterface.h"
#include "Report.h"
#include "nppHelpers.h"
#include "nppMenu.h"

#include "AboutBoxDlg.h"
#include "DebugDlg.h"
#include "HowtoUseDlg.h"
#include "InputDlg.h"
#include "MessageDlg.h"
#include "OptionsDlg.h"
#include "SelectFileDlg.h"
#include "XPathEvalDlg.h"
#include "XSLTransformDlg.h"
#include "MSXMLWrapper.h"
#include "menuCmdID.h"

#include "NppPluginQt.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QTextCursor>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <libxml/xmlversion.h>

#include <fstream>
#include <functional>
#include <sstream>

extern NppData nppData;

namespace {
    QString qs(const CStringW& s) { return QString::fromWCharArray(s.GetString()); }
    CStringW cs(const QString& s) { return CStringW(s.toStdWString().c_str()); }

    // the filters of MFC ("XML Schema (*.xsd)|*.xsd|All files|*.*|") for QFileDialog
    QString qtFilter(const CStringW& mfc) {
        const QStringList parts = qs(mfc).split(QLatin1Char('|'), Qt::SkipEmptyParts);
        QStringList filters;
        for (int i = 0; i + 1 < parts.size(); i += 2) {
            QString patterns = parts[i + 1];
            patterns.replace(QLatin1Char(';'), QLatin1Char(' '));
            filters << parts[i].section(QLatin1Char('('), 0, 0).trimmed() + QStringLiteral(" (") + patterns + QLatin1Char(')');
        }
        return filters.join(QStringLiteral(";;"));
    }

    CStringW openFile(QWidget* parent, const CStringW& filters) {
        return cs(QFileDialog::getOpenFileName(parent, QString(), QString(), qtFilter(filters)));
    }

    QLineEdit* findEdit(QDialog* dlg, const char* name) { return dlg ? dlg->findChild<QLineEdit*>(QString::fromLatin1(name)) : nullptr; }

    std::string readAll(const CStringW& path) {
        std::ifstream t(npp_path(path.GetString()), std::ios::binary);
        std::stringstream ss;
        ss << t.rdbuf();
        return ss.str();
    }
}

// ---- base ------------------------------------------------------------------------------------------------
CLinuxDialog::~CLinuxDialog() {
    delete m_dialog;
}

QDialog* CLinuxDialog::ensure() {
    if (!m_dialog) {
        m_dialog = new QDialog(nppMainWindow());
        m_dialog->setAttribute(Qt::WA_DeleteOnClose, false);
        build(m_dialog);
    }
    return m_dialog;
}

INT_PTR CLinuxDialog::DoModal() {
    QDialog* dlg = ensure();
    for (;;) {
        if (dlg->exec() != QDialog::Accepted)
            return IDCANCEL;
        if (accepted())
            return IDOK;
    }
}

BOOL CLinuxDialog::Create(UINT, CWnd*) {
    ensure();
    return TRUE;
}

BOOL CLinuxDialog::ShowWindow(int cmd) {
    QDialog* dlg = ensure();
    const bool was = dlg->isVisible();
    if (cmd == SW_HIDE)
        dlg->hide();
    else {
        dlg->show();
        dlg->raise();
        dlg->activateWindow();
    }
    return was ? TRUE : FALSE;
}

// ---- message ----------------------------------------------------------------------------------------------
CMessageDlg::CMessageDlg(CWnd*) {}

void CMessageDlg::build(QDialog* dlg) {
    dlg->setWindowTitle(QStringLiteral("XML Tools"));
    auto* layout = new QVBoxLayout(dlg);
    auto* text = new QPlainTextEdit(dlg);
    text->setObjectName(QStringLiteral("message"));
    text->setReadOnly(true);
    text->setPlainText(qs(m_sMessage));
    layout->addWidget(text);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok, dlg);
    QObject::connect(buttons, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
    layout->addWidget(buttons);
    dlg->resize(560, 320);
}

// ---- input ------------------------------------------------------------------------------------------------
CInputDlg::CInputDlg(CWnd*) {}

void CInputDlg::build(QDialog* dlg) {
    dlg->setWindowTitle(qs(m_sInputTitle));
    auto* layout = new QVBoxLayout(dlg);
    layout->addWidget(new QLabel(qs(m_sInputCaption), dlg));
    auto* edit = new QLineEdit(qs(m_sInputBuffer), dlg);
    edit->setObjectName(QStringLiteral("input"));
    layout->addWidget(edit);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
    QObject::connect(buttons, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    layout->addWidget(buttons);
}

bool CInputDlg::accepted() {
    if (QLineEdit* e = findEdit(m_dialog, "input"))
        m_sInputBuffer = cs(e->text());
    return true;
}

// ---- schema of the validation ----------------------------------------------------------------------------
CSelectFileDlg::CSelectFileDlg(CWnd*) {
    m_sRootElemenSample = L"Please select a XML schema file";
}

CStringW CSelectFileDlg::ShowOpenFileDlg(CStringW filetypes) {
    return openFile(m_dialog, filetypes);
}

void CSelectFileDlg::build(QDialog* dlg) {
    dlg->setWindowTitle(QStringLiteral("Select file..."));
    auto* layout = new QGridLayout(dlg);
    layout->addWidget(new QLabel(QStringLiteral("The XML document has no schema reference (xsi:schemaLocation or xsi:noNamespaceSchemaLocation). "
        "Please select the XML schema to validate it with:"), dlg), 0, 0, 1, 3);
    qobject_cast<QLabel*>(layout->itemAtPosition(0, 0)->widget())->setWordWrap(true);
    layout->addWidget(new QLabel(QStringLiteral("XML Schema:"), dlg), 1, 0);
    auto* file = new QLineEdit(qs(m_sSelectedFilename), dlg);
    file->setObjectName(QStringLiteral("filename"));
    layout->addWidget(file, 1, 1);
    auto* browse = new QPushButton(QStringLiteral("..."), dlg);
    layout->addWidget(browse, 1, 2);
    layout->addWidget(new QLabel(QStringLiteral("Namespace:"), dlg), 2, 0);
    auto* ns = new QLineEdit(qs(m_sValidationNamespace), dlg);
    ns->setObjectName(QStringLiteral("namespace"));
    layout->addWidget(ns, 2, 1, 1, 2);
    layout->addWidget(new QLabel(QStringLiteral("To avoid this dialog, declare the schema in the root element, for instance:"), dlg), 3, 0, 1, 3);
    auto* sample = new QPlainTextEdit(qs(m_sRootElemenSample), dlg);
    sample->setObjectName(QStringLiteral("sample"));
    sample->setReadOnly(true);
    layout->addWidget(sample, 4, 0, 1, 3);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
    QObject::connect(buttons, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    layout->addWidget(buttons, 5, 0, 1, 3);
    QObject::connect(browse, &QPushButton::clicked, dlg, [this, file, ns, sample] {
        CStringW ret = ShowOpenFileDlg(L"XML Schema (*.xsd)|*.xsd|All files (*.*)|*.*|");
        if (ret.IsEmpty())
            return;
        if (m_sRootElementName.GetLength() > 0) {
            // the target namespace of the schema, and an example of root element that references it
            const std::string str = readAll(ret);
            XmlWrapperInterface* wrapper = new MSXMLWrapper(str.c_str(), str.size());
            std::vector<XPathResultEntryType> nodes = wrapper->xpathEvaluate(L"/xs:schema/@targetNamespace", L"xmlns:xs=\"http://www.w3.org/2001/XMLSchema\"");
            m_sRootElemenSample = L"<";
            m_sRootElemenSample += m_sRootElementName;
            m_sRootElemenSample += L"\r\n    xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\"";
            if (nodes.size() == 1) {
                ns->setText(QString::fromStdWString(nodes.at(0).value));
                m_sRootElemenSample += L"\r\n    xsi:schemaLocation=\"";
                m_sRootElemenSample += nodes.at(0).value.c_str();
                m_sRootElemenSample += L" ";
            }
            else {
                ns->clear();
                m_sRootElemenSample += L"\r\n    xsi:noNamespaceSchemaLocation=\"";
            }
            m_sRootElemenSample += ret;
            m_sRootElemenSample += L"\">";
            sample->setPlainText(qs(m_sRootElemenSample).replace(QStringLiteral("\r\n"), QStringLiteral("\n")));
            delete wrapper;
        }
        file->setText(qs(ret));
    });
    dlg->resize(560, 300);
}

bool CSelectFileDlg::accepted() {
    if (QLineEdit* e = findEdit(m_dialog, "filename"))
        m_sSelectedFilename = cs(e->text());
    if (QLineEdit* e = findEdit(m_dialog, "namespace"))
        m_sValidationNamespace = cs(e->text());
    return true;
}

// ---- XSL transformation -----------------------------------------------------------------------------------
CXSLTransformDlg::CXSLTransformDlg(CWnd*, unsigned long flags) : m_iFlags(flags) {}

HWND CXSLTransformDlg::getCurrentHScintilla(int which) {
    return (which == 0) ? nppData._scintillaMainHandle : nppData._scintillaSecondHandle;
}

CStringW CXSLTransformDlg::ShowOpenFileDlg(CStringW filetypes) {
    return openFile(m_dialog, filetypes);
}

void CXSLTransformDlg::build(QDialog* dlg) {
    dlg->setWindowTitle(QStringLiteral("XSL Transformation"));
    auto* layout = new QGridLayout(dlg);
    layout->addWidget(new QLabel(QStringLiteral("XSL file (or XML file when the current document is a stylesheet):"), dlg), 0, 0, 1, 3);
    auto* file = new QLineEdit(qs(m_sSelectedFile), dlg);
    file->setObjectName(QStringLiteral("xslfile"));
    layout->addWidget(file, 1, 0, 1, 2);
    auto* browse = new QPushButton(QStringLiteral("..."), dlg);
    layout->addWidget(browse, 1, 2);
    layout->addWidget(new QLabel(QStringLiteral("Parameters (name=value name2='value 2'):"), dlg), 2, 0, 1, 3);
    auto* options = new QLineEdit(qs(m_sXSLTOptions), dlg);
    options->setObjectName(QStringLiteral("xsltoptions"));
    layout->addWidget(options, 3, 0, 1, 3);
    auto* transform = new QPushButton(QStringLiteral("Transform"), dlg);
    transform->setObjectName(QStringLiteral("transform"));
    auto* close = new QPushButton(QStringLiteral("Close"), dlg);
    auto* row = new QHBoxLayout;
    row->addStretch(1);
    row->addWidget(transform);
    row->addWidget(close);
    layout->addLayout(row, 4, 0, 1, 3);
    QObject::connect(browse, &QPushButton::clicked, dlg, [this, file] {
        CStringW ret = ShowOpenFileDlg(L"XML/XSL Files|*.xml;*.xsl;*.xslt|All files|*.*|");
        if (ret.GetLength())
            file->setText(qs(ret));
    });
    QObject::connect(transform, &QPushButton::clicked, dlg, [this] { OnBtnTransform(); });
    QObject::connect(close, &QPushButton::clicked, dlg, &QDialog::hide);
    dlg->resize(520, 170);
}

void CXSLTransformDlg::OnBtnTransform() {
    if (QLineEdit* e = findEdit(m_dialog, "xslfile"))
        m_sSelectedFile = cs(e->text());
    if (QLineEdit* e = findEdit(m_dialog, "xsltoptions"))
        m_sXSLTOptions = cs(e->text());
    if (m_sSelectedFile.GetLength() <= 0) {
        Report::_printf_err(L"Cannot continue, missing parameters. Please select a file.");
        return;
    }
    int currentEdit;
    ::SendMessage(nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, (LPARAM)&currentEdit);
    HWND hCurrentEditView = getCurrentHScintilla(currentEdit);
    size_t currentLength = (size_t)::SendMessage(hCurrentEditView, SCI_GETLENGTH, 0, 0);
    std::string data(currentLength + 1, '\0');
    ::SendMessage(hCurrentEditView, SCI_GETTEXT, currentLength + 1, reinterpret_cast<LPARAM>(data.data()));
    XmlWrapperInterface* wrapper = new MSXMLWrapper(data.c_str(), currentLength);
    XSLTransformResultType res;
    if (wrapper->xslTransform(m_sSelectedFile.GetString(), &res, m_sXSLTOptions.GetString())) {
        ::SendMessage(nppData._nppHandle, NPPM_MENUCOMMAND, 0, IDM_FILE_NEW);
        ::SendMessage(nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, (LPARAM)&currentEdit);
        hCurrentEditView = getCurrentHScintilla(currentEdit);
        if (res.encoding != UniMode::uniEnd)
            Report::setEncoding(res.encoding, hCurrentEditView);
        ::SendMessage(hCurrentEditView, SCI_SETTEXT, 0, reinterpret_cast<LPARAM>(res.data.c_str()));
    }
    else {
        std::vector<ErrorEntryType> errors = wrapper->getLastErrors();
        displayXMLErrors(errors, hCurrentEditView, L"Error while XSL transformation");
    }
    delete wrapper;
}

// ---- XPath expression evaluation ---------------------------------------------------------------------------
CXPathEvalDlg::CXPathEvalDlg(CWnd*, unsigned long flags) : m_iFlags(flags) {}

HWND CXPathEvalDlg::getCurrentHScintilla(int which) {
    return (which == 0) ? nppData._scintillaMainHandle : nppData._scintillaSecondHandle;
}

void CXPathEvalDlg::build(QDialog* dlg) {
    dlg->setWindowTitle(QStringLiteral("XPath expression evaluation"));
    auto* layout = new QGridLayout(dlg);
    layout->addWidget(new QLabel(QStringLiteral("XPath expression:"), dlg), 0, 0);
    auto* expression = new QLineEdit(qs(m_sExpression), dlg);
    expression->setObjectName(QStringLiteral("expression"));
    layout->addWidget(expression, 0, 1);
    layout->addWidget(new QLabel(QStringLiteral("Namespace definition:"), dlg), 1, 0);
    auto* ns = new QLineEdit(qs(m_sNamespace), dlg);
    ns->setObjectName(QStringLiteral("namespace"));
    layout->addWidget(ns, 1, 1);
    auto* info = new QLabel(QStringLiteral("If your expression requires namespaces, please declare them in \"Namespace definition\" field the same way you "
        "declare namespaces in xml, for instance:\nxmlns:npp='http://notepad-plus-plus.org' xmlns:a='another-namespace'"), dlg);
    info->setWordWrap(true);
    layout->addWidget(info, 2, 1);
    auto* evaluate = new QPushButton(QStringLiteral("Evaluate"), dlg);
    evaluate->setObjectName(QStringLiteral("evaluate"));
    evaluate->setDefault(true);
    auto* copy = new QPushButton(QStringLiteral("Copy to clipboard"), dlg);
    auto* clear = new QPushButton(QStringLiteral("Clear list"), dlg);
    auto* row = new QHBoxLayout;
    row->addStretch(1);
    row->addWidget(evaluate);
    row->addWidget(copy);
    row->addWidget(clear);
    layout->addLayout(row, 3, 0, 1, 2);
    auto* results = new QTreeWidget(dlg);
    results->setObjectName(QStringLiteral("results"));
    results->setRootIsDecorated(false);
    results->setHeaderLabels({QStringLiteral("Type"), QStringLiteral("Name"), QStringLiteral("Value")});
    results->setColumnWidth(0, 100);
    results->setColumnWidth(1, 150);
    layout->addWidget(results, 4, 0, 1, 2);
    layout->setRowStretch(4, 1);
    QObject::connect(evaluate, &QPushButton::clicked, dlg, [this] { OnBtnEvaluate(); });
    QObject::connect(expression, &QLineEdit::returnPressed, dlg, [this] { OnBtnEvaluate(); });
    QObject::connect(copy, &QPushButton::clicked, dlg, [this] { OnBnClickedBtnCopy2clipboard(); });
    QObject::connect(clear, &QPushButton::clicked, dlg, [this] { OnBnClickedBtnClearlist(); });
    dlg->resize(680, 420);
}

void CXPathEvalDlg::OnBtnEvaluate() {
    if (QLineEdit* e = findEdit(m_dialog, "expression"))
        m_sExpression = cs(e->text());
    if (QLineEdit* e = findEdit(m_dialog, "namespace"))
        m_sNamespace = cs(e->text());
    m_sResult = L"";
    if (!m_sExpression.GetLength())
        Report::_printf_err(L"Empty expression; evaluation aborted.");
    else
        execute_xpath_expression(m_sExpression);
}

int CXPathEvalDlg::execute_xpath_expression(CStringW xpathExpr) {
    int currentEdit;
    ::SendMessage(nppData._nppHandle, NPPM_GETCURRENTSCINTILLA, 0, (LPARAM)&currentEdit);
    HWND hCurrentEditView = getCurrentHScintilla(currentEdit);
    size_t currentLength = (size_t)::SendMessage(hCurrentEditView, SCI_GETLENGTH, 0, 0);
    std::string data(currentLength + 1, '\0');
    ::SendMessage(hCurrentEditView, SCI_GETTEXT, currentLength + 1, reinterpret_cast<LPARAM>(data.data()));
    XmlWrapperInterface* wrapper = new MSXMLWrapper(data.c_str(), currentLength);
    std::vector<XPathResultEntryType> nodes = wrapper->xpathEvaluate(xpathExpr.GetString(), m_sNamespace.GetString());
    std::vector<ErrorEntryType> errors = wrapper->getLastErrors();
    if (errors.empty())
        print_xpath_nodes(nodes);
    else
        displayXMLErrors(errors, hCurrentEditView, L"Error: unable to parse XML", ERRORS_DISPLAY_MODE_ALERT);
    delete wrapper;
    return 0;
}

void CXPathEvalDlg::AddToList(CStringW type, CStringW name, CStringW value) {
    if (auto* results = m_dialog ? m_dialog->findChild<QTreeWidget*>(QStringLiteral("results")) : nullptr)
        results->addTopLevelItem(new QTreeWidgetItem({qs(type), qs(name), qs(value)}));
    m_sResult.AppendFormat(L"%s\t%s\t%s\n", type.GetString(), name.GetString(), value.GetString());
}

void CXPathEvalDlg::print_xpath_nodes(std::vector<XPathResultEntryType> nodes) {
    if (auto* results = m_dialog ? m_dialog->findChild<QTreeWidget*>(QStringLiteral("results")) : nullptr)
        results->clear();
    if (nodes.empty())
        AddToList(L"", L"No result", L"");
    for (const XPathResultEntryType& n : nodes)
        AddToList(n.type.c_str(), n.name.c_str(), n.value.c_str());
}

void CXPathEvalDlg::OnBnClickedBtnCopy2clipboard() {
    if (m_sResult.IsEmpty()) {
        QMessageBox::information(m_dialog, QStringLiteral("XML Tools"), QStringLiteral("Result is empty."));
        return;
    }
    QApplication::clipboard()->setText(qs(m_sResult));
    QMessageBox::information(m_dialog, QStringLiteral("XML Tools"), QStringLiteral("Result has been copied into clipboard."));
}

void CXPathEvalDlg::OnBnClickedBtnClearlist() {
    if (auto* results = m_dialog ? m_dialog->findChild<QTreeWidget*>(QStringLiteral("results")) : nullptr)
        results->clear();
    m_sResult.Empty();
}

// ---- options -------------------------------------------------------------------------------------------------
// a property list like the CMFCPropertyGridCtrl of Windows: groups, then a name and an editor per option
struct COptionsDlg::Fields {
    std::vector<std::function<void()>> apply; // the values of the editors into the options
    QSpinBox* annotationStyle = nullptr;
    QSpinBox* annotationHighlightStyle = nullptr;
};

COptionsDlg::COptionsDlg(CWnd*) {}

COptionsDlg::~COptionsDlg() {
    delete m_fields;
}

void COptionsDlg::build(QDialog* dlg) {
    delete m_fields;
    m_fields = new Fields;
    Fields* f = m_fields;
    dlg->setWindowTitle(QStringLiteral("XML Tools options"));
    auto* layout = new QVBoxLayout(dlg);
    auto* tree = new QTreeWidget(dlg);
    tree->setObjectName(QStringLiteral("properties"));
    tree->setColumnCount(2);
    tree->setHeaderHidden(true);
    tree->setColumnWidth(0, 260);
    auto* description = new QLabel(dlg);
    description->setWordWrap(true);
    description->setMinimumHeight(60);
    description->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    QTreeWidgetItem* group = nullptr;
    auto addGroup = [&](const QString& name) {
        group = new QTreeWidgetItem(tree, {name});
        QFont bold = group->font(0);
        bold.setBold(true);
        group->setFont(0, bold);
        group->setFirstColumnSpanned(true);
        group->setExpanded(true);
    };
    auto addRow = [&](const QString& name, const QString& help, QWidget* editor) {
        auto* item = new QTreeWidgetItem(group, {name});
        item->setToolTip(0, help);
        item->setData(0, Qt::UserRole, help);
        tree->setItemWidget(item, 1, editor);
    };
    auto addBool = [&](const QString& name, const QString& help, bool* value) {
        auto* box = new QCheckBox(dlg);
        box->setChecked(*value);
        addRow(name, help, box);
        f->apply.push_back([box, value] { *value = box->isChecked(); });
    };
    auto addInt = [&](const QString& name, const QString& help, int* value, int min, int max) -> QSpinBox* {
        auto* spin = new QSpinBox(dlg);
        spin->setRange(min, max);
        spin->setValue(*value);
        addRow(name, help, spin);
        f->apply.push_back([spin, value] { *value = spin->value(); });
        return spin;
    };
    auto addString = [&](const QString& name, const QString& help, std::wstring* value) {
        auto* edit = new QLineEdit(QString::fromStdWString(*value), dlg);
        addRow(name, help, edit);
        f->apply.push_back([edit, value] { *value = edit->text().toStdWString(); });
    };
    auto addChoice = [&](const QString& name, const QString& help, std::wstring* value, const QStringList& choices) {
        auto* combo = new QComboBox(dlg);
        combo->addItems(choices);
        combo->setCurrentText(QString::fromStdWString(*value));
        addRow(name, help, combo);
        f->apply.push_back([combo, value] { *value = combo->currentText().toStdWString(); });
    };

    struct_xmltoolsoptions& o = xmltoolsoptions;
    addGroup(QStringLiteral("Options"));
    auto* level = new QComboBox(dlg);
    level->addItems({QStringLiteral("0 = TRACE"), QStringLiteral("1 = INFO"), QStringLiteral("2 = WARNINGS"), QStringLiteral("3 = ERRORS")});
    level->setCurrentIndex(std::clamp(static_cast<int>(config.dbgLevel), 0, 3));
    addRow(QStringLiteral("Debug level"), QStringLiteral("The messages of the debug window: 0 = TRACE, 1 = INFO, 2 = WARNINGS, 3 = ERRORS"), level);
    f->apply.push_back([level] { config.dbgLevel = static_cast<DBG_LEVEL>(level->currentIndex()); });
    addChoice(QStringLiteral("Error display mode"), QStringLiteral("How errors are reported: Annotation (inline, see the annotation styles below), "
        "Dialog (all the errors in a scrollable dialog), Alert (a single alert dialog)."), &o.errorDisplayMode,
        {QStringLiteral("Annotation"), QStringLiteral("Dialog"), QStringLiteral("Alert")});
    f->annotationStyle = addInt(QStringLiteral("Annotations style"), QStringLiteral("Error messages style when displayed as annotations. Use \"Annotation preview\" to see a sample."), &o.annotationStyle, 0, 255);
    f->annotationHighlightStyle = addInt(QStringLiteral("Highlighted annotations style"), QStringLiteral("Highlighted error messages style when displayed as annotations."), &o.annotationHighlightStyle, 0, 255);
    addInt(QStringLiteral("Max displayed errors"), QStringLiteral("Maximum number of displayed errors; -1: no limit (slow with many errors)."), &o.maxErrorsNum, -1, 100000);
    addBool(QStringLiteral("Add node position in XPath"), QStringLiteral("The XPath of \"<a><b></b><b>Content</b></a>\" resolves to \"/a/b[2]\" instead of \"/a/b\"."), &o.printXPathIndex);
    addGroup(QStringLiteral("Status bar"));
    addBool(QStringLiteral("Display current XPath in statusbar"), QStringLiteral("The current XPath is printed in the status bar."), &o.xpathOnStatusbar);
    addString(QStringLiteral("Identity attribute names"), QStringLiteral("Semi-colon separated attribute names whose values are copied into the XPath of the status bar, "
        "for instance \".../sample[id='hello']/...\"."), &o.identityAttributes);
    addBool(QStringLiteral("Dump attributes name"), QStringLiteral("\".../sample[id='hello']/...\" instead of \".../sample[hello]/...\"."), &o.dumpAttributeName);
    addGroup(QStringLiteral("XML to Text conversion"));
    addBool(QStringLiteral("& / &amp;"), QString(), &o.convertAmp);
    addBool(QStringLiteral("< / &lt;"), QString(), &o.convertLt);
    addBool(QStringLiteral("> / &gt;"), QString(), &o.convertGt);
    addBool(QStringLiteral("\" / &quote;"), QString(), &o.convertQuote);
    addBool(QStringLiteral("' / &apos;"), QString(), &o.convertApos);
    addGroup(QStringLiteral("Pretty print options"));
    addChoice(QStringLiteral("Formating engine"), QStringLiteral("SimpleXml: fast and low memory; QuickXml: quick and simple, focus on conformity; "
        "StringXml: string processing, permissive on invalid xml but slow on big files."), &o.formatingEngine,
        {QStringLiteral("SimpleXml"), QStringLiteral("QuickXml"), QStringLiteral("StringXml")});
    addBool(QStringLiteral("Auto-close tags"), QStringLiteral("\"<sample></sample>\" becomes \"<sample/>\"."), &o.ppAutoclose);
    addBool(QStringLiteral("Ensure conformity"), QStringLiteral("Whitespace are trimmed and line breaks added only where it does not change the xml structure (QuickXml)."), &o.ensureConformity);
    addInt(QStringLiteral("Max indent level"), QStringLiteral("The max indentation level; 0: no limit."), &o.maxIndentLevel, 0, 1000);
    addBool(QStringLiteral("Apply xml:space=\"preserve\""), QStringLiteral("The formating engine takes care of xml:space (QuickXml)."), &o.applySpacePreserve);
    addGroup(QStringLiteral("Toolbar (after a restart)"));
    addBool(QStringLiteral("Toolbar icons"), QString(), &o.tbEnabled);
    addBool(QStringLiteral("Check XML syntax now"), QString(), &o.tbCheckXML);
    addBool(QStringLiteral("Validate now"), QString(), &o.tbValidateXML);
    addBool(QStringLiteral("First error"), QString(), &o.tbFirstError);
    addBool(QStringLiteral("Previous error"), QString(), &o.tbPrevError);
    addBool(QStringLiteral("Next error"), QString(), &o.tbNextError);
    addBool(QStringLiteral("Last error"), QString(), &o.tbLastError);
    addBool(QStringLiteral("Pretty print"), QString(), &o.tbPrettyPrint);
    addBool(QStringLiteral("Pretty print - indent attributes"), QString(), &o.tbPrettyPrintIndentAttr);
    addBool(QStringLiteral("Pretty print - indent only"), QString(), &o.tbPrettyPrintIndentOnly);
    addBool(QStringLiteral("Linearize"), QString(), &o.tbLinearize);
    addBool(QStringLiteral("Current XML Path"), QString(), &o.tbCurrentXMLPath);
    addBool(QStringLiteral("Current XML Path with predicates"), QString(), &o.tbCurrentXMLPathNS);
    addBool(QStringLiteral("Evaluate XPath expression"), QString(), &o.tbEvalXPath);
    addBool(QStringLiteral("XSL Transformation"), QString(), &o.tbXSLTransform);
    addBool(QStringLiteral("Escape characters"), QString(), &o.tbEscape);
    addBool(QStringLiteral("Unescape characters"), QString(), &o.tbUnescape);
    addBool(QStringLiteral("Comment selection"), QString(), &o.tbComment);
    addBool(QStringLiteral("Uncomment selection"), QString(), &o.tbUncomment);
    addBool(QStringLiteral("Options"), QString(), &o.tbOptions);
    QObject::connect(tree, &QTreeWidget::currentItemChanged, description, [description](QTreeWidgetItem* item) {
        description->setText(item ? item->data(0, Qt::UserRole).toString() : QString());
    });
    layout->addWidget(tree, 1);
    layout->addWidget(description);

    auto* row = new QHBoxLayout;
    auto* preview = new QPushButton(QStringLiteral("Annotation preview"), dlg);
    row->addWidget(preview);
    row->addStretch(1);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, dlg);
    row->addWidget(buttons);
    layout->addLayout(row);
    QObject::connect(buttons, &QDialogButtonBox::accepted, dlg, [this, dlg] {
        if (accepted())
            dlg->accept();
    });
    QObject::connect(buttons, &QDialogButtonBox::rejected, dlg, &QDialog::reject);
    QObject::connect(dlg, &QDialog::finished, dlg, [] { clearErrors(NULL, true); });
    QObject::connect(preview, &QPushButton::clicked, dlg, [f] {
        // OnBnClickedBtnviewannotation: examples of the annotation styles in the current document
        std::wstring prevStatus = xmltoolsoptions.errorDisplayMode;
        int prevStyle = xmltoolsoptions.annotationStyle;
        int prevHighlightStyle = xmltoolsoptions.annotationHighlightStyle;
        xmltoolsoptions.errorDisplayMode = L"Annotation";
        xmltoolsoptions.annotationStyle = f->annotationStyle->value();
        xmltoolsoptions.annotationHighlightStyle = f->annotationHighlightStyle->value();
        clearErrors();
        registerError(displayXMLError(L"This is an annotation example."));
        registerError(displayXMLError(L"This is an highlighted annotation example."));
        for (int i = 0; i < 64; ++i) {
            xmltoolsoptions.annotationStyle = i;
            registerError(displayXMLError(Report::str_format(L"Example of annotation style #%d", i)));
        }
        highlightError(1);
        xmltoolsoptions.errorDisplayMode = prevStatus;
        xmltoolsoptions.annotationStyle = prevStyle;
        xmltoolsoptions.annotationHighlightStyle = prevHighlightStyle;
    });
    dlg->resize(620, 560);
}

bool COptionsDlg::accepted() {
    if (m_fields) {
        for (auto& apply : m_fields->apply)
            apply();
    }
    savePluginParams();
    return true;
}

// ---- about, how to use, debug -----------------------------------------------------------------------------
CAboutBoxDlg::CAboutBoxDlg(CWnd*) {}
CAboutBoxDlg::~CAboutBoxDlg() {}

void CAboutBoxDlg::build(QDialog* dlg) {
    dlg->setWindowTitle(QStringLiteral("About XML Tools"));
    auto* layout = new QVBoxLayout(dlg);
    auto* text = new QLabel(QString::fromStdWString(Report::str_format(L"XML Tools Plugin\nversion %s\nLinux\n\nXML engine: libxml2 %s, XSLT: %s",
        XMLTOOLS_VERSION_NUMBER, QString::fromLatin1(LIBXML_DOTTED_VERSION).toStdWString().c_str(),
        (MSXMLWrapper(nullptr, 0).getCapabilities() & XmlCapabilityType::XSL_TRANSFORM) ? L"libxslt" : L"not available (libxslt is not installed)")), dlg);
    text->setObjectName(QStringLiteral("abouttext"));
    text->setAlignment(Qt::AlignCenter);
    layout->addWidget(text);
    auto* link = new QLabel(QStringLiteral("Home page: <a href=\"%1\">%1</a>").arg(QString::fromWCharArray(XMLTOOLS_HOMEPAGE_URL)), dlg);
    link->setOpenExternalLinks(true);
    layout->addWidget(link);
    auto* donate = new QLabel(QStringLiteral("<a href=\"%1\">Donate</a>").arg(QString::fromWCharArray(PAYPAL_URL)), dlg);
    donate->setOpenExternalLinks(true);
    donate->setAlignment(Qt::AlignCenter);
    layout->addWidget(donate);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok, dlg);
    QObject::connect(buttons, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
    layout->addWidget(buttons);
}

CHowtoUseDlg::CHowtoUseDlg(CWnd*) {}

void CHowtoUseDlg::build(QDialog* dlg) {
    dlg->setWindowTitle(QStringLiteral("XML Tools"));
    auto* layout = new QVBoxLayout(dlg);
    auto* text = new QLabel(QStringLiteral("XML Tools uses libxml2, and libxslt for the XSL transformations.\n"
        "Install them with the packages of your distribution (libxml2, libxslt) and restart Notepad++."), dlg);
    text->setWordWrap(true);
    layout->addWidget(text);
    auto* buttons = new QDialogButtonBox(QDialogButtonBox::Ok, dlg);
    QObject::connect(buttons, &QDialogButtonBox::accepted, dlg, &QDialog::accept);
    layout->addWidget(buttons);
}

CDebugDlg::CDebugDlg(CWnd*) {}
CDebugDlg::~CDebugDlg() {}

void CDebugDlg::build(QDialog* dlg) {
    dlg->setWindowTitle(QStringLiteral("XML Tools debug"));
    auto* layout = new QVBoxLayout(dlg);
    auto* text = new QPlainTextEdit(dlg);
    text->setObjectName(QStringLiteral("debug"));
    text->setReadOnly(true);
    text->setLineWrapMode(QPlainTextEdit::NoWrap);
    text->setMaximumBlockCount(5000);
    text->setPlainText(qs(s_valDebug));
    layout->addWidget(text);
    auto* clear = new QPushButton(QStringLiteral("Clear"), dlg);
    layout->addWidget(clear, 0, Qt::AlignRight);
    QObject::connect(clear, &QPushButton::clicked, dlg, [this] {
        s_valDebug.Empty();
        UpdateWindowText();
    });
    QObject::connect(dlg, &QDialog::finished, dlg, &QDialog::hide);
    dlg->resize(640, 400);
}

void CDebugDlg::UpdateWindowText() {
    if (auto* text = m_dialog ? m_dialog->findChild<QPlainTextEdit*>(QStringLiteral("debug")) : nullptr) {
        text->setPlainText(qs(s_valDebug));
        text->moveCursor(QTextCursor::End);
    }
}

void CDebugDlg::Add(CStringW line) {
    // the last lines only: the debug messages come with every notification
    s_valDebug += line;
    if (s_valDebug.GetLength() > 200000)
        s_valDebug = s_valDebug.Right(150000);
    if (auto* text = m_dialog ? m_dialog->findChild<QPlainTextEdit*>(QStringLiteral("debug")) : nullptr) {
        QTextCursor end(text->document());
        end.movePosition(QTextCursor::End);
        end.insertText(qs(line));
    }
}

void CDebugDlg::AddLine(CStringW line) {
    Add(line + L"\n");
}
