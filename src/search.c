/* vi:ts=4:sw=4
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *
 * Read the file "credits.txt" for a list of people who contributed.
 * Read the file "uganda.txt" for copying and usage conditions.
 */
/*
 * search.c: code for normal mode searching commands
 */

#include "vim.h"
#include "globals.h"
#include "proto.h"
#include "param.h"
#include "ops.h"		/* for mincl */
#ifdef KANJI
#include "kanji.h"
#endif

/* modified Henry Spencer's regular expression routines */
#include "regexp.h"

static int inmacro __ARGS((char_u *, char_u *));
static int cls __ARGS((void));

static char_u *top_bot_msg = (char_u *)"search hit TOP, continuing at BOTTOM";
static char_u *bot_top_msg = (char_u *)"search hit BOTTOM, continuing at TOP";

/*
 * This file contains various searching-related routines. These fall into
 * three groups:
 * 1. string searches (for /, ?, n, and N)
 * 2. character searches within a single line (for f, F, t, T, etc)
 * 3. "other" kinds of searches like the '%' command, and 'word' searches.
 */

/*
 * String searches
 *
 * The string search functions are divided into two levels:
 * lowest:	searchit(); called by dosearch() and edit().
 * Highest: dosearch(); changes curwin->w_cursor, called by normal().
 *
 * The last search pattern is remembered for repeating the same search.
 * This pattern is shared between the :g, :s, ? and / commands.
 * This is in myregcomp().
 *
 * The actual string matching is done using a heavily modified version of
 * Henry Spencer's regular expression library.
 */

/*
 * Two search patterns are remembered: One for the :substitute command and
 * one for other searches. last_pattern points to the one that was
 * used the last time.
 */
static char_u 	*search_pattern = NULL;
static char_u 	*subst_pattern = NULL;
static char_u 	*last_pattern = NULL;

static int		want_start;				/* looking for start of line? */

#ifdef USE_GREP
typedef struct grep {
	int					left;			/* or search						*/
	int					next;			/* and search						*/
	int					not;			/* not search						*/
	regexp			*	prog;			/* regexec parameter				*/
	char_u			*	str;			/* original string pointer			*/
} GREP;
static GREP				g_dmy;
static GREP			*	gpattern = &g_dmy;
static int				g_max	= 0;
static int				p_grep	= 0;
static int				p_wn	= FALSE;
static int				p_join	= FALSE;
static int				p_icase	= FALSE;
static int				p_jicase= FALSE;

static char_u * skip_grepregexp __ARGS((char_u *, int));
static int grepsub __ARGS((GREP *, char_u *, int, linenr_t, int *, char_u **, char_u **));
static int grepexec __ARGS((regexp *, char_u *, int, linenr_t));
static int do_wrapnext __ARGS((int));

static char_u *
skip_grepregexp(char_u *orgstr, int dirc)
{
	regexp	*	prog;
	char_u		pattern[CMDBUFFSIZE];
	char_u	*	p;
	char_u	*	str;
	int			no;
	int			not;
	int			and;
	char_u	*	firstp = NULL;

	for (no = 0; no < g_max; no++)
	{
		if (gpattern[no].prog)
			free(gpattern[no].prog);
		memset((char *)&gpattern[no], 0, sizeof(GREP));
	}
	p_grep	= 0;
	p_wn	= FALSE;
	p_join	= FALSE;
	p_icase	= FALSE;
	p_jicase= FALSE;
	firstp	= skip_regexp(orgstr, dirc);
	strcpy(pattern, orgstr);
	str		= pattern;
	no		= 0;
	not		= FALSE;
	and		= FALSE;
	for (;;)
	{
		p = skip_regexp(str, dirc);
		if (*p == dirc && p[1] != NUL)
		{
			*p++ = NUL;
			if ((prog = regcomp(str)) == NULL)
				goto error;
			else
			{
				if (no >= g_max)
				{
					GREP	*	p;

					if ((p = (GREP *)malloc(sizeof(GREP) * (g_max + 1))) == NULL)
						goto error;
					memset((char *)p, 0, sizeof(GREP) * (g_max + 1));
					memcpy(p, gpattern, sizeof(GREP) * g_max);
					g_max++;
					if (gpattern != &g_dmy)
						free(gpattern);
					gpattern = p;
				}
				gpattern[no].not = not;
				gpattern[no].prog= prog;
				gpattern[no].str = str;
				if (no != 0)
				{
					if (and)
						gpattern[no - 1].next = no;
					else
						gpattern[no - 1].left = no;
				}
				no++;
				not = FALSE;
				and = FALSE;
			}
			if (strncmp(p, "&&",  2) == 0 && p[2] == dirc)
			{
				and = TRUE;
				p  += 3;
			}
			else if (strncmp(p, "&&~", 3) == 0 && p[3] == dirc)
			{
				and = TRUE;
				not = TRUE;
				p  += 4;
			}
			else if (strncmp(p, "||",  2) == 0 && p[2] == dirc)
				p  += 3;
			else if (strncmp(p, "||~", 3) == 0 && p[3] == dirc)
			{
				not = TRUE;
				p  += 4;
			}
			else if (strchr("jlLwWiIcC", *p) != NULL)
			{
				while (*p)
				{
					if (*p == 'W')
					{
						p++;
						p_wn = TRUE;
						if (p_grep == 0)
							p_grep = 1;
					}
					else if (*p == 'w')
					{
						char_u	*	w;
						int			i;

						p++;
						for (i = 0; i < no; i++)
						{
							if ((w = malloc(strlen(gpattern[i].str) + 5)) == NULL)
								goto error;
							strcpy(w, "\\<");
							strcat(w, gpattern[i].str);
							strcat(w, "\\>");
							if ((prog = regcomp(w)) == NULL)
							{
								free(w);
								goto error;
							}
							free(gpattern[i].prog);
							gpattern[i].prog= prog;
							free(w);
						}
						if (p_grep == 0)
							p_grep = 1;
					}
					else if (*p == 'J')
					{
						p++;
						p_join = TRUE;
						if (p_grep <= 1)
							p_grep = 2;
					}
					else if (*p == 'l' || *p == 'L')
					{
						p++;
						if (isdigit(*p))
							p_grep = atol((char *)p);
						else						/* single */
							p_grep = 1;
						while (isdigit(*p))			/* skip number */
							++p;
					}
					else if (*p == 'i' || *p == 'I')
					{
						p++;
						p_icase = TRUE;
						if (p_grep == 0)
							p_grep = 1;
					}
					else if (*p == 'j' || *p == 'C')
					{
						p++;
						p_jicase = TRUE;
						if (p_grep == 0)
							p_grep = 1;
					}
					else if (*p == dirc)
					{
						p++;
						break;
					}
					else
						break;
				}
				firstp = orgstr + (p - pattern);
				break;
			}
			else
				break;
			str = p;
		}
		else
			break;
	}
	return(firstp);
error:
	if (gpattern[0].prog != NULL)
		free(gpattern[0].prog);
	gpattern[0].prog = NULL;
	return(firstp);
}

