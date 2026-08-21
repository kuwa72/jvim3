#!/usr/bin/env bash
#
# Everything that has to be true before a tag is pushed, in one command.
#
#   scripts/release.sh 1.0.1            check, show the notes, stop
#   scripts/release.sh 1.0.1 --push     ... and then tag and push
#
# Without --push nothing is changed: it checks, prints what would be published,
# and tells you the two commands to run. With --push it creates an annotated tag
# and pushes it, which is what starts a release.
#
# It never creates the release itself. CI does that, from the tag, and only
# after the build and the tests have passed on every platform -- which is the
# reason a broken build cannot become a release, and is not worth weakening for
# convenience.
#
# The check that matters most is the last one before tagging: that CI is green
# on this exact commit. A tag has been pushed onto a failing commit here before
# and had to be deleted and redone.

set -uo pipefail

root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$root"

do_push=no
version=
for arg in "$@"; do
	case $arg in
	--push)	do_push=yes ;;
	-h|--help)	sed -n '3,20p' "$0" | sed 's/^# \{0,1\}//'; exit 0 ;;
	-*)		echo "unknown option: $arg" >&2; exit 2 ;;
	*)		version=$arg ;;
	esac
done

if [ -z "$version" ]; then
	echo "usage: $0 <version> [--push]        e.g. $0 1.0.1" >&2
	exit 2
fi
version=${version#v}
case $version in
[0-9]*.[0-9]*.[0-9]*)	;;
*)	echo "'$version' is not a version like 1.0.1" >&2; exit 2 ;;
esac
tag=v$version

step=0
say() { step=$((step+1)); printf '\n[%d] %s\n' "$step" "$1"; }
die() { printf '\n  %s\n' "$1" >&2; exit 1; }

# --- 1. tools ----------------------------------------------------------------
say "tools"
command -v git >/dev/null 2>&1 || die "no git"
if [ "$do_push" = yes ]; then
	command -v gh >/dev/null 2>&1 || die "no gh, which --push needs"
fi
echo "  ok"

# --- 2. the working tree -----------------------------------------------------
say "working tree"
dirty=$(git status --porcelain)
[ -z "$dirty" ] || die "uncommitted changes (including untracked files):
$dirty"
branch=$(git rev-parse --abbrev-ref HEAD)
[ "$branch" = master ] || die "on '$branch', not master"
echo "  clean, on master"

# --- 3. in step with the remote ---------------------------------------------
say "in step with origin"
git fetch --quiet origin --tags || die "cannot reach origin"
local_head=$(git rev-parse HEAD)
remote_head=$(git rev-parse origin/master)
if [ "$local_head" != "$remote_head" ]; then
	if git merge-base --is-ancestor origin/master HEAD; then
		die "master is ahead of origin. Push first: CI has to build the exact
  commit that gets tagged."
	fi
	die "master and origin/master have diverged, or this is behind. Sort that
  out first."
fi
echo "  HEAD is origin/master, ${local_head:0:8}"

# --- 4. the tag is free -----------------------------------------------------
say "the tag $tag is free"
git rev-parse -q --verify "refs/tags/$tag" >/dev/null &&
	die "$tag already exists locally"
git ls-remote --exit-code --tags origin "refs/tags/$tag" >/dev/null 2>&1 &&
	die "$tag already exists on origin"
# Only this tree's own semantic versions are comparable. The nine
# v3.0-j2.1b-utf8.* tags sort above everything by version order and would make
# this check nonsense; see RELEASING.md.
newest=$(git tag -l 'v[0-9]*.[0-9]*.[0-9]*' | sort -V | tail -1)
if [ -z "$newest" ]; then
	echo "  free. No semantic version tag yet, so this is the first: the nine
  v3.0-j2.1b-utf8.* tags are excluded from the comparison by pattern, and
  $tag is newer than all of them despite sorting lower."
else
	[ "$(printf '%s\n%s\n' "$newest" "$tag" | sort -V | tail -1)" = "$tag" ] ||
		die "$tag is not newer than $newest"
	echo "  free, and newer than $newest"
