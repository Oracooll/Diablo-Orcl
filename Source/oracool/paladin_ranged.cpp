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
	// No shield check here any more: requiresShield is part of IsPaladinSkillUnlocked, which
	// CanUsePaladinSkill already asked before this ran, so a shieldless Paladin never gets here.
	const int damage = RollWeaponDamage(player) * BlessedShieldPercentAt(spellLevel) / 100;
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
	const int damage = RollWeaponDamage(player) * BlessedHammerPercentAt(spellLevel) / 100;
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

	// CHARGED BOLTS, dispersing from where the fist struck (user, 2026-09-12: "FotH to disperse
	// Charged Bolts when it lands instead of this current asset").
	//
	// This was a rigid Nova ring - 36 MiniNovaBall missiles on a radius-4 arc, re-skinned to
	// holy_spark - which served the original 2026-08-15 spec ("cast Mini-Nova spell at cursor
	// location [...] travel distance of lightnings of 4 tiles"). Charged Bolts are a different
	// thing: they WANDER. So the ring is now an aiming device rather than the effect itself - each
	// bolt is launched at a point on it and then goes where it goes, which is what "disperse" means
	// and what a Nova ring by construction cannot do.
	//
	// The count is the vanilla Charged Bolt spell's own scaling plus a little, because this is an
	// area finisher rather than a primary attack. Far fewer than 36 on purpose: a ChargedBolt lives
	// with _mirange 256 and steers every tick, where MiniNovaBall was short and unblockable, so
	// thirty-six of them would be both a missile-pool and a frame-time problem. AddMissile returning
	// nullptr on a full pool is handled the way it always is here - by moving on.
	const int boltDamage = std::max(damage * FistNovaPercentAt(spellLevel) / 100, 1);
	const int boltCount = spellLevel / 2 + 5;
	// The radius-4 ring the spec asked for, kept as the aim points. Eight directions, so the spread
	// is even however many bolts the level buys.
	constexpr std::array<WorldTileDisplacement, 8> aimRing = {
		{ { 4, 0 }, { 3, 3 }, { 0, 4 }, { -3, 3 }, { -4, 0 }, { -3, -3 }, { 0, -4 }, { 3, -3 } }
	};
	for (int i = 0; i < boltCount; i++) {
		const WorldTileDisplacement aim = aimRing[static_cast<size_t>(i) % aimRing.size()];
		Missile *bolt = AddMissile(target, target + aim, player._pdir, MissileID::ChargedBolt,
		    TARGET_MONSTERS, static_cast<int>(player.getId()), boltDamage, spellLevel);
		if (bolt == nullptr)
			continue;
		// AddChargedBolt OVERWRITES _midam with GenerateRnd(caster's Magic / 4) + 1, because the
		// vanilla spell is a Sorcerer's and scales off Magic. Fist of the Heavens is a Paladin's
		// weapon-damage skill, so that roll would throw away everything the sheet quotes and leave
		// the bolts doing almost nothing on a Paladin's Magic. Writing it back after the add is the
		// only place to say so - the adder takes no damage argument it will respect.
		bolt->_midam = boltDamage;
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
	case PaladinSkill::BlessedShield:
		percent = BlessedShieldPercentAt(rank);
		break;
	case PaladinSkill::BlessedHammer:
		percent = BlessedHammerPercentAt(rank);
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
		damage += player._pDamageMod;
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
		// The CHARGED BOLTS, which is Lightning (user, 2026-09-12: "make sure lightning dmg skill
		// have their dmg font in gero stats screen in proper font color. start with foth").
		//
		// This asked ApocalypseBoom - the central blast - on the reasoning that "the mace only
		// falls; the blast on the target is what deals the damage the sheet quotes". ApocalypseBoom
		// is PHYSICAL, so the sheet drew Fist of the Heavens' damage in physical white while the
		// skill played a Lightning cast animation and threw Lightning bolts. Three descriptions of
		// one skill, and the sheet had the odd one.
		//
		// The bolts are the right thing to ask. They carry most of the damage at every level, they
		// are what the player sees, and asking the missile the skill actually throws is the same
		// rule the comment in ReadiedSpellDamageType describes - it is how Blessed Hammer's blue
		// stopped appearing as white. The central blast stays Physical, which is honest: a falling
		// mace is not lightning.
		return GetMissileData(MissileID::ChargedBolt).damageType();
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
