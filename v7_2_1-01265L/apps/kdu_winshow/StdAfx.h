// stdafx.h : include file for standard system include files,
//  or project specific include files that are used frequently, but
//      are changed infrequently
//

#if !defined(AFX_STDAFX_H__E984AEA8_E7A6_42BC_B09C_482D32D4727E__INCLUDED_)
#define AFX_STDAFX_H__E984AEA8_E7A6_42BC_B09C_482D32D4727E__INCLUDED_

#if _MSC_VER < 1500
#  define WINVER 0x0400
#else
#  define WINVER 0x0500
#endif // _MSC_VER (test for .NET2008 and higher)

#if _MSC_VER > 1000
#pragma once
#endif // _MSC_VER > 1000

#define VC_EXTRALEAN		// Exclude rarely-used stuff from Windows headers

#include <afxwin.h>         // MFC core and standard components
#include <afxext.h>         // MFC extensions
#include <afxdisp.h>        // MFC Automation classes
#include <afxdtctl.h>		// MFC support for Internet Explorer 4 Common Controls
#ifndef _AFX_NO_AFXCMN_SUPPORT
#include <afxcmn.h>			// MFC support for Windows Common Controls
#endif // _AFX_NO_AFXCMN_SUPPORT
// #include <afxdhtml.h>

//{{AFX_INSERT_LOCATION}}
// Microsoft Visual C++ will insert additional declarations immediately before the previous line.

#endif // !defined(AFX_STDAFX_H__E984AEA8_E7A6_42BC_B09C_482D32D4727E__INCLUDED_)
