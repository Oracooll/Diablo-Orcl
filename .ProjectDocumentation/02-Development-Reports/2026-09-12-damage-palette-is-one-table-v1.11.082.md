# Diablo II's damage palette, taken literally

2026-09-12 — v1.11.082

## What was asked

> also make sure cold and physical dmg floating texts use proper color.

## Where v1.11.080 left it

That version made fire, lightning and magic defer to the character sheet's table and kept **two
deliberate divergences**: physical gold over a monster against white on the sheet, and acid yellow
because white was cold's. Both were documented and both were pinned by the test, so they were
choices rather than oversights — but they were still two places where an element could be described
two ways.

Neither cold nor physical was *broken* the way fire had been. Cold rendered white, physical rendered
gold, both intentionally. So this was a taste call with two defensible answers and a visible
consequence — physical is the most common number on screen — and it went to the user rather than
being guessed at. They chose the full Diablo II mapping.

## The obstacle that had already dissolved

The sheet's table carried this reasoning since 2026-08-31:

> Blue is MAGIC here, not cold: **this engine has no cold damage at all.**

That was true when written and had stopped being true twice over:

1. **Cold damage exists.** `DamageType::Cold` is dealt by Holy Freeze (`aura_field.cpp:112`), Frost
   Nova (`missiles.cpp:2120`), and the Round 1 cold missiles — with its own resistance divisor and
   its own chill.
2. **Blue was free.** Magic moved off blue on 2026-09-11 for RGB 104,49,49, then 208,98,98 the same
   day. Nothing took the seat.

So cold could simply take its own colour back, and that freed white for physical. Two compromises
resolved by one substitution, which is why the full D2 reading is cheaper than it looks rather than
more expensive.

## The palette now

| Element | Colour | Was |
|---|---|---|
| Physical | white | gold |
| Fire | red | grey (`ColorUiSilver`) |
| Lightning | yellow | blue |
| Magic | `ColorMagicDamage` 208,98,98 | orange |
| Cold | blue | white |
| Acid | `ColorOracoolGreen` (D2's poison seat) | yellow — lightning's ink |

`DamageTypeColor` in `charpanel.cpp` is now **the** table and `DamageTextColor` is a pure deferral
to it — one line, no exceptions. Six elements, six colours, and a `seen.size() == 6` assertion so a
future collision cannot be introduced quietly. Acid joined that set: it had been sharing lightning's
yellow, which is exactly the kind of collision the assertion exists to refuse.

## The ambiguity that was accepted, not solved

Blue is also the character sheet's "buffed/active" colour on twenty-odd other rows, including the
aura row. So on the sheet a cold damage row and a buffed row are the same blue.

Accepted deliberately, and written into the table's comment: on the sheet, blue on a damage row *is*
the row being coloured by element, which is the entire point of that feature; over a monster's head
there is no buffed row for it to be confused with. The alternative — a second blue — would add a
colour to the registry to solve a problem nobody has reported.

## Verification

**722/722 tests pass.** `OracoolAudit.AnElementIsTheSameColourOnTheSheetAndOverTheMonsterSHead` now
asserts all six elements agree with the sheet (no exception list left), each colour individually so
a red run names the element, `Fire != ColorUiSilver`, and six distinct colours.

The existing readied-slot palette tests were checked first and are unaffected: none of them readies
a cold or acid spell, and the `ColorBlue` at `oracool_audit_test.cpp:11085` is the aura row's own
path, not `DamageTypeColor`.

Debug and Release build clean, RTM refreshed. No asset changed, so no MPQ repack.

## What to look for in play

Every damage number moved, not just the auras. Swing a weapon — **white** now, where it was gold.
Holy Freeze and Frost Nova — **blue**. Holy Fire and a Firebolt — **red**. Holy Shock and Charged
Bolt — **yellow**. Sanctuary and Blessed Hammer — **dusty red**. Acid from a monster — **green**.

White physical is the one to judge honestly: it is the number you will see most, and gold had been
its colour since vanilla.
