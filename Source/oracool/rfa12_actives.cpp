#include "oracool/rfa12_actives.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <vector>

#include "automap.h"
#include "dead.h"
#include "engine/backbuffer_state.hpp"
#include "engine/random.hpp"
#include "items.h"
#include "levels/gendung.h"
#include "missiles.h"
#include "monster.h"
#include "oracool/aura_field.h"
#include "oracool/chill.h"
#include "oracool/class_tree.h"
#include "oracool/companion.h"
#include "oracool/passives.h"
#include "oracool/necro_summoning.h"
#include "oracool/rage.h"
#include "oracool/rfa12_effects.h"
#include "oracool/stat_sheet.h"
#include "oracool/warcries.h"
#include "player.h"
#include "spells.h"

namespace devilution::oracool {

namespace {

constexpr int TicksPerSecond = 20;

// =================================================================================================
// Helpers
// =================================================================================================

int RankOf(const Player &player, SpellID spell)
{
	return std::max(player.GetSpellLevel(spell), 1);
}

/** @brief A roll across [min, max] whole points, in the 1/64 units monster life is kept in. */
int Roll(int min, int max)
{
	return (min + GenerateRnd(std::max(max - min, 0) + 1)) << 6;
}

/** @brief One blow of the weapon in hand, as the character sheet rolls it - bonuses included, no to-hit. */
int WeaponBlow(const Player &player)
{
	int dam = player._pIMinDam + GenerateRnd(std::max(player._pIMaxDam - player._pIMinDam, 0) + 1);
	dam += dam * player._pIBonusDam / 100;
	dam += player._pIBonusDamMod + player._pDamageMod;
	return std::max(dam, 1) << 6;
}

/** @brief Earthen Might (Barbarian, 2026-09-14): 3 Rage for every enemy the ground-splitters strike. */
void EarthenMightRage(Player &player, size_t struck)
{
	if (struck == 0 || !PassiveActive(player, ClassTreeSkill::EarthenMight))
		return;
	NoteRageCombat(player);
	GainRage(player, 3 * static_cast<int>(struck));
}

bool Hittable(const Monster &monster)
{
	return (monster.hitPoints >> 6) > 0 && !monster.isPlayerMinion() && monster.isPossibleToHit();
}

/** @brief "Uniques shrug it off" - the exemption every stagger in this fork carries. */
bool ShrugsOff(const Monster &monster)
{
	return monster.isUnique() || monster.lesserAffix != LesserUniqueAffix::None || monster.type().type == MT_DIABLO;
}

int FrostbitePercentOn(const Monster &monster);

/** @brief A skill's strike: immunity and resistance honoured, kill credit and the flinch to @p player. */
void Strike(Player &player, Monster &monster, DamageType type, int damage)
{
	if (damage <= 0 || !Hittable(monster))
		return;
	if (monster.isImmune(MissileID::Null, type))
		return;
	if (monster.isResistant(MissileID::Null, type))
		damage >>= 2;
	if (type == DamageType::Cold)
		damage += damage * FrostbitePercentOn(monster) / 100;
	if (damage <= 0)
		return;
	ApplyMonsterDamage(type, monster, damage);
	if ((monster.hitPoints >> 6) <= 0)
		M_StartKill(monster, player);
	else
		M_StartHit(monster, player, damage);
}

void Stagger(Monster &monster, int ticks)
{
	if (ShrugsOff(monster) || monster.mode == MonsterMode::Petrified || (monster.hitPoints >> 6) <= 0)
		return;
	StunMonster(monster, ticks);
}

/** @brief Moves @p monster one tile in @p dir, through the engine's knockback (so the immune stay put). */
void Shove(Monster &monster, Direction dir)
{
	if (ShrugsOff(monster) || monster.mode == MonsterMode::Petrified || (monster.hitPoints >> 6) <= 0)
		return;
	// M_GetKnockback moves a monster OPPOSITE to where it faces; face it the other way first.
	monster.direction = Opposite(dir);
	M_GetKnockback(monster);
}

/** @brief Every hittable monster within @p radius tiles of @p centre, gathered before anything is struck. */
std::vector<Monster *> MonstersWithin(Point centre, int radius)
{
	std::vector<Monster *> out;
	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		Monster &monster = Monsters[ActiveMonsters[i]];
		if (Hittable(monster) && centre.WalkingDistance(monster.position.tile) <= radius)
			out.push_back(&monster);
	}
	return out;
}

/** @brief The hittable monster nearest @p centre within @p radius, or null. */
Monster *NearestTo(Point centre, int radius, const Monster *except = nullptr)
{
	Monster *best = nullptr;
	int bestDistance = radius + 1;
	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		Monster &monster = Monsters[ActiveMonsters[i]];
		if (&monster == except || !Hittable(monster))
			continue;
		const int distance = centre.WalkingDistance(monster.position.tile);
		if (distance < bestDistance) {
			bestDistance = distance;
			best = &monster;
		}
	}
	return best;
}

/** @brief The tiles from @p from toward @p toward, @p length of them, stopping at a wall. */
std::vector<Point> LineOfTiles(Point from, Point toward, int length)
{
	std::vector<Point> tiles;
	const Direction dir = toward == from ? Direction::South : GetDirection(from, toward);
	Point tile = from;
	for (int i = 0; i < length; i++) {
		tile = tile + dir;
		if (!InDungeonBounds(tile) || IsTileSolid(tile))
			break;
		tiles.push_back(tile);
	}
	return tiles;
}

/** @brief Every hittable monster standing on a tile of the line, nearest first. */
std::vector<Monster *> MonstersOnLine(Point from, Point toward, int length)
{
	std::vector<Monster *> out;
	for (const Point tile : LineOfTiles(from, toward, length)) {
		Monster *monster = FindMonsterAtPosition(tile);
		if (monster != nullptr && Hittable(*monster) && std::find(out.begin(), out.end(), monster) == out.end())
			out.push_back(monster);
	}
	return out;
}

