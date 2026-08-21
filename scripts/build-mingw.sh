#!/bin/bash
#
# Build JVim 3 for Windows with mingw-w64.
#
# Works as a cross build from Linux/WSL and natively inside an MSYS2 shell.
#
#   scripts/build-mingw.sh                 GUI exe, 32 bit (recommended)
#   scripts/build-mingw.sh both            GUI + console exe
#   scripts/build-mingw.sh release         both architectures, zipped, in release/
#   scripts/build-mingw.sh clean
#   scripts/build-mingw.sh warn            build with warnings shown
#   ARCH=x86_64 scripts/build-mingw.sh     64 bit (see BUILDING-mingw.md)
#
# Output lands in dist/<arch>/ together with the help file, so the directory
# can be copied to the Windows side as is.

set -euo pipefail

ARCH=${ARCH:-i686}
JOBS=${JOBS:-$(nproc 2>/dev/null || echo 4)}
TARGET=${1:-all}

root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
src=$root/src
dist=$root/dist/$ARCH

# Which toolchain -- and it has to target msvcrt. Nothing is built against UCRT
# here or in CI: see BUILDING-mingw.md, "Which C runtime". Which runtime a
# mingw-w64 targets is settled when the toolchain itself was built, the headers
# defining _UCRT or not and libmingwex compiled to match, so no flag changes it
# and the only lever is which toolchain gets used.
#
#   MINGW_BIN=/usr/bin                 the directory; the prefix is derived per
#                                      architecture, so "release" can use it
#   CROSS=/usr/bin/i686-w64-mingw32-   the exact prefix, one architecture only
#
# With neither, the candidates below are tried in order and the first msvcrt one
# wins, so a UCRT toolchain earlier in PATH does not get to decide.
crt_of() {				# prefix -> msvcrt | ucrt | unknown
	local defs
	# Captured rather than piped into grep: under pipefail, grep exiting at its
	# first match kills the compiler with SIGPIPE and the pipeline reports
	# failure, which here would read a UCRT toolchain as msvcrt.
	defs=$(echo | "${1}gcc" -dM -E -include stdio.h - 2>/dev/null) || {
		echo unknown
		return
	}
	case $defs in
	*"#define _UCRT"*)	echo ucrt ;;
	*)					echo msvcrt ;;
	esac
}

if [ -n "${MINGW_BIN-}" ]; then
	candidates=("${MINGW_BIN%/}/${ARCH}-w64-mingw32-")
elif [ -n "${CROSS-}" ]; then
	# A CROSS names one architecture and is inherited by the builds "release"
	# spawns, where it would hand the 64 bit build a 32 bit compiler.
	case ${CROSS##*/} in
	"${ARCH}-w64-mingw32-" | "")
		candidates=("$CROSS")
		;;
	*)
		echo "CROSS=$CROSS is not a $ARCH toolchain." >&2
		echo "  For one architecture, match it: CROSS=${CROSS%/*}/${ARCH}-w64-mingw32-" >&2
		echo "  For both, name the directory instead: MINGW_BIN=${CROSS%/*}" >&2
		exit 1
		;;
	esac
else
	# PATH first, then where apt puts it, then a native MSYS2 gcc.
	candidates=("${ARCH}-w64-mingw32-" "/usr/bin/${ARCH}-w64-mingw32-" "")
fi

CROSS=
crt=
seen=
seen_crt=
for c in "${candidates[@]}"; do
	command -v "${c}gcc" >/dev/null 2>&1 || continue
	if [ -z "$c" ]; then		# a bare gcc has to be a mingw one
		machine=$("${c}gcc" -dumpmachine 2>/dev/null || true)
		case $machine in
		*mingw*)	;;
		*)			continue ;;
		esac
	fi
	this=$(crt_of "$c")
	if [ -z "$seen_crt" ]; then	# the first one that exists, for the message
		seen=$c
		seen_crt=$this
	fi
	if [ "$this" = msvcrt ]; then
		CROSS=$c
		crt=$this
		break
	fi
done

if [ -z "$crt" ]; then
	pkg=gcc-mingw-w64-i686-win32
	[ "$ARCH" = i686 ] || pkg=gcc-mingw-w64-x86-64-win32
	if [ -z "$seen_crt" ]; then
		echo "no mingw-w64 toolchain found for $ARCH." >&2
	else
		# "unknown" lands here too: a compiler that cannot be asked never
		# passes for the right one.
		echo "the $ARCH toolchain found targets $seen_crt; this tree is msvcrt only." >&2
		echo "  ${seen}gcc is $(command -v "${seen}gcc")" >&2
	fi
	echo "  Debian/Ubuntu: apt install $pkg   (the -win32 packages are msvcrt)" >&2
	echo "  MSYS2:         pacman -S mingw-w64-i686-gcc  (in the MINGW32 shell)" >&2
	echo "  Or name one:   MINGW_BIN=<dir> $0 $TARGET" >&2
	echo "  Or take CI's:  scripts/fetch-ci-build.sh" >&2
	exit 1
