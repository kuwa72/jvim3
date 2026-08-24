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
'1:0-7 MdHead n/^#\\+\\s.*$\n3:0-6 String m/`.*`\n5:0-5 MdCode p/^```\n6:0-12 MdCode p/^```\n7:0-3 MdCode p/^```\n'

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

# The last line is the reason the property rule wants an indent: "a:hover" is a
# name before a colon as much as "color" is, and is expected to stay plain.
case_ 'CSS: selector, property, colour, unit' t.css \
'/* c */\n.a, #b {\n  color: #ff8800;\n  margin: 1.5em;\n}\na:hover { color: red }\n' \
'1:0-7 Comment p/\\/\\*\n2:0-2 Tag n/[.#]\\i[-_a-zA-Z0-9]*\n2:4-6 Tag n/[.#]\\i[-_a-zA-Z0-9]*\n3:0-7 Identifier m-/^\\s\\+[-a-zA-Z]\\+\\s*:\n3:9-16 Constant n/#\\x\\x\\x\\x\\x\\x\n4:0-8 Identifier m-/^\\s\\+[-a-zA-Z]\\+\\s*:\n4:10-15 Float n/\\d*\\.\\d\\+[a-z%]*\n'

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

case_ 'C#: keywords, types, number' t.cs \
'using System;\n// c\npublic class A {\n    public static int F(string s) { return 0x1f; }\n}\n' \
'1:0-5 Include w/using\n2:0-4 Comment n/\\/\\/.*$\n3:0-6 StorageClass w/public\n3:7-12 Structure w/class\n4:4-10 StorageClass w/public\n4:11-17 StorageClass w/static\n4:18-21 Type w/int\n4:24-30 Type w/string\n4:36-42 Statement w/return\n4:43-47 Number w/0x\\x\\+\n'

# HTML is the only type using "t", where the rule names three patterns: the tag
# to look inside, the one that ends it, and what to match in between. A "t" rule
# is dumped by the third -- a dozen of these begin "t/<" and the first would
# tell them apart from nothing.
case_ 'HTML: tag, argument, entity' t.html \
'<!-- c -->\n<body bgcolor="#ffffff">\n<a href="http://x.example/">&amp;</a>\n</body>\n' \
'1:0-10 Comment p/<!--\n2:0-1 Delimiter n/<\n2:1-5 HtmlTag iwt/body\n2:6-13 HtmlArg iwt/bgcolor\n2:14-23 Number t/"#\\x\\x\\x\\x\\x\\x"\n2:23-24 Delimiter n/>\n3:0-1 Delimiter n/<\n3:1-2 HtmlTag iwt/a\n3:3-7 HtmlArg iwt/href\n3:8-27 String mt/"[^#].*"\n3:27-28 Delimiter n/>\n3:28-33 SpecialChar n/&amp;\n3:33-35 Delimiter n/<\\/\n3:35-36 HtmlTag iwt/a\n3:36-37 Delimiter n/>\n4:0-2 Delimiter n/<\\/\n4:2-6 HtmlTag iwt/body\n4:6-7 Delimiter n/>\n'

# A tag over more than one line, with 'synlines' set as low as it goes.
#
# The rules that colour what is inside a tag used to find the tag by searching
# that many lines around the one being drawn, and the ">" here is two lines
# below the "td", so the tag name came out plain. The tag a line is inside of
# is remembered per line now, the way a region already was, and 'synlines'
# reaches nothing.
case_ 'HTML: a tag over three lines' t.html \
'<td\nwidth=3\n>4</td>\n' \
'1:0-1 Delimiter n/<\n1:1-3 HtmlTag iwt/td\n2:0-5 HtmlArg iwt/width\n2:6-7 Number t/\\d\\+\n3:0-1 Delimiter n/>\n3:2-4 Delimiter n/<\\/\n3:4-6 HtmlTag iwt/td\n3:6-7 Delimiter n/>\n' \
':set synlines=1\r'

case_ 'XML: declaration, tag, attribute' t.xml \
'<?xml version="1.0"?>\n<!-- c -->\n<root id="1">\n  <item/>\n</root>\n' \
'1:0-21 PreProc m/<?.*?>\n2:0-10 Comment p/<!--\n3:0-5 Statement n/<\\/\\=[^ >]\\+\n3:9-12 String m/".*"\n3:12-13 Statement n/\\/\\=>\n4:2-8 Statement n/<\\/\\=[^ >]\\+\n4:8-9 Statement n/\\/\\=>\n5:0-6 Statement n/<\\/\\=[^ >]\\+\n5:6-7 Statement n/\\/\\=>\n'

