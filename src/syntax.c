/* vi:ts=4:sw=4
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *
 * Read the file "credits.txt" for a list of people who contributed.
 * Read the file "uganda.txt" for copying and usage conditions.
 */

/*
 * syntax.c: code for syntax highlighting
 */

#if defined(KANJI) && defined(NT) && defined(SYNTAX)

#include "vim.h"
#include "globals.h"
#include "proto.h"
#include "param.h"
#include "kanji.h"
#include "regexp.h"

#define MAX_COLS		0x7fffffff
#define	S_LINE(_max)	(p_synl > 0 ? p_synl : ((_max) > (Rows * 10) ? Rows * 4 : Rows * 2))
#define	T_LINE(_max)	(p_synl > 0 ? p_synl : ((_max) > (Rows * 10) ? Rows * 2 : Rows * 1))

#define SYNTAX_CACHE	1

typedef struct _syntax {
	struct _syntax	*	next;
	char_u			*	name;
	int					color;
	int					ic;
	int					jic;
	int					word;
	int					last;
	int					min;
	int					type;
	linenr_t			startpos;
	linenr_t			endpos;
	char_u			*	str;
	regexp			*	prog;
	regexp			*	progend;
#if SYNTAX_CACHE2
	struct _syntax	*	prep;
#endif
	int					hash;
} syntax;

#define TYPE_NON		0
#define TYPE_PAIR		1
#define TYPE_TAG		2
#define TYPE_TAGP		3
#define TYPE_CRCH		4

typedef struct _syntag {
	struct _syntag	*	next;
	char_u			*	string_l;
	char_u			*	string_r;
	char_u			*	str_l;
	char_u			*	str_r;
	regexp			*	prog_l;
	regexp			*	prog_r;
} syntag;

typedef struct {
	char_u				id;
	int					rgb;
} syncolor;

typedef struct _synlink {
	struct _synlink	*	next;
	char_u			*	name;
	char_u			*	lname;
	int					color;
} synlink;

static syntax			defcolor	= {NULL, NULL, 'A'};

static syncolor			usercolor[] = {
	{'[',	0x00000000,},	{'\\',	0x00000000,},	{']',	0x00000000,},
	{'^',	0x00000000,},	{'_',	0x00000000,},
	{'Z',	0x00000000,},	{'Y',	0x00000000,},	{'X',	0x00000000,},
	{'W',	0x00000000,},	{'V',	0x00000000,},
} ;

#if SYNTAX_CACHE
typedef struct {
	char_u			*	idx;
	int					cnt;
} synindex;
# define HASH_SIZE		0x400
static synindex			synhash[HASH_SIZE];
#endif

	void
syn_clr(buf)
BUF			*	buf; 			/* buffer we are a window into */
{
	syntax			*	twp;
	syntax			*	tnp;
	syntag			*	gwp;
	syntag			*	gnp;
	synlink			*	lwp;
	synlink			*	lnp;
	int					no;

	for (no = 0; no < sizeof(usercolor) / sizeof(syncolor); no++)
		usercolor[no].rgb = 0x00000000;
	twp = (syntax *)buf->b_syn_ptr;
	buf->b_syn_ptr = NULL;
	while (twp)
	{
		tnp = twp->next;
		if (twp->str)
			free(twp->str);
		if (twp->prog)
			free(twp->prog);
		if (twp->progend)
			free(twp->progend);
		free(twp);
		twp = tnp;
	}
	gwp = (syntag *)buf->b_syn_tag;
	buf->b_syn_tag = NULL;
	while (gwp)
	{
		gnp = gwp->next;
		if (gwp->string_l)
			free(gwp->string_l);
		if (gwp->string_r)
			free(gwp->string_r);
		if (gwp->str_l)
			free(gwp->str_l);
		if (gwp->str_r)
			free(gwp->str_r);
		gwp = gnp;
	}
	lwp = (synlink *)buf->b_syn_link;
	while (lwp)
	{
		lnp = lwp->next;
		if (lwp->name)
			free(lwp->name);
		free(lwp);
		lwp = lnp;
	}
	buf->b_syn_link		= NULL;
	buf->b_syn_line		= 0;
	buf->b_syn_match	= NULL;
	buf->b_syn_matchend	= NULL;
	buf->b_syn_curp		= NULL;
}

static int
syn_regexec(min, ic, jic, prog, ptr, at_bol)
int				min;
int				ic;
int				jic;
regexp		*	prog;
char_u		*	ptr;
int 			at_bol;
{
	char_u			*	startp;
	char_u			*	endp;
	char_u				c;
	int					rc;
	int					magic;

	magic		= p_magic;
	reg_magic	= TRUE;
	reg_ic		= ic;
	reg_jic		= jic;
	rc = regexec(prog, ptr, at_bol);
	p_magic		= magic;
	if (!rc)
		return(0);
	if (!min)
		return(1);
	reg_magic = TRUE;
	while (rc)
	{
		startp	= prog->startp[0];
		endp	= prog->endp[0];
		if (ISkanjiPointer(startp, &endp[-1]) == 2)
		{
			c = endp[-2];
			endp[-2] = '\0';
			rc = regexec(prog, startp, at_bol);
			endp[-2] = c;
		}
		else
		{
			c = endp[-1];
			endp[-1] = '\0';
			rc = regexec(prog, startp, at_bol);
			endp[-1] = c;
		}
	}
	p_magic = magic;
	prog->startp[0]	= startp;
	prog->endp[0]	= endp;
	return(1);
}

static regexp *
syn_regcomp(ic, jic, string)
int				ic;
int				jic;
char_u		*	string;
{
	regexp		*	prog;
	int				magic;

	magic		= p_magic;
	reg_magic	= TRUE;
	reg_ic		= ic;
	reg_jic		= jic;
	prog = regcomp(string);
	p_magic		= magic;
	return(prog);
}

static int
syn_isregstr(str)
char_u		*	str;
{
	while (*str)
	{
		if (ISkanji(*str))
			str++;
		else if (*str == '\\')
		{
			str++;
			if (ISkanji(*str))
				str++;
			else if (*str != '\0' && strchr("<>+=|(", *str) != NULL)
				return(TRUE);
			else if (*str != '\0' && strchr("etrbn", *str) != NULL)
				str++;
#ifndef notdef
			else if (*str != '\0' && strchr("iIkKfFpPsSdDxXoOwWhHaAlLuUetrbn", *str) != NULL)
				return(TRUE);
#endif
		}
		else if (strchr(".^$[*", *str) != NULL)
			return(TRUE);
		if (*str != '\0')
			str++;
	}
	return(FALSE);
}

static int
syn_cls(ptr)
char_u		*	ptr;
{
	int				c = *ptr;

	if (ISkanji(c))
	{
		int		ret;

		if ((ret = jpcls(ptr)) >= 0)
			return(ret);
	}
	if (c == ' ' || c == '\t' || c == '\0')
		return(0);

	if (isidchar(c))
		return(1);

	return(2);
}

