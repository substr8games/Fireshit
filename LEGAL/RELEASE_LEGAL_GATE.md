# Fireshit Legal Release Gate
## Version 0.1

A failed item blocks the entire Fireshit suite release.

## License identity

- [ ] Root `LICENSE` contains the intended FCIL version.
- [ ] `LEGAL/FIRESHIT_CAPTURE_IMMUNITY_LICENSE.md` matches the root license.
- [ ] No first-party file claims MIT, GPL, or another unintended license.
- [ ] First-party package metadata uses `LicenseRef-FCIL-0.1`; source-file SPDX headers are recommended and required when added by project policy.
- [ ] Copyright notices identify `Giz@SUBSTR8Games`.
- [ ] README and public documentation call Fireshit source-available, not open source.

## Suite alignment

- [ ] Every first-party component actually included in the release, including Firemod, FrameTap, SUBSTR8 HUD, services, plugins, icons, configuration, and documentation, carries the suite license or an expressly recorded narrow metadata exception.
- [ ] Release tag, package version, documentation, and suite version agree.
- [ ] Source and binary archives include the license and notices.
- [ ] No module is independently represented as MIT-licensed or separately relicensable.

## Third-party material

- [ ] Every bundled or vendored component appears in `THIRD_PARTY_NOTICES.md`.
- [ ] Every required license and attribution notice is present.
- [ ] No barred license is present without a written exception.
- [ ] Generated files have known provenance.
- [ ] Lockfiles were reviewed against actual shipped contents.
- [ ] System dependencies are documented separately from bundled material.

## Contributions and provenance

- [ ] Every substantive outside contribution has an executed assignment.
- [ ] Employer or client rights were resolved.
- [ ] AI-assisted material passed the human acceptance gate.
- [ ] Substantial AI-assisted work has a provenance record.
- [ ] Original graphics and source masters are preserved.

## Packaging

- [ ] Cargo packages use a valid `license-file` path.
- [ ] npm packages use `SEE LICENSE IN LICENSE`.
- [ ] Arch packaging uses `LicenseRef-FCIL-0.1`.
- [ ] The Arch package installs `/usr/share/licenses/fireshit/LICENSE`.
- [ ] Split packages include or reference the canonical suite license.
- [ ] Third-party notices are installed where the package format requires them.

## Marks and public identity

- [ ] Official names and icons identify only canonical releases.
- [ ] Fork or compatibility material does not imply official status.
- [ ] Public release pages identify Giz@SUBSTR8Games as steward and licensing authority.
- [ ] No third-party mark is used without permission.

## Privacy and security

- [ ] No undisclosed telemetry, analytics, upload, or remote logging path exists.
- [ ] Every networked feature is documented.
- [ ] `SECURITY.md` identifies a usable private reporting path in the canonical repository.
- [ ] Capture and recording responsibility language is present.

## Release approval

Fireshit version: 1.1

Reviewed by: __________________________

Review date: __________________________

Approved for publication: YES / NO

Signature or signed-tag reference: ______________________________
