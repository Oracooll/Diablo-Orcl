---
date: 2026-08-13
status: plan - not implemented
area: Gameplay / Paladin class
---

# Paladin Auras — Gameplay Implementation Plan

The 24 auras are **shipped as data and UI** (v1.1.80): named, described, tiered by level, iconned,
and listed on the Abilities window's Auras sheet. Nothing about them affects combat yet, and
clicking one deliberately does nothing. This is the plan for making them work.

It is deliberately a separate pass. Auras touch damage, resistances, attack speed, monster state and
the save format; none of that should have ridden along with a window redesign.

## The one rule everything else follows

**Exactly one aura is active at a time, with no duration.** It stays on until replaced or switched
off. This comes from the design brief and it is worth restating because it removes an entire class
of work: no stacking, no timers, no expiry bookkeeping, no "which of my six buffs lapsed" UI. Aura
choice becomes a standing tactical decision instead of another rotation.

## The load-bearing discovery

`CalcPlrItemVals` (items.cpp:3126) is the single function where every derived player stat is
assembled. It declares a local accumulator per stat — `bdam`, `btohit`, `bac`, `fr`/`lr`/`mr`,
`ihp`, `ghit`, `lrad`, `enac`, `spllvladd`, `sadd`/`madd`/`dadd`/`vadd` — sums contributions into
them, and writes the totals onto the player at the end.

**It already has a precedent for a non-item modifier.** `SpellFlag::RageActive` contributes to
`sadd`/`dadd`/`vadd` inside this same function (items.cpp:3230). An active aura is the same shape of
thing: a persistent state that contributes to the accumulation and is recalculated whenever
`CalcPlrItemVals` runs.

That means **most auras need no new stat plumbing at all** — only a contribution in one function.
Counting them:

| Maps onto an existing accumulator | Needs new mechanics |
| --- | --- |
| Might (`bdam`), Defense (`bac`), Fanaticism (`btohit`/`bdam` + speed flag), Resistance (`fr`/`lr`/`mr`), Defiance (`bac`), Holy Fire (`_pIFMinDam`/`_pIFMaxDam`), Holy Shock (`_pILMinDam`/`_pILMaxDam`), Life Aura (`ihp`), Focus (`btohit`), Aura Mastery (`bac`+`btohit`+resists), Blessing (`btohit`/`bdam`), Endurance (`vadd`/`ghit`), Vigilance (`lrad`), Righteousness (`bdam`) | Vigor, Regeneration, Holy Freeze, Conviction, Sanctuary, Retribution, Purge, Shield Aura, Cleanse, Swiftness |

Fourteen of twenty-four are a few lines each in a function that already does exactly this.

### Two things the engine does not have

- **No cold damage accumulator.** Diablo I has `_pIFMinDam`/`_pIFMaxDam` (fire) and
  `_pILMinDam`/`_pILMaxDam` (lightning) and nothing for cold. Holy Freeze therefore cannot mirror
  Holy Fire and Holy Shock; its cold damage needs a new pair of fields and a new branch wherever the
  other two are applied (`player.cpp:624` and `:961`).
- **No poison duration model.** Cleanse's brief ("reduces poison duration/damage") assumes a system
  that isn't there. Suggest re-specifying it against what does exist — trap damage, `_pIGetHit`, or
  the Hellfire `ItemSpecialEffectHf` curses — rather than building a status-effect framework for one
  aura.

## State and persistence

One field on `Player`:

```cpp
oracool::Aura _pActiveAura = oracool::Aura::None;
```

**It goes in `PlayerPack` (pack.cpp), not in `SavePlayer`/`LoadPlayer`.** V1 is always-New-Game with
no Continue, so the per-character persistent set is the pack, and `loadsave.cpp` is not the place new
fields belong — see the standing project note. `pfile.cpp`'s `ReadHero` does an exact-`sizeof()`
compatibility check on the pack, so adding a field is a deliberate, breaking-by-design change to
existing test characters.

Aura *levels* (below) are a second field: 24 bytes, one per aura, same treatment.

## Staged build order

Each stage is independently shippable and testable. Do not start a later one before an earlier one
is in the game and verified.

### Stage 1 — Activation, with the fourteen simple auras

- `_pActiveAura` on `Player`, in the pack, defaulted to `None`.
- Clicking an unlocked aura on the Auras sheet sets it; clicking the active one clears it.
- The Auras sheet marks the active row — reuse `DrawSmallSpellIconBorder`'s idea, a bordered icon.
- `CalcPlrItemVals` gains an aura block beside the `RageActive` block, contributing to the existing
  accumulators for those fourteen.
