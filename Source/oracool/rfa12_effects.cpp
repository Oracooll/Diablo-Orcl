#include "oracool/rfa12_effects.h"

#include <algorithm>
#include <array>
#include <cstdlib>

#include <fmt/format.h>

#include "engine/backbuffer_state.hpp"
#include "engine/random.hpp"
#include "itemdat.h"
#include "levels/gendung.h"
#include "lighting.h"
#include "missiles.h"
#include "monster.h"
#include "oracool/aura_field.h"
#include "oracool/chill.h"
#include "oracool/curses.h"
#include "oracool/rfa12_actives.h"
#include "oracool/whirlwind.h"
#include "player.h"
#include "utils/language.h"

namespace devilution::oracool {

namespace {

using Skill = ClassTreeSkill;

constexpr int TicksPerSecond = 20;

// ---- per player --------------------------------------------------------------------------------

/** A tile Doom Procession has hallowed behind the Paladin, and how long it holds. */
struct Wake {
	Point tile;
	int ticksLeft = 0;
};

constexpr size_t MaxWakes = 8;

struct PlayerClocks {
	int retaliationStacks = 0; // Retaliation: blows taken since the last blow landed, to three
	int mercyCooldown = 0;     // Mercy: ticks until it may answer again
	int walkTicks = 0;         // Doom Procession: consecutive ticks walking
	Point lastTile;            // ...and the tile stood on last tick
	int quietTicks = 0;        // Soft Tread: ticks since the last attack or cast
	int sovereignCount = 0;    // Sovereign Measure: blows on the lone enemy since the last impact
	int pulseClock = 0;        // Radiance and Siren's Call
	std::array<Wake, MaxWakes> wakes {};
};

std::array<PlayerClocks, MAX_PLRS> PlayerState;

PlayerClocks &ClocksOf(const Player &player)
{
	return PlayerState[player.getId()];
}

// ---- per monster slot --------------------------------------------------------------------------

struct MonsterMarks {
	int bleedTicks = 0;   // Deep Wounds: ticks of bleeding left
	int bleedDamage = 0;  // ...and what each second of it takes, in 1/64 units
	int noRegenTicks = 0; // Lasting Wounds
	bool wounded = false; // Scent of Blood: the Rogue has drawn blood from it
	int scentTicks = 0;   // ...and ticks it stays drawn out of the light
	int struckTicks = 0;  // Unfinished Business: ticks since it last struck the player
	Point lastTile;       // Dead Ground: where it stood last tick
	int stillTicks = 0;   // ...and how long it has stood there
	int deadGroundCooldown = 0;
	bool tithe = false;   // Tithe of Ash: its corpse is taken
};

std::array<MonsterMarks, MaxMonsters> MonsterState;

MonsterMarks &MarksOf(const Monster &monster)
{
	return MonsterState[monster.getId()];
}

// ---- small helpers -----------------------------------------------------------------------------

/**
 * @brief The points in @p skill if it is ON for @p player, else 0: built, this class's, unlocked and
 * invested - and for an aura, the lit one.
 */
int PointsIfOn(const Player &player, Skill skill)
{
	const ClassTreeSkillData &data = GetClassTreeSkillData(skill);
	if (!data.implemented || data.heroClass != player._pClass || !IsClassTreeSkillUnlocked(player, skill))
		return 0;
	const int points = ClassTreeRank(player, skill); // with Battle Command's rank (2026-09-29)
	if (points <= 0)
		return 0;
	if (data.kind == ClassTreeKind::Aura && GetActiveClassAura(player) != skill)
		return 0;
	return points;
}

/** @brief The local player's points in @p aura if @p monster stands inside it, else 0. */
int AuraPointsReaching(const Monster &monster, Skill aura)
{
	if (MyPlayer == nullptr || !MyPlayer->isOnActiveLevel())
		return 0;
	const int points = PointsIfOn(*MyPlayer, aura);
	if (points <= 0)
		return 0;
	return monster.position.tile.WalkingDistance(MyPlayer->position.tile) <= AuraRadiusForPoints(points) ? points : 0;
}

bool Walking(const Player &player)
{
	return IsAnyOf(player._pmode, PM_WALK_NORTHWARDS, PM_WALK_SOUTHWARDS, PM_WALK_SIDEWAYS);
}

bool HoldsType(const Player &player, ItemType type)
{
	for (const Item &item : { player.InvBody[INVLOC_HAND_LEFT], player.InvBody[INVLOC_HAND_RIGHT] }) {
		if (!item.isEmpty() && item._iStatFlag && item._itype == type)
			return true;
	}
	return false;
}

bool HoldsSpearOrPike(const Player &player)
{
	for (const Item &item : { player.InvBody[INVLOC_HAND_LEFT], player.InvBody[INVLOC_HAND_RIGHT] }) {
		if (item.isEmpty() || !item._iStatFlag || item.IDidx < 0 || item.IDidx > IDI_LAST)
			continue;
		const unique_base_item base = AllItemsList[static_cast<size_t>(item.IDidx)].iItemId;
		if (base == UITYPE_SPEAR || base == UITYPE_PIKE)
			return true;
	}
	return false;
}

bool HoldsTwoHandedMelee(const Player &player)
{
	for (const Item &item : { player.InvBody[INVLOC_HAND_LEFT], player.InvBody[INVLOC_HAND_RIGHT] }) {
		if (!item.isEmpty() && item._iStatFlag && item._iClass == ICLASS_WEAPON && item._iLoc == ILOC_TWOHAND
		    && item._itype != ItemType::Bow)
			return true;
	}
	return false;
}

/** @brief Live, hittable monsters within @p range tiles of @p centre. */
int MonstersWithin(Point centre, int range)
{
	int count = 0;
	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		const Monster &other = Monsters[ActiveMonsters[i]];
		if (!other.isPossibleToHit() || other.hitPoints >> 6 <= 0 || other.isPlayerMinion())
			continue;
		if (centre.WalkingDistance(other.position.tile) <= range)
			count++;
	}
	return count;
}

int Roll(int min, int max)
{
	return (min + GenerateRnd(std::max(max - min, 0) + 1)) << 6;
}

void Heal(Player &player, int amount)
{
	if (amount <= 0 || player._pHitPoints <= 0 || player._pHitPoints >= player._pMaxHP)
		return;
	player._pHitPoints = std::min(player._pHitPoints + amount, player._pMaxHP);
	player._pHPBase = std::min(player._pHPBase + amount, player._pMaxHPBase);
	RedrawComponent(PanelDrawComponent::Health);
}

void RestoreMana(Player &player, int amount)
{
	if (amount <= 0 || player._pMana >= player._pMaxMana || HasAnyOf(player._pIFlags, ItemSpecialEffect::NoMana))
		return;
	player._pMana = std::min(player._pMana + amount, player._pMaxMana);
	player._pManaBase = std::min(player._pManaBase + amount, player._pMaxManaBase);
	RedrawComponent(PanelDrawComponent::Mana);
}

/** @brief A skill's own strike: immunity and resistance honoured, kill credit to @p player. */
void Strike(Player &player, Monster &monster, DamageType type, int damage)
{
	if (damage <= 0 || (monster.hitPoints >> 6) <= 0 || monster.isPlayerMinion() || !monster.isPossibleToHit())
		return;
	if (monster.isImmune(MissileID::Null, type))
		return;
	if (monster.isResistant(MissileID::Null, type))
		damage >>= 2;
	if (damage <= 0)
		return;
	ApplyMonsterDamage(type, monster, damage);
	if ((monster.hitPoints >> 6) <= 0)
		M_StartKill(monster, player);
	else
		M_StartHit(monster, player, damage);
}

/**
 * @brief Holy light on an undead: no immunity or resistance stops it, the rule Holy Bolt has always had. Nearly
 * every undead in the game - zombies, skeletons - is immune to magic, so Radiance struck as plain magic hurt none of
 * them (dev note, 2026-09-27: "radiance aura doesnt seem to hurt undead").
 */
void StrikeHoly(Player &player, Monster &monster, int damage)
{
	if (damage <= 0 || (monster.hitPoints >> 6) <= 0 || monster.isPlayerMinion() || !monster.isPossibleToHit())
		return;
	ApplyMonsterDamage(DamageType::Magic, monster, damage);
	if ((monster.hitPoints >> 6) <= 0)
		M_StartKill(monster, player);
	else
		M_StartHit(monster, player, damage);
}

// ---- the numbers, one place each, so the rule and its tooltip cannot disagree ------------------

int BaneOfEvilPercent(int p) { return std::min(25 + 5 * (p - 1), 150); }
int ConvictionLikeCut(int p) { return std::min(10 + 2 * (p - 1), 50); }
int DominionPercent(int p) { return std::min(10 + (p - 1), 35); }
int RetaliationPerStack(int p) { return 10 + 3 * (p - 1); }
int MercyHealPercent(int p) { return std::min(20 + (p - 1), 50); }
int TitheMana(int p) { return 2 + (p - 1); }
int BattleHardenedPercent(int p) { return std::min(10 + 2 * (p - 1), 40); }
int DeepWoundsChance(int p) { return std::min(10 + 2 * (p - 1), 50); }
int DeepWoundsPerSecond(int p) { return 2 + p; }
int BloodlustPercent(int p) { return 2 + (p - 1) / 5; }
int UnfinishedBusinessPercent(int p) { return 3 + (p - 1) / 3; }
int DeadGroundPercent(int p) { return 25 + 2 * (p - 1); }
int DeadeyePercent(int p) { return 50 + 5 * (p - 1); }
int BlockPercent(int p) { return std::min(5 + (p - 1), 30); }
int DeepBreathPerTick(int p) { return 2 + (p - 1); } // life, 1/64 units
AuraDamage RadianceDamage(int p) { return { 3 + (p - 1), 6 + 2 * (p - 1) }; }
AuraDamage WakeDamage(int p) { return { 4 + 2 * (p - 1), 8 + 3 * (p - 1) }; }
AuraDamage SovereignDamage(int p) { return { 6 + 2 * (p - 1), 10 + 3 * (p - 1) }; }

constexpr int RadiancePulseTicks = 2 * TicksPerSecond;
constexpr int SirenPulseTicks = TicksPerSecond;
constexpr int MercyCooldownTicks = 20 * TicksPerSecond;
constexpr int LastingWoundsTicks = 4 * TicksPerSecond;
constexpr int DeepWoundsTicks = 3 * TicksPerSecond;
constexpr int ScentTicks = 2 * TicksPerSecond;
constexpr int StruckMemoryTicks = 5 * TicksPerSecond;
constexpr int DeadGroundStillTicks = 2 * TicksPerSecond;
constexpr int DeadGroundCooldownTicks = 6 * TicksPerSecond;
constexpr int ProcessionArmTicks = 2 * TicksPerSecond;
constexpr int WakeTicks = 3 * TicksPerSecond;
constexpr int SoftTreadQuietTicks = 3 * TicksPerSecond;
constexpr int DeadeyeChancePercent = 10;
constexpr int MercyBelowLifePercent = 30;
constexpr int SanctityShortenPercent = 50;
constexpr int RetaliationMaxStacks = 3;

/** @brief A per-tick trickle in 1/64 units, as the tooltip reads it: points a second, one decimal. */
double PerSecond(int perTick)
{
	return perTick * TicksPerSecond / 64.0;
}

bool DeadGroundApplies(const Player &player, const Monster &target)
{
	const MonsterMarks &marks = MarksOf(target);
	return PointsIfOn(player, Skill::DeadGround) > 0 && marks.stillTicks >= DeadGroundStillTicks && marks.deadGroundCooldown == 0;
}

} // namespace

