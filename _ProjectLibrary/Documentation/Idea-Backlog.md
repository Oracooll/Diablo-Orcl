# Diablo Oracool Edition — Idea Backlog

A running list of ideas and concepts floated during development but not being built right now. This is a holding pen, not a commitment — nothing here has design decisions locked in, and nothing here is scheduled. When the user wants to pick one up, it gets its own design pass (open questions listed and answered, then implemented) the same way Rare/Buffed Unique/Primal items and Torment difficulty were, and moves out of this file into `Gameplay-Changes.md`/`CHANGELOG.md` as its own dated entry.

New ideas get appended here as they're floated, without waiting for a request to record them. Nothing gets removed except by moving it to "built" (with a link/date) or by the user saying to drop it.

---

## Diablo 2/3-style systems (floated 2026-08-05, not started)

- **Set items** — items that grant bonus effects when multiple pieces of the same set are equipped together.
- **Salvaging** — breaking down unwanted items into materials.
- **Crafting** — using salvaged materials (or other resources) to create or upgrade items.
- **Skills / skill trees / synergies** — a Diablo 2-style skill tree with per-skill investment and cross-skill synergy bonuses, as opposed to vanilla Diablo 1's simpler spell-book/spell-level system.
- **Autosave-only play** — remove manual save entirely, relying solely on the existing autosave system. Currently both coexist (manual "Save Game" is still in the ESC menu after the v0.3.31 fix).
- **Run/movement-speed system** — a movement speed stat (walk vs. run), plus item affixes that grant increased movement speed.

---

## Per-affix "perfect roll" indicator (floated 2026-08-06, not started)

- Show players when an individual affix on a Rare/Buffed Unique item rolled at its maximum possible value (not just the whole-item Primal case, which already forces every affix to max). Leading idea: color that specific affix line gold, possibly paired with a text marker like "(perfect)" for accessibility.
- Requires a new per-affix flag on `OracoolAffix` (items.h) — the struct currently only stores `type`/`param1`/`param2`, not the roll range, so "was this roll the max" can't be reliably reconstructed at display time (multiple `PLStruct` rows can share the same `item_effect_type` with different ranges, and only the final post-scaling value is stored).
- Adding the flag grows the per-item save record (`SaveItem`/`LoadItemData` in loadsave.cpp), so it needs an `OracoolItemFormatVersion` bump — same pattern as when `_iOracoolBroken` was added, and the same lesson learned from the Stash-corruption bug (version bumps must stay in sync or old saves get silently misread instead of cleanly rejected). Breaks compatibility with existing saves/characters; consistent with the user's standing policy of accepting save breaks for simpler/more robust code, as long as they're warned first.

---

## How to use this file

- Every idea gets one bullet (or a short subsection if it has more than a sentence of context) with the date it was floated.
- Group loosely related ideas under a shared heading if they arrived together, like the batch above.
- When an idea is picked up for real development, move its entry to a "Built" section at the bottom (or just delete it) with a link to the shipping `OE-###`/version, so this file only ever shows what's still actually pending.