/** @brief The tile ahead of @p player toward @p target, and the two beside it. */
std::array<Point, 3> FrontArc(const Player &player, Point target)
{
	const Direction dir = target == player.position.tile ? player._pdir : GetDirection(player.position.tile, target);
	return { player.position.tile + dir, player.position.tile + Left(dir), player.position.tile + Right(dir) };
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

void Ring(Player &player, Point tile)
{
	AddMissile(tile, tile, player._pdir, MissileID::WarcryRing, TARGET_MONSTERS, static_cast<int>(player.getId()), 0, 0);
}

/**
 * @brief A census skill's own art (RfA-16) from @p from to @p tile, lasting @p ticks where the effect loops.
 * False, spawning nothing, while its sheet is not in the archive - the caller keeps its placeholder then.
 */
bool Show(Player &player, MissileID effect, MissileGraphicID art, Point from, Point tile, int ticks = 0)
{
	if (!MissileArtLoaded(art))
		return false;
	AddMissile(from, tile, GetDirection(from, tile), effect, TARGET_MONSTERS, static_cast<int>(player.getId()), ticks, 0);
	return true;
}

bool TeleportTo(Player &player, Point dst)
{
	if (dst == player.position.tile || !InDungeonBounds(dst))
		return false;
	return AddMissile(player.position.tile, dst, player._pdir, MissileID::Teleport, TARGET_MONSTERS,
	           static_cast<int>(player.getId()), 0, 0)
	    != nullptr;
}

/** @brief @p target, pulled back along the line from @p from to at most @p range tiles. */
Point Clamped(Point from, Point target, int range)
{
	const Displacement delta = target - from;
	const int longest = std::max(std::abs(delta.deltaX), std::abs(delta.deltaY));
	if (longest <= range)
		return target;
	return from + Displacement { delta.deltaX * range / longest, delta.deltaY * range / longest };
}

bool HoldsShield(const Player &player)
{
	for (const Item &item : { player.InvBody[INVLOC_HAND_LEFT], player.InvBody[INVLOC_HAND_RIGHT] }) {
		if (!item.isEmpty() && item._iStatFlag && item._itype == ItemType::Shield)
			return true;
	}
	return false;
}

int Percent(int value, int percent)
{
	return value * percent / 100;
}

// =================================================================================================
// State
// =================================================================================================

struct Marks {
	int judgmentTicks = 0;
	int judgmentPercent = 0;
	int oathTicks = 0;
	int oathCharges = 0;
	int oathRank = 0;
	int huntTicks = 0;
	int huntPercent = 0;
	int frostbiteTicks = 0;
	int frostbitePercent = 0;
	int tragedyTicks = 0;
	int tragedyPercent = 0;
	int satireTicks = 0;
	int ashenTicks = 0;
	int ashenRank = 0;
	int burnTicks = 0;
	int burnDamage = 0;
	int elegyTicks = 0;
	int elegyDamage = 0;
	int palmTicks = 0;
	int palmRank = 0;
	int threadPartner = -1;
	int threadTicks = 0;
	int threadRank = 0;
	int brittleCooldown = 0;
	int whiteoutStamp = 0;
};

std::array<Marks, MaxMonsters> MonsterMarks;

Marks &MarksOf(const Monster &monster)
{
	return MonsterMarks[monster.getId()];
}

int FrostbitePercentOn(const Monster &monster)
{
	const Marks &marks = MarksOf(monster);
	return marks.frostbiteTicks > 0 ? marks.frostbitePercent : 0;
}

enum class Buff : uint8_t {
	IronWill,
	Conduit,
	Saga,
	Astral,
	Evasion,
	StaticCharge,
	Retribution,
	Bloodcall,
	Feedback,
	Clarity,
	Rally,
	Chord,
	Immolate,
	Spheres,
	Count,
};

bool IsSheetBuff(Buff buff)
{
	return IsAnyOf(buff, Buff::IronWill, Buff::Conduit, Buff::Saga, Buff::Astral);
}

struct PlayerState {
	std::array<int, static_cast<size_t>(Buff::Count)> ticks {};
	std::array<int, static_cast<size_t>(Buff::Count)> rank {};
	int chordPool = 0;
	int rallyPerTick = 0;
	bool heroicCouplet = false;
	// Staff of Echoes: the blow that will land again.
	int echoTicks = 0;
	int echoMonster = -1;
	int echoDamage = 0;
	// Hunter's Claim.
	int claimTicks = 0;
	int claimMonster = -1;
	// Wrath of the Heavens: pillars left to fall.
	int wrathPillars = 0;
	int wrathClock = 0;
	int wrathRank = 0;
	// Funeral Star: the channel.
	int funeralTicks = 0;
	Point funeralTile;
	Point funeralFrom;
	int funeralRank = 0;
	// Storm Crucible: the first conductor, waiting for its pair.
	int crucibleTicks = 0;
	Point crucibleTile;
	// Ancestral Call / Spirit Guardian: the summon's clock.
	// Heaven's Descent and Leaping Crane land a tick after the teleport.
	int landingTicks = 0;
	Point landingTile;
	SpellID landingSpell = SpellID::Invalid;
	int landingRank = 0;
};

std::array<PlayerState, MAX_PLRS> Players12;

PlayerState &StateOf(const Player &player)
{
	return Players12[player.getId()];
}

void StartBuff(Player &player, Buff buff, int ticks, int rank)
{
	PlayerState &state = StateOf(player);
	const bool wasOn = state.ticks[static_cast<size_t>(buff)] > 0;
	state.ticks[static_cast<size_t>(buff)] = ticks;
	state.rank[static_cast<size_t>(buff)] = rank;
	if (IsSheetBuff(buff) && !wasOn)
		CalcPlrInv(player, false);
	else if (IsSheetBuff(buff))
		CalcPlrInv(player, false);
}

int BuffRank(const Player &player, Buff buff)
{
	const PlayerState &state = StateOf(player);
	return state.ticks[static_cast<size_t>(buff)] > 0 ? state.rank[static_cast<size_t>(buff)] : 0;
}

/** A ground effect: where it is, how long it lasts, and its own clock. */
struct Field {
	SpellID spell = SpellID::Invalid;
	uint8_t owner = 0;
	Point tile;
	Point tile2;
	Direction dir = Direction::South;
	int ticksLeft = 0;
	int clock = 0;
	int rank = 0;
	int step = 0;
	int stamp = 0;
};

constexpr size_t MaxFields = 32;
std::array<Field, MaxFields> Fields;
int FieldStamp = 0;

Field *NewField(const Player &player, SpellID spell, Point tile, int ticks, int rank)
{
	auto slot = std::min_element(Fields.begin(), Fields.end(), [](const Field &a, const Field &b) { return a.ticksLeft < b.ticksLeft; });
	*slot = Field {};
	slot->spell = spell;
	slot->owner = static_cast<uint8_t>(player.getId());
	slot->tile = tile;
	slot->ticksLeft = ticks;
	slot->rank = rank;
	slot->stamp = ++FieldStamp;
	return &*slot;
}

// =================================================================================================
// The numbers - one function each, shared by the effect and, where the row quotes it, the sentence
// =================================================================================================

struct Range {
	int min;
	int max;
};

Range Scale(int rank, int min, int max, int minPer, int maxPer)
{
	const int p = std::max(rank, 1) - 1;
	return { min + minPer * p, max + maxPer * p };
}

int Rolled(Range r)
{
	return Roll(r.min, r.max);
}

// =================================================================================================
// Swung
// =================================================================================================

std::optional<SpellID> ArmedSpell;

bool IsMeleeSpell(SpellID spell)
{
	switch (spell) {
	case SpellID::VotiveStrike:
	case SpellID::Judgment:
	case SpellID::Oathbrand:
	case SpellID::HolyLance:
	case SpellID::Crusade:
	case SpellID::AegisSlam:
	case SpellID::Cleave:
	case SpellID::Rend:
	case SpellID::HammerOfTheAncients:
	case SpellID::ClaspOfRuin:
	case SpellID::CinderTouch:
	case SpellID::Sweep:
	case SpellID::ReapingPoint:
	case SpellID::TurningPike:
	case SpellID::LowBranch:
	case SpellID::StaffOfEchoes:
	case SpellID::TigerClaw:
	case SpellID::PressurePoint:
	case SpellID::ExplodingPalm:
		return true;
	default:
		return false;
	}
}

int MeleeBonusPercent(SpellID spell, int rank)
{
	const int p = rank - 1;
	switch (spell) {
	case SpellID::VotiveStrike:
		return 30 + 8 * p;
	case SpellID::Judgment:
	case SpellID::Oathbrand:
	case SpellID::TigerClaw:
	case SpellID::ExplodingPalm:
		return 10 + 3 * p;
	case SpellID::Rend:
	case SpellID::ClaspOfRuin:
	case SpellID::TurningPike:
		return 20 + 5 * p;
	case SpellID::HammerOfTheAncients:
		return 150 + 15 * p;
	case SpellID::ReapingPoint:
		return 15 + 4 * p;
	default:
		return 0;
	}
}

/** @brief Mana, or the Barbarian's Rage - see oracool/rage.h. */
bool CanPay(const Player &player, SpellID spell)
{
	return CanPaySkill(player, spell);
}

/** @brief Settles a use that landed: the price paid, or a Rage generator's Rage for each landed blow. */
void Pay(Player &player, SpellID spell, int landedBlows = 1)
{
	SettleSkill(player, spell, landedBlows);
}

// =================================================================================================
// Cast
// =================================================================================================

bool IsPoetry(SpellID spell)
{
	return IsAnyOf(spell, SpellID::BitterCouplet, SpellID::MockingRhyme, SpellID::Epitaph, SpellID::Elegy, SpellID::SonnetOfSight,
	    SpellID::Satire, SpellID::VerseOfBinding, SpellID::Tragedy, SpellID::Saga, SpellID::LastWord);
}

bool IsBowSkill(SpellID spell)
{
	return IsAnyOf(spell, SpellID::BarbedShaft, SpellID::ShockArrow, SpellID::PiercingShot, SpellID::RainOfArrows,
	    SpellID::CripplingShot, SpellID::HuntersMark, SpellID::Barrage, SpellID::PhantomVolley);
}

void Bleed(const Monster &monster, int ticks, int perSecond)
{
	BleedMonster(monster, ticks, perSecond << 6);
}

bool CastOnce(Player &player, SpellID spell, Point target, int r)
{
	const Point here = player.position.tile;
	const int earshot = AuraRadiusForPoints(r);

	switch (spell) {
	// ---------------- Paladin ----------------
	case SpellID::HeavensDescent: {
		const Point dst = Clamped(here, target, 6);
		if (!TeleportTo(player, dst))
			return false;
		PlayerState &state = StateOf(player);
		state.landingTicks = 2;
		state.landingTile = dst;
		state.landingSpell = spell;
		state.landingRank = r;
		return true;
	}
	case SpellID::WrathOfTheHeavens: {
		PlayerState &state = StateOf(player);
		state.wrathPillars = 5;
		state.wrathClock = 0;
		state.wrathRank = r;
		return true;
	}
	// ---------------- Barbarian ----------------
	case SpellID::Backhand:
	case SpellID::RearwardReach: {
		Monster *behind = FindMonsterAtPosition(here + Opposite(player._pdir));
		if (behind == nullptr || !Hittable(*behind))
			return false;
		Strike(player, *behind, DamageType::Physical, Percent(WeaponBlow(player), 100 + 8 * (r - 1)));
		return true;
	}
	case SpellID::GroundStomp: {
		const auto around = MonstersWithin(here, 1);
		for (Monster *m : around)
			Stagger(*m, 30 + 4 * (r - 1));
		EarthenMightRage(player, around.size());
		return !around.empty();
	}
	case SpellID::SeismicSlam: {
		const auto line = MonstersOnLine(here, target, 5);
		for (Monster *m : line)
			Strike(player, *m, DamageType::Physical, Percent(WeaponBlow(player), 80 + 10 * (r - 1)));
		EarthenMightRage(player, line.size());
		Ring(player, here);
		return true;
	}
	case SpellID::Earthquake: {
		Field *f = NewField(player, spell, here, 4 * TicksPerSecond, r);
		f->clock = TicksPerSecond - 1;
		return true;
	}
	case SpellID::ThreateningShout: {
		const auto heard = MonstersWithin(here, earshot);
		for (Monster *m : heard)
			DebuffMonster(*m, (10 + (r - 1)) * TicksPerSecond, -std::min(15 + (r - 1), 40), 0);
		return !heard.empty();
	}
	case SpellID::RallyingCry: {
		PlayerState &state = StateOf(player);
		const int total = Percent(player._pMaxHP, std::min(20 + 2 * (r - 1), 50));
		state.rallyPerTick = std::max(total / (5 * TicksPerSecond), 1);
		StartBuff(player, Buff::Rally, 5 * TicksPerSecond, r);
		return true;
	}
	case SpellID::Intimidate: {
		const auto heard = MonstersWithin(here, earshot);
		for (Monster *m : heard)
			DebuffMonster(*m, (10 + (r - 1)) * TicksPerSecond, 0, -std::min(15 + 2 * (r - 1), 60));
		return !heard.empty();
	}
	case SpellID::SplitRanks: {
		const Direction dir = target == here ? player._pdir : GetDirection(here, target);
		bool any = false;
		for (Monster *m : MonstersWithin(here, 3)) {
			const Direction toward = GetDirection(here, m->position.tile);
			const bool left = toward == Left(dir);
			if (toward != dir && !left && toward != Right(dir))
				continue;
			Shove(*m, left ? Left(Left(dir)) : Right(Right(dir)));
			any = true;
		}
		return any;
	}
	case SpellID::IronWill:
		StartBuff(player, Buff::IronWill, (20 + 2 * (r - 1)) * TicksPerSecond, r);
		return true;
	case SpellID::Bloodcall:
		StartBuff(player, Buff::Bloodcall, 10 * TicksPerSecond, r);
		return true;
	case SpellID::AncestralCall:
	case SpellID::SpiritGuardian:
		// Companions (user, 2026-09-14) - the Ancients together, and the Monk's guardian. oracool/companion.h.
		return SummonCompanions(player, spell, target, r);
	case SpellID::EarthshakerCry: {
		const auto heard = MonstersWithin(here, 8);
		const Range d = Scale(r, 5, 10, 3, 5);
		for (Monster *m : heard) {
			Strike(player, *m, DamageType::Magic, Rolled(d));
			Stagger(*m, 2 * TicksPerSecond);
		}
		return true;
	}
	// ---------------- Sorceress: cold ----------------
	case SpellID::ChillTouch: {
		bool any = false;
		const Range d = Scale(r, 3, 6, 2, 3);
		for (const Point tile : FrontArc(player, target)) {
			Monster *m = FindMonsterAtPosition(tile);
			if (m == nullptr || !Hittable(*m))
				continue;
			Strike(player, *m, DamageType::Cold, Rolled(d));
			if ((m->hitPoints >> 6) > 0)
				ChillMonster(*m, 40 + 4 * (r - 1));
			any = true;
		}
		return any;
	}
	case SpellID::IceNeedle: {
		auto line = MonstersOnLine(here, target, 8);
		if (line.size() > 2)
			line.resize(2);
		const Range d = Scale(r, 5, 9, 3, 4);
		for (Monster *m : line) {
			Strike(player, *m, DamageType::Cold, Rolled(d));
			if ((m->hitPoints >> 6) > 0)
				ChillMonster(*m, 40);
		}
		return !line.empty();
	}
	case SpellID::Frostbite: {
		Monster *m = NearestTo(target, 2);
		if (m == nullptr)
			return false;
		Marks &marks = MarksOf(*m);
		marks.frostbiteTicks = 6 * TicksPerSecond;
		marks.frostbitePercent = std::min(20 + 2 * (r - 1), 60);
		ChillMonster(*m, 6 * TicksPerSecond);
		return true;
	}
	case SpellID::IceLance: {
		const auto line = MonstersOnLine(here, target, 8);
		const Range d = Scale(r, 6, 11, 3, 5);
		for (Monster *m : line) {
			Strike(player, *m, DamageType::Cold, Rolled(d));
			if ((m->hitPoints >> 6) > 0)
				ChillMonster(*m, 40);
		}
		return !line.empty();
	}
	case SpellID::BrittleGround: {
		Field *f = NewField(player, spell, target, 6 * TicksPerSecond, r);
		f->tile2 = target + (target == here ? player._pdir : GetDirection(here, target));
		return true;
	}
	case SpellID::FrozenSentinel:
		NewField(player, spell, target, 15 * TicksPerSecond, r);
		return true;
	case SpellID::Whiteout: {
		Field *f = NewField(player, spell, here, 8 * 6, r);
		f->dir = target == here ? player._pdir : GetDirection(here, target);
		return true;
	}
	case SpellID::AbsoluteZero: {
		const Range d = Scale(r, 8, 16, 4, 6);
		for (Monster *m : MonstersWithin(here, 8)) {
			Strike(player, *m, DamageType::Cold, Rolled(d));
			if ((m->hitPoints >> 6) <= 0)
				continue;
			if (ShrugsOff(*m))
				ChillMonster(*m, 2 * TicksPerSecond);
			else
				FreezeMonster(*m, 2 * TicksPerSecond);
		}
		Ring(player, here);
		return true;
	}
	// ---------------- Sorceress: lightning ----------------
	case SpellID::Arc: {
		Monster *m = NearestTo(target, 3);
		if (m == nullptr)
			return false;
		const Range d = Scale(r, 2, 12, 2, 5);
		int damage = Rolled(d);
		std::vector<Monster *> struck;
		for (int hop = 0; hop < 3 && m != nullptr; hop++) {
			struck.push_back(m);
			const Point at = m->position.tile;
			Strike(player, *m, DamageType::Lightning, damage);
			damage = Percent(damage, 75);
			Monster *next = nullptr;
			for (Monster *candidate : MonstersWithin(at, 3)) {
				if (std::find(struck.begin(), struck.end(), candidate) == struck.end()) {
					next = candidate;
					break;
				}
			}
			m = next;
		}
		return true;
	}
	case SpellID::StaticCharge:
		StartBuff(player, Buff::StaticCharge, (20 + 2 * (r - 1)) * TicksPerSecond, r);
		return true;
	case SpellID::BallLightning: {
		Field *f = NewField(player, spell, here, 8 * 10, r);
		f->tile2 = here;
		f->dir = target == here ? player._pdir : GetDirection(here, target);
		return true;
	}
	case SpellID::Conduit:
		StartBuff(player, Buff::Conduit, 10 * TicksPerSecond, r);
		return true;
	case SpellID::LightningRod:
		NewField(player, spell, target, 12 * TicksPerSecond, r);
		return true;
	case SpellID::FaradayRing:
		NewField(player, spell, target, 4 * TicksPerSecond, r);
		return true;
	case SpellID::StormCrucible: {
		PlayerState &state = StateOf(player);
		if (state.crucibleTicks <= 0) {
			state.crucibleTicks = 8 * TicksPerSecond;
			state.crucibleTile = target;
			Ring(player, target);
			return true;
		}
		Field *f = NewField(player, spell, state.crucibleTile, 3 * 15, r);
		f->tile2 = target;
		f->clock = 14;
		state.crucibleTicks = 0;
		return true;
	}
	case SpellID::RideTheLightning: {
		const Point dst = Clamped(here, target, 6);
		const auto line = MonstersOnLine(here, dst, here.WalkingDistance(dst));
		if (!TeleportTo(player, dst))
			return false;
		const Range d = Scale(r, 3, 12, 2, 4);
		for (Monster *m : line)
			Strike(player, *m, DamageType::Lightning, Rolled(d));
		return true;
	}
	// ---------------- Sorceress: fire ----------------
	case SpellID::EmberMine:
		NewField(player, spell, target, 20 * TicksPerSecond, r);
		return true;
	case SpellID::FlameRing: {
		const Range d = Scale(r, 4, 8, 2, 3);
		for (Monster *m : MonstersWithin(here, 2))
			Strike(player, *m, DamageType::Fire, Rolled(d));
		Ring(player, here);
		return true;
	}
	case SpellID::AshenBrand: {
		Monster *m = NearestTo(target, 2);
		if (m == nullptr)
			return false;
		MarksOf(*m).ashenTicks = 4 * TicksPerSecond;
		MarksOf(*m).ashenRank = r;
		return true;
	}
	case SpellID::FurnaceMouth: {
		Field *f = NewField(player, spell, target, 3 * TicksPerSecond + 1, r);
		f->dir = target == here ? player._pdir : GetDirection(here, target);
		f->clock = TicksPerSecond - 1;
		return true;
	}
	case SpellID::Firestorm:
		NewField(player, spell, target, 4 * TicksPerSecond, r);
		return true;
	case SpellID::Immolate:
		StartBuff(player, Buff::Immolate, 10 * TicksPerSecond, r);
		return true;
	case SpellID::FuneralStar: {
		PlayerState &state = StateOf(player);
		state.funeralTicks = 2 * TicksPerSecond;
		state.funeralTile = target;
		state.funeralFrom = here;
		state.funeralRank = r;
		return true;
	}
	// ---------------- Rogue: bow ----------------
	case SpellID::BarbedShaft: {
		Monster *m = NearestTo(target, 1);
		if (m == nullptr)
			return false;
		Strike(player, *m, DamageType::Physical, Percent(WeaponBlow(player), 100 + 5 * (r - 1)));
		Bleed(*m, 3 * TicksPerSecond, 2 + r);
		return true;
	}
	case SpellID::ShockArrow: {
		Monster *m = NearestTo(target, 1);
		if (m == nullptr)
			return false;
		const Point at = m->position.tile;
		Strike(player, *m, DamageType::Physical, WeaponBlow(player));
		if (Monster *other = NearestTo(at, 3, m); other != nullptr)
			Strike(player, *other, DamageType::Lightning, Rolled(Scale(r, 1, 6, 1, 3)));
		return true;
	}
	case SpellID::PiercingShot: {
		const auto line = MonstersOnLine(here, target, 10);
		for (Monster *m : line)
			Strike(player, *m, DamageType::Physical, Percent(WeaponBlow(player), 80 + 5 * (r - 1)));
		return !line.empty();
	}
	case SpellID::RainOfArrows: {
		for (Monster *m : MonstersWithin(target, 2))
			Strike(player, *m, DamageType::Physical, Percent(WeaponBlow(player), 50 + 4 * (r - 1)));
		Ring(player, target);
		return true;
	}
	case SpellID::CripplingShot: {
		Monster *m = NearestTo(target, 1);
		if (m == nullptr)
			return false;
		Strike(player, *m, DamageType::Physical, WeaponBlow(player));
		if ((m->hitPoints >> 6) > 0)
			ChillMonster(*m, 4 * TicksPerSecond);
		return true;
	}
	case SpellID::HuntersMark: {
		Monster *m = NearestTo(target, 2);
		if (m == nullptr)
			return false;
		MarksOf(*m).huntTicks = (10 + (r - 1)) * TicksPerSecond;
		MarksOf(*m).huntPercent = std::min(20 + 2 * (r - 1), 60);
		return true;
	}
	case SpellID::Barrage: {
		Monster *m = NearestTo(target, 1);
		if (m == nullptr)
			return false;
		for (int i = 0; i < 5 && (m->hitPoints >> 6) > 0; i++)
			Strike(player, *m, DamageType::Physical, Percent(WeaponBlow(player), 40 + 3 * (r - 1)));
		return true;
	}
	case SpellID::PhantomVolley: {
		const auto all = MonstersWithin(here, 6);
		for (Monster *m : all)
			Strike(player, *m, DamageType::Magic, Percent(WeaponBlow(player), 40 + 3 * (r - 1)));
		return !all.empty();
	}
	// ---------------- Rogue: magic and spear ----------------
	case SpellID::ShadowStep: {
		Monster *m = NearestTo(target, 2);
		if (m == nullptr)
			return false;
		const Point behind = m->position.tile + GetDirection(here, m->position.tile);
		return TeleportTo(player, behind);
	}
	case SpellID::HuntersClaim: {
		Monster *best = nullptr;
		for (Monster *m : MonstersWithin(target, 3)) {
			if (m->isUnique() && (best == nullptr || target.WalkingDistance(m->position.tile) < target.WalkingDistance(best->position.tile)))
				best = m;
		}
		if (best == nullptr)
			return false;
		StateOf(player).claimMonster = static_cast<int>(best->getId());
		StateOf(player).claimTicks = 8 * TicksPerSecond;
		return true;
	}
	case SpellID::Harpoon: {
		const auto line = MonstersOnLine(here, target, 8);
		if (line.empty())
			return false;
		Monster &m = *line.front();
		Strike(player, m, DamageType::Physical, Percent(WeaponBlow(player), 60 + 5 * (r - 1)));
		if ((m.hitPoints >> 6) > 0) {
			Shove(m, GetDirection(m.position.tile, here));
			Stagger(m, TicksPerSecond / 2);
		}
		return true;
	}
	case SpellID::Vault:
		return TeleportTo(player, Clamped(here, target, std::min(3 + (r - 1) / 5, 6)));
	case SpellID::AnchorJavelin: {
		const auto line = MonstersOnLine(here, target, 8);
		if (line.empty())
			return false;
		Strike(player, *line.front(), DamageType::Physical, Percent(WeaponBlow(player), 70 + 5 * (r - 1)));
		Stagger(*line.front(), 2 * TicksPerSecond);
		return true;
	}
	// ---- the census notes (2026-09-14) ----
	case SpellID::Meteor:
		// A second to fall, three to burn: TickField does both. The rock's ten frames take that second.
		NewField(player, spell, target, 4 * TicksPerSecond, r);
		Show(player, MissileID::MeteorFall, MissileGraphicID::Meteor, target, target);
		return true;
	case SpellID::Valkyrie:
	case SpellID::Decoy:
		// Companions (user, 2026-09-14): the Valkyrie archer, and the Decoy that draws every blow. oracool/companion.h.
		return SummonCompanions(player, spell, target, r);
	case SpellID::PoisonJavelin: {
		const auto line = MonstersOnLine(here, target, 8);
		if (line.empty())
			return false;
		Monster &m = *line.front();
		const Point pool = m.position.tile;
		Strike(player, m, DamageType::Acid, Percent(WeaponBlow(player), 60 + 5 * (r - 1)));
		NewField(player, spell, pool, 3 * TicksPerSecond, r);
		Show(player, MissileID::AcidJavelin, MissileGraphicID::AcidJavelin, here, pool);
		Show(player, MissileID::AcidCloud, MissileGraphicID::AcidCloud, pool, pool, 3 * TicksPerSecond);
		return true;
	}
	case SpellID::PlagueJavelin: {
		const auto line = MonstersOnLine(here, target, 8);
		const Point burst = line.empty() ? Clamped(here, target, 8) : Point(line.front()->position.tile);
		NewField(player, spell, burst, 5 * TicksPerSecond, r);
		Show(player, MissileID::AcidJavelin, MissileGraphicID::AcidJavelin, here, burst);
		if (!Show(player, MissileID::AcidCloud, MissileGraphicID::AcidCloud, burst, burst, 5 * TicksPerSecond))
			Ring(player, burst);
		return true;
	}
	case SpellID::ValkyriesSpear: {
		for (Monster *m : MonstersWithin(target, 1))
			Strike(player, *m, DamageType::Physical, Percent(WeaponBlow(player), 150 + 15 * (r - 1)));
		Ring(player, target);
		return true;
	}
	// ---------------- Bard: harmony ----------------
	case SpellID::PluckedNeedle: {
		const auto line = MonstersOnLine(here, target, 8);
		if (line.empty())
			return false;
		Strike(player, *line.front(), DamageType::Magic, Rolled(Scale(r, 4, 8, 2, 4)));
		return true;
	}
	case SpellID::ShatterNote: {
		const auto line = MonstersOnLine(here, target, 8);
		const Range d = Scale(r, 3, 7, 2, 3);
		for (Monster *m : line) {
			Strike(player, *m, DamageType::Magic, Rolled(d));
			DebuffMonster(*m, 8 * TicksPerSecond, 0, -std::min(15 + (r - 1), 50));
		}
		return !line.empty();
	}
	case SpellID::TuningFork: {
		Field *f = NewField(player, spell, target, 12 * TicksPerSecond, r);
		f->clock = TicksPerSecond - 1;
		return true;
	}
	case SpellID::Thunderclap: {
		const auto around = MonstersWithin(here, 2);
		const Range d = Scale(r, 1, 4, 1, 2);
		for (Monster *m : around) {
			Strike(player, *m, DamageType::Lightning, Rolled(d));
			Stagger(*m, 30);
		}
		return !around.empty();
	}
	case SpellID::DissonantThread: {
		Monster *a = NearestTo(target, 4);
		Monster *b = a == nullptr ? nullptr : NearestTo(target, 4, a);
		if (a == nullptr || b == nullptr)
			return false;
		for (auto [one, other] : { std::pair { a, b }, std::pair { b, a } }) {
			Marks &marks = MarksOf(*one);
			marks.threadPartner = static_cast<int>(other->getId());
			marks.threadTicks = 8 * TicksPerSecond;
			marks.threadRank = r;
		}
		return true;
	}
	case SpellID::SoundWave: {
		const auto line = MonstersOnLine(here, target, 12);
		const Range d = Scale(r, 4, 9, 2, 4);
		for (Monster *m : line)
			Strike(player, *m, DamageType::Magic, Rolled(d));
		return !line.empty();
	}
	case SpellID::DeafeningRoar: {
		const auto heard = MonstersWithin(here, earshot);
		for (Monster *m : heard)
			Stagger(*m, 2 * TicksPerSecond);
		return !heard.empty();
	}
	case SpellID::ChordOfWarding:
		StateOf(player).chordPool = (20 + 8 * (r - 1)) << 6;
		StartBuff(player, Buff::Chord, 15 * TicksPerSecond, r);
		return true;
	case SpellID::Feedback:
		StartBuff(player, Buff::Feedback, 6 * TicksPerSecond, r);
		return true;
	case SpellID::GrandFinale: {
		const bool singing = GetActiveClassAura(player) != ClassTreeSkill::None;
		Range d = Scale(r, 6, 12, 3, 5);
		if (singing)
			d = { d.min * 3 / 2, d.max * 3 / 2 };
		for (Monster *m : MonstersWithin(here, 3))
			Strike(player, *m, DamageType::Magic, Rolled(d));
		Ring(player, here);
		return true;
	}
	case SpellID::MusicOfTheSpheres:
		StartBuff(player, Buff::Spheres, 15 * TicksPerSecond, r);
		return true;
	// ---------------- Bard: poetry ----------------
	case SpellID::BitterCouplet: {
		Monster *m = NearestTo(target, 2);
		if (m == nullptr)
			return false;
		BlockMonsterRegen(*m, 4 * TicksPerSecond);
		return true;
	}
	case SpellID::MockingRhyme: {
		Monster *m = NearestTo(target, 2);
		if (m == nullptr)
			return false;
		DebuffMonster(*m, 10 * TicksPerSecond, -std::min(20 + 2 * (r - 1), 50), 0);
		return true;
	}
	case SpellID::Epitaph: {
		std::optional<Point> corpse;
		int best = 3;
		for (int y = target.y - 2; y <= target.y + 2; y++) {
			for (int x = target.x - 2; x <= target.x + 2; x++) {
				const Point tile { x, y };
				if (!InDungeonBounds(tile) || dCorpse[x][y] == 0)
					continue;
				if (target.WalkingDistance(tile) < best) {
					best = target.WalkingDistance(tile);
					corpse = tile;
				}
			}
		}
		if (!corpse)
			return false;
		dCorpse[corpse->x][corpse->y] = 0;
		const Range d = Scale(r, 6, 12, 3, 5);
		for (Monster *m : MonstersWithin(*corpse, 2))
			Strike(player, *m, DamageType::Magic, Rolled(d));
		Ring(player, *corpse);
		return true;
	}
	case SpellID::Elegy: {
		Monster *m = NearestTo(target, 2);
		if (m == nullptr)
			return false;
		MarksOf(*m).elegyTicks = 6 * TicksPerSecond;
		MarksOf(*m).elegyDamage = (3 + (r - 1)) << 6;
		return true;
	}
	case SpellID::SonnetOfSight: {
		constexpr int Reach = 12;
		for (int y = -Reach; y <= Reach; y++) {
			for (int x = -Reach; x <= Reach; x++) {
				const Point tile = here + Displacement { x, y };
				if (InDungeonBounds(tile))
					SetAutomapView(tile, MAP_EXP_SELF);
			}
		}
		for (Monster *m : MonstersWithin(here, Reach))
			ScentMonster(*m, (5 + (r - 1) / 2) * TicksPerSecond);
		return true;
	}
	case SpellID::Satire: {
		const auto nearby = MonstersWithin(target, 2);
		for (Monster *m : nearby)
			MarksOf(*m).satireTicks = (8 + (r - 1) / 2) * TicksPerSecond;
		return !nearby.empty();
	}
	case SpellID::VerseOfBinding: {
		const auto nearby = MonstersWithin(target, 2);
		for (Monster *m : nearby)
			Stagger(*m, 3 * TicksPerSecond);
		return !nearby.empty();
	}
	case SpellID::HeroicCouplet:
		StateOf(player).heroicCouplet = true;
		return true;
	case SpellID::Tragedy: {
		Monster *m = NearestTo(target, 2);
		if (m == nullptr)
			return false;
		MarksOf(*m).tragedyTicks = 8 * TicksPerSecond;
		MarksOf(*m).tragedyPercent = std::min(25 + (r - 1), 50);
		return true;
	}
	case SpellID::Saga:
		StartBuff(player, Buff::Saga, 20 * TicksPerSecond, r);
		return true;
	case SpellID::LastWord: {
		Monster *m = NearestTo(target, 2);
		if (m == nullptr || ShrugsOff(*m) || m->hitPoints * 5 >= m->maxHitPoints)
			return false;
		ApplyMonsterDamage(DamageType::Magic, *m, m->hitPoints);
		M_StartKill(*m, player);
		return true;
	}
	// ---------------- Monk: staff ----------------
	case SpellID::LongThrust: {
		const auto line = MonstersOnLine(here, target, 2);
		if (line.empty())
			return false;
		Strike(player, *line.front(), DamageType::Physical, Percent(WeaponBlow(player), 100 + 8 * (r - 1)));
		return true;
	}
	case SpellID::MountainPole: {
		const auto around = MonstersWithin(here, 1);
		for (Monster *m : around) {
			Strike(player, *m, DamageType::Physical, Percent(WeaponBlow(player), 50 + 5 * (r - 1)));
			Stagger(*m, TicksPerSecond);
		}
		return !around.empty();
	}
	case SpellID::BambooRain: {
		auto nearby = MonstersWithin(here, 2);
		if (nearby.size() > 3)
			nearby.resize(3);
		for (Monster *m : nearby)
			Strike(player, *m, DamageType::Physical, Percent(WeaponBlow(player), 70 + 5 * (r - 1)));
		return !nearby.empty();
	}
	case SpellID::DragonTailSweep:
	case SpellID::WhirlingKick: {
		const auto around = MonstersWithin(here, 1);
		for (Monster *m : around) {
			Strike(player, *m, DamageType::Physical, Percent(WeaponBlow(player), (spell == SpellID::WhirlingKick ? 70 : 60) + 5 * (r - 1)));
			Shove(*m, GetDirection(here, m->position.tile));
		}
		return !around.empty();
	}
	case SpellID::HeavenSplitter: {
		const auto line = MonstersOnLine(here, target, 4);
		for (Monster *m : line)
			Strike(player, *m, DamageType::Physical, Percent(WeaponBlow(player), 120 + 10 * (r - 1)));
		Ring(player, here);
		return true;
	}
	case SpellID::ThousandReeds: {
		const auto all = MonstersWithin(here, 6);
		for (Monster *m : all)
			Strike(player, *m, DamageType::Physical, Percent(WeaponBlow(player), 40 + 3 * (r - 1)));
		return !all.empty();
	}
	// ---------------- Monk: body ----------------
	case SpellID::LeapingCrane: {
		const Point dst = Clamped(here, target, std::min(4 + (r - 1) / 4, 7));
		if (!TeleportTo(player, dst))
			return false;
		PlayerState &state = StateOf(player);
		state.landingTicks = 2;
		state.landingTile = target;
		state.landingSpell = spell;
		state.landingRank = r;
		return true;
	}
	case SpellID::ShoulderGate: {
		const Point dst = Clamped(here, target, 2);
		if (!TeleportTo(player, dst))
			return false;
		PlayerState &state = StateOf(player);
		state.landingTicks = 2;
		state.landingTile = target;
		state.landingSpell = spell;
		state.landingRank = r;
		return true;
	}
	case SpellID::SevenSidedStrike: {
		auto nearby = MonstersWithin(target, 4);
		const size_t cap = static_cast<size_t>(std::min(3 + (r - 1) / 3, 7));
		if (nearby.size() > cap)
			nearby.resize(cap);
		for (Monster *m : nearby)
			Strike(player, *m, DamageType::Physical, Percent(WeaponBlow(player), 60 + 4 * (r - 1)));
		return !nearby.empty();
	}
	case SpellID::DragonsWrath: {
		const auto line = MonstersOnLine(here, target, 8);
		for (Monster *m : line)
			Strike(player, *m, DamageType::Physical, Percent(WeaponBlow(player), 100 + 8 * (r - 1)));
		Ring(player, here);
		return true;
	}
	// ---------------- Monk: spirit ----------------
	case SpellID::MantraOfClarity:
		StartBuff(player, Buff::Clarity, 30 * TicksPerSecond, r);
		return true;
	case SpellID::MantraOfEvasion:
		StartBuff(player, Buff::Evasion, 30 * TicksPerSecond, r);
		return true;
	case SpellID::ChiWave: {
		Monster *m = NearestTo(target, 3);
		if (m == nullptr)
			return false;
		const Range d = Scale(r, 4, 8, 2, 3);
		std::vector<Monster *> struck;
		for (int hop = 0; hop < 4 && m != nullptr; hop++) {
			struck.push_back(m);
			const Point at = m->position.tile;
			Strike(player, *m, DamageType::Magic, Rolled(d));
			Monster *next = nullptr;
			for (Monster *candidate : MonstersWithin(at, 3)) {
				if (std::find(struck.begin(), struck.end(), candidate) == struck.end()) {
					next = candidate;
					break;
				}
			}
			m = next;
		}
		return true;
	}
	case SpellID::BlindingFlash: {
		const auto nearby = MonstersWithin(here, 3);
		for (Monster *m : nearby) {
			if (ShrugsOff(*m) || m->mode == MonsterMode::Petrified)
				continue;
			m->goal = MonsterGoal::Retreat;
			m->goalVar1 = 3;
			m->goalVar2 = static_cast<int8_t>(GenerateRnd(8));
		}
		return !nearby.empty();
	}
	case SpellID::MantraOfRetribution:
		StartBuff(player, Buff::Retribution, 30 * TicksPerSecond, r);
		return true;
	case SpellID::Serenity:
		ClearPlayerSlow(player);
		return true;
	case SpellID::WaveOfLight: {
		const Range d = Scale(r, 7, 14, 3, 5);
		for (Monster *m : MonstersWithin(target, 2))
			Strike(player, *m, DamageType::Magic, Rolled(d));
		Ring(player, target);
		return true;
	}
	case SpellID::AstralProjection:
		StartBuff(player, Buff::Astral, 6 * TicksPerSecond, r);
		return true;
	case SpellID::AncestralCourt: {
		Field *f = NewField(player, spell, target, TicksPerSecond + 15, r);
		f->clock = 0;
		return true;
	}
	default:
		// The Necromancer's pages live in their own modules and come through this door (oracool/necro_summoning.h).
		return CastNecromancerSummoning(player, spell, target, r);
	}
}

// =================================================================================================
// The ticks
// =================================================================================================

void TickField(Player &player, Field &field)
{
	const int r = field.rank;
	field.clock++;
	switch (field.spell) {
	case SpellID::Earthquake:
		if (field.clock % TicksPerSecond == 0) {
			const auto shaken = MonstersWithin(field.tile, 3);
			for (Monster *m : shaken)
				Strike(player, *m, DamageType::Physical, Percent(WeaponBlow(player), 30 + 5 * (r - 1)));
			EarthenMightRage(player, shaken.size());
			Ring(player, field.tile);
		}
		break;
	case SpellID::BrittleGround:
		for (const Point tile : { field.tile, field.tile2 }) {
			Monster *m = InDungeonBounds(tile) ? FindMonsterAtPosition(tile) : nullptr;
			if (m == nullptr || !Hittable(*m) || !m->isWalking())
				continue;
			Marks &marks = MarksOf(*m);
			if (marks.brittleCooldown > 0)
				continue;
			marks.brittleCooldown = TicksPerSecond;
			Strike(player, *m, DamageType::Cold, Rolled(Scale(r, 4, 8, 2, 3)));
			if ((m->hitPoints >> 6) > 0)
				ChillMonster(*m, TicksPerSecond);
		}
		break;
	case SpellID::FrozenSentinel:
		if (field.clock % 30 == 0) {
			if (Monster *m = NearestTo(field.tile, 8); m != nullptr) {
				AddMissile(field.tile, m->position.tile, GetDirection(field.tile, m->position.tile), MissileID::IceBolt,
				    TARGET_MONSTERS, static_cast<int>(player.getId()), 0, r);
			}
		}
		break;
	case SpellID::Whiteout:
		if (field.clock % 6 == 0) {
			field.tile = field.tile + field.dir;
			if (!InDungeonBounds(field.tile) || IsTileSolid(field.tile)) {
				field.ticksLeft = 0;
				break;
			}
			for (const Point tile : { field.tile, field.tile + Left(Left(field.dir)), field.tile + Right(Right(field.dir)) }) {
				Monster *m = InDungeonBounds(tile) ? FindMonsterAtPosition(tile) : nullptr;
				if (m == nullptr || !Hittable(*m) || MarksOf(*m).whiteoutStamp == field.stamp)
					continue;
				MarksOf(*m).whiteoutStamp = field.stamp;
				Strike(player, *m, DamageType::Cold, Rolled(Scale(r, 5, 10, 3, 4)));
				if ((m->hitPoints >> 6) > 0)
					ChillMonster(*m, 40);
			}
			Ring(player, field.tile);
		}
		break;
	case SpellID::BallLightning:
		if (field.clock % 10 == 0) {
			const Point next = field.tile2 + field.dir;
			if (!InDungeonBounds(next) || IsTileSolid(next)) {
				field.ticksLeft = 0;
				break;
			}
			field.tile2 = next;
			AddMissile(next, next + field.dir, field.dir, MissileID::ChargedBolt, TARGET_MONSTERS,
			    static_cast<int>(player.getId()), 0, r);
		}
		break;
	case SpellID::LightningRod:
	case SpellID::FaradayRing: {
		const int reach = field.spell == SpellID::FaradayRing ? 2 : 1;
		for (Missile &missile : Missiles) {
			if (missile._miDelFlag || missile.sourceType() != MissileSource::Monster)
				continue;
			if (field.tile.WalkingDistance(missile.position.tile) > reach)
				continue;
			if (field.spell == SpellID::LightningRod && GetMissileData(missile._mitype).damageType() != DamageType::Lightning)
				continue;
			missile._miDelFlag = true;
			if (field.spell == SpellID::LightningRod) {
				for (Monster *m : MonstersWithin(field.tile, 2))
					Strike(player, *m, DamageType::Lightning, Rolled(Scale(r, 4, 14, 2, 5)));
				Ring(player, field.tile);
				field.ticksLeft = 0;
				break;
			}
		}
		break;
	}
	case SpellID::StormCrucible:
		if (field.clock % 15 == 0) {
			const Range d = Scale(r, 4, 16, 2, 6);
			for (Monster *m : MonstersOnLine(field.tile, field.tile2, field.tile.WalkingDistance(field.tile2)))
				Strike(player, *m, DamageType::Lightning, Rolled(d));
			Ring(player, field.tile);
			Ring(player, field.tile2);
		}
		break;
	case SpellID::EmberMine: {
		Monster *m = FindMonsterAtPosition(field.tile);
		if (m != nullptr && Hittable(*m)) {
			const Range d = Scale(r, 6, 12, 3, 5);
			for (Monster *nearby : MonstersWithin(field.tile, 1))
				Strike(player, *nearby, DamageType::Fire, Rolled(d));
			Ring(player, field.tile);
			field.ticksLeft = 0;
		}
		break;
	}
	// ---- the census notes (2026-09-14) ----
	case SpellID::Meteor:
		if (field.step == 0 && field.clock >= TicksPerSecond) {
			field.step = 1;
			const Range d = Scale(r, 20, 40, 8, 12);
			for (Monster *m : MonstersWithin(field.tile, 2))
				Strike(player, *m, DamageType::Fire, Rolled(d));
			// The burst, then its ground burn for the rest of the field's life.
			if (!Show(player, MissileID::MeteorImpact, MissileGraphicID::MeteorImpact, field.tile, field.tile, field.ticksLeft))
				Ring(player, field.tile);
		} else if (field.step == 1 && field.clock % TicksPerSecond == 0) {
			const Range burn = Scale(r, 3, 6, 1, 2);
			for (Monster *m : MonstersWithin(field.tile, 1))
				Strike(player, *m, DamageType::Fire, Rolled(burn));
		}
		break;
	case SpellID::PoisonJavelin:
		if (field.clock % TicksPerSecond == 0) {
			const Range d = Scale(r, 2, 4, 1, 1);
			for (Monster *m : MonstersWithin(field.tile, 1))
				Strike(player, *m, DamageType::Acid, Rolled(d));
		}
		break;
	case SpellID::PlagueJavelin:
		if (field.clock % TicksPerSecond == 0) {
			const Range d = Scale(r, 4, 8, 2, 3);
			for (Monster *m : MonstersWithin(field.tile, 2))
				Strike(player, *m, DamageType::Acid, Rolled(d));
			if (!MissileArtLoaded(MissileGraphicID::AcidCloud))
				Ring(player, field.tile); // the cloud shows the pulse's reach while it hangs there
		}
		break;
	case SpellID::FurnaceMouth:
		if (field.clock % TicksPerSecond == 0) {
			const Range d = Scale(r, 4, 9, 2, 4);
			for (Monster *m : MonstersOnLine(field.tile, field.tile + field.dir, 3))
				Strike(player, *m, DamageType::Fire, Rolled(d));
			Ring(player, field.tile);
		}
		break;
	case SpellID::Firestorm:
		if (field.clock % 8 == 0) {
			const Point landing = field.tile + Displacement { GenerateRnd(7) - 3, GenerateRnd(7) - 3 };
			if (InDungeonBounds(landing))
				AddMissile(player.position.tile, landing, GetDirection(player.position.tile, landing), MissileID::Fireball,
				    TARGET_MONSTERS, static_cast<int>(player.getId()), 0, r);
		}
		break;
	case SpellID::TuningFork:
		if (field.clock % TicksPerSecond == 0) {
			const Range d = Scale(r, 2, 5, 1, 2);
			for (Monster *m : MonstersWithin(field.tile, 2))
				Strike(player, *m, DamageType::Magic, Rolled(d));
			Ring(player, field.tile);
		}
		break;
	case SpellID::AncestralCourt:
		if (IsAnyOf(field.clock, TicksPerSecond, TicksPerSecond + 5, TicksPerSecond + 10)) {
			const Range d = Scale(r, 5, 9, 2, 4);
			for (Monster *m : MonstersWithin(field.tile, 2))
				Strike(player, *m, DamageType::Magic, Rolled(d));
			Ring(player, field.tile);
		}
		break;
	default:
		break;
	}
}

void TickLanding(Player &player, PlayerState &state)
{
	if (state.landingTicks <= 0 || --state.landingTicks > 0)
		return;
	const int r = state.landingRank;
	switch (state.landingSpell) {
	case SpellID::HeavensDescent: {
		const Range d = Scale(r, 8, 16, 4, 6);
		for (Monster *m : MonstersWithin(player.position.tile, 1))
			Strike(player, *m, DamageType::Magic, Rolled(d));
		Ring(player, player.position.tile);
		break;
	}
	case SpellID::LeapingCrane:
		if (Monster *m = NearestTo(player.position.tile, 1); m != nullptr)
			Strike(player, *m, DamageType::Physical, Percent(WeaponBlow(player), 80 + 5 * (r - 1)));
		break;
	case SpellID::ShoulderGate:
		if (Monster *m = NearestTo(player.position.tile, 1); m != nullptr)
			Stagger(*m, TicksPerSecond);
		break;
	default:
		break;
	}
	state.landingSpell = SpellID::Invalid;
}

} // namespace

