# Fireshit Third-Party Material Policy
## Version 0.1

## 0. Rule

First-party Fireshit material is FCIL. Third-party material retains its own license. Inclusion in the repository does not relicense third-party work and does not relicense Fireshit under the third party’s terms.

## 1. Scope

This policy applies to:

- vendored source;
- copied snippets;
- generated code derived from third-party source;
- bundled libraries;
- icons, fonts, audio, images, and other assets;
- documentation excerpts;
- build tools shipped inside a release;
- patches applied to third-party work; and
- code or assets proposed through contributions or AI-assisted workflows.

## 2. Generally admissible licenses

After provenance and compatibility review, the following are generally admissible:

- MIT
- Apache-2.0
- BSD-2-Clause
- BSD-3-Clause
- ISC
- 0BSD
- Zlib
- clearly documented public-domain material
- CC0-1.0 where appropriate for the material type

Admission is not automatic. Notice, attribution, patent, modification, and source requirements must still be satisfied.

## 3. Barred licenses

The following may not be copied into, vendored with, generated into, or bundled as part of Fireshit without an explicit written exception from Giz@SUBSTR8Games:

- GPL
- AGPL
- SSPL
- LGPL
- MPL
- EPL
- CDDL
- Creative Commons ShareAlike licenses
- licenses that impose source disclosure on unrelated Fireshit code
- licenses that prohibit FCIL distribution
- licenses with unclear ownership
- licenses whose field-of-use terms conflict with Fireshit
- “no commercial use” assets unless the intended Fireshit distribution is expressly permitted
- code copied from sources that provide no usable license

## 4. System dependencies

A library supplied independently by the operating system may be used as an external dependency when it:

- is not copied into the Fireshit release;
- remains independently replaceable;
- is invoked through an ordinary system or library boundary;
- is listed as a platform dependency; and
- does not impose terms on first-party Fireshit code merely through that use.

This rule does not override the actual terms of the dependency.

## 5. Required review record

Every bundled or vendored third-party component must record:

- component name;
- version or commit;
- upstream source;
- copyright holder;
- exact license;
- repository location;
- release location;
- whether modified;
- required notices;
- reviewer;
- approval date; and
- removal or replacement notes, when applicable.

The canonical release inventory is `THIRD_PARTY_NOTICES.md`.

Lockfiles and package-manager metadata are evidence, not the complete legal inventory.

## 6. Modifications

Modified third-party files must preserve upstream notices and identify Fireshit modifications when required or reasonably useful.

Fireshit modification notices must not imply ownership of unchanged upstream material.

## 7. Snippets

A small snippet is not exempt merely because it is small.

Before copying any snippet, determine whether it is:

- original first-party work;
- too trivial to carry copyrightable expression;
- permissively licensed with satisfied conditions; or
- prohibited from inclusion.

When provenance is unclear, rewrite from requirements rather than copying.

## 8. AI-assisted intake

AI output must be reviewed for recognizable third-party source, comments, identifiers, unusual phrasing, or license headers.

A model output that reproduces or closely adapts third-party material remains third-party material.

Unknown provenance is a reason to reject or rewrite the material.

## 9. Release gate

A release fails legal review when:

- a bundled component lacks an identified license;
- a required notice is missing;
- first-party code claims a third-party license;
- barred material is present without written exception;
- a dependency was copied into the release without inventory; or
- the package does not include applicable third-party notices.
