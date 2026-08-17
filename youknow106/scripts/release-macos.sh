#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
CMAKE_FILE="${PROJECT_DIR}/CMakeLists.txt"
CHANGELOG_FILE="${PROJECT_DIR}/CHANGELOG.md"
USER_GUIDE_FILE="${PROJECT_DIR}/Docs/USER_GUIDE.md"
CUSTOMER_LICENSE_FILE="${PROJECT_DIR}/EULA.md"

if [[ -z "${APP_SIGN_IDENTITY:-}" || "${APP_SIGN_IDENTITY}" == "-" ]]; then
    echo "error: set APP_SIGN_IDENTITY to a Developer ID Application identity" >&2
    exit 1
fi
if [[ -z "${INSTALLER_SIGN_IDENTITY:-}" ]]; then
    echo "error: set INSTALLER_SIGN_IDENTITY to a Developer ID Installer identity" >&2
    exit 1
fi
if [[ -z "${NOTARY_PROFILE:-}" ]]; then
    echo "error: set NOTARY_PROFILE to a notarytool Keychain profile" >&2
    exit 1
fi

if [[ -n "${JUCE_PATH:-}" || -n "${YOUKNOW106_JUCE_PATH:-}" \
      || -n "${FETCHCONTENT_SOURCE_DIR_JUCE:-}" ]]; then
    echo "error: commercial releases forbid local JUCE overrides" >&2
    exit 1
fi
for tool in awk git grep sed; do
    command -v "${tool}" >/dev/null 2>&1 || {
        echo "error: release tool '${tool}' was not found" >&2
        exit 1
    }
done
if [[ ! -f "${CMAKE_FILE}" ]]; then
    echo "error: missing ${CMAKE_FILE}" >&2
    exit 1
fi
if [[ ! -f "${CHANGELOG_FILE}" ]]; then
    echo "error: missing ${CHANGELOG_FILE}" >&2
    exit 1
fi
if [[ ! -f "${USER_GUIDE_FILE}" ]]; then
    echo "error: missing ${USER_GUIDE_FILE}" >&2
    exit 1
fi
if [[ ! -f "${CUSTOMER_LICENSE_FILE}" ]]; then
    echo "error: add the approved customer licence at ${CUSTOMER_LICENSE_FILE}" >&2
    exit 1
fi

PROJECT_VERSION="$(awk '
    /^[[:space:]]*project\(YouKnow106[[:space:]]+VERSION[[:space:]]+/ {
        for (field = 1; field <= NF; ++field) {
            if ($field == "VERSION") {
                print $(field + 1)
                exit
            }
        }
    }
' "${CMAKE_FILE}")"
if [[ -z "${PROJECT_VERSION}" ]]; then
    echo "error: could not read the YouKnow106 version from CMakeLists.txt" >&2
    exit 1
fi
CHANGELOG_DATE="$(awk -v prefix="## ${PROJECT_VERSION} - " \
    'index($0, prefix) == 1 { print substr($0, length(prefix) + 1); exit }' \
    "${CHANGELOG_FILE}")"
if [[ ! "${CHANGELOG_DATE}" =~ ^[0-9]{4}-[0-9]{2}-[0-9]{2}$ ]]; then
    echo "error: CHANGELOG.md needs a dated ${PROJECT_VERSION} release heading" >&2
    exit 1
fi
if ! grep -Fq "YouKnow106 ${PROJECT_VERSION} is" "${USER_GUIDE_FILE}" \
        || ! grep -Fq "YouKnow106-${PROJECT_VERSION}-macOS-universal.pkg" \
            "${USER_GUIDE_FILE}"; then
    echo "error: user guide does not match version ${PROJECT_VERSION} and its universal PKG" >&2
    exit 1
fi
if ! git -C "${PROJECT_DIR}" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    echo "error: commercial releases must be built from a git worktree" >&2
    exit 1
fi
SOURCE_CHANGES="$(git -C "${PROJECT_DIR}" status --porcelain --untracked-files=normal)"
if [[ -n "${SOURCE_CHANGES}" ]]; then
    echo "error: commercial releases require a clean source tree" >&2
    printf '%s\n' "${SOURCE_CHANGES}" >&2
    exit 1
fi

EXPECTED_TAG="youknow106-v${PROJECT_VERSION}"
HEAD_COMMIT="$(git -C "${PROJECT_DIR}" rev-parse HEAD)"
TAG_COMMIT="$(git -C "${PROJECT_DIR}" rev-parse -q --verify \
    "refs/tags/${EXPECTED_TAG}^{commit}" 2>/dev/null || true)"
if [[ -z "${TAG_COMMIT}" || "${TAG_COMMIT}" != "${HEAD_COMMIT}" ]]; then
    echo "error: tag ${EXPECTED_TAG} must point exactly at HEAD (${HEAD_COMMIT})" >&2
    exit 1
fi

EXPECTED_BUILD_DIR="${PROJECT_DIR}/build-macos"
if [[ -n "${BUILD_DIR:-}" && "${BUILD_DIR}" != "${EXPECTED_BUILD_DIR}" ]]; then
    echo "error: commercial releases use the fixed build directory ${EXPECTED_BUILD_DIR}" >&2
    exit 1
fi
export BUILD_DIR="${EXPECTED_BUILD_DIR}"
if [[ -e "${BUILD_DIR}" || -L "${BUILD_DIR}" ]]; then
    echo "error: commercial releases require a fresh build directory" >&2
    echo "Move or remove ${BUILD_DIR}, then rerun the release." >&2
    exit 1
fi

unset JUCE_PATH YOUKNOW106_JUCE_PATH FETCHCONTENT_SOURCE_DIR_JUCE
export CONFIG=Release
export MACOSX_DEPLOYMENT_TARGET=11.0
export BUILD_UNIVERSAL=ON
export VERSION="${PROJECT_VERSION}"
export RELEASE_MODE=1
"${SCRIPT_DIR}/build-macos.sh"
exec "${SCRIPT_DIR}/sign-and-package-macos.sh"
