#!/bin/bash
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
# here. "script" (util-linux) is used to give jvim a pty.

set -uo pipefail

root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
jvim=${1:-$root/src/jvim3}

if [ ! -x "$jvim" ]; then
	echo "no jvim binary at $jvim" >&2
	echo "build one with: cd src && cp makjunix.mak makefile && make" >&2
	exit 2
fi
command -v script >/dev/null 2>&1 || { echo "'script' not found" >&2; exit 2; }

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT
printf ':w! %s/out\r:q!\r' "$tmp" > "$tmp/cmds"

pass=0; fail=0; xfail=0; xpass=0

# roundtrip <name> <expect: ok|knownfail> <jvim options> <printf-escaped bytes>
roundtrip() {
	local name=$1 expect=$2 opts=$3 data=$4
	printf "$data" > "$tmp/in"
	rm -f "$tmp/out"
	script -qec "TERM=xterm $jvim -T xterm $opts -s $tmp/cmds $tmp/in" \
		/dev/null >/dev/null 2>&1
	local got
	if cmp -s "$tmp/in" "$tmp/out"; then got=ok; else got=bad; fi

	if [ "$expect" = ok ] && [ "$got" = ok ]; then
		printf '  PASS        %s\n' "$name"; pass=$((pass+1))
	elif [ "$expect" = ok ]; then
		printf '  FAIL        %s\n' "$name"
		printf '                in  %s\n' "$(xxd -p "$tmp/in" | tr -d '\n')"
		printf '                out %s\n' "$(xxd -p "$tmp/out" 2>/dev/null | tr -d '\n')"
		fail=$((fail+1))
	elif [ "$got" = bad ]; then
		printf '  KNOWN-FAIL  %s\n' "$name"; xfail=$((xfail+1))
	else
		printf '  NOW PASSES  %s  <- update the expectation\n' "$name"
		xpass=$((xpass+1))
	fi
}

ASCII='abc\n'
NIHONGO='\xe6\x97\xa5\xe6\x9c\xac\xe8\xaa\x9e\n'		# 日本語  JIS X 0208
MARU1='\xe2\x91\xa0\n'								# ①      in CP932
HANGUL='\xed\x95\x9c\n'								# 한      BMP, not in CP932
EMOJI='\xf0\x9f\x98\x80\n'							# 😀      non-BMP
ACCENT='\xc3\xa9\n'									# é       Latin-1
EUC_NIHONGO='\xc6\xfc\xcb\xdc\xb8\xec\n'
SJIS_NIHONGO='\x93\xfa\x96\x7b\x8c\xec\n'

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

# edit <name> <expect: ok|knownfail> <keys> <input bytes> <expected bytes>
# Keys and text are fed as a script, so the key code is forced to UTF-8 too.
edit() {
	local name=$1 expect=$2 keys=$3 data=$4 want=$5

	printf "$data" > "$tmp/in"
	printf "$want" > "$tmp/want"
	rm -f "$tmp/out"
	printf "$keys:w! %s/out\r:q!\r" "$tmp" > "$tmp/ecmds"
	script -qec "TERM=xterm $jvim -T xterm -K TTT -k t -s $tmp/ecmds $tmp/in" \
		/dev/null >/dev/null 2>&1
	local got
	if cmp -s "$tmp/want" "$tmp/out"; then got=ok; else got=bad; fi

	if [ "$expect" = ok ] && [ "$got" = ok ]; then
		printf '  PASS        %s\n' "$name"; pass=$((pass+1))
	elif [ "$expect" = ok ]; then
		printf '  FAIL        %s\n' "$name"
		printf '                want %s\n' "$(xxd -p "$tmp/want" | tr -d '\n')"
		printf '                got  %s\n' "$(xxd -p "$tmp/out" 2>/dev/null | tr -d '\n')"
		fail=$((fail+1))
	elif [ "$got" = bad ]; then
		printf '  KNOWN-FAIL  %s\n' "$name"; xfail=$((xfail+1))
	else
		printf '  NOW PASSES  %s  <- update the expectation\n' "$name"
		xpass=$((xpass+1))
	fi
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

# typed <name> <expect: ok|knownfail> <keys as raw bytes> <expected bytes>
#
# The keys go to jvim's terminal instead of through -s, because a script file is
# read straight by vgetorpeek() while terminal input goes through inchar(), which
# reads at most MAXMAPLEN (50) bytes at a time. A character landing across one of
# those boundaries is what mangled pasted text: kanjiconvsfrom() has to carry the
# split bytes over in its "tail", or every byte of that character becomes '?'.
typed() {
	local name=$1 expect=$2 keys=$3 want=$4

	: > "$tmp/in"
	printf "$want" > "$tmp/want"
	rm -f "$tmp/out"
	printf "$keys:w! %s/out\r:q!\r" "$tmp" > "$tmp/keys"
	script -qec "TERM=xterm $jvim -T xterm -K TTTT -k t $tmp/in" /dev/null \
		< "$tmp/keys" >/dev/null 2>&1
	local got
	if cmp -s "$tmp/want" "$tmp/out"; then got=ok; else got=bad; fi

	if [ "$expect" = ok ] && [ "$got" = ok ]; then
		printf '  PASS        %s\n' "$name"; pass=$((pass+1))
	elif [ "$expect" = ok ]; then
		printf '  FAIL        %s\n' "$name"
		printf '                want %s\n' "$(xxd -p "$tmp/want" | tr -d '\n')"
		printf '                got  %s\n' "$(xxd -p "$tmp/out" 2>/dev/null | tr -d '\n')"
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

echo
printf 'pass %d  fail %d  known-fail %d  newly-passing %d\n' \
		"$pass" "$fail" "$xfail" "$xpass"
[ "$fail" -eq 0 ] || exit 1
exit 0
