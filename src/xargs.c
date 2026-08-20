/* wild card expand routine
 *  for Turbo-C, MS-C, LSI-C..
 *
 *  This code is in the public domain.
 *
 *  Bug for use of wild card in directory part was fixed.
 *
 * $Id: xargs1.c 1.9a 92/05/03 06:00:00 AssistantIO Exp $
 *
 *  Bug for DOT_DLM=0 was fixed.
 *
 * $Id: xargs1.c 1.9 92/03/16 16:30:57 AssistantIO Exp $
 *
 *  Buggy code was fixed, macro became externaly definable.
 *
 * $Id: xargs1.c 1.8 91/11/24 08:15:12 AssistantIO Exp $
 *
 *  Configuration from file was added.
 *
 * $Id: xargs1.c 1.7 91/10/27 17:15:12 AssistantIO Exp $
 *
 *  Definition of ustrcmp() was changed.
 *
 * $Id: xargs1.c 1.6 91/08/31 09:15:12 AssistantIO Exp $
 *
 *  Recursive globbing was added.
 *  Use of character class regexp was added.
 *
 * $Id: xargs1.c 1.5 91/04/17 08:51:50 AssistantIO Exp $
 */
/*
 * add glob_filename	K.Tsuchida   ken_t@st.rim.or.jp
 */
/* WindowsNT Support (enclosed bt "WIN32")
 *   Modified by Tomonori.Watanabe(Tom_W)  GCD02235@niftyserve.or.jp
 */
#ifndef USE_CFG
#define USE_CFG 0		/* if 1, configuratin from file */
#endif
#ifndef USE_CCL
#define USE_CCL 1		/* if 0, to the original source */
#endif
#ifndef DOT_DLM
#define DOT_DLM 0		/* if 0, '.' is not delimiter */
#endif

/* wild card expand routine
 *  for Turbo-C, MS-C, LSI-C..
 *
 * $Id: xargs.c 1.2 90/12/28 21:51:50 serow Exp Locker: serow $
 */

/* #include <stdio.h> */
#include <string.h>
#include <io.h>
#include <stdlib.h>
#ifdef	WIN32			/*Tom_W*/
# include <windows.h>
#else
# include <dos.h>
#endif

#define PSP	_psp

#ifndef ArgsAttr
#define ArgsAttr	0x3f	/*	1	/* Default attribute when file search */
#endif
#ifndef DirAttr
#define DirAttr		0x3f	/*	16	/* Default attribute when dir search */
#endif
#ifndef DO_SORT
#define DO_SORT	1	/* NON 0:wild card args will be sorted by name */
#endif
#ifndef CVLOWER
# ifndef WIN32
#  define CVLOWER	1	/* if 1, wildargs to lower */
# else
#  define CVLOWER	0	/* if 1, wildargs to lower */
# endif
#endif

#if USE_CCL
#define LBRACE	0x7f
#if DOT_DLM
#define is_delimiter(c)	((c) == '\0' || (c) == '.')
#else
#define is_delimiter(c)	((c) == '\0')
#endif

#define IGNORE_CASE	1

#ifdef  isupper
# undef isupper
#endif
#define isupper(c)	((c) >= 'A' && (c) <= 'Z')
#define tolower(c)	((c) | 0x20)
#endif /* USE_CCL */

#ifdef  isspace
# undef isspace
#endif
#define isspace(c)	((c) == ' ' || (c) == '\t' || (c) == '\n' )
#define UCH(c)  ((unsigned char)(c))
#define isk1(c) ((0x81<=UCH(c)&&UCH(c)<=0x9F)||(0xE0<=UCH(c)&&UCH(c)<=0xFC))
#define isk2(c) ((0x40<=UCH(c)&&UCH(c)<=0x7E)||(0x80<=UCH(c)&&UCH(c)<=0xFC))

#define REALLOC_UNIT	256/*128*/

#ifdef LDATA
#undef LDATA
#endif

#ifdef	WIN32		/*Tom_W*/
typedef	struct {
	HANDLE 			hFind;
	WIN32_FIND_DATA 	data;
} DTA_BUF;
#define	d_attribute	data.dwFileAttributes
#undef	ArgsAttr
#define ArgsAttr	FILE_ATTRIBUTE_NORMAL
#undef	DirAttr
#define DirAttr		FILE_ATTRIBUTE_DIRECTORY

