# "Player sent an invalid packet" — and the drop was quietly not saved

2026-09-12 — v1.11.083

## What was reported

> i just killed the skeleton king on nightmare and got a msg in the log - player XXXX send a request
> for invalid packet or something like this.

then, crucially:

> happened again when i opened a sarcophacus - player XXXX sent an invalid packet or something like
> this.

The second report is what made this solvable. One report looked like a Skeleton King quest bug; two
reports with nothing in common but **spawning items** pointed straight at the item-drop path.

## The message

`Source/msg.cpp:69` — `"Player '{}' sent an invalid packet."` It has exactly one source: the
`ValidateField` / `ValidateFields` macros (`msg.cpp:49-65`), used in exactly one function,
`IsPItemValid`. So every instance of this message in the game is item-spawn packet validation.
Nothing quest-related and nothing object-related can produce it.

## It was not a log line

On a failed check the handler skips `DeltaPutItem` (`OnDropItem`, `msg.cpp:2042` region). The item
was already created in the local `Items[]` array, so it lies on the floor and can be picked up — but
it is in **no delta**. Leave the level and come back and it is gone.

So this was **silent item loss on every container and boss-unique drop from Nightmare upward**,
including the Skeleton King's crown, with a log line as its only symptom.

## The cause

The fork redefined the depth items generate at. `ItemsGetCurrlevel` returns the **area level** rather
than the floor number (`items.cpp:534`), and `oracool::AreaLevel` adds 16 per difficulty block. The
validator was never taught that:

- **Sarcophagus.** `OperateSarcophagus` → `CreateRndItem` → `SetupBaseItem`, which generates at
  `2 * ItemsGetCurrlevel()`. Nightmare floor 3 → `2 * (3 + 16) = 38`, against `IsDungeonItemValid`'s
  vanilla ceiling of **30**. Rejected.
- **Skeleton King.** `MT_SKING` carries `Uniq(UITEM_SKCROWN)`, so the drop goes through `SpawnUnique`
  — which is **explicitly difficulty-branched** (`items.cpp:4674`). On Normal it leaves
  `_iCreateInfo` at 0, which validates trivially. Above Normal it calls `SetupAllItems` with
  `curlv * 2` and `uper = 15`, so the crown carries `38 | CF_UPER15` and routes to
  `IsUniqueMonsterItemValid` — which requires an **exact match** against some unique monster's base
  mlvl and has no ceiling to fall back on. No unique monster is level 38. Rejected.

Which is exactly why Normal was fine and Nightmare was not, and why ordinary monster drops never
misfired: those use the monster's own vanilla `level`, which by construction matches a
`MonstersData` entry.

## The fix, and the fix I backed out of

`IsItemValid` (`items/validation.cpp:172`) has begun with `if (!gbIsMultiplayer) return true;` all
along. **`IsPItemValid` never got one**, and that asymmetry is the whole bug: the validation is
anti-cheat for packets from a remote player, and in single player the only sender is this process,
one function call earlier. Every rejection was a false positive. That early-out is the fix
(`msg.cpp:1029`), placed so `InDungeonBounds` and `IsItemAvailable` still apply.

**I first also widened both ceilings** to the fork's ladder, on the reasoning that the rules were
wrong and should be fixed at the root. Two tests said otherwise:
`NetPackTest.UnPackNetPlayer_invalid_uniqueMonsterItemLevel` and `..._invalid_monsterItemLevel`.

They were right and I was wrong. Those two functions have **two callers with different needs**:
`IsPItemValid` validates a locally generated drop, but `UnPackNetPlayer` validates a **remote
player's packed equipment**, where an ilvl matching no monster genuinely is a forgery. Loosening the
shared rule would have fixed a false positive on one path by deleting a real check on the other. So
the ceilings are back to vanilla, carrying comments that say what they do not cover and why they are
left alone.

The lesson is the one the tests enforced: "the rule is wrong" and "the rule is being asked the wrong
question" look identical from one caller.

## Tests

- `WouldSurviveNetworkValidation.RejectsDungeonLevelBeyondFallbackCeilingWithNoMonsterMatch` —
  restored unchanged after my detour.
- `WouldSurviveNetworkValidation.TheForkOwnDropLevelsAreStillRejectedByTheVanillaRules` — new, and
  it pins the bug's **premise** rather than its symptom: ilvl 34 and ilvl 38 are still rejected by
  the rules, and the same sarcophagus on Normal is still accepted. If one of those ever flips, the
  early-out is no longer the only thing between the player and silent item loss, and this needs
  revisiting rather than explaining away.

`WouldSurviveNetworkValidation` itself deliberately still mirrors the rules rather than the
early-out, so in single player it is now pessimistic rather than wrong — it can only make the debug
`drop` command reroll a level that would in fact have been accepted.

**723/723 tests pass.** Debug and Release build clean; RTM refreshed. No asset changed.

## What to look for in play

Kill the Skeleton King on Nightmare again — no message, and the crown should still be on the floor
after leaving the level and returning. Same for a sarcophagus, a chest, and any floor drop above
Normal.

Worth knowing: **items already lost this way are not recoverable.** They were never written to a
delta. Anything that vanished from a floor on Nightmare or above before this build is gone.
