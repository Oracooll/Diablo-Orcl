---
date: 2026-08-14
version: 1.5.2
area: Waypoints / save format / UI art / startup / menu chrome
---

# Twenty-Five Waypoints, and the Bytes We Could Not Borrow

Hellfire went into the build folder, which added eight dungeon levels the travel list had never heard
of. This is the list catching up - and the first change in this project's history that could not pay
for itself out of `PlayerPack`'s spare change.

> extend the waypoint panel to 25. make each waypoint around 3-4 rows high and place a nice quality WP
> picture in front of it and make the list scrollable.

and then, after asking what a vanilla spellbook row measured:

> make wp row 43px + 2px gap between two rows. make the wp picture fit 43x43 invisible frame as to its
> diameter tangents on the 43px row borders. so we will have 25 rows x 45px total height of wp list and
> it will be scrollable.

**1.5.0 rather than 1.4.10**, on the user's call, and named for Hellfire rather than for anything in
this report: "bump up to 1.5.0 now because intrducing HF is a big step."

## Part one: 43 is not an arbitrary number

The row height came out of a question - what were spell rows in the vanilla spellbook? The baseline
commit answers exactly: `constexpr Size SpellBookDescription { 250, 43 }`, seven rows to a page,
each holding a 37x38 icon beside two 18px lines. So 43 is the game's own list rhythm, and the travel
list now shares it instead of the 35px it had invented for itself.

The pad fills that row outright - no inset, its diameter tangent to both borders - which meant recutting
the art. The old `ui\waypoint_icons.png` was 60x30, two 30x30 cells. The instruction on how to get to
43 was explicit and worth recording because it is the general rule for this vault:

> i want srunk down detailed better than enlarged small res asset.

So `tools/CutWaypointIcons.ps1` reduces from the 1536x1024 master in `02-source-art/hud-icons/`, never
from the shipped sheet. Two things it has to get right:

- **The background was never black.** Rendered in a viewer the master looks like a sigil on black with
  a radial glow. It is not: the file is already `Format32bppArgb` with real transparency, and the
  "black" is alpha over a black viewer ground. The first three attempts to find the sigil measured
  *luminance* and returned nearly the full frame every time, because that glow is bright. Measuring
  alpha instead found it immediately. No chroma key is involved and none should be added.
- **Both cells share ONE crop rectangle.** The lit state's glow reaches about 110px further up and
  down than the dormant stone does - measured, dormant `y 116..861`, lit `y 73..927`. Cutting each
  state to its own bounding box would scale them differently, and the pad would visibly jump in size
  the instant a waypoint was activated. The dormant stone is the reference; the lit state gets the
  identical rect and its glow is clipped at the cell edge, which is exactly what "tangent to the row
  borders" asks for.

The measured stone is 745x746 - square to within a pixel, which is the confirmation that a square
crop is not distorting a diamond. Reduction is `HighQualityBicubic` through a **premultiplied**
intermediate; straight-alpha bicubic blends the RGB of fully transparent pixels into the edge and
leaves a dark fringe all the way round the pad.

No engine change was needed for the new size. `hud_art.cpp` already derives the cell as
`WaypointIconsArt.width / 2` by the sheet's own dimensions - a comment there says it is written that
way "so a recut at a different icon size still lines up", and this is the recut that collected on it.

## Part two: the eight rows that could never have lit up

Extending `WaypointNames` to 25 is a one-line kind of change. It would also have been a lie.

`AddWaypointSigilObject` (objects.cpp) placed a sigil on level 0 and levels 1-16, and returned early
for anything higher. Nothing else in the game unlocks a waypoint - the only way is to stand on its
sigil and click it (`OperateWaypoint`). So a 25-row list on top of a 16-level placement rule is eight
rows that can never light up, can never be travelled to, and give the player no way to find that out
except by walking the whole Crypt looking for something that was never placed.

The bound is now the travel list's own last index rather than a literal, so the two cannot drift:

```cpp
} else if (currlevel >= 1 && currlevel < static_cast<int>(Player::MaxWaypointSlots)) {
```

The reverse case needed handling too. In a plain Diablo game there are no levels past 16, so listing
the Nest and the Crypt there would be the same eight dead rows from the other direction.
`VisibleWaypointCount()` returns 25 or 17 off `gbIsHellfire` - the storage is always 25, only the
list shortens.

The names keep the existing convention, which on inspection numbers by **absolute** dungeon level and
not by level-within-region: that is why the list already read "6. Catacombs Level 5". Hellfire's two
regions carry on counting rather than restarting - "18. Nest Level 17" through "25. Crypt Level 24".

## Part three: the bytes we could not borrow

Every persistent field this project has added lives in `PlayerPack`, and every one of them until now
was paid for out of a reserved byte. `pWaypointUnlockedNormal` was `reserved2[2]`.
`pWaypointUnlockedNightmare` was `wReserved8`. Hell and Torment split `reserved3[4]`. The struct is
`#pragma pack(1)` and `pfile.cpp`'s `ReadHero` accepts a "hero" blob **only** when its size matches
`sizeof(PlayerPack)` exactly, so keeping that size fixed is what has kept characters loading across
every previous change.

Twenty-four waypoints need 24 bits per difficulty. The four masks were `uint16_t`. That is 4 bytes
short, and the wallet is empty - one `uint8_t reserved` is all that remains. Repurposing the spare
top bytes of `pDamAcFlags` (four bytes carrying a one-byte value) or of `pDifficulty` (four bytes
carrying 0-3) would have fit, and would have been the kind of cleverness that is discovered years
later by someone debugging a save corruption.

So the masks widened to `uint32_t` and the struct grew by 8 bytes, and every character saved before
this stopped loading.

That was affordable **precisely now and probably never again**: adding `hellfire.mpq` had already
moved saves from `.sv` to `.hsv` the same afternoon, so the old files were out of reach regardless.
Any future waypoint growth is free - there are 8 spare bits - but anything else wanting room in this
struct should still go hunting for reserved bytes first.

`writehero_test` caught it, which is the entire reason that test exists. Its golden SHA-256 is
re-baselined for the fourth time, with the reason written next to the previous three, and its own
`SwapLE` helper gained the four widened fields - a no-op on a little-endian host, which is what
`run_big_endian_tests.sh` is for.

## Part four: scrolling, borrowed rather than invented

25 rows at a 45px pitch is 1125px of list against a 595px viewport, so a little over half of it is
off-screen at any moment.

The scrolling is the Abilities window's, to the pixel - `panels/spell_book.cpp` had already solved
this: pixel offset rather than line index, `out.subregion(...)` so a row straddling an edge is clipped
instead of spilling onto the title band, a groove drawn with `DrawThemedFill` and a thumb with
`DrawOrnateSeparatorVertical`, thumb height `viewport² / listHeight` against a 24px floor. The event
log's line-based scroll was the other candidate and is the wrong shape for a list of fixed-height
rows.

Two details are this list's own:

- **The 2px gap belongs to neither row.** `listY % RowPitch >= RowHeight` returns no hit, so a click
  landing between two waypoints does nothing rather than being rounded into whichever one is above.
- **Hover and click still cannot disagree.** `MouseToEntry` was already the single source for both;
  it now adds the scroll offset in one place. Scrolling is exactly the case where two separate
  implementations would have drifted apart.

The wheel is gated on the cursor being over the panel, like the character sheet and spell book, so it
still zooms the dungeon everywhere else while the list is open.

## Part five: Hellfire stops announcing itself (1.5.1)

> i don't want Diablo Orcl to ask Diablo or Hellfire on boot or after initial installation. Hellfire
> is Default.
> I also don't want the Hellfire logo on the menu screens. I preffer the Diablo logo. Use it
> exclusively unless we are breaking some law.

