/**
 * @file oracool/necro_summoning.cpp
 *
 * See necro_summoning.h.
 */
#include "oracool/necro_summoning.h"

#include <algorithm>
#include <array>
#include <optional>

#include "engine.h"
#include "engine/random.hpp"
#include "monster.h"
#include "multi.h"
#include "oracool/class_tree.h"
#include "oracool/companion.h"
#include "oracool/corpses.h"
#include "oracool/minions.h"
#include "oracool/passives.h"
#include "player.h"

namespace devilution::oracool {

namespace {

constexpr int TicksPerSecond = 20;
/** How far from the cursor a corpse may lie for a raise. */
constexpr int CorpseReach = 3;

struct OwnerState {
	/** Army of the Dead: where it erupted, how many pulses are left, ticks to the next. */
	Point armyTile;
	int armyPulses = 0;
	int armyClock = 0;
	int armyRank = 0;
};
std::array<OwnerState, MAX_PLRS> Owners;

OwnerState &StateOf(const Player &player)
{
	return Owners[std::min<size_t>(player.getId(), MAX_PLRS - 1)];
}

int Points(const Player &player, ClassTreeSkill skill)
{
	if (!IsClassTreeSkillUnlocked(player, skill))
		return 0;
	return ClassTreeInvestment(player, skill);
}

/** @brief The skeleton body a rank earns: the four skeleton axemen, weakest to strongest, as the rank climbs. */
_monster_id SkeletonBodyAt(int rank)
{
	if (rank >= 10)
		return MT_XSKELAX;
	if (rank >= 7)
		return MT_RSKELAX;
	if (rank >= 4)
		return MT_TSKELAX;
	return MT_WSKELAX;
}

_monster_id MageBodyAt(int rank)
{
	if (rank >= 10)
		return MT_XSKELBW;
	if (rank >= 7)
		return MT_RSKELBW;
	if (rank >= 4)
		return MT_TSKELBW;
	return MT_WSKELBW;
}

MinionSpec SkeletonSpec(const Player &player, int rank, bool mage)
{
	const int mastery = Points(player, ClassTreeSkill::SkeletonMastery);
	const int plating = Points(player, ClassTreeSkill::BonePlating);
	MinionSpec spec {};
	spec.group = mage ? MinionGroup::Mage : MinionGroup::Skeleton;
	spec.type = mage ? MageBodyAt(rank) : SkeletonBodyAt(rank);
	spec.life = (mage ? 14 : 20) + 6 * rank + 8 * mastery;
	spec.minDamage = (mage ? 1 : 2) + rank / 2 + mastery;
	spec.maxDamage = (mage ? 4 : 5) + rank + 2 * mastery;
	spec.toHit = 60 + 4 * rank + 2 * mastery;
	spec.armorClass = (mage ? 6 : 10) + 2 * rank + 4 * plating;
	if (mage) {
		// Fire, lightning, poison, bone - one of each in turn, so a line of mages is never one colour.
		static const MissileID Elements[] = { MissileID::Firebolt, MissileID::ChargedBolt, MissileID::Acid, MissileID::Arrow };
		spec.missile = Elements[static_cast<size_t>(MinionCount(player, MinionGroup::Mage)) % 4];
	}
	return spec;
}

MinionSpec GolemSpec(const Player &player, GolemKind kind, int rank)
{
	const int mastery = Points(player, ClassTreeSkill::GolemMastery);
	const int plating = Points(player, ClassTreeSkill::BonePlating);
	MinionSpec spec {};
	spec.group = MinionGroup::Golem;
	spec.type = MT_GOLEM;
	spec.golem = kind;
	switch (kind) {
	case GolemKind::Clay:
		spec.life = 100 + 30 * rank;
		spec.minDamage = 4 + 2 * rank;
		spec.maxDamage = 10 + 3 * rank;
		spec.armorClass = 20 + 3 * rank;
		spec.ramp = 200; // tan: the potter's earth
		break;
	case GolemKind::Blood:
		spec.life = 80 + 25 * rank;
		spec.minDamage = 6 + 2 * rank;
		spec.maxDamage = 14 + 3 * rank;
		spec.armorClass = 15 + 2 * rank;
		spec.ramp = 224; // the reds
		break;
	case GolemKind::Iron:
		spec.life = 120 + 35 * rank;
		spec.minDamage = 8 + 3 * rank;
		spec.maxDamage = 16 + 4 * rank;
		spec.armorClass = 40 + 4 * rank;
		spec.ramp = 240; // the greys
		break;
	case GolemKind::Fire:
		spec.life = 90 + 25 * rank;
		spec.minDamage = 5 + 2 * rank;
		spec.maxDamage = 12 + 3 * rank;
		spec.armorClass = 15 + 2 * rank;
		spec.ramp = 208; // the oranges
		break;
	case GolemKind::None:
		break;
	}
	spec.life += spec.life * 15 * mastery / 100;
	spec.toHit = 70 + 4 * rank + 3 * mastery;
	spec.armorClass += 4 * plating;
	return spec;
}

bool RaiseGolem(Player &player, GolemKind kind, int rank, Point target)
{
	// One golem of any kind: a new one replaces the old.
	DismissMinions(player, MinionGroup::Golem);
	return SummonMinion(player, GolemSpec(player, kind, rank), target);
}

bool RaiseFromCorpse(Player &player, Point target, int rank, bool mage)
{
	const MinionGroup group = mage ? MinionGroup::Mage : MinionGroup::Skeleton;
	if (MinionCount(player, group) >= RaisedCountAtRank(rank, MinionGroupCap(group))) {
		player.Say(HeroSpeech::ICantDoThat);
		return false;
	}
	const std::optional<Corpse> corpse = TakeCorpseNear(target, CorpseReach, /*forRevive=*/false);
	if (!corpse) {
		player.Say(HeroSpeech::ICantDoThat);
		return false;
	}
	OnPassiveCorpseConsumed(player);
	const MinionSpec spec = SkeletonSpec(player, rank, mage);
	if (!SummonMinion(player, spec, corpse->position))
		return SummonMinion(player, spec, player.position.tile);
	return true;
}

bool Revive(Player &player, Point target, int rank)
{
	if (MinionCount(player, MinionGroup::Revived) >= RaisedCountAtRank(rank, MinionGroupCap(MinionGroup::Revived))) {
		player.Say(HeroSpeech::ICantDoThat);
		return false;
	}
	const std::optional<Corpse> corpse = TakeCorpseNear(target, CorpseReach, /*forRevive=*/true);
	if (!corpse) {
		player.Say(HeroSpeech::ICantDoThat);
		return false;
	}
	MinionSpec spec {};
	spec.group = MinionGroup::Revived;
	spec.type = corpse->type;
	// As it was, plus a little of the hero's craft; Diablo II's Revive gives its dead the same.
	spec.life = corpse->maxLife + corpse->maxLife * 5 * rank / 100;
	spec.minDamage = corpse->minDamage;
	spec.maxDamage = corpse->maxDamage;
	spec.toHit = corpse->toHit;
	spec.armorClass = corpse->armorClass;
	// Three minutes, and half a minute more for every point of Lasting Bond.
	spec.ticksLeft = (180 + 30 * Points(player, ClassTreeSkill::LastingBond)) * TicksPerSecond;
	if (PassiveActive(player, ClassTreeSkill::ExtendedServitude))
		spec.ticksLeft += spec.ticksLeft / 4; // a quarter longer (N8)
	OnPassiveCorpseConsumed(player);
	if (!SummonMinion(player, spec, corpse->position))
		return SummonMinion(player, spec, player.position.tile);
	return true;
}

/** @brief A blow of the hero's on @p monster: nothing to the immune, a quarter to the resistant, credit and reaction his. */
void HeroStrikes(Player &player, Monster &monster, DamageType type, int damage)
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
	if ((monster.hitPoints >> 6) <= 0) {
		M_StartKill(monster, player);
		return;
	}
	M_StartHit(monster, player, damage);
}

void ArmyPulse(Player &player, OwnerState &state)
{
	const int rank = state.armyRank;
	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		Monster &monster = Monsters[ActiveMonsters[i]];
		if (monster.position.tile.WalkingDistance(state.armyTile) > 2)
			continue;
		const int damage = (4 + 2 * rank + GenerateRnd(5 + 2 * rank)) << 6;
		HeroStrikes(player, monster, DamageType::Magic, damage);
	}
}

} // namespace

