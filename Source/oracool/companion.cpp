#include "oracool/companion.h"
#include "oracool/warcries.h" // IsMonsterConverted

#include "oracool/minions.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <fmt/format.h>

#include "DiabloUI/ui_flags.hpp"
#include "effects.h"
#include "engine/assets.hpp"
#include "engine/clx_sprite.hpp"
#include "engine/direction.hpp"
#include "engine/displacement.hpp"
#include "engine/load_cl2.hpp"
#include "engine/palette.h"
#include "engine/path.h"
#include "engine/random.hpp"
#include "engine/rectangle.hpp"
#include "engine/render/clx_render.hpp"
#include "engine/render/primitive_render.hpp"
#include "engine/render/text_render.hpp"
#include "items.h"
#include "levels/gendung.h"
#include "missiles.h"
#include "oracool/event_log.h" // IsCornerHudShown
#include "oracool/passives.h" // PassiveDamageDealtPercent - a melee companion's pool
#include "oracool/rfa12_effects.h"
#include "oracool/skill_sounds.h"
#include "oracool/whirlwind.h" // WhirlFrame, DrawWhirlingBlades - Talic's spin looks like the hero's
#include "multi.h"
#include "player.h"
#include "playerdat.hpp"
#include "plrmsg.h"
#include "utils/language.h"

