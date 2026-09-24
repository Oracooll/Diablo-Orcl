# Every vendor offers Orcl items — v1.12.172

2026-09-24

> double check and make sure all vendors can offer orcl items. (in-game /dev note)

## What was already right

Every vendor rolls affixes through `GetItemBonus` → `GetItemPower` → `DrawUnifiedAffix`, the
Loot 2.0 pool. The shared base pool (`GetItemIndexForDroppableItem`) refuses every Orcl base,
socketable and shard **on purpose** — it is the save format, replayed from seeds — so the fork's
gear reaches shops only through side passes. Griswold's Rare, Unique and Set shelves, Adria
(gems, runes, jewels, heads for a Necromancer) and Pepin (charms) had theirs.

## What was not

| Vendor | Gap | Fix |
|---|---|---|
| Wirt, Shop | no side pass at all | `RollBoyShopSlot`: every third slot is Orcl magic gear (`StockOracoolMagicItems`), interleaved so the one-page trim cannot cut them all; a bought slot restocks as its kind |
| Wirt, Gamble | slots only vanilla types; base from the vanilla pool | the six worn types join `GambleSlots` (Orcl bases only); a third of the other slots draw an Orcl base of their type (`RndOracoolGearBaseOfType`); Orcl bases stamped with a bare level, never `CF_BOY` |
| Wirt, Gamble | the pool never returns `IDI_NONE`, so a low-level slot could hold another type's base at this slot's price | the pick is checked against the slot's type |
| Griswold Basic + Magic | Orcl bases gated by vendor level, capped at 16 — ~40 of 147 expansion bases | gated by `max(character level, vendor level)`, as the Rare shelf is since 2026-09-12 |
| Griswold Magic | Orcl items' ilvl never passed 30, never Torment-tier | `itemLevel = VendorItemLevel(lvl)`, as the Rare shelf stamps |
| Griswold Unique | shrunken-head uniques offered to anyone | refused unless `NecroHeadsMayDrop()` |
| Pepin | the three encounter charms were in his filter, off the shelf only by level | excluded by name |

## Left open

Imbuement Shards, Signets and Sealed Maps are sold by nobody and no comment says why. Asked.

Debug build and tests clean.
