#!/usr/bin/env bash
#
# Runtime tests for the Windows build of JVim, through script input.
#
#   scripts/test-winrun.sh [jvim.exe]
#
# Until this existed, neither Windows executable was ever *run* by anything
# automatic: CI compiled them and the suites tested the same portable sources on
# a Unix. So everything Windows-only -- the console it starts in, the shell it
# calls, the ANSI file APIs it opens names through -- was covered by nothing, and
# ":r !cmd" shipped broken twice.
#
# What this cannot test is keys: script input returns from inchar() before the
# keyboard code conversion, so no "-s" case can reach a cursor key or CTRL-@.
# scripts/test-winkeys.sh does that, by typing for real, and needs a Windows
# machine with a compiler for its key drivers. This needs neither, which is why
# it can run on a CI runner.
#
# It runs in two places, and works out which:
#
#   On Windows, under Git Bash or MSYS -- a GitHub windows-latest runner is
#   here. The executable is started directly.
#
#   Under WSL, which is how it gets run by hand. The executable is started
#   through cmd.exe from a real Windows directory, because a WSL working
#   directory reaches a Win32 child as a UNC name that cmd refuses -- it would
#   silently run in C:\Windows instead, and ":r !" would not be testing what it
#   looks like it is testing. WINTMP says where that directory is.
#
# Either way HOME and VIM point at the work directory, which holds no rc, so
# that a _vimrc belonging to whoever is running this cannot decide what the
# editor does. The shipped sample alone sets textmode and several mappings, and
# every expectation below assumes none of that.

set -uo pipefail

root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
exe=${1:-$root/src/jvim32.exe}

# COUNT_ONLY=1 prints "cases N" and stops, without needing Windows or a build,
# so that check-docs.sh can check the count quoted in the documentation from
# wherever it runs.
count_only=${COUNT_ONLY:-}

pass=0; fail=0; cases=0

if [ -n "$count_only" ]; then
	cases=$(grep -cE '^(run|writes_name) ' "$0")
	printf 'cases %d\n' "$cases"
	exit 0
fi

if [ ! -f "$exe" ]; then
	echo "no Windows build at $exe" >&2
	echo "build one with: ./scripts/build-mingw.sh both" >&2
	echo "or fetch the one CI built: ./scripts/fetch-ci-build.sh" >&2
	exit 2
fi

case $(uname -s) in
MINGW*|MSYS*|CYGWIN*)
	native=1 ;;
*)
	native=
	if ! command -v cmd.exe >/dev/null 2>&1; then
		echo "not on Windows and no cmd.exe: this needs Windows or WSL on it" >&2
		exit 2
	fi
	if ! command -v wslpath >/dev/null 2>&1; then
		echo "no wslpath, so the Windows form of a path cannot be worked out" >&2
		exit 2
	fi ;;
esac

if [ -n "$native" ]; then
	work=$(mktemp -d)
	winpath() { cygpath -w "$1" 2>/dev/null || printf '%s' "$1"; }
	# Directly, from the work directory. HOME and VIM are given in Windows form:
	# the editor hands them to the Windows file APIs, which do not know an MSYS
	# path -- and a name they cannot open is exactly the isolation wanted here.
	win() {
		( cd "$work" && HOME=$(winpath "$work") VIM=$(winpath "$work") \
				./jvim.exe "$@" ) >/dev/null 2>&1
	}
else
	: "${WINTMP:=/mnt/c/tmp}"
	if [ ! -d "$WINTMP" ] && ! mkdir -p "$WINTMP" 2>/dev/null; then
		echo "no Windows directory to work in; set WINTMP to one (default /mnt/c/tmp)" >&2
		exit 2
	fi
	work=$(mktemp -d "$WINTMP/jvimrun.XXXXXX")
	winpath() { wslpath -w "$1"; }
	# No space before the "&&": cmd would put it in the value.
	win() {
		cmd.exe /c "set HOME=$(winpath "$work")&& set VIM=$(winpath "$work")&& \
cd /d $(winpath "$work") && jvim.exe $*" >/dev/null 2>&1
	}