namespace devilution::oracool {

namespace {

constexpr int TicksPerSecond = 20;
constexpr size_t AnimCount = 6;
/** @brief At most this many at once: the golem slots a single-player game leaves free. */
constexpr size_t MaxCompanions = 3;
/** @brief Below this brightness a colour is a sheet's outline and shadow - see RampTranslation. */
constexpr int ShadowLuminance = 40;

// =================================================================================================================
// Hero sheets
// =================================================================================================================

/** @brief A set of hero sheets, owned, bound as monster animations. */
struct HeroSheets {
	bool active = false;
	std::array<std::unique_ptr<OwnedClxSpriteListOrSheet>, AnimCount> owned;
	std::array<AnimStruct, AnimCount> anims {};
	// The magic cast sheet, for a whirling companion's spin (2026-09-29). Its own slot, not Special: Special plays when a
	// body spawns (SpawnCompanionBody), and every companion would arrive casting.
	std::unique_ptr<OwnedClxSpriteListOrSheet> spinOwned;
	AnimStruct spin {};
};

/**
 * @brief Every colour to one palette ramp by its brightness. The ramps run light to dark from their base; @p lightest
 * shifts toward the light end (negative: the dark). With @p keepShadow the near-black - outline, foot shadow - stays
 * itself (user, 2026-09-14: "darker and with black shadow").
 */
} // namespace

std::unique_ptr<uint8_t[]> RampTranslation(uint8_t ramp, int lightest, bool keepShadow)
{
	auto trn = std::make_unique<uint8_t[]>(256);
	for (int i = 0; i < 256; i++) {
		const SDL_Color &c = orig_palette[static_cast<size_t>(i)];
		const int luminance = (c.r * 30 + c.g * 59 + c.b * 11) / 100;
		if (keepShadow && luminance < ShadowLuminance) {
			trn[static_cast<size_t>(i)] = static_cast<uint8_t>(i);
			continue;
		}
		const int shade = std::clamp(15 - lightest - luminance * (16 - lightest) / 255, 0, 15);
		trn[static_cast<size_t>(i)] = static_cast<uint8_t>(ramp + shade);
	}
	return trn;
}

namespace {

bool LoadSheet(HeroSheets &set, HeroClass cls, size_t armour, MonsterGraphic graphic, const char *cel, PlayerWeaponGraphic weapon,
    uint16_t width, int frames)
{
	armour = std::min<size_t>(armour, ArmourChar.size() - 1);
	const char prefix[3] = { CharChar[static_cast<size_t>(cls)], ArmourChar[armour], WepChar[static_cast<size_t>(weapon)] };
	const std::string path = fmt::format(R"(plrgfx\{0}\{1}\{1}{2})", PlayersData[static_cast<size_t>(cls)].classPath,
	    string_view(prefix, 3), cel);
	if (!FindAsset((path + DEVILUTIONX_CL2_EXT).c_str()).ok())
		return false;
	OptionalOwnedClxSpriteSheet sheet = LoadCl2Sheet(path.c_str(), width);
	if (!sheet)
		return false;
	const auto i = static_cast<size_t>(graphic);
	set.owned[i] = std::make_unique<OwnedClxSpriteListOrSheet>(std::move(*sheet));
	AnimStruct &anim = set.anims[i];
	anim.sprites = OptionalClxSpriteListOrSheet { *set.owned[i] };
	anim.width = width;
	anim.frames = static_cast<int8_t>(frames);
	anim.rate = 1;
	return true;
}

int AttackFrames(const PlayerAnimData &data, PlayerWeaponGraphic weapon)
{
	switch (weapon) {
	case PlayerWeaponGraphic::Unarmed: return data.unarmedFrames;
	case PlayerWeaponGraphic::UnarmedShield: return data.unarmedShieldFrames;
	case PlayerWeaponGraphic::Sword: return data.swordFrames;
	case PlayerWeaponGraphic::SwordShield: return data.swordShieldFrames;
	case PlayerWeaponGraphic::Bow: return data.bowFrames;
	case PlayerWeaponGraphic::Axe: return data.axeFrames;
	case PlayerWeaponGraphic::Mace: return data.maceFrames;
	case PlayerWeaponGraphic::MaceShield: return data.maceShieldFrames;
	case PlayerWeaponGraphic::Staff: return data.staffFrames;
	}
	return data.unarmedFrames;
}

int ActionFrame(const PlayerAnimData &data, PlayerWeaponGraphic weapon)
{
	switch (weapon) {
	case PlayerWeaponGraphic::Unarmed: return data.unarmedActionFrame;
	case PlayerWeaponGraphic::UnarmedShield: return data.unarmedShieldActionFrame;
	case PlayerWeaponGraphic::Sword: return data.swordActionFrame;
	case PlayerWeaponGraphic::SwordShield: return data.swordShieldActionFrame;
	case PlayerWeaponGraphic::Bow: return data.bowActionFrame;
	case PlayerWeaponGraphic::Axe: return data.axeActionFrame;
	case PlayerWeaponGraphic::Mace: return data.maceActionFrame;
	case PlayerWeaponGraphic::MaceShield: return data.maceShieldActionFrame;
	case PlayerWeaponGraphic::Staff: return data.staffActionFrame;
	}
	return data.unarmedActionFrame;
}

/** @brief @p cls's sheets - the town pair (stand, walk) or the dungeon five. False, leaving @p set empty, without a stand. */
bool LoadHeroSheets(HeroSheets &set, HeroClass cls, size_t armour, PlayerWeaponGraphic weapon, bool town, bool withSpin = false)
{
	set = HeroSheets {};
	const PlayerAnimData &frames = PlayersAnimData[static_cast<size_t>(cls)];
	const PlayerSpriteData &widths = PlayersSpriteData[static_cast<size_t>(cls)];
	if (!LoadSheet(set, cls, armour, MonsterGraphic::Stand, town ? "st" : "as", weapon, widths.stand, town ? frames.townIdleFrames : frames.idleFrames))
		return false;
	LoadSheet(set, cls, armour, MonsterGraphic::Walk, town ? "wl" : "aw", weapon, widths.walk, town ? frames.townWalkingFrames : frames.walkingFrames);
	if (!town) {
		LoadSheet(set, cls, armour, MonsterGraphic::Attack, "at", weapon,
		    weapon == PlayerWeaponGraphic::Bow ? widths.bow : widths.attack, AttackFrames(frames, weapon));
		LoadSheet(set, cls, armour, MonsterGraphic::GotHit, "ht", weapon, widths.swHit, frames.recoveryFrames);
		// The death sheet is only ever authored unarmed - see LoadPlrGFX.
		LoadSheet(set, cls, armour, MonsterGraphic::Death, "dt", PlayerWeaponGraphic::Unarmed, widths.death, frames.deathFrames);
		// The spin's cast sheet, loaded through Special's slot and moved to its own - a whirler's only (audit, 2026-09-29:
		// every companion decoded it, three of them on each level change for the Ancients).
		const auto special = static_cast<size_t>(MonsterGraphic::Special);
		if (withSpin && LoadSheet(set, cls, armour, MonsterGraphic::Special, "qm", weapon, widths.magic, frames.castingFrames)) {
			set.spinOwned = std::move(set.owned[special]);
			set.spin = set.anims[special];
			set.anims[special] = AnimStruct {};
		}
	}
	for (MonsterGraphic graphic : { MonsterGraphic::Walk, MonsterGraphic::Attack, MonsterGraphic::GotHit, MonsterGraphic::Death, MonsterGraphic::Special }) {
		const auto i = static_cast<size_t>(graphic);
		if (!set.anims[i].sprites)
			set.anims[i] = set.anims[static_cast<size_t>(MonsterGraphic::Stand)];
	}
	set.active = true;
	return true;
}

// =================================================================================================================
// Definitions
// =================================================================================================================

enum class Role : uint8_t {
	Fighter,
	Guard, // holds the attention of what comes near, and taunts on its ability
	Bait,  // holds the attention of everything near, and does nothing else
};

enum class Ability : uint8_t {
	None,
	Volley,      // a spread of arrows
	DoubleThrow, // two hammers at once (Madawc, D2's Double Throw)
	Taunt,       // widens the guard's hold for four seconds
};

/** How far a thrower throws: a bow's reach, less two. */
constexpr int ThrowReach = 6;
/** Ticks between a spin's strikes: four a second, the hero's Whirlwind's rate. */
constexpr int SpinStrikeTicks = 5;
/** The shortest spin: a second, even if what started it falls at once. */
constexpr int SpinMinTicks = TicksPerSecond;
/** Madawc's hammer: the Blessed Hammer's sheet at the size of the Barbarian's thrown axe (26px against 35 on average). */
constexpr int ThrownHammerPercent = 75;

struct CompanionDef {
	const char *name;
	HeroClass sheetClass;
	uint8_t armour; // ArmourChar index; the nearest lighter one is tried if the archive lacks it
	PlayerWeaponGraphic weapon;
	bool casterGear; // wears the caster's own sheets instead
	uint8_t ramp;
	int8_t lightest;
	bool keepShadow;
	CompanionAttack attack;
	Role role;
	Ability ability;
	int8_t abilitySeconds;
	int16_t hpAt1;
	int16_t hpAt20; // the life line runs through both and keeps rising past 20
	int8_t resAt1, resPerRank, resCap;
	int8_t physAt1, physPerRank, physCap;
	int8_t dmgAt1, dmgPerRank;
	int8_t secondsAt1, secondsPerRank;
};

// clang-format off
const std::array<CompanionDef, CompanionKindCount> Defs { {
	// The Valkyrie (user, 2026-09-14): "30 sec on level 1 and up 5 sec every level", "up to 1000hp at lvl 20 and more
	// onwards", "reach res 90% rather soon" - level 11.
	{ N_("Valkyrie"),        HeroClass::Rogue,   2, PlayerWeaponGraphic::Bow,        false, PAL16_YELLOW, -3, true,  CompanionAttack::Bow,   Role::Fighter, Ability::Volley,      8, 150, 1000, 40, 5, 90, 20, 2, 50, 50, 5, 30, 5 },
	// The Ancients, together: shorter-lived and weaker each, three of them.
	// As D2's three at Arreat Summit, and each in his own colour (2026-09-29): Korlic, steel blue, a plain melee fighter
	// ("korlic to move from leap to regular melee attack"); Talic, red, fights by Whirlwind; Madawc, gold, throws the
	// Blessed Hammer's hammer, two at once on his ability - D2's Double Throw.
	{ N_("Korlic"),          HeroClass::Warrior, 2, PlayerWeaponGraphic::Sword,      false, PAL16_BLUE,    0, true,  CompanionAttack::Melee, Role::Fighter, Ability::None,        0, 120,  800, 30, 5, 80, 25, 2, 55, 35, 3, 20, 2 },
	{ N_("Talic"),           HeroClass::Warrior, 2, PlayerWeaponGraphic::Axe,        false, PAL16_RED,     0, true,  CompanionAttack::Whirl, Role::Fighter, Ability::None,        0, 120,  800, 30, 5, 80, 25, 2, 55, 35, 3, 20, 2 },
	{ N_("Madawc"),          HeroClass::Warrior, 2, PlayerWeaponGraphic::MaceShield, false, PAL16_YELLOW,  0, true,  CompanionAttack::Throw, Role::Fighter, Ability::DoubleThrow, 6, 120,  800, 30, 5, 80, 25, 2, 55, 35, 3, 20, 2 },
	{ N_("Spirit Guardian"), HeroClass::Monk,    2, PlayerWeaponGraphic::Staff,      false, PAL16_BLUE,    1, true,  CompanionAttack::Melee, Role::Guard,   Ability::Taunt,       8, 200, 1200, 40, 5, 85, 30, 2, 60, 30, 3, 30, 5 },
	{ N_("Decoy"),           HeroClass::Rogue,   0, PlayerWeaponGraphic::Unarmed,    true,  PAL16_BLUE,    2, false, CompanionAttack::None,  Role::Bait,    Ability::None,        0, 100,  900, 30, 4, 75, 20, 2, 50,  0, 0, 15, 1 },
} };
// clang-format on

const CompanionDef &DefOf(CompanionKind kind)
{
	return Defs[static_cast<size_t>(kind)];
}

// ---- the ability numbers, one place each: TryCompanionAbility and CompanionTauntTarget read them, and so does
// ---- CompanionFactsAt, so the rule and its tooltip cannot disagree (2026-09-26).

/** @brief Every ability hits a quarter harder from this level. */
constexpr int AbilityPowerLevel = 10;
/** @brief Every ability comes round a quarter sooner from this level. */
constexpr int AbilityHasteLevel = 20;
/** @brief How far a decoy draws, how far a guard holds, and how far its taunt holds while it lasts. */
constexpr int BaitHoldRadius = 6;
constexpr int GuardHoldRadius = 3;
constexpr int GuardTauntRadius = 8;
constexpr int TauntTicks = 4 * TicksPerSecond;

/** @brief An ability's power at @p rank, in percent of the owner's blow, before the ability's own share. */
int AbilityPower(const CompanionStats &stats, int rank)
{
	return stats.damagePercent * (rank >= AbilityPowerLevel ? 125 : 100) / 100;
}


/** @brief The Valkyrie's volley: three arrows, five from the power level. */
int VolleyArrows(int rank)
{
	return rank >= AbilityPowerLevel ? 5 : 3;
}

/** @brief How long @p def's ability takes to come round at @p rank, in ticks. */
int AbilityCooldownTicks(const CompanionDef &def, int rank)
{
	int ticks = def.abilitySeconds * TicksPerSecond;
	if (rank >= AbilityHasteLevel)
		ticks = ticks * 3 / 4;
	return ticks;
}

struct KindList {
	std::array<CompanionKind, 3> kinds;
	uint8_t count;
};

KindList KindsFor(SpellID spell)
{
	switch (spell) {
	case SpellID::Valkyrie: return { { CompanionKind::Valkyrie }, 1 };
	case SpellID::AncestralCall: return { { CompanionKind::Korlic, CompanionKind::Talic, CompanionKind::Madawc }, 3 };
	case SpellID::SpiritGuardian: return { { CompanionKind::SpiritGuardian }, 1 };
	case SpellID::Decoy: return { { CompanionKind::Decoy }, 1 };
	default: return { {}, 0 };
	}
}

const char *StanceName(CompanionStance stance)
{
	switch (stance) {
	case CompanionStance::Follow: return N_("Follow");
	case CompanionStance::Hold: return N_("Hold position");
	case CompanionStance::Aggressive: return N_("Aggressive");
	case CompanionStance::Passive: return N_("Passive");
	}
	return N_("Follow");
}

// =================================================================================================================
// Instances
// =================================================================================================================

struct TownState {
	HeroSheets sheets;
	std::unique_ptr<uint8_t[]> trn;
	Point tile {};
	Point next {};
	Direction dir = Direction::South;
	int stepTick = 0;  // 0 standing, else ticks into a step toward `next`
	int stepTicks = 0; // how long this step takes
	int frame = 0;
	int frameTick = 0;
	int placeWait = 0;
};

struct Instance {
	bool active = false;
	CompanionKind kind = CompanionKind::Valkyrie;
	uint8_t owner = 0;
	int rank = 1;
	int ticksLeft = 0;
	int hitPoints = -1; // 1/64ths, carried between levels; -1 is full
	int slot = -1;      // the golem slot its body stands in, or -1
	int cooldown = 0;
	int returnWait = 0;
	int order = 0; // its place in formation
	int tauntTicks = 0;
	int target = -1; // the monster it is attacking
	bool volley = false;
	int spinTicks = 0; // a whirling companion's spin: ticks left, topped up while anything is beside it
	int spinClock = 0; // ticks into the spin - its strikes and its turning
	TownState town;
};

std::array<Instance, MaxCompanions> Instances;
std::array<HeroSheets, MAX_PLRS> SlotSheets;
std::array<int8_t, MAX_PLRS> SlotInstance = [] {
	std::array<int8_t, MAX_PLRS> slots;
	slots.fill(-1);
	return slots;
}();
CompanionStance Stance = CompanionStance::Follow;
int FocusMonster = -1;
int FocusTicks = 0;
/** Whether the focus is an ORDER (Command the Dead) rather than the automatic turn on what struck the owner - the Commanded marker. */
bool FocusCommanded = false;

Instance *InstanceInSlot(const Monster &monster)
{
	if (&monster < &Monsters[0] || &monster >= &Monsters[0] + MAX_PLRS)
		return nullptr;
	const int i = SlotInstance[monster.getId()];
	return i < 0 ? nullptr : &Instances[static_cast<size_t>(i)];
}

Player *OwnerOf(const Instance &inst)
{
	return inst.owner < Players.size() ? &Players[inst.owner] : nullptr;
}

bool SlotFree(size_t slot)
{
	return SlotInstance[slot] < 0 && Monsters[slot].position.tile == GolemHoldingCell;
}

int FreeSlot(const Instance &inst)
{
	// In a multiplayer game the other slots are other players' - a companion may only borrow its owner's.
	if (gbIsMultiplayer)
		return inst.owner < MAX_PLRS && SlotFree(inst.owner) ? inst.owner : -1;
	for (size_t slot = 1; slot < MAX_PLRS; slot++) {
		if (SlotFree(slot))
			return static_cast<int>(slot);
	}
	return -1;
}

bool Targetable(const Monster &monster)
{
	// A convert fights beside it (round 36). This comment sat mid-line in v1.12.262 and swallowed the hit and hidden tests
	// (round 37 audit: a Valkyrie could strike Lachdanan).
	return !monster.isPlayerMinion() && !IsMonsterConverted(monster) && (monster.hitPoints >> 6) > 0
	    && monster.isPossibleToHit() && (monster.flags & MFLAG_HIDDEN) == 0
	    && monster.position.tile != GolemHoldingCell && monster.mode != MonsterMode::Death;
}

std::vector<Monster *> TargetsWithin(Point centre, int radius)
{
	std::vector<Monster *> found;
	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		Monster &monster = Monsters[ActiveMonsters[i]];
		if (Targetable(monster) && centre.WalkingDistance(monster.position.tile) <= radius)
			found.push_back(&monster);
	}
	return found;
}

/** @brief A blow of @p owner's own, at @p percent: the weapon roll with the hero's bonus damage, in 1/64ths. */
int OwnerBlow(const Player &owner, int percent, const Monster *target = nullptr)
{
	int damage = RandomIntBetween(owner._pIMinDam, std::max(owner._pIMinDam, owner._pIMaxDam));
	// The damage passives in the pool, as a companion's arrow takes them - a burst, spending nothing of the hero's (round 88
	// audit: Korlic, Talic and the Guardian hit for less than the Valkyrie's arrows at the same share).
	SetCompanionBlowInFlight(true); // the pool's Dead Ground reads it (round 92 audit)
	// Melee (round 94 audit): every OwnerBlow is a staff or a blade's, and as a spell it took Mythic Rhythm and the spell
	// passives. A burst still, so Counterstroke and Momentum are not spent.
	const int pool = target != nullptr ? PassiveDamageDealtPercent(owner, *target, true, /*burst=*/true) + Rfa12DamageDealtPercent(owner, *target, true) : 0;
	SetCompanionBlowInFlight(false);
	damage = PooledWeaponDamage(owner, damage, pool); // its share of the hero's pooled blow
	if (target != nullptr && target->data().monsterClass == MonsterClass::Demon && HasAnyOf(owner._pIFlags, ItemSpecialEffect::TripleDemonDamage))
		damage *= 3;
	damage = damage * percent / 100;
	return std::max(damage, 1) << 6;
}

/** @brief @p owner strikes @p monster through a companion: the kill and the experience are the owner's. */
void StrikeFor(const Player &owner, Monster &monster, int damage)
{
	if (!Targetable(monster))
		return;
	ApplyMonsterDamage(DamageType::Physical, monster, damage);
	SetCompanionBlowInFlight(true); // the owner's credit, not the owner's focus (NoteOwnerStruck)
	if ((monster.hitPoints >> 6) <= 0)
		M_StartKill(monster, owner);
	else
		M_StartHit(monster, owner, damage);
	SetCompanionBlowInFlight(false);
}

void Ring(const Instance &inst, Point tile)
{
	AddMissile(tile, tile, Direction::South, MissileID::WarcryRing, TARGET_MONSTERS, inst.owner, 0, 0);
}

/** @brief The tree row whose sounds a companion kind uses, or None. */
ClassTreeSkill SoundSkillOf(CompanionKind kind)
{
	switch (kind) {
	case CompanionKind::Valkyrie:
		return ClassTreeSkill::Valkyrie;
	case CompanionKind::Decoy:
		return ClassTreeSkill::Decoy;
	// RfA-27 batch 51's arrive cues: the Ancients' horn and footfalls (one cue for the three - the mixer drops the repeats
	// of one sound inside 80ms), and the Spirit Guardian's tone.
	case CompanionKind::Korlic:
	case CompanionKind::Talic:
	case CompanionKind::Madawc:
		return ClassTreeSkill::AncestralCall;
	case CompanionKind::SpiritGuardian:
		return ClassTreeSkill::SpiritGuardian;
	default:
		return ClassTreeSkill::None;
	}
}

/**
 * @brief The arrival and departure: a pillar of light and its sound.
 *
 * An ARRIVAL plays the companion's own delivered arrive cue in place of the resurrection chime when it has one
 * (2026-09-26 asset audit: decoy-arrive.wav and valkyrie-arrive.wav were packed and never played) - one sound for
 * the moment, never both.
 */
void Flourish(const Instance &inst, Point tile, bool arriving = false)
{
	AddMissile(tile, tile, Direction::South, MissileID::ResurrectBeam, TARGET_MONSTERS, inst.owner, 0, 0);
	const ClassTreeSkill row = SoundSkillOf(inst.kind);
	if (arriving && row != ClassTreeSkill::None && inst.owner == MyPlayerId && PlaySkillSound(row, SkillSoundEvent::Arrive))
		return;
	PlaySfxLoc(LS_RESUR, tile);
}

void ApplyStats(Instance &inst, Monster &body, bool full)
{
	const CompanionStats stats = CompanionStatsAt(inst.kind, inst.rank);
	body.maxHitPoints = stats.hitPoints << 6;
	body.hitPoints = full || inst.hitPoints <= 0 ? body.maxHitPoints : std::min(inst.hitPoints, body.maxHitPoints);
	body.golemToHit = 100;
	body.minDamage = 0; // its blows are the owner's, dealt by CompanionMeleeHit
	body.maxDamage = 0;
	body.armorClass = 0; // its physical resistance does the armour's work
	inst.hitPoints = body.hitPoints;
}

bool LoadDress(HeroSheets &sheets, const CompanionDef &def, const Player *owner, bool town)
{
	if (def.casterGear && owner != nullptr) {
		const auto weapon = static_cast<PlayerWeaponGraphic>(std::min<int>(owner->_pgfxnum & 0xF, static_cast<int>(PlayerWeaponGraphic::Staff)));
		return LoadHeroSheets(sheets, owner->_pClass, static_cast<size_t>(owner->_pgfxnum >> 4), weapon, town);
	}
	for (size_t armour = static_cast<size_t>(def.armour) + 1; armour-- > 0;) {
		if (LoadHeroSheets(sheets, def.sheetClass, armour, def.weapon, town, def.attack == CompanionAttack::Whirl))
			return true;
	}
	return false;
}

void ClearSlotDress(size_t slot)
{
	SlotSheets[slot] = HeroSheets {};
	SlotInstance[slot] = -1;
	Monsters[slot].uniqueMonsterTRN = nullptr;
}

bool SpawnInDungeon(Instance &inst, Point near, bool full)
{
	const Player *owner = OwnerOf(inst);
	if (owner == nullptr || !LevelHasGolemSlots())
		return false;
	const int slot = FreeSlot(inst);
	if (slot < 0)
		return false;
	Monster &body = Monsters[slot];
	std::optional<Point> spot;
	for (int radius = 0; radius <= 5 && !spot; radius++) {
		for (int dy = -radius; dy <= radius && !spot; dy++) {
			for (int dx = -radius; dx <= radius; dx++) {
				if (std::max(std::abs(dx), std::abs(dy)) != radius)
					continue;
				const Point tile = near + Displacement { dx, dy };
				// On this side of a wall (round 82 audit: back from the stairs, a Decoy or a Hold guard stood in the next room for
				// its whole life), as SpawnBody and PlaceCompanionNear choose.
				if (InDungeonBounds(tile) && IsTileAvailable(body, tile) && LineClearMissile(near, tile)) {
					spot = tile;
					break;
				}
			}
		}
	}
	if (!spot)
		return false;

	const CompanionDef &def = DefOf(inst.kind);
	// Dressed BEFORE the body is stood up, so its first animation is already the hero sheet's.
	SlotInstance[static_cast<size_t>(slot)] = static_cast<int8_t>(&inst - Instances.data());
	inst.slot = slot;
	LoadDress(SlotSheets[static_cast<size_t>(slot)], def, owner, /*town=*/false);
	body.uniqueMonsterTRN = RampTranslation(def.ramp, def.lightest, def.keepShadow);
	SpawnCompanionBody(body, *spot, *spot == owner->position.tile ? Direction::South : GetDirection(*spot, owner->position.tile));
	ApplyStats(inst, body, full);
	Flourish(inst, *spot, /*arriving=*/true);
	return true;
}

void LetGo(Instance &inst, bool flourish)
{
	if (inst.slot >= 0) {
		Monster &body = Monsters[inst.slot];
		if (flourish && body.position.tile != GolemHoldingCell)
			Flourish(inst, body.position.tile);
		ReleaseCompanionBody(body);
		ClearSlotDress(static_cast<size_t>(inst.slot));
	}
	if (flourish && inst.town.sheets.active && leveltype == DTYPE_TOWN)
		Flourish(inst, inst.town.tile);
	inst = Instance {};
}

void ReassignOrders()
{
	int order = 0;
	for (Instance &inst : Instances) {
		if (inst.active)
			inst.order = order++;
	}
}

/** @brief Its place in formation around @p owner: behind and to the right, behind and to the left, straight behind. */
Point FormationHome(const Player &owner, int order)
{
	const Direction behind = Opposite(owner._pdir);
	Point home = owner.position.tile;
	switch (order) {
	case 0: home += Right(behind); break;
	case 1: home += Left(behind); break;
	default:
		home += behind;
		home += behind;
		break;
	}
	return InDungeonBounds(home) && IsTileWalkable(home) ? home : Point(owner.position.tile);
}

// ---- town ---------------------------------------------------------------------------------------------------------

constexpr int TownStepTicks = 8;
constexpr int TownHurryTicks = 4;
constexpr int TownHurryDistance = 4;
constexpr int TownFollowDistance = 2;
constexpr int TownCatchUpDistance = 14;
constexpr int StandTicksPerFrame = 3;

/**
 * @brief One tile's walk in screen pixels per direction - the renderer's own MovingOffset (GetOffsetForWalking). NOT
 * Displacement::worldToScreen(), whose vertical axis is the camera's (v1.12.014).
 */
constexpr Displacement WalkStep[8] = { { 0, 32 }, { -32, 16 }, { -64, 0 }, { -32, -16 }, { 0, -32 }, { 32, -16 }, { 64, 0 }, { 32, 16 } };

bool TownTileFree(Point tile)
{
	if (!InDungeonBounds(tile) || !IsTileWalkable(tile) || IsTileOccupied(tile))
		return false;
	for (const Instance &other : Instances) {
		const TownState &t = other.town;
		if (other.active && t.sheets.active && (t.tile == tile || (t.stepTick > 0 && t.next == tile)))
			return false;
	}
	return true;
}

std::optional<Point> FreeTownTileNear(Point centre, int maxRadius)
{
	for (int radius = 0; radius <= maxRadius; radius++) {
		for (int dy = -radius; dy <= radius; dy++) {
			for (int dx = -radius; dx <= radius; dx++) {
				if (std::max(std::abs(dx), std::abs(dy)) != radius)
					continue;
				const Point tile = centre + Displacement { dx, dy };
				if (TownTileFree(tile))
					return tile;
			}
		}
	}
	return std::nullopt;
}

Direction Turned(Direction dir, int eighths)
{
	return static_cast<Direction>((static_cast<int>(dir) + eighths + 8) % 8);
}

const AnimStruct &TownAnim(const TownState &t)
{
	return t.sheets.anims[static_cast<size_t>(t.stepTick > 0 ? MonsterGraphic::Walk : MonsterGraphic::Stand)];
}

bool PlaceInTown(Instance &inst, Point target)
{
	const Player *owner = OwnerOf(inst);
	if (owner == nullptr || leveltype != DTYPE_TOWN)
		return false;
	const std::optional<Point> spot = FreeTownTileNear(target, 5);
	if (!spot)
		return false;
	TownState &t = inst.town;
	if (!t.sheets.active) {
		const CompanionDef &def = DefOf(inst.kind);
		if (!LoadDress(t.sheets, def, owner, /*town=*/true)) {
			t = TownState {};
			return false;
		}
		t.trn = RampTranslation(def.ramp, def.lightest, def.keepShadow);
	}
	t.tile = *spot;
	t.stepTick = 0;
	t.frame = 0;
	t.dir = *spot == owner->position.tile ? Direction::South : GetDirection(*spot, owner->position.tile);
	Flourish(inst, *spot, /*arriving=*/true);
	return true;
}

} // namespace

