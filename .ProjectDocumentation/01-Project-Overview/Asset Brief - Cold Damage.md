# Asset brief: Cold — missiles, impacts and armour shells

**For:** an image-generation model, or a human artist.
**Deliverable:** 13 sprite sheets. Nothing else. See §1.2 for the long list of things that look like
they are needed and are not.
**Project:** Diablo Orcl (Oracool Edition) — a single-player Diablo fork built on DevilutionX.
**Written:** 2026-08-31, from the user's request: *"introduce Cold dmg, spells and mechanics. Should
be possible. belzebub has it."*

---

## 1. What is being made, and why

This engine has **no cold**. `DamageType` is Physical, Fire, Lightning, Magic and Acid, and the game
has been apologising for the gap in player-facing text ever since the class trees were written:

> *"A shard of ice that chills what it hits. **Inert: this engine has no cold damage.**"*
> *"Armour of ice that freezes attackers. **Inert: no cold, no freeze.**"*
> *"Pierces cold resistance. **Inert: there is no cold to master.**"*

Fifteen skills are already written, named, described, positioned in their class trees and **already
have icons**. They do nothing. Adding cold turns them all on at once.

### 1.1 The fifteen skills waiting on this

| Skill | Class | Needs |
|---|---|---|
| Ice Bolt | Sorcerer | projectile + impact |
| Ice Blast | Sorcerer | projectile + impact |
| Glacial Spike | Sorcerer | projectile + **shatter** impact |
| Frost Nova | Sorcerer | ground effect |
| Blizzard | Sorcerer | area effect |
| Frozen Orb | Sorcerer | wandering orb |
| Frozen Armor | Sorcerer | armour shell |
| Shiver Armor | Sorcerer | armour shell |
| Chilling Armor | Sorcerer | armour shell |
| Cold Mastery | Sorcerer | *(passive — no art)* |
| Cold Arrow | Rogue | arrow + impact |
| Ice Arrow | Rogue | arrow + impact |
| Freezing Arrow | Rogue | arrow + **burst** impact |
| Holy Freeze | Paladin | *(aura — art already exists)* |
| Resist Cold | Paladin | *(aura — art already exists)* |

### 1.2 What is NOT needed — please read this before drawing anything

Each of these was checked in the repository, not assumed:

- **Skill icons.** All fifteen already have them. The class-tree icon strips are cut one cell per
  tree row — `sorc_tree_icons.png` is 2688×56, exactly 48 cells for 48 Sorcerer rows;
  `rogue_tree_icons.png` is 2744×56 for 49. The ice skills are among them.
- **Paladin aura ground rings.** `aura_holy_freeze.png` and `aura_resist_cold.png` both already ship
  in `oracool_assets/ui/`.
- **A cold gem, rune or charm.** Sapphire exists — it was re-purposed as the *mana* gem precisely
  because there was no cold for it to be. Re-pointing it is a data change, not an art one.
- **A "chilled" or "frozen" monster overlay.** Done in code, procedurally: the fork already builds a
  palette translation per monster and walks each colour along its own ramp (`TintVariant`,
  `monster_variants.cpp`). A chilled monster shifts toward the blue ramp the same way. **Drawing a
  frost overlay would be wasted work.**
- **Character-sheet or UI graphics for cold resistance.** Text rows, already added.

---

## 2. Hard technical constraints

Engine facts, not preferences. Art that ignores them cannot ship.

### 2.1 The palette

The game is 8-bit indexed colour. Sprites that appear on any floor may use only indices **128–255**;
0–127 are redefined per level type and colour-cycled, so anything quantised into them changes colour
from floor to floor.

| Indices | Ramp | Bright → dark |
|---|---|---|
| 128–135 | **Blue**, pure and saturated | `#9F9FFF` → `#000019` |
| 136–143 | Red | `#FF9F9F` → `#230000` |
| 144–151 | Yellow | `#FFFD9F` → `#191900` |
| 152–159 | Green, muted | `#8CBE8C` → `#041C04` |
| 160–175 | Dusty rose stone, 16 steps | `#E8CACA` → `#0C0707` |
| 176–191 | **Cool blue-grey stone**, 16 steps | `#C8CDEA` → `#05070C` |
| 192–207 | Gold / tan, 16 steps | `#FFE3A4` → `#140B00` |
| 208–223 | Orange / copper, 16 steps | `#FFE2B3` → `#0F0500` |
| 224–239 | Crimson, 16 steps | `#FFBDBD` → `#0C0505` |
| 240–255 | Greyscale, 16 steps, plus pure white at 255 | `#F3F3F3` → `#1E1E1E`, `#FFFFFF` |

