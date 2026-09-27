# 2026-09-27 - Every ChatGPT sound out, vanilla stand-ins in (v1.12.209)

**Date:** 2026-09-27. Debug only. The user said: "remove all chatgpt sounds from the game. they are no good. replace with vanilla sounds per your decision. make an artefact with all places we use vanilla sounds instead of our own sound assets. i need a play button to hear the placeholder vanilla sound and a remarks text box to input my opinion."

## What left the game

- **Files:** all 485 remaining delivered WAVs are gone from `Packaging/resources/oracool_assets/sfx`, which now holds no sound files.
  - 471 skill cues: Paladin 138, Sorcerer 97, Rogue 79, Monk 70, Bard 38, Necromancer 49.
  - 14 UI sounds: `sfx/ui`, from RfA-03/04/18/19/20 and the set stinger.
  - The Barbarian's 58 went in v1.12.208.
- **Review copies:** they stay in `Resources\<Class> Sound Assets`.
- **The old generator:** `tools/GenSkillSounds.ps1` is marked superseded and must not be run over the new table.

## What plays instead

**Skill cues.** `tools/GenVanillaSkillSounds.js` writes `Source/oracool/skill_sounds_data.inc`. The engine is unchanged: `SkillSound` rows are archive paths, so a vanilla path loads from the player's own `diabdat.mpq` / `hellfire.mpq` like any engine sound.
- **The slots:** all 529 (skill, event, class, page), saved as `tools/skill_sound_slots.csv` from the pre-removal table, including the Barbarian's.
- **The rules:** a default per class page and event, refined by keywords in the skill's name. The first matching keyword wins.
  - fire: Firebolt and its impact;
  - lightning: Lightning, with Charged Bolt, Nova and Teleport where they fit;
  - cold: Stone Curse to cast, Shatter on impact;
  - bows: bow shot and arrow hit;
  - bone and poison: Bone Spirit and Acid;
  - summons: Resurrect and Golem;
  - holy: Holy Bolt;
  - melee: weapon swings with a Blood Star impact;
  - Barbarian war cries: each a monster's roar (Horned Demon, Overlord, Black Knight, Skeleton King, Slayer, Diablo, Gargoyle, Butcher, Fallen);
  - every Learn cue: reading a book.
- **Aura loops:** the 54 are dropped. Vanilla has no seamless loop, and auras keep their start and stop cues.
- **Result:** 475 cues from 59 distinct vanilla files. `OracoolSkillAssets.EveryVisibleSkillHasItsIconSoundsRingAndMissileArt` mounts the game archives and found every one.

**UI events** (`skill_sounds.cpp`): `UiEventPaths` now names vanilla files, so no event goes silent. Several callers (milestone, encounter cleared, map unseal, rift close) had no fallback of their own.

| Event | Vanilla sound |
|---|---|
| Salvage | anvil |
| Transmute, Cube transmute | cauldron |
| Shard imbue | magic item |
| Socket | gem set down |
| Runeword complete | magic item 2 |
| Milestone | shrine chime |
| Encounter cleared, set complete | quest done |
| Map unseal | scroll |
| Signet use | ring |
| Rift open | the Town Portal's cast |
| Rift close | fade-out |
| Cube open | Hellfire's lid |

`EveryUiEventSoundPathSurvivedTheCompiler` now accepts `sfx\misc\` and `sfx\items\`, and mounts the game archives to find them.

## The review page

"Vanilla Sound Stand-ins", https://claude.ai/artifact/BynAUZsPnJ1a5TNtGYsEaL (private):
- **Contents:** all 529 skill slots and the 14 game events, grouped by class and page.
- **Each row:** the skill, the event, the vanilla sound, a play button and a remarks box.
- **Remarks:** saved to the page's database (`db`, collection `remarks`, one document per place), so Claude can read them back.
- **Audio:** the 68 files are the player's own, written out by `OracoolPreview.DISABLED_ExportVanillaSounds` through the engine's reader. diabdat encrypts each sound under its name, and `tools/oracool_mpq_extract.exe` does not decrypt, so its output was ciphertext. Four are Hellfire's (`fballbow`, `sting1`, `nestxpld`, `cropen`). They sit on the private page only and are not in the repository or any archive.

## Tests

v1.12.209 builds clean; 877 of 877 pass; oracool.mpq repacked, with no sounds in it now.
