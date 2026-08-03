# Diablo Oracool Edition

Diablo Oracool Edition is a controlled modification of DevilutionX 1.5.5.

## Baseline

- Upstream version: DevilutionX 1.5.5
- Working branch: `oracool-main`
- Primary platform: Windows
- Project root: `C:\DiabloDOE`
- Project library: `C:\DiabloDOE\_ProjectLibrary`

## Working rules

1. Preserve a reproducible, working 1.5.5 baseline before gameplay changes.
2. Make one focused feature change at a time.
3. Document gameplay, compatibility, and build effects.
4. Keep generated builds and release packages out of source history.
5. Test launch behavior before and after substantial changes.
6. Store Oracool documentation and supporting project materials under `_ProjectLibrary`.

## Milestones

- Milestone 0: Clean DevilutionX 1.5.5 build and launch verification. Completed.
- Milestone 1: Oracool Edition identity and core gameplay changes.

## Post-migration roadmap

1. Gold Stacks Buff: completed and user-accepted at the existing save format's exact maximum of 65,535 per inventory stack.
2. Fixed Pepin potion section in Griswold's `Buy consumables`: exactly four infinite potions (Healing, Full Healing, Rejuvenation, and Full Rejuvenation) appear in that order before Adria's stock; implementation and build verification are complete, with user acceptance pending.
3. Add `RESPAWN IN TOWN` to the post-death menu. Selecting it must revive the character in town with all equipped gear and carried items retained; death must not drop any of the character's items. The implementation must preserve the existing death-menu choices and avoid changing save compatibility.
4. Correct release branding and version display. The main menu must show the Oracool release version, beginning with `Diablo Oracool Edition v0.1.0`, instead of presenting the DevilutionX engine version as the Oracool version. Technical or About information should separately state `Based on DevilutionX 1.5.5`. Keep the engine/network compatibility version independent from the visible Oracool release version.
5. Develop **RARE ITEMS** as a major new item category. Rare items are generated from normal white base items, display their names in yellow, may carry up to two prefixes and up to two suffixes, and use the name format `RARE (BASIC BASE ITEM NAME)`. Their complete statistics must be presented through an expanded Unique-style description panel. That panel must render above every other open interface screen, remain readable, and disappear when the mouse cursor is no longer hovering over the corresponding item. Apply the same always-on-top hover behavior retroactively to the description panel for existing Unique items. Detailed generation probabilities, affix compatibility, identification behavior, value calculation, drop sources, save representation, and multiplayer policy will be designed before implementation.
6. Develop **BUFFED UNIQUES** without altering vanilla predefined Unique items. Buffed Uniques are procedurally generated from white base items and function as enhanced magical items with two to three prefixes and two to three suffixes. They are not a finite predefined catalogue. Their gold names use `Unique ` followed by the base item name, such as `Unique Full Plate Mail`, independent of the generated affixes.
7. Develop **PRIMAL ITEMS** as the highest item tier. A Primal item is a Buffed Unique with exactly three prefixes and three suffixes and every base/statistical roll at its maximum permitted value. Its cyan, uppercase name uses `PRIMAL ` followed by the base item name, for example `PRIMAL FULL PLATE MAIL`; affixes do not alter the displayed name. Its drop chance is one fifth of the vanilla Unique chance.
8. Add a new **TORMENT** difficulty above the existing difficulties. Its unlock requirements, monster scaling, resistance rules, experience, treasure quality, multiplayer behavior, and relationship to the new item tiers require a separate balance specification.
9. Make every non-quest consumable stackable with a maximum stack size of 99. Identical consumables automatically merge during pickup and purchase; using one consumes a single unit. Display the current quantity in very small, readable numerals over the item icon. Quest consumables remain unstacked. Define splitting controls, cursor behavior, save representation, and full-inventory edge cases before implementation.
10. Add a **BELT MOD**. Each potion or scroll type occupies only one belt slot, and using it automatically refills that belt slot from an inventory stack of the exact same item type. Define belt quantity display, refill timing, selection priority, hotkey behavior, depleted-stack behavior, and interactions with automatic saving before implementation.

### Approved development order

1. Stackable potions and scrolls.
2. Belt automatic refill.
3. Shared extended item-data and save foundation.
4. Rare Items.
5. Buffed Uniques.
6. Primal Items.
7. Torment difficulty and final balance integration.

Branding correction and Respawn in Town remain independent roadmap work and may be scheduled separately.

### Global future-modification rules

- Every new Oracool gameplay modification is single-player-only. This is a hard rule.
- A deliberate Oracool save-format extension is permitted when required. It must be versioned, validated, documented, and protected against silent data loss.
- New item-tier drop chances derive from the vanilla Unique-item drop chance and therefore respond to `Unique Item Drop Multiplier`.
- Buffed Uniques use the same base drop chance as vanilla Unique items.
- Primals use one fifth of the vanilla Unique-item drop chance.
- Rare items must be fairly common: less common than magical items, more common than Unique items, and substantially closer to magical-item frequency. Determine the exact ratio from the existing drop algorithm and play testing.
- The internal quality-selection method is an implementation decision. It must produce only the intended item, preserve understandable user-facing probabilities, avoid accidental extra drops, and keep vanilla Unique behavior stable.
