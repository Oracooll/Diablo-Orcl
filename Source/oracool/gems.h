/**
 * @file oracool/gems.h
 *
 * Oracool: Megaplan Phase 1 - gems and sockets, the raw-material half of the item endgame.
 *
 * The design imports Diablo II's soul with Diablo I's restraint:
 *   - Sockets roll ONLY on plain, tierless NORMAL-quality equipment (TryAddSocketsToDroppedItem),
 *     which makes "basic item" - the label that used to mean "vendor it" - the raw material of the
 *     socket economy, exactly the role white items played in D2's runeword culture.
 *   - Five gems (Ruby, Sapphire, Topaz, Emerald, Skull), one quality tier each for now, with
 *     HOST-DEPENDENT effects: what a gem does in a weapon differs from what it does in armor or a
 *     shield. That asymmetry is the whole fun of socketing decisions.
 *   - A socketed gem is permanent (no Hellforge yet - a possible crafting recipe later).
 *
 * The gems are identified by base-item index alone (IsOracoolGemIdx); effects live in the table
 * here and reach the character sheet through the "sockets" bonus provider in stat_sheet.cpp.
 */
#pragma once

#include <string>
#include <string_view>

#include "itemdat.h"

namespace devilution {
struct Item;
struct Player;
} // namespace devilution

