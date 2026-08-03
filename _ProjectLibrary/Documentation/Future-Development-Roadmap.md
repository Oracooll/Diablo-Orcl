# Diablo Oracool Edition - Future Development Roadmap

**Project base:** DevilutionX 1.5.5  
**Roadmap status:** Design document for modifications not yet implemented  
**Prepared:** 3 August 2026  
**Audience:** Developers, reviewers, balance testers, and technical advisers

## 1. Purpose

Diablo Oracool Edition is a controlled single-player modification of DevilutionX 1.5.5. This document consolidates the planned work that remains after the first accepted migration and quality-of-life features. It is intended to support technical review and second opinions before the larger systems are implemented.

The roadmap is directional rather than a promise that every implementation detail is final. Confirmed player-facing requirements are distinguished from open engineering and balance decisions.

## 2. Global requirements

- All new gameplay modifications are strictly single-player-only.
- Existing multiplayer behavior must remain vanilla.
- Existing predefined vanilla Unique items must remain unchanged.
- An Oracool-specific save-format extension is acceptable when required.
- Any save extension must be versioned, validated, documented, backward-aware, and protected against silent truncation or item loss.
- Features must be independently configurable where practical, with safe defaults.
- Each major feature must receive focused automated tests, a complete regression build, documentation, and user play testing.
- New item tiers must produce one intended item per drop and must not accidentally create duplicate or extra drops.

## 3. Independent near-term work

These two tasks do not depend on the larger item-system sequence and may be scheduled separately.

### 3.1 Release branding and version display

The main menu must show the Oracool release version rather than presenting the DevilutionX engine version as the Oracool version.

- Visible product version: `Diablo Oracool Edition v0.1.0`
- Separate technical information: `Based on DevilutionX 1.5.5`
- The underlying engine and network-compatibility identifiers must remain independent from the visible Oracool release version.

### 3.2 Respawn in Town

Add `RESPAWN IN TOWN` to the post-death menu.

- Return the dead character to town.
- Retain all equipped gear, inventory items, belt contents, and carried gold.
- Do not drop any character item because of this choice.
- Preserve every existing death-menu choice.
- Avoid unnecessary save-format changes.
- Restrict the feature to single-player.

## 4. Approved major development order

1. Stackable Consumables.
2. Belt Mod and automatic refill.
3. Shared extended item-data and save foundation.
4. Rare Items.
5. Buffed Uniques.
6. Primal Items.
7. Torment difficulty and final balance integration.

The order is intentional. Consumable stacking provides an early practical use for extended saved item state. The shared item foundation then prevents Rare, Buffed Unique, and Primal systems from each inventing incompatible storage mechanisms.

## 5. OE-012 - Stackable Consumables

### Confirmed behavior

- Every non-quest consumable is stackable, including consumable categories beyond potions and scrolls.
- Quest consumables remain unstacked.
- Only identical consumables may share a stack.
- Maximum stack quantity: 99.
- Ground pickups merge into compatible partial stacks before taking a new inventory slot.
- Store purchases merge into compatible partial stacks before taking a new inventory slot.
- Using a stacked consumable removes one unit and leaves the remaining stack in place.
- A small but readable quantity number is rendered over the item icon.

### Engineering decisions still required

- Exact identity rules for consumables with internal state.
- Stack splitting controls and controller equivalents.
- Cursor-held stack behavior.
- Handling partial capacity when a pickup or purchase exceeds available stack space.
- Store selling and pricing of partial stacks.
- Save migration for pre-existing unstacked consumables.
- Interaction with automatic saving and failed inventory transactions.

## 6. OE-013 - Belt Mod

### Confirmed behavior

- A potion or scroll type occupies one belt slot.
- Using the belt item consumes one unit.
- The slot automatically refills from an inventory stack of the exact same item type.
- The belt icon shows the total available quantity for that consumable type.

### Engineering decisions still required

