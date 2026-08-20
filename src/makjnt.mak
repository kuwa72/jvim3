# vi:ts=8:sw=8
# Makefile for VIM on WINNT, using MS SDK
#
NODEBUG=1
TARGETOS=WIN95
!include <ntwin32.mak>
#CDEBUG=/Zi
#LDEBUG=/DEBUG /PDB:YES 
lcommon=/MAP /MAPINFO:LINES
ccommon=$(CDEBUG) /O2 /Ox /GA /FAcs
optlibs=user32.lib shell32.lib winmm.lib comctl32.lib ole32.lib olepro32.lib uuid.lib

#>>>>> choose options:
### -DDIGRAPHS		digraph support (at the cost of 1.6 Kbyte code)
### -DCOMPATIBLE	start in vi-compatible mode
### -DNOBACKUP		default is no backup file
### -DDEBUG		output a lot of debugging garbage
### -DTERMCAP		include termcap file support
### -DNO_BUILTIN_TCAPS	do not include builtin termcap entries
###				(use only with -DTERMCAP)
### -DSOME_BUILTIN_TCAPS include most useful builtin termcap entries
###				(use only without -DNO_BUILTIN_TCAPS)
### -DALL_BUILTIN_TCAPS	include all builtin termcap entries
###				(use only without -DNO_BUILTIN_TCAPS)
### -DVIMRC_FILE	name of the .vimrc file in current dir
### -DEXRC_FILE		name of the .exrc file in current dir
### -DSYSVIMRC_FILE	name of the global .vimrc file
### -DSYSEXRC_FILE	name of the global .exrc file
### -DDEFVIMRC_FILE	name of the system-wide .vimrc file
### -DVIM_HLP		name of the help file
### -DWEBB_COMPLETE	include Webb's code for command line completion
### -DWEBB_KEYWORD_COMPL include Webb's code for keyword completion
### -DNOTITLE		'title' option off by default

# generic for grep function
#
GRPOPT  = -DUSE_GREP

# generic for grep function
#
TAGOPT  = -DUSE_TAGEX

# generic for option function
#
OPTOPT  = -DUSE_OPT

# The extend file system (USE_EXFILE: editing an archive or an ftp URL in
# place), the BDF font renderer (USE_BDF) and the MIME decoder (USE_MATOME) are
# gone, sources and all. See BUILDING-mingw.md for why.

# generic for syntax highlighting function
#
SYNOPT  = -DSYNTAX
SYNOBJ  = syntax.obj

# generic for file history function
#
HISTOPT = -DUSE_HISTORY

DEFINES	= -DKANJI -DTRACK -DFEPCTRL -DCRMARK -DFEXRC -DNT106KEY -DWEBB_COMPLETE -DWEBB_KEYWORD_COMPL -DNO_FREE_NULL -DXARGS -DTERMCAP -DWIN32 $(GRPOPT) $(TAGOPT) $(OPTOPT) $(SYNOPT) $(HISTOPT)

#>>>>> name of the compiler and linker, name of lib directory
CC	= cl -nologo
LINK	= cl
CFLAGS	= -I. -Iinc -DMSDOS -DNT -DUCODE $(DEFINES) $(DBG) $(cflags) $(cdebug) $(cvarsdll)

#>>>>> end of choices
###########################################################################
INCL =	vim.h globals.h param.h keymap.h macros.h ascii.h msdos.h structs.h
OBJ =	alloc.obj winjnt.obj buffer.obj charset.obj cmdcmds.obj cmdline.obj \
	csearch.obj digraph.obj edit.obj fileio.obj getchar.obj help.obj \
	linefunc.obj main.obj mark.obj memfile.obj memline.obj message.obj \
	misccmds.obj normal.obj ops.obj param.obj quickfix.obj regexp.obj \
	regsub.obj screen.obj search.obj tag.obj term.obj undo.obj window.obj \
	termlib.obj kanji.obj fepnt.obj track.obj xargs.obj \
	$(GRPOBJ) $(OPTOBJ) $(SYNOBJ)

