#include "oracool/lesser_uniques.h"

#include "oracool/endgame_boss.h"
#include "oracool/monster_difficulty.h"
#include "oracool/monster_variants.h"

#include <algorithm>
#include <cstring>
#include <vector>

#include "engine/random.hpp"
#include "levels/gendung.h"
#include "missiles.h"
#include "monster.h"
#include "options.h"
#include "oracool/rng_streams.h"
#include "utils/language.h"
#include "utils/str_cat.hpp"
#include "oracool/rift.h" // InRift - the generated set level

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
 * TWO tests, and the second was missing until the 2026-08-15 self-audit.
 *
 * mtalkmsg catches the uniques that SPEAK - Gharbad greeting you, Zhar objecting to being disturbed,
 * Lazarus taunting. That is the game's own marker for a quest boss, and reading it beats listing
 * names by hand.
 *
 * But four quest bosses do not talk: the Skeleton King, the Butcher, the Hork Demon and Na-Krul, all
 * TEXT_NONE. Every one of them was a legal candidate, and each carried its own hazard, because
 * GetUniqueMonstPosition special-cases exactly this set:
 *
 *   - the Butcher's position is read off SetPiece, so a borrowed one would spawn in the real
 *     Butcher's own room
 *   - Na-Krul's case WRITES UberDiabloMonsterIndex, so a borrowed one would repoint - or with
 *     UberRow unset, erase - the state the Na-Krul quest runs on
 *
 * Their monster types are all MonsterAvailability::Never, which is the game's marker for "this
 * creature exists only for scripted content". Nothing that appears on a random level can be Never,
 * so the test costs no ordinary champion and catches every quest boss added later for free - the
 * same reason mtalkmsg was chosen over a name list.
 *
 * LevelHasMonsterType is not protection here: these types ARE loaded on the floor their quest owns,
 * and PlaceLesserUniques runs after PlaceQuestMonsters, so that is precisely where they were
 * reachable.
 */
