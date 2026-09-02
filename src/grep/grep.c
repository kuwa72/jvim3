/*=============================================================== vim:ts=4:sw=4:
 *
 * GREP	- Japanized GREP
 *
 *============================================================================*/
#ifdef MSDOS
# include <io.h>
# if !defined(NT) && !defined(__GO32__)
#  include <alloc.h>
# endif
#endif
#define EXTERN
#include "vim.h"
#include "globals.h"
#include "proto.h"
#include "kanji.h"
#include "regexp.h"
#include "fcntl.h"
#if defined(USE_LOCALE) && defined(UNIX)
#include <locale.h>
#endif

/*==============================================================================
 *	option parameter
 *============================================================================*/
static long		o_after = 0;			/* -A : print after match lines		  */
static long		o_before= 0;			/* -B : print before match lines	  */
static int		o_conly = FALSE;		/* -c : count match lines			  */
static int		o_icase = FALSE;		/* -i : ignore case					  */
static int		o_nonly = FALSE;		/* -l : print only file name		  */
static int		o_num	= FALSE;		/* -n : print line number			  */
static int		o_noerr = FALSE;		/* -q : suppress error message		  */
static int		o_nomsg = FALSE;		/* -s : suppress normal output		  */
static int		o_nomat = FALSE;		/* -v : print not match lines		  */
static int		o_word	= FALSE;		/* -w : match words					  */
static int		o_line	= FALSE;		/* -x : match lines					  */
static int		o_jcase = FALSE;		/* -j : jignore case				  */
static long		o_count = 1;			/* -L : search lines				  */
static long		o_join	= FALSE;		/* -J : join search lines			  */
#if !defined(__DATE__) || !defined(__TIME__)
static char		version[] = "Japanized grep by Ken'ichi Tsuchida (1998 Mar 14)";
#else
static char		version[] = "Japanized grep by Ken'ichi Tsuchida (1998 Mar 14, compiled " __DATE__ " " __TIME__ ")";
#endif

#ifdef NT
/*==============================================================================
 *	GUI parameter
 *============================================================================*/
static HANDLE				hInst;
static HWND					hWin;
static HWND					hDlg;
static TCHAR				szAppName[] = TEXT("Grep");
static HANDLE				hEdit;
#endif

/*==============================================================================
 *	I/O parameter
 *============================================================================*/
static unsigned char	*	IObuffer;
static long					IOsize;
static unsigned char	*	IOstart;
static long					IOrest;
static int					o_ubig;

/*==============================================================================
 *	file parameter
 *============================================================================*/
static int					multi = FALSE;
#ifdef NT
static char				**	dir_list = NULL;
static int					dir_cnt = 0;
static int					DirList(char *);
#endif

/*==============================================================================
 *	line parameter
 *============================================================================*/
typedef struct {
	long					num;			/* line number					  */
	long					size;			/* line size					  */
	long					rest;			/* line rest size				  */
	unsigned char		*	lp;				/* line pointer					  */
} LINE;
static LINE				**	line;
static LINE				*	linep;
static int					o_fd = -1;

/*==============================================================================
 *	regexec parameter
 *============================================================================*/
typedef struct grep {
	int						left;			/* or search					  */
	int						next;			/* near search					  */
	int						not;			/* not search					  */
	regexp				*	prog;			/* regexec parameter			  */
} GREP;
static GREP				*	gpattern = NULL;

/*==============================================================================
 *	global parameter
 *============================================================================*/
int							reg_ic;
int							reg_jic;
int							p_magic = TRUE;
int							p_jkc	= FALSE;
int							p_opt;
char_u						p_jp[4] = JP_DEF;
#ifdef NT
int							p_cpage = CP_ACP;
#endif

int
emsg(msg)
char_u		*	msg;
{
	if (!o_noerr)
		fprintf(stderr, "%s\n", msg);
	return(TRUE);
}

