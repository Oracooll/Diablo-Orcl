# Fire damage had been grey for ten versions

2026-09-12 — v1.11.080

## What was asked

> when my auras do fire/lightning/magic dmg use the coresponding font color for the floating dmg
> texts over mobs heads.

## What was actually wrong

Nothing about auras. Aura damage has always carried its element: `AuraStrike`
(`Source/oracool/aura_field.cpp:74`) takes a `DamageType` and hands it to `ApplyMonsterDamage`,
which hands the same value to `AddFloatingNumber`. Holy Fire passes `Fire`, Holy Shock `Lightning`,
Holy Freeze `Cold`, Sanctuary `Magic`. The plumbing was already right and needed no change.

The **colour table** was wrong, and had been describing elements in two places that disagreed:

| Element | Character sheet said | Floating number drew |
|---|---|---|
| Fire | `ColorRed` | `ColorUiSilver` → **grey** |
| Lightning | `ColorYellow` | `ColorBlue` |
| Magic | `ColorMagicDamage` (208,98,98) | `ColorOrange` |
| Cold | white | white ✓ |

All three elements the user named were wrong, and fire was wrong in the most interesting way.

## Why fire was grey, with a comment promising red

The line read:

```cpp
num.style |= UiFlags::ColorUiSilver; // UiSilver appears dark red ingame
```

That comment was **true when written**. `ColorUiSilver` resolved through `oracool_uisilver.trn`,
and on the indexed path that file's band landed on a reddish ramp. Then renderer stage 4
(v1.11.010, 2026-09-11) retired the 21 Orcl `.trn` files into `RgbDefinedColors` as literal hex,
and `ColorInGameUiSilver` became exactly what its name says:

```
0xF3F3F3 0xDEDEDE 0xCCCCCC 0xB8B8B8 ... 0x111111
```

A plain grey ramp. The colour was chosen by a **name whose meaning moved underneath it**, and the
comment went on asserting the old meaning for ten versions. Nothing compared the two, because
nothing could: one was a name in `floatingnumbers.cpp`, the other sixteen hex values in
`text_render.cpp`.

This is the same class of bug as the stale `02-source-art` paths and the stale vcvars directory,
all three found today. A name that used to mean something is worse than a name that never did,
because the comment defends it.

## The fix: one table, not two

`DamageTypeColor` was file-local in `charpanel.cpp`'s anonymous namespace. It is now exported
(`charpanel.hpp`, `DVL_API_FOR_TEST`) and is **the** definition of what an element looks like.
`DamageTextColor` — a new exported function in `qol/floatingnumbers.h`, lifted out of the switch
that was buried in the file-local `UpdateFloatingData` — defers to it for fire, lightning, magic
and cold.

Two divergences are kept deliberately, and are pinned by the test so they read as choices:

- **Physical** is gold over a monster, white on the sheet. Gold is vanilla's damage number and by
  far the most common thing on screen; white would put the loudest number in the game in cold's ink.
- **Acid** keeps yellow. On the sheet it falls through to white, but here white is cold's. Acid is
  monster-only, so it shares lightning's yellow rather than borrowing an element's colour.

Moving `DamageTypeColor` out of the anonymous namespace was needed for the export and produced one
build error on the way — `error C2668: ambiguous call`, the anonymous-namespace definition and the
new header declaration both being visible at the call site in `charpanel.cpp:249`.

## The test

`OracoolAudit.AnElementIsTheSameColourOnTheSheetAndOverTheMonsterSHead` asserts:

- the four deferred elements match `DamageTypeColor` exactly — the cross-file invariant that was
  broken, now structural *and* checked
- each named regression separately, so a red run says which bug returned
- `Fire != ColorUiSilver`, so fire can never again wear the silver whose meaning moved
- both deliberate divergences
- every element a **player** can deal has a distinct colour, which is the entire point of colouring
  them (acid excluded, being monster-only and sharing lightning's yellow)

**721/721 tests pass** (up from 720). Debug and Release build clean; RTM refreshed. No asset
changed, so no MPQ repack.

## What to look for in play

Cast each aura and watch the numbers over a monster's head: Holy Fire **red**, Holy Shock
**yellow**, Sanctuary **dusty red** (208,98,98, the same ink the character sheet uses for magic),
Holy Freeze **white**. Every other elemental source moved with them — a Firebolt's number is red
now too, where it used to be grey.
