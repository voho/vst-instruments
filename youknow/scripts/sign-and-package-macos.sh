#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${PROJECT_DIR}/build-macos}"
CONFIG="${CONFIG:-Release}"
VERSION_OVERRIDE="${VERSION:-}"
PROJECT_VERSION=""
RELEASE_MODE="${RELEASE_MODE:-0}"
APP_SIGN_IDENTITY="${APP_SIGN_IDENTITY:--}"
INSTALLER_SIGN_IDENTITY="${INSTALLER_SIGN_IDENTITY:-}"
NOTARY_PROFILE="${NOTARY_PROFILE:-}"
ARTIFACT_DIR="${BUILD_DIR}/YouKnow_artefacts/${CONFIG}"
DIST_DIR="${BUILD_DIR}/dist"
PACKAGE_ROOT="${BUILD_DIR}/package-root"
CACHE_FILE="${BUILD_DIR}/CMakeCache.txt"
CMAKE_FILE="${PROJECT_DIR}/CMakeLists.txt"
RELEASE_NOTES_FILE="${PROJECT_DIR}/README.md"
USER_GUIDE_FILE="${PROJECT_DIR}/USER_GUIDE.md"
CUSTOMER_LICENSE_FILE="${PROJECT_DIR}/LICENSE"
PRODUCT_NAME="YouKnow"
VENDOR_NAME="Protocodus"
PRODUCT_WEBSITE="https://protocodus.cz"
SUPPORT_EMAIL="protocodus@proton.me"
STANDALONE_BUNDLE_IDENTIFIER="cz.protocodus.youknow"
AU_BUNDLE_IDENTIFIER="cz.protocodus.youknow.au"
VST3_BUNDLE_IDENTIFIER="cz.protocodus.youknow.vst3"
PACKAGE_IDENTIFIER="cz.protocodus.youknow.pkg"
AU_TYPE="aumu"
AU_SUBTYPE="Yk06"
AU_MANUFACTURER="Ykno"
VST3_PROCESSOR_CID="ABCDEF019182FAEB596B6E6F596B3036"
VST3_CONTROLLER_CID="ABCDEF011234ABCD596B6E6F596B3036"
PRODUCT_COPYRIGHT="Copyright (c) 2026 ${VENDOR_NAME}"
MINIMUM_MACOS="11.0"
PREFLIGHT_ONLY=0
case "${1:-}" in
    --preflight) PREFLIGHT_ONLY=1; shift ;;
    "") ;;
    *) echo "usage: $0 [--preflight]" >&2; exit 1 ;;
esac
if [[ "$#" -ne 0 ]]; then
    echo "usage: $0 [--preflight]" >&2
    exit 1
fi

