# Faster Cast Rate on uniques, sets and runewords, and the spell-animation GIFs (v1.11.042)

**Date:** 2026-09-11
**Branch:** renderer-32bit (default), local commit

The user asked:

> add FCR to uniques, sets and runewords too
>
> also - i need a folder with all spells animations, i GIF so i can see them animated, in order to pick which ones to use in new implemented skills. Make these gifs in C:\Users\hroga\OneDrive\2. Personal Files\Software\Diablo\Resources\00-original-game-art\spellanimations and name them in a way so i know which spell i am looking at.

## Faster Cast Rate: where it lives now

None of the delivered data had a cast-rate stat: not the 250 uniques' JSON, the fifteen `set-data.json` files, or the runeword scheme. So every source is **authored**.

| Source | What | How it survives a load |
|---|---|---|
| Drop tail (v1.11.040) | rings, amulets and helms 5–15%, staves 10–30% | re-derived from the affix records |
| **Uniques** | 20 caster pieces: staves, circlets, amulets, rings, arcane foci, relics. One at 30%, five at 20%, five at 15%, nine at 10% | re-derived from the unique's own row (`RederiveFastCast`) |
| **Set rungs** | Starless Hour 4pc +20, Choir of Silence 4pc +15, Dawnwarden 4pc +15, Leoric's Court 12pc +20 | recomputed every recalc (`ApplySetBonusesToTotals`) |
| **Runewords** | D2's cast-rate words: Stealth 25, White 20, Splendor 10, Spirit 30, Heart of the Oak 40, Insight 35, Obedience 40 | recomputed every recalc (`ApplyRunewordToTotals`) |

**Why the item format did not grow.** `_iPLFastCast` would need a stored field, and the hero-items loader `app_fatal`s on any other item-format version ("Please start a new character"). A bump would have refused the user's own hero. Instead:
- Unique values are fixed (param1 == param2), never rolled, so the loader can re-read them from the unique's row exactly.
- The test enforces that.

**The pieces:**
- **Uniques:**
  - `faster_cast_rate_percent` token → `IPL_FASTCAST` in `unique_affixes.cpp` (46 tokens).
  - The authored table `$authoredFastCast` in `tools/GenUniqueItems.ps1`; the generator throws if a named item is not emitted.
  - `IPL_FASTCAST` added to the generator's additive merge list.
  - Every chosen item had a free power slot. No other row changed, and no icon file changed.
- **Sets:**
  - `faster_cast_rate` keyword in `item_set_stats.cpp`: 117 keywords, ten of them the fork's own, test updated.
  - Added only to existing override rungs with a free stat, so no rung lost anything.
- **Runewords:** a new `fastCast` column in `RunewordDefinition` and `GenRunewords.ps1` (`fcr`), a line in the word's bonus list, and the totals. All 370 rows are otherwise byte-identical.
- **Loader:** `RederiveFastCast(Item&)` (items.cpp) replaces the inline block in `LoadItemData`. It sums the records, plus `UniqueItemFastCast(_iUid)` for a unique.
- **Tooltip:**
  - `PrintItemPower` gained `IPL_GOLDFIND`, `IPL_MAGICFIND`, `IPL_MOVESPEED(_CURSE)` and `IPL_FASTCAST`. A unique's gold-find line (Ashen Signet, Gutter Crown Seal) printed "Another ability (NW)" until now.
  - The drop-tail FCR line skips uniques, which print it on their own power line.
- `UniqueItems` is exported to the tests (`DVL_API_FOR_TEST`), like `UniqueItemCount` already was.

**Test:** `OracoolAudit.FasterCastRateReachesUniquesSetRungsAndRunewords` checks:
- exactly 20 uniques with a fixed value;
- `UniqueItemFastCast` and `RederiveFastCast` round-tripping a unique and a drop-tail record;
- four set rungs;
- the seven words' values reaching the totals.

Every golden stayed where it was (pack fixtures, writehero).

## The spell-animation GIFs

`tools/BuildSpellAnimationGifs.ps1` writes 52 looping GIFs, one per original missile graphic, into `Resources\00-original-game-art\spellanimations`, with a `README.md` index.
- **Source:** the sheets `oracool_art_export` already wrote to `00-original-game-art\missiles`, so the colours are the game's.
- **Names:** from the game's own tables. `Source\spelldat.cpp` gives spell → missiles; `Source\misdat.cpp` gives missile → graphic, and graphic → frame width, directions, frame count and frame delay. Examples:
  - `Spell - Firebolt, Fireball, Immolation (Fireball, fireba).gif`
  - `Monster - DoomSerpents (doom).gif`
  - `Effect - BigExplosion (bigexp).gif`: an effect spawned in code, not on a spell's row.
- **Layout:** a graphic with several directions shows all of them in a grid; a single-direction one is drawn at 2x.
- **Timing and background:** the game's timing, 20 ticks a second times the graphic's frame delay, on a dark ground.
- **Encoder:** a small C# GIF89a writer (LZW) compiled by the script, because the machine has no ImageMagick, ffmpeg or Python.

Oracool's own PNG missile sheets (Ice Bolt, Blessed Hammer and so on) are not in the folder. It holds original art only, as the user asked. The tool carries no art: it reads the extracted frames and writes beside them, outside the repository.

## Also

A build was blocked by a leftover `DiabloOrcl.exe` holding the Debug exe (LNK1168). The user said: "it is a ghost proces. a lefover from a game. close it everytime you encounter it." It was closed, and that is now a standing rule.

## Verification

Debug and Release built, ctest **708/708**, RTM refreshed with exe 1.11.042. **Not seen in play.**

**To check:**
- A cast-rate unique's tooltip: e.g. "Rain over Blackstone", with "+30% faster cast rate" on its own line.
- A Starless Hour 4-piece or Spirit shield raising the stat sheet's faster cast rate.