#if SYNTAX_CACHE
static void
syn_makeidx(ptr)
char_u		*	ptr;
{
	int				sclass;
	int				oclass	= -1;
	int				nocase	= 0;
	int				incase	= 0;
	char_u		*	indexp = NULL;

	memset(synhash, 0, sizeof(synhash));
	while (*ptr)
	{
		sclass = syn_cls(ptr);
		if (sclass != oclass)
		{
			if (oclass != (-1) && synhash[incase % HASH_SIZE].cnt < 0x80)
			{
				synhash[incase % HASH_SIZE].cnt++;
				if (synhash[incase % HASH_SIZE].idx == NULL)
					synhash[incase % HASH_SIZE].idx = indexp;
			}
			if (oclass != (-1) && synhash[nocase % HASH_SIZE].cnt < 0x80)
			{
				synhash[nocase % HASH_SIZE].cnt++;
				if (synhash[nocase % HASH_SIZE].idx == NULL)
					synhash[nocase % HASH_SIZE].idx = indexp;
			}
			incase = 0;
			nocase = 0;
			indexp = ptr;
		}
		incase += *ptr;
		nocase += toupper(*ptr);
		if (ISkanji(*ptr))
			ptr++;
		ptr++;
		oclass = sclass;
	}
	synhash[incase % HASH_SIZE].cnt++;
	if (synhash[incase % HASH_SIZE].idx == NULL)
		synhash[incase % HASH_SIZE].idx = indexp;
	synhash[nocase % HASH_SIZE].cnt++;
	if (synhash[nocase % HASH_SIZE].idx == NULL)
		synhash[nocase % HASH_SIZE].idx = indexp;
}
#endif

static char_u	*
syn_strstr(s1, s2, ic, word, hash)
char_u		*	s1;
char_u		*	s2;
int				ic;
int				word;
int				hash;
{
	int				pos;

#if SYNTAX_CACHE
	if (word)
	{
		if (synhash[hash].cnt == 0)
			return(NULL);
		s1 = synhash[hash].idx;
	}
#endif
	while (*s1)
	{
		while (*s1)
		{
			if (ic)
			{
				if (ISkanji(*s1) || ISkana(*s1))
				{
					if (*s1 == *s2)
						break;
				}
				else if (toupper(*s1) == toupper(*s2))
					break;
			}
			else if (*s1 == *s2)
				break;
			if (ISkanji(*s1))
				s1++;
			s1++;
		}
		if (*s1 == '\0')
			return(NULL);
		pos = 0;
		while (s2[pos])
		{
			if (s1[pos] == '\0')
				return(NULL);
			if (ISkanji(s1[pos]) != ISkanji(s2[pos]))
				break;
			if (ISkanji(s1[pos]))
			{
				if (s1[pos] != s2[pos] || s1[pos + 1] != s2[pos + 1])
					break;
				pos++;
			}
			else if (ic)
			{
				if (toupper(s1[pos]) != toupper(s2[pos]))
					break;
			}
			else if (s1[pos] != s2[pos])
				break;
			pos++;
		}
		if (s2[pos] == '\0')
			return(s1);
		s1++;
#if SYNTAX_CACHE
		if (word && synhash[hash].cnt == 1)
			return(NULL);
#endif
	}
	return(s1);
}

static char_u *
syn_strsave(p)
char_u		*	p;
{
	char_u	*	w;

	w = p = strsave(p);
	while (*w)
	{
		if (*w == '\\')
		{
			if (ISkanji(*w))
				w += 2;
			else
			{
				switch (w[1]) {
				case 'e':	w[1] = '\033';	break;
				case 't':	w[1] = '\t';	break;
				case 'r':	w[1] = '\r';	break;
				case 'b':	w[1] = '\010';	break;
				case 'n':	w[1] = '\n';	break;
				default:					break;
				}
				memmove(w, &w[1], strlen(w));
			}
		}
		else if (ISkanji(*w))
			w++;
		w++;
	}
	*w = '\0';
	return(p);
}

static void
syn_addtag(buf, string_l, string_r, regp_l, regp_r)
BUF			*	buf; 			/* buffer we are a window into */
char_u		*	string_l;
char_u		*	string_r;
regexp		*	regp_l;
regexp		*	regp_r;
{
	syntag			*	gwp;
	syntag			*	gnp;

	gwp = (syntag *)buf->b_syn_tag;
	while (gwp)
	{
		if ((strcmp(gwp->string_l, string_l) == 0)
				&& (strcmp(gwp->string_r, string_r) == 0))
			return;
		gwp = gwp->next;
	}
	gwp = malloc(sizeof(syntag));
	memset(gwp, 0, sizeof(syntag));
	gwp->string_l	= strsave(string_l);
	gwp->string_r	= strsave(string_r);
	if (!syn_isregstr(string_l) && !syn_isregstr(string_r))
	{
		gwp->str_l	= syn_strsave(string_l);
		gwp->str_r	= syn_strsave(string_r);
	}
	else
	{
		gwp->prog_l	= regp_l;
		gwp->prog_r	= regp_r;
	}
	if (buf->b_syn_tag == NULL)
		buf->b_syn_tag = (char_u *)gwp;
	else
	{
		gnp = (syntag *)buf->b_syn_tag;
		while (gnp->next)
			gnp = gnp->next;
		gnp->next = gwp;
	}
}

static int
syn_color(buf, name)
BUF			*	buf; 			/* buffer we are a window into */
char_u		*	name;
{
	char_u			*	p;
	int					no;
	int					rgb = 0;
	int					r = 0;
	int					g = 0;
	int					b = 0;

	p = name;
	while (*p && !iswhite(*p))
		++p;
	if (*p != NUL)
		*p++ = NUL;
	skipspace(&p);
	if (strlen(name) != strlen("user0"))
		return(1);
	if (vim_strnicmp(name, "user", strlen("user")) != 0)
		return(1);
	no = name[4];
	if ('0' <= no && no <= '9')
		no -= '0';
	else
		return(1);
	if (no >= sizeof(usercolor) / sizeof(syncolor))
		return(1);
	if (*p != '#')
		return(1);
	p++;
	if (strlen(p) != 6)
		return(1);
	while (*p)
	{
		rgb = rgb << 4;
		if ('0' <= *p && *p <= '9')
			rgb |= *p - '0';
		else if ('a' <= *p && *p <= 'f')
			rgb |= *p - 'a' + 10;
		else if ('A' <= *p && *p <= 'F')
			rgb |= *p - 'A' + 10;
		else
			return(1);
		p++;
	}
	r = (rgb & 0x00ff0000) >> 16;
	g = (rgb & 0x0000ff00) >>  8;
	b =  rgb & 0x000000ff;
	rgb = (b << 16) | (g << 8) | r;
	usercolor[no].rgb = rgb;
	return(0);
}

int
syn_user_color(id)
char_u			id;
{
	int					no;

	for (no = 0; no < sizeof(usercolor) / sizeof(syncolor); no++)
	{
		if (usercolor[no].id == id)
			return(usercolor[no].rgb);
	}
	return(0);
}

