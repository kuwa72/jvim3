/* vi:set ts=4 sw=4:
 *
 * VIM - Vi IMproved
 *
 * Code Contributions By:	Atsushi  Nakamura		anaka@mrit.mei.co.jp
 */

#ifdef TRACK
/*
 * track.c: functions for draw tracks.
 */

#include "vim.h"
#include "globals.h"
#include "proto.h"
#include "param.h"
#ifdef KANJI
#include "kanji.h"
#endif

extern char_u *get_inserted __ARGS((void));

/*
 *	directions where a line exists.
 *	1 : up
 *	2 : down
 *	4 : left
 *	8 : right
 */

#define TK_U	1
#define TK_D	2
#define TK_L	4
#define TK_R	8

struct tracktab
{
	char	*name;
	int		vw;
	char	**ch;
};

#ifdef KANJI

#define JP_TRACKTAB
#include "jptab.h"
#undef JP_TRACKTAB

#endif

static char *
tracktab_as[] =
{
	" ",  "V",  "A",  "|",  ">",  "+",  "+",  "+",
	"<",  "+",  "+",  "+",  "-",  "+",  "+",  "+"
};

static struct tracktab
tracktabs[] =
{
	{ "as",    1, tracktab_as    },
#ifdef KANJI
	{ "jp",    2, tracktab_jp    },
	{ "bj",    2, tracktab_bj    },
	{ "hj",    2, tracktab_hj    },
#endif
	{ NULL, 0, NULL }
},
*tracktab = &tracktabs[0];

static void		tracktab_sw	__ARGS((char *));
static char	*	track_vcol	__ARGS((char_u *, int, int));
static int		track_has_arc	__ARGS((char *, char **, int));
static int		track_code	__ARGS((int, int, int));
static void		track_ins	__ARGS((int));

	static void
tracktab_sw(char *tname)
{
	struct tracktab *tp;

	if (tname && !STRCMP(tracktab->name, tname))
		return;

	for(tp = &tracktabs[0]; tp->name; tp++)
		if (!STRCMP(tp->name, tname))
		{
			tracktab = tp;
			break;
		}

	if (!(tp->name))
	{
		tracktab = &tracktabs[0];
		smsg("%s unknown track character set.", tname);
	}
}

	char *
tracktab_next(char *tset)
{
	struct tracktab *tp;

	for(tp = &tracktabs[0]; tp->name; tp++)
		if (!STRCMP(tp->name, tset))
		{
			tp++;
			break;
		}
	if (!tp->name)
		tp = &tracktabs[0];

	tracktab = tp;
	return tp->name;
}

	char *
tracktab_prev(char *tset)
{
	struct tracktab *tp;

	tp = &tracktabs[0];
	if (STRCMP(tp->name, tset))
		for(tp++; tp->name; tp++)
			if (!STRCMP(tp->name, tset))
				return (tp-1)->name;

	for(tp = &tracktabs[0]; tp->name; tp++);
	return (tp-1)->name;
}

#define TV_JUST		1
#define TV_NEXT		2
#define TV_PREV		3
#define TV_FPAD		-1
#define TV_BPAD		-2

	static char *
track_vcol(char_u *line, int cvcol, int mode)
{
	int  vcol, pcol;
	char_u *ctop = line;

	vcol = pcol = 0;
	while(*line && vcol < cvcol)
	{
		pcol = vcol;
		ctop = line;
#ifdef KANJI
		if (ISkanji(*line))
		{
			vcol += 2;
			line += 2;
		}
		else
#endif
		vcol += chartabsize(line++, vcol);
	}

	switch(mode) {
	case TV_JUST:
		if (vcol != cvcol)
			return NULL;
	case TV_NEXT:
		return line;
	case TV_PREV:		/* tail byte of the previous char */
		return vcol == cvcol ? line: ctop;
	case TV_FPAD:
		if (!*line)
			return (char *)(long)(cvcol - vcol);
		return (char *)(long)((vcol == cvcol ? 0 : cvcol - pcol));
	case TV_BPAD:
		return (char *)(long)((vcol - cvcol));
	default: /* error */
		return NULL;
	}
}

	static int