- Whether the belt holds a physical unit or represents a link to an inventory supply.
- Which inventory stack is consumed first when several partial stacks exist.
- Refill timing relative to animation, input, networking guards, and automatic saving.
- Behavior when the final unit is consumed.
- Controller, hotkey, drag-and-drop, and belt-reordering behavior.
- Safe handling when inventory changes during use.

## 7. Shared extended item-data and save foundation

This is an enabling architecture phase rather than a visible item tier.

### Required capabilities

- Persist up to three prefixes and three suffixes on one generated item.
- Persist every affix identity and rolled value exactly.
- Persist item-tier identity: Rare, Buffed Unique, or Primal.
- Persist perfect-roll identity where required.
- Persist consumable stack quantities up to 99.
- Load existing Oracool and compatible DevilutionX 1.5.5 characters safely.
- Reject or recover corrupted extension data without silently destroying items.
- Keep multiplayer on the unmodified vanilla path.

### Required safeguards

- Explicit save-extension version.
- Bounds checking for item counts, affix identifiers, rolls, and stack quantities.
- Round-trip tests for every new field.
- Backup or recovery strategy before irreversible migration.
- Documented behavior when a new-format character is opened by an older build.

## 8. Shared item-statistics popup

Rare, existing vanilla Unique, Buffed Unique, and Primal items use the engine's existing vanilla Unique-item statistics popup method.

- Hovering over an eligible item opens its statistics popup.
- The popup must render above inventory, character, store, stash, and every other open interface panel.
- Text must remain readable regardless of the underlying screen.
- The popup disappears as soon as the pointer no longer hovers over the owning item.
- Existing vanilla Unique items receive the same corrected always-on-top behavior retroactively.
- Long property lists must fit safely through layout expansion, wrapping, paging, or another reviewed presentation method.

## 9. OE-008 - Rare Items

### Confirmed identity

- Generated from normal white base items.
- Yellow item-name color, distinct from white, blue, and gold items.
- At least one prefix and one suffix.
- At most two prefixes and two suffixes.
- Display name: `Rare ` plus the unmodified base-item name. The game renderer may control visible letter case.
- Affixes do not need to appear in the item name because the statistics popup lists the complete properties.

### Drop behavior

- Rare items should be fairly common.
- They are less common than magical items but more common than Unique items.
- Their frequency should be substantially closer to Magic than to Unique.
- The exact chance will be derived from the vanilla item-generation pipeline and tuned through play testing.

### Design decisions still required

- Eligible item classes and drop sources.
- Identification state when dropped.
- Affix compatibility, exclusions, duplication policy, and level requirements.
- Distribution among two, three, and four total affixes while preserving the one-prefix/one-suffix minimum.
- Roll ranges, price calculation, store eligibility, and repair/recharge behavior.
- Exact interaction with `Unique Item Drop Multiplier`, if any.

## 10. OE-009 - Buffed Uniques

Buffed Uniques are not modifications of the predefined vanilla Unique catalogue. They are a procedurally generated, effectively unlimited class based on white base items.

### Confirmed identity

- Existing vanilla Unique items remain unchanged.
- Generated from normal white base items.
- Function mechanically as heavily enhanced magical items.
- At least two prefixes and two suffixes.
- At most three prefixes and three suffixes.
- Gold item-name color, matching vanilla Uniques.
- Display name: `Unique ` plus the unmodified base-item name, for example `Unique Full Plate Mail`.
- Affixes do not alter the displayed name, so different items may share a name while having different statistics.
- Complete properties appear in the shared hover popup.

### Drop behavior

- Base drop chance equals the vanilla Unique-item drop chance.
- The chance responds to the existing `Unique Item Drop Multiplier` setting.
- Internal roll ordering is an engineering decision, provided the player-facing probability and vanilla Unique behavior remain stable.

### Design decisions still required

- Eligible base items and drop sources.
- Affix compatibility, duplication restrictions, and level rules.
- Identification state, pricing, selling, repair, and recharge behavior.
- Distribution between four, five, and six total affixes.

