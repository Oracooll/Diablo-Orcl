# The Barbarian's look: a fifth larger, dyed to his trousers

2026-09-16 — v1.12.021

## Why

> "i had simpler ideas: Can we make barb sprite 20% larger than warrior? Can we die his body shirt
> and boots matching blue of pants? Can we die his hair grey? Can we die his gloves blue as pants?"
> — "these changes are all regarding light sprites."

The Barbarian wears the Warrior's CL2s (`GetPlayerSpriteClass`, no `hfbarb.mpq` shipped). The
options for changing that ran from a palette recolour to a 270-sheet body set; the user chose the
cheap end, and it turns out to be cheaper than it looked.

## What was measured first

`tools/oracool_sprite_export.cpp` dumped the Warrior's 255 sheets; a `--dump-palette` flag was
added so the exported colours could be mapped back to palette indices. The light-armour Warrior is
drawn from five clean 8-entry ramps, one per garment:

| garment | indices |
|---|---|
| trousers | 184–191 (blue) |
| mail shirt | 240–255 (greyscale, 16 entries) |
| boots, belt, straps | 216–223 (brown) |
| gloves | 168–175 (dark red) |
| hair | 206–207 — the dark end of the skin ramp 200–207; the face is 201–205 |

Two things share a ramp with what the user wants dyed: metal weapon blades sit on the greyscale
(mostly 243–244, where the mail never goes — it starts at 246), and the belt is the boots' leather.

## What it is now

`oracool/hero_look.{h,cpp}`:

- **`HeroDyeTrn(player)`** — a 256-entry translation, Barbarian in light armour only. Mail
  240–255 → 184–191 two greys to a blue; boots and gloves one to one onto the trousers; hair
  206/207 → 247/250, visibly grey rather than the near-black their brightness would pick. Medium and
  heavy armour have not been measured and get nothing.
- **`SpriteScalePercent(class)`** — 120 for the Barbarian, 100 for everyone.

`oracool/sprite_import.cpp` gained **`ScaleSpriteSheet(sheet, percent)`**: every frame rendered into
a column (two passes — colour, and a mask through an all-ones TRN, because index 0 is the foot shadow
and "still zero" is not "not drawn"), nearest-neighbour resampled, re-encoded with `SurfaceToClx`
through an index the sprite never uses. Frames keep their feet on the floor; the renderer anchors a
sprite at its bottom and centres it by its own width, so nothing else moved.

`LoadPlrGFX` applies both after the class TRN it already bakes: `ClxApplyTrans` with the dye, then
the scaled sheet replaces the original. Load-time only — `DrawPlayer` is untouched, and so is every
light level.

## Not verified here

Not run in the game. What to look at, in light armour:

- He should stand a head taller than before and stay centred on his tile walking in all eight
  directions.
- Shirt, boots, belt and gloves in the trousers' blue; hair grey; face unchanged.
- A sword raised: the blade keeps its steel with a blue edge. If that reads wrong, the mail line
  in `LightBarbarianDye` can stop at 246 and keep the blade whole.
- The character-select preview (`oracool/hero_preview.cpp`) loads its own sheets and shows the
  old Warrior; not changed here.

Tests: `oracool_hero_look_test` — the table maps exactly the five ramps and nothing else, the scaler
keeps a solid index-0 frame solid and a corner on the floor. Debug build only, per the user
(2026-09-16: "from now on only build x64 debug").
