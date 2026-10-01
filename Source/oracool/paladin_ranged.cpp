#include "oracool/paladin_ranged.h"
#include "oracool/rfa12_actives.h" // LastOpenTileToward

#include "engine/random.hpp"
#include "missiles.h"
#include "monster.h"
#include "oracool/class_tree.h"
#include "oracool/paladin_melee.h" // HasShieldEquipped
#include "oracool/passives.h"      // Blunt, Towering Shield
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
	const int weaponRoll = damage;
	damage += damage * player._pIBonusDam / 100;
	damage += player._pIBonusDamMod;
	damage += StatDamage(player, weaponRoll);
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

// Fist of the Heavens' and Blessed Shield's shares of weapon damage grow a level at a time since
// 2026-09-12 - FistCentrePercentAt, FistNovaPercentAt and BlessedShieldPercentAt, paladin_ranged.h.

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
	// On the foot of a wall, the last open tile toward it (round 46 audit: LineClear does not test the end tile, and the
	// ring's 36 bolts fanned out of the wall into both rooms).
	if (InDungeonBounds(target) && IsTileSolid(target)) {
		target = LastOpenTileToward(player.position.tile, target);
		if (target == player.position.tile)
			return false; // the wall at his feet: nowhere to bring the mace down (round 47 audit)
	}
	// Not on a tile past a wall, before the mana (round 39 audit: a shift-click brought the mace down in the next room).
	if (!LineClearMissile(player.position.tile, target))
		return false;
	if (!SpendPaladinSkillMana(player, PaladinSkill::FistOfTheHeavens))
		return false;

	AddMissile(target, target, player._pdir, MissileID::FallingMace, TARGET_MONSTERS,
	    static_cast<int>(player.getId()), std::max(damage, 1), spellLevel);
	return true;
}

/**
 * @brief Hurls the shield at the target, to bounce on to the two nearest monsters.
 *
 * User spec (2026-08-15): thrown at the monster, travelling at twice a Holy Bolt's speed. The splash it
 * carried then ("causing splash dmg with range 1 on hit") gave way on 2026-09-11 to two bounces, each
 * for less - "100% on first, 75% on second, 50% on third". The flight is ProcessBlessedShieldThrow's;
 * the damage rolled here is the first strike's.
 */
