# The Net Area of a Well

**Version:** 1.7.94
**Date:** 2026-08-18
**Tests:** 445/447 (the two standing baseline failures)

Five items, raised from play. One of them turned out to be the cause of two others.

## 1 — Message History fills downward

`DrawPlrMsg` stacked from the bottom of its window upward, so the newest line sat at the floor and
the reader's eye had to start at the wrong end. It now starts at the window's top pad, walks down,
and stops the moment the next block would cross the bottom pad — the newest message is the first
one you read, and older ones simply do not fit rather than being clipped in half.

## 2 and 3 — "I can't assign Regular Attack to RMB" / "combat skills don't assign to RMB"

Both were one bug, and it was not in the assignment.

v1.7.91 gave a lit aura the RMB well: `DrawSpell` drew the aura and **returned**. So with any aura
burning, whatever you readied afterwards was written to `_pRSpell` correctly and then never shown.
Every assignment path — the Abilities sheets, the basic-attack quick list, the hotkeys — was working
the whole time against a well that refused to report it.

The aura no longer owns the slot. It gets a 14px badge in the well's top-left corner carrying its
initial, drawn by a new `DrawRmbAuraBadge` called right after `DrawSpell` so it survives that
function's early returns, including the no-spell-readied one. An aura has no SpellID and can never be
the readied spell, so it has no claim on the slot's main face; what it needs is to be visible.

The top-right corner was left alone deliberately — that is the F-key badge's corner on the Abilities
window, and the two conventions should not collide.

## 4 and 5 — the 46×46 hard boundary

`hud_layout` now exports the wells' **net opening**: `SkillWellNetSize {46,46}`, with
`GetLmbSkillWellNetRect()` / `GetRmbSkillWellNetRect()` centring it in each button rect. Every well
draw goes through those instead of an icon-sized origin.

That alone was not enough. The class-tree strips are cut at **56px** and the Paladin strip at
**38px**, so centring either in a 46px hole gave five pixels of painting over the bezel or a moat of
bare plate. Both now scale to the net rect:

- `BlitStripCellScaled` — nearest-neighbour blit of one square strip cell into any rect, keying on
  index 0 exactly as `DrawStripIcon` does. Nearest-neighbour because the source is already palette-
  quantized: there are no in-between colours to interpolate toward, and a blend would have to
  re-quantize per pixel per frame.
- `DrawSmallSpellIconFittedTo` — the containment twin of `DrawSmallSpellIconScaledTo`. That one
  rounds the scale UP so the plate always covers its cell, which is right on a tree page where the
  overhang lands on more page. It is wrong here, where the overhanging pixel lands on the bezel. The
  new one rounds DOWN and centres the remainder: at most one pixel short on an axis, never one over.

Scaled rather than clipped, because clipping a 56px cell to 46 shaves off the icon's own painted
border — the part that makes it read as an icon at all.

## Files

- `Source/plrmsg.cpp` — top-down fill
- `Source/panels/spell_list.cpp`, `spell_list.hpp`, `Source/control.h`,
  `Source/engine/render/scrollrt.cpp` — the aura badge
- `Source/oracool/hud_layout.h` / `.cpp` — the net rects
- `Source/oracool/hud_art.cpp` — `BlitStripCellScaled`, `DrawStripIconScaledTo`,
  `DrawClassTreeIconScaledTo`, and `TryDrawSkillSpellIcon` rewritten to fill rather than centre
- `Source/panels/spell_icons.cpp` / `.hpp` — `DrawSmallSpellIconFittedTo`
- `Source/oracool/attack_skills.cpp` — well draws repointed at the net rect

## To look at in game

1. Message History — newest line at the top.
2. Light an aura, then ready a combat skill on RMB. The skill shows; the aura is a lettered badge in
   the well's top-left.
3. Click the RMB well, pick Regular/Fist Attack from the strip. The well returns to the attack icon.
4. Any readied skill or spell: plate and icon both inside the bezel, nothing touching it.
