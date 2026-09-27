#include "oracool/aura_field.h"

#include <algorithm>

#include <vector>

#include <fmt/format.h>

#include "engine/points_in_rectangle_range.hpp"
#include "engine/random.hpp"
#include "levels/gendung.h"
#include "misdat.h"
#include "missiles.h"
#include "monster.h"
#include "oracool/chill.h"
#include "oracool/class_tree.h"
#include "utils/language.h"
#include "oracool/monster_difficulty.h"
#include "oracool/rfa12_effects.h"
#include "oracool/warcries.h"
#include "player.h"

namespace devilution::oracool {

namespace {

using Skill = ClassTreeSkill;

constexpr int TicksPerSecond = 20;

/** @brief How far Sanctuary drives an undead when it repels it. Matches the Fallen's own flight. */
constexpr int RepelDistance = 4;

// ConvictionBreaksImmunityAt moved to the header - the class-tree tooltip quotes it now.

/**
 * @brief How close a monster must stand to a champion to be part of its pack.
 *
 * Deliberately tighter than a player aura at full investment. A pack should be something the player
 * can break up by pulling monsters away from their leader, which needs the edge of it to be
 * somewhere they can reach.
 */
constexpr int PackAuraRadius = 6;

/** @brief What a Relentless champion lends its pack, in percent of their own damage. */
constexpr int MightPackDamagePercent = 40;

/** @brief What a Fortified champion lends its pack, in armour class. */
constexpr int DefiancePackArmor = 20;

/**
 * @brief The points in @p aura if the local player has it lit and usable, else 0.
 *
 * One place for the three conditions every outward aura shares, so a new consumer cannot forget
 * one: the player must exist and be on this level, the aura must be the lit one, and it must be
 * unlocked and paid for.
 */
int LitAuraPoints(Skill aura)
{
	if (MyPlayer == nullptr || !MyPlayer->isOnActiveLevel())
		return 0;
	const Player &player = *MyPlayer;
	if (GetActiveClassAura(player) != aura || !IsClassTreeSkillUnlocked(player, aura))
		return 0;
	return ClassTreeInvestment(player, aura);
}

/** @brief A roll across @p range, in the 1/64ths monster hit points are kept in. */
int RollDamage(AuraDamage range)
{
	return (range.min + GenerateRnd(std::max(range.max - range.min, 0) + 1)) << 6;
}

/**
 * @brief One monster struck by an aura: nothing if it is immune to @p type, a quarter if it resists (the
 * spell rule), and a cold strike chills it until the next pulse. Kill credit and the hit reaction go to
 * @p player, the way a warcry's do.
 *
 * @p holy is Sanctuary's burn on the undead: no immunity or resistance stops it, Holy Bolt's rule - nearly every
 * undead is immune to magic, so as plain magic it burned none of them (2026-09-27, found with Radiance's).
 */
void AuraStrike(Player &player, Monster &monster, DamageType type, int damage, bool holy = false)
{
	if (damage <= 0 || (monster.hitPoints >> 6) <= 0 || monster.isPlayerMinion() || !monster.isPossibleToHit())
		return;
	if (!holy && monster.isImmune(MissileID::Null, type))
		return;
	if (!holy && monster.isResistant(MissileID::Null, type))
		damage >>= 2;
	if (damage <= 0)
		return;
	ApplyMonsterDamage(type, monster, damage);
	if ((monster.hitPoints >> 6) <= 0) {
		M_StartKill(monster, player);
		return;
	}
	M_StartHit(monster, player, damage);
	if (type == DamageType::Cold)
		ChillMonster(monster, HolyPulseTicks);
	if (type == DamageType::Cold)
		AddColdHitFlash(monster.position.tile, static_cast<int>(player.getId())); // the frost flash (asset audit, 2026-09-26)
}

/**
 * @brief The three holy auras' pulse: once every HolyPulseTicks, everything within reach is struck. The
 * first pulse lands the tick the aura is lit; the floor shockwave (WarcryRing) marks each one.
 */
void ProcessHolyPulse(Player &player)
{
	static int clock = HolyPulseTicks - 1;
	const Skill aura = GetActiveClassAura(player);
	if (aura != Skill::HolyFire && aura != Skill::HolyFreeze && aura != Skill::HolyShock) {
		clock = HolyPulseTicks - 1;
		return;
	}
	const int points = LitAuraPoints(aura);
	if (points <= 0 || ++clock < HolyPulseTicks)
		return;
	clock = 0;

	const DamageType type = aura == Skill::HolyFire ? DamageType::Fire
	    : aura == Skill::HolyFreeze                 ? DamageType::Cold
	                                                : DamageType::Lightning;
	const AuraDamage range = HolyPulseDamage(aura, points);
	const int radius = HolyPulseRadius(points);
	// Gathered first, struck second: a kill mid-scan moves things on the tile map under the loop.
	std::vector<Monster *> struck;
	for (const Point tile : PointsInRectangle(Rectangle { player.position.tile, radius })) {
		if (!InDungeonBounds(tile))
			continue;
		const int id = dMonster[tile.x][tile.y];
		if (id == 0)
			continue;
		Monster &monster = Monsters[std::abs(id) - 1];
		if (monster.position.tile != tile || player.position.tile.WalkingDistance(tile) > radius)
			continue; // the second tile of a monster mid-step, or a corner past the reach
		struck.push_back(&monster);
	}
	for (Monster *monster : struck)
		AuraStrike(player, *monster, type, RollDamage(range));
	AddMissile(player.position.tile, player.position.tile, player._pdir, MissileID::WarcryRing, TARGET_MONSTERS,
	    static_cast<int>(player.getId()), 0, 0);
}

/** @brief The monsters an aura of the local player may strike within @p radius tiles. Gathered before striking. */
std::vector<Monster *> AuraTargetsWithin(const Player &player, int radius)
{
	std::vector<Monster *> found;
	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		Monster &monster = Monsters[ActiveMonsters[i]];
		if ((monster.hitPoints >> 6) <= 0 || monster.isPlayerMinion() || !monster.isPossibleToHit())
			continue;
		if (player.position.tile.WalkingDistance(monster.position.tile) <= radius)
			found.push_back(&monster);
	}
	return found;
}

/**
 * @brief Static Field (Sorcerer, user note 2026-09-14: "like DMG aura of Paladin. Similar to Holy Fire").
 *
 * Holy Fire's pulse and reach, but the strike is a share of what the monster has LEFT, Diablo II's way:
 * 4% at one point, +1% a point, 20% at most; a unique loses half. At least one point of damage.
 */
void ProcessStaticField(Player &player)
{
	static int clock = HolyPulseTicks - 1;
	const int points = LitAuraPoints(Skill::StaticField);
	if (points <= 0) {
		clock = HolyPulseTicks - 1;
		return;
	}
	if (++clock < HolyPulseTicks)
		return;
	clock = 0;
	const int percent = StaticFieldPercent(points);
	for (Monster *monster : AuraTargetsWithin(player, AuraFieldRadius(Skill::StaticField, points))) {
		int damage = monster->hitPoints * percent / 100;
		if (monster->isUnique())
			damage /= 2;
		AuraStrike(player, *monster, DamageType::Lightning, std::max(damage, 1 << 6));
	}
	AddMissile(player.position.tile, player.position.tile, player._pdir, MissileID::WarcryRing, TARGET_MONSTERS,
	    static_cast<int>(player.getId()), 0, 0);
}

/** @brief Thunder Storm (Sorcerer, 2026-09-14): every HolyPulseTicks a bolt falls on one enemy within 6 tiles. */
void ProcessThunderStorm(Player &player)
{
	static int clock = 0;
	const int points = LitAuraPoints(Skill::ThunderStorm);
	if (points <= 0) {
		clock = 0;
		return;
	}
	if (++clock < HolyPulseTicks)
		return;
	clock = 0;
	const std::vector<Monster *> near = AuraTargetsWithin(player, AuraFieldRadius(Skill::ThunderStorm, points));
	if (near.empty())
		return;
	Monster &target = *near[GenerateRnd(static_cast<int32_t>(near.size()))];
	const Point tile = target.position.tile;
	AuraStrike(player, target, DamageType::Lightning, RollDamage(ThunderStormDamage(points)));
	// RfA-16's bolt falls where it struck; without its sheet the floor shockwave marks the spot instead.
	const MissileID mark = MissileArtLoaded(MissileGraphicID::ThunderBolt) ? MissileID::ThunderBolt : MissileID::WarcryRing;
	AddMissile(tile, tile, player._pdir, mark, TARGET_MONSTERS, static_cast<int>(player.getId()), 0, 0);
}

/** @brief Whether @p monster is close enough to the local player for an aura of @p points. */
bool WithinAura(const Monster &monster, int points)
{
	return monster.position.tile.WalkingDistance(MyPlayer->position.tile) <= AuraRadiusForPoints(points);
}

} // namespace

