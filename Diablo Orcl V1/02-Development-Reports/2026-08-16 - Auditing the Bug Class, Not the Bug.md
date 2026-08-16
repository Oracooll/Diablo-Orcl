---
date: 2026-08-16
version: 1.7.22
area: Audit - unguarded sprite-optional derefs, and the inert-row rule made exhaustive
---

# Auditing the Bug Class, Not the Bug

Yesterday's crash on death was fixed at its cause. But it was the **second** crash of exactly this
shape — v1.6.24 had one, on the Shield Bash block sheet — and both times the fix removed one *path*
into an unguarded dereference rather than the dereference itself. Two of a kind is a class, so this
pass went looking for the rest of it instead of starting Phase 3.3 on top.

## The sweep

Twelve places in the tree dereference a sprite optional. Six were already guarded. **Six were not**,
and four of those are reachable in ordinary play.

### The one that had already crashed twice

`PlayerAnimationData::spritesForDirection` did this:

```cpp
return (*sprites)[static_cast<size_t>(direction)];
```

Its exact twin for monsters — `AnimStruct::spritesForDirection`, twenty lines of header away —
returns `std::nullopt` instead. Two parallel functions written to different standards, and the
player's was the wrong one, because `LoadPlrGFX` declines four graphics outright: Attack and Hit in
town, Block without the block flag, and (until v1.7.21) Death with a weapon held. An unloaded
animation is a reachable state, not a programming error.

This is the deref behind the v1.6.24 crash. That fix stopped Shield Bash from *selecting* the block
graphic; the deref stayed, and every caller that checked for an empty result was checking downstream
of an assert that had already fired. It now returns an optional like its twin, and its four callers
handle it.

### The others

- **`Corpse::spritesForDirection`** — same shape, same fix. A corpse with no graphics is reachable
  between a level being torn down and the next one's art loading, and `dCorpse` still names it in
  that window.
- **`DrawItem`** — `item.AnimInfo.currentSprite()`, unguarded. This fork ships ~73 new base items
  across three cursor sheets, so "the drop animation is always loaded" is a bigger assumption than
  it used to be.
- **`DrawMissilePrivate`** — `(*missile._miAnimData)[...]`, unguarded, even though
  `MissileData::spritesForDirection` correctly returns nullopt when the graphic is absent.
- **`InitMissileAnimationFromMonster`** — the one caller that took the monster helper's careful
  `OptionalClxSpriteList` and threw the guard away with a bare `*`.

Every one of them now skips the frame. **A missing sprite should cost a drawn entity, never the
session.**

## The inert-row rule was pinned by a list of five

`class_tree.h` makes a standing promise: a row the engine has no channel for is listed, described,
and contributes **nothing**, so a player is never told a point bought something it did not.

That promise was pinned by a test naming five Paladin auras. There are 161 skills across six
classes, and most of the inert ones were on no list at all — including every inert row the Bard and
the Monk added, which is the majority of both.

It is exhaustive now: walk the whole table, and for every row declaring `implemented == false`,
invest a point, light it if it is an aura, and assert the bonus totals are byte-identical to
nothing. **All 90-odd of them pass**, which is the good outcome — the rule was being honoured, it
just was not being checked. Now it cannot rot as rows are added, and it would catch the obvious
mistake of giving a row an effect while leaving its flag false.

The test also refuses to pass if fewer than fifty rows come back inert, so inverting the flag
somewhere cannot quietly turn the whole check into a no-op.

## What the audit did not find

Worth recording, because a clean result is information too:

- The oracool module caches (`hero_preview`'s sheet, `ui_backgrounds`' prepared sprites) guard every
  deref already.
- `MissileData::spritesForDirection` and `AnimStruct::spritesForDirection` were already correct.
- `UseItemDropAnimation` already declines gracefully, with a comment saying why.
- The `Monsters[MaxMonsters]` bound used by yesterday's `ClearMonsterScaleCache` unbind loop is
  right — `MaxMonsters` is the array's own extent.

## Verified

**413 tests, the same two pre-existing failures.** One new test, and it is the exhaustive one.
