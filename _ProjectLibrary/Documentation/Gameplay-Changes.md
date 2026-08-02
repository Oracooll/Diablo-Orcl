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

## OE-007: Pepin items in Griswold's Buy Consumables

- Status: Build verified; awaiting user acceptance
- Scope: Single-player Griswold consumables store
- Change: Griswold's existing `Buy consumables` list now combines Adria's generated stock followed by Pepin's generated stock.
- Purchasing: Every entry retains its original price, requirements, inventory-placement checks, identification behavior, and vendor-specific purchase handling.
- Replenishment: Adria's first three pinned items and Pepin's first two single-player pinned items remain replenishing. Purchased generated entries are removed from their original vendor stock.
- Navigation: Confirmation, cancellation, insufficient-gold, insufficient-room, Back, and Escape paths remain inside Griswold's interface.
- Compatibility: Adria's and Pepin's own stores remain unchanged. The combined list is absent in multiplayer and introduces no save-format changes.
- Configuration: Uses the existing `Griswold Sell Consumables=1` setting and remains enabled by default.
- Verification: The complete Debug build succeeds, combined stock ordering has focused automated coverage, and all 216 automated tests pass.