namespace devilution::oracool {

struct ItemBonusTotals;

/**
 * @brief The seven gem types, in the column order of Gems.png.
 *
 * Enum order IS the sheet's column order, which is also the order the icon specs were written in,
 * so the art and this list cannot drift.
 */
enum class GemType : uint8_t {
	Amethyst,
	Diamond,
	Emerald,
	Ruby,
	Sapphire,
	Topaz,
	Skull,
	LAST = Skull,
};
constexpr size_t GemTypeCount = 7;

/** @brief The five quality tiers, in the row order of Gems.png - Diablo II's own ladder. */
enum class GemQuality : uint8_t {
	Chipped,
	Flawed,
	Normal,
	Flawless,
	Perfect,
	LAST = Perfect,
};
constexpr size_t GemQualityCount = 5;

/** @brief The item index of one gem. Every (type, quality) pair has one. */
uint16_t GemIndexFor(GemType type, GemQuality quality);

/** @brief Whether @p gemIdx is a perfect gem - the top of its ladder, so nothing refines it. */
bool IsPerfectGem(uint16_t gemIdx);

/**
 * @brief The same gem one quality better, or @p gemIdx itself if it is already perfect (or not a
 * gem at all). The crafting window's Refine Gems recipe is the only caller.
 */
uint16_t NextGemQuality(uint16_t gemIdx);

/** @brief Decomposes a gem item index. False if @p gemIdx is not a gem at all. */
bool GemTypeAndQuality(uint16_t gemIdx, GemType &type, GemQuality &quality);

/**
 * @brief What @p quality multiplies a gem's effects by, in percent.
 *
 * Normal is 100 because the five original gems' tuned numbers ARE the normal-quality row - the
 * ladder was added around them rather than replacing them, so nothing already balanced moved.
 */
int GemQualityPercent(GemQuality quality);

/** @brief The three effect groups a socket host falls into. */
enum class SocketHost : uint8_t {
	Weapon,
	Shield,
	/** Body armor, helms, and the six worn accessory types. */
	Armor,
};

/** @brief Which effect group @p hostType's sockets use. */
SocketHost SocketHostForItemType(ItemType hostType);

/** @brief The item-panel words for a set of special-effect flags ("faster attack, life steal"); empty for none. */
std::string FlagText(ItemSpecialEffect flags);

/**
 * @brief An attack-speed flag's words, with what it does for a hero shooting a bow: the flags quicken the arrow there and
 * give no draw speed (round 27 audit). The bow's own line says "arrows" already.
 */
std::string AttackSpeedWords(std::string_view words);

/**
 * @brief Whether @p item may receive sockets at drop time.
 *
 * Sockets v2: basic-quality equipment of any BASE TIER for the eleven worn equipment types, and
 * jewelry at any quality (rings and amulets have no basic versions, so basic-only would mean
 * never). Quality still excludes magic and better everywhere else - the white item's whole role
 * is being the host.
 */
bool CanItemHaveSockets(const Item &item);

/**
 * @brief The socket ceiling for @p item: its inventory footprint in 28x28 cells, so a 1x1 ring
 * takes one and a 2x3 two-hander takes six. Clamped to Item::MaxItemSockets.
 */
int MaxSocketsForItem(const Item &item);

/**
 * @brief The rune ladder, in Diablo II's order.
 *
 * The ENUM order is not the rune order and cannot be - the five that shipped in v1.7.8 sit before
 * the charms and the whole gem ladder, and the other 28 are appended after all of it. Anything
 * that means "the next rune up" must come through here rather than adding 1 to an index.
 */
/** @brief The array bound callers need when collecting every rune. */
constexpr size_t MaxRuneLadder = 33;
size_t RuneLadderSize();
uint16_t RuneAtLadderPosition(size_t position);
/** @brief The rune one rung above @p runeIdx, or @p runeIdx itself for Zod or a non-rune. */
uint16_t NextRune(uint16_t runeIdx);
/** @brief Whether @p runeIdx is Zod - the top of the ladder, so nothing ascends it. */
bool IsTopRune(uint16_t runeIdx);

/**
 * @brief The jewel ladder - three grades per family, climbed by the Temper Jewels recipe.
 *
 * The fifteen ids are GRADE-MAJOR (all five Flawed, then all five Plain, then all five Radiant), so
 * the next grade of a jewel is exactly JewelFamilyCount ids further on. That is a property of the
 * generator's one-walk-one-order discipline rather than a coincidence, and the static_assert in the
 * .cpp is what keeps it true if the generator is ever changed to emit family-major.
 */
constexpr size_t JewelFamilyCount = 5;
constexpr size_t JewelGradeCount = 3;

/** @brief Whether @p jewelIdx is Radiant - the top of its ladder, so nothing tempers it. */
bool IsTopJewel(uint16_t jewelIdx);

/**
 * @brief The same jewel one grade better, or @p jewelIdx itself if it is Radiant (or not a jewel).
 *
 * Same contract as NextGemQuality, deliberately: the recipes read alike because they do alike.
 */
uint16_t NextJewelGrade(uint16_t jewelIdx);

/** @brief Applies gem @p gemIdx's effect for @p host onto @p totals. Unknown indices are inert. */
void ApplyGemToTotals(uint16_t gemIdx, SocketHost host, ItemBonusTotals &totals);

/**
 * @brief Hel's total requirement reduction on @p item, in percent, capped at 60.
 *
 * Two runes act on the HOST rather than on the totals, because what they change is not a total:
 * a requirement is the item's own number, compared against the player's. Reading it here instead
 * of writing it into the item is what stops the reduction compounding every time the character
 * sheet recalculates.
 */
int SocketRequirementReductionPercent(const Item &item);

/** @brief @p baseRequirement after Hel. Never returns 0 for a requirement that existed. */
int EffectiveRequirement(const Item &item, int baseRequirement);

/** @brief Zod: whether @p item's sockets make it immune to durability loss. */
bool SocketsMakeIndestructible(const Item &item);
/** @brief Whether the socketed stone @p socketed is the indestructible one (Zod) - the one an ethereal host keeps (round 74). */
bool IsIndestructibleStone(uint16_t socketed);

/**
 * @brief Stamps Zod's indestructibility onto @p item if its sockets now carry one.
 *
 * Called on insertion. Zod is the one rune written into the host instead of read off it, because
 * "indestructible" is a durability value this engine already has - ten decrement sites test for
 * it. _iMaxDur is left alone so extraction can restore a destructible item.
 */
void ApplyZodToHost(Item &item);

/** @brief One line of description for @p gemIdx socketed in @p host, e.g. "Ruby: +8 fire damage".
 * Translated and formatted; empty for an unknown index. */
std::string GemSocketLine(uint16_t gemIdx, SocketHost host);

/**
 * @brief The same effects framed for a LOOSE stone - "In weapons: +7% damage, knockback".
 *
 * Empty when the stone does nothing in @p host, so a caller can print all three hosts and let the
 * blanks fall away. Runes typically fill all three; a gem's shield and armor lists often differ.
 */
std::string GemHostEffectLine(uint16_t gemIdx, SocketHost host);

/**
 * @brief The insertion rule, in one place: if @p held is a gem and @p target has an open socket,
 * the gem goes into the first empty slot and the function returns true (caller clears the cursor
 * and recalculates). False leaves both items untouched, and the paste proceeds as a normal swap.
 */
bool TrySocketGem(Item &target, const Item &held);

/** @brief Total mana restored to @p player on each kill by socketed runes (Tir's D2 rule: +2 per
 * Tir). Summed over every usable worn item's filled sockets; consumed in MonsterDeath. */
int RuneManaPerKill(const Player &player);

/** @brief Total life restored to @p player on each kill by socketed Skulls in WEAPON hosts - the
 * quality-scaled substitute for D2's skull leech. Consumed in MonsterDeath beside the rune mana. */
int GemLifePerKill(const Player &player);

} // namespace devilution::oracool
