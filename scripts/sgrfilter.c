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
 * This was three lines of awk to begin with. The suites already build ptyrun.c
 * because script(1) is a different program on every system, and a filter that
 * has to agree with four awks is the same problem; in C there is nothing left
 * for them to disagree about.
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
static int			pending;			/* a clear waiting to see if anything follows */

static void settle(void);

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

	settle();
	if (nline >= LINEMAX)
		return;
	start[nline++] = olen;
	for (i = 0; i < esclen; i++)
		put(esc[i]);
	put('|');
}

/*
 * The display was cleared: everything written before it is not on the screen
 * any more, so it is not in the answer either -- but only if something is
 * drawn afterwards. Leaving the alternate screen on the way out clears it too,
 * and that clear must not take the answer with it.
 *
 * Waiting for the next thing drawn is the whole of the test. Keying on what an
 * editor says on its way out is not: it sets the window title back, and to
 * what depends on the system. Linux says "Thanks for flying Vim" there and
 * DragonFly says "xterm", which is how this went wrong twice.
 */
static void
clear(void)
{
	pending = 1;
}

static void
settle(void)
{
	if (!pending)
		return;
	pending = 0;
	olen = 0;
	nline = 0;
	start[nline++] = 0;
	put('|');
}

int
main(void)
{
	static char		in[OUTMAX];
	size_t			n = 0;
	size_t			i;
	size_t			got;

	while ((got = fread(in + n, 1, sizeof(in) - n, stdin)) > 0)
	{
		n += got;
		if (n >= sizeof(in))
			break;
	}

	clear();
	for (i = 0; i < n; )
	{
		if (in[i] != ESC)
		{
			/* text: what was actually drawn, less the line ends */
			if (in[i] != '\r' && in[i] != '\n')
			{
				settle();
				put(in[i]);
			}
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
				clear();
			i = j + 1;
		}
		else if (in[i] == ']')
		{
			size_t		j = i;			/* a window title, up to the bell */

			while (j < n && in[j] != BEL)
				j++;
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
