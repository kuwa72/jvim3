/* vi:ts=4:sw=4
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *
 * Read the file "credits.txt" for a list of people who contributed.
 * Read the file "uganda.txt" for copying and usage conditions.
 */

/*
 * misccmds.c: functions that didn't seem to fit elsewhere
 */

#include "vim.h"
#include "globals.h"
#include "proto.h"
#include "param.h"
#ifdef KANJI
#include "kanji.h"
#endif

static void check_status __ARGS((BUF *));

#if !defined(HAVE_MKSTEMP) && !defined(LATTICE) && !defined(NT)
extern char *mktemp __ARGS((char *));	/* for vim_mktemp() at the end */
#endif

/*
 * Check if the word at 'p' of length 'len' matches a word in 'p_cinwords'
 */
	static int
in_cinwords(char_u *p, int len)
{
	char_u	*cw;
	char_u	*end;

	if (p_cinwords == NULL || *p_cinwords == NUL || len <= 0)
		return FALSE;

	for (cw = p_cinwords; *cw != NUL; )
	{
		while (*cw == ' ' || *cw == ',')
			cw++;
		if (*cw == NUL)
			break;
		end = cw;
		while (*end != NUL && *end != ' ' && *end != ',')
			end++;
		if ((end - cw) == len && STRNCMP(p, cw, (size_t)len) == 0)
			return TRUE;
		cw = end;
	}
	return FALSE;
}

/*
 * Check if a word represents a block closing keyword (like end, fi, done, esac)
 */
	static int
is_block_closer(char_u *p)
{
	static const char *closers[] = {"end", "fi", "done", "esac", "elseif", "else", "catch", "finally", "except"};
	int		i;
	int		len;
	char_u	*pp;

	for (pp = p; islower(*pp); ++pp)
		;
	if (isidchar(*pp))
		return FALSE;
	len = (int)(pp - p);
	for (i = (int)(sizeof(closers)/sizeof(closers[0])); --i >= 0; )
	{
		if (STRLEN(closers[i]) == (size_t)len && STRNCMP(p, closers[i], (size_t)len) == 0)
			return TRUE;
	}
	return FALSE;
}


/*
 * count the size of the indent in the current line
 */
	int
get_indent(void)
{
	char_u *ptr;
	int count = 0;

	for (ptr = ml_get(curwin->w_cursor.lnum); *ptr; ++ptr)
	{
		if (*ptr == TAB)	/* count a tab for what it is worth */
			count += (int)curbuf->b_p_ts - (count % (int)curbuf->b_p_ts);
		else if (*ptr == ' ')
			++count;			/* count a space for one */
		else
			break;
	}
	return (count);
}

/*
 * set the indent of the current line
 * leaves the cursor on the first non-blank in the line
 */
	void
set_indent(int size, int delete)
{
	int				oldstate = State;
	char_u		   *line;
	char_u		   *newline;
	int				ind = 0;			/* bytes of old indent to drop */
	int				ntabs = 0;
	int				nspaces;
	long_u			taillen;

	State = INSERT;		/* don't want REPLACE for State */
	curwin->w_cursor.col = 0;

	/*
	 * The whole indent is built here and put in place with one ml_replace().
	 * This used to call inschar() once per character of the new indent and
	 * delchar() once per character of the old one, and each of those rewrites
	 * the line, so the work was quadratic in the size of the indent: ":set
	 * sw=1000000" then ">>" took 0.85 seconds and ten million did not finish at
	 * all. Neither loop called breakcheck(), so it could not be interrupted
	 * either, which is what made a mistyped ":set sw=" a lock-up rather than a
	 * wait.
	 */
	if (size < 0)			/* nothing asks for this, and it used to spin */
		size = 0;

	line = ml_get(curwin->w_cursor.lnum);
	if (delete)							/* drop the old indent */
		while (iswhite(line[ind]))
			++ind;

	if (!curbuf->b_p_et)			/* if 'expandtab' is set, don't use TABs */
	{
		ntabs = size / (int)curbuf->b_p_ts;
		size -= ntabs * (int)curbuf->b_p_ts;
	}
	nspaces = size;

	taillen = (long_u)STRLEN(line + ind);
	newline = lalloc((long_u)ntabs + (long_u)nspaces + taillen + 1, TRUE);
	if (newline == NULL)
	{
		State = oldstate;
		return;
	}
	if (ntabs > 0)
		memset((char *)newline, TAB, (size_t)ntabs);
	if (nspaces > 0)
		memset((char *)newline + ntabs, ' ', (size_t)nspaces);
	memmove((char *)newline + ntabs + nspaces, (char *)line + ind,
													(size_t)taillen + 1);

#ifdef USE_SYNTAX
	if (SYN_ON(curwin))
		syn_inschar(line, 0);
#endif
	ml_replace(curwin->w_cursor.lnum, newline, FALSE);
	/*
	 * With 'revins' inschar() leaves the cursor where it is, so this did too;
	 * otherwise it ends up on the first non-blank, which is what the comment
	 * above this function has always promised.
	 */
	if (!p_ri)
		curwin->w_cursor.col = (colnr_t)(ntabs + nspaces);
#ifdef USE_SYNTAX
	if (SYN_ON(curwin))
		syn_inschar(newline, 0);
#endif
	CHANGED;
	State = oldstate;
}

