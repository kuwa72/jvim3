/* vi:ts=4:sw=4
 *
 * utf8.c
 *
 * UTF-8 primitives for the internal representation: byte classification,
 * character length, encode/decode, display width, and stepping over
 * characters. See utf8.h for the classification the rest of the editor
 * relies on.
 */

#include "vim.h"
#include "globals.h"
#include "proto.h"
#include "utf8.h"

#ifdef KANJI

/*
 * Number of bytes in the character starting with lead byte c.
 * A byte that cannot start a character counts as one byte, so that malformed
 * input is stepped over rather than looped on.
 */
	int
utf_len(int c)
{
	unsigned	b = (unsigned)c & 0xff;

	if (b < 0x80)
		return 1;
	if (b < 0xc2)			/* continuation byte, or overlong lead */
		return 1;
	if (b < 0xe0)
		return 2;
	if (b < 0xf0)
		return 3;
	if (b < 0xf5)
		return 4;
	return 1;
}

/*
 * Byte class: UTF8_ASCII, UTF8_LEAD or UTF8_TAIL.
 */
	int
utf_class(int c)
{
	if (UTF8_ISTAIL(c))
		return UTF8_TAIL;
	if (UTF8_ISLEAD(c))
		return UTF8_LEAD;
	return UTF8_ASCII;
}

/*
 * Decode the character at ptr. Returns the code point, or UTF8_ERROR when the
 * sequence is malformed or truncated. When lenp is not NULL it gets the number
 * of bytes consumed (always at least 1, so callers can make progress).
 */
	int
utf_decode(char_u *ptr, int *lenp)
{
	int		len;
	int		i;
	int		cp;

	if (ptr == NULL || *ptr == NUL)
	{
		if (lenp != NULL)
			*lenp = 1;
		return UTF8_ERROR;
	}
	if (*ptr < 0x80)
	{
		if (lenp != NULL)
			*lenp = 1;
		return *ptr;
	}
	len = utf_len(*ptr);
	if (len == 1)
	{
		if (lenp != NULL)
			*lenp = 1;
		return UTF8_ERROR;
	}
	switch (len)
	{
	case 2:		cp = *ptr & 0x1f; break;
	case 3:		cp = *ptr & 0x0f; break;
	default:	cp = *ptr & 0x07; break;
	}
	for (i = 1; i < len; i++)
	{
		if (!UTF8_ISTAIL(ptr[i]))
		{							/* truncated: consume only what is valid */
			if (lenp != NULL)
				*lenp = 1;
			return UTF8_ERROR;
		}
		cp = (cp << 6) | (ptr[i] & 0x3f);
	}
	/* Reject overlong forms, surrogates and out of range values. */
	if ((len == 2 && cp < 0x80)
			|| (len == 3 && cp < 0x800)
			|| (len == 4 && cp < 0x10000)
			|| (cp >= 0xd800 && cp <= 0xdfff)
			|| cp > 0x10ffff)
	{
		if (lenp != NULL)
			*lenp = 1;
		return UTF8_ERROR;
	}
	if (lenp != NULL)
		*lenp = len;
	return cp;
}

/*
 * Encode code point cp into buf, which needs UTF8_MAXLEN bytes. Returns the
 * number of bytes written. An unencodable value becomes '?', so the caller
 * always advances.
 */
	int
utf_encode(int cp, char_u *buf)
{
	if (cp < 0)
		goto bad;
	if (cp < 0x80)
	{
		buf[0] = cp;
		return 1;
	}
	if (cp < 0x800)
	{
		buf[0] = 0xc0 | (cp >> 6);
		buf[1] = 0x80 | (cp & 0x3f);
		return 2;
	}
	if (cp < 0x10000)
	{
		if (cp >= 0xd800 && cp <= 0xdfff)
			goto bad;
		buf[0] = 0xe0 | (cp >> 12);
		buf[1] = 0x80 | ((cp >> 6) & 0x3f);
		buf[2] = 0x80 | (cp & 0x3f);
		return 3;
	}
	if (cp <= 0x10ffff)
	{
		buf[0] = 0xf0 | (cp >> 18);
		buf[1] = 0x80 | ((cp >> 12) & 0x3f);
		buf[2] = 0x80 | ((cp >> 6) & 0x3f);
		buf[3] = 0x80 | (cp & 0x3f);
		return 4;
	}
bad:
	buf[0] = '?';
	return 1;
}

/*
 * Zero width: combining marks and the like. Keeping this separate from the
 * wide table makes both readable.
 */
	static int
