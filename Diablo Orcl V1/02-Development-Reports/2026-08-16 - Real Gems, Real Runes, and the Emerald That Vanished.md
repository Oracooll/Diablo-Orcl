---
date: 2026-08-16
version: 1.7.8
area: Gem and rune art from the user's sheets; runes to Diablo II's exact numbers
---

# Real Gems, Real Runes, and the Emerald That Vanished

The socket economy shipped in 1.7.0 wearing borrowed clothes - grey vanilla rocks standing in
for gems and runes. This unit replaces every stand-in with the user's own art and finishes the
rune mechanics to Diablo II's sheet.

## The art

Two source sheets, both green-screen renders in the Oracool.MPQ folder:

- **Gems** (`item-gems-v1.png`, from the ChatGPT render): a 7x5 grid of gem varieties. We cut
  row 3 - the classic oval cuts - for our five: Ruby, Sapphire, Topaz, Emerald, Skull.
- **Runes** (`item-runes-v1.png`, pasted mid-session): all 33 D2 runes on stone tablets, names
  legible at 28px. We cut our five (El, Tir, Ral, Ort, Sol); the other 28 are waiting in the
  sheet whenever higher rungs ship.

Ten new 28x28 frames (ICURS 372-381) through the established green-key pipeline, appended to
`oracool_items.cel`, size tables extended in cursor.cpp, item rows repointed. The Hellfire rune
icons and the three unused-vanilla-icon stopgaps are superseded.

## The emerald that vanished

The first cut produced nine perfect icons and one empty square: the emerald. A green gem on a
green-screen background is invisible to a chroma key - the extractor ate it whole. Its cell is
now pre-processed (backdrop to black, `item-gems-emerald-dark-v1.png`) and cut with the dark-mode
extractor instead.

Then it came out grey: **town.pal contains no green** - the same fact that forced the skill
plates' palette-injected green ramp. The fix follows the same logic to its conclusion: the
pipeline's quantization palette now carries the exact 8-shade forest ramp the engine injects at
152-159 into every in-game palette, so quantization finally matches what the game actually
displays. (`tools/town.pal` is now committed - it had been a loose file, and it took the
pipeline down when it vanished.)

## Runes to D2's numbers

Per the directive "make them same as D2", from the Arreat Summit sheet:

| Rune | Weapon | Armor | Shield | Everywhere |
|---|---|---|---|---|
| El | +5% to-hit (50 AR) | +15 defense | +15 defense | +1 light radius |
| Tir | - | - | - | +2 mana per kill |
| Ral | 5-30 fire damage | FR +30% | FR +35% | - |
| Ort | 1-50 lightning damage | LR +30% | LR +35% | - |
| Sol | +9 damage | Damage Reduced 7 | Damage Reduced 7 | - |

Two documented adaptations: El's Attack Rating maps at D2's own ~10 AR : 1% convention, and
Sol's min-only damage is flat +9 (D1's damage roll cannot survive min crossing max on small
weapons). Damage Reduction rides the vanilla beneficial-negative getHit channel; light radius
feeds the existing totals field with its 2-15 clamp.

**Tir needed a new mechanism**: mana-on-kill has no totals field to live in. `RuneManaPerKill`
(gems.cpp) sums Tir sockets across usable worn items; MonsterDeath grants it to the local player
on every real kill (minions excluded, mirroring the telemetry hook), honoring the mana-steal
path's NoMana guard, clamped to max, orb redrawn.

Socket description lines are now multi-part ("El: +15 armor, +1 light radius") since the runes
stack several effects on one host - the old first-nonzero-field logic showed only one.

## Verified

396 tests, the usual two pre-existing failures. New pins: every D2 rune number per host, DR's
negative getHit, Tir's stacking (2 Tirs = +4), unusable items' sockets staying inert, and only
Tir carrying mana-per-kill.
