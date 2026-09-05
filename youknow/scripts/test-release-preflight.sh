#!/usr/bin/env bash
set -euo pipefail

# No build, signing, account access or repository mutation: exercise the
# release entry points with disposable source documents and mocked tools.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
TEST_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/youknow-preflight.XXXXXX")"
trap 'rm -rf "${TEST_ROOT}"' EXIT
mkdir -p "${TEST_ROOT}/project/scripts" "${TEST_ROOT}/project/ThirdParty" \
    "${TEST_ROOT}/bin" "${TEST_ROOT}/project/build-macos/dist"
cp "${SCRIPT_DIR}/sign-and-package-macos.sh" "${SCRIPT_DIR}/release-macos.sh" \
    "${TEST_ROOT}/project/scripts/"
for document in LICENSE USER_GUIDE.md THIRD_PARTY_NOTICES.md ThirdParty/JUCE-LICENSE.md PRIVACY.md; do
    printf 'Fixture document\n' > "${TEST_ROOT}/project/${document}"
done
printf 'project(YouKnow VERSION 1.1.0 LANGUAGES CXX)\n' > "${TEST_ROOT}/project/CMakeLists.txt"
printf '### 1.1.0 — 2026-09-05\n' > "${TEST_ROOT}/project/README.md"
printf 'preserve\n' > "${TEST_ROOT}/project/build-macos/dist/YouKnow-old-macOS-universal.pkg"
cat > "${TEST_ROOT}/bin/mock-tool" <<'MOCK'
#!/usr/bin/env bash
set -eu
case "${0##*/}" in
    uname) echo Darwin ;;
    git)
        case "$*" in
            *" status "*)
                if [[ "$*" != *" -- ." || "${TEST_SOURCE_DIRTY:-0}" == 1 ]]; then
                    echo ' M unrelated/source.cpp'
                fi ;;
            *"--is-inside-work-tree"*) echo true ;;
            *"refs/tags/"*) echo "${TEST_TAG_COMMIT:-fixture-head}" ;;
            *"rev-parse HEAD"*) echo fixture-head ;;
            *) exit 90 ;;
        esac ;;
    security)
        echo 'Developer ID Application: Fixture'
        echo 'Developer ID Installer: Fixture' ;;
    xcodebuild) [[ "${TEST_NO_XCODE:-0}" == 0 ]] ;;
    xcrun)
        case "$*" in
            '--sdk macosx --show-sdk-version') echo 15.0 ;;
            'notarytool history --keychain-profile fixture --output-format json') echo '{}' ;;
            *) exit 91 ;;
        esac ;;
    *) echo "Unexpected tool execution: ${0##*/}" >&2; exit 92 ;;
esac
MOCK
chmod +x "${TEST_ROOT}/bin/mock-tool"
for tool in uname git security xcodebuild xcrun cmake ctest codesign ditto lipo \
    pkgbuild productbuild plutil xattr pkgutil productsign spctl; do
    ln -s mock-tool "${TEST_ROOT}/bin/${tool}"
done
export PATH="${TEST_ROOT}/bin:${PATH}" RELEASE_MODE=1 CONFIG=Release
export APP_SIGN_IDENTITY='Developer ID Application: Fixture'
export INSTALLER_SIGN_IDENTITY='Developer ID Installer: Fixture' NOTARY_PROFILE=fixture
unset BUILD_DIR VERSION JUCE_PATH YOUKNOW_JUCE_PATH FETCHCONTENT_SOURCE_DIR_JUCE
PACKAGER="${TEST_ROOT}/project/scripts/sign-and-package-macos.sh"

expect_failure() {
    local expected="$1"
    shift
    if "$@" > "${TEST_ROOT}/output" 2>&1; then
        echo "error: expected failure: ${expected}" >&2
        exit 1
    fi
    if ! grep -F -- "${expected}" "${TEST_ROOT}/output" >/dev/null; then
        cat "${TEST_ROOT}/output" >&2
        exit 1
    fi
}

"${PACKAGER}" --preflight >/dev/null
test -f "${TEST_ROOT}/project/build-macos/dist/YouKnow-old-macOS-universal.pkg"
test ! -e "${TEST_ROOT}/project/build-macos/package-root"
expect_failure APP_SIGN_IDENTITY env APP_SIGN_IDENTITY=- "${PACKAGER}" --preflight
expect_failure 'clean YouKnow source tree' env TEST_SOURCE_DIRTY=1 "${PACKAGER}" --preflight
expect_failure 'must point exactly at HEAD' env TEST_TAG_COMMIT=older "${PACKAGER}" --preflight
expect_failure 'full Xcode' env TEST_NO_XCODE=1 "${PACKAGER}" --preflight
expect_failure 'does not match CMake version' env VERSION=2.0.0 "${PACKAGER}" --preflight
expect_failure 'fresh build directory' bash "${TEST_ROOT}/project/scripts/release-macos.sh"
expect_failure 'full Xcode' env TEST_NO_XCODE=1 bash "${TEST_ROOT}/project/scripts/release-macos.sh"
printf '### 1.1.0 — unreleased (2026-09-05)\n' > "${TEST_ROOT}/project/README.md"
expect_failure 'dated' "${PACKAGER}" --preflight
test -f "${TEST_ROOT}/project/build-macos/dist/YouKnow-old-macOS-universal.pkg"
echo 'Release preflight checks passed.'