int Rfa12DamageDealtPercent(const Player &player, const Monster &target, bool melee)
{
	int percent = 0;
	const MonsterClass monsterClass = target.data().monsterClass;
	if (const int p = PointsIfOn(player, Skill::BaneOfEvil); p > 0 && (monsterClass == MonsterClass::Demon || monsterClass == MonsterClass::Undead))
		percent += BaneOfEvilPercent(p);
	if (&player == MyPlayer) {
		if (const int p = AuraPointsReaching(target, Skill::Dominion); p > 0)
			percent += DominionPercent(p);
	}
	if (melee) {
		if (const int p = PointsIfOn(player, Skill::Retaliation); p > 0)
			percent += ClocksOf(player).retaliationStacks * RetaliationPerStack(p);
	} else {
		if (DeadGroundApplies(player, target))
			percent += DeadGroundPercent(PointsIfOn(player, Skill::DeadGround));
		if (const int p = PointsIfOn(player, Skill::Deadeye); p > 0 && GenerateRnd(100) < DeadeyeChancePercent)
			percent += DeadeyePercent(p);
	}
	return percent + Rfa12ActiveDamageDealtPercent(player, target, melee);
}

int Rfa12MonsterDamagePercent(const Monster &monster)
{
	const int p = AuraPointsReaching(monster, Skill::Dominion);
	return p > 0 ? -DominionPercent(p) : 0;
}

