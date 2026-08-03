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
6. Develop **BUFFED UNIQUES**. These are enhanced Unique items with a provisional minimum of two prefixes and two suffixes and a maximum of three prefixes and three suffixes. Their relationship to the original Unique powers, generation rules, naming, visual identity, and save representation must be designed before implementation.
7. Develop **PRIMAL ITEMS** as the highest item tier. A Primal item is a perfect-roll Buffed Unique with exactly three prefixes and three suffixes, with every applicable roll at its maximum value. Define drop rarity, eligibility, visual identity, and compatibility safeguards during detailed design.
8. Add a new **TORMENT** difficulty above the existing difficulties. Its unlock requirements, monster scaling, resistance rules, experience, treasure quality, multiplayer behavior, and relationship to the new item tiers require a separate balance specification.
9. Add stackable potions and scrolls with a maximum stack size of 99. Display the current quantity in very small, readable numerals over the item icon. Define stacking compatibility, splitting, pickup, stores, cursor behavior, save representation, and full-inventory edge cases before implementation.
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
