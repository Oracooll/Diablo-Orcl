/**
 * @file oracool/lesser_uniques.h
 *
 * Oracool: lesser uniques - underpowered versions of the game's named champions, scattered on any
 * level rather than waiting on the one floor they were written for.
 *
 * User request (2026-08-15): "lets introduce underpowered versions of unique bosses along with
 * minions randomly on every level [...] we need to introduce diversification and freshness in the
 * dungeons [...] make diablo 1 world feel new."
 *
 * The premise is that almost none of this is new content. UniqueMonstersData holds 101 hand-authored
 * champions - name, palette, AI, hit points, damage, resistances, minion pack - and a normal
 * playthrough meets perhaps a dozen, because each is pinned to a single level by mlevel == currlevel
 * and most are quest-gated on top. Diablo 1 already has a champion system; it spawns each champion
 * exactly once, on one floor, forever. This stops wasting the other ninety.
 *
 * See the vault's "Lesser Uniques - Design" for the full plan and for why the user's "shrunken in
 * size" is answered with palette and naming instead: there is no scale parameter anywhere in the CLX
 * renderer, and legibility - not literal size - is what the request is actually after.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>

namespace devilution {

// Forward-declared rather than including monster.h: this header is included from monster.cpp itself,
// and the enum's underlying type is fixed, so the declaration is all a caller needs.
enum class UniqueMonsterType : uint8_t;
struct Monster;
struct Player;

/**
 * @brief The champion modifier a lesser unique carries.
 *
 * Oracool: user request (2026-08-15) - "you have diablo 2 3 4 to draw inspiration from". These are
 * the D2/D3 champion modifiers, chosen because each is expressible with machinery Diablo 1 already
 * has rather than because it sounded good on a list:
 *
 *   Warded     - resistances, already a per-unique bitfield
 *   Relentless - MFLAG_KNOCKBACK and the stun path already exist
 *   Fortified  - armour, which is per-monster where movement speed is not (see the .cpp)
 *   Vampiric   - Blood Star monsters already drain life
 *   Thunderous - MissileID::MiniNovaBall, added for Fist of the Heavens at 1.5.78
 *
 * Lives in the devilution namespace rather than oracool because Monster carries one as a field, and
 * a member's type should not need a nested namespace to name.
 */
enum class LesserUniqueAffix : uint8_t {
	None,
	Warded,
	Relentless,
	Fortified,
	Vampiric,
	Thunderous,
	LAST = Thunderous,
};

namespace oracool {

/**
 * @brief Picks a champion this level could plausibly host, or nullopt if none fits.
 *
 * Three constraints, each doing real work:
 *
 *  - Its monster type must ALREADY be in LevelMonsterTypes. That makes a lesser unique free (the
 *    sprite and its animations are loaded either way) and coherent (a Cathedral floor draws its
 *    champions from the skeletons actually walking it, never from something that does not belong).
 *  - It must not talk. mtalkmsg is the game's own marker for quest content - Garbud, Zhar, Lazarus -
 *    and a lesser unique must never be a quest boss with the serial numbers filed off.
 *  - It must not be the champion this floor already hosts, so the real one stays singular.
 *
 * @param excludeLevelOwned when true, skips uniques whose mlevel is the current level.
 */
std::optional<UniqueMonsterType> ChooseLesserUnique(bool excludeLevelOwned = true);

/** @brief How many lesser uniques this level should try to host. */
int LesserUniqueCountForLevel();

/** @brief Rolls a modifier for a new champion. Never None - a lesser unique always has one. */
LesserUniqueAffix RollLesserUniqueAffix();

/** @brief The word that goes in front of a champion's name, e.g. "Warded". */
const char *GetLesserUniqueAffixName(LesserUniqueAffix affix);

/**
 * @brief Applies the parts of @p affix that are set once, when the champion is created.
 *
 * The rest - the parts that fire during combat - live at the points in the engine where that combat
 * already happens, because a resistance is a field and a life-steal is an event.
 */
void ApplyLesserUniqueAffix(Monster &monster);

/**
 * @brief Oracool: a champion's on-hit modifier, called when @p monster wounds a player.
 *
 * Vampiric heals the monster for a share of what it dealt. Nothing else keys off this yet, but it is
 * the one seam where "the champion did damage" is known, so it is where that family belongs.
 */
void OnLesserUniqueDealtDamage(Monster &monster, int damage);

/** @brief Oracool: a champion's on-death modifier - Thunderous discharges here. */
void OnLesserUniqueKilled(Monster &monster);

} // namespace oracool
} // namespace devilution
