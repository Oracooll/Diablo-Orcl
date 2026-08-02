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

- Status: Build verified; user acceptance pending
- Features: Unique Item Drop Multiplier, Unlock All Town Entrances, Permanent Infravision, Auto Identify Drops, and Auto Pickup Range.
- Compatibility: All gameplay behavior is guarded to single-player. Defaults preserve vanilla behavior.
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

- Status: Build verified; user acceptance pending
- Scope: Single-player save scheduling
- Change: Adds periodic saving plus successful level-change, non-gold item-pickup, and store-purchase triggers.
- Debouncing: Rapid pickups and purchases restart one configurable delay and produce a single save.
- Safety: Pending saves wait until gameplay is active, the player is alive, menus and stores are closed, no item is held by the cursor, and no demo is running or recording.
- Reset behavior: Every completed save, including a manual save, resets the interval and clears pending work to prevent duplicate saves.
- Notification: Automatic saves optionally show the standard brief `Game Saved` message.
- Compatibility: Multiplayer behavior and the save format are unchanged.