/*
 * Opencmd
 *
 * Add a blank line below or above the current line.
 *
 * Return TRUE for success, FALSE for failure
 */

	int
Opencmd(int dir, int redraw, int delspaces)
{
	char_u   *ptr, *p_extra;
	FPOS	old_cursor; 			/* old cursor position */
	int		newcol = 0;			/* new cursor column */
	int 	newindent = 0;		/* auto-indent of the new line */
	int		n;
	int		truncate = FALSE;	/* truncate current line afterwards */
	int		no_si = FALSE;		/* reset did_si afterwards */
	int		retval = FALSE;		/* return value, default is FAIL */

	ptr = strsave(ml_get(curwin->w_cursor.lnum));
	if (ptr == NULL)			/* out of memory! */
		return FALSE;

	u_clearline();				/* cannot do "U" command when adding lines */
	did_si = FALSE;
	if (curbuf->b_p_ai || curbuf->b_p_si)
	{
		/*
		 * count white space on current line
		 */
		newindent = get_indent();
		if (newindent == 0)
			newindent = old_indent;		/* for ^^D command in insert mode */
		old_indent = 0;

			/*
			 * If we just did an auto-indent, then we didn't type anything on the
			 * prior line, and it should be truncated.
			 */
		if (dir == FORWARD && did_ai)
			truncate = TRUE;
		else if (curbuf->b_p_si && *ptr != NUL)
		{
			char_u	*p;
			char_u	*pp;
			int		save;


			if (dir == FORWARD)
			{
				p = ptr + STRLEN(ptr) - 1;
#ifdef KANJI
				while (p > ptr)
				{
					switch (ISkanjiPointer(ptr,p))
					{
						case 0:
							if (!isspace(*p))
								goto end_while;
							p--;
							break;
						case 2:
							p -= 2;
							break;
						/* ??? */
						case 1:
							p--;
							break;
					}
				}
end_while:
				if (ISkanjiPointer(ptr,p) == 0 && (*p == '{' || *p == ':'))	/* line ends in '{' or ':': do indent */
#else
				while (p > ptr && isspace(*p))	/* find last non-blank in line */
					--p;
				if (*p == '{' || *p == ':')		/* line ends in '{' or ':': do indent */
#endif
				{
					did_si = TRUE;
					no_si = TRUE;
				}
				else							/* look for "if" and the like */
				{
					p = ptr;
					skipspace(&p);
					for (pp = p; islower(*pp); ++pp)
						;
					if (!isidchar(*pp))			/* careful for vars starting with "if" */
					{
						save = *pp;
						*pp = NUL;
						if (in_cinwords(p, (int)(pp - p)))
							did_si = TRUE;
						*pp = save;
					}
				}
			}
			else
			{
				p = ptr;
				skipspace(&p);
				if (*p == '}' || is_block_closer(p))	/* if line starts with '}' or block closer: do indent */
					did_si = TRUE;
			}
		}
		did_ai = TRUE;

		if (curbuf->b_p_si)
			can_si = TRUE;
	}
	if (State == INSERT || State == REPLACE)	/* only when dir == FORWARD */
	{
		p_extra = ptr + curwin->w_cursor.col;
		if (curbuf->b_p_ai && delspaces)
			skipspace(&p_extra);
		if (*p_extra != NUL)
			did_ai = FALSE; 		/* append some text, don't trucate now */
	}
	else
		p_extra = (char_u *)"";				/* append empty line */

	old_cursor = curwin->w_cursor;
	if (dir == BACKWARD)
		--curwin->w_cursor.lnum;
	if (ml_append(curwin->w_cursor.lnum, p_extra, (colnr_t)0, FALSE) == FAIL)
		goto theend;
	mark_adjust(curwin->w_cursor.lnum + 1, MAXLNUM, 1L);
	if (newindent || did_si)
	{
		++curwin->w_cursor.lnum;
		if (did_si)
		{
			if (p_sr)
				newindent -= newindent % (int)curbuf->b_p_sw;
			newindent += (int)curbuf->b_p_sw;
		}
		set_indent(newindent, FALSE);
		newcol = curwin->w_cursor.col;
		if (no_si)
			did_si = FALSE;
	}
	curwin->w_cursor = old_cursor;

	if (dir == FORWARD)
	{
		if (truncate || State == INSERT || State == REPLACE)
		{
			if (truncate)
				*ptr = NUL;
			else
				*(ptr + curwin->w_cursor.col) = NUL;	/* truncate current line at cursor */
			ml_replace(curwin->w_cursor.lnum, ptr, FALSE);
			ptr = NULL;
		}

		/*
		 * Get the cursor to the start of the line, so that 'curwin->w_row' gets
		 * set to the right physical line number for the stuff that
		 * follows...
		 */
		curwin->w_cursor.col = 0;

		if (redraw)
		{
			n = RedrawingDisabled;
			RedrawingDisabled = TRUE;
			cursupdate();				/* don't want it to update srceen */
			RedrawingDisabled = n;

			/*
			 * If we're doing an open on the last logical line, then go ahead and
			 * scroll the screen up. Otherwise, just insert a blank line at the
			 * right place. We use calls to plines() in case the cursor is
			 * resting on a long line.
			 */
			n = curwin->w_row + plines(curwin->w_cursor.lnum);
			if (n == curwin->w_height)
				scrollup(1L);
			else
				win_ins_lines(curwin, n, 1, TRUE, TRUE);
		}
		++curwin->w_cursor.lnum;	/* cursor moves down */
	}
	else if (redraw) 				/* insert physical line above current line */
		win_ins_lines(curwin, curwin->w_row, 1, TRUE, TRUE);

	curwin->w_cursor.col = newcol;
	if (redraw)
	{
		updateScreen(VALID_TO_CURSCHAR);
		cursupdate();			/* update curwin->w_row */
	}
	CHANGED;

	retval = TRUE;				/* success! */
theend:
	free(ptr);
	return retval;
}

