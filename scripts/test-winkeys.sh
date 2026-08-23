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
#
# None of it appears on your screen. The editors are real ones with real
# windows, but both drivers put themselves on a desktop of their own before
# starting anything, so nothing here takes the keyboard of whoever is running
# it. See scripts/windesk.h; WINDESK_OFF=1 turns it off.

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
	cases=$(grep -cE '^(run|expand_lists|opens_long_name|opens_past_max_path|colours_a_file|draws_a_background) ' "$0")
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

# HOME and VIM point at the work directory, which has no rc in it, so that a
# _vimrc in whoever is running this cannot decide what the editor does. The
# shipped sample alone sets textmode, several mappings and a syntax rule set,
# and every case below assumes none of that. No space before the "&&": cmd
# would put it in the value.
winenv="set HOME=$(wslpath -w "$work")&& set VIM=$(wslpath -w "$work")&&"
win() { cmd.exe /c "$winenv cd /d $(wslpath -w "$work") && $*" >/dev/null 2>&1; }
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

# colours_a_file <name>
#
# Whether the syntax engine runs at all in the Windows GUI build. SYN_ON() is
# 'syntax' *and* GuiWin there, so the console build beside it colours nothing
# however the rules are written, and the one suite that reads colouring back --
# scripts/test-syntax.sh, through ":syntax dump" -- needs a pty and runs on a
# Unix. What each rule matches is that suite's business; this asks the single
# question it cannot: does any of it happen here.
#
# Its own directory, because HOME for the cases above deliberately holds no rc
# and this one needs one. The dump is opened in text mode, so the lines come
# back with CRLF.
colours_a_file() {
	local name=$1
	local w="$work/syn" want

	rm -rf "$w"; mkdir -p "$w"
	cp -R "$root/syntax" "$w/syntax"
	cp "$work/guikeys.exe" "$work/jvim32w.exe" "$w/"
	printf 'set fexrc\r\nset syntax\r\nsource $VIM/syntax/filetype.jvsyn\r\n' \
			> "$w/_jvimrc"
	printf 'def f(n):\r\n    return None\r\n' > "$w/t.py"
	printf ':syntax dump out\r:q!\r' > "$w/keys"
	cmd.exe /c "set HOME=$(wslpath -w "$w")&& set VIM=$(wslpath -w "$w")&& \
			cd /d $(wslpath -w "$w") && guikeys.exe keys jvim32w.exe t.py" \
			>/dev/null 2>&1
	want='1:0-3 Statement w/def
2:4-10 Statement w/return
2:11-15 Constant w/None'

	if [ "$(tr -d '\r' < "$w/out" 2>/dev/null)" = "$want" ]; then
		printf '  PASS        %s\n' "$name"; pass=$((pass+1))
	else
		printf '  FAIL        %s\n' "$name"
		printf '                want %s\n' "$(printf '%s' "$want" | tr '\n' '|')"
		printf '                got  %s\n' \
			"$(tr -d '\r' < "$w/out" 2>/dev/null | tr '\n' '|')"
		fail=$((fail+1))
	fi
}

# draws_a_background <name>
#
# The one thing only a picture can answer. ":syntax dump" says which rule
# coloured which bytes and scripts/test-sgr.sh says what a terminal is sent,
# but the GUI paints from its own screen array with FillRect and ExtTextOutW,
# and neither of those leaves a trace anywhere a test can read. So take the
# window as a bitmap and look at it.
#
# diff.jvsyn draws an added line on #e6ffe6 and a removed one on #ffe6e6. A
# 24 bit BMP holds a pixel as blue, green, red, so those are "e6ffe6" and
# "e6e6ff" in the file. Twenty of either in a row is two and a half characters
# of one of them and cannot be anything else on a white window.
draws_a_background() {
	local name=$1
	local w="$work/bg" h ok=1 c pat

	rm -rf "$w"; mkdir -p "$w"
	cp -R "$root/syntax" "$w/syntax"
	cp "$work/guikeys.exe" "$work/jvim32w.exe" "$w/"
	printf 'set fexrc\r\nset syntax\r\nsource $VIM/syntax/filetype.jvsyn\r\n' \
			> "$w/_jvimrc"
	printf -- '-gone\r\n+added\r\n' > "$w/t.diff"
	printf ':\r' > "$w/keys"
	cmd.exe /c "set HOME=$(wslpath -w "$w")&& set VIM=$(wslpath -w "$w")&& \
			cd /d $(wslpath -w "$w") && \
			guikeys.exe -shot shot.bmp keys jvim32w.exe t.diff" \
			>/dev/null 2>&1
	h=$(hex "$w/shot.bmp")
	for c in e6e6ff e6ffe6; do
		pat=$(printf "$c%.0s" $(seq 1 20))
		printf '%s' "$h" | grep -q "$pat" || ok=0
	done

	if [ "$ok" = 1 ]; then
		printf '  PASS        %s\n' "$name"; pass=$((pass+1))
	else
		printf '  FAIL        %s\n' "$name"
		printf '                no run of either tint in the window\n'
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
colours_a_file "the rules colour a file"
draws_a_background "a rule draws on a colour"

echo
echo "Japanese file names, which are three bytes a character:"
expand_lists "a name after it is listed" 'dir\z.txt'
opens_long_name "a name over 260 bytes opens"
opens_past_max_path "a path over 260 characters opens"

echo
printf 'pass %d  fail %d\n' "$pass" "$fail"
[ "$fail" -eq 0 ] || exit 1
exit 0
