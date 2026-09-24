# First /dev batch, and the Skeleton King crash — v1.12.171

2026-09-24

> i got this while entering skeleton king chambers. also while you are fixing this check
> development.md and process everything there.

The first batch of in-game `/dev` notes (twenty lines, eleven items) plus an assert. Every processed
note is in `development-archive.md` with its outcome; two stay open in `development.md` (vendor
stock audit, XP versus Diablo II), both handed to audits before any change.

## The crash: theme rooms on every set level

`assert(InDungeonBounds(position))` in `DoLighting`, entering the Skeleton King's lair.

`diablo.cpp`'s set-level fresh-entry branch has called `oracool::FinishRiftLevel(true)` since
v1.12.062, and `FinishRiftLevel` checked only `fresh`. So every set level — the lair, the Chamber of
Bone, Poisoned Water, Lazarus, the arenas — ran `CreateThemeRooms` on its first visit, with the
theme table still holding the **previous floor's** themes (set levels never call `InitThemes`).
A Shrine or Library theme whose region is not on the new map leaves `themex/themey` at (0,0) and
places its candles at (-1,0): `dObject[-1][0]` is written, `AddObjectLight` lights it, and the
assert fires. Where a stale theme did fit, a quest map got stray shrines and books.

- `FinishRiftLevel` returns unless `InRift()` — its neighbour `RiftLevelPopulated` always did.
- `AddObject` refuses a position off the map. In Release the bad write was silent memory
  corruption; the lighting assert only caught it because a candle carries a light.
- The assert itself is kept: it found this.

## The rift map crash

The automap header prints `QuestLevelNames[setlvlnum]`. `SL_RIFT_NEPHALEM` and `SL_RIFT_GUARDIAN`
joined the enum with no rows in that table, so Tab in a rift read one and two past its end. Rows
added, and a `static_assert` ties the table's length to `SL_LAST + 1`.

## The gait was never written

v1.12.146 moved the run toggle into the INI, but `ToggleRun` only set the value in memory and the
INI is not written on exit. `ToggleRun` now saves.

## Wirt's Gamble tab ignored Escape and Space

`StoreESC` had a case for his Shop tab and none for Gamble. Space closes stores through the same
function, so both did nothing. Enter was left alone: it buys the selected item behind the Yes/No
prompt on **every** shop grid through one function, and is the keyboard route to a purchase.

## Longer /dev notes

`TalkMessage` and the eight-line history were `MAX_SEND_STR_LEN` (80, the network packet). Now
640 bytes; `NetSendCmdString` still copies into its own 80, so nothing wider can reach the wire.
The drawn box is the real limit (`DrawString`'s fit count truncates the input), so for a line
starting `/dev` the box grows upward a row ahead of the text, to 18 rows. The font wraps per
character, so typed width ÷ 250 is the row count.

## Layout notes

- Belt consumables `+4` → `+2`; menu glyph `+2`, portal glyph `+1` (glyph only, plate stays).
- Gillian's Reroll and Imbue: two passes of the themed dark fill in the small frame, under the item.
- Stash: the gold pile is a press-and-release button that sinks 2px down-left; the withdraw box
  drops so its foot is the grid frame's foot (y 628), text and IME rect with it.
- Durability icons draw before the store, inventory and spellbook, so windows cover them.
- Wirt's menu: "Leave Wirt".

Debug build and 832/832 tests clean.