int Rfa12MonsterArmorCutPercent(const Monster &monster)
{
	const int p = AuraPointsReaching(monster, Skill::Condemnation);
	return p > 0 ? ConvictionLikeCut(p) : 0;
}

int Rfa12DamageTakenPercent(const Player &player, DamageType type)
{
	if (type == DamageType::Physical || player._pHitPoints * 2 >= player._pMaxHP)
		return 0;
	const int p = PointsIfOn(player, Skill::BattleHardened);
	return p > 0 ? -BattleHardenedPercent(p) : 0;
}

int Rfa12BlockBonus(const Player &player)
{
	int bonus = 0;
	if (const int p = PointsIfOn(player, Skill::StaffParry); p > 0 && HoldsType(player, ItemType::Staff))
		bonus += BlockPercent(p);
	if (const int p = PointsIfOn(player, Skill::Brace); p > 0 && HoldsSpearOrPike(player))
		bonus += BlockPercent(p);
	return bonus;
}

bool Rfa12GrantsBlock(const Player &player)
{
	return PointsIfOn(player, Skill::Brace) > 0 && HoldsSpearOrPike(player);
}

bool PlayerIgnoresKnockback(const Player &player)
{
	if (PointsIfOn(player, Skill::Immovable) > 0)
		return true;
	return PointsIfOn(player, Skill::HeavyFoot) > 0 && HoldsTwoHandedMelee(player);
}

