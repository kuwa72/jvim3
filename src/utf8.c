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

typedef struct { int first, last; } utf_range;

/*
 * East Asian Wide and Fullwidth: two columns. Sorted ranges, binary searched.
 * Derived from EastAsianWidth.txt (W and F); ranges are merged where the gaps
 * only hold unassigned code points, which keeps the table small without
 * changing the answer for assigned characters.
 */
static utf_range utf_wide[] = {
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
 * East Asian Ambiguous (the A class of EastAsianWidth.txt): one column in a
 * Western context and two in an East Asian one, which is not a contradiction
 * so much as a record of history. The class was drawn up from the legacy CJK
 * charsets, so it is very nearly the set of characters CP932 and its cousins
 * encode as double byte -- and a Japanese font draws exactly those double
 * width. An arrow is the everyday case: U+2192 is CP932 0x81A8, and MS Gothic,
 * Myrica and the rest give it a full width glyph. Called one column it lands on
 * top of whatever follows it.
 *
 * So: two, which is what 'ambiwidth' set to double means elsewhere.
 *
 * Deliberately not here, though the A class has them, is everything below
 * U+2000 that is a letter rather than a symbol -- Latin-1, Latin Extended, IPA,
 * the spacing modifiers. Those come out of the Latin half of a mixed Japanese
 * font, half width, and calling them double would push Western text apart to
 * no purpose. Greek and Cyrillic are in, because a Japanese font takes those
 * from its CJK half and draws them full width, CP932 having them in JIS X 0208
 * rows 6 and 7.
 */
static utf_range utf_ambig[] = {
	{0x0391, 0x03a1}, {0x03a3, 0x03a9},		/* Greek capitals */
	{0x03b1, 0x03c1}, {0x03c3, 0x03c9},		/* Greek smalls */
	{0x0401, 0x0401}, {0x0410, 0x044f}, {0x0451, 0x0451},	/* Cyrillic */
	{0x2010, 0x2010}, {0x2013, 0x2016}, {0x2018, 0x2019},
	{0x201c, 0x201d}, {0x2020, 0x2022}, {0x2024, 0x2027},
	{0x2030, 0x2030}, {0x2032, 0x2033}, {0x2035, 0x2035},
	{0x203b, 0x203b}, {0x203e, 0x203e},
	{0x2074, 0x2074}, {0x207f, 0x207f}, {0x2081, 0x2084},
	{0x20ac, 0x20ac},
	{0x2103, 0x2103}, {0x2105, 0x2105}, {0x2109, 0x2109},
	{0x2113, 0x2113}, {0x2116, 0x2116}, {0x2121, 0x2122},
	{0x2126, 0x2126}, {0x212b, 0x212b},
	{0x2153, 0x2154}, {0x215b, 0x215e},
	{0x2160, 0x216b}, {0x2170, 0x2179}, {0x2189, 0x2189},
	{0x2190, 0x2199}, {0x21b8, 0x21b9}, {0x21d2, 0x21d2},
	{0x21d4, 0x21d4}, {0x21e7, 0x21e7},
	{0x2200, 0x2200}, {0x2202, 0x2203}, {0x2207, 0x2208},
	{0x220b, 0x220b}, {0x220f, 0x220f}, {0x2211, 0x2211},
	{0x2215, 0x2215}, {0x221a, 0x221a}, {0x221d, 0x2220},
	{0x2223, 0x2223}, {0x2225, 0x2225}, {0x2227, 0x222c},
	{0x222e, 0x222e}, {0x2234, 0x2237}, {0x223c, 0x223d},
	{0x2248, 0x2248}, {0x224c, 0x224c}, {0x2252, 0x2252},
	{0x2260, 0x2261}, {0x2264, 0x2267}, {0x226a, 0x226b},
	{0x226e, 0x226f}, {0x2282, 0x2283}, {0x2286, 0x2287},
	{0x2295, 0x2295}, {0x2299, 0x2299}, {0x22a5, 0x22a5},
	{0x22bf, 0x22bf}, {0x2312, 0x2312},
	{0x2460, 0x24e9}, {0x24eb, 0x254b},		/* enclosed, box drawing */
	{0x2550, 0x2573}, {0x2580, 0x258f}, {0x2592, 0x2595},
	{0x25a0, 0x25a1}, {0x25a3, 0x25a9}, {0x25b2, 0x25b3},
	{0x25b6, 0x25b7}, {0x25bc, 0x25bd}, {0x25c0, 0x25c1},
	{0x25c6, 0x25c8}, {0x25cb, 0x25cb}, {0x25ce, 0x25d1},
	{0x25e2, 0x25e5}, {0x25ef, 0x25ef},
	{0x2605, 0x2606}, {0x2609, 0x2609}, {0x260e, 0x260f},
	{0x261c, 0x261c}, {0x261e, 0x261e},
	{0x2640, 0x2640}, {0x2642, 0x2642},
	{0x2660, 0x2661}, {0x2663, 0x2665}, {0x2667, 0x266a},
	{0x266c, 0x266d}, {0x266f, 0x266f},
	{0x269e, 0x269f}, {0x26bf, 0x26bf}, {0x26c6, 0x26cd},
	{0x26cf, 0x26d3}, {0x26d5, 0x26e1}, {0x26e3, 0x26e3},
	{0x26e8, 0x26e9}, {0x26eb, 0x26f1}, {0x26f4, 0x26f4},
	{0x26f6, 0x26f9}, {0x26fb, 0x26fc}, {0x26fe, 0x26ff},
	{0x273d, 0x273d}, {0x2776, 0x277f},
	{0x2b56, 0x2b59},
	{0x3248, 0x324f},
	{0xe000, 0xf8ff},						/* private use */
	{0xfffd, 0xfffd},
	{0xf0000, 0xffffd}, {0x100000, 0x10fffd},	/* private use, planes 15-16 */
};

/*
 * Is 'cp' inside one of the sorted ranges of 'tab'?
 */
	static int
utf_inranges(int cp, utf_range *tab, int n)
{
	int		lo = 0;
	int		hi = n - 1;
	int		mid;

	while (lo <= hi)
	{
		mid = (lo + hi) / 2;
		if (cp < tab[mid].first)
			hi = mid - 1;
		else if (cp > tab[mid].last)
			lo = mid + 1;
		else
			return TRUE;
	}
	return FALSE;
}

/*
 * Display width of a code point, in columns.
 */
	int
utf_cpwidth(int cp)
{
	/*
	 * The cut used to be at 0x1100, which was quick but stepped in front of
	 * the zero width check below: every combining mark under that -- the Latin
	 * diacriticals, Hebrew points, Arabic harakat, the Thai marks -- was given
	 * a column of its own, which is not what utf_iszerowidth() lists them for.
	 */
	if (cp < 0x0300)					/* the common case, no table needed */
		return 1;
	if (utf_iszerowidth(cp))
		return 0;
	if (utf_inranges(cp, utf_wide,
						(int)(sizeof(utf_wide) / sizeof(utf_wide[0]))))
		return 2;
	if (utf_inranges(cp, utf_ambig,
						(int)(sizeof(utf_ambig) / sizeof(utf_ambig[0]))))
		return 2;
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