track_has_arc(char *ptr, char *tc[], int dir)
{
	int i;

	for(i = 1; i < 16; i++)
		if (i & dir && !STRNCMP(ptr, tc[i], STRLEN(tc[i])))
			return TRUE;
	return FALSE;
}

	static int
track_code(int move, int vstart, int vend)
{
	int		len;
	long	i;
	char_u	*line, *ptr;
	char	**tc;
	int		code;

	line = ml_get(curwin->w_cursor.lnum);
	tc   = tracktab->ch;
	len  = tracktab->vw;
	code = move;

	/* left char. */
	if (!(code & TK_L) && ((ptr = track_vcol(line, vstart, TV_PREV)) != NULL))
	{
		if (curbuf->b_p_tt)
			while(ptr - line >= len && STRCHR(" \t", *(ptr - 1)))
				ptr --;

		if (ptr - line >= len && track_has_arc(ptr - len, tc, TK_R))
			code |= TK_L;
	}

	/* right char. */
	if (!(code & TK_R) && ((ptr = track_vcol(line, vend, TV_NEXT)) != NULL))
	{
		if (curbuf->b_p_tt)
			while(*ptr && STRCHR(" \t", *ptr))
				ptr ++;

		if (track_has_arc(ptr, tc, TK_L))
			code |= TK_R;
	}

	/* up char. */
	if (!(code & TK_U))
		for(i = curwin->w_cursor.lnum - 1; i > 0; i --)
		{
			ptr = track_vcol(ml_get(i), vstart, TV_JUST);

			if (curbuf->b_p_tt && (!ptr || (*ptr && STRCHR(" \t", *ptr))))
				continue;

			if (ptr && track_has_arc(ptr, tc, TK_D))
				code |= TK_U;

			break;
		}

	/* down char. */
	if (!(code & TK_D))
		for(i = curwin->w_cursor.lnum + 1; i <= curbuf->b_ml.ml_line_count; i++)
		{
			ptr = track_vcol(ml_get(i), vstart, TV_JUST);

			if (curbuf->b_p_tt && (!ptr || (*ptr && STRCHR(" \t", *ptr))))
				continue;

			if (ptr && track_has_arc(ptr, tc, TK_U))
				code |= TK_D;

			break;
		}

	return code;
}

	static void 
