#!/usr/bin/env bash
#
# Keyboard tests for the Windows builds of JVim.
#
#   scripts/test-winkeys.sh [jvim32.exe] [jvim32w.exe]
#
# The other two suites feed keys with "-s", which is read straight from the file
# and never converted. Everything a real keyboard produces goes the other way,
# through GetChars() and inchar(), and on Windows a special key arrives there as
# K_NUL plus a key code byte -- so the cursor keys, the function keys and CTRL-@
# are exactly what "-s" cannot test. That blind spot is where they broke: the
# code conversion turned K_NUL into '?', and every cursor key inserted "?K"
# instead of moving, in the buffer and on the ":" line alike.
#
# So here the keys are really typed: scripts/feedkeys.c queues console key
# events for the console build, and scripts/guikeys.c posts window messages to
# the GUI build. Both need Windows, which this reaches through WSL.
#
# Neither driver is only good for what is tested below:
#   feedkeys.exe -dump screen.txt keys prog ...    the console screen, as UTF-8
#   guikeys.exe  -shot  window.bmp keys prog ...   the GUI window, as a bitmap
# The GUI paints from its own screen array with ExtTextOutW, so a layout question
# -- whether a column landed where it should -- can only be answered by looking
# at the window.

set -uo pipefail

root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
con=${1:-$root/src/jvim32.exe}
gui=${2:-$root/src/jvim32w.exe}

# COUNT_ONLY=1 prints "cases N" and stops, without needing Windows, a build or
# the cross compiler. scripts/check-docs.sh asks that, from wherever it runs, so
# the case count quoted in the documentation is checked against this file.
count_only=${COUNT_ONLY:-}

if [ -z "$count_only" ]; then
for exe in "$con" "$gui"; do
	if [ ! -f "$exe" ]; then
		echo "no Windows build at $exe" >&2
		echo "build one with: ./scripts/build-mingw.sh both" >&2
		exit 2
	fi
done
fi

if [ -n "$count_only" ]; then
	# Count the cases without any of the Windows preflight below.
	cases=$(grep -cE '^(run|expand_lists|opens_long_name|opens_past_max_path) ' "$0")
	printf 'cases %d\n' "$cases"
	exit 0
fi

# The drivers and the editor have to run from a real Windows directory: a WSL
# path reaches a Win32 child as a UNC name, which cmd.exe refuses to start in.
: "${WINTMP:=/mnt/c/tmp}"
if [ ! -d "$WINTMP" ] && ! mkdir -p "$WINTMP" 2>/dev/null; then
	echo "no Windows directory to work in; set WINTMP to one (default /mnt/c/tmp)" >&2
	exit 2
fi
if ! command -v cmd.exe >/dev/null 2>&1; then
	echo "no cmd.exe: this needs to run under WSL on Windows" >&2
	exit 2
fi

CC_WIN=${CC_WIN:-i686-w64-mingw32-gcc}
if ! command -v "$CC_WIN" >/dev/null 2>&1; then
	echo "no $CC_WIN to build the key drivers with; set CC_WIN" >&2
	exit 2
fi

work=$(mktemp -d "$WINTMP/jvimkeys.XXXXXX")
trap 'rm -rf "$work"' EXIT

for src in feedkeys guikeys; do
	"$CC_WIN" -O2 -o "$work/$src.exe" "$root/scripts/$src.c" -lgdi32 2>"$work/cc.err" || {
		echo "cannot build $root/scripts/$src.c:" >&2
		cat "$work/cc.err" >&2
		exit 2
	}
done
cp "$con" "$work/jvim32.exe"
cp "$gui" "$work/jvim32w.exe"

win() { cmd.exe /c "cd /d $(wslpath -w "$work") && $*" >/dev/null 2>&1; }
hex() { od -An -tx1 -v "$1" 2>/dev/null | tr -d ' \n'; }

pass=0; fail=0

