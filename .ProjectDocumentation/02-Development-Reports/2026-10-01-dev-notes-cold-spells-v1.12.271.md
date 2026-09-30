# 2026-10-01 - Dev notes: the Cold spells' look, the Nephalem portal (v1.12.271)

**Date:** 2026-10-01. Debug only. The user: "check my dev notes and apply." Sixteen notes from the v1.12.270 play session (2026-09-30, 23:26-23:45). Fifteen are done and archived in `development-archive.md`; the Absolute Zero note stays open in `development.md` as a reminder for the user.

## Casts that play with no foe

- **Chill Touch, Ice Needle, Ice Lance** returned false when nothing was in reach, so the cast was refused and nothing was drawn. They now return true on every cast and spend their mana:
  - Chill Touch always draws its cone.
  - Ice Needle and Ice Lance fly to the farthest foe struck, else to their full reach toward the cursor (`FlightEnd`).
- **Impact cues:** these still play only when the cast struck something.

## Art changes

| Skill | Change |
|---|---|
| Ice Lance | Vanilla `Arrow` sheet, tinted `hue::IceBlue`; frame = 16-way facing + 1, as `AddArrow` does. `Fly` now returns its missile. |
| Glacial Spike | 75%; shatter 50% |
| Frost Nova | `FrostNovaPercent` 250 -> 200 |
| Brittle Ground | `Tint::Glint` on the held ice sheet |
| Frozen Sentinel | Vanilla `Guardian` sheet, tinted ice blue: rise (row 0, on cast), stand (row 1, from field clock 15), sink (row 2, last 3 ticks) - `SentinelArt`. Its bolts are drawn at 50%. |
| Blizzard | Shards at 50% (floor 16 kept, where they break); 3 per 4 ticks. Only the one on `_mirange % 4 == 0` strikes and sounds; the others carry `var5 = 1` and are picture only, so damage is unchanged. |
| Ice armours | The hero tint is `Tint::Glint` in `ColdArmourHue`: Frozen white, Shiver yellow, Chilling deep blue |
| Whiteout | Vanilla `FireWall` rise (row 0), tinted ice blue, on each step's three tiles; the `WhiteoutWall` flight is gone. The fallback Ring now fires only when no flame could be raised. |
| Frozen Orb | 50%, `Tint::Glint`, `_miAnimAdd = 2` (spins twice as fast) |

**`Tint::Glint`** (new, `oracool/missile_tint`): bands of light run through the brightness levels, driven only by the game clock. That way a looping or held sheet has no seam at its wrap; `HueCycle` keys its bands to the frame and jumps at every loop. rgb 0 keeps the sheet's own colours; any other value glazes toward that hue at 0.6.

## The Nephalem portal

- **Core:** `tools/BuildRiftPortals.ps1` fills the gold oval's core at (40,27,3), was (96,66,8). The sheet was rebuilt and `oracool.mpq` repacked; the purple and town sheets came out byte-identical.
- **Shadow:**
  - `AddRiftPortal` lifted every rift portal 27px, a lift measured for the Rift Monument's arch. Inside a rift the exit therefore floated above the floor. The lift now applies only in town.
  - The rim pixels the town sheet strips as its "shadow" were hue-shifted into gold on the gold sheet. They are now a neutral dark grey at half their brightness. A first try left them blue-grey, and they showed as blue specks.
- **Behind the hero:** `LayArrivalExit` tries North, NorthWest, NorthEast, West, East, SouthWest, SouthEast, then South. Before, it tried South first, the tile in front of the hero.

## Left for the user

- **Absolute Zero:** it needs a new animation asset, which the user will make with ChatGPT. The note stays in `development.md`.
- **Monument shadow:** the Rift Monument's gold portal uses the same sheet, so it also shows the darker core and the grey rim shadow.

## Test

- Debug build and ctest: 892/892 passed.
- The Debug `diablo.ini` md5 is unchanged across ctest.
- Nothing seen in play yet.
