# Asset Brief — Full UI Overhaul

Everything below the line is written to be pasted into ChatGPT as a standalone request. It assumes
no knowledge of this project. Attach the 340x720 stone-panel reference image alongside it.

Every size in the schedule was read out of the running code and the shipped art on 2026-09-03, not
estimated. Where a size looks odd (34x31 tabs, 2744x56 strips) it is odd in the game too.

---

## THE REQUEST

I am re-skinning the entire user interface of a Diablo-engine action RPG. I have one reference
image I want the whole interface to grow out of, and I need you to produce the art assets listed in
the schedule below.

### The reference

The attached image is a 340x720 panel: a pale limestone slab with an organic crazed-crack network
across it, set inside a narrow brushed-steel frame with a small square rivet plate in each corner.
Cool neutral greys, soft top-left lighting, matte and slightly dusty. That panel is the anchor.
Everything else must read as the same material family: the same stone, the same steel, the same
rivets, the same light direction.

### Hard technical constraints — please respect these exactly

1. **Deliver PNG with a real alpha channel.** Everything outside the artwork must be fully
   transparent, not white and not black.
2. **The engine reduces every asset to a 256-colour indexed palette at load.** Wide smooth
   gradients will band badly. Favour texture, grain and hard-edged form over soft airbrushed ramps.
   The reference image survives this well because it is essentially greyscale with fine detail.
   Stay in that register.
3. **Exact pixel dimensions.** These are not ratios to scale — the game blits them 1:1. If the
   schedule says 340x720, deliver exactly 340x720.
4. **No text, no numbers, no letters baked into any asset.** Every label is drawn by the game in
   its own font, on top of the art.
5. **Multi-frame sheets are a single horizontal strip: square cells, left to right, no padding and
   no gutters.** The cell size equals the strip's height. A 56px-tall strip of 49 icons is exactly
   2744x56. A missing frame silently shifts every icon after it onto the wrong skill.
6. **One sheet is a grid instead of a strip**, and it is called out explicitly.
7. **Light source is top-left, everywhere.** Recesses darker along the top-left inner edge,
   highlights on the bottom-right lip.
8. Deliver each asset as its own file, using the exact filename given.

### Art direction

- **Material palette:** limestone for panel fields, brushed steel for frames, bars and rivets, and
  a restrained warm brass used only for accents and active states. The interface leans gold today;
  keep gold as punctuation, never as the ground.
- **Wear is subtle.** Chipped edges and faint grime in the recesses. No heavy scratching, no rust.
- **Silhouette first.** Many of these are 28 to 56 pixels across. At that size, form and contrast
  are everything and fine detail disappears. Design the small pieces at final size rather than
  shrinking large ones.
- **Frames must stay visually thin.** The big panels allow about 24px of margin and a 3px inner
  rule. A heavy frame eats the content area the game needs.

---

## THE SCHEDULE

68 files, grouped by what they do.

### Group 1 — Full window panels

These are the core windows, drawn one at a time down the right-hand side of the screen. Make them
four clear variations on the reference rather than four identical copies: different crack networks
and subtly different stone tone, so the player knows which window is open from the corner of their
eye.

| # | Filename | Size | Notes |
|---|---|---|---|
| 1 | `panel_bg.png` | 340x720 | Generic side panel, the neutral default |
| 2 | `inventory_background.png` | 340x720 | Inventory |
| 3 | `stash_background.png` | 340x720 | Stash — slightly cooler and darker, it is a vault |
| 4 | `shop_background.png` | 340x720 | Vendor — slightly warmer, brass accents allowed |
| 5 | `waypoint_panel.png` | 340x660 | Note the shorter height |

### Group 2 — Slot and grid bezels

Recessed frames drawn *behind* item and icon squares. **The frame is exactly 6px on every side**,
so the file is always the interior plus 12px in each dimension.