int RaisedCountAtRank(int rank, int cap)
{
	return std::clamp(1 + std::max(rank - 1, 0) / 3, 1, cap);
}

bool IsNecromancerSummoning(SpellID spell)
{
	return IsAnyOf(spell, SpellID::RaiseSkeleton, SpellID::CommandTheDead, SpellID::ClayGolem, SpellID::GatherTheDead,
	    SpellID::RaiseSkeletalMage, SpellID::DarkMending, SpellID::BloodGolem, SpellID::FrenzyOfTheDead, SpellID::IronGolem,
	    SpellID::UnholyOffering, SpellID::FireGolem, SpellID::NecroRevive, SpellID::ArmyOfTheDead);
}

bool CastNecromancerSummoning(Player &player, SpellID spell, Point target, int rank)
{
	const int r = std::max(rank, 1);
	switch (spell) {
	case SpellID::RaiseSkeleton:
		return RaiseFromCorpse(player, target, r, /*mage=*/false);
	case SpellID::RaiseSkeletalMage:
		return RaiseFromCorpse(player, target, r, /*mage=*/true);
	case SpellID::ClayGolem:
		return RaiseGolem(player, GolemKind::Clay, r, target);
	case SpellID::BloodGolem:
		return RaiseGolem(player, GolemKind::Blood, r, target);
	case SpellID::IronGolem:
		return RaiseGolem(player, GolemKind::Iron, r, target);
	case SpellID::FireGolem:
		return RaiseGolem(player, GolemKind::Fire, r, target);
	case SpellID::NecroRevive:
		return Revive(player, target, r);
	case SpellID::CommandTheDead: {
		Monster *monster = FindMonsterAtPosition(target);
		if (monster == nullptr || monster->isPlayerMinion() || (monster->hitPoints >> 6) <= 0) {
			player.Say(HeroSpeech::ICantDoThat);
			return false;
		}
		FocusCompanionsOn(*monster, (6 + r) * TicksPerSecond);
		return true;
	}
	case SpellID::GatherTheDead:
		return GatherMinions(player) > 0;
	case SpellID::DarkMending:
		return HealMinions(player, 8, 20 + 4 * r) > 0;
	case SpellID::FrenzyOfTheDead:
		if (MinionCount(player) == 0) {
			player.Say(HeroSpeech::ICantDoThat);
			return false;
		}
		FrenzyMinions(player, 10 * TicksPerSecond, 40 + 4 * r);
		return true;
	case SpellID::UnholyOffering: {
		const int life = SacrificeMinion(player, target);
		if (life <= 0) {
			player.Say(HeroSpeech::ICantDoThat);
			return false;
		}
		const int heal = life * std::min(30 + 3 * r, 75) / 100;
		player._pHitPoints = std::min(player._pHitPoints + heal, player._pMaxHP);
		player._pHPBase = std::min(player._pHPBase + heal, player._pMaxHPBase);
		return true;
	}
	case SpellID::ArmyOfTheDead: {
		OwnerState &state = StateOf(player);
		state.armyTile = target;
		state.armyPulses = 6;
		state.armyClock = 0;
		state.armyRank = r;
		return true;
	}
	default:
		return false;
	}
}

void ProcessNecromancerSummoningTick(Player &player)
{
	OwnerState &state = StateOf(player);
	if (state.armyPulses <= 0)
		return;
	if (--state.armyClock > 0)
		return;
	state.armyClock = TicksPerSecond / 2;
	state.armyPulses--;
	ArmyPulse(player, state);
}

void ClearNecromancerSummoningState()
{
	for (OwnerState &state : Owners)
		state = {};
}

} // namespace devilution::oracool