# %VAR%, then the two forms of a parameter, then a parameter with modifiers.
# The rule for the last of these used to be seventeen rules written after the
# %VAR% one, so %VAR% -- which matches from any % to the next one -- took "%~f1
# %" out of line 3 and left the rest plain, and %~dpnx2 was in none of the
# seventeen anyway. All three lines are one Value per parameter now.
case_ 'batch: a parameter and its modifiers' t.bat \
'echo %PATH%\necho %1 %*\necho %~f1 %~dpnx2\necho %~$PATH:3\n' \
'1:0-4 Keyword iw/echo\n1:5-11 Value m/%.*%\n2:0-4 Keyword iw/echo\n2:5-7 Value n/%[\\d\\*]\n2:8-10 Value n/%[\\d\\*]\n3:0-4 Keyword iw/echo\n3:5-9 Value n/%\\~[fdpnxsatz]*\\($PATH:\\)\\=\\d\n3:10-17 Value n/%\\~[fdpnxsatz]*\\($PATH:\\)\\=\\d\n4:0-4 Keyword iw/echo\n4:5-14 Value n/%\\~[fdpnxsatz]*\\($PATH:\\)\\=\\d\n'

case_ 'batch: REM, a variable, a label' t.bat \
'REM a note\n@echo off\nset OUT=%TEMP%\nif exist %1 goto done\n:done\n' \
'1:0-10 Comment i/REM.*\n2:1-5 Keyword iw/echo\n3:0-3 Keyword iw/set\n3:8-14 Value m/%.*%\n4:0-2 Conditional iw/if\n4:9-11 Value n/%[\\d\\*]\n4:12-16 Statement iw/goto\n'

# \047 is the apostrophe, which a comment starts with and which cannot be
# written inside the single quotes these arguments use.
case_ 'VBScript: apostrophe comment, keywords' t.vbs \
'\047 c\nDim n\nn = 1.5\nIf n > 0 Then\n  MsgBox "hi"\nEnd If\n' \
'1:0-3 Comment n/\\\047.*\n2:0-3 Statement iw/Dim\n3:4-7 Float n/\\d*\\.\\d\\+\n4:0-2 Statement iw/If\n4:7-8 Number w/\\d\\+\n4:9-13 Statement iw/Then\n5:2-8 Identifier iw/MsgBox\n5:9-13 String m/".*[^\\\\]"\n6:0-3 Statement iw/End\n6:4-6 Statement iw/If\n'

# The plain-text rules are for mail: headers, quoting depth by alternating
# colour, and a bare URL.
case_ 'text: mail headers and quoting' t.txt \
'From: me@x.example\nSubject: hi\n\n> quoted\n>> deeper\nsee http://x.example/p\n' \
'1:0-18 Type i/^[-a-z]*From: .*$\n2:0-11 String i/^Subject: .*$\n4:0-8 Comment n/^\\s*[|>].*\n5:0-9 Identifier n/^\\s*[|>]\\s*[|>].*\n6:4-22 Url n/http:\\/\\/[-:&\\~\\%_?=/\\.a-zA-Z0-9]*\n'

case_ 'Java: types, character, numbers' T.java \
'// c\npackage a.b;\npublic class T {\n    static char c = \047x\047;\n    static double d = 1.5;\n    static int n = 0x1f;\n}\n' \
'1:0-4 Comment n/\\/\\/.*$\n2:0-7 Include w/package\n3:0-6 StorageClass w/public\n3:7-12 Typedef w/class\n4:4-10 StorageClass w/static\n4:11-15 Type w/char\n4:20-23 Character m/\047.*[^\\\\]\047\n5:4-10 StorageClass w/static\n5:11-17 Type w/double\n5:22-25 Float n/\\d*\\.\\d\\+\n6:4-10 StorageClass w/static\n6:11-14 Type w/int\n6:19-23 Number w/0x\\x\\+\n'

case_ 'INI: comment, section, key' t.ini \
'; c\n[core]\nname = jvim\n' \
'1:0-3 Comment n/^;.*\n2:0-6 Tag n/\\[.*\\]\n3:0-5 Identifier m-/^\\s*[^ ;=]\\+\\s*=\n'

