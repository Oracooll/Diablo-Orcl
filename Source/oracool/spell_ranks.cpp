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
	0,  // Ice Bolt - earned on the Sorceress's tree, so the tree's own tier gate is its requirement
	0,  // Ice Blast - the same, and the seven after it
	0,  // Glacial Spike
	0,  // Frost Nova
	0,  // Blizzard
	0,  // Frozen Orb
	0,  // Frozen Armor
	0,  // Shiver Armor
	0,  // Chilling Armor
	0,  // Magic Arrow - the Rogue's bow page, all earned on the tree (Round 3)
	0,  // Fire Arrow
	0,  // Cold Arrow
	0,  // Multiple Shot
	0,  // Exploding Arrow
	0,  // Ice Arrow
	0,  // Guided Arrow
	0,  // Strafe
	0,  // Immolation Arrow
	0,  // Freezing Arrow
	0,  // Bash - the melee skills, Barbarian then Monk, all earned on the tree (Round 4)
	0,  // Leap
	0,  // Double Swing
	0,  // Stun
	0,  // Leap Attack
	0,  // Concentrate
	0,  // Frenzy
	0,  // Whirlwind
	0,  // Berserk
	0,  // Sweeping Reed
	0,  // Breaking Current
	0,  // Vaulting Strike
	0,  // Wheel of Heaven
	0,  // Seven Reeds
	0,  // Open Palm
	0,  // Hundred Fists
	0,  // Radiant Palm
	// The seventeen Round 6 cries, all earned on the tree.
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	// The eight Round 7 javelin rows.
	0, 0, 0, 0, 0, 0, 0, 0,
	// The Paladin's three Round 8 rows.
	0, 0, 0,
	// The three Round 9 corpse cries.
	0, 0, 0,
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
	// floors are cheap, and the last one lands at Hell/Hell, the rung by which everything in the game
	// must be findable (user, 2026-09-12: "hell/hell should be the threshhold for reaching god tier
	// items. everything should be droppable by then").
	//
	// They were 1, 6, 18, 30, 42 and 52, written for the 96-rung ladder, where 52 was the start of Hell
	// difficulty. On the 64-rung ladder 52 sits in TORMENT, which would have put the last books past
	// the threshold; the shape is kept and the span squeezed into 1-45.
	switch (SpellRequiredLevel(spell)) {
	case 1:
		return 1;
	case 6:
		return 5;
	case 12:
		return 13;
	case 18:
		return 22;
	case 24:
		return 33;
	case 30:
		return 45;
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
