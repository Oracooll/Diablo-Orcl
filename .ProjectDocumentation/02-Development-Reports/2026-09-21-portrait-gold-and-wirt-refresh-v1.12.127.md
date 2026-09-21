# Pepin and Wirt take Griswold's gold, and Wirt takes his Refresh — v1.12.127

**Date:** 2026-09-21
**Version:** v1.12.127
**Branch:** renderer-32bit

## The request

> for wirt and pepin - remove the current gold counter and put the Griswold one.
> just for wirt - remove the current refresh button and use Griswold one.

## 1. The gold

Both pages drew the shared row's readout: a centred **"Your gold: 12,345"** on the line above the
grid. Both now draw Griswold's instead — the pile at (27,627) with the bare number under it at
(25,654), no label, because the pile says what the number is.

One function at one pair of positions:

```cpp
void DrawShopGoldPile(const Surface &out)   // lifted out of DrawRedesignedControls
```

That is the point of the request rather than an incidental tidy: three shops now count gold in the
same place in the same language, and if that place ever moves, it moves for all three.

The page arrows keep their place on the old gold row. They were not part of the request and were
left where they are.

## 2. Wirt's Refresh

His was a wide word-button in the control strip (`ControlKind::Service`). It is now Griswold's
painted 34×34 plate with the refresh glyph, at **Griswold's own Refresh position** — slot 5,
(282,121) — which is what "use Griswold one" asks for: the same button in the same place across the
shops.

Pepin gets no frame. His control row has no services at all, so there is nothing of his to replace.

### The trap: his Refresh is not a store action

`ShopServiceSlotEnabled` answered `ShopTabHasRefresh(id)` for the Refresh slot, and that asks whether
the page has a **Refresh action row**. `GetShopActions` returns nothing for `BoyBuy`/`BoyGamble` —
Wirt's old button called `RefreshBoyStock` directly and never went through an action line. So the
painted frame would have been drawn **greyed out and dead on the one vendor the user asked to have
it**, and every test would still have passed.

Two places needed his case:

```cpp
bool PageRefreshesBoyStock(TalkID id) { return IsAnyOf(id, TalkID::BoyBuy, TalkID::BoyGamble); }

// enabled:
case ServiceButton::Refresh:  return ShopTabHasRefresh(id) || PageRefreshesBoyStock(id);

// release:
case ServiceButton::Refresh:
    if (PageRefreshesBoyStock(stextflag)) { RefreshBoyStock(stextflag); PlayUiSelectSound(); }
    else                                    ShopRunRefresh(stextflag);
```

The sound is not decoration: `ShopRunRefresh` goes through the action path, which plays one;
`RefreshBoyStock` does not. Without the explicit call, the button the user asked to *replace* would
have come back silent.

## 3. The structural change: one question, four call sites

The service frames were gated on `IsRedesignedShopScreen(stextflag)` in four places — draw, click,
hover, release — each followed by its own `ShopServiceSlotVisible` test. Adding a second kind of page
to four gates is how one gets forgotten.

Now a single function answers:

```cpp
bool ShopServiceSlotOnPage(int slot, TalkID id)   // Griswold: all. Portrait: Refresh, if the row had one.
bool PageHasServiceFrames(TalkID id)              // the gate those four sites share
```

And the rule that stops Wirt having **two** Refreshes — a painted frame *and* a row button — is one
filter in `ShopControlButtons`:

```cpp
if (ServiceHasPaintedFrame(service, id)) continue;
```

The frame is drawn from the slot table and the row is built from `ServicesFor`. Without that line
both would be correct, independently, and the player would see the button twice.

## Files

- `Source/oracool/shop_grid.cpp` — `PageRefreshesBoyStock`, `ShopServiceSlotOnPage`,
  `PageHasServiceFrames`, `ServiceHasPaintedFrame`, `DrawShopServiceFrames` and `DrawShopGoldPile`
  extracted, the four gates switched, the Refresh release branched, the row filtered, and
  `DrawShopControls` given the portrait path.

No asset changes, so no MPQ repack.

## Noted, not changed

Griswold's gold sits at y 627–654 against a painted frame whose bottom band ends at 628 — a 2px
overlap that Pepin and Wirt now inherit, since they were given his exact position on purpose. It is
one constant if it ever wants moving.
