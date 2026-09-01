#!/usr/bin/env bash
#
# Encoding round trip tests for JVim.
#
#   scripts/test-encoding.sh [jvim-binary]
#
# Each case writes a file, opens it, saves it unchanged and compares the bytes.
# A correct editor round trips every case. This is the acceptance harness for
# moving the internal representation to UTF-8: cases marked KNOWN-FAIL are the
# ones the Shift-JIS internal representation cannot do, and they are expected to
# flip to PASS as that work lands.
#
# Needs a Unix build (src/makjunix.mak); the Windows build cannot be driven from
# here. jvim is given a terminal by scripts/ptyrun.c, which is compiled below.

set -uo pipefail

root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
jvim=${1:-$root/src/jvim3}

# COUNT_ONLY=1 prints "cases N" and stops: how many cases this file holds, with
# no binary, no compiler and no run. scripts/check-docs.sh asks that so the
# counts quoted in the documentation are checked against the suites themselves
# rather than against a number somebody typed.
count_only=${COUNT_ONLY:-}

if [ -z "$count_only" ] && [ ! -x "$jvim" ]; then
	echo "no jvim binary at $jvim" >&2
	echo "build one with: ./scripts/build-unix.sh" >&2
	exit 2
fi

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
printf ':w! %s/out\r:q!\r' "$tmp" > "$tmp/cmds"

# jvim needs a terminal. script(1) was used for that, but it is a different
# program on every system and NetBSD's quits before the command has run when
# its own standard input is a file, so scripts/ptyrun.c does it instead.
if [ -z "$count_only" ]; then
	${CC:-cc} -o "$tmp/ptyrun" "$root/scripts/ptyrun.c" 2>"$tmp/cc.err" || {
		echo "cannot build $root/scripts/ptyrun.c, which gives jvim a pty:" >&2
		cat "$tmp/cc.err" >&2
		exit 2
	}
fi
# HOME is the temporary directory, which has no rc in it: a ~/.vimrc belonging
# to whoever is running this would otherwise decide what the editor does, and
# every case below assumes the defaults.
pty() { "$tmp/ptyrun" /bin/sh -c "HOME=$tmp $1"; }

# xxd is vim's own, so it is not on a machine that has no vim yet; od is POSIX.
hex() { od -An -tx1 -v "$1" 2>/dev/null | tr -d ' \n'; }

pass=0; fail=0; xfail=0; xpass=0; cases=0

# Counted in both modes, so "cases" is the number of cases in this file whether
# or not they were run, and stays right when a KNOWN-FAIL is added back.
counted() {
	cases=$((cases+1))
	[ -n "$count_only" ]
}

# roundtrip <name> <expect: ok|knownfail> <jvim options> <printf-escaped bytes>
roundtrip() {
	counted && return
	local name=$1 expect=$2 opts=$3 data=$4
	printf "$data" > "$tmp/in"
	cp "$tmp/in" "$tmp/want"		# the file itself is the expectation
	rm -f "$tmp/out"
	pty "TERM=xterm $jvim -T xterm $opts -s $tmp/cmds $tmp/in" >/dev/null 2>&1
	verdict "$name" "$expect"
}

# $tmp/want against $tmp/out, shared by every helper here. The hex dump on
# failure is the only useful thing to print for a file of bytes.
verdict() {
	local name=$1 expect=$2
	local got
	if cmp -s "$tmp/want" "$tmp/out"; then got=ok; else got=bad; fi

	if [ "$expect" = ok ] && [ "$got" = ok ]; then
		printf '  PASS        %s\n' "$name"; pass=$((pass+1))
	elif [ "$expect" = ok ]; then
		printf '  FAIL        %s\n' "$name"
		printf '                want %s\n' "$(hex "$tmp/want")"
		printf '                got  %s\n' "$(hex "$tmp/out")"
		fail=$((fail+1))
	elif [ "$got" = bad ]; then
		printf '  KNOWN-FAIL  %s\n' "$name"; xfail=$((xfail+1))
	else
		printf '  NOW PASSES  %s  <- update the expectation\n' "$name"
		xpass=$((xpass+1))
	fi
}

