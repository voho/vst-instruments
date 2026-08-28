#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
ARTIFACT_DIR="${1:-${PROJECT_DIR}/build-macos/Electry_artefacts/Release}"
AU_BUNDLE="${ARTIFACT_DIR}/AU/Electry.component"
VST3_BUNDLE="${ARTIFACT_DIR}/VST3/Electry.vst3"

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "error: this validator requires macOS" >&2
    exit 1
fi

for bundle in "${AU_BUNDLE}" "${VST3_BUNDLE}"; do
    if [[ ! -d "${bundle}" ]]; then
        echo "error: expected bundle is missing: ${bundle}" >&2
        exit 1
    fi
done

plist_value() {
    /usr/libexec/PlistBuddy -c "Print :$2" "$1"
}

expect_value() {
    local actual="$1"
    local expected="$2"
    local label="$3"
    if [[ "${actual}" != "${expected}" ]]; then
        echo "error: ${label}: expected '${expected}', got '${actual}'" >&2
        exit 1
    fi
}

AU_PLIST="${AU_BUNDLE}/Contents/Info.plist"
VST3_PLIST="${VST3_BUNDLE}/Contents/Info.plist"
expect_value "$(plist_value "${AU_PLIST}" CFBundleIdentifier)" "audio.electry.synth" "AU bundle ID"
expect_value "$(plist_value "${AU_PLIST}" AudioComponents:0:type)" "aumu" "AU type"
expect_value "$(plist_value "${AU_PLIST}" AudioComponents:0:subtype)" "Elc1" "AU subtype"
expect_value "$(plist_value "${AU_PLIST}" AudioComponents:0:manufacturer)" "Eltr" "AU manufacturer"
expect_value "$(plist_value "${AU_PLIST}" AudioComponents:0:factoryFunction)" "ElectryAUFactory" "AU factory"
expect_value "$(plist_value "${AU_PLIST}" AudioComponents:0:version)" "66048" "AU component version"
expect_value "$(plist_value "${AU_PLIST}" AudioComponents:0:sandboxSafe)" "true" "AU sandbox-safe flag"
expect_value "$(plist_value "${AU_PLIST}" CFBundleShortVersionString)" "1.2.0" "AU version"
expect_value "$(plist_value "${VST3_PLIST}" CFBundleIdentifier)" "audio.electry.synth" "VST3 bundle ID"
expect_value "$(plist_value "${VST3_PLIST}" CFBundleShortVersionString)" "1.2.0" "VST3 version"

AU_BINARY="${AU_BUNDLE}/Contents/MacOS/$(plist_value "${AU_PLIST}" CFBundleExecutable)"
VST3_BINARY="${VST3_BUNDLE}/Contents/MacOS/$(plist_value "${VST3_PLIST}" CFBundleExecutable)"
for binary in "${AU_BINARY}" "${VST3_BINARY}"; do
    if [[ ! -x "${binary}" ]]; then
        echo "error: bundle executable is missing: ${binary}" >&2
        exit 1
    fi
done

/usr/bin/nm -gjU "${AU_BINARY}" | /usr/bin/grep -qx _ElectryAUFactory
for symbol in _GetPluginFactory _bundleEntry _bundleExit; do
    /usr/bin/nm -gjU "${VST3_BINARY}" | /usr/bin/grep -qx "${symbol}"
done

AU_ARCHS="$(/usr/bin/lipo -archs "${AU_BINARY}")"
VST3_ARCHS="$(/usr/bin/lipo -archs "${VST3_BINARY}")"
expect_value "${AU_ARCHS}" "${VST3_ARCHS}" "AU/VST3 architecture set"

for bundle in "${AU_BUNDLE}" "${VST3_BUNDLE}"; do
    /usr/bin/codesign --verify --deep --strict --verbose=2 "${bundle}"
done

AU_BINARY_HASH="$(/usr/bin/shasum -a 256 "${AU_BINARY}" | /usr/bin/awk '{print $1}')"
VST3_BINARY_HASH="$(/usr/bin/shasum -a 256 "${VST3_BINARY}" | /usr/bin/awk '{print $1}')"
AU_PLIST_HASH="$(/usr/bin/shasum -a 256 "${AU_PLIST}" | /usr/bin/awk '{print $1}')"
VST3_PLIST_HASH="$(/usr/bin/shasum -a 256 "${VST3_PLIST}" | /usr/bin/awk '{print $1}')"

AU_CD_HASH="$(/usr/bin/codesign -d --verbose=4 "${AU_BUNDLE}" 2>&1 | /usr/bin/awk -F= '/^CDHash=/{print $2}')"
VST3_CD_HASH="$(/usr/bin/codesign -d --verbose=4 "${VST3_BUNDLE}" 2>&1 | /usr/bin/awk -F= '/^CDHash=/{print $2}')"

VALIDATOR_BINARY="$(/usr/bin/mktemp "${TMPDIR:-/tmp}/electry-au-validator.XXXXXX")"
cleanup() {
    /bin/rm -f -- "${VALIDATOR_BINARY}"
}
trap cleanup EXIT

/usr/bin/xcrun --sdk macosx clang++ \
    -std=c++20 -Wall -Wextra -Wpedantic -Werror \
    "${PROJECT_DIR}/Tests/ValidateAUArtifact.cpp" \
    -framework AudioToolbox -framework CoreFoundation \
    -o "${VALIDATOR_BINARY}"
"${VALIDATOR_BINARY}" "${AU_BINARY}"

expect_value "$(/usr/bin/shasum -a 256 "${AU_BINARY}" | /usr/bin/awk '{print $1}')" \
    "${AU_BINARY_HASH}" "AU binary hash after validation"

echo "PASS: exact macOS bundle identity and integrity"
echo "  architectures: ${AU_ARCHS}"
echo "  AU executable SHA-256: ${AU_BINARY_HASH}"
echo "  AU Info.plist SHA-256: ${AU_PLIST_HASH}"
echo "  AU code-directory hash: ${AU_CD_HASH}"
echo "  VST3 executable SHA-256: ${VST3_BINARY_HASH}"
echo "  VST3 Info.plist SHA-256: ${VST3_PLIST_HASH}"
echo "  VST3 code-directory hash: ${VST3_CD_HASH}"
echo "  VST3 host load: use ElectryVST3ArtifactSmokeTests (or an external path-based validator)"
echo "  auval: not run; auval selects registered components by type/subtype/manufacturer, not bundle path"
