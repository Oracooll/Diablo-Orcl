# The Twelve-Item Review

**Version:** 1.7.89 → 1.7.93
**Date:** 2026-08-18
**Request:** the twelve-item list raised from play, after the audit.

All twelve are closed.

## Tree rules (3, 4, 5, 6, 7, 9 — v1.7.89)

**Withdrawn to unbuilt**, so both wear the red X and take no points: **Holy Bolt**, which collided
with the engine's own Holy Bolt spell, and **Conversion**, whose Berserk behaviour was wrong.

**An unbuilt row is wholly inert** — no click, no hotkey, no spend corners. This reverses the
morning's "clicking an unbuilt skill assigns Regular/Fist Attack"; item 11 replaces it with a
deliberate route. **A rank of zero refuses too**: the plate was already red for exactly that state,
and a red plate that still answered a click was the inconsistency.

The hotkey path got the *same* gate rather than its own — a key that binds what the mouse refuses is
the divergence the pair exists to prevent.

## The three things that were one thing (8, 10 — v1.7.91)

Offensive and Defensive auras were never two bugs. **An aura has no SpellID** — it is a toggle, not
a cast — so it can never *be* the readied spell `DrawSpell` looks for. The function had nothing to
find. The well now reports the lit aura ahead of the readied-spell path, which fixes both sheets and
every future aura page at once.

## Chat leftovers (2 — v1.7.91)

`scrollrt` suppresses the *plate* under `talkflag`, but the town portal and RMB skill draw on their
own passes — so they sat on a plate that was no longer there.

## The history window (1 — v1.7.92)

One frame around the window, not one per message; the event log's own rect, mirroring its geometry
rather than inventing a second answer for the same column. An empty history draws nothing rather
than an empty framed box.

## Icons that fit (12 — v1.7.92)

`TryDrawSkillSpellIcon` was handed an origin sized from the engine's **small spell icon**, but since
1.7.82 the art it draws is a class-tree or Paladin strip cell — neither of which is that size. It
takes the **well** now and centres what it actually draws.

## The quick list (11 — v1.7.93)

Clicking either well pops a horizontal strip carrying the two basic attacks and nothing else. It is
also what fills the hole 1.7.89 opened: with unbuilt cells no longer handing back a plain swing,
there was otherwise no way to get one back onto a button.

Both entries select the same state — `SpellID::Invalid` with `SpellType::Invalid` *is* the basic
attack — and which face the well then shows is decided by what is in hand. They are a pair because
that is what the player sees on the two wells, not because they are two choices.

None of the old speedbook was revived: `DrawSpellList`, `CheckSpellList` and `GetSpellListItems`
stay unreachable behind `spselflag`. The list is two fixed entries that are not spells and could not
be expressed in a spell-mask walk, so it is its own small popup.

## Process

Two failures worth recording. Splices into `spell_book.cpp` ate a closing brace and a guard line —
caught by the compiler, nothing shipped. And **v1.7.89 was committed while the suite read 3 of 447
without checking which third one it was**; it was `CastableSkillsInvestThroughTheSpellLevelSeam`,
which used Holy Bolt as its witness. Fixed in v1.7.90 by retargeting it at Zeal, a better witness
anyway since Zeal reaches its slot through the borrowed path.

Suite 445 of 447 at every version, the two standing baseline failures.