# helpscreen <name> <expect> <help file bytes> <expected bytes on the screen>
#
# 'helpfile' is the one file the editor converts without loading it into a
# buffer: help() reads it whole, converts it to the internal UTF-8 in one go and
# paints it (kopen() in src/help.c). Nothing it does reaches a buffer, so the
# terminal is the only place the answer shows up, and RETURN leaves the screen
# again so the editor can quit normally.
#
# The file here is written in ISO-2022-JP, and long enough in one run of kanji
# that the conversion grows it: two bytes a character become three, while the
# two escape sequences go away entirely. That direction is the whole case. The
# buffer for the converted text used to be the size of the file, so every help
# file that was not already UTF-8 failed to convert and ":help" drew an empty
# screen -- and the failure wrote a NUL in front of the buffer on its way out.
# doc.j/vim.hlp is UTF-8 now and needs no conversion at all, which is exactly
# why this case writes its own file rather than using it.
helpscreen() {
	counted && return
	local name=$1 expect=$2 data=$3 want=$4

	printf "$data" > "$tmp/hlp"
	printf "$want" > "$tmp/want"
	printf 'a\n' > "$tmp/in"
	printf ':set helpfile=%s/hlp\r:help\r\r:q!\r' "$tmp" > "$tmp/helpcmds"
	rm -f "$tmp/screen"
	pty "TERM=xterm $jvim -T xterm -s $tmp/helpcmds $tmp/in" > "$tmp/screen" 2>&1

	local got=bad
	grep -qF -f "$tmp/want" "$tmp/screen" && got=ok

	if [ "$expect" = ok ] && [ "$got" = ok ]; then
		printf '  PASS        %s\n' "$name"; pass=$((pass+1))
	elif [ "$expect" = ok ]; then
		printf '  FAIL        %s\n' "$name"
		printf '                %s is not on the screen\n' "$(hex "$tmp/want")"
		fail=$((fail+1))
	elif [ "$got" = bad ]; then
		printf '  KNOWN-FAIL  %s\n' "$name"; xfail=$((xfail+1))
	else
		printf '  NOW PASSES  %s  <- update the expectation\n' "$name"
		xpass=$((xpass+1))
	fi
}

# detects <name> <expect> <input bytes> <expected bytes>
#
# What judge_jcode() decided, read off the bytes that come back: no -k, so
# autodetection runs, and a UTF-8 file taken for Shift-JIS comes back as
# mojibake rather than as itself. roundtrip() above is the same thing where the
# answer is "unchanged"; this is for the cases where a byte is expected to be
# lost and everything else is expected to survive.
detects() {
	counted && return
	local name=$1 expect=$2 data=$3 want=$4
	printf "$data" > "$tmp/in"
	printf "$want" > "$tmp/want"
	rm -f "$tmp/out"
	pty "TERM=xterm $jvim -T xterm -s $tmp/cmds $tmp/in" >/dev/null 2>&1
	verdict "$name" "$expect"
}

ASCII='abc\n'
NIHONGO='\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\n'		# 日本語  JIS X 0208
MARU1='\xe2\x91\xa0\n'								# ①      in CP932
HANGUL='\xed\x95\x9c\n'								# 한      BMP, not in CP932
EMOJI='\xf0\x9f\x98\x80\n'							# 😀      non-BMP
ACCENT='\xc3\xa9\n'									# é       Latin-1
EUC_NIHONGO='\xc6\xfc\xcb\xdc\xb8\xec\n'
SJIS_NIHONGO='\x93\xfa\x96\x7b\x8c\xea\n'			# 日本語 again; 8c ea, not ec

echo "jvim: $jvim"
echo
echo "legacy encodings (must keep working):"
roundtrip "EUC-JP"                   ok  "-k E" "$ASCII$EUC_NIHONGO"
roundtrip "Shift-JIS"                ok  "-k S" "$ASCII$SJIS_NIHONGO"
roundtrip "ASCII only"               ok  ""     "$ASCII"

echo
echo "UTF-8, characters that exist in CP932:"
roundtrip "UTF-8 kanji"              ok  "-k T" "$ASCII$NIHONGO"
roundtrip "UTF-8 circled digit"      ok  "-k T" "$ASCII$NIHONGO$MARU1"

