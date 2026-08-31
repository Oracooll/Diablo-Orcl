---
date: 2026-08-15
version: 1.5.63
area: Paladin skills / asset pipeline
---

# Seven Icons From One Sheet, and the One We Left On It

> Now i have added a file with brand new icons which you need to extract and put in Paladin Skills
> list. Names of skill are included.

`Paladin Skills.png` — a 1448×1086 contact sheet, 4×2 grid, white glyphs on a green key with a label
under each. Eight icons: the five new skills, plus **Smite** ("ignore this skill for now. Don't add
it."), plus fresh drawings of Charge and Zeal.

## Why all seven come from the new sheet, not just the five

Charge and Zeal already had icons, cut in the previous pass from two separately-framed files. Reusing
those and appending five would have put two different hands on the same list — different line weight,
different drop shadow, different sense of scale. The new sheet redraws them, so the cutter takes all
seven from it and the old `charge.png` / `zeal.png` path is retired.

## What the grid made harder than the framed files did

The old cutter took one file per icon and used a border flood fill. Neither assumption survives.

**The labels are the same white as the icons**, so no colour test can separate them. They are
separated by **geometry** instead: the sheet has four clean horizontal bands — icons, labels, icons,
labels — with empty green between. Every bbox is measured inside an icon band only.

The band edges are **found by scanning, not hardcoded**, and the script throws if it doesn't find
exactly four bands and four columns. A re-export at a different size still cuts correctly; a re-export
with a changed layout fails loudly instead of quietly cutting somebody's label into a skill icon.

```
  icon band y 147..394        column x 76..349
  icon band y 552..808        column x 422..676
                              column x 751..1027
                              column x 1108..1388
```

**The glyphs are unframed and their proportions differ** — Charge is 272×173, Fist of the Heavens is
190×257. The old script *asserted each cut was square*, which was correct when a drawn frame defined
the cut and would be actively wrong now.

Each glyph is padded to square before scaling. The detail worth recording is that the padding box is
**one size for the whole strip**, taken from the largest glyph (272px), not per-icon. Padding each
glyph to its own square would scale every one to fill 38×38, so a wide icon and a tall one would end
up drawn at different strokes-per-pixel — and the set would stop looking like a set. One box keeps
their relative sizes as the artist drew them.

The border flood fill is also gone, replaced by a plain colour threshold. That fill existed because
the *first* icons had gold highlights a threshold could punch holes through. This artwork is white and
black with nothing near the key, so the simpler test is also the safe one here.

## Smite is skipped in the cutter, not filtered later

> **Amended at 1.5.64.** Smite's *drawing* is now used, for **Shield Bash** — cell 5 shows a shield
> driven into a recoiling figure and reads as a bash, where the cell the sheet labels "Shield Bash"
> is a shield with an impact burst beside it. The two cells swapped roles; the skipped cell is now
> cell 6. Everything below still holds, and the change was one line in `$layout`, which is what the
> cell → skill mapping is shaped that way for.


Its grid cell is named in the layout table and mapped to `$null`. So the strip has seven cells rather
than eight-with-a-hole, `PaladinSkillCount` is 7, and the enum still lines up with the sheet
index-for-index — `GetPaladinSkillIconIndex` stays the identity. Adding Smite later is one edit: give
its cell a name.

## What the five new skills do

Nothing yet, and the code now says so rather than leaving it to be inferred.

They arrived as art plus one line of description each — the same shape the auras and the Barbarian
skills arrived in, and the same answer applies: listed, described, hover-panelled, and inert until a
gameplay pass. `IsPaladinSkillImplemented` exists to state that in one place.

Their **levels and mana costs are placeholders I chose**, because none were given. They are a ladder
rather than five guesses — the two shield moves bracket the melee ones, and the two that call
something down from outside the Paladin's own reach sit at the top:

| Skill | Level | Mana | Mechanics |
|---|---|---|---|
| Zeal | 6 | 2 / hit | yes |
| Shield Bash | 8 | 3 | — |
| Hammer of Faith | 10 | 5 | — |
| Charge | 12 | 10 | yes |
| Blessed Hammer | 16 | 8 | — |
| Blessed Shield | 20 | 10 | — |
| Fist of the Heavens | 24 | 15 | — |

Zeal's 6/2 and Charge's 12/10 are the user's own numbers and are untouched.

## One thing that got better on the way

`CheckSBook` decided whether a Paladin row could be readied by **naming Charge**. With five more rows
that would have been a name-check that silently had to be right five more times. It now tests whether
the row carries a `SpellID` at all — which is the narrower and truer question, is decided in exactly
one place (`BuildSkillsSheetRows`), and makes a future skill readiable by giving it a slot there and
nowhere else. Zeal proves the test is the right one: it *is* implemented and still correctly cannot be
readied, because it applies itself to every swing rather than being cast.

## State

352/354, the standing baseline. `oracool.mpq` repacked (70 files).

Not tested in-game — the user runs the game.
