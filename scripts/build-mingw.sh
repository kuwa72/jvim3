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

# A package is for somebody else to run, so a release is refused the wrong C
# runtime rather than warned about it. Exported, so the per-architecture builds
# it spawns are held to the same thing.
if [ "$TARGET" = release ]; then
	export REQUIRE_MSVCRT=1
fi

root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
src=$root/src
dist=$root/dist/$ARCH

# Native MSYS2 gcc needs no prefix; a cross toolchain does. CROSS from the
# environment wins, for pointing at a particular toolchain without reordering
# PATH -- CROSS=/usr/bin/i686-w64-mingw32- to prefer the apt one over a
# Homebrew one, say.
if [ -n "${CROSS-}" ]; then
	:
elif command -v "${ARCH}-w64-mingw32-gcc" >/dev/null 2>&1; then
	CROSS="${ARCH}-w64-mingw32-"
elif command -v gcc >/dev/null 2>&1 && gcc -dumpmachine | grep -q mingw; then
	CROSS=""
else
	echo "no mingw-w64 toolchain found for $ARCH." >&2
	echo "  Debian/Ubuntu: apt install gcc-mingw-w64-i686-win32   (msvcrt)" >&2
	echo "  MSYS2:         pacman -S mingw-w64-i686-gcc  (in the MINGW32 shell)" >&2
	exit 1
fi

# Which C runtime the toolchain targets. This is decided when the toolchain
# itself is built -- the headers define _UCRT or they do not, and libmingwex is
# compiled to match -- so no flag here can change it; the only lever is which
# toolchain is on PATH.
#
# It has to be msvcrt, which is what the releases are built with and what
# BUILDING-mingw.md explains. A UCRT toolchain builds a different editor from
# the one that ships, and not only in the printf corners: msvcrt's tmpnam()
# returns a name in the root of the current drive, which no ordinary process may
# write to, while UCRT's returns one under TEMP. ":r !cmd" therefore worked in
# every local build and failed in every release, twice over, until it was
# changed to stop using tmpnam() at all. A build that cannot be trusted to
# behave like the release is worse than no build, so say which one this is.
# "unknown" rather than an optimistic "msvcrt" when the probe will not run: the
# gate below refuses that too, so a compiler this cannot ask never passes for
# the right one.
crt=unknown
if crt_defs=$(echo | ${CROSS}gcc -dM -E -include stdio.h - 2>/dev/null); then
	case $crt_defs in
	*"#define _UCRT"*)	crt=ucrt ;;
	*)					crt=msvcrt ;;
	esac
fi

# A warning is enough while building for oneself -- a UCRT exe does run, and
# demanding the other toolchain to compile anything at all would be its own kind
# of broken. Anything that becomes a package for somebody else sets
# REQUIRE_MSVCRT and gets a refusal instead; "release" sets it for the builds it
# spawns, so both architectures are held to it, not just the one checked here.
if [ "$crt" != msvcrt ] && [ -n "${REQUIRE_MSVCRT-}" ]; then
	echo "this toolchain targets $crt; release packages are msvcrt." >&2
	echo "  ${CROSS}gcc is $(command -v "${CROSS}gcc" || echo "not on PATH")" >&2
	echo "  apt install gcc-mingw-w64-i686-win32 gcc-mingw-w64-x86-64-win32," >&2
	echo "  then CROSS=/usr/bin/${ARCH}-w64-mingw32- $0 $TARGET" >&2
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
if [ "$crt" != msvcrt ]; then
	cat >&2 <<EOF

WARNING: this toolchain targets $crt, and the releases are msvcrt. The exe runs,
but it is not the editor anybody else has, so testing it on Windows proves
nothing about the release. Build with an msvcrt toolchain before believing a
Windows result:
  apt install gcc-mingw-w64-i686-win32 gcc-mingw-w64-x86-64-win32
  CROSS=/usr/bin/${ARCH}-w64-mingw32- $0 $TARGET
EOF
fi
cat <<'EOF'

Next:
  1. copy dist/<arch>/ to the Windows side
  2. run jvim32w.exe (GUI) or jvim32.exe (console); ":help" finds vim.hlp
     beside it, and a _vimrc there is read as well. Set VIM only if you keep
     them somewhere else.

On an abnormal exit a report is written to %LOCALAPPDATA%\jvim3\ .
Resolve it with:  scripts/resolve-crash.sh <report.log>
EOF