// =================================================================================================
// The exported surface
// =================================================================================================

bool IsRfa12Active(SpellID spell)
{
	return spell > SpellID::GrimWard && spell <= SpellID::LAST;
}

bool IsRfa12Melee(SpellID spell)
{
	return IsMeleeSpell(spell);
}

bool Rfa12MeleeUsable(const Player &player, SpellID spell)
{
	if (spell == SpellID::AegisSlam)
		return HoldsShield(player);
	return true;
}

void ArmRfa12Melee(std::optional<SpellID> spell)
{
	ArmedSpell = spell;
}

std::optional<SpellID> ArmedRfa12Melee()
{
	return ArmedSpell;
}

int Rfa12MeleeDamagePercent(const Player &player)
{
	if (&player != MyPlayer || !ArmedSpell.has_value() || !CanPay(player, *ArmedSpell))
		return 0;
	return MeleeBonusPercent(*ArmedSpell, RankOf(player, *ArmedSpell));
}

bool ApplyRfa12MeleeOnSwing(Player &player, Monster *front, bool frontHit, int frontDamage)
{
	if (&player != MyPlayer || !ArmedSpell.has_value())
		return false;
	const SpellID spell = *ArmedSpell;
	if (!CanPay(player, spell) || !Rfa12MeleeUsable(player, spell))
		return false;
	const int r = RankOf(player, spell);
	const bool landed = front != nullptr && frontHit;
	const bool alive = landed && (front->hitPoints >> 6) > 0;
	bool struck = false;
	// Blows that struck a monster - the Barbarian's Rage is earned per blow (2026-09-14).
	int landedBlows = landed ? 1 : 0;
	const Point ahead = player.position.tile + player._pdir;

	switch (spell) {
	case SpellID::VotiveStrike:
		if (landed && !alive) {
			TakeCorpseOf(*front);
			struck = true;
		}
		break;
	case SpellID::Judgment:
		if (alive) {
			MarksOf(*front).judgmentTicks = 4 * TicksPerSecond;
			MarksOf(*front).judgmentPercent = std::min(15 + (r - 1), 40);
			struck = true;
		}
		break;
	case SpellID::Oathbrand:
		if (alive) {
			Marks &marks = MarksOf(*front);
			marks.oathTicks = 6 * TicksPerSecond;
			marks.oathCharges = 3;
			marks.oathRank = r;
			struck = true;
		}
		break;
	case SpellID::HolyLance:
		for (const Point tile : LineOfTiles(ahead, ahead + player._pdir, 2)) {
			Monster *m = FindMonsterAtPosition(tile);
			if (m != nullptr && Hittable(*m)) {
				Strike(player, *m, DamageType::Magic, Percent(WeaponBlow(player), 80 + 5 * (r - 1)));
				struck = true;
			}
		}
		break;
	case SpellID::Crusade: {
		int blows = 0;
		for (Monster *m : MonstersWithin(player.position.tile, 1)) {
			if (m == front || blows >= 3)
				continue;
			Strike(player, *m, DamageType::Physical, Percent(WeaponBlow(player), 75 + 5 * (r - 1)));
			blows++;
			struck = true;
		}
		break;
	}
	case SpellID::AegisSlam:
		for (const Point tile : std::array<Point, 3> { ahead, player.position.tile + Left(player._pdir), player.position.tile + Right(player._pdir) }) {
			Monster *m = FindMonsterAtPosition(tile);
			if (m == nullptr || !Hittable(*m))
				continue;
			if (m != front)
				Strike(player, *m, DamageType::Physical, Percent(WeaponBlow(player), 60 + 5 * (r - 1)));
			Stagger(*m, TicksPerSecond + 2 * (r - 1));
			Shove(*m, player._pdir);
			struck = true;
		}
		break;
	case SpellID::Cleave:
	case SpellID::Sweep:
		for (const Point tile : { player.position.tile + Left(player._pdir), player.position.tile + Right(player._pdir) }) {
			Monster *m = FindMonsterAtPosition(tile);
			if (m == nullptr || m == front || !Hittable(*m))
				continue;
			Strike(player, *m, DamageType::Physical, Percent(WeaponBlow(player), (spell == SpellID::Cleave ? 70 : 80) + 5 * (r - 1)));
			struck = true;
			landedBlows++;
		}
		break;
	case SpellID::Rend:
		if (alive) {
			Bleed(*front, 4 * TicksPerSecond, 3 + 2 * (r - 1));
			struck = true;
		}
		break;
	case SpellID::ClaspOfRuin:
		if (alive) {
			Stagger(*front, TicksPerSecond);
			struck = true;
		}
		break;
	case SpellID::CinderTouch:
		if (alive) {
			MarksOf(*front).burnTicks = 3 * TicksPerSecond;
			MarksOf(*front).burnDamage = (2 + r) << 6;
			struck = true;
		}
		break;
	case SpellID::ReapingPoint: {
		Monster *beyond = InDungeonBounds(ahead + player._pdir) ? FindMonsterAtPosition(ahead + player._pdir) : nullptr;
		if (beyond != nullptr && Hittable(*beyond)) {
			Strike(player, *beyond, DamageType::Physical, Percent(WeaponBlow(player), 100 + 5 * (r - 1)));
			struck = true;
		}
		break;
	}
	case SpellID::TurningPike:
		if (landed) {
			for (const Point side : { player.position.tile + Left(player._pdir), player.position.tile + Right(player._pdir) }) {
				if (InDungeonBounds(side) && !IsTileSolid(side) && dMonster[side.x][side.y] == 0 && dPlayer[side.x][side.y] == 0) {
					TeleportTo(player, side);
					break;
				}
			}
			struck = true;
		}
		break;
	case SpellID::LowBranch:
		if (alive) {
			ChillMonster(*front, 3 * TicksPerSecond);
			struck = true;
		}
		break;
	case SpellID::StaffOfEchoes:
		if (alive && frontDamage > 0) {
			PlayerState &state = StateOf(player);
			state.echoTicks = TicksPerSecond;
			state.echoMonster = static_cast<int>(front->getId());
			state.echoDamage = Percent(frontDamage, 60 + 4 * (r - 1));
			struck = true;
		}
		break;
	case SpellID::TigerClaw:
		if (alive) {
			Bleed(*front, 3 * TicksPerSecond, 2 + r);
			struck = true;
		}
		break;
	case SpellID::PressurePoint:
		if (alive) {
			ChillMonster(*front, 3 * TicksPerSecond);
			DebuffMonster(*front, 6 * TicksPerSecond, 0, -std::min(20 + 2 * (r - 1), 60));
			struck = true;
		}
		break;
	case SpellID::ExplodingPalm:
		if (alive) {
			MarksOf(*front).palmTicks = 6 * TicksPerSecond;
			MarksOf(*front).palmRank = r;
			Bleed(*front, 6 * TicksPerSecond, 1 + r);
			struck = true;
		}
		break;
	default:
		break;
	}

	// The Barbarian settles on any landed blow: Clasp of Ruin's killing blow found no living target to
	// stagger and so earned no Rage, and Cleave earned once however many it cut (2026-09-14).
	if (UsesRage(player)) {
		if (struck || landedBlows > 0)
			Pay(player, spell, landedBlows);
	} else if (struck || (landed && MeleeBonusPercent(spell, r) > 0)) {
		Pay(player, spell);
	}
	return struck;
}