#else	/*WIN32*/
#ifdef __TURBOC__
#if (sizeof(char *) == 4)
#define LDATA	1
#endif
#else /* may be MS-C */
#if defined(M_I86LM) || defined(M_I86CM) || defined(M_I86HM)
#define LDATA	1
#endif
#endif

#if defined(LSI_C)
/* union REGS dose not have '.cflag' but '.flags' */
#else
#define HAVE_CFLAG	1
#endif

#ifdef FP_OFF
#undef FP_OFF
#endif
#ifdef FP_SEG
#undef FP_SEG
#endif
#ifdef MK_FP
#undef MK_FP
#endif
#define FP_OFF(fp) ((unsigned int)(fp))
#define FP_SEG(fp) ((unsigned int)((unsigned long)(fp) >> 16))
#define MK_FP(sg,of) ((void far *)(((unsigned long)(sg) << 16)|(unsigned)(of)))

typedef struct {
	char           d_buf[21];
	char           d_attribute;
	unsigned short d_time;
	unsigned short d_date;
	long           d_size;
	char           d_name[13];
} DTA_BUF;

#if !USE_CCL
static DTA_BUF    dtabuf;
#endif
static union REGS reg, nreg;

#ifdef LDATA
static struct SREGS sreg;
#endif
#endif	/*WIN32*/

static char     ERNEM[] = "not enought mem\r\n";
/***************************************************************/

#if USE_CCL
static int regcmp(char *t, char *p)
{
	int r, include, exclude;
	unsigned int ks, ke, kp, kt;
	char *s, *u;

	while (*p) {
		switch (*p) {
		case LBRACE:
			if (is_delimiter(*t)) {
				return 0;
			}
			s = t;
			u = s;
			p++;
			do {
				t = s;
				r = 1;
				do {
					if (is_delimiter(*p)) {
						return -1;
					}
					kt = UCH(*t++);
					if (isk1(kt) && isk2(*t)) {
						kt = (kt << 8) + UCH(*t++);
					}
					kp = UCH(*p++);
					if (isk1(kp) && isk2(*p)) {
						kp = (kp << 8) + UCH(*p++);
					}
#if IGNORE_CASE
					if (isupper(kt)) {
						kt = tolower(kt);
					}
					if (isupper(kp)) {
						kp = tolower(kp);
					}
#endif
					if (kt != kp) {
						r = 0;
					}
				} while (*p != ',' && *p != '}' && *p != '\0');
				if (*p == '\0')
					return 0;
				if (r == 1 && t - s > u - s) {
					u = t;
				}
				if (*p == ',') {
					p++;
				}
			} while (*p != '}' && *p != '\0');
			if (*p == '\0')
				return 0;
			if (u == s) {
				return 0;
			}
			p++;
			t = u;
			break;
		case '?':
			if (is_delimiter(*t)) {
				return 0;
			}
			if (isk1(*t) && isk2(*(t + 1))) {
				t++;
			}
			t++;
			p++;
			break;
		case '*':
			while (!((r = regcmp(t, p + 1)) != 0
					 || is_delimiter(*t))) {
				if (isk1(*t) && isk2(*(t + 1))) {
					t++;
				}
				t++;
			}
			return r;
		case '[':
			if (is_delimiter(*t)) {
				return 0;
			}
			include = 0;
			exclude = 0;
			p++;
			if (*p == '^') {
				exclude = 1;
				p++;
			}
			if (*p == '\0')
				return 0;
			kt = UCH(*t++);
			if (isk1(kt) && isk2(*t)) {
				kt = (kt << 8) + UCH(*t++);
			}
#if IGNORE_CASE
			if (isupper(kt)) {
				kt = tolower(kt);
			}
#endif
			do {
				if (is_delimiter(*p)) {
					return -1;
				}
				ks = UCH(*p++);
				if (isk1(ks) && isk2(*p)) {
					ks = (ks << 8) + UCH(*p++);
				}
				ke = ks;
				if (*p == '-' && *(p + 1) != ']') {
					p++;
					if (*p == '\0')
						return 0;
					if (is_delimiter(*p)) {
						return -1;
					}
					ke = UCH(*p++);
					if (isk1(ke) && isk2(*p)) {
						ke = (ke << 8) + UCH(*p++);
					}
				}
#if IGNORE_CASE
				if (isupper(ks)) {
					ks = tolower(ks);
				}
				if (isupper(ke)) {
					ke = tolower(ke);
				}
#endif
				if (kt >= ks && kt <= ke) {
					include = 1;
				}
			} while (*p != ']' && *p != '\0');
			if (*p == '\0')
				return 0;
			if (include == exclude) {
				return 0;
			}
			p++;
			break;
#if DOT_DLM
		case '.':
			if (!(is_delimiter(*t))) {
				return 0;
			}
			if (*t == '.') {
				t++;
			}
			p++;
			break;
#endif
		default:
			kt = UCH(*t++);
			if (isk1(kt) && isk2(*t)) {
				kt = (kt << 8) + UCH(*t++);
			}
			kp = UCH(*p++);
			if (isk1(kp) && isk2(*p)) {
				kp = (kp << 8) + UCH(*p++);
			}
#if IGNORE_CASE
			if (isupper(kt)) {
				kt = tolower(kt);
			}
			if (isupper(kp)) {
				kp = tolower(kp);
			}
#endif
			if (kt != kp) {
				return 0;
			}
		}
	}
	return *t == '\0' ? 1 : 0;
}
#endif /* USE_CCL */

