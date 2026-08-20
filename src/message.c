/* vi:ts=4:sw=4
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *
 * Read the file "credits.txt" for a list of people who contributed.
 * Read the file "uganda.txt" for copying and usage conditions.
 */

/*
 * message.c: functions for displaying messages on the command line
 */

#include <stdarg.h>

#include "vim.h"
#include "globals.h"
#define MESSAGE			/* don't include prototype for smsg() */
#include "proto.h"
#include "param.h"
#ifdef KANJI
#include "kanji.h"
#endif

static int msg_check_screen __ARGS((void));

static int lines_left = -1;			/* lines left for listing */

/*
 * msg(s) - displays the string 's' on the status line
 * return TRUE if wait_return not called
 */
	int
msg(char_u *s)
{
	if (!screen_valid())			/* terminal not initialized */
	{
		fprintf(stderr, "%s", (char *)s);
		fflush(stderr);
		return TRUE;
	}

	msg_start();
	if (msg_highlight)			/* actually it is highlighting instead of invert */
		start_highlight();
	msg_outtrans(s, -1);
	if (msg_highlight)
	{
		stop_highlight();
		msg_highlight = FALSE;		/* clear for next call */
	}
	msg_ceol();
	return msg_end();
}

#ifndef PROTO		/* automatic prototype generation does not understand this */
/*
 * proto.h has always declared this as smsg(char_u *, ...), but the definition
 * took eleven fixed arguments and callers passed as many as they felt like --
 * the pre-ANSI way of doing this. It survived because __PARMS() expanded to ()
 * and so nobody saw the declaration. They do now, and on a machine where the
 * variadic calling convention differs from the ordinary one -- arm64 macOS
 * passes variadic arguments on the stack, not in registers -- the callee read
 * registers the caller never filled, and printing "%s" of that crashed.
 */
	void
smsg(char_u *s, ...)
{
	va_list		ap;

	va_start(ap, s);
	vsprintf((char *)IObuff, (char *)s, ap);
	va_end(ap);
	msg(IObuff);
}
#endif

/*
 * emsg() - display an error message
 *
 * Rings the bell, if appropriate, and calls message() to do the real work
 *
 * return TRUE if wait_return not called
 */
	int
emsg(char_u *s)
{
	if (p_eb)
		beep();					/* also includes flush_buffers() */
	else
		flush_buffers(FALSE);	/* flush internal buffers */
	(void)set_highlight('e');	/* set highlight mode for error messages */
	msg_highlight = TRUE;
/*
 * Msg returns TRUE if wait_return() was not called.
 * In that case may call sleep() to give the user a chance to read the message.
 * Don't call sleep() if dont_sleep is set.
 */
	if (msg(s))
	{
		if (dont_sleep)
		{
			msg_outchar('\n');	/* one message per line, don't overwrite */
			cmdline_row = msg_row;
			need_wait_return = TRUE;
		}
		else
#ifdef notdef
			sleep(1);			/* give the user a chance to read the message */
#else
		{
			vim_delay();
			vim_delay();
		}
#endif
		return TRUE;
	}
	return FALSE;
}

	int
emsg2(char_u *s, char_u *a1)
{
	sprintf((char *)IObuff, (char *)s, (char *)a1);
	return emsg(IObuff);
}

/*
 * emsg() with one number in it, for a "%ld". Callers used to cast the number to
 * char_u * and call emsg2(), which passes a pointer where sprintf() reads a
 * long: the same width only where long is as wide as a pointer.
 */
	int
emsgn(char_u *s, long n)
{
	sprintf((char *)IObuff, (char *)s, n);
	return emsg(IObuff);
}

/*
 * wait for the user to hit a key (normally a return)
 * if 'redraw' is TRUE, clear and redraw the screen
 * if 'redraw' is FALSE, just redraw the screen
 * if 'redraw' is -1, don't redraw at all
 */
	void
