/* vi:ts=4:sw=4
 *
 * VIM - Vi IMproved
 *
 */

/*
 * winnt.c
 *
 * Windows NT system-dependent routines.
 * A reasonable approximation of the amiga dependent code.
 * Portions lifted from SDK samples, from the MSDOS dependent code,
 * and from NetHack 3.1.3.
 *
 * rogerk@wonderware.com
 */

#include <io.h>
#include <direct.h>
#include <stdint.h>

#include "vim.h"
#include "globals.h"
#include "param.h"
#include "ops.h"
#include "proto.h"
#include <fcntl.h>
#ifdef KANJI
#include "kanji.h"
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdlib.h>
#undef DELETE
#include <windows.h>
#include <windowsx.h>
#include <wincon.h>
#include <commctrl.h>
#include <dlgs.h>
#ifndef NO_WHEEL
# include <zmouse.h>
# ifndef SPI_GETWHEELSCROLLLINES
#  define SPI_GETWHEELSCROLLLINES   104
# endif
#endif
#ifndef WS_EX_LAYERED
# define	WS_EX_LAYERED			0x80000
#endif
#ifndef LWA_ALPHA
# define	LWA_ALPHA				2
#endif
#ifndef LSFW_LOCK
# define	LSFW_LOCK				1
#endif
#ifndef LSFW_UNLOCK
# define	LSFW_UNLOCK				2
#endif
#include "winjmenu.h"
#include <ocidl.h>
#include <olectl.h>
#include <crtdbg.h>
#include <process.h>	/* _beginthread */
#define HIMETRIC_INCH	2540

#define CUST_MENU					0

#define MAX_HISTORY					30
#define TITLE_LEN					48

static int WaitForChar __ARGS((int));
static int cbrk_handler __ARGS(());
static void delay __ARGS((int));
static void gotoxy __ARGS((int, int));
static void scroll __ARGS((void));
static void vbell __ARGS((void));
static void cursor_visible __ARGS((int));
static void clrscr __ARGS((void));
static void clreol __ARGS((void));
static void insline __ARGS((int));
static void delline __ARGS((int));
static void normvideo __ARGS((void));
static void textattr __ARGS((WORD));
static void putch __ARGS((char));
static int kbhit __ARGS((void));
static int tgetch __ARGS((void));
static void resizeConBufAndWindow __ARGS((HANDLE, long, long));
#ifndef notdef
static int isctlkey __ARGS((void));
#endif

/* Win32 Console handles for input and output */
HANDLE          hConIn;
HANDLE          hConOut;
static int		maxRows;
#ifndef notdef
static HANDLE	h_mainthread;
#endif
static BOOL		v_nt;
static BOOL		IsTelnet = FALSE;

/* Win32 Screen buffer,coordinate,console I/O information */
CONSOLE_SCREEN_BUFFER_INFO csbi;
COORD           ntcoord;
INPUT_RECORD    ir;

/* The attribute of the screen when the editor was started */
WORD            DefaultAttribute;

#define KEY_TIME		1
#define MOUSE_TIME		3
#define TRIPLE_TIME		4
#define SHOW_TIME		5
#define TAIL_TIME		6
#define WM_TASKTRAY		(WM_APP + 100)

#define KEY_REP			10		/* key repeat count */
#define KEY_REDRAW		7		/* redraw use count */

#if defined(KANJI) && defined(SYNTAX)
#define	CMODE			'*'
#else
#define	CMODE			0x80
#endif

static INT				iScrollLines			= 3;
#ifndef NO_WHEEL
static UINT				uiMsh_MsgMouseWheel		= 0;
static UINT				uiMsh_Msg3DSupport		= 0;
static UINT				uiMsh_MsgScrollLines	= 0;
static BOOL				f3DSupport				= 0;
#endif
static BOOL				bIClose					= FALSE;
static BOOL				bWClose					= FALSE;
static int				nowRows = 25;
static int				nowCols = 80;
static char				keybuf[128];
static char_u		*	cbuf					= keybuf;
static char				szAppName[16]			= "JVim";
static const WCHAR	   *szAppNameW				= L"JVim";
/*
 * The window is a Unicode one (RegisterClassW below), so WM_CHAR carries UTF-16
 * code units and the IME and the emoji picker can deliver anything. The
 * character is staged here as UTF-8, which is what the buffer holds.
 */
static char_u			wc_bytes[UTF8_MAXLEN];
static int				wc_len					= 1;
static int				c_size					= sizeof(keybuf);
static int				c_end					= 0;
static int				c_next					= 0;
static int				c_ind					= 0;
static int				w_p_tw;
static int				w_p_wm;
static int				w_p_ai;
static int				w_p_si;
static int				w_p_et;
static int				w_p_uc;
static int				w_p_sm;
static int				w_p_ru;
static int				w_p_ri;
static int				w_p_paste;
	   HWND				hVimWnd	= NULL;
static HACCEL			hAcc = NULL;
static DWORD			config_x;
static DWORD			config_y;
static DWORD			config_w;
static DWORD			config_h;
static DWORD			config_sbar		= TRUE;
static DWORD			config_save		= FALSE;
static DWORD			config_comb		= FALSE;
static DWORD			config_tray		= FALSE;
static DWORD			config_mouse	= FALSE;
#ifdef NT106KEY
static DWORD			config_nt106	= FALSE;
#endif
static DWORD			config_menu		= TRUE;
/*
 * The DPI that config_font, config_jfont, config_w and config_h are pixel sizes
 * for. 96 is also what settings written before JVim became DPI aware mean,
 * since Windows virtualised the DPI to 96 for the process that stored them.
 */
static int				config_dpi		= 96;
/*
 * LOGFONTW, not LOGFONTA. LF_FACESIZE is 32 and Windows counts it in
 * *characters*: the wide struct holds 32 of them, the ANSI one 32 bytes, which
 * is ten Japanese characters in UTF-8. A face name longer than that was cut --
 * mid character, so what came back was not a font name and the font silently
 * fell back to the default. Windows is UTF-16 at its own API anyway, so the
 * wide form is both the roomier and the more direct one: nothing has to be
 * converted to create the font.
 */
static LOGFONTW			config_font;
#ifdef KANJI
static LOGFONTW			config_jfont;
static BOOL				v_difffont		= FALSE;
static INT_PTR CALLBACK	FontDialogProc(HWND, UINT, WPARAM, LPARAM);
#endif
static DWORD			config_fgcolor	= RGB(  0,   0,   0);
static DWORD			config_bgcolor	= RGB(255, 255, 255);
static DWORD			config_tbcolor	= (-1);	/* bold */
static DWORD			config_socolor	= (-1);	/* standout */
static DWORD			config_ticolor	= (-1);	/* invert/reverse */
static DWORD			config_tbbitmap	= (-1);	/* bold */
static DWORD			config_sobitmap	= (-1);	/* standout */
static DWORD			config_tibitmap	= (-1);	/* invert/reverse */
static DWORD			config_color[16];
static char				config_printer[MAXPATHL];
static DWORD			config_bitmap	= FALSE;
static char				config_bitmapfile[MAXPATHL];
static DWORD			config_bitsize	= 100;
static DWORD			config_bitcenter= TRUE;
static DWORD			config_wave		= FALSE;
static char				config_wavefile[MAXPATHL];
static int				config_overflow = 3; 	/* larger than 2 */
static DWORD			config_show = 500;
static DWORD			config_fadeout = TRUE;
static BOOL				config_grepwin = TRUE;
#ifdef USE_HISTORY
static DWORD			config_history = TRUE;
static DWORD			config_hauto = TRUE;
static HMENU			hHist;
#endif
static char				config_load[CMDBUFFSIZE];
static char				config_unload[CMDBUFFSIZE];
static BOOL				config_ini	= FALSE;
static int				do_resize	= FALSE;
static BOOL				do_time		= FALSE;
static BOOL				do_vb		= FALSE;
static BOOL				do_trip		= FALSE;
static BOOL				do_drag		= FALSE;
static int				v_row		= 0;
static int				v_col		= 0;
static int				v_region	= 0;
static BOOL				v_cursor	= FALSE;
static BOOL				v_focus		= FALSE;
static int				v_caret		= 0;
static int				v_xchar		= 0;
static int				v_ychar		= 0;
static int				v_lspace	= 0;
static int				v_cspace	= 0;
static int				v_trans		= 0;
static DWORD			scheme_fgcolor;	/* "hi Normal guifg=", RGB() of syn_normal_fg */
static DWORD			scheme_bgcolor;	/* "hi Normal guibg=", RGB() of syn_normal_bg */
static DWORD		*	v_fgcolor	= &config_fgcolor;
static DWORD		*	v_bgcolor	= &config_bgcolor;
static DWORD		*	v_tbcolor	= &config_tbcolor;
static DWORD		*	v_socolor	= &config_socolor;
static DWORD		*	v_ticolor	= &config_ticolor;
static BOOL				v_ttfont;
static HFONT			v_font;
static HFONT			hSystemUIFont	= NULL;
static INT			*	v_space		= NULL;
static short		*	v_char		= NULL;
static INT				v_ssize		= 0;
static HANDLE			hInst;
static HMENU			v_menu		= NULL;
static BOOL				v_extend	= FALSE;
static BOOL				v_macro		= FALSE;
static OSVERSIONINFO	ver_info;
static BOOL				do_msg		= FALSE;
static BOOL				bSyncPaint	= FALSE;
static HCURSOR			hIbeamCurs	= NULL;
static HCURSOR			hArrowCurs	= NULL;
static HCURSOR			hWaitCurs	= NULL;
static LPSTR			lpCurrCurs	= NULL;
static NOTIFYICONDATA	nIcon;
static INT_PTR CALLBACK	PrinterDialog(HWND, UINT, WPARAM, LPARAM);
static INT_PTR CALLBACK	BitmapDialog(HWND, UINT, WPARAM, LPARAM);
static INT_PTR CALLBACK	WaveDialog(HWND, UINT, WPARAM, LPARAM);
static INT_PTR CALLBACK	CommandDialog(HWND, UINT, WPARAM, LPARAM);
static INT_PTR CALLBACK	LoadDialog(HWND, UINT, WPARAM, LPARAM);
static INT_PTR CALLBACK	QuitConfirmDialog(HWND, UINT, WPARAM, LPARAM);
static void				SetDialogSystemFont(HWND);
static void				LoadCommand();
static void				UnloadCommand();
static char *			DisplayPathName(char *, unsigned int);
static char 		*	HistoryGetMenu(int);
static char			*	HistoryGetCommand(int);
static void				HistoryRename(int, int);
static INT_PTR CALLBACK	LineSpaceDialog(HWND, UINT, WPARAM, LPARAM);
static INT_PTR CALLBACK	LineSpaceDialogEx(HWND, UINT, WPARAM, LPARAM);
static void				ScrollBar();
static int				isbitmap(char *, HWND);
static int				iswave(char *);
static BOOL				CopyScreenToBitmap(HDC hDC, BOOL force);
static BOOL				LoadBitmapFromBMPFile(HDC hDC, LPTSTR szFileName);
static void				SetLayerd(void);

typedef HWND			(WINAPI *tCreateUpDownControl)(DWORD, int, int, int, int, HWND, int, HINSTANCE, HWND, int, int, int);
static	tCreateUpDownControl		pCreateUpDownControl		= NULL;

typedef DWORD			(WINAPI *tSetLayeredWindowAttributes)(HWND, DWORD, BYTE, DWORD);
static	tSetLayeredWindowAttributes	pSetLayeredWindowAttributes	= NULL;

typedef BOOL			(WINAPI *tAllowSetForegroundWindow)(DWORD);
static	tAllowSetForegroundWindow	pAllowSetForegroundWindow	= NULL;

typedef	BOOL			(WINAPI *tLockSetForegroundWindow)(UINT);
static	tLockSetForegroundWindow	pLockSetForegroundWindow	= NULL;

typedef UINT			(WINAPI *tGetDpiForWindow)(HWND);
static	tGetDpiForWindow			pGetDpiForWindow			= NULL;

typedef UINT			(WINAPI *tGetDpiForSystem)(void);
static	tGetDpiForSystem			pGetDpiForSystem			= NULL;

/* Windows 8.1; the mingw and SDK headers only declare it above WINVER 0x0605. */
#ifndef WM_DPICHANGED
# define WM_DPICHANGED	0x02E0
#endif

static int	dpi_of __ARGS((HWND));
static void	dpi_scale_to __ARGS((int));

typedef struct filelist
{
	char_u        **file;
	int             nfiles;
	int             maxfiles;
} FileList;

static void		addfile __ARGS((FileList *, char *, int));
#ifdef __BORLANDC__
static int      pstrcmp();      /* __ARGS((char **, char **)); BCC does not
								 * like this */
#else
static int      pstrcmp __ARGS((const void *, const void *));
#endif
static void		strlowcpy __ARGS((char *, char *));
static int		expandpath __ARGS((FileList *, char *, int, int, int));

static int      cbrk_pressed = FALSE;   /* set by ctrl-break interrupt */
static int      ctrlc_pressed = FALSE;  /* set when ctrl-C or ctrl-break
										 * detected */
static BOOL		syntax_on();

#if defined(KANJI) && defined(SYNTAX)
# define istrans()		((config_bitmap || (syntax_on() && !v_ttfont)) ? TRUE : FALSE)
#  define italicplus()	((config_bitmap || (syntax_on() && !v_ttfont)) ? 1 : 0)
#  define issynpaint()	(syntax_on() && !v_ttfont && !config_bitmap)
#else
# define istrans()		config_bitmap
# define italicplus()	(0)
# define issynpaint()	(0)
#endif
/* Set by PaintWindow(): a background bitmap is there, so leave the text bare. */
static BOOL		v_bmpon			= FALSE;
#ifdef KANJI
# define iskanakan(c)	(ISkanji(c) ? 1 : 0)
/*
 * The screen holds UTF-8, so the character in a cell comes from the code point
 * plane (see screen.c): 0 means the byte in the character plane is all there
 * is, -1 means the cell is the right half of a double width character.
 */
# define CELLCP(row, col)	(WinScreenCP != NULL ? WinScreenCP[row][col] : 0)
# define CELLCONT(row, col)	(CELLCP(row, col) == -1)
# define CELLWIDE(row, col)	(CELLCP(row, col) > 0 \
									&& utf_cpwidth(CELLCP(row, col)) == 2)
/* Font choice: 1 for a character the Japanese font should draw. */
# define CELLKANA(row, col)	(CELLCP(row, col) > 0 ? 1 : 0)
static void	push_wchar __ARGS((void));
static WCHAR *	utf8_to_wide __ARGS((char_u *));
static char_u *	wide_to_utf8 __ARGS((WCHAR *));
static void	SetDlgItemTextU8 __ARGS((HWND, int, char_u *));
static int	GetDlgItemTextU8 __ARGS((HWND, int, char_u *, int));
static BOOL	AppendMenuU8 __ARGS((HMENU, UINT, UINT_PTR, char_u *));
static BOOL	ModifyMenuU8 __ARGS((HMENU, UINT, UINT, UINT_PTR, char_u *));
static void	font_widen __ARGS((LOGFONTW *, LOGFONTA *));
static int	font_load __ARGS((HKEY, char *, char *, LOGFONTW *));
static int	font_save __ARGS((HKEY, char *, char *, LOGFONTW *));
static void	font_narrow __ARGS((LOGFONTA *, LOGFONTW *));
static BOOL	ChooseFontJ __ARGS((HWND, LOGFONTW *));
static BOOL	RegGetStringU8 __ARGS((HKEY, char *, char_u *, int));
static LONG	RegSetStringU8 __ARGS((HKEY, char *, char_u *));
static int	cell_head __ARGS((int, int));
static int	cell_class __ARGS((int, int));
static int	cell_width __ARGS((int, int));
#endif

#if defined(KANJI) && defined(SYNTAX)
/*
 *
 */
static BOOL
syntax_on(void)
{
	WIN		*	wp;

	wp = firstwin;
	while (wp != NULL)
	{
		if (wp->w_p_syt)
			return(TRUE);
		wp = wp->w_next;
	}
	return(FALSE);
}
#endif

/*
 * The DPI of the monitor 'hWnd' is on, or of the system when there is no window
 * yet. GetDpiForWindow() and GetDpiForSystem() arrived in Windows 10 1607 and
 * are looked up at run time; before them every monitor was at the one DPI a
 * screen DC reports, and before Windows 8.1 that could not change while the
 * process ran either.
 */
static int
dpi_of(HWND hWnd)
{
	UINT		dpi = 0;
	HDC			hDC;

	if (hWnd != NULL && pGetDpiForWindow != NULL)
		dpi = pGetDpiForWindow(hWnd);
	else if (pGetDpiForSystem != NULL)
		dpi = pGetDpiForSystem();
	if (dpi == 0 && (hDC = GetDC(hWnd)) != NULL)
	{
		dpi = GetDeviceCaps(hDC, LOGPIXELSY);
		ReleaseDC(hWnd, hDC);
	}
	return(dpi == 0 ? 96 : (int)dpi);
}

/*
 * Restate the font and window sizes, which are all in pixels, for 'dpi'.
 *
 * The character grid comes out of this unchanged: the font and the window that
 * has to hold Rows by Columns of it grow by the same factor, so this is a
 * resize of the window and not a reflow of the text. What it buys is a font
 * asked for in the pixels the display really has, which is the whole point of
 * being DPI aware -- ask for 14 pixels on a 144 DPI screen and the text is
 * two thirds the size it used to be.
 *
 * v_lspace and v_cspace are left alone. They are hand-set nudges of a pixel or
 * two from a dialog that offers 0 to 10, and scaling them would make the dialog
 * disagree with what the user typed into it.
 */
static void
dpi_scale_to(int dpi)
{
	if (dpi <= 0)
		return;
	if (BenchTime || dpi == config_dpi || config_dpi <= 0)
	{
		config_dpi = dpi;			/* nothing to scale, but record where we are */
		return;
	}
	config_font.lfHeight	= MulDiv(config_font.lfHeight, dpi, config_dpi);
	config_font.lfWidth		= MulDiv(config_font.lfWidth, dpi, config_dpi);
#ifdef KANJI
	config_jfont.lfHeight	= MulDiv(config_jfont.lfHeight, dpi, config_dpi);
	config_jfont.lfWidth	= MulDiv(config_jfont.lfWidth, dpi, config_dpi);
#endif
	if (config_w != (DWORD)CW_USEDEFAULT)
		config_w = (DWORD)MulDiv((int)config_w, dpi, config_dpi);
	if (config_h != (DWORD)CW_USEDEFAULT)
		config_h = (DWORD)MulDiv((int)config_h, dpi, config_dpi);
	config_dpi = dpi;
}

/*
 *
 */
static VOID
LoadConfig(BOOL init)
{
	HKEY		hKey;
	DWORD		size;
	DWORD		type;
	int			openkey = FALSE;
	char		name[_MAX_PATH];

	if (BenchTime)
		goto error;
	while (init)		/* ini file */
	{
		/* MAXPATHL, not _MAX_PATH: this holds the exe's own path, and that is
		 * counted in bytes here (see vim.h) */
		char			szIniFile[MAXPATHL];	/* private profile file name  */
		char			szSecName[32];		/* private profile section name  */
		char			facebuf[LF_FACESIZE * 4];	/* a face name as ini bytes */
		char			color[128];
		char		*	p;
		char		*	last;
		HWND			hWnd;
		DWORD			rgb[3];

		if (strcmp(GuiIni, "reg") == 0)
			break;
		if ((p = STRCHR(GuiIni, ':')) != NULL && getperm(p + 1) != (-1))
		{
			size_t	seclen = (size_t)(p - (char *)GuiIni);

			/* 'GuiIni' is whatever "-I" was given, up to MAXPATHL of it, and
			 * these two are far shorter than that */
			if (seclen > sizeof(szSecName) - 1)
				seclen = sizeof(szSecName) - 1;
			ZeroMemory(szSecName, sizeof(szSecName));
			strncpy(szSecName, (char *)GuiIni, seclen);
			lstrcpynA(szIniFile, p + 1, sizeof(szIniFile));
		}
		else
		{
			lstrcpynA(szSecName, GuiIni, sizeof(szSecName));
			if (GetModuleFileName(NULL, szIniFile, sizeof(szIniFile)) == 0)
				break;
			last = p = szIniFile + 3;	/* drive + : + \ */
			while (*p)
			{
				if (*p == '.')
					last = p + 1;
				p++;
			}
			*last = '\0';
			lstrcpy(last, "ini");
		}
		if (getperm(szIniFile) == (-1))
			break;
		config_ini = TRUE;
		hWnd = CreateDialog(hInst, "LOAD", NULL, LoadDialog);
		ShowWindow(hWnd, SW_NORMAL);
		Sleep(1000);
		/* get parameter */
		GetPrivateProfileString(szSecName, "printer", "",
							config_printer, sizeof(config_printer), szIniFile);
		Columns = GetPrivateProfileInt(szSecName, "cols", 80, szIniFile);
		Rows = GetPrivateProfileInt(szSecName, "rows", 25, szIniFile);
		config_sbar = GetPrivateProfileInt(szSecName, "scrollbar", TRUE, szIniFile);
		config_menu = GetPrivateProfileInt(szSecName, "menu", TRUE, szIniFile);
		GetPrivateProfileString(szSecName, "bitmap", "",
					config_bitmapfile, sizeof(config_bitmapfile), szIniFile);
		if (!isbitmap(config_bitmapfile, NULL))
			config_bitmapfile[0] = '\0';
		else
			config_bitmap = TRUE;
		GetPrivateProfileString(szSecName, "wave", "",
						config_wavefile, sizeof(config_wavefile), szIniFile);
		if (!iswave(config_wavefile))
			config_wavefile[0] = '\0';
		else
			config_wave = TRUE;
		config_fadeout
				= GetPrivateProfileInt(szSecName, "fadeout", 1, szIniFile);
		config_grepwin
				= GetPrivateProfileInt(szSecName, "grepwin", 1, szIniFile);
#ifdef USE_HISTORY
		config_history	= FALSE;
		config_hauto	= FALSE;
		GetPrivateProfileString(szSecName, "history", "auto",
										color, sizeof(color), szIniFile);
		if (strcmp("auto", color) == 0)
		{
			config_history	= TRUE;
			config_hauto	= TRUE;
		}
		else if (strcmp("on", color) == 0)
			config_history	= TRUE;
#endif
		config_fgcolor = RGB(0, 0, 0);
		GetPrivateProfileString(szSecName, "textcolor", "",
										color, sizeof(color), szIniFile);
		if (strcmp("white", color) == 0)
			config_fgcolor = RGB(255, 255, 255);
		else if (strcmp("black", color) == 0)
			config_fgcolor = RGB(0, 0, 0);
		else if (strcmp("blue", color) == 0)
			config_fgcolor = RGB(0, 0, 128);
		else
		{
			sscanf(color, "%d,%d,%d", &rgb[0], &rgb[1], &rgb[2]);
			config_fgcolor = RGB(rgb[0], rgb[1], rgb[2]);
		}
		config_bgcolor = RGB(255, 255, 255);
		GetPrivateProfileString(szSecName, "backcolor", "",
										color, sizeof(color), szIniFile);
		if (strcmp("white", color) == 0)
			config_bgcolor = RGB(255, 255, 255);
		else if (strcmp("black", color) == 0)
			config_bgcolor = RGB(0, 0, 0);
		else if (strcmp("blue", color) == 0)
			config_bgcolor = RGB(0, 0, 128);
		else
		{
			sscanf(color, "%d,%d,%d", &rgb[0], &rgb[1], &rgb[2]);
			config_bgcolor = RGB(rgb[0], rgb[1], rgb[2]);
		}
		config_font.lfHeight
				= GetPrivateProfileInt(szSecName, "fontsize", 14, szIniFile);
		{
			HDC         hDC;
			INT			PointSize = GetPrivateProfileInt(szSecName, "fontsize", 14, szIniFile);

			hDC = GetDC(hWnd);
			config_font.lfHeight =
						-MulDiv(PointSize, GetDeviceCaps(hDC, LOGPIXELSY), 72);
			ReleaseDC(hWnd, hDC);
		}
		config_font.lfWidth			= 0;
		config_font.lfEscapement	= 0;
		config_font.lfOrientation	= 0;
		config_font.lfWeight		= 0;
		config_font.lfItalic		= 0;
		config_font.lfUnderline		= 0;
		config_font.lfStrikeOut		= 0;
		/* config_font.lfCharSet	= OEM_CHARSET; */
		config_font.lfCharSet		= 0;
		config_font.lfOutPrecision	= OUT_DEFAULT_PRECIS;
		config_font.lfClipPrecision = CLIP_DEFAULT_PRECIS;
		config_font.lfQuality		= DEFAULT_QUALITY;
		config_font.lfPitchAndFamily= FIXED_PITCH | FF_MODERN;
		/*
		 * The ini is bytes, the face name is characters. GetPrivateProfileStringW
		 * would need the section, key and path in wide form too, so read the
		 * bytes and widen -- with a buffer that can hold 32 characters of them,
		 * rather than the 32 bytes the face name used to be.
		 */
		GetPrivateProfileString(szSecName, "fontname", "FixedSys",
							facebuf, sizeof(facebuf), szIniFile);
		MultiByteToWideChar(p_cpage, 0, facebuf, -1,
							config_font.lfFaceName, LF_FACESIZE);
#ifdef KANJI
		GetPrivateProfileString(szSecName, "jfontname", "",
							facebuf, sizeof(facebuf), szIniFile);
		MultiByteToWideChar(p_cpage, 0, facebuf, -1,
							config_jfont.lfFaceName, LF_FACESIZE);
		if (config_jfont.lfFaceName[0] == L'\0')
			memcpy(&config_jfont, &config_font, sizeof(config_jfont));
		else
		{
			config_jfont.lfHeight
					= GetPrivateProfileInt(szSecName, "jfontsize", 14, szIniFile);
			{
				HDC         hDC;
				INT			PointSize = GetPrivateProfileInt(szSecName, "jfontsize", 14, szIniFile);

				hDC = GetDC(hWnd);
				config_jfont.lfHeight =
							-MulDiv(PointSize, GetDeviceCaps(hDC, LOGPIXELSY), 72);
				ReleaseDC(hWnd, hDC);
			}
			config_jfont.lfWidth		= 0;
			config_jfont.lfEscapement	= 0;
			config_jfont.lfOrientation	= 0;
			config_jfont.lfWeight		= 0;
			config_jfont.lfItalic		= 0;
			config_jfont.lfUnderline	= 0;
			config_jfont.lfStrikeOut	= 0;
			config_jfont.lfOutPrecision	= OUT_DEFAULT_PRECIS;
			config_jfont.lfClipPrecision = CLIP_DEFAULT_PRECIS;
			config_jfont.lfQuality		= DEFAULT_QUALITY;
			config_jfont.lfPitchAndFamily= FIXED_PITCH | FF_MODERN;
		}
		/* config_jfont.lfCharSet	= OEM_CHARSET; */
		config_jfont.lfCharSet		= SHIFTJIS_CHARSET;
#endif
		/*
		 * "fontsize" in an ini file is a point size, and it was just turned
		 * into pixels through the DPI of a real DC, so it needs no further
		 * scaling -- unlike the registry, which stores pixels.
		 */
		config_dpi = dpi_of(hWnd);
		v_lspace = GetPrivateProfileInt(szSecName, "linespace", 0, szIniFile);
		if (v_lspace > 10)
			v_lspace = 10;
		v_cspace = GetPrivateProfileInt(szSecName, "charspace", 0, szIniFile);
		if (v_cspace > 10)
			v_cspace = 10;
		config_x = CW_USEDEFAULT;
		config_y = CW_USEDEFAULT;
		config_w = CW_USEDEFAULT;
		config_h = CW_USEDEFAULT;
		DestroyWindow(hWnd);
		return;
	}
	/*
	 *	Common Registory
	 */
	if (RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Vim", 0,
										KEY_ALL_ACCESS, &hKey) != ERROR_SUCCESS)
		goto error;
	openkey = TRUE;
	size = sizeof(config_printer);
	type = REG_SZ;
	if (RegGetStringU8(hKey, "printer", (char_u *)config_printer, sizeof(config_printer)) == FALSE)
		goto error;
	size = sizeof(config_tray);
	type = REG_DWORD;
	if (RegQueryValueEx(hKey, "tray", NULL, &type, (BYTE *)&config_tray, &size)
															!= ERROR_SUCCESS)
		goto error;
	size = sizeof(config_mouse);
	type = REG_DWORD;
	if (RegQueryValueEx(hKey, "mouse", NULL, &type, (BYTE *)&config_mouse, &size)
															!= ERROR_SUCCESS)
		goto error;
#ifdef NT106KEY
	size = sizeof(config_nt106);
	type = REG_DWORD;
	if (RegQueryValueEx(hKey, "nt106", NULL, &type, (BYTE *)&config_nt106, &size)
															!= ERROR_SUCCESS)
		goto error;
#endif
	size = sizeof(config_menu);
	type = REG_DWORD;
	if (RegQueryValueEx(hKey, "menu", NULL, &type, (BYTE *)&config_menu, &size)
															!= ERROR_SUCCESS)
		goto error;
	size = sizeof(config_sbar);
	type = REG_DWORD;
	if (RegQueryValueEx(hKey, "scrollbar", NULL, &type, (BYTE *)&config_sbar, &size)
															!= ERROR_SUCCESS)
		goto error;
	size = sizeof(config_fadeout);
	type = REG_DWORD;
	if (RegQueryValueEx(hKey, "fadeout", NULL, &type, (BYTE *)&config_fadeout, &size)
															!= ERROR_SUCCESS)
		goto error;
	size = sizeof(config_grepwin);
	type = REG_DWORD;
	if (RegQueryValueEx(hKey, "grepwin", NULL, &type, (BYTE *)&config_grepwin, &size)
															!= ERROR_SUCCESS)
		goto error;
#ifdef USE_HISTORY
	size = sizeof(config_history);
	type = REG_DWORD;
	if (RegQueryValueEx(hKey, "history", NULL, &type, (BYTE *)&config_history, &size)
															!= ERROR_SUCCESS)
		goto error;
	size = sizeof(config_hauto);
	type = REG_DWORD;
	if (RegQueryValueEx(hKey, "hauto", NULL, &type, (BYTE *)&config_hauto, &size)
															!= ERROR_SUCCESS)
		goto error;
#endif
	size = sizeof(config_color);
	type = REG_BINARY;
	if (RegQueryValueEx(hKey, "custcolor", NULL, &type, (BYTE *)config_color, &size)
															!= ERROR_SUCCESS)
		goto error;
	size = sizeof(config_overflow);
	type = REG_DWORD;
	if (RegQueryValueEx(hKey, "overflow", NULL, &type, (BYTE *)&config_overflow, &size)
															!= ERROR_SUCCESS)
		goto error;
	size = sizeof(config_show);
	type = REG_DWORD;
	if (RegQueryValueEx(hKey, "show", NULL, &type, (BYTE *)&config_show, &size)
															!= ERROR_SUCCESS)
		goto error;
	/*
	 *	Original Registory
	 */
	RegCloseKey(hKey);
	openkey = FALSE;
	sprintf(name, "Software\\Vim\\%d", GuiConfig);
	if (RegOpenKeyEx(HKEY_CURRENT_USER, name, 0,
									KEY_ALL_ACCESS, &hKey) != ERROR_SUCCESS)
		goto error;
	openkey = TRUE;
	size = sizeof(config_w);
	type = REG_DWORD;
	if (RegQueryValueEx(hKey, "width", NULL, &type, (BYTE *)&config_w, &size)
															!= ERROR_SUCCESS)
		goto error;
	size = sizeof(config_h);
	type = REG_DWORD;
	if (RegQueryValueEx(hKey, "height", NULL, &type, (BYTE *)&config_h, &size)
															!= ERROR_SUCCESS)
		goto error;
	size = sizeof(Columns);
	type = REG_DWORD;
	if (RegQueryValueEx(hKey, "cols", NULL, &type, (BYTE *)&Columns, &size)
															!= ERROR_SUCCESS)
		goto error;
	size = sizeof(Rows);
	type = REG_DWORD;
	if (RegQueryValueEx(hKey, "rows", NULL, &type, (BYTE *)&Rows, &size)
															!= ERROR_SUCCESS)
		goto error;
	size = sizeof(config_fgcolor);
	type = REG_DWORD;
	if (RegQueryValueEx(hKey, "color-fg", NULL, &type, (BYTE *)&config_fgcolor, &size)
															!= ERROR_SUCCESS)
		goto error;
	size = sizeof(config_bgcolor);
	type = REG_DWORD;
	if (RegQueryValueEx(hKey, "color-bg", NULL, &type, (BYTE *)&config_bgcolor, &size)
															!= ERROR_SUCCESS)
		goto error;
	size = sizeof(config_tbcolor);
	type = REG_DWORD;
	if (RegQueryValueEx(hKey, "color-tb", NULL, &type, (BYTE *)&config_tbcolor, &size)
															!= ERROR_SUCCESS)
		goto error;
	size = sizeof(config_socolor);
	type = REG_DWORD;
	if (RegQueryValueEx(hKey, "color-so", NULL, &type, (BYTE *)&config_socolor, &size)
															!= ERROR_SUCCESS)
		goto error;
	size = sizeof(config_ticolor);
	type = REG_DWORD;
	if (RegQueryValueEx(hKey, "color-ti", NULL, &type, (BYTE *)&config_ticolor, &size)
															!= ERROR_SUCCESS)
		goto error;
	size = sizeof(config_tbbitmap);
	type = REG_DWORD;
	if (RegQueryValueEx(hKey, "bitmap-tb", NULL, &type, (BYTE *)&config_tbbitmap, &size)
															!= ERROR_SUCCESS)
		goto error;
	size = sizeof(config_sobitmap);
	type = REG_DWORD;
	if (RegQueryValueEx(hKey, "bitmap-so", NULL, &type, (BYTE *)&config_sobitmap, &size)
															!= ERROR_SUCCESS)
		goto error;
	size = sizeof(config_tibitmap);
	type = REG_DWORD;
	if (RegQueryValueEx(hKey, "bitmap-ti", NULL, &type, (BYTE *)&config_tibitmap, &size)
															!= ERROR_SUCCESS)
		goto error;
	/*
	 * "fontw" is the LOGFONTW; "font" is the LOGFONTA an older build wrote and
	 * still reads. Prefer the wide one and fall back, so a config from either
	 * works and a face name of more than ten Japanese characters survives.
	 */
	if (!font_load(hKey, "fontw", "font", &config_font))
		goto error;
#ifdef KANJI
	if (!font_load(hKey, "jfontw", "jfont", &config_jfont))
		goto error;
#endif
	size = sizeof(v_lspace);
	type = REG_DWORD;
	if (RegQueryValueEx(hKey, "lspace", NULL, &type, (BYTE *)&v_lspace, &size)
															!= ERROR_SUCCESS)
		goto error;
	size = sizeof(v_cspace);
	type = REG_DWORD;
	if (RegQueryValueEx(hKey, "cspace", NULL, &type, (BYTE *)&v_cspace, &size)
															!= ERROR_SUCCESS)
		goto error;
	size = sizeof(v_trans);
	type = REG_DWORD;
	if (RegQueryValueEx(hKey, "trans", NULL, &type, (BYTE *)&v_trans, &size)
															!= ERROR_SUCCESS)
		goto error;
	size = sizeof(config_bitmap);
	type = REG_DWORD;
	if (RegQueryValueEx(hKey, "bitmap", NULL, &type, (BYTE *)&config_bitmap, &size)
															!= ERROR_SUCCESS)
		goto error;
	size = sizeof(config_bitsize);
	type = REG_DWORD;
	if (RegQueryValueEx(hKey, "bitsize", NULL, &type, (BYTE *)&config_bitsize, &size)
															!= ERROR_SUCCESS)
		goto error;
	size = sizeof(config_bitcenter);
	type = REG_DWORD;
	if (RegQueryValueEx(hKey, "bitcenter", NULL, &type, (BYTE *)&config_bitcenter, &size)
															!= ERROR_SUCCESS)
		goto error;
	size = sizeof(config_bitmapfile);
	type = REG_SZ;
	if (RegGetStringU8(hKey, "bitmapfile", (char_u *)config_bitmapfile, sizeof(config_bitmapfile)) == FALSE)
		goto error;
	if (!isbitmap(config_bitmapfile, NULL))
	{
		config_bitmap = FALSE;
		config_bitmapfile[0] = '\0';
	}
	size = sizeof(config_wave);
	type = REG_DWORD;
	if (RegQueryValueEx(hKey, "wave", NULL, &type, (BYTE *)&config_wave, &size)
															!= ERROR_SUCCESS)
		goto error;
	size = sizeof(config_wavefile);
	type = REG_SZ;
	if (RegGetStringU8(hKey, "wavefile", (char_u *)config_wavefile, sizeof(config_wavefile)) == FALSE)
		goto error;
	if (!iswave(config_wavefile))
	{
		config_wave = FALSE;
		config_wavefile[0] = '\0';
	}
	size = sizeof(config_comb);
	type = REG_DWORD;
	if (RegQueryValueEx(hKey, "comb", NULL, &type, (BYTE *)&config_comb, &size)
															!= ERROR_SUCCESS)
		goto error;
	size = sizeof(config_load);
	type = REG_SZ;
	if (RegGetStringU8(hKey, "load", (char_u *)config_load, sizeof(config_load)) == FALSE)
		goto error;
	size = sizeof(config_unload);
	type = REG_SZ;
	if (RegGetStringU8(hKey, "unload", (char_u *)config_unload, sizeof(config_unload)) == FALSE)
		goto error;
	size = sizeof(config_save);
	type = REG_DWORD;
	if (RegQueryValueEx(hKey, "save", NULL, &type, (BYTE *)&config_save, &size)
															!= ERROR_SUCCESS)
		goto error;
	if (config_save)
	{
		size = sizeof(config_x);
		type = REG_DWORD;
		if (RegQueryValueEx(hKey, "posx", NULL, &type, (BYTE *)&config_x, &size)
															!= ERROR_SUCCESS)
			goto error;
		size = sizeof(config_y);
		type = REG_DWORD;
		if (RegQueryValueEx(hKey, "posy", NULL, &type, (BYTE *)&config_y, &size)
															!= ERROR_SUCCESS)
			goto error;
		if (config_w > (DWORD)GetSystemMetrics(SM_CXSCREEN))
			config_x = 1;
		if (config_h > (DWORD)GetSystemMetrics(SM_CYSCREEN))
			config_y = 1;
		if (config_x > 0x7fffffff)
			config_x = 1;
		if ((config_x & 0x7fffffff) > (DWORD)GetSystemMetrics(SM_CXSCREEN))
			config_x = 1;
		if (config_y > 0x7fffffff)
			config_y = 1;
		if ((config_y & 0x7fffffff) > (DWORD)GetSystemMetrics(SM_CYSCREEN))
			config_y = 1;
	}
	else
	{
		config_x = CW_USEDEFAULT;
		config_y = CW_USEDEFAULT;
	}
	/*
	 * Last, and read leniently: "dpi" is not in a key that a JVim from before
	 * the DPI aware manifest wrote, and a missing value there means 96, not a
	 * key to throw away and start again from the defaults.
	 */
	{
		DWORD		dpi = 96;

		size = sizeof(dpi);
		type = REG_DWORD;
		if (RegQueryValueEx(hKey, "dpi", NULL, &type, (BYTE *)&dpi, &size)
											!= ERROR_SUCCESS || dpi == 0)
			dpi = 96;
		config_dpi = (int)dpi;
	}
	RegCloseKey(hKey);
	return;
error:
	if (openkey)
		RegCloseKey(hKey);
	if (!init && !BenchTime)
		return;
	Columns	= 80;
	Rows	= 25;
	config_x = CW_USEDEFAULT;
	config_y = CW_USEDEFAULT;
	if (BenchTime)
		config_x = config_y = 1;
	config_w = CW_USEDEFAULT;
	config_h = CW_USEDEFAULT;

	v_cspace		= 0;
	v_lspace		= 0;
	v_trans			= 0;
	config_sbar		= TRUE;
	config_fadeout	= TRUE;
	config_grepwin	= TRUE;
#ifdef USE_HISTORY
	config_history	= TRUE;
	config_hauto	= TRUE;
#endif
	config_save		= FALSE;
	config_comb		= FALSE;
	config_load[0]	= '\0';
	config_unload[0]= '\0';
	config_tray		= FALSE;
	config_mouse	= FALSE;
#ifdef NT106KEY
	config_nt106	= TRUE;
#endif
	config_menu		= TRUE;
	config_dpi		= 96;		/* the sizes below are the 96 DPI ones */
	config_font.lfHeight		= 14;
	config_font.lfWidth			= 0;
	config_font.lfEscapement	= 0;
	config_font.lfOrientation	= 0;
	config_font.lfWeight		= 0;
	config_font.lfItalic		= 0;
	config_font.lfUnderline		= 0;
	config_font.lfStrikeOut		= 0;
	/* config_font.lfCharSet	= OEM_CHARSET; */
	config_font.lfCharSet		= SHIFTJIS_CHARSET;
	config_font.lfOutPrecision	= OUT_DEFAULT_PRECIS;
	config_font.lfClipPrecision = CLIP_DEFAULT_PRECIS;
	config_font.lfQuality		= DEFAULT_QUALITY;
	config_font.lfPitchAndFamily= FIXED_PITCH | FF_MODERN;
	lstrcpyW(config_font.lfFaceName, L"FixedSys");
#ifdef KANJI
	config_jfont.lfHeight		= 14;
	config_jfont.lfWidth		= 0;
	config_jfont.lfEscapement	= 0;
	config_jfont.lfOrientation	= 0;
	config_jfont.lfWeight		= 0;
	config_jfont.lfItalic		= 0;
	config_jfont.lfUnderline	= 0;
	config_jfont.lfStrikeOut	= 0;
	/* config_jfont.lfCharSet	= OEM_CHARSET; */
	config_jfont.lfCharSet		= SHIFTJIS_CHARSET;
	config_jfont.lfOutPrecision	= OUT_DEFAULT_PRECIS;
	config_jfont.lfClipPrecision = CLIP_DEFAULT_PRECIS;
	config_jfont.lfQuality		= DEFAULT_QUALITY;
	config_jfont.lfPitchAndFamily= FIXED_PITCH | FF_MODERN;
	lstrcpyW(config_jfont.lfFaceName, L"FixedSys");
#endif
	config_fgcolor	= RGB(  0,   0,   0);
	config_bgcolor	= RGB(255, 255, 255);
	config_tbcolor	= (-1);
	config_socolor	= (-1);
	config_ticolor	= (-1);
	config_tbbitmap	= (-1);
	config_sobitmap	= (-1);
	config_tibitmap	= (-1);
	config_printer[0] = '\0';
	config_bitmap	= FALSE;
	config_bitsize	= 100;
	config_bitcenter= TRUE;
	config_bitmapfile[0] = '\0';
	if (BenchTime && GuiConfig)
	{
		sprintf(name, "Software\\Vim\\%d", GuiConfig);
		if (RegOpenKeyEx(HKEY_CURRENT_USER, name, 0,
										KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS)
		{
			size = sizeof(config_bitmap);
			type = REG_DWORD;
			RegQueryValueEx(hKey, "bitmap", NULL, &type, (BYTE *)&config_bitmap, &size);
			size = sizeof(config_bitsize);
			type = REG_DWORD;
			RegQueryValueEx(hKey, "bitsize", NULL, &type, (BYTE *)&config_bitsize, &size);
			size = sizeof(config_bitcenter);
			type = REG_DWORD;
			RegQueryValueEx(hKey, "bitcenter", NULL, &type, (BYTE *)&config_bitcenter, &size);
			size = sizeof(config_bitmapfile);
			type = REG_SZ;
			RegGetStringU8(hKey, "bitmapfile", (char_u *)config_bitmapfile, sizeof(config_bitmapfile));
			if (!isbitmap(config_bitmapfile, NULL))
			{
				config_bitmap = FALSE;
				config_bitmapfile[0] = '\0';
			}
			RegCloseKey(hKey);
		}
	}
	config_wave		= FALSE;
	config_wavefile[0] = '\0';
	config_overflow	= 3;
	config_show		= 500;
	if (BenchTime)
	{
		char_u	font[] = {	0xf5, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00,
							0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
							0x90, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80,
							0x03, 0x02, 0x01, 0x31, 0x82, 0x6c, 0x82, 0x72,
							0x20, 0x83, 0x53, 0x83, 0x56, 0x83, 0x62, 0x83,
							0x4e, 0x00, 0x00, 0x00, 0x54, 0x67, 0x02, 0x00,
							0xdb, 0x0f, 0x78, 0x7b, 0x2f, 0x13, 0x97, 0x27,
							0x00, 0xa0, 0x00, 0x00};
		{
			/* the blob is a LOGFONTA, with its face name in CP932 */
			LOGFONTA	narrow;

			memcpy(&narrow, font, sizeof(narrow));
			font_widen(&config_font, &narrow);
#ifdef KANJI
			font_widen(&config_jfont, &narrow);
#endif
		}
	}
}

static VOID
SaveConfig(void)
{
	HKEY		hKey;
	DWORD		size;
	int			openkey = FALSE;
	RECT		rcWindow;
	char		name[_MAX_PATH];

	if (BenchTime || config_ini)
		return;
	if (GetWindowRect(hVimWnd, &rcWindow))
	{
		config_x = rcWindow.left;
		config_y = rcWindow.top;
	}
	else
	{
		config_x = CW_USEDEFAULT;
		config_y = CW_USEDEFAULT;
	}
	/*
	 *	Common Registory
	 */
	if (RegCreateKeyEx(HKEY_CURRENT_USER, "Software\\Vim", 0, NULL,
			REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, &size)
															!= ERROR_SUCCESS)
		goto error;
	openkey = TRUE;
#ifdef KANJI
	{
		size = strlen(longJpVersion) + 1;
		if (RegSetValueEx(hKey, NULL, 0, REG_SZ, longJpVersion, size)
															!= ERROR_SUCCESS)
			goto error;
	}
#endif
	size = strlen(config_printer) + 1;
	if (RegSetStringU8(hKey, "printer", (char_u *)config_printer)
															!= ERROR_SUCCESS)
		goto error;
	size = sizeof(config_tray);
	if (RegSetValueEx(hKey, "tray", 0, REG_DWORD, (BYTE *)&config_tray, size)
															!= ERROR_SUCCESS)
		goto error;
	size = sizeof(config_mouse);
	if (RegSetValueEx(hKey, "mouse", 0, REG_DWORD, (BYTE *)&config_mouse, size)
															!= ERROR_SUCCESS)
		goto error;
#ifdef NT106KEY
	size = sizeof(config_nt106);
	if (RegSetValueEx(hKey, "nt106", 0, REG_DWORD, (BYTE *)&config_nt106, size)
															!= ERROR_SUCCESS)
		goto error;
#endif
	size = sizeof(config_menu);
	if (RegSetValueEx(hKey, "menu", 0, REG_DWORD, (BYTE *)&config_menu, size)
															!= ERROR_SUCCESS)
		goto error;
	size = sizeof(config_sbar);
	if (RegSetValueEx(hKey, "scrollbar", 0, REG_DWORD, (BYTE *)&config_sbar, size)
															!= ERROR_SUCCESS)
		goto error;
	size = sizeof(config_fadeout);
	if (RegSetValueEx(hKey, "fadeout", 0, REG_DWORD, (BYTE *)&config_fadeout, size)
															!= ERROR_SUCCESS)
		goto error;
	size = sizeof(config_grepwin);
	if (RegSetValueEx(hKey, "grepwin", 0, REG_DWORD, (BYTE *)&config_grepwin, size)
															!= ERROR_SUCCESS)
		goto error;
#ifdef USE_HISTORY
	size = sizeof(config_history);
	if (RegSetValueEx(hKey, "history", 0, REG_DWORD, (BYTE *)&config_history, size)
															!= ERROR_SUCCESS)
		goto error;
	size = sizeof(config_hauto);
	if (RegSetValueEx(hKey, "hauto", 0, REG_DWORD, (BYTE *)&config_hauto, size)
															!= ERROR_SUCCESS)
		goto error;
#endif
	size = sizeof(config_color);
	if (RegSetValueEx(hKey, "custcolor", 0, REG_BINARY, (BYTE *)config_color, size)
															!= ERROR_SUCCESS)
		goto error;
	size = sizeof(config_overflow);
	if (RegSetValueEx(hKey, "overflow", 0, REG_DWORD, (BYTE *)&config_overflow, size)
															!= ERROR_SUCCESS)
		goto error;
	size = sizeof(config_show);
	if (RegSetValueEx(hKey, "show", 0, REG_DWORD, (BYTE *)&config_show, size)
															!= ERROR_SUCCESS)
		goto error;
	/*
	 *	Original Registory
	 */
	RegCloseKey(hKey);
	openkey = FALSE;
	sprintf(name, "Software\\Vim\\%d", GuiConfig);
	if (RegCreateKeyEx(HKEY_CURRENT_USER, name, 0, NULL,
			REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, &size)
														!= ERROR_SUCCESS)
		goto error;
	openkey = TRUE;
	{
		GetDateFormat(LOCALE_USER_DEFAULT, 0, NULL, NULL, name, sizeof(name));
		strcat(name, " ");
		GetTimeFormat(LOCALE_USER_DEFAULT, 0, NULL, NULL, &name[strlen(name)], sizeof(name) - strlen(name));
		size = strlen(name) + 1;
		if (RegSetValueEx(hKey, NULL, 0, REG_SZ, name, size) != ERROR_SUCCESS)
			goto error;
	}
	size = sizeof(config_w);
	if (RegSetValueEx(hKey, "width", 0, REG_DWORD, (BYTE *)&config_w, size)
															!= ERROR_SUCCESS)
		goto error;
	size = sizeof(config_h);
	if (RegSetValueEx(hKey, "height", 0, REG_DWORD, (BYTE *)&config_h, size)
															!= ERROR_SUCCESS)
		goto error;
	size = sizeof(Columns);
	if (RegSetValueEx(hKey, "cols", 0, REG_DWORD, (BYTE *)&Columns, size)
															!= ERROR_SUCCESS)
		goto error;
	size = sizeof(Rows);
	if (RegSetValueEx(hKey, "rows", 0, REG_DWORD, (BYTE *)&Rows, size)
															!= ERROR_SUCCESS)
		goto error;
	{
		/* What "width", "height" and "font" below are pixel sizes for. */
		DWORD		dpi = (DWORD)config_dpi;

		size = sizeof(dpi);
		if (RegSetValueEx(hKey, "dpi", 0, REG_DWORD, (BYTE *)&dpi, size)
															!= ERROR_SUCCESS)
			goto error;
	}
	size = sizeof(config_fgcolor);
	if (RegSetValueEx(hKey, "color-fg", 0, REG_DWORD, (BYTE *)&config_fgcolor, size)
															!= ERROR_SUCCESS)
		goto error;
	size = sizeof(config_bgcolor);
	if (RegSetValueEx(hKey, "color-bg", 0, REG_DWORD, (BYTE *)&config_bgcolor, size)
															!= ERROR_SUCCESS)
		goto error;
	size = sizeof(config_tbcolor);
	if (RegSetValueEx(hKey, "color-tb", 0, REG_DWORD, (BYTE *)&config_tbcolor, size)
															!= ERROR_SUCCESS)
		goto error;
	size = sizeof(config_socolor);
	if (RegSetValueEx(hKey, "color-so", 0, REG_DWORD, (BYTE *)&config_socolor, size)
															!= ERROR_SUCCESS)
		goto error;
	size = sizeof(config_ticolor);
	if (RegSetValueEx(hKey, "color-ti", 0, REG_DWORD, (BYTE *)&config_ticolor, size)
															!= ERROR_SUCCESS)
		goto error;
	size = sizeof(config_tbbitmap);
	if (RegSetValueEx(hKey, "bitmap-tb", 0, REG_DWORD, (BYTE *)&config_tbbitmap, size)
															!= ERROR_SUCCESS)
		goto error;
	size = sizeof(config_sobitmap);
	if (RegSetValueEx(hKey, "bitmap-so", 0, REG_DWORD, (BYTE *)&config_sobitmap, size)
															!= ERROR_SUCCESS)
		goto error;
	size = sizeof(config_tibitmap);
	if (RegSetValueEx(hKey, "bitmap-ti", 0, REG_DWORD, (BYTE *)&config_tibitmap, size)
															!= ERROR_SUCCESS)
		goto error;
	/* both forms: the wide one is what this build reads back, the narrow one is
	 * for an older build that only knows "font" */
	if (!font_save(hKey, "fontw", "font", &config_font))
		goto error;
#ifdef KANJI
	if (!font_save(hKey, "jfontw", "jfont", &config_jfont))
		goto error;
#endif
	size = sizeof(v_lspace);
	if (RegSetValueEx(hKey, "lspace", 0, REG_DWORD, (BYTE *)&v_lspace, size)
															!= ERROR_SUCCESS)
		goto error;
	size = sizeof(v_cspace);
	if (RegSetValueEx(hKey, "cspace", 0, REG_DWORD, (BYTE *)&v_cspace, size)
															!= ERROR_SUCCESS)
		goto error;
	size = sizeof(v_trans);
	if (RegSetValueEx(hKey, "trans", 0, REG_DWORD, (BYTE *)&v_trans, size)
															!= ERROR_SUCCESS)
		goto error;
	size = sizeof(config_bitmap);
	if (RegSetValueEx(hKey, "bitmap", 0, REG_DWORD, (BYTE *)&config_bitmap, size)
															!= ERROR_SUCCESS)
		goto error;
	size = sizeof(config_bitsize);
	if (RegSetValueEx(hKey, "bitsize", 0, REG_DWORD, (BYTE *)&config_bitsize, size)
															!= ERROR_SUCCESS)
		goto error;
	size = sizeof(config_bitcenter);
	if (RegSetValueEx(hKey, "bitcenter", 0, REG_DWORD, (BYTE *)&config_bitcenter, size)
															!= ERROR_SUCCESS)
		goto error;
	size = strlen(config_bitmapfile) + 1;
	if (RegSetStringU8(hKey, "bitmapfile", (char_u *)config_bitmapfile)
															!= ERROR_SUCCESS)
		goto error;
	size = sizeof(config_wave);
	if (RegSetValueEx(hKey, "wave", 0, REG_DWORD, (BYTE *)&config_wave, size)
															!= ERROR_SUCCESS)
		goto error;
	size = strlen(config_wavefile) + 1;
	if (RegSetStringU8(hKey, "wavefile", (char_u *)config_wavefile)
															!= ERROR_SUCCESS)
		goto error;
	size = sizeof(config_comb);
	if (RegSetValueEx(hKey, "comb", 0, REG_DWORD, (BYTE *)&config_comb, size)
															!= ERROR_SUCCESS)
		goto error;
	size = strlen(config_load) + 1;
	if (RegSetStringU8(hKey, "load", (char_u *)config_load)
															!= ERROR_SUCCESS)
		goto error;
	size = strlen(config_unload) + 1;
	if (RegSetStringU8(hKey, "unload", (char_u *)config_unload)
															!= ERROR_SUCCESS)
		goto error;
	size = sizeof(config_save);
	if (RegSetValueEx(hKey, "save", 0, REG_DWORD, (BYTE *)&config_save, size)
															!= ERROR_SUCCESS)
		goto error;
	size = sizeof(config_x);
	if (RegSetValueEx(hKey, "posx", 0, REG_DWORD, (BYTE *)&config_x, size)
															!= ERROR_SUCCESS)
		goto error;
	size = sizeof(config_y);
	if (RegSetValueEx(hKey, "posy", 0, REG_DWORD, (BYTE *)&config_y, size)
															!= ERROR_SUCCESS)
		goto error;
	RegCloseKey(hKey);
	return;
error:
	if (openkey)
		RegCloseKey(hKey);
}

/*
 * Called by syn_highlight() (syntax.c) whenever a colour scheme sets or
 * clears "hi Normal". Repoints v_fgcolor/v_bgcolor at the scheme's own
 * colours -- the same trick already used to switch v_tbcolor etc. between a
 * solid colour and a bitmap -- so the Text/Back Color the user configured is
 * used unless and until a scheme overrides it, and is never itself changed by
 * one. The caller still has to trigger the redraw (updateScreen(CLEAR)).
 */
void
syn_win_apply_normal(void)
{
	if (syn_normal_fg >= 0)
	{
		scheme_fgcolor = RGB((syn_normal_fg >> 16) & 0xff,
							 (syn_normal_fg >> 8)  & 0xff,
							  syn_normal_fg		   & 0xff);
		v_fgcolor = &scheme_fgcolor;
	}
	else
		v_fgcolor = &config_fgcolor;

	if (syn_normal_bg >= 0)
	{
		scheme_bgcolor = RGB((syn_normal_bg >> 16) & 0xff,
							 (syn_normal_bg >> 8)  & 0xff,
							  syn_normal_bg		   & 0xff);
		v_bgcolor = &scheme_bgcolor;
	}
	else
		v_bgcolor = &config_bgcolor;
}

static BOOL
ResetScreen(HWND hWnd)
{
	HDC         hDC;
	TEXTMETRIC  tm;

	if (NULL != v_font)
		DeleteObject(v_font);

	v_font = CreateFontIndirectW(&config_font);

	hDC = GetDC(hWnd);
	SelectObject(hDC, v_font);
	GetTextMetrics(hDC, &tm);
	ReleaseDC(hWnd, hDC);

	if (tm.tmPitchAndFamily & TMPF_TRUETYPE)
		v_ttfont = TRUE;
	else
		v_ttfont = FALSE;
	if (tm.tmPitchAndFamily & TMPF_FIXED_PITCH)
		v_xchar = (tm.tmMaxCharWidth * 1 + tm.tmAveCharWidth * 2) / 3;
	else
		v_xchar = tm.tmAveCharWidth;
	v_ychar = tm.tmHeight + tm.tmExternalLeading;
#ifdef KANJI
	{
		DeleteObject(v_font);
		v_font = CreateFontIndirectW(&config_jfont);

		hDC = GetDC(hWnd);
		SelectObject(hDC, v_font);
		GetTextMetrics(hDC, &tm);
		ReleaseDC(hWnd, hDC);

		v_difffont = memcmp(&config_font, &config_jfont, sizeof(config_font));

		if (tm.tmPitchAndFamily & TMPF_TRUETYPE)
			v_ttfont = TRUE;
		if (tm.tmPitchAndFamily & TMPF_FIXED_PITCH)
			v_xchar = (tm.tmMaxCharWidth * 1 + tm.tmAveCharWidth * 2) / 3 > v_xchar ? (tm.tmMaxCharWidth * 1 + tm.tmAveCharWidth * 2) / 3 : v_xchar;
		else
			v_xchar = tm.tmAveCharWidth > v_xchar ? tm.tmAveCharWidth : v_xchar;
		v_ychar = tm.tmHeight + tm.tmExternalLeading > v_ychar ? tm.tmHeight + tm.tmExternalLeading : v_ychar;

		DeleteObject(v_font);
		v_font = CreateFontIndirectW(&config_font);
	}
#endif
	v_xchar += v_cspace;
	v_ychar += v_lspace;

	do_resize = TRUE;
	if (v_cursor && v_focus)
		HideCaret(hWnd);
	v_caret = 0;
	return(TRUE);
}

/*
 * Append the staged character to the key buffer.
 */
	static void
push_wchar(void)
{
	int		i;

	for (i = 0; i < wc_len; i++)
		cbuf[c_end++] = wc_bytes[i];
}

/*
 * Which cell does the character covering 'col' start at?
 */
	static int
cell_head(int row, int col)
{
	if (col > 0 && CELLCONT(row, col))
		return col - 1;
	return col;
}

/*
 * Character class of the character in a cell, or -1 when the cell holds a plain
 * byte rather than a multi-byte character.
 */
	static int
cell_class(int row, int col)
{
	int		cp = CELLCP(row, cell_head(row, col));

	return cp > 0 ? jpclscp(cp) : -1;
}

/*
 * Cells covered by the character starting at 'col'.
 */
	static int
cell_width(int row, int col)
{
	return CELLWIDE(row, col) ? 2 : 1;
}

static VOID
MoveCursor(HWND hWnd)
{
	int			width;

	if (v_cursor && v_focus)
	{
#ifdef KANJI
		if (WinScreen == NULL || v_row >= Rows)
			width = 1;
		else
			width = CELLWIDE(v_row, v_col) ? 2 : 1;
#else
		width = 1;
#endif
		if (width != v_caret)
		{
			CreateCaret(hWnd, NULL, width * (v_xchar - v_cspace), v_ychar - v_lspace);
			ShowCaret(hWnd);
			v_caret = width;
		}
	}
	SetCaretPos(v_col * v_xchar, v_row * v_ychar);
}

static INT
WindowSize(HWND hWnd, WORD wVertSize, WORD wHorzSize)
{
	RECT		rcClient;
	int			i;

	if (!IsIconic(hWnd) && GetClientRect(hWnd, &rcClient))
	{
		do_resize = TRUE;
		nowRows = (rcClient.bottom - rcClient.top) / v_ychar;
		nowCols = (rcClient.right - rcClient.left) / v_xchar;
	}
	else
	{
		nowRows = Rows;
		nowCols = Columns;
	}
	if (nowCols > v_ssize)
	{
		/* Two UTF-16 units per cell, for characters outside the BMP. */
		v_ssize = nowCols;
		free(v_space);
		v_space = malloc(sizeof(INT) * nowCols * 2);
		free(v_char);
		v_char = malloc(sizeof(short) * nowCols * 2);
	}
	for (i = 0; i < v_ssize; i++)
		v_space[i] = v_xchar;
	return(0);
}

#ifdef KANJI
static void
SetFontType(char_u *c, char_u mode, HDC hDC, HFONT *phOldFont)
{
	LOGFONTW		logfont;

	if (c != NULL && iskanakan(*c))
		memcpy(&logfont, &config_jfont, sizeof(logfont));
	else
		memcpy(&logfont, &config_font, sizeof(logfont));
	SelectObject(hDC, *phOldFont);
	if (mode >= 0x80)
	{
			 if (0x80 <= mode && mode <= 0x9f)	mode = 1;
		else if (0xa0 <= mode && mode <= 0xbf)	mode = 2;
		else if (0xc0 <= mode && mode <= 0xdf)	mode = 3;
		else if (0xe0 <= mode && mode <= 0xff)	mode = 4;
		else									mode = 0;
	}
	if (NULL != v_font)
		DeleteObject(v_font);
	switch (mode) {
	case 1:
		logfont.lfItalic		= 0;
		logfont.lfUnderline		= 0;
		logfont.lfWeight		= FW_BOLD;
		break;
	case 2:
		logfont.lfItalic		= 1;
		logfont.lfUnderline		= 0;
		logfont.lfWeight		= FW_NORMAL;
		break;
	case 3:
		logfont.lfItalic		= 0;
		logfont.lfUnderline		= 1;
		logfont.lfWeight		= FW_NORMAL;
		break;
	case 4:
		logfont.lfItalic		= 1;
		logfont.lfUnderline		= 0;
		logfont.lfWeight		= FW_BOLD;
		break;
	default:
		logfont.lfItalic		= 0;
		logfont.lfUnderline		= 0;
		logfont.lfWeight		= FW_NORMAL;
		break;
	}
	v_font = CreateFontIndirectW(&logfont);
	*phOldFont = SelectObject(hDC, v_font);
}

static DWORD
GetColor(char_u mode, int tb)
{
	int			color;
	DWORD		red;
	DWORD		green;
	DWORD		blue;
	DWORD		rgb;
	char_u	*	p = NULL;

	if (tb == 't')
		rgb = *v_fgcolor;
	else
		rgb = *v_bgcolor;
	if (mode >= 0x80)
	{
			 if (0x80 <= mode && mode <= 0x9f) mode -= 0x40;
		else if (0xa0 <= mode && mode <= 0xbf) mode -= 0x60;
		else if (0xc0 <= mode && mode <= 0xdf) mode -= 0x80;
		else if (0xe0 <= mode && mode <= 0xff) mode -= 0xa0;
	}
	switch (mode & 0x7f) {
	case 'b':	/* bold */
		if (*v_tbcolor != (-1))
		{
			if (tb == 't')
				rgb = *v_tbcolor;
			else
				rgb = *v_socolor;
		}
		else
			p = T_TB;
		break;
	case 's':	/* standout */
		if (*v_socolor != (-1))
		{
			if (tb == 't')
				rgb = *v_socolor;
			else
				rgb = *v_tbcolor;
		}
		else
			p = T_SO;
		break;
	default:
#ifdef USE_SYNTAX
		{
			/*
			 * A colour a syntax rule asked for. The palette itself lives in
			 * syn_decode(), because a terminal paints the same ids as SGR
			 * escapes and the two must not be able to drift apart.
			 */
			int		syn;
			int		syn_rgb = 0;

			syn = syn_decode(mode & 0x7f, &syn_rgb);
			if (syn & SYN_TEXT)
				break;				/* leave the ordinary colours alone */
			if (syn & SYN_RGB)
			{
				if (tb == 't')
					rgb = RGB((syn_rgb >> 16) & 0xff, (syn_rgb >> 8) & 0xff,
															syn_rgb & 0xff);
				break;
			}
			if ((syn & SYN_REVERSE) && !config_bitmap)
			{
				if (tb == 't')
					rgb = *v_bgcolor;
				else
					rgb = *v_fgcolor;
				break;
			}
		}
		/* no rule named this, or reverse with a bitmap behind it */
#endif
		/* invert/reverse */
		if (*v_ticolor != (-1))
		{
			if (tb == 't')
				rgb = *v_ticolor;
			else
				rgb = *v_tbcolor;
		}
		else
			p = T_TI;
		break;
	}
	if (p != NULL && *p != NUL)
	{
		p += 2;
		color = getdigits(&p);
		if (tb != 't')
			color = (color & 0xf0) >> 4;
		red = green = blue = 0;
		if (color & FOREGROUND_BLUE)
			blue = 255;
		if (color & FOREGROUND_GREEN)
			green = 255;
		if (color & FOREGROUND_RED)
			red = 255;
		if ((color & FOREGROUND_INTENSITY) == 0)
		{
			blue	= (blue  == 0 ? 0 : 160);
			green	= (green == 0 ? 0 : 160);
			red		= (red   == 0 ? 0 : 160);
		}
		rgb = RGB(red, green, blue);
	}
	return(rgb);
}
#endif

/*
 * The colour behind a cell, from its two attribute bytes. A colour a rule
 * asked to draw on if there is one; otherwise what the foreground implies,
 * which is the text colour for reverse and the window's own for everything
 * else. Every place that paints a background asks here, so that the run pass
 * over a row and the run inside PrintChar() cannot answer differently.
 *
 * 'do_vb' is the whole window inverted for a visual bell. That swaps the two
 * colours the window was given, and not one a rule named: a rule asking for
 * maroon means maroon either way round.
 */
static DWORD
run_bkcolor(char_u mode, char_u bgmode)
{
#if defined(KANJI) && defined(USE_SYNTAX)
	int			rgb = 0;

	if (syn_bgcolor(bgmode, &rgb))
		return(RGB((rgb >> 16) & 0xff, (rgb >> 8) & 0xff, rgb & 0xff));
#endif
#ifdef KANJI
	if (mode)
		return(do_vb ? GetColor(mode, 't') : GetColor(mode, 'b'));
#else
	if (mode & 0x80)
		return(do_vb ? *v_bgcolor : *v_fgcolor);
#endif
	return(do_vb ? *v_fgcolor : *v_bgcolor);
}

static void
PrintChar(HDC hdc, RECT *rt, HFONT *phOldFont, char_u *p, int size, char_u mode,
			char_u bgmode, int row, int col)
{
#if defined(KANJI) && defined(SYNTAX)
	HBRUSH		hbrush;
	HBRUSH		holdbrush;
	DWORD		color;

	if (issynpaint())
	{
		color = run_bkcolor(mode, bgmode);
		if ((!do_vb && *v_bgcolor != color) || (do_vb && *v_fgcolor != color))
		{
			hbrush	= CreateSolidBrush(color);
			holdbrush = SelectObject(hdc, hbrush);
			FillRect(hdc, rt, hbrush);
			SelectObject(hdc, holdbrush);
			DeleteObject(hbrush);
		}
	}
	if (syntax_on())
		SetFontType(p, mode, hdc, phOldFont);
	else if (v_difffont)
	{
		SelectObject(hdc, *phOldFont);
		if (NULL != v_font)
			DeleteObject(v_font);
		if (p != NULL && CELLKANA(row, col))
			v_font = CreateFontIndirectW(&config_jfont);
		else
			v_font = CreateFontIndirectW(&config_font);
		*phOldFont = SelectObject(hdc, v_font);
	}
#endif
	{
		/*
		 * Glyphs only. PaintWindow() has already laid the background of the
		 * whole row down, before any of its runs were drawn, so that nothing
		 * painted here can erase ink that is already on the screen.
		 */
		int		i;
		int		n = 0;				/* UTF-16 units staged in v_char */
		int		x = rt->left;		/* where the staged units start */
		int		adv = 0;			/* and how wide they will come out */

		/*
		 * A run of cells goes out as few ExtTextOut calls as it can, with
		 * v_space holding one advance per cell so that a double width
		 * character gets both of its cells and everything lands on the
		 * character grid.
		 *
		 * A character outside the BMP breaks that and has to go on its own.
		 * lpDx is documented as one entry per character of the string, but GDI
		 * applies it per *glyph*, and a surrogate pair is two characters that
		 * become one glyph. So the entry meant for the character after the
		 * pair was eaten by the pair itself: that character advanced by the
		 * zero belonging to the low surrogate, landing on top of its
		 * neighbour, and every entry after that was off by one place. Mostly
		 * they hold the same number so it very nearly looked right, but the
		 * line came out a cell short and its tail sat half a character to the
		 * left of the grid -- which is why an emoji, and the end of any line
		 * holding one, appeared to have lost its right half, while a line of
		 * nothing but kanji was fine.
		 *
		 * Drawn by itself at a known x, with no lpDx to misread, the glyph
		 * gets the advance its own font intends and we go on from where the
		 * grid says the next cell starts.
		 */
		for (i = 0; i < size; i++)
		{
			int		cp = CELLCP(row, col + i);
			int		wid;

			if (cp == -1)
				continue;				/* right half, drawn with its left */
			if (cp <= 0)
			{							/* the character plane byte is it */
				v_char[n] = (short)(unsigned char)p[i];
				v_space[n] = v_xchar;
				n++;
				adv += v_xchar;
				continue;
			}
			wid = utf_cpwidth(cp);
			if (wid < 1)
				wid = 1;
			if (cp > 0xffff)
			{
				WCHAR	pair[2];
				int		v = cp - 0x10000;

				if (n > 0)
				{
					ExtTextOutW(hdc, x, rt->top, 0, NULL,
									(LPCWSTR)v_char, n, v_space);
					x += adv;
					n = 0;
					adv = 0;
				}
				pair[0] = (WCHAR)(0xd800 + (v >> 10));
				pair[1] = (WCHAR)(0xdc00 + (v & 0x3ff));
				ExtTextOutW(hdc, x, rt->top, 0, NULL, pair, 2, NULL);
				x += wid * v_xchar;
				continue;
			}
			v_char[n] = (short)cp;
			v_space[n] = wid * v_xchar;
			n++;
			adv += wid * v_xchar;
		}
		if (n > 0)
			ExtTextOutW(hdc, x, rt->top, 0, NULL,
							(LPCWSTR)v_char, n, v_space);
	}
}

static BOOL
PaintWindow(HWND hWnd)
{
	int				nRow, nCol, nEndRow, nEndCol;
	HDC				hDC;
	HFONT			hOldFont;
	PAINTSTRUCT		ps;
	RECT			rect;
	char_u		*	p;
	int				caret = FALSE;

	if (GetUpdateRect(hWnd, &rect, FALSE) != TRUE)
		return(0);

	hDC		= BeginPaint(hWnd, &ps);
	rect	= ps.rcPaint;
	hOldFont= SelectObject(hDC, v_font);

	/*
	 * TRANSPARENT throughout, because PrintChar() paints the background of a
	 * run itself as one rectangle. What it needs to know is whether anything
	 * else has already put something there: a background bitmap that really
	 * did load, or the fill that issynpaint() does below and per run.
	 */
	v_bmpon = config_bitmap
					&& LoadBitmapFromBMPFile(hDC, config_bitmapfile);
	SetBkMode(hDC, TRANSPARENT);

	if (!screen_valid())
		goto no_draw;

	nRow	= min(Rows - 1, max(0, rect.top / v_ychar));
	nEndRow	= min(Rows - 1, ((rect.bottom - 1) / v_ychar));
#ifdef KANJI
	/*
	 * A cell to either side of the damage as well. The left one has always
	 * been needed, to catch the left half of a double width character whose
	 * right half is the first dirty cell. The right one is for ink: a glyph
	 * may reach past the cells it was given, and since the background is no
	 * longer painted a cell at a time there is nothing to hide the part of a
	 * neighbour that leans in here.
	 */
	nCol	= min(Columns - 1, max(0, (rect.left - v_xchar) / v_xchar));
	nEndCol	= min(Columns - 1, ((rect.right - 1) / v_xchar) + 1);
#else
	nCol	= min(Columns - 1, max(0, rect.left / v_xchar));
	nEndCol	= min(Columns - 1, ((rect.right - 1) / v_xchar));
#endif
	if (v_cursor && v_focus)
	{
		HideCaret(hWnd);
		caret = TRUE;
	}
#if defined(KANJI) && defined(SYNTAX)
	if (syntax_on() && !v_ttfont
			&& !config_bitmap)
	{
		HBRUSH		hbrush;
		HBRUSH		holdbrush;

		rect.top	= nRow * v_ychar;
		rect.bottom	= nEndRow * v_ychar + v_ychar;
		rect.left	= nCol * v_xchar;
		rect.right	= nEndCol * v_xchar + v_xchar;
		if (do_vb)
			hbrush	= CreateSolidBrush(*v_fgcolor);
		else
			hbrush	= CreateSolidBrush(*v_bgcolor);
		holdbrush = SelectObject(hDC, hbrush);
		FillRect(hDC, &rect, hbrush);
		SelectObject(hDC, holdbrush);
		DeleteObject(hbrush);
	}
#endif
	for (; nRow <= nEndRow; nRow++)
	{
		int			i;
		int			i0;
		char_u		attr;
#if defined(KANJI) && defined(SYNTAX)
		/*
		 * The run's background, which breaks the run only where PrintChar()
		 * is the one painting it. Where the pass above the loop does, the
		 * glyphs go over it transparently and two runs that differ in
		 * nothing but their ground can be drawn as one.
		 */
		char_u		bgattr;
#endif
#ifdef KANJI
		int			kanji;
#endif

		rect.top	= nRow * v_ychar;
		rect.bottom	= nRow * v_ychar + v_ychar;
		rect.left	= nCol * v_xchar;

		p = WinScreen[nRow];
		/*
		 * The background of the row goes down first, all of it, before a
		 * single glyph of the row is drawn.
		 *
		 * It used to be part of drawing each run, and that cannot work: the
		 * loop below starts a new run wherever the attribute changes *or* the
		 * text crosses between ASCII and a multi-byte character, so a line of
		 * Japanese is chopped into a good many of them, and the blanks after
		 * the last word are a run of their own. Ink that leans out of the
		 * cells a run was given -- the right edge of a kana, an emoji from a
		 * fallback font -- was painted out by the background of the run after
		 * it. Which is why the last character of a line lost its right side
		 * while a line ending in a full stop, whose glyph sits well inside its
		 * cell, looked perfectly fine.
		 *
		 * Spans of one colour are filled together, so this is normally one or
		 * two FillRect calls for the row.
		 */
		if (!v_bmpon && !issynpaint())
		{
			int			c0 = nCol;
			int			c;

			for (c = nCol; c <= nEndCol + 1; c++)
			{
#ifdef KANJI
				/* both planes: two runs can share a colour and not a ground */
				if (c <= nEndCol && p[Columns + c] == p[Columns + c0]
						&& p[Columns * 2 + c] == p[Columns * 2 + c0])
#else
				if (c <= nEndCol && (p[c] & 0x80) == (p[c0] & 0x80))
#endif
					continue;
				{
					RECT		fr;
					HBRUSH		hbrush;

					fr.top		= rect.top;
					fr.bottom	= rect.bottom;
					fr.left		= c0 * v_xchar;
					fr.right	= c * v_xchar;
#ifdef KANJI
					hbrush = CreateSolidBrush(run_bkcolor(p[Columns + c0],
														p[Columns * 2 + c0]));
#else
					hbrush = CreateSolidBrush(run_bkcolor(p[c0], 0));
#endif
					FillRect(hDC, &fr, hbrush);
					DeleteObject(hbrush);
				}
				c0 = c;
			}
		}
		attr = 0xff;
#if defined(KANJI) && defined(SYNTAX)
		bgattr = p[Columns * 2 + nCol];
#endif
#ifdef KANJI
		kanji = CELLKANA(nRow, nCol);
#endif
		for (i = i0 = nCol; i <= nEndCol; i++)
		{
#ifdef KANJI
			if (attr != p[Columns + i] || kanji != CELLKANA(nRow, i)
#if defined(KANJI) && defined(SYNTAX)
					|| (issynpaint() && bgattr != p[Columns * 2 + i])
#endif
				)
#else
			if (attr != (p[i] & 0x80))
#endif
			{
#ifdef KANJI
				/* either plane: a rule may ask for a ground and no colour */
				if (p[Columns + i0] || p[Columns * 2 + i0])
#else
				if (p[i0] & 0x80)
#endif
				{
#ifdef KANJI
					/*
					 * One call each way round. run_bkcolor() knows what a
					 * visual bell does to the two colours the window was
					 * given, and what it does not do to one a rule named.
					 */
					SetTextColor(hDC, p[Columns + i0]
							? GetColor(p[Columns + i0], do_vb ? 'b' : 't')
							: (do_vb ? *v_bgcolor : *v_fgcolor));
					SetBkColor(hDC, run_bkcolor(p[Columns + i0],
													p[Columns * 2 + i0]));
#else
					if (do_vb)
					{
						SetTextColor(hDC, *v_fgcolor);
						SetBkColor(hDC, *v_bgcolor);
					}
					else
					{
						SetTextColor(hDC, *v_bgcolor);
						SetBkColor(hDC, *v_fgcolor);
					}
#endif
				}
				else
				{
					if (do_vb)
					{
						SetTextColor(hDC, *v_bgcolor);
						SetBkColor(hDC, *v_fgcolor);
					}
					else
					{
						SetTextColor(hDC, *v_fgcolor);
						SetBkColor(hDC, *v_bgcolor);
					}
				}
				if ((i - i0) > 0)
				{
					rect.right	= i * v_xchar;
#ifdef KANJI
					if (CELLCONT(nRow, i0) && CELLWIDE(nRow, i))
					{
						rect.left  = (i0 + 1) * v_xchar;
						rect.right = (i + 1) * v_xchar;
						PrintChar(hDC, &rect, &hOldFont, p + i0 + 1, i - i0,
									p[Columns + i0], p[Columns * 2 + i0], nRow, i0 + 1);
					}
					else if (CELLCONT(nRow, i0))
					{
						rect.left  = (i0 + 1) * v_xchar;
						if ((i - (i0 + 1)) > 0)
							PrintChar(hDC, &rect, &hOldFont, p + i0 + 1,
									i - (i0 + 1), p[Columns + i0], p[Columns * 2 + i0], nRow, i0 + 1);
					}
					else if (CELLWIDE(nRow, i))
					{
						rect.right = (i + 1) * v_xchar;
						PrintChar(hDC, &rect, &hOldFont, p + i0, (i - i0) + 1,
									p[Columns + i0], p[Columns * 2 + i0], nRow, i0);
					}
					else
#endif
					PrintChar(hDC, &rect, &hOldFont, p + i0, i - i0,
								p[Columns + i0], p[Columns * 2 + i0], nRow, i0);
					rect.left	= i * v_xchar;
					i0 = i;
				}
#ifdef KANJI
				attr = p[Columns + i];
#if defined(KANJI) && defined(SYNTAX)
				bgattr = p[Columns * 2 + i];
#endif
				kanji = CELLKANA(nRow, i);
#else
				attr = (p[i] & 0x80);
#endif
			}
		}
		if ((i - i0) > 0)
		{
#ifdef KANJI
			if (p[Columns + i0] || p[Columns * 2 + i0])
#else
			if (p[i0] & 0x80)
#endif
			{
#ifdef KANJI
				SetTextColor(hDC, p[Columns + i0]
						? GetColor(p[Columns + i0], do_vb ? 'b' : 't')
						: (do_vb ? *v_bgcolor : *v_fgcolor));
				SetBkColor(hDC, run_bkcolor(p[Columns + i0],
												p[Columns * 2 + i0]));
#else
				if (do_vb)
				{
					SetTextColor(hDC, *v_fgcolor);
					SetBkColor(hDC, *v_bgcolor);
				}
				else
				{
					SetTextColor(hDC, *v_bgcolor);
					SetBkColor(hDC, *v_fgcolor);
				}
#endif
			}
			else
			{
				if (do_vb)
				{
					SetTextColor(hDC, *v_bgcolor);
					SetBkColor(hDC, *v_fgcolor);
				}
				else
				{
					SetTextColor(hDC, *v_fgcolor);
					SetBkColor(hDC, *v_bgcolor);
				}
			}
			rect.right	= i * v_xchar;
#ifdef KANJI
			if (CELLCONT(nRow, i0) && CELLWIDE(nRow, i))
			{
				rect.left  = (i0 + 1) * v_xchar;
				rect.right = (i + 1) * v_xchar;
				PrintChar(hDC, &rect, &hOldFont, p + i0 + 1, i - i0,
							p[Columns + i0], p[Columns * 2 + i0], nRow, i0 + 1);
			}
			else if (CELLCONT(nRow, i0))
			{
				rect.left  = (i0 + 1) * v_xchar;
				if ((i - (i0 + 1)) > 0)
					PrintChar(hDC, &rect, &hOldFont, p + i0 + 1,
							i - (i0 + 1), p[Columns + i0], p[Columns * 2 + i0], nRow, i0 + 1);
			}
			else if (CELLWIDE(nRow, i))
			{
				rect.right = (i + 1) * v_xchar;
				PrintChar(hDC, &rect, &hOldFont, p + i0, (i - i0) + 1,
							p[Columns + i0], p[Columns * 2 + i0], nRow, i0);
			}
			else
#endif
			PrintChar(hDC, &rect, &hOldFont, p + i0, i - i0,
						p[Columns + i0], p[Columns * 2 + i0], nRow, i0);
		}
	}
#if defined(KANJI) && defined(SYNTAX)
	if (syntax_on())
		SetFontType(NULL, 0, hDC, &hOldFont);
#endif
no_draw:
	SelectObject(hDC, hOldFont);
	EndPaint(hWnd, &ps);
	if (caret == TRUE)
		ShowCaret(hWnd);
	return(0);
}

static BOOL
keybuf_chk(int area)
{
	char		*	p;

	if ((area + c_end) >= c_size)
	{
		if ((p = alloc(c_end + ((area / sizeof(keybuf)) + 1) * sizeof(keybuf))) != NULL)
		{
			memcpy(p, cbuf, c_size);
			if (cbuf != keybuf)
				free(cbuf);
			cbuf = p;
			c_size = c_end + ((area / sizeof(keybuf)) + 1) * sizeof(keybuf);
			return(TRUE);
		}
		return(FALSE);
	}
	return(TRUE);
}

static WIN *
get_linecol(LPARAM lParam, FPOS *pos, int *row, int *col)
{
	WIN		*	wp;
	int			x;
	int			y;
	int			i;
	int			j;
	linenr_t	lnum;

	*col = x = min(Columns - 1, max(0, LOWORD(lParam) / v_xchar));
	*row = y = min(Rows - 1, (HIWORD(lParam) - 1) / v_ychar);
#ifdef KANJI
	if (CELLCONT(y, x))
		x--;
#endif
	wp = firstwin;
	pos->lnum = 0;
	pos->col = MAXCOL;
	while (wp != NULL)
	{
		if (wp->w_winpos <= y && y < (wp->w_winpos + wp->w_height))
			break;
		wp = wp->w_next;
	}
	if (wp == NULL)
		return(NULL);
	if (wp->w_p_wrap)			/* long line wrapping, adjust curwin->w_row */
	{
		lnum = wp->w_topline;
		for (i = wp->w_winpos; i < (wp->w_winpos + wp->w_height); )
		{
			if (wp->w_buffer->b_ml.ml_line_count < lnum)
				break;
			j = plines_win(wp, lnum);
			if (i <= y && y < (i + j))
			{
				j = Columns * (y - i) + x - (wp->w_p_nu ? 8 : 0);
				if (j < 0)
					j = 0;
				pos->col = vcol2col(wp, lnum, j, NULL, 0, 0);
				pos->lnum = lnum;
				return(wp);
			}
			i += j;
			lnum ++;
		}
	}
	else
	{
		lnum = wp->w_topline + (y - wp->w_winpos);
		j = x - (wp->w_p_nu ? 8 : 0) + curwin->w_leftcol;
		if (j < 0)
			j = 0;
		pos->col = vcol2col(wp, lnum, j, NULL, 0, 0);
		pos->lnum = lnum;
	}
	return(wp);
}

static WIN *
get_statusline(LPARAM lParam, int *row)
{
	WIN		*	wp;
	int			y;

	*row = y = min(Rows - 1, (HIWORD(lParam) - 1) / v_ychar);
	wp = firstwin;
	while (wp != NULL)
	{
		if (y == (wp->w_winpos + wp->w_height))
			break;
		wp = wp->w_next;
	}
	if (wp == NULL || wp == lastwin)
		return(NULL);
	return(wp);
}

static VOID
cursor_refresh(HWND hWnd)
{
	adjust_cursor();
	cursupdate();
	if (VIsual.lnum)
		updateScreen(INVERTED);
	if (must_redraw)
		updateScreen(must_redraw);
	showruler(FALSE);
	setcursor();
	cursor_on();
	flushbuf();
	MoveCursor(hWnd);
}

static VOID
clear_visual(HWND hWnd)
{
	if (VIsual.lnum != 0)
	{
		VIsual.lnum = 0;
		Visual_block = FALSE;
		updateScreen(NOT_VALID);
		cursor_refresh(hWnd);
	}
}

static VOID
clear_cmode(HWND hWnd)
{
	int				i;
	int				j;
	char_u		*	p;
	RECT			rect;

	for (i = 0; i < Rows; i++)
	{
		p = WinScreen[i];
		for (j = 0; j < Columns; j++)
#if defined(KANJI) && defined(SYNTAX)
		{
			/*
			 * Both planes, or a screen would come back with its colours
			 * wiped and its grounds kept. Either way the redraw this asks
			 * for puts them back.
			 */
			p[Columns + j]		= 0;
			p[Columns * 2 + j]	= 0;
		}
#else
			p[Columns + j] &= ~CMODE;
#endif
	}
	rect.left	= 0;
	rect.right	= Columns * v_xchar;
	rect.top	= 0;
	rect.bottom	= Rows * v_ychar;
	InvalidateRect(hWnd, &rect, FALSE);
}

/*
 * Mark one cell of a row as selected.
 *
 * Where there are colours the marker is written over the one the cell had --
 * CMODE is '*', which is not a colour id -- so the ground has to go with it,
 * or the selection would be drawn over the background of whatever it covers.
 * Both come back with the redraw clear_cmode() asks for.
 */
static void
mark_cmode(char_u *p, int j)
{
#if defined(KANJI) && defined(SYNTAX)
	p[Columns + j]		= CMODE;
	p[Columns * 2 + j]	= 0;
#else
	p[Columns + j]	   |= CMODE;
#endif
}

static VOID
draw_cmode(HWND hWnd, int cs_row, int cs_col, int ce_row, int ce_col)
{
	int				nRow, nCol, nEndRow, nEndCol;
	int				i;
	int				j;
	char_u		*	p;
	RECT			rect;

	clear_cmode(hWnd);
	nRow	= min(cs_row, ce_row);
	nEndRow	= max(cs_row, ce_row);
	nCol	= min(cs_col, ce_col);
	nEndCol	= max(cs_col, ce_col);
	for (i = nRow; i <= nEndRow; i++)
	{
		p = WinScreen[i];
		for (j = nCol; j <= nEndCol; j++)
		{
#ifdef KANJI
			/*
			 * Half of a wide character at either end of the selection takes
			 * its other half with it. The two builds used to disagree about
			 * which half that is at the far end -- j - 1 where SYNTAX was
			 * defined, j + 1 where it was not -- and CELLWIDE() says the cell
			 * is the left half, so it is the one after. Neither arm looked
			 * where it was going, either: at the ends of a row j - 1 and
			 * j + 1 are in another row's planes.
			 */
			if (j == nCol)
			{
				if (CELLCONT(i, j) && j > 0)
					mark_cmode(p, j - 1);
			}
			else if (j == nEndCol)
			{
				if (CELLWIDE(i, j) && j + 1 < Columns)
					mark_cmode(p, j + 1);
			}
#endif
			mark_cmode(p, j);
		}
	}
	rect.left	= nCol * v_xchar;
	rect.right	= (nEndCol + 1) * v_xchar - 1;
	rect.top	= nRow * v_ychar;
	rect.bottom	= (nEndRow + 1) * v_ychar - 1;
	InvalidateRect(hWnd, &rect, FALSE);
}

/*
 * Put UTF-8 text on the clipboard as CF_UNICODETEXT, so that characters outside
 * the ANSI code page survive. Returns TRUE on success.
 */
	int
clip_put(char_u *text, int len)
{
	HANDLE	hClipData;
	WCHAR  *lpClipData;
	int		wlen;

	if (text == NULL || len <= 0)
		return FALSE;
	wlen = MultiByteToWideChar(CP_UTF8, 0, (LPCSTR)text, len, NULL, 0);
	if (wlen <= 0)
		return FALSE;
	if ((hClipData = GlobalAlloc(GMEM_MOVEABLE,
							(wlen + 1) * sizeof(WCHAR))) == NULL)
		return FALSE;
	if ((lpClipData = (WCHAR *)GlobalLock(hClipData)) == NULL)
	{
		GlobalFree(hClipData);
		return FALSE;
	}
	MultiByteToWideChar(CP_UTF8, 0, (LPCSTR)text, len, lpClipData, wlen);
	lpClipData[wlen] = 0;
	GlobalUnlock(hClipData);
	if (OpenClipboard(hVimWnd) == FALSE)
	{
		GlobalFree(hClipData);
		return FALSE;
	}
	EmptyClipboard();
	SetClipboardData(CF_UNICODETEXT, hClipData);
	CloseClipboard();
	return TRUE;
}

/*
 * Fetch the clipboard as UTF-8 in a fresh buffer the caller frees, or NULL.
 * CF_UNICODETEXT is preferred; CF_TEXT is accepted from programs that only
 * offer the ANSI form.
 */
	char_u *
clip_get(void)
{
	HANDLE	hClipData;
	void   *lpClipData;
	char_u *text = NULL;
	int		len;

	if (OpenClipboard(hVimWnd) == FALSE)
		return NULL;
	if ((hClipData = GetClipboardData(CF_UNICODETEXT)) != NULL)
	{
		if ((lpClipData = GlobalLock(hClipData)) != NULL)
		{
			len = WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR)lpClipData, -1,
										NULL, 0, NULL, NULL);
			if (len > 0 && (text = alloc((unsigned)len)) != NULL)
				WideCharToMultiByte(CP_UTF8, 0, (LPCWSTR)lpClipData, -1,
										(LPSTR)text, len, NULL, NULL);
			GlobalUnlock(hClipData);
		}
	}
	else if ((hClipData = GetClipboardData(CF_TEXT)) != NULL)
	{
		if ((lpClipData = GlobalLock(hClipData)) != NULL)
		{
			int		alen = (int)strlen((char *)lpClipData);

			if ((text = alloc((unsigned)(alen * UTF8_MAXLEN + 1))) != NULL)
			{
				len = kanjiconvsfrom((char_u *)lpClipData, alen, text,
							alen * UTF8_MAXLEN, NULL, JP_SJIS, NULL);
				if (len < 0)
					len = 0;
				text[len] = NUL;
			}
			GlobalUnlock(hClipData);
		}
	}
	CloseClipboard();
	return text;
}

/*
 * Collect the marked cells of the screen as UTF-8 into a fresh buffer, one
 * newline per marked row. The character in a cell comes from the code point
 * plane, not from the character plane, which only holds the first bytes.
 * Returns NULL when nothing is marked; the caller frees the result.
 */
	static char_u *
cmode_text(int *lenp)
{
	char_u	*top;
	char_u	*ptr;
	int		i, j;
	int		num = 0;
	int		line = 0;
	BOOL	flg;

	for (i = 0; i < Rows; i++)
	{
		flg = FALSE;
		for (j = 0; j < Columns; j++)
		{
#if defined(KANJI) && defined(SYNTAX)
			if (WinScreen[i][Columns + j] == CMODE)
#else
			if (WinScreen[i][Columns + j] & CMODE)
#endif
			{
				num++;
				flg = TRUE;
			}
		}
		if (flg)
			line++;
	}
	if (num == 0)
		return NULL;
	/* At worst every cell is a character of UTF8_MAXLEN bytes. */
	if ((top = alloc((unsigned)(num * UTF8_MAXLEN + line * 2 + 1))) == NULL)
		return NULL;
	ptr = top;
	for (i = 0; i < Rows; i++)
	{
		flg = FALSE;
		for (j = 0; j < Columns; j++)
		{
#if defined(KANJI) && defined(SYNTAX)
			if (WinScreen[i][Columns + j] != CMODE)
#else
			if (!(WinScreen[i][Columns + j] & CMODE))
#endif
				continue;
			flg = TRUE;
			{
				int		cp = CELLCP(i, j);

				if (cp == -1)
					continue;			/* right half of a wide character */
				if (cp > 0)
					ptr += utf_encode(cp, ptr);
				else
					*ptr++ = WinScreen[i][j];
			}
		}
		if (flg)
			*ptr++ = '\n';
	}
	if (ptr > top && ptr[-1] == '\n')
		ptr--;							/* no newline after the last row */
	*ptr = NUL;
	*lenp = (int)(ptr - top);
	return top;
}

static VOID
yank_cmode(HWND hWnd, BOOL clip)
{
	char_u		*	text;
	int				len = 0;
	int				i;

	if ((text = cmode_text(&len)) == NULL)
		return;
	if (clip)
		(void)clip_put(text, len);
	else
	{
		if (keybuf_chk(len + 1))
			for (i = 0; i < len; i++)
				cbuf[c_end++] = text[i];
	}
	free(text);
}

static void
TopWindow(HWND hWnd)
{
	if ((ver_info.dwPlatformId != VER_PLATFORM_WIN32_NT
				&& ver_info.dwMajorVersion == 4
				&& ver_info.dwMinorVersion >= 10)
		|| (ver_info.dwPlatformId == VER_PLATFORM_WIN32_NT
				&& ver_info.dwMajorVersion >= 5))
	{		/* Windows 98 later or Windows 2000 later */
		SetForegroundWindow(hWnd);
		if (GetForegroundWindow() != hWnd)
		{
			ShowWindow(hWnd, SW_MINIMIZE);
			OpenIcon(hWnd);
		}
	}
	else
		SetForegroundWindow(hWnd);
}

LRESULT FAR PASCAL
WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	extern void			start_arrow __ARGS((void));
	static DWORD		repeat = 0;
	static BOOL			state_shift	= FALSE;
	static BOOL			state_ctrl	= FALSE;
	static WIN		*	selwin		= NULL;
	static WIN		*	selstatus	= NULL;
	static FPOS			selpos;
	static int			updown	= 0;
	static int			leftright = 0;
	static BOOL			vmode	= FALSE;
	static BOOL			cmode	= FALSE;
	static int			cs_row	= 0;
	static int			cs_col	= 0;
	static int			ce_row	= 0;
	static int			ce_col	= 0;
	static LPARAM		mouse_pos;
	static BOOL			s_cursor	= TRUE;
	static DWORD		oldmix	= 0;
	static FILETIME		byFile;
	static char			filter[] = "ALL\0*.*\0C\0*.cpp;*.h;*.def;*.rc\0DOC\0*.txt;*.doc\0";
	OPENFILENAME		ofn;
	CHOOSECOLOR			cfColor;
	COPYDATASTRUCT	*	cds;
	DWORD			*	pColor;
	HANDLE				hClipData;
	char			*	lpClipData;
	char_u			*	p;
	INT					i;
	INT					files;
	WIN				*	wp;
	BUF				*	buf;
	FPOS				pos;
	int					row;
	int					col;
	int					more;
	BOOL				redraw	= FALSE;
	RECT				rcWindow;
	HMENU				hEdit;
	HMENU				hMenu;
	POINT				point;
	BOOL				bClear;
	int					rc;
	LOGFONT				logfont;

	switch (uMsg) {
	case WM_CREATE:
		nIcon.cbSize	= sizeof(NOTIFYICONDATA);
		nIcon.uID		= 1;
		nIcon.hWnd		= hWnd;
		nIcon.uFlags	= NIF_MESSAGE|NIF_ICON|NIF_TIP;
		nIcon.hIcon		= LoadIcon(hInst, "vim");
		nIcon.uCallbackMessage = WM_TASKTRAY;
		DragAcceptFiles(hWnd, TRUE);
		ResetScreen(hWnd);
		MoveCursor(hWnd);
		return(0);
	case WM_TASKTRAY:
		switch (lParam) {
		case WM_LBUTTONDBLCLK:
			Shell_NotifyIcon(NIM_DELETE, &nIcon);
			ShowWindow(hWnd, SW_SHOW);
			OpenIcon(hWnd);
			SetForegroundWindow(hWnd);
			break;
		case WM_RBUTTONUP:
			SetForegroundWindow(hWnd);
			GetCursorPos(&point);
			hEdit = CreatePopupMenu();
			AppendMenu(hEdit,  MF_STRING,   IDM_OPEN,  "&Open");
			AppendMenu(hEdit,  MF_STRING,   IDM_EXIT,  "&Exit");
			AppendMenu(hEdit,  MF_SEPARATOR,0,			NULL);
			AppendMenu(hEdit,  MF_STRING,   IDM_CANCEL, "&Cancel");
			TrackPopupMenu(hEdit, TPM_LEFTBUTTON | TPM_RIGHTBUTTON,
								point.x, point.y, 0, hWnd, NULL);
			DestroyMenu(hEdit);
			break;
		}
		return(0);
	case WM_ERASEBKGND:
		return(1);
	case WM_DPICHANGED:
		/*
		 * Dragged to a monitor at another scale. lParam suggests a rectangle,
		 * but its size is the old window scaled by the DPI ratio, which is only
		 * the right size by accident; take the position from it and let
		 * mch_set_winsize() work the size out of the resized font, so the same
		 * text stays on screen.
		 */
		dpi_scale_to(HIWORD(wParam));
		ResetScreen(hWnd);
		SetWindowPos(hWnd, NULL, ((RECT *)lParam)->left, ((RECT *)lParam)->top,
					0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
		nowCols = Columns;
		nowRows = Rows;
		mch_set_winsize();
		return(0);
	case WM_PAINT:
		return(PaintWindow(hWnd));
	case WM_SIZE:
		return(WindowSize(hWnd, HIWORD(lParam), LOWORD(lParam)));
	case WM_SETCURSOR:
		if (LOWORD(lParam) == HTCLIENT && lpCurrCurs != IDC_IBEAM)
		{
			SetCursor(hIbeamCurs);
			lpCurrCurs = IDC_IBEAM;
		}
		else if (LOWORD(lParam) != HTCLIENT && lpCurrCurs != IDC_ARROW)
		{
			SetCursor(hArrowCurs);
			lpCurrCurs = IDC_ARROW;
		}
		break;
	case WM_CHAR:
		{
			static WCHAR	hisur = 0;		/* pending high surrogate */
			WCHAR			wc = (WCHAR)wParam;

			if (wc >= 0xd800 && wc <= 0xdbff)
			{
				hisur = wc;					/* wait for the other half */
				return(0);
			}
			if (hisur != 0 && wc >= 0xdc00 && wc <= 0xdfff)
			{
				/*
				 * A character outside the BMP. It can never be a command key,
				 * so hand it straight to the buffer.
				 */
				wc_len = utf_encode(0x10000 + ((hisur - 0xd800) << 10)
											+ (wc - 0xdc00), wc_bytes);
				hisur = 0;
				if (!keybuf_chk(UTF8_MAXLEN + 1))
					return(0);
				push_wchar();
				return(0);
			}
			hisur = 0;
			wc_len = utf_encode(wc, wc_bytes);
		}
		if (cmode)
		{
			if (LOBYTE(wParam) == Ctrl('C'))
				yank_cmode(hWnd, TRUE);
			clear_cmode(hWnd);
#if defined(KANJI) && defined(SYNTAX)
			if (syntax_on())
				updateScreen(CLEAR);
#endif
			cmode = FALSE;
			if (LOBYTE(wParam) == Ctrl('C'))
			{
				ctrlc_pressed = FALSE;
				return(0);
			}
		}
		if (s_cursor && config_mouse
			&& (((State & NORMAL) && strchr("aAiIoOR", LOBYTE(wParam)) != NULL)
								|| ((State & INSERT) && LOBYTE(wParam) != ESC)))
		{
			s_cursor = FALSE;
			ShowCursor(FALSE);
		}
		else if (!s_cursor && (State & INSERT) && LOBYTE(wParam) == ESC)
		{
			s_cursor = TRUE;
			ShowCursor(TRUE);
		}
		if (!keybuf_chk(UTF8_MAXLEN + 1))
			return(0);
		switch (LOBYTE(wParam)) {
		case Ctrl('^'):
		case Ctrl('@'):
			/* already processed on WM_KEYDOWN */
			break;
		default:
#ifdef FEPCTRL
			if (!(State & NORMAL)
					&& (curbuf->b_p_fc && (p_fk != NULL && STRRCHR(p_fk, ESC+'@') != NULL)))
			/* shift + space key special routine */
			{
				static int		kanji = 0;

				if (kanji)
				{
					if (LOBYTE(wParam) == 0x40)
					{
						if (fep_get_mode())
							fep_force_off();
						else
							fep_force_on();
					}
					else
					{			/* a legacy FEP pair, in CP932 */
						char_u	sj[2];
						char_u	u8[UTF8_MAXLEN + 1];
						int		n, j;

						sj[0] = kanji;
						sj[1] = LOBYTE(wParam);
						n = kanjiconvsfrom(sj, 2, u8, (int)sizeof(u8),
											NULL, JP_SJIS, NULL);
						for (j = 0; j < n; j++)
							cbuf[c_end++] = u8[j];
					}
					kanji = 0;
				}
				else if (LOBYTE(wParam) == ' ' && state_shift)
				{
					if (fep_get_mode())
						fep_force_off();
					else
						fep_force_on();
				}
				else if (LOBYTE(wParam) == 0x81 && state_shift)
					kanji = 0x81;
				else
					push_wchar();
			}
			else
#endif
			if (repeat && (config_overflow < 3))
				push_wchar();
			else if (repeat && (LOBYTE(wParam) == 'j'
										|| LOBYTE(wParam) == 'k'
										|| LOBYTE(wParam) == Ctrl('N')
										|| LOBYTE(wParam) == Ctrl('P')
										|| LOBYTE(wParam) == Ctrl('E')
										|| LOBYTE(wParam) == Ctrl('Y')))
			{
				i = 1;
				if ((p_sj > 1)
						&& ((curwin->w_winpos + curwin->w_height - 1 <= v_row)
												|| (curwin->w_winpos == v_row)))
					;
				else if (curwin->w_height <= 3)
					;
				else if ((curwin->w_winpos + curwin->w_height - 1 <= v_row)
												|| (curwin->w_winpos == v_row))
				{
					repeat++;
					i = repeat / KEY_REP + (repeat > 3 ? 2 : 1);
					if (i > curwin->w_p_scroll)
						i = curwin->w_p_scroll;
					if (i > config_overflow)
						i = config_overflow;
				}
				if (i == 0)
					;
				else if (vpeekc() != NUL)
				{
					if (c_next == 0)
						;
					else if ((c_end - c_next) <= config_overflow)
						i = 1;
					else
					{
						if (i > 3)
							config_overflow = --i;
						i = 0;		/* key buffer overflow */
					}
				}
				else
				{
					if (i == config_overflow && curwin->w_height > 10 && c_next == 0)
						config_overflow = ++i;
					else if ((c_end - c_next) <= config_overflow)
					{
						col = config_overflow - (c_end - c_next);
						i = i > col ? col : i;
					}
					else
						i = 0;		/* key buffer overflow */
				}
				while (i--)
				{
					if (!keybuf_chk(UTF8_MAXLEN))
						break;
					push_wchar();
				}
			}
			else if (repeat && (!curwin->w_p_wrap || (p_ww & 4))
							&& (LOBYTE(wParam) == 'h' || LOBYTE(wParam) == 'l'))
			{
				repeat++;
				if (c_next != 0 && vpeekc() != NUL)
					;
				else if (repeat > KEY_REP)
					push_wchar();
				push_wchar();
			}
			else if (repeat && isalpha(LOBYTE(wParam))
					&& (c_end - c_next) && (cbuf[c_end - 1] == LOBYTE(wParam)))
				/* key repeat cancel */;
			else
				push_wchar();
			break;
		}
		return(0);
	case WM_SYSKEYDOWN:
		if (lParam & (1 << 29))		/* ALT (Menu) Key Down */
			break;
		/* no break */
	case WM_KEYDOWN:
		if ((State & NORMAL) && (lParam & 0x40000000))
		{
			if (repeat == 0)
				repeat = 1;
		}
		else
			repeat = 0;
		if (wParam == VK_SHIFT)
			state_shift	= TRUE;
		else if (wParam == VK_CONTROL)
			state_ctrl	= TRUE;
		else if (wParam == VK_NUMLOCK || wParam == VK_CAPITAL)
			;
		else if (state_ctrl)
		{
			if (GetKeyState(VK_CONTROL) & 0x8000)
			{
				if (!keybuf_chk(2))
					return(0);
				switch (wParam) {
				case '6':
				case 0xde: /* '^' key */
					cbuf[c_end++] = '^' & 0x1f;
					break;
				case 'c':
				case 'C':
					ctrlc_pressed = TRUE;
					break;
				case '@':
				case 0xc0: /* '@' key */
					cbuf[c_end++] = K_ZERO;
					break;
				case VK_LEFT:
					cbuf[c_end++] = K_NUL;
					cbuf[c_end++] = 's';
					break;
				case VK_RIGHT:
					cbuf[c_end++] = K_NUL;
					cbuf[c_end++] = 't';
					break;
				}
			}
		}
		else if (state_shift)
		{
			if (!keybuf_chk(2))
				return(0);
			switch (wParam) {
			case VK_LEFT:	cbuf[c_end++] = K_NUL; cbuf[c_end++] = 's'; break;
			case VK_RIGHT:	cbuf[c_end++] = K_NUL; cbuf[c_end++] = 't'; break;
			case VK_F1:		cbuf[c_end++] = K_NUL; cbuf[c_end++] = 'T'; break;
			case VK_F2:		cbuf[c_end++] = K_NUL; cbuf[c_end++] = 'U'; break;
			case VK_F3:		cbuf[c_end++] = K_NUL; cbuf[c_end++] = 'V'; break;
			case VK_F4:		cbuf[c_end++] = K_NUL; cbuf[c_end++] = 'W'; break;
			case VK_F5:		cbuf[c_end++] = K_NUL; cbuf[c_end++] = 'X'; break;
			case VK_F6:		cbuf[c_end++] = K_NUL; cbuf[c_end++] = 'Y'; break;
			case VK_F7:		cbuf[c_end++] = K_NUL; cbuf[c_end++] = 'Z'; break;
			case VK_F8:		cbuf[c_end++] = K_NUL; cbuf[c_end++] = '['; break;
			case VK_F9:		cbuf[c_end++] = K_NUL; cbuf[c_end++] = '\\'; break;
			case VK_F10:	cbuf[c_end++] = K_NUL; cbuf[c_end++] = ']'; break;
			}
		}
		else
		{
			if (repeat && (config_overflow < 3))
				;
			else if (repeat && (wParam == VK_UP || wParam == VK_DOWN))
			{
				if ((p_sj > 1)
						&& ((curwin->w_winpos + curwin->w_height - 1 <= v_row)
												|| (curwin->w_winpos == v_row)))
					;
				else if (curwin->w_height <= 3)
					;
				else
				{
					repeat++;
					i = repeat / KEY_REP + 1;
					if (i > curwin->w_p_scroll)
						i = curwin->w_p_scroll;
					if (i > config_overflow)
						i = config_overflow;
					if (vpeekc() != NUL)
					{
						if (c_next == 0)
							;
						else if ((c_end - c_next) <= (2 * config_overflow))
							i = 1;
						else
						{
							if (i > 3)
								config_overflow = --i;
							i = 0;		/* key buffer overflow */
						}
					}
					else
					{
						if (i == config_overflow && curwin->w_height > 10 && c_next == 0)
							config_overflow = ++i;
						else if ((c_end - c_next) <= (config_overflow * 2))
						{
							col = ((config_overflow * 2) - (c_end - c_next)) / 2;
							i = i > col ? col : i;
						}
						else
							i = 0;		/* key buffer overflow */
					}
					while (i-- > 1)
					{
						if (!keybuf_chk(2))
							break;
						cbuf[c_end++] = K_NUL;
						if (wParam == VK_UP)
							cbuf[c_end++] = 'H';
						else
							cbuf[c_end++] = 'P';
					}
				}
			}
			else if (repeat && (!curwin->w_p_wrap || (p_ww & 8))
					&& (wParam == VK_LEFT || wParam == VK_RIGHT))
			{
				repeat++;
				if (c_next != 0 && vpeekc() != NUL)
					;
				else if (repeat > KEY_REP && keybuf_chk(2))
				{
					cbuf[c_end++] = K_NUL;
					if (wParam == VK_LEFT)
						cbuf[c_end++] = 'K';
					else if (wParam == VK_RIGHT)
						cbuf[c_end++] = 'M';
				}
			}
			if (!keybuf_chk(2))
				return(0);
			switch (wParam) {
			case VK_PRIOR:	cbuf[c_end++] = K_NUL; cbuf[c_end++] = 'I'; break;
			case VK_NEXT:	cbuf[c_end++] = K_NUL; cbuf[c_end++] = 'Q'; break;
			case VK_UP:		cbuf[c_end++] = K_NUL; cbuf[c_end++] = 'H'; break;
			case VK_DOWN:	cbuf[c_end++] = K_NUL; cbuf[c_end++] = 'P'; break;
			case VK_LEFT:	cbuf[c_end++] = K_NUL; cbuf[c_end++] = 'K'; break;
			case VK_RIGHT:	cbuf[c_end++] = K_NUL; cbuf[c_end++] = 'M'; break;
			case VK_DELETE:	cbuf[c_end++] = '\177';	break;
			case VK_F1:		cbuf[c_end++] = K_NUL; cbuf[c_end++] = ';'; break;
			case VK_F2:		cbuf[c_end++] = K_NUL; cbuf[c_end++] = '<'; break;
			case VK_F3:		cbuf[c_end++] = K_NUL; cbuf[c_end++] = '='; break;
			case VK_F4:		cbuf[c_end++] = K_NUL; cbuf[c_end++] = '>'; break;
			case VK_F5:		cbuf[c_end++] = K_NUL; cbuf[c_end++] = '?'; break;
			case VK_F6:		cbuf[c_end++] = K_NUL; cbuf[c_end++] = '@'; break;
			case VK_F7:		cbuf[c_end++] = K_NUL; cbuf[c_end++] = 'A'; break;
			case VK_F8:		cbuf[c_end++] = K_NUL; cbuf[c_end++] = 'B'; break;
			case VK_F9:		cbuf[c_end++] = K_NUL; cbuf[c_end++] = 'C'; break;
			case VK_F10:	cbuf[c_end++] = K_NUL; cbuf[c_end++] = 'D'; break;
#ifdef NT106KEY
			/* ZENKAKU / HANKAKU KEY */
			case 0xf3: case 0xf4:
				if (config_nt106)
					cbuf[c_end++] = '[' & 0x1f;		/* ESC key !! */
				break;
#endif
			}
		}
		return(0);
	case WM_KEYUP:
		if (repeat && c_ind == 0)
		{
			for (i = c_next; i < c_end; i++)
			{
				if (cbuf[i] == cbuf[c_end - 1])
					c_end = i + 1;
			}
		}
		repeat = 0;
		switch (wParam) {
		case VK_SHIFT:
		case VK_LSHIFT:
		case VK_RSHIFT:
			state_shift = FALSE;
			break;
		case VK_CONTROL:
		case VK_LCONTROL:
		case VK_RCONTROL:
			state_ctrl	= FALSE;
			break;
		case VK_NUMLOCK:
		case VK_CAPITAL:
			break;
		}
		return(0);
	case WM_SYSKEYUP:
		return(0);
	case WM_SETFOCUS:
		v_focus = TRUE;
		MoveCursor(hWnd);
		return(0);
	case WM_KILLFOCUS:
		if (v_cursor && v_focus)
			HideCaret(hWnd);
		v_focus = FALSE;
		v_caret = 0;
		state_shift	= FALSE;
		state_ctrl	= FALSE;
		repeat = 0;
		return(0);
	case WM_DESTROY:
		DragAcceptFiles(hWnd, FALSE);
		if (hSystemUIFont != NULL)
		{
			DeleteObject(hSystemUIFont);
			hSystemUIFont = NULL;
		}
		DeleteObject(v_font);
		PostQuitMessage(0);
		return(0);
	case WM_COPYDATA:
		cds = (COPYDATASTRUCT *)lParam;
		if (do_msg != TRUE && GuiWin == '1' && NameBuff != NULL
				&& cds->dwData != 0 && (State & NORMAL) && cmode == FALSE
				&& selwin == NULL && VIsual.lnum == 0
				&& !(!p_hid && curbuf->b_changed && (autowrite(curbuf) == FAIL)))
		{
			COPYDATASTRUCT	*	cds = (COPYDATASTRUCT *)lParam;

			do_msg = TRUE;
			more = p_more;
			p_more = FALSE;
			++no_wait_return;
			if (!did_cd)
			{
				BUF		*buf;

				for (buf = firstbuf; buf != NULL; buf = buf->b_next)
				{
					buf->b_xfilename = buf->b_filename;
					mf_fullname(buf->b_ml.ml_mfp);
				}
				status_redraw_all();
			}
			did_cd = TRUE;
			p = cds->lpData;
			SetCurrentDirectory(p);
			strncpy(IObuff, &p[cds->dwData], IOSIZE);
			docmdline(IObuff);
			--no_wait_return;
			p_more = more;
			cursor_refresh(hWnd);
			if (!(ver_info.dwPlatformId == VER_PLATFORM_WIN32_NT
										&& ver_info.dwMajorVersion == 3))
				Shell_NotifyIcon(NIM_DELETE, &nIcon);
			ShowWindow(hWnd, SW_SHOW);
			OpenIcon(hWnd);
			SetForegroundWindow(hWnd);
			SetTimer(hWnd, SHOW_TIME, config_show, NULL);
			do_msg = FALSE;
			return(TRUE);
		}
		break;
	case WM_DROPFILES:
		if (RedrawingDisabled || no_wait_return)
			return(0);
		selwin = NULL;
		if (cmode)
			clear_cmode(hWnd);
		clear_visual(hWnd);
#if defined(KANJI) && defined(SYNTAX)
		if (syntax_on() && cmode)
			updateScreen(CLEAR);
#endif
		cmode = FALSE;
		more = p_more;
		p_more = FALSE;
		++no_wait_return;
		if (State & NORMAL)
		{
			int			change = FALSE;
			int			bitmap = FALSE;

			if (!p_hid && curbuf->b_changed && (autowrite(curbuf) == FAIL))
				change = TRUE;
			if (change && win_split(0L, FALSE) == FAIL)
			{
				WIN 	*	nextwp;
				WIN		*	now = curwin;

				for (wp = firstwin; wp != NULL; wp = nextwp)
				{
					nextwp = wp->w_next;
					if (wp->w_buffer->b_changed && (autowrite(wp->w_buffer) == FAIL))
						;
					else if (lastwin != firstwin)
					{
						win_enter(wp, TRUE);
						close_window(TRUE);
					}
				}
				win_enter(now, TRUE);
			}
			else
				change = FALSE;
			if (change && win_split(0L, FALSE) == FAIL)
				;
			else
			{
				files = DragQueryFile((HANDLE)wParam, 0xffffffff, NULL, 0);
				memset(IObuff, '\0', IOSIZE);
				strcpy(IObuff, ":args");
				for (i = 0; i < files; i++)
				{
					if (DragQueryFile((HANDLE)wParam, i, NameBuff, MAXPATHL) != 0)
					{
						if (isbitmap(NameBuff, NULL))
						{
							if (bitmap)
								Sleep(1000);
							strcpy(config_bitmapfile, NameBuff);
							config_bitmap = TRUE;
							InvalidateRect(hWnd, NULL, TRUE);
							UpdateWindow(hWnd);
							bitmap = TRUE;
						}
						else if (iswave(NameBuff))
						{
							int			j = 0;

							if (bitmap)
								Sleep(1000);
							strcpy(config_wavefile, NameBuff);
							config_wave = TRUE;
							sndPlaySound(config_wavefile, SND_SYNC);
						}
						else
						{
							if ((strlen(IObuff) + strlen(NameBuff) + 3) > IOSIZE)
								break;
							strcat(IObuff, " \"");
							strcat(IObuff, NameBuff);
							strcat(IObuff, "\"");
						}
					}
				}
				do_drag = TRUE;
				if (strlen(IObuff) > 5)
					docmdline(IObuff);
				do_drag = FALSE;
			}
		}
		else if (State & INSERT)
		{
			files = DragQueryFile((HANDLE)wParam, 0xffffffff, NULL, 0);
			memset(IObuff, '\0', IOSIZE);
			for (i = 0; i < files; i++)
			{
				if (DragQueryFile((HANDLE)wParam, i, NameBuff, MAXPATHL) != 0)
				{
					if ((strlen(IObuff) + strlen(NameBuff) + 3) > IOSIZE)
						break;
					strcat(IObuff, "\"");
					strcat(IObuff, NameBuff);
					strcat(IObuff, "\"");
				}
				if (i != (files - 1))
					strcat(IObuff, "\n");
			}
			if (keybuf_chk(strlen(IObuff)))
			{
				memcpy(&cbuf[c_end], IObuff, strlen(IObuff));
				c_end += strlen(IObuff);
			}
		}
		else
			emsg("No command mode");
		--no_wait_return;
		p_more = more;
		cursor_refresh(hWnd);
		DragFinish((HANDLE)wParam);
		if (IsIconic(hWnd))
			OpenIcon(hWnd);
		TopWindow(hVimWnd);
		return(0);
	case WM_QUERYOPEN:
		return(TRUE);
	case WM_INITMENU:
		hMenu = GetSystemMenu(hVimWnd, FALSE);
		for (i = 0; i < 2; i++)
		{
			int				j;

			for (j = IDM_CONF0; j <= IDM_CONF3; j++)
				CheckMenuItem(hMenu, j, MF_BYCOMMAND | MF_UNCHECKED);
			if (GuiConfig <= IDM_CONF3)
				CheckMenuItem(hMenu, IDM_CONF0 + GuiConfig, MF_BYCOMMAND | MF_CHECKED);
#ifdef USE_HISTORY
			if (i == 0)
				DeleteMenu(hMenu, 12, MF_BYPOSITION);
			for (j = IDM_HIST1; j <= IDM_HIST9; j++)
				DeleteMenu(hMenu, j, MF_BYCOMMAND);
			if (!config_ini)
			{
				char		*	p;
				HMENU			hHMenu;

				if (i == 0)
				{
					hHMenu = CreatePopupMenu();
					AppendMenu(hHMenu, MF_STRING, IDM_HSAVE, "&Save History");
					AppendMenu(hMenu,  MF_POPUP,  (UINT_PTR)hHMenu, "&History");
				}
				else
					hHMenu = GetSubMenu(hMenu, 5);
				for (j = IDM_HIST1; j <= IDM_HIST9; j++)
				{
					if ((p = HistoryGetMenu(j - IDM_HISTM)) != NULL)
					{
						AppendMenuU8(hHMenu, MF_STRING, j, (char_u *)p);
					}
				}
			}
#endif
			if (State & NORMAL)
			{
				if (i == 0)
				{
					EnableMenuItem(hMenu, 7, MF_BYPOSITION | MF_ENABLED);
					EnableMenuItem(hMenu, 8, MF_BYPOSITION | MF_ENABLED);
					if (config_ini)
						EnableMenuItem(hMenu, 9, MF_BYPOSITION | MF_GRAYED);
					else
						EnableMenuItem(hMenu, 9, MF_BYPOSITION | MF_ENABLED);
				}
				else
				{
					EnableMenuItem(hMenu, 1, MF_BYPOSITION | MF_ENABLED);
					EnableMenuItem(hMenu, 2, MF_BYPOSITION | MF_ENABLED);
					EnableMenuItem(hMenu, 3, MF_BYPOSITION | MF_ENABLED);
					if (config_ini)
						EnableMenuItem(hMenu, 4, MF_BYPOSITION | MF_GRAYED);
					else
						EnableMenuItem(hMenu, 4, MF_BYPOSITION | MF_ENABLED);
#ifdef USE_HISTORY
					if (config_ini)
						EnableMenuItem(hMenu, 5, MF_BYPOSITION | MF_GRAYED);
					else
						EnableMenuItem(hMenu, 5, MF_BYPOSITION | MF_ENABLED);
#endif
					EnableMenuItem(hMenu, IDM_HELP,  MF_BYCOMMAND | MF_ENABLED);
				}
				if (VIsual.lnum != 0 || cmode)
					EnableMenuItem(hMenu, IDM_YANK,  MF_BYCOMMAND | MF_ENABLED);
				else
					EnableMenuItem(hMenu, IDM_YANK,  MF_BYCOMMAND | MF_GRAYED);
				if (VIsual.lnum != 0)
					EnableMenuItem(hMenu, IDM_DELETE,MF_BYCOMMAND | MF_ENABLED);
				else
					EnableMenuItem(hMenu, IDM_DELETE,MF_BYCOMMAND | MF_GRAYED);
				if (cmode)
					EnableMenuItem(hMenu, IDM_PASTE, MF_BYCOMMAND | MF_GRAYED);
				else
					EnableMenuItem(hMenu, IDM_PASTE, MF_BYCOMMAND | MF_ENABLED);
				EnableMenuItem(hMenu, IDM_CLICK, MF_BYCOMMAND | MF_ENABLED);
				EnableMenuItem(hMenu, IDM_FILE,  MF_BYCOMMAND | MF_ENABLED);
				if (curbuf != NULL && curbuf->b_filename != NULL)
					EnableMenuItem(hMenu, IDM_SFILE, MF_BYCOMMAND | MF_ENABLED);
				else
					EnableMenuItem(hMenu, IDM_SFILE, MF_BYCOMMAND | MF_GRAYED);
				EnableMenuItem(hMenu, IDM_AFILE, MF_BYCOMMAND | MF_ENABLED);
				if (strlen(config_printer) == 0 ||
						(curbuf->b_ml.ml_line_count == 1 && strlen(ml_get(1)) == 0))
					EnableMenuItem(hMenu, IDM_PRINT, MF_BYCOMMAND | MF_GRAYED);
				else
					EnableMenuItem(hMenu, IDM_PRINT, MF_BYCOMMAND | MF_ENABLED);
				EnableMenuItem(hMenu, IDM_EXIT,  MF_BYCOMMAND | MF_ENABLED);
				if (!curbuf->b_changed && (lastwin == firstwin)
											&& (curbuf->b_filename != NULL))
					EnableMenuItem(hMenu, IDM_TAIL,  MF_BYCOMMAND | MF_ENABLED);
				else
					EnableMenuItem(hMenu, IDM_TAIL,  MF_BYCOMMAND | MF_GRAYED);
			}
			else
			{
				if (i == 0)
				{
					EnableMenuItem(hMenu, 7, MF_BYPOSITION | MF_GRAYED);
					EnableMenuItem(hMenu, 8, MF_BYPOSITION | MF_GRAYED);
					EnableMenuItem(hMenu, 9, MF_BYPOSITION | MF_GRAYED);
#ifdef USE_HISTORY
					if (config_ini)
						EnableMenuItem(hMenu, 12, MF_BYPOSITION | MF_GRAYED);
#endif
				}
				else
				{
					EnableMenuItem(hMenu, 2, MF_BYPOSITION | MF_DISABLED);
					EnableMenuItem(hMenu, 3, MF_BYPOSITION | MF_DISABLED);
					if (config_ini)
						EnableMenuItem(hMenu, 4, MF_BYPOSITION | MF_GRAYED);
					else
						EnableMenuItem(hMenu, 4, MF_BYPOSITION | MF_DISABLED);
#ifdef USE_HISTORY
					EnableMenuItem(hMenu, 5, MF_BYPOSITION | MF_GRAYED);
#endif
					EnableMenuItem(hMenu, IDM_HELP,  MF_BYCOMMAND|MF_DISABLED);
				}
				if (cmode)
					EnableMenuItem(hMenu, IDM_YANK,  MF_BYCOMMAND | MF_ENABLED);
				else
					EnableMenuItem(hMenu, IDM_YANK,  MF_BYCOMMAND | MF_GRAYED);
				EnableMenuItem(hMenu, IDM_DELETE,MF_BYCOMMAND | MF_GRAYED);
				EnableMenuItem(hMenu, IDM_PASTE, MF_BYCOMMAND | MF_ENABLED);
				EnableMenuItem(hMenu, IDM_CLICK, MF_BYCOMMAND | MF_GRAYED);
				EnableMenuItem(hMenu, IDM_FILE,  MF_BYCOMMAND | MF_GRAYED);
				EnableMenuItem(hMenu, IDM_SFILE, MF_BYCOMMAND | MF_GRAYED);
				EnableMenuItem(hMenu, IDM_AFILE, MF_BYCOMMAND | MF_GRAYED);
				EnableMenuItem(hMenu, IDM_PRINT, MF_BYCOMMAND | MF_GRAYED);
				EnableMenuItem(hMenu, IDM_EXIT,  MF_BYCOMMAND | MF_GRAYED);
				EnableMenuItem(hMenu, IDM_TAIL,  MF_BYCOMMAND | MF_GRAYED);
			}
			if ((curwin->w_arg_idx + 1) < arg_count)
				EnableMenuItem(hMenu, IDM_NFILE, MF_BYCOMMAND | MF_ENABLED);
			else
				EnableMenuItem(hMenu, IDM_NFILE, MF_BYCOMMAND | MF_GRAYED);
			if (curwin->w_arg_idx >= 1)
				EnableMenuItem(hMenu, IDM_PFILE, MF_BYCOMMAND | MF_ENABLED);
			else
				EnableMenuItem(hMenu, IDM_PFILE, MF_BYCOMMAND | MF_GRAYED);
			if (config_bitmap)
				CheckMenuItem(hMenu, IDM_BITMAP, MF_BYCOMMAND | MF_CHECKED);
			else
				CheckMenuItem(hMenu, IDM_BITMAP, MF_BYCOMMAND | MF_UNCHECKED);
			if (config_wave)
				CheckMenuItem(hMenu, IDM_WAVE, MF_BYCOMMAND | MF_CHECKED);
			else
				CheckMenuItem(hMenu, IDM_WAVE, MF_BYCOMMAND | MF_UNCHECKED);
			if (config_save)
				CheckMenuItem(hMenu, IDM_SAVE, MF_BYCOMMAND | MF_CHECKED);
			else
				CheckMenuItem(hMenu, IDM_SAVE, MF_BYCOMMAND | MF_UNCHECKED);
			if (config_comb)
				CheckMenuItem(hMenu, IDM_COMB, MF_BYCOMMAND | MF_CHECKED);
			else
				CheckMenuItem(hMenu, IDM_COMB, MF_BYCOMMAND | MF_UNCHECKED);
			if (config_sbar)
				CheckMenuItem(hMenu, IDM_SBAR, MF_BYCOMMAND | MF_CHECKED);
			else
				CheckMenuItem(hMenu, IDM_SBAR, MF_BYCOMMAND | MF_UNCHECKED);
			if (pSetLayeredWindowAttributes)
			{
				if (config_fadeout)
					CheckMenuItem(hMenu, IDM_FADEOUT, MF_BYCOMMAND | MF_CHECKED);
				else
					CheckMenuItem(hMenu, IDM_FADEOUT, MF_BYCOMMAND | MF_UNCHECKED);
			}
			if (config_grepwin)
				CheckMenuItem(hMenu, IDM_GREPWIN, MF_BYCOMMAND | MF_CHECKED);
			else
				CheckMenuItem(hMenu, IDM_GREPWIN, MF_BYCOMMAND | MF_UNCHECKED);
#ifdef USE_HISTORY
			CheckMenuItem(hMenu, (UINT_PTR)hHist, MF_BYCOMMAND | MF_UNCHECKED);
			CheckMenuItem(hMenu, IDM_HISTORY, MF_BYCOMMAND | MF_UNCHECKED);
			CheckMenuItem(hMenu, IDM_HAUTO,   MF_BYCOMMAND | MF_UNCHECKED);
			if (!config_ini)
			{
				if (config_history || config_hauto)
					CheckMenuItem(hMenu, (UINT_PTR)hHist, MF_BYCOMMAND | MF_CHECKED);
				if (config_history)
					CheckMenuItem(hMenu, IDM_HISTORY, MF_BYCOMMAND | MF_CHECKED);
				if (config_hauto)
					CheckMenuItem(hMenu, IDM_HAUTO, MF_BYCOMMAND | MF_CHECKED);
			}
#endif
			if (config_tray)
				CheckMenuItem(hMenu, IDM_TRAY, MF_BYCOMMAND | MF_CHECKED);
			else
				CheckMenuItem(hMenu, IDM_TRAY, MF_BYCOMMAND | MF_UNCHECKED);
			if (GuiWin == '1')
				CheckMenuItem(hMenu, IDM_ONEWIN, MF_BYCOMMAND | MF_CHECKED);
			else
				CheckMenuItem(hMenu, IDM_ONEWIN, MF_BYCOMMAND | MF_UNCHECKED);
			if (config_mouse)
				CheckMenuItem(hMenu, IDM_MOUSE, MF_BYCOMMAND | MF_CHECKED);
			else
				CheckMenuItem(hMenu, IDM_MOUSE, MF_BYCOMMAND | MF_UNCHECKED);
#ifdef NT106KEY
			if (config_nt106)
				CheckMenuItem(hMenu, IDM_NT106, MF_BYCOMMAND | MF_CHECKED);
			else
				CheckMenuItem(hMenu, IDM_NT106, MF_BYCOMMAND | MF_UNCHECKED);
#endif
			if (config_menu)
				CheckMenuItem(hMenu, IDM_MENU, MF_BYCOMMAND | MF_CHECKED);
			else
				CheckMenuItem(hMenu, IDM_MENU, MF_BYCOMMAND | MF_UNCHECKED);
			if (v_macro)
				CheckMenuItem(hMenu, IDM_TAIL, MF_BYCOMMAND | MF_CHECKED);
			else
				CheckMenuItem(hMenu, IDM_TAIL, MF_BYCOMMAND | MF_UNCHECKED);
			if (GuiConfig && !config_ini)
				EnableMenuItem(hMenu, IDM_COMB, MF_BYCOMMAND | MF_ENABLED);
			else
				EnableMenuItem(hMenu, IDM_COMB, MF_BYCOMMAND | MF_GRAYED);
			if (GuiConfig && !config_ini)
				EnableMenuItem(hMenu, IDM_COMS, MF_BYCOMMAND | MF_ENABLED);
			else
				EnableMenuItem(hMenu, IDM_COMS, MF_BYCOMMAND | MF_GRAYED);
			hMenu = v_menu;
		}
		DrawMenuBar(hWnd);
		return(0);
	case WM_COMMAND:
		wParam = LOWORD(wParam);
	case WM_SYSCOMMAND:
		selwin = NULL;
		bClear = TRUE;
		if (!s_cursor)
		{
			s_cursor = TRUE;
			ShowCursor(TRUE);
		}
		switch (wParam) {
		case IDM_PASTE:
		case IDM_YANK:
		case IDM_DELETE:
		case IDM_CLICK:
		case IDM_PRINT:
			break;
		default:
			if ((wParam & 0xf000) != 0xf000)
			{
				if (cmode)
					clear_cmode(hWnd);
				clear_visual(hWnd);
#if defined(KANJI) && defined(SYNTAX)
				if (syntax_on() && cmode)
					updateScreen(CLEAR);
#endif
				cmode = FALSE;
			}
			break;
		}
		switch (wParam) {
		case IDM_EXTEND:
			if (!v_extend)
			{
				HMENU			hMenu;
				HMENU			hFile;
				HMENU			hSetup;
				HMENU			hColor;

				hMenu = GetSystemMenu(hVimWnd, FALSE);
#if CUST_MENU
				hFile = GetSubMenu(hMenu, 5);
				AppendMenu(hFile, MF_STRING, IDM_TAIL,  "&Tail");
				hSetup = GetSubMenu(hMenu, 3);
#else
				hFile = GetSubMenu(hMenu, 10);
				AppendMenu(hFile, MF_STRING, IDM_TAIL,  "&Tail");
				hSetup = GetSubMenu(hMenu, 8);
#endif
				hColor = CreatePopupMenu();
				AppendMenu(hColor, MF_STRING, IDM_TBCOLOR,  "&TB");
				AppendMenu(hColor, MF_STRING, IDM_SOCOLOR,  "&SO");
				AppendMenu(hColor, MF_STRING, IDM_TICOLOR,  "T&I");
				AppendMenu(hColor, MF_STRING, IDM_DELCOLOR, "&DEL");
				InsertMenu(hSetup, IDM_BITMAP, MF_POPUP, (UINT_PTR)hColor,"&Extend Color");
				InsertMenu(hSetup, IDM_SAVE, MF_UNCHECKED, IDM_COMB, "Combi&nation");
				InsertMenu(hSetup, IDM_SAVE, MF_UNCHECKED, IDM_COMS, "C&ombination Command");
				hSetup = GetSubMenu(v_menu, 3);
				InsertMenu(hSetup, IDM_BITMAP, MF_POPUP, (UINT_PTR)hColor,"&Extend Color");
				InsertMenu(hSetup, IDM_SAVE, MF_UNCHECKED, IDM_COMB, "Combi&nation");
				InsertMenu(hSetup, IDM_SAVE, MF_UNCHECKED, IDM_COMS, "C&ombination Command");
			}
			v_extend = TRUE;
			break;
		case IDM_VER:
			{
				char		msg[1024];
				sprintf(msg,
#ifdef KANJI
					"    %s\r\nPorted to W32-GUI and Japanized Ver. %s",
					longVersion,
					longJpVersion
#else
					"%s",
					longVersion
#endif
				);
				MessageBox(hWnd, msg,
#ifdef KANJI
					JpVersion,
#else
					Version,
#endif
				MB_OK);
			}
			break;
		case IDM_VER2:
			{
				char		msg[4096];
				sprintf(msg,
#ifdef KANJI
					"    %s\r\nPorted to W32-GUI and Japanized Ver. %s",
					longVersion,
					longJpVersion
#else
					"%s",
					longVersion
#endif
				);
				if (vimgetenv("VIM") != NULL)
					sprintf(&msg[strlen(msg)],
							"\r\n    System Dir   %s", vimgetenv("VIM"));
				if (vimgetenv("HOME") != NULL)
					sprintf(&msg[strlen(msg)],
							"\r\n    Home Dir     %s", vimgetenv("HOME"));
				MessageBox(hWnd, msg,
#ifdef KANJI
					JpVersion,
#else
					Version,
#endif
				MB_OK);
			}
			break;
		case IDM_HELP:
			if ((State & NORMAL) && VIsual.lnum == 0 && !cmode)
			{
				docmdline(":help");
				cursor_refresh(hWnd);
			}
			break;
		case IDM_NFILE:
			if ((State & NORMAL) && ((curwin->w_arg_idx + 1) < arg_count)
												&& VIsual.lnum == 0 && !cmode)
				docmdline(":next");
			SendMessage(hVimWnd, WM_INITMENU, 0, 0);
			cursor_refresh(hWnd);
			break;
		case IDM_PFILE:
			if ((State & NORMAL) && (curwin->w_arg_idx >= 1)
												&& VIsual.lnum == 0 && !cmode)
				docmdline(":Next");
			SendMessage(hVimWnd, WM_INITMENU, 0, 0);
			cursor_refresh(hWnd);
			break;
		case IDM_TAIL:
			v_macro = !v_macro;
			if (v_macro)
			{
				SetTimer(hWnd, TAIL_TIME, config_show * 5, NULL);
				ZeroMemory(&byFile, sizeof(byFile));
			}
			else
				KillTimer(hWnd, TAIL_TIME);
			break;
		case IDM_MENU:
			config_menu = !config_menu;
			if (config_menu)
				SetMenu(hVimWnd, v_menu);
			else
				SetMenu(hVimWnd, NULL);
			do_resize = TRUE;
			{
				RECT		rcWindow;
				if (GetWindowRect(hWnd, &rcWindow))
				{
					config_x = rcWindow.left;
					config_y = rcWindow.top;
				}
			}
			if ((config_x & 0x7fffffff) > (DWORD)GetSystemMetrics(SM_CXSCREEN))
				config_x = 1;
			if ((config_y & 0x7fffffff) > (DWORD)GetSystemMetrics(SM_CYSCREEN))
				config_y = 1;
			nowCols = Columns;
			nowRows = Rows;
			MoveWindow(hWnd, config_x, config_y, config_w, config_h, TRUE);
			mch_get_winsize();
			break;
		case IDM_FONT:
#ifdef KANJI
			DialogBoxParamW(hInst, L"JFONT", hWnd, FontDialogProc, (LPARAM)NULL);
#else
			{
				CHOOSEFONT			cfFont;

				memset(&cfFont, 0, sizeof(cfFont));
				memcpy(&logfont, &config_font, sizeof(logfont));
				cfFont.lStructSize	= sizeof(cfFont);
				cfFont.hwndOwner	= hWnd;
				cfFont.hDC			= NULL;
				cfFont.rgbColors	= *v_fgcolor;
				cfFont.lpLogFont	= &logfont;
				cfFont.Flags		= CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT
										| CF_NOVERTFONTS | CF_FIXEDPITCHONLY ;
				cfFont.lCustData	= 0;
				cfFont.lpfnHook		= NULL;
				cfFont.lpTemplateName= NULL;
				cfFont.hInstance	= hInst;
				if (ChooseFont(&cfFont))
					memcpy(&config_font, &logfont, sizeof(logfont));
			}
#endif
			ResetScreen(hWnd);
			break;
		case IDM_LSPACE:
			if (pSetLayeredWindowAttributes != NULL)
				DialogBoxParamW(hInst, L"LINESPACEEX", hWnd, LineSpaceDialogEx, (LPARAM)NULL);
			else
				DialogBoxParamW(hInst, L"LINESPACE", hWnd, LineSpaceDialog, (LPARAM)NULL);
			ResetScreen(hWnd);
			break;
		case IDM_FILE:
			memset(&ofn, 0, sizeof(ofn));
			NameBuff[0] = '\0';
			IObuff[0] = '\0';
			if (curbuf->b_filename != NULL)
			{
				strcpy(IObuff, curbuf->b_filename);
				*gettail(IObuff) = NUL;
			}
			ofn.lStructSize		= sizeof(ofn);
			ofn.hwndOwner		= hWnd;
			ofn.hInstance		= hInst;
			ofn.lpstrFilter		= "ALL(*.*)\0*.*\0EFS(*.lzh;*.gz;*.Z;*.tgz;*.taz)\0*.lzh;*.gz;*.Z;*.tgz;*.taz;*.tar;*.tar.gz;*.tar.Z\0C(*.c;*.cpp;*.h;*.def;*.rc)\0*.c;*.cpp;*.h;*.def;*.rc\0DOC(*.txt;*.doc)\0*.txt;*.doc\0";
			ofn.lpstrCustomFilter = (LPSTR)NULL;
			ofn.nMaxCustFilter	= 0L;
			ofn.nFilterIndex	= 1;
			ofn.lpstrFile		= NameBuff;
			ofn.nMaxFile		= MAXPATHL;
			ofn.lpstrFileTitle	= NULL;
			ofn.nMaxFileTitle	= 0;
			ofn.lpstrInitialDir	= IObuff;
			ofn.lpstrTitle		= NULL;
			ofn.Flags			= OFN_HIDEREADONLY | OFN_CREATEPROMPT | OFN_ALLOWMULTISELECT
									| OFN_EXPLORER | OFN_NOCHANGEDIR /*| OFN_NOVALIDATE */;
			ofn.nFileOffset		= 0;
			ofn.nFileExtension	= 0;
			ofn.lpstrDefExt		= NULL;
			i = GetOpenFileName(&ofn);
			if (i)
			{
				int			change = FALSE;

				selwin = NULL;
				more = p_more;
				p_more = FALSE;
				++no_wait_return;
				if (!p_hid && curbuf->b_changed && (autowrite(curbuf) == FAIL))
					change = TRUE;
				if (change && win_split(0L, FALSE) == FAIL)
				{
					WIN 	*	nextwp;
					WIN		*	now = curwin;

					for (wp = firstwin; wp != NULL; wp = nextwp)
					{
						nextwp = wp->w_next;
						if (wp->w_buffer->b_changed && (autowrite(wp->w_buffer) == FAIL))
							;
						else if (lastwin != firstwin)
						{
							win_enter(wp, TRUE);
							close_window(TRUE);
						}
					}
					win_enter(now, TRUE);
				}
				else
					change = FALSE;
				if (change && win_split(0L, FALSE) == FAIL)
					;
				else
				{
					memset(IObuff, '\0', IOSIZE);
					strcpy(IObuff, ":args");
					if (NameBuff[ofn.nFileOffset - 1] != '\0')
					{
						strcat(IObuff, " \"");
						strcat(IObuff, NameBuff);
						strcat(IObuff, "\"");
					}
					else
					{
						col = strlen(NameBuff);
						for (;;)
						{
							col++;
							if (NameBuff[col] == '\0')
								break;
							if ((IOSIZE - strlen(IObuff))
									< (strlen(NameBuff) + strlen(&NameBuff[col]) + 5))
								break;
							strcat(IObuff, " \"");
							strcat(IObuff, NameBuff);
							strcat(IObuff, "\\");
							strcat(IObuff, &NameBuff[col]);
							strcat(IObuff, "\"");
							col += strlen(&NameBuff[col]);
						}
					}
					do_drag = TRUE;
					docmdline(IObuff);
					do_drag = FALSE;
				}
				--no_wait_return;
				p_more = more;
				cursor_refresh(hWnd);
			}
			break;
		case IDM_SFILE:
			selwin = NULL;
			more = p_more;
			p_more = FALSE;
			++no_wait_return;
			docmdline(":w!");
			--no_wait_return;
			p_more = more;
			cursor_refresh(hWnd);
			break;
		case IDM_AFILE:
			memset(&ofn, 0, sizeof(ofn));
			NameBuff[0] = '\0';
			IObuff[0] = '\0';
			if (curbuf->b_filename != NULL)
			{
				strcpy(IObuff, curbuf->b_filename);
				*gettail(IObuff) = NUL;
				strcpy(NameBuff, gettail(curbuf->b_filename));
			}
			ofn.lStructSize		= sizeof(ofn);
			ofn.hwndOwner		= hWnd;
			ofn.hInstance		= hInst;
			ofn.lpstrFilter		= NULL;
			ofn.lpstrCustomFilter = (LPSTR)NULL;
			ofn.nMaxCustFilter	= 0L;
			ofn.nFilterIndex	= 0;
			ofn.lpstrFile		= NameBuff;
			ofn.nMaxFile		= MAXPATHL;
			ofn.lpstrFileTitle	= NULL;
			ofn.nMaxFileTitle	= 0;
			ofn.lpstrInitialDir	= IObuff;
			ofn.lpstrTitle		= NULL;
			ofn.Flags			= OFN_HIDEREADONLY | OFN_CREATEPROMPT | OFN_OVERWRITEPROMPT
									| OFN_EXPLORER | OFN_NOCHANGEDIR /*| OFN_NOVALIDATE */;
			ofn.nFileOffset		= 0;
			ofn.nFileExtension	= 0;
			ofn.lpstrDefExt		= NULL;
			if (GetSaveFileName(&ofn))
			{
				selwin = NULL;
				more = p_more;
				p_more = FALSE;
				++no_wait_return;
				memset(IObuff, '\0', IOSIZE);
				strcpy(IObuff, ":w! ");
				strcat(IObuff, NameBuff);
				docmdline(IObuff);
				--no_wait_return;
				p_more = more;
				cursor_refresh(hWnd);
			}
			break;
		case IDM_BWHITE:
			*v_bgcolor = RGB(255, 255, 255);
			ResetScreen(hWnd);
			break;
		case IDM_BBLACK:
			*v_bgcolor = RGB(0, 0, 0);
			ResetScreen(hWnd);
			break;
		case IDM_FWHITE:
			*v_fgcolor = RGB(255, 255, 255);
			ResetScreen(hWnd);
			break;
		case IDM_FBLACK:
			*v_fgcolor = RGB(0, 0, 0);
			ResetScreen(hWnd);
			break;
		case IDM_FBLUE:
			*v_fgcolor = RGB(0, 0, 128);
			ResetScreen(hWnd);
			break;
		case IDM_BCOLOR:
			pColor	= v_bgcolor;
			goto SetColor;
		case IDM_FCOLOR:
			pColor	= v_fgcolor;
			goto SetColor;
		case IDM_TBCOLOR:
			pColor = v_tbcolor;
			goto SetColor;
		case IDM_SOCOLOR:
			pColor = v_socolor;
			goto SetColor;
		case IDM_TICOLOR:
			pColor = v_ticolor;
SetColor:
			memset(&cfColor, 0, sizeof(cfColor));
			cfColor.lStructSize	= sizeof(cfColor);
			cfColor.hwndOwner	= hWnd;
			cfColor.hInstance	= hInst;
			cfColor.rgbResult	= *pColor;
			cfColor.lpCustColors= config_color;
			cfColor.Flags		= 0;
			cfColor.lCustData	= 0;
			cfColor.lpfnHook	= NULL;
			cfColor.lpTemplateName	= NULL;
			if (ChooseColor(&cfColor))
				*pColor = cfColor.rgbResult;
			ResetScreen(hWnd);
			break;
		case IDM_DELCOLOR:
			*v_tbcolor = (-1);
			*v_socolor = (-1);
			*v_ticolor = (-1);
			ResetScreen(hWnd);
			break;
		case IDM_CANCEL:
			break;
		case IDM_PASTE:
			if (cmode)
				yank_cmode(hWnd, FALSE);
			else
			{
				long			size = 0;
				extern int		restart_edit;	/* this is in edit.c */

				if ((State & NORMAL) || State == INSERT)
				{
					char_u	*text = clip_get();

					if (text != NULL)
					{
						size = strlen((char *)text);
						free(text);
					}
				}
				if ((size > 2048) && !VIsual.lnum)
				{
					if (State & NORMAL)
					{
						yankbuffer = '@';
						if (restart_edit)
							doput(BACKWARD, 1, FALSE);
						else
							doput(FORWARD, 1, FALSE);
						yankbuffer = 0;
						curwin->w_cursor = curbuf->b_endop;
						cursor_refresh(hWnd);
						if (restart_edit && !lineempty(curwin->w_cursor.lnum))
						{
							inc_cursor();
							if (gchar_cursor() != NUL)
								dec_cursor();
						}
					}
					else if (keybuf_chk(3))
					{
						cbuf[c_end++] = Ctrl('O');
						cbuf[c_end++] = 'g';
						cbuf[c_end++] = 'V';
						NoMap = TRUE;
					}
					break;	/* no use keyboard buffer */
				}
				else
				{
					int				imode = 0;
					char_u		*	text = clip_get();

					if ((State & NORMAL) && !restart_edit)
						imode = 2;
					if (text != NULL)
					{
						if (!keybuf_chk(strlen((char *)text) + 1 + imode))
						{
							free(text);
							break;
						}
						if (imode)
						{
							if (VIsual.lnum)
							{
								cbuf[c_end++] = 's';
								bClear = FALSE;
							}
							else
								cbuf[c_end++] = 'a';
						}
						for (p = text; *p; )
						{
#ifdef KANJI
							/* the clipboard is UTF-8 here, see clip_get() */
							if (p[0] == 0xc2 && p[1] == 0xa0)
							{			/* no-break space: paste a space */
								cbuf[c_end++] = ' ';
								p += 2;
							}
							else if (ISkanji(*p))
							{
								int		n = utf_lenat(p, 0);

								while (n-- > 0)
									cbuf[c_end++] = *p++;
							}
							else
#endif
							{
								if (*p == '\r' && *(p+1) == '\n')
									++p;
								cbuf[c_end++] = *p++;
							}
						}
						if (imode)
							cbuf[c_end++] = ESC;
						free(text);
					}
				}
			}
			c_ind = c_end;
			if (c_ind > 0)
			{
				w_p_tw = curbuf->b_p_tw;
				w_p_wm = curbuf->b_p_wm;
				w_p_ai = curbuf->b_p_ai;
				w_p_si = curbuf->b_p_si;
				w_p_et = curbuf->b_p_et;
				w_p_uc = p_uc;
				w_p_sm = p_sm;
				w_p_ru = p_ru;
				w_p_ri = p_ri;
				w_p_paste = p_paste;
				p_uc = c_ind;
				curbuf->b_p_tw = 0;
				curbuf->b_p_wm = 0;
				curbuf->b_p_ai = FALSE;
				curbuf->b_p_si = FALSE;
				curbuf->b_p_et = FALSE;
				p_sm = 0;
				p_ru = 0;
				p_ri = 0;
				p_paste = TRUE;
			}
			break;
		case IDM_YANK:
			if (cmode)
			{
				yank_cmode(hWnd, TRUE);
				break;
			}
		case IDM_DELETE:
		case IDM_CLICK:
			if (VIsual.lnum)
			{
				curbuf->b_startop = VIsual;
				if (lt(curbuf->b_startop, curwin->w_cursor))
				{
					curbuf->b_endop = curwin->w_cursor;
					if (wParam != IDM_YANK)		/* notepad compatible */
						curwin->w_cursor = curbuf->b_startop;
				}
				else
				{
					curbuf->b_endop = curbuf->b_startop;
					curbuf->b_startop = curwin->w_cursor;
				}
				nlines = curbuf->b_endop.lnum - curbuf->b_startop.lnum + 1;
				mincl = TRUE;
				if (VIsual.col == VISUALLINE)
					mtype = MLINE;
				else
				{
					mtype = MCHAR;
					if (*ml_get_pos(&(curbuf->b_endop)) == NUL)
						mincl = FALSE;
				}
				curwin->w_set_curswant = 1;
				if (mtype == MCHAR && !mincl &&
									equal(curbuf->b_startop, curbuf->b_endop))
					break;
#ifdef KANJI
				if (mincl && curbuf->b_endop.col != VISUALLINE
										&& ISkanji(gchar(&curbuf->b_endop)))
				{
					mincl = FALSE;
					curbuf->b_endop.col += 2;
				}
#endif
				if (mtype == MCHAR && mincl == FALSE && curbuf->b_endop.col == 0 && nlines > 1)
				{
					--nlines;
					--curbuf->b_endop.lnum;
					if (inindent())
						mtype = MLINE;
					else
					{
						curbuf->b_endop.col = STRLEN(ml_get(curbuf->b_endop.lnum));
						if (curbuf->b_endop.col)
						{
							--curbuf->b_endop.col;
							mincl = TRUE;
						}
					}
				}
				if (Visual_block)				/* block mode */
				{
					colnr_t			n;
					startvcol = getvcol(curwin, &(curbuf->b_startop), 2);
					n = getvcol(curwin, &(curbuf->b_endop), 2);
					if (n < startvcol)
						startvcol = (colnr_t)n;
					endvcol = getvcol(curwin, &(curbuf->b_startop), 3);
					n = getvcol(curwin, &(curbuf->b_endop), 3);
					if (n > endvcol)
						endvcol = n;
					coladvance(startvcol);
				}
			}
			if ((wParam == IDM_YANK) && (VIsual.lnum))
			{
				operator = STRCHR(opchars, 'y') - opchars + 1;
				yankbuffer = '@';
				(void)doyank(FALSE);
				yankbuffer = 0;
				operator = NOP;
				goto get_clipdata;
			}
			else if (wParam == IDM_YANK)
				goto get_clipdata;
			else if ((wParam == IDM_DELETE) && (VIsual.lnum))
			{
				operator = STRCHR(opchars, 'd') - opchars + 1;
				yankbuffer = '@';
				dodelete();
				yankbuffer = 0;
				operator = NOP;
				goto get_clipdata;
			}
			else if (wParam == IDM_CLICK && VIsual.lnum
							&& curbuf->b_startop.lnum == curbuf->b_endop.lnum)
			{
				if (mtype == MLINE)
				{
					i = strlen(ml_get(curbuf->b_startop.lnum));
					curbuf->b_startop.col = 0;
				}
				else
				{
					i = curbuf->b_endop.col - curbuf->b_startop.col + 1 - !mincl;
#ifdef KANJI
					i = kanji_fixlen(ml_get(curbuf->b_startop.lnum),
									(int)curbuf->b_startop.col, (int)i);
#endif
				}
				if (i == 0 || i >= MAXPATHL || i >= IOSIZE)
					break;
				strncpy(IObuff, ml_get(curbuf->b_startop.lnum) + curbuf->b_startop.col, (int)i);
				IObuff[i] = NUL;
				rc = (INT_PTR)ShellExecute(NULL, NULL, IObuff, NULL, ".", SW_SHOW);
				if (!(rc == ERROR_FILE_NOT_FOUND || rc == ERROR_PATH_NOT_FOUND
						|| rc == SE_ERR_NOASSOC || rc == SE_ERR_ASSOCINCOMPLETE))
					break;
				if (FullName(IObuff, NameBuff, MAXPATHL) == OK
											&& strcmp(IObuff, NameBuff) != 0)
					ShellExecute(NULL, NULL, NameBuff, NULL, ".", SW_SHOW);
				break;
			}
			else
				break;
get_clipdata:
			if ((i = yank_to_clipboard(NULL)) != 0)
			{
				char_u	*text = alloc((unsigned)i + 1);

				if (text == NULL)
					break;
				i = yank_to_clipboard(text);
				(void)clip_put(text, (int)strlen((char *)text));
				free(text);
			}
			break;
		case IDM_COMB:
			config_comb = !config_comb;
			break;
		case IDM_SAVE:
			config_save = !config_save;
			break;
		case IDM_TRAY:
			config_tray = !config_tray;
			break;
		case IDM_ONEWIN:
			if (GuiWin == '1')
				GuiWin = 'w';
			else
				GuiWin = '1';
			break;
		case IDM_MOUSE:
			config_mouse = !config_mouse;
			break;
#ifdef NT106KEY
		case IDM_NT106:
			config_nt106 = !config_nt106;
			break;
#endif
		case IDM_OPEN:
			if (!(ver_info.dwPlatformId == VER_PLATFORM_WIN32_NT
										&& ver_info.dwMajorVersion == 3))
			{
				Shell_NotifyIcon(NIM_DELETE, &nIcon);
				ShowWindow(hWnd, SW_SHOW);
				OpenIcon(hWnd);
				TopWindow(hVimWnd);
			}
			break;
		case IDM_EXIT:
			bIClose = TRUE;
			PostMessage(hWnd, WM_CLOSE, 0, 0);
			break;
		case IDM_NEW:
			{
				char					name[MAXPATHL];
				char					command[MAXPATHL + 16];
				STARTUPINFO				si;
				PROCESS_INFORMATION		pi;

				GetModuleFileName(NULL, name, sizeof(name));
				memset(&pi, 0, sizeof(pi));
				memset(&si, 0, sizeof(si));
				si.cb = sizeof(si);
				if (config_ini)
					sprintf(command, "%s -I %s", name, GuiIni);
				else
					sprintf(command, "%s -n%d", name, GuiConfig);
				if (CreateProcess(NULL, command, NULL, NULL, FALSE,
						CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi) == TRUE)
				{
					CloseHandle(pi.hProcess);
					CloseHandle(pi.hThread);
				}
			}
			break;
		case IDM_PRINT:
			{
				char					filename[MAXPATHL];
				char				*	fn;
				int						textmode = curbuf->b_p_tx;
#ifdef USE_OPT
				int						opt = p_opt;
#endif
#ifdef KANJI
				char_u					code = *curbuf->b_p_jc;
#endif
				DWORD					hThreadID;
				extern DWORD WINAPI		PrinterThread(PVOID filename);

				fn = curbuf->b_sfilename != NULL ? gettail(curbuf->b_sfilename) : (char_u *)"Untitled";
				for (i = 0; i < 1000; i++)
				{
					filename[0] = '\0';
					if (p_dir != NULL && *p_dir != '\0')
					{
						if (*p_dir == '>')	/* skip '>' in front of dir name */
							STRCPY(filename, p_dir + 1);
						else
							STRCPY(filename, p_dir);
						if (!ispathsep(*(filename + STRLEN(filename) - 1)))
							STRCAT(filename, PATHSEPSTR);
					}
					if (i == 0)
						sprintf(&filename[STRLEN(filename)], "%s", fn);
					else
						sprintf(&filename[STRLEN(filename)], "%s(%d)", fn, i);
					if (getperm(filename) < 0)
						break;		/* for loop */
				}
				curbuf->b_p_tx = TRUE;
#ifdef USE_OPT
				p_opt = 0;
#endif
#ifdef KANJI
				*curbuf->b_p_jc = tolower(JP_SYS);
#endif
				++no_wait_return;
				if (VIsual.lnum)
				{
					curbuf->b_startop = VIsual;
					if (lt(curbuf->b_startop, curwin->w_cursor))
					{
						curbuf->b_endop = curwin->w_cursor;
						curwin->w_cursor = curbuf->b_startop;
					}
					else
					{
						curbuf->b_endop = curbuf->b_startop;
						curbuf->b_startop = curwin->w_cursor;
					}
#if 0
					if (1 < curbuf->b_startop.lnum
							|| curbuf->b_endop.lnum < curbuf->b_ml.ml_line_count)
						STRCAT(filename, "[digest]");
#endif
					buf_write(curbuf, filename, NULL,
							curbuf->b_startop.lnum, curbuf->b_endop.lnum,
							FALSE, 0, FALSE);
				}
				else
					buf_write(curbuf, filename, NULL,
							(linenr_t)1, curbuf->b_ml.ml_line_count,
							FALSE, 0, FALSE);
				--no_wait_return;
				updateScreen(CLEAR);
				cursor_refresh(hWnd);
#ifdef KANJI
				*curbuf->b_p_jc = code;
#endif
#ifdef USE_OPT
				p_opt = opt;
#endif
				curbuf->b_p_tx = textmode;
				fn = malloc(strlen(filename) + 1);
				strcpy(fn, filename);
				CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)PrinterThread, fn, 0, &hThreadID);
			}
			break;
		case IDM_PRINTSET:
			if (DialogBoxParamW(hInst, L"PRINTER", hWnd, PrinterDialog, (LPARAM)NULL) != 0)
				return(FALSE);
			break;
		case IDM_BITMAP:
			DialogBoxParamW(hInst, L"BITMAPSEL", hWnd, BitmapDialog, (LPARAM)NULL);
			if (!config_ini && config_bitmap)
			{
				v_tbcolor = &config_tbbitmap;
				v_socolor = &config_sobitmap;
				v_ticolor = &config_tibitmap;
			}
			else
			{
				v_tbcolor = &config_tbcolor;
				v_socolor = &config_socolor;
				v_ticolor = &config_ticolor;
			}
			bSyncPaint = TRUE;
			break;
		case IDM_BITMAPONOFF:
			config_bitmap = !config_bitmap;
			if (!isbitmap(config_bitmapfile, NULL))
				config_bitmap = FALSE;
			if (!config_ini && config_bitmap)
			{
				v_tbcolor = &config_tbbitmap;
				v_socolor = &config_sobitmap;
				v_ticolor = &config_tibitmap;
			}
			else
			{
				v_tbcolor = &config_tbcolor;
				v_socolor = &config_socolor;
				v_ticolor = &config_ticolor;
			}
			InvalidateRect(hWnd, NULL, TRUE);
			break;
		case IDM_BITMAPUP:
		case IDM_BITMAPDOWN:
			if (config_bitmap)
			{
				char_u		*	q;
				char			fname[FIND_NAMELEN];
				HANDLE          hFind;
				char			buf[MAXPATHL];
				char			first[MAXPATHL];
				char			before[MAXPATHL];
				BOOL			bFlg = TRUE;
				BOOL			bFind = FALSE;
				int				cnt = 0;

				STRCPY(buf, config_bitmapfile);
				q = buf;
				q = gettail(buf);
				*q = NUL;
				if (buf[0] && ispathsep(*(buf + STRLEN(buf) - 1)))
					q = buf + STRLEN(buf) - 1;
				*q = NUL;
				strcat(buf, "\\*.*");
				if ((hFind = find_first_name(buf, fname, sizeof(fname), NULL))
													!= INVALID_HANDLE_VALUE)
				{
					while (bFlg)
					{
						*q = NUL;
						if (STRLEN(buf) + STRLEN(fname) + 2 > sizeof(buf))
						{
							bFlg = find_next_name(hFind, fname,
													sizeof(fname), NULL);
							continue;
						}
						strcat(buf, "\\");
						strcat(buf, fname);
						if (isbitmap(buf, NULL))
						{
							if (bFind && wParam == IDM_BITMAPUP)
							{
								strcpy(config_bitmapfile, buf);
								bFind = FALSE;
								break;
							}
							if (cnt == 0)
								strcpy(first, buf);
							if (stricmp(buf, config_bitmapfile) == 0)
							{
								if (wParam == IDM_BITMAPDOWN && cnt)
								{
									strcpy(config_bitmapfile, before);
									break;
								}
								bFind = TRUE;
							}
							else
								strcpy(before, buf);
							cnt++;
						}
						bFlg = find_next_name(hFind, fname,
													sizeof(fname), NULL);
					}
					FindClose(hFind);
					if (bFind)
					{
						if (wParam == IDM_BITMAPUP)
							strcpy(config_bitmapfile, first);
						else if (cnt > 1)
							strcpy(config_bitmapfile, before);
					}
				}
				if (!isbitmap(config_bitmapfile, NULL))
					config_bitmap = FALSE;
				InvalidateRect(hWnd, NULL, TRUE);
			}
			break;
		case IDM_TRANSUP:
			if (v_trans <= 95)
			{
				v_trans += 5;
				SetLayerd();
			}
			break;
		case IDM_TRANSDOWN:
			if (v_trans >= 5)
				v_trans -= 5;
			else
				v_trans = 0;
			SetLayerd();
			break;
		case IDM_BITSIZEUP:
			if (config_bitmap)
			{
				config_bitsize += 5;
				if (config_bitsize > 100)
					config_bitsize = 100;
				InvalidateRect(hWnd, NULL, TRUE);
			}
			break;
		case IDM_BITSIZEDOWN:
			if (config_bitmap)
			{
				config_bitsize -= 5;
				if (config_bitsize < 10)
					config_bitsize = 10;
				InvalidateRect(hWnd, NULL, TRUE);
			}
			break;
		case IDM_WAVE:
			DialogBoxParamW(hInst, L"WAVE", hWnd, WaveDialog, (LPARAM)NULL);
			break;
		case IDM_WAVEONOFF:
			config_wave = !config_wave;
			if (!iswave(config_wavefile))
				config_wave = FALSE;
			break;
		case IDM_SBAR:
			config_sbar = !config_sbar;
			ScrollBar();
			mch_set_winsize();
			break;
		case IDM_FADEOUT:
			config_fadeout = !config_fadeout;
			break;
		case IDM_GREPWIN:
			config_grepwin = !config_grepwin;
			break;
#ifdef USE_HISTORY
		case IDM_HISTORY:
			config_history = !config_history;
			break;
		case IDM_HAUTO:
			config_hauto = !config_hauto;
			break;
		case IDM_HSAVE:
			{
				DWORD	hwork = config_hauto;

				config_hauto = TRUE;
				win_history_append(curbuf);
				config_hauto = hwork;
			}
			break;
#endif
		case IDM_COMS:
			if (v_extend && GuiConfig)
				DialogBoxParamW(hInst, L"COMMAND", hWnd, CommandDialog, (LPARAM)NULL);
			break;
		case IDM_CONFS:
			SaveConfig();
			break;
		case IDM_CONFUP:
			if (!config_ini)
			{
				int			find = 0;
				int			max = v_extend ? IDM_CONF9 : IDM_CONF3;
				HKEY		hKey;
				char		name[8];

				for (i = GuiConfig + 1; i <= (max - IDM_CONF0); i++)
				{
					sprintf(name, "Software\\Vim\\%d", i);
					if (RegOpenKeyEx(HKEY_CURRENT_USER, name, 0,
										KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS)
					{
						find = IDM_CONF0 + i;
						RegCloseKey(hKey);
						break;
					}
				}
				if (!find)
				{
					for (i = IDM_CONF0 - IDM_CONF0; i < GuiConfig; i++)
					{
						sprintf(name, "Software\\Vim\\%d", i);
						if (RegOpenKeyEx(HKEY_CURRENT_USER, name, 0,
										KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS)
						{
							find = IDM_CONF0 + i;
							RegCloseKey(hKey);
							break;
						}
					}
				}
				if (find)
					return(SendMessage(hWnd, WM_COMMAND, find, 0));
			}
			break;
		case IDM_CONFDOWN:
			if (!config_ini)
			{
				int			find = 0;
				int			max = v_extend ? IDM_CONF9 : IDM_CONF3;
				HKEY		hKey;
				char		name[8];

				for (i = GuiConfig - 1; i >= IDM_CONF0 - IDM_CONF0; i--)
				{
					sprintf(name, "Software\\Vim\\%d", i);
					if (RegOpenKeyEx(HKEY_CURRENT_USER, name, 0,
										KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS)
					{
						find = IDM_CONF0 + i;
						RegCloseKey(hKey);
						break;
					}
				}
				if (!find)
				{
					for (i = max ; i >= GuiConfig; i--)
					{
						sprintf(name, "Software\\Vim\\%d", i);
						if (RegOpenKeyEx(HKEY_CURRENT_USER, name, 0,
										KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS)
						{
							find = IDM_CONF0 + i;
							RegCloseKey(hKey);
							break;
						}
					}
				}
				if (find)
					return(SendMessage(hWnd, WM_COMMAND, find, 0));
			}
			break;
		default:
			if (IDM_CONF0 <= wParam && wParam <= IDM_CONF9)
			{
				if (RedrawingDisabled)
					break;
				if (!v_extend && IDM_CONF3 < wParam)
					break;
				flushbuf();
				SendMessage(hVimWnd, WM_SYSCOMMAND, SC_RESTORE, 0);
				if (config_comb)
					UnloadCommand();
				GuiConfig = wParam - IDM_CONF0;
				LoadConfig(FALSE);
				dpi_scale_to(dpi_of(hWnd));
				oldmix = 0;
				if (!config_ini && config_bitmap)
				{
					v_tbcolor = &config_tbbitmap;
					v_socolor = &config_sobitmap;
					v_ticolor = &config_tibitmap;
				}
				else
				{
					v_tbcolor = &config_tbcolor;
					v_socolor = &config_socolor;
					v_ticolor = &config_ticolor;
				}
				ResetScreen(hWnd);
				ScrollBar();
				do_resize = TRUE;
				if (!config_save)
				{
					RECT		rcWindow;
					if (GetWindowRect(hWnd, &rcWindow))
					{
						config_x = rcWindow.left;
						config_y = rcWindow.top;
					}
				}
				if ((config_x & 0x7fffffff) > (DWORD)GetSystemMetrics(SM_CXSCREEN))
					config_x = 1;
				if ((config_y & 0x7fffffff) > (DWORD)GetSystemMetrics(SM_CYSCREEN))
					config_y = 1;
				nowCols = Columns;
				nowRows = Rows;
				MoveWindow(hWnd, config_x, config_y, config_w, config_h, TRUE);
				mch_get_winsize();
				comp_Botline_all();
				cursor_refresh(hWnd);
				if (config_comb)
					LoadCommand();
				SetLayerd();
				break;
			}
#ifdef USE_HISTORY
			else if (IDM_HIST1 <= wParam && wParam <= IDM_HIST9)
			{
				char	*	cl;

				++no_wait_return;
				if ((cl = HistoryGetCommand(wParam - IDM_HISTM)) != NULL)
				{
					if (keybuf_chk(strlen(cl)))
					{
						memcpy(&cbuf[c_end], cl, strlen(cl));
						c_end += strlen(cl);
					}
				}
				--no_wait_return;
				break;
			}
#endif
			return(DefWindowProcW(hWnd, uMsg, wParam, lParam));
		}
		if (cmode)
			clear_cmode(hWnd);
		if (bClear)
			clear_visual(hWnd);
#if defined(KANJI) && defined(SYNTAX)
		if (syntax_on() && cmode)
			updateScreen(CLEAR);
#endif
		cmode = FALSE;
		return(0);
	case WM_TIMER:
		if (wParam == KEY_TIME)
			do_time = TRUE;
		else if (wParam == SHOW_TIME)
		{
			KillTimer(hWnd, SHOW_TIME);
			TopWindow(hVimWnd);
		}
		else if (wParam == TAIL_TIME)
		{
			HANDLE			hFile;
			FILETIME		nowFile;

			KillTimer(hWnd, TAIL_TIME);
			v_macro = FALSE;
			if (curbuf->b_changed || !(State & NORMAL)
					|| (lastwin != firstwin) || (curbuf->b_filename == NULL))
				return(0);
			hFile = CreateFile(curbuf->b_filename, GENERIC_READ,
										FILE_SHARE_READ | FILE_SHARE_WRITE,
										NULL, OPEN_EXISTING, 0, 0);
			if (hFile == INVALID_HANDLE_VALUE)
				return(0);
			v_macro = TRUE;
			while (GetFileTime(hFile, NULL, &nowFile, NULL)
								&& (CompareFileTime(&byFile, &nowFile) < 0))
			{
				if (curwin->w_cursor.lnum != curbuf->b_ml.ml_line_count)
					break;
				CopyMemory(&byFile, &nowFile, sizeof(byFile));
				more = p_more;
				p_more = FALSE;
				++no_wait_return;
				docmdline(":e!");
				--no_wait_return;
				p_more = more;
				curwin->w_cursor.lnum = curbuf->b_ml.ml_line_count;
				if (keybuf_chk(2))
				{
					cbuf[c_end++] = 'z';
					cbuf[c_end++] = '-';
				}
				beginline(TRUE);
				cursor_refresh(hWnd);
				break;
			}
			CloseHandle(hFile);
			SetTimer(hWnd, TAIL_TIME, config_show * 5, NULL);
		}
		else if ((wParam == MOUSE_TIME) && (selwin != NULL) && VIsual.lnum)
		{
			if (updown == 0 && leftright == 0)
				return(0);
			else if (updown > 0)
				oneup(updown);
			else if (updown < 0)
				onedown(-updown);
			else if (leftright < 0 && selwin->w_leftcol)
				oneleft();
			else if (leftright > 0 && selwin->w_p_wrap != TRUE)
			{
				if (p_ss == 0)
					i = Columns / 2;
				else
					i = p_ss;
				while (i--)
					oneright();
			}
			cursor_refresh(hWnd);
		}
		else if (wParam == TRIPLE_TIME)
			do_trip = FALSE;
		return(0);
	case WM_MOUSEACTIVATE:
		if (LOWORD(lParam) == HTCLIENT || LOWORD(lParam) == HTVSCROLL)
			return(MA_ACTIVATEANDEAT);
		return(MA_ACTIVATE);
	case WM_LBUTTONDOWN:
		if (cmode)
		{
			clear_cmode(hWnd);
#if defined(KANJI) && defined(SYNTAX)
			if (syntax_on())
				updateScreen(CLEAR);
#endif
			cmode = FALSE;
		}
		if (VIsual.lnum != 0)
		{
			updateScreen(NOT_VALID);
			cursor_refresh(hWnd);
			clear_visual(hWnd);
		}
		wp = get_linecol(lParam, &pos, &row, &col);
		if (GetKeyState(VK_MENU) & 0x8000)
		{
			if (do_trip)
			{
				int			start	= Columns;
				int			end		= 0;

				p = WinScreen[row];
				for (col = 0; col < Columns; col++)
				{
					if (p[col] != ' ')
					{
						start = col;
						break;
					}
				}
				for (col = Columns - 1; col >= 0; col--)
				{
					if (p[col] != ' ')
					{
						end = col;
						break;
					}
				}
				for (col = start; col <= end; col++)
				{
					mark_cmode(p, col);
					cmode = TRUE;
				}
				KillTimer(hWnd, TRIPLE_TIME);
				do_trip = FALSE;
				if (cmode)
				{
					rcWindow.left	= 0;
					rcWindow.right	= Columns * v_xchar - 1;
					rcWindow.top	= row * v_ychar;
					rcWindow.bottom	= (row + 1) * v_ychar - 1;
					InvalidateRect(hWnd, &rcWindow, FALSE);
				}
			}
			else
			{
				if (cmode)
					clear_cmode(hWnd);
				cs_row = row;
				cs_col = col;
				cmode = TRUE;
			}
		}
		else if (wp == NULL)
		{
			if (((wp = get_statusline(lParam, &row)) != NULL) && (State & NORMAL))
			{
				if (curwin != wp)
					win_enter(wp, TRUE);
				curwin->w_set_curswant = TRUE;
				updateScreen(CLEAR);
				cursor_refresh(hWnd);
				if (VIsual.lnum != 0)
					clear_visual(hWnd);
				selstatus = wp;
				cs_row = row;
			}
		}
		else if (wp == NULL || pos.lnum == 0)
			;
		else if (State & NORMAL)
		{
			wp->w_cursor = pos;
			if (curwin != wp)
				win_enter(wp, TRUE);
			curwin->w_set_curswant = TRUE;
			updateScreen(NOT_VALID);
			cursor_refresh(hWnd);
			if (VIsual.lnum != 0)
				clear_visual(hWnd);
			if (pos.lnum != 0 && SetTimer(hWnd, MOUSE_TIME, 60, NULL) != 0)
			{
				vmode = FALSE;
				selwin = wp;
				selpos = pos;
				if (wParam & MK_SHIFT)
					selpos.col = MAXCOL;
				else if (wParam & MK_CONTROL)
					vmode = TRUE;
				SetCapture(hWnd);
			}
		}
		else if (State & INSERT)
		{
			start_arrow();
			wp->w_cursor = pos;
			win_enter(wp, TRUE);
			curwin->w_set_curswant = TRUE;
			cursor_refresh(hWnd);
#ifdef FEPCTRL
			if (curbuf->b_p_fc && fep_get_mode())
				fep_win_sync(hVimWnd);
#endif
		}
		return(0);
	case WM_MOUSEMOVE:
		if (mouse_pos == lParam)
			return(0);
		mouse_pos = lParam;
		if (!s_cursor)
		{
			s_cursor = TRUE;
			ShowCursor(TRUE);
		}
		updown = 0;
		leftright = 0;
		if (cmode && (wParam & MK_LBUTTON))
		{
			get_linecol(lParam, &pos, &ce_row, &ce_col);
			draw_cmode(hVimWnd, cs_row, cs_col, ce_row, ce_col);
		}
		else if ((selwin != NULL) && (wParam & MK_LBUTTON))
		{
			GetClientRect(hVimWnd, &rcWindow);
			if ((short)HIWORD(lParam) < 0)
			{
				if ((short)HIWORD(lParam) < -(v_ychar * 5))
					updown = 5;
				else
					updown = 1;
				return(0);
			}
			else if (rcWindow.bottom < HIWORD(lParam))
			{
				if ((HIWORD(lParam) - rcWindow.bottom) > (v_ychar * 5))
					updown = -5;
				else
					updown = -1;
				return(0);
			}
			if ((short)LOWORD(lParam) < 0)
			{
				leftright = -1;
				lParam = MAKELONG(1, HIWORD(lParam));
			}
			else if (rcWindow.right < LOWORD(lParam))
			{
				leftright = 1;
				lParam = MAKELONG(rcWindow.right - 1, HIWORD(lParam));
			}
			if (((wp = get_linecol(lParam, &pos, &row, &col)) != NULL)
														&& (wp == selwin))
			{
				if (selpos.col == pos.col && selpos.lnum == pos.lnum)
					return(0);
				if (VIsual.lnum == 0)
				{
					VIsual = selpos;
					Visual_block = vmode;
					if (selpos.col == MAXCOL)
						wp->w_cursor.col = 0;
					else
						wp->w_cursor.col = selpos.col;
					wp->w_cursor.lnum = selpos.lnum;
					cursor_refresh(hWnd);
				}
				if (pos.lnum != 0)
					wp->w_cursor = pos;
				cursor_refresh(hWnd);
			}
			if (row < selwin->w_winpos)
			{
				updown = 1;
				leftright = 0;
			}
			else if ((selwin->w_winpos + selwin->w_height) <= row)
			{
				updown = -1;
				leftright = 0;
			}
			else
				updown = 0;
		}
		else if ((selstatus != NULL) && (wParam & MK_LBUTTON))
		{
			i = min(Rows - 1, (HIWORD(lParam) - 1) / v_ychar);
			if (i > cs_row)
				win_setheight(curwin->w_height + (i - cs_row));
			else if (i < cs_row)
				win_setheight(curwin->w_height - (cs_row - i));
			if (i == (curwin->w_winpos + curwin->w_height))
				cs_row = i;
			cursor_refresh(hWnd);
		}
		return(0);
	case WM_LBUTTONUP:
		if (selwin != NULL)
		{
			wp = get_linecol(lParam, &pos, &row, &col);
			if (wp == selwin
					&& selpos.col == pos.col && selpos.lnum == selpos.lnum)
				;
			else if (VIsual.lnum)
			{
				if ((wp != NULL) && (wp == selwin) && (pos.lnum != 0))
					wp->w_cursor = pos;
				updateScreen(VALID);
				cursor_refresh(hWnd);
			}
			selwin = NULL;
			KillTimer(hWnd, MOUSE_TIME);
			ReleaseCapture();
		}
		selstatus = NULL;
		return(0);
	case WM_LBUTTONDBLCLK:
		selwin = NULL;
		if (cmode)
			clear_cmode(hWnd);
		clear_visual(hWnd);
#if defined(KANJI) && defined(SYNTAX)
		if (syntax_on() && cmode)
			updateScreen(CLEAR);
#endif
		cmode = FALSE;
		wp = get_linecol(lParam, &pos, &row, &col);
		if (GetKeyState(VK_MENU) & 0x8000)
		{
			p = WinScreen[row];
#ifdef KANJI
			col = cell_head(row, col);
			if (CELLCP(row, col) > 0)
			{
				int class = cell_class(row, col);

				while (col > 0)
				{
					int		prev = cell_head(row, col - 1);

					if (CELLCP(row, prev) <= 0
							|| cell_class(row, prev) != class)
						break;
					col = prev;
				}
				while (col < Columns && CELLCP(row, col) > 0
						&& cell_class(row, col) == class)
				{
					int		w = cell_width(row, col);

					mark_cmode(p, col);
					if (w == 2 && col + 1 < Columns)
						mark_cmode(p, col + 1);
					col += w;
					cmode = TRUE;
				}
			}
			else
			{
				while (col > 0 && CELLCP(row, col - 1) == 0
						&& isidchar(p[col - 1]))
					--col;
				while (col < Columns && CELLCP(row, col) == 0
						&& isidchar(p[col]))
				{
					mark_cmode(p, col);
					col++;
					cmode = TRUE;
				}
			}
#else
			while (col > 0 && isidchar(ptr[col - 1]))
				--col;
			while (col < Columns && isidchar(p[col]))
			{
				p[Columns + col] |= CMODE;
				++col;
				cmode = TRUE;
			}
#endif
			if (cmode)
			{
				rcWindow.left	= 0;
				rcWindow.right	= Columns * v_xchar - 1;
				rcWindow.top	= row * v_ychar;
				rcWindow.bottom	= (row + 1) * v_ychar - 1;
				InvalidateRect(hWnd, &rcWindow, FALSE);
				if (SetTimer(hWnd, TRIPLE_TIME, GetDoubleClickTime(), NULL) != 0)
					do_trip = TRUE;
			}
		}
		else if (wp == NULL || pos.lnum == 0)
			;
		else if (State & NORMAL)
		{
			if (Visual_block)
				return(0);
			if (!(GetKeyState(VK_CONTROL) & 0x8000))
			{
				cbuf[c_end++] = 'g';
				cbuf[c_end++] = 'g';
				return(0);
			}
			p = ml_get_buf(wp->w_buffer, pos.lnum, FALSE);
			i = pos.col;
			while (i > 0 && !iswhite(p[i - 1]))
#ifdef KANJI
				if (ISkanjiPosition(p, i) == 2
						&& isjpspace(utf_prev(p, p + i)))
					break;
				else
#endif
				--i;
			p = &p[i];
			i = 0;
			while (p[i] != NUL && !iswhite(p[i]))
#ifdef KANJI
				if (isjpspace(p + i))
					break;
				else
#endif
				++i;
			if (i == 0 || i >= MAXPATHL || i >= IOSIZE)
				return(0);
			strncpy(IObuff, p, i);
			IObuff[i] = NUL;
			rc = (INT_PTR)ShellExecute(NULL, NULL, IObuff, NULL, ".", SW_SHOW);
			if (!(rc == ERROR_FILE_NOT_FOUND || rc == ERROR_PATH_NOT_FOUND
					|| rc == SE_ERR_NOASSOC || rc == SE_ERR_ASSOCINCOMPLETE))
				return(0);
			if (FullName(IObuff, NameBuff, MAXPATHL) == OK
									&& strcmp(IObuff, NameBuff) != 0)
			{
				rc = (INT_PTR)ShellExecute(NULL, NULL, NameBuff, NULL, ".", SW_SHOW);
				if (!(rc == ERROR_FILE_NOT_FOUND || rc == ERROR_PATH_NOT_FOUND
						|| rc == SE_ERR_NOASSOC || rc == SE_ERR_ASSOCINCOMPLETE))
					return(0);
			}
			p = ml_get_buf(wp->w_buffer, pos.lnum, FALSE);
			i = pos.col;
			while (i > 0 && p[i - 1] != '"')
				--i;
			if (i == 0 && p[0] != '"')
				return(0);
			p = &p[i];
			i = 0;
			while (p[i] != NUL && p[i] != '"')
				++i;
			if (i == 0 || i >= MAXPATHL || i >= IOSIZE || p[i] == NUL)
				return(0);
			strncpy(IObuff, p, i);
			IObuff[i] = NUL;
			ShellExecute(NULL, NULL, IObuff, NULL, ".", SW_SHOW);
		}
		return(0);
	case WM_RBUTTONUP:
		selwin = NULL;
		redraw = TRUE;
		if (!s_cursor)
		{
			s_cursor = TRUE;
			ShowCursor(TRUE);
		}
		GetWindowRect(hWnd, &rcWindow);
		hEdit = CreatePopupMenu();
		if (cmode)
		{
			AppendMenu(hEdit,  MF_STRING,   IDM_YANK,   "&Yank");
			if (State == CMDLINE || State == INSERT || State == REPLACE)
			{
				AppendMenu(hEdit,  MF_STRING,   IDM_PASTE,  "&Paste");
				if (GetKeyState(VK_MENU) & 0x8000)
				{
					redraw = FALSE;
					yank_cmode(hWnd, FALSE);
					clear_cmode(hWnd);
#if defined(KANJI) && defined(SYNTAX)
					if (syntax_on())
						updateScreen(CLEAR);
#endif
					cmode = FALSE;
				}
			}
		}
		else if (VIsual.lnum == 0)
		{
			AppendMenu(hEdit,  MF_STRING,   IDM_PASTE,  "&Put");
			if (State & NORMAL)
			{
				if ((curwin->w_arg_idx + 1) < arg_count)
					AppendMenu(hEdit,  MF_STRING,   IDM_NFILE,  "&Next");
				if (curwin->w_arg_idx >= 1)
					AppendMenu(hEdit,  MF_STRING,   IDM_PFILE,  "P&rev");
			}
		}
		else if (State & NORMAL)
		{
			AppendMenu(hEdit,  MF_STRING,   IDM_DELETE, "&Delete");
			AppendMenu(hEdit,  MF_STRING,   IDM_YANK,   "&Yank");
			AppendMenu(hEdit,  MF_STRING,   IDM_PASTE,  "P&ut");
			AppendMenu(hEdit,  MF_SEPARATOR,0,			NULL);
			AppendMenu(hEdit,  MF_STRING,   IDM_CLICK,  "&Run");
			if (strlen(config_printer))
			{
				AppendMenu(hEdit,  MF_SEPARATOR,0,			NULL);
				AppendMenu(hEdit,  MF_STRING,   IDM_PRINT,  "&Print");
			}
		}
		if (redraw)
		{
			AppendMenu(hEdit,  MF_SEPARATOR,0,			NULL);
			AppendMenu(hEdit,  MF_STRING,   IDM_CANCEL, "&Cancel");
			TrackPopupMenu(hEdit, TPM_LEFTALIGN|TPM_LEFTBUTTON|TPM_RIGHTBUTTON,
					rcWindow.left + LOWORD(lParam),
					rcWindow.top + HIWORD(lParam) + (cmode || VIsual.lnum == 0 ? v_ychar - 2 : -2),
					0, hWnd, NULL);
		}
		DestroyMenu(hEdit);
		return(0);
#ifdef WM_MOUSEWHEEL
	case WM_MOUSEWHEEL:
		{
			WPARAM			UpDown;

			if ((signed)wParam < 0)
				UpDown = SB_LINEDOWN;
			else
				UpDown = SB_LINEUP;
			for (i = 0; i < iScrollLines; i++)
				PostMessage(hWnd, WM_VSCROLL, UpDown, 0);
		}
		return(0);
#endif
	case WM_VSCROLL:
		selwin = NULL;
		if (cmode)
		{
			clear_cmode(hWnd);
#if defined(KANJI) && defined(SYNTAX)
			if (syntax_on())
				updateScreen(CLEAR);
#endif
			cmode = FALSE;
		}
		clear_visual(hWnd);
		if (!((State & NORMAL) || (State & INSERT)))
			return(0);
		if (State & INSERT)
			start_arrow();
		switch (LOWORD(wParam)) {
		case SB_LINEDOWN:
			if (curwin->w_empty_rows
							&& curwin->w_botline >= curbuf->b_ml.ml_line_count)
				;
			else
			{
				scrollup(1);
				coladvance(curwin->w_curswant);
				updateScreen(VALID);
			}
			break;
		case SB_LINEUP:
			scrolldown(1);
			coladvance(curwin->w_curswant);
			updateScreen(VALID);
			break;
		case SB_PAGEDOWN:
			if (curwin->w_empty_rows
							&& curwin->w_botline >= curbuf->b_ml.ml_line_count)
				;
			else
				(void)onepage(FORWARD, 1);
			break;
		case SB_PAGEUP:
			if (curwin->w_cursor.lnum <= (Rows - 1))
			{
				curwin->w_cursor.lnum = 1;
				beginline(TRUE);
			}
			else
				(void)onepage(BACKWARD, 1);
			break;
		case SB_THUMBTRACK:
			{
				linenr_t	old = curbuf->b_ml.ml_line_count;
				linenr_t	lnum = curbuf->b_ml.ml_line_count;
				INT			high = curwin->w_height - curwin->w_status_height;
				linenr_t	disp = (high * lnum) / curbuf->b_ml.ml_line_count;
				INT			param;

				while (lnum > 0x7ff)
					lnum >>= 1;
				if (HIWORD(wParam) < 1)
					param = 0;
				else
					param = ((HIWORD(wParam) + 1) * lnum) / (lnum - disp);
				lnum = (param * curbuf->b_ml.ml_line_count) / lnum;
				if (lnum <= 0)
					lnum = 1;
				else if (lnum >= curbuf->b_ml.ml_line_count)
					lnum = curbuf->b_ml.ml_line_count;
				curwin->w_cursor.lnum = lnum;
				beginline(TRUE);
			}
			break;
		case SB_ENDSCROLL:
			break;
		}
		cursor_refresh(hWnd);
		return(0);
	case WM_QUERYENDSESSION:
		hWnd = CreateDialog(hInst, "TERM", NULL, LoadDialog);
		ShowWindow(hWnd, SW_NORMAL);
		Sleep(1000);
		ml_sync_all(FALSE);
		ctrlc_pressed = TRUE;
		for (buf = firstbuf; buf != NULL; buf = buf->b_next)
		{
			if (buf->b_changed && (autowrite(buf) == FAIL))
			{
				DestroyWindow(hWnd);
				return(TRUE);
			}
		}
		getout(0);
		return(TRUE);
	case WM_ENDSESSION:
		ShowWindow(hWnd, SW_HIDE);
		break;
	case WM_CLOSE:
		if (config_tray && BenchTime == 0 && !bIClose
				&& (!(ver_info.dwPlatformId == VER_PLATFORM_WIN32_NT
										&& ver_info.dwMajorVersion == 3)))
		{
			ShowWindow(hWnd, SW_HIDE);
			GetWindowText(hWnd, nIcon.szTip, sizeof(nIcon.szTip));
			Shell_NotifyIcon(NIM_ADD, &nIcon);
			return(0);
		}
		if (bIClose && !(ver_info.dwPlatformId == VER_PLATFORM_WIN32_NT
											&& ver_info.dwMajorVersion == 3))
		{
			Shell_NotifyIcon(NIM_DELETE, &nIcon);
			ShowWindow(hWnd, SW_SHOW);
			OpenIcon(hWnd);
			TopWindow(hVimWnd);
		}
		bIClose = FALSE;
		ctrlc_pressed = TRUE;
		for (buf = firstbuf; BenchTime == 0 && buf != NULL; buf = buf->b_next)
		{
			if (buf->b_changed && (autowrite(buf) == FAIL))
			{
				i = (int)DialogBoxParamW(hInst, L"QUITCONFIRM", hWnd, QuitConfirmDialog, 0L);
				switch (i) {
				case IDYES:
					i = 0;
					for (buf = firstbuf; buf != NULL; buf = buf->b_next)
					{
						if (buf->b_changed && buf->b_filename == NULL)
						{
							char		batbuf[MAXPATHL];

							for (i++; i <= 0xfffff; i++)
							{
								batbuf[0] = '\0';
								if (p_dir != NULL && *p_dir != NUL)
								{
									if (*p_dir == '>')
										STRCPY(batbuf, p_dir + 1);
									else
										STRCPY(batbuf, p_dir);
									if (!ispathsep(*(batbuf + STRLEN(batbuf) - 1)))
										STRCAT(batbuf, PATHSEPSTR);
								}
								sprintf(&batbuf[STRLEN(batbuf)], "bak%05x.txt", i);
								if (getperm(batbuf) < 0)
								{
									buf->b_filename = strsave(batbuf);
									break;		/* for loop */
								}
							}
						}
					}
					docmdline(":wqall!");
					break;
				case IDNO:
					docmdline(":qall!");
					break;
				case IDCANCEL:
					return(0);
				}
				break;
			}
		}
		bWClose = TRUE;
		docmdline(":qall!");
		/* no break */
	default:
#ifndef NO_WHEEL
		if (uiMsh_MsgMouseWheel != 0 && uMsg == uiMsh_MsgMouseWheel)
			return(SendMessage(hWnd, WM_MOUSEWHEEL, wParam, lParam));
#endif
		break;
	}
	return(DefWindowProcW(hWnd, uMsg, wParam, lParam));
}

/*
 *
 */
void
wincmd_paste(void)
{
	if (GuiWin)
		SendMessage(hVimWnd, WM_COMMAND, IDM_PASTE, 0);
	NoMap = FALSE;
}

/*
 *
 */
void
wincmd_cut(void)
{
	if (GuiWin)
		SendMessage(hVimWnd, WM_COMMAND, IDM_YANK, 0);
}

/*
 *
 */
void
wincmd_delete(void)
{
	if (GuiWin)
		SendMessage(hVimWnd, WM_COMMAND, IDM_DELETE, 0);
}

/*
 *
 */
void
wincmd_active(void)
{
	if (GuiWin)
	{
		SetForegroundWindow(hVimWnd);
		SetTimer(hVimWnd, SHOW_TIME, config_show, NULL);
	}
}

/*
 *
 */
void
wincmd_redraw(void)
{
	if (GuiWin)
		bSyncPaint = TRUE;
}

/*
 *
 */
int
wincmd_grep(char *linep, char *filep)
{
	char					name[MAXPATHL];
	char					command[1024];
	STARTUPINFO				si;
	PROCESS_INFORMATION		pi;

	if (GuiWin && config_grepwin)
	{
		GetModuleFileName(NULL, name, sizeof(name));
		memset(&pi, 0, sizeof(pi));
		memset(&si, 0, sizeof(si));
		si.cb = sizeof(si);
		if (config_ini)
			sprintf(command, "%s -I %s -no -v +%s %s", name, GuiIni,    linep, filep);
		else
			sprintf(command, "%s -n%d  -no -v +%s %s", name, GuiConfig, linep, filep);
		if (CreateProcess(NULL, command, NULL, NULL, FALSE,
				CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi) == TRUE)
		{
			CloseHandle(pi.hProcess);
			CloseHandle(pi.hThread);
			return(TRUE);
		}
	}
	return(FALSE);
}

/*
 *
 */
int
wincmd_togle(void)
{
	HWND			hWnd;
	char			buf[128];

	if (GuiWin)
	{
		hWnd = GetFirstSibling(hVimWnd);
		while (IsWindow(hWnd))
		{
			GetClassName(hWnd, buf, sizeof(buf));
			if (strcmp(buf, szAppName) == 0 && hVimWnd != hWnd)
			{
				TopWindow(hWnd);
				return(TRUE);
			}
			hWnd = GetNextSibling(hWnd);
		}
	}
	return(FALSE);
}

/*
 *
 */
int
wincmd_rotate(void)
{
	HWND			hWnd;
	char			buf[128];

	if (GuiWin)
	{
		hWnd = GetLastSibling(hVimWnd);
		while (IsWindow(hWnd))
		{
			GetClassName(hWnd, buf, sizeof(buf));
			if (strcmp(buf, szAppName) == 0 && hVimWnd != hWnd)
			{
				TopWindow(hWnd);
				return(TRUE);
			}
			hWnd = GetPrevSibling(hWnd);
		}
	}
	return(FALSE);
}

/*
 *
 */
	void
vim_delay(void)
{
	delay(500);
}

/*
 * this version of remove is not scared by a readonly (backup) file
 */
	int
vim_remove(char_u *name)
{
	setperm(name, S_IREAD|S_IWRITE);    /* default permissions */
	return unlink(name);
}

	static void
ScrollBar(void)
{
	SCROLLINFO		si;
	static UINT		sbar = (-1);
	static SCROLLINFO		oSi;

	if (curbuf == NULL || curwin == NULL)
		return;
	if (config_sbar)
	{
		INT			nPos, nPage;
		INT			high = curwin->w_height - curwin->w_status_height;
		linenr_t	lnum;
		linenr_t	elnum = curwin->w_empty_rows ? curwin->w_empty_rows - 1 : 0;
		INT			shift = 0;

		if (high < 1)
			high = 1;

		lnum = curbuf->b_ml.ml_line_count + elnum;
		while (lnum > 0x7ff)
		{
			shift++;
			lnum >>= 1;
		}
		high >>= shift;
		if (high <= 0)
			high = 1;
		nPage = ((curwin->w_botline - curwin->w_topline - 1) * lnum)
										/ (curbuf->b_ml.ml_line_count + elnum);
		if (nPage < 1)
			nPage = 1;
		nPos  = (curwin->w_topline >> shift) - 1;
		if (nPos < 0)
			nPos = 0;

		memset(&si, 0, sizeof(si));
		si.cbSize = sizeof(si);

		if (config_sbar != sbar)
		{
			si.fMask = SIF_ALL;
			si.nMin  = 0;
			si.nMax  = 1;
			si.nPos  = 0;
			si.nPage = 1;
			SetScrollInfo(hVimWnd, SB_VERT, &si, TRUE);
		}

		si.fMask = SIF_ALL | SIF_DISABLENOSCROLL;
		si.nMin  = 0;
		si.nMax  = lnum - 1;
		si.nPos  = nPos;
		si.nPage = nPage;
		if ((curwin->w_botline - 1) >= curbuf->b_ml.ml_line_count)
			si.nPage = lnum - nPos;
		if (curwin->w_topline <= 1
					&& (curwin->w_botline - 1) >= curbuf->b_ml.ml_line_count)
			si.nMax = 0;
		if (memcmp(&oSi, &si, sizeof(si)) != 0)
		{
			SetScrollInfo(hVimWnd, SB_VERT, &si, TRUE);
			memcpy(&oSi, &si, sizeof(si));
		}
		sbar = config_sbar;
	}
	else if (config_sbar != sbar)
	{
		sbar = config_sbar;
		memset(&oSi, 0, sizeof(oSi));
		oSi.cbSize = sizeof(oSi);
		oSi.fMask  = SIF_ALL;
		SetScrollInfo(hVimWnd, SB_VERT, &oSi, TRUE);
	}
}

/*
 * mch_write(): write the output buffer to the screen
 */
	void
mch_write(char_u *s, int len)
{
	char_u         *p;
	WORD			row,
					col;
	static DWORD	btime = 0;

	s[len] = '\0';
	if (GuiWin)
	{
		RECT			rect;

		ScrollBar();
		while (len--)
		{
			if (s[0] == '\n')
			{
				v_col = 0;
				v_row++;
				if (v_row >= (Rows - 1))
				{
					v_row = Rows - 1;
					if (!config_bitmap)
						ScrollWindow(hVimWnd, 0, v_ychar, NULL, NULL);
					else
						InvalidateRect(hVimWnd, NULL, FALSE);
				}
				s++;
				continue;
			}
			else if (s[0] == '\r')
			{
				v_col = 0;
				s++;
				continue;
			}
			else if (s[0] == '\b')		/* backspace */
			{
				if (--v_col < 0)
					v_col = 0;
				s++;
				continue;
			}
			else if (s[0] == '\a' || s[0] == '\007')
			{
				if (p_vb)
				{
					do_vb = TRUE;
					InvalidateRect(hVimWnd, NULL, FALSE);
					UpdateWindow(hVimWnd);
					delay(50);
					do_vb = FALSE;
					InvalidateRect(hVimWnd, NULL, FALSE);
					UpdateWindow(hVimWnd);
					Sleep(50);
				}
				else if (config_wave)
					sndPlaySound(config_wavefile, SND_ASYNC|SND_NOSTOP);
				else if ((btime + 50) < GetTickCount())
				{
					MessageBeep(0);
					btime = GetTickCount();
					if (BenchTime)
						ctrlc_pressed = TRUE;
				}
				s++;
				continue;
			} else if (s[0] == ESC && len > 1 && s[1] == '|') {
				switch (s[2]) {
				case 'v':
					if (v_cursor && v_focus)
						HideCaret(hVimWnd);
					v_cursor = FALSE;
					goto wgot3;

				case 'V':
					if (v_cursor != TRUE && v_focus)
						ShowCaret(hVimWnd);
					v_cursor = TRUE;
					MoveCursor(hVimWnd);
					goto wgot3;

				case 'J':	/* clear screen */
					rect.left	= 0;
					rect.right	= Columns * v_xchar;
					rect.top	= 0;
					rect.bottom	= Rows * v_ychar;
					if (!config_bitmap)
					{
						HDC			hDC;
						HBRUSH		hbrush;
						HBRUSH		holdbrush;
						BOOL		hide = FALSE;

						if (v_cursor && v_focus)
						{
							HideCaret(hVimWnd);
							hide = TRUE;
						}
						UpdateWindow(hVimWnd);
						hDC = GetDC(hVimWnd);
						hbrush	= CreateSolidBrush(*v_bgcolor);
						holdbrush = SelectObject(hDC, hbrush);
						FillRect(hDC, &rect, hbrush);
						SelectObject(hDC, holdbrush);
						DeleteObject(hbrush);
						ReleaseDC(hVimWnd, hDC);
						if (hide)
							ShowCaret(hVimWnd);
					}
					else
						InvalidateRect(hVimWnd, &rect, FALSE);
					goto wgot3;

				case 'K':	/* clreol */
					rect.left	= v_col * v_xchar;
					rect.right	= Columns * v_xchar;
					rect.top	= v_row * v_ychar;
					rect.bottom	= rect.top + v_ychar;
					if (!config_bitmap)
					{
						HDC			hDC;
						HBRUSH		hbrush;
						HBRUSH		holdbrush;
						BOOL		hide = FALSE;

						if (v_cursor && v_focus)
						{
							HideCaret(hVimWnd);
							hide = TRUE;
						}
						UpdateWindow(hVimWnd);
						hDC = GetDC(hVimWnd);
						hbrush	= CreateSolidBrush(*v_bgcolor);
						holdbrush = SelectObject(hDC, hbrush);
						FillRect(hDC, &rect, hbrush);
						SelectObject(hDC, holdbrush);
						DeleteObject(hbrush);
						ReleaseDC(hVimWnd, hDC);
						if (hide)
							ShowCaret(hVimWnd);
					}
					else
						InvalidateRect(hVimWnd, &rect, FALSE);
					/*
					 * The blanking above goes straight to the screen and
					 * redraws no text, so ink from the character ending at
					 * v_col - 1 that leans into v_col is wiped and never comes
					 * back. That was the right side of the last character of a
					 * line going missing. Ask for that character again;
					 * PaintWindow() widens what it is given by a cell, which
					 * takes in the left half of a double width one.
					 *
					 * Afterwards, never before: the UpdateWindow() above would
					 * otherwise service it and the FillRect() wipe it again.
					 */
					if (v_col > 0)
					{
						RECT		lr;

						lr.left		= (v_col - 1) * v_xchar;
						lr.right	= v_col * v_xchar;
						lr.top		= rect.top;
						lr.bottom	= rect.bottom;
						InvalidateRect(hVimWnd, &lr, FALSE);
					}
					goto wgot3;

				case 'L':	/* insline */
					rect.left	= 0;
					rect.top	= v_row * v_ychar;
					rect.right	= Columns * v_xchar;
					rect.bottom	= v_region * v_ychar;
					if (!config_bitmap)
						ScrollWindow(hVimWnd, 0, v_ychar, NULL, &rect);
					else
						InvalidateRect(hVimWnd, &rect, FALSE);
					goto wgot3;

				case 'M':	/* delline */
					rect.left	= 0;
					rect.top	= v_row * v_ychar;
					rect.right	= Columns * v_xchar;
					rect.bottom	= v_region * v_ychar;
					if (!config_bitmap)
						ScrollWindow(hVimWnd, 0, -v_ychar, NULL, &rect);
					else
						InvalidateRect(hVimWnd, &rect, FALSE);
			wgot3:  s += 3;
					len -= 2;
					continue;

				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
					p = s + 2;
					row = getdigits(&p);        /* no check for length! */
					if (p > s + len)
						break;
					if (*p == ';')
					{
						++p;
						col = getdigits(&p);    /* no check for length! */
						if (p > s + len)
							break;
						if (*p == 'H')
						{
							if (!RedrawingDisabled && !((row - 2) <= v_row && v_row <= row))
								UpdateWindow(hVimWnd);
							v_col = col - 1;
							v_row = row - 1;
							len -= p - s;
							s = p + 1;
							continue;
						}
						else if (*p == 'S')
						{
							v_region = col + 1;
							len -= p - s;
							s = p + 1;
							continue;
						}
					}
					else if (*p == 'm')
					{
						/* video color */
						len -= p - s;
						s = p + 1;
						continue;
					}
					else if (*p == 'L')
					{
						/* insline(row) */
						rect.left	= 0;
						rect.top	= v_row * v_ychar;
						rect.right	= Columns * v_xchar;
						rect.bottom	= v_region * v_ychar;
						if (!config_bitmap)
							ScrollWindow(hVimWnd, 0, v_ychar * row, NULL, &rect);
						else
							InvalidateRect(hVimWnd, &rect, FALSE);
						len -= p - s;
						s = p + 1;
						continue;
					}
					else if (*p == 'M')
					{
						/* delline(row) */
						rect.left	= 0;
						rect.top	= v_row * v_ychar;
						rect.right	= Columns * v_xchar;
						rect.bottom	= v_region * v_ychar;
						if (!config_bitmap)
							ScrollWindow(hVimWnd, 0, -(v_ychar * row), NULL, &rect);
						else
							InvalidateRect(hVimWnd, &rect, FALSE);
						len -= p - s;
						s = p + 1;
						continue;
					}
				}
			}
			else
			{
				int           prefix = 1;
				int           width = 1;

				if (len >= 2)
				{
					int		i;
					int		w = 0;

					prefix = strcspn(s, "\n\r\a\b\033\007");
					/*
					 * Take the run up to the end of the row, counting the
					 * columns the characters occupy and not the bytes they are
					 * made of. charsize() is 0 for a trailing byte, so summing
					 * it over the bytes gives the width and the run can only be
					 * cut where a character starts.
					 *
					 * This used to advance v_col by the byte count. The screen
					 * holds UTF-8, so that ran ahead by a byte for every kana
					 * -- which is how v_col came to be described as drifting and
					 * why the damaged rectangle is a whole row. It was hidden
					 * while flushbuf() folded the stream into Shift-JIS on the
					 * way here, where a kana happens to be two bytes for its two
					 * columns; nothing outside the code page ever added up.
					 */
					for (i = 0; i < prefix; i++)
					{
						int		cw = charsize(s + i);

						if (w + cw > (int)Columns - v_col)
						{
							prefix = i;
							break;
						}
						w += cw;
					}
					if (prefix <= 0)
					{
						prefix = 1;
						w = charsize(s);
					}
					width = w;
				}
				else
					width = charsize(s);
				/*
				 * The whole row, not the cells this run is thought to touch.
				 *
				 * prefix is a count of bytes and v_col a count of columns, and
				 * the screen holds UTF-8, so the two part company the moment
				 * anything is not ASCII: three bytes for the two columns of a
				 * kana, four for the two of an emoji. Everything below treats
				 * them as the same thing, which leaves v_col running ahead of
				 * where the text really is.
				 *
				 * Reckoning it properly turns out to be the wrong repair. The
				 * damaged rectangle came out too wide for the same reason, and
				 * too wide is harmless -- it is what kept the display honest
				 * while v_col drifted. Made exact, it stopped covering for the
				 * drift and lines began to end early. So the rectangle is the
				 * row now: nothing can be left unpainted, ink that leans out
				 * of its cells is inside the clip region wherever it lands,
				 * and BeginPaint() is spared the arithmetic. A row is one or
				 * two FillRects and a handful of ExtTextOuts, which is not
				 * worth being clever about.
				 */
				rect.left	= 0;
				rect.right	= Columns * v_xchar;
				rect.top	= v_row * v_ychar;
				rect.bottom	= rect.top + v_ychar;
				InvalidateRect(hVimWnd, &rect, FALSE);
				v_col += width;
				s += prefix;
				len -= prefix - 1;
				if (v_col >= Columns)
				{
					v_col = 0;
					v_row++;
					if (v_row >= Rows)
					{
						v_row = Rows - 1;
						if (!config_bitmap)
							ScrollWindow(hVimWnd, 0, v_ychar, NULL, NULL);
						else
							InvalidateRect(hVimWnd, NULL, FALSE);
					}
				}
				continue;
			}
			s++;
		}
	}
	else if (IsTelnet || !term_console)
		write(1, s, (unsigned)len);
	else
	{
		while (len--) {

			/* optimization: use one single WriteConsole for runs of text,
			   rather than calling putch() multiple times.  It ain't curses,
			   but it helps. */

			DWORD           prefix = strcspn(s, "\n\r\a\033");

			if (prefix) {
				DWORD           nwritten;

				if (WriteConsole(hConOut, s, prefix, &nwritten, 0)) {

					len -= (nwritten - 1);
					s += nwritten;
				}
				continue;
			}

			if (s[0] == '\n') {
				if (ntcoord.Y == (Rows - 1)) {
					gotoxy(1, ntcoord.Y + 1);
					scroll();
				} else {
					gotoxy(1, ntcoord.Y + 2);
				}
				s++;
				continue;
			} else if (s[0] == '\r') {
				gotoxy(1, ntcoord.Y + 1);
				s++;
				continue;
			} else if (s[0] == '\a') {
				vbell();
				s++;
				continue;
			} else if (s[0] == ESC && len > 1 && s[1] == '|') {
				switch (s[2]) {

				case 'v':
					cursor_visible(0);
					goto got3;

				case 'V':
					cursor_visible(1);
					goto got3;

				case 'J':
					clrscr();
					goto got3;

				case 'K':
					clreol();
					goto got3;

				case 'L':
					insline(1);
					goto got3;

				case 'M':
					delline(1);
			got3:   s += 3;
					len -= 2;
					continue;

				case '0':
				case '1':
				case '2':
				case '3':
				case '4':
				case '5':
				case '6':
				case '7':
				case '8':
				case '9':
					p = s + 2;
					row = getdigits(&p);        /* no check for length! */
					if (p > s + len)
						break;
					if (*p == ';') {
						++p;
						col = getdigits(&p);    /* no check for length! */
						if (p > s + len)
							break;
						if (*p == 'H') {
							gotoxy(col, row);
							len -= p - s;
							s = p + 1;
							continue;
						}
					} else if (*p == 'm') {
						if (row == 0)
							normvideo();
						else
							textattr(row);
						len -= p - s;
						s = p + 1;
						continue;
					} else if (*p == 'L') {
						insline(row);
						len -= p - s;
						s = p + 1;
						continue;
					} else if (*p == 'M') {
						delline(row);
						len -= p - s;
						s = p + 1;
						continue;
					}
				}
			}
			putch(*s++);
		}
	}
}

/*
 *  Keyboard translation tables.
 *  (Adopted from the MSDOS port)
 */

#define KEYPADLO    0x47
#define KEYPADHI    0x53

#define PADKEYS     (KEYPADHI - KEYPADLO + 1)
#define iskeypad(x)    (KEYPADLO <= (x) && (x) <= KEYPADHI)

/*
 * Wait until console input is available
 */

	static int
WaitForChar(int msec)
{
	if (GuiWin)
	{
		MSG				msg;
		int				settime = FALSE;
#ifdef FEPCTRL
		int				fepsync = FALSE;
#endif

		if (msec == 0)
			do_time = TRUE;
		else if (msec > 0)
		{
			do_time = FALSE;
			if (SetTimer(hVimWnd, KEY_TIME, msec, NULL) != 0)
				settime = TRUE;
		}
		else
			do_time = FALSE;
		for (;;)
		{
			if (do_resize)
			{
				if (settime)
					KillTimer(hVimWnd, KEY_TIME);
				return FALSE;
			}
			if (kbhit())
			{
				if (settime)
					KillTimer(hVimWnd, KEY_TIME);
				return TRUE;
			}
			if (do_time)
				break;
#ifdef FEPCTRL
			if (curbuf->b_p_fc && fep_get_mode())
			{
				if (fepsync == FALSE)
					fep_win_sync(hVimWnd);
				fepsync = TRUE;
			}
			else
				fepsync = FALSE;
#endif
			if (PeekMessageW(&msg, NULL, WM_KEYFIRST, WM_KEYLAST, PM_REMOVE))
			{
				if (!TranslateAcceleratorW(hVimWnd, hAcc, &msg))
				{
					TranslateMessage(&msg);
					DispatchMessageW(&msg);
				}
				continue;
			}
			if (GetMessageW(&msg, NULL, 0, 0))
			{
				if (!TranslateAcceleratorW(hVimWnd, hAcc, &msg))
				{
					TranslateMessage(&msg);
					DispatchMessageW(&msg);
				}
			}
		}
		if (settime)
			KillTimer(hVimWnd, KEY_TIME);
		return FALSE;
	}
	else
	{
		DWORD           count;
		int             ch;
		int             scan;
		int             retval = 0;
		DWORD			time = GetTickCount() + msec;

retry:
		if (WaitForSingleObject(hConIn, msec) == WAIT_OBJECT_0) {
			count = 0;
			(void)PeekConsoleInput(hConIn, &ir, 1, &count);
			if (count > 0) {
				ch = ir.Event.KeyEvent.uChar.AsciiChar;
				scan = ir.Event.KeyEvent.wVirtualScanCode;
#ifndef notdef
				if (!ch)
					ch = isctlkey();
#endif
				if (((ir.EventType == KEY_EVENT) && ir.Event.KeyEvent.bKeyDown) &&
					(ch || (iskeypad(scan)))) {
					retval = 1;     /* Found what we sought */
				}
			} else {                /* There are no events in console event queue */
				retval = 0;
			}
		}
		if (retval == 0
#ifdef FEPCTRL
					&& curbuf->b_p_fc && fep_get_mode() == 0
#endif
								&& time > GetTickCount()) {
			if (count)
				(void)ReadConsoleInput(hConIn, &ir, 1, &count);
			goto retry;
		}
		return retval;
	}
}

static int pending = 0;

	static int
tgetch(void)
{
	int             valid = 0;
	DWORD           count;
	unsigned short int scan;
	unsigned char   ch;

	if (pending)
	{
		ch = pending;
		pending = 0;
	}
	else
	{

		valid = 0;
		while (!valid)
		{
			(void)ReadConsoleInput(hConIn, &ir, 1, &count);
			if (ir.EventType == WINDOW_BUFFER_SIZE_EVENT)
			{
				set_winsize(Rows, Columns, FALSE);
			}
			else
			{
				if ((ir.EventType == KEY_EVENT) && ir.Event.KeyEvent.bKeyDown)
				{
					ch = ir.Event.KeyEvent.uChar.AsciiChar;
					scan = ir.Event.KeyEvent.wVirtualScanCode;
#ifndef notdef
					if (!ch)
						ch = isctlkey();
#endif
					if (ch || (iskeypad(scan)))
						valid = 1;
				}
			}
		}
		if (!ch)
		{
			pending = scan;
			ch = 0;
		}
	}
	return ch;
}


	static int
kbhit(void)
{
	if (GuiWin)
		return(c_next < c_end);
	else
	{
		int             done = 0;   /* true =  "stop searching"        */
		int             retval;     /* true =  "we had a match"        */
		DWORD           count;
		unsigned short int scan;
		unsigned char   ch;

		if (pending)
			return 1;

		done = 0;
		retval = 0;
		while (!done)
		{
			count = 0;
			PeekConsoleInput(hConIn, &ir, 1, &count);
			if (count > 0) {
				ch = ir.Event.KeyEvent.uChar.AsciiChar;
				scan = ir.Event.KeyEvent.wVirtualScanCode;
#ifndef notdef
				if (!ch)
					ch = isctlkey();
#endif
				if (((ir.EventType == KEY_EVENT) && ir.Event.KeyEvent.bKeyDown) &&
						(ch || (iskeypad(scan)) ))
				{
					done = 1;       /* Stop looking         */
					retval = 1;     /* Found what we sought */
				} else              /* Discard it, its an insignificant event */
					ReadConsoleInput(hConIn, &ir, 1, &count);
			}
			else                	/* There are no events in console event queue */
			{
				done = 1;           /* Stop looking               */
				retval = 0;
			}
		}
		return retval;
	}
}

	static int
getch(void)
{
	int				c;

	c = cbuf[c_next++];
	if (c_next == c_end)
		c_next = c_end = 0;
	if (GuiWin && c_ind > 0 && (c_next == c_ind || c_end == 0))
		c_ind = -1;
	return c;
}

/*
 * GetChars(): low level input funcion.
 * Get a characters from the keyboard.
 * If time == 0 do not wait for characters.
 * If time == n wait a short time for characters.
 * If time == -1 wait forever for characters.
 */
	int
GetChars(char_u *buf, int maxlen, int time)
{
	int             len = 0;
	int             c;
	static int		disp = 0;
	static int		oldState = (-1);

	if (GuiWin)
	{
		if (oldState != (State & NORMAL))
		{
			oldState = (State & NORMAL);
			SendMessage(hVimWnd, WM_INITMENU, 0, 0);
		}
		if (c_ind < 0)
		{
			c_ind = 0;
			curbuf->b_p_tw = w_p_tw;
			curbuf->b_p_wm = w_p_wm;
			curbuf->b_p_ai = w_p_ai;
			curbuf->b_p_si = w_p_si;
			curbuf->b_p_et = w_p_et;
			p_sm = w_p_sm;
			p_ru = w_p_ru;
			p_ri = w_p_ri;
			p_uc = w_p_uc;
			p_paste = w_p_paste;
			disp = 0;
		}
		else if (c_ind > (c_next + 32))
		{
			if (config_overflow < KEY_REDRAW)
			{
				RedrawingDisabled = TRUE;
				if ((disp % 41) == 0)
					RedrawingDisabled = FALSE;
				disp++;
			}
		}
		flushbuf();
		MoveCursor(hVimWnd);
		if (time >= 0)
		{
			while (WaitForChar(time) == 0)		/* no character available */
			{
				if (!do_resize)		/* return if not interrupted by resize */
					return 0;
				set_winsize(0, 0, FALSE);
				do_resize = FALSE;
				cursor_refresh(hVimWnd);
			}
		}
		else	/* time == -1 */
		{
		/*
		 * If there is no character available within 2 seconds (default)
		 * write the autoscript file to disk
		 */
			if (WaitForChar((int)p_ut) == 0)
			{
				updatescript(0);
			}
		}

	/*
	 * Try to read as many characters as there are.
	 * Works for the controlling tty only.
	 */
		--maxlen;		/* may get two chars at once */
		/*
		 * we will get at least one key. Get more if they are available
		 * After a ctrl-break we have to read a 0 (!) from the buffer.
		 * bioskey(1) will return 0 if no key is available and when a
		 * ctrl-break was typed. When ctrl-break is hit, this does not always
		 * implies a key hit.
		 */
		for (;;)	/* repeat until we got a character */
		{
			if (do_resize)		/* window changed size */
			{
				set_winsize(0, 0, FALSE);
				do_resize = FALSE;
				cursor_refresh(hVimWnd);
			}
			WaitForChar(-1);
			if (do_resize)
				continue;
			c = 0;
			while (kbhit() && len < maxlen)
			{
				switch (c = getch()) {
				case 0:
					*buf++ = K_NUL;
					break;
				default:
					*buf++ = c;
					break;
				}
				len++;
				if (c_ind < 0)
				{
					RedrawingDisabled = FALSE;
					break;
				}
			}
			if (c == K_NUL && WaitForChar((int)p_tm))
			{
				*buf++ = getch();
				len++;
			}
			if (len > 0)
				return len;
		}
	}
	else if (IsTelnet)
	{
		DWORD		count;

		flushbuf();
		if (time >= 0)
		{
			while (time > 0)
			{
				if (PeekNamedPipe(hConIn, NULL, 0, NULL, NULL, &count) == 0)
					return 0;
				if (count == 0)
				{
					Sleep(20);
					time -= 20;
				}
				else
					break;
			}
			if (count == 0)
				return 0;
		}
		else
		{
			time = p_ut;
			while (time > 0)
			{
				if (PeekNamedPipe(hConIn, NULL, 0, NULL, NULL, &count) == 0)
					return 0;
				if (count == 0)
				{
					Sleep(20);
					time -= 20;
				}
				else
					break;
			}
			if (count == 0)
			{
				updatescript(0);
				count = maxlen;
			}
		}
		if (count > (DWORD)maxlen)
			count = maxlen;
		if (ReadFile(hConIn, buf, count, &count, NULL) != 0 && count)
			return count;
		return 0;
	}
	else
	{
		if (time >= 0) {
			if (time == 0)          /* don't know if time == 0 is allowed */
				time = 1;
			if (WaitForChar(time) == 0)     /* no character available */
				return 0;
		} else {                    /* time == -1 */
			/* If there is no character available within 2 seconds (default)
			 * write the autoscript file to disk */
			if (WaitForChar((int) p_ut) == 0)
				updatescript(0);
		}
		if (!v_nt) {
			DWORD	count = 0;
			for (;;) {
#ifdef FEPCTRL
				if (curbuf->b_p_fc && fep_get_mode() != 0) {
						/* IME enable mode... try special IME handling */
					if (ReadConsole(hConIn, buf, maxlen, &count, NULL) && count)
						return count;
				}
#endif
				if (WaitForSingleObject(hConIn, INFINITE) != WAIT_OBJECT_0) {
					return 0;			/* no KEY data */
				}
				ir.Event.KeyEvent.uChar.AsciiChar = 0;
				if (!PeekConsoleInput(hConIn, &ir, 1, &count) || count == 0) {
					continue;
				}
				if (ir.EventType != KEY_EVENT) {	/* MOUSE/WINDOWS EVENT */
					(void)ReadConsoleInput(hConIn, &ir, 1, &count);
					continue;
				}
				if (ir.Event.KeyEvent.bKeyDown
					&& (ir.Event.KeyEvent.dwControlKeyState
						& (RIGHT_ALT_PRESSED|LEFT_ALT_PRESSED|ENHANCED_KEY)) == 0
					&& ir.Event.KeyEvent.uChar.AsciiChar != 0) {
										/* but Bata Release : ZEN/HAN key is '@' */
					if (ReadConsole(hConIn, buf, maxlen, &count, NULL) && count) {
						return count;
					}
					continue;
				}
				if (!ReadConsoleInput(hConIn, &ir, 1, &count) || count == 0) {
					continue;
				}
				if (ir.EventType == KEY_EVENT && ir.Event.KeyEvent.bKeyDown) {
					if ((buf[0] = isctlkey()) != 0) {
						return 1;
					} else if (iskeypad(ir.Event.KeyEvent.wVirtualScanCode)) {
						buf[0] = K_NUL;
						buf[1] = ir.Event.KeyEvent.wVirtualScanCode;
						return 2;
					} else if ((ir.Event.KeyEvent.wVirtualScanCode == '}')
								&& (ir.Event.KeyEvent.dwControlKeyState
									& (RIGHT_CTRL_PRESSED | LEFT_CTRL_PRESSED))) {
						ReadConsoleInput(hConIn, &ir, 1, &count);
						buf[0] = Ctrl('\\');
						return 1;
					}
				}
			}
		}

	/*
	 * Try to read as many characters as there are.
	 * Works for the controlling tty only.
	 */
		--maxlen;                   /* may get two chars at once */
		/* we will get at least one key. Get more if they are available After a
		 * ctrl-break we have to read a 0 (!) from the buffer. bioskey(1) will
		 * return 0 if no key is available and when a ctrl-break was typed. When
		 * ctrl-break is hit, this does not always implies a key hit. */
		cbrk_pressed = FALSE;
		while ((len == 0 || kbhit()) && len < maxlen) {
			switch (c = tgetch()) {
			case 0:
				*buf++ = K_NUL;
				break;
			case 3:
				cbrk_pressed = TRUE;
				/* FALLTHROUGH */
			default:
				*buf++ = c;
			}
			len++;
		}
		return len;
	}
}

/*
 * We have no job control, fake it by starting a new shell.
 */
	void
mch_suspend(void)
{
	outstr("new shell started\n");
	call_shell(NULL, 0, TRUE);
}

#if 0	/* ken */
extern int      _fmode;
#endif
char            OrigTitle[256];

/*
 *
 */
int APIENTRY
WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	char			**	argv;
	int					argc	= 1;
	int					num		= 4;
	char			*	p;
	int					c;
	BOOL				cygnus;
	static char			name[MAXPATHL];
	static char		*	dmy[2]	= {name, NULL};

	hInst = hInstance;
	SubSysCon = FALSE;
	GetModuleFileName(NULL, name, sizeof(name));
	if (lpCmdLine == NULL || (lpCmdLine != NULL && *lpCmdLine == '\0')
				|| (argv = malloc(num * sizeof(char *))) == NULL
				|| (p = malloc(strlen(lpCmdLine) + 1)) == NULL)
		argv = dmy;
	else
	{
		argv[0] = name;
		strcpy(p, lpCmdLine);
		while (*p != '\0')
		{
			if (argc >= num)
			{
				num += 4;
				if ((argv = realloc(argv, num * sizeof(char *))) == NULL)
					goto end;
			}
			if (*p == '"')
			{
				argv[argc++] = p + 1;
				while (*++p != '"')
					if (*p == '\0')
						goto end;
			}
			else if (getperm(p) != (-1))
			{
				argv[argc++] = p;
				break;
			}
			else
			{
				argv[argc++] = p;
				c = *p;
				cygnus = FALSE;
				if (p[0] == '/' && p[1] == '/' && isalpha(p[2]) && p[3] == '/')
					cygnus = TRUE;
				else if (strnicmp("/cygdrive/", p, 10) == 0
											&& isalpha(p[10]) && p[11] == '/')
					cygnus = TRUE;
				while (c != ' ' && c != '\t')
				{
#ifdef KANJI
					if (ISkanji(c))
						++p;
#endif
					c = *++p;
					if (cygnus && c == '\\')
					{
						memmove(&p[0], &p[1], strlen(p));
						if (*p == ' ' || *p == '\t')
							;
						else
							c = *p;
					}
					if (c == '\0')
						goto end;
				}
			}
			*p = '\0';
			c = *++p;
			while (c == ' ' || c == '\t')
				c = *++p;
		}
end:
		;
	}
	/*
	 * No SEH wrapper here on purpose: it used to be
	 *   __try { main(); } __except (EXCEPTION_CONTINUE_EXECUTION) { ; }
	 * which swallowed access violations and resumed at the faulting
	 * instruction, so crashes turned into silent corruption. main() installs
	 * w32crash_init() instead, which reports and then dies.
	 */
	main(argc, argv);
	return(0);
}

/*
 *
 */
	void
mch_windinit(int argc, char **argv, char *command)
{
	CONSOLE_SCREEN_BUFFER_INFO	csbi;

	_fmode = O_BINARY;          /* we do our own CR-LF translation */
	flushbuf();

	v_nt = FALSE;

	ver_info.dwOSVersionInfoSize = sizeof(ver_info);
	if (!GetVersionEx(&ver_info))
	{
		FatalAppExit(0, "Win32 API error.");
		ExitProcess(99);
	}
	if (ver_info.dwPlatformId == VER_PLATFORM_WIN32s)
	{
		FatalAppExit(0, "Win32s is not support");
		ExitProcess(99);
	}
	if (ver_info.dwPlatformId == VER_PLATFORM_WIN32_NT)
		v_nt = TRUE;
	if (GuiWin)
	{
		HMENU		hMenu;
		HMENU		hTColor;
		HMENU		hBColor;
		HMENU		hEdit;
		HMENU		hSetup;
		HMENU		hGSetup;
		HMENU		hFile;
		HMENU		hConf;
		WNDCLASSW	wndclass;
		UINT		w;
		HANDLE		hLibrary;

		if (SubSysCon)
			hInst = GetModuleHandle(NULL);
		v_region = Rows = nowRows;
		Columns = nowCols;
		if (SubSysCon)
			FreeConsole();

		w = SetErrorMode(SEM_NOOPENFILEERRORBOX);
		hLibrary = LoadLibrary("comctl32.dll");
		if (hLibrary != NULL)
		{
			pCreateUpDownControl
					= (tCreateUpDownControl)GetProcAddress(hLibrary, "CreateUpDownControl");
		}
		hLibrary = LoadLibrary("user32.dll");
		if (hLibrary != NULL)
		{
			pSetLayeredWindowAttributes
					= (tSetLayeredWindowAttributes)GetProcAddress(hLibrary, "SetLayeredWindowAttributes");
			pAllowSetForegroundWindow
					= (tAllowSetForegroundWindow)GetProcAddress(hLibrary, "AllowSetForegroundWindow");
			pLockSetForegroundWindow
					= (tLockSetForegroundWindow)GetProcAddress(hLibrary, "LockSetForegroundWindow");
			pGetDpiForWindow
					= (tGetDpiForWindow)GetProcAddress(hLibrary, "GetDpiForWindow");
			pGetDpiForSystem
					= (tGetDpiForSystem)GetProcAddress(hLibrary, "GetDpiForSystem");
		}
		SetErrorMode(w);

		v_cursor				= FALSE;
		v_font					= NULL;

		v_ssize = 256;
		if ((v_space = malloc(sizeof(INT) * v_ssize * 2)) == NULL)
			ExitProcess(99);
		if ((v_char = malloc(sizeof(short) * v_ssize * 2)) == NULL)
			ExitProcess(99);
		wndclass.style			= CS_DBLCLKS;
		wndclass.lpfnWndProc	= WndProc;
		wndclass.cbClsExtra		= 0;
		wndclass.cbWndExtra		= sizeof(LONG);
		wndclass.hInstance		= hInst;
		wndclass.hIcon			= LoadIcon(NULL, IDI_WINLOGO);
		wndclass.hCursor		= NULL; /* LoadCursor(NULL, IDC_IBEAM); */
		wndclass.hbrBackground	= NULL;	/* GetStockObject(WHITE_BRUSH); */
		wndclass.lpszMenuName	= NULL;
		wndclass.lpszClassName	= szAppNameW;
		/*
		 * Register as Unicode: an ANSI window only ever gets WM_CHAR in the
		 * ANSI code page, so anything outside CP932 arrived as '?'.
		 */
		if (RegisterClassW(&wndclass) == 0)
			ExitProcess(99);
		LoadConfig(TRUE);
		/*
		 * The stored sizes are for whatever DPI they were stored at. Restate
		 * them for this machine before the window is built out of them; the
		 * monitor the window lands on gets a second look below, once there is
		 * a window to ask about.
		 */
		dpi_scale_to(dpi_of(NULL));
		if (!config_ini && config_bitmap)
		{
			v_tbcolor = &config_tbbitmap;
			v_socolor = &config_sobitmap;
			v_ticolor = &config_tibitmap;
		}
		else
		{
			v_tbcolor = &config_tbcolor;
			v_socolor = &config_socolor;
			v_ticolor = &config_ticolor;
		}
		v_menu = LoadMenuW(hInst, L"VIMMENU");
		if (pSetLayeredWindowAttributes == NULL)
			DeleteMenu(v_menu, IDM_FADEOUT, MF_BYCOMMAND);
		hVimWnd = CreateWindowW(szAppNameW, szAppNameW,
				   WS_OVERLAPPEDWINDOW|(config_sbar ? WS_VSCROLL : 0),
				   config_x, config_y,
				   config_w, config_h, NULL, config_menu ? v_menu : NULL, hInst, NULL);
		if (NULL == hVimWnd)
			ExitProcess(99);
		hIbeamCurs	= LoadCursor(NULL, IDC_IBEAM);
		hArrowCurs	= LoadCursor(NULL, IDC_ARROW);
		hWaitCurs	= LoadCursor(NULL, IDC_WAIT);
		lpCurrCurs	= IDC_IBEAM;
		SetCursor(hIbeamCurs);
		hAcc = LoadAcceleratorsW(hInst, L"ACCKEYS");
		SetClassLongPtr(hVimWnd, GCLP_HICON, (LONG_PTR)LoadIcon(hInst, "vim"));
		hTColor= CreatePopupMenu();
		AppendMenu(hTColor,MF_STRING,    IDM_FWHITE,   "&White");
		AppendMenu(hTColor,MF_STRING,    IDM_FBLACK,   "&Black");
		AppendMenu(hTColor,MF_STRING,    IDM_FBLUE,    "&NavyBlue");
		AppendMenu(hTColor,MF_STRING,    IDM_FCOLOR,   "&Choice");
		hBColor= CreatePopupMenu();
		AppendMenu(hBColor,MF_STRING,    IDM_BWHITE,   "&White");
		AppendMenu(hBColor,MF_STRING,    IDM_BBLACK,   "&Black");
		AppendMenu(hBColor,MF_STRING,    IDM_BCOLOR,   "&Choice");
		hSetup= CreatePopupMenu();
		AppendMenu(hSetup, MF_STRING,    IDM_FONT,     "&Font");
		AppendMenu(hSetup, MF_STRING,    IDM_LSPACE,   "&Line Space");
		AppendMenu(hSetup, MF_POPUP,     (UINT_PTR)hTColor,"&Text Color");
		AppendMenu(hSetup, MF_POPUP,     (UINT_PTR)hBColor,"Back &Color");
		AppendMenu(hSetup, MF_STRING,    IDM_BITMAP,   "&Bitmap File");
		AppendMenu(hSetup, MF_STRING,    IDM_WAVE,     "&Wave File");
		AppendMenu(hSetup, MF_UNCHECKED, IDM_SAVE,     "&Save Window Position");
		hGSetup= CreatePopupMenu();
		AppendMenu(hGSetup,MF_CHECKED,   IDM_SBAR,     "&Scrollbar\tAlt+S");
		AppendMenu(hGSetup,MF_UNCHECKED, IDM_MENU,     "&Menu\tAlt+M");
		AppendMenu(hGSetup,MF_UNCHECKED, IDM_TRAY,     "&Task Tray");
		AppendMenu(hGSetup,MF_UNCHECKED, IDM_ONEWIN,   "One &Window");
		AppendMenu(hGSetup,MF_UNCHECKED, IDM_MOUSE,    "&Erase Mouse");
#ifdef NT106KEY
		AppendMenu(hGSetup,MF_UNCHECKED, IDM_NT106,    "&ZEN/HAN to ESC");
#endif
		if (pSetLayeredWindowAttributes)
			AppendMenu(hGSetup,MF_UNCHECKED, IDM_FADEOUT,  "Fade&out");
		AppendMenu(hGSetup,MF_UNCHECKED, IDM_GREPWIN,  "&Grep Window");
#ifdef USE_HISTORY
		{
			hHist = CreatePopupMenu();
			AppendMenu(hHist,  MF_UNCHECKED, IDM_HISTORY,  "&History");
			AppendMenu(hHist,  MF_UNCHECKED, IDM_HAUTO,    "&Auto History");
			AppendMenu(hGSetup,MF_POPUP,     (UINT_PTR)hHist,  "&History");
		}
#endif
		AppendMenu(hGSetup,MF_STRING,    IDM_PRINTSET, "&Print Command");
		hConf = CreatePopupMenu();
		AppendMenu(hConf,  MF_STRING,    IDM_CONFS,    "&Save Config");
		AppendMenu(hConf,  MF_UNCHECKED, IDM_CONF0,    "Default(&0)\tAlt+0");
		AppendMenu(hConf,  MF_UNCHECKED, IDM_CONF1,    "Config (&1)\tAlt+1");
		AppendMenu(hConf,  MF_UNCHECKED, IDM_CONF2,    "Config (&2)\tAlt+2");
		AppendMenu(hConf,  MF_UNCHECKED, IDM_CONF3,    "Config (&3)\tAlt+3");
		hEdit = CreatePopupMenu();
		AppendMenu(hEdit,  MF_STRING,    IDM_DELETE,   "&Delete\tAlt+X");
		AppendMenu(hEdit,  MF_STRING,    IDM_YANK,     "&Yank\tAlt+C");
		AppendMenu(hEdit,  MF_STRING,    IDM_PASTE,    "&Paste\tAlt+V");
		hFile = CreatePopupMenu();
		AppendMenu(hFile,  MF_STRING,    IDM_NEW,      "&New Window\tAlt+N");
		AppendMenu(hFile,  MF_STRING,    IDM_CLICK,    "&Run");
		AppendMenu(hFile,  MF_STRING,    IDM_FILE,     "&Open File\tAlt+O");
		AppendMenu(hFile,  MF_STRING,    IDM_SFILE,    "&Save File");
		AppendMenu(hFile,  MF_STRING,    IDM_AFILE,    "Save &As...");
		AppendMenu(hFile,  MF_STRING,    IDM_PRINT,    "&Print\tAlt+P");
		hMenu = GetSystemMenu(hVimWnd, FALSE);
		DeleteMenu(hMenu,  5, MF_BYPOSITION);
#if CUST_MENU
		DeleteMenu(hMenu, SC_SIZE,     MF_BYCOMMAND);
		DeleteMenu(hMenu, SC_MOVE,     MF_BYCOMMAND);
		DeleteMenu(hMenu, SC_MINIMIZE, MF_BYCOMMAND);
		DeleteMenu(hMenu, SC_MAXIMIZE, MF_BYCOMMAND);
		DeleteMenu(hMenu, SC_CLOSE,    MF_BYCOMMAND);
		DeleteMenu(hMenu, SC_RESTORE,  MF_BYCOMMAND);
		hWin = CreatePopupMenu();
		AppendMenu(hWin,  MF_STRING, SC_RESTORE,  "Restore Window(&R)");
		AppendMenu(hWin,  MF_STRING, SC_MOVE,     "Move Window(&M)");
		AppendMenu(hWin,  MF_STRING, SC_SIZE,     "Change Window Size(&S)");
		AppendMenu(hWin,  MF_STRING, SC_MINIMIZE, "Min Window(&N)");
		AppendMenu(hWin,  MF_STRING, SC_MAXIMIZE, "Max Window(&X)");
		AppendMenu(hWin,  MF_STRING, SC_CLOSE,    "Close Window(&C)");
		AppendMenu(hMenu, MF_POPUP,  (UINT_PTR)hWin,  "&Window");
#else
		{
			char		buf[128];
			char	*	p;
			UINT		item[] = {SC_RESTORE,SC_MINIMIZE,SC_MAXIMIZE,SC_CLOSE};
			int			i;

			for (i = 0; i < sizeof(item) / sizeof(UINT); i++)
			{
				GetMenuString(hMenu, item[i],  buf, sizeof(buf), MF_BYCOMMAND);
				if ((p = strchr(buf, '\t')) != NULL)
					*p = NUL;
				ModifyMenu(hMenu, item[i], MF_BYCOMMAND|MF_STRING, item[i], buf);
			}
		}
#endif
		AppendMenu(hMenu,  MF_SEPARATOR, 0, NULL);
		AppendMenu(hMenu,  MF_POPUP,     (UINT_PTR)hGSetup,"G&lobal Setup");
		AppendMenu(hMenu,  MF_POPUP,     (UINT_PTR)hSetup, "Set&up");
		AppendMenu(hMenu,  MF_POPUP,     (UINT_PTR)hConf,  "Confi&g");
		AppendMenu(hMenu,  MF_POPUP,     (UINT_PTR)hFile,  "&File");
		AppendMenu(hMenu,  MF_POPUP,     (UINT_PTR)hEdit,  "&Edit");
		SetMenuItemBitmaps(hMenu, IDM_BITMAP, MF_BYCOMMAND, NULL, NULL);
		SetMenuItemBitmaps(hMenu, IDM_WAVE, MF_BYCOMMAND, NULL, NULL);
		SetMenuItemBitmaps(hMenu, IDM_SAVE, MF_BYCOMMAND, NULL, NULL);
		SetMenuItemBitmaps(hMenu, IDM_MENU, MF_BYCOMMAND, NULL, NULL);
		SetMenuItemBitmaps(hMenu, IDM_TRAY, MF_BYCOMMAND, NULL, NULL);
		SetMenuItemBitmaps(hMenu, IDM_ONEWIN, MF_BYCOMMAND, NULL, NULL);
		SetMenuItemBitmaps(hMenu, IDM_MOUSE, MF_BYCOMMAND, NULL, NULL);
#ifdef NT106KEY
		SetMenuItemBitmaps(hMenu, IDM_NT106, MF_BYCOMMAND, NULL, NULL);
#endif
		if (pSetLayeredWindowAttributes)
			SetMenuItemBitmaps(hMenu, IDM_FADEOUT, MF_BYCOMMAND, NULL, NULL);
		SetMenuItemBitmaps(hMenu, IDM_GREPWIN, MF_BYCOMMAND, NULL, NULL);
#ifdef USE_HISTORY
		SetMenuItemBitmaps(hMenu, (UINT_PTR)hHist, MF_BYCOMMAND, NULL, NULL);
		SetMenuItemBitmaps(hMenu, IDM_HISTORY, MF_BYCOMMAND, NULL, NULL);
		SetMenuItemBitmaps(hMenu, IDM_HAUTO,   MF_BYCOMMAND, NULL, NULL);
#endif
		if (GuiWin == '1')
		{
			HWND			hWnd;
			COPYDATASTRUCT	cds;
			char			buf[MAXPATHL];
			int				size = 8;
			int				j;
			char		*	p;

			if (argc > 1)
			{
				GetCurrentDirectory(MAXPATHL, buf);
				for (j = 0; j < (argc - 1); j++)
					size += strlen(argv[j]) + 3;
				size += strlen(buf) + 1
							+ (command == NULL ? 0 : strlen(command) + 1);
			}
			cds.dwData = 0;
			cds.cbData = size;
			if ((cds.lpData = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, cds.cbData)) != NULL)
			{
				if (argc > 1)
				{
					p = cds.lpData;
					strcpy(p, buf);
					size = strlen(buf) + 1;
					if (argc == 2)
					{
						strcpy(&p[size], ":e ");
						if (command
								&& (command[0] == '$' || ('0' <= command[0] && command[0] <= '9')))
						{
							strcat(&p[size], "+");
							strcat(&p[size], command);
							strcat(&p[size], " ");
						}
						strcat(&p[size], argv[0]);
					}
					else
					{
						strcpy(&p[size], ":args");
						for (j = 0; j < (argc - 1); j++)
						{
							strcat(&p[size], " \"");
							strcat(&p[size], argv[j]);
							strcat(&p[size], "\"");
						}
					}
					cds.dwData = size;
				}
				do_msg	= TRUE;
				hWnd = GetFirstSibling(hVimWnd);
				while (IsWindow(hWnd))
				{
					GetClassName(hWnd, buf, sizeof(buf));
					if (strcmp(buf, szAppName) == 0 && hVimWnd != hWnd)
					{
						if (SendMessage(hWnd, WM_COPYDATA,
									(WPARAM)hVimWnd, (LPARAM)&cds) == TRUE)
							ExitProcess(0);
					}
					hWnd = GetNextSibling(hWnd);
				}
				HeapFree(GetProcessHeap(), 0, cds.lpData);
				do_msg	= FALSE;
			}
		}
		/*
		 * With per monitor awareness the window can have been created on a
		 * monitor that is not at the system DPI, and WM_DPICHANGED does not
		 * arrive for that -- only for a later move. Correct the font now,
		 * while the window is still hidden; the mch_set_winsize() further
		 * down puts the window back around Rows by Columns of it.
		 */
		if (dpi_of(hVimWnd) != config_dpi)
		{
			dpi_scale_to(dpi_of(hVimWnd));
			ResetScreen(hVimWnd);
		}
		ShowWindow(hVimWnd, SW_SHOWDEFAULT);
		UpdateWindow(hVimWnd);
		TopWindow(hVimWnd);
		GetWindowText(hVimWnd, OrigTitle, sizeof(OrigTitle));

#ifndef NO_WHEEL
		/* NT 4 and later supports WM_MOUSEWHEEL */
		/* Future Win32 versions ( >= 5.0 ) should support WM_MOUSEWHEEL */
		if ((ver_info.dwMajorVersion >= 5)
					|| (VER_PLATFORM_WIN32_NT == ver_info.dwPlatformId
											&& ver_info.dwMajorVersion >= 4))
		{
			if (!SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0,
											&iScrollLines, SPIF_UPDATEINIFILE))
				iScrollLines = 3;
		}
		else
		{
			HwndMSWheel(&uiMsh_MsgMouseWheel, &uiMsh_Msg3DSupport,
						  &uiMsh_MsgScrollLines, &f3DSupport, &iScrollLines);
			if (iScrollLines == 0)
				iScrollLines = 3;
		}
#endif

		mch_set_winsize();
		do_resize = FALSE;

		SetLayerd();
	}
	else if (IsTelnet)
	{
		BenchTime = 0;
		hConIn = GetStdHandle(STD_INPUT_HANDLE);
		hConOut = GetStdHandle(STD_OUTPUT_HANDLE);
	}
	else
	{
		BenchTime = 0;
		/* Obtain handles for the standard Console I/O devices */
		hConIn = CreateFile("CONIN$",
						GENERIC_READ | GENERIC_WRITE,
						FILE_SHARE_READ | FILE_SHARE_WRITE,
						NULL, OPEN_EXISTING, 0, NULL);

		hConOut = CreateFile("CONOUT$",
						 GENERIC_READ | GENERIC_WRITE,
						 FILE_SHARE_READ | FILE_SHARE_WRITE,
						 NULL, OPEN_EXISTING, 0, NULL);
#ifndef notdef
		if (!DuplicateHandle(GetCurrentProcess(), GetCurrentThread(),
							GetCurrentProcess(), &h_mainthread,
						0, FALSE, DUPLICATE_SAME_ACCESS))
		{
			FatalAppExit(0, "initialize error\n");
			ExitProcess(99);
		}
#endif
		GetConsoleScreenBufferInfo(hConOut, &csbi);
		DefaultAttribute = csbi.wAttributes;
#ifndef notdef
		if (!v_nt)
			DefaultAttribute = csbi.wAttributes | FOREGROUND_INTENSITY;
#endif
		mch_get_winsize();
		GetConsoleTitle(OrigTitle, sizeof(OrigTitle));
	}
	if (vimgetenv("HOME") == NULL)
	{
		char	home[MAXPATHL+8];
		BOOL	bEnv = FALSE;

		if (v_nt && vimgetenv("HOMEDRIVE") != NULL
											&& vimgetenv("HOMEPATH") != NULL)
		{
			bEnv = TRUE;
			if (vimgetenv("HOMESHARE") != NULL)
				;
			else if (strcmp(vimgetenv("HOMEPATH"), "\\") == 0)
			{
				if (vimgetenv("SystemDrive") != NULL)
				{
					if (stricmp(vimgetenv("HOMEDRIVE"), vimgetenv("SystemDrive")) == 0)
						bEnv = FALSE;
				}
			}
		}
		strcpy(home, "HOME=");
		if (bEnv)
		{
			strcat(home, vimgetenv("HOMEDRIVE"));
			strcat(home, vimgetenv("HOMEPATH"));
		}
		else
		{
			char *	p;
			char *	last;

			GetModuleFileName(NULL, &home[5], MAXPATHL);
			last = p = home + 8;	/* drive + : + \ */
			while (*p)
			{
				if (*p == '\\')
					last = p;
				p++;
			}
			*last = '\0';
		}
		putenv(home);
	}
	if (vimgetenv("VIM") == NULL)
	{
		char	vim[MAXPATHL+8];
		char *	p;
		char *	last;

		strcpy(vim, "VIM=");
		GetModuleFileName(NULL, &vim[4], MAXPATHL);
		last = p = vim + 7;	/* drive + : + \ */
		while (*p)
		{
			if (*p == '\\')
				last = p;
			p++;
		}
		*last = '\0';
		putenv(vim);
	}
	if (vimgetenv("TMP") == NULL)
	{
		char	tmp[MAXPATHL+4];
		strcpy(tmp, "TMP=");
		if (vimgetenv("TEMP") != NULL)
			strcat(tmp, vimgetenv("TEMP"));
		else if (GetTempPath(MAXPATHL, &tmp[4]) != 0)
			;
		else
			GetCurrentDirectory(MAXPATHL, &tmp[4]);
		if (tmp[strlen(tmp) - 1] == '\\')
			tmp[strlen(tmp) - 1] = '\0';
		putenv(tmp);
	}
	if (BenchTime)
	{
		putenv("VIMINIT=");	putenv("EXINIT=");	putenv("HOME=_");
		putenv("TEMP=.");	putenv("TMP=.");
		BenchTime = GetTickCount();
	}
}

	void
check_win(int argc, char **argv)
{
	if (!isatty(0) || !isatty(1))
	{
#ifdef notdef		/* Windows NT telnetd support */
		fprintf(stderr, "VIM: no controlling terminal\n");
		exit(2);
#else
		IsTelnet = TRUE;
#endif
	}
	/* In some cases with DOS 6.0 on a NEC notebook there is a 12 seconds
	 * delay when starting up that can be avoided by the next two lines.
	 * Don't ask me why! This could be fixed by removing setver.sys from
	 * config.sys. Forget it. gotoxy(1,1); cputs(" "); */
}

/*
 * fname_case(): Set the case of the filename, if it already exists.
 *                 msdos filesystem is far to primitive for that. do nothing.
 */
	void
fname_case(char_u *name)
{
#ifndef notdef
	HANDLE          hFind;
	char_u		*	tname;
	char_u			buf[MAXPATHL];
	char_u			found[FIND_NAMELEN];

	if (GetFullPathName(name, sizeof(buf), buf, (LPSTR *)&tname) == 0)
		return;
	if ((hFind = find_first_name(buf, found, sizeof(found), NULL))
													!= INVALID_HANDLE_VALUE)
	{
		/* only the case may change, so the name has to be the same length --
		 * that also keeps a name too long for 'buf' from being cut short here */
		if (strlen(name) == strlen(buf) && strlen(found) == strlen(tname))
		{
			strcpy(tname, found);
			strcpy(name, buf);
		}
		FindClose(hFind);
	}
#endif
}


/*
 * Set the window title from UTF-8, so a file name with characters outside the
 * ANSI code page shows up as itself rather than as question marks.
 */
	static WCHAR *
utf8_to_wide(char_u *text)
{
	WCHAR	*	w;
	int			wlen;

	if (text == NULL)
		return NULL;
	wlen = MultiByteToWideChar(CP_UTF8, 0, (LPCSTR)text, -1, NULL, 0);
	if (wlen <= 0)
		return NULL;
	if ((w = (WCHAR *)alloc((unsigned)(wlen * sizeof(WCHAR)))) == NULL)
		return NULL;
	MultiByteToWideChar(CP_UTF8, 0, (LPCSTR)text, -1, w, wlen);
	return w;
}

/*
 * The GUI keeps its strings in UTF-8. Everything below goes through UTF-16 so
 * that none of it depends on what the process ANSI code page happens to be:
 * dialogs, menus, fonts and the registry all speak Unicode natively anyway.
 */
	static char_u *
wide_to_utf8(WCHAR *w)
{
	char_u	*	text;
	int			len;

	if (w == NULL)
		return NULL;
	len = WideCharToMultiByte(CP_UTF8, 0, w, -1, NULL, 0, NULL, NULL);
	if (len <= 0 || (text = alloc((unsigned)len)) == NULL)
		return NULL;
	WideCharToMultiByte(CP_UTF8, 0, w, -1, (LPSTR)text, len, NULL, NULL);
	return text;
}

/*
 * Reading a directory: find_first_name() and find_next_name() hand back one
 * entry's name in UTF-8, and its attributes in 'attr' when that is not NULL.
 *
 * Not FindFirstFileA/FindNextFileA. Those fill a WIN32_FIND_DATAA, whose
 * cAlternateFileName is 14 bytes, and with the process code page set to UTF-8
 * (see jvim.manifest) the 8.3 name of a file with Japanese in it no longer fits
 * there -- "1_ああ~1.MP3" is 14 bytes before the NUL. The call then returns
 * FALSE with ERROR_MORE_DATA, and every caller of a find loop reads FALSE as
 * "no more files": a ":e" completion stopped at the first such name and nothing
 * after it was ever listed. A name whose UTF-8 form does not fit cFileName's
 * 260 bytes comes back truncated as well, which is worse than stopping.
 *
 * The wide API has neither limit, so the scan goes through it and the name is
 * converted here.
 */
	static int
find_name_out(WIN32_FIND_DATAW *fw, char_u *name, int namelen, DWORD *attr)
{
	if (WideCharToMultiByte(CP_UTF8, 0, fw->cFileName, -1, (LPSTR)name,
											namelen, NULL, NULL) <= 0)
		return FALSE;			/* no room for this one: skip it */
	if (attr != NULL)
		*attr = fw->dwFileAttributes;
	return TRUE;
}

	int
find_next_name(HANDLE hFind, char_u *name, int namelen, DWORD *attr)
{
	WIN32_FIND_DATAW	fw;

	while (FindNextFileW(hFind, &fw))
		if (find_name_out(&fw, name, namelen, attr))
			return TRUE;
	return FALSE;
}

	HANDLE
find_first_name(char_u *pat, char_u *name, int namelen, DWORD *attr)
{
	WIN32_FIND_DATAW	fw;
	HANDLE				hFind;
	WCHAR			*	wpat;

	if ((wpat = utf8_to_wide(pat)) == NULL)
		return INVALID_HANDLE_VALUE;
	hFind = FindFirstFileW(wpat, &fw);
	free(wpat);
	if (hFind == INVALID_HANDLE_VALUE)
		return hFind;
	if (find_name_out(&fw, name, namelen, attr))
		return hFind;
	if (find_next_name(hFind, name, namelen, attr))	/* first one did not fit */
		return hFind;
	FindClose(hFind);
	return INVALID_HANDLE_VALUE;
}

/*
 * The font in the registry. Two values: the LOGFONTW this build uses, and a
 * LOGFONTA beside it so that an older build still finds a font it understands.
 */
	static int
font_load(HKEY hKey, char *wname, char *aname, LOGFONTW *lf)
{
	DWORD		size;
	DWORD		type;
	LOGFONTA	narrow;

	size = sizeof(*lf);
	type = REG_BINARY;
	if (RegQueryValueEx(hKey, wname, NULL, &type, (BYTE *)lf, &size)
										== ERROR_SUCCESS && size == sizeof(*lf))
		return TRUE;
	size = sizeof(narrow);
	type = REG_BINARY;
	if (RegQueryValueEx(hKey, aname, NULL, &type, (BYTE *)&narrow, &size)
														!= ERROR_SUCCESS)
		return FALSE;
	font_widen(lf, &narrow);
	return TRUE;
}

	static int
font_save(HKEY hKey, char *wname, char *aname, LOGFONTW *lf)
{
	LOGFONTA	narrow;

	if (RegSetValueEx(hKey, wname, 0, REG_BINARY, (BYTE *)lf, sizeof(*lf))
														!= ERROR_SUCCESS)
		return FALSE;
	font_narrow(&narrow, lf);
	return (RegSetValueEx(hKey, aname, 0, REG_BINARY, (BYTE *)&narrow,
								sizeof(narrow)) == ERROR_SUCCESS);
}

/*
 * A LOGFONTA from an older config, widened. Everything but the face name copies
 * across; the name may have been written in the code page or, by the builds in
 * between, in UTF-8, so try UTF-8 first and fall back to the code page.
 */
	static void
font_widen(LOGFONTW *w, LOGFONTA *a)
{
	memset(w, 0, sizeof(*w));
	w->lfHeight			= a->lfHeight;
	w->lfWidth			= a->lfWidth;
	w->lfEscapement		= a->lfEscapement;
	w->lfOrientation	= a->lfOrientation;
	w->lfWeight			= a->lfWeight;
	w->lfItalic			= a->lfItalic;
	w->lfUnderline		= a->lfUnderline;
	w->lfStrikeOut		= a->lfStrikeOut;
	w->lfCharSet		= a->lfCharSet;
	w->lfOutPrecision	= a->lfOutPrecision;
	w->lfClipPrecision	= a->lfClipPrecision;
	w->lfQuality		= a->lfQuality;
	w->lfPitchAndFamily	= a->lfPitchAndFamily;
	if (a->lfFaceName[0] == NUL)
		return;
	if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, a->lfFaceName, -1,
										w->lfFaceName, LF_FACESIZE) > 0)
		return;
	if (MultiByteToWideChar(p_cpage, 0, a->lfFaceName, -1,
										w->lfFaceName, LF_FACESIZE) <= 0)
		w->lfFaceName[0] = L'\0';
}

/*
 * The other way, for the two places that still want a LOGFONTA: the copy of the
 * font kept in the registry for older builds to read, and the IME conversion
 * font. The face name goes out in the code page and can be cut short there --
 * which is the limit those two have always had.
 */
	static void
font_narrow(LOGFONTA *a, LOGFONTW *w)
{
	memset(a, 0, sizeof(*a));
	a->lfHeight			= w->lfHeight;
	a->lfWidth			= w->lfWidth;
	a->lfEscapement		= w->lfEscapement;
	a->lfOrientation	= w->lfOrientation;
	a->lfWeight			= w->lfWeight;
	a->lfItalic			= w->lfItalic;
	a->lfUnderline		= w->lfUnderline;
	a->lfStrikeOut		= w->lfStrikeOut;
	a->lfCharSet		= w->lfCharSet;
	a->lfOutPrecision	= w->lfOutPrecision;
	a->lfClipPrecision	= w->lfClipPrecision;
	a->lfQuality		= w->lfQuality;
	a->lfPitchAndFamily	= w->lfPitchAndFamily;
	WideCharToMultiByte(p_cpage, 0, w->lfFaceName, -1, a->lfFaceName,
										LF_FACESIZE, NULL, NULL);
	a->lfFaceName[LF_FACESIZE - 1] = NUL;
}

	static void
SetDlgItemTextU8(HWND hWnd, int id, char_u *text)
{
	WCHAR	*	w;

	if ((w = utf8_to_wide(text)) == NULL)
	{
		SetDlgItemText(hWnd, id, text == NULL ? "" : (LPCSTR)text);
		return;
	}
	SetDlgItemTextW(hWnd, id, w);
	free(w);
}

/*
 * Use the system UI font (NONCLIENTMETRICS.lfMessageFont) in a dialog.
 * Applied at WM_INITDIALOG so dialogs keep their resource layouts but
 * render with the same font as other Windows message boxes.
 */
	static void
SetDialogSystemFont(HWND hWnd)
{
	NONCLIENTMETRICSW	ncm;
	HWND				hCtrl;

	if (hSystemUIFont == NULL)
	{
		ncm.cbSize = sizeof(ncm);
		if (SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, ncm.cbSize, &ncm, 0))
			hSystemUIFont = CreateFontIndirectW(&ncm.lfMessageFont);
	}
	if (hSystemUIFont == NULL)
		return;
	for (hCtrl = GetWindow(hWnd, GW_CHILD); hCtrl != NULL;
									hCtrl = GetWindow(hCtrl, GW_HWNDNEXT))
		SendMessage(hCtrl, WM_SETFONT, (WPARAM)hSystemUIFont, TRUE);
}

	static int
GetDlgItemTextU8(HWND hWnd, int id, char_u *buf, int len)
{
	WCHAR		w[MAXPATHL];
	char_u	*	text;
	int			n = 0;

	if (len <= 0)
		return 0;
	buf[0] = NUL;
	if (GetDlgItemTextW(hWnd, id, w, (int)(sizeof(w) / sizeof(w[0]))) <= 0)
		return 0;
	if ((text = wide_to_utf8(w)) == NULL)
		return 0;
	lstrcpynA((LPSTR)buf, (LPCSTR)text, len);
	n = (int)strlen((char *)buf);
	free(text);
	return n;
}

	static BOOL
AppendMenuU8(HMENU hMenu, UINT flags, UINT_PTR id, char_u *text)
{
	WCHAR	*	w;
	BOOL		r;

	if ((w = utf8_to_wide(text)) == NULL)
		return AppendMenu(hMenu, flags, id, (LPCSTR)text);
	r = AppendMenuW(hMenu, flags, id, w);
	free(w);
	return r;
}

	static BOOL
ModifyMenuU8(HMENU hMenu, UINT pos, UINT flags, UINT_PTR id, char_u *text)
{
	WCHAR	*	w;
	BOOL		r;

	if ((w = utf8_to_wide(text)) == NULL)
		return ModifyMenu(hMenu, pos, flags, id, (LPCSTR)text);
	r = ModifyMenuW(hMenu, pos, flags, id, w);
	free(w);
	return r;
}


/*
 * Show the font chooser and put what the user picked back into 'lf'. ChooseFontW
 * and a LOGFONTW throughout, so the face name is never converted and never has
 * to fit anything narrower than the 32 characters Windows allows.
 */
	static BOOL
ChooseFontJ(HWND hWnd, LOGFONTW *lf)
{
	CHOOSEFONTW	cf;
	LOGFONTW	w;

	w = *lf;
	memset(&cf, 0, sizeof(cf));
	cf.lStructSize	= sizeof(cf);
	cf.hwndOwner	= hWnd;
	cf.hDC			= NULL;
	cf.rgbColors	= *v_fgcolor;
	cf.lpLogFont	= &w;
	/*
	 * CF_FIXEDPITCHONLY: JVim draws on a character grid, so a proportional font
	 * cannot work and should not be offered. CF_ANSIONLY is deliberately absent:
	 * it hid every font whose charset is not the ANSI one, i.e. most Japanese
	 * ones.
	 */
	cf.Flags		= CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT
							| CF_NOVERTFONTS | CF_FIXEDPITCHONLY;
	cf.hInstance	= hInst;
	if (!ChooseFontW(&cf))
		return FALSE;

	*lf = w;
	return TRUE;
}

/*
 * Read a REG_SZ value as UTF-8, or write one from UTF-8. The registry itself is
 * Unicode, so this is the only form that cannot lose anything.
 */
	static BOOL
RegGetStringU8(HKEY hKey, char *name, char_u *buf, int len)
{
	WCHAR		w[MAXPATHL];
	WCHAR	*	wname;
	char_u	*	text;
	DWORD		size = sizeof(w);
	DWORD		type = REG_SZ;
	LONG		rc;

	if ((wname = utf8_to_wide((char_u *)name)) == NULL)
		return FALSE;
	rc = RegQueryValueExW(hKey, wname, NULL, &type, (BYTE *)w, &size);
	free(wname);
	if (rc != ERROR_SUCCESS)
		return FALSE;
	w[sizeof(w) / sizeof(w[0]) - 1] = 0;
	if ((text = wide_to_utf8(w)) == NULL)
		return FALSE;
	lstrcpynA((LPSTR)buf, (LPCSTR)text, len);
	free(text);
	return TRUE;
}

	static LONG
RegSetStringU8(HKEY hKey, char *name, char_u *text)
{
	WCHAR	*	wname;
	WCHAR	*	w;
	LONG		rc;

	if ((w = utf8_to_wide(text)) == NULL)
		return ERROR_INVALID_DATA;
	wname = utf8_to_wide((char_u *)name);
	rc = RegSetValueExW(hKey, wname, 0, REG_SZ, (BYTE *)w,
						(DWORD)((lstrlenW(w) + 1) * sizeof(WCHAR)));
	free(w);
	if (wname != NULL)
		free(wname);
	return rc;
}

	static void
SetWindowTextU8(HWND hWnd, char_u *text)
{
	WCHAR	*	w;

	if ((w = utf8_to_wide(text)) == NULL)
	{
		if (text != NULL)
			SetWindowText(hWnd, (LPCSTR)text);	/* try the ANSI form */
		return;
	}
	SetWindowTextW(hWnd, w);
	free(w);
}

	static void
SetConsoleTitleU8(char_u *text)
{
	WCHAR	*	w;

	if ((w = utf8_to_wide(text)) == NULL)
	{
		if (text != NULL)
			SetConsoleTitle((LPCSTR)text);
		return;
	}
	SetConsoleTitleW(w);
	free(w);
}

/*
 * mch_settitle(): set titlebar of our window
 * Can the icon also be set?
 */
	void
mch_settitle(char_u *title, char_u *icon)
{
	if (title != NULL && !p_icon)
	{
		if (GuiWin)
		{
			if (icon != NULL && strlen(title) > (TITLE_LEN + 6))
				SetWindowTextU8(hVimWnd, icon);
			else
				SetWindowTextU8(hVimWnd, DisplayPathName(title, (TITLE_LEN + 6) > sizeof(nIcon.szTip) ? sizeof(nIcon.szTip) : TITLE_LEN + 6));
		}
		else if (IsTelnet)
			;
		else
		{
			if (icon != NULL && strlen(title) > sizeof(nIcon.szTip))
				SetConsoleTitleU8(icon);
			else
				SetConsoleTitleU8(title);
		}
	}
	else if (icon != NULL)
	{
		if (GuiWin)
			SetWindowTextU8(hVimWnd, icon);
		else if (IsTelnet)
			;
		else
			SetConsoleTitleU8(icon);
	}
}

/*
 * Restore the window/icon title.
 * which is one of:
 *    1  Just restore title
 *  2  Just restore icon (which we don't have)
 *    3  Restore title and icon (which we don't have)
 */
	void
mch_restore_title(int which)
{
	mch_settitle((which & 1) ? OrigTitle : NULL, NULL);
}

/*
 * Get name of current directory into buffer 'buf' of length 'len' bytes.
 * Return non-zero for success.
 */
	int
vim_dirname(char_u *buf, int len)
{
#ifdef __BORLANDC__
	return (getcwd(buf, len) != NULL ? OK : FAIL);
#else
	return (_getcwd(buf, len) != NULL ? OK : FAIL);
#endif
}

/*
 * get absolute filename into buffer 'buf' of length 'len' bytes
 */
	int
FullName(char_u *fname, char_u *buf, int len)
{
	WCHAR	*	wname;
	WCHAR	*	wfull = NULL;
	DWORD		need;
	int			ok = FALSE;

	if (fname == NULL)          /* always fail */
		return FAIL;

	/*
	 * GetFullPathNameW, not _fullpath(): the ANSI call behind _fullpath() stops
	 * at 260 *characters* and returns ERROR_FILENAME_EXCED_RANGE, whatever
	 * longPathAware in the manifest says -- while open() and CreateFile() honour
	 * it and will happily use a longer name. So this is the one place that has
	 * to go through the wide call to get the long path the rest of the process
	 * can then act on.
	 *
	 * A result that does not fit 'buf' is a failure, not something to truncate:
	 * a shortened path names a different file, or none.
	 */
	if ((wname = utf8_to_wide(fname)) != NULL)
	{
		need = GetFullPathNameW(wname, 0, NULL, NULL);
		if (need != 0
				&& (wfull = (WCHAR *)alloc((unsigned)(need * sizeof(WCHAR))))
																	!= NULL
				&& GetFullPathNameW(wname, need, wfull, NULL) != 0)
		{
			if (WideCharToMultiByte(CP_UTF8, 0, wfull, -1, (LPSTR)buf, len,
													NULL, NULL) > 0)
				ok = TRUE;
		}
		free(wfull);
		free(wname);
	}
	if (ok)
		return OK;
	strncpy(buf, fname, len);       /* failed, use the relative path name */
	buf[len - 1] = NUL;             /* strncpy() does not when it fills up */
	return FAIL;
}

/*
 * return TRUE is fname is an absolute path name
 */
	int
isFullName(char_u *fname)
{
#ifdef notdef
	return (STRCHR(fname, ':') != NULL);
#else
	if (strlen(fname) > 3 && isalpha(fname[0]) && fname[1] == ':' && fname[2] == '\\')
		return(TRUE);
	if (strlen(fname) >= 2 && fname[0] == '\\' && fname[1] == '\\')
		return(TRUE);
	return(FALSE);
#endif
}

/*
 * get file permissions for 'name'
 * -1 : error
 * else FA_attributes defined in dos.h
 */
	long
getperm(char_u *name)
{
	struct stat statb;
	long        r;

	if (stat(name, &statb))
		return -1;
	r = statb.st_mode & 0x7fffffff;
	return r;
}

/*
 * set file permission for 'name' to 'perm'
 */
	int
setperm(char_u *name, long perm)
{
	return chmod(name, perm);
}

/*
 * check if "name" is a directory
 */
int             isdir(char_u *name)
{
	int f;

	f = getperm(name);
	if (f == -1)
		return -1;                    /* file does not exist at all */
	if ((f & S_IFDIR) == 0)
		return FAIL;                /* not a directory */
	return OK;
}

/*
 * Careful: mch_windexit() may be called before mch_windinit()!
 */
	void
mch_windexit(int r)
{
	if (GuiWin && NameBuff != NULL)
	{
		if (GuiConfig == 0)
			SaveConfig();
	}
	else
		GuiWin = FALSE;
	if (GuiWin && !BenchTime)
	{
		if (config_fadeout && (bWClose || v_trans) && pSetLayeredWindowAttributes != NULL)
		{
			BYTE			gbAlpha = 0xff;
			DWORD			dwTime  = 30;

			gbAlpha = (BYTE)(((230 * (100 - v_trans)) / 100) + 25);
			if (gbAlpha > 180)
				gbAlpha = 180;
			else if (gbAlpha > 50)
				gbAlpha -= 20;
			SetWindowLong(hVimWnd, GWL_EXSTYLE,
						GetWindowLong(hVimWnd, GWL_EXSTYLE) | WS_EX_LAYERED);
			pSetLayeredWindowAttributes(hVimWnd, 0, gbAlpha, LWA_ALPHA);
			UpdateWindow(hVimWnd);
			while (gbAlpha > 15)
			{
				pSetLayeredWindowAttributes(hVimWnd, 0, gbAlpha, LWA_ALPHA);
				gbAlpha -= 12;
				if (gbAlpha > 200)
					dwTime = 10;
				else if (gbAlpha > 100)
					dwTime = 15;
				else if (gbAlpha > 10)
					dwTime = 20;
				Sleep(dwTime);
			}
		}
		else
			delay(100);
	}
#ifdef FEPCTRL
	if (FepInit)
		fep_term();
#endif
	settmode(0);
	stoptermcap();
	flushbuf();
	ml_close_all();                 /* remove all memfiles */
	mch_restore_title(3);
	if (scriptout)
		fclose(scriptout);
	if (GuiWin)
	{
		MSG				msg;

		DestroyWindow(hVimWnd);
		while (GetMessageW(&msg, NULL, 0, 0))
		{
			if (!TranslateAcceleratorW(hVimWnd, hAcc, &msg))
			{
				TranslateMessage(&msg);
				DispatchMessageW(&msg);
			}
		}
	}
	if (BenchTime && !ctrlc_pressed)
	{
		DWORD		tm = GetTickCount() - BenchTime;
		char		buf[256];

		sprintf(buf, "%dm %ds %d",
				tm / (60 * 1000), (tm % (60 * 1000)) / 1000, tm % 1000);
		MessageBox(NULL, buf, "Bench Mark Time", MB_OK);
	}
	ExitProcess(r);
}

/*
 * function for ctrl-break interrupt
 */
#ifndef notdef
	static void
v_hangup(void *arg)
{
	TerminateThread(h_mainthread, 0);    /* forced terminate main processing */
	docmdline(":qall!");
}
#endif

	BOOL WINAPI
handler_routine(DWORD dwCtrlType)
{
#ifdef notdef
	cbrk_pressed = TRUE;
	ctrlc_pressed = TRUE;
#else
# ifdef __BORLANDC__
	DWORD		IdThread;
	HANDLE		hang_thread;
# else
	uintptr_t	hang_thread;		/* what _beginthread() returns */
# endif
	static int firsttime = TRUE;

	switch (dwCtrlType) {
	case CTRL_BREAK_EVENT:
	case CTRL_C_EVENT:
		cbrk_pressed = TRUE;
		ctrlc_pressed = TRUE;
		return(TRUE);
	case CTRL_CLOSE_EVENT:
	case CTRL_LOGOFF_EVENT:
	case CTRL_SHUTDOWN_EVENT:
		if (!firsttime) {
			return (TRUE);                /* show default dialog box */
		}
		firsttime = FALSE;
		SuspendThread(h_mainthread);		/* suspend main processing */
# ifdef __BORLANDC__
#  if 1
		TerminateThread(h_mainthread, 0);	/*forced terminate main processing*/
		docmdline(":qall!");
#  else
		hang_thread = CreateThread(NULL, 0,
					(LPTHREAD_START_ROUTINE)v_hangup, NULL, 0, &IdThread);
		if (hang_thread != NULL) {
			WaitForSingleObject(hang_thread, 10000);
										/* wait for finish thread */
		}
#  endif
# else    /* MICROSOFT */
		hang_thread = _beginthread(v_hangup, 0x2000, NULL);
						/* process exception by multi-thread C Library manner */
		if (hang_thread != (uintptr_t)-1) {
			WaitForSingleObject((HANDLE)hang_thread, 10000);
										/* wait for finish thread */
		}
# endif
		return(TRUE);
	}
	return(FALSE);
#endif
}

/*
 * set the tty in (raw) ? "raw" : "cooked" mode
 *
 */

	void
mch_settmode(int raw)
{
	DWORD           cmodein;
	DWORD           cmodeout;

	if (GuiWin || IsTelnet)
		return;
	if (term_console)
		scroll_region = FALSE;
	GetConsoleMode(hConIn, &cmodein);
	GetConsoleMode(hConOut, &cmodeout);

	if (raw) {
		if (term_console)
			outstr(T_TP);       /* set colors */

		cmodein &= ~(ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT |
#ifdef KANJI
					 ENABLE_MOUSE_INPUT |
#endif
					 ENABLE_ECHO_INPUT);
#ifdef notdef
		cmodein |= ENABLE_WINDOW_INPUT;
#endif

		SetConsoleMode(hConIn, cmodein);

#ifndef KANJI
		cmodeout &= ~(ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT);
#endif

		SetConsoleMode(hConOut, cmodeout);
		SetConsoleCtrlHandler(handler_routine, TRUE);
	} else {

		if (term_console)
			normvideo();        /* restore screen colors */

		cmodein |= (ENABLE_LINE_INPUT | ENABLE_PROCESSED_INPUT |
#ifdef KANJI
					ENABLE_MOUSE_INPUT |
#endif
					ENABLE_ECHO_INPUT);
		cmodein &= ~(ENABLE_WINDOW_INPUT);

		SetConsoleMode(hConIn, cmodein);

#ifndef KANJI
		cmodeout |= (ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT);
#endif

		SetConsoleMode(hConOut, cmodeout);

		SetConsoleCtrlHandler(handler_routine, FALSE);
	}
}

	int
mch_get_winsize(void)
{
	if (GuiWin)
	{
		Columns = nowCols;
		v_region = Rows = nowRows;
		mch_set_winsize();
#ifdef FEPCTRL
		if (FepInit)
		{
			LOGFONTW		wide;
			LOGFONTA		logfont;

			wide = config_jfont;
			wide.lfHeight			= -v_ychar;
			wide.lfWidth			= v_xchar;
			wide.lfItalic			= 0;
			wide.lfUnderline		= 0;
			wide.lfWeight			= FW_NORMAL;
			/* the IME call takes the narrow struct */
			font_narrow(&logfont, &wide);
			fep_win_font(hVimWnd, &logfont);
		}
#endif
	}
	else if (IsTelnet)
	{
		extern void getlinecol();

		getlinecol();	/* get "co" and "li" entries from termcap */
		if (Columns <= 0 || Rows <= 0)
		{
			Columns = 80;
			maxRows = Rows = 25;
			return OK;
		}
	}
	else
	{
		/*
		 * Use the console mode API
		 */
		if (GetConsoleScreenBufferInfo(hConOut, &csbi)) {
			maxRows = Rows = csbi.dwSize.Y;
			maxRows = csbi.dwSize.Y;
			Rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
			Columns = csbi.dwSize.X;
			DefaultAttribute = csbi.wAttributes;
#ifndef notdef
			if (!v_nt)
				DefaultAttribute = csbi.wAttributes | FOREGROUND_INTENSITY;
#endif
#ifdef KANJI
			if (term_console) {
				if (!(ver_info.dwPlatformId == VER_PLATFORM_WIN32_NT
						&& ver_info.dwMajorVersion == 3
						&& ver_info.dwMinorVersion == 50)) {
					Rows--;
				} else {
#  ifdef FEPCTRL
					if (FepInit)
					{
						if (fep_init())
							Rows--;
					}
#  endif
				}
			}
#endif
		} else {
			maxRows = Rows = 25;
			Columns = 80;
		}

		if (Columns < 5 || Columns > MAX_COLUMNS ||
			Rows < 2 || Rows > MAX_COLUMNS) {
			/* these values are overwritten by termcap size or default */
			Columns = 80;
			maxRows = Rows = 25;
			return OK;
		}
	}
	check_winsize();
	/*script_winsize();*/

	return OK;
}

/*********************************************************************
* FUNCTION: perr(PCHAR szFileName, int line, PCHAR szApiName,        *
*                DWORD dwError)                                      *
*                                                                    *
* PURPOSE: report API errors. Allocate a new console buffer, display *
*          error number and error text, restore previous console     *
*          buffer                                                    *
*                                                                    *
* INPUT: current source file name, current line number, name of the  *
*        API that failed, and the error number                       *
*                                                                    *
* RETURNS: none                                                      *
*********************************************************************/

/* maximum size of the buffer to be returned from FormatMessage */
#define MAX_MSG_BUF_SIZE 512

	void
perr(PCHAR szFileName, int line, PCHAR szApiName, DWORD dwError)
{
	CHAR            szTemp[1024];
	DWORD           cMsgLen;
	CHAR           *msgBuf;     /* buffer for message text from system */
	int             iButtonPressed;     /* receives button pressed in the
										 * error box */

	/* format our error message */
	sprintf(szTemp, "%s: Error %d from %s on line %d:\n", szFileName,
			dwError, szApiName, line);
	/* get the text description for that error number from the system */
	cMsgLen = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM |
						 FORMAT_MESSAGE_ALLOCATE_BUFFER | 40, NULL, dwError,
	 MAKELANGID(0, SUBLANG_ENGLISH_US), (LPTSTR) & msgBuf, MAX_MSG_BUF_SIZE,
							NULL);
	if (!cMsgLen)
		sprintf(szTemp + strlen(szTemp), "Unable to obtain error message text! \n"
				"%s: Error %d from %s on line %d", __FILE__,
				GetLastError(), "FormatMessage", __LINE__);
	else
		strcat(szTemp, msgBuf);
	strcat(szTemp, "\n\nContinue execution?");
	MessageBeep(MB_ICONEXCLAMATION);
	iButtonPressed = MessageBox(NULL, szTemp, "Console API Error",
						  MB_ICONEXCLAMATION | MB_YESNO | MB_SETFOREGROUND);
	/* free the message buffer returned to us by the system */
	if (cMsgLen)
		LocalFree((HLOCAL) msgBuf);
	if (iButtonPressed == IDNO)
		exit(1);
	return;
}
#define PERR(bSuccess, api) {if (!(bSuccess)) perr(__FILE__, __LINE__, \
	api, GetLastError());}


	static void
resizeConBufAndWindow(HANDLE hConsole, long xSize, long ySize)
{
	CONSOLE_SCREEN_BUFFER_INFO csbi;    /* hold current console buffer info */
	BOOL            bSuccess;
	SMALL_RECT      srWindowRect;       /* hold the new console size */
	COORD           coordScreen;

	bSuccess = GetConsoleScreenBufferInfo(hConsole, &csbi);
	PERR(bSuccess, "GetConsoleScreenBufferInfo");
	/* get the largest size we can size the console window to */
	coordScreen = GetLargestConsoleWindowSize(hConsole);
	PERR(coordScreen.X | coordScreen.Y, "GetLargestConsoleWindowSize");
	/* define the new console window size and scroll position */
	srWindowRect.Right = (SHORT) (min(xSize, coordScreen.X) - 1);
	srWindowRect.Bottom = (SHORT) (min(ySize, coordScreen.Y) - 1);
	srWindowRect.Left = srWindowRect.Top = (SHORT) 0;
	/* define the new console buffer size */
	coordScreen.X = xSize;
	coordScreen.Y = ySize;
	/* if the current buffer is larger than what we want, resize the */
	/* console window first, then the buffer */
	if ((DWORD) csbi.dwSize.X * csbi.dwSize.Y > (DWORD) xSize * ySize) {
		bSuccess = SetConsoleWindowInfo(hConsole, TRUE, &srWindowRect);
		PERR(bSuccess, "SetConsoleWindowInfo");
		bSuccess = SetConsoleScreenBufferSize(hConsole, coordScreen);
		PERR(bSuccess, "SetConsoleScreenBufferSize");
	}
	/* if the current buffer is smaller than what we want, resize the */
	/* buffer first, then the console window */
	if ((DWORD) csbi.dwSize.X * csbi.dwSize.Y < (DWORD) xSize * ySize) {
		bSuccess = SetConsoleScreenBufferSize(hConsole, coordScreen);
		PERR(bSuccess, "SetConsoleScreenBufferSize");
		bSuccess = SetConsoleWindowInfo(hConsole, TRUE, &srWindowRect);
		PERR(bSuccess, "SetConsoleWindowInfo");
	}
	/* if the current buffer *is* the size we want, don't do anything! */
	return;
}

	void
mch_set_winsize(void)
{
	if (GuiWin)
	{
		RECT		rcClient;
		RECT		rcWindow;

		GetClientRect(hVimWnd, &rcClient);
		if (GetWindowRect(hVimWnd, &rcWindow))
		{
			config_w = ((rcWindow.right - rcWindow.left)
							- (rcClient.right - rcClient.left))
							+ v_xchar * Columns;
			config_h = ((rcWindow.bottom - rcWindow.top)
							- (rcClient.bottom - rcClient.top))
							+ v_ychar * Rows;
			MoveWindow(hVimWnd, rcWindow.left, rcWindow.top, config_w, config_h, TRUE);
		}
	}
	else if (IsTelnet)
		;
	else
	{
		resizeConBufAndWindow(hConOut, Columns, Rows);
		mch_get_winsize();
	}
}

/*
 * Room for the shell, its switch and the whole command line. These buffers used
 * to be MAXPATHL, which was 260 on Windows: dodos() arrives with up to
 * CMDBUFFSIZE (1024) of command, and dofilter() builds its command in IObuff
 * with two temp file paths appended, so sprintf() into 260 bytes was writing
 * off the end of the stack for any command that was not short.
 */
#define SHCMDLEN	(MAXPATHL + IOSIZE)

/*
 * cmd.exe will not start in a UNC directory. It says so -- on its own console,
 * which under the GUI is a minimized window -- and runs in C:\Windows instead,
 * so ":r !dir" quietly listed the Windows directory. Editing a file over a
 * share, or through \\wsl.localhost when the exe is started from a WSL shell,
 * is enough to be there.
 *
 * There is nothing to be done about cmd's refusal, but where it lands instead
 * can at least be somewhere that belongs to the person running it. Move to it
 * for the duration of the shell command and say so, rather than leaving them to
 * wonder why the listing is of C:\Windows.
 *
 * Returns TRUE if it moved, having put the old directory in 'saved'.
 */
	static int
shell_cwd_enter(char *saved, int len)
{
	static int	told = FALSE;
	char		here[MAXPATHL];
	char		to[MAXPATHL];

	if (GetCurrentDirectory(sizeof(here), here) == 0)
		return FALSE;
	if (!(here[0] == '\\' && here[1] == '\\'))	/* not a UNC path */
		return FALSE;
	if (GetEnvironmentVariable("USERPROFILE", to, sizeof(to)) == 0
			&& GetTempPath(sizeof(to), to) == 0)
		return FALSE;
	if (STRLEN(here) >= (size_t)len || !SetCurrentDirectory(to))
		return FALSE;
	STRCPY(saved, here);
	/* Once per session. Every shell command run while editing over a share
	 * would otherwise say it, and after the first time it is noise. */
	if (!told)
	{
		told = TRUE;
		smsg((char_u *)"cmd.exe cannot run in %s; using %s", here, to);
		outchar('\n');
	}
	return TRUE;
}

	int
call_shell(char_u *cmd, int filter, int cooked)
{
	int             x = 0;
	char            newcmd[SHCMDLEN];
	char            oldcwd[MAXPATHL];
	int             moved;

	flushbuf();

	moved = shell_cwd_enter(oldcwd, sizeof(oldcwd));

#ifdef FEPCTRL
	if (FepInit && !GuiWin)
		fep_term();
#endif

	if (cooked)
		settmode(0);            /* set to cooked mode */

	if (GuiWin)
	{
		STARTUPINFO				si;
		PROCESS_INFORMATION		pi;

		memset(&pi, 0, sizeof(pi));
		memset(&si, 0, sizeof(si));
		si.cb = sizeof(si);
		if (cmd == NULL)
		{
			if (CreateProcess(p_sh, NULL, NULL, NULL, FALSE,
					CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi) == TRUE)
			{
				CloseHandle(pi.hProcess);
				CloseHandle(pi.hThread);
			}
			else
				x = GetLastError();
		}
		else if (filter || DoMake || BenchTime)
		{
			si.dwFlags = STARTF_USESHOWWINDOW;
			si.wShowWindow = /*SW_HIDE*/SW_MINIMIZE;
			snprintf(newcmd, sizeof(newcmd), "%s /c %s", p_sh, cmd);
			if (CreateProcess(NULL, newcmd, NULL, NULL, TRUE,
					CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi) == TRUE)
			{
				for (;;)
				{
					if (WaitForSingleObject(pi.hProcess, 100) == WAIT_OBJECT_0)
					{
						DWORD	status;

						/* the shell's own exit status, which this branch used
						 * to drop: it is the only sign the user gets that the
						 * command failed when cmd.exe wrote the reason to its
						 * own console rather than to the redirected output */
						if (GetExitCodeProcess(pi.hProcess, &status))
							x = (int)status;
						CloseHandle(pi.hProcess);
						CloseHandle(pi.hThread);
						break;
					}
					breakcheck();
				}
				TopWindow(hVimWnd);
			}
			else
				x = GetLastError();
		}
		else
		{
			static	char	*	ext[]	= {".com", ".exe", ".bat", NULL};
			char			**	ep		= ext;
			/* the whole command is copied in here and then split at the
			 * first blank, so these hold a command line, not a path */
			char				exe[SHCMDLEN];
			char				arg[SHCMDLEN];
			char			*	p		= exe;
			char			*	last	= NULL;
			BOOL				bShell	= FALSE;

			exe[0] = arg[0] = NUL;
			strcpy(exe, cmd);
			if (getperm(p) == (-1))
			{
				while (*p)
				{
					if (*p == ' ' || *p == '\t')
					{
						*p = NUL;
						p++;
						strcpy(arg, p);
						break;
					}
					p++;
				}
			}
			p = exe;
			while (*p)
			{
				if (*p == '.')
					last = p;
				p++;
			}
			if (last)
			{
				bShell = TRUE;
				while (*ep)
				{
					if (strnicmp(last, *ep, strlen(*ep)) == 0)
					{
						bShell = FALSE;
						break;
					}
					ep++;
				}
			}
			if (last == NULL || bShell == FALSE)
			{
				char		exebuf[MAXPATHL];
				SHFILEINFO	shinfo;
				DWORD		dwRtn;

				if ((INT_PTR)FindExecutable(exe, ".", exebuf) <= 32)
					;
				else
				{
					dwRtn = SHGetFileInfo(exebuf, SHGFI_USEFILEATTRIBUTES,
										&shinfo, sizeof(shinfo), SHGFI_EXETYPE);
					if ((LOWORD(dwRtn) == 0x4550/*PE*/ && HIWORD(dwRtn) == 0)
							|| (LOWORD(dwRtn) == 0x5a4d/*MZ*/ && HIWORD(dwRtn) == 0))
						;
					else
						bShell = TRUE;
				}
			}
			if (bShell)
			{
				x = (INT_PTR)ShellExecute(NULL, NULL, exe, arg, ".", SW_SHOW);
				if (x > 32)
					x = 0;
			}
			else
			{
				if (v_nt)
					snprintf(newcmd, sizeof(newcmd), "%s /c %s && pause", p_sh, cmd);
				else
				{
					FILE	*	fp;
					int			i;
					char		batbuf[MAXPATHL];

					for (i = 0; i < 1000; i++)
					{
						batbuf[0] = '\0';
						if (p_dir != NULL && *p_dir != NUL)
						{
							if (*p_dir == '>')	/* skip '>' in front of dir */
								STRCPY(batbuf, p_dir + 1);
							else
								STRCPY(batbuf, p_dir);
							if (!ispathsep(*(batbuf + STRLEN(batbuf) - 1)))
								STRCAT(batbuf, PATHSEPSTR);
						}
						sprintf(&batbuf[STRLEN(batbuf)], "vim%05d.bat", i);
						if (getperm(batbuf) < 0)
						{
							if ((fp = fopen(batbuf, "w")) != NULL)
							{
								fprintf(fp, "%s\r\n", cmd);
								fprintf(fp, "pause\r\n");
								fprintf(fp, "del \"%s\"\r\n", batbuf);
								fclose(fp);
								snprintf(newcmd, sizeof(newcmd), "%s /c %s", p_sh, batbuf);
								break;		/* for loop */
							}
						}
					}
				}
#if 0
				if (pLockSetForegroundWindow)
					pLockSetForegroundWindow(LSFW_UNLOCK);
#endif
				if (CreateProcess(NULL, newcmd, NULL, NULL, FALSE,
						CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi) == TRUE)
				{
#if 0
					if (pAllowSetForegroundWindow)
						pAllowSetForegroundWindow(pi.dwProcessId);
#endif
					CloseHandle(pi.hProcess);
					CloseHandle(pi.hThread);
				}
				else
					x = GetLastError();
#if 0
				if (pLockSetForegroundWindow)
					pLockSetForegroundWindow(LSFW_LOCK);
#endif
			}
		}
		if (!filter && x == 0 && c_end == 0)
			cbuf[c_end++] = '\n';
	}
	else
	{
		if (cmd == NULL)
			x = system(p_sh);
		else
		{
			snprintf(newcmd, sizeof(newcmd), "%s /c %s", p_sh, cmd);
			x = system(newcmd);
		}
		outchar('\n');
	}
	if (moved)					/* back to the directory being edited */
		SetCurrentDirectory(oldcwd);
	if (cooked)
		settmode(1);            /* set to raw mode */

#ifdef WEBB_COMPLETE
	if (x && !expand_interactively)
#else
	if (x)
#endif
	{
		smsg("%d returned", x);
		outchar('\n');
	}

#ifdef FEPCTRL
	if (FepInit && !GuiWin)
		fep_init();
#endif

	resettitle();
	return x;
}

#define FL_CHUNK 32

	static void
addfile(FileList *fl, char *f, int isdir)
{
	char           *p;

	if (!fl->file) {
		fl->file = (char_u **) alloc(sizeof(char *) * FL_CHUNK);
		if (!fl->file)
			return;
		fl->nfiles = 0;
		fl->maxfiles = FL_CHUNK;
	}
	if (fl->nfiles >= fl->maxfiles) {
		char_u        **t;
		int             i;

		t = (char_u **)lalloc(sizeof(char *) * (fl->maxfiles + FL_CHUNK), TRUE);
		if (!t)
			return;
		for (i = fl->nfiles - 1; i >= 0; i--)
			t[i] = fl->file[i];
		free(fl->file);
		fl->file = t;
		fl->maxfiles += FL_CHUNK;
	}
	p = alloc((unsigned) (strlen(f) + 1 + isdir));
	if (p) {
		strcpy(p, f);
		if (isdir)
			strcat(p, "/");
	}
	fl->file[fl->nfiles++] = p;
}

#ifdef __BORLANDC__
	static int
pstrcmp(char_u **a, char_u **b)
{
	return (strcmp(*a, *b));
}
#else
	static int
pstrcmp(const void *s1, const void *s2)
{
	unsigned char * p1 = *(unsigned char **)s1;
	unsigned char * p2 = *(unsigned char **)s2;

#ifdef notdef
	return (strcmp(p1, p2));
#else
	return (stricmp(p1, p2));
#endif
}
#endif

	int
has_wildcard(char_u *s)
{
	int			pos = 1;

	if (do_drag)
		return 0;
	if (s)
#ifndef notdef
		if (*s == '~' || *s == '$')
			return 1;
		else
#endif
		for (; *s; ++s, ++pos)
		{
#ifdef XARGS
			if (*s == '[' || *s == '{')
				return pos;
			else
#endif
			if (*s == '?' || *s == '*')
				return pos;
#ifdef KANJI
			if (ISkanji(*s))
			{
				++s;
				++pos;
			}
#endif
		}
	return 0;
}

static int
has_wildcards(char_u *s)
{
	int			pos = 1;
	if (s)
		for (; *s; ++s, ++pos)
		{
			if (*s == '?' || *s == '*')
				return pos;
#ifdef KANJI
			if (ISkanji(*s))
			{
				++s;
				++pos;
			}
#endif
		}
	return 0;
}

	static void
strlowcpy(char *d, char *s)
{
	while (*s)
		*d++ = tolower(*s++);
	*d = '\0';
}

	static int
expandpath(FileList *fl, char *path, int fonly, int donly, int notf)
{
	/*
	 * Room for the whole path: a UTF-8 name is up to three bytes a character,
	 * so MAX_PATH bytes will not hold one, let alone one with a directory in
	 * front of it.
	 *
	 * Allocated rather than on the stack. This recurses once per wildcard
	 * component of the pattern and the pattern comes off the command line, which
	 * is what decides how deep it goes; two buffers this size per frame is a
	 * stack to be run out of.
	 */
	int             buflen = MAXPATHL + FIND_NAMELEN;
	char           *buf;
	char           *name;
	char           *p,
				   *s,
				   *e;
	int             lastn,
					c = 1,
					r,
					retval;
	HANDLE          hFind;

	if ((buf = (char *)alloc((unsigned)buflen)) == NULL)
		return 0;
	if ((name = (char *)alloc((unsigned)FIND_NAMELEN)) == NULL)
	{
		free(buf);
		return 0;
	}
	lastn = fl->nfiles;

/*
 * Find the first part in the path name that contains a wildcard.
 * Copy it into buf, including the preceding characters.
 */
	p = buf;
	s = NULL;
	e = NULL;
#ifndef notdef
	memset(buf, NUL, (size_t)buflen);
#endif
	while (*path) {
		if (*path == '\\' || *path == ':' || *path == '/') {
			if (e)
				break;
			else
				s = p;
		}
		if (*path == '*' || *path == '?')
			e = p;
#ifdef KANJI
		if (ISkanji(*path))
			*p++ = *path++;
#endif
		*p++ = *path++;
	}
	e = p;
	if (s)
		s++;
	else
		s = buf;

	/* now we have one wildcard component between s and e */
	*e = '\0';
	r = 0;
	/* If we are expanding wildcards we try both files and directories */
	if ((hFind = find_first_name(buf, name, FIND_NAMELEN, NULL))
													== INVALID_HANDLE_VALUE) {
		/* not found */
#ifndef notdef
		if (has_wildcard(buf))
			retval = 0;
		else
#endif
		{
			strcpy(e, path);
			if (notf)
				addfile(fl, buf, FALSE);
			retval = 1;         /* unexpanded or empty */
		}
		free(name);
		free(buf);
		return retval;
	}
	while (c) {
		if ((int)((s - buf) + strlen(name) + strlen(path)) < buflen) {
#ifdef notdef
			strlowcpy(s, name);
#else
			strcpy(s, name);
#endif
			if (*s != '.' || (s[1] != '\0' && (s[1] != '.' || s[2] != '\0'))) {
				strcat(buf, path);
				if (!has_wildcard(path))
					addfile(fl, buf, (isdir(buf) > 0));
				else
					r |= expandpath(fl, buf, fonly, donly, notf);
			}
		}
		c = find_next_name(hFind, name, FIND_NAMELEN, NULL);
	}
	qsort(fl->file + lastn, fl->nfiles - lastn, sizeof(char *), pstrcmp);
	FindClose(hFind);
	free(name);
	free(buf);
	return r;
}

/*
 * MSDOS rebuilt of Scott Ballantynes ExpandWildCard for amiga/arp.
 * jw
 */

	int
ExpandWildCards(int num_pat, char_u **pat, int *num_file, char_u ***file, int files_only, int list_notfound)
{
	int             i,
					r = 0;
	FileList        f;

	f.file = NULL;
	f.nfiles = 0;
	for (i = 0; i < num_pat; i++)
	{
#if !defined(notdef) && defined(XARGS)
		char_u		buf[MAXPATHL+4];
		int			j;
		char	**	result;

		memset(buf, 0, sizeof(buf));
		expand_env(pat[i], buf, MAXPATHL);
		if (strcspn(buf, " \t{") < strlen(buf))
		{
			if (do_drag)
				j = strlen(buf) + 1;
			else if ((j = has_wildcards(buf)) == 0)
				j = strlen(buf) + 1;
			memmove(&buf[1], buf, strlen(buf) + 1);
			memmove(&buf[j+1], &buf[j], strlen(buf) + 1 - j);
			buf[0] = '\"';
			buf[j] = '\"';
		}
		result = glob_filename(buf);
		for (j = 0; result[j] != NULL; j++)
		{
			if (!has_wildcard(result[j]))
				addfile(&f, result[j], files_only ? FALSE : (isdir(result[j]) == TRUE));
			else
				r |= expandpath(&f, result[j], files_only, 0, list_notfound);
			free(result[j]);
		}
		free(result);
#else
		if (!has_wildcard(pat[i]))
			addfile(&f, pat[i], files_only ? FALSE : (isdir(pat[i]) > 0));
		else
			r |= expandpath(&f, pat[i], files_only, 0, list_notfound);
#endif
	}
	if (r == 0)
	{
		*num_file = f.nfiles;
		*file = f.file;
	}
	else
	{
		*num_file = 0;
		*file = NULL;
	}
	return (r ? FAIL : OK);
}

	void
FreeWild(int num, char_u **file)
{
	if (file == NULL || num <= 0)
		return;
	while (num--)
		free(file[num]);
	free(file);
}

/*
 * The normal chdir() does not change the default drive.
 * This one does.
 */
#undef chdir
int             vim_chdir(char_u *path)
{
	if (path[0] == NUL)         /* just checking... */
		return FAIL;
	if (path[1] == ':') {       /* has a drive name */
		if (_chdrive(toupper(path[0]) - 'A' + 1))
			return -1;          /* invalid drive name */
		path += 2;
	}
	if (*path == NUL)           /* drive name only */
		return OK;
#ifdef __BORLANDC__
	return chdir(path);         /* let the normal chdir() do the rest */
#else
	return _chdir(path);        /* let the normal chdir() do the rest */
#endif
}

	static void
clrscr(void)
{
	DWORD           count;

	ntcoord.X = 0;
	ntcoord.Y = 0;
	FillConsoleOutputCharacter(hConOut, ' ', Columns * maxRows,
							   ntcoord, &count);
	FillConsoleOutputAttribute(hConOut, DefaultAttribute, maxRows * Columns,
							   ntcoord, &count);
}

	static void
clreol(void)
{
	DWORD           count;
	FillConsoleOutputCharacter(hConOut, ' ',
								Columns - ntcoord.X,
								ntcoord, &count);
	FillConsoleOutputAttribute(hConOut, DefaultAttribute,
								Columns - ntcoord.X,
								ntcoord, &count);
}

	static void
insline(int count)
{
	SMALL_RECT      source;
	COORD           dest;
	CHAR_INFO       fill;

	dest.X = 0;
	dest.Y = ntcoord.Y + count;

	source.Left = 0;
	source.Top = ntcoord.Y;
	source.Right = Columns;
	source.Bottom = Rows - 1;

	fill.Char.AsciiChar = ' ';
	fill.Attributes = DefaultAttribute;

	ScrollConsoleScreenBuffer(hConOut, &source, (PSMALL_RECT) 0, dest, &fill);
#ifndef notdef
	if (maxRows != Rows)
	{
		DWORD           w;
		dest.X = 0;
		dest.Y = maxRows - 1;
		FillConsoleOutputCharacter(hConOut, ' ', Columns, dest, &w);
		FillConsoleOutputAttribute(hConOut, DefaultAttribute, Columns, dest, &w);
	}
#endif
}

	static void
delline(int count)
{
	SMALL_RECT      source;
	COORD           dest;
	CHAR_INFO       fill;

	dest.X = 0;
	dest.Y = ntcoord.Y;

	source.Left = 0;
	source.Top = ntcoord.Y + count;
	source.Right = Columns;
	source.Bottom = maxRows - 1;

	/* get current attributes and fill out CHAR_INFO structure for fill char */
	fill.Char.AsciiChar = ' ';
	fill.Attributes = DefaultAttribute;

	ScrollConsoleScreenBuffer(hConOut, &source, (PSMALL_RECT) 0, dest, &fill);
#ifndef notdef
	if (count > 2)
	{
		DWORD           w;
		dest.X = 0;
		dest.Y = maxRows - 1 - count;
		FillConsoleOutputCharacter(hConOut, ' ', Columns * count, dest, &w);
		FillConsoleOutputAttribute(hConOut, DefaultAttribute,
												Columns * count, dest, &w);
	}
#endif
}


	static void
scroll(void)
{
	SMALL_RECT      source;
	COORD           dest;
	CHAR_INFO       fill;

	dest.X = 0;
	dest.Y = 0;

	source.Left = 0;
	source.Top = 1;
	source.Right = Columns;
	source.Bottom = Rows - 1;

	/* get current attributes and fill out CHAR_INFO structure for fill char */
	fill.Char.AsciiChar = ' ';
	fill.Attributes = DefaultAttribute;

	ScrollConsoleScreenBuffer(hConOut, &source, (PSMALL_RECT) 0, dest, &fill);
}

	static void
gotoxy(int x, int y)
{
	ntcoord.X = x - 1;
	ntcoord.Y = y - 1;
	SetConsoleCursorPosition(hConOut, ntcoord);
}

	static void
normvideo(void)
{
	WORD            attr = DefaultAttribute;

	SetConsoleTextAttribute(hConOut, attr);
}

	static void
textattr(WORD attr)
{
	SetConsoleTextAttribute(hConOut, attr);
}

	static void
putch(char c)
{
	DWORD           count;

	WriteConsole(hConOut, &c, 1, &count, 0);
	ntcoord.X += count;
}

	static void
delay(int x)
{
	if (GuiWin)
	{
		if (BenchTime == 0)
			WaitForChar(x);
	}
	else if (IsTelnet)
		Sleep(x);
	else
	{
#ifdef notdef
		Sleep(x);
#else
		while (x > 0)
		{
			if (kbhit())
				break;
			Sleep(100);
			x -= 100;
		}
#endif
	}
}

#ifdef __BORLANDC__
	void
#else
	int
#endif
sleep(int x)
{
#ifdef notdef
	Sleep(x * 1000);
#else
	delay(x * 1000);
#endif
#ifndef __BORLANDC__
	return 0;
#endif
}

	static void
vbell(void)
{
	COORD           origin = {0, 0};
	WORD            flash = ~DefaultAttribute & 0xff;
	DWORD           count;
	LPWORD          oldattrs;

	if (p_vb) {
		oldattrs = (LPWORD) alloc(Rows * Columns * sizeof(WORD));
		if (oldattrs) {
			ReadConsoleOutputAttribute(hConOut, oldattrs, Rows * Columns,
											origin, &count);
			FillConsoleOutputAttribute(hConOut, flash, Rows * Columns,
											origin, &count);
			WriteConsoleOutputAttribute(hConOut, oldattrs, Rows * Columns,
											origin, &count);
			free(oldattrs);
		}
	} else {
		WriteConsole(hConOut, "\a", 1, &count, 0);
	}
}

	static void
cursor_visible(int visible)
{
#ifndef FEPCTRL
	CONSOLE_CURSOR_INFO cci;

	cci.bVisible = visible ? TRUE : FALSE;
#ifdef notdef
	cci.dwSize = 100;           /* 100 percent cursor */
#else
	cci.dwSize = 30;            /*  30 percent cursor */
#endif
	SetConsoleCursorInfo(hConOut, &cci);
#endif
}

	void
set_window(void)
{
}

/*
 * check for an "interrupt signal": CTRL-break or CTRL-C
 */
	void
breakcheck(void)
{
	if (GuiWin)
	{
		MSG				msg;

		while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE) == TRUE)
		{
			if (!TranslateAcceleratorW(hVimWnd, hAcc, &msg))
			{
				TranslateMessage(&msg);
				DispatchMessageW(&msg);
			}
		}
	}
	if (ctrlc_pressed)
	{
		ctrlc_pressed = FALSE;
		got_int = TRUE;
	}
}

	long
mch_avail_mem(int spec)
{
	return 0x7fffffff;        /* virual memory eh */
}

/*
 * return non-zero if a character is available
 */
	int
mch_char_avail(void)
{
	return WaitForChar(0);
}

/*
 * set screen mode, always fails.
 */
	int
mch_screenmode(char_u *arg)
{
	EMSG("Screen mode setting not supported");
	return FAIL;
}

#ifndef notdef
	static int
isctlkey(void)
{
	switch (ir.Event.KeyEvent.wVirtualKeyCode) {
	case VK_DELETE:
		return '\177';
#ifdef NT106KEY
	case 0xf3:  case 0xf4:        /* ZENKAKU / HANKAKU KEY */
		if (config_nt106)
			return '[' & 0x1f;        /* ESC key !! */
		break;
#endif
	case '6':                    /* jkeyb.sys */ /* ken add */
	case 0xde:                    /* ^ key */
		if (ir.Event.KeyEvent.dwControlKeyState
				& (RIGHT_CTRL_PRESSED | LEFT_CTRL_PRESSED))
			return '^' & 0x1f;
		break;
	case '@':
	case 0xc0: /* '@' key */
		if (ir.Event.KeyEvent.dwControlKeyState
				& (RIGHT_CTRL_PRESSED | LEFT_CTRL_PRESSED))
			return K_ZERO;
		break;
	}
	return 0;
}
#endif

	void
chk_ctlkey(int *c, int *k)
{
	int		w;

	if (term_console)
		return;
	while (1)
	{
		if (*c == Ctrl('Q') || *c == Ctrl(']'))
		{
			w = vgetc();
			switch (w) {
			case 'h':
				*c = K_LARROW;
				break;
			case 'j':
				*c = K_DARROW;
				break;
			case 'k':
				*c = K_UARROW;
				break;
			case 'l':
				*c = K_RARROW;
				break;
			case 'H':
			case 'b':
				*c = K_SLARROW;
				break;
			case 'J':
			case Ctrl('F'):
				*c = K_SDARROW;
				break;
			case 'K':
			case Ctrl('B'):
				*c = K_SUARROW;
				break;
			case 'L':
			case 'w':
				*c = K_SRARROW;
				break;
			default:
				beep();
#ifdef KANJI
				if (ISkanji(w))
					vgetc();
#endif
				*c = vgetc();
#ifdef KANJI
				if (ISkanji(*c))
					*k = vgetc();
#endif
				continue;
			}
		}
		break;
	}
}

static int
iswave(char *fname)
{
	int				fd;
	char			magic[15];
	int				len;

	if ((strlen(fname) >= strlen(".wav"))
			&& strnicmp(&fname[strlen(fname) - 4], ".wav", strlen(".wav")) == 0)
	{
		if ((fd = open(fname, O_RDONLY, S_IREAD | S_IWRITE)) < 0)
			return(FALSE);
		len = read(fd, magic, 15);
		close(fd);
		if (len < 15)
			return(FALSE);
		if (memcmp(magic, "RIFF", 4) == 0
								&& memcmp(&magic[8], "WAVEfmt", 4) == 0)
			return(TRUE);
	}
	return(FALSE);
}

/*------------------------------------------------------------------------------
 *	login dialog
 *----------------------------------------------------------------------------*/
static INT_PTR CALLBACK
PrinterDialog(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	int					wmId;
	OPENFILENAME		ofn;

	switch (uMsg) {
	case WM_INITDIALOG:
		SetDialogSystemFont(hWnd);
		if (strlen(config_printer))
			SetDlgItemTextU8(hWnd, 1000, (char_u *)config_printer);
		return(TRUE);
	case WM_DESTROY:
		break;
	case WM_COMMAND:
		wmId = LOWORD(wParam);
		switch (wmId) {
		case IDOK:
			GetDlgItemTextU8(hWnd, 1000, (char_u *)config_printer, sizeof(config_printer));
			EndDialog(hWnd, 0);
			return(TRUE);
		case IDCANCEL:
			EndDialog(hWnd, 1);
			return(TRUE);
		case 1001:
			memset(&ofn, 0, sizeof(ofn));
			NameBuff[0] = '\0';
			GetDlgItemTextU8(hWnd, 1000, (char_u *)IObuff, IOSIZE);
			*gettail(IObuff) = NUL;
			ofn.lStructSize		= sizeof(ofn);
			ofn.hwndOwner		= hWnd;
			ofn.hInstance		= hInst;
			ofn.lpstrFilter		= "Program File(*.exe;*.com;*.bat)\0*.exe;*.com;*.bat\0ALL(*.*)\0*.*\0";
			ofn.lpstrCustomFilter = (LPSTR)NULL;
			ofn.nMaxCustFilter	= 0L;
			ofn.nFilterIndex	= 1;
			ofn.lpstrFile		= NameBuff;
			ofn.nMaxFile		= MAXPATHL;
			ofn.lpstrFileTitle	= NULL;
			ofn.nMaxFileTitle	= 0;
			ofn.lpstrInitialDir	= IObuff;
			ofn.lpstrTitle		= "Print Command Select";
			ofn.Flags			= OFN_HIDEREADONLY | OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST;
			ofn.nFileOffset		= 0;
			ofn.nFileExtension	= 0;
			ofn.lpstrDefExt		= NULL;
			if (GetOpenFileName(&ofn))
			{
				strcpy(IObuff, "\"");
				strcat(IObuff, NameBuff);
				strcat(IObuff, "\"");
				SendDlgItemMessage(hWnd, 1000, EM_SETSEL, 0, (LPARAM)-2);
				SetDlgItemTextU8(hWnd, 1000, (char_u *)IObuff);
			}
			break;
		}
		break;
	}
	return(FALSE);
}

DWORD WINAPI
PrinterThread(PVOID filename)
{
	STARTUPINFO				si;
	PROCESS_INFORMATION		pi;
	char					command[MAXPATHL * 2];

	memset(&pi, 0, sizeof(pi));
	memset(&si, 0, sizeof(si));
	si.cb = sizeof(si);
	si.dwFlags = STARTF_USESHOWWINDOW;
	si.wShowWindow = /*SW_HIDE*/SW_MINIMIZE;
	sprintf(command, "%s \"%s\"", config_printer, filename);
	if (CreateProcess(NULL, command, NULL, NULL, FALSE,
						CREATE_NEW_CONSOLE, NULL, NULL, &si, &pi) == TRUE)
	{
		WaitForSingleObject(pi.hProcess, INFINITE);
		CloseHandle(pi.hProcess);
		CloseHandle(pi.hThread);
		TopWindow(hVimWnd);
	}
	remove(filename);
	free(filename);
	ExitThread(0);
	return(0);
}

/*------------------------------------------------------------------------------
 *	login dialog
 *----------------------------------------------------------------------------*/
LRESULT PASCAL
BitmapHookProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static HWND		hwndBrush;
	LPOFNOTIFY		pofn;

	switch (uMsg) {
	case WM_INITDIALOG:
		SetDialogSystemFont(hwnd);
		hwndBrush = GetDlgItem(hwnd, 1010);
		return(TRUE);
	case WM_NOTIFY:
		pofn = (LPOFNOTIFY)lParam;
		if (pofn->hdr.code == CDN_SELCHANGE)
		{
			if (CommDlg_OpenSave_GetSpec(GetParent(hwnd), NameBuff, MAXPATHL) <= MAXPATHL)
				isbitmap(NameBuff, hwndBrush);
		}
		break;
	case WM_COMMAND:
		if (wParam == 1013)
			GetDlgItemText(hwnd, edt1, NameBuff, MAXPATHL);
		break;
	}
	return(FALSE);
}

/*------------------------------------------------------------------------------
 *	login dialog
 *----------------------------------------------------------------------------*/
static INT_PTR CALLBACK
BitmapDialog(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	int					wmId;
	OPENFILENAME		ofn;
	static DWORD		bUse;
	static HWND			udWnd;
	static DWORD		bitsize;
	static DWORD		bitcenter;

	switch (uMsg) {
	case WM_INITDIALOG:
		SetDialogSystemFont(hWnd);
		if (strlen(config_bitmapfile))
			SetDlgItemTextU8(hWnd, 1000, (char_u *)config_bitmapfile);
		bUse = config_bitmap;
		if (bUse)
			CheckDlgButton(hWnd, 1003, MF_CHECKED);
		else
			CheckDlgButton(hWnd, 1003, MF_UNCHECKED);
		bitsize = config_bitsize;
		bitcenter  = config_bitcenter;
		if (config_bitcenter)
			CheckDlgButton(hWnd, 1004, MF_CHECKED);
		else
			CheckDlgButton(hWnd, 1004, MF_UNCHECKED);
		if (pCreateUpDownControl != NULL)
		{
			udWnd = pCreateUpDownControl(
					WS_CHILD | WS_BORDER | WS_VISIBLE | UDS_SETBUDDYINT | UDS_ALIGNRIGHT,
					132, 39, 8, 12, hWnd, 1009, hInst,
					GetDlgItem(hWnd, 1002), 100, 10, config_bitsize);
		}
		else
		{
			udWnd = NULL;
			wsprintf(NameBuff, "%d", config_bitsize);
			SetDlgItemTextU8(hWnd, 1002, (char_u *)NameBuff);
		}
		return(TRUE);
	case WM_DESTROY:
		break;
	case WM_VSCROLL:
		if (udWnd == (HWND)lParam)
		{
			config_bitsize = GetDlgItemInt(hWnd, 1002, NULL, FALSE);
			updateScreen(CLEAR);
			return 1;
		}
		break;
	case WM_COMMAND:
		wmId = LOWORD(wParam);
		switch (wmId) {
		case IDOK:
			GetDlgItemTextU8(hWnd, 1000, (char_u *)config_bitmapfile, sizeof(config_bitmapfile));
			config_bitmap = bUse;
			if (isbitmap(config_bitmapfile, NULL))
			{
				config_bitsize = GetDlgItemInt(hWnd, 1002, NULL, FALSE);
				if (config_bitsize > 100)
					config_bitsize = 100;
				if (config_bitsize < 10)
					config_bitsize = 100;
				EndDialog(hWnd, 0);
			}
			else
			{
				config_bitmap = FALSE;
				config_bitmapfile[0] = '\0';
				config_bitsize   = bitsize;
				config_bitcenter = bitcenter;
				EndDialog(hWnd, 1);
			}
			return(TRUE);
		case IDCANCEL:
			config_bitcenter = bitcenter;
			EndDialog(hWnd, 1);
			return(TRUE);
		case 1001:
			memset(&ofn, 0, sizeof(ofn));
			NameBuff[0] = '\0';
			GetDlgItemTextU8(hWnd, 1000, (char_u *)IObuff, IOSIZE);
			*gettail(IObuff) = NUL;
			ofn.lStructSize		= sizeof(ofn);
			ofn.hwndOwner		= hWnd;
			ofn.hInstance		= hInst;
			ofn.lpstrFilter		= "Graphic Files(*.bmp;*.gif;*.jpg;*.ico;*.emf;*.wmf)\0*.bmp;*.gif;*.jpg;*.ico;*.emf;*.wmf\0Bitmaps(*.bmp)\0*.bmp\0GIF Files(*.gif)\0*.gif\0JPEG Files(*.jpg)\0*.jpg\0Icons(*.ico)\0*.ico\0Enhanced Metafiles(*.emf)\0*.emf\0Windows Metafiles(*.wmf)\0*.wmf\0ALL(*.*)\0*.*\0";
			ofn.lpstrCustomFilter = (LPSTR)NULL;
			ofn.nMaxCustFilter	= 0L;
			ofn.nFilterIndex	= 1;
			ofn.lpstrFile		= NameBuff;
			ofn.nMaxFile		= MAXPATHL;
			ofn.lpstrFileTitle	= NULL;
			ofn.nMaxFileTitle	= 0;
			ofn.lpstrInitialDir	= IObuff;
			ofn.lpstrTitle		= "Bitmap File Select";
			ofn.Flags			= OFN_HIDEREADONLY | OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST;
			ofn.nFileOffset		= 0;
			ofn.nFileExtension	= 0;
			ofn.lpstrDefExt		= NULL;
			if (!(ver_info.dwPlatformId == VER_PLATFORM_WIN32_NT
											&& ver_info.dwMajorVersion == 3))
			{
				ofn.Flags			= OFN_HIDEREADONLY | OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST | OFN_ENABLEHOOK | OFN_ENABLETEMPLATE;
				ofn.lpTemplateName	= "BITMAPDLGEXP";
				ofn.lpfnHook		= (LPOFNHOOKPROC)BitmapHookProc;
			}
			if (GetOpenFileName(&ofn))
			{
				SendDlgItemMessage(hWnd, 1000, EM_SETSEL, 0, (LPARAM)-2);
				SetDlgItemTextU8(hWnd, 1000, (char_u *)NameBuff);
				if (strcmp(config_bitmapfile, NameBuff) != 0)
				{
					config_bitsize = 100;
					wsprintf(NameBuff, "%d", config_bitsize);
					SetDlgItemTextU8(hWnd, 1002, (char_u *)NameBuff);
				}
			}
			break;
		case 1003:
			if (bUse)
			{
				bUse = FALSE;
				CheckDlgButton(hWnd, 1003, MF_UNCHECKED);
			}
			else
			{
				bUse = TRUE;
				CheckDlgButton(hWnd, 1003, MF_CHECKED);
			}
			break;
		case 1004:
			if (config_bitcenter)
			{
				config_bitcenter = FALSE;
				CheckDlgButton(hWnd, 1004, MF_UNCHECKED);
			}
			else
			{
				config_bitcenter = TRUE;
				CheckDlgButton(hWnd, 1004, MF_CHECKED);
			}
			updateScreen(CLEAR);
			break;
		}
		break;
	}
	return(FALSE);
}

/*------------------------------------------------------------------------------
 *	login dialog
 *----------------------------------------------------------------------------*/
LRESULT PASCAL
WaveHookProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	LPOFNOTIFY pofn;

	switch (uMsg) {
	case WM_INITDIALOG:
		SetDialogSystemFont(hwnd);
		return(TRUE);
	case WM_NOTIFY:
		pofn = (LPOFNOTIFY)lParam;
		if (pofn->hdr.code == CDN_SELCHANGE)
			CommDlg_OpenSave_GetSpec(GetParent(hwnd), NameBuff, MAXPATHL);
		break;
	case WM_COMMAND:
		if (wParam == 1013)
		{
			if (ver_info.dwPlatformId == VER_PLATFORM_WIN32_NT
											&& ver_info.dwMajorVersion == 3)
				GetDlgItemText(hwnd, edt1, NameBuff, MAXPATHL);
			PlaySound(NameBuff, NULL, SND_FILENAME);
		}
		break;
	}
	return FALSE;
}

/*------------------------------------------------------------------------------
 *	login dialog
 *----------------------------------------------------------------------------*/
static INT_PTR CALLBACK
WaveDialog(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	int					wmId;
	OPENFILENAME		ofn;
	static DWORD		bUse;

	switch (uMsg) {
	case WM_INITDIALOG:
		SetDialogSystemFont(hWnd);
		if (strlen(config_wavefile))
			SetDlgItemTextU8(hWnd, 1000, (char_u *)config_wavefile);
		bUse = config_wave;
		if (bUse)
			CheckDlgButton(hWnd, 1003, MF_CHECKED);
		else
			CheckDlgButton(hWnd, 1003, MF_UNCHECKED);
		return(TRUE);
	case WM_DESTROY:
		break;
	case WM_COMMAND:
		wmId = LOWORD(wParam);
		switch (wmId) {
		case IDOK:
			GetDlgItemTextU8(hWnd, 1000, (char_u *)config_wavefile, sizeof(config_wavefile));
			config_wave = bUse;
			if (iswave(config_wavefile))
				EndDialog(hWnd, 0);
			else
			{
				config_wavefile[0] = '\0';
				config_wave = FALSE;
				EndDialog(hWnd, 1);
			}
			return(TRUE);
		case IDCANCEL:
			EndDialog(hWnd, 1);
			return(TRUE);
		case 1001:
			memset(&ofn, 0, sizeof(ofn));
			NameBuff[0] = '\0';
			GetDlgItemTextU8(hWnd, 1000, (char_u *)IObuff, IOSIZE);
			*gettail(IObuff) = NUL;
			ofn.lStructSize		= sizeof(ofn);
			ofn.hwndOwner		= hWnd;
			ofn.hInstance		= hInst;
			ofn.lpstrFilter		= "Wave File(*.wav)\0*.wav\0ALL(*.*)\0*.*\0";
			ofn.lpstrCustomFilter = (LPSTR)NULL;
			ofn.nMaxCustFilter	= 0L;
			ofn.nFilterIndex	= 1;
			ofn.lpstrFile		= NameBuff;
			ofn.nMaxFile		= MAXPATHL;
			ofn.lpstrFileTitle	= NULL;
			ofn.nMaxFileTitle	= 0;
			ofn.lpstrInitialDir	= IObuff;
			ofn.lpstrTitle		= "Wave File Select";
			ofn.Flags			= OFN_HIDEREADONLY | OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST;
			ofn.nFileOffset		= 0;
			ofn.nFileExtension	= 0;
			ofn.lpstrDefExt		= NULL;
			if (ver_info.dwPlatformId == VER_PLATFORM_WIN32_NT
											&& ver_info.dwMajorVersion == 3)
			{
				ofn.Flags			= OFN_HIDEREADONLY | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST | OFN_ENABLEHOOK | OFN_ENABLETEMPLATE;
				ofn.lpTemplateName	= "WAVEFILEOPENDIALOG";
			}
			else
			{
				ofn.Flags			= OFN_HIDEREADONLY | OFN_EXPLORER | OFN_NOCHANGEDIR | OFN_PATHMUSTEXIST | OFN_ENABLEHOOK | OFN_ENABLETEMPLATE;
				ofn.lpTemplateName	= "WAVEDLGEXP";
			}
			ofn.lpfnHook		= (LPOFNHOOKPROC)WaveHookProc;
			if (GetOpenFileName(&ofn))
			{
				SendDlgItemMessage(hWnd, 1000, EM_SETSEL, 0, (LPARAM)-2);
				SetDlgItemTextU8(hWnd, 1000, (char_u *)NameBuff);
			}
			break;
		case 1003:
			if (bUse)
			{
				bUse = FALSE;
				CheckDlgButton(hWnd, 1003, MF_UNCHECKED);
			}
			else
			{
				bUse = TRUE;
				CheckDlgButton(hWnd, 1003, MF_CHECKED);
			}
			break;
		}
		break;
	}
	return(FALSE);
}

/*------------------------------------------------------------------------------
 *	login dialog
 *----------------------------------------------------------------------------*/
static INT_PTR CALLBACK
CommandDialog(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	int					wmId;

	switch (uMsg) {
	case WM_INITDIALOG:
		SetDialogSystemFont(hWnd);
		SendDlgItemMessage(hWnd, 1001, EM_SETLIMITTEXT, 0, (LPARAM)sizeof(config_load));
		SendDlgItemMessage(hWnd, 1002, EM_SETLIMITTEXT, 0, (LPARAM)sizeof(config_unload));
		SetDlgItemTextU8(hWnd, 1001, (char_u *)"");
		SetDlgItemTextU8(hWnd, 1002, (char_u *)"");
		if (GuiConfig == 0)
		{
			SendDlgItemMessage(hWnd, 1001, EM_SETREADONLY, TRUE, 0);
			SendDlgItemMessage(hWnd, 1002, EM_SETREADONLY, TRUE, 0);
		}
		else
		{
			SetDlgItemTextU8(hWnd, 1001, (char_u *)config_load);
			SetDlgItemTextU8(hWnd, 1002, (char_u *)config_unload);
		}
		return(TRUE);
	case WM_DESTROY:
		break;
	case WM_COMMAND:
		wmId = LOWORD(wParam);
		switch (wmId) {
		case IDOK:
			GetDlgItemTextU8(hWnd, 1001, (char_u *)config_load, sizeof(config_load));
			GetDlgItemTextU8(hWnd, 1002, (char_u *)config_unload, sizeof(config_unload));
			EndDialog(hWnd, 0);
			return(TRUE);
		case IDCANCEL:
			EndDialog(hWnd, 1);
			return(TRUE);
		}
		break;
	}
	return(FALSE);
}

/*------------------------------------------------------------------------------
 *	login dialog
 *----------------------------------------------------------------------------*/
static void
UnloadCommand(void)
{
	++no_wait_return;
	if (GuiConfig != 0)
		docmdline(config_unload);
	--no_wait_return;
}

/*------------------------------------------------------------------------------
 *	login dialog
 *----------------------------------------------------------------------------*/
static void
LoadCommand(void)
{
	char			*	p;
	char				load[CMDBUFFSIZE];

	++no_wait_return;
	if (GuiConfig != 0)
	{
		STRCPY(load, config_load);
		p = load;
		while (*p)
		{
			if (*p == '\\' && p[1])
			{
				if (isalpha(p[1]))
					*p = toupper(p[1]) - 0x40;
				else
					*p = p[1];
				STRCPY(p + 1, p + 2);
			}
			p++;
		}
		docmdline(load);
	}
	--no_wait_return;
}

/*------------------------------------------------------------------------------
 *	login dialog
 *----------------------------------------------------------------------------*/
void
InitCommand(void)
{
	if (config_comb && GuiConfig)
		LoadCommand();
}

/*------------------------------------------------------------------------------
 *	login dialog
 *----------------------------------------------------------------------------*/
static INT_PTR CALLBACK
LoadDialog(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
	case WM_INITDIALOG:
		SetDialogSystemFont(hWnd);
		SetWindowPos(hWnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE	| SWP_NOSIZE);
		return(TRUE);
	default:
		break;
	}
	return(FALSE);
}

/*------------------------------------------------------------------------------
 *
 *----------------------------------------------------------------------------*/
static char *
DisplayPathName(char *fname, unsigned int max)
{
	char			*	file;
	char			*	top;
	char			*	bot;
	char			*	p;
	static char			dot[] = "...";
	static char			disp[_MAX_PATH];

	memset(disp, '\0', sizeof(disp));
	if (strlen(fname) < max)
	{
		strcpy(disp, fname);
		return(disp);
	}
	file = gettail(fname);
	if (strlen(file) >= sizeof(disp))
		return(file);
	if ((strlen(file) + sizeof(dot) + 3) > max)
		strcpy(disp, file);
	else
	{
		--file;		/* path separater include */
		bot = p = fname + 3;
		while (*p && p < file)
		{
#ifdef KANJI
			if (ISkanji(*p))
				p += utf_lenat((char_u *)p, 0) - 1;
			else
#endif
			if (*p == '\\' || *p == '/' || *p == ':')
				bot =p;
			p++;
		}
		if ((strlen(bot) + sizeof(dot)) < max)
			file = bot;
		top = p = fname + 3;
		while (*p)
		{
#ifdef KANJI
			if (ISkanji(*p))
				p += utf_lenat((char_u *)p, 0) - 1;
			else
#endif
			if (*p == '\\' || *p == '/' || *p == ':')
				top = p + 1;
			p++;
			if ((strlen(file) + sizeof(dot) + (p - fname)) > max)
				break;
		}
		strncpy(disp, fname, top - fname);
		strcat(disp, dot);
		strcat(disp, file);
	}
	return(disp);
}

#ifdef USE_HISTORY
/*------------------------------------------------------------------------------
 *
 *----------------------------------------------------------------------------*/
static int
HistoryCount(void)
{
	HKEY				hKey;
	char				name[CMDBUFFSIZE];
	int					i;

	for (i = 1; i <= MAX_HISTORY; i++)
	{
		sprintf(name, "Software\\Vim\\History\\%03d", i);
		if (RegOpenKeyEx(HKEY_CURRENT_USER, name, 0,
										KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS)
			RegCloseKey(hKey);
		else
			return(i - 1);
	}
	return(MAX_HISTORY);
}

/*------------------------------------------------------------------------------
 *
 *----------------------------------------------------------------------------*/
static BOOL
HistoryGetNo(int no, char *fname, int *line)
{
	HKEY				hKey;
	DWORD				size;
	DWORD				type;
	char				name[CMDBUFFSIZE];

	sprintf(name, "Software\\Vim\\History\\%03d", no);
	if (RegOpenKeyEx(HKEY_CURRENT_USER, name, 0,
									KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS)
	{
		if (RegGetStringU8(hKey, "name", (char_u *)fname, MAXPATHL))
		{
			size = sizeof(*line);
			type = REG_DWORD;
			if (RegQueryValueEx(hKey, "line", NULL, &type,
									(BYTE *)line, &size) == ERROR_SUCCESS)
			{
				RegCloseKey(hKey);
				return(TRUE);
			}
		}
		RegCloseKey(hKey);
	}
	return(FALSE);
}

/*------------------------------------------------------------------------------
 *
 *----------------------------------------------------------------------------*/
static char *
HistoryGetMenu(int no)
{
	int					line;
	char				name[MAXPATHL];
	static char			buff[CMDBUFFSIZE];

	if (HistoryGetNo(no, name, &line))
	{
		sprintf(buff, "&%d %s", no, DisplayPathName(name, TITLE_LEN));
		return(buff);
	}
	return(NULL);
}

/*------------------------------------------------------------------------------
 *
 *----------------------------------------------------------------------------*/
static char *
HistoryGetCommand(int no)
{
	int					line;
	char				name[MAXPATHL];
	static char			buff[CMDBUFFSIZE];
	int					max;
	int					i;

	if (HistoryGetNo(no, name, &line))
	{
		while (getperm(name) == (-1))
		{
			max = HistoryCount();
			if (max != no)
			{
				for (i = no; i < max; i++)
					HistoryRename(i + 1, i);
			}
			sprintf(buff, "File \"%s\" not found.", name);
			MessageBox(hVimWnd, buff,
#ifdef KANJI
					JpVersion,
#else
					Version,
#endif
					MB_OK);
			return(NULL);
		}
		sprintf(buff, ":e +%d %s\n", line, name);
		return(buff);
	}
	return(NULL);
}

/*------------------------------------------------------------------------------
 *
 *----------------------------------------------------------------------------*/
static void
HistoryRename(int old, int new)
{
	HKEY				hoKey;
	HKEY				hnKey;
	DWORD				size;
	DWORD				type;
	DWORD				line;
	char				oname[CMDBUFFSIZE];
	char				nname[CMDBUFFSIZE];
	char				name[MAXPATHL];

	sprintf(oname, "Software\\Vim\\History\\%03d", old);
	sprintf(nname, "Software\\Vim\\History\\%03d", new);
	if (RegCreateKeyEx(HKEY_CURRENT_USER, nname, 0, NULL,
			REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hnKey, &size)
															!= ERROR_SUCCESS)
		return;
	if (RegOpenKeyEx(HKEY_CURRENT_USER, oname, 0,
									KEY_ALL_ACCESS, &hoKey) != ERROR_SUCCESS)
	{
		RegCloseKey(hnKey);
		return;
	}
	size = sizeof(name);
	type = REG_SZ;
	if (RegQueryValueEx(hoKey, NULL, NULL, &type,
								(BYTE *)name, &size) == ERROR_SUCCESS)
		RegSetValueEx(hnKey, NULL, 0, REG_SZ, (BYTE *)name, size);
	size = sizeof(name);
	type = REG_SZ;
	if (RegQueryValueEx(hoKey, "name", NULL, &type,
								(BYTE *)name, &size) == ERROR_SUCCESS)
		RegSetValueEx(hnKey, "name", 0, REG_SZ, (BYTE *)name, size);
	size = sizeof(line);
	type = REG_DWORD;
	if (RegQueryValueEx(hoKey, "line", NULL, &type,
								(BYTE *)&line, &size) == ERROR_SUCCESS)
		RegSetValueEx(hnKey, "line", 0, REG_DWORD, (BYTE *)&line, size);
	RegDeleteKey(hoKey, "name");
	RegDeleteKey(hoKey, "line");
	RegCloseKey(hoKey);
	if (RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Vim\\History", 0,
									KEY_ALL_ACCESS, &hoKey) == ERROR_SUCCESS)
	{
		sprintf(oname, "%03d", old);
		RegDeleteKey(hoKey, oname);
		RegCloseKey(hoKey);
	}
	RegCloseKey(hnKey);
}

/*------------------------------------------------------------------------------
 *
 *----------------------------------------------------------------------------*/
static int
HistoryDuplicate(char *fname)
{
	HKEY				hKey;
	DWORD				size;
	DWORD				type;
	char				name[CMDBUFFSIZE];
	int					i;

	for (i = 1; i <= MAX_HISTORY; i++)
	{
		sprintf(name, "Software\\Vim\\History\\%03d", i);
		if (RegOpenKeyEx(HKEY_CURRENT_USER, name, 0,
										KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS)
		{
			if (RegGetStringU8(hKey, "name", (char_u *)name, sizeof(name)))
			{
				if (fnamecmp(fname, name) == 0)
				{
					RegCloseKey(hKey);
					return(i);
				}
			}
			RegCloseKey(hKey);
		}
		else
			break;
	}
	return(0);
}

/*------------------------------------------------------------------------------
 *
 *----------------------------------------------------------------------------*/
static void
HistoryAppend(char *name, int line)
{
	HKEY				hKey;
	DWORD				size;
	int					max;
	int					i;
	char				date[_MAX_PATH];

	if (name == NULL)
		return;
	if ((max = HistoryDuplicate(name)) == 0)
	{
		max = HistoryCount();
		if (max < MAX_HISTORY)
			max++;
	}
	for (i = max; i > 1; i--)
		HistoryRename(i - 1, i);
	if (RegCreateKeyEx(HKEY_CURRENT_USER, "Software\\Vim\\History\\001", 0, NULL,
			REG_OPTION_NON_VOLATILE, KEY_ALL_ACCESS, NULL, &hKey, &size)
															!= ERROR_SUCCESS)
		return;
	GetDateFormat(LOCALE_USER_DEFAULT, 0, NULL, NULL, date, sizeof(date));
	strcat(date, " ");
	GetTimeFormat(LOCALE_USER_DEFAULT, 0, NULL, NULL, &date[strlen(date)], sizeof(date) - strlen(date));
	size = strlen(date) + 1;
	RegSetValueEx(hKey, NULL, 0, REG_SZ, date, size);
	/* The name is UTF-8; the registry is Unicode, so keep it lossless. */
	RegSetStringU8(hKey, "name", (char_u *)name);
	size = sizeof(line);
	RegSetValueEx(hKey, "line", 0, REG_DWORD, (BYTE *)&line, size);
	RegCloseKey(hKey);
}

/*------------------------------------------------------------------------------
 *
 *----------------------------------------------------------------------------*/
static int
HistoryGetLine(char *fname)
{
	HKEY				hKey;
	DWORD				size;
	DWORD				type;
	DWORD				line;
	char				name[CMDBUFFSIZE];
	int					i;

	if (fname == NULL)
		return(0);
	for (i = 1; i <= MAX_HISTORY; i++)
	{
		sprintf(name, "Software\\Vim\\History\\%03d", i);
		if (RegOpenKeyEx(HKEY_CURRENT_USER, name, 0,
										KEY_ALL_ACCESS, &hKey) == ERROR_SUCCESS)
		{
			if (RegGetStringU8(hKey, "name", (char_u *)name, sizeof(name)))
			{
				if (fnamecmp(fname, name) == 0)
				{
					size = sizeof(line);
					type = REG_DWORD;
					RegQueryValueEx(hKey, "line", NULL, &type, (BYTE *)&line, &size);
					RegCloseKey(hKey);
					return(line);
				}
			}
			RegCloseKey(hKey);
		}
		else
			break;
	}
	return(0);
}

/*------------------------------------------------------------------------------
 *
 *----------------------------------------------------------------------------*/
void
win_history_append(BUF *buf)
{
	WIN 	*	wp;

	if (!GuiWin || !config_hauto || config_ini)
		return;
	if (buf->b_filename == NULL || buf->b_mtime == 0)
		return;
	if (getperm(buf->b_filename) == (-1))
		return;
	for (wp = firstwin; wp != NULL; wp = wp->w_next)
	{
		if (wp->w_buffer == buf)
		{
			HistoryAppend(buf->b_filename, wp->w_cursor.lnum);
			break;
		}
	}
}

/*------------------------------------------------------------------------------
 *
 *----------------------------------------------------------------------------*/
char *
win_history_line(BUF *buf)
{
	int				line;
	static char		cmdline[128];

	if (!GuiWin || !config_history || config_ini)
		return(NULL);
	if (buf->b_filename == NULL)
		return(NULL);
	line = HistoryGetLine(buf->b_filename);
	if (line <= 1)
		return(NULL);
	sprintf(cmdline, "+%d", line - 1);
	sprintf(cmdline, ":%d", line);
	return(cmdline);
}
#endif

/*------------------------------------------------------------------------------
 *	login dialog
 *----------------------------------------------------------------------------*/
static INT_PTR CALLBACK
LineSpaceDialogEx(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	int					wmId;
	static HWND			udCWnd;
	static HWND			udLWnd;
	static HWND			udTWnd;
	static DWORD		cspace;
	static DWORD		lspace;
	static DWORD		tspace;

	switch (uMsg) {
	case WM_INITDIALOG:
		SetDialogSystemFont(hWnd);
		cspace = v_cspace;
		lspace = v_lspace;
		tspace = v_trans;
		if (pCreateUpDownControl != NULL)
		{
			udCWnd = pCreateUpDownControl(
					WS_CHILD | WS_BORDER | WS_VISIBLE | UDS_SETBUDDYINT | UDS_ALIGNRIGHT,
					132, 39, 8, 12, hWnd, 1009, hInst,
					GetDlgItem(hWnd, 1001), 10, 0, v_cspace);
			udLWnd = pCreateUpDownControl(
					WS_CHILD | WS_BORDER | WS_VISIBLE | UDS_SETBUDDYINT | UDS_ALIGNRIGHT,
					132, 39, 8, 12, hWnd, 1009, hInst,
					GetDlgItem(hWnd, 1002), 10, 0, v_lspace);
			udTWnd = pCreateUpDownControl(
					WS_CHILD | WS_BORDER | WS_VISIBLE | UDS_SETBUDDYINT | UDS_ALIGNRIGHT,
					132, 39, 8, 12, hWnd, 1009, hInst,
					GetDlgItem(hWnd, 1003), 100, 0, v_trans);
		}
		else
		{
			udCWnd = NULL;
			udLWnd = NULL;
			udTWnd = NULL;
			wsprintf(NameBuff, "%d", v_cspace);
			SetDlgItemTextU8(hWnd, 1001, (char_u *)NameBuff);
			wsprintf(NameBuff, "%d", v_lspace);
			SetDlgItemTextU8(hWnd, 1002, (char_u *)NameBuff);
			wsprintf(NameBuff, "%d", v_trans);
			SetDlgItemTextU8(hWnd, 1003, (char_u *)NameBuff);
		}
		return(TRUE);
	case WM_DESTROY:
		break;
	case WM_VSCROLL:
		if (udCWnd == (HWND)lParam)
		{
			v_cspace = GetDlgItemInt(hWnd, 1001, NULL, FALSE);
			ResetScreen(hVimWnd);
			mch_set_winsize();
			updateScreen(CLEAR);
			return 1;
		}
		if (udLWnd == (HWND)lParam)
		{
			v_lspace = GetDlgItemInt(hWnd, 1002, NULL, FALSE);
			ResetScreen(hVimWnd);
			mch_set_winsize();
			updateScreen(CLEAR);
			return 1;
		}
		if (udTWnd == (HWND)lParam)
		{
			v_trans = GetDlgItemInt(hWnd, 1003, NULL, FALSE);
			ResetScreen(hVimWnd);
			mch_set_winsize();
			updateScreen(CLEAR);
			SetLayerd();
			return 1;
		}
		break;
	case WM_COMMAND:
		wmId = LOWORD(wParam);
		switch (wmId) {
		case IDOK:
			v_cspace = GetDlgItemInt(hWnd, 1001, NULL, FALSE);
			if (v_cspace > 10)
				v_cspace = 10;
			v_lspace = GetDlgItemInt(hWnd, 1002, NULL, FALSE);
			if (v_lspace > 10)
				v_lspace = 10;
			v_trans = GetDlgItemInt(hWnd, 1003, NULL, FALSE);
			if (v_trans > 100)
				v_trans = 100;
			SetLayerd();
			EndDialog(hWnd, 0);
			return(TRUE);
		case IDCANCEL:
			v_cspace = cspace;
			v_lspace = lspace;
			v_trans  = tspace;
			SetLayerd();
			EndDialog(hWnd, 1);
			return(TRUE);
		}
		break;
	}
	return(FALSE);
}

/*------------------------------------------------------------------------------
 *	login dialog
 *----------------------------------------------------------------------------*/
static INT_PTR CALLBACK
LineSpaceDialog(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	int					wmId;
	static HWND			udCWnd;
	static HWND			udLWnd;
	static DWORD		cspace;
	static DWORD		lspace;

	switch (uMsg) {
	case WM_INITDIALOG:
		SetDialogSystemFont(hWnd);
		cspace = v_cspace;
		lspace = v_lspace;
		if (pCreateUpDownControl != NULL)
		{
			udCWnd = pCreateUpDownControl(
					WS_CHILD | WS_BORDER | WS_VISIBLE | UDS_SETBUDDYINT | UDS_ALIGNRIGHT,
					132, 39, 8, 12, hWnd, 1009, hInst,
					GetDlgItem(hWnd, 1001), 10, 0, v_cspace);
			udLWnd = pCreateUpDownControl(
					WS_CHILD | WS_BORDER | WS_VISIBLE | UDS_SETBUDDYINT | UDS_ALIGNRIGHT,
					132, 39, 8, 12, hWnd, 1009, hInst,
					GetDlgItem(hWnd, 1002), 10, 0, v_lspace);
		}
		else
		{
			udCWnd = NULL;
			udLWnd = NULL;
			wsprintf(NameBuff, "%d", v_cspace);
			SetDlgItemTextU8(hWnd, 1001, (char_u *)NameBuff);
			wsprintf(NameBuff, "%d", v_lspace);
			SetDlgItemTextU8(hWnd, 1002, (char_u *)NameBuff);
		}
		return(TRUE);
	case WM_DESTROY:
		break;
	case WM_VSCROLL:
		if (udCWnd == (HWND)lParam)
		{
			v_cspace = GetDlgItemInt(hWnd, 1001, NULL, FALSE);
			ResetScreen(hVimWnd);
			mch_set_winsize();
			updateScreen(CLEAR);
			return 1;
		}
		if (udLWnd == (HWND)lParam)
		{
			v_lspace = GetDlgItemInt(hWnd, 1002, NULL, FALSE);
			ResetScreen(hVimWnd);
			mch_set_winsize();
			updateScreen(CLEAR);
			return 1;
		}
		break;
	case WM_COMMAND:
		wmId = LOWORD(wParam);
		switch (wmId) {
		case IDOK:
			v_cspace = GetDlgItemInt(hWnd, 1001, NULL, FALSE);
			if (v_cspace > 10)
				v_cspace = 10;
			v_lspace = GetDlgItemInt(hWnd, 1002, NULL, FALSE);
			if (v_lspace > 10)
				v_lspace = 10;
			EndDialog(hWnd, 0);
			return(TRUE);
		case IDCANCEL:
			v_cspace = cspace;
			v_lspace = lspace;
			EndDialog(hWnd, 1);
			return(TRUE);
		}
		break;
	}
	return(FALSE);
}

static void
SetLayerd(void)
{
	static int			vtrans		= 0;

	if (pSetLayeredWindowAttributes == NULL)
		v_trans = 0;
	else
	{
		if (vtrans == v_trans)
			return;
		if (v_trans == 0)
		{
			pSetLayeredWindowAttributes(hVimWnd, 0, (BYTE)255, LWA_ALPHA);
			SetWindowLong(hVimWnd, GWL_EXSTYLE,
					GetWindowLong(hVimWnd, GWL_EXSTYLE) & ~WS_EX_LAYERED);
		}
		else
		{
			if (vtrans == 0)
			{
				SetWindowLong(hVimWnd, GWL_EXSTYLE,
						GetWindowLong(hVimWnd, GWL_EXSTYLE) | WS_EX_LAYERED);
			}
			pSetLayeredWindowAttributes(hVimWnd, 0,
					(BYTE)(((230 * (100 - v_trans)) / 100) + 25), LWA_ALPHA);
		}
		vtrans = v_trans;
		ResetScreen(hVimWnd);
	}
}

static BOOL
LoadBitmapFromBMPFile(HDC hdc, LPTSTR szFileName)
{
	HANDLE				hFile;
	DWORD				dwFileSize;
	LPVOID				pvData		= NULL;
	HGLOBAL				hGlobal;
	DWORD				dwBytesRead = 0;
	LPSTREAM			pstm		= NULL;
	LPPICTURE			gpPicture	= NULL;
	static HBITMAP		hBitmap		= NULL;
	static HDC			hDCMem		= NULL;
	static int			cxDesired	= 0;
	static int			cyDesired	= 0;
	static DWORD		bitsize		= 0;
	static DWORD		bitcenter	= -1;
	static BOOL			bh			= FALSE;
	static char			bitmap[MAXPATHL];
	static DWORD		bgcolor;
	static long			hmWidth;
	static long			hmHeight;
	static int			cx;
	static int			cy;
	double				per = 1;
	int					nWidth;
	int					nHeight;
	HBRUSH				hbrush, holdbrush;
	RECT				rcWindow;
	HBITMAP				hOldBitmap;
	HDC					hMemDC;
	RGBQUAD				rgb[256];
	LPLOGPALETTE		pLogPal;
	HPALETTE			hPalette;
	HPALETTE			hOldPalette;
	int					i;

	if ((hBitmap != NULL
				&& (cxDesired != (Columns * v_xchar) || cyDesired != (Rows * v_ychar) || config_bitsize != bitsize || config_bitcenter != bitcenter))
			|| strcmp(bitmap, szFileName) != 0
			|| bSyncPaint
			|| bgcolor != *v_bgcolor)
	{
		if (hBitmap)
			DeleteObject(hBitmap);
		hBitmap = NULL;
		if (hDCMem)
			DeleteDC(hDCMem);
		hDCMem = NULL;
		strcpy(bitmap, szFileName);
		bgcolor = *v_bgcolor;
		bitsize   = config_bitsize;
		bitcenter = config_bitcenter;
		bSyncPaint = FALSE;
	}
	if (hBitmap == NULL)
	{
		if ((hFile = CreateFile(szFileName, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL)) == INVALID_HANDLE_VALUE)
			return(FALSE);
		if ((dwFileSize = GetFileSize(hFile, NULL)) == -1)
		{
			CloseHandle(hFile);
			return(FALSE);
		}
		if ((hGlobal = GlobalAlloc(GMEM_MOVEABLE, dwFileSize)) == NULL)
		{
			CloseHandle(hFile);
			return(FALSE);
		}
		if ((pvData = GlobalLock(hGlobal)) == NULL)
		{
			GlobalUnlock(hGlobal);
			GlobalFree(hGlobal);
			CloseHandle(hFile);
			return(FALSE);
		}
		if (!ReadFile(hFile, pvData, dwFileSize, &dwBytesRead, NULL))
		{
			GlobalUnlock(hGlobal);
			GlobalFree(hGlobal);
			CloseHandle(hFile);
			return(FALSE);
		}
		GlobalUnlock(hGlobal);
		CloseHandle(hFile);
		if (CreateStreamOnHGlobal(hGlobal, TRUE, &pstm))
		{
			GlobalFree(hGlobal);
			return(FALSE);
		}
		if (OleLoadPicture(pstm, dwFileSize, FALSE, (REFIID)&IID_IPicture, (LPVOID *)&gpPicture))
		{
			GlobalFree(hGlobal);
			pstm->lpVtbl->Release(pstm);
			return(FALSE);
		}
		pstm->lpVtbl->Release(pstm);
		gpPicture->lpVtbl->get_Width(gpPicture, &hmWidth);
		gpPicture->lpVtbl->get_Height(gpPicture, &hmHeight);

		cxDesired = Columns * v_xchar;
		cyDesired = Rows * v_ychar;

		if (hmWidth > cxDesired || hmHeight > cyDesired)
		{
			if (((double)hmWidth / (double)cxDesired)
								> ((double)hmHeight / (double)cyDesired))
			{
				per = 1.0 / ((double)hmWidth / (double)cxDesired);
				bh = FALSE;
			}
			else
			{
				per = 1.0 / ((double)hmHeight / (double)cyDesired);
				bh = TRUE;
			}
		}
		else
		{
			if (((double)cxDesired / (double)hmWidth)
								< ((double)cyDesired / (double)hmHeight))
			{
				per = (double)cxDesired / (double)hmWidth;
				bh = FALSE;
			}
			else
			{
				per = (double)cyDesired / (double)hmHeight;
				bh = TRUE;
			}
		}
		cx = cxDesired;
		cy = cyDesired;
		if (bh)
			cx = (int)(hmWidth * per);
		else
			cy = (int)(hmHeight * per);

		hMemDC = CreateCompatibleDC(NULL);
		GetDIBColorTable(hMemDC, 0, 256, rgb);
		pLogPal = malloc(sizeof(LOGPALETTE) + (256 * sizeof(PALETTEENTRY)));
		pLogPal->palVersion = 0x300;
		pLogPal->palNumEntries = 256;
		for (i = 0; i < 256; i++)
		{
			pLogPal->palPalEntry[i].peRed	= rgb[i].rgbRed;
			pLogPal->palPalEntry[i].peGreen	= rgb[i].rgbGreen;
			pLogPal->palPalEntry[i].peBlue	= rgb[i].rgbBlue;
			pLogPal->palPalEntry[i].peFlags	= 0;
		}
		hPalette = CreatePalette(pLogPal);
		free(pLogPal);
		DeleteDC(hMemDC);

		GetWindowRect(hVimWnd, &rcWindow);
		hbrush	= CreateSolidBrush(*v_bgcolor);
		holdbrush = SelectObject(hdc, hbrush);
		rcWindow.left = 0;
		rcWindow.top = 0;
		rcWindow.right = cxDesired;
		rcWindow.bottom = cyDesired;
		FillRect(hdc, &rcWindow, hbrush);
		SelectObject(hdc, holdbrush);
		DeleteObject(hbrush);
		// convert himetric to pixels
		nWidth	= MulDiv(hmWidth, GetDeviceCaps(hdc, LOGPIXELSX), HIMETRIC_INCH);
		nHeight	= MulDiv(hmHeight, GetDeviceCaps(hdc, LOGPIXELSY), HIMETRIC_INCH);
		if (config_bitsize != 100)
		{
			cx = (cx * config_bitsize) / 100;
			cy = (cy * config_bitsize) / 100;
		}
		gpPicture->lpVtbl->Render(gpPicture, hdc,
				config_bitcenter ? (cxDesired - cx) / 2 : cxDesired - cx,
				config_bitcenter ? (cyDesired - cy) / 2 : 0,
				cx, cy, 0, hmHeight, hmWidth, -hmHeight, &rcWindow);
		hBitmap		= CreateCompatibleBitmap(hdc, cxDesired, cyDesired);
		hDCMem		= CreateCompatibleDC(hdc);
		hOldBitmap	= SelectObject(hDCMem, hBitmap);
		hOldPalette	= SelectPalette(hdc, hPalette, FALSE);
		RealizePalette(hdc);
		BitBlt(hDCMem, 0, 0, cxDesired, cyDesired, hdc, 0, 0, SRCCOPY);
		SelectObject(hDCMem, hOldBitmap);
		SelectPalette(hdc, hOldPalette, FALSE);
		DeleteObject(hPalette);
		gpPicture->lpVtbl->Release(gpPicture);
	}
	else if (!hBitmap)
		return(FALSE);
	hMemDC		= CreateCompatibleDC(hdc);
	hOldBitmap	= SelectObject(hMemDC, hBitmap);
	BitBlt(hdc, 0, 0, cxDesired, cyDesired, hMemDC, 0, 0, SRCCOPY);
	SelectObject(hMemDC, hOldBitmap);
	DeleteDC(hMemDC);
	return(TRUE);
}

static BOOL
isbitmap(LPTSTR szFileName, HWND hwnd)
{
	HANDLE				hFile;
	DWORD				dwFileSize;
	LPVOID				pvData		= NULL;
	HGLOBAL				hGlobal;
	DWORD				dwBytesRead = 0;
	LPSTREAM			pstm		= NULL;
	static LPPICTURE	gpPicture	= NULL;

	if ((hFile = CreateFile(szFileName, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL)) == INVALID_HANDLE_VALUE)
		return(FALSE);
	if ((dwFileSize = GetFileSize(hFile, NULL)) == -1)
	{
		CloseHandle(hFile);
		return(FALSE);
	}
	if ((hGlobal = GlobalAlloc(GMEM_MOVEABLE, dwFileSize)) == NULL)
	{
		CloseHandle(hFile);
		return(FALSE);
	}
	if ((pvData = GlobalLock(hGlobal)) == NULL)
	{
		GlobalUnlock(hGlobal);
		GlobalFree(hGlobal);
		CloseHandle(hFile);
		return(FALSE);
	}
	if (!ReadFile(hFile, pvData, dwFileSize, &dwBytesRead, NULL))
	{
		GlobalUnlock(hGlobal);
		GlobalFree(hGlobal);
		CloseHandle(hFile);
		return(FALSE);
	}
	GlobalUnlock(hGlobal);
	CloseHandle(hFile);
	if (CreateStreamOnHGlobal(hGlobal, TRUE, &pstm))
	{
		GlobalFree(hGlobal);
		return(FALSE);
	}
	if (OleLoadPicture(pstm, dwFileSize, FALSE, (REFIID)&IID_IPicture, (LPVOID *)&gpPicture))
	{
		GlobalFree(hGlobal);
		pstm->lpVtbl->Release(pstm);
		return(FALSE);
	}
	if (hwnd != NULL)
	{
		long				hmWidth;
		long				hmHeight;
		RECT				rcWindow;
		HDC					hdc;

		GetClientRect(hwnd, &rcWindow);
		hdc = GetDC(hwnd);
		gpPicture->lpVtbl->get_Width(gpPicture, &hmWidth);
		gpPicture->lpVtbl->get_Height(gpPicture, &hmHeight);
		gpPicture->lpVtbl->Render(gpPicture, hdc, 0, 0, rcWindow.right, rcWindow.bottom, 0, hmHeight, hmWidth, -hmHeight, &rcWindow);
		ReleaseDC(hwnd, hdc);
	}
	gpPicture->lpVtbl->Release(gpPicture);
	pstm->lpVtbl->Release(pstm);
	return(TRUE);
}

/*------------------------------------------------------------------------------
 *  quit confirmation dialog
 *----------------------------------------------------------------------------*/
static INT_PTR CALLBACK
QuitConfirmDialog(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	int		wmId;

	switch (uMsg) {
	case WM_INITDIALOG:
		SetDialogSystemFont(hWnd);
#ifdef KANJI
		SetDlgItemTextU8(hWnd, 1000, (char_u *)"保存されていない変更があります。\r\n終了方法を選択してください。");
		SetDlgItemTextU8(hWnd, IDYES, (char_u *)"保存して終了");
		SetDlgItemTextU8(hWnd, IDNO, (char_u *)"保存せず終了");
		SetDlgItemTextU8(hWnd, IDCANCEL, (char_u *)"キャンセル");
#else
		SetDlgItemTextU8(hWnd, 1000, (char_u *)"There are unsaved changes.\r\nChoose how to quit.");
		SetDlgItemTextU8(hWnd, IDYES, (char_u *)"Save and Quit");
		SetDlgItemTextU8(hWnd, IDNO, (char_u *)"Discard and Quit");
		SetDlgItemTextU8(hWnd, IDCANCEL, (char_u *)"Cancel");
#endif
		return TRUE;
	case WM_COMMAND:
		wmId = LOWORD(wParam);
		switch (wmId) {
		case IDYES:
		case IDNO:
		case IDCANCEL:
			EndDialog(hWnd, wmId);
			return TRUE;
		}
		break;
	}
	return FALSE;
}

#ifdef KANJI
/*------------------------------------------------------------------------------
 *
 *----------------------------------------------------------------------------*/
static INT_PTR CALLBACK
FontDialogProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	int					wmId;
	static LOGFONTW		logfont;
	static LOGFONTW		jlogfont;
	LOGFONTW			work;

	switch (uMsg) {
	case WM_INITDIALOG:
		SetDialogSystemFont(hWnd);
		memcpy(&logfont, &config_font, sizeof(logfont));
		memcpy(&jlogfont, &config_jfont, sizeof(jlogfont));
		SetDlgItemTextW(hWnd, 2000, config_font.lfFaceName);
		SetDlgItemTextW(hWnd, 4000, config_jfont.lfFaceName);
		return(TRUE);
	case WM_DESTROY:
		break;
	case WM_COMMAND:
		wmId = LOWORD(wParam);
		switch (wmId) {
		case IDOK:
			memcpy(&config_font, &logfont, sizeof(config_font));
			memcpy(&config_jfont, &jlogfont, sizeof(config_jfont));
			EndDialog(hWnd, 0);
			return(TRUE);
		case IDCANCEL:
			EndDialog(hWnd, 1);
			return(TRUE);
		case 1001:
		case 3001:
			if (wmId == 1001)
				memcpy(&work, &logfont, sizeof(work));
			else
				memcpy(&work, &jlogfont, sizeof(work));
			if (ChooseFontJ(hWnd, &work))
			{
				if (wmId == 1001)
					memcpy(&logfont, &work, sizeof(work));
				else
					memcpy(&jlogfont, &work, sizeof(work));
				SetDlgItemTextW(hWnd, 2000, logfont.lfFaceName);
				SetDlgItemTextW(hWnd, 4000, jlogfont.lfFaceName);
			}
			break;
		}
		break;
	}
	return(FALSE);
}
#endif

	int
mbox(char_u *s, ...)
{
	char		buf[256];
	va_list		ap;

	va_start(ap, s);
	wvsprintf(buf, s, ap);
	va_end(ap);
	MessageBox(NULL, buf, "Debug", MB_OK);
	return(0);
}

	int
debugf(char_u *s, ...)
{
	char		buf[512];
	va_list		ap;
	static FILE *fp = NULL;

	if (fp == NULL)
	{
		if ((fp = fopen("d:\\tmp\\vim.log", "w")) == NULL)
			return 0;
	}
	GetTimeFormat(LOCALE_USER_DEFAULT, 0, NULL, NULL, buf, sizeof(buf));
	wsprintf(&buf[strlen(buf)], ":%08x: ", GetTickCount());
	va_start(ap, s);
	wvsprintf(&buf[strlen(buf)], s, ap);
	va_end(ap);
	fprintf(fp, buf);
	fflush(fp);
	return(0);
}
