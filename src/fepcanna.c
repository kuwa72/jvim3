/* canna.c */

/*
 * Author:
 *	Nobuyuki Koganemaru
 *	kogane@kces.koganemaru.co.jp
 *
 *	Jun-ichiro "itojun" Itoh/ESD
 *	itojun@foretune.co.jp / itojun@mt.cs.keio.ac.jp / JINnet itojun
 */
/*
 * JVim support
 *  Ken'ichi Tsuchida
 *  ken_t@st.rim.or.jp
 *
 */

/* canna (Japanese-conversion server) interface */

/* Known bugs:
 * - While canna fence is active, doing backspace(bs) will simply erase
 *   underlying characters, without redrawing the underlying text.
 *   It will not be a problem, but it may seem strange for users.
 * - While entering Japanese chars in vgets(), (say, while doing : or /)
 *   screen will crush if the user enters characters exceed screen width.
 *   (need to fix vgets(), so as to be able to edit line longer than
 *   screen width)
 * - While entering Japanese chars in vgets(), henkan mode will not be
 *   displayed.  Here are our future options:
 *   - to forget about mode display
 *     (current approach)
 *   - to have another status line
 *     (how can we control cursor movement?)
 *   - to have redraw facility in vgets()
 *     (unacceptable: what if there's no termcap entry available?)
 *   - to shift vgets() display area to right
 * - Support for function key -> canna control code translation is needed.
 * - Resize will not take effect, while canna fence is active.
 *   We have two problems here:
 *   - if the screen is resized while reading input from keyboard,
 *     control-L will be inserted. (see ttyread() in unix.c for detail)
 *     it may not be recognized, if canna fence is active.
 *   - if the screen is resized while canna extend-mode is active,
 *     KC_SETSIZE won't take effect until we leave canna extend-mode.
 *     thus, screen will crush if there's no enough room for guideline.
 * - There may be a chance of overwrite, over lefthalf of kanji chars.
 *   We have no way to check that.  We Hope your terminal handles it correctly.
 */

#include "vim.h"
#include "globals.h"
#include "proto.h"
#include "param.h"
#include "kanji.h"
#ifdef CANNA
#include <canna/jrkanji.h>

/* variables visible to external */
int	canna_mode = FALSE;		/* actual conversion mode */
int	canna_empty = TRUE;		/* true if canna fence is empty */

/* canna library controls */
static	jrKanjiStatus		ks;
static	jrKanjiStatusWithValue	ksv;
extern	void			(*jrBeepFunc)();
extern	char			*jrKanjiError;

/* canna inject/result buffer */
#define MAXBUF		1024
static	unsigned char	inbuf[MAXBUF];
static	unsigned char	outbuf[MAXBUF];
static	int		ohead = 0;
static	int		otail = 0;

/* internal controls */
static	int		ready = FALSE;	/* initialized? */
static	int		fence = TRUE;
static	int		errorkey;	/* illegal key was tapped */
static	char		curserver[80] = "";
					/* current canna server */
static	int		prevecholen = 0;

/* screen controls */
static	int		visualmode = FALSE;
					/* true if we can use visual screen
					 * controls.
					 */

#define	MINMODEWIDTH	10
static	char		*modeline;	/* canna modeline string */
static	int		modewidth = 0;

static	int		leftmargin = 0;

#define	GUIDEPOS	(((modewidth) < MINMODEWIDTH) ? MINMODEWIDTH : (modewidth))
#define	MAXGUIDEWIDTH	100
static	char		*guideline;	/* canna guideline string */
static	int		guidewidth = 0;
static	int		guidelen = 0;
static	int		guiderevpos = 0;
static	int		guiderevlen = 0;

/*------------------------------------------------------------*/

static int		disp_row;
static int		disp_col;
static int		redraw = FALSE;

/*------------------------------------------------------------*/

static void		canna_ulstart __PARMS((void));
static void		canna_ulstop __PARMS((void));

/*------------------------------------------------------------*/

#if 0
# define		DTRC(_1, _2, _3, _4)	cannaTrace(_1, _2, _3, _4)
#else
# define		DTRC(_1, _2, _3, _4)
#endif

/*------------------------------------------------------------*/
static
void	cannaTrace(fmt, a1, a2, a3)
	char_u	*fmt;
	int		 a1, a2, a3;
{
	static FILE		*fp = NULL;

	if (fp == NULL)
	{
		if ((fp = fopen("canna.trc", "w+")) == NULL)
			return;
		fputs("start trace\n", fp);
	}
	fprintf(fp, fmt, a1, a2, a3);
	fflush(fp);
}

