---
date: 2026-08-20
version: 1.8.67
tags: [items, sockets, runes, gems, ui]
---

# What a Stone Does, and a Red Number on the Floor

Two requests, and a third thing found underneath the first.

## Loose runes and gems describe themselves

The question was *"why runes show available RW instead of their affixes/stats?"* — and the answer
was that they never showed them. `RuneTeachingLines` was the only rune branch in the description,
and a loose **gem** had no branch at all. The one thing you need in order to decide whether to
socket a stone was the one thing not printed.

The effects always existed and were always rendered — runes are `GemData` rows and `GemSocketLine`
formats them — but only from the socket loop further down the same function. A stone described
itself *after* the decision had been made, never before.

Now a loose gem or rune prints up to three lines, one per host: **In weapons / In shields / In
armor**. Three because a loose stone does not know where it is going. Empty lists fall away, so a
gem that does nothing in shields says nothing about shields.

`MaxTaughtWords` drops from 6 to 3. The per-host lines are what the panel is *for*; the recipes are
the bonus, and at six words plus a remainder the panel already spanned most of the screen on a rune
as common as El.

## The bug underneath

Extracting `GemEffectParts` out of `GemSocketLine` exposed why this could not simply be reused:
**the formatter only ever knew the launch gems' channels.** Eleven fields that Sockets v2 added in
1.8.8–1.8.9 were never printed at all:

`weaponDamagePercent`, `allStrength`, `allMagic`, `allDexterity`, `allVitality`, `allMagicFind`,
`allGoldFind`, `weaponFlags`, `armorFlags`, `shieldFlags`, `requirementPercentReduction`, and Zod's
`indestructible`.

The flags are where D2's knockback, attack speed, hit recovery, block speed, steal and demon damage
actually live in this engine. So a rune whose only effect in a host **is** a flag rendered as a
colon with nothing after it:

```
Shael Rune:
```

That has been true on every socketed item since 1.8.9. It survived a wiki audit, a numbers sweep and
four green test runs, because nothing reads a socket line except a person — and the person reading
it would have assumed the rune was weak, not that the formatter was blind.

Had I only added the new lines, they would have been blank for the same runes. That is the trap: an
output that looks implemented and says nothing.

### Pinned

`OracoolAudit.EveryRuneDescribesItselfInEveryHost` asserts that all 33 runes produce a non-empty
line in all three hosts, through both entry points. **Proven to fail first** — disabling the flag
text turned it red on Nef, Amn and Shael before the fix went back in. Gems are deliberately *not*
asserted: a gem legitimately does nothing in some hosts, so the same assertion there would be wrong.

## The red socket count

*"when a socketed item drops add a sucket number suffix to the name in brackets in RED font."*

`Short Bow [3]`, the brackets in `ColorRed`.

Two details that are not obvious from the request:

**It is the total, not the empties.** `[3]` is the item's ceiling, which is what makes a host worth
walking to; a partly filled base is still a host. Empty-count would make a fully socketed rare read
as `[0]`.

**Red regardless of quality.** The suffix does not take `item.getTextColor()`. The point is legibility
across a floor of drops, so it stays red on a unique's gold and a rare's yellow alike.

The label is a single `DrawString` in one colour, so the suffix is a second call. It carries its own
string and the name's own width; `ItemLabel::width` covers **both**, so the backing plate and the
label-collision solver size correctly without knowing the suffix exists. Sizing to the name alone
would have let the red digits overhang the plate and collide with the next label unseen — the same
class of bug as everything else this week.

## Verification

485 tests, the two standing baseline failures only.

**Needs eyes**, since no test renders a panel or a label: hover a loose rune and a loose gem in the
stash and check the host lines read sensibly and the panel is shorter than before; drop a socketed
item and check the red `[n]` sits inside its plate rather than spilling past it.

Wiki updated — `sockets.html` and `history.html` — and the artifact republished.
