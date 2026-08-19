/* utf8.c */
int		utf_len			__ARGS((int));
int		utf_class		__ARGS((int));
int		utf_decode		__ARGS((char_u *, int *));
int		utf_encode		__ARGS((int, char_u *));
int		utf_cpwidth		__ARGS((int));
int		utf_width		__ARGS((char_u *));
char_u *utf_head		__ARGS((char_u *, char_u *));
char_u *utf_prev		__ARGS((char_u *, char_u *));
int		utf_headoff		__ARGS((char_u *, int));
int		utf_lenat		__ARGS((char_u *, int));
int		utf_iskana		__ARGS((char_u *));
int		utf_strlen		__ARGS((char_u *));