static int
syn_get_color(buf, name, p, lname)
BUF			*	buf;
char_u		*	name;
char_u		**	p;
char_u		**	lname;
{
	int					color = 0;
	synlink			*	lwp;

	if (lname != NULL)
		*lname = NULL;
	lwp = (synlink *)buf->b_syn_link;
	while (lwp)
	{
		if (stricmp(name, lwp->name) == 0)
		{
			if (lname)
				*lname = lwp->name;
			return(lwp->color);
		}
		lwp = lwp->next;
	}

	/* choice syntax type */
		 if (stricmp("bold",   name) == 0) color = 0x40;
	else if (stricmp("italic", name) == 0) color = 0x60;
	else if (stricmp("uline",  name) == 0) color = 0x80;
	else if (stricmp("bolic",  name) == 0) color = 0xa0;
	else ;
	if (color)
	{
		name = *p;
		while (**p && !iswhite(**p))
			++(*p);
		if (**p != NUL)
		{
			**p = NUL;
			++(*p);
		}
		skipspace(&(*p));
	}

	/* choice color */
		 if (stricmp("reverse", name) == 0)	color += '@';
	else if (stricmp("rev",     name) == 0)	color += '@';
	else if (stricmp("text",    name) == 0)	color += 'A';
	else if (stricmp("white",   name) == 0)	color += 'B';
	else if (stricmp("black",   name) == 0)	color += 'C';
	else if (stricmp("red",     name) == 0)	color += 'D';
	else if (stricmp("green",   name) == 0)	color += 'E';
	else if (stricmp("blue",    name) == 0)	color += 'F';
	else if (stricmp("yellow",  name) == 0)	color += 'G';
	else if (stricmp("pink",    name) == 0)	color += 'H';
	else if (stricmp("fuchsia", name) == 0)	color += 'H';
	else if (stricmp("magenta", name) == 0)	color += 'H';
	else if (stricmp("silver",  name) == 0)	color += 'I';
	else if (stricmp("gold",    name) == 0)	color += 'J';
	else if (stricmp("light",   name) == 0)	color += 'K';
	else if (stricmp("lime",    name) == 0)	color += 'K';
	else if (stricmp("navy",    name) == 0)	color += 'L';
	else if (stricmp("aqua",    name) == 0)	color += 'M';
	else if (stricmp("cyan",    name) == 0)	color += 'M';
	else if (stricmp("gray",    name) == 0)	color += 'N';
	else if (stricmp("maroon",  name) == 0)	color += 'O';
	else if (stricmp("olive",   name) == 0)	color += 'P';
	else if (stricmp("purple",  name) == 0)	color += 'Q';
	else if (stricmp("teal",    name) == 0)	color += 'R';
	else if (stricmp("user0",   name) == 0)	color += '[';
	else if (stricmp("user1",   name) == 0)	color += '\\';
	else if (stricmp("user2",   name) == 0)	color += ']';
	else if (stricmp("user3",   name) == 0)	color += '^';
	else if (stricmp("user4",   name) == 0)	color += '_';
	else if (stricmp("user5",   name) == 0)	color += 'Z';
	else if (stricmp("user6",   name) == 0)	color += 'Y';
	else if (stricmp("user7",   name) == 0)	color += 'X';
	else if (stricmp("user8",   name) == 0)	color += 'W';
	else if (stricmp("user9",   name) == 0)	color += 'V';
	else									return(0);
	return(color);
}

static int
syn_link(buf, name)
BUF			*	buf; 			/* buffer we are a window into */
char_u		*	name;
{
	char_u			*	clr;
	char_u			*	type;
	char_u			*	wk;
	synlink			*	lwp;
	synlink			*	lp;
	char_u			*	lname;
	int					color = 0;

	type = name;
	while (*type && !iswhite(*type))
		++type;
	if (*type != NUL)
		*type++ = NUL;
	skipspace(&type);
	clr = type;
	while (*clr && !iswhite(*clr))
		++clr;
	if (*clr != NUL)
		*clr++ = NUL;
	skipspace(&clr);
	wk = clr;
	if ((color = syn_get_color(buf, type, &clr, &lname)) == 0)
		return(1);
	if (syn_get_color(buf, name, &wk, NULL) != 0)
	{
		lwp = (synlink *)buf->b_syn_link;
		while (lwp)
		{
			if (stricmp(name, lwp->name) == 0)
			{
				syntax			*	twp;
				char_u			sbuf[CMDBUFFSIZE + 1];

				lwp->color	= color;
				if (lname && stricmp(name, lname) == 0)
					lwp->lname	= NULL;
				else
					lwp->lname	= lname;
				lwp = (synlink *)buf->b_syn_link;
				while (lwp)
				{
					if (lwp->lname && stricmp(name, lwp->lname) == 0
								&& (lname == NULL || stricmp(lname, lwp->name) != 0))
					{
						strcpy(sbuf, lwp->name);
						strcat(sbuf, " ");
						strcat(sbuf, name);
						syn_link(buf, sbuf);
					}
					lwp = lwp->next;
				}
				twp = (syntax *)buf->b_syn_ptr;
				while (twp)
				{
					if (twp->name && stricmp(twp->name, name) == 0)
						twp->color = color;
					twp = twp->next;
				}
				return(0);
			}
			lwp = lwp->next;
		}
		return(1);
	}

	/* check */
		 if (stricmp("load",   name) == 0) return(1);
	else if (stricmp("clear",  name) == 0) return(1);
	else if (stricmp("color",  name) == 0) return(1);
	else if (stricmp("link",   name) == 0) return(1);
	else ;

	lp = malloc(sizeof(synlink));
	memset(lp, 0, sizeof(synlink));
	lp->name	= strsave(name);
	lp->lname	= lname;
	lp->color	= color;
	if (buf->b_syn_link == NULL)
		buf->b_syn_link = (unsigned char *)lp;
	else
	{
		lwp = (synlink *)buf->b_syn_link;
		while (lwp->next)
			lwp = lwp->next;
		lwp->next = lp;
	}
	return(0);
}

