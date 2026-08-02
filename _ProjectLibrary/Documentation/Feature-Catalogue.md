# Oracool Edition Feature Catalogue

This catalogue reconstructs the stable DevilutionX 1.5.4 prototype from its development history. It is the authoritative scope for the 1.5.5 migration.

## Foundation

| Prototype | Feature | Configuration | Required behavior |
|---|---|---|---|
| v0.1 | Portable saves | Always active on Windows | Store saves under `Saved_Games` beside the executable. Existing saves are not moved automatically. |
| v0.2 | Local configuration | Always active on Windows | Store `diablo.ini` beside the executable. |
| v0.3 | Oracool framework | Internal | Central edition identity, option category, and single-player guard. |

## World, items, and character

| Prototype | Feature | Default | Required behavior |
|---|---|---:|---|
| v0.5 | `Unique Item Drop Multiplier` | `1` | Single-player multiplier clamped to 1–100; final probability cannot exceed 100%. |
| v0.6 | `Unlock All Town Entrances` | `0` | Unlock Catacombs, Caves, and Hell; also Hive and Crypt in Hellfire. Cathedral unchanged. |
| v0.7 | `Permanent Infravision` | `0` | Continuously grant infravision in single-player without consuming an item or inflating a duration counter. |
| v0.8 | `Auto Identify Drops` | `0` | Identify newly generated world drops; do not change the state of items deliberately dropped by the player. |
| v0.9 | `Remove Stat Limits` | `0` | Permit allocation to 255. Preserve real over-cap values when disabled, prevent further allocation, and display them red. Display 255 gold. Multiplayer stays vanilla. |
| v0.10 | `Reset Stats Button` | `0` | Show an `R` on the local character panel. Reset base stats to class starts and set distributable points to exactly `5 × (level − 1)`. Repeated resets are idempotent. Permanent non-level stat gains are lost and not refunded. |
| v0.11 | `Auto Pickup Range` | `1` | Clamp to 1–10 and extend only existing enabled auto-pickup categories, searching nearer tiles first without duplicate requests. |
| v0.18 | `Permanent Free Town Portal` | `0` | Give new and existing single-player characters memorized Town Portal level 1 without lowering a higher level; memorized casting costs zero mana. Disabling restores normal mana cost without removing the spell. |

Implementation status (DevilutionX 1.5.5): all features in this table are build-verified. The stat-limit, reset, and Town Portal features await user acceptance testing; earlier world/item features also remain recorded as user-acceptance pending.

## Griswold

| Prototype | Feature | Default | Required behavior |
|---|---|---:|---|
| v0.4 | `Griswold Premium Refresh` | `0` | Add free Refresh to the six-item Premium shop in single-player. |
| v0.12 | `Refresh Until Button` | `0` | Search internally for exact, case-insensitive full names; semicolon-separated targets; trimmed whitespace; configurable timeout; 100,000-generation hard cap; retain the successful or last inventory; clear result/error feedback. |
| v0.13 | `Griswold Buy All Items` | `0` | Buy every ordinary item with a valid sell value while rejecting gold, quest items, Lazarus's staff, and unsellable/zero-value items. |
| v0.14 | `Griswold Sell Consumables` | `0` | Add Buy Consumables using Adria's generated inventory, purchase rules, and stock. Back returns to Griswold. |
| v0.15 | `Griswold Recharge Staves` | `0` | Reuse Adria's eligibility, pricing, confirmation, payment, and recharge behavior. Back returns to Griswold. |
| v0.16 | `Griswold Restore Health` / `Griswold Restore Mana` | `0` | Independently and silently refill enabled resources whenever Griswold's main menu opens. No new menu entries or sounds. |
| v0.17 | Unique shop | Disabled | Separate stock of 1–8 eligible, identified, non-duplicate unique items. Configurable price multiplier based on normal sell value. Purchased items disappear without replacement; Premium Refresh does not affect this stock. |
| v0.19 | `Griswold Sort Sell Items by Price` | `0` | Stable descending-price sort across eligible inventory and belt items; selling must remove the correct original item. |
| v0.20 | Premium limit controls | `0` | Independently ignore affix quality-level limits and premium price rejection while retaining item-type compatibility, good-affix rules, mode restrictions, and base-item progression. |

## Auto-save

Prototype v0.21 provides single-player automatic saving with these settings:

- `Auto Save=0`
- `Auto Save Interval Minutes=5` with choices 1, 2, 3, 5, 10, 15, 30, and 60.
- `Auto Save on Level Change=1`.
- `Auto Save on Item Pickup=1`, excluding gold and failed pickups.
- `Auto Save on Store Purchase=1`, excluding cancelled or failed purchases.
- `Auto Save Item Delay Seconds=3` with choices 0, 1, 2, 3, 5, 10, 15, and 30.
- `Auto Save Notification=1`.

Rapid pickups and purchases are debounced into one save. Pending work waits until saving is safe, and manual or completed saves reset pending timers to avoid duplicate writes.

## Global constraints

- Gameplay changes are single-player only unless explicitly stated otherwise.
- Every gameplay feature is independently configurable.
- Vanilla behavior is the default.
- Existing engine routines are reused where practical.
- No save-format change is introduced by the migration unless separately approved.
