/* feponew.c */

/*
 * Author:
 *	  Ken'ichi Tsuchida
 *    ken_t@st.rim.or.jp
 *
 * BASE code (canna.c)
 *	  Nobuyuki Koganemaru
 *	  kogane@kces.koganemaru.co.jp
 *	  Jun-ichiro "itojun" Itoh/ESD
 *	  itojun@foretune.co.jp / itojun@mt.cs.keio.ac.jp / JINnet itojun
 */

#ifdef ONEW
#include "vim.h"
#include "globals.h"
#include "proto.h"
#include "param.h"
#include "kanji.h"

extern void		Onew_kakutei __ARGS((int));
extern int		Onew_RK_init __ARGS((void));
extern void		Onew_RK_cmode_set __ARGS((int));
extern void		Onew_KK_freqsave __ARGS((void));
extern int		Onew_RK_imode __ARGS((void));
extern int		Onew_kanakan __ARGS((int, char *, int, int));
extern int		Onew_romkan __ARGS((void));
extern int		Onew_putmode __ARGS((char *, char *));
extern int		Onew_putmsg __ARGS((int, char *, char *, char *));
extern int		ouiTrace __ARGS((char *, char *, int, int));

#if 0
# define		DTRC(_1, _2, _3, _4)	onewTrace(_1, _2, _3, _4)
#else
# define		DTRC(_1, _2, _3, _4)
#endif

static void		onewTrace __PARMS((char *, char *, int, int));
static void		onew_msg __PARMS((char_u *, int *, int *));
static void		onew_clrmsg __PARMS((void));
static void		onew_ulstart __PARMS((void));
static void		onew_ulstop __PARMS((void));

/* variables visible to external */
static int				onew_mode = FALSE;		/* actual conversion mode */
static int				onew_esc  = FALSE;		/* escape key input */

/* input controls */
#define MAXBUF			1024
static unsigned char	inbuf[MAXBUF];
static int				ilen = 0;
static unsigned char	outbuf[MAXBUF];
static int				ohead = 0;
static int				otail = 0;
static unsigned char	knjbuf[MAXBUF];
static unsigned char	wrkbuf[MAXBUF];
static unsigned char	msgbuf[MAXBUF];
static int				prev_mode;
static char			*	prev_help = NULL;
static int				kakutei_char = (-1);
static int				nouline = FALSE;

/* internal controls */
static int				ready = FALSE;	/* initialized? */

/* screen controls */
static int				visualmode = FALSE;
static int				disp_row;
static int				disp_col;
static int				mline  = FALSE;
static int				sline  = FALSE;
static int				cline  = FALSE;
static int				fence  = TRUE;
static int				o_smd;
static int				o_ru;
static int				freq_save = FALSE;

/*------------------------------------------------------------*/
int
ONEW_PEEKCHAR(timeout)
int		*	timeout;
{
	*timeout = -1;
	return(0);
}

int
ONEW_GETCHAR()
{
	/* pending EUC bytes of the last converted character; the FEP speaks
	 * EUC, the input stream is UTF-8 */
	static char_u	kanji[8];
	static int		kanji_len = 0;
	static int		kanji_i   = 0;
	char_u			k1;
	int				c;

	if (kanji_i < kanji_len)
		return(kanji[kanji_i++]);
	kanji_i = kanji_len = 0;

	k1 = vgetc() & 0xff;
	if (k1 == K_SPECIAL)
		return(0xffffff80);
	if (ISkanji(k1))
	{
		char_u		ub[UTF8_MAXLEN + 1];
		char_u		*euc;
		int			i, n = utf_len(k1);

		/* take the whole character -- 1-4 bytes, not two -- and convert it
		 * to the EUC bytes the FEP expects */
		ub[0] = k1;
		for (i = 1; i < n; i++)
			ub[i] = vgetc() & 0xff;
		ub[n] = NUL;
		if ((euc = kanjiconvsto(ub, JP_EUC, TRUE)) != NULL && euc[0] != NUL)
		{
			k1 = euc[0];
			for (i = 1; euc[i] != NUL
							&& kanji_len < (int)sizeof(kanji); i++)
				kanji[kanji_len++] = euc[i];
			free(euc);
		}
		else
			k1 = '?';
	}
	else if (ISkana(k1))
	{
		char_u	kanji2 = NUL;

		kanato(&k1, &kanji2, JP_EUC);
		if (kanji2 != NUL)
			kanji[kanji_len++] = kanji2;
	}
	c = k1;
	c &= 0xff;
	/* BOW can not input Ctrl('@'). Ctrl('Z') is not assinged edit.c */
	DTRC("@ONEW_GETCHAR", "'%c'", c, c == Ctrl('Z') ? c = NUL : 0);
	if (c == ESC)
		onew_esc = TRUE;
	else
		onew_esc = FALSE;
	if ((0 <= c && c <= 0x20)
				&& (p_fk != NULL && STRRCHR(p_fk, c + '@') != NULL))
		c = K_ZERO;
	return(c);
}