bool CastRfa12Active(Player &player, SpellID spell, Point target)
{
	if (!IsRfa12Active(spell) || IsRfa12Melee(spell))
		return false;
	if (IsBowSkill(spell) && !player.UsesRangedWeapon()) {
		player.Say(HeroSpeech::ICantDoThat);
		return false;
	}
	const int r = RankOf(player, spell);
	PlayerState &state = StateOf(player);
	if (!CastOnce(player, spell, target, r))
		return false;
	// Heroic Couplet: the next Poetry verse takes effect twice.
	if (IsPoetry(spell) && state.heroicCouplet) {
		state.heroicCouplet = false;
		CastOnce(player, spell, target, r);
	}
	return true;
}

bool Rfa12CastLeavesRing(SpellID spell)
{
	return IsAnyOf(spell, SpellID::GroundStomp, SpellID::ThreateningShout, SpellID::RallyingCry, SpellID::Intimidate,
	    SpellID::SplitRanks, SpellID::IronWill, SpellID::Bloodcall, SpellID::EarthshakerCry, SpellID::Thunderclap,
	    SpellID::DeafeningRoar, SpellID::MountainPole, SpellID::BlindingFlash);
}

int Rfa12ActiveDamageDealtPercent(const Player &player, const Monster &target, bool melee)
{
	const Marks &marks = MarksOf(target);
	int percent = 0;
	if (marks.judgmentTicks > 0)
		percent += marks.judgmentPercent;
	if (!melee && marks.huntTicks > 0 && player._pClass == HeroClass::Rogue)
		percent += marks.huntPercent;
	return percent;
}

