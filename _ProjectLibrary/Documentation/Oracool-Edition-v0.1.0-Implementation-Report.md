# Diablo Oracool Edition v0.1.0 - Implementation Report

**Engine base:** DevilutionX 1.5.5  
**Platform:** Windows x64  
**Release date:** 2 August 2026  
**Report prepared:** 3 August 2026  
**Purpose:** Detailed record of the work completed for Oracool Edition v0.1.0

## 1. Executive summary

Diablo Oracool Edition v0.1.0 is the first packaged development preview of a controlled single-player modification built on DevilutionX 1.5.5. It consolidates the complete migration of the earlier DevilutionX 1.5.4 Oracool prototype, repairs incompatibilities discovered during that migration, and adds several post-migration extensions.

The release contains an optimized Windows x64 executable, the required runtime libraries, installation instructions, and license notices. It is designed to be extracted directly into a working vanilla DevilutionX 1.5.5 folder. It does not distribute Blizzard game data, personal configuration, or saved characters.

The principal results are:

- Portable Windows saves and configuration stored beside the executable.
- A dedicated Oracool Edition settings group available both in-game and in `diablo.ini`.
- Single-player guards that preserve vanilla multiplayer behavior.
- World, item, character, merchant, automatic-save, and gold-stack modifications.
- A significantly expanded Griswold interface with refresh, search, purchasing, selling, recharge, restoration, and Unique-item services.
- All Boolean Oracool modifications enabled by default for new configurations.
- An optimized v0.1.0 package verified file-by-file before release.

## 2. Project identity and versioning

### Implemented identity

- Runtime product name: `Diablo Oracool Edition`.
- Executable filename remains `devilutionx.exe` for build and deployment stability.
- Internal CMake project naming remains compatible with DevilutionX.
- Engine base remains DevilutionX 1.5.5.
- Oracool release number is v0.1.0.

### Known presentation issue

The current v0.1.0 executable displays the DevilutionX engine version as part of the main-menu product label, producing `Diablo Oracool Edition 1.5.5`. This is technically the engine base but is not the correct visible Oracool release number.

The correction is documented but was not rebuilt into the original v0.1.0 archive:

- Intended main-menu label: `Diablo Oracool Edition v0.1.0`.
- Separate technical statement: `Based on DevilutionX 1.5.5`.
- Engine and network compatibility identifiers must remain independent from the visible Oracool release number.

## 3. Development and documentation foundation

### Source control

- Working branch: `oracool-main`.
- Changes were divided into focused commits rather than maintained as an undocumented patch collection.
- Generated builds and release archives remain outside source history.
- Implementation decisions, feature behavior, migration status, testing, and release records are stored under `_ProjectLibrary`.
- User-owned Obsidian workspace data is intentionally excluded from project commits.

### Build foundation

- Clean DevilutionX 1.5.5 CMake configuration was established with Visual Studio build tools and Ninja.
- Debug builds were used for implementation and automated regression testing.
- The public v0.1.0 package was compiled as an optimized Windows x64 Release build.
- Runtime dependencies were inspected from the completed executable before packaging.

### Compatibility policy

- Gameplay modifications are single-player-only unless explicitly documented otherwise.
- Multiplayer follows vanilla behavior.
- Every configurable feature can be disabled to restore its corresponding vanilla behavior.
- The v0.1.0 migration introduces no new character save format.
- Existing engine routines are reused where practical to reduce behavioral drift.

## 4. Portable configuration and saves

### Local configuration

On Windows, `diablo.ini` is stored beside `devilutionx.exe` instead of depending on the normal per-user application-data path.

Benefits include:

- A self-contained portable game folder.
- Easier backup and migration.
- Direct access to advanced settings.
- Predictable configuration location during development and testing.

### Portable saved characters

Character data is stored under `Saved_Games` beside the executable.

- Existing saves are not moved automatically.
- The game creates and uses the local folder for new or copied saves.
- Release archives intentionally do not include or overwrite saved characters.

### Oracool settings interface

- A dedicated `[Oracool Edition]` group exists in `diablo.ini`.
- The group is positioned at the bottom of the generated INI layout.
- Options are organized alphabetically within functional groups.
- Detailed descriptions explain the purpose and limits of each setting.
- Supported settings are also available from the in-game options interface.
- Every Boolean Oracool modification defaults to enabled in a newly generated configuration.
- Existing user values are preserved rather than replaced during an upgrade.

## 5. World and item quality-of-life features

### Unique Item Drop Multiplier