/*
 * plines(p) - return the number of physical screen lines taken by line 'p'
 */
	int
plines(linenr_t p)
{
	return plines_win(curwin, p);
}

	int
plines_win(WIN *wp, linenr_t p)
{
	long		col = 0;
	char_u		*s;
	int			lines;

	if (!wp->w_p_wrap)
		return 1;

	s = ml_get_buf(wp->w_buffer, p, FALSE);
	if (*s == NUL)				/* empty line */
		return 1;

#ifdef KANJI
	{
		int	i = wp->w_p_nu ? 8 : 0,
			j, kanji= 0;
		for (col = i; *s != NUL; s++)
		{
			if (kanji)
			{
				kanji= 0;
				i++;
				col++;
			}
			else
			{
				if (ISkanji(*s))
				{
					kanji = 1;
					i++;
					col++;
					if (i >= Columns)
					{
						i = 1;
						col++;	/* dummy byte */
					}
					continue;
				}
				j = chartabsize(s, i);
				col += j;
				i += j;
			}
			if (i >= Columns)
				i = 0;
		}
	}
#else
	while (*s != NUL)
		col += chartabsize(s++, col);
#endif

	/*
	 * If list mode is on, then the '$' at the end of the line takes up one
	 * extra column.
	 */
#ifdef CRMARK
	if (wp->w_p_list || wp->w_p_cr)
#else
	if (wp->w_p_list)
#endif
		col += 1;

#ifndef KANJI
	/*
	 * If 'number' mode is on, add another 8.
	 */
	if (wp->w_p_nu)
		col += 8;
#endif

	lines = (col + (Columns - 1)) / Columns;
	if (lines <= wp->w_height)
		return lines;
	return (int)(wp->w_height);		/* maximum length */
}

/*
 * Count the physical lines (rows) for the lines "first" to "last" inclusive.
 */
	int
plines_m(linenr_t first, linenr_t last)
{
	return plines_m_win(curwin, first, last);
}

	int
plines_m_win(WIN *wp, linenr_t first, linenr_t last)
{
	int count = 0;

	while (first <= last)
		count += plines_win(wp, first++);
	return (count);
}

/*
 * Insert, or in REPLACE state replace, one character at the cursor.
 *
 * The character is given as its bytes, because in the internal UTF-8 it can be
 * one to four of them; the old (c, k) pair could only carry two. In REPLACE
 * state the whole of the old character goes, whatever its length, and with
 * 'nojreplace' the columns it occupied are kept by padding with spaces.
 */
	void
