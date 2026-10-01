/**
 * @file oracool/necro_summoning.cpp
 *
 * See necro_summoning.h.
 */
#include "oracool/necro_summoning.h"

#include <algorithm>
#include <array>
#include <optional>
#include <string>

#include <fmt/format.h>

#include "engine.h"
#include "engine/random.hpp"
#include "missiles.h"
#include "monster.h"
#include "multi.h"
#include "oracool/class_tree.h"
#include "oracool/companion.h"
#include "oracool/corpses.h"
#include "oracool/curses.h"
#include "oracool/minions.h"
#include "oracool/missile_tint.h"
#include "oracool/passives.h"
#include "oracool/skill_sounds.h"
#include "player.h"
#include "utils/language.h"

namespace devilution::oracool {

namespace {

constexpr int TicksPerSecond = 20;
/** How far from the cursor a corpse may lie for a raise. */
constexpr int CorpseReach = 3;

// ---- the page's numbers, one place each: the rules below and NecroSummoningFactsAt / NecroPassiveFactsAt both read them ----

/** Skeleton Mastery, per point, for skeletons and mages: life, damage min/max, to hit. */
constexpr int SkeletonMasteryLife = 8;
constexpr int SkeletonMasteryMinDamage = 1;
constexpr int SkeletonMasteryMaxDamage = 2;
constexpr int SkeletonMasteryToHit = 2;
/** Golem Mastery, per point: life in percent, to hit. */
constexpr int GolemMasteryLifePercent = 15;
constexpr int GolemMasteryToHit = 3;
/** Bone Plating, per point: armour for skeletons, mages and golems. */
constexpr int BonePlatingArmour = 4;
/** Command the Dead: the focus lasts 6 s + 1 s a rank. */
constexpr int CommandBaseSeconds = 6;
/** Dark Mending: minions within this many tiles of the hero. */
constexpr int DarkMendingRadius = 8;
/** Frenzy of the Dead: how long. */
constexpr int FrenzySeconds = 10;
/** Revive: three minutes, half a minute more a point of Lasting Bond; Extended Servitude adds 1/N of that. */
constexpr int ReviveBaseSeconds = 180;
constexpr int LastingBondSecondsPerPoint = 30;
constexpr int ExtendedServitudeDivisor = 4;
/** Army of the Dead: pulses, ticks between them, and their reach around the cursor. */
constexpr int ArmyPulses = 6;
constexpr int ArmyPulseTicks = TicksPerSecond / 2;
constexpr int ArmyRadius = 2;

int DarkMendingPercent(int rank) { return 20 + 4 * rank; }
int FrenzyPercent(int rank) { return 40 + 4 * rank; }
int UnholyOfferingPercent(int rank) { return std::min(30 + 3 * rank, 75); }
int ReviveLifePercent(int rank) { return 5 * rank; }
/** Army of the Dead: magic damage min + GenerateRnd(spread) a pulse, in whole points. */
int ArmyDamageMin(int rank) { return 4 + 2 * rank; }
int ArmyDamageSpread(int rank) { return 5 + 2 * rank; }

int ReviveTicks(int lastingBondPoints, bool extendedServitude)
{
	int ticks = (ReviveBaseSeconds + LastingBondSecondsPerPoint * lastingBondPoints) * TicksPerSecond;
	if (extendedServitude)
		ticks += ticks / ExtendedServitudeDivisor; // a quarter longer (N8)
	return ticks;
}

/** @brief @p ticks as seconds: "12", or "12.5" when it does not come out whole. */
std::string SecondsText(int ticks)
{
	if (ticks % TicksPerSecond == 0)
		return fmt::format("{:d}", ticks / TicksPerSecond);
	return fmt::format("{:.1f}", static_cast<double>(ticks) / TicksPerSecond);
}

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
	spec.life = (mage ? 14 : 20) + 6 * rank + SkeletonMasteryLife * mastery;
	spec.minDamage = (mage ? 1 : 2) + rank / 2 + SkeletonMasteryMinDamage * mastery;
	spec.maxDamage = (mage ? 4 : 5) + rank + SkeletonMasteryMaxDamage * mastery;
	spec.toHit = 60 + 4 * rank + SkeletonMasteryToHit * mastery;
	spec.armorClass = (mage ? 6 : 10) + 2 * rank + BonePlatingArmour * plating;
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
	spec.life += spec.life * GolemMasteryLifePercent * mastery / 100;
	spec.toHit = 70 + 4 * rank + GolemMasteryToHit * mastery;
	spec.armorClass += BonePlatingArmour * plating;
	// The monster keeps its damage in a byte (minions.cpp clamps there): held here too, so the tooltip and the Fire Golem's
	// burn read what the golem strikes for (round 44 audit).
	spec.minDamage = std::min(spec.minDamage, 255);
	spec.maxDamage = std::min(spec.maxDamage, 255);
	spec.armorClass = std::min(spec.armorClass, 255); // the body's armour is a byte too (round 46 audit)
	return spec;
}

bool RaiseGolem(Player &player, GolemKind kind, int rank, Point target)
{
	// One golem of any kind: a new one replaces the old - but only when the new one can stand (user, 2026-09-27: "fix
	// the decisions for me too"). A cast at solid rock dismissed the old golem and raised nothing. The type and a record
	// are known up front, and the body falls back to the hero's own side, as Raise's does.
	const MinionSpec spec = GolemSpec(player, kind, rank);
	// A free record too, before the old golem is dismissed: its dying body keeps its own record, so with every record held
	// the old golem went and nothing replaced it (round 33 audit).
	if (!CanAddMinionBody(spec.type) || !MinionRecordFree()) {
		player.Say(HeroSpeech::ICantDoThat);
		return false;
	}
	DismissMinions(player, MinionGroup::Golem);
	if (SummonMinion(player, spec, target) || SummonMinion(player, spec, player.position.tile))
		return true;
	player.Say(HeroSpeech::ICantDoThat);
	return false;
}

bool RaiseFromCorpse(Player &player, Point target, int rank, bool mage)
{
	const MinionGroup group = mage ? MinionGroup::Mage : MinionGroup::Skeleton;
	if (MinionCount(player, group) >= RaisedCountAtRank(rank, MinionGroupCap(group))) {
		player.Say(HeroSpeech::ICantDoThat);
		return false;
	}
	// Know that a body can stand BEFORE the corpse is eaten (audit, 2026-09-19): the corpse was
	// taken, the haste granted and the effect played, and only then SummonMinion could refuse - a
	// floor already carrying its full count of monster types and no skeleton among them, or no
	// free tile - leaving a spent corpse and a fizzled cast. The type check is answerable up front;
	// the haste and the effect wait for the body.
	const MinionSpec spec = SkeletonSpec(player, rank, mage);
	// A free record too, as Revive asks: dying bodies hold theirs, and the corpse was eaten for a cast that fizzled (round 33).
	// A corpse he can see (round 41 audit: one behind a wall was eaten and the skeleton stood in the other room).
	const std::optional<Corpse> corpse = PeekCorpseNearSeen(target, CorpseReach, player.position.tile, /*forRevive=*/false);
	if (!corpse || !CanAddMinionBody(spec.type) || !MinionRecordFree()) {
		player.Say(HeroSpeech::ICantDoThat);
		return false;
	}
	// The body first, the corpse after (round 48 audit: with no tile free the corpse was eaten and nothing stood).
	if (!SummonMinion(player, spec, corpse->position) && !SummonMinion(player, spec, player.position.tile)) {
		player.Say(HeroSpeech::ICantDoThat);
		return false;
	}
	TakeCorpseNear(corpse->position, 0, /*forRevive=*/false);
	OnPassiveCorpseConsumed(player);
	AddMissile(corpse->position, corpse->position, player._pdir, MissileID::RaiseDeadEffect, TARGET_MONSTERS, static_cast<int>(player.getId()), 0, 0);
	return true;
}

bool Revive(Player &player, Point target, int rank)
{
	if (MinionCount(player, MinionGroup::Revived) >= RaisedCountAtRank(rank, MinionGroupCap(MinionGroup::Revived))) {
		player.Say(HeroSpeech::ICantDoThat);
		return false;
	}
	// A record for the body before the corpse is taken (2026-09-27): with all of them held - bodies still dying - the
	// corpse was eaten and nothing rose.
	if (!MinionRecordFree()) {
		player.Say(HeroSpeech::ICantDoThat);
		return false;
	}
	const std::optional<Corpse> corpse = PeekCorpseNearSeen(target, CorpseReach, player.position.tile, /*forRevive=*/true); // seen (round 41), not yet taken
	if (!corpse || !CanAddMinionBody(corpse->type)) {
		player.Say(HeroSpeech::ICantDoThat);
		return false;
	}
	MinionSpec spec {};
	spec.group = MinionGroup::Revived;
	spec.type = corpse->type;
	// As it was, plus a little of the hero's craft; Diablo II's Revive gives its dead the same.
	spec.life = corpse->maxLife + corpse->maxLife * ReviveLifePercent(rank) / 100;
	spec.minDamage = corpse->minDamage;
	spec.maxDamage = corpse->maxDamage;
	spec.toHit = corpse->toHit;
	spec.armorClass = corpse->armorClass;
	// Three minutes, and half a minute more for every point of Lasting Bond.
	spec.ticksLeft = ReviveTicks(Points(player, ClassTreeSkill::LastingBond), PassiveActive(player, ClassTreeSkill::ExtendedServitude));
	// The body first, the corpse, the haste and the effect after (audit, 2026-09-19; round 48: the corpse is taken only once
	// the body stands).
	if (!SummonMinion(player, spec, corpse->position) && !SummonMinion(player, spec, player.position.tile)) {
		player.Say(HeroSpeech::ICantDoThat);
		return false;
	}
	TakeCorpseNear(corpse->position, 0, /*forRevive=*/true);
	OnPassiveCorpseConsumed(player);
	AddMissile(corpse->position, corpse->position, player._pdir, MissileID::RaiseDeadEffect, TARGET_MONSTERS, static_cast<int>(player.getId()), 0, 0);
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
	damage += damage * PassiveDamageDealtPercent(player, monster, /*melee=*/false) / 100; // Army of the Dead (round 15)
	if (damage <= 0)
		return;
	OnCursedMonsterStruck(monster, player, nullptr, damage); // Life Tap, as every tree skill's strike since round 20 (round 26)
	ApplyMonsterDamage(type, monster, damage);
	if ((monster.hitPoints >> 6) <= 0) {
		M_StartKill(monster, player);
		return;
	}
	M_StartHit(monster, player, damage);
}

/** @brief A skeleton type loaded on this level, for the Army's charge - one of the hero's own first; null if none is. */
const CMonster *ArmySkeletonType()
{
	static constexpr _monster_id Skeletons[] = { MT_WSKELAX, MT_TSKELAX, MT_RSKELAX, MT_XSKELAX, MT_WSKELSD, MT_TSKELSD, MT_RSKELSD, MT_XSKELSD };
	for (const _monster_id wanted : Skeletons) {
		for (size_t i = 0; i < LevelMonsterTypeCount; i++) {
			const CMonster &type = LevelMonsterTypes[i];
			if (type.type == wanted && type.getAnimData(MonsterGraphic::Walk).sprites)
				return &type;
		}
	}
	return nullptr;
}

/**
 * @brief The Army's picture for one pulse (v1.12.211, user 2026-09-27: "green skeletons charging and bursting into green
 * bone hits"): three skeletons, in the Necromancer's green, running in from beyond the field and bursting into bone where
 * they land. Where no skeleton is loaded on the level the bone bursts alone. Drawn only; the pulse strikes by itself.
 */
void ArmyCharge(const Player &player, Point centre)
{
	constexpr uint32_t Green = oracool::Rgb(120, 214, 104);
	const CMonster *skeleton = ArmySkeletonType();
	for (int i = 0; i < 3; i++) {
		const Point landing = centre + Displacement { GenerateRnd(2 * ArmyRadius + 1) - ArmyRadius, GenerateRnd(2 * ArmyRadius + 1) - ArmyRadius };
		const Direction from = static_cast<Direction>(GenerateRnd(8));
		const Point start = landing + Displacement(from) * 5;
		Missile *charge = skeleton != nullptr && InDungeonBounds(start) && InDungeonBounds(landing)
		    ? AddCreatureBolt(start, landing, *skeleton, static_cast<int>(player.getId()), 12, MissileGraphicID::BoneHitNecro)
		    : nullptr;
		if (charge != nullptr) {
			charge->oracoolTint = oracool::Tint::Hue;
			charge->oracoolTintRgb = Green;
		} else if (Missile *burst = AddArtEffect(landing, MissileGraphicID::BoneHitNecro, static_cast<int>(player.getId())); burst != nullptr) {
			burst->oracoolTint = oracool::Tint::Hue;
			burst->oracoolTintRgb = Green;
		}
	}
}

void ArmyPulse(Player &player, OwnerState &state)
{
	const int rank = state.armyRank;
	// The cue of the dead tearing loose (RfA-27 batch 51), the local player's own, and the skeletons' charge.
	ArmyCharge(player, state.armyTile);
	if (&player == MyPlayer)
		PlaySkillSound(ClassTreeSkill::ArmyOfTheDead, SkillSoundEvent::Impact);
	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		Monster &monster = Monsters[ActiveMonsters[i]];
		if (monster.position.tile.WalkingDistance(state.armyTile) > ArmyRadius
		    || !LineClearMissile(state.armyTile, monster.position.tile)) // in sight of the charge (round 8 audit)
			continue;
		const int damage = (ArmyDamageMin(rank) + GenerateRnd(ArmyDamageSpread(rank))) << 6;
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
		// Not in town, where dMonster holds towner ids and FindMonsterAtPosition answers a stale slot (round 5 audit).
		Monster *monster = leveltype == DTYPE_TOWN ? nullptr : FindMonsterAtPosition(target);
		// Nor one the army could never strike - a talker, a hidden or fading one, one behind a wall - paid for an order that did
		// nothing (round 46 audit).
		if (monster == nullptr || monster->isPlayerMinion() || IsCompanion(*monster) || (monster->hitPoints >> 6) <= 0
		    || !monster->isPossibleToHit() || (monster->flags & MFLAG_HIDDEN) != 0
		    || !LineClearMissile(player.position.tile, monster->position.tile)) {
			player.Say(HeroSpeech::ICantDoThat);
			return false;
		}
		FocusCompanionsOn(*monster, (CommandBaseSeconds + r) * TicksPerSecond);
		return true;
	}
	case SpellID::GatherTheDead:
		return GatherMinions(player) > 0;
	case SpellID::DarkMending:
		return HealMinions(player, DarkMendingRadius, DarkMendingPercent(r)) > 0;
	case SpellID::FrenzyOfTheDead:
		if (MinionCount(player) == 0) {
			player.Say(HeroSpeech::ICantDoThat);
			return false;
		}
		FrenzyMinions(player, FrenzySeconds * TicksPerSecond, FrenzyPercent(r));
		return true;
	case SpellID::UnholyOffering: {
		const int life = SacrificeMinion(player, target);
		if (life <= 0) {
			player.Say(HeroSpeech::ICantDoThat);
			return false;
		}
		const int heal = life * UnholyOfferingPercent(r) / 100;
		player._pHitPoints = std::min(player._pHitPoints + heal, player._pMaxHP);
		player._pHPBase = std::min(player._pHPBase + heal, player._pMaxHPBase);
		return true;
	}
	case SpellID::ArmyOfTheDead: {
		OwnerState &state = StateOf(player);
		state.armyTile = target;
		state.armyPulses = ArmyPulses;
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
	// Not from a corpse (round 31 audit): the last pulses fought on after the Necromancer fell.
	if (player._pmode == PM_DEATH || (player._pHitPoints >> 6) <= 0) {
		state.armyPulses = 0;
		return;
	}
	if (--state.armyClock > 0)
		return;
	state.armyClock = ArmyPulseTicks;
	state.armyPulses--;
	ArmyPulse(player, state);
}

void ClearNecromancerSummoningState()
{
	for (OwnerState &state : Owners)
		state = {};
}


std::string NecroSummoningFactsAt(const Player &player, SpellID spell, int rank)
{
	// The facts, from the same specs and helpers CastNecromancerSummoning runs, with this hero's masteries.
	const int r = std::max(rank, 1);
	std::string out;
	const auto line = [&out](const std::string &s) {
		if (!out.empty())
			out += '\n';
		out += s;
	};
	const auto percentLine = [&line](const char *format, int percent) { line(fmt::format(fmt::runtime(_(format)), percent)); };
	const auto resistLine = [&]() {
		const int resist = SummonResistPercent(Points(player, ClassTreeSkill::SummonResist));
		if (resist > 0)
			percentLine(N_("Resist fire, lightning and magic: {:d}%"), resist);
	};
	const auto body = [&](const MinionSpec &spec) {
		line(fmt::format(fmt::runtime(_("Life: {:d}")), spec.life));
		line(fmt::format(fmt::runtime(_("Damage: {:d} - {:d}")), spec.minDamage, spec.maxDamage));
		line(fmt::format(fmt::runtime(_("Armour: {:d}")), spec.armorClass));
		line(fmt::format(fmt::runtime(_("To hit: {:d}")), spec.toHit));
		resistLine();
	};
	const auto golem = [&](GolemKind kind) { body(GolemSpec(player, kind, r)); };
	switch (spell) {
	case SpellID::RaiseSkeleton:
		line(fmt::format(fmt::runtime(_("Skeletons: up to {:d}")), RaisedCountAtRank(r, MinionGroupCap(MinionGroup::Skeleton))));
		body(SkeletonSpec(player, r, /*mage=*/false));
		break;
	case SpellID::RaiseSkeletalMage:
		line(fmt::format(fmt::runtime(_("Mages: up to {:d}")), RaisedCountAtRank(r, MinionGroupCap(MinionGroup::Mage))));
		body(SkeletonSpec(player, r, /*mage=*/true));
		break;
	case SpellID::ClayGolem:
		golem(GolemKind::Clay);
		line(fmt::format(fmt::runtime(_("Its blows chill: {:s} s")), SecondsText(ClayGolemChillTicks)));
		break;
	case SpellID::BloodGolem:
		golem(GolemKind::Blood);
		percentLine(N_("Heals itself and you: {:d}% of damage dealt"), 100 / BloodGolemShareDivisor);
		break;
	case SpellID::IronGolem:
		golem(GolemKind::Iron);
		percentLine(N_("Returns: {:d}% of blows taken"), 100 / IronGolemReturnDivisor);
		break;
	case SpellID::FireGolem:
		golem(GolemKind::Fire);
		line(fmt::format(fmt::runtime(_("Burns what stands beside it: {:d}% of a blow every {:s} s")), 100 / FireGolemBurnDivisor, SecondsText(FireGolemPulseTicks)));
		percentLine(N_("Fire heals it: {:d}% of fire damage"), 100 / FireGolemFireHealDivisor);
		break;
	case SpellID::NecroRevive:
		line(fmt::format(fmt::runtime(_("Revived: up to {:d}")), RaisedCountAtRank(r, MinionGroupCap(MinionGroup::Revived))));
		percentLine(N_("Life: +{:d}% of its own"), ReviveLifePercent(r));
		line(fmt::format(fmt::runtime(_("Duration: {:s} s")), SecondsText(ReviveTicks(Points(player, ClassTreeSkill::LastingBond), PassiveActive(player, ClassTreeSkill::ExtendedServitude)))));
		line(std::string(_("Damage, armour and to hit: as in life")));
		resistLine();
		break;
	case SpellID::CommandTheDead:
		line(fmt::format(fmt::runtime(_("Duration: {:d} s")), CommandBaseSeconds + r));
		break;
	case SpellID::GatherTheDead:
		line(fmt::format(fmt::runtime(_("Calls every minion farther than {:d} tiles to your side")), GatherLeaveRadius));
		break;
	case SpellID::DarkMending:
		line(fmt::format(fmt::runtime(_("Radius: {:d} tiles")), DarkMendingRadius));
		percentLine(N_("Minion life healed: {:d}%"), DarkMendingPercent(r));
		break;
	case SpellID::FrenzyOfTheDead:
		line(fmt::format(fmt::runtime(_("Duration: {:d} s")), FrenzySeconds));
		percentLine(N_("Minion damage: +{:d}%"), FrenzyPercent(r));
		break;
	case SpellID::UnholyOffering:
		percentLine(N_("Heals you: {:d}% of the minion's life"), UnholyOfferingPercent(r));
		break;
	case SpellID::ArmyOfTheDead:
		line(fmt::format(fmt::runtime(_("Radius: {:d} tiles")), ArmyRadius));
		line(fmt::format(fmt::runtime(_("Magic damage: {:d} - {:d}")), ArmyDamageMin(r), ArmyDamageMin(r) + ArmyDamageSpread(r) - 1));
		line(fmt::format(fmt::runtime(_("Strikes: {:d}, every {:s} s")), ArmyPulses, SecondsText(ArmyPulseTicks)));
		break;
	default:
		break;
	}
	return out;
}

std::string NecroPassiveFactsAt(const Player &player, ClassTreeSkill skill, int points)
{
	(void)player; // every line reads @p points; the player's own investment is not the question here
	const int p = std::max(points, 1);
	switch (skill) {
	case ClassTreeSkill::SkeletonMastery:
		return fmt::format(fmt::runtime(_("Skeleton and mage life: +{:d}")), SkeletonMasteryLife * p) + '\n'
		    + fmt::format(fmt::runtime(_("Skeleton and mage damage: +{:d} - +{:d}")), SkeletonMasteryMinDamage * p, SkeletonMasteryMaxDamage * p) + '\n'
		    + fmt::format(fmt::runtime(_("Skeleton and mage to hit: +{:d}")), SkeletonMasteryToHit * p);
	case ClassTreeSkill::GolemMastery:
		return fmt::format(fmt::runtime(_("Golem life: +{:d}%")), GolemMasteryLifePercent * p) + '\n'
		    + fmt::format(fmt::runtime(_("Golem to hit: +{:d}")), GolemMasteryToHit * p);
	case ClassTreeSkill::BonePlating:
		return fmt::format(fmt::runtime(_("Skeleton, mage and golem armour: +{:d}")), BonePlatingArmour * p);
	case ClassTreeSkill::SummonResist:
		return fmt::format(fmt::runtime(_("Minions resist fire, lightning and magic: {:d}%")), SummonResistPercent(p));
	case ClassTreeSkill::LastingBond:
		return fmt::format(fmt::runtime(_("Revived duration: +{:d} s")), LastingBondSecondsPerPoint * p);
	case ClassTreeSkill::ExtendedServitude:
		return fmt::format(fmt::runtime(_("Revived duration: +{:d}%")), 100 / ExtendedServitudeDivisor);
	case ClassTreeSkill::GrislyTribute:
		return fmt::format(fmt::runtime(_("Heals you: {:d}% of the damage your minions deal")), 100 / GrislyTributeDivisor);
	case ClassTreeSkill::AberrantAnimator:
		return fmt::format(fmt::runtime(_("Minions return: {:d}% of blows taken")), 100 / AberrantAnimatorDivisor);
	default:
		return CursePassiveFactsAt(skill, points); // Curse Mastery, Essence Tap, Wide Malice, Eternal Torment
	}
}

} // namespace devilution::oracool
