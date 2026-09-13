# RfA-13's glyphs are in: every skill in every tree has its icon

2026-09-13 — v1.11.113

## Why

v1.11.108–112 built the 162 RfA-12 skills, drawing placeholder letters where their icons would go. RfA-13
asked for the 162 glyphs, and batch 31 delivered all of them. This puts them in the six tree strips, so all
434 tree rows now draw a glyph.

## The delivery, and one tangle

The artist delivered in three passes into `Resources/01-in-use-assets/delivered-packs/batch-31-new-skill-glyphs`.
The first shadow fix ran in place in that folder while the artist was still exporting the later classes
into it. The two processes rewrote each other's files: 125 PNGs changed under the artist, and some of mine
were overwritten mid-run. The artist noticed, stopped writing the parent folder, and handed over a separate,
self-checked snapshot, `artist-verified-export-2026-09-13/`:

- 162 glyphs on the RfA's exact paths, all in its manifest;
- two colours plus clear, binary alpha, 56×56;
- 162 distinct silhouettes; three lookalikes redesigned at native size (Arc vs Chain Lightning, Chord of
  Warding vs Sonic Barrier, Heaven Splitter vs Mountain Pole).

That snapshot is now the only source. `tools/FixBatch31GlyphShadows.ps1` reads it and **writes** the house-style
glyphs into the parent `glyphs/` folder, which is what `BuildGlyphStrips.ps1` reads. It never edits the
snapshot, and its output depends only on the snapshot: two runs back to back are byte-identical (checked by
hash on all 162).

## Two corrections, both the brief's

- **Shadow direction.** RfA-13 said the shadow falls "down and right"; every glyph already in the game has it
  two pixels left and one up, (-2,-1). The artist followed the brief at (+2,+2). The script rebuilds the
  shadow from the white shape (shadow = white moved by (-2,-1), wherever that lands on clear), so the artist's
  drawing is untouched. The RfA-13 file carries a correction note.
- **Safe box.** `AuditGlyphStrips` holds every glyph inside pixels 8–47 on both axes. 144 of the 162 overshot
  by a pixel or so; each is moved the least distance that puts it inside, keeping the artist's placement
  wherever it already fit. None had to lose a shadow row or column, and none is too big for the box.

## The engine side

- `tools/BuildGlyphStrips.ps1`: batch 31 joins `$extraPacks`. Every class stamps 72 glyphs (the Barbarian and
  Rogue strips keep one retired row each); every glyph matched a tree row bar the 3 retired ones.
- `Source/oracool/hud_art.cpp`: `StripHasFrame` now scans the cell's alpha, so a frame that is present but
  fully transparent draws the placeholder letters rather than an empty square. Nothing hits it now; it
  keeps a future half-delivered pack from blanking icons.
- The strips are packed into `oracool.mpq` by the normal build (`oracool_mpq_pack`).

## Checks

- `AuditGlyphStrips`: 434 frames, all glyphs, 0 legacy, 0 empty, **problems: 0**.
- Full suite: 740/740 (Timedemo.WarriorLevel1to2 skipped, as always).
- Not yet seen in play: the icons at their real size on the skill pages. The artist flagged the eye and
  spear families on the Rogue pages as the closest lookalikes to check there.
