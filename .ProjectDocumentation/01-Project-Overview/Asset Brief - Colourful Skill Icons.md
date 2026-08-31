# Asset Brief — Colourful Skill Icons

**Goal:** replace every skill icon in the game with a colourful, illustrated icon in the Diablo III
manner. The current set is effectively greyscale and reads as a wall of identical grey tiles.

This brief is the **specification**. Its companion, *Skill and Spell Reference*, is the **content** —
every skill, its frame number, and what it depicts. You need both.

The same workflow already succeeded once: *Asset Brief — Paladin Aura Ground Effects* produced
twenty ground rings that shipped and work. This is the same pipeline at larger scale.

---

## 1. What we are replacing, and how much of it

Measured, not estimated. Every current strip is essentially colourless:

| Strip | Frames | Mean saturation | Vivid pixels |
|---|---:|---:|---:|
| `paladin_tree_icons.png` | 31 | 0.012 | 0.0% |
| `barb_tree_icons.png` | 30 | 0.006 | 0.0% |
| `sorc_tree_icons.png` | 30 | 0.015 | 0.0% |
| `rogue_tree_icons.png` | 30 | 0.012 | 0.0% |
| `bard_tree_icons.png` | 21 | 0.073 | 0.0% |
| `monk_tree_icons.png` | 21 | 0.053 | 0.0% |

Zero vivid pixels in the entire icon set. That is the problem in one number.

**Total required: 273 icons** — the six strips above must also GROW, because each class gained a
Passive Skills page that has no art at all:

| Class | Existing | New passives | Total frames needed |
|---|---:|---:|---:|
| Paladin | 31 | 18 | **49** |
| Barbarian | 30 | 19 | **49** |
| Sorceress | 30 | 18 | **48** |
| Rogue | 30 | 19 | **49** |
| Bard | 21 | 18 | **39** |
| Monk | 21 | 18 | **39** |

Plus `paladin_skill_icons.png` (7 frames, 38×38) and `attack_icons.png` (2 frames, 38×38).

The 53 **book spells** (Firebolt, Healing, Teleport, …) currently use Blizzard's own icons out of
`diabdat.mpq`. Replacing those needs a small engine change first and is a later phase — see §8.

---

## 2. Hard technical constraints

These are engine facts. Art that ignores them cannot ship.

### 2.1 The palette — the single most important constraint

The game is 8-bit indexed colour. Anything shown in both town and dungeon — which every icon is —
may only use palette indices **128–255**, because 0–127 are redefined per level type and
colour-cycled. Colours outside this gamut are snapped to the nearest entry and come out wrong.

**The entire available gamut:**

| Indices | Ramp | Bright → dark |
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

**Read this honestly before starting.** Diablo III's colour language leans hard on hues this engine
does not have: arcane purple, ice cyan, poison acid-green, necrotic magenta. **There is no purple,
no cyan, no magenta, no teal, and no pink beyond the dusty rose. The green is muted and cannot do
neon or acid.**

So we cannot literally reproduce D3's palette, and pretending otherwise produces art that gets
crushed into the nearest ramp and looks nothing like what was drawn.

What we CAN do — and what makes this worth doing — is go from **zero** saturated colour to full use
of four saturated ramps plus gold, copper and crimson. That is an enormous change from the table in
§1. Substitutions that work:

| D3 reaches for | Use instead |
|---|---|
| Arcane purple | Saturated **blue** (128–135) with white cores |
| Ice cyan | **Cool blue-grey** (176–191) with white highlights |
| Poison acid-green | **Muted green** (152–159) darkened, with yellow (144–151) speckle |
| Necrotic magenta | **Crimson** (224–239) with dusty rose (160–175) |
| Holy gold | **Gold/tan** (192–207) — this one is exact and should be used freely |
| Fire orange | **Orange/copper** (208–223) into yellow — also exact |

### 2.2 Alpha is a hard cutoff — there is no anti-aliasing

At load, every pixel with **alpha < 128 becomes fully transparent** and every pixel with alpha ≥ 128
becomes **fully opaque**. There is no partial transparency anywhere in an icon.

This is currently being got wrong. In a sample cell of the Paladin strip: 2,094 px fully
transparent, 466 px fully opaque, and **576 px of partial alpha** — 18% of the cell — every one of
which is being hard-cut. That is why the present icons read as thin and ragged.

