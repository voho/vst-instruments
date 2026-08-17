# Commercial macOS release checklist

Every **BLOCKER** item must be resolved and recorded before publication. A
checked build or packaging item is not evidence that an unresolved legal item
has been cleared.

## Rights and business gates

- [ ] **BLOCKER:** Record the commercial JUCE 8 licence and seat entitlement
  covering the released binary and release team.
- [ ] **BLOCKER:** Obtain and record a qualified rights assessment for the 128
  historical factory tone states and their archival display names. The cited
  archives provide no explicit MIT grant; see
  [`THIRD_PARTY_NOTICES.md`](../THIRD_PARTY_NOTICES.md).
- [ ] **BLOCKER:** Complete and record trademark/trade-dress review of the
  YouKnow106 name, Roland/Juno references, panel presentation, marketing copy,
  icon, and store artwork. The existing no-affiliation statement is not a
  clearance opinion.
- [ ] Confirm Protocodus owns or has written commercial permission for every
  shipped source file, texture, icon, screenshot, demo, preset, and other asset.
- [ ] Approve the customer licence/EULA, save the final text as `EULA.md`, and
  approve sales terms, refund policy, tax setup, and any store-specific
  disclosures. Production packaging rejects a missing `EULA.md`.
- [ ] Approve the privacy notice and support-data retention/request process for
  every sales jurisdiction; add the legal entity/address if required.

## Candidate identity

- [ ] Version agrees across CMake, every bundle `Info.plist`, the package, user
  guide, `CHANGELOG.md`, store listing, and release tag.
- [ ] Replace `Unreleased` in the matching changelog heading with the ISO release
  date before tagging; the production script rejects an undated changelog.
- [ ] Vendor is Protocodus; website is <https://protocodus.cz>; support is
  [protocodus@proton.me](mailto:protocodus@proton.me).
- [ ] Confirm the chosen public hostname resolves over HTTPS and matches every
  bundle, manifest, store page, and customer document. The supplied
  `www.protocodus.cz` hostname must not replace the working apex URL until its
  DNS and HTTPS configuration are live.
- [ ] Bundle/package identifiers and AU/VST3 manufacturer and plug-in codes are
  final and unchanged from any previously published build. Confirm Standalone
  uses `cz.protocodus.youknow106`, Audio Unit uses
  `cz.protocodus.youknow106.au`, and VST3 uses
  `cz.protocodus.youknow106.vst3`.
- [ ] Release commit is reviewed, clean, and tagged exactly
  `youknow106-v<version>`; that annotated or lightweight tag points at HEAD.
- [ ] Release uses the JUCE source fetched from the pinned CMake declaration;
  `JUCE_PATH`, `YOUKNOW106_JUCE_PATH`, `FETCHCONTENT_SOURCE_DIR_JUCE`, and
  cached local overrides are absent.
- [ ] Candidate targets macOS 11.0 and contains both `arm64` and `x86_64` slices
  in VST3, Audio Unit, and Standalone.

## Build and validation

- [ ] Any preliminary `scripts/build-macos.sh` run and all CTest steps pass in
  Release configuration. Before the final command, move or remove any existing
  `build-macos` directory; the production lane rejects it, then builds and
  retests from a fresh fetched dependency tree.
- [ ] Run `auval -v aumu Yk06 Ykno -strict`, Tracktion pluginval at strictness
  10 with a recorded random seed, and Steinberg's VST3 validator with its
  extensive-test option; retain the commands, tool versions, binary hash, and
  reports with the release record.
- [ ] Smoke-test VST3 and Audio Unit in supported hosts and the Standalone app
  on a clean Intel Mac and Apple-silicon Mac, including a macOS 11 machine.
- [ ] Exercise note/MIDI input, automation and state recall, all factory
  programs, QUALITY changes, chorus, SysEx load/save, and audio-device changes.
- [ ] Profile idle and standard-bypass behavior in the supported hosts with
  several instances. The continuous chorus-noise model currently reports an
  infinite tail and therefore may prevent host suspension; explicitly accept
  that behavior for v1 or qualify a finite-tail/idle-retirement policy before
  release.
- [ ] Test every control keyboard-only and with VoiceOver at every supported UI
  size; confirm the performance lever announces both axes and springs to zero.
