# Asset Brief — UI Chrome Overhaul

Everything below the line is written to be pasted into ChatGPT as a standalone request. Attach the
340x720 stone-panel reference image alongside it.

**Scope, set by the user on 2026-09-03:** windows, the HUD, the two points buttons, the inventory
and stash grids, the vendor windows, the inventory item slots, and the buttons that go with those
windows. **Explicitly out of scope:** skill, spell and aura icons, and the legacy coloured backings
behind them. Those are all staying as they are, so the six class icon strips, the attack icons and
the spell plates are not in this list.

Every size below was read out of the running code and the shipped art, not estimated. Where one
looks odd — 34x31 tabs, 41x22 stat buttons — it is odd in the game too.

---

## THE REQUEST

I am re-skinning the window chrome and HUD of a Diablo-engine action RPG. I have one reference
image the whole interface should grow out of, and I need the art assets listed below.

I am **not** replacing any spell or skill artwork. Everything here is frames, panels, plates,
grids, slots and buttons — the furniture that content sits inside.

### The reference

The attached image is a 340x720 panel: a pale limestone slab with an organic crazed-crack network
across it, set inside a narrow brushed-steel frame with a small square rivet plate in each corner.
Cool neutral greys, soft top-left lighting, matte and slightly dusty. That panel is the anchor.
Everything else must read as the same material family — the same stone, the same steel, the same
rivets, the same light direction.

### Hard technical constraints — please respect these exactly

1. **PNG with a real alpha channel.** Everything outside the artwork fully transparent, not white
   and not black.
2. **The engine reduces every asset to a 256-colour indexed palette at load.** Wide smooth
   gradients band badly. Favour texture, grain and hard-edged form over soft airbrushed ramps. The
   reference image survives this well because it is essentially greyscale with fine detail.
3. **Exact pixel dimensions.** These are not ratios to scale — the game blits them 1:1. If the
   schedule says 340x720, deliver exactly 340x720.
4. **No text, no numbers, no letters baked into any asset.** Every label and count is drawn by the
   game in its own font, on top of the art. This includes the points buttons: draw the medallion,
   not the number.
5. **Multi-frame sheets are a single horizontal strip — cells left to right, no padding, no
   gutters** — unless the entry says otherwise.
6. **Light source is top-left everywhere.** Recesses darker along the top-left inner edge,
   highlights on the bottom-right lip.
7. One file per asset, using the exact filename given.

### Art direction

- **Materials:** limestone for panel fields, brushed steel for frames, bars and rivets, restrained
  warm brass for accents and active states only. Gold is punctuation, never the ground.
- **Wear is subtle.** Chipped edges, faint grime in recesses. No heavy scratches, no rust.
- **Design small pieces at final size.** Many of these are 18 to 40 pixels. Contrast and silhouette
  carry them; fine detail disappears.
- **Frames stay visually thin.** The panels allow about 24px of margin and a 3px inner rule. A
  heavy frame eats the content area the game needs.
- **Empty slots must read as empty and recessed**, because the game paints item pictures into them.
  A slot that looks decorated will fight the item sitting in it.

---

## THE SCHEDULE

47 files in eight groups.

### Group 1 — Windows (5 files)

The core windows, drawn one at a time down the right side of the screen. Make them clear variations
rather than identical copies: different crack networks, subtly different stone tone, so the player
knows which window is open from the corner of their eye.

| # | Filename | Size | Notes |
|---|---|---|---|
| 1 | `panel_bg.png` | 340x720 | Generic side panel — character sheet, quest log, the neutral default |
| 2 | `inventory_background.png` | 340x720 | Inventory |
| 3 | `stash_background.png` | 340x720 | Stash — cooler and a little darker, it is a vault |
| 4 | `shop_background.png` | 340x720 | Vendor — warmer, brass accents allowed |
| 5 | `waypoint_panel.png` | 340x660 | Note the shorter height |

### Group 2 — Grids and item slots (7 files)

Recessed frames drawn *behind* item squares. **The frame is exactly 6px on every side**, so each
file is its interior plus 12px in each dimension. The interior is where item pictures land, so keep
it clean.

| # | Filename | Size | Interior | Used for |
|---|---|---|---|---|
| 6 | `grid_bezel_1x1.png` | 40x40 | 28x28 | A single item cell: rings, amulets, potions |
| 7 | `grid_bezel_2x1.png` | 68x40 | 56x28 | Wide items |
| 8 | `grid_bezel_2x2.png` | 68x68 | 56x56 | Helms, shields |
| 9 | `grid_bezel_2x3.png` | 68x96 | 56x84 | Body armour, two-handed weapons |
| 10 | `grid_bezel_inventory.png` | 292x208 | 280x196 | The whole 10x7 backpack as one frame |
| 11 | `grid_bezel_stash.png` | 292x460 | 280x448 | The whole 10x16 stash as one frame |
| 12 | `grid_bezel_shop.png` | 292x460 | 280x448 | The 10x16 vendor grid |

For 10, 11 and 12 the interior should be a continuous recessed field carrying a **faint 28px
lattice** that suggests individual cells. Visible but quiet — item art sits on top of it.

Numbers 6 to 9 are also the **equipment slots** on the character panel, so they must look right
holding a single object as well as tiled in a grid.

