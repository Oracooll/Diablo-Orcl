# Diablo Oracool Edition v0.2.8 Release Record

## Identity

- Oracool release: `v0.2.8`
- DevilutionX engine base: `1.5.5`
- Release date: 2026-08-05
- Source branch: `oracool-main`
- Feature baseline commit: `6a88e6519e50c851ea3637873c3022953cdcf93e`
- Build type: optimized Windows x64 Release (Discord SDK integration enabled)

The executable deliberately retains the engine version `1.5.5`. The separate
`v0.2.8` number identifies this Oracool Edition release without changing
DevilutionX network/version compatibility identifiers.

## Artifact

- File: `C:\DiabloDOE\Releases\Diablo Oracool Edition v0.2.8.zip`
- Size: 4,232,118 bytes
- SHA-256: `A1BB1F155686526AC68C82D94AFB686B7F6A4367608EAC9E949BC5A6AF27FF3D`
- Verified files: 15

Every file in the ZIP was compared byte-for-byte (SHA-256) against its
staged Release-build source, matching the v0.2.6/v0.2.1/v0.2.0/v0.1.0
package structure exactly (devilutionx.exe, 8 distributable libraries,
license notices, README).

## Release scope

Covers both OE-025 (v0.2.7) and OE-026 (v0.2.8), neither individually
published before now. Fully save-compatible with v0.2.6 - no save-format
change since then.

- OE-025 (v0.2.7): new inventory sort button, repacks the backpack and
  extra tabs by sell value. Built and automated-test-verified overnight
  while the user was away; this release is its first in-game exposure.
- OE-026 (v0.2.8): gold pickup and sale proceeds now go to the shared
  Stash pool instead of inventory. Fixed `HasRoomForGold` (autopickup.cpp)
  so gold auto-pickup doesn't stop once the inventory fills with unrelated
  items. Character panel gold display now includes the Stash total.

Detailed behavior and test status remain authoritative in
`Gameplay-Changes.md` (see OE-025, OE-026) and `Testing-Guide.md`.
Automated regression: 185 tests across 7 suites, 100% passing at release
time. Released immediately per user request, ahead of their manual
in-game pass - both features are new ground for manual testing.

## Installation

Extract directly into a working vanilla DevilutionX 1.5.5 folder and allow
replacement of same-named files. Run `devilutionx.exe`; the title should
identify Diablo Oracool Edition v0.2.8 based on DevilutionX 1.5.5. Save
compatibility notes are unchanged from v0.2.6 - see `Release-v0.2.6.md`.
