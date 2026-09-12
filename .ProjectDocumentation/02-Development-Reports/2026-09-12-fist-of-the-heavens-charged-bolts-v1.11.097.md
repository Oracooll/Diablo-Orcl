# Fist of the Heavens disperses Charged Bolts, and the sheet finally agrees it is lightning

2026-09-12 — v1.11.097

## What the user asked

Two things, one after the other:

> "FotH to disperse Charged Bolts when it lands instead of this current asset."

> "make sure lightning dmg skill have their dmg font in gero stats screen in proper font color.
> start with foth."

The second turned out to be a consequence of the first being half-done for a month: the skill cast
lightning, but the hero stats sheet drew its damage in physical white.

## 1. The landing effect: a Nova ring becomes an aiming device

What was there: 36 `MissileID::MiniNovaBall` missiles on a radius-4 arc, re-skinned to `holy_spark`.
That served the original 2026-08-15 spec — "cast Mini-Nova spell at cursor location [...] travel
distance of lightnings of 4 tiles" — and `ProcessNovaCommon` already fires at a radius-4 ring, so
the 4 tiles needed no work at the time.

Charged Bolts are a different animal: **they wander**. A Nova ring by construction cannot disperse,
because every bolt's path is fixed at launch. So the ring stops being the effect and becomes the
*aim*: eight points on a radius-4 ring, each bolt launched at one of them, and from there it goes
where it goes.

```cpp
const int boltDamage = std::max(damage * FistNovaPercentAt(spellLevel) / 100, 1);
const int boltCount = spellLevel / 2 + 5;
```

Five bolts at rank 1, climbing one per two ranks, which is Diablo II's own Charged Bolt curve.

### The trap: AddChargedBolt overwrites the damage you hand it

`AddMissile` takes a damage argument, and every other missile in this file trusts it.
`AddChargedBolt` does not — it writes `_midam = GenerateRnd(Magic/4) + 1`, a *Sorcerer's* scaling
off the caster's Magic stat. A Paladin's Fist of the Heavens scales off the **weapon**
(`RollWeaponDamage`, the standing 2026-08-15 decision for this whole file), so accepting that would
have silently swapped the skill's damage source and made gear irrelevant.

The fix is one line per bolt, written back after the missile exists:

```cpp
bolt->_midam = boltDamage;
```

The central blast is untouched: `ApocalypseBoom` still lands on the target for
`FistCentrePercentAt`, and `IS_ISHIEL` still plays.

## 2. The stats sheet: three descriptions of one skill, and the sheet had the odd one

`PaladinCastDamageType(FistOfTheHeavens)` returned `GetMissileData(MissileID::ApocalypseBoom)`,
whose `damageType()` is **Physical**. So the character sheet quoted FotH's damage in physical white
while the skill played a Lightning cast animation and threw Lightning missiles.

The reasoning behind the original choice was defensible — "the mace only falls; the blast on the
target is what deals the damage the sheet quotes" — but the bolts are the right thing to ask. They
carry most of the damage at every level, they are what the player sees, and asking the missile the
skill actually throws is the same rule that stopped Blessed Hammer's blue appearing as white:

```cpp
return GetMissileData(MissileID::ChargedBolt).damageType();
```

The central blast stays Physical in its own right, which is honest: a falling mace is not lightning.

## Was FotH the only one wrong?

Checked, because "start with foth" implies others. It was the only one. Every non-Paladin spell
derives its sheet colour from `sMissiles[0]`, which is by construction the missile it throws; the
Paladin's four are the only hand-written rows. `PowerStrike` reads weapon damage on the sheet, which
is correct — it is a hybrid, and the physical half is the half the sheet is quoting.

## Test

Added to the readied-slot palette table in `OracoolCharPanel.DamageFieldsAreColouredByDamageType`:

```cpp
{ SpellID::FistOfTheHeavens, UiFlags::ColorYellow,
  "Fist of the Heavens is lightning - the bolts it disperses, not the mace" },
```

**Proven by reintroducing the bug.** Reverting line 316 to `ApocalypseBoom` and rebuilding gave:

```
OracoolCharPanel.DamageFieldsAreColouredByDamageType ...***Failed
  oracool_audit_test.cpp(8761): error: Expected equality of these values:
  Fist of the Heavens is lightning - the bolts it disperses, not the mace
```

The message names the reason, not just the mismatch, so the next person to change this reads the
argument rather than re-deriving it.

## Verification

- Debug: **727 tests, 0 failed.**
- Release built and linked; `DiabloOrcl RTM\DiabloOrcl.exe` refreshed.
- **No MPQ repack:** no asset changed. `holy_spark` is no longer drawn by this skill but stays
  shipped — it is referenced elsewhere.

## What to look at in play

1. Cast Fist of the Heavens. The mace falls, `IS_ISHIEL` plays, and bolts **scatter irregularly**
   rather than fanning in a clean circle. That irregularity is the whole change.
2. Bolt count should visibly grow as you invest — 5 at rank 1, 10 at rank 10.
3. Damage should track your **weapon**, not your Magic stat. Swap to a much better mace and the
   bolts should hurt more.
4. Hero stats screen, FotH in a readied slot: the damage line should be **yellow**, matching
   Lightning, where it was white before.

