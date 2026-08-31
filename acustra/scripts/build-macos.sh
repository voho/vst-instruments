#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-${PROJECT_DIR}/build-macos}"
CONFIG="${CONFIG:-Release}"
MACOSX_DEPLOYMENT_TARGET="${MACOSX_DEPLOYMENT_TARGET:-11.0}"
BUILD_UNIVERSAL="${BUILD_UNIVERSAL:-ON}"

if [[ "$(uname -s)" != "Darwin" ]]; then
    echo "error: this script requires macOS and Xcode" >&2
    exit 1
fi

for tool in cmake xcodebuild codesign; do
    command -v "${tool}" >/dev/null 2>&1 || {
        echo "error: required tool '${tool}' was not found" >&2
        exit 1
    }
done
xcodebuild -version >/dev/null 2>&1 || {
    echo "error: select a full Xcode installation with xcode-select" >&2
    exit 1
}

cmake_args=(
    -S "${PROJECT_DIR}"
    -B "${BUILD_DIR}"
    -G Xcode
    "-DCMAKE_OSX_DEPLOYMENT_TARGET=${MACOSX_DEPLOYMENT_TARGET}"
    "-DACUSTRA_BUILD_UNIVERSAL=${BUILD_UNIVERSAL}"
    -DACUSTRA_BUILD_PLUGIN=ON
    -DACUSTRA_BUILD_TOOLS=ON
    -DBUILD_TESTING=ON
)

if [[ "${BUILD_UNIVERSAL}" == "ON" ]]; then
    cmake_args+=("-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64")
else
    cmake_args+=("-DCMAKE_OSX_ARCHITECTURES=$(uname -m)")
fi
if [[ -n "${JUCE_PATH:-}" ]]; then
    cmake_args+=("-DACUSTRA_JUCE_PATH=${JUCE_PATH}")
fi

cmake "${cmake_args[@]}"
cmake --build "${BUILD_DIR}" --config "${CONFIG}" --parallel

snapshot="${ACUSTRA_EDITOR_SNAPSHOT:-${PROJECT_DIR}/Docs/screenshots/acustra-standalone.png}"
ACUSTRA_EDITOR_SNAPSHOT="${snapshot}" \
    ctest --test-dir "${BUILD_DIR}" -C "${CONFIG}" --output-on-failure

ARTIFACT_DIR="${BUILD_DIR}/Acustra_artefacts/${CONFIG}"
artifacts=(
    "${ARTIFACT_DIR}/VST3/Acustra.vst3"
    "${ARTIFACT_DIR}/AU/Acustra.component"
    "${ARTIFACT_DIR}/Standalone/Acustra.app"
)
for artifact in "${artifacts[@]}"; do
    if [[ ! -d "${artifact}" ]]; then
        echo "error: expected build artifact is missing: ${artifact}" >&2
        exit 1
    fi
    codesign --force --sign - "${artifact}"
    codesign --verify --deep --strict --verbose=2 "${artifact}"
done

echo
echo "Build complete. Artifacts:"
printf '  %s\n' "${artifacts[@]}"