void
ONEW_GOT_2BCHAR(buf, a, b)
char	*	buf;
char		a;
char		b;
{
	DTRC("@ONEW_GOT_2BCHAR", "%c%c", a, b);
	sprintf(buf, "%c%c", a, b);
}

int
ONEW_KANAKAN(ch)
int			ch;
{
	DTRC("@ONEW_KANAKAN:S", "'%c'", ch, 0);
	if (ilen != 0)
	{
		inbuf[ilen] = NUL;
		ch = Onew_kanakan(ch, inbuf, 0, ilen);
		freq_save = TRUE;
		Onew_kakutei(ch);
		DTRC("@ONEW_KANAKAN:E", "'%c'", ch, 0);
	}
	else if (ch == ' ')
		nouline = TRUE;
	return(ch);
}

int
ONEW_KAKUTEI(ch)
int			ch;
{
	DTRC("@ONEW_KAKUTEI:S", "%c", ch, 0);
	if (0 < ilen)
	{
		if (ch == BS || ch == DEL)
		{
			knjbuf[0]	= NUL;
			kakutei_char = ch;
			return(1);
		}
		else
		{
			if (knjbuf[0])
			{
				DTRC("@ONEW_KAKUTEI:K", "[%s]", (int)knjbuf, 0);
				otail += kanjiconvsfrom(knjbuf, STRLEN(knjbuf),
								&outbuf[otail], sizeof(outbuf) - otail,
								NULL, JP_EUC, NULL);
			}
			else
			{
				DTRC("@ONEW_KAKUTEI:I", "[%s]", (int)inbuf, 0);
				otail += kanjiconvsfrom(inbuf, STRLEN(inbuf),
								&outbuf[otail], sizeof(outbuf) - otail,
								NULL, JP_EUC, NULL);
			}
			outbuf[otail] = NUL;
			knjbuf[0]	= NUL;
			ilen		= 0;
			inbuf[0]	= NUL;
			kakutei_char = ch;
			return(0);
		}
	}
	if (visualmode)
	{
		if (sline)
			win_redr_status(curwin);
		if (mline != TRUE)
			updateline();
		else
			win_update(curwin);
	}
	else
		redrawcmdline();
	DTRC("@ONEW_KAKUTEI:E", "", 0, 0);
	return(0);
}

int
ONEW_MESSAGE_COLS()
{
	return(Columns - 1);
}

void
ONEW_MESSAGE(so, form, a, b, c, d, e)
int			so;
char	*	form, *a, *b, *c, *d, *e;
{
	DTRC("@ONEW_MESSAGE", "%d %d", visualmode, onew_mode);
	if (onew_mode)
	{
		sprintf(wrkbuf, form, a, b, c, d, e);
		DTRC("@ONEW_MESSAGE", "%s", (int)wrkbuf, 0);
		kanjiconvsfrom(wrkbuf, STRLEN(wrkbuf), msgbuf, sizeof(msgbuf), NULL, JP_EUC, NULL);
		if (ISkanji(msgbuf[Columns - 2]))
			msgbuf[Columns - 2] = NUL;
		msgbuf[Columns - 1] = NUL;
		if (!p_ordw && so)
		{
			set_highlight('s');
			start_highlight();
		}
		if (visualmode)
		{
			gotocmdline(TRUE, NUL);
			outstrn(msgbuf);
		}
		else
		{
			disp_row = disp_col = 0;
			/*
			windgoto(0, 0);
			if (T_EL != NULL && *T_EL != NUL)
				outstr(T_EL);
			flushbuf();
			*/
			screen_fill(0, 1, 0, (int)Columns, ' ', ' ');
			onew_msg(msgbuf, &disp_row, &disp_col);
		}
		flushbuf();
		if (!p_ordw && so)
			stop_highlight();
		setcursor();
	}
}

