# Round 4 — the melee pages are swung (v1.9.186)

Round 4 of [[Plan - Developing Every Inert Skill]]. Seventeen rows go live: the Barbarian's Bash,
Leap, Double Swing, Stun, Leap Attack, Concentrate, Frenzy, Whirlwind and Berserk; the Monk's
Sweeping Reed, Breaking Current, Vaulting Strike, Wheel of Heaven, Seven Reeds, Open Palm, Hundred
Fists and Radiant Palm.

## The mechanism

`oracool/melee_skills.{h,cpp}` is the Paladin's latch (`paladin_melee.h`) generalised, and a
second latch rather than a rewrite of the first. A readied melee skill is **swung**: the click
becomes the ordinary attack with the latch naming the skill; `PlrHitMonst` asks the latch for a
damage multiplier on every blow (`ClassMeleeSkillDamagePercent`); `DoAttack` asks it once per
swing, at the hit frame, for everything else (`ApplyClassMeleeSkillOnSwing`). The per-swing hook
runs whether or not the front tile held a monster — Whirlwind through an empty tile still spins.

Every skill is a `Profile` (bonus %, bonus per rank, extra blows, extra-blow growth and cap,
extra-blow share) plus at most one reaction:

| Reaction | Skills | Through |
|---|---|---|
| knockback | Bash, Open Palm | `M_GetKnockback` |
| stagger, uniques exempt | Stun (30 ticks +4/rank), Breaking Current (20) | `StunMonster` |
| strike all eight neighbours at a share | Whirlwind, Wheel of Heaven (66% +5/rank) | own roll of the weapon |
| strike the two tiles beside the target at full | Sweeping Reed | own roll, arc does not reach behind |
| a kill splashes its blow onto every neighbour | Radiant Palm | the front hit's damage |

Extra blows (Double Swing 2 @75%+5, Frenzy 2 @100%, Seven Reeds 3+1/3 ranks ≤7 @60%, Hundred
Fists 4+1/2 ranks ≤7 @50%) are shares of the **first** blow's damage, which already carries the
skill's bonus — so the arithmetic is what the row says.

The three leaps ride the engine's own `MissileID::Teleport`, clamped to four tiles (+1 every three
ranks, eight at most) along each axis toward the cursor; the teleport's own `FindClosestValidPosition`
turns "onto the monster" into "beside it". Leap Attack and Vaulting Strike swing with their bonus
when the target is adjacent and leap when it is not; plain Leap always leaps.

Mana: paid once per swing, only when the skill did something or its bonus rode a blow that landed.
An unaffordable skill is a plain swing at no cost, the latch kept so the next swing asks again.
The click itself is refused with "not enough mana" when the pool is empty, before the swing starts.

## The collision

`SpellID::Berserk` already existed — the vanilla turn-a-monster spell, which the Bard's row uses.
The Barbarian's Berserk is `SpellID::BerserkBlow`. The test pins that the vanilla one is not a
melee skill.

## Held back

Double Throw (no throwing weapons exist) and Purifying Breath (a breath cone; Round 6's shape).
Not built by design, and the rows say so: Concentrate's uninterruptibility, Berserk's defence
penalty, Frenzy's attack speed, Whirlwind's travel.

## Numbers

- MAX_SPELLS 78 → 95; seventeen more bytes in the investment chunk; writehero hash re-baselined
  to `14a42387…` with its reason.
- 616/617, the standing `Drlg_l1` baseline the only failure.
- New test: `OracoolMeleeSkills.EverySkillMapsBothWaysAndTheBonusAnswersOnlyWhenArmed`.

## To look at in play

Ready Bash on a button and hit something — it should fly back. Whirlwind beside two monsters with
an empty front tile — both should take a blow. Leap at a far tile — a two-tick teleport. Stun a
unique — nothing (by design). Run the mana out — the swings continue, plain and free.
