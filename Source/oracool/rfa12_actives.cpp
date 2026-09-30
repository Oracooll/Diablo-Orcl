#include "oracool/rfa12_actives.h"
#include "oracool/sat_math.h" // AddPercentSat - the damage passives past int (round 27 audit)

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

#include <fmt/format.h>

#include "automap.h"
#include "dead.h"
#include "engine/backbuffer_state.hpp"
#include "engine.h"
#include "engine/random.hpp"
#include "items.h"
#include "levels/gendung.h"
#include "missiles.h"
#include "monster.h"
#include "oracool/aura_field.h"
#include "oracool/chill.h"
#include "oracool/class_tree.h"
#include "oracool/cold.h"
#include "oracool/companion.h"
#include "oracool/passives.h"
#include "oracool/corpses.h"
#include "oracool/curses.h"
#include "oracool/missile_tint.h"
#include "oracool/necro_summoning.h"
#include "oracool/passives.h"
#include "oracool/rage.h"
#include "oracool/rfa12_effects.h"
#include "oracool/endgame_boss.h" // IsKnockbackImmune - Shove's refusal
#include "oracool/melee_skills.h" // NoteSideSweep - Cleave and Sweep stand the vanilla cleave aside
#include "oracool/skill_sounds.h"
#include "oracool/stat_sheet.h"
#include "oracool/warcries.h"
#include "nthread.h" // ProgressToNextGameTick: Serenity's ring glides between ticks
#include "player.h"
#include "spells.h"
#include "utils/language.h"

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

/** @brief Earthen Might: Rage for every enemy a ground skill strikes. */
constexpr int EarthenMightPerEnemy = 3;

/** @brief Earthen Might (Barbarian, 2026-09-14): 3 Rage for every enemy the ground-splitters strike. */
void EarthenMightRage(Player &player, size_t struck)
{
	if (struck == 0 || !PassiveActive(player, ClassTreeSkill::EarthenMight))
		return;
	NoteRageCombat(player);
	GainRage(player, EarthenMightPerEnemy * static_cast<int>(struck));
}

bool Hittable(const Monster &monster)
{
	// Never in town: town's dMonster holds towner ids, and the Monsters slots they index are the golem bodies and the last
	// floor's first monsters - Chill Touch at Griswold killed a 1-HP golem slot, Ice Needle through Cain struck a stale
	// unique (round 5 audit, v1.12.230). Nor the hero's own companions.
	// Nor a monster Conversion turned to the Paladin's side: Crusade, Aegis Slam, Holy Lance, Wrath and the rest struck
	// his own allies (round 16 audit, v1.12.241).
	return leveltype != DTYPE_TOWN && (monster.hitPoints >> 6) > 0 && !monster.isPlayerMinion() && !IsCompanion(monster)
	    && !IsMonsterConverted(monster) && monster.isPossibleToHit();
}

/** @brief "Uniques shrug it off" - the exemption every stagger in this fork carries. */
bool ShrugsOff(const Monster &monster)
{
	return monster.isUnique() || monster.lesserAffix != LesserUniqueAffix::None || monster.type().type == MT_DIABLO;
}

int FrostbitePercentOn(const Monster &monster);

/**
 * @brief Set while one of the eight bow skills casts or ticks: its strikes are arrows to the Rogue's passives - Night
 * Stalker, Thrill of the Hunt, Archery, Leech, the marks. They counted as spells and fed none of them (round 20 audit).
 */
bool BowStrikeInFlight = false;

struct BowStrikeScope {
	// The value it replaced comes back, not false: a scope opened inside another would end the outer one (round 34 audit).
	explicit BowStrikeScope(bool bow)
	    : previous(BowStrikeInFlight)
	{
		BowStrikeInFlight = bow;
	}
	~BowStrikeScope() { BowStrikeInFlight = previous; }
	bool previous;
};

/** @brief A skill's strike: immunity and resistance honoured, kill credit and the flinch to @p player. */
void Strike(Player &player, Monster &monster, DamageType type, int damage, bool melee = false, bool applyPassives = true)
{
	if (damage <= 0 || !Hittable(monster))
		return;
	if (monster.isImmune(MissileID::Null, type))
		return;
	// Cold: Cold Mastery decides how much of the resistance the monster keeps, and adds its share, as the missile path and
	// ColdSpellDamage do - the six RfA-12 cold skills had neither (round 15 audit, v1.12.240).
	if (monster.isResistant(MissileID::Null, type))
		damage /= type == DamageType::Cold ? ColdResistanceDivisor(player) : 4;
	if (type == DamageType::Cold)
		damage = AddPercentSat(damage, FrostbitePercentOn(monster) + ColdMasteryDamagePercent(player)); // saturating (round 27 audit)
	// The damage-dealt passives, as MonsterMHit and PlrHitMonst apply them: Power Hungry, Conflagration, Spreading
	// Malediction and the rest never reached a skill that strikes through here - 87 callers, the whole RfA-12 book
	// (round 13 audit, v1.12.238). Melee only when the caller says so - a swing's extra blows - as MonsterMHit counts
	// every spell as not melee (round 14 audit: "adjacent is melee" gave spells the melee passives). And not at all for
	// a share of a blow that already took them (the echo, Tragedy's share), which paid them twice.
	// With the RfA-12 half of that sum (Hunter's Mark, Judgment, Dead Ground, Deadeye), which the missile path adds beside
	// the passives' - round 13 brought over only the first half (round 15 audit, v1.12.240).
	if (applyPassives)
		damage = AddPercentSat(damage, PassiveDamageDealtPercent(player, monster, melee) + Rfa12DamageDealtPercent(player, monster, melee));
	if (damage <= 0)
		return;
	// Once every six seconds per enemy, as its text says (round 16) - and only for a strike that lands something (round 17).
	if (applyPassives && !melee)
		SpendDeadGroundIfApplies(player, monster);
	// Life Tap, as a swing and a spell missile feed it: every tree skill strikes through here, and Teeth or Bone Spear on a
	// tapped monster healed nothing (round 20 audit, v1.12.245). Before the damage, while the curse is still live.
	if (applyPassives)
		OnCursedMonsterStruck(monster, player, nullptr, damage);
	ApplyMonsterDamage(type, monster, damage);
	// An arrow's, as MonsterMHit feeds a plain arrow's; and a swing's extra blow (Sweep, Reaping Point), as the class melee
	// strike feeds it - Leech healed nothing from them (round 29 audit).
	if (&player == MyPlayer && applyPassives && (BowStrikeInFlight || melee))
		OnPassiveHit(player, monster, damage, melee);
	// Scent of Blood marks what every skill wounds, not only a plain swing (round 29 audit: no bow, javelin or spear skill
	// ever marked one).
	if (&player == MyPlayer && applyPassives)
		MarkWoundedByScent(player, monster, damage);
	if (&player == MyPlayer && applyPassives)
		OnPassiveMissileHit(player, monster, damage, type, /*arrow=*/BowStrikeInFlight, /*sharedRulesDone=*/melee); // Paralysis, Temporal Flux, the marks
	// None of these cold skills has impact art of its own (Chill Touch, Ice Needle, Ice Lance, Brittle Ground,
	// Whiteout, Absolute Zero): the cold hit flash marks the blow (hit_cold.png, 2026-09-26).
	if (type == DamageType::Cold)
		AddColdHitFlash(monster.position.tile, static_cast<int>(player.getId()));
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
	// Not an Unyielding one: the knockback refuses it, and the turn below was never undone (round 30 audit).
	if (IsKnockbackImmune(monster))
		return;
	// M_GetKnockback moves a monster OPPOSITE to where it faces; face it the other way first - and back, when a wall
	// refuses the move (round 30 audit: it slid and faced the wrong way).
	const Direction facing = monster.direction;
	const Point oldBefore = monster.position.old;
	monster.direction = Opposite(dir);
	M_GetKnockback(monster);
	if (monster.position.old == oldBefore) // refused: M_GetKnockback moves position.old when it takes the shove
		monster.direction = facing;
}

/** @brief Every hittable monster within @p radius tiles of @p centre, gathered before anything is struck. */
std::optional<Point> CastSightFrom; // see below, at NearestTo

/** @brief Lifts the cast's sight gate for a chain's hop or a death's burst, which see from where they are (round 35). */
struct NoCastSight {
	std::optional<Point> saved = CastSightFrom;
	NoCastSight() { CastSightFrom = std::nullopt; }
	~NoCastSight() { CastSightFrom = saved; }
	NoCastSight(const NoCastSight &) = delete;
	NoCastSight &operator=(const NoCastSight &) = delete;
};

std::vector<Monster *> MonstersWithin(Point centre, int radius)
{
	std::vector<Monster *> out;
	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		Monster &monster = Monsters[ActiveMonsters[i]];
		// And in sight of the centre, as Frost Nova since round 3: Absolute Zero froze the next room (round 5 audit).
		// And, while a cast is being made, in the hero's sight too, as NearestTo since round 15: Rain of Arrows, Seven-Sided
		// Strike and Wave of Light struck a pack behind a wall with the cursor on its far side (round 34 audit). The fields
		// that tick after the cast are unset and keep their own centre's sight.
		if (Hittable(monster) && centre.WalkingDistance(monster.position.tile) <= radius
		    && LineClearMissile(centre, monster.position.tile)
		    && (!CastSightFrom || LineClearMissile(*CastSightFrom, monster.position.tile)))
			out.push_back(&monster);
	}
	return out;
}

/** @brief The hittable monster nearest @p centre within @p radius, or null. */
/**
 * @brief While a cast is being made, the hero's tile: NearestTo takes only a monster he can see from it. The target was
 * found around the cursor alone, so a shot or a curse clicked beside a wall struck the monster behind it (round 15
 * audit, v1.12.240 - the check Absolute Zero got in round 5). Unset for the fields and marks that tick after the cast.
 * Declared above MonstersWithin, which asks it too (round 34).
 */

