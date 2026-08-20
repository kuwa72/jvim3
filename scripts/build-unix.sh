#!/bin/sh
#
# Build JVim 3 on a Unix: Linux, FreeBSD, NetBSD, OpenBSD, macOS.
#
#   scripts/build-unix.sh              build src/jvim3
#   scripts/build-unix.sh clean
#   scripts/build-unix.sh test         build, then run the encoding tests
#
# src/makjunix.mak still expects you to uncomment three lines by hand for your
# machine. This works out the same answers by asking the compiler, and hands
# them to that makefile on the command line, so the makefile stays untouched.
#
# Override anything it decides through the environment, e.g.
#   CC=clang EXTRA_CFLAGS=-I/opt/local/include scripts/build-unix.sh
#
# POSIX sh on purpose: /bin/sh on the BSDs is not bash.

set -u

target=${1:-all}
root=$(cd "$(dirname "$0")/.." && pwd)
src=$root/src
tmp=${TMPDIR:-/tmp}/jvim-conf.$$
trap 'rm -f "$tmp".*' 0 1 2 3 15

CC=${CC:-cc}
EXTRA_CFLAGS=${EXTRA_CFLAGS:-}
EXTRA_LIBS=${EXTRA_LIBS:-}
PREFIX=${PREFIX:-/usr/local}

# --- little configure ------------------------------------------------------
# try_cc <flags> <source>   : does this compile?
try_cc() {
	_flags=$1
	shift
	cat > "$tmp".c <<EOF
$*
EOF
	$CC $_flags $EXTRA_CFLAGS -c "$tmp".c -o "$tmp".o 2>/dev/null
}

# try_link <libs> <source>  : does this link?
try_link() {
	_libs=$1
	shift
	cat > "$tmp".c <<EOF
$*
EOF
	$CC $EXTRA_CFLAGS "$tmp".c -o "$tmp".exe $_libs $EXTRA_LIBS 2>/dev/null
}

say() { printf '  %-22s %s\n' "$1" "$2"; }

echo "configuring for $(uname -s) $(uname -r), $CC"

# The sources use K&R function definitions throughout, which C23 removed; gcc 15
# and later default to it. Pin the dialect to something that still accepts them.
std=
for s in gnu89 gnu17 c89; do
	if try_cc "-std=$s" 'int main(){return 0;}'; then
		std="-std=$s"
		break
	fi
done
say "dialect" "${std:-(compiler default)}"

# -fcommon: the sources declare the same global in several files, which became
# an error in gcc 10.
fcommon=
try_cc "-fcommon" 'int main(){return 0;}' && fcommon=-fcommon
say "tentative globals" "${fcommon:-not needed}"

case $(uname -s) in
Linux|GNU*)			machine="-DBSD_UNIX" ;;
FreeBSD|NetBSD|OpenBSD|DragonFly)	machine="-DBSD_UNIX" ;;
Darwin)				machine="-DBSD_UNIX" ;;
SunOS)				machine="-DSYSV_UNIX -DSOLARIS -DTERMINFO" ;;
AIX)				machine="-DSYSV_UNIX -DAIX" ;;
*)					machine="-DBSD_UNIX" ;;
esac
say "machine" "$machine"

# setlocale(), so that LANG picks the kanji codes; see set_init() in param.c.
if try_cc "$std" '#include <locale.h>
int main(){ setlocale(LC_CTYPE, ""); return 0; }'; then
	machine="$machine -DUSE_LOCALE"
	say "setlocale" "yes"
else
	say "setlocale" "no"
fi

# mkstemp(), for the temp files ":!" and wildcard expansion make. Without it
# they fall back to mktemp(), which only picks a name.
if try_link "" '#include <stdlib.h>
int main(){ char t[] = "/tmp/jvimXXXXXX"; return mkstemp(t) < 0; }'; then
	machine="$machine -DHAVE_MKSTEMP"
	say "mkstemp" "yes"
else
	say "mkstemp" "no, falling back to mktemp"
fi

# A terminfo/termcap library, for the real terminal database rather than the
# handful of entries compiled in.
termlib=
for l in -ltinfo -lncursesw -lncurses -lcurses -ltermlib -ltermcap; do
	if try_link "$l" 'int tgetent(char *, char *); int main(){ return tgetent(0,0); }'; then
		termlib=$l
		break
	fi
done
if [ -n "$termlib" ] && try_cc "$std" '#include <termcap.h>
int main(){return 0;}'; then
	tcaps="-DTERMCAP -DSOME_BUILTIN_TCAPS"
elif [ -n "$termlib" ] && try_cc "$std" '#include <curses.h>
#include <term.h>
int main(){return 0;}'; then
	tcaps="-DTERMCAP -DSOME_BUILTIN_TCAPS"
else
	# No library or no header: fall back to the built-in terminal entries.
	termlib=
	tcaps="-DALL_BUILTIN_TCAPS"
fi
say "terminal" "${tcaps} ${termlib:-(no library)}"

# Saving and restoring the xterm title needs X11.
x11=
if try_cc "$std" '#include <X11/Xlib.h>
int main(){return 0;}' && try_link "-lX11" '#include <X11/Xlib.h>
int main(){ XOpenDisplay(0); return 0; }'; then
	machine="$machine -DUSE_X11"
	x11=-lX11
	say "X11 title" "yes"
else
	say "X11 title" "no"
fi

libs="$termlib $x11 $EXTRA_LIBS"

defs="-DDIGRAPHS $tcaps -DNO_FREE_NULL -DVIM_ISSPACE \
 -DWEBB_COMPLETE -DWEBB_KEYWORD_COMPL \
 -DVIM_HLP=\\\"$PREFIX/lib/jvim3.hlp\\\" \
 -DDEFVIMRC_FILE=\\\"$PREFIX/etc/jvim3rc\\\" \
 -DKANJI -DUCODE -DTRACK -DCRMARK -DFEXRC -DUSE_GREP -DUSE_TAGEX -DUSE_OPT"

OPT=${OPT:--O2 -g}
cflags="$OPT $std $fcommon $EXTRA_CFLAGS"

echo
case $target in
clean)
	(cd "$src" && ${MAKE:-make} -f makjunix.mak clean MACHINE="$machine" \
		LIBS="$libs" CC="$CC $cflags" DEFS="$defs" \
		TERMLIB= FEPOBJS= FEPLIBS=)
	rm -f "$src/jvim3"
	echo "cleaned"
	exit 0
	;;
all|test)
	;;
*)
	echo "usage: $0 [all|test|clean]" >&2
	exit 2
	;;
esac

# cd rather than "make -C": not every make has that option.
(cd "$src" && ${MAKE:-make} -f makjunix.mak jvim3 \
	MACHINE="$machine" \
	LIBS="$libs" \
	CC="$CC $cflags" \
	DEFS="$defs" \
	TERMLIB= FEPOPT= FEPOBJS= FEPLIBS=) || exit 1

echo
echo "built $src/jvim3"
"$src/jvim3" -h 2>&1 | head -2 || true

if [ "$target" = test ]; then
	echo
	"$root/scripts/test-encoding.sh" "$src/jvim3"
fi
