/* vi:ts=4:sw=4
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *
 * Read the file "credits.txt" for a list of people who contributed.
 * Read the file "uganda.txt" for copying and usage conditions.
 */

#if defined(SYSV_UNIX) || defined(BSD_UNIX)
# ifndef UNIX
#  define UNIX
# endif
#endif

/*
 * Shorhand for unsinged variables. Many systems, but not all, have u_char
 * already defined, so we use char_u to avoid trouble.
 */
typedef unsigned char	char_u;
typedef unsigned short	short_u;
typedef unsigned int	int_u;
typedef unsigned long	long_u;

#include <stdio.h>
#include <ctype.h>

#if !defined(DOMAIN) && !defined(NOLIMITS)
# include <limits.h>		/* For INT_MAX, remove this if it does not exist */
#endif

#ifdef BSD_UNIX
# ifndef apollo
#  include <strings.h>
# endif
# ifdef __STDC__
#  include <string.h>
# endif
#else
# include <string.h>
#endif

#if defined(NT) && !defined(NO_EARLY_WINDOWS_H)
/*
 * mingw-w64's <winnt.h> declares struct bit-fields named CR, NL and friends
 * (IMAGE_ARM64_RUNTIME_FUNCTION_ENTRY). They are parsed as the macros from
 * ascii.h if that is included first, which breaks every file. Pull in the
 * Win32 headers before the macros are defined.
 */
# include <windows.h>
#endif

#include "ascii.h"
#include "keymap.h"
#include "term.h"
#include "macros.h"

/*
 * Syntax colouring. It needs the KANJI screen, which keeps an attribute for
 * every cell and so has somewhere to put a colour, and it needs somewhere to
 * send that colour: the Win32 GUI paints it, a terminal is sent SGR escapes.
 * -DSYNTAX asks for it; this is the name the rest of the tree tests, so that
 * "which platforms" is answered in one place rather than in twenty #ifs.
 */
#if defined(KANJI) && defined(SYNTAX)
# define USE_SYNTAX
/*
 * Is this window being coloured? On Windows only the GUI can: the console
 * build draws through the same screen array but has no way to paint it.
 */
# ifdef NT
#  define SYN_ON(wp)	((wp)->w_p_syt && GuiWin)
# else
#  define SYN_ON(wp)	((wp)->w_p_syt)
# endif
/* What syn_decode() answers with. One of TEXT, REVERSE or RGB, plus a type. */
# define SYN_BOLD		0x01
# define SYN_ITALIC		0x02
# define SYN_ULINE		0x04
# define SYN_TEXT		0x08	/* whatever the ordinary text colour is */
# define SYN_REVERSE	0x10	/* text and background the other way round */
# define SYN_RGB		0x20	/* a colour, left in the int passed by address */
#endif

#ifdef LATTICE
# include <sys/types.h>
# include <sys/stat.h>
#else
# ifdef _DCC
#  include <sys/stat.h>
# else
#  ifdef MSDOS 
#   include <sys/stat.h>
#  else
#   ifdef UNIX
#	 ifndef linux
#	  define volatile		/* needed for gcc */
#	  define signed			/* needed for gcc */
#    endif
#    include <sys/types.h>
#    include <sys/stat.h>
#   else
#     include <stat.h>
#   endif
#  endif
# endif
#endif

#if !defined(DOMAIN) && !defined(NOSTDLIB)
# include <stdlib.h>
#endif

#ifdef AMIGA
/*
 * arpbase.h must be included before functions.h
 */
# include <libraries/arpbase.h>

/*
 * This won't be needed if you have a version of Lattice 4.01 without broken
 * break signal handling.
 */
# include <signal.h>
#endif

#ifndef AMIGA
/*
 * For the Amiga we use a version of getenv that does local variables under 2.0
 */
# define vimgetenv(x) (char_u *)getenv((char *)x)
#endif

#ifdef AZTEC_C
# include <functions.h>
# define __ARGS(x)	x
# define __PARMS(x)	x
#endif

#ifdef SASC
# include <clib/exec_protos.h>
# define __ARGS(x)	x
# define __PARMS(x)	x
#endif

#ifdef _DCC
# include <functions.h>
# define __ARGS(x)	x
# define __PARMS(x)	x
#endif

