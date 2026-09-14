#pragma once
/**
 * @file oracool/decoy.h
 *
 * The Rogue's Decoy wears the Rogue (user, 2026-09-14: "why not use Rogue sprites operated by the mechanics
 * behind Golem?" ... "go ahead, use blue ghost tint").
 *
 * The body is still the Golem's: its slot (Monsters[playerId]), its brain (GolumAi), its data row and its level
 * type. Only what is DRAWN changes. At the cast, MakeDecoy loads the Rogue's own sheets for the gear she wears at
 * that moment - dungeon stand, walk, attack, hit and death - with the frame counts and widths the player code uses
 * for them, and gives the monster a blue ghost translation. GetScaledAnim, the one place both monster sprite binders
 * ask for an override, hands those sheets out while the slot is a decoy.
 *
 * The sheets are the player's own archive's, read at runtime - nothing is shipped. A sheet the archive lacks falls
 * back to the stand; a missing stand leaves the Golem as it was.
 *
 * A decoy ends when the slot is summoned again (AddGolem clears it) and on every new level (InitGolems).
 */

#include <cstddef>

#include "monster.h"

namespace devilution {
struct Player;
} // namespace devilution

namespace devilution::oracool {

/** @brief Dresses @p golem - @p player's summon slot, already spawned - as a blue ghost of @p player. */
void MakeDecoy(const Player &player, Monster &golem);

/** @brief The slot is no longer a decoy: its sheets are released and its translation cleared. */
void ClearDecoy(Monster &golem);

/** @brief Every decoy released - a new level. */
void ClearDecoys();

bool IsDecoy(const Monster &monster);

/** @brief The decoy's animation for @p graphic, or nullptr when @p monster is not a decoy. Asked by GetScaledAnim. */
const AnimStruct *GetDecoyAnim(const Monster &monster, MonsterGraphic graphic);

} // namespace devilution::oracool
