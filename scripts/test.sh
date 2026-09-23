#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SOURCE_DIR="${1:-${ROOT_DIR}/.deps/mitsuba3-v3.8.0}"
BUILD_DIR="${2:-${ROOT_DIR}/build/mitsuba3-v3.8.0}"

if [[ ! -f "${BUILD_DIR}/setpath.sh" ]]; then
    echo "Built Mitsuba environment not found: ${BUILD_DIR}/setpath.sh" >&2
    exit 2
fi

set +u
source "${BUILD_DIR}/setpath.sh"
set -u

python -m pytest -q \
    "${SOURCE_DIR}/src/sensors/tests/test_telecentric_microscope.py" \
    "${SOURCE_DIR}/src/emitters/tests/test_kohler.py" \
    "${SOURCE_DIR}/src/render/tests/test_microfacet.py"
