#include "oracool/item_set_stats.h"

#include <algorithm>
#include <cstring>

namespace devilution::oracool {

namespace {
constexpr SetStatFidelity Power = SetStatFidelity::Power;
constexpr SetStatFidelity Approx = SetStatFidelity::Approx;
constexpr SetStatFidelity Inert = SetStatFidelity::Inert;
} // namespace

// ALPHABETICAL, and the test enforces it - this is a lookup table people will read as a reference,
// and 107 rows in arrival order would be unsearchable by eye.
//
// The IPL semantics below were read out of SaveItemPower (items.cpp), not assumed from the names:
//   IPL_ACP      -> _iPLAC        (armour bonus)      IPL_TARGAC   -> _iPLEnAc (enhanced armour)
//   IPL_DAMP     -> _iPLDam (%)   IPL_DAMMOD -> _iPLDamMod (flat)
//   IPL_GETHIT   -> _iPLGetHit -= r, so a POSITIVE param reduces damage taken
//   IPL_FASTATTACK / IPL_FASTRECOVER take a TIER in param1, not a percentage
const SetStatMapping SetStatMappings[SetStatMappingCount] = {
	// clang-format off
	{ "acid_damage",                              Inert,  IPL_INVALID,     "no acid damage type; the engine has fire, lightning and magic only" },
	{ "active_temper_resistance",                 Inert,  IPL_INVALID,     "needs the Wyrmhide temper state machine" },
	{ "all_spell_levels",                         Power,  IPL_SPLLVLADD,   nullptr },
	{ "armor_class_flat",                         Approx, IPL_ACP,         "IPL_SETAC would OVERWRITE the base item's armour rather than add to it, so this rides the additive channel instead" },
	{ "armor_per_held_petition",                  Inert,  IPL_INVALID,     "needs the Crimson Compact petition counter" },
	{ "attack_speed",                             Approx, IPL_FASTATTACK,  "the engine has four discrete speed tiers, not a percentage; the value picks the nearest tier" },
	{ "aura",                                     Inert,  IPL_INVALID,     "auras are a class-tree investment, not an item property - see oracool/class_tree.h" },
	{ "block",                                    Inert,  IPL_INVALID,     "block CHANCE is derived from dexterity and class; only block SPEED is an item power here" },
	{ "block_speed",                              Power,  IPL_FASTBLOCK,   nullptr },
	{ "boss_mute_duration",                       Inert,  IPL_INVALID,     "needs a monster silence state" },
	{ "bow_attack_speed",                         Approx, IPL_FASTATTACK,  "not bow-specific: the speed tiers apply to every weapon the wearer holds" },
	{ "bow_chance_to_hit",                        Approx, IPL_TOHIT,       "not bow-specific: applies to every attack" },
	{ "bow_damage_flat",                          Approx, IPL_DAMMOD,      "not bow-specific: applies to every attack" },
	{ "bow_distance_penalty",                     Inert,  IPL_INVALID,     "missile damage does not fall off with distance in this engine" },
	{ "chance_to_hit",                            Power,  IPL_TOHIT,       nullptr },
	{ "condemned_duration",                       Inert,  IPL_INVALID,     "needs the Choir of Silence condemned state" },
	{ "condemned_target_direct_damage_per_spent", Inert,  IPL_INVALID,     "needs the Choir of Silence condemned state" },
	{ "confluence_duration",                      Inert,  IPL_INVALID,     "needs the Confluence sequence tracker" },
	{ "confluence_physical_damage",               Inert,  IPL_INVALID,     "needs the Confluence sequence tracker" },
	{ "confluence_sequence",                      Inert,  IPL_INVALID,     "needs the Confluence sequence tracker" },
	{ "confluence_set_elemental_damage",          Inert,  IPL_INVALID,     "needs the Confluence sequence tracker" },
	{ "counterstroke_damage",                     Inert,  IPL_INVALID,     "needs an on-block riposte hook" },
	{ "counterstroke_duration",                   Inert,  IPL_INVALID,     "needs an on-block riposte hook" },
	{ "crimson_compact_stacks",                   Inert,  IPL_INVALID,     "needs the Crimson Compact stack counter" },
	{ "crimson_melee_damage_per_stack",           Inert,  IPL_INVALID,     "needs the Crimson Compact stack counter" },
	{ "crimson_recovery",                         Inert,  IPL_INVALID,     "needs the Crimson Compact stack counter" },
	{ "crowned_resolve",                          Inert,  IPL_INVALID,     "needs Leoric's Fallen Court court-rank state" },
	{ "curse_duration",                           Inert,  IPL_INVALID,     "curses carry no duration here - the same gap Cleansing sits in" },
	{ "damage_taken_flat",                        Power,  IPL_GETHIT,      nullptr },
	{ "damage_vs_demons",                         Approx, IPL_3XDAMVDEM,   "the engine's flag is a fixed TRIPLE against demons; the declared percentage is ignored" },
	{ "damage_vs_undead",                         Inert,  IPL_INVALID,     "IPL_ACUNDEAD is ARMOUR against undead, not damage to them; no damage-vs-undead channel exists" },
	{ "dexterity",                                Power,  IPL_DEX,         nullptr },
	{ "direct_damage_per_blight_stack",           Inert,  IPL_INVALID,     "needs the Black Orchard blight counter" },
	{ "direct_damage_per_petition",               Inert,  IPL_INVALID,     "needs the Crimson Compact petition counter" },
	{ "eligible_vendor_sale_proceeds",            Inert,  IPL_INVALID,     "needs a sale-price hook in the store paths" },
	{ "enhanced_armor",                           Power,  IPL_TARGAC,      nullptr },
	{ "enhanced_bow_damage",                      Approx, IPL_DAMP,        "not bow-specific: applies to every attack" },
	{ "enhanced_damage",                          Power,  IPL_DAMP,        nullptr },
	{ "evasion",                                  Inert,  IPL_INVALID,     "no dodge roll exists; being missed is decided by the attacker's to-hit alone" },
	{ "final_audience_targets",                   Inert,  IPL_INVALID,     "needs Leoric's Fallen Court court-rank state" },
	{ "fire_damage",                              Power,  IPL_FIREDAM,     nullptr },
	{ "flow_required",                            Inert,  IPL_INVALID,     "needs the Steps of the Empty Hand flow counter" },
	{ "gold_from_monsters",                       Inert,  IPL_INVALID,     "Player::_pGoldFind exists (charms feed it) but no IPL_ power writes it; wiring one is a small, separate change" },
	{ "hit_recovery",                             Approx, IPL_FASTRECOVER, "the engine has three discrete recovery tiers, not a percentage; the value picks the nearest tier" },
	{ "hostile_damage_taken_per_greed_rank",      Inert,  IPL_INVALID,     "needs the Rat King's greed rank" },
	{ "hostile_spell_damage_reduction",           Inert,  IPL_INVALID,     "monster spell damage is not separable from monster damage here" },
	{ "hush_credits_required",                    Inert,  IPL_INVALID,     "needs the Choir of Silence hush counter" },
	{ "hush_duration",                            Inert,  IPL_INVALID,     "needs the Choir of Silence hush counter" },
	{ "interrupt",                                Inert,  IPL_INVALID,     "no cast-interrupt state on monsters" },
	{ "knockback",                                Power,  IPL_KNOCKBACK,   nullptr },
	{ "life",                                     Power,  IPL_LIFE,        nullptr },
	{ "light_radius",                             Power,  IPL_LIGHT,       nullptr },
	{ "lightning_damage",                         Power,  IPL_LIGHTDAM,    nullptr },
	{ "magic",                                    Power,  IPL_MAG,         nullptr },
	{ "mana",                                     Power,  IPL_MANA,        nullptr },
	{ "max_life",                                 Approx, IPL_LIFE,        "one channel: this engine's life bonus IS the maximum, so max_life and life are the same stat" },
	{ "max_mana",                                 Approx, IPL_MANA,        "one channel: this engine's mana bonus IS the maximum" },
	{ "max_resist_fire",                          Inert,  IPL_INVALID,     "the 75% resistance cap is a constant, not a per-character value" },
	{ "melee_damage_flat",                        Power,  IPL_DAMMOD,      nullptr },
	{ "memorized_spell_mana_cost",                Inert,  IPL_INVALID,     "IPL_NOMANA is all-or-nothing; a percentage discount has no channel" },
	{ "minimum_petitions",                        Inert,  IPL_INVALID,     "needs the Crimson Compact petition counter" },
	{ "mute_duration",                            Inert,  IPL_INVALID,     "needs a monster silence state" },
	{ "off_hand_focus",                           Inert,  IPL_INVALID,     "no off-hand casting stat exists" },
	{ "ordinary_door_action_range",               Inert,  IPL_INVALID,     "object interaction range is a constant" },
	{ "ordinary_trap_disarm_range",               Inert,  IPL_INVALID,     "object interaction range is a constant" },
	{ "penitent_cooldown_per_trigger",            Inert,  IPL_INVALID,     "needs the Clockwork Penitent engine" },
	{ "penitent_engine_damage",                   Inert,  IPL_INVALID,     "needs the Clockwork Penitent engine" },
	{ "penitent_engine_triggers",                 Inert,  IPL_INVALID,     "needs the Clockwork Penitent engine" },
	{ "petition_cap",                             Inert,  IPL_INVALID,     "needs the Crimson Compact petition counter" },
	{ "petition_gain",                            Inert,  IPL_INVALID,     "needs the Crimson Compact petition counter" },
	{ "potion_healing",                           Inert,  IPL_INVALID,     "potion strength is fixed per potion; no wearer multiplier exists" },
	{ "primary_condemned_duration",               Inert,  IPL_INVALID,     "needs the Choir of Silence condemned state" },
	{ "primary_condemned_kill_refund",            Inert,  IPL_INVALID,     "needs the Choir of Silence condemned state" },
	{ "proc",                                     Inert,  IPL_INVALID,     "named triggered effects; each needs its own implementation, and the name is the whole spec" },
	{ "resist_all",                               Power,  IPL_ALLRES,      nullptr },
	{ "resist_fire",                              Power,  IPL_FIRERES,     nullptr },
	{ "resist_lightning",                         Power,  IPL_LIGHTRES,    nullptr },
	{ "resist_magic",                             Power,  IPL_MAGICRES,    nullptr },
	{ "route_credit_tiles",                       Inert,  IPL_INVALID,     "needs the Lost Cartographer route tracker" },
	{ "secondary_condemned_half_magnitude",       Inert,  IPL_INVALID,     "needs the Choir of Silence condemned state" },
	{ "shed_charge_roots",                        Inert,  IPL_INVALID,     "needs the Wyrmhide shed-charge state" },
	{ "shedstrike",                               Inert,  IPL_INVALID,     "needs the Wyrmhide shed-charge state" },
	{ "shedstrike_duration",                      Inert,  IPL_INVALID,     "needs the Wyrmhide shed-charge state" },
	{ "shield_block",                             Inert,  IPL_INVALID,     "block CHANCE is derived, not an item power" },
	// Corrected 2026-08-16, before it shipped: this was Power/IPL_SPELL on the reasoning that the
	// engine can grant a spell from an item. It can - but every one of the six `skill:` values in the
	// delivered data names a bespoke set ability (pass_judgment, wyrmturn, deploy_penitent_engine,
	// invoke_crimson_compact), and none of them is a SpellID that exists. IPL_SPELL would have had
	// nothing to grant. Found by the generator refusing to pack a non-numeric value, which is the
	// whole reason it refuses rather than defaulting to zero.
	{ "skill",                                    Inert,  IPL_INVALID,     "names an ability invented for the set; IPL_SPELL can only grant a spell the game already has" },
	{ "spell_damage",                             Inert,  IPL_INVALID,     "spell damage scales on spell level and character level; no flat wearer bonus exists" },
	{ "spell_mana_cost",                          Inert,  IPL_INVALID,     "IPL_NOMANA is all-or-nothing; a percentage discount has no channel" },
	{ "spell_reflect",                            Inert,  IPL_INVALID,     "no missile reflection hook - the Reflect spell is a timed player state, not a wearer stat" },
	{ "stance",                                   Inert,  IPL_INVALID,     "needs the Steps of the Empty Hand stance machine" },
	{ "strength",                                 Power,  IPL_STR,         nullptr },
	{ "survey_radius",                            Inert,  IPL_INVALID,     "needs the Lost Cartographer route tracker" },
	{ "temper_carrier",                           Inert,  IPL_INVALID,     "needs the Wyrmhide temper state machine" },
	{ "temper_damage",                            Inert,  IPL_INVALID,     "needs the Wyrmhide temper state machine" },
	{ "thorns",                                   Approx, IPL_THORNS,      "the engine's thorns is a flat return, so a declared range only lights it - the same gap the Thorns aura sits in" },
	{ "to_hit",                                   Power,  IPL_TOHIT,       nullptr },
	{ "trace_route",                              Inert,  IPL_INVALID,     "needs the Lost Cartographer route tracker" },
	{ "trace_route_path_length",                  Inert,  IPL_INVALID,     "needs the Lost Cartographer route tracker" },
	{ "unarmed_attack_speed",                     Approx, IPL_FASTATTACK,  "not unarmed-specific: the speed tiers apply whatever is held" },
	{ "unarmed_damage",                           Approx, IPL_DAMMOD,      "not unarmed-specific: applies to every attack" },
	{ "unarmed_to_hit",                           Approx, IPL_TOHIT,       "not unarmed-specific: applies to every attack" },
	{ "vitality",                                 Power,  IPL_VIT,         nullptr },
	{ "walk_speed",                               Inert,  IPL_INVALID,     "movement speed is a player state (the Vigor aura sets it), not an item power; an IPL_ for it is a separate change" },
	{ "weapon_damage",                            Power,  IPL_DAMMOD,      nullptr },
	{ "weapon_hands",                             Approx, IPL_ONEHAND,     "only the two-handed-becomes-one-handed direction exists; the reverse does not" },
	{ "wearer_direct_damage_taken_by_target_per_spent", Inert, IPL_INVALID, "needs the Crimson Compact petition counter" },
	{ "wyrmturn_cost",                            Inert,  IPL_INVALID,     "needs the Wyrmhide temper state machine" },
	{ "wyrmturn_lockout",                         Inert,  IPL_INVALID,     "needs the Wyrmhide temper state machine" },
	// clang-format on
};

const SetStatMapping *FindSetStat(string_view keyword)
{
	// Binary search: the table is alphabetical and the test keeps it that way.
	const auto *begin = std::begin(SetStatMappings);
	const auto *end = std::end(SetStatMappings);
	const auto *it = std::lower_bound(begin, end, keyword,
	    [](const SetStatMapping &row, string_view key) { return string_view(row.keyword) < key; });
	if (it == end || string_view(it->keyword) != keyword)
		return nullptr;
	return it;
}

bool IsSetStatLive(string_view keyword)
{
	const SetStatMapping *row = FindSetStat(keyword);
	return row != nullptr && row->fidelity != SetStatFidelity::Inert;
}

} // namespace devilution::oracool