track_ins(int dir)
{
	FPOS		cpos;
	char_u	*	line;
	char_u	*	ins;
	char_u	*	nextp;
	char_u	*	prevp;
	int			ndel, fpad, bpad;
	int			vstart, vend;
	int			ralign;
	int			rvstart, rvend;
	int			rcode, lcode;
	int			oldState;

	if (curwin->w_curswant == MAXCOL)
		curwin->w_curswant = curwin->w_virtcol;

	if (!u_save(curwin->w_cursor.lnum - 1, curwin->w_cursor.lnum + 1))
		return;

	tracktab_sw(curbuf->b_p_trs);

	vstart = vend = curwin->w_curswant;
	vend += tracktab->vw;

	rvstart = rvend = curwin->w_curswant + 1;
	rvstart -= tracktab->vw;

	if (rvstart < 0)
	{
		rvstart = 0;
		rvend = tracktab->vw;
	}

	/* determine alignment */
		/* vertical matching (matched lines.) */
	rcode = track_code(0, rvstart, rvend) & (TK_U | TK_D);
	lcode = track_code(0,  vstart,  vend) & (TK_U | TK_D);

	line = ml_get(curwin->w_cursor.lnum);
	if      ((lcode && !rcode) || (lcode & (TK_U | TK_D)) == (TK_U | TK_D))
		ralign = FALSE;
	else if ((!lcode && rcode) || (rcode & (TK_U | TK_D)) == (TK_U | TK_D))
		ralign = TRUE;
	else if (lcode & dir)
		ralign = FALSE;
	else if (rcode & dir)
		ralign = TRUE;
	else
	{	/* holizontal matching (padding chars.) */
		lcode = rcode = 0;
		if (track_vcol(line, vstart,  TV_FPAD))
			lcode |= TK_L;
		if (track_vcol(line, vend,    TV_BPAD))
			lcode |= TK_R;
		if (track_vcol(line, rvstart, TV_FPAD))
			rcode |= TK_L;
		if (track_vcol(line, rvend,   TV_BPAD))
			rcode |= TK_R;
		if      (!rcode && lcode)
			ralign = TRUE;
		else if (rcode && !lcode)
			ralign = FALSE;
		else
		{
			rcode &= ~dir;
			lcode &= ~dir;
			if      (!rcode && lcode)
				ralign = TRUE;
			else if (rcode && !lcode)
				ralign = FALSE;
			else if (dir & TK_L)
				ralign = TRUE;
			else if (dir & TK_R)
				ralign = FALSE;
			else
				ralign = FALSE;
		}
	}
	/*	*/

	if (ralign)
	{
		vstart = rvstart;
		vend   = rvend;
	}

	oldState = State;
	if (State == NORMAL)
		State = INSERT;

	/* find suitable track character */
	ins = tracktab->ch[track_code(dir, vstart, vend)];

	nextp = track_vcol(line, vend,   TV_NEXT);
	prevp = track_vcol(line, vstart, TV_PREV);

	ndel = nextp - prevp;
	fpad = (long) track_vcol(line, vstart, TV_FPAD);
	bpad = (long) track_vcol(line, vend,   TV_BPAD);

	if (ralign && (curwin->w_cursor.col = prevp - line) <= 0)
		curwin->w_cursor.col = 0;

	if (- bpad >= tracktab->vw)
	{
		curwin->w_cursor.col = STRLEN(line);
		ndel = 0;
	}

#ifdef KANJI		/* quick hack */
	if (fpad == 0 && bpad == 1 && dir == TK_L && ndel == 2
								&& tracktab->vw == 1 && ISkanji(gchar_cursor()))
	{
		fpad = 1;
		bpad = 0;
	}
#endif
	/* remove old characters */
	for (;ndel > 0; ndel--)
		delchar(FALSE);

	/* padd preceeding space */
	for (; fpad > 0; fpad--)
#ifdef KANJI
		inschar1(' ');
#else
		inschar(' ');
#endif

	/* insert track */
	cpos = curwin->w_cursor;
#ifdef KANJI
	for (; *ins; ins += utf_len(*ins))
		inschar(ins, utf_lenat(ins, 0));
#else
	inschar(*ins);
#endif

	/* padd tailing space */
	for (; bpad > 0; bpad--)
#ifdef KANJI
		inschar1(' ');
#else
		inschar(' ');
#endif

	if (dir == TK_R)
	{
		line = ml_get_pos(&curwin->w_cursor);
		if (!*line)
#ifdef KANJI
			inschar1('X');
#else
			inschar('X');
#endif
	}

	/* recover saved states */
	curwin->w_cursor = cpos;
	updateline();
	State = oldState;
}

	void
track_right(void)
{
	track_ins(TK_R);
}

	void
track_left(void)
{
	track_ins(TK_L);
}

	void
track_up(void)
{
	track_ins(TK_U);
}

	void
track_down(void)
{
	track_ins(TK_D);
}

	void
showtrack(void)
{
	if (!Track)
		return;
	gotocmdline(TRUE, NUL);
	msg_outstr((char_u *)"-- TRACK[");
	msg_outstr(curbuf->b_p_trs);
	msg_outstr((char_u *)"] --");
	showruler(FALSE);
	setcursor();
}

#endif	/* TRACK */
