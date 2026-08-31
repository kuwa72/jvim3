#!/usr/bin/env bash
#
# Check that the numbers and platform names written in the documentation still
# match the tree they describe.
#
#   scripts/check-docs.sh                 check against the repository as it is
#   scripts/check-docs.sh --version 1.2.3 also check VERSION and the CHANGELOG
#                                         for a release about to be tagged
#
# This exists because those numbers drifted, badly, and silently: two READMEs
# disagreed with each other about which release was the latest, the test count
# appeared as 42/58/100 in some files and 46/64/110 in others, the Japanese
# README still claimed six operating systems after macOS was dropped, and the
# release notes -- pasted into every release -- said tests passed on a platform
# that was no longer built. None of that is catchable by reading; all of it is
# catchable by asking the suites and the workflow what is actually true.
#
# It reports every mismatch it finds and then exits 1. It never edits anything:
# the fix is a human's, or an agent's, edit.

set -uo pipefail

root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"

want_version=
while [ $# -gt 0 ]; do
	case $1 in
	--version)	want_version=${2#v}; shift 2 ;;
	-h|--help)	sed -n '3,20p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
	*)			echo "unknown argument: $1" >&2; exit 2 ;;
	esac
done

problems=0
bad() { printf '%s\n' "$*" >&2; problems=$((problems+1)); }

# The prose documents. Anything quoting a count or a platform belongs here.
DOCS="README.md README.ja.md BUILDING-unix.md BUILDING-mingw.md BUILDING.ja.md
      USAGE.md USAGE.ja.md CONTRIBUTING.md CONTRIBUTING.ja.md CHANGELOG.md
      RELEASING.md .github/release-body.md"
docs=""
for d in $DOCS; do
	[ -f "$d" ] && docs="$docs $d"
done

# ---------------------------------------------------------------- test counts
# Asked of the suites themselves rather than counted by grep: a case can be
# written over several lines, and a KNOWN-FAIL is still a case.
enc=$(COUNT_ONLY=1 ./scripts/test-encoding.sh 2>/dev/null | sed -n 's/^cases //p')
edt=$(COUNT_ONLY=1 ./scripts/test-editing.sh  2>/dev/null | sed -n 's/^cases //p')
win=$(COUNT_ONLY=1 ./scripts/test-winkeys.sh  2>/dev/null | sed -n 's/^cases //p')
syn=$(COUNT_ONLY=1 ./scripts/test-syntax.sh   2>/dev/null | sed -n 's/^cases //p')
sgr=$(COUNT_ONLY=1 ./scripts/test-sgr.sh      2>/dev/null | sed -n 's/^cases //p')
hos=$(COUNT_ONLY=1 ./scripts/test-hostile.sh  2>/dev/null | sed -n 's/^cases //p')
case ${enc:-x}${edt:-x}${syn:-x}${sgr:-x}${hos:-x} in
*x*)	bad "cannot get the case counts from the test suites (enc='$enc' edt='$edt' syn='$syn' sgr='$sgr' hos='$hos')"
		enc=0; edt=0; syn=0; sgr=0; hos=0 ;;
esac
# What "build-unix.sh test" runs, which is what a document saying "the tests"
# means. The Windows suite is not in it: it needs Windows.
total=$((enc + edt + syn + sgr + hos))

echo "suites say: encoding $enc, editing $edt, syntax $syn, sgr $sgr, hostile $hos, total $total${win:+, winkeys $win}"

# Every "<n> cases" / "<n> tests" / "<n> ケース" / "<n> 個のテスト" in the prose
# has to be one of those numbers, and where the line names a particular suite it
# has to be that suite's number.
allowed=" $enc $edt $syn $sgr $hos $total ${win:-} "
# The first released heading in the CHANGELOG. Anything at or below it is a
# record of what that release shipped, and stays true by not being touched --
# adding a test today does not change how many 1.0.0 had.
chlog_history=$(sed -n '/^## [0-9]/{=;q;}' CHANGELOG.md 2>/dev/null)