static void
syn_loadtag(buf, fname)
BUF			*	buf; 			/* buffer we are a window into */
char_u		*	fname;
{
	FILE			*	tp;
	char_u				lbuf[LSIZE];
	char				wbuf[2];
	char			*	wk;
	int					c;
	int					color;
	char_u			*	p;
	syntax			*	r;
	syntax			*	w;
	char_u			*	lname;

	/* tags option color set */
	wbuf[0] = '\0';
	wk = wbuf;
	if (syn_get_color(buf, "tagsClass",		&wk, NULL) == 0)
	{
		strcpy(lbuf, "tagsClass				bolic	gold");
		syn_link(buf, lbuf);
	}
	if (syn_get_color(buf, "tagsDefine",	&wk, NULL) == 0)
	{
		strcpy(lbuf, "tagsDefine			italic	gold");
		syn_link(buf, lbuf);
	}
	if (syn_get_color(buf, "tagsValue",		&wk, NULL) == 0)
	{
		strcpy(lbuf, "tagsValue				bold	silver");
		syn_link(buf, lbuf);
	}
	if (syn_get_color(buf, "tagsFunction",	&wk, NULL) == 0)
	{
		strcpy(lbuf, "tagsFunction			bold	gold");
		syn_link(buf, lbuf);
	}
	if (syn_get_color(buf, "tagsEnum",		&wk, NULL) == 0)
	{
		strcpy(lbuf, "tagsEnum				bolic	gold");
		syn_link(buf, lbuf);
	}
	if (syn_get_color(buf, "tagsNames",		&wk, NULL) == 0)
	{
		strcpy(lbuf, "tagsNames				bolic	gold");
		syn_link(buf, lbuf);
	}
	if (syn_get_color(buf, "tagsProto",		&wk, NULL) == 0)
	{
		strcpy(lbuf, "tagsProto				bolic	gold");
		syn_link(buf, lbuf);
	}
	if (syn_get_color(buf, "tagsStruct",	&wk, NULL) == 0)
	{
		strcpy(lbuf, "tagsStruct			bolic	gold");
		syn_link(buf, lbuf);
	}
	if (syn_get_color(buf, "tagsTypedef",	&wk, NULL) == 0)
	{
		strcpy(lbuf, "tagsTypedef			bolic	gold");
		syn_link(buf, lbuf);
	}
	if (syn_get_color(buf, "tagsUnion",		&wk, NULL) == 0)
	{
		strcpy(lbuf, "tagsUnion				bolic	gold");
		syn_link(buf, lbuf);
	}
	if (syn_get_color(buf, "tagsVariable",	&wk, NULL) == 0)
	{
		strcpy(lbuf, "tagsVariable			italic	silver");
		syn_link(buf, lbuf);
	}
	if (syn_get_color(buf, "tagsMember",	&wk, NULL) == 0)
	{
		strcpy(lbuf, "tagsMember			bolic	silver");
		syn_link(buf, lbuf);
	}
	if (syn_get_color(buf, "tagsExternal",	&wk, NULL) == 0)
	{
		strcpy(lbuf, "tagsExternal			bolic	silver");
		syn_link(buf, lbuf);
	}
	if (syn_get_color(buf, "tagsUnknown",	&wk, NULL) == 0)
	{
		strcpy(lbuf, "tagsUnknown			bolic	silver");
		syn_link(buf, lbuf);
	}

	/* get stack of tag file names from tags option */
	if ((tp = fopen(fileconvsto(fname), "r")) == NULL)
		return;
	while (!got_int && fgets((char *)lbuf, LSIZE, tp) != NULL)
	{
		if (strlen(lbuf) == (LSIZE - 1) && lbuf[strlen(lbuf) - 1] != '\n')
		{
			while ((c = fgetc(tp)) != EOF)
			{
				if (c == '\n')
					break;
			}
			continue;
		}
		{
			char        tmp[LSIZE];
			int         len;

			len = kanjiconvsfrom(lbuf, STRLEN(lbuf), tmp, LSIZE, NULL, JP_SYS, NULL);
			tmp[len] = NUL;
			STRCPY(lbuf, tmp);
		}
		p = strstr(lbuf, ";\"\t");
		if (p == NULL)
			continue;
		if (strchr(p_synt, p[3]) == NULL)
			continue;
		switch (p[3]) {
		case 'c':	/* class	*/
			color = syn_get_color(buf, "tagsClass",		wk, &lname);
			break;
		case 'd':	/* define	*/
			color = syn_get_color(buf, "tagsDefine",	wk, &lname);
			break;
		case 'e':	/*enum value*/
			color = syn_get_color(buf, "tagsValue",		wk, &lname);
			break;
		case 'f':	/* function	*/
			color = syn_get_color(buf, "tagsFunction",	wk, &lname);
			break;
		case 'g':	/* enum		*/
			color = syn_get_color(buf, "tagsEnum",		wk, &lname);
			break;
		case 'm':	/* member	*/
			color = syn_get_color(buf, "tagsMember",	wk, &lname);
			break;
		case 'n':	/* namespaces	*/
			color = syn_get_color(buf, "tagsNames",		wk, &lname);
			break;
		case 'p':	/* prototypes	*/
			color = syn_get_color(buf, "tagsProto",		wk, &lname);
			break;
		case 's':	/* struct	*/
			color = syn_get_color(buf, "tagsStruct",	wk, &lname);
			break;
		case 't':	/* typedef	*/
			color = syn_get_color(buf, "tagsTypedef",	wk, &lname);
			break;
		case 'u':	/* union	*/
			color = syn_get_color(buf, "tagsUnion",		wk, &lname);
			break;
		case 'v':	/* variable	*/
			color = syn_get_color(buf, "tagsVariable",	wk, &lname);
			break;
		case 'x':	/* external	*/
			color = syn_get_color(buf, "tagsExternal",	wk, &lname);
			break;
		default:
			color = syn_get_color(buf, "tagsUnknown",	wk, &lname);
			break;
		}
		p = lbuf;
		skiptospace(&p);	/* skip tag */
		if (*p == NUL)
			break;
		*p++ = '\0';
		r = malloc(sizeof(syntax));
		memset(r, 0, sizeof(syntax));
		r->name		= lname;
		r->color	= color;
		r->word		= TRUE;
		if ((r->str = strsave(lbuf)) == NULL)
		{
			free(r);
			continue;
		}
		if (buf->b_syn_ptr == NULL)
			buf->b_syn_ptr = (char_u *)r;
		else
		{
			w = (syntax *)buf->b_syn_ptr;
			while (w->next)
				w = w->next;
			w->next = r;
#if SYNTAX_CACHE2
			r->prep = w;
#endif
#if SYNTAX_CACHE
			if (r->word)
			{
				char_u		*	p = r->str;
				int				hash	= 0;

				while (*p)
				{
					if (r->ic)
						hash += toupper(*p);
					else
						hash += *p;
					if (ISkanji(*p))
						p++;
					p++;
				}
				r->hash = hash % HASH_SIZE;
			}
#endif
		}
		breakcheck();
	}
	fclose(tp);
}

static void
syn_load(buf, fname)
BUF			*	buf; 			/* buffer we are a window into */
char_u		*	fname;
{
	char_u			*	np;
	int					i;
	char_u				sbuf[CMDBUFFSIZE + 1];

	if (*fname != '\0')
		syn_loadtag(buf, fname);
	else
	{
		for (np = p_tags; *np; )
		{
			for (i = 0; i < CMDBUFFSIZE && *np; ++i)
			{
				if (*np == ' ')
				{
					++np;
					break;
				}
				sbuf[i] = *np++;
			}
			sbuf[i] = '\0';
			syn_loadtag(buf, sbuf);
		}
	}
}

static int
syn_crchar(buf, name)
BUF			*	buf;
char_u		*	name;
{
	char_u			*	type;
	char_u			*	lname;
	syntax			*	w;
	syntax			*	r;
	int					color = 0;

	type = name;
	while (*type && !iswhite(*type))
		++type;
	if (*type != NUL)
		*type++ = NUL;
	skipspace(&type);
	if ((color = syn_get_color(buf, name, &type, &lname)) == 0)
		return(1);
	w = (syntax *)buf->b_syn_ptr;
	while (w)
	{
		if (w->type == TYPE_CRCH)
		{
			w->name = lname;
			w->color= color;
			return(0);
		}
		w = w->next;
	}
	r = malloc(sizeof(syntax));
	memset(r, 0, sizeof(syntax));
	r->name		= lname;
	r->color	= color;
	r->type		= TYPE_CRCH;
	if (buf->b_syn_ptr == NULL)
		buf->b_syn_ptr = (char_u *)r;
	else
	{
		w = (syntax *)buf->b_syn_ptr;
		while (w->next)
			w = w->next;
		w->next = r;
#if SYNTAX_CACHE2
		r->prep = w;
#endif
	}
	return(0);
}

	int