static int
grepsub(GREP *gp, char_u *string, int at_bol, linenr_t lnum, int *look, char_u **match, char_u **matchend)
{
	linenr_t		i;
	int				found = FALSE;

	if (p_join)
	{
		long				n = 0;
		char_u			*	p;
		char_u			*	cp;
		char_u			*	ep;
		int					kanji = FALSE;

		for (i = 0; i < p_grep; i++)
		{
			if (lnum + i > curbuf->b_ml.ml_line_count)
				break;
			n += strlen(i == 0 ? string : ml_get(lnum + i));
		}
		if ((p = malloc(n + p_grep + 1)) == NULL)
			return(FALSE);
		p[0] = '\0';
		for (i = 0; i < p_grep; i++)
		{
			if (lnum + i > curbuf->b_ml.ml_line_count)
				break;
			cp = (i == 0) ? string : ml_get(lnum + i);
			while (i != 0 && *cp)
			{
				if (*cp == ' ' || *cp == '\t')
					cp++;
				else if (ISkanji(*cp))
				{
					if (isjpspace(cp))
						cp += 2;
					else
						break;
				}
				else
					break;
			}
			if (i != 0)
			{
				if (ISkanji(*cp) && kanji)
					;
				else
					strcat(p, " ");
			}
			ep = p + strlen(p);
			strcat(p, cp);
			cp = ep;
			ep = NULL;
			while (*cp)
			{
				if (*cp == ' ' || *cp == '\t')
				{
					if (ep == NULL)
						ep = cp;
				}
				else if (ISkanji(*cp) && isjpspace(cp))
				{
					if (ep == NULL)
						ep = cp;
					cp++;
				}
				else
				{
					ep = NULL;
					if (ISkanji(*cp))
					{
						kanji = TRUE;
						cp++;
					}
					else
						kanji = FALSE;
				}
				cp++;
			}
			if (ep != NULL)
				*ep = '\0';
		}
		if (regexec(gp->prog, p, at_bol))
		{
			if (gp->prog->startp[0] < (p + strlen(string)))
			{
				*look = TRUE;
				if (*match == NULL
							|| *match > (string + (gp->prog->startp[0] - p)))
				{
					*match = string + (gp->prog->startp[0] - p);
					if ((gp->prog->endp[0] - p) > strlen(string))
						*matchend = string + strlen(string);
					else
						*matchend = string + (gp->prog->endp[0] - p);
				}
			}
			found = TRUE;
		}
		free(p);
	}
	else
	{
		for (i = 0; i < p_grep; i++)
		{
			if (lnum + i > curbuf->b_ml.ml_line_count)
				break;
			if (regexec(gp->prog, i == 0 ? string : ml_get(lnum + i), at_bol))
			{
				if (i == 0)
				{
					*look = TRUE;
					if (*match == NULL || *match > gp->prog->startp[0])
					{
						*match = gp->prog->startp[0];
						*matchend = gp->prog->endp[0];
					}
				}
				found = TRUE;
				break;
			}
		}
	}
	if (gp->not && !found)
		found = TRUE;
	else if (gp->not && found)
		found = FALSE;
	if (!found && gp->left)
		found = grepsub(&gpattern[gp->left], string, at_bol, lnum, look, match, matchend);
	if (found && gp->next)
		return(grepsub(&gpattern[gp->next], string, at_bol, lnum, look, match, matchend));
	if (found && gp->next == 0)
		return(TRUE);
	return(FALSE);
}

static int
grepexec(regexp *prog, char_u *string, int at_bol, linenr_t lnum)
{
	int				look = FALSE;
	char_u		*	match		= NULL;
	char_u		*	matchend	= NULL;

	if (p_grep == 0)
		return(regexec(prog, string, at_bol));
	if (grepsub(&gpattern[0], string, at_bol, lnum, &look, &match, &matchend))
	{
		prog->startp[0] = match;
		prog->endp[0] = matchend;
		return(look);
	}
	return(FALSE);
}

static int
do_wrapnext(int dir)
{
	int			other = TRUE;
	int			more = p_more;
	int			i;

	if (!p_wn)
		return(FAIL);
	if (got_int)
		return(FAIL);
	i = curwin->w_arg_idx + dir;
	if (i < 0 || i >= arg_count)
		return(FAIL);
	if (p_hid)
		other = otherfile(fix_fname(arg_files[i]));
	if ((!p_hid || !other)
			&& (curbuf->b_changed && (!other || curbuf->b_nwindows <= 1)
										&& (autowrite(curbuf) == FAIL)))
		return(FAIL);
	curwin->w_arg_idx = i;
	p_more = FALSE;
	++no_wait_return;
	(void)doecmd(arg_files[curwin->w_arg_idx], NULL, NULL, p_hid, (linenr_t)1);
	--no_wait_return;
	p_more = more;
	breakcheck();
	if (got_int)
		return(FAIL);
	flushbuf();
	return(OK);
}
#endif

/*
 * Return TRUE if pattern contains an uppercase character.
 */
	int
pattern_has_uppercase(char_u *pat)
{
	char_u *p;

	if (pat == NULL)
		return FALSE;
	for (p = pat; *p; p++)
	{
		if (*p == '\\')
		{
			p++;
			if (*p == NUL)
				break;
			continue;
		}
#ifdef KANJI
		if (ISkanji(*p))
		{
			p++;
			if (*p == NUL)
				break;
			continue;
		}
#endif
		if (isasciiupper(*p))
			return TRUE;
	}
	return FALSE;
}

/*
 * translate search pattern for regcomp()
 *
 * sub_cmd == 0: save pat in search_pattern (normal search command)
 * sub_cmd == 1: save pat in subst_pattern (:substitute command)
 * sub_cmd == 2: save pat in both patterns (:global command)
 * which_pat == 0: use previous search pattern if "pat" is NULL
 * which_pat == 1: use previous sustitute pattern if "pat" is NULL
 * which_pat == 2: use last used pattern if "pat" is NULL
 *
 */
	regexp *