- [ ] Confirm support, privacy, change history, approved customer terms,
  source-code licence, third-party notices, JUCE licence index, and the exact
  JUCE dependency licence files are present in the shared documentation folder
  and all three bundles.

## Sign, notarize, and package

- [ ] Install valid `Developer ID Application` and `Developer ID Installer`
  identities, and create a tested `notarytool` Keychain profile.
- [ ] Before using the hosted lane, enable GitHub release immutability and
  create a protected `youknow106-production` environment. Require a reviewer
  and restrict deployments to the `youknow106-v*` tag pattern. Store signing
  material only as environment secrets, never repository-level secrets.
- [ ] Add an active tag ruleset for `refs/tags/youknow106-v*` that restricts
  updates and deletions with no bypass actors. This locks the candidate tag
  during the build; publishing the immutable release permanently locks its tag
  and assets.
- [ ] Configure `YOUKNOW106_APP_SIGN_IDENTITY` and
  `YOUKNOW106_INSTALLER_SIGN_IDENTITY` as environment variables. Configure the
  following environment secrets: application and installer certificate PKCS#12
  blobs and passwords (`YOUKNOW106_APP_CERTIFICATE_P12_BASE64`,
  `YOUKNOW106_APP_CERTIFICATE_P12_PASSWORD`,
  `YOUKNOW106_INSTALLER_CERTIFICATE_P12_BASE64`,
  `YOUKNOW106_INSTALLER_CERTIFICATE_P12_PASSWORD`); a Team App Store Connect
  API key (`YOUKNOW106_NOTARY_KEY_P8_BASE64`, `YOUKNOW106_NOTARY_KEY_ID`,
  `YOUKNOW106_NOTARY_ISSUER_ID`); and a fine-grained token scoped only to this
  repository with Administration read/write and Actions read access
  (`YOUKNOW106_RELEASE_SETTINGS_TOKEN`). GitHub omits ruleset bypass actors
  without write access, so the workflow fails closed if that field cannot be
  inspected. Keep this token only in the protected release environment.
- [ ] From the clean, exactly tagged commit, export `APP_SIGN_IDENTITY`,
  `INSTALLER_SIGN_IDENTITY`, and `NOTARY_PROFILE`, then run
  `scripts/release-macos.sh`. This is the sole production lane: it forces and
  performs the Release, universal, macOS 11 build and tests before packaging.
- [ ] For a hosted release, push the exact `youknow106-v<version>` tag after all
  earlier blockers are checked. Confirm the `Release YouKnow106` workflow
  verifies that the commit is on `main`, imports credentials only after the
  protected-environment approval, and publishes exactly the notarized PKG,
  manifest, and checksum file as an immutable GitHub release.
- [ ] Confirm the script reports Apple notarization `Accepted`, staples and
  validates the ticket, and passes `pkgutil --check-signature` and
  `spctl --assess --type install`.
- [ ] Verify the manifest records the intended version, universal slices,
  macOS 11.0 target, source commit, all three bundle identifiers, package
  identifier, customer-licence and JUCE-licence hashes, notarization
  submission, and final package SHA-256.
- [ ] Verify `shasum -a 256 -c YouKnow106-<version>-macOS-universal-SHA256SUMS.txt`
  from the distribution directory.
- [ ] Install the final PKG on a clean standard-user system, launch each format,
  reboot/rescan, upgrade over the prior public build if applicable, and follow
  the user-guide uninstall procedure.

## Publication and support

- [ ] Publish only the signed, Apple-notarized, stapled universal PKG as the v1
  software payload. Do not publish a ZIP, unsigned PKG, unstapled candidate, or
  loose plug-in bundle. The manifest and checksum file may accompany the PKG.
- [ ] Check the downloaded public PKG hash against the retained release record
  and re-run Gatekeeper and stapler validation on that exact download.
- [ ] Publish the matching changelog, system requirements, format list,
  installation/uninstallation steps, privacy notice, support address, price,
  licence, and no-affiliation statement.
- [ ] Archive the source tag, compiler/Xcode/CMake versions, test and validator
  reports, final PKG, manifest/checksums, notarization result, certificates and
  expiry dates, store copy, and signed legal approvals.
- [ ] Test the support mailbox and website links, assign an owner for customer
  reports, and define the patch-release and certificate-renewal process.