bool PlayerHoldsAgainstHit(const Player &player)
{
	if (PointsIfOn(player, Skill::AnthemOfValor) > 0)
		return true;
	return PointsIfOn(player, Skill::GripOfIron) > 0 && player._pmode == PM_ATTACK
	    && MonstersWithin(player.position.tile, 1) == 1;
}

bool MonsterRegenBlocked(const Monster &monster)
{
	const MonsterMarks &marks = MarksOf(monster);
	return marks.noRegenTicks > 0 || marks.bleedTicks > 0;
}

bool MonsterMayNotice(const Monster &monster)
{
	if (MyPlayer == nullptr || !MyPlayer->isOnActiveLevel())
		return true;
	const Player &player = *MyPlayer;
	if (Rfa12ActiveHidesPlayer(player))
		return false; // Astral Projection
	const int distance = monster.position.tile.WalkingDistance(player.position.tile);
	if (CursedMonsterBlinded(monster))
		return distance <= DimVisionSightTiles; // Dim Vision (oracool/curses.h): only what stands beside it
	if (PointsIfOn(player, Skill::Nocturne) > 0)
		return distance <= std::max(2, player._pLightRad / 2);
	if (PointsIfOn(player, Skill::SoftTread) > 0 && Walking(player) && ClocksOf(player).quietTicks >= SoftTreadQuietTicks)
		return distance <= std::max(2, player._pLightRad * 2 / 3);
	return true;
}

bool MonsterScented(const Monster &monster)
{
	return MarksOf(monster).scentTicks > 0;
}

Monster *Rfa12ReachTarget(const Player &player, Point swingTile)
{
	if (PointsIfOn(player, Skill::LongReach) <= 0 || !(HoldsType(player, ItemType::Staff) || HoldsSpearOrPike(player)))
		return nullptr;
	if (!InDungeonBounds(swingTile) || dMonster[swingTile.x][swingTile.y] != 0 || dPlayer[swingTile.x][swingTile.y] != 0
	    || IsTileSolid(swingTile))
		return nullptr;
	const Point beyond = swingTile + player._pdir;
	if (!InDungeonBounds(beyond))
		return nullptr;
	Monster *monster = FindMonsterAtPosition(beyond);
	if (monster == nullptr || monster->isPlayerMinion() || !monster->isPossibleToHit())
		return nullptr;
	return monster;
}

bool TitheTakesCorpse(const Monster &monster)
{
	MonsterMarks &marks = MarksOf(monster);
	const bool taken = marks.tithe;
	marks.tithe = false;
	return taken;
}