syn_add(buf, reg)
BUF			*	buf;
char_u		*	reg;
{
	syntax			*	w;
	syntax			*	r;
	char_u			*	p;
	char_u			*	lname;
	int					color = 0;
	char_u				pattern[1024];
	char_u				pattern_l[1024];
	char_u			*	nextp;
	int					l_ic	= FALSE;
	int					l_jic	= FALSE;
	int					l_word	= FALSE;
	int					l_last	= 0;
	int					l_min	= FALSE;
	int					l_type	= TYPE_NON;
	int					magic;
	int					rc;
	char_u			*	tagprog;
	char_u			*	tagprogend;
	int					tagfirst= TRUE;

	p = reg;
	while (*p && !iswhite(*p))
		++p;
	if (*p != NUL)
		*p++ = NUL;
	skipspace(&p);
	if (stricmp("clear", reg) == 0)
	{
		syn_clr(buf);
		updateScreen(CLEAR);
		return(0);
	}
	if (stricmp("load", reg) == 0)
	{
		syn_load(buf, p);
		updateScreen(CLEAR);
		return(0);
	}
	if (stricmp("color", reg) == 0)
	{
		rc = syn_color(buf, p);
		updateScreen(CLEAR);
		return(rc);
	}
	if (stricmp("link", reg) == 0)
	{
		rc = syn_link(buf, p);
		updateScreen(CLEAR);
		return(rc);
	}
	if (stricmp("crchar", reg) == 0)
	{
		rc = syn_crchar(buf, p);
		updateScreen(CLEAR);
		return(rc);
	}

	/* choice syntax type */
	if ((color = syn_get_color(buf, reg, &p, &lname)) == 0)
		return(1);

	/* get mode */
	while (*p != '\0' && *p != '/')
	{
		switch (toupper(*p)) {
		case 'I': l_ic		= TRUE;			break;
		case 'J': l_jic		= TRUE;			break;
		case 'W': l_word	= TRUE;			break;
		case 'P': l_type	= TYPE_PAIR;	break;
		case '-': l_last++;					break;
		case '+': l_last--;					break;
		case 'M': l_min		= TRUE;			break;
		case 'T': l_type	= TYPE_TAG;		break;
		default:							break;
		}
		p++;
	}
	if (*p == '\0')
		return(1);
	if (*p == '/')
		p++;

	magic = p_magic;
	reg_magic = TRUE;
	while (*p)
	{
		r = malloc(sizeof(syntax));
		memset(r, 0, sizeof(syntax));
		r->name		= lname;
		r->color	= color;
		r->ic		= l_ic;
		r->jic		= l_jic;
		r->word		= l_word;
		r->last		= l_last;
		r->min		= l_min;
		r->type		= l_type;
		nextp = skip_regexp(p, '/');
		if (*nextp == '/')
			*nextp++ = '\0';
		if (l_type || l_jic || syn_isregstr(p))
		{
			if (l_type != TYPE_TAG || tagfirst)
			{
				strcpy(pattern, p);
				if (l_word && l_type != TYPE_TAG)
				{
					strcpy(pattern, "\\<");
					strcat(pattern, p);
					strcat(pattern, "\\>");
				}
				if ((r->prog = syn_regcomp(l_ic, l_jic, pattern)) == NULL)
				{
					free(r);
					p_magic = magic;
					return(2);
				}
				if (l_type == TYPE_TAG || l_type == TYPE_PAIR)
					strcpy(pattern_l, pattern);
				if (l_type == TYPE_TAG)
					tagprog = strsave(pattern);
				if (l_type)
				{
					p = nextp;
					if (*p == '\0')
					{
						if (l_type == TYPE_TAG)
							free(tagprog);
						free(r->prog);
						free(r);
						p_magic = magic;
						return(2);
					}
					nextp = skip_regexp(p, '/');
					if (*nextp == '/')
						*nextp++ = '\0';
					strcpy(pattern, p);
					if (l_word && l_type != TYPE_TAG)
					{
						strcpy(pattern, "\\<");
						strcat(pattern, nextp);
						strcat(pattern, "\\>");
					}
					if ((r->progend = syn_regcomp(l_ic, l_jic, pattern)) == NULL)
					{
						if (l_type == TYPE_TAG)
							free(tagprog);
						free(r->prog);
						free(r);
						p_magic = magic;
						return(2);
					}
					if (l_type == TYPE_TAG || l_type == TYPE_PAIR)
						syn_addtag(buf, pattern_l, pattern, r->prog, r->progend);
					if (l_type == TYPE_TAG)
						tagprogend = strsave(pattern);
				}
			}
			if (l_type == TYPE_TAG)
			{
				if (!tagfirst)
				{
					if ((r->prog = syn_regcomp(l_ic, l_jic, tagprog)) == NULL)
					{
						free(tagprog);
						free(tagprogend);
						free(r);
						p_magic = magic;
						return(2);
					}
					if ((r->progend = syn_regcomp(l_ic, l_jic, tagprogend)) == NULL)
					{
						free(tagprog);
						free(tagprogend);
						free(r->prog);
						free(r);
						p_magic = magic;
						return(2);
					}
				}
				if (*p == '\0')
				{
					if (!tagfirst)
					{
						free(tagprog);
						free(tagprogend);
					}
					free(r->progend);
					free(r->prog);
					free(r);
					p_magic = magic;
					return(2);
				}
				if (tagfirst)
				{
					p = nextp;
					nextp = skip_regexp(p, '/');
					if (*nextp == '/')
						*nextp++ = '\0';
				}
				if (l_jic || syn_isregstr(p))
				{
					strcpy(pattern, p);
					if (l_word)
					{
						strcpy(pattern, "\\<");
						strcat(pattern, p);
						strcat(pattern, "\\>");
					}
					r->str = strsave(pattern);
				}
				else
				{
					r->type = TYPE_TAGP;
					r->str = syn_strsave(p);
				}
			}
			tagfirst = FALSE;
		}
		else
			r->str = syn_strsave(p);
		if (buf->b_syn_ptr == NULL)
			buf->b_syn_ptr = (char_u *)r;
		else
		{
			w = (syntax *)buf->b_syn_ptr;
			while (w->next)
				w = w->next;
			w->next = r;
#if SYNTAX_CACHE2
			r->prep = w;
#endif
#if SYNTAX_CACHE
			if (r->word && r->str)
			{
				char_u		*	p = r->str;
				int				hash	= 0;

				while (*p)
				{
					if (r->ic)
						hash += toupper(*p);
					else
						hash += *p;
					if (ISkanji(*p))
						p++;
					p++;
				}
				r->hash = hash % HASH_SIZE;
			}
#endif
		}
		p = nextp;
	}
	if (l_type == TYPE_TAG)
	{
		free(tagprog);
		free(tagprogend);
	}
	updateScreen(CLEAR);
	p_magic = magic;
	return(0);
}