bool Rfa12ActiveEvadesMelee(const Player &player)
{
	const int r = BuffRank(player, Buff::Evasion);
	return r > 0 && GenerateRnd(100) < std::min(10 + 2 * (r - 1), 40);
}

int Rfa12ActiveAbsorbDamage(Player &player, int damage)
{
	PlayerState &state = StateOf(player);
	if (damage <= 0 || BuffRank(player, Buff::Chord) <= 0 || state.chordPool <= 0)
		return damage;
	const int drunk = std::min(state.chordPool, damage);
	state.chordPool -= drunk;
	if (state.chordPool <= 0)
		state.ticks[static_cast<size_t>(Buff::Chord)] = 0;
	return damage - drunk;
}

bool Rfa12ActiveStripsResistances(const Monster &monster)
{
	return MarksOf(monster).satireTicks > 0;
}

bool Rfa12ActiveArrowIgnores(const Player &player, const Monster &monster)
{
	const PlayerState &state = StateOf(player);
	if (state.claimTicks <= 0 || state.claimMonster < 0 || state.claimMonster == static_cast<int>(monster.getId()))
		return false;
	const Monster &claimed = Monsters[state.claimMonster];
	return (claimed.hitPoints >> 6) > 0 && !monster.isUnique();
}

bool Rfa12ActiveHidesPlayer(const Player &player)
{
	return BuffRank(player, Buff::Astral) > 0;
}