while IFS=: read -r file line text; do
	[ -n "${file:-}" ] || continue
	if [ "$file" = CHANGELOG.md ] && [ -n "${chlog_history:-}" ] &&
	   [ "$line" -ge "$chlog_history" ]; then
		continue
	fi
	n=$(printf '%s\n' "$text" |
		sed -n 's/.*[^0-9]\([0-9][0-9]*\) *\(cases\|tests\|ケース\|個のテスト\).*/\1/p' |
		head -1)
	[ -n "$n" ] || continue
	case $text in
	*test-encoding*)
		[ "$n" = "$enc" ] ||
			bad "$file:$line: says $n, should be $enc (the encoding suite)" ;;
	*test-editing*)
		[ "$n" = "$edt" ] ||
			bad "$file:$line: says $n, should be $edt (the editing suite)" ;;
	*test-winkeys*)
		[ -z "$win" ] || [ "$n" = "$win" ] ||
			bad "$file:$line: says $n, should be $win (the Windows key suite)" ;;
	*test-syntax*)
		[ -z "$syn" ] || [ "$n" = "$syn" ] ||
			bad "$file:$line: says $n, should be $syn (the syntax suite)" ;;
	*test-sgr*)
		[ -z "$sgr" ] || [ "$n" = "$sgr" ] ||
			bad "$file:$line: says $n, should be $sgr (the SGR suite)" ;;
	*test-hostile*)
		[ -z "$hos" ] || [ "$n" = "$hos" ] ||
			bad "$file:$line: says $n, should be $hos (the hostile-input suite)" ;;
	*)
		case $allowed in
		*" $n "*)	;;
		*)			bad "$file:$line: says $n, which is not $enc, $edt, $syn, $sgr, $hos or $total" ;;
		esac ;;
	esac
done < <(grep -nE '[0-9]+ *(cases|tests|ケース|個のテスト)' $docs 2>/dev/null)

# The grep above reads one line at a time, so a count that a paragraph wrapped
# between the number and the word is invisible to it. That is not theory: "All
# 100\ntests on each" sat in BUILDING-unix.md through two suites growing. Say so
# rather than widen the match, because the fix is to keep the pair on one line
# where the check above can see it, not to have two ways of reading a count.
while read -r msg; do
	[ -n "$msg" ] && bad "$msg"
done < <(for f in $docs; do
	[ -f "$f" ] || continue
	awk -v f="$f" '
		NR > 1 && prev ~ /[^0-9][0-9]+[ \t]*$/ &&
		$0 ~ /^(cases|tests|ケース|個のテスト)([^a-zA-Z]|$)/ {
			printf "%s:%d: a count is split across the line end, where the check above cannot see it -- keep the number and the word together\n", f, NR - 1
		}
		{ prev = $0 }' "$f"
done)

# ------------------------------------------------------------ platform claims
# The release job's "needs:" is the definition of what gates a release, so it is
# the definition of what CI covers.
needs=$(sed -n 's/^ *needs: *\[\(.*\)\].*/\1/p' .github/workflows/build.yml | head -1)
if [ -z "$needs" ]; then
	bad ".github/workflows/build.yml: cannot find the release job's needs: list"
