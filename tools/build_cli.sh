#!/usr/bin/env bash
# build_cli.sh -- Build paf CLI tool on Linux / macOS
#
# Usage:
#   bash tools/build_cli.sh          # builds to bin/paf
#   OUT=my_paf bash tools/build_cli.sh
#
# Must be run from the repository root.

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="${OUT:-${REPO_ROOT}/bin/paf}"

mkdir -p "${REPO_ROOT}/bin"

echo "Building paf CLI -> ${OUT}"

gcc -O2 \
    -I"${REPO_ROOT}/libpaf/include" \
    "${REPO_ROOT}/libpaf/src"/*.c \
    "${REPO_ROOT}/tools/paf_cli.c" \
    -lpthread \
    -o "${OUT}"

echo "Build successful: ${OUT}"