static void error(char * s)
{
	write(2, "xargs: ", 7);
	write(2, s, strlen(s));
	exit(1);
}

#if USE_CCL
#ifdef	WIN32		/*Tom_W*/
/*  In WindowsNT support you must set USE_CCL=1 !!  */
static char *find_file(char *dir, int attr, DTA_BUF *dtabuf)
{

	if (dir != (char *) 0) {	/* get first entry */
		if ((dtabuf->hFind = FindFirstFile(dir, &dtabuf->data))
						 == INVALID_HANDLE_VALUE)
			return (char *)0;
	} else {			/* get next entry */
		if (!FindNextFile(dtabuf->hFind, &dtabuf->data)) {
			FindClose(dtabuf->hFind);
			return (char *)0;
		}
	}
	return dtabuf->data.cFileName;
}
#else	/*!WIN32*/
static void set_dta(DTA_BUF *dtabuf)
{
	reg.h.ah = 0x1A;
#ifdef LDATA
	reg.x.dx = FP_OFF(dtabuf);
	sreg.ds = FP_SEG(dtabuf);
	intdosx(&reg, &nreg, &sreg);
#else
	reg.x.dx = (unsigned) dtabuf;
	intdos(&reg, &nreg);
#endif
}

static char *find_file(char *dir, int attr, DTA_BUF *dtabuf)
{
	set_dta(dtabuf);	/* setup DTA */
	if (dir != (char *) 0) { /* get first entry */
		reg.h.ah = 0x4E;
		reg.h.cl = (char) attr;
#ifdef LDATA
		reg.x.dx = FP_OFF(dir);
		sreg.ds = FP_SEG(dir);
#else
		reg.x.dx = (unsigned) dir;
#endif
	} else {			/* get next entry */
		reg.h.ah = 0x4F;
	}
#ifdef LDATA
	intdosx(&reg, &nreg, &sreg);
#else
	intdos(&reg, &nreg);
#endif

#ifdef HAVE_CFLAG
	if (nreg.x.cflag)
#else
	if (nreg.x.flags & 1)
#endif
		return (char *) 0;

	if (dtabuf->d_name[0] == 5) dtabuf->d_name[0] = 0xE5;
	return dtabuf->d_name;
}

#endif	/*WIN32*/
#else /* !USE_CCL */

static void set_dta(void)
{
	reg.h.ah = 0x1A;
#ifdef LDATA
	reg.x.dx = FP_OFF(&dtabuf);
	sreg.ds = FP_SEG(&dtabuf);
	intdosx(&reg, &nreg, &sreg);
#else
	reg.x.dx = (int) &dtabuf;
	intdos(&reg, &nreg);
#endif
}