fi
trap 'rm -rf "$work"' EXIT
cp "$exe" "$work/jvim.exe"

hex() { od -An -tx1 -v "$1" 2>/dev/null | tr -d ' \n'; }

# run <name> <keys> <input> <wanted output>
#
# The keys go in through -s and have to end by writing "out" and quitting; that
# part is appended, so a case only writes what it does. -nw keeps the console
# build in the console: jvim32.exe opens a window without it, GuiWin defaulting
# to 'w' in globals.h, and a window on a runner with no desktop session is not
# what is wanted here.
run() {
	local name=$1 keys=$2 data=$3 want=$4

	cases=$((cases+1))
	keys=${keys//%/%%}
	data=${data//%/%%}
	want=${want//%/%%}

	printf "$data" > "$work/in"
	printf "$want" > "$work/want"
	rm -f "$work/out"
	printf "$keys:w! out\r:q!\r" > "$work/keys"
	win -nw -s keys in

	if cmp -s "$work/want" "$work/out"; then
		printf '  PASS        %s\n' "$name"; pass=$((pass+1))
	else
		printf '  FAIL        %s\n' "$name"
		printf '                want %s\n' "$(hex "$work/want")"
		printf '                got  %s\n' "$(hex "$work/out")"
		fail=$((fail+1))
	fi
}

# writes_name <name> <file name bytes>
#
# ":w" to a name that is not ASCII. The Windows build reaches the file system
# through the ANSI APIs with a UTF-8 code page, which is where Japanese names
# have gone wrong before -- and a name is not something a Unix run of the same
# sources can check, since it is the Windows API doing the work.
writes_name() {
	local name=$1 fname=$2

	cases=$((cases+1))
	printf 'abc\n' > "$work/in"
	printf ":w! $fname\r:q!\r" > "$work/keys"
	win -nw -s keys in

	# printf again rather than reusing the expansion: the name has to be
	# compared as the bytes the file system reports.
	local want
	want=$(printf "$fname")
	if [ -f "$work/$want" ]; then
		printf '  PASS        %s\n' "$name"; pass=$((pass+1))
	else
		printf '  FAIL        %s\n' "$name"
		printf '                wanted a file called %s\n' "$want"
		printf '                got %s\n' "$(ls -b "$work" | tr '\n' ' ')"
		fail=$((fail+1))
	fi
}

echo "jvim: $exe"
printf 'started %s\n' "$( [ -n "$native" ] && echo 'directly, on Windows' || echo 'through cmd.exe, from WSL' )"
echo
echo "the Windows executable, driven by a script:"

# It runs at all, and the ordinary operators do what they do everywhere. If the
# executable is broken, this is what says so first.
run 'x removes a character'   'x'          'abc\ndef\n' 'bc\ndef\n'
run 'dd removes a line'       'dd'         'abc\ndef\n' 'def\n'
run ':s substitutes'          ':s/b/X/\r'  'abc\n'      'aXc\n'

# ":r !" runs the shell, which on Windows is cmd.exe, started in the editor's
# working directory. Broken twice in shipped releases, both times because the
# directory it started in was not the one it looked like: nothing in CI ran it.
run ':r ! reads what a command wrote' ':r !echo hi\r' 'abc\n' 'abc\nhi\n'

# The editor's own encoding, through the Windows file paths rather than the Unix
# ones. Byte for byte, the same as the Unix suites ask for.
run 'UTF-8 content round trips' '' \
	'\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\n' \
	'\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\n'

writes_name 'a Japanese file name can be written' \
	'\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e.txt'

echo
printf 'cases %d\n' "$cases"
printf 'pass %d  fail %d\n' "$pass" "$fail"
[ "$fail" -eq 0 ] || exit 1
exit 0
