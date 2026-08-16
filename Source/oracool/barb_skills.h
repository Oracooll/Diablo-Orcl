/**
 * @file oracool/barb_skills.h
 *
 * Oracool: the Barbarian's 18 skills - names, descriptions, kinds and unlock levels.
 *
 * DATA ONLY.  Nothing here casts, buffs or damages anything: the
 * skills are listed, described and unlocked by level. The gameplay design - including the Fury
 * resource the brief proposes as the Barbarian's answer to the Paladin's auras - is a later pass.
 */
#pragma once

#include <cstddef>
#include <cstdint>

#include "utils/stdcompat/string_view.hpp"

namespace devilution {

struct Player;

namespace oracool {

/**
 * @brief The 18 skills, in the order of the icon sheet (ui\barb_skill_icons.png).
 *
 * Enum order IS icon order - GetBarbSkillIconIndex is the identity - so the sheet and this list
 * cannot drift. The Abilities window displays them in a different order (by level, then name); see
 * GetBarbSkillAtDisplayIndex for why that is a lookup rather than a reordering of this enum.
 */
enum class BarbSkill : uint8_t {
	Berserk,
	FIRST = Berserk,
	BattleCry,
	BattleOrders,
	Shout,
	WarCry,
	IncreasedStamina,

	NaturalResistance,
	Concentrate,
	DoubleSwing,
	Whirlwind,
	Frenzy,
	Taunt,

	IronSkin,
	Toughness,
	LeapAttack,
	Stomp,
	SeismicSlam,
	FindItem,
	LAST = FindItem,

	None = 0xFF,
};

constexpr size_t BarbSkillCount = 18;

/**
 * @brief What kind of thing a skill is, which is the first thing a player needs to know about it.
 *
 * A Passive is always on once learned; the rest have to be used. Shown as a tag on the row, because
 * "do I press this?" is not answerable from the description alone.
 */
enum class BarbSkillKind : uint8_t {
	Combat,
	Warcry,
	Passive,
	Utility,
};

struct BarbSkillData {
	/** Untranslated; run through _() at the point of display. */
	const char *name;
	/** One sentence, short enough to wrap to two lines in the Abilities window. */
	const char *description;
	BarbSkillKind kind;
	/** Character level at which the skill becomes available. */
	int minLevel;
};

const BarbSkillData &GetBarbSkillData(BarbSkill skill);

/** @brief Display name of a skill kind, for the row's tag. */
string_view GetBarbSkillKindName(BarbSkillKind kind);

/** @brief Whether @p player is high enough level to use @p skill. */
bool IsBarbSkillUnlocked(const Player &player, BarbSkill skill);

/** @brief Whether this class has these skills at all. Barbarian only, by design. */
bool ClassHasBarbSkills(const Player &player);

/** @brief Index of @p skill's icon in ui\barb_skill_icons.png - the identity, see the enum. */
inline int GetBarbSkillIconIndex(BarbSkill skill)
{
	return static_cast<int>(skill);
}

/** @brief The skill at display position @p index, ordered by unlock level then by name. */
BarbSkill GetBarbSkillAtDisplayIndex(size_t index);

} // namespace oracool
} // namespace devilution
