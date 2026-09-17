# The Necromancer, phase N1: the class exists (v1.12.030)

**Date:** 2026-09-17 - **Branch:** renderer-32bit - **Build:** x64 Debug only - **Tests:** 799 of 799

First of the eleven phases in [[Plan - The Necromancer]]. The user's word: "go. start N1".

## What N1 is

A seventh hero class that can be created, saved, loaded and played. He fights like a bare Sorcerer: every one
of his own skills is listed and inert. Nothing of Essence, the army, curses or the new item families is in this
build - those are N2 onward.

## What was built

- **`HeroClass::Necromancer` = 6** (decision D1). The Bard keeps 4, hidden; the Barbarian keeps 5. The class
  byte in the hero file is additive, so no save breaks.
- **Data rows** in `playerdat.cpp`: 15 / 30 / 20 / 20 (the 85 points every class starts on), caps 50 / 250 / 80 /
  80, a little more life and a little less mana per point than the Sorcerer. The Sorcerer's sprite widths,
  frame counts and voice.
- **Body** (D2): `GetPlayerSpriteClass` sends him to the Sorcerer's sheets unconditionally; `CharChar` grew to
  seven. `oracool/sprite_import` knows a `necromancer` folder for the day he has art of his own.
- **Dye**, `oracool/hero_look`: measured on the exported Sorcerer sheets in all three armour tiers. The robe is
  the red ramp 232-239 (34% of the light figure, 14% heavy, 7% medium), its trim 224-231, the heavy tier's
  pure reds 136-143; skin and staff are 168-175. Robe and trim go to dark green-greys, skin to ash. These are
  own colours in true colour - the palette has no green - and each falls back to itself, so an indexed target
  draws a plain Sorcerer. One table serves all tiers. Proof sheet was made before the code
  (scratchpad `necro_proof.png`).
- **`HeroDyeId`**: the sheet cache key carried "dyed or not"; it now carries WHICH dye (1 the light Barbarian,
  2 the Necromancer), so his cached mixed sheets can never be served to a Sorcerer or the reverse.
- **Every class switch** the Sorcerer has, he has: voice bank, inventory panel, quest-book speech (9 sites in
  `objects.cpp`), shrine (+2 Magic), Slain Hero (a spell book), ear, victory film, mana potion doubling, magic
  to-hit +20, the boy's stock filter, starting look on the hero-select screen.
- **Start**: the Sorcerer's staff, two mana potions, and Firebolt at level 2 - until Teeth exists (N7) a
  Necromancer with nothing to cast is not playable. The wand replaces the staff in N9.
- **Hero select**: a "Necromancer" row under the Sorcerer. The portrait is the Sorcerer's until
  `ui_art\hero6.png` exists (the override loop already looks for it).
- **The tree**: 72 rows appended after the Monk's block as `NECROMANCER_FIRST..LAST` (506 rows in all). Three
  Diablo II pages of eighteen - Summoning, Poison & Bone, Curses - and eighteen Diablo III passives. D2 has
  ten skills a page; the other eight per page are from D3/D4 or written to fit (Command the Dead, Gather the
  Dead, Dark Mending, Bone Splinters, Blight, Decompose, Bone Storm, Frailty, Bane, Death Mark, Doom ...). Each
  description says what it will do and ends "Not yet built: awaits ..." naming the engine system. Rows that
  will be paid in Essence say so in words. The source list is scratchpad `necro_rows.js`.
- A tree icon strip slot (`ui\necro_tree_icons.png`) that does not exist yet: plates draw without icons.

## Found on the way

- `diablo.cpp`'s skill dump indexed a six-name array by class - an out-of-bounds read for a seventh class that
  no compiler flags. Fixed. It was the only such table (searched for every `[...Class)]` index).
- Three old tests assumed six classes: the RfA-12 census took him for a Monk through a `default:`, the passive
  sweep expects every passive built, and the page census listed six classes. All three now know him.
- The tests hold inert rows to the phrase "Not yet built"; his rows use it.

## Tests

Three new (`OracoolNecromancer.*`): he is class 6 on the Sorcerer's body with 85 points; he is dyed in every
tier, only the measured ramps, and the Sorcerer is not; his cache keys differ from the Sorcerer's.

## Not in this build

Portrait, silhouette, tree icons (art request A); Essence (N2); everything that makes a row do something.

## For the user's look in play

Create a Necromancer. Check: the dyed body in all three armour tiers and in town, with and without a shield
(the gear looks apply - same sheets); the four tree pages read sensibly; the hero-select preview is dyed too.