myregcomp(char_u *pat, int sub_cmd, int which_pat)
{
	regexp *retval;

	if (pat == NULL || *pat == NUL)     /* use previous search pattern */
	{
		if (which_pat == 0)
		{
			if (search_pattern == NULL)
			{
				emsg(e_noprevre);
				return (regexp *) NULL;
			}
			pat = search_pattern;
		}
		else if (which_pat == 1)
		{
			if (subst_pattern == NULL)
			{
				emsg(e_nopresub);
				return (regexp *) NULL;
			}
			pat = subst_pattern;
		}
		else	/* which_pat == 2 */
		{
			if (last_pattern == NULL)
			{
				emsg(e_noprevre);
				return (regexp *) NULL;
			}
			pat = last_pattern;
		}
	}

	/*
	 * save the currently used pattern in the appropriate place,
	 * unless the pattern should not be remembered
	 */
	if (!keep_old_search_pattern)
	{
		if (sub_cmd == 0 || sub_cmd == 2)	/* search or global command */
		{
			if (search_pattern != pat)
			{
				free(search_pattern);
				search_pattern = strsave(pat);
				last_pattern = search_pattern;
				reg_magic = p_magic;		/* Magic sticks with the r.e. */
			}
		}
		if (sub_cmd == 1 || sub_cmd == 2)	/* substitute or global command */
		{
			if (subst_pattern != pat)
			{
				free(subst_pattern);
				subst_pattern = strsave(pat);
				last_pattern = subst_pattern;
				reg_magic = p_magic;		/* Magic sticks with the r.e. */
			}
		}
	}

	want_start = (*pat == '^');		/* looking for start of line? */
	reg_ic = (p_scs && pattern_has_uppercase(pat)) ? FALSE : p_ic;	/* tell the regexec routine how to search */
#ifdef KANJI
	reg_jic = p_jic;				/* tell the regexec routine how to search */
#endif
#ifdef USE_GREP
	reg_ic = reg_ic ? reg_ic : p_icase;
# ifdef KANJI
	reg_jic = reg_jic ? reg_jic : p_jicase;
# endif
	if (p_grep && sub_cmd == 0 && which_pat == 2 && gpattern[0].prog != NULL)
	{
		if ((retval = malloc(sizeof(regexp))) != NULL)
			memcpy(retval, gpattern[0].prog, sizeof(regexp));
	}
	else
#endif
	retval = regcomp(pat);
	return retval;
}

/*
 * lowest level search function.
 * Search for 'count'th occurrence of 'str' in direction 'dir'.
 * Start at position 'pos' and return the found position in 'pos'.
 * Return OK for success, FAIL for failure.
 */
	int
searchit(FPOS *pos, int dir, char_u *str, long count, int end, int message)
{
	int 				found;
	linenr_t			lnum = 0;			/* init to shut up gcc */
	linenr_t			startlnum;
	regexp				*prog;
	char_u				*s;
	char_u				*ptr;
	int					i;
	char_u				*match, *matchend;
	int 				loop;
#ifdef USE_GREP
	int					wscan = p_ws;
	long				fcount= count;
#endif

	if ((prog = myregcomp(str, 0, 2)) == NULL)
	{
		if (message)
			emsg(e_invstring);
		return FAIL;
	}
/*
 * find the string
 */
#ifdef USE_GREP
	if (p_wn)
		p_ws = FALSE;
retry:
#endif
	found = 1;
	while (count-- && found)    /* stop after count matches, or no more matches */
	{
		startlnum = pos->lnum;	/* remember start of search for detecting no match */
		found = 0;				/* default: not found */

#ifdef KANJI
		i = pos->col;
		lnum = pos->lnum;
		if (dir == FORWARD)
		{
			i += kanjilenCol(lnum, i);
		}
		else /* dir == BACKWARD */
		{
			i --;
			if (i < 0)
				-- lnum;
			else if (ISkanjiCol(lnum, i) == 2)
				i --;
		}
#else
		i = pos->col + dir; 	/* search starts one postition away */
		lnum = pos->lnum;

		if (dir == BACKWARD && i < 0)
			--lnum;
#endif

		for (loop = 0; loop != 2; ++loop)   /* do this twice if 'wrapscan' is set */
		{
			for ( ; lnum > 0 && lnum <= curbuf->b_ml.ml_line_count; lnum += dir, i = -1)
			{
				s = ptr = ml_get(lnum);
				if (dir == FORWARD && i > 0)    /* first line for forward search */
				{
					if (want_start || STRLEN(s) <= (size_t)i)   /* match not possible */
						continue;
					s += i;
				}

#ifdef USE_GREP
				if (grepexec(prog, s, dir == BACKWARD || i <= 0, lnum))
#else
				if (regexec(prog, s, dir == BACKWARD || i <= 0))
#endif
				{							/* match somewhere on line */
					match = prog->startp[0];
					matchend = prog->endp[0];
					if (dir == BACKWARD && !want_start)
					{
						/*
						 * Now, if there are multiple matches on this line,
						 * we have to get the last one. Or the last one before
						 * the cursor, if we're on that line.
						 */
#ifdef KANJI
# ifdef USE_GREP
						while (*match && grepexec(prog, match + utf_len(*match), (int)FALSE, lnum))
# else
						while (*match && regexec(prog, match + utf_len(*match), (int)FALSE))
# endif
#else
						while (*match != NUL && regexec(prog, match + 1, (int)FALSE))
#endif
						{
							if ((i >= 0) && ((prog->startp[0] - s) > i))
								break;
							match = prog->startp[0];
							matchend = prog->endp[0];
						}

						if ((i >= 0) && ((match - s) > i))
							continue;
					}

					pos->lnum = lnum;
					if (end)
						pos->col = (int) (matchend - ptr - 1);
					else
						pos->col = (int) (match - ptr);
					found = 1;
#ifdef USE_GREP
					fcount--;
#endif
					break;
				}
				/* breakcheck is slow, do it only once in 16 lines */
				if ((lnum & 15) == 0)
					breakcheck();       /* stop if ctrl-C typed */
				if (got_int)
					break;

				if (loop && lnum == startlnum)  /* if second loop stop where started */
					break;
			}
	/* stop the search if wrapscan isn't set, after an interrupt and after a match */
			if (!p_ws || got_int || found)
				break;

			/*
			 * If 'wrapscan' is set we continue at the other end of the file.
			 * If 'terse' is not set, we give a message.
			 * This message is also remembered in keep_msg for when the screen
			 * is redrawn. The keep_msg is cleared whenever another message is
			 * written.
			 */
#ifndef notdef
			if (msg_scrolled && !p_terse && message)
			{
				if (must_redraw < NOT_VALID)
					must_redraw = NOT_VALID;
				redraw_cmdline = TRUE;
				msg_scrolled = 0;
			}
#endif
			if (dir == BACKWARD)    /* start second loop at the other end */
			{
				lnum = curbuf->b_ml.ml_line_count;
				if (!p_terse && message)
				{
					msg(top_bot_msg);
					keep_msg = top_bot_msg;
				}
			}
			else
			{
				lnum = 1;
				if (!p_terse && message)
				{
					msg(bot_top_msg);
					keep_msg = bot_top_msg;
				}
			}
		}
		if (got_int)
			break;
	}
#ifdef USE_GREP
	if (fcount && !got_int && p_wn)
	{
		found = 0;
		if (do_wrapnext(dir) == OK)
		{
			if (must_redraw)
				updateScreen(must_redraw);
			flushbuf();
			count = fcount;
			if (dir == BACKWARD)
			{
				pos->lnum = curbuf->b_ml.ml_line_count;
				pos->col  = strlen(ml_get(pos->lnum));
			}
			else
			{
				pos->lnum = 1;
				pos->col  = -1;
			}
			goto retry;
		}
	}
	p_ws = wscan;
#endif

	free(prog);

	if (!found)             /* did not find it */
	{
		if (got_int)
			emsg(e_interr);
		else if (message)
		{
			if (p_ws)
				emsg(e_patnotf);
			else if (lnum == 0)
				EMSG("search hit TOP without match");
			else
				EMSG("search hit BOTTOM without match");
		}
		return FAIL;
	}

	return OK;
}

