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

### Batch C: Griswold

- Test every menu route, Back route, insufficient-gold case, full-inventory case, purchase removal, and persistence behavior.
- Verify Premium Refresh and Refresh Until do not mutate unique-shop stock.
- Test unique-shop counts 1 and 8 and multiple price multipliers.
- Test stable descending sell sorting, including equal prices and belt items.

### Batch D: Auto-save

- Periodic save at one minute.
- Level transition in both directions.
- One non-gold pickup and rapid multiple pickups.
- Gold-only pickup does not save.
- Successful purchase saves; cancelled and failed purchases do not.
- Notification off still saves.
- Master option off disables every trigger.

