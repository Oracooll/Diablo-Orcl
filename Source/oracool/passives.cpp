#include "oracool/passives.h"

#include <algorithm>
#include <array>

#include "engine/backbuffer_state.hpp"
#include "engine/random.hpp"
#include "misdat.h"
#include "missiles.h"
#include "monster.h"
#include "oracool/chill.h"
#include "oracool/rage.h"
#include "oracool/warcries.h"
#include "player.h"

namespace devilution::oracool {

namespace {

using Skill = ClassTreeSkill;

/** The clocks, per player index. */
struct Clocks {
	int stillTicks = 0;        // consecutive ticks not walking
	int rampageStacks = 0;     // Rampage: kills stacked
	int rampageTicks = 0;      // ...and ticks left before they fall off
	int cheatDeathCooldown = 0; // ticks until the next save is allowed
	int cadenceCount = 0;      // Cadence: melee blows since the last beat
	int inspireTicks = 0;      // Inspiring Presence: ticks under a warcry blessing, for the per-second mend
	int juggernautCooldown = 0; // Juggernaut: ticks until the next heal may fire
};

std::array<Clocks, MAX_PLRS> ClocksOf;

Clocks &ClocksFor(const Player &player)
{
	return ClocksOf[player.getId()];
}

constexpr int StillnessTicks = 30;      // a moment: a second and a half
constexpr int RampageHoldTicks = 100;   // five seconds
constexpr int RampageMaxStacks = 5;
constexpr int CheatDeathCooldownTicks = 1200; // a minute
constexpr int JuggernautCooldownTicks = 200;  // ten seconds

bool WieldingMace(const Player &player)
{
	for (const Item &item : { player.InvBody[INVLOC_HAND_LEFT], player.InvBody[INVLOC_HAND_RIGHT] }) {
		if (!item.isEmpty() && item._iStatFlag && item._itype == ItemType::Mace)
			return true;
	}
	return false;
}

bool WearingShield(const Player &player)
{
	for (const Item &item : { player.InvBody[INVLOC_HAND_LEFT], player.InvBody[INVLOC_HAND_RIGHT] }) {
		if (!item.isEmpty() && item._iStatFlag && item._itype == ItemType::Shield)
			return true;
	}
	return false;
}

bool Still(const Player &player)
{
	return ClocksFor(player).stillTicks >= StillnessTicks;
}

bool BelowAThird(const Player &player)
{
	return player._pHitPoints * 3 < player._pMaxHP;
}

bool Stunned(const Monster &monster)
{
	return monster.mode == MonsterMode::Delay;
}

/** @brief Monsters that can be hit within @p range tiles of @p centre, not counting @p except. */
int MonstersNear(Point centre, int range, const Monster *except)
{
	int count = 0;
	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		const Monster &other = Monsters[ActiveMonsters[i]];
		if (&other == except || !other.isPossibleToHit() || other.hitPoints >> 6 <= 0)
			continue;
		if (centre.WalkingDistance(other.position.tile) <= range)
			count++;
	}
	return count;
}

/** @brief The Rogue's three slips share one curve: a tenth, a twenty-fifth more a rank, two fifths at most. */
int SlipChance(const Player &player, Skill skill)
{
	if (!PassiveActive(player, skill))
		return 0;
	const int points = ClassTreeInvestment(player, skill);
	return std::min(10 + 4 * (points - 1), 40);
}

void Heal(Player &player, int amount)
{
	if (amount <= 0 || player._pHitPoints <= 0 || player._pHitPoints >= player._pMaxHP)
		return;
	player._pHitPoints = std::min(player._pHitPoints + amount, player._pMaxHP);
	player._pHPBase = std::min(player._pHPBase + amount, player._pMaxHPBase);
	RedrawComponent(PanelDrawComponent::Health);
}

} // namespace

bool PassiveActive(const Player &player, Skill skill)
{
	if (skill == Skill::None || skill > Skill::LAST)
		return false;
	const ClassTreeSkillData &data = GetClassTreeSkillData(skill);
	if (!data.implemented || data.heroClass != player._pClass)
		return false;
	if (!IsClassTreeSkillUnlocked(player, skill))
		return false;
	if (IsPassiveSkillRow(skill))
		return PassiveSlotOf(player, skill) >= 0;
	return ClassTreeInvestment(player, skill) > 0;
}

