#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${PROJECT_DIR}/build-macos}"
CONFIG="${CONFIG:-Release}"
VERSION="${VERSION:-1.0.0}"
APP_SIGN_IDENTITY="${APP_SIGN_IDENTITY:--}"
INSTALLER_SIGN_IDENTITY="${INSTALLER_SIGN_IDENTITY:-}"
NOTARY_PROFILE="${NOTARY_PROFILE:-}"
ARTIFACT_DIR="${BUILD_DIR}/Drumalor_artefacts/${CONFIG}"
DIST_DIR="${BUILD_DIR}/dist"

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "error: this script requires macOS" >&2
    exit 1
fi

if [[ ! "${VERSION}" =~ ^[0-9A-Za-z][0-9A-Za-z._-]*$ ]]; then
    echo "error: VERSION contains unsafe filename characters: ${VERSION}" >&2
    exit 1
fi

for tool in codesign ditto lipo mktemp pkgbuild; do
    command -v "${tool}" >/dev/null 2>&1 || {
        echo "error: required tool '${tool}' was not found" >&2
        exit 1
    }
done

VST3="${ARTIFACT_DIR}/VST3/Drumalor.vst3"
AU="${ARTIFACT_DIR}/AU/Drumalor.component"
APP="${ARTIFACT_DIR}/Standalone/Drumalor.app"

for artifact in "${VST3}" "${AU}" "${APP}"; do
    if [[ ! -d "${artifact}" ]]; then
        echo "error: missing build artifact: ${artifact}" >&2
        echo "Run scripts/build-macos.sh first." >&2
        exit 1
    fi
done

executables=(
    "${VST3}/Contents/MacOS/Drumalor"
    "${AU}/Contents/MacOS/Drumalor"
    "${APP}/Contents/MacOS/Drumalor"
)
ARCHITECTURES=""
for executable in "${executables[@]}"; do
    current_architectures="$(lipo -archs "${executable}")"
    if [[ -z "${ARCHITECTURES}" ]]; then
        ARCHITECTURES="${current_architectures}"
    elif [[ "${current_architectures}" != "${ARCHITECTURES}" ]]; then
        echo "error: build artifacts have mismatched architectures" >&2
        exit 1
    fi
done

if [[ " ${ARCHITECTURES} " == *" arm64 "* \
      && " ${ARCHITECTURES} " == *" x86_64 "* ]]; then
    ARCH_LABEL="universal"
else
    ARCH_LABEL="${ARCHITECTURES// /-}"
fi

mkdir -p "${BUILD_DIR}" "${DIST_DIR}"
PACKAGE_ROOT="$(mktemp -d "${BUILD_DIR%/}/drumalor-package-root.XXXXXX")"
cleanup() {
    rm -rf -- "${PACKAGE_ROOT}"
}
trap cleanup EXIT

mkdir -p \
    "${PACKAGE_ROOT}/Library/Audio/Plug-Ins/VST3" \
    "${PACKAGE_ROOT}/Library/Audio/Plug-Ins/Components" \
    "${PACKAGE_ROOT}/Applications"

ditto "${VST3}" "${PACKAGE_ROOT}/Library/Audio/Plug-Ins/VST3/Drumalor.vst3"
ditto "${AU}" "${PACKAGE_ROOT}/Library/Audio/Plug-Ins/Components/Drumalor.component"
ditto "${APP}" "${PACKAGE_ROOT}/Applications/Drumalor.app"

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

sign_bundle "${PACKAGE_ROOT}/Library/Audio/Plug-Ins/VST3/Drumalor.vst3"
sign_bundle "${PACKAGE_ROOT}/Library/Audio/Plug-Ins/Components/Drumalor.component"
sign_bundle "${PACKAGE_ROOT}/Applications/Drumalor.app"

ZIP_PATH="${DIST_DIR}/Drumalor-${VERSION}-macOS-${ARCH_LABEL}.zip"
PKG_UNSIGNED="${DIST_DIR}/Drumalor-${VERSION}-unsigned.pkg"
PKG_FINAL="${DIST_DIR}/Drumalor-${VERSION}-macOS-${ARCH_LABEL}.pkg"

rm -f "${ZIP_PATH}" "${PKG_UNSIGNED}" "${PKG_FINAL}"
ditto -c -k --sequesterRsrc "${PACKAGE_ROOT}" "${ZIP_PATH}"

pkgbuild \
    --root "${PACKAGE_ROOT}" \
    --identifier audio.drumalor.synth.pkg \
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
    echo "Note: notarization was applied to the installer package, not the ZIP." >&2
fi

echo
echo "Packaging complete:"
echo "  ${ZIP_PATH}"
echo "  ${PKG_FINAL}"
