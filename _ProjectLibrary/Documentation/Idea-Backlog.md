# Diablo Oracool Edition — Idea Backlog

A running list of ideas and concepts floated during development but not being built right now. This is a holding pen, not a commitment — nothing here has design decisions locked in, and nothing here is scheduled. When the user wants to pick one up, it gets its own design pass (open questions listed and answered, then implemented) the same way Rare/Buffed Unique/Primal items and Torment difficulty were, and moves out of this file into `Gameplay-Changes.md`/`CHANGELOG.md` as its own dated entry.

New ideas get appended here as they're floated, without waiting for a request to record them. Nothing gets removed except by moving it to "built" (with a link/date) or by the user saying to drop it.

Ideas are split below into **non-save-breaking** and **save-breaking**, per the user's direction (2026-08-06): push the non-save-breaking ones first, then batch all save-breaking ones together in one later pass rather than breaking saves repeatedly. The split is a current best guess based on the project's established save-format patterns (see "How the split is decided" below) — it can shift once a real design pass locks in specifics for a given idea.

---

## Non-save-breaking (push these first)

### Autosave-only play (floated 2026-08-05, not started)

Remove manual save entirely, relying solely on the existing autosave system. Currently both coexist (manual "Save Game" is still in the ESC menu after the v0.3.31 fix). Pure behavior/UI change — no new persistent data, so no save-format impact either way.

### Run/movement-speed system (floated 2026-08-05, not started)

A movement speed stat (walk vs. run), plus item affixes that grant increased movement speed. The affix itself can reuse the existing `OracoolAffix` fields (just a new `item_effect_type` enum value, which doesn't grow the struct); the walk/run state itself is a live, derived gameplay toggle, not something that needs to persist across a save/load.

### Salvaging (floated 2026-08-05, not started)

Breaking down unwanted items into materials. The new persistent state here (a materials count) is player-scoped, not per-item, so it can live in its own new, absent-tolerant save file — same additive pattern already proven safe for `heroinvtabs` (a save from before this feature simply has zero materials, nothing rejected).

### Crafting (floated 2026-08-05, not started)

Using salvaged materials (or other resources) to create or upgrade items. As long as "upgrade" means rerolling/replacing values already representable in the existing `OracoolAffix` fields (no new per-item data), this stays non-breaking for the same reason Salvaging does. Depends on Salvaging for its material source.

### Skills / skill trees / synergies (floated 2026-08-05, not started)

A Diablo 2-style skill tree with per-skill investment and cross-skill synergy bonuses, as opposed to vanilla Diablo 1's simpler spell-book/spell-level system. Per-skill point allocation is player-scoped data, not per-item — like `heroinvtabs`, it can be its own new, absent-tolerant save file (a save from before this feature simply has no points invested). This avoids the per-item, per-container sync fragility that was the actual reason the old item-tier sidecar got merged into the core record in the v0.2.0 Foundations Pass — that lesson is about item data living in multiple containers (backpack/belt/tabs/stash/ground), which doesn't apply to a single player-level skill sheet.

---

## Save-breaking (batch together for one later pass)

### Set items (floated 2026-08-05, not started)

Items that grant bonus effects when multiple pieces of the same set are equipped together. Needs a new piece of data intrinsic to the item itself (which set it belongs to), generated at drop time — the same category of data `OracoolItemTier`/`OracoolAffix` are. Consistent with the project's post-v0.2.0 policy of growing the core per-item record instead of reintroducing a per-container sidecar (which is what caused the tier-data bugs the Foundations Pass fixed), this would need an `OracoolItemFormatVersion` bump, breaking compatibility with existing saves.

### Per-affix "perfect roll" indicator (floated 2026-08-06, not started)

Show players when an individual affix on a Rare/Buffed Unique item rolled at its maximum possible value (not just the whole-item Primal case, which already forces every affix to max). Leading idea: color that specific affix line gold, possibly paired with a text marker like "(perfect)" for accessibility.

- Requires a new per-affix flag on `OracoolAffix` (items.h) — the struct currently only stores `type`/`param1`/`param2`, not the roll range, so "was this roll the max" can't be reliably reconstructed at display time (multiple `PLStruct` rows can share the same `item_effect_type` with different ranges, and only the final post-scaling value is stored).
- Adding the flag grows the per-item save record (`SaveItem`/`LoadItemData` in loadsave.cpp), so it needs an `OracoolItemFormatVersion` bump — same pattern as when `_iOracoolBroken` was added, and the same lesson learned from the Stash-corruption bug (version bumps must stay in sync or old saves get silently misread instead of cleanly rejected). Breaks compatibility with existing saves/characters; consistent with the user's standing policy of accepting save breaks for simpler/more robust code, as long as they're warned first.

---

## How the split is decided

The dividing line is whether the new data can live in its own new, separately-versioned, **absent-tolerant** save file — old saves just don't have it yet and default to empty/zero, same as `heroinvtabs` today (see `AbsentInvTabsFileLeavesTabsEmpty`) — versus needing to grow the existing fixed-size **per-item** record (`SaveItem`/`LoadItemData`), which forces an `OracoolItemFormatVersion` bump that cleanly *rejects* older saves rather than risk silently misreading them.

- **Player-scoped** new data (a materials count, skill points invested) fits the first case: safe as its own additive file, same pattern as Tabbed Inventory's own save file.
- **Item-scoped** new data (a set ID, a per-affix flag) fits the second case: it's the same category of data `OracoolItemTier`/`OracoolAffix` already are, which the project deliberately consolidated into the core per-item record in the v0.2.0 Foundations Pass specifically to avoid the per-container-sync bugs a separate item-data sidecar caused. Reusing that sidecar approach for new item data would work technically, but would reintroduce exactly the bug class that pass was built to eliminate — so the save-breaking path (grow the core record) is the one actually worth taking for new item data going forward.

## How to use this file

- Every idea gets one bullet (or a short subsection if it has more than a sentence of context) with the date it was floated.
- Group loosely related ideas under a shared heading if they arrived together, like the batch above.
- New ideas go under **Non-save-breaking** or **Save-breaking** per the reasoning above; if it's genuinely unclear until a real design pass happens, say so in the entry rather than guessing.
- When an idea is picked up for real development, move its entry to a "Built" section at the bottom (or just delete it) with a link to the shipping `OE-###`/version, so this file only ever shows what's still actually pending.
