---
date: 2026-08-15
version: 1.6.16
area: Palette / skill plates / gold
---

# The Green the Palette Never Shipped

User: *"belzebub has green text so there must be a way to alter the engine to produce other colors.
belzebub also has gold stacks more than 65000 gold. they found a way. so should we."*

Right on both counts - and one of the two was already done.

## Green: the palette is ours to edit

Every earlier colour conversation in this fork treated the palette's six ramps as a wall. Belzebub
is the existence proof they are a **convention**: the palette is 256 RGB entries we load from .pal
files, and a mod that controls its own pipeline can rewrite them and point a translation table at
the result.

**The donor was chosen by measurement.** Entries 128-159 turn out to be four bright 8-shade
mini-ramps - blue, red, yellow, orange. Counting pixel usage across four real gameplay screenshots:
the blue run carries ~23,000 pixels (lightning, water, waypoints), red ~12,000 (fire), while the
bright-yellow run (PAL8_YELLOW, 144-151) carries ~2,700 - most of them its near-black darkest shade.
That run is now the **green ramp**, injected by `LoadPalette` on every palette load (no .pal files
edited, no MPQs repacked), each green shade keeping its donor's brightness so anything unaudited
shifts hue rather than structure.

**The two known consumers of the old bright yellow were re-pointed, not broken:**
- the automap's bright lines (`MapColorsBright`) now draw from the big PAL16_YELLOW ramp
- the RMB assignment ring likewise
- the class-skill plates' three bright accent pixels are painted in indices 144-146 inside the CEL,
  so `SetSpellTrans(Skill)` - which used to be the identity - now remaps that trio onto the big
  yellow ramp, keeping those plates pure yellow

**The plates wear it.** `SetSpellTransGreen` maps the plate art's ramps onto the eight greens (two
source shades per green), `SkillPlateTint::Pink` is renamed `Green` and every sheet and well path
follows. Downstream adapts on its own: the HUD PNG quantizer and the divine TRN both watch
`orig_palette` and rebuild when it changes.

**Free side effect:** the palette now contains green, so green TEXT (Belzebub's trick) is one small
font-TRN away whenever it is wanted.

## Gold: already done, and further than Belzebub

The fork's Gold Stacks Buff already sets the single-player per-stack cap to
**`GoldStackSaveLimit = 100,000,000`** - three orders of magnitude past Belzebub's 65k. The uint16
`ItemPack.wValue` field everyone assumes is the ceiling was traced when that buff was built and
proven non-authoritative: single-player's real save path stores `_ivalue` as int32, and
`LoadMatchingItems` overwrites the compact copy on every load. On top of that, most gold never sits
in stacks at all - pickups and sale proceeds route to the shared Stash pool, which holds up to
~2.1 billion.

What was missing was a **pin**: a new regression test asserts that `CalcPlrInv` in single-player
sets `MaxGold` to the full 100M, so nobody can quietly re-tie the cap to the 16-bit field.

## Watch in game

The green's donor entries had ~2,700 stray pixels across four frames - mostly near-black - so some
spark or highlight somewhere may now read faintly green. If anything looks wrong, a screenshot pins
it immediately: the palette index will say exactly which shade to adjudicate.

## State

**368/370** - the suite grew by the gold pin; the same two pre-existing failures.
