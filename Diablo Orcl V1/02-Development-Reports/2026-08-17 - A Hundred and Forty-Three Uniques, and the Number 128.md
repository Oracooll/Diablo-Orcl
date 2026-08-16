# A Hundred and Forty-Three Uniques, and the Number 128

**Version:** 1.7.56
**Date:** 2026-08-17
**Files:** `Source/oracool/unique_affixes.{h,cpp}` (new), `unique_items_data.inc` (generated), `tools/GenUniqueItems.ps1` (new), `Source/itemdat.{h,cpp}`, `Source/items.{h,cpp}`, `test/pack_test.cpp`

---

## What went in

143 of the 250-unique expansion, droppable, wearing their base item's sprite.

> i am waiting for the sprites. you can put them in the game now and latter attach the sprites.

That instruction shaped the design more than anything else. Because no art exists yet, **nothing here allocates an icon** — no CEL frames, no cursor ids, no frame-order contract to get wrong. Each unique deliberately carries no `IPL_INVCURS`, the power that replaces an item's sprite, so it simply looks like the base it rolled on. Adding the art later is one more column in the generator, not a change to any of this.

Identity rides the vanilla `UniqueItems` table. Appending is what makes this cheap: naming, description, the drop roll in `CheckUnique`, and `_iUid`'s persistence are all machinery that does not care how long the array is — it scans to a `UITYPE_INVALID` sentinel. So 143 new uniques cost **no save-format change, no new field on `Item`, and no new code path.**

---

## The affix audit, and why it was not optional

The package declares an `enginePower` for every affix. I resolved all 43 tokens through an independently-audited table instead, read against `SaveItemPower` line by line, and the generator reports every disagreement rather than preferring a side.

There was exactly one disagreement. It was on the token carried by **109 of the 250 items**:

| | |
|---|---|
| Package says | `flat_armor` → `IPL_SETAC` |
| `SaveItemPower` does | `item._iAC = r` — **overwrites** |

A Gothic Plate rolls 42 armour. `"+8 flat armor"` applied as declared would have left it at **8**. A downgrade, printed as a bonus, on nearly half the expansion.

There is no flat-armour *add* in this engine at all, so it rides `IPL_ACP` — the same decision `item_set_stats.cpp` reached for `armor_class_flat`, for the same reason. Their other 42 tokens check out exactly.

This is the third instance of the same failure in three days: `enhanced_armor` → `IPL_TARGAC`, `armor_class_flat` on a bonus rung, and now this. **The pattern is always a power whose name describes the intent and whose implementation does something else.** Reading `SaveItemPower` is cheap; trusting a name is not.

640 affixes live, 53 inert. The 19 inert tokens are recorded with what each would need.

---

## The number 128

The test suite caught a real engine bug, and it is a good one.

Vanilla wrote **128** into four places that never named each other:

```cpp
std::bitset<128> uok = {};              // CheckUnique
bool UniqueItemFlags[128];              // items.cpp
uint8_t itemData = 0;                   // CheckUnique
itemData = (itemData + 1) % 128;        // CheckUnique
```

...plus a fifth ceiling hidden in a type:

```cpp
enum _unique_items : int8_t             // ids cap at 127
```

Vanilla has 87 uniques, so none of it was ever approached. The expansion took the table to **230** and all five broke at once — surfacing as `Assertion failed: bitset subscript out of range` inside `UnPackItem`, a message mentioning neither uniques nor the table.

Worth noting what the `uint8_t` and the `% 128` did *independently* of the crash: even without the bitset overrun, **every unique past index 127 was unreachable**. The selection walk could not step to it and the return type could not represent it. The expansion would have appeared to work while silently never dropping more than half of itself.

All five now derive from one `MaxUniqueItems` in `itemdat.h`, with a `static_assert` beside the table's definition:

```cpp
static_assert(sizeof(UniqueItems) / sizeof(UniqueItems[0]) <= MaxUniqueItems, ...);
```

Outgrowing it is a compile error naming both the constant and the enum's width.

---

## The golden corpus, and what it cost

143 new candidates in a pool of 87 **displaces vanilla uniques in the selection walk**. Nine of `pack_test`'s ninety golden items now recreate as different uniques — `Bone Chain Armor` → `Black Meridian Robe`, `The Protector` → `The Crooked Meridian`, and seven more. Both members of each pair share a `UITYPE_`; the new one simply qualifies at a lower level and lands earlier in the walk.

This is the expansion working as asked, not a defect. The user confirmed the trade explicitly ("accept it"), so the corpus was regenerated rather than the drop pool being gated.

Two things worth recording about how that went:

**`CompareItems` opened with `ASSERT_STREQ` on the name.** An `ASSERT` aborts the enclosing function, so a changed name hid every other difference in that item and each run revealed exactly one field. Nine items differed across **120 fields**; at one per rebuild that is a crawl. Changed to `EXPECT_STREQ`, which is what a comparison helper wants — see everything at once.

**The rows were regenerated, not hand-edited.** A temporary `DISABLED_` test dumped all ninety rows in the fixture's own literal format; the table was spliced wholesale and the generator removed. Hand-transcribing 120 field changes would have been slower and less trustworthy than the thing it was checking.

One packed fixture also changed: `The Protector` was a staff carrying 86 charges, and the seed now yields an item with none, so `bCh`/`bMCh` are zeroed. The fixture is a packed item *as the game would write it*, not an archaeological record — and V1 is new-game-only, so no real save holds the old reading.

---

## Verification

- Debug build clean at 1.7.56.
- Generator: 250 items read, 143 emitted, 640 live affixes, 53 inert, **1 package/table disagreement** (`flat_armor`, expected).
- Full suite **441 of 443** — the two failures are the standing baseline pair, and `PackTest.UnPackItem_hellfire` is green again.

---

## The 107 that did not go in

They sit on 18 base tokens this engine has no `unique_base_item` value for. The generator names them when it runs rather than emitting them pointing at `UITYPE_NONE`, which would look like a live row and never drop.

| Group | Items | State |
|---|---:|---|
| gloves / gauntlets / boots / greaves / sash / war belt / pauldrons / shoulder mantle | 66 | **The fork already has these bases** — they carry `UITYPE_NONE` and need 8 new `UITYPE_` values wired to them |
| relic / reliquary / cloak / battle cloak | 23 | Slots this fork has not built; the same gap that leaves 21 set items unspawnable |
| spear / pike / war lute / war quiver / canticle / arcane focus | 18 | Weapon bases that do not exist here |

The first group is the cheap 66 and is mostly mechanical. The other two need real content decisions first.

Also still open: `gold_find_percent` is inert for the third time now. `ItemBonusTotals::goldFind` and `Player::_pGoldFind` both exist and charms feed them — no `IPL_` writes it from an item. Wiring one would light up these uniques *and* the Rat King's Tithe set, whose entire identity is gold.