**Consequence: do not rely on soft edges, glows that fade to nothing, or feathered shadows.** They
will be sliced at the halfway line into a jagged silhouette.

### 2.3 Draw full-square illustrations, not floating emblems

This solves §2.2 and is more like D3 at the same time. Diablo III's skill icons are **full square
illustrations** — a scene in a frame — not a shape floating on nothing. The current icons are
floating emblems, 67% of each cell empty, which is exactly why their cut edges are visible.

**Specification:**

- The cell is **56 × 56** pixels.
- The illustration is **full-bleed within the middle 48 × 48**, fully opaque, with a **4-pixel fully
  transparent margin on all four sides**.
- That margin is not decoration: the game draws a coloured plate behind each icon and the border is
  how the plate shows through. **The plate's colour is the skill's state** — green means active or
  paid for, red means available, grey means locked. Cover the margin and the player loses that.
- Inside the 48 × 48, every pixel is opaque. No soft vignette to the edge.

### 2.4 Format and layout

- **PNG, 32-bit with alpha.** Alpha used only as on/off, per §2.2.
- Deliver each class as **one horizontal strip**: height 56, width 56 × frame count, frames in the
  exact order given in *Skill and Spell Reference*.
- Frame order is not cosmetic. The frame index is also the skill's save-file slot. A strip in the
  wrong order silently mislabels every skill after the mistake.
- Individual 56 × 56 files, named `<class>_<frame>_<name>.png`, are also acceptable — assembly into
  strips happens here.

---

## 3. Style direction

**One sentence:** a small, readable, richly coloured illustration of the skill's effect, lit from
one source, dark at the edges so it reads against a bright plate.

- **Read at 56 px first.** Silhouette and one dominant colour must be identifiable instantly. Detail
  that only appears when zoomed in is wasted.
- **One dominant hue per icon**, drawn from that skill's page identity (§4), plus at most one accent.
  Two competing hues at this size read as mud.
- **Depict the effect, not the tool.** D3 draws the *moment* — the swing landing, the bolt leaving
  the hand, the ward snapping shut. A picture of a sword is a weapon icon; a sword trailing fire
  mid-swing is a skill icon.
- **Dark corners, bright centre.** A radial fall-off into near-black at the corners makes the icon
  sit inside its frame and keeps the coloured plate border legible.
- **Contrast comes from value, not hue.** With only eight steps in each saturated ramp, a light-to-
  dark value structure is what carries the shape.
- **Consistent light** across a strip: single source, upper-left, so 49 icons read as one set.

**Avoid:** photorealism, text or numerals, thin outlines under 2 px (they disappear), busy
backgrounds, and full-colour rainbow icons that use every ramp at once.

---

## 4. Colour identity per class and page

Each page gets a dominant ramp so a player can tell pages apart at a glance, and each class reads as
one family. This is the single biggest win available and costs nothing to follow.

| Class | Page | Dominant ramp | Accent |
|---|---|---|---|
| **Paladin** | Combat Skills | Gold / tan 192–207 | White 255 |
| | Offensive Auras | Crimson 224–239 | Yellow 144–151 |
| | Defensive Auras | Blue 128–135 | Gold 192–207 |
| | Passive Skills | Greyscale 240–255 | Gold 192–207 |
| **Barbarian** | Combat Skills | Crimson 224–239 | Copper 208–223 |
| | Combat Masteries | Copper / orange 208–223 | Greyscale |
| | Warcries | Yellow 144–151 | Crimson |
| | Passive Skills | Greyscale 240–255 | Crimson |
| **Sorceress** | Cold Spells | Cool blue-grey 176–191 | White 255 |
| | Lightning Spells | Yellow 144–151 | Blue 128–135 |
| | Fire Spells | Orange 208–223 | Red 136–143 |
| | Passive Skills | Blue 128–135 | White |
| **Rogue** | Bow & Crossbow | Green 152–159 | Gold |
| | Passive & Magic | Blue 128–135 | Green |
| | Javelin & Spear | Copper 208–223 | Green |
| | Passive Skills | Greyscale 240–255 | Green |
| **Bard** | Melody | Gold 192–207 | Dusty rose 160–175 |
| | Harmony | Blue 128–135 | Gold |
| | Poetry | Dusty rose 160–175 | Gold |
| | Passive Skills | Greyscale 240–255 | Gold |
| **Monk** | Way of the Staff | Copper 208–223 | Gold |
| | Way of the Body | Crimson 224–239 | White |
| | Way of the Spirit | Yellow 144–151 | Blue |
| | Passive Skills | Greyscale 240–255 | Yellow |

