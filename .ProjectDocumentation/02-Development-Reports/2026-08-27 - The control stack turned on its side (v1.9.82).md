# The control stack turned on its side (v1.9.82)

Date: 2026-08-27
Version: 1.9.82
Tests: 562/564 (the two standing baseline failures)

Two reports, and the second one was a better idea than what it replaced.

---

## The Set tab bounced back to Griswold's dialog

> "SET button sends me back to dialog window of Griswold"

**The lowest `requiredLevel` on any named set piece is 18.** The generator filters candidates by
`def.requiredLevel > player._pLevel`, so a character below 18 earns none of them, the shelf comes out
empty, and the text-store's start returns false — which drops the player back to the vendor's dialog.

That is exactly the failure `HasCuratedShelf`'s own comment warned about ("a tab that opens an empty
screen") and the tab strip was asking the wrong question: the INI switch says whether a shelf is
*wanted*, not whether there is anything on it. The strip now asks `CuratedShelfHasStock`.

**A second bug was hiding behind the first, and it would have outlived the fix.** The shelves are
built once per game, flagged by `initialized` — but the flag was being set on the *attempt*, not on
the result. So a character who reached level 18 mid-game would have had the Set shelf permanently
marked done-and-empty. `initialized` now means *something was built*; an empty attempt leaves it
unset and the next town visit tries again. Worth noting this affects the Rare and Unique shelves too,
which are level-gated by the same convention.

---

## One row of tall buttons instead of five rows of wide ones

> "Why are we using 4-5 rows of horizontal buttons? Cant we use one row of vertical button tall as
> much as 4-5 rows, but narrow enough to fit 11-12 buttons?"

That was the right question, and I had not asked it. The stack had grown to **three rows of tabs plus
a services row plus a bulk row** — five full-width rows spent on labels of five to eight characters
each. It was a layout paying for width in the one dimension this panel has none of, and I had been
solving it by shaving pixels off row heights.

Turned on its side, the same eleven controls cost **one row**: Basic, Magic, Rare, Set, Unique,
Supplies, Sold, Repair, Repair All, Recharge, Refresh. The grid keeps all sixteen rows, which was the
standing constraint.

### What it took

Three separate systems became one. Tabs, services and bulk actions had their own rect function, their
own draw loop and their own hit test — three places that had to agree about where a row started, and
the reason adding a control used to be a four-site change. They are one tagged list now
(`ControlButton`), and only the *click* differs between kinds.

`ShopControlSlots` is fixed at **twelve**, not derived from the button count. Deriving it would make
Adria's three buttons a hundred pixels wide each, and a vertical label in a hundred-pixel box reads
as a mistake. A vendor with fewer buttons gets the same narrow ones, centred as a group.

### The vertical labels

The engine has no rotated text, so a vertical label is a stack of glyphs. Uppercased first — caps
have no descenders, which is what allows the pitch to be tightened below the font's own line height
without letters touching.

**The pitch is computed from the label's length and the height available, and the font is then the
largest that fits that pitch** (the four small font sizes this fork added earn their keep here). A
long label therefore *shrinks* rather than overflowing. That is deliberate: the silent clip is a
failure this exact panel has already had once, when "Supplies" rendered as "SUPPLIE" and nothing
reported it.

Spaces become gaps rather than glyphs, so "REPAIR ALL" reads as two words rather than one column of
letters.

### What moved as a consequence

**Repair All's price is no longer appended to its label.** A price on a vertical label would be a
column of digits down the side of the panel. It is in the hover hint instead — `SetServiceHint`'s
"Cost" line, added yesterday — so hovering the button still shows the number, which is the
requirement as stated ("make it on hover").

The placeholder service *icons* are gone. They had already been replaced by text and the drawing code
was dead.

---

## The honest caveat

**A vertical label is slower to read than a horizontal one.** That cost is real. The argument for
paying it is that this is a strip of seven to eleven buttons whose positions a player learns within a
visit, which is the case where the trade is cheapest — and the alternative was spending five rows of
a panel that has 106 pixels to spend.

But per this project's own rule, **constants and tests both pass while a window is visibly wrong**.
This one needs your eyes before I call it done. If it reads badly, say so and I will put the
horizontal rows back — the change is contained to one file.

---

## Verification

562/564. The save-format tests contend over shared files under `ctest -j`; rerun serially they pass.
The two real failures are the standing baseline pair.

## To look at in game

- Griswold's strip: one row of tall narrow buttons. Check the longest labels — SUPPLIES, RECHARGE,
  REPAIR ALL — are legible and not clipped.
- Adria and Pepin: fewer buttons, same width, centred.
- Hover Repair All and confirm the cost still appears in the info box.
- The Set tab should now be **absent** below level 18 rather than present and bouncing you out.
