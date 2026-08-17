# YouKnow106 release record

Copy this file for each commercial candidate and keep the completed record in
the approved release archive. Record references and hashes for confidential
material; never store private keys, passwords, API tokens, certificate blobs,
or confidential legal advice in this repository.

## Candidate

- Version:
- Release date (UTC):
- Tag:
- Source commit:
- Release approver:
- Release archive location:
- Store/listing URL:
- Completed checklist evidence reference/SHA-256:
- Audio Unit identity (`type/subtype/manufacturer`):
- VST3 processor/controller CIDs:

## Rights and business approvals

| Gate | Approval/reference | Approver | Date |
| --- | --- | --- | --- |
| Commercial JUCE licence and seats |  |  |  |
| Factory-tone data and names |  |  |  |
| Trademark and trade dress |  |  |  |
| Source and asset ownership |  |  |  |
| EULA, sales terms, refunds and tax |  |  |  |
| Privacy and support-data process |  |  |  |

- EULA SHA-256:
- Privacy notice SHA-256:
- Store copy SHA-256:
- Infinite-tail/host-suspension decision:

## Build provenance

- macOS and build host:
- Xcode and SDK:
- C++ compiler:
- CMake:
- JUCE version/commit:
- JUCE `LICENSE.md` SHA-256:
- Build command:
- CTest command/result/report SHA-256:
- Production signed VST3 executable SHA-256:
- Production signed Audio Unit executable SHA-256:
- Production signed Standalone executable SHA-256:

## Automated validation

| Validator | Version/source | Exact command | Tested binary SHA-256 | Result | Report SHA-256 |
| --- | --- | --- | --- | --- | --- |
| `auval -strict` |  |  |  |  |  |
| pluginval strictness 10 |  | Seed:  |  |  |  |
| Steinberg VST3 extensive |  |  |  |  |  |

- Relationship between tested and production binaries, with approval if they
  are source-equivalent rather than byte-identical:

## Host and hardware qualification

| Mac/OS | Host/version | Format | Scenarios | Result/report |
| --- | --- | --- | --- | --- |
| Apple silicon |  |  |  |  |
| Intel, macOS 11 |  |  |  |  |

Record note and MIDI input, automation, state recall, all factory programs,
QUALITY changes, chorus, SysEx load/save, audio-device changes, idle/bypass CPU,
multi-instance use, keyboard-only operation, VoiceOver, and every supported UI
size.

## Signing, notarization and package

- Developer ID Application identity/team/serial/expiry:
- Developer ID Installer identity/team/serial/expiry:
- Notarization submission ID/status:
- Stapler validation result:
- `pkgutil --check-signature` result:
- `spctl --assess --type install` result:
- Manifest SHA-256:
- Manifest verification result/workflow run:
- Checksum-file SHA-256:
- `SHA256SUMS` verification result:
- Final PKG SHA-256:
- Clean install/reboot/rescan result:
- Upgrade/uninstall result:
- Pre-v1 nightly identity migration result:

## Publication and support

- Immutable GitHub release URL:
- Production workflow run URL/result:
- Downloaded PKG SHA-256:
- Downloaded Gatekeeper/stapler result:
- Product/support/privacy URLs checked at:
- Support mailbox test result and owner:
- Patch-release owner/process:
- Certificate-renewal owner/date/process:

## Exceptions

List every accepted deviation, its risk owner, rationale, and expiry. An empty
section means no exceptions were accepted. No exception may waive a **BLOCKER**
from the commercial release checklist.
