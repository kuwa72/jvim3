/* vi:ts=4:sw=4
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *
 * Read the file "credits.txt" for a list of people who contributed.
 * Read the file "uganda.txt" for copying and usage conditions.
 */

/*
 * help.c: display help from the vim.hlp file
 */

#include "vim.h"
#include "globals.h"
#include "proto.h"
#include "param.h"
#ifdef NT
# include <windows.h>
#endif

static long helpfilepos;		/* position in help file */
static FILE *helpfd;			/* file descriptor of help file */

#ifdef KANJI
#include "kanji.h"
static char_u *	helpmem;			/* file descriptor of help file */
static int		helpsize;
static int		helppos;

static char_u *
kopen(char_u *fnamep, char *type)
{
	int			w_jkc = p_jkc;
	int			len;
	FILE	*	fp;
	char_u	*	wp;
	int			c;
	int			i;
	char_u		code  = JP_SYS;
	int			ubig;

	if ((fp = fopen((char *)fnamep, type)) == NULL)
		return(NULL);
	fseek(fp, 0, 2);
	len = ftell(fp);
	if ((wp = malloc(len + 1)) == NULL)
	{
		fclose(fp);
		return(NULL);
	}
	if ((helpmem = malloc(len + 1)) == NULL)
	{
		free(wp);
		fclose(fp);
		return(NULL);
	}
	fseek(fp, 0, 0);
	i = 0;
	while ((c = fgetc(fp)) != EOF)
	{
		wp[i] = c;
		i++;
	}
	wp[len] = NUL;
	fclose(fp);
	code = judge_jcode(&code, &ubig, wp, len);
# ifdef UCODE
	if (toupper(code) == JP_WIDE)	/* UNICODE */
		len = wide2multi(wp, len, ubig, TRUE);
	/* MS UTF8 */
	else if (toupper(code) == JP_UTF8
			&& (wp[0] == 0xef && wp[1] == 0xbb && wp[2] == 0xbf))
	{
		len -= 3;
		memmove((char *)wp, (char *)wp + 3, len);
	}
# endif
	p_jkc = FALSE;
	helpsize = kanjiconvsfrom(wp, len, helpmem, len, NULL, code, NULL);
	helpmem[helpsize] = NUL;
	p_jkc = w_jkc;
	free(wp);
	return(helpmem);
}

static void
kclose(char_u *mp)
{
	free(mp);
	helpsize = helppos = 0;
}

long int
ktell(char_u *mp)
{
	return(helppos);
}

int
kseek(char_u *mp, long int offset, int wherefrom)
{
	helppos = offset;
	return(0);
}

int
kgetc(char_u *mp)
{
	int		c;

	if (helppos >= helpsize)
		c = EOF;
	else
	{
		c = helpmem[helppos];
		helppos++;
	}
	return(c);
}
#endif

#define MAXSCREENS 52			/* one screen for a-z and A-Z */

	void
help(void)
{
	int		c;
	int		eof;
	int		screens;
	int		i;
	long	filepos[MAXSCREENS];	/* seek position for each screen */
	int		screennr;			/* screen number; index == 0, 'c' == 1, 'd' == 2, etc */
#ifdef MSDOS
	char_u	*fnamep;
#endif

/*
 * try to open the file specified by the "helpfile" option
 */
#ifdef KANJI
	if ((helpmem = kopen((char *)p_hf, READBIN)) == NULL)
#else
	if ((helpfd = fopen((char *)p_hf, READBIN)) == NULL)
#endif
	{
#if !defined(__GO32__)
# ifdef MSDOS
	/*
	 * for MSDOS: try the DOS search path
     */
#  ifndef NT
		fnamep = searchpath("vim.hlp");
#  else
		{
			/*
			 * SearchPathW, not SearchPathA: the ANSI one stops at 260
			 * characters (see FullName() in winjnt.c). The name comes back as
			 * UTF-8, which is what the rest of this build hands to kopen().
			 */
			static char		buf[MAXPATHL];
			WCHAR			w[MAXPATHL];
			WCHAR		*	tail;
			DWORD			len;

			len = SearchPathW(NULL, L"vim.hlp", NULL,
								(DWORD)(sizeof(w) / sizeof(w[0])), w, &tail);
			if (len == 0 || len >= (DWORD)(sizeof(w) / sizeof(w[0]))
					|| WideCharToMultiByte(CP_UTF8, 0, w, -1, buf,
											sizeof(buf), NULL, NULL) <= 0)
				fnamep = NULL;
			else
				fnamep = buf;
		}
#  endif
#  ifdef KANJI
		if (fnamep == NULL || (helpmem = kopen((char *)fnamep, READBIN)) == NULL)
#  else
		if (fnamep == NULL || (helpfd = fopen((char *)fnamep, READBIN)) == NULL)
#  endif
		{
			smsg((char_u *)"Sorry, help file \"%s\" and \"vim.hlp\" not found", p_hf);
			return;
		}
# else
		smsg((char_u *)"Sorry, help file \"%s\" not found", p_hf);
		return;
# endif
#endif
	}
	helpfilepos = 0;
	screennr = 0;
	for (i = 0; i < MAXSCREENS; ++i)
		filepos[i] = 0;
	State = HELP;
	for (;;)
	{
#ifdef NT
		if (GuiWin)
			screenclear();
#endif
		screens = redrawhelp();				/* show one or more screens */
		eof = (screens < 0);
		if (!eof && screennr + screens < MAXSCREENS)
#ifdef KANJI
			filepos[screennr + screens] = ktell(helpmem);
#else
			filepos[screennr + screens] = ftell(helpfd);
#endif

		if ((c = vgetc()) == '\n' || c == '\r' || c == Ctrl('C') || c == ESC)
			break;

		if (c == ' ' ||
#ifdef KANJI
				(c == K_SDARROW) ||	/* page down */
#else
# ifdef MSDOS
				(c == K_NUL && vpeekc() == 'Q') ||	/* page down */
# endif
#endif
				c == Ctrl('F'))						/* one screen forwards */
		{
			if (screennr < MAXSCREENS && !eof)
				++screennr;
		}
		else if (c == 'a')					/* go to first screen */
			screennr = 0;
		else if (c == 'b' ||
#ifdef KANJI
				(c == K_SUARROW) ||	/* page up */
#else
# ifdef MSDOS
				(c == K_NUL && vpeekc() == 'I') ||	/* page up */
# endif
#endif
				c == Ctrl('B'))					/* go one screen backwards */
		{
			if (screennr > 0)
				--screennr;
		}
		else if (isalpha(c))				/* go to specified screen */
		{
			if (isupper(c))
				c = c - 'A' + 'z' + 1;		/* 'A' comes after 'z' */
			screennr = c - 'b';
		}
#ifndef KANJI
# ifdef MSDOS
		if (c == K_NUL)
			c = vgetc();
# endif
#endif
		for (i = screennr; i > 0; --i)
			if (filepos[i])
				break;
#ifdef KANJI
		kseek(helpmem, filepos[i], 0);
#else
		fseek(helpfd, filepos[i], 0);
#endif
		while (i < screennr)
		{
#ifdef KANJI
			while ((c = kgetc(helpmem)) != '\f' && c != -1)
#else
			while ((c = getc(helpfd)) != '\f' && c != -1)
#endif
				;
			if (c == -1)
				break;
#ifdef KANJI
			filepos[++i] = ktell(helpmem);	/* store the position just after the '\f' */
#else
			filepos[++i] = ftell(helpfd);	/* store the position just after the '\f' */
#endif
		}
		screennr = i;						/* required when end of file reached */
		helpfilepos = filepos[screennr];
	}
	State = NORMAL;
#ifdef KANJI
	kclose(helpmem);
#else
	fclose(helpfd);
#endif
	updateScreen(CLEAR);
}

