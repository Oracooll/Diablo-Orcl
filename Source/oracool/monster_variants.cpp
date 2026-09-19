#include "oracool/monster_variants.h"

#include <algorithm>

#include "lighting.h"
#include "misdat.h"
#include "monster.h"
#include "multi.h"
#include "options.h"
#include "oracool/lesser_uniques.h"

namespace devilution::oracool {

namespace {

/**
 * @brief How many monsters in a hundred are a variant, on NORMAL.
 *
 * Common enough that a floor looks varied, rare enough that the ordinary monster is still the thing
 * you are fighting. Below about ten the effect is invisible; much above twenty and the palette stops
 * meaning anything because everything is wearing one.
 *
 * The upper difficulties climb past twenty deliberately, and that same reasoning is why they stop
 * at twenty-eight rather than going further: by Torment a player has seen every variant many times,
 * so the rate can afford to be high enough to shape a floor, but a floor where a third of the
 * monsters are recoloured has made the recolour the default and the ordinary monster the surprise.
 */
constexpr int VariantPercent = 15;

/**
 * @brief The ceiling the INI dial cannot push past.
 *
 * The Torment rung, which is where the ladder deliberately stops. A dial that could take a floor to
 * 84% recoloured would not be a stronger version of this feature - it would be a different one, in
 * which the ordinary monster is the surprise. Stated as its own constant so the ladder and its
 * ceiling cannot drift apart.
 */
constexpr int MaxVariantPercent = 28;

/** @brief Life and damage adjustments, as percentages, for the two that trade one for the other. */
constexpr int HollowLifePercent = 135;
constexpr int HollowDamagePercent = 75;
constexpr int FeralLifePercent = 70;
constexpr int FeralDamagePercent = 130;
// The 2026-09-19 kinds' numbers - first guesses, to be corrected from play like Hollow's and Feral's.
constexpr int IronhideArmorPercent = 150;
constexpr int BrutalSpecialPercent = 150;
constexpr int LuminousRadius = 5;

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

/**
 * @brief Which variants each dungeon type offers.
 *
 * The rosters are the point of this table, so the reasoning is here rather than in a commit
 * message. Each floor gets a SUBSET, because a variant that can appear anywhere describes nothing:
 * with one global list the Cathedral and Hell drew from the same four and "Ashen" was texture.
 *
 *  - CATHEDRAL is the shallow end and gets the two body variants, Hollow and Feral. They teach the
 *    idea - this one is tougher, this one hits harder - without an elemental resistance the player
 *    may have no answer to on floor two.
 *  - CATACOMBS adds Stormtouched. Lightning is the first resistance worth planning around, and by
 *    the Catacombs a caster has more than one spell.
 *  - CAVES swap to Ashen for the obvious reason: it is the lava tileset, and fire-hardened belongs
 *    where fire lives. Feral stays; Hollow does not, so the Caves read as faster and meaner rather
 *    than merely tougher.
 *  - HELL offers all four. The deepest floors are where the player is expected to have answers.
 *  - CRYPT is Hollow and Stormtouched - the drained, the charged; no fire in a Crypt.
 *  - NEST is Feral and Ashen, which is what a nest of hive beasts over lava should be.
 *  - TOWN has none, and this is load-bearing rather than tidy: an Ashen Griswold is nonsense.
 *
 * Kept as one table with one accessor so "which variants live here" has a single answer. A second
 * list somewhere - a display filter, a test's own copy - is how a roster comes to disagree with
 * itself.
 */
// 2026-09-19, the user's rule for the eleven new kinds: "I want all of these monster variants to be
// meet-able in all zones except cathedral." So the Cathedral keeps its two teaching variants and
// every other dungeon offers its old subset PLUS all eleven - the old subset is kept so the Caves
// still read as faster and meaner (no Hollow) and the Crypt still has no fire (no Ashen).
constexpr MonsterVariant CathedralRoster[] = { MonsterVariant::Hollow, MonsterVariant::Feral };
constexpr MonsterVariant CatacombsRoster[] = { MonsterVariant::Hollow, MonsterVariant::Feral,
	MonsterVariant::Stormtouched,
	MonsterVariant::Veiled, MonsterVariant::Ironhide, MonsterVariant::Brutal, MonsterVariant::Frenzied,
	MonsterVariant::Fleet, MonsterVariant::Searing, MonsterVariant::Voltaic, MonsterVariant::Venomous,
	MonsterVariant::Unyielding, MonsterVariant::Gilded, MonsterVariant::Luminous };
constexpr MonsterVariant CavesRoster[] = { MonsterVariant::Feral, MonsterVariant::Ashen,
	MonsterVariant::Veiled, MonsterVariant::Ironhide, MonsterVariant::Brutal, MonsterVariant::Frenzied,
	MonsterVariant::Fleet, MonsterVariant::Searing, MonsterVariant::Voltaic, MonsterVariant::Venomous,
	MonsterVariant::Unyielding, MonsterVariant::Gilded, MonsterVariant::Luminous };
constexpr MonsterVariant HellRoster[] = { MonsterVariant::Hollow, MonsterVariant::Feral,
	MonsterVariant::Stormtouched, MonsterVariant::Ashen,
	MonsterVariant::Veiled, MonsterVariant::Ironhide, MonsterVariant::Brutal, MonsterVariant::Frenzied,
	MonsterVariant::Fleet, MonsterVariant::Searing, MonsterVariant::Voltaic, MonsterVariant::Venomous,
	MonsterVariant::Unyielding, MonsterVariant::Gilded, MonsterVariant::Luminous };
constexpr MonsterVariant NestRoster[] = { MonsterVariant::Feral, MonsterVariant::Ashen,
	MonsterVariant::Veiled, MonsterVariant::Ironhide, MonsterVariant::Brutal, MonsterVariant::Frenzied,
	MonsterVariant::Fleet, MonsterVariant::Searing, MonsterVariant::Voltaic, MonsterVariant::Venomous,
	MonsterVariant::Unyielding, MonsterVariant::Gilded, MonsterVariant::Luminous };
constexpr MonsterVariant CryptRoster[] = { MonsterVariant::Hollow, MonsterVariant::Stormtouched,
	MonsterVariant::Veiled, MonsterVariant::Ironhide, MonsterVariant::Brutal, MonsterVariant::Frenzied,
	MonsterVariant::Fleet, MonsterVariant::Searing, MonsterVariant::Voltaic, MonsterVariant::Venomous,
	MonsterVariant::Unyielding, MonsterVariant::Gilded, MonsterVariant::Luminous };

struct Roster {
	const MonsterVariant *variants;
	int count;
};

Roster RosterFor(dungeon_type dungeon)
{
	switch (dungeon) {
	case DTYPE_CATHEDRAL:
		return { CathedralRoster, static_cast<int>(std::size(CathedralRoster)) };
	case DTYPE_CATACOMBS:
		return { CatacombsRoster, static_cast<int>(std::size(CatacombsRoster)) };
	case DTYPE_CAVES:
		return { CavesRoster, static_cast<int>(std::size(CavesRoster)) };
	case DTYPE_HELL:
		return { HellRoster, static_cast<int>(std::size(HellRoster)) };
	case DTYPE_NEST:
		return { NestRoster, static_cast<int>(std::size(NestRoster)) };
	case DTYPE_CRYPT:
		return { CryptRoster, static_cast<int>(std::size(CryptRoster)) };
	case DTYPE_TOWN:
	case DTYPE_NONE:
		break;
	}
	return { nullptr, 0 };
}

/** @brief The ladder before the INI dial is applied. */
int BaseVariantPercentFor(_difficulty difficulty)
{
	// Normal keeps the shipped 15. The climb is modest for the reason recorded on VariantPercent:
	// past about a third the recolour becomes the default and the ordinary monster the surprise.
	switch (difficulty) {
	case DIFF_NIGHTMARE:
		return 19;
	case DIFF_HELL:
		return 23;
	case DIFF_TORMENT:
		return 28;
	default:
		return VariantPercent;
	}
}

int VariantPercentFor(_difficulty difficulty)
{
	// Monster Variant Chance (INI, 2026-08-31) scales the LADDER rather than replacing it, so the
	// per-difficulty shape above survives the dial - see the option's own comment in options.h.
	const int scaled = BaseVariantPercentFor(difficulty) * *sgOptions.Oracool.monsterVariantChancePercent / 100;
	// Clamped at the Torment ladder's own ceiling. Without this, 300 puts Torment at 84% and the
	// recolour stops meaning anything - which is the exact failure the header warns about, and the
	// reason the ladder stops at 28 rather than climbing further on its own.
	return std::clamp(scaled, 0, MaxVariantPercent);
}

int VariantRosterSize(dungeon_type dungeon)
{
	return RosterFor(dungeon).count;
}

MonsterVariant VariantInRoster(dungeon_type dungeon, int index)
{
	const Roster roster = RosterFor(dungeon);
	if (index < 0 || index >= roster.count)
		return MonsterVariant::None;
	return roster.variants[index];
}

MonsterVariant VariantForSeed(uint32_t seed, dungeon_type dungeon)
{
	const Roster roster = RosterFor(dungeon);
	if (roster.count == 0)
		return MonsterVariant::None;

	// Two INDEPENDENT draws out of one seed - whether, and which. Taken from different ends of the
	// value so a monster that just missed being a variant is not always the same variant when a
	// sibling seed does qualify.
	//
	// The RATE climbs with the difficulty (v1.9.16). A re-run walks the same twenty-four floors, so
	// without this the fourth pass through the Cathedral met exactly as many variants as the first
	// - the monsters were bigger and nothing else about the encounter had changed. Denser special
	// encounters is a thing a player notices; a bigger health bar on the same fight is not.
	if (static_cast<int>(seed % 100) >= VariantPercentFor(sgGameInitInfo.nDifficulty))
		return MonsterVariant::None;
	return roster.variants[(seed / 100) % static_cast<uint32_t>(roster.count)];
}

MonsterVariant VariantOf(const Monster &monster)
{
	// Uniques and champions are excluded at the source rather than at the call site: they carry an
	// identity already, and a "Feral Gharbad the Weak" is two names fighting over one monster.
	if (monster.isUnique() || monster.lesserAffix != LesserUniqueAffix::None)
		return MonsterVariant::None;

	// And the SCRIPTED monsters, which is the same argument one step further out (user, 2026-09-12:
	// "dont put prefix on the dark lord name", then "dont recolor the dark lord. keep it or return
	// it to vanilla").
	//
	// Diablo slipped through both tests above. He is MT_DIABLO placed by his own quest, not a
	// UniqueMonstersData entry, so isUnique() is FALSE for him - and he was therefore picking up a
	// variant like any Skeleton: "Ashen The Dark Lord" in the health bar, and his own palette
	// replaced by a variant TRN.
	//
	// MonsterAvailability::Never is the predicate rather than a check for MT_DIABLO, because it says
	// the actual reason: a variant is a KIND of ordinary monster, and a monster the dungeon never
	// places at random is not an ordinary monster. It also covers the types that never spawn at all
	// (Wyrm, Cave Slug, Devil Wyrm, Devourer), where excluding them changes nothing, and the quest
	// bosses that isUnique() already caught.
	//
	// Nothing is stored, so this returns him to vanilla immediately: the variant is derived from
	// rndItemSeed every time it is asked for, and both the name and the recolour ask here.
	if (monster.data().availability == MonsterAvailability::Never)
		return MonsterVariant::None;

	const MonsterVariant variant = VariantForSeed(monster.rndItemSeed, leveltype);
	// Brutal is its special attack; a type with no special has nothing for it to be, and an
	// ordinary monster is the honest answer rather than a recolour that does nothing.
	if (variant == MonsterVariant::Brutal && !monster.data().hasSpecial)
		return MonsterVariant::None;
	return variant;
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
	case MonsterVariant::Veiled:
		return N_("Veiled");
	case MonsterVariant::Ironhide:
		return N_("Ironhide");
	case MonsterVariant::Brutal:
		return N_("Brutal");
	case MonsterVariant::Frenzied:
		return N_("Frenzied");
	case MonsterVariant::Fleet:
		return N_("Fleet");
	case MonsterVariant::Searing:
		return N_("Searing");
	case MonsterVariant::Voltaic:
		return N_("Voltaic");
	case MonsterVariant::Venomous:
		return N_("Venomous");
	case MonsterVariant::Unyielding:
		return N_("Unyielding");
	case MonsterVariant::Gilded:
		return N_("Gilded");
	case MonsterVariant::Luminous:
		return N_("Luminous");
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
	// The eleven of 2026-09-19. The field kinds land here; the hook kinds (Frenzied, Fleet, Searing,
	// Voltaic, Venomous, Unyielding, Gilded) are asked at their seams through the Variant* functions
	// below and change nothing at spawn but the recolour.
	case MonsterVariant::Veiled:
		monster.resistance |= RESIST_MAGIC;
		break;
	case MonsterVariant::Ironhide:
		monster.armorClass = ScalePercent(monster.armorClass, IronhideArmorPercent);
		break;
	case MonsterVariant::Brutal:
		// A monster without a special attack never draws Brutal (VariantOf declines it), so this
		// never lands on nothing - but the guard stays, because the draw is the part that changes.
		if (monster.data().hasSpecial) {
			monster.minDamageSpecial = ScalePercent(monster.minDamageSpecial, BrutalSpecialPercent);
			monster.maxDamageSpecial = ScalePercent(monster.maxDamageSpecial, BrutalSpecialPercent);
		}
		break;
	case MonsterVariant::Luminous:
		// A torch-bearer: the light rides the monster (the walk code moves lightId) and the death
		// path frees it, both exactly as a unique's light already does.
		if (monster.lightId == NO_LIGHT)
			monster.lightId = AddLight(monster.position.tile, LuminousRadius);
		break;
	case MonsterVariant::Frenzied:
	case MonsterVariant::Fleet:
	case MonsterVariant::Searing:
	case MonsterVariant::Voltaic:
	case MonsterVariant::Venomous:
	case MonsterVariant::Unyielding:
	case MonsterVariant::Gilded:
		break;
	case MonsterVariant::None:
		return;
	}

