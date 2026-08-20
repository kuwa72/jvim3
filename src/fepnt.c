/*
 * WindowsNT support (minimum function for jelvis)
 *   Written by Tomonori.Watanabe (GCD02235@niftyserve.or.jp)
 *                                (tom_w@st.rim.or.jp)
 *
 * This version is freely redistributable.
 */
/*
 * add function for jvim
 *   add by K.Tsuchida (ken_t@st.rim.or.jp/gdh01571@niftyserve.or.jp)
 */

/*
 * General purpose Japanese FEP control routines for MS-DOS.
 * Original is Written by Junn Ohta (ohta@src.ricoh.co.jp, msa02563)
 *
 *	int fep_init(void)
 *		checks FEP and turn it off, returns FEP type.
 *	void fep_term(void)
 *		restore the status of FEP saved by fep_init().
 *	void fep_on(void)
 *		restore the status of FEP saved by fep_off().
 *	void fep_off(void)
 *		save the status of FEP and turn it off.
 *	void fep_force_on(void)
 *		turn FEP on by its default "on" status.
 *	void fep_force_off(void)
 *		don't save the status of FEP and turn it off.
 *	int fep_get_mode(void)
 *		return the current status of FEP (0 = off).
 *
 */
#ifdef FEPCTRL

#include <windows.h>
#include <winnls32.h>
#include <ime.h>
#include <stdlib.h>				/* getenv() prototype defined */
#include "fepctrl.h"

static HGLOBAL		imeheap = NULL;
static long			imeParam;
static IMESTRUCT	*ime	= NULL;
static HWND			console;
static DWORD		fep_initial_state;
static DWORD		fep_state;
static BOOL			ime_initial_state;
static BOOL			ime_state;
static DWORD		fep_err;
static IMEPRO		ImeProfile;

typedef LRESULT	(WINAPI *SIMEME)(HWND, LPARAM);
typedef BOOL	(WINAPI *WINNLSGES)(HWND);
typedef BOOL	(WINAPI *WINNLSEIME)(HWND, BOOL);
typedef BOOL	(WINAPI *IGetIME)(HWND, LPIMEPRO);
typedef HWND	(WINAPI *IGetDefaultIMEWnd)(HWND);
typedef	HIMC	(WINAPI *IGetContext)(HWND);
typedef	BOOL	(WINAPI *ISetCompositionWindow)(HIMC, LPCOMPOSITIONFORM);
typedef	BOOL	(WINAPI *IReleaseContext)(HWND, HIMC);
typedef BOOL	(WINAPI *IGetOpenStatus)(HIMC);
typedef BOOL	(WINAPI *ISetOpenStatus)(HIMC, BOOL);

static SIMEME					pSendIMEMessageEx		= NULL;
static WINNLSGES				pWINNLSGetEnableStatus	= NULL;
static WINNLSEIME				pWINNLSEnableIME		= NULL;
static IGetIME					pIGetIME				= NULL;
static HANDLE					hWLibrary				= NULL;
static IGetDefaultIMEWnd		pIGetDefaultIMEWnd		= NULL;
static IGetContext				pIGetContext			= NULL;
static ISetCompositionWindow	pISetCompositionWindow	= NULL;
static IReleaseContext			pIReleaseContext		= NULL;
static IGetOpenStatus			pIGetOpenStatus			= NULL;
static ISetOpenStatus			pISetOpenStatus			= NULL;
static HANDLE					hILibrary				= NULL;

#define VIM_GUI
#ifdef VIM_GUI
extern HWND						hVimWnd;
extern int						GuiWin;
#endif

