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

- Status: Build verified; user acceptance pending
- Scope: Single-player Griswold inventory and purchasing
- Change: Adds `Buy unique items` with an independent, identified, non-duplicate stock generated once per game. The configured stock count is clamped to 1-8 and candidates above the character's level are excluded.
- Pricing: Purchase price is the unique item's normal value multiplied by `Griswold Unique Item Price Multiplier`.
- Persistence: Purchased items disappear without replacement. Premium Refresh and Refresh Until do not touch unique stock.
- Compatibility: The shop is absent in multiplayer and introduces no save-format change.
- Build verification: The `x64-Debug` executable compiled and linked successfully on DevilutionX 1.5.5.
