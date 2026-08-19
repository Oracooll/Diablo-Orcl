---
date: 2026-08-19
version: 1.8.7
area: Wiki - diffed against the 181-report vault, three new sections
---

# What the Diary Knew and the Wiki Did Not

The wiki was read back against every dev report in the vault, on the user's instruction: "make sure
everything relevant mentioned in the Obsidian diary is available in the Wiki." Reading all 181
titles and then the bodies of the ones naming mechanics the wiki never mentioned produced a
concrete gap list rather than an impression.

## What was missing

Three whole systems, each with its own reports and none of them in the wiki beyond a passing
sentence:

- **The socket economy** - 35 gems, 5 runes, 3 runewords, 6 charms, and the crafting recipes. The
  affixes page had one note saying sockets roll on the drop paths; nothing said what a gem *does*.
- **Saving** - autosave triggers, save-on-exit, the deliberate absence of Continue, per-difficulty
  waypoint persistence, and what the event log actually records.
- **The debug console** - 56 commands, the fastest route to any item or floor, documented nowhere.

Plus five smaller holes inside existing pages: Magic and Gold Find, ethereal items, the run toggle,
the respec at Adria, Wirt as a gambler, and the quest log starting full.

## What shipped

**`tools/BuildWiki.ps1` grew five parsers**, so the new pages are generated like every other one -
none of these numbers are re-typed:

- `oracool/gems.cpp` - gems and runes off one table, told apart by their index constant's name. The
  rows use designated initialisers, which is exactly why they can be parsed: every number arrives
  labelled, so nothing depends on column order.
- `oracool/charms.cpp`, `oracool/runewords.cpp`, and the drop-hook constants in `items.cpp` (gem /
  rune / charm percentages, the socket chance and its weights, the ethereal chance).
- `debug.cpp`'s `DebugCmdList`, and `oracool/auto_save.h` - whose `ScheduleAutoSaveFor*`
  declarations **are** the trigger list, so it cannot drift the way a hand-typed one would.

**Three new pages**: `sockets.html`, `saving.html`, `debug.html` - now 23 in the sidebar, in
`wiki.js` and `BundleWiki.ps1` alike.

**Six existing pages extended**: mechanics (Magic/Gold Find, ethereal, run toggle, respec), world
(the respec price, Wirt's unidentified stock, waypoint persistence, the full quest log, crafting),
start and index (new cards and veteran-list entries), controls (R, and Enter as the debug console),
affixes and history (the megaplan phases, and what each one added).

## One parse worth recording

`etherealPercent` first came back as 25 - the regex anchored on the *name* `TryMakeDroppedItemEthereal`,
whose first occurrence in items.cpp is the call site, so `.*?GenerateRnd(100) >= (\d+)` walked into
the neighbouring socket roll instead. Anchoring on `void TryMakeDroppedItemEthereal` fixed it. The
same shape of error is why the socket chance is now parsed too rather than left as the literal 25
that happened to be right.

## Verified

Regenerated and rebundled: 23 pages, 68 sprites inlined, 9.73 MB. All three new sections were
opened in the bundle and checked for the id-collision failure that blanked pages in the first build
- 56 debug rows, 35 gem/rune rows, 13 autosave tags, all rendering inside their own section. The
hosted Artifact was republished to the same URL.

No engine code was touched, so no build or version bump: this is documentation.