int
fep_init(void)
{
	UINT		w;

	if (imeheap != NULL)
		return (FEP_MSKANJI);

	w = SetErrorMode(SEM_NOOPENFILEERRORBOX);
	hWLibrary = LoadLibrary("user32.dll");
	SetErrorMode(w);
	if (hWLibrary == NULL)
		return (FEP_NONE);
	pSendIMEMessageEx
			= (SIMEME)GetProcAddress(hWLibrary, "SendIMEMessageExA");
	pWINNLSGetEnableStatus
			= (WINNLSGES)GetProcAddress(hWLibrary, "WINNLSGetEnableStatus");
	pWINNLSEnableIME
			= (WINNLSEIME)GetProcAddress(hWLibrary, "WINNLSEnableIME");
	pIGetIME = (IGetIME)GetProcAddress(hWLibrary, "IMPGetIMEA");
	if (pSendIMEMessageEx == NULL || pWINNLSGetEnableStatus == NULL
								|| pWINNLSEnableIME == NULL || pIGetIME == NULL)
	{
		FreeLibrary(hWLibrary);
		hWLibrary = NULL;
		return (FEP_NONE);
	}
	w = SetErrorMode(SEM_NOOPENFILEERRORBOX);
	hILibrary = LoadLibrary("imm32.dll");
	SetErrorMode(w);
	if (hILibrary != NULL)
	{
		pIGetDefaultIMEWnd = (IGetDefaultIMEWnd)
				GetProcAddress(hILibrary, "ImmGetDefaultIMEWnd");
		pIGetOpenStatus = (IGetOpenStatus)
				GetProcAddress(hILibrary, "ImmGetOpenStatus");
		pISetOpenStatus = (ISetOpenStatus)
				GetProcAddress(hILibrary, "ImmSetOpenStatus");
		pIGetContext = (IGetContext)
				GetProcAddress(hILibrary, "ImmGetContext");
		pISetCompositionWindow = (ISetCompositionWindow)
				GetProcAddress(hILibrary, "ImmSetCompositionWindow");
		pIReleaseContext = (IReleaseContext)
				GetProcAddress(hILibrary, "ImmReleaseContext");
		if (pIGetDefaultIMEWnd == NULL || pIGetOpenStatus == NULL
				|| pISetOpenStatus == NULL || pIGetContext == NULL
				|| pISetCompositionWindow == NULL || pIReleaseContext == NULL)
		{
			FreeLibrary(hILibrary);
			hILibrary = NULL;
		}
	}
	ime = NULL;
	console = GetForegroundWindow();
	if (!IsWindow(console))
	{
		fep_err = GetLastError();
		return (FEP_NONE);
	}
	if ((imeheap = GlobalAlloc(GHND|GMEM_SHARE, ((sizeof(IMESTRUCT)+32)&~31))) == NULL)
		return (FEP_NONE);
	if ((ime = (IMESTRUCT *)GlobalLock(imeheap)) == NULL)
	{
		GlobalFree(imeheap);
		imeheap = NULL;
		return (FEP_NONE);
	}
	if ((pWINNLSGetEnableStatus)(NULL) == TRUE)
		ime_initial_state = TRUE;
	else
	{
		ime_initial_state = FALSE;
		(*pWINNLSEnableIME)(NULL, TRUE);
	}
	imeParam = (long)imeheap;
	ime->fnc = IME_GETOPEN;
	GlobalUnlock(imeheap);
	fep_initial_state = (*pSendIMEMessageEx)(console, imeParam);
	fep_state = fep_initial_state;
	fep_force_off();
	pIGetIME(NULL, &ImeProfile);
	return (FEP_MSKANJI);
}

void
fep_term(void)
{
	if (imeheap != NULL && (ime = (IMESTRUCT *)GlobalLock(imeheap)) != NULL)
	{
		ime->fnc = IME_SETOPEN;
		ime->wParam = fep_initial_state;
		GlobalUnlock(imeheap);
		(*pSendIMEMessageEx)(console, imeParam);
		GlobalFree(imeheap);
		imeheap = NULL;
		if (ime_initial_state != TRUE)
			(*pWINNLSEnableIME)(NULL, FALSE);
		FreeLibrary(hWLibrary);
		if (hILibrary != NULL)
			FreeLibrary(hILibrary);
	}
}

void
fep_on(void)
{
	if (imeheap != NULL && (ime = (IMESTRUCT *)GlobalLock(imeheap)) != NULL)
	{
		ime->fnc = IME_SETOPEN;
		ime->wParam = fep_state;
		GlobalUnlock(imeheap);
		(*pSendIMEMessageEx)(console, imeParam);
	}
}