| # | Filename | Size | Interior | Used for |
|---|---|---|---|---|
| 6 | `grid_bezel_1x1.png` | 40x40 | 28x28 | One inventory cell: rings, amulets, potions |
| 7 | `grid_bezel_2x1.png` | 68x40 | 56x28 | Wide items |
| 8 | `grid_bezel_2x2.png` | 68x68 | 56x56 | Helms, shields, skill icons |
| 9 | `grid_bezel_2x3.png` | 68x96 | 56x84 | Body armour, two-handed weapons |
| 10 | `grid_bezel_inventory.png` | 292x208 | 280x196 | The whole 10x7 backpack as one frame |
| 11 | `grid_bezel_stash.png` | 292x460 | 280x448 | The whole 10x16 stash as one frame |
| 12 | `grid_bezel_shop.png` | 292x460 | 280x448 | The 10x16 vendor grid |

For 10, 11 and 12 the interior should be a continuous recessed field carrying a **faint 28px
lattice** that suggests the individual cells. Visible but quiet — item art sits on top of it.

### Group 3 — Ornate border kit

A decorative border assembled from tiling pieces, used to frame sub-windows at any size. The bars
must tile seamlessly along their long axis.

| # | Filename | Size | Notes |
|---|---|---|---|
| 13 | `border_corner_tl.png` | 24x24 | Top-left corner, with rivet |
| 14 | `border_corner_tr.png` | 24x24 | |
| 15 | `border_corner_bl.png` | 24x24 | |
| 16 | `border_corner_br.png` | 24x24 | |
| 17 | `border_h_unit.png` | 12x24 | Horizontal bar unit, tiles left to right |
| 18 | `border_v_unit.png` | 24x12 | Vertical bar unit, tiles top to bottom |

### Group 4 — The bottom HUD

| # | Filename | Size | Notes |
|---|---|---|---|
| 19 | `middle_hud.png` | 356x64 | The central plate. Carries a 46x46 round skill socket at each end and a row of small belt slots between them. Both sockets must read as recessed. |
| 20 | `health_orb.png` | 105x96 | Left orb. **The sphere interior must be empty and transparent** — the game fills it with liquid that rises and falls. Deliver the ornate steel-and-brass surround only. |
| 21 | `mana_orb.png` | 97x96 | Right orb, mirrored. Same rule about the empty interior. |
| 22 | `skill_points.png` | 64x64 | A medallion shown when unspent skill points are available. Must read as clickable. |
| 23 | `level_up_icon.png` | 180x61 | Wide banner plate for the level-up notice |
| 24 | `town_portal_icon.png` | 81x29 | Small wide button |
| 25 | `burger_menu_button.png` | 52x26 | Small wide button, three-bar menu glyph |
| 26 | `inventory_sort.png` | 84x28 | Two frames of 42x28 side by side: normal, hovered |

### Group 5 — Icon strips

Re-read constraint 5 before starting these.

| # | Filename | Size | Cells | Cell | Contents |
|---|---|---|---|---|---|
| 27 | `attack_icons.png` | 76x38 | 2 | 38x38 | A melee weapon swing; a bare fist |
| 28 | `waypoint_icons.png` | 86x43 | 2 | 43x43 | Waypoint dormant; waypoint lit |
| 29 | `inventory_tabs_chest.png` | 102x31 | 3 | **34x31, not square** | One tab: unselected, hovered, selected |
| 30 | `spell_slot_frames.png` | 168x56 | 3 | 56x56 | An empty spell plate in three tints: neutral, active green, unavailable red. Frame only — the spell picture is drawn inside it by the game. |

### Group 6 — The menu icon grid

| # | Filename | Size | Layout |
|---|---|---|---|
| 31 | `menu_icons.png` | 90x264 | **A grid, not a strip.** 3 columns x 8 rows, each cell 30x33. **Column is the state** (normal, hovered, pressed). **Row is the entry**, top to bottom: Character, Inventory, Abilities, Stash, Quest Log, Waypoints, Options, Exit. Each cell is a complete small button with its pictogram baked in. |

### Group 7 — Class skill-tree icon strips

Six strips at 56x56 cells, and the largest job in the set. Each icon is a distinct fantasy skill
emblem on a transparent background, with **no frame** — the game draws the bezel behind it.

