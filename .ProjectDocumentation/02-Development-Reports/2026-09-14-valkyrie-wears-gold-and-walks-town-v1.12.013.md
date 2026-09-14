# The Valkyrie wears gold, and walks the town

2026-09-14 — v1.12.013

## Why

> "valkyrie is producing a regular golem. fix it."
>
> "i also want to be able to cast this skill in town"

## A Valkyrie, not a Golem

The Decoy's machinery (v1.12.009) already dresses the golem slot in the Rogue's own player sheets. It is now a
shared hero-sheet loader with three users:

| Summon | Sheets | Tint |
|---|---|---|
| Decoy | the caster's current gear | blue ghost (blue ramp, 2 shades light) |
| Valkyrie | the Rogue in **heavy armour with sword and shield**, whatever the caster wears | **gold** (yellow ramp, 3 shades light) |
| Town Valkyrie | the same, town stand and walk sheets | gold |

- **The dungeon Valkyrie.** After the Golem spawns, the cast calls `MakeValkyrie`. Her body, AI, damage and life
  stay the Golem's; only what is drawn changes, through `GetScaledAnim` as for the Decoy.
- **Missing armour.** If the archive lacks the heavy sheets, she steps down to medium, then light.
- **Translations.** The two tints now come from one `RampTranslation(ramp, lightest)`.

## In town

A monster cannot stand in town. There is no golem slot (v1.12.012), and town's `dMonster` holds the **townsfolk's**
indices: a golem written there would draw as a townsperson and answer clicks as one. So in town the Valkyrie is a
companion, separate from the monster system:

- **Casting.** `SummonTownValkyrie` places her on the nearest free walkable tile to the cursor, within 5 tiles.
  A second cast replaces the first.
- **Following.** `ProcessTownValkyries`, called from `GameLogic`'s town branch, runs each tick:
  - she stands and animates, and turns to face her Rogue;
  - once the Rogue is more than 2 tiles away she walks a tile at a time toward her, taking 8 ticks a step and trying
    two turns either side of straight;
  - past 14 tiles (a waypoint, a portal) she reappears beside her.
- **Drawing.** `DrawTownValkyries` runs in the tile loop right after the players, offset along her step, centred like a
  player's sprite, in the gold translation.
- **Leaving.** `InitLevelMonsters` (every level load, town included) sends her away.
- **Combat.** She never fights. There is nothing to fight in town, and damage is suppressed there anyway.

## Tests

- `OracoolCensusNotes.TheTownValkyrieOnlyAnswersInTown`: no companion is called outside town, out-of-range ids are
  never one, and clearing empties them. The sheets need the player's archive, so drawing is not tested headless.

## Not verified here

This build was not run in the game. Worth a look:
- **Tint.** Whether the gold reads as a Valkyrie.
- **Town walking.** Whether the walk steps smoothly or slides.
- **Drawing order.** She is drawn with the tile she is leaving, so a wall may cut her feet mid-step.
- **Dungeon walking.** The walk timing is still the Golem's, the same open question as the Decoy's.