void OnRfa12Hit(Player &player, Monster &monster, int damage, bool melee)
{
	MonsterMarks &marks = MarksOf(monster);
	PlayerClocks &clocks = ClocksOf(player);
	if (damage > 0 && PointsIfOn(player, Skill::ScentOfBlood) > 0)
		marks.wounded = true;
	if (melee) {
		clocks.retaliationStacks = 0;
		if (const int p = PointsIfOn(player, Skill::DeepWounds); p > 0 && GenerateRnd(100) < DeepWoundsChance(p)) {
			marks.bleedTicks = DeepWoundsTicks;
			marks.bleedDamage = std::max(marks.bleedDamage, DeepWoundsPerSecond(p) << 6);
		}
		if (PointsIfOn(player, Skill::LastingWounds) > 0)
			marks.noRegenTicks = LastingWoundsTicks;
		if (const int p = PointsIfOn(player, Skill::Bloodlust); p > 0 && damage > 0)
			Heal(player, damage * BloodlustPercent(p) / 100);
	} else if (DeadGroundApplies(player, monster)) {
		marks.deadGroundCooldown = DeadGroundCooldownTicks;
	}
	OnRfa12ActiveHit(player, monster, damage, melee);
	if (const int p = PointsIfOn(player, Skill::SovereignMeasure); p > 0 && (monster.hitPoints >> 6) > 0
	    && MonstersWithin(player.position.tile, 4) == 1) {
		if (++clocks.sovereignCount >= 4) {
			clocks.sovereignCount = 0;
			const AuraDamage d = SovereignDamage(p);
			Strike(player, monster, DamageType::Magic, Roll(d.min, d.max));
		}
	}
}

void OnRfa12Struck(Player &player, Monster &monster)
{
	OnRfa12ActiveStruck(player, monster);
	if (PointsIfOn(player, Skill::Retaliation) > 0) {
		PlayerClocks &clocks = ClocksOf(player);
		clocks.retaliationStacks = std::min(clocks.retaliationStacks + 1, RetaliationMaxStacks);
	}
	MarksOf(monster).struckTicks = StruckMemoryTicks;
}

void OnRfa12PlayerDamaged(Player &player)
{
	PlayerClocks &clocks = ClocksOf(player);
	const int p = PointsIfOn(player, Skill::Mercy);
	if (p <= 0 || clocks.mercyCooldown > 0 || player._pHitPoints >> 6 <= 0 || player._pHitPoints * 100 >= player._pMaxHP * MercyBelowLifePercent)
		return;
	Heal(player, player._pMaxHP * MercyHealPercent(p) / 100);
	clocks.mercyCooldown = MercyCooldownTicks;
}

void OnRfa12MonsterKilled(Player &player, const Monster &monster)
{
	OnRfa12ActiveMonsterKilled(player, monster);
	MonsterMarks &marks = MarksOf(monster);
	if (const int p = AuraPointsReaching(monster, Skill::TitheOfAsh); p > 0 && &player == MyPlayer && !monster.isPlayerMinion()) {
		RestoreMana(player, TitheMana(p) << 6);
		marks.tithe = true;
	}
	if (const int p = PointsIfOn(player, Skill::UnfinishedBusiness); p > 0 && marks.struckTicks > 0)
		Heal(player, player._pMaxHP * UnfinishedBusinessPercent(p) / 100);
	marks.bleedTicks = 0;
	marks.noRegenTicks = 0;
	marks.wounded = false;
	marks.scentTicks = 0;
}

int Rfa12SlowShortenPercent(const Player &player)
{
	return PointsIfOn(player, Skill::Sanctity) > 0 ? SanctityShortenPercent : 0;
}

