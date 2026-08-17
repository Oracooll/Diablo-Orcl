# Audit: The Duplicated Ring

**Version:** 1.7.67
**Date:** 2026-08-17
**Scope:** everything shipped fast this session that no test pinned — the refund path, the F-key intercept, the set-completion edge, the unique selection walk, the generator's value packing, the sound teardown.

Run while the user was away, per instruction. One real defect found and fixed; five suspicions investigated and cleared, written down so they stay cleared.

---

## FIXED: two copies of one set piece counted as two pieces

`WornSetPieces` counted equipped *items* belonging to the set. A player wearing **two copies of the same set ring** — one in each ring slot — counted 2 toward completion. On a set with one item missing, that reads as complete: the ladder's top rung lights, the cumulative bonuses grant, and the completion bell rings, for a set the player does not own. Leoric's Fallen Court carries two distinct rings, so its 13-piece ladder was reachable at 12 real pieces plus a duplicate.

The fix walks the set's own piece definitions asking "is *this* piece worn" (`IsSetPieceWorn`, cursor identity), which cannot double-count — a piece is worn or it is not, however many copies are equipped. Same complexity, one loop either way.

Pinned by `OracoolItemSets.DuplicateSetPieceCountsOnce`: one ring worn = 1, the same ring worn twice = still 1.

Reachability today: debug set grants plus manual duplication — no set drops exist yet. But the drop system is planned, and this was exactly the kind of edge that would have survived into it unnoticed.

---

## CLEARED, with reasons

**`CheckUnique`'s `break` on `IsUniqueAvailable`.** The suspicion: the walk stops at the first unavailable unique, and `IsUniqueAvailable` is `gbIsHellfire || i <= 89` — under Diablo mode every expansion unique (index ≥ 90) would be unreachable *and* would stop the scan. Cleared because this fork forces `gbIsHellfire = true` at init; the predicate is constant-true in every configuration V1 can run. Worth re-checking only if a Diablo-mode toggle ever returns.

**Refund-to-zero on a readied ability.** The minus button can empty a skill that is readied on LMB/RMB or bound to an F-key. Cleared: spell-typed actives at level 0 hit the engine's own `Fail_Level0` gate in `CheckSpell` — the established "you cannot cast this" path, with feedback. The Paladin melee actives (Zeal, Shield Bash…) predate the tree and work at base strength with zero investment *by design* — investment is their scaling, not their existence. The aura case was already handled at refund time (an empty aura is put out). No dangling cast path found.

**Negative "downside" affixes vs the generator's clamps.** The expansion's downsides are `light_radius` (×32), `durability_percent` (×9), `mana` (×8), `life` (×8) — all channels that take negative parameters natively (`IPL_LIGHT`, `IPL_DUR`, `IPL_MANA`, `IPL_LIFE` all add signed values). Confirmed **no negative value reaches a clamped token** (`attack_speed`, `hit_recovery`, the steals) anywhere in the 250 — the case where my `Max(1, …)` clamp would have turned a penalty into a bonus does not occur in the data. The generator would deserve a guard if a future batch changes that; noted in its comments.

**The F-key intercept vs text entry.** F1–F6 are intercepted after `control_presskeys` (chat capture) and the gold-entry/withdraw handlers, so typing "f1" into chat or a gold field is unaffected. Dead players skip the intercept (the branch is on the alive path), and the keymapper never sees the keys — which is the point.

**Aura loop teardown.** `FreeGame` silences the loop and resets the completion baseline; `LoadGameLevel` releases and re-attaches around the transition. No path found that leaves a loop handle alive across a character change.

**Unique selection determinism.** Among multiple qualifying uniques on one base at one level, the walk picks the highest qualifying index deterministically — variety comes from the vanilla dupe flags (`UniqueItemFlags`), not from the roll. This is vanilla's own semantics, unchanged; the expansion shadowing vanilla uniques within it is the displacement the user already accepted.

---

## Verification

Debug build clean at 1.7.67; full suite **442 of 444** (one test added this audit) — the two failures are the standing baseline pair.

---

## Round two (v1.7.68)

**FIXED: dead F-key bindings.** `HandleAbilityFKey` stored `GetSBookTrans`'s verdict as the
binding's `SpellType` — and that function folds *momentary* castability into its answer. A
memorized spell bound while mana happened to be short came back `SpellType::Invalid`; the binding
stored it, and the F-key stayed dead after the mana returned, wearing its badge the whole time.
The stored type is now the spell's identity alone (innate Skill / memorized Spell). A binding
outlives the moment it was made in.

**FIXED: the phantom tooltip line.** `PrintItemPower` renders `IPL_INVCURS` as a lone space —
tolerable on vanilla's few icon-carrying uniques, a blank last line on all 143 expansion uniques
once each carried its icon that way. The description loop skips the icon assignment; it was never
a stat.

**VERIFIED, unchanged:** Griswold's unique-shop option scans the whole table, so the 143 are
purchasable there with sprites and band-derived prices — first-class uniques, consistent with the
accepted displacement. Suite 442/444.