static
void	canna_msg(msg, row, col)
	char_u	*msg;
	int		*row;
	int		*col;
{
	char_u	*	buf = msg;
	int			pos = 0;

	while (*msg)
	{
		screen_msg(msg, *row, *col);
		while (*msg && *col < Columns)
		{
			if (ISkanji(*msg))
			{
				/* a character is utf_lenat() bytes and utf_width()
				 * columns, not always two of each */
				int		w = utf_width(msg);

				if (w > 1 && *col + w > Columns)
				{
					screen_msg("\\", *row, *col);
					*row += 1;
					*col  = 0;
					break;
				}
				*col += w - 1;
				msg += utf_lenat(msg, 0) - 1;
			}
			*col += 1;
			++msg;
		}
		if (*col >= Columns)
		{
			*col  = 0;
			*row += 1;
		}
		if (*row >= Rows)
			break;
	}
	if (*row >= (curwin->w_winpos + curwin->w_height))
		redraw = TRUE;
	flushbuf();
}

/* Beep if an illegal key was tapped.
 * Also, the routine will update illegal keystroke flag (errorkey),
 * because canna library will not report if the injected key is legal or not.
 */
static
void	canna_beep()
{
	DTRC("@canna_beep\n", 0, 0, 0);
	errorkey = TRUE;
	beep();
}

static
int		canna_display(guide, str, len, revpos, revlen, row, col)
	int	guide;
	char	*str;
	int	len;
	int	revpos;
	int	revlen;
	int *row;
	int *col;
{
	int	i;
	int	amount;

	errorkey = TRUE;
	if (!fence && !guide)
		canna_ulstart();
	i = kanjiconvsfrom(str, revpos, IObuff, IOSIZE, NULL, JP_EUC, NULL);
	IObuff[i] = NUL;
	amount = i;
	canna_msg(IObuff, row, col);
	if (!fence && !guide)
		canna_ulstop();
	set_highlight('F');
	start_highlight();
	i = kanjiconvsfrom(&str[revpos], revlen, IObuff, IOSIZE, NULL, JP_EUC, NULL);
	IObuff[i] = NUL;
	amount += i;
	canna_msg(IObuff, row, col);
	stop_highlight();
	if (!fence && !guide)
		canna_ulstart();
	i = kanjiconvsfrom(&str[revpos + revlen], len - (revpos + revlen), IObuff, IOSIZE, NULL, JP_EUC, NULL);
	IObuff[i] = NUL;
	amount += i;
	canna_msg(IObuff, row, col);
	if (!fence && !guide)
		canna_ulstop();
	return(amount);
}

/* update modeline/guideline */
static
void	canna_showmodeline()
{
	int	i;

	if (canna_mode && visualmode)
	{
		i = kanjiconvsfrom(modeline, STRLEN(modeline), IObuff, IOSIZE, NULL, JP_EUC, NULL);
		memset(&IObuff[i], ' ', guidewidth + 2);
		IObuff[i + guidewidth] = NUL;
		disp_row = Rows - 1;
		disp_col = leftmargin;
		canna_msg(IObuff, &disp_row, &disp_col);
		disp_row = Rows - 1;
		disp_col = leftmargin + modewidth + 1;
		canna_display(TRUE, guideline, guidelen, guiderevpos, guiderevlen, &disp_row, &disp_col);
		redraw = FALSE;
	}
}

