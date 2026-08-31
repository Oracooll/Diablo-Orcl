---
date: 2026-08-15
version: 1.5.72
area: Paladin skills / melee
---

# The Three Skills That Ride a Swing

Steps 1–4 of the implementation plan: the foundation, Zeal reworked, Shield Bash and Hammer of Faith
built. Three of the seven now do something, plus Charge, which already did.

## The latch, and why it had to exist

`DoAttack` resolves every swing in one place. It **cannot tell which mouse button threw it** — by the
time the animation reaches its hit frame, several frames after the click, all that survives is
`_pRSpell` / `_pLRSpell`, and which of the two acted is gone.

"Zeal is active only" is a statement about that missing fact. So it gets latched at the click:

```
ArmMeleeSkill(std::optional<PaladinSkill>)   // at the click
ArmedMeleeSkill()                            // at the hit frame
```

Armed in `CheckPlrSpell` — the one funnel through which a mouse button acts, and it already receives
the pair for whichever button was pressed. Cleared in `LeftMouseCmd`, the path a plain swing takes,
and on the two exits that mean "no swing is happening": shift (which is the engine-wide *ignore what
is readied*) and the out-of-range walk. The controller and hold-to-attack repeat paths deliberately
do **not** clear it — you are still holding the same button down.

File-scope state rather than a `Player` field, deliberately: single-player only, one swing resolving
at a time, and it must never reach the save format or the net packet.

## Zeal: passive → active

`IsWarriorSplashDamageEnabled` returned `CanUsePaladinSkill(player, Zeal)`, so from level 6 it fired
on **every** swing, forever, whatever was on your buttons. It now fires when Zeal is what the swing
was thrown with. Same five-target cap, same "mana only if it actually carries".

`warrior_splash.{h,cpp}` is gone, replaced by `oracool/paladin_melee.{h,cpp}` — it was named for the
one skill that used it, and three do now.

## Hammer of Faith is not Zeal with a different name

The two delivered descriptions overlap ("hits up to five adjacent enemies" against "a splash damage
melee attack"), so they needed distinct shapes or one would be strictly worse:

| | Zeal | Hammer of Faith |
|---|---|---|
| Targets | up to 5 | every square touching the target |
| Damage each | full | half |
| Reading | a flurry across several enemies | one blow whose force carries |

Half damage is the load-bearing half of that. At full damage Hammer of Faith would beat Zeal at every
enemy count, and the pair would be a worse option and a better one rather than two answers.

## Shield Bash, and the stun primitive

The open question from the plan, resolved by reading rather than guessing.

`M_StartHit` only holds a monster for its own hit animation — too short to read as a stun. Stone
Curse's petrify *looks* right in code: it has a duration and saves the mode to restore. But
`Monster::getVisualMonsterMode()` finds the saved pose by **searching the missile list for a
`MissileID::StoneCurse` that owns this monster** — so a petrify inflicted by anything else renders as
the stone statue. Shield Bash is a shove, not a spell.

The right primitive was `AiDelay`, monster.cpp's own "make the AI wait before thinking again": the
monster stands with its normal animation, does nothing, and returns to Stand on its own, with no
graphic swap and no missile to keep alive. It was file-local and reads as an AI-pacing detail, so it
is now exposed as `StunMonster(monster, ticks)` — a name that says what a caller wants — with
`AiDelay`'s Lazarus guard inherited rather than restated.

25 ticks, about a second and a quarter. Shield Bash adds **no damage of its own**: the stun is the
skill, which is also why it does not scale with anything.

## Weapon damage, without any skill knowing what a weapon is

The user chose weapon-based scaling. The hook already receives the damage the swing itself dealt, so
every skill scales from that number. Nothing here reads an item, a stat or a level to compute damage
— which means gear matters automatically, and there is no second damage formula to keep in step with
the engine's own.

## The test that had stopped testing anything

`Player.WarriorSplashDamage_Disabled_UntilSkillsSystemExists` asserted Zeal was unavailable. It
passed — but on a fixture whose `_pLevel` was 0, so it would have passed whatever the gate did. It
has been replaced by two that check real invariants: the class/level/mana/single-player gates each
tested by moving one input across its boundary, and the latch's overwrite behaviour, since a latch
that accumulated would let the previous skill apply to the next blow.

## State

353/355 — the standing two failures, and one test replaced by two.

Not tested in-game — the user runs the game. Worth checking: Zeal only splashes when it is the
readied skill; switching buttons mid-fight does not leave the old skill applying; Shield Bash does
nothing without a shield; and Hammer of Faith catches the whole ring rather than five.
