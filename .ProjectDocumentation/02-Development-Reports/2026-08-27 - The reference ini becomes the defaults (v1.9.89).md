# The reference ini becomes the defaults (v1.9.89)

Date: 2026-08-27
Version: 1.9.89
Tests: 564/566 serially (the two standing baseline failures)

The user's own `diablo.ini`, adopted as what a fresh install carries.

## What actually differed

The file names about 140 settings. **Only eight of them differed from the shipped defaults** — the
rest already matched, which is worth knowing because it means the file is mostly a record of the
defaults being right rather than of them being wrong.

| setting | was | now |
|---|---|---|
| Splash | Logo and Title | None |
| Monster Density | 100 | 300 |
| Lesser Unique Density | 100 | 300 |
| Rare Item Drop Chance | 20 | 2 |
| Buffed Unique Item Drop Chance | 5 | 1 |
| Unique Item Drop Multiplier | 5 | 1 |
| Permanent Infravision | on | off |
| Griswold Sell Rare Items | off | on |

Five of those move as **one balance decision** rather than five: three times the monsters and
champion packs, with special items an order of magnitude rarer than before. Density supplies the
kills; rarity is what keeps a kill worth having. The old defaults had both dials turned up, which is
the combination that makes a Rare item ordinary within an hour.

## Two things I checked rather than assumed

**`Sound Volume=0` and `Music Volume=0` are not muted.** I was about to flag shipping a silent game
as a concern. `VOLUME_MAX` is **0** and `VOLUME_MIN` is -1600 — the scale is millibels, so zero is
full volume and already the default. A concern raised there would have been noise.

**The whole Keymapping section already matches.** Every bound key in the file — T for portal, the
belt on 1-4, LALT for item highlighting — is the code's default, and every unbound one (QuickSpell
1-12, the potions, Help, Quit) is `SDLK_UNKNOWN` in the code. Nothing to change.

Settings absent from the file (Panel Docking, Balance Telemetry, Vendor Tiered Stock Chance, Game
Speed Readout) were left alone. Absence is not a value.

## Pinned

`OracoolAudit.ShippedDefaultsMatchTheReferenceIni` asserts the eight. A default is easy to adopt and
easy to lose again: it is one literal in a constructor argument list two hundred entries long, and
nothing else in the build refers to it. Only the eight that changed are pinned — pinning the rest
would be pinning DevilutionX's defaults, which is not this fork's business.

---

# Corrections to yesterday's packaging work

Three, and they were all found by doing the work rather than by reviewing it.

## `dist\` broke the CMake configure

The packaging script wrote its zip to `dist\` at the repository root. `CMakeLists.txt:16` treats the
**mere existence** of that folder as "this is a source distribution": it sets `SRC_DIST`, calls
`add_subdirectory(dist)`, and reaches an `install()` for a `devilutionx.mpq` this machine does not
build (no `smpq`). Configure failed outright, and every build after it.

The output goes to the repository root now — where `.gitignore`'s existing
`DiabloOrcl-*-win64.zip` rule was already waiting for it, which is the clue I should have taken as
the answer in the first place.

## I duplicated a README that already existed

`Packaging/windows/RELEASE_README.txt` is the release README, and CMake's own `install()` step
already ships it. I wrote a second one under `tools\`.

Two READMEs, one updated and one not, with no way to tell from either which the release had used —
worse than no template at all. Mine is deleted; the script stamps the real one. Its version line
read **v1.9.31**, fifty-seven versions stale, which is what a hand-typed version number does and
why the placeholder is there now.

## There is a cpack path, and it is inert

CMake can package this properly — `install()` rules for the binary, README, CHANGELOG,
`devilutionx.mpq` and `oracool.mpq`. It is gated on `BUILD_ASSETS_MPQ`, which needs `smpq`, which is
not installed here. That is why `assets\` ships loose and why packaging was being done by hand at
all.

Worth saying plainly: **the script is filling a gap that installing `smpq` would close properly.**
It is the right fix for today and the wrong one for ever.