# run <where: console|gui> <name> <keys> <input> <wanted output>
run() {
	local where=$1 name=$2 keys=$3 data=$4 want=$5

	keys=${keys//%/%%}
	data=${data//%/%%}
	want=${want//%/%%}

	printf "$data" > "$work/in"
	printf "$want" > "$work/want"
	rm -f "$work/out"
	printf "$keys:w! out\r:q!\r" > "$work/keys"
	if [ "$where" = console ]; then
		win feedkeys.exe keys jvim32.exe -nw in
	else
		win guikeys.exe keys jvim32w.exe in
	fi

	if cmp -s "$work/want" "$work/out"; then
		printf '  PASS        %s\n' "$name"; pass=$((pass+1))
	else
		printf '  FAIL        %s\n' "$name"
		printf '                want %s\n' "$(hex "$work/want")"
		printf '                got  %s\n' "$(hex "$work/out")"
		fail=$((fail+1))
	fi
}

# expand_lists <name> <wanted in the list>
#
# What the completion of a directory holds, taken from the command line itself
# rather than off the screen: ":!echo <pattern>" plus CTRL-A puts every match on
# the command line, and the shell writes them to a file. The names come out in
# the system code, so only an ASCII one can be checked -- which is the point
# here, since what used to go missing was everything *after* a name the ANSI
# directory API could not return (see find_first_name() in winjnt.c).
expand_lists() {
	local name=$1 wanted=$2

	rm -rf "$work/dir"; mkdir -p "$work/dir"
	: > "$work/dir/a.txt"
	: > "$work/dir/z.txt"
	# a name whose 8.3 form is too long for WIN32_FIND_DATAA in UTF-8, and
	# which sorts between the two
	: > "$work/dir/m_日本語のファイル名.txt"
	rm -f "$work/out"
	printf ':!echo dir\\\001 > out\r:q!\r' > "$work/keys"
	win feedkeys.exe keys jvim32.exe -nw

	if grep -aqF -- "$wanted" "$work/out" 2>/dev/null; then
		printf '  PASS        %s\n' "$name"; pass=$((pass+1))
	else
		printf '  FAIL        %s\n' "$name"
		printf '                wanted %s in the list\n' "$wanted"
		printf '                got    %s\n' "$(cat "$work/out" 2>/dev/null | tr -d '\r\n' | cut -c1-120)"
		fail=$((fail+1))
	fi
}

# opens_long_name <name>
#
# A file whose name runs past 260 bytes once it is in UTF-8 -- 80 Japanese
# characters is 240 of them -- has to be completable *and* openable. MAXPATHL
# counts bytes, so while it was 260 the completion offered the name and ":e"
# then truncated it to a path that does not exist, leaving an empty buffer.
# The prefix typed here is ASCII, so nothing but the completion can produce the
# name.
opens_long_name() {
	local name=$1
	local kana file

	kana=$(python3 -c "print('あ' * 80, end='')" 2>/dev/null) || {
		printf '  SKIP        %s (no python3 to build the name)\n' "$name"; return
	}
	rm -rf "$work/deep"; mkdir -p "$work/deep"
	file="1_$kana.mp3"
	printf 'LONGNAME\n' > "$work/deep/$file" 2>/dev/null || {
		printf '  SKIP        %s (the file system will not take the name)\n' "$name"; return
	}
	rm -f "$work/out"
	printf ':e deep\\1_\t\r:w! out\r:q!\r' > "$work/keys"
	win feedkeys.exe keys jvim32.exe -nw

	if [ "$(cat "$work/out" 2>/dev/null | tr -d '\r\n')" = LONGNAME ]; then
		printf '  PASS        %s\n' "$name"; pass=$((pass+1))
	else
		printf '  FAIL        %s\n' "$name"
		printf '                the completed name did not open (buffer was empty)\n'
		fail=$((fail+1))
	fi
}

# opens_past_max_path <name>
#
# A whole path longer than MAX_PATH, which is 260 *characters*. jvim.manifest
# asks for longPathAware and FullName() goes through GetFullPathNameW, because
# the ANSI call refuses a path that long whatever the manifest says. The
# directories here are ASCII, so only the file name comes from the completion.
#
# Windows has to have long paths turned on for this to be possible at all
# (LongPathsEnabled, off by default), so it says it skipped rather than failing.
opens_past_max_path() {
	local name=$1
	local d i kana enabled

	enabled=$(reg.exe query \
			'HKLM\SYSTEM\CurrentControlSet\Control\FileSystem' \
			/v LongPathsEnabled 2>/dev/null | tr -d '\r' | sed -n 's/.*0x//p')
	if [ "$enabled" != 1 ]; then
		printf '  SKIP        %s (LongPathsEnabled is off here)\n' "$name"
		return
	fi
	kana=$(python3 -c "print('あ' * 8, end='')" 2>/dev/null) || {
		printf '  SKIP        %s (no python3)\n' "$name"; return
	}
	rm -rf "$work/deep2"
	d="$work/deep2"
	for i in 0 1 2 3 4 5 6 7 8 9 10 11; do
		d="$d/dir_${i}_xxxxxxxxxxxxxx"
	done
	mkdir -p "$d" 2>/dev/null || {
		printf '  SKIP        %s (cannot make the tree)\n' "$name"; return
	}
	printf 'DEEPOK\n' > "$d/1_$kana.txt" 2>/dev/null || {
		printf '  SKIP        %s (cannot make the file)\n' "$name"; return
	}
	rm -f "$work/out"
	printf ':e %s\\1_\t\r:w! out\r:q!\r' \
			"$(printf '%s' "${d#$work/}" | tr '/' '\\')" > "$work/keys"
	win feedkeys.exe keys jvim32.exe -nw

	if [ "$(cat "$work/out" 2>/dev/null | tr -d '\r\n')" = DEEPOK ]; then
		printf '  PASS        %s\n' "$name"; pass=$((pass+1))
	else
		printf '  FAIL        %s\n' "$name"
		printf '                the completed path did not open\n'
		fail=$((fail+1))
	fi
}

echo "console: $con"
echo "gui:     $gui"

echo
echo "the console build, driven by console key events:"
run console "cursor right moves"    '<R><R><R><R>x'      'ABCDEFGH\n' 'ABCDFGH\n'
run console "cursor down moves"     '<D>dd'              'a\nb\nc\n'  'a\nc\n'
run console "cursor keys in insert" 'i<R><U><L><D><ESC>' 'ABCDEFGH\n' 'ABCDEFGH\n'
# one step left on the ":" line puts the "x" before the "Z", so what runs is
# ":s/A/xZ" rather than ":s/A/Zx"
run console "cursor keys on : line" ':s/A/Z<L>x<CR>'      'ABC\n'     'xZBC\n'
run console "up recalls a : line"   ':1d\r:<U><CR>'      'a\nb\nc\n'  'c\n'
run console "page up searches back" ':1d\r:1<PGUP><CR>'  'a\nb\nc\n'  'c\n'

echo
echo "the GUI build, driven by window messages:"
run gui "cursor right moves"        '<R><R><R><R>x'      'ABCDEFGH\n' 'ABCDFGH\n'
run gui "cursor down moves"         '<D>dd'              'a\nb\nc\n'  'a\nc\n'
run gui "cursor keys in insert"     'i<R><U><L><D><ESC>' 'ABCDEFGH\n' 'ABCDEFGH\n'
run gui "up recalls a : line"       ':1d\r:<U><CR>'      'a\nb\nc\n'  'c\n'
# a typed character whose UTF-8 ends in 0xa0, the byte K_ZERO uses: the key
# codes may only be picked out where a character starts
run gui "kanji input holding 0xa0"  'i<u30A0><u3042><ESC>' 'X\n' '\xe3\x82\xa0\xe3\x81\x82X\n'

echo
echo "Japanese file names, which are three bytes a character:"
expand_lists "a name after it is listed" 'dir\z.txt'
opens_long_name "a name over 260 bytes opens"
opens_past_max_path "a path over 260 characters opens"

echo
printf 'pass %d  fail %d\n' "$pass" "$fail"
[ "$fail" -eq 0 ] || exit 1
exit 0
