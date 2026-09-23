#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MITSUBA_DIR="${1:-}"
EXPECTED_COMMIT="2ba361481801f6e158bbab2b64345f6778bd2865"

if [[ -z "${MITSUBA_DIR}" ]]; then
    echo "Usage: $0 /path/to/mitsuba3" >&2
    exit 2
fi

MITSUBA_DIR="$(cd "${MITSUBA_DIR}" && pwd)"
if [[ ! -e "${MITSUBA_DIR}/.git" ]]; then
    echo "Not a Mitsuba Git checkout: ${MITSUBA_DIR}" >&2
    exit 2
fi

actual_commit="$(git -C "${MITSUBA_DIR}" rev-parse HEAD)"
if [[ "${actual_commit}" != "${EXPECTED_COMMIT}" &&
      "${MITSUBA_TELECENTRIC_ALLOW_UNPINNED:-0}" != "1" ]]; then
    echo "Expected Mitsuba commit ${EXPECTED_COMMIT}, got ${actual_commit}." >&2
    echo "Set MITSUBA_TELECENTRIC_ALLOW_UNPINNED=1 only for a deliberate port." >&2
    exit 2
fi

if [[ -n "$(git -C "${MITSUBA_DIR}" status --porcelain)" ]]; then
    echo "Mitsuba checkout must be clean before applying the extension." >&2
    exit 2
fi

patches=(
    "${ROOT_DIR}/patches/mitsuba-3.8.0/0001-reference-dependent-kohler-integration.patch"
    "${ROOT_DIR}/patches/mitsuba-3.8.0/0002-ggx-visible-normal-boundary-stability.patch"
)

for patch in "${patches[@]}"; do
    git -C "${MITSUBA_DIR}" apply --check "${patch}"
done

cp -a "${ROOT_DIR}/extension/include/." "${MITSUBA_DIR}/include/"
cp -a "${ROOT_DIR}/extension/src/." "${MITSUBA_DIR}/src/"

for patch in "${patches[@]}"; do
    git -C "${MITSUBA_DIR}" apply "${patch}"
done

echo "Applied Mitsuba Telecentric Microscopy 0.1.1 to ${MITSUBA_DIR}"
