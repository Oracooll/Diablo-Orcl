#include "oracool/paladin_ranged.h"

#include "engine/random.hpp"
#include "missiles.h"
#include "monster.h"
#include "oracool/class_tree.h"
#include "oracool/paladin_melee.h" // HasShieldEquipped
#include "oracool/skill_sounds.h"
#include "player.h"
#include <fmt/format.h>
#include "utils/language.h"

namespace devilution::oracool {

namespace {

/**
 * @brief One roll of the character's own weapon damage, in whole hit points.
 *
 * Oracool: user decision (2026-08-15) - these skills scale off the WEAPON, not off character level,
 * so gear matters and they keep a melee feel even when they strike at a distance.
 *
 * Mirrors the first lines of the engine's own melee roll (player.cpp's PlrHitMonst): the weapon's
 * min..max, then the item's percentage bonus, its flat bonus, and the character's damage modifier.
 * Deliberately WITHOUT the two things that follow it there - the Warrior/Barbarian double-damage
 * roll and the weapon-type modifiers - because those describe a swing connecting, and none of these
 * skills is a swing.
 *
 * Whole hit points rather than the 1/64ths the player's own pools use, matching what every missile
 * puts in _midam; CheckMissileCol does the shift.
 */
int RollWeaponDamage(const Player &player)
{
	const int minDamage = player._pIMinDam;
	const int maxDamage = std::max(player._pIMaxDam, minDamage);
	int damage = GenerateRnd(maxDamage - minDamage + 1) + minDamage;
	damage += damage * player._pIBonusDam / 100;
	damage += player._pIBonusDamMod;
	damage += player._pDamageMod;
	return std::max(damage, 1);
}

/**
 * @brief Drops one explosion on @p tile carrying @p damage.
 *
 * MissileID::ApocalypseBoom is the engine's own one-tile blast: it plants itself where it is told,
 * runs its animation, and damages whatever is standing there exactly once. Apocalypse builds its
 * whole effect by scattering these over an area, so using them the same way is reuse of a pattern,
 * not a workaround - and it is what "reuse existing art now, swap later" (user, 2026-08-15) buys.
 */
void DropBlast(const Player &player, Point tile, int damage, int spellLevel)
{
	if (!InDungeonBounds(tile))
		return;
	AddMissile(tile, tile, Direction::South, MissileID::ApocalypseBoom, TARGET_MONSTERS,
	    static_cast<int>(player.getId()), damage, spellLevel);
}

/** @brief Fist of the Heavens' blast on the target's own tile, as a percentage of weapon damage. */
constexpr int FistCentrePercent = 150;
/** @brief And what each mini-Nova bolt carries - it is the shockwave, not the fist. */
constexpr int FistNovaPercent = 60;

/** @brief Blessed Shield's damage, as a percentage of weapon damage. */
constexpr int BlessedShieldPercent = 125;

/**
 * @brief A divine fist lands on the target, and lightning runs out from where it struck.
 *
 * User spec (2026-08-15): the impact, then "cast Mini-Nova spell at cursor location [...] with dmg
 * according to equipped weapon and travel distance of lightnings of 4 tiles".
 *
 * The 4 tiles needed no work: ProcessNovaCommon already fires its bolts at a radius-4 ring, so
 * vanilla Nova's reach IS the number asked for. What "mini" needed was a smaller bolt, and that art
 * also already shipped - MissileID::MiniNovaBall is NovaBall's behaviour with ChargedBolt's sprite,
 * whose file is literally named "miniltng".
 *
 * The mace really does fall: MissileID::FallingMace plays items\mace.cel, the tumble an item makes
 * when it lands on the floor. Everything below happens on its last frame - see
 * FistOfTheHeavensImpact.
 */
bool CastFistOfTheHeavens(Player &player, Point target, int spellLevel)
{
	const int damage = RollWeaponDamage(player);
	// Room for the missile is checked BEFORE the mana is taken - AddMissile returns nullptr on
	// a full pool, and this used to spend first and discard that result (audit, 2026-08-26).
	if (!MissilePoolHasRoom())
		return false;
	if (!SpendPaladinSkillMana(player, PaladinSkill::FistOfTheHeavens))
		return false;

	AddMissile(target, target, player._pdir, MissileID::FallingMace, TARGET_MONSTERS,
	    static_cast<int>(player.getId()), std::max(damage, 1), spellLevel);
	return true;
}

/**
 * @brief Hurls the shield at the target, to burst over a tile's radius where it lands.
 *
 * User spec (2026-08-15): thrown at the monster, travelling at twice a Holy Bolt's speed, "causing
 * splash dmg with range 1 on hit". A real travelling missile now, where the first version dropped
 * blasts on several enemies at once - which delivered damage to a crowd but never actually threw
 * anything.
 *
 * The spin and the brighter shield are NOT here: the game ships no shield missile art, and the item
 * shield is a static inventory icon with no frames to spin. HolyBolt's bright bolt stands in, which
 * is at least the right register for a blessed throw. See the dev report.
 */
bool CastBlessedShield(Player &player, Point target, int spellLevel)
{
	// No shield check here any more: requiresShield is part of IsPaladinSkillUnlocked, which
	// CanUsePaladinSkill already asked before this ran, so a shieldless Paladin never gets here.
	const int damage = RollWeaponDamage(player) * BlessedShieldPercent / 100;
	// Room for the missile is checked BEFORE the mana is taken - AddMissile returns nullptr on
	// a full pool, and this used to spend first and discard that result (audit, 2026-08-26).
	if (!MissilePoolHasRoom())
		return false;
	if (!SpendPaladinSkillMana(player, PaladinSkill::BlessedShield))
		return false;

	AddMissile(player.position.tile, target, player._pdir, MissileID::BlessedShieldThrow,
	    TARGET_MONSTERS, static_cast<int>(player.getId()), std::max(damage, 1), spellLevel);
	return true;
}

/** @brief Blessed Hammer's damage, as a percentage of weapon damage, on each tile it crosses. */
constexpr int BlessedHammerPercent = 60;

/**
 * @brief A hammer that winds outward from the caster's own feet.
 *
 * Launched FROM the player rather than at the target - "spirals outward of you" - so the click's job
 * is only to say that a target was in range, which the caller has already checked. The spiral itself
 * is ProcessBlessedHammer's; see the note there for why it is the one missile that does not travel
 * on a velocity vector.
 *
 * The per-tile damage is the lowest of the three because the hammer crosses many tiles in one cast,
 * and each crossing is a hit.
 */
bool CastBlessedHammer(Player &player, int spellLevel)
{
	const int damage = RollWeaponDamage(player) * BlessedHammerPercent / 100;
	// Room for the missile is checked BEFORE the mana is taken - AddMissile returns nullptr on
	// a full pool, and this used to spend first and discard that result (audit, 2026-08-26).
	if (!MissilePoolHasRoom())
		return false;
	if (!SpendPaladinSkillMana(player, PaladinSkill::BlessedHammer))
		return false;

	AddMissile(player.position.tile, player.position.tile, player._pdir, MissileID::BlessedHammer,
	    TARGET_MONSTERS, static_cast<int>(player.getId()), std::max(damage, 1), spellLevel);
	return true;
}

} // namespace

void FistOfTheHeavensImpact(Player &player, Point target, int damage, int spellLevel)
{
	DropBlast(player, target, damage * FistCentrePercent / 100, spellLevel);
	// "Replace the sound on ground hit with the sound we use for SORT buttons" - IS_ISHIEL, the
	// shield-into-slot sound the Stash's and the inventory's Sort buttons both play.
	PlaySfxLoc(IS_ISHIEL, target);

	// The ring, laid out exactly as ProcessNovaCommon does: a quarter arc mirrored into four, which
	// is what gives Nova its round front rather than a square one. Its radius is 4 tiles, which is
	// already the travel distance the user asked for.
	constexpr std::array<WorldTileDisplacement, 9> quarterRadius = {
		{ { 4, 0 }, { 4, 1 }, { 4, 2 }, { 4, 3 }, { 4, 4 }, { 3, 4 }, { 2, 4 }, { 1, 4 }, { 0, 4 } }
	};
	const int boltDamage = std::max(damage * FistNovaPercent / 100, 1);
	// The holy spark once delivered (2026-09-11); until then MiniNovaBall's own ChargedBolt. Swapped
	// per missile, because MiniNovaBall is also a lesser unique's nova and keeps its look there.
	const bool holySpark = MissileArtLoaded(MissileGraphicID::HolySpark);
	int sparkFrame = 0;
	for (WorldTileDisplacement quarterOffset : quarterRadius) {
		const std::array<WorldTileDisplacement, 4> offsets {
			quarterOffset, quarterOffset.flipXY(), quarterOffset.flipX(), quarterOffset.flipY()
		};
		for (WorldTileDisplacement offset : offsets) {
			Missile *spark = AddMissile(target, target + offset, player._pdir, MissileID::MiniNovaBall,
			    TARGET_MONSTERS, static_cast<int>(player.getId()), boltDamage, spellLevel);
			if (spark != nullptr && holySpark) {
				UseMissileGraphic(*spark, MissileGraphicID::HolySpark);
				spark->_miAnimFrame = sparkFrame++ % spark->_miAnimLen + 1; // out of step, as NovaBall's are
			}
		}
	}
}

bool PlayPaladinMissileSound(const Missile &missile)
{
	if (missile._mitype != MissileID::BlessedShieldThrow || missile._micaster != TARGET_MONSTERS || missile._misource < 0)
		return false;
	return PlaySkillSound(ClassTreeSkill::BlessedShield, SkillSoundEvent::Cast);
}

void PlayBlessedShieldImpactSound(const Missile &missile)
{
	if (missile._micaster == TARGET_MONSTERS && missile._misource >= 0)
		PlaySkillSound(ClassTreeSkill::BlessedShield, SkillSoundEvent::Impact);
}

bool CastRangedPaladinSkill(Player &player, PaladinSkill skill, Point target)
{
	if (!CanUsePaladinSkill(player, skill))
		return false;

	const int spellLevel = player.GetSpellLevel(GetPaladinSkillData(skill).spellId);
	switch (skill) {
	case PaladinSkill::FistOfTheHeavens:
		return CastFistOfTheHeavens(player, target, spellLevel);
	case PaladinSkill::BlessedShield:
		return CastBlessedShield(player, target, spellLevel);
	case PaladinSkill::BlessedHammer:
		return CastBlessedHammer(player, spellLevel);
	// The melee three and Charge never reach here.
	case PaladinSkill::Charge:
	case PaladinSkill::Zeal:
	case PaladinSkill::HammerOfFaith:
	case PaladinSkill::ShieldBash:
		break;
	}
	return false;
}

std::string PaladinRangedFactsAt(PaladinSkill skill, int rank)
{
	(void)rank; // these scale off the weapon, not the rank - see the top of the file
	std::string out;
	const auto line = [&out](const std::string &s) {
		if (!out.empty())
			out += '\n';
		out += s;
	};
	switch (skill) {
	case PaladinSkill::FistOfTheHeavens:
		line(fmt::format(fmt::runtime(_("Damage: {:d}% at the centre, {:d}% around it")), FistCentrePercent, FistNovaPercent));
		break;
	case PaladinSkill::BlessedShield:
		line(fmt::format(fmt::runtime(_("Damage: {:d}% per target")), BlessedShieldPercent));
		break;
	case PaladinSkill::BlessedHammer:
		line(fmt::format(fmt::runtime(_("Damage: {:d}% per hit")), BlessedHammerPercent));
		break;
	default:
		break;
	}
	return out;
}

} // namespace devilution::oracool
