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

### Batch D: Auto-save

- Periodic save at one minute.
- Level transition in both directions.
- One non-gold pickup and rapid multiple pickups.
- Gold-only pickup does not save.
- Successful purchase saves; cancelled and failed purchases do not.
- Notification off still saves.
- Master option off disables every trigger.