bool IsQuestUnique(const UniqueMonsterData &data)
{
	if (data.mtalkmsg != TEXT_NONE)
		return true;
	return MonstersData[static_cast<size_t>(data.mtype)].availability == MonsterAvailability::Never;
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

/** @brief How many shades TintLesserUnique picks between: -2..+2, so five. */
constexpr uint32_t TintChoices = 5;

/**
 * @brief The mixed-radix range of Monster::lesserNameSeed: given, then epithet, then tint.
 *
 * 50 x 26 x 5 = 6,500, which is both small enough for the two spare save bytes and small enough to
 * stay under GenerateRnd's 0x7FFF bias-correction threshold. Each field reads a different "digit", so
 * a champion's name and its colour are independent draws off one number rather than two views of the
 * same low bits.
 */
constexpr uint32_t NameSeedRange = GivenNameCount * EpithetCount * TintChoices;
static_assert(NameSeedRange <= 0x7FFF,
    "lesserNameSeed's range must stay inside GenerateRnd's bias-corrected band - see RollLesserUniqueNameSeed");

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
	// A rift is the one set level that is GENERATED, and it asks for champions and a boss (PlaceRiftMonsters) -
	// which this refused, so none ever came (audit, 2026-09-27).
	if (currlevel == 0 || (setlevel && !InRift()))
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
	// And the DIFFICULTY gates which exist at all (v1.9.16). All six were on the table from the
	// first floor of Normal, so the only thing a re-run changed about a champion was its numbers.
	const _difficulty difficulty = sgGameInitInfo.nDifficulty;
	std::vector<LesserUniqueAffix> allowed;
	std::vector<LesserUniqueAffix> available;
	for (int i = 1; i <= static_cast<int>(LesserUniqueAffix::LAST); i++) {
		const auto affix = static_cast<LesserUniqueAffix>(i);
		if (!ChampionAffixAllowedOn(affix, difficulty))
			continue;
		allowed.push_back(affix);
		if (!IsAffixUsedBy(forType, affix))
			available.push_back(affix);
	}

	// Every modifier already spent on this identity - a floor with six packs and two champions can
	// reach that. Repeating one is better than refusing to place, and the pairing is still novel.
	//
	// The repeat is drawn from ALLOWED rather than from the whole enum, which is the half that
	// would have leaked: the old fallback rolled 1..LAST directly, so a crowded Normal floor could
	// hand out the Vampiric champion the difficulty gate above had just excluded - and only on
	// crowded floors, which is exactly the kind of bug that never reproduces on demand.
	if (available.empty()) {
		if (allowed.empty())
			return LesserUniqueAffix::Relentless; // unreachable: Normal's three are always allowed
		return allowed[GenerateRnd(static_cast<int32_t>(allowed.size()))];
	}
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
	case LesserUniqueAffix::Colossal:
		return N_("Colossal");
	case LesserUniqueAffix::Dread:
		// A boss's own word. Its SECOND trait supplies a second one, so the full name reads
		// "Dread Warding <name>" - see oracool/endgame_boss.h.
		return N_("Dread");
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
		// Knockback is how a player buys space, and taking it away is what makes this one
		// frightening. Nothing to set here: the immunity is asked as a question at the one place
		// the player knocks a monster back - see oracool::IsKnockbackImmune.
		//
		// It used to raise MFLAG_KNOCKBACK, and the comment claimed "the combat code already
		// honours it". It does, for the opposite thing: that flag lives in the monster-hits-PLAYER
		// path and means "this monster's blows knock YOU back". So Relentless spent months granting
		// an offensive power nobody designed and never granting the immunity it advertised - the
		// player's knockback goes through M_GetKnockback, which never consulted the flag at all.
		break;
	case LesserUniqueAffix::Fortified:
		// Armour, not speed. The first draft of this list had "Fleet", and it had to go: monster
		// movement is paced by the ANIMATION, and animation timing lives on the shared CMonster rather
		// than on the individual - so making one champion fast would make every monster of its type
		// fast with it. Armour is per-monster, immediate, and reads just as clearly in a fight.
		monster.armorClass = static_cast<uint8_t>(std::min(monster.armorClass + FortifiedArmorBonus, 255));
		break;
	case LesserUniqueAffix::Colossal:
		// The size itself is drawn from scaled sprite data (oracool/monster_scale.h); what belongs
		// HERE is the part that makes a bigger creature a harder one, so the silhouette is a promise
		// rather than a costume. Life rather than damage: a Colossal champion should take longer to
		// bring down, not delete a player who misjudged its reach.
		monster.maxHitPoints += monster.maxHitPoints / 2;
		monster.hitPoints = monster.maxHitPoints;
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

uint16_t RollLesserUniqueNameSeed()
{
	return static_cast<uint16_t>(GenerateRnd(static_cast<int32_t>(NameSeedRange)));
}

void TintLesserUnique(Monster &monster)
{
	// Oracool bug fix (2026-08-15, self-audit): LESSER uniques only. This guard was missing, and it
	// did not matter while the only caller was PlaceLesserUniqueMonst - but the caller added to
	// SyncMonsterAnim an hour earlier runs for EVERY unique on the level, on every load and every
	// level entry. A scripted unique has lesserNameSeed 0, which decodes to a shift of -2, so Gharbad,
	// Zhar, Lazarus, the Butcher and the rest were all quietly being repainted two shades darker.
	//
	// lesserAffix is the marker for "this is a lesser unique" - uniqueType cannot be, because a lesser
	// unique IS a real UniqueMonsterType borrowing a champion's identity.
	if (monster.lesserAffix == LesserUniqueAffix::None)
		return;
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
	// The seed's third digit, so the colour is an independent draw from the name rather than another
	// view of the same low bits - see NameSeedRange.
	const int shift = static_cast<int>((monster.lesserNameSeed / (GivenNameCount * EpithetCount)) % TintChoices) - 2;
	if (shift == 0) // -2..+2, and 0 is a valid outcome
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
	// lesserNameSeed, not aiSeed. The first version of this read aiSeed, reasoning that it is already
	// per-monster and already saved - both true, and both beside the point. multi.cpp's MonsterSeeds
	// rewrites EVERY monster's aiSeed from the game-loop counter on every tick, in single-player too:
	// it is a per-tick nonce that happens to live on the monster, not an identity. The name changed
	// several times a second (user report, 2026-08-15: "his name was constantly changing").
	//
	// Two digits of one mixed-radix number, so the given name and the epithet are independent draws
	// rather than marching in lockstep as the seed increments.
	const uint32_t seed = monster.lesserNameSeed;
	const char *given = GivenNames[seed % GivenNameCount];
	const char *epithet = Epithets[(seed / GivenNameCount) % EpithetCount];
	return StrCat(given, " ", _(epithet));
}

int IsQuestUniqueForTest(const char *mName)
{
	for (size_t i = 0; UniqueMonstersData[i].mtype != -1; i++) {
		if (std::strcmp(UniqueMonstersData[i].mName, mName) == 0)
			return IsQuestUnique(UniqueMonstersData[i]) ? 1 : 0;
	}
	return -1;
}

std::string GetMonsterDisplayName(const Monster &monster)
{
	if (monster.lesserAffix == LesserUniqueAffix::None) {
		// Phase 3: a recoloured variant wears its word in front of the ORDINARY name - "Ashen
		// Skeleton", not a name of its own. That is the whole distinction from a champion: a
		// champion is somebody, a variant is a kind. Making them look alike in the bar would sell
		// a variant as a boss and blunt both.
		if (const char *variant = VariantNamePrefix(VariantOf(monster)); variant != nullptr)
			return StrCat(_(variant), " ", monster.name());
		return std::string(monster.name());
	}
	// A boss wears BOTH its words - "Dread Devouring Ashgore" - because it has two things wrong
	// with it and the name is the only place a player learns the second one before it happens.
	if (IsEndgameBoss(monster)) {
		return StrCat(_(GetLesserUniqueAffixName(monster.lesserAffix)), " ",
		    _(BossTraitName(SecondaryTraitFor(monster))), " ", GetLesserUniqueName(monster));
	}
	return StrCat(_(GetLesserUniqueAffixName(monster.lesserAffix)), " ", GetLesserUniqueName(monster));
}

void OnLesserUniqueKilled(Monster &monster)
{
	if (monster.lesserAffix != LesserUniqueAffix::Thunderous)
		return;
	// Phase 0.3 (rng_streams.h): the burst draws random animation frames from the vanilla LCG deep
	// inside AddMissile - the exact perturbation that once shifted SpawnLoot's seeded drop when this
	// ran in the wrong order. The order fix (loot first) stands; the guard makes the property hold
	// by CONSTRUCTION rather than by call order, so the next reorder cannot reintroduce the bug.
	oracool::MainSeedGuard cosmeticBurst;
	// The discharge is the reward for killing it AND the sting for standing next to it - the corpse
	// is not a safe place to be. Reuses the mini-Nova ring built for Fist of the Heavens at 1.5.78.
	AddMissile(monster.position.tile, monster.position.tile, Direction::South,
	    MissileID::MiniNovaBall, TARGET_PLAYERS, -1, monster.maxDamage, 0);
}

} // namespace devilution::oracool

