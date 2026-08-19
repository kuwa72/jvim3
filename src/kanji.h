/*******************************************************************************
 * vi:ts=4:sw=4
 *	original Ogasawara Hiroyuki (COR.)
 *  original Atsushi Nakamura
 ******************************************************************************/
#ifdef KANJI

#define BRCHAR		'\\'		/* line break char */

#define	JP_EUC		'E'			/* EUC */
#define	JP_JIS		'J'			/* JIS */
#define	JP_SJIS		'S'			/* Shift-JIS */
#define JP_WIDE		'U'			/* UNICODE */
#define JP_UTF8		'T'			/* UTF-8 */
#define JP_NONE		'X'			/* No Kanji */
#define JP_FSTR		"EJSejs"
#define JP_STR		"EJS"
/*
 * 'jmask' is "key display system [file]". The fourth character is the code a
 * brand new file is written in; it used to share the system code, which is the
 * wrong default now that the internal representation is UTF-8: on Windows the
 * system code has to stay CP932 for the ANSI file APIs and the IME, while a new
 * file should be UTF-8. A three character jmask still works and means "write
 * new files in the system code", as before.
 *
 * The system code stays CP932 on Windows because it is also what a command run
 * from ":!" writes, and those still speak CP932. File names are separate: they
 * go through fileconvsto()/fileconvsfrom(), which use UTF-8 on Windows because
 * the manifest (jvim.manifest) makes the ...A file APIs take UTF-8. That is what
 * lets a file name hold characters CP932 has no room for.
 */
#define	JP_KEY		*p_jp		/* key input code */
#define	JP_DISP		*(p_jp + 1)	/* terminal display code */
#define	JP_SYS		*(p_jp + 2)	/* system code for pipes and file names */
#define	JP_FILE		*(p_jp + 3)	/* write code for a new file */
#define JP_MASKLEN	4

/*
 * The code file names are handed to the operating system in. On Windows the
 * manifest asks for UTF-8 as the process ANSI code page, so the ...A file APIs
 * take UTF-8 and a name with characters outside CP932 works; elsewhere a file
 * name is in the system code, as it always was.
 */
#if defined(NT) && defined(UCODE)
# define FILECODE	JP_UTF8
#else
# define FILECODE	((char)toupper(JP_SYS))
#endif

# ifndef JP_DEF
#  if defined(MSDOS) || defined(__CYGWIN__)
#   define	JP_DEF	"SSST"
#  else
#   define	JP_DEF	"EEET"
#  endif
# endif

# ifdef UCODE
#  undef JP_FSTR
#  define JP_FSTR       "EJSUTejsut"
#  undef JP_STR
#  define JP_STR		"EJST"
# endif

#define JP_ASCII	0
#define JP_KANJI	1
#define	JP_KANA		2

#define JPC_ALNUM	3
#define JPC_HIRA	4
#define JPC_KATA	5		/* 2byte kana */
#define JPC_KANJI	6
#define JPC_KIGOU	7
#define JPC_KIGOU2	8
#define JPC_KANA	9		/* 1byte kana */

#define JP1_ALNUM	'#'
#define JP1_HIRA	'$'
#define JP1_KATA	'%'
#define JP1_KIGOU	'!'
#define JP1_KIGOU2	'"'

#define HexChar(_c)	((_c) >= 10 ? 'A' + (_c) - 10 : '0' + (_c));

/* The internal representation is UTF-8; utf8.h has the primitives. */
#include "utf8.h"

#endif /* KANJI */
