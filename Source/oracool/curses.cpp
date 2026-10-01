/**
 * @file oracool/curses.cpp
 *
 * See curses.h.
 */
#include "oracool/curses.h"
#include "oracool/rfa12_effects.h" // TakeCorpseOf: Death Mark uses the body up
#include "oracool/sat_math.h" // AddPercentSat - the damage passives past int (round 27 audit)

#include "oracool/endgame_boss.h" // FightsAsUnique - Diablo and the Dread bosses stand as uniques

#include <algorithm>
#include <array>
#include <string>

#include <fmt/format.h>

#include "DiabloUI/ui_flags.hpp"
#include "engine.h"
#include "engine/random.hpp"
#include "engine/rectangle.hpp"
#include "engine/render/primitive_render.hpp"
#include "engine/render/text_render.hpp"
#include "levels/gendung.h"
#include "monster.h"
#include "multi.h"
#include "oracool/chill.h"
#include "oracool/class_tree.h"
#include "missiles.h"
#include "oracool/essence.h"
#include "oracool/hud_art.h"
#include "oracool/minions.h"
#include "oracool/passives.h"
#include "oracool/rfa12_actives.h" // Rfa12SkillMarkers: the skill markers ride beside the curse's sigil
#include "oracool/skill_sounds.h"
#include "oracool/warcries.h"
#include "player.h"
#include "utils/language.h"

