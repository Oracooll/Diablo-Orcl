#include "oracool/spell_ranks.h"

#include "player.h"
#include "spells.h"

namespace devilution::oracool {

namespace {

/**
 * @brief Band per SpellID, indexed by the enum. 0 means "not a book spell - no band".
 *
 * Derived from sBookLvl (1-2 -> 1, 3-4 -> 6, 5-6 -> 12, 7-9 -> 18, 10-14 -> 24, 15+ -> 30) and then written
 * out, so a spell can be moved a band without moving every other spell that shares its book depth.
 *
 * The three name-only stubs - Doom Serpents, Blood Ritual, Invisibility - are banded like anything
 * else. They do nothing yet, and the Spells sheet already sinks them to the bottom for that reason;
 * giving them a band costs nothing and means they need no special case on the day they get bodies.
 *
 * The seven Paladin skills, the six retired class skills and Town Portal are all 0: none of them is
 * learned from a book. The trees carry their own minLevel, which the Rule of Rangs is applied to at
 * the tree's own call site.
 */
constexpr int SpellBand[] = {
	0,  // Null
	1,  // Firebolt
	1,  // Healing
	6,  // Lightning
	12, // Flash
	0,  // Identify (class skill, retired)
	6,  // Fire Wall
	0,  // Town Portal (a built-in ability here, never a book spell)
	12, // Stone Curse
	12, // Infravision
	18, // Phasing
	12, // Mana Shield
	18, // Fireball
	18, // Guardian
	18, // Chain Lightning
	18, // Flame Wave
	18, // Doom Serpents
	24, // Blood Ritual
	24, // Nova
	18, // Invisibility
	6,  // Inferno
	24, // Golem
	0,  // Rage (class skill, retired)
	24, // Teleport
	30, // Apocalypse
	30, // Etherealize
	0,  // Item Repair (class skill, retired)
	0,  // Staff Recharge (class skill, retired)
	0,  // Trap Disarm (class skill, retired)
	18, // Elemental
	1,  // Charged Bolt
	1,  // Holy Bolt
	12, // Resurrect
	1,  // Telekinesis
	1,  // Heal Other
	24, // Blood Star
	18, // Bone Spirit
	1,  // Mana
	24, // the Magi
	18, // the Jester
	6,  // Lightning Wall
	24, // Immolation
	6,  // Warp
	6,  // Reflect
	6,  // Berserk
	12, // Ring of Fire
	0,  // Search (class skill, retired)
	24, // Rune of Fire
	24, // Rune of Light
	24, // Rune of Nova
	24, // Rune of Immolation
	24, // Rune of Stone
	0,  // Charge (earned by level, not read)
	0,  // Zeal
	0,  // Hammer of Faith
	0,  // Blessed Shield
	0,  // Fist of the Heavens
	0,  // Shield Bash
	0,  // Blessed Hammer
};
static_assert(sizeof(SpellBand) / sizeof(SpellBand[0]) == MAX_SPELLS,
    "every SpellID needs a band - this table is indexed by the enum");

} // namespace

int SpellRequiredLevel(SpellID spell)
{
	if (spell == SpellID::Invalid || static_cast<size_t>(spell) >= MAX_SPELLS)
		return 0;
	return SpellBand[static_cast<size_t>(spell)];
}

int SpellRankRequiredLevel(SpellID spell, int rank)
{
	return RankRequiredLevel(SpellRequiredLevel(spell), rank);
}

int SpellBookItemLevel(SpellID spell)
{
	// One row per band. The gaps widen with depth on purpose: the early bands sit inside Normal, where
	// floors are cheap, and the last one lands at the start of Hell, which is where a spell that ends
	// fights should first become findable.
	switch (SpellRequiredLevel(spell)) {
	case 1:
		return 1;
	case 6:
		return 6;
	case 12:
		return 18;
	case 18:
		return 30;
	case 24:
		return 42;
	case 30:
		return 52;
	default:
		return 0; // not a book spell
	}
}

bool CanLearnSpell(const Player &player, SpellID spell)
{
	return player._pLevel >= SpellRequiredLevel(spell);
}

bool CanReadSpellBookTo(const Player &player, SpellID spell, int rank)
{
	return player._pLevel >= SpellRankRequiredLevel(spell, rank);
}

} // namespace devilution::oracool
