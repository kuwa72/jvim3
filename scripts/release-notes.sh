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
enc=$(COUNT_ONLY=1 ./scripts/test-encoding.sh 2>/dev/null | sed -n 's/^cases //p')
edt=$(COUNT_ONLY=1 ./scripts/test-editing.sh  2>/dev/null | sed -n 's/^cases //p')
if [ -z "$enc" ] || [ -z "$edt" ]; then
	echo "cannot get the case counts from the test suites" >&2
	exit 1
fi
tests=$((enc + edt))

# The platforms that gate a release are the release job's needs:, minus the
# ones that are not an operating system the suites run on.
needs=$(sed -n 's/^ *needs: *\[\(.*\)\].*/\1/p' .github/workflows/build.yml | head -1)
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
awk -v changes="$changes" -v version="$version" -v tests="$tests" \
	-v platforms="$platforms" -v prev="$prev" -v compare="$compare" '
{
	gsub(/\{\{CHANGES\}\}/,     changes)
	gsub(/\{\{VERSION\}\}/,     version)
	gsub(/\{\{TESTS\}\}/,       tests)
	gsub(/\{\{PLATFORMS\}\}/,   platforms)
	gsub(/\{\{PREV_TAG\}\}/,    prev)
	gsub(/\{\{COMPARE_URL\}\}/, compare)
	print
}' "$template"