#ifdef KANJI
inschar(char_u *bytes, int nbytes)
#else
inschar(int c)
#endif
{
	char_u  *p;
	int				rir0;		/* reverse replace in column 0 */
	char_u			*new;
	char_u			*old;
	int				oldlen;
	int				extra;
	colnr_t			col = curwin->w_cursor.col;
	linenr_t		lnum = curwin->w_cursor.lnum;
#ifdef KANJI
	int				c = bytes[0];
	int				oldn = 0;	/* bytes of the character being replaced */
	int				padn = 0;	/* spaces to keep the columns lined up */
#endif

	old = ml_get(lnum);
	oldlen = STRLEN(old) + 1;
#ifdef USE_SYNTAX
	if (SYN_ON(curwin))
		syn_inschar(old, col);
#endif

	rir0 = (State == REPLACE && p_ri && col == 0);
#ifdef KANJI
	if (!rir0 && State == REPLACE && old[col] != NUL)
	{
		oldn = utf_lenat(old, (int)col);
		if (!p_jrep)
		{
			int		ow = utf_width(old + col);
			int		nw = utf_width(bytes);

			if (ow > nw)
				padn = ow - nw;
		}
		new = alloc((unsigned)(oldlen - oldn + nbytes + padn));
		if (new == NULL)
			return;
		memmove((char *)new, (char *)old, (size_t)col);
		p = new + col;
		memmove((char *)p, (char *)bytes, (size_t)nbytes);
		if (padn)
			memset((char *)p + nbytes, ' ', (size_t)padn);
		memmove((char *)p + nbytes + padn, (char *)old + col + oldn,
									(size_t)(oldlen - col - oldn));
	}
	else
	{
		extra = nbytes;
		new = alloc((unsigned)(oldlen + extra));
		if (new == NULL)
			return;
		memmove((char *)new, (char *)old, (size_t)col);
		p = new + col;
		memmove((char *)p + extra, (char *)old + col, (size_t)(oldlen - col));
		memmove((char *)p, (char *)bytes, (size_t)nbytes);
		if (rir0)
		{
			/* reverse replace in column 0: the old first character moves right
			 * and a space takes its place */
			p[nbytes] = ' ';
			extraspace = TRUE;
		}
	}
#else
	if (rir0 || State != REPLACE || *(old + col) == NUL)
		extra = 1;
	else
		extra = 0;
	new = alloc((unsigned)(oldlen + extra));
	if (new == NULL)
		return;
	memmove((char *)new, (char *)old, (size_t)col);
	p = new + col;
	memmove((char *)p + extra, (char *)old + col, (size_t)(oldlen - col));
	if (rir0)
	{
		*(p + 1) = c;			/* replace the char that was in column 0 */
		c = ' ';				/* insert a space */
		extraspace = TRUE;
	}
	*p = c;
#endif
	ml_replace(lnum, new, FALSE);

	/*
	 * If we're in insert mode and showmatch mode is set, then check for
	 * right parens and braces. If there isn't a match, then beep. If there
	 * is a match AND it's on the screen, then flash to it briefly. If it
	 * isn't on the screen, don't do anything.
	 */
	if (p_sm && State == INSERT && (c == ')' || c == '}' || c == ']'))
	{
		FPOS		   *lpos, csave;

		if ((lpos = showmatch(NUL)) == NULL)		/* no match, so beep */
			beep();
		else if (lpos->lnum >= curwin->w_topline)
		{
			updateScreen(VALID_TO_CURSCHAR); /* show the new char first */
			csave = curwin->w_cursor;
			curwin->w_cursor = *lpos; 	/* move to matching char */
			cursupdate();
			showruler(0);
			setcursor();
			cursor_on();		/* make sure that the cursor is shown */
			flushbuf();
			vim_delay();		/* brief pause */
			curwin->w_cursor = csave; 	/* restore cursor position */
			cursupdate();
		}
	}
#ifdef KANJI
	if (!p_ri)							/* normal insert: cursor right */
		curwin->w_cursor.col += nbytes + padn;
	else if (State == REPLACE && !rir0)	/* reverse replace mode: cursor left */
	{
		char_u	*base = ml_get(lnum);

		curwin->w_cursor.col = (colnr_t)(utf_prev(base,
									base + curwin->w_cursor.col) - base);
	}
#else
	if (!p_ri)							/* normal insert: cursor right */
		++curwin->w_cursor.col;
	else if (State == REPLACE && !rir0)	/* reverse replace mode: cursor left */
		--curwin->w_cursor.col;
#endif
#ifdef USE_SYNTAX
	if (SYN_ON(curwin))
		syn_inschar(new, col);
#endif
	CHANGED;
}