void
ONEW_DISP_ROMKANMODE(mode, help, imode)
char	*	mode;
char	*	help;
int			imode;
{
	DTRC("@ONEW_DISP_ROMKANMODE", "%s,%s", (int)mode, (int)help);
#if 1
	if (onew_mode)
	{
		int		i;
		int		row;

		if (visualmode)
			row = Rows;
		else
			row = 1;
		set_highlight('s');
		start_highlight();
		if (mode == NULL)
		{
			i = kanjiconvsfrom(help, STRLEN(help), IObuff, IOSIZE, NULL, JP_EUC, NULL);
			IObuff[i] = NUL;
			for (; i < Columns; i++)
				IObuff[i] = ' ';
			if (ISkanji(IObuff[Columns - 2]))
				IObuff[Columns - 2] = NUL;
			IObuff[Columns - 1] = NUL;
			disp_row = row - 1;
			disp_col = 0;
			onew_msg(IObuff, &disp_row, &disp_col);
		}
		else if (prev_help == NULL || help == NULL
						|| strcmp(prev_help, help) != 0 || prev_mode != imode)
		{
			i  = kanjiconvsfrom(mode, STRLEN(mode), IObuff, IOSIZE, NULL, JP_EUC, NULL);
			IObuff[i] = NUL;
			i += kanjiconvsfrom(help, STRLEN(help), &IObuff[i], IOSIZE - i, NULL, JP_EUC, NULL);
			IObuff[i] = NUL;
			for (; i < Columns; i++)
				IObuff[i] = ' ';
			if (ISkanji(IObuff[Columns - 2]))
				IObuff[Columns - 2] = NUL;
			IObuff[Columns - 1] = NUL;
			disp_row = row - 1;
			disp_col = 0;
			onew_msg(IObuff, &disp_row, &disp_col);
		}
		stop_highlight();
	}
#else
	if (mode == NULL)
		Onew_putmsg(FALSE, "", NULL, NULL);
	else if (prev_help == NULL || help == NULL
						|| strcmp(prev_help, help) != 0 || prev_mode != imode)
		Onew_putmsg(FALSE, "%s %s", mode, help);
#endif
	prev_mode = imode;
	prev_help = help;
}

int
ONEW_DISP_KANAKANB(so, l, c, r)
int			so;
char	*	l;			/* left char	*/
char	*	c;			/* current char */
char	*	r;			/* right char	*/
{
	int			i;

	DTRC("@ONEW_DISP_KANAKANB", "%d[%s]", so, (int)l);
	DTRC("@ONEW_DISP_KANAKANB", "[%s][%s]", (int)c, (int)r);
	sprintf(knjbuf, "%s%s%s", l, c, r);
	/* start display */
	if (visualmode)
	{
		if (sline)
			win_redr_status(curwin);
		if (cline)
		{
			onew_clrmsg();
			Onew_putmode(NULL, NULL);
		}
		if (mline != TRUE)
			updateline();
		else
			win_update(curwin);
		disp_row = curwin->w_winpos + curwin->w_row;
		disp_col = curwin->w_col;
	}
	else
	{
		redrawcmdline();
		disp_row = msg_row;
		disp_col = msg_col;
	}
	if (fence)
		onew_msg("|", &disp_row, &disp_col);
	/* display left char */
	if (l != NULL && *l != NUL)
	{
		if (!fence)
			onew_ulstart();
		i = kanjiconvsfrom(l, STRLEN(l), wrkbuf, sizeof(wrkbuf), NULL, JP_EUC, NULL);
		wrkbuf[i] = NUL;
		onew_msg(wrkbuf, &disp_row, &disp_col);
		if (!fence)
			onew_ulstop();
	}
	/* display current char */
	if (c != NULL && *c != NUL)
	{
		set_highlight('F');
		start_highlight();
		i = kanjiconvsfrom(c, STRLEN(c), wrkbuf, sizeof(wrkbuf), NULL, JP_EUC, NULL);
		wrkbuf[i] = NUL;
		onew_msg(wrkbuf, &disp_row, &disp_col);
		stop_highlight();
	}
	/* display right char */
	if (r != NULL && *r != NUL)
	{
		if (!fence)
			onew_ulstart();
		i = kanjiconvsfrom(r, STRLEN(r), wrkbuf, sizeof(wrkbuf), NULL, JP_EUC, NULL);
		wrkbuf[i] = NUL;
		onew_msg(wrkbuf, &disp_row, &disp_col);
		if (!fence)
			onew_ulstop();
	}
	if (fence)
		onew_msg("|", &disp_row, &disp_col);
	/* windgoto(Rows - 1, Columns - 1); */
	return(0);
}

