# Fork items tumble to the floor as their own shape, not as leather

2026-09-13 — v1.11.118

## Why

The RfA-14 ground audit (see `Resources/ChatGPT RfA/RfA-14 - Amulet Ground Tumble.md`) built all 905
floor-reachable items — every base, all 250 uniques, all 94 set pieces — and recorded the tumble each lands
with. **228** landed on `larmor`, vanilla's leather-armour tumble, without being light armour:

| Kind | Items on `larmor` |
|---|---|
| Fork tier bases (Iron → Spectral) | 15 helms, 15 medium armours, 16 shields |
| Set pieces | 14 helms, 9 shields, 11 amulets, 8 rings, 4 maces, 3 swords, 1 bow |
| Fork uniques | 20 helms, 15 shields, 15 amulets, 20 rings, 15 swords, 9 axes, 9 maces, 9 bows, 9 staves, 7 heavy armours, 4 medium armours |

User: "do the mapping fix for the 202 items".

## Cause

Each of those items carries its own Oracool icon id (≥ `ICURS_ORACOOL_FIRST`). `GetItemDropAnimIndex` maps
cursor ids, cannot place these, and returns its fallback, `larmor`. `GetItemDropAnimIndexFor` catches only
the six worn slots and six exotic bases before falling through. RfA-08 had promised to point the rest at
vanilla's matching tumbles; the mapping was never written.

## Fix

`GetItemDropAnimIndexFor` now re-keys **only that fallback** on the item's type:

| Type | Tumble |
|---|---|
| Sword | `swrdflip` |
| Axe | `axe` |
| Mace | `mace` |
| Bow | `bow` |
| Staff | `staff` |
| Helm | `helmut` |
| Medium armour | `armor2` |
| Heavy armour | `fplatear` |
| Shield | `shield` |
| Ring, **Amulet** | `ring` |

- An item whose cursor id the table *can* place (vanilla icons, the eight socketable families) is unchanged.
- Light armour keeps `larmor`, which is its shape.
- **Amulets** ride the ring — what vanilla's own amulets have always tumbled as — until RfA-14's
  `amuletflip` arrives. That is the 26 beyond the 202.
- The vanilla sheets are named by their position in `ItemDropNames` as constants beside the function.

**Frame counts.** Every target is 13 frames except `armor2` (15). A saved medium armour already on a floor
stores 13; `RepairFloorItemAnimation` (v1.11.114) sees the mismatch on load and settles it on the new sheet's
last frame, so no migration is needed.

## Tests

`OracoolAudit.NoDroppableItemTumblesAsLeatherUnlessItIsLightArmour` — the audit's three walks kept as the
regression: every droppable base, every unique and every set piece that is not light armour must not land on
`larmor`.

## A separate question the user asked

> "if all items have animation what was the problem with set items and runes orbs and charms?"

That was a different fault (v1.11.114): those drop hooks placed the item but never STARTED its tumble, so it
had no sprites at all — invisible, unlabelled and unclickable until a level reload drew its mid-air frame.
The tumble art existed; the drop never asked for it. This change is about which sheet an item uses once it
does.

## For the user to look at

Drop or find a unique sword, a set helm and a fork tier shield: each tumbles and lies as a sword, a helm and a
shield. Amulets lie as a ring until the amulet sheet is delivered.