void ProcessRfa12Tick(Player &player)
{
	ProcessRfa12ActivesTick(player);
	if (&player != MyPlayer || player._pHitPoints <= 0 || player._pmode == PM_DEATH)
		return;
	PlayerClocks &clocks = ClocksOf(player);
	if (clocks.mercyCooldown > 0)
		clocks.mercyCooldown--;
	const bool acting = IsAnyOf(player._pmode, PM_ATTACK, PM_RATTACK, PM_SPELL);
	clocks.quietTicks = acting ? 0 : std::min(clocks.quietTicks + 1, 1 << 20);

	// The songs and breath that mend: a trickle per tick in 1/64 units, like Prayer and Meditation.
	if (const int p = PointsIfOn(player, Skill::MinstrelsTune); p > 0)
		RestoreMana(player, 2 + 2 * (p - 1));
	if (const int p = PointsIfOn(player, Skill::HymnOfRenewal); p > 0) {
		Heal(player, 1 + (p - 1));
		RestoreMana(player, 1 + (p - 1));
	}
	if (const int p = PointsIfOn(player, Skill::DeepBreath); p > 0)
		Heal(player, DeepBreathPerTick(p));

	const bool pulse = ++clocks.pulseClock % RadiancePulseTicks == 0;
	const bool siren = clocks.pulseClock % SirenPulseTicks == 0;
	const int radiance = PointsIfOn(player, Skill::Radiance);
	const int sirens = PointsIfOn(player, Skill::SirensCall);
	if ((radiance > 0 && pulse) || (sirens > 0 && siren)) {
		const int radius = AuraRadiusForPoints(std::max(radiance, sirens));
		for (size_t i = 0; i < ActiveMonsterCount; i++) {
			Monster &monster = Monsters[ActiveMonsters[i]];
			if ((monster.hitPoints >> 6) <= 0 || monster.isPlayerMinion())
				continue;
			const int distance = monster.position.tile.WalkingDistance(player.position.tile);
			if (radiance > 0 && pulse && distance <= AuraRadiusForPoints(radiance)
			    && monster.data().monsterClass == MonsterClass::Undead) {
				const AuraDamage d = RadianceDamage(radiance);
				StrikeHoly(player, monster, Roll(d.min, d.max));
			}
			if (sirens > 0 && siren && distance <= radius && (monster.hitPoints >> 6) > 0 && monster.mode != MonsterMode::Petrified) {
				monster.activeForTicks = UINT8_MAX;
				monster.enemy = static_cast<uint8_t>(player.getId());
				monster.enemyPosition = player.position.tile;
				monster.flags &= ~MFLAG_TARGETS_MONSTER;
				monster.goal = MonsterGoal::Normal;
				ChillMonster(monster, SirenPulseTicks + TicksPerSecond / 2);
			}
		}
	}

	// Doom Procession: two seconds on the move arms it, and every tile left behind after that burns for
	// a while, striking the first enemy that steps onto it.
	const int procession = PointsIfOn(player, Skill::DoomProcession);
	if (procession <= 0) {
		clocks.walkTicks = 0;
		clocks.wakes.fill(Wake {});
	} else {
		clocks.walkTicks = Walking(player) ? clocks.walkTicks + 1 : 0;
		if (clocks.walkTicks >= ProcessionArmTicks && player.position.tile != clocks.lastTile && InDungeonBounds(clocks.lastTile)) {
			auto oldest = std::min_element(clocks.wakes.begin(), clocks.wakes.end(),
			    [](const Wake &a, const Wake &b) { return a.ticksLeft < b.ticksLeft; });
			*oldest = { clocks.lastTile, WakeTicks };
		}
		for (Wake &wake : clocks.wakes) {
			if (wake.ticksLeft <= 0)
				continue;
			wake.ticksLeft--;
			const int id = dMonster[wake.tile.x][wake.tile.y];
			if (id == 0)
				continue;
			Monster &monster = Monsters[std::abs(id) - 1];
			if (monster.position.tile != wake.tile || monster.isPlayerMinion())
				continue;
			const AuraDamage d = WakeDamage(procession);
			Strike(player, monster, DamageType::Magic, Roll(d.min, d.max));
			wake.ticksLeft = 0;
		}
	}
	clocks.lastTile = player.position.tile;

	// The monsters' marks run down.
	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		Monster &monster = Monsters[ActiveMonsters[i]];
		MonsterMarks &marks = MarksOf(monster);
		if (marks.lastTile == monster.position.tile) {
			marks.stillTicks = std::min(marks.stillTicks + 1, 1 << 20);
		} else {
			marks.stillTicks = 0;
			marks.lastTile = monster.position.tile;
		}
		if (marks.deadGroundCooldown > 0)
			marks.deadGroundCooldown--;
		if (marks.struckTicks > 0)
			marks.struckTicks--;
		if (marks.noRegenTicks > 0)
			marks.noRegenTicks--;
		if (marks.wounded && IsTileLit(monster.position.tile))
			marks.scentTicks = std::max(marks.scentTicks, ScentTicks);
		else if (marks.scentTicks > 0)
			marks.scentTicks--;
		if (marks.bleedTicks > 0) {
			marks.bleedTicks--;
			if (marks.bleedTicks % TicksPerSecond == 0 && (monster.hitPoints >> 6) > 0) {
				ApplyMonsterDamage(DamageType::Physical, monster, marks.bleedDamage);
				if ((monster.hitPoints >> 6) <= 0)
					M_StartKill(monster, player);
			}
			if (marks.bleedTicks == 0)
				marks.bleedDamage = 0;
		}
	}
}

void ClearRfa12State()
{
	ClearRfa12ActivesState();
	ResetWhirlwind(); // a spin never crosses a level change
	PlayerState.fill(PlayerClocks {});
	MonsterState.fill(MonsterMarks {});
}