/*
 * redraw the help info for the current position in the help file
 *
 * return the number of screens displayed, or -1 if end of file reached
 */
	int
redrawhelp(void)
{
	int nextc;
	int col = 0;
	int	line = 0;
	int	screens = 1;

#ifdef KANJI
	kseek(helpmem, helpfilepos, 0);
#else
	fseek(helpfd, helpfilepos, 0);
#endif
	outstr(T_ED);
#ifdef NT
	if (GuiWin)
	{
		IObuff[0] = NUL;
		msg_pos(0, 0);
	}
#endif
	(void)set_highlight('h');
	windgoto(0,0);
#ifdef KANJI
	while ((nextc = kgetc(helpmem)) != -1 && (nextc != '\f' || line < Rows - 24))
#else
	while ((nextc = getc(helpfd)) != -1 && (nextc != '\f' || line < Rows - 24))
#endif
	{
		if (nextc == Ctrl('B'))			/* begin of standout */
		{
#ifdef NT
			if (GuiWin)
			{
				IObuff[col] = '\0';
				if (IObuff[0] != NUL)
					msg_outstr(IObuff);
				col = 0;
			}
#endif
			start_highlight();
		}
		else if (nextc == Ctrl('E'))	/* end of standout */
		{
#ifdef NT
			if (GuiWin)
			{
				IObuff[col] = '\0';
				msg_outstr(IObuff);
				col = 0;
			}
#endif
			stop_highlight();
		}
		else if (nextc == '\f')			/* start of next screen */
		{
			++screens;
#ifdef NT
			if (GuiWin)
			{
				IObuff[col] = '\0';
				msg_outstr(IObuff);
				col = 0;
			}
			else
#endif
			outchar('\n');
			++line;
		}
		else
		{
#ifdef NT
			if (!GuiWin)
#endif
#ifndef notdef
			if (nextc != '\n')
#endif
			outchar(nextc);
			if (nextc == '\n')
			{
#ifdef NT
				if (GuiWin)
				{
					IObuff[--col] = '\0';
					msg_outstr(IObuff);
					col = 0;
				}
#endif
				++line;
#ifndef notdef
# ifdef NT
				if (GuiWin)
				{
					msg_pos(line, 0);
					msg_ceol();
				}
				else
# endif
				windgoto(line, 0);
#endif
			}
#ifdef NT
			else if (GuiWin)
				IObuff[col++] = nextc;
#endif
		}
	}
#ifdef NT
	if (GuiWin)
	{
# ifdef KANJI
		msg_pos(0, (int)(Columns - STRLEN(JpVersion) - 1));
		msg_outstr(JpVersion);
# else
		msg_pos(0, (int)(Columns - STRLEN(Version) - 1));
		msg_outstr(Version);
# endif
		col = (int)Columns - 52;
		if (col < 0)
			col = 0;
		msg_pos((int)Rows - 1, col);
		msg_outstr("<space = next; return = quit; a = index; b = back>");
		return (nextc == -1 ? -1 : screens);
	}
#endif
#ifdef KANJI
	windgoto(0, (int)(Columns - STRLEN(JpVersion) - 1));
	outstrn(JpVersion);
#else
	windgoto(0, (int)(Columns - STRLEN(Version) - 1));
	outstrn(Version);
#endif
	col = (int)Columns - 52;
	if (col < 0)
		col = 0;
	windgoto((int)Rows - 1, col);
	OUTSTRN("<space = next; return = quit; a = index; b = back>");
	return (nextc == -1 ? -1 : screens);
}
