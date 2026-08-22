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

/*
 * vim.h comes first: USE_SYNTAX is decided there, so the guard cannot be put
 * in front of it the way it used to be when the condition was spelled out.
 */
#include "vim.h"

#ifdef USE_SYNTAX

#include "globals.h"
#include "proto.h"
#include "param.h"
#include "kanji.h"
#include "regexp.h"

#define MAX_COLS		0x7fffffff
/*
 * How far a tag search looks for the tag it is inside of. Pair regions used to
 * be found the same way and had their own, wider window; they are remembered
 * per line now instead, so 'synlines' only reaches the tag search.
 */
#define	T_LINE(_max)	(p_synl > 0 ? p_synl : ((_max) > (Rows * 10) ? Rows * 2 : Rows * 1))

#define SYNTAX_CACHE	1

typedef struct _syntax {
	struct _syntax	*	next;
	char_u			*	name;
	/*
	 * What the rule was written as, kept only so that "syntax dump" can say
	 * which line of which rule file put a colour where. A rule that matches
	 * the wrong thing is otherwise silent -- it colours something, or nothing,
	 * and there is no way to ask which of two hundred rules did it.
	 */
	char_u			*	pat;
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

/*
 * The rule the last is_syntax() answered with. Only "syntax dump" reads it:
 * is_syntax() returns a colour, and several rules can share one, so this is the
 * only way to say which rule to go and look at.
 */
static syntax		*	syn_hit = NULL;

#if SYNTAX_CACHE
typedef struct {
	char_u			*	idx;
	int					cnt;
} synindex;
# define HASH_SIZE		0x400
static synindex			synhash[HASH_SIZE];
#endif

	void
syn_clr(BUF *buf)
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
		if (twp->pat)
			free(twp->pat);
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
	/* The rules the line states were worked out from are gone with them. */
	if (buf->b_syn_state != NULL)
		free(buf->b_syn_state);
	buf->b_syn_state	= NULL;
	buf->b_syn_statelen	= 0;
	buf->b_syn_stateval	= 0;
	buf->b_syn_pairs	= 0;
}

static int
syn_regexec(int min, int ic, int jic, regexp *prog, char_u *ptr, int at_bol)
{
	char_u			*	startp;
	char_u			*	endp;
	char_u			*	lastp;
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
		/*
		 * Cut the last character off and see whether the match survives: that
		 * is how the "m" mode finds the shortest one. The cut has to land on a
		 * character boundary, and a UTF-8 character is up to four bytes, not
		 * the two this used to step back over. Stopping at an empty match also
		 * keeps a pattern that matches nothing out of an endless loop.
		 */
		if (endp <= startp)
			break;
		lastp = utf_prev(startp, endp);
		c = *lastp;
		*lastp = '\0';
		rc = regexec(prog, startp, at_bol);
		*lastp = c;
	}
	p_magic = magic;
	prog->startp[0]	= startp;
	prog->endp[0]	= endp;
	return(1);
}