// =================================================================================================================
// Definitions, answered
// =================================================================================================================

CompanionStats CompanionStatsAt(CompanionKind kind, int rank)
{
	const CompanionDef &def = DefOf(kind);
	const int steps = std::max(rank, 1) - 1;
	return {
		def.hpAt1 + (def.hpAt20 - def.hpAt1) * steps / 19,
		std::min(def.resAt1 + def.resPerRank * steps, static_cast<int>(def.resCap)),
		std::min(def.physAt1 + def.physPerRank * steps, static_cast<int>(def.physCap)),
		def.dmgAt1 + def.dmgPerRank * steps,
		def.secondsAt1 + def.secondsPerRank * steps,
	};
}

std::string CompanionFactsAt(SpellID spell, int rank)
{
	const KindList list = KindsFor(spell);
	if (list.count == 0)
		return {};
	const int r = std::max(rank, 1);
	std::string out;
	const auto say = [&out](auto format, const auto &...args) {
		if (!out.empty())
			out += '\n';
		out += fmt::format(fmt::runtime(format), args...);
	};
	const auto secs = [](int ticks) {
		if (ticks % TicksPerSecond == 0)
			return fmt::format("{:d}", ticks / TicksPerSecond);
		return fmt::format("{:.1f}", static_cast<double>(ticks) / TicksPerSecond);
	};

	// The Ancients share one set of numbers; the first speaks for all three.
	const CompanionKind first = list.kinds[0];
	const CompanionStats stats = CompanionStatsAt(first, r);
	if (list.count > 1)
		say(_("Life: {:d} each"), stats.hitPoints);
	else
		say(_("Life: {:d}"), stats.hitPoints);
	say(_("Fire, lightning, magic, cold and acid resistance: {:d}%"), stats.elementalResist);
	say(_("Physical damage reduced: {:d}%"), stats.physicalResist);
	if (DefOf(first).attack != CompanionAttack::None)
		say(_("Damage: {:d}% of yours"), stats.damagePercent);

	for (uint8_t i = 0; i < list.count; i++) {
		const CompanionKind kind = list.kinds[i];
		const CompanionDef &def = DefOf(kind);
		const int percent = AbilityPower(CompanionStatsAt(kind, r), r); // Double Throw's hammers (ThrowHammers)
		const std::string every = secs(AbilityCooldownTicks(def, r));
		switch (def.ability) {
		case Ability::Volley:
			say(_("Volley: {:d} arrows every {} s"), VolleyArrows(r), every);
			break;
		case Ability::DoubleThrow:
			say(_("{} throws two hammers at once every {} s, {:d}% of yours each"), _(def.name), every, percent);
			break;
		case Ability::Taunt:
			say(_("Holds the enemies within {:d} tiles; every {} s a taunt holds all within {:d} for {} s"), GuardHoldRadius, every,
			    GuardTauntRadius, secs(TauntTicks));
			break;
		case Ability::None:
			if (def.role == Role::Bait)
				say(_("Draws every enemy within {:d} tiles"), BaitHoldRadius);
			break;
		}
		// How each fights, when it is not a plain swing (2026-09-29).
		if (def.attack == CompanionAttack::Whirl)
			say(_("{} fights by Whirlwind: everything beside him, four times a second"), _(def.name));
		else if (def.attack == CompanionAttack::Throw)
			say(_("{} throws his hammer from up to {:d} tiles"), _(def.name), ThrowReach);
	}
	say(_("Duration: {:d} s"), stats.seconds);
	return out;
}

