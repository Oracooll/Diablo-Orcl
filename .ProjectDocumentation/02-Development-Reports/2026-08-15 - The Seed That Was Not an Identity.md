---
date: 2026-08-15
version: 1.6.4
area: Monsters / lesser uniques
---

# The Seed That Was Not an Identity

> "i saw one of the lesser uniqes. his name was constantly changing."

## What I got wrong

Two builds ago I wrote this, and was pleased with it:

> Derived from the monster's `aiSeed`, which is already per-monster and already saved — so the name
> survives a save and reload without costing a field or another look at the save format.

Both halves of that are true. Both are beside the point. `multi.cpp`'s `MonsterSeeds()`:

```cpp
sgdwGameLoops++;
const uint32_t seed = (sgdwGameLoops >> 8) | (sgdwGameLoops << 24);
for (size_t i = 0; i < MaxMonsters; i++)
    Monsters[i].aiSeed = seed + i;
```

Every monster. Every tick. **Single-player included** — `game_loop` calls `multi_handle_delta()`
unconditionally, because the loopback network layer runs in SP too.

`aiSeed` is a **per-tick nonce that happens to be stored per monster**. I read "per-monster" off the
declaration and "already saved" off `loadsave.cpp`, and never asked the third question — *does it hold
still?* Storage location and save coverage say nothing about lifetime.

## The fix

A real field: `Monster::lesserNameSeed`, rolled once at placement and saved.

**Still no save-format change.** The same "Unused" run that already holds `lesserAffix` had three
bytes left; this takes two, leaving one. A save written before it reads zero, which gives an old
champion the first name in the book — acceptable, since they are level furniture and a fresh floor
re-rolls them.

**One roll, not three.** The seed is a mixed-radix number — `given + 50*epithet + 1300*tint`, range
6,500 — so name and colour are independent digits of one value rather than three fields that would not
fit in two bytes. The range also matters for a second reason: `GenerateRnd` corrects for LCG bias only
below `0x7FFF`, and above it hands back raw low bits — exactly the bits `% 50` would then read. A
`static_assert` pins that.

## The second bug, found while fixing the first

The tint did not survive a save/load either, and for a related reason.

`SyncMonsterAnim` — which runs on load and on every level entry — calls `InitTRNForUniqueMonster`,
which **reloads the champion's palette from its `.trn` file**. The tint lives only in that in-memory
buffer, so every reload silently reverted a champion to its authored colour.

The recolour is now re-applied right after the reload. Safe to run every time *precisely because* the
reload just happened: `TintLesserUnique` shifts, so it must only ever see a fresh palette, and both
call sites are immediately after one. That constraint is now written on the function.

Same root cause as the name, in a different disguise: **state derived from something whose lifetime
nobody had checked.**

## State

**354/356** — the usual two. `Loadsave` passes, which round-trips the monster block, so the two-byte
change is symmetric on both sides.

Not tested in-game — the user runs it. Worth watching: a champion's name should hold still while you
fight it, survive a save and reload, and still be there after a walk up the stairs and back — that
last one is what the tint fix bought, and it is the case that proves it.