static regexp *
syn_regcomp(int ic, int jic, char_u *string)
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
syn_isregstr(char_u *str)
{
	while (*str)
	{
		/*
		 * A multi-byte character is skipped whole: the trailing str++ below
		 * accounts for its first byte, so step over the rest here.
		 */
		if (ISkanji(*str))
			str += utf_lenat(str, 0) - 1;
		else if (*str == '\\')
		{
			str++;
			if (ISkanji(*str))
				str += utf_lenat(str, 0) - 1;
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
syn_cls(char_u *ptr)
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
/*
 * The bucket a word falls in. One byte per character, which is all the index
 * built by syn_makeidx() over the buffer text can afford to look at, and the
 * two have to arrive at the same number for a word to be found at all.
 */
static int
syn_hash(char_u *str, int ic)
{
	char_u		*	p		= str;
	int				hash	= 0;

	while (*p)
	{
		hash += ic ? toupper(*p) : *p;
		p += ISkanji(*p) ? utf_lenat(p, 0) : 1;
	}
	return(hash % HASH_SIZE);
}

static void
syn_makeidx(char_u *ptr)
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
		/*
		 * One byte per character goes into the hash, the same way syn_hash()
		 * builds the number a rule is looked up by. Only the stepping knows
		 * about character lengths.
		 */
		incase += *ptr;
		nocase += toupper(*ptr);
		ptr += ISkanji(*ptr) ? utf_lenat(ptr, 0) : 1;
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
syn_strstr(char_u *s1, char_u *s2, int ic, int word, int hash)
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
			s1 += ISkanji(*s1) ? utf_lenat(s1, 0) : 1;
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
				/*
				 * Every byte of the character has to match, however many it
				 * has. A short s1 stops at its NUL, which cannot equal a byte
				 * of s2, so this never reads past the end.
				 */
				int		len = utf_lenat(s2, pos);
				int		i;

				for (i = 0; i < len; i++)
					if (s1[pos + i] != s2[pos + i])
						break;
				if (i < len)
					break;
				pos += len - 1;
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
		/* The next place a match could start is the next character. */
		s1 += ISkanji(*s1) ? utf_lenat(s1, 0) : 1;
#if SYNTAX_CACHE
		if (word && synhash[hash].cnt == 1)
			return(NULL);
#endif
	}
	return(s1);
}

static char_u *
syn_strsave(char_u *p)
{
	char_u	*	w;

	w = p = strsave(p);
	while (*w)
	{
		/*
		 * A backslash escapes the character after it: the named ones become
		 * the control character they stand for, anything else just loses the
		 * backslash. A trailing backslash with nothing after it is left alone,
		 * so the walk cannot step past the end of the string.
		 */
		if (*w == '\\' && w[1] != '\0')
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
		w += ISkanji(*w) ? utf_lenat(w, 0) : 1;
	}
	return(p);
}

static void
syn_addtag(BUF *buf, char_u *string_l, char_u *string_r, regexp *regp_l, regexp *regp_r)
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
syn_color(BUF *buf, char_u *name)
{
	char_u			*	p;
	int					no;
	int					rgb = 0;

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
	usercolor[no].rgb = rgb;		/* 0xRRGGBB, the way it was written */
	return(0);
}

static int
syn_user_color(char_u id)
{
	int					no;

	for (no = 0; no < sizeof(usercolor) / sizeof(syncolor); no++)
	{
		if (usercolor[no].id == id)
			return(usercolor[no].rgb);
	}
	return(0);
}

/*
 * What a colour id asks for, for whoever is painting it -- the Win32 GUI with
 * a brush, a terminal with an SGR escape. Both ask here so that the two cannot
 * drift apart, which is the whole reason the palette is not written out twice.
 *
 * Returns the attributes and one of SYN_TEXT, SYN_REVERSE or SYN_RGB (with the
 * colour left in *rgb as 0xRRGGBB), or 0 when the id names no colour at all --
 * 'b' and 's', which are the bold and standout contexts of the 'highlight'
 * option rather than anything from a syntax rule, come back that way.
 */
	int
syn_decode(int id, int *rgb)
{
	int					attr = 0;

	/*
	 * A type is folded into the id by adding to it, so an id above the letters
	 * is a letter plus one of the four; syn_get_color() is where that is done.
	 */
	if (id >= 0x80)
	{
			 if (id <= 0x9f) { attr = SYN_BOLD;					id -= 0x40; }
		else if (id <= 0xbf) { attr = SYN_ITALIC;				id -= 0x60; }
		else if (id <= 0xdf) { attr = SYN_ULINE;				id -= 0x80; }
		else				 { attr = SYN_BOLD | SYN_ITALIC;	id -= 0xa0; }
	}
	switch (id) {
	case '@':	return(attr | SYN_REVERSE);
	case 'A':	return(attr | SYN_TEXT);				/* the text colour */
	case 'B':	*rgb = 0xffffff;	break;				/* white */
	case 'C':	*rgb = 0x000000;	break;				/* black */
	case 'D':	*rgb = 0xff0000;	break;				/* red */
	case 'E':	*rgb = 0x008000;	break;				/* green */
	case 'F':	*rgb = 0x0000ff;	break;				/* blue */
	case 'G':	*rgb = 0xffff00;	break;				/* yellow */
	case 'H':	*rgb = 0xff00ff;	break;				/* fuchsia */
	case 'I':	*rgb = 0xc0c0c0;	break;				/* silver */
	/*
	 * Not the gold of HTML, which is #ffd700 and unreadable on white. This is
	 * the colour the shipped rules have been drawn in since 2002 and the "+a"
	 * the manual means by "the sixteen HTML 3.2 colours and a bit".
	 */
	case 'J':	*rgb = 0x808000;	break;				/* gold */
	case 'K':	*rgb = 0x00ff00;	break;				/* lime */
	case 'L':	*rgb = 0x000080;	break;				/* navy */
	case 'M':	*rgb = 0x00ffff;	break;				/* aqua */
	case 'N':	*rgb = 0x808080;	break;				/* gray */
	case 'O':	*rgb = 0x800000;	break;				/* maroon */
	case 'P':	*rgb = 0x808000;	break;				/* olive */
	case 'Q':	*rgb = 0x800080;	break;				/* purple */
	case 'R':	*rgb = 0x008080;	break;				/* teal */
	case '[': case '\\': case ']': case '^': case '_':
	case 'V': case 'W': case 'X': case 'Y': case 'Z':
		*rgb = syn_user_color((char_u)id);
		break;
	default:
		return(0);							/* not a colour: 'b', 's', ... */
	}
	return(attr | SYN_RGB);
}

static int
syn_get_color(BUF *buf, char_u *name, char_u **p, char_u **lname)
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
syn_link(BUF *buf, char_u *name)
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

	/* An alias may not be named after a sub-command of ":syntax" */
		 if (stricmp("load",   name) == 0) return(1);
	else if (stricmp("clear",  name) == 0) return(1);
	else if (stricmp("color",  name) == 0) return(1);
	else if (stricmp("link",   name) == 0) return(1);
	else if (stricmp("dump",   name) == 0) return(1);
	else if (stricmp("crchar", name) == 0) return(1);
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
syn_loadtag(BUF *buf, char_u *fname)
{
	FILE			*	tp;
	char_u				lbuf[LSIZE];
	char_u				wbuf[2];
	char_u			*	wk;
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
			color = syn_get_color(buf, "tagsClass",		&wk, &lname);
			break;
		case 'd':	/* define	*/
			color = syn_get_color(buf, "tagsDefine",	&wk, &lname);
			break;
		case 'e':	/*enum value*/
			color = syn_get_color(buf, "tagsValue",		&wk, &lname);
			break;
		case 'f':	/* function	*/
			color = syn_get_color(buf, "tagsFunction",	&wk, &lname);
			break;
		case 'g':	/* enum		*/
			color = syn_get_color(buf, "tagsEnum",		&wk, &lname);
			break;
		case 'm':	/* member	*/
			color = syn_get_color(buf, "tagsMember",	&wk, &lname);
			break;
		case 'n':	/* namespaces	*/
			color = syn_get_color(buf, "tagsNames",		&wk, &lname);
			break;
		case 'p':	/* prototypes	*/
			color = syn_get_color(buf, "tagsProto",		&wk, &lname);
			break;
		case 's':	/* struct	*/
			color = syn_get_color(buf, "tagsStruct",	&wk, &lname);
			break;
		case 't':	/* typedef	*/
			color = syn_get_color(buf, "tagsTypedef",	&wk, &lname);
			break;
		case 'u':	/* union	*/
			color = syn_get_color(buf, "tagsUnion",		&wk, &lname);
			break;
		case 'v':	/* variable	*/
			color = syn_get_color(buf, "tagsVariable",	&wk, &lname);
			break;
		case 'x':	/* external	*/
			color = syn_get_color(buf, "tagsExternal",	&wk, &lname);
			break;
		default:
			color = syn_get_color(buf, "tagsUnknown",	&wk, &lname);
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
#if SYNTAX_CACHE
		/*
		 * Outside the branch below: a word rule that landed first in the list
		 * kept the hash of 0 it was allocated with, looked itself up in the
		 * wrong bucket, and so was never found in the text.
		 */
		if (r->word)
			r->hash = syn_hash(r->str, r->ic);
#endif
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
		breakcheck();
	}
	fclose(tp);
}