namespace devilution::oracool {

namespace {

constexpr int TicksPerSecond = 20;

// ---- the curses' numbers, one place each: the rules below and CurseFactsAt / CursePassiveFactsAt both read them ----

/** Duration 8 s + 1 s a rank; Curse Mastery adds a fifth of that per point. */
constexpr int CurseBaseSeconds = 8;
constexpr int CurseMasteryPercentPerPoint = 20;
/** Eternal Torment: a day of ticks - the curse ends with the monster (its slot is cleared with the body). */
constexpr int EternalTormentTicks = 24 * 60 * 60 * TicksPerSecond;
/** Area curses: radius 2 around the cursor, +1 a point of Wide Malice, 5 at most. */
constexpr int CurseBaseRadius = 2;
constexpr int CurseMaxRadius = 5;
constexpr int WeakenDamagePercent = -33;
constexpr int DecrepifyDamagePercent = -25;
constexpr int DecrepifyArmorPercent = -20;
constexpr int DecrepifyTakenPercent = 20;
/** Terror: past this distance from the hero the monster stands and shivers. */
constexpr int TerrorFleeTiles = 9;
/** Attract: monsters within this distance of the cursed one turn on it. */
constexpr int AttractReach = 8;
/** Death Mark: the burst reaches this far; the share is clamped to this many life points. */
constexpr int DeathMarkBurstRadius = 2;
constexpr int DeathMarkMinBurst = 4;
constexpr int DeathMarkMaxBurst = 400;
/** Soul Harvest: every cursed monster within this distance of the hero; Essence per monster torn. */
constexpr int SoulHarvestRadius = 6;
constexpr int SoulHarvestEssence = 5;

int CurseTicksAt(int rank, int masteryPoints)
{
	int ticks = (CurseBaseSeconds + rank) * TicksPerSecond;
	ticks += ticks * CurseMasteryPercentPerPoint * masteryPoints / 100;
	return ticks;
}

int CurseRadiusAt(int wideMalicePoints)
{
	return std::min(CurseBaseRadius + wideMalicePoints, CurseMaxRadius);
}

int AmplifyPercent(int rank) { return std::min(50 + 5 * rank, 100); }
int LowerResistPercent(int rank) { return std::min(25 + 3 * rank, 70); }
int DoomPercent(int rank) { return std::min(15 + 2 * rank, 45); }
int FrailtyPercent(int rank) { return std::min(10 + rank, 25); }
int IronMaidenPercent(int rank) { return 100 + 25 * rank; }
int LifeTapPercent(int rank) { return std::min(20 + 2 * rank, 50); }
/** Bane: acid (the engine's poison) once a second, in whole points. */
int BanePerSecond(int rank) { return 2 + rank; }
/** Soul Harvest: magic damage min + GenerateRnd(spread), in whole points. */
int SoulHarvestMin(int rank) { return 6 + 2 * rank; }
int SoulHarvestSpread(int rank) { return 6 + 2 * rank; }
int EssenceTapEssence(int points) { return 2 + 2 * points; }

/** @brief @p ticks as seconds: "12", or "12.5" when it does not come out whole. */
std::string SecondsText(int ticks)
{
	if (ticks % TicksPerSecond == 0)
		return fmt::format("{:d}", ticks / TicksPerSecond);
	return fmt::format("{:.1f}", static_cast<double>(ticks) / TicksPerSecond);
}

struct Curse {
	CurseKind kind = CurseKind::None;
	int ticks = 0;
	int rank = 0;
	uint8_t owner = 0;
	/** Confuse set the berserk flags; they come off with the curse, and only if they were not there before. */
	bool turned = false;
	/** Bane's own second, kept when Bane is laid again (round 37 audit: a recast within a second pushed its strike back). */
	int pulse = 0;
};

std::array<Curse, MaxMonsters> Curses;

Curse &Of(const Monster &monster)
{
	return Curses[monster.getId()];
}

bool Live(const Monster &monster)
{
	return Of(monster).kind != CurseKind::None && Of(monster).ticks > 0 && (monster.hitPoints >> 6) > 0;
}

int Points(const Player &player, ClassTreeSkill skill)
{
	if (!IsClassTreeSkillUnlocked(player, skill))
		return 0;
	return ClassTreeInvestment(player, skill);
}

CurseKind KindOf(SpellID spell)
{
	switch (spell) {
	case SpellID::AmplifyDamage:
		return CurseKind::AmplifyDamage;
	case SpellID::DimVision:
		return CurseKind::DimVision;
	case SpellID::NecroWeaken:
		return CurseKind::Weaken;
	case SpellID::Frailty:
		return CurseKind::Frailty;
	case SpellID::NecroIronMaiden:
		return CurseKind::IronMaiden;
	case SpellID::Terror:
		return CurseKind::Terror;
	case SpellID::Bane:
		return CurseKind::Bane;
	case SpellID::Confuse:
		return CurseKind::Confuse;
	case SpellID::LifeTap:
		return CurseKind::LifeTap;
	case SpellID::Attract:
		return CurseKind::Attract;
	case SpellID::Decrepify:
		return CurseKind::Decrepify;
	case SpellID::DeathMark:
		return CurseKind::DeathMark;
	case SpellID::LowerResist:
		return CurseKind::LowerResist;
	case SpellID::Doom:
		return CurseKind::Doom;
	default:
		return CurseKind::None;
	}
}

bool Cursable(const Monster &monster)
{
	return (monster.hitPoints >> 6) > 0 && !monster.isPlayerMinion() && monster.isPossibleToHit() && monster.mode != MonsterMode::Death;
}

void Release(const Monster &monster, Curse &curse)
{
	if (curse.turned)
		const_cast<Monster &>(monster).flags &= ~(MFLAG_BERSERK | MFLAG_GOLEM);
	// Weaken and Decrepify are laid through the warcry debuff and the chill with the curse's own
	// clock; releasing the curse (a replacement, or its end) has to take those back too, or an
	// Eternal Torment Decrepify replaced by Doom leaves the monster slowed for good (audit, 2026-09-19).
	if (curse.kind == CurseKind::Weaken || curse.kind == CurseKind::Decrepify)
		ClearWarcryStateForMonster(monster);
	if (curse.kind == CurseKind::Decrepify)
		ClearChillUpTo(monster, curse.ticks); // its own chill, not a longer one laid over it (round 62 audit)
	curse = {};
}

/** @brief Lays @p kind on @p monster for @p ticks at @p rank. False if Doom stands in the way. */
bool Lay(Monster &monster, CurseKind kind, int ticks, int rank, const Player &owner)
{
	Curse &curse = Of(monster);
	if (curse.kind == CurseKind::Doom && curse.ticks > 0 && kind != CurseKind::Doom)
		return false;
	const int banePulse = curse.kind == CurseKind::Bane && curse.ticks > 0 ? curse.pulse : 0;
	Release(monster, curse);
	if (kind == CurseKind::Bane)
		curse.pulse = banePulse;
	// The curse's owner is the one who hit it: a Confused or Attracted pack's kills are his (round 37 audit).
	monster.tag(owner);
	curse.kind = kind;
	curse.ticks = ticks;
	curse.rank = rank;
	curse.owner = owner.getId();
	switch (kind) {
	case CurseKind::Weaken:
		DebuffMonster(monster, ticks, WeakenDamagePercent, 0);
		break;
	case CurseKind::Decrepify:
		DebuffMonster(monster, ticks, DecrepifyDamagePercent, DecrepifyArmorPercent);
		ChillMonster(monster, ticks);
		break;
	case CurseKind::Confuse:
		if ((monster.flags & MFLAG_BERSERK) == 0 && !FightsAsUnique(monster)) {
			monster.flags |= MFLAG_BERSERK | MFLAG_GOLEM;
			curse.turned = true;
		}
		break;
	default:
		break;
	}
	return true;
}

/** @brief A blow credited to the curse's owner, immunity and resistance honoured. */
/** @brief Strikes @p monster for @p owner; whether any damage landed (Soul Harvest pays only for those). */
bool OwnerStrikes(Player &owner, Monster &monster, DamageType type, int damage, bool applyPassives = true)
{
	if (damage <= 0 || (monster.hitPoints >> 6) <= 0 || monster.isPlayerMinion() || !monster.isPossibleToHit())
		return false;
	if (monster.isImmune(MissileID::Null, type))
		return false;
	if (monster.isResistant(MissileID::Null, type))
		damage >>= 2;
	// The damage-dealt passives for a fresh blow of the hero's (Soul Harvest, Death Mark, Bane) - Spreading Malediction
	// never reached them. Not for Iron Maiden, which returns the monster's own blow (round 15 audit, v1.12.240).
	if (applyPassives)
		damage += damage * PassiveDamageDealtPercent(owner, monster, /*melee=*/false) / 100;
	if (damage <= 0)
		return false;
	if (applyPassives)
		OnCursedMonsterStruck(monster, owner, nullptr, damage); // Life Tap on Soul Harvest and Death Mark (round 26 audit)
	ApplyMonsterDamage(type, monster, damage);
	if ((monster.hitPoints >> 6) <= 0) {
		M_StartKill(monster, owner);
		return true;
	}
	M_StartHit(monster, owner, damage);
	return true;
}

Player *OwnerOf(const Curse &curse)
{
	if (curse.owner >= Players.size())
		return nullptr;
	return &Players[curse.owner];
}

} // namespace

bool IsNecromancerCurse(SpellID spell)
{
	return KindOf(spell) != CurseKind::None || spell == SpellID::SoulHarvest;
}

CurseKind CurseOn(const Monster &monster)
{
	return Live(monster) ? Of(monster).kind : CurseKind::None;
}

int CursedMonstersNear(Point centre, int radius)
{
	int count = 0;
	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		const Monster &monster = Monsters[ActiveMonsters[i]];
		if (Live(monster) && centre.WalkingDistance(monster.position.tile) <= radius)
			count++;
	}
	return count;
}

