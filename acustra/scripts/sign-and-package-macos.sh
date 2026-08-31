#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${PROJECT_DIR}/build-macos}"
CONFIG="${CONFIG:-Release}"
VERSION_OVERRIDE="${VERSION:-}"
APP_SIGN_IDENTITY="${APP_SIGN_IDENTITY:--}"
INSTALLER_SIGN_IDENTITY="${INSTALLER_SIGN_IDENTITY:-}"
NOTARY_PROFILE="${NOTARY_PROFILE:-}"
ARTIFACT_DIR="${BUILD_DIR}/Acustra_artefacts/${CONFIG}"
DIST_DIR="${BUILD_DIR}/dist"
PACKAGE_ROOT="${BUILD_DIR}/package-root"

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "error: this script requires macOS" >&2
    exit 1
fi
for tool in codesign ditto lipo pkgbuild; do
    command -v "${tool}" >/dev/null 2>&1 || {
        echo "error: required tool '${tool}' was not found" >&2
        exit 1
    }
done

VST3="${ARTIFACT_DIR}/VST3/Acustra.vst3"
AU="${ARTIFACT_DIR}/AU/Acustra.component"
APP="${ARTIFACT_DIR}/Standalone/Acustra.app"
for artifact in "${VST3}" "${AU}" "${APP}"; do
    if [[ ! -d "${artifact}" ]]; then
        echo "error: missing build artifact: ${artifact}" >&2
        echo "Run scripts/build-macos.sh first." >&2
        exit 1
    fi
done

PLIST_BUDDY=/usr/libexec/PlistBuddy
if [[ ! -x "${PLIST_BUDDY}" ]]; then
    echo "error: PlistBuddy was not found" >&2
    exit 1
fi
bundle_version() {
    "${PLIST_BUDDY}" -c "Print :CFBundleShortVersionString" \
        "$1/Contents/Info.plist"
}

VST3_VERSION="$(bundle_version "${VST3}")"
AU_VERSION="$(bundle_version "${AU}")"
APP_VERSION="$(bundle_version "${APP}")"
if [[ -z "${VST3_VERSION}" || "${VST3_VERSION}" != "${AU_VERSION}" \
      || "${VST3_VERSION}" != "${APP_VERSION}" ]]; then
    echo "error: build artifact versions disagree" >&2
    echo "  VST3: ${VST3_VERSION:-missing}" >&2
    echo "  AU: ${AU_VERSION:-missing}" >&2
    echo "  App: ${APP_VERSION:-missing}" >&2
    exit 1
fi
VERSION="${VST3_VERSION}"
if [[ -n "${VERSION_OVERRIDE}" && "${VERSION_OVERRIDE}" != "${VERSION}" ]]; then
    echo "error: VERSION=${VERSION_OVERRIDE} does not match bundle version ${VERSION}" >&2
    exit 1
fi

VST3_ARCHS="$(lipo -archs "${VST3}/Contents/MacOS/Acustra")"
AU_ARCHS="$(lipo -archs "${AU}/Contents/MacOS/Acustra")"
APP_ARCHS="$(lipo -archs "${APP}/Contents/MacOS/Acustra")"
if [[ "${VST3_ARCHS}" != "${AU_ARCHS}" || "${VST3_ARCHS}" != "${APP_ARCHS}" ]]; then
    echo "error: build artifact architectures disagree" >&2
    exit 1
fi
if [[ "${APP_ARCHS}" == *arm64* && "${APP_ARCHS}" == *x86_64* ]]; then
    ARTIFACT_ARCH=universal
else
    ARTIFACT_ARCH="${APP_ARCHS// /-}"
fi

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
ditto "${VST3}" "${PACKAGE_ROOT}/Library/Audio/Plug-Ins/VST3/Acustra.vst3"
ditto "${AU}" "${PACKAGE_ROOT}/Library/Audio/Plug-Ins/Components/Acustra.component"
ditto "${APP}" "${PACKAGE_ROOT}/Applications/Acustra.app"