const char *CompanionName(CompanionKind kind)
{
	return DefOf(kind).name;
}

CompanionAttack CompanionAttackOf(CompanionKind kind)
{
	return DefOf(kind).attack;
}

bool IsCompanionSpell(SpellID spell)
{
	return KindsFor(spell).count > 0;
}

bool SummonCompanions(Player &owner, SpellID spell, Point target, int rank)
{
	const KindList list = KindsFor(spell);
	if (list.count == 0)
		return false;
	// A cursor the hero cannot see is no anchor (round 83 audit: whatever path skipped CastOnce's clamp set the Decoy in the
	// next room): beside him instead.
	if (!InDungeonBounds(target) || IsTileSolid(target) || !LineClearMissile(owner.position.tile, target))
		target = owner.position.tile;
	const bool town = leveltype == DTYPE_TOWN;
	if (!town && !LevelHasGolemSlots()) {
		owner.Say(HeroSpeech::ICantCastThatHere); // a quest's set level has no golem slot to stand in
		return false;
	}
	const auto calledByThisCast = [&list](CompanionKind kind) {
		return std::find(list.kinds.begin(), list.kinds.begin() + list.count, kind) != list.kinds.begin() + list.count;
	};

	for (uint8_t i = 0; i < list.count; i++) {
		const CompanionKind kind = list.kinds[i];
		Instance *inst = nullptr;
		for (Instance &candidate : Instances) {
			if (candidate.active && candidate.kind == kind && candidate.owner == owner.getId())
				inst = &candidate;
		}
		if (inst == nullptr) {
			for (Instance &candidate : Instances) {
				if (!candidate.active) {
					inst = &candidate;
					break;
				}
			}
		}
		if (inst == nullptr) {
			// Full: the companion with the least time left makes room - never one this cast is calling.
			for (Instance &candidate : Instances) {
				if (!calledByThisCast(candidate.kind) && (inst == nullptr || candidate.ticksLeft < inst->ticksLeft))
					inst = &candidate;
			}
			if (inst == nullptr)
				continue;
			LetGo(*inst, /*flourish=*/true);
		}
		if (!inst->active) {
			inst->active = true;
			inst->kind = kind;
			inst->owner = static_cast<uint8_t>(owner.getId());
		}
		inst->rank = std::max(rank, 1);
		inst->ticksLeft = CompanionStatsAt(kind, inst->rank).seconds * TicksPerSecond;
		inst->hitPoints = -1;
		inst->cooldown = DefOf(kind).abilitySeconds * TicksPerSecond / 2; // the first ability comes sooner
		if (town) {
			PlaceInTown(*inst, target);
		} else if (inst->slot >= 0 && (Monsters[inst->slot].mode == MonsterMode::Death || Monsters[inst->slot].hitPoints <= 0)) {
			// A recast during the body's death sheet (audit, 2026-09-19): the refresh below filled a
			// bar on a body ProcessCompanions was about to clear, and the cast was lost. Let the dying
			// body go and stand a new one.
			ReleaseCompanionBody(Monsters[inst->slot]);
			ClearSlotDress(static_cast<size_t>(inst->slot));
			inst->slot = -1;
			inst->spinTicks = 0;
			inst->volley = false;
			SpawnInDungeon(*inst, target, /*full=*/true);
		} else if (inst->slot >= 0) {
			Monster &body = Monsters[inst->slot];
			ApplyStats(*inst, body, /*full=*/true);
			Flourish(*inst, body.position.tile, /*arriving=*/true);
		} else {
			SpawnInDungeon(*inst, target, /*full=*/true); // with no room yet, ProcessCompanions keeps trying
		}
	}
	ReassignOrders();
	return true;
}