char_u	*
ml_get_buf(buf, lnum, will_change)
BUF			*buf;
linenr_t	lnum;
int			will_change;		/* line will be changed */
{
	return(NULL);
}

int
mf_release_all()
{
	return(0);
}

long
mch_avail_mem(spec)
int spec;
{
#if !defined(MSDOS) || defined(NT)
	return 0x7fffffff;		/* virual memory eh */
#else
	return coreleft();
#endif
}

/*==============================================================================
 *	prototype define
 *============================================================================*/
static void			usage __ARGS((void));
static int			fileopen __ARGS((char *));
static int			fileread __ARGS((int, LINE *));
static int			grepsub __ARGS((GREP *, int *));
static void			compile __ARGS((char_u *, int, int));
static int			do_grep __ARGS((char *));

/*==============================================================================
 *	line parameter
 *============================================================================*/

static void
usage()
{
	fprintf(stderr, "usage: grep [-[[ABL] ]<num>] [-[CVcijlnqsvwx]] [-K <code>] [-[efE]] <expr> [<files...>]\n");
	fprintf(stderr, "	-A <num>    print after match lines.\n");
	fprintf(stderr, "	-B <num>    print before match lines.\n");
	fprintf(stderr, "	-C          print context match two lines.\n");
#if 0
	fprintf(stderr, "	-R <DIR>    search directory.\n");
#endif
	fprintf(stderr, "	-V          print version.\n");
	fprintf(stderr, "	-c          count match lines.\n");
	fprintf(stderr, "	-f <file>   regular expression define file.\n");
	fprintf(stderr, "	-i          ignore case.\n");
	fprintf(stderr, "	-l          print only file name.\n");
	fprintf(stderr, "	-n          print line number.\n");
	fprintf(stderr, "	-q          suppress error message.\n");
	fprintf(stderr, "	-s          suppress normal output.\n");
	fprintf(stderr, "	-v          print not match lines.\n");
	fprintf(stderr, "	-w          match words.\n");
	fprintf(stderr, "	-x          match lines.\n");
#if 0
	fprintf(stderr, "	-J          join search lines.\n");
#endif
	fprintf(stderr, "	-j          ignore japanese case.\n");
	fprintf(stderr, "	-e <expr>   or regular expression.\n");
	fprintf(stderr, "	-E <expr>   and regular expression.\n");
#ifdef NT
	fprintf(stderr, "	-K <code>   file/disp/system kanji code. [jJeEsSuU][JES][JES]\n");
#else
	fprintf(stderr, "	-K <code>   file/disp/system kanji code. [jJeEsS][JES][JES]\n");
#endif
	fprintf(stderr, "	-L <num>    search lines.\n");
	exit(2);
}

static int
fileopen(fname)
char		*	fname;
{
	int					fd;

	if (IObuffer == NULL)
	{
#if !defined(MSDOS) || defined(NT)
		if (sizeof(int) > 2)
			IOsize = 0x10000L;					/* read 64K at a time */
		else
#endif
			IOsize = 0x7ff0L;					/* limit buffer to 32K */
		while ((IObuffer = malloc(IOsize)) == NULL)
		{
			IOsize >>= 1;
			if (IOsize < 1024)
			{
				if (!o_noerr)
					fprintf(stderr, "memory allocation error.\n");
				exit(2);
			}
		}
	}
	IOstart= IObuffer;
	IOrest = 0;
	if (fname == NULL)			/* standerd input */
		fd = 0;
	else
		fd = open(fname, O_RDONLY);
	if (fd < 0)
		return(-1);
	if ((IOrest = read(fd, (char *)IObuffer, (size_t)IOsize)) <= 0)
		return(fd);
	if (isupper(JP_KEY))
		JP_KEY = judge_jcode(&JP_KEY, &o_ubig, IObuffer, IOrest);
#ifdef NT
	if (toupper(JP_KEY) == JP_WIDE)	/* UNICODE */
		IOrest = wide2multi(IObuffer, IOrest, o_ubig, TRUE);
#endif
	return(fd);
}

