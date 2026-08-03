# Gameplay Changes

## OE-001: Runtime product name

- Status: Verified
- Scope: Branding only
- Change: The runtime product and window title is `Diablo Oracool Edition`.
- Compatibility: No gameplay, save-game, or multiplayer behavior is changed.
- Build system: The internal CMake project and executable remain named `DevilutionX` and `devilutionx.exe` for stability.
- Verification: The `x64-Debug` build succeeded and launched with `Diablo Oracool Edition v1.5.5Debug` in the window title.

## OE-002: Portable configuration foundation

- Status: User verified
- Scope: Windows paths and configuration
- Change: `diablo.ini` and `Saved_Games` are stored beside the executable; the complete Oracool option category and shared single-player guard are available.

## OE-003: World and item quality-of-life batch

- Status: User verified
- Features: Unique Item Drop Multiplier, Unlock All Town Entrances, Permanent Infravision, Auto Identify Drops, and Auto Pickup Range.
- Compatibility: All gameplay behavior is guarded to single-player. Vanilla behavior remains available through individual settings.
- Build verification: The `x64-Debug` executable compiled and linked successfully on DevilutionX 1.5.5.

## OE-004: Griswold Unique Items shop

- Status: User verified
- Scope: Single-player Griswold inventory and purchasing
- Change: Adds `Buy unique items` with an independent, identified, non-duplicate stock generated once per game. The configured stock count is clamped to 1-8 and candidates above the character's level are excluded.
- Pricing: Purchase price is the unique item's normal value multiplied by `Griswold Unique Item Price Multiplier`.
- Persistence: Purchased items disappear without replacement. Premium Refresh and Refresh Until do not touch unique stock.
- Compatibility: The shop is absent in multiplayer and introduces no save-format change.
- Build verification: The `x64-Debug` executable compiled and linked successfully on DevilutionX 1.5.5.

### Acceptance correction

- The initial port allowed purchases to disappear correctly but inherited Griswold's quest-base exclusion when listing the purchased unique for resale. Unique Shop merchandise is now explicitly marked as smith merchandise and is eligible for resale even when its underlying base item ID belongs to the protected quest range. Actual quest items remain excluded.

## OE-005: Automatic saving

- Status: User verified
- Scope: Single-player save scheduling
- Change: Adds periodic saving plus successful level-change, non-gold item-pickup, and store-purchase triggers.
- Debouncing: Rapid pickups and purchases restart one configurable delay and produce a single save.
- Safety: Pending saves wait until gameplay is active, the player is alive, menus and stores are closed, no item is held by the cursor, and no demo is running or recording.
- Reset behavior: Every completed save, including a manual save, resets the interval and clears pending work to prevent duplicate saves.
- Notification: Automatic saves optionally show the standard brief `Game Saved` message.
- Compatibility: Multiplayer behavior and the save format are unchanged.

## OE-006: Gold Stacks Buff

- Status: User verified
- Scope: Single-player inventory gold stacks
- Change: Raises the maximum value of one inventory gold stack from 5,000 to 65,535.
- Limit rationale: 65,535 is the maximum unsigned 16-bit value stored in the existing `ItemPack.wValue` field. A higher stack would be truncated during character saving, so it is not safe without a save-format change.
- Integration: Existing pickup, merging, automatic placement, splitting, shop-payment, stash-withdrawal, cursor, and total-gold paths already use the shared runtime stack limit and therefore inherit the new cap.
- Auric Amulet: The buff supersedes the amulet's normal 10,000 stack benefit while enabled because 65,535 is already the format maximum.
- Compatibility: Disabled and multiplayer games retain the vanilla 5,000 limit, including the Auric Amulet's existing 10,000 behavior. No save-format change is introduced.
- Configuration: `Gold Stacks Buff=1` in `[Oracool Edition]`; enabled by default and applied when starting or loading a game.
- Verification: The complete Debug build succeeds, the 65,535 packed-item round trip has focused automated coverage, and all 214 automated tests pass.

### Acceptance correction