- **Call `CalcPlrItemVals` on activation.** This is the whole of the "make it take effect" work —
  everything downstream already reads the totals it writes.
- Show the active aura on the HUD. The natural home is the LMB skill well, which is currently
  decorative.

Testable end to end: switch to Might, watch the character sheet's Damage rise; switch to Defense,
watch Armor class rise.

### Stage 2 — Aura levels

Per the brief, auras level like spells rather than through skill points: Holy Tomes, shrines and
quest rewards raise an individual aura's level, and strength scales on aura level plus character
level. Reuse the spell-level machinery's shape (`_pSplLvl[64]`, `GetSpellLevel`) rather than
inventing a parallel one.

Until this lands, Stage 1 should scale on character level alone so the numbers are not flat.

### Stage 3 — The pulses (Holy Fire, Holy Shock, Sanctuary)

A periodic area effect centred on the player. The missile system already does this — these should be
missiles on a timer, not a new hand-rolled loop in `ProcessPlayers`. Note that
`oracool/gradual_healing.cpp` is an existing precedent for a per-tick player effect and is the right
thing to read first for Regeneration, which is the simplest member of this group.

Holy Freeze belongs here too but is gated on the cold-damage fields above.

### Stage 4 — The monster-facing auras (Conviction, Sanctuary's AC ignore, Holy Freeze's slow)

These modify *monsters* within a radius, which is the genuinely new work: a per-monster "is inside an
aura" query, and hooks where monster AC and resistances are read. The most likely design is to
evaluate at hit time — `PlrHitMonst` already has both the player and the monster in hand — rather
than maintaining per-monster state that has to be cleaned up when the player walks away.

### Stage 5 — The conditional-damage auras (Purge, Righteousness, Blessing's bonus, Retribution)

"Against demons and undead" needs the monster's class at damage time. `MonsterClass` /
`ItemSpecialEffectHf::ACAgainstDemons` show the shape this already takes for items. Retribution is a
percentage variant of the existing `ItemSpecialEffect::Thorns` (monster.cpp:1231), which is a flat
1–3 — the aura wants a proportional version alongside it.

### Stage 6 — Movement and recovery (Vigor, Swiftness, Shield Aura, Focus's interruption)

Attack speed and hit recovery already exist as tiers in `_pIFlags`
(`FastAttack`/`FasterAttack`/…, `FastHitRecovery`/…) and an aura can simply contribute those flags
in `CalcPlrItemVals` — that part is Stage 1 work. **Walk speed is the exception**: Diablo I has no
walk-speed modifier at all, and adding one touches player animation and movement, which is why
Vigor and Swiftness are last rather than first despite sounding simple.

## Multiplayer

Activation must go over the wire, not be set locally — the same lesson as the friendly-fire toggle,
which must use `NetSendCmd(true, CMD_FRIENDLYMODE)` even in single player. A new `CMD_SETAURA`
carrying the aura index, handled the way other player-state commands are, keeps other clients'
view of the Paladin correct. Everything the aura then does is derived from `_pActiveAura` through
`CalcPlrItemVals`, which each client runs for itself, so only the one byte needs to travel.

## Open questions for the user

1. **Does an aura cost anything?** The brief implies not — no duration, no mana. Free and permanent
   makes the "which aura" decision purely tactical, which is the stated goal, but it also means
   there is never a reason to have no aura active.
2. **Cleanse** needs re-specifying against mechanics Diablo I actually has (see above).
3. **Aura Mastery vs Blessing vs Righteousness** overlap considerably in the brief — the doc itself
   says "there is some deliberate overlap, but I would refine that before implementation". Worth
   settling before Stage 5, not during.
4. **Does the active aura persist across levels and saves?** Assumed yes in this plan.

## What exists today (v1.1.80)

- `Source/oracool/auras.h/.cpp` — the enum, table, tiers, unlock rule and icon indices.
- `ui\aura_icons.png` — 24 icons, cut by `tools/CutPaladinAuras.ps1`, packed in `oracool.mpq`.
- The Abilities window's Auras sheet — lists all 24 with descriptions, greys the locked ones and
  states the level that unlocks them.
- `oracool::ClassHasAuras()` — Paladin only, and note it tests `HeroClass::Warrior`, since Oracool
  renames the Warrior to "Paladin" in display data only.
