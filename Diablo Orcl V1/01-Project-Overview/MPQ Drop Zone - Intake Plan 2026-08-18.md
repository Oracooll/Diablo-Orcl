# MPQ Drop Zone — Intake Plan, 2026-08-18

**Swept:** 2026-08-18, against `C:\Users\hroga\OneDrive\2. Personal Files\Software\Diablo\Oracool.MPQ\`
**Previous sweep:** 2026-08-16 (33 loose files, all consumed — see *MPQ Drop Zone - Introduction Plan*)
**Engine version at sweep time:** 1.7.70

## What landed

Thirteen delivered packages and two loose masters, all dated **2026-08-17 evening** (20:06 → 23:10),
plus a new root folder `ChatGPT Assets/` holding every package as both a zip and an extracted tree.

**Every `.sha256` verifies.** One caveat worth recording: the sidecars are written with CRLF line
endings, so `sha256sum -c` fails on all of them with `No such file or directory` until the `\r` is
stripped (`cat *.sha256 | tr -d '\r' | sha256sum -c -`). That is a false alarm, not a corrupt drop —
but it looks exactly like a corrupt drop, so check it before panicking next time.

The root's eleven zips are a **subset** of `ChatGPT Assets/` (seventeen). Six exist only in the
staging folder: the three artisan title families, the bottom-bezel family, and the two already-consumed
2026-08-16 packs (inventory-stash-background, stash-tab-button). Filing is therefore mostly
deduplication, not copying.

### Superseded inside the drop

| Keep | Discard |
|---|---|
| `grand-reliquary-chest-sprite-pack-v1.1.0` | `v1.0.0` (v1.1.0 adds the mirrored orientation) |
| `burger-menu-icon-pack-v1.1.0` | `v1.0.0` |
| `grid-bezel-family-v1.0.0` | `grid-bezel-pilot-v0.1.0` (folded in as `assets/source/pilot-runtime/`) |
| `artisan-large-title-family-v3.0.0` | `artisan-titled-panel-family-v1.0.0`, `artisan-integrated-title-family-v2.0.0` — v1/v2/v3 are three generations of one idea |

## The seven units

Ordered cheapest-and-most-certain first, which is also lowest-risk first.

### Unit A — Grand Reliquary chest → `objects\orclstash.cel`

The cleanest thing in the drop, and the one with an exact precedent. The town Stash Chest currently
**borrows vanilla `OBJ_CHEST3`** (`objects.cpp:4174 AddStashChestObject`, which reuses the type
deliberately after an A/B against a custom `OBJ_STASHCHEST` that had a draw-order bug). This pack
ships a bespoke three-state 160×160 sprite — closed / opening / open — **already built as a
`.cel` and already quantized against the patched `town.pal`**, exactly as `orclwayp.cel` was.

Nothing needs cutting; the runtime file is in the box. The work is engine-side: a new `OFILE_`
entry beside `OFILE_ORCLWAYP` and pointing the stash chest at it, which is the same three-line shape
as the waypoint change (`objdat.cpp:359`, `objects.cpp:2037`, `objects.cpp:4273`).

**One decision:** `orclstash.cel` or `orclstash_mirrored.cel`. Which way the chest faces depends on
where it sits relative to the player's approach in town; worth eyeballing both in the asset studio.

Loose master: `Chest Icon.png` (1254×1254 RGB) — files to `02-source-art/world/`.

### Unit B — Three icon refreshes (burger, portal, level-up)

All three replace assets **already shipped and already wired**, at the same on-screen size, so this
is a pure art swap with no geometry or code change:

| Asset | Live now | Pack gives |
|---|---|---|
| `ui\burger_menu_button.png` | 81×29 = 3 × 27×29 states | 3 × 1254×1254 RGBA masters, alpha-registered |
| `ui\town_portal_icon.png` | 81×29 = 3 × 27×29 states | 3 × 1254×1254 (inactive / hover / click) |
| `ui\level_up_icon.png` | 180×61, two states | 2 × masters (inactive / active) |

Every README insists on the same discipline, and it is the right one: **resize all states with one
identical transform**, then binary alpha, then quantize against `town.pal` indices 128..255. Cutting
states independently makes the icon jump when it changes state.

Two things to carry forward. The portal pack is deliberately frameless — "no physical frame, stone,
metal, pedestal, or rune construction" — a pure energy ellipse, which is a change of intent from the
current framed cell, so the plate's painted portal ring behind it (`hud_art.cpp:92`) may now read as
double framing. And the level-up masters are the **source of `points_icons_dark/lit.png`** as well
(the 99-frame numbered strips were cut from the old states with the cross swapped for a numeral) —
so re-cutting the icon without re-cutting those strips would leave two generations side by side.

Loose master: `Level Up.png` (1254×1254 RGB) — files to `02-source-art/hud-icons/`.

### Unit C — Grid bezel family

Per-slot and per-grid frames on a strict 28-pixel cadence, in four colourways:

- 1×1 (40×40) — the pilot cell
- 2×1 belt (68×40), 2×2 equipment (68×68), 2×3 equipment (68×96)
- 10×7 inventory (292×208) — **matches this fork's own inventory grid exactly**
- 10×16 stash (292×460)

The integration rule is precise and cheap: draw at intrinsic size, no scaling, positioned six pixels
up and left of the existing logical slot rect so the asset's (6,6) lands on the slot's old top-left.
Cell geometry and item placement do not move. `outer = cells*28 + 12` if a new configuration is ever
needed.

That the 10×7 figure is the fork's actual inventory shape is the strongest signal in the drop that
these were measured against the real build rather than guessed.

Also here: `grid-bezel-plus-pilot`, a 1×1 limestone cell wearing a large red plus — presumably the
"spend a point" affordance, and the only asset in the drop with no obvious home yet.

### Unit D — Side-panel stone backgrounds

Four 340×720 background families, four colourways each, in ascending elaboration:

1. `stone-panel-background-family` — plain
2. `stone-panel-bottom-bezel-family` — plus a bottom bezel
3. `stone-panel-artisan-bezel-family` — plus the judgment-frieze artisan bezel
4. `artisan-large-title-family v3` — plus a large carved title, **cut per screen**: hero-stats,
   inventory, quests, spells, stash, waypoints (6 screens × 4 colourways = 24 files)

These target the side panels currently composed at runtime from the `panel_frame_*` kit (17 pieces).
Swapping to a baked 340×720 background per screen is a real simplification — but it also **retires
that kit**, and the fork's waypoint panel is already a flat 340×660 baked composition, so there is a
consistency argument for going the same way everywhere.

Note the height: **340×720 vs the waypoint panel's 340×660**. Sixty pixels of difference to
reconcile before anything is wired.

### Unit E — Bottom HUD limestone plate

The largest and riskiest item: a **1536×1024 RGB art master** intended to replace `ui\middle_hud.png`,
which is currently **356×64**. Eight framed slots sharing one bottom baseline, labels baked in
(LMB, Menu, 1, 2, 3, 4, Portal, RMB), Menu and Portal recesses left blank for Unit B's icons.

This is not an art swap — it is a re-cut of the plate that `oracool/hud_layout` geometry, the belt
slot rects, the skill wells, the XP counter placement, and the ≤640-wide dirty-rect path are all
built around. The README's own instructions confirm the scale of it: quantize the RGB master
yourself, verify the exact runtime draw dimensions before integration, and **"do not add a
procedural bottom offset — the artwork is already screen-bottom aligned"**, which contradicts how
the current plate is anchored.

Do this last, alone, on its own version.

## The one open decision — SETTLED: ashen limestone

> **Decided 2026-08-18 by the user: ashen limestone.** The other three colourways stay in
> `ChatGPT Assets/` as delivered and are not filed into `02-source-art/` — they are options that
> were not taken, not source material.


Four of the seven units ship **four colourways**: ashen limestone, cold crypt slate, oxblood basalt,
sepulchral bronze. They are not mixable — picking per-screen would make the UI look assembled from
scavenged parts.

**Ashen limestone is the strong default** and is what this plan assumes unless told otherwise: the
bottom HUD plate exists *only* in limestone, the grid-bezel-plus pilot is limestone, and the
limestone plate is the newest asset in the drop (22:38). The other three read as options generated
alongside it rather than as competitors.

## Filing (once each unit is consumed)

Per the standing rule, source art is copied into the tree with a versioned name rather than read from
the root, so a re-cut survives a reorganisation:

| From | To |
|---|---|
| `Chest Icon.png`, chest pack sources | `02-source-art/world/` |
| `Level Up.png`, level-up masters | `02-source-art/hud-icons/` |
| burger + portal masters | `02-source-art/menu-icons/` |
| grid bezel runtime + masters | `02-source-art/inventory/` |
| stone panel families | `02-source-art/ui-backgrounds/` |
| bottom HUD limestone plate | `02-source-art/bottom-hud/` |

The zips themselves stay in `ChatGPT Assets/`, which is where they already are — the root copies are
the duplicates and are what the sweep clears.

## Two live trees, not one

Worth writing down because it has bitten before: assets exist in **two** places.

- `Packaging/resources/oracool_assets/` — **the live set**, packed into `oracool.mpq`
  (`CMakeLists.txt:377`, `tools/build_oracool_mpq.cmd`)
- `Packaging/resources/assets/` — the loose fallback path, packed as plain `assets`
  (`CMakeLists.txt:493`)

They have already drifted: `inventory_tabs_chest.png`, `points_icons_*.png`, `difficulty_bg.png`,
`inventory_background.png`, `stash_background.png` and the six class-tree icon strips exist only in
`oracool_assets`, while `attack_icons.png`, `aura_icons.png`, `barb_skill_icons.png`,
`inventory_tabs.png` and `inventory_tabs_v3.png` exist only in `assets`. Anything shipped from this
drop goes into `oracool_assets` — `ApplyGridStoneTexture.ps1` writing to three trees at once is the
existing precedent for keeping them honest where it matters.
