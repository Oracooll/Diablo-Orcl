---
date: 2026-08-15
version: 1.5.60
area: Missiles / Abilities window input
---

# A Bolt Off the Edge of Town

Two reports, unrelated to each other.

## 1. `assert(InDungeonBounds(tile))`, missiles.cpp:721

> i was casting random spells in town and after casting mana shield game crashed

Mana Shield is a red herring — it spawns nothing that moves. What crashed was a **lightning bolt**
still travelling from an earlier cast. `SpawnLightning` walks the bolt tile by tile through
`MoveMissile` and reads `dPiece[tile.x][tile.y]` for each one. Aim at the edge of the map and the
bolt walks straight off the array.

Town is where this is easy to do. A dungeon keeps the player well inside its own walls; town lets you
stand near the boundary and fire outward.

What makes this a clear-cut fix rather than a judgement call is that **the file already had an
opinion about this and `SpawnLightning` was the one place that ignored it.** Every other `checkTile`
in missiles.cpp treats leaving the dungeon as a hard stop:

```cpp
if (!InDungeonBounds(target)) {
    return false;
}
```

`SpawnLightning` asserted instead. So it now stops, setting `_mirange = 0` the same way it already
did for a tile that blocks missiles — the bolt dies at the map edge, which is what it looks like it
should do anyway.

### The second read, which was the worse one

There was a follow-up bug two lines further down, and it would have survived the obvious fix:

```cpp
auto position = missile.position.tile;
int pn = dPiece[position.x][position.y];          // <- unguarded
if (!TileHasAny(pn, TileProperties::BlockMissile)) {
    if (position != Point { missile.var1, missile.var2 } && InDungeonBounds(position)) {
```

`MoveMissile` parks the missile **on** the tile that failed the check, so `position` here can be the
out-of-bounds tile the lambda just rejected. The `InDungeonBounds` test was there — one line too
late to protect the read above it. In a Debug build the assert fired first and hid this; in Release
it is a plain out-of-bounds read with no assert to catch it. The bounds test now short-circuits the
`dPiece` access instead of trailing it.

## 2. The left button could not be assigned

> i cant place a skill/spell on LMB. maybe we make it as whichever mouse button i click with on a
> skill/spell that's where the skill/spell lands. ok?

Yes — and this is better than what was there.

1.5.58 assigned the left button with a **shift-click** in the Abilities window. I chose the modifier
over the obvious gesture on the reasoning that right-clicking inside a panel is already how this game
closes panels, so the right button wasn't free to mean anything else.

That reasoning was wrong about this specific window. `RightMouseDown`'s entire response to a click
inside the Abilities window was:

```cpp
if (sbookflag && GetSpellBookPanelRect().contains(MousePosition))
    return;
```

It does not close the window. It does nothing at all. The right button was free the whole time, and I
spent a modifier that nobody would find to avoid a conflict that did not exist.

So `CheckSBook` now takes which button clicked it, and the row lands on that button. Left-click →
left button, right-click → right button. Regular Attack clears whichever button clicked it, so both
have a way back to swinging. Touch keeps tapping to the left button — a touchscreen has no second
one to offer.

The shift-click is gone rather than kept as an alias. Two gestures for one action, one of them
undiscoverable, is how you end up explaining your own UI in a changelog.

## State

352/354, the standing baseline (`Drlg_l1.CreateL5Dungeon_diablo_3_844660068` and
`Timedemo.WarriorLevel1to2`, both failing before this change).

Not tested in-game — the user runs the game.