/*
 * Highest level string search function.
 * Search for the 'count'th occurence of string 'str' in direction 'dirc'
 *					If 'dirc' is 0: use previous dir.
 * If 'str' is 0 or 'str' is empty: use previous string.
 *			  If 'reverse' is TRUE: go in reverse of previous dir.
 *				 If 'echo' is TRUE: echo the search command and handle options
 *			  If 'message' is TRUE: may give error message
 *
 * return 0 for failure, 1 for found, 2 for found and line offset added
 */
	int
dosearch(int dirc, char_u *str, int reverse, long count, int echo, int message)
{
	FPOS			pos;		/* position of the last match */
	char_u			*searchstr;
	static int		lastsdir = '/';	/* previous search direction */
	static int		lastoffline;/* previous/current search has line offset */
	static int		lastend;	/* previous/current search set cursor at end */
	static long 	lastoff;	/* previous/current line or char offset */
	int				old_lastsdir;
	int				old_lastoffline;
	int				old_lastend;
	long			old_lastoff;
	int				ret;		/* Return value */
	char_u			*p;
	long			c;
	char_u			*dircp = NULL;

	/*
	 * save the values for when keep_old_search_pattern is set
	 * (no if around this because gcc wants them initialized)
	 */
	old_lastsdir = lastsdir;
	old_lastoffline = lastoffline;
	old_lastend = lastend;
	old_lastoff = lastoff;

	if (dirc == 0)
		dirc = lastsdir;
	else
		lastsdir = dirc;
	if (reverse)
	{
		if (dirc == '/')
			dirc = '?';
		else
			dirc = '/';
	}
	searchstr = str;
									/* use previous string */
	if (str == NULL || *str == NUL || *str == dirc)
	{
		if (search_pattern == NULL)
		{
			emsg(e_noprevre);
			ret = 0;
			goto end_dosearch;
		}
		searchstr = (char_u *)"";	/* will use search_pattern in myregcomp() */
	}
	if (str != NULL && *str != NUL)	/* look for (new) offset */
	{
		/*
		 * Find end of regular expression.
		 * If there is a matching '/' or '?', toss it.
		 */
#ifdef USE_GREP
		p = skip_grepregexp(str, dirc);
#else
		p = skip_regexp(str, dirc);
#endif
		if (*p == dirc)
		{
			dircp = p;		/* remember where we put the NUL */
			*p++ = NUL;
		}
		lastoffline = FALSE;
		lastend = FALSE;
		lastoff = 0;
		/*
		 * Check for a line offset or a character offset.
		 * for get_address (echo off) we don't check for a character offset,
		 * because it is meaningless and the 's' could be a substitute command.
		 */
		if (*p == '+' || *p == '-' || isdigit(*p))
			lastoffline = TRUE;
		else if (echo && (*p == 'e' || *p == 's' || *p == 'b'))
		{
			if (*p == 'e')			/* end */
				lastend = TRUE;
			++p;
		}
		if (isdigit(*p) || *p == '+' || *p == '-')     /* got an offset */
		{
			if (isdigit(*p) || isdigit(*(p + 1)))
				lastoff = atol((char *)p);		/* 'nr' or '+nr' or '-nr' */
			else if (*p == '-')			/* single '-' */
				lastoff = -1;
			else						/* single '+' */
				lastoff = 1;
			++p;
			while (isdigit(*p))			/* skip number */
				++p;
		}
		searchcmdlen = p - str;			/* compute lenght of search command
														for get_address() */
	}

	if (echo)
	{
		msg_start();
		msg_outchar(dirc);
		msg_outtrans(*searchstr == NUL ? search_pattern : searchstr, -1);
		if (lastoffline || lastend || lastoff)
		{
			msg_outchar(dirc);
			if (lastend)
				msg_outchar('e');
			else if (!lastoffline)
				msg_outchar('s');
			if (lastoff < 0)
			{
				msg_outchar('-');
				msg_outnum((long)-lastoff);
			}
			else if (lastoff > 0 || lastoffline)
			{
				msg_outchar('+');
				msg_outnum((long)lastoff);
			}
		}
		msg_ceol();
		(void)msg_check();

		gotocmdline(FALSE, NUL);
		flushbuf();
	}

	pos = curwin->w_cursor;

	c = searchit(&pos, dirc == '/' ? FORWARD : BACKWARD, searchstr, count, lastend, message);
	if (dircp)
		*dircp = dirc;			/* put second '/' or '?' back for normal() */
	if (c == FAIL)
	{
		ret = 0;
		goto end_dosearch;
	}
	if (lastend)
		mincl = TRUE;			/* 'e' includes last character */

	if (!lastoffline)           /* add the character offset to the column */
	{
		if (lastoff > 0)        /* offset to the right, check for end of line */
		{
			p = ml_get_pos(&pos) + 1;
			c = lastoff;
			while (c-- && *p++ != NUL)
				++pos.col;
		}
		else					/* offset to the left, check for start of line */
		{
			if ((c = pos.col + lastoff) < 0)
				c = 0;
			pos.col = c;
		}
	}

	if (!tag_busy)
		setpcmark();
	curwin->w_cursor = pos;
	curwin->w_set_curswant = TRUE;

	if (!lastoffline)
	{
		ret = 1;
		goto end_dosearch;
	}

/*
 * add the offset to the line number.
 */
	c = curwin->w_cursor.lnum + lastoff;
	if (c < 1)
		curwin->w_cursor.lnum = 1;
	else if (c > curbuf->b_ml.ml_line_count)
		curwin->w_cursor.lnum = curbuf->b_ml.ml_line_count;
	else
		curwin->w_cursor.lnum = c;
	curwin->w_cursor.col = 0;

	ret = 2;

end_dosearch:
	if (keep_old_search_pattern)
	{
		lastsdir = old_lastsdir;
		lastoffline = old_lastoffline;
		lastend = old_lastend;
		lastoff = old_lastoff;
	}
	return ret;
}


