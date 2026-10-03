#pragma once
/**
 * @file oracool/elixirs.h
 *
 * Timed elixirs (user, 2026-10-03: "make them give +5 point. to last 5 minutes. to have countdown timer next to minimap.
 * to be able to stack 10 consumes to reach temp +50 per stat. at 11th consume hero says I cant do this. when barb tries
 * to consume magic elixir to refuse and say I cant do this. countdown icon to be the elixir potion icon.").
 *
 * Vanilla's four elixirs (Strength, Magic, Dexterity, Vitality) added one BASE point for good. Here each drink is a
 * stack of +5 to the stat for five minutes; up to ten stacks per stat (+50). Every drink puts that stat's clock back to
 * five minutes, and the whole stack ends together when it runs out. The bonus is an ITEM-side bonus (CalcPlrItemVals
 * adds it like a ring's +Strength): base stats, stat points and requirements bought with base points are untouched.
 *
 * Transient - never saved. A hero enters the game with none; death does not take them (they were bought and drunk).
 * The countdown column beside the mini-map (oracool/spell_timers) shows one row per stat with the elixir's own icon.
 */

#include <cstdint>
#include <optional>

#include "itemdat.h"

namespace devilution {

struct Player;
struct Item;
enum class CharacterAttribute : uint8_t;

namespace oracool {

/** @brief What one drink adds to its stat. */
constexpr int ElixirBonusPerStack = 5;
/** @brief Drinks of one kind that can stand at once - the eleventh is refused. */
constexpr int ElixirMaxStacks = 10;
/** @brief Five minutes of game ticks (20 a second). */
constexpr int ElixirDurationTicks = 5 * 60 * 20;

/** @brief The stat an elixir raises, or nullopt for anything that is not one of the four. */
std::optional<CharacterAttribute> ElixirAttribute(item_misc_id miscId);

/**
 * @brief Whether @p player must refuse to drink @p item, said aloud ("I can't do that") and in the log when so: an
 * eleventh elixir of one kind, or an Elixir of Magic for a Barbarian (he has no mana and puts nothing into Magic).
 * Asked BEFORE the item is used, by the backpack, the belt and the stash, so a refused elixir is kept.
 */
bool RefuseElixir(Player &player, const Item &item);

/** @brief One drink: a stack more (to ElixirMaxStacks) and the clock back to five minutes; the stats recalculated. */
void DrinkElixir(Player &player, CharacterAttribute attribute);

/** @brief The bonus the running elixirs give @p attribute: stacks x ElixirBonusPerStack. */
int ElixirBonus(const Player &player, CharacterAttribute attribute);

/** @brief The stacks of @p attribute's elixir now standing, 0 to ElixirMaxStacks. */
int ElixirStacks(const Player &player, CharacterAttribute attribute);

/** @brief Ticks left on @p attribute's elixirs, or 0. */
int ElixirTicksLeft(const Player &player, CharacterAttribute attribute);

/** @brief The inventory cursor of @p attribute's elixir - the countdown row's icon. */
item_cursor_graphic ElixirCursor(CharacterAttribute attribute);

/** @brief Counts every running elixir down a tick; the stats are recalculated when one runs out. */
void ProcessElixirsTick(Player &player);

/** @brief Ends every elixir at once, with no recalculation - a new game's hero, never a level change. */
void ResetElixirs(Player &player);

} // namespace oracool
} // namespace devilution
