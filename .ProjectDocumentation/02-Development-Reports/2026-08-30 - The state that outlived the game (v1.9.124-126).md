# The state that outlived the game (v1.9.124-1.9.126)

**Date:** 2026-08-30
**Versions:** 1.9.124, 1.9.125, 1.9.126
**Trigger:** user, away again: "do a bunch of audits and fix what you find."
**Tests:** 586/587. Six new tests; one red, the level-3 dungeon golden.

Eleven audits. Seven found nothing and are recorded as such. The four that found something all
turned out to be **one bug wearing four faces**, and it is a face this project had not looked at
before: state that lives in a file-local static outlives the *game*, not the *process*.

---

## The finding, in one sentence

`FreeGame()` reset exactly two Oracool things — an aura's sound handle and the set-completion
baseline — and `StartGame()` reset two more. Everything else a feature kept in a file-local static
survived into the next character started in the same session.

That matters because leaving a game closes nothing. "Main Menu" and "Exit Game" both funnel through
`GamemenuNewGame`, which saves the character and clears `gbRunGame` without touching a single
window.

### 1. Levski's Roar handed the next character your items

The worst of the four, because it is an **item-duplication vector** rather than a cosmetic leak.

The monument's grid and its open flag are statics. `CloseLevskiRoar` is deliberately allowed to
**refuse** while the backpack is full — that is a good rule, and it is documented as one: the grid
is not save state, so force-closing would destroy what is in it. But the refusal plus the
no-close-on-exit path meant the window stayed open, and full, into the next character. Who could see
those items, and take them out.

Fixed at both ends, which also leaves the player **better off than before**:

- `GamemenuNewGame` closes the monument **before** `SaveOnExit`, so everything the backpack has room
  for is handed back and then persisted by that save. Items merely staged in the grid used to be
  lost on *every* exit; now they mostly survive.
- `FreeGame` calls the new `ResetLevskiRoarForNewGame` unconditionally as the backstop for whatever
  did not fit. It returns nothing to anyone — by then the player is torn down and already saved.

### 2. The event log showed the next character the last one's life

Its entries are a file-local deque and nothing ever cleared them, so a new character opened the log
onto the previous one's kills, crafts and the death that ended them. `PendingDeathSource` went with
it — a half-finished sentence about someone else's death that would otherwise have been attached to
the new character's first.

### 3. Zeal's burst held pointers into a freed monster array

`ZealStruck` is an array of raw `Monster *`. `StartStand` already resets the chain on every
interruption a game can produce, and a new game reaches `StartStand` long before the player can
swing — so this is **hardening, not a reported fault**. One call in `FreeGame` makes a
dangling-pointer class impossible by construction rather than merely unreachable, which is the trade
this project's stated priority asks for.

### 4. Telemetry timed one monster's fight with another's clock

Not a cross-session bug — a per-*level* one, and the most consequential for the user's actual work.

The kill clocks are keyed by monster **slot**, and slots are handed to a different monster on every
level. A monster wounded but never killed leaves its clock running, and the next occupant of that
slot was credited with the elapsed time as its own fight length.

This was found once before, on 2026-08-16, and **mitigated rather than fixed**: `TelemetryRecordKill`
discards anything over ten minutes as "a stale clock, not a fight". That catches the worst of them
and only the worst. A stale clock *under* ten minutes logs a wrong fight length that looks entirely
plausible, so it passes the filter and lands in `balance_telemetry.csv` — the file this fork
balances from. A plausible wrong number is the expensive kind.

`InitLevelMonsters` now clears the clocks alongside the sprite cache it already clears, for exactly
the same reason: what it holds is about to belong to different monsters. The reset is deliberately
not gated on the telemetry option, so a clock cannot survive the option being switched off and back
on. The ten-minute filter stays as the backstop it always was.

---

## Clean audits

Recorded so the next pass does not repeat them.

- **Spell-level storage.** `PlayerPack` saves `pSplLvl[37]` + `pSplLvl2[10]` = 47 of 59 spell
  levels. IDs 52–58 are the class skills, whose investment lives in `_pSkillInvestment` and **is**
  fully chunk-persisted, so nothing is lost there. IDs 47–51 are Hellfire's rune spells, whose
  levels are genuinely unsaved — but that is inherited from upstream (vanilla had the same 47-slot
  pack) and unreachable in practice, since nothing puts a rune spell in `_pMemSpells`.
