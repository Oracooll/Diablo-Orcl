# Five fixes from the first real play session (v1.9.65)

**Date:** 2026-08-26
**Version:** 1.9.64 → 1.9.65
**Tests:** 562, of which 560 pass — the two standing baseline failures, unchanged.

---

## The session got past the crash

The first report since v1.9.9 that describes *play* rather than a crash: town, a vendor, 854 gold, a
level-up, Cathedral 1. The v1.9.64 save fix appears to have held.

Five things came back from it.

---

## 1. Smite demanded level 8 on a tier that opens at level 1

> "there is a bug with Smite skill - is it requiring lvl 8 for some reason?!"

It was. Seven Paladin actives exist twice over: as rows on the class tree's Combat Skills page, and
as entries in `paladin_skills.cpp`. Each had a level in **both** places, and the stricter applied.

The tier is not an internal detail — it is the row's position on the page, and the page states that
its top tier opens at level 1. Two rows disagreed with their own tier:

| skill | tier | tier level | table said |
|---|---|---|---|
| Smite (Shield Bash) | 0 | 1 | **8** |
| Charge | 1 | 6 | **12** |

Both moved onto their tiers. Charge was not reported, but it is the identical defect one tier up and
would have been met at level 6.

The shield requirement is untouched — that is a real condition the player controls.

`EveryBorrowedPaladinSkillMatchesItsTreeTier` now asserts the property the *player* experiences: a
character at a row's tier level can have that row, and one level below cannot. Two constants matching
today is not the same as two constants that cannot drift.

## 2. The mlvl text overlapped the health bar

Drawn at `height - 13` — inside the frame, on top of the bar art and whatever colour the health fill
happened to be behind it. Legible against an empty bar, invisible against a full one.

It now sits below the frame, still right-aligned, and gained the same black offset the monster's name
above it uses, because outside the frame it is drawn over the dungeon floor.

## 3. The purchase confirmation is gone

> "we don't need it with the new interface"

The "Are you sure you want to buy this item?" screen is inherited from the text-list stores, where a
click was a cursor landing on a row. The shop grid is not that interface.

Answered by auto-confirming rather than by deleting the screen, deliberately: `TalkID::Confirm` is
the one place every vendor action converges — buy, sell, repair, recharge, buy-back. The handlers
still run their afford and room checks first, and those open `NoMoney` or `NoRoom` *instead* of
`Confirm`, so this only ever answers a transaction that was already going to be allowed.

## 4. Buying is a right-click; left-click only looks

`CheckShopGridClick` now takes the button. A left click on an item moves the selection so the footer
describes it and **spends nothing** — which is precisely what makes an unconfirmed right-click safe.
Every other control on the panel answers both buttons, because for those the click is the whole
intent.

## 5. Selling is a right-click in the backpack

`ShopSellInventoryItem` sells the item where it lies, using the vendor's own acceptance rules — Adria
still refuses armour, Griswold still refuses potions.

**Backpack grid only.** A worn item's `cii` is below `INVITEM_INV_FIRST` and is refused, per the
request and for an obvious reason: selling the armour off your back to a mis-click is not a trade.

## And the gold sound

Played once in `ConfirmEnter`, after the switch, rather than in each of its eleven branches — a new
vendor action gets the sound by existing, which is the only way this stays true. The Storyteller's
identify returns before it, correctly: it is the one branch that charges nothing. The held-item sell
path plays it too, since that one bypasses `ConfirmEnter` entirely.

---

## The art drop

Swept as asked. The batch is **not** in `Diablo Orcl V1/Oracool.MPQ/` — it is one level up, in the
sibling `Diablo/Oracool.MPQ/`, which is the actual drop zone:

`colorful-skill-icons-batch-01-paladin-combat.zip` — 11 Paladin Combat Skills icons, named to the
indices in the reference sheet generated on 2026-08-25 (`paladin_00_sacrifice` … `paladin_08`, plus
`paladin_29_hammer_of_faith` and `paladin_30_blessed_shield`, which are the two appended rows). It
carries a manifest, a QA report, contact sheets, and 2 MB source renders beside the small icons.

Integration is its own unit of work and is not in this version.
