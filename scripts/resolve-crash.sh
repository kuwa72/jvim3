#!/bin/bash
#
# Turn a JVim crash report into source locations.
#
#   scripts/resolve-crash.sh <report.log> [exe]
#
# The exe defaults to dist/<arch>/jvim32w.exe, then src/jvim32w.exe. It must be
# the same build that crashed, with its debug info (built with -g, or with the
# .exe.debug file next to it from "make split").
#
# The report logs each frame as
#   #01 0x00401234  jvim32w.exe+0x1234  static=0x00401234
# "static" is the address with ASLR undone, which is what addr2line wants.

set -euo pipefail

log=${1:-}
if [ -z "$log" ] || [ ! -f "$log" ]; then
	echo "usage: $0 <report.log> [exe]" >&2
	exit 2
fi

root=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
exe=${2:-}
if [ -z "$exe" ]; then
	for cand in "$root"/dist/*/jvim32w.exe "$root"/dist/*/jvim32.exe \
				"$root/src/jvim32w.exe" "$root/src/jvim32.exe"; do
		[ -f "$cand" ] && { exe=$cand; break; }
	done
fi
if [ -z "$exe" ] || [ ! -f "$exe" ]; then
	echo "no executable found; pass it as the second argument" >&2
	exit 2
fi

# Pick an addr2line that can read this PE.
a2l=""
for cand in ${ADDR2LINE:-} i686-w64-mingw32-addr2line x86_64-w64-mingw32-addr2line addr2line; do
	command -v "$cand" >/dev/null 2>&1 && { a2l=$cand; break; }
done
[ -n "$a2l" ] || { echo "addr2line not found" >&2; exit 2; }

module=$(basename "$exe")
echo "report : $log"
echo "exe    : $exe"
echo "using  : $a2l"
echo

# Header lines of the report are worth seeing before the stack.
sed -n '1,/^stack/p' "$log" | sed '$d'

echo "stack:"
matched=0
while read -r line; do
	# frame label, module+offset, static address
	label=$(echo "$line" | grep -o '#[0-9][0-9]*' | head -1)
	mod=$(echo "$line" | sed -n 's/.*  \([^ ]*\)+0x[0-9a-fA-F]*  static=.*/\1/p')
	addr=$(echo "$line" | sed -n 's/.*static=\(0x[0-9a-fA-F]*\).*/\1/p')
	[ -n "$addr" ] || continue

	if [ "$mod" != "$module" ]; then
		printf '  %-4s %s  (not %s, skipped)\n' "${label:-#??}" "$mod" "$module"
		continue
	fi
	matched=1
	# -i expands inlined frames, -f gives the function, -C demangles.
	loc=$("$a2l" -e "$exe" -f -C -i "$addr" 2>/dev/null | paste - - | \
			sed 's/\t/  at  /' | sed 's/^/        /')
	printf '  %-4s %s %s\n' "${label:-#??}" "$mod" "$addr"
	printf '%s\n' "$loc"
done < <(grep 'static=0x' "$log")

if [ "$matched" = 0 ]; then
	echo
	echo "No frame belonged to $module. Either the report came from a"
	echo "different build, or the fault was inside a system DLL (see the"
	echo "module names above)."
fi

dmp=${log%.log}.dmp
if [ -f "$dmp" ]; then
	echo
	echo "minidump: $dmp"
	echo "  open it in Visual Studio or WinDbg on the Windows side for a"
	echo "  full postmortem (locals, other threads, heap)."
fi
