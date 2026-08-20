/* vi:ts=4:sw=4
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *
 * Read the file "credits.txt" for a list of people who contributed.
 * Read the file "uganda.txt" for copying and usage conditions.
 */

#include "vim.h"
#include "globals.h"
#include "proto.h"
#include "param.h"
#ifdef KANJI
#include "kanji.h"
#include "utf8.h"
#endif


	char_u *
transchar(int c)
{
#ifdef KANJI
	static char_u buf[5];
#else
	static char_u buf[3];
#endif

	if (c < ' ' || c == DEL)
	{
		if (c == NL)
			c = NUL;			/* we use newline in place of a NUL */
		buf[0] = '^';
		buf[1] = c ^ 0x40;		/* DEL displayed as ^? */
		buf[2] = NUL;
	}
#ifdef KANJI
	else if (c < 0x80 || ISdisp(c))
#else
	else if (c <= '~' || c > 0xa0 || p_gr)
#endif
	{
		buf[0] = c;
		buf[1] = NUL;
	}
	else
	{
#ifdef KANJI
		buf[0] = '[';
		buf[1] = HexChar((c&0xf0)>>4);
		buf[2] = HexChar(c&0x0f);
		buf[3] = ']';
		buf[4] = NUL;
#else
		buf[0] = '~';
		buf[1] = c - 0x80 + '@';
		buf[2] = NUL;
#endif
	}
	return buf;
}

/*
 * Return the number of screen columns the character at 'p' takes.
 *
 * KANJI: this is per character, not per byte. A trailing byte of a multi-byte
 * character counts 0, so a loop that walks bytes and sums charsize() still ends
 * up with the width of the text.
 */
	int
charsize(char_u *p)
{
#ifdef KANJI
	int		c = *p;

	if (c < ' ' || c == DEL)
		return 2;					/* shown as ^X */
	if (c < 0x80)
		return 1;
	return utf_width(p);			/* 0 for a trailing byte, 4 for junk */
#else
	int		c = *p;

	return ((c >= ' ' && (p_gr || c <= '~')) || c > 0xa0 ? 1 : 2);
#endif
}

/*
 * Columns taken by the transchar() form of a single byte: "^X" is 2, "[XX]" is
 * 4, a printable ASCII byte is 1. For bytes only; use charsize() for text.
 */
	int
transcharsize(int c)
{
#ifdef KANJI
	c &= 0xff;
	return (c < ' ' || c == DEL) ? 2 : (c < 0x80 ? 1 : 4);
#else
	return ((c >= ' ' && (p_gr || c <= '~')) || c > 0xa0 ? 1 : 2);
#endif
}

/*
 * return the number of characters string 's' will take on the screen
 */
	int
strsize(char_u *s)
{
	int	len = 0;

	while (*s)
	{
		len += charsize(s);
		s++;
	}
	return len;
}

/*
 * return the number of characters 'c' will take on the screen, taking
 * into account the size of a tab
 */
	int
chartabsize(char_u *p, long col)
{
	register int	c = *p;

#ifdef KANJI
	if (c >= ' ' &&  c != DEL)
	{
		if (c < 0x80)
			return(1);
		return utf_width(p);	/* 0 for a trailing byte, 4 for junk */
	}
#else
	if ((c >= ' ' && (c <= '~' || p_gr)) || c > 0xa0)
   		return 1;
#endif
   	else if (c == TAB && !curwin->w_p_list)
   		return (int)(curbuf->b_p_ts - (col % curbuf->b_p_ts));
   	else
		return 2;
}

/*
 * return TRUE if 'c' is an identifier character
 */
	int
isidchar(int c)
{
		return (
#ifdef __STDC__
				isalnum(c)
#else
				isalpha(c) || isdigit(c)
#endif
				|| c == '_'
	/*
	 * we also accept alhpa's with accents
	 */
#ifdef MSDOS
				|| (c >= 0x80 && c <= 0xa7) || (c >= 0xe0 && c <= 0xeb)
#else
				|| (c >= 0xc0 && c <= 0xff)
#endif
#ifdef KANJI
				|| ISdisp(c)
#endif
				);
}

#ifndef notdef
/*
 * return TRUE if 'c' is an abbr character
 */
	int
isabchar(int c)
{
		return (
#ifdef __STDC__
				isalnum(c)
#else
				isalpha(c) || isdigit(c)
#endif
				|| c == '_'
				|| (isascii(c) && isgraph(c))
	/*
	 * we also accept alhpa's with accents
	 */
#ifdef MSDOS
				|| (c >= 0x80 && c <= 0xa7) || (c >= 0xe0 && c <= 0xeb)
#else
# ifdef KANJI
				|| ISdisp(c)
# else
				|| (c >= 0xc0 && c <= 0xff)
# endif
#endif
				);
}
#endif