int AuraRadiusForPoints(int points)
{
	if (points <= 0)
		return 0;
	// Four tiles at one point, one more per two points, capped at eight. Eight is about the point
	// where the field covers everything already on screen, past which positioning stops mattering.
	return std::min(4 + (points - 1) / 2, 8);
}

int HolyPulseRadius(int points)
{
	return points <= 0 ? 0 : std::min(3 + points, 10);
}

AuraDamage HolyPulseDamage(Skill aura, int points)
{
	const int p = std::max(points, 1) - 1;
	switch (aura) {
	case Skill::HolyFire:
		return { 4 + 3 * p, 8 + 6 * p };
	case Skill::HolyFreeze:
		// A little under the fire, because every hit also chills.
		return { 3 + 2 * p, 6 + 5 * p };
	case Skill::HolyShock:
		// Lightning's wide spread, as every lightning in the game has it.
		return { 1 + p, 14 + 8 * p };
	default:
		return { 0, 0 };
	}
}

AuraDamage SanctuaryDamage(int points)
{
	const int p = std::max(points, 1) - 1;
	return { 4 + 2 * p, 8 + 4 * p };
}

int ConvictionArmorCutPercent(int points)
{
	return points <= 0 ? 0 : std::min(3 * points, 60);
}

int StaticFieldPercent(int points)
{
	return std::min(4 + (std::max(points, 1) - 1), 20);
}

