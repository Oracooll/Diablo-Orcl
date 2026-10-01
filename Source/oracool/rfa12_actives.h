/**
 * @file oracool/rfa12_actives.h
 *
 * Oracool (2026-09-13): the 114 RfA-12 actives - the skills that are swung or cast.
 *
 * Every one has its own SpellID (v1.11.111) and one of two ways in, chosen by its SpellsData row:
 *
 *   SWUNG   MissileID::Null. The click becomes the ordinary attack with a latch naming the skill -
 *           oracool/melee_skills.h's mechanism, on a latch of its own so the Round 4 enum is untouched.
 *           PlrHitMonst asks Rfa12MeleeDamagePercent; DoAttack asks ApplyRfa12MeleeOnSwing once a swing.
 *
 *   CAST    MissileID::Warcry. The cast animation, the mana and the aim are the engine's; the missile's
 *           AddWarcry calls CastRfa12Active at the cast frame, and a false answer fizzles the cast and
 *           refunds it. The Rogue's bow skills come this way too, and refuse without a bow.
 *
 * What a skill leaves behind is state here: a hero's timed buffs, marks on monsters (Judgment,
 * Oathbrand, Hunter's Mark, curses, brands, threads), and ground effects (mines, turrets, storms, a
 * wake, conductors). It is per player and per monster slot, cleared with the RfA-12 effects' own state,
 * and ticked from ProcessRfa12Tick. The engine asks about it only through oracool/rfa12_effects.h, so
 * the hook sites that module wired do not multiply.
 */
#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "engine/point.hpp"
#include "spelldat.h"

namespace devilution {
struct Player;
struct Monster;
enum class MissileGraphicID : uint8_t; // misdat.h - the body overlays' sheets
} // namespace devilution

