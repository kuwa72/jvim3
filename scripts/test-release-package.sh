#!/usr/bin/env bash
#
# Verify that a release package contains a complete install.
#
# The Windows deploy script checks the same things when it unpacks, and a
# missing file there fails late. This test runs against the zips produced by
# scripts/build-mingw.sh release so the problem is caught before deploy.
#
#   scripts/test-release-package.sh

set -euo pipefail

root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
rel=$root/release

if [ ! -d "$rel" ]; then
	echo "no release/ directory; build one with: ./scripts/build-mingw.sh release" >&2
	exit 2
fi

missing=0
for zip in "$rel"/jvim3-*-win32.zip "$rel"/jvim3-*-win64.zip; do
	[ -f "$zip" ] || {
		echo "missing package: $zip" >&2
		missing=$((missing + 1))
		continue
	}

	# Bit width from the file name.
	case $zip in
	*win64*) bits=64 ;;
	*win32*) bits=32 ;;
	esac

	has() {
		# Avoid grep -q in a pipeline under pipefail: an early exit from
		# grep closes the pipe and unzip is killed by SIGPIPE.
		unzip -l "$zip" | grep -E "$1" >/dev/null
	}

	for f in "/jvim${bits}w\.exe$" "/jvim${bits}\.exe$" "/vim\.hlp$" "/jvimtutor\.bat$"; do
		if ! has "$f"; then
			echo "$(basename "$zip"): missing $f" >&2
			missing=$((missing + 1))
		fi
	done

	# Tutor files -- either the Japanese translation or the English original.
	if ! has "/tutor/tutor\.j$" && ! has "/tutor/tutor$"; then
		echo "$(basename "$zip"): missing tutor/tutor or tutor/tutor.j" >&2
		missing=$((missing + 1))
	fi

	# Syntax rules: the sample rc sources $VIM/syntax/*.jvsyn.
	if ! has "/syntax/.*\.jvsyn$"; then
		echo "$(basename "$zip"): missing syntax/*.jvsyn" >&2
		missing=$((missing + 1))
	fi
done

if [ "$missing" -ne 0 ]; then
	echo "release package verification failed: $missing missing item(s)" >&2
	exit 1
fi

echo "release packages are complete"
exit 0
