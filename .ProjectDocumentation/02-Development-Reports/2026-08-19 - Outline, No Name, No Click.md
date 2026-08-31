---
date: 2026-08-19
version: 1.8.19
area: Levski's Roar - the two reasons a selectable object still did nothing
---

# Outline, No Name, No Click

User play-test of 1.8.18: "when i hover on the item - no pop up. there is outline, though. nothing
happens when i click on it."

The outline is the useful half of that report. It proves `_oSelFlag` works and the object is being
found under the cursor - so both remaining failures are **downstream of selection**, and they turn
out to be two unrelated causes rather than one.

## No popup - certain

`Object::name()` is a switch ending in `default:` that returns nothing, and OBJ_STAND was never a
case in it. Hovering found the object; the object had nothing to say.

Now returns **"Levski's Roar"** when `currlevel == 0` and "Rock Stand" elsewhere - the Caves keep
the Anvil of Fury's own furniture sensibly named, the same currlevel split the operate path already
uses.

## No click - diagnosed, not yet confirmed

OBJ_STAND is flagged `Solid` in objdat.cpp. The chain that matters:

1. `CMD_OPOBJXY` reaches `OnObjectTileAction`, which calls
   `MakePlrPath(player, position, !object->_oSolidFlag && !object->_oDoorFlag)`.
2. For a solid object that third argument is **false**, so the path targets an ADJACENT tile rather
   than the object's own.
3. `ACTION_OPERATE` then gates on `IsPlayerAdjacentToObject` before calling OperateObject.

If no neighbouring tile of {55,66} is walkable, the command is sent, the player never moves, and
the operate never fires - which is precisely "outline, but nothing happens".

`_oSolidFlag` is cleared on the placed monument so the path lands ON its tile, where adjacency is
trivially satisfied. This is the same shape as the barrel, which clears its own solid flag after
AddObject. Town furniture blocking movement was never the point, and clearing it removes the
reachability question entirely instead of betting that {55,66} has a free neighbour.

**This one is a diagnosis, not a verified fix.** If it still does not open, the discriminator is
whether the character walks toward the stand at all: walking and not opening means adjacency is
still the problem; not walking at all means the command is not being sent and the fault is further
up, in the town click branch.

## The pattern across three builds

Three separate defects, none reachable by the suite, all in the same feature:

| Build | Defect | Why no test saw it |
|---|---|---|
| 1.8.17 | `selFlag = 0`, object unclickable | No test clicks a town object |
| 1.8.19 | no name, no hover popup | No test reads a hover string |
| 1.8.19 | `Solid` blocks the operate path | No test walks a player to an object |

Every one is a property the object INHERITS from a vanilla table written for where the type
normally lives. The lesson is narrower than "add tests": placing a vanilla object type somewhere
new means auditing every field of its data row against the new use, not just the one that broke
last. That is now the note attached to the Pipeline's object-harness entry.

## Verified

**454 tests, the usual two.** Neither fix is covered by them, which is the finding above.
