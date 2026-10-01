/**
 * @file oracool/chill.h
 *
 * Oracool, Round 1 of the inert-skill plan (2026-09-03): monsters can be CHILLED.
 *
 * Cold damage on its own is a fifth colour of number. What makes cold read as cold - and what every
 * inert row on the Sorceress's first page is really waiting for - is that the thing you hit slows
 * down. So this ships with the first cold missile rather than after it.
 *
 * HOW SLOW IS IMPLEMENTED. A chilled monster loses every other game tick: ProcessMonsters skips its
 * AI and its animation on alternate ticks while the chill lasts. That is half speed for movement,
 * attacks and recovery together, without touching a single rate constant - which matters, because
 * monster speed in this engine is not one number but the interaction of the AI's own cadence with
 * the animation's, and multiplying either one apart from the other desynchronises them.
 *
 * WHY A SIDE TABLE RATHER THAN A FIELD ON MONSTER. Monsters are serialised into the level save by a
 * fixed layout, so a new field is a save-format change - for a status effect that must not survive
 * one anyway. A chilled monster the player leaves the level to escape should not still be chilled
 * when they come back an hour later.
 *
 * The table is a file-local static, so it OUTLIVES THE GAME (see the project note of the same name):
 * ClearChills is called from InitMonsters, which runs on every level entry and on every new game,
 * and that is what stops one character's frost from landing on another's monsters.
 */
#pragma once

#include <cstdint>

namespace devilution {
struct Monster;
} // namespace devilution

namespace devilution::oracool {

/** @brief How long one hit of Ice Bolt holds a monster, in game ticks. Two seconds at 20 fps. */
constexpr int IceBoltChillTicks = 40;

/**
 * @brief Chills @p monster for @p ticks, or extends an existing chill to that length.
 *
 * Extends rather than adds: a monster under continuous fire stays at half speed for as long as the
 * shots keep landing, and stacking would let a few seconds of casting freeze something for a minute.
 */
void ChillMonster(const Monster &monster, int ticks);

/** @brief Whether @p monster is chilled right now - for the tint, and for anything that asks. */
bool IsMonsterChilled(const Monster &monster);

/**
 * @brief Freezes @p monster solid for @p ticks: EVERY tick is the ice's, not every other one.
 *
 * Round 2 (2026-09-03), for Ice Blast, Glacial Spike and Frozen Armor. Extends rather than stacks,
 * like the chill, and for the same reason. A freeze is drawn in the cold tint and stops the monster
 * outright; callers decide who may be frozen at all - oracool/cold.h downgrades uniques to a chill.
 */
void FreezeMonster(const Monster &monster, int ticks);

/** @brief Whether @p monster is frozen solid right now. Implies chilled. */
bool IsMonsterFrozen(const Monster &monster);

/**
 * @brief Whether the ice owns @p monster's turn this tick. Ages the chill by one tick as it answers.
 *
 * Called once per monster per tick from ProcessMonsters, and it is the whole mechanic: true means
 * that monster does nothing this tick.
 */
bool ChillTakesThisTick(const Monster &monster);

/** @brief Forgets every chill. Called when a level's monsters are created - see the header. */
void ClearChills();
/** @brief Ends one monster's chill at once - for a curse that laid it and is now released (audit, 2026-09-19). */
void ClearChill(const Monster &monster);
/**
 * @brief Ends @p monster's chill if it runs no longer than @p ticks - a curse taking back the chill it laid, and not a longer one
 * another source laid over it (round 62 audit: Decrepify's end wiped a Clay Golem's or Rigor Mortis's chill).
 */
void ClearChillUpTo(const Monster &monster, int ticks);
/**
 * @brief Ends one slot's chill AND freeze. The slot is being freed or reused: a monster that died cold
 * must not hand its timers to the next monster spawned into its slot (external audit of v1.12.188, SKL-02).
 */
void ClearColdStateForMonster(const Monster &monster);

} // namespace devilution::oracool