void OnRfa12ActiveHit(Player &player, Monster &monster, int damage, bool melee)
{
	Marks &marks = MarksOf(monster);
	if (melee && marks.oathCharges > 0 && marks.oathTicks > 0 && (monster.hitPoints >> 6) > 0) {
		marks.oathCharges--;
		Strike(player, monster, DamageType::Magic, Rolled(Scale(marks.oathRank, 4, 8, 2, 3)));
	}
	if (marks.tragedyTicks > 0 && damage > 0) {
		const int share = Percent(damage, marks.tragedyPercent);
		const Point at = monster.position.tile;
		const int saved = marks.tragedyTicks;
		marks.tragedyTicks = 0; // what it shares cannot come back to it
		for (Monster *m : MonstersWithin(at, 1)) {
			if (m != &monster)
				Strike(player, *m, DamageType::Magic, share);
		}
		marks.tragedyTicks = saved;
	}
}

void OnRfa12ActiveStruck(Player &player, Monster &monster)
{
	if (const int r = BuffRank(player, Buff::StaticCharge); r > 0)
		Strike(player, monster, DamageType::Lightning, Rolled(Scale(r, 2, 10, 1, 3)));
	if (const int r = BuffRank(player, Buff::Retribution); r > 0 && (monster.hitPoints >> 6) > 0)
		Strike(player, monster, DamageType::Magic, Rolled(Scale(r, 3, 6, 1, 2)));
}

