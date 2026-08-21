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
#   #01 0x00401234  jvim32w.exe+0x1234
#
# The address addr2line wants is the offset in the module plus the exe's
# ImageBase, which is worked out here. Reports from before 2026-08 also carry a
# "static=" field; it is ignored, because the loader rewrites the ImageBase in a
# relocated image's header and it therefore came out equal to the running address
# (see the note at the top of src/w32crash.c).

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

# The matching objdump, to read the exe's ImageBase.
objd=${a2l%addr2line}objdump
command -v "$objd" >/dev/null 2>&1 || objd=objdump
# Captured, not piped: awk leaving early under pipefail kills objdump with
# SIGPIPE and takes the whole script with it (the same trap as in
# scripts/build-mingw.sh).
hdr=$("$objd" -p "$exe" 2>/dev/null) || hdr=""
imagebase=$(printf '%s\n' "$hdr" | sed -n 's/^ImageBase[[:space:]]*\([0-9a-fA-F]*\).*/0x\1/p')
imagebase=${imagebase%%$'\n'*}
[ -n "$imagebase" ] || imagebase=0

module=$(basename "$exe")
echo "report : $log"
echo "exe    : $exe"
echo "using  : $a2l"
printf 'base   : %s\n' "$(printf '0x%08x' "$imagebase")"
echo

# Header lines of the report are worth seeing before the stack.
sed -n '1,/^stack/p' "$log" | sed '$d'

echo "stack:"
matched=0
while read -r line; do
	# frame label and module+offset.
	#
	# sed, not "grep -o ... | head -1": the "faulting pc" line carries no #NN,
	# and under "set -e" a grep that matches nothing takes the whole script down
	# -- silently, right after it has printed "stack:". That, not the addresses,
	# is why this used to resolve nothing at all.
	label=$(printf '%s' "$line" | sed -n 's/.*\(#[0-9][0-9]*\).*/\1/p')
	[ -n "$label" ] || label="pc"
	mod=$(echo "$line" | sed -n 's/.*  \([^ ]*\)+0x[0-9a-fA-F]*.*/\1/p')
	off=$(echo "$line" | sed -n 's/.*+\(0x[0-9a-fA-F]*\).*/\1/p')
	addr=""
	if [ -n "$off" ] && [ "$imagebase" != 0 ]; then
		addr=$(printf '0x%x' $((imagebase + off)))
	fi
	[ -n "$addr" ] || continue

	if [ "$mod" != "$module" ]; then
		printf '  %-4s %s  (not %s, skipped)\n' "$label" "$mod" "$module"
		continue
	fi
	matched=1
	# -i expands inlined frames, -f gives the function, -C demangles.
	loc=$("$a2l" -e "$exe" -f -C -i "$addr" 2>/dev/null | paste - - | \
			sed 's/\t/  at  /' | sed 's/^/        /')
	printf '  %-4s %s %s\n' "$label" "$mod" "$addr"
	printf '%s\n' "$loc"
done < <(grep -E '^  (#[0-9]+|faulting pc) .*\+0x' "$log")

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
