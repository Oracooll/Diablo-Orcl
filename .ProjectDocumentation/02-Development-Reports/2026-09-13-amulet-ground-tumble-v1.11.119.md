# Batch 32 enters the game: amulets tumble as amulets

2026-09-13 — v1.11.119

## Why

The RfA-14 ground audit found the amulet to be the one item shape with no tumble of its own: vanilla's amulets
dropped as the ring, and the fork's 26 unique and set amulets as leather armour (v1.11.118 moved those to the
ring as a stand-in). RfA-14 requested one sheet; the user asked for a 5-minute watch on Resources, and the
batch arrived at 15:38.

## The delivery — checked before use

`Resources/01-in-use-assets/delivered-packs/batch-32-amulet-tumble/`:

| Check | Result |
|---|---|
| Canvas | 1248 × 160, 13 frames of 96 × 160 |
| Alpha | only 0 or 255 — zero partial pixels (scanned) |
| Colour | zero green-, purple- or cyan-dominant pixels (scanned) |
| Frames populated | all 13 (150 … 648 opaque px each) |
| At rest (frame 13) | dark-crimson stone in a brass setting, open S-curved chain to the left; 52 × 23 px footprint |
| Tellable from neighbours | viewed against `relicflip`, `charmflip`, `orbflip`, `signetflip`, `jewelflip` — distinct |

The artist's notes call two things out honestly: no in-engine playback was done, and the final three frames hold
the same rest pose rather than a micro-wobble. Both are fine for a drop; the playback is the user's look.

## Integration

- `amuletflip.png` to `Resources/01-in-use-assets/items/` and `Packaging/resources/oracool_assets/items/`.
- `ITEMTYPES` 63 → 64; `OracoolAmuletDropAnim = 63`; the count assert raised to twenty-one fork tumbles.
- `"amuletflip"` appended to `ItemDropNames`, `13` to `ItemAnimLs`, `IS_IRING` / `IS_FRING` to the pickup and
  drop sound tables — a pendant lands like jewellery.
- `GetItemDropAnimIndexFor`: **every** `ItemType::Amulet` goes to the new sheet, ahead of the cursor lookup — the
  plain bases and the classic uniques included, not only the fork's. The Relic and Reliquary bases are
  amulets too, but the UITYPE switch above already returns `relicflip` for them. The ring stand-in is gone.
- The sheet is packed into `oracool.mpq` by the normal build (`oracool_mpq_pack`).

**Save safety:** 13 frames, like the ring it replaces, so an amulet already lying on a floor keeps a valid
frame; `RepairFloorItemAnimation` would settle it regardless.

## Tests

`OracoolAudit.NoDroppableItemTumblesAsLeatherUnlessItIsLightArmour` now also requires every droppable amulet
to land on `amuletflip` (or `relicflip` for the relic bases), and counts them.

## For the user to look at

Drop an amulet: it tumbles as a pendant with its chain trailing, and lies as a crimson stone with an open chain.