static syntax *
ps_search(synp, top, ptr, find)
syntax		*	synp;
char_u		*	top;
char_u		*	ptr;
int				find;
{
	int					rc;

	if (ptr == NULL)
		ptr = top;
	if (synp->type == TYPE_TAG && syn_isregstr(synp->str))
	{
		regexp		*	prog;

		if ((prog = syn_regcomp(synp->ic, synp->jic, synp->str)) == NULL)
			return(NULL);
		rc = syn_regexec(synp->min, synp->ic, synp->jic, prog, ptr, top == ptr);
		if (rc)
		{
			synp->startpos	= prog->startp[0] - top;
			synp->endpos	= prog->endp[0] - top;
			free(prog);
			return(synp);
		}
		free(prog);
		return(NULL);
	}
	if (synp->str != NULL)
	{
		if (find)
		{
			if ((ptr = syn_strstr(ptr, synp->str, synp->ic, synp->word, synp->hash)) == NULL)
				return(NULL);
		}
		else
		{
			if (ISkanji(*ptr) || ISkana(*ptr))
				rc = *ptr == synp->str[0] ? 0 : 1;
			else if (synp->ic)
				rc = toupper(*ptr) == toupper(synp->str[0]) ? 0 : 1;
			else
				rc = *ptr == synp->str[0] ? 0 : 1;
			if (rc != 0)
				return(NULL);
			if (synp->ic)
				rc = vim_strnicmp(ptr, synp->str, strlen(synp->str));
			else
				rc = strncmp(ptr, synp->str, strlen(synp->str));
			if (rc != 0)
				return(NULL);
		}
		if (synp->word)
		{
			int			sclass;
			int			oclass;

			if (top != ptr)
			{
				sclass = syn_cls(utf_prev(top, ptr));
				oclass = syn_cls(ptr);
				if (sclass == oclass)
					return(NULL);
			}
			if (ptr[strlen(synp->str) + 0] != '\0')
			{
				sclass = syn_cls(utf_prev(ptr, ptr + strlen(synp->str)));
				oclass = syn_cls(ptr + strlen(synp->str));
				if (sclass == oclass)
					return(NULL);
			}
		}
		synp->startpos	= ptr - top;
		synp->endpos	= ptr + strlen(synp->str) - top;
		return(synp);
	}
	if (syn_regexec(synp->min, synp->ic, synp->jic, synp->prog, ptr, top == ptr))
	{
		synp->startpos	= synp->prog->startp[0] - top;
		synp->endpos	= synp->prog->endp[0] - top;
		return(synp);
	}
	return(NULL);
}

static syntax *
pe_search(synp, top, ptr, at_bol)
syntax		*	synp;
char_u		*	top;
char_u		*	ptr;
int				at_bol;
{
	if (synp->type == TYPE_PAIR)
	{
		if (syn_regexec(synp->min, synp->ic, synp->jic, synp->progend, ptr, at_bol))
		{
			synp->startpos	= synp->progend->startp[0] - top;
			synp->endpos	= synp->progend->endp[0] - top;
			return(synp);
		}
	}
	return(NULL);
}

static int
syn_tagchk(synp, buf, lnum, top, ptr)
syntax		*	synp;
BUF			*	buf; 			/* buffer we are a window into */
linenr_t		lnum;
char_u		**	top;
char_u		**	ptr;
{
	int				rc;
	linenr_t		pos  = *ptr - *top;
	linenr_t		line;

	if (!(synp->type == TYPE_TAG || synp->type == TYPE_TAGP))
		return(1);
	/* start position search */
	rc = syn_regexec(synp->min, synp->ic, synp->jic, synp->prog, *top, TRUE);
	if (rc == 0 || (*top + synp->endpos) <= synp->prog->startp[0])
	{
		for (line = lnum - 1; line > 0 && line >= (lnum - T_LINE(buf->b_ml.ml_line_count)); line--)
		{
			rc = syn_regexec(synp->min, synp->ic, synp->jic, synp->prog, ml_get_buf(buf, line, FALSE), TRUE);
			if (rc)
			{
				rc = syn_regexec(synp->min, synp->ic, synp->jic, synp->progend, synp->prog->endp[0], FALSE);
				while (rc)
				{
					rc = syn_regexec(synp->min, synp->ic, synp->jic, synp->prog, synp->progend->endp[0], FALSE);
					if (!rc)
						goto breakbreak;
					rc = syn_regexec(synp->min, synp->ic, synp->jic, synp->progend, synp->prog->endp[0], FALSE);
					if (!rc)
						break;
				}
				break;
			}
			rc = syn_regexec(synp->min, synp->ic, synp->jic, synp->progend, ml_get_buf(buf, line, FALSE), TRUE);
			if (rc)
				goto breakbreak;
		}
		if (line == 0)
			goto breakbreak;
		*top = ml_get_buf(buf, lnum, FALSE);
		*ptr = *top + pos;
	}
	/* end position search */
	rc = syn_regexec(synp->min, synp->ic, synp->jic, synp->progend, *top, TRUE);
	while (rc)
	{
		if ((*top + synp->endpos) <= synp->progend->startp[0])
			return(1);
		rc = syn_regexec(synp->min, synp->ic, synp->jic, synp->prog, synp->progend->endp[0], FALSE);
		if (!rc || (*top + synp->endpos) <= synp->prog->startp[0])
			return(0);
		rc = syn_regexec(synp->min, synp->ic, synp->jic, synp->progend, synp->prog->endp[0], FALSE);
	}
	for (line = lnum + 1; line <= buf->b_ml.ml_line_count && line <= (lnum + T_LINE(buf->b_ml.ml_line_count)); line++)
	{
		rc = syn_regexec(synp->min, synp->ic, synp->jic, synp->progend, ml_get_buf(buf, line, FALSE), TRUE);
		if (rc)
			break;
	}
	if (!rc)
		goto breakbreak;
	*top = ml_get_buf(buf, lnum, FALSE);
	*ptr = *top + pos;
	return(1);
breakbreak:
	*top = ml_get_buf(buf, lnum, FALSE);
	*ptr = *top + pos;
	return(0);
}

static syntax *
fwd_search(buf, lnum, top, ptr, ftop)
BUF			*	buf; 			/* buffer we are a window into */
linenr_t		lnum;
char_u		**	top;
char_u		**	ptr;
linenr_t	*	ftop;
{
	syntax			*	synp;
	syntax			*	topsynp = NULL;
#if SYNTAX_CACHE
	char_u			*	org = NULL;
#endif

	*ftop = MAX_COLS;
	for (synp = (syntax *)buf->b_syn_ptr; synp != NULL; synp = synp->next)
	{
		if (synp->type == TYPE_CRCH)
			continue;
#if SYNTAX_CACHE
		if (synp->word && synp->str && *ptr != org)
		{
			syn_makeidx(*ptr);
			org = *ptr;
		}
		if (synp->word && synp->str && synhash[synp->hash].cnt == 0)
			continue;
#endif
		if (ps_search(synp, *top, *ptr, TRUE) != NULL)
		{
			if (!syn_tagchk(synp, buf, lnum, top, ptr))
				continue;
			if (*ftop > synp->startpos)
			{
				*ftop	= synp->startpos;
				topsynp	= synp;
			}
			if (synp->startpos <= (*ptr - *top) && (*ptr - *top) < synp->endpos)
			{
				topsynp = synp;
				break;
			}
		}
	}
#if SYNTAX_CACHE2
	if (topsynp != NULL && topsynp->prep != NULL && topsynp->word)
	{
		syntax			*	prep = topsynp->prep;
		syntax			*	next = topsynp->next;

		synp = (syntax *)buf->b_syn_ptr;
		topsynp->prep = NULL;
		topsynp->next = synp;
		synp->prep = topsynp;
		prep->next = next;
		if (next != NULL)
			next->prep = prep;
		buf->b_syn_ptr = (char_u *)topsynp;
	}
#endif
	return(topsynp);
}

