#!/usr/bin/env bash
#
# Hostile-input tests for JVim: what it does when the input is not what it was
# meant to be given.
#
#   scripts/test-hostile.sh [jvim-binary]
#
# The other four suites feed the editor input it is meant to accept and check
# that the right bytes come out. This one feeds it input nobody intended -- a
# file that is one enormous line, bytes that are not text, a multi-byte
# sequence cut in half by the end of the file, a regexp deep enough to exhaust
# a recursive matcher, a colour scheme written by somebody who did not read the
# manual -- and checks that the editor comes through it.
#
# Coming through it means exiting, and then one of:
#
#   the bytes it wrote are the bytes it should have written, or
#   the file is untouched, because the right answer to this input is an error
#   message and then business as usual.
#
# Both of those require it to exit. A case that leaves the editor waiting for a
# key fails here even though nothing crashed: ptyrun kills the child after
# PTYRUN_TIMEOUT seconds and the case says so, which is also how this suite
# notices that something has become too slow to finish at all.
#
# The input files come from scripts/hostilegen.c rather than from awk or dd:
# five operating systems run this, and their awks do not agree about a zero
# byte. jvim is given a terminal by scripts/ptyrun.c, as in the other suites.

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
	for src in ptyrun hostilegen; do
		${CC:-cc} -o "$tmp/$src" "$root/scripts/$src.c" 2>"$tmp/cc.err" || {
			echo "cannot build $root/scripts/$src.c:" >&2
			cat "$tmp/cc.err" >&2
			exit 2
		}
	done
fi

pass=0; fail=0; xfail=0; xpass=0; cases=0

# Counted in both modes, so "cases" is the number of cases in this file whether
# or not they were run, and stays right when a KNOWN-FAIL is added back.
counted() {
	cases=$((cases+1))
	[ -n "$count_only" ]
}

# --------------------------------------------------------------------- specs
#
# A case says what its file holds rather than holding it, because two million
# x's do not belong in the middle of a test. write_spec turns one of these into
# a file:
#
#   lit:<text>          printf %b, so \n and \xff work
#   rep:<text>:<n>      the text n times
#   repnl:<text>:<n>    the text n times, then one newline
#   allbytes:<n>        0x00 .. 0xff, n times
#   badutf              invalid and truncated UTF-8, from hostilegen
#
# and two more that only make sense for the "want" side:
#
#   same                whatever the input was: nothing should have changed
#   nonempty            it wrote something, and that is all this case claims
write_spec() {
	local spec=$1 path=$2

	case $spec in
	lit:*)		printf '%b' "${spec#lit:}" > "$path" ;;
	repnl:*)	spec=${spec#repnl:}
				"$tmp/hostilegen" repeat "${spec%:*}" "${spec##*:}" > "$path"
				printf '\n' >> "$path" ;;
	rep:*)		spec=${spec#rep:}
				"$tmp/hostilegen" repeat "${spec%:*}" "${spec##*:}" > "$path" ;;
	allbytes:*)	"$tmp/hostilegen" allbytes "${spec#allbytes:}" > "$path" ;;
	badutf)		"$tmp/hostilegen" badutf > "$path" ;;
	*)			echo "test-hostile.sh: unknown spec '$spec'" >&2; exit 2 ;;
	esac
}