wait_return(int redraw)
{
	int				c;
	int				oldState;
	int				tmpState;

/*
 * With the global command (and some others) we only need one return at the
 * end. Adjust cmdline_row to avoid the next message overwriting the last one.
 */
	if (no_wait_return)
	{
		need_wait_return = TRUE;
		cmdline_row = msg_row;
		if (!termcap_active)
			starttermcap();
		return;
	}
	need_wait_return = FALSE;
	lines_left = -1;
	oldState = State;
	State = HITRETURN;
	if (got_int)
		msg_outstr((char_u *)"Interrupt: ");

	(void)set_highlight('r');
	start_highlight();
#ifdef ORG_HITRETURN
	msg_outstr("Press RETURN to continue");
	stop_highlight();
	do {
		c = vgetc();
	} while (strchr("\r\n: ", c) == NULL);
	if (c == ':')			 		/* this can vi too (but not always!) */
		stuffcharReadbuff(c);
#else
	msg_outstr((char_u *)"Press RETURN or enter command to continue");
	stop_highlight();
	do
	{
		c = vgetc();
		got_int = FALSE;
	} while (c == Ctrl('C'));
	breakcheck();
# ifdef KANJI
	if (strchr("\r\n ", c) == NULL && c < 0x100)
# else
	if (strchr("\r\n ", c) == NULL)
# endif
		stuffcharReadbuff(c);
#endif

	/*
	 * If the user hits ':' we get a command line from the next line.
	 */
	if (c == ':')
		cmdline_row = msg_row;

	if (!termcap_active)			/* start termcap before redrawing */
		starttermcap();

/*
 * If the window size changed set_winsize() will redraw the screen.
 * Otherwise the screen is only redrawn if 'redraw' is set and no ':' typed.
 */
	tmpState = State;
	State = oldState;				/* restore State before set_winsize */
	msg_check();
	if (tmpState == SETWSIZE)		/* got resize event while in vgetc() */
		set_winsize(0, 0, FALSE);
	else if (redraw == TRUE)
	{
		if (c == ':')
			must_redraw = CLEAR;
		else
			updateScreen(CLEAR);
	}
	else if (msg_scrolled && c != ':' && redraw != -1)
		updateScreen(VALID);

	if (c == ':')
		skip_redraw = TRUE;			/* skip redraw once */
}

/*
 * Prepare for outputting characters in the command line.
 */
	void
msg_start(void)
{
	did_msg = TRUE;					/* for doglob() */
	keep_msg = NULL;				/* don't display old message now */
	msg_pos(cmdline_row, 0);
	cursor_off();
	lines_left = cmdline_row;
}

/*
 * Move message position. This should always be used after moving the cursor.
 * Use negative value if row or col does not have to be changed.
 */
	void
msg_pos(int row, int col)
{
	if (row >= 0)
		msg_row = row;
	if (col >= 0)
		msg_col = col;
	screen_start();
}

	void
msg_outchar(int c)
{
	char_u		buf[2];

	buf[0] = c;
	buf[1] = NUL;
	msg_outstr(buf);
}

	void
msg_outnum(long n)
{
	char_u		buf[20];

	sprintf((char *)buf, "%ld", n);
	msg_outstr(buf);
}

/*
 * output 'len' characters in 'str' (including NULs) with translation
 * if 'len' is -1, output upto a NUL character
 * return the number of characters it takes on the screen
 */
	int
msg_outtrans(register char_u *str, register int len)
{
	int retval = 0;

	if (len == -1)
		len = STRLEN(str);
	while (--len >= 0)
	{
#ifdef KANJI
		char_u		buf[UTF8_MAXLEN + 1];
		if (ISkanji(*str) && str[1])
		{
			int		n = utf_lenat(str, 0);
			int		k;

			if (n > len + 1)
				n = len + 1;		/* stay inside the length we were given */
			for (k = 0; k < n; k++)
				buf[k] = str[k];
			buf[n] = NUL;
			msg_outstr(buf);
			retval += utf_width(buf);
			str += n;
			len -= n - 1;
			continue;
		}
#endif
		msg_outstr(transchar(*str));
		retval += charsize(str);
		++str;
	}
	return retval;
}

/*
 * print line for :p command
 */
	void
msg_prt_line(char_u *s)
{
	register int	si = 0;
	register int	c;
	register int	col = 0;

	int 			n_extra = 0;
	int             n_spaces = 0;
	char_u			*p = NULL;			/* init to make SASC shut up */
	int 			n;

	for (;;)
	{
		if (n_extra)
		{
			--n_extra;
			c = *p++;
		}
		else if (n_spaces)
		{
		    --n_spaces;
			c = ' ';
		}
		else
		{
			c = s[si++];
#ifdef KANJI
			if (ISkanji(c) && s[si])
			{
				char_u		buf[UTF8_MAXLEN + 1];
				int			n = utf_lenat(s, si - 1);
				int			k;

				for (k = 0; k < n; k++)
					buf[k] = s[si - 1 + k];
				buf[n] = NUL;
				si += n - 1;
				msg_outstr(buf);
				col += utf_width(buf);
				continue;
			}
			else
#endif
			if (c == TAB && !curwin->w_p_list)
			{
				/* tab amount depends on current column */
				n_spaces = curbuf->b_p_ts - col % curbuf->b_p_ts - 1;
				c = ' ';
			}
			else if (c == NUL && curwin->w_p_list)
			{
				p = (char_u *)"";
				n_extra = 1;
				c = '$';
			}
			else if (c != NUL && (n = transcharsize(c)) > 1)
			{
				n_extra = n - 1;
				p = transchar(c);
				c = *p++;
			}
		}

		if (c == NUL)
			break;

		msg_outchar(c);
		col++;
	}
}

