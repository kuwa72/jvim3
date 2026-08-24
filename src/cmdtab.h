/* vi:ts=4
 *
 * VIM - Vi IMproved
 *
 * Code Contributions By:	Bram Moolenaar			mool@oce.nl
 *							Tim Thompson			twitch!tjt
 *							Tony Andrews			onecom!wldrdg!tony 
 *							G. R. (Fred) Walter		watmath!watcgl!grwalter 
 */

/*
 * THIS FILE IS AUTOMATICALLY PRODUCED - DO NOT EDIT
 */

#define RANGE	0x01			/* allow a linespecs */
#define BANG	0x02			/* allow a ! after the command name */
#define EXTRA	0x04			/* allow extra args after command name */
#define XFILE	0x08			/* expand wildcards in extra part */
#define NOSPC	0x10			/* no spaces allowed in the extra part */
#define	DFLALL	0x20			/* default file range is 1,$ */
#define NODFL	0x40			/* do not default to the current file name */
#define NEEDARG	0x80			/* argument required */
#define TRLBAR	0x100			/* check for trailing vertical bar */
#define REGSTR	0x200			/* allow "x for register designation */
#define COUNT	0x400			/* allow count in argument, after command */
#define NOTRLCOM 0x800			/* no trailing comment allowed */
#define ZEROR	0x1000			/* zero line number allowed */
#define USECTRLV 0x2000			/* do not remove CTRL-V from argument */
#define NOTADR	0x4000			/* number before command is not an address */
#define FILES	(XFILE + EXTRA)	/* multiple extra files allowed */
#define WORD1	(EXTRA + NOSPC)	/* one extra word allowed */
#define FILE1	(FILES + NOSPC)	/* 1 file allowed, defaults to current file */
#define NAMEDF	(FILE1 + NODFL)	/* 1 file allowed, defaults to "" */
#define NAMEDFS	(FILES + NODFL)	/* multiple files allowed, default is "" */

/*
 * This array maps ex command names to command codes. The order in which
 * command names are listed below is significant -- ambiguous abbreviations
 * are always resolved to be the first possible match (e.g. "r" is taken
 * to mean "read", not "rewind", because "read" comes before "rewind").
 * Not supported commands are included to avoid ambiguities.
 */
