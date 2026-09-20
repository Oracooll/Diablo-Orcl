/**
 * @file cursor.cpp
 *
 * Implementation of cursor tracking functionality.
 */
#include "cursor.h"

#include <cstdint>

#include <fmt/format.h>

#include "DiabloUI/diabloui.h"
#include "control.h"
#include "oracool/hud_layout.h"
#include "oracool/inventory_layout.h"
#include "oracool/rift.h"      // RiftTier: the portal's hover line
#include "oracool/stonegate.h" // StonegateEntryTile: where a click on the portal walks to
#include "oracool/item_tint.h"
#include "oracool/shop_grid.h"
#include "controls/plrctrls.h"
#include "doom.h"
#include "engine.h"
#include "engine/backbuffer_state.hpp"
#include "engine/load_cel.hpp"
#include "engine/palette.h"
#include "engine/point.hpp"
#include "engine/render/clx_render.hpp"
#include "engine/render/primitive_render.hpp"
#include "engine/render/text_render.hpp"
#include "engine/trn.hpp"
#include "hwcursor.hpp"
#include "inv.h"
#include "panels/spell_book.hpp" // GetSpellBookPanelRect
#include "levels/trigs.h"
#include "missiles.h"
#include "options.h"
#include "qol/itemlabels.h"
#include "qol/stash.h"
#include "towners.h"
#include "track.h"
#include "utils/attributes.h"
#include "utils/language.h"
#include "utils/str_cat.hpp"
#include "utils/sdl_bilinear_scale.hpp"
#include "utils/surface_to_clx.hpp"
#include "utils/utf8.hpp"

