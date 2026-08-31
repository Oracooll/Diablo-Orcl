---
title: 2026-08-12 - The Iron Helm, a New Item at an Old Slot
date: 2026-08-12
tags: [dev-report]
summary: A stray root folder of ChatGPT exports got sorted per the art vault's own naming convention, and the one helmet render among them shipped as a genuinely new item - "Iron Helm" at ILOC_HELM. Unlike the six worn slots added earlier this session, ILOC_HELM already existed in vanilla Diablo, so this was table-row-and-icon work, not new-equip-location plumbing. A follow-up question from the user ("are you sure about this command") caught that the debug command claimed as the way to reach it didn't actually work - it needed its own fix.
---

# The Iron Helm, a New Item at an Old Slot

Two requests back to back: sort a folder of raw ChatGPT exports sitting in the art vault's root, and turn the one armor render among them into a real, shipped item.

## Sorting first

Six files sat directly in `Oracool.MPQ/`, all with the raw `ChatGPT Image DD.MM.YYYY, HH_MM_SS.png` export name the vault's own README explicitly calls out as the thing that "sorted badly and said nothing about what was inside." Only one was actually the armor piece asked for. The other five were a separate thread entirely - 16 alternate designs for the existing level-up notification icon, two near-identical passes at a panel border/trim sheet, and two passes (24-cell, then a 30-cell superset) of inventory slot-background texture exploration. All five got renamed to describe their contents and filed into the numbered folders the vault's README already documents (`02-source-art/`, `03-concepts/`, `03-concepts/superseded/` for the ones an later pass superseded) - no new convention invented, just the existing one actually applied. Worth flagging for later: `02-source-art/items/` already has unused `item-icons-shields-v1/v2.png` and `item-icons-body-armour.png` sitting in it, so two of the three vanilla-slot pieces from the earlier 9-piece art prompt may already have source art waiting on a similar pass.

## The helmet: cut, then a false alarm

