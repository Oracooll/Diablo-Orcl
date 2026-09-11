# RfA-04 applied: base icons, glyphs, event sounds, drop tumbles (v1.11.036)

**Date:** 2026-09-11
**Branch:** renderer-32bit, local commits only
**Request:** `Resources\ChatGPT RfA\RfA-04 - Base Icons, Glyphs, Event Sounds, Tumbles.md`, batches 12-15, under the Gold asset loop

All four batches arrived together, and every file passed its checks:
- the icons, tumbles and glyphs at exact size, with alpha only 0 or 255;
- the glyphs containing only the two glyph colours (0 off-colour pixels);
- the WAVs mono 22050 Hz 16-bit and inside their duration ranges.

The art was reviewed on two contact sheets plus the tumble previews. An agent took the tumbles (items.cpp and items.h only), and I took the rest.

## Batch 12: the 18 base icons

The unique-expansion bases wore their first unique's sprite. Each now has its own plain icon:
- **Frames:** the 18 frames were built alone (ItemIconCel, asis) and **appended** to `oracool_items.cel` (608 to 626). Every existing frame is byte-identical.
- **IDs and wiring:** `ICURS_ORACOOL_UNQBASE_*` in `unqbase_icons_curs*.inc` sit after the late unique run. The 18 base rows point at them.
- **Rebuild source:** the art is filed under `02-source-art\items\unqbase` and the spec is added to `build_item_icons.cmd`.
- **Uniques unaffected:** the uniques keep their own icons through IPL_INVCURS.

## Batch 13: five skill glyphs

- `BuildGlyphStrips.ps1` now reads later glyph packs (`$extraPacks`). A key the first pack already has is an error, not an override.
- The rebuilt strips stamp **Rogue 49/49, Bard 39/39, Monk 39/39** (Valkyrie, Sonic Barrier, Charm, Inner Sight, Spirit Ward).
- The Sorceress stays at 35/48. Her 13 book spells were excluded from the glyph brief on purpose.
- Only the three affected strips changed.

## Batch 14: four event sounds

`UiEventSound` gains Milestone, EncounterCleared, MapUnseal and SignetUse:
- **Milestone:** played by `ClaimMilestone`.
- **Encounter cleared:** played as the guardian's reward drops.
- **Map unseal:** played once the map actually opens (not on a refusal).
- **Signet use:** replaces the generic item-use sound, with the old one kept as the fallback.

**The first runeword completion also claims a milestone**, and the milestone's sound then stands in for the runeword chime, so one click never makes two sounds.

## Batch 15: three drop tumbles

`jewelflip`, `salvageflip` and `mapflip` are drop animations 48-50 (`ITEMTYPES` 51), mapped by range with contiguity asserts:
- jewels, landing and pick-up like the gems;
- salvage materials, like cloth;
- sealed maps, like the vanilla scroll.

The reward charms stay on charmflip. Visual and sound only, and the frame count stays 13, so saves are unchanged.

## Verification

Debug and Release built. ctest **701/701 with no expected value changed**. Both trees repacked, RTM refreshed with exe 1.11.036, all four packages filed under `02-source-art\delivered-packs`. Line endings audited against HEAD on every touched file (cursor.cpp is mixed and unchanged in its CRLF lines).

Not seen or heard in play. Things to check:
1. A plain Spear, Relic or cloak on the floor and in the backpack.
2. The Rogue, Bard and Monk trees' five new glyphs.
3. A milestone, a cleared encounter, a map opening and a signet used.
4. A jewel, a salvage material and a sealed map dropped.

## Open

- **Two sounds at once:** a milestone claimed by the same action as the orb-absorb sound (filling an item to its orb cap) plays both.
- **Orphan manifest entry:** the glyph pack still lists the removed Paladin Holy Bolt, which the builder reports and skips.
