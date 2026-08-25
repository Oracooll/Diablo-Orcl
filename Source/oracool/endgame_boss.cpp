#include "oracool/endgame_boss.h"

#include "engine/random.hpp"
#include "levels/gendung.h"
#include "monster.h"
#include "oracool/area_level.h"
#include "utils/language.h"

namespace devilution::oracool {

namespace {

/**
 * @brief The boss profile, against the champion's 300/150/+4/4 for comparison.
 *
 * Eight times life rather than three, but only two times damage rather than one and a half. The
 * split is the same judgement PlaceLesserUniqueMonst already made and is worth restating: a boss
 * should be a LONG fight, not a fast death. Health is a fight the player can read and retreat from;
 * damage is a fight that ends before they know what happened, and at eight times the health there
 * is plenty of time for a mistake to be fatal anyway.
 */
constexpr int HealthPercent = 800;
constexpr int DamagePercent = 200;
constexpr int ArmorBonus = 10;
constexpr int PackSize = 6;

/** @brief Extra life for the Adamant trait, on top of the profile above. */
constexpr int AdamantExtraPercent = 40;
/** @brief What Devouring claws back, as a percentage of damage dealt. */
constexpr int DevouringPercent = 40;
/** @brief Warding's armour-class contribution, beside its resistances. */
constexpr int WardingArmorBonus = 8;

/**
 * @brief From here up, every floor has one. Area level 49 is the first Hell floor.
 *
 * MaxAreaLevel is 96 - twenty-four floors across four difficulty blocks - so 49 is exactly where
 * the third block starts. Written as arithmetic on those two facts rather than as the number 49, so
 * that a change to the floor count moves this with it instead of leaving it pointing at whatever
 * alvl 49 has become.
 */
constexpr int BossGuaranteedAreaLevel = MaxAreaLevel / 2 + 1;

/**
 * @brief Below the guarantee, a chance on floors past here - two thirds of the way through a block.
 *
 * The point is that a player meets the IDEA of a boss before they meet the one that matters. A
 * first encounter at alvl 49, with a treasure class behind it and no prior warning, is a difficulty
 * spike wearing a reward.
 */
constexpr int BossIntroAreaLevel = MaxAreaLevel / 6;
constexpr int BossIntroPercent = 25;

} // namespace

int BossHealthPercent() { return HealthPercent; }
int BossDamagePercent() { return DamagePercent; }
int BossArmorBonus() { return ArmorBonus; }
int BossPackSize() { return PackSize; }

bool IsEndgameBoss(const Monster &monster)
{
	return monster.lesserAffix == LesserUniqueAffix::Dread;
}

BossTrait SecondaryTraitOf(uint16_t seed)
{
	constexpr int TraitCount = static_cast<int>(BossTrait::LAST) + 1;
	// The name seed's LOW bits pick the name and its tint (GetLesserUniqueName, TintLesserUnique),
	// so the trait is taken from further up the value. Otherwise every Devouring boss would also be
	// the same colour and carry the same name, and three independent-looking properties would turn
	// out to be one.
	return static_cast<BossTrait>((seed / 97) % TraitCount);
}

BossTrait SecondaryTraitFor(const Monster &monster)
{
	return SecondaryTraitOf(monster.lesserNameSeed);
}

const char *BossTraitName(BossTrait trait)
{
	switch (trait) {
	case BossTrait::Warding:
		return N_("Warding");
	case BossTrait::Implacable:
		return N_("Implacable");
	case BossTrait::Devouring:
		return N_("Devouring");
	case BossTrait::Adamant:
		return N_("Adamant");
	}
	return "";
}

int BossCountForLevel()
{
	// Town has none, and neither do the set levels - the same two exclusions
	// LesserUniqueCountForLevel makes, for the same reason: their contents are authored, and
	// dropping a boss into Lachdanan's tomb is vandalism rather than variety.
	if (currlevel == 0 || setlevel)
		return 0;

	const int alvl = CurrentAreaLevel();
	if (alvl >= BossGuaranteedAreaLevel)
		return 1;
	if (alvl >= BossIntroAreaLevel && GenerateRnd(100) < BossIntroPercent)
		return 1;
	return 0;
}

void ApplyBossTrait(Monster &monster)
{
	if (!IsEndgameBoss(monster))
		return;

	switch (SecondaryTraitFor(monster)) {
	case BossTrait::Warding:
		// Resistant, never immune - the rule the champion list set and the monster variants kept.
		// An immune boss on a floor where the player has one damage type is not a harder fight, it
		// is a fight they have to walk away from, and this one is the floor's whole reward.
		monster.resistance |= RESIST_MAGIC | RESIST_FIRE | RESIST_LIGHTNING;
		monster.armorClass = static_cast<uint8_t>(std::min(monster.armorClass + WardingArmorBonus, 255));
		break;
	case BossTrait::Implacable:
		// Nothing to set - the immunity is a question asked at M_GetKnockback, not a field. Same
		// inversion Relentless carried: MFLAG_KNOCKBACK makes the monster knock the PLAYER back,
		// which is neither what the name says nor what the trait was for. See IsKnockbackImmune.
		break;
	case BossTrait::Adamant:
		monster.maxHitPoints += monster.maxHitPoints * AdamantExtraPercent / 100;
		monster.hitPoints = monster.maxHitPoints;
		break;
	case BossTrait::Devouring:
		// An event, not a field: it fires when the boss lands a hit.
		break;
	}
}

bool IsKnockbackImmune(const Monster &monster)
{
	if (monster.lesserAffix == LesserUniqueAffix::Relentless)
		return true;
	return IsEndgameBoss(monster) && SecondaryTraitFor(monster) == BossTrait::Implacable;
}

void OnBossDealtDamage(Monster &monster, int damage)
{
	if (!IsEndgameBoss(monster) || damage <= 0)
		return;
	if (SecondaryTraitFor(monster) != BossTrait::Devouring)
		return;
	monster.hitPoints = std::min(monster.hitPoints + damage * DevouringPercent / 100, monster.maxHitPoints);
}

} // namespace devilution::oracool
