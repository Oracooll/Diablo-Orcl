# The Necromancer, phase N9: wands, scythes and shrunken heads (v1.12.039)

**Date:** 2026-09-18 - Debug only - **819 of 819 tests**. N4-N8 still await the user's look in play.

## The three families (`oracool/necro_items`, decisions D6, D9, D10)

Twenty-four bases appended to `AllItemsList` as `IDI_ORACOOL_NECRO_*`, three contiguous runs of eight with
FIRST/LAST marks, level 1 / 5 / 10 / 15 / 20 / 26 / 33 / 40, one unique type each (24 new `UITYPE`s, so a unique on one
can never be re-chosen for another):

| Family | Kind in the engine | Bases |
|---|---|---|
| Wands | one-handed `ItemType::Mace` (the mace sheets; the shield swap works on them), a Magic requirement | Bone, Grave, Yew, Tomb, Petrified, Grim, Lich, Unholy Wand |
| Scythes | two-handed `ItemType::Axe` (the axe sheets), a Strength requirement | Reaping, Bone, Grave, War, Dread, Harvest, Doom Scythe, Deathbringer |
| Shrunken heads | `ItemType::Shield`, `ICLASS_ARMOR`, in the shield hand, a Magic requirement | Preserved, Zombie, Fetish, Gargoyle, Demon, Unraveller, Overseer Head, Bloodlord Skull |

Six uniques, two a family on the top two bases: Arm of King Leoric, Blackhand Key; Reaper's Toll, Bonehew;
Homunculus, Darkforce Spawn (`unique_items_data.inc`, no `IPL_INVCURS` until their art exists).

**Icons and tumbles are vanilla stand-ins** - a wand looks like a mace, a scythe like an axe, a head like a small
shield - until RfA-17's new batch 40 (added to the draft: the 24 bases with icon-cell sizes).

## Where they come from

- **Not the seeded pool.** A first build seated them there as IDROP_REGULAR bases; the pack fixtures' items came
  back as other things (a book as a scythe) and twelve pack tests failed - exactly what the 2026-08-15 guard on
  that pool warns of, and what the 18 unique bases avoided on 2026-09-11 by staying out of the SHOP pools. So:
- **`TrySpawnNecroBase`** (items.cpp), beside the set and gem hooks in `MonsterDeath`: one kill in sixteen drops
  one, depth-gated by `BandedQlvl`, a unique rolling like on any base (uper 15 on a unique monster), through
  `FinishOracoolDrop` so it tumbles. Heads only while the hero is a Necromancer.
- **Griswold**: wands and scythes joined `OracoolGearBasesFor`, so the Basic, Magic and Rare tabs' Oracool passes
  offer them to anyone. **Adria**: two shrunken heads through `StockOracoolFixedItems`, for a Necromancer only.

## His alone (D10)

`ClassMayUseItem` in `Player::CanUseItem`: a shrunken head is red in any hand but a Necromancer's, and neither
drops nor is sold for another hero. Wands and scythes are anyone's.

## The rest

- A held head is the LIGHT shield's look, whatever it looks like (`GearLookFor`, D11's first half; the decal is later).
- **Swift Harvesting** is built: a wand or scythe in hand, `FastAttack` on the hero's item flags (`CalcPlrInv`).
- `IDI_LAST` moved; nothing that stores an index moved with it (the hero hash did not change).

## For the user's look in play

Kill things until a wand or scythe drops (about one kill in sixteen); Griswold's Oracool shelves; as a Necromancer,
Adria's heads; wear a head (light shield in the swap), a wand (mace swing), a scythe (axe swing); as another class,
pick up a head and see it red. Slot Swift Harvesting with a wand. A screenshot is the only verification.
