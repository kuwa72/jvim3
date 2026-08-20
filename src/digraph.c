/* vi:ts=4:sw=4
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *
 * Read the file "credits.txt" for a list of people who contributed.
 * Read the file "uganda.txt" for copying and usage conditions.
 */

#ifdef DIGRAPHS
/*
 * digraph.c: code for digraphs
 */

#include "vim.h"
#include "globals.h"
#include "proto.h"
#include "param.h"

static void printdigraph __ARGS((char_u *));

char_u	(*digraphnew)[3];			/* pointer to added digraphs */
int		digraphcount = 0;			/* number of added digraphs */

#ifdef MSDOS
char_u	digraphdefault[][3] = 		/* standard MSDOS digraphs */
	   {{'C', ',', 128},
		{'u', '"', 129},
		{'e', '\'', 130},
		{'a', '^', 131},
		{'a', '"', 132},
		{'a', '`', 133},
		{'a', '@', 134},
		{'c', ',', 135},
		{'e', '^', 136},
		{'e', '"', 137},
		{'e', '`', 138},
		{'i', '"', 139},
		{'i', '^', 140},
		{'i', '`', 141},
		{'A', '"', 142},
		{'A', '@', 143},
		{'E', '\'', 144},
		{'a', 'e', 145},
		{'A', 'E', 146},
		{'o', '^', 147},
		{'o', '"', 148},
		{'o', '`', 149},
		{'u', '^', 150},
		{'u', '`', 151},
		{'y', '"', 152},
		{'O', '"', 153},
		{'U', '"', 154},
	    {'c', '|', 155},
	    {'$', '$', 156},
	    {'Y', '-', 157},	/* ~] (SAS C can't handle the real char) */
	    {'P', 't', 158},
	    {'f', 'f', 159},
		{'a', '\'', 160},
		{'i', '\'', 161},
		{'o', '\'', 162},
		{'u', '\'', 163},	/* xx (SAS C can't handle the real char) */
		{'n', '~', 164},
		{'N', '~', 165},
		{'a', 'a', 166},
		{'o', 'o', 167},
		{'~', '?', 168},
		{'-', 'a', 169},
		{'a', '-', 170},
		{'1', '2', 171},
		{'1', '4', 172},
		{'~', '!', 173},
		{'<', '<', 174},
		{'>', '>', 175},

		{'s', 's', 225},
		{'j', 'u', 230},
		{'o', '/', 237},
		{'+', '-', 241},
		{'>', '=', 242},
		{'<', '=', 243},
		{':', '-', 246},
		{'~', '~', 247},
		{'~', 'o', 248},
		{'2', '2', 253},
		{NUL, NUL, NUL}
		};

#else	/* MSDOS */

char_u	digraphdefault[][3] = 		/* standard ISO digraphs */
	   {{'~', '!', 161},
	    {'c', '|', 162},
	    {'$', '$', 163},
	    {'o', 'x', 164},
	    {'Y', '-', 165},
	    {'|', '|', 166},
	    {'p', 'a', 167},
	    {'"', '"', 168},
	    {'c', 'O', 169},
		{'a', '-', 170},
		{'<', '<', 171},
		{'-', ',', 172},
		{'-', '-', 173},
		{'r', 'O', 174},
		{'-', '=', 175},
		{'~', 'o', 176},
		{'+', '-', 177},
		{'2', '2', 178},
		{'3', '3', 179},
		{'\'', '\'', 180},
		{'j', 'u', 181},
		{'p', 'p', 182},
		{'~', '.', 183},
		{',', ',', 184},
		{'1', '1', 185},
		{'o', '-', 186},
		{'>', '>', 187},
		{'1', '4', 188},
		{'1', '2', 189},
		{'3', '4', 190},
		{'~', '?', 191},
		{'A', '`', 192},
		{'A', '\'', 193},
		{'A', '^', 194},
		{'A', '~', 195},
		{'A', '"', 196},
		{'A', '@', 197},
		{'A', 'E', 198},
		{'C', ',', 199},
		{'E', '`', 200},
		{'E', '\'', 201},
		{'E', '^', 202},
		{'E', '"', 203},
		{'I', '`', 204},
		{'I', '\'', 205},
		{'I', '^', 206},
		{'I', '"', 207},
		{'D', '-', 208},
		{'N', '~', 209},
		{'O', '`', 210},
		{'O', '\'', 211},
		{'O', '^', 212},
		{'O', '~', 213},
		{'O', '"', 214},
		{'/', '\\', 215},
		{'O', '/', 216},
		{'U', '`', 217},
		{'U', '\'', 218},
		{'U', '^', 219},
		{'U', '"', 220},
		{'Y', '\'', 221},
		{'I', 'p', 222},
		{'s', 's', 223},
		{'a', '`', 224},
		{'a', '\'', 225},
		{'a', '^', 226},
		{'a', '~', 227},
		{'a', '"', 228},
		{'a', '@', 229},
		{'a', 'e', 230},
		{'c', ',', 231},
		{'e', '`', 232},
		{'e', '\'', 233},
		{'e', '^', 234},
		{'e', '"', 235},
		{'i', '`', 236},
		{'i', '\'', 237},
		{'i', '^', 238},
		{'i', '"', 239},
		{'d', '-', 240},
		{'n', '~', 241},
		{'o', '`', 242},
		{'o', '\'', 243},
		{'o', '^', 244},
		{'o', '~', 245},
		{'o', '"', 246},
		{':', '-', 247},
		{'o', '/', 248},
		{'u', '`', 249},
		{'u', '\'', 250},
		{'u', '^', 251},
		{'u', '"', 252},
		{'y', '\'', 253},
		{'i', 'p', 254},
		{'y', '"', 255},
		{NUL, NUL, NUL}
		};
