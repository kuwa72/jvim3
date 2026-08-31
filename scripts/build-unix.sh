#!/bin/sh
#
# Build JVim 3 on a Unix: Linux, FreeBSD, NetBSD, OpenBSD, macOS.
#
#   scripts/build-unix.sh              build src/jvim3
#   scripts/build-unix.sh clean
#   scripts/build-unix.sh test         build, then run the test suites
#   scripts/build-unix.sh strict       build with the warnings CI refuses
#   scripts/build-unix.sh asan         build with AddressSanitizer, then test
#   scripts/build-unix.sh ubsan        build with UndefinedBehaviorSanitizer, then test
#   scripts/build-unix.sh install      build and install to PREFIX (default: /usr/local)
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

# asan and ubsan are the test target with a sanitizer switched on. They are
# here, rather than in a line of environment variables typed from memory, so
# that what CI runs and what anybody can run before a push are the same command.
#
# The flags go into EXTRA_CFLAGS and EXTRA_LIBS and not into OPT, because the
# little configure below compiles and links its probes with those two: a probe
# built unlike the editor answers a question nobody asked. -O1 rather than -O2
# because a sanitizer report is only useful if the frame it names is real.
sanitize=
case $target in
asan)	sanitize=address ;;
ubsan)	sanitize=undefined ;;
esac
if [ -n "$sanitize" ]; then
	OPT=${OPT:--O1 -g}
	EXTRA_CFLAGS="-fsanitize=$sanitize -fno-omit-frame-pointer $EXTRA_CFLAGS"
	EXTRA_LIBS="-fsanitize=$sanitize $EXTRA_LIBS"
	target=test
fi

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

# The compiler's own default. This used to be pinned to -std=gnu89 because the
# sources were full of K&R function definitions, which C23 removed and gcc 15
# defaults to rejecting; they are all prototypes now. Put -std=gnu89 back
# through EXTRA_CFLAGS if some compiler needs it.
std=
say "dialect" "${std:-(compiler default)}"

# -fcommon: the sources declare the same global in several files, which became
# an error in gcc 10.
fcommon=
try_cc "-fcommon" 'int main(){return 0;}' && fcommon=-fcommon
say "tentative globals" "${fcommon:-not needed}"

# BSD4_4 picks the <termios.h> path in unix.c. Without it the BSDs and macOS
# take the <sgtty.h> one, which is compatibility cruft that macOS in particular
# is no longer a safe bet for.
case $(uname -s) in
Linux|GNU*)			machine="-DBSD_UNIX" ;;
FreeBSD|NetBSD|OpenBSD|DragonFly)	machine="-DBSD_UNIX -DBSD4_4" ;;
Darwin)				machine="-DBSD_UNIX -DBSD4_4" ;;
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

# The release number this tree calls itself, from VERSION at the top of it.
# Read with "read" rather than sed or cut: this script is POSIX sh and assumes
# no tools, and read strips the newline by itself.
jvim_version=
[ -f "$root/VERSION" ] && read jvim_version < "$root/VERSION"
case $jvim_version in
''|*[!0-9.]*)
	echo "$root/VERSION should hold one line like 1.0.0, not '$jvim_version'" >&2
	exit 2
	;;
esac

# Which commit this is, when there is one to ask. A release tarball has no git
# and then says nothing, which is honest. Not "git describe --tags": the CI
# checkout is shallow and has no tags, so describe fails there and anything
# built on it misfires quietly.
jvim_build=
if command -v git >/dev/null 2>&1 &&
   git -C "$root" rev-parse --git-dir >/dev/null 2>&1; then
	# No brackets or other shell metacharacters in here: this value travels
	# through DEFS= into the make recipe's own /bin/sh, which would try to
	# interpret them. version.c puts the brackets round it instead.
	jvim_build=$(git -C "$root" rev-parse --short=8 HEAD 2>/dev/null) || jvim_build=
	if [ -n "$jvim_build" ] &&
	   [ -n "$(git -C "$root" status --porcelain 2>/dev/null)" ]; then
		jvim_build=$jvim_build-dirty
	fi
fi
say "version" "$jvim_version${jvim_build:+ $jvim_build}"

defs="-DDIGRAPHS $tcaps -DNO_FREE_NULL -DVIM_ISSPACE \
 -DWEBB_COMPLETE -DWEBB_KEYWORD_COMPL \
 -DVIM_HLP=\\\"$PREFIX/lib/jvim3.hlp\\\" \
 -DDEFVIMRC_FILE=\\\"$PREFIX/etc/jvim3rc\\\" \
 -DVIMDIR=\\\"$PREFIX/lib/jvim3\\\" \
 -DKANJI -DUCODE -DTRACK -DCRMARK -DFEXRC -DUSE_GREP -DUSE_TAGEX -DUSE_OPT \
 -DSYNTAX"

