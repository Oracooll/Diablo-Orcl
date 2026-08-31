# Diablo Oracool Edition — Idea Backlog

A running list of ideas and concepts floated during development but not being built right now. This is a holding pen, not a commitment — nothing here has design decisions locked in, and nothing here is scheduled. When the user wants to pick one up, it gets its own design pass (open questions listed and answered, then implemented) the same way Rare/Buffed Unique/Primal items and Torment difficulty were, and moves out of this file into `Gameplay-Changes.md`/`CHANGELOG.md` as its own dated entry.

New ideas get appended here as they're floated, without waiting for a request to record them. Nothing gets removed except by moving it to "built" (with a link/date) or by the user saying to drop it.

**Ordering (as of 2026-08-09):** ideas are arranged in ascending order of development difficulty, easiest first — per the user's direction, since the list has grown long enough to need a build-order lens. Each entry carries a `Difficulty` tag (Trivial / Easy / Medium / Hard / Very Hard) and a `Save impact` tag (Non-save-breaking / Save-breaking). The save-impact classification itself follows the reasoning in "How the split is decided" below, and the user's earlier 2026-08-06 direction — batch all save-breaking work together in one pass rather than breaking saves repeatedly — still applies whenever items are actually scheduled; it's just no longer the primary sort key for this file. Difficulty estimates are a current best guess from each idea's own description, not a locked-in estimate — they can shift once a real design pass locks in specifics.

---

## Tier 1 — Trivial

Empty — both entries formerly here (Tristram NPC repositioning, Stash relocation near the town well) are done; see the Built section.

---

## Tier 2 — Easy

### Resistance penetration / soft cap (floated 2026-08-09, not started)

**Difficulty:** Easy · **Save impact:** Non-save-breaking

Diablo 2-style diminishing returns on elemental resistances: a soft cap (e.g. 75%) beyond which further resistance gives sharply reduced benefit, plus higher difficulties (Nightmare/Hell) applying a flat resistance penalty to the player, making resistance gear matter more at end-game instead of being trivially maxed out early. Pure formula change against the existing Fire/Lightning/Magic/All resistance stats (`IPL_FIRERES` etc., `itemdat.h`) — no new persistent data, so no save-format impact.

### Health Globes (floated 2026-08-09, not started)

**Difficulty:** Easy · **Save impact:** Non-save-breaking

Diablo 3-style: monsters have a chance to drop a globe on death that heals HP (and optionally restores mana) when walked over, shifting part of the healing loop from menu-driven potion sipping toward an in-combat pickup. No new persistent state — a drop-table addition and a pickup handler, same category of change as existing loot drops.

### Gambling (floated 2026-08-09, not started)

**Difficulty:** Easy · **Save impact:** Non-save-breaking

Diablo 2-style: spend gold at a vendor for a random unidentified item, rather than only ever buying known/fixed shop stock. Reuses the same item-generation pipeline the extensively-modified Griswold shop (`v0.4`–`v0.20` in the Feature Catalogue) already exercises — likely just a new shop screen/action, no new item-intrinsic fields.

### Run/movement-speed system (floated 2026-08-05, not started)

**Difficulty:** Easy · **Save impact:** Non-save-breaking

