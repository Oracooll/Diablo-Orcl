#include "oracool/unique_affixes.h"

#include <algorithm>
#include <cstring>

namespace devilution::oracool {

namespace {
constexpr UniqueAffixFidelity Power = UniqueAffixFidelity::Power;
constexpr UniqueAffixFidelity Approx = UniqueAffixFidelity::Approx;
constexpr UniqueAffixFidelity Inert = UniqueAffixFidelity::Inert;
} // namespace

// ALPHABETICAL, and the test enforces it.
//
// Every row was read against SaveItemPower in items.cpp, not taken from the package's own
// `enginePower` column. The generator reports where the two disagree; there is exactly one
// disagreement and it is flat_armor, below.
const UniqueAffixMapping UniqueAffixMappings[UniqueAffixMappingCount] = {
	// clang-format off
	{ "all_attributes",                        Power,  IPL_ATTRIBS,     nullptr },
	{ "all_resist",                            Power,  IPL_ALLRES,      nullptr },
	{ "attack_rating",                         Power,  IPL_TOHIT,       nullptr },
	{ "attack_speed",                          Approx, IPL_FASTATTACK,  "a discrete tier 1..4, not a percentage; the package's 1..3 is already a tier and passes straight through" },
	{ "aura_strength_percent",                 Inert,  IPL_INVALID,     "auras are a class-tree investment with no wearer multiplier - see oracool/class_tree.h" },
	{ "block_chance_points",                   Inert,  IPL_INVALID,     "block CHANCE is derived from dexterity and class; only block SPEED is an item power" },
	{ "block_speed",                           Power,  IPL_FASTBLOCK,   nullptr },
	{ "class_resource_cost_reduction_percent", Inert,  IPL_INVALID,     "no class resource exists; mana is the only pool and IPL_NOMANA is all-or-nothing" },
	{ "cold_weapon_damage",                    Inert,  IPL_INVALID,     "no cold damage type; the engine has fire, lightning and magic only" },
	{ "cooldown_reduction_percent",            Inert,  IPL_INVALID,     "skills have no cooldowns here - the Paladin actives gate on mana, not time" },
	{ "demon_damage_multiplier",               Approx, IPL_3XDAMVDEM,   "the engine's flag is a fixed TRIPLE against demons; the declared multiplier is ignored" },
	{ "dexterity",                             Power,  IPL_DEX,         nullptr },
	{ "distance_penalty_reduction_percent",    Inert,  IPL_INVALID,     "missile damage does not fall off with distance in this engine" },
	{ "durability_percent",                    Power,  IPL_DUR,         nullptr },
	{ "enhanced_armor_percent",                Power,  IPL_ACP,         nullptr },
	{ "enhanced_damage_percent",               Power,  IPL_DAMP,        nullptr },
	// Not the package's: it has no cast-rate token at all. tools/GenUniqueItems.ps1 authors it onto twenty
	// caster pieces (2026-09-11, user: "add FCR to uniques, sets and runewords too").
	{ "faster_cast_rate_percent",              Power,  IPL_FASTCAST,    nullptr },
	{ "fire_damage",                           Power,  IPL_FIREDAM,     nullptr },
	{ "fire_resist",                           Power,  IPL_FIRERES,     nullptr },
	// The one place the package's own column is wrong, and it is on 109 of the 250 items.
	//
	// unique-items.json declares flat_armor as IPL_SETAC. SaveItemPower does `item._iAC = r` for
	// that power: it OVERWRITES the base's armour. A Gothic Plate rolls 42 armour; "+8 flat armor"
	// applied as declared would leave it at 8. There is no flat-armour ADD in this engine at all, so
	// this rides the percentage channel, which is the same decision item_set_stats.cpp made for
	// `armor_class_flat` and for the same reason.
	{ "flat_armor",                            Approx, IPL_ACP,         "declared IPL_SETAC, which OVERWRITES the base's armour; read as a percentage bonus instead, the only additive armour channel there is" },
	{ "flat_damage",                           Power,  IPL_DAMMOD,      nullptr },
	{ "fury_gain_percent",                     Inert,  IPL_INVALID,     "no fury resource; the Barbarian's tree is investment-based, not resource-based" },
	// Was Inert with the note "wiring one is a small, separate change and would light this up plus
	// the Rat King's Tithe set". That change is done (2026-08-21): IPL_GOLDFIND, Item::_iPLGoldFind,
	// and item format version 8.
	{ "gold_find_percent",                     Power,  IPL_GOLDFIND,    nullptr },
	{ "healing_done_percent",                  Inert,  IPL_INVALID,     "potion and spell healing are fixed amounts; no wearer multiplier exists" },
	{ "hit_recovery",                          Approx, IPL_FASTRECOVER, "a discrete tier 1..3, not a percentage; the package's 1..3 is already a tier" },
	{ "life",                                  Power,  IPL_LIFE,        nullptr },
	{ "life_steal_percent",                    Approx, IPL_STEALLIFE,   "only 3 and 5 exist as flags; any other value silently does NOTHING, so the generator refuses one" },
	{ "light_radius",                          Power,  IPL_LIGHT,       nullptr },
	{ "lightning_damage",                      Power,  IPL_LIGHTDAM,    nullptr },
	{ "lightning_resist",                      Power,  IPL_LIGHTRES,    nullptr },
	{ "magic",                                 Power,  IPL_MAG,         nullptr },
	{ "magic_resist",                          Power,  IPL_MAGICRES,    nullptr },
	{ "mana",                                  Power,  IPL_MANA,        nullptr },
	{ "mana_cost_reduction_percent",           Inert,  IPL_INVALID,     "IPL_NOMANA is all-or-nothing; a percentage discount has no channel" },
	{ "mana_steal_percent",                    Approx, IPL_STEALMANA,   "only 3 and 5 exist as flags; any other value silently does NOTHING" },
	{ "monster_physical_reduction_percent",    Inert,  IPL_INVALID,     "IPL_GETHIT reduces damage taken by a FLAT amount, not a percentage" },
	{ "movement_speed_percent",                Inert,  IPL_INVALID,     "movement speed is a player state (the Vigor aura sets it), not an item power" },
	{ "on_kill_restore_mana",                  Inert,  IPL_INVALID,     "no on-kill hook for the killer's own resources" },
	{ "poison_damage_over_time",               Inert,  IPL_INVALID,     "no damage-over-time state on monsters; the engine's poison is a one-shot missile effect" },
	{ "projectile_pierce_percent",             Inert,  IPL_INVALID,     "arrow piercing is a per-missile behaviour, not a wearer stat" },
	{ "song_duration_percent",                 Inert,  IPL_INVALID,     "the Bard's songs are listed and inert in the class tree; there is no duration to extend yet" },
	{ "spell_damage_percent",                  Inert,  IPL_INVALID,     "spell damage scales on spell level and character level; no flat wearer bonus exists" },
	{ "staff_block_chance_points",             Inert,  IPL_INVALID,     "block CHANCE is derived, and staves do not block at all here" },
	{ "strength",                              Power,  IPL_STR,         nullptr },
	{ "unarmed_damage_percent",                Inert,  IPL_INVALID,     "no unarmed-specific damage channel; IPL_DAMP would boost every attack, not just the empty hand" },
	{ "vitality",                              Power,  IPL_VIT,         nullptr },
	// clang-format on
};

const UniqueAffixMapping *FindUniqueAffix(string_view token)
{
	// Binary search; the table is alphabetical and the test keeps it that way.
	const auto *begin = UniqueAffixMappings;
	const auto *end = begin + UniqueAffixMappingCount;
	const auto *row = std::lower_bound(begin, end, token,
	    [](const UniqueAffixMapping &candidate, string_view needle) {
		    return std::strcmp(candidate.token, std::string(needle).c_str()) < 0;
	    });
	if (row == end || token != row->token)
		return nullptr;
	return row;
}

bool IsUniqueAffixLive(string_view token)
{
	const UniqueAffixMapping *row = FindUniqueAffix(token);
	return row != nullptr && row->fidelity != UniqueAffixFidelity::Inert;
}

} // namespace devilution::oracool