static void
syn_load(BUF *buf, char_u *fname)
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
syn_crchar(BUF *buf, char_u *name)
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

/*
 * One line of "syntax dump": the run of text from 'start' to 'end' and the rule
 * that coloured it, as
 *
 *     12:4-6 Conditional w/if
 *
 * Byte offsets into the line, the second one past the end. The rule is named by
 * its group and by the pattern as it was written, which between them say which
 * line of which file in syntax/ to go and change.
 */
static void
syn_dumprun(FILE *fp, linenr_t lnum, int start, int end, syntax *r)
{
	char_u			flags[16];
	char_u		*	f = flags;
	int				i;

	if (r == NULL || start >= end)
		return;
	if (r->ic)								*f++ = 'i';
	if (r->jic)								*f++ = 'j';
	if (r->word)							*f++ = 'w';
	if (r->min)								*f++ = 'm';
	if (r->type == TYPE_PAIR)				*f++ = 'p';
	if (r->type == TYPE_TAG
			|| r->type == TYPE_TAGP)		*f++ = 't';
	if (r->type == TYPE_CRCH)				*f++ = 'c';
	for (i = 0; i < r->last && f < flags + 10; i++)
		*f++ = '-';
	for (i = 0; i > r->last && f < flags + 10; i--)
		*f++ = '+';
	if (f == flags)							*f++ = 'n';		/* no mode at all */
	*f = NUL;
	fprintf(fp, "%ld:%d-%d %s %s/%s\n", (long)lnum, start, end,
			r->name != NULL ? (char *)r->name : "-",
			(char *)flags, r->pat != NULL ? (char *)r->pat : "");
}