void OnRfa12ActiveMissileStruck(Player &player, Monster &monster, int damage)
{
	if (const int r = BuffRank(player, Buff::Feedback); r > 0 && damage > 0)
		Strike(player, monster, DamageType::Magic, Percent(damage, std::min(30 + 2 * (r - 1), 70)));
}

void OnRfa12ActiveMonsterKilled(Player &player, const Monster &monster)
{
	Marks &marks = MarksOf(monster);
	const Point at = monster.position.tile;
	if (marks.ashenTicks > 0) {
		const int r = marks.ashenRank;
		marks.ashenTicks = 0;
		for (Monster *m : MonstersWithin(at, 1))
			Strike(player, *m, DamageType::Fire, Rolled(Scale(r, 6, 12, 3, 5)));
		Ring(player, at);
	}
	if (marks.palmTicks > 0) {
		const int r = marks.palmRank;
		marks.palmTicks = 0;
		for (Monster *m : MonstersWithin(at, 1))
			Strike(player, *m, DamageType::Physical, Percent(WeaponBlow(player), 50 + 5 * (r - 1)));
		Ring(player, at);
	}
	if (const int r = BuffRank(player, Buff::Bloodcall); r > 0) {
		Heal(player, (3 + (r - 1)) << 6);
		GainRage(player, 2 + (r - 1)); // Rage, not mana: Bloodcall is the Barbarian's (2026-09-13)
	}
	marks = Marks {};
}