echo
echo "UTF-8, characters outside CP932 (needs an internal UTF-8 buffer):"
roundtrip "UTF-8 hangul (BMP)"       ok        "-k T" "$ASCII$NIHONGO$HANGUL"
roundtrip "UTF-8 accented latin"     ok        "-k T" "$ASCII$ACCENT"
roundtrip "UTF-8 emoji (non-BMP)"    ok        "-k T" "$ASCII$NIHONGO$EMOJI"
roundtrip "UTF-8 emoji, autodetect"  ok        ""     "$ASCII$NIHONGO$EMOJI"
roundtrip "UTF-8 emoji, BOM"         ok        ""     "\xef\xbb\xbf$ASCII$NIHONGO$EMOJI"

echo
echo "autodetection, where the file is not perfectly one thing:"
# A file that is UTF-8 apart from one byte that is not. Everything valid in it
# survives and the bad byte comes back as "?", which is what USAGE.md's known
# limits say and is not going to change without teaching the whole multi-byte
# layer about byte sequences that are not characters. -b round trips the same
# file exactly; the case below and the one in test-hostile.sh are the two halves
# of that. #30.
detects "UTF-8 with one bad byte keeps the rest" ok \
	"$NIHONGO$NIHONGO$NIHONGO$NIHONGO$NIHONGO$NIHONGO\xff\n" \
	"$NIHONGO$NIHONGO$NIHONGO$NIHONGO$NIHONGO$NIHONGO?\n"
# And the other direction, so that a change to the detection cannot quietly
# start reading Shift-JIS files as something else.
detects "Shift-JIS is still Shift-JIS" ok \
	"$SJIS_NIHONGO$SJIS_NIHONGO$SJIS_NIHONGO$SJIS_NIHONGO" \
	"$SJIS_NIHONGO$SJIS_NIHONGO$SJIS_NIHONGO$SJIS_NIHONGO"

# edit <name> <expect: ok|knownfail> <keys> <input bytes> <expected bytes>
# Keys and text are fed as a script, so the key code is forced to UTF-8 too.
edit() {
	counted && return
	local name=$1 expect=$2 keys=$3 data=$4 want=$5

	printf "$data" > "$tmp/in"
	printf "$want" > "$tmp/want"
	rm -f "$tmp/out"
	printf "$keys:w! %s/out\r:q!\r" "$tmp" > "$tmp/ecmds"
	pty "TERM=xterm $jvim -T xterm -K TTT -k t -s $tmp/ecmds $tmp/in" \
		>/dev/null 2>&1
	verdict "$name" "$expect"
}

NIHON='\xe6\x97\xa5\xe6\x9c\xac'						# 日本
GO='\xe8\xaa\x9e'									# 語
HON='\xe6\x9c\xac'									# 本
NI='\xe6\x97\xa5'									# 日
EM='\xf0\x9f\x98\x80'								# 😀 4 bytes
KO='\xed\x95\x9c'									# 한 outside CP932

echo
echo "editing over multi-byte characters:"
# x must delete a whole character, however many bytes it is
edit "x on 3 byte char"       ok "x"    "$NIHON$GO\n"      "$HON$GO\n"
edit "x on 4 byte char"       ok "3lx"  "abc$EM""def\n"    "abcdef\n"
edit "x on char outside CP932" ok "x"   "$KO$GO\n"         "$GO\n"
# l/h move by character, not by byte
edit "l then x"               ok "lx"   "$NIHON$GO\n"      "$NI$GO\n"
edit "\$ then x"              ok "\$x"  "$NIHON$GO\n"      "$NIHON\n"
edit "l l h x"                ok "llhx" "$NIHON$GO\n"      "$NI$GO\n"
# a word of ideographs is one word
edit "dw over kanji"          ok "dw"   "$NIHON$GO\n"      "\n"
edit "dw stops at space"      ok "dw"   "$NIHON$GO abc\n"  "abc\n"
# insert multi-byte text
edit "insert 4 byte char"     ok "i$EM\033" "abc\n"       "$EM""abc\n"
edit "append after wide char" ok "a$KO\033" "$NI\n"        "$NI$KO\n"
# undo restores the bytes
edit "u after x"              ok "xu"   "$NIHON$GO\n"      "$NIHON$GO\n"

echo
echo "screen columns over double width characters:"
# 日本語 occupies columns 1-2, 3-4, 5-6. "N|" goes to a screen column, so this
# checks that the column to byte mapping is right in both directions.
edit "3| picks 2nd char"      ok "3|x"  "$NIHON$GO\n"      "$NI$GO\n"
edit "5| picks 3rd char"      ok "5|x"  "$NIHON$GO\n"      "$NIHON\n"
edit "2| snaps to 1st char"   ok "2|x"  "$NIHON$GO\n"      "$HON$GO\n"
edit "7| stays on last char"  ok "7|x"  "$NIHON$GO\n"      "$NIHON\n"