/*
 * insert or replace a single plain byte
 */
	void
inschar1(int c)
{
#ifdef KANJI
	char_u	b = (char_u)c;

	inschar(&b, 1);
#else
	inschar(c);
#endif
}

/*
 * insert a string at the cursor position
 */
	void
insstr(char_u *s)
{
	char_u		*old, *new;
	int			newlen = STRLEN(s);
	int			oldlen;
	colnr_t				col = curwin->w_cursor.col;
	linenr_t			lnum = curwin->w_cursor.lnum;

	old = ml_get(lnum);
	oldlen = STRLEN(old);
	new = alloc((unsigned)(oldlen + newlen + 1));
	if (new == NULL)
		return;
	memmove((char *)new, (char *)old, (size_t)col);
	memmove((char *)new + col, (char *)s, (size_t)newlen);
	memmove((char *)new + col + newlen, (char *)old + col, (size_t)(oldlen - col + 1));
	ml_replace(lnum, new, FALSE);
	curwin->w_cursor.col += newlen;
	CHANGED;
}

/*
 * delete one character under the cursor
 *
 * return FAIL for failure, OK otherwise
 */
	int
delchar(int fixpos)
{
	char_u		*old, *new;
	int			oldlen;
	linenr_t	lnum = curwin->w_cursor.lnum;
	colnr_t		col = curwin->w_cursor.col;
	int			was_alloced;

	old = ml_get(lnum);
	oldlen = STRLEN(old);
#ifdef USE_SYNTAX
	if (SYN_ON(curwin))
		syn_delchar(old, col);
#endif

	if (col >= oldlen)	/* can't do anything (happens with replace mode) */
		return FAIL;

/*
 * If the old line has been allocated the deleteion can be done in the
 * existing line. Otherwise a new line has to be allocated
 */
	was_alloced = ml_line_alloced();		/* check if old was allocated */
	if (was_alloced)
		new = old;							/* use same allocated memory */
	else
	{
		new = alloc((unsigned)oldlen);		/* need to allocated a new line */
		if (new == NULL)
			return FAIL;
		memmove((char *)new, (char *)old, (size_t)col);
	}
	memmove((char *)new + col, (char *)old + col + 1, (size_t)(oldlen - col));
	if (!was_alloced)
		ml_replace(lnum, new, FALSE);

	/*
	 * If we just took off the last character of a non-blank line, we don't
	 * want to end up positioned at the newline.
	 */
	if (fixpos && curwin->w_cursor.col > 0 && col == oldlen - 1)
#ifdef KANJI
	{
		--curwin->w_cursor.col;
		kanji_align();
	}
#else
		--curwin->w_cursor.col;
#endif

	CHANGED;
	return OK;
}

	void
dellines(long nlines, int dowindow, int undo)
{
	int 			num_plines = 0;

	if (nlines <= 0)
		return;
	/*
	 * There's no point in keeping the window updated if we're deleting more
	 * than a window's worth of lines.
	 */
	if (nlines > (curwin->w_height - curwin->w_row) && dowindow)
	{
		dowindow = FALSE;
		/* flaky way to clear rest of window */
		win_del_lines(curwin, curwin->w_row, curwin->w_height, TRUE, TRUE);
	}
	if (undo && !u_savedel(curwin->w_cursor.lnum, nlines))
		return;

	mark_adjust(curwin->w_cursor.lnum, curwin->w_cursor.lnum + nlines - 1, MAXLNUM);
	mark_adjust(curwin->w_cursor.lnum + nlines, MAXLNUM, -nlines);

	while (nlines-- > 0)
	{
		if (bufempty()) 		/* nothing to delete */
			break;

		/*
		 * Set up to delete the correct number of physical lines on the
		 * window
		 */
		if (dowindow)
			num_plines += plines(curwin->w_cursor.lnum);

		ml_delete(curwin->w_cursor.lnum);

		CHANGED;

		/* If we delete the last line in the file, stop */
		if (curwin->w_cursor.lnum > curbuf->b_ml.ml_line_count)
		{
			curwin->w_cursor.lnum = curbuf->b_ml.ml_line_count;
			break;
		}
	}
	curwin->w_cursor.col = 0;
	/*
	 * Delete the correct number of physical lines on the window
	 */
	if (dowindow && num_plines > 0)
		win_del_lines(curwin, curwin->w_row, num_plines, TRUE, TRUE);
}

	int
