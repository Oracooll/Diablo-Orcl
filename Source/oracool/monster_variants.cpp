#include "oracool/monster_variants.h"

#include <algorithm>

#include "monster.h"
#include "oracool/lesser_uniques.h"

namespace devilution::oracool {

namespace {

/**
 * @brief How many monsters in a hundred are a variant.
 *
 * Common enough that a floor looks varied, rare enough that the ordinary monster is still the thing
 * you are fighting. Below about ten the effect is invisible; much above twenty and the palette stops
 * meaning anything because everything is wearing one.
 */
constexpr int VariantPercent = 15;

constexpr int VariantCount = static_cast<int>(MonsterVariant::LAST);

/** @brief Life and damage adjustments, as percentages, for the two that trade one for the other. */
constexpr int HollowLifePercent = 135;
constexpr int HollowDamagePercent = 75;
constexpr int FeralLifePercent = 70;
constexpr int FeralDamagePercent = 130;

/** @brief How far along its ramp a variant's colour moves. Kept to the same modest range the
 * champion tint uses, and for the same reason: any stronger and the creature stops reading as
 * itself. Never zero - a variant that does not look different is a lie told by its name. */
constexpr int VariantTintShift = 2;

uint8_t ScalePercent(uint8_t base, int percent)
{
	return static_cast<uint8_t>(std::clamp(base * percent / 100, 1, 255));
}

/**
 * @brief Builds the monster's own palette translation and walks it along its ramps.
 *
 * An ordinary monster has no TRN at all - only uniques load one - so the identity map comes first.
 * Everything after that is TintLesserUnique's rule, including the reason entries below 128 are
 * skipped: that half of the palette is level-specific, and shifting inside it changes colour
 * unpredictably from floor to floor.
 */
void TintVariant(Monster &monster, int shift)
{
	if (!monster.uniqueMonsterTRN)
		monster.uniqueMonsterTRN = std::make_unique<uint8_t[]>(256);
	uint8_t *trn = monster.uniqueMonsterTRN.get();
	for (int i = 0; i < 256; i++)
		trn[i] = static_cast<uint8_t>(i);
	for (int i = 128; i < 256; i++) {
		const int ramp = trn[i] & 0xF0;
		const int within = (trn[i] & 0x0F) + shift;
		// Clamped by SKIPPING rather than saturating, so a colour already at the end of its ramp
		// stays put instead of piling several shades onto one index and flattening the shading.
		if (within < 0 || within > 15)
			continue;
		trn[i] = static_cast<uint8_t>(ramp | within);
	}
}

} // namespace

MonsterVariant VariantOf(const Monster &monster)
{
	// Uniques and champions are excluded at the source rather than at the call site: they carry an
	// identity already, and a "Feral Gharbad the Weak" is two names fighting over one monster.
	if (monster.isUnique() || monster.lesserAffix != LesserUniqueAffix::None)
		return MonsterVariant::None;

	// Two INDEPENDENT draws out of one seed - whether, and which. Taken from different ends of the
	// value so a monster that just missed being a variant is not always the same variant when a
	// sibling seed does qualify.
	const uint32_t seed = monster.rndItemSeed;
	if (static_cast<int>(seed % 100) >= VariantPercent)
		return MonsterVariant::None;
	return static_cast<MonsterVariant>(1 + static_cast<int>((seed / 100) % VariantCount));
}

const char *VariantNamePrefix(MonsterVariant variant)
{
	switch (variant) {
	case MonsterVariant::Ashen:
		return N_("Ashen");
	case MonsterVariant::Stormtouched:
		return N_("Stormtouched");
	case MonsterVariant::Hollow:
		return N_("Hollow");
	case MonsterVariant::Feral:
		return N_("Feral");
	case MonsterVariant::None:
		break;
	}
	return nullptr;
}

void ApplyMonsterVariant(Monster &monster)
{
	const MonsterVariant variant = VariantOf(monster);
	if (variant == MonsterVariant::None)
		return;

	switch (variant) {
	case MonsterVariant::Ashen:
		// Resistant, never immune - the same rule the Warded champion follows. An immune ordinary
		// monster on a floor where the player has one damage type is not a harder fight, it is a
		// wall they have to walk away from, and unlike a champion there would be a dozen of them.
		monster.resistance |= RESIST_FIRE;
		break;
	case MonsterVariant::Stormtouched:
		monster.resistance |= RESIST_LIGHTNING;
		break;
	case MonsterVariant::Hollow:
		monster.maxHitPoints = monster.maxHitPoints * HollowLifePercent / 100;
		monster.hitPoints = monster.maxHitPoints;
		monster.minDamage = ScalePercent(monster.minDamage, HollowDamagePercent);
		monster.maxDamage = ScalePercent(monster.maxDamage, HollowDamagePercent);
		break;
	case MonsterVariant::Feral:
		monster.maxHitPoints = std::max(monster.maxHitPoints * FeralLifePercent / 100, 64);
		monster.hitPoints = monster.maxHitPoints;
		monster.minDamage = ScalePercent(monster.minDamage, FeralDamagePercent);
		monster.maxDamage = ScalePercent(monster.maxDamage, FeralDamagePercent);
		break;
	case MonsterVariant::None:
		return;
	}

	// Direction of the shift alternates with the variant, so the four do not all read as "the pale
	// one" - two lighten and two darken.
	const int shift = (static_cast<int>(variant) % 2 == 0) ? VariantTintShift : -VariantTintShift;
	TintVariant(monster, shift);
}

} // namespace devilution::oracool
