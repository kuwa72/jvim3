#!/usr/bin/env bash
#
# What the terminal is actually sent: the SGR escapes, and the text under them.
#
#   scripts/test-sgr.sh [jvim-binary]
#
# scripts/test-syntax.sh asks ":syntax dump" which rule coloured which bytes.
# That is a question about the rules. This is the other half -- whether the
# colour the rules asked for reaches the terminal, and reaches only the cells
# it was meant for. Nothing else in the tree looks at the painter: the Windows
# suite drives the GUI, and the dump never draws.
#
# It found something the first time it was run. jvim redraws only the cells
# that changed, and when the cursor has to move a few columns it types the
# cells in between instead of positioning (screen_char(), screen.c). Those
# cells are typed in whatever colour is current, not their own, so an unchanged
# space in front of a coloured word came out inside the word's colour. With a
# foreground that is invisible -- a space has no ink. It will not be invisible
# once a rule can ask for a background.
#
# The output compared is one line per SGR escape:
#
#     [0;1;38;2;46;139;87m|int
#     [m| x =
#
# the escape, a '|', and the text written while it was in force. Text written
# before any escape gets a line with nothing in front of the '|'. Cursor
# positioning is dropped, so a case does not break when a line moves. The '|'
# is there to make a leading space visible, which is exactly the bug above.
#
# A case's text must not contain '~': the filler down the left of an empty
# window is where the comparison stops.

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
	printf 'set fexrc\nset syntax\nsource $VIM/syntax/filetype.jvsyn\n' \
			> "$tmp/.jvimrc"
fi

pass=0; fail=0; cases=0

counted() {
	cases=$((cases+1))
	[ -n "$count_only" ]
}

