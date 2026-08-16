# The Uniques Get Their Faces

**Version:** 1.7.62
**Date:** 2026-08-17
**Files:** `tools/GenUniqueItems.ps1`, `tools/build_item_icons.cmd`, `Source/itemdat.h`, `Source/cursor.cpp`, the four generated `unique_items_curs*` artefacts, `test/pack_test.cpp`

---

## The sweep

One drop in the MPQ root: `unique-item-sprites-250.zip`, 190.6 MB, with a `.sha256` beside it — **checksum verified before extraction**. Inside: 250 native 28px-per-cell PNGs, a master-resolution set, chroma and RGBA source sheets, and `sprite-manifest.json` joining art to the design catalog by `id`. The package ships its own engine-handoff checklist, and it is a good one — "missing art is a hard build error, not a silent generic-icon fallback" is now literally what the generator does.

This is the second half of the promise made at 1.7.56: *"you can put them in the game now and later attach the sprites."* The 143 in-game uniques were built deliberately without `IPL_INVCURS` so that attaching art later would be a generator change, not an engine change. That bet paid off — **no engine code path changed today except two declarations.**

## What the generator grew

`GenUniqueItems.ps1` now emits the same four icon artefacts as the item-set generator, in one walk and one order:

1. the CEL spec list (asis mode, real transparency),
2. `unique_items_curs.inc` — `ICURS_ORACOOL_UNQ_*` ids **506..648**, bracketed by `FIRST`/`LAST` aliases,
3. the `InvItemWidth3` rows,
4. the `InvItemHeight3` rows.

`cursor.cpp` static_asserts that the unique run starts exactly one past the set run's end. The CEL is a concatenation of the two spec lists; a gap or overlap would shift every unique icon while nothing complained — the same silent failure mode the set icons guard against, guarded the same way.

Each unique reaches its frame through **`IPL_INVCURS`**, vanilla's own icon channel, appended as the last power. Two things ride along free:

- The inventory **footprint** follows the package's declared grid, because everything that sizes an item reads `InvItemWidth3/Height3` through `_iCurs`.
- The description path already tolerates it — `PrintItemPower(IPL_INVCURS)` returns a lone space, which is how vanilla's own INVCURS-carrying uniques have always kept their tooltips clean.

## The one struct change

`UniqueItem::powers[6]` → `powers[7]`. Twenty-six of the emitted uniques carry six live affixes, and the icon needs a slot of its own. Vanilla's rows aggregate-initialise six entries, so their seventh is zero (`IPL_INVALID`) and none of their behaviour moves.

## The acceptance gates

The handoff asks for 250-IDs-to-250-frames. What holds here:

- Every **emitted** unique resolves its PNG or the generator hard-errors. No fallback icon exists for any of the 143.
- No duplicate frames; ids and frames are one contiguous run by construction.
- The **107 not emitted** are the same 107 as before — bases this engine has no `UITYPE_` for (66 of them on armour slots the fork already has but never gave `UITYPE_` values; 41 on relic/cloak/spear-family bases that do not exist). Their art is extracted, indexed, and filed for the day their slots do.

## Fallout, contained

Nine `pack_test` golden rows: the expansion uniques among them now recreate wearing their own icons, so their pinned `_iCurs` moved from the base item's frame to their `ICURS_ORACOOL_UNQ_*` one. Verified field-by-field that **only `_iCurs` changed** — nine values patched by item name, not by cursor value, since vanilla rings legitimately share the old numbers.

## Verification

- Generator: 143/143 sprites resolved, 0 missing, ids 506..648.
- CEL rebuilt to 649 frames; MPQ repacked; loose-assets channel updated.
- Debug build clean at 1.7.62; full suite **441/443**, the standing baseline pair.

Worth looking at in play: spawn a set with `giveuset`-style debug drops or hunt one — the uniques now look like themselves in inventory, on the cursor, and on the ground label, and multi-cell items (2×3 armours, 1×3 pikes) occupy the grid their art declares.
