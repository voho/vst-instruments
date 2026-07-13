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

command -v cmake >/dev/null 2>&1 || {
    echo "error: cmake 3.22 or newer is required" >&2
    exit 1
}
command -v xcodebuild >/dev/null 2>&1 || {
    echo "error: full Xcode is required" >&2
    exit 1
}
xcodebuild -version >/dev/null 2>&1 || {
    echo "error: select a full Xcode installation with xcode-select" >&2
    echo "Example: sudo xcode-select -s /Applications/Xcode.app/Contents/Developer" >&2
    exit 1
}

cmake_args=(
    -S "${PROJECT_DIR}"
    -B "${BUILD_DIR}"
    -G Xcode
    "-DCMAKE_OSX_DEPLOYMENT_TARGET=${MACOSX_DEPLOYMENT_TARGET}"
    "-DMARS_BUILD_UNIVERSAL=${BUILD_UNIVERSAL}"
    -DMARS_BUILD_PLUGIN=ON
    -DBUILD_TESTING=ON
)

if [[ "${BUILD_UNIVERSAL}" == "ON" ]]; then
    cmake_args+=("-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64")
else
    cmake_args+=("-DCMAKE_OSX_ARCHITECTURES=$(uname -m)")
fi

if [[ -n "${JUCE_PATH:-}" ]]; then
    cmake_args+=("-DMARS_JUCE_PATH=${JUCE_PATH}")
fi

cmake "${cmake_args[@]}"
cmake --build "${BUILD_DIR}" --config "${CONFIG}" --parallel
ctest --test-dir "${BUILD_DIR}" -C "${CONFIG}" --output-on-failure

echo
echo "Build complete. Artifacts:"
echo "  ${BUILD_DIR}/Mars_artefacts/${CONFIG}/VST3/Mars.vst3"
echo "  ${BUILD_DIR}/Mars_artefacts/${CONFIG}/AU/Mars.component"
echo "  ${BUILD_DIR}/Mars_artefacts/${CONFIG}/Standalone/Mars.app"
