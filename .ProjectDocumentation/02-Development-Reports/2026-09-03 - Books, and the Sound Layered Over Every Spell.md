# Books, and the sound layered over every spell (v1.9.195)

Three reports, 2026-09-03.

## 1. "not sure i learned the spells" / "not sure i upgraded a single level"

**The mechanics were already right. The feedback was not.** A read that worked and a read that was
refused both produced the same thing on screen: a page-turn sound and a voice line. Nothing named
the spell, the level, or the reason.

What the investigation found, and it explains the stack exactly:

- The **Rule of Rangs** (the user's own rule, 2026-08-19) makes each rank of a spell cost one more
  character level than the last. Fire Ball sits in the level-18 band, so rank 1 wants level 18,
  rank 2 wants 19, rank 3 wants 20.
- A **stack of three books is three separate reads**, each gated on its own rank. Reading a stack
  of three Fire Ball books at level 18 grants exactly one level, then refuses twice — correctly,
  and silently. That is "i am not sure i upgraded a single or let alone three levels".
- The book is **not consumed** on a refusal. That gate was already in place, in `UseInvItem`,
  ahead of the call that eats the item.

Two changes:

- **A read now says what it did**: "You have learned Fire Ball." or "Fire Ball is now level 3."
- **A refusal now says why**: "Fire Ball needs level 19 to reach level 2. The book is unread."

## 2. The one real bug: a maxed spell ate the book

At `MaxSpellLevel` (30 here, not vanilla's 15) the level gate passed — the Rule of Rangs is
satisfied by any high-enough character — and `UseItem` then skipped the level change because
`newSpellLevel > MaxSpellLevel`, granted the mana, and returned. The caller consumed the book
regardless. A book of a maxed spell was destroyed for nothing.

Now refused before consumption, with "Fire Ball is already at its highest level."

## 3. "some unnecessary chatgpt sound played every time i cast"

The class-tree sound package layered a **cast cue** over 92 of the 163 tree rows in `StartSpell`,
and an **impact cue** in `CheckMissileCol` and on the Paladin's melee skills. A spell with a
perfectly good engine sound therefore made two noises.

All three are removed. Every spell plays its own `sSFX` again, unconditionally.

**Kept, deliberately:** the aura loops (`SetAuraLoop`) — an aura has no engine sound at all, so
those are its only voice rather than a second one — and the Learn click in the Abilities window,
which is UI feedback rather than a spell. Say the word and those go too.

The missile still records which tree row threw it; nothing reads it now, and the comment at the
write site says so.

## Tests

`OracoolBooks.TheLevelGateRefusesTheReadRatherThanEatingTheBook` pins the bands, both sides of the
rank-1 and rank-2 boundaries, that a band-1 spell is readable at level 1, and that the ceiling is
not reachable through the Rule of Rangs alone — which is what makes the new ceiling check load-
bearing rather than dead code.

623/624, the standing baseline.

## To look at in play

Read a book you cannot use yet: the line names the level you need and the book stays. Read one you
can: the line names the spell and its new level. Cast anything and listen for one sound.
