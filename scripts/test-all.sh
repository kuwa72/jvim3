#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

echo "=== Building jvim3 ==="
"${ROOT_DIR}/scripts/build-unix.sh"

echo "=== Running Editing Tests ==="
"${ROOT_DIR}/scripts/test-editing.sh"

echo "=== Running Encoding Tests ==="
"${ROOT_DIR}/scripts/test-encoding.sh"

echo "=== Running Syntax Tests ==="
"${ROOT_DIR}/scripts/test-syntax.sh"

echo "=== Running Hostile Input Tests ==="
"${ROOT_DIR}/scripts/test-hostile.sh"

echo "=== Running SGR Tests ==="
"${ROOT_DIR}/scripts/test-sgr.sh"

echo "=== All Tests Passed Successfully ==="
