# Engine rule: every colour has its own .trn; only the raw gold has none (v1.10.015)

**Date:** 2026-09-07
**Request:** "lets make a rule - only raw gold stays without trn file. also - no more colors borrowing, each color to have it own trn file with appropriate name. do it."

## Before

Two colours were made in memory after loading: the set green was vanilla yellow.trn with its band shifted onto the injected green minis (2026-08-15), and orange was vanilla orange.trn re-pointed from those same minis onto PAL16_ORANGE after the green ramp took them (2026-08-16). What the table named was not what was drawn.

## Now

`oracool_green1.trn` (GN-1) and `oracool_orange1.trn` (OR-1) are files of their own, generated from the two vanilla files with exactly the shifts LoadFont used to apply; the table names them and LoadFont edits nothing after loading. ColorGold keeps its empty slot: the raw gold glyph, the one colour with no file, by rule. Vanilla orange.trn stays on disk untouched and joins the not-used list (it points at minis the green ramp overwrote, so it would draw green in a level).

The rule is written at the top of the legend and in the "Adding a colour" steps. Orcl files: 12; vanilla files: 16, of which 5 are unused.

## Tests

Suite 692/692; oracool.mpq repacked with 421 files. The legend and the wiki's Text colours page regenerated and republished. Committed locally; the RTM folder refreshed with the exe and the archive.
