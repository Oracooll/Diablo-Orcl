# The skill picker

**Version:** 1.8.80
**Date:** 2026-08-20
**Part A** of [[Plan - The Skill Picker and the Points-Only Abilities Window]] - the last of the three.

> "Selecting lmb/rmb skills/spell happens from these pop-ups when i click lmb/rmb... Basically we
> only navigate using the left mouse button. Easier to remember and to muscle memorize."

## The gesture

**Left click the LMB well** -> the LMB picker. **Left click the RMB well** -> the RMB picker. Click
an entry, it binds and closes. Two clicks to act, the same motion mirrored for each button.

**Right click still casts.** The picker is reached from the well on the HUD, never from
right-clicking the world. That was the one hard constraint and it is structural: nothing in this
module is wired to a right-click.

## It supersedes the quick list

`OpenAttackQuickList` was already this window with only two entries in it - a strip floating above
its own well, opened per-button, absorbing clicks. Part A is that idea with everything in it, so all
five call sites were repointed rather than a sixth popup added. The two basic attacks are simply the
first two entries now.

## What is in it, and why it is small

Two sections:

- **Skills** - the two basic attacks, then class-tree rows that are `implemented`, unlocked, have a
  rank in them, and are not passives. Passives are excluded because a passive cannot be readied and
  an entry that does nothing when clicked is worse than an absent one.
- **Spells** - everything `_pMemSpells | _pISpells | _pAblSpells` says the character has,
  deduplicated against the skills above so a Paladin's Zeal is not offered twice.

This filter is the whole reason the window is small. It lists what you can cast **right now**, not
the 163-row table with its unlearned tiers - which is what made the earlier "93 icons into 50 cells"
problem disappear rather than get solved.

## Geometry

- **38x38** icons. Not a free choice: the RMB well alternates between a strip icon and the engine's
  readied-spell icon at 37x38, and cells that disagreed with the well would make the slot appear to
  resize on selection.
- **5 columns**, 6px gaps -> 214px wide. Rows grow to content.
- Anchored above the middle HUD plate, over its own well, clamped to the screen.
- **Sized to content**: a level-3 character sees three icons, not a grid of empty cells. The concept
  mock's two ~320x455 blocks were sized for 93 entries; this needs a fraction of that.
- Overflow scrolls on the wheel, clamped at both ends - without the upper bound the wheel would push
  the list past its own end into an empty window, which looks like the picker losing its contents.

## Chrome - the four things the concept was missing

Opaque fill (the mock drew icons straight onto the dungeon and the world read through them as
noise), `DrawOrnateBorder`, a red X via the shared `window_close` helper, and **no click-through**:
any click inside the rect is absorbed even when it lands on no cell. It joins `CloseAllWindows`, the
Escape chain, and `IsPointOverFloatingWindow` - the hover-through fix from v1.8.76, which at this
size would have been the most obvious instance of that bug yet.

A click **outside** closes the picker and passes through, deliberately: a popup you must dismiss
before acting costs two clicks to cancel, which is the friction this window exists to remove.

## Aura handling

An aura is a toggle, not a binding - either button lights it, clicking a burning one puts it out,
and it never displaces what is on the left button. That rule moved here intact from the Abilities
window, which lost it in Part B.

## Verification

Build clean. **489/491**, the two standing baseline failures. No asset touched, so no MPQ repack.

This is drawn over the world, so **a screenshot is the only real verification** - the user runs it.

## To test

1. Left click the LMB well, then the RMB well - each opens its own picker over its own button.
2. Pick something: it binds to that button and the window closes.
3. **Right click the world - it must still cast.** If it opens a picker, that is the one thing here
   that would be badly wrong.
4. Escape, the red X, and a click outside all close it; a click on the frame does nothing and does
   NOT reach the world.
5. Sweep the mouse over it - nothing behind it should highlight.
6. An aura entry toggles rather than binds, and shows on the RMB well.
7. Abilities window: left click invests, right click refunds, eligible cells outlined bright, and
   nothing in it readies anything any more.
