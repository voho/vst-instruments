#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
EXPECTED_BUILD_DIR="${PROJECT_DIR}/build-macos"
if [[ -n "${BUILD_DIR:-}" && "${BUILD_DIR}" != "${EXPECTED_BUILD_DIR}" ]]; then
    echo "error: commercial releases use the fixed build directory ${EXPECTED_BUILD_DIR}" >&2
    exit 1
fi

export BUILD_DIR="${EXPECTED_BUILD_DIR}"
export CONFIG=Release
export MACOSX_DEPLOYMENT_TARGET=11.0
export BUILD_UNIVERSAL=ON
export RELEASE_MODE=1

# Run the packager's source, documentation and credential checks before the
# full build. Preflight never stages, signs, submits or deletes artifacts.
"${SCRIPT_DIR}/sign-and-package-macos.sh" --preflight
if [[ -e "${BUILD_DIR}" || -L "${BUILD_DIR}" ]]; then
    echo "error: commercial releases require a fresh build directory" >&2
    echo "Move or remove ${BUILD_DIR}, then rerun the release." >&2
    exit 1
fi

"${SCRIPT_DIR}/build-macos.sh"
exec "${SCRIPT_DIR}/sign-and-package-macos.sh"
