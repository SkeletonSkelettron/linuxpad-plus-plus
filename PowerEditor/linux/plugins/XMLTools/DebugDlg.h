#pragma once

#include "resource.h"
// Boîte de dialogue CDebugDlg

#ifdef NPP_PLUGIN_SDK_LINUX
#include "linux/LinuxDialog.h"

class CDebugDlg : public CLinuxDialog {
  void UpdateWindowText();
public:
  CDebugDlg(CWnd* pParent = NULL);
  ~CDebugDlg() override;
  void AddLine(CStringW line);
  void Add(CStringW line);
  enum { IDD = IDD_DLGDEBUG };
  CStringW s_valDebug;
protected:
  void build(QDialog* dlg) override;
};
#else
class CDebugDlg : public CDialog
{
  DECLARE_DYNAMIC(CDebugDlg)

  void UpdateWindowText();

public:
  CDebugDlg(CWnd* pParent = NULL);   // constructeur standard
  virtual ~CDebugDlg();

  void AddLine(CStringW line);
  void Add(CStringW line);

// Données de boîte de dialogue
  enum { IDD = IDD_DLGDEBUG };

protected:
  virtual void DoDataExchange(CDataExchange* pDX);    // Prise en charge de DDX/DDV

  DECLARE_MESSAGE_MAP()
public:
  CStringW s_valDebug;
  afx_msg void OnSize(UINT nType, int cx, int cy);
  virtual BOOL OnInitDialog();
  afx_msg void OnBnClickedBtnclose();
  afx_msg void OnBnClickedBtnclear();
};
#endif // NPP_PLUGIN_SDK_LINUX
