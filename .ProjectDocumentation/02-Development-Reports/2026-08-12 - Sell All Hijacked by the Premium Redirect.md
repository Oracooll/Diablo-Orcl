---
title: 2026-08-12 - Sell All Hijacked by the Premium Redirect
date: 2026-08-12
tags: [dev-report]
summary: Third strike for Sell All. Clicking it on a four-item page opened the last item's confirmation instead - the Premium Refresh redirect on Back's shared row was gated on "does that line have text" and a four-item sell list arms exactly that line. Now every redirect is gated on its own screen, and the routing itself has a regression test.
---

# Sell All Hijacked by the Premium Redirect

User report: "again Sell All is not working. My cursor is spot on the Sell All button." The screenshots showed the tell: the click opened the **Sapphire Buckler's** single-item confirmation - the last item in the list - not Sell All and not nothing.

## The mechanism

Back's row is shared real estate: Refresh Until flush left, Refresh / Repair all / Sell all flush right, Back in the centre. A click on that row is routed to whichever button's border zone it falls in.

The Sell all and Repair all redirects were gated on their own screen's `stextflag`. The **Premium pair was not** - it checked only "does that line index have text", and the comment beside it recorded the assumption: *only the Premium screen ever populates those exact line indices.*

A sell page with exactly **four items** breaks that assumption. The fourth item's name sits on line 17 and its two attribute lines on 18 and 19 - and line 19 is precisely `PremiumRefreshLine()` for this font. So `stext[19].hasText()` was true, the Premium branch matched first, every right-zone click on Back's row became "line 19", and the unselectable-row walk-back then turned line 19 into line 17: the last item. Click "Sell all", get "sell Sapphire Buckler?".

**Intermittent by list shape**, which is why this is the third report in this area: with one to three items on the page, line 19 is empty and everything works. Nothing about the previous two fixes was wrong; they each fixed a different defect in the same crowded row.

The left-zone twin was worse than the reported symptom: on the Sell screen `PremiumRefreshUntilLine()` numerically coincides with `SmithSellAllLine()` itself, so a click near Back's *left* edge would have sold the entire inventory with no confirmation.

## The fix

Every redirect on the row is now gated on its own screen's `stextflag`, Premium included. The block also moved out of `CheckStoreBtn` into a named, exported function - `ResolveBackRowClickLine(mouseX, uiLeft)` - for the reason below.

## The first regression test tested the wrong layer

The first test written for this set `stextsel` to Sell All's line directly and asserted `StoreEnter` ran Sell All. It passed - **with the bug still present** - because the bug lived in the click routing, which assigning `stextsel` bypasses entirely.

The test now drives the routing itself: build the four-item sell screen, assert the trap is armed (`stext[PremiumRefreshLine()]` has text - if a layout change ever un-arms it, the test demands a new arrangement, not deletion), then assert a right-zone click resolves to Sell All's line and a left-zone click does **not**. Against the pre-fix code the right-zone assertion fails with exactly the hijacked value.

Two things the test surfaced along the way:

- `StartStore` sets `stextflag` only *after* the screen populates, so the Sell all button is actually absent for the first frame and added by the per-frame rescroll. The game always gets that frame; a headless test has to request it (`RescrollStoreForTest`).
- `AllItemsList` wasn't exported across the test DLL boundary; it now carries `DVL_API_FOR_TEST` like the other data tables.

## Verification

Debug build clean at `ORACOOL_VERSION` **1.1.15**. Tests **348/350** - the suite grew by one, and the two failures are the same pre-existing pair (`Drlg_l1` stairs, `Timedemo`'s pre-save-break character).

Play-test: sell screen with exactly four items (the shape that broke), Sell All from the right zone; then Griswold Premium's Refresh/Refresh Until/Back row, which must still route all three correctly.

## Related

- [[2026-08-12 - Equipment Slots Play-Test Fixes]]
