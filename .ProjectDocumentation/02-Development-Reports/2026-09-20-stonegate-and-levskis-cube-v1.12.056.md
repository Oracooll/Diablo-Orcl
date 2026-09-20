# The Stonegate and Levski's Cube (v1.12.055-056)

**Date:** 2026-09-20 - Debug only - **green: 821/821 (build 28)**. Overnight work on the user's word ("proceed with
the implementation of all of them"), from the answers on the Cube plan page (https://claude.ai/artifact/LSxpjNZdSu3skd6nJ5HmAM)
and the Stonegate request. Nothing seen in play.

## The Stonegate (v1.12.055, RfA-19 / batch 42 - delivered within an hour of the request)

- `oracool/stonegate`: a town object at (62, 74) with fallbacks (an OBJ_STAND wearing its own art, as the Roar),
  `objects\orclgate.cel` - 17 frames of 192 x 304 built by the new `tools/FramesCel.cs` + `build_stonegate_cel.cmd`
  from the delivered real-alpha frames (closed, eight lit gold, eight lit violet). A click opens the golden portal
  in the opening (a Nephalem Rift), a second the violet (a Guardian Rift), a third closes it; the stone's lit loop is
  driven from `ProcessObjects`; the open/close sounds are batch 42's.
- The portals are two new missiles, `RiftPortalGold`/`RiftPortalPurple` (spelldat/misdat), PNG strips of sixteen
  96 x 256 frames composed from the delivered frames into `missiles\portal_gold.png` / `portal_purple.png`; a
  rift portal stands until the gate closes (no range).
- NOT built: the rifts themselves. Walking into a portal does nothing; the event log says so. The Roadmap card
  carries the design to come (a rift level, a guardian, keystones - the two keystone icons are delivered).
- `IsLevskiRoarObject` was "any stand in town" and now also matched the gate - the click hook tells them apart by
  identity (`IsStonegateObject`), and the Roar's predicate excludes the gate as of the next build.

## Levski's Cube (v1.12.056, RfA-20 / batch 43 - the object delivered first)

Decisions: D1 a town object; D2 a carved name, no towner; D3/D4 the Powers "later"; D5 reagents; D6 no token;
D7 all four crafts; D8 the recipe SPLIT to the artisans.

- **Hosts** (`TransmuteHost` in oracool/crafting.h): every recipe belongs to one book. Griswold's Forge (a new
  tab, `TalkID::SmithTransmute`): Reforge, Ennoble, Consecrate, Awaken, the three Rerolls, Make Ethereal, Mend.
  Ogden's table (a new tavern menu line): Refine Gems, Ascend Runes, Free the Sockets, Temper Jewels, Recolour
  Gems, Punch Sockets. Gillian's hearth (a new menu line): Rework Charms, Recast Set Pieces, Enrich Magic, Cleanse
  Shards. The Cube (the town object): the additions below. One window serves all four: `OpenLevskiWindowFor(host)`
  closes the store and opens the book; recipes of another book are zero-height rows, and the readiest-recipe
  fallback is per book.
- **New recipes** (19-25): Rejuvenation (3 healing + 3 mana), Full Rejuvenation (3 rejuvenation), Unbind the
  Level (a wearable that asks a level + 1 Shard of Ease -> `_iOracoolLevelFree`, Kanai's Work of Cathan; **item
  format 12**, one byte), and the four Diablo II crafts: a plain wearable + a jewel + a rune + a perfect gem ->
  re-rolled as a Rare with two fixed properties on top (Blood: life leech + life; Caster: mana + magic; Hit Power:
  knockback + to-hit; Safety: all resistances + damage taken), applied through `SaveItemPower` and recorded in the
  affix list, named "<Craft> <name>". Upgrade Rare = the existing Ennoble; the unique base upgrade = the existing
  Awaken; both Griswold's.
- **The object**: batch 43a's thirteen frames (twelve idle, one open; 96 x 160) packed by FramesCel as the town
  object's sheet `orclroar.cel` (the old Roar sheet kept in TEMP and in git history). The idle loop driver and
  the open pose come in the next build; the window art (43b), button and glyphs (43c) and sounds (43d) when they
  land - the delivery sweep runs every five minutes.