static char *find_file(char *dir)
{
	if (dir != (char *) 0) { /* get first entry */
		set_dta();	/* setup DTA */
		reg.h.ah = 0x4E;
		reg.h.cl = ArgsAttr;
#ifdef LDATA
		reg.x.dx = FP_OFF(dir);
		sreg.ds = FP_SEG(dir);
#else
		reg.x.dx = (unsigned) dir;
#endif
	} else {			/* get next entry */
		reg.h.ah = 0x4F;
#ifdef LDATA
		reg.x.dx = FP_OFF(&dtabuf);
		sreg.ds = FP_SEG(&dtabuf);
#else
		reg.x.dx = (unsigned) &dtabuf;
#endif
	}
#ifdef LDATA
	intdosx(&reg, &nreg, &sreg);
#else
	intdos(&reg, &nreg);
#endif

#ifdef HAVE_CFLAG
	if (nreg.x.cflag)
#else
	if (nreg.x.flags & 1)
#endif
		return (char *) 0;

	if (dtabuf.d_name[0] == 5) dtabuf.d_name[0] = 0xE5;
	return dtabuf.d_name;
}

#endif /* USE_CCL */

#if CVLOWER
static char * jstrtolower(char *p)
{
	char *op = p;

	while (*p) {
		if (isk1(*p) && p[1])
			p++;
		else if (('A' <= *p) && (*p <= 'Z'))
			*p |= 0x20;
		p++;
	}
	return op;
}
#else
#define jstrtolower(s)	(s)
#endif

static char * jrindex(char * p, char c)
{
	char           *oldp;

	for (oldp = (char *)0; *p; p++) {
		if (isk1(*p) && isk2(p[1])) {
			p++;
		} else if (*p == c) {
			oldp = p;
		}
	}
	return oldp;
}

#if DO_SORT
static int ustrcmp(const void *s1, const void *s2)
{
	unsigned char * p1 = *(unsigned char **)s1;
	unsigned char * p2 = *(unsigned char **)s2;

# if 0
	while ((*p1 != '\0') && (*p1 == *p2)) {
		p1++;
		p2++;
	}
	return *p1 - *p2;
# else
	return (stricmp(p1, p2));
# endif
}
#endif

static char * xalloc(size_t n)
{
	char  *bp;

	if ((bp = malloc(n)) == (char *) 0)
		error(ERNEM);
	return bp;
}

static char *xrealloc(void *blk, size_t n)
{
	char  *bp;

	if ((bp = realloc(blk, n)) == (char *) 0)
		error(ERNEM);
	return bp;
}

