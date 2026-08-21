#!/usr/bin/env bash
#
# Print a starting point for a CHANGELOG entry, from the commits since a tag.
#
#   scripts/changelog-draft.sh                  since the last release
#   scripts/changelog-draft.sh v1.0.0           since that tag
#
# Paste what is useful into CHANGELOG.md and throw the rest away. This does not
# write the changelog: the commit subjects here are written as English
# sentences, which makes most of them usable as they stand, but some are about
# the build or the tests and mean nothing to somebody using the editor. Those
# are printed commented out, decided by which paths the commit touched -- so no
# ceremony is needed on a commit for this to work, and it works backwards over
# the commits already made.
#
# Nothing is generated automatically into a release. A changelog nobody edited
# is a commit log with extra steps.

set -uo pipefail

root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"

since=${1:-}
if [ -z "$since" ]; then
	since=$(git describe --tags --abbrev=0 HEAD 2>/dev/null) || since=
fi
if [ -z "$since" ]; then
	echo "# no tag found to start from; pass one" >&2
	range=
else
	range="$since..HEAD"
fi

n=$(git log --no-merges --format=%H ${range:+$range} | grep -c .) || n=0
if [ "$n" -eq 0 ]; then
	echo "# nothing since ${since:-the beginning}"
	exit 0
fi

echo "## <version> — $(date +%Y-%m-%d)"
echo
echo "# $n commits since ${since:-the beginning}. Lines commented out below"
echo "# touched only the build, the scripts, CI or documentation."
echo

git log --no-merges --reverse --format='%H%x09%s' ${range:+$range} |
while IFS=$'\t' read -r sha subject; do
	# Does this commit touch anything a user of the editor would see? A commit
	# only under scripts/, .github/ or in a .md file does not.
	interesting=no
	while read -r path; do
		[ -n "$path" ] || continue
		case $path in
		scripts/*|.github/*|*.md|VERSION)	;;
		*)									interesting=yes ;;
		esac
	done < <(git show --name-only --format= "$sha")

	if [ "$interesting" = yes ]; then
		printf -- '- %s\n' "$subject"
	else
		printf -- '#  (build/docs only?)  - %s\n' "$subject"
	fi
done

echo
echo "### 日本語"
echo
echo "# 上の各行に対応する日本語を書いてください。"
