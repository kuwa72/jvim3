#!/usr/bin/env bash
#
# Syntax colouring tests: what the rules in syntax/ actually colour.
#
#   scripts/test-syntax.sh [jvim-binary]
#
# ":syntax dump" walks the buffer the way the screen does and writes one line
# per coloured run -- "3:4-6 Conditional w/if", which is line 3, bytes 4 to 6,
# the group, and the rule as it was written. A case here is a few lines of a
# language and the dump they should produce, so a rule that stops matching, or
# starts matching the wrong thing, fails here instead of being noticed months
# later by someone looking at the screen.
#
# That is also the loop to use when writing rules: change syntax/<type>.jvsyn,
# run this, read the diff. There is nothing to look at with your eyes.
#
# jvim is given a terminal by scripts/ptyrun.c, as in the other suites.

set -uo pipefail

root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
jvim=${1:-$root/src/jvim3}

# COUNT_ONLY=1 prints "cases N" and stops, with no binary and no compiler.
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
	# The rules as they are in the tree, not as they may be installed, and an
	# rc that does nothing but reach them. HOME is here so that whoever is
	# running this cannot have an opinion about it.
	printf 'set fexrc\nset syntax\nsource $VIM/syntax/filetype.jvsyn\n' \
			> "$tmp/.jvimrc"
fi

pass=0; fail=0; cases=0

counted() {
	cases=$((cases+1))
	[ -n "$count_only" ]
}

# case <name> <file name> <source> <wanted dump> [ex commands first]
#
# The file name decides which rules are read, so it carries the real suffix.
# The last argument is for cases about the rule language itself: whatever it
# holds is typed before the dump, so a rule added by hand shows up in it.
#
# printf's "%b" rather than using the string as the format: a source that
# starts with "--" is not then read as an option, and a "%" in a pattern is
# just a "%".
case_() {
	counted && return
	local name=$1 file=$2 src=$3 want=$4 pre=${5:-}

	printf '%b' "$src" > "$tmp/$file"
	printf '%b' "$want" > "$tmp/want"
	rm -f "$tmp/out"
	printf '%b' "$pre" > "$tmp/keys"
	printf ':syntax dump %s/out\r:q!\r' "$tmp" >> "$tmp/keys"
	"$tmp/ptyrun" /bin/sh -c \
		"HOME=$tmp VIM=$root TERM=xterm $jvim -T xterm -s $tmp/keys $tmp/$file" \
		>/dev/null 2>&1
	[ -f "$tmp/out" ] || : > "$tmp/out"

	if cmp -s "$tmp/want" "$tmp/out"; then
		printf '  PASS        %s\n' "$name"; pass=$((pass+1))
	else
		printf '  FAIL        %s\n' "$name"
		diff -u "$tmp/want" "$tmp/out" | sed -n '3,12p' | sed 's/^/                /'
		fail=$((fail+1))
	fi
}

if [ -z "$count_only" ]; then
	echo
	echo "the rules in syntax/, through \":syntax dump\":"
fi

case_ 'C: comment, string, type' c.c \
'int main(void)\n{\n\t/* two\n\t   lines */\n\tchar *s = "hi";\n}\n' \
'1:0-3 Type w/int\n1:4-8 Function -/[_a-zA-Z][_a-zA-Z0-9]*\\s*(\n1:9-13 Type w/void\n3:1-7 Comment p/\\/\\*\n4:0-12 Comment p/\\/\\*\n5:1-5 Type w/char\n5:11-15 String m/".*[^\\\\]"\n'

case_ 'C: a comment that never closes' c.c \
'int a;\n/* open\nstill\n' \
'1:0-3 Type w/int\n2:0-7 Comment p/\\/\\*\n3:0-5 Comment p/\\/\\*\n'

case_ 'Python: triple quotes on one line' t.py \
'x = """same line"""\ny = 1\n' \
'1:4-19 String p/"""\n2:4-5 Number w/\\d\\+\n'