const char *CurseName(CurseKind kind)
{
	switch (kind) {
	case CurseKind::AmplifyDamage:
		return N_("Amplify Damage");
	case CurseKind::DimVision:
		return N_("Dim Vision");
	case CurseKind::Weaken:
		return N_("Weaken");
	case CurseKind::Frailty:
		return N_("Frailty");
	case CurseKind::IronMaiden:
		return N_("Iron Maiden");
	case CurseKind::Terror:
		return N_("Terror");
	case CurseKind::Bane:
		return N_("Bane");
	case CurseKind::Confuse:
		return N_("Confuse");
	case CurseKind::LifeTap:
		return N_("Life Tap");
	case CurseKind::Attract:
		return N_("Attract");
	case CurseKind::Decrepify:
		return N_("Decrepify");
	case CurseKind::DeathMark:
		return N_("Death Mark");
	case CurseKind::LowerResist:
		return N_("Lower Resist");
	case CurseKind::Doom:
		return N_("Doom");
	case CurseKind::None:
		break;
	}
	return "";
}

bool CastNecromancerCurse(Player &player, SpellID spell, Point target, int rank)
{
	const int r = std::max(rank, 1);
	if (spell == SpellID::SoulHarvest) {
		// Every cursed monster within six: magic damage to each, Essence to the hero for each.
		int torn = 0;
		for (size_t i = 0; i < ActiveMonsterCount; i++) {
			Monster &monster = Monsters[ActiveMonsters[i]];
			if (!Live(monster) || monster.position.tile.WalkingDistance(player.position.tile) > SoulHarvestRadius
			    || !LineClearMissile(player.position.tile, monster.position.tile)) // in sight (round 8 audit)
				continue;
			const Point from = monster.position.tile;
			// Paid only for a soul actually torn: a magic-immune monster took nothing and paid 5 Essence (round 14 audit).
			if (!OwnerStrikes(player, monster, DamageType::Magic, (SoulHarvestMin(r) + GenerateRnd(SoulHarvestSpread(r))) << 6))
				continue;
			torn++;
			// RfA-27: the soul torn loose - a wisp flying from it to him (batch 54), landing with the harvest's impact cue
			// (batch 51), which the brief has play once for each cursed monster. At once while the wisp's sheet is missing.
			const ClassTreeSkill cue = &player == MyPlayer ? ClassTreeSkill::SoulHarvest : ClassTreeSkill::None;
			if (AddArtBolt(from, player.position.tile, MissileGraphicID::SoulWisp, static_cast<int>(player.getId()), 16, MissileGraphicID::None, cue) == nullptr
			    && cue != ClassTreeSkill::None)
				PlaySkillSound(cue, SkillSoundEvent::Impact);
		}
		if (torn == 0) {
			player.Say(HeroSpeech::ICantDoThat);
			return false;
		}
		GainEssence(player, SoulHarvestEssence * torn);
		return true;
	}
	const CurseKind kind = KindOf(spell);
	if (kind == CurseKind::None)
		return false;
	int ticks = CurseTicksAt(r, Points(player, ClassTreeSkill::CurseMastery));
	// Eternal Torment: it ends when the monster does. (A day of ticks; the slot is cleared with the body.)
	if (PassiveActive(player, ClassTreeSkill::EternalTorment))
		ticks = EternalTormentTicks;
	const int radius = CurseRadiusAt(Points(player, ClassTreeSkill::WideMalice));
	const bool single = IsAnyOf(kind, CurseKind::Attract, CurseKind::DeathMark);
	int laid = 0;
	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		Monster &monster = Monsters[ActiveMonsters[i]];
		if (!Cursable(monster))
			continue;
		const int distance = monster.position.tile.WalkingDistance(target);
		// A single curse finds its monster on either end of a step (audit, 2026-09-29): the cast aims at the tile a walking
		// monster is stepping into, while its own tile stays the one it left until the step ends.
		const bool onTarget = monster.position.tile == target || monster.position.future == target;
		if (single ? !onTarget : distance > radius)
			continue;
		if (kind == CurseKind::Terror && FightsAsUnique(monster))
			continue; // "Uniques do not" run - and are not marked either
		// Nor Confuse or Frailty, which do nothing to a unique: laid anyway, they cost Essence and replaced the curse it had
		// (round 8 audit, v1.12.233).
		// Nor Dim Vision: a blinded boss loses the hero at two tiles, and never fights back (round 21 audit).
		if (IsAnyOf(kind, CurseKind::Confuse, CurseKind::Frailty, CurseKind::DimVision) && FightsAsUnique(monster))
			continue;
		if (kind == CurseKind::Bane && monster.isImmune(MissileID::Null, DamageType::Acid))
			continue; // it rots nothing there (round 41 audit)
		// In sight of the cast, as every area skill since round 5 (round 8 audit).
		if (!single && !LineClearMissile(target, monster.position.tile))
			continue;
		// And of the Necromancer, single curses too (round 37 audit: a cursor on the room behind a wall cursed its pack).
		if (!LineClearMissile(player.position.tile, monster.position.tile))
			continue;
		if (Lay(monster, kind, ticks, r, player))
			laid++;
	}
	if (laid == 0) {
		player.Say(HeroSpeech::ICantDoThat);
		return false;
	}
	// The script ring on the floor (batch 38); it removes itself while the sheet is not in the archive.
	AddMissile(target, target, player._pdir, MissileID::CurseCastEffect, TARGET_MONSTERS, static_cast<int>(player.getId()), 0, 0);
	return true;
}