else
	oses=""
	for job in $(printf '%s\n' "$needs" | tr ',' ' '); do
		case $job in
		unix)		oses="$oses Linux" ;;
		freebsd)	oses="$oses FreeBSD" ;;
		netbsd)		oses="$oses NetBSD" ;;
		openbsd)	oses="$oses OpenBSD" ;;
		dragonfly)	oses="$oses DragonFly" ;;
		windows|docs|release)	;;
		# Not systems: they run on the Linux runner and say something about the
		# build rather than about a platform, so they gate a release without
		# adding to the count of operating systems the documents claim.
		asan|ubsan)	;;
		*)			bad ".github/workflows/build.yml: job '$job' has no name in $0" ;;
		esac
	done
	n_os=$(printf '%s\n' $oses | grep -c .)
	echo "workflow says: $n_os systems run the suites --$oses"

	# Each one has to be named in both READMEs.
	for os in $oses; do
		for f in README.md README.ja.md; do
			grep -q "$os" "$f" ||
				bad "$f: does not mention $os, which CI builds and tests"
		done
	done

	# And nothing that is not built may appear in a claim about coverage. Kept
	# narrow on purpose: BUILDING-unix.md discusses macOS as a build target and
	# its history quite legitimately, and must not trip this.
	while IFS=: read -r file line text; do
		[ -n "${file:-}" ] || continue
		case $text in
		*CI*|*tests*|*pass*|*Platforms*|*対応環境*|*環境で*|*operating\ systems*|*テスト通過*)
			bad "$file:$line: names a platform CI does not build, in a coverage claim" ;;
		esac
	done < <(grep -nE 'macOS|Mac OS|OS X|Darwin' $docs 2>/dev/null |
		grep -vE 'not (a target|in CI|verified)|nobody|dropped|外し|対象では')

	# The spelled-out count, which is what said "six" after macOS went.
	word=$(case $n_os in
		4) echo four ;; 5) echo five ;; 6) echo six ;; 7) echo seven ;; *) echo "" ;;
		esac)
	if [ -n "$word" ]; then
		while IFS=: read -r file line text; do
			[ -n "${file:-}" ] || continue
			case $text in
			*"$word operating systems"*)	;;
			*)	bad "$file:$line: should say $word operating systems ($n_os are built)" ;;
			esac
		done < <(grep -nE '(four|five|six|seven) operating systems' $docs 2>/dev/null)
	fi
	while IFS=: read -r file line text; do
		[ -n "${file:-}" ] || continue
		n=$(printf '%s\n' "$text" | sed -n 's/.*[^0-9]\([0-9]\) *つの OS.*/\1/p' | head -1)
		[ -z "$n" ] || [ "$n" = "$n_os" ] ||
			bad "$file:$line: says $n つの OS, should be $n_os"
	done < <(grep -nE '[0-9] *つの OS' $docs 2>/dev/null)
fi

# ------------------------------------------------------- the two READMEs agree
en_h=$(grep -c '^## ' README.md)
ja_h=$(grep -c '^## ' README.ja.md)
[ "$en_h" = "$ja_h" ] ||
	bad "README.md has $en_h sections but README.ja.md has $ja_h"

# No hand-typed "latest release" any more: it is a badge, so that it cannot go
# stale. Catch it coming back.
grep -nE '^(Latest release|最新リリース) ' README.md README.ja.md 2>/dev/null |
	while IFS=: read -r file line _; do
		echo "$file:$line: a hand-typed latest release; the badge above it does this" >&2
	done
grep -qE '^(Latest release|最新リリース) ' README.md README.ja.md 2>/dev/null &&
	problems=$((problems+1))