case_ 'Python: keywords, number, comment' t.py \
'def f(n):\n    if n > 0x1f:  # why\n        return None\n' \
'1:0-3 Statement w/def\n2:4-6 Conditional w/if\n2:11-15 Number w/0x\\x\\+\n2:18-23 Comment n/#.*$\n3:8-14 Statement w/return\n3:15-19 Constant w/None\n'

case_ 'Go: package, string, type' t.go \
'package main\n\nfunc f() error {\n\treturn nil\n}\n' \
'1:0-7 Include w/package\n3:0-4 Statement w/func\n3:9-14 Type w/error\n4:1-7 Statement w/return\n4:8-11 Constant w/nil\n'

case_ 'Rust: attribute and macro' t.rs \
'#[derive(Debug)]\nfn main() {\n    println!("hi");\n}\n' \
'1:0-16 PreProc n/^\\s*#!\\=\\[.*\\]\n2:0-2 Statement w/fn\n3:4-12 Macro -/\\i\\+![({[]\n3:13-17 String m/".*[^\\\\]"\n'

case_ 'shell: shebang beats the comment rule' t.sh \
'#!/bin/sh\n# a comment\nfor f in x; do :; done\n' \
'1:0-9 PreProc n/^#!.*$\n2:0-11 Comment n/#.*$\n3:0-3 Repeat w/for\n3:6-8 Conditional w/in\n3:12-14 Repeat w/do\n3:18-22 Repeat w/done\n'

case_ 'YAML: key, string, boolean' t.yml \
'name: jvim\nok: true\nquoted: "x"\n' \
'1:0-4 Identifier m-/^\\s*[^ #]\\+:\n2:0-2 Identifier m-/^\\s*[^ #]\\+:\n2:4-8 Boolean w/true\n3:0-6 Identifier m-/^\\s*[^ #]\\+:\n3:8-11 String m/".*[^\\\\]"\n'

case_ 'Markdown: heading, code fence, inline code' t.md \
'# Title\n\n`code` here\n\n```sh\nnot markdown\n```\n' \
'1:0-7 MdHead n/^#\\+\\s.*$\n3:0-6 String m/`.*`\n5:0-5 Comment p/^```\n6:0-12 Comment p/^```\n7:0-3 Comment p/^```\n'

case_ 'JSON: a key is not a value' t.json \
'{\n  "name": "jvim",\n  "n": 3\n}\n' \
'2:2-8 Identifier m-/".*[^\\\\]":\n2:10-16 String m/".*[^\\\\]"\n3:2-5 Identifier m-/".*[^\\\\]":\n3:7-8 Number n/-\\=\\d\\+\n'

case_ 'a file type with no rules is left alone' t.unknown \
'nothing here is coloured\n' \
''

case_ 'Japanese in a rule matches' t.py \
'x = "\343\201\202"  # \343\201\202\n' \
'1:4-9 String m/".*[^\\\\]"\n1:11-16 Comment n/#.*$\n'

case_ 'JavaScript: template literal, keywords' t.js \
'const x = 42;  // c\nclass A { async run() { return `t${x}`; } }\n' \
'1:0-5 StorageClass w/const\n1:10-12 Number w/\\d\\+\n1:15-19 Comment n/\\/\\/.*$\n2:0-5 Structure w/class\n2:10-15 Statement w/async\n2:24-30 Statement w/return\n2:31-38 String p/`\n'

case_ 'Ruby: instance variable, keywords' t.rb \
'# c\nrequire "json"\nclass Foo\n  def run\n    @n = nil\n  end\nend\n' \
'1:0-3 Comment n/#.*$\n2:0-7 Include w/require\n2:8-14 String m/".*[^\\\\]"\n3:0-5 Structure w/class\n4:2-5 Statement w/def\n5:4-6 Identifier n/@@\\=\\i\\+\n5:9-12 Constant w/nil\n6:2-5 Statement w/end\n7:0-3 Statement w/end\n'