gchar(FPOS *pos)
{
	return (int)(*(ml_get_pos(pos)));
}

	int
gchar_cursor(void)
{
	return (int)(*(ml_get_cursor()));
}

/*
 * Write a character at the current cursor position.
 * It is directly written into the block.
 */
	void
pchar_cursor(int c)
{
	*(ml_get_buf(curbuf, curwin->w_cursor.lnum, TRUE) + curwin->w_cursor.col) = c;
}

/*
 * return TRUE if the cursor is before or on the first non-blank in the line
 */
	int
inindent(void)
{
	char_u *ptr;
	int col;

	for (col = 0, ptr = ml_get(curwin->w_cursor.lnum); iswhite(*ptr); ++col)
		++ptr;
	if (col >= curwin->w_cursor.col)
		return TRUE;
	else
		return FALSE;
}

/*
 * skipspace: skip over ' ' and '\t'.
 *
 * note: you must give a pointer to a char_u pointer!
 */
	void
skipspace(char_u **pp)
{
    char_u *p;

#ifdef KANJI
    for(p = *pp; *p; ++p)
	{
		if (*p == ' ' || *p == '\t')
			continue;
		if (ISkanji(*p) && jpcls(p) == 0)
			p += utf_len(*p) - 1;
		else
			break;
	}
#else
    for (p = *pp; *p == ' ' || *p == '\t'; ++p)	/* skip to next non-white */
    	;
#endif
    *pp = p;
}

/*
 * skiptospace: skip over text until ' ' or '\t'.
 *
 * note: you must give a pointer to a char_u pointer!
 */
	void
skiptospace(char_u **pp)
{
	char_u *p;

	for (p = *pp; *p != ' ' && *p != '\t' && *p != NUL; ++p)
#ifdef KANJI
		if (ISkanji(*p))
		{
			if (jpcls(p) == 0)
				break;
			p += utf_len(*p) - 1;
		}
#else
		;
#endif
	*pp = p;
}

/*
 * skiptodigit: skip over text until digit found
 *
 * note: you must give a pointer to a char_u pointer!
 */
	void
skiptodigit(char_u **pp)
{
	char_u *p;

	for (p = *pp; !isdigit(*p) && *p != NUL; ++p)
		;
	*pp = p;
}

/*
 * getdigits: get a number from a string and skip over it
 *
 * note: you must give a pointer to a char_u pointer!
 */

	long
getdigits(char_u **pp)
{
    char_u *p;
	long retval;

	p = *pp;
	retval = atol((char *)p);
    while (isdigit(*p))	/* skip to next non-digit */
    	++p;
    *pp = p;
	return retval;
}

	char_u *
plural(long n)
{
	static char_u buf[2] = "s";

	if (n == 1)
		return &(buf[1]);
	return &(buf[0]);
}

/*
 * set_Changed is called when something in the current buffer is changed
 */
	void
set_Changed(void)
{
	if (!curbuf->b_changed)
	{
		change_warning();
		curbuf->b_changed = TRUE;
		check_status(curbuf);
	}
}

/*
 * unset_Changed is called when the changed flag must be reset for buffer 'buf'
 */
	void
unset_Changed(BUF *buf)
{
	if (buf->b_changed)
	{
		buf->b_changed = 0;
		check_status(buf);
	}
}

/*
 * check_status: called when the status bars for the buffer 'buf'
 *				 need to be updated
 */
	static void
check_status(BUF *buf)
{
	WIN		*wp;
	int		i;

	i = 0;
	for (wp = firstwin; wp != NULL; wp = wp->w_next)
		if (wp->w_buffer == buf && wp->w_status_height)
		{
			wp->w_redr_status = TRUE;
			++i;
		}
	if (i && must_redraw < NOT_VALID)		/* redraw later */
		must_redraw = NOT_VALID;
}

/*
 * If the file is readonly, give a warning message with the first change.
 * Don't use emsg(), because it flushes the macro buffer.
 * If we have undone all changes b_changed will be FALSE, but b_did_warn
 * will be TRUE.
 */
	void
change_warning(void)
{
	if (curbuf->b_did_warn == FALSE && curbuf->b_changed == 0 && curbuf->b_p_ro)
	{
		curbuf->b_did_warn = TRUE;
		MSG("Warning: Changing a readonly file");
		sleep(1);			/* give him some time to think about it */
	}
}