namespace devilution::oracool {

struct ItemBonusTotals;
enum class ClassTreeSkill : uint16_t;

/** @brief Whether @p spell is one of the 114 RfA-12 actives. */
bool IsRfa12Active(SpellID spell);

/** @brief Whether @p spell is one of the eight RfA-12 bow skills and @p player holds no bow to loose it from. */
bool Rfa12LacksBowFor(const Player &player, SpellID spell);

/** @brief Whether @p spell is swung on the RfA-12 melee latch rather than cast. */
bool IsRfa12Melee(SpellID spell);

/** @brief Whether @p player can swing @p spell at all right now - Aegis Slam wants a shield. */
bool Rfa12MeleeUsable(const Player &player, SpellID spell);

/** @brief Records which RfA-12 melee skill the swing now being launched was thrown with. */
void ArmRfa12Melee(std::optional<SpellID> spell);
/** @brief LatchClassMeleeSwingPrice for the RfA-12 melee latch (round 68 audit). */
void LatchRfa12SwingPrice(const Player &player);
void ForgetRfa12SwingPrice();

/** @brief The RfA-12 melee skill the swing being resolved was thrown with, if any. */
std::optional<SpellID> ArmedRfa12Melee();

/** @brief The armed skill's damage bonus on the swing being resolved, in percent. */
int Rfa12MeleeDamagePercent(const Player &player);
/** @brief The swing bonus @p spell adds at @p player's rank, or -1 when it is not an RfA-12 melee skill - the hero sheet's number. */
int Rfa12MeleeBonusPercentFor(const Player &player, SpellID spell);

/** @brief Everything the armed skill does beyond the swing's own blow. True if it struck anything. */
bool ApplyRfa12MeleeOnSwing(Player &player, Monster *front, bool frontHit, int frontDamage);

/** @brief Performs a cast RfA-12 active at its cast frame. False if it had nothing to do. */
bool CastRfa12Active(Player &player, SpellID spell, Point target);

/** @brief Ticks before @p spell can be cast again (Absolute Zero's 30 seconds, 2026-10-01). Zero when it is ready. */
int Rfa12CooldownTicksLeft(const Player &player, SpellID spell);
/** @brief How far @p spell's cooldown has run, 0 just cast to 1 ready - the skill well's band. 1 for a skill with none. */
float Rfa12CooldownProgress(const Player &player, SpellID spell);

/** @brief The last open tile of the straight line from @p here to @p aim, short of a wall or blocked sight (round 46). */
Point LastOpenTileToward(Point here, Point aim);

/**
 * @brief Where @p player can land near @p dst: the nearest tile within five a hero may stand on AND that he can see from where
 * he stands - the engine's teleport search, with sight (round 46 audit: a crowded target sent him through the wall). Nullopt
 * when there is none.
 */
std::optional<Point> SightedLandingNear(const Player &player, Point dst);

/** @brief Whether a cast of @p spell leaves the cry's shockwave on the floor under the caster. */
bool Rfa12CastLeavesRing(SpellID spell);

// ---- asked through oracool/rfa12_effects.h ----------------------------------------------------

int Rfa12ActiveDamageDealtPercent(const Player &player, const Monster &target, bool melee);
bool Rfa12ActiveEvadesMelee(const Player &player);
/** @brief The chance, in percent, that Rfa12ActiveEvadesMelee slips a blow (Mantra of Evasion). No roll. */
int Rfa12ActiveMeleeEvadeChance(const Player &player);
/** @brief Chord of Warding: what is left of @p damage (1/64 units) after the ward drinks its share. */
int Rfa12ActiveAbsorbDamage(Player &player, int damage);
bool Rfa12ActiveStripsResistances(const Monster &monster);
/** @brief Hunter's Claim: whether @p player's arrow passes by @p monster on its way to the claimed one. */
bool Rfa12ActiveArrowIgnores(const Player &player, const Monster &monster);
/** @brief Bone Armor's shell (RfA-17 batch 38): the frame to draw over the hero, or -1 while no shell stands. */
int Rfa12BoneShellFrame(const Player &player);

/** @brief One loop worn around the hero's body (RfA-27 batch 56): which sheet, and which of its frames now. */
struct Rfa12BodyOverlay {
	MissileGraphicID art;
	int frame;
};

/**
 * @brief The body overlays @p player wears for his running buffs - Static Charge, Conduit, Immolate, the mantras of
 * Clarity and Evasion, Poison Dagger - at most @p capacity of them into @p out. Only sheets in the archive are listed, so
 * a missing one is simply not drawn. Returns how many were written. For scrollrt's DrawPlayerIcons.
 */
size_t Rfa12BodyOverlays(const Player &player, Rfa12BodyOverlay *out, size_t capacity);

/**
 * @brief The skill markers over a monster's head (RfA-27 batch 58), in ui\skill_markers.png's cell order - one bit per
 * cell, bit N for cell N. Each is a mark or condition the skill left, drawn only while it holds.
 */
enum class SkillMarker : uint8_t {
	Judged,       // Judgment
	OathBranded,  // Oathbrand
	Frostbitten,  // Frostbite
	AshenBranded, // Ashen Brand
	Hunted,       // Hunter's Mark
	Claimed,      // Hunter's Claim
	Pinned,       // Anchor Javelin
	Bleeding,     // Rend, Barbed Shaft, Tiger Claw, Exploding Palm (any bleed)
	Slowed,       // Crippling Shot, Low Branch, Pressure Point
	ArmourBroken, // Pressure Point
	Rotting,      // Decompose
	Commanded,    // Command the Dead
};
constexpr int SkillMarkerCount = static_cast<int>(SkillMarker::Commanded) + 1;

/** @brief The SkillMarker bits that hold on @p monster now; 0 for a dead one. */
uint16_t Rfa12SkillMarkers(const Monster &monster);
/** @brief Astral Projection: whether the hero is out of body, and unnoticed. */
bool Rfa12ActiveHidesPlayer(const Player &player);
/** @brief Conduit is on him: drawn in Tint::Electric (user, 2026-10-01). */
bool Rfa12ConduitWorn(const Player &player);
/** @brief Mantra of Retribution: whether the ring of thorns is worn now (drawn as a colour-cycled still, v1.12.211). */
bool Rfa12RetributionWorn(const Player &player);
/** @brief Serenity's ring (v1.12.211): how far through its rise and fall, 0..1, or nothing while no ring plays. */
std::optional<double> Rfa12SerenityProgress(const Player &player);
void OnRfa12ActiveHit(Player &player, Monster &monster, int damage, bool melee);
void OnRfa12ActiveStruck(Player &player, Monster &monster);
void OnRfa12ActiveMissileStruck(Player &player, Monster &monster, int damage);
void OnRfa12ActiveMonsterKilled(Player &player, const Monster &monster);
void ApplyRfa12ActiveBuffsToTotals(const Player &player, ItemBonusTotals &totals);
void ProcessRfa12ActivesTick(Player &player);
void ClearRfa12ActivesState();
void ClearRfa12ActivesForMonster(const Monster &monster);
void ClearRfa12ActiveBuffs(Player &player);
/** @brief The hero's own RfA-12 buffs only (a respawn): companions, curses and clocks stay. The caller recalculates. */
void ClearRfa12PlayerBuffs(Player &player);

/** @brief Ticks left on @p player's timed buff from @p spell; 0 when not carried, or @p spell leaves no buff. For the countdown column. */
int Rfa12BuffTicks(const Player &player, SpellID spell);

/**
 * @brief Every ground effect @p player has running that lasts 5 s or more (Brittle Ground, Frozen Sentinel, the placed
 * fields), one entry a skill with its longest ticks left, in SpellID order. For the countdown column (2026-10-01).
 */
std::vector<std::pair<SpellID, int>> Rfa12FieldTimers(const Player &player);

/** @brief Frostbite: the extra cold damage @p monster takes, in percent. */
int Rfa12FrostbitePercent(const Monster &monster);

/**
 * @brief Tooltip lines for what @p spell does at @p rank - its main effect with numbers, from the same
 * named helpers the cast reads. Every RfA-12 active, the census actives, and the Necromancer's (curses and
 * summons are routed to CurseFactsAt / NecroSummoningFactsAt). Empty for any other spell.
 */
std::string Rfa12ActiveFactsAt(const Player &player, SpellID spell, int rank);

/**
 * @brief Tooltip lines for the tree passives and Passive Skills page rows whose RULE lives in rfa12_actives.cpp, at
 * @p points (1 for a Passive Skills page row): the main effect with its numbers, from the same named
 * helpers the rule reads. Empty for any other row. See oracool/skill_facts.h for the two rules.
 */
std::string Rfa12ActivesPassiveFactsAt(const Player &player, ClassTreeSkill skill, int points);

/** @brief The Abilities window's sentence for @p spell - its tree row's. Empty for anything else. */
const char *Rfa12ActiveDescription(SpellID spell);

/**
 * @brief Vanilla's Holy Bolt explosion on @p tile as a Paladin skill's impact, at @p percent of its size, tinted @p rgb
 * (oracool::hue, missile_tint.h; 0 for its own colours). Drawn only. False when holyexpl is not loaded (headless). For the skills outside
 * rfa12_actives.cpp: Hammer of Faith, Zeal, Smite, Sacrifice, Charge.
 */
bool DrawHolyBurst(const Player &player, Point tile, int percent, uint32_t rgb);

} // namespace devilution::oracool