###########################################################################
vim32.exe: $(OBJ) version.obj vim32.res
	$(link) $(linkdebug) $(conlflags) $(lcommon) $(LDEBUG) -out:vim32.exe $(OBJ) version.obj vim32.res $(guilibsdll)
        - del version.obj

vim32w.exe: $(OBJ) version.obj vim32.res
	$(link) $(linkdebug) $(guiflags) $(LDEBUG) -out:vim32w.exe $(OBJ) version.obj vim32.res $(guilibsdll)

vim32s.exe: vim32s\vim32s.c
	- del *.obj
	nmake -f vim32s\mak32s.mak

clip.exe: clip\clip.c
	- del *.obj
	nmake -f clip\clip.mak

all: vim32w.exe vim32.exe grep.exe vim32s.exe clip.exe

clean:
	- del *.obj
	- del *.exe
	- del *.res
	- del *.tr
	- del *.td
	- del *.tr2
	- del *.td2
	- del *.aps
	- del *.o
	- del vim
	- del jptab
	- del mkcmdtab
	- del tags
	- del *.@@@
	- del *.com
	- del *.lzh
	- del *.wup
	- del jvim.cfg
	- del *.pdb
	- del *.map
	- del *.cod
	- del /Q VC\debug\*.*
	- del /Q VC\release\*.*

update:
	@nmake -s -f makjnt.mak clean
	lha32 u -rx d:\usr\backup.src\vim3jp.lzh *.*

wsp:
	wsp -O vim32.exe vim32w.exe vim32.wup
	wsp -S vim32.wup vim32w.com
	- del vim32.wup

lzh:
	nmake -f makjnt.mak wsp
	lha u -rx vim3j21b.lzh vim32.exe vim32w.com vim32s.exe grep.exe clip.exe
	lha u -rx vim3j21b.lzh doc.j\*.* doc.e\*.*
	lha d     vim3j21b.lzh doc.j\readme.doc
	lha d     vim3j21b.lzh doc.j\readme.1st
	lha d     vim3j21b.lzh doc.j\history.txt
	lha d     vim3j21b.lzh doc.j\fj.txt
	lha d     vim3j21b.lzh doc.j\_jvimrc
	lha u     vim3j21b.lzh doc.j\readme.doc
	lha u     vim3j21b.lzh vim32s\vim32s.ini
	lha u     vim3j21b.lzh doc.j\_jvimrc

beta:
	nmake -f makjnt.mak wsp
	lha u     vim3j21b.lzh vim32.exe vim32w.com vim32s.exe grep.exe clip.exe
	lha u     vim3j21b.lzh \usr\com\ctags.exe
	lha u     vim3j21b.lzh doc.j\readme.doc
	lha u -rx vim3j21b.lzh doc.j\history.txt
	lha u     vim3j21b.lzh doc.j\vim.hlp
	lha u -rx vim3j21b.lzh doc.j\vim-jp.htm
	lha u     vim3j21b.lzh doc.j\vim32.ini
	lha u     vim3j21b.lzh vim32s\vim32s.ini
	lha u     vim3j21b.lzh doc.j\_jvimrc
	lha u     vim3j21b.lzh bench\set.reg

betaup:
	nmake -f makjnt.mak wsp
	lha u     vim3j21b.lzh vim32.exe vim32w.com vim32s.exe grep.exe clip.exe
	lha u     vim3j21b.lzh doc.j\readme.doc
	lha u     vim3j21b.lzh doc.j\vim.hlp
	lha u -rx vim3j21b.lzh doc.j\vim-jp.htm
	lha u     vim3j21b.lzh doc.j\vim32.ini
	lha u     vim3j21b.lzh vim32s\vim32s.ini
	lha u     vim3j21b.lzh doc.j\_jvimrc