void
fep_off(void)
{
	if (imeheap != NULL && (ime = (IMESTRUCT *)GlobalLock(imeheap)) != NULL)
	{
		ime->fnc = IME_GETOPEN;
		GlobalUnlock(imeheap);
		fep_state = (*pSendIMEMessageEx)(console, imeParam);
	}
	fep_force_off();
}

void
fep_force_off(void)
{
#ifdef VIM_GUI
	if (GuiWin)
	{
		HWND				hIWnd;
		HIMC				hImc;

		if ((hIWnd = (*pIGetDefaultIMEWnd)(hVimWnd)) == NULL)
			return;
		hImc = (*pIGetContext)(hIWnd);
		(*pISetOpenStatus)(hImc, FALSE);
		(*pIReleaseContext)(hVimWnd, hImc);
		return;
	}
#endif
	if (imeheap != NULL && (ime = (IMESTRUCT *)GlobalLock(imeheap)) != NULL)
	{
		ime->fnc = IME_SETOPEN;
		ime->wParam = FALSE;
		GlobalUnlock(imeheap);
		(*pSendIMEMessageEx)(console, imeParam);
	}
}

void
fep_force_on(void)
{
#ifdef VIM_GUI
	if (GuiWin)
	{
		HWND				hIWnd;
		HIMC				hImc;

		if ((hIWnd = (*pIGetDefaultIMEWnd)(hVimWnd)) == NULL)
			return;
		hImc = (*pIGetContext)(hIWnd);
		(*pISetOpenStatus)(hImc, TRUE);
		(*pIReleaseContext)(hVimWnd, hImc);
		return;
	}
#endif
	if (imeheap != NULL && (ime = (IMESTRUCT *)GlobalLock(imeheap)) != NULL)
	{
		ime->fnc = IME_SETOPEN;
		ime->wParam = TRUE;
		GlobalUnlock(imeheap);
		(*pSendIMEMessageEx)(console, imeParam);
	}
}

int
fep_get_mode(void)
{
#ifdef VIM_GUI
	if (GuiWin)
	{
		HWND				hIWnd;
		HIMC				hImc;
		BOOL				ret;

		if ((hIWnd = (*pIGetDefaultIMEWnd)(hVimWnd)) == NULL)
			return(FALSE);
		hImc = (*pIGetContext)(hIWnd);
		ret = (*pIGetOpenStatus)(hImc);
		(*pIReleaseContext)(hVimWnd, hImc);
		return(ret);
	}
#endif
	if (imeheap != NULL && (ime = (IMESTRUCT *)GlobalLock(imeheap)) != NULL)
	{
		ime->fnc = IME_GETOPEN;
		GlobalUnlock(imeheap);
		return((*pSendIMEMessageEx)(console, imeParam));
	}
	return(0);
}

/*
 *	WINDOWS IME display sync routine
 */
void
fep_win_sync(HWND hWnd)
{
	HIMC			hImc;
	COMPOSITIONFORM	CompForm;

	if (hILibrary != NULL)
	{
		CompForm.dwStyle = CFS_POINT;
		GetCaretPos((LPPOINT) &CompForm.ptCurrentPos);
		hImc = (*pIGetContext)(hWnd);
		(*pISetCompositionWindow)(hImc, (COMPOSITIONFORM FAR*)&CompForm);
		(*pIReleaseContext)(hWnd, hImc);
	}
}

/*
 *	WINDOWS IME display font routine
 */
void
fep_win_font(HWND hWnd, LOGFONT *font)
{
	HANDLE		hWork;
	LPLOGFONT	pFont;

	if (imeheap != NULL && (ime = (IMESTRUCT *)GlobalLock(imeheap)) != NULL)
	{
		ime->fnc = IME_SETCONVERSIONFONTEX;
		hWork = GlobalAlloc (GMEM_SHARE | GMEM_MOVEABLE, sizeof(LOGFONT));
		pFont = (LPLOGFONT)GlobalLock(hWork);
		*pFont = *font;
		ime->lParam1 = (LPARAM)hWork;
		(*pSendIMEMessageEx)(hWnd, (LPARAM)ime);
		GlobalUnlock(hWork);
		GlobalUnlock(ime);
		GlobalFree(hWork);
	}
}
#endif
