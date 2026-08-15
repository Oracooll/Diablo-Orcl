#include "oracool/lesser_uniques.h"

#include <algorithm>
#include <vector>

#include "engine/random.hpp"
#include "levels/gendung.h"
#include "missiles.h"
#include "monster.h"
#include "options.h"
#include "utils/language.h"
#include "utils/str_cat.hpp"

namespace devilution::oracool {

namespace {

/** @brief Armour a Fortified champion adds. Enough to notice, not enough to make it unhittable. */
constexpr int FortifiedArmorBonus = 20;

/** @brief Whether this level already loaded the sprite @p type walks around in. */
bool LevelHasMonsterType(_monster_id type)
{
	for (size_t i = 0; i < LevelMonsterTypeCount; i++) {
		if (LevelMonsterTypes[i].type == type)
			return true;
	}
	return false;
}

/**
 * @brief Whether @p data is quest content rather than a champion we may borrow.
 *
 * mtalkmsg is the game's own marker: the uniques that speak are the ones a quest is about - Garbud
 * greeting you, Zhar objecting to being disturbed, Lazarus taunting. Reading the data rather than
 * listing names by hand means a unique added later is classified correctly without anyone
 * remembering to update a list here.
 */
bool IsQuestUnique(const UniqueMonsterData &data)
{
	return data.mtalkmsg != TEXT_NONE;
}

/**
 * @brief Whether a champion of this identity is already standing on the level.
 *
 * Needed once a floor hosts several packs (2-6 by difficulty as of 1.6.1): without it a level could
 * field three of the same named champion, which reads as a bug rather than as variety. Scanning the
 * live monsters rather than tracking picks in a list also catches the floor's SCRIPTED unique for
 * free, since PlaceUniqueMonsters runs before this does.
 */
bool IsUniqueAlreadyPlaced(UniqueMonsterType type)
{
	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		if (Monsters[ActiveMonsters[i]].uniqueType == type)
			return true;
	}
	return false;
}

/**
 * @brief The namebook - given names and epithets, combined rather than listed.
 *
 * Oracool: user request (2026-08-15) - "you can rename them randomly. make a thousand random names
 * namebook and draw from there."
 *
 * A thousand names, but not a thousand strings. 50 given names against 26 epithets is 1,300
 * combinations from 76 entries, which is the same variety at a fifteenth of the size - and it stays
 * editable: adding one given name adds twenty-six names, not one. A literal list of a thousand would
 * also have to be translated a thousand times.
 *
 * This is what makes a repeated champion honest. The user's earlier constraint - never two Rotfeasts
 * on a floor - stops being a constraint when the second one is called something else entirely; the
 * SPRITE repeats, which is unavoidable on a floor that loads four monster types, but the character
 * does not.
 */
const char *const GivenNames[] = {
	"Malgrith", "Vorgath", "Skarn", "Yzrel", "Thagrim", "Nurvok", "Ashvane", "Belgor",
	"Cythrak", "Dreggan", "Ekthar", "Falgrim", "Ghorrim", "Hesk", "Ithrune", "Jarnok",
	"Kelvorn", "Lurghan", "Mordrek", "Naskul", "Orvath", "Prygg", "Quorrin", "Rhaskel",
	"Sythrin", "Tormund", "Ulgrek", "Vashk", "Wrethan", "Xarphel", "Yggron", "Zelkath",
	"Braugh", "Crellik", "Draskin", "Emberok", "Fenrig", "Grosvane", "Halgrin", "Ixthal",
	"Jorvath", "Kraggen", "Lysshen", "Morrow", "Nyxhal", "Oskaran", "Pelthar", "Rukkath",
	"Sorrell", "Tzavik",
};
constexpr size_t GivenNameCount = sizeof(GivenNames) / sizeof(GivenNames[0]);

/**
 * @brief The epithet half, marked for translation where the given names are not.
 *
 * A given name is an invented proper noun - "Malgrith" is Malgrith in every language, and asking a
 * translator to render it would produce fifty entries of busywork that all come back unchanged. An
 * epithet is an English phrase and reads as one, so these are the half worth extracting.
 */
const char *const Epithets[] = {
	N_("the Unclean"), N_("the Flayer"), N_("the Gravebound"), N_("the Wretched"), N_("the Devourer"),
	N_("the Hollow"), N_("the Blightborn"), N_("the Sundered"), N_("the Pale"), N_("the Rotting"),
	N_("the Merciless"), N_("the Forsaken"), N_("the Cruel"), N_("the Undying"), N_("the Ravenous"),
	N_("the Defiler"), N_("the Skinless"), N_("the Wailing"), N_("the Corpsemaker"), N_("the Blackened"),
	N_("the Vile"), N_("the Faithless"), N_("the Gorged"), N_("the Marrowdrinker"), N_("the Twisted"),
	N_("the Nameless"),
};
constexpr size_t EpithetCount = sizeof(Epithets) / sizeof(Epithets[0]);

/** @brief Whether a champion of @p type on this level already carries @p affix. */
bool IsAffixUsedBy(UniqueMonsterType type, LesserUniqueAffix affix)
{
	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		const Monster &monster = Monsters[ActiveMonsters[i]];
		if (monster.uniqueType == type && monster.lesserAffix == affix)
			return true;
	}
	return false;
}

} // namespace

