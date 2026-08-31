---
date: 2026-08-19
version: 1.8.12
area: Full reaudit of 1.8.0-1.8.11 against the wiki
---

# Auditing the Whole 1.8 Line Against the Wiki

Every mechanic the 1.8 line introduced, checked against the SOURCE rather than against the dev
report that announced it - a report says what was intended, and the wiki should agree with what
shipped.

## Verified correct, against the code

- **alvl** - `AreaFloorCount 24`, `FloorsPerArea 4`, `AreaCount 6`, `MaxAreaLevel 96`. The ladder
  page and the mechanics page both match.
- **mlvl** - `ItemLevelOfMonster` is the area level +3 for a unique, +2 for a champion, clamped to
  96. The wiki says exactly that.
- **Banded qlvl** - seventeen groups mapping the authored 1-51 onto a 1-60 ladder, with 99 passed
  through as the never-drops sentinel. The claim that every base is obtainable by Hell/Hell (alvl
  61-64) holds: the banding tops out at 60.
- **Tier scales and weights** - `{100,100,100,100} / {180,140,125,400} / {290,180,150,1200} /
  {420,220,175,3000}` and `60/25/10/5`. Already parsed by the generator, so they cannot drift.
- **Vendor tiers** - the INI default is 35%, which is what "about a third of the shelf" means.
- **Ethereal** - 5%, +35%, half durability.
- **Sockets v2** - 25% socketed, weights 60/25/8/4/2/1, cap by footprint.

## Four discrepancies, all fixed

**The quality band curves were hand-copied into the wiki's JavaScript.** `RARE_BY_BAND`,
`BUFFED_BY_BAND` and `PRIMAL_BY_BAND` were typed into affixes.html as literals. They happened to
still be right - `{10,30,70,110}`, `{10,25,50,80}`, `{0,5,15,40}` - but a retune in item_tiers.cpp
would have left the page quoting the old curve with nothing to catch it. This is the exact failure
mode the generator exists to prevent, sitting inside the generated wiki. Now parsed.

**The core-mechanics page still described Sockets v1.** It said "a quarter of plain, TIERLESS
equipment drops with one to THREE sockets" - both halves wrong since 1.8.8, where the tier
exclusion was removed and the cap became the item's footprint.

**The version history stopped at 1.8.7.** Four builds - the socket rework, the 33 runes, the
reaudit and the runewords - had shipped without a line.

**The overview card said "three runewords".** It says 370.

## Method note

The sweep that found these was a grep for stale numerals across every page (`one to three`,
`five runes`, `three runewords`, `358`), run after the targeted checks. Worth keeping: prose drifts
where tables do not, because a table gets regenerated and a sentence does not.

## Verified

Wiki regenerated and rebundled; the odds table recomputes from the parsed curves and still reads
2.00 / 1.00 / 0.00% at ilvl 1-24 through 22.00 / 8.00 / 2.00% at 73-96. No engine code changed in
this unit, so the test state is 1.8.11's: **452 tests, the usual two**.
