#!/usr/bin/env bash
#
# Print the release notes for a tag, on stdout.
#
#   scripts/release-notes.sh v1.0.0
#
# The notes are this tag's section of CHANGELOG.md, followed by the standing
# part -- what the packages hold, how to run it, where the documentation is --
# from .github/release-body.md.
#
# It is one place rather than two because it used to be neither: the notes were
# a heredoc inside the workflow, identical for all nine releases, and by the
# time anybody looked it claimed the tests passed on a platform that had been
# removed and quoted a test count that was a hundred when the answer was a
# hundred and ten. The template holds no numbers and no platform names of its
# own now; the ones below are computed, and scripts/check-docs.sh reads the
# template too, so a number typed back into it is caught.

set -uo pipefail

root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"

tag=${1:-}
if [ -z "$tag" ]; then
	echo "usage: $0 <tag>            e.g. $0 v1.0.0" >&2
	exit 2
fi
version=${tag#v}

template=.github/release-body.md
[ -f "$template" ] || { echo "no $template" >&2; exit 2; }

# --- what changed: this version's CHANGELOG section, without its heading ------
changes=$(awk -v v="$version" '
	$0 ~ "^## \\[?" v "\\]?([ ]|$|—)" { inside = 1; next }
	inside && /^## / { exit }
	inside { print }' CHANGELOG.md)
changes=$(printf '%s\n' "$changes" | sed -e :a -e '/^\n*$/{$d;N;ba' -e '}')
if [ -z "$(printf '%s' "$changes" | tr -d ' \t\n')" ]; then
	echo "CHANGELOG.md has no '## $version' section, or it is empty" >&2
	echo "add one before releasing; see RELEASING.md" >&2
	exit 1
fi

# --- the facts the template asks for -----------------------------------------
#
# Every suite the "test" target runs, not two of them: this said enc + edt back
# when those were all there were, and went on saying it -- 129 -- while three
# more suites brought the answer to 235. Adding a suite means adding it here,
# the same as in check-docs.sh and build-unix.sh.
tests=0
for suite in encoding editing syntax sgr hostile; do
	n=$(COUNT_ONLY=1 ./scripts/test-$suite.sh 2>/dev/null | sed -n 's/^cases //p')
	if [ -z "$n" ]; then
		echo "cannot get the case count from scripts/test-$suite.sh" >&2
		exit 1
	fi
	tests=$((tests + n))
done

# The platforms that gate a release are the release job's needs:, minus the
# ones that are not an operating system the suites run on.
#
# The release job's, and not the first needs: in the file: windows-runtime is
# written above it and needs only [windows], so head -1 answered with a list
# holding no operating system at all and the notes said "passed on ." Read the
# block that begins with the job id instead.
needs=$(sed -n '/^  release:/,/^  [a-z]/{s/^ *needs: *\[\(.*\)\].*/\1/p;}' \
			.github/workflows/build.yml | head -1)
oses=""
for job in $(printf '%s\n' "$needs" | tr ',' ' '); do
	case $job in
	unix)		oses="$oses Linux" ;;
	freebsd)	oses="$oses FreeBSD" ;;
	netbsd)		oses="$oses NetBSD" ;;
	openbsd)	oses="$oses OpenBSD" ;;
	dragonfly)	oses="$oses DragonFly" ;;
	esac
done
# "a, b and c"
platforms=$(printf '%s\n' $oses | paste -sd, - | sed 's/,/, /g; s/, \([^,]*\)$/ and \1/')

# The release before this one, for the compare link. Version-sorting is wrong
# for these tags on purpose -- see RELEASING.md -- so ask git for the tag that
# comes before this commit in the history instead.
prev=$(git describe --tags --abbrev=0 "$tag^" 2>/dev/null) ||
	prev=$(git describe --tags --abbrev=0 HEAD 2>/dev/null) || prev=
if [ -n "$prev" ]; then
	compare="https://github.com/kuwa72/jvim3/compare/$prev...$tag"
else
	prev="the beginning"
	compare="https://github.com/kuwa72/jvim3/commits/$tag"
fi

# --- fill the template -------------------------------------------------------
# awk rather than sed: the replacements hold newlines, slashes and ampersands.
#
# Not gsub()/sub(): their replacement argument treats a bare "&" as "the text
# just matched" and "\" as the start of an escape, so a changelog entry with
# "K&R" in it -- 1.1.0 has one -- came out as "K{{CHANGES}}R". replace() below
# does the substitution with substr()/index() instead, where nothing in the
# replacement text means anything but itself.
#
# "-v changes=" has its own, earlier round of escape processing -- the same
# rules as a string literal in the program, so a lone "\" in front of a
# character awk does not recognise as an escape is silently dropped ("\_"
# arrived as "_"). Doubled here so that round undoes it exactly and every
# backslash in the changelog survives.
changes_v=$(printf '%s' "$changes" | sed 's/\\/\\\\/g')
awk -v changes="$changes_v" -v version="$version" -v tests="$tests" \
	-v platforms="$platforms" -v prev="$prev" -v compare="$compare" '
function replace(s, pat, rep,    i, out) {
	out = ""
	while ((i = index(s, pat)) > 0) {
		out = out substr(s, 1, i - 1) rep
		s = substr(s, i + length(pat))
	}
	return out s
}
{
	line = $0
	line = replace(line, "{{CHANGES}}",     changes)
	line = replace(line, "{{VERSION}}",     version)
	line = replace(line, "{{TESTS}}",       tests)
	line = replace(line, "{{PLATFORMS}}",   platforms)
	line = replace(line, "{{PREV_TAG}}",    prev)
	line = replace(line, "{{COMPARE_URL}}", compare)
	print line
}' "$template"