- Initial user testing exposed a second, hard-coded 5,000 clamp in recurring player validation. A large stash withdrawal was first placed into one enlarged stack, then immediately truncated to 5,000 while the stash had already deducted the full request.
- Player validation now uses the active shared stack limit. Stash withdrawal also deducts only the amount actually placed and immediately recalculates the carried-gold total, so insufficient inventory capacity cannot destroy gold.
- Correction verification: focused partial-withdrawal coverage passes and the complete suite passes all 215 tests.
- User acceptance: On 2026-08-02, the corrected build passed the user's full practical testing with no further problems found.

## OE-007: Fixed Pepin potions in Griswold's Buy Consumables

- Status: User verified
- Scope: Single-player Griswold consumables store
- Change: Griswold's `Buy consumables` list begins with exactly four Pepin potions, in this order: Potion of Healing, Potion of Full Healing, Potion of Rejuvenation, and Potion of Full Rejuvenation. Adria's normal stock follows. No other Pepin merchandise appears in Griswold's list.
- Purchasing: The four fixed potions use their normal prices, payment checks, and inventory-placement behavior.
- Replenishment: All four fixed potions are infinite: buying one leaves it available. Adria's entries retain their normal replenishment or removal behavior.
- Navigation: Confirmation, cancellation, insufficient-gold, insufficient-room, Back, and Escape paths remain inside Griswold's interface.
- Compatibility: Pepin's and Adria's own stores remain unchanged. The Griswold service remains single-player-only and introduces no save-format changes. Fixed potions use exact base-item recreation so rejuvenation potions retain their type after save and reload.
- Configuration: Uses the existing `Griswold Sell Consumables=1` setting and remains enabled by default.
- Verification: The complete Debug build succeeds, exact stock order and post-purchase replenishment behavior have focused automated coverage, and all 216 automated tests pass.

### Implementation history

- The first 1.5.5 implementation appended Pepin's full generated inventory after Adria's. A scroll-range initialization bug initially hid that section; explicit combined-store mode fixed it.
- The design was then revised at user request: Griswold now shows only four dedicated, replenishing Pepin potions before Adria's stock and no longer mirrors Pepin's generated inventory.

## OE-008: Rare Items

- Status: Roadmap concept; not yet designed or implemented
- Scope: Major new item-quality category and item-description interface
- Base items: Rare items are derived from normal white base items.
- Name color: Yellow, visually distinct from white normal items, blue magical items, and gold Unique items.
- Drop frequency: Fairly common. Rare items occur less often than magical items but more often than Unique items, with their frequency deliberately much closer to magical items. The exact probability will be derived from the vanilla drop pipeline and tuned through play testing.
- Affixes: A Rare item may have up to two prefixes and up to two suffixes.
- Naming: `RARE (BASIC BASE ITEM NAME)`, using the unmodified base item's name inside the parentheses.
- Statistics display: Hovering over a Rare item invokes the engine's existing vanilla Unique-item statistics popup method so all properties can be read clearly.
- Interface priority: The description window must render above every other open screen or panel and remain readable regardless of the underlying interface state.
- Hover lifetime: The description remains visible only while the mouse cursor is hovering over the item that owns it and disappears when the cursor moves away.
- Tier integration: The same hover-triggered, always-on-top statistics popup must be used for Rare, existing Unique, Buffed Unique, and Primal items.
- Design work required: Define generation probability and sources, affix selection and compatibility rules, minimum and maximum affix counts, identification behavior, pricing, inventory/store interaction, save representation, backward compatibility, and multiplayer policy before implementation begins.

## OE-009: Buffed Uniques

- Status: Roadmap concept; not yet designed or implemented
- Vanilla separation: Existing predefined vanilla Unique items and their behavior remain unchanged.
- Item foundation: Procedurally generated from normal white base items; the population is effectively unbounded rather than a finite predefined catalogue.
- Item model: Enhanced magical items presented as a new Unique class.
- Affix range: At least two prefixes and two suffixes; at most three prefixes and three suffixes.
- Naming: `Unique ` followed by the unmodified base item name, for example `Unique Full Plate Mail`. Affixes do not alter this displayed name, so separately generated items may share a name while having different properties.
- Name color: The same gold used by vanilla Unique items.
- Drop chance: Equal to the vanilla Unique-item drop chance and affected by `Unique Item Drop Multiplier`.
- Statistics display: Hovering uses the same engine-native Unique-item popup shared by Rare, existing Unique, and Primal items; it must render above other open interface panels.
- Design work required: Establish affix compatibility and roll ranges, pricing, identification behavior, drop eligibility, and the extended save representation.