## 11. OE-010 - Primal Items

Primals are the highest planned item tier and are derived from the Buffed Unique rules.

### Confirmed identity

- Exactly three prefixes and three suffixes.
- Every applicable base-item statistic is rolled at its permitted maximum.
- Every generated affix value is rolled at its permitted maximum.
- Cyan item-name color.
- Display name: `Primal ` plus the unmodified base-item name, for example `Primal Full Plate Mail`.
- Letter case is not stored as a hard requirement; the game font may render the visible name in uppercase.
- Affixes do not alter the displayed name.
- Complete properties appear in the shared hover popup.

### Drop behavior

- Base drop chance is one fifth of the vanilla Unique-item drop chance.
- The chance responds to `Unique Item Drop Multiplier`.
- Generation must not reduce, replace, or duplicate vanilla Unique drops unintentionally.

### Design decisions still required

- Eligible base items and sources.
- Whether maximum durability and charges count as perfect-roll statistics.
- Pricing and store interaction.
- Additional visual or audio feedback beyond cyan text.

## 12. OE-011 - Torment Difficulty

Torment is a new difficulty above Hell.

### Scaling method

1. Measure how relevant systems change from Normal to Nightmare.
2. Measure how the same systems change from Nightmare to Hell.
3. Identify linear, multiplicative, capped, and exceptional rules.
4. Extend those patterns from Hell into a proposed Torment baseline.
5. Build and conduct practical user testing.
6. Revise parameters that prove unfair, trivial, or technically unsafe.

### Configuration

- Setting: `Torment Difficulty Multiplier`.
- Range: 1.1 to 5.0.
- Step: 0.1.
- The setting scales Torment from the Hell baseline.
- It does not modify Hell itself.
- The initial default will be selected after measuring the existing progression.

### Systems requiring separate analysis

- Monster health, damage, armor class, hit chance, resistances, immunities, and AI exceptions.
- Player resistance or other difficulty penalties.
- Experience rewards and level scaling.
- Item level, treasure quality, and new-tier drop opportunities.
- Bosses, quests, shrines, traps, and special level rules.
- Unlock requirements and difficulty-menu presentation.
- Save metadata and safe fallback when Torment is unavailable.

## 13. Drop-system policy

- Buffed Unique chance: 1.0 times the vanilla Unique chance.
- Primal chance: 0.2 times the vanilla Unique chance.
- Rare chance: between Magic and Unique, deliberately closer to Magic; exact value pending inspection and testing.
- Buffed Unique and Primal chances respond to `Unique Item Drop Multiplier`.
- The exact internal quality-roll method is delegated to implementation.
- The final method must yield one intended item per generated drop, preserve specified relative frequencies, avoid extra-drop side effects, and keep vanilla Unique behavior stable.

## 14. Review questions for a third party

A reviewer is invited to comment on:

1. A robust, versioned extension strategy for additional affixes and stack quantities.
2. Compatibility risks when older builds encounter extended saves.
3. Safe integration points in DevilutionX item generation and packing.
4. Fair distributions for Rare and Buffed Unique affix counts.
5. A Rare-item probability that feels common without overwhelming magical items.
6. Popup layout strategies for six-affix items at all supported resolutions.
7. Belt-stack ownership models that avoid duplication or loss.
8. Torment scaling formulas that extend Hell coherently without producing unavoidable damage or immunity walls.
9. Automated tests and fuzz cases most likely to expose item-loss or save-corruption defects.

## 15. Definition of done for each major feature

A roadmap feature is not complete until:

- Its detailed design decisions are documented.
- It is guarded to single-player wherever required.
- Focused automated tests cover normal behavior and failure boundaries.
- The full project builds and the complete regression suite passes.
- Save/load behavior is verified where relevant.
- User-facing configuration descriptions are complete.
- Manual test instructions are written.
- The user has tested and accepted the feature or explicitly deferred acceptance.
