---
date: 2026-08-20
version: 1.8.64
tags: [audit, objects, town, wiki]
---

# Reaudit, and the Flag That Was Really a Range

A re-audit of v1.8.61–1.8.63 plus the wiki, and the one real defect it turned up — which the
user had already found by walking into it.

## The defect: `_oSolidFlag` is an activation-range switch

The user's report: *"you need to put a 1 tile range for Levski activation because now my hero
walks right over/under it to activate it."*

`msg.cpp:1532` is where this lives:

```cpp
MakePlrPath(player, position, !object->_oSolidFlag && !object->_oDoorFlag);
```

The third argument is `endsAtTarget`. A **non-solid** object is walked *onto*; a **solid** one is
approached and stopped beside. So the flag is not really about collision at all from the player's
point of view — it is the object's activation range, one tile versus zero.

`AddLevskiRoarObject` had been clearing it since 2026-08-19, overriding `OBJ_STAND`'s own table row
(`Solid | MissilesPassThrough | Light`). The stated reason was insurance against "outline, but
clicking does nothing": a solid object needs a walkable neighbour, and without one the click sends
its command and nothing fires.

**That insurance was for a bug that was already fixed.** The unreachable monument of 1.8.17 was
`selFlag 0`, not solidity. Clearing the flag was belt-and-braces against a failure that no longer
existed, and it carried a cost that stayed invisible for exactly as long as the monument was a
knee-high rock stand. Give it a statue and a plaza, and the hero walks through the middle of it.

Fixed by **deleting the override** rather than setting it back — the table row is already correct,
and a line that restates a default is a line that can drift from it.

The insurance is genuinely unnecessary: the monument needs one walkable neighbour out of eight, the
placement loop already logs loudly when it falls back to another tile, and
`IsPlayerAdjacentToObject` is a plain Chebyshev test with no further requirement.

**The general lesson**, which is the same shape as this week's others: the flag's *name* describes
collision, and its *effect* at the only call site that matters is range. Reading the name and not
the call site is what put the override there.

## Audit findings, code

Everything else was clean. Checked and confirmed:

- **`object_graphic_id` and `ObjMasterLoadList` agree** — 68 entries each. A mismatch here is
  silent: the enum indexes the list, so an extra enumerator reads off the end.
- **`NumObjectGraphicFiles`** feeds three `filesWidths[]` arrays; all three are declared from the
  constant rather than a literal, so they grew with it.
- **`_oAnimFrame` is 1** for `OBJ_STAND` — non-animated objects take `animDelay` as their frame,
  which is 1 in that row. The monument's CEL has exactly one frame, so nothing indexes past it.
- **Object picking is tile-based**, via `FindObjectAtPosition` and `_oSelFlag`, not sprite bounds —
  so halving the sprite to 96px did not shrink the click target.
- **`_oAnimWidth` in the save**: written and read back, and `ApplyLevskiRoarGraphics` overwrites it
  on every `SyncObjectAnim`. A save written while the constant was 192 loads correctly.
- **No dangling references** to the two deleted chest scripts, beyond prose.

Two documentation defects, both fixed:

- `SyncObjectAnim` carried **two contradictory comment lines** about the parked chest call — one
  saying it was live, one saying it was parked. Residue from parking and unparking it in the same
  day.
- `build_levski_roar_cel.cmd` pointed at `tools/CutChestStates.ps1` for the filename-encoding
  story. That script is deleted; the note now tells the story without the dead pointer.

## Audit findings, wiki

The **bundle-vs-pages drift check passes** — 99 source files, fingerprint `6dd4805b63e2`. That
check is working exactly as built.

It is also answering a different question than the one that matters now. It verifies the bundle
matches the pages. Nothing verifies the **pages match the code**, and they do not:

| | State |
|---|---|
| `data.js` | generated at **1.8.40**, 2026-08-19 20:58 — code is at 1.8.64 |
| `history.html` | last row is **1.8.17** — 47 versions missing |
| `ui.html` | says Levski's Roar has *"Placeholder art"* — false since 1.8.62 |
| `controls.html` | no **W** / runeword book — the window shipped in 1.8.57 |
| `debug.html` | missing the socket/rune/gem/charm/ethereal spawners |
| `monsters.html` | no Runt or Giant sizes |
| `assets.html` | `MonumentCel.cs` absent |

`mechanics.html` **does** carry the resistance soft cap, correctly and data-driven.

This is a content pass, not a defect, and it is its own unit — deliberately not folded in here.

## Verification

Build clean (the only warnings are the pre-existing `C4267` set in `multi.cpp`). 470 tests, the two
standing baseline failures only.

The solidity change cannot be tested from the suite — no test walks a player up to a town object.
**It needs a click in town**: the hero should stop one tile short of the monument and operate it
from there, rather than stepping onto its tile.
