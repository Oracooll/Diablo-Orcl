# Gear looks for every class, and two INI switches

2026-09-17 — v1.12.028

## Why

> "i think the results are great. let's apply these combos across all classes."
> "and make this "Shields Sprites Swap" an INI option - On/Off"

## Measured first

The Rogue's, the Sorcerer's and the Monk's sheets were exported (255 / 255 / 291) and looked at. All
three were drawn, like the Warrior, with a shield per armour tier:

| class | light | medium | heavy |
|---|---|---|---|
| Warrior (and Barbarian) | round red buckler | steel heater, cross | blue heater, lion |
| Rogue (and Bard) | small dark round shield | red heater | gold and black shield |
| Sorcerer | black round shield | white heater, ankh | the same, larger |
| Monk | gold square shield | blue tower shield | larger blue tower shield |

Same-ramp twin scores, sword against sword-and-shield:

| class | result |
|---|---|
| Monk | 82-95% everywhere - the cleanest sheets in the game |
| Sorcerer | 72-89%, bar the light tier's casts (49-57%) |
| Rogue | patchy: 61-79% for light and medium, and her HEAVY standing sheet scores 40% |

## What changed

- `WantsMixedSheet` takes Warrior, Rogue, Sorcerer and Monk sprite classes (the Barbarian and the Bard
  wear the first two, so all six heroes are covered). Nothing else in the mixer was class-specific except
  one line: "seen from behind" tested for the Warrior's BLUE shield face, and now tests for any face
  colour that is not grey.
- **A look is worn whole or not at all** (`LookDeclinedByCore`). The mixer declines sheet by sheet, and
  for the Rogue that meant a Tower Shield while she walked and her own buckler the moment she stood
  still - worse than no swap. Standing and walking are now the core of a look: if either is known to
  have been declined, every animation of that look is declined. Town has its own pair. The core sheets
  are requested and built first, so they are known before anything else lands.
- Two INI options, both default on, both in the settings menu:
  **Shields Sprites Swap** and **Swords Sprites Swap** (the user asked for the first; the second is its
  twin, because leaving the rougher of the two features without a switch made no sense). `GearLookFor`
  reads them, so off means "the tier's own look" for that piece. They take effect on the next change of
  gear or level.
- `CacheVersion` 5.

## What each class gets

| class | shield swap |
|---|---|
| Warrior, Barbarian | every combination, as v1.12.027 |
| Sorcerer | every combination standing, walking, attacking, hit, in town; the light tier's casts keep their own |
| Monk | every combination, nearly every animation |
| Rogue, Bard | light and medium shields swap freely. Anything involving the HEAVY tier's sheets - heavy armour, or a Tower/Gothic Shield - swaps only with a mace in hand, not a sword, because her heavy sword-and-shield standing sheet is not a twin of her heavy sword one |

## Verified how

`oracool_sprite_export <class> --mix` for each class; contact sheets `contact_rogue.png`,
`contact_sorceror.png`, `contact_monk.png` - all five cross-tier combinations standing, eight facings,
plus walk, attack and town where they mix. No heavy-body artefacts in any of them. Mixed / declined per
class before the whole-look rule: Rogue 26 / 73, Sorcerer 52 / 47, Monk 71 / 28. 795 tests pass. Not
run in the game.
