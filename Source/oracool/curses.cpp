/**
 * @file oracool/curses.cpp
 *
 * See curses.h.
 */
#include "oracool/curses.h"

#include <algorithm>
#include <array>
#include <string>

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
#include "oracool/essence.h"
#include "oracool/minions.h"
#include "oracool/warcries.h"
#include "player.h"
#include "utils/language.h"

namespace devilution::oracool {

namespace {

constexpr int TicksPerSecond = 20;

struct Curse {
	CurseKind kind = CurseKind::None;
	int ticks = 0;
	int rank = 0;
	uint8_t owner = 0;
	/** Confuse set the berserk flags; they come off with the curse, and only if they were not there before. */
	bool turned = false;
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
	curse = {};
}

/** @brief Lays @p kind on @p monster for @p ticks at @p rank. False if Doom stands in the way. */
bool Lay(Monster &monster, CurseKind kind, int ticks, int rank, const Player &owner)
{
	Curse &curse = Of(monster);
	if (curse.kind == CurseKind::Doom && curse.ticks > 0 && kind != CurseKind::Doom)
		return false;
	Release(monster, curse);
	curse.kind = kind;
	curse.ticks = ticks;
	curse.rank = rank;
	curse.owner = owner.getId();
	switch (kind) {
	case CurseKind::Weaken:
		DebuffMonster(monster, ticks, -33, 0);
		break;
	case CurseKind::Decrepify:
		DebuffMonster(monster, ticks, -25, -20);
		ChillMonster(monster, ticks);
		break;
	case CurseKind::Confuse:
		if ((monster.flags & MFLAG_BERSERK) == 0 && !monster.isUnique()) {
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
void OwnerStrikes(Player &owner, Monster &monster, DamageType type, int damage)
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
		M_StartKill(monster, owner);
		return;
	}
	M_StartHit(monster, owner, damage);
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
			if (!Live(monster) || monster.position.tile.WalkingDistance(player.position.tile) > 6)
				continue;
			OwnerStrikes(player, monster, DamageType::Magic, (6 + 2 * r + GenerateRnd(6 + 2 * r)) << 6);
			torn++;
		}
		if (torn == 0) {
			player.Say(HeroSpeech::ICantDoThat);
			return false;
		}
		GainEssence(player, 5 * torn);
		return true;
	}
	const CurseKind kind = KindOf(spell);
	if (kind == CurseKind::None)
		return false;
	int ticks = (8 + r) * TicksPerSecond;
	ticks += ticks * 20 * Points(player, ClassTreeSkill::CurseMastery) / 100;
	const int radius = std::min(2 + Points(player, ClassTreeSkill::WideMalice), 5);
	const bool single = IsAnyOf(kind, CurseKind::Attract, CurseKind::DeathMark);
	int laid = 0;
	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		Monster &monster = Monsters[ActiveMonsters[i]];
		if (!Cursable(monster))
			continue;
		const int distance = monster.position.tile.WalkingDistance(target);
		if (single ? monster.position.tile != target : distance > radius)
			continue;
		if (kind == CurseKind::Terror && monster.isUnique())
			continue; // "Uniques do not" run - and are not marked either
		if (Lay(monster, kind, ticks, r, player))
			laid++;
	}
	if (laid == 0) {
		player.Say(HeroSpeech::ICantDoThat);
		return false;
	}
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
			damage += damage * std::min(50 + 5 * curse.rank, 100) / 100;
		break;
	case CurseKind::LowerResist:
		if (IsAnyOf(type, DamageType::Fire, DamageType::Lightning, DamageType::Magic, DamageType::Acid))
			damage += damage * std::min(25 + 3 * curse.rank, 70) / 100;
		break;
	case CurseKind::Decrepify:
		damage += damage * 20 / 100;
		break;
	case CurseKind::Doom:
		damage += damage * std::min(15 + 2 * curse.rank, 45) / 100;
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
	if (monster.isUnique())
		return false;
	return hitPoints <= maxHitPoints * std::min(10 + Of(monster).rank, 25) / 100;
}

void OnCursedMonsterDealtBlow(Monster &monster, int damage)
{
	if (!Live(monster) || Of(monster).kind != CurseKind::IronMaiden || damage <= 0)
		return;
	Player *owner = OwnerOf(Of(monster));
	if (owner == nullptr)
		return;
	OwnerStrikes(*owner, monster, DamageType::Physical, damage * (100 + 25 * Of(monster).rank) / 100);
}

void OnCursedMonsterStruck(const Monster &monster, Player &player, Monster *minion, int damage)
{
	if (!Live(monster) || Of(monster).kind != CurseKind::LifeTap || damage <= 0)
		return;
	const int share = damage * std::min(20 + 2 * Of(monster).rank, 50) / 100;
	if (minion != nullptr) {
		minion->hitPoints = std::min(minion->hitPoints + share, minion->maxHitPoints);
		return;
	}
	if (player._pHitPoints <= 0)
		return;
	player._pHitPoints = std::min(player._pHitPoints + share, player._pMaxHP);
	player._pHPBase = std::min(player._pHPBase + share, player._pMaxHPBase);
}

bool CursedMonsterBlinded(const Monster &monster)
{
	return Live(monster) && Of(monster).kind == CurseKind::DimVision;
}

bool CursedMonsterFlees(Monster &monster)
{
	if (!Live(monster) || Of(monster).kind != CurseKind::Terror || monster.isUnique())
		return false;
	if (monster.mode != MonsterMode::Stand)
		return false;
	Player *owner = OwnerOf(Of(monster));
	if (owner == nullptr)
		return false;
	// Far enough: it stands and shivers rather than walking off the map.
	if (monster.position.tile.WalkingDistance(owner->position.tile) > 9)
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
		if (distance > 8)
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
			GainEssence(*owner, 2 + 2 * tap);
		// Death Mark: the corpse bursts as a Corpse Explosion of the curse's rank.
		if (curse.kind == CurseKind::DeathMark) {
			const int share = std::clamp((monster.maxHitPoints >> 6) * std::min(40 + 5 * curse.rank, 100) / 100, 4, 400) << 6;
			for (size_t i = 0; i < ActiveMonsterCount; i++) {
				Monster &other = Monsters[ActiveMonsters[i]];
				if (&other == &monster || other.position.tile.WalkingDistance(monster.position.tile) > 2)
					continue;
				OwnerStrikes(*owner, other, DamageType::Physical, share);
			}
		}
	}
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
		if (curse.kind == CurseKind::Bane && curse.ticks % TicksPerSecond == 0)
			OwnerStrikes(player, monster, DamageType::Acid, (2 + curse.rank) << 6);
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

void DrawCurseMarker(const Surface &out, const Monster &monster, Point anchor)
{
	if (!Live(monster))
		return;
	// A lettered chip: the first letter of the curse on a dark plate, in the curse's hue. RfA-17's sigils replace it.
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

} // namespace devilution::oracool
