---
date: 2026-08-15
version: 1.6.3
area: Paladin skills / input
---

# Five Reasons the Skills Did Nothing

> "skills dont seem to work. neither does shift+click to force attack. i dont see zeal making burst
> hits. […] shift left click still moved my hero. many things are not ok."

Five separate faults, none of them in the skills themselves. Every one of them was upstream — in
where the game thinks the UI is, in what shift is allowed to mean, in when a queued strike is thrown
away, and in which hand a shield counts as being in.

## 1. The bottom third of the screen ate every skill click

The big one, and the reason it read as "skills don't work" rather than as a targeting problem.

`CheckPlrSpell` opened with `if (GetMainPanel().contains(MousePosition)) return;`. `GetMainPanel()` is
still the vanilla **640×128 rect at the screen bottom** — the HUD overhaul deliberately left it there,
because the character/inventory flyouts centre against it. But it has not been *solid UI* since that
overhaul: what is actually chrome now is a small centre plate.

So a right-click on a monster anywhere in that band returned having done **nothing at all**. Not
cast, not walked, no "I can't do that" — nothing. And with the button empty, the same click went to
`LeftMouseCmd` instead and worked perfectly, which is exactly the shape of *"the skills are broken"*
rather than *"the bottom of the screen is dead"*.

The router (`LeftMouseDown`) and the cast path had **two different ideas of where the UI is**, and
only one of them had been updated. There is now one: `oracool::IsPointOverHudChrome`, which both call.

## 2. The left button never saw shift

```cpp
CheckPlrSpell(false, lmbPlayer._pLRSpell, lmbPlayer._pLRSplType);
```

Hardcoded. The whole of the previous build's shift handling was unreachable from the left button, so
*"shift left click still moved my hero"* was a precise description of a one-word bug.

## 3. Shift did nothing instead of forcing the cast

The previous version treated out-of-range-with-shift as the one case that deliberately does nothing,
reasoning that walking there would disobey shift.

That reasoning stopped one line short. The answer is not to walk **or** to give up — it is to act
**where the cursor is**, which is what shift means everywhere else in this game. So now:

- a **ranged** skill fires at the cursor's own tile, monster or not
- a **melee** skill swings in place toward it (`CMD_SATTACKXY`, vanilla's own shift-click command)
  with the latch armed, so Zeal, Hammer of Faith and Shield Bash still ride the swing if it connects

That is what "shift click didnt produce blessed hamer" was pointing at.

## 4. Zeal's queued strikes were deleted, every time

`ProcessZealBurst` abandoned the burst whenever `_pmode` was no longer `PM_ATTACK`. The arithmetic
makes that impossible to satisfy:

- the first strike lands at the swing's **hit frame** — a little past the middle of the animation
- the rest are spread across a budget of **150% of the whole swing**

By construction the queued strikes fall *after* the animation has ended. The guard was not trimming
the burst's tail; it was deleting all of it. At level 6, both extra strikes; at higher levels, most.

Now it stops only for the things that mean the Paladin is not there to throw it — dead, or changing
floors. Walking away does not, and should not: the strikes were already paid for by the swing that
landed, and `NextZealTarget` re-checks reach before each one, so stepping back simply finds nothing
to hit and ends the burst on its own.

**The spacing changed too.** The gap was `budget / strikes`, which put half a second between the two
blows of a level-6 burst and a tenth between the five of a level-16 one — so the low-level version
read as *slower* rather than *shorter*. It is now `budget / MaxZealStrikes`: one rhythm at every
level, and only the length grows.

## 5. A shield in the left hand was not a shield

```cpp
return player.InvBody[INVLOC_HAND_RIGHT]._itype == ItemType::Shield;
```

Shields are `ILOC_ONEHAND` — Buckler, Small Shield, Large Shield and every tier above them — and
`CheckInvPaste`'s one-hand case puts the item in **whichever hand slot it was dropped on**. Drop a
shield on the left and it stays there.

The engine's own armour-class and block-chance code has always checked both hands. This was the odd
one out, and the cost was silent: Shield Bash and Blessed Shield are gated on it, so both vanished
from the Abilities window for a player whose shield happened to sit on the left.

## The sixth, found while fixing the fifth

`_pAblSpells` — the mask that decides what a player *has* — was rebuilt only at character creation, on
level-up, and on level load. But since Shield Bash and Blessed Shield now depend on **equipment**,
picking up a shield left that mask stale.

The failure was a nasty one to read: the Abilities window drew the two rows as unlocked, because
drawing tests `HasShieldEquipped` live, but **refused to ready them**, because readying tests
`IsSpellKnown`, which reads the mask. The skill appeared, and then did nothing when clicked, until
the next floor. It is now rebuilt in `CalcPlrInv`, which is where equipment changes land.

## What was NOT broken

**Blessed Hammer only firing on a click on a monster** was the standing rule working as specified —
*"ranged skills only initiate when clicked on monsters within range, else - move command"*. With
fix 3, shift now overrides it and casts at the cursor.

**Blessed Shield and Fist of the Heavens** are level 20 and 24. Their missiles, their art and their
impacts were all verified intact and reachable; they were unreachable through fixes 1, 3 and 5, and
Blessed Shield additionally through 6.

## State

**354/356** — `Drlg_l1.CreateL5Dungeon_diablo_3_844660068` and `Timedemo.WarriorLevel1to2`, red before
any of this.

Every object compiled and all 34 test binaries linked, but **`DiabloOrcl.exe` itself could not be
relinked** — the running game held the file. It needs one more build after the game is closed.