- Configurable single-player multiplier from 1 to 100.
- Multiplies the final vanilla Unique-item probability.
- The resulting probability is capped at 100 percent.
- Multiplayer retains the vanilla chance.

### Unlock All Town Entrances

- Unlocks Catacombs, Caves, and Hell entrances.
- Also unlocks Hive and Crypt entrances when Hellfire content is active.
- The Cathedral entrance remains unchanged.
- Restricted to single-player.

### Permanent Infravision

- Continuously grants infravision in single-player.
- Does not consume an item.
- Does not inflate a temporary-effect duration counter.
- Disabling the option restores ordinary behavior.

### Auto Identify Drops

- Newly generated world drops are automatically identified.
- Items deliberately dropped by the player retain their existing identification state.
- This distinction prevents the option from becoming an unintended free-identification action for carried items.

### Auto Pickup Range

- Configurable from 1 to 10 tiles.
- Extends only item categories already enabled for automatic pickup.
- Searches nearer tiles first.
- Prevents duplicate pickup requests.
- Does not independently enable categories the player has disabled.

## 6. Character progression features

### Remove Stat Limits

- Allows base-attribute allocation up to 255 in single-player.
- Values stop safely at 255.
- A value of exactly 255 is displayed in gold.
- If the option is later disabled, legitimate over-class-cap values are preserved rather than truncated.
- Preserved over-cap values are displayed in red and cannot receive further points while the option is disabled.
- Multiplayer retains vanilla class limits.

### Reset Stats Button

- Adds an `R` control to the local character panel.
- Pressing it resets base attributes to the selected class's starting values.
- Available stat points become exactly five times `(character level - 1)`.
- Repeated resets are idempotent and do not generate additional points.
- Permanent non-level stat gains are lost and are not refunded.
- The control is absent in multiplayer.

### Permanent Free Town Portal

- Gives new and existing single-player characters memorized Town Portal level 1.
- Does not reduce an already higher memorized spell level.
- Casting the memorized spell costs zero mana while enabled.
- Disabling the option restores the normal mana cost without forcibly removing the learned spell.

## 7. Griswold merchant expansion

Griswold received the largest concentrated group of interface and merchant modifications. New services preserve single-player restrictions and reuse existing merchant rules where possible.

### Premium Refresh

- Adds a free `Refresh` action to Griswold's Premium Items screen.
- Replaces the complete six-item premium stock.
- Does not charge gold.
- Purchased items and selection behavior continue to use normal store rules.
- The action is absent when disabled or in multiplayer.

### Refresh Until

- Adds a `Refresh until` search action to the Premium Items screen.
- Accepts semicolon-separated exact item names from `Griswold Refresh Until Item Names`.
- Trims surrounding whitespace.
- Matches case-insensitively but requires the complete displayed item name.
- Stops when any requested item is generated.
- Retains the successful inventory for purchase.
- Supports a configurable timeout.
- A timeout of zero still respects a 100,000-generation hard cap.
- Provides success, empty-input, timeout, and hard-limit feedback.
- Retains the last generated stock after an unsuccessful search.

### Restore Health and Restore Mana

- Independent settings silently refill the enabled resource when Griswold's main menu opens.
- Health and mana restoration can be enabled separately or together.
- No additional menu entries or spell sounds are produced.
- Multiplayer remains unchanged.

### Buy Consumables

- Adds a dedicated Griswold `Buy consumables` service.
- Reuses Adria's generated inventory, purchase rules, replenishment behavior, payment checks, and placement checks.
- Confirmation, cancellation, insufficient-gold, insufficient-room, Back, and Escape paths return to Griswold rather than Adria.

### Fixed Pepin potion extension

The final v0.1.0 design places exactly four Pepin potions before Adria's normal stock:

1. Potion of Healing.
2. Potion of Full Healing.
3. Potion of Rejuvenation.
4. Potion of Full Rejuvenation.

All four are permanently available and remain in stock after purchase. No other Pepin merchandise appears in Griswold's list. Pepin's own store remains unchanged.

The first implementation attempted to append Pepin's complete generated stock. A scroll-range initialization defect initially hid that section. The defect was corrected, after which the design was deliberately simplified to the four requested infinite potions. Exact base-item recreation ensures rejuvenation potions retain the correct type after save and reload.

### Recharge Staves

- Adds a Griswold `Recharge staves` service.
- Reuses Adria's staff eligibility, pricing, confirmation, payment, and charge-restoration behavior.
- Supports equipped and inventory staves.
- Fully charged or otherwise ineligible staves remain absent.
- Every exit path returns to Griswold.

### Buy All Items and Sell All