bool HasCompanion(CompanionKind kind)
{
	return std::any_of(Instances.begin(), Instances.end(), [kind](const Instance &inst) { return inst.active && inst.kind == kind; });
}

void DismissCompanions(const Player &owner)
{
	// They go with their master, as the army does (round 82 audit: the Ancients fought on through the death animation,
	// stood beside him in town after a Respawn and re-formed on the next floor).
	bool any = false;
	for (Instance &inst : Instances) {
		if (!inst.active || inst.owner != owner.getId())
			continue;
		LetGo(inst, /*flourish=*/true);
		any = true;
	}
	if (any)
		ReassignOrders();
}

void ForgetCompanions()
{
	ForgetMinions(); // the same moment: a new game has no army either

	for (Instance &inst : Instances)
		inst = Instance {};
	for (HeroSheets &sheets : SlotSheets)
		sheets = HeroSheets {};
	SlotInstance.fill(-1);
	Stance = CompanionStance::Follow;
	FocusMonster = -1;
	FocusTicks = 0;
	FocusCommanded = false;
}

// =================================================================================================================
// Engine hooks
// =================================================================================================================

bool IsCompanion(const Monster &monster)
{
	return InstanceInSlot(monster) != nullptr;
}

const Player *CompanionOwner(const Monster &monster)
{
	const Instance *inst = InstanceInSlot(monster);
	return inst != nullptr ? OwnerOf(*inst) : nullptr;
}

void StartCompanionSpin(Monster &companion)
{
	Instance *inst = InstanceInSlot(companion);
	if (inst == nullptr)
		return;
	if (inst->spinTicks <= 0)
		inst->spinClock = 0;
	inst->spinTicks = std::max(inst->spinTicks, SpinMinTicks);
}

void StopCompanionSpin(Monster &companion)
{
	if (Instance *inst = InstanceInSlot(companion); inst != nullptr)
		inst->spinTicks = 0;
}

bool IsCompanionSpinning(const Monster &companion)
{
	const Instance *inst = InstanceInSlot(companion);
	return inst != nullptr && inst->spinTicks > 0 && companion.mode != MonsterMode::Death;
}

std::optional<ClxSprite> CompanionSpinSprite(const Monster &companion)
{
	if (!IsCompanionSpinning(companion))
		return std::nullopt;
	const Instance *inst = InstanceInSlot(companion);
	const HeroSheets &sheets = SlotSheets[companion.getId()];
	if (!sheets.active || !sheets.spin.sprites)
		return std::nullopt;
	// Turning a facing a tick, as the hero's spin does, through the cast sheet's full-cloud frames.
	const OptionalClxSpriteList frames = sheets.spin.spritesForDirection(static_cast<Direction>(inst->spinClock % 8));
	if (!frames || frames->numSprites() == 0)
		return std::nullopt;
	return WhirlFrame(*frames, inst->spinClock);
}

void DrawCompanionBlades(const Surface &out, const Monster &companion, Point foot, bool front)
{
	if (CompanionSpinSprite(companion))
		DrawWhirlingBlades(out, foot, front);
}

const AnimStruct *GetCompanionAnim(const Monster &monster, MonsterGraphic graphic)
{
	if (InstanceInSlot(monster) == nullptr)
		return nullptr;
	const HeroSheets &sheets = SlotSheets[monster.getId()];
	if (!sheets.active)
		return nullptr;
	const AnimStruct &anim = sheets.anims[static_cast<size_t>(graphic)];
	return anim.sprites ? &anim : nullptr;
}

int CompanionDamageTaken(const Monster &monster, DamageType type, int damage)
{
	const Instance *inst = InstanceInSlot(monster);
	if (inst == nullptr)
		return damage;
	const CompanionStats stats = CompanionStatsAt(inst->kind, inst->rank);
	const int resist = type == DamageType::Physical ? stats.physicalResist : stats.elementalResist;
	return damage * (100 - resist) / 100;
}

void ForgetCompanionInSlot(Monster &slot)
{
	Instance *inst = InstanceInSlot(slot);
	if (inst == nullptr)
		return;
	ClearSlotDress(slot.getId());
	inst->slot = -1; // it waits for room elsewhere
	inst->spinTicks = 0;
	inst->volley = false;
}

void OnCompanionLevelLoad()
{
	for (Instance &inst : Instances) {
		// A companion that fell just before the level changed stays fallen: its death sheet never finished, and the slot
		// cleared below let the next level stand it up again (round 17 audit, v1.12.242). Monsters[] still holds the old
		// level here.
		if (inst.slot >= 0 && static_cast<size_t>(inst.slot) < MaxMonsters) {
			const Monster &body = Monsters[inst.slot];
			if (body.mode == MonsterMode::Death || (body.hitPoints >> 6) <= 0) {
				inst = Instance {};
				continue;
			}
		}
		inst.slot = -1;
		inst.returnWait = TicksPerSecond - 5; // back a quarter-second after the level is up
		inst.town = TownState {};
		inst.target = -1;
		// Not carried down the stairs (audit, 2026-09-29): a spin or a Double Throw from the level left.
		inst.spinTicks = 0;
		inst.volley = false;
	}
	for (HeroSheets &sheets : SlotSheets)
		sheets = HeroSheets {};
	SlotInstance.fill(-1);
	FocusMonster = -1;
	FocusTicks = 0;
	FocusCommanded = false;
}