// ---- the seams ----------------------------------------------------------------------------------------------------

int CurseDamageTaken(const Monster &monster, DamageType type, int damage)
{
	if (!Live(monster) || damage <= 0)
		return damage;
	const Curse &curse = Of(monster);
	switch (curse.kind) {
	case CurseKind::AmplifyDamage:
		if (type == DamageType::Physical)
			damage = AddPercentSat(damage, AmplifyPercent(curse.rank)); // saturating, as every curse here (round 27 audit)
		break;
	case CurseKind::LowerResist:
		if (IsAnyOf(type, DamageType::Fire, DamageType::Lightning, DamageType::Magic, DamageType::Acid))
			damage = AddPercentSat(damage, LowerResistPercent(curse.rank));
		break;
	case CurseKind::Decrepify:
		damage = AddPercentSat(damage, DecrepifyTakenPercent);
		break;
	case CurseKind::Doom:
		damage = AddPercentSat(damage, DoomPercent(curse.rank));
		break;
	default:
		break;
	}
	return damage;
}

bool CurseFinishes(const Monster &monster, int hitPoints, int maxHitPoints)
{
	if (!Live(monster) || Of(monster).kind != CurseKind::Frailty || hitPoints <= 0)
		return false;
	if (FightsAsUnique(monster)) // Diablo and the Dread bosses too (round 19 audit)
		return false;
	return hitPoints <= maxHitPoints * FrailtyPercent(Of(monster).rank) / 100;
}

