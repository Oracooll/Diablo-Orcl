# Gameplay and mechanics audit: v1.9.122

**Audited commit:** 2c3e8d15d0fdc475562024bfc9b2feee9eb8852a  
**Code commit:** f760a9c91f6bad046435388b5685f2860b565ae8  
**Date:** 2026-08-30

## GP-01 — High — A new hero spawns inside the solid Stash Chest

### Observation

The v1.9.115 play-test change moved the town Stash Chest from {55,67} to {56,67}. That destination
was already the local player's new-game spawn tile. The code now places two solid/occupying entities
on the same world tile every time a local new game starts.

### Evidence

- Source/objects.cpp:2050-2058 documents and declares StashChestPosition {56,67}.
- Source/objects.cpp:4232-4238 creates an ordinary OBJ_CHEST3 at StashChestPosition.
- Source/objdat.cpp:181 declares OBJ_CHEST3 with the Solid flag.
- Source/objects.cpp:876 copies that data flag to Object::_oSolidFlag.
- Source/multi.cpp:369-382 sets the first local spawn to {56,67} and assigns both
  player.position.tile and player.position.future to it.
- Source/multi.cpp:375-376 still says the player is “next to” the old chest at {55,67}, which hid
  the cross-file invariant break during review.
- Source/levels/town.cpp:363-366 also sets the new-game ViewPosition to {56,67}, explicitly because
  that is the local player's tile.

LoadGameLevel initializes the player and later adds the town furniture. Separate dPlayer and
dObject grids allow both records to exist, so placement does not reject the collision.

### Deterministic simulation

1. Create any new local hero.
2. Start a new game in town.
3. SetupLocalPositions assigns the hero tile {56,67}.
4. AddStashChestObject adds a solid OBJ_CHEST3 at {56,67}.
5. The hero begins inside the solid chest. Interaction/path behavior is abnormal until the player
   manages to move out; the chest cannot be approached normally while both share its tile.

This does not depend on class, difficulty, RNG, resolution, or saved state.

### Why tests missed it

test/oracool_town_objects_test.cpp:62 correctly pins the chest to {56,67}. Its tests verify:

- the chest exists;
- town furniture does not overlap other town furniture;
- the chest is solid/selectable and is approached rather than walked onto.

No test compares town furniture positions with the local-player spawn set. The placement test was
updated when the chest moved, so it confirmed the new coordinate without checking what else owned
that coordinate.

### Recommended repair

Choose one authoritative pair of positions and keep them adjacent, not equal.

- Restore the local spawn to a known walkable adjacent tile such as the now-vacated {55,67}, after
  confirming that tile against town collision data; or
- move the chest again and leave the spawn/camera at {56,67}.

Also update the stale comment in Source/multi.cpp.

Add an invariant test that builds the local spawn set and all fixed town furniture, then asserts:

- the local spawn is a walkable town tile;
- no solid object uses it;
- every advertised “next to” object is within the intended walking distance;
- the camera begins on the player, if that remains a requirement.

## GP-02 — Medium / decision required — Zeal's actual accuracy contract conflicts with both UI sources

### Observation

The v1.9.116 Zeal nerf has three mutually inconsistent descriptions:

1. Source/oracool/paladin_skills.cpp:75 tells the player: one point adds a strike up to four, then
   +1% chance to hit “thereafter.”
2. Source/oracool/class_tree.cpp:121 still says each invested pair adds a strike, up to five.
3. The implementation grants +1% for every invested point, starting with point one, and adds it in
   the generic melee-hit function used by ordinary attacks and the other melee skills.

At least the displayed behavior is wrong. The global application may also be a balance bug.

### Evidence

- Source/oracool/paladin_melee.cpp:27 caps strikes at four.
- Source/oracool/paladin_melee.cpp:31-34 makes every one point add a strike until the cap and defines
  one percentage point of accuracy per investment.
- Source/oracool/paladin_melee.cpp:225-247 returns:
  - two base strikes at unlock;
  - one extra strike per point, capped at four;
  - the entire Zeal investment as ZealToHitBonus.
- Source/player.cpp:621-645 calls ZealToHitBonus inside PlrHitMonst for every Paladin melee hit. It
  does not ask ArmedMeleeSkill and does not require the armed skill to be Zeal.
- Source/player.cpp:905-947 shows PlrHitMonst is also used for a normal primary swing and class
  cleave hits.
- No test mentions ZealToHitBonus. Existing Zeal tests pin only the strike-count ladder.

### Concrete current behavior

For a level-6-or-higher Paladin:

| Zeal investment | Strikes | Accuracy added by implementation |
|---:|---:|---:|
| 0 | 2 | 0% |
| 1 | 3 | +1% to every melee hit |
| 2 | 4 | +2% to every melee hit |
| 3 | 4 | +3% to every melee hit |
| 10 | 4 | +10% to every melee hit |

The paladin_skills text implies the third row should be the first to receive a post-cap bonus, and
that it is a property of Zeal. The class_tree text describes neither the current strike count nor
the current point cadence.

### Decision and repair options

Resolve these two questions explicitly:

1. Is the accuracy reward paid for every invested point, or only points beyond the two points that
   reach the four-strike cap?
2. Is it a passive Paladin melee bonus, or Zeal's own hit bonus?

If the intended rule is “post-cap and Zeal only,” calculate max(0, investment - 2) and add it only
when the latched melee skill is Zeal. If the current implementation is intended, update both
player-facing descriptions to say every point improves all melee attacks.

Whichever contract is chosen, add tests for:

- investments 0, 1, 2, 3, and a large value;
- normal attack versus armed Zeal;
- Hammer of Faith and Shield Bash;
- level below the Zeal unlock gate;
- the 5..95 final hit-chance clamp.

Remove the duplicated stale “pair/up to five” sentence or derive every Zeal description from one
data source.

## Mechanics checked without a new defect

### Unique-drop narrowing and champion second-drop chance

The v1.9.117 paths are bounded:

- Unique Item Drop Multiplier is clamped to 1..100.
- Unique Drop Chance Percent is clamped before use.
- Reconstruction pins the multiplier to 1 and the narrowing percentage to 100, avoiding live-option
  changes altering an already-saved item.
- Champion Extra Drop Chance is clamped to 0..100 and gates only the second SpawnItem call.

These paths need probability/distribution tests, but no deterministic arithmetic or reconstruction
defect was found in this pass.

### Remembered mouse-button lifecycle

A suspected lifecycle wipe was ruled out. CreatePlayer applies the remembered pair before the new
hero is packed; PackPlayer records both buttons, and UnPackPlayer restores them after InitPlayer's
default reset. The confirmed failure is option-file persistence, documented in the persistence
report, not the new-hero pack/unpack order.
