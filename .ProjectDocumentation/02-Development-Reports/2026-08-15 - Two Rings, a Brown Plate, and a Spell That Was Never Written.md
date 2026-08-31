---
date: 2026-08-15
version: 1.5.61
area: Abilities window colour coding / Etherealize
---

# Two Rings, a Brown Plate, and a Spell That Was Never Written

Three requests. The middle one is the interesting one.

## 1. Red for the left button, yellow for the right

> We need RED outline on a skill/spell when it is used in LMB, just like we need the existing YELLOW
> outline to inform us a skill/spell is assigned to RMB. These color code outlines to work across all
> ability sheets.

The old marker was a single 2px yellow border drawn by `DrawSpellRow` and nowhere else, so it only
ever appeared on the Spells and Class Skills sheets — the Skills sheet's rows, which are icon-only,
had no marker at all.

`DrawAssignmentRings` now takes an icon rect and both readied pairs, and every row kind calls it. The
one design decision worth recording: **each button owns a ring position as well as a colour.** Right
is always the outer ring, left always the inner one. If the placement were "whichever ring is free",
a ring would jump outward when the other assignment was cleared, and the same spell on both buttons
would show one marker hiding the other. Fixed positions make both legible at once and nothing moves.

Two rows get rings that are not spells:

- **Regular Attack** is ringed for whichever button holds no spell. That row *means* "this button
  swings", and clicking it is how you assign that — so a button sitting on it should say so, the
  same as any other row.
- **Fist Attack** never is. Which of the two attack icons you get is decided by what is in your hand,
  not by a click, and its row exists to say so.

The rings reuse the outline primitive the hover highlight already drew through, split into
`DrawColoredOutline` with `DrawHoverOutline` as its gold caller. That matters for a reason that is
easy to miss: it draws through the engine's line primitives, so a row half-scrolled out of the
content subregion is **clipped**. The vanilla `DrawSmallSpellIconBorder` the old marker used goes
through `UnsafeDrawBorder2px`, which is not.

## 2. Etherealize crashed, and it was a null function pointer

> i was testing spells one by one. Game crushed at Etherealize. All in the list above - no problems.

`AddMissile` ends with:

```cpp
missileData.mAddProc(missile, parameter);
```

Unconditional. And `MissileID::Etherealize`'s `mAddProc` is `nullptr`.

Etherealize is cut content. Vanilla defined the spell, defined its missile, defined
`SpellFlag::Etherealize`, wired that flag into three places that make the player untouchable —
arrows pass through, monsters miss, melee misses — and cleared it in `InitMissiles`. What it never
wrote is the half that would ever **set** it. The spell was unreachable, so the missing caster never
mattered.

It became reachable when this fork gave books to the fifteen bookless spells. So the crash is ours,
in the sense that we made the road; the hole in it was already there.

Two fixes, and both were worth doing:

**The spell now works.** `AddEtherealize` sets the flag and starts a timer; `ProcessEtherealize`
re-asserts the flag each tick and clears it at zero. Re-asserting rather than setting once is not
belt-and-braces: `InitMissiles` clears the flag on every level entry and the missile survives that,
so without it a staircase would silently end the effect while the timer kept running.
`ProcessInfravision` does the same thing for the same reason. Duration is a tenth of Infravision's
base — about eight seconds at spell level 1 — because the description we wrote for it says "briefly
untouchable", and this is untouchable, not merely dark-sighted. The missile also gained the
`Invisible` flag, since its graphic entry has an empty filename and nothing should try to draw it.

**And `AddMissile` no longer calls through a null pointer.** A missile with no add proc is one vanilla
defined and never spawned; it is now treated as a fizzle. `mProc` was already guarded a few hundred
lines below — only `mAddProc` was not, which is what made this an asymmetry rather than a policy.

I audited the rest before fixing: of the sixteen missiles with a null `mAddProc`, Etherealize is the
**only** one any spell points at. So this was the one crash of its kind, which matches the report —
every spell before it in the list was fine.

## 3. Brown for the Skills sheet

> **Superseded the same day, at 1.5.62.** The colour is unchanged; its NAME is now *pink*, on the
> user's reading of the screen, and the Auras and Barbarian sheets took it too. See
> *Pink Is What It Looks Like*.


> we need to come up with a color of the background of the skills (the not the CLASS SKILLS). Spells
> have Blue. Class Skils have YELLOW. Maybe we make SPECIFIC SKILLS brow or green or dark blue or
> something else?

Brown — and the palette chose it, not taste.

The plate is recoloured through the engine's spell TRNs, which can only reach the six 16-shade ramps
the game ships: beige, blue, yellow, orange, red, grey. Five are spoken for.

| Ramp | Already means |
|---|---|
| Blue | Spells |
| Yellow | Class Skills |
| Grey | Cannot cast / not learned |
| Orange | A staff's charges |
| Red | Left-button assignment, as of item 1 above |
| **Beige** | **free — the Skills sheet** |

Green was never available: **there is no green ramp in the palette.** Dark blue would have collided
with spells. So beige it is, which reads as brown against the yellow beside it.

The mechanism is `SkillPlateTint`, a two-value enum rather than a raw `SpellType`, because the call
site should say "brown" and not "scroll". Internally brown *is* `SpellType::Scroll`'s translation
table — that TRN is named for the thing it was written to colour and is used here for the colour
alone. The Auras and Barbarian sheets keep the yellow plate for now; they were not named in the
request and are sheets of their own.

## State

352/354, the standing baseline (`Drlg_l1.CreateL5Dungeon_diablo_3_844660068` and
`Timedemo.WarriorLevel1to2`, both failing before this change).

Not tested in-game — the user runs the game.