void ClearRfa12StateForMonster(const Monster &monster)
{
	ClearRfa12ActivesForMonster(monster);
	MarksOf(monster) = MonsterMarks {};
}

void OnRfa12MissileStruck(Player &player, Monster &monster, int damage)
{
	OnRfa12ActiveMissileStruck(player, monster, damage);
}

int Rfa12AbsorbDamage(Player &player, int damage)
{
	return Rfa12ActiveAbsorbDamage(player, damage);
}

bool Rfa12EvadesMelee(const Player &player)
{
	return Rfa12ActiveEvadesMelee(player);
}

bool Rfa12StripsResistances(const Monster &monster)
{
	return Rfa12ActiveStripsResistances(monster);
}

bool Rfa12ArrowIgnores(const Player &player, const Monster &monster)
{
	return Rfa12ActiveArrowIgnores(player, monster);
}

int Rfa12ColdDamagePercent(const Monster &monster)
{
	return Rfa12FrostbitePercent(monster);
}

void ApplyRfa12BuffsToTotals(const Player &player, ItemBonusTotals &totals)
{
	ApplyRfa12ActiveBuffsToTotals(player, totals);
}

void BleedMonster(const Monster &monster, int ticks, int perSecond)
{
	MonsterMarks &marks = MarksOf(monster);
	marks.bleedTicks = std::max(marks.bleedTicks, ticks);
	marks.bleedDamage = std::max(marks.bleedDamage, perSecond);
}

bool MonsterBleeding(const Monster &monster)
{
	return MarksOf(monster).bleedTicks > 0 && (monster.hitPoints >> 6) > 0;
}

void BlockMonsterRegen(const Monster &monster, int ticks)
{
	MonsterMarks &marks = MarksOf(monster);
	marks.noRegenTicks = std::max(marks.noRegenTicks, ticks);
}

void ScentMonster(const Monster &monster, int ticks)
{
	MonsterMarks &marks = MarksOf(monster);
	marks.scentTicks = std::max(marks.scentTicks, ticks);
}

void TakeCorpseOf(const Monster &monster)
{
	MarksOf(monster).tithe = true;
}

bool Rfa12AuraReachesMonsters(Skill aura)
{
	switch (aura) {
	case Skill::Radiance:
	case Skill::Condemnation:
	case Skill::TitheOfAsh:
	case Skill::Dominion:
	case Skill::SirensCall:
		return true;
	default:
		return false;
	}
}

std::string Rfa12AuraFactsAt(Skill aura, int points)
{
	const int p = std::max(points, 1);
	switch (aura) {
	case Skill::Radiance: {
		const AuraDamage d = RadianceDamage(p);
		return fmt::format(fmt::runtime(_("Undead in reach take {:d} - {:d} holy damage every {:d} seconds, through magic immunity")), d.min, d.max,
		    RadiancePulseTicks / TicksPerSecond);
	}
	case Skill::BaneOfEvil:
		return fmt::format(fmt::runtime(_("+{:d}% damage against demons and undead")), BaneOfEvilPercent(p));
	case Skill::Condemnation:
		return fmt::format(fmt::runtime(_("Enemy armour: -{:d}%")), ConvictionLikeCut(p));
	case Skill::TitheOfAsh:
		return fmt::format(fmt::runtime(_("A kill in reach restores {:d} mana and leaves no corpse")), TitheMana(p));
	case Skill::Retaliation:
		return fmt::format(fmt::runtime(_("Each blow taken: +{:d}% damage on your next blow, stacking {:d} times")), RetaliationPerStack(p),
		    RetaliationMaxStacks);
	case Skill::DoomProcession: {
		const AuraDamage d = WakeDamage(p);
		return fmt::format(fmt::runtime(_("After {:d} seconds on the move, each tile you leave burns for {:d} seconds: {:d} - {:d} magic damage")),
		    ProcessionArmTicks / TicksPerSecond, WakeTicks / TicksPerSecond, d.min, d.max);
	}
	case Skill::Dominion:
		return fmt::format(fmt::runtime(_("Enemies in reach deal {:d}% less damage and take {:d}% more")), DominionPercent(p), DominionPercent(p));
	case Skill::Immovable:
		return std::string(_("Knockback cannot move you"));
	case Skill::Mercy:
		return fmt::format(fmt::runtime(_("Below {:d}% life: heals {:d}% of your life, once every {:d} seconds")), MercyBelowLifePercent,
		    MercyHealPercent(p), MercyCooldownTicks / TicksPerSecond);
	case Skill::Sanctity:
		return fmt::format(fmt::runtime(_("Slows on you wear off {:d}% sooner")), SanctityShortenPercent);
	case Skill::MinstrelsTune:
		return std::string(_("Restores mana as it plays"));
	case Skill::HymnOfRenewal:
		return std::string(_("Restores life and mana as it plays"));
	case Skill::Nocturne:
		return std::string(_("Monsters notice you only within half your sight"));
	case Skill::AnthemOfValor:
		return std::string(_("Blows do not interrupt you"));
	case Skill::SirensCall:
		return std::string(_("Enemies in reach are drawn to you at half speed"));
	case Skill::SymphonyOfWar:
		return std::string(_("Your other Melody songs lend half their strength"));
	case Skill::SovereignMeasure: {
		const AuraDamage d = SovereignDamage(p);
		return fmt::format(fmt::runtime(_("With one enemy near, every 4th blow on it deals {:d} - {:d} magic damage")), d.min, d.max);
	}
	default:
		return {};
	}
}


