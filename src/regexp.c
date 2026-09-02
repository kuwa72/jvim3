/* vi:ts=4:sw=4
 * NOTICE NOTICE NOTICE NOTICE NOTICE NOTICE NOTICE NOTICE NOTICE NOTICE
 *
 * This is NOT the original regular expression code as written by
 * Henry Spencer. This code has been modified specifically for use
 * with the VIM editor, and should not be used apart from compiling
 * VIM. If you want a good regular expression library, get the
 * original code. The copyright notice that follows is from the
 * original.
 *
 * NOTICE NOTICE NOTICE NOTICE NOTICE NOTICE NOTICE NOTICE NOTICE NOTICE
 *
 *
 * regcomp and regexec -- regsub and regerror are elsewhere
 *
 *		Copyright (c) 1986 by University of Toronto.
 *		Written by Henry Spencer.  Not derived from licensed software.
 *
 *		Permission is granted to anyone to use this software for any
 *		purpose on any computer system, and to redistribute it freely,
 *		subject to the following restrictions:
 *
 *		1. The author is not responsible for the consequences of use of
 *				this software, no matter how awful, even if they arise
 *				from defects in it.
 *
 *		2. The origin of this software must not be misrepresented, either
 *				by explicit claim or by omission.
 *
 *		3. Altered versions must be plainly marked as such, and must not
 *				be misrepresented as being the original software.
 *
 * Beware that some of this code is subtly aware of the way operator
 * precedence is structured in regular expressions.  Serious changes in
 * regular-expression syntax might require a total rethink.
 *
 * $Log:		regexp.c,v $
 * Revision 1.2  88/04/28  08:09:45  tony
 * First modification of the regexp library. Added an external variable
 * 'reg_ic' which can be set to indicate that case should be ignored.
 * Added a new parameter to regexec() to indicate that the given string
 * comes from the beginning of a line and is thus eligible to match
 * 'beginning-of-line'.
 *
 * Revisions by Olaf 'Rhialto' Seibert, rhialto@cs.kun.nl:
 * Changes for vi: (the semantics of several things were rather different)
 * - Added lexical analyzer, because in vi magicness of characters
 *   is rather difficult, and may change over time.
 * - Added support for \< \> \1-\9 and ~
 * - Left some magic stuff in, but only backslashed: \| \+
 * - * and \+ still work after \) even though they shouldn't.
 */
#include "vim.h"
#include "globals.h"
#include "proto.h"
#undef DEBUG

#include <stdio.h>
#include "regexp.h"
#include "regmagic.h"
#include "param.h"
#ifdef KANJI
#include "kanji.h"
#endif

/*
 * The "internal use only" fields in regexp.h are present to pass info from
 * compile to execute that permits the execute phase to run lots faster on
 * simple cases.  They are:
 *
 * regstart 	char that must begin a match; '\0' if none obvious
 * reganch		is the match anchored (at beginning-of-line only)?
 * regmust		string (pointer into program) that match must include, or NULL
 * regmlen		length of regmust string
 *
 * Regstart and reganch permit very fast decisions on suitable starting points
 * for a match, cutting down the work a lot.  Regmust permits fast rejection
 * of lines that cannot possibly match.  The regmust tests are costly enough
 * that regcomp() supplies a regmust only if the r.e. contains something
 * potentially expensive (at present, the only such thing detected is * or +
 * at the start of the r.e., which can involve a lot of backup).  Regmlen is
 * supplied because the test in regexec() needs it and regcomp() is computing
 * it anyway.
 */

/*
 * Structure for regexp "program".	This is essentially a linear encoding
 * of a nondeterministic finite-state machine (aka syntax charts or
 * "railroad normal form" in parsing technology).  Each node is an opcode
 * plus a "next" pointer, possibly plus an operand.  "Next" pointers of
 * all nodes except BRANCH implement concatenation; a "next" pointer with
 * a BRANCH on both ends of it is connecting two alternatives.	(Here we
 * have one of the subtle syntax dependencies:	an individual BRANCH (as
 * opposed to a collection of them) is never concatenated with anything
 * because of operator precedence.)  The operand of some types of node is
 * a literal string; for others, it is a node leading into a sub-FSM.  In
 * particular, the operand of a BRANCH node is the first node of the branch.
 * (NB this is *not* a tree structure:	the tail of the branch connects
 * to the thing following the set of BRANCHes.)  The opcodes are:
 */

/* definition	number			   opnd?	meaning */
#define END 	0				/* no	End of program. */
#define BOL 	1				/* no	Match "" at beginning of line. */
#define EOL 	2				/* no	Match "" at end of line. */
#define ANY 	3				/* no	Match any one character. */
#define ANYOF	4				/* str	Match any character in this string. */
#define ANYBUT	5				/* str	Match any character not in this
								 *		string. */
#define BRANCH	6				/* node Match this alternative, or the
								 *		next... */
#define BACK	7				/* no	Match "", "next" ptr points backward. */
#define EXACTLY 8				/* str	Match this string. */
#define NOTHING 9				/* no	Match empty string. */
#define STAR	10				/* node Match this (simple) thing 0 or more
								 *		times. */
#define PLUS	11				/* node Match this (simple) thing 1 or more
								 *		times. */
#define BOW		12				/* no	Match "" after [^a-zA-Z0-9_] */
#define EOW		13				/* no	Match "" at    [^a-zA-Z0-9_] */
#define MOPEN	20				/* no	Mark this point in input as start of
								 *		#n. */
 /* MOPEN+1 is number 1, etc. */
#define MCLOSE	30				/* no	Analogous to MOPEN. */
#define BACKREF	40				/* node Match same string again \1-\9 */

#define Magic(x)	((x)|('\\'<<8))

/*
 * Opcode notes:
 *
 * BRANCH		The set of branches constituting a single choice are hooked
 *				together with their "next" pointers, since precedence prevents
 *				anything being concatenated to any individual branch.  The
 *				"next" pointer of the last BRANCH in a choice points to the
 *				thing following the whole choice.  This is also where the
 *				final "next" pointer of each individual branch points; each
 *				branch starts with the operand node of a BRANCH node.
 *
 * BACK 		Normal "next" pointers all implicitly point forward; BACK
 *				exists to make loop structures possible.
 *
 * STAR,PLUS	'=', and complex '*' and '+', are implemented as circular
 *				BRANCH structures using BACK.  Simple cases (one character
 *				per match) are implemented with STAR and PLUS for speed
 *				and to minimize recursive plunges.
 *
 * MOPEN,MCLOSE	...are numbered at compile time.
 */

/*
 * A node is one char of opcode followed by two chars of "next" pointer.
 * "Next" pointers are stored as two 8-bit pieces, high order first.  The
 * value is a positive offset from the opcode of the node containing it.
 * An operand, if any, simply follows the node.  (Note that much of the
 * code generation knows about this implicit relationship.)
 *
 * Using two bytes for the "next" pointer is vast overkill for most things,
 * but allows patterns to get big without disasters.
 */
#define OP(p)	(*(p))
#define NEXT(p) (((*((p)+1)&0377)<<8) + (*((p)+2)&0377))
#define OPERAND(p)		((p) + 3)

/*
 * See regmagic.h for one further detail of program structure.
 */


/*
 * Utility definitions.
 */
#ifndef CHARBITS
#define UCHARAT(p)		((int)*(unsigned char *)(p))
#else
#define UCHARAT(p)		((int)*(p)&CHARBITS)
#endif

#define EMSG_RETURN(m) { emsg(m); return NULL; }

static int ismult __ARGS((int));
#ifdef KANJI
static void regjp __ARGS((int, char_u));
static char * strjpchr __ARGS((char_u *, char_u, char_u));
static char * mstrjpchr __ARGS((char_u *, char_u *));
#endif
#ifndef notdef
static char_u	* regstrext __ARGS((char_u *));
#endif

	static int
ismult(int c)
{
	return (c == Magic('*') || c == Magic('+') || c == Magic('='));
}

/*
 * Flags to be passed up and down.
 */
#define HASWIDTH		01		/* Known never to match null string. */
#define SIMPLE			02		/* Simple enough to be STAR/PLUS operand. */
#define SPSTART 		04		/* Starts with * or +. */
#define WORST			0		/* Worst case. */

/*
 * The following allows empty REs, meaning "the same as the previous RE".
 * per the ed(1) manual.
 */
/* #define EMPTY_RE */			/* this is done outside of regexp */
#ifdef EMPTY_RE
char_u		   *reg_prev_re;
#endif

#define TILDE
#ifdef TILDE
char_u		   *reg_prev_sub;
#endif

/*
 * This if for vi's "magic" mode. If magic is false, only ^$\ are magic.
 */

int				reg_magic = 1;

/*
 * Global work variables for regcomp().
 */