fi

# A linuxbrew toolchain picks up LIBRARY_PATH/LD_* from the host and then hands
# the cross linker host libraries. Build in a clean environment.
run_make() {
	env -i \
		PATH="$PATH" HOME="$HOME" TERM="${TERM:-dumb}" \
		make -C "$src" -f makefile.mingw CROSS="$CROSS" "$@"
}

# release: both architectures, packaged, ready to upload. Everything lands in
# release/ because "clean" takes dist/ with it between the two builds.
if [ "$TARGET" = release ]; then
	# On the console, not only in the per-architecture logs below, which are not
	# kept: this is the line that shows a CI image having quietly changed the
	# runtime its mingw-w64 defaults to.
	echo "packaging with the $crt runtime ($(${CROSS}gcc -dumpversion 2>/dev/null || echo \?))"
	version=${VERSION:-$(git -C "$root" describe --tags --always 2>/dev/null || echo snapshot)}
	rel=$root/release
	rm -rf "$rel"
	mkdir -p "$rel"
	for arch in i686 x86_64; do
		case $arch in
		i686)	bits=32 ;;
		x86_64)	bits=64 ;;
		esac
		ARCH=$arch "$0" clean >/dev/null
		ARCH=$arch "$0" both > "$rel/build-win$bits.log" 2>&1 || {
			echo "the $arch build failed; see $rel/build-win$bits.log" >&2
			exit 1
		}
		name=jvim3-$version-win$bits
		mkdir -p "$rel/$name"
		cp -p "$root/dist/$arch/vim.hlp" "$root/dist/$arch/_jvimrc.sample" "$rel/$name/"
		# makefile.mingw calls its targets jvim32*.exe whatever the architecture
		# is; the name in the package says which one it actually is.
		cp -p "$root/dist/$arch/jvim32w.exe" "$rel/$name/jvim${bits}w.exe"
		cp -p "$root/dist/$arch/jvim32.exe" "$rel/$name/jvim$bits.exe"
		if command -v zip >/dev/null 2>&1; then
			(cd "$rel" && zip -qr "$name.zip" "$name")
		else		# no zip(1) here, but python3 can do it
			(cd "$rel" && python3 -m zipfile -c "$name.zip" "$name")
		fi
		ARCH=$arch "$0" clean >/dev/null
	done
	echo
	echo "release packages in $rel:"
	ls -l "$rel"/*.zip
	exit 0
fi

case $TARGET in
clean)
	run_make clean
	rm -rf "$root/dist"
	echo "cleaned"
	exit 0
	;;
warn)
	run_make -j"$JOBS" WARN=1 all
	MAKETARGET=all
	;;
all | both | split | jvim32.exe | jvim32w.exe)
	run_make -j"$JOBS" "$TARGET"
	MAKETARGET=$TARGET
	;;
*)
	echo "usage: $0 [all|both|warn|split|release|clean]" >&2
	exit 2
	;;
esac

mkdir -p "$dist"
for exe in jvim32w.exe jvim32.exe; do
	[ -f "$src/$exe" ] && cp -p "$src/$exe" "$dist/"
done
# vim.hlp, not jvim3.hlp: the default 'helpfile' on Windows is "$VIM\vim.hlp",
# and $VIM with nothing set is the directory the exe is in, so ":help" works in
# an unpacked package without a _vimrc.
cp -p "$root/doc.j/vim.hlp" "$dist/vim.hlp"
cp -p "$root/doc.j/_jvimrc" "$dist/_jvimrc.sample"

echo
echo "built for $ARCH (${CROSS:-native}, $crt) -> $dist"
ls -la "$dist"
cat <<'EOF'

Next:
  1. copy dist/<arch>/ to the Windows side
  2. run jvim32w.exe (GUI) or jvim32.exe (console); ":help" finds vim.hlp
     beside it, and a _vimrc there is read as well. Set VIM only if you keep
     them somewhere else.

On an abnormal exit a report is written to %LOCALAPPDATA%\jvim3\ .
Resolve it with:  scripts/resolve-crash.sh <report.log>
EOF