AuraDamage ThunderStormDamage(int points)
{
	return { 1, 20 + 10 * (std::max(points, 1) - 1) };
}

int AuraTricklePerTick(int points)
{
	return points <= 0 ? 0 : 2 + 2 * (points - 1);
}

bool AuraReachesMonsters(Skill aura)
{
	switch (aura) {
	case Skill::HolyFire:
	case Skill::HolyFreeze:
	case Skill::HolyShock:
	case Skill::StaticField:
	case Skill::ThunderStorm:
	case Skill::Sanctuary:
	case Skill::Conviction:
	case Skill::Redemption:
	case Skill::DirgeOfDread:
	case Skill::Discord:
	case Skill::Weaken:
		return true;
	default:
		return Rfa12AuraReachesMonsters(aura);
	}
}

int AuraFieldRadius(Skill aura, int points)
{
	// Static Field has Holy Fire's pulse and reach; Thunder Storm looks the same distance at every level.
	if (aura == Skill::HolyFire || aura == Skill::HolyFreeze || aura == Skill::HolyShock || aura == Skill::StaticField)
		return HolyPulseRadius(points);
	if (aura == Skill::ThunderStorm)
		return points <= 0 ? 0 : ThunderStormReach;
	return AuraRadiusForPoints(points);
}

std::string AuraFieldFactsAt(Skill aura, int points)
{
	const int p = std::max(points, 1);
	switch (aura) {
	case Skill::HolyFire: {
		const AuraDamage d = HolyPulseDamage(aura, p);
		return fmt::format(fmt::runtime(_("Fire damage: {:d} - {:d} every 3 seconds to everything in reach")), d.min, d.max);
	}
	case Skill::HolyFreeze: {
		const AuraDamage d = HolyPulseDamage(aura, p);
		return fmt::format(fmt::runtime(_("Cold damage: {:d} - {:d} every 3 seconds, chilling everything in reach")), d.min, d.max);
	}
	case Skill::HolyShock: {
		const AuraDamage d = HolyPulseDamage(aura, p);
		return fmt::format(fmt::runtime(_("Lightning damage: {:d} - {:d} every 3 seconds to everything in reach")), d.min, d.max);
	}
	case Skill::Sanctuary: {
		const AuraDamage d = SanctuaryDamage(p);
		return fmt::format(fmt::runtime(_("Undead in reach flee and take {:d} - {:d} holy damage a second, through magic immunity")), d.min, d.max);
	}
	case Skill::Conviction:
		return fmt::format(fmt::runtime(_("Enemy armour: -{:d}%")), ConvictionArmorCutPercent(p));
	case Skill::Thorns:
		return fmt::format(fmt::runtime(_("Returns {:d}% of melee damage taken")), ThornsReturnPercentAt(p));
	case Skill::Cleansing:
		return fmt::format(fmt::runtime(_("Slows and chills on you wear off {:d}% sooner")), CleansingShortenPercentAt(p));
	case Skill::StaticField:
		return fmt::format(fmt::runtime(_("Every {:d} seconds everything in reach loses {:d}% of its remaining life as lightning (uniques half)")),
		    HolyPulseTicks / TicksPerSecond, StaticFieldPercent(p));
	case Skill::ThunderStorm: {
		const AuraDamage d = ThunderStormDamage(p);
		return fmt::format(fmt::runtime(_("Every {:d} seconds a bolt strikes one enemy in reach for {:d} - {:d} lightning damage")),
		    HolyPulseTicks / TicksPerSecond, d.min, d.max);
	}
	case Skill::Redemption:
		return fmt::format(fmt::runtime(_("Once a second the nearest corpse in reach is consumed for {:d}% of your life and mana")),
		    RedemptionSharePercent(p));
	case Skill::HealingMantra:
		return fmt::format(fmt::runtime(_("Life regeneration: {:.1f} a second")),
		    AuraTricklePerTick(p) * TicksPerSecond / 64.0);
	default:
		return Rfa12AuraFactsAt(aura, points);
	}
}

