#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${PROJECT_DIR}/build-macos}"
CONFIG="${CONFIG:-Release}"
# The default rides the CMake project version so a manual run can never
# label packages with a number the binaries do not carry.
if [[ -z "${VERSION:-}" ]]; then
    VERSION="$(sed -n 's/^project(Ghost VERSION \([0-9][0-9.]*\).*/\1/p' \
        "${PROJECT_DIR}/CMakeLists.txt")"
fi
if [[ ! "${VERSION}" =~ ^[0-9]+(\.[0-9]+){1,3}$ ]]; then
    echo "error: could not determine a valid version: '${VERSION}'" >&2
    exit 1
fi
APP_SIGN_IDENTITY="${APP_SIGN_IDENTITY:--}"
INSTALLER_SIGN_IDENTITY="${INSTALLER_SIGN_IDENTITY:-}"
NOTARY_PROFILE="${NOTARY_PROFILE:-}"
ARTIFACT_DIR="${BUILD_DIR}/Ghost_artefacts/${CONFIG}"
DIST_DIR="${BUILD_DIR}/dist"
PACKAGE_ROOT="${BUILD_DIR}/package-root"

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "error: this script requires macOS" >&2
    exit 1
fi

for tool in codesign ditto pkgbuild; do
    command -v "${tool}" >/dev/null 2>&1 || {
        echo "error: required tool '${tool}' was not found" >&2
        exit 1
    }
done

VST3="${ARTIFACT_DIR}/VST3/Ghost.vst3"
AU="${ARTIFACT_DIR}/AU/Ghost.component"
APP="${ARTIFACT_DIR}/Standalone/Ghost.app"

for artifact in "${VST3}" "${AU}" "${APP}"; do
    if [[ ! -d "${artifact}" ]]; then
        echo "error: missing build artifact: ${artifact}" >&2
        echo "Run scripts/build-macos.sh first." >&2
        exit 1
    fi
done

case "${PACKAGE_ROOT}" in
    "${BUILD_DIR}"/*) ;;
    *) echo "error: unsafe package staging path: ${PACKAGE_ROOT}" >&2; exit 1 ;;
esac

rm -rf "${PACKAGE_ROOT}"
mkdir -p \
    "${PACKAGE_ROOT}/Library/Audio/Plug-Ins/VST3" \
    "${PACKAGE_ROOT}/Library/Audio/Plug-Ins/Components" \
    "${PACKAGE_ROOT}/Applications" \
    "${DIST_DIR}"

ditto "${VST3}" "${PACKAGE_ROOT}/Library/Audio/Plug-Ins/VST3/Ghost.vst3"
ditto "${AU}" "${PACKAGE_ROOT}/Library/Audio/Plug-Ins/Components/Ghost.component"
ditto "${APP}" "${PACKAGE_ROOT}/Applications/Ghost.app"

# Licence and notice files ride inside the package root at the standard
# documentation path, so the drag-install zip and the installer package
# both carry them (the MIT licence requires the notice to accompany
# binaries; the installer lands them at /Library/Documentation/Ghost).
DOC_DIR="${PACKAGE_ROOT}/Library/Documentation/Ghost"
mkdir -p "${DOC_DIR}"
cp "${PROJECT_DIR}/LICENSE" \
   "${PROJECT_DIR}/THIRD_PARTY_NOTICES.md" \
   "${PROJECT_DIR}/ThirdParty/JUCE-LICENSE.md" \
   "${DOC_DIR}/"

sign_bundle() {
    local bundle="$1"
    if [[ "${APP_SIGN_IDENTITY}" == "-" ]]; then
        codesign --force --sign - "${bundle}"
    else
        codesign --force --options runtime --timestamp \
            --sign "${APP_SIGN_IDENTITY}" "${bundle}"
    fi
    codesign --verify --deep --strict --verbose=2 "${bundle}"
}

sign_bundle "${PACKAGE_ROOT}/Library/Audio/Plug-Ins/VST3/Ghost.vst3"
sign_bundle "${PACKAGE_ROOT}/Library/Audio/Plug-Ins/Components/Ghost.component"
sign_bundle "${PACKAGE_ROOT}/Applications/Ghost.app"

ZIP_PATH="${DIST_DIR}/Ghost-${VERSION}-macOS-universal.zip"
PKG_UNSIGNED="${DIST_DIR}/Ghost-${VERSION}-unsigned.pkg"
PKG_FINAL="${DIST_DIR}/Ghost-${VERSION}-macOS-universal.pkg"

rm -f "${ZIP_PATH}" "${PKG_UNSIGNED}" "${PKG_FINAL}"
ditto -c -k --sequesterRsrc "${PACKAGE_ROOT}" "${ZIP_PATH}"

pkgbuild \
    --root "${PACKAGE_ROOT}" \
    --identifier audio.ghost.synth.pkg \
    --version "${VERSION}" \
    --install-location / \
    "${PKG_UNSIGNED}"

if [[ -n "${INSTALLER_SIGN_IDENTITY}" ]]; then
    command -v productsign >/dev/null 2>&1 || {
        echo "error: productsign was not found" >&2
        exit 1
    }
    productsign --sign "${INSTALLER_SIGN_IDENTITY}" \
        "${PKG_UNSIGNED}" "${PKG_FINAL}"
    rm -f "${PKG_UNSIGNED}"
else
    mv "${PKG_UNSIGNED}" "${PKG_FINAL}"
fi

if [[ -n "${NOTARY_PROFILE}" ]]; then
    if [[ "${APP_SIGN_IDENTITY}" == "-" || -z "${INSTALLER_SIGN_IDENTITY}" ]]; then
        echo "error: notarization requires Developer ID Application and Installer identities" >&2
        exit 1
    fi
    xcrun notarytool submit "${PKG_FINAL}" \
        --keychain-profile "${NOTARY_PROFILE}" --wait
    xcrun stapler staple "${PKG_FINAL}"
    xcrun stapler validate "${PKG_FINAL}"
fi

echo
echo "Packaging complete:"
echo "  ${ZIP_PATH}"
echo "  ${PKG_FINAL}"