clear_distribution_artifacts() {
    case "${DIST_DIR}" in
        "${BUILD_DIR}"/*) ;;
        *) echo "error: unsafe distribution path: ${DIST_DIR}" >&2; exit 1 ;;
    esac
    mkdir -p "${DIST_DIR}"
    rm -f "${DIST_DIR}"/YouKnow-*-macOS-*.zip
    rm -f "${DIST_DIR}"/YouKnow-*-macOS-*.pkg
    rm -f "${DIST_DIR}"/YouKnow-*-macOS-*.manifest.txt
    rm -f "${DIST_DIR}"/YouKnow-*-macOS-*-SHA256SUMS.txt
    rm -f "${DIST_DIR}"/YouKnow-*-unsigned.pkg
}

case "${RELEASE_MODE}" in
    0|1) ;;
    *)
        echo "error: RELEASE_MODE must be 0 or 1" >&2
        exit 1
        ;;
esac

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "error: this script requires macOS" >&2
    exit 1
fi

for tool in awk cmake cmp codesign ditto grep lipo mktemp pkgbuild plutil productbuild sed shasum xattr xcrun; do
    command -v "${tool}" >/dev/null 2>&1 || {
        echo "error: required tool '${tool}' was not found" >&2
        exit 1
    }
done

for document in "${CUSTOMER_LICENSE_FILE}" "${RELEASE_NOTES_FILE}" "${USER_GUIDE_FILE}" \
    "${PROJECT_DIR}/THIRD_PARTY_NOTICES.md" \
    "${PROJECT_DIR}/ThirdParty/JUCE-LICENSE.md" "${PROJECT_DIR}/PRIVACY.md"; do
    if [[ ! -s "${document}" ]]; then
        echo "error: missing or empty distribution document: ${document}" >&2
        exit 1
    fi
done

if [[ "${RELEASE_MODE}" == "1" ]]; then
    if [[ -z "${APP_SIGN_IDENTITY}" || "${APP_SIGN_IDENTITY}" == "-" ]]; then
        echo "error: release mode requires APP_SIGN_IDENTITY for a Developer ID Application identity" >&2
        exit 1
    fi
    if [[ -z "${INSTALLER_SIGN_IDENTITY}" ]]; then
        echo "error: release mode requires INSTALLER_SIGN_IDENTITY for a Developer ID Installer identity" >&2
        exit 1
    fi
    if [[ -z "${NOTARY_PROFILE}" ]]; then
        echo "error: release mode requires NOTARY_PROFILE" >&2
        exit 1
    fi

    if [[ "${CONFIG}" != "Release" ]]; then
        echo "error: release mode requires CONFIG=Release" >&2
        exit 1
    fi
    if [[ -n "${JUCE_PATH:-}" || -n "${YOUKNOW_JUCE_PATH:-}" \
          || -n "${FETCHCONTENT_SOURCE_DIR_JUCE:-}" ]]; then
        echo "error: release mode forbids local JUCE source overrides" >&2
        exit 1
    fi
    command -v git >/dev/null 2>&1 || {
        echo "error: release mode requires git" >&2
        exit 1
    }
    if [[ ! -f "${CMAKE_FILE}" ]]; then
        echo "error: missing ${CMAKE_FILE}" >&2
        exit 1
    fi
    if [[ ! -f "${RELEASE_NOTES_FILE}" ]]; then
        echo "error: missing ${RELEASE_NOTES_FILE}" >&2
        exit 1
    fi
    PROJECT_VERSION="$(awk '
        /^[[:space:]]*project\(YouKnow[[:space:]]+VERSION[[:space:]]+/ {
            for (field = 1; field <= NF; ++field) {
                if ($field == "VERSION") {
                    print $(field + 1)
                    exit
                }
            }
        }
    ' "${CMAKE_FILE}")"
    if [[ -z "${PROJECT_VERSION}" ]]; then
        echo "error: could not read the YouKnow version from CMakeLists.txt" >&2
        exit 1
    fi
    if [[ -n "${VERSION_OVERRIDE}" && "${VERSION_OVERRIDE}" != "${PROJECT_VERSION}" ]]; then
        echo "error: VERSION=${VERSION_OVERRIDE} does not match CMake version ${PROJECT_VERSION}" >&2
        exit 1
    fi
    # The release history lives in the instrument README, under a per-version
    # heading. A release may only be cut once that heading carries a real date
    # instead of "unreleased", so the shipped notes can never describe a version as
    # unreleased while it is being packaged.
    RELEASE_DATE="$(awk -v version="${PROJECT_VERSION}" '
        index($0, "### " version " ") == 1 && $0 !~ /unreleased/ {
            if (match($0, /[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]/))
                print substr($0, RSTART, RLENGTH)
            exit
        }
    ' "${RELEASE_NOTES_FILE}")"
    if [[ ! "${RELEASE_DATE}" =~ ^[0-9]{4}-[0-9]{2}-[0-9]{2}$ ]]; then
        echo "error: README.md needs a dated '### ${PROJECT_VERSION}' release-history heading" >&2
        exit 1
    fi
    if ! git -C "${PROJECT_DIR}" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
        echo "error: commercial releases must be built from a git worktree" >&2
        exit 1
    fi
    SOURCE_CHANGES="$(git -C "${PROJECT_DIR}" status --porcelain --untracked-files=normal -- .)"
    if [[ -n "${SOURCE_CHANGES}" ]]; then
        echo "error: commercial releases require a clean YouKnow source tree" >&2
        printf '%s\n' "${SOURCE_CHANGES}" >&2
        exit 1
    fi
    EXPECTED_TAG="youknow-v${PROJECT_VERSION}"
    HEAD_COMMIT="$(git -C "${PROJECT_DIR}" rev-parse HEAD)"
    TAG_COMMIT="$(git -C "${PROJECT_DIR}" rev-parse -q --verify \
        "refs/tags/${EXPECTED_TAG}^{commit}" 2>/dev/null || true)"
    if [[ -z "${TAG_COMMIT}" || "${TAG_COMMIT}" != "${HEAD_COMMIT}" ]]; then
        echo "error: tag ${EXPECTED_TAG} must point exactly at HEAD (${HEAD_COMMIT})" >&2
        exit 1
    fi

    for tool in ctest pkgutil productsign security spctl xcodebuild; do
        command -v "${tool}" >/dev/null 2>&1 || {
            echo "error: release tool '${tool}' was not found" >&2
            exit 1
        }
    done
    if ! xcodebuild -version >/dev/null 2>&1 \
        || ! xcrun --sdk macosx --show-sdk-version >/dev/null 2>&1; then
        echo "error: release mode requires a full Xcode installation and macOS SDK" >&2
        exit 1
    fi

    SIGNING_IDENTITIES="$(security find-identity -v 2>/dev/null || true)"
    if ! grep -F -- "${APP_SIGN_IDENTITY}" <<< "${SIGNING_IDENTITIES}" \
        | grep -F -- "Developer ID Application:" >/dev/null; then
        echo "error: APP_SIGN_IDENTITY is not an available Developer ID Application identity" >&2
        exit 1
    fi
    if ! grep -F -- "${INSTALLER_SIGN_IDENTITY}" <<< "${SIGNING_IDENTITIES}" \
        | grep -F -- "Developer ID Installer:" >/dev/null; then
        echo "error: INSTALLER_SIGN_IDENTITY is not an available Developer ID Installer identity" >&2
        exit 1
    fi
    if ! xcrun notarytool history --keychain-profile "${NOTARY_PROFILE}" \
        --output-format json >/dev/null; then
        echo "error: NOTARY_PROFILE is missing, invalid, or cannot authenticate" >&2
        exit 1
    fi
fi

if [[ "${PREFLIGHT_ONLY}" == "1" ]]; then
    echo "Packaging preflight passed; build artifacts have not been checked."
    exit 0
fi

if [[ ! -f "${CACHE_FILE}" ]]; then
    echo "error: missing CMake cache: ${CACHE_FILE}" >&2
    echo "Run scripts/build-macos.sh first." >&2
    exit 1
fi

JUCE_SOURCE_DIR="$(sed -n 's/^JUCE_SOURCE_DIR:[^=]*=//p' "${CACHE_FILE}")"
DEPLOYMENT_TARGET="$(sed -n 's/^CMAKE_OSX_DEPLOYMENT_TARGET:[^=]*=//p' \
    "${CACHE_FILE}")"
CXX_COMPILER="$(sed -n 's/^CMAKE_CXX_COMPILER:[^=]*=//p' "${CACHE_FILE}")"
LOCAL_JUCE_OVERRIDE="$(sed -n \
    -e 's/^YOUKNOW_JUCE_PATH:[^=]*=//p' \
    -e 's/^FETCHCONTENT_SOURCE_DIR_JUCE:[^=]*=//p' \
    "${CACHE_FILE}")"
if [[ -z "${JUCE_SOURCE_DIR}" || ! -d "${JUCE_SOURCE_DIR}" ]]; then
    echo "error: JUCE_SOURCE_DIR in ${CACHE_FILE} is missing or invalid" >&2
    exit 1
fi
if [[ -z "${CXX_COMPILER}" || ! -x "${CXX_COMPILER}" ]]; then
    CXX_COMPILER="$(xcrun --find clang++ 2>/dev/null || true)"
fi
if [[ -z "${CXX_COMPILER}" || ! -x "${CXX_COMPILER}" ]]; then
    echo "error: no executable C++ compiler in ${CACHE_FILE} or xcrun" >&2
    exit 1
fi
if [[ -z "${DEPLOYMENT_TARGET}" ]]; then
    echo "error: CMAKE_OSX_DEPLOYMENT_TARGET is missing from ${CACHE_FILE}" >&2
    exit 1
fi
if [[ "${RELEASE_MODE}" == "1" && "${DEPLOYMENT_TARGET}" != "${MINIMUM_MACOS}" ]]; then
    echo "error: release build must target macOS ${MINIMUM_MACOS}; cache has '${DEPLOYMENT_TARGET:-unset}'" >&2
    exit 1
fi

CMAKE_TOOL_VERSION="$(cmake --version | sed -n '1p')"
CXX_TOOL_VERSION="$("${CXX_COMPILER}" --version | sed -n '1p')"
XCODE_TOOL_VERSION="unavailable"
if XCODE_VERSION_OUTPUT="$(xcodebuild -version 2>/dev/null)"; then
    XCODE_TOOL_VERSION="$(printf '%s\n' "${XCODE_VERSION_OUTPUT}" | paste -sd ' ' -)"
fi
MACOS_SDK_VERSION="unavailable"
if command -v xcrun >/dev/null 2>&1; then
    MACOS_SDK_VERSION="$(xcrun --sdk macosx --show-sdk-version 2>/dev/null \
        || printf 'unavailable')"
fi
if [[ -z "${CMAKE_TOOL_VERSION}" || -z "${CXX_TOOL_VERSION}" ]]; then
    echo "error: could not record the CMake or C++ compiler version" >&2
    exit 1
fi
if [[ "${RELEASE_MODE}" == "1" \
      && ( "${XCODE_TOOL_VERSION}" == "unavailable" \
           || "${MACOS_SDK_VERSION}" == "unavailable" ) ]]; then
    echo "error: release mode requires a full Xcode installation and macOS SDK" >&2
    exit 1
fi
if [[ "${RELEASE_MODE}" == "1" && -n "${LOCAL_JUCE_OVERRIDE}" ]]; then
    echo "error: release cache uses local JUCE override: ${LOCAL_JUCE_OVERRIDE}" >&2
    exit 1
fi
if [[ "${RELEASE_MODE}" == "1" ]]; then
    CACHE_SOURCE_DIR="$(sed -n 's/^CMAKE_HOME_DIRECTORY:[^=]*=//p' "${CACHE_FILE}")"
    if [[ ! -d "${CACHE_SOURCE_DIR}" ]] \
        || [[ "$(cd "${CACHE_SOURCE_DIR}" && pwd -P)" != "$(cd "${PROJECT_DIR}" && pwd -P)" ]]; then
        echo "error: release cache was configured from a different source directory" >&2
        exit 1
    fi
    EXPECTED_JUCE_SOURCE_DIR="${BUILD_DIR}/_deps/juce-src"
    if [[ ! -d "${EXPECTED_JUCE_SOURCE_DIR}" ]]; then
        echo "error: release build is missing fetched JUCE at ${EXPECTED_JUCE_SOURCE_DIR}" >&2
        exit 1
    fi
    JUCE_SOURCE_REAL="$(cd "${JUCE_SOURCE_DIR}" && pwd -P)"
    EXPECTED_JUCE_SOURCE_REAL="$(cd "${EXPECTED_JUCE_SOURCE_DIR}" && pwd -P)"
    if [[ "${JUCE_SOURCE_REAL}" != "${EXPECTED_JUCE_SOURCE_REAL}" ]]; then
        echo "error: release build did not use the pinned fetched JUCE source" >&2
        exit 1
    fi
fi

VST3="${ARTIFACT_DIR}/VST3/YouKnow.vst3"
AU="${ARTIFACT_DIR}/AU/YouKnow.component"
APP="${ARTIFACT_DIR}/Standalone/YouKnow.app"

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

bundle_value() {
    "${PLIST_BUDDY}" -c "Print :$2" "$1/Contents/Info.plist" 2>/dev/null \
        || true
}

require_value() {
    local label="$1"
    local expected="$2"
    local actual="$3"

    if [[ "${actual}" != "${expected}" ]]; then
        echo "error: ${label} is '${actual:-missing}', expected '${expected}'" >&2
        exit 1
    fi
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
if [[ "${RELEASE_MODE}" == "1" && "${VERSION}" != "${PROJECT_VERSION}" ]]; then
    echo "error: bundle version ${VERSION} does not match CMake version ${PROJECT_VERSION}" >&2
    exit 1
fi

require_value "VST3 bundle identifier" "${VST3_BUNDLE_IDENTIFIER}" \
    "$(bundle_value "${VST3}" CFBundleIdentifier)"
require_value "Audio Unit bundle identifier" "${AU_BUNDLE_IDENTIFIER}" \
    "$(bundle_value "${AU}" CFBundleIdentifier)"
require_value "Standalone bundle identifier" "${STANDALONE_BUNDLE_IDENTIFIER}" \
    "$(bundle_value "${APP}" CFBundleIdentifier)"

for bundle in "${VST3}" "${AU}" "${APP}"; do
    require_value "$(basename "${bundle}") display name" \
        "${PRODUCT_NAME}" "$(bundle_value "${bundle}" CFBundleDisplayName)"
    require_value "$(basename "${bundle}") copyright" \
        "${PRODUCT_COPYRIGHT}" "$(bundle_value "${bundle}" NSHumanReadableCopyright)"
done

require_value "Audio Unit type" "${AU_TYPE}" \
    "$(bundle_value "${AU}" AudioComponents:0:type)"
require_value "Audio Unit subtype" "${AU_SUBTYPE}" \
    "$(bundle_value "${AU}" AudioComponents:0:subtype)"
require_value "Audio Unit manufacturer" "${AU_MANUFACTURER}" \
    "$(bundle_value "${AU}" AudioComponents:0:manufacturer)"

VST3_MODULE_INFO="${VST3}/Contents/Resources/moduleinfo.json"
if [[ ! -f "${VST3_MODULE_INFO}" ]]; then
    echo "error: missing VST3 module metadata: ${VST3_MODULE_INFO}" >&2
    exit 1
fi
module_value() {
    plutil -extract "$1" raw -o - "${VST3_MODULE_INFO}" 2>/dev/null || true
}
require_value "VST3 product name" "${PRODUCT_NAME}" "$(module_value Name)"
require_value "VST3 version" "${VERSION}" "$(module_value Version)"
require_value "VST3 vendor" "${VENDOR_NAME}" \
    "$(module_value 'Factory Info.Vendor')"
require_value "VST3 website" "${PRODUCT_WEBSITE}" \
    "$(module_value 'Factory Info.URL')"
require_value "VST3 support email" "${SUPPORT_EMAIL}" \
    "$(module_value 'Factory Info.E-Mail')"
require_value "VST3 processor CID" "${VST3_PROCESSOR_CID}" \
    "$(module_value 'Classes.0.CID')"
require_value "VST3 controller CID" "${VST3_CONTROLLER_CID}" \
    "$(module_value 'Classes.1.CID')"

if [[ ! -f "${APP}/Contents/Resources/AppIcon.icns" ]]; then
    echo "error: standalone app icon is missing" >&2
    exit 1
fi

VST3_ARCHS="$(lipo -archs "${VST3}/Contents/MacOS/YouKnow")"
AU_ARCHS="$(lipo -archs "${AU}/Contents/MacOS/YouKnow")"
APP_ARCHS="$(lipo -archs "${APP}/Contents/MacOS/YouKnow")"
if ! [[ "${VST3_ARCHS}" == "${AU_ARCHS}" \
        && "${VST3_ARCHS}" == "${APP_ARCHS}" ]]; then
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
if [[ "${RELEASE_MODE}" == "1" && "${ARTIFACT_ARCH}" != "universal" ]]; then
    echo "error: release artifacts must contain both arm64 and x86_64" >&2
    exit 1
fi

VTOOL="$(xcrun --find vtool 2>/dev/null || true)"
if [[ -z "${VTOOL}" || ! -x "${VTOOL}" ]]; then
    echo "error: xcrun could not find an executable vtool" >&2
    exit 1
fi

require_macos_target() {
    local executable="$1"
    local architecture="$2"
    local build_info=""
    local platform=""
    local minimum=""

    if ! build_info="$("${VTOOL}" -arch "${architecture}" -show-build \
            "${executable}" 2>&1)"; then
        echo "error: vtool could not inspect ${executable} (${architecture})" >&2
        printf '%s\n' "${build_info}" >&2
        exit 1
    fi

    platform="$(printf '%s\n' "${build_info}" \
        | awk '$1 == "platform" { print $2; exit }')"
    minimum="$(printf '%s\n' "${build_info}" \
        | awk '$1 == "minos" { print $2; exit }')"
    require_value "$(basename "${executable}") ${architecture} platform" \
        "MACOS" "${platform}"
    require_value "$(basename "${executable}") ${architecture} minimum macOS" \
        "${DEPLOYMENT_TARGET}" "${minimum}"
}

for executable in \
    "${VST3}/Contents/MacOS/YouKnow" \
    "${AU}/Contents/MacOS/YouKnow" \
    "${APP}/Contents/MacOS/YouKnow"; do
    for architecture in ${APP_ARCHS}; do
        require_macos_target "${executable}" "${architecture}"
    done
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

ditto "${VST3}" "${PACKAGE_ROOT}/Library/Audio/Plug-Ins/VST3/YouKnow.vst3"
ditto "${AU}" "${PACKAGE_ROOT}/Library/Audio/Plug-Ins/Components/YouKnow.component"
ditto "${APP}" "${PACKAGE_ROOT}/Applications/YouKnow.app"

JUCE_LICENSE_INDEX="${JUCE_SOURCE_DIR}/LICENSE.md"
REPOSITORY_JUCE_LICENSE="${PROJECT_DIR}/ThirdParty/JUCE-LICENSE.md"
JUCE_DEPENDENCY_LICENSES=(
    "modules/juce_audio_plugin_client/AU/AudioUnitSDK/LICENSE.txt"
    "modules/juce_audio_processors_headless/format_types/VST3_SDK/LICENSE.txt"
    "modules/juce_audio_formats/codecs/flac/Flac Licence.txt"
    "modules/juce_audio_formats/codecs/oggvorbis/Ogg Vorbis Licence.txt"
    "modules/juce_graphics/image_formats/jpglib/README"
    "modules/juce_graphics/image_formats/pnglib/LICENSE"
    "modules/juce_core/zip/zlib/README"
    "modules/juce_core/zip/zlib/LICENSE"
    "modules/juce_graphics/fonts/harfbuzz/COPYING"
    "modules/juce_graphics/unicode/sheenbidi/LICENSE"
)

REQUIRED_DOCUMENTS=(
    "${PROJECT_DIR}/LICENSE"
    "${PROJECT_DIR}/THIRD_PARTY_NOTICES.md"
    "${PROJECT_DIR}/ThirdParty/JUCE-LICENSE.md"
    "${USER_GUIDE_FILE}"
    "${PROJECT_DIR}/PRIVACY.md"
    "${JUCE_LICENSE_INDEX}"
)
for document in "${REQUIRED_DOCUMENTS[@]}"; do
    if [[ ! -f "${document}" ]]; then
        echo "error: missing distribution document: ${document}" >&2
        exit 1
    fi
done
for relative_path in "${JUCE_DEPENDENCY_LICENSES[@]}"; do
    if [[ ! -f "${JUCE_SOURCE_DIR}/${relative_path}" ]]; then
        echo "error: missing JUCE dependency license: ${relative_path}" >&2
        exit 1
    fi
done
if ! cmp -s "${REPOSITORY_JUCE_LICENSE}" "${JUCE_LICENSE_INDEX}"; then
    echo "error: ThirdParty/JUCE-LICENSE.md does not match JUCE_SOURCE_DIR/LICENSE.md" >&2
    exit 1
fi

stage_documentation() {
    local destination="$1"
    local relative_path

    mkdir -p "${destination}/ThirdParty/JUCE"
    ditto "${PROJECT_DIR}/LICENSE" "${destination}/LICENSE"
    ditto "${PROJECT_DIR}/THIRD_PARTY_NOTICES.md" \
        "${destination}/THIRD_PARTY_NOTICES.md"
    # Ship the self-contained customer guide; developer notes stay in the repo.
    ditto "${USER_GUIDE_FILE}" "${destination}/README.md"
    ditto "${PROJECT_DIR}/PRIVACY.md" "${destination}/PRIVACY.md"
    ditto "${JUCE_LICENSE_INDEX}" "${destination}/ThirdParty/JUCE/LICENSE.md"

    for relative_path in "${JUCE_DEPENDENCY_LICENSES[@]}"; do
        mkdir -p "$(dirname "${destination}/ThirdParty/JUCE/${relative_path}")"
        ditto "${JUCE_SOURCE_DIR}/${relative_path}" \
            "${destination}/ThirdParty/JUCE/${relative_path}"
    done
}

NOTICE_ROOT="${PACKAGE_ROOT}/Library/Application Support/Protocodus/YouKnow/Documentation"
stage_documentation "${NOTICE_ROOT}"

# Keep the documentation inside every independently copyable bundle. It is
# staged before signing so the signatures cover these exact files.
for bundle in \
    "${PACKAGE_ROOT}/Library/Audio/Plug-Ins/VST3/YouKnow.vst3" \
    "${PACKAGE_ROOT}/Library/Audio/Plug-Ins/Components/YouKnow.component" \
    "${PACKAGE_ROOT}/Applications/YouKnow.app"; do
    stage_documentation "${bundle}/Contents/Resources/Documentation"
done

# Extended attributes from a developer checkout otherwise become AppleDouble
# `._*` files in pkgbuild's payload. The staged copies are signed immediately
# after this cleanup, so no signed source artifact is modified.
xattr -cr "${PACKAGE_ROOT}"

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

sign_bundle "${PACKAGE_ROOT}/Library/Audio/Plug-Ins/VST3/YouKnow.vst3"
sign_bundle "${PACKAGE_ROOT}/Library/Audio/Plug-Ins/Components/YouKnow.component"
sign_bundle "${PACKAGE_ROOT}/Applications/YouKnow.app"

ARTIFACT_BASE="YouKnow-${VERSION}-macOS-${ARTIFACT_ARCH}"
PKG_FINAL="${DIST_DIR}/${ARTIFACT_BASE}.pkg"
ZIP_PATH="${DIST_DIR}/${ARTIFACT_BASE}.zip"
MANIFEST_PATH="${DIST_DIR}/${ARTIFACT_BASE}.manifest.txt"
CHECKSUM_PATH="${DIST_DIR}/${ARTIFACT_BASE}-SHA256SUMS.txt"

# Avoid stale version or architecture names in wildcard-driven publication.
clear_distribution_artifacts

PACKAGE_WORK_DIR="$(mktemp -d "${BUILD_DIR}/package-work.XXXXXX")"
cleanup() {
    rm -rf "${PACKAGE_WORK_DIR}"
}
trap cleanup EXIT

PKG_UNSIGNED="${PACKAGE_WORK_DIR}/YouKnow-unsigned.pkg"
PKG_COMPONENT="${PACKAGE_WORK_DIR}/YouKnow-component.pkg"
PKG_WORK="${PACKAGE_WORK_DIR}/YouKnow.pkg"
COMPONENT_PLIST="${PACKAGE_WORK_DIR}/components.plist"
pkgbuild --analyze --root "${PACKAGE_ROOT}" "${COMPONENT_PLIST}"
component_index=0
while "${PLIST_BUDDY}" -c "Print :${component_index}:RootRelativeBundlePath" \
    "${COMPONENT_PLIST}" >/dev/null 2>&1; do
    "${PLIST_BUDDY}" -c "Delete :${component_index}:BundleIsRelocatable" \
        "${COMPONENT_PLIST}" >/dev/null 2>&1 || true
    "${PLIST_BUDDY}" -c "Add :${component_index}:BundleIsRelocatable bool false" \
        "${COMPONENT_PLIST}"
    component_index=$((component_index + 1))
done
if [[ "${component_index}" -ne 3 ]]; then
    echo "error: expected three installable bundles; pkgbuild found ${component_index}" >&2
    exit 1
fi

COPYFILE_DISABLE=1 pkgbuild \
    --root "${PACKAGE_ROOT}" \
    --component-plist "${COMPONENT_PLIST}" \
    --identifier "${PACKAGE_IDENTIFIER}" \
    --version "${VERSION}" \
    --install-location / \
    "${PKG_COMPONENT}"

# A product archive lets Installer enforce the same OS/CPU requirements as
# the binaries, including native Installer execution on Apple Silicon.
REQUIREMENTS_PLIST="${PACKAGE_WORK_DIR}/requirements.plist"
"${PLIST_BUDDY}" -c "Clear dict" -c "Add :os array" \
    -c "Add :os:0 string ${DEPLOYMENT_TARGET}" -c "Add :arch array" \
    -c "Add :home bool false" "${REQUIREMENTS_PLIST}" >/dev/null
for architecture in ${APP_ARCHS}; do
    "${PLIST_BUDDY}" -c "Add :arch: string ${architecture}" "${REQUIREMENTS_PLIST}"
done
productbuild --product "${REQUIREMENTS_PLIST}" --package "${PKG_COMPONENT}" \
    "${PKG_UNSIGNED}"

if [[ -n "${INSTALLER_SIGN_IDENTITY}" ]]; then
    command -v productsign >/dev/null 2>&1 || {
        echo "error: productsign was not found" >&2
        exit 1
    }
    productsign --sign "${INSTALLER_SIGN_IDENTITY}" \
        "${PKG_UNSIGNED}" "${PKG_WORK}"
else
    mv "${PKG_UNSIGNED}" "${PKG_WORK}"
fi

NOTARY_STATUS="not requested"
NOTARY_SUBMISSION_ID="none"
if [[ -n "${NOTARY_PROFILE}" ]]; then
    if [[ "${APP_SIGN_IDENTITY}" == "-" || -z "${INSTALLER_SIGN_IDENTITY}" ]]; then
        echo "error: notarization requires Developer ID Application and Installer identities" >&2
        exit 1
    fi
    NOTARY_RESULT="$(xcrun notarytool submit "${PKG_WORK}" \
        --keychain-profile "${NOTARY_PROFILE}" --wait --output-format json)"
    NOTARY_STATUS="$(printf '%s' "${NOTARY_RESULT}" \
        | plutil -extract status raw -o - -)"
    NOTARY_SUBMISSION_ID="$(printf '%s' "${NOTARY_RESULT}" \
        | plutil -extract id raw -o - -)"
    if [[ "${NOTARY_STATUS}" != "Accepted" ]]; then
        echo "error: Apple notarization status was '${NOTARY_STATUS}'" >&2
        printf '%s\n' "${NOTARY_RESULT}" >&2
        exit 1
    fi
    xcrun stapler staple "${PKG_WORK}"
    xcrun stapler validate "${PKG_WORK}"
fi

if [[ "${RELEASE_MODE}" == "1" ]]; then
    pkgutil --check-signature "${PKG_WORK}"
    spctl --assess --type install --verbose=4 "${PKG_WORK}"
fi

# Publish only after all requested signing, notarization and validation passes.
mv "${PKG_WORK}" "${PKG_FINAL}"

PUBLISHED_ARTIFACTS=("${PKG_FINAL}")
if [[ "${RELEASE_MODE}" == "0" ]]; then
    ZIP_WORK="${PACKAGE_WORK_DIR}/YouKnow.zip"
    ditto -c -k --norsrc --noextattr --noqtn --noacl \
        "${PACKAGE_ROOT}" "${ZIP_WORK}"
    mv "${ZIP_WORK}" "${ZIP_PATH}"
    PUBLISHED_ARTIFACTS+=("${ZIP_PATH}")
fi

PACKAGE_SHA256="$(shasum -a 256 "${PKG_FINAL}" | awk '{print $1}')"
JUCE_LICENSE_SHA256="$(shasum -a 256 "${JUCE_LICENSE_INDEX}" | awk '{print $1}')"
CUSTOMER_LICENSE_SHA256="not supplied"
if [[ -f "${CUSTOMER_LICENSE_FILE}" ]]; then
    CUSTOMER_LICENSE_SHA256="$(shasum -a 256 "${CUSTOMER_LICENSE_FILE}" \
        | awk '{print $1}')"
fi
SOURCE_COMMIT="$(git -C "${PROJECT_DIR}" rev-parse HEAD 2>/dev/null || printf 'unknown')"
SOURCE_STATE="unavailable"
if git -C "${PROJECT_DIR}" rev-parse --is-inside-work-tree >/dev/null 2>&1; then
    if [[ -n "$(git -C "${PROJECT_DIR}" status --porcelain --untracked-files=normal -- .)" ]]; then
        SOURCE_STATE="dirty"
    else
        SOURCE_STATE="clean"
    fi
fi

{
    printf 'Product: %s\n' "${PRODUCT_NAME}"
    printf 'Vendor: %s\n' "${VENDOR_NAME}"
    printf 'Website: %s\n' "${PRODUCT_WEBSITE}"
    printf 'Support: %s\n' "${SUPPORT_EMAIL}"
    printf 'Version: %s\n' "${VERSION}"
    printf 'Formats: VST3, Audio Unit, Standalone\n'
    printf 'Architectures: %s\n' "${APP_ARCHS}"
    printf 'Minimum macOS: %s\n' "${DEPLOYMENT_TARGET}"
    printf 'CMake: %s\n' "${CMAKE_TOOL_VERSION}"
    printf 'Xcode: %s\n' "${XCODE_TOOL_VERSION}"
    printf 'macOS SDK: %s\n' "${MACOS_SDK_VERSION}"
    printf 'C++ compiler: %s\n' "${CXX_TOOL_VERSION}"
    printf 'Standalone bundle identifier: %s\n' \
        "${STANDALONE_BUNDLE_IDENTIFIER}"
    printf 'Audio Unit bundle identifier: %s\n' "${AU_BUNDLE_IDENTIFIER}"
    printf 'VST3 bundle identifier: %s\n' "${VST3_BUNDLE_IDENTIFIER}"
    printf 'Package identifier: %s\n' "${PACKAGE_IDENTIFIER}"
    printf 'Audio Unit identity: %s/%s/%s\n' \
        "${AU_TYPE}" "${AU_SUBTYPE}" "${AU_MANUFACTURER}"
    printf 'VST3 processor CID: %s\n' "${VST3_PROCESSOR_CID}"
    printf 'VST3 controller CID: %s\n' "${VST3_CONTROLLER_CID}"
    printf 'Package: %s\n' "$(basename "${PKG_FINAL}")"
    printf 'Package SHA-256: %s\n' "${PACKAGE_SHA256}"
    printf 'Release mode: %s\n' "${RELEASE_MODE}"
    printf 'Notarization status: %s\n' "${NOTARY_STATUS}"
    printf 'Notarization submission: %s\n' "${NOTARY_SUBMISSION_ID}"
    printf 'Source commit: %s\n' "${SOURCE_COMMIT}"
    printf 'Source tree: %s\n' "${SOURCE_STATE}"
    printf 'Customer licence SHA-256: %s\n' "${CUSTOMER_LICENSE_SHA256}"
    printf 'JUCE licensing basis: JUCE 8 Starter (publisher-declared)\n'
    printf 'JUCE LICENSE.md SHA-256: %s\n' "${JUCE_LICENSE_SHA256}"
    printf 'JUCE dependency license files:\n'
    for relative_path in "${JUCE_DEPENDENCY_LICENSES[@]}"; do
        printf '  %s\n' "${relative_path}"
    done
} > "${MANIFEST_PATH}"

PUBLISHED_ARTIFACTS+=("${MANIFEST_PATH}")
(
    cd "${DIST_DIR}"
    for artifact in "${PUBLISHED_ARTIFACTS[@]}"; do
        shasum -a 256 "$(basename "${artifact}")"
    done
) > "${CHECKSUM_PATH}"
PUBLISHED_ARTIFACTS+=("${CHECKSUM_PATH}")

echo
echo "Packaging complete:"
for artifact in "${PUBLISHED_ARTIFACTS[@]}"; do
    echo "  ${artifact}"
done
