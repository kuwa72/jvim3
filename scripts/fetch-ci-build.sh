#!/bin/bash
#
# Fetch the Windows package CI built for a commit, and unpack it where Windows
# can actually run it.
#
# The releases are built against msvcrt; a Homebrew mingw-w64 targets UCRT, and
# which one a toolchain targets cannot be changed by a flag (BUILDING-mingw.md,
# "Which C runtime"). So a local build here is not the editor anybody else gets,
# and testing it on Windows proves nothing about the release -- which is how
# ":r !cmd" shipped broken twice. Installing a second toolchain fixes that but
# is not always worth it, especially on a machine that also does UCRT work.
# This takes the binary CI already built instead, which is the same one the
# release page serves.
#
#   scripts/fetch-ci-build.sh                 the newest good run for HEAD
#   scripts/fetch-ci-build.sh <sha|branch>    ... for that commit instead
#   scripts/fetch-ci-build.sh <run-id>        ... that exact run
#   ARCH=x86_64 scripts/fetch-ci-build.sh     the 64 bit package
#   DEST=/mnt/c/wherever scripts/fetch-ci-build.sh
#
# Needs gh, logged in. The default destination is under the Windows user
# profile on purpose: started from a WSL directory the editor's cwd reaches
# cmd.exe as a UNC path, which it refuses, so ":r !cmd" would silently run in
# C:\Windows and the thing being tested would not be what ran.

set -euo pipefail

ARCH=${ARCH:-i686}
case $ARCH in
i686)	bits=32 ;;
x86_64)	bits=64 ;;
*)		echo "ARCH must be i686 or x86_64, not $ARCH" >&2; exit 2 ;;
esac

root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
want=${1:-HEAD}

command -v gh >/dev/null 2>&1 || {
	echo "gh is not installed; it is what downloads the artifact." >&2
	exit 1
}

# A bare number long enough to be a run id is one; anything else is a commit.
if [[ $want =~ ^[0-9]{6,}$ ]]; then
	run=$want
else
	sha=$(git -C "$root" rev-parse "$want" 2>/dev/null) || {
		echo "not a commit, branch or run id: $want" >&2
		exit 1
	}
	# Newest first, so the head of the list is the run to trust: a commit can be
	# built more than once (pushed to a branch, then to master).
	run=$(gh run list --limit 40 --json databaseId,headSha,conclusion \
		-q "[.[] | select(.headSha == \"$sha\" and .conclusion == \"success\")][0].databaseId")
	if [ -z "$run" ] || [ "$run" = null ]; then
		pending=$(gh run list --limit 40 --json databaseId,headSha,status \
			-q "[.[] | select(.headSha == \"$sha\" and .status != \"completed\")][0].databaseId")
		if [ -n "$pending" ] && [ "$pending" != null ]; then
			echo "run $pending for ${sha:0:7} is still going; waiting."
			gh run watch "$pending" --exit-status --interval 20 >/dev/null
			run=$pending
		else
			echo "no successful run for ${sha:0:7}." >&2
			echo "Push it and let CI build, or name a run id." >&2
			exit 1
		fi
	fi
fi

# Where Windows can reach it, and where cmd.exe will accept the cwd.
if [ -z "${DEST-}" ]; then
	profile=$(cmd.exe /c 'echo %USERPROFILE%' 2>/dev/null | tr -d '\r' || true)
	if [ -n "$profile" ] && command -v wslpath >/dev/null 2>&1; then
		DEST=$(wslpath -u "$profile")/jvim3-ci/win$bits
	else
		DEST=$root/dist/ci/win$bits
		echo "no Windows profile found; unpacking to $DEST." >&2
		echo "If that is a WSL path, copy it to the Windows side before" >&2
		echo "testing anything that runs a shell command." >&2
	fi
fi

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

echo "run $run -> $DEST"
gh run download "$run" -n windows -D "$tmp" >/dev/null

zip=$(ls "$tmp"/jvim3-*-win$bits.zip 2>/dev/null | head -1)
[ -n "$zip" ] || { echo "no win$bits package in that run's artifact." >&2; exit 1; }

rm -rf "$DEST"
mkdir -p "$DEST"
if command -v unzip >/dev/null 2>&1; then
	unzip -q "$zip" -d "$tmp/x"
else
	python3 -c 'import sys,zipfile;zipfile.ZipFile(sys.argv[1]).extractall(sys.argv[2])' \
		"$zip" "$tmp/x"
fi

# The package holds one directory named for the version, and its *contents* are
# what DEST wants: the exe at the top, and syntax/ still a directory beside it.
#
# This used to unpack with "unzip -j", which junks the paths. The exe landed
# where it belongs, so it looked right and ran -- but syntax/ was flattened into
# the same directory, and "source $VIM/syntax/filetype.jvsyn" had nothing to
# open. The package only grew a subdirectory when the rules were split out of
# the rc, which is after this script was written.
inner=$tmp/x
only=$(ls -A "$tmp/x")
if [ "$(printf '%s\n' "$only" | wc -l)" -eq 1 ] && [ -d "$tmp/x/$only" ]; then
	inner=$tmp/x/$only
fi
cp -pR "$inner/." "$DEST/"

# Say so here rather than let the editor say "can't open file" later. Anything
# the package puts in a subdirectory is reachable only through $VIM, which is
# exactly what a flattening unpack breaks.
[ -f "$DEST/syntax/filetype.jvsyn" ] || {
	echo "unpacked, but $DEST/syntax/filetype.jvsyn is not there." >&2
	echo "The rules are what an rc reaches with \"source \$VIM/syntax/...\";" >&2
	echo "without them the editor starts and colours nothing." >&2
	exit 1
}

# Prove it is the runtime the release uses rather than taking it on trust: this
# is the whole point of not building locally.
exe=$DEST/jvim${bits}w.exe
if [ -f "$exe" ] && command -v "${ARCH}-w64-mingw32-objdump" >/dev/null 2>&1; then
	# Capture once and match in the shell. "objdump | grep -q" reports every
	# package as "unknown" as often as not: grep exits at the first hit, objdump
	# dies of SIGPIPE, and pipefail makes that the status of the whole pipeline.
	imports=$("${ARCH}-w64-mingw32-objdump" -p "$exe" 2>/dev/null || true)
	shopt -s nocasematch
	if [[ $imports == *"DLL Name: msvcrt.dll"* ]]; then
		crt=msvcrt
	elif [[ $imports == *"api-ms-win-crt"* ]]; then
		crt=ucrt
	else
		crt=unknown
	fi
	shopt -u nocasematch
	echo "$(basename "$exe") links $crt"
	[ "$crt" = msvcrt ] || echo "  ... which is not what the releases use." >&2
fi

echo
ls -la "$DEST"
cat <<EOF

Run it from there, not from a WSL directory:
  cd $DEST && ./jvim${bits}w.exe
EOF
