---
title: 2026-08-12 - Grid Grain, and a Silent Asset Pipeline Gap
date: 2026-08-12
tags: [dev-report]
summary: The 70-cell backpack grid gets a procedural stone-grain texture, closing the gap with DevilutionX's own shared-stash grid without reusing that grid's art. Along the way, found that build_inventory_assets.cmd had never written to the folder oracool.mpq actually packs from - every previous panel rebuild this session silently shipped stale art.
---

# Grid Grain, and a Silent Asset Pipeline Gap

User request, arrived as a direct visual mockup: two versions of the inventory panel side by side, "vanilla grid texture" against "current design," asking to spot the difference. The equipment slots were identical in both; the 70-cell backpack grid was the only thing that changed - mottled, blotchy stone on the left, flat uniform cells on the right.

## Two false starts first

The investigation took a wrong turn twice before landing on the right question, both worth recording because they're exactly the mistakes a repeat of this task would make again.

**First**, misread the request as being about the equipment paperdoll slots (legs/boots appearing frame-less in an early screenshot). Traced that all the way down: `InvCompose.cs` already stamps the correctly-shaped `slot-frame-*.png` onto all 13 equipment slots, confirmed identical across every asset channel including the live MPQ, confirmed still visible after running it through the actual runtime palette-quantization code. That part was never broken - the user corrected the scope before more time went into it.

**Second**, once the DevilutionX shared-stash panel was identified as the actual reference, decoded `stash.clx` directly (writing a small CLX decoder from the documented format in `clx_encode.hpp`/`clx_sprite.hpp`, since no live screenshot of it existed this session) to see what "the stash's texture" really was. It turned out to be a hand-authored, per-cell mottled motif - DevilutionX's own art. Copying it verbatim into `oracool.mpq` would repeat exactly the concern that kept `town.pal` out of a commit earlier in this project: it isn't Oracool's to ship. That ruled out extraction and pointed at generating an equivalent *effect* from scratch instead.

## The actual fix

`ApplyGridGrain` in `InvCompose.cs`: for each of the 70 grid cells, draws blotchy (not per-pixel) noise - a single random delta per 3x3 block, nearest-neighbour filled, matching the chunky multi-pixel patches the stash reference showed under zoom rather than fine TV-static. Skewed dark (mostly `-48` to `+20`) since the reference reads as scattered darker patches over the base tone, not scattered highlights. Confined to each cell's interior, staying clear of the frame ring the existing grid-tiling loop already draws, and seeded for reproducible rebuilds.

The amplitude took three iterations to land, because eyeballing the unconstrained composition isn't the test that matters - the game re-quantizes this 24-bit PNG down to roughly 60 usable entries in the shared upper palette half at runtime (`NearestGlobalPaletteIndex`). The first attempt (±22/+8) survived that quantization as essentially nothing - visually present in the source file, crushed flat by the time it reached the palette. A deliberately absurd ±120/+60 test confirmed the mechanism worked at all; ±48/+20 was the point that read as grain rather than static once pushed through the same quantization pass the game actually performs.

## The pipeline gap this exposed

`build_inventory_assets.cmd` only ever wrote to `Packaging\resources\assets\ui\` - the loose-fallback asset tree - and mirrored into the Debug build folder. It never touched `Packaging\resources\oracool_assets\ui\`, which is the *separate* tree `build_oracool_mpq.cmd` actually packs from. Every other `build_*.cmd` script in this project (`build_item_icons.cmd`, `build_waypoint_cel.cmd`) mirrors into both; this one predates `oracool.mpq` entirely and was simply never updated when that pipeline was introduced.

The consequence: every rebuild of the inventory panel compiled cleanly, previewed correctly (`InvPreview.exe` reads from the just-written loose folder), and then packed the *old* art into the MPQ - with no error anywhere in the chain, because nothing was actually wrong with any individual step. Caught only by hashing the panel PNG independently in all three locations and finding two of them frozen at a stale value while the third moved. `build_inventory_assets.cmd` now mirrors into `oracool_assets\ui` too, matching every sibling script's convention, with the mismatch documented inline so it can't quietly happen again.

## Verification

Cross-checked the packed `oracool.mpq` directly (extracted `ui\inventory_panel.png` back out and hashed it against the source) rather than trusting the pack step's own exit code, given what had just gone wrong with the same file. All three channels now hash identically and differ from the pre-grain baseline. Debug build clean at `ORACOOL_VERSION` **1.1.21**, tests **349/351**, the same two pre-existing failures.

Not yet play-tested at actual on-screen scale and brightness - everything here was verified against the same palette-quantization the game performs, but not against a running session.

## Related

- [[2026-08-12 - Six New Equipment Slots]]
