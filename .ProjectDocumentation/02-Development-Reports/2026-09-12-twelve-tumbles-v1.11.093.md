# Twelve tumbles, keyed on the item rather than its icon

2026-09-12 — v1.11.093

## What this closes

Batches 21 and 22 were delivered hours ago and verified, then sat unfiled and unwired while five
sweeps reported them. They are in now, which empties the Resources root for the first time today and
leaves **no outstanding asset work of any kind**.

Before this, gloves, boots, bracers, belts, legs and shoulders all hit the floor as the same
flopping piece of vanilla `larmor`, and so did every cloak, relic, spear, lute, quiver and Canticle.

## The design decision: key on the ITEM, not its cursor id

The eight existing fork tumbles are mapped by `ICURS` range, which works because gems and runes are
contiguous blocks. That approach does **not** scale here, and the reason is the point of the change:
these twelve have to serve **250 uniques and 94 set pieces** as well as the bases, and every one of
those carries its own `IPL_INVCURS` icon id. An id list of that length is 344 chances to point an
item at the wrong sprite, and a CEL has no way to notice one — as Knellbranch demonstrated four
versions ago.

So `GetItemDropAnimIndexFor(const Item &)` asks what the item **is**, using two keys that both
survive the thing an id list cannot:

- **`_iLoc`** — the six worn slots. A Seraphic belt, an Iron belt and a unique belt all answer
  "belt" without any of them being named anywhere.
- **`AllItemsList[IDidx].iItemId`** — the base's UITYPE. A unique or set piece is a row built *on* a
  base, so it reports the base's shape, which is exactly what a tumble should follow.

Anything unrecognised falls through to the old cursor-id function, so every vanilla item and all
eight socketable families are untouched.

`ITEMTYPES` 51 → 63, with the twelve appended so no existing index moves. All twelve are **13
frames**, which is what keeps this save-safe: an item already lying on a floor stores its frame, and
every tumble in the game including `larmor` is 13, so re-pointing an item at a new sheet cannot land
it on a frame that does not exist.

## Four mistakes on the way, all caught by measuring

1. **A second sound table.** I filled `ItemInvSnds` and missed `ItemDropSnds` — the floor half. A
   `static_assert` caught it, which is what those asserts are there for.
2. **Infinite recursion, 64 tests SEGFAULT at once.** A bulk rewrite of the call sites from
   `GetItemDropAnimIndex(item._iCurs)` to `GetItemDropAnimIndexFor(item)` also rewrote the **two
   fallback calls inside the new function**, so it called itself. A stack overflow reads as dozens of
   unrelated SEGFAULTs from ctest; the give-away was that the failures spanned inventory, pack and
   audit tests with nothing in common.
3. **`UITYPE_CLOAK` compiled and matched nothing.** The real enums are `UITYPE_ORCLCLOAK` and
   `UITYPE_BATTLECLOAK`; `UITYPE_CLOAK` exists (it is Hellfire's) so the case was valid C++ and
   silently dead. The Travelling Cloak was still tumbling as `larmor` after the function was
   written. **The test caught this, not me** — which is the argument for the test asserting the rule
   rather than restating the table.
4. **A test written outside the namespace.** The first version landed after `} // namespace
   devilution`, so every `ILOC_*` was undeclared. Moved in and rewritten to use only exported API.

## The test

`OracoolDropTumbles.TheTumbleFollowsTheItemsShapeNotItsIcon` asserts the **rule**, not a list: six
worn slots each answer differently and all above the vanilla range; the pairs that share a shape
share a sheet (both cloaks, relic/reliquary, spear/pike, canticle/focus); all twelve are reachable,
since a sheet nothing maps to would ship and never draw; and — the case an id list could not express
— **a unique built on each base keeps the base's tumble with no unique named anywhere.**

**725/725 tests pass.** Archive 469 → **481 files**. Debug and Release clean; RTM refreshed.

## Also fixed: undefined behaviour I introduced an hour earlier

The sixth sweep caught it. `FrontEndPalette` was a **block-scope** `constexpr char[]` in
`LoadHeros()`, and the importer **caches that pointer** and `strcmp`s it on the next call — after the
function has returned. A dead stack read; the kind of bug that works until a sanitizer notices.

Fixed twice over, because `static` alone would leave the trap armed for the next caller: the cache
now stores a `std::string` **copy**, so no caller's lifetime can matter.

## Still open, from the sixth sweep

Reported rather than fixed, and worth doing in this order:

1. **Nothing asserts a strip's frame count against its class's row count.** `DrawStripIcon` clamps
   out-of-range silently, so a truncated strip draws a plate with no glyph and no error. That is
   precisely how my own 49→48 truncation got past a green suite an hour ago. Needs an exported
   accessor for the tree strips plus archive mounting, so it is real work rather than a one-liner.
2. **Four committed spec files bake machine-specific absolute paths** (`C:\Users\hroga\...`), and
   `build_item_icons.cmd` consumes them verbatim while only checking the *spec* exists, never the
   art. `oracool_items.cel` is not rebuildable on another machine, or after `%TEMP%` is cleared,
   without re-running the generators first.
3. **Three generated `.inc` headers name `02-source-art`**, a folder that no longer exists; the
   generators were updated on 2026-09-12 and never re-run.
4. `LevelPaletteMissing` is never assigned, so the intended "palette absent → degrade" path is
   unreachable and a missing `.pal` app-fatals instead.
5. `BuildGlyphStrips.ps1`'s unused-glyph audit reads only the first pack, so it is blind on the two
   extra packs — the exact failure that audit exists to catch. Its comment about "18 Sorceress rows
   drawing as before" is also now false.
6. Four delivered packs are byte-identical in both `01-` and `02-` tiers. Harmless today; a future
   tidy-up that treats the `02-` copy as the home reproduces the 09-11 glyph breakage verbatim.
7. `wiki/data.js` is stamped v1.11.081 against a v1.11.093 tree, and `BuildWiki.ps1` stamps
   `Get-Date`, so the wiki is never byte-reproducible and nothing guards source→wiki drift.
8. Seven more stale source comments naming `Oracool.MPQ/` or `02-source-art`.