int ConvictionPointsOn(const Monster &monster)
{
	const int points = LitAuraPoints(Skill::Conviction);
	if (points <= 0)
		return 0;
	return WithinAura(monster, points) ? points : 0;
}

int AuraPointsOn(const Monster &monster, Skill aura)
{
	const int points = LitAuraPoints(aura);
	if (points <= 0)
		return 0;
	return WithinAura(monster, points) ? points : 0;
}

uint16_t ConvictionAdjusted(uint16_t resistances, int points)
{
	if (points <= 0)
		return resistances;
	// Strip the plain resistances FIRST, then step immunities down into plain resistances. Doing it
	// in this order is what keeps a deep Conviction from deleting an immunity outright: the newly
	// demoted bits arrive after the stripping has already happened, so the monster ends up
	// resistant rather than bare.
	uint16_t out = resistances & ~static_cast<uint16_t>(RESIST_MAGIC | RESIST_FIRE | RESIST_LIGHTNING);
	if (points >= ConvictionBreaksImmunityAt)
		out = DemoteImmunitiesToResistances(out);
	return out;
}

uint16_t EffectiveResistances(const Monster &monster)
{
	uint16_t resistances = ConvictionAdjusted(monster.resistance, ConvictionPointsOn(monster));
	// Satire (RfA-12) mocks the plain resistances away; immunities hold.
	if (Rfa12StripsResistances(monster))
		resistances &= ~static_cast<uint16_t>(RESIST_MAGIC | RESIST_FIRE | RESIST_LIGHTNING);
	return resistances;
}

