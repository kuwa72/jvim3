/*
 * sgrfilter -- what a terminal was sent, as escape-and-text lines.
 *
 * Reads on standard input everything an editor wrote to a terminal and writes
 * one line per SGR escape: the escape, a '|', and the text drawn while it was
 * in force. Text drawn before any escape gets a line with nothing before the
 * '|'. Cursor positioning, the window title and the alternate screen are
 * dropped, so a case does not break when a line moves.
 *
 * scripts/test-sgr.sh is the only thing that uses it.
 *
 * This was three lines of awk to begin with, and awk is where it went wrong.
 * The one true awk, which is what DragonFly has, does not read "\007" inside a
 * bracket expression the way gawk and mawk do; the window title went
 * unrecognised, "Thanks for flying Vim" is a window title, and so the clear
 * that follows it in the teardown was taken for a redraw and threw the whole
 * run away. Every case came out empty on one system out of five and nothing
 * said why. The suites already build ptyrun.c because script(1) is a different
 * program on every system -- this is the same answer to the same problem.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ESC			033
#define BEL			007

/*
 * Room for a screenful many times over. The input is one editor drawing one
 * small file, so these are not close to being reached; they are here so that
 * a runaway cannot walk off the end of either.
 */
#define OUTMAX		(1 << 20)
#define LINEMAX		8192

static char			out[OUTMAX];
static size_t		olen;
static size_t		start[LINEMAX];		/* where each line begins in out */
static int			nline;

static void
put(int c)
{
	if (olen < sizeof(out) - 1)
		out[olen++] = (char)c;
}

/*
 * Begin a line, with 'esc' in front of the '|' -- the empty string for text
 * that was drawn before any escape at all.
 */
static void
newline(const char *esc, size_t esclen)
{
	size_t		i;

	if (nline >= LINEMAX)
		return;
	start[nline++] = olen;
	for (i = 0; i < esclen; i++)
		put(esc[i]);
	put('|');
}

/*
 * Whether a window title is the one an editor sets on its way out. strstr()
 * would want a terminator this has not got.
 */
static int
is_goodbye(const char *p, size_t len)
{
	static const char	bye[] = "Thanks for flying";
	size_t				want = sizeof(bye) - 1;
	size_t				i;

	if (len < want)
		return(0);
	for (i = 0; i + want <= len; i++)
		if (memcmp(p + i, bye, want) == 0)
			return(1);
	return(0);
}

/*
 * The display was cleared: everything written before it is not on the screen
 * any more, so it is not in the answer either.
 */
static void
clear(void)
{
	olen = 0;
	nline = 0;
	newline("", 0);
}

int
main(void)
{
	static char		in[OUTMAX];
	size_t			n = 0;
	size_t			i;
	size_t			got;
	int				done = 0;

	while ((got = fread(in + n, 1, sizeof(in) - n, stdin)) > 0)
	{
		n += got;
		if (n >= sizeof(in))
			break;
	}

	clear();
	for (i = 0; i < n && !done; )
	{
		if (in[i] != ESC)
		{
			/* text: what was actually drawn, less the line ends */
			if (in[i] != '\r' && in[i] != '\n')
				put(in[i]);
			i++;
			continue;
		}
		i++;								/* past the escape */
		if (i >= n)
			break;
		if (in[i] == '[')
		{
			size_t		j = i + 1;

			while (j < n && (('0' <= in[j] && in[j] <= '9')
								|| in[j] == ';' || in[j] == '?'))
				j++;
			if (j >= n)
				break;
			if (in[j] == 'm')				/* a colour: start a line with it */
				newline(in + i, j - i + 1);
			else if (in[j] == 'J' && j - i == 2 && in[i + 1] == '2')
				clear();					/* and everything before it goes */
			i = j + 1;
		}
		else if (in[i] == ']')
		{
			/*
			 * A window title. The last one an editor sets on the way out says
			 * "Thanks for flying Vim", and the clear that follows it is the
			 * teardown -- past that there is nothing left to read.
			 */
			size_t		j = i;

			while (j < n && in[j] != BEL)
				j++;
			if (is_goodbye(in + i, j - i))
				done = 1;
			i = (j < n) ? j + 1 : n;
		}
		else
			i++;							/* keypad, save, restore */
	}

	for (i = 0; i < (size_t)nline; i++)
	{
		size_t		from = start[i];
		size_t		to   = (i + 1 < (size_t)nline) ? start[i + 1] : olen;
		size_t		k;

		/* the filler down the left of an empty window, and all that follows */
		for (k = from; k < to; k++)
			if (out[k] == '~')
			{
				to = k;
				break;
			}
		if (to - from <= 1 && (to == from || out[from] == '|'))
			continue;						/* neither escape nor text */
		fwrite(out + from, 1, to - from, stdout);
		putchar('\n');
	}
	return 0;
}
