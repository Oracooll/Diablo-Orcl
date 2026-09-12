# Crafting rebuilds keep the ethereal bargain and the name, and no longer destroy Mystic Orbs

2026-09-13 — v1.11.104

## Why

Audit finding #11 said the tier-up recipes lose what a player put into an item. The user settled the
naming question as decision **D5**: a tier-up keeps the item's name.

## The audit was partly wrong, and the correction matters

The audit claimed the rebuild wipes **Mystic Orbs and sockets**. That was inferred from
`RetierOracoolItem` without checking the recipe gates, and it overstated the problem.

`IsTierRecipeGear` has refused both socketed and orbed items since the 2026-08-26 audit, with a comment
giving exactly the right reason — a reroll cannot put stones or orbs back. The real picture, read from
every target finder:

| Recipe | Refuses socketed | Refuses orbed (before this change) |
|---|---|---|
| 5 Reforge | yes | **no** |
| 6 Ennoble | yes | **no** |
| 7 Recast | yes | **no** |
| 9 Enrich, 10 Consecrate, 11 Awaken, 12/13/14 Rerolls | yes | yes (`IsTierRecipeGear`) |

So:

- **Sockets were never at risk.** Every rebuild finder rejects a socketed item.
- **Orbs were genuinely destroyed by three recipes** — Reforge, Ennoble and Recast — which find their
  targets without `IsTierRecipeGear`.
- **The ethereal bargain was lost by every rebuild**, because `GetItemAttrs` zeroes the flag and
  `InitializeItem` starts from an empty Item. Its +35% vanished and its durability was quietly given
  back.
- **The name changed on every rebuild**, because the new seed hashes to a new pool name.

## What changed

**Orbs** — `GridMaterialsFor` (crafting.cpp) now refuses every recipe that rebuilds its target when the
target carries a Mystic Orb, once, rather than trusting each finder to remember. Recolour (8), Make
Ethereal (15) and Mend (16) change the item in place and are unaffected. Refusal costs the player
nothing: ingredients are consumed only after a recipe succeeds.

**Ethereal and name** — `ReforgeOracoolItem`, `RetierOracoolItem` and `EnnobleOracoolRare` capture a
`RebuildKeepsake` before the rebuild and restore it after:

- ethereal is re-applied through `MakeItemEthereal`, so the +35% is recomputed on the new roll rather
  than copied from the old one;
- the name is kept only when it came out of the name pool (magic or a rolled tier) **and** the result is
  still a rolled item. Ennoble passes `keepName=false` — a unique has its own name. A plain item being
  Enriched had no rolled name, so it takes a new one.

**Set recipes** — Recast and Consecrate re-apply ethereal after `InitializeItem`. On an indestructible set
piece `MakeItemEthereal` declines, and the piece simply stays whole.

## Why orbs are refused rather than kept

Keeping them needs the save to record *which* orbs an item took, and it records only how many. Adding
that is an item-format version bump, and the loader compares that version for **exact equality** in
`LoadHeroItems`, `LoadStash` and `LoadInventoryTabs` (loadsave.cpp:2726, 2792, 2902). A bump would make
all three refuse to load existing saves. That was not a change to make unattended while live saves are
in use. The proper fix — a v10 record with a backward-compatible reader — is left as a decision.

## Test

`OracoolAudit.ARebuildKeepsEtherealAndNameAndStillRefusesSocketsAndOrbs`:

- Reroll Rares keeps ethereal and the name; Awaken keeps both while climbing to Primal; Enrich keeps
  ethereal and gives a plain item a new name.
- A socketed item and an orbed item are refused by Reroll Rares, with the seed and the reagent stack
  untouched.
- Reforge and Recast refuse an orbed item, and each fixture is shown craftable once the orb is removed,
  so the refusal cannot pass for some other reason.

**Proven by reverting both fixes at once** (guard disabled, ethereal restore disabled):

```
Reroll Rares removed the ethereal bargain
Awaken removed the ethereal bargain
Enrich removed the ethereal bargain
Reforge is offered on an item carrying a Mystic Orb
Recast is offered on an item carrying a Mystic Orb
```

## Housekeeping found on the way

PowerShell here-strings drop the newline before their closing `'@`, so several inserted blocks glued a
function's opening brace onto the next line (`{\t// A CHOSEN recipe`). A scan of all seven edited files
found and repaired every instance — one glued brace in the test file, and missing final newlines in
`smart_loot.h` and `smart_loot.cpp` — and an initial socket-carrying helper that could never fire was
removed rather than left looking meaningful.

## Verification

- Debug: **732 tests, 0 failed.**
- Release built and linked; `DiabloOrcl RTM\DiabloOrcl.exe` refreshed.
- No save format change; no asset change.

## What to look at in play

1. Make an item ethereal, then Reroll or Enrich it: it should stay ethereal, with half durability.
2. Reroll a rare with a name you know: the name should stay.
3. Put a Mystic Orb on a magic item: Reforge should no longer light up for it.
