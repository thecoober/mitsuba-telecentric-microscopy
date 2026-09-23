#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SOURCE_DIR="${1:-${ROOT_DIR}/.deps/mitsuba3-v3.8.0}"
BUILD_DIR="${2:-${ROOT_DIR}/build/mitsuba3-v3.8.0}"

if [[ ! -f "${SOURCE_DIR}/CMakeLists.txt" ]]; then
    echo "Mitsuba source not found: ${SOURCE_DIR}" >&2
    echo "Run scripts/bootstrap.sh first." >&2
    exit 2
fi

cmake -S "${SOURCE_DIR}" -B "${BUILD_DIR}" -GNinja \
    -DCMAKE_BUILD_TYPE=Release
cmake --build "${BUILD_DIR}"

echo "Built patched Mitsuba in ${BUILD_DIR}"