# typed <name> <expect: ok|knownfail> <keys as raw bytes> <expected bytes> [opts]
#
# The keys go to jvim's terminal instead of through -s, because a script file is
# read straight by vgetorpeek() while terminal input goes through inchar(), which
# reads at most MAXMAPLEN (50) bytes at a time. A character landing across one of
# those boundaries is what mangled pasted text: kanjiconvsfrom() has to carry the
# split bytes over in its "tail", or every byte of that character becomes '?'.
#
# 'opts' overrides the codes; the default is UTF-8 throughout.
typed() {
	counted && return
	local name=$1 expect=$2 keys=$3 want=$4 opts=${5:--K TTTT -k t}

	: > "$tmp/in"
	printf "$want" > "$tmp/want"
	rm -f "$tmp/out"
	printf "$keys:w! %s/out\r:q!\r" "$tmp" > "$tmp/keys"
	pty "TERM=xterm $jvim -T xterm $opts $tmp/in" \
		< "$tmp/keys" >/dev/null 2>&1
	local got
	if cmp -s "$tmp/want" "$tmp/out"; then got=ok; else got=bad; fi

	if [ "$expect" = ok ] && [ "$got" = ok ]; then
		printf '  PASS        %s\n' "$name"; pass=$((pass+1))
	elif [ "$expect" = ok ]; then
		printf '  FAIL        %s\n' "$name"
		printf '                want %s\n' "$(hex "$tmp/want")"
		printf '                got  %s\n' "$(hex "$tmp/out")"
		fail=$((fail+1))
	elif [ "$got" = bad ]; then
		printf '  KNOWN-FAIL  %s\n' "$name"; xfail=$((xfail+1))
	else
		printf '  NOW PASSES  %s  <- update the expectation\n' "$name"
		xpass=$((xpass+1))
	fi
}

# 66 bytes, with a character sitting exactly across the 50 byte boundary.
BOUNDARY='## \xf0\x9f\x8f\xaeAI\xe3\x81\xaf\xe5\x91\xb3\xe6\x96\xb9\xe3\x81\x8b\xe3\x80\x81\xe3\x81\x9d\xe3\x82\x8c\xe3\x81\xa8\xe3\x82\x82\xe3\x80\x8c\xe6\xb0\x97\xe9\x9b\xa3\xe3\x81\x97\xe3\x81\x84\xe6\x8c\x87\xe7\xa4\xba\xe5\xbd\xb9\xe3\x80\x8d\xe3\x81\x8b'
# 120 x "あいうえお", built so printf sees the escapes rather than the bytes.
LONG=""
n=0
while [ $n -lt 120 ]; do
	LONG="$LONG\\xe3\\x81\\x82\\xe3\\x81\\x84\\xe3\\x81\\x86\\xe3\\x81\\x88\\xe3\\x81\\x8a"
	n=$((n + 1))
done

echo
echo "input read in chunks (a character across a 50 byte boundary):"
typed "char across boundary"   ok "i$BOUNDARY\033"        "$BOUNDARY\n"
typed "600 byte kana run"      ok "i$LONG\033"            "$LONG\n"
typed "run with emoji"         ok "i$EM$LONG$EM\033"      "$EM$LONG$EM\n"

echo
echo "typed characters holding a byte that is also a key code:"
# 0xa0 is K_ZERO and 0xfd is K_NUL, the two bytes inchar() has to keep out of
# the conversion for a special key to survive (see keyconvsfrom() in term.c).
# Both are also perfectly ordinary bytes inside a character -- 0x82 0xa0 is
# HIRAGANA A in Shift-JIS -- so the key codes may only be recognised where a
# character starts, never by scanning the bytes.
typed "0xa0 as the last byte"  ok "i\xe3\x82\xa0\033"     "\xe3\x82\xa0\n"
typed "0xa0 in the middle"     ok "i\xe2\xa0\x80\033"     "\xe2\xa0\x80\n"
typed "Shift-JIS 0x82 0xa0"    ok "i\x82\xa0\033"         "\xe3\x81\x82\n"  "-K SSST -k t"
typed "Shift-JIS kanji run"    ok "i\x93\xfa\x96\x7b\x8c\xea\033" \
															"\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\n" \
															"-K SSST -k t"