/*
 * "syntax dump <file>": what the rules did to this buffer, as text.
 *
 * A rule that matches the wrong thing, or nothing, says so in no other way --
 * the screen simply comes out a colour short, and finding out which of two
 * hundred rules is responsible means reading pixels. This walks the buffer the
 * way the screen does, through is_syntax(), so what it reports is what would be
 * drawn, and writes one line per coloured run. Text no rule claimed is left
 * out: the lines that are there are the answers, and the rest is the question.
 */
static int
syn_dump(WIN *wp, char_u *fname)
{
	BUF				*	buf = wp->w_buffer;
	FILE			*	fp;
	linenr_t			lnum;
	char_u			*	top;
	char_u			*	ptr;
	int					clr;
	int					last;			/* colour of the run being collected */
	syntax			*	rule;
	int					start;
	int					off;

	if (fname == NULL || *fname == NUL)
		return(1);
	if (!wp->w_p_syt)
	{
		emsg((char_u *)"'syntax' is off, so nothing would be coloured");
		return(0);
	}
	if ((fp = fopen((char *)fileconvsto(fname), "w")) == NULL)
	{
		emsg2((char_u *)"Cannot open \"%s\" for writing", fname);
		return(0);
	}
	for (lnum = 1; lnum <= buf->b_ml.ml_line_count && !got_int; lnum++)
	{
		top = ptr = ml_get_buf(buf, lnum, FALSE);
		last  = 0;
		rule  = NULL;
		start = 0;
		while (*ptr != NUL)
		{
			syn_hit = NULL;
			clr = is_syntax(wp, lnum, &top, &ptr);
			off = (int)(ptr - top);		/* is_syntax() may fetch the line again */
			if (clr != last || syn_hit != rule)
			{
				syn_dumprun(fp, lnum, start, off, rule);
				start = off;
				last  = clr;
				rule  = clr ? syn_hit : NULL;
			}
			ptr += ISkanji(*ptr) ? utf_lenat(ptr, 0) : 1;
		}
		syn_dumprun(fp, lnum, start, (int)(ptr - top), rule);
		breakcheck();
	}
	fclose(fp);
	/*
	 * The walk above left the per-line cache pointing at the last line looked
	 * at, which is not where the screen is.
	 */
	buf->b_syn_line		= -1;
	buf->b_syn_match	= NULL;
	buf->b_syn_matchend	= NULL;
	buf->b_syn_curp		= NULL;
	return(0);
}

	int
