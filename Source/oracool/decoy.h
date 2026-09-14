#pragma once
/**
 * @file oracool/decoy.h
 *
 * The Rogue's summons wear the Rogue - her own player sheets, read from the player's archive at runtime, so nothing
 * is shipped. Three wearers:
 *
 * - **Decoy** (user, 2026-09-14: "why not use Rogue sprites operated by the mechanics behind Golem?" ... "go ahead,
 *   use blue ghost tint"): the sheets for the gear she wears at the cast, in a blue ghost translation.
 * - **Valkyrie** (user, 2026-09-14: "valkyrie is producing a regular golem. fix it."): the Rogue in heavy armour with
 *   sword and shield, in gold, whatever the caster wears.
 * - **The town Valkyrie** (user, 2026-09-14: "i also want to be able to cast this skill in town"): town has no golem
 *   slot, and its dMonster holds the townsfolk, so a monster cannot stand there. In town she is a companion instead -
 *   the same gold sheets, drawn beside the players, following the Rogue, never fighting. She leaves with the level.
 *
 * In a dungeon the body is still the Golem's: its slot (Monsters[playerId]), its brain (GolumAi), its data row and
 * its level type. Only what is DRAWN changes - GetScaledAnim, the one place both monster sprite binders ask for an
 * override, hands the sheets out while the slot is dressed. A sheet the archive lacks falls back to the stand; a
 * missing stand leaves the Golem as it was.
 *
 * A dressed slot is cleared when the slot is summoned again (AddGolem) and on every new level (InitGolems); town
 * companions on every level load (InitLevelMonsters).
 */

#include <cstddef>

#include "engine/point.hpp"
#include "engine/surface.hpp"
#include "monster.h"

namespace devilution {
struct Player;
} // namespace devilution

namespace devilution::oracool {

/** @brief Dresses @p golem - @p player's summon slot, already spawned - as a blue ghost of @p player. */
void MakeDecoy(const Player &player, Monster &golem);

/** @brief Dresses @p golem - @p player's summon slot, already spawned - as a gold Valkyrie. */
void MakeValkyrie(Monster &golem);

/** @brief The slot is no longer dressed: its sheets are released and its translation cleared. */
void ClearDecoy(Monster &golem);

/** @brief Every dressed slot released - a new level. */
void ClearDecoys();

/** @brief Whether @p monster's slot wears hero sheets - a Decoy or a Valkyrie. */
bool IsDecoy(const Monster &monster);

/** @brief The dressed slot's animation for @p graphic, or nullptr when @p monster is not dressed. Asked by GetScaledAnim. */
const AnimStruct *GetDecoyAnim(const Monster &monster, MonsterGraphic graphic);

/** @brief In town: @p player's Valkyrie appears near @p target and follows. False outside town or with nowhere to stand. */
bool SummonTownValkyrie(const Player &player, Point target);

bool HasTownValkyrie(size_t playerId);

/** @brief Every town companion gone - any level load. */
void ClearTownValkyries();

/** @brief One game tick of the town companions: animate, and follow their Rogue. */
void ProcessTownValkyries();

/** @brief Draws any town companion standing on @p tilePosition. Called from the tile loop, after the players. */
void DrawTownValkyries(const Surface &out, Point tilePosition, Point targetBufferPosition);

} // namespace devilution::oracool