echo
echo "yank, put, replace, search, join:"
edit "yl then p"              ok "ylp"  "$NIHON$GO\n"      "$NI$NI$HON$GO\n"
edit "yl then P"              ok "ylP"  "$NIHON$GO\n"      "$NI$NI$HON$GO\n"
edit "r over wide char"       ok "rZ"   "$NIHON$GO\n"      "Z$HON$GO\n"
edit "search then x"          ok "/$GO\rx" "$NIHON$GO\n"  "$NIHON\n"
# Japanese text has no spaces between words, so J does not add one; that is
# what jvim's 'jjoinspaces' is about and it is on by default.
edit "J joins kanji, no space" ok "J"   "$NI\n$HON\n"      "$NI$HON\n"
edit "J joins ascii, w/ space" ok "J"   "ab\ncd\n"          "ab cd\n"
edit "cw over kanji"          ok "cwZ\033" "$NIHON$GO abc\n" "Z abc\n"
edit "visual v l l d"         ok "vlld" "$NIHON$GO\n"      "\n"
edit "e then x"               ok "ex"   "$NIHON$GO abc\n"  "$NIHON abc\n"

AA='\xe3\x81\x82'								# あ
II='\xe3\x81\x84'								# い
UU='\xe3\x81\x86'								# う
NN='\xe3\x82\x93'								# ん

echo
echo "file names, which are three bytes a character here too:"
# The Windows suite has these as well, where they are about the manifest making
# the ...A file APIs take UTF-8. This is the part of it that is not Windows:
# the name goes out through fileconvsto() and comes back through the directory
# scan, and both have to agree about how long a character is. "-K TTT" pins the
# codes, so what the machine's locale happens to be does not decide the answer.
edit "a name of 3 byte characters" ok \
	":w! $tmp/$NIHON$GO.txt\r:e! $tmp/$NIHON$GO.txt\rdd" 'a\nb\n' 'b\n'
# expansion hands the name to the shell and reads the answer back, so the name
# makes a second round trip through the code conversion on the way
edit "a name found by a wildcard" ok \
	":w! $tmp/$NIHON$GO.txt\r:e! $tmp/$NIHON*\rdd" 'a\nb\n' 'b\n'

echo
echo "regexp character classes over multi-byte characters:"
# [あ] must not match い: they share their first two bytes (e3 81), which is what
# the old two-byte comparison keyed on.
edit "[a] matches only a"      ok ":s/[$AA]//g\r"       "$AA$II$UU\n"      "$II$UU\n"
edit "[au] matches both"       ok ":s/[$AA$UU]//g\r"    "$AA$II$UU\n"      "$II\n"
edit "[a-n] range by code pt"  ok ":s/[$AA-$NN]//g\r"   "$AA$II$UU""abc\n" "abc\n"
edit "[a-c] leaves kana"       ok ":s/[a-c]//g\r"       "$AA""abc$II\n"    "$AA$II\n"
edit "search a not i"          ok "/$AA\rx"             "$II$AA$UU\n"      "$II$UU\n"

echo
echo "the help file, which is converted and not loaded:"
# 日本語 ten times over, in one run of ISO-2022-JP: 67 bytes in the file, 91 in
# the internal UTF-8. It has to be a run, because a short one shrinks -- the six
# bytes of the two escape sequences pay for three characters.
JIS_NIHONGO='\x46\x7c\x4b\x5c\x38\x6c'					# 日本語 in ISO-2022-JP
U8_NIHONGO='\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e'		# and in UTF-8
jis_run= ; u8_run=
for i in 1 2 3 4 5 6 7 8 9 10; do
	jis_run="$jis_run$JIS_NIHONGO"
	u8_run="$u8_run$U8_NIHONGO"
done
helpscreen "ISO-2022-JP help file" ok "\033\$B$jis_run\033(B\n" "$u8_run\n"

if [ -n "$count_only" ]; then
	printf 'cases %d\n' "$cases"
	exit 0
fi

echo
printf 'cases %d\n' "$cases"
printf 'pass %d  fail %d  known-fail %d  newly-passing %d\n' \
		"$pass" "$fail" "$xfail" "$xpass"
[ "$fail" -eq 0 ] || exit 1
exit 0