int PassiveDamageTakenPercent(const Player &player, DamageType damageType)
{
	int percent = 0;
	const bool notSteel = damageType != DamageType::Physical;
	if (PassiveActive(player, Skill::Blur))
		percent -= 17;
	if (notSteel && PassiveActive(player, Skill::SixthSense))
		percent -= 25;
	if (notSteel && PassiveActive(player, Skill::Vigilant))
		percent -= 20;
	if (WearingShield(player) && PassiveActive(player, Skill::SwordAndBoard))
		percent -= 30;
	if (BelowAThird(player) && PassiveActive(player, Skill::Relentless))
		percent -= 25;
	if (Still(player) && PassiveActive(player, Skill::UnwaveringWill))
		percent -= 20;
	// Whatever stacks, a blow always lands: a quarter of it at the least.
	return std::max(percent, -75);
}

int PassiveDamageDealtPercent(const Player &player, const Monster &target, bool melee)
{
	int percent = 0;
	const int targetLife = target.hitPoints;
	const int targetMax = std::max(target.maxHitPoints, 1);
	const int distance = player.position.tile.WalkingDistance(target.position.tile);
	const bool chilled = IsMonsterChilled(target) || IsMonsterFrozen(target);

	if (PassiveActive(player, Skill::Ruthless) && targetLife * 3 < targetMax)
		percent += 40;
	if (PassiveActive(player, Skill::Ambush) && targetLife * 4 >= targetMax * 3)
		percent += 40;
	if (PassiveActive(player, Skill::Brawler) && MonstersNear(player.position.tile, 1, nullptr) >= 3)
		percent += 20;
	if (PassiveActive(player, Skill::Determination))
		percent += std::min(MonstersNear(player.position.tile, 1, nullptr), 4) * 5;
	if (PassiveActive(player, Skill::SteadyAim) && MonstersNear(player.position.tile, 3, nullptr) == 0)
		percent += 20;
	if (PassiveActive(player, Skill::Audacity) && distance <= 2)
		percent += 15;
	if (PassiveActive(player, Skill::PowerHungry) && distance >= 5)
		percent += 20;
	if (PassiveActive(player, Skill::ColdBlooded) && chilled)
		percent += 10;
	if (PassiveActive(player, Skill::CullTheWeak) && chilled)
		percent += 20;
	if (PassiveActive(player, Skill::RelentlessAssault) && (IsMonsterFrozen(target) || Stunned(target)))
		percent += 30;
	if (PassiveActive(player, Skill::SingleOut) && MonstersNear(target.position.tile, 2, &target) == 0)
		percent += 25;
	if (PassiveActive(player, Skill::Rampage))
		percent += ClocksFor(player).rampageStacks * 5;
	if (PassiveActive(player, Skill::UnwaveringWill) && Still(player))
		percent += 10;
	// The Barbarian's (2026-09-14). Berserker Rage reads the pool at the moment of the blow.
	if (PassiveActive(player, Skill::BerserkerRage) && UsesRage(player) && player._pRage * 2 >= MaxRage(player))
		percent += 25;
	if (PassiveActive(player, Skill::NoEscape) && distance >= 5)
		percent += 25;
	// The beat: the third blow since the last one. Counted in OnPassiveHit, read here, so the
	// blow that IS the beat carries the bonus and the count restarts after it lands.
	if (melee && PassiveActive(player, Skill::Cadence) && ClocksFor(player).cadenceCount == 2)
		percent += 50;
	return percent;
}

bool PassiveEvadesMelee(const Player &player)
{
	const bool walking = IsAnyOf(player._pmode, PM_WALK_NORTHWARDS, PM_WALK_SOUTHWARDS, PM_WALK_SIDEWAYS);
	const int chance = SlipChance(player, walking ? Skill::Evade : Skill::Dodge);
	return chance > 0 && GenerateRnd(100) < chance;
}

bool PassiveEvadesMissile(const Player &player)
{
	const int chance = SlipChance(player, Skill::Avoid);
	return chance > 0 && GenerateRnd(100) < chance;
}

bool ArrowPierces(Missile &missile)
{
	if (missile.sourceType() != MissileSource::Player || !GetMissileData(missile._mitype).isArrow())
		return false;
	const Player &player = *missile.sourcePlayer();
	if (!PassiveActive(player, Skill::Pierce))
		return false;
	const int points = ClassTreeInvestment(player, Skill::Pierce);
	const int chance = std::min(15 + 5 * (points - 1), 60);
	return GenerateRnd(100) < chance;
}