void OnCursedMonsterDealtBlow(Monster &monster, int damage)
{
	if (!Live(monster) || Of(monster).kind != CurseKind::IronMaiden || damage <= 0)
		return;
	Player *owner = OwnerOf(Of(monster));
	if (owner == nullptr)
		return;
	OwnerStrikes(*owner, monster, DamageType::Physical, damage * IronMaidenPercent(Of(monster).rank) / 100, /*applyPassives=*/false);
}

void OnCursedMonsterStruck(const Monster &monster, Player &player, Monster *minion, int damage)
{
	// The curse, not the life (round 41 audit): the killing blow landed after the hit points reached 0 on most paths, and
	// Life Tap healed nothing for it. ProcessCursesTick releases it the next tick.
	if (Of(monster).kind != CurseKind::LifeTap || Of(monster).ticks <= 0 || damage <= 0)
		return;
	// In 64 bits: a huge hit's share wrapped negative and drained the Necromancer it was meant to heal (round 27 audit).
	const int64_t share = PercentOfSat(damage, LifeTapPercent(Of(monster).rank));
	if (minion != nullptr) {
		minion->hitPoints = static_cast<int>(std::min<int64_t>(minion->hitPoints + share, minion->maxHitPoints));
		return;
	}
	if (player._pHitPoints <= 0)
		return;
	player._pHitPoints = static_cast<int>(std::min<int64_t>(player._pHitPoints + share, player._pMaxHP));
	player._pHPBase = static_cast<int>(std::min<int64_t>(player._pHPBase + share, player._pMaxHPBase));
}

bool CursedMonsterBlinded(const Monster &monster)
{
	return Live(monster) && Of(monster).kind == CurseKind::DimVision;
}

bool CursedMonsterFlees(Monster &monster)
{
	if (!Live(monster) || Of(monster).kind != CurseKind::Terror || FightsAsUnique(monster))
		return false;
	if (monster.mode != MonsterMode::Stand)
		return false;
	// Not a hidden or fading one: its own AI must fade it back in (round 40 audit: a faded Counselor or Unseen stayed
	// hidden and unhittable for the whole curse - a day under Eternal Torment).
	if ((monster.flags & MFLAG_HIDDEN) != 0)
		return false;
	Player *owner = OwnerOf(Of(monster));
	if (owner == nullptr)
		return false;
	// Far enough: it stands and shivers rather than walking off the map.
	if (monster.position.tile.WalkingDistance(owner->position.tile) > TerrorFleeTiles)
		return true;
	return MonsterStepAwayFrom(monster, owner->position.tile);
}

