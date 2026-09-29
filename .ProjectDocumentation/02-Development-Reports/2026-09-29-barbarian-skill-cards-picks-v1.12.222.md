# 2026-09-29 - The Barbarian Skill Cards page's picks (v1.12.222)

**Date:** 2026-09-29. Debug only. The user: "Check my barb notes and apply".

The Barbarian Skill Cards page (db `picks`) held 40 documents:
- 15 animation picks (Ground Stomp's unchanged);
- 25 sound picks;
- 3 comments: Double Swing, Frenzy and a Whirlwind redesign.

The three Whirlwind decisions the note left open were asked and answered:
- **Control:** hold the right button to spin.
- **Cloud:** "the animation in the magic cast sprite sheet".
- **Hit rate:** every 0.25 s.

## Found on the way: melee skills never played their cues

The cue table has Cast and Impact cues for every class melee skill: Bash, Stun, Double Swing, Frenzy, Whirlwind, Leap, Zeal, Smite, Sacrifice and the rest. No code played them:
- **Cast:** only Hammer of Faith replaced the swing's whoosh with its own Cast cue (`PlayArmedSwingCue`).
- **Impact:** only the RfA-12 swings played theirs.

Now:
- **Cast:** `PlayArmedSwingCue` answers for every armed melee skill through `ArmedMeleeSpell`: the Paladin latch, the class-melee latch, the RfA-12 latch and Weapon Throw. The Cast cue replaces the whoosh, one sound, as spells do with IS_CAST2.
- **Impact:** class melee skills (`melee_skills.cpp`) and the Paladin's (`ApplyMeleeSkillOnHit`, Hammer of Faith excepted) play their Impact cue on a landed blow.
- **Leap and Leap Attack:** play both cues when they leap.
- **Weapon Throw:** its thrown weapon plays Impact where it hits (`missiles.cpp`).
- **Ground Stomp:** plays Impact when it stuns anything.

## Sounds

`tools/skill_sound_card_picks.json` holds the class pages' sound picks. It is built by the scratchpad's `cards/mkcardpicks.js` from the page's db and its built data. `GenVanillaSkillSounds.js` merges it over the sound page's picks.

Six sound places had no slot yet and were added to `tools/skill_sound_slots.csv`, now 535 slots:
- Backhand, Cleave, Rend, Clasp of Ruin and Hammer of the Ancients, Cast;
- Ground Stomp, Impact.

The regenerated table has 480 cues and 131 picks. The changes:
- **Holy Bolt impact:** Backhand, Bash, Clasp of Ruin, Cleave, Double Swing, Leap, Rend and Stun.
- **Cast:**
  - Backhand, Cleave, Hammer of the Ancients and Rend: the weapon swing;
  - Clasp of Ruin: the heavy swing;
  - Frenzy and Weapon Throw: sting1;
  - Leap and Leap Attack: cast2;
  - Seismic Slam: the firebolt;
  - Whirlwind: the flame wave.
- **Other impacts:** Leap Attack the flame wave; Weapon Throw the blood star impact; Whirlwind the axe; Frenzy silent.
- **Ground Stomp:** Holy Bolt for both its Cast and its Impact.

## Animations

| Skill | Now |
|---|---|
| Backhand | its arc at 50%, fire orange (`hue::FireOrange`) |
| Cleave | its arc at 50%, Paladin gold |
| Rend | vanilla Blood Star Red Explosion at 50%, on the struck enemy (was its ChatGPT strike) |
| Clasp of Ruin | vanilla Blood Star Blue at 50% (was its ChatGPT sheet) |
| Hammer of the Ancients | vanilla Blood Star at 50% (was its ChatGPT sheet) |
| Seismic Slam | its Fire Wall flames at 50%, still on the floor |
| Bash, Concentrate | Holy Bolt burst 25%, infrared, on a landed blow |
| Stun, Double Swing | Holy Bolt burst 25%, vanilla colours |
| Frenzy | Holy Bolt burst 50%, infrared |
| Leap, Leap Attack | Holy Bolt burst 150%, pale warm, where he lands |
| Whirlwind | Holy Bolt burst 25%, pale warm, on every enemy a spin strike lands on |

- **Blood Star sheets:** they are the monsters' own and load only with them. `LoadMonsterOwnedArt` loads one on first use.
- **Plain bursts:** `HolyBurst` treats a tint of 0 as vanilla's colours.

## Double Swing and Frenzy: two swings to a click

Note: "This attack should make two swings in the span of one attack". Their second blow was an instant extra strike inside one swing. Now it is a second swing, Zeal's chain for these two (`melee_skills.cpp`):
- **First swing:** arms the chain, landed or not.
- **Skip frames:** each swing skips `ClassMeleeSwingSkipFrames` (half its windup, keeping 3), and the first hands over at its blow (`TryContinueClassMeleeChain`, beside `TryContinueZealChain` in `DoAttack`). Two swings take about one attack's time.
- **Damage:** the chained swing deals the extra blow's share. Double Swing's is 75%, +5% a rank, of the first blow; Frenzy's is 100%.
- **Rage:** generators earn Rage on each landed swing.

## Whirlwind (`oracool/whirlwind.h/.cpp`, new)

- **Right button only:**
  - The skill menu dims it on the left, and picking it there readies it on the right.
  - The Abilities window readies it on the right from either button.
  - A left hotkey binding of it readies nothing.
  - A saved left binding of it is not restored.
- **Hold to spin:** pressing the right button with Whirlwind readied starts it, needing 5 Rage (`RageCost`). It spins until the button is let go, the Rage runs out, he dies or leaves the level (`ClearRfa12State`).
- **Rage:** 5 a second while it spins (a point every 4 ticks). The tooltip says "Rage: 5 a second".
- **Glide:**
  - He moves toward the cursor through the engine's own walk (`CMD_WALKXY`) at run speed (`StartWalkAnimation`, beside the run toggle).
  - On the cursor's own tile he keeps on his heading, three tiles ahead, so he does not stop.
  - Walls, doors and monsters block him as they block a walk.
- **Look:** he is drawn from his magic cast sheet (`player_graphic::Magic`), its facing turning a step a tick and its frames running on. `DrawPlayer` asks `WhirlwindSprite`.
- **Strikes:** every 5 ticks each enemy beside him takes a real blow through `PlayerStrikesMonster` (a new wrapper of `PlrHitMonst`: to-hit roll and all).
  - Damage is 66%, +5% a rank (`ClassMeleeSkillDamagePercent` answers `WhirlwindDamagePercent - 100` while it spins).
  - Each blow that lands draws the burst; the Impact cue sounds once per strike.
- **Text:** the tree row, the description and the tooltip facts say all this.

## Tests

- `OracoolRage.EveryBarbarianActiveIsTheUsersPick`: Whirlwind costs 5, to start.
- `GeneratorsFillAndSpendersDrainWithinThePool`: settles Battle Cry where it settled Whirlwind.
- New `OracoolRage.WhirlwindIsHeldOnTheRightButton`.

Debug build and ctest: 880/880 passed. Not seen in play yet; Whirlwind especially wants a play pass.
