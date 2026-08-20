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

# Native MSYS2 gcc needs no prefix; a cross toolchain does.
if command -v "${ARCH}-w64-mingw32-gcc" >/dev/null 2>&1; then
	CROSS="${ARCH}-w64-mingw32-"
elif command -v gcc >/dev/null 2>&1 && gcc -dumpmachine | grep -q mingw; then
	CROSS=""
else
	echo "no mingw-w64 toolchain found for $ARCH." >&2
	echo "  Debian/Ubuntu: apt install mingw-w64" >&2
	echo "  Homebrew:      brew install mingw-w64" >&2
	echo "  MSYS2:         pacman -S mingw-w64-i686-gcc  (in the MINGW32 shell)" >&2
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
echo "built for $ARCH (${CROSS:-native}) -> $dist"
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