utf_iszerowidth(int cp)
{
	return ((cp >= 0x0300 && cp <= 0x036f)		/* combining diacriticals */
			|| (cp >= 0x0483 && cp <= 0x0489)
			|| (cp >= 0x0591 && cp <= 0x05bd)
			|| (cp >= 0x0610 && cp <= 0x061a)
			|| (cp >= 0x064b && cp <= 0x065f)
			|| (cp >= 0x0e31 && cp <= 0x0e3a)
			|| (cp >= 0x1ab0 && cp <= 0x1aff)
			|| (cp >= 0x1dc0 && cp <= 0x1dff)
			|| (cp >= 0x20d0 && cp <= 0x20f0)
			|| (cp >= 0xfe00 && cp <= 0xfe0f)	/* variation selectors */
			|| (cp >= 0xfe20 && cp <= 0xfe2f)
			|| cp == 0x200b || cp == 0x200c || cp == 0x200d
			|| cp == 0xfeff						/* BOM in mid text */
			|| (cp >= 0xe0100 && cp <= 0xe01ef));
}

/*
 * East Asian Wide and Fullwidth: two columns. Sorted ranges, binary searched.
 * Derived from EastAsianWidth.txt (W and F); ranges are merged where the gaps
 * only hold unassigned code points, which keeps the table small without
 * changing the answer for assigned characters.
 */
static struct { int first, last; } utf_wide[] = {
	{0x1100, 0x115f},		/* Hangul Jamo initial consonants */
	{0x231a, 0x231b},
	{0x2329, 0x232a},
	{0x23e9, 0x23ec}, {0x23f0, 0x23f0}, {0x23f3, 0x23f3},
	{0x25fd, 0x25fe},
	{0x2614, 0x2615},
	{0x2648, 0x2653},
	{0x267f, 0x267f}, {0x2693, 0x2693}, {0x26a1, 0x26a1},
	{0x26aa, 0x26ab}, {0x26bd, 0x26be}, {0x26c4, 0x26c5},
	{0x26ce, 0x26ce}, {0x26d4, 0x26d4}, {0x26ea, 0x26ea},
	{0x26f2, 0x26f3}, {0x26f5, 0x26f5}, {0x26fa, 0x26fa},
	{0x26fd, 0x26fd}, {0x2705, 0x2705}, {0x270a, 0x270b},
	{0x2728, 0x2728}, {0x274c, 0x274c}, {0x274e, 0x274e},
	{0x2753, 0x2755}, {0x2757, 0x2757}, {0x2795, 0x2797},
	{0x27b0, 0x27b0}, {0x27bf, 0x27bf}, {0x2b1b, 0x2b1c},
	{0x2b50, 0x2b50}, {0x2b55, 0x2b55},
	{0x2e80, 0x303e},		/* CJK radicals .. CJK symbols */
	{0x3041, 0x33ff},		/* kana, bopomofo, hangul compat, CJK compat */
	{0x3400, 0x4dbf},		/* CJK ext A */
	{0x4e00, 0xa4cf},		/* CJK unified, Yi */
	{0xa960, 0xa97f},		/* Hangul Jamo ext A */
	{0xac00, 0xd7a3},		/* Hangul syllables */
	{0xf900, 0xfaff},		/* CJK compatibility ideographs */
	{0xfe10, 0xfe19},		/* vertical forms */
	{0xfe30, 0xfe6f},		/* CJK compat forms, small form variants */
	{0xff00, 0xff60},		/* fullwidth forms */
	{0xffe0, 0xffe6},		/* fullwidth signs */
	{0x16fe0, 0x16fe4}, {0x16ff0, 0x16ff1},
	{0x17000, 0x18d08},		/* Tangut, Khitan */
	{0x1aff0, 0x1b16f},		/* Kana extensions */
	{0x1f004, 0x1f004}, {0x1f0cf, 0x1f0cf}, {0x1f18e, 0x1f18e},
	{0x1f191, 0x1f19a}, {0x1f200, 0x1f320}, {0x1f32d, 0x1f335},
	{0x1f337, 0x1f37c}, {0x1f37e, 0x1f393}, {0x1f3a0, 0x1f3ca},
	{0x1f3cf, 0x1f3d3}, {0x1f3e0, 0x1f3f0}, {0x1f3f4, 0x1f3f4},
	{0x1f3f8, 0x1f43e}, {0x1f440, 0x1f440}, {0x1f442, 0x1f4fc},
	{0x1f4ff, 0x1f53d}, {0x1f54b, 0x1f54e}, {0x1f550, 0x1f567},
	{0x1f57a, 0x1f57a}, {0x1f595, 0x1f596}, {0x1f5a4, 0x1f5a4},
	{0x1f5fb, 0x1f64f}, {0x1f680, 0x1f6c5}, {0x1f6cc, 0x1f6cc},
	{0x1f6d0, 0x1f6d2}, {0x1f6d5, 0x1f6d7}, {0x1f6dc, 0x1f6df},
	{0x1f6eb, 0x1f6ec}, {0x1f6f4, 0x1f6fc}, {0x1f7e0, 0x1f7f0},
	{0x1f90c, 0x1f93a}, {0x1f93c, 0x1f945}, {0x1f947, 0x1f9ff},
	{0x1fa70, 0x1faff}, {0x20000, 0x3fffd},		/* CJK ext B .. */
};