**Passive Skills pages are deliberately desaturated.** They are always-on traits rather than things
you cast, only four of a class's eighteen can be active at once, and the greyed treatment separates
them from the three pages of active skills at a glance. The accent hue keeps them in the family.

---

## 5. Working order

Do not attempt 273 icons in one pass. Twelve batches, each one page of one class:

1. Paladin Combat Skills (11) · 2. Paladin Offensive Auras (10) · 3. Paladin Defensive Auras (10)
4. Paladin Passives (18) · 5–8. Barbarian, four pages · 9–12. Sorceress, and so on.

**Do batch 1 first and stop.** Eleven icons is enough to judge whether the palette substitutions
survive the engine's quantiser, and it is cheap to throw away. Nobody should discover a systematic
colour problem on icon 250.

---

## 6. Delivery

Drop the files in the **root of `Oracool.MPQ`** — the standing drop zone for new art. Strips or
individual frames both work.

Naming, per class: `paladin_tree_icons.png`, `barb_tree_icons.png`, `sorc_tree_icons.png`,
`rogue_tree_icons.png`, `bard_tree_icons.png`, `monk_tree_icons.png`.

Wrong-sized art is **refused rather than stretched**, deliberately — a strip whose height is not 56
would silently shift every frame.

---

## 7. The prompt to give the generator

Use this per icon, filling the three bracketed fields from *Skill and Spell Reference* and §4.

> Draw a single fantasy skill icon for a dark medieval action RPG, in the style of Diablo III's
> skill icons: a small, richly coloured, fully illustrated square scene, not a flat symbol and not a
> floating emblem.
>
> **Canvas:** exactly 56 × 56 pixels. The illustration fills the central 48 × 48 and is fully
> opaque; the outer 4-pixel border on all four sides is fully transparent. PNG with alpha.
>
> **Hard colour limit.** Use only these hues: saturated blue, saturated red, saturated yellow, muted
> sage green, dusty rose, cool blue-grey, gold/tan, orange/copper, crimson, and greyscale.
> **No purple, no cyan, no magenta, no teal, no neon green.** These are unavailable and will be
> replaced by the wrong colour automatically.
>
> **No anti-aliasing and no soft edges.** Every pixel is either fully opaque or fully transparent —
> partial transparency is destroyed. Edges must be hard.
>
> **Composition:** one dominant hue, bright centre falling to near-black at the corners, single light
> source from the upper left, readable in silhouette at actual size.
>
> **The skill is [NAME]. It depicts: [DESCRIPTION FROM THE REFERENCE].**
> **Dominant colour: [RAMP FROM §4]. Accent: [ACCENT FROM §4].**

---

## 8. Later phase: the 53 book spells

The spells on the Spells sheet — Firebolt, Healing, Lightning, Town Portal, Teleport, and the rest —
draw from Blizzard's `ctrlpan\spelicon` inside `diabdat.mpq`. They are the most-seen icons in the
game, because they also fill the two skill wells on the HUD.

Replacing them is worth doing, but it needs an engine change first: an Oracool spell-icon strip and
an override in `Source/panels/spell_icons.cpp`, which today has no Oracool path at all. That is a
small piece of work and should happen **after** batch 1 above has proven the visual direction — no
point building the plumbing before we know the art lands.

Their frame order is `SpellID`, which is listed in `Source/spelldat.h`.

---

## 9. What happens after delivery

Extending a strip is a drop-in: the icon frame index is a skill's position within its class, which
already matches the reference document, and the engine derives the frame count from the image width.
An icon that is missing simply draws an empty plate — which is what all 110 passives do today — so a
partial delivery is safe to ship and improves the game incrementally.

The MPQ must be repacked afterwards (`tools\build_oracool_mpq.cmd`); a normal build does not do it.
