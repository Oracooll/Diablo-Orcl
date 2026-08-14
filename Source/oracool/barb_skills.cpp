#include "oracool/barb_skills.h"

#include <algorithm>
#include <array>
#include <cassert>

#include "player.h"
#include "utils/language.h"

namespace devilution {
namespace oracool {

namespace {

// Order must match the BarbSkill enum, which matches ui\barb_skill_icons.png.
//
// The unlock levels are Oracool's, not the brief's - the design document describes each skill and
// groups them into Warcries, Combat and Passives, but never assigns levels. They are laid out as
// three tiers of six at levels 1 / 8 / 16, which mirrors the Paladin's aura tiers (1/8/16/24) so
// the two classes progress on the same rhythm, and which falls out of the icon sheet's own 6x3
// grid the way the aura sheet's 6x4 gave four tiers.
//
// Within a tier the ordering follows the brief's own signals - its Fury costs and its note that
// Shout is "suitable from early levels onward" - so the cheap, always-on and introductory abilities
// come first and Whirlwind, War Cry and Berserk come last.
const BarbSkillData SkillTable[BarbSkillCount] = {
	{ N_("Berserk"), N_("Devastating strike - far more damage and to-hit, at the cost of Armor Class."), BarbSkillKind::Combat, 16 },
	{ N_("Battle Cry"), N_("Nearby enemies suffer reduced Armor Class and physical damage for a time."), BarbSkillKind::Warcry, 8 },
	{ N_("Battle Orders"), N_("Temporarily increases maximum Life, and Mana by a smaller amount."), BarbSkillKind::Warcry, 8 },
	{ N_("Shout"), N_("Temporarily increases Armor Class. Reliable from the earliest levels."), BarbSkillKind::Warcry, 1 },
	{ N_("War Cry"), N_("Damages and briefly stuns every monster around the Barbarian."), BarbSkillKind::Warcry, 16 },
	{ N_("Increased Stamina"), N_("Endurance: improves movement speed and hit recovery."), BarbSkillKind::Passive, 1 },

	{ N_("Natural Resistance"), N_("An innate bonus to Fire, Lightning and Magic Resistance."), BarbSkillKind::Passive, 8 },
	{ N_("Concentrate"), N_("A focused strike with better to-hit and Armor Class, hard to interrupt."), BarbSkillKind::Combat, 8 },
	{ N_("Double Swing"), N_("Strikes with both weapons in sequence. Requires two one-handed weapons."), BarbSkillKind::Combat, 1 },
	{ N_("Whirlwind"), N_("Spins to a tile up to three away, striking every monster passed."), BarbSkillKind::Combat, 16 },
	{ N_("Frenzy"), N_("Consecutive hits stack attack and movement speed until combat lulls."), BarbSkillKind::Combat, 16 },
	{ N_("Taunt"), N_("Forces nearby monsters to attack, and lowers their Chance to Hit."), BarbSkillKind::Warcry, 8 },

	{ N_("Iron Skin"), N_("Permanently increases the Armor Class gained from worn armor."), BarbSkillKind::Passive, 1 },
	{ N_("Toughness"), N_("Increases maximum Life and slightly improves hit recovery."), BarbSkillKind::Passive, 1 },
	{ N_("Leap Attack"), N_("Leaps to a nearby tile and lands with a powerful, knocking-back blow."), BarbSkillKind::Combat, 8 },
	{ N_("Stomp"), N_("Slams the ground, stunning and knocking back adjacent enemies."), BarbSkillKind::Combat, 16 },
	{ N_("Seismic Slam"), N_("Projects a shockwave forward, damaging and knocking back a line of foes."), BarbSkillKind::Combat, 16 },
	{ N_("Find Item"), N_("Searches a slain monster's corpse for extra loot. Consumes the corpse."), BarbSkillKind::Utility, 1 },
};

static_assert(sizeof(SkillTable) / sizeof(SkillTable[0]) == BarbSkillCount,
    "Barbarian skill table and BarbSkillCount disagree - the icon strip has one cell per entry");

} // namespace

const BarbSkillData &GetBarbSkillData(BarbSkill skill)
{
	assert(static_cast<size_t>(skill) < BarbSkillCount);
	return SkillTable[static_cast<size_t>(skill)];
}

string_view GetBarbSkillKindName(BarbSkillKind kind)
{
	switch (kind) {
	case BarbSkillKind::Combat:
		return _("Combat");
	case BarbSkillKind::Warcry:
		return _("Warcry");
	case BarbSkillKind::Passive:
		return _("Passive");
	case BarbSkillKind::Utility:
		return _("Utility");
	}
	return {};
}

bool IsBarbSkillUnlocked(const Player &player, BarbSkill skill)
{
	if (!ClassHasBarbSkills(player))
		return false;
	return player._pLevel >= GetBarbSkillData(skill).minLevel;
}

bool ClassHasBarbSkills(const Player &player)
{
	return player._pClass == HeroClass::Barbarian;
}

BarbSkill GetBarbSkillAtDisplayIndex(size_t index)
{
	assert(index < BarbSkillCount);

	// Built once - the order depends only on the table, which is a compile-time constant, and the
	// Abilities window asks for every visible row on every draw.
	static const std::array<BarbSkill, BarbSkillCount> DisplayOrder = [] {
		std::array<BarbSkill, BarbSkillCount> order {};
		for (size_t i = 0; i < BarbSkillCount; i++)
			order[i] = static_cast<BarbSkill>(i);
		std::stable_sort(order.begin(), order.end(), [](BarbSkill a, BarbSkill b) {
			const int levelA = GetBarbSkillData(a).minLevel;
			const int levelB = GetBarbSkillData(b).minLevel;
			if (levelA != levelB)
				return levelA < levelB;
			return string_view(_(GetBarbSkillData(a).name)) < string_view(_(GetBarbSkillData(b).name));
		});
		return order;
	}();

	return DisplayOrder[index];
}

} // namespace oracool
} // namespace devilution