Monster *NearestTo(Point centre, int radius, const Monster *except = nullptr)
{
	Monster *best = nullptr;
	int bestDistance = radius + 1;
	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		Monster &monster = Monsters[ActiveMonsters[i]];
		if (&monster == except || !Hittable(monster))
			continue;
		if (CastSightFrom && !LineClearMissile(*CastSightFrom, monster.position.tile))
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

/** @brief Ticks between Earthquake's rings: five a second, three or four growing at once. */
constexpr int EarthquakeRingTicks = 4;

/**
 * @brief A cry's ring on @p tile grown to reach @p reach tiles (dev notes, 2026-09-29: Earthquake's and Rend's rings
 * "scale them to a size matching the range"). The sheet's ring ends 150px wide; a reach of R tiles is about 90R across,
 * so 60% of the sheet a tile. Its centre stays on the floor, 80px up its 160px cell. Null while the sheet is missing.
 */
Missile *ReachRing(Player &player, Point tile, int reach)
{
	Missile *ring = AddMissile(tile, tile, player._pdir, MissileID::WarcryRing, TARGET_MONSTERS, static_cast<int>(player.getId()), 0, 0);
	if (ring == nullptr || ring->_miDelFlag)
		return nullptr;
	ScaleMissile(*ring, 60 * std::max(reach, 1), 80);
	return ring;
}

Point LineEnd(Point from, Point toward, int length); // below, with the travelling waves

/**
 * @brief Seismic Slam's wave (v1.12.211, user 2026-09-27: "use flamewave effect and asset. send 2-3 flames forward up to
 * the range of Seismic Wave. Tinted gold"): three of vanilla's Flame Wave flames - its Fire Wall graphic, burning - rolling
 * side by side from @p here toward @p target for @p reach tiles. Drawn only; the slam's blow is struck by its caller.
 */
void GoldenFlameWave(const Player &player, Point here, Point target, int reach)
{
	const Direction dir = GetDirection(here, target);
	const Displacement ahead = target - here;
	// Flame Wave's own spread: the centre, and a flame either side at right angles to the line.
	for (const Point start : { here, here + Left(Left(dir)), here + Right(Right(dir)) }) {
		Missile *flame = AddArtBolt(start, LineEnd(start, start + ahead, reach), MissileGraphicID::FireWall, static_cast<int>(player.getId()), 16);
		if (flame == nullptr)
			continue;
		SetMissDir(*flame, 1); // the burning row: Fire Wall's sheet has two (the rise, the burn), not sixteen facings
		flame->oracoolTint = Tint::Hue;
		flame->oracoolTintRgb = Rgb(255, 204, 92);
		// Half size since the Barbarian Skill Cards page (2026-09-29), still burning on the floor: the tile's centre is
		// 16px above the sprite's bottom edge.
		ScaleMissile(*flame, 50, 16);
	}
}

/**
 * @brief Flame Ring's fire (v1.12.211, user 2026-09-27: "use fire wall asset for this one. scale if you need to. as many
 * flame assets as you need"): vanilla Fire Wall flames on every tile of a ring @p radius tiles round @p here, burning for a
 * second. Drawn only; the ring's damage is struck by its caller.
 */
void RingOfFireWall(const Player &player, Point here, int radius)
{
	constexpr int BurnTicks = 20;
	std::vector<Point> placed;
	for (int step = 0; step < 24; step++) {
		const double a = step * 2.0 * 3.14159265358979 / 24.0;
		const Point tile = here + Displacement { static_cast<int>(std::lround(radius * std::cos(a))), static_cast<int>(std::lround(radius * std::sin(a))) };
		if (std::find(placed.begin(), placed.end(), tile) != placed.end() || !InDungeonBounds(tile) || TileHasAny(dPiece[tile.x][tile.y], TileProperties::Solid))
			continue;
		placed.push_back(tile);
		AddArtEffectFacing(tile, MissileGraphicID::FireWall, static_cast<int>(player.getId()), 1, BurnTicks);
	}
	if (placed.empty())
		Ring(const_cast<Player &>(player), here);
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

// ---- RfA-27 (2026-09-26): the skills' own sheets and cues -----------------------------------------------------------
// Every sheet below is drawn only - the blow it pictures is dealt by the code around it - and spawns nothing while it is
// not in the archive (headless tests included), so the stand-in it replaced, where there was one, stays as the fallback.

/** @brief @p spell's tree row for @p player's own cues, or None for anyone but the local player. */
ClassTreeSkill CueRow(const Player &player, SpellID spell)
{
	if (&player != MyPlayer)
		return ClassTreeSkill::None;
	return ClassTreeSkillForSpell(player._pClass, spell);
}

/**
 * @brief @p spell's delivered Impact cue, once for a resolved cast or a landed blow (RfA-27 batch 51) - the local player's
 * own. Nothing when the row has none. No vanilla sound plays at these moments, so it is the moment's only voice.
 */
void Impact(const Player &player, SpellID spell)
{
	const ClassTreeSkill row = CueRow(player, spell);
	if (row != ClassTreeSkill::None)
		PlaySkillSound(row, SkillSoundEvent::Impact);
}

/** @brief A sheet standing on @p tile (AddArtEffect, with its delivery anchor). Null while it is not in the archive. */
Missile *Art(const Player &player, MissileGraphicID art, Point tile, int ticks = 0)
{
	return AddArtEffect(tile, art, static_cast<int>(player.getId()), ticks);
}

/** @brief A sixteen-facing sheet on @p tile, turned to @p dir (an eight-way facing, on the sheet's even rows). */
Missile *ArtFacing(const Player &player, MissileGraphicID art, Point tile, Direction dir, int ticks = 0)
{
	return AddArtEffectFacing(tile, art, static_cast<int>(player.getId()), 2 * static_cast<int>(dir), ticks);
}

/** @brief A sheet on the hero that keeps to him while it plays - a one-shot over the body. */
void ArtOnHero(const Player &player, MissileGraphicID art)
{
	if (Missile *effect = Art(player, art, player.position.tile); effect != nullptr)
		ArtEffectFollowsItsCaster(*effect);
}

/**
 * @brief A flying sheet from @p from to @p to (AddArtBolt). It lands with @p impactSpell's Impact cue and leaves
 * @p arrivalArt standing where it lands; while the sheet is missing both happen at once, here.
 */
void Fly(const Player &player, MissileGraphicID art, Point from, Point to, SpellID impactSpell = SpellID::Invalid, int speed = 32,
    MissileGraphicID arrivalArt = MissileGraphicID::None)
{
	const ClassTreeSkill row = impactSpell != SpellID::Invalid ? CueRow(player, impactSpell) : ClassTreeSkill::None;
	if (AddArtBolt(from, to, art, static_cast<int>(player.getId()), speed, arrivalArt, row) != nullptr)
		return;
	if (impactSpell != SpellID::Invalid)
		Impact(player, impactSpell);
	if (arrivalArt != MissileGraphicID::None)
		Art(player, arrivalArt, to);
}

/** @brief The last open tile of the @p length-tile line from @p from toward @p toward - where a travelling wave stops. */
Point LineEnd(Point from, Point toward, int length)
{
	const std::vector<Point> tiles = LineOfTiles(from, toward, length);
	return tiles.empty() ? from : tiles.back();
}

/** @brief Whether @p spell's row has a cast or impact cue of its own - the teleport's borrowed chime then stays quiet. */
bool HasOwnCue(const Player &player, SpellID spell)
{
	const ClassTreeSkill row = CueRow(player, spell); // the cues are the local player's own; anyone else keeps the chime
	return row != ClassTreeSkill::None && (HasSkillSound(row, SkillSoundEvent::Cast) || HasSkillSound(row, SkillSoundEvent::Impact));
}

/**
 * @brief Moves @p player to @p dst through the engine's Teleport. @p spell names the skill that moved him: when its row
 * has a delivered cue (RfA-27), Teleport's own LS_ELEMENTL stays quiet - one sound per moment, the skill's.
 */
bool TeleportTo(Player &player, Point dst, SpellID spell = SpellID::Invalid)
{
	if (dst == player.position.tile || !InDungeonBounds(dst))
		return false;
	std::optional<_sfx_id> sound;
	if (spell != SpellID::Invalid && HasOwnCue(player, spell))
		sound = SFX_NONE;
	return AddMissile(player.position.tile, dst, player._pdir, MissileID::Teleport, TARGET_MONSTERS,
	           static_cast<int>(player.getId()), 0, 0, nullptr, sound)
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
	// The Necromancer's poison (2026-09-18): the same shape as the burn, in acid.
	int poisonTicks = 0;
	int poisonDamage = 0;
	int elegyTicks = 0;
	int elegyDamage = 0;
	// Each one's own second, which a refresh leaves alone (round 35 audit): the pulse came when the remaining count hit a
	// whole second, so a poison, burn or elegy renewed faster than once a second never struck at all.
	int burnPulse = 0;
	int poisonPulse = 0;
	int elegyPulse = 0;
	int palmTicks = 0;
	int palmRank = 0;
	int threadPartner = -1;
	int threadTicks = 0;
	int threadRank = 0;
	int brittleCooldown = 0;
	int whiteoutStamp = 0;
	// RfA-27's markers (2026-09-26) for the conditions that had no clock of their own here: Anchor Javelin's pin, the
	// slow of Crippling Shot / Low Branch / Pressure Point (the chill every cold spell lays is not this mark), Pressure
	// Point's broken armour and Decompose's rot. They only draw a sigil; the conditions themselves are unchanged.
	int pinTicks = 0;
	int slowMarkTicks = 0;
	int armourBreakTicks = 0;
	int rotTicks = 0;
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
	Venom,     // Poison Dagger: every landed blow poisons
	BoneShell, // Bone Armor: bonePool absorbs
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
	int bonePool = 0; // Bone Armor, in 1/64 points
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
	// Serenity: the ring of light still rising and falling round her (drawn only; the cure was at the cast).
	int serenityTicks = 0;
};

/** @brief Serenity's ring (v1.12.211): two seconds, up from the feet over the head and back down. */
constexpr int SerenityTicks = 2 * TicksPerSecond;

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

// ---- one place each (2026-09-26): the cast reads these and so does Rfa12ActiveFactsAt, so the rule and ----
// ---- its tooltip cannot disagree. The hidden Bard's rows keep their inline numbers. ----------------------

/** @brief The whole points a skill rolls, before resistances. {0, 0} for a skill that rolls none. */
Range SkillDamage(SpellID spell, int r)
{
	switch (spell) {
	case SpellID::Oathbrand: return Scale(r, 4, 8, 2, 3); // each branded blow
	case SpellID::HeavensDescent: return Scale(r, 8, 16, 4, 6);
	case SpellID::WrathOfTheHeavens: return Scale(r, 10, 20, 4, 7); // each pillar
	case SpellID::EarthshakerCry: return Scale(r, 5, 10, 3, 5);
	case SpellID::ChillTouch: return Scale(r, 3, 6, 2, 3);
	case SpellID::IceNeedle: return Scale(r, 5, 9, 3, 4);
	case SpellID::IceLance: return Scale(r, 6, 11, 3, 5);
	case SpellID::BrittleGround: return Scale(r, 4, 8, 2, 3);
	case SpellID::Whiteout: return Scale(r, 5, 10, 3, 4);
	case SpellID::AbsoluteZero: return Scale(r, 8, 16, 4, 6);
	case SpellID::Arc: return Scale(r, 2, 12, 2, 5); // the first strike
	case SpellID::StaticCharge: return Scale(r, 2, 10, 1, 3);
	case SpellID::LightningRod: return Scale(r, 4, 14, 2, 5);
	case SpellID::StormCrucible: return Scale(r, 4, 16, 2, 6);
	case SpellID::RideTheLightning: return Scale(r, 3, 12, 2, 4);
	case SpellID::EmberMine: return Scale(r, 6, 12, 3, 5);
	case SpellID::FlameRing: return Scale(r, 4, 8, 2, 3);
	case SpellID::AshenBrand: return Scale(r, 6, 12, 3, 5);
	case SpellID::FurnaceMouth: return Scale(r, 4, 9, 2, 4);
	case SpellID::Immolate: return Scale(r, 3, 6, 1, 2); // a second
	case SpellID::FuneralStar: return Scale(r, 15, 30, 6, 10);
	case SpellID::ShockArrow: return Scale(r, 1, 6, 1, 3); // the arc
	case SpellID::Meteor: return Scale(r, 20, 40, 8, 12); // the impact
	case SpellID::PoisonJavelin: return Scale(r, 2, 4, 1, 1); // the pool, a second
	case SpellID::PlagueJavelin: return Scale(r, 4, 8, 2, 3); // the cloud, a second
	case SpellID::ChiWave: return Scale(r, 4, 8, 2, 3);
	case SpellID::MantraOfRetribution: return Scale(r, 3, 6, 1, 2);
	case SpellID::WaveOfLight: return Scale(r, 7, 14, 3, 5);
	case SpellID::AncestralCourt: return Scale(r, 5, 9, 2, 4); // each strike
	case SpellID::Teeth: return Scale(r, 2, 5, 1, 2); // each tooth
	case SpellID::BoneSplinters: return Scale(r, 3, 6, 1, 2);
	case SpellID::BoneWall: return Scale(r, 3, 6, 1, 2);
	case SpellID::BoneSpikes: return Scale(r, 4, 9, 2, 3);
	case SpellID::BoneSpear: return Scale(r, 6, 12, 3, 4);
	case SpellID::BonePrison: return Scale(r, 2, 5, 1, 2);
	case SpellID::BoneStorm: return Scale(r, 2, 4, 1, 1);
	case SpellID::DeathNova: return Scale(r, 6, 12, 2, 4);
	default: return { 0, 0 };
	}
}

/** @brief Meteor's burning ground, a second. */
Range MeteorBurn(int r)
{
	return Scale(r, 3, 6, 1, 2);
}

/** @brief A skill's share of one weapon blow, in percent. 0 for a skill that deals none. */
int BlowPercent(SpellID spell, int r)
{
	const int p = r - 1;
	switch (spell) {
	case SpellID::HolyLance: return 80 + 5 * p;
	case SpellID::Crusade: return 75 + 5 * p;
	case SpellID::AegisSlam: return 60 + 5 * p; // the two beside the target
	case SpellID::Cleave: return 70 + 5 * p;
	case SpellID::Sweep: return 80 + 5 * p;
	case SpellID::Backhand:
	case SpellID::RearwardReach: return 100 + 8 * p;
	case SpellID::SeismicSlam: return 80 + 10 * p;
	case SpellID::Earthquake: return 30 + 5 * p; // a second
	case SpellID::BarbedShaft: return 100 + 5 * p;
	case SpellID::ShockArrow:
	case SpellID::CripplingShot: return 100;
	case SpellID::PiercingShot: return 80 + 5 * p;
	case SpellID::RainOfArrows: return 50 + 4 * p;
	case SpellID::Barrage: return 40 + 3 * p; // each arrow
	case SpellID::PhantomVolley: return 40 + 3 * p;
	case SpellID::Harpoon: return 60 + 5 * p;
	case SpellID::ReapingPoint: return 100 + 5 * p; // the enemy beyond
	case SpellID::AnchorJavelin: return 70 + 5 * p;
	case SpellID::PoisonJavelin: return 60 + 5 * p;
	case SpellID::ValkyriesSpear: return 150 + 15 * p;
	case SpellID::LongThrust: return 100 + 8 * p;
	case SpellID::MountainPole: return 50 + 5 * p;
	case SpellID::BambooRain: return 70 + 5 * p;
	case SpellID::DragonTailSweep: return 60 + 5 * p;
	case SpellID::WhirlingKick: return 70 + 5 * p;
	case SpellID::StaffOfEchoes: return 60 + 4 * p; // of the blow that landed
	case SpellID::HeavenSplitter: return 120 + 10 * p;
	case SpellID::ThousandReeds: return 40 + 3 * p;
	case SpellID::LeapingCrane: return 80 + 5 * p;
	case SpellID::SevenSidedStrike: return 60 + 4 * p;
	case SpellID::ExplodingPalm: return 50 + 5 * p; // the burst
	case SpellID::DragonsWrath: return 100 + 8 * p;
	default: return 0;
	}
}

/** @brief How long what a skill leaves behind lasts - buff, mark, debuff, bleed, burn or ground - in ticks. */
int EffectTicks(SpellID spell, int r)
{
	const int p = r - 1;
	switch (spell) {
	case SpellID::Judgment: return 4 * TicksPerSecond;
	case SpellID::Oathbrand: return 6 * TicksPerSecond;
	case SpellID::Rend: return 4 * TicksPerSecond;
	case SpellID::Earthquake: return 4 * TicksPerSecond;
	case SpellID::ThreateningShout:
	case SpellID::Intimidate: return (10 + p) * TicksPerSecond;
	case SpellID::RallyingCry: return 5 * TicksPerSecond;
	case SpellID::IronWill: return (20 + 2 * p) * TicksPerSecond;
	case SpellID::Bloodcall: return 10 * TicksPerSecond;
	case SpellID::Frostbite: return 6 * TicksPerSecond;
	case SpellID::BrittleGround: return 6 * TicksPerSecond;
	case SpellID::FrozenSentinel: return 15 * TicksPerSecond;
	case SpellID::StaticCharge: return (20 + 2 * p) * TicksPerSecond;
	case SpellID::Conduit: return 10 * TicksPerSecond;
	case SpellID::LightningRod: return 12 * TicksPerSecond;
	case SpellID::FaradayRing: return 4 * TicksPerSecond;
	case SpellID::StormCrucible: return 8 * TicksPerSecond; // to place the pair
	case SpellID::CinderTouch: return 3 * TicksPerSecond;
	case SpellID::EmberMine: return 20 * TicksPerSecond;
	case SpellID::AshenBrand: return 4 * TicksPerSecond;
	case SpellID::Firestorm: return 4 * TicksPerSecond;
	case SpellID::Immolate: return 10 * TicksPerSecond;
	case SpellID::FuneralStar: return 2 * TicksPerSecond; // standing still
	case SpellID::BarbedShaft: return 3 * TicksPerSecond;
	case SpellID::HuntersMark: return (10 + p) * TicksPerSecond;
	case SpellID::HuntersClaim: return 8 * TicksPerSecond;
	case SpellID::Meteor: return 4 * TicksPerSecond; // a second to fall, three to burn
	case SpellID::PoisonJavelin: return 3 * TicksPerSecond;
	case SpellID::PlagueJavelin: return 5 * TicksPerSecond;
	case SpellID::StaffOfEchoes: return TicksPerSecond; // until the echo
	case SpellID::TigerClaw: return 3 * TicksPerSecond;
	case SpellID::PressurePoint: return 6 * TicksPerSecond; // the armour
	case SpellID::ExplodingPalm: return 6 * TicksPerSecond;
	case SpellID::MantraOfClarity:
	case SpellID::MantraOfEvasion:
	case SpellID::MantraOfRetribution: return 30 * TicksPerSecond;
	case SpellID::AstralProjection: return 6 * TicksPerSecond;
	case SpellID::BoneArmor: return 60 * TicksPerSecond;
	case SpellID::PoisonDagger: return 20 * TicksPerSecond;
	case SpellID::Blight: return 4 * TicksPerSecond;
	case SpellID::BoneWall: return 8 * TicksPerSecond;
	case SpellID::BonePrison: return 3 * TicksPerSecond;
	case SpellID::BoneStorm: return 8 * TicksPerSecond;
	default: return 0;
	}
}

/** @brief How long a skill's stun (or hold) keeps its target, in ticks. */
int StunTicks(SpellID spell, int r)
{
	const int p = r - 1;
	switch (spell) {
	case SpellID::AegisSlam: return TicksPerSecond + 2 * p;
	case SpellID::GroundStomp: return 30 + 4 * p;
	case SpellID::ClaspOfRuin: return TicksPerSecond;
	case SpellID::EarthshakerCry: return 2 * TicksPerSecond;
	case SpellID::Harpoon: return TicksPerSecond / 2;
	case SpellID::AnchorJavelin: return 2 * TicksPerSecond;
	case SpellID::MountainPole: return TicksPerSecond;
	case SpellID::ShoulderGate: return TicksPerSecond;
	case SpellID::BoneSpikes: return TicksPerSecond;
	case SpellID::BonePrison: return 3 * TicksPerSecond;
	default: return 0;
	}
}

/** @brief How long a skill's chill (half speed) - or Absolute Zero's freeze - lasts, in ticks. */
int SlowTicks(SpellID spell, int r)
{
	const int p = r - 1;
	switch (spell) {
	case SpellID::ChillTouch: return 40 + 4 * p;
	case SpellID::IceNeedle:
	case SpellID::IceLance:
	case SpellID::Whiteout: return 40;
	case SpellID::Frostbite: return 6 * TicksPerSecond;
	case SpellID::BrittleGround: return TicksPerSecond;
	case SpellID::AbsoluteZero: return 2 * TicksPerSecond;
	case SpellID::CripplingShot: return 4 * TicksPerSecond;
	case SpellID::LowBranch:
	case SpellID::PressurePoint: return 3 * TicksPerSecond;
	default: return 0;
	}
}

/** @brief A bleed's, burn's or poison's whole points a second, before Virulence. */
int PerSecond(SpellID spell, int r)
{
	const int p = r - 1;
	switch (spell) {
	case SpellID::Rend: return 3 + 2 * p;
	case SpellID::CinderTouch:
	case SpellID::BarbedShaft:
	case SpellID::TigerClaw:
	case SpellID::PoisonDagger:
	case SpellID::Blight:
	case SpellID::PoisonNova:
	case SpellID::DeathNova: return 2 + r;
	case SpellID::ExplodingPalm: return 1 + r;
	case SpellID::PoisonExplosion: return 3 + r;
	case SpellID::Decompose: return 5 + 2 * r;
	default: return 0;
	}
}

/** @brief How long one of the Necromancer's poisons lasts on what it touches, in ticks, before Virulence. */
int PoisonTicks(SpellID spell)
{
	switch (spell) {
	case SpellID::PoisonDagger: return 4 * TicksPerSecond;
	case SpellID::Blight: return 2 * TicksPerSecond; // renewed every second the monster stands in the pool
	case SpellID::PoisonExplosion:
	case SpellID::PoisonNova: return 6 * TicksPerSecond;
	case SpellID::Decompose:
	case SpellID::DeathNova: return 5 * TicksPerSecond;
	default: return 0;
	}
}

/** @brief A skill's main percent that is not a share of a blow: a mark, a cut, a heal, a resistance, a chance. */
int EffectPercent(SpellID spell, int r)
{
	const int p = r - 1;
	switch (spell) {
	case SpellID::Judgment: return std::min(15 + p, 40); // damage taken
	case SpellID::ThreateningShout: return std::min(15 + p, 40); // damage dealt, cut
	case SpellID::RallyingCry: return std::min(20 + 2 * p, 50); // of maximum life
	case SpellID::Intimidate: return std::min(15 + 2 * p, 60); // armour, cut
	case SpellID::IronWill: return 15 + 3 * p; // fire, lightning, cold and magic resistance
	case SpellID::Frostbite: return std::min(20 + 2 * p, 60); // cold damage taken
	case SpellID::Conduit: return std::min(20 + 2 * p, 60); // faster cast rate
	case SpellID::HuntersMark: return std::min(20 + 2 * p, 60); // arrow damage taken
	case SpellID::PressurePoint: return std::min(20 + 2 * p, 60); // armour, cut
	case SpellID::MantraOfEvasion: return std::min(10 + 2 * p, 40); // melee blows that miss
	case SpellID::AstralProjection: return 50; // movement speed
	case SpellID::CorpseExplosion: return CorpseBurstPercent(r); // of the corpse's life - Death Mark's burst too (curses.h)
	default: return 0;
	}
}

/** @brief How far a skill reaches - its radius, line, leap or search - in tiles. */
int ReachTiles(SpellID spell, int r)
{
	const int p = r - 1;
	switch (spell) {
	case SpellID::Rend: return 3; // a cast round him since 2026-09-29
	case SpellID::HeavensDescent: return 6;
	case SpellID::WrathOfTheHeavens: return 5;
	case SpellID::SeismicSlam: return 5;
	case SpellID::Earthquake: return 3;
	case SpellID::ThreateningShout:
	case SpellID::Intimidate: return AuraRadiusForPoints(r); // earshot
	case SpellID::SplitRanks: return 3;
	case SpellID::EarthshakerCry: return 8;
	case SpellID::IceNeedle:
	case SpellID::IceLance: return 8;
	case SpellID::AbsoluteZero: return 8;
	case SpellID::FrozenSentinel: return 8;
	case SpellID::RideTheLightning: return 6;
	case SpellID::FlameRing: return 2;
	case SpellID::FuneralStar: return 3;
	case SpellID::PiercingShot: return 10;
	case SpellID::RainOfArrows: return 2;
	case SpellID::PhantomVolley: return 6;
	case SpellID::Harpoon: return 8;
	case SpellID::Vault: return std::min(3 + p / 5, 6);
	case SpellID::AnchorJavelin: return 8;
	case SpellID::PoisonJavelin: return 8;
	case SpellID::PlagueJavelin: return 2; // the cloud
	case SpellID::ValkyriesSpear: return 1;
	case SpellID::LongThrust: return 2;
	case SpellID::BambooRain: return 2;
	case SpellID::HeavenSplitter: return 4;
	case SpellID::ThousandReeds: return 6;
	case SpellID::LeapingCrane: return std::min(4 + p / 4, 7);
	case SpellID::ShoulderGate: return 2;
	case SpellID::SevenSidedStrike: return 4;
	case SpellID::DragonsWrath: return 8;
	case SpellID::BlindingFlash: return 3;
	case SpellID::WaveOfLight: return 2;
	case SpellID::AncestralCourt: return 2;
	case SpellID::BoneSpear: return 9;
	case SpellID::PoisonNova: return 5;
	case SpellID::DeathNova: return 4;
	case SpellID::Arc:
	case SpellID::ChiWave: return 3; // each leap
	case SpellID::ShockArrow: return 3; // the arc
	case SpellID::ShadowStep: return 2; // from the cursor
	case SpellID::CorpseExplosion:
	case SpellID::PoisonExplosion: return 2; // around the corpse
	case SpellID::BoneSpikes: return 1;
	case SpellID::BoneStorm: return 2;
	default: return 0;
	}
}

// The counts and clocks the facts quote.
constexpr int CrusadeOthers = 3;
constexpr int OathbrandCharges = 3;
constexpr int WrathPillars = 5;
constexpr int WrathPillarTicks = 12;
constexpr int ArcHops = 3;
constexpr int ArcFalloffPercent = 75;
constexpr int ChiWaveHops = 4;
constexpr int BarrageArrows = 5;
constexpr int BambooRainTargets = 3;
constexpr int IceNeedleTargets = 2;
constexpr int BoneSplinterTargets = 3;
constexpr int WhiteoutStepTicks = 6;
constexpr int WhiteoutTicks = 8 * WhiteoutStepTicks; // eight tiles
constexpr int BallLightningStepTicks = 10;
constexpr int BallLightningTicks = 8 * BallLightningStepTicks; // eight tiles, a charged bolt at each
constexpr int FrozenSentinelPeriod = 30;
constexpr int FirestormPeriod = 8;
constexpr int FirestormScatter = 3; // tiles either way of the cursor
constexpr int FurnaceMouthTicks = 3 * TicksPerSecond + 1;
constexpr int FurnaceMouthTiles = 3;
constexpr int CruciblePeriod = 15;
constexpr int CrucibleTicks = 3 * CruciblePeriod; // three runs
constexpr int AncestralCourtTicks = TicksPerSecond + 15;
constexpr int AncestralCourtStrikes = 3; // at one second, and five and ten ticks after
constexpr int MeteorRadius = 2;
constexpr int MeteorBurnRadius = 1;
constexpr int LightningRodBurstRadius = 2;
constexpr int FaradayRingReach = 2;
constexpr int BoneWallPeriod = TicksPerSecond / 2;
constexpr int BoneStormPeriod = TicksPerSecond / 2;

/** @brief Seven-Sided Strike's enemies. */
int SevenSidedTargets(int r)
{
	return std::min(3 + (r - 1) / 3, 7);
}

/** @brief Bloodcall: life (whole points) and Rage every kill. */
int BloodcallLife(int r)
{
	return 3 + (r - 1);
}
int BloodcallRage(int r)
{
	return 2 + (r - 1);
}

/** @brief Conduit's and the Mantra of Clarity's mana, a tick, in 1/64 points. */
int ManaFlowPerTick(int r)
{
	return 2 + (r - 1);
}

/** @brief How far Teeth's line reaches, in tiles. */
constexpr int TeethLineTiles = 6;

/** @brief Teeth: the teeth that fly on down the line, past the three of the fan - no more than the line has tiles. */
int TeethDownTheLine(int r)
{
	return std::min(2 + r, TeethLineTiles);
}

/** @brief Bone Armor's shell, in whole points. */
int BoneArmorPool(int r)
{
	return 20 + 10 * r;
}

/** @brief Corpse Explosion's burst floor and ceiling, whole points. */
constexpr int CorpseBurstMin = 4;
constexpr int CorpseBurstMax = 400;

/** @brief The Necromancer's Serration (+5% a tile flown, to +50%) and Rigor Mortis (a second's chill). */
constexpr int SerrationPerTile = 5;
constexpr int SerrationCap = 50;
constexpr int RigorMortisTicks = TicksPerSecond;

/** @brief Marrow, +8% a point, and Virulence, +25% longer and +10% deeper a point. */
int MarrowBonusPercent(int points)
{
	return 8 * points;
}
int VirulenceLongerPercent(int points)
{
	return 25 * points;
}
int VirulenceDeeperPercent(int points)
{
	return 10 * points;
}

/** @brief The number of ticks in (first, first + ticks] that fall on @p period - how often a clocked field pulses. */
constexpr int PulseCount(int firstClock, int ticks, int period)
{
	return (firstClock + ticks) / period - firstClock / period;
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
	case SpellID::Backhand: // a swing since 2026-09-27 - see ApplyRfa12MeleeOnSwing
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

// =================================================================================================
// The Necromancer's Poison & Bone page (2026-09-18, phase N6)
// =================================================================================================

/** @brief Marrow: every bone skill +8% a point. */
int MarrowPercent(const Player &player)
{
	if (!IsClassTreeSkillUnlocked(player, ClassTreeSkill::Marrow))
		return 100;
	return 100 + MarrowBonusPercent(ClassTreeInvestment(player, ClassTreeSkill::Marrow));
}

/** @brief Virulence: poisons +25% longer and +10% deeper a point. */
int VirulencePoints(const Player &player)
{
	return IsClassTreeSkillUnlocked(player, ClassTreeSkill::Virulence) ? ClassTreeInvestment(player, ClassTreeSkill::Virulence) : 0;
}

/** @brief A poison's length (ticks) and depth (1/64 points a second) once @p player's Virulence has had its say. */
struct PoisonDose {
	int ticks;
	int perSecond64;
};

PoisonDose VirulentDose(const Player &player, int ticks, int perSecond)
{
	const int v = VirulencePoints(player);
	// In 1/64 points, the unit the poison ticks in: in whole points "+10%" of a 3-a-second poison was 0.3, dropped, so
	// Virulence 1-3 added nothing to every rank-1 poison (round 29 audit).
	const int perSecond64 = perSecond << 6;
	return { ticks + ticks * VirulenceLongerPercent(v) / 100, perSecond64 + perSecond64 * VirulenceDeeperPercent(v) / 100 };
}

/** @brief A bone skill's roll through Marrow, as BoneStrike deals it (Serration's per-tile share aside). */
Range BoneRange(const Player &player, Range d)
{
	const int percent = MarrowPercent(player);
	return { d.min * percent / 100, d.max * percent / 100 };
}

/** @brief A bone skill's blow: magic, through Marrow - and Serration (+5% a tile flown, 50% at most) and Rigor Mortis (a second's chill). */
void BoneStrike(Player &player, Monster &monster, int damage)
{
	int percent = MarrowPercent(player);
	if (PassiveActive(player, ClassTreeSkill::Serration))
		percent += std::min(SerrationPerTile * player.position.tile.WalkingDistance(monster.position.tile), SerrationCap);
	Strike(player, monster, DamageType::Magic, damage * percent / 100);
	if ((monster.hitPoints >> 6) > 0 && PassiveActive(player, ClassTreeSkill::RigorMortis))
		ChillMonster(monster, RigorMortisTicks);
	// The bone-hit burst (batch 38) where the blow landed; nothing while the sheet is not in the archive.
	Show(player, MissileID::BoneHitBurst, MissileGraphicID::BoneHitNecro, monster.position.tile, monster.position.tile);
}

/**
 * @brief Poisons @p monster for @p ticks at @p perSecond whole points a second, through Virulence. A stronger poison
 * replaces a weaker; a weaker one only extends the clock.
 */
void Poison(Player &player, Monster &monster, int ticks, int perSecond)
{
	if (!Hittable(monster) || monster.isImmune(MissileID::Null, DamageType::Acid))
		return;
	const PoisonDose dose = VirulentDose(player, ticks, perSecond);
	Marks &marks = MarksOf(monster);
	marks.poisonTicks = std::max(marks.poisonTicks, dose.ticks);
	marks.poisonDamage = std::max(marks.poisonDamage, dose.perSecond64);
}

/** @brief A drawn bolt (batch 38) from the hero to @p to; it removes itself while its sheet is not in the archive. */
Missile *Bolt(Player &player, MissileID bolt, Point to)
{
	const Point here = player.position.tile;
	if (to == here)
		to = here + player._pdir;
	return AddMissile(here, to, GetDirection(here, to), bolt, TARGET_MONSTERS, static_cast<int>(player.getId()), 0, 0);
}

/**
 * @brief RfA-27: @p spell's Impact cue as @p bolt lands (ProcessAcidJavelin reads var3), or at once when the bolt was not
 * drawn - its sheet missing, or no room for it.
 */
void ImpactOnLanding(const Player &player, Missile *bolt, SpellID spell)
{
	const ClassTreeSkill row = CueRow(player, spell);
	if (row == ClassTreeSkill::None)
		return;
	if (bolt != nullptr && !bolt->_miDelFlag)
		bolt->var3 = static_cast<int>(row) + 1;
	else
		PlaySkillSound(row, SkillSoundEvent::Impact);
}

/** @brief A corpse skill's burst: the corpse within reach of the cursor, taken, or nothing. */
std::optional<Corpse> BurstCorpse(Player &player, Point target)
{
	// Not a corpse the hero cannot see: it was spent and the burst struck no one (round 37 audit).
	if (CastSightFrom && !LineClearMissile(*CastSightFrom, target)) {
		player.Say(HeroSpeech::ICantDoThat);
		return std::nullopt;
	}
	std::optional<Corpse> corpse = TakeCorpseNear(target, 3, /*forRevive=*/false);
	if (!corpse)
		player.Say(HeroSpeech::ICantDoThat);
	else
		OnPassiveCorpseConsumed(player);
	return corpse;
}

bool CastOnce(Player &player, SpellID spell, Point target, int r)
{
	const Point here = player.position.tile;
	const int earshot = AuraRadiusForPoints(r);

	switch (spell) {
	// ---------------- Paladin ----------------
	case SpellID::HeavensDescent: {
		const Point dst = Clamped(here, target, ReachTiles(spell, r));
		if (!TeleportTo(player, dst, spell))
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
		state.wrathPillars = WrathPillars;
		state.wrathClock = 0;
		state.wrathRank = r;
		return true;
	}
	// ---------------- Barbarian ----------------
	case SpellID::RearwardReach: {
		Monster *behind = FindMonsterAtPosition(here + Opposite(player._pdir));
		if (behind == nullptr || !Hittable(*behind))
			return false;
		Strike(player, *behind, DamageType::Physical, Percent(WeaponBlow(player), BlowPercent(spell, r)));
		// RfA-27 batch 53: the arc behind him - row n is the arc behind a hero facing n, so his own facing picks it.
		ArtFacing(player, MissileGraphicID::RearwardReach, here, player._pdir);
		Impact(player, spell);
		return true;
	}
	case SpellID::GroundStomp: {
		const auto around = MonstersWithin(here, 1);
		for (Monster *m : around)
			Stagger(*m, StunTicks(spell, r));
		EarthenMightRage(player, around.size());
		// RfA-27 batch 55: the cracked ring at his feet, in place of the cry's shockwave (Rfa12CastLeavesRing). On every
		// stomp, anything in reach or not, and at twice its size (dev notes, 2026-09-29); its floor point is 29px up.
		if (Missile *ring = Art(player, MissileGraphicID::GroundStomp, here); ring != nullptr)
			ScaleMissile(*ring, 200, 29);
		if (around.empty())
			return false;
		Impact(player, spell); // its Impact cue, when it stuns anything (the Barbarian Skill Cards page, 2026-09-29)
		return true;
	}
	case SpellID::Rend: {
		// A spell since the dev notes of 2026-09-29, D3's shape: "rend to play magic cast sprite animation and to apply to
		// all mobs within range as a curse. use the war cry anymation as visual effect when rend cast and increase its scale
		// to the range rend affects mobs". Everything within reach bleeds for the skill's four seconds; the ring, blood
		// red and grown to that reach, goes out whether anything is there or not.
		if (Missile *ring = ReachRing(player, here, ReachTiles(spell, r)); ring != nullptr) {
			ring->oracoolTint = Tint::HueCycle;
			ring->oracoolTintRgb = hue::Infrared;
		}
		const auto torn = MonstersWithin(here, ReachTiles(spell, r));
		for (Monster *m : torn)
			Bleed(*m, EffectTicks(spell, r), PerSecond(spell, r));
		if (torn.empty())
			return false;
		Impact(player, spell);
		return true;
	}
	case SpellID::SeismicSlam: {
		const auto line = MonstersOnLine(here, target, ReachTiles(spell, r));
		for (Monster *m : line)
			Strike(player, *m, DamageType::Physical, Percent(WeaponBlow(player), BlowPercent(spell, r)));
		EarthenMightRage(player, line.size());
		GoldenFlameWave(player, here, target, ReachTiles(spell, r));
		return !line.empty(); // a slam on nothing costs nothing, as Rend and Ground Stomp (round 28 audit)
	}
	case SpellID::Earthquake: {
		Field *f = NewField(player, spell, here, EffectTicks(spell, r), r);
		f->clock = TicksPerSecond - 1;
		// Drawn as cry rings, one after another for the four seconds (TickField), sized to its reach (dev note,
		// 2026-09-29: "replace current animation with war cray animation, but release wave one after the other rapidly for
		// four seconds"). The v1.12.223 spinning flare is gone with it.
		return true;
	}
	case SpellID::ThreateningShout: {
		const auto heard = MonstersWithin(here, ReachTiles(spell, r));
		for (Monster *m : heard)
			DebuffMonster(*m, EffectTicks(spell, r), -EffectPercent(spell, r), 0);
		return !heard.empty();
	}
	case SpellID::RallyingCry: {
		PlayerState &state = StateOf(player);
		const int total = Percent(player._pMaxHP, EffectPercent(spell, r));
		state.rallyPerTick = std::max(total / EffectTicks(spell, r), 1);
		StartBuff(player, Buff::Rally, EffectTicks(spell, r), r);
		return true;
	}
	case SpellID::Intimidate: {
		const auto heard = MonstersWithin(here, ReachTiles(spell, r));
		for (Monster *m : heard)
			DebuffMonster(*m, EffectTicks(spell, r), 0, -EffectPercent(spell, r));
		return !heard.empty();
	}
	case SpellID::SplitRanks: {
		const Direction dir = target == here ? player._pdir : GetDirection(here, target);
		bool any = false;
		for (Monster *m : MonstersWithin(here, ReachTiles(spell, r))) {
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
		StartBuff(player, Buff::IronWill, EffectTicks(spell, r), r);
		return true;
	case SpellID::Bloodcall:
		StartBuff(player, Buff::Bloodcall, EffectTicks(spell, r), r);
		return true;
	case SpellID::AncestralCall:
	case SpellID::SpiritGuardian:
		// Companions (user, 2026-09-14) - the Ancients together, and the Monk's guardian. oracool/companion.h.
		return SummonCompanions(player, spell, target, r);
	case SpellID::EarthshakerCry: {
		const auto heard = MonstersWithin(here, ReachTiles(spell, r));
		const Range d = SkillDamage(spell, r);
		for (Monster *m : heard) {
			Strike(player, *m, DamageType::Magic, Rolled(d));
			Stagger(*m, StunTicks(spell, r));
		}
		return true;
	}
	// ---------------- Sorceress: cold ----------------
	case SpellID::ChillTouch: {
		bool any = false;
		const Range d = SkillDamage(spell, r);
		// Once each: a walking monster stands on two tiles of the fan and was struck twice (round 5 audit).
		std::vector<const Monster *> chilled;
		for (const Point tile : FrontArc(player, target)) {
			Monster *m = FindMonsterAtPosition(tile);
			if (m == nullptr || !Hittable(*m) || std::find(chilled.begin(), chilled.end(), m) != chilled.end())
				continue;
			chilled.push_back(m);
			Strike(player, *m, DamageType::Cold, Rolled(d));
			if ((m->hitPoints >> 6) > 0)
				ChillMonster(*m, SlowTicks(spell, r));
			any = true;
		}
		// RfA-27 batch 53: the cone of frost mist through the three tiles ahead.
		if (any)
			ArtFacing(player, MissileGraphicID::ChillTouch, here, target == here ? player._pdir : GetDirection(here, target));
		return any;
	}
	case SpellID::IceNeedle: {
		auto line = MonstersOnLine(here, target, ReachTiles(spell, r));
		if (line.size() > static_cast<size_t>(IceNeedleTargets))
			line.resize(static_cast<size_t>(IceNeedleTargets));
		const Range d = SkillDamage(spell, r);
		for (Monster *m : line) {
			Strike(player, *m, DamageType::Cold, Rolled(d));
			if ((m->hitPoints >> 6) > 0)
				ChillMonster(*m, SlowTicks(spell, r));
		}
		// RfA-27 batch 54: the needle, to the farthest it struck, landing with its impact cue (at once while the sheet is missing).
		if (!line.empty())
			Fly(player, MissileGraphicID::IceNeedle, here, line.back()->position.tile, spell);
		return !line.empty();
	}
	case SpellID::Frostbite: {
		Monster *m = NearestTo(target, 2);
		if (m == nullptr)
			return false;
		Marks &marks = MarksOf(*m);
		marks.frostbiteTicks = EffectTicks(spell, r);
		marks.frostbitePercent = EffectPercent(spell, r);
		ChillMonster(*m, SlowTicks(spell, r));
		AddColdHitFlash(m->position.tile, static_cast<int>(player.getId())); // a chill with no art of its own
		return true;
	}
	case SpellID::IceLance: {
		const auto line = MonstersOnLine(here, target, ReachTiles(spell, r));
		const Range d = SkillDamage(spell, r);
		for (Monster *m : line) {
			Strike(player, *m, DamageType::Cold, Rolled(d));
			if ((m->hitPoints >> 6) > 0)
				ChillMonster(*m, SlowTicks(spell, r));
		}
		// RfA-27 batch 54: the lance, through to the farthest it struck, landing with its impact cue (at once without the sheet).
		if (!line.empty())
			Fly(player, MissileGraphicID::IceLance, here, line.back()->position.tile, spell);
		return !line.empty();
	}
	case SpellID::BrittleGround: {
		Field *f = NewField(player, spell, target, EffectTicks(spell, r), r);
		f->tile2 = target + (target == here ? player._pdir : GetDirection(here, target));
		// The frozen floor under both tiles for the field's life (ice_ground.png, 2026-09-26): one of the sheet's two
		// variants each, held rather than animated, on the floor under whoever walks it. Nothing while it is missing.
		int variant = 1;
		for (const Point tile : { f->tile, f->tile2 }) {
			if (!InDungeonBounds(tile) || IsTileSolid(tile))
				continue;
			if (Missile *ice = AddArtEffect(tile, MissileGraphicID::IceGround, static_cast<int>(player.getId()), EffectTicks(spell, r)); ice != nullptr) {
				ice->_miPreFlag = true;
				ice->position.offset = { 0, 32 }; // the patch's centre, y 80 of 128, on the tile's centre: (128 - 80) - 16
				ice->_miAnimFrame = std::min(variant, ice->_miAnimLen);
				ice->_miAnimAdd = 0;
			}
			variant++;
		}
		return true;
	}
	case SpellID::FrozenSentinel:
		NewField(player, spell, target, EffectTicks(spell, r), r);
		return true;
	case SpellID::Whiteout: {
		Field *f = NewField(player, spell, here, WhiteoutTicks, r);
		f->dir = target == here ? player._pdir : GetDirection(here, target);
		// RfA-27 batch 54: the wall of driven snow, rolling at the field's own pace - a tile every WhiteoutStepTicks, which is
		// 32 screen pixels north and south, 64 east and west and about 36 on the diagonals. TickField rings each step without it.
		const int pixelsPerTile = IsAnyOf(f->dir, Direction::East, Direction::West) ? 64 : IsAnyOf(f->dir, Direction::North, Direction::South) ? 32 : 36;
		Fly(player, MissileGraphicID::WhiteoutWall, here, LineEnd(here, here + f->dir, WhiteoutTicks / WhiteoutStepTicks), SpellID::Invalid,
		    std::max(pixelsPerTile / WhiteoutStepTicks, 1));
		return true;
	}
	case SpellID::AbsoluteZero: {
		const Range d = SkillDamage(spell, r);
		for (Monster *m : MonstersWithin(here, ReachTiles(spell, r))) {
			Strike(player, *m, DamageType::Cold, Rolled(d));
			if ((m->hitPoints >> 6) <= 0)
				continue;
			if (ShrugsOff(*m))
				ChillMonster(*m, SlowTicks(spell, r));
			else
				FreezeMonster(*m, SlowTicks(spell, r));
		}
		if (Art(player, MissileGraphicID::AbsoluteZero, here) == nullptr) // RfA-27 batch 55; the ring without it
			Ring(player, here);
		return true;
	}
	// ---------------- Sorceress: lightning ----------------
	case SpellID::Arc: {
		Monster *m = NearestTo(target, 3);
		if (m == nullptr)
			return false;
		const Range d = SkillDamage(spell, r);
		int damage = Rolled(d);
		std::vector<Monster *> struck;
		for (int hop = 0; hop < ArcHops && m != nullptr; hop++) {
			struck.push_back(m);
			const Point at = m->position.tile;
			Art(player, MissileGraphicID::ArcSpark, at); // RfA-27 batch 54: the spark on each one it leaps to
			Strike(player, *m, DamageType::Lightning, damage);
			damage = Percent(damage, ArcFalloffPercent);
			Monster *next = nullptr;
			const NoCastSight hopSees; // a hop needs the struck one's sight, not the hero's (round 35 audit)
			for (Monster *candidate : MonstersWithin(at, ReachTiles(spell, r))) {
				if (std::find(struck.begin(), struck.end(), candidate) == struck.end()) {
					next = candidate;
					break;
				}
			}
			m = next;
		}
		Impact(player, spell); // RfA-27: once for the cast, however many it leapt to
		return true;
	}
	case SpellID::StaticCharge:
		StartBuff(player, Buff::StaticCharge, EffectTicks(spell, r), r);
		return true;
	case SpellID::BallLightning: {
		Field *f = NewField(player, spell, here, BallLightningTicks, r);
		f->tile2 = here;
		f->dir = target == here ? player._pdir : GetDirection(here, target);
		return true;
	}
	case SpellID::Conduit:
		StartBuff(player, Buff::Conduit, EffectTicks(spell, r), r);
		return true;
	case SpellID::LightningRod:
		NewField(player, spell, target, EffectTicks(spell, r), r);
		Art(player, MissileGraphicID::LightningRod, target, EffectTicks(spell, r)); // RfA-27 batch 55: the rod, for its twelve seconds
		return true;
	case SpellID::FaradayRing:
		NewField(player, spell, target, EffectTicks(spell, r), r);
		Art(player, MissileGraphicID::FaradayRing, target, EffectTicks(spell, r)); // RfA-27 batch 55: the cage on the floor
		return true;
	case SpellID::StormCrucible: {
		PlayerState &state = StateOf(player);
		if (state.crucibleTicks <= 0) {
			state.crucibleTicks = EffectTicks(spell, r);
			state.crucibleTile = target;
			// RfA-27 batch 55: the first conductor, waiting for its pair; the cry's ring without it.
			if (Art(player, MissileGraphicID::StormConductor, target, EffectTicks(spell, r)) == nullptr)
				Ring(player, target);
			return true;
		}
		Field *f = NewField(player, spell, state.crucibleTile, CrucibleTicks, r);
		f->tile2 = target;
		f->clock = CruciblePeriod - 1;
		state.crucibleTicks = 0;
		// The pair stands for the storm's three runs: the waiting one gives way to one that lasts as long as the field.
		EndArtEffects(f->tile, MissileGraphicID::StormConductor, static_cast<int>(player.getId()));
		Art(player, MissileGraphicID::StormConductor, f->tile, CrucibleTicks);
		Art(player, MissileGraphicID::StormConductor, f->tile2, CrucibleTicks);
		return true;
	}
	case SpellID::RideTheLightning: {
		const Point dst = Clamped(here, target, ReachTiles(spell, r));
		const auto line = MonstersOnLine(here, dst, here.WalkingDistance(dst));
		if (!TeleportTo(player, dst, spell))
			return false;
		// RfA-27 batch 57: her body become the bolt, flying the way she went.
		Fly(player, MissileGraphicID::RideTheLightning, here, dst);
		const Range d = SkillDamage(spell, r);
		for (Monster *m : line)
			Strike(player, *m, DamageType::Lightning, Rolled(d));
		return true;
	}
	// ---------------- Sorceress: fire ----------------
	case SpellID::EmberMine:
		NewField(player, spell, target, EffectTicks(spell, r), r);
		Art(player, MissileGraphicID::EmberMine, target, EffectTicks(spell, r)); // RfA-27 batch 55: the ember, waiting
		return true;
	case SpellID::FlameRing: {
		const Range d = SkillDamage(spell, r);
		for (Monster *m : MonstersWithin(here, ReachTiles(spell, r)))
			Strike(player, *m, DamageType::Fire, Rolled(d));
		RingOfFireWall(player, here, ReachTiles(spell, r));
		return true;
	}
	case SpellID::AshenBrand: {
		Monster *m = NearestTo(target, 2);
		if (m == nullptr)
			return false;
		MarksOf(*m).ashenTicks = EffectTicks(spell, r);
		MarksOf(*m).ashenRank = r;
		return true;
	}
	case SpellID::FurnaceMouth: {
		Field *f = NewField(player, spell, target, FurnaceMouthTicks, r);
		f->dir = target == here ? player._pdir : GetDirection(here, target);
		f->clock = TicksPerSecond - 1;
		return true;
	}
	case SpellID::Firestorm:
		NewField(player, spell, target, EffectTicks(spell, r), r);
		return true;
	case SpellID::Immolate:
		StartBuff(player, Buff::Immolate, EffectTicks(spell, r), r);
		return true;
	case SpellID::FuneralStar: {
		PlayerState &state = StateOf(player);
		state.funeralTicks = EffectTicks(spell, r);
		state.funeralTile = target;
		state.funeralFrom = here;
		state.funeralRank = r;
		Art(player, MissileGraphicID::FuneralStarCharge, target, EffectTicks(spell, r)); // RfA-27 batch 55: the star gathering
		return true;
	}
	// ---------------- Rogue: bow ----------------
	case SpellID::BarbedShaft: {
		Monster *m = NearestTo(target, 1);
		if (m == nullptr)
			return false;
		const Point at = m->position.tile;
		Strike(player, *m, DamageType::Physical, Percent(WeaponBlow(player), BlowPercent(spell, r)));
		Bleed(*m, EffectTicks(spell, r), PerSecond(spell, r));
		Fly(player, MissileGraphicID::BarbedArrow, here, at, spell); // RfA-27 batch 54: the arrow, landing with its cue
		return true;
	}
	case SpellID::ShockArrow: {
		Monster *m = NearestTo(target, 1);
		if (m == nullptr)
			return false;
		const Point at = m->position.tile;
		Strike(player, *m, DamageType::Physical, Percent(WeaponBlow(player), BlowPercent(spell, r)));
		if (Monster *other = NearestTo(at, ReachTiles(spell, r), m); other != nullptr)
			Strike(player, *other, DamageType::Lightning, Rolled(SkillDamage(spell, r)));
		Fly(player, MissileGraphicID::ShockArrow, here, at, spell); // RfA-27 batch 54: the arrow, landing with its cue
		return true;
	}
	case SpellID::PiercingShot: {
		const auto line = MonstersOnLine(here, target, ReachTiles(spell, r));
		for (Monster *m : line)
			Strike(player, *m, DamageType::Physical, Percent(WeaponBlow(player), BlowPercent(spell, r)));
		// RfA-27 batch 54: the arrow, through to the farthest it struck, landing with its cue.
		if (!line.empty())
			Fly(player, MissileGraphicID::PiercingArrow, here, line.back()->position.tile, spell);
		return !line.empty();
	}
	case SpellID::RainOfArrows: {
		for (Monster *m : MonstersWithin(target, ReachTiles(spell, r)))
			Strike(player, *m, DamageType::Physical, Percent(WeaponBlow(player), BlowPercent(spell, r)));
		if (Art(player, MissileGraphicID::RainOfArrows, target) == nullptr) // RfA-27 batch 55; the ring without it
			Ring(player, target);
		Impact(player, spell);
		return true;
	}
	case SpellID::CripplingShot: {
		Monster *m = NearestTo(target, 1);
		if (m == nullptr)
			return false;
		const Point at = m->position.tile;
		Strike(player, *m, DamageType::Physical, Percent(WeaponBlow(player), BlowPercent(spell, r)));
		if ((m->hitPoints >> 6) > 0) {
			ChillMonster(*m, SlowTicks(spell, r));
			MarksOf(*m).slowMarkTicks = SlowTicks(spell, r); // RfA-27 batch 58: the Slowed sigil
		}
		Fly(player, MissileGraphicID::CripplingArrow, here, at, spell); // RfA-27 batch 54: the arrow, landing with its cue
		return true;
	}
	case SpellID::HuntersMark: {
		Monster *m = NearestTo(target, 2);
		if (m == nullptr)
			return false;
		MarksOf(*m).huntTicks = EffectTicks(spell, r);
		MarksOf(*m).huntPercent = EffectPercent(spell, r);
		return true;
	}
	case SpellID::Barrage: {
		Monster *m = NearestTo(target, 1);
		if (m == nullptr)
			return false;
		const Point at = m->position.tile;
		for (int i = 0; i < BarrageArrows && (m->hitPoints >> 6) > 0; i++)
			Strike(player, *m, DamageType::Physical, Percent(WeaponBlow(player), BlowPercent(spell, r)));
		// RfA-27 batch 54: five arrows, a little slower each, so they arrive as a string rather than as one.
		for (int i = 0; i < BarrageArrows; i++)
			Fly(player, MissileGraphicID::BarrageArrow, here, at, SpellID::Invalid, 32 - 3 * i);
		return true;
	}
	case SpellID::PhantomVolley: {
		const auto all = MonstersWithin(here, ReachTiles(spell, r));
		for (Monster *m : all)
			Strike(player, *m, DamageType::Magic, Percent(WeaponBlow(player), BlowPercent(spell, r)));
		// RfA-27 batch 54: a spectral arrow to each; the first to land carries the volley's one impact cue.
		bool first = true;
		for (Monster *m : all) {
			Fly(player, MissileGraphicID::PhantomArrow, here, m->position.tile, first ? spell : SpellID::Invalid);
			first = false;
		}
		return !all.empty();
	}
	// ---------------- Rogue: magic and spear ----------------
	case SpellID::ShadowStep: {
		Monster *m = NearestTo(target, ReachTiles(spell, r));
		if (m == nullptr)
			return false;
		const Point behind = m->position.tile + GetDirection(here, m->position.tile);
		if (!TeleportTo(player, behind, spell))
			return false;
		// RfA-27 batch 57: the smoke where she left and where she stands.
		Art(player, MissileGraphicID::ShadowStep, here);
		Art(player, MissileGraphicID::ShadowStep, behind);
		return true;
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
		StateOf(player).claimTicks = EffectTicks(spell, r);
		return true;
	}
	case SpellID::Harpoon: {
		const auto line = MonstersOnLine(here, target, ReachTiles(spell, r));
		if (line.empty())
			return false;
		Monster &m = *line.front();
		const Point at = m.position.tile;
		Strike(player, m, DamageType::Physical, Percent(WeaponBlow(player), BlowPercent(spell, r)));
		if ((m.hitPoints >> 6) > 0) {
			Shove(m, GetDirection(m.position.tile, here));
			Stagger(m, StunTicks(spell, r));
		}
		Fly(player, MissileGraphicID::Harpoon, here, at, spell); // RfA-27 batch 54: the harpoon, landing with its cue
		return true;
	}
	case SpellID::Vault: {
		const Point dst = Clamped(here, target, ReachTiles(spell, r));
		if (!TeleportTo(player, dst, spell))
			return false;
		// RfA-27 batch 57: the dust at take-off and landing, and the landing's cue.
		Art(player, MissileGraphicID::VaultDust, here);
		Art(player, MissileGraphicID::VaultDust, dst);
		Impact(player, spell);
		return true;
	}
	case SpellID::AnchorJavelin: {
		const auto line = MonstersOnLine(here, target, ReachTiles(spell, r));
		if (line.empty())
			return false;
		Monster &pinned = *line.front();
		const Point at = pinned.position.tile;
		Strike(player, pinned, DamageType::Physical, Percent(WeaponBlow(player), BlowPercent(spell, r)));
		// RfA-27 batch 58: the Pinned sigil, for as long as the pin holds - the ones Stagger leaves alone are not pinned.
		if (!ShrugsOff(pinned) && pinned.mode != MonsterMode::Petrified && (pinned.hitPoints >> 6) > 0)
			MarksOf(pinned).pinTicks = StunTicks(spell, r);
		Stagger(pinned, StunTicks(spell, r));
		Fly(player, MissileGraphicID::AnchorJavelin, here, at, spell); // RfA-27 batch 54: the javelin, landing with its cue
		return true;
	}
	// ---- the census notes (2026-09-14) ----
	case SpellID::Meteor:
		// A second to fall, three to burn: TickField does both. The rock's ten frames take that second.
		NewField(player, spell, target, EffectTicks(spell, r), r);
		Show(player, MissileID::MeteorFall, MissileGraphicID::Meteor, target, target);
		return true;
	case SpellID::Valkyrie:
	case SpellID::Decoy:
		// Companions (user, 2026-09-14): the Valkyrie archer, and the Decoy that draws every blow. oracool/companion.h.
		return SummonCompanions(player, spell, target, r);
	case SpellID::PoisonJavelin: {
		const auto line = MonstersOnLine(here, target, ReachTiles(spell, r));
		if (line.empty())
			return false;
		Monster &m = *line.front();
		const Point pool = m.position.tile;
		Strike(player, m, DamageType::Acid, Percent(WeaponBlow(player), BlowPercent(spell, r)));
		NewField(player, spell, pool, EffectTicks(spell, r), r);
		Show(player, MissileID::AcidJavelin, MissileGraphicID::AcidJavelin, here, pool);
		Show(player, MissileID::AcidCloud, MissileGraphicID::AcidCloud, pool, pool, EffectTicks(spell, r));
		return true;
	}
	case SpellID::PlagueJavelin: {
		const auto line = MonstersOnLine(here, target, 8);
		const Point burst = line.empty() ? Clamped(here, target, 8) : Point(line.front()->position.tile);
		NewField(player, spell, burst, EffectTicks(spell, r), r);
		Show(player, MissileID::AcidJavelin, MissileGraphicID::AcidJavelin, here, burst);
		if (!Show(player, MissileID::AcidCloud, MissileGraphicID::AcidCloud, burst, burst, EffectTicks(spell, r)))
			Ring(player, burst);
		return true;
	}
	case SpellID::ValkyriesSpear: {
		for (Monster *m : MonstersWithin(target, ReachTiles(spell, r)))
			Strike(player, *m, DamageType::Physical, Percent(WeaponBlow(player), BlowPercent(spell, r)));
		// RfA-27 batches 54-55: the spear flies and bursts where it lands, with its cue; without the spear the burst and the
		// cue come at once, and with neither sheet the cry's ring stands in, as before.
		if (!MissileArtLoaded(MissileGraphicID::ValkyrieSpear) && !MissileArtLoaded(MissileGraphicID::ValkyrieBurst))
			Ring(player, target);
		Fly(player, MissileGraphicID::ValkyrieSpear, here, target, spell, 32, MissileGraphicID::ValkyrieBurst);
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
		const auto line = MonstersOnLine(here, target, ReachTiles(spell, r));
		if (line.empty())
			return false;
		Strike(player, *line.front(), DamageType::Physical, Percent(WeaponBlow(player), BlowPercent(spell, r)));
		// RfA-27 batch 53: the staff's streak along his aim, and the jab's cue.
		ArtFacing(player, MissileGraphicID::LongThrust, here, target == here ? player._pdir : GetDirection(here, target));
		Impact(player, spell);
		return true;
	}
	case SpellID::MountainPole: {
		const auto around = MonstersWithin(here, 1);
		for (Monster *m : around) {
			Strike(player, *m, DamageType::Physical, Percent(WeaponBlow(player), BlowPercent(spell, r)));
			Stagger(*m, StunTicks(spell, r));
		}
		if (around.empty())
			return false;
		// RfA-27 batch 55: the crater and its dust ring, in place of the cry's shockwave (Rfa12CastLeavesRing).
		Art(player, MissileGraphicID::MountainPole, here);
		return true;
	}
	case SpellID::BambooRain: {
		auto nearby = MonstersWithin(here, ReachTiles(spell, r));
		if (nearby.size() > static_cast<size_t>(BambooRainTargets))
			nearby.resize(static_cast<size_t>(BambooRainTargets));
		for (Monster *m : nearby) {
			Art(player, MissileGraphicID::StaffFlurry, m->position.tile); // RfA-27 batch 52: the flurry on each it strikes
			Strike(player, *m, DamageType::Physical, Percent(WeaponBlow(player), BlowPercent(spell, r)));
		}
		if (!nearby.empty())
			Impact(player, spell);
		return !nearby.empty();
	}
	case SpellID::DragonTailSweep:
	case SpellID::WhirlingKick: {
		const auto around = MonstersWithin(here, 1);
		for (Monster *m : around) {
			Strike(player, *m, DamageType::Physical, Percent(WeaponBlow(player), BlowPercent(spell, r)));
			Shove(*m, GetDirection(here, m->position.tile));
		}
		if (around.empty())
			return false;
		// RfA-27 batch 53: the circular sweep around him.
		Art(player, spell == SpellID::DragonTailSweep ? MissileGraphicID::DragonTailSweep : MissileGraphicID::WhirlingKick, here);
		return true;
	}
	case SpellID::HeavenSplitter: {
		const auto line = MonstersOnLine(here, target, ReachTiles(spell, r));
		for (Monster *m : line)
			Strike(player, *m, DamageType::Physical, Percent(WeaponBlow(player), BlowPercent(spell, r)));
		// RfA-27 batch 54: the blade of force skimming the floor down the line; the cry's ring while it is missing.
		if (MissileArtLoaded(MissileGraphicID::HeavenSplitterWave))
			Fly(player, MissileGraphicID::HeavenSplitterWave, here, LineEnd(here, target, ReachTiles(spell, r)), SpellID::Invalid, 16);
		else
			Ring(player, here);
		return true;
	}
	case SpellID::ThousandReeds: {
		const auto all = MonstersWithin(here, ReachTiles(spell, r));
		for (Monster *m : all) {
			Art(player, MissileGraphicID::StaffFlurry, m->position.tile); // RfA-27 batch 52: the flurry on each it strikes
			Strike(player, *m, DamageType::Physical, Percent(WeaponBlow(player), BlowPercent(spell, r)));
		}
		return !all.empty();
	}
	// ---------------- Monk: body ----------------
	case SpellID::LeapingCrane: {
		const Point dst = Clamped(here, target, ReachTiles(spell, r));
		if (!TeleportTo(player, dst, spell))
			return false;
		Art(player, MissileGraphicID::LeapingCrane, here); // RfA-27 batch 57: the take-off; the landing's is TickLanding's
		PlayerState &state = StateOf(player);
		state.landingTicks = 2;
		state.landingTile = target;
		state.landingSpell = spell;
		state.landingRank = r;
		return true;
	}
	case SpellID::ShoulderGate: {
		const Point dst = Clamped(here, target, ReachTiles(spell, r));
		if (!TeleportTo(player, dst, spell))
			return false;
		Art(player, MissileGraphicID::ShoulderGate, here); // RfA-27 batch 57: the rush's start; the impact ring is TickLanding's
		PlayerState &state = StateOf(player);
		state.landingTicks = 2;
		state.landingTile = target;
		state.landingSpell = spell;
		state.landingRank = r;
		return true;
	}
	case SpellID::SevenSidedStrike: {
		auto nearby = MonstersWithin(target, ReachTiles(spell, r));
		const size_t cap = static_cast<size_t>(SevenSidedTargets(r));
		if (nearby.size() > cap)
			nearby.resize(cap);
		for (Monster *m : nearby) {
			Art(player, MissileGraphicID::SevenSidedStrike, m->position.tile); // RfA-27 batch 52: on each one struck
			Strike(player, *m, DamageType::Physical, Percent(WeaponBlow(player), BlowPercent(spell, r)));
		}
		return !nearby.empty();
	}
	case SpellID::DragonsWrath: {
		const auto line = MonstersOnLine(here, target, ReachTiles(spell, r));
		for (Monster *m : line)
			Strike(player, *m, DamageType::Physical, Percent(WeaponBlow(player), BlowPercent(spell, r)));
		// RfA-27 batch 54: the wave of wind and force down the line; the cry's ring while it is missing.
		if (MissileArtLoaded(MissileGraphicID::DragonsWrathWave))
			Fly(player, MissileGraphicID::DragonsWrathWave, here, LineEnd(here, target, ReachTiles(spell, r)), SpellID::Invalid, 16);
		else
			Ring(player, here);
		return true;
	}
	// ---------------- Monk: spirit ----------------
	case SpellID::MantraOfClarity:
		StartBuff(player, Buff::Clarity, EffectTicks(spell, r), r);
		return true;
	case SpellID::MantraOfEvasion:
		StartBuff(player, Buff::Evasion, EffectTicks(spell, r), r);
		return true;
	case SpellID::ChiWave: {
		Monster *m = NearestTo(target, 3);
		if (m == nullptr)
			return false;
		const Range d = SkillDamage(spell, r);
		std::vector<Monster *> struck;
		Point from = here; // RfA-27 batch 54: the orb, from the hero to the first and on from each to the next
		for (int hop = 0; hop < ChiWaveHops && m != nullptr; hop++) {
			struck.push_back(m);
			const Point at = m->position.tile;
			Fly(player, MissileGraphicID::ChiWave, from, at, hop == 0 ? spell : SpellID::Invalid, 24);
			from = at;
			Strike(player, *m, DamageType::Magic, Rolled(d));
			Monster *next = nullptr;
			const NoCastSight hopSees; // as Arc's (round 35 audit)
			for (Monster *candidate : MonstersWithin(at, ReachTiles(spell, r))) {
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
		const auto nearby = MonstersWithin(here, ReachTiles(spell, r));
		for (Monster *m : nearby) {
			if (ShrugsOff(*m) || m->mode == MonsterMode::Petrified)
				continue;
			m->goal = MonsterGoal::Retreat;
			m->goalVar1 = 3;
			m->goalVar2 = static_cast<int8_t>(GenerateRnd(8));
		}
		if (nearby.empty())
			return false;
		// RfA-27 batch 55: the flash, in place of the cry's shockwave (Rfa12CastLeavesRing).
		Art(player, MissileGraphicID::BlindingFlash, here);
		return true;
	}
	case SpellID::MantraOfRetribution:
		StartBuff(player, Buff::Retribution, EffectTicks(spell, r), r);
		return true;
	case SpellID::Serenity:
		ClearPlayerSlow(player);
		// An aura ring rising round her and sinking back, colour-cycled, since v1.12.211 (user, 2026-09-27: "use a still
		// image, like an aura ring, animate it up and down and use color cycling on it"). Drawn by scrollrt's DrawPlayer.
		StateOf(player).serenityTicks = SerenityTicks;
		return true;
	case SpellID::WaveOfLight: {
		const Range d = SkillDamage(spell, r);
		for (Monster *m : MonstersWithin(target, ReachTiles(spell, r)))
			Strike(player, *m, DamageType::Magic, Rolled(d));
		if (Art(player, MissileGraphicID::WaveOfLight, target) == nullptr) // RfA-27 batch 55: the bell; the ring without it
			Ring(player, target);
		Impact(player, spell);
		return true;
	}
	case SpellID::AstralProjection:
		StartBuff(player, Buff::Astral, EffectTicks(spell, r), r);
		return true;
	case SpellID::AncestralCourt: {
		Field *f = NewField(player, spell, target, AncestralCourtTicks, r);
		f->clock = 0;
		// RfA-27 batch 55: the three shades gathering (two ticks a frame, so they strike about when the first blow lands).
		Art(player, MissileGraphicID::AncestralCourt, target);
		return true;
	}
	// ---------------- Necromancer: Poison & Bone ----------------
	case SpellID::Teeth: {
		// A fan: the front arc's three tiles, and one more tooth a rank flies on to the next monster in the line.
		bool any = false;
		const Range d = SkillDamage(spell, r);
		std::vector<const Monster *> struck;
		for (const Point tile : FrontArc(player, target)) {
			Monster *m = FindMonsterAtPosition(tile);
			if (m == nullptr || !Hittable(*m) || std::find(struck.begin(), struck.end(), m) != struck.end())
				continue; // a walking monster holds two fan tiles: one tooth (round 5 audit)
			BoneStrike(player, *m, Rolled(d));
			struck.push_back(m);
			any = true;
		}
		// Down the line, past the fan: the line starts on the fan's middle tile, whose monster already took a tooth (audit,
		// 2026-09-29 - it took two).
		auto line = MonstersOnLine(here, target, TeethLineTiles);
		line.erase(std::remove_if(line.begin(), line.end(), [&](const Monster *m) { return std::find(struck.begin(), struck.end(), m) != struck.end(); }), line.end());
		if (line.size() > static_cast<size_t>(TeethDownTheLine(r)))
			line.resize(static_cast<size_t>(TeethDownTheLine(r)));
		for (Monster *m : line) {
			BoneStrike(player, *m, Rolled(d));
			any = true;
		}
		// The fan itself (batch 38): a tooth to each arc tile and one down the line.
		for (const Point tile : FrontArc(player, target))
			Bolt(player, MissileID::BoneToothBolt, tile);
		Bolt(player, MissileID::BoneToothBolt, LineEnd(here, target, TeethLineTiles)); // the strike's line (round 37)
		if (!MissileArtLoaded(MissileGraphicID::BoneTooth))
			Ring(player, here);
		return any;
	}
	case SpellID::BoneArmor: {
		PlayerState &state = StateOf(player);
		state.bonePool = BoneArmorPool(r) << 6;
		StartBuff(player, Buff::BoneShell, EffectTicks(spell, r), r);
		// The shell is a player-icon overlay in scrollrt.cpp DrawPlayerIcons, keyed on Rfa12BoneShellFrame (batch 38).
		return true;
	}
	case SpellID::PoisonDagger:
		StartBuff(player, Buff::Venom, EffectTicks(spell, r), r);
		return true;
	case SpellID::CorpseExplosion: {
		const std::optional<Corpse> corpse = BurstCorpse(player, target);
		if (!corpse)
			return false;
		// A share of the dead one's life, physical, to everything within two tiles - Diablo II's own rule.
		const int share = std::clamp(corpse->maxLife * EffectPercent(spell, r) / 100, CorpseBurstMin, CorpseBurstMax) << 6;
		for (Monster *m : MonstersWithin(corpse->position, ReachTiles(spell, r)))
			Strike(player, *m, DamageType::Physical, share);
		if (!Show(player, MissileID::CorpseBurst, MissileGraphicID::CorpseExplosion, corpse->position, corpse->position))
			Ring(player, corpse->position);
		Impact(player, spell); // RfA-27 batch 51
		return true;
	}
	case SpellID::BoneSplinters: {
		auto line = MonstersOnLine(here, target, 4);
		if (line.size() > static_cast<size_t>(BoneSplinterTargets))
			line.resize(static_cast<size_t>(BoneSplinterTargets));
		const Range d = SkillDamage(spell, r);
		for (Monster *m : line)
			BoneStrike(player, *m, Rolled(d));
		for (int k = 0; k < 3; k++)
			Bolt(player, MissileID::BoneToothBolt, Clamped(here, target, 2 + k));
		return !line.empty();
	}
	case SpellID::Blight: {
		const Point pool = Clamped(here, target, 8);
		Field *f = NewField(player, spell, pool, EffectTicks(spell, r), r);
		f->clock = TicksPerSecond - 1;
		ImpactOnLanding(player, Bolt(player, MissileID::PoisonBoltFlight, pool), spell); // RfA-27: the splash as the bolt lands
		Show(player, MissileID::AcidCloud, MissileGraphicID::AcidCloud, pool, pool, EffectTicks(spell, r));
		return true;
	}
	case SpellID::BoneWall: {
		// A line of five across the cursor, at right angles to the cast.
		const Point centre = Clamped(here, target, 8);
		if (!LineClearMissile(here, centre)) { // not raised in the next room (round 37 audit)
			player.Say(HeroSpeech::ICantDoThat);
			return false;
		}
		Field *f = NewField(player, spell, centre, EffectTicks(spell, r), r);
		f->dir = Right(Right(target == here ? player._pdir : GetDirection(here, target)));
		// The five segments (batch 38), each rising once and standing for the wall's life.
		bool drawn = false;
		for (int k = -2; k <= 2; k++) {
			Point tile = centre;
			for (int step = 0; step < std::abs(k); step++)
				tile = tile + (k < 0 ? Opposite(f->dir) : f->dir);
			if (InDungeonBounds(tile))
				drawn = Show(player, MissileID::BoneWallEffect, MissileGraphicID::BoneWall, tile, tile, EffectTicks(spell, r)) || drawn;
		}
		if (!drawn)
			Ring(player, centre);
		return true;
	}
	case SpellID::BoneSpikes: {
		const auto struck = MonstersWithin(target, ReachTiles(spell, r));
		const Range d = SkillDamage(spell, r);
		for (Monster *m : struck) {
			BoneStrike(player, *m, Rolled(d));
			Stagger(*m, StunTicks(spell, r));
		}
		if (!Show(player, MissileID::BoneSpikesEffect, MissileGraphicID::BoneSpikes, target, target))
			Ring(player, target);
		return true;
	}
	case SpellID::PoisonExplosion: {
		const std::optional<Corpse> corpse = BurstCorpse(player, target);
		if (!corpse)
			return false;
		for (Monster *m : MonstersWithin(corpse->position, ReachTiles(spell, r)))
			Poison(player, *m, PoisonTicks(spell), PerSecond(spell, r));
		Show(player, MissileID::CorpseBurst, MissileGraphicID::CorpseExplosion, corpse->position, corpse->position);
		Show(player, MissileID::AcidCloud, MissileGraphicID::AcidCloud, corpse->position, corpse->position, 3 * TicksPerSecond);
		Impact(player, spell); // RfA-27 batch 51
		return true;
	}
	case SpellID::BoneSpear: {
		const auto line = MonstersOnLine(here, target, ReachTiles(spell, r));
		const Range d = SkillDamage(spell, r);
		for (Monster *m : line)
			BoneStrike(player, *m, Rolled(d));
		// Drawn down the line the strike runs, to its wall, not at the exact cursor (round 37 audit).
		ImpactOnLanding(player, Bolt(player, MissileID::BoneSpearBolt, LineEnd(here, target, ReachTiles(spell, r))), spell); // RfA-27
		if (!MissileArtLoaded(MissileGraphicID::BoneSpear))
			Ring(player, here);
		return true;
	}
	case SpellID::Decompose: {
		Monster *m = FindMonsterAtPosition(target);
		// Not on a poison-immune one: the poison slid off and the cast was paid for nothing (round 33 audit).
		// And in the hero's sight, as NearestTo's picks since round 15 (round 37 audit: through a wall).
		if (m == nullptr || !Hittable(*m) || m->isImmune(MissileID::Null, DamageType::Acid)
		    || (CastSightFrom && !LineClearMissile(*CastSightFrom, m->position.tile))) {
			player.Say(HeroSpeech::ICantDoThat);
			return false;
		}
		Poison(player, *m, PoisonTicks(spell), PerSecond(spell, r));
		MarksOf(*m).rotTicks = VirulentDose(player, PoisonTicks(spell), PerSecond(spell, r)).ticks; // RfA-27 batch 58: Rotting
		return true;
	}
	case SpellID::BonePrison: {
		Monster *m = FindMonsterAtPosition(target);
		if (m == nullptr || !Hittable(*m) || (CastSightFrom && !LineClearMissile(*CastSightFrom, m->position.tile))) { // round 37
			player.Say(HeroSpeech::ICantDoThat);
			return false;
		}
		Field *f = NewField(player, spell, m->position.tile, EffectTicks(spell, r), r);
		f->step = static_cast<int>(m->getId());
		Stagger(*m, StunTicks(spell, r));
		if (Art(player, MissileGraphicID::BonePrison, m->position.tile) == nullptr) // RfA-27 batch 55; the ring without it
			Ring(player, m->position.tile);
		return true;
	}
	case SpellID::BoneStorm: {
		Field *f = NewField(player, spell, here, EffectTicks(spell, r), r);
		f->clock = 0;
		Show(player, MissileID::BoneStormEffect, MissileGraphicID::BoneStorm, here, here, EffectTicks(spell, r)); // it follows (ProcessCensusEffect)
		return true;
	}
	case SpellID::NecroBoneSpirit: {
		// The book spell's own missile and brain: it hunts what it was aimed at and finds another if that falls.
		// Dressed in the delivered skull (batch 38, sixteen facings) when that sheet is in the archive -
		// ProcessBoneSpirit knows the sheet, turns it by sixteenths and ends it in the bone-hit burst.
		const Direction dir = target == here ? player._pdir : GetDirection(here, target);
		Missile *spirit = AddMissile(here, target, dir, MissileID::BoneSpirit, TARGET_MONSTERS, static_cast<int>(player.getId()), 0, r);
		if (spirit != nullptr && MissileArtLoaded(MissileGraphicID::BoneSpiritNecro)) {
			spirit->_miAnimType = MissileGraphicID::BoneSpiritNecro;
			SetMissDir(*spirit, GetDirection16(here, target == here ? here + dir : target));
		}
		return true;
	}
	case SpellID::PoisonNova: {
		const auto struck = MonstersWithin(here, ReachTiles(spell, r));
		for (Monster *m : struck)
			Poison(player, *m, PoisonTicks(spell), PerSecond(spell, r));
		// Sixteen bolts outward (batch 38), one a facing.
		static const Displacement Ring16[16] = { { 0, 5 }, { -2, 5 }, { -4, 4 }, { -5, 2 }, { -5, 0 }, { -5, -2 }, { -4, -4 }, { -2, -5 }, { 0, -5 }, { 2, -5 }, { 4, -4 }, { 5, -2 }, { 5, 0 }, { 5, 2 }, { 4, 4 }, { 2, 5 } };
		for (const Displacement &d16 : Ring16)
			Bolt(player, MissileID::PoisonBoltFlight, here + d16);
		if (!MissileArtLoaded(MissileGraphicID::PoisonBolt))
			Ring(player, here);
		return true;
	}
	case SpellID::DeathNova: {
		const auto struck = MonstersWithin(here, ReachTiles(spell, r));
		const Range d = SkillDamage(spell, r);
		for (Monster *m : struck) {
			BoneStrike(player, *m, Rolled(d));
			if ((m->hitPoints >> 6) > 0)
				Poison(player, *m, PoisonTicks(spell), PerSecond(spell, r));
		}
		// Vanilla's Flash since v1.12.211 (user, 2026-09-27: "use appropriately recoloured flash spell"), in the blight's
		// yellow-green: its top half behind the Necromancer, its bottom half in front, as Flash draws them.
		Missile *top = Art(player, MissileGraphicID::FlashTop, here);
		Missile *bottom = Art(player, MissileGraphicID::FlashBottom, here);
		for (Missile *half : { top, bottom }) {
			if (half == nullptr)
				continue;
			half->oracoolTint = Tint::Hue;
			half->oracoolTintRgb = Rgb(176, 212, 88);
		}
		if (top != nullptr)
			top->_miPreFlag = true;
		if (top == nullptr && bottom == nullptr)
			Ring(player, here);
		return true;
	}
	default:
		// The Necromancer's other pages live in their own modules and come through this door.
		if (IsNecromancerCurse(spell))
			return CastNecromancerCurse(player, spell, target, r);
		return CastNecromancerSummoning(player, spell, target, r);
	}
}

// =================================================================================================
// The ticks
// =================================================================================================

void TickField(Player &player, Field &field)
{
	const int r = field.rank;
	const BowStrikeScope bow { IsBowSkill(field.spell) }; // Rain of Arrows' later volleys are arrows too
	field.clock++;
	switch (field.spell) {
	// ---------------- Necromancer: Poison & Bone ----------------
	case SpellID::Blight:
		if (field.clock % TicksPerSecond == 0) {
			for (Monster *m : MonstersWithin(field.tile, 1))
				Poison(player, *m, PoisonTicks(field.spell), PerSecond(field.spell, r));
		}
		break;
	case SpellID::BoneWall:
		// Five tiles across the cast: whatever stands in one is cut and thrown back the way it came, twice a second.
		if (field.clock % (field.spell == SpellID::BoneWall ? BoneWallPeriod : BoneStormPeriod) == 0) {
			std::vector<const Monster *> cut; // once a pulse: a walker holds two of the wall's tiles (round 5 audit)
			for (int k = -2; k <= 2; k++) {
				Point tile = field.tile;
				for (int step = 0; step < std::abs(k); step++)
					tile = tile + (k < 0 ? Opposite(field.dir) : field.dir);
				Monster *m = InDungeonBounds(tile) ? FindMonsterAtPosition(tile) : nullptr;
				if (m == nullptr || !Hittable(*m) || std::find(cut.begin(), cut.end(), m) != cut.end())
					continue;
				cut.push_back(m);
				BoneStrike(player, *m, Rolled(SkillDamage(field.spell, r)));
				if ((m->hitPoints >> 6) > 0)
					Shove(*m, GetDirection(Players[field.owner].position.tile, m->position.tile));
			}
		}
		break;
	case SpellID::BonePrison:
		if (field.clock % TicksPerSecond == 0 && field.step >= 0 && static_cast<size_t>(field.step) < MaxMonsters) {
			Monster &held = Monsters[field.step];
			// The prisoner is the monster staggered ON the prison's tile; a slot refilled after its death
			// (a fresh spawn, a re-forming minion) is somewhere else and is not cut (audit, 2026-09-19).
			if (Hittable(held) && held.position.tile == field.tile) {
				BoneStrike(player, held, Rolled(SkillDamage(field.spell, r)));
				Stagger(held, TicksPerSecond + 5);
			}
		}
		break;
	case SpellID::BoneStorm:
		field.tile = player.position.tile; // it follows
		if (field.clock % (field.spell == SpellID::BoneWall ? BoneWallPeriod : BoneStormPeriod) == 0) {
			for (Monster *m : MonstersWithin(field.tile, ReachTiles(field.spell, r)))
				BoneStrike(player, *m, Rolled(SkillDamage(field.spell, r)));
		}
		break;
	case SpellID::Earthquake:
		if (field.clock % TicksPerSecond == 0) {
			const auto shaken = MonstersWithin(field.tile, ReachTiles(field.spell, r));
			for (Monster *m : shaken)
				Strike(player, *m, DamageType::Physical, Percent(WeaponBlow(player), BlowPercent(field.spell, r)));
			EarthenMightRage(player, shaken.size());
			Impact(player, field.spell); // one tremor pulse a second
		}
		// A ring every fifth of a second, in the quake's molten brown to dark orange (v1.12.211's cycle) - its look.
		if (field.clock % EarthquakeRingTicks == 0) {
			if (Missile *ring = ReachRing(player, field.tile, ReachTiles(field.spell, r)); ring != nullptr)
				ring->oracoolTint = Tint::Earthquake;
		}
		break;
	case SpellID::BrittleGround:
		{
			bool cracked = false;
			for (const Point tile : { field.tile, field.tile2 }) {
				Monster *m = InDungeonBounds(tile) ? FindMonsterAtPosition(tile) : nullptr;
				if (m == nullptr || !Hittable(*m) || !m->isWalking())
					continue;
				Marks &marks = MarksOf(*m);
				if (marks.brittleCooldown > 0)
					continue;
				marks.brittleCooldown = TicksPerSecond;
				Strike(player, *m, DamageType::Cold, Rolled(SkillDamage(field.spell, r)));
				if ((m->hitPoints >> 6) > 0)
					ChillMonster(*m, SlowTicks(field.spell, r));
				cracked = true;
			}
			if (cracked)
				Impact(player, field.spell); // RfA-27: the ice crunching underfoot, once a tick however many stepped on it
		}
		break;
	case SpellID::FrozenSentinel:
		if (field.clock % FrozenSentinelPeriod == 0) {
			if (Monster *m = NearestTo(field.tile, ReachTiles(field.spell, r)); m != nullptr) {
				AddMissile(field.tile, m->position.tile, GetDirection(field.tile, m->position.tile), MissileID::IceBolt,
				    TARGET_MONSTERS, static_cast<int>(player.getId()), 0, r);
			}
		}
		break;
	case SpellID::Whiteout:
		if (field.clock % WhiteoutStepTicks == 0) {
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
				Strike(player, *m, DamageType::Cold, Rolled(SkillDamage(field.spell, r)));
				if ((m->hitPoints >> 6) > 0)
					ChillMonster(*m, SlowTicks(field.spell, r));
			}
			if (!MissileArtLoaded(MissileGraphicID::WhiteoutWall)) // RfA-27: the wall rolls on its own (CastOnce)
				Ring(player, field.tile);
		}
		break;
	case SpellID::BallLightning:
		if (field.clock % BallLightningStepTicks == 0) {
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
		const int reach = field.spell == SpellID::FaradayRing ? FaradayRingReach : 1;
		bool fizzled = false;
		for (Missile &missile : Missiles) {
			if (missile._miDelFlag || missile.sourceType() != MissileSource::Monster)
				continue;
			if (field.tile.WalkingDistance(missile.position.tile) > reach)
				continue;
			if (field.spell == SpellID::LightningRod && GetMissileData(missile._mitype).damageType() != DamageType::Lightning)
				continue;
			// Not a charging monster's carrier: deleting it left the beast in Charge mode with no missile, frozen and
			// unhittable until the level was left (round 7 audit, v1.12.232).
			if (missile._mitype == MissileID::Rhino)
				continue;
			// Its light goes with it: the next ProcessMissiles deletes a flagged missile before its own process frees the
			// light, so every caught fireball left a glow and a spent slot in the light pool (round 7 audit).
			if (missile._mlid != NO_LIGHT) {
				AddUnLight(missile._mlid);
				missile._mlid = NO_LIGHT;
			}
			missile._miDelFlag = true;
			if (field.spell == SpellID::LightningRod) {
				for (Monster *m : MonstersWithin(field.tile, LightningRodBurstRadius))
					Strike(player, *m, DamageType::Lightning, Rolled(SkillDamage(field.spell, r)));
				// RfA-27 batch 55: the rod bursts - its standing sheet goes, its burst plays; the ring without the burst.
				EndArtEffects(field.tile, MissileGraphicID::LightningRod, static_cast<int>(player.getId()));
				if (Art(player, MissileGraphicID::LightningRodBurst, field.tile) == nullptr)
					Ring(player, field.tile);
				Impact(player, field.spell);
				field.ticksLeft = 0;
				break;
			}
			fizzled = true;
		}
		if (fizzled)
			Impact(player, field.spell); // RfA-27: Faraday Ring's fizzle, once a tick however many it caught
		break;
	}
	case SpellID::StormCrucible:
		if (field.clock % CruciblePeriod == 0) {
			const Range d = SkillDamage(field.spell, r);
			for (Monster *m : MonstersOnLine(field.tile, field.tile2, field.tile.WalkingDistance(field.tile2)))
				Strike(player, *m, DamageType::Lightning, Rolled(d));
			// RfA-27 batch 55: a segment of lightning on every tile between the pair; the two rings without the sheet.
			if (MissileArtLoaded(MissileGraphicID::StormArc)) {
				for (const Point tile : LineOfTiles(field.tile, field.tile2, field.tile.WalkingDistance(field.tile2)))
					Art(player, MissileGraphicID::StormArc, tile);
			} else {
				Ring(player, field.tile);
				Ring(player, field.tile2);
			}
			Impact(player, field.spell);
		}
		break;
	case SpellID::EmberMine: {
		Monster *m = FindMonsterAtPosition(field.tile);
		if (m != nullptr && Hittable(*m)) {
			const Range d = SkillDamage(field.spell, r);
			for (Monster *nearby : MonstersWithin(field.tile, 1))
				Strike(player, *nearby, DamageType::Fire, Rolled(d));
			// RfA-27 batch 55: the ember goes off - its waiting sheet goes, the burst plays; the ring without the burst.
			EndArtEffects(field.tile, MissileGraphicID::EmberMine, static_cast<int>(player.getId()));
			// Apocalypse's explosion since v1.12.211 (user, 2026-09-27: "use appocalypse asset").
			if (Art(player, MissileGraphicID::ApocalypseBoom, field.tile) == nullptr)
				Ring(player, field.tile);
			Impact(player, field.spell);
			field.ticksLeft = 0;
		}
		break;
	}
	// ---- the census notes (2026-09-14) ----
	case SpellID::Meteor:
		if (field.step == 0 && field.clock >= TicksPerSecond) {
			field.step = 1;
			const Range d = SkillDamage(field.spell, r);
			for (Monster *m : MonstersWithin(field.tile, MeteorRadius))
				Strike(player, *m, DamageType::Fire, Rolled(d));
			// The burst, then its ground burn for the rest of the field's life.
			if (!Show(player, MissileID::MeteorImpact, MissileGraphicID::MeteorImpact, field.tile, field.tile, field.ticksLeft))
				Ring(player, field.tile);
		} else if (field.step == 1 && field.clock % TicksPerSecond == 0) {
			const Range burn = MeteorBurn(r);
			for (Monster *m : MonstersWithin(field.tile, MeteorBurnRadius))
				Strike(player, *m, DamageType::Fire, Rolled(burn));
		}
		break;
	case SpellID::PoisonJavelin:
		if (field.clock % TicksPerSecond == 0) {
			const Range d = SkillDamage(field.spell, r);
			for (Monster *m : MonstersWithin(field.tile, 1))
				Strike(player, *m, DamageType::Acid, Rolled(d));
		}
		break;
	case SpellID::PlagueJavelin:
		if (field.clock % TicksPerSecond == 0) {
			const Range d = SkillDamage(field.spell, r);
			for (Monster *m : MonstersWithin(field.tile, ReachTiles(field.spell, r)))
				Strike(player, *m, DamageType::Acid, Rolled(d));
			if (!MissileArtLoaded(MissileGraphicID::AcidCloud))
				Ring(player, field.tile); // the cloud shows the pulse's reach while it hangs there
		}
		break;
	case SpellID::FurnaceMouth:
		if (field.clock % TicksPerSecond == 0) {
			const Range d = SkillDamage(field.spell, r);
			for (Monster *m : MonstersOnLine(field.tile, field.tile + field.dir, FurnaceMouthTiles))
				Strike(player, *m, DamageType::Fire, Rolled(d));
			// RfA-27 batch 53: the jet spat along the vent's facing, each of the four pulses; the ring without it.
			if (ArtFacing(player, MissileGraphicID::FurnaceMouth, field.tile, field.dir) == nullptr)
				Ring(player, field.tile);
			Impact(player, field.spell);
		}
		break;
	case SpellID::Firestorm:
		if (field.clock % FirestormPeriod == 0) {
			const Point landing = field.tile + Displacement { GenerateRnd(2 * FirestormScatter + 1) - FirestormScatter, GenerateRnd(2 * FirestormScatter + 1) - FirestormScatter };
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
			const Range d = SkillDamage(field.spell, r);
			for (Monster *m : MonstersWithin(field.tile, ReachTiles(field.spell, r)))
				Strike(player, *m, DamageType::Magic, Rolled(d));
			// RfA-27: the shades' sheet (CastOnce) shows the strikes; the ring without it. One cue, with the first.
			if (!MissileArtLoaded(MissileGraphicID::AncestralCourt))
				Ring(player, field.tile);
			if (field.clock == TicksPerSecond)
				Impact(player, field.spell);
		}
		break;
	default:
		break;
	}
}

/**
 * @brief Vanilla's Holy Bolt explosion (holyexpl) on @p tile, as a Paladin skill's impact: at @p percent of its size,
 * tinted @p rgb, its centre where Holy Bolt's full-size burst puts it (ScaleMissile keeps the centre). Drawn only; the
 * blow has landed. From the dev note "votive strike to use Holy Bolt Explosion animation on impact" (v1.12.214), the
 * Visual FX Schedule's comments (v1.12.215), and the sizes and hues picked on the Paladin Skill Cards page (v1.12.217).
 * False when holyexpl is not loaded (headless).
 */
bool HolyBurst(const Player &player, Point tile, int percent, uint32_t rgb)
{
	Missile *burst = Art(player, MissileGraphicID::HolyBoltExplosion, tile);
	if (burst == nullptr || !burst->_miAnimData)
		return false;
	if (rgb != 0) { // 0: vanilla's own colours (the Skill Cards pages' "None" tint)
		burst->oracoolTint = Tint::Hue;
		burst->oracoolTintRgb = rgb;
	}
	ScaleMissile(*burst, percent);
	return true;
}

/** @brief Loads @p art if it is one of the monsters' own sheets, which load only with the monster that uses them. */
void LoadMonsterOwnedArt(MissileGraphicID art)
{
	MissileFileData &data = GetMissileSpriteData(art);
	if (!HeadlessMode && !data.sprites && data.flags == MissileGraphicsFlags::MonsterOwned)
		data.LoadGFX();
}

/**
 * @brief RfA-27 batches 52-53: a swung skill's sheet - the arc or thrust from where he stood along his facing, drawn on
 * every swing the skill was paid for, and the strike flash on what the blow landed on (@p landedOn). Nothing while a
 * sheet is not in the archive; a swing drew nothing of its own before.
 */
void SwingArt(const Player &player, SpellID spell, Point from, Direction facing, std::optional<Point> landedOn)
{
	MissileGraphicID arc = MissileGraphicID::None;
	MissileGraphicID flash = MissileGraphicID::None;
	// The size and tint the Skill Cards pages picked for a sheet (100 and 0: as delivered).
	int artScale = 100;
	uint32_t artHue = 0;
	switch (spell) {
	// Half size, Paladin gold (the Barbarian Skill Cards page, 2026-09-29).
	case SpellID::Cleave: arc = MissileGraphicID::CleaveArc, artScale = 50, artHue = hue::PaladinGold; break;
	// Row n: the arc behind a hero facing n. Half size, fire orange (the same page).
	case SpellID::Backhand: arc = MissileGraphicID::BackhandArc, artScale = 50, artHue = hue::FireOrange; break;
	case SpellID::AegisSlam:
		// Holy Bolt's burst, half size, gold, on what the shield struck (Visual FX Schedule, 2026-09-28); was its
		// ChatGPT arc sheet.
		if (landedOn)
			HolyBurst(player, *landedOn, 50, hue::PaladinGold);
		break;
	case SpellID::Sweep: arc = MissileGraphicID::SweepArc; break;
	case SpellID::LowBranch: arc = MissileGraphicID::LowBranch; break;
	case SpellID::TurningPike: arc = MissileGraphicID::TurningPike; break;
	case SpellID::HolyLance:
		// Its own thrust sheet again (v1.12.211): the redrawn lance runs out along the facing through the two tiles behind
		// the target, and the user approved it in the animation review. The first one stood still on the hero, which is
		// why the Guided Arrow stood in for it from v1.12.201.
		//
		// It sets off from the struck monster's tile, not the hero's (user, 2026-09-29: "Holy Lance should initiate its
		// projectile from the tile the hit monster is, not from the hero tile") - where the blow landed, and where its
		// strike on the two tiles behind begins. A swing that hit nothing sends it from the tile in front.
		ArtFacing(player, MissileGraphicID::HolyLance, landedOn.value_or(from + facing), facing);
		break;
	case SpellID::ReapingPoint: arc = MissileGraphicID::ReapingPoint; break;
	case SpellID::Crusade:
		// Holy Bolt's burst where he stands - the sweep is all round him (Visual FX Schedule, 2026-09-28; was its ChatGPT
		// ring-slash sheet): 75%, holy blue since the Paladin Skill Cards page (2026-09-28).
		HolyBurst(player, from, 75, hue::HolyBlue);
		break;
	case SpellID::VotiveStrike:
		if (landedOn)
			HolyBurst(player, *landedOn, 25, hue::Infrared); // v1.12.214; a quarter size since the Skill Cards page
		break;
	// Holy Bolt's burst on the struck body (Visual FX Schedule, 2026-09-28; were their ChatGPT strike flashes), a quarter
	// size since the Paladin Skill Cards page: gold for Judgment, lavender for Oathbrand.
	case SpellID::Judgment:
		if (landedOn)
			HolyBurst(player, *landedOn, 25, hue::PaladinGold);
		break;
	case SpellID::Oathbrand:
		if (landedOn)
			HolyBurst(player, *landedOn, 25, hue::SpectralLavender);
		break;
	// Vanilla's Blood Star Blue at half size in place of its delivered strike (the Barbarian Skill Cards page, 2026-09-29).
	case SpellID::ClaspOfRuin: flash = MissileGraphicID::BloodStarBlue, artScale = 50; break;
	case SpellID::HammerOfTheAncients:
		// Holy Bolt's burst, full size, infrared (dev note, 2026-09-29); was the Blood Star at half size.
		if (landedOn)
			HolyBurst(player, *landedOn, 100, hue::Infrared);
		break;
	case SpellID::CinderTouch: flash = MissileGraphicID::CinderTouch; break;
	case SpellID::TigerClaw: flash = MissileGraphicID::TigerClaw; break;
	case SpellID::PressurePoint: flash = MissileGraphicID::PressurePoint; break;
	case SpellID::ExplodingPalm: flash = MissileGraphicID::ExplodingPalm; break;
	default: break;
	}
	Missile *drawn = nullptr;
	if (arc != MissileGraphicID::None)
		drawn = ArtFacing(player, arc, from, facing);
	if (flash != MissileGraphicID::None && landedOn) {
		LoadMonsterOwnedArt(flash); // the Blood Stars are monsters' sheets, loaded only with them
		drawn = Art(player, flash, *landedOn);
	}
	if (drawn != nullptr) {
		if (artHue != 0) {
			drawn->oracoolTint = Tint::Hue;
			drawn->oracoolTintRgb = artHue;
		}
		if (artScale != 100)
			ScaleMissile(*drawn, artScale); // the arcs and flashes keep their centre
	}
}

void TickLanding(Player &player, PlayerState &state)
{
	if (state.landingTicks <= 0 || --state.landingTicks > 0)
		return;
	const int r = state.landingRank;
	switch (state.landingSpell) {
	case SpellID::HeavensDescent: {
		const Range d = SkillDamage(state.landingSpell, r);
		for (Monster *m : MonstersWithin(player.position.tile, 1))
			Strike(player, *m, DamageType::Magic, Rolled(d));
		// Holy Bolt's burst, full size, gold, where he lands (Visual FX Schedule, 2026-09-28; was RfA-27 batch 55's
		// ChatGPT sheet), and its cue; the ring without the sheet.
		// 125%, Vengeance amber since the Paladin Skill Cards page (2026-09-28).
		if (!HolyBurst(player, player.position.tile, 125, hue::VengeanceAmber))
			Ring(player, player.position.tile);
		Impact(player, state.landingSpell);
		break;
	}
	case SpellID::LeapingCrane:
		if (Monster *m = NearestTo(player.position.tile, 1); m != nullptr)
			Strike(player, *m, DamageType::Physical, Percent(WeaponBlow(player), BlowPercent(state.landingSpell, r)), /*melee=*/true); // a weapon blow (round 15)
		Art(player, MissileGraphicID::LeapingCrane, player.position.tile); // RfA-27 batch 57: the landing's wind burst
		Impact(player, state.landingSpell);
		break;
	case SpellID::ShoulderGate:
		if (Monster *m = NearestTo(player.position.tile, 1); m != nullptr)
			Stagger(*m, StunTicks(state.landingSpell, r));
		Art(player, MissileGraphicID::ShoulderGate, player.position.tile); // RfA-27 batch 57: the impact ring
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
	// A swing needs something to swing: with a bow the hero drew the bow and struck in melee with the skill's bonus
	// (round 19 audit, v1.12.244).
	if (player.UsesRangedWeapon())
		return false;
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

int Rfa12MeleeBonusPercentFor(const Player &player, SpellID spell)
{
	if (!IsMeleeSpell(spell))
		return -1;
	return MeleeBonusPercent(spell, RankOf(player, spell));
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
	// Where the swing was thrown from and what it landed on, for its sheet: Turning Pike moves him, and a killing blow's
	// monster is gone by the time anything asks.
	const Point swungFrom = player.position.tile;
	const Direction swungToward = player._pdir;
	const std::optional<Point> landedOn = landed ? std::optional<Point>(front->position.tile) : std::nullopt;

	switch (spell) {
	case SpellID::VotiveStrike:
		if (landed && !alive) {
			TakeCorpseOf(*front);
			struck = true;
		}
		break;
	case SpellID::Judgment:
		if (alive) {
			MarksOf(*front).judgmentTicks = EffectTicks(spell, r);
			MarksOf(*front).judgmentPercent = EffectPercent(spell, r);
			struck = true;
		}
		break;
	case SpellID::Oathbrand:
		if (alive) {
			Marks &marks = MarksOf(*front);
			marks.oathTicks = EffectTicks(spell, r);
			marks.oathCharges = OathbrandCharges;
			marks.oathRank = r;
			struck = true;
		}
		break;
	case SpellID::HolyLance:
		// Each monster once, and not the one the swing itself struck: a walker stands on two lance tiles and took two
		// blows, and the front target a third (round 16 audit, v1.12.241).
		for (Monster *m : MonstersOnLine(ahead, ahead + player._pdir, 2)) {
			if (m != nullptr && m != front && Hittable(*m)) {
				Strike(player, *m, DamageType::Magic, Percent(WeaponBlow(player), BlowPercent(spell, r)), /*melee=*/true);
				struck = true;
			}
		}
		break;
	case SpellID::Crusade: {
		int blows = 0;
		for (Monster *m : MonstersWithin(player.position.tile, 1)) {
			if (m == front || blows >= CrusadeOthers)
				continue;
			Strike(player, *m, DamageType::Physical, Percent(WeaponBlow(player), BlowPercent(spell, r)), /*melee=*/true);
			blows++;
			struck = true;
		}
		break;
	}
	case SpellID::AegisSlam: {
		std::vector<const Monster *> slammed; // once each: a walker holds two of the three tiles (round 5 audit)
		for (const Point tile : std::array<Point, 3> { ahead, player.position.tile + Left(player._pdir), player.position.tile + Right(player._pdir) }) {
			Monster *m = FindMonsterAtPosition(tile);
			if (m == nullptr || !Hittable(*m) || std::find(slammed.begin(), slammed.end(), m) != slammed.end())
				continue;
			slammed.push_back(m);
			if (m != front)
				Strike(player, *m, DamageType::Physical, Percent(WeaponBlow(player), BlowPercent(spell, r)), /*melee=*/true);
			// Shoved first, then stunned: the shove's knockback put it into hit recovery over the stun (round 16 audit).
			Shove(*m, player._pdir);
			Stagger(*m, StunTicks(spell, r));
			struck = true;
		}
		break;
	}
	case SpellID::Backhand: {
		// A regular blow at the enemy in front, and the back of it at the one behind him (dev note, 2026-09-27:
		// "backhand to be a regular strike, not a cast. it hits main target infront and causes dmg to target behind
		// hero as well"). It was a cast that struck only behind, with no swing.
		const Point behindTile = player.position.tile + Opposite(player._pdir);
		Monster *behind = InDungeonBounds(behindTile) ? FindMonsterAtPosition(behindTile) : nullptr;
		if (behind != nullptr && behind != front && Hittable(*behind)) {
			Strike(player, *behind, DamageType::Physical, Percent(WeaponBlow(player), BlowPercent(spell, r)), /*melee=*/true);
			struck = true;
			landedBlows++;
		}
		break;
	}
	case SpellID::Cleave:
	case SpellID::Sweep: {
		// They strike the side tiles, so the vanilla axe or staff cleave stands aside, as for Sweeping Reed since round 26:
		// each side enemy took both blows (round 34 audit).
		NoteSideSweep();
		const Monster *first = nullptr; // a walker holds both side tiles on a diagonal facing: once, and one Rage (round 7 audit)
		for (const Point tile : { player.position.tile + Left(player._pdir), player.position.tile + Right(player._pdir) }) {
			Monster *m = FindMonsterAtPosition(tile);
			if (m == nullptr || m == front || m == first || !Hittable(*m))
				continue;
			first = m;
			Strike(player, *m, DamageType::Physical, Percent(WeaponBlow(player), BlowPercent(spell, r)), /*melee=*/true);
			struck = true;
			landedBlows++;
		}
		break;
	}
	case SpellID::ClaspOfRuin:
		if (alive) {
			Stagger(*front, StunTicks(spell, r));
			struck = true;
		}
		break;
	case SpellID::CinderTouch:
		if (alive) {
			// The longer and the stronger, as the poison takes them (round 35 audit).
			MarksOf(*front).burnTicks = std::max(MarksOf(*front).burnTicks, EffectTicks(spell, r));
			MarksOf(*front).burnDamage = std::max(MarksOf(*front).burnDamage, PerSecond(spell, r) << 6);
			struck = true;
		}
		break;
	case SpellID::ReapingPoint: {
		Monster *beyond = InDungeonBounds(ahead + player._pdir) ? FindMonsterAtPosition(ahead + player._pdir) : nullptr;
		if (beyond != nullptr && Hittable(*beyond)) {
			Strike(player, *beyond, DamageType::Physical, Percent(WeaponBlow(player), BlowPercent(spell, r)), /*melee=*/true);
			struck = true;
		}
		break;
	}
	case SpellID::TurningPike:
		if (landed) {
			for (const Point side : { player.position.tile + Left(player._pdir), player.position.tile + Right(player._pdir) }) {
				// PosOkPlayer, objects included: a barrel beside him passed, and the landing search put him elsewhere (round 20).
				if (InDungeonBounds(side) && PosOkPlayer(player, side) && dMonster[side.x][side.y] == 0) {
					TeleportTo(player, side, spell); // its impact cue is the moment's sound (RfA-27)
					break;
				}
			}
			struck = true;
		}
		break;
	case SpellID::LowBranch:
		if (alive) {
			ChillMonster(*front, SlowTicks(spell, r));
			MarksOf(*front).slowMarkTicks = SlowTicks(spell, r); // RfA-27 batch 58: the Slowed sigil
			struck = true;
		}
		break;
	case SpellID::StaffOfEchoes:
		if (alive && frontDamage > 0) {
			PlayerState &state = StateOf(player);
			state.echoTicks = EffectTicks(spell, r);
			state.echoMonster = static_cast<int>(front->getId());
			state.echoDamage = Percent(frontDamage, BlowPercent(spell, r));
			struck = true;
		}
		break;
	case SpellID::TigerClaw:
		if (alive) {
			Bleed(*front, EffectTicks(spell, r), PerSecond(spell, r));
			struck = true;
		}
		break;
	case SpellID::PressurePoint:
		if (alive) {
			ChillMonster(*front, SlowTicks(spell, r));
			DebuffMonster(*front, EffectTicks(spell, r), 0, -EffectPercent(spell, r));
			// RfA-27 batch 58: the Slowed and Armour-broken sigils.
			MarksOf(*front).slowMarkTicks = SlowTicks(spell, r);
			MarksOf(*front).armourBreakTicks = EffectTicks(spell, r);
			struck = true;
		}
		break;
	case SpellID::ExplodingPalm:
		if (alive) {
			MarksOf(*front).palmTicks = EffectTicks(spell, r);
			MarksOf(*front).palmRank = r;
			Bleed(*front, EffectTicks(spell, r), PerSecond(spell, r));
			struck = true;
		}
		break;
	default:
		break;
	}

	SwingArt(player, spell, swungFrom, swungToward, landedOn);
	// RfA-27 batch 51: the blow's impact cue, once a swing that landed on anything - the weapon's own swing sound stays.
	// Staff of Echoes' cue is its echo's (ProcessRfa12ActivesTick).
	if ((landed || struck) && spell != SpellID::StaffOfEchoes)
		Impact(player, spell);

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
	struct SightScope {
		explicit SightScope(Point from) { CastSightFrom = from; }
		~SightScope() { CastSightFrom = std::nullopt; }
	} sight { player.position.tile };
	const BowStrikeScope bow { IsBowSkill(spell) };
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
	// RfA-27 batch 55: three of these draw a floor burst of their own now (CastOnce), and the ring is only their stand-in.
	if (spell == SpellID::GroundStomp)
		return !MissileArtLoaded(MissileGraphicID::GroundStomp);
	if (spell == SpellID::MountainPole)
		return !MissileArtLoaded(MissileGraphicID::MountainPole);
	if (spell == SpellID::BlindingFlash)
		return !MissileArtLoaded(MissileGraphicID::BlindingFlash);
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

int Rfa12ActiveMeleeEvadeChance(const Player &player)
{
	const int r = BuffRank(player, Buff::Evasion);
	return r > 0 ? EffectPercent(SpellID::MantraOfEvasion, r) : 0;
}

bool Rfa12ActiveEvadesMelee(const Player &player)
{
	const int chance = Rfa12ActiveMeleeEvadeChance(player);
	return chance > 0 && GenerateRnd(100) < chance;
}

int Rfa12BoneShellFrame(const Player &player)
{
	const PlayerState &state = StateOf(player);
	if (BuffRank(player, Buff::BoneShell) <= 0 || state.bonePool <= 0)
		return -1;
	return GetAnimationFrame(12, 100); // twelve frames at ten a second, a slow orbit (the argument is ms a frame; it was 10, 100 fps)
}

size_t Rfa12BodyOverlays(const Player &player, Rfa12BodyOverlay *out, size_t capacity)
{
	// RfA-27 batch 56: the buffs worn as a loop around the body, each for as long as it lasts - drawn like the bone shell.
	struct Worn {
		Buff buff;
		MissileGraphicID art;
		int frames;
	};
	static constexpr Worn Overlays[] = {
		{ Buff::StaticCharge, MissileGraphicID::StaticCharge, 8 },
		{ Buff::Conduit, MissileGraphicID::Conduit, 8 },
		{ Buff::Immolate, MissileGraphicID::Immolate, 8 },
		{ Buff::Clarity, MissileGraphicID::MantraOfClarity, 12 },
		{ Buff::Evasion, MissileGraphicID::MantraOfEvasion, 12 },
		// Mantra of Retribution is a colour-cycled still since v1.12.211 (Rfa12RetributionWorn), and Astral Projection a
		// tint on the hero (Rfa12ActiveHidesPlayer): the user's animation review, 2026-09-27.
		{ Buff::Venom, MissileGraphicID::PoisonDagger, 8 },
	};
	size_t count = 0;
	for (const Worn &worn : Overlays) {
		if (count >= capacity)
			break;
		if (BuffRank(player, worn.buff) <= 0 || !MissileArtLoaded(worn.art))
			continue;
		out[count++] = { worn.art, GetAnimationFrame(worn.frames, 100) }; // ten frames a second (the argument is ms a frame)
	}
	return count;
}

uint16_t Rfa12SkillMarkers(const Monster &monster)
{
	if ((monster.hitPoints >> 6) <= 0)
		return 0;
	const Marks &marks = MarksOf(monster);
	uint16_t mask = 0;
	const auto mark = [&mask](SkillMarker marker) { mask = static_cast<uint16_t>(mask | (1U << static_cast<unsigned>(marker))); };
	if (marks.judgmentTicks > 0)
		mark(SkillMarker::Judged);
	if (marks.oathTicks > 0 && marks.oathCharges > 0)
		mark(SkillMarker::OathBranded);
	if (marks.frostbiteTicks > 0)
		mark(SkillMarker::Frostbitten);
	if (marks.ashenTicks > 0)
		mark(SkillMarker::AshenBranded);
	if (marks.huntTicks > 0)
		mark(SkillMarker::Hunted);
	for (const PlayerState &state : Players12) {
		if (state.claimTicks > 0 && state.claimMonster == static_cast<int>(monster.getId()))
			mark(SkillMarker::Claimed);
	}
	if (marks.pinTicks > 0)
		mark(SkillMarker::Pinned);
	if (MonsterBleeding(monster))
		mark(SkillMarker::Bleeding);
	if (marks.slowMarkTicks > 0 && IsMonsterChilled(monster))
		mark(SkillMarker::Slowed);
	if (marks.armourBreakTicks > 0)
		mark(SkillMarker::ArmourBroken);
	if (marks.rotTicks > 0 && marks.poisonTicks > 0)
		mark(SkillMarker::Rotting);
	if (IsCommandedTarget(monster))
		mark(SkillMarker::Commanded);
	return mask;
}

int Rfa12ActiveAbsorbDamage(Player &player, int damage)
{
	PlayerState &state = StateOf(player);
	if (damage <= 0)
		return damage;
	// Bone Armor (the Necromancer, 2026-09-18) first: it is the shell, the chord is the song.
	if (BuffRank(player, Buff::BoneShell) > 0 && state.bonePool > 0) {
		const int held = std::min(state.bonePool, damage);
		state.bonePool -= held;
		damage -= held;
		if (state.bonePool <= 0)
			state.ticks[static_cast<size_t>(Buff::BoneShell)] = 0;
		if (damage <= 0)
			return 0;
	}
	if (BuffRank(player, Buff::Chord) <= 0 || state.chordPool <= 0)
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

bool Rfa12RetributionWorn(const Player &player)
{
	return BuffRank(player, Buff::Retribution) > 0;
}

std::optional<double> Rfa12SerenityProgress(const Player &player)
{
	const int ticks = StateOf(player).serenityTicks;
	if (ticks <= 0)
		return std::nullopt;
	// Between game ticks by the frame's share of the next one, so the ring glides at any frame rate.
	const double between = std::min<int>(ProgressToNextGameTick, AnimationInfo::baseValueFraction) / static_cast<double>(AnimationInfo::baseValueFraction);
	return std::clamp((SerenityTicks - ticks + between) / SerenityTicks, 0.0, 1.0);
}

void OnRfa12ActiveHit(Player &player, Monster &monster, int damage, bool melee)
{
	Marks &marks = MarksOf(monster);
	// Poison Dagger (the Necromancer, 2026-09-18): every landed weapon blow poisons.
	if (melee && damage > 0 && BuffRank(player, Buff::Venom) > 0 && (monster.hitPoints >> 6) > 0)
		Poison(player, monster, PoisonTicks(SpellID::PoisonDagger), PerSecond(SpellID::PoisonDagger, BuffRank(player, Buff::Venom)));
	if (melee && marks.oathCharges > 0 && marks.oathTicks > 0 && (monster.hitPoints >> 6) > 0) {
		marks.oathCharges--;
		Strike(player, monster, DamageType::Magic, Rolled(SkillDamage(SpellID::Oathbrand, marks.oathRank)));
	}
	if (marks.tragedyTicks > 0 && damage > 0) {
		const int share = Percent(damage, marks.tragedyPercent);
		const Point at = monster.position.tile;
		const int saved = marks.tragedyTicks;
		marks.tragedyTicks = 0; // what it shares cannot come back to it
		for (Monster *m : MonstersWithin(at, 1)) {
			if (m != &monster)
				Strike(player, *m, DamageType::Magic, share, /*melee=*/false, /*applyPassives=*/false); // a share (round 14)
		}
		marks.tragedyTicks = saved;
	}
}

void OnRfa12ActiveStruck(Player &player, Monster &monster)
{
	// RfA-27 batch 51: each discharge's own impact cue.
	if (const int r = BuffRank(player, Buff::StaticCharge); r > 0) {
		Strike(player, monster, DamageType::Lightning, Rolled(SkillDamage(SpellID::StaticCharge, r)));
		Impact(player, SpellID::StaticCharge);
	}
	if (const int r = BuffRank(player, Buff::Retribution); r > 0 && (monster.hitPoints >> 6) > 0) {
		Strike(player, monster, DamageType::Magic, Rolled(SkillDamage(SpellID::MantraOfRetribution, r)));
		Impact(player, SpellID::MantraOfRetribution);
	}
}

void OnRfa12ActiveMissileStruck(Player &player, Monster &monster, int damage)
{
	if (const int r = BuffRank(player, Buff::Feedback); r > 0 && damage > 0)
		Strike(player, monster, DamageType::Magic, Percent(damage, std::min(30 + 2 * (r - 1), 70)));
}

void OnRfa12ActiveMonsterKilled(Player &player, const Monster &monster)
{
	// A burst on a death is not an arrow, though an arrow's Strike made the kill: Ashen Brand's and Exploding Palm's blows
	// fed the arrow-only passives (round 34 audit).
	BowStrikeScope notAnArrow { false };
	// Nor behind the cast's sight gate: a burst sees from the body, as it does after a swing or a field tick (round 35).
	const NoCastSight burstSees;
	Marks &marks = MarksOf(monster);
	const Point at = monster.position.tile;
	if (marks.ashenTicks > 0) {
		const int r = marks.ashenRank;
		marks.ashenTicks = 0;
		for (Monster *m : MonstersWithin(at, 1))
			Strike(player, *m, DamageType::Fire, Rolled(SkillDamage(SpellID::AshenBrand, r)));
		// RfA-27: the branded body bursting (batch 55) with its cue (batch 51); the ring without the sheet.
		if (Art(player, MissileGraphicID::AshenBurst, at) == nullptr)
			Ring(player, at);
		Impact(player, SpellID::AshenBrand);
	}
	if (marks.palmTicks > 0) {
		const int r = marks.palmRank;
		marks.palmTicks = 0;
		for (Monster *m : MonstersWithin(at, 1))
			Strike(player, *m, DamageType::Physical, Percent(WeaponBlow(player), BlowPercent(SpellID::ExplodingPalm, r)), /*melee=*/true); // round 15
		if (Art(player, MissileGraphicID::ExplodingPalmBurst, at) == nullptr) // RfA-27 batch 55; the ring without it
			Ring(player, at);
	}
	if (const int r = BuffRank(player, Buff::Bloodcall); r > 0) {
		Heal(player, BloodcallLife(r) << 6);
		GainRage(player, BloodcallRage(r)); // Rage, not mana: Bloodcall is the Barbarian's (2026-09-13)
	}
	marks = Marks {};
}

void ApplyRfa12ActiveBuffsToTotals(const Player &player, ItemBonusTotals &totals)
{
	if (const int r = BuffRank(player, Buff::IronWill); r > 0) {
		totals.fireResist += EffectPercent(SpellID::IronWill, r);
		totals.lightningResist += EffectPercent(SpellID::IronWill, r);
		totals.magicResist += EffectPercent(SpellID::IronWill, r);
		totals.coldResist += EffectPercent(SpellID::IronWill, r);
	}
	if (const int r = BuffRank(player, Buff::Conduit); r > 0)
		totals.fastCast += EffectPercent(SpellID::Conduit, r);
	if (BuffRank(player, Buff::Saga) > 0)
		totals.spellLevelAdd += 2;
	if (BuffRank(player, Buff::Astral) > 0)
		totals.moveSpeed += EffectPercent(SpellID::AstralProjection, 1);
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
		RestoreMana(player, ManaFlowPerTick(r));
	if (const int r = BuffRank(player, Buff::Conduit); r > 0)
		RestoreMana(player, ManaFlowPerTick(r));
	if (const int r = BuffRank(player, Buff::Immolate); r > 0 && state.ticks[static_cast<size_t>(Buff::Immolate)] % TicksPerSecond == 0) {
		const Range d = SkillDamage(SpellID::Immolate, r);
		const auto burning = MonstersWithin(player.position.tile, 1);
		for (Monster *m : burning)
			Strike(player, *m, DamageType::Fire, Rolled(d));
		if (!burning.empty())
			Impact(player, SpellID::Immolate); // RfA-27: the soft burn pulse, a second, while it burns something
	}
	if (const int r = BuffRank(player, Buff::Spheres); r > 0 && state.ticks[static_cast<size_t>(Buff::Spheres)] % 10 == 0) {
		const Range d = Scale(r, 1, 3, 1, 1);
		for (Monster *m : MonstersWithin(player.position.tile, 2))
			Strike(player, *m, DamageType::Magic, Rolled(d));
	}

	// Wrath of the Heavens: a pillar every 12 ticks on a monster within five tiles.
	if (state.wrathPillars > 0 && ++state.wrathClock % WrathPillarTicks == 0) {
		state.wrathPillars--;
		const auto nearby = MonstersWithin(player.position.tile, ReachTiles(SpellID::WrathOfTheHeavens, state.wrathRank));
		if (!nearby.empty()) {
			Monster &m = *nearby[static_cast<size_t>(GenerateRnd(static_cast<int>(nearby.size())))];
			const Point at = m.position.tile;
			Strike(player, m, DamageType::Magic, Rolled(SkillDamage(SpellID::WrathOfTheHeavens, state.wrathRank)));
			// RfA-27: the pillar slamming down (batch 55) and its cue (batch 51), each of the five; the ring without the sheet.
			// Three quarters size since the Paladin Skill Cards page (2026-09-28), its foot still on the floor: the floor
			// point sits the delivery anchor's offset + 16 above the sheet's bottom edge (CensusEffectOffset's rule).
			if (Missile *pillar = Art(player, MissileGraphicID::WrathPillar, at); pillar == nullptr)
				Ring(player, at);
			else
				ScaleMissile(*pillar, 75, pillar->position.offset.deltaY + 16);
			Impact(player, SpellID::WrathOfTheHeavens);
		}
	}

	// Funeral Star: stand still until it bursts.
	if (state.funeralTicks > 0) {
		if (player.position.tile != state.funeralFrom) {
			state.funeralTicks = 0;
			EndArtEffects(state.funeralTile, MissileGraphicID::FuneralStarCharge, static_cast<int>(player.getId())); // she moved
		} else if (--state.funeralTicks == 0) {
			const Range d = SkillDamage(SpellID::FuneralStar, state.funeralRank);
			for (Monster *m : MonstersWithin(state.funeralTile, ReachTiles(SpellID::FuneralStar, state.funeralRank)))
				Strike(player, *m, DamageType::Fire, Rolled(d));
			// RfA-27: the star bursting (batch 55) and its cue (batch 51); the ring without the sheet.
			if (Art(player, MissileGraphicID::FuneralStarBurst, state.funeralTile) == nullptr)
				Ring(player, state.funeralTile);
			Impact(player, SpellID::FuneralStar);
		}
	}

	if (state.crucibleTicks > 0)
		state.crucibleTicks--;
	if (state.claimTicks > 0)
		state.claimTicks--;

	// Staff of Echoes: the blow lands again.
	if (state.echoTicks > 0 && --state.echoTicks == 0 && state.echoMonster >= 0) {
		Monster &m = Monsters[state.echoMonster];
		if (Hittable(m) && player.position.tile.WalkingDistance(m.position.tile) <= 1) {
			// RfA-27: the ghost of the blow landing again (batch 52), and the echo's cue (batch 51).
			Art(player, MissileGraphicID::StaffEcho, m.position.tile);
			Strike(player, m, DamageType::Physical, state.echoDamage, /*melee=*/true, /*applyPassives=*/false); // round 14
			Impact(player, SpellID::StaffOfEchoes);
		}
		state.echoMonster = -1;
	}

	TickLanding(player, state);
	if (state.serenityTicks > 0)
		state.serenityTicks--;

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
		         &marks.satireTicks, &marks.ashenTicks, &marks.palmTicks, &marks.brittleCooldown, &marks.pinTicks,
		         &marks.slowMarkTicks, &marks.armourBreakTicks, &marks.rotTicks }) {
			if (*ticks > 0)
				(*ticks)--;
		}
		if (marks.oathTicks == 0)
			marks.oathCharges = 0;
		// One pulse per second of its own (round 35 audit); one pulse a second of the duration, as before.
		// A dose's last part-second strikes too, as the remaining-count pulse did (round 36 audit: Virulence's 6.25 s dealt 6).
		const auto pulse = [](int &ticks, int &clock) {
			ticks--;
			const bool due = ++clock >= TicksPerSecond || ticks == 0;
			if (due)
				clock = 0;
			return due;
		};
		if (marks.burnTicks > 0) {
			if (pulse(marks.burnTicks, marks.burnPulse))
				Strike(player, m, DamageType::Fire, marks.burnDamage);
			if (marks.burnTicks == 0)
				marks.burnDamage = 0; // a lower-rank burn later is its own strength (round 36 audit)
		}
		if (marks.poisonTicks > 0) {
			if (pulse(marks.poisonTicks, marks.poisonPulse))
				Strike(player, m, DamageType::Acid, marks.poisonDamage);
			if (marks.poisonTicks == 0)
				marks.poisonDamage = 0;
		}
		if (marks.elegyTicks > 0) {
			if (pulse(marks.elegyTicks, marks.elegyPulse))
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
	// And every other record that names this monster by slot (audit, 2026-09-27): the slot goes to the next monster, and
	// a Hunter's Claim, an echo or a thread still naming it would act on a stranger - a claim kept alive by a newcomer
	// makes the arrows pass every non-unique by.
	const int id = static_cast<int>(monster.getId());
	for (PlayerState &state : Players12) {
		if (state.claimMonster == id) {
			state.claimMonster = -1;
			state.claimTicks = 0;
		}
		if (state.echoMonster == id) {
			state.echoMonster = -1;
			state.echoTicks = 0;
		}
	}
	for (Marks &marks : MonsterMarks) {
		if (marks.threadPartner == id) {
			marks.threadPartner = -1;
			marks.threadTicks = 0;
		}
	}
}

void ClearRfa12ActiveBuffs(Player &player)
{
	// A new game: no companions. They are statics and would otherwise follow one character into the next.
	ForgetCompanions();
	ClearNecromancerSummoningState();
	ClearAllCurses();
	ForgetRfa12Clocks(); // Mercy's cooldown and Soft Tread's quiet survive the stairs, not a new game
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

void ClearRfa12PlayerBuffs(Player &player)
{
	StateOf(player) = PlayerState {};
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
	// The Necromancer's (2026-09-26). Bone Armor's clock is zeroed when its pool is spent, so the row goes with the shell.
	case SpellID::BoneArmor: buff = Buff::BoneShell; break;
	case SpellID::PoisonDagger: buff = Buff::Venom; break;
	case SpellID::BoneStorm: {
		// Not a buff but a field that follows him: the longest of his storms still blowing.
		int ticks = 0;
		for (const Field &field : Fields) {
			if (field.spell == SpellID::BoneStorm && field.owner == player.getId() && field.ticksLeft > ticks)
				ticks = field.ticksLeft;
		}
		return ticks;
	}
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

bool DrawHolyBurst(const Player &player, Point tile, int percent, uint32_t rgb)
{
	return HolyBurst(player, tile, percent, rgb);
}


// =================================================================================================
// The tooltip (the two rules, user 2026-09-26): what a skill does at a rank, from the helpers above
// =================================================================================================

namespace {

/** @brief @p ticks as seconds: whole where they are whole, else to a tenth. */
std::string Secs(int ticks)
{
	if (ticks % TicksPerSecond == 0)
		return fmt::format("{:d}", ticks / TicksPerSecond);
	return fmt::format("{:.1f}", static_cast<double>(ticks) / TicksPerSecond);
}

/** @brief Mana a second from a flow of @p perTick in 1/64 points a tick. */
double ManaPerSecond(int perTick)
{
	return static_cast<double>(perTick * TicksPerSecond) / 64.0;
}

struct FactLines {
	std::string text;
	void add(const std::string &line)
	{
		if (line.empty())
			return;
		if (!text.empty())
			text += '\n';
		text += line;
	}
};

} // namespace

std::string Rfa12ActiveFactsAt(const Player &player, SpellID spell, int rank)
{
	if (IsNecromancerCurse(spell))
		return CurseFactsAt(player, spell, rank);
	if (IsNecromancerSummoning(spell))
		return NecroSummoningFactsAt(player, spell, rank);
	if (IsCompanionSpell(spell))
		return CompanionFactsAt(spell, rank); // Valkyrie, Decoy, Ancestral Call, Spirit Guardian

	const int r = std::max(rank, 1);
	FactLines out;
	const auto say = [&out](auto format, const auto &...args) { out.add(fmt::format(fmt::runtime(format), args...)); };
	const auto blowBonus = [&]() { say(_("Damage: +{:d}%"), MeleeBonusPercent(spell, r)); };
	const auto bleed = [&]() { say(_("Bleed: {:d} a second for {} s"), PerSecond(spell, r), Secs(EffectTicks(spell, r))); };
	const auto duration = [&]() { say(_("Duration: {} s"), Secs(EffectTicks(spell, r))); };
	const auto poison = [&]() {
		const PoisonDose dose = VirulentDose(player, PoisonTicks(spell), PerSecond(spell, r));
		say(_("Poison: {:.1f} a second for {} s"), dose.perSecond64 / 64.0, Secs(dose.ticks));
	};
	Range d = SkillDamage(spell, r);
	// The cold skills with Cold Mastery's share, as Strike deals them and as Ice Bolt's hover already shows (round 29 audit).
	if (IsAnyOf(spell, SpellID::ChillTouch, SpellID::IceNeedle, SpellID::IceLance, SpellID::BrittleGround, SpellID::Whiteout, SpellID::AbsoluteZero)) {
		const int mastery = ColdMasteryDamagePercent(player);
		d = { d.min + d.min * mastery / 100, d.max + d.max * mastery / 100 };
	}
	const Range bone = BoneRange(player, d);
	const int blow = BlowPercent(spell, r);
	const int reach = ReachTiles(spell, r);

	switch (spell) {
	// ---------------- Paladin ----------------
	case SpellID::VotiveStrike:
		blowBonus();
		say(_("An enemy it kills leaves no corpse"));
		break;
	case SpellID::Judgment:
		blowBonus();
		say(_("Mark: the target takes +{:d}% damage for {} s"), EffectPercent(spell, r), Secs(EffectTicks(spell, r)));
		break;
	case SpellID::Oathbrand:
		blowBonus();
		say(_("Brand: your next {:d} blows each add {:d} - {:d} magic damage, for {} s"), OathbrandCharges, d.min, d.max, Secs(EffectTicks(spell, r)));
		break;
	case SpellID::HolyLance:
		say(_("The two tiles beyond the target: {:d}% of a blow, as magic"), blow);
		break;
	case SpellID::Crusade:
		say(_("Also hits up to {:d} other enemies beside you at {:d}% of a blow"), CrusadeOthers, blow);
		break;
	case SpellID::AegisSlam:
		say(_("The two beside the target: {:d}% of a blow"), blow);
		say(_("Stun and knockback on the three tiles ahead: {} s"), Secs(StunTicks(spell, r)));
		say(_("Requires a shield"));
		break;
	case SpellID::HeavensDescent:
		say(_("Leap: up to {:d} tiles"), reach);
		say(_("Magic damage: {:d} - {:d} to everything beside you"), d.min, d.max);
		break;
	case SpellID::WrathOfTheHeavens:
		say(_("Pillars: {:d} over {} s, each on an enemy within {:d} tiles"), WrathPillars, Secs(WrathPillars * WrathPillarTicks), reach);
		say(_("Magic damage: {:d} - {:d} a pillar"), d.min, d.max);
		break;
	// ---------------- Barbarian ----------------
	case SpellID::Cleave:
		say(_("Also hits the enemies beside you at {:d}% of a blow"), blow);
		break;
	case SpellID::Backhand:
		say(_("The enemy behind you: {:d}% of a blow"), blow);
		break;
	case SpellID::GroundStomp:
		say(_("Stun: {} s, everything beside you (uniques shrug it off)"), Secs(StunTicks(spell, r)));
		break;
	case SpellID::Rend:
		bleed();
		say(_("Radius: {:d} tiles"), reach);
		break;
	case SpellID::HammerOfTheAncients:
		blowBonus();
		break;
	case SpellID::SeismicSlam:
		say(_("Damage: {:d}% of a blow, everything on a {:d}-tile line"), blow, reach);
		break;
	case SpellID::ClaspOfRuin:
		blowBonus();
		say(_("Hold: {} s (uniques shrug it off)"), Secs(StunTicks(spell, r)));
		break;
	case SpellID::Earthquake:
		say(_("Damage: {:d}% of a blow a second for {} s, within {:d} tiles"), blow, Secs(EffectTicks(spell, r)), reach);
		break;
	case SpellID::ThreateningShout:
		say(_("Enemy damage: -{:d}% for {} s"), EffectPercent(spell, r), Secs(EffectTicks(spell, r)));
		say(_("Radius: {:d} tiles"), reach);
		break;
	case SpellID::RallyingCry:
		say(_("Heals: {:d}% of your life over {} s"), EffectPercent(spell, r), Secs(EffectTicks(spell, r)));
		break;
	case SpellID::Intimidate:
		say(_("Enemy armour: -{:d}% for {} s"), EffectPercent(spell, r), Secs(EffectTicks(spell, r)));
		say(_("Radius: {:d} tiles"), reach);
		break;
	case SpellID::SplitRanks:
		say(_("Shoves aside the enemies ahead of you within {:d} tiles"), reach);
		break;
	case SpellID::IronWill:
		say(_("Fire, lightning, cold and magic resistance: +{:d}%"), EffectPercent(spell, r));
		duration();
		break;
	case SpellID::Bloodcall:
		say(_("Every kill: +{:d} life, +{:d} Rage"), BloodcallLife(r), BloodcallRage(r));
		duration();
		break;
	case SpellID::EarthshakerCry:
		say(_("Magic damage: {:d} - {:d} within {:d} tiles"), d.min, d.max, reach);
		say(_("Stun: {} s (uniques shrug it off)"), Secs(StunTicks(spell, r)));
		break;
	case SpellID::WeaponThrow:
		say(_("Damage: the sword or axe in your hand, thrown, with half your Strength bonus"));
		break;
	// ---------------- Sorceress ----------------
	case SpellID::ChillTouch:
		say(_("Cold damage: {:d} - {:d} on the three tiles ahead"), d.min, d.max);
		say(_("Chill: {} s"), Secs(SlowTicks(spell, r)));
		break;
	case SpellID::IceNeedle:
		say(_("Cold damage: {:d} - {:d} to the first {:d} enemies on a {:d}-tile line"), d.min, d.max, IceNeedleTargets, reach);
		say(_("Chill: {} s"), Secs(SlowTicks(spell, r)));
		break;
	case SpellID::Frostbite:
		say(_("Cold damage taken: +{:d}% for {} s"), EffectPercent(spell, r), Secs(EffectTicks(spell, r)));
		say(_("Chill: {} s"), Secs(SlowTicks(spell, r)));
		break;
	case SpellID::IceLance:
		say(_("Cold damage: {:d} - {:d} to everything on a {:d}-tile line"), d.min, d.max, reach);
		say(_("Chill: {} s"), Secs(SlowTicks(spell, r)));
		break;
	case SpellID::BrittleGround:
		say(_("Cold damage: {:d} - {:d} to an enemy walking across, at most once a second"), d.min, d.max);
		say(_("Chill: {} s"), Secs(SlowTicks(spell, r)));
		duration();
		break;
	case SpellID::FrozenSentinel: {
		say(_("An Ice Bolt at level {:d} every {} s, at the nearest enemy within {:d} tiles"), r, Secs(FrozenSentinelPeriod), reach);
		int min = -1;
		int max = -1;
		ColdSpellDamage(player, SpellID::IceBolt, r, min, max);
		if (min >= 0)
			say(_("Cold damage: {:d} - {:d} a bolt"), min, max);
		duration();
		break;
	}
	case SpellID::Whiteout:
		say(_("Cold damage: {:d} - {:d}, once to each enemy in a three-tile wall rolling {:d} tiles"), d.min, d.max, WhiteoutTicks / WhiteoutStepTicks);
		say(_("Chill: {} s"), Secs(SlowTicks(spell, r)));
		break;
	case SpellID::AbsoluteZero:
		say(_("Cold damage: {:d} - {:d} within {:d} tiles"), d.min, d.max, reach);
		say(_("Freeze: {} s (uniques are chilled instead)"), Secs(SlowTicks(spell, r)));
		break;
	case SpellID::Arc:
		say(_("Lightning damage: {:d} - {:d}"), d.min, d.max);
		say(_("Leaps to {:d} more within {:d} tiles, each at {:d}% of the strike before"), ArcHops - 1, reach, ArcFalloffPercent);
		break;
	case SpellID::StaticCharge:
		say(_("Lightning damage to each melee attacker: {:d} - {:d}"), d.min, d.max);
		duration();
		break;
	case SpellID::BallLightning: {
		say(_("A charged bolt at every tile, for {:d} tiles"), BallLightningTicks / BallLightningStepTicks);
		if (MyPlayer != nullptr) {
			int min = -1;
			int max = -1;
			GetDamageAmtAtLevel(SpellID::ChargedBolt, r, &min, &max);
			if (min >= 0)
				say(_("Lightning damage: {:d} - {:d} a bolt"), min, max);
		}
		break;
	}
	case SpellID::Conduit:
		say(_("Faster cast rate: +{:d}%"), EffectPercent(spell, r));
		say(_("Mana: +{:.1f} a second"), ManaPerSecond(ManaFlowPerTick(r)));
		duration();
		break;
	case SpellID::LightningRod:
		say(_("Swallows the first enemy lightning missile to come near and bursts: {:d} - {:d} lightning damage within {:d} tiles"), d.min, d.max, LightningRodBurstRadius);
		duration();
		break;
	case SpellID::FaradayRing:
		say(_("Destroys every enemy missile within {:d} tiles of it"), FaradayRingReach);
		duration();
		break;
	case SpellID::StormCrucible:
		say(_("Lightning damage: {:d} - {:d} along the line between the pair, {:d} times"), d.min, d.max,
		    PulseCount(CruciblePeriod - 1, CrucibleTicks, CruciblePeriod));
		say(_("Place the pair within {} s"), Secs(EffectTicks(spell, r)));
		break;
	case SpellID::RideTheLightning:
		say(_("Lightning damage: {:d} - {:d} to everything on the way"), d.min, d.max);
		say(_("Range: {:d} tiles"), reach);
		break;
	case SpellID::CinderTouch:
		say(_("Fire damage: {:d} a second for {} s"), PerSecond(spell, r), Secs(EffectTicks(spell, r)));
		break;
	case SpellID::EmberMine:
		say(_("Fire damage: {:d} - {:d} to everything beside it when stepped on"), d.min, d.max);
		duration();
		break;
	case SpellID::FlameRing:
		say(_("Fire damage: {:d} - {:d} within {:d} tiles"), d.min, d.max, reach);
		break;
	case SpellID::AshenBrand:
		say(_("If it dies: {:d} - {:d} fire damage to everything beside it"), d.min, d.max);
		duration();
		break;
	case SpellID::FurnaceMouth:
		say(_("Fire damage: {:d} - {:d} on the {:d} tiles ahead, {:d} times a second apart"), d.min, d.max, FurnaceMouthTiles,
		    PulseCount(TicksPerSecond - 1, FurnaceMouthTicks, TicksPerSecond));
		break;
	case SpellID::Firestorm: {
		// Aimed, not dropped: each ball flies from the hero toward its point and bursts on the first thing it meets (round 20).
		say(_("Fireballs at level {:d}: {:d} over {} s, aimed within {:d} tiles of the cursor"), r,
		    PulseCount(0, EffectTicks(spell, r), FirestormPeriod), Secs(EffectTicks(spell, r)), FirestormScatter);
		if (MyPlayer != nullptr) {
			int min = -1;
			int max = -1;
			GetDamageAmtAtLevel(SpellID::Fireball, r, &min, &max);
			if (min >= 0)
				say(_("Fire damage: {:d} - {:d} a fireball"), min, max);
		}
		break;
	}
	case SpellID::Immolate:
		say(_("Fire damage: {:d} - {:d} a second to everything beside you"), d.min, d.max);
		duration();
		break;
	case SpellID::FuneralStar:
		say(_("Fire damage: {:d} - {:d} within {:d} tiles"), d.min, d.max, reach);
		say(_("Stand still for {} s; moving cancels it"), Secs(EffectTicks(spell, r)));
		break;
	case SpellID::Meteor: {
		const Range burn = MeteorBurn(r);
		say(_("Fire damage: {:d} - {:d} within {:d} tiles, a second after the cast"), d.min, d.max, MeteorRadius);
		say(_("Burning ground: {:d} - {:d} a second for {:d} s"), burn.min, burn.max, PulseCount(0, EffectTicks(spell, r), TicksPerSecond) - 1);
		break;
	}
	// ---------------- Rogue ----------------
	case SpellID::BarbedShaft:
		say(_("Damage: {:d}% of an arrow"), blow);
		bleed();
		break;
	case SpellID::ShockArrow:
		say(_("Damage: {:d}% of an arrow"), blow);
		say(_("Lightning damage: {:d} - {:d} to one more enemy within {:d} tiles"), d.min, d.max, reach);
		break;
	case SpellID::PiercingShot:
		say(_("Damage: {:d}% of an arrow, every enemy on a {:d}-tile line"), blow, reach);
		break;
	case SpellID::RainOfArrows:
		say(_("Damage: {:d}% of an arrow, everything within {:d} tiles of the cursor"), blow, reach);
		break;
	case SpellID::CripplingShot:
		say(_("Damage: {:d}% of an arrow"), blow);
		say(_("Slow: half speed for {} s"), Secs(SlowTicks(spell, r)));
		break;
	case SpellID::HuntersMark:
		say(_("Arrow damage taken: +{:d}% for {} s"), EffectPercent(spell, r), Secs(EffectTicks(spell, r)));
		break;
	case SpellID::Barrage:
		say(_("Arrows: {:d} at one target, each {:d}% of an arrow"), BarrageArrows, blow);
		break;
	case SpellID::PhantomVolley:
		say(_("Magic damage: {:d}% of an arrow, every enemy within {:d} tiles"), blow, reach);
		break;
	case SpellID::ShadowStep:
		say(_("Steps to the far side of an enemy within {:d} tiles of the cursor"), reach);
		break;
	case SpellID::HuntersClaim:
		say(_("Your arrows pass every ordinary monster on the way to the claimed one, for {} s"), Secs(EffectTicks(spell, r)));
		break;
	case SpellID::Sweep:
		say(_("Also hits the enemies beside you at {:d}% of a blow"), blow);
		break;
	case SpellID::Harpoon:
		say(_("Damage: {:d}% of a blow, the first enemy on an {:d}-tile line"), blow, reach);
		say(_("Drags it a tile toward you; stun {} s"), Secs(StunTicks(spell, r)));
		break;
	case SpellID::Vault:
		say(_("Range: {:d} tiles"), reach);
		break;
	case SpellID::ReapingPoint:
		blowBonus();
		say(_("The enemy behind your target: {:d}% of a blow"), blow);
		break;
	case SpellID::AnchorJavelin:
		say(_("Damage: {:d}% of a blow, the first enemy on an {:d}-tile line"), blow, reach);
		say(_("Pin: {} s (uniques only take the damage)"), Secs(StunTicks(spell, r)));
		break;
	case SpellID::TurningPike:
		blowBonus();
		say(_("You pivot to a free tile beside the target"));
		break;
	case SpellID::ValkyriesSpear:
		say(_("Damage: {:d}% of a blow, everything within {:d} tile of the cursor"), blow, reach);
		break;
	case SpellID::PoisonJavelin:
		say(_("Acid damage: {:d}% of a blow, the first enemy on an {:d}-tile line"), blow, reach);
		say(_("Acid pool: {:d} - {:d} a second for {:d} s, beside it"), d.min, d.max, PulseCount(0, EffectTicks(spell, r), TicksPerSecond));
		break;
	case SpellID::PlagueJavelin:
		say(_("Acid damage: {:d} - {:d} a second for {:d} s, within {:d} tiles"), d.min, d.max, PulseCount(0, EffectTicks(spell, r), TicksPerSecond), reach);
		break;
	// ---------------- Monk ----------------
	case SpellID::LongThrust:
		say(_("Damage: {:d}% of a blow, the first enemy within {:d} tiles"), blow, reach);
		break;
	case SpellID::LowBranch:
		say(_("Slow: half speed for {} s"), Secs(SlowTicks(spell, r)));
		break;
	case SpellID::RearwardReach:
		say(_("The enemy behind you: {:d}% of a blow"), blow);
		break;
	case SpellID::MountainPole:
		say(_("Damage: {:d}% of a blow, everything beside you"), blow);
		say(_("Stun: {} s"), Secs(StunTicks(spell, r)));
		break;
	case SpellID::BambooRain:
		say(_("Damage: {:d}% of a blow, up to {:d} enemies within {:d} tiles"), blow, BambooRainTargets, reach);
		break;
	case SpellID::DragonTailSweep:
	case SpellID::WhirlingKick:
		say(_("Damage: {:d}% of a blow, everything beside you"), blow);
		say(_("Knocks back"));
		break;
	case SpellID::StaffOfEchoes:
		say(_("Echo: {:d}% of the blow lands again after {} s"), blow, Secs(EffectTicks(spell, r)));
		break;
	case SpellID::HeavenSplitter:
	case SpellID::DragonsWrath:
		say(_("Damage: {:d}% of a blow, everything on a {:d}-tile line"), blow, reach);
		break;
	case SpellID::ThousandReeds:
		say(_("Damage: {:d}% of a blow, every enemy within {:d} tiles"), blow, reach);
		break;
	case SpellID::TigerClaw:
		blowBonus();
		bleed();
		break;
	case SpellID::LeapingCrane:
		say(_("Range: {:d} tiles"), reach);
		say(_("Landing: {:d}% of a blow to the enemy beside you"), blow);
		break;
	case SpellID::PressurePoint:
		say(_("Slow: half speed for {} s"), Secs(SlowTicks(spell, r)));
		say(_("Enemy armour: -{:d}% for {} s"), EffectPercent(spell, r), Secs(EffectTicks(spell, r)));
		break;
	case SpellID::ShoulderGate:
		say(_("Rush: up to {:d} tiles"), reach);
		say(_("Stun: {} s"), Secs(StunTicks(spell, r)));
		break;
	case SpellID::SevenSidedStrike:
		say(_("Damage: {:d}% of a blow, up to {:d} enemies within {:d} tiles of the cursor"), blow, SevenSidedTargets(r), reach);
		break;
	case SpellID::ExplodingPalm:
		blowBonus();
		bleed();
		say(_("If it dies meanwhile: {:d}% of a blow to everything beside it"), blow);
		break;
	case SpellID::MantraOfClarity:
		say(_("Mana: +{:.1f} a second"), ManaPerSecond(ManaFlowPerTick(r)));
		duration();
		break;
	case SpellID::MantraOfEvasion:
		say(_("Melee blows that miss you: {:d}%"), EffectPercent(spell, r));
		duration();
		break;
	case SpellID::ChiWave:
		say(_("Magic damage: {:d} - {:d}, up to {:d} enemies, leaping {:d} tiles"), d.min, d.max, ChiWaveHops, reach);
		break;
	case SpellID::BlindingFlash:
		say(_("Enemies within {:d} tiles are blinded and wander off (uniques shrug it off)"), reach);
		break;
	case SpellID::MantraOfRetribution:
		say(_("Magic damage to each melee attacker: {:d} - {:d}"), d.min, d.max);
		duration();
		break;
	case SpellID::Serenity:
		say(_("Ends every slow and chill on you"));
		break;
	case SpellID::WaveOfLight:
		say(_("Magic damage: {:d} - {:d} within {:d} tiles"), d.min, d.max, reach);
		break;
	case SpellID::AstralProjection:
		say(_("Movement speed: +{:d}%, and monsters that have not seen you do not notice you"), EffectPercent(spell, r));
		duration();
		break;
	case SpellID::AncestralCourt:
		say(_("Magic damage: {:d} - {:d}, {:d} strikes within {:d} tiles"), d.min, d.max, AncestralCourtStrikes, reach);
		break;
	// ---------------- Necromancer: Poison & Bone (bone damage includes Marrow, poison Virulence) ----------------
	case SpellID::Teeth:
		say(_("Magic damage: {:d} - {:d} a tooth"), bone.min, bone.max);
		say(_("Teeth: 3 across the front, and {:d} down the line"), TeethDownTheLine(r));
		break;
	case SpellID::BoneArmor:
		say(_("Absorbs: {:d} damage, for up to {} s"), BoneArmorPool(r), Secs(EffectTicks(spell, r)));
		break;
	case SpellID::PoisonDagger:
		say(_("Every weapon blow you land poisons"));
		poison();
		duration();
		break;
	case SpellID::CorpseExplosion:
		say(_("Damage: {:d}% of the corpse's life ({:d} - {:d}), within {:d} tiles"), EffectPercent(spell, r), CorpseBurstMin, CorpseBurstMax, reach);
		break;
	case SpellID::BoneSplinters:
		say(_("Magic damage: {:d} - {:d}, up to {:d} enemies ahead"), bone.min, bone.max, BoneSplinterTargets);
		break;
	case SpellID::Blight:
		poison();
		say(_("Pool: {} s; standing in it renews the poison every second"), Secs(EffectTicks(spell, r)));
		break;
	case SpellID::BoneWall:
		say(_("Magic damage: {:d} - {:d}, {:d} times a second, to what stands in it; it is thrown back"), bone.min, bone.max, TicksPerSecond / BoneWallPeriod);
		duration();
		break;
	case SpellID::BoneSpikes:
		say(_("Magic damage: {:d} - {:d} within {:d} tile of the cursor"), bone.min, bone.max, reach);
		say(_("Stun: {} s"), Secs(StunTicks(spell, r)));
		break;
	case SpellID::PoisonExplosion:
		poison();
		say(_("Radius: {:d} tiles around the corpse"), reach);
		break;
	case SpellID::BoneSpear:
		say(_("Magic damage: {:d} - {:d}, everything on a {:d}-tile line"), bone.min, bone.max, reach);
		break;
	case SpellID::Decompose:
		poison();
		break;
	case SpellID::BonePrison:
		say(_("Hold: {} s"), Secs(StunTicks(spell, r)));
		say(_("Magic damage: {:d} - {:d} a second"), bone.min, bone.max);
		break;
	case SpellID::BoneStorm:
		say(_("Magic damage: {:d} - {:d}, {:d} times a second, within {:d} tiles"), bone.min, bone.max, TicksPerSecond / BoneStormPeriod, reach);
		duration();
		break;
	case SpellID::NecroBoneSpirit:
		say(_("Damage: a third of the target's remaining life"));
		break;
	case SpellID::PoisonNova:
		poison();
		say(_("Radius: {:d} tiles"), reach);
		break;
	case SpellID::DeathNova:
		say(_("Magic damage: {:d} - {:d} within {:d} tiles"), bone.min, bone.max, reach);
		poison();
		break;
	default:
		// The Bard's (a hidden class), and anything not cast here.
		break;
	}
	return out.text;
}

std::string Rfa12ActivesPassiveFactsAt(const Player &player, ClassTreeSkill skill, int points)
{
	(void)player;
	const int p = std::max(points, 1);
	FactLines out;
	const auto say = [&out](auto format, const auto &...args) { out.add(fmt::format(fmt::runtime(format), args...)); };
	switch (skill) {
	case ClassTreeSkill::Marrow:
		say(_("Bone skill damage: +{:d}%"), MarrowBonusPercent(p));
		break;
	case ClassTreeSkill::Virulence:
		say(_("Poisons last +{:d}% longer"), VirulenceLongerPercent(p));
		say(_("Poison damage: +{:d}%"), VirulenceDeeperPercent(p));
		break;
	case ClassTreeSkill::EarthenMight:
		say(_("Rage: +{:d} for every enemy Ground Stomp, Seismic Slam or Earthquake strikes"), EarthenMightPerEnemy);
		break;
	case ClassTreeSkill::Serration:
		say(_("Bone damage: +{:d}% for every tile it flew, to +{:d}%"), SerrationPerTile, SerrationCap);
		break;
	case ClassTreeSkill::RigorMortis:
		say(_("Bone hits chill for {} s"), Secs(RigorMortisTicks));
		break;
	default:
		break;
	}
	return out.text;
}

bool Rfa12LacksBowFor(const Player &player, SpellID spell)
{
	return IsRfa12Active(spell) && IsBowSkill(spell) && !player.UsesRangedWeapon();
}

} // namespace devilution::oracool
