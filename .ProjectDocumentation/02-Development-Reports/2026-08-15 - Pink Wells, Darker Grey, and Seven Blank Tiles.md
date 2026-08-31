---
date: 2026-08-15
version: 1.6.14
area: Skills UI / wells, speedbook, plate tints
---

# Pink Wells, Darker Grey, and Seven Blank Tiles

User report: *"look into skils. there are bugs there. also make the inactive skill background darker
gray. lmb skill are with yellow background. make it the pink one."* Two requested changes and two
found bugs, one of which is almost certainly the "bugs there".

## The bug: the speedbook drew seven blank tiles

The speedbook - the grid the `s` key and a click on the RMB well open - draws its icons from the
engine's LARGE spell-icon sheet. The Paladin skills have no frames in that sheet; their `SpellITbl`
entries deliberately point at the empty plate (frame 26), a fallback chosen so a forgetful draw site
shows a bare plate rather than another spell's symbol. The speedbook was exactly such a site: all
seven skills appeared as blank yellow squares, selectable but unreadable.

Fixed with `TryDrawSkillSpellIconLarge` - the large plate drawn in the Skills-sheet pink with the
38px strip icon centred on it - asked first by `DrawSpellList`, exactly as every small-icon site
already asks `TryDrawSkillSpellIcon`. If the strip art is ever missing, the plate still draws: a
pink plate beats a blank tile.

## The second bug: two wells, two answers to one question

The RMB well greys a readied spell the player cannot currently cast - empty mana, spell level 0,
town restriction. The LMB well never did: it kept the spell's full colour regardless, because
`DrawWellIcon` skipped the castability dance `DrawSpell` performs. The same checks now run in both,
so the two wells cannot disagree about whether a spell is usable.

## The requested changes

**Pink wells.** A Paladin skill readied on either button drew on the vanilla YELLOW plate - the
default tint `TryDrawSkillSpellIcon` shipped with - while the Skills sheet draws the same icon on
PINK. The default is now Pink, which moves all three well paths at once (LMB well, RMB well, and the
readied-RMB path in `DrawSpell`). The user's colour language holds everywhere now: blue = spells,
yellow = class skills, pink = the Skills sheet's abilities. Class skills readied on a button keep
their yellow - they are drawn through the engine's own sheet and ramp, untouched.

**Darker grey.** The locked-plate grey was `SpellType::Invalid`'s table, which maps each colour ramp
onto PAL16_GRAY at the SAME within-ramp index - pale, and it read as merely faded next to the pink
plates around it. A new `SetSpellTransDarkGrey` maps four shades further down the ramp instead
(saturated at the last opaque shade), so a locked plate reads unmistakably "off". Only the
plate-based sheets use it (Skills, Auras, Barbarian); the Spells sheet's unlearned grey is the
vanilla treatment and stays.

## Checked while hunting, found sound

The Skills sheet's four walks (draw, click, hover, scroll extent) all use the same uniform row
height through the same row builder - the stale-row and height-mismatch families from the store
audit do not live here. `BuildSkillsSheetRows`' ordering (attacks first, then the class skill row,
then Paladin skills sorted by level gate) matches the click and hover paths by construction, since
all three walk the one list.

## State

**367/369** - the usual two. Worth looking at in game: the speedbook now shows the seven skill
icons on pink large plates; a skill on LMB shows its pink backing in the well; locked rows on the
Skills/Auras/Barbarian sheets are visibly darker; and a readied spell greys on the LMB well when
mana runs out, exactly as it always has on the RMB well.
