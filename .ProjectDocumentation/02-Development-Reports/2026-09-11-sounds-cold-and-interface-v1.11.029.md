# Sounds: the cold spells and every silent button (v1.11.028 to v1.11.029)

**Date:** 2026-09-11
**Branch:** renderer-32bit, local commits only

## Cold spells (v1.11.028)

The user asked: "wire the cold spell sounds while you wait".

The cold missiles borrowed other spells' sounds: Firebolt's launch and impact, Nova's for Frost Nova and Blizzard, and Mana Shield's for the armours. Their own cues had been in `sfx\skills\sorcerer\cold-spells` since the class-tree sound package arrived. Nothing played them, because the 2026-09-03 fix ("some unnecessary chatgpt sound played every time i cast") had removed the package's cast and impact cues. Those cues were stacking on top of each spell's own sound.

So the cold cues **replace** the borrowed sounds and never stack on them. The count per moment stays what it was.

| Missile | Launch | Impact |
|---|---|---|
| Ice Bolt, Ice Blast, Glacial Spike | own cast cue (was Firebolt) | own impact cue (was Firebolt's) |
| Frost Nova, Frozen Orb | own cast cue (was Nova / Firebolt) | none |
| Blizzard / its shards | Blizzard cast (was Nova) | Blizzard impact on each landing shard |
| Frozen / Shiver / Chilling Armor | the armour's start cue (was Mana Shield) | its stop cue when it wears off (not on death, not on a recast) |

`oracool::ColdMissileCueSkill` holds the mapping. `PlayColdMissileSound` plays it at the two places the engine sounds a missile row: AddMissile's launch and CheckMissileCol's impact. It returns false when there is no cue, so the borrowed sound still plays as a fallback. The three armours read their skill off the missile's `oracoolSkill` stamp, the first reader since 2026-09-03. Test: `Missiles.ColdMissilesUseTheirOwnCues`.

Left out on purpose:
- The armours' 2-second loop cues would hum for the whole 20+ seconds.
- The Rogue's cold arrows would stack a cast cue on the bow shot.

## Interface sounds (v1.11.029)

The user asked: "audit which interfacing actions are soundless, like clicking on any of the new interface buttons we have introduced around the game and town vendors and so on and apply a sound."

An audit agent found that most fork controls were silent. The sounds come from vanilla, not a new choice. The pause menu, quest log and stores all play `IS_TITLEMOV` for moving and `IS_TITLSLCT` for acting, so `oracool/ui_sound.h` wraps that pair as `PlayUiMoveSound` / `PlayUiSelectSound`. It guards against a null MyPlayer, because PlaySFX reads it and two tests drive these handlers without one.

**Now sounding:**
- **Every close X:** `CheckWindowCloseButtonClick` plays the click itself, since every caller closes on true. Its header now says it's a click handler, never a hover test. The shop's, the runeword book's and Levski's own X buttons play it too. Levski's only plays it when the close actually happens.
- **HUD menu:** entries, the belt Menu cell, and clicking away to close. The Town Portal cell clicks only when it can't cast; `CastTownPortalAtFeet` now returns bool.
- **Shop:** vendor tabs (on a real change), page arrows, selecting a stock item, picking up the Repair and Recharge cursors, and the Refresh actions.
  - Dropping an item on Repair or Recharge plays `IS_GOLD`, like the hammer path.
  - Repair All plays `IS_REPAIR` once, only if something was mended.
- **Levski's Roar:**
  - Opening the monument, closing it, the Recipe Book toggle and selecting a recipe row.
  - A transmute that made something plays `IS_ISHIEL`, matching salvage.
  - Putting an item in or taking one out by hand plays the backpack's item sounds.
- **Runeword Book:** the Possible toggle and the slot and rune filters. The W key plays Select on open and Move on close.
- **Crafting window:** recipe rows.
- **Abilities window:**
  - Sheet arrows and refunds.
  - Arming, filling and emptying passive slots.
  - Assigning LMB/RMB, and F1-F8 bind/unbind.
  - Investing in an active skill plays Select when the skill has no Learn cue of its own.
- **Skill picker and wells:** open and close from the wells and the A/S keys, Shift-clearing a well, and choosing an attack or spell. Auras play their own start or stop cue and get no extra click.
- **Other:** the skill-points frame, inventory tabs, the stash gold total, waypoint travel and the row you're already on, F9/F10 (only when the speed changes), F11 (only when the log's visibility changes), and R.
- **Front end:** the first click that arms a dialog button plays the list's move sound.

**Rules kept:**
- No second sound on any path that already makes one: coins, Sort, salvage, the Town Portal cast, aura cues, Learn cues.
- Refusals keep their existing feedback.
- Vanilla's silent controls stay silent.

**Found in review and fixed before building:**
- The agent spotted TRANSMUTE success by matching two refusal messages word for word. crafting.cpp now owns those words, and both its returns and the new `IsTransmuteRefusal` use the same source.
- The edit had silently turned two bare-LF lines in inv.cpp into CRLF. They were restored byte for byte.

**Possible doubles still open (from the agent):**
- Refunding a point until a burning aura goes out plays the refund click and the aura's stop cue.
- An aura with no start or stop cue is still silent in the picker.

**Not done:** the shop's "Sell all" button (`StoreSellItemAt` plays no gold sound), and the D and T keys.

## Verification

Debug build, ctest **699/699** after each step. Release built. RTM refreshed with exe 1.11.029. Not heard in play; the check is the user's.