static int
fileread(fd, lp)
int					fd;
LINE			*	lp;
{
	long				size;
	unsigned char	*	ptr;
	unsigned char		c;
	int					n;
	int					found = FALSE;
	int					eof   = FALSE;
	long				rest;

	if (IOrest == 0)	/* end of file */
		return(-1);
	lp->rest += lp->size;
	lp->size  = 0;
	ptr  = IOstart;
	size = IOrest;
	ptr--;
	while (++ptr, --size >= 0)
	{
		if ((c = *ptr) != '\0' && c != '\n' && !(eof && size == 0))
			continue;
		if (c == '\0')
			*ptr = '\n';		/* NULs are replaced by newlines! */
		else if (ptr[-1] == '\r')	/* remove CR */
			ptr[-1] = '\0';
		found = TRUE;
#if 0
retry:
#endif
		n = (ptr - IOstart) * 2 + 1;
		if (n > lp->rest)
		{
			char	*	p = lp->lp;

			if ((lp->lp = malloc(n)) == NULL)
			{
				if (!o_noerr)
					fprintf(stderr, "memory allocation error.\n");
				exit(2);
			}
			if (p != NULL)
			{
				memmove(lp->lp, p, lp->size);
				free(p);
			}
			lp->rest = n;
			lp->lp[lp->size] = '\0';
		}
		n = kanjiconvsfrom(IOstart, ptr - IOstart,
					&lp->lp[lp->size], n, NULL, (char_u)toupper(JP_KEY), NULL);
		lp->size += n;
		lp->lp[lp->size] = '\0';	/* end of line */
		lp->rest = lp->rest - n;
#if 0
retry:
#endif
		ptr++;
		IOrest -= ptr - IOstart;
		IOstart = ptr;	/* nothing left to write */
		if (IOrest == 0)
		{
retry:
			rest = IOrest;
			if (IOrest != 0)
				memmove(IObuffer, IOstart, IOrest);
			if ((IOrest = read(fd, (char *)IObuffer + IOrest, (size_t)IOsize - IOrest)) < 0)
			{
				if (!o_noerr)
					fprintf(stderr, "file read error.\n");
				return(-1);
			}
			if (IOrest == 0)
				eof = TRUE;
			IOrest += rest;
#ifdef NT
			if (toupper(JP_KEY) == JP_WIDE)	/* UNICODE */
				IOrest = wide2multi(IObuffer, IOrest, o_ubig, FALSE);
#endif
			if (IOrest == 0)		/* end of file */
				return(0);
			ptr = IOstart = IObuffer;	/* nothing left to write */
			size = IOrest;
			ptr--;
		}
		if (found == TRUE)
			return(0);
	}
	ptr--;
	goto retry;
}

static void
compile(str, no, and)
char_u			*	str;
int					no;
int					and;
{
	regexp			*	prog;
	char_u			*	buf;
	int					n;

	if ((buf = malloc(sizeof(GREP) * (no + 1))) == NULL)
	{
		if (!o_noerr)
			fprintf(stderr, "memory allocation error.\n");
		exit(2);
	}
	memset(buf, 0, sizeof(GREP) * (no + 1));
	if (gpattern != NULL)
	{
		memcpy(buf, gpattern, sizeof(GREP) * no);
		free(gpattern);
	}
	gpattern = (GREP *)buf;
	n = strlen(str) * 2 + ((o_word || o_line) ? 7 : 1);
	if ((buf = malloc(n)) == NULL)
	{
		if (!o_noerr)
			fprintf(stderr, "memory allocation error.\n");
		exit(2);
	}
	n = kanjiconvsfrom(str, strlen(str), buf, n, NULL, JP_SYS, NULL);
	buf[n] = '\0';
	if (o_word)
	{
		memmove(&buf[2], buf, n);
		buf[0] = '\\';
		buf[1] = '<';
		buf[n + 2] = '\\';
		buf[n + 3] = '>';
		buf[n + 4] = '\0';
	}
	else if (o_line)
	{
		memmove(&buf[3], buf, n);
		buf[0] = '^';
		buf[1] = '\\';
		buf[2] = '(';
		buf[n + 3] = '\\';
		buf[n + 4] = ')';
		buf[n + 5] = '$';
		buf[n + 6] = '\0';
	}
	if ((prog = regcomp(buf)) == NULL)
	{
		if (!o_noerr)
			fprintf(stderr, "regular expression error \"%s\".\n", str);
		exit(2);
	}
	free(buf);
	gpattern[no].not = FALSE;
	gpattern[no].prog= prog;
	if (no != 0)
	{
		if (and)
			gpattern[no - 1].next = no;
		else
			gpattern[no - 1].left = no;
	}
}

