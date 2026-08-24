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
ARTIFACT_DIR="${BUILD_DIR}/Septum_artefacts/${CONFIG}"
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

VST3="${ARTIFACT_DIR}/VST3/Septum.vst3"
AU="${ARTIFACT_DIR}/AU/Septum.component"
APP="${ARTIFACT_DIR}/Standalone/Septum.app"

for artifact in "${VST3}" "${AU}" "${APP}"; do
    if [[ ! -d "${artifact}" ]]; then
        echo "error: missing build artifact: ${artifact}" >&2
        echo "Run scripts/build-macos.sh first." >&2
        exit 1
    fi
done

PLIST_BUDDY="/usr/libexec/PlistBuddy"
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

VST3_ARCHS="$(lipo -archs "${VST3}/Contents/MacOS/Septum")"
AU_ARCHS="$(lipo -archs "${AU}/Contents/MacOS/Septum")"
APP_ARCHS="$(lipo -archs "${APP}/Contents/MacOS/Septum")"
if [[ "${VST3_ARCHS}" != "${AU_ARCHS}" || "${VST3_ARCHS}" != "${APP_ARCHS}" ]]; then
    echo "error: build artifact architectures disagree" >&2
    echo "  VST3: ${VST3_ARCHS}" >&2
    echo "  AU: ${AU_ARCHS}" >&2
    echo "  App: ${APP_ARCHS}" >&2
    exit 1
fi

if [[ "${APP_ARCHS}" == *arm64* && "${APP_ARCHS}" == *x86_64* ]]; then
    ARTIFACT_ARCH="universal"
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

ditto "${VST3}" "${PACKAGE_ROOT}/Library/Audio/Plug-Ins/VST3/Septum.vst3"
ditto "${AU}" "${PACKAGE_ROOT}/Library/Audio/Plug-Ins/Components/Septum.component"
ditto "${APP}" "${PACKAGE_ROOT}/Applications/Septum.app"

NOTICE_ROOT="${PACKAGE_ROOT}/Library/Application Support/Septum/Documentation"
JUCE_NOTICE="${PROJECT_DIR}/ThirdParty/JUCE-LICENSE.md"
for notice in "${PROJECT_DIR}/LICENSE" "${PROJECT_DIR}/THIRD_PARTY_NOTICES.md" \
              "${JUCE_NOTICE}"; do
    if [[ ! -f "${notice}" ]]; then
        echo "error: missing distribution notice: ${notice}" >&2
        exit 1
    fi
done

mkdir -p "${NOTICE_ROOT}"
ditto "${PROJECT_DIR}/LICENSE" "${NOTICE_ROOT}/Septum-LICENSE.txt"
ditto "${PROJECT_DIR}/THIRD_PARTY_NOTICES.md" \
    "${NOTICE_ROOT}/THIRD_PARTY_NOTICES.md"
ditto "${JUCE_NOTICE}" "${NOTICE_ROOT}/JUCE-LICENSE.md"

# Keep the same notices inside every independently copyable bundle. They are
# installed before signing so the bundle signatures cover the documentation.
for bundle in \
    "${PACKAGE_ROOT}/Library/Audio/Plug-Ins/VST3/Septum.vst3" \
    "${PACKAGE_ROOT}/Library/Audio/Plug-Ins/Components/Septum.component" \
    "${PACKAGE_ROOT}/Applications/Septum.app"; do
    bundle_notice_root="${bundle}/Contents/Resources/Documentation"
    mkdir -p "${bundle_notice_root}"
    ditto "${PROJECT_DIR}/LICENSE" "${bundle_notice_root}/Septum-LICENSE.txt"
    ditto "${PROJECT_DIR}/THIRD_PARTY_NOTICES.md" \
        "${bundle_notice_root}/THIRD_PARTY_NOTICES.md"
    ditto "${JUCE_NOTICE}" "${bundle_notice_root}/JUCE-LICENSE.md"
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

sign_bundle "${PACKAGE_ROOT}/Library/Audio/Plug-Ins/VST3/Septum.vst3"
sign_bundle "${PACKAGE_ROOT}/Library/Audio/Plug-Ins/Components/Septum.component"
sign_bundle "${PACKAGE_ROOT}/Applications/Septum.app"

ZIP_PATH="${DIST_DIR}/Septum-${VERSION}-macOS-${ARTIFACT_ARCH}.zip"
PKG_UNSIGNED="${DIST_DIR}/Septum-${VERSION}-unsigned.pkg"
PKG_FINAL="${DIST_DIR}/Septum-${VERSION}-macOS-${ARTIFACT_ARCH}.pkg"

# Leave exactly one Septum release set in dist, avoiding stale version or
# architecture names in wildcard-driven CI publication.
rm -f "${DIST_DIR}/Septum-"*-macOS-*.zip
rm -f "${DIST_DIR}/Septum-"*-macOS-*.pkg
rm -f "${DIST_DIR}/Septum-"*-unsigned.pkg

COPYFILE_DISABLE=1 pkgbuild \
    --root "${PACKAGE_ROOT}" \
    --identifier cz.protocodus.septum.pkg \
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

# Create the ZIP after pkgbuild from the same signed tree, explicitly omitting
# local extended attributes, quarantine state, ACLs, and resource metadata.
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
