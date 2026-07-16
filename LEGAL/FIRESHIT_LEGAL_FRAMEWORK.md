# [FIRESHIT][2026-07-15][LEGAL FRAMEWORK][SPECIFICATION][LOCKED][SUITE][OWNERSHIP, LICENSING, MARKS, CONTRIBUTIONS, DISTRIBUTION]

## 0. FINAL LAW

Fireshit is a source-available, capture-immune software suite governed by the Fireshit Capture Immunity License.

The legal boundary follows Fireshit code, assets, documentation, releases, and official identity. It does not spread through APIs, protocols, command-line invocation, IPC, plugins, separate applications, or ordinary interoperability.

First-party Fireshit material is governed by FCIL, except for narrowly identified file-specific metadata necessary for software-catalog redistribution.

Third-party material retains its original license and must remain separately identified.

No Fireshit release may publish until its license metadata, copyright ownership, third-party inventory, marks policy, contributor records, and release contents agree.

Fireshit does not claim ownership of the ecosystem around it. It denies anyone ownership of Fireshit through capture.

## 1. LEGAL AUTHORITIES

The Fireshit legal system consists of the following authorities:

1. `LICENSE`  
   The exact Fireshit Capture Immunity License applicable to the release.

2. `LEGAL/FIRESHIT_CAPTURE_IMMUNITY_LICENSE.md`  
   The versioned canonical license text.

3. `LEGAL/FIRESHIT_MARKS_POLICY.md`  
   Rules governing names, logos, icons, official status, compatibility claims, and forks.

4. `LEGAL/FIRESHIT_CONTRIBUTOR_ASSIGNMENT.md`  
   The signed agreement required for substantive outside contributions.

5. `LEGAL/FIRESHIT_THIRD_PARTY_POLICY.md`  
   The admission, classification, and preservation rules for third-party material.

6. `THIRD_PARTY_NOTICES.md`  
   The release-specific inventory of bundled third-party material and applicable licenses.

7. `LEGAL/FIRESHIT_COMMERCIAL_LICENSING.md`  
   The procedure for requesting written exceptions to FCIL restrictions.

8. `LEGAL/FIRESHIT_AI_PROVENANCE.md`  
   The human-authorship and provenance requirements for AI-assisted material.

9. `PRIVACY.md`  
   The suite’s local-processing and telemetry policy.

10. `SECURITY.md`  
    The vulnerability-reporting and coordinated-disclosure policy.

11. `CONTRIBUTING.md`  
    The operational contribution rules and legal intake gate.

The root `LICENSE` controls first-party code in the release. A file carrying a more specific valid license notice is governed by that notice. The AppStream metadata license exception is recorded in `THIRD_PARTY_NOTICES.md` and applies only to the catalog record itself.

## 2. COPYRIGHT OWNERSHIP

Copyright in original Fireshit code, documentation, graphics, icons, interfaces, build material, and other expressive works is held by the identified Fireshit copyright steward.

The public copyright notice is:

> Copyright © 2026 Giz@SUBSTR8Games. All rights reserved.

The steward’s legal identity must be used privately in registrations, assignments, commercial agreements, and enforcement documents where a legal name is required.

Every first-party source file should carry:

```text
SPDX-FileCopyrightText: 2026 Giz@SUBSTR8Games
SPDX-License-Identifier: LicenseRef-FCIL-0.1
```

Files containing third-party material must carry the correct copyright and license declarations for that material.

Possession of a repository copy, package, binary, build artifact, patch, or generated output does not transfer copyright ownership.

## 3. AI-ASSISTED MATERIAL

AI systems are tools and are not authors, copyright owners, licensors, or contributors to Fireshit.

Raw generated material does not enter the canonical repository automatically.

Before AI-assisted code, documentation, graphics, or other material becomes first-party Fireshit material, Giz or another authorized human contributor must:

1. review the material in full;
2. determine that it belongs in the project;
3. verify its technical and legal provenance to the extent reasonably possible;
4. make any modifications necessary to express the contributor’s own design and implementation decisions;
5. integrate it into the surrounding architecture;
6. test or otherwise validate its operation;
7. accept responsibility for the resulting contribution; and
8. record the human contributor as the author of the human-authored contribution.

The repository must not describe a model as an author or copyright holder.

Material substantially copied from a named third-party source through an AI system remains third-party material and is not converted into first-party Fireshit material.

Copyright registrations must accurately disclose material known to be purely AI-generated and identify the human-authored selection, arrangement, modification, integration, and expression being claimed.

## 4. MARKS AND OFFICIAL IDENTITY

