# Imbuement Shards: the Mystic Orbs replaced, S2 to S8 in one run (v1.12.044)

**Date:** 2026-09-19 - Debug only - **819 of 819 tests** - built on the second machine (MSVC 14.51).
Plan: `01-Project-Overview/Plan - Imbuement Shards.md`; the ten decisions and the phase states on the
Imbuement Shards artifact (https://claude.ai/artifact/WntDqFo7C888ykoK9S7gS5). The user's word: "go, start s2 with
the sweep armed" and, on the build, "commit it and update the roadmap card and the imbuement shards artefact".

## What a shard is, in the code

`oracool/imbuement.h` - the Mystic Orb module (`mystic_orbs.*`, `GenMysticOrbs.ps1` and its seven `.inc`s) is gone.
Twenty-four kinds (`ShardKind`, the generator's order = save format, append only) out of `tools/GenImbuementShards.ps1`:
the eight orb item indices RE-LABELLED as Strength, Dexterity, Magic, Vitality, Warding, Fury, Fortune, Avarice (an item
index is positional save format, so the orbs' places are kept), and sixteen new kinds appended after the Necromancer's
bases - Blood, Spirit, Keenness, Precision, Flame, Spark, Bulwark, Stone, Ember, Storm, Veil, Radiance, Arcana,
Refinement, Tempering, Ease. `IsOracoolShardIdx` is therefore two ranges; the drop walk goes through `ForEachShardItem`.

**The ledger.** `Item::_iOracoolImbueCount` and `_iOracoolImbuements[20]` (item format 10 -> 11, the orb count byte
replaced; `OracoolItemExtensionSaveSize` grew by twenty). Which shards, not how many. The effect is computed from the
ledger at sheet time by the new `imbuements` BonusProvider (`stat_sheet.cpp`, `ApplyImbuementsToTotals`); nothing is
written into the item's stat fields - the twenty-Strength test pins `_iPLStr` untouched and the sheet at exactly +20.

- Refinement (D5): +3% per shard of the item's OWN affix totals, computed by `AddItem` on the item alone and scaled
  (rounded away from zero); base damage and armour, flags and spells untouched; gems, sets, runewords come through their
  own providers.
- Ease (D4): three points off each of Str/Mag/Dex per shard through `EffectiveRequirement` (gems.cpp) - the Hel seam -
  and it may reach zero; Hel's own percentage applies after, still floored at 1.
- Tempering: +10 maximum durability, an item field, moved on apply and on restore; declines an indestructible item.
- Limits (D3): Refinement, Tempering and Stone ten; Radiance five; Arcana three; Ease until every requirement is zero;
  everything else the cap of twenty (D2). Life and mana ride in 64ths like IPL_LIFE/IPL_MANA.

**Applying** (D8): the inventory drop site (`inv.cpp`) calls `TryImbue`; the tooltip shows "Imbued: n / 20" and the
breakdown ("Strength x3, Refinement x2"); the shard item shows its own line. The event log records the breakdown.
The milestone `FillOrbCap` keeps its saved bit and reads "Imbue an item to its limit" at twenty.

**Crafting** (D9): `IsTierRecipeGear` no longer refuses an imbued item and the second refusal in `GridMaterialsFor`
is gone. `TransmuteLevskiGridWith` captures the ledger before the transform and restores it after every rebuilding
recipe (Reforge, Ennoble, Recast, the tier ladder, the rerolls) - unconditionally, because every one of them
re-derives durability from the base whether or not the ledger bytes survive the pass, so Tempering's ten go back on
exactly once. Recipe 18, **Cleanse Shards** (`CraftingRecipeCount` 19): one imbued item in, the same item with an
empty ledger out, nothing returned.

**Drops** (D10): the treasure classes' `orbWeight` became `shardWeight`, `SocketableFamily::Orb` became `Shard`; the
qlvl column of each row is the band - shallow kinds from rung 1, the stats and striking kinds from rung 9, Stone,
Arcana and Refinement from rung 17.

## Art - RfA-18, batch 41, delivered the same hour

The brief (`Resources/ChatGPT RfA/RfA-18 - Imbuement Shards.md`) went out with a standing instruction for ChatGPT to
poll and deliver without a go-ahead; the package landed complete within the hour and was audited against the brief:
24 icons exactly 28x28 (installed under `Resources/01-in-use-assets/items/shards`, the generator's real-art fallback
now finds 24 of 24), `shardflip.png` - 13 frames of 96x160 matching the orb strip - installed in the orb tumble's
slot (`OracoolShardDropAnim`, `orbflip.png` removed), and `imbue.wav` (16-bit mono 22050 Hz, 0.78 s) in the orb
sound's slot (`UiEventSound::ShardImbue`, `orb-absorb.wav` removed). The icon sheet was re-cut with the real art.
The pack moved to `01-in-use-assets/delivered-packs/batch-41-imbuement-shards`.

## Found on the way

- `tools/build_item_icons.cmd` needs every generator's temp-folder stand-in art on a fresh machine (344 specs missing
  until the seven generators ran). `tools/GenUniqueItems.ps1` does NOT know the six hand-added Necromancer uniques and
  deletes their rows on a run; restored with `git checkout` and recorded in memory. Not fixed here.
- The audit test's "an orbed item is refused" fixtures became "an imbued item is rebuilt WITH its ledger" - Reforge,
  Recast and the Cleanse recipe are pinned, Tempering's durability measured by stripping a copy.
- The per-item save record's size constant had to grow with the ledger; the buffer guard caught it on the first
  test run (every hero save failed until then).
- gtest discovery: a full rebuild had twelve test binaries time out listing their tests (5 s) behind a saturated
  linker, each listing in 0.07 s alone; `DISCOVERY_TIMEOUT 30` in test/CMakeLists.txt.

## Verification

- 819 of 819, including the shuffled whole-binary lane. The writehero golden did not move.
- Exe reports 1.12.044; oracool.mpq repacked with the new sheet, tumble and sound.
- Not verified: nothing seen in play. S9 is the user's.

## Not done / left alone

- The two Mystic Orb Roadmap cards: "Keep Mystic Orbs through a rebuild" is closed by construction and marked so;
  "An item record that reads the previous version" still stands on its own merits.
- The old machine's Debug tree still carries the orb archive until it rebuilds.

## Related

- [[Plan - Imbuement Shards]] - [[2026-09-18-second-machine-first-build-v1.12.042]]