/*
 * ask for a reply from the user, a 'y' or a 'n'.
 * No other characters are accepted, the message is repeated until a valid
 * reply is entered or CTRL-C is hit.
 *
 * return the 'y' or 'n'
 */
	int
ask_yesno(char_u *str)
{
	int r = ' ';

	while (r != 'y' && r != 'n')
	{
		(void)set_highlight('r');		/* same highlighting as for wait_return */
		msg_highlight = TRUE;
		smsg((char_u *)"%s (y/n)?", str);
		r = vgetc();
		if (r == Ctrl('C'))
			r = 'n';
		msg_outchar(r);		/* show what you typed */
		flushbuf();
	}
	return r;
}

	void
msgmore(long n)
{
	long pn;

	if (global_busy)		/* no messages now, wait until global is finished */
		return;

	if (n > 0)
		pn = n;
	else
		pn = -n;

	if (pn > p_report)
		smsg((char_u *)"%ld %s line%s %s", pn, n > 0 ? "more" : "fewer", plural(pn),
											got_int ? "(Interrupted)" : "");
}

/*
 * give a warning for an error
 */
	void
beep(void)
{
#ifndef notdef
	if (!Exec_reg)
#endif
	flush_buffers(FALSE);		/* flush internal buffers */
	if (p_vb)
	{
		if (T_VB && *T_VB)
		    outstr(T_VB);
		else
		{						/* very primitive visual bell */
	        MSG("    ^G");
	        MSG("     ^G");
	        MSG("    ^G ");
	        MSG("     ^G");
	        MSG("       ");
			showmode();			/* may have deleted the mode message */
		}
	}
	else
	    outchar('\007');
}

/*
 * Expand environment variable with path name.
 * "~/" is also expanded, like $HOME.
 * If anything fails no expansion is done and dst equals src.
 */
	void
expand_env(char_u *src, char_u *dst, int dstlen)
{
	char_u	*tail;
	int		c;
	char_u	*var;

	if (*src == '$' || (*src == '~' && STRCHR("/ \t\n", src[1]) != NULL))
	{
/*
 * The variable name is copied into dst temporarily, because it may be
 * a string in read-only memory.
 */
		if (*src == '$')
		{
			tail = src + 1;
			var = dst;
			c = dstlen - 1;
			while (c-- > 0 && *tail && isidchar(*tail))
				*var++ = *tail++;
			*var = NUL;
/*
 * It is possible that vimgetenv() uses IObuff for the expansion, and that the
 * 'dst' is also IObuff. This works, as long as 'var' is the first to be copied
 * to 'dst'!
 */
			var = vimgetenv(dst);
		}
		else
		{
			var = vimgetenv((char_u *)"HOME");
			tail = src + 1;
		}
		if (var && (STRLEN(var) + STRLEN(tail) + 1 < (unsigned)dstlen))
		{
			STRCPY(dst, var);
#if !defined(notdef) && defined(MSDOS)
			if (var[strlen(var) - 1] == '\\' && tail[0] == '\\')
				STRCAT(dst, &tail[1]);
			else
#endif
			STRCAT(dst, tail);
			return;
		}
	}
	STRNCPY(dst, src, (size_t)dstlen);
}

/*
 * Replace home directory by "~/"
 * If anything fails dst equals src.
 */
	void
home_replace(char_u *src, char_u *dst, int dstlen)
{
	char_u	*home;
	size_t	len;

	/*
	 * If there is no "HOME" environment variable, or when it
	 * is very short, don't replace anything.
	 */
	if ((home = vimgetenv((char_u *)"HOME")) == NULL || (len = STRLEN(home)) <= 1)
		STRNCPY(dst, src, (size_t)dstlen);
	else
	{
#ifndef notdef
		if (home[len - 1] == '\\' || home[len - 1] == '/')
			len--;
#endif
		skipspace(&src);
		while (*src && dstlen > 0)
		{
#ifndef notdef
			if (strlen(src) > len && src[len] != '\\' && src[len] != '/')
				;
			else
#endif
			if (STRNCMP(src, home, len) == 0)
			{
				src += len;
				if (--dstlen > 0)
					*dst++ = '~';
			}
#ifdef MSDOS
			else if (vim_strnicmp(src, home, len) == 0)
			{
				src += len;
				if (--dstlen > 0)
					*dst++ = '~';
			}
#endif
			while (*src && *src != ' ' && --dstlen > 0)
				*dst++ = *src++;
			while (*src == ' ' && --dstlen > 0)
				*dst++ = *src++;
		}
		*dst = NUL;
	}
}

/*
 * Compare two file names and return TRUE if they are different files.
 * For the first name environment variables are expanded
 */
	int