int CurseLureTarget(const Monster &monster)
{
	int best = -1;
	int bestDistance = 0;
	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		const Monster &other = Monsters[ActiveMonsters[i]];
		if (&other == &monster || !Live(other) || Of(other).kind != CurseKind::Attract)
			continue;
		const int distance = monster.position.tile.WalkingDistance(other.position.tile);
		if (distance > AttractReach)
			continue;
		if (best < 0 || distance < bestDistance) {
			best = static_cast<int>(other.getId());
			bestDistance = distance;
		}
	}
	return best;
}

void OnCursedMonsterDeath(const Monster &monster)
{
	Curse &curse = Of(monster);
	if (curse.kind == CurseKind::None || curse.ticks <= 0)
		return;
	Player *owner = OwnerOf(curse);
	if (owner != nullptr) {
		// Essence Tap: a cursed death returns Essence.
		if (const int tap = Points(*owner, ClassTreeSkill::EssenceTap); tap > 0)
			GainEssence(*owner, EssenceTapEssence(tap));
		// Death Mark: the corpse bursts as a Corpse Explosion of the curse's rank.
		if (curse.kind == CurseKind::DeathMark) {
			// The body is what bursts: none is left to raise or burst again (user, 2026-10-01).
			TakeCorpseOf(monster);
			AddMissile(monster.position.tile, monster.position.tile, Direction::South, MissileID::CorpseBurst, TARGET_MONSTERS, static_cast<int>(owner->getId()), 0, 0);
			const int share = std::clamp((monster.maxHitPoints >> 6) * CorpseBurstPercent(curse.rank) / 100, DeathMarkMinBurst, DeathMarkMaxBurst) << 6;
			for (size_t i = 0; i < ActiveMonsterCount; i++) {
				Monster &other = Monsters[ActiveMonsters[i]];
				if (&other == &monster || other.position.tile.WalkingDistance(monster.position.tile) > DeathMarkBurstRadius
				    || !LineClearMissile(monster.position.tile, other.position.tile)) // the burst stops at walls (round 8 audit)
					continue;
				OwnerStrikes(*owner, other, DamageType::Physical, share);
			}
		}
	}
	// Not Life Tap yet: the killing blow's heal reads it after this, and it has nothing to undo (round 41 audit).
	if (curse.kind != CurseKind::LifeTap)
		Release(monster, curse);
}

void ProcessCursesTick(Player &player)
{
	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		Monster &monster = Monsters[ActiveMonsters[i]];
		Curse &curse = Of(monster);
		if (curse.kind == CurseKind::None || curse.owner != player.getId())
			continue;
		if (curse.ticks <= 0 || (monster.hitPoints >> 6) <= 0) {
			Release(monster, curse);
			continue;
		}
		curse.ticks--;
		if (curse.kind == CurseKind::Bane && (++curse.pulse >= TicksPerSecond || curse.ticks == 0)) {
			curse.pulse = 0; // its own second (round 37 audit)
			OwnerStrikes(player, monster, DamageType::Acid, BanePerSecond(curse.rank) << 6);
		}
		if (curse.ticks == 0)
			Release(monster, curse);
	}
}

void ClearCurseForMonster(const Monster &monster)
{
	Curses[monster.getId()] = {};
}

void ClearAllCurses()
{
	for (Curse &curse : Curses)
		curse = {};
}

void ReleaseConfusedForLevelSave()
{
	for (size_t i = 0; i < Curses.size(); i++) {
		if (!Curses[i].turned)
			continue;
		Monsters[i].flags &= ~(MFLAG_BERSERK | MFLAG_GOLEM);
		Curses[i].turned = false;
	}
}