The following are Fireshit marks and controlled project identities:

- Fireshit
- Firemod
- FrameTap
- SUBSTR8 HUD
- Firestate
- the canonical Fireshit suite icon
- module icons and official compatibility badges
- official graphical lockups and distinctive project identity assets

The marks policy grants permission for:

- truthful attribution;
- discussion, review, criticism, and reporting;
- screenshots showing the software in operation;
- factual statements that a separate work interoperates with Fireshit;
- links to official Fireshit releases; and
- unmodified display of marks inside an authorized Fireshit release.

Written permission is required for:

- use of a Fireshit mark in the primary name of a fork, product, service, package repository, domain, or organization;
- modified, recolored, remixed, or repurposed Fireshit brand assets;
- claims of official, certified, approved, endorsed, supported, or verified status;
- use that creates a false impression of project ownership or affiliation; and
- merchandise or commercial promotional use.

A fork must adopt a distinct primary name and visual identity. It may state:

> Based on Fireshit version [VERSION].

Independent projects may state:

> Compatible with Fireshit.

They may not state or imply:

> Official Fireshit  
> Fireshit Certified  
> Approved by Fireshit  
> Supported by Fireshit

unless the project steward has granted that designation in writing.

Technical compatibility status is an editorial and engineering determination made by the Fireshit project. Removing a project from an official compatibility list does not revoke lawful interoperability rights.

## 5. CONTRIBUTOR AUTHORITY

Fireshit uses centralized copyright stewardship.

A substantive outside contribution may be merged only after the contributor signs the Fireshit Contributor Assignment Agreement.

The agreement must:

- identify the contributor and copyright steward;
- assign copyright in the accepted contribution to the steward;
- grant the contributor a permanent license to reuse their own contribution independently;
- authorize the steward to enforce, license, relicense, and commercially license the contribution;
- include an express patent license for patent claims necessarily infringed by the contribution;
- confirm that the contributor has authority to contribute the material;
- confirm that employer or client rights have been resolved;
- disclose third-party and AI-assisted material;
- exclude warranties beyond provenance and authority; and
- state that the project has no obligation to accept or retain the contribution.

A Git commit, pull request, issue comment, or developer certificate sign-off does not substitute for the signed assignment when ownership is being transferred.

Trivial typo fixes and suggestions that contain no meaningful copyrightable authorship may be accepted without assignment at the steward’s discretion.

No outside contribution is merged under an implied license.

## 6. THIRD-PARTY MATERIAL

Third-party code never becomes FCIL merely because it is included in the Fireshit repository or package.

Each third-party component retains:

- its copyright notice;
- its exact license;
- its attribution requirements;
- its source-offer requirements, where applicable; and
- any required notices or disclaimers.

The following licenses are generally admissible after provenance review:

- MIT
- Apache-2.0
- BSD-2-Clause
- BSD-3-Clause
- ISC
- 0BSD
- Zlib
- clearly documented public-domain material
- CC0-1.0 where suitable for the type of material

The following are barred from first-party source, vendored code, generated code, copied assets, and shipped bundled components unless Giz gives an explicit written project exception:

- GPL
- AGPL
- SSPL
- LGPL
- MPL
- EPL
- CDDL
- Creative Commons ShareAlike licenses
- licenses imposing source disclosure on unrelated Fireshit code
- licenses prohibiting FCIL distribution
- licenses with unclear provenance or incompatible field-of-use terms

Externally provided system libraries may be used when they are not copied into the Fireshit release, remain independently replaceable, and are documented as platform dependencies. Their presence does not change the license of first-party Fireshit code.

`THIRD_PARTY_NOTICES.md` must identify every vendored or bundled third-party component by:

- name;
- version or commit;
- source location;
- copyright holder;
- license;
- location inside the release;
- whether it is modified;
- applicable notice file; and
- reviewer approval.

Lockfiles alone are not the legal inventory.

## 7. COMMERCIAL PERMISSIONS

FCIL remains the public license.

The copyright steward may grant separate written permission for activities including:

- OEM installation;
- proprietary hardware bundling;
- commercial redistribution;
- managed deployment;
- hosted access;
- enterprise integration;
- private-label distribution; and
- other uses prohibited by FCIL.

Every commercial exception must identify:

- the legal parties;
- covered Fireshit versions and components;
- permitted activity;
- territory;
- term;
- fees or consideration;
- source and modification obligations;
- marks permissions;
- security and support obligations;
- transfer and sublicensing limits;
- termination conditions; and
- governing law and venue.

No conversation, issue comment, pull request, email summary, payment, sponsorship, or donation creates a commercial exception.