static char_u    *regparse;	/* Input-scan pointer. */
static int		regnpar;		/* () count. */
static char_u 	regdummy;
static char_u   *regcode;		/* Code-emit pointer; &regdummy = don't. */
static long 	regsize;		/* Code size. */
static char_u   **regendp;		/* Ditto for endp. */

/*
 * META contains all characters that may be magic, except '^' and '$'.
 * This depends on the configuration options TILDE, BACKREF.
 * (could be done simpler for compilers that know string concatenation)
 */

#ifdef TILDE
# ifdef BACKREF
	   static char_u META[] = ".[()|=+*<>~123456789";
# else
	   static char_u META[] = ".[()|=+*<>~";
# endif
#else
# ifdef BACKREF
	   static char_u META[] = ".[()|=+*<>123456789";
# else
	   static char_u META[] = ".[()|=+*<>";
# endif
#endif

/*
 * Forward declarations for regcomp()'s friends.
 */
static void		initchr __ARGS((char_u *));
static int		getchr __ARGS((void));
static int		peekchr __ARGS((void));
#define PeekChr() curchr	/* shortcut only when last action was peekchr() */
static int 		curchr;
static void		skipchr __ARGS((void));
static void		ungetchr __ARGS((void));
static char_u    *reg __ARGS((int, int *));
static char_u    *regbranch __ARGS((int *));
static char_u    *regpiece __ARGS((int *));
static char_u    *regatom __ARGS((int *));
static char_u    *regnode __ARGS((int));
static char_u    *regnext __ARGS((char_u *));
static void 	regc __ARGS((int));
static void 	unregc __ARGS((void));
static void 	reginsert __ARGS((int, char_u *));
static void 	regtail __ARGS((char_u *, char_u *));
static void 	regoptail __ARGS((char_u *, char_u *));

#undef STRCSPN
#ifdef STRCSPN
static int		strcspn __ARGS((const char_u *, const char_u *));
#endif

/*
 * skip past regular expression
 * stop at end of 'p' of where 'dirc' is found ('/', '?', etc)
 * take care of characters with a backslash in front of it
 * skip strings inside [ and ].
 */
	char_u *
skip_regexp(char_u *p, int dirc)
{
	int		in_range = FALSE;

	for (; p[0] != NUL; ++p)
	{
		if (p[0] == dirc && !in_range)		/* found end of regexp */
			break;
#ifdef KANJI
		if (ISkanji(p[0]))
		{
			++p;
			continue;
		}
#endif
		if ((p[0] == '[' && p_magic) || (p[0] == '\\' && p[1] == '[' && !p_magic))
		{
			in_range = TRUE;
			if (p[0] == '\\')
				++p;
								/* "[^]" and "[]" are not the end of a range */
			if (p[1] == '^')
				++p;
			if (p[1] == ']')
				++p;
		}
		else if (p[0] == '\\' && p[1] != NUL)
			++p;	/* skip next character */
		else if (p[0] == ']')
			in_range = FALSE;
	}
	return p;
}

/*
 - regcomp - compile a regular expression into internal code
 *
 * We can't allocate space until we know how big the compiled form will be,
 * but we can't compile it (and thus know how big it is) until we've got a
 * place to put the code.  So we cheat:  we compile it twice, once with code
 * generation turned off and size counting turned on, and once "for real".
 * This also means that we don't allocate space until we are sure that the
 * thing really will compile successfully, and we never have to move the
 * code and thus invalidate pointers into it.  (Note that it has to be in
 * one piece because free() must be able to free it all.)
 *
 * Beware that the optimization-preparation code in here knows about some
 * of the structure of the compiled regexp.
 */
	regexp		   *
regcomp(char_u *exp)
{
	regexp *r;
	char_u  *scan;
	char_u  *longest;
	int	len;
	int 			flags;
/*	extern char    *malloc();*/

#ifndef notdef
	exp = regstrext(exp);
#endif
	if (exp == NULL) {
		EMSG_RETURN(e_null);
	}

#ifdef EMPTY_RE			/* this is done outside of regexp */
	if (*exp == '\0') {
		if (reg_prev_re) {
			exp = reg_prev_re;
		} else {
			EMSG_RETURN(e_noprevre);
		}
	}
#endif

	/* First pass: determine size, legality. */
	initchr((char_u *)exp);
	regnpar = 1;
	regsize = 0L;
	regcode = &regdummy;
	regendp = NULL;
	regc(MAGIC);
	if (reg(0, &flags) == NULL)
		return NULL;

	/* Small enough for pointer-storage convention? */
	if (regsize >= 32767L)		/* Probably could be 65535L. */
		EMSG_RETURN(e_toolong);

	/* Allocate space. */
/*	r = (regexp *) malloc((unsigned) (sizeof(regexp) + regsize));*/
	r = (regexp *) alloc((unsigned) (sizeof(regexp) + regsize));
	if (r == NULL)
		EMSG_RETURN(e_outofmem);

#ifdef EMPTY_RE			/* this is done outside of regexp */
	if (exp != reg_prev_re) {
		free(reg_prev_re);
		if (reg_prev_re = alloc(STRLEN(exp) + 1))
			STRCPY(reg_prev_re, exp);
	}
#endif

	/* Second pass: emit code. */
	initchr((char_u *)exp);
	regnpar = 1;
	regcode = r->program;
	regendp = r->endp;
	regc(MAGIC);
	if (reg(0, &flags) == NULL) {
		free(r);
		return NULL;
	}

	/* Dig out information for optimizations. */
	r->regstart = '\0'; 		/* Worst-case defaults. */
	r->reganch = 0;
	r->regmust = NULL;
	r->regmlen = 0;
	scan = r->program + 1;		/* First BRANCH. */
	if (OP(regnext(scan)) == END) { 	/* Only one top-level choice. */
		scan = OPERAND(scan);

		/* Starting-point info. */
		if (OP(scan) == EXACTLY)
			r->regstart = *OPERAND(scan);
		else if (OP(scan) == BOL)
			r->reganch++;

		/*
		 * If there's something expensive in the r.e., find the longest
		 * literal string that must appear and make it the regmust.  Resolve
		 * ties in favor of later strings, since the regstart check works
		 * with the beginning of the r.e. and avoiding duplication
		 * strengthens checking.  Not a strong reason, but sufficient in the
		 * absence of others.
		 */
		/*
		 * When the r.e. starts with BOW, it is faster to look for a regmust
		 * first. Used a lot for "#" and "*" commands. (Added by mool).
		 */
		if (flags & SPSTART || OP(scan) == BOW) {
			longest = NULL;
			len = 0;
			for (; scan != NULL; scan = regnext(scan))
				if (OP(scan) == EXACTLY && STRLEN(OPERAND(scan)) >= (size_t)len) {
					longest = OPERAND(scan);
					len = STRLEN(OPERAND(scan));
				}
			r->regmust = longest;
			r->regmlen = len;
		}
	}
#ifdef DEBUG
	regdump(r);
#endif
	return r;
}

/*
 - reg - regular expression, i.e. main body or parenthesized thing
 *
 * Caller must absorb opening parenthesis.
 *
 * Combining parenthesis handling with the base level of regular expression
 * is a trifle forced, but the need to tie the tails of the branches to what
 * follows makes it hard to avoid.
 */
	static char_u *
reg(int paren, int *flagp)
{
	char_u  *ret;
	char_u  *br;
	char_u  *ender;
	int	parno = 0;
	int 			flags;

	*flagp = HASWIDTH;			/* Tentatively. */

	/* Make an MOPEN node, if parenthesized. */
	if (paren) {
		if (regnpar >= NSUBEXP)
			EMSG_RETURN(e_toombra);
		parno = regnpar;
		regnpar++;
		ret = regnode(MOPEN + parno);
		if (regendp)
			regendp[parno] = NULL;	/* haven't seen the close paren yet */
	} else
		ret = NULL;

	/* Pick up the branches, linking them together. */
	br = regbranch(&flags);
	if (br == NULL)
		return NULL;
	if (ret != NULL)
		regtail(ret, br);		/* MOPEN -> first. */
	else
		ret = br;
	if (!(flags & HASWIDTH))
		*flagp &= ~HASWIDTH;
	*flagp |= flags & SPSTART;
	while (peekchr() == Magic('|')) {
		skipchr();
		br = regbranch(&flags);
		if (br == NULL)
			return NULL;
		regtail(ret, br);		/* BRANCH -> BRANCH. */
		if (!(flags & HASWIDTH))
			*flagp &= ~HASWIDTH;
		*flagp |= flags & SPSTART;
	}

	/* Make a closing node, and hook it on the end. */
	ender = regnode((paren) ? MCLOSE + parno : END);
	regtail(ret, ender);

	/* Hook the tails of the branches to the closing node. */
	for (br = ret; br != NULL; br = regnext(br))
		regoptail(br, ender);

	/* Check for proper termination. */
	if (paren && getchr() != Magic(')')) {
		EMSG_RETURN(e_toombra);
	} else if (!paren && peekchr() != '\0') {
		if (PeekChr() == Magic(')')) {
			EMSG_RETURN(e_toomket);
		} else
			EMSG_RETURN(e_trailing);/* "Can't happen". */
		/* NOTREACHED */
	}
	/*
	 * Here we set the flag allowing back references to this set of
	 * parentheses.
	 */
	if (paren && regendp)
		regendp[parno] = ender;	/* have seen the close paren */
	return ret;
}