patch:
	- copy readme.1st doc.j\readme.1st
	- del ..\jvim.diff
	- del jvim.diff
	- del jvim.tar.gz
	- del jvim.diff.bak
	- del readme.1st
	- del readme.1st.bak
	renlow -f ../* ../*/* ../*/*/* ../*/*/*/*
	- del doc.j\history.txt
	- del doc.j\fj.txt
	- copy doc.j\readme.1st ..
	- del doc.j\readme.1st
	- diff -crN ../orig . >> ..\jvim.diff
	copy ..\jvim.diff .
	copy ..\readme.1st .
	crlf jvim.diff readme.1st
	tar32 cvf jvim.tar readme.1st jvim.diff src/vim.ico
	gzip jvim.tar

install:
	nmake -f makjnt.mak all
	copy vim32w.exe \usr\com\vim32.exe
	copy vim32s.exe \usr\com\vim32s.exe
	copy grep.exe \usr\com
	copy clip.exe \usr\com

addcr:	addcr.c
	$(CC) addcr.c
	command /c addcr.bat

###########################################################################
ctags.exe:	ctags/ctags.c xargs.c
	cl -DWIN32 ctags/ctags.c xargs.c

###########################################################################
GREPOBJS = grep.obj alloc.obj charset.obj kanji.obj xargs.obj regexp.obj regsub.obj

grep.exe: $(GREPOBJS) grep.res
	$(link) $(linkdebug) $(conlflags) -out:grep.exe $(GREPOBJS) grep.res $(guilibsdll)
	- del grep.obj

grep.obj: grep/grep.c $(INCL)
	$(CC) -I. $(CFLAGS) -c grep\grep.c

grep.res: grep/grep.rc
	rc /fo grep.res grep/grep.rc

###########################################################################
alloc.obj:	alloc.c  $(INCL)
winjnt.obj:	winjnt.c  $(INCL)
buffer.obj:	buffer.c  $(INCL)
charset.obj:	charset.c  $(INCL)
cmdcmds.obj:	cmdcmds.c  $(INCL)
cmdline.obj:	cmdline.c  $(INCL) cmdtab.h
csearch.obj:	csearch.c  $(INCL)
digraph.obj:	digraph.c  $(INCL)
edit.obj:	edit.c  $(INCL)
fileio.obj:	fileio.c  $(INCL)
getchar.obj:	getchar.c  $(INCL)
help.obj:	help.c  $(INCL)
linefunc.obj:	linefunc.c  $(INCL)
main.obj:	main.c  $(INCL)
mark.obj:	mark.c  $(INCL)
memfile.obj:	memfile.c  $(INCL)
memline.obj:	memline.c  $(INCL)
message.obj:	message.c  $(INCL)
misccmds.obj:	misccmds.c  $(INCL)
normal.obj:	normal.c  $(INCL) ops.h
ops.obj:	ops.c  $(INCL) ops.h
param.obj:	param.c  $(INCL)
quickfix.obj:	quickfix.c  $(INCL)
regexp.obj:	regexp.c  $(INCL)
regsub.obj:	regsub.c  $(INCL)
screen.obj:	screen.c  $(INCL)
search.obj:	search.c  $(INCL)
tag.obj:	tag.c  $(INCL)
term.obj:	term.c  $(INCL) term.h
undo.obj:	undo.c  $(INCL)
window.obj:	window.c  $(INCL)
kanji.obj:	kanji.c  $(INCL) jptab.h
track.obj:	track.c  $(INCL) jptab.h
fepnt.obj:	fepnt.c  $(INCL)
syntax.obj:	syntax.c  $(INCL)
mkcmdtab.exe: 	mkcmdtab.c
jptab.exe: 	jptab.c
version.obj:	version.c  $(INCL)

jptab.h: jptab.c
	$(CC) $(CFLAGS) jptab.c
	jptab.exe > jptab.h

cmdtab.h: cmdtab.tab mkcmdtab.c
	$(CC) $(CFLAGS) mkcmdtab.c
	mkcmdtab cmdtab.tab cmdtab.h

vim32.res: vim32.rc vim.ico
	rc $(DEFINES) vim32.rc