#endif	/* MSDOS */
 
/*
 * handle digraphs after typing a character
 */
	int
dodigraph(int c)
{
	static int	backspaced;		/* character before BS */
	static int	lastchar;		/* last typed character */

	if (c == -1)				/* init values */
	{
		backspaced = -1;
	}
	else if (p_dg)
	{
		if (backspaced >= 0)
			c = getdigraph(backspaced, c, FALSE);
		backspaced = -1;
		if (c == BS && lastchar >= 0)
			backspaced = lastchar;
	}
	lastchar = c;
	return c;
}

/*
 * lookup the pair char1, char2 in the digraph tables
 * if no match, return char2
 */
	int
getdigraph(int char1, int char2, int meta)
{
	int		i;
	int		retval;

	retval = 0;
	for (i = 0; ; ++i)			/* search added digraphs first */
	{
		if (i == digraphcount)	/* end of added table, search defaults */
		{
			for (i = 0; digraphdefault[i][0] != 0; ++i)
				if (digraphdefault[i][0] == char1 && digraphdefault[i][1] == char2)
				{
					retval = digraphdefault[i][2];
					break;
				}
			break;
		}
		if (digraphnew[i][0] == char1 && digraphnew[i][1] == char2)
		{
			retval = digraphnew[i][2];
			break;
		}
	}

	if (retval == 0)			/* digraph deleted or not found */
	{
		if (char1 == ' ' && meta)		/* <space> <char> --> meta-char */
			return (char2 | 0x80);
		return char2;
	}
	return retval;
}

/*
 * put the digraphs in the argument string in the digraph table
 * format: {c1}{c2} char {c1}{c2} char ...
 */
	void
putdigraph(char_u *str)
{
	int		char1, char2, n;
	char_u	(*newtab)[3];
	int		i;

	while (*str)
	{
		skipspace(&str);
		if ((char1 = *str++) == 0 || (char2 = *str++) == 0)
			return;
		if (char1 == ESC || char2 == ESC)
		{
			EMSG("Escape not allowed in digraph");
			return;
		}
		skipspace(&str);
		if (!isdigit(*str))
		{
			emsg(e_number);
			return;
		}
		n = getdigits(&str);
		if (digraphnew)		/* search the table for existing entry */
		{
			for (i = 0; i < digraphcount; ++i)
				if (digraphnew[i][0] == char1 && digraphnew[i][1] == char2)
				{
					digraphnew[i][2] = n;
					break;
				}
			if (i < digraphcount)
				continue;
		}
		newtab = (char_u (*)[3])alloc(digraphcount * 3 + 3);
		if (newtab)
		{
			memmove((char *)newtab, (char *)digraphnew, (size_t)(digraphcount * 3));
			free(digraphnew);
			digraphnew = newtab;
			digraphnew[digraphcount][0] = char1;
			digraphnew[digraphcount][1] = char2;
			digraphnew[digraphcount][2] = n;
			++digraphcount;
		}
	}
}

	void
listdigraphs(void)
{
	int		i;

	printdigraph(NULL);
	msg_start();
	msg_outchar('\n');
	for (i = 0; digraphdefault[i][0] && !got_int; ++i)
	{
		if (getdigraph(digraphdefault[i][0], digraphdefault[i][1], FALSE) == digraphdefault[i][2])
			printdigraph(digraphdefault[i]);
		breakcheck();
	}
	for (i = 0; i < digraphcount && !got_int; ++i)
	{
		printdigraph(digraphnew[i]);
		breakcheck();
	}
	msg_outchar('\n');
	wait_return(TRUE);		/* clear screen, because some digraphs may be wrong,
							 * in which case we messed up NextScreen */
}

	static void
printdigraph(char_u *p)
{
	char_u		buf[9];
	static int	len;

	if (p == NULL)
		len = 0;
	else if (p[2] != 0)
	{
		if (len > Columns - 11)
		{
			msg_outchar('\n');
			len = 0;
		}
		if (len)
			msg_outstr((char_u *)"   ");
		sprintf((char *)buf, "%c%c %c %3d", p[0], p[1], p[2], p[2]);
		msg_outstr(buf);
		len += 11;
	}
}

#endif /* DIGRAPHS */