#ifdef __TURBOC__
# define __ARGS(x) x
# define __PARMS(x)	x
#endif
#ifdef __BORLANDC__
# define __ARGS(x) x
# define __PARMS(x)	x
#endif

#if defined(MSDOS) && !defined(NT)
# include <dos.h>
# include <dir.h>
#endif
#if defined(MSDOS) && defined(NT) && defined(__BORLANDC__)
# include <dir.h>
#endif
#if __TURBOC__ >= 0x500
# include <wtypes.h>
#endif

#ifdef SOLARIS
# include <stdlib.h>
#endif

#ifdef UNIX
# include <unistd.h>		/* any unix that doesn't have it? */
# ifdef SCO
#  undef M_XENIX
#  include <sys/ndir.h>		/* for MAXNAMLEN */
# else
/* <sys/dir.h> is a compatibility header the other systems still ship and
 * DragonFly does not; <dirent.h> is where this belongs. */
#  if defined(SOLARIS) || defined(AIX) || defined(ARCHIE)  || __FreeBSD__ >= 3 || __CYGWIN__ || defined(__bsdi__) || defined(__DragonFly__)
#   include <dirent.h>		/* for MAXNAMLEN */
#  else
#   include <sys/dir.h>		/* for MAXNAMLEN */
#  endif
# endif
# ifdef USL
#  define MAXNAMLEN DIRSIZ
# endif
# if defined(UFS_MAXNAMLEN) && !defined(MAXNAMLEN)
#  define MAXNAMLEN UFS_MAXNAMLEN		/* for dynix/ptx */
# endif
# if defined(NAME_MAX) && !defined(MAXNAMLEN)
#  define MAXNAMLEN NAME_MAX			/* for Linux before .99p3 */
# endif
# if !defined(MAXNAMLEN)
#  define MAXNAMLEN 512                 /* for all other Unix */
# endif
#endif

#ifdef UNICOS		/* would make sense for other systems too */
# include <errno.h>
#endif

#if defined(__STDC__) || defined(__GNUC__)
# ifndef __ARGS
#  define __ARGS(x) x
# endif /* __ARGS */
/*
 * __PARMS was left out here, so on every compiler that is neither Aztec, SAS,
 * DICE, Turbo C nor Borland it fell through to the "()" fallback below -- which
 * means every prototype in the proto directory was reduced to an empty list
 * and none of them have been checked against their definitions for 30 years.
 */
# ifndef __PARMS
#  define __PARMS(x) x
# endif /* __PARMS */
#else /*__STDC__*/
# if defined(_SEQUENT_) && !defined(_STDLIB_H_)
  extern char *getenv();
  extern void *malloc();
# endif
#endif /* __STDC__ */

#if defined(_MSC_VER) && (_MSC_VER >= 1000)
# ifndef __ARGS
#  define __ARGS(x)  x
# endif
# ifndef __PARMS
#  define __PARMS(x) x
# endif
#endif

#ifndef __ARGS
#define __ARGS(x)	()
#endif
#ifndef __PARMS
#define __PARMS(x)	()
#endif

/*
 * for systems that do not allow free(NULL)
 */
#ifdef NO_FREE_NULL
# define free(x)	nofreeNULL(x)
  extern void nofreeNULL __ARGS((void *));
#endif

/*
 * fnamecmp() is used to compare filenames.
 * On some systems case in a filename does not matter, on others it does.
 * (this does not account for maximum name lengths, thus it is not 100% accurate!)
 */
#if defined(AMIGA) || defined(MSDOS)
# define fnamecmp(x, y) stricmp((char *)(x), (char *)(y))
#else
# define fnamecmp(x, y) strcmp((char *)(x), (char *)(y))
#endif

/*
 * flags for updateScreen()
 * The higher the value, the higher the priority
 */
#define VALID					10	/* buffer not changed */
#define INVERTED				20	/* redisplay inverted part */
#define VALID_TO_CURSCHAR		30	/* buffer at/below cursor changed */
#define NOT_VALID				40	/* buffer changed */
#define CURSUPD					50	/* buffer changed, update cursor first */
#define CLEAR					60	/* screen messed up, clear it */

/* values for State */
/*
 * The lowest three bits are used to distinguish normal/cmdline/insert+replace
 * mode. This is used for mapping.
 */