namespace {

/**
 * @brief One tick of a whirling companion's spin: everything beside it struck every SpinStrikeTicks, at its share of
 * the owner's blow, and the spin kept going while anything is there. It ends a little after the last enemy beside it.
 */
void ProcessSpin(Instance &inst, const Monster &body, const Player &owner)
{
	if (inst.spinTicks <= 0)
		return;
	if (body.mode == MonsterMode::Death) {
		inst.spinTicks = 0;
		return;
	}
	inst.spinClock++;
	const std::vector<Monster *> around = TargetsWithin(body.position.tile, 1);
	if (inst.spinClock % SpinStrikeTicks == 0 && !around.empty()) {
		const int percent = CompanionStatsAt(inst.kind, inst.rank).damagePercent;
		for (Monster *monster : around)
			StrikeFor(owner, *monster, OwnerBlow(owner, percent, monster));
		PlaySkillSound(ClassTreeSkill::Whirlwind, SkillSoundEvent::Impact); // the hero's Whirlwind's strike, once a strike
	}
	if (!around.empty())
		inst.spinTicks = std::max(inst.spinTicks, SpinStrikeTicks + 1);
	inst.spinTicks--;
}

} // namespace

void ProcessCompanions(Player &owner)
{
	ProcessMinions(owner);

	if (&owner != MyPlayer)
		return;
	if (FocusTicks > 0 && --FocusTicks == 0) {
		FocusMonster = -1;
		FocusCommanded = false;
	}
	const bool town = leveltype == DTYPE_TOWN;

	// A revisited level restores whatever stood in the golem slots when it was left. A companion slot with no
	// companion behind it is that stale body - it goes before it can wander about as a plain Golem.
	if (!town && LevelHasGolemSlots() && !gbIsMultiplayer) {
		for (size_t slot = 1; slot < MAX_PLRS; slot++) {
			Monster &body = Monsters[slot];
			if (SlotInstance[slot] < 0 && body.position.tile != GolemHoldingCell) {
				ReleaseCompanionBody(body);
				body.uniqueMonsterTRN = nullptr;
			}
		}
	}

	for (Instance &inst : Instances) {
		if (!inst.active || inst.owner != owner.getId())
			continue;
		if (--inst.ticksLeft <= 0) {
			LetGo(inst, /*flourish=*/true);
			ReassignOrders();
			continue;
		}
		if (inst.cooldown > 0)
			inst.cooldown--;
		if (inst.tauntTicks > 0)
			inst.tauntTicks--;
		if (town)
			continue;

		if (inst.slot >= 0) {
			Monster &body = Monsters[inst.slot];
			// Falling, while the body is still on the map. At the end of the death animation DeleteMonsterList moves it to the
			// holding cell but leaves it in Death mode, so this test alone kept a dead companion forever: on the HUD, holding
			// its slot, and standing again at the next level (round 6 audit, v1.12.231).
			if (body.mode == MonsterMode::Death && body.position.tile != GolemHoldingCell) {
				inst.spinTicks = 0; // it falls on its death sheet, not spinning (audit, 2026-09-29)
				continue; // falling; the slot empties when the body is gone
			}
			if (body.position.tile == GolemHoldingCell || (body.hitPoints >> 6) <= 0) {
				// Fallen, for good: a companion does not come back from death, only from a level change.
				ReleaseCompanionBody(body); // out of Death mode, into the holding cell
				ClearSlotDress(static_cast<size_t>(inst.slot));
				inst = Instance {};
				ReassignOrders();
				continue;
			}
			inst.hitPoints = body.hitPoints;
			ProcessSpin(inst, body, owner);
		} else if (LevelHasGolemSlots() && ++inst.returnWait >= TicksPerSecond) {
			inst.returnWait = 0;
			SpawnInDungeon(inst, owner.position.tile, /*full=*/false);
		}
	}
}

bool CompanionBlowInFlight = false;

bool IsCompanionBlowInFlight()
{
	return CompanionBlowInFlight;
}

void SetCompanionBlowInFlight(bool inFlight)
{
	CompanionBlowInFlight = inFlight;
}

void NoteOwnerStruck(const Player &player, const Monster &monster)
{
	if (&player != MyPlayer || monster.isPlayerMinion())
		return;
	// A companion's own blow or arrow is routed as the owner's for the credit; it is not what the owner is striking, and
	// one Talic spin tick pulled the rest off the hero's target (round 17 audit, v1.12.242).
	if (CompanionBlowInFlight)
		return;
	// Not over a standing command: any blow of the hero's - a Teeth, a poison tick - moved the army off the commanded
	// monster and took its sigil (round 15 audit, v1.12.240).
	if (FocusCommanded && FocusTicks > 0)
		return;
	FocusMonster = static_cast<int>(monster.getId());
	FocusTicks = 3 * TicksPerSecond;
	FocusCommanded = false;
}

int CompanionTauntTarget(const Monster &monster)
{
	// Nor a convert: it is on the companion's side (round 37 audit).
	if (monster.isPlayerMinion() || IsMonsterConverted(monster) || (monster.hitPoints >> 6) <= 0)
		return -1;
	int best = -1;
	int bestDistance = 0;
	for (const Instance &inst : Instances) {
		if (!inst.active || inst.slot < 0)
			continue;
		const CompanionDef &def = DefOf(inst.kind);
		if (def.role == Role::Fighter)
			continue;
		const Monster &body = Monsters[inst.slot];
		if (body.position.tile == GolemHoldingCell || (body.hitPoints >> 6) <= 0 || body.mode == MonsterMode::Death)
			continue;
		const int radius = def.role == Role::Bait ? BaitHoldRadius : (inst.tauntTicks > 0 ? GuardTauntRadius : GuardHoldRadius);
		const int distance = monster.position.tile.WalkingDistance(body.position.tile);
		if (distance <= radius && (best < 0 || distance < bestDistance)
		    && LineClearMissile(monster.position.tile, body.position.tile)) { // not through a wall (round 82 audit)
			best = inst.slot;
			bestDistance = distance;
		}
	}
	return best;
}

bool CompanionMakesWay(const Player &player, const Monster &monster)
{
	if (MinionMakesWay(player, monster))
		return true;
	const Instance *inst = InstanceInSlot(monster);
	return inst != nullptr && inst->owner == player.getId() && !monster.isWalking() && monster.mode != MonsterMode::Death
	    && (monster.hitPoints >> 6) > 0;
}

void CompanionsMakeWay(Player &player, Point tile)
{
	if (leveltype == DTYPE_TOWN || !InDungeonBounds(tile))
		return;
	const int id = dMonster[tile.x][tile.y];
	if (id <= 0)
		return;
	Monster &monster = Monsters[id - 1];
	if (CompanionMakesWay(player, monster))
		MoveCompanionTo(monster, player.position.tile);
}

// =================================================================================================================
// The brain
// =================================================================================================================

CompanionOrders GetCompanionOrders(const Monster &companion)
{
	CompanionOrders orders {};
	const Instance *inst = InstanceInSlot(companion);
	if (inst == nullptr) {
		GetMinionOrders(companion, orders); // a minion of the army is driven by the same brain (oracool/minions.h)
		return orders;
	}
	const Player *owner = OwnerOf(*inst);
	if (owner == nullptr || !owner->plractive)
		return orders;
	const CompanionDef &def = DefOf(inst->kind);
	orders.valid = true;
	orders.owner = owner->position.tile;
	orders.home = FormationHome(*owner, inst->order);
	orders.attack = def.attack;
	orders.missile = MissileID::Arrow;
	if (def.role == Role::Bait) {
		// A decoy stays where it was put and does nothing but draw blows.
		orders.leash = orders.settle = orders.regroup = 1000;
		return orders;
	}
	const bool armed = def.attack != CompanionAttack::None;
	switch (Stance) {
	case CompanionStance::Follow:
		// User, 2026-09-14: "stick close to me - 2-3 tiles range and shoot whoever is closest".
		orders.leash = 3;
		orders.settle = 2;
		orders.regroup = 10;
		orders.attacks = armed;
		orders.reach = def.attack == CompanionAttack::Bow ? 8 : def.attack == CompanionAttack::Throw ? ThrowReach : 4;
		break;
	case CompanionStance::Hold:
		orders.leash = 1000;
		orders.settle = 1000;
		orders.regroup = 14;
		orders.attacks = armed;
		orders.reach = def.attack == CompanionAttack::Bow ? 8 : def.attack == CompanionAttack::Throw ? ThrowReach : 1;
		break;
	case CompanionStance::Aggressive:
		orders.leash = 6;
		orders.settle = 3;
		orders.regroup = 12;
		orders.attacks = armed;
		orders.reach = def.attack == CompanionAttack::Throw ? ThrowReach : 8; // a throw's reach (audit, 2026-09-29)
		break;
	case CompanionStance::Passive:
		orders.leash = 2;
		orders.settle = 1;
		orders.regroup = 8;
		orders.attacks = false;
		break;
	}
	return orders;
}