/*
 - regbranch - one alternative of an | operator
 *
 * Implements the concatenation operator.
 */
	static char_u    *
regbranch(int *flagp)
{
	char_u  *ret;
	char_u  *chain;
	char_u  *latest;
	int 			flags;

	*flagp = WORST; 			/* Tentatively. */

	ret = regnode(BRANCH);
	chain = NULL;
	while (peekchr() != '\0' && PeekChr() != Magic('|') && PeekChr() != Magic(')')) {
		latest = regpiece(&flags);
		if (latest == NULL)
			return NULL;
		*flagp |= flags & HASWIDTH;
		if (chain == NULL)		/* First piece. */
			*flagp |= flags & SPSTART;
		else
			regtail(chain, latest);
		chain = latest;
	}
	if (chain == NULL)			/* Loop ran zero times. */
		(void) regnode(NOTHING);

	return ret;
}

/*
 - regpiece - something followed by possible [*+=]
 *
 * Note that the branching code sequences used for = and the general cases
 * of * and + are somewhat optimized:  they use the same NOTHING node as
 * both the endmarker for their branch list and the body of the last branch.
 * It might seem that this node could be dispensed with entirely, but the
 * endmarker role is not redundant.
 */
static char_u    *
regpiece(int *flagp)
{
	char_u  *ret;
	int	op;
	char_u  *next;
	int 			flags;

	ret = regatom(&flags);
	if (ret == NULL)
		return NULL;

	op = peekchr();
	if (!ismult(op)) {
		*flagp = flags;
		return ret;
	}
	if (!(flags & HASWIDTH) && op != Magic('='))
		EMSG_RETURN((char_u *)"*+ operand could be empty");
	*flagp = (op != Magic('+')) ? (WORST | SPSTART) : (WORST | HASWIDTH);

	if (op == Magic('*') && (flags & SIMPLE))
		reginsert(STAR, ret);
	else if (op == Magic('*')) {
		/* Emit x* as (x&|), where & means "self". */
		reginsert(BRANCH, ret); /* Either x */
		regoptail(ret, regnode(BACK));	/* and loop */
		regoptail(ret, ret);	/* back */
		regtail(ret, regnode(BRANCH));	/* or */
		regtail(ret, regnode(NOTHING)); /* null. */
	} else if (op == Magic('+') && (flags & SIMPLE))
		reginsert(PLUS, ret);
	else if (op == Magic('+')) {
		/* Emit x+ as x(&|), where & means "self". */
		next = regnode(BRANCH); /* Either */
		regtail(ret, next);
		regtail(regnode(BACK), ret);	/* loop back */
		regtail(next, regnode(BRANCH)); /* or */
		regtail(ret, regnode(NOTHING)); /* null. */
	} else if (op == Magic('=')) {
		/* Emit x= as (x|) */
		reginsert(BRANCH, ret); /* Either x */
		regtail(ret, regnode(BRANCH));	/* or */
		next = regnode(NOTHING);/* null. */
		regtail(ret, next);
		regoptail(ret, next);
	}
	skipchr();
	if (ismult(peekchr()))
		EMSG_RETURN((char_u *)"Nested *=+");

	return ret;
}

/*
 - regatom - the lowest level
 *
 * Optimization:  gobbles an entire sequence of ordinary characters so that
 * it can turn them into a single node, which is smaller to store and
 * faster to run.
 */
static char_u    *
regatom(int *flagp)
{
	char_u  *ret;
	int 			flags;

	*flagp = WORST; 			/* Tentatively. */

	switch (getchr()) {
	  case Magic('^'):
		ret = regnode(BOL);
		break;
	  case Magic('$'):
		ret = regnode(EOL);
		break;
	  case Magic('<'):
		ret = regnode(BOW);
		break;
	  case Magic('>'):
		ret = regnode(EOW);
		break;
	  case Magic('.'):
		ret = regnode(ANY);
		*flagp |= HASWIDTH | SIMPLE;
		break;
	  case Magic('['):{
			/*
			 * In a character class, different parsing rules apply.
			 * Not even \ is special anymore, nothing is.
			 */
			if (*regparse == '^') { 	/* Complement of range. */
				ret = regnode(ANYBUT);
				regparse++;
			} else
				ret = regnode(ANYOF);
#ifdef KANJI
# define S_CHAR		's'
# define R_CHAR		'r'
			if (*regparse == ']' || *regparse == '-')
			{
				regc(S_CHAR);
				regc(*regparse++);
			}
			/*
			 * Members are stored as S_CHAR followed by one character, or
			 * R_CHAR followed by the two ends of a range. A character is one to
			 * four UTF-8 bytes, so it is copied by its length rather than as
			 * "one byte or two", and a range compares as code points, which
			 * also means the two ends no longer have to be the same width.
			 */
			while (*regparse != '\0' && *regparse != ']')
			{
				int		clen;

				if (*regparse == '\\' && regparse[1])
				{
					regparse++;
					regc(S_CHAR);
					for (clen = utf_lenat(regparse, 0); clen > 0; clen--)
						regc(*regparse++);
				}
				else if (regparse[utf_lenat(regparse, 0)] == '-'
						&& regparse[utf_lenat(regparse, 0) + 1] != ']'
						&& regparse[utf_lenat(regparse, 0) + 1] != '\0')
				{
					int			cclass;
					int			cclassend;

					regc(R_CHAR);
					cclass = utf_decode(regparse, NULL);
					if (cclass == UTF8_ERROR)
						cclass = *regparse;
					for (clen = utf_lenat(regparse, 0); clen > 0; clen--)
						regc(*regparse++);
					regparse++;					/* the '-' */
					cclassend = utf_decode(regparse, NULL);
					if (cclassend == UTF8_ERROR)
						cclassend = *regparse;
					for (clen = utf_lenat(regparse, 0); clen > 0; clen--)
						regc(*regparse++);
					if (cclass > cclassend + 1)
						EMSG_RETURN(e_invrange);
				}
				else
				{
					regc(S_CHAR);
					for (clen = utf_lenat(regparse, 0); clen > 0; clen--)
						regc(*regparse++);
				}
			}
#else
			if (*regparse == ']' || *regparse == '-')
				regc(*regparse++);
			while (*regparse != '\0' && *regparse != ']') {
				if (*regparse == '-') {
					regparse++;
					if (*regparse == ']' || *regparse == '\0')
						regc('-');
					else {
						int	class;
						int	classend;

						class = UCHARAT(regparse - 2) + 1;
						classend = UCHARAT(regparse);
						if (class > classend + 1)
							EMSG_RETURN(e_invrange);
						for (; class <= classend; class++)
							regc(class);
						regparse++;
					}
				} else if (*regparse == '\\' && regparse[1]) {
					regparse++;
					regc(*regparse++);
				} else
					regc(*regparse++);
			}
#endif
			regc('\0');
			if (*regparse != ']')
				EMSG_RETURN(e_toomsbra);
			skipchr();			/* let's be friends with the lexer again */
			*flagp |= HASWIDTH | SIMPLE;
		}
		break;
	  case Magic('('):
		ret = reg(1, &flags);
		if (ret == NULL)
			return NULL;
		*flagp |= flags & (HASWIDTH | SPSTART);
		break;
	  case '\0':
	  case Magic('|'):
	  case Magic(')'):
		EMSG_RETURN(e_internal);	/* Supposed to be caught earlier. */
		/* break; Not Reached */
	  case Magic('='):
	  case Magic('+'):
	  case Magic('*'):
		EMSG_RETURN((char_u *)"=+* follows nothing");
		/* break; Not Reached */
#ifdef TILDE
	  case Magic('~'):			/* previous substitute pattern */
			if (reg_prev_sub) {
				char_u *p;

				ret = regnode(EXACTLY);
				p = reg_prev_sub;
				while (*p) {
					regc(*p++);
				}
				regc('\0');
				if (p - reg_prev_sub) {
					*flagp |= HASWIDTH;
					if ((p - reg_prev_sub) == 1)
						*flagp |= SIMPLE;
				}
			} else
				EMSG_RETURN(e_nopresub);
			break;
#endif
#ifdef BACKREF
	  case Magic('1'):
	  case Magic('2'):
	  case Magic('3'):
	  case Magic('4'):
	  case Magic('5'):
	  case Magic('6'):
	  case Magic('7'):
	  case Magic('8'):
	  case Magic('9'): {
			int				refnum;

			ungetchr();
			refnum = getchr() - Magic('0');
			/*
			 * Check if the back reference is legal. We use the parentheses
			 * pointers to mark encountered close parentheses, but this
			 * is only available in the second pass. Checking opens is
			 * always possible.
			 * Should also check that we don't refer to something that
			 * is repeated (+*=): what instance of the repetition should
			 * we match? TODO.
			 */
			if (refnum < regnpar &&
				(regendp == NULL || regendp[refnum] != NULL))
				ret = regnode(BACKREF + refnum);
			else
				EMSG_RETURN((char_u *)"Illegal back reference");
		}
		break;
#endif
	  default:{
			int	len;
			int				chr;

			ungetchr();
			len = 0;
			ret = regnode(EXACTLY);
			while ((chr = peekchr()) != '\0' && (chr < Magic(0)))
			{
#ifdef KANJI
				if (ISkanji(chr))
					regjp(chr, regparse[1]);
				else
#endif
				regc(chr);
				skipchr();
				len++;
			}
#ifdef DEBUG
			if (len == 0)
				 EMSG_RETURN((char_u *)"Unexpected magic character; check META.");
#endif
			/*
			 * If there is a following *, \+ or \= we need the character
			 * in front of it as a single character operand
			 */
			if (len > 1 && ismult(chr))
			{
#ifdef KANJI
				if (ISkanji(chr))
					regjp(chr, regparse[1]);
				else
#endif
				unregc();			/* Back off of *+= operand */
				ungetchr();			/* and put it back for next time */
				--len;
			}
			regc('\0');
			*flagp |= HASWIDTH;
			if (len == 1)
				*flagp |= SIMPLE;
		}
		break;
	}

	return ret;
}

