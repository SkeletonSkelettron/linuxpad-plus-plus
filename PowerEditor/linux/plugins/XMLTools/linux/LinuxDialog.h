// This file is part of the Linux port of XML Tools for Notepad++ (GPL v3, see LICENSE).
// The dialogs of XML Tools are MFC dialogs on Windows; on Linux they are Qt dialogs (linux/XmlToolsDialogs.cpp)
// with the members the tools use: DoModal, or Create then ShowWindow for the modeless ones.

#pragma once

#include <windows.h>
#include <atlstr.h>

class CWnd;    // the parent of MFC: unused, the dialogs belong to the window of Notepad++
class QDialog;

class CLinuxDialog {
public:
    CLinuxDialog() = default;
    CLinuxDialog(const CLinuxDialog&) = delete;
    CLinuxDialog& operator=(const CLinuxDialog&) = delete;
    virtual ~CLinuxDialog();

    INT_PTR DoModal();                    // IDOK or IDCANCEL
    BOOL Create(UINT id, CWnd* parent = nullptr);
    BOOL ShowWindow(int cmd);
    QDialog* dialog() const { return m_dialog; }

protected:
    virtual void build(QDialog* dlg) = 0; // the widgets
    virtual bool accepted() { return true; } // OK: the fields into the members; false keeps the dialog open
    QDialog* ensure();

    QDialog* m_dialog = nullptr;
};
