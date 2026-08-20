# Plan: the skill picker, and the points-only Abilities window

**Status:** proposed, 2026-08-20. Against v1.8.77.

## The change in one line

**Selecting** what sits on a mouse button moves out of the Abilities window into a popup that opens
by left-clicking that button's HUD well. The Abilities window keeps only **management**: left click
spends a point, right click takes one back.

## Why this is the right cut

Every sizing problem of the last hour - 93 icons, 50 cells, shrink them, page them, scroll them -
came from one window doing two jobs with opposite requirements. Managing points wants space, text,
tier structure and descriptions. Picking wants density and speed. Split them and both get easy.

Splitting also **dissolves** the capacity problem rather than solving it. The picker needs only what
you can cast right now - invested tree skills plus known spells - not the 163-row table with its
unlearned tiers. Mid-game that is roughly 20-40 entries, not 93.

---

## Part A - the picker popup

### Opening and closing

- **Left click on the LMB well** opens the LMB picker. **Left click on the RMB well** opens the RMB
  picker. Two separate popups with the same catalogue; each binds only its own button.
- **RMB must keep casting.** The picker opens from clicking the WELL on the HUD, never from
  right-clicking the world. This is the one hard constraint in Part A.
- Clicking an entry binds it and closes the popup. Two clicks to act, satisfying the standing
  front-end rule, and the same gesture mirrored for both buttons - which is the whole point.
- Red X, Escape, and clicking outside all close it. It joins `CloseAllWindows`, and it joins
  `oracool::IsPointOverFloatingWindow` (`hud_layout.cpp`) so it cannot be hovered through - the bug
  fixed on Levski's panel in v1.8.76, which at this size would be far worse.
- Opening one closes the other, and closes the Abilities window. One picker at a time.

### Contents

Everything readiable, in two groups, colour-coded as in the concept:

1. **Skills** (green) - tree rows that are `implemented`, unlocked, and have investment > 0, plus
   Regular Attack and Fist Attack. Passives are excluded: a passive cannot be readied, and listing
   it would invite a click that does nothing.
2. **Spells** (blue) - `IsSpellKnown`, plus staff-charge spells (`_pISpells`).

Both lists already exist: `BuildSpellRows` and the tree page walk in `panels/spell_book.cpp`. The
picker should call into shared builders rather than a third copy of either.

### Geometry

38x38 icons - not negotiable downward, because the RMB well alternates between this icon and the
engine's readied-spell icon at 37x38, and they must match or the slot appears to resize.

- **5 columns**, 6px gaps: `5*38 + 4*6` = **214** wide.
- **Rows grow to content**, capped at **10**: `10*38 + 9*6` = **434** tall. Beyond that it scrolls -
  reuse the waypoint panel's pixel scrolling and themed scrollbar.
- Anchored so its **bottom sits just above the middle HUD plate** and its outer edge lines up with
  its own well: LMB picker to the left, RMB picker mirrored to the right.

At full height that is 214x434 per side, well under the concept mock's ~320x455, and it leaves the
centre of the screen clear. **Size to content**: at level 3 the popup is three icons tall, not a
grid of empty cells.

### Chrome