A movement speed stat (walk vs. run), plus item affixes that grant increased movement speed. The affix itself can reuse the existing `OracoolAffix` fields (just a new `item_effect_type` enum value, which doesn't grow the struct); the walk/run state itself is a live, derived gameplay toggle, not something that needs to persist across a save/load.

### Elective Mode — free skill-to-hotkey binding (floated 2026-08-09, not started)

**Difficulty:** Easy · **Save impact:** Non-save-breaking

Diablo 3-style: let the player bind any known spell/skill to any hotkey slot, instead of the fixed slot assignment vanilla Diablo 1 uses. Pure input/UI behavior change against the existing hotkey-to-spell mapping already persisted per character — no new save fields expected, same category as the Autosave-only play and HUD rearrangement ideas.

### Enchanting — single-affix reroll (floated 2026-08-09, not started)

**Difficulty:** Easy (for the minimal version — see caveat) · **Save impact:** Non-save-breaking

Diablo 3 Mystic-style: pick one existing affix on a Rare/Buffed Unique item and reroll just that affix's value (or, in a later pass, reroll which affix it is) at a vendor or shrine. Scoped to a numeric-only reroll of values already representable in the existing `OracoolAffix` fields, this is the same "stays non-breaking if kept minimal" case already noted under the Crafting entry — same caveat applies if a richer version (tracking which affix was enchanted, limiting rerolls per item) gets designed instead, which would push both the difficulty and save-impact classification up.

---

## Tier 3 — Medium

### Charms (floated 2026-08-09, not started)

**Difficulty:** Medium · **Save impact:** Save-breaking (tentative — see caveat)

Diablo 2-style: small items that grant passive stat bonuses simply by occupying inventory space, rather than being equipped in a gear slot — a power-vs-inventory-space tradeoff distinct from anything currently in the game. Likely representable with the existing item/affix save structure (a Charm is just an item type living in the backpack), which would keep it non-save-breaking — but tentatively bucketed here since "does it need a new persistent flag distinguishing it from a normal item at a glance" is an open question for a real design pass, and the project's classification framework defaults to save-breaking when unsure rather than assuming the shortcut.

### Salvaging (floated 2026-08-05, re-confirmed 2026-08-09, not started)

**Difficulty:** Medium · **Save impact:** Non-save-breaking

Breaking down unwanted items into materials. The new persistent state here (a materials count) is player-scoped, not per-item, so it can live in its own new, absent-tolerant save file — same additive pattern already proven safe for `heroinvtabs` (a save from before this feature simply has zero materials, nothing rejected). Feeds the Levski's Cube idea as a material source.

### Paragon-style post-cap progression (floated 2026-08-09, not started)

**Difficulty:** Medium · **Save impact:** Non-save-breaking

Diablo 3-style: once a character hits the level cap, further experience earned converts into small, incremental permanent stat bonuses ("Paragon points") instead of being wasted, keeping XP gain meaningful at endgame. The new state (points earned/spent past the cap) is player-scoped, not per-item — fits the same "own new, absent-tolerant save file" pattern as the Skills and Salvaging entries.

### Per-affix "perfect roll" indicator (floated 2026-08-06, not started)

**Difficulty:** Medium · **Save impact:** Save-breaking

Show players when an individual affix on a Rare/Buffed Unique item rolled at its maximum possible value (not just the whole-item Primal case, which already forces every affix to max). Leading idea: color that specific affix line gold, possibly paired with a text marker like "(perfect)" for accessibility.

- Requires a new per-affix flag on `OracoolAffix` (items.h) — the struct currently only stores `type`/`param1`/`param2`, not the roll range, so "was this roll the max" can't be reliably reconstructed at display time (multiple `PLStruct` rows can share the same `item_effect_type` with different ranges, and only the final post-scaling value is stored).
- Adding the flag grows the per-item save record (`SaveItem`/`LoadItemData` in loadsave.cpp), so it needs an `OracoolItemFormatVersion` bump — same pattern as when `_iOracoolBroken` was added, and the same lesson learned from the Stash-corruption bug (version bumps must stay in sync or old saves get silently misread instead of cleanly rejected). Breaks compatibility with existing saves/characters; consistent with the user's standing policy of accepting save breaks for simpler/more robust code, as long as they're warned first.

### Elite monster affixes (floated 2026-08-09, not started)

**Difficulty:** Medium · **Save impact:** Non-save-breaking

Diablo 3-style: on higher difficulties, a chance for a monster pack to spawn with one or two random combat-modifying affixes (e.g. Fast, Extra Health, a damaging ground trail, a periodic pulse, an arcane-sentry-style hazard) layered on top of its normal behavior, adding tactical variety to random encounters without needing new zones or monster types. Purely a spawn-time modifier applied when a level generates or monsters spawn, same category as the existing `Unique Item Drop Multiplier` — no persistent per-monster save state implied. Difficulty scales up with how many distinct affix behaviors get designed in a first pass; rated Medium for a small initial set (2-3 affixes).

### Transmogrification (floated 2026-08-09, not started)

**Difficulty:** Medium · **Save impact:** Save-breaking

Diablo 3-style: let the player change an equipped item's *displayed* appearance to any other item they've collected, without changing its actual stats. Needs a new per-item field (which appearance ID it displays as) on top of the existing per-item record — same category of item-intrinsic data as `OracoolItemTier`/`OracoolAffix`, so an `OracoolItemFormatVersion` bump.

---

## Tier 4 — Hard

### Diablo 2-style shop interface (floated 2026-08-09, not started)

**Difficulty:** Hard · **Save impact:** Non-save-breaking

Rework the shop screens (Griswold, Pepin, Adria, Wirt, Cain) to visually resemble Diablo 2's shop UI, layered on top of the extensive existing shop logic customizations already shipped (`v0.4`–`v0.20` in the Feature Catalogue — Premium Refresh, Buy All Items, Sell Consumables, Recharge Staves, the Unique Shop, etc.). Pure UI/rendering rework against existing shop data and purchase logic — no new persistent fields expected, but a significant art/UI-code scope given how many shop variants and states (Premium stock, Unique Shop, consumables) already exist to re-skin consistently. Should be designed alongside the HUD/UI rearrangement entry rather than in isolation, so the whole game shares one visual language.

### Item Tiers, Diablo 2-style (Normal/Exceptional/Elite) (floated 2026-08-08, not started)

**Difficulty:** Hard · **Save impact:** Save-breaking

Add three tiers to basic weapons and armor, D2-style: Normal (today's baseline), Exceptional, and Elite. Exceptional drops on Nightmare and up; Elite drops on Hell and up. Exceptional doubles the base item's damage/armor spec; Elite triples it. Both new tiers can still roll as magic, rare, buffed unique, or Primal on top of the base tier boost. STR/Magic/Dex requirements scale with tier too: +50% for Exceptional, +100% for Elite. Proposed naming: append `(II)` to the base item name for Exceptional, `(IIII)` for Elite, so the tier is visible on the drop/tooltip without a new color.

Open design notes from the first discussion pass (not yet resolved):

- **Naming inconsistency**: `(II)` → `(IIII)` skips `(III)` and doesn't read as a clean progression at a glance. Worth picking a scheme where the visual jump from tier to tier is more legible (e.g. `(II)`/`(III)`, or a bracketed tier word).
- **Open question — real Uniques**: do vanilla/Oracool Unique items (the fixed hand-picked ones, not the Buffed Unique tier) also get Exceptional/Elite base-stat versions, or are Uniques exempt and only the generic magic/rare/buffed-unique/Primal rolls scale?
- **Open question — drop mechanic**: should Exceptional/Elite be a guaranteed replacement once the difficulty threshold is hit (all qualifying drops from Nightmare+ are automatically Exceptional-or-better), or a percentage chance layered on top of the normal drop table on those difficulties?
- **No new UI color suggested** — the `(II)`/`(IIII)` suffix is meant to carry the distinction textually rather than needing a new `UiFlags` color bit, unlike Rare/Buffed Unique/Primal which do use color.
- **Testing caution**: appending a tier suffix to already-long item names (especially ones that already carry prefix/suffix affix text) risks overflowing the inventory tooltip/name-display width — needs a check against the longest existing base item name once this is built.

Needs a new piece of data intrinsic to the item itself (which base tier it rolled), so it survives save/load — same category as `OracoolItemTier`, landing in the save-breaking bucket per the project's established classification framework.

### Set items (floated 2026-08-05, not started)

**Difficulty:** Hard · **Save impact:** Save-breaking

Items that grant bonus effects when multiple pieces of the same set are equipped together. Needs a new piece of data intrinsic to the item itself (which set it belongs to), generated at drop time — the same category of data `OracoolItemTier`/`OracoolAffix` are. Consistent with the project's post-v0.2.0 policy of growing the core per-item record instead of reintroducing a per-container sidecar (which is what caused the tier-data bugs the Foundations Pass fixed), this would need an `OracoolItemFormatVersion` bump, breaking compatibility with existing saves.

### Crafting (floated 2026-08-05, not started)

**Difficulty:** Hard · **Save impact:** Save-breaking

Using salvaged materials (or other resources) to create or upgrade items. A minimal version — pure numeric rerolls of values already representable in the existing `OracoolAffix` fields — could theoretically stay non-breaking, but a genuinely robust crafting system (tracking which affixes were crafted-in vs. rolled, socket counts, upgrade tiers, or anything else worth building cleanly rather than hacking around) is realistically going to want new data intrinsic to the item itself, same category as the Set items entry. Classified save-breaking on that basis rather than assuming the minimal-shortcut design. Depends on Salvaging for its material source.

### Levski's Cube — combine/crafting hub (floated 2026-08-09, not started)

**Difficulty:** Hard · **Save impact:** Save-breaking (tentative — see caveat)

Oracool's own take on Diablo 2's Horadric Cube: a dedicated combine screen where specific known item/material combinations (e.g. 3 identical gems + 1 base item, or salvaged materials + a base item) produce a specific known result. Intended as the actual interaction point for the Crafting and Salvaging entries — Salvaging supplies the materials, Crafting is the transformation, Levski's Cube is where the player performs it — rather than a fourth, unrelated mechanic. Open question for a design pass: whether recipe inputs are consumed from the existing backpack/belt directly (likely non-save-breaking) or need their own dedicated staging container (which would need new persistent state, same category as Tabbed Inventory's own save file). Depends on both Crafting and Salvaging existing.

### Socketed items, gems, and jewels (floated 2026-08-09, not started)

**Difficulty:** Hard · **Save impact:** Save-breaking

Diablo 2-style base mechanic: weapons and armor can roll with a number of empty sockets, into which the player can place gems, jewels, or (once the Runewords idea exists) specific rune sequences for bonus effects. This is genuinely new ground for the engine — DevilutionX/Oracool currently has no socket concept at all (`Source/items.h` has no socket fields). Needs new item-intrinsic data (socket count, contents per socket) on top of the existing per-item record, so it's an `OracoolItemFormatVersion` bump like the Set Items/Legendary Powers entries. Foundational for the Runewords entry.

### Runewords (floated 2026-08-09, not started)

**Difficulty:** Hard · **Save impact:** Save-breaking

Diablo 2-style: placing a specific ordered sequence of runes into a fully-socketed item unlocks a powerful, named combined effect (distinct from the sum of the individual runes' own bonuses). Depends entirely on the Socketed items entry existing first. Needs the rune sequence stored per item (or derivable from socket contents) — same item-intrinsic category as Socketed items, so no additional format-version bump beyond that feature's if designed together.

### HUD/UI rearrangement for the 960x720 canvas (floated 2026-08-08 as "1024x768 minimum resolution", corrected 2026-08-09, design confirmed 2026-08-10, **Phase 1 structural skeleton built 2026-08-10, v1.0.55**)

**Difficulty:** Hard · **Save impact:** Non-save-breaking (core layout) — the LMB/RMB skill-slot mechanic below may need its own small persisted binding, see its own note

**Correction (2026-08-09):** this entry originally proposed raising the minimum supported resolution to 1024x768. The project has since shipped its actual curated resolution list with a 960x720 floor (see [[2026-08-09 - Resolution List Investigation]] and [[2026-08-09 - Resolution List Sort Order]]), and the 2026-08-09 request for this HUD rework explicitly says "the new canvas of 960x720." Design target corrected to 960x720 to match what's actually shipped, not the earlier 1024x768 assumption.

#### Current-state research (2026-08-10)

Before redesigning, the existing layout was mapped precisely (file:line references kept in session history, not repeated here). Two findings that shape the redesign:

- **The control panel doesn't currently reserve viewport space** on any canvas ≥640px wide (`CalculatePanelAreas()`, `Source/control.cpp`) - it's a fixed 640px-wide block, centered, floating on top of the full-height 3D view. This is *why* the new design corner-locks the orbs independently instead of keeping one centered block: at 960/1280 width, a fixed 640px panel leaves dead space on both sides and never reaches the true screen edges.
- **No clickable mini-map controls exist today** - panning/zooming/recentering the mini-map is 100% keybind/Alt+modifier driven (Alt+arrows, Alt+wheel, Alt+backtick). Folding these into the new Menu popup (see below) means *adding* real buttons for them, not relocating existing ones.
- No cursor-following tooltip exists anywhere in the codebase yet; the closest reusable pattern is the floating item-name-label system (`Source/qol/itemlabels.cpp`) - translucent box, anti-overlap stacking, and hover hit-testing already built, just anchored to item world-position instead of the cursor.
- New art assets go in `Packaging/resources/assets/` (overrides `devilutionx.mpq`, loaded before the original game data), as **CLX** format - not the `Oracoo.MPQ/assets/ui/hud/` path an earlier version of this entry guessed at, which doesn't exist in this repo.

#### Design (2026-08-10, user confirmed)

Three independently-anchored pieces replace the single centered panel:

1. **Health Orb** - locked to the bottom-left screen corner, independent of resolution/canvas width.
2. **Mana Orb** - locked to the bottom-right screen corner, same independence.
3. **Middle HUD** - centered cluster containing, left to right:
   - **Left skill button** - the player's LMB (left-click) action.
   - **6-slot belt**, redesigned from vanilla's 8:
     - Slot 1: **Menu button** (not an item slot) - opens a single popup containing everything that used to be a separate button: Char, Quests, Map, system Menu, Inventory, Spellbook, the chat toggle, the friendly-fire toggle, and the new mini-map pan/zoom/recenter controls. Confirmed as the complete list - nothing else keeps its own dedicated persistent button.
     - Slots 2-5: general potion/scroll slots (4, not 8 - each already stacks to 99, so fewer slots hold the same practical capacity).
     - Slot 6: **permanent Town Portal**, always available, free to cast - a dedicated belt slot for the existing `Permanent Free Town Portal` option (`v0.18`, Feature Catalogue), matching Diablo 3's always-on Town Portal skill treatment instead of leaving it as a memorized-but-invisible spell.
   - **Right skill button** - the player's RMB (right-click) action.
   - **XP bar sits directly below the skill+belt row, spanning its full width** - both skill buttons plus the belt between them, not just the belt alone. Confirmed against a mockup (2026-08-10).
4. The black description/info box is removed entirely, replaced by a **cursor-following tooltip**.

**LMB/RMB skill-slot mechanic:** the two skill buttons are a new interaction layer, built ahead of the full Skills/skill-trees system (Very Hard, elsewhere in this backlog) - "we will introduce skills later, for now they can be used for spells" (user, 2026-08-10). Confirmed behavior matches Diablo 2's own model exactly: clicking a monster with a spell assigned to LMB casts it; clicking open ground still walks there; item pickup and NPC dialogue stay on left-click as today. This is more than a HUD change - it's a genuinely new control-scheme layer (left-click can now cast, which it never does in vanilla), so implementation will need its own small design pass on top of layout: what UI lets the player *choose* which spell occupies each slot (a radial picker like D2? reusing the spellbook?), and whether "which spell is bound to LMB vs RMB" needs to persist per character (likely yes, small player-scoped save addition, same non-save-breaking category as the Waypoints unlock table).

- **Mockup confirmed (2026-08-10):** the user signed off on the three-piece concept (corner-locked orbs, centered skill+belt+XP-bar cluster, cursor-following tooltip) as the direction to build toward.
- **Phase 1 built (2026-08-10, v1.0.55):** the full structural skeleton is implemented with placeholder art, per the user's "implement raw, replace assets gradually" instruction - see [[2026-08-10 - HUD Overhaul Phase 1 - Structural Skeleton]]. Corner-locked orbs, 6-slot belt (Menu popup with all 10 consolidated actions, 4 item slots on hotkeys 1-4, SP-only Town Portal button), relocated RMB speedbook slot, inert LMB placeholder, XP bar under the row, cursor-following tooltip, old panel/buttons/info-box code deleted. Belt-slot migration protects older test saves. Save format untouched. Awaiting the user's manual play-test.
- **Still open (later phases):** replacing placeholder visuals with real art assets (`Packaging/resources/assets/`, CLX); the LMB/RMB spell-assignment-and-cast mechanic (see paragraph above - deliberately excluded from Phase 1); tooltip sizing-to-text and visual polish; exact pixel positions are tunable constants in `Source/oracool/hud_layout.cpp`.

---

## Tier 5 — Very Hard

### Skills / skill trees / synergies (floated 2026-08-05, re-confirmed 2026-08-09, not started)

**Difficulty:** Very Hard · **Save impact:** Non-save-breaking

A Diablo 2-style skill tree with per-skill investment and cross-skill synergy bonuses, as opposed to vanilla Diablo 1's simpler spell-book/spell-level system. Per-skill point allocation is player-scoped data, not per-item — like `heroinvtabs`, it can be its own new, absent-tolerant save file (a save from before this feature simply has no points invested). This avoids the per-item, per-container sync fragility that was the actual reason the old item-tier sidecar got merged into the core record in the v0.2.0 Foundations Pass — that lesson is about item data living in multiple containers (backpack/belt/tabs/stash/ground), which doesn't apply to a single player-level skill sheet.

Re-confirmed 2026-08-09 as "we will introduce skills for each character" — implies each playable class (including any new ones, see the Barbarian entry) gets its own tree, not one shared tree, which should be an explicit open question for the eventual design pass. Rated Very Hard despite the non-save-breaking classification because of the sheer system size: tree UI, per-skill point allocation, and synergy-formula design across every class.

### Barbarian class, cloned from the Warrior sprite (floated 2026-08-09, not started)

**Difficulty:** Very Hard · **Save impact:** Non-save-breaking (the class-slot addition itself — see caveat)

Add a new playable class, Barbarian, using the Warrior's existing sprite/animation set as placeholder art, with its own ability kit to be decided. Adding a new value to an existing class enum is typically non-save-breaking on its own (old saves already store one of the pre-existing class values and are unaffected by a new one being added) — but this idea is genuinely multi-part and shouldn't be force-classified as a whole:

- **Depends on the Skills / skill trees idea** for what the Barbarian's own tree/abilities actually are — there's no ability kit to design until that system's shape is decided.
- **Starting stats/equipment loadout** for a new class is ordinary character-creation data, same pattern as the three existing classes.
- **Placeholder art now, real art later**: cloning the Warrior sprite is explicitly a shortcut to unblock design/mechanics work before committing to new animation assets — flag this as a known follow-up cost, not a finished decision.
- **Open question**: does renaming Warrior to Paladin happen alongside introducing Barbarian, so the roster reads as Paladin/Rogue/Sorcerer/Barbarian, or are these independent?

### Unique Legendary Powers, Diablo 3-style (floated 2026-08-07, not started)

**Difficulty:** Very Hard · **Save impact:** Save-breaking

Confirmed via a design discussion (comparing D1/Oracool's affix system to D3's legendary powers) that vanilla Diablo 1's ~80 uniques - and by extension Oracool's Rare/Buffed Unique/Primal tiers, which reuse the same engine - have no unique-only mechanic at all. A `UniqueItem`'s `powers[6]` array draws from the exact same shared `item_effect_type` enum (`IPL_TOHIT`, `IPL_FIRERES`, `IPL_INDESTRUCTIBLE`, etc., `itemdat.h`) that ordinary magic prefixes/suffixes use - a unique is just a fixed, hand-picked combination of those same numeric affixes, never a distinct ability. This idea is about adding real D3-style legendary powers: a specific item doing something no ordinary affix can replicate (e.g. "this hammer's melee hits also cast Chain Lightning," or "Whirlwind no longer costs Fury" for a specific weapon) - true per-item behavior hooks, not another numeric stat line.

- Needs a new piece of data intrinsic to the item itself (which legendary power, if any, it carries) plus new gameplay-code hooks wherever that power's trigger condition lives (on-hit, on-cast, resource cost, etc. - scope depends entirely on which powers get designed). Same category of item-intrinsic data as `OracoolItemTier`/`OracoolAffix`, so it needs an `OracoolItemFormatVersion` bump under the project's established save-format-consolidation policy.
- No design specifics locked in yet - which items get a power, what the powers actually do, and how many to ship in a first pass are all open questions for a real design pass when this gets picked up. Likely benefits from happening alongside or after Set Items, since both want a genuinely distinctive per-item identity beyond the shared affix pool. Rated Very Hard because each power is essentially bespoke gameplay code, and scope is entirely open-ended until specific powers are designed.

### Mercenaries / Followers (floated 2026-08-09, not started)

**Difficulty:** Very Hard · **Save impact:** Save-breaking

Diablo 2/3-style: recruit a hireable NPC companion in town who fights alongside the player and can be equipped with its own gear. This is the largest-scope idea on this list — needs persistent per-character state (which follower is hired, its level/XP) plus an entire second equipment-slot set for the follower's own gear, new AI/combat behavior, and a town recruiting UI. Same item-intrinsic category as Set Items for the follower's equipment specifically, so `OracoolItemFormatVersion`-bump territory; flagged here as needing a real design pass before any of this is scoped further, same spirit as the open questions already listed under Item Tiers and HUD rearrangement.

### Randomized bonus dungeon, light Nephalem Rift-style (floated 2026-08-09, not started)

**Difficulty:** Very Hard · **Save impact:** Unclear — needs a design pass (see caveat)

Diablo 3-style: a repeatable, randomly-composed level (or level segment) for fast, replayable farming runs, distinct from the game's fixed dungeon layouts. The most speculative/ambitious idea on this list — level-generation scope, reward-tracking, and whether it needs any new persistent state at all are all wide open. Flagged for a real design pass before any save-impact classification is even attempted, same as the Item Tiers naming/drop-mechanic open questions.

---

## How the split is decided

This is the reasoning behind each entry's `Save impact` tag above — it no longer determines the file's primary ordering (see the difficulty-based tiers above), but it still matters for scheduling: the user's 2026-08-06 direction to batch all save-breaking work into one later pass, rather than breaking saves repeatedly, still applies whenever items are actually picked up for development.

**The classification always assumes the idea gets built with fine, robust coding in mind — never bucketed as non-breaking just because some hackier shortcut could technically dodge a version bump.** If the clean, sync-safe way to build something needs new per-item data, it's save-breaking, even if a sloppier sidecar-based version could technically avoid the bump — reusing that sidecar approach would just reintroduce the exact bug class (per-container sync drift across backpack/belt/tabs/stash/ground) the v0.2.0 Foundations Pass was built to eliminate. An idea only lands in non-save-breaking when its *properly-built* form genuinely doesn't need new item-intrinsic data, not merely because a shortcut exists.

With that lens, the dividing line is whether the new data can live in its own new, separately-versioned, **absent-tolerant** save file — old saves just don't have it yet and default to empty/zero, same as `heroinvtabs` today (see `AbsentInvTabsFileLeavesTabsEmpty`) — versus needing to grow the existing fixed-size **per-item** record (`SaveItem`/`LoadItemData`), which forces an `OracoolItemFormatVersion` bump that cleanly *rejects* older saves rather than risk silently misreading them.

- **Player-scoped** new data (a materials count, skill points invested) fits the first case: safe as its own additive file, same pattern as Tabbed Inventory's own save file — and that file itself isn't a shortcut, it's the established robust pattern for single-owner, no-duplication player data.
- **Item-scoped** new data (a set ID, a per-affix flag, real crafting metadata) fits the second case: it's the same category of data `OracoolItemTier`/`OracoolAffix` already are. When in doubt about whether a feature's *robust* form needs item-intrinsic data, default to assuming it does (see the Crafting entry) rather than assuming the minimal design that happens to dodge a version bump.

## How to use this file

- Every idea gets one bullet (or a short subsection if it has more than a sentence of context) with the date it was floated, a `Difficulty` tag, and a `Save impact` tag.
- New ideas are inserted into the tier matching their estimated difficulty (Trivial/Easy/Medium/Hard/Very Hard), not appended to the end — this file is a build-order list, not a chronological log. If an idea's difficulty is genuinely unclear until a real design pass happens, say so in the entry rather than guessing, and place it in the tier that reflects the more conservative (harder) estimate.
- **Reference other entries by name, not by position** ("see the Salvaging entry," not "see Salvaging above") — positional references break silently whenever this file gets reordered, which is exactly what happened to the 2026-08-06 non-save-breaking/save-breaking split when it got superseded by difficulty ordering on 2026-08-09.
- Group loosely related ideas under a shared heading if they arrived together and land in the same tier.
- When an idea is picked up for real development, move its entry to a "Built" section at the bottom (or just delete it) with a link to the shipping `OE-###`/version, so this file only ever shows what's still actually pending.

## Built

### Tristram NPC repositioning (floated 2026-08-09, confirmed done 2026-08-10)

Town NPCs moved physically closer to the town well hub, reducing walking between them. Confirmed present in the current codebase (`Source/towners.cpp`'s `TownersData` table) - Adria's move specifically is documented inline as tested against an in-game tile-coordinate debug overlay. No dedicated dev report exists for this one; noted here from the user confirming it done alongside Stash relocation.

### Stash relocation near the town well (floated 2026-08-09, confirmed done 2026-08-10)

The Stash Chest's position (`StashChestPosition`, `Source/objects.cpp`) sits inside the same well-hub cluster as the repositioned NPCs and the player's own town spawn point, all within a few tiles of each other. Confirmed present in the current codebase; no dedicated dev report exists for this one either.

### Rename Warrior to Paladin (floated 2026-08-09, shipped 2026-08-09, Oracool v1.0.2)

Cosmetic-only rename of the Warrior class's display name to Paladin — character creation, character panel, Discord status, and the Warrior-flavored Oracool options all updated; internal enum, asset paths, and ini keys left untouched. See [[2026-08-09 - Rename Warrior to Paladin]] for the full report.

### Autosave-only play (floated 2026-08-05, shipped 2026-08-09, Oracool v1.0.5)

Full character-building-first save model, built across three reports:

- [[2026-08-09 - Autosave-Only Play, Part 1]] — instant autosave triggers for items, gold, XP, stat points, and equipment changes; every Save/Load menu mention removed (pause menu, F2/F3 hotkeys, hero-select dialog); quest state stopped persisting across a load.
- [[2026-08-09 - Character-Only Persistence, No Continue]] — redesigned further once the underlying goal was made explicit (build the strongest character, not progress toward finishing a game session): single-player character selection now skips straight to difficulty selection and always starts a fresh dungeon, with no "Continue"/resume-session concept at all. Multiplayer's resume-session choice was left intact.
- [[2026-08-09 - Quest Log Reveal All]] — every quest available in a session now shows in the quest log immediately, without touching any underlying quest-trigger mechanics (which stay exactly vanilla) — a new `Quest Log Reveal All` option, single-player only, defaulting to enabled.

### Waypoints / fast travel (floated 2026-08-09, shipped 2026-08-10, Oracool v1.0.53)

Diablo 2-style: a physical sigil in town and on every dungeon level (1-16), unlocked on first interaction, opening a travel list to warp between any unlocked waypoint - persisted per character *and* per difficulty. Built and hardened across a long chain of reports, starting with [[2026-08-09 - Autosave-Only Play, Part 1]]'s staging decision to prove the mechanism on one real dungeon level first, through [[2026-08-10 - Waypoint System Complete]]'s closing summary (which lists all nine bugs found and fixed along the way) and [[2026-08-10 - All 16 Dungeon Waypoints and givewp Debug Command]]'s extension to every level. User-tested clean across all 16 dungeon levels plus town, zero problems.

Deliberately left out of this pass (see [[2026-08-10 - Waypoint System Complete]] for the full reasoning): a channeled-cast/interruptible warp mechanic (warping is instant for now), and a lingering cosmetic black-flash-on-arrival specific to warping into town.