/*
 * Character Searches
 */

/*
 * searchc(c, dir, type, count)
 *
 * Search for character 'c', in direction 'dir'. If 'type' is 0, move to the
 * position of the character, otherwise move to just before the char.
 * Repeat this 'count' times.
 */
	int
#ifndef KANJI
searchc(int c, int dir, int type, long count)
#else
searchc(int c, int k, int dir, int type, long count)
#endif
{
	static int	 	lastc = NUL;	/* last character searched for */
#ifdef KANJI
	static int		lastk = NUL;
#endif
	static int		lastcdir;		/* last direction of character search */
	static int		lastctype;		/* last type of search ("find" or "to") */
	int				col;
	char_u			*p;
	int 			len;

	if (c != NUL)       /* normal search: remember args for repeat */
	{
		lastc = c;
#ifdef KANJI
		lastk = k;
#endif
		lastcdir = dir;
		lastctype = type;
	}
	else				/* repeat previous search */
	{
		if (lastc == NUL)
			return FALSE;
		if (dir)        /* repeat in opposite direction */
			dir = -lastcdir;
		else
			dir = lastcdir;
	}

	p = ml_get(curwin->w_cursor.lnum);
	col = curwin->w_cursor.col;
	len = STRLEN(p);

	/*
	 * On 'to' searches, skip one to start with so we can repeat searches in
	 * the same direction and have it work right.
	 * REMOVED to get vi compatibility
	 * if (lastctype)
	 *	col += dir;
	 */

	while (count--)
	{
			for (;;)
			{
#ifdef KANJI
				if (dir > 0 && ISkanji(p[col])) col ++;
				col += dir;
				if (dir < 0 && ISkanjiPosition(p, col + 1) == 2) col --;

				if (col < 0 || col >= len)
					return FALSE;

				if (ISkanji(p[col]))
				{
					if (p[col] == lastc && p[col + 1] == lastk)
						break;
				}
				else if (p[col] == lastc)
					break;
#else
				if ((col += dir) < 0 || col >= len)
					return FALSE;
				if (p[col] == lastc)
						break;
#endif
			}
	}
	if (lastctype)
#ifdef KANJI
	{
		if (dir < 0 && ISkanji(p[col]))
			col ++;
		col -= dir;
		if (dir > 0 && ISkanjiPosition(p, col + 1) == 2)
			col --;
    }
#else
		col -= dir;
#endif
	curwin->w_cursor.col = col;
	return TRUE;
}

/*
 * "Other" Searches
 */

/*
 * showmatch - move the cursor to the matching paren or brace
 *
 * Improvement over vi: Braces inside quotes are ignored.
 */
	FPOS		   *
