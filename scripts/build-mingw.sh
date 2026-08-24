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
# Copy a text file into the package with the line separator Windows uses.
#
# dosource() (cmdline.c) warns "Wrong line separator, ^M may be missing" for
# every sourced file whose lines end in a bare LF, and it means it: a mapping
# that ends in a real ^M loses it. The repository is a Unix one and stores LF,
# so the package used to carry LF into Windows and the editor said so three
# times before it had finished starting -- once for the rc, once for
# filetype.jvsyn, once for the rules it pulls in.
#
# Only a CR at the end of a line is a separator, and only that one may be
# touched. A CR in the middle of a line is content: doc.j/_jvimrc line 37 is
#
#     "map    n /^Mz.
#
# where the ^M is a real carriage return in the middle of the mapping. A first
# pass of "tr -d '\r'" ate it and left "/z." behind, which is the whole reason
# a rule file may not simply be run through a CR filter.
#
# awk rather than "sed -e s/\r$//": \r in a sed expression is a GNU extension
# and this script runs on macOS too. \015 rather than \r for the same reason.
# Stripping the trailing one first makes this idempotent, so a file that
# already has CRLF does not come out with two CRs.
cp_crlf() {
	awk '{ sub(/\015$/, ""); printf "%s\r\n", $0 }' < "$1" > "$2"
}

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

# The release number this tree calls itself, and which commit this is. Worked
# out here rather than left to makefile.mingw because "env -i" below wipes the
# environment, so these have to travel as command line assignments. No brackets
# in jvim_build: version.c puts those round it.
jvim_version=$(tr -d ' \t\r\n' < "$root/VERSION")
case $jvim_version in
''|*[!0-9.]*)
	echo "$root/VERSION should hold one line like 1.0.0, not '$jvim_version'" >&2
	exit 2
	;;
esac
jvim_build=
if command -v git >/dev/null 2>&1 &&
   git -C "$root" rev-parse --git-dir >/dev/null 2>&1; then
	jvim_build=$(git -C "$root" rev-parse --short=8 HEAD 2>/dev/null) || jvim_build=
	if [ -n "$jvim_build" ] &&
	   [ -n "$(git -C "$root" status --porcelain 2>/dev/null)" ]; then
		jvim_build=$jvim_build-dirty
	fi
fi

# A linuxbrew toolchain picks up LIBRARY_PATH/LD_* from the host and then hands
# the cross linker host libraries. Build in a clean environment.
run_make() {
	env -i \
		PATH="$PATH" HOME="$HOME" TERM="${TERM:-dumb}" \
		make -C "$src" -f makefile.mingw CROSS="$CROSS" \
			JVIMVER="$jvim_version" JVIMBUILD="$jvim_build" "$@"
}

# release: both architectures, packaged, ready to upload. Everything lands in
# release/ because "clean" takes dist/ with it between the two builds.
if [ "$TARGET" = release ]; then
	# On the console, not only in the per-architecture logs below, which are not
	# kept: this is the line that shows a CI image having quietly changed the
	# runtime its mingw-w64 defaults to.
	echo "packaging with the $crt runtime ($(${CROSS}gcc -dumpversion 2>/dev/null || echo \?))"
	# The package name. VERSION= from the environment still wins, which is how
	# CI names a tagged package; a leading "v" is stripped so the file reads
	# jvim3-1.0.0-win32.zip rather than jvim3-v1.0.0-win32.zip. Left alone, an
	# untagged build names itself after the release it came after plus the
	# commit, which is self-describing and needs no tags fetched.
	version=${VERSION:-$jvim_version${jvim_build:+-$jvim_build}}
	version=${version#v}
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
		cp -p "$root/dist/$arch/vim.hlp" "$root/dist/$arch/_jvimrc.sample" \
			"$root/dist/$arch/jvimrc.sample" "$rel/$name/"
		cp -pR "$root/dist/$arch/syntax" "$rel/$name/syntax"
		cp -pR "$root/dist/$arch/colors" "$rel/$name/colors"
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

# The manifest is XML and goes into the exe as a resource, where nothing checks
# it. Windows does, at load time: a malformed one and the exe does not start at
# all, with "the side-by-side configuration is incorrect" and no hint as to why.
# A comment holding a double hyphen was enough to do it, so check it here where
# the answer is cheap.
if [ "$TARGET" != clean ] && command -v python3 >/dev/null 2>&1; then
	python3 - "$src/jvim.manifest" <<'EOF' || exit 2
import sys, xml.dom.minidom
try:
    xml.dom.minidom.parse(sys.argv[1])
except Exception as e:
    sys.exit("%s is not valid XML: %s" % (sys.argv[1], e))
EOF
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
cp_crlf "$root/doc.j/_jvimrc" "$dist/_jvimrc.sample"
# The short one that also works on a Unix; _jvimrc.sample is the long Windows
# one. Copied to %HOME%\_jvimrc, which is read before _vimrc.
cp_crlf "$root/jvimrc.sample" "$dist/jvimrc.sample"
# The rules an rc reaches with "source $VIM/syntax/...", and $VIM with nothing
# set is this directory, so they are where the sample expects them already.
rm -rf "$dist/syntax"
mkdir -p "$dist/syntax"
for f in "$root"/syntax/*; do
	[ -f "$f" ] && cp_crlf "$f" "$dist/syntax/$(basename "$f")"
done
rm -rf "$dist/colors"
mkdir -p "$dist/colors"
for f in "$root"/colors/*; do
	[ -f "$f" ] && cp_crlf "$f" "$dist/colors/$(basename "$f")"
done


# vim.hlp is deliberately not converted: ":help" opens it as a buffer, where a
# bare LF costs nothing but a "[notextmode]" on the message line. Only what
# gets sourced has to be CRLF.
#
# Checked rather than assumed, because the failure is quiet -- the editor still
# works, it just complains on the way up, and whoever added a file to syntax/
# would not see it from a Unix.
lf_only=
for f in "$dist"/_jvimrc.sample "$dist"/jvimrc.sample "$dist"/syntax/*; do
	[ -f "$f" ] || continue
	grep -q $'\r$' "$f" || lf_only="$lf_only $(basename "$f")"
done
if [ -n "$lf_only" ]; then
	echo "these go into the package with Unix line separators:$lf_only" >&2
	echo "the editor sources them and warns once for each; see cp_crlf()." >&2
	exit 1
fi

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