syn_add(BUF *buf, char_u *reg)
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
	if (stricmp("dump", reg) == 0)
		return(syn_dump(curwin, p));
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
		case 'N':							break;	/* no mode, spelled out */
		default:
			{
				/*
				 * A letter nobody handles is a typo. The manual says the rest
				 * are ignored, and they were -- so a rule with one in it was
				 * added, matched something other than what was meant, and said
				 * nothing about why. "n" has to stay, because it is what every
				 * rule that wants no mode at all has been written with since
				 * 1998, and it too was landing here.
				 */
				char_u		bad[2];

				bad[0] = *p;
				bad[1] = NUL;
				emsg2((char_u *)"Unknown syntax mode \"%s\": the modes are i j w p t m n - +",
						bad);
				return(0);			/* said so already */
			}
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
		r->pat = strsave(p);	/* as written, for "syntax dump" to name it */
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
					free(r->pat);
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
						free(r->pat);
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
						free(r->pat);
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
						free(r->pat);
						free(r);
						p_magic = magic;
						return(2);
					}
					if ((r->progend = syn_regcomp(l_ic, l_jic, tagprogend)) == NULL)
					{
						free(tagprog);
						free(tagprogend);
						free(r->prog);
						free(r->pat);
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
					free(r->pat);
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
		if (r->type == TYPE_PAIR)
			buf->b_syn_pairs++;		/* whether the line states are worth keeping */
#if SYNTAX_CACHE
		/* Outside the branch below; see the same line in syn_loadtag(). */
		if (r->word && r->str)
			r->hash = syn_hash(r->str, r->ic);
#endif
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
		p = nextp;
	}
	if (l_type == TYPE_TAG)
	{
		free(tagprog);
		free(tagprogend);
	}
	/* A new rule can open a region the remembered line states knew nothing of. */
	buf->b_syn_stateval = 0;
	updateScreen(CLEAR);
	p_magic = magic;
	return(0);
}

static syntax *
ps_search(syntax *synp, char_u *top, char_u *ptr, int find)
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
pe_search(syntax *synp, char_u *top, char_u *ptr, int at_bol)
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
syn_tagchk(syntax *synp, BUF *buf, linenr_t lnum, char_u **top, char_u **ptr)
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
fwd_search(BUF *buf, linenr_t lnum, char_u **top, char_u **ptr, linenr_t *ftop)
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
bak_search(BUF *buf, char_u *top, char_u *ptr, linenr_t *ftop)
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

/*
 * What a line inherits from the ones above it.
 *
 * Only a pair rule -- the "p" search mode -- can run past the end of a line, so
 * the only thing a line needs to know about its predecessors is which pair
 * region, if any, was still open when they ended. That is one number per line:
 * b_syn_state[i] is the state at the start of line i + 1, held as the position
 * of the rule among the pair rules, one based, or 0 for nothing open. Entries
 * up to b_syn_stateval have been worked out; the rest have not been reached
 * yet, and anything a change makes doubtful is dropped by syn_changed().
 *
 * This is what lets a line be coloured on its own. It used to be guessed by
 * searching 'synlines' lines in each direction every time a line was drawn,
 * which cost searches per line and still got a comment or a string wrong once
 * it grew longer than that window.
 *
 * The state is held as a position rather than a pointer because the rule list
 * is thrown away and rebuilt by ":syntax clear", which every buffer runs on the
 * way in: a position that no longer names a rule reads back as "nothing open",
 * where a stale pointer would be read.
 */

static syntax *
syn_pair(BUF *buf, int no)
{
	syntax			*	synp;

	if (no <= 0)
		return(NULL);
	for (synp = (syntax *)buf->b_syn_ptr; synp != NULL; synp = synp->next)
		if (synp->type == TYPE_PAIR && --no == 0)
			return(synp);
	return(NULL);
}

static int
syn_pairno(BUF *buf, syntax *want)
{
	syntax			*	synp;
	int					no = 0;

	if (want == NULL)
		return(0);
	for (synp = (syntax *)buf->b_syn_ptr; synp != NULL; synp = synp->next)
	{
		if (synp->type != TYPE_PAIR)
			continue;
		no++;
		if (synp == want)
			return(no);
	}
	return(0);
}

/*
 * Run one line of the state machine: given what was open when the line began,
 * say what is still open when it ends.
 */
