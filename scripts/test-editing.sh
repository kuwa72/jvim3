#!/usr/bin/env bash
#
# Editing tests for JVim: the vi behaviour the encoding suite does not touch.
#
#   scripts/test-editing.sh [jvim-binary]
#
# scripts/test-encoding.sh covers kanji and UTF-8. This covers the rest of the
# editor -- motions, operators, registers, marks, undo, ex ranges, ":g", ":s",
# the ":!" filter and wildcard expansion -- so that a change to the C can be
# told apart from a change to the editor. It is all ASCII on purpose; the other
# suite is where the byte level cases live.
#
# jvim is given a terminal by scripts/ptyrun.c, as in the other suite.

set -uo pipefail

root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
jvim=${1:-$root/src/jvim3}

# COUNT_ONLY=1 prints "cases N" and stops: how many cases this file holds, with
# no binary, no compiler and no run. See the same comment in test-encoding.sh.
count_only=${COUNT_ONLY:-}

if [ -z "$count_only" ] && [ ! -x "$jvim" ]; then
	echo "no jvim binary at $jvim" >&2
	exit 2
fi

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

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
hex() { od -An -tx1 -v "$1" 2>/dev/null | tr -d ' \n'; }

pass=0; fail=0; xfail=0; xpass=0; cases=0

# Counted in both modes, so "cases" is the number of cases in this file whether
# or not they were run, and stays right when a KNOWN-FAIL is added back.
counted() {
	cases=$((cases+1))
	[ -n "$count_only" ]
}