static struct
{
	char_u	*cmd_name;	/* name of the command */
	short	 cmd_argt;	/* command line arguments permitted/needed/used */
} cmdnames[] =
{
	{(char_u *)"append",		BANG+RANGE+TRLBAR},			/* not supported */
#define CMD_append 0
	{(char_u *)"all",			TRLBAR},
#define CMD_all 1
	{(char_u *)"abbreviate",	EXTRA+TRLBAR+NOTRLCOM+USECTRLV},
#define CMD_abbreviate 2
	{(char_u *)"args",			RANGE+NOTADR+BANG+NAMEDFS},
#define CMD_args 3
	{(char_u *)"argument",		BANG+RANGE+NOTADR+COUNT+EXTRA},
#define CMD_argument 4
	{(char_u *)"buffer",		RANGE+NOTADR+COUNT+TRLBAR},
#define CMD_buffer 5
	{(char_u *)"ball",			TRLBAR},
#define CMD_ball 6
	{(char_u *)"buffers",		TRLBAR},
#define CMD_buffers 7
	{(char_u *)"bdelete",		BANG+RANGE+NOTADR+COUNT+EXTRA+TRLBAR},
#define CMD_bdelete 8
	{(char_u *)"bunload",		BANG+RANGE+NOTADR+COUNT+EXTRA+TRLBAR},
#define CMD_bunload 9
	{(char_u *)"bmodified",		RANGE+NOTADR+COUNT+TRLBAR},
#define CMD_bmodified 10
	{(char_u *)"bnext",			RANGE+NOTADR+COUNT+TRLBAR},
#define CMD_bnext 11
	{(char_u *)"bNext",			RANGE+NOTADR+COUNT+TRLBAR},
#define CMD_bNext 12
	{(char_u *)"bprevious",		RANGE+NOTADR+COUNT+TRLBAR},
#define CMD_bprevious 13
	{(char_u *)"brewind",		RANGE+TRLBAR},
#define CMD_brewind 14
	{(char_u *)"blast",			RANGE+TRLBAR},
#define CMD_blast 15
	{(char_u *)"change",		BANG+RANGE+COUNT+TRLBAR},	/* not supported */
#define CMD_change 16
	{(char_u *)"cabbrev",		EXTRA+TRLBAR+NOTRLCOM+USECTRLV},
#define CMD_cabbrev 17
	{(char_u *)"cc",			TRLBAR+WORD1+BANG},
#define CMD_cc 18
	{(char_u *)"cd",			NAMEDF+TRLBAR},
#define CMD_cd 19
	{(char_u *)"center",		TRLBAR+RANGE+EXTRA},
#define CMD_center 20
	{(char_u *)"cf",			TRLBAR+FILE1+BANG},
#define CMD_cf 21
	{(char_u *)"chdir",			NAMEDF+TRLBAR},
#define CMD_chdir 22
	{(char_u *)"cl",			TRLBAR},
#define CMD_cl 23
	{(char_u *)"close",			BANG+TRLBAR},
#define CMD_close 24
	{(char_u *)"cmap",			BANG+EXTRA+TRLBAR+NOTRLCOM+USECTRLV},
#define CMD_cmap 25
	{(char_u *)"cn",			TRLBAR+WORD1+BANG},
#define CMD_cn 26
	{(char_u *)"cnoremap",		BANG+EXTRA+TRLBAR+NOTRLCOM+USECTRLV},
#define CMD_cnoremap 27
	{(char_u *)"cnoreabbrev",	EXTRA+TRLBAR+NOTRLCOM+USECTRLV},
#define CMD_cnoreabbrev 28
	{(char_u *)"copy",			RANGE+EXTRA+TRLBAR},
#define CMD_copy 29
	{(char_u *)"colorscheme",	EXTRA+TRLBAR},
#define CMD_colorscheme 30
	{(char_u *)"cp",			TRLBAR+WORD1+BANG},
#define CMD_cp 31
	{(char_u *)"cq",			TRLBAR+BANG},
#define CMD_cq 32
	{(char_u *)"cunmap",		BANG+EXTRA+TRLBAR+USECTRLV},
#define CMD_cunmap 33
	{(char_u *)"cunabbrev",		EXTRA+TRLBAR+USECTRLV},
#define CMD_cunabbrev 34
	{(char_u *)"delete",		RANGE+REGSTR+COUNT+TRLBAR},
#define CMD_delete 35
	{(char_u *)"display",		TRLBAR},
#define CMD_display 36
	{(char_u *)"digraphs",		EXTRA+TRLBAR},
#define CMD_digraphs 37
	{(char_u *)"edit",			BANG+FILE1},
#define CMD_edit 38
	{(char_u *)"else",			TRLBAR},
#define CMD_else 39
	{(char_u *)"elseif",		EXTRA+TRLBAR},
#define CMD_elseif 40
	{(char_u *)"endif",			TRLBAR},
#define CMD_endif 41
	{(char_u *)"ex",			BANG+FILE1},
#define CMD_ex 42
	{(char_u *)"exit",			BANG+FILE1+DFLALL+TRLBAR},
#define CMD_exit 43
	{(char_u *)"file",			FILE1+TRLBAR},
#define CMD_file 44
	{(char_u *)"files",			TRLBAR},
#define CMD_files 45
	{(char_u *)"finish",		TRLBAR},
#define CMD_finish 46
	{(char_u *)"global",		RANGE+BANG+EXTRA+DFLALL},
#define CMD_global 47
	{(char_u *)"help",			TRLBAR},
#define CMD_help 48
	{(char_u *)"highlight",		EXTRA+TRLBAR+USECTRLV},
#define CMD_highlight 49
	{(char_u *)"insert",		BANG+RANGE+TRLBAR},			/* not supported */
#define CMD_insert 50
	{(char_u *)"iabbrev",		EXTRA+TRLBAR+NOTRLCOM+USECTRLV},
#define CMD_iabbrev 51
	{(char_u *)"if",			EXTRA+TRLBAR},
#define CMD_if 52
	{(char_u *)"imap",			BANG+EXTRA+TRLBAR+NOTRLCOM+USECTRLV},
#define CMD_imap 53
	{(char_u *)"inoremap",		BANG+EXTRA+TRLBAR+NOTRLCOM+USECTRLV},
#define CMD_inoremap 54
	{(char_u *)"inoreabbrev",	EXTRA+TRLBAR+NOTRLCOM+USECTRLV},
#define CMD_inoreabbrev 55
	{(char_u *)"iunmap",		BANG+EXTRA+TRLBAR+USECTRLV},
#define CMD_iunmap 56
	{(char_u *)"iunabbrev",		EXTRA+TRLBAR+USECTRLV},
#define CMD_iunabbrev 57
	{(char_u *)"join",			RANGE+COUNT+TRLBAR},
#define CMD_join 58
	{(char_u *)"jumps",			TRLBAR},
#define CMD_jumps 59
	{(char_u *)"k",				RANGE+WORD1+TRLBAR},
#define CMD_k 60
	{(char_u *)"list",			RANGE+COUNT+TRLBAR},
#define CMD_list 61
	{(char_u *)"last",			EXTRA+BANG},
#define CMD_last 62
	{(char_u *)"left",			TRLBAR+RANGE+EXTRA},
#define CMD_left 63
	{(char_u *)"let",			EXTRA+TRLBAR+NOTRLCOM+USECTRLV},
#define CMD_let 64
	{(char_u *)"move",			RANGE+EXTRA+TRLBAR},
#define CMD_move 65
	{(char_u *)"mark",			RANGE+WORD1+TRLBAR},
#define CMD_mark 66
	{(char_u *)"marks",			TRLBAR},
#define CMD_marks 67
	{(char_u *)"map",			BANG+EXTRA+TRLBAR+NOTRLCOM+USECTRLV},
#define CMD_map 68
	{(char_u *)"make",			NEEDARG+EXTRA+TRLBAR+XFILE},
#define CMD_make 69
	{(char_u *)"mkexrc",		BANG+FILE1+TRLBAR},
#define CMD_mkexrc 70
	{(char_u *)"mkvimrc",		BANG+FILE1+TRLBAR},
#define CMD_mkvimrc 71
	{(char_u *)"mfstat",		TRLBAR},				/* for debugging */
#define CMD_mfstat 72
	{(char_u *)"mode",			WORD1+TRLBAR},
#define CMD_mode 73
	{(char_u *)"next",			RANGE+NOTADR+BANG+NAMEDFS},
#define CMD_next 74
	{(char_u *)"new",			BANG+FILE1+RANGE+NOTADR},
#define CMD_new 75
	{(char_u *)"number",		RANGE+COUNT+TRLBAR},
#define CMD_number 76
	{(char_u *)"#",				RANGE+COUNT+TRLBAR},
#define CMD_pound 77
	{(char_u *)"noremap",		BANG+EXTRA+TRLBAR+NOTRLCOM+USECTRLV},
#define CMD_noremap 78
	{(char_u *)"noreabbrev",	EXTRA+TRLBAR+NOTRLCOM+USECTRLV},
#define CMD_noreabbrev 79
	{(char_u *)"Next",			EXTRA+RANGE+NOTADR+COUNT+BANG},
#define CMD_Next 80
	{(char_u *)"only",			BANG+TRLBAR},
#define CMD_only 81
	{(char_u *)"print",			RANGE+COUNT+TRLBAR},
#define CMD_print 82
	{(char_u *)"pop",			RANGE+NOTADR+COUNT+TRLBAR+ZEROR},
#define CMD_pop 83
	{(char_u *)"put",			RANGE+BANG+REGSTR+TRLBAR},
#define CMD_put 84
	{(char_u *)"preserve",		TRLBAR},
#define CMD_preserve 85
	{(char_u *)"previous",		EXTRA+RANGE+NOTADR+COUNT+BANG},
#define CMD_previous 86
	{(char_u *)"pwd",			TRLBAR},
#define CMD_pwd 87
	{(char_u *)"quit",			BANG+TRLBAR},
#define CMD_quit 88
	{(char_u *)"qall",			BANG+TRLBAR},
#define CMD_qall 89
	{(char_u *)"read",			RANGE+NAMEDF+TRLBAR+ZEROR},
#define CMD_read 90
	{(char_u *)"rewind",		EXTRA+BANG},
#define CMD_rewind 91
	{(char_u *)"recover",		FILE1+TRLBAR},				/* not supported */
#define CMD_recover 92
	{(char_u *)"redo",			TRLBAR},
#define CMD_redo 93
	{(char_u *)"right",			TRLBAR+RANGE+EXTRA},
#define CMD_right 94
	{(char_u *)"resize",		TRLBAR+WORD1},
#define CMD_resize 95
	{(char_u *)"substitute",	RANGE+EXTRA},
#define CMD_substitute 96
	{(char_u *)"sargument",		BANG+RANGE+NOTADR+COUNT+EXTRA},
#define CMD_sargument 97
	{(char_u *)"sall",			TRLBAR},
#define CMD_sall 98
	{(char_u *)"sbuffer",		RANGE+NOTADR+COUNT+TRLBAR},
#define CMD_sbuffer 99
	{(char_u *)"sball",			TRLBAR},
#define CMD_sball 100
	{(char_u *)"sbmodified",	RANGE+NOTADR+COUNT+TRLBAR},
#define CMD_sbmodified 101
	{(char_u *)"sbnext",		RANGE+NOTADR+COUNT+TRLBAR},
#define CMD_sbnext 102
	{(char_u *)"sbNext",		RANGE+NOTADR+COUNT+TRLBAR},
#define CMD_sbNext 103
	{(char_u *)"sbprevious",	RANGE+NOTADR+COUNT+TRLBAR},
#define CMD_sbprevious 104
	{(char_u *)"sbrewind",		TRLBAR},
#define CMD_sbrewind 105
	{(char_u *)"sblast",		TRLBAR},
#define CMD_sblast 106
	{(char_u *)"suspend",		TRLBAR+BANG},
#define CMD_suspend 107
	{(char_u *)"set",			EXTRA+TRLBAR},
#define CMD_set 108
	{(char_u *)"setkeymap",		NAMEDF+TRLBAR},
#define CMD_setkeymap 109
	{(char_u *)"shell",			TRLBAR},
#define CMD_shell 110
	{(char_u *)"sleep",			RANGE+COUNT+NOTADR+TRLBAR},
#define CMD_sleep 111
	{(char_u *)"source",		NAMEDF+NEEDARG+TRLBAR},
#define CMD_source 112
	{(char_u *)"split",			BANG+FILE1+RANGE+NOTADR},
#define CMD_split 113
	{(char_u *)"snext",			RANGE+NOTADR+BANG+NAMEDFS},
#define CMD_snext 114
	{(char_u *)"sNext",			EXTRA+RANGE+NOTADR+COUNT+BANG},
#define CMD_sNext 115
	{(char_u *)"sprevious",		EXTRA+RANGE+NOTADR+COUNT+BANG},
#define CMD_sprevious 116
	{(char_u *)"srewind",		EXTRA+BANG},
#define CMD_srewind 117
	{(char_u *)"slast",			EXTRA+BANG},
#define CMD_slast 118
	{(char_u *)"stop",			TRLBAR+BANG},
#define CMD_stop 119
	{(char_u *)"sunhide",		TRLBAR},
#define CMD_sunhide 120
	{(char_u *)"swapname",		TRLBAR},
#define CMD_swapname 121
	{(char_u *)"t",				RANGE+EXTRA+TRLBAR},
#define CMD_t 122
	{(char_u *)"tag",			RANGE+NOTADR+COUNT+BANG+WORD1+TRLBAR+ZEROR},
#define CMD_tag 123
	{(char_u *)"tags",			TRLBAR},
#define CMD_tags 124
	{(char_u *)"unabbreviate",	EXTRA+TRLBAR+USECTRLV},
#define CMD_unabbreviate 125
	{(char_u *)"undo",			TRLBAR},
#define CMD_undo 126
	{(char_u *)"unhide",		TRLBAR},
#define CMD_unhide 127
	{(char_u *)"unmap",			BANG+EXTRA+TRLBAR+USECTRLV},
#define CMD_unmap 128
	{(char_u *)"vglobal",		RANGE+EXTRA+DFLALL},
#define CMD_vglobal 129
	{(char_u *)"version",		TRLBAR},
#define CMD_version 130
	{(char_u *)"visual",		RANGE+BANG+FILE1},
#define CMD_visual 131
	{(char_u *)"write",			RANGE+BANG+FILE1+DFLALL+TRLBAR},
#define CMD_write 132
	{(char_u *)"wnext",			RANGE+NOTADR+BANG+FILE1+TRLBAR},
#define CMD_wnext 133
	{(char_u *)"wNext",			RANGE+NOTADR+BANG+FILE1+TRLBAR},
#define CMD_wNext 134
	{(char_u *)"wprevious",		RANGE+NOTADR+BANG+FILE1+TRLBAR},
#define CMD_wprevious 135
	{(char_u *)"winsize",		EXTRA+NEEDARG+TRLBAR},
#define CMD_winsize 136
	{(char_u *)"wq",			BANG+FILE1+DFLALL+TRLBAR},
#define CMD_wq 137
	{(char_u *)"wall",			BANG+TRLBAR},
#define CMD_wall 138
	{(char_u *)"wqall",			BANG+FILE1+DFLALL+TRLBAR},
#define CMD_wqall 139
	{(char_u *)"xit",			BANG+FILE1+DFLALL+TRLBAR},
#define CMD_xit 140
	{(char_u *)"xall",			BANG+TRLBAR},
#define CMD_xall 141
	{(char_u *)"yank",			RANGE+REGSTR+COUNT+TRLBAR},
#define CMD_yank 142
	{(char_u *)"z",				RANGE+COUNT+TRLBAR},		/* not supported */
#define CMD_z 143
	{(char_u *)"syntax",		EXTRA+TRLBAR+USECTRLV},
#define CMD_syntax 144
	{(char_u *)"@",				RANGE+EXTRA+TRLBAR},
#define CMD_at 145
	{(char_u *)"!",				RANGE+NAMEDFS},
#define CMD_bang 146
	{(char_u *)"<",				RANGE+COUNT+TRLBAR},
#define CMD_lshift 147
	{(char_u *)">",				RANGE+COUNT+TRLBAR},
#define CMD_rshift 148
	{(char_u *)"=",				RANGE+TRLBAR},
#define CMD_equal 149
	{(char_u *)"&",				RANGE+EXTRA},
#define CMD_and 150
	{(char_u *)"~",				RANGE+EXTRA}
#define CMD_tilde 151
#define CMD_SIZE 152

};