# A pattern too long to write out is written as @rep:a:1015@ in the keys, and
# expanded here. Only ever "one string, n times", which is all a case has
# needed: what matters about these patterns is their length.
expand_keys() {
	local keys=$1 token what n

	while :; do
		token=$(printf '%s\n' "$keys" | sed -n 's/.*\(@rep:[^:]*:[0-9]*@\).*/\1/p')
		[ -n "$token" ] || break
		what=${token#@rep:}; what=${what%%:*}
		n=${token%@}; n=${n##*:}
		keys=${keys//"$token"/$("$tmp/hostilegen" repeat "$what" "$n")}
	done
	printf '%s' "$keys"
}

# ---------------------------------------------------------------------- cases
#
# case_ <name> <expect: ok|knownfail> <in spec> <want spec> <keys>
#
# The keys go through -s. "%s" in them is the temporary directory, so a case
# can write to %s/out, which is what is compared. ARGS, ENV_ and T are set in
# front of a call rather than passed, because most cases want none of them:
#
#   ARGS=-b     extra arguments to jvim, e.g. binary mode
#   ENV_=...    environment for it, e.g. ENV_=LANG=ja_JP.UTF-8
#   T=10        seconds this case is allowed; ptyrun's default is 20
#
# HOME is $tmp, which holds no rc unless the case put one there: a ~/.jvimrc
# belonging to whoever is running this must not be able to change the answer.
case_() {
	counted && return
	local name=$1 expect=$2 in_spec=$3 want_spec=$4 keys=$5

	write_spec "$in_spec" "$tmp/in"
	case $want_spec in
	same)		cp "$tmp/in" "$tmp/want" ;;
	nonempty)	rm -f "$tmp/want" ;;
	*)			write_spec "$want_spec" "$tmp/want" ;;
	esac

	rm -f "$tmp/out"
	# shellcheck disable=SC2059  # the keys are a format on purpose
	printf "$(expand_keys "$keys")" "$tmp" > "$tmp/keys"
	PTYRUN_TIMEOUT=${T:-20} "$tmp/ptyrun" /bin/sh -c \
		"HOME=$tmp TERM=xterm ${ENV_:-} $jvim -T xterm ${ARGS:-} -s $tmp/keys $tmp/in" \
		>/dev/null 2>&1

	# bash drops an assignment made in front of a function call when the
	# function returns -- but not in POSIX mode, where it would leak into every
	# case after this one, and "-b" leaking is a case that passes for the wrong
	# reason. Cleared here so that it cannot depend on how bash was started.
	ARGS=; ENV_=; T=

	local got=bad
	if [ "$want_spec" = nonempty ]; then
		[ -s "$tmp/out" ] && got=ok
	elif [ -f "$tmp/out" ] && cmp -s "$tmp/want" "$tmp/out"; then
		got=ok
	fi

	if [ "$expect" = ok ] && [ "$got" = ok ]; then
		printf '  PASS        %s\n' "$name"; pass=$((pass+1))
	elif [ "$expect" = ok ]; then
		printf '  FAIL        %s\n' "$name"
		# Sizes first: "out is one byte and in is two million" is usually the
		# whole story, and cmp names the first byte that differs, which is the
		# only other line worth having for a file this big.
		printf '                in   %s bytes\n' "$(wc -c < "$tmp/in" | tr -d ' ')"
		[ -f "$tmp/want" ] &&
			printf '                want %s bytes\n' "$(wc -c < "$tmp/want" | tr -d ' ')"
		if [ -f "$tmp/out" ]; then
			printf '                got  %s bytes\n' "$(wc -c < "$tmp/out" | tr -d ' ')"
			[ -f "$tmp/want" ] &&
				cmp "$tmp/want" "$tmp/out" 2>&1 | sed 's/^/                /'
		else
			printf '                got  nothing: the file was never written\n'
		fi
		fail=$((fail+1))
	elif [ "$got" = bad ]; then
		printf '  KNOWN-FAIL  %s\n' "$name"; xfail=$((xfail+1))
	else
		printf '  NOW PASSES  %s  <- update the expectation\n' "$name"
		xpass=$((xpass+1))
	fi
}

if [ -z "$count_only" ]; then
	echo
	echo "hostile input:"
fi

# ------------------------------------------------------------- one long line
#
# A minified .js, a one-line JSON dump, a base64 blob: two million characters
# with no line break in them is an ordinary thing to be handed, and these paths
# walk the line rather than indexing into it.

case_ '2 MB in one line: dd' ok \
	'repnl:x:2000000' 'lit:\n' \
	'dd:w! %s/out\r:q!\r'

case_ '2 MB in one line: G$x' ok \
	'repnl:x:2000000' 'repnl:x:1999999' \
	'G$x:w! %s/out\r:q!\r'

# Two million substitutions on one line, in half a second, and the ten second
# cap is the assertion. Before #22 this was quadratic: 400k characters took 53
# seconds and this size would have taken about twenty minutes, so the case is
# what says the quadratic has not come back.
T=10 case_ '2 MB in one line: :s/x/y/g finishes' ok \
	'repnl:x:2000000' 'repnl:y:2000000' \
	':s/x/y/g\r:w! %s/out\r:q!\r'

# ------------------------------------------------------------ not text at all
#
# Every byte value there is, including the zero the editor keeps as a line
# break inside the buffer and the 0x0d that decides where a line ends.

ARGS=-b case_ 'all 256 byte values: -b writes them back unchanged' ok \
	'allbytes:40' 'same' \
	':w! %s/out\r:q!\r'

# Without -b the round trip is lossy by design, so this asks only that the
# editor takes an edit and writes a file rather than falling over: 0x0d ends a
# line and 0x00 is a line break in the buffer, and no expectation about the
# bytes would mean anything.
case_ 'all 256 byte values: text mode takes an append' ok \
	'allbytes:40' 'nonempty' \
	'GA!\033:w! %s/out\r:q!\r'

