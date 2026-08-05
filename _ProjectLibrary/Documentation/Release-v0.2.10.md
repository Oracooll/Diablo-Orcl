# Diablo Oracool Edition v0.2.10 Release Record

## Identity

- Oracool release: `v0.2.10`
- DevilutionX engine base: `1.5.5`
- Release date: 2026-08-05
- Source branch: `oracool-main`
- Feature baseline commit: `dc18f6fc0022f0684443d6db7d01947f49dd8e70`
- Build type: optimized Windows x64 Release (Discord SDK integration enabled)

The executable deliberately retains the engine version `1.5.5`. The separate
`v0.2.10` number identifies this Oracool Edition release without changing
DevilutionX network/version compatibility identifiers.

## Artifact

- File: `C:\DiabloDOE\Releases\Diablo Oracool Edition v0.2.10.zip`
- Size: 4,232,351 bytes
- SHA-256: `CAF4F3684EECA2E0F310517A4829BF747EE71C27BF90D5B061A6182A33E931F0`
- Verified files: 15

Every file in the ZIP was compared byte-for-byte (SHA-256) against its
staged Release-build source, matching the v0.2.8/v0.2.6/v0.2.1/v0.2.0/v0.1.0
package structure exactly (devilutionx.exe, 8 distributable libraries,
license notices, README).

## Release scope

Covers both OE-027 (v0.2.9) and OE-028 (v0.2.10), neither individually
published before now. Fully save-compatible with v0.2.6 through v0.2.9 - no
save-format change since v0.2.6.

- OE-027 (v0.2.9): autosave notifications switched from the full "Game
  Saved" banner to a subtle top-left blink-and-fade indicator. Manual
  saves via the game menu are unchanged.
- OE-028 (v0.2.10): TAB now cycles through no map / mini-map / full map
  instead of a plain on/off toggle. `DrawAutomap` refactored into a shared
  `DrawAutomapCore` so the mini-map reuses the same tile/player rendering
  at a smaller scale in a screen corner.

Both features are **unverified visually** - no rendering/screenshot
tooling was available while building them. This release is their first
real-world exposure; the user will evaluate legibility, sizing, and feel
during their regular playthrough rather than a pre-release manual pass.

Detailed behavior and test status remain authoritative in
`Gameplay-Changes.md` (see OE-027, OE-028) and `Testing-Guide.md`.
Automated regression: 185 tests across 7 suites, 100% passing at release
time (both features are rendering/timing-only, with no new pure-logic
unit to isolate - the full existing suite passing confirms no regression
in the refactored automap code, not the new visuals themselves).

## Installation

Extract directly into a working vanilla DevilutionX 1.5.5 folder and allow
replacement of same-named files. Run `devilutionx.exe`; the title should
identify Diablo Oracool Edition v0.2.10 based on DevilutionX 1.5.5. Save
compatibility notes are unchanged from v0.2.6 - see `Release-v0.2.6.md`.