#if USE_CCL
int
expand_arg(char *arg, char *wld,
	char ***argbuf, int *argcnt, size_t *absiz)
{
	char *ap, *qp, *pp, *wp, *cp, *dp;
	int r, l, c, ap_keep;
	DTA_BUF    dtabuf;
	static char *work;

	if (wld == NULL) {
		work = xalloc(strlen(arg) + 128 + 1);
		work[0] = '\0';
		wld = work;
	}
	ap = arg;
	qp = wld;
	pp = arg;
	wp = NULL;
	dp = NULL;
	while (*ap != '\0'
		   && (wp == NULL || (*ap != '\\' && *ap != '/'))) {
		if (isk1(*ap) && ap[1] != '\0') {
			*qp++ = *ap++;
			*qp++ = *ap++;
		} else {
			if (wp == NULL
				&& (*ap == '*' || *ap == '?'
					|| *ap == '[' || *ap == LBRACE)) {
				wp = qp;
			} else if (wp == NULL
					   && (*ap == ':' || *ap == '\\' || *ap == '/')) {
				pp = ap + 1;
				wld = qp + 1;
#if !DOT_DLM
				dp = NULL;
			} else if (wp == NULL && *ap == '.') {
#else
			} else if (wp != NULL && *ap == '.') {
#endif
				dp = qp;
			}
			c = *ap++;
#ifndef	WIN32			/* ken */
			if (isupper(c)) {
				c = tolower(c);
			}
#endif
			*qp++ = (char) c;
		}
	}
	*qp = '\0';
	if (wp) {
#if DOT_DLM
		if (dp) {
#else
		if (!dp) {
#endif
			strcpy(wp, "*.*");
		} else {
			strcpy(wp, "*");
		}
	}
	ap_keep = *ap;
	if ((cp = find_file(work, ap_keep ? DirAttr : ArgsAttr, &dtabuf)) != (char *)0) {
		*ap = '\0';
		*wld = '\0';
		do {
#ifdef	WIN32			/*Tom_W*/
			/* NOTE: WindowsNT support unix like file name,
			 *      such as ".profile"!!
			 */
			if (strcmp(cp, ".") != 0 && strcmp(cp, "..") != 0) {
			    if ((r = regcmp(cp, pp)) == 1) {
#else
			if (*cp != '.' && (r = regcmp(cp, pp)) == 1) {
#endif
				if (ap_keep == '\0') {
					strcat(strcpy((*argbuf)[(*argcnt)++] = xalloc(strlen(work) + strlen(cp) + 1), work), jstrtolower(cp));
					if ((size_t) (*argcnt) >= *absiz)
						*argbuf = (char **) xrealloc(*argbuf, (*absiz += REALLOC_UNIT) * sizeof((*argbuf)[0]));
				} else if (dtabuf.d_attribute & DirAttr) {
					strcpy(wld, jstrtolower(cp));
					l = strlen(cp);
					wld[l] = (char) ap_keep;
					wld[l + 1] = '\0';
					if (expand_arg(ap + 1, wld + l + 1, argbuf, argcnt, absiz)) {
						return -1;
					}
				}
#ifdef	WIN32			/*Tom_W*/
			    } else if (r == -1) {
				FindClose(dtabuf.hFind);
			    }
			}
#else
			} else if (*cp != '.' && r == -1) {
				return -1;
			}
#endif
		} while ((cp = find_file((char *)0, 0, &dtabuf)) != (char *)0);
		*ap = (char) ap_keep;
	}
	return 0;
}
#endif

static void setargv(char * NCptr, int *_ARGC, char ***_ARGV)
{
	int      i;
	char     c;
	char     quote, withwild;	/* flags */
	char    *str;
	char    *ap;
	char    *cp;
	size_t   absiz;
	char    *buf = 0;
	size_t   bfsiz;
#define argbuf (*_ARGV)
#define argcnt (*_ARGC)

#if USE_CCL
	char	*wmk = 0;
	int      no_sort = 0;

	if ((ap = getenv("XARGS")) != NULL
		&& strstr(ap, "NO_SORT") != NULL) {
		no_sort = 1;
	}
	wmk = (char *) xalloc((bfsiz = REALLOC_UNIT * 2) * sizeof(wmk[0]));
#endif /* USE_CCL */

	buf = (char *) xalloc((bfsiz = REALLOC_UNIT * 2) * sizeof(buf[0]));
	ap = _ARGV[0][0];
	argbuf = (char **) xalloc((absiz = REALLOC_UNIT) * sizeof(argbuf[0]));

	for (str = NCptr, i = 0;
	     ((c = *str) != '\r' && c != '\0');
	     str++, i++);
	*str = 0;
	cp = str;
	str = NCptr;
	for (; *str == ' ' || *str == '\t'; str++)
		;
	for (; --cp >= str && (*cp == ' ' || *cp == '\t' || *cp == '\r');)
		;
	cp[1] = 0;
	argbuf[0] = jstrtolower(ap); /* may be my name... */
	argcnt = 1;

	while (*str) {
		withwild = quote = 0;
		while (isspace(*str))
			str++;
		if (*str == 0)
			break;
		ap = buf;
#if USE_CCL
		if (*str == '~' && getenv("HOME")
				&& (str[1] == 0 || isspace(str[1]) || str[1] == '\\' || str[1] == '/')) {
			int	len = strlen(getenv("HOME"));
			if (len >= bfsiz) {
				wmk = (char *)xrealloc(buf, (bfsiz + REALLOC_UNIT) * sizeof(wmk[0]));
				buf = (char *)xrealloc(buf, (bfsiz + REALLOC_UNIT) * sizeof(buf[0]));
				ap = buf;
				bfsiz += REALLOC_UNIT;
			}
			strcpy(wmk, getenv("HOME"));
			strcpy(buf, getenv("HOME"));
			ap += len;
			str++;
		}
		if (*str == 0)
			goto addbuf;
#endif
		do {
			if (ap == &buf[bfsiz - 1]) {
#if USE_CCL
				wmk = (char *)xrealloc(buf, (bfsiz + REALLOC_UNIT) * sizeof(wmk[0]));
#endif
				buf = (char *)xrealloc(buf, (bfsiz + REALLOC_UNIT) * sizeof(buf[0]));
				ap = &buf[bfsiz - 1];
				bfsiz += REALLOC_UNIT;
			}
			if (isk1(*str) && str[1]) {
#if USE_CCL
				wmk[ap - buf] = *str;
#endif
				*ap++ = *str++;
				if (ap == &buf[bfsiz - 1]) {
#if USE_CCL
					wmk = (char *)xrealloc(buf, (bfsiz + REALLOC_UNIT) * sizeof(wmk[0]));
#endif
					buf = (char *)xrealloc(buf, (bfsiz + REALLOC_UNIT) * sizeof(buf[0]));
					ap = &buf[bfsiz - 1];
					bfsiz += REALLOC_UNIT;
				}
#if USE_CCL
				wmk[ap - buf] = *str;
#endif
				*ap++ = *str++;
				continue;
			}
#if USE_CCL
			if (!quote && (*str == '*' || *str == '?'
						   || *str == '[' || *str == '{'))
#else
			if (!quote && (*str == '*' || *str == '?'))
#endif /* USE_CCL */
				withwild = 1;
			if (*str == quote) {	/* end of quoted string */
				quote = 0;
				str++;	/* Skip quote */
			} else if (!quote && (*str == '\'' || *str == '"')) {
				quote = *str++;
			} else {
				 if (*str == '\\' && (str[1] == '"' || str[1] == '\''))
					str++;
#if USE_CCL
				 if (!quote && *str == '\\' && str[1] == '{') {
					str++;
					wmk[ap - buf] = *str;
				 } else if (!quote && *str == '{') {
					wmk[ap - buf] = LBRACE;
				 } else {
					wmk[ap - buf] = *str;
				 }
#endif
				 *ap++ = *str++;
			}
		} while (*str && (quote || !isspace(*str)));
addbuf:
		*ap = 0;
#if USE_CCL
		wmk[ap - buf] = 0;
		i = argcnt;
		if (withwild) {
			(void) expand_arg(wmk, NULL, &argbuf, &argcnt, &absiz);
		}
		if (argcnt - i > 0) {
#if DO_SORT
			if (!no_sort) {
				qsort(&argbuf[i], argcnt - i, sizeof(argbuf[0]), ustrcmp);
			}
#endif
		} else {
			strcpy(argbuf[argcnt++] = xalloc(strlen(buf) + 1), buf);
			if ((size_t) argcnt >= absiz)
				argbuf = (char **) xrealloc(argbuf, (absiz += REALLOC_UNIT) * sizeof(argbuf[0]));
		}
	}
	free(wmk);
#else /* USE_CCL */
		if (withwild && ((cp = find_file(buf)) != (char *)0)) {
			{
				char * ap1, * ap2;

				ap1 = jrindex(buf, '\\');
				ap2 = jrindex(buf, '/');
				if ((ap = (ap1 < ap2) ? ap2:ap1) == (char *)0)
					ap = jrindex(buf, ':');
			}
			if (ap) {
				ap[1] = 0;
			} else {
				buf[0] = 0;
			}
#if DO_SORT
			i = argcnt;
#endif
			do {
				strcat(strcpy(argbuf[argcnt++] = xalloc(strlen(buf) + strlen(cp) + 1), buf), jstrtolower(cp));
				if (argcnt >= absiz)
					argbuf = (char **) xrealloc(argbuf, (absiz += REALLOC_UNIT) * sizeof(argbuf[0]));
			} while ((cp = find_file((char *)0)) != (char *)0);
#if DO_SORT
			qsort(&argbuf[i], argcnt - i, sizeof(argbuf[0]), ustrcmp);
#endif
		} else {
			argbuf[argcnt++] = ap = xalloc((int) (ap - buf) + 1);
			if (argcnt >= absiz)
				argbuf = (char **) xrealloc(argbuf, (absiz += REALLOC_UNIT) * sizeof(argbuf[0]));
			for (cp = buf; (*ap++ = *cp++) != '\0';)
				;
		}
	}
#endif /* USE_CCL */

	argbuf[argcnt] = (char *) 0;
	argbuf = (char **) xrealloc(argbuf, (argcnt + 1) * sizeof(argbuf[0]));
	free(buf);
}