/*
 - regnode - emit a node
 */
static char_u    *				/* Location. */
regnode(int op)
{
	char_u  *ret;
	char_u  *ptr;

	ret = regcode;
	if (ret == &regdummy) {
		regsize += 3;
		return ret;
	}
	ptr = ret;
	*ptr++ = op;
	*ptr++ = '\0';				/* Null "next" pointer. */
	*ptr++ = '\0';
	regcode = ptr;

	return ret;
}

/*
 - regc - emit (if appropriate) a byte of code
 */
static void
regc(int b)
{
	if (regcode != &regdummy)
		*regcode++ = b;
	else
		regsize++;
}

#ifdef KANJI
/*
 - regjp - emit (if appropriate) a word of code
 */
static void
regjp(int b, char_u k)
{
	if (regcode != &regdummy)
	{
		*(char_u *)regcode++ = b;
		*(char_u *)regcode++ = k;
	}
	else
		regsize += 2;
}
#endif

/*
 - unregc - take back (if appropriate) a byte of code
 */
static void
unregc(void)
{
	if (regcode != &regdummy)
#ifdef KANJI
	{
		if (ISkanji(* --regcode))
			regcode--;
	}
#else
		regcode--;
#endif
	else
		regsize--;
}

/*
 - reginsert - insert an operator in front of already-emitted operand
 *
 * Means relocating the operand.
 */
static void
reginsert(int op, char_u *opnd)
{
	char_u  *src;
	char_u  *dst;
	char_u  *place;

	if (regcode == &regdummy) {
		regsize += 3;
		return;
	}
	src = regcode;
	regcode += 3;
	dst = regcode;
	while (src > opnd)
		*--dst = *--src;

	place = opnd;				/* Op node, where operand used to be. */
	*place++ = op;
	*place++ = '\0';
	*place = '\0';
}

/*
 - regtail - set the next-pointer at the end of a node chain
 */
static void
regtail(char_u *p, char_u *val)
{
	char_u  *scan;
	char_u  *temp;
	int	offset;

	if (p == &regdummy)
		return;

	/* Find last node. */
	scan = p;
	for (;;) {
		temp = regnext(scan);
		if (temp == NULL)
			break;
		scan = temp;
	}

	if (OP(scan) == BACK)
		offset = (int)(scan - val);
	else
		offset = (int)(val - scan);
	*(scan + 1) = (char_u) ((offset >> 8) & 0377);
	*(scan + 2) = (char_u) (offset & 0377);
}

/*
 - regoptail - regtail on operand of first argument; nop if operandless
 */
static void
regoptail(char_u *p, char_u *val)
{
	/* "Operandless" and "op != BRANCH" are synonymous in practice. */
	if (p == NULL || p == &regdummy || OP(p) != BRANCH)
		return;
	regtail(OPERAND(p), val);
}

/*
 - getchr() - get the next character from the pattern. We know about
 * magic and such, so therefore we need a lexical analyzer.
 */

/* static int		curchr; */
static int		prevchr;
static int		nextchr;	/* used for ungetchr() */

static void
initchr(char_u *str)
{
	regparse = str;
	curchr = prevchr = nextchr = -1;
}

static int
peekchr(void)
{
	if (curchr < 0) {
		switch (curchr = regparse[0]) {
		case '.':
		case '*':
	/*	case '+':*/
	/*	case '=':*/
		case '[':
		case '~':
			if (reg_magic)
				curchr = Magic(curchr);
			break;
		case '^':
			/* ^ is only magic as the very first character */
			if (prevchr < 0)
				curchr = Magic('^');
			break;
		case '$':
			/* $ is only magic as the very last character and in front of '\|' */
			if (regparse[1] == NUL || (regparse[1] == '\\' && regparse[2] == '|'))
				curchr = Magic('$');
			break;
		case '\\':
			regparse++;
			if (regparse[0] == NUL)
				curchr = '\\';	/* trailing '\' */
			else if (STRCHR(META, regparse[0]))
			{
				/*
				 * META contains everything that may be magic sometimes, except
				 * ^ and $ ("\^" and "\$" are never magic).
				 * We now fetch the next character and toggle its magicness.
				 * Therefore, \ is so meta-magic that it is not in META.
				 */
				curchr = -1;
				peekchr();
				curchr ^= Magic(0);
			}
			else
			{
				/*
				 * Next character can never be (made) magic?
				 * Then backslashing it won't do anything.
				 */
				curchr = regparse[0];
			}
			break;
		}
	}

	return curchr;
}

static void
skipchr(void)
{
#ifdef KANJI
	if (ISkanji(*regparse))
		regparse += 2;
	else
#endif
	regparse++;
	prevchr = curchr;
	curchr = nextchr;		/* use previously unget char, or -1 */
	nextchr = -1;
}

static int
getchr(void)
{
	int chr;

	chr = peekchr();
	skipchr();

	return chr;
}

/*
 * put character back. Works only once!
 */
static void
ungetchr(void)
{
	nextchr = curchr;
	curchr = prevchr;
	/*
	 * Backup regparse as well; not because we will use what it points at,
	 * but because skipchr() will bump it again.
	 */
#ifdef KANJI
	if (ISkanji(curchr))
	{
		nextchr = -1;
		regparse-= 2;
	}
	else
#endif
	regparse--;
}

/*
 * regexec and friends
 */

/*
 * Global work variables for regexec().
 */
static char_u    *reginput;		/* String-input pointer. */
static char_u    *regbol; 		/* Beginning of input, for ^ check. */
static char_u   **regstartp;		/* Pointer to startp array. */
/* static char_u   **regendp;	*/	/* Ditto for endp. */
#ifdef KANJI
static char_u    *regbegin; 		/* Beginning of start. */
#endif

/*
 * Forwards.
 */
static int		regtry __ARGS((regexp *, char_u *));
static int		regmatch __ARGS((char_u *));
static int		regrepeat __ARGS((char_u *));

#ifdef DEBUG
int 			regnarrate = 1;
void			regdump __ARGS((regexp *));
static char_u    *regprop __ARGS((char_u *));
#endif

/*
 - regexec - match a regexp against a string
 */