static int
grepsub(GREP *gp, int *look)
{
	int					i;
	int					found = FALSE;

	if (o_join)
	{
		long				n = 0;
		char_u			*	p;
		char_u			*	cp;
		char_u			*	ep;
		int					kanji = FALSE;

		for (i = 0; i < o_count; i++)
		{
			if (line[i + o_before] == NULL)
				break;
			n += strlen(line[i + o_before]->lp);
		}
		if ((p = malloc(n + o_count + 1)) == NULL)
			return(FALSE);
		p[0] = '\0';
		for (i = 0; i < o_count; i++)
		{
			if (line[i + o_before] == NULL)
				break;
			cp = line[i + o_before]->lp;
			while (i != 0 && *cp)
			{
				if (*cp == ' ' || *cp == '\t')
					cp++;
				else if (ISkanji(*cp))
				{
					if (cp[0] == 0x81 && cp[1] == 0x40)
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
				else if (ISkanji(*cp) && cp[0] == 0x81 && cp[1] == 0x40)
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
		if (regexec(gp->prog, p, TRUE))
		{
			if (gp->prog->startp[0] < (p + strlen(line[o_before]->lp)))
				*look = TRUE;
			found = TRUE;
		}
		free(p);
	}
	else
	{
		for (i = 0; i < o_count; i++)
		{
			if (line[i + o_before] == NULL)
				break;
			if (regexec(gp->prog, line[i + o_before]->lp, TRUE))
			{
				if (i == 0)
					*look = TRUE;
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
		found = grepsub(&gpattern[gp->left], look);
	if (found && gp->next)
		return(grepsub(&gpattern[gp->next], look));
	if (found && gp->next == 0)
		return(TRUE);
	return(FALSE);
}

static int
do_grep(fname)
char				*	fname;
{
	long					lnum;
	LINE				*	lp;
	int						found = 0;
	int						look;
	long					pnum  = 0;
	long					i;
	char_u				*	p;
	long					counter = 0;
	int						quote	= FALSE;
	int						loop = o_before + ((o_count > o_after) ? o_count : o_after);
	int						disp;

	if ((o_fd = fileopen(fname)) < 0)
	{
#ifdef NT
		if (!o_noerr && !dir_cnt)
#else
		if (!o_noerr)
#endif
			fprintf(stderr, "file open error <%s>.\n", fname);
		return(0);
	}
	if (multi && fname != NULL)
	{
		regexp			*	pg1;
		regexp			*	pg2;
		static char_u		p1[] = ":[0-9][0-9]*$";
		static char_u		p2[] = ":[0-9][0-9]*:";

		if ((pg1 = regcomp(p1)) == NULL)
		{
			close(o_fd);
			if (!o_noerr)
				fprintf(stderr, "memory allocation error.\n");
			exit(2);
		}
		if ((pg2 = regcomp(p2)) == NULL)
		{
			close(o_fd);
			if (!o_noerr)
				fprintf(stderr, "memory allocation error.\n");
			exit(2);
		}
		if (regexec(pg1, fname, TRUE) || regexec(pg2, fname, TRUE))
			quote = TRUE;
		free(pg1);
		free(pg2);
	}

	reg_ic  = o_icase;
	reg_jic = o_jcase;

	for (lnum = 0; lnum < o_before; lnum++)
	{
		line[lnum] = &linep[lnum];
		line[lnum]->num = 0;
	}
	for (lnum = 0; lnum < ((o_count > o_after) ? o_count : o_after); lnum++)
	{
		if (fileread(o_fd, &linep[lnum + o_before]) < 0)
			break;
		line[lnum + o_before] = &linep[lnum + o_before];
		line[lnum + o_before]->num = lnum + 1;
	}
	for (;;)
	{
		look = FALSE;
		if (grepsub(&gpattern[0], &look) && look)
		{
			/* found it */
			if (o_nomat)
				look = FALSE;
			else
				found = 1;
		}
		else if (o_nomat && !look)
		{
			look = TRUE;
			found = 1;
		}
		else
			look = FALSE;
		if (look)
		{
			/* found action */
			if (o_nonly || o_nomsg)
			{
				if (!o_nomsg)
					printf("%s\n", fname);
				close(o_fd);
				return(found);
			}
			counter++;
			if (o_conly || o_nomsg)
				;
			else
			{
				if (o_after == 1 && o_before == 0)
					disp = o_count;
				else
					disp = o_after + o_before;
				for (i = 0; i < disp; i++)
				{
					lp = line[i];
					if (lp == NULL)
						break;
					if (lp->num == 0)
						continue;
					if (pnum < lp->num)
					{
						if (multi)
						{
							if (quote)
								printf("\"%s\":", fname);
							else
								printf("%s:", fname);
						}
						if (o_num)
							printf("%ld:", lp->num);
						p = kanjiconvsto(lp->lp, JP_DISP, TRUE);
						printf("%s\n", p);
						free(p);
						pnum = lp->num;
					}
				}
			}
		}
		lp = line[0];
		memmove(line, &line[1], sizeof(LINE *) * (loop - 1));
		line[loop - 1] = NULL;
		if (fileread(o_fd, lp) >= 0)
		{
			lp->num = ++lnum;
			line[loop - 1] = lp;
		}
		if (line[o_before] == NULL)
			break;
	}
	if (o_conly)
	{
		if (multi)
		{
			if (quote)
				printf("\"%s\":", fname);
			else
				printf("%s:", fname);
		}
		printf("%ld\n", counter);
	}
	close(o_fd);
	return(found);
}

static int
getarg(argc, argv)
int					argc;
char			**	argv;
{
	char			*	optarg;
	int					i;
	int					no = 0;
	int					fd;
	char			*	cp;
	int					skip;
	int					rtn = argc;
	LINE				f_opt;

	++argv;
	--argc;
	/*
	 * Process the command line arguments
	 */
	while (argc >= 1 && argv[0][0] == '-')
	{
		if (argv[0][1] == '-' && argv[0][2] == '\0')
		{
			++ argv;
			-- argc;
			break;
		}
		skip = FALSE;
		for (cp = argv[0] + 1; *cp && skip == FALSE; cp++)
		{
			switch (*cp) {
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				o_after = o_before = o_after * 10 + *cp - '0';
				break;
			case 'V':
				fprintf(stderr, "%s.\n", version);
				exit(0);
			case 'C':						/* print context match two lines  */
				o_after = o_before = 2;
				o_nomsg = FALSE;
				break;
			case 'J':						/* join search lines			  */
				o_join	= TRUE;
				o_line	= FALSE;
				if (o_count <= 1)
					o_count = 2;
				break;
			case 'c':						/* count match lines			  */
				o_conly = TRUE;
				o_nomsg = FALSE;
				o_nonly = FALSE;
				o_num	= FALSE;
				break;
			case 'i':						/* ignore case					  */
				o_icase = TRUE;
				break;
			case 'l':						/* print only file name			  */
				o_nonly = TRUE;
				o_nomsg = FALSE;
				o_conly = FALSE;
				o_num	= FALSE;
				break;
			case 'n':						/* print line number			  */
				o_num	= TRUE;
				o_nomsg = FALSE;
				o_conly = FALSE;
				o_nonly = FALSE;
				break;
			case 'q':						/* suppress error message		  */
				o_noerr = TRUE;
				break;
			case 's':						/* suppress normal output		  */
				o_nomsg = TRUE;
				o_conly = FALSE;
				o_nonly = FALSE;
				o_num	= FALSE;
				o_after = o_before = 0;
				break;
			case 'v':						/* print not match lines		  */
				o_nomat = TRUE;
				break;
			case 'w':						/* match words					  */
				o_word	= TRUE;
				o_line	= FALSE;
				break;
			case 'x':						/* match lines					  */
				o_line	= TRUE;
				o_word	= FALSE;
				o_join	= FALSE;
				break;

			case 'j':	o_jcase = TRUE; break;

			case 'f':						/* regular expression define file */
			case 'A':						/* print after match lines		  */
			case 'B':						/* print before match lines		  */
			case 'e':
			case 'E':
			case 'L':
			case 'K':
			case 'R':						/* Recurcev directory			  */
				if (cp[1] != '\0')
					optarg = &cp[1];
				else
				{
					++ argv;
					-- argc;
					if (argc >= 1)
						optarg = argv[0];
					else
					{
						fprintf(stderr, "option unmatch.\n");
						usage();
					}
				}
				skip = TRUE;
				switch (*cp) {
				case 'f':					/* regular expression define file */
					memset(&f_opt, 0, sizeof(LINE));
					if ((fd = fileopen(optarg)) < 0)
					{
						fprintf(stderr, "file open error <%s>.\n", optarg);
						exit(2);
					}
					while (fileread(fd, &f_opt) >= 0)
					{
						if (strlen(f_opt.lp) != 0)
						{
							compile(f_opt.lp, no, FALSE);
							no++;
						}
					}
					close(fd);
					break;
				case 'A':					/* print after match lines		  */
					o_after = atol((char *)optarg);
					break;
				case 'B':					/* print before match lines		  */
					o_before = atol((char *)optarg);
					break;
				case 'e':
				case 'E':
					compile(optarg, no, *cp == 'E' ? TRUE : FALSE);
					no++;
					break;
				case 'L':
					if ((o_count = atol((char *)optarg)) == 0)
						o_count = 1;
					break;
				case 'K':
					{
						char_u	*	p;
						char_u	*	q;

						q = optarg;
						for (i = 0, p = optarg; i < 3 && *p; i++, p++)
						{
							if (i == 0)
							{
								if (strchr(JP_FSTR, *p))
									p_jp[p - q] = *p;
							}
							else
							{
								if ('a' <= *p && *p <= 'z')
									*p -= 'a' - 'A';
								if (strchr(JP_STR, *p))
									p_jp[p - q] = *p;
							}
						}
					}
					break;
#ifdef NT
				case 'R':
					{
						char	**	result;
						int			j;

						result = glob_filename(optarg);
						for (j = 0; result[j] != NULL; j++)
						{
							DirList(result[j]);
							free(result[j]);
						}
						free(result);
					}
					break;
#endif
				}
				break;
			default:
				usage();
				break;
			}
		}
		++ argv;
		-- argc;
	}

	o_after++;
	if ((line = (LINE **)malloc(sizeof(LINE *) * ((o_count > o_after ? o_count : o_after) + o_before))) == NULL)
	{
		if (!o_noerr)
			fprintf(stderr, "memory allocation error.\n");
		exit(2);
	}
	memset(line, 0, sizeof(LINE *) * ((o_count > o_after ? o_count : o_after) + o_before));
	if ((linep = (LINE *)malloc(sizeof(LINE) * ((o_count > o_after ? o_count : o_after) + o_before))) == NULL)
	{
		if (!o_noerr)
			fprintf(stderr, "memory allocation error.\n");
		exit(2);
	}
	memset(linep, 0, sizeof(LINE) * ((o_count > o_after ? o_count : o_after) + o_before));

	if (no == 0 && argc == 0)
		usage();
	if (no == 0 && argc >= 1)
	{
		compile(argv[0], no, TRUE);
		++ argv;
		-- argc;
	}
	return(rtn - argc);
}

#ifdef NT
static int
do_winsearch(fname)
char			*	fname;
{
	int					rtn = 0;

		rtn |= do_grep(fname);
	return(rtn);
}
#endif

static int
do_search(argc, argv)
int					argc;
char			**	argv;
{
	int					i;
	int					rtn = 0;

	if (argc == 0)
	{
#ifdef NT
		if (dir_cnt)
		{
			char	**	result;
			int			j;
			int			k;
			char		buf[MAXPATHL];

			multi = TRUE;
			for (k = 0; k < dir_cnt; k++)
			{
				strcpy(buf, dir_list[k]);
				if (buf[strlen(buf) - 1] != '\\' && buf[strlen(buf) - 1] != '/')
					strcat(buf, "\\");
				strcat(buf, "*");
				result = glob_filename(buf);
				for (j = 0; result[j] != NULL; j++)
				{
					rtn |= do_winsearch(result[j]);
					free(result[j]);
				}
				free(result);
			}
		}
		else
#endif
		rtn |= do_grep(NULL);
	}
	else
	{
		if (argc > 1)
			multi = TRUE;
		for (i = 0; i < argc; i++)
		{
#ifdef NT
			if (dir_cnt)
			{
				char	**	result;
				int			j;
				int			k;
				char		buf[MAXPATHL];

				multi = TRUE;
				for (k = 0; k < dir_cnt; k++)
				{
					strcpy(buf, dir_list[k]);
					if (buf[strlen(buf) - 1] != '\\' && buf[strlen(buf) - 1] != '/')
						strcat(buf, "\\");
					strcat(buf, argv[i]);
					result = glob_filename(buf);
					for (j = 0; result[j] != NULL; j++)
					{
						rtn |= do_winsearch(result[j]);
						free(result[j]);
					}
					free(result);
				}
			}
			else
				rtn |= do_winsearch(argv[i]);
#else
			rtn |= do_grep(argv[i]);
#endif
		}
	}
	return(rtn ? 0 : 1);
}

int
#ifdef NT
win_main(argc, argv)
#else
main(argc, argv)
#endif
int					argc;
char			**	argv;
{
	int					rtn;

#ifdef MSDOS
	_fmode = O_BINARY;          /* we do our own CR-LF translation */
#endif
#ifdef USE_LOCALE
	setlocale(LC_ALL, "");		/* for ctype() and the like */
#endif
#ifdef UNIX
	{
		char	*p	= NULL;

# ifdef USE_LOCALE
		p = setlocale(LC_CTYPE, "");
# endif
		if (p == NULL)
			p = (char *)vimgetenv((char_u *)"LC_ALL");
		if (p == NULL)
			p = (char *)vimgetenv((char_u *)"LC_CTYPE");
		if (p == NULL)
			p = (char *)vimgetenv((char_u *)"LANG");
		if (p != NULL)
		{
# ifdef UCODE
			if (strstr(p, "UTF-8") != NULL
					|| strstr(p, "utf-8") != NULL
					|| strstr(p, "UTF8") != NULL
					|| strstr(p, "utf8") != NULL)
				JP_KEY = JP_DISP = JP_SYS = JP_UTF8;
			else
# endif
			if (strstr(p, "JIS") != NULL
					|| strstr(p, "jis") != NULL
					|| strstr(p, "ISO-2022-JP") != NULL
					|| strstr(p, "iso-2022-jp") != NULL)
				JP_KEY = JP_DISP = JP_SYS = JP_JIS;
			else if (strstr(p, "euc") != NULL
					|| strstr(p, "EUC") != NULL
					|| strstr(p, "ujis") != NULL
					|| strstr(p, "UJIS") != NULL
					|| STRCMP(p, "ja") == 0
					|| STRCMP(p, "japanese") == 0
					|| STRCMP(p, "japanese.euc") == 0)
				JP_KEY = JP_DISP = JP_SYS = JP_EUC;
			else if (strstr(p, "SJIS") != NULL
					|| strstr(p, "sjis") != NULL
					|| strstr(p, "PCK") != NULL
					|| strstr(p, "pck") != NULL
					|| strstr(p, "mscode") != NULL
					|| strstr(p, "MSCODE") != NULL
					|| strstr(p, "cp932") != NULL
					|| strstr(p, "CP932") != NULL)
				JP_KEY = JP_DISP = JP_SYS = JP_SJIS;
		}
	}
#endif

	rtn = getarg(argc, argv);
	argc -= rtn;
	argv =  &argv[rtn];

#ifdef XARGS
# ifdef NT
	if (dir_cnt == 0 && argc != 0)
# else
	if (argc != 0)
# endif
	{
		xargs(&argc, &argv);
		argc -= rtn;
		argv =  &argv[rtn];
	}
#endif
	rtn = 0;
	/*
	 * execute grep for each file
	 */
	rtn = do_search(argc, argv);
	return rtn;
}

#ifdef NT
int
main(argc, argv)
int					argc;
char			**	argv;
{
	int					rtn;

	if (vimgetenv("VIM32DEBUG") == NULL)
	{
		__try {
			rtn = win_main(argc, argv);
		}
		__except (EXCEPTION_CONTINUE_EXECUTION) {
			;
		}
	}
	else
		rtn = win_main(argc, argv);
	return rtn;
}

static int
DirList(dir)
char			*	dir;
{
	char			*	name;
    HANDLE				hFind;
    WIN32_FIND_DATA		fdata;
	char			*	wk;

	if ((hFind = FindFirstFile(dir, &fdata)) != INVALID_HANDLE_VALUE)
	{
		FindClose(hFind);
		if (!(fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
			return(FALSE);
	}
	if ((wk = malloc(sizeof(char *) * (dir_cnt + 1))) == NULL)
	{
		fprintf(stderr, "memory allocation error.\n");
		exit(2);
	}
	if (dir_list)
		memcpy(wk, dir_list, sizeof(char *) * dir_cnt);
	dir_list = (char **)wk;
	if ((name = malloc(strlen(dir) + 1)) == NULL)
	{
		fprintf(stderr, "memory allocation error.\n");
		exit(2);
	}
	strcpy(name, dir);
	dir_list[dir_cnt++] = name;
	if ((name = malloc(strlen(dir) + strlen("\\*") + 1)) == NULL)
	{
		fprintf(stderr, "memory allocation error.\n");
		exit(2);
	}
	strcpy(name, dir);
	if (dir[strlen(dir) - 1] != '\\' && dir[strlen(dir) - 1] != '/')
		strcat(name, "\\");
	strcat(name, "*");
	if ((hFind = FindFirstFile(name, &fdata)) != INVALID_HANDLE_VALUE)
	{
		do {
			if ((fdata.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
					&& strcmp(fdata.cFileName, ".") != 0
					&& strcmp(fdata.cFileName, "..") != 0)
			{
				if ((wk = malloc(strlen(dir) + strlen("fdata.cFileName") + 2)) == NULL)
				{
					fprintf(stderr, "memory allocation error.\n");
					exit(2);
				}
				strcpy(wk, dir);
				if (dir[strlen(dir) - 1] != '\\' && dir[strlen(dir) - 1] != '/')
					strcat(wk, "\\");
				strcat(wk, fdata.cFileName);
				DirList(wk);
				free(wk);
			}
		} while (FindNextFile(hFind, &fdata));
		FindClose(hFind);
	}
	free(name);
    return(TRUE);
}

#endif
