# 2026-10-02 - Dev notes and audit round 68 (v1.12.315-316)

**Date:** 2026-10-02. Debug only. The user closed the game, asked for the dev notes to be applied and rebuilt, then for the non-stop audit to go on.
- **v1.12.315 (1ea1c3e1):** 904 tests pass.
- **v1.12.316 (17a829d9):** 904 tests pass. oracool.mpq repacked: the Ember Mine fringe fix, the 35% furnace and the doubled meteor frames.

## Dev notes (16, archived in `development-archive.md`)
The three UI notes were coded by a helper and reviewed here. None of them has been seen on screen yet.

- **Vendor and artisan buttons:** each stands in the 1x1 ring/amulet slot frame, grown to fit it, with the inventory slots' drop shadow. Pressed, the face and frame sink and the shadow shrinks 1px each side.
  - New in `grid_bezel`: `DrawButtonBezel` and `DrawButtonSlotGround`.
  - Applied in `shop_grid`, `workshop` and `levski_roar`.
  - The Cube's painted tesseract and the close X buttons stay unframed.
- **Book spells in the tree:** a row that is a book spell draws in the spells sheet's blue once learned by book, and red until then (`SkillPlateTint::Book`).
- **Staves:** their tooltips read "Spell: <name>  Charges: x/y".
- **Cold lights:**

  | Spell | Light radius |
  |---|---|
  | Ice Needle's arrow | 1 |
  | Ice Lance's arrow | 2 |
  | Each Brittle Ground patch | 2 |
  | Frozen Sentinel | 3 |
  | Each Blizzard shard | 1 |
  | Frost Nova | 5 |
  | Absolute Zero's vortex | 4 |
  | Chill Touch | 2 |
  | Each Whiteout flame | 1 |
  | The ice armours, on the hero | 4 |

  Flying art bolts now carry their light (`ProcessAcidJavelin`).
- **Lightning Ball** (renamed from Ball Lightning): spins four times as fast (`_miAnimAdd` 4).
- **Furnace Mouth:**
  - The furnace is drawn at 35%, in `tools/BuildFireSheets.py`. Its flame starts at the shared floor point, so it still meets the head and keeps its three tiles.
  - Each second, its head turns to the nearest enemy within 3 tiles and fires: the mark, plus anything on the line toward it. With no enemy in reach it holds its fire.
- **Meteor:**
  - The fall has 20 frames: each in-between moves the rock half way along its path, and it plays one frame a tick.
  - The impact has 28 frames, with cross-faded in-betweens; the burn loop now runs from frame 21 at 2 ticks a frame.
  - Built by `tools/BuildMeteorFrames.py` from the delivered sheets, kept in `Resources/.../Fire Spells/Meteor`.

## Audit round 68
- **Regression review of v1.12.314:**
  - The conversion clear on death is undone. It had flipped a dying converted monster's shots in flight.
  - The Luminous light is instead kept at `DeleteMonster`, when the monster is converted.
  - Summons are clamped to the last clear tile toward a cursor past a wall, never refused. The refusal had blocked live companions' refreshes and clicks beside walls.
  - A minion hunk that did nothing is reverted.
- **One price per swing:**
  - `LatchClassMeleeSwingPrice`, `LatchRfa12SwingPrice` and `PaidAtFront` answer "is the skill paid" once per hit frame.
  - Before, Rage or mana gained between the front blow and the settle (a kill's Bloodcall, Weapons Master, a mana steal, Righteousness) charged a swing that had carried no bonus.
  - A mana settle never goes below zero (Sacrifice or Peril under Mana Shield).
  - No RfA-12 bonus on Whirlwind's blows.
  - Test: `OracoolRage.ASwingIsPricedOnceAtItsHitFrame`.
- **Perched gargoyle:** lifting one is no landing, so it costs no Rage and triggers no passives (Hammer of the Ancients paid 10 Rage to wake one).
- **Inventory (v1.12.316):**
  - A readied scroll keeps its binding while its last stack is in hand.
  - A held stack dropped on a full stack swaps with it.
  - Gold top-off no longer overflows.
  - A scroll read from pages 2-10 is not taken from page one's same index.
  - Ctrl+click beside an artisan's bench or a store drops nothing.
  - A level change that closes the stash keeps a held item with nowhere to go in hand. Before, it was dropped on a floor about to be freed, and lost.
- **Quests (v1.12.316):**
  - Zhar dropped for want of a library keeps his type in the level's list, marked in `_qvar2`. A revisit had shifted every saved monster's type index.
  - The waypoint sigil no longer settles over a floor item.
  - The Divine shrine reads the rift's depth (`ShrineFloor`).

## Left open
- **Gamepad on pages 2-10:** the pad's shift-move and equip don't work there.
- **Gamepad on the belt:** the pad's belt slots may not line up with the HUD's. This needs a screenshot.
- **Corrupt level files:** `LoadLevel` trusts every count in the file.
- **Oil to-hit:** the display of Oil of Accuracy's to-hit.
- **Zeal:** a missed first swing loses its burst. This may be the intended design.
- **Belt refill:** whether the belt refilling a slot the player filled with scrolls is allowed. This is the user's call.
