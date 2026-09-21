# Adria takes the last portrait, and the rule stops naming vendors — v1.12.128

**Date:** 2026-09-21
**Version:** v1.12.128
**Branch:** renderer-32bit

## The request

> now do Adria's canvas `Resources\Adria The Witch Shop UI\Adria 340x720.png`
> Apply same redactions as with Wirt including moving the gold, replacing the refresh button.

## Measured first

```
col x=60 : bright at y 159..168 and 619..628
row y=450: bright at x 11..28 and 311..331
```

Band at **x 26..313, y 159..628** — the fourth canvas in a row on exactly that geometry. Painting
swap; nothing moves. Two files were in the folder and only the one named was touched.

## One line of table, and the rest came free

The canvas took the entry `PortraitCanvasFor` was built for last build:

```cpp
case TalkID::WitchBuy:
case TalkID::WitchSell:
	return AdriaCanvasAsset;
```

That alone gave her the canvas, no title, no tint, no second bezel, one pass of grid fill, and
Griswold's gold pile in place of the "Your gold:" line.

## "Replacing the refresh button" — she has no Refresh

Worth stating plainly, because it is the one place the request and the code do not line up: Adria has
**no Refresh button**. `GetShopActions` has no Witch case, so `ShopTabHasRefresh` is false on both her
tabs, and the single service her control row carries is **Recharge**.

So Recharge is the button that became a painted plate — Griswold's Recharge frame, at Griswold's own
Recharge position (slot 2, x 96). That is the analogue of what Wirt got: *the row's service becomes
the painted plate*.

To make it so, the portrait rule stopped naming a service:

```cpp
// was: the slot must be Refresh, and the row must have carried one
// now:
const std::vector<ServiceButton> services = ServicesFor(id);
return std::find(services.begin(), services.end(), ShopServiceSlotDoes[slot]) != services.end();
```

A rule about **services** rather than about **vendors**. Wirt's row carried Refresh so he gets one
plate, Pepin's carried nothing so he gets none, Adria's carries Recharge so she gets that one — and
the next vendor needs no case here at all.

Her Recharge keeps everything the wide button had: the drop target (`ShopRechargeHeldItem`), the
armed-cursor click (`ArmShopRechargeCursor`) and the hint. Those were already in the painted frame's
handlers for Griswold's own Recharge; she simply reaches them now.

## Files

- `Packaging/resources/oracool_assets/ui/adria_canvas.png` — new, 391871 bytes, 340×720.
- `Source/oracool/shop_grid.cpp` — `AdriaCanvasAsset`, her two tabs in the table, and
  `ShopServiceSlotOnPage` generalised off the Refresh literal.

**Asset added — `tools\build_oracool_mpq.cmd` run, 601 files.**

## Worth a look in play

Her lone Recharge plate sits at Griswold's x 96 — left of centre, because that is where the third of
his six sits. A single button at a position meant for one of six may read as off-centre; Wirt's sits
far right at 282 for the same reason. Both are deliberate ("use Griswold one"), and both are one
number if they should instead centre on the page.