- Expands Griswold's sell-to-merchant eligibility to every positive-value non-quest item.
- Includes items normally sellable only to Adria, including consumables and staves.
- Excludes gold, quest items, Lazarus's staff, and zero-value items.
- Adds a `Sell all` action.
- Calculates proceeds from the same displayed individual sale prices.
- Stops safely when proceeds cannot fit.

An early migration accepted only vanilla Griswold item categories. User testing revealed that Adria-sellable items were still rejected. Eligibility was expanded and then accepted in practical testing.

### Sort Sell Items by Price

- Sorts eligible inventory and belt entries by descending sale price.
- Equal-price items preserve their original relative order.
- Selling removes the correct original item despite the displayed sorting layer.

### Menu selection restoration

Opening and closing a Griswold service now returns the selector to the exact service that was closed. This corrects an early defect in which the selection moved one row downward after Back or Escape.

### Procedural Unique shop

- Adds `Buy unique items` as an independent Griswold service.
- Generates between 1 and 8 eligible items according to configuration.
- Items are identified and contain no duplicate Unique names.
- Candidates above the character's level are excluded.
- Prices use the normal sell value multiplied by `Griswold Unique Item Price Multiplier`.
- Purchased stock disappears without replacement.
- Premium Refresh and Refresh Until do not alter Unique-shop stock.
- Remaining stock persists for the current game.

Purchased Unique-shop items initially inherited a quest-base resale exclusion and could not always be sold back. Shop merchandise is now explicitly marked so purchased items are eligible for resale while actual quest items remain protected.

### Premium generation limit controls

Two independent controls can relax:

- Affix quality-level restrictions.
- Premium-item price rejection.

Item-type compatibility, good-affix rules, game-mode restrictions, and base-item progression remain intact. The code is implemented and build-verified. High-level boundary testing is deferred because the available test character was level 30 rather than the level needed to exercise every boundary.

## 8. Automatic saving

The automatic-save system adds several single-player triggers without allowing saves during unsafe interface or gameplay states.

### Supported triggers

- Configurable periodic interval: 1, 2, 3, 5, 10, 15, 30, or 60 minutes.
- Successful level transition in either direction.
- Successful non-gold item pickup.
- Successful store purchase.

### Delayed and debounced behavior

- Pickup and purchase saves use a configurable delay of 0, 1, 2, 3, 5, 10, 15, or 30 seconds.
- Rapid qualifying actions restart one timer and produce one save.
- Gold pickup does not trigger an item-pickup save.
- Cancelled or failed purchases do not trigger a store-purchase save.

### Safety conditions

Pending automatic work waits until:

- Active gameplay is running.
- The player is alive.
- Menus and stores are closed.
- No item is held by the cursor.
- No demo is running or recording.

Every completed save, including a manual save, resets pending timers to prevent immediate duplicate saves. An optional standard `Game Saved` notification may be disabled without disabling the save itself.

## 9. Gold Stacks Buff

### Final behavior

- Raises one inventory gold stack from 5,000 to 65,535 in single-player.
- 65,535 is the exact maximum unsigned 16-bit value supported by the existing packed-item save field.
- Larger values would be truncated without a save-format change.
- Pickup, merging, placement, splitting, dropping, shop payment, stash withdrawal, cursor display, and total-gold calculations use the shared active limit.
- Additional gold moves into another inventory square after 65,535.
- The option supersedes the Auric Amulet's 10,000 benefit while enabled.
- Disabling it restores the vanilla 5,000 limit and the Auric Amulet's existing behavior.
- Multiplayer retains vanilla limits.

### Data-loss defect and correction

Initial testing revealed a critical second hard-coded 5,000 clamp in recurring player validation. A large stash withdrawal was placed into an enlarged stack, after which validation truncated it to 5,000 even though the stash had deducted the full requested amount.

The correction introduced two safeguards:

- Player validation now uses the active shared stack limit.
- Stash withdrawal deducts only the amount actually placed and immediately recalculates carried gold.

This prevents both truncation and loss when inventory capacity can accept only part of a requested withdrawal. The corrected implementation passed focused tests, the complete suite, and broad practical user testing.

## 10. Default configuration policy

- Every Boolean Oracool modification is enabled by default for a new `diablo.ini`.
- Numeric and text settings retain safe documented defaults.
- Individual toggles remain available in-game and in the INI.
- Disabling a feature restores its corresponding vanilla behavior wherever technically possible.
- Existing user configuration is preserved during upgrades.

This policy makes Oracool Edition's identity immediately visible while retaining granular control for players who prefer selected vanilla mechanics.