**Cold has exactly two ramps to work with: the saturated blue (128–135) and the cool blue-grey stone
(176–191), with greyscale and white for highlights.**

That is the whole palette for ice. Note what it means in practice:

- **There is no cyan and no teal.** Do not draw glacier-blue or turquoise ice. It will quantise into
  the saturated blue or the blue-grey and will not look like what was drawn.
- The saturated blue ramp is only **8 steps**, and it is the same ramp the game's *magic* damage
  uses. Reserve it for the brightest cores and the hottest edge of a flash.
- The blue-grey stone ramp is **16 steps** and is the workhorse. Body, mass and shading of ice should
  live here — it reads as cold precisely because it is desaturated.
- **White is the reason ice reads as ice.** Cold's signature in this palette is white-and-pale-blue,
  not blue. Lean on 240–255.

### 2.2 Format for every sheet

- **32-bit PNG with a real alpha channel.** Not a green screen, not black-on-black. Partial
  transparency is expected and used.
- One PNG per entry in §3, named exactly as the table says.
- **Frames laid out left to right in a single row**, unless the entry says otherwise. Every frame is
  the same size, edge to edge, with no padding between them: sheet width = frame width × frame count.
- The subject is **centred in its frame** with its own margin inside it. Do not crop tight to the ink
  — the engine positions by frame centre.
- No drop shadows, no outer glow reaching the frame edge, no vignette.

### 2.3 Directional sheets: 16 facings

A projectile that flies across the world is drawn once **per direction**, 16 of them, because the
world is isometric and a bolt travelling north-east is a different picture from one travelling south.

For those entries the sheet is a **grid**, not a row:

- **16 rows, one per facing**, starting due south and rotating clockwise in 22.5° steps
  (S, SSW, SW, WSW, W, WNW, NW, NNW, N, NNE, NE, ENE, E, ESE, SE, SSE).
- **N columns, one per animation frame.**
- Sheet size = frame width × N, by frame height × 16.

The world's ground plane is squashed 2:1 (tiles are 64 × 32), so a projectile seen travelling
east–west looks **longer and flatter** than the same projectile seen travelling north–south. Draw
that foreshortening; do not simply rotate one sprite sixteen times.

### 2.4 Sizes

Match the sizes the engine already uses for comparable effects, taken from `MissileSpriteData`:

| Kind | Frame | Precedent |
|---|---|---|
| Bolt / arrow | **96 × 96** | Firebolt, Fire Arrow |
| Heavier projectile, orb | **128 × 128** | Fireball, magma ball |
| Ground / area effect | **128 × 128** | Fire Wall |
| Big flash or nova | **160 × 160** | the blue flash pair |

---

## 3. The thirteen sheets

Frame counts are the engine's own animation lengths for the effect each one is modelled on, so they
can be dropped in without retiming.

### 3.1 Projectiles — directional, 16 rows

| # | File | Frame | Frames | What it is |
|---|---|---|---|---|
| 1 | `ice_bolt.png` | 96 × 96 | 16 | A single shard of ice, sharp end forward, faint frost trail. The cold twin of Firebolt — small, fast, unremarkable, the one you cast a thousand times. |
| 2 | `ice_blast.png` | 96 × 96 | 16 | Heavier than the bolt: a fist-sized chunk with visible facets, a brighter core, a short crystalline wake. |
| 3 | `glacial_spike.png` | 128 × 128 | 16 | A long spear of ice, clearly heavy, tumbling slightly. The biggest single projectile in the set. |
| 4 | `frost_arrow.png` | 96 × 96 | 4 | An ordinary arrow sheathed in frost, a thin vapour trail behind it. **One sheet serves all three Rogue arrows** — Cold, Ice and Freezing differ in what they do, not what they look like in flight. Four frames, matching Fire Arrow. |
| 5 | `frozen_orb.png` | 128 × 128 | 16 | A slowly rotating sphere of packed ice, shedding shards. It hangs and wanders rather than flying straight, so the animation should read as *turning in place*, not as travelling. |

### 3.2 Impacts — non-directional, one row

These play where a missile lands. One row of frames; no facings.