**The prompt.** Vanilla runs `UiSelStartUpGameOption()` from `DiabloInit` when `hellfire.mpq` is
present and `GameMode.gameMode` is still `Ask`, which is the shipped default. Flipping that default
to `Hellfire` is the obvious fix and on its own it would have changed nothing here: the local
`diablo.ini` already read `Game=0`, and a stored value beats a default forever. Worse, `Ask` is
deliberately hidden from the settings menu ("Ask is missing, cause we want to hide it from
UI-Settings"), so a config stuck on it has no in-game escape.

So both: the default is now `Hellfire`, **and** a stored `Ask` is migrated on boot, guarded on
`gbIsHellfire` so an install without the archive is never promised a game it cannot deliver. The
dialog call is gone; `UiSelStartUpGameOption()` itself stays in the tree unreferenced, the same way
the multiplayer screens do - unreachable, not deleted. Choosing Diablo from the settings menu still
works, and so does `--diablo`.

**The logo forks in three places, not one.** Worth writing down, because finding only the first would
have looked like a finished job:

| where | Hellfire art | Diablo art | what it is |
|---|---|---|---|
| `DiabloUI/diabloui.cpp` `LoadUiGFX` | `ui_art\hf_logo2`, 16 frames, key 0 | `ui_art\smlogo`, 15 frames, key 250 | every front-end menu |
| `gmenu.cpp` `sgpLogo` | `data\hf_logo3`, 430px | `data\diabsmal`, 296px | the in-game pause menu |
| `DiabloUI/title.cpp` | `ui_art\hf_logo1` + `hf_titlew.clx` | `ui_art\title` + `ui_art\logo` | the startup title screen |

None of the three is a filename swap. The front-end pair differ in frame count *and* transparent
index; the pause-menu pair differ in width by 134px; and the title screen's Hellfire branch composes
an animated background with an optional widescreen strip, where Diablo's paints a background and
lays a logo and a copyright line over it. All three now take the Diablo path unconditionally.

**Then the Diablo logo turned out to be the wrong Diablo logo (1.5.2).**

> the diablo logo doesn't look nice. where is the vanilla logo?
> I don't want the main menu to say exit hellfire.

Diablo ships the same flaming letters at two sizes and vanilla only ever used the small one in the
menus. Swapping `hf_logo2` (640x154) for `smlogo` (390x154) had not changed the masthead so much as
**shrunk it by 40%**. `ui_art\logo` is the other one - 550x216 - and it is what the title screen has
always used.

Both are 15 frames keyed on index 250, so the load is a filename swap. The layout was not. Frame 0 of
each was measured rather than assumed, and both are full-bleed: ink runs from y=0 to within 2px of the
frame's bottom, so there is no transparent padding to absorb the extra 62px. Three screens hang off
the masthead and all three had to move:

- **The hero screens.** `HeroTitleTop` 61 -> 117. Not the full 62, and that is the interesting part:
  the whole 62 would have left `HeroContentTop..HeroContentBottom` at 307px against a six-class list
  of 6 x 52 = **312**, so the class list would have overflowed into the action row's clearance by
  5px. 117 gives a 313px band - six classes fit with a single pixel spare, and 21px of air under the
  flames instead of the old 27. **The band now holds exactly six class rows**, which is written into
  the header: a seventh class, a taller row or a lower title all need the list to start scrolling.
- **The main menu.** Raised by exactly 62 so the logo's *bottom* edge stays put, because the menu list
  is pinned at `uiPosition.y + 192` and the logo growing downward would have run 24px into the first
  entry. The old 38px of air under the flames is preserved and the logo grows upward instead.
- **The settings screen.** Its title offset and `ListTop` both moved 161 -> 223 and 204 -> 266.
  `blockHeight` is derived from `ListTop` and `blockTop` from `blockHeight`, so the block re-centres
  itself and nothing else there needed touching.

`Exit Hellfire` is gone from the main menu too - always `Exit Diablo` now. It was the last string
announcing the expansion.

**On the legal question.** There is nothing to avoid here. Both logos are original Blizzard art read
at runtime out of archives the user owns, and this project redistributes neither - they live in
`diabdat.mpq` and `hellfire.mpq`, never in `oracool.mpq`. Which of the two gets drawn has no legal
dimension at all; what would matter is shipping either one, and nothing here does. The only cost is
cosmetic honesty: the game now shows the Diablo logo while running Hellfire's levels and monsters.

## Files

- `tools/CutWaypointIcons.ps1` (new) - the 43x43 cut, the shared crop, and why both are what they are.
- `Packaging/resources/{assets,oracool_assets}/ui/waypoint_icons.png` - 60x30 replaced by 86x43.
- `Source/oracool/waypoint_menu.cpp/.h` - 25 names, `VisibleWaypointCount`, the 43/2/45 geometry, the
  viewport, the scrollbar, scroll state and the two scroll entry points.
- `Source/objects.cpp` - `AddWaypointSigilObject` places a sigil on every level the list can offer.
- `Source/player.h` - `MaxWaypointSlots`, and `_pWaypointUnlocked` grown to it.
- `Source/pack.h/.cpp` - the four masks widened to 32 bits, and the note on why this one grew the
  struct when nothing before it did.
- `Source/diablo.cpp` - the wheel, both directions.
- `test/writehero_test.cpp` - re-baselined golden hash, reason four, and the widened fields in `SwapLE`.
- `Source/diablo.cpp`, `Source/options.cpp` - the startup dialog removed, `Ask` migrated, the default
  changed to Hellfire (1.5.1).
- `Source/DiabloUI/diabloui.cpp`, `Source/gmenu.cpp`, `Source/DiabloUI/title.cpp` - the Diablo logo in
  all three places it used to fork (1.5.1), then the big `ui_art\logo` rather than `smlogo` (1.5.2).
- `Source/DiabloUI/hero/hero_layout.h`, `mainmenu.cpp`, `settingsmenu.cpp` - the three layouts moved
  to clear a masthead 62px taller, and the note that the hero band now holds exactly six class rows.
- `Source/DiabloUI/mainmenu.cpp` - "Exit Diablo" unconditionally.

## Verification

Debug build clean at `ORACOOL_VERSION` **1.5.0**, **1.5.1** and **1.5.2**. Tests **352/354** at each -
`Drlg_l1.CreateL5Dungeon_diablo_3_844660068` and `Timedemo.WarriorLevel1to2`, the same two
pre-existing failures as every build this session.

`writehero_test` failed first, as designed, and was re-baselined from
`9c2d0684...` to `67e45a2e...` only after confirming the cause was the intended struct change.

`oracool.mpq` repacked (69 files, 14,797,418 bytes) and `ui\waypoint_icons.png` extracted back out of
the packed archive and SHA-256 matched against source - `792605c8c79ae5b3...` both sides. The
closed-loop check every asset change in this project gets, after the time a pack step reported success
and shipped a stale file.

The cut was inspected at 8x on a dark ground before shipping: both states identical in size and
position, the diamond tangent on all four edges, corners fully transparent, and no dark fringe from
the reduction.

**Not seen in game.** To confirm:
- the list at 43px rows - whether the name at FontSize24 sits right beside a pad that now fills the
  row, and whether the 2px gap reads as separation or as a seam;
- scrolling with the wheel, that it stops cleanly at both ends, and that the wheel still zooms the
  dungeon when the cursor is outside the panel;
- clicking in the gap between two rows, which should do nothing at all;
- **a sigil actually appearing on a Nest or Crypt level** - this is the part with no test behind it.
  `GetRndObjLoc` and the waypoint's own placement scoring have never run on `DTYPE_NEST`, and while
  `IsWaypointTileClear` has a rule for `DTYPE_CRYPT` it has none for the Nest;
- that a fresh character's list shows 25 rows in this Hellfire build, and that the first sixteen still
  behave exactly as before.