static syntax *
bak_search(buf, top, ptr, ftop)
BUF			*	buf; 			/* buffer we are a window into */
char_u		*	top;
char_u		*	ptr;
linenr_t	*	ftop;
{
	syntax			*	synp;
	syntax			*	topsynp	= NULL;
	linenr_t			fpos	= MAX_COLS;

	for (synp = (syntax *)buf->b_syn_ptr; synp != NULL; synp = synp->next)
	{
		if (pe_search(synp, top, ptr, TRUE) != NULL)
		{
			if (ftop == NULL || synp->endpos < *ftop)
			{
				if (synp->endpos < fpos)
				{
					fpos = synp->endpos;
					topsynp = synp;
				}
				if (ptr - top == fpos)
					break;
			}
		}
	}
	if (topsynp)
		pe_search(topsynp, top, ptr, TRUE);
	return(topsynp);
}

static void
syn_endcalc(buf, last)
BUF			*	buf; 			/* buffer we are a window into */
int				last;
{
	if (ISkanjiPointer(buf->b_syn_match, buf->b_syn_matchend - 1) == 2)
		buf->b_syn_matchend -= 1;
	while (last)
	{
		if (ISkanjiPointer(buf->b_syn_match, buf->b_syn_matchend) == 2)
		{
			if (last > 0)
			{
				if (buf->b_syn_match >= buf->b_syn_matchend - 2)
				{
					buf->b_syn_matchend = buf->b_syn_match;
					break;
				}
				buf->b_syn_matchend -= 2;
			}
			else
			{
				if (buf->b_syn_matchend[0] == '\0')
					break;
				buf->b_syn_matchend += 1;
			}
		}
		else
		{
			if (last > 0)
			{
				if (buf->b_syn_match >= buf->b_syn_matchend - 1)
					break;
				buf->b_syn_matchend -= 1;
			}
			else
			{
				if (buf->b_syn_matchend[0] == '\0')
					break;
				buf->b_syn_matchend += 1;
			}
		}
		if (ISkanjiPointer(buf->b_syn_match, buf->b_syn_matchend) == 1)
			buf->b_syn_matchend += 1;
		if (last > 0)
			last --;
		else
			last ++;
	}
}

void
syn_inschar(top, col)
char_u		*	top;
colnr_t			col;
{
	syntag			*	tagp;
	int					rc;
	char_u			*	str;
	regexp			*	prog;
	int					lr;

	for (tagp = (syntag *)curwin->w_buffer->b_syn_tag; tagp != NULL; tagp = tagp->next)
	{
		for (lr = 0; lr < 2; lr++)
		{
			str  = lr == 0 ? tagp->str_l  : tagp->str_r;
			prog = lr == 0 ? tagp->prog_l : tagp->prog_r;
			if (str)
			{
				colnr_t		len = strlen(str);
				colnr_t		i;

				/* broken tag */
				if (len > 1)
				{
					for (i = col >= len ? col - len : 0 ; i <= col; i++)
					{
						if (strncmp(&top[i], str, len) == 0)
						{
							if (i < col && col < i + len)
							{
								if (syn_strstr(&top[col], tagp->str_r, FALSE, FALSE, 0) != NULL)
									return;
								must_redraw = NOT_VALID;		/* buffer changed */
								return;
							}
						}
					}
				}
				/* complete tag */
				if (col >= (len - 1)
						&& strncmp(&top[col - (len - 1)], str, len) == 0)
				{
					if (lr == 0 && syn_strstr(&top[col], tagp->str_r, FALSE, FALSE, 0) != NULL)
						return;
					must_redraw = NOT_VALID;		/* buffer changed */
					return;
				}
			}
			else if (syn_regexec(TRUE, FALSE, FALSE, prog, top, TRUE))
			{
				rc = 1;
				while (rc)
				{
					/* broken tag */
					if (prog->startp[0] < top + col
											&& top + col < prog->endp[0] - 1)
					{
						must_redraw = NOT_VALID;		/* buffer changed */
						return;
					}
					/* complete tag */
					if (prog->endp[0] == top + col)
					{
						must_redraw = NOT_VALID;		/* buffer changed */
						return;
					}
					rc = syn_regexec(TRUE, FALSE, FALSE, prog, prog->endp[0], FALSE);
				}
			}
		}
	}
}

void
syn_delchar(top, col)
char_u		*	top;
colnr_t			col;
{
	syntag			*	tagp;
	int					rc;
	char_u			*	str;
	regexp			*	prog;
	int					lr;

	for (tagp = (syntag *)curwin->w_buffer->b_syn_tag; tagp != NULL; tagp = tagp->next)
	{
		for (lr = 0; lr < 2; lr++)
		{
			str  = lr == 0 ? tagp->str_l  : tagp->str_r;
			prog = lr == 0 ? tagp->prog_l : tagp->prog_r;
			if (str)
			{
				colnr_t		len = strlen(str);
				colnr_t		i;

				for (i = col >= len ? col - (len - 1) : 0 ; i <= col; i++)
				{
					if (strncmp(&top[i], str, len) == 0)
					{
						if (syn_strstr(&top[col], tagp->str_r, FALSE, FALSE, 0) != NULL)
							return;
						must_redraw = NOT_VALID;		/* buffer changed */
						return;
					}
				}
			}
			else if (syn_regexec(TRUE, FALSE, FALSE, prog, top, TRUE))
			{
				rc = 1;
				while (rc)
				{
					if (prog->startp[0] <= top + col
											&& top + col < prog->endp[0])
					{
						must_redraw = NOT_VALID;		/* buffer changed */
						return;
					}
					rc = syn_regexec(TRUE, FALSE, FALSE, prog, prog->endp[0], FALSE);
				}
			}
		}
	}
}

	int
is_crsyntax(wp)
WIN			*	wp;
{
	syntax			*	w;

	if (!wp->w_p_syt)
		return(0);
	w = (syntax *)wp->w_buffer->b_syn_ptr;
	while (w)
	{
		if (w->type == TYPE_CRCH)
			return(w->color);
		w = w->next;
	}
	return(0);
}

	int