## Tests and verification

- Build 28 green: 821/821 tests passed. Build 27 died on crafting.cpp (no IDI_REJUV/IDI_FULLREJUV bases - the potions are found by misc id now; SaveItemPower is file-local, so an ApplyOracoolItemPower wrapper was exported). The two tests that pinned the sound table's length and the missile count were
  extended.
- Not verified in play: everything above. First things to look at: the Stonegate in the south-east of town and
  its two portals; the Forge tab at Griswold's; Ogden's and Gillian's new menu lines; a rejuvenation transmute at
  the Cube; a craft; the Required Level line on any item (v1.12.054).

## Related

- [[2026-09-20-item-level-requirements-v1.12.054]] - the level requirement the Unbind rite removes.

## v1.12.057 - the Cube's own window (batch 43b/43c), the same night

Batch 43b (the window painting) and 43c (the TRANSMUTE button and four glyphs) landed at 03:05 and 03:15. GPT
painted the layout the brief asked for - and the brief had described the Roar's window as "a recipe list on the
right", which it is not: the Roar's painting carries the 4x2 block of salvage plates there. So the new painting has
NO salvage cells and NO recipe-book plate; it has the twelve wells at the Roar's grid origin (26,106), a 142x26
recess under the grid, and a bezel on the right with eight line positions at a 20 px pitch and a scroll track.

- `oracool/levski_cube_skin.h`: the painting measured by row and column scans (recess interior 140x24 at (19,233),
  so the button rect is (18,231) 142x26; bezel interior (179,77) 154x172, dividers every 20 px; track x 344-348;
  the red X at (364,3) over the corner ornament).
- `CubeSkin()` in levski_roar.cpp: the Cube host with `ui\cube_bg.png` in the archive wears it. Under it the
  window has two controls - the X and TRANSMUTE (the 43c button art at rest / pressed; the game's gold label until
  it loaded) - and the host's recipes are listed IN the bezel, one name per line, gold when the grid can run it,
  whitegold otherwise, white on a filled band when selected; a click selects (again clears), the wheel scrolls a
  line per notch while the cursor is on the window, a thumb in the painted track shows the position, and hovering
  a line puts the name and formula on the info panel (the line has room for the name alone).
- The artisans keep the Roar's painting, whose salvage block is now Griswold's alone: the seven tier plates are
  drawn, hoverable and clickable only on his Forge book. Ogden's and Gillian's books show the painting's empty
  carved cells there. (The painting's title still reads "Levski's Roar" on the artisans' books - a wart to paint
  over when their windows get their own dress.)
- Sounds: `UiEventSound::CubeOpen` (the Cube object opening its window) and `CubeTransmute` (a transmute on the
  Cube's book; the artisans keep the old transmute sound). Batch 43d has not landed, so `cube_open.wav` and
  `cube_transmute.wav` are COPIES of rift_open / transmute holding the slots - the archive test insists every path
  names a real file. Overwrite them when the delivery comes; no code changes.
- The four 24 px recipe glyphs (flask, hammer, arrow-through-gem, broken chain) are parked in the delivery folder:
  the bezel's lines are 20 px tall.

Build 29 green: 821/821. The button art missed build 29's pack list (copied a minute after configure); build 30 (v1.12.058) carries it.

## v1.12.061 - batch 43d, the Cube's sounds

`sfx/cube_open.wav` (0.68 s) and `cube_transmute.wav` (0.78 s), 16-bit mono 22050 Hz, replace the placeholder
copies in `Packaging/.../sfx/ui/`; no code change - `UiEventSound::CubeOpen/CubeTransmute` were wired at
v1.12.057. The RfA-20 delivery report is in. The five-minute sweep did not see the `sfx/` folder for two hours
after GPT wrote it (the user relayed "completely delivered"); OneDrive sync lag, most likely - when a delivery
is announced, list the folder directly. Build 34 green: 821/821.