/* echoback canna fence, and update modeline/guideline if needed */
void	canna_echoback()
{
	int		needredraw;

	needredraw = FALSE;

	/* echoback canna fence */
	if (ks.length > 0)
	{
		if (visualmode)
		{
			/* Redraw the current line, and lines which
			 * have been affected by canna fence.
			 * It will be very slow, so it is optional.
			 * NOTE: I don't know why it have to be 2L.
			 * It should be 1L, I think, but it isn't.
			 */
			DTRC("@canna_echoback:1\n", 0, 0, 0);
			RedrawingDisabled++;
			cursupdate();
			RedrawingDisabled--;
			setcursor();
#if 0 /* MODOKI */
			updateline();
#else
			if (curwin->w_col + (fence ? prevecholen + 2 : prevecholen) > Columns)
			{
				DTRC("@multi-line wp=%d wr=%d wh=%d\n",
							curwin->w_winpos, curwin->w_row, curwin->w_height);
				if (curwin->w_row >= curwin->w_height - 1)
					win_redr_status(curwin);
				if (curwin->w_winpos + curwin->w_row + 1 >= Rows - 1)
				{
					DTRC("@update-status\n", 0, 0, 0);
					redrawcmdline();
					showmode();
				}
				win_update(curwin);
			}
			else
				updateline();
#endif
			disp_row = curwin->w_winpos + curwin->w_row;
			disp_col = curwin->w_col;
		}
		else
		{
			DTRC("@canna_echoback:2\n", 0, 0, 0);
			redrawcmdline();
			disp_row = msg_row;
			disp_col = msg_col;
		}
		if (fence)
			canna_msg("|", &disp_row, &disp_col);
		prevecholen = canna_display(FALSE, ks.echoStr, ks.length, ks.revPos, ks.revLen, &disp_row, &disp_col);
		DTRC("@prevecholen = %d\n", prevecholen, 0, 0);
		/* hack for pseudo-cursor at the rightmost of canna fence */
		if (ks.length == ks.revPos)
		{
			if (fence)
			{
				set_highlight('F');
				start_highlight();
				canna_msg("|", &disp_row, &disp_col);
				stop_highlight();
			}
		}
		else
		{
			if (fence)
				canna_msg("|", &disp_row, &disp_col);
		}
	}
	else
	{
		/* Advice to redraw the current line, and lines which have
		 * been affected by canna fence.
		 * We need to do it in !visualmode too, because
		 * fence may be used on screen.
		 * NOTE: I don't know why it have to be 2L.
		 * It should be 1L, I think, but it isn't.
		 */
		prevecholen = 0;
		if (visualmode && !errorkey)
		{
			DTRC("@canna_echoback:3\n", 0, 0, 0);
			updateline();
			setcursor();
			if (redraw == TRUE)
				updateScreen(CLEAR);
			redraw = FALSE;
			needredraw = TRUE;
		}
	}

	if (ks.info & KanjiModeInfo)
	{
		/* we got new modeline */
		DTRC("@canna_echoback:4\n", 0, 0, 0);
		strcpy(modeline, ks.mode);
		needredraw = TRUE;
	}
	if (ks.info & KanjiGLineInfo)
	{
		/* we got new guideline */
		DTRC("@canna_echoback:5\n", 0, 0, 0);
		memcpy(guideline, ks.gline.line, ks.gline.length);
		guidelen = ks.gline.length;
		guiderevpos = ks.gline.revPos;
		guiderevlen = ks.gline.revLen;
		needredraw = TRUE;
	}

	if (visualmode)
	{
		if (needredraw)
		{
			DTRC("@canna_echoback:6\n", 0, 0, 0);
			canna_showmodeline();
		}
	}
}

/* turn canna on/off */
static
void	canna_setmode(mode)
	int	mode;
{
	int	i;
	int	ilen;

	if (!ready)
		return;

	DTRC("@canna_setmode(%d)\n", mode, 0, 0);
	if (mode)
	{
		/* turn canna on */
		canna_mode = TRUE;

		ksv.ks = &ks;
		ksv.buffer = inbuf;
		ksv.bytes_buffer = MAXBUF;
		ksv.val = CANNA_MODE_HenkanMode;
		jrKanjiControl(0, KC_CHANGEMODE, (char *)&ksv);
		ohead = 0;
		otail = 0;

		if (visualmode)
		{
			/* initialize modeline/guideline */
			jrKanjiControl(0, KC_QUERYMODE, modeline);
			canna_showmodeline();
		}
	}
	else
	{
		/* turn canna off */
		canna_mode = FALSE;

		/* obtain fixed chars */
		ksv.ks = &ks;
		ksv.buffer = inbuf;
		ksv.bytes_buffer = MAXBUF;
		jrKanjiControl(0, KC_KAKUTEI, (char *)&ksv);
		ilen = ksv.val;
		if (ilen > 0)
		{
			otail += kanjiconvsfrom(inbuf, ilen,
							&outbuf[otail], sizeof(outbuf) - otail,
							NULL, JP_EUC, NULL);
		}

		/* fence/modeline/guideline clean-ups */
		if (visualmode)
		{
			updateScreen(INVERTED);
			showmode();
		}
	}
}