is_syntax(wp, lnum, top, ptr)
WIN			*	wp;
linenr_t		lnum;
char_u		**	top;
char_u		**	ptr;
{
	BUF				*	buf		= wp->w_buffer;
	syntax			*	synp	= (syntax *)buf->b_syn_curp;
	linenr_t			ftop	= 0;
	linenr_t			lpos	= *ptr - *top;
	syntax			*	pep;
	syntax			*	wrkp;
	linenr_t			startpos;
	linenr_t			ldiff	= 0;
	linenr_t			nline	= buf->b_syn_nline;

	if (!wp->w_p_syt)
		return(0);
	if (buf->b_syn_ptr == NULL)
		return(0);
	if (buf->b_syn_line != lnum)
	{
		ldiff = lnum - buf->b_syn_line;
		buf->b_syn_nline = 0;
	}
	else if (synp)
	{
		if (buf->b_syn_match <= *ptr && *ptr < buf->b_syn_matchend)
		{
			if (*ptr >= (buf->b_syn_matchend - 1))
			{
				buf->b_syn_match	= NULL;
				buf->b_syn_matchend	= NULL;
				buf->b_syn_curp		= NULL;
			}
			return(synp->color);
		}
		return(0);
	}
	buf->b_syn_line		= lnum;
	buf->b_syn_match	= NULL;
	buf->b_syn_matchend	= NULL;
	buf->b_syn_curp		= NULL;
	if (*ptr == NULL || (*ptr != NULL && **ptr == '\0'))
		return(0);
	if (*top == *ptr)
	{
		linenr_t			high;
		linenr_t			low;
		linenr_t			higher;
		int					count = 0;

		high	= lnum;
		higher	= lnum + S_LINE(buf->b_ml.ml_line_count);
		if (ldiff == 0 || ldiff == 1)
			high = nline > lnum ? nline + 1 : lnum;
		else if (ldiff == -1)
			higher = nline == 0 ? lnum + S_LINE(buf->b_ml.ml_line_count) : lnum;
		else
			high = lnum;

		if ((pep = bak_search(buf, *top, *ptr, NULL)) != NULL)
		{
			startpos = pep->endpos;
			if ((synp = ps_search(pep, *ptr, NULL, FALSE)) != NULL)
			{
				if (startpos <= synp->startpos)
				{
					buf->b_syn_match	= ml_get_buf(buf, lnum, FALSE);
					buf->b_syn_matchend	= ml_get_buf(buf, lnum, FALSE) + startpos;
					buf->b_syn_curp		= (char_u *)pep;
					syn_endcalc(buf, pep->last);
					if (*ptr >= (buf->b_syn_matchend - 1))
					{
						buf->b_syn_match	= NULL;
						buf->b_syn_matchend	= NULL;
						buf->b_syn_curp		= NULL;
					}
					return(pep->color);
				}
			}
		}

		for (pep = NULL; high <= buf->b_ml.ml_line_count && high <= higher; high++)
		{
			*top = ml_get_buf(buf, high, FALSE);
			if ((pep = bak_search(buf, *top, *top, NULL)) != NULL)
			{
				startpos = pep->endpos;
				if ((synp = ps_search(pep, *top, NULL, FALSE)) != NULL)
				{
					if (startpos <= synp->startpos)
						break;
					goto breakbreak;
				}
				break;
			}
		}
		if (pep == NULL)
			buf->b_syn_nline = high - 1;
		else
		{
			startpos = pep->endpos;
			for (low = high - 1; low > 0 && low >= lnum - S_LINE(buf->b_ml.ml_line_count); low--)
			{
				*top = ml_get_buf(buf, low, FALSE);
				if ((synp = ps_search(pep, *top, NULL, FALSE)) != NULL)
				{
					if (low > lnum)
						goto breakbreak;
					if (pe_search(pep, *top, *top + pep->endpos, FALSE) != NULL)
						goto breakbreak;
					*ptr = *top = ml_get_buf(buf, lnum, FALSE);
					if (lnum == high)
					{
						buf->b_syn_match	= *top;
						buf->b_syn_matchend	= *top + startpos;
					}
					else if (lnum == low)
					{
						buf->b_syn_match	= *top + synp->startpos;
						buf->b_syn_matchend	= *top + strlen(*top);
						if (((wrkp = fwd_search(buf, lnum, top, ptr, &ftop)) != NULL) && wrkp->startpos < synp->startpos)
							break;
					}
					else
					{
						buf->b_syn_match	= *top;
						buf->b_syn_matchend	= *top + strlen(*top);
					}
					buf->b_syn_curp		= (char_u *)synp;
					syn_endcalc(buf, 0);
					if (buf->b_syn_match <= *ptr && *ptr < buf->b_syn_matchend)
					{
						if (*ptr >= (buf->b_syn_matchend - 1))
						{
							buf->b_syn_match	= NULL;
							buf->b_syn_matchend	= NULL;
							buf->b_syn_curp		= NULL;
						}
						return(synp->color);
					}
					return(0);
				}
				if (pe_search(pep, *top, *top, FALSE) != NULL)
					break;
			}
		}
breakbreak:
		*ptr = *top = ml_get_buf(buf, lnum, FALSE);
	}
	if ((synp = fwd_search(buf, lnum, top, ptr, &ftop)) != NULL)
	{
		startpos = synp->startpos;
		if ((pep = bak_search(buf, *top, *ptr, &ftop)) != NULL)
			synp = pep;
		else if (synp->type == TYPE_PAIR)
		{
			if (pe_search(synp, *top, *top + startpos, startpos == 0) != NULL)
				synp->startpos = startpos;
			else
			{
				linenr_t			high;
				linenr_t			higher;

				higher	= lnum + S_LINE(buf->b_ml.ml_line_count);
				for (pep = NULL, high = lnum + 1; high <= buf->b_ml.ml_line_count && high <= higher; high++)
				{
					*top = ml_get_buf(buf, high, FALSE);
					if ((pep = pe_search(synp, *top, *top, FALSE)) != NULL)
						break;
				}
				*top = ml_get_buf(buf, lnum, FALSE);
				*ptr = *top + lpos;
				if (pep == NULL)
					return(0);
				synp->startpos = startpos;
				synp->endpos   = startpos + strlen(*top + startpos);
			}
		}
		buf->b_syn_match	= ml_get_buf(buf, lnum, FALSE) + synp->startpos;
		buf->b_syn_matchend	= ml_get_buf(buf, lnum, FALSE) + synp->endpos;
		buf->b_syn_curp = (char_u *)synp;
		syn_endcalc(buf, synp->last);
		if (buf->b_syn_match <= *ptr && *ptr < buf->b_syn_matchend)
		{
			if (*ptr >= (buf->b_syn_matchend - 1))
			{
				buf->b_syn_match	= NULL;
				buf->b_syn_matchend	= NULL;
				buf->b_syn_curp		= NULL;
			}
			return(synp->color);
		}
	}
	if (ftop == MAX_COLS)
	{
		buf->b_syn_match	= *ptr;
		buf->b_syn_matchend	= *ptr + strlen(*ptr);
		buf->b_syn_curp		= (char_u *)&defcolor;
		syn_endcalc(buf, 0);
		if (*ptr >= (buf->b_syn_matchend - 1))
		{
			buf->b_syn_match	= NULL;
			buf->b_syn_matchend	= NULL;
			buf->b_syn_curp		= NULL;
		}
	}
	return(0);
}
#endif