# run <name> <expect: ok|knownfail> <keys> <input> <wanted output>
#
# The keys are fed through "-s", so \r ends an ex command and \033 is escape.
run() {
	counted && return
	local name=$1 expect=$2 keys=$3 data=$4 want=$5

	# printf eats a "%" as the start of a conversion, and ":%d" is a perfectly
	# ordinary thing to want to type, so double them all.
	keys=${keys//%/%%}
	data=${data//%/%%}
	want=${want//%/%%}

	printf "$data" > "$tmp/in"
	printf "$want" > "$tmp/want"
	rm -f "$tmp/out"
	printf "$keys:w! %s/out\r:q!\r" "$tmp" > "$tmp/keys"
	pty "TERM=xterm $jvim -T xterm -s $tmp/keys $tmp/in" >/dev/null 2>&1

	verdict "$name" "$expect"
}

# runtyped <name> <expect> <keys> <input> <wanted output>
#
# The same, except that the keys are *typed*: they go down the pty rather than
# through "-s". That is a different road into the editor. Script input is taken
# as it stands, while typed bytes are first converted from the code the keyboard
# speaks (keyconvsfrom() in src/term.c), so this is the only way to test the
# terminal key codes -- the cursor keys and CTRL-@ -- at all.
#
# "-T builtin_xterm" pins the key strings to the built in xterm entry, so what
# has to be typed for a cursor key does not depend on the system's termcap.
runtyped() {
	counted && return
	local name=$1 expect=$2 keys=$3 data=$4 want=$5

	keys=${keys//%/%%}
	data=${data//%/%%}
	want=${want//%/%%}

	printf "$data" > "$tmp/in"
	printf "$want" > "$tmp/want"
	rm -f "$tmp/out"
	printf "$keys:w! %s/out\r:q!\r" "$tmp" \
		| pty "TERM=xterm $jvim -T builtin_xterm $tmp/in" >/dev/null 2>&1

	verdict "$name" "$expect"
}

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

ABC='a\nb\nc\nd\ne\n'

echo "jvim: $jvim"
echo
echo "motions and operators:"
run "dd"                    ok "dd"        "$ABC"              'b\nc\nd\ne\n'
run "3dd"                   ok "3dd"       "$ABC"              'd\ne\n'
run "dj"                    ok "dj"        "$ABC"              'c\nd\ne\n'
run "d\$ on a word"          ok "wd\$"      'one two three\n'   'one \n'
run "dw"                    ok "dw"        'one two three\n'   'two three\n'
run "d2w"                   ok "d2w"       'one two three\n'   'three\n'
run "de"                    ok "de"        'one two three\n'   ' two three\n'
run "db from end"           ok "\$db"      'one two three\n'   'one two e\n'
run "x with a count"        ok "3x"        'abcdef\n'          'def\n'
run "D"                     ok "wD"        'one two three\n'   'one \n'
run "C"                     ok "wCx\033"   'one two three\n'   'one x\n'
run "f then dt"             ok "df "       'one two three\n'   'two three\n'
run "% over parens"         ok "d%"        '(a b) c\n'         ' c\n'
run "cw"                    ok "cwX\033"   'one two\n'         'X two\n'
run "r"                     ok "rZ"        'abc\n'             'Zbc\n'
run "R"                     ok "RXY\033"   'abcd\n'            'XYcd\n'
run "~"                     ok "3~"        'abc\n'             'ABC\n'
run "J"                     ok "J"         'a\nb\n'            'a b\n'
run "o"                     ok "oX\033"    'a\n'               'a\nX\n'
run "O"                     ok "OX\033"    'a\n'               'X\na\n'
run "p line"                ok "yyp"       'a\nb\n'            'a\na\nb\n'
run "P line"                ok "yyP"       'a\nb\n'            'a\na\nb\n'
run ">> and <<"             ok ":set sw=4\r>>"  'a\n'          '    a\n'
run "<< removes indent"     ok ":set sw=4\r<<"  '        a\n'  '    a\n'

echo
echo "registers, marks and undo:"
run "named register"        ok '"ayy"ap'   'a\nb\n'            'a\na\nb\n'
run "delete then put"       ok 'ddp'       'a\nb\n'            'b\na\n'
run "mark and delete to it" ok "majjd'a"   "$ABC"              'd\ne\n'
run "backtick mark"         ok 'wma0d`a'   'one two\n'         'two\n'
run "u after dd"            ok "ddu"       "$ABC"              "$ABC"
run "u after two dd"        ok "dddduu"    "$ABC"              "$ABC"
run "U on a line"           ok "xxU"       'abc\n'             'abc\n'
run "redo with ."           ok "dd."       "$ABC"              'c\nd\ne\n'

echo
echo "ex ranges and commands:"
run ":1,2d"                 ok ":1,2d\r"   "$ABC"              'c\nd\ne\n'
run ":%d leaves one line"   ok ":%d\r"     "$ABC"              '\n'
run ":.,+1d"                ok "j:.,+1d\r" "$ABC"              'a\nd\ne\n'
run ":\$d"                  ok ":\$d\r"    "$ABC"              'a\nb\nc\nd\n'
run ":2,3m0"                ok ":2,3m0\r"  "$ABC"              'b\nc\na\nd\ne\n'
run ":1co\$"                ok ":1co\$\r"  'a\nb\n'            'a\nb\na\n'
run ":g//d"                 ok ":g/b/d\r"  'a\nb\nab\n'        'a\n'
run ":v//d"                 ok ":v/b/d\r"  'a\nb\nab\n'        'b\nab\n'
run ":s with g"             ok ":s/a/X/g\r" 'aab\n'            'XXb\n'
run ":s without g"          ok ":s/a/X/\r" 'aab\n'             'Xab\n'
run ":%s"                   ok ":%s/a/X/\r" 'ab\nba\n'         'Xb\nbX\n'
run ":s with a range"       ok ":2,3s/a/X/\r" 'a\na\na\n'      'a\nX\nX\n'
run "& repeats a subst"     ok ":s/a/X/\rj&"      'a\na\n'      'X\nX\n'
run ":s with ^ and \$"      ok ":s/^/> /\r" 'a\n'              '> a\n'
run ":d into a register"    ok ':1d a\r:$put a\r' 'a\nb\n'     'b\na\n'
run ":put"                  ok 'yy:$put\r' 'a\nb\n'            'a\nb\na\n'
run ":normal ex range join" ok ':1,2j\r'   'a\nb\nc\n'         'a b\nc\n'

echo
echo "search:"
run "/ then dd"             ok "/c\rdd"    "$ABC"              'a\nb\nd\ne\n'
run "? then dd"             ok "G?b\rdd"   "$ABC"              'a\nc\nd\ne\n'
run "n wraps round"         ok "/a\rndd"   'a\nx\na\ny\n'      'x\na\ny\n'
run "search offset"         ok "/b/+1\rdd" 'a\nb\nc\n'         'a\nb\n'
run ":s with \\\\<"          ok ":%s/\\\\<a\\\\>/X/g\r" 'a ab a\n' 'X ab X\n'

echo
echo "the shell filter and wildcards, which use a temp file:"
# ":!" writes the range to a temp file, runs the command and reads it back;
# wildcard expansion runs the shell and reads its output from another one.
run ":%!sort"               ok ':%!sort\r' 'c\na\nb\n'         'a\nb\nc\n'
run ":1,2!tr a-z A-Z"       ok ':1,2!tr a-z A-Z\r' 'a\nb\nc\n' 'A\nB\nc\n'
run ":r !echo"              ok ':r !echo hello\r' 'a\n'        'a\nhello\n'
run ":w! then :e"           ok ":w! $tmp/side\r:e! $tmp/si*\rdd" 'a\nb\n' 'b\n'

echo
echo "keys as typed, which go through the code conversion \"-s\" skips:"
# The cursor keys arrive as a terminal key code -- an escape sequence here, and
# a K_NUL pair on Windows -- and CTRL-@ as K_ZERO. None of those are characters,
# and converting them as if they were turned each into a '?' that got inserted
# instead of moving the cursor.
# The leading "0" is a no-op -- the cursor is on the first column already -- and
# it is there because an escape sequence arriving as the very first thing typed
# is not recognised on macOS, where these three failed while every case with a
# keystroke before the sequence passed. Whatever swallows it, it is not what
# these are testing.
runtyped "typed cursor right"   ok '0\033OC\033OC\033OCx' 'abcdef\n'  'abcef\n'
runtyped "typed cursor down"    ok '0\033OBdd'            "$ABC"      'a\nc\nd\ne\n'
runtyped "typed cursor in insert" ok 'i\033OC\033'        'abc\n'     'abc\n'
runtyped "typed shift-right"    ok '0\033Ovx'             'one two\n' 'one wo\n'
runtyped "typed up on the : line" ok ':1d\r:\033OA\r'     "$ABC"      'c\nd\ne\n'
runtyped "typed CTRL-@"         ok 'iabc\033i\000'        'X\n'       'ababccX\n'

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
