# Resources rebuilt into four folders, and the private archive is dissolved

**Version:** 1.11.063
**Date:** 2026-09-12
**Branch:** renderer-32bit

## What was asked

> "clear up the Resources folder. Delete Private Assets folder, there are no private assets. All
> goes online as we agreed it is a non-profit mod. Move all content from root and private asset
> folder among the other folders."

Then the target layout:

> "00-original-game-art - everything here is vanilla art from Diablo/Hellfire makers. Not a single
> edited-by-me item should live here. This is our source of original UI elements.
> 01-in-use-assets - every assets that made its way into the latest patches lives here. Only art
> that is in the game as of latest patch.
> 02-concept-assets - every asset we have developed but didn't make its way into the final patches
> of the game or was replaced by newer assets."

And then:

> "you need to keep history of which assets make it into the game or get retired from the game."

## What the private folder actually held

It was not empty of live content. `03-private-assets/oracool_private_assets/` held **twenty assets
the game loads** - nine cutscene paintings, three object `.cel` sprites and eight UI panels - and it
was the pack source for `oracool_private.mpq`, which the engine mounts **ahead of** `oracool.mpq`.

Before publishing them, their provenance was checked rather than assumed: the nine cutscenes are
byte-identical to the AI-generated `loading-screens-no-bars` set and **differ** from the extracted
originals in `00-original-game-art/gendata/`. So they are the user's own work, and the earlier rule
that verbatim Blizzard files stay out of every archive is not breached by publishing them.

All twenty moved into `Packaging/resources/oracool_assets/` with zero filename collisions, verified
byte-for-byte after the copy. The shipped archive went 444 -> **464** files.

## How 2,754 files were classified

Per file, not per folder (the user's choice). Two objective tests:

1. its content matches something in the shipped archive, or
2. a build script reads it by path.

Anything else is a concept. That is why a delivered pack now appears in both trees - the variant that
shipped in `01-in-use-assets/`, its rejected siblings in `02-concept-assets/`.

| | files |
|---|---|
| `00-original-game-art/` (untouched) | 8,403 |
| `01-in-use-assets/` | 1,105 |
| `02-concept-assets/` | 1,628 |
| `ChatGPT RfA/` | 3 |
| moved into the repo's shipped archive | 20 |

`02-source-art/` and `03-private-assets/` are gone, and the root holds only `README.md` and the new
`ASSET-LEDGER.md`.

## The ledger

`Resources/ASSET-LEDGER.md` is the history the user asked for: what the three folders mean, an
*Entered the game* and a *Left the game* table, the procedure for keeping them, and a full inventory
of the 464 shipped files. It names `Packaging/resources/oracool_assets/` as the authority - when the
ledger and the archive disagree, the archive is right.

## Retiring the private archive

`oracool_private.mpq` is no longer built. The engine still *mounts* one when present, which is the
trap worth recording: a stale copy sits **ahead** of `oracool.mpq` in the search order and would
shadow the real files with their old versions. The copies in both build trees and in the RTM folder
were deleted, and `build_oracool_mpq.cmd` now says so where the packing step used to be.

The CMake block that packed it is removed (it would have self-skipped, since it guards on the
folder's existence, but dead config that reads as live is worse than no config).

## Repointing

Thirty-four scripts referenced the old paths. They were rewritten by resolving each referenced path
against the new tree rather than by find-and-replace, so each one follows its own file into whichever
bucket it landed in. Afterwards every referenced path was verified to exist: **45 resolve, 0 broken**.

Two were already broken *before* this move and are now fixed:

- `build_asset_studio.cmd` looked for `town.pal` under `raw/`; the extracted tree uses `palettes/`.
- `GenRunes.ps1` looked for `item-runes-v1.png` under `items/`; it has always been in `item-sets/`.

`CutPaladinSkills.ps1` pointed at a top-level `paladin-skills/` that does not exist - the sheet is
under `skill-glyphs/paladin-skills/` - and now points there.

## Verification

- Debug and Release build clean; **718/718** tests pass.
- `oracool.mpq` repacked at 464 files for both trees; RTM updated with the Release exe and the new
  archive, and its stale `oracool_private.mpq` removed.
- Every asset path referenced by `tools/` and `CMakeLists.txt` resolves.

## A trap worth remembering

The shell heredocs used to write the helper scripts **collapse `\` to `\`**, so a Perl character
class written as `[\/]` reached disk as `[\/]` - a class matching only `/`. The first rewriting pass
therefore silently skipped every backslash path and changed 6 files instead of 28, and reported
success. Writing the same classes as `[\x5c/]` survives. The same collapsing broke a `sed`
expression in the verification step. This is the third time backslashes in generated scripts have
cost a pass in this project.