#if USE_CFG
#define CFG_ALLOC_UNIT	256
#define CFG_REALLOC_UNIT	256
#endif

void
xargs(int *argc, char ***argv)
{
#ifdef	WIN32				/*Tom_W*/
	char *s;
#else
	char far *pp;
	char s[128];
#endif
#if USE_CFG
	char *cfg_fname, *cfg_line, *period, *p;
	int cfg_allocated, cfg_used;
	FILE *fp;
#endif
	int  i;

#if USE_CFG
	cfg_fname = xalloc(strlen(*argv[0]) + 4);
	strcpy(cfg_fname, *argv[0]);
	period = NULL;
	for (p = cfg_fname; *p != '\0'; p++) {
		if (isk1(*p) && isk2(p[1])) {
			p++;
		} else if (*p == '\\' || *p == '/') {
			period = NULL;
		} else if (*p == '.') {
			period = p;
		}
	}
	*period = '\0';
	strcat(cfg_fname, ".cfg");
	cfg_line = xalloc(cfg_allocated = CFG_ALLOC_UNIT);
	cfg_line[cfg_used = 0] = '\0';

	if ((fp = fopen(cfg_fname, "r")) != NULL) {
		while (fgets(cfg_line + cfg_used, cfg_allocated - cfg_used, fp) != NULL) {
			cfg_used = strlen(cfg_line);
			if (cfg_line[cfg_used - 1] == '\n') {
				cfg_line[cfg_used - 1] = ' ';
			}
			if (cfg_used + 1 >= cfg_allocated) {
				cfg_line = xrealloc(cfg_line, cfg_allocated += CFG_REALLOC_UNIT);
			}
		}
		fclose(fp);
	}
#endif

#ifdef	WIN32			/*Tom_W*/
	s = GetCommandLine();
	while (*s && *s != ' ' && *s != '\t')
		++s;				/* skip argv[0] */
	while (*s && (*s == ' ' || *s == '\t'))
		++s;
	i = strlen(s);
#else	/*WIN32*/
	pp = MK_FP(PSP, 129);
	for (i = 0; i < 127 && (s[i] = *pp++) != '\0' && s[i] != '\r'; i++)
		;
#endif	/*WIN32*/
#if USE_CFG
	s[i] = 0;
	if ((i = cfg_used + 1 + strlen(s) + 1) > cfg_allocated) {
		cfg_line = xrealloc(cfg_line, i);
	}
	strcat(cfg_line, " ");
	strcat(cfg_line, s);
	setargv(cfg_line, argc, argv);
	free(cfg_line);
#else
#ifndef	WIN32				/*Tom_W*/
	if (i == 127 && (s[126] != '\r' && s[126] != 0)) {
		s[126] = 0;
	}
#endif
	setargv(s, argc, argv);
#endif
}

