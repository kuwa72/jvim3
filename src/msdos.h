/* vi:ts=4:sw=4
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *
 * Read the file "credits.txt" for a list of people who contributed.
 * Read the file "uganda.txt" for copying and usage conditions.
 */

/*
 * MSDOS Machine-dependent things.
 */

/*
 * Names for the EXRC, HELP and temporary files.
 * Some of these may have been defined in the makefile.
 */

#ifndef SYSVIMRC_FILE
# ifdef notdef	/* ken */
#  define SYSVIMRC_FILE	"$VIM\\_vimrc"
# else
#  define SYSVIMRC_FILE	"$HOME\\_vimrc"
# endif
#endif

#ifndef SYSEXRC_FILE
# ifdef notdef	/* ken */
#  define SYSEXRC_FILE	"$VIM\\_exrc"
# else
#  define SYSEXRC_FILE	"$HOME\\_exrc"
# endif
#endif

#ifndef VIMRC_FILE
# define VIMRC_FILE		"_vimrc"
#endif

#ifndef EXRC_FILE
# define EXRC_FILE		"_exrc"
#endif

#ifndef DEFVIMRC_FILE /* ken */
# define DEFVIMRC_FILE	"$VIM\\vimrc"
#endif

#ifndef VIM_HLP
# define VIM_HLP		"$VIM\\vim.hlp"
#endif

#ifndef notdef
# ifndef BACKUPDIR
#  define BACKUPDIR		"$HOME"
# endif
#endif

#ifndef DEF_DIR
# ifndef notdef	/* ken */
#  define DEF_DIR		"$TMP"
# else
#  define DEF_DIR		"c:\\tmp"
# endif
#endif

#define TMPNAME1		"viXXXXXX"		/* put it in current dir */
#define TMPNAME2		"voXXXXXX"		/*  is there a better place? */
#ifdef NT
/* Room for a full path: vim_mktemp() puts the file where GetTempPath() says,
 * not in the current directory. L_tmpnam, which this used to be, is 14 with
 * msvcrt -- P_tmpdir plus twelve -- and would not hold one. */
# define TMPNAMELEN		MAXPATHL
#else
# define TMPNAMELEN		10
#endif

#ifndef MAXMEM
# ifdef NT
#  define MAXMEM		512				/* use up to 512Kbyte for buffer */
# else
#  define MAXMEM		256				/* use up to 256Kbyte for buffer */
# endif
#endif
#ifndef MAXMEMTOT
# ifdef NT
#  define MAXMEMTOT		2048			/* use up to 2048Kbyte for Vim */
# else
#  define MAXMEMTOT		0				/* decide in set_init */
# endif
#endif

#ifdef NT
# define BASENAMELEN	_MAX_PATH		/* length of base of file name */
#else
# define BASENAMELEN	8				/* length of base of file name */
#endif

/*
 * MSDOS Machine-dependent routines.
 */

#ifdef remove
# undef remove                   /* MSDOS remove()s when not readonly */
#endif
#define remove vim_remove

/* use chdir() that also changes the default drive */
#define chdir vim_chdir
