/*
 * hostilegen.c: write one piece of hostile input to stdout, for
 * scripts/test-hostile.sh.
 *
 *   hostilegen repeat <string> <count>   the string, count times
 *   hostilegen allbytes <count>          0x00 .. 0xff, count times
 *   hostilegen badutf                    invalid and truncated UTF-8
 *
 * A C program and not awk, dd or python. The suites run on five operating
 * systems: their awks disagree about what printf "%c" does with a zero, dd
 * with bs=1 for two million bytes is slow enough to notice, and python is not
 * on every guest. The other suites already compile ptyrun.c and sgrfilter.c
 * with ${CC:-cc}, so one more costs nothing. sgrfilter.c is there for the same
 * reason: a suite that fails on one BSD and nowhere else, because of how its
 * awk reads a byte, costs a round trip through CI to work out.
 *
 * Nothing here reads a file or takes a path. It writes to stdout and the
 * caller redirects, so a mistake in a test cannot overwrite anything.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

	static void
usage(void)
{
	fprintf(stderr,
		"usage: hostilegen repeat <string> <count>\n"
		"       hostilegen allbytes <count>\n"
		"       hostilegen badutf\n");
	exit(2);
}

/* A count that has to be a positive number and has to fit. */
	static long
count_arg(const char *s)
{
	char	*end;
	long	n;

	n = strtol(s, &end, 10);
	if (end == s || *end != '\0' || n <= 0)
		usage();
	return n;
}

	int
main(int argc, char **argv)
{
	long	i, n;

	if (argc < 2)
		usage();

	if (strcmp(argv[1], "repeat") == 0)
	{
		if (argc != 4)
			usage();
		n = count_arg(argv[3]);
		for (i = 0; i < n; i++)
			if (fputs(argv[2], stdout) == EOF)
				return 1;
	}
	else if (strcmp(argv[1], "allbytes") == 0)
	{
		if (argc != 3)
			usage();
		n = count_arg(argv[2]);
		for (i = 0; i < n; i++)
		{
			int c;

			for (c = 0; c < 256; c++)
				if (putc(c, stdout) == EOF)
					return 1;
		}
	}
	else if (strcmp(argv[1], "badutf") == 0)
	{
		/*
		 * One valid character to start, so the file is recognisably UTF-8,
		 * then the three ways it can be wrong: a byte that never begins a
		 * sequence (0xff), a continuation byte with nothing to continue
		 * (0x80), and a two byte sequence with its second byte missing --
		 * once in the middle of the file and once cut off by the end of it,
		 * which is the case that has nowhere to look for the rest.
		 */
		static const unsigned char bad[] = {
			0xe3, 0x81, 0x82,				/* U+3042, valid */
			0xff, 0xfe, 0x80, 0x80,
			0xe3, 0x81,						/* truncated, mid-file */
			0x0a,
			0xe3							/* truncated, at EOF */
		};

		if (argc != 2)
			usage();
		if (fwrite(bad, 1, sizeof(bad), stdout) != sizeof(bad))
			return 1;
	}
	else
		usage();

	if (fflush(stdout) != 0 || ferror(stdout))
	{
		perror("hostilegen");
		return 1;
	}
	return 0;
}