| # | Filename | Size | Cells |
|---|---|---|---|
| 32 | `paladin_tree_icons.png` | 2744x56 | 49 |
| 33 | `barb_tree_icons.png` | 2744x56 | 49 |
| 34 | `rogue_tree_icons.png` | 2744x56 | 49 |
| 35 | `sorc_tree_icons.png` | 2688x56 | 48 |
| 36 | `bard_tree_icons.png` | 2184x56 | 39 |
| 37 | `monk_tree_icons.png` | 2184x56 | 39 |

If 273 unique emblems is impractical in one pass, work class by class and tell me which class you
are on. Do not shorten a strip: a missing frame mis-indexes every icon after it.

### Group 8 — Character silhouettes

Flat dark semi-transparent shapes, shown behind the hero on the character-select screen.

| # | Filename | Size |
|---|---|---|
| 38 | `silhouette_paladin.png` | 245x356 |
| 39 | `silhouette_barbarian.png` | 250x356 |
| 40 | `silhouette_sorcerer.png` | 248x356 |
| 41 | `silhouette_archer.png` | 281x356 |
| 42 | `silhouette_bard.png` | 250x356 |
| 43 | `silhouette_monk.png` | 250x356 |

### Group 9 — Full-screen backgrounds

Wide painted scenes rather than panels. Same world as the stone, but illustration, not interface.

| # | Filename | Size | Subject |
|---|---|---|---|
| 44 | `main_menu_bg.png` | 1916x821 | Title screen |
| 45 | `hero_select_bg.png` | 1916x821 | Character select |
| 46 | `difficulty_bg.png` | 1915x821 | Difficulty select. Needs four horizontal bands, one per difficulty, readable as separate zones. |
| 47 | `choose_hero_bg.png` | 1680x720 | Class choice |
| 48 | `hero_settings_bg.png` | 1680x720 | Character creation |

### Group 10 — Stone field textures

Stone fields used as window interiors, seven variations so windows differ. These are the closest
thing to the reference image itself: field only, no frame and no rivets.

| # | Filenames | Size |
|---|---|---|
| 49–55 | `texture_stone_v1.png` through `texture_stone_v7.png` | 480x720 each |

### Group 11 — Small interface furniture

| # | Filename | Size | Notes |
|---|---|---|---|
| 56 | `window_close.png` | 54x18 | 3 frames of 18x18: normal, hovered, pressed. A red X. |
| 57 | `scrollbar_track.png` | 12x64 | Tiles vertically |
| 58 | `scrollbar_thumb.png` | 12x32 | |
| 59 | `scrollbar_arrow.png` | 36x12 | 3 frames of 12x12: up, down, disabled |
| 60 | `page_arrow.png` | 64x16 | 4 frames of 16x16: left, right, left hovered, right hovered |
| 61 | `tooltip_bg.png` | 64x64 | A dark plate with a 2px light rule, built so it can be nine-sliced. Corners inside the outer 16px. |
| 62 | `toast_bg.png` | 200x34 | Wide notification plate |
| 63 | `button_wide.png` | 240x104 | 4 frames of 240x26 stacked **vertically**: normal, hovered, pressed, disabled |
| 64 | `button_small.png` | 160x80 | 4 frames of 40x20 in a horizontal strip: normal, hovered, pressed, disabled |
| 65 | `tab_vertical.png` | 78x80 | 3 frames of 26x80: unselected, hovered, selected. A vertical side tab for the vendor panel. |
| 66 | `divider_h.png` | 280x8 | Horizontal separator for use inside panels |
| 67 | `checkbox.png` | 72x18 | 4 frames of 18x18: off, off hovered, on, on hovered |
| 68 | `slider.png` | 128x16 | A 96x16 track then a 32x16 handle, side by side |

---

## What I need back

The files as a flat set, with those exact names. If you can only manage a subset per response, work
down the groups in order and tell me where you stopped.

Groups 1, 2 and 4 together are enough for me to judge whether the direction is working, so start
there if you want a checkpoint before committing to the icon strips.

Please ask me before inventing any size that is not on this list.