int
regexec(regexp *prog, char_u *string, int at_bol)
{
	char_u  *s;

	/* Be paranoid... */
	if (prog == NULL || string == NULL) {
		emsg(e_null);
		return 0;
	}
	/* Check validity of program. */
	if (UCHARAT(prog->program) != MAGIC) {
		emsg(e_re_corr);
		return 0;
	}
	/* If there is a "must appear" string, look for it. */
	if (prog->regmust != NULL) {
		s = string;
#ifdef KANJI
		while ((s = strjpchr(s, prog->regmust[0], prog->regmust[1])) != NULL)
#else
		while ((s = cstrchr(s, prog->regmust[0])) != NULL)
#endif
		{
			if (cstrncmp(s, prog->regmust, prog->regmlen) == 0)
				break;			/* Found it. */
#ifdef KANJI
			if (ISkanji(*s))
				s += 2;
			else
#endif
			s++;
		}
		if (s == NULL)			/* Not present. */
			return 0;
	}
	/* Mark beginning of line for ^ . */
	if (at_bol)
		regbol = string;		/* is possible to match bol */
	else
		regbol = NULL;			/* we aren't there, so don't match it */
#ifdef KANJI
	regbegin = string;	 		/* Beginning of start. */
#endif

	/* Simplest case:  anchored match need be tried only once. */
	if (prog->reganch)
		return regtry(prog, string);

	/* Messy cases:  unanchored match. */
	s = string;
	if (prog->regstart != '\0')
		/* We know what char it must start with. */
#ifdef KANJI
		while ((s = strjpchr(s, prog->regstart, NUL)) != NULL)
#else
		while ((s = cstrchr(s, prog->regstart)) != NULL)
#endif
		{
			if (regtry(prog, s))
				return 1;
#ifdef KANJI
			if (ISkanji(*s))
				s += 2;
			else
#endif
			s++;
		}
	else
		/* We don't -- general case. */
		do {
			if (regtry(prog, s))
				return 1;
#ifdef KANJI
			if (ISkanji(*s))		/* happen ?? */
				s++;
#endif
		} while (*s++ != '\0');

	/* Failure. */
	return 0;
}

/*
 - regtry - try match at specific point
 */
static int						/* 0 failure, 1 success */
regtry(regexp *prog, char_u *string)
{
	int	i;
	char_u **sp;
	char_u **ep;

	reginput = string;
	regstartp = prog->startp;
	regendp = prog->endp;

	sp = prog->startp;
	ep = prog->endp;
	for (i = NSUBEXP; i > 0; i--) {
		*sp++ = NULL;
		*ep++ = NULL;
	}
	if (regmatch(prog->program + 1)) {
		prog->startp[0] = string;
		prog->endp[0] = reginput;
		return 1;
	} else
		return 0;
}

/*
 - regmatch - main matching routine
 *
 * Conceptually the strategy is simple:  check to see whether the current
 * node matches, call self recursively to see whether the rest matches,
 * and then act accordingly.  In practice we make some effort to avoid
 * recursion, in particular by going through "ordinary" nodes (that don't
 * need to know whether the rest of the match failed) by a loop instead of
 * by recursion.
 */
static int						/* 0 failure, 1 success */
regmatch(char_u *prog)
{
	char_u  *scan;		/* Current node. */
	char_u		   *next;		/* Next node. */

	scan = prog;
#ifdef DEBUG
	if (scan != NULL && regnarrate)
		fprintf(stderr, "%s(\n", regprop(scan));
#endif
	while (scan != NULL) {
#ifdef DEBUG
		if (regnarrate) {
			fprintf(stderr, "%s...\n", regprop(scan));
		}
#endif
		next = regnext(scan);
		switch (OP(scan)) {
		  case BOL:
			if (reginput != regbol)
				return 0;
			break;
		  case EOL:
			if (*reginput != '\0')
				return 0;
			break;
		  case BOW:		/* \<word; reginput points to w */
#define isidchar(x)	(isalnum(x) || ((x) == '_'))
#ifdef KANJI
			if (!reginput[0])
				return 0;
			if (ISkanji(reginput[0]))
			{
				int sclass;		/* sclass == JPC_KIGOU : Symbol */
				if (!(sclass = jpcls(reginput)))
					return 0;
				if (((reginput - regbegin) >= 1)
								&& ISkanjiPointer(regbegin, &reginput[-1]) == 2
								&& sclass == jpcls(utf_prev(regbegin, reginput)))
					return 0;
				break;
			}
			else if (ISkana(reginput[0]))
			{
				if (((reginput - regbegin) >= 1)
								&& ISkanjiPointer(regbegin, &reginput[-1]) == 2)
					break;
				if (((reginput - regbegin) >= 1) && ISkana(reginput[-1]))
					return 0;
				break;
			}
			else
			{
				if (((reginput - regbegin) >= 1)
								&& ISkanjiPointer(regbegin, &reginput[-1]) == 2)
					break;
				if (((reginput - regbegin) >= 1) && ISkana(reginput[-1]))
					break;
			}
#endif
			if (reginput != regbol && isidchar(reginput[-1]))
				return 0;
			if (!reginput[0] || !isidchar(reginput[0]))
				return 0;
			break;
		  case EOW:		/* word\>; reginput points after d */
#ifdef KANJI
			if (reginput == regbol)
				return 0;
			if (regbol != NULL && reginput > regbol
						&& ISkanji(*utf_prev(regbol, reginput)))
			{
				int sclass;
				if (!(sclass = jpcls(utf_prev(regbol, reginput))))
					return 0;
				if (ISkanji(reginput[0]) && sclass == jpcls(reginput))
					return 0;
				break;
			}
			else if (regbol != NULL && (reginput >= (regbol + 1))
													&& ISkana(reginput[-1]))
			{
				if (ISkana(reginput[0]))
					return 0;
				break;
			}
			else
			{
				if (reginput[0] != NUL && ISkanji(reginput[0]))
					break;
				if (reginput[0] != NUL && ISkana(reginput[0]))
					break;
			}
#endif
			if (reginput == regbol || !isidchar(reginput[-1]))
				return 0;
			if (reginput[0] && isidchar(reginput[0]))
				return 0;
			break;
		  case ANY:
			if (*reginput == '\0')
				return 0;
#ifdef KANJI
			if (ISkanji(*reginput))
				reginput += 2;
			else
#endif
			reginput++;
			break;
		  case EXACTLY:{
				int	len;
				char_u  *opnd;

				opnd = OPERAND(scan);
				/* Inline the first character, for speed. */
#ifdef KANJI
				if (!reg_jic)
#endif
				if (*opnd != *reginput && (!reg_ic || TO_UPPER(*opnd) != TO_UPPER(*reginput)))
					return 0;
				len = STRLEN(opnd);
#ifdef KANJI
				if (reg_jic)
				{
					if ((len = jp_strnicmp(reginput, opnd, len)) == 0)
						return 0;
				}
				else
#endif
				if (len > 1 && cstrncmp(opnd, reginput, len) != 0)
					return 0;
				reginput += len;
			}
			break;
		  case ANYOF:
#ifdef KANJI
			if (*reginput == '\0' ||
				mstrjpchr(OPERAND(scan), reginput) == NULL)
				return 0;
			if (ISkanji(*reginput))
				reginput += utf_lenat(reginput, 0);
			else
#else
			if (*reginput == '\0' || cstrchr(OPERAND(scan), *reginput) == NULL)
				return 0;
#endif
			reginput++;
			break;
		  case ANYBUT:
#ifdef KANJI
			if (*reginput == '\0' ||
				mstrjpchr(OPERAND(scan), reginput) != NULL)
				return 0;
			if (ISkanji(*reginput))
				reginput += utf_lenat(reginput, 0);
			else
#else
			if (*reginput == '\0' || cstrchr(OPERAND(scan), *reginput) != NULL)
				return 0;
#endif
			reginput++;
			break;
		  case NOTHING:
			break;
		  case BACK:
			break;
		  case MOPEN + 1:
		  case MOPEN + 2:
		  case MOPEN + 3:
		  case MOPEN + 4:
		  case MOPEN + 5:
		  case MOPEN + 6:
		  case MOPEN + 7:
		  case MOPEN + 8:
		  case MOPEN + 9:{
				int	no;
				char_u  *save;

				no = OP(scan) - MOPEN;
				save = regstartp[no];
				regstartp[no] = reginput; /* Tentatively */
#ifdef DEBUG
				printf("MOPEN  %d pre  @'%s' ('%s' )'%s'\n",
					no, save,
					regstartp[no] ? regstartp[no] : "NULL",
					regendp[no] ? regendp[no] : "NULL");
#endif

				if (regmatch(next)) {
#ifdef DEBUG
				printf("MOPEN  %d post @'%s' ('%s' )'%s'\n",
					no, save,
					regstartp[no] ? regstartp[no] : "NULL",
					regendp[no] ? regendp[no] : "NULL");
#endif
					return 1;
				}
				regstartp[no] = save;		/* We were wrong... */
				return 0;
			}
			/* break; Not Reached */
		  case MCLOSE + 1:
		  case MCLOSE + 2:
		  case MCLOSE + 3:
		  case MCLOSE + 4:
		  case MCLOSE + 5:
		  case MCLOSE + 6:
		  case MCLOSE + 7:
		  case MCLOSE + 8:
		  case MCLOSE + 9:{
				int	no;
				char_u  *save;

				no = OP(scan) - MCLOSE;
				save = regendp[no];
				regendp[no] = reginput; /* Tentatively */
#ifdef DEBUG
				printf("MCLOSE %d pre  @'%s' ('%s' )'%s'\n",
					no, save,
					regstartp[no] ? regstartp[no] : "NULL",
					regendp[no] ? regendp[no] : "NULL");
#endif

				if (regmatch(next)) {
#ifdef DEBUG
				printf("MCLOSE %d post @'%s' ('%s' )'%s'\n",
					no, save,
					regstartp[no] ? regstartp[no] : "NULL",
					regendp[no] ? regendp[no] : "NULL");
#endif
					return 1;
				}
				regendp[no] = save;		/* We were wrong... */
				return 0;
			}
			/* break; Not Reached */
#ifdef BACKREF
		  case BACKREF + 1:
		  case BACKREF + 2:
		  case BACKREF + 3:
		  case BACKREF + 4:
		  case BACKREF + 5:
		  case BACKREF + 6:
		  case BACKREF + 7:
		  case BACKREF + 8:
		  case BACKREF + 9:{
				int	no;
				int				len;

				no = OP(scan) - BACKREF;
				if (regendp[no] != NULL) {
					len = (int)(regendp[no] - regstartp[no]);
					if (cstrncmp(regstartp[no], reginput, len) != 0)
						return 0;
					reginput += len;
				} else {
					/*emsg("backref to 0-repeat");*/
					/*return 0;*/
				}
			}
			break;
#endif
		  case BRANCH:{
				char_u  *save;

				if (OP(next) != BRANCH) /* No choice. */
					next = OPERAND(scan);		/* Avoid recursion. */
				else {
					do {
						save = reginput;
						if (regmatch(OPERAND(scan)))
							return 1;
						reginput = save;
						scan = regnext(scan);
					} while (scan != NULL && OP(scan) == BRANCH);
					return 0;
					/* NOTREACHED */
				}
			}
			break;
		  case STAR:
		  case PLUS:{
				int	nextch;
				int	no;
				char_u  *save;
				int	min;

				/*
				 * Lookahead to avoid useless match attempts when we know
				 * what character comes next.
				 */
				nextch = '\0';
				if (OP(next) == EXACTLY)
				{
					nextch = *OPERAND(next);
					if (reg_ic)
						nextch = TO_UPPER(nextch);
				}
				min = (OP(scan) == STAR) ? 0 : 1;
				save = reginput;
				no = regrepeat(OPERAND(scan));
				while (no >= min)
				{
					/* If it could work, try it. */
					if (nextch == '\0' || (*reginput == nextch ||
									(reg_ic && TO_UPPER(*reginput) == nextch)))
						if (regmatch(next))
							return 1;
					/* Couldn't or didn't -- back up. */
					no--;
					reginput = save + no;
#ifdef KANJI
					if (ISkanjiPointer(save, reginput) == 2)
					{
						no --;
						reginput --;
					}
#endif
				}
				return 0;
			}
			/* break; Not Reached */
		  case END:
			return 1; 		/* Success! */
			/* break; Not Reached */
		  default:
			emsg(e_re_corr);
			return 0;
			/* break; Not Reached */
		}

		scan = next;
	}

	/*
	 * We get here only if there's trouble -- normally "case END" is the
	 * terminating point.
	 */
	emsg(e_re_corr);
	return 0;
}

