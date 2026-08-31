---
date: 2026-08-15
version: 1.5.70
area: Paladin skills / gameplay plan
status: plan, not yet built
---

# Paladin Skills — Implementation Plan

The UI is finished: seven skills, icons, level gates, mana prices, both mouse buttons, save
persistence. Two of them do something (Charge, Zeal). Five do not. This is the plan for the rest.

## Decisions taken (user, 2026-08-15)

| Question | Answer | What it rules out |
|---|---|---|
| Zeal: passive or active? | **Active only** — splashes only when Zeal is readied on the button that swung | Today's behaviour, where it fires on every swing from level 6 |
| Art for the three with no visuals | **Reuse existing missiles now, swap later** | Waiting on an art pass before the skills are playable |
| Damage source | **Weapon damage**, with a per-skill multiplier | Spell-style level scaling that ignores gear |
| Shield gate on Blessed Shield / Shield Bash | **Require a shield equipped** | Firing bare-handed |

Two judgement calls I made rather than asking, both easily reversed:

- **Grey plates stay level-only.** The shield requirement is *gear*, which changes minute to minute;
  greying a row on every weapon swap would make the sheet flicker and would collide with grey's
  existing meaning ("not earned yet"). The requirement is stated in the hover popup, and a cast
  without a shield refuses with a line.
- **Zeal and Hammer of Faith need distinct shapes**, because the two delivered descriptions overlap.
  Zeal = *rapid successive strikes on up to five adjacent enemies* (many targets, one at a time).
  Hammer of Faith = *one swing whose impact also damages everything adjacent to the target* (one
  blow, splash around it).

## Standing rules for every skill (user, 2026-08-15)

> skills rule: melee skills only initiate when clicked on monsters within range, else - move command.
> it goes for lmb/rmb
> skills rule: ranged skills only initiate when clicked on monsters within range, else - move command.
> it goes for lmb/rmb
> skills rule: we need to define range of skills. i suggest no more than a 640x480px worth of screen
> estate.

These are laws for the whole set, not per-skill behaviour, so they belong above the build order.

**A click that cannot reach a target is a MOVE, not a nothing.** The current code fails this, and
fails it silently:

```cpp
if (oracool::PaladinSkillForSpell(spellID).has_value()) {
    if (pcursmonst != -1 && !isShiftHeld) { ...attack... }
    return;                     // <- no monster under the cursor: the click does nothing at all
}
```

Charge's own branch two blocks above has the same shape. Both need the else: walk to the clicked
tile. This is the more useful reading of "the ability never does nothing" than the one already
written there — a skill on your button should never make the ground unclickable.

**Range has to be a per-skill number**, because the rule is "within range" and nothing currently
carries one. It belongs in `PaladinSkillData` beside `minLevel` and `manaCost`, so the sheet, the
hover popup and the targeting check read one table — the same reason the spell slot went there.

### Deriving the cap from 640×480

Worth showing the arithmetic, because the answer is a tile count and the request is in pixels.

DevilutionX's isometric grid is `TILE_WIDTH` 64, `TILE_HEIGHT` 32, so **one tile step moves the view
32px horizontally and 16px vertically**. A 640×480 box centred on the player reaches ±320px and
±240px, which is:

| Axis | Budget | Per tile | Tiles |
|---|---|---|---|
| Horizontal | 320px | 32px | **10** |
| Vertical | 240px | 16px | 15 |

Horizontal binds, so the cap is **10 tiles**. Melee skills are 1 tile (adjacent, as they already
are); ranged skills get their own value up to 10.

That is also a sane number on its own terms — 10 tiles is a little under half the visible width at
960×720, so a ranged skill reaches meaningfully across the screen without out-ranging what the player
can see coming. If the 640×480 figure was meant as the whole reachable *area* rather than a radius,
the answer halves to 5 tiles; say so and it is one constant.

## The one structural piece to build first

Three of the seven are melee (Zeal, Hammer of Faith, Shield Bash). `DoAttack` resolves every swing
in one place (`player.cpp:842`) and **does not know which mouse button launched it** — by the time
the animation reaches its hit frame, all that survives is `_pRSpell` / `_pLRSpell`.

"Active only" needs that knowledge. So: a latch, armed at the moment a button acts and read at the
moment the swing lands.

```
oracool::ArmMeleeSkill(std::optional<PaladinSkill>)   // set at launch
oracool::ArmedMeleeSkill()                            // read in DoAttack
```

Armed in `CheckPlrSpell` — the single funnel through which a mouse button acts, and it already
receives the pair for whichever button was pressed, so `PaladinSkillForSpell(spellID)` is the whole
implementation. Cleared in `LeftMouseCmd`, the path a plain swing takes. The controller
(`plrctrls.cpp`) and hold-to-attack (`track.cpp`) repeat paths deliberately inherit the current
value: you are still holding the same button.

Single-player only, one swing at a time, so file-scope state is honest here — the same shape
`IsFuriousChargeOnCooldown` already uses.

Then `DoAttack`'s existing Zeal hook becomes one dispatch for all three melee skills, and
`warrior_splash.cpp`'s unconditional `CanUsePaladinSkill(Zeal)` gate becomes "Zeal is what is armed".

## Build order

Sequenced so each step is independently testable and the hard ones come last.

0. **Targeting rules** — the per-skill range field, the in-range check, and the walk-instead-of-
   nothing fallback on both buttons. First, because every skill after it inherits the behaviour, and
   because it also fixes Charge, which is already shipping with the silent-nothing bug.
1. **Foundation** — the latch, a shared weapon-damage helper, `HasShieldEquipped`, and the melee
   dispatch in `DoAttack`.
2. **Zeal** — flip from passive to armed-only. Smallest change, proves the latch works end to end.
3. **Shield Bash** — melee hit plus a stun. *Open question to resolve while building:* the right stun
   primitive. `M_StartHit` puts a monster in `HitRecovery` for its own animation length, which is too
   short to read as a stun; Stone Curse's `MonsterMode::Petrified` is a full freeze with its own
   duration counter and is the closer model. Petrified without the graphic swap is the likely answer.
4. **Hammer of Faith** — one swing, splash around the target. Shares almost everything with Zeal's
   ring-collection code; the difference is the shape, not the machinery.
5. **Fist of the Heavens** — targeted AoE at a point. Borrow `MissileID::ApocalypseBoom`'s graphic.
   First of the three that needs a missile rather than a melee hook.
6. **Blessed Shield** — thrown, striking several enemies. Needs a missile that survives its first hit
   and re-targets.
7. **Blessed Hammer** — spiralling projectile. Genuinely new movement: nothing in `missiles.cpp`
   travels a spiral, so this needs its own `Process` function driving position from an angle and a
   growing radius rather than from a velocity vector. Last for that reason.

## Things already true that this must not break

- `MAX_ITEM_SPELLS` stays 52. Item generation draws `GenerateRnd(maxSpells)` and walks from there, so
  widening it shifts every seeded book in the game — `PackTest`'s fixtures encode that and will say so.
- All seven have `sBookLvl` and `sStaffLvl` of -1. They are earned, never found.
- `CheckPlrSpell` currently intercepts every Paladin skill and makes it swing, so that nothing charges
  mana for an unimplemented effect. Each skill leaves that block as it gains real behaviour.
- Mana is charged **only when the effect happens** — Charge's dash, Zeal's carry. A skill that finds
  no target must cost nothing. This is the rule the existing code states twice and the new skills
  should keep.