### Group 3 — Window border kit (6 files)

A decorative border assembled from tiling pieces, used to frame sub-windows at any size. The bars
must tile seamlessly along their long axis.

| # | Filename | Size |
|---|---|---|
| 13 | `border_corner_tl.png` | 24x24 |
| 14 | `border_corner_tr.png` | 24x24 |
| 15 | `border_corner_bl.png` | 24x24 |
| 16 | `border_corner_br.png` | 24x24 |
| 17 | `border_h_unit.png` | 12x24 |
| 18 | `border_v_unit.png` | 24x12 |

### Group 4 — The HUD (6 files)

| # | Filename | Size | Notes |
|---|---|---|---|
| 19 | `middle_hud.png` | 356x64 | The central plate. Carries a **46x46 round skill socket at each end** and a row of small belt slots between them. Both sockets must read as clearly recessed — the game drops spell pictures into them. |
| 20 | `health_orb.png` | 105x96 | Left orb. **The sphere interior must be empty and fully transparent** — the game fills it with liquid that rises and falls. Deliver the ornate surround only. |
| 21 | `mana_orb.png` | 97x96 | Right orb, mirrored. Same rule about the empty interior. |
| 22 | `level_up_icon.png` | 180x61 | Wide banner plate for the level-up notice |
| 23 | `town_portal_icon.png` | 81x29 | Small wide button |
| 24 | `burger_menu_button.png` | 52x26 | Small wide button, three-bar menu glyph |

### Group 5 — The two points buttons (2 files)

Both appear only when the player has something to spend, and both must read as "click me". The
game draws the count on top, so leave the centre clear.

| # | Filename | Size | Notes |
|---|---|---|---|
| 25 | `skill_points.png` | 64x64 | Skill points medallion. **Keep a clean 40x39 area dead centre** for the number the game draws there. |
| 26 | `stat_plus_button.png` | 82x22 | The "+" button beside each attribute on the character sheet. **2 frames of 41x22**: normal, pressed. The plus glyph IS part of the art here — this one is a symbol button, not a label holder. |

### Group 6 — The menu icon grid (1 file)

| # | Filename | Size | Layout |
|---|---|---|---|
| 27 | `menu_icons.png` | 90x264 | **A grid, not a strip.** 3 columns x 8 rows, cells of 30x33. **Column is the state** — normal, hovered, pressed. **Row is the entry**, top to bottom: Character, Inventory, Abilities, Stash, Quest Log, Waypoints, Options, Exit. Each cell is a complete small button with its pictogram baked in. These are navigation buttons, not spell icons. |

### Group 7 — Window buttons and furniture (13 files)

The controls that live on and around the windows above.

| # | Filename | Size | Notes |
|---|---|---|---|
| 28 | `inventory_tabs_chest.png` | 102x31 | 3 frames of **34x31 — not square**: unselected, hovered, selected. The inventory's ten tab strip. |
| 29 | `inventory_sort.png` | 84x28 | 2 frames of 42x28: normal, hovered. The Sort control; the word is drawn by the game. |
| 30 | `window_close.png` | 54x18 | 3 frames of 18x18: normal, hovered, pressed. The red X in every window's corner. |
| 31 | `button_wide.png` | 240x104 | 4 frames of 240x26 stacked **vertically**: normal, hovered, pressed, disabled. The general window button; text drawn on top. |
| 32 | `button_small.png` | 160x80 | 4 frames of 40x20 in a horizontal strip: normal, hovered, pressed, disabled. The stash's page controls sit at this size. |
| 33 | `tab_vertical.png` | 78x80 | 3 frames of **26x80**: unselected, hovered, selected. The vendor panel's tab column runs down the side. |
| 34 | `service_button.png` | 200x104 | 4 frames of 200x26 stacked **vertically**: normal, hovered, pressed, disabled. The vendor's Repair / Recharge / Repair All row. |
| 35 | `page_arrow.png` | 64x16 | 4 frames of 16x16: left, right, left hovered, right hovered |
| 36 | `scrollbar_track.png` | 12x64 | Tiles vertically |
| 37 | `scrollbar_thumb.png` | 12x32 | |
| 38 | `scrollbar_arrow.png` | 36x12 | 3 frames of 12x12: up, down, disabled |
| 39 | `divider_h.png` | 280x8 | Horizontal separator for use inside a panel |
| 40 | `tooltip_bg.png` | 64x64 | A dark plate with a 2px light rule, built so it can be nine-sliced. Keep the corner detail inside the outer 16px. |

### Group 8 — Stone field textures (7 files)

Stone fields used as window interiors behind content. The closest thing to the reference image
itself: field only, no frame, no rivets. Seven variations so windows can differ.

| # | Filenames | Size |
|---|---|---|
| 41–47 | `texture_stone_v1.png` through `texture_stone_v7.png` | 480x720 each |

---

## What I need back

The files as a flat set with those exact names. If you can only manage a subset per response, work
down the groups in order and tell me where you stopped.

**Groups 1, 2 and 4 together are enough for me to judge the direction** — one window, the grid
bezels and the HUD plate with its orbs. Start there if you want a checkpoint before doing the
buttons.

Please ask me before inventing any size that is not on this list.
