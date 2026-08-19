#!/bin/bash
#
# Build JVim 3 for Windows with mingw-w64.
#
# Works as a cross build from Linux/WSL and natively inside an MSYS2 shell.
#
#   scripts/build-mingw.sh                 GUI exe, 32 bit (recommended)
#   scripts/build-mingw.sh both            GUI + console exe
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
	echo "usage: $0 [all|both|warn|split|clean]" >&2
	exit 2
	;;
esac

mkdir -p "$dist"
for exe in jvim32w.exe jvim32.exe; do
	[ -f "$src/$exe" ] && cp -p "$src/$exe" "$dist/"
done
cp -p "$root/doc.j/vim.hlp" "$dist/jvim3.hlp"
cp -p "$root/doc.j/_jvimrc" "$dist/_jvimrc.sample"

echo
echo "built for $ARCH (${CROSS:-native}) -> $dist"
ls -la "$dist"
cat <<'EOF'

Next:
  1. copy dist/<arch>/ to the Windows side
  2. set VIM to that directory so jvim3.hlp and _vimrc are found
  3. run jvim32w.exe (GUI) or jvim32.exe (console)

On an abnormal exit a report is written to %LOCALAPPDATA%\jvim3\ .
Resolve it with:  scripts/resolve-crash.sh <report.log>
EOF