void ApplyRfa12ActiveBuffsToTotals(const Player &player, ItemBonusTotals &totals)
{
	if (const int r = BuffRank(player, Buff::IronWill); r > 0) {
		totals.fireResist += 15 + 3 * (r - 1);
		totals.lightningResist += 15 + 3 * (r - 1);
		totals.magicResist += 15 + 3 * (r - 1);
	}
	if (const int r = BuffRank(player, Buff::Conduit); r > 0)
		totals.fastCast += std::min(20 + 2 * (r - 1), 60);
	if (BuffRank(player, Buff::Saga) > 0)
		totals.spellLevelAdd += 2;
	if (BuffRank(player, Buff::Astral) > 0)
		totals.moveSpeed += 50;
}

void ProcessRfa12ActivesTick(Player &player)
{
	if (&player != MyPlayer)
		return;
	PlayerState &state = StateOf(player);

	// Companions keep their own time, on every level and whether or not the owner stands (oracool/companion.h).
	ProcessCompanions(player);
	ProcessNecromancerSummoningTick(player);

	// Buffs run down; a sheet buff that ends takes its numbers with it.
	for (size_t i = 0; i < state.ticks.size(); i++) {
		if (state.ticks[i] > 0 && --state.ticks[i] == 0 && IsSheetBuff(static_cast<Buff>(i)))
			CalcPlrInv(player, false);
	}
	if (player._pHitPoints <= 0 || player._pmode == PM_DEATH)
		return;

	if (BuffRank(player, Buff::Rally) > 0)
		Heal(player, state.rallyPerTick);
	if (const int r = BuffRank(player, Buff::Clarity); r > 0)
		RestoreMana(player, 2 + (r - 1));
	if (const int r = BuffRank(player, Buff::Conduit); r > 0)
		RestoreMana(player, 2 + (r - 1));
	if (const int r = BuffRank(player, Buff::Immolate); r > 0 && state.ticks[static_cast<size_t>(Buff::Immolate)] % TicksPerSecond == 0) {
		const Range d = Scale(r, 3, 6, 1, 2);
		for (Monster *m : MonstersWithin(player.position.tile, 1))
			Strike(player, *m, DamageType::Fire, Rolled(d));
	}
	if (const int r = BuffRank(player, Buff::Spheres); r > 0 && state.ticks[static_cast<size_t>(Buff::Spheres)] % 10 == 0) {
		const Range d = Scale(r, 1, 3, 1, 1);
		for (Monster *m : MonstersWithin(player.position.tile, 2))
			Strike(player, *m, DamageType::Magic, Rolled(d));
	}

	// Wrath of the Heavens: a pillar every 12 ticks on a monster within five tiles.
	if (state.wrathPillars > 0 && ++state.wrathClock % 12 == 0) {
		state.wrathPillars--;
		const auto nearby = MonstersWithin(player.position.tile, 5);
		if (!nearby.empty()) {
			Monster &m = *nearby[static_cast<size_t>(GenerateRnd(static_cast<int>(nearby.size())))];
			const Point at = m.position.tile;
			Strike(player, m, DamageType::Magic, Rolled(Scale(state.wrathRank, 10, 20, 4, 7)));
			Ring(player, at);
		}
	}

	// Funeral Star: stand still until it bursts.
	if (state.funeralTicks > 0) {
		if (player.position.tile != state.funeralFrom) {
			state.funeralTicks = 0;
		} else if (--state.funeralTicks == 0) {
			const Range d = Scale(state.funeralRank, 15, 30, 6, 10);
			for (Monster *m : MonstersWithin(state.funeralTile, 3))
				Strike(player, *m, DamageType::Fire, Rolled(d));
			Ring(player, state.funeralTile);
		}
	}

	if (state.crucibleTicks > 0)
		state.crucibleTicks--;
	if (state.claimTicks > 0)
		state.claimTicks--;

	// Staff of Echoes: the blow lands again.
	if (state.echoTicks > 0 && --state.echoTicks == 0 && state.echoMonster >= 0) {
		Monster &m = Monsters[state.echoMonster];
		if (Hittable(m) && player.position.tile.WalkingDistance(m.position.tile) <= 1)
			Strike(player, m, DamageType::Physical, state.echoDamage);
		state.echoMonster = -1;
	}

	TickLanding(player, state);

	for (Field &field : Fields) {
		if (field.ticksLeft <= 0 || field.owner != player.getId())
			continue;
		field.ticksLeft--;
		TickField(player, field);
	}

	// The marks run down, and the ones that act do.
	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		Monster &m = Monsters[ActiveMonsters[i]];
		Marks &marks = MarksOf(m);
		for (int *ticks : { &marks.judgmentTicks, &marks.oathTicks, &marks.huntTicks, &marks.frostbiteTicks, &marks.tragedyTicks,
		         &marks.satireTicks, &marks.ashenTicks, &marks.palmTicks, &marks.brittleCooldown }) {
			if (*ticks > 0)
				(*ticks)--;
		}
		if (marks.oathTicks == 0)
			marks.oathCharges = 0;
		if (marks.burnTicks > 0) {
			marks.burnTicks--;
			if (marks.burnTicks % TicksPerSecond == 0)
				Strike(player, m, DamageType::Fire, marks.burnDamage);
		}
		if (marks.elegyTicks > 0) {
			marks.elegyTicks--;
			if (marks.elegyTicks % TicksPerSecond == 0)
				Strike(player, m, DamageType::Magic, marks.elegyDamage);
		}
		if (marks.threadTicks > 0 && marks.threadPartner >= 0) {
			marks.threadTicks--;
			Monster &partner = Monsters[marks.threadPartner];
			if (!Hittable(partner) || !Hittable(m)) {
				marks.threadTicks = 0;
			} else if (m.position.tile.WalkingDistance(partner.position.tile) > 3) {
				const Range d = Scale(marks.threadRank, 6, 12, 3, 5);
				MarksOf(partner).threadTicks = 0;
				marks.threadTicks = 0;
				Strike(player, m, DamageType::Magic, Rolled(d));
				Strike(player, partner, DamageType::Magic, Rolled(d));
			}
		}
	}
}

void ClearRfa12ActivesState()
{
	MonsterMarks.fill(Marks {});
	Fields.fill(Field {});
	for (PlayerState &state : Players12) {
		// The hero's own buffs walk down the stairs with him, like the cries'; what he left on the
		// level does not.
		state.echoTicks = 0;
		state.echoMonster = -1;
		state.claimTicks = 0;
		state.claimMonster = -1;
		state.wrathPillars = 0;
		state.funeralTicks = 0;
		state.crucibleTicks = 0;
		state.landingTicks = 0;
	}
}

void ClearRfa12ActivesForMonster(const Monster &monster)
{
	MarksOf(monster) = Marks {};
}

void ClearRfa12ActiveBuffs(Player &player)
{
	// A new game: no companions. They are statics and would otherwise follow one character into the next.
	ForgetCompanions();
	ClearNecromancerSummoningState();
	bool sheetMoved = false;
	PlayerState &state = StateOf(player);
	for (size_t i = 0; i < state.ticks.size(); i++) {
		if (state.ticks[i] > 0 && IsSheetBuff(static_cast<Buff>(i)))
			sheetMoved = true;
	}
	state = PlayerState {};
	if (sheetMoved)
		CalcPlrInv(player, false);
}

int Rfa12BuffTicks(const Player &player, SpellID spell)
{
	// The casts that StartBuff, and the buff each starts - CastOnce's cases, read the other way round.
	Buff buff;
	switch (spell) {
	case SpellID::RallyingCry: buff = Buff::Rally; break;
	case SpellID::IronWill: buff = Buff::IronWill; break;
	case SpellID::Bloodcall: buff = Buff::Bloodcall; break;
	case SpellID::StaticCharge: buff = Buff::StaticCharge; break;
	case SpellID::Conduit: buff = Buff::Conduit; break;
	case SpellID::Immolate: buff = Buff::Immolate; break;
	case SpellID::ChordOfWarding: buff = Buff::Chord; break;
	case SpellID::Feedback: buff = Buff::Feedback; break;
	case SpellID::MusicOfTheSpheres: buff = Buff::Spheres; break;
	case SpellID::Saga: buff = Buff::Saga; break;
	case SpellID::MantraOfClarity: buff = Buff::Clarity; break;
	case SpellID::MantraOfEvasion: buff = Buff::Evasion; break;
	case SpellID::MantraOfRetribution: buff = Buff::Retribution; break;
	case SpellID::AstralProjection: buff = Buff::Astral; break;
	default: return 0;
	}
	return std::max(StateOf(player).ticks[static_cast<size_t>(buff)], 0);
}

int Rfa12FrostbitePercent(const Monster &monster)
{
	return FrostbitePercentOn(monster);
}

const char *Rfa12ActiveDescription(SpellID spell)
{
	if (!IsRfa12Active(spell))
		return "";
	for (size_t i = 0; i < ClassTreeSkillCount; i++) {
		const ClassTreeSkillData &data = GetClassTreeSkillData(static_cast<ClassTreeSkill>(i));
		if (data.spellId == spell)
			return data.description;
	}
	return "";
}

} // namespace devilution::oracool