The single-render helmet (a riveted iron/steel open-face helm, product-shot style with a soft dark vignette rather than the six worn-item sheets' flat black canvas) went through `tools/ItemIconCel.cs` unmodified at first, using the same background-flood-fill logic that sheet has used all session. The vignette looked, on paper, like exactly the kind of thing that pipeline wasn't built for - a gradual glow ramp rather than a hard black-to-content edge - and a horizontal luma scan confirmed the ramp crosses the fill's cutoff well before reaching the helmet's own silhouette.

Tested the fix before trusting the theory: added an optional per-spec backdrop-cutoff override to the tool (default unchanged, so the six existing call sites are untouched) and re-cut at a raised threshold. Side-by-side at 8x, the raised cut wasn't cleaner - it ate into the helmet's own dangling cheek-strap, which sits in the same brightness band as the vignette glow. The *default* cutoff, unchanged, turned out to already produce a clean silhouette with no visible halo; the vignette's spatial falloff was tighter around the actual subject than the single scanline sample suggested. Measured, didn't guess, and the measurement said the fix wasn't needed - which is its own useful outcome, and cheaper to find out before shipping a worse cut than after.

The backdrop-cutoff override stayed in the tool anyway (zero risk to the existing six, and the next single-render "product shot" style icon will likely need it even if this one didn't).

## Wiring it in: much lighter than the six worn slots

The six items added earlier this session each needed a brand new `ILOC_*`/`SLOTXY_*` pair, `NUM_INVLOC` grown from 7 to 13, and hand-written slot chains in `inv.cpp` updated in multiple places - all because those equip locations didn't exist before. `ILOC_HELM` already exists in vanilla Diablo (Cap, Skull Cap, Helm, Full Helm and Great Helm all share it), so none of that applied here. This was table-row work:

- `itemdat.h`: `IDI_ORACOOL_HELM` appended after the six (index 174, `IDI_LAST` moved to match), `ICURS_ORACOOL_HELM = 235` appended to the cursor-graphic enum.
- `itemdat.cpp`: one new `AllItemsList` row - `IDROP_NEVER` (same reason as the six: joining the loot tables perturbs `pack_test`'s RNG-seeded golden items, a deliberate separate follow-up, not something to fold in here), stats matched to vanilla "Helm" since that's the tier the art actually reads as.
- `cursor.cpp`: `InvItemWidth3`/`InvItemHeight3` grew a seventh entry (56x56). The frame-count static_asserts caught nothing, which is itself the confirmation the arithmetic still lines up.
- `loadsave.cpp`: `IsOracoolItemIdx`'s range extended by one - the exact guard added earlier this session after the six worn items were discovered to be getting destroyed by the Diablo save remap. Same exposure, same fix, and skipping it here would have reintroduced that bug for this one item specifically.
- `tools/build_item_icons.cmd`: a seventh spec line, source rect the full canvas (a single render, not a multi-item sheet needing a sub-crop).

Everything downstream of `iLoc` - pack/sync/msg validation, right-click equip, hover highlighting, the paperdoll slot itself - already handles `ILOC_HELM` generically, because it's handled the same five vanilla items since before this project touched it. Confirmed by grep rather than assumed: `pack.cpp`'s slot validator already has an `INVLOC_HEAD` case, and `IsOracoolEquipLocation` (which gates the `give*set` debug commands' willingness to surface `IDROP_NEVER` items) is explicitly scoped to the six worn locations only - `ILOC_HELM` correctly falls outside it, so those commands keep surfacing vanilla Cap for the helm slot as before.

## `drop Iron Helm` didn't actually work - a claim caught, not assumed

Said `drop Iron Helm` was the way to reach the item in-game, in this same report, on first pass. It wasn't - the user asked "are you sure about this command," which was the right question. Traced it rather than re-asserting: `drop` (`DebugSpawnItem`) doesn't look items up by name at all. It repeatedly rolls a random item via `RndItemForMonsterLevel`, and only keeps the roll if the *generated* name happens to contain the query - a loot simulator wearing a name filter, not a lookup. That RNG path runs through `GetItemIndexForDroppableItem`, which unconditionally skips every `IDROP_NEVER` entry (`items.cpp:1564`). Iron Helm is `IDROP_NEVER` by design, so `drop Iron Helm` could never terminate on a match - it would burn the full 3-second/1,000,000-try budget and report "not found," every time.

The six worn items dodge this because `givebset` and friends go through a *different* function (`FirstBaseItemForEquipLocation`) with its own, separate `IDROP_NEVER` bypass - but that bypass is scoped to the six new `ILOC_*` locations specifically, and doesn't (and structurally can't just be extended to) help `ILOC_HELM`: that function returns the *first* item matching a location, and four vanilla Helm-tier items outrank Iron Helm in table order regardless of any drop-rate bypass. Iron Helm had no reachable path through any existing debug command.

Fixed in `DebugSpawnItem` itself: a small direct-match check against just the seven `IDI_ORACOOL_*` indices, spawning immediately on a name hit via the same plain-item construction `givebset`'s basic tier already uses (`GetItemAttrs`+`SetupItem`). Deliberately wired as a *fallback*, tried only after the existing random search has already given up - not first. Several of these names genuinely overlap vanilla ones (`"helm"` is a substring of both "Iron Helm" and vanilla "Helm"/"Full Helm"/"Great Helm"); checking first would have made `drop helm` silently start returning Iron Helm instead of the vanilla items it already correctly finds. Running only after the existing search exhausts itself preserves every previously-working query exactly as it behaved before, and only adds a path for queries - like this one - that could never have succeeded anyway. `IsOracoolItemIdx` (the same range loadsave.cpp's save-remap guard uses) moved from a loadsave.cpp-local `constexpr` into `itemdat.h` so both files reference one definition instead of two hand-copied ranges.

## A naming-convention catch

First pass set both the long and short display name to `"Iron Helm"`. Vanilla's own rows use the short name (`iSName`) as a *category* label, not a repeat of the flavor name - `"Full Helm"`/`"Helm"`, `"Skull Cap"`/`"Cap"` - because `iSName` is what magic/rare name generation actually builds affixed names from (`GenerateMagicItemName`, `items.cpp:1314`). Left as `"Iron Helm"` for both, a magic roll would have produced something like "Holy Iron Helm" instead of the correct "Holy Helm". Fixed to `iSName = "Helm"` before shipping, matching the five vanilla rows at the same slot and the pattern the six worn items already followed (`"Leather Belt"`/`"Belt"`, etc.).

## Test coverage

Extended `PackTest.PackItem_diablo_roundtrip_preserves_oracool_worn_items` (added for the six worn items' save-destroying bug) to include `IDI_ORACOOL_HELM` in its loop rather than writing a parallel test - same exposure, same guard, so the existing coverage picks it up for free. 59/59 PackTest+NetPackTest cases pass.

## Verification

Closed-loop MPQ check repeated from the last two units of work: extracted `data\inv\oracool_items.cel` back out of the freshly-packed `oracool.mpq` and hash-matched it against the source (`48e0fcd9...`, match). Full suite stayed at **349/351** (the same two pre-existing failures) across both builds - `ORACOOL_VERSION` **1.1.23** for the item itself, **1.1.24** for the `drop` fix, both confirmed embedded in their respective built exes. The shipped icon was also hash-verified against a fresh preview render pulled from the same build that produced the packed MPQ, not a separate/potentially-stale one.

Still not verified: actually equipping the item and looking at it on the paperdoll or character panel in a running session - that needs an interactive pass this tooling can't drive itself. `drop Iron Helm` is now confirmed to actually work as that path, rather than just asserted to.

## Related

- [[2026-08-12 - Pulling the Disliked Armor Icons Pending a Redo]]
- [[2026-08-12 - Six New Equipment Slots]]
- [[2026-08-12 - Fixing Torn Edges and Floating Debris in Item Icons]]