case_ 'CSS: selector, colour, unit' t.css \
'/* c */\n.a, #b {\n  color: #ff8800;\n  margin: 1.5em;\n}\n' \
'1:0-7 Comment p/\\/\\*\n2:0-2 Tag n/[.#]\\i[-_a-zA-Z0-9]*\n2:4-6 Tag n/[.#]\\i[-_a-zA-Z0-9]*\n3:9-16 Constant n/#\\x\\x\\x\\x\\x\\x\n4:10-15 Float n/\\d*\\.\\d\\+[a-z%]*\n'

case_ 'SQL: keywords are matched either case' t.sql \
'-- c\nSELECT id FROM t WHERE n > 10;\n' \
'1:0-4 Comment n/--.*$\n2:0-6 Statement iw/select\n2:10-14 Statement iw/from\n2:17-22 Statement iw/where\n2:27-29 Number w/\\d\\+\n'

case_ 'Lua: a comment is two dashes' t.lua \
'-- c\nlocal M = {}\nfunction M.f() return nil end\n' \
'1:0-4 Comment n/--.*$\n2:0-5 Statement w/local\n3:0-8 Statement w/function\n3:15-21 Statement w/return\n3:22-25 Constant w/nil\n3:26-29 Statement w/end\n'

case_ 'PHP: variables and types' t.php \
'<?php\n$x = "a";\nfunction f(int $n): string { return null; }\n' \
'1:0-5 PreProc n/<?php\n2:0-2 Identifier n/\\$\\i\\+\n2:5-8 String m/".*[^\\\\]"\n3:0-8 Statement iw/function\n3:11-14 Type iw/int\n3:15-17 Identifier n/\\$\\i\\+\n3:20-26 Type iw/string\n3:29-35 Statement iw/return\n3:36-40 Constant iw/null\n'

case_ 'diff: the added and removed lines' t.diff \
'diff --git a/x b/x\n@@ -1 +1 @@\n-old\n+new\n' \
'1:0-18 Statement n/^diff .*$\n2:0-11 Special n/^@@.*$\n3:0-4 DiffDel n/^-.*$\n4:0-4 DiffAdd n/^+.*$\n'

case_ 'TOML: table, key, value' t.toml \
'# c\nname = "jvim"\n\n[deps]\nok = true\n' \
'1:0-3 Comment n/#.*$\n2:0-5 Identifier m-/^\\s*[^ #=]\\+\\s*=\n2:7-13 String m/".*[^\\\\]"\n4:0-6 Tag n/^\\s*\\[.*\\]\n5:0-3 Identifier m-/^\\s*[^ #=]\\+\\s*=\n5:5-9 Boolean w/true\n'

case_ 'Makefile: assignment, target, variable' t.mk \
'CC = gcc\nall: prog\n\t$(CC) -o prog\n' \
'1:0-3 Type m-/^[A-Za-z_][A-Za-z0-9_]*\\s*=\n2:0-3 PreProc m-/^[A-Za-z0-9_.%$][^ #=:]*:\n3:1-6 Identifier n/\\$[({][^)}]*[)}]\n'

case_ 'Dockerfile: matched by name, not by suffix' Dockerfile \
'# c\nFROM debian:12 AS build\nRUN echo "hi"\n' \
'1:0-3 Comment n/#.*$\n2:0-4 Statement iw/FROM\n2:15-17 Operator iw/AS\n3:0-3 Statement iw/RUN\n3:9-13 String m/".*[^\\\\]"\n'

# The rule language itself. A mode letter nobody handles used to be dropped in
# silence, so the rule went in and matched the wrong thing; now it is refused,
# and the dump is the way to see which of the two happened.
case_ 'a known mode letter adds the rule' t.unknown \
'def x\n' \
'1:0-3 - w/def\n' \
':syntax red w/def\r'

case_ 'an unknown mode letter is refused' t.unknown \
'def x\n' \
'' \
':syntax red q/def\r'

if [ -n "$count_only" ]; then
	printf 'cases %d\n' "$cases"
	exit 0
fi

echo
printf 'cases %d\n' "$cases"
printf 'pass %d  fail %d\n' "$pass" "$fail"
[ "$fail" -eq 0 ] || exit 1
exit 0
