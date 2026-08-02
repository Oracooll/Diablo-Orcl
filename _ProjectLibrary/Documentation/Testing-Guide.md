# Oracool Edition Testing Guide

## Test states

For every toggleable feature, test both enabled and disabled states after restarting the game. Unless noted otherwise, also confirm multiplayer retains vanilla behavior.

## Migration batches

### Batch A: Foundation

- Confirm `diablo.ini` is beside `devilutionx.exe`.
- Create or save a character and confirm files appear under `Saved_Games` beside the executable.
- Confirm the game starts with no existing local INI or save folder.

### Batch B: World, items, and character

- Exercise both values of every option.
- Confirm player-dropped unidentified items remain unidentified when Auto Identify Drops is enabled.
- Test all relevant Diablo and Hellfire town entrances.
- Validate stat colors, the 255 cap, save/reload, toggle transitions, and repeated Reset Stats operations.
- Validate Town Portal on a new character, an existing character, and a character with a spell level above 1.

#### Character acceptance checklist

- With `Remove Stat Limits=1`, allocate every base attribute beyond its class cap and verify allocation stops at 255; 255 is gold.
- Save and reload an over-cap character. With the option changed to `0`, verify the real over-cap value remains intact, is red, and has no `+` button.
- With `Reset Stats Button=1`, open the local character panel and click the silver `R`; it turns red while pressed.
- Verify class starting base attributes are restored and available points equal exactly `5 × (level − 1)`. Click `R` again and verify the result is unchanged.
- Confirm multiplayer shows no reset control and retains vanilla stat limits.
- With `Permanent Free Town Portal=1`, verify Town Portal appears as a memorized level-1 spell on new and existing characters, costs zero mana, and does not reduce an existing higher spell level.
- Disable the portal option and verify ordinary mana cost returns; the already-granted learned spell may remain.

### Batch C: Griswold

- Test every menu route, Back route, insufficient-gold case, full-inventory case, purchase removal, and persistence behavior.
- Verify Premium Refresh and Refresh Until do not mutate unique-shop stock.
- Test unique-shop counts 1 and 8 and multiple price multipliers.
- Test stable descending sell sorting, including equal prices and belt items.

#### Basic services acceptance checklist

- Enable `Griswold Premium Refresh`, open Premium Items, and verify a `Refresh` footer action appears above `Back`.
- Activate Refresh several times and verify the complete premium inventory changes without charging gold.
- Buy an item after refreshing and verify the correct selected item is purchased and removed.
- Disable the option and verify the Refresh action disappears; confirm it is also absent in multiplayer.
- Test `Griswold Restore Health` and `Griswold Restore Mana` separately, then together. Enter Griswold's main menu with depleted resources and verify only enabled resources silently refill.
- Confirm restoration produces no spell sound or extra menu entry, and remains inactive in multiplayer.

#### Refresh Until acceptance checklist

- Under the final `[Oracool Edition]` section, set `Griswold Refresh Until Button=1`, choose `Griswold Refresh Until Timeout Seconds`, and set `Griswold Refresh Until Item Names` to one or more exact displayed names separated by semicolons—for example `King's Sword of Haste; Awesome Plate`.
- Verify leading/trailing spaces around each semicolon-separated target are ignored and matching is case-insensitive but requires the complete item name.
- Activate `Refresh until` and verify success feedback reports the matching item and attempt count; confirm the matching premium inventory remains available for purchase.
- Use an empty target setting and verify clear feedback appears without changing stock.
- Search for an impossible name with a short nonzero timeout and verify timeout feedback appears while the last generated stock remains.
- Set timeout to `0`, search for an impossible name, and verify the operation stops at the 100,000-generation hard limit.
- Enter multiple targets and verify finding any one succeeds. Confirm the action is absent in multiplayer and when its toggle is disabled.

#### Consumables and recharge acceptance checklist

- Set `Griswold Sell Consumables=1` and `Griswold Recharge Staves=1`, then verify Griswold's expanded menu shows `Buy consumables` and `Recharge staves`.
- Open Buy Consumables and compare its current stock and prices with Adria's Buy Items screen.
- Buy both a replenishing pinned consumable and, if available, a non-pinned item; verify gold, inventory placement, and stock removal behave exactly as at Adria.
- Test insufficient gold and full inventory, then verify confirmation cancellation, `Back`, and Escape return to Griswold—not Adria.
- Test an equipped staff and an inventory staff with missing charges. Verify eligibility, price, confirmation, gold deduction, and restored charges match Adria.
- Confirm a fully charged or otherwise ineligible staff is absent, and all recharge dialog paths return to Griswold.
- Disable each option separately and verify only its corresponding menu entry disappears. Confirm both services remain absent in multiplayer.

#### Bulk sale and sorting acceptance checklist

- Set `Griswold Buy All Items=1` and verify `Sell all` appears above `Back` in Griswold's Sell Items screen.
- Prepare equipment, Adria-sellable consumables, and staves in both inventory and belt, together with gold and any quest item. Verify the expanded individual sell list contains every positive-value non-quest item, then activate `Sell all` and verify those items are removed and paid for while gold and quest items remain.
- Verify the resulting gold total equals the sum of the individual displayed sale prices.
- Test with nearly full inventory/gold capacity and verify the operation stops safely if proceeds cannot fit.
- Set `Griswold Sort Sell Items by Price=1` and verify displayed prices descend from highest to lowest across inventory and belt.
- Include two equal-price items and verify their original relative order remains stable.
- Sell items from the beginning, middle, and end of the sorted list and verify the selected original item—not a neighboring inventory item—is removed.
- Disable each option and verify vanilla behavior returns. Confirm neither feature is active in multiplayer.
- With the expanded Griswold menu active, open and close each service using both `Back` and Escape; verify the selector returns to the exact service that was closed.

### Batch D: Auto-save

- Periodic save at one minute.
- Level transition in both directions.
- One non-gold pickup and rapid multiple pickups.
- Gold-only pickup does not save.
- Successful purchase saves; cancelled and failed purchases do not.
- Notification off still saves.
- Master option off disables every trigger.