static syntax *
syn_linestate(BUF *buf, syntax *open, char_u *line)
{
	char_u			*	p = line;

	while (*p != '\0')
	{
		linenr_t			at = p - line;

		if (open != NULL)
		{
			if (pe_search(open, line, p, p == line) == NULL)
				return(open);			/* runs on into the next line */
			p = line + open->endpos;
			open = NULL;
		}
		else
		{
			syntax		*	best	= NULL;
			linenr_t		bestpos	= MAX_COLS;
			syntax		*	synp;

			for (synp = (syntax *)buf->b_syn_ptr; synp != NULL; synp = synp->next)
			{
				if (synp->type != TYPE_PAIR)
					continue;
				if (ps_search(synp, line, p, TRUE) == NULL)
					continue;
				if (synp->startpos < bestpos)
				{
					bestpos	= synp->startpos;
					best	= synp;
				}
			}
			if (best == NULL)
				return(NULL);			/* nothing else opens on this line */
			/* the loop above left the other rules' positions on top */
			(void)ps_search(best, line, p, TRUE);
			p = line + best->endpos;
			open = best;
		}
		/*
		 * A pattern that matches nothing would leave p where it was and spin
		 * here for ever, so make sure the line is always being consumed.
		 */
		if ((p - line) <= at)
			p = line + at + utf_lenat(line, (int)at);
	}
	return(open);
}

/*
 * Room for the states of lines 1..lnum. FALSE when there is none, which turns
 * multi-line colouring off rather than getting it wrong.
 */
static int
syn_stateroom(BUF *buf, linenr_t lnum)
{
	linenr_t			want;
	short			*	p;

	if (lnum > buf->b_syn_statelen)
	{
		/* The whole buffer at once: the states are asked for in order. */
		want = buf->b_ml.ml_line_count;
		if (want < lnum)
			want = lnum;
		if (want > (linenr_t)(0x7fffffffL / (long)sizeof(short)))
			return(FALSE);
		p = (short *)realloc(buf->b_syn_state, (size_t)want * sizeof(short));
		if (p == NULL)
			return(FALSE);
		buf->b_syn_state	= p;
		buf->b_syn_statelen	= want;
	}
	if (buf->b_syn_stateval < 1)
	{
		buf->b_syn_state[0]	= 0;		/* nothing is open before line 1 */
		buf->b_syn_stateval	= 1;
	}
	return(TRUE);
}

/*
 * The pair region open at the start of 'lnum', working out and remembering the
 * states of any lines in between that have not been reached yet.
 *
 * ml_get_buf() is called for those lines, so the caller's pointers into the
 * line it is drawing have to be fetched again afterwards.
 */
static syntax *
syn_state(BUF *buf, linenr_t lnum)
{
	syntax			*	open;
	linenr_t			n;

	/* With no region rule nothing can reach across a line, so keep nothing. */
	if (lnum < 1 || buf->b_syn_ptr == NULL || buf->b_syn_pairs <= 0)
		return(NULL);
	if (!syn_stateroom(buf, lnum))
		return(NULL);
	if (lnum <= buf->b_syn_stateval)
		return(syn_pair(buf, buf->b_syn_state[lnum - 1]));
	open = syn_pair(buf, buf->b_syn_state[buf->b_syn_stateval - 1]);
	for (n = buf->b_syn_stateval; n < lnum; n++)
	{
		open = syn_linestate(buf, open, ml_get_buf(buf, n, FALSE));
		buf->b_syn_state[n] = (short)syn_pairno(buf, open);
	}
	buf->b_syn_stateval = lnum;
	return(open);
}

/*
 * Line 'lnum' changed: the states of the lines after it are no longer known.
 * The state at the start of 'lnum' itself still is -- it was settled by the
 * lines above, which this did not touch.
 *
 * Typing the two characters that open a C comment changes the colour of
 * everything below, so the lines under the one that changed have to be drawn
 * again; only the changed line itself would be otherwise. Nothing is lowered
 * unless a region rule exists and some state was worked out from it, so a
 * buffer with no multi-line rules never asks for the extra redraw.
 */
	void
syn_changed(BUF *buf, linenr_t lnum)
{
	if (lnum < 1)
		lnum = 1;
	if (buf->b_syn_stateval > lnum)
	{
		buf->b_syn_stateval = lnum;
		if (must_redraw < NOT_VALID)
			must_redraw = NOT_VALID;
	}
}