## 11. Verification and acceptance record

### Automated verification

- Clean Debug builds succeeded throughout the migration.
- The complete suite passed 213 tests after the main migration.
- Gold Stacks Buff and its packed-save boundary increased the suite to 214 passing tests.
- The gold-validation and lossless partial-withdrawal correction increased it to 215 passing tests.
- The final fixed Pepin-potion implementation completed with all 216 automated tests passing.
- Focused coverage includes stock order, replenishment, resale mapping, gold boundaries, and save round trips.

### Manual acceptance

The user tested and accepted:

- World and item quality-of-life features.
- Remove Stat Limits, Reset Stats, and Permanent Free Town Portal.
- Griswold Premium Refresh, Refresh Until, restoration, consumables, recharge, expanded buying, bulk selling, price sorting, menu-selection restoration, and Unique shop.
- Unique-shop purchase removal and resale correction.
- Automatic-save triggers and exclusions.
- Corrected Gold Stacks Buff, including broad practical testing.
- The final four fixed, infinite Pepin potions before Adria's stock.

### Deferred validation

High-level boundary testing of Griswold's premium generation limit controls remains deferred until a sufficiently high-level character is available. The feature is implemented and build-verified.

## 12. Release package

### Artifact

- Filename: `Diablo Oracool Edition v0.1.0.zip`.
- Original location: `C:\DiabloDOE\Releases`.
- Size: 4,212,131 bytes.
- Verified file entries: 15.
- SHA-256: `692B38E1A127A0BC59166478B6864915F2BE7BE6228E6E58D6B85CC07559B858`.

### Included files

- Optimized Windows x64 `devilutionx.exe`.
- Eight required adjacent runtime libraries.
- Installation instructions.
- DevilutionX and relevant bundled license notices.

### Intentionally excluded

- Blizzard Diablo or Hellfire MPQ game data.
- `diablo.ini` and personal settings.
- `Saved_Games` and character data.
- Debug symbols and test executables.
- Build intermediates and source files.

### Installation model

1. Back up the destination DevilutionX 1.5.5 folder.
2. Extract the archive directly into that folder.
3. Permit replacement of matching executable and library files.
4. Retain legally obtained MPQ game data in the destination.
5. Run `devilutionx.exe`.

Important character files should remain backed up because v0.1.0 is a development preview.

## 13. Representative implementation history

- `b2d2d98` - Brand runtime as Diablo Oracool Edition.
- `7afd72c` - Add portable configuration and save foundation.
- `4c09fdd` - Port world and item quality-of-life features.
- `5a5d4f5` - Port character progression features.
- `b28f56b` - Port Griswold refresh and restoration services.
- `bed4b3d` - Port Griswold consumables and recharge services.
- `22fe826` - Port bulk sale and price sorting.
- `e7e9624` - Expand Griswold bulk-sale eligibility.
- `11af36c` - Correct Griswold menu-selection restoration.
- `26e0dc0` - Port Refresh Until search.
- `10a0aca`, `68f8e41`, `6033b93` - Organize, document, and enable Oracool settings by default.
- `9968f78` - Port premium generation limit controls.
- `a815da0`, `d633a34` - Add the Unique shop and correct resale eligibility.
- `4cb6ac8` - Port automatic saving.
- `2180454`, `4791aab`, `7a72ebc` - Add, correct, and accept Gold Stacks Buff.
- `921abe5`, `cf6ed89`, `9c52d0c` - Develop and finalize the fixed Pepin-potion section.
- `c15f483` - Document and package Oracool Edition v0.1.0.

## 14. Known exclusions from v0.1.0

The following discussed features are not implemented in this release:

- Corrected visible Oracool release number in the main menu.
- Respawn in Town.
- Stackable consumables and belt automatic refill.
- Extended item-data/save format.
- Rare Items.
- Procedural Buffed Uniques.
- Primal Items.
- Torment difficulty.

These belong to the separate Future Development Roadmap and must not be interpreted as v0.1.0 functionality.

## 15. Overall assessment

Oracool Edition v0.1.0 establishes a reproducible and extensively tested foundation for continued development. It successfully migrates the earlier 1.5.4 modification set, adds accepted post-migration improvements, preserves vanilla multiplayer behavior, and packages the result as a clean optimized release.

The most important engineering lessons from v0.1.0 are the value of centralized runtime limits, exact mapping between displayed store entries and original inventory items, explicit interface-return state, save-boundary tests, and practical user testing after automated verification. Those lessons directly inform the design standards for later Oracool systems.