| # | File | Frame | Frames | What it is |
|---|---|---|---|---|
| 6 | `ice_impact.png` | 96 × 96 | 10 | The default cold hit: a small burst of frost and splinters, fading to vapour. Used by Ice Bolt, Ice Blast and the arrows. Modelled on the magma-ball explosion's timing. |
| 7 | `glacial_shatter.png` | 128 × 128 | 12 | The spike breaking apart — chunks thrown outward, then dust. Bigger and slower than #6; this is the one the player should feel. |
| 8 | `freezing_burst.png` | 128 × 128 | 12 | Freezing Arrow's landing: a low outward puff of frost across the ground, wider than it is tall (remember the 2:1 squash). |

### 3.3 Ground and area effects — non-directional, one row

| # | File | Frame | Frames | What it is |
|---|---|---|---|---|
| 9 | `frost_nova.png` | 160 × 160 | 19 | A ring of ice bursting outward from the caster and fading. **Drawn as a 2:1 ellipse** — it lies on the floor. Expands from nothing to the full frame. Matches the blue flash's 19 frames. |
| 10 | `blizzard_shard.png` | 128 × 128 | 13 | ONE falling shard: enters at the top of the frame, strikes the ground low in it, breaks. The game scatters many of these across an area with staggered timing — do not draw a whole storm, draw one piece of it. |
| 11 | `ice_ground.png` | 128 × 128 | 2 | A patch of frozen floor left behind, in two variants (the sheet's two frames), which the game tiles and fades. Flat, 2:1, subtle — this sits under monsters and must not hide them. |

### 3.4 Armour shells — non-directional, one row, looping

Worn by the caster. They must not obscure the character: think a thin shell at the silhouette's edge,
not a solid coating.

| # | File | Frame | Frames | What it is |
|---|---|---|---|---|
| 12 | `ice_armor_shell.png` | 96 × 96 | 8 | A slow shimmer of ice at the body's outline, seamlessly looping. **One sheet serves all three armours** — Frozen, Shiver and Chilling differ in what they do to an attacker, not in how they look. The game tints the three apart in code. |
| 13 | `ice_armor_break.png` | 96 × 96 | 10 | The shell cracking and falling away when the armour triggers or expires. |

---

## 4. Art direction

**The reference is Diablo II, seen through Diablo I's palette.** Not stylised, not modern, not
painterly-with-soft-gradients. Hard-edged, high-contrast, readable at a glance against a dark stone
floor at 1× zoom.

- **Ice is white with blue in its shadows**, not blue with white highlights. That is the single most
  important note on this page. Blue-dominant ice will read as magic damage, which this engine already
  has and colours blue.
- **Sharp, faceted, angular.** Straight edges and clean breaks. Ice in this game is broken glass, not
  frosted plastic.
- **Vapour is white and thin**, and it dissipates rather than billowing.
- **Every effect must read at 96 pixels while moving.** Fine internal detail is lost; silhouette and
  contrast are all that survive. Squint at it — if it becomes a grey blob, it is too detailed.
- **No text, no runes, no symbols**, unless the entry asks for them. None do.

---

## 5. What I do with them

Stated so the boundary is clear.

1. **A PNG loader for missiles.** The engine loads missile graphics as CL2 sheets out of the archive
   and has no PNG route for them. The fork already solved exactly this problem for *player* sprites —
   `oracool/sprite_import.h` lets a class ship PNG sheets instead of CL2s — and the missile side is
   the same technique applied to a different loader. **This is mine to write; it does not affect what
   is drawn.**
2. **The `DamageType::Cold` enumerator**, its resistance channel, its place in the difficulty penalty
   and resistance curve, its slot in the save format, and its `IPL_` item powers.
3. **The chilled state**, procedurally, per §1.2.
4. Re-pointing the fifteen skills at real missiles and deleting fifteen apologies from their
   descriptions.

Deliver the thirteen PNGs into the `Oracool.MPQ` root, as usual, and I will sweep them from there.

---

## 6. Checklist before delivery

- [ ] 13 PNGs, named exactly as in §3.
- [ ] 32-bit PNG, real alpha. No green screen, no black background.
- [ ] Directional sheets (#1–5) are 16 rows × N columns, south first, clockwise.
- [ ] All other sheets are a single row.
- [ ] Frame sizes exactly as tabled; sheet dimensions are an exact multiple of them, no padding.
- [ ] Colour lives in the saturated blue (128–135), the blue-grey stone (176–191), and greyscale/white
      (240–255). **No cyan, no teal, no purple.**
- [ ] White-dominant, blue-shadowed — not blue-dominant.
- [ ] Ground effects (#8, #9, #11) drawn as 2:1 ellipses, not circles.
- [ ] Nothing touches the frame edge except where the entry says it expands to fill.