int
ONEW_DISP_KANAHALVES(str)
char	*	str;
{
	int			len	= 0;

	DTRC("@ONEW_DISP_KANAHALVES", "%s:%x", (int)str, *str);
	DTRC("@ONEW_DISP_KANAHALVES", "%x:%x", (int)onew_esc, onew_esc);
	if (onew_esc)
		return(0);
	if (*str == ' ')		/* Back Space */
	{
		if (visualmode)
		{
			if (sline)
				win_redr_status(curwin);
			if (cline)
			{
				onew_clrmsg();
				Onew_putmode(NULL, NULL);
			}
			if (mline != TRUE)
				updateline();
			else
				win_update(curwin);
		}
		else
			redrawcmdline();
	}
	if (inbuf[0] != NUL)
	{
		len = kanjiconvsfrom(inbuf, STRLEN(inbuf), wrkbuf, sizeof(wrkbuf),
															NULL, JP_EUC, NULL);
	}
	wrkbuf[len] = NUL;
	if (*str != ' ')
		strcat(wrkbuf, str);
	if (visualmode)
	{
		disp_row = curwin->w_winpos + curwin->w_row;
		disp_col = curwin->w_col;
	}
	else
	{
		disp_row = msg_row;
		disp_col = msg_col;
	}
	if (wrkbuf[0] != NUL)
	{
		if (!fence)
			onew_ulstart();
		onew_msg(wrkbuf, &disp_row, &disp_col);
		if (!fence)
			onew_ulstop();
	}
	return(0);
}

void
ONEW_BEEP()
{
	beep();
}

/*------------------------------------------------------------*/
static void
onewTrace(title, fmt, a1, a2)
char_u	*	title;
char_u	*	fmt;
int		 	a1, a2;
{
	static FILE		*fp = NULL;

	if (fp == NULL)
	{
		if ((fp = fopen("onew.trc", "w+")) == NULL)
			return;
		fputs("start trace\n", fp);
	}
	fprintf(fp, "%s ", title);
	fprintf(fp, fmt, a1, a2);
	fprintf(fp, "\n");
}