Monster *PickCompanionTarget(const Monster &companion, const CompanionOrders &orders)
{
	if (!orders.attacks)
		return nullptr;
	const auto inReach = [&](const Monster &monster) {
		if (orders.attack == CompanionAttack::Bow || orders.attack == CompanionAttack::Throw)
			return companion.position.tile.WalkingDistance(monster.position.tile) <= orders.reach
			    && LineClearMissile(companion.position.tile, monster.position.tile);
		const int fromHer = companion.position.tile.WalkingDistance(monster.position.tile);
		if (Stance == CompanionStance::Hold)
			return fromHer <= 1;
		// Within the leash (one past it) and not behind a wall: a melee companion took a monster in the next room over a
		// reachable one, or one past its leash, and stood idle or rocked a tile back and forth (round 17 audit).
		return std::min(fromHer, orders.owner.WalkingDistance(monster.position.tile)) <= orders.reach
		    && orders.owner.WalkingDistance(monster.position.tile) <= orders.leash + 1
		    && LineClearMissile(companion.position.tile, monster.position.tile);
	};
	// Focus fire: what the owner is striking comes first.
	if (FocusMonster >= 0 && static_cast<size_t>(FocusMonster) < MaxMonsters) {
		Monster &focus = Monsters[FocusMonster];
		if (Targetable(focus) && inReach(focus))
			return &focus;
	}
	// Then the enemy nearest the owner - the companion guards the hero.
	Monster *best = nullptr;
	int bestDistance = 0;
	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		Monster &monster = Monsters[ActiveMonsters[i]];
		if (!Targetable(monster) || !inReach(monster))
			continue;
		const int distance = orders.owner.WalkingDistance(monster.position.tile);
		if (best == nullptr || distance < bestDistance) {
			best = &monster;
			bestDistance = distance;
		}
	}
	return best;
}

void AimCompanion(Monster &companion, const Monster &target)
{
	if (Instance *inst = InstanceInSlot(companion); inst != nullptr)
		inst->target = static_cast<int>(target.getId());
}

CompanionAct TryCompanionAbility(Monster &companion, Monster &target)
{
	Instance *inst = InstanceInSlot(companion);
	if (inst == nullptr || inst->cooldown > 0)
		return CompanionAct::None;
	const Player *owner = OwnerOf(*inst);
	if (owner == nullptr)
		return CompanionAct::None;
	const CompanionDef &def = DefOf(inst->kind);
	const CompanionStats stats = CompanionStatsAt(inst->kind, inst->rank);
	// Level 10 makes every ability hit a quarter harder; level 20 brings them round a quarter sooner.
	const Point here = companion.position.tile;
	const Point there = target.position.tile;
	const int distance = here.WalkingDistance(there);

	CompanionAct act = CompanionAct::None;
	switch (def.ability) {
	case Ability::Volley:
		if (distance <= 8 && LineClearMissile(here, there)) {
			inst->volley = true;
			act = CompanionAct::Volley;
		}
		break;
	case Ability::DoubleThrow:
		// Thrown as his ordinary throw is, twice (CompanionShot).
		if (distance <= ThrowReach && LineClearMissile(here, there)) {
			inst->volley = true;
			act = CompanionAct::Volley;
		}
		break;
	case Ability::Taunt:
		inst->tauntTicks = TauntTicks;
		Ring(*inst, here);
		act = CompanionAct::Acted;
		break;
	case Ability::None:
		break;
	}
	if (act != CompanionAct::None)
		inst->cooldown = AbilityCooldownTicks(def, inst->rank);
	return act;
}

int CompanionActionFrame(const Monster &companion)
{
	const Instance *inst = InstanceInSlot(companion);
	if (inst == nullptr)
		return 0;
	const CompanionDef &def = DefOf(inst->kind);
	return std::max(ActionFrame(PlayersAnimData[static_cast<size_t>(def.sheetClass)], def.weapon) - 1, 0);
}

void CompanionMeleeHit(Monster &companion)
{
	const Instance *inst = InstanceInSlot(companion);
	if (inst == nullptr || inst->target < 0 || static_cast<size_t>(inst->target) >= MaxMonsters)
		return;
	const Player *owner = OwnerOf(*inst);
	Monster &target = Monsters[inst->target];
	if (owner == nullptr || !Targetable(target) || companion.position.tile.WalkingDistance(target.position.tile) > 1)
		return;
	StrikeFor(*owner, target, OwnerBlow(*owner, CompanionStatsAt(inst->kind, inst->rank).damagePercent, &target));
}

namespace {

/**
 * @brief Madawc's throw (2026-09-29: "madawc to toss the hammer asset we introduced for blessed hammer skill. scaled to be
 * no bigger that regular barb axe toss"): the engine's arrow with his owner's blow at his share, as the Barbarian's Weapon
 * Throw flies, wearing the Blessed Hammer's spinning sheet at ThrownHammerPercent. Two, side by side, for Double Throw.
 */
void ThrowHammers(Instance &inst, Point from, Point to)
{
	const Direction dir = GetDirection(from, to);
	const Displacement side { Left(Left(dir)) };
	const int hammers = inst.volley ? 2 : 1;
	// Double Throw is his ability, at its power: a quarter harder from level 10, as the tooltip says (audit, 2026-09-29).
	const CompanionStats stats = CompanionStatsAt(inst.kind, inst.rank);
	const int percent = inst.volley ? AbilityPower(stats, inst.rank) : stats.damagePercent;
	for (int i = 0; i < hammers; i++) {
		// Both at the target; the second leaves from beside him, where the tile is open (audit, 2026-09-29: aimed a tile
		// aside, it flew past a lone target).
		Point start = from;
		if (i > 0) {
			const Point beside = from + side;
			if (InDungeonBounds(beside) && IsTileWalkable(beside))
				start = beside;
		}
		Missile *hammer = AddMissile(start, to, GetDirection(start, to), MissileID::Arrow, TARGET_MONSTERS, inst.owner, 4, 0);
		if (hammer == nullptr)
			continue;
		hammer->companionPercent = static_cast<int16_t>(percent);
		if (MissileArtLoaded(MissileGraphicID::BlessedHammerSpin)) {
			UseMissileGraphic(*hammer, MissileGraphicID::BlessedHammerSpin);
			ScaleMissile(*hammer, ThrownHammerPercent);
		}
	}
	inst.volley = false;
	PlaySfxLoc(PS_SWING, from);
}

} // namespace

void CompanionShot(Monster &companion)
{
	Instance *inst = InstanceInSlot(companion);
	if (inst == nullptr)
		return;
	const Player *owner = OwnerOf(*inst);
	if (owner == nullptr)
		return;
	Point to = companion.enemyPosition;
	if (inst->target >= 0 && static_cast<size_t>(inst->target) < MaxMonsters && Targetable(Monsters[inst->target]))
		to = Monsters[inst->target].position.tile;
	const Point from = companion.position.tile;
	if (to == from) {
		inst->volley = false; // spent with the shot that did not fly, not carried to the next ordinary one (round 31 audit)
		return;
	}
	if (DefOf(inst->kind).attack == CompanionAttack::Throw) {
		ThrowHammers(*inst, from, to);
		return;
	}
	// The hero's own arrows: fire or lightning if her gear makes them so.
	MissileID type = MissileID::Arrow;
	if (HasAnyOf(owner->_pIFlags, ItemSpecialEffect::FireArrows))
		type = MissileID::FireArrow;
	else if (HasAnyOf(owner->_pIFlags, ItemSpecialEffect::LightningArrows))
		type = MissileID::LightningArrow;
	const Direction dir = GetDirection(from, to);
	const Displacement side { Left(Left(dir)) };
	const int arrows = inst->volley ? VolleyArrows(inst->rank) : 1;
	const int percent = CompanionStatsAt(inst->kind, inst->rank).damagePercent;
	for (int i = 0; i < arrows; i++) {
		const int spread = (i + 1) / 2 * (i % 2 == 0 ? -1 : 1); // 0, +1, -1, +2, -2
		const Point dst = to + Displacement { side.deltaX * spread, side.deltaY * spread };
		if (Missile *arrow = AddMissile(from, dst, dir, type, TARGET_MONSTERS, inst->owner, 4, 0); arrow != nullptr)
			arrow->companionPercent = static_cast<int16_t>(percent);
	}
	inst->volley = false;
	PlaySfxLoc(PS_BFIRE, from);
}

void OnCompanionRegrouped(const Monster &companion)
{
	if (const Instance *inst = InstanceInSlot(companion); inst != nullptr)
		Flourish(*inst, companion.position.tile);
}