std::string Rfa12PassiveFactsAt(const Player &player, ClassTreeSkill skill, int points)
{
	// The lines state the rule whether or not its weapon is in hand; the condition is part of the line.
	(void)player;
	const int p = std::max(points, 1);
	switch (skill) {
	case Skill::DeepWounds:
		return fmt::format(fmt::runtime(_("Melee blows: {:d}% chance to bleed {:d} damage a second for {:d} seconds, with no regeneration")),
		    DeepWoundsChance(p), DeepWoundsPerSecond(p), DeepWoundsTicks / TicksPerSecond);
	case Skill::BattleHardened:
		return fmt::format(fmt::runtime(_("Below half life: -{:d}% fire, lightning and magic damage taken")), BattleHardenedPercent(p));
	case Skill::Bloodlust:
		return fmt::format(fmt::runtime(_("Melee blows return {:d}% of their damage as life")), BloodlustPercent(p));
	case Skill::UnfinishedBusiness:
		return fmt::format(fmt::runtime(_("Killing an enemy that struck you in the last {:d} seconds heals {:d}% of your life")),
		    StruckMemoryTicks / TicksPerSecond, UnfinishedBusinessPercent(p));
	case Skill::LastingWounds:
		return fmt::format(fmt::runtime(_("Enemies you strike in melee cannot regenerate life for {:d} seconds")), LastingWoundsTicks / TicksPerSecond);
	case Skill::GripOfIron:
		return std::string(_("While you swing with one enemy beside you, its blows do not interrupt you"));
	case Skill::HeavyFoot:
		return std::string(_("Holding a two-handed melee weapon: knockback cannot move you"));
	case Skill::LongReach:
		return std::string(_("With a staff, spear or pike, a swing at an empty tile strikes the enemy beyond it"));
	case Skill::ScentOfBlood:
		return fmt::format(fmt::runtime(_("Enemies you wound stay visible for {:d} seconds after they leave the light")), ScentTicks / TicksPerSecond);
	case Skill::SoftTread:
		return fmt::format(fmt::runtime(_("After {:d} seconds walking without attacking, monsters notice you only within two thirds of your light")),
		    SoftTreadQuietTicks / TicksPerSecond);
	case Skill::DeadGround:
		return fmt::format(fmt::runtime(_("First ranged hit on an enemy still for {:d} seconds: +{:d}% damage, once every {:d} seconds per enemy")),
		    DeadGroundStillTicks / TicksPerSecond, DeadGroundPercent(p), DeadGroundCooldownTicks / TicksPerSecond);
	case Skill::Deadeye:
		return fmt::format(fmt::runtime(_("Ranged hits: {:d}% chance to deal +{:d}% damage")), DeadeyeChancePercent, DeadeyePercent(p));
	case Skill::StaffParry:
		return fmt::format(fmt::runtime(_("Holding a staff: +{:d}% chance to block")), BlockPercent(p));
	case Skill::Brace:
		return fmt::format(fmt::runtime(_("Holding a spear or pike: you can block, +{:d}% chance to block")), BlockPercent(p));
	case Skill::DeepBreath:
		return fmt::format(fmt::runtime(_("Life regeneration: {:.1f} a second")), PerSecond(DeepBreathPerTick(p)));
	default:
		return {};
	}
}

} // namespace devilution::oracool