void ProcessOutwardAura(Player &player)
{
	if (&player != MyPlayer || player._pHitPoints <= 0)
		return;

	ProcessHolyPulse(player);
	ProcessStaticField(player);
	ProcessThunderStorm(player);

	const int sanctuary = LitAuraPoints(Skill::Sanctuary);
	if (sanctuary <= 0)
		return;
	// Once a second the hallowed ground burns the undead standing on it (2026-09-12) - champions too:
	// they are too proud to run, not too proud to burn.
	static int sanctuaryClock = 0;
	const bool burn = ++sanctuaryClock % TicksPerSecond == 0;

	// Repulsion has to PUSH, so unlike Conviction it cannot be a question asked at the point of
	// use - there is no such point. It rides MonsterGoal::Retreat, which is the same channel
	// M_FallenFear has always used, and which the monster AI clears by itself once the retreat
	// finishes. Re-set every tick while the undead is still in the field, so walking away ends it
	// without anything having to be undone.
	const int radius = AuraRadiusForPoints(sanctuary);
	const Rectangle field { player.position.tile, radius };
	for (const Point tile : PointsInRectangle(field)) {
		if (!InDungeonBounds(tile))
			continue;
		const int id = dMonster[tile.x][tile.y];
		if (id == 0)
			continue;
		Monster &monster = Monsters[std::abs(id) - 1];
		if ((monster.hitPoints >> 6) <= 0)
			continue;
		// Undead only - that is what a sanctuary is for, and it is the same test HolyBolt already
		// uses to decide what it may burn.
		if (monster.data().monsterClass != MonsterClass::Undead)
			continue;
		if (burn && monster.position.tile == tile) {
			AuraStrike(player, monster, DamageType::Magic, RollDamage(SanctuaryDamage(sanctuary)), /*holy=*/true);
			if ((monster.hitPoints >> 6) <= 0)
				continue;
		}
		// A champion is frightened by nothing. Letting an aura walk a unique out of the room would
		// make the fight the player came for un-fightable.
		if (monster.isUnique())
			continue;
		monster.goal = MonsterGoal::Retreat;
		monster.goalVar1 = RepelDistance;
		monster.goalVar2 = static_cast<int>(GetDirection(player.position.tile, monster.position.tile));
	}
}

PackAuraBonus PackAuraFrom(LesserUniqueAffix affix, int distance)
{
	PackAuraBonus bonus {};
	if (distance > PackAuraRadius)
		return bonus;
	switch (affix) {
	case LesserUniqueAffix::Relentless:
		// Might. The champion that will not be knocked back drives its pack forward with it.
		bonus.damagePercent = MightPackDamagePercent;
		break;
	case LesserUniqueAffix::Fortified:
		// Defiance. The armoured one shelters what stands beside it.
		bonus.armorBonus = DefiancePackArmor;
		break;
	default:
		// Warded, Vampiric, Thunderous and Colossal are personal - a resistance, a life-steal, a
		// death burst and a size. Nothing there is lendable, and leaving them personal is what
		// keeps the six affixes from blurring into one another.
		break;
	}
	return bonus;
}

PackAuraBonus PackAuraOn(const Monster &monster)
{
	PackAuraBonus bonus {};
	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		const Monster &champion = Monsters[ActiveMonsters[i]];
		// A monster only takes from ANOTHER monster's presence, and a corpse leads nobody.
		if (&champion == &monster || (champion.hitPoints >> 6) <= 0)
			continue;
		const PackAuraBonus lent = PackAuraFrom(champion.lesserAffix,
		    champion.position.tile.WalkingDistance(monster.position.tile));
		// The strongest of each kind rather than the sum, so two champions in one room do not
		// multiply into something the floor was never balanced for.
		bonus.damagePercent = std::max(bonus.damagePercent, lent.damagePercent);
		bonus.armorBonus = std::max(bonus.armorBonus, lent.armorBonus);
	}
	return bonus;
}

uint8_t RaiseDamageByPercent(uint8_t base, int percent)
{
	if (percent <= 0)
		return base;
	// Clamped rather than wrapped, the same care the Torment difficulty block already takes with
	// these uint8_t fields: a wrap would make a stronger pack unpredictably WEAKER.
	return static_cast<uint8_t>(std::min(base + base * percent / 100, 255));
}

uint8_t PackAdjustedDamage(const Monster &monster, uint8_t base)
{
	return RaiseDamageByPercent(base, PackAuraOn(monster).damagePercent);
}

const char *PackAuraName(const Monster &monster)
{
	const PackAuraBonus bonus = PackAuraOn(monster);
	// Named for what Diablo II called them, because that is what the effect IS - the point of
	// showing it is that a player who knows the word knows what it does.
	if (bonus.damagePercent > 0)
		return N_("Might");
	if (bonus.armorBonus > 0)
		return N_("Defiance");
	return "";
}

int PackAdjustedArmor(const Monster &monster)
{
	return monster.armorClass + PackAuraOn(monster).armorBonus;
}

} // namespace devilution::oracool
