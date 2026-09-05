#!/bin/bash
# Build, package and deploy Windows binaries where the real Windows machine
# can run and test them. Runs on the WSL host (uses mingw-w64 toolchain and
# /mnt/c or wslpath for the copy).
#
#   tools/deploy-windows.sh [DEST]
#
# DEST is where the deployed per-arch folders go. With no argument it comes
# from $JVIM_DEPLOY_DEST (or $JVIM3_DEPLOY_DEST), and failing that from the
# Windows %USERPROFILE%\Downloads\jvim3-latest asked of cmd.exe -- not from
# $USER (which may differ between WSL and Windows).
#
# Safe in-place replacement:
#   - Detects if an exe is currently running in Windows and refuses to touch it.
#   - Cleans the contents inside the folder rather than removing the directory
#     itself (preventing EACCES from Windows Explorer or open shells).
#   - Preserves user custom _vimrc / _jvimrc if placed in the deploy folder.
set -euo pipefail

# Resolve DEST before cd, so a relative argument means what the caller meant.
dest_arg=${1:-}
if [ -n "$dest_arg" ]; then
  dest=$(cd "$(dirname "$dest_arg")" 2>/dev/null && pwd)/$(basename "$dest_arg") \
    || dest=$dest_arg
fi

root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"

# Ask Windows for its own profile directory and translate it to a /mnt path.
windows_downloads() {
  local up
  up=$(cmd.exe /c 'echo %USERPROFILE%' 2>/dev/null | tr -d '\r\n') || return 1
  case $up in
    [A-Za-z]:\\*) ;;
    *) return 1 ;;
  esac
  if command -v wslpath >/dev/null 2>&1; then
    wslpath -u "$up/Downloads" 2>/dev/null && return 0
  fi
  local drive=${up%%:*}
  local rest=${up#*:}
  printf '/mnt/%s%s/Downloads' \
    "$(printf '%s' "$drive" | tr 'A-Z' 'a-z')" \
    "$(printf '%s' "$rest" | tr '\\' '/')"
}

if [ -z "${dest:-}" ]; then
  dest=${JVIM_DEPLOY_DEST:-${JVIM3_DEPLOY_DEST:-}}
fi
if [ -z "$dest" ]; then
  dl=$(windows_downloads) || dl=""
  [ -n "$dl" ] && dest=$dl/jvim3-latest
fi
if [ -z "$dest" ]; then
  echo "deploy: could not work out where to deploy." >&2
  echo "deploy: pass it, or set JVIM_DEPLOY_DEST, e.g." >&2
  echo "deploy:   tools/deploy-windows.sh /mnt/c/Users/<you>/Downloads/jvim3-latest" >&2
  exit 1
fi

# Only ever create the leaf. Creating parents would mean the path is wrong
# (a mistyped or guessed user name), and /mnt/c/Users is not writable anyway.
if [ ! -d "$dest" ]; then
  parent=$(dirname "$dest")
  if [ ! -d "$parent" ]; then
    echo "deploy: $parent does not exist -- is $dest the right place?" >&2
    exit 1
  fi
  mkdir -p "$dest"
fi
echo "### deploying to $dest"

sha=$(git rev-parse --short HEAD 2>/dev/null || echo "unknown")
version=$(tr -d ' \t\r\n' < "$root/VERSION" 2>/dev/null || echo "1.0.0")

# Build both 32-bit and 64-bit release packages
"$root/scripts/build-mingw.sh" release

declare -A arch_bits=([i686]=32 [x86_64]=64)

for arch in i686 x86_64; do
  bits=${arch_bits[$arch]}
  name="win$bits"
  folder_name="jvim3-$name"
  echo "### $arch ($name) -> $folder_name"

  zip=$(ls "$root"/release/jvim3-*-win$bits.zip 2>/dev/null | head -1)
  [ -n "$zip" ] && [ -f "$zip" ] || {
    echo "deploy: package for win$bits was not produced" >&2
    exit 1
  }

  out=$dest/$folder_name

  # Refuse to touch a running install. Windows keeps a running .exe open with
  # FILE_SHARE_READ only, so opening it for write fails -- that is the probe.
  for exe in "$out"/*.exe; do
    [ -e "$exe" ] || continue
    if ! (exec 3<>"$exe") 2>/dev/null; then
      echo "deploy: $(basename "$exe") in $out is in use; close jvim and run again" >&2
      exit 1
    fi
  done

  # Unpack first, swap second, so a failure part way through leaves the
  # existing install alone.
  rm -rf "$out.tmp"
  mkdir -p "$out.tmp"
  if command -v unzip >/dev/null 2>&1; then
    unzip -q "$zip" -d "$out.tmp"
  else
    python3 -m zipfile -e "$zip" "$out.tmp"
  fi

  inner=$(find "$out.tmp" -mindepth 1 -maxdepth 1 -type d | head -1)
  inner=${inner:-$out.tmp}

  # Replace the *contents*, never the directory itself (which causes EACCES if
  # Windows Explorer or another shell is open in it).
  # Preserve any user custom _vimrc / _jvimrc.
  mkdir -p "$out"
  find "$out" -mindepth 1 -maxdepth 1 ! -name _vimrc ! -name .vimrc ! -name _jvimrc -exec rm -rf {} + 2>/dev/null || {
    echo "deploy: could not clear $out (a file in it is in use?)" >&2
    exit 1
  }
  cp -r "$inner"/. "$out"/
  rm -rf "$out.tmp"

  # Copy the zip archive to destination as well
  cp "$zip" "$dest/"

  # Verification
  gui_exe="$out/jvim${bits}w.exe"
  con_exe="$out/jvim${bits}.exe"
  hlp_file="$out/vim.hlp"
  tutor_bat="$out/jvimtutor.bat"
  syntax_count=$(ls "$out"/syntax/*.jvsyn 2>/dev/null | wc -l)

  printf '  %-14s gui=%s con=%s hlp=%s tutor=%s syntax=%d\n' \
    "$folder_name" \
    "$([ -f "$gui_exe" ] && echo yes || echo NO)" \
    "$([ -f "$con_exe" ] && echo yes || echo NO)" \
    "$([ -f "$hlp_file" ] && echo yes || echo NO)" \
    "$([ -f "$tutor_bat" ] && echo yes || echo NO)" \
    "$syntax_count"

  if [ ! -f "$gui_exe" ] || [ ! -f "$con_exe" ] || [ ! -f "$hlp_file" ] || [ ! -f "$tutor_bat" ] || [ "$syntax_count" -eq 0 ]; then
    echo "deploy: incomplete installation in $out" >&2
    exit 1
  fi
done

echo "### deployed to $dest ($sha)"
ls -la "$dest"