static void
canna_ulstart()
{
	set_highlight('f');
	start_highlight();
}

static void
canna_ulstop()
{
	stop_highlight();
}

/*------------------------------------------------------------*/
/*
 * internal interface for canna
 */

/* inject key stroke into canna, and fill reply buffer if needed. */
void	canna_inject(c)
	int		c;
{
	int	ilen;
	int cc = c;
	/* the bytes of a multi-byte character arrive one per call; collect
	 * the whole character -- up to UTF8_MAXLEN bytes, not the two a
	 * Shift-JIS character always was -- before echoing it to outbuf */
	static char_u	kanji[UTF8_MAXLEN];
	static int		kanji_need = 0;
	static int		kanji_have = 0;

	if (!ready)
		return;
	DTRC("@canna_inject(%d)\n", c, 0, 0);
	errorkey = FALSE;
	if (canna_mode)
	{
		if (kanji_have)
		{
			kanji[kanji_have++] = c;
			if (kanji_have == kanji_need)
			{
				int		i;

				for (i = 0; i < kanji_need; i++)
					outbuf[otail++] = kanji[i];
				kanji_have = 0;
			}
			return;
		}
		else if (ISkanji(c))
		{
			kanji[0] = c;
			kanji_need = utf_len(c);
			kanji_have = 1;
			return;
		}
		else if (ISkana(c))
		{
			outbuf[otail++] = c;
			return;
		}
		switch (c) {
		case DEL:			c = BS;						break;
		case K_UARROW:		c = CANNA_KEY_Up;			break;
		case K_DARROW:		c = CANNA_KEY_Down;			break;
		case K_LARROW:		c = CANNA_KEY_Left;			break;
		case K_RARROW:		c = CANNA_KEY_Right;		break;
		case K_SUARROW:		c = CANNA_KEY_Shift_Up;		break;
		case K_SDARROW:		c = CANNA_KEY_Shift_Down;	break;
		case K_SLARROW:		c = CANNA_KEY_Shift_Left;	break;
		case K_SRARROW:		c = CANNA_KEY_Shift_Right;	break;
		case K_HELP:		c = CANNA_KEY_Help;			break;
		case K_F1:			c = CANNA_KEY_F1;			break;
		case K_F2:			c = CANNA_KEY_F2;			break;
		case K_F3:			c = CANNA_KEY_F3;			break;
		case K_F4:			c = CANNA_KEY_F4;			break;
		case K_F5:			c = CANNA_KEY_F5;			break;
		case K_F6:			c = CANNA_KEY_F6;			break;
		case K_F7:			c = CANNA_KEY_F7;			break;
		case K_F8:			c = CANNA_KEY_F8;			break;
		case K_F9:			c = CANNA_KEY_F9;			break;
		case K_F10:			c = CANNA_KEY_F10;			break;
		case K_SF1:			c = CANNA_KEY_PF1;			break;
		case K_SF2:			c = CANNA_KEY_PF2;			break;
		case K_SF3:			c = CANNA_KEY_PF3;			break;
		case K_SF4:			c = CANNA_KEY_PF4;			break;
		case K_SF5:			c = CANNA_KEY_PF5;			break;
		case K_SF6:			c = CANNA_KEY_PF6;			break;
		case K_SF7:			c = CANNA_KEY_PF7;			break;
		case K_SF8:			c = CANNA_KEY_PF8;			break;
		case K_SF9:			c = CANNA_KEY_PF9;			break;
		case K_SF10:		c = CANNA_KEY_PF10;			break;
		}

		ilen = jrKanjiString(0, c, (char *)inbuf, MAXBUF, &ks);
		if (ilen < 0)
		{
			smsg("canna: %s", jrKanjiError);
			return;
		}

		canna_empty = (ks.info & KanjiEmptyInfo);
		if (!errorkey)
		{
			canna_echoback();
		}
		if (ilen > 0)
		{
			otail += kanjiconvsfrom(inbuf, ilen,
							&outbuf[otail], sizeof(outbuf) - otail,
							NULL, JP_EUC, NULL);
		}
		if (canna_empty && outbuf[otail == 0 ? 0 : otail - 1] != cc
				&& ((0 <= cc && cc <= 0x20) || cc == K_ZERO) && cc != ESC
				&& (p_fk != NULL && STRRCHR(p_fk, cc == K_ZERO ? '@' : cc + '@') != NULL))
			outbuf[otail++] = cc;
	}
	else
	{
		/* for safety */
		outbuf[otail++] = c;
	}
}