namespace {

/** @brief The curse's own sigil, centred on @p anchor.x with its foot 4px above @p anchor.y - or its lettered chip. */
void DrawCurseSigil(const Surface &out, const Monster &monster, Point anchor)
{
	// RfA-17 batch 38's sigils (2026-09-18), 24x24, in CurseKind order; the lettered chip below stands in without the strip.
	if (DrawCurseMarkerIcon(out, { anchor.x - 12, anchor.y - 28 }, static_cast<int>(Of(monster).kind) - 1))
		return;
	// A lettered chip: the first letter of the curse on a dark plate, in the curse's hue.
	static const char *Letters = "?ADWFITBCLADMRO"; // in CurseKind order, None first
	const char letter[2] = { Letters[static_cast<size_t>(Of(monster).kind)], '\0' };
	UiFlags colour = UiFlags::ColorWhite;
	switch (Of(monster).kind) {
	case CurseKind::AmplifyDamage:
	case CurseKind::IronMaiden:
	case CurseKind::LifeTap:
		colour = UiFlags::ColorRed;
		break;
	case CurseKind::Bane:
	case CurseKind::LowerResist:
		colour = UiFlags::ColorOracoolGreen;
		break;
	case CurseKind::Attract:
	case CurseKind::DeathMark:
	case CurseKind::Doom:
		colour = UiFlags::ColorGold;
		break;
	case CurseKind::Confuse:
	case CurseKind::Terror:
		colour = UiFlags::ColorOrange;
		break;
	default:
		colour = UiFlags::ColorBlue;
		break;
	}
	const Rectangle plate { { anchor.x - 6, anchor.y - 14 }, { 12, 12 } };
	if (plate.position.x < 0 || plate.position.y < 0 || plate.position.x + plate.size.width >= out.w() || plate.position.y + plate.size.height >= out.h())
		return;
	FillRect(out, plate.position.x, plate.position.y, plate.size.width, plate.size.height, 0);
	DrawString(out, letter, plate, { colour | UiFlags::FontSize12 | UiFlags::AlignCenter | UiFlags::VerticalCenter });
}

} // namespace

void DrawCurseMarker(const Surface &out, const Monster &monster, Point anchor)
{
	// RfA-27 batch 58 (2026-09-26): the skill markers - Judged, Hunted, Bleeding and the rest - share the curse's row. One
	// line of 24px sigils centred over the head, the curse first, so neither covers the other. A skill marker takes no
	// place while ui/skill_markers.png is not in the archive.
	constexpr int Cell = 24;
	constexpr int Gap = 2;
	const bool cursed = Live(monster);
	uint16_t marks = Rfa12SkillMarkers(monster);
	if (marks != 0 && !HasSkillMarkerArt())
		marks = 0;
	int count = cursed ? 1 : 0;
	for (int i = 0; i < SkillMarkerCount; i++) {
		if ((marks & (1U << i)) != 0)
			count++;
	}
	if (count == 0)
		return;
	int left = anchor.x - (count * Cell + (count - 1) * Gap) / 2;
	if (cursed) {
		DrawCurseSigil(out, monster, { left + Cell / 2, anchor.y });
		left += Cell + Gap;
	}
	for (int i = 0; i < SkillMarkerCount; i++) {
		if ((marks & (1U << i)) == 0)
			continue;
		DrawSkillMarkerIcon(out, { left, anchor.y - 28 }, i);
		left += Cell + Gap;
	}
}


int CorpseBurstPercent(int rank)
{
	return std::min(40 + 5 * rank, 100);
}

