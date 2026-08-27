# Smaller auras, and three shelves instead of one (v1.9.77)

**Date:** 2026-08-27
**Version:** 1.9.76 → 1.9.77
**Tests:** 564, of which 562 pass — the two standing baseline failures, unchanged.

---

## The aura ring is a mark on the character now

> "shrink auras visual assets to 2-3 tile radius. now the aura graphics spans about 10 tile maybe."

It did, and the arithmetic says so. `AuraRadiusForPoints` runs four tiles at one point up to eight,
and the ground projection multiplies that by √2 to reach the diagonal — so **even a single point drew
an ellipse about eleven tiles across.** A wash of colour under half the screen rather than a ring
around a character.

Drawn radius is now **2 tiles, or 3 once five points are in.** Points still change the ring, so
investment is still visible; they change it by a tile instead of by five.

**This deliberately breaks the ring's relationship to the aura's reach**, and that is worth stating
rather than burying. The gameplay radius is untouched — monsters are affected exactly as before — but
a player can no longer read the field's edge off the floor.

I think that costs nothing real. It was not readable before either: at the eight-tile cap the ellipse
covered everything already on screen, which is the reason `AuraRadiusForPoints` gives for capping
there in the first place. What is lost is the *appearance* of information.

If you would rather the ring still meant something measurable, the alternative is to shrink the
gameplay radius to match — but that is a balance change and I have not made it.

## Adria and Pepin have their own Oracool lines

Griswold's shelf was wired up in v1.9.74; these two were left because each vendor rolls differently.
They are not simply given the same gear, because the same gear would be wrong for both:

| vendor | Oracool line | why |
|---|---|---|
| Griswold | armour and weapons | already shipped — his trade |
| **Adria** | gems, runes, jewels | she deals in the magical; socketables are a spellcaster's shelf |
| **Pepin** | stat charms | steady always-on help, the same shape as his potions |

Gems, runes, jewels and charms are **fixed-identity** items — no affixes, no level scaling, the index
*is* the item — so they go through a separate hook from Griswold's rolled gear, built with
`InitializeItem` and stamped `_iCreateInfo = 0`.

That stamp is why this is a second function rather than a flag on the first. Getting it wrong is not
cosmetic: a town stamp routes the item through the pool that excludes Oracool indices entirely, and
it comes back as something else on the next load — precisely what had been happening to Charms of
Salvaging until yesterday. Keeping the two paths visibly separate is what stops that being an easy
mistake to make again.

Everything is depth-gated by the same banded qlvl ladder the drop hook reads, so a low-level Adria
offers chipped gems and a high-level one offers the rest.

---

## Still queued — four items

The Rare tab; the Set shop; the hammer-cursor Repair rework; and Refresh on Basic/Rare/Supplies.
