/* charset.c */
unsigned char *transchar __PARMS((int c));
int charsize __PARMS((unsigned char *p));
int transcharsize __PARMS((int c));
int strsize __PARMS((unsigned char *s));
int chartabsize __PARMS((unsigned char *p, long col));
int isidchar __PARMS((int c));
#ifndef notdef
int isabchar __PARMS((int c));
#endif