showmatch(int initc)
{
	static FPOS		pos;				/* current search position */
	int				findc;				/* matching brace */
	int				c;
	int 			count = 0;			/* cumulative number of braces */
	int 			idx = 0;			/* init for gcc */
#ifdef notdef
	static char_u 	table[6] = {'(', ')', '[', ']', '{', '}'};
#else
	static char_u 	table[8] = {'(', ')', '[', ']', '{', '}', '<', '>'};
#endif
	int 			inquote = 0;		/* non-zero when inside quotes */
	char_u			*linep;				/* pointer to current line */
	char_u			*ptr;
	int				do_quotes;			/* check for quotes in current line */
	int				hash_dir = 0;		/* Direction searched for # things */
	int				comment_dir = 0;	/* Direction searched for comments */
#ifdef USE_OPT
	int				incomment = 0;		/* non-zero when inside block comment */
#endif

	pos = curwin->w_cursor;
	linep = ml_get(pos.lnum);

	/*
	 * if initc given, look in the table for the matching character
	 */
	if (initc != NUL)
	{
		for (idx = 0; idx < (sizeof(table)/sizeof(char_u)); ++idx)
			if (table[idx] == initc)
			{
				initc = table[idx = idx ^ 1];
				break;
			}
		if (idx == (sizeof(table)/sizeof(char_u)))			/* invalid initc! */
			return NULL;
	}
	/*
	 * no initc given, look under the cursor
	 */
	else
	{
		if (linep[0] == '#' && pos.col == 0)
			hash_dir = 1;

		/*
		 * Are we on a comment?
		 */
		if (linep[pos.col] == '/')
		{
			if (linep[pos.col + 1] == '*')
			{
				comment_dir = 1;
				idx = 0;
			}
			else if (pos.col > 0 && linep[pos.col - 1] == '*')
			{
				comment_dir = -1;
				idx = 1;
			}
		}
		if (linep[pos.col] == '*')
		{
			if (linep[pos.col + 1] == '/')
			{
				comment_dir = -1;
				idx = 1;
			}
			else if (pos.col > 0 && linep[pos.col - 1] == '/')
			{
				comment_dir = 1;
				idx = 0;
			}
		}

		/*
		 * If we are not on a comment or the # at the start of a line, then
		 * look for brace anywhere on this line after the cursor.
		 */
		if (!hash_dir && !comment_dir)
		{
			/*
			 * find the brace under or after the cursor
			 */
			linep = ml_get(pos.lnum);
			for (;;)
			{
				initc = linep[pos.col];
				if (initc == NUL)
					break;

#ifdef KANJI
				if (ISkanji(initc))
				{
					pos.col += 2;
					continue;
				}
#endif
				for (idx = 0; idx < (sizeof(table)/sizeof(char_u)); ++idx)
					if (table[idx] == initc)
						break;
				if (idx != (sizeof(table)/sizeof(char_u)))
					break;
				++pos.col;
			}
			if (idx == (sizeof(table)/sizeof(char_u)))
			{
				if (linep[0] == '#')
					hash_dir = 1;
				else
					return NULL;
			}
		}
		if (hash_dir)
		{
			/*
			 * Look for matching #if, #else, #elif, or #endif
			 */
			mtype = MLINE;		/* Linewise for this case only */
			ptr = linep + 1;
			while (*ptr == ' ' || *ptr == TAB)
				ptr++;
			if (STRNCMP(ptr, "if", (size_t)2) == 0 || STRNCMP(ptr, "el", (size_t)2) == 0)
				hash_dir = 1;
			else if (STRNCMP(ptr, "endif", (size_t)5) == 0)
				hash_dir = -1;
			else
				return NULL;
			pos.col = 0;
			while (!got_int)
			{
				if (hash_dir > 0)
				{
					if (pos.lnum == curbuf->b_ml.ml_line_count)
						break;
				}
				else if (pos.lnum == 1)
					break;
				pos.lnum += hash_dir;
				linep = ml_get(pos.lnum);
				if ((pos.lnum & 15) == 0)
					breakcheck();
				if (linep[0] != '#')
					continue;
				ptr = linep + 1;
				while (*ptr == ' ' || *ptr == TAB)
					ptr++;
				if (hash_dir > 0)
				{
					if (STRNCMP(ptr, "if", (size_t)2) == 0)
						count++;
					else if (STRNCMP(ptr, "el", (size_t)2) == 0)
					{
						if (count == 0)
							return &pos;
					}
					else if (STRNCMP(ptr, "endif", (size_t)5) == 0)
					{
						if (count == 0)
							return &pos;
						count--;
					}
				}
				else
				{
					if (STRNCMP(ptr, "if", (size_t)2) == 0)
					{
						if (count == 0)
							return &pos;
						count--;
					}
					else if (STRNCMP(ptr, "endif", (size_t)5) == 0)
						count++;
				}
			}
			return NULL;
		}
	}

	findc = table[idx ^ 1];		/* get matching brace */
	idx &= 1;

	do_quotes = -1;
	while (!got_int)
	{
		/*
		 * Go to the next position, forward or backward. We could use
		 * inc() and dec() here, but that is much slower
		 */
		if (idx)              			/* backward search */
		{
			if (pos.col == 0)   		/* at start of line, go to previous one */
			{
				if (pos.lnum == 1)      /* start of file */
					break;
				--pos.lnum;
				linep = ml_get(pos.lnum);
				pos.col = STRLEN(linep);	/* put pos.col on trailing NUL */
				do_quotes = -1;
					/* we only do a breakcheck() once for every 16 lines */
				if ((pos.lnum & 15) == 0)
					breakcheck();
			}
			else
				--pos.col;
#ifdef KANJI
			if (linep[pos.col] && ISkanjiPosition(linep, pos.col + 1))
			{
				pos.col--;
				continue;
			}
#endif
#ifdef USE_OPT
			if (!comment_dir && !inquote && (curbuf->b_p_opt & FOPT_C_COMMENT))
			{
				char_u		k = linep[pos.col];
				char_u	*	s;
				char_u	*	c;

				linep[pos.col] = NUL;
				s = strrchr(linep, '"');
				c = strrchr(linep, '/');
				linep[pos.col] = k;
				/* line comment */
				if (!incomment && c > s && linep != c && c[-1] == '/')
				{
					pos.col = &c[-1] - linep;
					continue;
				}
				/* block comment */
				if (!incomment && c > s && linep != c && c[-1] == '*')
					incomment = -1;
				if (incomment && c > s && c[1] == '*')
				{
					incomment = 0;
					pos.col = c - linep;
					continue;
				}
			}
#endif
		}
		else							/* forward search */
		{
			if (linep[pos.col] == NUL)  /* at end of line, go to next one */
			{
				if (pos.lnum == curbuf->b_ml.ml_line_count) /* end of file */
					break;
				++pos.lnum;
				linep = ml_get(pos.lnum);
				pos.col = 0;
				do_quotes = -1;
					/* we only do a breakcheck() once for every 16 lines */
				if ((pos.lnum & 15) == 0)
					breakcheck();
			}
			else
				++pos.col;
#ifdef KANJI
			if (ISkanjiPosition(linep, pos.col + 1))
			{
				pos.col++;
				continue;
			}
#endif
#ifdef USE_OPT
			if (!comment_dir && !inquote && (curbuf->b_p_opt & FOPT_C_COMMENT))
			{
				/* line comment */
				if (!incomment && linep[pos.col] == '/' && linep[pos.col + 1] == '/')
				{
					pos.col = STRLEN(linep);
					continue;
				}
				/* block comment */
				if (!incomment && linep[pos.col] == '/' && linep[pos.col + 1] == '*')
					incomment = 1;
				if (incomment && linep[pos.col] == '*' && linep[pos.col + 1] == '/')
					incomment = 0;
			}
#endif
		}

		if (comment_dir)
		{
			/* Note: comments do not nest, and we ignore quotes in them */
			if (linep[pos.col] != '/' ||
							(comment_dir == 1 && pos.col == 0) ||
							linep[pos.col - comment_dir] != '*')
				continue;
			return &pos;
		}

		if (do_quotes == -1)		/* count number of quotes in this line */
		{
			/*
			 * count the number of quotes in the line, skipping \" and '"'
			 */
			for (ptr = linep; *ptr; ++ptr)
				if (*ptr == '"' && (ptr == linep || ptr[-1] != '\\') &&
#ifdef KANJI
					(ptr > linep ? !ISkanjiPointer(linep, &ptr[-1]) : TRUE) &&
#endif
							(ptr == linep || ptr[-1] != '\'' || ptr[1] != '\''))
					++do_quotes;
			do_quotes &= 1;			/* result is 1 with even number of quotes */

			/*
			 * If we find an uneven count, check current line and previous
			 * one for a '\' at the end.
			 */
			if (!do_quotes)
			{
				inquote = FALSE;
				if (ptr[-1] == '\\')
				{
					do_quotes = 1;
					if (idx)					/* backward search */
						inquote = TRUE;
				}
				if (pos.lnum > 1)
				{
					ptr = ml_get(pos.lnum - 1);
					if (*ptr && *(ptr + STRLEN(ptr) - 1) == '\\')
					{
						do_quotes = 1;
						if (!idx)				/* forward search */
							inquote = TRUE;
					}
				}
			}
		}

		/*
		 * Things inside quotes are ignored by setting 'inquote'.
		 * If we find a quote without a preceding '\' invert 'inquote'.
		 * At the end of a line not ending in '\' we reset 'inquote'.
		 *
		 * In lines with an uneven number of quotes (without preceding '\')
		 * we do not know which part to ignore. Therefore we only set
		 * inquote if the number of quotes in a line is even,
		 * unless this line or the previous one ends in a '\'.
		 * Complicated, isn't it?
		 */
#ifdef USE_OPT
		if (incomment && (curbuf->b_p_opt & FOPT_C_COMMENT))
			;
		else
#endif
		switch (c = linep[pos.col])
		{
		case NUL:
			inquote = FALSE;
			break;

		case '"':
				/* a quote that is preceded with a backslash is ignored */
			if (do_quotes && (pos.col == 0 || linep[pos.col - 1] != '\\'))
				inquote = !inquote;
			break;

		/*
		 * Skip things in single quotes: 'x' or '\x'.
		 * Be careful for single single quotes, eg jon's.
		 * Things like '\233' or '\x3f' are not skipped, there is never a
		 * brace in them.
		 */
		case '\'':
			if (idx)						/* backward search */
			{
				if (pos.col > 1)
				{
					if (linep[pos.col - 2] == '\'')
						pos.col -= 2;
					else if (linep[pos.col - 2] == '\\' && pos.col > 2 && linep[pos.col - 3] == '\'')
						pos.col -= 3;
				}
			}
			else if (linep[pos.col + 1])	/* forward search */
			{
				if (linep[pos.col + 1] == '\\' && linep[pos.col + 2] && linep[pos.col + 3] == '\'')
					pos.col += 3;
				else if (linep[pos.col + 2] == '\'')
					pos.col += 2;
			}
			break;

		default:
			if (!inquote)      /* only check for match outside of quotes */
			{
				if (c == initc)
					count++;
				else if (c == findc)
				{
					if (count == 0)
						return &pos;
					count--;
				}
			}
		}
	}
	return (FPOS *) NULL;       /* never found it */
}

