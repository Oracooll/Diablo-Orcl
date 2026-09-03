#include "oracool/spell_descriptions.h"

#include <array>
#include <optional>

#include "oracool/cold.h"
#include "oracool/paladin_skills.h"
#include "oracool/melee_skills.h"
#include "oracool/rogue_arrows.h"
#include "oracool/warcries.h"
#include "utils/language.h"

namespace devilution {
namespace oracool {

namespace {

// Order IS the SpellID enum's, starting at Null and running to the last spell BEFORE the Paladin's
// skills - those are handled separately in GetSpellDescription, see there.
//
// Kept to one sentence and to roughly the length of the aura and Barbarian descriptions, because
// they all wrap into the same text column beside the same 38px icon.
constexpr std::array<const char *, static_cast<size_t>(SpellID::Charge)> Descriptions { {
	/* Null             */ "",
	/* Firebolt         */ N_("Hurls a small bolt of fire at a single target."),
	/* Healing          */ N_("Restores a portion of the caster's own health."),
	/* Lightning        */ N_("Looses a bolt of lightning that races along the ground."),
	/* Flash            */ N_("Discharges lightning into every tile around the caster."),
	/* Identify         */ N_("Reveals the hidden properties of a single item."),
	/* FireWall         */ N_("Raises a wall of flame that burns whatever crosses it."),
	/* TownPortal       */ N_("Opens a gateway to town, and another to return by."),
	/* StoneCurse       */ N_("Turns a monster to stone, holding it helpless for a while."),
	/* Infravision      */ N_("Reveals the warmth of living things through walls and dark."),
	/* Phasing          */ N_("Blinks the caster a short way off, to somewhere unchosen."),
	/* ManaShield       */ N_("Turns damage aside into mana, until the mana runs out."),
	/* Fireball         */ N_("Throws a ball of fire that bursts where it lands."),
	/* Guardian         */ N_("Plants a hydra that spits fire at whatever draws near."),
	/* ChainLightning   */ N_("Looses lightning that leaps on from one enemy to the next."),
	/* FlameWave        */ N_("Rolls a wall of fire outward from the caster."),
	/* DoomSerpents     */ N_("An unfinished incantation. Nothing answers it yet."),
	/* BloodRitual      */ N_("An unfinished rite. Nothing answers it yet."),
	/* Nova             */ N_("Bursts lightning outward in every direction at once."),
	/* Invisibility     */ N_("An unfinished working. Nothing answers it yet."),
	/* Inferno          */ N_("Breathes a cone of fire over everything ahead."),
	/* Golem            */ N_("Raises a servant of stone to fight in the caster's stead."),
	/* Rage             */ N_("Drives the Barbarian into a fury, and leaves him spent after."),
	/* Teleport         */ N_("Carries the caster at once to a chosen spot in sight."),
	/* Apocalypse       */ N_("Erupts fire beneath every monster on the level."),
	/* Etherealize      */ N_("Makes the caster briefly untouchable."),
	/* ItemRepair       */ N_("Mends a worn item back to full durability."),
	/* StaffRecharge    */ N_("Restores spent charges to a staff."),
	/* TrapDisarm       */ N_("Disarms a trap without setting it off."),
	/* Elemental        */ N_("Looses a wandering flame that seeks monsters out."),
	/* ChargedBolt      */ N_("Scatters several erratic bolts of lightning."),
	/* HolyBolt         */ N_("Strikes the undead with a bolt of blessed light."),
	/* Resurrect        */ N_("Restores a fallen companion to life."),
	/* Telekinesis      */ N_("Pulls a distant item or lever without touching it."),
	/* HealOther        */ N_("Restores a companion's health from a distance."),
	/* BloodStar        */ N_("Spends the caster's own blood to hurl a searing star."),
	/* BoneSpirit       */ N_("Sends a spirit that tears away part of a monster's life."),
	/* Mana             */ N_("Draws mana back to the caster, more of it at higher levels."),
	/* Magi             */ N_("Fills the caster's mana to the brim at once."),
	/* Jester           */ N_("Casts a spell at random - a gamble, and rarely a kind one."),
	/* LightningWall    */ N_("Raises a wall of lightning that scours whatever crosses it."),
	/* Immolation       */ N_("Bursts fire outward in every direction at once."),
	/* Warp             */ N_("Carries the caster back to the way into the level."),
	/* Reflect          */ N_("Turns a share of incoming damage back on whoever dealt it."),
	/* Berserk          */ N_("Turns a monster to the caster's side for a while."),
	/* RingOfFire       */ N_("Rings the caster in flame that burns what steps through."),
	/* Search           */ N_("Marks the items lying nearby for a while."),
	/* RuneOfFire       */ N_("Sets a rune that erupts in fire when disturbed."),
	/* RuneOfLight      */ N_("Sets a rune that bursts into blinding light when disturbed."),
	/* RuneOfNova       */ N_("Sets a rune that looses lightning all around when disturbed."),
	/* RuneOfImmolation */ N_("Sets a rune that engulfs its own ground in fire when disturbed."),
	/* RuneOfStone      */ N_("Sets a rune that turns whatever disturbs it to stone."),
} };

/**
 * @brief Whether every slot in the table above was actually filled in.
 *
 * This replaces a `Descriptions.size() == SpellID::LAST + 1` static_assert that could never fail:
 * the array's size came FROM the enum, so it was asserting a tautology while a short initialiser
 * list quietly value-initialised the missing tail to nullptr. It had already happened - Charge was
 * added to the enum, no line was added here, and the check said nothing. Scanning for the nullptr is
 * the check that was intended.
 */
constexpr bool EveryDescriptionFilled()
{
	for (const char *description : Descriptions) {
		if (description == nullptr)
			return false;
	}
	return true;
}
static_assert(EveryDescriptionFilled(),
    "a SpellID below SpellID::Charge has no description - this table is indexed by the enum");

} // namespace

const char *GetSpellDescription(SpellID spell)
{
	// The Paladin's skills describe themselves in paladin_skills.cpp, beside their level gates and
	// mana prices, rather than having those sentences copied here where the two could disagree.
	if (const std::optional<PaladinSkill> skill = PaladinSkillForSpell(spell); skill.has_value())
		return GetPaladinSkillData(*skill).description;
	// The cold line, likewise beside the numbers it describes - oracool/cold.cpp.
	if (IsColdSpell(spell))
		return ColdSpellDescription(spell);
	// And the Rogue's bow page - oracool/rogue_arrows.cpp.
	if (RogueArrowForSpell(spell).has_value())
		return RogueArrowDescription(spell);
	// And the Barbarian's and Monk's melee skills - oracool/melee_skills.cpp.
	if (ClassMeleeSkillForSpell(spell).has_value())
		return ClassMeleeSkillDescription(spell);
	// And the cries - oracool/warcries.cpp.
	if (IsWarcry(spell))
		return WarcryDescription(spell);
	// And the Rogue's two thrown bolts (Round 7), which ride Lightning and Nova and have no module.
	if (spell == SpellID::LightningBoltSkill)
		return N_("Hurl a bolt of lightning that races along the ground toward the target, at the rank. A javelin would carry it; here the bolt carries itself.");
	if (spell == SpellID::LightningFury)
		return N_("Hurl lightning that bursts outward in every direction at once, at the rank.");

	const auto index = static_cast<int>(spell);
	if (index < 0 || static_cast<size_t>(index) >= Descriptions.size())
		return "";
	return Descriptions[static_cast<size_t>(index)];
}

} // namespace oracool
} // namespace devilution
