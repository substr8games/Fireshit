# Fireshit 1.1 Legal Release Audit

Audit date: **2026-07-15**  
Release type: **Source release**  
Steward: **Giz@SUBSTR8Games**

## Passed

- Root and module license files are byte-identical FCIL 0.1 copies.
- Firemod, FrameTap, and SUBSTR8 HUD build metadata identify version 1.1.0.
- First-party package metadata identifies `LicenseRef-FCIL-0.1`.
- AppStream identifies FCIL as the project license and CC0 only as the catalog metadata license.
- Public README and embedded documentation identify Fireshit 1.1 and FCIL.
- All twelve referenced public documentation images are present.
- Root `run.sh` exists and exposes every operation cited by the public reference guide.
- Security reporting identifies the canonical GitHub private vulnerability-reporting path.
- Third-party notices contain a completed release declaration with no unknown placeholders.
- Firestate executable claims were removed because Firestate is not included in this source release.
- Git internals are absent from the archive.
- Stale 1.0 binaries are absent from the 1.1 source release.
- The historical MIT-to-FCIL boundary is disclosed in `LICENSE_TRANSITION.md`.

## Publication condition

Official 1.1 binaries must be built from the committed 1.1 source before being attached to a release. Their checksums must be generated after that build.

The historical `v1.0` tag must remain unchanged. Publish the corrected source as a new `v1.1` tag.
