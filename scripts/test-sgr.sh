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
# the escape, a '|', and the text written while it was in force. Cursor
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
normalise() {
	LC_ALL=C awk '
		BEGIN { RS = "\033"; ORS = "" }
		NR > 1 {
			if (match($0, /^\[[0-9;]*m/))
			{
				printf "\n%s|", substr($0, RSTART, RLENGTH)
				r = substr($0, RSTART + RLENGTH)
			}
			else
			{
				r = $0
				sub(/^\[[0-9;?]*[A-Za-z]/, "", r)		# CSI
				sub(/^\]2;[^\007]*\007/, "", r)			# the window title
				sub(/^[=>78]/, "", r)					# keypad, save, restore
			}
			gsub(/\r|\n/, "", r)
			printf "%s", r
		}
		END { print "" }' |
		grep '|' |					# drop what came before the first colour
		sed 's/~.*$//'				# and the filler down an empty window
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
	"$tmp/ptyrun" /bin/sh -c \
		"HOME=$tmp VIM=$root TERM=xterm COLORTERM=$ct \
			$jvim -T xterm -s $tmp/keys $tmp/$file" \
		2>/dev/null | normalise > "$tmp/out"

	if cmp -s "$tmp/want" "$tmp/out"; then
		printf '  PASS        %s\n' "$name"; pass=$((pass+1))
	else
		printf '  FAIL        %s\n' "$name"
		diff -u "$tmp/want" "$tmp/out" | sed -n '3,14p' | sed 's/^/                /'
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

if [ -z "$count_only" ]; then
	echo
fi
echo "cases $cases"
[ -n "$count_only" ] && exit 0
echo "pass $pass  fail $fail"
[ "$fail" -eq 0 ]