fi

# --- 5. VERSION, the CHANGELOG and the documentation ------------------------
say "VERSION, the CHANGELOG and the documentation"
./scripts/check-docs.sh --version "$version" || die "check-docs.sh found problems"

# --- 6. build and test ------------------------------------------------------
say "build and test on this machine"
./scripts/build-unix.sh test > /tmp/release-test.$$ 2>&1 || {
	tail -30 /tmp/release-test.$$ >&2
	rm -f /tmp/release-test.$$
	die "the tests failed"
}
grep -E '^(cases|pass) ' /tmp/release-test.$$ | sed 's/^/  /'
rm -f /tmp/release-test.$$

# --- 7. the binary says which release it is ---------------------------------
say "the binary names itself"
# Read out of the binary rather than run it: jvim needs a controlling terminal
# and exits before it prints anything without one.
if grep -aq "JVim 3 $version" src/jvim3; then
	echo "  src/jvim3 carries \"JVim 3 $version\""
else
	die "src/jvim3 does not carry \"JVim 3 $version\". VERSION and what got
  compiled disagree -- rebuild, and check scripts/build-unix.sh."
fi

# --- 8. the Windows packages, if this machine can make them -----------------
say "Windows packages"
if VERSION=$version ./scripts/build-mingw.sh release > /tmp/release-win.$$ 2>&1; then
	ls -1 release/*.zip 2>/dev/null | sed 's/^/  /'
	for z in release/*.zip; do
		[ -f "$z" ] || continue
		case $z in
		*"$version"*)	;;
		*)	die "$z is not named after $version" ;;
		esac
	done
	rm -f /tmp/release-win.$$
else
	echo "  not built here:"
	tail -3 /tmp/release-win.$$ | sed 's/^/    /'
	rm -f /tmp/release-win.$$
	echo "  CI will build them. Afterwards, scripts/fetch-ci-build.sh gets the
  exact package it produced, and scripts/test-winkeys.sh runs the 14 key
  cases against it from WSL."
fi

# --- 9. is CI green on this commit? -----------------------------------------
say "CI on ${local_head:0:8}"
if command -v gh >/dev/null 2>&1; then
	conclusion=$(gh run list --commit "$local_head" --limit 1 \
		--json conclusion --jq '.[0].conclusion' 2>/dev/null)
	case ${conclusion:-} in
	success)	echo "  green" ;;
	'' |null)	echo "  no run found for this commit yet.
  Push the commit and let CI finish before tagging: a tag on a commit whose
  build fails cannot become a release, and has to be deleted and redone." ;;
	*)			die "the last run on this commit was '$conclusion'. Fix that
  before tagging." ;;
	esac
else
	echo "  no gh, cannot check. Make sure the run on this commit passed."
fi

# --- 10. what would be published -------------------------------------------
say "the release notes that would be published"
echo
./scripts/release-notes.sh "$tag" || die "cannot build the release notes"
echo
echo "--- and the commits this covers, so you can see what the notes leave out:"
prev=$(git describe --tags --abbrev=0 HEAD 2>/dev/null)
git log --no-merges --oneline ${prev:+$prev..HEAD} | sed 's/^/  /'

# --- 11. tag -----------------------------------------------------------------
if [ "$do_push" != yes ]; then
	cat <<EOF

Everything checked. Nothing has been changed.

To release:
    ./scripts/release.sh $version --push

or by hand:
    git tag -a $tag -m "JVim 3 $tag"
    git push origin $tag
EOF
	exit 0
fi

say "tagging and pushing"
git tag -a "$tag" -m "JVim 3 $tag" || die "cannot create the tag"
if ! git push origin "$tag"; then
	die "the tag was created locally but not pushed. Either push it, or remove
  it with:  git tag -d $tag"
fi

cat <<EOF

  pushed $tag

Watch the build:      gh run watch
The release appears:  https://github.com/kuwa72/jvim3/releases/tag/$tag
EOF
