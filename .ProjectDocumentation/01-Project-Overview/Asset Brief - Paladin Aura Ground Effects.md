# Asset brief: Paladin aura ground effects

**For:** an image-generation model, or a human artist.
**Deliverable:** 20 ground-glow images, one per Paladin aura.
**Project:** Diablo Orcl (Oracool Edition) — a single-player Diablo fork built on DevilutionX.
**Written:** 2026-08-25.

---

## 1. What is being made, and why

In Diablo II, a Paladin with an aura lit stands inside a glowing ring on the floor. It is how the
player knows the aura is on, and which one. This game has 20 Paladin auras and **no such effect** —
an active aura is currently visible only as a lit icon in the interface.

These 20 images are that ring: a flat pool of light on the ground, centred on the character, that
moves with them.

> **Status, stated plainly:** the code that would draw this does not exist yet. This brief is the
> specification it will be written against, so art produced to it will fit. It is not describing
> something the game already loads.

---

## 2. Hard technical constraints

These are engine facts, not preferences. Art that ignores them cannot ship.

### 2.1 The palette — the most important constraint on this page

The game is 8-bit indexed colour. A sprite that appears **both in town and in the dungeon** — which
this does — may only use palette indices **128–255**, because indices 0–127 are redefined for every
level type and colour-cycled. Art quantised outside that range comes out a different colour on every
floor.

The shared upper half contains exactly these ramps. **There is nothing else available.**

| Indices | Ramp | Bright end → dark end |
|---|---|---|
| 128–135 | **Blue**, pure and saturated | `#9F9FFF` → `#000019` |
| 136–143 | **Red**, pure and saturated | `#FF9F9F` → `#230000` |
| 144–151 | **Yellow**, pure and saturated | `#FFFD9F` → `#191900` |
| 152–159 | **Green**, muted and sage-like | `#8CBE8C` → `#041C04` |
| 160–175 | **Dusty rose stone**, 16 steps | `#E8CACA` → `#0C0707` |
| 176–191 | **Cool blue-grey stone**, 16 steps | `#C8CDEA` → `#05070C` |
| 192–207 | **Gold / tan**, 16 steps | `#FFE3A4` → `#140B00` |
| 208–223 | **Orange / copper**, 16 steps | `#FFE2B3` → `#0F0500` |
| 224–239 | **Crimson**, 16 steps | `#FFBDBD` → `#0C0505` |
| 240–255 | **Greyscale**, 16 steps, plus pure white at 255 | `#F3F3F3` → `#1E1E1E`, `#FFFFFF` |

**There is no purple, no cyan, no magenta, no pink beyond the dusty rose, and no teal.** Do not
produce them; they will be crushed into the nearest ramp and will not look like what was drawn. The
green is muted — it cannot do neon or acid.

### 2.2 Shape: everything on the floor is a 2:1 ellipse

The world is isometric with 64 × 32 pixel tiles. A **circle** on the ground therefore projects to an
**ellipse twice as wide as it is tall**. A round ring will read as a ball hovering in the air.

Draw the ring as an ellipse with a 2:1 width-to-height ratio. Anything laid on the ground —
lettering, runic marks, radiating spokes — obeys the same squash.

### 2.3 Size

An aura reaches 4 tiles at one point invested, growing to a cap of 8 tiles. Draw every image at the
**maximum** radius; the game will scale down for smaller ones, so scaling only ever shrinks.

- **Canvas: 512 × 256 pixels.**
- The ellipse's outer edge touches all four canvas edges. The character stands at the exact centre,
  (256, 128).

### 2.4 Format

- **32-bit PNG with a real alpha channel.** Not a green screen, not a black background — the ring
  is a translucent glow lying over a dungeon floor, and it needs genuine partial transparency to sit
  on stone without looking like a decal.
- No drop shadow, no outer glow bleeding past the canvas edge, no vignette.
- One file per aura. Static — **no animation frames.** Motion (a slow pulse in brightness, a slow
  rotation) is applied in code, so the art stays one image per aura.

### 2.5 What must not be in the image

- **No text, no names, no numbers.** The game draws the aura's name itself, in a translated font.
- **No character, no armour, no Paladin.** The player sprite is drawn separately, standing in the
  middle of this.
- **No square border, no card frame, no icon plate.** This is not an icon. These already exist.
- Nothing outside the ellipse except transparency.

---

## 3. Art direction

Diablo's first game, not its third: **grim, dim, and physical.** These rings are light falling on
wet stone, not a modern game's neon UI overlay.

- **Dark centre, bright edge.** The ring should be strongest at its rim and fade toward the middle,
  so the character is never obscured. Roughly: fully transparent for the inner half, rising to
  strongest at 85–100% of the radius.
- **Two elements at most.** A rim, and one motif. Diablo's floors are busy; a third element becomes
  noise at play size.
- **Low overall opacity.** Peak alpha around 60–70%, not 100%. This lies *on* the floor; the stone's
  texture should still read through it.
- **Hand-painted, not vector.** Slightly irregular edges, some grain. A crisp mathematical ring
  looks wrong against Diablo's painted tiles.
- **No lens flares, no bloom, no chromatic aberration, no modern glow.**

Where two auras share a colour ramp (fire twice, cold twice), **distinguish them by motif, not by
hue** — the ramps available do not permit a second orange.

---

## 4. The twenty auras

Each row gives the aura's real in-game description, the palette ramp to build it from, and a motif.
The motif is a suggestion; the ramp is not.

Six are marked **INERT** — the skill exists in the tree and does nothing yet, because this engine
lacks the mechanic. Art for them is still wanted (it will be waiting when the mechanic lands), but
they are the ones to skip if the set is being trimmed.

### Offensive auras