# ------------------------------------------------------------- the rule files
# A rule the editor already has is walked again for every character of every
# line and can never colour anything the first one did not. None of this shows
# on the screen, which is why it accumulated: make.jvsyn was the same nine
# rules written out twice, rc.jvsyn had CURSOR and IDI_WINLOGO twice in one
# list and a stray "//" that built a rule for the empty word, and java and vbs
# each repeated a keyword the line above already had.
#
# Only a repeat within one group is reported. The same word under two groups is
# dead too -- the earlier rule wins -- but sometimes deliberately so: php has
# "static" as a StorageClass and again as a Type, and both readings are real.
for f in syntax/*.jvsyn; do
	[ -f "$f" ] || continue
	dup=$(grep -v '^[[:space:]]*"' "$f" | grep -v '^[[:space:]]*$' |
			sort | uniq -d)
	[ -z "$dup" ] || bad "$f: these rules appear more than once:
$(printf '%s\n' "$dup" | sed 's/^/    /')"

	# The words of a w rule, which are what the "/" separates. A t rule spends
	# its first two fields on the delimiters of the tag it looks inside, so
	# html's twenty "itw/</>/..." lines are not twenty rules for "<".
	words=$(awk '
		/^[[:space:]]*"/ || /^[[:space:]]*$/	{ next }
		{
			if (match($0, /^syntax[ \t]+[^ \t]+[ \t]+[a-z]*w[a-z]*\//) == 0)
				next
			n = split($0, a, /[ \t]+/)
			group = a[2]
			mode  = substr(a[3], 1, index(a[3], "/") - 1)
			rest  = substr(a[3], index(a[3], "/") + 1)
			first = (index(mode, "t") > 0) ? 3 : 1
			nw = split(rest, w, "/")
			for (i = first; i <= nw; i++)
			{
				if (w[i] == "")
				{
					print NR ": an empty word, from a \"/\" with nothing before it"
					continue
				}
				key = group SUBSEP ((index(mode, "i") > 0) ? tolower(w[i]) : w[i])
				if (key in seen)
					print NR ": \"" w[i] "\" is already a " group \
							" at line " seen[key]
				else
					seen[key] = NR
			}
		}' "$f")
	[ -z "$words" ] || bad "$f: words a rule already has:
$(printf '%s\n' "$words" | sed 's/^/    /')"
done

# ------------------------------------------------------------------- VERSION
version=$(tr -d ' \t\r\n' < VERSION 2>/dev/null)
case $version in
''|*[!0-9.]*)	bad "VERSION should hold one line like 1.0.0, not '$version'" ;;
esac
if [ -n "$want_version" ]; then
	[ "$version" = "$want_version" ] ||
		bad "VERSION says $version but the release being cut is $want_version"
fi

# ------------------------------------------------------------------ CHANGELOG
if [ ! -f CHANGELOG.md ]; then
	bad "no CHANGELOG.md"
elif [ -n "$want_version" ]; then
	if ! grep -qE "^## \[?$want_version\]?( |$|—)" CHANGELOG.md; then
		bad "CHANGELOG.md has no '## $want_version' section for this release"
	else
		body=$(awk -v v="$want_version" '
			$0 ~ "^## \\[?" v "\\]?([ ]|$|—)" { inside = 1; next }
			inside && /^## / { exit }
			inside { print }' CHANGELOG.md)
		[ -n "$(printf '%s' "$body" | tr -d ' \t\n')" ] ||
			bad "CHANGELOG.md: the $want_version section is empty"
		# Not "... | grep -q": pipefail plus grep -q's exit-on-first-match can
		# have grep close its end of the pipe while printf is still writing,
		# SIGPIPE-killing printf -- and pipefail then reports *that* exit
		# status, not grep's, so a section that plainly has the word fails
		# this check anyway. A case pattern needs no pipe to race.
		case $body in
		*日本語*)	;;
		*)		bad "CHANGELOG.md: the $want_version section has no 日本語 block" ;;
		esac
	fi
fi

# -------------------------------------------------------------- relative links
# The first released heading in the CHANGELOG. Anything at or below it is a
# record of what that release shipped, and stays true by not being touched --
# adding a test today does not change how many 1.0.0 had.
chlog_history=$(sed -n '/^## [0-9]/{=;q;}' CHANGELOG.md 2>/dev/null)

while IFS=: read -r file line text; do
	[ -n "${file:-}" ] || continue
	if [ "$file" = CHANGELOG.md ] && [ -n "${chlog_history:-}" ] &&
	   [ "$line" -ge "$chlog_history" ]; then
		continue
	fi
	printf '%s\n' "$text" | grep -oE '\]\([^)#][^)]*\)' | tr -d '])(' |
	while read -r target; do
		case $target in
		http*|mailto:*|'')	continue ;;
		esac
		t=${target%%#*}
		[ -z "$t" ] && continue
		dir=$(dirname "$file")
		[ -e "$dir/$t" ] || [ -e "$t" ] ||
			echo "$file:$line: link to $t, which does not exist" >&2
	done
done < <(grep -nE '\]\([^)#][^)]*\)' $docs 2>/dev/null)
# Counted separately: the loop above runs in a subshell and cannot add to
# problems. A missing link is reported but does not by itself fail the run.

echo
if [ "$problems" -eq 0 ]; then
	echo "docs agree with the tree"
	exit 0
fi
echo "$problems problem(s) above" >&2
exit 1