# Everything jvim wrote, as escape-and-text lines.
#
# RS is the escape character, so each record is one escape and the text after
# it. An SGR escape (the ones ending in 'm') starts a new output line; every
# other escape -- cursor motion, the window title, the alternate screen -- has
# its own shape stripped off and only its text kept.
#
# Two markers bound what is kept. Clearing the display throws away everything
# written so far, and jvim does that twice on the way in -- so does the line
# collected so far. "Thanks for flying Vim" is the last thing it says, and the
# clear that follows it is a teardown, not a redraw.
#
# No escape character appears in a regexp here, only in strings built with
# sprintf(). The awk on DragonFly is the one true awk, which does not read
# "\007" inside a bracket expression the way gawk and mawk do: the window title
# then went unrecognised, "Thanks for flying" with it, and the clear at the end
# of the teardown threw the whole run away. Every case came out empty and
# nothing said why.
normalise() {
	LC_ALL=C awk '
		BEGIN {
			ESC = sprintf("%c", 27); BEL = sprintf("%c", 7)
			CR  = sprintf("%c", 13); NL  = sprintf("%c", 10)
			RS = ESC; ORS = ""; n = 1; out[1] = "|"; done = 0
		}
		NR > 1 {
			if (done)
				next
			if (index($0, "Thanks for flying") > 0)
			{
				done = 1			# the teardown, not another redraw
				next
			}
			r = $0
			if (match(r, /^\[[0-9;]*m/))
			{
				out[++n] = substr(r, RSTART, RLENGTH) "|"
				r = substr(r, RSTART + RLENGTH)
			}
			else if (match(r, /^\[[0-9;?]*[A-Za-z]/))
			{
				csi = substr(r, RSTART, RLENGTH)
				r = substr(r, RSTART + RLENGTH)
				if (csi == "[2J") { n = 1; out[1] = "|" }
			}
			else if (substr(r, 1, 3) == "]2;")		# the window title
			{
				b = index(r, BEL)
				r = (b > 0) ? substr(r, b + 1) : ""
			}
			else
				sub(/^[=>78]/, "", r)		# keypad, save, restore
			gsub(CR, "", r)
			gsub(NL, "", r)
			out[n] = out[n] r
		}
		END { for (i = 1; i <= n; i++) print out[i] "\n" }' |
		sed 's/~.*$//' |			# the filler down an empty window
		grep -v '^|$'				# and a run with neither escape nor text
}

# case <name> <file name> <source> <wanted> [COLORTERM]
#
# The file name decides which rules are read, so it carries the real suffix.
# COLORTERM defaults to truecolor; a case passes something else to ask for the
# nearest of the sixteen a terminal has always had.
case_() {
	counted && return
	local name=$1 file=$2 src=$3 want=$4 ct=${5:-truecolor}

	printf '%b' "$src" > "$tmp/$file"
	printf '%b' "$want" > "$tmp/want"
	printf ':q!\r' > "$tmp/keys"
	# To a file and then through the filter, rather than down a pipe: an empty
	# result is otherwise two questions at once, and the byte count below
	# answers the first of them.
	"$tmp/ptyrun" /bin/sh -c \
		"HOME=$tmp VIM=$root TERM=xterm COLORTERM=$ct \
			$jvim -T xterm -s $tmp/keys $tmp/$file" \
		> "$tmp/raw" 2>/dev/null
	normalise < "$tmp/raw" > "$tmp/out"

	if cmp -s "$tmp/want" "$tmp/out"; then
		printf '  PASS        %s\n' "$name"; pass=$((pass+1))
	else
		printf '  FAIL        %s\n' "$name"
		diff -u "$tmp/want" "$tmp/out" | sed -n '3,14p' | sed 's/^/                /'
		if [ ! -s "$tmp/out" ]; then
			printf '                %s bytes came back from the editor\n' \
					"$(wc -c < "$tmp/raw" | tr -d ' ')"
			od -c "$tmp/raw" | sed -n '1,3p' | sed 's/^/                /'
		fi
		fail=$((fail+1))
	fi
}

if [ -z "$count_only" ]; then
	echo
	echo "what the terminal is sent:"
fi

# The colour of a run belongs to the run and to nothing next to it. The space
# in front of "1" is not part of the Number match -- ":syntax dump" says
# "1:8-9 Number w/\d\+" -- so it must not be typed in the Number colour.
case_ 'C: a run is coloured, and only the run' t.c \
'int x = 1;\n' \
'[0;1;38;2;46;139;87m|int\n[m| x =\n[0;38;2;255;0;255m|1\n[m|;\n'

# A comment reaching to the end of the line, and the line after it plain again.
#
# The "[0m" is not a reset that lost its colour: it is the ordinary text
# colour, id 'A', which is what is_syntax() answers for the run of a line that
# no rule matches. The empty "[m|" before it is the stop that precedes it.
#
# The space between "int" and "y" is not sent at all. It did not change -- the
# window was cleared to spaces -- so there is nothing to draw, and the cursor
# steps over it. Before run_iscolor() it was retyped, in the wrong colour.
case_ 'C: a comment ends where the line does' t.c \
'/* c */\nint y;\n' \
'[0;38;2;0;0;255m|/* c */\n[m|\n[0;1;38;2;46;139;87m|int\n[m|\n[0m|y;\n[m|\n'

# Without a terminal that says it can take a colour, the nearest of the sixteen.
# 38;2 must not appear at all: a terminal that cannot read it prints it.
#
# 90 for SeaGreen (#2e8b57) is not a typo. sgr_nearest() measures plain
# distance in RGB and xterm's bright black (#7f7f7f) really is nearer to it
# than any of the greens. A desaturated colour reduced that way goes grey; the
# comment on sgr_nearest() says what it does and this is it.
case_ 'a terminal with no COLORTERM gets one of sixteen' t.c \
'int x;\n' \
'[0;1;90m|int\n[m|\n[0m|x;\n[m|\n' \
'xterm-256color'

# ---------------------------------------------------------- what is behind it
#
# 48;2 is 38;2 with a background instead of a foreground, and it goes in the
# same escape: one run of text, one escape, both of its colours.
#
# The tints are written into diff.jvsyn as "#e6ffe6" and "#ffe6e6". None of the
# sixteen named colours is pale enough to read text off, and these two rules
# match a whole line.
case_ 'diff: the added and the removed line are tinted' t.diff \
'-gone\n+added\n' \
'[0;38;2;255;0;0;48;2;255;230;230m|-gone\n[m|\n[0;38;2;0;128;0;48;2;230;255;230m|+added\n[m|\n'

# The same two SGR numbers ten higher, which is the whole of the difference
# between a foreground and a background. 47 is white: #e6ffe6 reduced to the
# sixteen is nearer white than any green, and a pale tint always will be.
case_ 'a background falls back to one of sixteen too' t.diff \
'+added\n' \
'[0;32;47m|+added\n[m|\n' \
'xterm-256color'

# What "syntax link Todo bold navy on yellow" is for. The rule said "reverse"
# before, because a background was the one thing a rule could not ask for, and
# reverse is the terminal swapping two colours it already has -- which is not
# blue on yellow and is not the same twice on two different terminals.
case_ 'Todo is drawn on a colour, not by swapping two' t.md \
'x TODO x\n' \
'|x\n[0;1;38;2;0;0;128;48;2;255;255;0m|TODO\n[m|\n[0m|x\n[m|\n'

# A group with a background and no foreground at all: the text keeps the colour
# it would have had, and only what is behind it changes.
case_ 'a fenced block changes only what is behind it' t.md \
'a\n```\nc\n```\n' \
'|a\n[0;48;2;240;240;240m|```\n[m|\n[0;48;2;240;240;240m|c\n[m|\n[0;48;2;240;240;240m|```\n[m|\n'

if [ -z "$count_only" ]; then
	echo
fi
echo "cases $cases"
[ -n "$count_only" ] && exit 0
echo "pass $pass  fail $fail"
[ "$fail" -eq 0 ]