NOTICE_ROOT="${PACKAGE_ROOT}/Library/Application Support/Acustra/Documentation"
JUCE_NOTICE="${PROJECT_DIR}/ThirdParty/JUCE-LICENSE.md"
CC0_NOTICE="${PROJECT_DIR}/ThirdParty/CC0-1.0.txt"
FREEPATS_NOTICE="${PROJECT_DIR}/ThirdParty/FreePats-Spanish-Classical-Guitar-README.txt"
EASTMAN_NOTICE="${PROJECT_DIR}/ThirdParty/Eastman-E1D-README.md"
SHINY_NOTICE="${PROJECT_DIR}/ThirdParty/Shinyguitar-README.txt"
BANK_MANIFEST="${PROJECT_DIR}/Assets/SampleBank/manifest.json"
for notice in "${PROJECT_DIR}/LICENSE" "${PROJECT_DIR}/THIRD_PARTY_NOTICES.md" \
              "${JUCE_NOTICE}" "${CC0_NOTICE}" "${FREEPATS_NOTICE}" \
              "${EASTMAN_NOTICE}" "${SHINY_NOTICE}" "${BANK_MANIFEST}"; do
    if [[ ! -f "${notice}" ]]; then
        echo "error: missing distribution notice: ${notice}" >&2
        exit 1
    fi
done

copy_documentation() {
    local destination="$1"
    mkdir -p "${destination}/ThirdParty" \
        "${destination}/Assets/SampleBank"
    ditto "${PROJECT_DIR}/LICENSE" "${destination}/LICENSE"
    ditto "${PROJECT_DIR}/THIRD_PARTY_NOTICES.md" \
        "${destination}/THIRD_PARTY_NOTICES.md"
    ditto "${JUCE_NOTICE}" "${destination}/ThirdParty/JUCE-LICENSE.md"
    ditto "${CC0_NOTICE}" "${destination}/ThirdParty/CC0-1.0.txt"
    ditto "${FREEPATS_NOTICE}" \
        "${destination}/ThirdParty/FreePats-Spanish-Classical-Guitar-README.txt"
    ditto "${EASTMAN_NOTICE}" \
        "${destination}/ThirdParty/Eastman-E1D-README.md"
    ditto "${SHINY_NOTICE}" \
        "${destination}/ThirdParty/Shinyguitar-README.txt"
    ditto "${BANK_MANIFEST}" \
        "${destination}/Assets/SampleBank/manifest.json"
}

copy_documentation "${NOTICE_ROOT}"

for bundle in \
    "${PACKAGE_ROOT}/Library/Audio/Plug-Ins/VST3/Acustra.vst3" \
    "${PACKAGE_ROOT}/Library/Audio/Plug-Ins/Components/Acustra.component" \
    "${PACKAGE_ROOT}/Applications/Acustra.app"; do
    bundle_notices="${bundle}/Contents/Resources/Documentation"
    copy_documentation "${bundle_notices}"
done

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
sign_bundle "${PACKAGE_ROOT}/Library/Audio/Plug-Ins/VST3/Acustra.vst3"
sign_bundle "${PACKAGE_ROOT}/Library/Audio/Plug-Ins/Components/Acustra.component"
sign_bundle "${PACKAGE_ROOT}/Applications/Acustra.app"

ZIP_PATH="${DIST_DIR}/Acustra-${VERSION}-macOS-${ARTIFACT_ARCH}.zip"
PKG_UNSIGNED="${DIST_DIR}/Acustra-${VERSION}-unsigned.pkg"
PKG_FINAL="${DIST_DIR}/Acustra-${VERSION}-macOS-${ARTIFACT_ARCH}.pkg"
rm -f "${DIST_DIR}/Acustra-"*-macOS-*.zip
rm -f "${DIST_DIR}/Acustra-"*-macOS-*.pkg
rm -f "${DIST_DIR}/Acustra-"*-unsigned.pkg

COPYFILE_DISABLE=1 pkgbuild \
    --root "${PACKAGE_ROOT}" \
    --identifier cz.protocodus.acustra.pkg \
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
ditto -c -k --norsrc --noextattr --noqtn --noacl \
    "${PACKAGE_ROOT}" "${ZIP_PATH}"

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