case_ 'module definition: the export table' t.def \
'; c\nLIBRARY jvim\nEXPORTS\n  Foo @1\n' \
'1:0-3 Comment n/;.*\n2:0-7 Keyword iw/LIBRARY\n3:0-7 Keyword iw/EXPORTS\n4:7-8 Number i/\\d\\+\n'

# .ec is embedded SQL in C, and filetype.jvsyn sources c.jvsyn as well as
# ec.jvsyn for it -- the comment and "int" below come from the first.
case_ 'embedded SQL: two rule files at once' t.ec \
'/* c */\nEXEC SQL BEGIN DECLARE SECTION;\nint n;\nEXEC SQL END DECLARE SECTION;\nEXEC SQL COMMIT;\n' \
'1:0-7 Comment p/\\/\\*\n2:0-31 Tag n/\\s*EXEC\\s\\+SQL\\s\\+BEGIN\\s\\+DECLARE\\s\\+SECTION\\s*;\n3:0-3 Type w/int\n4:0-29 Tag n/\\s*EXEC\\s\\+SQL\\s\\+END\\s\\+DECLARE\\s\\+SECTION\\s*;\n5:0-8 Tag w/EXEC\\s\\+SQL\n5:9-15 Keyword w/COMMIT\n'

case_ 'resource script: dialog, control, string' t.rc \
'// c\n#include <windows.h>\nIDD_MAIN DIALOG\nBEGIN\n  LTEXT "hi", 7\nEND\n' \
'1:0-4 Comment n/\\/\\/.*$\n2:0-8 Include n/^\\s*#\\s*include\n3:9-15 StorageClass w/DIALOG\n4:0-5 Statement w/BEGIN\n5:2-7 Structure w/LTEXT\n5:8-12 String m/".*"\n5:14-15 Number w/\\d\\+\n6:0-3 Statement w/END\n'

# An rc colours its own options, and tells "set x" from "set nox". A comment
# there is two quotes: one is how a rule writes a quote of its own.
case_ 'an rc: set, unset, comment' jvimrc \
'"" c\nset autoindent\nset ts=4\nset noerrorbells\n' \
'1:0-4 Comment n/"".*\n2:0-3 SetSpecial n/^set\n2:4-14 SetCommand w/autoindent\n3:0-3 SetSpecial n/^set\n3:4-6 SetShortCmd w/ts\n4:0-3 SetSpecial n/^set\n4:4-16 UnsetCommand w/noerrorbells\n'

# The shipped samples are the file most people see an rc in, and their names
# are not the names an rc is read from, so they need saying separately.
case_ 'the sample rc is coloured under its own name' _jvimrc.sample \
'set autoindent\n" a note\n' \
'1:0-3 SetSpecial n/^set\n1:4-14 SetCommand w/autoindent\n2:0-8 Comment n/^\\s*".*\n'

# A rule file is the same language as the "syntax" lines of an rc, so it reads
# the rc rules. Every group name is drawn in its own group and every colour
# name in its own colour, which is what makes a rule file worth looking at:
# "Error" is red here because Error is red.
#
# "white" is drawn on grey. It is the one colour name that cannot be shown in
# itself on the window's own background, and a rule can say what goes behind it
# now, so it does.
case_ 'a rule file is coloured in the colours it names' t.jvsyn \
'syntax link Error bolic white on maroon\n" a note\nsyntax Comment n/x\n' \
'1:0-6 SetSpecial n/^syntax\n1:7-11 SetSpecial w/link\n1:12-17 Error w/Error\n1:18-23 - w/bolic\n1:24-29 - w/white\n1:30-32 SetSpecial w/on\n1:33-39 - w/maroon\n2:0-8 Comment n/^\\s*".*\n3:0-6 SetSpecial n/^syntax\n3:7-14 Comment w/Comment\n'

