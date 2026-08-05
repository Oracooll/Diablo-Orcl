# Diablo Oracool Edition v0.3.0 Release Record

## Identity

- Oracool release: `v0.3.0`
- DevilutionX engine base: `1.5.5`
- Release date: 2026-08-05
- Source branch: `oracool-main`
- Feature baseline commit: `0aed218`
- Build type: optimized Windows x64 Release (Discord SDK integration enabled)

The executable deliberately retains the engine version `1.5.5`. The separate
`v0.3.0` number identifies this Oracool Edition release without changing
DevilutionX network/version compatibility identifiers.

## Artifact

- File: `C:\DiabloDOE\Releases\Diablo Oracool Edition v0.3.0.zip`
- Size: 4,234,692 bytes
- SHA-256: `5CF2FAC932C98446F956C66B16CC6C047AFA9C4325E082C926C5B8D9CAF7FF73`
- Verified files: 15

Package structure matches the v0.2.10/v0.2.8/v0.2.6/v0.2.1/v0.2.0/v0.1.0
packages exactly (devilutionx.exe, 8 distributable libraries, license
notices, README). The 8 libraries were reused byte-for-byte from the
v0.2.10 package (unchanged since that release); only `devilutionx.exe` and
the README are new for v0.3.0.

## Release scope

Covers OE-011 (Torment difficulty), OE-029 (level cap raised to 99), and
OE-030 (experience bar hover now shows per-level progress). This is a
**save-format and network-packet-format break** - `Player::_pExperience`/
`_pNextExper` widened from `uint32_t` to `uint64_t` because the level-99
experience curve (~33.4 billion at level 99) exceeds `UINT32_MAX`. No
version-guard byte exists for this specific field (unlike the OE-016 item
extension format, which does have one) - an old save will not crash, but
will silently misread every player field after the widened ones. Users
must start a fresh character; this is documented prominently in the
release README, CHANGELOG.md, and Testing-Guide.md.

- OE-011: Torment sits above Hell as a new single-player-only difficulty.
  Adjustable "Torment Difficulty Multiplier" (1.1x-5.0x, default 2.0x)
  scales Hell's own monster/treasure formulas further. New "Difficulty
  Level Gate" option (on by default) requires level 15/30/40 for
  Nightmare/Hell/Torment respectively.
- OE-029: character level cap raised from 50 to 99. Levels 1-50 are
  byte-for-byte the original vanilla curve; 51-99 use a new geometric-delta
  formula documented in `Source/playerdat.cpp`. Always-on (no toggle),
  and - unlike every other Oracool feature - not single-player-only, since
  `MaxCharacterLevel` is a core shared constant with no natural per-mode
  branch point.
- OE-030: the experience bar's hover tooltip now shows progress relative
  to the current level (`gained / needed`, resetting to 0 on level-up)
  instead of ever-growing absolute totals.

Both Torment and the level cap increase are explicit user-directed
always-on features (no INI toggle for either), per direct instruction:
"these two features ... are major always-on features ... Save
compatibility is secondary. [Robust] and future-proof coding is primary
target."

Detailed behavior and test status remain authoritative in
`Gameplay-Changes.md` (see OE-011, OE-029, OE-030) and `Testing-Guide.md`.
Automated regression: 194 tests across 9 suites, 100% passing at release
time. **This release has not been manually playtested yet** - the user's
regular playthrough starting now is its first real-world exposure.

## Installation

Extract directly into a working vanilla DevilutionX 1.5.5 folder and allow
replacement of same-named files. Run `devilutionx.exe`; the title should
identify Diablo Oracool Edition v0.3.0 based on DevilutionX 1.5.5. See the
"IMPORTANT: THIS RELEASE IS NOT SAVE-COMPATIBLE" note above and in the
bundled README before loading any existing character.