#define NORMAL					0x01
#define NORMAL_BUSY				0x11	/* busy interpreting a command */
#define CMDLINE 				0x02
#define INSERT					0x04
#define REPLACE 				0x24	/* replace mode */
#define HELP					0x30	/* displaying help */
#define NOMAPPING 				0x40	/* no :mapping mode for vgetc() */
#define ONLYKEY 				0x70	/* like NOMAPPING, but keys allowed */
#define HITRETURN				0x51	/* waiting for a return */
#define SETWSIZE				0x60	/* window size has changed */
#define ABBREV					0x80	/* abbreviation instead of mapping */

/* directions */
#define FORWARD 				 1
#define BACKWARD				 -1

/* return values for functions */
#define OK						1
#define FAIL					0

/* for GetChars */
#define T_PEEK					1	/* do not wait at all */
#define T_WAIT					2	/* wait for a short time */
#define T_BLOCK					3	/* wait forever */

#define VISUALLINE			MAXCOL	/* Visual is linewise */

#ifdef WEBB_COMPLETE
/*
 * values for command line completion
 */
#define CONTEXT_UNKNOWN			-2
#define EXPAND_UNSUCCESSFUL		-1
#define EXPAND_NOTHING			0
#define EXPAND_COMMANDS			1
#define EXPAND_FILES			2
#define EXPAND_DIRECTORIES		3
#define EXPAND_SETTINGS			4
#define EXPAND_BOOL_SETTINGS	5
#define EXPAND_TAGS				6
#endif /* WEBB_COMPLETE */
/*
 * Boolean constants
 */
#ifndef TRUE
#define FALSE	(0)			/* note: this is an int, not a long! */
#define TRUE	(1)
#endif

/*
 * Maximum and minimum screen size (height is unlimited)
 */
#ifdef UNIX
# define MAX_COLUMNS 	1024L
#else
# define MAX_COLUMNS 	255L
#endif
#define MIN_COLUMNS		5
#define MIN_ROWS		1
#define STATUS_HEIGHT	1		/* height of a status line under a window */

/*
 * Buffer sizes
 */
#if defined(UNIX) || defined(NT)		/* Unix has plenty of memory */
# define CMDBUFFSIZE	1024	/* size of the command processing buffer */
#else
# define CMDBUFFSIZE	256		/* size of the command processing buffer */
#endif

#define LSIZE		512			/* max. size of a line in the tags file */

#define IOSIZE	   (1024+1) 	/* file i/o and sprintf buffer size */
/*
 * How much of IObuff a file name may take when it goes into a message. The rest
 * of the message -- "[readonly] [long lines split] N lines, N characters" and
 * the like -- is sprintf()ed on after the name, and home_replace() filling the
 * buffer to its last byte first left that writing past the end. Names are longer
 * than they were: MAXPATHL counts bytes, and a UTF-8 name is three of them a
 * character.
 */
#define MSGNAMELEN (IOSIZE - 200)	/* file name in a message, with room after */

#define	TERMBUFSIZE	1024

#if defined(linux) || defined(__CYGWIN__)
# define TBUFSZ 2048			/* buffer size for termcap entry */
#else
# define TBUFSZ 1024			/* buffer size for termcap entry */
#endif

/*
 * maximum length of a file name path
 */
#ifdef UNIX
# define MAXPATHL	1024		/* Unix has long paths and plenty of memory */
#else
# ifdef NT
/*
 * A count of bytes, and a name is held in UTF-8 -- three bytes a character for
 * Japanese. MAX_PATH (260) is a count of *characters*, so 260 bytes would not
 * even fit a name Windows itself considers legal: a file whose name ran past it
 * could not be opened, because the completion offered the name and ":e" then
 * truncated it to a path that did not exist.
 *
 * 4096 rather than 3 * 260: jvim.manifest asks for longPathAware, so where
 * Windows has long paths turned on there is no 260 character limit to scale
 * from. 4096 bytes is 1365 Japanese characters, past anything a real tree
 * produces, and FullName() reports a path that does not fit rather than
 * shortening it into a name that means something else.
 */
#  define MAXPATHL	4096
# else
#  if 0
#  define MAXPATHL	128			/* not too long to put name on stack */
#  else
#  define MAXPATHL	260			/* MS-DOS names are short and stacks small */
#  endif
# endif
#endif