bool PassiveCheatsDeath(Player &player)
{
	Clocks &clocks = ClocksFor(player);
	if (clocks.cheatDeathCooldown > 0)
		return false;
	const bool nearDeath = PassiveActive(player, Skill::NearDeathExperience);
	if (!nearDeath && !PassiveActive(player, Skill::Indestructible) && !PassiveActive(player, Skill::NervesOfSteel)
	    && !PassiveActive(player, Skill::Awareness))
		return false;

	SetPlayerHitPoints(player, player._pMaxHP / 3);
	if (nearDeath) {
		player._pMana = player._pMaxMana / 3;
		player._pManaBase = player._pMaxManaBase - (player._pMaxMana - player._pMana);
		RedrawComponent(PanelDrawComponent::Mana);
	}
	clocks.cheatDeathCooldown = CheatDeathCooldownTicks;
	return true;
}

void OnPassiveHit(Player &player, const Monster & /*target*/, int damage, bool melee)
{
	if (damage > 0 && PassiveActive(player, Skill::Leech))
		Heal(player, damage * 3 / 100);
	if (melee && PassiveActive(player, Skill::Cadence)) {
		Clocks &clocks = ClocksFor(player);
		clocks.cadenceCount = (clocks.cadenceCount + 1) % 3;
	}
	// Weapons Master's mace: a point of Rage for every blow that lands, with or without a skill.
	if (melee && PassiveActive(player, Skill::WeaponsMaster) && WieldingMace(player))
		GainRage(player, 1);
}

bool PassiveShrugsOffStagger(Player &player)
{
	if (!PassiveActive(player, Skill::Juggernaut))
		return false;
	if (GenerateRnd(100) < 50)
		return true;
	// The stagger lands - and sometimes gives something back.
	Clocks &clocks = ClocksFor(player);
	if (clocks.juggernautCooldown == 0 && GenerateRnd(100) < 30) {
		Heal(player, player._pMaxHP / 5);
		clocks.juggernautCooldown = JuggernautCooldownTicks;
	}
	return false;
}

int PassiveSlowShortenPercent(const Player &player)
{
	return PassiveActive(player, Skill::Juggernaut) ? 50 : 0;
}

int PassiveWarcryDurationPercent(const Player &player)
{
	return PassiveActive(player, Skill::InspiringPresence) ? 200 : 100;
}

void OnPassiveManaSpent(Player &player, int cost)
{
	if (cost <= 0)
		return;
	if (PassiveActive(player, Skill::Bloodthirst) || PassiveActive(player, Skill::Transcendence))
		Heal(player, cost / 2);
}

void OnPassiveMonsterKilled(Player &player, const Monster &monster)
{
	Clocks &clocks = ClocksFor(player);
	if (PassiveActive(player, Skill::Rampage)) {
		clocks.rampageStacks = std::min(clocks.rampageStacks + 1, RampageMaxStacks);
		clocks.rampageTicks = RampageHoldTicks;
	}
	if (PassiveActive(player, Skill::Requiem) && player.position.tile.WalkingDistance(monster.position.tile) <= 4)
		Heal(player, player._pMaxHP / 50);
	if (PassiveActive(player, Skill::PoundOfFlesh))
		Heal(player, player._pMaxHP * 3 / 100);
}

bool PassiveRunActive(const Player &player)
{
	return PassiveActive(player, Skill::FleetFooted);
}

void ProcessPassivesTick(Player &player)
{
	Clocks &clocks = ClocksFor(player);
	const bool walking = IsAnyOf(player._pmode, PM_WALK_NORTHWARDS, PM_WALK_SOUTHWARDS, PM_WALK_SIDEWAYS);
	clocks.stillTicks = walking ? 0 : std::min(clocks.stillTicks + 1, 1 << 20);
	if (clocks.rampageTicks > 0 && --clocks.rampageTicks == 0)
		clocks.rampageStacks = 0;
	if (clocks.cheatDeathCooldown > 0)
		clocks.cheatDeathCooldown--;
	if (clocks.juggernautCooldown > 0)
		clocks.juggernautCooldown--;
	// Inspiring Presence: under any warcry blessing, a hundredth of your life every second.
	if (player._pHitPoints > 0 && PassiveActive(player, Skill::InspiringPresence) && AnyWarcryBuffActive(player)) {
		if (++clocks.inspireTicks % 20 == 0)
			Heal(player, player._pMaxHP / 100);
	} else {
		clocks.inspireTicks = 0;
	}
	// Brooding: still for a moment, and then a hundredth of your life every second.
	if (player._pHitPoints > 0 && Still(player) && PassiveActive(player, Skill::Brooding)
	    && (clocks.stillTicks - StillnessTicks) % 20 == 0)
		Heal(player, player._pMaxHP / 100);
}

void ClearPassiveState()
{
	ClocksOf.fill(Clocks {});
}

void ClearPassiveClocks(Player &player)
{
	ClocksFor(player) = Clocks {};
}

} // namespace devilution::oracool