/*
 - regrepeat - repeatedly match something simple, report how many
 */
static int
regrepeat(char_u *p)
{
	int	count = 0;
	char_u  *scan;
	char_u  *opnd;

	scan = reginput;
	opnd = OPERAND(p);
	switch (OP(p)) {
	  case ANY:
		count = STRLEN(scan);
		scan += count;
		break;
	  case EXACTLY:
#ifdef KANJI
		if (ISkanji(*opnd))
			while (*opnd == *scan && *(opnd + 1) == *(scan + 1)) {
				count+=2;
				scan +=2;
			}
		else
#endif
		while (*opnd == *scan || (reg_ic && TO_UPPER(*opnd) == TO_UPPER(*scan)))
		{
			count++;
			scan++;
		}
		break;
	  case ANYOF:
#ifdef KANJI
		while (*scan != '\0' &&
						mstrjpchr(opnd, scan) != NULL)
			if (ISkanji(*scan))
			{
				int		n = utf_lenat(scan, 0);

				count += n;
				scan += n;
			}
			else
			{
				count ++;
				scan ++;
			}
#else
		while (*scan != '\0' && cstrchr(opnd, *scan) != NULL)
		{
			count++;
			scan++;
		}
#endif
		break;
	  case ANYBUT:
#ifdef KANJI
		while (*scan != '\0' &&
						mstrjpchr(opnd, scan) == NULL)
			if (ISkanji(*scan))
			{
				int		n = utf_lenat(scan, 0);

				count += n;
				scan += n;
			}
			else
			{
				count ++;
				scan ++;
			}
#else
		while (*scan != '\0' && cstrchr(opnd, *scan) == NULL) {
			count++;
			scan++;
		}
#endif
		break;
	  default:					/* Oh dear.  Called inappropriately. */
		emsg(e_re_corr);
		count = 0;				/* Best compromise. */
		break;
	}
	reginput = scan;

	return count;
}

/*
 - regnext - dig the "next" pointer out of a node
 */
static char_u    *
regnext(char_u *p)
{
	int	offset;

	if (p == &regdummy)
		return NULL;

	offset = NEXT(p);
	if (offset == 0)
		return NULL;

	if (OP(p) == BACK)
		return p - offset;
	else
		return p + offset;
}

#ifdef DEBUG

/*
 - regdump - dump a regexp onto stdout in vaguely comprehensible form
 */
void
regdump(regexp *r)
{
	char_u  *s;
	int	op = EXACTLY;		/* Arbitrary non-END op. */
	char_u  *next;
	/*extern char_u    *strchr();*/


	s = r->program + 1;
	while (op != END) { 		/* While that wasn't END last time... */
		op = OP(s);
		printf("%2d%s", s - r->program, regprop(s));	/* Where, what. */
		next = regnext(s);
		if (next == NULL)		/* Next ptr. */
			printf("(0)");
		else
			printf("(%d)", (s - r->program) + (next - s));
		s += 3;
		if (op == ANYOF || op == ANYBUT || op == EXACTLY) {
			/* Literal string, where present. */
			while (*s != '\0') {
				putchar(*s);
				s++;
			}
			s++;
		}
		putchar('\n');
	}

	/* Header fields of interest. */
	if (r->regstart != '\0')
		printf("start `%c' ", r->regstart);
	if (r->reganch)
		printf("anchored ");
	if (r->regmust != NULL)
		printf("must have \"%s\"", r->regmust);
	printf("\n");
}

/*
 - regprop - printable representation of opcode
 */
static char_u    *
regprop(char_u *op)
{
	char_u  *p;
	static char_u 	buf[50];

	(void) strcpy(buf, ":");

	switch (OP(op)) {
	  case BOL:
		p = "BOL";
		break;
	  case EOL:
		p = "EOL";
		break;
	  case ANY:
		p = "ANY";
		break;
	  case ANYOF:
		p = "ANYOF";
		break;
	  case ANYBUT:
		p = "ANYBUT";
		break;
	  case BRANCH:
		p = "BRANCH";
		break;
	  case EXACTLY:
		p = "EXACTLY";
		break;
	  case NOTHING:
		p = "NOTHING";
		break;
	  case BACK:
		p = "BACK";
		break;
	  case END:
		p = "END";
		break;
	  case MOPEN + 1:
	  case MOPEN + 2:
	  case MOPEN + 3:
	  case MOPEN + 4:
	  case MOPEN + 5:
	  case MOPEN + 6:
	  case MOPEN + 7:
	  case MOPEN + 8:
	  case MOPEN + 9:
		sprintf(buf + STRLEN(buf), "MOPEN%d", OP(op) - MOPEN);
		p = NULL;
		break;
	  case MCLOSE + 1:
	  case MCLOSE + 2:
	  case MCLOSE + 3:
	  case MCLOSE + 4:
	  case MCLOSE + 5:
	  case MCLOSE + 6:
	  case MCLOSE + 7:
	  case MCLOSE + 8:
	  case MCLOSE + 9:
		sprintf(buf + STRLEN(buf), "MCLOSE%d", OP(op) - MCLOSE);
		p = NULL;
		break;
	  case BACKREF + 1:
	  case BACKREF + 2:
	  case BACKREF + 3:
	  case BACKREF + 4:
	  case BACKREF + 5:
	  case BACKREF + 6:
	  case BACKREF + 7:
	  case BACKREF + 8:
	  case BACKREF + 9:
		sprintf(buf + STRLEN(buf), "BACKREF%d", OP(op) - BACKREF);
		p = NULL;
		break;
	  case STAR:
		p = "STAR";
		break;
	  case PLUS:
		p = "PLUS";
		break;
	  default:
		sprintf(buf + STRLEN(buf), "corrupt %d", OP(op));
		p = NULL;
		break;
	}
	if (p != NULL)
		(void) strcat(buf, p);
	return buf;
}
#endif