static void
syn_endcalc(BUF *buf, int last)
{
	char_u		*	top = buf->b_syn_match;
	char_u		*	end = buf->b_syn_matchend;

	if (top == NULL || end == NULL || end <= top)
		return;
	/*
	 * Leave matchend one byte inside the last character rather than past it.
	 * is_syntax() drops its cache when the drawing loop reaches matchend - 1,
	 * and the loop only ever stops on a character's first byte, so pointing
	 * past the end would keep the cache alive after the loop had moved on.
	 * A one-byte character stays exactly where it was.
	 */
	end = utf_head(top, end - 1) + 1;
	/*
	 * What the "-" and "+" search modes asked for: take that many characters
	 * off the end, or add that many.
	 */
	for ( ; last > 0; last--)
	{
		if (end - 1 <= top)		/* nothing left to give back */
		{
			end = top;
			break;
		}
		end = utf_head(top, end - 2) + 1;
	}
	for ( ; last < 0; last++)
	{
		char_u	*	next = (end - 1) + utf_lenat(end - 1, 0);

		if (*next == '\0')
			break;
		end = next + 1;
	}
	buf->b_syn_matchend = end;
}

void
syn_inschar(char_u *top, colnr_t col)
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
syn_delchar(char_u *top, colnr_t col)
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
is_crsyntax(WIN *wp)
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
is_syntax(WIN *wp, linenr_t lnum, char_u **top, char_u **ptr)
{
	BUF				*	buf		= wp->w_buffer;
	syntax			*	synp	= (syntax *)buf->b_syn_curp;
	linenr_t			ftop	= 0;
	syntax			*	pep;
	linenr_t			startpos;
	linenr_t			endpos;

	if (!wp->w_p_syt)
		return(0);
	if (buf->b_syn_ptr == NULL)
		return(0);
	/* Still inside the span the last call worked out for this line? */
	if (buf->b_syn_line == lnum && synp)
	{
		if (buf->b_syn_match <= *ptr && *ptr < buf->b_syn_matchend)
		{
			if (*ptr >= (buf->b_syn_matchend - 1))
			{
				buf->b_syn_match	= NULL;
				buf->b_syn_matchend	= NULL;
				buf->b_syn_curp		= NULL;
			}
			syn_hit = synp;
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
		syntax			*	openp = syn_state(buf, lnum);

		/* syn_state() fetched other lines, which moves the one being drawn */
		*ptr = *top = ml_get_buf(buf, lnum, FALSE);
		if (openp != NULL)
		{
			/*
			 * A region that opened on an earlier line reaches this one, so it
			 * colours the line from the start to wherever it closes -- or all
			 * of it, when it closes later still.
			 */
			buf->b_syn_match = *top;
			if (pe_search(openp, *top, *top, TRUE) != NULL)
				buf->b_syn_matchend = *top + openp->endpos;
			else
				buf->b_syn_matchend = *top + strlen(*top);
			buf->b_syn_curp = (char_u *)openp;
			syn_endcalc(buf, openp->last);
			if (*ptr >= (buf->b_syn_matchend - 1))
			{
				buf->b_syn_match	= NULL;
				buf->b_syn_matchend	= NULL;
				buf->b_syn_curp		= NULL;
			}
			syn_hit = openp;
			return(openp->color);
		}
	}
	if ((synp = fwd_search(buf, lnum, top, ptr, &ftop)) != NULL)
	{
		/*
		 * Both ends of the match, kept before bak_search() runs: that asks
		 * every rule whether its closing token is here, and the answer is left
		 * on the rule this one is about to use.
		 */
		startpos = synp->startpos;
		endpos   = synp->endpos;
		if ((pep = bak_search(buf, *top, *ptr, &ftop)) != NULL)
			synp = pep;
		else if (synp->type == TYPE_PAIR)
		{
			/*
			 * Where the region closes, looked for past the token that opened
			 * it. Starting the search at the opening token instead let a
			 * region whose two tokens are the same string -- Python's """, a
			 * shell heredoc -- close on the very characters that opened it, so
			 * only the opening token was coloured and the text after it was
			 * left plain. A region with two different tokens cannot match
			 * there and never noticed the difference.
			 */
			if (pe_search(synp, *top, *top + endpos, endpos == 0) != NULL)
				synp->startpos = startpos;
			else
			{
				/*
				 * The region opens here and does not close on this line, so it
				 * colours the rest of it; syn_state() reports it as open at
				 * the start of the next line and the colour carries on there.
				 * This used to search ahead for the closing token and, if it
				 * was more than 'synlines' away, leave the region uncoloured
				 * altogether.
				 */
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
			syn_hit = synp;
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
