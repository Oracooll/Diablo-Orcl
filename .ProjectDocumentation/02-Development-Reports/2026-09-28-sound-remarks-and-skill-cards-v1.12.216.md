# 2026-09-28 - New sound-page picks, Hammer of Faith's burst, the per-class Skill Cards pages (v1.12.216)

**Date:** 2026-09-28. Debug only. The user: "go through my comments on Vanilla Sound Stand-ins and on Diablo Orcl Visual FX Schedule for any new ones and apply them ... Also - i want you to make one artefact per hero class ..."

## New comments

**Visual FX Schedule:** nothing new. The six Paladin comments were all applied in v1.12.215.

**Vanilla Sound Stand-ins** (db `choices` and `remarks`, compared with the copy read for v1.12.213):

| Place | Now |
|---|---|
| Hammer of Faith, impact | "None of the above", remark "Nova. Add Holy Bolt explosion on impact. Scaled 100%. Tinted blue." → `sfx\misc\nova.wav`, plus the burst below |
| Judgment, impact | option 1, Holy Bolt (`sfx\misc\holybolt.wav`); its "Shield clank" remark was cleared |

Where each change went:
- **The picks:** the scratchpad's mkpicks.js resolves the Hammer of Faith remark, and `tools/skill_sound_picks.json` was regenerated (106 picks).
- **The cue table:** `Source/oracool/skill_sounds_data.inc` was regenerated (475 cues).
- **The burst:** `rfa12_actives.h` exports `DrawHolyBurst(player, tile, half, HolyBurstColour)` over v1.12.215's `HolyBurst`. `ApplyHammerOfFaith` (`paladin_melee.cpp`) draws it on the primary target at full size in blue, beside the impact cue.
  - Both the cue and the burst play only when the shockwave splashes: at least one neighbour, and the mana paid. That was already the rule for the cue.

## The Skill Cards pages

There is one private page per class, with db collection `picks`:

| Class | Page |
|---|---|
| Paladin | https://claude.ai/artifact/7fRxzHAUUvEd1FNPVQEd3E |
| Barbarian | https://claude.ai/artifact/HWSHQEZ9H277enesYzJhHd |
| Sorcerer | https://claude.ai/artifact/NcvyWN5QYXt8xE866Eibjn |
| Rogue | https://claude.ai/artifact/2LSKG1XPW4qRm7w9WEwewH |
| Monk | https://claude.ai/artifact/2pnP9zaCj8EJzzPmvTBiw9 |
| Necromancer | https://claude.ai/artifact/SGx15Ae8KYnpw1oZ962gQP |

**The data:**
- **Exporter:** the new test `OracoolPreview.DISABLED_ExportClassSkillCards` writes each class's three tree pages in `BuildClassTreePage` order. Each row carries its tier, column, level, kind, rank cap, spell, and the graphics that spell's missiles draw.
- **Vanilla sheets:** the test also writes every vanilla missile sheet, 52 including the monsters' own, for the picker.

**Each card shows:**
- the skill's animation(s) as the Visual FX Schedule rendered them, or as a vanilla sheet re-drawn in the browser with the game's Tint::Hue and ScaleClxList maths;
- pickers for vanilla sheet, scale and tint, starting at the game's values;
- the sound places (Cast/Start, Impact/Arrive, Loop, plus Stop/Learn), with play buttons and a replace picker over the 124 exported vanilla sounds;
- a comment box per asset.

**Built by:** scratchpad `cards/` (data.js, build.js, template.html).

## Test

Debug build and ctest: 878/878 passed.
