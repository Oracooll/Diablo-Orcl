# 2026-09-27 - Dev notes batch, Barbarian sounds out (v1.12.208)

**Date:** 2026-09-27. Debug only. The user said "check my dev notes and process". There were nine notes; eight are done and one is open pending details. Mid-turn the user also asked:
- "remove all barb sounds introduces by chatgpt. they are not good enough to be in the game";
- "extract all other heroes classes sounds delivered by chatgpt. i need to check them".

Each note's outcome is in `development-archive.md` under "Batch of 2026-09-27 (fourth)". The engineering follows.

## The immortal monsters

- **The cause:** War Cry (`warcries.cpp`) runs `Strike` then `Stagger` on every monster in earshot. When the strike killed, `M_StartKill` set `MonsterMode::Death`, and `Stagger` then called `StunMonster`, whose `AiDelay` set `MonsterMode::Delay` over it. The death animation never ran and the monster sat at 0 life, so no targeting found it. When the delay ran out, its AI resumed.
- **Why it was War Cry and not the others:** `rfa12_actives.cpp`'s own `Stagger` already refused a monster at 0 life; the war-cry file's copy did not.
- **The fix:** in `StunMonster` (`monster.cpp`), a dead or dying monster is left alone. That one function sits under every skill that stuns, so all of them are covered. No other fork code writes a monster's mode directly.

## Advanced Stats beside the inventory

- `OpenAdvancedStats` no longer clears `invflag` and `sbookflag`, and `IsAdvancedStatsOpen` no longer closes on them. It still closes with the sheet.
- `scrollrt.cpp` draws it after the inventory or Abilities window.
- `diablo.cpp`'s left-click and `cursor.cpp`'s hover test its rectangle before the inventory's, so the overlapping 60px belongs to it.

## Other changes

- **Rune recipes:** the tooltip block in `items.cpp` is gone, along with `RuneTeachingLines`, `HostName` and their test lines.
- **Split box:** `PlaceGoldDropBox` (`control.cpp`) anchors the box beside the pointer when either opener runs, clamped on screen. `DrawGoldSplit` draws from that anchor, and the three fixed text-input rects in `inv.cpp` are gone.
- **Backhand:**
  - joins `IsMeleeSpell`;
  - `ApplyRfa12MeleeOnSwing` strikes the tile behind at `BlowPercent`;
  - `SwingArt` draws `BackhandArc`;
  - the cast branch keeps Rearward Reach only.
- **Right-click cancel:** `RightMouseDown` calls `DisarmShopServiceCursor` or `CancelSalvageItemCursor` before any shop handling.
- **Repair and recharge prices:** `ShopRepairPriceFor` and `ShopRechargePriceFor` (`stores.cpp`) wrap the file-local price functions and answer only while their cursor is live. `inv.cpp` prints the line under "Sells for".
- **Vendor hint cards:**
  - `ShowPanelStringsAsHintCard` (`cursor_tooltip.cpp`) records the panel text.
  - `DrawCursorTooltip` draws it through `DrawHintCard` while the text is unchanged. It uses the item card's `MeasureCard` and `DrawCard`, hue 0xD4A23C (the unique's), with the body word-wrapped to 226px, so the card is at most 250px.
  - `SetShopHoverInfoString` asks for it after the last line.
- **Axe note (open):** `OracoolPreview.DISABLED_BarbarianAxeSwing` renders the Warrior's axe and sword attack sheets per tier. The axe sheets are correct.

## The Barbarian's sounds, out

- **Removed:** all 58 cues (19 war cries, 29 combat skills, 10 masteries) are out of `skill_sounds_data.inc`, now 471 sounds across 6 classes, and their files are deleted from `Packaging/resources/oracool_assets/sfx/skills/barbarian`.
- **Generator:** `tools/GenSkillSounds.ps1` skips the class, so a re-run cannot bring them back.
- **What plays now:** his skills fall back to what plays without a cue.
- **Review copies:** in `Resources\Barbarian Sound Assets`, byte-identical to what shipped.
- **Left behind:** the three emptied folders. The removal was blocked by a safety check; they hold no files and nothing packs them.

## The other classes' sounds, for review

All 471 remaining cues are copied to `Resources\<Class> Sound Assets\<page>\<Skill> - <event>.wav`, named from the skill tree by enum order. The table's rows are declared in enum order, so a renamed row reads by its current name.

| Class | Files |
|---|---|
| Paladin | 138 |
| Sorcerer | 97 |
| Rogue | 79 |
| Monk | 70 |
| Bard | 38 |
| Necromancer | 49 |

Every copy is a valid WAV and byte-identical to the shipped set.

## Tests

v1.12.208 builds clean; 877 of 877 pass. New tests: `AStunNeverRaisesTheDead`, `AdvancedStatsLeavesTheInventoryOpen`, `BackhandIsASwing`, `TheRepairPriceShowsOnlyUnderTheHammer`. oracool.mpq repacked.