/*
 * The following is provided for those people who do not have strcspn() in
 * their C libraries.  They should get off their butts and do something
 * about it; at least one public-domain implementation of those (highly
 * useful) string routines has been published on Usenet.
 */
#ifdef STRCSPN
/*
 * strcspn - find length of initial segment of s1 consisting entirely
 * of characters not from s2
 */

static int
strcspn(const char_u *s1, const char_u *s2)
{
	char_u  *scan1;
	char_u  *scan2;
	int	count;

	count = 0;
	for (scan1 = s1; *scan1 != '\0'; scan1++) {
		for (scan2 = s2; *scan2 != '\0';)		/* ++ moved down. */
			if (*scan1 == *scan2++)
				return count;
		count++;
	}
	return count;
}
#endif

/*
 * Compare two strings, ignore case if reg_ic set.
 * Return 0 if strings match, non-zero otherwise.
 */
	int
cstrncmp(char_u *s1, char_u *s2, int n)
{
#ifdef KANJI
	if (reg_jic)
		return (jp_strnicmp(s1, s2, (size_t)n) ? 0 : 1);
#endif

	if (!reg_ic)
		return STRNCMP(s1, s2, (size_t)n);

	return vim_strnicmp(s1, s2, (size_t)n);
}

/*
 * cstrchr: This function is used a lot for simple searches, keep it fast!
 */
	char_u *
cstrchr(char_u *s, int c)
{
	char_u		   *p;

	if (!reg_ic)
#ifdef KANJI
	{
		for (p = s; *p; p++)
		{
			if (*p == c)
				return p;
			if (ISkanji(*p))
				p++;
		}
		return NULL;
	}
#else
		return STRCHR(s, c);
#endif

	c = TO_UPPER(c);

	for (p = s; *p; p++)
	{
		if (TO_UPPER(*p) == c)
			return p;
#ifdef KANJI
		if (ISkanji(*p))
			p++;
#endif
	}
	return NULL;
}

#ifdef KANJI
static	char *
strjpchr(char_u *s, char_u c, char_u k)
{
	if (reg_jic)
	{
		char_u		work[2];

		work[0] = c;
		work[1] = k;
		while (*s)
		{
			if (ISkanji(*s))
			{
				if ((ISkanji(c) || ISkana(c)) && k == NUL)
					return(s);
				if (jp_strnicmp(s, work, (size_t)utf_len(work[0])) != 0)
					return(s);
				s += utf_lenat(s, 0);
			}
			else
			{
				if (c == *s && k == NUL)
					return(s);
				if (ISkanji(c) && k == NUL)
					return(s);
				if (jp_strnicmp(s, work, (size_t)utf_len(work[0])) != 0)
					return(s);
				s ++;
			}
		}
	}
	else
	{
		while (*s)
		{
			if (ISkanji(*s))
			{
				if (*s == c && (*(s + 1) == k || k == NUL))
					return s;
				s += utf_lenat(s, 0);
			}
			else
			{
				if (*s == c)
					return s;
				if (reg_ic && (isalpha(*s) && (TO_UPPER(*s) == TO_UPPER(c))))
					return s;
				s ++;
			}
		}
	}
	return NULL;
}

/*
 * Does the character at 'ptr' belong to the character class 'opnd'?
 * Returns a pointer inside 'opnd' when it does, NULL when it does not.
 *
 * The class is a series of S_CHAR followed by one character, or R_CHAR followed
 * by the two ends of a range; the characters are UTF-8. The comparison is on
 * code points. It used to compare the first two bytes, which made [\xe3\x81\x82]
 * (a) match \xe3\x81\x84 (i) too, since both start e3 81, and step two bytes at
 * a time over a three byte character.
 */
static	char *
mstrjpchr(char_u *s, char_u *ptr)
{
	int		cp;
	int		fold = 0;

	cp = utf_decode(ptr, NULL);
	if (cp == UTF8_ERROR)
		cp = *ptr;
	if (reg_jic)
		fold = jp_foldcp(cp);
	while (*s)
	{
		int		tag = *s++;
		int		lo;
		int		hi;

		lo = utf_decode(s, NULL);
		if (lo == UTF8_ERROR)
			lo = *s;
		s += utf_lenat(s, 0);
		if (tag == S_CHAR)
			hi = lo;
		else
		{
			hi = utf_decode(s, NULL);
			if (hi == UTF8_ERROR)
				hi = *s;
			s += utf_lenat(s, 0);
		}
		if (cp >= lo && cp <= hi)
			return (char *)s;
		if (reg_ic && cp < 0x80 && lo < 0x80 && hi < 0x80)
		{
			int		c2;

			for (c2 = lo; c2 <= hi; c2++)
				if (isalpha(c2) && TO_UPPER(c2) == TO_UPPER(cp))
					return (char *)s;
		}
		if (reg_jic)
		{
			/*
			 * Fold both sides. Folding the ends of a range is not sound in
			 * general, but it is within a block, which is what a range like
			 * hiragana a-n is, and it keeps this O(1).
			 */
			if (lo == hi)
			{
				if (jp_foldcp(lo) == fold)
					return (char *)s;
			}
			else if (fold >= jp_foldcp(lo) && fold <= jp_foldcp(hi))
				return (char *)s;
		}
	}
	return NULL;
}
#endif