Commercial permission exists only in a signed written agreement.

A commercial agreement does not alter FCIL rights for the public or for other recipients.

## 8. PRIVACY AND RECORDING AUTHORITY

Fireshit processes capture, recording, configuration, logs, and related data locally unless a user deliberately directs output elsewhere.

The canonical privacy posture is:

- no telemetry;
- no behavioral analytics;
- no advertising identifiers;
- no hidden remote logging;
- no cloud account requirement;
- no automatic upload of recordings;
- no sale of user information; and
- no transmission of captured application audio or video to the Fireshit project.

Any future networked feature must be documented before release and must identify:

- the destination;
- the data transmitted;
- the purpose;
- retention behavior;
- whether activation is optional; and
- how it is disabled.

Users control the material they capture and are responsible for authorization, consent, copyright, privacy, employment, contractual, and recording-law compliance.

Fireshit does not grant rights in third-party applications, displays, audio, communications, or content merely because the software can capture them.

## 9. SECURITY AND RESEARCH

`SECURITY.md` must provide a private reporting channel for vulnerabilities affecting current supported releases.

Good-faith security research is authorized when the researcher:

- tests only systems and copies they own or are authorized to test;
- avoids accessing unrelated personal data;
- avoids service disruption;
- avoids persistence, extortion, or public exploitation;
- reports the issue privately with sufficient reproduction information; and
- allows a reasonable coordinated-disclosure period.

The project does not promise payment or a bug bounty unless one is announced separately.

Security reports do not transfer ownership of the reported research or automatically create confidentiality obligations beyond the stated disclosure process.

## 10. REGISTRATION AND ENFORCEMENT

The project steward should preserve:

- dated source history;
- signed release tags;
- release archives;
- contributor assignments;
- design-source files;
- icon and graphic masters;
- third-party review records;
- AI provenance records;
- published license versions; and
- evidence of public use of project marks.

Copyright registration should cover major release snapshots and major original visual-asset collections.

Trademark clearance should occur before filing applications for Fireshit and the principal module names.

Enforcement should proceed in the following order when practical:

1. preserve evidence;
2. identify the copied material and applicable license version;
3. confirm ownership and registration status;
4. issue a direct compliance demand;
5. use platform infringement procedures where appropriate;
6. pursue commercial resolution where appropriate; and
7. escalate to formal legal action when required.

The project must not issue knowingly false copyright or trademark complaints.

## 11. RELEASE GATE

A public Fireshit release is legally ready only when all of the following are true:

- the root license is the intended FCIL version;
- every first-party module carries matching license metadata;
- no first-party file claims MIT, GPL, or another unintended license;
- all bundled third-party material is inventoried;
- all required third-party notices are present;
- every substantive outside contribution has valid ownership documentation;
- AI-assisted material has passed human-authority review;
- package manifests point to the custom license correctly;
- the Arch package installs the custom license;
- source and binary archives contain the license;
- README and public documentation describe Fireshit as source-available;
- marks and official-status language are correct; and
- the release tag, package version, documentation, and suite version agree.

A failed legal gate blocks the entire suite release.

## 12. CANONICAL PACKAGE METADATA

### Rust/Cargo

Use the license file rather than pretending FCIL has a standard SPDX-list identifier:

```toml
[package]
license-file = "../../LICENSE"
```

The relative path must resolve correctly for each package.

### npm/package.json

```json
{
  "license": "SEE LICENSE IN LICENSE"
}
```

The referenced license file must be included in the published package.

### Arch PKGBUILD

```bash
license=('LicenseRef-FCIL-0.1')
```

The package must install the license at:

```text
/usr/share/licenses/fireshit/LICENSE
```

Split packages must install or reference the same canonical suite license and include any applicable third-party notices.

### Source files

```text
SPDX-FileCopyrightText: 2026 Giz@SUBSTR8Games
SPDX-License-Identifier: LicenseRef-FCIL-0.1
```

### Public description

> Fireshit is source-available software distributed under the Fireshit Capture Immunity License. Inspection, local execution, modification, community redistribution, and independent interoperability are permitted under the license. Proprietary capture, paid rehosting, closed redistribution, and unauthorized commercial packaging are prohibited.

## 13. GOVERNING PRINCIPLE

Fireshit grants practical user sovereignty while retaining project sovereignty.

Users may understand the system, build it, run it, repair it, modify it, and construct independent software around it.

The project retains the exclusive authority to permit commercial enclosure, official branding, proprietary redistribution, and capture of the Fireshit codebase itself.

This is defensive architecture.