/* check if canna reply buffer is filled. */
int	canna_isready()
{
	return (ohead != otail);
}

/* fetch a character from canna reply buffer. */
int	canna_getkey()
{
	int	ret;

	if (!ready)
		return -1;
	if (ohead == otail)
	{
		return -1;
	}
	else
	{
		ret = outbuf[ohead++];
		if (ohead == otail)
		{
			ohead = 0;
			otail = 0;
		}
		return ret;
	}
}

/* Switch the screen control mode.
 * flag = FALSE: switch to dumb mode. (for ex mode)
 * flag = TRUE:  switch to visual mode. (for vi input mode)
 */
void	canna_visualmode(flag)
	int	flag;
{
	int	guideroom;

	if (!ready)
		return;

	visualmode = flag;
	if (visualmode)
	{
		/* Calculate the room for ruler/showmode.
		 * Refer to fillkeybuf() in tio.c for these magic numbers.
		 * NOTE: The order of the following expression is important.
		 * Smaller value should come first.
		 */
		leftmargin = guideroom = 0;
		guideroom += (p_smd) ? 13 : 0;
		guideroom += (Recording) ? 9 : 0;
		leftmargin = guideroom;
		guideroom += (p_ru) ? 17 : 0;
		/* add two for spacing */
		guideroom += 2;

		guidewidth = Columns - GUIDEPOS - guideroom;
		guidewidth = (guidewidth < 0) ? 0 : guidewidth;
		guidewidth = (guidewidth < MAXGUIDEWIDTH) ? guidewidth : MAXGUIDEWIDTH;
	}
	else
	{
		/* we have no room for guideline */
		guidewidth = 0;
	}
	jrKanjiControl(0, KC_SETWIDTH, (char *)guidewidth);
}

/* resize notification */
void	canna_resize()
{
	canna_visualmode(visualmode);
}

/* switch canna server if needed */
void	canna_setserver(name)
	char	*name;
{
	if (!ready)
		return;
	if (strcmp(curserver, name) != 0)
	{
		jrKanjiControl(0, KC_DISCONNECTSERVER, (char*)0);
		jrKanjiControl(0, KC_SETSERVERNAME, name);
		strcpy(curserver, name);
	}
}

/*------------------------------------------------------------*/
/*
 * external fepctrl-based interface for canna
 */

static	int	keepmode = FALSE;	/* canna is active in input mode?
					 * (canna will be active when fep_on())
					 */

int	canna_init()
{
	int	r;
	char	*s;

	r = jrKanjiControl(0, KC_INITIALIZE, 0);
	if (r != 0)
		return 0;

	jrKanjiControl(0, KC_SETAPPNAME, longJpVersion);

	/* modeline set-up */
	modewidth = jrKanjiControl(0, KC_QUERYMAXMODESTR, 0);
	modeline = (char *)malloc(modewidth + 1);
	modeline[0] = '\0';

	/* guideline set-up */
	guideline = (char *)malloc(MAXGUIDEWIDTH + 1);
	guideline[0] = '\0';

	/* beep if illegal key is tapped */
	jrBeepFunc = canna_beep;
	jrKanjiControl(0, KC_SETUNDEFKEYFUNCTION, kc_normal);

	/* visual mode off */
	visualmode = FALSE;
	guidewidth = 0;
	jrKanjiControl(0, KC_SETWIDTH, (char *)guidewidth);

	ready = TRUE;
	fence = TRUE;
	if (T_US != NULL && *T_US != NUL)
		fence = FALSE;

	return 1;	/*found*/
}

void	canna_term()
{
	if (!ready)
		return;
	jrKanjiControl(0, KC_FINALIZE, 0);
}

void	canna_on()
{
	if (!ready)
		return;
	if (keepmode)
		canna_setmode(TRUE);
}

void	canna_off()
{
	if (!ready)
		return;
	keepmode = canna_mode;
	canna_setmode(FALSE);
}

void	canna_force_on()
{
	if (!ready)
		return;
	canna_setmode(TRUE);
}

void	canna_force_off()
{
	if (!ready)
		return;
	canna_setmode(FALSE);
}

int		canna_get_mode()
{
	if (!ready)
		return 0;
	return canna_mode;
}

#endif	/*CANNA*/
