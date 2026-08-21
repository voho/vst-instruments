#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${PROJECT_DIR}/build-macos}"
CONFIG="${CONFIG:-Release}"
VERSION="${VERSION:-1.1.0}"
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