// =================================================================================================================
// Stance and HUD
// =================================================================================================================

void FocusCompanionsOn(const Monster &monster, int ticks)
{
	FocusMonster = static_cast<int>(monster.getId());
	FocusTicks = ticks;
	FocusCommanded = true;
}

void OnCompanionFocusSlotFreed(size_t id)
{
	// The id outlived its monster: a skeleton raised into the slot wore the Commanded sigil, and a hostile there drew the
	// whole army (round 5 audit, v1.12.230).
	if (FocusMonster == static_cast<int>(id)) {
		FocusMonster = -1;
		FocusTicks = 0;
	}
}

bool IsCommandedTarget(const Monster &monster)
{
	return FocusCommanded && FocusTicks > 0 && FocusMonster == static_cast<int>(monster.getId()) && (monster.hitPoints >> 6) > 0;
}

const char *CompanionStanceName()
{
	return StanceName(Stance);
}

CompanionStance GetCompanionStance()
{
	return Stance;
}

void CycleCompanionStance()
{
	Stance = static_cast<CompanionStance>((static_cast<int>(Stance) + 1) % 4);
}

void AnnounceCompanionStance()
{
	EventPlrMsg(fmt::format(fmt::runtime(_("Companions: {:s}")), _(StanceName(Stance))), UiFlags::ColorWhitegold);
}

void ProcessTownCompanions()
{
	if (leveltype != DTYPE_TOWN)
		return;
	for (Instance &inst : Instances) {
		if (!inst.active)
			continue;
		const Player *owner = OwnerOf(inst);
		if (owner == nullptr || !owner->plractive)
			continue;
		TownState &v = inst.town;
		if (!v.sheets.active) {
			if (++v.placeWait >= TicksPerSecond) {
				v.placeWait = 0;
				PlaceInTown(inst, owner->position.tile);
			}
			continue;
		}
		const Point home = owner->position.tile;

		if (v.stepTick > 0) {
			const int frames = std::max<int>(TownAnim(v).frames, 1);
			v.frame = (v.stepTick * frames / v.stepTicks) % frames;
			if (++v.stepTick > v.stepTicks) {
				v.tile = v.next;
				v.stepTick = 0;
				v.frame = 0;
				v.frameTick = 0;
			}
			continue;
		}

		const int distance = v.tile.WalkingDistance(home);
		if (DefOf(inst.kind).role != Role::Bait) {
			if (distance > TownCatchUpDistance) {
				if (const std::optional<Point> spot = FreeTownTileNear(home, 3))
					v.tile = *spot;
			} else if (distance > TownFollowDistance) {
				const Direction toward = GetDirection(v.tile, home);
				for (int turn : { 0, 1, -1, 2, -2 }) {
					const Direction dir = Turned(toward, turn);
					const Point step = v.tile + dir;
					if (TownTileFree(step)) {
						v.dir = dir;
						v.next = step;
						v.stepTicks = distance > TownHurryDistance ? TownHurryTicks : TownStepTicks;
						v.stepTick = 1;
						v.frame = 0;
						break;
					}
				}
				if (v.stepTick > 0)
					continue;
			} else if (distance > 0) {
				v.dir = GetDirection(v.tile, home);
			}
		}

		if (++v.frameTick >= StandTicksPerFrame) {
			v.frameTick = 0;
			v.frame = (v.frame + 1) % std::max<int>(TownAnim(v).frames, 1);
		}
	}
}

void DrawTownCompanions(const Surface &out, Point tilePosition, Point targetBufferPosition)
{
	if (leveltype != DTYPE_TOWN)
		return;
	for (const Instance &inst : Instances) {
		const TownState &v = inst.town;
		if (!inst.active || !v.sheets.active || v.tile != tilePosition)
			continue;
		const AnimStruct &anim = TownAnim(v);
		if (!anim.sprites)
			continue;
		const ClxSpriteList list = anim.sprites->isSheet() ? anim.sprites->sheet()[static_cast<size_t>(v.dir)] : anim.sprites->list();
		if (list.numSprites() == 0)
			continue;
		const ClxSprite sprite = list[static_cast<size_t>(std::clamp<int>(v.frame, 0, static_cast<int>(list.numSprites()) - 1))];
		Point position = targetBufferPosition;
		if (v.stepTick > 0) {
			const Displacement step = WalkStep[static_cast<size_t>(v.dir)];
			position += Displacement { step.deltaX * v.stepTick / v.stepTicks, step.deltaY * v.stepTick / v.stepTicks };
		}
		position.x -= (static_cast<int>(sprite.width()) - 64) / 2;
		ClxDrawTRN(out, position, sprite, v.trn.get());
	}
}

namespace {

// The panel sits in the top-left corner under the clock and its speed band (game_clock.cpp: margin 8, clock 20,
// speed 16), where Diablo III keeps its companion portraits.
constexpr int HudX = 8;
constexpr int HudY = 8 + 20 + 16 + 6;
constexpr int HudWidth = 156;
constexpr int HeaderHeight = 16;
constexpr int RowHeight = 24;

Rectangle HeaderRect()
{
	return { Point { HudX, HudY }, Size { HudWidth, HeaderHeight } };
}

int MyCompanionCount()
{
	if (MyPlayer == nullptr)
		return 0;
	return static_cast<int>(std::count_if(Instances.begin(), Instances.end(),
	    [](const Instance &inst) { return inst.active && inst.owner == MyPlayer->getId(); }));
}

} // namespace

void DrawCompanionHud(const Surface &out)
{
	const int count = MyCompanionCount();
	if (count == 0)
		return;
	DrawHalfTransparentRectTo(out, HudX, HudY, HudWidth, HeaderHeight + count * RowHeight + 4);
	DrawString(out, fmt::format(fmt::runtime(_("Companions: {:s}")), _(StanceName(Stance))),
	    Rectangle { Point { HudX + 4, HudY }, Size { HudWidth - 8, HeaderHeight } },
	    { UiFlags::VerticalCenter | UiFlags::FontSize12 | UiFlags::ColorWhitegold });

	int y = HudY + HeaderHeight;
	const int barWidth = HudWidth - 8;
	for (const Instance &inst : Instances) {
		if (!inst.active || inst.owner != MyPlayer->getId())
			continue;
		const CompanionStats stats = CompanionStatsAt(inst.kind, inst.rank);
		DrawString(out, _(DefOf(inst.kind).name), Rectangle { Point { HudX + 4, y }, Size { barWidth, 12 } },
		    { UiFlags::FontSize12 | UiFlags::ColorWhite });
		DrawString(out, fmt::format("{:d}s", (inst.ticksLeft + TicksPerSecond - 1) / TicksPerSecond),
		    Rectangle { Point { HudX + 4, y }, Size { barWidth, 12 } }, { UiFlags::FontSize12 | UiFlags::ColorGold | UiFlags::AlignRight });

		const int maxLife = stats.hitPoints << 6;
		int life = inst.hitPoints < 0 ? maxLife : inst.hitPoints;
		if (inst.slot >= 0)
			life = Monsters[inst.slot].hitPoints;
		const int lifeWidth = std::clamp(barWidth * std::max(life, 0) / std::max(maxLife, 1), 0, barWidth);
		FillRect(out, HudX + 4, y + 14, barWidth, 4, 0);
		FillRect(out, HudX + 4, y + 14, lifeWidth, 4, PAL16_RED + 4);
		const int total = std::max(stats.seconds * TicksPerSecond, 1);
		FillRect(out, HudX + 4, y + 19, std::clamp(barWidth * inst.ticksLeft / total, 0, barWidth), 2, PAL16_YELLOW + 4);
		y += RowHeight;
	}
}

namespace {

/** @brief The whole panel as drawn - header and rows - which takes every click on it (round 31 audit: a click on a row
 * walked or cast at the tile beneath). */
Rectangle CompanionPanelRect()
{
	return { Point { HudX, HudY }, Size { HudWidth, HeaderHeight + MyCompanionCount() * RowHeight + 4 } };
}

} // namespace

bool IsPointOverCompanionHeader(Point mouse)
{
	return MyPlayer != nullptr && MyCompanionCount() > 0 && IsCornerHudShown() && CompanionPanelRect().contains(mouse);
}

bool HandleCompanionHudClick(Point mouse)
{
	if (MyPlayer == nullptr || MyCompanionCount() == 0 || !IsCornerHudShown() || !CompanionPanelRect().contains(mouse))
		return false;
	// The stance cycles on the header only; the rows take the click and do nothing.
	if (HeaderRect().contains(mouse)) {
		CycleCompanionStance();
		AnnounceCompanionStance();
	}
	return true;
}

} // namespace devilution::oracool