/*
 * Display width of a code point, in columns.
 */
	int
utf_cpwidth(int cp)
{
	int		lo, hi, mid;

	if (cp < 0x1100)					/* the common case, no table needed */
		return 1;
	if (utf_iszerowidth(cp))
		return 0;
	lo = 0;
	hi = (int)(sizeof(utf_wide) / sizeof(utf_wide[0])) - 1;
	while (lo <= hi)
	{
		mid = (lo + hi) / 2;
		if (cp < utf_wide[mid].first)
			hi = mid - 1;
		else if (cp > utf_wide[mid].last)
			lo = mid + 1;
		else
			return 2;
	}
	return 1;
}

/*
 * Display width of the character at ptr.
 *
 * A continuation byte is 0 so that summing over bytes gives the width of the
 * text. A malformed byte is shown as "[XX]" by transchar(), so it is 4.
 */
	int
utf_width(char_u *ptr)
{
	int		cp;
	int		len;

	if (ptr == NULL || *ptr == NUL)
		return 0;
	if (UTF8_ISTAIL(*ptr))
		return 0;
	if (*ptr < 0x80)
		return 1;						/* caller handles control chars */
	cp = utf_decode(ptr, &len);
	if (cp == UTF8_ERROR)
		return 4;						/* displayed as [XX] */
	return utf_cpwidth(cp);
}

/*
 * Start of the character that the byte at ptr belongs to. base is the start of
 * the string, so we never walk off the front.
 */
	char_u *
utf_head(char_u *base, char_u *ptr)
{
	char_u	*p = ptr;

	while (p > base && UTF8_ISTAIL(*p))
		p--;
	return p;
}

/*
 * Start of the character before the one at ptr, or base.
 */
	char_u *
utf_prev(char_u *base, char_u *ptr)
{
	if (ptr <= base)
		return base;
	return utf_head(base, ptr - 1);
}

/*
 * Byte offset of the start of the character containing offset col, counted
 * from the start of str. Used to snap a column onto a character boundary.
 */
	int
utf_headoff(char_u *str, int col)
{
	char_u	*p;

	if (str == NULL || col <= 0)
		return 0;
	p = utf_head(str, str + col);
	return (int)(p - str);
}

/*
 * Length in bytes of the character at str[col], at least 1.
 */
	int
utf_lenat(char_u *str, int col)
{
	int		len;
	int		i;

	if (str == NULL || str[col] == NUL)
		return 1;
	len = utf_len(str[col]);
	/*
	 * Stop at the first byte that is not a continuation byte, so a truncated
	 * sequence consumes only the part that is really there.
	 */
	for (i = 1; i < len; i++)
		if (!UTF8_ISTAIL(str[col + i]))
			return i;
	return len;
}

/*
 * Is the character at ptr a kana? Covers hiragana, katakana and the halfwidth
 * katakana block, which used to be the one-byte kana of Shift-JIS.
 */
	int
utf_iskana(char_u *ptr)
{
	int		cp = utf_decode(ptr, NULL);

	if (cp == UTF8_ERROR)
		return 0;
	return ((cp >= 0x3041 && cp <= 0x309f)		/* hiragana */
			|| (cp >= 0x30a0 && cp <= 0x30ff)	/* katakana */
			|| (cp >= 0x31f0 && cp <= 0x31ff)	/* katakana phonetic ext */
			|| (cp >= 0xff61 && cp <= 0xff9f));	/* halfwidth katakana */
}

/*
 * Number of characters (not bytes) in str.
 */
	int
utf_strlen(char_u *str)
{
	int		n = 0;

	if (str == NULL)
		return 0;
	while (*str != NUL)
	{
		str += utf_len(*str);
		n++;
	}
	return n;
}

#endif /* KANJI */