std::optional<UniqueMonsterType> ChooseLesserUnique(bool excludeLevelOwned)
{
	// Collected rather than sampled-until-hit: the candidate set is usually small (a level loads a
	// handful of monster types, and only some have champions written for them), so rejection sampling
	// could spin for a long time or miss a level's only candidate entirely.
	//
	// Two lists, because a repeat is ACCEPTABLE but not PREFERRED. Shallow floors load few monster
	// types and so have few champions written for them, and Torment wanting six packs on level 2 was
	// simply getting fewer - the level ran out of distinct identities and stopped. A second Rotfeast
	// with a different modifier is a better answer than an empty floor (user call, 2026-08-15), but
	// only once every unused champion has been spent.
	std::vector<UniqueMonsterType> fresh;
	std::vector<UniqueMonsterType> repeats;
	for (size_t i = 0; UniqueMonstersData[i].mtype != -1; i++) {
		const UniqueMonsterData &data = UniqueMonstersData[i];
		if (IsQuestUnique(data))
			continue;
		if (excludeLevelOwned && data.mlevel == currlevel)
			continue;
		if (!LevelHasMonsterType(data.mtype))
			continue;

		const auto type = static_cast<UniqueMonsterType>(i);
		if (IsUniqueAlreadyPlaced(type))
			repeats.push_back(type);
		else
			fresh.push_back(type);
	}

	const std::vector<UniqueMonsterType> &pool = !fresh.empty() ? fresh : repeats;
	if (pool.empty())
		return std::nullopt;
	return pool[GenerateRnd(static_cast<int32_t>(pool.size()))];
}

int LesserUniqueCountForLevel()
{
	// Town has no champions, and neither do the set levels, whose contents are authored rather than
	// generated - dropping a random pack into Lachdanan's tomb would be vandalism, not variety.
	if (currlevel == 0 || setlevel)
		return 0;
	// Oracool: user call after playing it (2026-08-15) - "one pack per level is not ok. increase
	// packs 2-3 per normal. 3-4 nightmare. 4-5 hell. 5-6 torment."
	//
	// A RANGE rather than a fixed count, so two floors at the same difficulty are not the same
	// arithmetic twice - which is what "diversification and freshness" asked for in the first place.
	// The ladder also does something the difficulty ladder alone does not: Hell is not merely the
	// same dungeon with tougher monsters, it is a dungeon with more champions IN it.
	int fewest = 2;
	int most = 3;
	switch (sgGameInitInfo.nDifficulty) {
	case DIFF_NIGHTMARE:
		fewest = 3;
		most = 4;
		break;
	case DIFF_HELL:
		fewest = 4;
		most = 5;
		break;
	case DIFF_TORMENT:
		fewest = 5;
		most = 6;
		break;
	case DIFF_NORMAL:
		break;
	}

	const int packs = fewest + GenerateRnd(most - fewest + 1);
	// The dial multiplies the roll rather than replacing it, so 300% on Torment is genuinely
	// crowded. PlaceLesserUniques stops early when MaxMonsters runs out, which is the real ceiling
	// and is checked per pack rather than trusted to a number here.
	return std::max(packs * *sgOptions.Oracool.lesserUniqueDensityPercent / 100, 1);
}

LesserUniqueAffix RollLesserUniqueAffix(UniqueMonsterType forType)
{
	// From 1, not 0: a lesser unique always carries something. A champion with no modifier is just a
	// monster with more health, and the point of the system is that each one is a different fight.
	//
	// And when this identity is already on the floor - which happens once a level wants more packs
	// than it has distinct champions - the modifier MUST differ. That is the whole justification for
	// allowing the repeat: a second Warded Rotfeast is the same fight twice, where a Thunderous one
	// is a new one wearing a familiar face.
	std::vector<LesserUniqueAffix> available;
	for (int i = 1; i <= static_cast<int>(LesserUniqueAffix::LAST); i++) {
		const auto affix = static_cast<LesserUniqueAffix>(i);
		if (!IsAffixUsedBy(forType, affix))
			available.push_back(affix);
	}

	// Every modifier already spent on this identity - a floor with six packs and two champions can
	// reach that. Repeating one is better than refusing to place, and the pairing is still novel.
	if (available.empty())
		return static_cast<LesserUniqueAffix>(1 + GenerateRnd(static_cast<int>(LesserUniqueAffix::LAST)));
	return available[GenerateRnd(static_cast<int32_t>(available.size()))];
}

