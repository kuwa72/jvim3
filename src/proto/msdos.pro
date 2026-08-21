/* vi:ts=4:sw=4
 *
 * VIM - Vi IMproved		by Bram Moolenaar
 *
 * Read the file "credits.txt" for a list of people who contributed.
 * Read the file "uganda.txt" for copying and usage conditions.
 */

/* prototypes from msdos.c */
long	mch_avail_mem __ARGS((int));
void	vim_delay __ARGS((void));
int		vim_remove __ARGS((char_u *));
void	mch_write __ARGS((char_u *, int));
int 	GetChars __ARGS((char_u *, int, int));
int		mch_char_avail __ARGS((void));
void	mch_suspend __ARGS((void));
#ifdef NT
void	mch_windinit __ARGS((int, char **, char *));
#else
void	mch_windinit __ARGS((void));
#endif
void	check_win __ARGS((int, char **));
void	fname_case __ARGS((char_u *));
void	mch_settitle __ARGS((char_u *, char_u *));
void	mch_restore_title __PARMS((int which));
int		vim_dirname __ARGS((char_u *, int));
int		FullName __ARGS((char_u *, char_u *, int));
int		isFullName __ARGS((char_u *));
long	getperm __ARGS((char_u *));
int		setperm __ARGS((char_u *, long));
int		isdir __ARGS((char_u *));
void	mch_windexit __ARGS((int));
void	mch_settmode __ARGS((int));
int		mch_screenmode __ARGS((char_u *));
int		mch_get_winsize __ARGS((void));
void	set_window __ARGS((void));
void	mch_set_winsize __ARGS((void));
int		call_shell __ARGS((char_u *, int, int));
void	breakcheck __ARGS((void));
char_u	*modname __ARGS((char_u *, char_u *));
int		has_wildcard __ARGS((char_u *));
int		ExpandWildCards __ARGS((int, char_u **, int *, char_u ***, int, int));
void	FreeWild __ARGS((int, char_u **));
int		vim_chdir __ARGS((char_u *));

/* prototypes from dos_v.c/winnt.c */
#ifdef TERMCAP		/* DOSGEN */
void	chk_ctlkey __ARGS((int *, int *));
#endif

/* prototypes from winnt.c */
#ifdef NT
# ifdef __BORLANDC__
void	sleep __ARGS((int));
# else
int		sleep __ARGS((int));
# endif
void	wincmd_paste __ARGS((void));
void	wincmd_cut __ARGS((void));
void	wincmd_delete __ARGS((void));
void	wincmd_active __ARGS((void));
void	wincmd_redraw __ARGS((void));
int		wincmd_grep __ARGS((char *, char *));
int		wincmd_togle __ARGS((void));
int		wincmd_rotate __ARGS((void));
void	InitCommand __ARGS((void));
void	win_history_append __ARGS((BUF *));
char *	win_history_line __ARGS((BUF *));
#endif
#ifdef NT
int		clip_put		__ARGS((char_u *, int));
char_u *clip_get		__ARGS((void));
/* reading a directory without the ANSI API's field limits; also used by xargs.c */
HANDLE	find_first_name	__ARGS((char_u *, char_u *, int, DWORD *));
int		find_next_name	__ARGS((HANDLE, char_u *, int, DWORD *));
#define FIND_NAMELEN	(MAX_PATH * 3 + 1)		/* a name in UTF-8, at most */
#endif
