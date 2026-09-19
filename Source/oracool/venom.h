/**
 * @file oracool/venom.h
 *
 * Oracool: poison ON THE PLAYER - the Venomous monster variant's bite (2026-09-19).
 *
 * The Necromancer's page gave MONSTERS a poison channel (marks on the monster, DamageType::Acid);
 * nothing poisoned the player until the Venomous variant. This is the smallest honest version: a
 * per-player bleed that ticks a fixed amount of life for a fixed number of game ticks, refreshed
 * (never stacked) by another bite, resisted by MAGIC resistance - the fork's own rule for poison,
 * which has no resistance of its own here (the Tal rune, the Emerald: "poison resist becomes magic
 * resist").
 *
 * Per-game state in a static, like the cold armours: it lives for the session and is cleared when
 * a player is initialised, so nothing reaches the next character - see the audit-lifetime rule.
 * Not saved: a bleed that outlives a save-and-load is not worth a format byte.
 */
#pragma once

#include <cstdint>

namespace devilution {
struct Player;
} // namespace devilution

namespace devilution::oracool {

/**
 * @brief Poisons @p player: @p totalDamage (in 64ths, like _pHitPoints) spread over @p ticks game
 * ticks. A bite while poisoned REFRESHES - the larger dose wins and the clock restarts - so a pack
 * of Venomous monsters is a steady bleed, not a multiplying one.
 */
void PoisonPlayer(Player &player, int totalDamage, int ticks);

/** @brief Whether @p player is poisoned right now (the character sheet's and the HUD's question). */
bool IsPlayerPoisoned(const Player &player);

/** @brief Game ticks of poison left on @p player, 0 when clean. */
int PlayerPoisonTicks(const Player &player);

/** @brief One game tick of the bleed for @p player. Called from the player's own tick. */
void TickPlayerVenom(Player &player);

/** @brief Clears @p player's poison - InitPlayer's reset, and a cure. */
void ClearPlayerVenom(const Player &player);

} // namespace devilution::oracool
