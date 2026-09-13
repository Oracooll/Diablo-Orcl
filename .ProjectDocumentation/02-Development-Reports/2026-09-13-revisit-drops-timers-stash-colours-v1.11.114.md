# Drops that only appeared on a revisit; the spell countdown column; stash tabs; consumable colours

2026-09-13 — v1.11.114

Four requests from one afternoon, built together.

## 1. Set items, charms and orbs that "only drop on the second visit"

### What the user saw

> "i never encounter drop of set items but many times if i revisit area i have cleared of mobs i find a
> number of set items as if they appear only on my second visit"
>
> "when i revisit an area is when the problem with the size of drop items assets appears - they appear
> unproportionaly big and away from their label" — charms, orbs, set items.

### Cause

Three of the fork's drop hooks built their item without `SetupAllItems`:

- `TrySpawnNamedSetPiece` (the fifteen named sets): `InitializeItem` → `MakeSetItem` → `FinalizeSetPiece`
- `TrySpawnOracoolGem` (gems, runes, **charms**, jewels, **Mystic Orbs**): `InitializeItem` → seed
- `TrySpawnSignet`: `InitializeItem` → seed

`SetupAllItems` ends in `SetupItem`, and `SetupItem` is where a dropped item gets its tumble
(`setNewAnimation`). These three placed the item with `GetSuperItemSpace` and stopped, so the item had
**no sprites**:

- `DrawItem` returns before drawing an item without sprites — invisible;
- the label is queued from `DrawItem`, so no label;
- `_iSelFlag` stayed 0 — not clickable.

The drop was real and on the floor the whole time. On a revisit the level is loaded from its save,
and `LoadItem` → `GetItemFrm` hands the item its sprites — but the saved frame count is 0 and the saved
frame is 0. Frame 0 is the **first frame of the tumble**, the item mid-air: larger and higher than it
lands. `isLastFrame()` is true with no frames, so the label queued at the landed position while the
sprite drew the mid-air one: "unproportionally big and away from their label".

A second, quieter fault made labels worse: `AddItemToLabelQueue` cached each label's horizontal offset
keyed by the item's **cursor** tumble index, measured on `currentFrame`. Since the worn-slot and
exotic-base tumbles (2026-09-12) an item's cursor index and its real tumble differ, and a broken item's
frame could be anything — whichever item measured a slot first set the offset for the rest of the
session.

### Fix

- `FinishOracoolDrop(ii, position)` (items.cpp/h): places the item **and** starts its tumble. The three
  hooks go through it.
- `RepairFloorItemAnimation(item)`: a floor item whose saved animation is not its tumble's (wrong frame
  count, or a frame out of range) is settled on its last frame and made clickable. Called for every
  item `LoadDroppedItems` reads, so the drops **already lying in existing saves** come back right on the
  next visit — the user does not have to find them again.
- Item labels key their offset by `GetItemDropAnimIndexFor(item)` and measure a clamped frame.

### Audit of the other tiers

Every `AllocateItem()` site was checked for the same gap:

| Path | Animation | |
|---|---|---|
| Vanilla monster drop, uniques, magic/rare/primal (`SpawnItem`, `SetupBaseItem`, `CreateMagicItem`, `SpawnUnique` both branches) | `SetupAllItems` → `SetupItem` | fine |
| Tier "set" suits (`TrySpawnOracoolSetItem`) | `SetupAllItems` | fine |
| Useful drops, spell books, quest items, Sealed Maps (`SpawnQuestItem`) | `SetupItem` | fine |
| Reward items (Map of Doom, Rune Bomb) | `setNewAnimation` direct | fine |
| Player drops, network drops, item doppels | `RespawnItem` | fine |
| Debug spawns | `SetupAllItems` | fine |
| **Named set pieces, socketables (gems/runes/charms/jewels/orbs), signets** | none | **fixed** |

Gems, runes and jewels share the socketable hook with the charms and orbs, so they were affected too —
just less noticed.

## 2. The spell countdown column

> "when a spell with countdown timer is active (infravision, etc...) put an 28x28px icon with blue
> backing of it next to the minimap, 6px away from it's left border, and next to the spell icon run a
> countdown seconds timer. if more than 1 such spells are active simultaniously make a columnd of them
> 6px vertically apart."

New `oracool/spell_timers.{h,cpp}`, drawn with the other corner widgets in `DrawView` (hidden with them,
and under the Crafting book). Each frame it asks the effects themselves what is running:

- **Engine spells on a missile clock**: Infravision, Etherealize, Search, Rage (active phase only).
- **The seven buff cries**: Shout, Battle Orders, Battle Command, Purifying Breath, Vengeance, Slow
  Missiles, Tranquility (`WarcryBuffTicks`).
- **The fourteen RfA-12 buffs**: new `Rfa12BuffTicks` in `rfa12_actives`.

A fixed order, so a row never jumps when another starts or ends. Each row: a 28×28 blue square whose right
edge is 6px left of the mini-map's frame, top-aligned with the mini-map and 6px apart; the seconds left,
rounded up, right-aligned to the left of the icon. A spell this fork added draws its tree glyph on the
blue; a legacy spell draws its own icon, shrunk to 28px, on the plate in the engine's blue spell tint
(`DrawTimedSpellIcon`, `DrawSmallSpellIconCoveringClipped` now takes a spell).