/*
 * findfunc(dir, what) - Find the next line starting with 'what' in direction 'dir'
 *
 * Return TRUE if a line was found.
 */
	int
findfunc(int dir, int what, long count)
{
	linenr_t	curr;

	curr = curwin->w_cursor.lnum;

	for (;;)
	{
		if (dir == FORWARD)
		{
				if (curr++ == curbuf->b_ml.ml_line_count)
						break;
		}
		else
		{
				if (curr-- == 1)
						break;
		}

		if (*ml_get(curr) == what)
		{
			if (--count > 0)
				continue;
			setpcmark();
			curwin->w_cursor.lnum = curr;
			curwin->w_cursor.col = 0;
			return TRUE;
		}
	}

	return FALSE;
}

/*
 * findsent(dir, count) - Find the start of the next sentence in direction 'dir'
 * Sentences are supposed to end in ".", "!" or "?" followed by white space or
 * a line break. Also stop at an empty line.
 * Return TRUE if the next sentence was found.
 */
	int
findsent(int dir, long count)
{
	FPOS			pos, tpos;
	int				c;
	int 			(*func) __PARMS((FPOS *));
	int 			startlnum;
	int				noskip = FALSE;			/* do not skip blanks */

	pos = curwin->w_cursor;
	if (dir == FORWARD)
		func = incl;
	else
		func = decl;

	while (count--)
	{
		/* if on an empty line, skip upto a non-empty line */
		if (gchar(&pos) == NUL)
		{
			do
				if ((*func)(&pos) == -1)
					break;
			while (gchar(&pos) == NUL);
			if (dir == FORWARD)
				goto found;
		}
		/* if on the start of a paragraph or a section and searching
		 * forward, go to the next line */
		else if (dir == FORWARD && pos.col == 0 && startPS(pos.lnum, NUL, FALSE))
		{
			if (pos.lnum == curbuf->b_ml.ml_line_count)
				return FALSE;
			++pos.lnum;
			goto found;
		}
		else if (dir == BACKWARD)
			decl(&pos);

		/* go back to the previous non-blank char */
		while ((c = gchar(&pos)) == ' ' || c == '\t' ||
#ifdef KANJI
				(dir == BACKWARD &&
					(STRCHR(".!?)]\"'", c) != NULL || isjsend(ml_get_pos(&pos)))
					&& c != NUL))
#else
					(dir == BACKWARD && strchr(".!?)]\"'", c) != NULL && c != NUL))
#endif
			if (decl(&pos) == -1)
				break;

		/* remember the line where the search started */
		startlnum = pos.lnum;

		for (;;)                /* find end of sentence */
		{
			if ((c = gchar(&pos)) == NUL ||
							(pos.col == 0 && startPS(pos.lnum, NUL, FALSE)))
			{
				if (dir == BACKWARD && pos.lnum != startlnum)
					++pos.lnum;
				break;
			}
#ifdef KANJI
			if (c == '.' || c == '!' || c == '?' || isjsend(ml_get_pos(&pos)))
#else
			if (c == '.' || c == '!' || c == '?')
#endif
			{
				tpos = pos;
				do
					if ((c = inc(&tpos)) == -1)
						break;
				while (strchr(")}\"'", c = gchar(&tpos)) != NULL && c != NUL);
#ifdef KANJI
				if (ISkanji(c) || c == -1  || c == ' ' || c == '\t' || c == NUL)
#else
				if (c == -1  || c == ' ' || c == '\t' || c == NUL)
#endif
				{
					pos = tpos;
					if (gchar(&pos) == NUL) /* skip NUL at EOL */
						inc(&pos);
					break;
				}
			}
			if ((*func)(&pos) == -1)
			{
				if (count)
					return FALSE;
				noskip = TRUE;
				break;
			}
		}
found:
			/* skip white space */
		while (!noskip && ((c = gchar(&pos)) == ' ' || c == '\t'))
			if (incl(&pos) == -1)
				break;
	}

	setpcmark();
	curwin->w_cursor = pos;
	return TRUE;
}

/*
 * findpar(dir, count, what) - Find the next paragraph in direction 'dir'
 * Paragraphs are currently supposed to be separated by empty lines.
 * Return TRUE if the next paragraph was found.
 * If 'what' is '{' or '}' we go to the next section.
 * If 'both' is TRUE also stop at '}'.
 */
	int
findpar(int dir, long count, int what, int both)
{
	linenr_t			curr;
	int					did_skip;		/* TRUE after separating lines have
												been skipped */
	int					first;			/* TRUE on first line */

	curr = curwin->w_cursor.lnum;

	while (count--)
	{
		did_skip = FALSE;
		for (first = TRUE; ; first = FALSE)
		{
				if (*ml_get(curr) != NUL)
					did_skip = TRUE;

				if (!first && did_skip && startPS(curr, what, both))
					break;

				if ((curr += dir) < 1 || curr > curbuf->b_ml.ml_line_count)
				{
						if (count)
								return FALSE;
						curr -= dir;
						break;
				}
		}
	}
	setpcmark();
	if (both && *ml_get(curr) == '}')	/* include line with '}' */
		++curr;
	curwin->w_cursor.lnum = curr;
	if (curr == curbuf->b_ml.ml_line_count)
	{
		if ((curwin->w_cursor.col = STRLEN(ml_get(curr))) != 0)
			--curwin->w_cursor.col;
		mincl = TRUE;
	}
	else
		curwin->w_cursor.col = 0;
	return TRUE;
}

/*
 * check if the string 's' is a nroff macro that is in option 'opt'
 */
	static int