| # | Aura | What it does in game | Ramp | Motif |
|---|---|---|---|---|
| 1 | **Might** | Increases the damage you deal. | Crimson 224–239 | A heavy, blunt-edged ring. Thick and simple — brute force, no ornament. |
| 2 | **Holy Fire** | Wreathes your weapon in flame, adding fire damage to every blow. | Orange 208–223 | Licking flame tongues around the rim, leaning outward. |
| 3 | **Thorns** | Returns damage to whatever strikes you. A flat return in this engine. | Grey 240–251 rim, Red 136–143 tips | Barbs and spines pointing outward from the ring, iron-grey with bloodied points. |
| 4 | **Blessed Aim** | Steadies your hand, raising your chance to hit. | Yellow 144–151 | A thin, exact ring with fine radial tick marks, like a sight or a dial. |
| 5 | **Concentration** | Raises damage and steadies you against interruption. | Gold 192–207 | Two concentric rings, inner one tight and unwavering. Stillness. |
| 6 | **Holy Freeze** *(INERT)* | Chills nearby enemies and adds cold damage. | Blue 128–135 | Frost crystals creeping inward from the rim; the ring's edge fractured like ice. |
| 7 | **Holy Shock** | Charges your weapon, adding lightning damage to every blow. | Blue 128–131 with White 255 | Jagged arcs stuttering around the rim, white at their hottest points. |
| 8 | **Sanctuary** | Hallows the ground: nearby undead break and flee. Champions are too proud to run. | Greyscale 240–247, near-white | A clean, cold, consecrated ring. Pale and severe. Faint radial beams. |
| 9 | **Fanaticism** | Drives you to strike faster, harder and truer. | Red 136–143 | Aggressive, uneven, flickering. The rim broken into fast slashing strokes. |
| 10 | **Conviction** | Strips enemy resistances; at five points, breaks immunities down into resistances. | Crimson 232–239, very dark | An oppressive, heavy ring that *darkens* the floor rather than lighting it. Ragged inner edge, as if eating the ground. |

### Defensive auras

| # | Aura | What it does in game | Ramp | Motif |
|---|---|---|---|---|
| 11 | **Prayer** | Mends your wounds steadily as you walk. | Gold 192–199, warm | A soft, kindly glow. Gentlest ring of the set — barely a rim at all, more a pool. |
| 12 | **Resist Fire** | Hardens you against fire. | Orange 208–223 | A *defensive* wall: flames turned inward-facing, a barrier rather than an attack. Contrast with Holy Fire's outward licks. |
| 13 | **Defiance** | Raises your armour class. | Cool stone 176–191 | Overlapping shield scales or plates laid flat around the rim. |
| 14 | **Resist Cold** | Hardens you against cold. No cold exists here, so it wards against magic instead. | Blue 128–135, paler end | Smooth, glassy, unbroken — the opposite of Holy Freeze's fracturing. |
| 15 | **Cleansing** *(INERT)* | Shortens poison and curses. | Green 152–159 | A dissipating ring, its edge breaking into rising motes that thin as they leave. |
| 16 | **Resist Lightning** | Hardens you against lightning. | Yellow 144–151 | A grounded, contained ring — arcs running *along* the rim rather than escaping it. |
| 17 | **Vigor** | Quickens your stride: you run instead of walking, wherever you are. | Green 152–155, bright end | Motion streaks trailing around the ring, as if it is being dragged. |
| 18 | **Meditation** | Restores your mana steadily as you walk. | Blue 128–135 | Slow concentric pulses moving inward toward the centre. Calm. |
| 19 | **Redemption** *(INERT)* | Consumes the fallen for life and mana. | White 255 with Gold 192–199 | Small motes drifting from the rim toward the centre and being consumed. |
| 20 | **Salvation** | Wards you against fire, lightning and magic alike. | Gold 192–207 with Blue 128–135 and Orange 208–215 | Three interwoven bands, one per element, braided into a single ring. The most ornate of the set. |

*(Three further Paladin auras exist in some versions of Diablo II and are **not** in this game:
Aura Mastery, Holy Bolt-based auras, and Resist Magic as a separate line. Do not produce them.)*

---

## 5. Delivery

- 20 files, PNG, 512 × 256, 32-bit with alpha.
- Named exactly: `aura_might.png`, `aura_holy_fire.png`, `aura_thorns.png`, `aura_blessed_aim.png`,
  `aura_concentration.png`, `aura_holy_freeze.png`, `aura_holy_shock.png`, `aura_sanctuary.png`,
  `aura_fanaticism.png`, `aura_conviction.png`, `aura_prayer.png`, `aura_resist_fire.png`,
  `aura_defiance.png`, `aura_resist_cold.png`, `aura_cleansing.png`, `aura_resist_lightning.png`,
  `aura_vigor.png`, `aura_meditation.png`, `aura_redemption.png`, `aura_salvation.png`.
- Drop them in the MPQ root drop zone, as with all other incoming art.

If the generator cannot hold the palette, produce the art anyway in full colour **within the hue
families listed** — the quantiser will map it, and staying inside the families is what makes that
mapping faithful. What cannot be recovered is a hue that has no ramp at all.

---

## 6. One-paragraph version, for pasting into a prompt

> A flat circular glow lying on a stone dungeon floor, seen from Diablo 1's isometric camera — so it
> is an ellipse exactly twice as wide as it is tall, 512 × 256 pixels, on a fully transparent
> background. Dark and empty at the centre, brightening toward the rim, peak opacity about 65% so
> the floor reads through it. Hand-painted, grainy, grim, low-fantasy — the light of Diablo 1, not a
> modern game's neon. No text, no character, no frame, no border, nothing outside the ellipse. The
> colour and motif are: **[insert the aura's ramp and motif from the table above]**.