	// Direction of the shift alternates with the variant, so the kinds do not all read as "the pale
	// one" - half lighten and half darken.
	const int shift = (static_cast<int>(variant) % 2 == 0) ? VariantTintShift : -VariantTintShift;
	TintVariant(monster, shift);
}

int VariantAnimTickDelta(const Monster &monster, MonsterGraphic graphic)
{
	const MonsterVariant variant = VariantOf(monster);
	if (variant == MonsterVariant::Frenzied && graphic == MonsterGraphic::Attack)
		return -1;
	if (variant == MonsterVariant::Fleet && graphic == MonsterGraphic::Walk)
		return -1;
	return 0;
}

DamageType VariantHitElement(const Monster &monster)
{
	switch (VariantOf(monster)) {
	case MonsterVariant::Searing:
		return DamageType::Fire;
	case MonsterVariant::Voltaic:
		return DamageType::Lightning;
	default:
		return DamageType::Physical;
	}
}

bool VariantPoisonsOnHit(const Monster &monster)
{
	return VariantOf(monster) == MonsterVariant::Venomous;
}

bool VariantIsKnockbackImmune(const Monster &monster)
{
	return VariantOf(monster) == MonsterVariant::Unyielding;
}

bool VariantDropsGilded(const Monster &monster)
{
	return VariantOf(monster) == MonsterVariant::Gilded;
}

} // namespace devilution::oracool