# The \\\" is the same form VIM_HLP above uses: it collapses to \" inside these
# double quotes, survives DEFS= through make into the recipe's own shell, and
# reaches the compiler as -DJVIM_VERSION="1.0.0".
defs="$defs -DJVIM_VERSION=\\\"$jvim_version\\\""
[ -z "$jvim_build" ] || defs="$defs -DJVIM_BUILDID=\\\"$jvim_build\\\""

OPT=${OPT:--O2 -g}
# The classes of warning that have bitten this codebase, as errors. This is the
# same list as the "warnings that have to stay away" job in
# .github/workflows/build.yml, here so that it can be run before the push rather
# than after it: it is what caught 'A' sliding into the wrong field of a struct
# after a member was added above it, which gcc only warns about and clang, on
# FreeBSD, refuses outright. The char/char_u signedness warnings stay warnings.
if [ "$target" = strict ]; then
	EXTRA_CFLAGS="-Wall -Wno-pointer-sign \
		-Werror=implicit-function-declaration -Werror=implicit-int \
		-Werror=incompatible-pointer-types -Werror=int-conversion \
		-Werror=strict-prototypes -Werror=old-style-definition \
		-Werror=return-type -Werror=uninitialized $EXTRA_CFLAGS"
fi
cflags="$OPT $std $fcommon $EXTRA_CFLAGS"
stamp_file="$src/.build-stamp"
config_sig="CC=$CC cflags=$cflags defs=$defs PREFIX=$PREFIX machine=$machine libs=$libs"

echo
case $target in
clean)
	(cd "$src" && ${MAKE:-make} -f makjunix.mak clean MACHINE="$machine" \
		LIBS="$libs" CC="$CC $cflags" DEFS="$defs" \
		TERMLIB= FEPOBJS= FEPLIBS=)
	rm -f "$src/jvim3" "$stamp_file"
	echo "cleaned"
	exit 0
	;;
all|test|strict|install)
	if [ -f "$stamp_file" ]; then
		old_sig=$(cat "$stamp_file" 2>/dev/null || true)
		if [ "$old_sig" != "$config_sig" ]; then
			echo "build configuration changed; cleaning previous objects..."
			(cd "$src" && ${MAKE:-make} -f makjunix.mak clean MACHINE="$machine" \
				LIBS="$libs" CC="$CC $cflags" DEFS="$defs" \
				TERMLIB= FEPOBJS= FEPLIBS=)
			rm -f "$src/jvim3" "$stamp_file"
		fi
	fi
	;;
*)
	echo "usage: $0 [all|test|strict|asan|ubsan|install|clean]" >&2
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

printf '%s\n' "$config_sig" > "$stamp_file"

echo
echo "built $src/jvim3"
"$src/jvim3" -h 2>&1 | head -2 || true

if [ "$target" = install ]; then
	echo
	echo "installing to $PREFIX..."
	(cd "$src" && ${MAKE:-make} -f makjunix.mak install \
		PREFIX="$PREFIX" \
		MACHINE="$machine" \
		LIBS="$libs" \
		CC="$CC $cflags" \
		DEFS="$defs" \
		TERMLIB= FEPOPT= FEPOBJS= FEPLIBS=) || exit 1
	echo
	echo "installed successfully to $PREFIX"
	exit 0
fi

if [ "$target" = test ]; then
	rc=0
	if [ -n "$sanitize" ]; then
		# Every suite sends the editor's stderr to /dev/null -- they compare
		# the bytes it writes, not what it says -- so a sanitizer report would
		# go the same way and the run would look clean. log_path writes each
		# report to a file instead, one per process, and they are collected
		# below. detect_leaks=0 because this editor exits without freeing, on
		# purpose, and LeakSanitizer would bury the real findings.
		sanlog=${TMPDIR:-/tmp}/jvim-$sanitize.$$
		rm -rf "$sanlog"
		mkdir -p "$sanlog" || exit 1
		ASAN_OPTIONS="detect_leaks=0:log_path=$sanlog/report"
		UBSAN_OPTIONS="print_stacktrace=1:log_path=$sanlog/report"
		export ASAN_OPTIONS UBSAN_OPTIONS
	fi
	echo
	"$root/scripts/test-encoding.sh" "$src/jvim3" || rc=1
	echo
	"$root/scripts/test-editing.sh" "$src/jvim3" || rc=1
	echo
	"$root/scripts/test-syntax.sh" "$src/jvim3" || rc=1
	echo
	"$root/scripts/test-sgr.sh" "$src/jvim3" || rc=1
	echo
	"$root/scripts/test-hostile.sh" "$src/jvim3" || rc=1
	if [ -n "$sanitize" ]; then
		echo
		if [ -n "$(find "$sanlog" -name 'report.*' -print -quit)" ]; then
			echo "$sanitize reported:"
			cat "$sanlog"/report.*
			rc=1
		else
			echo "$sanitize: nothing reported"
		fi
		rm -rf "$sanlog"
	fi
	exit $rc
fi