Per the standing new-window checklist, and all four absent from the concept: a dark backing so the
world does not read through the icons, `DrawOrnateBorder` around it, a red X, and no click-through.
The hover description popup (task #84) comes along - picking blind from a grid of icons is worse
than the list it replaces.

---

## Part B - the Abilities window becomes points-only

### The click rebind

| | Now | After |
|---|---|---|
| Left click on icon | ready it on the clicked button | **invest one point** |
| Right click on icon | ready it on the right button | **refund one point** |
| Left click bottom-right corner | invest | (gone) |
| Left click bottom-left corner | refund | (gone) |

**The spend corners disappear entirely, and so do the `+`/`-` glyphs.** They only ever existed
because the icon itself meant "ready this" - `SpendPlusRect` / `SpendMinusRect` were carved out of
the icon precisely so the two actions could not be confused. Remove readying and the whole icon
becomes the spend target, which is what makes this simplification possible rather than merely
tidy.

Touch points in `Source/panels/spell_book.cpp`: `SpendPlusRect` / `SpendMinusRect` /
`DrawSpendGlyph` (declared ~751-838, drawn 820-822 and 1025-1027) delete; the tree click handler
(~1544-1560) and the spells click handler (~1644-1650) rewrite; the ready-on-click tails (~1600-1628
for the tree, ~1655+ for spells) delete.

### Eligibility by brightness

The `+` glyph currently appears only when the character holds a point the cell can take. That
signal survives, as brightness rather than a glyph: **cells eligible for a point draw brighter than
the rest.** `CanInvestClassTreePoint` already answers exactly this question and is already called at
line 1024 to decide whether to draw the glyph - the same call now picks the draw path.

The skill level indicator stays where it is.

### What does not change

Refund is already free, unlimited, and unconfirmed by explicit design (user, 2026-08-17: "We want
players to be able to redistribute skill points at will"). Right-click-to-subtract is a **rebind of
an existing mechanic**, not a new one - `RefundClassTreePoint` and `RefundSkillPoint` both exist and
are tested.

---

## Part C - spells take levels from books only

### What is actually there

Three separate stores already feed `Player::GetSpellLevel` (`player.h:748`):

| Store | Filled by |
|---|---|
| `_pSplLvl[64]` | **books** - vanilla D1 |
| `_pSkillInvestment[MAX_SPELLS]` | **skill points**, keyed by SpellID |
| `_pISplLvlAdd` | item bonuses |

So the rule needs no new field and no save break. It needs one door closed.

### The door to close

`_pSkillInvestment` has **two** writers:

1. The **Spells sheet**, via `InvestSkillPoint` / `RefundSkillPoint` (spell_book.cpp:1644-1648).
   **This is the one the rule deletes.**
2. The **class tree**, for rows that carry a `SpellID` - `InvestClassTreePoint` writes here so that
   investment flows into `GetSpellLevel` and every ladder that already scales with spell level.

### The collision, and why it must be settled first

Keeping writer 2 is not optional. The file comment on `class_tree.h` is explicit that this seam is
**the entire reason the Sorceress page is mostly a wiring job**: Fire Bolt, Fireball, Fire Wall,
Inferno, Lightning, Chain Lightning, Nova, Charged Bolt, Teleport, Telekinesis, Mana Shield and
Guardian are engine spells, and tree investment is what raises them. Cutting `_pSkillInvestment` out
of `GetSpellLevel` outright would leave the whole Sorceress tree inert, and take most Paladin
actives with it.

But a Sorcerer's Fireball is then *both* a tree skill raised by points and a book spell - so the
rule "spells are only affected by books" is true of the Spells sheet and false of her tree.

**Recommended scoping:** the rule governs the **Spells sheet**, not the array.

- A spell can no longer be pumped from the spell list. Books and items only.
- A tree skill is a skill, and takes points, even where it is implemented as a `SpellID`.
- One line states it: *points are spent in the tree; books raise spells; where a tree skill happens
  to be a spell, the tree is still where its points go.*

This preserves the Sorceress, honours the intent, and costs one deletion.

### Migration

A character who invested from the Spells sheet into a spell that is **not** a row in their own
class's tree keeps points in a place they can no longer reach. On load, refund those to the unspent
pool - `ClassTreeSkillForSpell(class, spell)` returning `None` is the exact test. One-time, in the
chunk load path, guarded so it cannot run twice.

---

## Order of work

1. **Part C** first, and alone. It is a rules change with a migration; it should land and be tested
   before any UI moves on top of it.
2. **Part B** next. It is contained in one file and leaves the game playable - selection still works
   from the picker's absence because the wells still open the Abilities window until step 3.
3. **Part A** last, repointing the wells from the Abilities window to the picker.

Steps 2 and 3 must not be split across a release: between them, nothing readies a skill.

## Verification

- Build Debug, full ctest. Baseline is **487/489**
  (`Drlg_l1.CreateL5Dungeon_diablo_3_844660068`, `Timedemo.WarriorLevel1to2`).
- `Writehero.pfile_write_hero` must not move - none of this changes the save format, and Part C's
  migration writes only through the existing chunk.
- New tests worth pinning: the Spells sheet refuses investment; a tree spell still raises
  `GetSpellLevel`; the migration refunds exactly the orphaned points and is idempotent.
- Parts A and B are drawn over the world, so **a screenshot is the only real verification** - the
  user runs the game.

## Open decision

**Part C's scope** is the one thing that changes what gets built. The recommendation above
(rule governs the Spells sheet; the tree keeps its channel) is what the rest of this plan assumes.
The alternative - no `SpellID` takes points at all - is a much larger job: it would mean redesigning
the Sorceress tree around non-spell mechanics, and should not be started as part of this work.