namespace devilution {
namespace {
/** Cursor images CEL */
OptionalOwnedClxSpriteList pCursCels;
OptionalOwnedClxSpriteList pCursCels2;
/** Oracool: our own icon sheet - see InvItemWidth3. */
OptionalOwnedClxSpriteList pCursCels3;

/** Maps from objcurs.cel frame number to frame width. */
const uint16_t InvItemWidth1[] = {
	// clang-format off
	// Cursors
	33, 32, 32, 32, 32, 32, 32, 32, 32, 32, 23,
	// Items
	1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28,
	1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28,
	1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28,
	1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28,
	1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28,
	1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28,
	1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28,
	1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28,
	2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28,
	2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28,
	2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28,
	2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28,
	2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28,
	2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28,
	2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28,
	2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28,
	2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28,
};
const uint16_t InvItemWidth2[] = {
	1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28,
	1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28,
	1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28,
	2 * 28, 2 * 28, 1 * 28, 1 * 28, 1 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28,
	2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28,
	2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28,
	2 * 28
	// clang-format on
};
constexpr uint16_t InvItems1Size = sizeof(InvItemWidth1) / sizeof(InvItemWidth1[0]);
constexpr uint16_t InvItems2Size = sizeof(InvItemWidth2) / sizeof(InvItemWidth2[0]);

/**
 * Oracool: a third icon sheet - the six worn item types the original game has no art for, plus
 * every other custom item icon added since: a standalone ILOC_HELM item, leather-tier armor and
 * shield, and an eight-tier progression (Iron through Diamond) across all nine worn/armor/shield
 * slots, 80 frames in total.
 *
 * Ships in oracool.mpq as data\inv\oracool_items.cel and is loaded unconditionally - unlike
 * objcurs2, which is Hellfire-only, these items exist in both games. Its ids continue straight on
 * from sheet 2's, whether or not sheet 2 was actually loaded: the id space is fixed at compile
 * time, so a Diablo game and a Hellfire game agree on which id means which icon.
 *
 * Frame order must match the ICURS_ORACOOL_* values in itemdat.h.
 */
const uint16_t InvItemWidth3[] = {
	2 * 28, // shoulders
	2 * 28, // bracers
	2 * 28, // gloves
	2 * 28, // belt
	2 * 28, // legs
	2 * 28, // boots
	2 * 28, // helm
	2 * 28, // leather_armor
	2 * 28, // leather_shield
	2 * 28, // iron_gloves
	2 * 28, // iron_shoulders
	2 * 28, // iron_bracers
	2 * 28, // iron_belt
	2 * 28, // iron_legs
	2 * 28, // iron_boots
	2 * 28, // iron_armor
	2 * 28, // iron_shield
	2 * 28, // steel_gloves
	2 * 28, // steel_shoulders
	2 * 28, // steel_bracers
	2 * 28, // steel_belt
	2 * 28, // steel_legs
	2 * 28, // steel_boots
	2 * 28, // steel_armor
	2 * 28, // steel_shield
	2 * 28, // steel_helm
	2 * 28, // crusader_gloves
	2 * 28, // crusader_shoulders
	2 * 28, // crusader_bracers
	2 * 28, // crusader_belt
	2 * 28, // crusader_legs
	2 * 28, // crusader_boots
	2 * 28, // crusader_armor
	2 * 28, // crusader_shield
	2 * 28, // crusader_helm
	2 * 28, // bone_gloves
	2 * 28, // bone_shoulders
	2 * 28, // bone_bracers
	2 * 28, // bone_belt
	2 * 28, // bone_legs
	2 * 28, // bone_boots
	2 * 28, // bone_armor
	2 * 28, // bone_shield
	2 * 28, // bone_helm
	2 * 28, // royal_gloves
	2 * 28, // royal_shoulders
	2 * 28, // royal_bracers
	2 * 28, // royal_belt
	2 * 28, // royal_legs
	2 * 28, // royal_boots
	2 * 28, // royal_armor
	2 * 28, // royal_shield
	2 * 28, // royal_helm
	2 * 28, // obsidian_gloves
	2 * 28, // obsidian_shoulders
	2 * 28, // obsidian_bracers
	2 * 28, // obsidian_belt
	2 * 28, // obsidian_legs
	2 * 28, // obsidian_boots
	2 * 28, // obsidian_armor
	2 * 28, // obsidian_shield
	2 * 28, // obsidian_helm
	2 * 28, // infernal_gloves
	2 * 28, // infernal_shoulders
	2 * 28, // infernal_bracers
	2 * 28, // infernal_belt
	2 * 28, // infernal_legs
	2 * 28, // infernal_boots
	2 * 28, // infernal_armor
	2 * 28, // infernal_shield
	2 * 28, // infernal_helm
	2 * 28, // diamond_gloves
	2 * 28, // diamond_shoulders
	2 * 28, // diamond_bracers
	2 * 28, // diamond_belt
	2 * 28, // diamond_legs
	2 * 28, // diamond_boots
	2 * 28, // diamond_armor
	2 * 28, // diamond_shield
	2 * 28, // diamond_helm
	2 * 28, // ruby_gloves
	2 * 28, // ruby_shoulders
	2 * 28, // ruby_bracers
	2 * 28, // ruby_belt
	2 * 28, // ruby_legs
	2 * 28, // ruby_boots
	2 * 28, // ruby_armor
	2 * 28, // ruby_shield
	2 * 28, // ruby_helm
	2 * 28, // onyx_gloves
	2 * 28, // onyx_shoulders
	2 * 28, // onyx_bracers
	2 * 28, // onyx_belt
	2 * 28, // onyx_legs
	2 * 28, // onyx_boots
	2 * 28, // onyx_armor
	2 * 28, // onyx_shield
	2 * 28, // onyx_helm
	2 * 28, // glacial_gloves
	2 * 28, // glacial_shoulders
	2 * 28, // glacial_bracers
	2 * 28, // glacial_belt
	2 * 28, // glacial_legs
	2 * 28, // glacial_boots
	2 * 28, // glacial_armor
	2 * 28, // glacial_shield
	2 * 28, // glacial_helm
	2 * 28, // cyborg_gloves
	2 * 28, // cyborg_shoulders
	2 * 28, // cyborg_bracers
	2 * 28, // cyborg_belt
	2 * 28, // cyborg_legs
	2 * 28, // cyborg_boots
	2 * 28, // cyborg_armor
	2 * 28, // cyborg_shield
	2 * 28, // cyborg_helm
	2 * 28, // fallen_gloves
	2 * 28, // fallen_shoulders
	2 * 28, // fallen_bracers
	2 * 28, // fallen_belt
	2 * 28, // fallen_legs
	2 * 28, // fallen_boots
	2 * 28, // fallen_armor
	2 * 28, // fallen_shield
	2 * 28, // fallen_helm
	2 * 28, // seraphic_gloves
	2 * 28, // seraphic_shoulders
	2 * 28, // seraphic_bracers
	2 * 28, // seraphic_belt
	2 * 28, // seraphic_legs
	2 * 28, // seraphic_boots
	2 * 28, // seraphic_armor
	2 * 28, // seraphic_shield
	2 * 28, // seraphic_helm
	2 * 28, // spectral_gloves
	2 * 28, // spectral_shoulders
	2 * 28, // spectral_bracers
	2 * 28, // spectral_belt
	2 * 28, // spectral_legs
	2 * 28, // spectral_boots
	2 * 28, // spectral_armor
	2 * 28, // spectral_shield
	2 * 28, // spectral_helm
	1 * 28, // gem_ruby
	1 * 28, // gem_sapphire
	1 * 28, // gem_topaz
	1 * 28, // gem_emerald
	1 * 28, // gem_skull
	1 * 28, // rune_el
	1 * 28, // rune_tir
	1 * 28, // rune_ral
	1 * 28, // rune_ort
	1 * 28, // rune_sol
	1 * 28, // gem_amethyst_chipped
	1 * 28, // gem_amethyst_flawed
	1 * 28, // gem_amethyst_normal
	1 * 28, // gem_amethyst_flawless
	1 * 28, // gem_amethyst_perfect
	1 * 28, // gem_diamond_chipped
	1 * 28, // gem_diamond_flawed
	1 * 28, // gem_diamond_normal
	1 * 28, // gem_diamond_flawless
	1 * 28, // gem_diamond_perfect
	1 * 28, // gem_emerald_chipped
	1 * 28, // gem_emerald_flawed
	1 * 28, // gem_emerald_flawless
	1 * 28, // gem_emerald_perfect
	1 * 28, // gem_ruby_chipped
	1 * 28, // gem_ruby_flawed
	1 * 28, // gem_ruby_flawless
	1 * 28, // gem_ruby_perfect
	1 * 28, // gem_sapphire_chipped
	1 * 28, // gem_sapphire_flawed
	1 * 28, // gem_sapphire_flawless
	1 * 28, // gem_sapphire_perfect
	1 * 28, // gem_topaz_chipped
	1 * 28, // gem_topaz_flawed
	1 * 28, // gem_topaz_flawless
	1 * 28, // gem_topaz_perfect
	1 * 28, // gem_skull_chipped
	1 * 28, // gem_skull_flawed
	1 * 28, // gem_skull_flawless
	1 * 28, // gem_skull_perfect
// The item sets' 94 frames - widths. GENERATED with the ICURS_ORACOOL_SET_* ids and the CEL's own
// frame order; see tools/GenItemSets.ps1.
#include "oracool/item_sets_curs_widths.inc"
// The expansion uniques' 143 frames - widths. GENERATED; see tools/GenUniqueItems.ps1. The width
// here IS the item's inventory footprint: IPL_INVCURS points _iCurs at the frame, and everything
// that sizes an item reads these tables through _iCurs.
#include "oracool/unique_items_curs_widths.inc"
// Sockets v2: the 28 new rune frames. GENERATED by tools/GenRunes.ps1; every one is 1x1.
#include "oracool/runes_curs_widths.inc"
#include "oracool/salvage_curs_widths.inc"
// The fifteen jewels, after the salvage block. GENERATED by tools/GenJewels.ps1; every one is 1x1.
#include "oracool/jewels_curs_widths.inc"
#include "oracool/shards_curs_widths.inc"
#include "oracool/signets_curs_widths.inc"
#include "oracool/growing_charms_curs_widths.inc"
#include "oracool/encounter_items_curs_widths.inc"
#include "oracool/charm_icons_curs_widths.inc"
#include "oracool/unique_items2_curs_widths.inc"
#include "oracool/unqbase_icons_curs_widths.inc"
// The sixteen new Imbuement Shard frames, the sheet's last run (2026-09-19). GENERATED by tools/GenImbuementShards.ps1.
#include "oracool/shards_curs_widths_late.inc"
	28, // ICURS_ORACOOL_KEYSTONE (the rifts, 2026-09-20): one hand-cut frame after the late shards
};
const uint16_t InvItemHeight3[] = {
	2 * 28, // shoulders
	2 * 28, // bracers
	2 * 28, // gloves
	1 * 28, // belt - the one 2x1 among them
	2 * 28, // legs
	2 * 28, // boots
	2 * 28, // helm
	2 * 28, // leather_armor
	2 * 28, // leather_shield
	2 * 28, // iron_gloves
	2 * 28, // iron_shoulders
	2 * 28, // iron_bracers
	1 * 28, // iron_belt - 2x1
	2 * 28, // iron_legs
	2 * 28, // iron_boots
	2 * 28, // iron_armor
	2 * 28, // iron_shield
	2 * 28, // steel_gloves
	2 * 28, // steel_shoulders
	2 * 28, // steel_bracers
	1 * 28, // steel_belt - 2x1
	2 * 28, // steel_legs
	2 * 28, // steel_boots
	2 * 28, // steel_armor
	2 * 28, // steel_shield
	2 * 28, // steel_helm
	2 * 28, // crusader_gloves
	2 * 28, // crusader_shoulders
	2 * 28, // crusader_bracers
	1 * 28, // crusader_belt - 2x1
	2 * 28, // crusader_legs
	2 * 28, // crusader_boots
	2 * 28, // crusader_armor
	2 * 28, // crusader_shield
	2 * 28, // crusader_helm
	2 * 28, // bone_gloves
	2 * 28, // bone_shoulders
	2 * 28, // bone_bracers
	1 * 28, // bone_belt - 2x1
	2 * 28, // bone_legs
	2 * 28, // bone_boots
	2 * 28, // bone_armor
	2 * 28, // bone_shield
	2 * 28, // bone_helm
	2 * 28, // royal_gloves
	2 * 28, // royal_shoulders
	2 * 28, // royal_bracers
	1 * 28, // royal_belt - 2x1
	2 * 28, // royal_legs
	2 * 28, // royal_boots
	2 * 28, // royal_armor
	2 * 28, // royal_shield
	2 * 28, // royal_helm
	2 * 28, // obsidian_gloves
	2 * 28, // obsidian_shoulders
	2 * 28, // obsidian_bracers
	1 * 28, // obsidian_belt - 2x1
	2 * 28, // obsidian_legs
	2 * 28, // obsidian_boots
	2 * 28, // obsidian_armor
	2 * 28, // obsidian_shield
	2 * 28, // obsidian_helm
	2 * 28, // infernal_gloves
	2 * 28, // infernal_shoulders
	2 * 28, // infernal_bracers
	1 * 28, // infernal_belt - 2x1
	2 * 28, // infernal_legs
	2 * 28, // infernal_boots
	2 * 28, // infernal_armor
	2 * 28, // infernal_shield
	2 * 28, // infernal_helm
	2 * 28, // diamond_gloves
	2 * 28, // diamond_shoulders
	2 * 28, // diamond_bracers
	1 * 28, // diamond_belt - 2x1
	2 * 28, // diamond_legs
	2 * 28, // diamond_boots
	2 * 28, // diamond_armor
	2 * 28, // diamond_shield
	2 * 28, // diamond_helm
	2 * 28, // ruby_gloves
	2 * 28, // ruby_shoulders
	2 * 28, // ruby_bracers
	1 * 28, // ruby_belt - 2x1
	2 * 28, // ruby_legs
	2 * 28, // ruby_boots
	2 * 28, // ruby_armor
	2 * 28, // ruby_shield
	2 * 28, // ruby_helm
	2 * 28, // onyx_gloves
	2 * 28, // onyx_shoulders
	2 * 28, // onyx_bracers
	1 * 28, // onyx_belt - 2x1
	2 * 28, // onyx_legs
	2 * 28, // onyx_boots
	2 * 28, // onyx_armor
	2 * 28, // onyx_shield
	2 * 28, // onyx_helm
	2 * 28, // glacial_gloves
	2 * 28, // glacial_shoulders
	2 * 28, // glacial_bracers
	1 * 28, // glacial_belt - 2x1
	2 * 28, // glacial_legs
	2 * 28, // glacial_boots
	2 * 28, // glacial_armor
	2 * 28, // glacial_shield
	2 * 28, // glacial_helm
	2 * 28, // cyborg_gloves
	2 * 28, // cyborg_shoulders
	2 * 28, // cyborg_bracers
	1 * 28, // cyborg_belt - 2x1
	2 * 28, // cyborg_legs
	2 * 28, // cyborg_boots
	2 * 28, // cyborg_armor
	2 * 28, // cyborg_shield
	2 * 28, // cyborg_helm
	2 * 28, // fallen_gloves
	2 * 28, // fallen_shoulders
	2 * 28, // fallen_bracers
	1 * 28, // fallen_belt - 2x1
	2 * 28, // fallen_legs
	2 * 28, // fallen_boots
	2 * 28, // fallen_armor
	2 * 28, // fallen_shield
	2 * 28, // fallen_helm
	2 * 28, // seraphic_gloves
	2 * 28, // seraphic_shoulders
	2 * 28, // seraphic_bracers
	1 * 28, // seraphic_belt - 2x1
	2 * 28, // seraphic_legs
	2 * 28, // seraphic_boots
	2 * 28, // seraphic_armor
	2 * 28, // seraphic_shield
	2 * 28, // seraphic_helm
	2 * 28, // spectral_gloves
	2 * 28, // spectral_shoulders
	2 * 28, // spectral_bracers
	1 * 28, // spectral_belt - 2x1
	2 * 28, // spectral_legs
	2 * 28, // spectral_boots
	2 * 28, // spectral_armor
	2 * 28, // spectral_shield
	2 * 28, // spectral_helm
	1 * 28, // gem_ruby
	1 * 28, // gem_sapphire
	1 * 28, // gem_topaz
	1 * 28, // gem_emerald
	1 * 28, // gem_skull
	1 * 28, // rune_el
	1 * 28, // rune_tir
	1 * 28, // rune_ral
	1 * 28, // rune_ort
	1 * 28, // rune_sol
	1 * 28, // gem_amethyst_chipped
	1 * 28, // gem_amethyst_flawed
	1 * 28, // gem_amethyst_normal
	1 * 28, // gem_amethyst_flawless
	1 * 28, // gem_amethyst_perfect
	1 * 28, // gem_diamond_chipped
	1 * 28, // gem_diamond_flawed
	1 * 28, // gem_diamond_normal
	1 * 28, // gem_diamond_flawless
	1 * 28, // gem_diamond_perfect
	1 * 28, // gem_emerald_chipped
	1 * 28, // gem_emerald_flawed
	1 * 28, // gem_emerald_flawless
	1 * 28, // gem_emerald_perfect
	1 * 28, // gem_ruby_chipped
	1 * 28, // gem_ruby_flawed
	1 * 28, // gem_ruby_flawless
	1 * 28, // gem_ruby_perfect
	1 * 28, // gem_sapphire_chipped
	1 * 28, // gem_sapphire_flawed
	1 * 28, // gem_sapphire_flawless
	1 * 28, // gem_sapphire_perfect
	1 * 28, // gem_topaz_chipped
	1 * 28, // gem_topaz_flawed
	1 * 28, // gem_topaz_flawless
	1 * 28, // gem_topaz_perfect
	1 * 28, // gem_skull_chipped
	1 * 28, // gem_skull_flawed
	1 * 28, // gem_skull_flawless
	1 * 28, // gem_skull_perfect
// The item sets' 94 frames. GENERATED with the ICURS_ORACOOL_SET_* ids and the CEL's frame order -
// see tools/GenItemSets.ps1. The static_asserts below are what catch the three drifting apart.
#include "oracool/item_sets_curs_heights.inc"
// The expansion uniques' 143 frames. GENERATED; see tools/GenUniqueItems.ps1.
#include "oracool/unique_items_curs_heights.inc"
// Sockets v2: the 28 new rune frames. GENERATED by tools/GenRunes.ps1.
#include "oracool/runes_curs_heights.inc"
#include "oracool/salvage_curs_heights.inc"
#include "oracool/jewels_curs_heights.inc"
#include "oracool/shards_curs_heights.inc"
#include "oracool/signets_curs_heights.inc"
#include "oracool/growing_charms_curs_heights.inc"
#include "oracool/encounter_items_curs_heights.inc"
#include "oracool/charm_icons_curs_heights.inc"
#include "oracool/unique_items2_curs_heights.inc"
#include "oracool/unqbase_icons_curs_heights.inc"
#include "oracool/shards_curs_heights_late.inc"
	28, // ICURS_ORACOOL_KEYSTONE
};

// The uniques' frames must start exactly one past the sets' - the CEL is built by concatenating
// the two spec lists in this order, and a gap or overlap here means every unique icon is off by
// the difference while nothing else complains.
static_assert(ICURS_ORACOOL_UNQ_FIRST == ICURS_ORACOOL_SET_COURT_RELIQUARY + 1,
    "the unique icons no longer start where the set icons end - FirstCursorId in GenUniqueItems.ps1 "
    "must be the set run's end + 1");
constexpr uint16_t InvItems3Size = sizeof(InvItemWidth3) / sizeof(InvItemWidth3[0]);
static_assert(sizeof(InvItemHeight3) / sizeof(InvItemHeight3[0]) == InvItems3Size,
    "oracool icon sheet width and height tables disagree on the frame count");

// itemdat.h hardcodes where our ids start, because that is how every other ICURS_ value is
// written. This is the check that the hardcoded number is still right: it depends on both vanilla
// sheet sizes and on CURSOR_FIRSTITEM, none of which are visible from itemdat.h.
static_assert(ICURS_ORACOOL_FIRST == InvItems1Size + InvItems2Size + 1 - static_cast<int>(CURSOR_FIRSTITEM),
    "ICURS_ORACOOL_FIRST no longer matches where the third icon sheet actually begins");
static_assert(ICURS_ORACOOL_LAST - ICURS_ORACOOL_FIRST + 1 == InvItems3Size,
    "the ICURS_ORACOOL_* range and the third icon sheet disagree on the frame count");

/** Maps from objcurs.cel frame number to frame height. */
const uint16_t InvItemHeight1[InvItems1Size] = {
	// clang-format off
	// Cursors
	29, 32, 32, 32, 32, 32, 32, 32, 32, 32, 35,
	// Items
	1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28,
	1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28,
	1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28,
	1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28,
	1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28,
	2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28,
	3 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28,
	3 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28,
	2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28,
	2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28, 2 * 28,
	3 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28,
	3 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28,
	3 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28,
	3 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28,
	3 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28,
	3 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28,
	3 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28,
};
const uint16_t InvItemHeight2[InvItems2Size] = {
	1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28,
	1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28,
	1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28, 1 * 28,
	2 * 28, 2 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28,
	3 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28,
	3 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28, 3 * 28,
	3 * 28
	// clang-format on
};

OptionalOwnedClxSpriteList *HalfSizeItemSprites;
OptionalOwnedClxSpriteList *HalfSizeItemSpritesRed;

} // namespace

/** Current highlighted monster */
int pcursmonst = -1;

/** inv_item value */
int8_t pcursinvitem;
/** Oracool Tabbed Inventory: extra-tab equivalent of pcursinvitem - see cursor.h */
int8_t pcursinvtabidx = -1;
int8_t pcursinvtabitem = -1;
/** StashItem value */
uint16_t pcursstashitem;
/** Current highlighted item */
int8_t pcursitem;
/** Current highlighted object */
Object *ObjectUnderCursor;
/** Current highlighted player */
int8_t pcursplr;
/** Current highlighted tile position */
Point cursPosition;
/** Previously highlighted monster */
int pcurstemp;
/** Index of current cursor image */
int pcurs;

void InitCursor()
{
	assert(!pCursCels);
	pCursCels = LoadCel("data\\inv\\objcurs", InvItemWidth1);
	if (gbIsHellfire)
		pCursCels2 = LoadCel("data\\inv\\objcurs2", InvItemWidth2);
	// Oracool: unconditional - the six worn types exist in Diablo as well as Hellfire.
	pCursCels3 = LoadCel("data\\inv\\oracool_items", InvItemWidth3);
	ClearCursor();
}

void FreeCursor()
{
	pCursCels = std::nullopt;
	pCursCels2 = std::nullopt;
	pCursCels3 = std::nullopt;
	ClearCursor();
}

ClxSprite GetInvItemSprite(int cursId)
{
	if (cursId <= InvItems1Size)
		return (*pCursCels)[cursId - 1];
	if (cursId <= InvItems1Size + InvItems2Size)
		return (*pCursCels2)[cursId - InvItems1Size - 1];
	return (*pCursCels3)[cursId - InvItems1Size - InvItems2Size - 1];
}

size_t GetNumInvItems()
{
	return InvItems1Size + InvItems2Size + InvItems3Size;
}

size_t GetNumInvItemsInSheet(int sheet)
{
	return sheet == 1 ? InvItems1Size : sheet == 2 ? InvItems2Size : InvItems3Size;
}

Size GetInvItemSize(int cursId)
{
	const int i = cursId - 1;
	if (i >= InvItems1Size + InvItems2Size)
		return { InvItemWidth3[i - InvItems1Size - InvItems2Size], InvItemHeight3[i - InvItems1Size - InvItems2Size] };
	if (i >= InvItems1Size)
		return { InvItemWidth2[i - InvItems1Size], InvItemHeight2[i - InvItems1Size] };
	return { InvItemWidth1[i], InvItemHeight1[i] };
}

ClxSprite GetHalfSizeItemSprite(int cursId)
{
	return (*HalfSizeItemSprites[cursId])[0];
}

ClxSprite GetHalfSizeItemSpriteRed(int cursId)
{
	return (*HalfSizeItemSpritesRed[cursId])[0];
}

void CreateHalfSizeItemSprites()
{
	if (HalfSizeItemSprites != nullptr)
		return;
	// Oracool: sized for all three sheets in both games, and indexed by _iCurs directly.
	//
	// This used to size itself by whether Hellfire's sheet 2 was loaded. That worked only because
	// sheet 2's ids are never used in a Diablo game, so the tail of the array was simply unreached.
	// Sheet 3's ids ARE used in both games and sit past sheet 2's block, so the index space has to
	// exist whether or not sheet 2's content does - see the skipped-but-counted loop below.
	const int numInvItems = InvItems1Size + InvItems2Size + InvItems3Size - (static_cast<size_t>(CURSOR_FIRSTITEM) - 1);
	HalfSizeItemSprites = new OptionalOwnedClxSpriteList[numInvItems];
	HalfSizeItemSpritesRed = new OptionalOwnedClxSpriteList[numInvItems];
	const uint8_t *redTrn = GetInfravisionTRN();

	constexpr int MaxWidth = 28 * 3;
	constexpr int MaxHeight = 28 * 3;
	OwnedSurface ownedItemSurface { MaxWidth, MaxHeight };
	OwnedSurface ownedHalfSurface { MaxWidth / 2, MaxHeight / 2 };

	const auto createHalfSize = [&, redTrn](const ClxSprite itemSprite, size_t outputIndex) {
		if (itemSprite.width() <= 28 && itemSprite.height() <= 28) {
			// Skip creating half-size sprites for 1x1 items because we always render them at full size anyway.
			return;
		}
		const Surface itemSurface = ownedItemSurface.subregion(0, 0, itemSprite.width(), itemSprite.height());
		SDL_Rect itemSurfaceRect = MakeSdlRect(0, 0, itemSurface.w(), itemSurface.h());
		SDL_SetClipRect(itemSurface.surface, &itemSurfaceRect);
		SDL_FillRect(itemSurface.surface, nullptr, 1);
		ClxDraw(itemSurface, { 0, itemSurface.h() }, itemSprite);

		const Surface halfSurface = ownedHalfSurface.subregion(0, 0, itemSurface.w() / 2, itemSurface.h() / 2);
		SDL_Rect halfSurfaceRect = MakeSdlRect(0, 0, halfSurface.w(), halfSurface.h());
		SDL_SetClipRect(halfSurface.surface, &halfSurfaceRect);
		BilinearDownscaleByHalf8(itemSurface.surface, paletteTransparencyLookup, halfSurface.surface, 1);
		HalfSizeItemSprites[outputIndex].emplace(SurfaceToClx(halfSurface, 1, 1));

		SDL_FillRect(itemSurface.surface, nullptr, 1);
		ClxDrawTRN(itemSurface, { 0, itemSurface.h() }, itemSprite, redTrn);
		BilinearDownscaleByHalf8(itemSurface.surface, paletteTransparencyLookup, halfSurface.surface, 1);
		HalfSizeItemSpritesRed[outputIndex].emplace(SurfaceToClx(halfSurface, 1, 1));
	};

	size_t outputIndex = 0;
	for (size_t i = static_cast<int>(CURSOR_FIRSTITEM) - 1; i < InvItems1Size; ++i, ++outputIndex) {
		createHalfSize((*pCursCels)[i], outputIndex);
	}
	// Sheet 2's slots are counted even in a Diablo game, where its content does not exist - that is
	// what keeps sheet 3's indices in the same place in both games.
	for (size_t i = 0; i < InvItems2Size; ++i, ++outputIndex) {
		if (gbIsHellfire)
			createHalfSize((*pCursCels2)[i], outputIndex);
	}
	for (size_t i = 0; i < InvItems3Size; ++i, ++outputIndex) {
		createHalfSize((*pCursCels3)[i], outputIndex);
	}
}

void FreeHalfSizeItemSprites()
{
	if (HalfSizeItemSprites != nullptr) {
		delete[] HalfSizeItemSprites;
		HalfSizeItemSprites = nullptr;
		delete[] HalfSizeItemSpritesRed;
		HalfSizeItemSpritesRed = nullptr;
	}
}

void DrawBrokenItemMarker(const Surface &out, Point topLeft, int width, int height)
{
	constexpr int Inset = 3;
	constexpr int Thickness = 2;
	constexpr uint8_t BrokenItemXColor = PAL8_RED;
	const Point a1 { topLeft.x + Inset, topLeft.y + Inset };
	const Point a2 { topLeft.x + width - Inset, topLeft.y + height - Inset };
	const Point b1 { topLeft.x + width - Inset, topLeft.y + Inset };
	const Point b2 { topLeft.x + Inset, topLeft.y + height - Inset };
	const int steps = std::max({ std::abs(a2.x - a1.x), std::abs(a2.y - a1.y), 1 });
	for (int i = 0; i <= steps; i++) {
		const int x = a1.x + (a2.x - a1.x) * i / steps;
		const int y = a1.y + (a2.y - a1.y) * i / steps;
		FillRect(out, x - Thickness / 2, y - Thickness / 2, Thickness, Thickness, BrokenItemXColor);
		const int x2 = b1.x + (b2.x - b1.x) * i / steps;
		const int y2 = b1.y + (b2.y - b1.y) * i / steps;
		FillRect(out, x2 - Thickness / 2, y2 - Thickness / 2, Thickness, Thickness, BrokenItemXColor);
	}
}

void DrawItem(const Item &item, const Surface &out, Point position, ClxSprite clx)
{
	const bool usable = !IsInspectingPlayer() ? item._iStatFlag : InspectPlayer->CanUseItem(item);
	if (usable) {
		// Oracool: each of the ten oils in its own colour - the shared flask through a palette
		// translation (2026-09-05).
		if (const uint8_t *tint = oracool::ItemTRN(item); tint != nullptr)
			ClxDrawTRN(out, position, clx, tint);
		else
			ClxDraw(out, position, clx);
	} else {
		ClxDrawTRN(out, position, clx, GetInfravisionTRN());
	}

	// Oracool: a broken (0 durability) item already renders grayscale via the branch above -
	// stamp a red X over its icon too, wherever it's drawn (inventory, belt, equipped slots all
	// share this one function), as a clearer at-a-glance signal than the grayscale alone.
	if (item._iOracoolBroken) {
		const int width = static_cast<int>(clx.width());
		const int height = static_cast<int>(clx.height());
		DrawBrokenItemMarker(out, { position.x, position.y - height }, width, height);
	}

	// Stack quantity, bottom-right of the icon. Deliberately not the top of the icon,
	// which DrawInvBelt already uses for the belt hotkey number (1-8).
	if (item.isStackableConsumable() && item.stackCount() > 1) {
		DrawString(out, StrCat(item.stackCount()),
		    { position - Displacement { 0, 11 }, { static_cast<int>(clx.width()), 12 } },
		    { UiFlags::ColorWhite | UiFlags::AlignRight });
	}
}

void ResetCursor()
{
	NewCursor(pcurs);
}

void NewCursor(const Item &item)
{
	if (item.isEmpty()) {
		NewCursor(CURSOR_HAND);
	} else {
		NewCursor(item._iCurs + CURSOR_FIRSTITEM);
	}
}

void NewCursor(int cursId)
{
	if (pcurs >= CURSOR_FIRSTITEM && cursId > CURSOR_HAND && cursId < CURSOR_HOURGLASS) {
		if (!TryDropItem()) {
			return;
		}
	}

	if (cursId < CURSOR_HOURGLASS && MyPlayer != nullptr) {
		MyPlayer->HoldItem.clear();
	}
	pcurs = cursId;

	if (IsHardwareCursorEnabled() && ControlDevice == ControlTypes::KeyboardAndMouse) {
		if (!ArtCursor && cursId == CURSOR_NONE)
			return;

		const CursorInfo newCursor = ArtCursor
		    ? CursorInfo::UserInterfaceCursor()
		    : CursorInfo::GameCursor(cursId);
		if (newCursor != GetCurrentCursorInfo())
			SetHardwareCursor(newCursor);
	}
}

void DrawSoftwareCursor(const Surface &out, Point position, int cursId)
{
	const ClxSprite sprite = GetInvItemSprite(cursId);
	if (!MyPlayer->HoldItem.isEmpty()) {
		const auto &heldItem = MyPlayer->HoldItem;
		ClxDrawOutline(out, GetOutlineColor(heldItem, true), position, sprite);
		DrawItem(heldItem, out, position, sprite);
	} else {
		ClxDraw(out, position, sprite);
	}
}

void InitLevelCursor()
{
	NewCursor(CURSOR_HAND);
	cursPosition = ViewPosition;
	pcurstemp = -1;
	pcursmonst = -1;
	ObjectUnderCursor = nullptr;
	pcursitem = -1;
	pcursstashitem = StashStruct::EmptyCell;
	pcursplr = -1;
	ClearCursor();
}

void CheckTown()
{
	for (auto &missile : Missiles) {
		if (missile._mitype == MissileID::TownPortal) {
			if (EntranceBoundaryContains(missile.position.tile, cursPosition)) {
				trigflag = true;
				SetPanelString(_("Town Portal"), UiFlags::ColorWhite);
				AddPanelString(fmt::format(fmt::runtime(_("from {:s}")), Players[missile._misource]._pName));
				cursPosition = missile.position.tile;
			}
		}
	}
}

/**
 * @brief Oracool: the rift portal in the Stonegate is a door the cursor can find (user, 2026-09-20:
 * "clicking on the gold/purple portals when they appear should be possible ... it should cover every
 * pixel of their asset"). The same seven-tile entrance boundary the town portal answers to - the
 * portal's tile and the tiles its 96x128 sprite rises over - and the click walks the hero onto the
 * entry tile in front of the gate, where TryEnterRiftFromTown takes over. The gate's own tile is the
 * object's (it is picked first), so the ring's base still opens the gate's menu.
 */
void CheckRiftPortal()
{
	if (leveltype != DTYPE_TOWN)
		return;
	for (auto &missile : Missiles) {
		if (missile._mitype != MissileID::RiftPortalGold && missile._mitype != MissileID::RiftPortalPurple)
			continue;
		if (!EntranceBoundaryContains(missile.position.tile, cursPosition))
			continue;
		Point entry;
		if (!oracool::StonegateEntryTile(entry))
			continue;
		trigflag = true;
		SetPanelString(missile._mitype == MissileID::RiftPortalGold ? _("Nephalem Rift") : _("Guardian Rift"), UiFlags::ColorWhite);
		AddPanelString(fmt::format(fmt::runtime(_("tier {:d} - walk in")), oracool::RiftTier()));
		cursPosition = entry;
	}
}

void CheckRportal()
{
	for (auto &missile : Missiles) {
		if (missile._mitype == MissileID::RedPortal) {
			if (EntranceBoundaryContains(missile.position.tile, cursPosition)) {
				trigflag = true;
				SetPanelString(_("Portal to"), UiFlags::ColorWhite);
				AddPanelString(!setlevel ? _("The Unholy Altar") : _("level 15"));
				cursPosition = missile.position.tile;
			}
		}
	}
}

void CheckCursMove()
{
	if (IsItemLabelHighlighted())
		return;

	int sx = MousePosition.x;
	int sy = MousePosition.y;

	if (CanPanelsCoverView()) {
		if (IsLeftPanelOpen()) {
			sx -= GetScreenWidth() / 4;
		} else if (IsRightPanelOpen()) {
			sx += GetScreenWidth() / 4;
		}
	}
	const Rectangle &mainPanel = GetMainPanel();
	if (mainPanel.contains(MousePosition) && track_isscrolling()) {
		sy = mainPanel.position.y - 1;
	}

	// Oracool: generalized from the old binary /2 to a continuous factor - dividing by 1.0f
	// reproduces the un-zoomed case exactly, and by 2.0f the old fully-zoomed case exactly.
	const float zoomFactor = *sgOptions.Oracool.dungeonZoomLevel;
	sx = static_cast<int>(sx / zoomFactor);
	sy = static_cast<int>(sy / zoomFactor);

	// Adjust by player offset and tile grid alignment
	int xo = 0;
	int yo = 0;
	CalcTileOffset(&xo, &yo);
	sx += xo;
	sy += yo;

	const Player &myPlayer = *MyPlayer;

	if (myPlayer.isWalking()) {
		Displacement offset = GetOffsetForWalking(myPlayer.AnimInfo, myPlayer._pdir, true);
		sx -= offset.deltaX;
		sy -= offset.deltaY;

		// Predict the next frame when walking to avoid input jitter
		DisplacementOf<int16_t> offset2 = myPlayer.position.CalculateWalkingOffsetShifted8(myPlayer._pdir, myPlayer.AnimInfo);
		DisplacementOf<int16_t> velocity = myPlayer.position.GetWalkingVelocityShifted8(myPlayer._pdir, myPlayer.AnimInfo);
		int fx = offset2.deltaX / 256;
		int fy = offset2.deltaY / 256;
		fx -= (offset2.deltaX + velocity.deltaX) / 256;
		fy -= (offset2.deltaY + velocity.deltaY) / 256;

		sx -= fx;
		sy -= fy;
	}

	// Convert to tile grid
	int mx = ViewPosition.x;
	int my = ViewPosition.y;

	int columns = 0;
	int rows = 0;
	TilesInView(&columns, &rows);
	int lrow = rows - RowsCoveredByPanel();

	// Center player tile on screen
	ShiftGrid(&mx, &my, -columns / 2, -lrow / 2);

	// Align grid
	if ((columns % 2) == 0 && (lrow % 2) == 0) {
		sy += TILE_HEIGHT / 2;
	} else if ((columns % 2) != 0 && (lrow % 2) != 0) {
		sx -= TILE_WIDTH / 2;
	} else if ((columns % 2) != 0 && (lrow % 2) == 0) {
		my++;
	}

	// Oracool: generalized from a fixed TILE_HEIGHT/4 fudge (see CalcViewportGeometry) to a linear
	// interpolation - exactly 0 at zoomFactor==1.0 and exactly TILE_HEIGHT/4 at zoomFactor==2.0.
	sy -= static_cast<int>((zoomFactor - 1.0f) * (TILE_HEIGHT / 4.0f));

	int tx = sx / TILE_WIDTH;
	int ty = sy / TILE_HEIGHT;
	ShiftGrid(&mx, &my, tx, ty);

	// Shift position to match diamond grid aligment
	int px = sx % TILE_WIDTH;
	int py = sy % TILE_HEIGHT;

	// Shift position to match diamond grid aligment
	bool flipy = py < (px / 2);
	if (flipy) {
		my--;
	}
	bool flipx = py >= TILE_HEIGHT - (px / 2);
	if (flipx) {
		mx++;
	}

	mx = clamp(mx, 0, MAXDUNX - 1);
	my = clamp(my, 0, MAXDUNY - 1);

	const Point currentTile { mx, my };

	// While holding the button down we should retain target (but potentially lose it if it dies, goes out of view, etc)
	if ((sgbMouseDown != CLICK_NONE || ControllerActionHeld != GameActionType_NONE) && IsNoneOf(LastMouseButtonAction, MouseActionType::None, MouseActionType::Attack, MouseActionType::Spell)) {
		InvalidateTargets();

		if (pcursmonst == -1 && ObjectUnderCursor == nullptr && pcursitem == -1 && pcursinvitem == -1 && pcursstashitem == StashStruct::EmptyCell && pcursplr == -1) {
			cursPosition = { mx, my };
			CheckTrigForce();
			CheckTown();
			CheckRportal();
			CheckRiftPortal();
		}
		return;
	}

	bool flipflag = (flipy && flipx) || ((flipy || flipx) && px < TILE_WIDTH / 2);

	pcurstemp = pcursmonst;
	pcursmonst = -1;
	ObjectUnderCursor = nullptr;
	pcursitem = -1;
	if (pcursinvitem != -1) {
		RedrawComponent(PanelDrawComponent::Belt);
	}
	pcursinvitem = -1;
	pcursstashitem = StashStruct::EmptyCell;
	pcursplr = -1;
	ActiveTabItemHovered = false;
	panelflag = false;
	trigflag = false;

	if (myPlayer._pInvincible) {
		return;
	}
	if (!myPlayer.HoldItem.isEmpty() || spselflag) {
		cursPosition = { mx, my };
		return;
	}
	// Oracool bug fix (user, 2026-08-19): "the area where the hover and ctrl+click works is just
	// very tiny. only a few px somewhere around the top part of the grid boxes."
	//
	// IsPointOverHudChrome, not mainPanel.contains(). This is the SECOND half of the fix made in
	// LeftMouseDown on 2026-08-15 - the old 640x128 main panel rect is mostly empty screen now, and
	// only the HUD plate itself should absorb input. That change went into the click router and
	// never into the hover router, so clicking behaved correctly while hovering was still being
	// swallowed by a rect nothing is drawn in.
	//
	// It only shows at narrow resolutions, which is why it survived this long. The main panel is
	// 640 wide and centred; the inventory is 320 wide against the right edge. At 1280 they never
	// touch. At 960 the panel spans x 160-800 and the inventory starts at 640, so they overlap over
	// the BOTTOM ROWS OF THE BACKPACK - which is where gold accumulates, and why the reported
	// symptom was "gold cannot be Ctrl+Clicked" rather than "the inventory is broken". The hover
	// returned here, pcursinvitem stayed -1, and TransferItemToStash(-1) returns on its first line.
	//
	// The width dependence is the whole reason three rounds of reading the gold path found nothing:
	// the gold path was never wrong.
	if (oracool::IsPointOverHudChrome(MousePosition)) {
		CheckPanelInfo();
		return;
	}
	if (DoomFlag) {
		return;
	}
	// The shop panel, like the inventory below it: it covers the world, so nothing behind it may be
	// targeted. Without this the towners in Griswold's shop were being named and highlighted through
	// their own shop screen (user report, 2026-08-23).
	if (oracool::IsShopGridScreen(stextflag) && oracool::IsPointOverShop(MousePosition))
		return;
	if (invflag && oracool::GetInventoryPanelRect().contains(MousePosition)) {
		pcursinvitem = CheckInvHLight();
		return;
	}
	// The stash's OWN rect, not GetLeftPanel's vanilla 320x352 slot.
	//
	// Bug (fixed 2026-08-16, user report: "hovering and ctrl+click doesnt work on last 5 rows of
	// stash grid"). LeftPanel is 320x352 centred vertically, so on a 720-tall screen it spans
	// y=120..472. The stash grid starts at y=161 and its sixteen 28px rows reach y=609, so row 11
	// began at 469 and everything from there down fell outside the gate - the last five rows,
	// exactly as reported.
	//
	// Both halves of that report are this one line. Hovering obviously stops, but ctrl+click stops
	// too, because CheckStashItem's ctrl branch transfers `pcursstashitem` - the value THIS
	// assignment produces. With no hover there is no item id, so the transfer moved nothing.
	//
	// The neighbours above and below were corrected when the windows outgrew that slot (the
	// inventory reads its own rect, the spellbook its own, and IsOverLeftPanel guards the rest);
	// this one was missed. Note that the IsOverLeftPanel check further down would have covered the
	// stash correctly - but it only returns, it does not set pcursstashitem.
	if (IsStashOpen && GetStashPanelRect().contains(MousePosition)) {
		pcursstashitem = CheckStashHLight(MousePosition);
	}
	// Oracool V1: the book owns a 340x720 rect now, not GetRightPanel's 320x352 - hovering the part
	// outside that slot must not highlight what is on the ground behind the window.
	if (sbookflag && GetSpellBookPanelRect().contains(MousePosition)) {
		return;
	}
	// The free-floating windows - Levski's Roar, its recipe book, the runeword book. Every docked
	// window above excludes itself by rect; these are centred over the world and had no such test,
	// so hovering them probed straight through to the ground behind. User report, 2026-08-20:
	// "you can see i can hover over ogden" with the monument panel open over the tavern.
	if (oracool::IsPointOverFloatingWindow(MousePosition)) {
		return;
	}
	// Same rect the click router uses (control.h's GetLeftPanelContentRect): hovering the part of
	// an open window that falls outside the vanilla 320x352 slot must not highlight monsters and
	// items on the ground behind it, or the cursor would invite exactly the click the router
	// refuses.
	if (IsOverLeftPanel(MousePosition)) {
		return;
	}

	if (leveltype != DTYPE_TOWN) {
		if (pcurstemp != -1) {
			if (!flipflag && mx + 2 < MAXDUNX && my + 1 < MAXDUNY && dMonster[mx + 2][my + 1] != 0 && IsTileLit({ mx + 2, my + 1 })) {
				const uint16_t monsterId = abs(dMonster[mx + 2][my + 1]) - 1;
				if (monsterId == pcurstemp && Monsters[monsterId].hitPoints >> 6 > 0 && (Monsters[monsterId].data().selectionType & 4) != 0) {
					cursPosition = Point { mx, my } + Displacement { 2, 1 };
					pcursmonst = monsterId;
				}
			}
			if (flipflag && mx + 1 < MAXDUNX && my + 2 < MAXDUNY && dMonster[mx + 1][my + 2] != 0 && IsTileLit({ mx + 1, my + 2 })) {
				const uint16_t monsterId = abs(dMonster[mx + 1][my + 2]) - 1;
				if (monsterId == pcurstemp && Monsters[monsterId].hitPoints >> 6 > 0 && (Monsters[monsterId].data().selectionType & 4) != 0) {
					cursPosition = Point { mx, my } + Displacement { 1, 2 };
					pcursmonst = monsterId;
				}
			}
			if (mx + 2 < MAXDUNX && my + 2 < MAXDUNY && dMonster[mx + 2][my + 2] != 0 && IsTileLit({ mx + 2, my + 2 })) {
				const uint16_t monsterId = abs(dMonster[mx + 2][my + 2]) - 1;
				if (monsterId == pcurstemp && Monsters[monsterId].hitPoints >> 6 > 0 && (Monsters[monsterId].data().selectionType & 4) != 0) {
					cursPosition = Point { mx, my } + Displacement { 2, 2 };
					pcursmonst = monsterId;
				}
			}
			if (mx + 1 < MAXDUNX && !flipflag && dMonster[mx + 1][my] != 0 && IsTileLit({ mx + 1, my })) {
				const uint16_t monsterId = abs(dMonster[mx + 1][my]) - 1;
				if (monsterId == pcurstemp && Monsters[monsterId].hitPoints >> 6 > 0 && (Monsters[monsterId].data().selectionType & 2) != 0) {
					cursPosition = Point { mx, my } + Displacement { 1, 0 };
					pcursmonst = monsterId;
				}
			}
			if (my + 1 < MAXDUNY && flipflag && dMonster[mx][my + 1] != 0 && IsTileLit({ mx, my + 1 })) {
				const uint16_t monsterId = abs(dMonster[mx][my + 1]) - 1;
				if (monsterId == pcurstemp && Monsters[monsterId].hitPoints >> 6 > 0 && (Monsters[monsterId].data().selectionType & 2) != 0) {
					cursPosition = Point { mx, my } + Displacement { 0, 1 };
					pcursmonst = monsterId;
				}
			}
			if (dMonster[mx][my] != 0 && IsTileLit({ mx, my })) {
				const uint16_t monsterId = abs(dMonster[mx][my]) - 1;
				if (monsterId == pcurstemp && Monsters[monsterId].hitPoints >> 6 > 0 && (Monsters[monsterId].data().selectionType & 1) != 0) {
					cursPosition = { mx, my };
					pcursmonst = monsterId;
				}
			}
			if (mx + 1 < MAXDUNX && my + 1 < MAXDUNY && dMonster[mx + 1][my + 1] != 0 && IsTileLit({ mx + 1, my + 1 })) {
				const uint16_t monsterId = abs(dMonster[mx + 1][my + 1]) - 1;
				if (monsterId == pcurstemp && Monsters[monsterId].hitPoints >> 6 > 0 && (Monsters[monsterId].data().selectionType & 2) != 0) {
					cursPosition = Point { mx, my } + Displacement { 1, 1 };
					pcursmonst = monsterId;
				}
			}
			if (pcursmonst != -1 && (Monsters[pcursmonst].flags & MFLAG_HIDDEN) != 0) {
				pcursmonst = -1;
				cursPosition = { mx, my };
			}
			if (pcursmonst != -1 && Monsters[pcursmonst].isPlayerMinion()) {
				pcursmonst = -1;
			}
			if (pcursmonst != -1) {
				return;
			}
		}
		if (!flipflag && mx + 2 < MAXDUNX && my + 1 < MAXDUNY && dMonster[mx + 2][my + 1] != 0 && IsTileLit({ mx + 2, my + 1 })) {
			int monsterId = abs(dMonster[mx + 2][my + 1]) - 1;
			if (Monsters[monsterId].hitPoints >> 6 > 0 && (Monsters[monsterId].data().selectionType & 4) != 0) {
				cursPosition = Point { mx, my } + Displacement { 2, 1 };
				pcursmonst = monsterId;
			}
		}
		if (flipflag && mx + 1 < MAXDUNX && my + 2 < MAXDUNY && dMonster[mx + 1][my + 2] != 0 && IsTileLit({ mx + 1, my + 2 })) {
			const uint16_t monsterId = abs(dMonster[mx + 1][my + 2]) - 1;
			if (Monsters[monsterId].hitPoints >> 6 > 0 && (Monsters[monsterId].data().selectionType & 4) != 0) {
				cursPosition = Point { mx, my } + Displacement { 1, 2 };
				pcursmonst = monsterId;
			}
		}
		if (mx + 2 < MAXDUNX && my + 2 < MAXDUNY && dMonster[mx + 2][my + 2] != 0 && IsTileLit({ mx + 2, my + 2 })) {
			const uint16_t monsterId = abs(dMonster[mx + 2][my + 2]) - 1;
			if (Monsters[monsterId].hitPoints >> 6 > 0 && (Monsters[monsterId].data().selectionType & 4) != 0) {
				cursPosition = Point { mx, my } + Displacement { 2, 2 };
				pcursmonst = monsterId;
			}
		}
		if (!flipflag && mx + 1 < MAXDUNX && dMonster[mx + 1][my] != 0 && IsTileLit({ mx + 1, my })) {
			const uint16_t monsterId = abs(dMonster[mx + 1][my]) - 1;
			if (Monsters[monsterId].hitPoints >> 6 > 0 && (Monsters[monsterId].data().selectionType & 2) != 0) {
				cursPosition = Point { mx, my } + Displacement { 1, 0 };
				pcursmonst = monsterId;
			}
		}
		if (flipflag && my + 1 < MAXDUNY && dMonster[mx][my + 1] != 0 && IsTileLit({ mx, my + 1 })) {
			const uint16_t monsterId = abs(dMonster[mx][my + 1]) - 1;
			if (Monsters[monsterId].hitPoints >> 6 > 0 && (Monsters[monsterId].data().selectionType & 2) != 0) {
				cursPosition = Point { mx, my } + Displacement { 0, 1 };
				pcursmonst = monsterId;
			}
		}
		if (dMonster[mx][my] != 0 && IsTileLit({ mx, my })) {
			const uint16_t monsterId = abs(dMonster[mx][my]) - 1;
			if (Monsters[monsterId].hitPoints >> 6 > 0 && (Monsters[monsterId].data().selectionType & 1) != 0) {
				cursPosition = { mx, my };
				pcursmonst = monsterId;
			}
		}
		if (mx + 1 < MAXDUNX && my + 1 < MAXDUNY && dMonster[mx + 1][my + 1] != 0 && IsTileLit({ mx + 1, my + 1 })) {
			const uint16_t monsterId = abs(dMonster[mx + 1][my + 1]) - 1;
			if (Monsters[monsterId].hitPoints >> 6 > 0 && (Monsters[monsterId].data().selectionType & 2) != 0) {
				cursPosition = Point { mx, my } + Displacement { 1, 1 };
				pcursmonst = monsterId;
			}
		}
		if (pcursmonst != -1 && (Monsters[pcursmonst].flags & MFLAG_HIDDEN) != 0) {
			pcursmonst = -1;
			cursPosition = { mx, my };
		}
		if (pcursmonst != -1 && (Monsters[pcursmonst].isPlayerMinion() || IsAnyOf(pcurs, CURSOR_HEALOTHER, CURSOR_RESURRECT))) {
			pcursmonst = -1;
		}
	} else {
		if (!flipflag && mx + 1 < MAXDUNX && dMonster[mx + 1][my] > 0) {
			pcursmonst = dMonster[mx + 1][my] - 1;
			cursPosition = Point { mx, my } + Displacement { 1, 0 };
		}
		if (flipflag && my + 1 < MAXDUNY && dMonster[mx][my + 1] > 0) {
			pcursmonst = dMonster[mx][my + 1] - 1;
			cursPosition = Point { mx, my } + Displacement { 0, 1 };
		}
		if (dMonster[mx][my] > 0) {
			pcursmonst = dMonster[mx][my] - 1;
			cursPosition = { mx, my };
		}
		if (mx + 1 < MAXDUNX && my + 1 < MAXDUNY && dMonster[mx + 1][my + 1] > 0) {
			pcursmonst = dMonster[mx + 1][my + 1] - 1;
			cursPosition = Point { mx, my } + Displacement { 1, 1 };
		}
	}

	if (pcursmonst == -1) {
		if (!flipflag && mx + 1 < MAXDUNX && dPlayer[mx + 1][my] != 0) {
			const uint8_t playerId = abs(dPlayer[mx + 1][my]) - 1;
			Player &player = Players[playerId];
			if (&player != MyPlayer && player._pHitPoints != 0) {
				cursPosition = Point { mx, my } + Displacement { 1, 0 };
				pcursplr = static_cast<int8_t>(playerId);
			}
		}
		if (flipflag && my + 1 < MAXDUNY && dPlayer[mx][my + 1] != 0) {
			const uint8_t playerId = abs(dPlayer[mx][my + 1]) - 1;
			Player &player = Players[playerId];
			if (&player != MyPlayer && player._pHitPoints != 0) {
				cursPosition = Point { mx, my } + Displacement { 0, 1 };
				pcursplr = static_cast<int8_t>(playerId);
			}
		}
		if (dPlayer[mx][my] != 0) {
			const uint8_t playerId = abs(dPlayer[mx][my]) - 1;
			if (playerId != MyPlayerId) {
				cursPosition = { mx, my };
				pcursplr = static_cast<int8_t>(playerId);
			}
		}
		if (TileContainsDeadPlayer({ mx, my })) {
			for (const Player &player : Players) {
				if (player.position.tile == Point { mx, my } && &player != MyPlayer) {
					cursPosition = { mx, my };
					pcursplr = static_cast<int8_t>(player.getId());
				}
			}
		}
		if (pcurs == CURSOR_RESURRECT) {
			for (int xx = -1; xx < 2; xx++) {
				for (int yy = -1; yy < 2; yy++) {
					if (TileContainsDeadPlayer({ mx + xx, my + yy })) {
						for (const Player &player : Players) {
							if (player.position.tile.x == mx + xx && player.position.tile.y == my + yy && &player != MyPlayer) {
								cursPosition = Point { mx, my } + Displacement { xx, yy };
								pcursplr = static_cast<int8_t>(player.getId());
							}
						}
					}
				}
			}
		}
		if (mx + 1 < MAXDUNX && my + 1 < MAXDUNY && dPlayer[mx + 1][my + 1] != 0) {
			const uint8_t playerId = abs(dPlayer[mx + 1][my + 1]) - 1;
			const Player &player = Players[playerId];
			if (&player != MyPlayer && player._pHitPoints != 0) {
				cursPosition = Point { mx, my } + Displacement { 1, 1 };
				pcursplr = static_cast<int8_t>(playerId);
			}
		}
	}
	if (pcursmonst == -1 && pcursplr == -1) {
		// No monsters or players under the cursor, try find an object starting with the tile below the current tile (tall
		//  objects like doors)
		Point testPosition = currentTile + Direction::South;
		Object *object = FindObjectAtPosition(testPosition);

		if (object == nullptr || object->_oSelFlag < 2) {
			// Either no object or can't interact from the test position, try the current tile
			testPosition = currentTile;
			object = FindObjectAtPosition(testPosition);

			if (object == nullptr || IsNoneOf(object->_oSelFlag, 1, 3)) {
				// Still no object (that could be activated from this position), try the tile to the bottom left or right
				//  (whichever is closest to the cursor as determined when we set flipflag earlier)
				testPosition = currentTile + (flipflag ? Direction::SouthWest : Direction::SouthEast);
				object = FindObjectAtPosition(testPosition);

				if (object != nullptr && object->_oSelFlag < 2) {
					// Found an object but it's not in range, clear the pointer
					object = nullptr;
				}
			}
		}
		if (object != nullptr) {
			// found object that can be activated with the given cursor position
			cursPosition = testPosition;
			ObjectUnderCursor = object;
		}
	}
	if (pcursplr == -1 && ObjectUnderCursor == nullptr && pcursmonst == -1) {
		if (!flipflag && mx + 1 < MAXDUNX && dItem[mx + 1][my] > 0) {
			const uint8_t itemId = dItem[mx + 1][my] - 1;
			if (Items[itemId]._iSelFlag >= 2) {
				cursPosition = Point { mx, my } + Displacement { 1, 0 };
				pcursitem = static_cast<int8_t>(itemId);
			}
		}
		if (flipflag && my + 1 < MAXDUNY && dItem[mx][my + 1] > 0) {
			const uint8_t itemId = dItem[mx][my + 1] - 1;
			if (Items[itemId]._iSelFlag >= 2) {
				cursPosition = Point { mx, my } + Displacement { 0, 1 };
				pcursitem = static_cast<int8_t>(itemId);
			}
		}
		if (dItem[mx][my] > 0) {
			const uint8_t itemId = dItem[mx][my] - 1;
			if (Items[itemId]._iSelFlag == 1 || Items[itemId]._iSelFlag == 3) {
				cursPosition = { mx, my };
				pcursitem = static_cast<int8_t>(itemId);
			}
		}
		if (mx + 1 < MAXDUNX && my + 1 < MAXDUNY && dItem[mx + 1][my + 1] > 0) {
			const uint8_t itemId = dItem[mx + 1][my + 1] - 1;
			if (Items[itemId]._iSelFlag >= 2) {
				cursPosition = Point { mx, my } + Displacement { 1, 1 };
				pcursitem = static_cast<int8_t>(itemId);
			}
		}
		if (pcursitem == -1) {
			cursPosition = { mx, my };
			CheckTrigForce();
			CheckTown();
			CheckRportal();
			CheckRiftPortal();
		}
	}

	if (pcurs == CURSOR_IDENTIFY) {
		ObjectUnderCursor = nullptr;
		pcursmonst = -1;
		pcursitem = -1;
		cursPosition = { mx, my };
	}
	if (pcursmonst != -1 && leveltype != DTYPE_TOWN && Monsters[pcursmonst].isPlayerMinion()) {
		pcursmonst = -1;
	}
}

} // namespace devilution