#ifdef TEST
#include <stdio.h>
#include <stdlib.h>

static char QUOTE[]  = "\033[33m\"\033[m";

void
main(int argc, char *argv[])
{
	int a = 0;

	(void)setbuf(stdout, malloc(BUFSIZ));	/* for speed up */

	xargs(&argc, &argv);

	printf("[argv]-------------------------- %d argument(s)\n", argc);
	while (argc--) {
		printf("[%4d]%s%s%s\n", a++, QUOTE, *argv++, QUOTE);
	}
	printf("-------------------------------- Thats all\n");
}
#endif


#undef argbuf
#undef argcnt

char  **
glob_filename(char *p)
{
	char	**	argv;
	char	**	argbuf;
	int			argc = 1;
	char	*	s;
	char	*	arg[3];
	int			i;

	s = xalloc(strlen(p) + 1);
	arg[0] = "xargs";
	arg[1] = p;
	arg[2] = NULL;
	argv = arg;
	strcpy(s, p);
	s[strlen(p)] = 0;
	setargv(s, &argc, &argv);
	argbuf = (char **) xalloc(argc * sizeof(char *));
	for (i = 1; i < argc; i++) {
		argbuf[i - 1] = argv[i];
	}
	argbuf[argc - 1] = NULL;
	free(argv);
	free(s);
	return argbuf;
}