## 3. Stash SORT

> "divide socketable and unsocketable consumables in different tabs, one after the other. socketable
> consumables to be the first of the two."
> "put assembled runewords in separate stash tab"
> "when a consumables stash tab is full move other consumables to another separate tab of their own"

- **Runewords** are a seventh sort tier, after Primal: an item completing a runeword gets a page of its
  own whatever its base's quality.
- **Socketables** — runes, gems, jewels — keep the material page and its layout. **Salvage materials
  left it**: they cannot go in a socket, so they sort with the consumables (still white → dark grey).
- **Unsocketable consumables** — potions, elixirs, scrolls, books, oils, trap runes, salvage — take the
  page after the socketables, in family blocks as before.
- **Overflow**: `placeInBlocks` carries a full page over to the next EMPTY page. Socketables that miss
  the material page's overflow band do the same. Only a stash with no empty page left falls back to
  first-fit.

## 4. Consumable name colours by function

> "use color font for all consumables which still use basic white font based on their function."

Six new colour values (no .trn files), on the magic-damage ramp recipe:

| Function | Items | Colour |
|---|---|---|
| Cast a spell | scrolls | cyan 108,216,232 |
| Raise a stat for good | elixirs, special elixir | violet 180,140,255 |
| Improve gear | oils, Mystic Orbs | steel 168,188,208 |
| Lay a trap | Hellfire trap runes | red-orange 255,122,60 |
| Crafting reagent | salvage materials | bronze 200,160,120 |
| Open an encounter | Sealed Maps | teal 111,216,168 |

And onto existing colours by the same rule: gems and jewels take the runes' OR-7 (the socket), the Signet
of Learning takes the books' GD-6 (it teaches), the Arena potion the rejuvenation's YL-3 (restores all).
Potions, books and runes keep theirs.

## 5. Ctrl+right click buys a stack of potions

> "make ctrl+right click on a consumable potion in the vendors to purchase a stack of up to 99 of these,
> limited by amount of available money, or free slots in the belt/inv grid."

`ShopBuyPotionStack` (stores.cpp), reached from the shop grid's right click when Ctrl is held:

- **Which potions**: healing, mana and rejuvenation (plain and full) that the vendor RESTOCKS — Pepin's
  pinned potions (his tab and Griswold's Supplies), Adria's three pinned slots. A potion that sells out is
  one item, so a stack of it would be copies the shop never had; those get the ordinary single purchase.
- **How many**: the largest N ≤ 99 that the gold pays for AND that fits as one stack. Placement is
  all-or-nothing and a stack goes whole to the belt or whole to the backpack (INV-01), so the dry-run
  probe counting down from the affordable maximum finds exactly what the commit will place.
- **One transaction**: N × price charged once, the stack placed once, the coin sound once, autosave.
- When not even one applies (no gold, no room, not a restocking potion) it declines and the click runs
  the ordinary purchase, whose NoMoney/NoRoom screens say why — Ctrl never turns a click into nothing.

## Tests

- `OracoolDropsTumbleAndReloadRepairsOnesThatDidNot` — the repair and the hooks' placement.
- `SortPutsSalvageOnThePageAfterTheSocketables` (replaces the salvage-row test),
  `SortPutsRunewordsOnAPageOfTheirOwn`, `SortOverflowsConsumablesOntoAPageOfTheirOwn`.
- `ConsumableNamesAreColouredByFunction`, including a sweep that no stackable consumable is white.
- The colour-field test lists the seven newer colours.

## Re-audit before commit (user: "reaudit recent bug fixes again to make sure all coding is sound")

The whole diff was read again line by line and each assumption checked against the code:

- **Every drop path** re-walked (`AllocateItem()` and every direct `dItem` write): only the three hooks
  lacked the tumble; `SpawnRewardItem` sets it directly, player/network drops use `RespawnItem`.
- **The repair touches only broken items**: a valid save's frame count equals `ItemAnimLs` for its tumble
  (PNG tumbles are accepted only at that exact count), so nothing healthy is re-animated. Hardened after
  the audit: a repaired Magic Rock keeps turning, and a quest item selected by `_iSelFlag` 2 keeps it.
- **Label cache**: `labelCenterOffsets` is used only in `AddItemToLabelQueue`, sized `ITEMTYPES`, and
  `AddItemToLabelQueue` is only reached from `DrawItem` after its sprites guard.
- **Salvage still merges** on SORT: it is `isStackableConsumable`, so `MergeStacks` covers it now that
  the material page's own merge loop no longer sees it.
- **Oracool runes keep OR-7**: they carry no misc id, so the Hellfire trap-rune range cannot claim them.
- **`StashSortTier::LAST`** sizes nothing, so adding the Runeword tier moves no array.
- **Colour table**: `ColorTranslations` grew 37 → 43, positional with the six appended `text_color` entries.
- **Stack buy** mirrors each vendor's single purchase exactly (seed for restocking entries; Pepin's
  create info and unidentify), is single-player only, and bounds-checks the Supplies index before use.

## For the user to look at

- A cleared floor revisited: the set pieces, charms and orbs that were lying invisible are now normal
  size, on their labels, and clickable. New drops of those families appear when the monster dies.
- Cast Infravision (or a cry): the countdown column beside the mini-map.
- SORT: socketables page, then the consumables page, runewords on their own page.