fullpathcmp(char_u *s1, char_u *s2)
{
#ifdef UNIX
	struct stat st1, st2;
	char_u buf1[MAXPATHL];

	expand_env(s1, buf1, MAXPATHL);
# ifdef KANJI
	if (stat(fileconvsto(buf1), &st1) == 0 && stat(fileconvsto(s2), &st2) == 0 &&
				st1.st_dev == st2.st_dev && st1.st_ino == st2.st_ino)
# else
	if (stat((char *)buf1, &st1) == 0 && stat((char *)s2, &st2) == 0 &&
				st1.st_dev == st2.st_dev && st1.st_ino == st2.st_ino)
# endif
		return FALSE;
	return TRUE;
#else
	char_u buf1[MAXPATHL];
	char_u buf2[MAXPATHL];

	expand_env(s1, buf2, MAXPATHL);
#ifdef KANJI
	if (FullName(fileconvsto(buf2), buf1, MAXPATHL) == OK && FullName(fileconvsto(s2), buf2, MAXPATHL) == OK)
#else
	if (FullName(buf2, buf1, MAXPATHL) == OK && FullName(s2, buf2, MAXPATHL) == OK)
#endif
		return STRCMP(buf1, buf2);
	/*
	 * one of the FullNames() failed, file probably doesn't exist.
	 */
	return TRUE;
#endif
}

/*
 * get the tail of a path: the file name.
 */
	char_u *
gettail(char_u *fname)
{
	char_u *p1, *p2;

	for (p1 = p2 = fname; *p2; ++p2)	/* find last part of path */
	{
		if (ispathsep(*p2))
			p1 = p2 + 1;
#if defined(KANJI) && defined(MSDOS)
		if (ISkanji(*p2))
			p2++;
#endif
	}
	return p1;
}

/*
 * return TRUE if 'c' is a path separator.
 */
	int
ispathsep(int c)
{
#ifdef UNIX
	return (c == PATHSEP);		/* UNIX has ':' inside file names */
#else
# ifdef MSDOS
	return (c == ':' || c == PATHSEP || c == '/');
# else
	return (c == ':' || c == PATHSEP);
# endif
#endif
}

/*
 * Make the temp file whose name template is in 'name', which has to end in six
 * X's. Return OK or FAIL.
 *
 * mkstemp() creates the file itself, owned by us and 0600, so there is no gap
 * between choosing a name and opening it for somebody to drop a symlink into.
 * mktemp() only picks a name, which is why the linkers on the BSDs warn about
 * every use of it. Systems that have no mkstemp() keep what they had.
 */
	int
vim_mktemp(char_u *name)
{
#ifdef HAVE_MKSTEMP
	int		fd;

	fd = mkstemp((char *)name);
	if (fd < 0)
		return FAIL;
	/* The file is what was wanted; the callers open it again by name. */
	close(fd);
	return OK;
#else
# ifdef NT
	/*
	 * Not tmpnam(). Linked against msvcrt it returns P_tmpdir with a name after
	 * it, and P_tmpdir is "\\", so the file it names is in the root of the
	 * current drive -- "\s144k." -- which no process that is not elevated may
	 * write to on any Windows since Vista. The shell's redirection failed, and
	 * the filter read reported "Can't read file \s144k." with nothing to show
	 * that the command itself had been fine. (The UCRT tmpnam does use TEMP,
	 * which is why a UCRT build of this same source did not show it.)
	 *
	 * GetTempFileName() puts the file where TMP or TEMP says, and creates it,
	 * so it behaves like the mkstemp() branch above.
	 */
	{
		char	dir[MAXPATHL];
		char	prefix[4];
		int		i;

		if (GetTempPath(sizeof(dir), dir) == 0)
			return FAIL;
		/* The letters in front of the X's tell the input temp file from the
		 * output one; GetTempFileName() takes three of them at most. */
		for (i = 0; i < 3 && name[i] != NUL && name[i] != 'X'; i++)
			prefix[i] = name[i];
		prefix[i] = NUL;
		/* 'name' is TMPNAMELEN, which is MAXPATHL, the buffer size this wants */
		return (GetTempFileName(dir, prefix, 0, (char *)name) == 0 ? FAIL : OK);
	}
# else
#  ifdef LATTICE
	/* No mktemp() in this library, and tmpnam() makes up its own name. */
	return (*tmpnam((char *)name) == NUL ? FAIL : OK);
#  else
	return (*mktemp((char *)name) == NUL ? FAIL : OK);
#  endif
# endif
#endif
}
