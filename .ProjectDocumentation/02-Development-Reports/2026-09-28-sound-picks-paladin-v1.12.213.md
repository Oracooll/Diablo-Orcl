# 2026-09-28 - The first sound picks: the Paladin, and three Barbarian cues (v1.12.213)

**Date:** 2026-09-28. Debug only. The user: "check the sounds stand-ins artefact and process what i have decided on so far."

## What was decided

The page (Vanilla Sound Stand-ins, db collection `choices`) held 106 picks:
- all 103 Paladin places;
- three Barbarian ones: Ancestral Call's cast and arrival, and Find Potion.

**How the picks break down:**
- **Option 1 (77 picks):** the sound already playing. No change, but each is now pinned.
- **A different candidate (22 picks).**
- **"None of the above" (7 picks):** each has a remark naming the sound wanted (collection `remarks`):
  - Blessed Shield cast and Resist Fire start: "Generic cast (cast2)".
  - Sanctity start: "Generic cast (cast8)".
  - Judgment and Smite impacts: "Shield clank".
  - Ancestral Call cast: "regular magic cast", which is vanilla's spell cast `IS_CAST2` (cast2).
  - Resist Lightning's loop: "none", so it stays silent.

The remark on Redemption's start adds a second sound: "on successful redemption from a corpse during the resurrection animation also play Generic cast (cast8)". `warcries.cpp` now plays `IS_CAST8` beside `LS_RESUR` at each corpse Redemption consumes.

## How they are applied

- **`tools/skill_sound_picks.json`:** the picks, keyed `<class>.<Skill>.<Event>`, each with a vanilla path, or null for silence, and where it came from.
- **`tools/GenVanillaSkillSounds.js`:** reads the picks file before its rules. A picked slot plays its file whatever the rules say, and a pick that names no slot stops the run.
- **Regenerated `Source/oracool/skill_sounds_data.inc`:** 475 cues from 62 vanilla files; 29 cues changed.

## Changed cues

| Place | Was | Now |
|---|---|---|
| Ancestral Call, cast / arrive | golum / guard | cast2 / resur |
| Find Potion, cast | invgrab | megas1 (the Slayer's bellow) |
| Blessed Hammer, cast / impact | cast6 / holybolt | cast2 / blsimpt |
| Blessed Shield, cast | swing | cast2 |
| Charge, cast / impact | swing2 / blsimpt | invaxe / invshiel |
| Fist of the Heavens, impact | elecimp1 | nova |
| Hammer of Faith, impact | blsimpt | invshiel |
| Heaven's Descent, cast | holybolt | cast8 |
| Holy Freeze, start | shatter | nova |
| Holy Shock, start | elecimp1 | lning1 |
| Judgment, impact | holybolt | invshiel |
| Prayer, start | fountain | cast8 |
| Redemption, start | fountain | holybolt (plus cast8 at every corpse) |
| Resist Fire, start | mshield | cast2 |
| Resist Lightning, start | mshield | lmag |
| Retaliation, start | sting1 | holybolt |
| Sacrifice, impact | acids2 | scurimp |
| Salvation, start | holybolt | cast8 |
| Sanctity, start | holybolt | cast8 |
| Smite, impact | blsimpt | invshiel |
| Thorns, start | sting1 | mshield |
| Vengeance, cast | cast8 | swing |
| Vigor, start | mshield | ethereal |
| Warding Light, start | holybolt | cast8 |
| Wrath of the Heavens, impact | elecimp1 | cbolt |

## The page

The page is not rebuilt. Its option numbers index each place's candidate list, and that list starts with the sound that was playing, so a rebuild would move the saved picks onto different sounds. For the places above, option 1 now describes the old sound. The picks are pinned in the JSON either way.

## Test

Debug build and ctest: see the changelog's v1.12.213 line.
