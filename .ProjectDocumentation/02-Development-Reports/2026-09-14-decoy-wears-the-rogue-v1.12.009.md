# The Decoy wears the Rogue, as a blue ghost

2026-09-14 — v1.12.009

## Why

> "Decoy - why not use Rogue sprites operated by the mechanics behind Golem? Is that imposible?"
>
> "go ahead, use blue ghost tint"

In v1.12.008 the Decoy was the Golem, disarmed, and it looked like the Golem. It was not impossible to do better.
Monsters and heroes draw from the same kind of sheet: CL2, eight directions, stand/walk/attack/hit/death. The
Rogue's sheets are already in the player's archive.

## How

`oracool/decoy.{h,cpp}` is new. The body stays the Golem's:

- its slot, `Monsters[playerId]`;
- its brain, `GolumAi`;
- its data row and its level type.

Only what is drawn changes.

- **Loading at the cast.** `MakeDecoy` loads the Rogue's own sheets for the gear she wears at that moment:
  - dungeon stand `as`, walk `aw`, attack `at`, hit `ht`;
  - death `dt`, which is unarmed, as `LoadPlrGFX` asks;
  - in town, stand `st` and walk `wl`.

  The frame counts come from `PlayersAnimData` and the widths from `PlayersSpriteData`, which is what the player
  code uses. Each sheet is asked of the archive with `FindAsset` first (the v1.11.059 block-sheet lesson). A missing
  sheet falls back to the stand, and a missing stand leaves the Golem as it was.
- **Handing them out.** `GetScaledAnim` is the single override both monster sprite binders ask
  (`NewMonsterAnim` and `Monster::changeAnimationData`). It returns the decoy's animation first. The draw centres
  on the sprite's own width, so 96 and 128 px sheets sit on their tile.
- **The blue ghost tint.** Every palette colour maps to the blue ramp (`PAL16_BLUE`) by its luminance, shifted
  two shades light. It is set as the monster's translation, so `DrawMonster` lights it like any translated
  ordinary monster, and the ghost still darkens in shadow.
- **Lifetime.**
  - `AddGolem` clears the decoy, so a real summon wears its own sprites. The Decoy cast dresses the slot again right
    after.
  - `InitGolems` clears every decoy on a new level.

Nothing is shipped. The sheets come from the player's own archive at runtime.

## Not verified here

This build was not run in the game.

- **Movement.** The Golem's walk timing is data-driven. If the double slides on its walk, the walk frames need
  matching to the Golem's.
- **Death.** The Rogue's death sheet is 20 frames, so a dying double plays her full fall.

## Tests

- `OracoolCensusNotes.NothingIsADecoyUntilOneIsCast`: no monster is a decoy before a cast, including one outside
  the monster table.
- Debug: 780/780 tests pass.