std::string CurseFactsAt(const Player &player, SpellID spell, int rank)
{
	// The facts, from the same helpers CastNecromancerCurse and the seams above run.
	const int r = std::max(rank, 1);
	std::string out;
	const auto line = [&out](const std::string &s) {
		if (!out.empty())
			out += '\n';
		out += s;
	};
	const auto percentLine = [&line](const char *format, int percent) { line(fmt::format(fmt::runtime(_(format)), percent)); };
	if (spell == SpellID::SoulHarvest) {
		line(fmt::format(fmt::runtime(_("Radius: {:d} tiles")), SoulHarvestRadius));
		line(fmt::format(fmt::runtime(_("Magic damage: {:d} - {:d} to each cursed monster")), SoulHarvestMin(r), SoulHarvestMin(r) + SoulHarvestSpread(r) - 1));
		line(fmt::format(fmt::runtime(_("Essence: +{:d} for each")), SoulHarvestEssence));
		return out;
	}
	const CurseKind kind = KindOf(spell);
	if (kind == CurseKind::None)
		return {};
	line(fmt::format(fmt::runtime(_("Duration: {:s} s")), SecondsText(CurseTicksAt(r, Points(player, ClassTreeSkill::CurseMastery)))));
	if (PassiveActive(player, ClassTreeSkill::EternalTorment))
		line(std::string(_("Eternal Torment: lasts until the monster dies")));
	if (IsAnyOf(kind, CurseKind::Attract, CurseKind::DeathMark))
		line(std::string(_("Target: one monster")));
	else
		line(fmt::format(fmt::runtime(_("Radius: {:d} tiles")), CurseRadiusAt(Points(player, ClassTreeSkill::WideMalice))));
	switch (kind) {
	case CurseKind::AmplifyDamage:
		percentLine(N_("Physical damage taken: +{:d}%"), AmplifyPercent(r));
		break;
	case CurseKind::DimVision:
		line(fmt::format(fmt::runtime(_("Sees you only within: {:d} tile")), DimVisionSightTiles));
		break;
	case CurseKind::Weaken:
		percentLine(N_("Enemy damage: {:d}%"), WeakenDamagePercent);
		break;
	case CurseKind::Frailty:
		percentLine(N_("Dies below: {:d}% life"), FrailtyPercent(r));
		line(std::string(_("Uniques are not affected")));
		break;
	case CurseKind::IronMaiden:
		percentLine(N_("Returns: {:d}% of its blows"), IronMaidenPercent(r));
		break;
	case CurseKind::Terror:
		line(fmt::format(fmt::runtime(_("Flees up to: {:d} tiles")), TerrorFleeTiles));
		line(std::string(_("Uniques are not affected")));
		break;
	case CurseKind::Bane:
		line(fmt::format(fmt::runtime(_("Poison damage: {:d} a second")), BanePerSecond(r)));
		break;
	case CurseKind::Confuse:
		line(std::string(_("Attacks the nearest creature, friend or foe")));
		line(std::string(_("Uniques are not affected")));
		break;
	case CurseKind::LifeTap:
		percentLine(N_("Heals the striker: {:d}% of damage dealt"), LifeTapPercent(r));
		break;
	case CurseKind::Attract:
		line(fmt::format(fmt::runtime(_("Draws monsters within: {:d} tiles")), AttractReach));
		break;
	case CurseKind::Decrepify:
		percentLine(N_("Enemy damage: {:d}%"), DecrepifyDamagePercent);
		percentLine(N_("Enemy armour: {:d}%"), DecrepifyArmorPercent);
		percentLine(N_("Damage taken: +{:d}%"), DecrepifyTakenPercent);
		line(std::string(_("Slowed to half speed")));
		break;
	case CurseKind::DeathMark:
		line(fmt::format(fmt::runtime(_("Bursts on death: {:d}% of its life within {:d} tiles")), CorpseBurstPercent(r), DeathMarkBurstRadius));
		break;
	case CurseKind::LowerResist:
		percentLine(N_("Fire, lightning, magic and poison damage taken: +{:d}%"), LowerResistPercent(r));
		break;
	case CurseKind::Doom:
		percentLine(N_("Damage taken: +{:d}%"), DoomPercent(r));
		line(std::string(_("No other curse can replace it")));
		break;
	case CurseKind::None:
		break;
	}
	return out;
}

std::string CursePassiveFactsAt(ClassTreeSkill skill, int points)
{
	const int p = std::max(points, 1);
	switch (skill) {
	case ClassTreeSkill::CurseMastery:
		return fmt::format(fmt::runtime(_("Curse duration: +{:d}%")), CurseMasteryPercentPerPoint * p);
	case ClassTreeSkill::EssenceTap:
		return fmt::format(fmt::runtime(_("Essence per cursed death: {:d}")), EssenceTapEssence(p));
	case ClassTreeSkill::WideMalice:
		return fmt::format(fmt::runtime(_("Curse radius: {:d} tiles")), CurseRadiusAt(p));
	case ClassTreeSkill::EternalTorment:
		return std::string(_("Curse duration: until the monster dies"));
	default:
		return {};
	}
}

} // namespace devilution::oracool
