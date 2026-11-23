#ifdef NPP_PLUGIN_SDK_LINUX
// Linux plugin SDK: no MFC. windows.h, the CStringW of atlstr.h; the dialogs are Qt dialogs (linux/LinuxDialog.h)
#pragma once
#define __AFXWIN_H__
#include <windows.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <atlstr.h>
#ifndef DEBUG_NEW
#define DEBUG_NEW new
#endif
#ifndef TRACE
#define TRACE(...)
#endif
#else
// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#if !defined(AFX_STDAFX_H__BE9E83F7_EE0F_4CF6_AFD2_432CBD75A5BE__INCLUDED_)
#define AFX_STDAFX_H__BE9E83F7_EE0F_4CF6_AFD2_432CBD75A5BE__INCLUDED_

#define _WIN32_WINNT 0x05010000

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define VC_EXTRALEAN    // Exclude rarely-used stuff from Windows headers

#include <afxwin.h>         // MFC core and standard components
#include <afxext.h>         // MFC extensions

#ifndef _AFX_NO_OLE_SUPPORT
#include <afxole.h>         // MFC OLE classes
#include <afxodlgs.h>       // MFC OLE dialog classes
#include <afxdisp.h>        // MFC Automation classes
#endif // _AFX_NO_OLE_SUPPORT
/*
#ifndef _AFX_NO_DB_SUPPORT
#include <afxdb.h>      // MFC ODBC database classes
#endif // _AFX_NO_DB_SUPPORT

#ifndef _AFX_NO_DAO_SUPPORT
#include <afxdao.h>      // MFC DAO database classes
#endif // _AFX_NO_DAO_SUPPORT

#include <afxdtctl.h>    // MFC support for Internet Explorer 4 Common Controls
*/
#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <afxcmn.h>      // MFC support for Windows Common Controls
#endif // _AFX_NO_AFXCMN_SUPPORT


//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_STDAFX_H__BE9E83F7_EE0F_4CF6_AFD2_432CBD75A5BE__INCLUDED_)
#include <afxcontrolbars.h>
#endif // NPP_PLUGIN_SDK_LINUX
