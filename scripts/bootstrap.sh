#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
TARGET_DIR="${1:-${ROOT_DIR}/.deps/mitsuba3-v3.8.0}"
UPSTREAM_URL="https://github.com/mitsuba-renderer/mitsuba3.git"
UPSTREAM_COMMIT="2ba361481801f6e158bbab2b64345f6778bd2865"

if [[ -e "${TARGET_DIR}" ]]; then
    echo "Target already exists: ${TARGET_DIR}" >&2
    exit 2
fi

mkdir -p "$(dirname "${TARGET_DIR}")"
git clone --recursive --branch v3.8.0 "${UPSTREAM_URL}" "${TARGET_DIR}"
git -C "${TARGET_DIR}" checkout "${UPSTREAM_COMMIT}"
git -C "${TARGET_DIR}" submodule update --init --recursive
"${ROOT_DIR}/scripts/apply_patches.sh" "${TARGET_DIR}"

echo "Prepared patched Mitsuba source at ${TARGET_DIR}"
