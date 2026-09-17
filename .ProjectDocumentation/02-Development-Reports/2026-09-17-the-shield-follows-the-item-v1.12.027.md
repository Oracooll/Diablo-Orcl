# The shield follows the item

2026-09-17 — v1.12.027

## Why

> "let's also borrow the shield from medium and use it as well. any problems?"

Each of the three armour tiers was drawn with a shield of its own - the light tier's round red
buckler, the medium tier's steel heater with a cross, the heavy tier's blue heater with a lion - and
until now the shield on screen followed the ARMOUR, with one exception added at v1.12.023 (big shields
brought the heavy heater onto a lighter body).

## What it is now

The shield follows the ITEM, on any body:

| item | shield drawn |
|---|---|
| Buckler, Small Shield | light tier's buckler |
| Large Shield, Kite Shield | medium tier's steel heater |
| Tower Shield, Gothic Shield | heavy tier's blue heater |
| anything else (the Orcl bases) | the body's own, as before |

This CHANGES one earlier rule: the Kite Shield was the heavy heater in v1.12.023-026 and is now the
medium one. And it works downward as well as up - a knight in plate carrying a buckler - which nobody
asked for by name but which the mapping implies; it cost nothing, since `sprite_mix` never cared which
tier was the "bigger" one. `LookTier` is now Own / Light / Medium / Heavy, the swap happens whenever the
item's tier is not the body's, and the sword is unchanged (heavy longsword on a light or medium body).

## Problems? - checked, and one found

**Medium's sheets are the best twins of the three** (77-92% same-ramp between sword and
sword+shield, every animation), so subtraction is at its cleanest there.

**The worry was grey on grey** - a steel shield lifted off a steel-armoured body, where a difference
seeded by ramp might not see the shield over the plate. It does: the medium body is mostly a blue
tabard and orange boots, the shield's edge always lies against something that is not steel, and index
differences within two pixels of a seed come along. All five new combinations were exported and looked
at, eight facings each: clean.

**The fire cast was not.** For some pairs of tiers it slipped past the size guards and came out as a
mottled body inside the flames. The guards had caught it for light+heavy and missed it for others, and
a rule that fails per pair is not a rule - so the fire cast (`fm`) is now named and never mixed.

| animation | shield | sword |
|---|---|---|
| town stand, town walk | item's | see v1.12.026 |
| stand, walk, attack, hit, magic | item's | see v1.12.026 |
| lightning | item's, except a medium piece (its sheet is a different size) | - |
| fire, block, death | body's own | own |

`CacheVersion` 4: cached sheets are rebuilt once, in the background.

## Verified how

`oracool_sprite_export --mix` gained five cases (L body + M shield, M+L, M+H, H+L, H+M); contact sheet
`engine_mix_contact3.png`. 795 tests pass, the item rules among them. Not run in the game.