static void
onew_msg(msg, row, col)
char_u	*	msg;
int		*	row;
int		*	col;
{
	while (*msg)
	{
		screen_msg(msg, *row, *col);
		while (*msg && *col < Columns)
		{
			if (ISkanji(*msg))
			{
				if (*col == Columns - 1)
				{
					screen_msg("\\", *row, *col);
					*row += 1;
					*col  = 0;
					break;
				}
				*col += 1;
				++msg;
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
	flushbuf();
}

static void
onew_clrmsg()
{
	DTRC("@onew_clrmsg", "", 0, 0);
	prev_mode = 0;
	prev_help = NULL;
	Onew_curmsg("");
	if (visualmode)
	{
		gotocmdline(TRUE, NUL);
		windgoto(Rows - 1, 0);
		if (T_EL != NULL && *T_EL != NUL)
			outstr(T_EL);
		else
		{
			memset(wrkbuf, ' ', Columns);
			wrkbuf[Columns - 1] = NUL;
			outstr(wrkbuf);
		}
	}
	else
	{
		WIN		*	wp;
		WIN		*	twp;
		linenr_t	lnum;
		linenr_t	tlnum;

		twp = firstwin;
		while (twp != NULL)
		{
			if (twp->w_winpos == 0)
				break;
			twp = twp->w_next;
		}
		if (twp == NULL)
			return;
		tlnum = twp->w_topline;
		wp = curwin;
		lnum = curwin->w_cursor.lnum;

		win_enter(twp, TRUE);
		curwin->w_cursor.lnum = tlnum;
		windgoto(0, 0);
		if (T_EL != NULL && *T_EL != NUL)
			outstr(T_EL);
		RedrawingDisabled++;
		cursupdate();
		RedrawingDisabled--;
		setcursor();
		screen_fill(0, 1, 0, (int)Columns, ' ', ' ');
		updateline();

		win_enter(wp, TRUE);
		curwin->w_cursor.lnum = lnum;
	}
	flushbuf();
}

static void
onew_ulstart()
{
	set_highlight('f');
	start_highlight();
}

static void
onew_ulstop()
{
	stop_highlight();
}

/*------------------------------------------------------------*/
/*
 * internal interface for onew
 */

/* inject key stroke into onew, and fill reply buffer if needed. */
void
onew_inject(ch)
int			ch;
{
	int				len;
	int				row;
	int				col;
	int				i;

	DTRC("@onew_inject:S", "", 0, 0);
	if (visualmode)
	{
		if (inbuf[0] == NUL)
		{
			Onew_putmode(NULL, NULL);
			RedrawingDisabled++;
			cursupdate();
			RedrawingDisabled--;
			setcursor();
			updateline();
		}
		row = curwin->w_winpos + curwin->w_row;
		col = curwin->w_col;
	}
	else
	{
		redrawcmdline();
		row = msg_row;
		col = msg_col;
	}
	DTRC("@onew_inject:P", "%d:%d", row, col);
	if (inbuf[0] != NUL)
	{
		DTRC("@onew_inject:B", "%s", (int)inbuf, 0);
		len = kanjiconvsfrom(inbuf, STRLEN(inbuf), wrkbuf, sizeof(wrkbuf),
															NULL, JP_EUC, NULL);
		wrkbuf[len] = NUL;
		if (!fence && !nouline)
			onew_ulstart();
		onew_msg(wrkbuf, &row, &col);
		if (!fence && !nouline)
			onew_ulstop();
		DTRC("@onew_inject:X", "%d:%d", len, disp_col);
		if (visualmode)
		{
			cline = sline = mline = FALSE;
			if (row >= (curwin->w_winpos + curwin->w_height))
			{
				if (row >= Rows - 1)
					cline = TRUE;
				else
					sline = TRUE;
			}
			for (i = 0, len = 0; i < Rows; i++)
			{
				if (curwin->w_lsize_lnum[i] == curwin->w_cursor.lnum)
				{
					DTRC("@BS:1", "%d:%d", len, curwin->w_winpos);
					DTRC("@BS:2", "%d:%d", row, curwin->w_lsize[i]);
					len += curwin->w_winpos;
					if (curwin->w_lsize[i] == 0 && len == row)
						mline = FALSE;
					else if (curwin->w_lsize[i] == 0)
						mline = TRUE;
					else if (len <= row && row <= (len + curwin->w_lsize[i] -1))
						mline = FALSE;
					else
						mline = TRUE;
					DTRC("@BS:3", "%d:%d", cline, sline);
					DTRC("@BS:4", "%d:%d", mline, curwin->w_height);
					break;
				}
				else
					len += curwin->w_lsize[i];
			}
		}
	}
	nouline = FALSE;
	ch = Onew_romkan() & 0xff;
	DTRC("@onew_inject:I", "%x:%x", ch, kakutei_char);
	switch (ch) {
	case ESC:
		if (inbuf[0] != NUL)
			Onew_kakutei(ch);
		onew_clrmsg();
		outbuf[otail++]	= ch;
		outbuf[otail]	= NUL;
		break;
	case NL:
	case CR:
		if (inbuf[0] != NUL)
			Onew_kakutei(ch);
		else if (kakutei_char != ch)
		{
			outbuf[otail++]	= ch;
			if (p_ordw)
				onew_clrmsg();
		}
		outbuf[otail]	= NUL;
		break;
	case 0x80:
		if (inbuf[0] != NUL)
			Onew_kakutei(ch);
		outbuf[otail++]	= K_SPECIAL;
		outbuf[otail]	= NUL;
		break;
	case K_ZERO:
		if (inbuf[0] != NUL)
			Onew_kakutei(ch);
		onew_clrmsg();
		outbuf[otail++]	= *p_fk - '@';
		outbuf[otail]	= NUL;
		break;
	case DEL:
	case BS:
		if (kakutei_char == ch)
			break;
		if (ilen > 0)
		{
			ilen--;
			if (0x80 & inbuf[ilen])
				ilen--;
			inbuf[ilen] = NUL;
		}
		else
		{
			outbuf[otail++] = ch;
			outbuf[otail]	= NUL;
		}
		if (visualmode)
		{
			if (sline)
				win_redr_status(curwin);
			if (cline)
			{
				onew_clrmsg();
				Onew_putmode(NULL, NULL);
			}
			if (mline != TRUE)
				updateline();
			else
				win_update(curwin);
		}
		break;
	default:
		if (kakutei_char != ch)
		{
			if (ch <= ' ')
			{
				if (inbuf[0] != NUL)
					Onew_kakutei(ch);
				outbuf[otail++]	= ch;
				outbuf[otail]	= NUL;
			}
			else
			{
				if (ilen >= sizeof(inbuf)
						|| (visualmode
								&& row >= (curwin->w_winpos + curwin->w_height)
								&& col >= (Columns / 2)))
				{
					onew_clrmsg();
					beep();
					emsg("onew input buffer empty");
					if (ch & 0x80)
						Onew_romkan();
					if (cline)
						Onew_putmode(NULL, NULL);
				}
				else
				{
					inbuf[ilen++] = ch;
					if (ch & 0x80)
						inbuf[ilen++] = Onew_romkan() & 0xff;
					inbuf[ilen] = NUL;
				}
			}
		}
		break;
	}
	kakutei_char = (-1);
	DTRC("@onew_inject:E", "%x:%x", ilen, ch);
}

/* check if onew reply buffer is filled. */
int
onew_isready()
{
	return(ohead != otail);
}

/* fetch a character from onew reply buffer. */
int
onew_getkey()
{
	int			ret;

	if (!ready)
		return(-1);
	if (ohead == otail)
		return(-1);
	ret = outbuf[ohead++];
	DTRC("@onew_getkey", "%x", ret, 0);
	if (ohead == otail)
	{
		ohead = 0;
		otail = 0;
		outbuf[0] = NUL;
		DTRC("@onew_getkey:clear", "%x", ret, 0);
		if (cline)
			onew_clrmsg();
		if (sline)
			win_redr_status(curwin);
	}
	return(ret);
}

/* Switch the screen control mode.
 * flag = FALSE: switch to dumb mode. (for ex mode)
 * flag = TRUE:  switch to visual mode. (for vi input mode)
 */
void
onew_visualmode(flag)
int			flag;
{
	if (!ready)
		return;
	visualmode = flag;
}

/* resize notification */
void
onew_resize()
{
	onew_visualmode(visualmode);
}

/*------------------------------------------------------------*/
/*
 * external fepctrl-based interface for onew
 */

static	int	keepmode = FALSE;	/* onew is active in input mode?
								 * (onew will be active when fep_on())
								 */

int
onew_init()
{
	if (Onew_RK_init())
	{
		Onew_RK_cmode_set('@');
		ready = TRUE;
		fence = TRUE;
		if (T_US != NULL && *T_US != NUL)
			fence = FALSE;
	}
	DTRC("@onew_init", "%x", ready, 0);
	return(ready);
}

void
onew_term()
{
	if (!ready)
		return;
	DTRC("@onew_term", "", 0, 0);
	Onew_KK_freqsave();
	ready = FALSE;
}

void
onew_on()
{
	if (!ready)
		return;
	DTRC("@onew_on", "%x", keepmode, 0);
	if (keepmode)
		onew_force_on();
}

void
onew_off()
{
	if (!ready)
		return;
	DTRC("@onew_off", "%x", keepmode, 0);
	keepmode = onew_mode;
	onew_force_off();
}

void
onew_force_on()
{
	if (!ready || onew_mode)
		return;
	DTRC("@onew_force_on", "", 0, 0);
	onew_mode = TRUE;
	onew_esc  = FALSE;
	Onew_RK_cmode_set('h');
	o_smd		= p_smd;
	o_ru		= p_ru;
	p_smd		= FALSE;
	if (curwin->w_status_height == 0)
		p_ru	= FALSE;
}

void
onew_force_off()
{
	if (!ready || !onew_mode)
		return;
	DTRC("@onew_force_off", "", 0, 0);
	onew_mode = FALSE;
	Onew_RK_cmode_set('@');
	p_smd		= o_smd;
	p_ru		= o_ru;
	if (p_smd || p_ru)
		showmode();
	if (!visualmode)
	{
		onew_clrmsg();
		redrawcmdline();
	}
}

int
onew_get_mode()
{
	if (!ready)
		return(0);
	DTRC("@onew_get_mode", "%x", Onew_RK_imode(), 0);
	return(Onew_RK_imode());
}

/*
 * onew_freqsave();	save dictionary contents
 */
void
onew_freqsave()
{
	if (freq_save)
	{
		smsg("saving kanji frequency...");
		Onew_KK_freqsave();
		smsg("");
		freq_save = FALSE;
	}
}

#endif	/*ONEW*/