const char *GetLesserUniqueAffixName(LesserUniqueAffix affix)
{
	switch (affix) {
	case LesserUniqueAffix::Warded:
		return N_("Warded");
	case LesserUniqueAffix::Relentless:
		return N_("Relentless");
	case LesserUniqueAffix::Fortified:
		return N_("Fortified");
	case LesserUniqueAffix::Vampiric:
		return N_("Vampiric");
	case LesserUniqueAffix::Thunderous:
		return N_("Thunderous");
	case LesserUniqueAffix::None:
		break;
	}
	return "";
}

void ApplyLesserUniqueAffix(Monster &monster)
{
	switch (monster.lesserAffix) {
	case LesserUniqueAffix::Warded:
		// Resistant, never immune. An immune champion on a floor where the player has one damage type
		// is not a harder fight, it is an unwinnable one they have to walk away from.
		monster.resistance |= RESIST_MAGIC | RESIST_FIRE | RESIST_LIGHTNING;
		break;
	case LesserUniqueAffix::Relentless:
		// Knockback is how a player buys space. Taking it away is what makes this one frightening,
		// and it costs nothing to express - the flag already exists and the combat code already
		// honours it.
		monster.flags |= MFLAG_KNOCKBACK;
		break;
	case LesserUniqueAffix::Fortified:
		// Armour, not speed. The first draft of this list had "Fleet", and it had to go: monster
		// movement is paced by the ANIMATION, and animation timing lives on the shared CMonster rather
		// than on the individual - so making one champion fast would make every monster of its type
		// fast with it. Armour is per-monster, immediate, and reads just as clearly in a fight.
		monster.armorClass = static_cast<uint8_t>(std::min(monster.armorClass + FortifiedArmorBonus, 255));
		break;
	case LesserUniqueAffix::Vampiric:
	case LesserUniqueAffix::Thunderous:
	case LesserUniqueAffix::None:
		// Events rather than fields: they fire when the champion hits or dies.
		break;
	}
}

void OnLesserUniqueDealtDamage(Monster &monster, int damage)
{
	if (monster.lesserAffix != LesserUniqueAffix::Vampiric || damage <= 0)
		return;
	// A third of what it dealt, and never past full - the champion claws back ground during a long
	// fight without being able to out-heal a player who is winning.
	constexpr int VampiricPercent = 33;
	monster.hitPoints = std::min(monster.hitPoints + damage * VampiricPercent / 100, monster.maxHitPoints);
}

void TintLesserUnique(Monster &monster)
{
	if (!monster.uniqueMonsterTRN)
		return;

	// Oracool: user request (2026-08-15) - "you can just recolor them a little bit." A floor that
	// loads four monster types will show the same sprite twice however the champions are named, so
	// the palette does the rest of the work the name started.
	//
	// A step or two ALONG each colour's own 16-shade ramp, not a hue change: the champion stays
	// recognisably what it is and reads a little paler or a little darker than its twin. Anything
	// stronger and the game's existing unique palettes - which are hand-picked per champion - stop
	// meaning what they were chosen to mean.
	const int shift = static_cast<int>(monster.aiSeed % 5) - 2; // -2..+2, and 0 is a valid outcome
	if (shift == 0)
		return;

	uint8_t *trn = monster.uniqueMonsterTRN.get();
	for (int i = 0; i < 256; i++) {
		const uint8_t mapped = trn[i];
		// Entries below 128 are the level-specific half of the palette, which differs per dungeon
		// type - shifting inside it would change colour unpredictably from floor to floor.
		if (mapped < 128)
			continue;
		const int ramp = mapped & 0xF0;
		const int within = (mapped & 0x0F) + shift;
		// Clamped by SKIPPING rather than by saturating: a colour at the end of its ramp stays put,
		// where saturating would pile several shades onto the same index and flatten the shading.
		if (within < 0 || within > 15)
			continue;
		trn[i] = static_cast<uint8_t>(ramp | within);
	}
}

std::string GetLesserUniqueName(const Monster &monster)
{
	// Derived from aiSeed rather than stored, and that is the whole trick: aiSeed is already per
	// monster AND already saved, so a champion keeps its name across a save and reload without this
	// costing a field, a string table, or another look at the save format.
	//
	// Two independent draws from one seed - the division moves to a different part of the number, so
	// the given name and the epithet do not march in lockstep as the seed increments.
	const uint32_t seed = monster.aiSeed;
	const char *given = GivenNames[seed % GivenNameCount];
	const char *epithet = Epithets[(seed / GivenNameCount) % EpithetCount];
	return StrCat(given, " ", _(epithet));
}

void OnLesserUniqueKilled(Monster &monster)
{
	if (monster.lesserAffix != LesserUniqueAffix::Thunderous)
		return;
	// The discharge is the reward for killing it AND the sting for standing next to it - the corpse
	// is not a safe place to be. Reuses the mini-Nova ring built for Fist of the Heavens at 1.5.78.
	AddMissile(monster.position.tile, monster.position.tile, Direction::South,
	    MissileID::MiniNovaBall, TARGET_PLAYERS, -1, monster.maxDamage, 0);
}

} // namespace devilution::oracool
