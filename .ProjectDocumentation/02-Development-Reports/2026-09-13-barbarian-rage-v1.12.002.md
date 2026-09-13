# The Barbarian runs on Rage

2026-09-13 — v1.12.002

## Why

> "lets discuss barbarian - if he is to not use mana at all then his skill cant cost mana as well. so we must go D3
> road here - his mana orb will be filling with rage for every hit he makes with certain skills and rage will be used
> to power other skills. 1. we need to introduce orange orb fill named Rage. 2. his mana stat in the hero stat screen
> must read Rage. 3. we must make ana rtifact with all his skills and i should chose which is a rage generator and
> which one is rage spender."
>
> "i made my picks, read them and apply"

The picks came from the Barbarian Rage artifact (collection `picks`, 35 rows).

## The picks

| Role | Skills | Rage |
|---|---|---|
| Generator | Bash, Backhand, Cleave, Double Swing, Concentrate, Frenzy, Clasp of Ruin, Berserk | +6 per hit |
| Generator | Stun | +7 per hit |
| Spender | Ancestral Call | 30 |
| Spender | Leap Attack | 14 |
| Spender | Rend, Split Ranks | 9 |
| Spender | Battle Command, Battle Cry, Battle Orders, Bloodcall, Earthquake, Earthshaker Cry, Find Item, Find Potion, Grim Ward, Ground Stomp, Hammer of the Ancients, Howl, Intimidate, Iron Will, Leap, Rallying Cry, Seismic Slam, Shout, Taunt, Threatening Shout, War Cry, Whirlwind | 10 |
| Neither | every passive | — |

## The rules

- **Pool:** 100 Rage, 120 with Animosity. Whole points.
- **Starts empty on every level:** `InitPlayer` resets it. It is never saved.
- **Generators fill only on a landed use.** The melee skills and RfA-12 melee actives settle only when the blow
  struck. The cast actives, such as Backhand, settle in `ConsumeSpell`, which runs only when the cast did not fizzle.
- **Spenders need the full cost in the pool.** Without it, a melee spender swings as a plain attack. A cast spender is
  refused by `CheckSpell`.
- **Drain:** after 3 seconds with nothing gained or spent, the pool loses 1 point every 5 ticks (4 a second).
- **No mana for the Barbarian's skills.** A full mana pool buys nothing (tested).

## Code

- **`oracool/rage.{h,cpp}`** (new) holds:
  - `ClassUsesRage` and `UsesRage`;
  - the picks as `RageGain` and `RageCost`;
  - `MaxRage`, `GainRage`, `ResetRage` and `ProcessRageTick`;
  - `CanPaySkill` and `SettleSkill`, one pay path for both resources;
  - `SkillResourceLine`, the tooltip line.
- **`Player`:** new fields `_pRage` and `_pRageIdleTicks`.
- **Pay paths:**
  - `melee_skills.cpp` and `rfa12_actives.cpp`: `CanPay` and `Pay` delegate to the rage module. Other classes keep
    the mana path through the same helpers.
  - `spells.cpp`: `CheckSpell` and `ConsumeSpell` take a Rage branch for the Barbarian.
- **Tick:** `ProcessClassTreeTick` drains Rage.
- **Passives and skills reworked around Rage:**
  - **Bloodthirst:** half of the Rage spent returns as life, through the same `OnPassiveManaSpent` hook.
  - **Animosity:** +20 max Rage instead of +20 mana.
  - **Bloodcall:** a kill restores Rage instead of mana. Its level-up stat channel is now Life +8/+3 instead of
    Mana.
  - **Battle Orders:** life only. The mana half is gone.
- **Display:**
  - **Orb:** the mana orb draws Rage over max Rage with `RageOrbLiquidArt`, which is the mana liquid recoloured
    orange at load (`TintRageLiquid`). There is no new asset and no MPQ repack.
  - **Orb numbers:** the value readout shows Rage.
  - **Hero stats sheet:** the row reads **Rage**, in orange. Rage below max is not drawn red.
  - **Hero-select column:** reads **Rage**. `pfile` stores max Rage in the mana field.
  - **Skill tooltips:** the tree and the spell book show "Rage Cost: N" or "Generates N Rage" in place of
    "Mana Cost".

## Tests

`test/oracool_rage_test.cpp`:

- every pick with its exact value, and no skill both or neither;
- only the Barbarian uses Rage;
- fill, spend and clamp;
- `CheckSpell` refuses a spender without Rage even with mana in plenty;
- the drain delay and rate;
- a level reset.

`OracoolMeleeSkills.EverySkillMapsBothWaysAndTheBonusAnswersOnlyWhenArmed` (in `oracool_audit_test.cpp`) needed
updating. It used a Barbarian with no mana to prove that an unaffordable Concentrate gives no bonus. Concentrate is
now a generator and always affordable, so the test failed, as the new rules say it should. It now checks three
things:

- Concentrate works from an empty pool.
- Leap Attack gives no bonus below 14 Rage, even with 1000 mana.
- Leap Attack gives its 50% at 14 Rage.

Debug: 766 of 766 tests pass. Release is built and copied to RTM.

## Not changed

- **Book spells, scrolls and staves** still use mana as they always have. A Barbarian who gains mana from items
  could still cast a book spell, but his orb no longer shows that mana.
- **Mana on items** (+mana affixes, mana leech) still rolls on Barbarian gear and does nothing visible for him.

## In play

- **Orb:** the right orb is orange and empty on entering a level.
- **Generators:** Bash, Frenzy and the other generators fill it only on hits.
- **Spenders:** Whirlwind, the warcries and the other spenders refuse or swing plain without enough Rage.
- **Drain:** Rage drains a few seconds after the fighting stops.
- **Labels:** the hero stats and hero select screens read Rage.
- **Tooltips:** skill tooltips show the Rage cost or gain.
