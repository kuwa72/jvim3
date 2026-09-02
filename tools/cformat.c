/*
 * cformat.c - Lightweight C code formatter for JVim 3.0
 *
 * Can be used as 'equalprg', 'formatprg', or via :%!cformat
 * Reads C source code from standard input and outputs indented code to standard output.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_LINE 4096

static int sw = 4;        /* Shift width / spaces per indent level */
static int use_tabs = 0;   /* Use tabs if 1, spaces if 0 */

static void
print_indent(int level)
{
	int i;
	if (use_tabs)
	{
		for (i = 0; i < level; i++)
			putchar('\t');
	}
	else
	{
		int spaces = level * sw;
		for (i = 0; i < spaces; i++)
			putchar(' ');
	}
}

static void
format_stream(FILE *in, FILE *out)
{
	char line[MAX_LINE];
	int indent_level = 0;
	int in_comment = 0;

	while (fgets(line, sizeof(line), in) != NULL)
	{
		char *p = line;
		char *start;
		int len;
		int brace_open = 0;
		int brace_close = 0;
		int is_preproc = 0;
		int in_string = 0;
		int in_char = 0;
		char *scan;

		/* Skip leading whitespace */
		while (*p && isspace((unsigned char)*p))
			p++;

		/* Handle empty lines */
		if (*p == '\0' || *p == '\r' || *p == '\n')
		{
			fputs("\n", out);
			continue;
		}

		/* Strip trailing CR/LF */
		len = (int)strlen(p);
		while (len > 0 && (p[len - 1] == '\r' || p[len - 1] == '\n'))
		{
			p[--len] = '\0';
		}

		/* Check for preprocessor directive */
		if (*p == '#' && !in_comment)
		{
			is_preproc = 1;
		}

		/* Scan line for braces and comment state */
		scan = p;
		while (*scan)
		{
			if (in_comment)
			{
				if (scan[0] == '*' && scan[1] == '/')
				{
					in_comment = 0;
					scan += 2;
					continue;
				}
			}
			else if (in_string)
			{
				if (*scan == '\\' && scan[1])
					scan++;
				else if (*scan == '"')
					in_string = 0;
			}
			else if (in_char)
			{
				if (*scan == '\\' && scan[1])
					scan++;
				else if (*scan == '\'')
					in_char = 0;
			}
			else
			{
				if (scan[0] == '/' && scan[1] == '*')
				{
					in_comment = 1;
					scan += 2;
					continue;
				}
				else if (scan[0] == '/' && scan[1] == '/')
				{
					/* Line comment: rest of line is comment */
					break;
				}
				else if (*scan == '"')
				{
					in_string = 1;
				}
				else if (*scan == '\'')
				{
					in_char = 1;
				}
				else if (*scan == '{')
				{
					brace_open++;
				}
				else if (*scan == '}')
				{
					brace_close++;
				}
			}
			scan++;
		}

		/* If line starts with '}', temporarily decrease indent for this line */
		start = p;
		if (*start == '}' && indent_level > 0)
			indent_level--;

		/* Print indented line */
		if (!is_preproc)
			print_indent(indent_level);

		fputs(p, out);
		fputc('\n', out);

		/* Update indent level for subsequent lines */
		if (*start != '}')
			indent_level += (brace_open - brace_close);
		else
			indent_level += (brace_open - (brace_close - 1));

		if (indent_level < 0)
			indent_level = 0;
	}
}

int
main(int argc, char **argv)
{
	int i;
	for (i = 1; i < argc; i++)
	{
		if (strncmp(argv[i], "-sw=", 4) == 0 || strncmp(argv[i], "-ts=", 4) == 0)
		{
			sw = atoi(argv[i] + 4);
			if (sw <= 0)
				sw = 4;
		}
		else if (strcmp(argv[i], "-tab") == 0 || strcmp(argv[i], "-tabs") == 0)
		{
			use_tabs = 1;
		}
		else if (strcmp(argv[i], "-space") == 0 || strcmp(argv[i], "-spaces") == 0)
		{
			use_tabs = 0;
		}
		else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0)
		{
			fprintf(stderr, "Usage: cformat [-sw=N] [-tab] [-space] < infile > outfile\n");
			return 0;
		}
	}

	format_stream(stdin, stdout);
	return 0;
}