# The names that belong to one rule file each. A .jvsyn is opened with
# common.jvsyn and jvimrc.jvsyn and nothing else, so while DiffAdd was defined
# inside diff.jvsyn it was not a colour on the screen when diff.jvsyn was the
# file being read, and the line that gave it its colour came out plain. They
# are all in common.jvsyn now, which is where every other group name is.
#
# E-Mail is written out as \< \> rather than as a w rule. A w rule is looked up
# in an index of the line built one character class at a time, and "E-Mail" is
# three runs of the line, so the whole of it is never a key to look up.
case_ 'a rule file colours the names only one rule file uses' t.jvsyn \
'syntax link DiffAdd green on #e6ffe6\nsyntax HtmlArg itw/x\nsyntax E-Mail n/x\nsyntax Value w/y\n' \
'1:0-6 SetSpecial n/^syntax\n1:7-11 SetSpecial w/link\n1:12-19 DiffAdd w/DiffAdd\n1:20-25 - w/green\n1:26-28 SetSpecial w/on\n1:29-36 Constant n/#\\x\\x\\x\\x\\x\\x\n2:0-6 SetSpecial n/^syntax\n2:7-14 HtmlArg w/HtmlArg\n3:0-6 SetSpecial n/^syntax\n3:7-13 E-Mail n/\\<E-Mail\\>\n4:0-6 SetSpecial n/^syntax\n4:7-12 Value w/Value\n'

# A quote on its own line is a comment; a quote inside a pattern is not. Rule
# files write \" constantly -- this very case is one -- and a comment rule that
# took any quote would paint the pattern of every String rule as a comment.
case_ 'a quote inside a pattern does not start a comment' t.jvsyn \
'syntax String m/\\".*[^\\\\]\\"\n' \
'1:0-6 SetSpecial n/^syntax\n1:7-13 String w/String\n'

# The block markers keep the quote in front of them, so they win the tie with
# the comment rule: both start at column 0, and the earlier rule takes it.
case_ 'a begin block is not swallowed by the comment rule' t.jvsyn \
'"begin suffixes=.c\n"source $VIM/syntax/c.jvsyn\n"end   suffixes\n' \
'1:0-18 PreCondit n/^"\\=begin\\s\\+suffixes\\s*=.*\n2:0-27 Comment n/^\\s*".*\n3:0-15 PreCondit n/^"\\=end\\s\\+suffixes\n'

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

# "on" takes a colour a foreground would take, or the colour itself.
case_ 'a rule can say what goes behind it' t.unknown \
'def x\n' \
'1:0-3 - w/def\n' \
':syntax red on #ffe6e6 w/def\r'

# "text" is whatever the ordinary text colour is and "reverse" asks the
# terminal to swap two colours over. Neither names a colour, so neither can be
# behind anything, and a rule that says so is refused rather than invented for.
case_ 'reverse is refused as a thing to go behind text' t.unknown \
'def x\n' \
'' \
':syntax red on reverse w/def\r'

# How many backslashes reach the regexp, which is the whole of what "|" does in
# a rule. ":syntax" is an ex command and the ex line eats one backslash, so the
# three forms are three different rules. The README claimed the last of them
# was impossible until someone tried two backslashes.
#
# None: the command stops at the pipe. The rule is n/aaa and "ccc" is run as a
# command of its own, which is not one, so it does nothing and says so.
case_ 'a bare pipe ends the command, not the pattern' t.unknown \
'aaa bbb ccc\n' \
'1:0-3 - n/aaa\n' \
':syntax red n/aaa|ccc\r'

# One: the regexp gets a plain pipe and matches a pipe.
case_ 'one backslash before a pipe matches a pipe' t.unknown \
'a|b zz\n' \
'1:0-3 - n/a|b\n' \
':syntax red n/a\\|b\r'

# Two: the regexp gets \| and branches. Both ends match, and the "bbb" between
# them does not.
case_ 'two backslashes before a pipe make an alternation' t.unknown \
'aaa bbb ccc\n' \
'1:0-3 - n/aaa\\|ccc\n1:8-11 - n/aaa\\|ccc\n' \
':syntax red n/aaa\\\\|ccc\r'

# A w rule wraps its pattern in \< \>, so an alternation inside one groups as
# "\<a" or "b\>" and colours the a in "ab". The list notation is what w is for,
# and it is also the faster of the two: each word becomes a rule looked up in a
# per-line index, where an alternation has to be run at every position.
case_ 'a w list does not colour the halves of a longer word' t.unknown \
'a b ab\n' \
'1:0-1 - w/a\n1:2-3 - w/b\n' \
':syntax red w/a/b\r'

if [ -n "$count_only" ]; then
	printf 'cases %d\n' "$cases"
	exit 0
fi

echo
printf 'cases %d\n' "$cases"
printf 'pass %d  fail %d\n' "$pass" "$fail"
[ "$fail" -eq 0 ] || exit 1
exit 0