#ifdef MSDOS
# define WRITEBIN	"wb"		/* no CR-LF translation */
# define READBIN	"rb"
# define APPENDBIN	"ab"
#else
# define WRITEBIN	"w"
# define READBIN	"r"
# define APPENDBIN	"a"
#endif

#define CHANGED   		set_Changed()
#define UNCHANGED(buf)	unset_Changed(buf)

/*
 * defines to avoid typecasts from (char_u *) to (char *) and back
 */
#define STRCHR(s, c)		(char_u *)strchr((char *)(s), c)
#define STRRCHR(s, c)		(char_u *)strrchr((char *)(s), c)
#define STRLEN(s)			strlen((char *)(s))
#define STRCPY(d, s)		strcpy((char *)(d), (char *)(s))
/*
 * Move a string over itself, e.g. STRMOVE(p, p + 1) to drop the first
 * character. strcpy() may not be used when the ranges overlap.
 */
#define STRMOVE(d, s)			memmove((char *)(d), (char *)(s), \
											STRLEN(s) + 1)
#define STRNCPY(d, s, n)	strncpy((char *)(d), (char *)(s), n)
#define STRCMP(d, s)		strcmp((char *)(d), (char *)(s))
#define STRNCMP(d, s, n)	strncmp((char *)(d), (char *)(s), n)
#define STRCAT(d, s)		strcat((char *)(d), (char *)(s))

#define MSG(s)				msg((char_u *)(s))
#define EMSG(s)				emsg((char_u *)(s))
#define EMSG2(s, p)			emsg2((char_u *)(s), (char_u *)(p))
#define OUTSTR(s)			outstr((char_u *)(s))
#define OUTSTRN(s)			outstrn((char_u *)(s))

typedef long			linenr_t;	/* line number type */
typedef unsigned		colnr_t;	/* column number type */

#define MAXLNUM (0x7fffffff)		/* maximum (invalid) line number */
#ifdef INT_MAX
# define MAXCOL	INT_MAX				/* maximum column number */
#else
# define MAXCOL	32767				/* maximum column number, 15 bits */
#endif

/*
 * Some versions of isspace() handle Meta characters like a space!
 * This define fixes that.
 */
#ifdef VIM_ISSPACE
# ifdef isspace
#  undef isspace
# endif /* isspace */
# define isspace(x)  (((x) >= 9 && (x) <= 13) || ((x) == 32))
#endif /* VIM_ISSPACE */

/*
 * iswhite() is used for "^" and the like
 */
#define iswhite(x)	((x) == ' ' || (x) == '\t')

/*
 * A key code is not a character.
 *
 * vgetc() returns one value for every key, and a special key is a number past
 * 255: K_UARROW is 321 in this build, K_DARROW 322, and the function keys go up
 * from there (see keymap.h). The ctype functions are defined only for a value
 * that fits in an unsigned char, plus EOF, and a C runtime is free to index its
 * table with whatever it is given: the Windows one does, so it answered "yes, an
 * upper case letter" for a cursor key. Where that answer decided an array index
 * -- a register name, a mark name -- the index landed hundreds of entries past
 * the end of the array.
 *
 * Nothing that names a register, a mark, a count or a digraph is outside ASCII,
 * so ask these instead of ctype wherever the value came from the keyboard. They
 * are range tests: no table, no locale, the same answer on every platform.
 */
#define isasciilower(c)	((c) >= 'a' && (c) <= 'z')
#define isasciiupper(c)	((c) >= 'A' && (c) <= 'Z')
#define isasciidigit(c)	((c) >= '0' && (c) <= '9')
#define isasciialpha(c)	(isasciilower(c) || isasciiupper(c))
#define isasciialnum(c)	(isasciialpha(c) || isasciidigit(c))
#define isasciixdigit(c) (isasciidigit(c) || ((c) >= 'a' && (c) <= 'f') \
										  || ((c) >= 'A' && (c) <= 'F'))

#include "structs.h"		/* file that defines many structures */

#ifdef AMIGA
# include "amiga.h"
#endif

#ifdef ARCHIE
# include "archie.h"
#endif

#ifdef MSDOS
# include "msdos.h"
#endif

#ifdef UNIX
# include "unix.h"
#endif

#ifdef _WIN32
# include <windows.h>
# undef   DELETE
#endif