inmacro(char_u *opt, char_u *s)
{
		char_u *macro;

		for (macro = opt; macro[0]; ++macro)
		{
				if (macro[0] == s[0] && (((s[1] == NUL || s[1] == ' ')
						&& (macro[1] == NUL || macro[1] == ' ')) || macro[1] == s[1]))
						break;
				++macro;
				if (macro[0] == NUL)
						break;
		}
		return (macro[0] != NUL);
}

/*
 * startPS: return TRUE if line 'lnum' is the start of a section or paragraph.
 * If 'para' is '{' or '}' only check for sections.
 * If 'both' is TRUE also stop at '}'
 */
	int
startPS(linenr_t lnum, int para, int both)
{
	char_u *s;

	s = ml_get(lnum);
	if (*s == para || *s == '\f' || (both && *s == '}'))
		return TRUE;
	if (*s == '.' && (inmacro(p_sections, s + 1) || (!para && inmacro(p_para, s + 1))))
		return TRUE;
	return FALSE;
}

/*
 * The following routines do the word searches performed by the 'w', 'W',
 * 'b', 'B', 'e', and 'E' commands.
 */

/*
 * To perform these searches, characters are placed into one of three
 * classes, and transitions between classes determine word boundaries.
 *
 * The classes are:
 *
 * 0 - white space
 * 1 - letters, digits and underscore
 * 2 - everything else
 */

static int		stype;			/* type of the word motion being performed */

/*
 * cls() - returns the class of character at curwin->w_cursor
 *
 * The 'type' of the current search modifies the classes of characters if a 'W',
 * 'B', or 'E' motion is being done. In this case, chars. from class 2 are
 * reported as class 1 since only white space boundaries are of interest.
 */
	static int
cls(void)
{
	int c;

	c = gchar_cursor();
#ifdef KANJI
	if (ISkanji(c))
	{
		int		ret;

		if ((stype != 0) && (p_ww & 64))
			return 1;
		if ((stype != 0) && (p_ww & 32))
			return JPC_HIRA;
		if ((ret = jpcls(ml_get_cursor())) >= 0)
		{
			if (ret != JPC_KIGOU && stype != 0)
				return JPC_HIRA;
			else
				return ret;
		}
	}
	if (ISkana(c))
	{
		if ((stype != 0) && (p_ww & 64))
			return 1;
		if ((stype != 0) && (p_ww & 32))
			return JPC_HIRA;
		return JPC_KANA;
	}
#endif
	if (c == ' ' || c == '\t' || c == NUL)
		return 0;

	if (isidchar(c))
		return 1;

	/*
	 * If stype is non-zero, report these as class 1.
	 */
	return (stype == 0) ? 2 : 1;
}


/*
 * fwd_word(count, type, eol) - move forward one word
 *
 * Returns TRUE if the cursor was already at the end of the file.
 * If eol is TRUE, last word stops at end of line (for operators).
 */
	int
fwd_word(long count, int type, int eol)
{
	int 		sclass; 	/* starting class */
	int			i;

	stype = type;
	while (--count >= 0)
	{
		sclass = cls();

		/*
		 * We always move at least one character.
		 */
		i = inc_cursor();
		if (i == -1)
			return TRUE;
		if (i == 1 && eol && count == 0)	/* started at last char in line */
			return FALSE;

		if (sclass != 0)
			while (cls() == sclass)
			{
				i = inc_cursor();
				if (i == -1 || (i == 1 && eol && count == 0))
					return FALSE;
			}

		/*
		 * go to next non-white
		 */
		while (cls() == 0)
		{
			/*
			 * We'll stop if we land on a blank line
			 */
			if (curwin->w_cursor.col == 0 && *ml_get(curwin->w_cursor.lnum) == NUL)
				break;

			i = inc_cursor();
			if (i == -1 || (i == 1 && eol && count == 0))
				return FALSE;
		}
	}
	return FALSE;
}

/*
 * bck_word(count, type) - move backward 'count' words
 *
 * Returns TRUE if top of the file was reached.
 */
	int
bck_word(long count, int type)
{
	int 		sclass; 	/* starting class */

	stype = type;
	while (--count >= 0)
	{
		sclass = cls();

		if (dec_cursor() == -1)		/* started at start of file */
			return TRUE;

		if (cls() != sclass || sclass == 0)
		{
			/*
			 * We were at the start of a word. Go back to the end of the prior
			 * word.
			 */
			while (cls() == 0)  /* skip any white space */
			{
				/*
				 * We'll stop if we land on a blank line
				 */
				if (curwin->w_cursor.col == 0 && *ml_get(curwin->w_cursor.lnum) == NUL)
					goto finished;

				if (dec_cursor() == -1)		/* hit start of file, stop here */
					return FALSE;
			}
			sclass = cls();
		}

		/*
		 * Move backward to start of this word.
		 */
		if (skip_chars(sclass, BACKWARD))
				return FALSE;

		inc_cursor();                    /* overshot - forward one */
finished:
		;
	}
	return FALSE;
}

/*
 * end_word(count, type, stop) - move to the end of the word
 *
 * There is an apparent bug in the 'e' motion of the real vi. At least on the
 * System V Release 3 version for the 80386. Unlike 'b' and 'w', the 'e'
 * motion crosses blank lines. When the real vi crosses a blank line in an
 * 'e' motion, the cursor is placed on the FIRST character of the next
 * non-blank line. The 'E' command, however, works correctly. Since this
 * appears to be a bug, I have not duplicated it here.
 *
 * Returns TRUE if end of the file was reached.
 *
 * If stop is TRUE and we are already on the end of a word, move one less.
 */
	int
end_word(long count, int type, int stop)
{
	int 		sclass; 	/* starting class */

	stype = type;
	while (--count >= 0)
	{
		sclass = cls();
		if (inc_cursor() == -1)
			return TRUE;

		/*
		 * If we're in the middle of a word, we just have to move to the end of it.
		 */
		if (cls() == sclass && sclass != 0)
		{
			/*
			 * Move forward to end of the current word
			 */
			if (skip_chars(sclass, FORWARD))
					return TRUE;
		}
		else if (!stop || sclass == 0)
		{
			/*
			 * We were at the end of a word. Go to the end of the next word.
			 */

			if (skip_chars(0, FORWARD))     /* skip any white space */
				return TRUE;

			/*
			 * Move forward to the end of this word.
			 */
			if (skip_chars(cls(), FORWARD))
				return TRUE;
		}
		dec_cursor();                    /* overshot - backward one */
		stop = FALSE;					/* we move only one word less */
	}
	return FALSE;
}

	int
skip_chars(int class, int dir)
{
		while (cls() == class)
			if ((dir == FORWARD ? inc_cursor() : dec_cursor()) == -1)
				return TRUE;
		return FALSE;
}