#ifndef notdef
static char_u	*
regstrext(char_u *exp)
{
	char_u			*	p;
	char_u			*	w = NULL;		/* set on the first pass, used on the second */
	static	char_u	*	exptop	= NULL;
	int					size;
	int					loop;
	int					range;

	if (exptop)
	{
		free(exptop);
		exptop = NULL;
	}
	if (!reg_magic || exp == NULL)
		return(exp);
	for (p = exp, size = 0, range = FALSE, loop = 0; loop < 2;
											p = exp, range = FALSE, loop++)
	{
		while (*p)
		{
# ifdef KANJI
			if (ISkanji(*p))
			{
				if (loop)
				{
					*w++ = p[0];
					*w++ = p[1];
				}
				p += 2;
			}
			else
# endif
			if (*p == '\\')
			{
# ifdef KANJI
				if (ISkanji(p[1]))
				{
					if (loop)
					{
						*w++ = p[0];
						*w++ = p[1];
						*w++ = p[2];
					}
					p += 3;
				}
				else
# endif
				switch (p[1]) {
				case 'e':				/* ESC */
					if (loop == 0)
						size += 1;
					else
						*w++ = '\033';
					p += 2;
					break;
				case 't':				/* TAB */
					if (loop == 0)
						size += 1;
					else
						*w++ = '\t';
					p += 2;
					break;
				case 'r':				/* CR */
					if (loop == 0)
						size += 1;
					else
						*w++ = '\015';
					p += 2;
					break;
				case 'b':				/* BS */
					if (loop == 0)
						size += 1;
					else
						*w++ = '\010';
					p += 2;
					break;
				case 'n':				/* NL */
					if (loop == 0)
						size += 1;
					else
						*w++ = '\012';
					p += 2;
					break;
				case 'i':				/* identifier character */
				case 'k':				/* keyword character */
					if (loop == 0)
						size += 10;
					else
					{
						if (!range)
							*w++ = '[';
						*w++ = '0';
						*w++ = '-';
						*w++ = '9';
						*w++ = 'a';
						*w++ = '-';
						*w++ = 'z';
						if (!reg_ic)
						{
							*w++ = 'A';
							*w++ = '-';
							*w++ = 'F';
						}
						*w++ = '_';
						if (!range)
							*w++ = ']';
					}
					p += 2;
					break;
				case 'I':				/* identifier but exluding digits */
				case 'K':				/* keyword but exluding digits */
					if (loop == 0)
						size += 7;
					else
					{
						if (!range)
							*w++ = '[';
						*w++ = 'a';
						*w++ = '-';
						*w++ = 'z';
						if (!reg_ic)
						{
							*w++ = 'A';
							*w++ = '-';
							*w++ = 'F';
						}
						*w++ = '_';
						if (!range)
							*w++ = ']';
					}
					p += 2;
					break;
				case 'f':				/* filename character */
					if (loop == 0)
						size += 12;
					else
					{
						if (!range)
							*w++ = '[';
						*w++ = '^';
						*w++ = '\\';
						*w++ = '\\';
						*w++ = '/';
						*w++ = ':';
						*w++ = '\\';
						*w++ = '*';
						*w++ = '?';
						*w++ = '"';
						*w++ = '<';
						*w++ = '>';
						*w++ = '|';
						if (!range)
							*w++ = ']';
					}
					p += 2;
					break;
				case 'F':				/* filename but exluding digits */
					if (loop == 0)
						size += 15;
					else
					{
						if (!range)
							*w++ = '[';
						*w++ = '^';
						*w++ = '\\';
						*w++ = '\\';
						*w++ = '/';
						*w++ = ':';
						*w++ = '\\';
						*w++ = '*';
						*w++ = '?';
						*w++ = '"';
						*w++ = '<';
						*w++ = '>';
						*w++ = '|';
						*w++ = '0';
						*w++ = '-';
						*w++ = '9';
						if (!range)
							*w++ = ']';
					}
					p += 2;
					break;
				case 'p':				/* pritable character */
					if (loop == 0)
						size += 7;
					else
					{
						if (!range)
							*w++ = '[';
						*w++ = '\t';
						*w++ = '\040';
						*w++ = '-';
						*w++ = '\137';
						*w++ = '\141';
						*w++ = '-';
						*w++ = '\176';
						if (!range)
							*w++ = ']';
					}
					p += 2;
					break;
				case 'P':				/* pritable but exluding digits */
					if (loop == 0)
						size += 7;
					else
					{
						if (!range)
							*w++ = '[';
						*w++ = '\t';
						*w++ = '\040';
						*w++ = '-';
						*w++ = '\057';
						*w++ = '\072';
						*w++ = '-';
						*w++ = '\137';
						*w++ = '\141';
						*w++ = '-';
						*w++ = '\176';
						if (!range)
							*w++ = ']';
					}
					p += 2;
					break;
				case 's':				/* whitespace  SP/TAB */
					if (loop == 0)
						size += 2;
					else
					{
						if (!range)
							*w++ = '[';
						*w++ = ' ';
						*w++ = '\t';
						if (!range)
							*w++ = ']';
					}
					p += 2;
					break;
				case 'S':				/* not whitespace */
					if (loop == 0)
						size += 3;
					else
					{
						if (!range)
							*w++ = '[';
						*w++ = '^';
						*w++ = ' ';
						*w++ = '\t';
						if (!range)
							*w++ = ']';
					}
					p += 2;
					break;
				case 'd':				/* digit */
					if (loop == 0)
						size += 3;
					else
					{
						if (!range)
							*w++ = '[';
						*w++ = '0';
						*w++ = '-';
						*w++ = '9';
						if (!range)
							*w++ = ']';
					}
					p += 2;
					break;
				case 'D':				/* not digit */
					if (loop == 0)
						size += 4;
					else
					{
						if (!range)
							*w++ = '[';
						*w++ = '^';
						*w++ = '0';
						*w++ = '-';
						*w++ = '9';
						if (!range)
							*w++ = ']';
					}
					p += 2;
					break;
				case 'x':				/* hex digit */
					if (loop == 0)
						size += 9;
					else
					{
						if (!range)
							*w++ = '[';
						*w++ = '0';
						*w++ = '-';
						*w++ = '9';
						*w++ = 'a';
						*w++ = '-';
						*w++ = 'f';
						if (!reg_ic)
						{
							*w++ = 'A';
							*w++ = '-';
							*w++ = 'F';
						}
						if (!range)
							*w++ = ']';
					}
					p += 2;
					break;
				case 'X':				/* not hex digit */
					if (loop == 0)
						size += 10;
					else
					{
						if (!range)
							*w++ = '[';
						*w++ = '^';
						*w++ = '0';
						*w++ = '-';
						*w++ = '9';
						*w++ = 'a';
						*w++ = '-';
						*w++ = 'f';
						if (!reg_ic)
						{
							*w++ = 'A';
							*w++ = '-';
							*w++ = 'F';
						}
						if (!range)
							*w++ = ']';
					}
					p += 2;
					break;
				case 'o':				/* octal digit */
					if (loop == 0)
						size += 3;
					else
					{
						if (!range)
							*w++ = '[';
						*w++ = '0';
						*w++ = '-';
						*w++ = '7';
						if (!range)
							*w++ = ']';
					}
					p += 2;
					break;
				case 'O':				/* not octal digit */
					if (loop == 0)
						size += 4;
					else
					{
						if (!range)
							*w++ = '[';
						*w++ = '^';
						*w++ = '0';
						*w++ = '-';
						*w++ = '7';
						if (!range)
							*w++ = ']';
					}
					p += 2;
					break;
				case 'w':				/* word character */
					if (loop == 0)
						size += 10;
					else
					{
						if (!range)
							*w++ = '[';
						*w++ = '0';
						*w++ = '-';
						*w++ = '9';
						*w++ = 'a';
						*w++ = '-';
						*w++ = 'z';
						if (!reg_ic)
						{
							*w++ = 'A';
							*w++ = '-';
							*w++ = 'F';
						}
						*w++ = '_';
						if (!range)
							*w++ = ']';
					}
					p += 2;
					break;
				case 'W':				/* not word character */
					if (loop == 0)
						size += 11;
					else
					{
						if (!range)
							*w++ = '[';
						*w++ = '^';
						*w++ = '0';
						*w++ = '-';
						*w++ = '9';
						*w++ = 'a';
						*w++ = '-';
						*w++ = 'z';
						if (!reg_ic)
						{
							*w++ = 'A';
							*w++ = '-';
							*w++ = 'F';
						}
						*w++ = '_';
						if (!range)
							*w++ = ']';
					}
					p += 2;
					break;
				case 'h':				/* head of word character */
					if (loop == 0)
						size += 7;
					else
					{
						if (!range)
							*w++ = '[';
						*w++ = 'a';
						*w++ = '-';
						*w++ = 'z';
						if (!reg_ic)
						{
							*w++ = 'A';
							*w++ = '-';
							*w++ = 'F';
						}
						*w++ = '_';
						if (!range)
							*w++ = ']';
					}
					p += 2;
					break;
				case 'H':				/* not head of word character */
					if (loop == 0)
						size += 8;
					else
					{
						if (!range)
							*w++ = '[';
						*w++ = '^';
						*w++ = 'a';
						*w++ = '-';
						*w++ = 'z';
						if (!reg_ic)
						{
							*w++ = 'A';
							*w++ = '-';
							*w++ = 'F';
						}
						*w++ = '_';
						if (!range)
							*w++ = ']';
					}
					p += 2;
					break;
				case 'a':				/* alphabetic character */
					if (loop == 0)
						size += 6;
					else
					{
						if (!range)
							*w++ = '[';
						*w++ = 'a';
						*w++ = '-';
						*w++ = 'z';
						if (!reg_ic)
						{
							*w++ = 'A';
							*w++ = '-';
							*w++ = 'F';
						}
						if (!range)
							*w++ = ']';
					}
					p += 2;
					break;
				case 'A':				/* not alphabetic character */
					if (loop == 0)
						size += 7;
					else
					{
						if (!range)
							*w++ = '[';
						*w++ = '^';
						*w++ = 'a';
						*w++ = '-';
						*w++ = 'z';
						if (!reg_ic)
						{
							*w++ = 'A';
							*w++ = '-';
							*w++ = 'F';
						}
						if (!range)
							*w++ = ']';
					}
					p += 2;
					break;
				case 'l':				/* lowercase character */
					if (loop == 0)
						size += 3;
					else
					{
						if (!range)
							*w++ = '[';
						*w++ = 'a';
						*w++ = '-';
						*w++ = 'z';
						if (!range)
							*w++ = ']';
					}
					p += 2;
					break;
				case 'L':				/* not lowercase character */
					if (loop == 0)
						size += 4;
					else
					{
						if (!range)
							*w++ = '[';
						*w++ = '^';
						*w++ = 'a';
						*w++ = '-';
						*w++ = 'z';
						if (!range)
							*w++ = ']';
					}
					p += 2;
					break;
				case 'u':				/* uppercase character */
					if (loop == 0)
						size += 3;
					else
					{
						if (!range)
							*w++ = '[';
						*w++ = 'A';
						*w++ = '-';
						*w++ = 'Z';
						if (!range)
							*w++ = ']';
					}
					p += 2;
					break;
				case 'U':				/* not uppercase character */
					if (loop == 0)
						size += 4;
					else
					{
						if (!range)
							*w++ = '[';
						*w++ = '^';
						*w++ = 'A';
						*w++ = '-';
						*w++ = 'Z';
						if (!range)
							*w++ = ']';
					}
					p += 2;
					break;
				case '\0':
					if (loop)
						*w++ = p[0];
					p++;
					break;
				default:
					if (loop)
					{
						*w++ = p[0];
						*w++ = p[1];
					}
					p += 2;
					break;
				}
			}
			else if (*p == '[')
			{
				range = TRUE;
				if (loop)
					*w++ = p[0];
				p++;
			}
			else if (*p == ']')
			{
				range = FALSE;
				if (loop)
					*w++ = p[0];
				p++;
			}
			else
			{
				if (loop)
					*w++ = p[0];
				p++;
			}
		}
		if (loop)
			*w = '\0';
		else
		{
			if (size == 0)
				return(exp);
			if ((w = alloc(strlen(exp) + 1 + size)) == NULL)
				return(NULL);
			exptop = w;
		}
	}
	return(exptop);
}
#endif
