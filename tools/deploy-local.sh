#!/bin/bash
# Build and deploy JVim 3 locally for Linux / Unix verification.
#
#   tools/deploy-local.sh [PREFIX]
#
# PREFIX defaults to $HOME/.local (or $JVIM_LOCAL_PREFIX).
# Installs:
#   - $PREFIX/bin/jvim3
#   - $PREFIX/lib/jvim3.hlp
#   - $PREFIX/lib/jvim3/syntax/*.jvsyn
#   - $PREFIX/lib/jvim3/jvimrc.sample
#   - $PREFIX/man/man1/jvim3.1
set -euo pipefail

prefix_arg=${1:-}
root=$(cd "$(dirname "$0")/.." && pwd)
cd "$root"

if [ -n "$prefix_arg" ]; then
  prefix=$(mkdir -p "$prefix_arg" && cd "$prefix_arg" && pwd)
else
  prefix=${JVIM_LOCAL_PREFIX:-$HOME/.local}
fi

echo "### Building and deploying JVim 3 to $prefix"

PREFIX="$prefix" "$root/scripts/build-unix.sh" install

echo
echo "=== Deployment completed ==="
echo "Binary:  $prefix/bin/jvim3"
echo "Syntax:  $prefix/lib/jvim3/syntax/"
echo "Help:    $prefix/lib/jvim3.hlp"
echo

# Check if $prefix/bin is in PATH
case ":$PATH:" in
  *":$prefix/bin:"*)
    echo "✔ $prefix/bin is in your PATH."
    echo "  You can run: jvim3"
    ;;
  *)
    echo "ℹ Note: $prefix/bin is not in your PATH."
    echo "  You can run directly: $prefix/bin/jvim3"
    echo "  Or add to PATH: export PATH=\"$prefix/bin:\$PATH\""
    ;;
esac