## OE-010: Primal Items

- Status: Roadmap concept; not yet designed or implemented
- Item hierarchy: Highest tier, derived from Buffed Uniques.
- Affixes: Exactly three prefixes and three suffixes.
- Perfect-roll rule: The base item and every applicable generated property use their maximum permitted values; a Primal is a fully maxed Buffed Unique.
- Name color: Cyan.
- Naming: `Primal ` followed by the unmodified base item name, for example `Primal Full Plate Mail`. Generated affixes do not alter the displayed name. Uppercase is not required in the stored text; the game font renderer may naturally display the name as `PRIMAL FULL PLATE MAIL`.
- Drop chance: One fifth of the vanilla Unique-item drop chance and affected by `Unique Item Drop Multiplier`.
- Statistics display: Hovering uses the same engine-native Unique-item popup shared by Rare, existing Unique, and Buffed Unique items; it must render above other open interface panels.
- Design work required: Eligibility, rarity, drop sources, visual identity, pricing, description layout, and save compatibility.

## OE-011: Torment Difficulty

- Status: Roadmap concept; not yet designed or implemented
- Scope: New difficulty above the existing game difficulties.
- Scaling method: First measure how every relevant parameter changes from Normal to Nightmare and from Nightmare to Hell. Use those actual progressions to propose a consistent but playable extension from Hell to Torment rather than choosing isolated multipliers.
- Balance process: The user will test Torment in play and scaling will be revised when practical results require it.
- Configuration: Add `Torment Difficulty Multiplier`, ranging from 1.1 to 5.0 in steps of 0.1. It scales Torment from the Hell baseline and does not alter Hell itself. Select the initial default from the measured Normal-to-Nightmare-to-Hell progression, then revise it through play testing if necessary.
- Design work required: Unlock conditions, monster health and damage, armor and resistance scaling, player penalties, experience rewards, treasure quality, quest behavior, and multiplayer compatibility.

## OE-012: Stackable Consumables

- Status: Roadmap concept; not yet designed or implemented
- Eligibility: Every non-quest consumable is stackable; quest consumables are excluded.
- Stack limit: 99 identical consumables per inventory item stack.
- Automatic merging: Pickups and store purchases merge into compatible partial stacks before consuming a new inventory slot.
- Consumption: Using a stacked consumable removes one unit and leaves the remainder in place.
- Quantity display: Very small numerals rendered legibly over the item icon.
- Design work required: Exact identity rules, splitting controls, cursor-held items, inventory capacity, extended save representation, and backward compatibility.

## OE-013: Belt Mod

- Status: Roadmap concept; not yet designed or implemented
- Belt occupancy: A potion or scroll type uses one belt slot.
- Automatic refill: After use, the belt slot refills from an inventory stack of the exact same item type.
- Quantity display: Show the total available quantity for that consumable type on the belt icon.
- Design work required: Belt quantity presentation, refill timing, inventory-stack priority, controller and hotkey behavior, depleted stacks, simultaneous inventory changes, and automatic-save integration.

## Shared compatibility policy for OE-008 through OE-013

- Multiplayer: All new gameplay systems are strictly single-player-only.
- Save format: An Oracool-specific, versioned save extension is allowed when needed to preserve additional affixes, perfect-roll identity, and consumable quantities. Migration and validation must prevent silent truncation or item loss.
- Drop multiplier: New high-tier item chances are derived from vanilla Unique-item probability so `Unique Item Drop Multiplier` affects them. Buffed Uniques use a 1:1 ratio and Primals a 1:5 ratio relative to vanilla Uniques. Rare items are fairly common between Magic and Unique frequency and closer to Magic; their exact ratio will be selected after inspecting the vanilla drop pipeline.
- Quality selection: The precise internal roll ordering is delegated as an implementation decision. It must yield one intended item per generated drop, preserve the specified relative frequencies, avoid extra-drop side effects, and leave vanilla Unique behavior stable.