- **Hero chunk round-trip.** Every tag written is read. Tag 5 is read-only by design (superseded,
  kept for migration); tag 3 is declared and unused.
- **Class-tree SpellID collisions.** Three rows share `SpellID::ManaShield` — Sorcerer, Bard and
  Monk. `_pSkillInvestment` is keyed by SpellID, so that would be a real collision except that a
  character only ever reaches its own class's rows.
- **The level-99 experience economy.** `_pExperience` and `ExpLvlsTbl` are 64-bit throughout; the
  two places that narrow to `int` clamp first. A single kill maxes at about 2.6M XP.
- **`_pSkillInvestment` bounds.** `CanInvestSkillPoint` reads the array before any bounds check, but
  `||` short-circuits into `IsSkillInvestable`, which guards both `Invalid` and `>= MAX_SPELLS`.
  Safe, though by ordering rather than by construction.
- **`GetSpellBitmask` shift UB.** It computes `1ULL << (id - 1)`, so `SpellID::Null` would shift by
  −1. Every caller that can see a non-spell guards with `IsValidSpell` first, and the one loop that
  walks all spells starts at 1.
- **Duplicate INI keys.** None within a category. The two that repeat across categories
  (`LastSinglePlayerHero`, `LastMultiplayerHero`) are the OnlyDiablo/OnlyHellfire pair, which write
  to different sections.
- **`refreshUntilItemNames`.** A 512-byte buffer holding user-typed text; both the INI read and the
  in-game entry field are bounded by `sizeof(...)` and `sizeof(...) - 1`.
- **Item-description nouns.** The `default: return _("item")` fallback is unreachable — the row-two
  line is gated to items with a worn slot, so charms, gems, runes and potions never reach it.

## Two tests that found nothing, and pin what was checked

- **Every one of the 370 runewords is formable on a real base item.** A word must fill its host
  exactly, and a host's socket ceiling is its footprint in backpack cells. The runeword table and
  `AllItemsList` are produced by different things, so their agreement was an assumption.
- **The XP bar is fully inside the HUD chrome rect in both HUD modes.** This **disproves** the
  external audit's UI-01 claim that it "allows world clicks through it" — checked rather than
  accepted, on all four corners.

---

## Verification

586/587 after every change. Six tests added, and the two that pin real fixes were checked against
the defect:

| Test | Fails when |
|---|---|
| `LeavingAGameDoesNotLeakLevskisGridToTheNextCharacter` | the reset is neutered — reports the grid taking fewer items than a clean one |
| `LeavingAGameDoesNotLeakTheEventLogToTheNextCharacter` | asserts the ENTRY COUNT, not the window flag |
| `LevelChangeClearsTelemetryKillClocks` | asserts a clock was actually started before the reset |
| `EveryRunewordIsFormableOnSomeRealBaseItem` | a zero capacity would fail it, so it cannot pass vacuously |
| `TheXpBarDoesNotLetClicksReachTheWorld` | all four corners, both HUD modes |

The event-log test is worth a note against itself. Its first version asserted only that the window
was closed, which would have gone on passing while every entry survived — the exact shape of vacuous
test this audit round exists to catch. It needed a small `EventLogEntryCount` accessor to be honest,
and `DVL_API_FOR_TEST` on `Monsters` for the telemetry one, since `Monster::getId()` is the offset
from `&Monsters[0]` and a stack-built monster reports a garbage slot.

## What is left

Nothing new is outstanding from this round. Still open from earlier today, unchanged:

- The mini-map click-through (a design call).
- Zeal's accuracy bonus applying to all Paladin melee rather than only Zeal (a balance call).
- Whether the Abilities window should stop binding F-keys (a requirements call).
- The Release tree at v1.9.114 (Debug-only is the standing instruction).
- The level-3 dungeon golden, argued to be inherited from the vendored baseline.

The four windows whose open flags also leak — the crafting menu, waypoint list, skill picker and
runeword book — were left alone. They hold no items and no pointers, `ClosePanels` resets most of
them on any level change, and the worst case is a window appearing open at the start of a session.
Worth folding into a single teardown list if this area is touched again.
