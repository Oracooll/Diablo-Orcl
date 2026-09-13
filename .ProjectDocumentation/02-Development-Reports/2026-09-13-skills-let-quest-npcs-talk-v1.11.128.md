# Attack skills let quest NPCs talk

2026-09-13 — v1.11.128

## Why

> "sweep code behind all attacking skills and make sure they let me engage quest npcs into conversation mode,
> because now they dont."

## How a quest NPC talks

Lachdanan, Zhar, Gharbad, Snotspill, the Warlord and Lazarus are monsters whose `talkMsg` is set and whose goal is
Inquiring or Talking (`CanTalkToMonst`). Talking to one is a **CMD_ATTACKID**: the hero walks up, and
`ACTION_ATTACKMON` checks `talkMsg` on arrival and calls `TalktoMonster` instead of swinging.

## The sweep

Every click that reaches the world with a readied ability goes through one function, `CheckPlrSpell`. It is
reached from:

- the left button with a skill readied, whenever `IsEnemyUnderCursor` (any non-town monster, quest NPCs
  included);
- the right button with a skill readied;
- the quick-cast keys (`QuickCast`, plrctrls.cpp);
- the controller's spell action;
- the held-button repeat in track.cpp (Spell and SpellMonsterTarget).

None of its branches checked for a talker:

| Branch | What it sent at a quest NPC |
|---|---|
| Rogue bow skills | CMD_RATTACKID (talks only if already standing), CMD_RATTACKXY with shift |
| RfA-12 melee skills | CMD_ATTACKID (talks), CMD_SATTACKXY with shift (swings) |
| Barbarian and Monk melee | CMD_ATTACKID, but a **leap** when not adjacent; CMD_SATTACKXY with shift |
| Paladin skills | **CMD_SPELLID** for ranged skills, a **walk** when out of range, CMD_SATTACKXY / CMD_SPELLXY with shift |
| Every ordinary spell | **CMD_SPELLID** or CMD_SPELLXY — `ACTION_SPELLMON` has no talk branch at all |

So with most skills or any spell on a button, a quest NPC could not be spoken to.

The plain-attack paths were already right, except one: `LeftMouseCmd` routes a talker to CMD_ATTACKID with or
without shift, but `RightMouseBasicAttack`'s shift branch swung in place.

## The fix

**One guard at the top of `CheckPlrSpell`**, after the interface checks (so a click on a window still does
nothing) and before any branch:

- **A monster in town** (a townsperson) gets `CMD_TALKXY`, the same as a plain town click. This also covers
  the right button with a skill readied, which used to answer "I can't cast that here".
- **A dungeon monster that `CanTalkToMonst`** gets `CMD_ATTACKID`, which walks up and talks.
- Shift does not override it, matching vanilla's shift-click.
- The four skill latches (Paladin, bow, class melee, RfA-12 melee) are **disarmed first**. Otherwise the next
  real swing would inherit a skill this click never used. `LastMouseButtonAction` is cleared so a held button
  does not keep re-opening the dialogue.

**`RightMouseBasicAttack`** now talks to a talker with shift held, as `LeftMouseCmd` does.

A quest NPC that has finished talking (its goal leaves Inquiring/Talking, e.g. a hostile Gharbad) is attacked
by the skill as before.

## Tests

The dispatch runs against the cursor globals, the network command queue and `MyPlayer`, and has no test fixture.
The full suite was run for regressions. **Verification is in play.**

## For the user to look at

With a skill or spell on either mouse button, click each quest NPC; the hero should walk up and the conversation
should open. Do the same with shift held and with the quick-cast keys. Then check that attacking an ordinary
monster with the same skill still works.
