#include "oracool/endgame_boss.h"
#include "oracool/monster_variants.h"

#include "engine/random.hpp"
#include "levels/gendung.h"
#include "monster.h"
#include "oracool/area_level.h"
#include "utils/language.h"
#include "oracool/rift.h" // InRift - the generated set level

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
 * @brief From here up, every floor has one: the first rung of Hell difficulty.
 *
 * MaxAreaLevel is 64 - sixteen rungs across four difficulty blocks since 2026-09-12 - so half of it
 * plus one is exactly where the third block starts, alvl 33. Written as arithmetic on those two facts
 * rather than as a number, which is why the ladder shrinking from 96 moved this with it instead of
 * leaving it pointing at whatever that rung has become.
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

bool FightsAsUnique(const Monster &monster)
{
	return monster.isUnique() || monster.type().type == MT_DIABLO || IsEndgameBoss(monster);
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
	// A rift is the one set level that is GENERATED, and it asks for champions and a boss (PlaceRiftMonsters) -
	// which this refused, so none ever came (audit, 2026-09-27).
	if (currlevel == 0 || (setlevel && !InRift()))
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
	// The Unyielding variant (2026-09-19): an ordinary monster that stands its ground.
	if (VariantIsKnockbackImmune(monster))
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