# --------------------------------------------------------------- broken UTF-8
#
# KNOWN-FAIL, and the one worth reading. A file that is UTF-8 apart from a few
# bytes that are not comes back with those bytes rewritten as "?" -- and the
# valid character in front of them damaged as well, while a sequence truncated
# by the end of the file has its high bit stripped and turns into an ASCII
# letter. -b round trips the same file exactly, so what is lost is lost in the
# text mode conversion. #30.
ENV_=LANG=ja_JP.UTF-8 case_ 'broken UTF-8: written back as it came in' knownfail \
	'badutf' 'same' \
	':w! %s/out\r:q!\r'

ARGS=-b ENV_=LANG=ja_JP.UTF-8 case_ 'broken UTF-8: -b writes it back unchanged' ok \
	'badutf' 'same' \
	':w! %s/out\r:q!\r'

# ------------------------------------------------------- patterns and options
#
# syn_add() builds its regexp in pattern[1024] on the stack, and CMDBUFFSIZE is
# 1024, so a pattern typed on the ":" line is as close to the end of that
# buffer as anything can get.
#
# The extra \r before the write is the "Press RETURN or enter command to
# continue" that an error message puts up: it eats what follows it, and in
# normal mode a return only moves the cursor down a line, so a case that
# expects errors can send one per error and be no worse off.

case_ 'a 1015 character pattern to :syntax' ok \
	'lit:aa BEG bb END cc\n' 'same' \
	':syntax red w/@rep:a:1015@\r\r:w! %s/out\r:q!\r'

case_ 'two 1000 character patterns to a pair rule' ok \
	'lit:aa BEG bb END cc\n' 'same' \
	':syntax red p/@rep:a:1000@/@rep:b:1000@\r\r:w! %s/out\r:q!\r'

# A recursive matcher given something to recurse on. The right answer is
# "Too many (" and not a stack that runs out.
case_ '60 nested groups in :s' ok \
	'lit:hello\n' 'same' \
	':s/@rep:\\\\(:60@a@rep:\\\\):60@/b/\r\r\r:w! %s/out\r:q!\r'

case_ 'a pattern that can match a prefix many ways' ok \
	'repnl:x:400000' 'same' \
	':s/\\(a*\\)*b/x/\r\r:w! %s/out\r:q!\r'

case_ 'a tabstop of two billion' ok \
	'lit:one\ntwo\n' 'same' \
	':set ts=2000000000\r\r:w! %s/out\r:q!\r'

# KNOWN-FAIL. set_indent() inserts the indent one character at a time with
# inschar(), and each of those rewrites the line, so the work is quadratic in
# the size of the indent: 1M takes 0.85s and 10M does not finish. Worse than
# #22, because the loop has no breakcheck() in it and CTRL-C cannot stop it.
# #30.
T=10 case_ 'a shiftwidth of ten million, then >>' knownfail \
	'lit:\tone\n' 'nonempty' \
	':set sw=10000000\r>>:w! %s/out\r:q!\r'

# -------------------------------------------------------------- a lot of lines
#
# The same work in the other shape it comes in: 200k lines, one substitution
# each, rather than one line with 200k of them.

case_ '200k lines: :g/foo/s//baz/' ok \
	'rep:foo bar
:200000' 'rep:baz bar
:200000' \
	':g/foo/s//baz/\r:w! %s/out\r:q!\r'

# ---------------------------------------------------------------- a rule file
#
# A colour scheme every token of which is longer than the command line can
# hold. "Command too long", repeated, is the right answer, and the editor is
# still usable afterwards. The scheme has to exist before the case runs, and
# the case has to be counted whether or not it runs, so the writing is guarded
# and the call is not.
if [ -z "$count_only" ]; then
	mkdir -p "$tmp/.jvim/colors"
	{
		printf 'syntax link '
		"$tmp/hostilegen" repeat A 3000
		printf ' '
		"$tmp/hostilegen" repeat B 3000
		printf '\n'
		printf 'hi Normal guifg='
		"$tmp/hostilegen" repeat z 2000
		printf '\n'
		printf 'syntax w /'
		"$tmp/hostilegen" repeat q 3000
		printf '/ red\n'
	} > "$tmp/.jvim/colors/junk.vim"
fi

case_ 'a colour scheme of 3000 character tokens' ok \
	'lit:hello\n' 'same' \
	':colorscheme junk\r\r\r\r\r\r:w! %s/out\r:q!\r'

if [ -n "$count_only" ]; then
	printf 'cases %d\n' "$cases"
	exit 0
fi

rm -rf "$tmp/.jvim"

echo
printf 'cases %d\n' "$cases"
printf 'pass %d  fail %d  known-fail %d  newly-passing %d\n' \
		"$pass" "$fail" "$xfail" "$xpass"
[ "$fail" -eq 0 ] || exit 1
exit 0