/*
 * output a string to the screen at position msg_row, msg_col
 * Update msg_row and msg_col for the next message.
 */
	void
msg_outstr(char_u *s)
{
	int		c;

	/*
	 * if there is no valid screen, use fprintf so we can see error messages
	 */
	if (!msg_check_screen())
	{
		fprintf(stderr, "%s", (char *)s);
		return;
	}

	while (*s)
	{
		/*
		 * the screen is scrolled up when:
		 * - When outputting a newline in the last row
		 * - when outputting a character in the last column of the last row
		 *   (some terminals scroll automatically, some don't. To avoid problems
		 *   we scroll ourselves)
		 */
#ifdef KANJI
		if ((msg_row >= Rows - 1 && (*s == '\n' || msg_col >= Columns - 1))
			|| (msg_row >= Rows - 1 && ISkanji(*s)
					&& msg_col + utf_width(s) > (int)Columns))
#else
		if (msg_row >= Rows - 1 && (*s == '\n' || msg_col >= Columns - 1))
#endif
		{
			screen_del_lines(0, 0, 1, (int)Rows);		/* always works */
			msg_row = Rows - 2;
			if (msg_col >= Columns)		/* can happen after screen resize */
				msg_col = Columns - 1;
			++msg_scrolled;
			if (cmdline_row > 0)
				--cmdline_row;
			/*
			 * if screen is completely filled wait for a character
			 */
			if (p_more && --lines_left == 0)
			{
#ifdef notdef
				windgoto((int)Rows - 1, 0);
				outstr((char_u *)"-- more --");
#else
				screen_msg("-- more --", Rows - 1, 0);
#endif
				c = vgetc();
				if (c == CR || c == NL)
					lines_left = 1;
				else if (c == 'q' || c == Ctrl('C'))
					got_int = TRUE;
				else
					lines_left = Rows - 1;
				outstr((char_u *)"\r          ");
			}
			screen_start();
		}
		if (*s == '\n')
		{
			msg_col = 0;
			++msg_row;
		}
		else
		{
#ifdef KANJI
			if (ISkanji(*s) && s[1])
			{
				int		n = utf_lenat(s, 0);
				int		w = utf_width(s);

				if (w == 2 && msg_col == (Columns - 1))
				{	/* a double width character is not split over the edge */
					screen_msg((char_u *)" ", msg_row, msg_col++);
					msg_col = 0;
					msg_row++;
				}
				screen_msg(s, msg_row, msg_col);
				msg_col += w;
				s += n;
				if (msg_col >= Columns)
				{
					msg_col = 0;
					++msg_row;
				}
				continue;
			}
#endif
			screen_outchar(*s, msg_row, msg_col);
			if (++msg_col >= Columns)
			{
				msg_col = 0;
				++msg_row;
			}
		}
		++s;
	}
}

/*
 * msg_check_screen - check if the screen is initialized.
 * Also check msg_row and msg_col, if they are too big it may cause a crash.
 */
	static int
msg_check_screen(void)
{
	if (!screen_valid())
		return FALSE;
	
	if (msg_row >= Rows)
		msg_row = Rows - 1;
	if (msg_col >= Columns)
		msg_col = Columns - 1;
	return TRUE;
}

/*
 * clear from current message position to end of screen
 * Note: msg_col is not updated, so we remember the end of the message
 * for msg_check().
 */
	void
msg_ceol(void)
{
	if (!msg_check_screen())
		return;
	screen_fill(msg_row, msg_row + 1, msg_col, (int)Columns, ' ', ' ');
	screen_fill(msg_row + 1, (int)Rows, 0, (int)Columns, ' ', ' ');
}

/*
 * end putting a message on the screen
 * call wait_return if the message does not fit in the available space
 * return TRUE if wait_return not called.
 */
	int
msg_end(void)
{
	lines_left = -1;
	/*
	 * if the string is larger than the window,
	 * or the ruler option is set and we run into it,
	 * we have to redraw the window.
	 * Do not do this if we are abandoning the file or editing the command line.
	 */
	if (!exiting && msg_check() && State != CMDLINE)
	{
		msg_outchar('\n');
		wait_return(FALSE);
		return FALSE;
	}
	flushbuf();
	return TRUE;
}

/*
 * If the written message has caused the screen to scroll up, or if we
 * run into the shown command or ruler, we have to redraw the window later.
 */
	int
msg_check(void)
{
	lines_left = -1;
	if (msg_scrolled || (msg_row == Rows - 1 && msg_col >= sc_col))
	{
		if (must_redraw < NOT_VALID)
			must_redraw = NOT_VALID;
		redraw_cmdline = TRUE;
		return TRUE;
	}
	return FALSE;
}