bool CastBlessedShield(Player &player, Point target, int spellLevel)
{
	// No shield check here any more: requiresShield is part of CanUsePaladinSkill (since
	// 2026-09-27), which was asked before this ran, so a shieldless Paladin never gets here.
	// Towering Shield (2026-09-14) on top.
	const int damage = RollWeaponDamage(player) * (BlessedShieldPercentAt(spellLevel) * (100 + PassiveSkillDamagePercent(player, SpellID::BlessedShield)) / 100) / 100;
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

// Blessed Hammer's share of weapon damage on each tile it crosses: BlessedHammerPercentAt, paladin_ranged.h.

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
	// Blunt (2026-09-14) on top.
	const int damage = RollWeaponDamage(player) * (BlessedHammerPercentAt(spellLevel) * (100 + PassiveSkillDamagePercent(player, SpellID::BlessedHammer)) / 100) / 100;
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
	DropBlast(player, target, damage * FistCentrePercentAt(spellLevel) / 100, spellLevel);
	// "Replace the sound on ground hit with the sound we use for SORT buttons" - IS_ISHIEL, the
	// shield-into-slot sound the Stash's and the inventory's Sort buttons both play.
	PlaySfxLoc(IS_ISHIEL, target);

	// The ring, laid out exactly as ProcessNovaCommon does: a quarter arc mirrored into four, which
	// is what gives Nova its round front rather than a square one. Its radius is 4 tiles, which is
	// already the travel distance the user asked for.
	constexpr std::array<WorldTileDisplacement, 9> quarterRadius = {
		{ { 4, 0 }, { 4, 1 }, { 4, 2 }, { 4, 3 }, { 4, 4 }, { 3, 4 }, { 2, 4 }, { 1, 4 }, { 0, 4 } }
	};
	const int boltDamage = std::max(damage * FistNovaPercentAt(spellLevel) / 100, 1);
	// THE LIGHTNING SPRITE, not the holy spark (user, 2026-09-13: "i just want to change the asset
	// that disperses with the lightning asset used in charged bolt").
	//
	// That asset needs no swap at all - it is what MiniNovaBall already draws. MiniNovaBall IS
	// NovaBall's behaviour with MissileGraphicID::ChargedBolt, the "miniltng" sheet the Charged Bolt
	// spell uses, so removing the re-skin below is the entire change. Between 2026-09-11 and now the
	// ring wore MissileGraphicID::HolySpark whenever that art was loaded, which is what made the
	// landing look like a burst of sparks rather than lightning.
	for (WorldTileDisplacement quarterOffset : quarterRadius) {
		const std::array<WorldTileDisplacement, 4> offsets {
			quarterOffset, quarterOffset.flipXY(), quarterOffset.flipX(), quarterOffset.flipY()
		};
		for (WorldTileDisplacement offset : offsets) {
			AddMissile(target, target + offset, player._pdir, MissileID::MiniNovaBall,
			    TARGET_MONSTERS, static_cast<int>(player.getId()), boltDamage, spellLevel);
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

bool IsCastPaladinSkill(PaladinSkill skill)
{
	switch (skill) {
	case PaladinSkill::FistOfTheHeavens:
	case PaladinSkill::BlessedShield:
	case PaladinSkill::BlessedHammer:
		return true;
	case PaladinSkill::Charge:
	case PaladinSkill::Zeal:
	case PaladinSkill::HammerOfFaith:
	case PaladinSkill::ShieldBash:
		break;
	}
	return false;
}

bool CanStartRangedPaladinSkill(const Player &player, PaladinSkill skill)
{
	return IsCastPaladinSkill(skill) && CanUsePaladinSkill(player, skill) && MissilePoolHasRoom();
}

std::optional<MagicType> PaladinCastAnimation(SpellID spell)
{
	switch (spell) {
	case SpellID::BlessedHammer:
		// Magic since its damage became magic (user, 2026-09-11: "since we are making it do Magic dmg then
		// when it is cast hero should play Magic spell animation, not Fire").
		return MagicType::Magic;
	case SpellID::BlessedShield:
		return MagicType::Magic;
	case SpellID::FistOfTheHeavens:
		return MagicType::Lightning;
	default:
		return std::nullopt;
	}
}

std::optional<std::pair<int, int>> PaladinCastDamageRange(const Player &player, PaladinSkill skill)
{
	int percent = 0;
	const int rank = std::max(player.GetSpellLevel(GetPaladinSkillData(skill).spellId), 1);
	switch (skill) {
	case PaladinSkill::FistOfTheHeavens:
		percent = FistCentrePercentAt(rank);
		break;
	// Towering Shield and Blunt on top, as the casts apply them - the sheet read 25% low with either (round 5 audit).
	case PaladinSkill::BlessedShield:
		percent = BlessedShieldPercentAt(rank) * (100 + PassiveSkillDamagePercent(player, SpellID::BlessedShield)) / 100;
		break;
	case PaladinSkill::BlessedHammer:
		percent = BlessedHammerPercentAt(rank) * (100 + PassiveSkillDamagePercent(player, SpellID::BlessedHammer)) / 100;
		break;
	case PaladinSkill::Charge:
	case PaladinSkill::Zeal:
	case PaladinSkill::HammerOfFaith:
	case PaladinSkill::ShieldBash:
		return std::nullopt;
	}
	// RollWeaponDamage at each end of the weapon's range, then the skill's share - the same arithmetic
	// the cast does, so the sheet quotes what a hit will actually take.
	const auto at = [&player, percent](int weapon) {
		int damage = weapon;
		damage += damage * player._pIBonusDam / 100;
		damage += player._pIBonusDamMod;
		damage += StatDamage(player, weapon);
		return std::max(std::max(damage, 1) * percent / 100, 1);
	};
	const int minDamage = player._pIMinDam;
	const int maxDamage = std::max(player._pIMaxDam, minDamage);
	return std::pair<int, int> { at(minDamage), at(maxDamage) };
}

std::optional<DamageType> PaladinCastDamageType(PaladinSkill skill)
{
	switch (skill) {
	case PaladinSkill::FistOfTheHeavens:
		// The RING, which is Lightning (user, 2026-09-12: "make sure lightning dmg skill have their
		// dmg font in gero stats screen in proper font color. start with foth").
		//
		// This asked ApocalypseBoom - the central blast - on the reasoning that "the mace only
		// falls; the blast on the target is what deals the damage the sheet quotes". ApocalypseBoom
		// is PHYSICAL, so the sheet drew Fist of the Heavens' damage in physical white while the
		// skill played a Lightning cast animation and threw Lightning bolts. Three descriptions of
		// one skill, and the sheet had the odd one.
		//
		// The ring is the right thing to ask. It carries most of the damage at every level, it is
		// what the player sees, and asking the missile the skill actually throws is the same rule
		// the comment in ReadiedSpellDamageType describes - it is how Blessed Hammer's blue stopped
		// appearing as white. The central blast stays Physical, which is honest: a falling mace is
		// not lightning.
		return GetMissileData(MissileID::MiniNovaBall).damageType();
	case PaladinSkill::BlessedShield:
		return GetMissileData(MissileID::BlessedShieldThrow).damageType();
	case PaladinSkill::BlessedHammer:
		return GetMissileData(MissileID::BlessedHammer).damageType();
	case PaladinSkill::Charge:
	case PaladinSkill::Zeal:
	case PaladinSkill::HammerOfFaith:
	case PaladinSkill::ShieldBash:
		break;
	}
	return std::nullopt;
}

std::string PaladinRangedFactsAt(PaladinSkill skill, int rank)
{
	rank = std::max(rank, 1); // weapon damage, and a share of it that grows with the level (2026-09-12)
	std::string out;
	const auto line = [&out](const std::string &s) {
		if (!out.empty())
			out += '\n';
		out += s;
	};
	switch (skill) {
	case PaladinSkill::FistOfTheHeavens:
		line(fmt::format(fmt::runtime(_("Damage: {:d}% at the centre, {:d}% around it")), FistCentrePercentAt(rank), FistNovaPercentAt(rank)));
		break;
	case PaladinSkill::BlessedShield:
		line(fmt::format(fmt::runtime(_("Magic damage: {:d}%, then {:d}% and {:d}% of that as it bounces")),
		    BlessedShieldPercentAt(rank), BlessedShieldStrikePercent[1], BlessedShieldStrikePercent[2]));
		break;
	case PaladinSkill::BlessedHammer:
		line(fmt::format(fmt::runtime(_("Magic damage: {:d}% per hit")), BlessedHammerPercentAt(rank)));
		break;
	default:
		break;
	}
	return out;
}

} // namespace devilution::oracool
