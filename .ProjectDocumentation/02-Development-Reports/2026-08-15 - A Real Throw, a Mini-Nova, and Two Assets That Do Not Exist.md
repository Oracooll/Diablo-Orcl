---
date: 2026-08-15
version: 1.5.78
area: Paladin skills / Blessed Shield and Fist of the Heavens
---

# A Real Throw, a Mini-Nova, and Two Assets That Do Not Exist

The last two items of the user's feedback list. Both mechanics are now as specified. Two pieces of
art are not, and this report is mostly about why.

## What the research found

Three of the four unknowns resolved better than expected, and one resolved badly.

**Nova's lightning already travels exactly 4 tiles.** `ProcessNovaCommon` fires its bolts at a
radius-4 ring — the `quarterRadius` array is literally `{4,0}` through `{0,4}`, a quarter arc
mirrored into four. So "travel distance of lightnings of 4 tiles" is vanilla geometry, and needed no
work at all.

**A shrunken Nova bolt already ships.** `MissileGraphicID::ChargedBolt`'s art file is `miniltng` —
mini lightning. `MissileID::MiniNovaBall` is NovaBall's own add and process functions with that
sprite, which is the whole of "shrunken down animation of Nova".

**The SORT button's sound is `IS_ISHIEL`**, the shield-into-slot sound, confirmed at inv.cpp:2485
and stash.cpp:462. It now plays where the fist lands.

**Neither the falling mace nor the spinning shield exists.** This is the one that resolved badly, and
it is worth being precise rather than approximate:

- I enumerated **all 42 missile sprites** in `MissileSpriteData`. There is no mace, no hammer and no
  shield among them. The full list is arrows, bolts, flares, explosions, portals, runes and
  monster-specific effects.
- The mace and shield exist as **item art**: static inventory/ground icons in the cursor sheet
  (`items-mace.png`, `items-shield.png` in the vault's asset plan — Buckler through Gothic Shield,
  2×3 cells each). A ground item has no animation frames, and a missile needs directional frames.
- The other mace art is **player-held weapon frames** (`heavy-mace-attack` and friends), which are
  the character swinging it, not the object falling.

So the user's "there is an animation for that" is, as far as the shipped assets go, not the case.
Saying so is more useful than shipping something that borrows an unrelated sprite and calling it a
mace.

## What was built

**Blessed Shield is a real throw now.** The previous version dropped blasts on several enemies at
once — it delivered damage to a crowd but never actually threw anything.
`MissileID::BlessedShieldThrow` leaves the hand at `HolyBoltSpeed * 2`, written as the doubling
rather than as `32` so the relationship survives a change to either, and bursts into a ring of
one-tile blasts where it lands — which is "splash dmg with range 1 on hit".

**Fist of the Heavens lands and then discharges.** Impact blast, `IS_ISHIEL`, then 36 mini-Nova bolts
on the radius-4 ring laid out exactly as `ProcessNovaCommon` does — the quarter arc mirrored into
four, which is what gives Nova its round front instead of a square one. Damage on every part comes
from the weapon.

## The two placeholders, stated plainly

| Wanted | Shipped | Why |
|---|---|---|
| A shield sprite, spinning, brightened | `MissileGraphicID::HolyBolt` | No shield missile art exists; the item shield is a static icon with no frames to spin. Holy's bright bolt is at least the right register for a *blessed* throw. |
| A mace falling to the ground | the impact blast alone | No falling-mace animation exists in any missile, object or item sprite. |

Both are one line to change once art exists. Making them properly means building a **missile art
pipeline** — extract, recolour, pack as a CL2/CLX with directional frames, ship in `oracool.mpq`,
name it in `MissileSpriteData`. That is a real piece of work, comparable to the player-sprite
importer already in `tools/`, and it did not belong inside a mechanics change.

## State

354/356, the standing baseline.

Not tested in-game — the user runs the game. Worth watching: whether the thrown shield's speed reads
as fast enough at 2×, and whether 36 mini-bolts is too dense a ring now that they are small.
