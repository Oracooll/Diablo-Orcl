/**
 * @file items.cpp
 *
 * Implementation of item functionality.
 */
#include "items.h"

#include <algorithm>
#include <bitset>
#ifdef _DEBUG
#include <random>
#endif
#include <climits>
#include <cstdint>

#include <fmt/core.h>
#include <fmt/format.h>

#include "DiabloUI/ui_flags.hpp"
#include "controls/plrctrls.h"
#include "cursor.h"
#include "doom.h"
#include "engine/backbuffer_state.hpp"
#include "engine/clx_sprite.hpp"
#include "engine/dx.h"
#include "engine/load_cel.hpp"
#include "engine/random.hpp"
#include "engine/render/clx_render.hpp"
#include "engine/render/primitive_render.hpp"
#include "engine/render/text_render.hpp"
#include "init.h"
#include "inv.h"
#include "inv_iterators.hpp"
#include "items/validation.h"
#include "levels/town.h"
#include "lighting.h"
#include "minitext.h"
#include "missiles.h"
#include "options.h"
#include "oracool/hidden_classes.h"
#include "oracool/necro_items.h"
#include "oracool/rift.h" // IsRiftGuardian - a rift guardian drops random items, not his quest unique
#include "oracool/item_tiers.h"
#include "oracool/rfa12_effects.h"
#include "oracool/sprite_mix.h"
#include "oracool/player_resistance.h"
#include "oracool/area_level.h"
#include "oracool/auto_save.h"
#include "oracool/class_skills.h"
#include "oracool/spell_ranks.h"
#include "oracool/event_log.h"
#include "oracool/gradual_healing.h"
#include "oracool/charms.h"
#include "oracool/gems.h"
#include "oracool/endgame_boss.h" // IsEndgameBoss - a Dread boss pays as a unique
#include "oracool/item_names.h"
#include "oracool/item_sets.h"
#include "oracool/oracool.h"
#include "oracool/runewords.h"
#include "oracool/smart_loot.h"
#include "oracool/imbuement.h"
#include "oracool/level_requirement.h"
#include "oracool/monster_variants.h"
#include "oracool/named_encounters.h"
#include "oracool/salvage.h"
#include "oracool/signets.h"
#include "oracool/sprite_scale.h" // ScaleClxList - the fork tumbles at their chosen size (InitItemGFX)
#include "oracool/skill_sounds.h"
#include "oracool/sprite_import.h"
#include "oracool/stat_sheet.h"
#include "oracool/treasure_class.h"
#include "panels/info_box.hpp"
#include "panels/ui_panels.hpp"
#include "player.h"
#include "playerdat.hpp"
#include "qol/stash.h"
#include "spells.h"
#include "stores.h"
#include "utils/format_int.hpp"
#include "utils/language.h"
#include "plrmsg.h"
#include "utils/log.hpp"
#include "utils/math.h"
#include "utils/stdcompat/algorithm.hpp"
#include "utils/str_case.hpp"
#include "utils/str_cat.hpp"
#include "utils/utf8.hpp"

namespace devilution {

Item Items[MAXITEMS + 1];
uint8_t ActiveItems[MAXITEMS];
uint8_t ActiveItemCount;
int8_t dItem[MAXDUNX][MAXDUNY];
CornerStoneStruct CornerStone;
bool UniqueItemFlags[MaxUniqueItems];
int MaxGold = GOLD_MAX_LIMIT;

/**
 * @brief True while an item is being rebuilt from a stored seed rather than generated fresh - see ReplayScope
 * and PoolQlvl, and DrawUnifiedAffix's item-level ceiling, which applies to fresh generation only.
 *
 * File-local (static) and defined HERE, outside every unnamed namespace, because readers sit in two different
 * unnamed-namespace blocks of this file; a forward declaration in one of them named a second entity.
 */
static bool ReplayingStoredItemSeed = false;

/** Maps from item_cursor_graphic to in-memory item type. */
int8_t ItemCAnimTbl[] = {
	20, 16, 16, 16, 4, 4, 4, 12, 12, 12,
	12, 12, 12, 12, 12, 21, 21, 25, 12, 28,
	28, 28, 38, 38, 38, 32, 38, 38, 38, 24,
	24, 26, 2, 25, 22, 23, 24, 21, 27, 27,
	29, 0, 0, 0, 12, 12, 12, 12, 12, 0,
	8, 8, 0, 8, 8, 8, 8, 8, 8, 6,
	8, 8, 8, 6, 8, 8, 6, 8, 8, 6,
	6, 6, 8, 8, 8, 5, 9, 13, 13, 13,
	5, 5, 5, 15, 5, 5, 18, 18, 18, 30,
	5, 5, 14, 5, 14, 13, 16, 18, 5, 5,
	7, 1, 3, 17, 1, 15, 10, 14, 3, 11,
	8, 0, 1, 7, 0, 7, 15, 7, 3, 3,
	3, 6, 6, 11, 11, 11, 31, 14, 14, 14,
	6, 6, 7, 3, 8, 14, 0, 14, 14, 0,
	33, 1, 1, 1, 1, 1, 7, 7, 7, 14,
	14, 17, 17, 17, 0, 34, 1, 0, 3, 17,
	8, 8, 6, 1, 3, 3, 11, 3, 12, 12,
	12, 12, 12, 12, 12, 12, 12, 12, 12, 12,
	12, 12, 12, 12, 12, 12, 12, 35, 39, 36,
	36, 36, 37, 38, 38, 38, 38, 38, 41, 42,
	8, 8, 8, 17, 0, 6, 8, 11, 11, 3,
	3, 1, 6, 6, 6, 1, 8, 6, 11, 3,
	6, 8, 1, 6, 6, 17, 40, 0, 0
};
static_assert(sizeof(ItemCAnimTbl) / sizeof(ItemCAnimTbl[0]) == ICURS_ORACOOL_FIRST,
    "ItemCAnimTbl no longer covers exactly the vanilla item graphics");

/** Maps of drop sounds effect of placing the item in the inventory. */
_sfx_id ItemInvSnds[] = {
	IS_IHARM,
	IS_IAXE,
	IS_IPOT,
	IS_IBOW,
	IS_GOLD,
	IS_ICAP,
	IS_ISWORD,
	IS_ISHIEL,
	IS_ISWORD,
	IS_IROCK,
	IS_IAXE,
	IS_ISTAF,
	IS_IRING,
	IS_ICAP,
	IS_ILARM,
	IS_ISHIEL,
	IS_ISCROL,
	IS_IHARM,
	IS_IBOOK,
	IS_IHARM,
	IS_IPOT,
	IS_IPOT,
	IS_IPOT,
	IS_IPOT,
	IS_IPOT,
	IS_IPOT,
	IS_IPOT,
	IS_IPOT,
	IS_IBODY,
	IS_IBODY,
	IS_IMUSH,
	IS_ISIGN,
	IS_IBLST,
	IS_IANVL,
	IS_ISTAF,
	IS_IROCK,
	IS_ISCROL,
	IS_ISCROL,
	IS_IROCK,
	IS_IMUSH,
	IS_IHARM,
	IS_ILARM,
	IS_ILARM,
	// Oracool: the fork's five tumbles (43-47).
	IS_IROCK, // gemflip
	IS_IROCK, // runeflip
	IS_IRING, // charmflip
	IS_IBLST, // shardflip (orbflip until 2026-09-19)
	IS_IRING, // signetflip
	// Oracool: batch 15's three (48-50).
	IS_IROCK, // jewelflip
	IS_ILARM, // salvageflip
	IS_ISCROL, // mapflip
	// Oracool: batches 21 and 22 (51-62). Still vanilla sounds - these twelve have their own SPRITE
	// now, not their own audio; the fork's item drop/pickup cues are a separate gap.
	// The six worn slots, all soft leather-and-iron, so all IS_ILARM like the larmor they replace.
	IS_ILARM, // gloveflip
	IS_ILARM, // bootflip
	IS_ILARM, // bracerflip
	IS_ILARM, // beltflip
	IS_ILARM, // legflip
	IS_ILARM, // shoulderflip
	// The six exotic bases, matched to what the object is rather than to leather by default.
	IS_ILARM, // cloakflip    - heavy cloth
	IS_IROCK, // relicflip    - a hard casket
	IS_IHARM, // spearflip    - a long iron polearm
	IS_IHARM, // luteflip     - a wooden body with strings; the metal drop is the closest
	IS_ILARM, // quiverflip   - leather and arrows
	IS_IBOOK, // focusflip    - a bound book
	// Batch 32 (2026-09-13, RfA-14): the amulet - a pendant and chain, jewellery, so the ring's sound.
	IS_IRING, // amuletflip
};

namespace {

// Oracool: the fork's own drop tumbles (batch 10, 2026-09-11), appended after Hellfire's 43 so no
// index ItemCAnimTbl uses moves.
constexpr int FirstOracoolDropAnim = 43;
constexpr int8_t OracoolGemDropAnim = 43;
constexpr int8_t OracoolRuneDropAnim = 44;
constexpr int8_t OracoolCharmDropAnim = 45;
constexpr int8_t OracoolShardDropAnim = 46; // the orbs' slot, re-drawn as a shard by RfA-18 batch 41
constexpr int8_t OracoolSignetDropAnim = 47;
// Batch 15 (2026-09-11), appended after the first five.
constexpr int8_t OracoolJewelDropAnim = 48;
constexpr int8_t OracoolSalvageDropAnim = 49;
constexpr int8_t OracoolMapDropAnim = 50;
// Batches 21 and 22 (2026-09-12), appended after the first eight. These are keyed by what the item
// IS rather than by cursor id - see GetItemDropAnimIndexFor - because they have to serve 250
// uniques and 94 set pieces as well as the bases, and an id list of that size is 344 chances to
// point an item at the wrong tumble.
constexpr int8_t OracoolGloveDropAnim = 51;
constexpr int8_t OracoolBootDropAnim = 52;
constexpr int8_t OracoolBracerDropAnim = 53;
constexpr int8_t OracoolBeltDropAnim = 54;
constexpr int8_t OracoolLegDropAnim = 55;
constexpr int8_t OracoolShoulderDropAnim = 56;
constexpr int8_t OracoolCloakDropAnim = 57;
constexpr int8_t OracoolRelicDropAnim = 58;
constexpr int8_t OracoolSpearDropAnim = 59;
constexpr int8_t OracoolLuteDropAnim = 60;
constexpr int8_t OracoolQuiverDropAnim = 61;
constexpr int8_t OracoolFocusDropAnim = 62;
// Batch 32 (2026-09-13, RfA-14): the amulet, the one item shape the ground audit found with no tumble.
constexpr int8_t OracoolAmuletDropAnim = 63;
static_assert(ITEMTYPES == FirstOracoolDropAnim + 21, "ITEMTYPES must count the twenty-one Oracool tumbles");

/**
 * @brief How large each fork tumble is drawn, in percent of its sheet (user, 2026-09-13: "we need to reduce
 * size of ground assets of oracool items", chosen per sheet on the Ground Tumble Scale page).
 *
 * The fork's sheets were drawn two to fifteen times the pixel area of the vanilla drop nearest their shape -
 * gloves 40x29 against the helm's 13x15, the amulet 52x23 against the ring's 7x7. Applied once, when the sheet
 * loads (InitItemGFX), with the engine's own nearest-neighbour scaler, so the art stays hard-edged and the frame
 * COUNT is untouched: a saved item's frame number is still valid. Indexed from FirstOracoolDropAnim.
 *
 * The nine the user picked are marked; the other twelve are the page's suggestion, which is what those rows
 * previewed when the picks were made.
 */
constexpr std::array<uint16_t, 21> OracoolDropAnimScale {
	90,  // gemflip        (suggested)
	90,  // runeflip       (suggested)
	100, // charmflip      (suggested)
	60,  // shardflip      (suggested)
	50,  // signetflip     picked
	80,  // jewelflip      (suggested)
	90,  // salvageflip    (suggested)
	75,  // mapflip        (suggested)
	60,  // gloveflip      (suggested)
	70,  // bootflip       (suggested)
	60,  // bracerflip     (suggested)
	60,  // beltflip       picked
	60,  // legflip        picked
	60,  // shoulderflip   (suggested)
	60,  // cloakflip      picked
	60,  // relicflip      picked
	60,  // spearflip      picked
	50,  // luteflip       picked
	50,  // quiverflip     (suggested)
	60,  // focusflip      picked
	40,  // amuletflip     picked
};
static_assert(OracoolDropAnimScale.size() == static_cast<size_t>(ITEMTYPES - FirstOracoolDropAnim),
    "every fork tumble needs a scale");

// Oracool: the ranges below lean on these blocks being contiguous; a generator that grows or splits
// one must fail here rather than hand a stray icon the wrong tumble.
static_assert(ICURS_ORACOOL_GEM_SKULL - ICURS_ORACOOL_GEM_RUBY == 4);
static_assert(ICURS_ORACOOL_RUNE_SOL - ICURS_ORACOOL_RUNE_EL == 4);
static_assert(ICURS_ORACOOL_GEM_SKULL_PERFECT - ICURS_ORACOOL_GEM_AMETHYST_CHIPPED == 29);
static_assert(ICURS_ORACOOL_RUNE_ZOD - ICURS_ORACOOL_RUNE_ELD == 27);
static_assert(ICURS_ORACOOL_CHARM_SALVAGE_ETHEREAL_IMBUEITIES - ICURS_ORACOOL_CHARM_SALVAGE_WHITE_SCALES == 6);
static_assert(ICURS_ORACOOL_SHARD_AVARICE - ICURS_ORACOOL_SHARD_STRENGTH == 7);
static_assert(ICURS_ORACOOL_SHARD_EASE - ICURS_ORACOOL_SHARD_BLOOD == 15);
static_assert(ICURS_ORACOOL_CHARM_LEGEND - ICURS_ORACOOL_CHARM_TRIALS == 2);
static_assert(ICURS_ORACOOL_CHARM_VAULT - ICURS_ORACOOL_CHARM_CHAPEL == 2);
static_assert(ICURS_ORACOOL_CHARM_GREED - ICURS_ORACOOL_CHARM_VIGOR == 5);
static_assert(ICURS_ORACOOL_JEWEL_WARDING_RADIANT - ICURS_ORACOOL_JEWEL_FERVOR_FLAWED == 14);
static_assert(ICURS_ORACOOL_SALVAGE_ETHEREAL_IMBUEITIES - ICURS_ORACOOL_SALVAGE_WHITE_SCALES == 6);
static_assert(ICURS_ORACOOL_MAP_VAULT - ICURS_ORACOOL_MAP_CHAPEL == 2);

constexpr bool InCursRange(uint16_t curs, int first, int last)
{
	return curs >= first && curs <= last;
}

} // namespace

/**
 * @brief Ground-drop animation index for an item graphic, safe for any cursor id.
 *
 * Oracool: ItemCAnimTbl is a flat array with one entry per VANILLA item graphic, indexed by
 * _iCurs with no bounds check. Our own icons live past its end, so reading it directly for one of
 * them is an out-of-bounds read that happens to work until it doesn't.
 *
 * Eight families of fork icons have their own tumble (items\<name>.png): gems, runes, charms,
 * Mystic Orbs and the Signet (batch 10), then jewels, salvage materials and the sealed maps
 * (batch 15). Everything else of ours - uniques, set pieces and worn gear - borrows "larmor".
 *
 * Visual and audio only: this picks a sprite, a frame count and two sounds. Nothing generated or
 * saved reads it, and every tumble here is 13 frames like larmor, so a saved item's frame count is
 * unchanged either way.
 */
int8_t GetItemDropAnimIndex(uint16_t curs)
{
	if (curs < ICURS_ORACOOL_FIRST)
		return ItemCAnimTbl[curs];

	// Gems: the five plain gems and the quality ladder (the first rune block sits between them).
	if (InCursRange(curs, ICURS_ORACOOL_GEM_RUBY, ICURS_ORACOOL_GEM_SKULL)
	    || InCursRange(curs, ICURS_ORACOOL_GEM_AMETHYST_CHIPPED, ICURS_ORACOOL_GEM_SKULL_PERFECT))
		return OracoolGemDropAnim;
	// Runes: the five shipped icons and the 28 of runes_curs.inc.
	if (InCursRange(curs, ICURS_ORACOOL_RUNE_EL, ICURS_ORACOOL_RUNE_SOL)
	    || InCursRange(curs, ICURS_ORACOOL_RUNE_ELD, ICURS_ORACOOL_RUNE_ZOD))
		return OracoolRuneDropAnim;
	// Charms: Phase 1, Salvaging, growing, and the three encounter rewards (not their maps).
	if (InCursRange(curs, ICURS_ORACOOL_CHARM_VIGOR, ICURS_ORACOOL_CHARM_GREED)
	    || InCursRange(curs, ICURS_ORACOOL_CHARM_SALVAGE_WHITE_SCALES, ICURS_ORACOOL_CHARM_SALVAGE_ETHEREAL_IMBUEITIES)
	    || InCursRange(curs, ICURS_ORACOOL_CHARM_TRIALS, ICURS_ORACOOL_CHARM_LEGEND)
	    || InCursRange(curs, ICURS_ORACOOL_CHARM_CHAPEL, ICURS_ORACOOL_CHARM_VAULT))
		return OracoolCharmDropAnim;
	// The Imbuement Shards' two islands (2026-09-19), one tumble for all twenty-four kinds.
	if (InCursRange(curs, ICURS_ORACOOL_SHARD_STRENGTH, ICURS_ORACOOL_SHARD_AVARICE)
	    || InCursRange(curs, ICURS_ORACOOL_SHARD_BLOOD, ICURS_ORACOOL_SHARD_EASE))
		return OracoolShardDropAnim;
	if (curs == ICURS_ORACOOL_SIGNET_LEARNING)
		return OracoolSignetDropAnim;
	// Jewels: the fifteen of jewels_curs.inc (five kinds, three grades).
	if (InCursRange(curs, ICURS_ORACOOL_JEWEL_FERVOR_FLAWED, ICURS_ORACOOL_JEWEL_WARDING_RADIANT))
		return OracoolJewelDropAnim;
	// Salvage materials: the seven of salvage_curs.inc (their Charms of Salvaging are charms, above).
	if (InCursRange(curs, ICURS_ORACOOL_SALVAGE_WHITE_SCALES, ICURS_ORACOOL_SALVAGE_ETHEREAL_IMBUEITIES))
		return OracoolSalvageDropAnim;
	// The three sealed encounter maps (their reward charms are charms, above).
	if (InCursRange(curs, ICURS_ORACOOL_MAP_CHAPEL, ICURS_ORACOOL_MAP_VAULT))
		return OracoolMapDropAnim;

	// 14 is "larmor" - light armour, a soft cloth/leather tumble. The closest existing match for
	// gloves, boots, bracers, shoulders and legs; the belt borrows it too rather than the noisier
	// metal-armour drop.
	constexpr int8_t OracoolWornDropAnim = 14;
	return OracoolWornDropAnim;
}

/**
 * @brief Ground-drop animation index for an ITEM, which knows more about itself than its cursor id.
 *
 * Batches 21 and 22 (2026-09-12) added twelve tumbles, and mapping them by cursor id the way the
 * eight socketable families are mapped would not work: they have to serve **250 uniques and 94 set
 * pieces** as well as the bases, and every one of those carries its own `IPL_INVCURS` icon. An id
 * list of that size is 344 chances to point an item at the wrong sprite, and a CEL has no way to
 * notice one.
 *
 * So these are keyed on what the item IS. Both keys survive the thing that breaks an id list:
 *
 *  - `_iLoc` is the six worn slots the fork added, so a Seraphic belt and an Iron belt and a unique
 *    belt all answer "belt" without any of them being named here.
 *  - `AllItemsList[IDidx].iItemId` is the base's UITYPE, and a unique or set piece is a row built
 *    ON a base, so it reports the base's shape. That is exactly what a tumble should follow.
 *
 * Falls through to the cursor-id function for everything it does not recognise, which is every
 * vanilla item and all eight socketable families.
 */
// Vanilla's own sheets, by their position in ItemDropNames - the shapes the fork's icons are re-keyed to
// below. Mace (6) and shield (7) are declared in items.h, where a missile already borrows them.
constexpr int8_t MediumArmorDropAnimIndex = 0; // armor2 - 15 frames, the one target that is not 13
constexpr int8_t AxeDropAnimIndex = 1;         // axe
constexpr int8_t BowDropAnimIndex = 3;         // bow
constexpr int8_t HelmDropAnimIndex = 5;        // helmut
constexpr int8_t SwordDropAnimIndex = 8;       // swrdflip
constexpr int8_t StaffDropAnimIndex = 11;      // staff
constexpr int8_t RingDropAnimIndex = 12;       // ring
constexpr int8_t OracoolFallbackDropAnim = 14; // larmor - GetItemDropAnimIndex's answer for an unplaced fork icon
constexpr int8_t HeavyArmorDropAnimIndex = 17; // fplatear

int8_t GetItemDropAnimIndexFor(const Item &item)
{
	// NB: the calls out of this function go to the CURSOR-ID form, never back to this one. A bulk
	// rewrite of the call sites caught these two as well on 2026-09-12 and turned both into
	// unbounded recursion - 64 tests went SEGFAULT at once, which is what a stack overflow looks
	// like from ctest.
	if (item.isEmpty())
		return GetItemDropAnimIndex(item._iCurs);

	// The six worn slots. Checked BEFORE the UITYPE, because a worn slot is the stronger statement:
	// whatever base a pair of gauntlets is built on, it lands like gloves.
	switch (item._iLoc) {
	case ILOC_GLOVES:
		return OracoolGloveDropAnim;
	case ILOC_BOOTS:
		return OracoolBootDropAnim;
	case ILOC_BRACERS:
		return OracoolBracerDropAnim;
	case ILOC_WAIST:
		return OracoolBeltDropAnim;
	case ILOC_LEGS:
		return OracoolLegDropAnim;
	case ILOC_SHOULDERS:
		return OracoolShoulderDropAnim;
	default:
		break;
	}

	// The six exotic bases, by the base's UITYPE. IDidx is bounds-checked because an item mid-recreate
	// can carry IDI_NONE.
	if (item.IDidx >= 0 && item.IDidx <= IDI_LAST) {
		switch (AllItemsList[static_cast<size_t>(item.IDidx)].iItemId) {
		// ORCLCLOAK and BATTLECLOAK, not UITYPE_CLOAK - that enum exists (it is Hellfire's) so a
		// case for it COMPILED and silently matched nothing, which is how the Travelling Cloak was
		// still tumbling as larmor after this function was written. The test caught it.
		case UITYPE_ORCLCLOAK:
		case UITYPE_BATTLECLOAK:
			return OracoolCloakDropAnim;
		case UITYPE_RELIC:
		case UITYPE_RELIQUARY:
			return OracoolRelicDropAnim;
		case UITYPE_SPEAR:
		case UITYPE_PIKE:
			return OracoolSpearDropAnim;
		case UITYPE_WARLUTE:
			return OracoolLuteDropAnim;
		case UITYPE_WARQUIVER:
			return OracoolQuiverDropAnim;
		case UITYPE_CANTICLE:
		case UITYPE_ARCANEFOCUS:
			return OracoolFocusDropAnim;
		default:
			break;
		}
	}

	// THE ITEM'S TYPE, for everything carrying its own Oracool icon that the cursor-id table cannot
	// place (2026-09-13, the ground-tumble audit). The fork's tier helms, armours and shields, and every
	// unique and set piece built on a weapon, helm, armour, shield or piece of jewellery, carry an icon
	// id past ICURS_ORACOOL_FIRST, so GetItemDropAnimIndex falls back to larmor for them - 228 items were
	// flopping to the floor as leather. Vanilla already has the right sheet for every one of those
	// shapes; this points them at it. RfA-08 promised this mapping and it was never written.
	//
	// Only the FALLBACK is re-keyed: the eight socketable families answer by cursor range above larmor,
	// and a light armour keeps larmor because larmor IS its shape.
	// EVERY amulet has its own sheet since batch 32 (2026-09-13, RfA-14) - vanilla's included, which
	// tumbled as the ring. Keyed on the type rather than the cursor for the same reason the worn slots
	// are: it covers the plain bases, the classic uniques and the fork's 26 in one line. The Relic and
	// Reliquary bases are amulets too, but the UITYPE switch above already sent them to relicflip.
	if (item._itype == ItemType::Amulet)
		return OracoolAmuletDropAnim;

	const int8_t byCursor = GetItemDropAnimIndex(item._iCurs);
	if (item._iCurs < ICURS_ORACOOL_FIRST || byCursor != OracoolFallbackDropAnim)
		return byCursor;
	switch (item._itype) {
	case ItemType::Sword:
		return SwordDropAnimIndex;
	case ItemType::Axe:
		return AxeDropAnimIndex;
	case ItemType::Mace:
		return MaceDropAnimIndex;
	case ItemType::Bow:
		return BowDropAnimIndex;
	case ItemType::Staff:
		return StaffDropAnimIndex;
	case ItemType::Helm:
		return HelmDropAnimIndex;
	case ItemType::MediumArmor:
		return MediumArmorDropAnimIndex;
	case ItemType::HeavyArmor:
		return HeavyArmorDropAnimIndex;
	case ItemType::Shield:
		return ShieldDropAnimIndex;
	case ItemType::Ring:
		return RingDropAnimIndex;
	default:
		return byCursor;
	}
}

namespace {

OptionalOwnedClxSpriteList itemanims[ITEMTYPES];

enum class PlayerArmorGraphic : uint8_t {
	// clang-format off
	Light  = 0,
	Medium = 1 << 4,
	Heavy  = 1 << 5,
	// clang-format on
};


/**
 * @brief When set, RndPL returns the maximum end of its range instead of rolling, so every
 * affix magnitude applied while this is active comes out as a "perfect roll." Only ever set
 * for the duration of generating a single Primal item's affixes (GetTieredItemAffixes) - never
 * left set across a frame boundary, since RndPL is also used outside item generation.
 */
bool ForcePerfectAffixRoll = false;
/**
 * @brief A base tier SetupAllItems keeps instead of rolling one - set by RetierOracoolItem. The Cube's rerolls and crafts
 * rolled a fresh tier, so a Torment base came out of "Reroll Rares" as Hell or lower four times in ten, while Ennoble,
 * Recast and the Mystic kept it (round 14 audit, v1.12.239).
 */
std::optional<oracool::BaseItemTier> PinnedBaseTier;

/** Holds item get records, tracking items being recently looted. This is in an effort to prevent items being picked up more than once. */
ItemGetRecordStruct itemrecord[MAXITEMS];

bool itemhold[3][3];

/** Specifies the number of active item get records. */
int gnNumGetRecords;

int OilLevels[] = { 1, 10, 1, 10, 4, 1, 5, 17, 1, 10 };
int OilValues[] = { 500, 2500, 500, 2500, 1500, 100, 2500, 15000, 500, 2500 };
item_misc_id OilMagic[] = {
	IMISC_OILACC,
	IMISC_OILMAST,
	IMISC_OILSHARP,
	IMISC_OILDEATH,
	IMISC_OILSKILL,
	IMISC_OILBSMTH,
	IMISC_OILFORT,
	IMISC_OILPERM,
	IMISC_OILHARD,
	IMISC_OILIMP,
};
char OilNames[10][25] = {
	N_("Oil of Accuracy"),
	N_("Oil of Mastery"),
	N_("Oil of Sharpness"),
	N_("Oil of Death"),
	N_("Oil of Skill"),
	N_("Blacksmith Oil"),
	N_("Oil of Fortitude"),
	N_("Oil of Permanence"),
	N_("Oil of Hardening"),
	N_("Oil of Imperviousness")
};

/** Map of item type .cel file names. */
const char *const ItemDropNames[] = {
	"armor2",
	"axe",
	"fbttle",
	"bow",
	"goldflip",
	"helmut",
	"mace",
	"shield",
	"swrdflip",
	"rock",
	"cleaver",
	"staff",
	"ring",
	"crownf",
	"larmor",
	"wshield",
	"scroll",
	"fplatear",
	"fbook",
	"food",
	"fbttlebb",
	"fbttledy",
	"fbttleor",
	"fbttlebr",
	"fbttlebl",
	"fbttleby",
	"fbttlewh",
	"fbttledb",
	"fear",
	"fbrain",
	"fmush",
	"innsign",
	"bldstn",
	"fanvil",
	"flazstaf",
	"bombs1",
	"halfps1",
	"wholeps1",
	"runes1",
	"teddys1",
	"cows1",
	"donkys1",
	"mooses1",
	// Oracool: PNG only (items\<name>.png); without the file they load larmor's CEL - see InitItemGFX.
	"gemflip",
	"runeflip",
	"charmflip",
	"shardflip",
	"signetflip",
	"jewelflip",
	"salvageflip",
	"mapflip",
	// Batches 21 and 22 (2026-09-12): six worn slots, then six exotic bases. Order must match the
	// Oracool*DropAnim constants above, which is what ties a name to a sheet.
	"gloveflip",
	"bootflip",
	"bracerflip",
	"beltflip",
	"legflip",
	"shoulderflip",
	"cloakflip",
	"relicflip",
	"spearflip",
	"luteflip",
	"quiverflip",
	"focusflip",
	// Batch 32 (2026-09-13): the amulet.
	"amuletflip",
};
static_assert(sizeof(ItemDropNames) / sizeof(ItemDropNames[0]) == ITEMTYPES);
/** Maps of item drop animation length. */
int8_t ItemAnimLs[] = {
	15,
	13,
	16,
	13,
	10,
	13,
	13,
	13,
	13,
	10,
	13,
	13,
	13,
	13,
	13,
	13,
	13,
	13,
	13,
	1,
	16,
	16,
	16,
	16,
	16,
	16,
	16,
	16,
	13,
	12,
	12,
	13,
	13,
	13,
	8,
	10,
	16,
	16,
	10,
	10,
	15,
	15,
	15,
	// Oracool: the eight tumbles - 13 frames, the same as larmor, their fallback.
	13,
	13,
	13,
	13,
	13,
	13, // jewelflip
	13, // salvageflip
	13, // mapflip
	// Batches 21 and 22 (2026-09-12): all twelve are 13 frames, which is what keeps this change
	// SAVE-SAFE. An item already lying on a floor in a save stores its frame, and every tumble in
	// the game - larmor included - is 13, so re-pointing an item at a new sheet cannot land it on a
	// frame that does not exist. A sheet with a different count would need a migration.
	13, // gloveflip
	13, // bootflip
	13, // bracerflip
	13, // beltflip
	13, // legflip
	13, // shoulderflip
	13, // cloakflip
	13, // relicflip
	13, // spearflip
	13, // luteflip
	13, // quiverflip
	13, // focusflip
	13, // amuletflip - batch 32, 13 like every fork tumble
};
static_assert(sizeof(ItemAnimLs) / sizeof(ItemAnimLs[0]) == ITEMTYPES);
/** Maps of drop sounds effect of dropping the item on ground. */
_sfx_id ItemDropSnds[] = {
	IS_FHARM,
	IS_FAXE,
	IS_FPOT,
	IS_FBOW,
	IS_GOLD,
	IS_FCAP,
	IS_FSWOR,
	IS_FSHLD,
	IS_FSWOR,
	IS_FROCK,
	IS_FAXE,
	IS_FSTAF,
	IS_FRING,
	IS_FCAP,
	IS_FLARM,
	IS_FSHLD,
	IS_FSCRL,
	IS_FHARM,
	IS_FBOOK,
	IS_FLARM,
	IS_FPOT,
	IS_FPOT,
	IS_FPOT,
	IS_FPOT,
	IS_FPOT,
	IS_FPOT,
	IS_FPOT,
	IS_FPOT,
	IS_FBODY,
	IS_FBODY,
	IS_FMUSH,
	IS_FSIGN,
	IS_FBLST,
	IS_FANVL,
	IS_FSTAF,
	IS_FROCK,
	IS_FSCRL,
	IS_FSCRL,
	IS_FROCK,
	IS_FMUSH,
	IS_FHARM,
	IS_FLARM,
	IS_FLARM,
	// Oracool: the five tumbles - stones for gems and runes, the bloodstone for orbs, the ring for
	// charms and the signet.
	IS_FROCK,
	IS_FROCK,
	IS_FRING,
	IS_FBLST,
	IS_FRING,
	// Batch 15: jewels land like the gems, salvage materials like a soft pouch (larmor's cloth),
	// the sealed maps like a scroll.
	IS_FROCK, // jewelflip
	IS_FLARM, // salvageflip
	IS_FSCRL, // mapflip
	// Batches 21 and 22: the floor-drop half of the twelve. Mirrors ItemInvSnds above - the six worn
	// slots land like light armour, and the six exotic bases like what they are.
	IS_FLARM, // gloveflip
	IS_FLARM, // bootflip
	IS_FLARM, // bracerflip
	IS_FLARM, // beltflip
	IS_FLARM, // legflip
	IS_FLARM, // shoulderflip
	IS_FLARM, // cloakflip
	IS_FROCK, // relicflip
	IS_FHARM, // spearflip
	IS_FHARM, // luteflip
	IS_FLARM, // quiverflip
	IS_FBOOK, // focusflip
	IS_FRING, // amuletflip - batch 32, jewellery lands like the ring
};
static_assert(sizeof(ItemDropSnds) / sizeof(ItemDropSnds[0]) == ITEMTYPES);
static_assert(sizeof(ItemInvSnds) / sizeof(ItemInvSnds[0]) == ITEMTYPES);
// Oracool: vanilla's two premium quality-level tables (six entries for Diablo, fifteen for
// Hellfire) are gone. They mapped a SLOT NUMBER to a delta, so they could only ever describe a stock
// of exactly their own length, and Griswold's premium stock is thirty slots now. PremiumLevelDelta,
// below SpawnOnePremium, computes the same spread for any length.

bool IsPrefixValidForItemType(int i, AffixItemType flgs, bool hellfireItem)
{
	AffixItemType itemTypes = ItemPrefixes[i].PLIType;

	if (!hellfireItem) {
		if (i > 82)
			return false;

		if (i >= 12 && i <= 20)
			itemTypes &= ~AffixItemType::Staff;
	}

	return HasAnyOf(flgs, itemTypes);
}

bool IsSuffixValidForItemType(int i, AffixItemType flgs, bool hellfireItem)
{
	AffixItemType itemTypes = ItemSuffixes[i].PLIType;

	if (!hellfireItem) {
		if (i > 94)
			return false;

		if ((i >= 0 && i <= 1)
		    || (i >= 14 && i <= 15)
		    || (i >= 21 && i <= 22)
		    || (i >= 34 && i <= 36)
		    || (i >= 41 && i <= 44)
		    || (i >= 60 && i <= 63))
			itemTypes &= ~AffixItemType::Staff;
	}

	return HasAnyOf(flgs, itemTypes);
}

int ItemsGetCurrlevel()
{
	// THE AREA LEVEL now, not the floor number (user, 2026-08-19: alvl/mlvl/ilvl). Every item this
	// function feeds - chest contents, floor spawns, shop stock, quest rewards, gold piles - takes
	// its depth from here, so redefining this one function moves the whole non-monster half of
	// generation onto the ladder at once.
	//
	// Two things change with it. Difficulty now counts: this returned the same 1-24 in Torment as in
	// Normal, which is why a Hell chest could hold Cathedral loot. And Hellfire's fold is gone - it
	// mapped Nest back to 9-12 and Crypt to 14-17, correct when those were a PARALLEL path to the
	// Cathedral, wrong here where they are floors 17-24 and deeper than everything before them.
	return oracool::CurrentAreaLevel();
}

bool ItemPlace(Point position)
{
	if (dMonster[position.x][position.y] != 0)
		return false;
	if (dPlayer[position.x][position.y] != 0)
		return false;
	if (dItem[position.x][position.y] != 0)
		return false;
	if (IsObjectAtPosition(position))
		return false;
	if (TileContainsSetPiece(position))
		return false;
	if (IsTileSolid(position))
		return false;

	return true;
}

Point GetRandomAvailableItemPosition()
{
	Point position = {};
	do {
		position = Point { GenerateRnd(80), GenerateRnd(80) } + Displacement { 16, 16 };
	} while (!ItemPlace(position));

	return position;
}

void AddInitItems()
{
	int curlv = ItemsGetCurrlevel();
	int rnd = GenerateRnd(3) + 3;
	for (int j = 0; j < rnd; j++) {
		int ii = AllocateItem();
		auto &item = Items[ii];

		Point position = GetRandomAvailableItemPosition();
		item.position = position;

		dItem[position.x][position.y] = ii + 1;

		item._iSeed = AdvanceRndSeed();
		SetRndSeed(item._iSeed);

		GetItemAttrs(item, PickRandomlyAmong({ IDI_MANA, IDI_HEAL }), curlv);

		item._iCreateInfo = curlv | CF_PREGEN;
		SetupItem(item);
		item.AnimInfo.currentFrame = item.AnimInfo.numberOfFrames - 1;
		item._iAnimFlag = false;
		item._iSelFlag = 1;
		DeltaAddItem(ii);
	}
}

void SpawnNote()
{
	_item_indexes id;

	switch (currlevel) {
	case 22:
		id = IDI_NOTE2;
		break;
	case 23:
		id = IDI_NOTE3;
		break;
	default:
		id = IDI_NOTE1;
		break;
	}

	Point position = GetRandomAvailableItemPosition();
	SpawnQuestItem(id, position, 0, 1, false);
}

void CalcSelfItems(Player &player)
{
	// A broken (0-durability, left equipped rather than destroyed - see BreakOrRemoveEquipment) item
	// contributes nothing at all, the same as if it had been removed: its flag starts false, so no
	// provider below ever counts it.
	for (Item &equipment : EquippedPlayerItemsRange(player)) {
		// Broken means EMPTIED. An item with durability back - a repair path that forgot the flag, a
		// shrine, the Repair skill - is whole again (user, 2026-09-11: a shield at 16/16 "has X on it and
		// doesnt appear as shield when i equip it").
		if (equipment._iOracoolBroken && equipment._iDurability > 0)
			equipment._iOracoolBroken = false;
		equipment._iStatFlag = !equipment._iOracoolBroken;
	}

	// Oracool fix (user, 2026-09-11: "I have hit 112 STR but the axe is RED"): each worn item's
	// requirement is measured against the SAME stats the character sheet shows and CanUseItem equips
	// by - base plus every bonus provider (worn items, sockets, charms, the class tree, set bonuses,
	// Rage) - and after Hel's reduction (EffectiveRequirement). This used to sum only the worn items'
	// own stat bonuses and read the raw requirement, so strength from anywhere else let an item be
	// equipped that this pass then switched off and drew red.
	//
	// Still a loop, as vanilla's was: an item that fails loses its flag, which takes its bonuses (and
	// any set bonus it completed) out of the next total, and every remaining item is measured again.
	bool changeflag;
	do {
		oracool::ItemBonusTotals totals;
		oracool::AccumulateBonuses({ &player }, totals);
		const int currstr = std::max(0, totals.strength + player._pBaseStr);
		const int currmag = std::max(0, totals.magic + player._pBaseMag);
		const int currdex = std::max(0, totals.dexterity + player._pBaseDex);

		changeflag = false;
		for (Item &equipment : EquippedPlayerItemsRange(player)) {
			if (!equipment._iStatFlag)
				continue;
			// And the level (2026-09-20): the fourth requirement, asked here as in CanUseItem, or
			// this pass would switch a level-gated item back on.
			if (IsItemValid(equipment)
			    && currstr >= oracool::EffectiveRequirement(equipment, equipment._iMinStr)
			    && currmag >= oracool::EffectiveRequirement(equipment, equipment._iMinMag)
			    && currdex >= oracool::EffectiveRequirement(equipment, equipment._iMinDex)
			    && player._pLevel >= oracool::RequiredLevel(equipment)
			    && oracool::ClassMayUseItem(player, equipment)) // the class rule too, as CanUseItem has it (round 18)
				continue;
			changeflag = true;
			equipment._iStatFlag = false;
		}
	} while (changeflag);
}

bool GetItemSpace(Point position, int8_t inum)
{
	int xx = 0;
	int yy = 0;
	for (int j = position.y - 1; j <= position.y + 1; j++) {
		xx = 0;
		for (int i = position.x - 1; i <= position.x + 1; i++) {
			itemhold[xx][yy] = ItemSpaceOk({ i, j });
			xx++;
		}
		yy++;
	}

	bool savail = false;
	for (int j = 0; j < 3; j++) {
		for (int i = 0; i < 3; i++) { // NOLINT(modernize-loop-convert)
			if (itemhold[i][j])
				savail = true;
		}
	}

	int rs = GenerateRnd(15) + 1;

	if (!savail)
		return false;

	xx = 0;
	yy = 0;
	while (rs > 0) {
		if (itemhold[xx][yy])
			rs--;
		if (rs <= 0)
			continue;
		xx++;
		if (xx != 3)
			continue;
		xx = 0;
		yy++;
		if (yy == 3)
			yy = 0;
	}

	xx += position.x - 1;
	yy += position.y - 1;
	Items[inum].position = { xx, yy };
	dItem[xx][yy] = inum + 1;

	return true;
}

void GetSuperItemSpace(Point position, int8_t inum)
{
	Point positionToCheck = position;
	if (GetItemSpace(positionToCheck, inum))
		return;
	for (int k = 2; k < 50; k++) {
		for (int j = -k; j <= k; j++) {
			for (int i = -k; i <= k; i++) {
				Displacement offset = { i, j };
				positionToCheck = position + offset;
				if (!ItemSpaceOk(positionToCheck))
					continue;
				Items[inum].position = positionToCheck;
				dItem[positionToCheck.x][positionToCheck.y] = inum + 1;
				return;
			}
		}
	}
}

void CalcItemValue(Item &item)
{
	int v = item._iVMult1 + item._iVMult2;
	if (v > 0) {
		v *= item._ivalue;
	}
	if (v < 0) {
		v = item._ivalue / v;
	}
	v = item._iVAdd1 + item._iVAdd2 + v;
	item._iIvalue = std::max(v, 1);
}

void GetBookSpell(Item &item, int lvl)
{
	int rv;

	if (lvl == 0)
		lvl = 1;

	int maxSpells = gbIsHellfire ? MAX_ITEM_SPELLS : 37;

	rv = GenerateRnd(maxSpells) + 1;

	if (gbIsSpawn && lvl > 5)
		lvl = 5;

	// The book's own gate is the SPELL BAND now, measured against the item level (user, 2026-08-19).
	// @p lvl arrives halved - GetItemAttrs is called with lvl/2 - so the ilvl stamped a moment ago in
	// SetupAllItems is the honest number to compare against, with lvl*2 as the fallback for the
	// paths that build an item without one (InitializeItem, RecreateItem).
	const int ilvl = item._iOracoolItemLevel > 0 ? item._iOracoolItemLevel : lvl * 2;

	int s = static_cast<int16_t>(SpellID::Firebolt);
	SpellID bs = SpellID::Firebolt;
	while (rv > 0) {
		int sLevel = GetSpellBookLevel(static_cast<SpellID>(s));
		// The band gate first: a spell whose book is too deep for this floor is simply not a
		// candidate, whatever its vanilla sBookLvl says.
		if (oracool::SpellBookItemLevel(static_cast<SpellID>(s)) > ilvl)
			sLevel = -1;
		// Oracool: Town Portal is a built-in ability now (see oracool::IsBuiltInPortalAbility), so
		// its book would teach nothing - skipped here rather than by jumping the enum index the way
		// the multiplayer-only spells below are, since that would depend on enum adjacency.
		if (sLevel != -1 && lvl >= sLevel && !oracool::IsBuiltInPortalAbility(static_cast<SpellID>(s))) {
			rv--;
			bs = static_cast<SpellID>(s);
		}
		s++;
		// Oracool: user request (2026-08-15) - Resurrect gets a book, so the skip that made its book
		// undroppable had to go with it.
		//
		// Vanilla jumps the enum past Resurrect and Heal Other in single-player because both target
		// ANOTHER player, which in a solo game means they can never be cast. That reasoning is sound
		// and it is why the two are still skipped upstream - but this build is single-player only, so
		// under it those two spells could never be obtained at all while still being listed in the
		// book. That is the "listed but unlearnable" state Search and Etherealize were just cleaned
		// out of, so keeping it here would have re-created the thing we removed.
		//
		// Both are lifted together: they were one decision and share one justification. What they do
		// when cast solo is unchanged - this only decides whether their books can drop.
		if (s == maxSpells)
			s = 1;
	}
	const string_view spellName = GetSpellData(bs).sNameText;
	const size_t iNameLen = string_view(item._iName).size();
	const size_t iINameLen = string_view(item._iIName).size();
	CopyUtf8(item._iName + iNameLen, spellName, sizeof(item._iName) - iNameLen);
	CopyUtf8(item._iIName + iINameLen, spellName, sizeof(item._iIName) - iINameLen);
	item._iSpell = bs;
	const SpellData &spellData = GetSpellData(bs);
	item._iMinMag = spellData.minInt;
	item._ivalue += spellData.bookCost();
	item._iIvalue += spellData.bookCost();
	switch (spellData.type()) {
	case MagicType::Fire:
		item._iCurs = ICURS_BOOK_RED;
		break;
	case MagicType::Lightning:
		item._iCurs = ICURS_BOOK_BLUE;
		break;
	case MagicType::Magic:
		item._iCurs = ICURS_BOOK_GREY;
		break;
	case MagicType::Cold:
		// The blue book, as Lightning uses. Oracool: cold has no book art of its own, and blue at
		// least names the half of the palette the spell inside is drawn from - grey means "neither
		// fire nor lightning", which is less true than blue is. Nothing rolls a cold book today
		// (every cold row's sBookLvl is -1), so this branch is what keeps the switch total rather
		// than a decision anybody will see.
		item._iCurs = ICURS_BOOK_BLUE;
		break;
	}
}

int RndPL(int param1, int param2)
{
	if (ForcePerfectAffixRoll)
		return param2;
	return param1 + GenerateRnd(param2 - param1 + 1);
}

int CalculateToHitBonus(int level)
{
	// BRACKETS, not exact keys (2026-09-25). Vanilla keyed this on the affix row's minimum damage percent - the
	// eleven values below - and called app_fatal on anything else. The fork re-applies affixes from their stored
	// record (the Mystic's reroll, RebuildOracoolItemWithAffixes), and a record holds the ROLLED percent, so a
	// rerolled "King's" or "Warrior's" item arrived with, say, 57 and killed the game ("Unknown to hit bonus" -
	// found by the tooltip sweep test, which rerolls every base). Each value now takes the bracket it falls in;
	// the eleven exact keys land where they always did, with the same roll.
	if (level <= -50)
		return -RndPL(6, 10);
	if (level < 0)
		return -RndPL(1, 5);
	if (level >= 151)
		return RndPL(76, 100);
	if (level >= 126)
		return RndPL(51, 75);
	if (level >= 111)
		return RndPL(41, 50);
	if (level >= 96)
		return RndPL(31, 40);
	if (level >= 81)
		return RndPL(21, 30);
	if (level >= 66)
		return RndPL(16, 20);
	if (level >= 51)
		return RndPL(11, 15);
	if (level >= 36)
		return RndPL(6, 10);
	return RndPL(1, 5);
}

int SaveItemPower(const Player &player, Item &item, ItemPower &power)
{
	if (!gbIsHellfire) {
		if (power.type == IPL_TARGAC) {
			power.param1 = 1 << power.param1;
			power.param2 = 3 << power.param2;
		}
	}

	int r = RndPL(power.param1, power.param2);

	switch (power.type) {
	case IPL_TOHIT:
		item._iPLToHit += r;
		break;
	case IPL_TOHIT_CURSE:
		item._iPLToHit -= r;
		break;
	case IPL_DAMP:
		item._iPLDam += r;
		break;
	case IPL_DAMP_CURSE:
		item._iPLDam -= r;
		break;
	case IPL_DOPPELGANGER:
		item._iDamAcFlags |= ItemSpecialEffectHf::Doppelganger;
		[[fallthrough]];
	case IPL_TOHIT_DAMP:
		r = RndPL(power.param1, power.param2);
		item._iPLDam += r;
		item._iPLToHit += CalculateToHitBonus(power.param1);
		break;
	case IPL_TOHIT_DAMP_CURSE:
		item._iPLDam -= r;
		item._iPLToHit += CalculateToHitBonus(-power.param1);
		break;
	case IPL_ACP:
		item._iPLAC += r;
		break;
	case IPL_ACP_CURSE:
		item._iPLAC -= r;
		break;
	case IPL_SETAC:
		item._iAC = r;
		break;
	case IPL_AC_CURSE:
		item._iAC -= r;
		break;
	case IPL_FIRERES:
		item._iPLFR += r;
		break;
	case IPL_LIGHTRES:
		item._iPLLR += r;
		break;
	case IPL_MAGICRES:
		item._iPLMR += r;
		break;
	case IPL_COLDRES:
		item._iPLCR += r;
		break;
	case IPL_ALLRES:
		// All four since 2026-09-26: "all resistances" includes cold, as in Diablo II.
		item._iPLFR = std::max(item._iPLFR + r, 0);
		item._iPLLR = std::max(item._iPLLR + r, 0);
		item._iPLMR = std::max(item._iPLMR + r, 0);
		item._iPLCR = std::max(item._iPLCR + r, 0);
		break;
	case IPL_SPLLVLADD:
		item._iSplLvlAdd = r;
		break;
	case IPL_CHARGES:
		item._iCharges *= power.param1;
		item._iMaxCharges = item._iCharges;
		break;
	case IPL_SPELL:
		item._iSpell = static_cast<SpellID>(power.param1);
		item._iCharges = power.param2;
		item._iMaxCharges = power.param2;
		break;
	case IPL_FIREDAM:
		item._iFlags |= ItemSpecialEffect::FireDamage;
		item._iFlags &= ~ItemSpecialEffect::LightningDamage;
		item._iFMinDam = power.param1;
		item._iFMaxDam = power.param2;
		item._iLMinDam = 0;
		item._iLMaxDam = 0;
		break;
	case IPL_LIGHTDAM:
		item._iFlags |= ItemSpecialEffect::LightningDamage;
		item._iFlags &= ~ItemSpecialEffect::FireDamage;
		item._iLMinDam = power.param1;
		item._iLMaxDam = power.param2;
		item._iFMinDam = 0;
		item._iFMaxDam = 0;
		break;
	case IPL_STR:
		item._iPLStr += r;
		break;
	case IPL_STR_CURSE:
		item._iPLStr -= r;
		break;
	case IPL_MAG:
		item._iPLMag += r;
		break;
	case IPL_MAG_CURSE:
		item._iPLMag -= r;
		break;
	case IPL_DEX:
		item._iPLDex += r;
		break;
	case IPL_DEX_CURSE:
		item._iPLDex -= r;
		break;
	case IPL_VIT:
		item._iPLVit += r;
		break;
	case IPL_VIT_CURSE:
		item._iPLVit -= r;
		break;
	case IPL_ATTRIBS:
		item._iPLStr += r;
		item._iPLMag += r;
		item._iPLDex += r;
		item._iPLVit += r;
		break;
	case IPL_ATTRIBS_CURSE:
		item._iPLStr -= r;
		item._iPLMag -= r;
		item._iPLDex -= r;
		item._iPLVit -= r;
		break;
	case IPL_GETHIT_CURSE:
		item._iPLGetHit += r;
		break;
	case IPL_GETHIT:
		item._iPLGetHit -= r;
		break;
	case IPL_LIFE:
		item._iPLHP += r << 6;
		break;
	case IPL_LIFE_CURSE:
		item._iPLHP -= r << 6;
		break;
	case IPL_MANA:
		item._iPLMana += r << 6;
		RedrawComponent(PanelDrawComponent::Mana);
		break;
	case IPL_MANA_CURSE:
		item._iPLMana -= r << 6;
		RedrawComponent(PanelDrawComponent::Mana);
		break;
	case IPL_DUR: {
		// An indestructible item stays so: "the ages" rolled before a durability row came back 254/254 and wore down (round 29
		// audit). Not by keeping them apart in the roll: that changes what existing seeds rebuild into (pack_test's goldens).
		if (item._iMaxDur == DUR_INDESTRUCTIBLE || item._iDurability == DUR_INDESTRUCTIBLE) // Zod's stamp too
			break;
		int bonus = r * item._iMaxDur / 100;
		// Capped just below DUR_INDESTRUCTIBLE. _iMaxDur is an int here but a BYTE in the packed
		// item record (pack.cpp's bMDur), so a +200% affix on a hardy base could push it past 255
		// and come back off the pack as a near-zero - and 255 itself is the "cannot break"
		// sentinel, which a durability affix has no business granting.
		item._iMaxDur = std::min(item._iMaxDur + bonus, DUR_INDESTRUCTIBLE - 1);
		item._iDurability = std::min(item._iDurability + bonus, item._iMaxDur);
	} break;
	case IPL_CRYSTALLINE:
		item._iPLDam += 140 + r * 2;
		[[fallthrough]];
	case IPL_DUR_CURSE:
		if (item._iMaxDur == DUR_INDESTRUCTIBLE || item._iDurability == DUR_INDESTRUCTIBLE)
			break; // as IPL_DUR: "the ages" holds (round 29 audit); Crystalline's damage above still applies
		item._iMaxDur -= r * item._iMaxDur / 100;
		// std::max<uint8_t> until 2026-08-27, which truncated the int field to its low eight bits
		// before comparing: a curse harsher than 100% left _iMaxDur negative, and -60 came back as
		// 196 - a durability CURSE that handed out durability, with the clamp-to-1 it was written
		// for never reached.
		item._iMaxDur = std::max(item._iMaxDur, 1);
		item._iDurability = item._iMaxDur;
		break;
	case IPL_INDESTRUCTIBLE:
		item._iDurability = DUR_INDESTRUCTIBLE;
		item._iMaxDur = DUR_INDESTRUCTIBLE;
		break;
	case IPL_LIGHT:
		item._iPLLight += power.param1;
		break;
	case IPL_LIGHT_CURSE:
		item._iPLLight -= power.param1;
		break;
	case IPL_MULT_ARROWS:
		item._iFlags |= ItemSpecialEffect::MultipleArrows;
		break;
	case IPL_FIRE_ARROWS:
		item._iFlags |= ItemSpecialEffect::FireArrows;
		item._iFlags &= ~ItemSpecialEffect::LightningArrows;
		item._iFMinDam = power.param1;
		item._iFMaxDam = power.param2;
		item._iLMinDam = 0;
		item._iLMaxDam = 0;
		break;
	case IPL_LIGHT_ARROWS:
		item._iFlags |= ItemSpecialEffect::LightningArrows;
		item._iFlags &= ~ItemSpecialEffect::FireArrows;
		item._iLMinDam = power.param1;
		item._iLMaxDam = power.param2;
		item._iFMinDam = 0;
		item._iFMaxDam = 0;
		break;
	case IPL_FIREBALL:
		item._iFlags |= (ItemSpecialEffect::LightningArrows | ItemSpecialEffect::FireArrows);
		item._iFMinDam = power.param1;
		item._iFMaxDam = power.param2;
		item._iLMinDam = 0;
		item._iLMaxDam = 0;
		break;
	case IPL_THORNS:
		item._iFlags |= ItemSpecialEffect::Thorns;
		break;
	case IPL_NOMANA:
		item._iFlags |= ItemSpecialEffect::NoMana;
		RedrawComponent(PanelDrawComponent::Mana);
		break;
	case IPL_ABSHALFTRAP:
		item._iFlags |= ItemSpecialEffect::HalfTrapDamage;
		break;
	case IPL_KNOCKBACK:
		item._iFlags |= ItemSpecialEffect::Knockback;
		break;
	case IPL_3XDAMVDEM:
		item._iFlags |= ItemSpecialEffect::TripleDemonDamage;
		break;
	case IPL_ALLRESZERO:
		item._iFlags |= ItemSpecialEffect::ZeroResistance;
		break;
	case IPL_STEALMANA:
		if (power.param1 == 3)
			item._iFlags |= ItemSpecialEffect::StealMana3;
		if (power.param1 == 5)
			item._iFlags |= ItemSpecialEffect::StealMana5;
		RedrawComponent(PanelDrawComponent::Mana);
		break;
	case IPL_STEALLIFE:
		if (power.param1 == 3)
			item._iFlags |= ItemSpecialEffect::StealLife3;
		if (power.param1 == 5)
			item._iFlags |= ItemSpecialEffect::StealLife5;
		RedrawComponent(PanelDrawComponent::Health);
		break;
	case IPL_TARGAC:
		if (gbIsHellfire)
			item._iPLEnAc = power.param1;
		else
			item._iPLEnAc += r;
		break;
	case IPL_FASTATTACK:
		if (power.param1 == 1)
			item._iFlags |= ItemSpecialEffect::QuickAttack;
		if (power.param1 == 2)
			item._iFlags |= ItemSpecialEffect::FastAttack;
		if (power.param1 == 3)
			item._iFlags |= ItemSpecialEffect::FasterAttack;
		if (power.param1 == 4)
			item._iFlags |= ItemSpecialEffect::FastestAttack;
		break;
	case IPL_FASTRECOVER:
		if (power.param1 == 1)
			item._iFlags |= ItemSpecialEffect::FastHitRecovery;
		if (power.param1 == 2)
			item._iFlags |= ItemSpecialEffect::FasterHitRecovery;
		if (power.param1 == 3)
			item._iFlags |= ItemSpecialEffect::FastestHitRecovery;
		break;
	case IPL_FASTBLOCK:
		item._iFlags |= ItemSpecialEffect::FastBlock;
		break;
	case IPL_DAMMOD:
		item._iPLDamMod += r;
		break;
	case IPL_RNDARROWVEL:
		item._iFlags |= ItemSpecialEffect::RandomArrowVelocity;
		break;
	case IPL_SETDAM:
		item._iMinDam = power.param1;
		item._iMaxDam = power.param2;
		break;
	case IPL_SETDUR:
		item._iDurability = power.param1;
		item._iMaxDur = power.param1;
		break;
	case IPL_ONEHAND:
		item._iLoc = ILOC_ONEHAND;
		break;
	case IPL_DRAINLIFE:
		item._iFlags |= ItemSpecialEffect::DrainLife;
		break;
	case IPL_RNDSTEALLIFE:
		item._iFlags |= ItemSpecialEffect::RandomStealLife;
		break;
	case IPL_NOMINSTR:
		item._iMinStr = 0;
		break;
	case IPL_INVCURS:
		item._iCurs = power.param1;
		break;
	case IPL_ADDACLIFE:
		item._iFlags |= (ItemSpecialEffect::LightningArrows | ItemSpecialEffect::FireArrows);
		item._iFMinDam = power.param1;
		item._iFMaxDam = power.param2;
		item._iLMinDam = 1;
		item._iLMaxDam = 0;
		break;
	case IPL_ADDMANAAC:
		item._iFlags |= (ItemSpecialEffect::LightningDamage | ItemSpecialEffect::FireDamage);
		item._iFMinDam = power.param1;
		item._iFMaxDam = power.param2;
		item._iLMinDam = 2;
		item._iLMaxDam = 0;
		break;
	case IPL_FIRERES_CURSE:
		item._iPLFR -= r;
		break;
	case IPL_LIGHTRES_CURSE:
		item._iPLLR -= r;
		break;
	case IPL_MAGICRES_CURSE:
		item._iPLMR -= r;
		break;
	case IPL_COLDRES_CURSE:
		item._iPLCR -= r;
		break;
	case IPL_DEVASTATION:
		item._iDamAcFlags |= ItemSpecialEffectHf::Devastation;
		break;
	case IPL_DECAY:
		item._iDamAcFlags |= ItemSpecialEffectHf::Decay;
		item._iPLDam += r;
		break;
	case IPL_PERIL:
		item._iDamAcFlags |= ItemSpecialEffectHf::Peril;
		break;
	case IPL_JESTERS:
		item._iDamAcFlags |= ItemSpecialEffectHf::Jesters;
		break;
	case IPL_ACDEMON:
		item._iDamAcFlags |= ItemSpecialEffectHf::ACAgainstDemons;
		break;
	case IPL_ACUNDEAD:
		item._iDamAcFlags |= ItemSpecialEffectHf::ACAgainstUndead;
		break;
	case IPL_MANATOLIFE: {
		int portion = ((player._pMaxManaBase >> 6) * 50 / 100) << 6;
		item._iPLMana -= portion;
		item._iPLHP += portion;
	} break;
	case IPL_LIFETOMANA: {
		int portion = ((player._pMaxHPBase >> 6) * 40 / 100) << 6;
		item._iPLHP -= portion;
		item._iPLMana += portion;
	} break;
	case IPL_GOLDFIND:
		// Additive percent, read by the drop tail exactly as the charms' figure is. See
		// Item::_iPLGoldFind for why this channel had to be opened at all.
		item._iPLGoldFind += r;
		break;
	case IPL_MAGICFIND:
		// The twin, and additive for the same reason - ItemBonusTotals sums it with the charms'
		// figure and the drop tail reads the total as one percentage.
		item._iPLMagicFind += r;
		break;
	case IPL_MOVESPEED:
		item._iPLMoveSpeed += r;
		break;
	case IPL_MOVESPEED_CURSE:
		item._iPLMoveSpeed -= r;
		break;
	case IPL_FASTCAST:
		item._iPLFastCast += r;
		break;
	default:
		break;
	}

	return r;
}

bool StringInPanel(const char *str)
{
	return GetLineWidth(str, GameFont12, 2) < 254;
}

int PLVal(int pv, int p1, int p2, int minv, int maxv)
{
	if (p1 == p2)
		return minv;
	if (minv == maxv)
		return minv;
	return minv + (maxv - minv) * (100 * (pv - p1) / (p2 - p1)) / 100;
}

/**
 * @brief Rolls one affix onto a magic item: its stat into the item's fields, its price into the _iVAdd/_iVMult
 * pair, and the affix itself - type, rolled value, price multiplier - onto the item's one affix list.
 *
 * The list entry is the part that is new (2026-09-25, user: "we dont separate affixes into prefix/sufix
 * anymore ... she must be able to reroll all affixes"). This used to price the affix and leave the caller
 * to note its TYPE in a vanilla prefix or suffix field, which kept no rolled value, so Gillian's Reroll -
 * which works from the list - never saw a magic item's affixes at all. Every roller that goes through here
 * now records the same entry a tiered item's affixes always had. Refuses (nothing applied) when the list is
 * full, so an affix is never applied that the item cannot list.
 */
void SaveItemAffix(const Player &player, Item &item, const PLStruct &affix)
{
	if (item._iOracoolAffixCount >= Item::MaxOracoolAffixes)
		return;
	auto power = affix.power;
	const int raw = SaveItemPower(player, item, power);

	const int value = PLVal(raw, power.param1, power.param2, affix.minVal, affix.maxVal);
	if (item._iVAdd1 != 0 || item._iVMult1 != 0) {
		item._iVAdd2 = value;
		item._iVMult2 = affix.multVal;
	} else {
		item._iVAdd1 = value;
		item._iVMult1 = affix.multVal;
	}
	item._iOracoolAffixes[item._iOracoolAffixCount++] = OracoolAffix { affix.power.type, raw, affix.multVal };
}

int GetStaffPrefixId(int lvl, bool onlygood, bool hellfireItem)
{
	int preidx = -1;
	if (FlipCoin(10) || onlygood) {
		int nl = 0;
		int l[256];
		for (int j = 0; ItemPrefixes[j].power.type != IPL_INVALID; j++) {
			if (!IsPrefixValidForItemType(j, AffixItemType::Staff, hellfireItem) || ItemPrefixes[j].PLMinLvl > lvl)
				continue;
			if (onlygood && !ItemPrefixes[j].PLOk)
				continue;
			l[nl] = j;
			nl++;
			if (ItemPrefixes[j].PLDouble) {
				l[nl] = j;
				nl++;
			}
		}
		if (nl != 0) {
			preidx = l[GenerateRnd(nl)];
		}
	}
	return preidx;
}

std::string GenerateStaffName(const ItemData &baseItemData, SpellID spellId, bool translate)
{
	string_view baseName = translate ? _(baseItemData.iName) : baseItemData.iName;
	string_view spellName = translate ? pgettext("spell", GetSpellData(spellId).sNameText) : GetSpellData(spellId).sNameText;
	string_view normalFmt = translate ? pgettext("spell", /* TRANSLATORS: Constructs item names. Format: {Item} of {Spell}. Example: War Staff of Firewall */ "{0} of {1}") : "{0} of {1}";
	std::string name = fmt::format(fmt::runtime(normalFmt), baseName, spellName);
	// Only when the base HAS a short name (2026-09-25): the starting staves carry none (nullptr in the table), and
	// a long name falling back to it handed a null string to the translator and faulted - found by the tooltip
	// sweep test, which names every base. A name that runs long is better than no item.
	if (baseItemData.iSName != nullptr && !StringInPanel(name.c_str())) {
		string_view shortName = translate ? _(baseItemData.iSName) : baseItemData.iSName;
		name = fmt::format(fmt::runtime(normalFmt), shortName, spellName);
	}
	return name;
}

std::string GenerateStaffNameMagical(const ItemData &baseItemData, SpellID spellId, int preidx, bool translate, std::optional<bool> forceNameLengthCheck)
{
	string_view baseName = translate ? _(baseItemData.iName) : baseItemData.iName;
	string_view magicFmt = translate ? pgettext("spell", /* TRANSLATORS: Constructs item names. Format: {Prefix} {Item} of {Spell}. Example: King's War Staff of Firewall */ "{0} {1} of {2}") : "{0} {1} of {2}";
	string_view spellName = translate ? pgettext("spell", GetSpellData(spellId).sNameText) : GetSpellData(spellId).sNameText;
	string_view prefixName = translate ? _(ItemPrefixes[preidx].PLName) : ItemPrefixes[preidx].PLName;

	std::string identifiedName = fmt::format(fmt::runtime(magicFmt), prefixName, baseName, spellName);
	if (baseItemData.iSName != nullptr && (forceNameLengthCheck ? *forceNameLengthCheck : !StringInPanel(identifiedName.c_str()))) { // no short name: keep the long one (2026-09-25)
		string_view shortName = translate ? _(baseItemData.iSName) : baseItemData.iSName;
		identifiedName = fmt::format(fmt::runtime(magicFmt), prefixName, shortName, spellName);
	}
	return identifiedName;
}

void GetStaffPower(const Player &player, Item &item, int lvl, SpellID bs, bool onlygood)
{
	// The item-level ceiling, as DrawUnifiedAffix keeps it (audit, 2026-09-27): a staff's spell prefix was picked by the
	// ROLL level alone, which chests, vendors, Wirt and unique monsters set above the stamped ilvl - a floor-5 staff
	// could carry a level-10 prefix. Fresh generation only, so a stored seed still rebuilds to what it was.
	const bool lowerToItemLevel = !ReplayingStoredItemSeed && item._iOracoolItemLevel > 0;
	const int ceiling = lowerToItemLevel ? std::min(lvl, static_cast<int>(item._iOracoolItemLevel)) : lvl;
	int preidx = GetStaffPrefixId(ceiling, onlygood, gbIsHellfire);
	if (preidx != -1) {
		item._iMagical = ITEM_QUALITY_MAGIC;
		// Onto the one affix list like every other affix (2026-09-25) - SaveItemAffix records it there.
		SaveItemAffix(player, item, ItemPrefixes[preidx]);
	}

	const ItemData &baseItemData = AllItemsList[item.IDidx];
	std::string staffName = GenerateStaffName(baseItemData, item._iSpell, false);

	CopyUtf8(item._iName, staffName, sizeof(item._iName));
	if (preidx != -1) {
		// THE NAME POOL, like every other rolled item (audit finding #14, 2026-09-13). This was vanilla's
		// "{Prefix} {Staff} of {Spell}" - the one magic name still built out of its affix after the pool
		// arrived. The spell is not lost: the unidentified name keeps it, and the tooltip lists it.
		CopyUtf8(item._iIName, oracool::GenerateOracoolItemName(item._iSeed), sizeof(item._iIName));
	} else {
		CopyUtf8(item._iIName, item._iName, sizeof(item._iIName));
	}

	CalcItemValue(item);
}

std::string GenerateMagicItemName(const string_view &baseNamel, const PLStruct *pPrefix, const PLStruct *pSufix, bool translate)
{
	if (pPrefix != nullptr && pSufix != nullptr) {
		string_view fmt = translate ? _(/* TRANSLATORS: Constructs item names. Format: {Prefix} {Item} of {Suffix}. Example: King's Long Sword of the Whale */ "{0} {1} of {2}") : "{0} {1} of {2}";
		return fmt::format(fmt::runtime(fmt), translate ? _(pPrefix->PLName) : pPrefix->PLName, baseNamel, translate ? _(pSufix->PLName) : pSufix->PLName);
	} else if (pPrefix != nullptr) {
		string_view fmt = translate ? _(/* TRANSLATORS: Constructs item names. Format: {Prefix} {Item}. Example: King's Long Sword */ "{0} {1}") : "{0} {1}";
		return fmt::format(fmt::runtime(fmt), translate ? _(pPrefix->PLName) : pPrefix->PLName, baseNamel);
	} else if (pSufix != nullptr) {
		string_view fmt = translate ? _(/* TRANSLATORS: Constructs item names. Format: {Item} of {Suffix}. Example: Long Sword of the Whale */ "{0} of {1}") : "{0} of {1}";
		return fmt::format(fmt::runtime(fmt), baseNamel, translate ? _(pSufix->PLName) : pSufix->PLName);
	}

	return std::string(baseNamel);
}

namespace {

bool FitsMovementSpeed(const Item &item)
{
	// Where the drop tail put it since 2026-09-07: worn armour of every kind, rings and amulets.
	return item._iClass == ICLASS_ARMOR || item._itype == ItemType::Ring || item._itype == ItemType::Amulet;
}

bool FitsTrinketFastCast(const Item &item)
{
	return item._itype == ItemType::Ring || item._itype == ItemType::Amulet || item._itype == ItemType::Helm;
}

bool FitsStaffFastCast(const Item &item)
{
	return item._itype == ItemType::Staff;
}

/**
 * @brief Armour of every kind (shields and helms included) and jewellery - NOT weapons, unlike the vanilla
 * resistance rows. A weapon rolls from its seed through this pool, and the vanilla weapons' seeds must go on
 * rebuilding the items they always did (pack_test pins them); a resistance belongs on what you wear anyway.
 */
bool FitsColdResist(const Item &item)
{
	return item._iClass == ICLASS_ARMOR || item._itype == ItemType::Ring || item._itype == ItemType::Amulet;
}

/** @brief One row of OracoolPoolRows: an affix row, and which items may carry it. */
struct OracoolPoolRow {
	PLStruct row;
	bool (*fits)(const Item &item);
};

/**
 * @brief The affixes that are not rows of the vanilla prefix and suffix tables.
 *
 * Movement Speed and Faster Cast used to be rolled on the drop tail, after the item was finished, and
 * so sat outside every affix limit (user report, 2026-09-13: a magic helm with four affixes). Once
 * every bonus is an affix (user, 2026-09-13: "We call all possible item bonuses affixes"), they belong
 * in the pool with the rest - these rows are that.
 *
 * NOT appended to ItemPrefixes or ItemSuffixes. Those tables' INDICES are load-bearing: the non-Hellfire
 * gating in IsPrefixValidForItemType is written in index ranges, the staff prefix roll picks by index,
 * and RepairOracoolAffixesIfCorrupted walks them. A pool entry needs none of that.
 *
 * Banded by level like the vanilla rows, so a deep find outpaces a shallow one, and priced like the
 * life rows of similar strength. The curse rows are priced like vanilla's curses - no value, a
 * negative multiplier, and PLOk false so an only-good roll never takes one.
 */
const OracoolPoolRow OracoolPoolRows[] = {
	// clang-format off
	{ { N_("swiftness"),   { IPL_MOVESPEED,       10, 13 },  1, AffixItemType::None, GOE_ANY, false, true,    100,  1000,  2 }, FitsMovementSpeed },
	{ { N_("swiftness"),   { IPL_MOVESPEED,       12, 16 },  5, AffixItemType::None, GOE_ANY, false, true,   1100,  2000,  3 }, FitsMovementSpeed },
	{ { N_("swiftness"),   { IPL_MOVESPEED,       15, 19 }, 12, AffixItemType::None, GOE_ANY, false, true,   2100,  4000,  5 }, FitsMovementSpeed },
	{ { N_("swiftness"),   { IPL_MOVESPEED,       18, 23 }, 20, AffixItemType::None, GOE_ANY, false, true,   4100,  6000,  7 }, FitsMovementSpeed },
	{ { N_("swiftness"),   { IPL_MOVESPEED,       21, 26 }, 30, AffixItemType::None, GOE_ANY, false, true,   6100, 10000,  9 }, FitsMovementSpeed },
	{ { N_("swiftness"),   { IPL_MOVESPEED,       25, 30 }, 45, AffixItemType::None, GOE_ANY, false, true,  10100, 15000, 11 }, FitsMovementSpeed },
	{ { N_("lead"),        { IPL_MOVESPEED_CURSE, 10, 14 },  1, AffixItemType::None, GOE_ANY, false, false,     0,     0, -2 }, FitsMovementSpeed },
	{ { N_("lead"),        { IPL_MOVESPEED_CURSE, 13, 17 }, 12, AffixItemType::None, GOE_ANY, false, false,     0,     0, -3 }, FitsMovementSpeed },
	{ { N_("lead"),        { IPL_MOVESPEED_CURSE, 16, 20 }, 30, AffixItemType::None, GOE_ANY, false, false,     0,     0, -4 }, FitsMovementSpeed },
	{ { N_("incantation"), { IPL_FASTCAST,         5,  7 },  1, AffixItemType::None, GOE_ANY, false, true,    100,  1000,  2 }, FitsTrinketFastCast },
	{ { N_("incantation"), { IPL_FASTCAST,         6,  8 },  5, AffixItemType::None, GOE_ANY, false, true,   1100,  2000,  3 }, FitsTrinketFastCast },
	{ { N_("incantation"), { IPL_FASTCAST,         7, 10 }, 12, AffixItemType::None, GOE_ANY, false, true,   2100,  4000,  5 }, FitsTrinketFastCast },
	{ { N_("incantation"), { IPL_FASTCAST,         9, 11 }, 20, AffixItemType::None, GOE_ANY, false, true,   4100,  6000,  7 }, FitsTrinketFastCast },
	{ { N_("incantation"), { IPL_FASTCAST,        10, 13 }, 30, AffixItemType::None, GOE_ANY, false, true,   6100, 10000,  9 }, FitsTrinketFastCast },
	{ { N_("incantation"), { IPL_FASTCAST,        12, 15 }, 45, AffixItemType::None, GOE_ANY, false, true,  10100, 15000, 11 }, FitsTrinketFastCast },
	{ { N_("incantation"), { IPL_FASTCAST,        10, 13 },  1, AffixItemType::None, GOE_ANY, false, true,    100,  1000,  2 }, FitsStaffFastCast },
	{ { N_("incantation"), { IPL_FASTCAST,        12, 16 },  5, AffixItemType::None, GOE_ANY, false, true,   1100,  2000,  3 }, FitsStaffFastCast },
	{ { N_("incantation"), { IPL_FASTCAST,        15, 19 }, 12, AffixItemType::None, GOE_ANY, false, true,   2100,  4000,  5 }, FitsStaffFastCast },
	{ { N_("incantation"), { IPL_FASTCAST,        18, 23 }, 20, AffixItemType::None, GOE_ANY, false, true,   4100,  6000,  7 }, FitsStaffFastCast },
	{ { N_("incantation"), { IPL_FASTCAST,        21, 26 }, 30, AffixItemType::None, GOE_ANY, false, true,   6100, 10000,  9 }, FitsStaffFastCast },
	{ { N_("incantation"), { IPL_FASTCAST,        25, 30 }, 45, AffixItemType::None, GOE_ANY, false, true,  10100, 15000, 11 }, FitsStaffFastCast },
	// COLD RESISTANCE (2026-09-26, user: "make it as real as it is in Diablo 2"): the fire and lightning rows'
	// own bands, levels and prices, so the new element is exactly as common and as strong as the old ones.
	{ { N_("warmth"),      { IPL_COLDRES,         10, 20 },  4, AffixItemType::None, GOE_ANY, false, true,    500,  1500,  2 }, FitsColdResist },
	{ { N_("warmth"),      { IPL_COLDRES,         21, 30 }, 10, AffixItemType::None, GOE_ANY, false, true,   2100,  3000,  2 }, FitsColdResist },
	{ { N_("warmth"),      { IPL_COLDRES,         31, 40 }, 16, AffixItemType::None, GOE_ANY, false, true,   3100,  4000,  2 }, FitsColdResist },
	{ { N_("warmth"),      { IPL_COLDRES,         41, 50 }, 20, AffixItemType::None, GOE_ANY, false, true,   8200, 12000,  3 }, FitsColdResist },
	{ { N_("warmth"),      { IPL_COLDRES,         51, 60 }, 26, AffixItemType::None, GOE_ANY, false, true,  17100, 20000,  5 }, FitsColdResist },
	// clang-format on
};

/**
 * @brief Which DATA table holds a drawn affix's row - only so RowOf can find the row. Never stored and never
 * a property of the item: every affix lands on the one list whatever its table (2026-09-25).
 */
enum class AffixSource : uint8_t {
	Prefix,
	Suffix,
	Oracool,
};

struct AffixCandidate {
	AffixSource source;
	int index;
};

const PLStruct &RowOf(AffixCandidate candidate)
{
	switch (candidate.source) {
	case AffixSource::Prefix:
		return ItemPrefixes[candidate.index];
	case AffixSource::Suffix:
		return ItemSuffixes[candidate.index];
	case AffixSource::Oracool:
		break;
	}
	return OracoolPoolRows[candidate.index].row;
}

/**
 * @brief One draw from the unified affix pool: the prefix table, the suffix table and OracoolPoolRows.
 *
 * Every tier rolls through here (user, 2026-09-13: "an item can have any combo of them within its limit
 * of affixes"). The eligibility rules are the ones both old rollers applied, applied once: item type,
 * level band, only-good, the running good/evil theme, and no power type twice.
 *
 * @param room Whether the item's affix list has a free place. ONE flag since 2026-09-25: there were three - one
 *        per table - from when a magic item stored a table's affix in its own vanilla field, and every caller
 *        passed the same answer to all three once storage became one list (user: "all afixes are now one pool").
 */
std::optional<AffixCandidate> DrawUnifiedAffix(const Item &item, int minlvl, int maxlvl, AffixItemType flgs, bool onlygood,
    bool hellfireItem, bool ignoreLevelLimits, bool room, const item_effect_type *picked, int pickedCount, goodorevil goe)
{
	if (!room)
		return std::nullopt;
	const auto eligibleIgnoringLevel = [&](const PLStruct &row) {
		if (onlygood && !row.PLOk)
			return false;
		if ((goe == GOE_GOOD && row.PLGOE == GOE_EVIL) || (goe == GOE_EVIL && row.PLGOE == GOE_GOOD))
			return false;
		for (int k = 0; k < pickedCount; k++) {
			if (picked[k] == row.power.type || picked[k] == AffixTwinOf(row.power.type))
				return false;
			// Fire and lightning weapon damage share one slot of fields - each power zeroes the other's - so a Flaming
			// Lightning sword paid for two and dealt one, printing "Fire hit damage: 0" (round 8 audit, v1.12.233).
			if ((IsAnyOf(picked[k], IPL_FIREDAM, IPL_LIGHTDAM) && IsAnyOf(row.power.type, IPL_FIREDAM, IPL_LIGHTDAM))
			    || (IsAnyOf(picked[k], IPL_FIRE_ARROWS, IPL_LIGHT_ARROWS) && IsAnyOf(row.power.type, IPL_FIRE_ARROWS, IPL_LIGHT_ARROWS)))
				return false;
		}
		return true;
	};
	// THE ITEM-LEVEL CEILING (user, 2026-09-13: "all items including oracool invented ones should abide the
	// ilvl-affix level corelation"). No affix whose level is above the item's may be drawn - by any tier, any
	// source, any caller. The ceiling is the roll's own maxlvl, lowered to the item's stamped ilvl when it has
	// one: a chest, barrel or Wirt item rolls at twice its displayed level, and without this its affixes could
	// reach twice what its tooltip says.
	//
	// ignoreLevelLimits used to lift BOTH bounds, so a Rare's guaranteed affixes, a Primal's six, a Magic Find
	// upgrade and the forced-tier shelves could put a level-60 Strange or Merciless on a floor-1 drop. Nobody
	// asked for that; it was how the count guarantee was kept. It now relaxes only the FLOOR - a narrow item
	// class at low level may take a gentler affix to fill its count - and never the ceiling.
	//
	// FRESH GENERATION ONLY for the ilvl lowering. A rebuild from a stored seed (RecreateItem: the multiplayer
	// pack, the hero-select preview) must reach the window the item was generated with, or the same seed comes
	// back as a different item - pack_test's reference items pin exactly that. Single-player loads read the full
	// record and never rebuild, so every item a player actually holds keeps what it rolled.
	const bool lowerToItemLevel = !ReplayingStoredItemSeed && item._iOracoolItemLevel > 0;
	const int ceiling = lowerToItemLevel ? std::min(maxlvl, static_cast<int>(item._iOracoolItemLevel)) : maxlvl;
	// Capped at 25, as GetItemBonus caps it: the jewellery tables stop at level 30, so from item level 62 a floor of 31
	// left a ring's bonus affixes only Movement Speed and Faster Cast - and a Reroll on a ring holding both offered
	// nothing, after the gold (round 14 audit, v1.12.239).
	// The cap for jewellery only, whose tables it was for; other gear keeps its window (round 15 audit: the cap had let
	// level-25 rows onto ilvl-90 weapons).
	const bool jewellery = item._itype == ItemType::Ring || item._itype == ItemType::Amulet;
	const int floorLevel = lowerToItemLevel ? (jewellery ? std::min({ minlvl, ceiling / 2, 25 }) : std::min(minlvl, ceiling / 2)) : minlvl;
	const auto eligible = [&](const PLStruct &row) {
		if (row.PLMinLvl > ceiling)
			return false;
		if (!ignoreLevelLimits && row.PLMinLvl < floorLevel)
			return false;
		return eligibleIgnoringLevel(row);
	};

	// Every table's eligible rows in ONE candidate list, in the same order (and so the same draw for the same
	// seed) as when each table had a storage flag of its own - the seeded pack corpus pins that.
	std::vector<AffixCandidate> pool;
	pool.reserve(512);
	for (int j = 0; ItemPrefixes[j].power.type != IPL_INVALID; j++) {
		if (!IsPrefixValidForItemType(j, flgs, hellfireItem))
			continue;
		if (HasAnyOf(flgs, AffixItemType::Staff) && ItemPrefixes[j].power.type == IPL_CHARGES)
			continue;
		if (!eligible(ItemPrefixes[j]))
			continue;
		pool.push_back({ AffixSource::Prefix, j });
		if (ItemPrefixes[j].PLDouble)
			pool.push_back({ AffixSource::Prefix, j });
	}
	for (int j = 0; ItemSuffixes[j].power.type != IPL_INVALID; j++) {
		if (!IsSuffixValidForItemType(j, flgs, hellfireItem))
			continue;
		if (!eligible(ItemSuffixes[j]))
			continue;
		pool.push_back({ AffixSource::Suffix, j });
	}
	{
		// ONE CANDIDATE PER POOL TYPE, however many level bands it has - at the strongest band the item's
		// level reaches. Offering every eligible band made the pool's share climb with depth: at item level
		// 50 only the deepest vanilla rows are eligible, while two of each pool type's six bands were, and a
		// guaranteed Rare pick ignores level entirely, so all six competed. Measured 2026-09-13, Movement
		// Speed and Faster Cast reached one magic ring in five at ilvl 50 and a fifth of Rares, against the
		// drop tail's flat ~6% and ~8%. One candidate each keeps them competing like a single vanilla row.
		constexpr item_effect_type PoolTypes[] = { IPL_MOVESPEED, IPL_MOVESPEED_CURSE, IPL_FASTCAST, IPL_COLDRES };
		for (const item_effect_type type : PoolTypes) {
			int chosen = -1;
			int gentlest = -1;
			for (int j = 0; j < static_cast<int>(std::size(OracoolPoolRows)); j++) {
				const OracoolPoolRow &entry = OracoolPoolRows[j];
				if (entry.row.power.type != type || !entry.fits(item) || !eligibleIgnoringLevel(entry.row))
					continue;
				if (gentlest < 0 || entry.row.PLMinLvl < OracoolPoolRows[gentlest].row.PLMinLvl)
					gentlest = j;
				if (entry.row.PLMinLvl > ceiling || (!ignoreLevelLimits && entry.row.PLMinLvl < floorLevel))
					continue;
				if (chosen < 0 || entry.row.PLMinLvl > OracoolPoolRows[chosen].row.PLMinLvl)
					chosen = j;
			}
			// A guaranteed pick below every band still needs something to take: the gentlest one - but only if
			// even that is within the item's level.
			if (chosen < 0 && ignoreLevelLimits && gentlest >= 0 && OracoolPoolRows[gentlest].row.PLMinLvl <= ceiling)
				chosen = gentlest;
			if (chosen >= 0)
				pool.push_back({ AffixSource::Oracool, chosen });
		}
	}
	if (pool.empty())
		return std::nullopt;
	return pool[GenerateRnd(static_cast<int32_t>(pool.size()))];
}

} // namespace

/**
 * @brief VANILLA'S roll, replayed only to rebuild a vanilla magic item's NAME from its seed - nothing here
 * rolls or stores an affix. Its one caller is GetTranslatedItemNameMagical, which UpdateHellfireFlag uses to
 * tell a Diablo item from a Hellfire one in a save from the original games by comparing names; the two
 * words it finds are name parts. Every item this fork rolls keeps its affixes on the one list (2026-09-25).
 */
void GetItemPowerPrefixAndSuffix(int minlvl, int maxlvl, AffixItemType flgs, bool onlygood, bool hellfireItem, tl::function_ref<void(const PLStruct &prefix)> prefixFound, tl::function_ref<void(const PLStruct &suffix)> suffixFound, bool ignoreLevelLimits = false)
{
	int preidx = -1;
	int sufidx = -1;

	int l[256];
	goodorevil goe;

	bool allocatePrefix = FlipCoin(4);
	bool allocateSuffix = !FlipCoin(3);
	if (!allocatePrefix && !allocateSuffix) {
		// At least try and give each item a prefix or suffix
		if (FlipCoin())
			allocatePrefix = true;
		else
			allocateSuffix = true;
	}
	goe = GOE_ANY;
	if (!onlygood && !FlipCoin(3))
		onlygood = true;
	if (allocatePrefix) {
		int nt = 0;
		for (int j = 0; ItemPrefixes[j].power.type != IPL_INVALID; j++) {
			if (!IsPrefixValidForItemType(j, flgs, hellfireItem))
				continue;
			// The ceiling always holds; ignoreLevelLimits relaxes only the floor (2026-09-13, see DrawUnifiedAffix).
			if (ItemPrefixes[j].PLMinLvl > maxlvl || (!ignoreLevelLimits && ItemPrefixes[j].PLMinLvl < minlvl))
				continue;
			if (onlygood && !ItemPrefixes[j].PLOk)
				continue;
			if (HasAnyOf(flgs, AffixItemType::Staff) && ItemPrefixes[j].power.type == IPL_CHARGES)
				continue;
			l[nt] = j;
			nt++;
			if (ItemPrefixes[j].PLDouble) {
				l[nt] = j;
				nt++;
			}
		}
		if (nt != 0) {
			preidx = l[GenerateRnd(nt)];
			goe = ItemPrefixes[preidx].PLGOE;
			prefixFound(ItemPrefixes[preidx]);
		}
	}
	if (allocateSuffix) {
		int nl = 0;
		for (int j = 0; ItemSuffixes[j].power.type != IPL_INVALID; j++) {
			if (IsSuffixValidForItemType(j, flgs, hellfireItem)
			    && ItemSuffixes[j].PLMinLvl <= maxlvl && (ignoreLevelLimits || ItemSuffixes[j].PLMinLvl >= minlvl)
			    && !((goe == GOE_GOOD && ItemSuffixes[j].PLGOE == GOE_EVIL) || (goe == GOE_EVIL && ItemSuffixes[j].PLGOE == GOE_GOOD))
			    && (!onlygood || ItemSuffixes[j].PLOk)) {
				l[nl] = j;
				nl++;
			}
		}
		if (nl != 0) {
			sufidx = l[GenerateRnd(nl)];
			suffixFound(ItemSuffixes[sufidx]);
		}
	}
}

} // namespace

void GetItemPower(const Player &player, Item &item, int minlvl, int maxlvl, AffixItemType flgs, bool onlygood, bool ignoreLevelLimits)
{
	// ONE POOL, D3-style (user, 2026-09-13: "We call all possible item bonuses affixes and an item can
	// have any combo of them within its limit of affixes"). Vanilla rolled a prefix one time in four and
	// a suffix two times in three, so a magic item could never carry two prefixes or two suffixes, and
	// Movement Speed only arrived from a drop-tail roll outside any limit. A magic item now draws its one
	// or two affixes from the prefix table, the suffix table and OracoolPoolRows together.
	//
	// The count keeps vanilla's odds - both halves came up one time in six - and so does the only-good
	// coin, so a magic item is exactly as likely to be cursed as it ever was.
	const int wanted = GenerateRnd(6) == 0 ? 2 : 1;
	if (!onlygood && !FlipCoin(3))
		onlygood = true;

	std::array<item_effect_type, 2> picked {};
	int pickedCount = 0;
	goodorevil goe = GOE_ANY;
	// Bounded by the picked-types array as well as by wanted, as the tiered roller's apply() is. A proof run
	// on 2026-09-13 asked this loop for a third affix and it wrote past the end of picked - a fail-fast crash
	// rather than a refusal. Production never asks for more than two; this makes that a rule, not an accident.
	for (int i = 0; i < wanted && pickedCount < static_cast<int>(picked.size()); i++) {
		// EVERY affix onto the one list, whatever table its row sits in (user, 2026-09-25: "remove any trace of
		// prefix/sufix segregation. all afixes are now one pool"). A table affix used to go to a vanilla prefix or
		// suffix field that kept only its type, and a pool affix to the list - so a magic item's affixes lived in two
		// places, Gillian's Reroll saw only one of them, and three readers had to add the two together.
		const bool room = item._iOracoolAffixCount < Item::MaxOracoolAffixes;
		const std::optional<AffixCandidate> drawn = DrawUnifiedAffix(item, minlvl, maxlvl, flgs, onlygood, gbIsHellfire,
		    ignoreLevelLimits, room, picked.data(), pickedCount, goe);
		if (!drawn)
			break;
		const PLStruct &affix = RowOf(*drawn);
		// Stat, price and list entry in one place. The pool rows are priced too - the drop-tail rolls they
		// replaced added a stat and no value at all.
		SaveItemAffix(player, item, affix);
		picked[pickedCount++] = affix.power.type;
		if (affix.PLGOE != GOE_ANY)
			goe = affix.PLGOE;
		item._iMagical = ITEM_QUALITY_MAGIC;
	}

	if (pickedCount == 0) {
		// No affix rolled at all, so this never becomes a magic item and the base name is right.
		CopyUtf8(item._iIName, GenerateMagicItemName(item._iName, nullptr, nullptr, false), sizeof(item._iIName));
		if (!StringInPanel(item._iIName) && AllItemsList[item.IDidx].iSName != nullptr) // some bases have none (round 40 audit)
			CopyUtf8(item._iIName, GenerateMagicItemName(AllItemsList[item.IDidx].iSName, nullptr, nullptr, false), sizeof(item._iIName));
		return;
	}
	// The name pool - hashed from the seed, consuming no randomness (see GenerateOracoolItemName).
	CopyUtf8(item._iIName, oracool::GenerateOracoolItemName(item._iSeed), sizeof(item._iIName));
	CalcItemValue(item);
}

namespace {


namespace {


} // namespace

void GetStaffSpell(const Player &player, Item &item, int lvl, bool onlygood)
{
	if (!gbIsHellfire && FlipCoin(4)) {
		GetItemPower(player, item, lvl / 2, lvl, AffixItemType::Staff, onlygood);
		return;
	}

	int maxSpells = gbIsHellfire ? MAX_ITEM_SPELLS : 37;
	int l = lvl / 2;
	if (l == 0)
		l = 1;
	int rv = GenerateRnd(maxSpells) + 1;

	if (gbIsSpawn && lvl > 10)
		lvl = 10;

	int s = static_cast<int16_t>(SpellID::Firebolt);
	SpellID bs = SpellID::Null;
	while (rv > 0) {
		int sLevel = GetSpellStaffLevel(static_cast<SpellID>(s));
		// Oracool: a Staff of Town Portal would carry charges of an ability the player already has
		// for free from the HUD's Portal button - excluded from the roll the same way the book's
		// own spell is (GetBookSpell), by predicate rather than by jumping the enum index.
		if (sLevel != -1 && l >= sLevel) {
			rv--;
			bs = static_cast<SpellID>(s);
		}
		s++;
		if (!gbIsMultiplayer && s == static_cast<int16_t>(SpellID::Resurrect))
			s = static_cast<int16_t>(SpellID::Telekinesis);
		if (!gbIsMultiplayer && s == static_cast<int16_t>(SpellID::HealOther))
			s = static_cast<int16_t>(SpellID::BloodStar);
		if (s == maxSpells)
			s = static_cast<int16_t>(SpellID::Firebolt);
	}

	// Oracool: a Staff of Town Portal would carry charges of an ability the player already has
	// for free from the HUD's Portal button, so it must not spawn.
	//
	// This substitution happens AFTER the roll, deliberately. The obvious implementation - adding
	// !IsBuiltInPortalAbility to the eligibility predicate in the walk above - is what shipped
	// first, and it broke staves across save/load: pack_test's round-trip caught a staff going in
	// as "Fire Wall" and coming back as "Lightning". The walk counts `rv` down over spells that
	// pass the predicate, so excluding one shifts which spell every subsequent roll lands on, and
	// that mapping is sensitive to `l` (= lvl / 2). Generation and RecreateItem do not always
	// reach here with the same lvl, so the two diverged and a staff's spell changed when the game
	// reloaded it.
	//
	// Substituting on the result instead leaves the walk bit-for-bit as vanilla, so the same seed
	// always yields the same spell, and the remap depends only on that spell.
	if (oracool::IsBuiltInPortalAbility(bs))
		bs = SpellID::Firebolt;

	int minc = GetSpellData(bs).sStaffMin;
	int maxc = GetSpellData(bs).sStaffMax - minc + 1;
	item._iSpell = bs;
	item._iCharges = minc + GenerateRnd(maxc);
	item._iMaxCharges = item._iCharges;

	item._iMinMag = GetSpellData(bs).minInt;
	int v = item._iCharges * GetSpellData(bs).staffCost() / 5;
	item._ivalue += v;
	item._iIvalue += v;
	GetStaffPower(player, item, lvl, bs, onlygood);
}

void GetOilType(Item &item, int maxLvl)
{
	int cnt = 2;
	int8_t rnd[32] = { 5, 6 };

	if (!gbIsMultiplayer) {
		if (maxLvl == 0)
			maxLvl = 1;

		cnt = 0;
		for (size_t j = 0; j < sizeof(OilLevels) / sizeof(OilLevels[0]); j++) {
			if (OilLevels[j] <= maxLvl) {
				rnd[cnt] = j;
				cnt++;
			}
		}
	}

	int8_t t = rnd[GenerateRnd(cnt)];

	CopyUtf8(item._iName, OilNames[t], sizeof(item._iName));
	CopyUtf8(item._iIName, OilNames[t], sizeof(item._iIName));
	item._iMiscId = OilMagic[t];
	item._ivalue = OilValues[t];
	item._iIvalue = OilValues[t];
}

/**
 * @brief Maps an item's equipment type to the AffixItemType bit vanilla's affix tables
 * (ItemPrefixes[]/ItemSuffixes[]) filter eligibility by, mirroring GetItemBonus's dispatch
 * below. Returns AffixItemType::None for types that never carry affixes.
 */
AffixItemType GetAffixItemTypeForItem(const Item &item)
{
	switch (item._itype) {
	case ItemType::Sword:
	case ItemType::Axe:
	case ItemType::Mace:
		return AffixItemType::Weapon;
	case ItemType::Bow:
		return AffixItemType::Bow;
	case ItemType::Shield:
		return AffixItemType::Shield;
	case ItemType::LightArmor:
	case ItemType::Helm:
	case ItemType::MediumArmor:
	case ItemType::HeavyArmor:
		return AffixItemType::Armor;
	case ItemType::Staff:
		return AffixItemType::Staff;
	case ItemType::Ring:
	case ItemType::Amulet:
		return AffixItemType::Misc;
	// Oracool bug fix: user report - the give*set commands could not produce magic/rare/tiered
	// samples of the new worn types. This map is what every affix roll consults, and the six new
	// types fell to the None fallthrough - "cannot carry affixes at all", the same bucket as gold.
	// They take armour affixes, exactly like the helm they sit alongside.
	case ItemType::Shoulders:
	case ItemType::Bracers:
	case ItemType::Gloves:
	case ItemType::Belt:
	case ItemType::Legs:
	case ItemType::Boots:
		return AffixItemType::Armor;
	case ItemType::None:
	case ItemType::Misc:
	case ItemType::Gold:
		return AffixItemType::None;
	}
	return AffixItemType::None;
}

void GetItemBonus(const Player &player, Item &item, int minlvl, int maxlvl, bool onlygood, bool allowspells, bool ignoreLevelLimits = false)
{
	if (minlvl > 25)
		minlvl = 25;

	if (item._itype == ItemType::Staff && allowspells) {
		GetStaffSpell(player, item, maxlvl, onlygood);
		return;
	}

	AffixItemType flgs = GetAffixItemTypeForItem(item);
	if (flgs != AffixItemType::None)
		GetItemPower(player, item, minlvl, maxlvl, flgs, onlygood, ignoreLevelLimits);
}

/**
 * @brief True while an item is being rebuilt from a stored seed rather than generated fresh.
 *
 * THE SEAM. The droppable pool below is part of the save format - UnPackItem rebuilds a dungeon
 * item's index by replaying its seed through this exact walk, and RecreateTownItem replays the
 * vendor pools the same way. So the pool a replay sees must be the pool that existed when the item
 * was made, while fresh generation is free to use the banded qlvl ladder (oracool/item_tiers.h).
 *
 * A flag rather than a parameter because the filters are lambdas handed to a shared walk, four
 * layers below the two functions that know which case this is.
 */
// ReplayingStoredItemSeed is defined at the top of this file, outside every unnamed namespace (2026-09-13):
// DrawUnifiedAffix, several hundred lines above, reads it too, and a forward declaration in a different
// unnamed-namespace block named a second entity and made every use here ambiguous.

/** @brief RAII: sets ReplayingStoredItemSeed for the duration of a recreation. */
struct ReplayScope {
	ReplayScope()
	    : previous(ReplayingStoredItemSeed)
	{
		ReplayingStoredItemSeed = true;
	}
	~ReplayScope()
	{
		ReplayingStoredItemSeed = previous;
	}
	bool previous;
};

/**
 * @brief The qlvl a POOL FILTER should compare against: authored while replaying, banded otherwise.
 *
 * Every gate that decides whether a base may appear in the shared pool goes through here, so the
 * two answers can never drift apart by one call site being forgotten.
 */
int PoolQlvl(const ItemData &item)
{
	return ReplayingStoredItemSeed ? item.iMinMLvl : oracool::BandedQlvl(item.iMinMLvl);
}

_item_indexes GetItemIndexForDroppableItem(bool considerDropRate, tl::function_ref<bool(const ItemData &item)> isItemOkay)
{
	static std::array<_item_indexes, IDI_LAST * 2> ril;

	size_t ri = 0;
	for (std::underlying_type_t<_item_indexes> i = IDI_GOLD; i <= IDI_LAST; i++) {
		if (!IsItemAvailable(i))
			continue;
		const ItemData &item = AllItemsList[i];
		if (item.iRnd == IDROP_NEVER)
			continue;
		// Oracool (2026-08-15): the set items are droppable (IDROP_REGULAR, so IsDungeonItemValid
		// accepts them on the wire) but NOT through this pool. UnPackItem recreates an item's INDEX
		// by replaying its seed through this exact walk, so growing the list re-routes every seeded
		// recreation: the first attempt put them here and pack_test watched a Jade Great Helm come
		// back as Jade Leggings. Set items drop through their own hook instead - see
		// TrySpawnOracoolSetItem.
		//
		// Scope correction (2026-08-27): this used to be described as "the pool IS the save format",
		// and that overstates it. LoadHeroItems reads a full stored record over everything the
		// replay produced, so in single-player the walk decides nothing that survives a load - what
		// it still decides is the multiplayer WIRE format and pack_test, which is why the exclusions
		// stay. The practical rule is unchanged; the reason it is obeyed is narrower than it looked.
		if (IsOracoolItemIdx(i))
			continue;
		// Phase 1: gems, charms and runes obey the same pool-is-save-format rule; own hooks drop them.
		if (IsOracoolGemIdx(i) || IsOracoolCharmIdx(i) || IsOracoolRuneIdx(i) || IsOracoolJewelIdx(i)
		    || IsOracoolShardIdx(i) || IsOracoolSignetIdx(i) || IsOracoolEncounterMapIdx(i))
			continue;
		if (IsAnyOf(item.iSpell, SpellID::Resurrect, SpellID::HealOther) && !gbIsMultiplayer)
			continue;
		// Oracool: Town Portal is a built-in ability cast from the HUD's Portal button, so its
		// scrolls are redundant and no longer spawn - as loot, as vendor stock, anywhere. This is
		// the single chokepoint every generation path funnels through (loot, all four vendors,
		// uniques), and it deliberately mirrors the single-player Resurrect/HealOther exclusion
		// directly above. Note this is a *generation* filter only: it is NOT IsItemAvailable, which
		// also gates save/network validation and would strip scrolls a character already carries.
		if (oracool::IsBuiltInPortalAbility(item.iSpell))
			continue;
		// The hidden Bard's instrument and hymn book (2026-09-14, oracool/hidden_classes.h): the same kind of
		// generation-only filter, for the same reason - carried ones still load.
		if (oracool::IsHiddenItemIdx(i))
			continue;
		// The Necromancer's three families (2026-09-18) ride their own hook, TrySpawnNecroBase, for the pool-is-replayed
		// reason above - a first attempt seated them here and the pack fixtures' items came back as other things.
		if (oracool::IsNecroBaseIdx(i))
			continue;
		if (!isItemOkay(item))
			continue;
		ril[ri] = static_cast<_item_indexes>(i);
		ri++;
		if (item.iRnd == IDROP_DOUBLE && considerDropRate) {
			ril[ri] = static_cast<_item_indexes>(i);
			ri++;
		}
	}

	return ril[GenerateRnd(static_cast<int>(ri))];
}

_item_indexes RndUItem(Monster *monster)
{
	int itemMaxLevel = ItemsGetCurrlevel() * 2;
	if (monster != nullptr)
		itemMaxLevel = ItemLevelOfMonster(*monster);
	return GetItemIndexForDroppableItem(false, [&itemMaxLevel](const ItemData &item) {
		if (item.itype == ItemType::Misc && item.iMiscId == IMISC_BOOK)
			return true;
		if (itemMaxLevel < PoolQlvl(item))
			return false;
		if (IsAnyOf(item.itype, ItemType::Gold, ItemType::Misc))
			return false;
		return true;
	});
}

_item_indexes RndAllItems()
{
	if (GenerateRnd(100) > 25)
		return IDI_GOLD;

	int itemMaxLevel = ItemsGetCurrlevel() * 2;
	return GetItemIndexForDroppableItem(false, [&itemMaxLevel](const ItemData &item) {
		if (itemMaxLevel < PoolQlvl(item))
			return false;
		return true;
	});
}

_item_indexes RndTypeItems(ItemType itemType, int imid, int lvl)
{
	int itemMaxLevel = lvl * 2;
	return GetItemIndexForDroppableItem(false, [&itemMaxLevel, &itemType, &imid](const ItemData &item) {
		if (itemMaxLevel < PoolQlvl(item))
			return false;
		if (item.itype != itemType)
			return false;
		if (imid != -1 && item.iMiscId != imid)
			return false;
		return true;
	});
}

_unique_items CheckUnique(Item &item, int lvl, int uper, bool recreate, bool allowTieredRoll)
{
	std::bitset<MaxUniqueItems> uok = {};

	int uniqueRollUpperBound = uper;
	// Reconstructing a previously-generated item (RecreateItem/UnPackItem, allowTieredRoll
	// false) must reproduce the pass/fail decision this roll made at generation time, not
	// whatever Unique Item Drop Multiplier happens to be live right now - the multiplier can
	// change between when an item was saved and when it's reconstructed (e.g. hero-select
	// preview, or simply because the player changed the setting), and a wider window than
	// what was in effect originally can flip an item that generated as Magic into Unique.
	// Pin the multiplier to its vanilla-neutral value of 1 for reconstruction; this still
	// consumes the same single GenerateRnd(100) draw either way, so later rolls in this
	// function's caller stay at the same RNG-stream position regardless of which path ran.
	if (oracool::IsSinglePlayer()) {
		const int multiplier = allowTieredRoll ? std::clamp(*sgOptions.Oracool.uniqueItemDropMultiplier, 1, 100) : 1;
		uniqueRollUpperBound = std::min(99, (uper + 1) * multiplier - 1);
		// And the NARROWING knob (user, 2026-08-30: "we need to nurf drop chances of unique
		// monsters. they seem to drop uniques very generously").
		//
		// A separate percentage rather than a smaller `uper`, because uper is doing two jobs at
		// once: it is this roll's window AND the high-quality marker - `uper == 15` sets CF_UPER15
		// and pushes GetItemBLevel four levels up. Lowering it would quietly change affix levels
		// and the stored createInfo, so every already-saved item would reconstruct differently.
		// This touches only the roll.
		//
		// Pinned to 100 on the reconstruction path for exactly the reason the multiplier is pinned
		// to 1 there: a narrower window than the one in force at generation time would turn an item
		// that dropped as Unique into a Magic one on the next load.
		//
		// NOT by narrowing the window (round 27 audit): an ordinary drop's window is 2 tickets, so 25% and 10% came out at
		// -1 - never a unique - and 75% at the same 1 ticket as 50%. A hash of the seed passes percent in a hundred fresh
		// drops instead, like the ten-fold filter below (another constant, so the two gates do not correlate). No extra
		// draw: the seeds rebuild the same items.
	}
	if (GenerateRnd(100) > uniqueRollUpperBound)
		return UITEM_INVALID;
	if (allowTieredRoll && oracool::IsSinglePlayer()) {
		const int percent = std::clamp(*sgOptions.Oracool.uniqueDropChancePercent, 1, 100);
		if (static_cast<int>(((item._iSeed * 2246822519U) >> 16) % 100) >= percent)
			return UITEM_INVALID;
	}
	// TEN TIMES rarer on a fresh drop (user, 2026-09-13: "decrease drop chance of uniques and set item
	// 10 fold"). One ticket in ten survives, decided by a HASH of the seed rather than a draw: the draw
	// above is replayed from the seed by RecreateItem and pinned by pack_test's reference items, so a
	// wider range or an extra draw would rebuild other items from those seeds. Fresh generation only,
	// the same trade the percent knob above makes - a recreated item keeps its original verdict.
	if (allowTieredRoll && oracool::IsSinglePlayer() && ((item._iSeed * 2654435761U) >> 16) % 10 != 0)
		return UITEM_INVALID;

	int numu = 0;
	for (int j = 0; UniqueItems[j].UIItemId != UITYPE_INVALID; j++) {
		if (!IsUniqueAvailable(j))
			break;
		// Never on an item below the unique's own level (user, 2026-10-01): a chest rolls at twice its level and a unique
		// monster at +4, so a unique could drop asking a higher level than where it was found. Fresh drops only - a stored
		// seed replays its original verdict (pack_test goldens) - and only when the item level is stamped.
		const bool belowItemLevel = allowTieredRoll && !ReplayingStoredItemSeed && oracool::IsSinglePlayer()
		    && item._iOracoolItemLevel > 0 && item._iOracoolItemLevel < UniqueItems[j].UIMinLvl;
		if (UniqueItems[j].UIItemId == AllItemsList[item.IDidx].iItemId
		    && lvl >= UniqueItems[j].UIMinLvl && !belowItemLevel
		    && (recreate || !UniqueItemFlags[j] || gbIsMultiplayer)) {
			uok[j] = true;
			numu++;
		}
	}

	if (numu == 0)
		return UITEM_INVALID;

	DiscardRandomValues(1);
	int pick = numu; // the LAST eligible, as vanilla: a seed rebuilds into the unique it was (pack_test goldens)
	// uint16_t and MaxUniqueItems, both of which were a uint8_t and a literal 128. With 230 uniques
	// in the table the old pair could neither represent an id past 255 nor walk past index 127, so
	// every unique the expansion added was unreachable even before the bitset overran.
	uint16_t itemData = 0;
	while (pick > 0) {
		if (uok[itemData])
			pick--;
		if (pick > 0)
			itemData = static_cast<uint16_t>((itemData + 1) % MaxUniqueItems);
	}

	return (_unique_items)itemData;
}

void GetUniqueItem(const Player &player, Item &item, _unique_items uid)
{
	UniqueItemFlags[uid] = true;

	for (auto power : UniqueItems[uid].powers) {
		if (power.type == IPL_INVALID)
			break;
		SaveItemPower(player, item, power);
		// A unique that SETS its armour or damage wrote over the tier's scaled numbers with the authored Normal ones,
		// while its price and its Tier line kept the tier: a Torment Demonspike Coat had Normal's 100 armour at thirty
		// times the price (round 14 audit, v1.12.239). The set value takes the tier too.
		if (power.type == IPL_SETAC)
			item._iAC = oracool::ScalePowerForBaseTier(item._iAC, item._iOracoolBaseTier);
		if (power.type == IPL_SETDAM) {
			item._iMinDam = std::min(oracool::ScalePowerForBaseTier(item._iMinDam, item._iOracoolBaseTier), 255);
			item._iMaxDam = std::min(oracool::ScalePowerForBaseTier(item._iMaxDam, item._iOracoolBaseTier), 255);
		}
	}

	CopyUtf8(item._iIName, UniqueItems[uid].UIName, sizeof(item._iIName));
	// At the base's tier, as the unidentified value already is (oracool::ScaleValueForBaseTier).
	item._iIvalue = oracool::ScaleValueForBaseTier(UniqueItems[uid].UIValue, item._iOracoolBaseTier);

	if (item._iMiscId == IMISC_UNIQUE)
		item._iSeed = uid;

	item._iUid = uid;
	item._iMagical = ITEM_QUALITY_UNIQUE;
	item._iCreateInfo |= CF_UNIQUE;
}

void ItemRndDur(Item &item)
{
	if (item._iDurability > 0 && item._iDurability != DUR_INDESTRUCTIBLE)
		item._iDurability = GenerateRnd(item._iMaxDur / 2) + (item._iMaxDur / 4) + 1;
}

int GetItemBLevel(int lvl, item_misc_id miscId, bool onlygood, bool uper15)
{
	int iblvl = -1;
	if (GenerateRnd(100) <= 10
	    || GenerateRnd(100) <= lvl
	    || onlygood
	    || IsAnyOf(miscId, IMISC_STAFF, IMISC_RING, IMISC_AMULET)) {
		iblvl = lvl;
	}
	if (uper15)
		iblvl = lvl + 4;
	return iblvl;
}

/**
 * @param allowTieredRoll Must be false when reconstructing a previously-generated item from
 * its stored seed (RecreateItem/UnPackItem) - SetRndSeed(iseed) below makes every roll in this
 * function deterministic from that one seed, so an extra roll here would shift every
 * subsequent roll (e.g. GetItemBonus's affix selection) and silently change a previously
 * saved item's reconstructed stats on every load. Fresh generation (SpawnItem, chest/quest
 * drops, etc., all of which pass a brand new AdvanceRndSeed() seed) leaves this at its
 * default true - consuming extra randomness there is exactly what a "did this drop become
 * Rare/Buffed Unique" roll is supposed to do.
 */
/**
 * @param itemLevel the ilvl to stamp on the item: the mlvl of the monster that dropped it, or the
 * alvl of the chest, floor or shop it came from (oracool/area_level.h). Passed EXPLICITLY rather
 * than derived from @p lvl, because @p lvl carries two different conventions inherited from vanilla
 * - monster drops pass the monster level, floor items pass twice the depth - and guessing which one
 * a caller meant is exactly the kind of thing that would put a wrong number on every chest item.
 * -1 keeps @p lvl, which is right for the monster-drop callers.
 */
void SetupAllItems(const Player &player, Item &item, _item_indexes idx, uint32_t iseed, int lvl, int uper, bool onlygood, bool recreate, bool pregen, bool allowTieredRoll = true, std::optional<OracoolItemTier> forcedTier = std::nullopt, int itemLevel = -1)
{
	item._iSeed = iseed;
	SetRndSeed(iseed);
	// ilvl BEFORE GetItemAttrs, which is where a book picks its spell and needs to know how deep it
	// was found (see GetBookSpell).
	item._iOracoolItemLevel = static_cast<uint8_t>(std::clamp(itemLevel < 0 ? lvl : itemLevel, 0, 255));
	GetItemAttrs(item, idx, lvl / 2);

	// The BASE TIER, rolled from the ilvl and applied to the base numbers before any affix touches
	// them (user, 2026-08-19: "tier 2,3,4 of all basic items, just like in D2 [...] all magic, rare,
	// uniques, primals and sets items to be able to drop in all 4 tiers"). See oracool/item_tiers.h -
	// the tier is a property of the item rather than three more copies of every base row, which is
	// also what lets a set piece or a unique carry one without needing a version of itself per tier.
	//
	// Takes NO draw from the seeded stream - it hashes the seed instead. Every generated item is
	// reconstructible from its seed, so a draw here would shift every affix roll after it and change
	// what existing seeds produce. See oracool/item_tiers.h.
	//
	// FRESH GENERATION ONLY, and the flag for that is allowTieredRoll - NOT the parameter named
	// `recreate`, which despite its name is set from CF_UNIQUE and means "this is a unique". The
	// recreate path (RecreateItem, UnPackItem) rebuilds an item from its seed for the compact
	// multiplayer pack and the character-select preview, and that pack has no room to carry a tier, so
	// a tiered recreate would silently disagree with the item it came from. Single-player - what V1 is
	// - never takes that path: SaveItem writes every stat and LoadItemData reads them back, tier
	// scaling included.
	if (allowTieredRoll)
		oracool::ApplyBaseTier(item, PinnedBaseTier.value_or(oracool::TierForItem(item._iOracoolItemLevel, iseed)));

	// CLAMPED to the six bits CF_LEVEL actually has. The area ladder reaches 96 and floor items pass
	// twice their depth, so an unclamped write would spill into the CF_ONLYGOOD/CF_UPER flag bits
	// above it. The real ilvl lives in _iOracoolItemLevel, which is a whole byte of its own.
	item._iCreateInfo = std::min(lvl, 63);

	if (pregen)
		item._iCreateInfo |= CF_PREGEN;
	if (onlygood)
		item._iCreateInfo |= CF_ONLYGOOD;

	if (uper == 15)
		item._iCreateInfo |= CF_UPER15;
	else if (uper == 1)
		item._iCreateInfo |= CF_UPER1;

	if (item._iMiscId != IMISC_UNIQUE) {
		int iblvl = GetItemBLevel(lvl, item._iMiscId, onlygood, uper == 15);
		if (iblvl != -1 && forcedTier) {
			// Oracool: debug-only path (giverare/giveunique/giveprimal) - forces the requested
			// tier unconditionally instead of the normal probabilistic fork below, reusing this
			// function's own ItemRndDur/SetupItem sequencing so a forced item is finished
			// exactly like a naturally-rolled one. Falls back to a plain magic roll if the item
			// type can't carry tiered affixes at all (e.g. potions, scrolls).
			const AffixItemType tieredFlgs = GetAffixItemTypeForItem(item);
			if (tieredFlgs == AffixItemType::None) {
				GetItemBonus(player, item, iblvl / 2, iblvl, onlygood, true);
			} else {
				switch (*forcedTier) {
				case OracoolItemTier::Rare:
					GetRareItemAffixes(player, item, iblvl / 2, iblvl, tieredFlgs, onlygood, /*ignoreLevelLimits=*/true);
					break;
				case OracoolItemTier::BuffedUnique:
					GetBuffedUniqueItemAffixes(player, item, iblvl / 2, iblvl, tieredFlgs, onlygood, /*ignoreLevelLimits=*/true);
					break;
				case OracoolItemTier::Primal:
					GetPrimalItemAffixes(player, item, iblvl / 2, iblvl, tieredFlgs, onlygood, /*ignoreLevelLimits=*/true);
					break;
				case OracoolItemTier::None:
					GetItemBonus(player, item, iblvl / 2, iblvl, onlygood, true);
					break;
				}
			}
		} else if (iblvl != -1) {
			_unique_items uid = CheckUnique(item, iblvl, uper, recreate, allowTieredRoll);
			const bool tieredRollEligible = allowTieredRoll && oracool::IsSinglePlayer();
			// A pure function of item._itype, unchanged by anything below - computed once
			// and reused instead of every tier's roll recomputing the same answer. Safe to
			// hoist unconditionally: it never touches shared RNG state, so this can't shift
			// where GenerateRnd() below lands in the random sequence for any item.
			const AffixItemType tieredFlgs = GetAffixItemTypeForItem(item);
			if (uid != UITEM_INVALID) {
				GetUniqueItem(player, item, uid);
			} else if (const int primalPerMille = oracool::QualityChancePerMille(OracoolItemTier::Primal, item._iOracoolItemLevel, *sgOptions.Oracool.primalItemDropChance);
			    tieredRollEligible && primalPerMille > 0 && GenerateRnd(1000) < primalPerMille
			    && tieredFlgs != AffixItemType::None) {
				// Primal is checked before Buffed Unique and Rare: it's the rarest and most
				// powerful tier, so it gets first crack at the item. Identification is
				// deliberately NOT forced here - every item, tiered or not, follows the
				// single shared Auto Identify Drops toggle checked below.
				GetPrimalItemAffixes(player, item, iblvl / 2, iblvl, tieredFlgs, onlygood);
			} else if (const int buffedPerMille = oracool::QualityChancePerMille(OracoolItemTier::BuffedUnique, item._iOracoolItemLevel, *sgOptions.Oracool.buffedUniqueItemDropChance);
			    tieredRollEligible && buffedPerMille > 0 && GenerateRnd(1000) < buffedPerMille
			    && tieredFlgs != AffixItemType::None) {
				// Buffed Unique is checked before Rare: it's meant to be the rarer of the
				// two tiers, so the rarer roll gets first crack at the item before a more
				// common tier claims it.
				GetBuffedUniqueItemAffixes(player, item, iblvl / 2, iblvl, tieredFlgs, onlygood);
			} else if (const int rarePerMille = oracool::QualityChancePerMille(OracoolItemTier::Rare, item._iOracoolItemLevel, *sgOptions.Oracool.rareItemDropChance);
			    tieredRollEligible && rarePerMille > 0 && GenerateRnd(1000) < rarePerMille
			    && tieredFlgs != AffixItemType::None) {
				// Rare items sit between Magic and Unique in the quality-roll fork: only
				// tried once an item has already failed its Unique, Primal, and Buffed
				// Unique rolls, so none of those is ever reduced or replaced, matching the
				// roadmap's drop policy.
				GetRareItemAffixes(player, item, iblvl / 2, iblvl, tieredFlgs, onlygood);
			} else {
				GetItemBonus(player, item, iblvl / 2, iblvl, onlygood, true);
			}
		}
		if (item._iMagical != ITEM_QUALITY_UNIQUE)
			ItemRndDur(item);
		if (item._iOracoolTier == OracoolItemTier::Primal)
			item._iDurability = item._iMaxDur; // perfect roll: full durability, overriding ItemRndDur's random roll above
	} else {
		if (item._iLoc != ILOC_UNEQUIPABLE) {
			if (iseed > 109 || AllItemsList[static_cast<size_t>(idx)].iItemId != UniqueItems[iseed].UIItemId) {
				item.clear();
				return;
			}

			GetUniqueItem(player, item, (_unique_items)iseed); // uid is stored in iseed for uniques
		}
	}
	SetupItem(item);
	if (oracool::IsSinglePlayer() && *sgOptions.Oracool.autoIdentifyDrops)
		item._iIdentified = true;
}

/** @brief Every unique @p item could be ennobled into - same base kind, within its own depth. */
std::vector<int> UniquesForBaseOf(const Item &item)
{
	std::vector<int> found;
	const auto baseKind = AllItemsList[item.IDidx].iItemId;
	if (baseKind == UITYPE_NONE)
		return found;
	// A vanilla unique is always within its OWN reach, whatever its item level says (sweep, 2026-09-25). A unique
	// bought off the Unique shelf carried ilvl 0, so its own row failed the depth test, the list came back
	// empty and the Cube refused to reroll it - and an under-levelled one lost itself from the "anything but
	// the current one" pool EnnobleOracoolRare builds from this.
	const bool isVanillaUnique = item._iMagical == ITEM_QUALITY_UNIQUE && !item.hasOracoolTier();
	const int ownUid = isVanillaUnique ? item._iUid : -1;
	for (int i = 0; i < static_cast<int>(UniqueItemCount); i++) {
		if (UniqueItems[i].UIItemId != baseKind)
			continue;
		if (UniqueItems[i].UIMinLvl <= item._iOracoolItemLevel || i == ownUid)
			found.push_back(i);
	}
	return found;
}

void SetupBaseItem(Point position, _item_indexes idx, bool onlygood, bool sendmsg, bool delta, bool spawn = false)
{
	if (ActiveItemCount >= MAXITEMS)
		return;

	int ii = AllocateItem();
	auto &item = Items[ii];
	GetSuperItemSpace(position, ii);
	int curlv = ItemsGetCurrlevel();

	SetupAllItems(*MyPlayer, item, idx, AdvanceRndSeed(), 2 * curlv, 1, onlygood, false, delta,
	    /*allowTieredRoll=*/true, std::nullopt, /*itemLevel=*/curlv);
	// The same tail a monster drop gets, at the level this item was generated at: chests, racks,
	// corpses, barrels, theme rooms and Find Item are fresh drops too (DROP-01, 2026-09-07).
	FinalizeFreshDrop(item, 2 * curlv);

	if (sendmsg)
		NetSendCmdPItem(false, CMD_DROPITEM, item.position, item);
	if (delta)
		DeltaAddItem(ii);
	if (spawn)
		NetSendCmdPItem(false, CMD_SPAWNITEM, item.position, item);
}

namespace {

/** @brief The useful drop the current RNG rolls at @p lvl (vanilla's draw, unchanged). */
_item_indexes RollUsefulIndex(int lvl)
{
	_item_indexes idx;

	if (gbIsHellfire) {
		switch (GenerateRnd(7)) {
		case 0:
			idx = IDI_PORTAL;
			if (lvl <= 1)
				idx = IDI_HEAL;
			break;
		case 1:
		case 2:
			idx = IDI_HEAL;
			break;
		case 3:
			idx = IDI_PORTAL;
			if (lvl <= 1)
				idx = IDI_MANA;
			break;
		case 4:
		case 5:
			idx = IDI_MANA;
			break;
		default:
			idx = IDI_OIL;
			break;
		}
	} else {
		idx = PickRandomlyAmong({ IDI_MANA, IDI_HEAL });

		if (lvl > 1 && FlipCoin(3))
			idx = IDI_PORTAL;
	}
	return idx;
}

} // namespace

void SetupAllUseful(Item &item, int iseed, int lvl)
{
	item._iSeed = iseed;
	SetRndSeed(iseed);
	_item_indexes idx = RollUsefulIndex(lvl);
	// Not a Town Portal scroll where the portal is a built-in ability (round 37 audit: barrels and chests still gave them).
	// A fresh drop moves its seed on until it rolls something else, so the seed it keeps rebuilds into what dropped (round 38
	// audit: swapping the index came back a scroll on a reload). A stored seed replays unchanged.
	if (idx == IDI_PORTAL && !ReplayingStoredItemSeed && oracool::IsBuiltInPortalAbility(SpellID::TownPortal)) {
		for (int tries = 0; tries < 32 && idx == IDI_PORTAL; tries++) {
			iseed = static_cast<int>(static_cast<uint32_t>(iseed) * 1103515245U + 12345U);
			item._iSeed = iseed;
			SetRndSeed(iseed);
			idx = RollUsefulIndex(lvl);
		}
	}

	GetItemAttrs(item, idx, lvl);
	// Held to the 6-bit field (audit, 2026-09-27): area level 64 - Torment's floors 16 and 24 - stored as 0, and the scroll
	// replayed from it on a reload came back a potion (the level-1 branch).
	item._iCreateInfo = std::min(lvl, static_cast<int>(CF_LEVEL)) | CF_USEFUL;
	SetupItem(item);
}

uint8_t Char2int(uint8_t input)
{
	if (input >= '0' && input <= '9')
		return input - '0';
	if (input >= 'A' && input <= 'F')
		return input - 'A' + 10;
	return 0;
}

void Hex2bin(const char *src, int bytes, uint8_t *target)
{
	for (int i = 0; i < bytes; i++, src += 2) {
		target[i] = (Char2int(src[0]) << 4) | Char2int(src[1]);
	}
}

void SpawnRock()
{
	if (ActiveItemCount >= MAXITEMS)
		return;

	const Object *stand = nullptr;
	for (int i = 0; i < ActiveObjectCount; i++) {
		const Object &object = Objects[ActiveObjects[i]];
		if (object._otype == OBJ_STAND) {
			stand = &object;
			break;
		}
	}

	if (stand == nullptr)
		return;

	int ii = AllocateItem();
	Item &item = Items[ii];

	item.position = stand->position;
	dItem[item.position.x][item.position.y] = ii + 1;
	int curlv = ItemsGetCurrlevel();
	GetItemAttrs(item, IDI_ROCK, curlv);
	SetupItem(item);
	item._iSelFlag = 2;
	item._iPostDraw = true;
	item.AnimInfo.currentFrame = 10;
	item._iCreateInfo |= CF_PREGEN;

	DeltaAddItem(ii);
}

void ItemDoppel()
{
	if (!gbIsMultiplayer)
		return;

	static int idoppely = 16;

	for (int idoppelx = 16; idoppelx < 96; idoppelx++) {
		if (dItem[idoppelx][idoppely] != 0) {
			Item *i = &Items[dItem[idoppelx][idoppely] - 1];
			if (i->position.x != idoppelx || i->position.y != idoppely)
				dItem[idoppelx][idoppely] = 0;
		}
	}

	idoppely++;
	if (idoppely == 96)
		idoppely = 16;
}

/**
 * @brief Prints a misc item's own description lines.
 *
 * Named for the oils it started as; it has switched on _iMiscId and covered every misc item for a
 * long time. Takes the ITEM now rather than just the id, because a Sealed Map's line has to name
 * WHICH encounter it opens and three maps share one misc id - and because the old `char` parameter
 * was one appended IMISC_ value away from narrowing silently.
 */
void PrintItemOil(const Item &item)
{
	switch (item._iMiscId) {
	case IMISC_OILACC:
		AddPanelString(_("increases a weapon's"));
		AddPanelString(_("chance to hit"));
		break;
	case IMISC_OILMAST:
		AddPanelString(_("greatly increases a"));
		AddPanelString(_("weapon's chance to hit"));
		break;
	case IMISC_OILSHARP:
		AddPanelString(_("increases a weapon's"));
		AddPanelString(_("damage potential"));
		break;
	case IMISC_OILDEATH:
		AddPanelString(_("greatly increases a weapon's"));
		AddPanelString(_("damage potential - not bows"));
		break;
	case IMISC_OILSKILL:
		AddPanelString(_("reduces attributes needed"));
		AddPanelString(_("to use armor or weapons"));
		break;
	case IMISC_OILBSMTH:
		AddPanelString(/*xgettext:no-c-format*/ _("restores 20% of an"));
		AddPanelString(_("item's durability"));
		break;
	case IMISC_OILFORT:
		AddPanelString(_("increases an item's"));
		AddPanelString(_("current and max durability"));
		break;
	case IMISC_OILPERM:
		AddPanelString(_("makes an item indestructible"));
		break;
	case IMISC_OILHARD:
		AddPanelString(_("increases the armor class"));
		AddPanelString(_("of armor and shields"));
		break;
	case IMISC_OILIMP:
		AddPanelString(_("greatly increases the armor"));
		AddPanelString(_("class of armor and shields"));
		break;
	case IMISC_RUNEF:
		AddPanelString(_("sets fire trap"));
		break;
	case IMISC_RUNEL:
	case IMISC_GR_RUNEL:
		AddPanelString(_("sets lightning trap"));
		break;
	case IMISC_GR_RUNEF:
		AddPanelString(_("sets fire trap"));
		break;
	case IMISC_RUNES:
		AddPanelString(_("sets petrification trap"));
		break;
	case IMISC_FULLHEAL:
		AddPanelString(_("restore all life"));
		break;
	case IMISC_HEAL:
		AddPanelString(_("restore some life"));
		break;
	case IMISC_MANA:
		AddPanelString(_("restore some mana"));
		break;
	case IMISC_FULLMANA:
		AddPanelString(_("restore all mana"));
		break;
	case IMISC_ELIXSTR:
		AddPanelString(_("increase strength"));
		break;
	case IMISC_ELIXMAG:
		AddPanelString(_("increase magic"));
		break;
	case IMISC_ELIXDEX:
		AddPanelString(_("increase dexterity"));
		break;
	case IMISC_ELIXVIT:
		AddPanelString(_("increase vitality"));
		break;
	case IMISC_SPECELIX:
		// Hellfire's Spectral Elixir, +3 to each attribute (UseItem). It printed nothing at all - no line here and
		// no way in through PrintItemMisc's gate (tooltip sweep, 2026-09-25).
		AddPanelString(_("+3 to all attributes"));
		break;
	case IMISC_REJUV:
		AddPanelString(_("restore some life and mana"));
		break;
	case IMISC_FULLREJUV:
		AddPanelString(_("restore all life and mana"));
		break;
	case IMISC_ARENAPOT:
		AddPanelString(_("restore all life and mana"));
		AddPanelString(_("(works only in arenas)"));
		break;
	case IMISC_ORACOOL_MAP: {
		// WHERE it goes and WHAT it pays, both, on the item itself. This is where "a known reward"
		// lives - the whole difference between an encounter and a lottery is that you can decide to
		// go, and a player who has never seen one still knows before they spend it.
		oracool::NamedEncounter encounter;
		if (oracool::EncounterForMapItem(item.IDidx, encounter)) {
			AddPanelString(fmt::format(fmt::runtime(_("opens {:s}")),
			    _(oracool::NamedEncounterName(encounter))));
			AddPanelString(fmt::format(fmt::runtime(_("its guardian carries: {:s}")),
			    _(AllItemsList[oracool::NamedEncounterReward(encounter)].iName)));
			AddPanelString(_("use in town - the map is consumed"));
		}
	} break;
	case IMISC_ORACOOL_KEYSTONE:
		// The tier is the whole item (oracool/rift.h): it says what the rift will be before the key is spent.
		AddPanelString(fmt::format(fmt::runtime(_("opens a Guardian Rift of tier {:d}")), std::max<int>(1, item._iOracoolRiftTier))); // as rift.cpp reads it
		AddPanelString(_("fifteen minutes; Diablo or Na-Krul at the end"));
		AddPanelString(_("use in town - spent when you step through the portal")); // as rift.cpp spends it (round 31 audit)
		break;
	case IMISC_ORACOOL_SIGNET:
		// The cap is stated on the item itself, because it is the whole mechanism and a player who
		// learns it only by being refused has learned it too late to plan around.
		AddPanelString(_("one permanent stat point"));
		AddPanelString(fmt::format(fmt::runtime(_("{:d} of {:d} used this life")),
		    oracool::SignetsUsed(*MyPlayer), oracool::SignetLifetimeCap));
		break;
	}
}

void printItemMiscKBM(const Item &item, const bool isOil, const bool isCastOnTarget)
{
	if (item._iMiscId == IMISC_MAPOFDOOM) {
		AddPanelString(_("Right-click to view"));
	} else if (isOil) {
		PrintItemOil(item);
		AddPanelString(_("Right-click to use"));
	} else if (isCastOnTarget) {
		AddPanelString(_("Right-click to read, then\nleft-click to target"));
	} else if (IsAnyOf(item._iMiscId, IMISC_BOOK, IMISC_NOTE, IMISC_SCROLL, IMISC_SCROLLT)) {
		AddPanelString(_("Right-click to read"));
	}
}

void printItemMiscGenericGamepad(const Item &item, const bool isOil, bool isCastOnTarget)
{
	if (item._iMiscId == IMISC_MAPOFDOOM) {
		AddPanelString(_("Activate to view"));
	} else if (isOil) {
		PrintItemOil(item);
		if (!invflag) {
			AddPanelString(_("Open inventory to use"));
		} else {
			AddPanelString(_("Activate to use"));
		}
	} else if (isCastOnTarget) {
		AddPanelString(_("Select from spell book, then\ncast spell to read"));
	} else if (IsAnyOf(item._iMiscId, IMISC_BOOK, IMISC_NOTE, IMISC_SCROLL, IMISC_SCROLLT)) {
		AddPanelString(_("Activate to read"));
	}
}

void printItemMiscGamepad(const Item &item, bool isOil, bool isCastOnTarget)
{
	string_view activateButton;
	string_view castButton;
	switch (GamepadType) {
	case GamepadLayout::Generic:
		printItemMiscGenericGamepad(item, isOil, isCastOnTarget);
		return;
	case GamepadLayout::Xbox:
		activateButton = controller_button_icon::Xbox_Y;
		castButton = controller_button_icon::Xbox_X;
		break;
	case GamepadLayout::PlayStation:
		activateButton = controller_button_icon::Playstation_Triangle;
		castButton = controller_button_icon::Playstation_Square;
		break;
	case GamepadLayout::Nintendo:
		activateButton = controller_button_icon::Nintendo_X;
		castButton = controller_button_icon::Nintendo_Y;
		break;
	}

	if (item._iMiscId == IMISC_MAPOFDOOM) {
		AddPanelString(fmt::format(fmt::runtime(_("{} to view")), activateButton));
	} else if (isOil) {
		PrintItemOil(item);
		if (!invflag) {
			AddPanelString(_("Open inventory to use"));
		} else {
			AddPanelString(fmt::format(fmt::runtime(_("{} to use")), activateButton));
		}
	} else if (isCastOnTarget) {
		AddPanelString(fmt::format(fmt::runtime(_("Select from spell book,\nthen {} to read")), castButton));
	} else if (IsAnyOf(item._iMiscId, IMISC_BOOK, IMISC_NOTE, IMISC_SCROLL, IMISC_SCROLLT)) {
		AddPanelString(fmt::format(fmt::runtime(_("{} to read")), activateButton));
	}
}

void PrintItemMisc(const Item &item)
{
	if (item._iMiscId == IMISC_EAR) {
		AddPanelString(fmt::format(fmt::runtime(pgettext("player", "Level: {:d}")), item._ivalue));
		return;
	}
	if (item._iMiscId == IMISC_AURIC) {
		AddPanelString(_("Doubles gold capacity"));
		return;
	}
	const bool isOil = (item._iMiscId >= IMISC_USEFIRST && item._iMiscId <= IMISC_USELAST)
	    || (item._iMiscId > IMISC_OILFIRST && item._iMiscId < IMISC_OILLAST)
	    || (item._iMiscId > IMISC_RUNEFIRST && item._iMiscId < IMISC_RUNELAST)
	    || item._iMiscId == IMISC_ARENAPOT
	    // The fork's three right-click items (tooltip audit, 2026-09-25). PrintItemOil has their description
	    // rows - what the signet gives and how many this life has used, the keystone's rift and tier, the map's
	    // encounter and its guardian's carry - but this gate is the only way to reach it, and their misc ids sit
	    // outside every range above, so those rows and the "Right-click to use" hint had never been shown.
	    || IsAnyOf(item._iMiscId, IMISC_ORACOOL_SIGNET, IMISC_ORACOOL_KEYSTONE, IMISC_ORACOOL_MAP, IMISC_SPECELIX);
	const bool mouseRequiresTarget = (item._iMiscId == IMISC_SCROLLT && item._iSpell != SpellID::Flash)
	    || (item._iMiscId == IMISC_SCROLL && IsAnyOf(item._iSpell, SpellID::TownPortal, SpellID::Identify));
	const bool gamepadRequiresTarget = item.isScroll() && TargetsMonster(item._iSpell);

	switch (ControlMode) {
	case ControlTypes::None:
		break;
	case ControlTypes::KeyboardAndMouse:
		printItemMiscKBM(item, isOil, mouseRequiresTarget);
		break;
	case ControlTypes::VirtualGamepad:
		printItemMiscGenericGamepad(item, isOil, gamepadRequiresTarget);
		break;
	case ControlTypes::Gamepad:
		printItemMiscGamepad(item, isOil, gamepadRequiresTarget);
		break;
	}
	// How to split a stack (user, 2026-09-24 dev note: "on every stackable item write a tip how to split
	// stack and make sure tip co[r]responds to the true mechanic"). The mechanic: Shift + right-click
	// on a stack of two or more, in the backpack (any page), the belt or the stash - TryStartStackSplit
	// and TryStartStashStackSplit, single-player only, stackable consumables only. The tip carries
	// the same conditions, so it never promises a split the click will not do. The mouse's alone:
	// no pad button starts a split.
	if (ControlMode == ControlTypes::KeyboardAndMouse && oracool::IsSinglePlayer() && item.isStackableConsumable()
	    && item.stackCount() > 1)
		AddPanelString(_("Shift + right-click to split the stack"));
}

/**
 * @brief Oracool: the hover panel's colour scheme, per user specification.
 *
 * The name takes the item's own tier colour (Item::getTextColor), and everything below it splits
 * into two kinds: what the item IS - its damage or armour, durability, charges, stat requirements -
 * which is white, and what has been ADDED to it - its affixes - which is blue. The
 * point is that a glance at the panel separates the base item from its rolls without reading a
 * word, and that a magic item's blue name is echoed by the blue lines that earned it.
 *
 * The tier label line ("unique item", or an Oracool tier's name) is not in either group: it names
 * the tier, so it takes the tier's colour along with the name.
 */
constexpr UiFlags ItemBaseStatColor = UiFlags::ColorWhite;
constexpr UiFlags ItemAffixColor = UiFlags::ColorBlue;

void PrintItemInfo(const Item &item)
{
	PrintItemMisc(item);
	// Sockets v2: what the panel prints is what CanUseItem checks - both go through Hel's
	// reduction, so a Hel'd item never claims a requirement it does not actually have.
	uint8_t str = static_cast<uint8_t>(oracool::EffectiveRequirement(item, item._iMinStr));
	uint8_t dex = static_cast<uint8_t>(oracool::EffectiveRequirement(item, item._iMinDex));
	uint8_t mag = static_cast<uint8_t>(oracool::EffectiveRequirement(item, item._iMinMag));
	if (str != 0 || mag != 0 || dex != 0) {
		std::string text = std::string(_("Required:"));
		// Each requirement the character does not meet is RED, the rest white (user, 2026-09-06:
		// "if i cant equipt an item due to a stats requrement nt fulfilled, in the description of
		// that item use red font for that stat requirement, so it is easier to spot it"). The same
		// comparison as Player::CanUseItem, against the character being inspected.
		std::vector<PanelLineRun> runs;
		const Player *who = InspectPlayer != nullptr ? InspectPlayer : MyPlayer;
		auto append = [&](uint8_t need, int have, const char *format) {
			if (need == 0)
				return;
			const bool unmet = who != nullptr && have < need;
			if (unmet)
				runs.push_back({ static_cast<uint16_t>(text.size()), UiFlags::ColorRed });
			text.append(fmt::format(fmt::runtime(_(format)), need));
			if (unmet)
				runs.push_back({ static_cast<uint16_t>(text.size()), ItemBaseStatColor });
		};
		append(str, who != nullptr ? who->_pStrength : 0, " {:d} Str");
		append(mag, who != nullptr ? who->_pMagic : 0, " {:d} Mag");
		append(dex, who != nullptr ? who->_pDexterity : 0, " {:d} Dex");
		// Oracool: requirements are base information about the item, so white - see the colour
		// scheme note in PrintItemDetails.
		AddPanelStringRuns(std::move(text), ItemBaseStatColor, std::move(runs));
	}
	// The fourth requirement (2026-09-20, decision D7: always, red when unmet): the character
	// level, derived from the item - see oracool/level_requirement.h. Its own line, since it is
	// asked of a different number than the three stats.
	if (const int level = oracool::RequiredLevel(item); level > 1) {
		const Player *who = InspectPlayer != nullptr ? InspectPlayer : MyPlayer;
		const bool unmet = who != nullptr && who->_pLevel < level;
		AddPanelStringRuns(fmt::format(fmt::runtime(_("Required Level: {:d}")), level), unmet ? UiFlags::ColorRed : ItemBaseStatColor, {});
	}
	// The class rule, which CanUseItem also asks: a head in a Paladin's hands was red with every printed requirement
	// white, and no reason given (round 19 audit, v1.12.244).
	if (const Player *who = InspectPlayer != nullptr ? InspectPlayer : MyPlayer; who != nullptr && !oracool::ClassMayUseItem(*who, item))
		AddPanelStringRuns(std::string(_("Your class cannot use this")), UiFlags::ColorRed, {});
}

/**
 * @brief Oracool: whether @p item is one of the unique expansion's bases (2026-09-11) - dungeon only.
 *
 * Vendor stock is not stored, it is REBUILT from a seed by re-picking a base from the shop's pool
 * (RecreateTownItem). A base added to that pool changes what every already-bought item rebuilds
 * into - a saved sword came back a helm in the pack goldens. So these bases never enter a shop
 * pool, and every item bought before them rebuilds exactly as it was bought.
 */
bool IsUniqueExpansionBase(const ItemData &item)
{
	return item.iItemId >= UITYPE_GLOVES && item.iItemId <= UITYPE_ARCANEFOCUS;
}

bool SmithItemOk(const Player &player, const ItemData &item)
{
	if (item.itype == ItemType::Misc)
		return false;
	if (item.itype == ItemType::Gold)
		return false;
	if (item.itype == ItemType::Staff && (!gbIsHellfire || IsValidSpell(item.iSpell)))
		return false;
	if (item.itype == ItemType::Ring)
		return false;
	if (item.itype == ItemType::Amulet)
		return false;

	return true;
}

template <bool (*Ok)(const Player &, const ItemData &), bool ConsiderDropRate = false>
_item_indexes RndVendorItem(const Player &player, int minlvl, int maxlvl)
{
	return GetItemIndexForDroppableItem(ConsiderDropRate, [&player, &minlvl, &maxlvl](const ItemData &item) {
		if (!Ok(player, item))
			return false;
		if (IsUniqueExpansionBase(item))
			return false; // dungeon only - see IsUniqueExpansionBase
		const int poolQlvl = PoolQlvl(item);
		if (poolQlvl < minlvl || poolQlvl > maxlvl)
			return false;
		return true;
	});
}

_item_indexes RndSmithItem(const Player &player, int lvl)
{
	return RndVendorItem<SmithItemOk, true>(player, 0, lvl);
}

/**
 * @brief Sorts a vendor's stock by base item, from @p itemList to the first empty slot.
 *
 * @param remaining how many slots there are from @p itemList to the end of the array.
 *
 * The bound is not decoration. This walked to the first empty slot with nothing stopping it at the
 * end of the array, which is fine only while a vendor's stock never fills its array - and Griswold's
 * already could before this was noticed: `iCnt` is capped at SMITH_ITEMS minus the salvage charms,
 * and StockSalvageCharms then fills exactly that many, so a maximum roll left no empty slot at all
 * and this walked off the end. Raising the stock counts to fill the shop grid (v1.9.28) turned that
 * from an occasional overrun into the normal case, which is how it was found.
 */
void SortVendor(Item *itemList, int remaining)
{
	int count = 1;
	while (count < remaining && !itemList[count].isEmpty())
		count++;

	auto cmp = [](const Item &a, const Item &b) {
		return a.IDidx < b.IDidx;
	};

	std::sort(itemList, itemList + count, cmp);
}

bool PremiumItemOk(const Player &player, const ItemData &item)
{
	if (item.itype == ItemType::Misc)
		return false;
	if (item.itype == ItemType::Gold)
		return false;
	if (!gbIsHellfire && item.itype == ItemType::Staff)
		return false;

	if (gbIsMultiplayer) {
		if (item.iMiscId == IMISC_OILOF)
			return false;
		if (item.itype == ItemType::Ring)
			return false;
		if (item.itype == ItemType::Amulet)
			return false;
	}

	return true;
}

_item_indexes RndPremiumItem(const Player &player, int minlvl, int maxlvl)
{
	return RndVendorItem<PremiumItemOk>(player, minlvl, maxlvl);
}

void SpawnOnePremium(Item &premiumItem, int plvl, const Player &player)
{
	const bool ignoreAffixLevelLimits = !gbIsMultiplayer && *sgOptions.Oracool.griswoldPremiumIgnoreAffixLevelLimits;
	const bool ignorePriceLimits = !gbIsMultiplayer && *sgOptions.Oracool.griswoldPremiumIgnorePriceLimits;
	int strength = std::max(player.GetMaximumAttributeValue(CharacterAttribute::Strength), player._pStrength);
	int dexterity = std::max(player.GetMaximumAttributeValue(CharacterAttribute::Dexterity), player._pDexterity);
	int magic = std::max(player.GetMaximumAttributeValue(CharacterAttribute::Magic), player._pMagic);
	strength += strength / 5;
	dexterity += dexterity / 5;
	magic += magic / 5;

	plvl = clamp(plvl, 1, 30);

	int maxCount = 150;
	const bool unlimited = !gbIsHellfire; // TODO: This could lead to an infinite loop if a suitable item can never be generated
	for (int count = 0; unlimited || count < maxCount; count++) {
		premiumItem = {};
		premiumItem._iSeed = AdvanceRndSeed();
		SetRndSeed(premiumItem._iSeed);
		_item_indexes itemType = RndPremiumItem(player, plvl / 4, plvl);
		oracool::StampVendorItemLevel(premiumItem, plvl);
		GetItemAttrs(premiumItem, itemType, plvl);
		oracool::ApplyVendorTier(premiumItem, plvl, premiumItem._iSeed,
		    ignorePriceLimits ? 0 : MaxVendorValue);
		GetItemBonus(player, premiumItem, plvl / 2, plvl, true, !gbIsHellfire, ignoreAffixLevelLimits);

		if (!gbIsHellfire) {
			if (ignorePriceLimits || premiumItem._iIvalue <= MaxVendorValue) {
				break;
			}
		} else {
			int itemValue = 0;
			switch (premiumItem._itype) {
			case ItemType::LightArmor:
			case ItemType::MediumArmor:
			case ItemType::HeavyArmor: {
				const auto *const mostValuablePlayerArmor = player.GetMostValuableItem(
				    [](const Item &item) {
					    return IsAnyOf(item._itype, ItemType::LightArmor, ItemType::MediumArmor, ItemType::HeavyArmor);
				    });

				itemValue = mostValuablePlayerArmor == nullptr ? 0 : mostValuablePlayerArmor->_iIvalue;
				break;
			}
			case ItemType::Shield:
			case ItemType::Axe:
			case ItemType::Bow:
			case ItemType::Mace:
			case ItemType::Sword:
			case ItemType::Helm:
			case ItemType::Staff:
			case ItemType::Ring:
			case ItemType::Amulet: {
				const auto *const mostValuablePlayerItem = player.GetMostValuableItem(
				    [filterType = premiumItem._itype](const Item &item) { return item._itype == filterType; });

				itemValue = mostValuablePlayerItem == nullptr ? 0 : mostValuablePlayerItem->_iIvalue;
				break;
			}
			default:
				itemValue = 0;
				break;
			}
			itemValue = itemValue * 4 / 5; // avoids forced int > float > int conversion
			if ((ignorePriceLimits || premiumItem._iIvalue <= MaxVendorValueHf)
			    && premiumItem._iMinStr <= strength
			    && premiumItem._iMinMag <= magic
			    && premiumItem._iMinDex <= dexterity
			    && premiumItem._iIvalue >= itemValue) {
				break;
			}
		}
	}
	premiumItem._iCreateInfo = plvl | CF_SMITHPREMIUM;
	premiumItem._iIdentified = true;
	premiumItem._iStatFlag = player.CanUseItem(premiumItem);
}

bool WitchItemOk(const Player &player, const ItemData &item)
{
	if (IsNoneOf(item.itype, ItemType::Misc, ItemType::Staff))
		return false;
	if (item.iMiscId == IMISC_MANA)
		return false;
	if (item.iMiscId == IMISC_FULLMANA)
		return false;
	if (item.iSpell == SpellID::TownPortal)
		return false;
	if (item.iMiscId == IMISC_FULLHEAL)
		return false;
	if (item.iMiscId == IMISC_HEAL)
		return false;
	if (item.iMiscId > IMISC_OILFIRST && item.iMiscId < IMISC_OILLAST)
		return false;
	if (item.iSpell == SpellID::Resurrect && !gbIsMultiplayer)
		return false;
	if (item.iSpell == SpellID::HealOther && !gbIsMultiplayer)
		return false;

	return true;
}

_item_indexes RndWitchItem(const Player &player, int lvl)
{
	return RndVendorItem<WitchItemOk>(player, 0, lvl);
}

_item_indexes RndBoyItem(const Player &player, int lvl)
{
	return RndVendorItem<PremiumItemOk>(player, 0, lvl);
}

bool HealerItemOk(const Player &player, const ItemData &item)
{
	if (item.itype != ItemType::Misc)
		return false;

	if (item.iMiscId == IMISC_SCROLL)
		return item.iSpell == SpellID::Healing;
	if (item.iMiscId == IMISC_SCROLLT)
		return item.iSpell == SpellID::HealOther && gbIsMultiplayer;

	if (!gbIsMultiplayer) {
		// Against MaxBaseAttribute, the cap every base stat has in this fork (ModifyPlr*, StatPointsToSpend, the hero file):
		// the class row's vanilla maximum stopped the elixirs early - a Paladin at 60 Dexterity, a Barbarian's Magic never
		// (round 10 audit, v1.12.235).
		constexpr int BaseCap = MaxBaseAttribute;
		if (item.iMiscId == IMISC_ELIXSTR)
			return player._pBaseStr < BaseCap;
		if (item.iMiscId == IMISC_ELIXMAG)
			return player._pBaseMag < BaseCap;
		if (item.iMiscId == IMISC_ELIXDEX)
			return player._pBaseDex < BaseCap;
		if (item.iMiscId == IMISC_ELIXVIT)
			return player._pBaseVit < BaseCap;
	}

	if (item.iMiscId == IMISC_REJUV)
		return true;
	if (item.iMiscId == IMISC_FULLREJUV)
		return true;

	return false;
}

_item_indexes RndHealerItem(const Player &player, int lvl)
{
	return RndVendorItem<HealerItemOk>(player, 0, lvl);
}

void RecreateSmithItem(const Player &player, Item &item, int lvl, int iseed)
{
	SetRndSeed(iseed);
	_item_indexes itype = RndSmithItem(player, lvl);
	GetItemAttrs(item, itype, lvl);

	item._iSeed = iseed;
	item._iCreateInfo = lvl | CF_SMITH;
	item._iIdentified = true;
}

void RecreatePremiumItem(const Player &player, Item &item, int plvl, int iseed)
{
	SetRndSeed(iseed);
	_item_indexes itype = RndPremiumItem(player, plvl / 4, plvl);
	GetItemAttrs(item, itype, plvl);
	GetItemBonus(player, item, plvl / 2, plvl, true, !gbIsHellfire);

	item._iSeed = iseed;
	item._iCreateInfo = plvl | CF_SMITHPREMIUM;
	item._iIdentified = true;
}

void RecreateBoyItem(const Player &player, Item &item, int lvl, int iseed)
{
	SetRndSeed(iseed);
	_item_indexes itype = RndBoyItem(player, lvl);
	GetItemAttrs(item, itype, lvl);
	GetItemBonus(player, item, lvl, 2 * lvl, true, true);

	item._iSeed = iseed;
	item._iCreateInfo = lvl | CF_BOY;
	item._iIdentified = true;
}

void RecreateWitchItem(const Player &player, Item &item, _item_indexes idx, int lvl, int iseed)
{
	if (IsAnyOf(idx, IDI_MANA, IDI_FULLMANA, IDI_PORTAL)) {
		GetItemAttrs(item, idx, lvl);
	} else if (gbIsHellfire && idx >= 114 && idx <= 117) {
		SetRndSeed(iseed);
		DiscardRandomValues(1);
		GetItemAttrs(item, idx, lvl);
	} else {
		SetRndSeed(iseed);
		_item_indexes itype = RndWitchItem(player, lvl);
		GetItemAttrs(item, itype, lvl);
		int iblvl = -1;
		if (GenerateRnd(100) <= 5)
			iblvl = 2 * lvl;
		if (iblvl == -1 && item._iMiscId == IMISC_STAFF)
			iblvl = 2 * lvl;
		if (iblvl != -1)
			GetItemBonus(player, item, iblvl / 2, iblvl, true, true);
	}

	item._iSeed = iseed;
	item._iCreateInfo = lvl | CF_WITCH;
	item._iIdentified = true;
}

void RecreateHealerItem(const Player &player, Item &item, _item_indexes idx, int lvl, int iseed)
{
	if (IsAnyOf(idx, IDI_HEAL, IDI_FULLHEAL, IDI_RESURRECT)) {
		GetItemAttrs(item, idx, lvl);
	} else {
		SetRndSeed(iseed);
		_item_indexes itype = RndHealerItem(player, lvl);
		GetItemAttrs(item, itype, lvl);
	}

	item._iSeed = iseed;
	item._iCreateInfo = lvl | CF_HEALER;
	item._iIdentified = true;
}

void RecreateTownItem(const Player &player, Item &item, _item_indexes idx, uint16_t icreateinfo, int iseed)
{
	if ((icreateinfo & CF_SMITH) != 0)
		RecreateSmithItem(player, item, icreateinfo & CF_LEVEL, iseed);
	else if ((icreateinfo & CF_SMITHPREMIUM) != 0)
		RecreatePremiumItem(player, item, icreateinfo & CF_LEVEL, iseed);
	else if ((icreateinfo & CF_BOY) != 0)
		RecreateBoyItem(player, item, icreateinfo & CF_LEVEL, iseed);
	else if ((icreateinfo & CF_WITCH) != 0)
		RecreateWitchItem(player, item, idx, icreateinfo & CF_LEVEL, iseed);
	else if ((icreateinfo & CF_HEALER) != 0)
		RecreateHealerItem(player, item, idx, icreateinfo & CF_LEVEL, iseed);
}

void CreateMagicItem(Point position, int lvl, ItemType itemType, int imid, int icurs, bool sendmsg, bool delta, bool spawn = false)
{
	if (ActiveItemCount >= MAXITEMS)
		return;

	int ii = AllocateItem();
	auto &item = Items[ii];
	_item_indexes idx = RndTypeItems(itemType, imid, lvl);

	// BOUNDED, for the reason CreateSpellBook's twin loop is (user report, 2026-09-02). This one's
	// target is still reachable - the drop pool excludes the fork's own item indices, so it rolls the
	// same vanilla bases it always did - but the shape is the hazard: roll until it matches, with
	// nothing to say what happens when it cannot. The Slain Hero proved what that costs.
	bool matched = false;
	// A unique rolled on an attempt that is thrown away does not spend that unique's one drop (round 11 audit,
	// v1.12.236): each discarded roll marked its unique found, as the vendor shelves' loops already guard against.
	std::array<bool, MaxUniqueItems> uniquesBefore;
	std::copy(std::begin(UniqueItemFlags), std::end(UniqueItemFlags), uniquesBefore.begin());
	for (int attempt = 0; attempt < 10000; attempt++) {
		item = {};
		std::copy(uniquesBefore.begin(), uniquesBefore.end(), std::begin(UniqueItemFlags));
		SetupAllItems(*MyPlayer, item, idx, AdvanceRndSeed(), 2 * lvl, 1, true, false, delta,
		    /*allowTieredRoll=*/true, std::nullopt, /*itemLevel=*/lvl);
		if (item._iCurs == icurs) {
			matched = true;
			break;
		}

		idx = RndTypeItems(itemType, imid, lvl);
	}
	if (!matched) {
		LogError("CreateMagicItem: no roll produced cursor {} for type {} at lvl {} - dropping nothing rather than hanging",
		    icurs, static_cast<int>(itemType), lvl);
		Items[ii] = {};
		ActiveItemCount--;
		std::copy(uniquesBefore.begin(), uniquesBefore.end(), std::begin(UniqueItemFlags));
		return;
	}
	if (!delta)
		FinalizeFreshDrop(item, lvl); // the drop tail - Na-Krul, the Slain Hero, the amulets (round 11 audit)
	GetSuperItemSpace(position, ii);

	if (sendmsg)
		NetSendCmdPItem(false, CMD_DROPITEM, item.position, item);
	if (delta)
		DeltaAddItem(ii);
	if (spawn)
		NetSendCmdPItem(false, CMD_SPAWNITEM, item.position, item);
}

void NextItemRecord(int i)
{
	gnNumGetRecords--;

	if (gnNumGetRecords == 0) {
		return;
	}

	itemrecord[i].dwTimestamp = itemrecord[gnNumGetRecords].dwTimestamp;
	itemrecord[i].nSeed = itemrecord[gnNumGetRecords].nSeed;
	itemrecord[i].wCI = itemrecord[gnNumGetRecords].wCI;
	itemrecord[i].nIndex = itemrecord[gnNumGetRecords].nIndex;
}

_item_indexes RndItemForMonsterLevel(int8_t monsterLevel)
{
	if (GenerateRnd(100) > 40)
		return IDI_NONE;

	if (GenerateRnd(100) > 25)
		return IDI_GOLD;

	return GetItemIndexForDroppableItem(true, [&monsterLevel](const ItemData &item) {
		return PoolQlvl(item) <= monsterLevel;
	});
}

StringOrView GetTranslatedItemName(const Item &item)
{
	// IDidx is bounded before it indexes the table. Every name in the game comes through here, so an
	// item carrying a stale or corrupt index turned an already-bad state into an out-of-bounds READ
	// on the render path - a crash while merely LOOKING at a list, which is the worst place for one
	// because it takes the evidence with it.
	//
	// Returning the empty string rather than asserting: a nameless row is a visible symptom the
	// player can report, and it leaves the rest of the panel drawable. The item is still wrong; this
	// only stops the wrongness being fatal.
	if (item.IDidx < 0 || item.IDidx > IDI_LAST)
		return string_view("");
	const auto &baseItemData = AllItemsList[static_cast<size_t>(item.IDidx)];

	if (item._iCreateInfo == 0) {
		return _(baseItemData.iName);
	} else if (item._iMiscId == IMISC_BOOK) {
		std::string name;
		const string_view spellName = pgettext("spell", GetSpellData(item._iSpell).sNameText);
		StrAppend(name, _(baseItemData.iName));
		StrAppend(name, spellName);
		return name;
	} else if (item._iMiscId == IMISC_EAR) {
		return fmt::format(fmt::runtime(_(/* TRANSLATORS: {:s} will be a Character Name */ "Ear of {:s}")), item._iIName);
	} else if (item._iMiscId > IMISC_OILFIRST && item._iMiscId < IMISC_OILLAST) {
		for (size_t i = 0; i < 10; i++) {
			if (OilMagic[i] != item._iMiscId)
				continue;
			return _(OilNames[i]);
		}
		app_fatal("unkown oil");
	} else if (item._itype == ItemType::Staff && item._iSpell != SpellID::Null && item._iMagical != ITEM_QUALITY_UNIQUE) {
		return GenerateStaffName(baseItemData, item._iSpell, true);
	} else {
		return _(baseItemData.iName);
	}
}

std::string GetTranslatedItemNameMagical(const Item &item, bool hellfireItem, bool translate, std::optional<bool> forceNameLengthCheck)
{
	std::string identifiedName;
	const auto &baseItemData = AllItemsList[static_cast<size_t>(item.IDidx)];

	int lvl = item._iCreateInfo & CF_LEVEL;
	bool onlygood = (item._iCreateInfo & (CF_ONLYGOOD | CF_SMITHPREMIUM | CF_BOY | CF_WITCH)) != 0;

	uint32_t currentSeed = GetLCGEngineState();
	SetRndSeed(item._iSeed);

	int minlvl;
	int maxlvl;
	if ((item._iCreateInfo & CF_SMITHPREMIUM) != 0) {
		DiscardRandomValues(2); // RndVendorItem and GetItemAttrs
		minlvl = lvl / 2;
		maxlvl = lvl;
	} else if ((item._iCreateInfo & CF_BOY) != 0) {
		DiscardRandomValues(2); // RndVendorItem and GetItemAttrs
		minlvl = lvl;
		maxlvl = lvl * 2;
	} else if ((item._iCreateInfo & CF_WITCH) != 0) {
		DiscardRandomValues(2); // RndVendorItem and GetItemAttrs
		int iblvl = -1;
		if (GenerateRnd(100) <= 5)
			iblvl = 2 * lvl;
		if (iblvl == -1 && item._iMiscId == IMISC_STAFF)
			iblvl = 2 * lvl;
		minlvl = iblvl / 2;
		maxlvl = iblvl;
	} else {
		DiscardRandomValues(1); // GetItemAttrs
		int iblvl = GetItemBLevel(lvl, item._iMiscId, onlygood, item._iCreateInfo & CF_UPER15);
		minlvl = iblvl / 2;
		maxlvl = iblvl;
		DiscardRandomValues(1); // CheckUnique
	}

	if (minlvl > 25)
		minlvl = 25;

	AffixItemType affixItemType = AffixItemType::None;

	switch (item._itype) {
	case ItemType::Sword:
	case ItemType::Axe:
	case ItemType::Mace:
		affixItemType = AffixItemType::Weapon;
		break;
	case ItemType::Bow:
		affixItemType = AffixItemType::Bow;
		break;
	case ItemType::Shield:
		affixItemType = AffixItemType::Shield;
		break;
	case ItemType::LightArmor:
	case ItemType::Helm:
	case ItemType::MediumArmor:
	case ItemType::HeavyArmor:
		affixItemType = AffixItemType::Armor;
		break;
	case ItemType::Staff: {
		bool allowspells = !hellfireItem || ((item._iCreateInfo & CF_SMITHPREMIUM) == 0);

		if (!allowspells)
			affixItemType = AffixItemType::Staff;
		else if (!hellfireItem && FlipCoin(4)) {
			affixItemType = AffixItemType::Staff;
		} else {
			DiscardRandomValues(2); // Spell and Charges

			int preidx = GetStaffPrefixId(maxlvl, onlygood, hellfireItem);
			if (preidx == -1 || item._iSpell == SpellID::Null) {
				if (forceNameLengthCheck) {
					// We generate names to check if it's a diablo or hellfire item. This checks fails => invalid item => don't generate a item name
					identifiedName.clear();
				} else {
					// This can happen, if the item is hacked or a bug in the logic exists
					LogWarn("GetTranslatedItemNameMagical failed for item '{}' with preidx '{}' and spellid '{}'", item._iIName, preidx, static_cast<std::underlying_type_t<SpellID>>(item._iSpell));
					identifiedName = item._iIName;
				}
			} else {
				identifiedName = GenerateStaffNameMagical(baseItemData, item._iSpell, preidx, translate, forceNameLengthCheck);
			}
		}
		break;
	}
	case ItemType::Ring:
	case ItemType::Amulet:
		affixItemType = AffixItemType::Misc;
		break;
	case ItemType::None:
	case ItemType::Misc:
	case ItemType::Gold:
		break;
	}

	if (affixItemType != AffixItemType::None) {
		const PLStruct *pPrefix = nullptr;
		const PLStruct *pSufix = nullptr;
		GetItemPowerPrefixAndSuffix(
		    minlvl, maxlvl, affixItemType, onlygood, hellfireItem,
		    [&pPrefix](const PLStruct &prefix) {
			    pPrefix = &prefix;
			    // GenerateRnd(prefix.power.param2 - prefix.power.param2 + 1)
			    DiscardRandomValues(1);
			    switch (pPrefix->power.type) {
			    case IPL_DOPPELGANGER:
			    case IPL_TOHIT_DAMP:
				    DiscardRandomValues(2);
				    break;
			    case IPL_TOHIT_DAMP_CURSE:
				    DiscardRandomValues(1);
				    break;
			    default:
				    break;
			    }
		    },
		    [&pSufix](const PLStruct &suffix) {
			    pSufix = &suffix;
		    });

		identifiedName = GenerateMagicItemName(_(baseItemData.iName), pPrefix, pSufix, translate);
		if (baseItemData.iSName != nullptr && (forceNameLengthCheck ? *forceNameLengthCheck : !StringInPanel(identifiedName.c_str()))) { // no short name: keep the long one (2026-09-25)
			identifiedName = GenerateMagicItemName(_(baseItemData.iSName), pPrefix, pSufix, translate);
		}
	}

	SetRndSeed(currentSeed);
	return identifiedName;
}

} // namespace

bool ItemTakesAffixes(const Item &item)
{
	return GetAffixItemTypeForItem(item) != AffixItemType::None;
}

// ---------------------------------------------------------------------------------------------
// Levski's Roar's two item-transforming recipes (v1.9.17).
//
// Here rather than beside SetupAllItems because that lives inside this file's big anonymous
// namespace, and these three have to be reachable from oracool/crafting.cpp. Exposed as NARROW
// operations rather than by exporting the generators: SetupAllItems is the seed-replay entry point
// and takes eleven parameters whose correct combination is a thing this file knows, and handing
// that to the crafting code would hand it the ability to produce items no drop path could.
// ---------------------------------------------------------------------------------------------

bool HasUniqueForBaseOf(const Item &item)
{
	return !UniquesForBaseOf(item).empty();
}

/**
 * @brief Clears the affix record so a reroll starts from a clean item.
 *
 * The rollers APPEND - apply writes at _iOracoolAffixCount and increments it - which is
 * correct for a freshly attributed item and wrong for one being rolled a second time. Without this,
 * rerolling the same item twice at Levski's Roar carried the first roll's affixes into the second
 * and ran off the end of the array.
 */
void ClearOracoolAffixRecord(Item &item)
{
	item._iOracoolAffixCount = 0;
	item._iOracoolAffixes = {};
	item._iOracoolPerfectRoll = false;
	item._iOracoolTier = OracoolItemTier::None;
}

namespace {

/**
 * @brief What a crafting rebuild must hand back to the item it rebuilt.
 *
 * Every tier recipe rolls its target again from a fresh seed, and GetItemAttrs starts that roll by
 * zeroing the stat fields and the ethereal flag. So Enrich, Awaken, Reforge, Ennoble and every Reroll
 * silently took away the ethereal bargain - its +35% gone, the durability quietly given back - and
 * renamed the item (audit, 2026-09-13; user decision D5: "Should crafting up a tier keep the item's
 * name?" - "Sure").
 *
 * Sockets and Mystic Orbs are not here, because no rebuild recipe accepts an item holding either:
 * every target finder rejects a socketed item, and orbed items are refused by IsTierRecipeGear and by
 * GridMaterialsFor (crafting.cpp). The save records how many orbs an item took, not which, so there
 * would be nothing to put back.
 */
struct RebuildKeepsake {
	bool ethereal = false;
	/** @brief The name came out of the name pool, as opposed to a base, unique or set name. */
	bool rolledName = false;
	std::array<char, sizeof(Item::_iIName)> name {};
	/** Kanai's Work of Cathan is for good: a rebuild through InitializeItem (Recast, Consecrate,
	 * Recolour) must not put the level requirement back (audit, 2026-09-20). */
	bool levelFree = false;
};

RebuildKeepsake CaptureRebuildKeepsake(const Item &item)
{
	RebuildKeepsake keepsake;
	keepsake.ethereal = item._iOracoolEthereal;
	keepsake.levelFree = item._iOracoolLevelFree;
	// Magic and every rolled tier are ITEM_QUALITY_MAGIC; uniques and set pieces are not.
	keepsake.rolledName = item._iMagical == ITEM_QUALITY_MAGIC && item._iOracoolTier != OracoolItemTier::Set;
	std::copy(std::begin(item._iIName), std::end(item._iIName), keepsake.name.begin());
	return keepsake;
}

/**
 * @param keepName False when the rebuild makes a named object - a unique has its own name, and
 *        keeping a rolled one would describe an item it no longer is.
 */
void RestoreRebuildKeepsake(Item &item, const RebuildKeepsake &keepsake, bool keepName)
{
	if (keepsake.ethereal && !item._iOracoolEthereal)
		MakeItemEthereal(item);
	if (keepsake.levelFree)
		item._iOracoolLevelFree = true;
	if (keepName && keepsake.rolledName && item._iMagical == ITEM_QUALITY_MAGIC)
		std::copy(keepsake.name.begin(), keepsake.name.end(), std::begin(item._iIName));
}

} // namespace

bool ReforgeOracoolItem(Item &item)
{
	// The oils survive the rebuild, as at Gillian's reroll (round 43 audit: every Cube rebuild put the base fields back -
	// Sharpness, Hardening, Permanence's indestructible - and the oil was gone). Not a unique's: the probe does not replay
	// its powers, which would read as oil work and double (round 44 audit).
	const std::optional<OracoolOilWork> oilWork = item._iMagical != ITEM_QUALITY_UNIQUE && MyPlayer != nullptr ? MeasureOracoolOilWork(*MyPlayer, item) : std::nullopt;
	if (item.isEmpty())
		return false;
	// The base tier stands, as at Retier (round 70 audit: the fresh roll drew a new one, so a Normal base could come back
	// Cruel - a free climb the base-tier recipes charge for). Read before the record is cleared.
	const auto reforgeBaseTier = static_cast<oracool::BaseItemTier>(item._iOracoolBaseTier);
	ClearOracoolAffixRecord(item);
	item._iOracoolLockedAffix = -1; // a new roll: the Mystic's lock named the old one's slot (round 36 audit)
	const int ilvl = item._iOracoolItemLevel;
	const auto idx = static_cast<_item_indexes>(item.IDidx);
	// lvl AND itemLevel both the item's own ilvl, which is what SpawnItem passes for a fresh drop:
	// there, mLevel IS the ilvl and itemLevel defaults to it. So a reforged item is distributed
	// exactly like one that had just fallen where this one did, and rerolling in town cannot
	// launder an item upward.
	const RebuildKeepsake keepsake = CaptureRebuildKeepsake(item);
	const int oldDurability = item._iDurability;
	const bool wasBroken = item._iOracoolBroken;
	// The unique flags put back after the roll, as Ennoble and the shelves do: a Reforge that came out as a vanilla unique
	// spent its one drop of the game (round 34 audit).
	std::array<bool, MaxUniqueItems> uniqueFlags;
	std::copy(std::begin(UniqueItemFlags), std::end(UniqueItemFlags), uniqueFlags.begin());
	PinnedBaseTier = reforgeBaseTier;
	SetupAllItems(*MyPlayer, item, idx, AdvanceRndSeed(), ilvl, 1, /*onlygood=*/false,
	    /*recreate=*/false, /*pregen=*/false, /*allowTieredRoll=*/true, std::nullopt, ilvl);
	PinnedBaseTier = std::nullopt;
	std::copy(uniqueFlags.begin(), uniqueFlags.end(), std::begin(UniqueItemFlags));
	RestoreRebuildKeepsake(item, keepsake, /*keepName=*/true);
	// The wear it had, as RetierOracoolItem keeps it: SetupAllItems cleared the broken flag and rolled fresh durability, a
	// free repair that bypassed Mend (round 8 audit, v1.12.233).
	if (oilWork)
		ReapplyOracoolOilWork(item, *oilWork);
	if (oldDurability != DUR_INDESTRUCTIBLE && item._iMaxDur != DUR_INDESTRUCTIBLE)
		item._iDurability = std::min<int>(oldDurability, item._iMaxDur);
	item._iOracoolBroken = wasBroken && item._iDurability == 0;
	item._iIdentified = true;
	return true;
}

bool RetierOracoolItem(Item &item, OracoolItemTier tier)
{
	// The oils survive the rebuild, as at Gillian's reroll (round 43 audit: every Cube rebuild put the base fields back -
	// Sharpness, Hardening, Permanence's indestructible - and the oil was gone). Not a unique's: the probe does not replay
	// its powers, which would read as oil work and double (round 44 audit).
	const std::optional<OracoolOilWork> oilWork = item._iMagical != ITEM_QUALITY_UNIQUE && MyPlayer != nullptr ? MeasureOracoolOilWork(*MyPlayer, item) : std::nullopt;
	if (item.isEmpty())
		return false;
	// Kept whole so a refusal below hands the item back as it came in (2026-09-25). A false return used to
	// leave the item already rebuilt - untiered, or tiered with nothing on it - while the caller treated the
	// craft as not having happened; Law of Kulle's Primal promotion, which tries a second retier on a rare
	// it has just made, would then have kept the broken result.
	const Item original = item;
	ClearOracoolAffixRecord(item);
	item._iOracoolLockedAffix = -1; // a new roll: the Mystic's lock named the old one's slot (round 36 audit)
	const int ilvl = item._iOracoolItemLevel;
	const auto idx = static_cast<_item_indexes>(item.IDidx);
	const bool forcing = tier != OracoolItemTier::None;
	const std::optional<OracoolItemTier> forced = forcing
	    ? std::optional<OracoolItemTier>(tier)
	    : std::nullopt;
	// onlygood TRUE whenever a tier is being forced, and that is load-bearing rather than
	// cosmetic. SetupAllItems only reaches the forced-tier branch when GetItemBLevel returns
	// something other than -1, and with onlygood false that call has a RANDOM component - it can
	// decide the item rolls no affixes at all. So a forced climb would silently not happen a large
	// share of the time, the recipe would return false, and the player would press Transmute on a
	// ready recipe and watch nothing occur. onlygood pins iblvl to the level, which is what makes
	// "force this tier" actually mean it.
	//
	// Left false for tier None, where the point IS to roll like an ordinary drop.
	const RebuildKeepsake keepsake = CaptureRebuildKeepsake(item);
	PinnedBaseTier = static_cast<oracool::BaseItemTier>(original._iOracoolBaseTier);
	SetupAllItems(*MyPlayer, item, idx, AdvanceRndSeed(), ilvl, 1, /*onlygood=*/forcing,
	    /*recreate=*/false, /*pregen=*/false, /*allowTieredRoll=*/true, forced, ilvl);
	PinnedBaseTier = std::nullopt;
	RestoreRebuildKeepsake(item, keepsake, /*keepName=*/true);
	// The wear it had, not a free repair (audit, 2026-09-29): SetupAllItems rolled fresh durability and cleared the broken
	// flag, so a broken item put through a reroll came back whole - Mend bypassed. The Mystic's rework keeps it the same way.
	if (oilWork)
		ReapplyOracoolOilWork(item, *oilWork);
	if (original._iDurability != DUR_INDESTRUCTIBLE && item._iMaxDur != DUR_INDESTRUCTIBLE)
		item._iDurability = std::min<int>(original._iDurability, item._iMaxDur);
	item._iOracoolBroken = original._iOracoolBroken && item._iDurability == 0;
	item._iIdentified = true;
	// A forced tier that the roller could not actually apply - the item has no affix type, say -
	// leaves the item rerolled but untiered, and the caller is told so rather than being allowed to
	// charge for a climb that did not happen.
	//
	// And a forced tier with NOTHING ON IT is refused the same way (sweep, 2026-09-25): a Rare, Buffed
	// Unique or Primal is its affixes, and one that rolled none - an item level of 0 left the whole pool
	// above its ceiling, which is what the Unique shelf's items carried until they were stamped - is a
	// plain item wearing a tier's name.
	const bool applied = tier == OracoolItemTier::None
	    || (item._iOracoolTier == tier && item._iOracoolAffixCount > 0);
	if (!applied) {
		item = original;
		return false;
	}
	return true;
}

/** @brief The first data row of power @p type in any of the pool's tables, or null. A type sits in one table only. */
const PLStruct *FindAffixRowForType(item_effect_type type)
{
	for (int i = 0; ItemPrefixes[i].power.type != IPL_INVALID; i++) {
		if (ItemPrefixes[i].power.type == type)
			return &ItemPrefixes[i];
	}
	for (int i = 0; ItemSuffixes[i].power.type != IPL_INVALID; i++) {
		if (ItemSuffixes[i].power.type == type)
			return &ItemSuffixes[i];
	}
	for (const OracoolPoolRow &row : OracoolPoolRows) {
		if (row.row.power.type == type)
			return &row.row;
	}
	return nullptr;
}

/**
 * @brief The affix that undoes @p type - a stat and its curse (Strength and weakness, swiftness and lead) - or @p type
 * itself when it has none. One item never carries both (audit, 2026-09-27): the pick excluded only the same type, and a
 * Rare off an ordinary monster could roll +Strength beside -Strength.
 */
int LargestAffixRollAtOrBelow(item_effect_type type, int level)
{
	int best = -1;
	const auto scan = [&](const PLStruct *table) {
		for (int j = 0; table[j].power.type != IPL_INVALID; j++) {
			if (table[j].power.type == type && table[j].PLMinLvl <= level)
				best = std::max({ best, table[j].power.param1, table[j].power.param2 });
		}
	};
	scan(ItemPrefixes);
	scan(ItemSuffixes);
	return best;
}

item_effect_type AffixTwinOf(item_effect_type type)
{
	constexpr std::pair<item_effect_type, item_effect_type> Twins[] = {
		{ IPL_TOHIT, IPL_TOHIT_CURSE }, { IPL_DAMP, IPL_DAMP_CURSE }, { IPL_TOHIT_DAMP, IPL_TOHIT_DAMP_CURSE },
		{ IPL_ACP, IPL_ACP_CURSE }, { IPL_STR, IPL_STR_CURSE }, { IPL_MAG, IPL_MAG_CURSE }, { IPL_DEX, IPL_DEX_CURSE },
		{ IPL_VIT, IPL_VIT_CURSE }, { IPL_ATTRIBS, IPL_ATTRIBS_CURSE }, { IPL_GETHIT, IPL_GETHIT_CURSE },
		{ IPL_LIFE, IPL_LIFE_CURSE }, { IPL_MANA, IPL_MANA_CURSE }, { IPL_DUR, IPL_DUR_CURSE },
		{ IPL_LIGHT, IPL_LIGHT_CURSE }, { IPL_MOVESPEED, IPL_MOVESPEED_CURSE },
	};
	for (const auto &[good, bad] : Twins) {
		if (type == good)
			return bad;
		if (type == bad)
			return good;
	}
	return type;
}

bool RollOracoolAffixFor(const Player &player, const Item &item, OracoolAffix &out, const item_effect_type *exclude, int excludeCount)
{
	if (item.isEmpty())
		return false;
	const int lvl = std::max<int>(1, item._iOracoolItemLevel);
	const AffixItemType flgs = GetAffixItemTypeForItem(item);
	// A SCRATCH copy: SaveItemPower writes the stat into the item as it rolls the value, and the Mystic is
	// only being offered a row to look at. The chosen one is applied for real by the rebuild below.
	Item scratch = item;
	std::vector<item_effect_type> picked(exclude, exclude + std::max(0, excludeCount));
	const std::optional<AffixCandidate> drawn = DrawUnifiedAffix(scratch, lvl, lvl, flgs, /*onlygood=*/true, gbIsHellfire,
	    /*ignoreLevelLimits=*/false, /*room=*/true, picked.data(), static_cast<int>(picked.size()), GOE_ANY);
	if (!drawn)
		return false;
	const PLStruct &affix = RowOf(*drawn);
	ItemPower power = affix.power;
	// A Primal's affixes are all perfect rolls, the offered one too (audit, 2026-09-27): the rebuild keeps the Primal's
	// perfect-roll flag, and an ordinary roll under it was a Primal that was not.
	const bool previousForcePerfectAffixRoll = ForcePerfectAffixRoll;
	ForcePerfectAffixRoll = item._iOracoolPerfectRoll;
	const int raw = SaveItemPower(player, scratch, power);
	ForcePerfectAffixRoll = previousForcePerfectAffixRoll;
	out = OracoolAffix { affix.power.type, raw, affix.multVal };
	return true;
}

/**
 * @brief Whether SaveItemPower writes @p type's two parameters into two different fields - a range's two
 * ends, or a spell and its charges - rather than rolling one value between them.
 */
static bool AffixUsesBothEndpoints(item_effect_type type)
{
	return IsAnyOf(type, IPL_FIREDAM, IPL_LIGHTDAM, IPL_FIRE_ARROWS, IPL_LIGHT_ARROWS, IPL_FIREBALL, IPL_SETDAM,
	    IPL_ADDACLIFE, IPL_ADDMANAAC, IPL_SPELL);
}

/**
 * @brief The table row @p affix was rolled from. A record keeps the type, the roll and the row's price
 * multiplier but not the row, and several rows can share a type (fire arrows: 1-3, 1-6, 1-16): the row
 * whose range holds the roll and whose multiplier matches, then any row whose range holds the roll, then
 * the first row of the type.
 */
static const PLStruct *FindAffixRowForRecord(const OracoolAffix &affix)
{
	const PLStruct *holding = nullptr;
	const auto consider = [&affix, &holding](const PLStruct &row) {
		if (row.power.type != affix.type)
			return false;
		if (affix.param1 < std::min(row.power.param1, row.power.param2) || affix.param1 > std::max(row.power.param1, row.power.param2))
			return false;
		if (row.multVal == affix.param2)
			return true;
		if (holding == nullptr)
			holding = &row;
		return false;
	};
	for (int i = 0; ItemPrefixes[i].power.type != IPL_INVALID; i++) {
		if (consider(ItemPrefixes[i]))
			return &ItemPrefixes[i];
	}
	for (int i = 0; ItemSuffixes[i].power.type != IPL_INVALID; i++) {
		if (consider(ItemSuffixes[i]))
			return &ItemSuffixes[i];
	}
	for (const OracoolPoolRow &row : OracoolPoolRows) {
		if (consider(row.row))
			return &row.row;
	}
	return holding != nullptr ? holding : FindAffixRowForType(affix.type);
}

namespace {
/** @brief Set while RebuildOracoolItemWithAffixes measures an item's oil work on a copy of itself. */
bool MeasuringOilWork = false;
} // namespace

std::optional<OracoolOilWork> MeasureOracoolOilWork(const Player &player, const Item &item)
{
	if (MeasuringOilWork || item.isEmpty())
		return std::nullopt;
	Item probe = item;
	std::array<OracoolAffix, Item::MaxOracoolAffixes> own {};
	const int ownCount = std::clamp<int>(item._iOracoolAffixCount, 0, Item::MaxOracoolAffixes);
	std::copy(item._iOracoolAffixes.begin(), item._iOracoolAffixes.begin() + ownCount, own.begin());
	MeasuringOilWork = true;
	const bool measured = RebuildOracoolItemWithAffixes(player, probe, own.data(), ownCount);
	MeasuringOilWork = false;
	if (!measured)
		return std::nullopt;
	// The armour the oils added: as recorded since format 16, or for an older item the whole difference - except a shop's
	// (round 60 audit): its seed first picked the base in the shop, so its armour roll differs from the probe's, and that
	// difference was carried on as "oil" for good. An older shop item's difference is its drift; its oil armour unknown, 0.
	const bool olderShopItem = item._iOracoolOilAC < 0 && (item._iCreateInfo & (CF_SMITH | CF_SMITHPREMIUM | CF_BOY | CF_WITCH)) != 0;
	const int oilAC = item._iOracoolOilAC >= 0 ? item._iOracoolOilAC : olderShopItem ? 0 : item._iAC - probe._iAC;
	return OracoolOilWork { item._iPLToHit - probe._iPLToHit, item._iMinDam - probe._iMinDam, item._iMaxDam - probe._iMaxDam,
		item._iMinStr - probe._iMinStr, item._iMinMag - probe._iMinMag, item._iMinDex - probe._iMinDex,
		oilAC, item._iMaxDur - probe._iMaxDur,
		// Only when the 255 was not an affix's (round 41 audit: an "of the ages" row reworked away stayed indestructible).
		item._iMaxDur == DUR_INDESTRUCTIBLE && probe._iMaxDur != DUR_INDESTRUCTIBLE,
		// The rest of the difference is the base's own drift from its seed's first draw (round 55 audit).
		item._iOracoolOilAC >= 0 || olderShopItem ? (item._iAC - probe._iAC) - oilAC : 0 };
}

void ReapplyOracoolOilWork(Item &item, const OracoolOilWork &oil, bool sameSeed)
{
	if (sameSeed)
		item._iAC = std::clamp<int>(item._iAC + oil.acDrift, 0, INT16_MAX); // the shop roll the seed cannot redraw
	item._iPLToHit += oil.toHit;
	item._iMinDam = std::clamp<int>(item._iMinDam + oil.minDam, 0, 255);
	item._iMaxDam = std::clamp<int>(item._iMaxDam + oil.maxDam, item._iMinDam, 255);
	item._iMinStr = static_cast<uint8_t>(std::clamp(item._iMinStr + oil.minStr, 0, 255));
	item._iMinMag = static_cast<uint8_t>(std::clamp(item._iMinMag + oil.minMag, 0, 255));
	item._iMinDex = static_cast<uint8_t>(std::clamp(item._iMinDex + oil.minDex, 0, 255));
	item._iAC = std::clamp<int>(item._iAC + oil.ac, 0, INT16_MAX);
	item._iOracoolOilAC = static_cast<int16_t>(std::clamp(oil.ac, 0, static_cast<int>(INT16_MAX))); // known from here on
	if (item._iMaxDur > 0 && item._iMaxDur != DUR_INDESTRUCTIBLE)
		item._iMaxDur = std::clamp(item._iMaxDur + oil.maxDur, 1, DUR_INDESTRUCTIBLE - 1);
	// Oil of Permanence: the maximum was the indestructible value itself, which the sum above cannot reach (round 40 audit:
	// it came back 254, Zod's stamp shape with no Zod, and Make Ethereal then halved it).
	if (oil.permanence && item._iMaxDur > 0) {
		item._iMaxDur = DUR_INDESTRUCTIBLE;
		item._iDurability = DUR_INDESTRUCTIBLE;
	}
}

bool RebuildOracoolItemWithAffixes(const Player &player, Item &item, const OracoolAffix *affixes, int count)
{
	if (item.isEmpty())
		return false;
	// The oils' work (round 39 audit): Accuracy, Sharpness, Death, Skill, Hardening, Imperviousness, Fortitude and the rest
	// write the base fields, which GetItemAttrs puts back - a reroll at the bench wiped every oil. Measured as the item
	// less the same item rebuilt with its own affixes, and added back to the rebuild.
	const std::optional<OracoolOilWork> oil = MeasureOracoolOilWork(player, item);
	// Rebuilt from the base and replayed, because an affix's stats are written INTO the item's fields as it
	// is rolled and there is no way to take one back out. Everything that is not an affix is carried across:
	// the seed and the level it was found at, its tier, its sockets and their stones, its shards, its name,
	// the ethereal bargain and Kanai's unbound level.
	const auto idx = static_cast<_item_indexes>(item.IDidx);
	const int ilvl = std::max<int>(1, item._iOracoolItemLevel);
	const OracoolItemTier tier = item._iOracoolTier;
	const bool perfectRoll = item._iOracoolPerfectRoll;
	const auto baseTier = static_cast<oracool::BaseItemTier>(item._iOracoolBaseTier);
	const int durability = item._iDurability;
	const bool broken = item._iOracoolBroken;
	// Whether the 255 came from an "of the ages" row this rebuild may drop (round 21 audit): only Zod's stamp or an Oil of
	// Permanence outlive the rebuild, and a rerolled-away affix left the item indestructible with no reason.
	bool agesBefore = false;
	for (int i = 0; i < item._iOracoolAffixCount; i++)
		agesBefore = agesBefore || item._iOracoolAffixes[i].type == IPL_INDESTRUCTIBLE;
	const uint32_t seed = item._iSeed;
	const uint16_t createInfo = item._iCreateInfo;
	const bool identified = item._iIdentified;
	const uint8_t sockets = item._iSocketCount;
	uint16_t socketed[Item::MaxItemSockets];
	std::copy(std::begin(item._iSocketed), std::end(item._iSocketed), std::begin(socketed));
	const RebuildKeepsake keepsake = CaptureRebuildKeepsake(item);
	const oracool::ImbuementLedger ledger = oracool::CaptureImbuements(item);
	// A staff's spell is not an affix, and GetItemAttrs puts the base's back (none, and no charges). Magic
	// staves reach Gillian now that their affix sits on the list (2026-09-25), so the spell, its charges and
	// the magic requirement the spell set are carried across like the sockets are.
	const bool keepsStaffSpell = item._iMiscId == IMISC_STAFF;
	const SpellID staffSpell = item._iSpell;
	// The charges WITHOUT the old record's Plentiful/Bountiful multiplier, and the new record's put on (round 45 audit: a
	// rework that replaced the row kept the doubled or tripled charges).
	const auto chargeMultiplier = [](const OracoolAffix *rows, int rowCount) {
		int multiplier = 1;
		for (int i = 0; i < rowCount; i++) {
			if (rows[i].type == IPL_CHARGES && rows[i].param1 > 1)
				multiplier *= rows[i].param1;
		}
		return multiplier;
	};
	const int oldChargeMultiplier = chargeMultiplier(item._iOracoolAffixes.data(), std::clamp<int>(item._iOracoolAffixCount, 0, Item::MaxOracoolAffixes));
	const int newChargeMultiplier = chargeMultiplier(affixes, std::max(count, 0));
	// Untouched when the multiplier is (round 46 audit: divide-then-multiply took 8/18 down to 6/18 on every rework), and
	// rescaled in 64 bits otherwise, multiplying first.
	const auto rescaled = [&](int charges) {
		return oldChargeMultiplier == newChargeMultiplier ? charges
		                                                  : static_cast<int>(static_cast<int64_t>(charges) * newChargeMultiplier / oldChargeMultiplier);
	};
	const int staffCharges = rescaled(item._iCharges);
	const int staffMaxCharges = rescaled(item._iMaxCharges);
	const uint8_t staffMinMag = item._iMinMag;
	// The spell's share of the base value (GetStaffSpell adds it): GetItemAttrs puts the bare base value back, and a
	// reworked staff was priced as a plain stick with the same spell on it (round 70 audit).
	const int staffValueBefore = item._ivalue;
	// Copied first: GetItemAttrs empties the item's affix list, and a caller may hand in that very list.
	std::array<OracoolAffix, Item::MaxOracoolAffixes> wanted {};
	const int wantedCount = std::clamp(count, 0, Item::MaxOracoolAffixes);
	std::copy(affixes, affixes + wantedCount, wanted.begin());

	// The to-hit half of King's, Dull and Doppelganger is not in the record, and replaying the row drew it again from the
	// game's RNG: reworking any OTHER row moved King's +80% anywhere in 76-100 (round 29 audit). When the rework keeps those
	// rows as they were, their to-hit is put back as it was - the share the tooltip reads (the item's to-hit less the plain
	// to-hit rows).
	const auto dampToHitShare = [](const Item &it) {
		int toHit = it._iPLToHit;
		for (int i = 0; i < it._iOracoolAffixCount; i++) {
			if (it._iOracoolAffixes[i].type == IPL_TOHIT)
				toHit -= it._iOracoolAffixes[i].param1;
			else if (it._iOracoolAffixes[i].type == IPL_TOHIT_CURSE)
				toHit += it._iOracoolAffixes[i].param1;
		}
		return toHit;
	};
	const auto isDampRow = [](const OracoolAffix &affix) {
		return IsAnyOf(affix.type, IPL_TOHIT_DAMP, IPL_TOHIT_DAMP_CURSE, IPL_DOPPELGANGER);
	};
	bool keepsDampRows = true;
	int dampRows = 0;
	{
		std::vector<std::pair<int, int>> before;
		std::vector<std::pair<int, int>> after;
		for (int i = 0; i < item._iOracoolAffixCount; i++) {
			if (isDampRow(item._iOracoolAffixes[i]))
				before.emplace_back(item._iOracoolAffixes[i].type, item._iOracoolAffixes[i].param1);
		}
		for (int i = 0; i < wantedCount; i++) {
			if (isDampRow(wanted[i]))
				after.emplace_back(wanted[i].type, wanted[i].param1);
		}
		std::sort(before.begin(), before.end());
		std::sort(after.begin(), after.end());
		keepsDampRows = before == after;
		dampRows = static_cast<int>(before.size());
	}
	const int dampToHitBefore = dampToHitShare(item);

	const int priceBefore = item._iIvalue;
	// The base's own rolls - its armour among them - come from the item's seed, as SetupAllItems seeds them (audit,
	// 2026-09-29: unseeded, every rework rerolled the base armour, down as often as up). The game's RNG is put back after.
	const uint32_t rngState = GetLCGEngineState();
	SetRndSeed(seed);
	GetItemAttrs(item, idx, ilvl);
	SetRndSeed(rngState);
	item._iSeed = seed;
	item._iCreateInfo = createInfo;
	item._iOracoolItemLevel = static_cast<uint8_t>(ilvl);
	int priceAddTotal = 0;
	int priceMultTotal = 0;
	// The record is cleared FIRST and the tier restored AFTER (sweep, 2026-09-25). It was the other way round,
	// and ClearOracoolAffixRecord resets the tier - so every Rare, Buffed Unique or Primal Gillian reworked
	// came back a magic item.
	ClearOracoolAffixRecord(item);
	item._iOracoolTier = tier;
	item._iOracoolPerfectRoll = perfectRoll;
	// The BASE tier on the base numbers before any affix touches them, as SetupAllItems does. GetItemAttrs
	// wrote the Normal numbers, so a Jagged or Cruel base came back from the bench with Normal damage and
	// armour while its tooltip still named the tier.
	oracool::ApplyBaseTier(item, baseTier);
	// The rolls a record does not store (the to-hit half of King's and Doppelganger) come back perfect on a Primal, as
	// they were rolled: a rework of any row left its +100 anywhere in 76-100 (round 14 audit, v1.12.239).
	const bool previousForcePerfectAffixRoll = ForcePerfectAffixRoll;
	ForcePerfectAffixRoll = perfectRoll;
	for (int i = 0; i < wantedCount && item._iOracoolAffixCount < Item::MaxOracoolAffixes; i++) {
		const PLStruct *row = FindAffixRowForRecord(wanted[i]);
		if (row == nullptr)
			continue;
		// A degenerate range, so the roll lands on exactly the value this affix already had. Not for a power
		// that writes its two parameters to two fields (external audit of v1.12.188, ITEM-02): a 3-9 fire
		// damage affix came back from the Mystic as 3-3. Those replay their row's own pair.
		ItemPower power = row->power;
		if (!AffixUsesBothEndpoints(wanted[i].type)) {
			power.param1 = wanted[i].param1;
			power.param2 = wanted[i].param1;
		}
		SaveItemPower(player, item, power);
		item._iOracoolAffixes[item._iOracoolAffixCount++] = OracoolAffix { wanted[i].type, wanted[i].param1, wanted[i].param2 };
		priceAddTotal += PLVal(wanted[i].param1, row->power.param1, row->power.param2, row->minVal, row->maxVal);
		priceMultTotal += row->multVal;
	}
	ForcePerfectAffixRoll = previousForcePerfectAffixRoll;
	if (keepsDampRows && dampRows > 0)
		item._iPLToHit += dampToHitBefore - dampToHitShare(item);
	if (item._iOracoolAffixCount > 0 && item._iMagical == ITEM_QUALITY_NORMAL)
		item._iMagical = ITEM_QUALITY_MAGIC;
	if (keepsStaffSpell) {
		item._iSpell = staffSpell;
		item._iCharges = staffCharges;
		item._iMaxCharges = staffMaxCharges;
		item._iMinMag = staffMinMag;
		item._ivalue = std::max(item._ivalue, staffValueBefore);
	}
	item._iSocketCount = sockets;
	std::copy(std::begin(socketed), std::end(socketed), std::begin(item._iSocketed));
	// The ethereal bargain before the shards, the order a drop has (audit, 2026-09-29): the other way round, the halving
	// took Tempering's durability down with the base's.
	RestoreRebuildKeepsake(item, keepsake, /*keepName=*/true);
	oracool::RestoreImbuements(item, ledger);
	if (oil)
		ReapplyOracoolOilWork(item, *oil, /*sameSeed=*/true); // the durability below then stays 255 under Permanence too
	// The wear the item had, not a free repair (sweep, 2026-09-25): GetItemAttrs set durability to the base's
	// full value, so a reroll at the bench mended the item as a side effect. Capped by the maximum the rebuild
	// arrived at (an ethereal item's is halved again above); an item that is now indestructible stays so.
	// Zod's stamp (durability indestructible, the maximum left intact) survives it (audit, 2026-09-29: clamped to the
	// maximum, a socketed Zod stopped working while the tooltip still promised it).
	if (durability == DUR_INDESTRUCTIBLE && agesBefore && item._iMaxDur != DUR_INDESTRUCTIBLE && !oracool::SocketsMakeIndestructible(item))
		item._iDurability = item._iMaxDur; // the affix went: whole, but wearing again
	else if (durability == DUR_INDESTRUCTIBLE)
		item._iDurability = DUR_INDESTRUCTIBLE;
	else if (item._iMaxDur != DUR_INDESTRUCTIBLE)
		item._iDurability = std::min(durability, item._iMaxDur);
	item._iOracoolBroken = broken && item._iDurability == 0;
	item._iIdentified = identified;
	// The price from the affixes it has now, as the roller priced them (audit, 2026-09-27): GetItemAttrs put the BASE
	// value back and nothing re-priced the item, so every item reworked at the bench sold for its base. A unique's
	// price is its own row's, not its affixes' - it keeps what it had.
	if (item._iMagical == ITEM_QUALITY_MAGIC)
		CalcOracoolTieredItemValue(item, priceAddTotal, priceMultTotal);
	else if (item._iMagical == ITEM_QUALITY_UNIQUE)
		item._iIvalue = priceBefore;
	return true;
}

bool EnnobleOracoolRare(Item &item)
{
	// The oils survive the rebuild, as at Gillian's reroll (round 43 audit: every Cube rebuild put the base fields back -
	// Sharpness, Hardening, Permanence's indestructible - and the oil was gone). Not a unique's: the probe does not replay
	// its powers, which would read as oil work and double (round 44 audit).
	const std::optional<OracoolOilWork> oilWork = item._iMagical != ITEM_QUALITY_UNIQUE && MyPlayer != nullptr ? MeasureOracoolOilWork(*MyPlayer, item) : std::nullopt;
	std::vector<int> candidates = UniquesForBaseOf(item);
	if (candidates.empty())
		return false;

	// Rerolling an item that is ALREADY unique must not be able to hand back the same one. This
	// picked uniformly from every unique the base supports, its current identity included, so
	// "Reroll Uniques" could consume its reagents and change nothing - and with the small candidate
	// lists some bases have, not rarely (external audit, 2026-08-25).
	//
	// A no-op for recipe 6, "Ennoble Rares", which comes in on a RARE and therefore has no current
	// unique to exclude. And the exclusion stands down when it would empty the pool: a base with
	// exactly one unique reroll rolls itself, which is at least honest, where refusing outright
	// would look like a broken recipe.
	if (item._iMagical == ITEM_QUALITY_UNIQUE && candidates.size() > 1) {
		const int current = item._iUid;
		candidates.erase(std::remove(candidates.begin(), candidates.end(), current), candidates.end());
		if (candidates.empty())
			candidates = UniquesForBaseOf(item);
	}

	ClearOracoolAffixRecord(item);
	item._iOracoolLockedAffix = -1; // a new roll: the Mystic's lock named the old one's slot (round 36 audit)
	const int uid = candidates[GenerateRnd(static_cast<int32_t>(candidates.size()))];
	const int ilvl = item._iOracoolItemLevel;
	const auto idx = static_cast<_item_indexes>(item.IDidx);
	const RebuildKeepsake keepsake = CaptureRebuildKeepsake(item);
	// The base tier and the wear survive the rebuild (round 8 audit, v1.12.233): GetItemAttrs writes Normal numbers and
	// the tier stayed printed on them, and the fresh durability was a free repair.
	const auto baseTier = static_cast<oracool::BaseItemTier>(item._iOracoolBaseTier);
	const int oldDurability = item._iDurability;
	const bool wasBroken = item._iOracoolBroken;
	GetItemAttrs(item, idx, ilvl);
	oracool::ApplyBaseTier(item, baseTier);
	// A crafted unique does not spend that unique's one drop, as the Unique shelf's does not: every Reroll Uniques press
	// struck one more from the drop pool for the rest of the game (round 16 audit, v1.12.241).
	const bool wasFound = UniqueItemFlags[uid];
	GetUniqueItem(*MyPlayer, item, static_cast<_unique_items>(uid));
	UniqueItemFlags[uid] = wasFound;
	SetupItem(item);
	// The ethereal bargain before the oils, as Reforge and Retier order them (round 44 audit: after, it took its +35% and its
	// halving on the oil's work too).
	RestoreRebuildKeepsake(item, keepsake, /*keepName=*/false);
	if (oilWork)
		ReapplyOracoolOilWork(item, *oilWork);
	if (oldDurability != DUR_INDESTRUCTIBLE && item._iMaxDur != DUR_INDESTRUCTIBLE)
		item._iDurability = std::min<int>(oldDurability, item._iMaxDur);
	item._iOracoolBroken = wasBroken && item._iDurability == 0;
	// Restored AFTER GetItemAttrs, which sets the item level from its own lvl argument - otherwise
	// an ennobled item forgets the depth it was found at, and a later reforge would roll it at
	// whatever GetItemAttrs happened to leave behind.
	item._iOracoolItemLevel = static_cast<uint8_t>(ilvl);
	item._iIdentified = true;
	return true;
}

/**
 * @brief Applies one ItemPower to @p item exactly as the affix roller does.
 *
 * A thin door onto SaveItemPower, which is file-local here and should stay that way - it is a big
 * switch with a lot of neighbours it belongs beside. The fifteen item sets need to apply their stats
 * from oracool/item_sets.cpp, and going through this rather than reimplementing the switch is what
 * guarantees a set item's stats land in the same fields, with the same signs and the same flag
 * semantics, as every other item's.
 *
 * Takes the power BY VALUE because SaveItemPower's parameter is non-const (it rolls ranges in place)
 * while the set tables are constant data.
 */
void ApplyItemPower(const Player &player, Item &item, ItemPower power)
{
	SaveItemPower(player, item, power);
}

/** @brief The word placed before the base item name for a tiered item's display name, e.g. "Rare {base}". */
string_view GetOracoolTierLabel(OracoolItemTier tier)
{
	switch (tier) {
	case OracoolItemTier::Rare:
		return _("Rare");
	case OracoolItemTier::BuffedUnique:
		return _("Unique");
	case OracoolItemTier::Primal:
		return _("Primal");
	case OracoolItemTier::Set:
		return _("Set");
	case OracoolItemTier::None:
		break;
	}
	return {};
}

/**
 * @brief The description-panel line shown under the belt row for a tiered item, matching
 * vanilla's own lowercase "unique item" convention for real Unique items. Buffed Unique uses
 * the same "unique item" wording as a real Unique (it's meant to visually blend in), while
 * Rare and Primal get their own distinct wording.
 */
string_view GetOracoolTierPanelLabel(OracoolItemTier tier)
{
	switch (tier) {
	case OracoolItemTier::Rare:
		return _("rare item");
	case OracoolItemTier::BuffedUnique:
		return _("unique item");
	case OracoolItemTier::Primal:
		return _("primal item");
	case OracoolItemTier::Set:
		return _("set item");
	case OracoolItemTier::None:
		break;
	}
	return {};
}

/**
 * @brief Just the quality word, without the noun the panel now supplies itself.
 *
 * Split out of GetOracoolTierPanelLabel rather than replacing it: that one is still what the label
 * reads as a whole phrase, and the panel line needs the halves separately so it can put the item's
 * real type where "item" used to be (user, 2026-08-30). Buffed Unique still says "unique", which is
 * the whole point of that tier - it is meant to read as a real Unique.
 */
string_view GetOracoolTierQualityWord(OracoolItemTier tier)
{
	switch (tier) {
	case OracoolItemTier::Rare:
		return _("rare");
	case OracoolItemTier::BuffedUnique:
		return _("unique");
	case OracoolItemTier::Primal:
		return _("primal");
	case OracoolItemTier::Set:
		return _("set");
	case OracoolItemTier::None:
		break;
	}
	return {};
}

/**
 * @brief What the item IS, for the quality line - "ring", "boots", "sword" (user, 2026-08-30).
 *
 * Row two used to end in the word "item" for everything: "unique item", "basic item". The quality
 * was doing all the work and the noun none, which is a wasted line on a panel where every line is
 * scarce. It now names the actual kind, so the row reads "unique ring" or "basic pants".
 *
 * The EQUIP SLOT answers first, because that is what the player is shopping for - a helm is a helm
 * whether its ItemType says LightArmor or Helm. ItemType only decides the weapons, where the slot
 * says no more than "one hand" and the difference between a sword and a bow is the whole point.
 *
 * Never empty for anything this line is printed for: the caller has already excluded ILOC_NONE,
 * ILOC_UNEQUIPABLE and ILOC_BELT, and every remaining slot has a case. The fallback exists for a
 * slot added later, and deliberately returns the old wording rather than nothing.
 */
string_view GetItemTypeNoun(const Item &item)
{
	switch (item._iLoc) {
	case ILOC_HELM:
		return _("helm");
	case ILOC_ARMOR:
		return _("armor");
	case ILOC_RING:
		return _("ring");
	case ILOC_AMULET:
		return _("amulet");
	case ILOC_SHOULDERS:
		return _("shoulders");
	case ILOC_BRACERS:
		return _("bracers");
	case ILOC_GLOVES:
		return _("gloves");
	case ILOC_WAIST:
		return _("belt");
	case ILOC_LEGS:
		return _("pants");
	case ILOC_BOOTS:
		return _("boots");
	case ILOC_ONEHAND:
	case ILOC_TWOHAND:
		break; // the slot cannot tell a sword from a bow - ItemType below can
	default:
		return _("item");
	}

	// The fork's own weapon and off-hand bases borrow a vanilla ItemType for the hero's animation and the
	// family rules - the Pike is an Axe, the Spear a Sword, the Necromancer's scythes Axes - and the type's
	// word then named them wrongly: a Pike unique read "unique axe" under a spear-shaped icon (user,
	// 2026-09-26 dev note: "i had a spear sprite whose name was axe"). The base answers first.
	if (item.IDidx >= 0 && item.IDidx <= IDI_LAST) {
		const unique_base_item base = AllItemsList[static_cast<size_t>(item.IDidx)].iItemId;
		switch (base) {
		case UITYPE_SPEAR:
			return _("spear");
		case UITYPE_PIKE:
			return _("pike");
		case UITYPE_WARLUTE:
			return _("lute");
		case UITYPE_WARQUIVER:
			return _("quiver");
		case UITYPE_CANTICLE:
			return _("canticle");
		case UITYPE_ARCANEFOCUS:
			return _("focus");
		default:
			break;
		}
		if (base >= UITYPE_NECRO_WAND_BONE_WAND && base <= UITYPE_NECRO_WAND_UNHOLY_WAND)
			return _("wand");
		if (base >= UITYPE_NECRO_SCYTHE_REAPING_SCYTHE && base <= UITYPE_NECRO_SCYTHE_DEATHBRINGER)
			return _("scythe");
		if (base >= UITYPE_NECRO_HEAD_PRESERVED_HEAD && base <= UITYPE_NECRO_HEAD_BLOODLORD_SKULL)
			return _("head");
	}

	switch (item._itype) {
	case ItemType::Sword:
		return _("sword");
	case ItemType::Axe:
		return _("axe");
	case ItemType::Bow:
		return _("bow");
	case ItemType::Mace:
		return _("mace");
	case ItemType::Staff:
		return _("staff");
	case ItemType::Shield:
		return _("shield");
	default:
		return _("weapon");
	}
}

/**
 * @brief Generates an Oracool-tiered item's affixes: exactly @p guaranteedAffixes drawn from the one pool (the
 * tier's minimum identity requirement - an unconditional guarantee, not a common case), then two independent
 * bonusAffixChancePercent chances for one more each, capped at Item::MaxOracoolAffixes - weighted toward fewer
 * total affixes by design. Any combination of tables is legal: all prefixes, all suffixes, or any mix (user,
 * 2026-09-13).
 * Shared engine behind GetRareItemAffixes (2 guaranteed), GetBuffedUniqueItemAffixes (4) and
 * GetPrimalItemAffixes (6, perfectRoll=true).
 * Reuses vanilla's exact roll-and-apply primitives (SaveItemPower/PLVal) so the real _iPL*
 * stat bonuses are identical in kind to an ordinary magic item's; only the identity bookkeeping
 * (which affixes, at what rolled value) goes into Item::_iOracoolAffixes
 * instead of the vanilla _iVAdd/_iVMult fields, which only have room for one of each.
 *
 * The guaranteed loop always relaxes the FLOOR of the caller's level window, regardless of
 * the ignoreLevelLimits argument or tier: a low-level drop or a narrow-pool item class like jewelry
 * can exhaust every eligible entry once a few affixes are already excluded as duplicates, and the
 * minimum count is a stated guarantee. It never relaxes the CEILING (user, 2026-09-13: "all items
 * including oracool invented ones should abide the ilvl-affix level corelation") - it lifted both until
 * then, which put level-60 affixes on floor-1 Rares and Primals. The duplicate-type and Good/Evil
 * exclusions are never relaxed. The optional bonus-affix rolls below respect the ignoreLevelLimits
 * argument as passed.
 *
 * @param perfectRoll When true, every affix's magnitude is forced to the maximum end of its
 * declared range (via RndPL, see ForcePerfectAffixRoll) instead of being randomly rolled, and
 * only beneficial (PLOk) affixes are ever considered regardless of the onlygood argument - a
 * maxed-out curse/drawback affix would contradict "perfect roll" being an unambiguous upgrade.
 * It also forces ignoreLevelLimits for the bonus-affix rolls (moot for Primal today, since its
 * guarantee already equals the hard cap and the bonus rolls never fire).
 */
void GetTieredItemAffixes(const Player &player, Item &item, int minlvl, int maxlvl, AffixItemType flgs, bool onlygood, OracoolItemTier tier, int guaranteedAffixes, int bonusAffixChancePercent, bool ignoreLevelLimits, bool perfectRoll = false)
{
	if (perfectRoll) {
		onlygood = true;
		ignoreLevelLimits = true;
	}

	std::array<item_effect_type, Item::MaxOracoolAffixes> pickedTypes {};
	int pickedCount = 0;
	goodorevil goe = GOE_ANY;
	// Oracool bug fix: user report - a Buffed Unique "Crown of the Eagle" showed nonsense like
	// "Resist Lightning: +10290%" and "+1000 to Dexterity". PLVal's return value is vanilla's own
	// PRICE-contribution scaling (the same minVal/maxVal gold-value range SaveItemAffix feeds into
	// item._iVAdd1/_iVMult1), not the displayed stat magnitude - that's `raw`, straight out of
	// SaveItemPower before any price scaling. OracoolAffix.param1 was storing the price-scaled
	// `value` (routinely in the thousands) instead of `raw` (the real roll, e.g. 51-60 for a
	// Lightning Resist affix) - PrintOracoolAffixPower always displayed param1 directly, so every
	// tiered item's tooltip showed price noise instead of its real stats. The item's actual
	// in-memory stat fields (_iPLLR, _iPLDex, ...) were never affected - SaveItemPower applies
	// those correctly as a side effect before PLVal is even called - so this was a display-only
	// bug, not a combat-balance one. Price still needs PLVal's scaled output, so it's now
	// accumulated locally here instead of being smuggled through OracoolAffix.
	int priceAddTotal = 0;
	int priceMultTotal = 0;

	// ONE POOL and ONE LIST for every tier (user, 2026-09-13: affixes are not segregated into prefixes and suffixes -
	// "i now want all possible combinations of affixes to be able to occur ... including ONLY prefixes and ONLY
	// suffixes for ALL affixes slots these items have"). A drawn affix takes the next free place in
	// _iOracoolAffixes whatever table it came from, so a Primal can be six prefixes, six suffixes or any mix.
	auto apply = [&](AffixCandidate drawn) {
		if (pickedCount >= static_cast<int>(std::size(pickedTypes)))
			return;
		// BOUNDED. Levski's Roar rerolls run this over an item that already carries affixes, and an
		// off-by-one anywhere in the pipeline should drop an affix, never corrupt memory.
		if (item._iOracoolAffixCount >= Item::MaxOracoolAffixes)
			return;
		const PLStruct &affix = RowOf(drawn);
		ItemPower power = affix.power;
		const int raw = SaveItemPower(player, item, power);
		const int value = PLVal(raw, power.param1, power.param2, affix.minVal, affix.maxVal);
		item._iOracoolAffixes[item._iOracoolAffixCount++] = OracoolAffix { affix.power.type, raw, affix.multVal };
		pickedTypes[pickedCount++] = affix.power.type;
		priceAddTotal += value;
		priceMultTotal += affix.multVal;
		if (affix.PLGOE != GOE_ANY)
			goe = affix.PLGOE;
	};
	const auto draw = [&](bool withLevelLimits) {
		// Every table is offered while the list has room: the only limit is the count.
		const bool room = item._iOracoolAffixCount < Item::MaxOracoolAffixes;
		return DrawUnifiedAffix(item, minlvl, maxlvl, flgs, onlygood, gbIsHellfire, !withLevelLimits,
		    room, pickedTypes.data(), pickedCount, goe);
	};

	const bool previousForcePerfectAffixRoll = ForcePerfectAffixRoll;
	ForcePerfectAffixRoll = perfectRoll;

	// The guaranteed affixes may go BELOW the level window - a ring in a narrow window has a small pool, and a
	// Rare should not ship with fewer than its guarantee - but never above the item's level (2026-09-13).
	for (int i = 0; i < guaranteedAffixes; i++) {
		if (const std::optional<AffixCandidate> drawn = draw(/*withLevelLimits=*/false))
			apply(*drawn);
	}
	// Two chances at one more each - a Rare lands on 2, 3 or 4 affixes, and a Buffed Unique on 4, 5 or 6.
	// Primal passes no bonus chance.
	for (int bonus = 0; bonus < 2 && bonusAffixChancePercent > 0; bonus++) {
		if (GenerateRnd(100) >= bonusAffixChancePercent)
			continue;
		if (const std::optional<AffixCandidate> drawn = draw(/*withLevelLimits=*/!ignoreLevelLimits))
			apply(*drawn);
	}
	ForcePerfectAffixRoll = previousForcePerfectAffixRoll;

	CalcOracoolTieredItemValue(item, priceAddTotal, priceMultTotal);

	item._iMagical = ITEM_QUALITY_MAGIC;
	item._iOracoolTier = tier;
	item._iOracoolPerfectRoll = perfectRoll;

	// THE NAME POOL, same as a magic item's (user, 2026-09-13). This was "{TierLabel} {BaseName}" -
	// "Rare Cap", "Primal Great Helm" - which is readable but says nothing and repeats endlessly
	// across a run. The tier is still on the tooltip's own Tier line and in the name's colour, so
	// nothing is lost by taking the label out of the name itself.
	std::string tieredName = oracool::GenerateOracoolItemName(item._iSeed);
	// Not on a base with no short name: fmt throws on a null C string (round 14 audit, v1.12.239).
	if (!StringInPanel(tieredName.c_str()) && AllItemsList[item.IDidx].iSName != nullptr) {
		const string_view tierLabel = GetOracoolTierLabel(tier);
		tieredName = fmt::format(fmt::runtime(_("{0} {1}")), tierLabel, AllItemsList[item.IDidx].iSName);
	}
	CopyUtf8(item._iIName, tieredName, sizeof(item._iIName));
}

void GetRareItemAffixes(const Player &player, Item &item, int minlvl, int maxlvl, AffixItemType flgs, bool onlygood, bool ignoreLevelLimits)
{
	constexpr int BonusAffixChancePercent = 30;
	GetTieredItemAffixes(player, item, minlvl, maxlvl, flgs, onlygood, OracoolItemTier::Rare, /*guaranteedAffixes=*/2, BonusAffixChancePercent, ignoreLevelLimits);
}

void GetBuffedUniqueItemAffixes(const Player &player, Item &item, int minlvl, int maxlvl, AffixItemType flgs, bool onlygood, bool ignoreLevelLimits)
{
	constexpr int BonusAffixChancePercent = 30;
	GetTieredItemAffixes(player, item, minlvl, maxlvl, flgs, onlygood, OracoolItemTier::BuffedUnique, /*guaranteedAffixes=*/4, BonusAffixChancePercent, ignoreLevelLimits);
}

void GetPrimalItemAffixes(const Player &player, Item &item, int minlvl, int maxlvl, AffixItemType flgs, bool onlygood, bool ignoreLevelLimits)
{
	// The guarantee already equals Item::MaxOracoolAffixes, so the bonus-affix roll inside GetTieredItemAffixes
	// can never fire (the count-below-cap guard blocks it) - bonusAffixChancePercent is 0 here for clarity.
	GetTieredItemAffixes(player, item, minlvl, maxlvl, flgs, onlygood, OracoolItemTier::Primal, /*guaranteedAffixes=*/Item::MaxOracoolAffixes, 0, ignoreLevelLimits, /*perfectRoll=*/true);
}

/**
 * @brief Oracool-tiered-item equivalent of CalcItemValue: applies the same add/mult-scaling shape
 * vanilla's own _iVAdd/_iVMult pricing uses, generalized to the caller's own running totals across
 * however many affixes a tiered item actually has (vanilla's 2-slot fields can't represent more
 * than one prefix and one suffix). addTotal/multTotal must be the sum of each affix's own PLVal
 * price contribution and multVal, accumulated by the caller while rolling the affixes - see
 * GetTieredItemAffixes, and the comment on the declaration in items.h for why this can't be
 * re-derived later from the stored OracoolAffix entries.
 */
void CalcOracoolTieredItemValue(Item &item, int addTotal, int multTotal)
{
	int v = multTotal;
	if (v > 0)
		v *= item._ivalue;
	if (v < 0)
		v = item._ivalue / v;
	v = addTotal + v;
	item._iIvalue = std::max(v, 1);
}

bool RepairOracoolAffixesIfCorrupted(Item & /*item*/)
{
	// Retired (audit, 2026-09-29). It repaired the v0.3.42 display bug (2026-08-07), and every item a save can still hold
	// - item format 13-15, from 2026-09-20 on - was written long after that fix. What it still caught were legitimate
	// values: a Blood or Hit Power craft merged into the Rare's own life or to-hit affix sums past every row's range into
	// some row's price range, and was "corrected" to 10 life or 1 to-hit - made permanent by the next rework. Kept, and
	// answering "nothing repaired", so its callers need not change.
	return false;
}

bool IsItemAvailable(int i)
{
	if (i < 0 || i > IDI_LAST)
		return false;

	if (gbIsSpawn) {
		if (i >= 62 && i <= 70)
			return false; // Medium and heavy armors
		if (IsAnyOf(i, 105, 107, 108, 110, 111, 113))
			return false; // Unavailable scrolls
	}

	if (gbIsHellfire)
		return true;

	return (
	           i != IDI_MAPOFDOOM                   // Cathedral Map
	           && i != IDI_LGTFORGE                 // Bovine Plate
	           && (i < IDI_OIL || i > IDI_GREYSUIT) // Hellfire exclusive items
	           && (i < 83 || i > 86)                // Oils
	           && i != 92                           // Scroll of Search
	           && (i < 161 || i > 165)              // Runes
	           && i != IDI_SORCERER                 // Short Staff of Mana
	           )
	    || (
	        // Bard items are technically Hellfire-exclusive
	        // but are just normal items with adjusted stats.
	        //
	        // Oracool: no longer gated on the Test Bard switch. This function does not only decide what
	        // can be generated - it also decides what survives UnPackItem (pack.cpp:330), so with the
	        // switch in the condition, turning the Bard off silently deleted an existing Bard's starting
	        // Sword and Dagger the next time that character loaded. Nothing is given up by keeping them
	        // available unconditionally: both are IDROP_NEVER, and every generation path - loot, all
	        // four vendors, uniques - skips IDROP_NEVER, so they still cannot spawn anywhere. The only
	        // way to hold one remains being a Bard.
	        IsAnyOf(i, IDI_BARDSWORD, IDI_BARDDAGGER));
}

int GetItemSellValue(const Item &item)
{
	int value = item._iMagical != ITEM_QUALITY_NORMAL && item._iIdentified ? item._iIvalue : item._ivalue;
	value = std::max(value / 4, 1);
	if (item.isStackableConsumable())
		value *= item.stackCount();
	return value;
}

uint8_t GetOutlineColor(const Item &item, bool checkReq)
{
	if (checkReq && !item._iStatFlag)
		return ICOL_RED;
	if (item._itype == ItemType::Gold)
		return ICOL_YELLOW;
	if (item._iMagical == ITEM_QUALITY_MAGIC)
		return ICOL_BLUE;
	if (item._iMagical == ITEM_QUALITY_UNIQUE)
		return ICOL_YELLOW;

	return ICOL_WHITE;
}

bool IsUniqueAvailable(int i)
{
	return gbIsHellfire || i <= 89;
}

bool CreateUniqueVendorItem(const Player &player, Item &item, _unique_items uid)
{
	if (uid == UITEM_INVALID || !IsUniqueAvailable(uid))
		return false;
	// A unique on a hidden class's base is not for sale (oracool/hidden_classes.h).
	if (oracool::IsHiddenItemBase(UniqueItems[uid].UIItemId))
		return false;

	_item_indexes baseItemIndex = IDI_GOLD;
	for (std::underlying_type_t<_item_indexes> i = IDI_GOLD; i <= IDI_LAST; ++i) {
		if (IsItemAvailable(i) && AllItemsList[i].iItemId == UniqueItems[uid].UIItemId) {
			baseItemIndex = static_cast<_item_indexes>(i);
			break;
		}
	}
	if (baseItemIndex == IDI_GOLD)
		return false;
	// A unique on a shrunken head is for a Necromancer only, as the heads themselves are (audit,
	// 2026-09-24): the shelf offered them to heroes who cannot equip them.
	if (oracool::IsNecroHeadIdx(baseItemIndex) && !oracool::NecroHeadsMayDrop())
		return false;

	item = {};
	item._iSeed = AdvanceRndSeed();
	SetRndSeed(item._iSeed);
	GetItemAttrs(item, baseItemIndex, UniqueItems[uid].UIMinLvl);
	// An item level, which this shelf never stamped (sweep, 2026-09-25): every unique bought here carried ilvl 0,
	// so the Cube's Reroll Uniques and Awaken - which roll at the item's own level - found no unique within reach
	// and nothing to roll. At least the unique's own level, lifted into the difficulty's block the way every
	// other shelf's stock is (VendorItemLevel).
	oracool::StampVendorItemLevel(item, std::max<int>(UniqueItems[uid].UIMinLvl, 1));
	item._iCreateInfo = std::max<int>(UniqueItems[uid].UIMinLvl, 1) | CF_UNIQUE | CF_SMITH;
	const bool wasGenerated = UniqueItemFlags[uid];
	GetUniqueItem(player, item, uid);
	UniqueItemFlags[uid] = wasGenerated;
	item._iIdentified = true;
	item._iStatFlag = player.CanUseItem(item);
	return true;
}

// Defined with the other Oracool stocking helpers below; the Rare shelf shares their base pool.
_item_indexes RndOracoolGearBase(int lvl);

bool CreateRareVendorItem(const Player &player, Item &item, int lvl)
{
	// The base comes from the PREMIUM pool, at the premium tab's own 1..30 window.
	//
	// It was RndSmithItem(player, lvl) - Griswold's BASIC pool at VendorStockLevel() - on the
	// reasoning that a Rare shelf should be "the same shop, rolled hard" rather than a separate item
	// universe. Measured 2026-09-12 after a user report ("many many item types are missing from it no
	// matter how many times i refresh"), that turned out to cost most of the table:
	//
	//   VendorStockLevel() is clamp(l + 2, 6, 16), a ceiling of 16, and the pool gate compares
	//   BandedQlvl, whose steps run 14 -> 17 -> 20. So nothing above authored qlvl 15 was reachable
	//   at ANY character level: 72 of 87 eligible bases, and HEAVY ARMOUR 0 of 5. SmithItemOk also
	//   drops rings, amulets and staves outright, so a rare ring could never appear at all.
	//
	// Two of those 15 (Breast Plate, Crown - authored 16, banded 17) were a regression from the
	// 2026-09-12 banding; the other 13 are vanilla's own shallow basic stock, which is the part that
	// made "the same shop, rolled hard" the wrong model for this shelf.
	//
	// RndPremiumItem with the premium tab's window reaches 99 of 99 bases including jewellery, and it
	// is the pool already vetted for "the good stuff" on the same vendor. Still through
	// GetItemIndexForDroppableItem, so the pick stays the replay-safe walk. `lvl` now sets only the
	// AFFIX depth below, which is what it was already clamped to 30 for.
	item = {};
	// Straight off the ambient stream. The first version reseeded it from itself
	// (`SetRndSeed(AdvanceRndSeed())`) before this call, which is a no-op dressed up as
	// determinism - the item's own generation is seeded separately, in the SetupAllItems call below.
	// 1..30, the same window SpawnOnePremium uses (plvl/4 .. plvl at plvl clamped to 30). A floor of 1
	// rather than lvl/4 because this shelf is not tied to a dungeon depth - it is the whole catalogue,
	// rolled rare, and a level-40 character should still be able to be offered a Buckler.
	_item_indexes idx = RndPremiumItem(player, 1, 30);
	// ORACOOL GEAR too (user, 2026-09-13: "None of oracool items make it there ... make all item types
	// that appear in basic and magic stores also make it to the rare store"). The premium pool above is
	// the droppable pool, which excludes every Oracool item on purpose - it is the save format - so the
	// Basic and Magic tabs reach the fork's gear through a second pass over OracoolGearBasesFor. This
	// shelf had no such pass, so none of it could ever appear here.
	//
	// A third of the rolls, the share the Basic tab stocks. Replay safety does not constrain this pick
	// the way it does the premium roll: the curated shelves are never saved (built once per game in
	// memory), and a bought item keeps its full record like any other.
	//
	// Depth-gated by the character's level, as the Magic tab gates its Oracool block by premiumlevel, so
	// a level-40 character is offered the deep tiers the vendor level alone (capped at 16) would hide.
	if (oracool::IsSinglePlayer() && GenerateRnd(3) == 0) {
		const _item_indexes oracoolBase = RndOracoolGearBase(std::max<int>(player._pLevel, lvl));
		if (oracoolBase != IDI_NONE)
			idx = oracoolBase;
	}
	if (idx == IDI_GOLD)
		return false;

	// Stamped as a ROLLED item, not a town one. CF_TOWN is the route that re-derives an item's index
	// by replaying its seed through the droppable pool, so a town stamp here would let the shelf's
	// wares come back as something else after a reload - the exact bug StockOracoolVendorItems'
	// comment records, and the exact bug that ate the Charms of Salvaging. SetupAllItems writes the
	// createInfo itself, which is why nothing sets it after this call.
	//
	// onlygood TRUE with the forced tier, for the reason RetierOracoolItem spells out: without it
	// GetItemBLevel has a random component that can decide the item rolls no affixes at all, and a
	// forced tier would silently not happen a large share of the time.
	//
	// The ITEM LEVEL is the vendor level lifted by the difficulty block (VendorItemLevel), as the Basic and
	// Premium shelves stamp theirs - not the roll level. It fell back to the clamped roll level, so the Rare
	// shelf's items never passed ilvl 30 and could never be Torment-tier, while the wiki's vendor table
	// promises Hell vendors 38-48 reaching Torment (audit, 2026-09-13). The roll level keeps its 1..30 clamp:
	// that is the premium tab's affix window, and affix depth is a separate question from the tier.
	SetupAllItems(player, item, idx, AdvanceRndSeed(), std::clamp(lvl, 1, 30), 1, /*onlygood=*/true,
	    /*recreate=*/false, /*pregen=*/false, /*allowTieredRoll=*/true,
	    std::optional<OracoolItemTier>(OracoolItemTier::Rare), /*itemLevel=*/oracool::VendorItemLevel(lvl));
	// A base with no affix type - a potion, a scroll - cannot carry a tier, and the roller says so
	// by leaving the tier unset rather than by failing. Reported as a miss so the caller rolls again
	// instead of shelving a plain item on the Rare tab.
	if (item._iOracoolTier != OracoolItemTier::Rare)
		return false;
	// New stock is sold whole: SetupAllItems wears a drop to 25-75% (ItemRndDur), and a shop sold it that way at full
	// price, with a repair fee waiting (round 3 audit, v1.12.228).
	item._iDurability = item._iMaxDur;
	item._iIdentified = true;
	item._iStatFlag = player.CanUseItem(item);
	return true;
}

bool CreateSetVendorItem(const Player &player, Item &item, int lvl,
    tl::function_ref<bool(const oracool::SetItemDefinition &)> alreadyStocked,
    const oracool::SetItemDefinition **chosenOut)
{
	// Every piece the character has EARNED and that can actually be built, minus the ones already on
	// the shelf. All three filters carry weight: the level gate is what makes the shelf grow with
	// the character, the base check keeps the two slots this fork has not built (relic, cloak) from
	// being picked and silently dropped, and the duplicate check is what stops a fifteen-set shelf
	// from being four copies of the same gauntlets.
	std::vector<const oracool::SetItemDefinition *> candidates;
	for (const oracool::ItemSetDefinition &set : oracool::ItemSets) {
		for (int i = 0; i < set.itemCount; i++) {
			const oracool::SetItemDefinition &def = oracool::ItemSetItems[set.firstItem + i];
			if (def.requiredLevel > player._pLevel)
				continue;
			if (oracool::BaseItemForSetPiece(def) < 0)
				continue;
			if (alreadyStocked(def))
				continue;
			candidates.push_back(&def);
		}
	}
	if (candidates.empty())
		return false;

	const oracool::SetItemDefinition &def = *candidates[GenerateRnd(static_cast<int>(candidates.size()))];
	item = {};
	InitializeItem(item, static_cast<_item_indexes>(oracool::BaseItemForSetPiece(def)));
	oracool::MakeSetItem(item, def);
	// The same finish every dropped set piece gets - seed, item level, base tier - so a bought piece
	// and a found one are the same kind of object. No ethereal roll: that is drop-only, and a vendor
	// handing over an ethereal piece the player did not ask for is the case FinalizeSetPiece's own
	// comment declines.
	FinalizeSetPiece(item, std::max(oracool::VendorItemLevel(lvl), def.requiredLevel), /*allowEtherealRoll=*/false); // lifted (round 12)
	// Never cheaper than what it salvages into: a set piece keeps its plain base's value (a helm, 40 gold), and Griswold's
	// free salvage paid three Set Engravings, 675 gold at sale - a gold faucet every game load (round 17 audit). The
	// floor is the three Engravings' full worth, and the shelf price only: BuyCuratedShelfItemAt hands the bought piece
	// its own value back (_ivalue, the base's tiered value).
	constexpr int SetShelfPriceFloor = 3 * 900;
	item._iIvalue = std::max(item._iIvalue, SetShelfPriceFloor);
	item._iIdentified = true;
	item._iStatFlag = player.CanUseItem(item);
	// The caller needs to know WHICH definition was built, so its next call can exclude it. Handing
	// back the row's address costs nothing and spares the caller from re-identifying the piece by
	// its rendered name, which is a translated string and no kind of identity.
	if (chosenOut != nullptr)
		*chosenOut = &def;
	return true;
}

void ClearUniqueItemFlags()
{
	memset(UniqueItemFlags, 0, sizeof(UniqueItemFlags));
}

const char *GetItemDropName(int index)
{
	return ItemDropNames[index];
}

void InitItemGFX()
{
	char arglist[64];

	for (int i = 0; i < ITEMTYPES; i++) {
		// Hellfire's eight (35-42) exist only in hellfire.mpq; the fork's own load in either mode.
		if (!gbIsHellfire && i >= 35 && i < FirstOracoolDropAnim)
			continue;
		// Oracool: a PNG sheet (items\<name>.png, one row) wins when present and has exactly the
		// frame count ItemAnimLs promises - the animation indexes frames by that count.
		OptionalOwnedClxSpriteList png = oracool::LoadPngItemDropSheet(ItemDropNames[i], ItemAnimWidth);
		if (png && ClxSpriteList { *png }.numSprites() == static_cast<uint32_t>(ItemAnimLs[i])) {
			// The fork's own sheets at the size chosen for each (see OracoolDropAnimScale). Scaled once, here,
			// so every drop, label and outline reads the smaller frames; the count cannot change.
			const int percent = OracoolDropAnimScalePercent(static_cast<int8_t>(i));
			if (percent != 100)
				png = oracool::ScaleClxList(ClxSpriteList { *png }, static_cast<unsigned>(percent));
			itemanims[i] = std::move(png);
			continue;
		}
		// The CEL otherwise. The fork's tumbles have none, so without their PNG they borrow larmor's
		// (13 frames, like theirs) - a public build without the art still draws every drop.
		*BufCopy(arglist, "items\\", i >= FirstOracoolDropAnim ? "larmor" : ItemDropNames[i]) = '\0';
		itemanims[i] = LoadCel(arglist, ItemAnimWidth);
	}
}

void InitItems()
{
	ActiveItemCount = 0;
	memset(dItem, 0, sizeof(dItem));

	for (auto &item : Items) {
		item.clear();
		item.position = { 0, 0 };
		item._iAnimFlag = false;
		item._iSelFlag = 0;
		item._iIdentified = false;
		item._iPostDraw = false;
	}

	for (uint8_t i = 0; i < MAXITEMS; i++) {
		ActiveItems[i] = i;
	}

	if (!setlevel) {
		DiscardRandomValues(1);
		if (Quests[Q_ROCK].IsAvailable())
			SpawnRock();
		if (Quests[Q_ANVIL].IsAvailable())
			SpawnQuestItem(IDI_ANVIL, SetPiece.position.megaToWorld() + Displacement { 11, 11 }, 0, 1, false);
		if (sgGameInitInfo.bCowQuest != 0 && currlevel == 20)
			SpawnQuestItem(IDI_BROWNSUIT, { 25, 25 }, 3, 1, false);
		if (sgGameInitInfo.bCowQuest != 0 && currlevel == 19)
			SpawnQuestItem(IDI_GREYSUIT, { 25, 25 }, 3, 1, false);
		// In multiplayer items spawn during level generation to avoid desyncs
		if (gbIsMultiplayer) {
			if (Quests[Q_MUSHROOM].IsAvailable())
				SpawnQuestItem(IDI_FUNGALTM, { 0, 0 }, 5, 1, false);
			if (currlevel == Quests[Q_VEIL]._qlevel + 1 && Quests[Q_VEIL]._qactive != QUEST_NOTAVAIL)
				SpawnQuestItem(IDI_GLDNELIX, { 0, 0 }, 5, 1, false);
		}
		if (currlevel > 0 && currlevel < 16)
			AddInitItems();
		if (currlevel >= 21 && currlevel <= 23)
			SpawnNote();
	}


	initItemGetRecords();
}

void CalcPlrItemVals(Player &player, bool loadgfx)
{
	// Oracool: Megaplan Phase 0.4 - the accumulation is a provider walk now (equipment, rage, and
	// every source Phase 1 adds), not an inline loop. See oracool/stat_sheet.h; the semantics of
	// each sum are pinned by OracoolStatSheet.CalcPlrItemValsAggregationPinned.
	oracool::ItemBonusTotals totals;
	oracool::AccumulateBonuses({ &player }, totals);

	int mind = totals.minDamage;
	int maxd = totals.maxDamage;
	int tac = totals.armor;
	int bdam = totals.bonusDamage;
	int btohit = totals.bonusToHit;
	int bac = totals.bonusArmor;
	ItemSpecialEffect iflgs = totals.flags;
	ItemSpecialEffectHf pDamAcFlags = totals.damAcFlags;
	int sadd = totals.strength;
	int madd = totals.magic;
	int dadd = totals.dexterity;
	int vadd = totals.vitality;
	SpellMask spl = totals.spells;
	int fr = totals.fireResist;
	int lr = totals.lightningResist;
	int mr = totals.magicResist;
	int cr = totals.coldResist;
	int dmod = totals.damageMod;
	int ghit = totals.getHit;
	int lrad = 10 + totals.lightRadius;
	int ihp = totals.hitPoints;
	int imana = totals.mana;
	int spllvladd = totals.spellLevelAdd;
	int enac = totals.enhancedAccuracy;
	int fmin = totals.fireMin;
	int fmax = totals.fireMax;
	int lmin = totals.lightningMin;
	int lmax = totals.lightningMax;

	if (mind == 0 && maxd == 0) {
		mind = 1;
		maxd = 1;

		if (player.InvBody[INVLOC_HAND_LEFT]._itype == ItemType::Shield && player.InvBody[INVLOC_HAND_LEFT]._iStatFlag) {
			maxd = 3;
		}

		if (player.InvBody[INVLOC_HAND_RIGHT]._itype == ItemType::Shield && player.InvBody[INVLOC_HAND_RIGHT]._iStatFlag) {
			maxd = 3;
		}

		if (player._pClass == HeroClass::Monk) {
			mind = std::max(mind, player._pLevel / 2);
			maxd = std::max(maxd, (int)player._pLevel);
		}
	}

	// The Rage stat swings moved into the "rage" bonus provider (stat_sheet.cpp) - the first
	// non-item source, proving the direct-contribution path the set-bonus system will use. The
	// resist half of the cooldown penalty stays below, where it interleaves with the Barbarian's
	// innate resist bonus.

	player._pIMinDam = mind;
	player._pIMaxDam = maxd;
	// The skills' "+N% armour" (2026-09-26): a share of the armour the gear and the flat bonuses give.
	if (totals.armorPercent != 0)
		bac += (tac + bac) * totals.armorPercent / 100;
	player._pIAC = tac;
	player._pIBonusDam = bdam;
	player._pIBonusToHit = btohit;
	player._pIBonusAC = bac;
	player._pIFlags = iflgs;
	// Swift Harvesting (the Necromancer, N9): a wand or scythe in hand swings fast.
	if (oracool::SwiftHarvestingApplies(player))
		player._pIFlags |= ItemSpecialEffect::FastAttack;
	player.pDamAcFlags = pDamAcFlags;
	player._pIBonusDamMod = dmod;
	player._pIGetHit = ghit;

	lrad = clamp(lrad, 2, 15);

	if (player._pLightRad != lrad) {
		ChangeLightRadius(player.lightId, lrad);
		ChangeVisionRadius(player.getId(), lrad);
		player._pLightRad = lrad;
	}

	player._pStrength = std::max(0, sadd + player._pBaseStr);
	player._pMagic = std::max(0, madd + player._pBaseMag);
	player._pDexterity = std::max(0, dadd + player._pBaseDex);
	player._pVitality = std::max(0, vadd + player._pBaseVit);

	// Diablo II's rule (user, 2026-10-01): the stat multiplies the weapon. Each class keeps its own weights - the divisor of
	// its old flat level x stat / k becomes stat x (100 / k)% a point, scaled by StatDamageTenthsPercentPerPoint - so a
	// Barbarian's axe still favours Strength by a third and a Rogue still splits Strength and Dexterity. Level no longer
	// multiplies it: growth comes from the weapon.
	const auto statBasisPoints = [](int stat, int divisor) {
		return static_cast<int>(std::min<int64_t>(int64_t { std::max(stat, 0) } * 1000 * StatDamageTenthsPercentPerPoint / divisor, INT_MAX / 2));
	};
	if (player._pClass == HeroClass::Rogue) {
		player._pStatDamageBasisPoints = statBasisPoints(player._pStrength + player._pDexterity, 200);
	} else if (player._pClass == HeroClass::Monk) {
		// A broken (0-durability, left equipped rather than destroyed) weapon no longer
		// counts as "holding" anything for these class-specific checks, matching how
		// CalcSelfItems already excludes it from stat bonuses via _iStatFlag.
		const bool leftIsFunctionalNonStaff = !player.InvBody[INVLOC_HAND_LEFT].isEmpty() && player.InvBody[INVLOC_HAND_LEFT]._iStatFlag && player.InvBody[INVLOC_HAND_LEFT]._itype != ItemType::Staff;
		const bool rightIsFunctionalNonStaff = !player.InvBody[INVLOC_HAND_RIGHT].isEmpty() && player.InvBody[INVLOC_HAND_RIGHT]._iStatFlag && player.InvBody[INVLOC_HAND_RIGHT]._itype != ItemType::Staff;
		player._pStatDamageBasisPoints = statBasisPoints(player._pStrength + player._pDexterity, 150);
		if (leftIsFunctionalNonStaff || rightIsFunctionalNonStaff)
			player._pStatDamageBasisPoints /= 2; // Monks get half the normal damage bonus if they're holding a non-staff weapon
	} else if (player._pClass == HeroClass::Bard) {
		const bool leftSword = player.InvBody[INVLOC_HAND_LEFT]._itype == ItemType::Sword && player.InvBody[INVLOC_HAND_LEFT]._iStatFlag;
		const bool rightSword = player.InvBody[INVLOC_HAND_RIGHT]._itype == ItemType::Sword && player.InvBody[INVLOC_HAND_RIGHT]._iStatFlag;
		const bool leftBow = player.InvBody[INVLOC_HAND_LEFT]._itype == ItemType::Bow && player.InvBody[INVLOC_HAND_LEFT]._iStatFlag;
		const bool rightBow = player.InvBody[INVLOC_HAND_RIGHT]._itype == ItemType::Bow && player.InvBody[INVLOC_HAND_RIGHT]._iStatFlag;
		if (leftSword || rightSword)
			player._pStatDamageBasisPoints = statBasisPoints(player._pStrength + player._pDexterity, 150);
		else if (leftBow || rightBow) {
			player._pStatDamageBasisPoints = statBasisPoints(player._pStrength + player._pDexterity, 250);
		} else {
			player._pStatDamageBasisPoints = statBasisPoints(player._pStrength, 100);
		}
	} else if (player._pClass == HeroClass::Barbarian) {
		const bool leftFunctional = player.InvBody[INVLOC_HAND_LEFT]._iStatFlag;
		const bool rightFunctional = player.InvBody[INVLOC_HAND_RIGHT]._iStatFlag;
		const bool leftAxe = player.InvBody[INVLOC_HAND_LEFT]._itype == ItemType::Axe && leftFunctional;
		const bool rightAxe = player.InvBody[INVLOC_HAND_RIGHT]._itype == ItemType::Axe && rightFunctional;
		const bool leftMace = player.InvBody[INVLOC_HAND_LEFT]._itype == ItemType::Mace && leftFunctional;
		const bool rightMace = player.InvBody[INVLOC_HAND_RIGHT]._itype == ItemType::Mace && rightFunctional;
		const bool leftBow = player.InvBody[INVLOC_HAND_LEFT]._itype == ItemType::Bow && leftFunctional;
		const bool rightBow = player.InvBody[INVLOC_HAND_RIGHT]._itype == ItemType::Bow && rightFunctional;
		const bool leftShield = player.InvBody[INVLOC_HAND_LEFT]._itype == ItemType::Shield && leftFunctional;
		const bool rightShield = player.InvBody[INVLOC_HAND_RIGHT]._itype == ItemType::Shield && rightFunctional;
		const bool leftStaffOrBow = leftFunctional && IsAnyOf(player.InvBody[INVLOC_HAND_LEFT]._itype, ItemType::Staff, ItemType::Bow);
		const bool rightStaffOrBow = rightFunctional && IsAnyOf(player.InvBody[INVLOC_HAND_RIGHT]._itype, ItemType::Staff, ItemType::Bow);

		if (leftAxe || rightAxe) {
			player._pStatDamageBasisPoints = statBasisPoints(player._pStrength, 75);
		} else if (leftMace || rightMace) {
			player._pStatDamageBasisPoints = statBasisPoints(player._pStrength, 75);
		} else if (leftBow || rightBow) {
			player._pStatDamageBasisPoints = statBasisPoints(player._pStrength, 300);
		} else {
			player._pStatDamageBasisPoints = statBasisPoints(player._pStrength, 100);
		}

		if (leftShield || rightShield) {
			if (leftShield)
				player._pIAC -= player.InvBody[INVLOC_HAND_LEFT]._iAC / 2;
			else if (rightShield)
				player._pIAC -= player.InvBody[INVLOC_HAND_RIGHT]._iAC / 2;
		} else if (!leftStaffOrBow && !rightStaffOrBow) {
			player._pStatDamageBasisPoints += statBasisPoints(player._pVitality, 100);
		}
		player._pIAC += player._pLevel / 4;
	} else {
		player._pStatDamageBasisPoints = statBasisPoints(player._pStrength, 100);
	}

	player._pISpells = spl;

	EnsureValidReadiedSpell(player);

	// Held at the field's range (2026-10-01, the user's 255-points question): the total is summed in an int and stored in an
	// int8_t, and past +127 it wrapped to -128 - every active skill would have dropped to level 0.
	player._pISplLvlAdd = static_cast<int8_t>(std::clamp(spllvladd, -128, 127));
	player._pIEnAc = enac;

	if (player._pClass == HeroClass::Barbarian) {
		mr += player._pLevel;
		fr += player._pLevel;
		lr += player._pLevel;
		cr += player._pLevel;
	}

	if (HasAnyOf(player._pSpellFlags, SpellFlag::RageCooldown)) {
		mr -= player._pLevel;
		fr -= player._pLevel;
		lr -= player._pLevel;
		cr -= player._pLevel;
	}

	if (HasAnyOf(iflgs, ItemSpecialEffect::ZeroResistance)) {
		// reset resistances to zero if the respective special effect is active
		mr = 0;
		fr = 0;
		lr = 0;
		cr = 0;
	}

	// Oracool: the soft cap and the difficulty's penetration, replacing vanilla's flat
	// clamp(x, 0, 75). This is the ONE place the three raw totals become the values the rest of the
	// game reads, which is why the curve lives behind this call rather than at the points that use
	// the result - missiles.cpp and objects.cpp read the fields and must keep seeing a plain
	// percentage. See oracool/player_resistance.h for the curve and its order.
	const _difficulty difficulty = sgGameInitInfo.nDifficulty;
	player._pMagResist = static_cast<int8_t>(oracool::ApplyResistanceCurve(mr, difficulty));
	player._pFireResist = static_cast<int8_t>(oracool::ApplyResistanceCurve(fr, difficulty));
	player._pLghtResist = static_cast<int8_t>(oracool::ApplyResistanceCurve(lr, difficulty));
	// Cold, a resistance of its own since 2026-09-26 (user: "make it as real as it is in Diablo 2"): the same
	// curve, the same difficulty penalty and the same cap as the other three.
	player._pColdResist = static_cast<int8_t>(oracool::ApplyResistanceCurve(cr, difficulty));

	vadd = (vadd * PlayersData[static_cast<size_t>(player._pClass)].itmLife) >> 6;
	ihp += (vadd << 6); // BUGFIX: blood boil can cause negative shifts here (see line 757)

	madd = (madd * PlayersData[static_cast<size_t>(player._pClass)].itmMana) >> 6;
	imana += (madd << 6);

	player._pMaxHP = ihp + player._pMaxHPBase;
	player._pHitPoints = std::min(ihp + player._pHPBase, player._pMaxHP);

	if (&player == MyPlayer && (player._pHitPoints >> 6) <= 0) {
		SetPlayerHitPoints(player, 0);
	}

	player._pMaxMana = imana + player._pMaxManaBase;
	player._pMana = std::min(imana + player._pManaBase, player._pMaxMana);

	player._pIFMinDam = fmin;
	player._pIFMaxDam = fmax;
	player._pILMinDam = lmin;
	player._pILMaxDam = lmax;

	// Phase 1: Magic/Gold Find, derived like everything else on the sheet.
	player._pMagicFind = totals.magicFind;
	player._pGoldFind = totals.goldFind;
	// Movement Speed: the items' affixes and the burning aura, one percentage (2026-09-07).
	player._pIMoveSpeed = totals.moveSpeed;
	// Faster Cast Rate: the items' affixes, one percentage (2026-09-11). StartSpell turns it into frames.
	player._pIFastCast = totals.fastCast;

	player._pInfraFlag = oracool::IsSinglePlayer() && *sgOptions.Oracool.permanentInfravision;

	player._pBlockFlag = false;
	if (player._pClass == HeroClass::Monk) {
		if (player.InvBody[INVLOC_HAND_LEFT]._itype == ItemType::Staff && player.InvBody[INVLOC_HAND_LEFT]._iStatFlag) {
			player._pBlockFlag = true;
			player._pIFlags |= ItemSpecialEffect::FastBlock;
		}
		if (player.InvBody[INVLOC_HAND_RIGHT]._itype == ItemType::Staff && player.InvBody[INVLOC_HAND_RIGHT]._iStatFlag) {
			player._pBlockFlag = true;
			player._pIFlags |= ItemSpecialEffect::FastBlock;
		}
		if (player.InvBody[INVLOC_HAND_LEFT].isEmpty() && player.InvBody[INVLOC_HAND_RIGHT].isEmpty())
			player._pBlockFlag = true;
		if (player.InvBody[INVLOC_HAND_LEFT]._iClass == ICLASS_WEAPON && player.GetItemLocation(player.InvBody[INVLOC_HAND_LEFT]) != ILOC_TWOHAND && player.InvBody[INVLOC_HAND_RIGHT].isEmpty())
			player._pBlockFlag = true;
		if (player.InvBody[INVLOC_HAND_RIGHT]._iClass == ICLASS_WEAPON && player.GetItemLocation(player.InvBody[INVLOC_HAND_RIGHT]) != ILOC_TWOHAND && player.InvBody[INVLOC_HAND_LEFT].isEmpty())
			player._pBlockFlag = true;
	}

	ItemType weaponItemType = ItemType::None;
	bool holdsShield = false;
	if (!player.InvBody[INVLOC_HAND_LEFT].isEmpty()
	    && player.InvBody[INVLOC_HAND_LEFT]._iClass == ICLASS_WEAPON
	    && player.InvBody[INVLOC_HAND_LEFT]._iStatFlag) {
		weaponItemType = player.InvBody[INVLOC_HAND_LEFT]._itype;
	}

	if (!player.InvBody[INVLOC_HAND_RIGHT].isEmpty()
	    && player.InvBody[INVLOC_HAND_RIGHT]._iClass == ICLASS_WEAPON
	    && player.InvBody[INVLOC_HAND_RIGHT]._iStatFlag) {
		weaponItemType = player.InvBody[INVLOC_HAND_RIGHT]._itype;
	}

	if (player.InvBody[INVLOC_HAND_LEFT]._itype == ItemType::Shield && player.InvBody[INVLOC_HAND_LEFT]._iStatFlag) {
		player._pBlockFlag = true;
		holdsShield = true;
	}
	if (player.InvBody[INVLOC_HAND_RIGHT]._itype == ItemType::Shield && player.InvBody[INVLOC_HAND_RIGHT]._iStatFlag) {
		player._pBlockFlag = true;
		holdsShield = true;
	}

	// Brace (RfA-12): a spear or pike held crosswise blocks, shield or no shield.
	if (oracool::Rfa12GrantsBlock(player))
		player._pBlockFlag = true;

	PlayerWeaponGraphic animWeaponId = holdsShield ? PlayerWeaponGraphic::UnarmedShield : PlayerWeaponGraphic::Unarmed;
	switch (weaponItemType) {
	case ItemType::Sword:
		animWeaponId = holdsShield ? PlayerWeaponGraphic::SwordShield : PlayerWeaponGraphic::Sword;
		break;
	case ItemType::Axe:
		// Oracool: an axe (or a pike) beside a shield - only Heavenly Strength allows the pair - wears the
		// mace-and-shield body, the one drawn sprite of a hafted weapon with a shield (user, 2026-09-11:
		// "hero wears axe + shield we will use mace+shield combo assets"). Its attack timing is that sprite's.
		animWeaponId = holdsShield ? PlayerWeaponGraphic::MaceShield : PlayerWeaponGraphic::Axe;
		break;
	case ItemType::Bow:
		animWeaponId = PlayerWeaponGraphic::Bow;
		break;
	case ItemType::Mace:
		animWeaponId = holdsShield ? PlayerWeaponGraphic::MaceShield : PlayerWeaponGraphic::Mace;
		break;
	case ItemType::Staff:
		// Oracool: and a staff beside a shield the same (user, 2026-09-11: "hero wears staff+shield - same combo").
		animWeaponId = holdsShield ? PlayerWeaponGraphic::MaceShield : PlayerWeaponGraphic::Staff;
		break;
	default:
		break;
	}

	PlayerArmorGraphic animArmorId = PlayerArmorGraphic::Light;
	if (player.InvBody[INVLOC_CHEST]._itype == ItemType::HeavyArmor && player.InvBody[INVLOC_CHEST]._iStatFlag) {
		if (player._pClass == HeroClass::Monk && player.InvBody[INVLOC_CHEST]._iMagical == ITEM_QUALITY_UNIQUE)
			player._pIAC += player._pLevel / 2;
		animArmorId = PlayerArmorGraphic::Heavy;
	} else if (player.InvBody[INVLOC_CHEST]._itype == ItemType::MediumArmor && player.InvBody[INVLOC_CHEST]._iStatFlag) {
		if (player._pClass == HeroClass::Monk) {
			if (player.InvBody[INVLOC_CHEST]._iMagical == ITEM_QUALITY_UNIQUE)
				player._pIAC += player._pLevel * 2;
			else
				player._pIAC += player._pLevel / 2;
		}
		animArmorId = PlayerArmorGraphic::Medium;
	} else if (player._pClass == HeroClass::Monk) {
		player._pIAC += player._pLevel * 2;
	}

	const uint8_t gfxNum = static_cast<uint8_t>(animWeaponId) | static_cast<uint8_t>(animArmorId);
	// Oracool: the sheets depend on more than _pgfxnum since v1.12.023 - a Buckler and a Tower Shield are the
	// same weapon class and not the same look (oracool/sprite_mix.h) - so a changed look reloads them too.
	const uint8_t gearLook = oracool::GearLookCode(player);
	if ((player._pgfxnum != gfxNum || player._pGearLook != gearLook) && loadgfx) {
		player._pgfxnum = gfxNum;
		player._pGearLook = gearLook;
		ResetPlayerGFX(player);
		SetPlrAnims(player);
		player.previewCelSprite = std::nullopt;
		player_graphic graphic = player.getGraphic();
		int8_t numberOfFrames;
		int8_t ticksPerFrame;
		player.getAnimationFramesAndTicksPerFrame(graphic, numberOfFrames, ticksPerFrame);
		LoadPlrGFX(player, graphic);
		PrewarmPlayerLook(player); // Oracool: the other animations of a mixed look, built ahead of their first frame
		OptionalClxSpriteList sprites;
		if (!HeadlessMode)
			sprites = player.AnimationData[static_cast<size_t>(graphic)].spritesForDirection(player._pdir);
		player.AnimInfo.changeAnimationData(sprites, numberOfFrames, ticksPerFrame);
	} else {
		player._pgfxnum = gfxNum;
		player._pGearLook = gearLook;
	}

	if (&player == MyPlayer) {
		const int previousMaxGold = MaxGold;
		if (oracool::IsSinglePlayer())
			MaxGold = GoldStackSaveLimit;
		else if (!player.InvBody[INVLOC_AMULET].isEmpty() && player.InvBody[INVLOC_AMULET].IDidx == IDI_AURIC)
			MaxGold = GOLD_MAX_LIMIT * 2;
		else
			MaxGold = GOLD_MAX_LIMIT;

		if (MaxGold < previousMaxGold)
			StripTopGold(player);
	}

	RedrawComponent(PanelDrawComponent::Mana);
	RedrawComponent(PanelDrawComponent::Health);
}

void CalcPlrInv(Player &player, bool loadgfx)
{
	// Determine the players current stats, this updates the statFlag on all equipped items that became unusable after
	//  a change in equipment.
	CalcSelfItems(player);

	// Determine the current item bonuses gained from usable equipped items
	if (&player != MyPlayer && !player.isOnActiveLevel()) {
		// Ensure we don't load graphics for players that aren't on our level
		loadgfx = false;
	}
	CalcPlrItemVals(player, loadgfx);

	// Oracool bug fix (2026-08-15): the innate skill mask depends on EQUIPMENT now, not just on
	// character level - Shield Bash and Blessed Shield require a shield - so it has to be rebuilt
	// wherever equipment changes, which is here. It was previously computed only at character
	// creation, on level-up and on level load, so picking up a shield left the mask stale: the
	// Abilities window drew the two rows as unlocked (that reads HasShieldEquipped live) but
	// refused to ready them, because readying tests IsSpellKnown, which reads the mask. The skill
	// appeared, and then did nothing when clicked, until the next floor.
	//
	// Recomputing the whole mask rather than toggling two bits keeps this site from having to know
	// which skills are gated on what - same reason NextPlrLevel does it this way.
	player._pAblSpells = oracool::InnateSpellsBitmask(player);

	if (&player == MyPlayer) {
		// Now that stat gains from equipped items have been calculated, mark unusable scrolls etc
		for (Item &item : InventoryAndBeltPlayerItemsRange { player }) {
			item.updateRequiredStatsCacheForPlayer(player);
		}
		player.CalcScrolls();
		CalcPlrStaff(player);
		if (IsStashOpen) {
			// If stash is open, ensure the items are displayed correctly
			Stash.RefreshItemStatFlags();
		}
		// Last, because the stinger fires on an equipment TRANSACTION and this is the point at which
		// one has fully settled: stat flags recomputed, requirements re-tested, unusable items
		// demoted. A set whose last piece the wearer cannot actually use is not complete, and asking
		// any earlier would have rung for it. The rising-edge test lives in the callee - see
		// CheckSetCompletionTransition for what must NOT ring it.
		oracool::CheckSetCompletionTransition(player);
		// The set-bonus milestone is asked here too, once the character is settled in a running game (not while a hero
		// loads, when a claim would pay a Signet in the menu): it was checked only at a level-up or an imbuement, so a hero
		// at the level cap never earned it (round 10 audit, v1.12.235). Claiming is idempotent.
		if (oracool::IsSetCompletionBaselineArmed() && oracool::AnySetBonusActive(player))
			oracool::ClaimMilestone(player, oracool::Milestone::WearSetBonus);
	}
}

void InitializeItem(Item &item, _item_indexes itemData)
{
	auto &pAllItem = AllItemsList[static_cast<size_t>(itemData)];

	// zero-initialize struct
	item = {};

	item._itype = pAllItem.itype;
	item._iCurs = pAllItem.iCurs;
	CopyUtf8(item._iName, pAllItem.iName, sizeof(item._iName));
	CopyUtf8(item._iIName, pAllItem.iName, sizeof(item._iIName));
	item._iLoc = pAllItem.iLoc;
	item._iClass = pAllItem.iClass;
	item._iMinDam = pAllItem.iMinDam;
	item._iMaxDam = pAllItem.iMaxDam;
	item._iAC = pAllItem.iMinAC;
	item._iMiscId = pAllItem.iMiscId;
	item._iSpell = pAllItem.iSpell;

	if (pAllItem.iMiscId == IMISC_STAFF) {
		item._iCharges = gbIsHellfire ? 18 : 40;
	}

	item._iMaxCharges = item._iCharges;
	item._iDurability = pAllItem.iDurability;
	item._iMaxDur = pAllItem.iDurability;
	item._iMinStr = pAllItem.iMinStr;
	item._iMinMag = pAllItem.iMinMag;
	item._iMinDex = pAllItem.iMinDex;
	item._ivalue = pAllItem.iValue;
	item._iIvalue = pAllItem.iValue;
	item._iMagical = ITEM_QUALITY_NORMAL;
	item.IDidx = static_cast<_item_indexes>(itemData);
	if (gbIsHellfire)
		item.dwBuff |= CF_HELLFIRE;
}

void GenerateNewSeed(Item &item)
{
	item._iSeed = AdvanceRndSeed();
}

int GetGoldCursor(int value)
{
	if (value >= GOLD_MEDIUM_LIMIT)
		return ICURS_GOLD_LARGE;

	if (value <= GOLD_SMALL_LIMIT)
		return ICURS_GOLD_SMALL;

	return ICURS_GOLD_MEDIUM;
}

void SetPlrHandGoldCurs(Item &gold)
{
	gold._iCurs = GetGoldCursor(gold._ivalue);
}

void CreatePlrItems(Player &player)
{
	for (auto &item : player.InvBody) {
		item.clear();
	}

	// converting this to a for loop creates a `rep stosd` instruction,
	// so this probably actually was a memset
	memset(&player.InvGrid, 0, sizeof(player.InvGrid));

	for (auto &item : player.InvList) {
		item.clear();
	}

	player._pNumInv = 0;

	for (auto &item : player.SpdList) {
		item.clear();
	}

	// NAKED HEROES (user, 2026-08-19: "heroes start completely naked with zero equipment and zero
	// potions and zero money [...] ON by def"). Everything above has already cleared the body, the
	// grid, the inventory and the belt, so being naked is simply stopping here - no class gear, no
	// two healing potions, and no 100 gold below.
	//
	// Both mouse buttons are already on the bare fist: CreatePlayer readies nothing on either since
	// the vanilla class skills were retired, and BasicAttackIcon reports Fist rather than Regular
	// whenever the hands are empty, which they now are.
	//
	// Read HERE, at creation, and nowhere else. The option decides what a character was born with,
	// not what they are allowed to carry - so turning it off later re-equips nobody and turning it on
	// strips nobody.
	if (*sgOptions.Oracool.nakedHeroes) {
		CalcPlrItemVals(player, false);
		return;
	}

	switch (player._pClass) {
	case HeroClass::Warrior:
		InitializeItem(player.InvBody[INVLOC_HAND_LEFT], IDI_WARRIOR);
		GenerateNewSeed(player.InvBody[INVLOC_HAND_LEFT]);

		InitializeItem(player.InvBody[INVLOC_HAND_RIGHT], IDI_WARRSHLD);
		GenerateNewSeed(player.InvBody[INVLOC_HAND_RIGHT]);

		{
			Item club;
			InitializeItem(club, IDI_WARRCLUB);
			GenerateNewSeed(club);
			AutoPlaceItemInInventorySlot(player, 0, club, true);
		}

		InitializeItem(player.SpdList[0], IDI_HEAL);
		GenerateNewSeed(player.SpdList[0]);

		InitializeItem(player.SpdList[1], IDI_HEAL);
		GenerateNewSeed(player.SpdList[1]);
		break;
	case HeroClass::Rogue:
		InitializeItem(player.InvBody[INVLOC_HAND_LEFT], IDI_ROGUE);
		GenerateNewSeed(player.InvBody[INVLOC_HAND_LEFT]);

		InitializeItem(player.SpdList[0], IDI_HEAL);
		GenerateNewSeed(player.SpdList[0]);

		InitializeItem(player.SpdList[1], IDI_HEAL);
		GenerateNewSeed(player.SpdList[1]);
		break;
	case HeroClass::Sorcerer:
		InitializeItem(player.InvBody[INVLOC_HAND_LEFT], gbIsHellfire ? IDI_SORCERER : IDI_SORCERER_DIABLO);
		GenerateNewSeed(player.InvBody[INVLOC_HAND_LEFT]);

		InitializeItem(player.SpdList[0], gbIsHellfire ? IDI_HEAL : IDI_MANA);
		GenerateNewSeed(player.SpdList[0]);

		InitializeItem(player.SpdList[1], gbIsHellfire ? IDI_HEAL : IDI_MANA);
		GenerateNewSeed(player.SpdList[1]);
		break;

	case HeroClass::Monk:
		InitializeItem(player.InvBody[INVLOC_HAND_LEFT], IDI_SHORTSTAFF);
		GenerateNewSeed(player.InvBody[INVLOC_HAND_LEFT]);
		InitializeItem(player.SpdList[0], IDI_HEAL);
		GenerateNewSeed(player.SpdList[0]);

		InitializeItem(player.SpdList[1], IDI_HEAL);
		GenerateNewSeed(player.SpdList[1]);
		break;
	case HeroClass::Bard:
		InitializeItem(player.InvBody[INVLOC_HAND_LEFT], IDI_BARDSWORD);
		GenerateNewSeed(player.InvBody[INVLOC_HAND_LEFT]);

		InitializeItem(player.InvBody[INVLOC_HAND_RIGHT], IDI_BARDDAGGER);
		GenerateNewSeed(player.InvBody[INVLOC_HAND_RIGHT]);
		InitializeItem(player.SpdList[0], IDI_HEAL);
		GenerateNewSeed(player.SpdList[0]);

		InitializeItem(player.SpdList[1], IDI_HEAL);
		GenerateNewSeed(player.SpdList[1]);
		break;
	case HeroClass::Necromancer:
		// The Sorcerer's staff and two mana potions until the wand family exists (phase N9).
		InitializeItem(player.InvBody[INVLOC_HAND_LEFT], gbIsHellfire ? IDI_SORCERER : IDI_SORCERER_DIABLO);
		GenerateNewSeed(player.InvBody[INVLOC_HAND_LEFT]);

		InitializeItem(player.SpdList[0], IDI_MANA);
		GenerateNewSeed(player.SpdList[0]);

		InitializeItem(player.SpdList[1], IDI_MANA);
		GenerateNewSeed(player.SpdList[1]);
		break;
	case HeroClass::Barbarian:
		InitializeItem(player.InvBody[INVLOC_HAND_LEFT], IDI_BARBARIAN);
		GenerateNewSeed(player.InvBody[INVLOC_HAND_LEFT]);
		// The one starter base that is NOT IDROP_NEVER (a Spiked Club, drop level 4): it asked level 4
		// of a level-1 hero (audit, 2026-09-20). Unbound, as the Work of Cathan leaves an item.
		player.InvBody[INVLOC_HAND_LEFT]._iOracoolLevelFree = true;

		InitializeItem(player.InvBody[INVLOC_HAND_RIGHT], IDI_WARRSHLD);
		GenerateNewSeed(player.InvBody[INVLOC_HAND_RIGHT]);
		InitializeItem(player.SpdList[0], IDI_HEAL);
		GenerateNewSeed(player.SpdList[0]);

		InitializeItem(player.SpdList[1], IDI_HEAL);
		GenerateNewSeed(player.SpdList[1]);
		break;
	}

	Item &goldItem = player.InvList[player._pNumInv];
	MakeGoldStack(goldItem, 100);

	player._pNumInv++;
	// The bottom-left cell. Vanilla's 30 was the bottom-left of its 10x4 backpack - the fourth row of the 10x7 one.
	player.InvGrid[InventoryGridCells - InventorySizeInSlots.width] = player._pNumInv;

	player._pGold = goldItem._ivalue;

	// The starting gear at item level 1 (tooltip sweep, 2026-09-25). InitializeItem leaves it at 0, and every Cube
	// recipe that rolls affixes rolls at the item's own level: Enrich, the Crafts and the tier ladder took a starting
	// sword, reported ready and made nothing - the roll at level 0 has no affix to give.
	for (Item &item : player.InvBody) {
		if (!item.isEmpty() && item._iOracoolItemLevel == 0)
			item._iOracoolItemLevel = 1;
	}
	for (int i = 0; i < player._pNumInv; i++) {
		Item &item = player.InvList[i];
		if (!item.isEmpty() && item._iLoc != ILOC_UNEQUIPABLE && item._iOracoolItemLevel == 0)
			item._iOracoolItemLevel = 1;
	}

	CalcPlrItemVals(player, false);
}

bool ItemSpaceOk(Point position)
{
	if (!InDungeonBounds(position)) {
		return false;
	}

	if (IsTileSolid(position)) {
		return false;
	}

	if (dItem[position.x][position.y] != 0) {
		return false;
	}

	if (dMonster[position.x][position.y] != 0) {
		return false;
	}

	if (dPlayer[position.x][position.y] != 0) {
		return false;
	}

	if (IsItemBlockingObjectAtPosition(position)) {
		return false;
	}

	if (oracool::IsBesideRiftWayHome(position)) // no loot where a pickup walk would end on the way home (round 19)
		return false;

	return true;
}

int AllocateItem()
{
	assert(ActiveItemCount < MAXITEMS);

	int inum = ActiveItems[ActiveItemCount];
	ActiveItemCount++;

	Items[inum] = {};

	return inum;
}

uint8_t PlaceItemInWorld(Item &&item, WorldTilePosition position)
{
	assert(ActiveItemCount < MAXITEMS);

	uint8_t ii = ActiveItems[ActiveItemCount];
	ActiveItemCount++;

	dItem[position.x][position.y] = ii + 1;
	auto &item_ = Items[ii];
	item_ = std::move(item);
	item_.position = position;
	RespawnItem(item_, true);

	if (CornerStone.isAvailable() && position == CornerStone.position) {
		CornerStone.item = item_;
		InitQTextMsg(TEXT_CORNSTN);
		Quests[Q_CORNSTN]._qactive = QUEST_DONE;
	}

	return ii;
}

Point GetSuperItemLoc(Point position)
{
	std::optional<Point> itemPosition = FindClosestValidPosition(ItemSpaceOk, position, 1, 50);

	return itemPosition.value_or(Point { 0, 0 }); // TODO handle no space for dropping items
}

void GetItemAttrs(Item &item, _item_indexes itemData, int lvl)
{
	// CRITICAL (audit, 2026-08-26): every per-roll bonus is cleared before the base is written.
	//
	// This is the only reset an item gets on its way through SetupAllItems, and it used to reset
	// the BASE fields alone - type, damage, armour, durability - while leaving every _iPL* bonus
	// exactly where the last roll put it. SaveItemPower applies almost all of them with +=.
	//
	// A fresh drop was safe, because a drop rolls into a brand new Item. But Reforge, Reroll,
	// Enrich, Awaken and Ennoble all reroll the SAME object, so each one added its new affixes on
	// top of every affix the item had ever had. The tooltip lists only the current roll, so the
	// character silently carried the sum of all of them - a rare rerolled twenty times is wearing
	// twenty rares. Unbounded, invisible, and it survived into the save.
	//
	// Cleared HERE rather than in each recipe: there are five of them today and the sixth would
	// have been written without this line. The one function every path already funnels through is
	// the only place that cannot be forgotten.
	item._iPLToHit = 0;
	item._iPLDam = 0;
	item._iPLAC = 0;
	item._iPLStr = 0;
	item._iPLMag = 0;
	item._iPLDex = 0;
	item._iPLVit = 0;
	item._iPLFR = 0;
	item._iPLLR = 0;
	item._iPLMR = 0;
	item._iPLCR = 0;
	item._iPLMana = 0;
	item._iPLHP = 0;
	item._iOracoolOilAC = 0; // a fresh base carries no oil (round 54 audit)
	// Nor shards: the ledger is put back by every rebuild that keeps it, and Make Ethereal read a stale one on a bare base
	// and halved Tempering that was not there yet (round 55 audit).
	item._iOracoolImbueCount = 0;
	item._iOracoolImbuements.fill(0);
	item._iPLDamMod = 0;
	item._iPLGetHit = 0;
	item._iPLLight = 0;
	item._iPLEnAc = 0;
	item._iPLMagicFind = 0;
	item._iPLGoldFind = 0;
	item._iPLMoveSpeed = 0;
	item._iPLFastCast = 0;
	// The rest of what a roll writes, and each one is a real bug of its own if left behind: fire
	// and lightning damage ranges, charges, spell level, value multipliers, the special-effect
	// flag words, and the two Oracool flags. _iOracoolEthereal and _iOracoolBroken in particular
	// outlived the stats they modified - a rerolled item stayed "ethereal" and "broken" while its
	// durability had just been reset to the base, which is an item that cannot be repaired and has
	// no stats.
	item._iFMinDam = 0;
	item._iFMaxDam = 0;
	item._iLMinDam = 0;
	item._iLMaxDam = 0;
	item._iSplLvlAdd = 0;
	item._iCharges = 0;
	item._iMaxCharges = 0;
	item._iVAdd1 = 0;
	item._iVAdd2 = 0;
	item._iVMult1 = 0;
	item._iVMult2 = 0;
	item._iDamAcFlags = ItemSpecialEffectHf::None;
	item._iOracoolEthereal = false;
	item._iOracoolBroken = false;

	auto &baseItemData = AllItemsList[static_cast<size_t>(itemData)];
	item._itype = baseItemData.itype;
	item._iCurs = baseItemData.iCurs;
	CopyUtf8(item._iName, baseItemData.iName, sizeof(item._iName));
	CopyUtf8(item._iIName, baseItemData.iName, sizeof(item._iIName));
	item._iLoc = baseItemData.iLoc;
	item._iClass = baseItemData.iClass;
	item._iMinDam = baseItemData.iMinDam;
	item._iMaxDam = baseItemData.iMaxDam;
	item._iAC = baseItemData.iMinAC + GenerateRnd(baseItemData.iMaxAC - baseItemData.iMinAC + 1);
	item._iFlags = baseItemData.iFlags;
	item._iMiscId = baseItemData.iMiscId;
	item._iSpell = baseItemData.iSpell;
	item._iMagical = ITEM_QUALITY_NORMAL;
	item._ivalue = baseItemData.iValue;
	item._iIvalue = baseItemData.iValue;
	item._iDurability = baseItemData.iDurability;
	item._iMaxDur = baseItemData.iDurability;
	item._iMinStr = baseItemData.iMinStr;
	item._iMinMag = baseItemData.iMinMag;
	item._iMinDex = baseItemData.iMinDex;
	item.IDidx = itemData;
	if (gbIsHellfire)
		item.dwBuff |= CF_HELLFIRE;
	// The affix list, which replaced vanilla's prefix/suffix pair here (2026-09-25): every per-roll bonus is
	// cleared above, and the list names those bonuses, so it goes with them - a vendor retry loop or a reroll
	// that rolls the same object again must not stack a second roll's affixes onto the first's. The tier is
	// left to the caller (RebuildOracoolItemWithAffixes puts it back; ClearOracoolAffixRecord clears it).
	item._iOracoolAffixCount = 0;
	item._iOracoolAffixes = {};

	if (item._iMiscId == IMISC_BOOK)
		GetBookSpell(item, lvl);

	if (gbIsHellfire && item._iMiscId == IMISC_OILOF)
		GetOilType(item, lvl);

	if (item._itype != ItemType::Gold)
		return;

	int rndv;
	int itemlevel = ItemsGetCurrlevel();
	switch (sgGameInitInfo.nDifficulty) {
	case DIFF_NORMAL:
		rndv = 5 * itemlevel + GenerateRnd(10 * itemlevel);
		break;
	// The area level already carries the difficulty (16 rungs a block since v1.8.0): vanilla's +16/+32 on top counted it
	// twice - a Nightmare floor-1 pile rolled like Normal floor 17 (round 10 audit, v1.12.235).
	case DIFF_NIGHTMARE:
	case DIFF_HELL:
		rndv = 5 * itemlevel + GenerateRnd(10 * itemlevel);
		break;
	case DIFF_TORMENT:
		// Oracool: the same formula, scaled further by the adjustable Torment multiplier.
		rndv = static_cast<int>((5 * itemlevel + GenerateRnd(10 * itemlevel)) * GetTormentDifficultyMultiplier());
		break;
	}
	if (leveltype == DTYPE_HELL)
		rndv += rndv / 8;

	item._ivalue = std::min(rndv, GOLD_MAX_LIMIT);
	SetPlrHandGoldCurs(item);
}

void SetupItem(Item &item)
{
	item.setNewAnimation(MyPlayer != nullptr && MyPlayer->pLvlLoad == 0);
	item._iIdentified = false;
}

namespace {

// Oracool: shared by every item-creation call site that finalizes a dungeon-dropped item, so the
// event log can flag Rare/Buffed Unique/Primal/vanilla Unique/Quest drops uniformly regardless of
// which path produced them (monster drop, boss-guaranteed unique, or scripted quest item).
void LogNoteworthyItemDrop(const Item &item)
{
	std::string descriptor;
	switch (item._iOracoolTier) {
	case OracoolItemTier::Primal:
		descriptor = "Primal";
		break;
	case OracoolItemTier::BuffedUnique:
		descriptor = "Buffed Unique";
		break;
	case OracoolItemTier::Rare:
		descriptor = "Rare";
		break;
	case OracoolItemTier::Set:
		// Audit finding, 2026-08-26: the named-set drop path calls this function deliberately, and
		// this switch had no Set case - so the one drop family added specifically to be noticed
		// was the one that never appeared in the event log or the telemetry.
		descriptor = "Set";
		break;
	case OracoolItemTier::None:
		if (item._iMagical == ITEM_QUALITY_UNIQUE)
			descriptor = "Unique";
		else if (item._iClass == ICLASS_QUEST)
			descriptor = "Quest";
		break;
	}
	if (descriptor.empty())
		return;

	std::string location;
	// currlevel is the set level's id there: a Nest rift read "Nest -7" (round 36 audit).
	if (setlevel)
		location = oracool::IsRiftLevel(setlvlnum) ? "Rift" : "Quest level";
	else switch (leveltype) {
	case DTYPE_TOWN:
		location = "Town";
		break;
	case DTYPE_NEST:
		location = fmt::format("Nest {:d}", currlevel - 16);
		break;
	case DTYPE_CRYPT:
		location = fmt::format("Crypt {:d}", currlevel - 20);
		break;
	default:
		location = fmt::format("Level {:d}", currlevel);
		break;
	}

	oracool::LogEvent(fmt::format("{:s} item dropped: {:s} ({:s})", descriptor, std::string(item.getName()), location));
}

} // namespace

Item *SpawnUnique(_unique_items uid, Point position, std::optional<int> level /*= std::nullopt*/, bool sendmsg /*= true*/, bool exactPosition /*= false*/)
{
	MakeRoomForGuaranteedReward(); // a quest unique is a promise, as a quest reward is (round 15 audit)
	if (ActiveItemCount >= MAXITEMS)
		return nullptr;

	// The base-item lookup happens BEFORE anything is allocated, so failing it costs nothing to
	// unwind. Bailing out after AllocateItem would leak the slot and leave a cleared item sitting on
	// the tile with dItem already pointing at it.
	//
	// The walk itself used to have no end test - the same shape as ItemMiscIdIdx, and with a live
	// way to miss: this fork added 143 expansion uniques and a large share of them are still waiting
	// on base items to exist. A unique whose UIItemId no base carries walked off the table, and
	// GetItemAttrs then built an item out of whatever it landed on.
	//
	// Failing to spawn is the honest outcome. Substituting some other base would put an item in the
	// world under a name that does not describe it - worse than a drop that visibly did not appear
	// and gets reported.
	std::underlying_type_t<_item_indexes> baseIdx = 0;
	while (baseIdx <= IDI_LAST && AllItemsList[baseIdx].iItemId != UniqueItems[uid].UIItemId)
		baseIdx++;
	if (baseIdx > IDI_LAST) {
		oracool::LogEvent(fmt::format("Unique {:s} has no base item - not spawned",
		                      std::string(_(UniqueItems[uid].UIName))),
		    UiFlags::ColorRed);
		return nullptr;
	}

	int ii = AllocateItem();
	auto &item = Items[ii];
	if (exactPosition && CanPut(position)) {
		item.position = position;
		dItem[position.x][position.y] = ii + 1;
	} else {
		GetSuperItemSpace(position, ii);
	}
	int curlv = ItemsGetCurrlevel();

	const std::underlying_type_t<_item_indexes> idx = baseIdx;

	if (sgGameInitInfo.nDifficulty == DIFF_NORMAL) {
		GetItemAttrs(item, static_cast<_item_indexes>(idx), curlv);
		GetUniqueItem(*MyPlayer, item, uid);
		SetupItem(item);
		// Its item level, as every other drop has one (audit, 2026-09-29: a Normal boss's quest unique came out level 0 -
		// Awaken refused it and Reroll Uniques could only roll it into itself). No lower than the unique's own level.
		item._iOracoolItemLevel = static_cast<uint8_t>(std::clamp<int>(std::max<int>(curlv, UniqueItems[uid].UIMinLvl), 1, 255));
	} else {
		// A quest's FLOOR run through the ladder, as CurrentAreaLevel does for quest set-levels: the raw floor stamped a
		// Torment Anvil reward ilvl 10, Normal-grade beside Griswold's ilvl 54 shelf (round 10 audit, v1.12.235).
		if (level)
			curlv = oracool::AreaLevel(*level, sgGameInitInfo.nDifficulty);
		const ItemData &uniqueItemData = AllItemsList[idx];
		_item_indexes idx = GetItemIndexForDroppableItem(false, [&uniqueItemData](const ItemData &item) {
			return item.itype == uniqueItemData.itype;
		});
		SetupAllItems(*MyPlayer, item, idx, AdvanceRndSeed(), curlv * 2, 15, true, false, false,
		    /*allowTieredRoll=*/true, std::nullopt, /*itemLevel=*/curlv);
		FinalizeFreshDrop(item, curlv); // a fresh drop's tail, which logs it too (round 11 audit)
		if (sendmsg)
			NetSendCmdPItem(false, CMD_SPAWNITEM, item.position, item);
		return &item;
	}

	LogNoteworthyItemDrop(item);

	if (sendmsg)
		NetSendCmdPItem(false, CMD_SPAWNITEM, item.position, item);

	return &item;
}

/**
 * @brief mlvl: what a monster is worth as a source of loot.
 *
 * The area level, plus a little for what the monster IS - a champion is a harder fight than its
 * neighbours and pays like one, and a unique more so again. Deliberately NOT Monster::level(), which
 * drives to-hit, block, experience and some missile damage from thirteen call sites; moving those
 * onto the area ladder would be a combat rebalance wearing a loot change's clothes (user,
 * 2026-08-19: "split now, revisit with telemetry").
 */
int ItemLevelOfMonster(const Monster &monster)
{
	int level = oracool::CurrentAreaLevel();
	// A rift guardian pays as a unique: Diablo, who has no unique row, paid an ordinary monster's level (round 11 audit).
	// A champion carries a uniqueType too (PrepareUniqueMonst), so it is asked for first: every champion paid +3, and
	// the health bar showed it (round 23 audit, v1.12.248). A Dread boss is a unique's +3.
	if (oracool::IsEndgameBoss(monster) || oracool::IsRiftGuardian(monster))
		level += 3;
	else if (monster.lesserAffix != LesserUniqueAffix::None)
		level += 2;
	else if (monster.isUnique())
		level += 3;
	return std::min(level, oracool::MaxAreaLevel);
}

void SpawnItem(Monster &monster, Point position, bool sendmsg, bool spawn /*= false*/)
{
	_item_indexes idx;
	bool onlygood = true;

	// A rift's guardian is the Skeleton King or the Butcher without his quest (2026-09-26 dev note): no Undead Crown, no
	// Cleaver - the unique monster's random item instead, the better base and the better quality roll.
	bool dropsSpecialTreasure = (monster.data().treasure & T_UNIQ) != 0 && !oracool::IsRiftGuardian(monster);
	// Not on a floor that is thrown away - a rift or a Sealed Map arena: the Brain left there was gone for good and the quest
	// could never finish (round 15 audit, v1.12.240).
	const bool disposableFloor = setlevel && (oracool::IsRiftLevel(setlvlnum) || IsArenaLevel(setlvlnum));
	bool dropBrain = Quests[Q_MUSHROOM]._qactive == QUEST_ACTIVE && Quests[Q_MUSHROOM]._qvar1 == QS_MUSHGIVEN && !disposableFloor;

	if (dropsSpecialTreasure && !UseMultiplayerQuests()) {
		Item *uniqueItem = SpawnUnique(static_cast<_unique_items>(monster.data().treasure & T_MASK), position, std::nullopt, false);
		if (uniqueItem != nullptr && sendmsg)
			NetSendCmdPItem(false, CMD_DROPITEM, uniqueItem->position, *uniqueItem);
		return;
	} else if (monster.isUnique() || dropsSpecialTreasure || oracool::IsRiftGuardian(monster)) {
		// Unqiue monster is killed => use better item base (for example no gold)
		// A rift guardian too: Diablo's six pile rolls were an ordinary monster's - mostly nothing and gold - beside
		// the other three guardians' unique-grade piles (round 11 audit, v1.12.236).
		idx = RndUItem(&monster);
	} else if (dropBrain && !gbIsMultiplayer) {
		// Normal monster is killed => need to drop brain to progress the quest
		Quests[Q_MUSHROOM]._qvar1 = QS_BRAINSPAWNED;
		NetSendCmdQuest(true, Quests[Q_MUSHROOM]);
		// brain replaces normal drop
		idx = IDI_BRAIN;
	} else {
		if (dropBrain && gbIsMultiplayer && sendmsg) {
			Quests[Q_MUSHROOM]._qvar1 = QS_BRAINSPAWNED;
			NetSendCmdQuest(true, Quests[Q_MUSHROOM]);
			// Drop the brain as extra item to ensure that all clients see the brain drop
			// When executing SpawnItem is not reliable, cause another client can already have the quest state updated before SpawnItem is executed
			Point posBrain = GetSuperItemLoc(position);
			SpawnQuestItem(IDI_BRAIN, posBrain, false, false, true);
		}
		// Normal monster
		if ((monster.data().treasure & T_NODROP) != 0)
			return;
		onlygood = false;
		const auto dropLevel = static_cast<int8_t>(std::min(ItemLevelOfMonster(monster), 127));
		idx = RndItemForMonsterLevel(dropLevel);
		// SMART LOOT aims the BASE, before anything is rolled onto it, so the quality roll below runs
		// exactly once as it always has. Candidates come from the equipment pool directly: drawing
		// them through RndItemForMonsterLevel meant nine re-rolls in ten came back as nothing or gold
		// and were thrown away, so "best of three" was nearly always best of one. They are drawn for
		// the SLOT the blind roll chose, so aiming never changes whether a ring or a helm drops.
		idx = oracool::SmartLootAimBase(idx, *MyPlayer, [dropLevel](item_equip_type slot) { return RndEquipmentForMonsterLevel(dropLevel, slot); });
	}

	if (idx == IDI_NONE)
		return;

	// The Brain is spawned whatever the floor holds: its quest state has already moved on, and on a full floor it never
	// existed (round 15 audit).
	if (idx == IDI_BRAIN)
		MakeRoomForGuaranteedReward();
	if (ActiveItemCount >= MAXITEMS)
		return;

	int ii = AllocateItem();
	auto &item = Items[ii];
	GetSuperItemSpace(position, ii);
	int uper = (monster.isUnique() || oracool::IsRiftGuardian(monster)) ? 15 : 1;

	// THE MONSTER'S LOOT LEVEL, not its authored level (audit, 2026-09-13: "rare items still seem very rare
	// and i am now in hell/hell", then "make sure drops reflect our vision ... progressive difficulty with
	// more rewarding and better loot").
	//
	// The wiki's contract (Mechanics) is that a monster's loot level is the AREA level, +3 for a unique and
	// +2 for a champion, and that an item's ilvl is the loot level of whatever dropped it. The base-item pool
	// above already asked ItemLevelOfMonster. This call did not: it passed monster.data().level, the level
	// the monster TYPE was authored at in 1996 - so the quality roll (GetItemBLevel's chance to roll at all,
	// the affix levels) and the ilvl stamp (which picks the rare/buffed/primal band) ignored both the
	// difficulty and the area ladder. An Advocate paid the same 30 on Hell/Hell as on Normal, Hell/Hell
	// dropped rares at Normal's rate (measured: 0.57% a kill on both), and the Hive and Crypt paid their
	// authored 22-30 instead of the Caves' and Hell's rungs. ReforgeOracoolItem was written on the promise
	// this now keeps - "mLevel IS the ilvl".
	const int mlvl = ItemLevelOfMonster(monster);
	SetupAllItems(*MyPlayer, item, idx, AdvanceRndSeed(), mlvl, uper, onlygood, false, false,
	    /*allowTieredRoll=*/true, std::nullopt, /*itemLevel=*/mlvl);
	FinalizeFreshDrop(item, mlvl);

	if (sendmsg)
		NetSendCmdPItem(false, CMD_DROPITEM, item.position, item);
	if (spawn)
		NetSendCmdPItem(false, CMD_SPAWNITEM, item.position, item);
}

/**
 * @brief Oracool: how many AFFIXES @p item's quality tier allows, in total.
 *
 * D3-style, and deliberately flat (user, 2026-09-13: "i would like to move to D3 style. We call all
 * possible item bonuses affixes and an item can have any combo of them within its limit of
 * affixes"). There is no separate prefix and suffix allowance to spend: every bonus an item can
 * carry is an affix, any mix of them is legal, and the only rule is the count.
 *
 * Diablo II is the other model - MagicPrefix.txt and MagicSuffix.txt, one of each on a magic item,
 * three of each on a rare - and this fork was built on it. Since v1.12.001 neither the budget nor the
 * storage is prefix-and-suffix: a tier says "four affixes", and they sit in one list in whatever mix
 * the pool dealt. The two vanilla tables remain only as where the rows are written.
 *
 * GetRareItemAffixes guarantees two plus two 30% bonuses (4), GetBuffedUniqueItemAffixes four plus two
 * bonuses (6), GetPrimalItemAffixes six with no bonus. Magic takes two - vanilla's count, from any tables.
 *
 * Zero for plain quality, for a vanilla unique, and for a set piece: none of the three carries a
 * rolled affix at all, and a set piece's six powers are a fixed list rather than affixes.
 */
DVL_API_FOR_TEST int OracoolAffixBudget(const Item &item)
{
	switch (item._iOracoolTier) {
	case OracoolItemTier::Rare:
		return 4; // two guaranteed, plus two 30% bonuses
	case OracoolItemTier::BuffedUnique:
		return 6; // four guaranteed, plus two 30% bonuses
	case OracoolItemTier::Primal:
		return Item::MaxOracoolAffixes; // six guaranteed, already at the cap
	case OracoolItemTier::Set:
		return 0;
	case OracoolItemTier::None:
		break;
	}
	if (item._iMagical == ITEM_QUALITY_MAGIC)
		return 2; // one or two affixes, from any tables (GetItemPower)
	return 0;     // plain quality, and vanilla uniques
}

/**
 * @brief Oracool: how many affixes @p item is already carrying - the length of its one affix list.
 *
 * This used to add two stores together: a magic item kept its table affixes in a vanilla prefix/suffix pair
 * of fields and only its pool affixes in the list, and a guard that read the list alone saw zero on every
 * magic item (a "Garnet Cap of the Tiger" reached the player with FOUR affixes on a two-affix tier). Since
 * 2026-09-25 every affix of every item is on the list (user: "all afixes are now one pool"), so its count is
 * the whole answer.
 */
DVL_API_FOR_TEST int OracoolAffixesUsed(const Item &item)
{
	return item._iOracoolAffixCount;
}

namespace {

/** @brief The accumulated _iPL* stat fields one power writes into, as bits - see SaveItemPower. */
enum LegacyAffixField : uint16_t {
	FieldToHit = 1 << 0,
	FieldDam = 1 << 1,
	FieldAC = 1 << 2,
	FieldFR = 1 << 3,
	FieldLR = 1 << 4,
	FieldMR = 1 << 5,
	FieldStr = 1 << 6,
	FieldMag = 1 << 7,
	FieldDex = 1 << 8,
	FieldVit = 1 << 9,
	FieldGetHit = 1 << 10,
	FieldHP = 1 << 11,
	FieldMana = 1 << 12,
	FieldDamMod = 1 << 13,
	FieldFind = 1 << 14,
	FieldCR = 1 << 15,
};

uint16_t LegacyAffixFields(item_effect_type type)
{
	switch (type) {
	case IPL_TOHIT:
	case IPL_TOHIT_CURSE:
		return FieldToHit;
	case IPL_DAMP:
	case IPL_DAMP_CURSE:
	case IPL_CRYSTALLINE:
	case IPL_DECAY:
		return FieldDam;
	case IPL_TOHIT_DAMP:
	case IPL_TOHIT_DAMP_CURSE:
	case IPL_DOPPELGANGER:
		return FieldDam | FieldToHit;
	case IPL_ACP:
	case IPL_ACP_CURSE:
		return FieldAC;
	case IPL_FIRERES:
	case IPL_FIRERES_CURSE:
		return FieldFR;
	case IPL_LIGHTRES:
	case IPL_LIGHTRES_CURSE:
		return FieldLR;
	case IPL_MAGICRES:
	case IPL_MAGICRES_CURSE:
		return FieldMR;
	case IPL_COLDRES:
	case IPL_COLDRES_CURSE:
		return FieldCR;
	case IPL_ALLRES:
		return FieldFR | FieldLR | FieldMR | FieldCR;
	case IPL_STR:
	case IPL_STR_CURSE:
		return FieldStr;
	case IPL_MAG:
	case IPL_MAG_CURSE:
		return FieldMag;
	case IPL_DEX:
	case IPL_DEX_CURSE:
		return FieldDex;
	case IPL_VIT:
	case IPL_VIT_CURSE:
		return FieldVit;
	case IPL_ATTRIBS:
	case IPL_ATTRIBS_CURSE:
		return FieldStr | FieldMag | FieldDex | FieldVit;
	case IPL_GETHIT:
	case IPL_GETHIT_CURSE:
		return FieldGetHit;
	case IPL_LIFE:
	case IPL_LIFE_CURSE:
		return FieldHP;
	case IPL_MANA:
	case IPL_MANA_CURSE:
		return FieldMana;
	case IPL_MANATOLIFE:
	case IPL_LIFETOMANA:
		return FieldHP | FieldMana;
	case IPL_DAMMOD:
		return FieldDamMod;
	case IPL_GOLDFIND:
	case IPL_MAGICFIND:
		return FieldFind;
	default:
		return 0;
	}
}

/**
 * @brief The rolled value a simple stat affix of @p type left in @p item's own field, as the positive
 * magnitude the list stores (SaveItemPower applies the sign). Nothing for a type whose roll is not
 * readable back out of one field.
 */
std::optional<int> LegacyAffixValueFromField(const Item &item, item_effect_type type)
{
	switch (type) {
	case IPL_TOHIT:
		return item._iPLToHit;
	case IPL_TOHIT_CURSE:
		return -item._iPLToHit;
	case IPL_DAMP:
	case IPL_TOHIT_DAMP:
	case IPL_DOPPELGANGER:
		return item._iPLDam; // the to-hit half is not in the list; the printer works it out from the totals
	case IPL_DAMP_CURSE:
	case IPL_TOHIT_DAMP_CURSE:
		return -item._iPLDam;
	case IPL_ACP:
		return item._iPLAC;
	case IPL_ACP_CURSE:
		return -item._iPLAC;
	case IPL_FIRERES:
		return item._iPLFR;
	case IPL_FIRERES_CURSE:
		return -item._iPLFR;
	case IPL_LIGHTRES:
		return item._iPLLR;
	case IPL_LIGHTRES_CURSE:
		return -item._iPLLR;
	case IPL_MAGICRES:
		return item._iPLMR;
	case IPL_MAGICRES_CURSE:
		return -item._iPLMR;
	case IPL_COLDRES:
		return item._iPLCR;
	case IPL_COLDRES_CURSE:
		return -item._iPLCR;
	case IPL_ALLRES:
		return item._iPLFR;
	case IPL_STR:
	case IPL_ATTRIBS:
		return item._iPLStr;
	case IPL_STR_CURSE:
	case IPL_ATTRIBS_CURSE:
		return -item._iPLStr;
	case IPL_MAG:
		return item._iPLMag;
	case IPL_MAG_CURSE:
		return -item._iPLMag;
	case IPL_DEX:
		return item._iPLDex;
	case IPL_DEX_CURSE:
		return -item._iPLDex;
	case IPL_VIT:
		return item._iPLVit;
	case IPL_VIT_CURSE:
		return -item._iPLVit;
	case IPL_GETHIT:
		return -item._iPLGetHit;
	case IPL_GETHIT_CURSE:
		return item._iPLGetHit;
	case IPL_LIFE:
		return item._iPLHP / 64;
	case IPL_LIFE_CURSE:
		return -item._iPLHP / 64;
	case IPL_MANA:
		return item._iPLMana / 64;
	case IPL_MANA_CURSE:
		return -item._iPLMana / 64;
	case IPL_DAMMOD:
		return item._iPLDamMod;
	case IPL_GOLDFIND:
		return item._iPLGoldFind;
	case IPL_MAGICFIND:
		return item._iPLMagicFind;
	default:
		return std::nullopt;
	}
}

/** @brief The one field LegacyAffixValueFromField reads for @p type - narrower than what the power writes. */
uint16_t LegacyAffixReadField(item_effect_type type)
{
	switch (type) {
	case IPL_TOHIT_DAMP:
	case IPL_TOHIT_DAMP_CURSE:
	case IPL_DOPPELGANGER:
		return FieldDam;
	case IPL_ALLRES:
		return FieldFR;
	case IPL_ATTRIBS:
	case IPL_ATTRIBS_CURSE:
		return FieldStr;
	default:
		return LegacyAffixFields(type);
	}
}

/** @brief The price multiplier of the row of @p type whose roll range holds @p value - the band it rolled in. */
int LegacyAffixMultVal(item_effect_type type, int value)
{
	const auto holds = [type, value](const PLStruct &row) {
		return row.power.type == type && value >= std::min(row.power.param1, row.power.param2)
		    && value <= std::max(row.power.param1, row.power.param2);
	};
	for (int j = 0; ItemPrefixes[j].power.type != IPL_INVALID; j++) {
		if (holds(ItemPrefixes[j]))
			return ItemPrefixes[j].multVal;
	}
	for (int j = 0; ItemSuffixes[j].power.type != IPL_INVALID; j++) {
		if (holds(ItemSuffixes[j]))
			return ItemSuffixes[j].multVal;
	}
	const PLStruct *row = FindAffixRowForType(type);
	return row != nullptr ? row->multVal : 0;
}

} // namespace

void MigrateLegacyAffixPair(Item &item, item_effect_type first, item_effect_type second)
{
	// ONE LIST (user, 2026-09-25: "remove any trace of prefix/sufix segregation. all afixes are now one pool").
	// An item saved before that kept its table affixes' TYPES in a vanilla pair of fields and nothing else about
	// them; the save still carries those two bytes, and this puts what they name onto the list so the item keeps
	// its affix lines, its level requirement and a place at Gillian's bench. The stats themselves are already in
	// the item's fields - nothing is applied here.
	//
	// A vanilla unique never rolled into the pair, and a type already on the list is not listed twice.
	if (item._iMagical == ITEM_QUALITY_UNIQUE)
		return;
	std::array<item_effect_type, 2> legacy {};
	int legacyCount = 0;
	for (const item_effect_type type : { first, second }) {
		if (type == IPL_INVALID || FindAffixRowForType(type) == nullptr)
			continue;
		bool listed = false;
		for (int i = 0; i < item._iOracoolAffixCount; i++)
			listed = listed || item._iOracoolAffixes[i].type == type;
		if (legacyCount == 1 && legacy[0] == type)
			listed = true;
		if (!listed)
			legacy[legacyCount++] = type;
	}
	if (legacyCount == 0)
		return;

	// Which stat fields the OTHER affixes write - a field one of them shares holds a sum, not this affix's roll.
	const auto othersFields = [&](int self) {
		uint16_t fields = 0;
		for (int k = 0; k < legacyCount; k++) {
			if (k != self)
				fields |= LegacyAffixFields(legacy[k]);
		}
		for (int i = 0; i < item._iOracoolAffixCount; i++)
			fields |= LegacyAffixFields(item._iOracoolAffixes[i].type);
		return fields;
	};

	// The old pair first, as the tooltip printed it, then what the list already held (pool rows, crafted lines).
	std::array<OracoolAffix, Item::MaxOracoolAffixes> merged {};
	int mergedCount = 0;
	for (int k = 0; k < legacyCount && mergedCount < Item::MaxOracoolAffixes; k++) {
		const item_effect_type type = legacy[k];
		// A simple stat's value is its field. When another affix writes the same field the field holds their
		// SUM - still what the old pair's line printed for it (it printed the item's totals), so the line reads
		// as it did; only an affix with no single field to read gets 0, and PrintOracoolAffixPower prints those
		// types from the item's totals anyway.
		int value = 0;
		if (const std::optional<int> fromField = LegacyAffixValueFromField(item, type))
			value = std::max(0, *fromField);
		const bool shared = (LegacyAffixReadField(type) & othersFields(k)) != 0;
		// The price band from the value only when the value is this affix's alone.
		merged[mergedCount++] = OracoolAffix { type, value, LegacyAffixMultVal(type, shared ? 0 : value) };
	}
	for (int i = 0; i < item._iOracoolAffixCount && mergedCount < Item::MaxOracoolAffixes; i++)
		merged[mergedCount++] = item._iOracoolAffixes[i];
	item._iOracoolAffixes = merged;
	item._iOracoolAffixCount = static_cast<uint8_t>(mergedCount);
}



int UniqueItemFastCast(int uid)
{
	if (uid < 0 || static_cast<size_t>(uid) >= UniqueItemCount)
		return 0;
	const UniqueItem &unique = UniqueItems[uid];
	int total = 0;
	for (int i = 0; i < unique.UINumPL; i++) {
		if (unique.powers[i].type == IPL_FASTCAST)
			total += unique.powers[i].param1;
	}
	return total;
}

void RederiveFastCast(Item &item)
{
	// Item::_iPLFastCast is not a stored field - the item format did not grow for it, because a bump would
	// refuse every existing hero. It comes back from what wrote it: the drop tail's records, and a unique's
	// own row, whose values are fixed (GenUniqueItems.ps1's authored table), so reading param1 is exact.
	// Set rungs and runewords need nothing here: both are recomputed into the totals on every recalc.
	item._iPLFastCast = 0;
	for (int i = 0; i < item._iOracoolAffixCount; i++) {
		if (item._iOracoolAffixes[i].type == IPL_FASTCAST)
			item._iPLFastCast += item._iOracoolAffixes[i].param1;
	}
	if (item._iMagical == ITEM_QUALITY_UNIQUE)
		item._iPLFastCast += UniqueItemFastCast(item._iUid);
}

void FinalizeFreshDrop(Item &item, int level)
{
	// Phase 1: the drop tail - Magic/Gold Find first (an upgraded item then correctly skips the
	// socket roll), then sockets, then ethereal. All AFTER setup and outside the seed replay -
	// see TryAddSocketsToDroppedItem's comment for why none of this may move into SetupAllItems.
	ApplyMagicAndGoldFindToDrop(item, level);
	TryAddSocketsToDroppedItem(item);
	TryMakeDroppedItemEthereal(item);
	LogNoteworthyItemDrop(item);
}

void CreateRndItem(Point position, bool onlygood, bool sendmsg, bool delta)
{
	_item_indexes idx = onlygood ? RndUItem(nullptr) : RndAllItems();

	// SMART LOOT, the chest/barrel/theme-room half. Aimed on the index here rather than inside
	// SetupBaseItem, which CreateTypeItem also calls with a type it chose on purpose (a weapon rack
	// drops a weapon) and must keep. Gold and consumables pass straight through SmartLootAimBase:
	// this pool is gold three times in four, and the first version replaced them with equipment.
	idx = oracool::SmartLootAimBase(idx, *MyPlayer, [onlygood](item_equip_type slot) {
		return onlygood ? RndUItem(nullptr) : RndEquipmentForCurrentLevel(slot);
	});

	SetupBaseItem(position, idx, onlygood, sendmsg, delta);
}

void CreateRndUseful(Point position, bool sendmsg)
{
	if (ActiveItemCount >= MAXITEMS)
		return;

	int ii = AllocateItem();
	auto &item = Items[ii];
	GetSuperItemSpace(position, ii);
	int curlv = ItemsGetCurrlevel();

	SetupAllUseful(item, AdvanceRndSeed(), curlv);
	if (sendmsg)
		NetSendCmdPItem(false, CMD_DROPITEM, item.position, item);
}

void CreateTypeItem(Point position, bool onlygood, ItemType itemType, int imisc, bool sendmsg, bool delta, bool spawn)
{
	_item_indexes idx;

	int curlv = ItemsGetCurrlevel();
	if (itemType != ItemType::Gold)
		idx = RndTypeItems(itemType, imisc, curlv);
	else
		idx = IDI_GOLD;

	SetupBaseItem(position, idx, onlygood, sendmsg, delta, spawn);
}

void RecreateItem(const Player &player, Item &item, _item_indexes idx, uint16_t icreateinfo, uint32_t iseed, int ivalue, bool isHellfire)
{
	// Everything below this line is a REPLAY: the item already exists somewhere and is being rebuilt
	// from its seed. The pool filters must therefore see the authored qlvl ladder, not the banded one -
	// see ReplayingStoredItemSeed. One guard at the entry point covers the dungeon path, all five
	// vendor paths through RecreateTownItem, and anything either of them calls.
	ReplayScope replaying;

	bool tmpIsHellfire = gbIsHellfire;
	gbIsHellfire = isHellfire;

	if (idx == IDI_GOLD) {
		InitializeItem(item, IDI_GOLD);
		item._iSeed = iseed;
		item._iCreateInfo = icreateinfo;
		item._ivalue = ivalue;
		SetPlrHandGoldCurs(item);
		gbIsHellfire = tmpIsHellfire;
		return;
	}

	if (icreateinfo == 0) {
		InitializeItem(item, idx);
		item._iSeed = iseed;
		gbIsHellfire = tmpIsHellfire;
		return;
	}

	if ((icreateinfo & CF_UNIQUE) == 0) {
		if ((icreateinfo & CF_TOWN) != 0) {
			RecreateTownItem(player, item, idx, icreateinfo, iseed);
			gbIsHellfire = tmpIsHellfire;
			return;
		}

		if ((icreateinfo & CF_USEFUL) == CF_USEFUL) {
			SetupAllUseful(item, iseed, icreateinfo & CF_LEVEL);
			gbIsHellfire = tmpIsHellfire;
			return;
		}
	}

	int level = icreateinfo & CF_LEVEL;

	int uper = 0;
	if ((icreateinfo & CF_UPER1) != 0)
		uper = 1;
	if ((icreateinfo & CF_UPER15) != 0)
		uper = 15;

	bool onlygood = (icreateinfo & CF_ONLYGOOD) != 0;
	bool recreate = (icreateinfo & CF_UNIQUE) != 0;
	bool pregen = (icreateinfo & CF_PREGEN) != 0;

	// RecreateItem always reconstructs a previously-generated item from its stored seed; see
	// SetupAllItems's allowTieredRoll doc comment for why that must never roll for Rare/Buffed Unique.
	SetupAllItems(player, item, idx, iseed, level, uper, onlygood, recreate, pregen, false);
	gbIsHellfire = tmpIsHellfire;
}

void RecreateEar(Item &item, uint16_t ic, uint32_t iseed, uint8_t bCursval, string_view heroName)
{
	InitializeItem(item, IDI_EAR);

	std::string itemName = fmt::format(fmt::runtime("Ear of {:s}"), heroName);

	CopyUtf8(item._iName, itemName, sizeof(item._iName));
	CopyUtf8(item._iIName, heroName, sizeof(item._iIName));

	item._iCurs = ((bCursval >> 6) & 3) + ICURS_EAR_SORCERER;
	item._ivalue = bCursval & 0x3F;
	item._iCreateInfo = ic;
	item._iSeed = iseed;
}

void CornerstoneSave()
{
	if (!CornerStone.activated)
		return;
	if (!CornerStone.item.isEmpty()) {
		ItemPack id;
		PackItem(id, CornerStone.item, (CornerStone.item.dwBuff & CF_HELLFIRE) != 0);
		const auto *buffer = reinterpret_cast<uint8_t *>(&id);
		for (size_t i = 0; i < sizeof(ItemPack); i++) {
			fmt::format_to(&sgOptions.Hellfire.szItem[i * 2], "{:02X}", buffer[i]);
		}
		sgOptions.Hellfire.szItem[sizeof(sgOptions.Hellfire.szItem) - 1] = '\0';
	} else {
		sgOptions.Hellfire.szItem[0] = '\0';
	}
}

void CornerstoneLoad(Point position)
{
	if (CornerStone.activated || position.x == 0 || position.y == 0) {
		return;
	}

	CornerStone.item.clear();
	CornerStone.activated = true;
	if (dItem[position.x][position.y] != 0) {
		int ii = dItem[position.x][position.y] - 1;
		for (int i = 0; i < ActiveItemCount; i++) {
			if (ActiveItems[i] == ii) {
				DeleteItem(i);
				break;
			}
		}
		dItem[position.x][position.y] = 0;
	}

	// No item restored from diablo.ini (round 21 audit, v1.12.246). Its save lost its only caller with the manual Save
	// Game, so an item placed here vanished at the game's end while an old build's stored item came back in EVERY new
	// game - a duplicate. The compact pack it used would also strip sets, sockets, shards and crafts. In V1 (always a
	// new game) the Cornerstone is a pedestal like any floor tile.
}

void SpawnQuestItem(_item_indexes itemid, Point position, int randarea, int selflag, bool sendmsg)
{
	if (randarea > 0) {
		int tries = 0;
		while (true) {
			tries++;
			if (tries > 1000 && randarea > 1)
				randarea--;

			position.x = GenerateRnd(MAXDUNX);
			position.y = GenerateRnd(MAXDUNY);

			bool failed = false;
			for (int i = 0; i < randarea && !failed; i++) {
				for (int j = 0; j < randarea && !failed; j++) {
					failed = !ItemSpaceOk(position + Displacement { i, j });
				}
			}
			if (!failed)
				break;
		}
	} else if (InDungeonBounds(position) && (dItem[position.x][position.y] != 0 || oracool::IsBesideRiftWayHome(position))) {
		// Nor beside a rift's exit (round 33 audit): a guardian dying at the arrival left his keystone where a pickup walk
		// ended on the exit, and the rest of the pile was lost with the rift.
		// A tile that already holds an item: the nearest free one instead (audit, 2026-09-27). The fork's boss drops pass
		// the monster's own tile - a rift guardian's keystone, a sealed map, an encounter's reward - and a boss can die
		// standing on loot; the new item took the tile over and the old one stayed in the list, unreachable. Vanilla's
		// brain drop asks GetSuperItemLoc first for the same reason.
		const Point free = GetSuperItemLoc(position);
		if (free != Point { 0, 0 })
			position = free;
	}

	if (ActiveItemCount >= MAXITEMS)
		return;

	int ii = AllocateItem();
	auto &item = Items[ii];

	item.position = position;

	dItem[position.x][position.y] = ii + 1;

	int curlv = ItemsGetCurrlevel();
	GetItemAttrs(item, itemid, curlv);

	SetupItem(item);
	LogNoteworthyItemDrop(item);
	item._iSeed = AdvanceRndSeed();
	SetRndSeed(item._iSeed);
	item._iPostDraw = true;
	if (selflag != 0) {
		item._iSelFlag = selflag;
		item.AnimInfo.currentFrame = item.AnimInfo.numberOfFrames - 1;
		item._iAnimFlag = false;
	}

	if (sendmsg)
		NetSendCmdPItem(true, CMD_SPAWNITEM, item.position, item);
	else {
		item._iCreateInfo |= CF_PREGEN;
		DeltaAddItem(ii);
	}
}

void MakeRoomForGuaranteedReward()
{
	// Frees one ground-item slot by discarding the least valuable thing lying about. Only ever called when the floor is at
	// MAXITEMS and a guaranteed reward has nowhere to land. Ordinary quality only: a unique, a set piece or another quest
	// item on the ground is somebody else's promise and must not be dropped to keep this one.
	if (ActiveItemCount < MAXITEMS)
		return;
	// The cheapest ordinary item, never gold or a quest item: the old walk took the last ordinary entry, which DeleteItem's
	// swaps had scrambled, and the create-info test let the Anvil, the Magic Rock and the Blood Stones through (round 16).
	int victim = -1;
	for (int i = ActiveItemCount - 1; i >= 0; i--) {
		const Item &candidate = Items[ActiveItems[i]];
		if (candidate._iMagical != ITEM_QUALITY_NORMAL || candidate._itype == ItemType::Gold)
			continue;
		// Nor a white item with sockets (a runeword, its gems) or affixes of its own: ordinary quality, not junk (round 23).
		// Any socketed white, empty sockets too - the user's rule (salvage.cpp, 2026-08-28): they are precious, not junk
		// (round 25 audit, restoring round 23's guard).
		if (candidate._iSocketCount > 0 || candidate._iOracoolAffixCount > 0)
			continue;
		if (candidate._iCreateInfo == 0 && candidate._iIdentified)
			continue; // quest-placed items carry no create info; leave them alone
		// Nothing that never drops at random: the Staff of Lazarus, the elixirs and the Guardian Keystone are misc-class,
		// worth 0, and were the first victims of "cheapest first" (round 17 audit, a regression of round 16).
		if (candidate.IDidx >= 0 && AllItemsList[candidate.IDidx].iRnd == IDROP_NEVER)
			continue;
		if (victim < 0 || candidate._ivalue < Items[ActiveItems[victim]]._ivalue)
			victim = i;
	}
	if (victim < 0)
		return;
	const Item &junk = Items[ActiveItems[victim]];
	// DeleteItem leaves the tile's dItem, and the reward about to be allocated reuses this slot: the junk's tile would
	// point at the charm (round 7 audit).
	if (InDungeonBounds(junk.position))
		dItem[junk.position.x][junk.position.y] = 0;
	DeleteItem(victim);
}

void SpawnRewardItem(_item_indexes itemid, Point position, bool sendmsg)
{
	// A quest's one-shot reward (Theodore, the Cathedral Map) makes room on a full floor rather than vanishing while the
	// quest moves on - Little Girl and Grave Matters could never complete (round 15 audit, v1.12.240).
	MakeRoomForGuaranteedReward();
	if (ActiveItemCount >= MAXITEMS)
		return;
	// Off an occupied tile, as SpawnQuestItem is (audit, 2026-09-27): Lester's reward and a boss's map land on a fixed
	// tile, and an item already there became unreachable.
	if (InDungeonBounds(position) && dItem[position.x][position.y] != 0) {
		const Point free = GetSuperItemLoc(position);
		if (free != Point { 0, 0 })
			position = free;
	}

	int ii = AllocateItem();
	auto &item = Items[ii];

	item.position = position;
	dItem[position.x][position.y] = ii + 1;
	int curlv = ItemsGetCurrlevel();
	GetItemAttrs(item, itemid, curlv);
	item.setNewAnimation(true);
	item._iSelFlag = 2;
	item._iPostDraw = true;
	item._iIdentified = true;
	GenerateNewSeed(item);

	if (sendmsg) {
		NetSendCmdPItem(true, CMD_SPAWNITEM, item.position, item);
	}
}

void SpawnMapOfDoom(Point position, bool sendmsg)
{
	SpawnRewardItem(IDI_MAPOFDOOM, position, sendmsg);
}

void SpawnRuneBomb(Point position, bool sendmsg)
{
	SpawnRewardItem(IDI_RUNEBOMB, position, sendmsg);
}

void SpawnTheodore(Point position, bool sendmsg)
{
	SpawnRewardItem(IDI_THEODORE, position, sendmsg);
}

int OracoolDropAnimScalePercent(int8_t animIndex)
{
	if (animIndex < FirstOracoolDropAnim || animIndex >= ITEMTYPES)
		return 100; // vanilla's own sheets are drawn as they were made
	return OracoolDropAnimScale[static_cast<size_t>(animIndex - FirstOracoolDropAnim)];
}

void FinishOracoolDrop(int ii, Point position)
{
	// The fork's drop hooks build their item with InitializeItem or MakeSetItem rather than
	// SetupAllItems, so they never passed through SetupItem - and SetupItem is where a dropped item
	// is given its tumble. Without it an item has no sprites: DrawItem returns before drawing it,
	// no label is queued, and _iSelFlag stays 0, so it cannot be picked up either. It was there all
	// along, invisible, until the level was saved and loaded again (user, 2026-09-13: "i never
	// encounter drop of set items but many times if i revisit area i have cleared of mobs i find a
	// number of set items"). That reload gave it sprites with a frame count of 0 and frame 0 - the
	// tumble's first, mid-air frame - which is the "unproportionally big and away from their label".
	GetSuperItemSpace(position, static_cast<int8_t>(ii));
	StartDropTumble(Items[ii]);
}

void StartDropTumble(Item &item)
{
	// Played while in the level, settled while it is still loading - SetupItem's own rule.
	item.setNewAnimation(MyPlayer != nullptr && MyPlayer->pLvlLoad == 0);
}

bool RepairFloorItemAnimation(Item &item)
{
	if (item.isEmpty())
		return false;
	const int8_t frames = ItemAnimLs[GetItemDropAnimIndexFor(item)];
	if (item.AnimInfo.numberOfFrames == frames && item.AnimInfo.currentFrame >= 0 && item.AnimInfo.currentFrame < frames)
		return false;
	// Settled rather than replayed: the item has been lying there since before the save. The vanilla
	// specials keep what RespawnItem gives them - the Magic Rock turns on its pedestal rather than
	// resting, and the quest items that are selected by _iSelFlag 2 stay selectable that way.
	const uint8_t selFlag = item._iSelFlag;
	if (item._iCurs == ICURS_MAGIC_ROCK) {
		item.setNewAnimation(true);
		item._iSelFlag = selFlag != 0 ? selFlag : 1;
		return true;
	}
	item.setNewAnimation(false);
	if (selFlag == 2)
		item._iSelFlag = 2;
	return true;
}

void RespawnItem(Item &item, bool flipFlag)
{
	int it = GetItemDropAnimIndexFor(item);
	item.setNewAnimation(flipFlag);
	item._iRequest = false;
	// Where the player is, or is about to be: a rune landing mid-step is stamped with the tile the
	// step ends on, so finishing that step does not count as the move that earns the pickup.
	if (MyPlayer != nullptr)
		item._iOracoolLandedNear = Point { MyPlayer->position.future.x, MyPlayer->position.future.y };

	if (IsAnyOf(item._iCurs, ICURS_MAGIC_ROCK, ICURS_TAVERN_SIGN, ICURS_ANVIL_OF_FURY))
		item._iSelFlag = 1;
	else if (IsAnyOf(item._iCurs, ICURS_MAP_OF_THE_STARS, ICURS_RUNE_BOMB, ICURS_THEODORE, ICURS_AURIC_AMULET))
		item._iSelFlag = 2;

	if (item._iCurs == ICURS_MAGIC_ROCK) {
		PlaySfxLoc(ItemDropSnds[it], item.position);
	}
}

void DeleteItem(int i)
{
	if (ActiveItemCount > 0)
		ActiveItemCount--;

	assert(i >= 0 && i < MAXITEMS && ActiveItemCount < MAXITEMS);

	if (pcursitem == ActiveItems[i]) // Unselect item if player has it highlighted
		pcursitem = -1;

	if (i < ActiveItemCount) {
		// If the deleted item was not already at the end of the active list, swap the indexes around to make the next item allocation simpler.
		std::swap(ActiveItems[i], ActiveItems[ActiveItemCount]);
	}
}

void ProcessItems()
{
	for (int i = 0; i < ActiveItemCount; i++) {
		int ii = ActiveItems[i];
		auto &item = Items[ii];
		if (!item._iAnimFlag)
			continue;
		item.AnimInfo.processAnimation();
		if (item._iCurs == ICURS_MAGIC_ROCK) {
			if (item._iSelFlag == 1 && item.AnimInfo.currentFrame == 10)
				item.AnimInfo.currentFrame = 0;
			if (item._iSelFlag == 2 && item.AnimInfo.currentFrame == 20)
				item.AnimInfo.currentFrame = 10;
		} else {
			if (item.AnimInfo.currentFrame == (item.AnimInfo.numberOfFrames - 1) / 2)
				PlaySfxLoc(ItemDropSnds[GetItemDropAnimIndexFor(item)], item.position);

			if (item.AnimInfo.isLastFrame()) {
				item.AnimInfo.currentFrame = item.AnimInfo.numberOfFrames - 1;
				item._iAnimFlag = false;
				item._iSelFlag = 1;
			}
		}
	}
	ItemDoppel();
}

void FreeItemGFX()
{
	for (auto &itemanim : itemanims) {
		itemanim = std::nullopt;
	}
}

void GetItemFrm(Item &item)
{
	int it = GetItemDropAnimIndexFor(item);
	if (itemanims[it])
		item.AnimInfo.sprites.emplace(*itemanims[it]);
}

OptionalClxSpriteList GetItemDropAnim(int8_t animIndex)
{
	if (animIndex < 0 || animIndex >= ITEMTYPES || !itemanims[animIndex])
		return std::nullopt;
	return OptionalClxSpriteList { *itemanims[animIndex] };
}

void TrySpawnOracoolSetItem(const Monster &monster, bool sendmsg)
{
	// Oracool: user report (2026-08-15) - "for 6 level not a single new tier item dropped. i think
	// they dont drop at all." They did not: every set item shipped IDROP_NEVER, reachable only
	// through the debug spawn commands. This is their drop path - a hook of its own rather than a
	// seat in GetItemIndexForDroppableItem's pool, because that pool is replayed from item seeds on
	// unpack and growing it transforms existing items (see the guard there).
	//
	// Single-player only, like the tier system itself: the compact multiplayer item pack cannot
	// recreate an item that is not in the seeded pool, and V1 does not play multiplayer.
	if (!oracool::IsSinglePlayer())
		return;

	// Roughly one monster in twelve carries a set piece - loot you notice without every floor
	// papering the ground in green names.
	constexpr int SetDropPercent = 8;
	if (GenerateRnd(100) >= SetDropPercent)
		return;

	const int mlvl = ItemLevelOfMonster(monster);
	// Every set item this depth has earned: iMinMLvl carries the tier ladder (leather at 1-2 up to
	// spectral at 50), so deeper floors drop better tiers by data rather than by a table here.
	_item_indexes candidates[IDI_LAST + 1];
	int candidateCount = 0;
	for (std::underlying_type_t<_item_indexes> i = IDI_ORACOOL_SHOULDERS; i <= IDI_ORACOOL_SPECTRAL_HELM; i++) {
		if (oracool::BandedQlvl(AllItemsList[i].iMinMLvl) <= mlvl)
			candidates[candidateCount++] = static_cast<_item_indexes>(i);
	}
	if (candidateCount == 0 || ActiveItemCount >= MAXITEMS)
		return;
	const _item_indexes idx = candidates[GenerateRnd(candidateCount)];

	// The same construction the debug set commands use, magic roll and tier ladder included, at the
	// monster's loot level. It was clamped to 30 "to keep IsDungeonItemValid satisfied on the loopback",
	// which capped every worn-tier piece at ilvl 30 from Nightmare down (audit, 2026-09-13). That check
	// never runs in single-player - IsPItemValid returns before it - and this hook is single-player only.
	const int lvl = std::max(mlvl, 1);
	Item item;
	SetupAllItems(*MyPlayer, item, idx, AdvanceRndSeed(), lvl, 1, /*onlygood=*/false,
	    /*recreate=*/false, /*pregen=*/false, /*allowTieredRoll=*/true, std::nullopt, /*itemLevel=*/lvl);
	// The drop tail every fresh drop gets (DROP-01): the fork's own gear bases drop only here, and none was ever
	// socketed or ethereal, nor touched by Magic Find (round 11 audit, v1.12.236).
	FinalizeFreshDrop(item, lvl);

	const int ii = AllocateItem();
	Items[ii] = item.pop();
	Point position = monster.position.tile;
	GetSuperItemSpace(position, ii);
	if (sendmsg)
		NetSendCmdPItem(false, CMD_SPAWNITEM, Items[ii].position, Items[ii]);
}

/**
 * @brief The Necromancer's wands, scythes and shrunken heads (2026-09-18, oracool/necro_items.h): a hook of their
 * own, for the reason on every other hook here - the pool is replayed from seeds. One kill in sixteen; the heads
 * only while the hero is a Necromancer; a unique may roll like on any dropped base.
 */
void TrySpawnGildedDrop(const Monster &monster, bool sendmsg)
{
	// The Gilded variant (2026-09-19): "drops better" is a SECOND item, rolled from the ordinary
	// pool two rungs deeper than the monster and with the good-item bias on - the bias a unique
	// monster's drop gets (uper 15). After the vanilla spawns, so the seed-driven stream stays
	// byte-identical, and through FinishOracoolDrop so it tumbles.
	if (!oracool::VariantDropsGilded(monster) || ActiveItemCount >= MAXITEMS)
		return;
	const int lvl = std::clamp(ItemLevelOfMonster(monster) + 2, 1, oracool::MaxAreaLevel); // the ladder's top (round 77 audit)
	// The equipment pool without the ordinary roll's nothing-and-gold outcomes: a Gilded kill is
	// always worth an item, or the recolour is a lie.
	const _item_indexes idx = RndEquipmentForMonsterLevel(static_cast<int8_t>(std::min(lvl, 127)));
	if (idx == IDI_NONE)
		return;
	Item item;
	SetupAllItems(*MyPlayer, item, idx, AdvanceRndSeed(), lvl, 15, /*onlygood=*/true,
	    /*recreate=*/false, /*pregen=*/false, /*allowTieredRoll=*/true, std::nullopt, /*itemLevel=*/lvl);
	FinalizeFreshDrop(item, lvl); // the drop tail (round 11 audit)
	const int ii = AllocateItem();
	Items[ii] = item.pop();
	FinishOracoolDrop(ii, monster.position.tile);
	if (sendmsg)
		NetSendCmdPItem(false, CMD_SPAWNITEM, Items[ii].position, Items[ii]);
}

void TrySpawnNecroBase(const Monster &monster, bool sendmsg)
{
	if (!oracool::IsSinglePlayer())
		return;
	constexpr int NecroDropPercent = 6;
	if (GenerateRnd(100) >= NecroDropPercent)
		return;
	const int mlvl = ItemLevelOfMonster(monster);
	_item_indexes candidates[IDI_LAST + 1];
	int candidateCount = 0;
	const bool heads = oracool::NecroHeadsMayDrop();
	for (std::underlying_type_t<_item_indexes> i = IDI_ORACOOL_NECRO_WAND_FIRST; i <= IDI_ORACOOL_NECRO_HEAD_LAST; i++) {
		if (oracool::IsNecroHeadIdx(i) && !heads)
			continue;
		if (oracool::BandedQlvl(AllItemsList[i].iMinMLvl) <= mlvl)
			candidates[candidateCount++] = static_cast<_item_indexes>(i);
	}
	if (candidateCount == 0 || ActiveItemCount >= MAXITEMS)
		return;
	const _item_indexes idx = candidates[GenerateRnd(candidateCount)];
	const int lvl = std::max(mlvl, 1);
	Item item;
	// A FRESH drop, so allowTieredRoll is true, as on every other dropped base: false is the replay setting, and it
	// barred Rares, Buffed Uniques, Primals and the base tier on wands, scythes and heads, and dropped their uniques
	// ten times too often (round 5 audit, v1.12.230).
	SetupAllItems(*MyPlayer, item, idx, AdvanceRndSeed(), lvl, monster.isUnique() ? 15 : 1, /*onlygood=*/false,
	    /*recreate=*/false, /*pregen=*/false, /*allowTieredRoll=*/true, std::nullopt, /*itemLevel=*/lvl);
	// The drop tail, as the tier-set and Gilded hooks got it in round 11: wands, scythes and heads never rolled sockets or
	// ethereal, nor met Magic Find (round 19 audit, v1.12.244).
	FinalizeFreshDrop(item, lvl);
	const int ii = AllocateItem();
	Items[ii] = item.pop();
	FinishOracoolDrop(ii, monster.position.tile);
	if (sendmsg)
		NetSendCmdPItem(false, CMD_SPAWNITEM, Items[ii].position, Items[ii]);
}

/**
 * @brief The NAMED sets' drop path - the fifteen designed sets, not the tier ladder.
 *
 * TrySpawnOracoolSetItem above is misleadingly named: it drops TIER items from the worn-type range,
 * which are "set" only in the sense of a matching suit. The 94 pieces of the fifteen named sets had
 * no drop path at all until now - giveitemset was the only way to see one, so fifteen sets of
 * artwork, stats and a whole cumulative bonus ladder were unreachable in play.
 *
 * A hook of its own rather than a seat in the droppable pool, for the reason recorded on every other
 * hook here: that pool is replayed from item seeds on unpack, and growing it transforms every
 * existing item.
 */
/**
 * @brief The Signet of Learning's drop - D2MXL-to-ORCL Phase 2b.
 *
 * CHAMPIONS AND BETTER ONLY, which is the whole design of the drop half. Milestones are the
 * reliable source of signets; this is the bonus, and a bonus that fell off ordinary monsters would
 * be a slow trickle nobody could aim at. Tied to TreasureBonusFor, so a monster worth more loot is
 * worth more signets by the same number that decides everything else - and a champion becomes a
 * thing you cross the room for on two counts rather than one.
 *
 * Its own hook rather than a seat in the socketable draw: that draw is one budget shared by five
 * families, and a sixth would quietly make every one of them rarer. This is additive.
 */
void TrySpawnSignet(const Monster &monster, bool sendmsg)
{
	if (!oracool::IsSinglePlayer())
		return;
	const int bonus = oracool::TreasureBonusFor(monster);
	if (bonus <= 1)
		return; // an ordinary kill never yields one
	if (ActiveItemCount >= MAXITEMS)
		return;

	// 3% times what the monster is worth: 6% on a champion, 12% on a unique, 18% on a Dread boss.
	// Roughly six signets across a full clear, against eight from the milestones - so the two halves
	// are comparable and neither makes the other pointless.
	constexpr int SignetDropPercent = 3;
	if (GenerateRnd(100) >= std::min(SignetDropPercent * bonus, 100))
		return;

	const int ii = AllocateItem();
	Item &signet = Items[ii];
	InitializeItem(signet, IDI_ORACOOL_SIGNET_LEARNING);
	GenerateNewSeed(signet);
	signet._iIdentified = true; // it has no rolls to hide
	FinishOracoolDrop(ii, monster.position.tile);
	if (sendmsg)
		NetSendCmdPItem(false, CMD_SPAWNITEM, signet.position, signet);
}

/**
 * @brief Finishes a named set piece the way every other dropped item is finished.
 *
 * Audit finding, 2026-08-26. The drop path was InitializeItem plus MakeSetItem and nothing else, so
 * a set piece arrived with:
 *
 *  - **no seed**. Two pieces for the same slot were therefore byte-identical as far as the network
 *    item record is concerned, and its six-second duplicate filter could reject the second pickup -
 *    a dropped item that simply refuses to be picked up.
 *  - **no item level**, so its depth read as zero everywhere depth is reported.
 *  - **no base tier**, though the user's own directive (see the note at ApplyBaseTier's call site)
 *    asks for sets to drop in all four tiers like everything else. Applied AFTER MakeSetItem
 *    deliberately: the set's declared numbers are the base the tier scales, not the other way
 *    round, and the reverse order would have the tier scaled away by the set's own stats.
 *  - **no ethereal roll**, so the one durable-equipment family in the game that could never be
 *    ethereal was the one whose pieces a player keeps longest.
 *
 * One function so the three construction sites - the monster drop, Recast and Consecrate - cannot
 * drift apart again, which is exactly how they came to differ in the first place.
 */

void FinalizeSetPiece(Item &item, int itemLevel, bool allowEtherealRoll, std::optional<oracool::BaseItemTier> keepTier)
{
	GenerateNewSeed(item);
	item._iOracoolItemLevel = static_cast<uint8_t>(std::clamp(itemLevel, 0, 255));
	item._iCreateInfo = std::min(itemLevel, 63);
	// A recipe keeps the tier the item had (round 34 audit: a Torment piece recast or consecrated came back re-rolled, often
	// lower), as Retier pins it; a fresh piece rolls it from its seed.
	oracool::ApplyBaseTier(item, keepTier.value_or(oracool::TierForItem(item._iOracoolItemLevel, item._iSeed)));
	// Drop-only, like every other durable item - see TryMakeDroppedItemEthereal for why the roll
	// lives there and not in MakeItemEthereal. Recast and Consecrate pass false: a recipe must not
	// hand a player an ethereal item they did not ask for, and Make Ethereal is its own recipe.
	if (allowEtherealRoll)
		TryMakeDroppedItemEthereal(item);
}


void TrySpawnNamedSetPiece(const Monster &monster, bool sendmsg)
{
	if (!oracool::IsSinglePlayer())
		return;

	// Rarer than a tier piece (8%) and rarer than a gem, because a named piece is a step toward a
	// COMPLETE set rather than a self-contained reward - the ladder is the payoff, and a set you
	// finish in an afternoon has no ladder worth climbing.
	//
	// The rate comes from the floor's TREASURE CLASS now (Phase 5), and is multiplied by what the
	// monster is worth - a champion twice, a unique four times. The base 3% survives as the shallow
	// zones' number; the deep ones pay 5. Capped at 100 so a hypothetical high table and a unique
	// cannot ask GenerateRnd for a percentage that does not exist.
	const oracool::TreasureClass &tc = oracool::CurrentTreasureClass();
	const int namedSetPercent = std::min(
	    oracool::ScaleRateForDifficulty(tc.setPercent) * oracool::TreasureBonusFor(monster), 100);
	// Drawn out of 1000, not 100: the table's percent is read as PER MILLE, ten times rarer (user,
	// 2026-09-13: "decrease drop chance of uniques and set item 10 fold") - 0.3% to 0.5% a kill.
	if (namedSetPercent <= 0 || GenerateRnd(1000) >= namedSetPercent)
		return;
	if (ActiveItemCount >= MAXITEMS)
		return;

	const int mlvl = ItemLevelOfMonster(monster);

	// Every piece this depth has earned AND that can actually be built. Both filters matter: the
	// level gate is what makes deep floors drop the deep sets, and the base check is what keeps the
	// two slots this fork has not built (relic, cloak) from being rolled and silently dropped.
	struct Candidate {
		const oracool::SetItemDefinition *def;
		int base;
		int weight;
	};
	std::vector<Candidate> candidates;
	int totalWeight = 0;
	for (const oracool::ItemSetDefinition &set : oracool::ItemSets) {
		// Computed once per SET rather than per piece: HeldSetPieces walks the backpack and the
		// whole stash for every piece of the set, so asking it inside the inner loop would make
		// this quadratic in the stash's size on every 3% drop.
		const int held = oracool::HeldSetPieces(*MyPlayer, set);
		for (int i = 0; i < set.itemCount; i++) {
			const oracool::SetItemDefinition &def = oracool::ItemSetItems[set.firstItem + i];
			if (oracool::BandedQlvl(def.requiredLevel) > mlvl)
				continue;
			const int base = oracool::BaseItemForSetPiece(def);
			if (base < 0)
				continue;

			// THE BIAS: a piece is worth more when you already hold pieces of ITS set and do not
			// hold this one. The rule itself lives in SetPieceDropWeight so it can be read and
			// tested without spawning a monster; only the two inputs are gathered here.
			const int weight = oracool::SetPieceDropWeight(held, oracool::IsSetPieceHeld(*MyPlayer, def));
			candidates.push_back({ &def, base, weight });
			totalWeight += weight;
		}
	}
	if (candidates.empty())
		return;

	// Weighted pick. GenerateRnd is the level's own stream, the same one the uniform pick used.
	int roll = GenerateRnd(totalWeight);
	size_t pick = 0;
	for (size_t i = 0; i < candidates.size(); i++) {
		roll -= candidates[i].weight;
		if (roll < 0) {
			pick = i;
			break;
		}
	}
	const Candidate &chosen = candidates[pick];
	Item item {};
	InitializeItem(item, static_cast<_item_indexes>(chosen.base));
	oracool::MakeSetItem(item, *chosen.def);
	FinalizeSetPiece(item, mlvl, /*allowEtherealRoll=*/true);

	const int ii = AllocateItem();
	Items[ii] = item.pop();
	FinishOracoolDrop(ii, monster.position.tile);
	LogNoteworthyItemDrop(Items[ii]);
	if (sendmsg)
		NetSendCmdPItem(false, CMD_SPAWNITEM, Items[ii].position, Items[ii]);
}

void TrySpawnOracoolGem(const Monster &monster, bool sendmsg)
{
	// Megaplan Phase 1: the gems' own drop path - a hook rather than a pool seat, for exactly the
	// reason TrySpawnOracoolSetItem's comment records: the droppable pool is save format.
	if (!oracool::IsSinglePlayer())
		return;

	// PHASE 5: the rate and the family split both come from the floor's treasure class now. They
	// used to be four constants applied to every monster in the game - 3% gem, 1% charm, 2% rune,
	// 1% jewel, everywhere - so depth changed which socketables were ELIGIBLE and never changed
	// what a floor was for. Two floors at the same area level were interchangeable.
	//
	// Still ONE draw for whether-anything-drops and one for which-family, as before, so the number
	// of GenerateRnd calls on this path is unchanged and no zone consumes the level's stream at a
	// different rate than another. What changed is only what the draws are taken against.
	const oracool::TreasureClass &tc = oracool::CurrentTreasureClass();
	const int bonus = oracool::TreasureBonusFor(monster);
	const int socketablePercent = std::min(oracool::ScaleRateForDifficulty(tc.socketablePercent) * bonus, 100);
	const int familyTotal = oracool::TotalFamilyWeight(tc);
	if (socketablePercent <= 0 || familyTotal <= 0)
		return;
	if (GenerateRnd(100) >= socketablePercent)
		return;

	const oracool::SocketableFamily family = oracool::FamilyForRoll(tc, GenerateRnd(familyTotal));

	const int mlvl = ItemLevelOfMonster(monster);
	_item_indexes idx;

	if (family == oracool::SocketableFamily::Gem) {
		// A gem is picked as a TYPE and a QUALITY rather than as one index out of thirty-five,
		// because those two axes want different rules: the type is a flat choice among seven, and
		// the quality is a ladder the depth opens and the weights keep steep, so a chipped stone is
		// the common find and a perfect one stays a prize even once the floor allows it.
		static constexpr int QualityWeights[oracool::GemQualityCount] = { 40, 30, 18, 9, 3 };
		const auto type = static_cast<oracool::GemType>(GenerateRnd(oracool::GemTypeCount));
		int available[oracool::GemQualityCount];
		int weights[oracool::GemQualityCount];
		int count = 0;
		int weightTotal = 0;
		for (size_t q = 0; q < oracool::GemQualityCount; q++) {
			const uint16_t candidate = oracool::GemIndexFor(type, static_cast<oracool::GemQuality>(q));
			if (oracool::BandedQlvl(AllItemsList[candidate].iMinMLvl) > mlvl)
				continue;
			available[count] = candidate;
			weightTotal += QualityWeights[q];
			weights[count] = weightTotal;
			count++;
		}
		if (count == 0 || ActiveItemCount >= MAXITEMS)
			return;
		const int pick = GenerateRnd(weightTotal);
		int chosen = 0;
		while (chosen + 1 < count && pick >= weights[chosen])
			chosen++;
		idx = static_cast<_item_indexes>(available[chosen]);
	} else {
		// The rune walk goes through the LADDER, not an index range: the 33 runes live in two enum
		// islands (the five from v1.7.8, then 28 appended after the gem ladder), so the old
		// EL..SOL span would have dropped exactly five of the thirty-three and never the rest.
		// Sized for the WHOLE item table, not for the rune ladder. Three different families write
		// this array - charms, runes, jewels - and it used to be MaxRuneLadder (33), which is the
		// size of only one of them. The charms are 13 today and the jewels 15, so nothing overflows
		// now; but the charm walk deliberately runs to IDI_LAST so that a charm family appended
		// later is picked up without touching it, and the day that happens this bound is what it
		// runs into. An array named for one family and filled by three is a trap with a fuse.
		_item_indexes candidates[IDI_LAST + 1];
		int candidateCount = 0;
		if (family == oracool::SocketableFamily::Charm) {
			// The charms live in three enum islands of their own (the MF/GF pair was appended after
			// the runes, the seven Charms of Salvaging after the salvage materials), so the walk
			// spans all of them and filters by the range check. Bounded by IDI_LAST rather than by
			// the last charm id on purpose: the previous GREED bound silently excluded every charm
			// appended after it, which is exactly how the salvage charms would have shipped
			// unobtainable as drops. The qlvl gate below is what keeps the deep tiers deep.
			for (int i = IDI_ORACOOL_CHARM_VIGOR; i <= IDI_LAST; i++) {
				// The Charms of Salvaging are SHOP ONLY (user, 2026-09-13: "make charms of salvaging
				// non-dropable, only purchcasable") - Griswold and Adria stock them.
				// Nor the named encounters' own charms (Chapel, Mourning, Vault): the encounter pays them, and Pepin's shelf
				// already leaves them out - they fell off Catacombs monsters from Nightmare on (round 11 audit, v1.12.236).
				if (!IsOracoolCharmIdx(i) || IsOracoolSalvageCharmIdx(i) || IsOracoolEncounterCharmIdx(i))
					continue;
				if (oracool::BandedQlvl(AllItemsList[i].iMinMLvl) <= mlvl)
					candidates[candidateCount++] = static_cast<_item_indexes>(i);
			}
		} else if (family == oracool::SocketableFamily::Rune) {
			// Runes TEN TIMES rarer (user, 2026-09-13). A second roll on the rune share alone rather
			// than a smaller rune weight: shrinking the weight would hand its share to gems, jewels,
			// charms and orbs, and only the runes were asked to change.
			if (GenerateRnd(10) != 0)
				return;
			for (size_t rung = 0; rung < oracool::RuneLadderSize(); rung++) {
				const uint16_t rune = oracool::RuneAtLadderPosition(rung);
				if (oracool::BandedQlvl(AllItemsList[rune].iMinMLvl) <= mlvl)
					candidates[candidateCount++] = static_cast<_item_indexes>(rune);
			}
		} else if (family == oracool::SocketableFamily::Jewel) {
			// The jewels are one contiguous island, so a plain range walk is enough - no ladder and
			// no two-island filter. The qlvl gate does the same work it does for every family here:
			// Flawed jewels from early on, Radiant ones only once the floor has earned them.
			for (int i = IDI_ORACOOL_JEWEL_FERVOR_FLAWED; i <= IDI_ORACOOL_JEWEL_WARDING_RADIANT; i++) {
				if (oracool::BandedQlvl(AllItemsList[i].iMinMLvl) <= mlvl)
					candidates[candidateCount++] = static_cast<_item_indexes>(i);
			}
		} else {
			// The Imbuement Shards: two islands (the orbs' old indices and the tail of the enum), walked
			// through the kind table so neither range is written here. Their qlvls are the drop bands
			// (decision D10): shallow kinds from the first rung, the stats from the Caves' rung, and
			// Refinement, Stone and Arcana from the second difficulty.
			oracool::ForEachShardItem([&](int i) {
				if (oracool::BandedQlvl(AllItemsList[i].iMinMLvl) <= mlvl)
					candidates[candidateCount++] = static_cast<_item_indexes>(i);
			});
		}
		if (candidateCount == 0 || ActiveItemCount >= MAXITEMS)
			return;
		idx = candidates[GenerateRnd(candidateCount)];
	}

	const int ii = AllocateItem();
	Item &gem = Items[ii];
	InitializeItem(gem, idx);
	GenerateNewSeed(gem);
	gem._iIdentified = true; // a gem has no rolls to hide
	FinishOracoolDrop(ii, monster.position.tile);
	if (sendmsg)
		NetSendCmdPItem(false, CMD_SPAWNITEM, gem.position, gem);
}

void TryAddSocketsToDroppedItem(Item &item)
{
	// Megaplan Phase 1: sockets roll ONLY here, on the drop paths, AFTER SetupAllItems - never
	// inside it. SetupAllItems is replayed from stored seeds when items are recreated, and a roll
	// added inside that replay would shift every seeded stream (the drop-pool lesson, again). The
	// full-record save paths (heroitems, stash, per-level items) carry the socket fields verbatim,
	// so nothing needs the roll to be reproducible.
	if (!oracool::IsSinglePlayer() || !oracool::CanItemHaveSockets(item))
		return;

	// A quarter of plain equipment is socketed: common enough that "basic item" stays worth a
	// look forever, rare enough that a full-footprint roll still lands as an event.
	if (GenerateRnd(100) >= 25)
		return;

	// Sockets v2: the ceiling is the item's own footprint, so a ring can only ever take one and a
	// two-hander can take six. The weights are declared for the full six and then TRUNCATED at the
	// item's cap and renormalised, which is what keeps a small item's distribution sane - a 1x2
	// glove rolls 60/25 over one and two sockets rather than rolling six and clamping every high
	// draw down onto two.
	const int cap = oracool::MaxSocketsForItem(item);
	if (cap <= 0)
		return;
	constexpr int SocketWeights[Item::MaxItemSockets] = { 60, 25, 8, 4, 2, 1 };
	int total = 0;
	for (int i = 0; i < cap; i++)
		total += SocketWeights[i];
	int roll = GenerateRnd(total);
	int count = 1;
	for (int i = 0; i < cap; i++) {
		if (roll < SocketWeights[i]) {
			count = i + 1;
			break;
		}
		roll -= SocketWeights[i];
	}
	item._iSocketCount = static_cast<uint8_t>(count);
}

void ApplyMagicAndGoldFindToDrop(Item &item, int mLevel)
{
	// Phase 1 Magic/Gold Find, consumed HERE and only here - the unseeded drop tail. Reading the
	// player's find stats inside seed-replayed setup would make recreation depend on whatever the
	// player wears at replay time; out here the roll happens once, at the true drop, and the
	// result rides the full-record save paths like every other drop-tail mutation.
	if (!oracool::IsSinglePlayer() || MyPlayer == nullptr || item.isEmpty())
		return;

	if (item._itype == ItemType::Gold) {
		const int goldFind = MyPlayer->_pGoldFind;
		if (goldFind > 0) {
			item._ivalue = std::min<int>(item._ivalue * (100 + goldFind) / 100, MaxGold);
			SetPlrHandGoldCurs(item);
		}
		return;
	}

	// Capped at 75 (round 37 audit): uncapped, 100 turned every white weapon and armour into a Rare.
	const int magicFind = std::min<int>(MyPlayer->_pMagicFind, 75);
	if (magicFind <= 0 || item._iMagical != ITEM_QUALITY_NORMAL || item.hasOracoolTier())
		return;
	if (item._iClass != ICLASS_WEAPON && item._iClass != ICLASS_ARMOR)
		return;
	if (GenerateRnd(100) >= magicFind)
		return;

	const AffixItemType flgs = GetAffixItemTypeForItem(item);
	if (flgs == AffixItemType::None)
		return;
	// The same shape the debug giverare path uses: half-to-full of the drop's level band.
	const int iblvl = std::max(1, mLevel);
	GetRareItemAffixes(*MyPlayer, item, iblvl / 2, iblvl, flgs, /*onlygood=*/false, /*ignoreLevelLimits=*/true);
}

bool MakeItemEthereal(Item &item)
{
	// Phase 1 ethereal: a ghost of an item - more of everything, half the lifespan, and no smith
	// can touch it. The bargain is stamped into the item's own stats here, so nothing downstream
	// computes anything.
	//
	// Split out of TryMakeDroppedItemEthereal on 2026-08-20 so the debug spawner can force what the
	// drop path rolls for. The ELIGIBILITY rules live here with the arithmetic rather than at the
	// caller, which is the point of the split: a command that could make a potion ethereal, or that
	// applied 135% by its own copy of the sum, would be testing something the game cannot produce.
	if (item.isEmpty())
		return false;
	if (item._iClass != ICLASS_WEAPON && item._iClass != ICLASS_ARMOR)
		return false;
	if (item._iMaxDur == 0 || item._iMaxDur == DUR_INDESTRUCTIBLE)
		return false;

	item._iOracoolEthereal = true;
	if (item._iClass == ICLASS_WEAPON) {
		// Held to the byte the fields are: a Torment Deathbringer (105-227) came out 141-50, and every blow landed at the
		// minimum (round 5 audit, v1.12.230).
		item._iMinDam = static_cast<uint8_t>(std::min(item._iMinDam * 135 / 100, 255));
		item._iMaxDam = static_cast<uint8_t>(std::clamp(item._iMaxDam * 135 / 100, static_cast<int>(item._iMinDam), 255));
	} else {
		item._iAC = std::max<int>(item._iAC * 135 / 100, item._iAC + 1);
		// The oils' share grows with the rest, or every rebuild after this would lose a third of it (round 55 audit).
		if (item._iOracoolOilAC > 0)
			item._iOracoolOilAC = static_cast<int16_t>(std::min(item._iOracoolOilAC * 135 / 100, static_cast<int>(INT16_MAX)));
	}
	// The base halves, Tempering's shards do not (round 54 audit: halved here and taken back whole by Cleanse, they cost
	// ten maximum durability a shard for good).
	const int tempering = std::clamp(oracool::ShardDurabilityBonus(item), 0, item._iMaxDur);
	item._iMaxDur = std::max<int>(1, (item._iMaxDur - tempering) / 2 + tempering);
	item._iDurability = std::min<int>(item._iDurability, item._iMaxDur);

	// Re-arm a socketed Zod, which the two lines above have just disarmed.
	//
	// Zod writes DUR_INDESTRUCTIBLE into _iDurability and leaves _iMaxDur alone (ApplyZodToHost, and
	// see its comment for why that representation). The eligibility guard at the top of this
	// function tests _iMaxDur, so a Zod-bearing item passes it - and then the durability clamp above
	// replaces the indestructible marker with half the old maximum. The rune stayed socketed,
	// stayed listed on the item, stayed spent, and stopped doing anything (external audit,
	// 2026-08-25).
	//
	// Re-applied rather than refused: an indestructible ethereal item is Diablo II's own best-known
	// rune combination, not an accident to be prevented. It just has to survive being made.
	oracool::ApplyZodToHost(item);
	return true;
}

void TryMakeDroppedItemEthereal(Item &item)
{
	// Rolled on the drop paths for any quality of durable equipment. Single-player only, and the
	// 5% is the drop rate rather than a property of ethereal itself - which is why it stays here
	// and not in MakeItemEthereal.
	if (!oracool::IsSinglePlayer())
		return;
	if (GenerateRnd(100) >= 5)
		return;

	MakeItemEthereal(item);
}

void GetItemStr(Item &item)
{
	if (item._itype != ItemType::Gold) {
		// Oracool: the name carries the item's tier colour, and SetPanelString records that as line
		// 0's colour so the stat lines below it can be coloured independently.
		SetPanelString(item.getName(), item.getTextColor());
	} else {
		int nGold = item._ivalue;
		// SetPanelString, not a bare assignment - see the identical note in CheckInvHLight. This is
		// the same slip in the same shape: the non-gold arm two lines up records its colour and this
		// one did not, so the stale list outlived the text it belonged to.
		SetPanelString(fmt::format(fmt::runtime(ngettext("{:s} gold piece", "{:s} gold pieces", nGold)), FormatInteger(nGold)), UiFlags::ColorWhite);
	}
}

// Oracool: shared by CheckIdentify/DoRepair/DoRecharge/DoOil below - tabIdx >= 0 (an Oracool
// Tabbed Inventory extra tab) takes priority over the vanilla InvBody/InvList encoding cii would
// otherwise resolve through, since a tab-sourced target has no meaningful cii of its own.
static Item *ResolveInvOrTabItem(Player &player, int cii, int tabIdx)
{
	if (tabIdx >= 0)
		return &player.InvTabList[tabIdx][cii];
	if (cii >= NUM_INVLOC)
		return &player.InvList[cii - NUM_INVLOC];
	return &player.InvBody[cii];
}

void CheckIdentify(Player &player, int cii, int tabIdx)
{
	Item *pi = ResolveInvOrTabItem(player, cii, tabIdx);

	pi->_iIdentified = true;
	CalcPlrInv(player, true);
}

void DoRepair(Player &player, int cii, int tabIdx)
{
	PlaySfxLoc(IS_REPAIR, player.position.tile);

	Item *pi = ResolveInvOrTabItem(player, cii, tabIdx);

	RepairItem(*pi, player._pLevel);
	CalcPlrInv(player, true);
}

void DoRecharge(Player &player, int cii, int tabIdx)
{
	Item *pi = ResolveInvOrTabItem(player, cii, tabIdx);

	RechargeItem(*pi, player);
	CalcPlrInv(player, true);
}

bool DoOil(Player &player, int cii, int tabIdx)
{
	Item *pi = ResolveInvOrTabItem(player, cii, tabIdx);
	if (!ApplyOilToItem(*pi, player))
		return false;
	CalcPlrInv(player, true);
	return true;
}

[[nodiscard]] StringOrView PrintItemPower(char plidx, const Item &item)
{
	switch (plidx) {
	case IPL_TOHIT:
	case IPL_TOHIT_CURSE:
		return fmt::format(fmt::runtime(_("chance to hit: {:+d}%")), item._iPLToHit);
	case IPL_DAMP:
	case IPL_DAMP_CURSE:
		return fmt::format(fmt::runtime(_(/*xgettext:no-c-format*/ "{:+d}% damage")), item._iPLDam);
	case IPL_TOHIT_DAMP:
	case IPL_TOHIT_DAMP_CURSE:
		return fmt::format(fmt::runtime(_("to hit: {:+d}%, {:+d}% damage")), item._iPLToHit, item._iPLDam);
	case IPL_ACP:
	case IPL_ACP_CURSE:
		return fmt::format(fmt::runtime(_(/*xgettext:no-c-format*/ "{:+d}% armor")), item._iPLAC);
	case IPL_SETAC:
	case IPL_AC_CURSE:
		return fmt::format(fmt::runtime(_("armor class: {:d}")), item._iAC);
	case IPL_FIRERES:
	case IPL_FIRERES_CURSE:
		return fmt::format(fmt::runtime(_("Resist Fire: {:+d}%")), item._iPLFR);
	case IPL_LIGHTRES:
	case IPL_LIGHTRES_CURSE:
		return fmt::format(fmt::runtime(_("Resist Lightning: {:+d}%")), item._iPLLR);
	case IPL_MAGICRES:
	case IPL_MAGICRES_CURSE:
		return fmt::format(fmt::runtime(_("Resist Magic: {:+d}%")), item._iPLMR);
	case IPL_COLDRES:
	case IPL_COLDRES_CURSE:
		return fmt::format(fmt::runtime(_("Resist Cold: {:+d}%")), item._iPLCR);
	// The value the item carries (audit, 2026-09-27). Vanilla printed "+75% MAX" from 75 up, its old cap; the fork caps
	// the hero's total at 90 after the difficulty penalty, and an item's own +80 is +80 of it.
	case IPL_ALLRES:
		return fmt::format(fmt::runtime(_("Resist All: {:+d}%")), item._iPLFR);
	case IPL_SPLLVLADD:
		if (item._iSplLvlAdd > 0)
			return fmt::format(fmt::runtime(ngettext("spells are increased {:d} level", "spells are increased {:d} levels", item._iSplLvlAdd)), item._iSplLvlAdd);
		else if (item._iSplLvlAdd < 0)
			return fmt::format(fmt::runtime(ngettext("spells are decreased {:d} level", "spells are decreased {:d} levels", -item._iSplLvlAdd)), -item._iSplLvlAdd);
		else
			return _("spell levels unchanged (?)");
	case IPL_CHARGES:
		return _("Extra charges");
	case IPL_SPELL:
		return fmt::format(fmt::runtime(ngettext("{:d} {:s} charge", "{:d} {:s} charges", item._iMaxCharges)), item._iMaxCharges, pgettext("spell", GetSpellData(item._iSpell).sNameText));
	case IPL_FIREDAM:
		if (item._iFMinDam == item._iFMaxDam)
			return fmt::format(fmt::runtime(_("Fire hit damage: {:d}")), item._iFMinDam);
		else
			return fmt::format(fmt::runtime(_("Fire hit damage: {:d}-{:d}")), item._iFMinDam, item._iFMaxDam);
	case IPL_LIGHTDAM:
		if (item._iLMinDam == item._iLMaxDam)
			return fmt::format(fmt::runtime(_("Lightning hit damage: {:d}")), item._iLMinDam);
		else
			return fmt::format(fmt::runtime(_("Lightning hit damage: {:d}-{:d}")), item._iLMinDam, item._iLMaxDam);
	case IPL_STR:
	case IPL_STR_CURSE:
		return fmt::format(fmt::runtime(_("{:+d} to strength")), item._iPLStr);
	case IPL_MAG:
	case IPL_MAG_CURSE:
		return fmt::format(fmt::runtime(_("{:+d} to magic")), item._iPLMag);
	case IPL_DEX:
	case IPL_DEX_CURSE:
		return fmt::format(fmt::runtime(_("{:+d} to dexterity")), item._iPLDex);
	case IPL_VIT:
	case IPL_VIT_CURSE:
		return fmt::format(fmt::runtime(_("{:+d} to vitality")), item._iPLVit);
	case IPL_ATTRIBS:
	case IPL_ATTRIBS_CURSE:
		return fmt::format(fmt::runtime(_("{:+d} to all attributes")), item._iPLStr);
	case IPL_GETHIT_CURSE:
	case IPL_GETHIT:
		return fmt::format(fmt::runtime(_("{:+d} damage from enemies")), item._iPLGetHit);
	case IPL_LIFE:
	case IPL_LIFE_CURSE:
		return fmt::format(fmt::runtime(_("Hit Points: {:+d}")), item._iPLHP >> 6);
	case IPL_MANA:
	case IPL_MANA_CURSE:
		return fmt::format(fmt::runtime(_("Mana: {:+d}")), item._iPLMana >> 6);
	case IPL_DUR:
		return _("high durability");
	case IPL_DUR_CURSE:
		return _("decreased durability");
	case IPL_INDESTRUCTIBLE:
		return _("indestructible");
	case IPL_LIGHT:
		// Signed by the value, not by the power (2026-09-25): a set piece declares IPL_LIGHT with -1, and the literal
		// '+' in front of a negative number printed "+-10% light radius".
		return fmt::format(fmt::runtime(_(/*xgettext:no-c-format*/ "{:+d}% light radius")), 10 * item._iPLLight);
	case IPL_LIGHT_CURSE:
		return fmt::format(fmt::runtime(_(/*xgettext:no-c-format*/ "-{:d}% light radius")), -10 * item._iPLLight);
	case IPL_MULT_ARROWS:
		return _("multiple arrows per shot");
	case IPL_FIRE_ARROWS:
		if (item._iFMinDam == item._iFMaxDam)
			return fmt::format(fmt::runtime(_("fire arrows damage: {:d}")), item._iFMinDam);
		else
			return fmt::format(fmt::runtime(_("fire arrows damage: {:d}-{:d}")), item._iFMinDam, item._iFMaxDam);
	case IPL_LIGHT_ARROWS:
		if (item._iLMinDam == item._iLMaxDam)
			return fmt::format(fmt::runtime(_("lightning arrows damage {:d}")), item._iLMinDam);
		else
			return fmt::format(fmt::runtime(_("lightning arrows damage {:d}-{:d}")), item._iLMinDam, item._iLMaxDam);
	case IPL_FIREBALL:
		if (item._iFMinDam == item._iFMaxDam)
			return fmt::format(fmt::runtime(_("fireball damage: {:d}")), item._iFMinDam);
		else
			return fmt::format(fmt::runtime(_("fireball damage: {:d}-{:d}")), item._iFMinDam, item._iFMaxDam);
	case IPL_THORNS:
		return _("attacker takes 1-3 damage");
	case IPL_NOMANA:
		return _("user loses all mana");
	case IPL_ABSHALFTRAP:
		return _("absorbs half of trap damage");
	case IPL_KNOCKBACK:
		return _("knocks target back");
	case IPL_3XDAMVDEM:
		return _(/*xgettext:no-c-format*/ "+200% damage vs. demons");
	case IPL_ALLRESZERO:
		return _("All Resistance equals 0");
	case IPL_STEALMANA:
		if (HasAnyOf(item._iFlags, ItemSpecialEffect::StealMana3))
			return _(/*xgettext:no-c-format*/ "hit steals 3% mana");
		if (HasAnyOf(item._iFlags, ItemSpecialEffect::StealMana5))
			return _(/*xgettext:no-c-format*/ "hit steals 5% mana");
		return {};
	case IPL_STEALLIFE:
		if (HasAnyOf(item._iFlags, ItemSpecialEffect::StealLife3))
			return _(/*xgettext:no-c-format*/ "hit steals 3% life");
		if (HasAnyOf(item._iFlags, ItemSpecialEffect::StealLife5))
			return _(/*xgettext:no-c-format*/ "hit steals 5% life");
		return {};
	case IPL_TARGAC:
		return _("penetrates target's armor");
	case IPL_FASTATTACK:
		// On a bow the flags quicken the arrow, not the draw - Hellfire's rule, always on here - so the line says so; it
		// read as a faster attack and gave no frames (round 16 audit, v1.12.241).
		if (item._itype == ItemType::Bow) {
			if (HasAnyOf(item._iFlags, ItemSpecialEffect::FastestAttack))
				return _("fastest arrows");
			if (HasAnyOf(item._iFlags, ItemSpecialEffect::FasterAttack))
				return _("faster arrows");
			if (HasAnyOf(item._iFlags, ItemSpecialEffect::FastAttack))
				return _("fast arrows");
			return _("quick arrows");
		}
		// Off the bow, while the hero shoots one, they quicken his arrows too (round 27 audit).
		if (HasAnyOf(item._iFlags, ItemSpecialEffect::QuickAttack))
			return oracool::AttackSpeedWords(_("quick attack"));
		if (HasAnyOf(item._iFlags, ItemSpecialEffect::FastAttack))
			return oracool::AttackSpeedWords(_("fast attack"));
		if (HasAnyOf(item._iFlags, ItemSpecialEffect::FasterAttack))
			return oracool::AttackSpeedWords(_("faster attack"));
		if (HasAnyOf(item._iFlags, ItemSpecialEffect::FastestAttack))
			return oracool::AttackSpeedWords(_("fastest attack"));
		return _("Another ability (NW)");
	case IPL_FASTRECOVER:
		if (HasAnyOf(item._iFlags, ItemSpecialEffect::FastHitRecovery))
			return _("fast hit recovery");
		if (HasAnyOf(item._iFlags, ItemSpecialEffect::FasterHitRecovery))
			return _("faster hit recovery");
		if (HasAnyOf(item._iFlags, ItemSpecialEffect::FastestHitRecovery))
			return _("fastest hit recovery");
		return _("Another ability (NW)");
	case IPL_FASTBLOCK:
		return _("fast block");
	case IPL_DAMMOD:
		return fmt::format(fmt::runtime(ngettext("adds {:d} point to damage", "adds {:d} points to damage", item._iPLDamMod)), item._iPLDamMod);
	case IPL_RNDARROWVEL:
		return _("fires random speed arrows");
	case IPL_SETDAM:
		return _("unusual item damage");
	case IPL_SETDUR:
		return _("altered durability");
	case IPL_ONEHAND:
		return _("one-handed"); // not only swords: Morrowbell is a mace (round 49 audit)
	case IPL_DRAINLIFE:
		return _("constantly lose hit points");
	case IPL_RNDSTEALLIFE:
		return _("life stealing");
	case IPL_NOMINSTR:
		return _("no strength requirement");
	case IPL_INVCURS:
		return { string_view(" ") };
	case IPL_ADDACLIFE:
		if (item._iFMinDam == item._iFMaxDam)
			return fmt::format(fmt::runtime(_("lightning damage: {:d}")), item._iFMinDam);
		else
			return fmt::format(fmt::runtime(_("lightning damage: {:d}-{:d}")), item._iFMinDam, item._iFMaxDam);
	case IPL_ADDMANAAC:
		// Single-player casts no bolts (the Hellfire pair is off there); what lands is the fire hit (round 16 audit).
		if (!gbIsMultiplayer)
			return fmt::format(fmt::runtime(_("fire hit damage: {:d}-{:d}")), item._iFMinDam, item._iFMaxDam);
		return _("charged bolts on hits");
	case IPL_DEVASTATION:
		return _("occasional triple damage");
	case IPL_DECAY:
		return fmt::format(fmt::runtime(_(/*xgettext:no-c-format*/ "decaying {:+d}% damage")), item._iPLDam);
	case IPL_PERIL:
		return _("2x dmg to monst, 1x to you");
	case IPL_JESTERS:
		return std::string(_(/*xgettext:no-c-format*/ "Random 0 - 600% damage"));
	case IPL_CRYSTALLINE:
		return fmt::format(fmt::runtime(_(/*xgettext:no-c-format*/ "low dur, {:+d}% damage")), item._iPLDam);
	case IPL_DOPPELGANGER:
		return fmt::format(fmt::runtime(_("to hit: {:+d}%, {:+d}% damage, 10% of hits clone the foe")), item._iPLToHit, item._iPLDam);
	case IPL_ACDEMON:
		return _("extra AC vs demons");
	case IPL_ACUNDEAD:
		return _("extra AC vs undead");
	case IPL_MANATOLIFE:
		return _("50% Mana moved to Health");
	case IPL_LIFETOMANA:
		return _("40% Health moved to Mana");
	// Oracool's own powers. A unique or a set piece prints its powers through here, and these fell to
	// "Another ability" below until a unique carried one (2026-09-11). Worded as the affix printer
	// words them, so the same stat reads the same wherever it came from.
	case IPL_GOLDFIND:
		return fmt::format(fmt::runtime(_("{:+d}% gold from monsters")), item._iPLGoldFind);
	case IPL_MAGICFIND:
		return fmt::format(fmt::runtime(_("{:+d}% chance a plain weapon or armor found is Rare")), item._iPLMagicFind);
	case IPL_MOVESPEED:
	case IPL_MOVESPEED_CURSE:
		return fmt::format(fmt::runtime(_("{:+d}% movement speed")), item._iPLMoveSpeed);
	case IPL_FASTCAST:
		return fmt::format(fmt::runtime(_("{:+d}% faster cast rate")), item._iPLFastCast);
	default:
		return _("Another ability (NW)");
	}
}

/**
 * @brief Oracool: regression fix - a Rare/Buffed Unique/Primal item's affix list is displayed by
 * calling PrintItemPower once per stored affix, but PrintItemPower's simple-stat cases (Strength,
 * Dexterity, to-hit, damage %, armor %, resistances, ...) read the item's single accumulated
 * vanilla field (item._iPLStr, item._iPLDam, ...) rather than that specific affix's own value.
 * Since a tiered item can carry several affixes that all add into the SAME accumulated field
 * (e.g. two different affix types both bumping _iPLStr), every one of those lines ended up
 * displaying the same combined total - looking like duplicate/identical affixes even though the
 * underlying rolls were correctly distinct (and the dedup-by-type check that already blocks a
 * literal repeat of one affix type was working exactly as designed the whole time). This wrapper
 * uses each affix's own individually-stored OracoolAffix::param1 for those simple cases instead,
 * with each case applying the correct sign for that specific affix type (positive/"good" types and
 * their "_CURSE"/negative counterparts share the same underlying roll magnitude in param1, but apply
 * it with opposite signs - see SaveItemPower). Falls back to the shared PrintItemPower only for
 * compound/rarer types not covered here (IPL_TOHIT_DAMP, IPL_SETAC/IPL_AC_CURSE, IPL_LIGHT/IPL_LIGHT_CURSE,
 * ...), which remain susceptible to the same field-collision display issue this fixes for the common
 * cases, but are rarer combinations and were out of scope for this pass.
 */
StringOrView PrintOracoolAffixPower(const OracoolAffix &affix, const Item &item)
{
	// affix.param1 is always the positive roll magnitude (see RepairOracoolAffixValue / SaveItemPower,
	// which always returns the unsigned RndPL roll regardless of whether the type adds or subtracts
	// it). Every case below must apply its own sign to match SaveItemPower's real effect on the item -
	// mixing a "curse" (subtracting) type into the same case as its positive counterpart, both
	// printing the unsigned affix.param1, previously made every curse-flavored line show a bonus
	// instead of a penalty (e.g. a -9 Strength affix displaying as "+9 to strength"). IPL_GETHIT is the
	// one intentionally inverted pair: IPL_GETHIT itself *reduces* damage taken (good, negative delta)
	// while IPL_GETHIT_CURSE *increases* it (bad, positive delta) - the naming refers to what the
	// affix does to the "get hit" stat, not to whether it's beneficial.
	switch (affix.type) {
	case IPL_STEALLIFE:
	case IPL_STEALMANA: {
		// The row's own roll (round 66 audit): the shared printer read the item's flags, 3% first, so a Blood Craft's 3%
		// beside a Rare's own 5% printed "3%" twice. The hit takes the larger (they do not add), so the smaller row says so.
		const bool life = affix.type == IPL_STEALLIFE;
		const bool both = life ? HasAllOf(item._iFlags, ItemSpecialEffect::StealLife3 | ItemSpecialEffect::StealLife5)
		                       : HasAllOf(item._iFlags, ItemSpecialEffect::StealMana3 | ItemSpecialEffect::StealMana5);
		std::string line = life ? fmt::format(fmt::runtime(_(/*xgettext:no-c-format*/ "hit steals {:d}% life")), affix.param1)
		                        : fmt::format(fmt::runtime(_(/*xgettext:no-c-format*/ "hit steals {:d}% mana")), affix.param1);
		if (both && affix.param1 < 5)
			line += _(/*xgettext:no-c-format*/ " (the 5% applies)");
		return line;
	}
	case IPL_TOHIT_DAMP:
	case IPL_TOHIT_DAMP_CURSE:
	case IPL_DOPPELGANGER: {
		// The to-hit-and-damage affixes, per affix (tooltip sweep, 2026-09-25: seven primals printed "to hit: +80%,
		// +145% damage" twice). The shared printer reads the item's TOTALS, so this affix's row carried the other
		// to-hit affixes' share as well, and a second such affix repeated the same line. The damage is this affix's
		// own roll (param1). Its to-hit is not in the record - param2 holds the price multiplier - so it is what
		// the item's to-hit leaves once the plain to-hit affixes are taken out, shared between the affixes of this
		// kind when there is more than one.
		const int damage = affix.type == IPL_TOHIT_DAMP_CURSE ? -affix.param1 : affix.param1;
		int toHit = item._iPLToHit;
		int sharers = 0;
		for (int i = 0; i < item._iOracoolAffixCount; i++) {
			const OracoolAffix &other = item._iOracoolAffixes[i];
			if (other.type == IPL_TOHIT)
				toHit -= other.param1;
			else if (other.type == IPL_TOHIT_CURSE)
				toHit += other.param1;
			else if (IsAnyOf(other.type, IPL_TOHIT_DAMP, IPL_TOHIT_DAMP_CURSE, IPL_DOPPELGANGER))
				sharers++;
		}
		if (sharers > 1)
			toHit /= sharers;
		// Doppelganger's own effect named on its row (sweep, 2026-09-25): a perfect-rolled primal carried it beside a
		// plain to-hit-and-damage affix, and the two rows read identically. player.cpp: 10% of hits on a non-unique
		// monster other than Diablo make a copy of it.
		if (affix.type == IPL_DOPPELGANGER)
			return fmt::format(fmt::runtime(_("to hit: {:+d}%, {:+d}% damage, 10% of hits clone the foe")), toHit, damage);
		return fmt::format(fmt::runtime(_("to hit: {:+d}%, {:+d}% damage")), toHit, damage);
	}
	case IPL_TOHIT:
		return fmt::format(fmt::runtime(_("chance to hit: {:+d}%")), affix.param1);
	case IPL_TOHIT_CURSE:
		return fmt::format(fmt::runtime(_("chance to hit: {:+d}%")), -affix.param1);
	case IPL_DAMP:
		return fmt::format(fmt::runtime(_(/*xgettext:no-c-format*/ "{:+d}% damage")), affix.param1);
	case IPL_DAMP_CURSE:
		return fmt::format(fmt::runtime(_(/*xgettext:no-c-format*/ "{:+d}% damage")), -affix.param1);
	case IPL_ACP:
		return fmt::format(fmt::runtime(_(/*xgettext:no-c-format*/ "{:+d}% armor")), affix.param1);
	case IPL_ACP_CURSE:
		return fmt::format(fmt::runtime(_(/*xgettext:no-c-format*/ "{:+d}% armor")), -affix.param1);
	case IPL_FIRERES:
		return fmt::format(fmt::runtime(_("Resist Fire: {:+d}%")), affix.param1);
	case IPL_FIRERES_CURSE:
		return fmt::format(fmt::runtime(_("Resist Fire: {:+d}%")), -affix.param1);
	case IPL_LIGHTRES:
		return fmt::format(fmt::runtime(_("Resist Lightning: {:+d}%")), affix.param1);
	case IPL_LIGHTRES_CURSE:
		return fmt::format(fmt::runtime(_("Resist Lightning: {:+d}%")), -affix.param1);
	case IPL_MAGICRES:
		return fmt::format(fmt::runtime(_("Resist Magic: {:+d}%")), affix.param1);
	case IPL_MAGICRES_CURSE:
		return fmt::format(fmt::runtime(_("Resist Magic: {:+d}%")), -affix.param1);
	case IPL_COLDRES:
		return fmt::format(fmt::runtime(_("Resist Cold: {:+d}%")), affix.param1);
	case IPL_COLDRES_CURSE:
		return fmt::format(fmt::runtime(_("Resist Cold: {:+d}%")), -affix.param1);
	case IPL_ALLRES:
		// Not in PrintItemPower's own switch - falling through to it previously read the item's shared
		// _iPLFR field directly, which also accumulates any separately-rolled Fire/Light/Magic Resist
		// affix on the same item, showing their combined total on this line instead of just this roll.
		return fmt::format(fmt::runtime(_("Resist All: {:+d}%")), affix.param1);
	case IPL_STR:
		return fmt::format(fmt::runtime(_("{:+d} to strength")), affix.param1);
	case IPL_STR_CURSE:
		return fmt::format(fmt::runtime(_("{:+d} to strength")), -affix.param1);
	case IPL_MAG:
		return fmt::format(fmt::runtime(_("{:+d} to magic")), affix.param1);
	case IPL_MAG_CURSE:
		return fmt::format(fmt::runtime(_("{:+d} to magic")), -affix.param1);
	case IPL_DEX:
		return fmt::format(fmt::runtime(_("{:+d} to dexterity")), affix.param1);
	case IPL_DEX_CURSE:
		return fmt::format(fmt::runtime(_("{:+d} to dexterity")), -affix.param1);
	case IPL_VIT:
		return fmt::format(fmt::runtime(_("{:+d} to vitality")), affix.param1);
	case IPL_VIT_CURSE:
		return fmt::format(fmt::runtime(_("{:+d} to vitality")), -affix.param1);
	case IPL_ATTRIBS:
		// Same shared-field collision as IPL_ALLRES above, but for Str/Mag/Dex/Vit together.
		return fmt::format(fmt::runtime(_("{:+d} to all attributes")), affix.param1);
	case IPL_ATTRIBS_CURSE:
		return fmt::format(fmt::runtime(_("{:+d} to all attributes")), -affix.param1);
	case IPL_GETHIT:
		return fmt::format(fmt::runtime(_("{:+d} damage from enemies")), -affix.param1);
	case IPL_GETHIT_CURSE:
		return fmt::format(fmt::runtime(_("{:+d} damage from enemies")), affix.param1);
	case IPL_GOLDFIND:
		// Worded to match the Charm of Greed's line, since the two stack and a player comparing them
		// should not have to work out whether they mean the same thing.
		return fmt::format(fmt::runtime(_("{:+d}% gold from monsters")), affix.param1);
	case IPL_MOVESPEED:
		return fmt::format(fmt::runtime(_("{:+d}% movement speed")), affix.param1);
	case IPL_MOVESPEED_CURSE:
		return fmt::format(fmt::runtime(_("{:+d}% movement speed")), -affix.param1);
	case IPL_FASTCAST:
		return fmt::format(fmt::runtime(_("{:+d}% faster cast rate")), affix.param1);
	case IPL_MAGICFIND:
		// Worded to match the Charm of Luck's line, for the reason the gold one above records: the
		// two stack, and a player comparing them should not have to work out whether they mean the
		// same thing.
		return fmt::format(fmt::runtime(_("{:+d}% chance a plain weapon or armor found is Rare")), affix.param1);
	case IPL_LIFE:
		return fmt::format(fmt::runtime(_("Hit Points: {:+d}")), affix.param1);
	case IPL_LIFE_CURSE:
		return fmt::format(fmt::runtime(_("Hit Points: {:+d}")), -affix.param1);
	case IPL_MANA:
		return fmt::format(fmt::runtime(_("Mana: {:+d}")), affix.param1);
	case IPL_MANA_CURSE:
		return fmt::format(fmt::runtime(_("Mana: {:+d}")), -affix.param1);
	// Their own share, not the item's whole +damage: with Jagged or King's beside them the tooltip counted that twice
	// (round 14 audit, v1.12.239).
	case IPL_DECAY:
		return fmt::format(fmt::runtime(_(/*xgettext:no-c-format*/ "decaying {:+d}% damage")), affix.param1);
	case IPL_CRYSTALLINE:
		return fmt::format(fmt::runtime(_(/*xgettext:no-c-format*/ "low dur, {:+d}% damage")), 140 + 2 * affix.param1);
	default:
		return PrintItemPower(affix.type, item);
	}
}

/**
 * @brief One set-bonus stat, rendered from the rung's OWN values.
 *
 * Neither existing printer fits. PrintItemPower reads an item's accumulated fields and a rung has no
 * item; PrintOracoolAffixPower takes an OracoolAffix, which carries a single magnitude and so cannot
 * say "5-10 fire damage". This reads param1/param2 straight off the ItemPower.
 *
 * Terser than the item lines above on purpose: a rung's stats are joined onto ONE line under its
 * name, because the alternative - a line each - makes Leoric's thirteen-piece ladder taller than the
 * screen. "+15% fire res" rather than "Resist Fire: +15%".
 *
 * Every type reachable from a rung has a case. ItemSetsTest.EverySetBonusStatHasText walks the
 * generated table and fails on any type that lands in the default, so a newly authored stat cannot
 * silently render as a blank.
 */
std::string PrintSetBonusPower(const ItemPower &power)
{
	switch (power.type) {
	case IPL_STR:
		return fmt::format(fmt::runtime(_("{:+d} str")), power.param1);
	case IPL_MAG:
		return fmt::format(fmt::runtime(_("{:+d} mag")), power.param1);
	case IPL_DEX:
		return fmt::format(fmt::runtime(_("{:+d} dex")), power.param1);
	case IPL_VIT:
		return fmt::format(fmt::runtime(_("{:+d} vit")), power.param1);
	case IPL_ATTRIBS:
		return fmt::format(fmt::runtime(_("{:+d} all attributes")), power.param1);
	case IPL_LIFE:
		return fmt::format(fmt::runtime(_("{:+d} life")), power.param1);
	case IPL_MANA:
		return fmt::format(fmt::runtime(_("{:+d} mana")), power.param1);
	case IPL_ALLRES:
		return fmt::format(fmt::runtime(_(/*xgettext:no-c-format*/ "{:+d}% all resist")), power.param1);
	case IPL_GOLDFIND:
		// Worded to match the Charm of Greed's own line, since the two stack and a player comparing
		// them should not have to work out whether they mean the same thing.
		return fmt::format(fmt::runtime(_(/*xgettext:no-c-format*/ "{:+d}% gold from monsters")), power.param1);
	case IPL_MOVESPEED:
		return fmt::format(fmt::runtime(_(/*xgettext:no-c-format*/ "{:+d}% movement speed")), power.param1);
	case IPL_MOVESPEED_CURSE:
		return fmt::format(fmt::runtime(_(/*xgettext:no-c-format*/ "{:+d}% movement speed")), -power.param1);
	case IPL_FASTCAST:
		return fmt::format(fmt::runtime(_(/*xgettext:no-c-format*/ "{:+d}% faster cast rate")), power.param1);
	case IPL_MAGICFIND:
		return fmt::format(fmt::runtime(_(/*xgettext:no-c-format*/ "{:+d}% chance a plain weapon or armor found is Rare")), power.param1);
	case IPL_FIRERES:
		return fmt::format(fmt::runtime(_(/*xgettext:no-c-format*/ "{:+d}% fire resist")), power.param1);
	case IPL_LIGHTRES:
		return fmt::format(fmt::runtime(_(/*xgettext:no-c-format*/ "{:+d}% lightning resist")), power.param1);
	case IPL_MAGICRES:
		return fmt::format(fmt::runtime(_(/*xgettext:no-c-format*/ "{:+d}% magic resist")), power.param1);
	case IPL_COLDRES:
		return fmt::format(fmt::runtime(_(/*xgettext:no-c-format*/ "{:+d}% cold resist")), power.param1);
	case IPL_DAMP:
		return fmt::format(fmt::runtime(_(/*xgettext:no-c-format*/ "{:+d}% damage")), power.param1);
	case IPL_DAMMOD:
		return fmt::format(fmt::runtime(_("{:+d} damage")), power.param1);
	case IPL_TOHIT:
		return fmt::format(fmt::runtime(_(/*xgettext:no-c-format*/ "{:+d}% to hit")), power.param1);
	case IPL_ACP:
		// Flat on a rung, not a percentage - see the flatArmor accumulator in ApplySetBonusesToTotals
		// for why a bonus cannot express a percentage of an item it does not have.
		return fmt::format(fmt::runtime(_("{:+d} armor")), power.param1);
	case IPL_GETHIT:
		// The parameter arrives positive and SUBTRACTS, so the sign is flipped for display.
		return fmt::format(fmt::runtime(_("{:+d} damage taken")), -power.param1);
	case IPL_LIGHT:
		return fmt::format(fmt::runtime(_("{:+d} light radius")), power.param1);
	case IPL_SPLLVLADD:
		return fmt::format(fmt::runtime(_("{:+d} to all spell levels")), power.param1);
	case IPL_FIREDAM:
		return fmt::format(fmt::runtime(_("{:d}-{:d} fire damage")), power.param1, power.param2);
	case IPL_LIGHTDAM:
		return fmt::format(fmt::runtime(_("{:d}-{:d} lightning damage")), power.param1, power.param2);
	case IPL_FIRE_ARROWS:
		return fmt::format(fmt::runtime(_("{:d}-{:d} fire arrow damage")), power.param1, power.param2);
	case IPL_LIGHT_ARROWS:
		return fmt::format(fmt::runtime(_("{:d}-{:d} lightning arrow damage")), power.param1, power.param2);
	case IPL_FASTATTACK:
		// A discrete tier in param1, 1..4 - never a percentage. Named rather than numbered, because
		// "attack speed 2" means nothing to a player.
		switch (power.param1) {
		case 1: return oracool::AttackSpeedWords(_("quick attack"));
		case 2: return oracool::AttackSpeedWords(_("fast attack"));
		case 3: return oracool::AttackSpeedWords(_("faster attack"));
		default: return oracool::AttackSpeedWords(_("fastest attack"));
		}
	case IPL_FASTRECOVER:
		switch (power.param1) {
		case 1: return std::string(_("fast hit recovery"));
		case 2: return std::string(_("faster hit recovery"));
		default: return std::string(_("fastest hit recovery"));
		}
	case IPL_FASTBLOCK:
		return std::string(_("fast block"));
	case IPL_THORNS:
		return std::string(_("attacker takes damage"));
	case IPL_STEALLIFE:
		return fmt::format(fmt::runtime(_(/*xgettext:no-c-format*/ "{:d}% life stolen per melee hit")), power.param1);
	case IPL_STEALMANA:
		return fmt::format(fmt::runtime(_(/*xgettext:no-c-format*/ "{:d}% mana stolen per melee hit")), power.param1);
	case IPL_MULT_ARROWS:
		return std::string(_("fires multiple arrows"));
	case IPL_ABSHALFTRAP:
		return std::string(_("half trap damage"));
	case IPL_3XDAMVDEM:
		return std::string(_("triple damage vs demons"));
	case IPL_ACDEMON:
		return std::string(_("extra armor vs demons"));
	case IPL_ACUNDEAD:
		return std::string(_("extra armor vs undead"));
	case IPL_KNOCKBACK:
		return std::string(_("knocks target back"));
	default:
		// Deliberately empty rather than a guess. The test named above turns this into a build-time
		// failure; at runtime the joiner below simply skips it rather than printing a blank comma.
		return std::string();
	}
}

/**
 * @brief Appends the affix/power lines that used to live in the separate fixed "item stats" window.
 *
 * Oracool: user request - there is now one cursor-following panel instead of a tooltip plus a
 * static box pinned beside the inventory, so these lines join the same panel-string list every
 * other line of item detail already goes into (see oracool::DrawCursorTooltip).
 *
 * Plain and magic items are absent here on purpose: PrintItemDetails prints their affix list itself,
 * and the old box was showing a magic item's affixes a second time in a different place.
 */
void AddItemPowerPanelStrings(const Item &item)
{
	// A set piece's stats come from its own definition, not from a roll.
	//
	// Bug (fixed 2026-08-16, user report: "i dont see any affixes" on a complete set). Set is an
	// OracoolItemTier, so it fell into the branch below and printed _iOracoolAffixes - the list the affix
	// ROLLER fills. A set item is never rolled: MakeSetItem
	// applies its declared powers straight into the _iPL* fields, so those arrays are empty and the
	// description had nothing to say.
	//
	// Each power from its OWN number, through PrintOracoolAffixPower (tooltip sweep, 2026-09-25). This used to read
	// the item's accumulated fields on the grounds that a set piece has one source per stat - but a piece can
	// declare the same stat twice (the Sign of the Absent Star: +25 mana and +10 mana) and then both rows
	// printed the total, "Mana: +35" twice. Types the per-affix printer does not cover still fall back to
	// the item's field, which for a single power is the same number.
	if (item._iOracoolTier == OracoolItemTier::Set) {
		const oracool::SetItemDefinition *def = oracool::FindSetItemByCursor(item._iCurs);
		if (def == nullptr)
			return;

		// The piece's OWN stats first, in the ordinary affix blue. Green is reserved for what the
		// SET grants (user, 2026-08-16: "Regullar affixes to be in blue") - so the two kinds of
		// bonus are told apart by colour, which is the whole reason the block below is green.
		for (const ItemPower &power : def->powers) {
			if (power.type == IPL_INVALID)
				break;
			AddPanelString(PrintOracoolAffixPower(OracoolAffix { power.type, power.param1, 0 }, item), ItemAffixColor);
		}

		const oracool::ItemSetDefinition *set = oracool::FindItemSetOwning(def->id);
		if (set == nullptr)
			return;
		const int worn = oracool::WornSetPieces(*MyPlayer, *set);
		AddPanelString(fmt::format(fmt::runtime(_("{:s} ({:d}/{:d})")), _(set->name), worn, set->itemCount),
		    UiFlags::ColorOracoolGreen);

		// Every piece of the set - worn ones green, owned-but-not-worn ones yellow (2026-09-13), missing
		// ones red - each followed by its slot in white brackets. The white tail is a two-run line - see
		// AddPanelStringSplit and the tail handling in oracool::DrawCursorTooltip; one colour per line
		// could not say this.
		for (int i = 0; i < set->itemCount; i++) {
			const oracool::SetItemDefinition &piece = oracool::ItemSetItems[set->firstItem + i];
			std::string name = StrCat("  ", _(piece.name));
			// The offset is taken BEFORE the bracket is appended, so it is the byte the white run
			// starts at whatever the translated name's length turns out to be.
			const size_t tailStart = name.size();
			name = StrCat(name, " (", _(oracool::SetSlotDisplayName(piece.slot)), ")");
			AddPanelStringSplit(std::move(name), oracool::SetPieceListColor(*MyPlayer, piece), tailStart);
		}

		// Then the ladder, in rung order, each labelled with the pieces it needs. Green once earned,
		// red until then - so the panel shows the whole progression rather than only what is already
		// in hand. Read straight off the set's rungs rather than through ForEachEarnedSetBonus,
		// because this list wants the UNEARNED ones too.
		for (int i = 0; i < set->bonusCount; i++) {
			const oracool::SetBonusDefinition &rung = oracool::ItemSetBonuses[set->firstBonus + i];
			const UiFlags rungColor = rung.pieces <= worn ? UiFlags::ColorOracoolGreen : UiFlags::ColorRed;

			// What the tier GRANTS, and only that. Its NAME - "Cinderbrand", "Take Root" - is not
			// shown (user, 2026-08-16: "they just take extra rows and nobody cares abot them. we,
			// players, care about the buff not the fancy name for it").
			//
			// The names stay in the DATA: the override table is organised by them, the tests name
			// them when they fail, and they are the record of what each tier was designed to be.
			// They are simply not what a player reading a tooltip needs.
			//
			// One line per tier, stats joined - which also halves the block, so Leoric's thirteen-
			// piece ladder now fits on screen rather than merely nearly fitting.
			std::string granted;
			for (const ItemPower &power : rung.powers) {
				if (power.type == IPL_INVALID)
					continue;
				std::string text = PrintSetBonusPower(power);
				if (text.empty())
					continue; // no rendering for this type; see PrintSetBonusPower's default
				if (!granted.empty())
					granted = StrCat(granted, ", ");
				granted = StrCat(granted, text);
			}
			// Unreachable with the current data - the generator refuses an empty tier and
			// EverySetBonusStatHasText refuses an unrenderable one - but a bare "  (4)" with nothing
			// after it would be the symptom, so fall back to the name rather than to nothing.
			if (granted.empty())
				granted = _(rung.name);
			AddPanelString(fmt::format(fmt::runtime(_("  ({:d}) {:s}")), rung.pieces, granted), rungColor);
		}
		return;
	}

	if (item.hasOracoolTier()) {
		// Rare/Buffed Unique/Primal items: unlike a static UniqueItem, the affix list comes from
		// the item instance itself (up to six affixes from any table, in one list), so the vanilla
		// UniqueItems[uid].powers[] table isn't involved at all here.
		for (int i = 0; i < item._iOracoolAffixCount; i++)
			AddPanelString(PrintOracoolAffixPower(item._iOracoolAffixes[i], item), ItemAffixColor);
		return;
	}

	const UniqueItem &uitem = UniqueItems[item._iUid];
	assert(uitem.UINumPL <= sizeof(uitem.powers) / sizeof(*uitem.powers));
	for (const auto &power : uitem.powers) {
		if (power.type == IPL_INVALID)
			break;
		// The icon assignment is not a stat. PrintItemPower renders it as a lone space - vanilla's
		// way of keeping its few INVCURS-carrying uniques readable - which was tolerable on a
		// handful of items and became a phantom blank line at the END of all 143 expansion uniques'
		// tooltips once every one of them carried its icon this way (audit, 2026-08-17).
		if (power.type == IPL_INVCURS)
			continue;
		// A fixed-value stat whose field another of the row's powers also writes prints its OWN value, as set pieces do:
		// PrintItemPower reads the accumulated field, so The Quiet Sun (all attributes 5, strength 11) read "+16 to all
		// attributes" and its all-resist line carried the fire resist too - about 55 expansion uniques (round 8 audit,
		// v1.12.233). Ranged rows (Hammer of Jholm's damage) keep PrintItemPower.
		if (power.param1 == power.param2
		    && IsAnyOf(power.type, IPL_ATTRIBS, IPL_ATTRIBS_CURSE, IPL_STR, IPL_STR_CURSE, IPL_MAG, IPL_MAG_CURSE, IPL_DEX,
		        IPL_DEX_CURSE, IPL_VIT, IPL_VIT_CURSE, IPL_ALLRES, IPL_FIRERES, IPL_LIGHTRES, IPL_MAGICRES, IPL_COLDRES)) {
			AddPanelString(PrintOracoolAffixPower(OracoolAffix { power.type, power.param1, 0 }, item), ItemAffixColor);
			continue;
		}
		// A negative durability percent SHORTENS the item's life, and a base with no durability (an amulet) has none to
		// change: "high durability" was wrong on both (round 8 audit).
		if (power.type == IPL_DUR && power.param1 < 0) {
			if (item._iMaxDur != 0)
				AddPanelString(_("decreased durability"), ItemAffixColor);
			continue;
		}
		AddPanelString(PrintItemPower(power.type, item), ItemAffixColor);
	}
}

/**
 * @brief Whether one of the affix lines below will already say "Indestructible".
 *
 * Oracool: user request - the base stat line stops repeating it when an affix states it. An
 * indestructible item has no durability to print in that slot either, so the line becomes just the
 * damage or armour and the blue affix line below carries the fact - which also says something the
 * merged line did not: that indestructibility was rolled rather than inherent.
 *
 * This deliberately mirrors what PrintItemDetails actually prints rather than asking the item
 * whether it has the property anywhere: a unique's powers are only listed for uniques, a set piece's
 * from its definition, and the affix list for every other item, so an item can be indestructible with
 * nothing below to say so. Those keep the word on the base line, which is the only place it would appear.
 */
bool AffixStatesIndestructible(const Item &item)
{
	// A Zod's socket line says it too (round 35 audit: "Indestructible" twice).
	if (oracool::SocketsMakeIndestructible(item))
		return true;
	// The one affix list is printed for every item that is neither a set piece nor a vanilla unique (2026-09-25:
	// it used to be tiered items' list plus a magic item's vanilla prefix/suffix pair).
	const bool listPrinted = item._iOracoolTier != OracoolItemTier::Set
	    && (item.hasOracoolTier() || item._iMagical != ITEM_QUALITY_UNIQUE);
	if (listPrinted) {
		for (int i = 0; i < item._iOracoolAffixCount; i++) {
			if (item._iOracoolAffixes[i].type == IPL_INDESTRUCTIBLE)
				return true;
		}
	} else if (item._iMagical == ITEM_QUALITY_UNIQUE && !item.hasOracoolTier()) {
		for (const auto &power : UniqueItems[item._iUid].powers) {
			if (power.type == IPL_INVALID)
				break;
			if (power.type == IPL_INDESTRUCTIBLE)
				return true;
		}
	}
	return false;
}

/**
 * The staff charges line names the spell too: the spell left the staff's name, so "Charges: x/y" alone no
 * longer said what the charges cast (user, 2026-10-02). Shared by the identified and unidentified views.
 */
static std::string StaffChargesLine(const Item &item)
{
	if (!IsValidSpell(item._iSpell))
		return fmt::format(fmt::runtime(_("Charges: {:d}/{:d}")), item._iCharges, item._iMaxCharges);
	return fmt::format(fmt::runtime(_("Spell: {:s}  Charges: {:d}/{:d}")),
	    pgettext("spell", GetSpellData(item._iSpell).sNameText), item._iCharges, item._iMaxCharges);
}

void PrintItemDetails(const Item &item)
{
	if (HeadlessMode)
		return;

	// The title line, for a completed runeword.
	//
	// User, 2026-08-20: "The gold name Runeword SPIRIT - move it to the top of description,
	// replacing the current white title Spirit." The caller has already set the title from
	// item.getName(); SetPanelString replaces it outright, and at this point the panel holds
	// nothing but that one line, so this cannot clobber anything else.
	//
	// Done HERE rather than in getName() on purpose: the name is persisted in _iIName and also
	// feeds the ground label and the cursor, where "Runeword: Spirit" would be a mouthful. This is
	// a framing of the name for one panel, not a change to what the item is called.
	if (const oracool::RunewordDefinition *word = oracool::GetActiveRuneword(item); word != nullptr)
		SetPanelString(fmt::format(fmt::runtime(_("Runeword: {:s}")), _(word->name)), UiFlags::ColorWhitegold);

	// Zod's stamp too: the host read "Dur: 255/60" (round 11 audit, v1.12.236).
	const bool indestructible = item._iMaxDur == DUR_INDESTRUCTIBLE || item._iDurability == DUR_INDESTRUCTIBLE;
	// Only suppressed when something below will actually print the word - see the helper.
	const bool affixSaysIndestructible = indestructible && AffixStatesIndestructible(item);

	// Oracool: user request (2026-08-16) - the tier line sits directly BELOW THE NAME and above the
	// damage stats, for every quality: "basic item" and "magic item" say so out loud now, not just by
	// the absence of colour, and the tiered labels moved up here from below the affix block. Each
	// line wears the name's own colour, so the word and the colour teach each other. Equipment only
	// (anything with a worn slot) - a potion calling itself a basic item would be noise, not
	// information.
	if (item._iLoc != ILOC_NONE && item._iLoc != ILOC_UNEQUIPABLE && item._iLoc != ILOC_BELT) {
		// The quality, then WHAT IT IS (user, 2026-08-30: "replace the word ITEM with the ACTUAL
		// TYPE OF ITEM"). The tiered labels used to carry the noun themselves - "rare item" - so
		// the quality word is taken from them and the noun supplied here, which is what stops the
		// two halves being written in two places.
		const string_view noun = GetItemTypeNoun(item);
		string_view quality;
		if (item.hasOracoolTier())
			quality = GetOracoolTierQualityWord(item._iOracoolTier);
		else if (item._iMagical == ITEM_QUALITY_UNIQUE)
			quality = _("unique");
		else if (item._iMagical == ITEM_QUALITY_MAGIC)
			quality = _("magic");
		else
			quality = _("basic");
		AddPanelString(fmt::format(fmt::runtime(_("{0} {1}")), quality, noun), item.getTextColor());
	}

	// The BASE TIER, in its own colour (user, 2026-08-19: white / blue / yellow / gold). Above the
	// ilvl line because it names what the item IS; the ilvl only says where it was found. Normal is
	// printed too rather than left blank - "Normal" is information once three other answers exist.
	if (oracool::CanCarryBaseTier(item)) {
		const auto baseTier = static_cast<oracool::BaseItemTier>(item._iOracoolBaseTier);
		AddPanelString(fmt::format(fmt::runtime(_("Tier: {:s}")), _(oracool::TierName(baseTier))),
		    oracool::TierColor(baseTier));
	}

	// ilvl, directly under the quality line (user, 2026-08-19: "items to have it in their
	// description"). Zero means the item predates the ilvl byte or was built by a path that stamps
	// none - a blank line is better than an invented number.
	if (item._iOracoolItemLevel > 0)
		AddPanelString(fmt::format(fmt::runtime(_("Item Level: {:d}")), item._iOracoolItemLevel), ItemBaseStatColor);

	// Oracool: colours per ItemBaseStatColor / ItemAffixColor - base stats white, rolls blue, the
	// tier label with the name's own colour.
	if (item._iClass == ICLASS_WEAPON) {
		if (item._iMinDam == item._iMaxDam) {
			if (!indestructible)
				AddPanelString(fmt::format(fmt::runtime(_(/* TRANSLATORS: Dur: is durability */ "damage: {:d}  Dur: {:d}/{:d}")), item._iMinDam, item._iDurability, item._iMaxDur), ItemBaseStatColor);
			else if (affixSaysIndestructible)
				AddPanelString(fmt::format(fmt::runtime(_("damage: {:d}")), item._iMinDam), ItemBaseStatColor);
			else
				AddPanelString(fmt::format(fmt::runtime(_("damage: {:d}  Indestructible")), item._iMinDam), ItemBaseStatColor);
		} else {
			if (!indestructible)
				AddPanelString(fmt::format(fmt::runtime(_(/* TRANSLATORS: Dur: is durability */ "damage: {:d}-{:d}  Dur: {:d}/{:d}")), item._iMinDam, item._iMaxDam, item._iDurability, item._iMaxDur), ItemBaseStatColor);
			else if (affixSaysIndestructible)
				AddPanelString(fmt::format(fmt::runtime(_("damage: {:d}-{:d}")), item._iMinDam, item._iMaxDam), ItemBaseStatColor);
			else
				AddPanelString(fmt::format(fmt::runtime(_("damage: {:d}-{:d}  Indestructible")), item._iMinDam, item._iMaxDam), ItemBaseStatColor);
		}
	}
	if (item._iClass == ICLASS_ARMOR) {
		if (!indestructible)
			AddPanelString(fmt::format(fmt::runtime(_(/* TRANSLATORS: Dur: is durability */ "armor: {:d}  Dur: {:d}/{:d}")), item._iAC, item._iDurability, item._iMaxDur), ItemBaseStatColor);
		else if (affixSaysIndestructible)
			AddPanelString(fmt::format(fmt::runtime(_("armor: {:d}")), item._iAC), ItemBaseStatColor);
		else
			AddPanelString(fmt::format(fmt::runtime(_("armor: {:d}  Indestructible")), item._iAC), ItemBaseStatColor);
	}
	if (item._iMiscId == IMISC_STAFF && item._iMaxCharges != 0) {
		AddPanelString(StaffChargesLine(item), ItemBaseStatColor);
	}
	// The tier label used to print here, between the affixes and the power list; it now leads the
	// panel instead (user request, 2026-08-16 - "just below their name and above the dmg stats").
	if (item.hasOracoolTier() || item._iMagical == ITEM_QUALITY_UNIQUE) {
		AddItemPowerPanelStrings(item);
	} else {
		// EVERY OTHER ITEM PRINTS ITS ONE AFFIX LIST - magic, crafted, a staff's affix - one line per affix
		// from its own rolled value (user, 2026-09-25: "all afixes are now one pool").
		//
		// A magic item used to print two vanilla prefix/suffix lines from the item's totals and this list only
		// when those two were empty - the fix for "when i rerolled a weapon i stopped seeing its affixes on its
		// pop-up display" (2026-09-22), because the Mystic's rebuild left the pair empty. With the pair gone
		// the list is the only store, so it always prints and nothing can print it twice.
		for (int i = 0; i < item._iOracoolAffixCount; i++)
			AddPanelString(PrintOracoolAffixPower(item._iOracoolAffixes[i], item), ItemAffixColor);
	}
	// Phase 1 ethereal: the whole bargain in one line, directly under the tier - the buffed stats
	// already show in the numbers above, so what the line carries is the PRICE.
	if (item._iOracoolEthereal)
		AddPanelString(_("Ethereal (no smith repairs it; the Cube's Mend does)"), UiFlags::ColorGray7); // GR-7, the ethereal colour (2026-09-07); the Cube mends it (round 27 audit)
	// A broken item gives nothing, and only the X on its icon said so (round 27 audit).
	if (item._iOracoolBroken)
		AddPanelString(item._iOracoolEthereal ? _("Broken - gives nothing until mended") : _("Broken - gives nothing until repaired"), UiFlags::ColorRed);
	// Movement Speed +X% from the item's own record (2026-09-07; an ordinary pool affix since 2026-09-13). A tiered item prints its records
	// with the other affixes above, so this line is the plain and magic items'.
	//
	// NOT when a line above already said it (tooltip audit, 2026-09-25). The branch above prints the affix
	// list of every plain and magic item, which is where these two pool affixes live, and a unique's power
	// list can carry movement speed. Each of those printed the stat and then this printed it again. What is
	// left for these rows is a stat the item has with no line of its own.
	const auto alreadyPrinted = [&item](item_effect_type a, item_effect_type b) {
		const auto is = [a, b](item_effect_type t) { return t == a || t == b; };
		if (item._iMagical == ITEM_QUALITY_UNIQUE && !item.hasOracoolTier() && item._iUid >= 0) {
			for (const ItemPower &power : UniqueItems[item._iUid].powers) {
				if (is(power.type))
					return true;
			}
		}
		if (!item.hasOracoolTier() && item._iMagical != ITEM_QUALITY_UNIQUE) {
			for (int i = 0; i < item._iOracoolAffixCount; i++) {
				if (is(item._iOracoolAffixes[i].type))
					return true;
			}
		}
		return false;
	};
	if (item._iIdentified && item._iPLMoveSpeed != 0 && !item.hasOracoolTier() && !alreadyPrinted(IPL_MOVESPEED, IPL_MOVESPEED_CURSE))
		AddPanelString(fmt::format(fmt::runtime(_("{:+d}% movement speed")), item._iPLMoveSpeed), ItemAffixColor);
	// Faster Cast Rate the same way (2026-09-11). Not on a unique, which prints it on its own power line.
	if (item._iIdentified && item._iPLFastCast != 0 && !item.hasOracoolTier() && item._iMagical != ITEM_QUALITY_UNIQUE
	    && !alreadyPrinted(IPL_FASTCAST, IPL_FASTCAST))
		AddPanelString(fmt::format(fmt::runtime(_("{:+d}% faster cast rate")), item._iPLFastCast), ItemAffixColor);
	// Imbuement Shards: how many this item has taken and how many it can, then WHICH. The player
	// needs to know what is left BEFORE they spend one, because a shard cannot be taken out short of
	// the Cleanse recipe - an item silently at its cap is exactly what the first line exists to
	// prevent, and the second is the ledger itself, the thing the orbs could never show.
	if (const std::string imbueLine = oracool::ImbueCountLine(item); !imbueLine.empty())
		AddPanelString(imbueLine, ItemBaseStatColor);
	if (const std::string kinds = oracool::ImbueBreakdownLine(item); !kinds.empty())
		AddPanelString(kinds, ItemAffixColor);
	// The SHARD's own line, when the thing being described is the shard rather than its target.
	if (IsOracoolShardIdx(item.IDidx)) {
		AddPanelString(_(oracool::ShardLine(item.IDidx)), ItemAffixColor);
		AddPanelString(_("drop onto a backpack item to imbue it"), ItemBaseStatColor);
	}
	// Phase 1 charms: the effect, and the rule that governs it - the description is where the
	// active-cap system explains itself.
	if (IsOracoolCharmIdx(item.IDidx)) {
		AddPanelString(oracool::CharmEffectLine(*MyPlayer, static_cast<uint16_t>(item.IDidx)), ItemAffixColor);
		AddPanelString(fmt::format(fmt::runtime(_("only your first {:d} charms are active")), oracool::CharmActiveCap), ItemBaseStatColor);
		// And whether THIS one is: the cap counts in pickup order, which the grid does not show (round 27 audit).
		if (const int state = oracool::CharmActiveState(*MyPlayer, item); state == 1)
			AddPanelString(_("Active"), ItemAffixColor);
		else if (state == 0)
			AddPanelString(fmt::format(fmt::runtime(_("Inactive - over the {:d}-charm cap")), oracool::CharmActiveCap), UiFlags::ColorRed);
		else
			AddPanelString(_("Inactive - only backpack charms work"), UiFlags::ColorRed); // the stash, the Cube (round 35 audit)
	}
	// A LOOSE gem or rune says what it does, per host, before it says anything else.
	//
	// User, 2026-08-20: "why runes show available RW instead of their affixes/stats?" - and the
	// answer was that they never showed them. The teaching lines below were the only rune branch in
	// this function, and a loose GEM had no branch at all, so the one thing you actually need to
	// decide whether to socket a stone was the one thing not printed. The effects were always there
	// (runes are GemData rows) and always rendered - but only from the socket loop further down,
	// which is to say only after the decision had been made.
	//
	// Three hosts because a loose stone does not know where it is going. Empty lists fall away, so
	// a gem that does nothing in shields simply says nothing about shields.
	// JEWELS are in this gate too (user, 2026-09-12: "jelews have no description in their pop-up.
	// add appropriate description"). They were the one socketable family with no branch anywhere in
	// this function: gems and runes had this one, Mystic Orbs have their own above, charms have
	// CharmEffectLine - and a jewel had nothing, so its panel showed a name, a qlvl and no reason to
	// pick it up.
	//
	// Nothing had to be written to describe them. The fifteen jewels are ordinary Gems[] rows with
	// real per-host numbers (jewels_effects.inc), and ResolveGem falls through to FindGemRow for an
	// index that is not a gem type - so GemHostEffectLine has always produced the right three lines
	// for a jewel. It was simply never asked. The canonical socketable set in items.h:595 already
	// listed all four families; this gate was the one place that had drifted from it.
	if (IsOracoolGemIdx(item.IDidx) || IsOracoolRuneIdx(item.IDidx) || IsOracoolJewelIdx(item.IDidx)) {
		// The level it imposes, before it is socketed (round 27 audit): a Zod made a level-20 hero's sword need 69 and
		// nothing said so beforehand.
		if (const int level = oracool::SocketedStoneLevel(static_cast<uint16_t>(item.IDidx)); level > 1) {
			const bool above = MyPlayer != nullptr && MyPlayer->_pLevel < level;
			AddPanelString(fmt::format(fmt::runtime(_("Its item will require level {:d}")), level), above ? UiFlags::ColorRed : ItemBaseStatColor);
		}
		for (const oracool::SocketHost host : { oracool::SocketHost::Weapon, oracool::SocketHost::Shield, oracool::SocketHost::Armor }) {
			std::string line = oracool::GemHostEffectLine(static_cast<uint16_t>(item.IDidx), host);
			if (!line.empty())
				AddPanelString(std::move(line), ItemAffixColor);
		}
	}
	// A rune's panel no longer lists the runewords it belongs to (dev note, 2026-09-27: "remove the possible recipes
	// from the description of runes"). The runeword book holds the recipes.
	// A completed runeword's OWN bonuses - the ones the word grants on top of its runes.
	//
	// User, 2026-08-20: "I don't see the extra affixes the runeword should bring." They were
	// applied (ApplySockets calls ApplyRunewordToTotals for every worn item) and never printed, so
	// the panel listed the four runes' individual effects and stopped - and the word itself read as
	// a name with nothing behind it.
	//
	// Formatted from the definition's own fields rather than from a second table, so a word whose
	// numbers are retuned cannot end up describing its old ones.
	// One describer for the panel and the runeword book (RunewordBonusLines), since 2026-09-05
	// when the words grew their second half - flags, attributes, finds, reduction, light.
	if (const oracool::RunewordDefinition *word = oracool::GetActiveRuneword(item); word != nullptr) {
		for (const std::string &line : oracool::RunewordBonusLines(*word))
			AddPanelString(line, ItemAffixColor);
	}
	// Phase 1 sockets: the socket line and one line per set gem, each in the gem economy's own
	// voice. The empty-socket count is the item's pitch - "Sockets: 1/3" is an invitation.
	if (item._iSocketCount > 0) {
		// GR-5 (user, 2026-09-07: "also use this color for the specs row Sockets X in its description").
		AddPanelString(fmt::format(fmt::runtime(_("Sockets: {:d}/{:d}")), item.socketedCount(), item._iSocketCount), UiFlags::ColorGray5);
		const oracool::SocketHost host = oracool::SocketHostForItemType(item._itype);
		for (const uint16_t gemIdx : item._iSocketed) {
			if (gemIdx != Item::EmptySocket)
				AddPanelString(oracool::GemSocketLine(gemIdx, host), ItemAffixColor);
		}
	}
	PrintItemInfo(item);
}

void PrintItemDur(const Item &item)
{
	if (HeadlessMode)
		return;

	const bool indestructible = item._iMaxDur == DUR_INDESTRUCTIBLE || item._iDurability == DUR_INDESTRUCTIBLE; // Zod's stamp too
	// Oracool: the unidentified view shows only base stats, so it is white throughout. "Not
	// Identified" takes the affix colour because it stands in for the affix lines that are being
	// withheld - it is a statement about the rolls, not about the base item.
	if (item._iClass == ICLASS_WEAPON) {
		if (item._iMinDam == item._iMaxDam) {
			if (indestructible)
				AddPanelString(fmt::format(fmt::runtime(_("damage: {:d}  Indestructible")), item._iMinDam), ItemBaseStatColor);
			else
				AddPanelString(fmt::format(fmt::runtime(_("damage: {:d}  Dur: {:d}/{:d}")), item._iMinDam, item._iDurability, item._iMaxDur), ItemBaseStatColor);
		} else {
			if (indestructible)
				AddPanelString(fmt::format(fmt::runtime(_("damage: {:d}-{:d}  Indestructible")), item._iMinDam, item._iMaxDam), ItemBaseStatColor);
			else
				AddPanelString(fmt::format(fmt::runtime(_("damage: {:d}-{:d}  Dur: {:d}/{:d}")), item._iMinDam, item._iMaxDam, item._iDurability, item._iMaxDur), ItemBaseStatColor);
		}
		if (item._iMiscId == IMISC_STAFF && item._iMaxCharges > 0) {
			AddPanelString(StaffChargesLine(item), ItemBaseStatColor);
		}
		if (item._iMagical != ITEM_QUALITY_NORMAL)
			AddPanelString(_("Not Identified"), ItemAffixColor);
	}
	if (item._iClass == ICLASS_ARMOR) {
		if (indestructible)
			AddPanelString(fmt::format(fmt::runtime(_("armor: {:d}  Indestructible")), item._iAC), ItemBaseStatColor);
		else
			AddPanelString(fmt::format(fmt::runtime(_("armor: {:d}  Dur: {:d}/{:d}")), item._iAC, item._iDurability, item._iMaxDur), ItemBaseStatColor);
		if (item._iMagical != ITEM_QUALITY_NORMAL)
			AddPanelString(_("Not Identified"), ItemAffixColor);
		if (item._iMiscId == IMISC_STAFF && item._iMaxCharges > 0) {
			AddPanelString(StaffChargesLine(item), ItemBaseStatColor);
		}
	}
	// Blue on jewellery too (tooltip audit, 2026-09-25): the weapon and armour lines above are the affix blue, and
	// this one was the default white - one message in two colours depending on the slot.
	if (IsAnyOf(item._itype, ItemType::Ring, ItemType::Amulet))
		AddPanelString(_("Not Identified"), ItemAffixColor);
	// What is not a roll shows unidentified too (round 66 audit): its stones, shards, ethereal state and breakage all apply
	// to an unidentified item, and the view printed none of them.
	if (item._iOracoolEthereal)
		AddPanelString(_("Ethereal (no smith repairs it; the Cube's Mend does)"), UiFlags::ColorGray7);
	if (item._iOracoolBroken)
		AddPanelString(item._iOracoolEthereal ? _("Broken - gives nothing until mended") : _("Broken - gives nothing until repaired"), UiFlags::ColorRed);
	if (const std::string imbueLine = oracool::ImbueCountLine(item); !imbueLine.empty())
		AddPanelString(imbueLine, ItemBaseStatColor);
	if (const std::string kinds = oracool::ImbueBreakdownLine(item); !kinds.empty())
		AddPanelString(kinds, ItemAffixColor);
	if (item._iSocketCount > 0) {
		AddPanelString(fmt::format(fmt::runtime(_("Sockets: {:d}/{:d}")), item.socketedCount(), item._iSocketCount), UiFlags::ColorGray5);
		const oracool::SocketHost host = oracool::SocketHostForItemType(item._itype);
		for (const uint16_t gemIdx : item._iSocketed) {
			if (gemIdx != Item::EmptySocket)
				AddPanelString(oracool::GemSocketLine(gemIdx, host), ItemAffixColor);
		}
	}
	PrintItemInfo(item);
}

void UseItem(size_t pnum, item_misc_id mid, SpellID spellID, int spellFrom)
{
	Player &player = Players[pnum];
	std::optional<SpellID> prepareSpellID;

	switch (mid) {
	case IMISC_HEAL:
		// Oracool: Gradual Healing drips this potion's usual random amount over a few seconds
		// instead of granting it all at once - see oracool/gradual_healing.h. Full Healing
		// Potions (IMISC_FULLHEAL below) stay instant either way, matching how Diablo 2 keeps
		// its Rejuvenation potions instant while throttling its regular Healing Potions.
		if (&player == MyPlayer && oracool::IsGradualHealingEnabled()) {
			oracool::QueueGradualHeal(player.CalcPartialLifeRestoreAmount());
		} else {
			player.RestorePartialLife();
			if (&player == MyPlayer) {
				RedrawComponent(PanelDrawComponent::Health);
			}
		}
		break;
	case IMISC_FULLHEAL:
		player.RestoreFullLife();
		if (&player == MyPlayer) {
			RedrawComponent(PanelDrawComponent::Health);
		}
		break;
	case IMISC_MANA:
		if (&player == MyPlayer && oracool::IsGradualHealingEnabled()) {
			oracool::QueueGradualMana(player.CalcPartialManaRestoreAmount());
		} else {
			player.RestorePartialMana();
			if (&player == MyPlayer) {
				RedrawComponent(PanelDrawComponent::Mana);
			}
		}
		break;
	case IMISC_FULLMANA:
		player.RestoreFullMana();
		if (&player == MyPlayer) {
			RedrawComponent(PanelDrawComponent::Mana);
		}
		break;
	case IMISC_ELIXSTR:
		ModifyPlrStr(player, 1);
		break;
	case IMISC_ELIXMAG:
		ModifyPlrMag(player, 1);
		if (gbIsHellfire) {
			player.RestoreFullMana();
			if (&player == MyPlayer) {
				RedrawComponent(PanelDrawComponent::Mana);
			}
		}
		break;
	case IMISC_ELIXDEX:
		ModifyPlrDex(player, 1);
		break;
	case IMISC_ELIXVIT:
		ModifyPlrVit(player, 1);
		if (gbIsHellfire) {
			player.RestoreFullLife();
			if (&player == MyPlayer) {
				RedrawComponent(PanelDrawComponent::Health);
			}
		}
		break;
	case IMISC_REJUV: {
		player.RestorePartialLife();
		player.RestorePartialMana();
		if (&player == MyPlayer) {
			RedrawComponent(PanelDrawComponent::Health);
			RedrawComponent(PanelDrawComponent::Mana);
		}
	} break;
	case IMISC_FULLREJUV:
	case IMISC_ARENAPOT:
		player.RestoreFullLife();
		player.RestoreFullMana();
		if (&player == MyPlayer) {
			RedrawComponent(PanelDrawComponent::Health);
			RedrawComponent(PanelDrawComponent::Mana);
		}
		break;
	case IMISC_ORACOOL_MAP:
	case IMISC_ORACOOL_KEYSTONE:
		// Handled in UseInvItem, not here. UseItem is given the MISC ID and not the item, and every
		// Sealed Map shares one misc id - so this function cannot tell which encounter to open.
		// UseInvItem has the item itself, which is also where the signet's cap gate lives and for a
		// related reason: both need to refuse before the item is consumed. The keystone's tier is on
		// the item too.
		break;
	case IMISC_ORACOOL_SIGNET:
		// The refusal is the interesting half. A signet used at the lifetime cap must NOT be
		// consumed - it is a capped, permanent resource, and silently eating one because the pool
		// was full is the single worst thing this item could do. CanUseItem below is what actually
		// stops the consumption; this branch only reports.
		if (oracool::ConsumeSignet(player)) {
			if (&player == MyPlayer) {
				oracool::LogEvent(StrCat("Signet of Learning: a permanent stat point (",
				    StrCat(oracool::SignetsUsed(player)), " of ",
				    StrCat(oracool::SignetLifetimeCap), " used)"));
				RedrawComponent(PanelDrawComponent::Health);
			}
		}
		break;
	case IMISC_SCROLL:
	case IMISC_SCROLLT:
		if (ControlMode == ControlTypes::KeyboardAndMouse && GetSpellData(spellID).isTargeted()) {
			prepareSpellID = spellID;
		} else {
			const int spellLevel = player.GetSpellLevel(spellID);
			// Find a valid target for the spell because tile coords
			// will be validated when processing the network message
			Point target = cursPosition;
			if (!InDungeonBounds(target))
				target = player.position.future + Displacement(player._pdir);
			// Use CMD_SPELLXY because it's the same behavior as normal casting
			assert(IsValidSpellFrom(spellFrom));
			NetSendCmdLocParam3(true, CMD_SPELLXY, target, static_cast<int16_t>(spellID), static_cast<uint8_t>(SpellType::Scroll), static_cast<uint16_t>(spellFrom));
		}
		break;
	case IMISC_BOOK: {
		uint8_t newSpellLevel = player._pSplLvl[static_cast<int16_t>(spellID)] + 1;
		// The level band and the Rule of Rangs, asked as one question (user, 2026-08-19: "apply lvl
		// req rule to books as well"). A book that would raise the spell past what the reader's level
		// allows is refused outright - it is not consumed, no mana is granted, nothing happens.
		//
		// The same answer updateRequiredStatsCacheForPlayer gives, so a book the inventory draws as
		// unusable is a book this path also refuses. One rule, asked in two places.
		if (!oracool::CanReadSpellBookTo(player, spellID, newSpellLevel))
			return; // the caller refuses first; this is the backstop, and it consumes nothing
		if (newSpellLevel <= MaxSpellLevel) {
			player._pSplLvl[static_cast<int16_t>(spellID)] = newSpellLevel;
			NetSendCmdParam2(true, CMD_CHANGE_SPELL_LEVEL, static_cast<uint16_t>(spellID), newSpellLevel);
			// SAYS WHAT WAS LEARNED. A read used to be a page-turn sound and nothing else, so a
			// player who had just spent a thousand gold could not tell whether anything had
			// happened - "i am not sure i upgraded a single or let alone three levels" (user,
			// 2026-09-03). One line, naming the spell and the level it now stands at.
			if (&player == MyPlayer) {
				EventPlrMsg(newSpellLevel == 1
				        ? fmt::format(fmt::runtime(_("You have learned {:s}.")),
				            pgettext("spell", GetSpellData(spellID).sNameText))
				        : fmt::format(fmt::runtime(_("{:s} is now level {:d}.")),
				            pgettext("spell", GetSpellData(spellID).sNameText), newSpellLevel),
				    UiFlags::ColorWhitegold);
			}
		}
		if (HasNoneOf(player._pIFlags, ItemSpecialEffect::NoMana)) {
			player._pMana += GetSpellData(spellID).sManaCost << 6;
			player._pMana = std::min(player._pMana, player._pMaxMana);
			player._pManaBase += GetSpellData(spellID).sManaCost << 6;
			player._pManaBase = std::min(player._pManaBase, player._pMaxManaBase);
		}
		if (&player == MyPlayer) {
			for (Item &item : InventoryPlayerItemsRange { player }) {
				item.updateRequiredStatsCacheForPlayer(player);
			}
			if (IsStashOpen) {
				Stash.RefreshItemStatFlags();
			}
			oracool::ScheduleAutoSaveForBookRead();
		}
		RedrawComponent(PanelDrawComponent::Mana);
	} break;
	case IMISC_MAPOFDOOM:
		doom_init();
		break;
	case IMISC_OILACC:
	case IMISC_OILMAST:
	case IMISC_OILSHARP:
	case IMISC_OILDEATH:
	case IMISC_OILSKILL:
	case IMISC_OILBSMTH:
	case IMISC_OILFORT:
	case IMISC_OILPERM:
	case IMISC_OILHARD:
	case IMISC_OILIMP:
		player._pOilType = mid;
		if (&player != MyPlayer) {
			return;
		}
		if (sbookflag) {
			sbookflag = false;
		}
		if (!invflag) {
			invflag = true;
		}
		NewCursor(CURSOR_OIL);
		break;
	case IMISC_SPECELIX:
		ModifyPlrStr(player, 3);
		ModifyPlrMag(player, 3);
		ModifyPlrDex(player, 3);
		ModifyPlrVit(player, 3);
		break;
	case IMISC_RUNEF:
		prepareSpellID = SpellID::RuneOfFire;
		break;
	case IMISC_RUNEL:
		prepareSpellID = SpellID::RuneOfLight;
		break;
	case IMISC_GR_RUNEL:
		prepareSpellID = SpellID::RuneOfNova;
		break;
	case IMISC_GR_RUNEF:
		prepareSpellID = SpellID::RuneOfImmolation;
		break;
	case IMISC_RUNES:
		prepareSpellID = SpellID::RuneOfStone;
		break;
	default:
		break;
	}

	if (prepareSpellID) {
		assert(IsValidSpellFrom(spellFrom));
		player.inventorySpell = *prepareSpellID;
		player.spellFrom = spellFrom;
		if (&player == MyPlayer)
			NewCursor(CURSOR_TELEPORT);
	}
}

bool UseItemOpensHive(const Item &item, Point position)
{
	if (item.IDidx != IDI_RUNEBOMB)
		return false;
	for (auto dir : PathDirs) {
		Point adjacentPosition = position + dir;
		if (OpensHive(adjacentPosition))
			return true;
	}
	return false;
}

bool UseItemOpensGrave(const Item &item, Point position)
{
	if (item.IDidx != IDI_MAPOFDOOM)
		return false;
	for (auto dir : PathDirs) {
		Point adjacentPosition = position + dir;
		if (OpensGrave(adjacentPosition))
			return true;
	}
	return false;
}

/**
 * @brief Oracool: puts the seven Charms of Salvaging into @p stock's first seven EMPTY slots.
 *
 * User request, 2026-08-20: "add in the game and in adria/griswold stores charms 1slot charms of
 * salvaging." Both vendors carry all seven rather than splitting them, because a utility item you
 * have to remember the vendor for is a worse utility item.
 *
 * They are ordinary restocking stock, not pinned like Adria's potions: buying one removes it and
 * the next restock brings it back. Pinning would have meant widening the witch's hardcoded
 * three-slot pinned block, which the random Hellfire book slots sit immediately behind - a
 * collision for no gain, since a vendor the player returns to anyway restocks on every level
 * change.
 *
 * The callers cap their random stock count so seven empty slots are guaranteed to exist.
 */
void StockSalvageCharms(Item *stock, int capacity, int lvl, uint16_t createInfoFlag)
{
	int placed = 0;
	for (int i = 0; i < capacity && placed < oracool::SalvageTierCount; i++) {
		if (!stock[i].isEmpty())
			continue;
		Item &item = stock[i];
		item = {};
		item._iSeed = AdvanceRndSeed();
		GetItemAttrs(item, static_cast<_item_indexes>(oracool::SalvageCharmFor(static_cast<oracool::SalvageTier>(placed))), 1);
		// NOT a town stamp, and this was a real bug until 2026-08-27.
		//
		// `lvl | CF_SMITH` marks the item as vendor-sourced, and CF_TOWN routing is the one path in
		// RecreateItem that IGNORES the packed index and re-derives it by replaying the seed through
		// GetItemIndexForDroppableItem - the pool every Oracool item is deliberately excluded from.
		// So a Charm of Salvaging bought from Griswold came back after a reload as whatever vanilla
		// item that seed happened to land on. Bought, saved, and quietly replaced.
		//
		// Zero instead, which sends RecreateItem down its `icreateinfo == 0` branch and rebuilds the
		// item from the index that was actually stored. A salvage charm has nothing else to
		// reconstruct - no affixes, no level scaling - so the identity is the whole item.
		(void)lvl;
		(void)createInfoFlag;
		item._iCreateInfo = 0;
		item._iIdentified = true;
		item._iStatFlag = true;
		placed++;
	}
}

/** @brief The most Oracool gear bases a depth can offer - the whole contiguous run of them. */
constexpr size_t OracoolGearBaseCount = (IDI_ORACOOL_SPECTRAL_HELM - IDI_ORACOOL_SHOULDERS + 1) + (IDI_ORACOOL_NECRO_SCYTHE_LAST - IDI_ORACOOL_NECRO_WAND_FIRST + 1);

/**
 * @brief Every Oracool GEAR base this depth has opened.
 *
 * Shared by the plain shelf and the affixed one, so the two cannot come to disagree about which
 * bases a depth offers - the gate is the same banded qlvl ladder the drop hook reads, and one copy
 * of it is the only way that stays true.
 *
 * Returns a sized container rather than filling a caller's raw pointer. The pointer version could
 * not say how much room it needed and both callers were sizing their buffer at IDI_LAST + 1 "to be
 * safe", which is the kind of safety that stops being safe the moment someone sizes one correctly.
 */
std::pair<std::array<_item_indexes, OracoolGearBaseCount>, size_t> OracoolGearBasesFor(int lvl)
{
	std::array<_item_indexes, OracoolGearBaseCount> bases {};
	size_t count = 0;
	for (std::underlying_type_t<_item_indexes> i = IDI_ORACOOL_SHOULDERS; i <= IDI_ORACOOL_SPECTRAL_HELM; i++) {
		if (!IsItemAvailable(i))
			continue;
		if (oracool::BandedQlvl(AllItemsList[i].iMinMLvl) <= lvl)
			bases[count++] = static_cast<_item_indexes>(i);
	}
	// The Necromancer's wands and scythes (2026-09-18): anyone's to buy. The heads are Adria's, and his alone.
	for (std::underlying_type_t<_item_indexes> i = IDI_ORACOOL_NECRO_WAND_FIRST; i <= IDI_ORACOOL_NECRO_SCYTHE_LAST; i++) {
		if (oracool::BandedQlvl(AllItemsList[i].iMinMLvl) <= lvl)
			bases[count++] = static_cast<_item_indexes>(i);
	}
	return { bases, count };
}

_item_indexes RndOracoolGearBase(int lvl)
{
	const auto [candidates, candidateCount] = OracoolGearBasesFor(lvl);
	if (candidateCount == 0)
		return IDI_NONE;
	return candidates[GenerateRnd(static_cast<int>(candidateCount))];
}

/** @brief One Oracool gear base of @p type at this depth, or IDI_NONE - Wirt's Gamble slots (2026-09-24). */
_item_indexes RndOracoolGearBaseOfType(int lvl, ItemType type)
{
	const auto [candidates, candidateCount] = OracoolGearBasesFor(lvl);
	std::array<_item_indexes, OracoolGearBaseCount> ofType {};
	int count = 0;
	for (size_t i = 0; i < candidateCount; i++) {
		if (AllItemsList[candidates[i]].itype == type)
			ofType[count++] = candidates[i];
	}
	if (count == 0)
		return IDI_NONE;
	return ofType[GenerateRnd(count)];
}

/** @brief Whether @p idx is one of the bases OracoolGearBasesFor draws from. */
bool IsOracoolGearBase(_item_indexes idx)
{
	return IsOracoolItemIdx(idx) || (idx >= IDI_ORACOOL_NECRO_WAND_FIRST && idx <= IDI_ORACOOL_NECRO_SCYTHE_LAST);
}

/**
 * @brief Fills up to @p want empty slots with PLAIN Oracool gear of this depth.
 *
 * Plain since 2026-08-27 - see the note at the roll itself. Griswold's Basic tab promises unaffixed
 * gear and this was handing it magic items.
 *
 * The shops carried nothing but vanilla stock (user, 2026-08-27: "all shops to also offer all of the
 * new items we have introduced. i now only see vanilla items"), and the reason is structural rather
 * than an oversight: every vendor rolls its stock through GetItemIndexForDroppableItem, and that
 * pool explicitly refuses Oracool items because **the pool is the save format**. UnPackItem rebuilds
 * a town item's index by replaying its seed through that exact walk, so adding to the list would
 * silently re-identify every item already bought and saved.
 *
 * So this is a hook beside the pool, the same shape TrySpawnOracoolSetItem uses for drops.
 *
 * ## About the stamp - corrected 2026-08-27
 *
 * This comment used to say a CF_SMITH stamp here "would be replaced on the next load", because
 * CF_TOWN is the route that re-derives an index by replaying the seed through the pool. That is
 * still true of the replay, and it is NOT true of the load: LoadHeroItems reads a complete stored
 * record over the replayed one, so in single-player the replay's output is discarded before anyone
 * sees it. Measured, not assumed - see StoredRecordWinsOverAnUnreplayableItem, which plants exactly
 * that combination and gets the charm back.
 *
 * The bare-level stamp stays, for two reasons that survive the correction: it is what the multiplayer
 * wire format still replays from, and it is honest - the item was not rolled by Griswold's own
 * generator, so claiming CF_SMITH would describe something that never happened.
 *
 * Depth-gated by the same banded qlvl ladder the drop hook reads, so a level-2 Griswold offers
 * leather and a level-50 one offers spectral, by data rather than by a table here.
 */
int StockOracoolVendorItems(Item *stock, int capacity, int lvl, int want)
{
	if (!oracool::IsSinglePlayer() || want <= 0)
		return 0;

	// Gated by the CHARACTER's level as well as the vendor's (audit, 2026-09-24, user dev note: "make
	// sure all vendors can offer orcl items"). The vendor level is capped at 16, which reached about 40
	// of the armour expansion's 147 bases and half the Necromancer's wands and scythes; the Rare
	// shelf found and fixed the same cap on 2026-09-12. The affix and item levels still follow lvl.
	const auto [candidates, candidateCount] = OracoolGearBasesFor(std::max<int>(MyPlayer->_pLevel, lvl));
	if (candidateCount == 0)
		return 0;

	int placed = 0;
	for (int i = 0; i < capacity && placed < want; i++) {
		if (!stock[i].isEmpty())
			continue;
		const _item_indexes idx = candidates[GenerateRnd(static_cast<int>(candidateCount))];
		// The same clamp the drop hook applies, for the same reason its comment gives.
		const int itemLevel = std::clamp(lvl, 1, 30);
		Item &item = stock[i];
		item = {};
		// PLAIN, and this is a fix rather than a preference (user, 2026-08-27: "why am i seeing
		// magic oracool items in basic shop?").
		//
		// It went through SetupAllItems, which is the DROP path: it rolls affixes and can roll a
		// quality tier on top of them. That is right for something falling out of a monster and
		// wrong for this shelf, because Griswold's shop has a rule and the rule is legible - Basic
		// sells plain gear, Magic sells affixed gear, and the tabs beyond those sell what their
		// names say. An Oracool base arriving on Basic already magical broke the one promise the tab
		// makes.
		//
		// So it is built exactly the way SpawnSmith builds its own basic stock: attributes from the
		// base row, a vendor ilvl stamp, a chance of a base TIER - and no call to the affix roller
		// at all. A base tier is not an affix; it is what the item IS, and Griswold's vanilla basics
		// already roll for one.
		item._iSeed = AdvanceRndSeed();
		SetRndSeed(item._iSeed);
		oracool::StampVendorItemLevel(item, itemLevel);
		GetItemAttrs(item, idx, itemLevel);
		oracool::ApplyVendorTier(item, lvl, item._iSeed,
		    gbIsHellfire ? MaxVendorValueHf : MaxVendorValue);
		// Still NOT a town stamp - see this function's header. A bare level is what a rolled dungeon
		// item carries, and it is what keeps the packed index instead of re-deriving it.
		item._iCreateInfo = std::min(itemLevel, 63);
		item._iIdentified = true;
		item._iStatFlag = MyPlayer->CanUseItem(item);
		placed++;
	}
	return placed;
}

/**
 * @brief Fills up to @p want empty slots with AFFIXED Oracool gear - Griswold's Magic tab.
 *
 * The gap left when the Basic tab's Oracool gear was made plain (user, 2026-08-27: "fill that gap -
 * add affixed oracool bases to magic tab"). Until this, an Oracool base with affixes on it existed
 * only as a monster drop: the premium shelf rolls its bases through RndPremiumItem, and that walks
 * the droppable pool Oracool items are deliberately excluded from.
 *
 * A SECOND PASS over the premium array rather than a branch inside SpawnOnePremium, and the reason
 * is the save format rather than tidiness. SpawnOnePremium's sequence is replayed verbatim by
 * RecreatePremiumItem - `SetRndSeed(seed)` then `RndPremiumItem` - so a single extra GenerateRnd
 * inserted between those two lines would make every recreated premium item differ from the one that
 * was generated. This pass touches none of that stream.
 *
 * `onlygood` is TRUE, and load-bearing: without it GetItemBLevel has a random component that can
 * decide an item rolls no affixes at all, and a shelf that is supposed to be magical would be part
 * plain. `allowTieredRoll` is FALSE, so these come out as ordinary magic items - the Magic tab
 * promises affixed gear, and Rare, Set and Unique each have a tab of their own to promise from.
 */
int StockOracoolMagicItems(Item *stock, int capacity, int lvl, int want)
{
	if (!oracool::IsSinglePlayer() || want <= 0)
		return 0;

	// Gated by the CHARACTER's level as well as the vendor's (audit, 2026-09-24, user dev note: "make
	// sure all vendors can offer orcl items"). The vendor level is capped at 16, which reached about 40
	// of the armour expansion's 147 bases and half the Necromancer's wands and scythes; the Rare
	// shelf found and fixed the same cap on 2026-09-12. The affix and item levels still follow lvl.
	const auto [candidates, candidateCount] = OracoolGearBasesFor(std::max<int>(MyPlayer->_pLevel, lvl));
	if (candidateCount == 0)
		return 0;

	int placed = 0;
	for (int i = 0; i < capacity && placed < want; i++) {
		if (!stock[i].isEmpty())
			continue;
		const _item_indexes idx = candidates[GenerateRnd(static_cast<int>(candidateCount))];
		const int itemLevel = std::clamp(lvl, 1, 30);
		Item &item = stock[i];
		item = {};
		// The ITEM level lifted by the difficulty, as the Rare shelf stamps its own (audit, 2026-09-24):
		// without it these never passed ilvl 30 and never reached a Torment tier.
		// A shelf item that comes out a unique does not spend that unique's one drop (audit, 2026-09-29: it was barred from
		// dropping for the rest of the game, and Wirt restocks on every purchase) - as CreateUniqueVendorItem already keeps it.
		std::array<bool, MaxUniqueItems> uniquesBefore;
		std::copy(std::begin(UniqueItemFlags), std::end(UniqueItemFlags), uniquesBefore.begin());
		SetupAllItems(*MyPlayer, item, idx, AdvanceRndSeed(), itemLevel, 1, /*onlygood=*/true,
		    /*recreate=*/false, /*pregen=*/false, /*allowTieredRoll=*/false, std::nullopt,
		    /*itemLevel=*/oracool::VendorItemLevel(lvl));
		std::copy(uniquesBefore.begin(), uniquesBefore.end(), std::begin(UniqueItemFlags));
		item._iDurability = item._iMaxDur; // sold whole, not worn like a drop (round 3 audit)
		// A bare level, never CF_SMITHPREMIUM - see StockOracoolVendorItems' header for why a town
		// stamp on an Oracool item comes back as something else after a reload.
		item._iCreateInfo = std::min(itemLevel, 63);
		item._iIdentified = true;
		item._iStatFlag = MyPlayer->CanUseItem(item);
		placed++;
	}
	return placed;
}

/**
 * @brief Fills up to @p want empty slots with items of one TYPE - Adria's books and staves.
 *
 * Adria's shelf was a random draw from everything she deals in, so how many books and how many
 * staves it held was a dice roll, and on a bad one there were neither (user, 2026-08-27: "adria shop
 * to fill as much as it can the 10x16 grid. add books and staves. including rare staves"). This
 * guarantees a block of each instead of hoping for one.
 *
 * One function for all three blocks, because they differ only in what they ask for: books want no
 * affixes (a book is its spell), plain staves take the ordinary roll, and rare staves force the
 * tier. Three near-copies would be three places for the retry bound to drift.
 *
 * @param onlygood pins the affix level so a forced tier actually lands - see RetierOracoolItem.
 * @param forcedTier when set, an item that does not come out at that tier is rerolled.
 */
int StockVendorTypedItems(Item *stock, int capacity, int lvl, int want, ItemType itemType, int miscId,
    bool onlygood, std::optional<OracoolItemTier> forcedTier)
{
	if (want <= 0)
		return 0;

	// Bounded per slot. RndTypeItems draws from the shared pool and a forced tier can miss - a base
	// that cannot carry tiered affixes is a legitimate miss - so an unbounded "keep trying until it
	// works" would spin forever on a depth that offers no valid candidate.
	constexpr int MaxAttemptsPerItem = 8;
	int placed = 0;
	for (int i = 0; i < capacity && placed < want; i++) {
		if (!stock[i].isEmpty())
			continue;
		Item &item = stock[i];
		bool made = false;
		for (int attempt = 0; attempt < MaxAttemptsPerItem && !made; attempt++) {
			item = {};
			const _item_indexes idx = RndTypeItems(itemType, miscId, lvl);
			// The pool can come back empty-handed, and when it does GetItemIndexForDroppableItem
			// hands back whatever its static scratch array happened to hold. Checking the TYPE of
			// what arrived is what turns that into "this depth has none of these" rather than a
			// silently wrong item on the shelf.
			if (AllItemsList[idx].itype != itemType)
				break;
			// A staff that comes out a unique does not spend that unique's one drop, as on the magic shelf: Adria
			// restocks on every town visit and Refresh (round 3 audit, v1.12.228).
			std::array<bool, MaxUniqueItems> uniquesBefore;
			std::copy(std::begin(UniqueItemFlags), std::end(UniqueItemFlags), uniquesBefore.begin());
			// At the difficulty-lifted item level every other shelf stamps: Adria's rare staves never reached a Hell or
			// Torment tier and her guaranteed books stayed at the Normal band (round 12 audit, v1.12.237).
			SetupAllItems(*MyPlayer, item, idx, AdvanceRndSeed(), lvl, 1, onlygood,
			    /*recreate=*/false, /*pregen=*/false, /*allowTieredRoll=*/forcedTier.has_value(), forcedTier,
			    /*itemLevel=*/oracool::VendorItemLevel(lvl));
			std::copy(uniquesBefore.begin(), uniquesBefore.end(), std::begin(UniqueItemFlags));
			item._iDurability = item._iMaxDur; // sold whole, not worn like a drop
			if (forcedTier.has_value() && item._iOracoolTier != *forcedTier)
				continue;
			made = true;
		}
		if (!made) {
			// Leave the slot empty and stop: if this depth cannot produce one, it cannot produce
			// the next either, and the remaining slots are better spent by whoever stocks after us.
			item = {};
			break;
		}
		item._iIdentified = true;
		item._iStatFlag = MyPlayer->CanUseItem(item);
		placed++;
	}
	return placed;
}

/**
 * @brief Stocks FIXED-IDENTITY Oracool goods - gems, runes, jewels, charms - that @p accepts picks.
 *
 * The sibling of StockOracoolVendorItems, and separate from it because these items are not rolled.
 * A gem has no affixes and no level scaling; it IS its index. So it is built with InitializeItem and
 * stamped `_iCreateInfo = 0`, which is the branch of RecreateItem that rebuilds from the packed
 * index rather than replaying a seed.
 *
 * That stamp is the whole reason this exists as its own function rather than a flag on the other
 * one. Getting it wrong is not cosmetic: a town stamp sends the item through the pool that excludes
 * Oracool indices entirely, and it comes back as something else - which is exactly what had been
 * happening to Charms of Salvaging until 2026-08-27.
 */
int StockOracoolFixedItems(Item *stock, int capacity, int lvl, int want,
    tl::function_ref<bool(std::underlying_type_t<_item_indexes>)> accepts)
{
	if (!oracool::IsSinglePlayer() || want <= 0)
		return 0;

	_item_indexes candidates[IDI_LAST + 1];
	int candidateCount = 0;
	for (std::underlying_type_t<_item_indexes> i = IDI_GOLD; i <= IDI_LAST; i++) {
		if (!IsItemAvailable(i) || !accepts(i))
			continue;
		if (oracool::BandedQlvl(AllItemsList[i].iMinMLvl) > lvl)
			continue;
		candidates[candidateCount++] = static_cast<_item_indexes>(i);
	}
	if (candidateCount == 0)
		return 0;

	int placed = 0;
	for (int i = 0; i < capacity && placed < want; i++) {
		if (!stock[i].isEmpty())
			continue;
		Item &item = stock[i];
		item = {};
		item._iSeed = AdvanceRndSeed();
		GetItemAttrs(item, candidates[GenerateRnd(candidateCount)], 1);
		item._iCreateInfo = 0;
		item._iIdentified = true;
		item._iStatFlag = true;
		placed++;
	}
	return placed;
}

/**
 * @brief How many slots each vendor holds back for its Oracool line, out of its own stock array.
 *
 * These are RESERVATIONS, not requests. The stocking helpers below fill empty slots, so a count
 * that is only asked for after the vanilla roll has run is a count the vanilla roll can take to
 * zero - which is exactly what it was doing. Subtract these from the vanilla item count and the
 * shelf's composition is decided up front instead of by a dice roll.
 */
constexpr int SmithOracoolCount = SMITH_ITEMS / 3;
/**
 * @brief Adria's reserved blocks. A COUNT each, not a fraction of the array.
 *
 * They were fractions while the array was the shelf. It is not any more - the array over-supplies
 * the grid on purpose and PlaceStock decides where the shelf ends - so a fraction would grow every
 * block the next time the array does, which is not what "twelve gems" means.
 *
 * Books and staves are guaranteed rather than hoped for (user, 2026-08-27: "add books and staves.
 * including rare staves"): they were reachable through her ordinary roll, so a bad draw left her
 * with none of either.
 */
constexpr int WitchOracoolCount = 12;
constexpr int WitchBookCount = 10;
constexpr int WitchStaffCount = 8;
constexpr int WitchRareStaffCount = 5;
constexpr int HealerOracoolCount = 5;
/** @brief Griswold's Magic shelf: a fifth of it, which is six of thirty. See SpawnPremium. */
constexpr int PremiumOracoolCount = SMITH_PREMIUM_ITEMS / 5;

void SpawnSmith(int lvl)
{
	constexpr int PinnedItemCount = 0;

	int maxValue = MaxVendorValue;
	// Oracool: the stock fills the shop grid now, so the count is derived from the array rather
	// than from vanilla's two hand-written numbers. The lower bound is three quarters of it, so a
	// bad roll still leaves a full-looking shop instead of a third of one.
	int maxItems = SMITH_ITEMS - oracool::SalvageTierCount - SmithOracoolCount;
	if (gbIsHellfire)
		maxValue = MaxVendorValueHf;

	const int minItems = maxItems * 3 / 4;
	int iCnt = GenerateRnd(maxItems - minItems + 1) + minItems;
	// Oracool: hold back the salvage charms' slots AND the rolled-gear slots below.
	//
	// Reserving only the first was a defect (user, 2026-08-27: "i still dont see oracool items in
	// magic shop"). StockOracoolVendorItems fills EMPTY slots, so a maximum vanilla roll plus seven
	// charms left it none, and the Oracool line vanished from the shelf entirely - not rarely, but
	// on every high roll. The same arithmetic held at Adria's and Pepin's.
	iCnt = std::min(iCnt, SMITH_ITEMS - oracool::SalvageTierCount - SmithOracoolCount);
	for (int i = 0; i < iCnt; i++) {
		Item &newItem = smithitem[i];

		do {
			newItem = {};
			newItem._iSeed = AdvanceRndSeed();
			SetRndSeed(newItem._iSeed);
			_item_indexes itemData = RndSmithItem(*MyPlayer, lvl);
			oracool::StampVendorItemLevel(newItem, lvl);
			GetItemAttrs(newItem, itemData, lvl);
			oracool::ApplyVendorTier(newItem, lvl, newItem._iSeed, maxValue);
		} while (newItem._iIvalue > maxValue);

		newItem._iCreateInfo = lvl | CF_SMITH;
		newItem._iIdentified = true;
	}
	for (int i = iCnt; i < SMITH_ITEMS; i++)
		smithitem[i].clear();

	StockSalvageCharms(smithitem, SMITH_ITEMS, lvl, CF_SMITH);

	// A third of the shelf, so the new gear is a real part of what Griswold sells rather than a
	// curiosity that turns up occasionally. The count is what fits after the vanilla roll and the
	// salvage charms have taken their slots - the page decides the rest (see PlaceStock).
	StockOracoolVendorItems(smithitem, SMITH_ITEMS, lvl, SmithOracoolCount);

	SortVendor(smithitem + PinnedItemCount, SMITH_ITEMS - PinnedItemCount);
}

/**
 * @brief The quality-level delta for premium slot @p index of @p count.
 *
 * Replaces vanilla's two hand-written tables (six entries for Diablo, fifteen for Hellfire), which
 * could not answer for the thirty-slot stock the shop grid holds. Same shape they had: the cheap end
 * of the list rolls a level below the player and the dear end up to three above, spread evenly
 * across however many slots there are.
 */
int PremiumLevelDelta(int index, int count)
{
	constexpr int Spread = 5; // -1 through +3
	return -1 + index * Spread / std::max(count, 1);
}

void SpawnPremium(const Player &player)
{
	int8_t lvl = player._pLevel;
	constexpr int maxItems = SMITH_PREMIUM_ITEMS;
	// The TAIL of the array is the Oracool block; the vanilla roll and its rotation both work over
	// the head. Splitting the array rather than interleaving keeps the rotation below exactly what
	// it was - it shifts a contiguous run and refills its end, and an Oracool item caught in that
	// run would be aged out by a rotation that has no way to replace it.
	constexpr int vanillaItems = maxItems - PremiumOracoolCount;
	if (numpremium < maxItems) {
		for (int i = 0; i < vanillaItems; i++) {
			if (premiumitems[i].isEmpty())
				SpawnOnePremium(premiumitems[i], premiumlevel + PremiumLevelDelta(i, vanillaItems), player);
		}
		numpremium = maxItems;
	}
	const bool depthChanged = premiumlevel < lvl;
	while (premiumlevel < lvl) {
		premiumlevel++;
		// One generalised rotation in place of vanilla's two hardcoded ones. Both of those discarded
		// roughly the cheapest third of the list on every level and refilled the tail; this does the
		// same thing without naming individual slots, which is what made them size-specific.
		constexpr int discard = std::max(1, vanillaItems / 3);
		std::move(&premiumitems[discard], &premiumitems[vanillaItems], &premiumitems[0]);
		for (int i = vanillaItems - discard; i < vanillaItems; i++) {
			premiumitems[i].clear();
			SpawnOnePremium(premiumitems[i], premiumlevel + PremiumLevelDelta(i, vanillaItems), player);
		}
	}
	// The Oracool block is rebuilt when the shelf's depth moves, which is how it keeps pace with the
	// character the way the rotation above does. It is NOT rebuilt on an ordinary visit, so an item
	// left unbought is still there next time - the same promise the rest of the shelf makes.
	if (depthChanged) {
		for (int i = vanillaItems; i < maxItems; i++)
			premiumitems[i].clear();
	}
	StockOracoolMagicItems(premiumitems, maxItems, premiumlevel, PremiumOracoolCount);

	// COUNTED, not assumed. `numpremium = maxItems` above is the vanilla line, and it was already a
	// claim rather than a fact; with a second stocking pass that can place fewer than it is asked
	// for, the field would say thirty over a shelf holding twenty-four. Nothing iterates it today -
	// it only gates the refill above and is decremented on a purchase - so this is not a live bug,
	// which is exactly why it is worth closing now rather than after something starts trusting it.
	RecountPremiumStock();
}

void RecountPremiumStock()
{
	numpremium = 0;
	for (const Item &item : premiumitems) {
		if (!item.isEmpty())
			numpremium++;
	}
}

void RestockOnePremiumSlot(int slot, const Player &player)
{
	constexpr int maxItems = SMITH_PREMIUM_ITEMS;
	constexpr int vanillaItems = maxItems - PremiumOracoolCount;
	if (slot < 0 || slot >= maxItems || !premiumitems[slot].isEmpty())
		return;

	// ONE slot, which is what a purchase actually frees. SpawnPremium used to be called instead,
	// and its refill branch fills EVERY empty vanilla slot whenever numpremium < maxItems - so
	// after the one-page trim had deliberately emptied the slots that would not fit, the first
	// purchase refilled all of them at once and the shelf grew back past a page without the player
	// pressing Refresh (external audit of v1.9.97, finding 1). The comment at the call site said
	// "restocking a sold slot"; this is the function that does that.
	if (slot < vanillaItems) {
		SpawnOnePremium(premiumitems[slot], premiumlevel + PremiumLevelDelta(slot, vanillaItems), player);
	} else {
		// StockOracoolMagicItems fills the first `want` EMPTY entries of the range it is handed, so
		// a range of exactly one entry restocks exactly this slot.
		StockOracoolMagicItems(&premiumitems[slot], 1, premiumlevel, 1);
	}
	RecountPremiumStock();
}

void SpawnWitch(int lvl)
{
	constexpr int PinnedItemCount = 3;
	constexpr std::array<_item_indexes, PinnedItemCount> PinnedItemTypes = { IDI_MANA, IDI_FULLMANA, IDI_PORTAL };
	constexpr int MaxPinnedBookCount = 4;
	constexpr std::array<_item_indexes, MaxPinnedBookCount> PinnedBookTypes = { IDI_BOOK1, IDI_BOOK2, IDI_BOOK3, IDI_BOOK4 };

	int bookCount = 0;
	const int pinnedBookCount = gbIsHellfire ? GenerateRnd(MaxPinnedBookCount) : 0;
	// Oracool: same fill as the smith - derived from the array, three quarters full at worst, with
	// every reserved block held back. Vanilla's `reservedItems` split existed to keep a 25-slot
	// array from overfilling a four-row list; the grid has room for all of it.
	const int maxItems = WITCH_ITEMS - oracool::SalvageTierCount - WitchOracoolCount
	    - WitchBookCount - WitchStaffCount - WitchRareStaffCount;
	const int itemCount = GenerateRnd(maxItems - maxItems * 3 / 4 + 1) + maxItems * 3 / 4;
	const int maxValue = gbIsHellfire ? MaxVendorValueHf : MaxVendorValue;

	for (int i = 0; i < WITCH_ITEMS; i++) {
		Item &item = witchitem[i];
		item = {};

		if (i < PinnedItemCount) {
			item._iSeed = AdvanceRndSeed();
			// Not a Town Portal scroll where the portal is built in (round 37 audit): a healing potion stands in.
			const _item_indexes pinned = PinnedItemTypes[i] == IDI_PORTAL && oracool::IsBuiltInPortalAbility(SpellID::TownPortal)
			    ? IDI_HEAL
			    : PinnedItemTypes[i];
			GetItemAttrs(item, pinned, 1);
			item._iCreateInfo = lvl;
			item._iStatFlag = true;
			continue;
		}

		if (gbIsHellfire) {
			if (i < PinnedItemCount + MaxPinnedBookCount && bookCount < pinnedBookCount) {
				_item_indexes bookType = PinnedBookTypes[i - PinnedItemCount];
				if (lvl >= oracool::BandedQlvl(AllItemsList[bookType].iMinMLvl)) {
					item._iSeed = AdvanceRndSeed();
					SetRndSeed(item._iSeed);
					DiscardRandomValues(1);
					GetItemAttrs(item, bookType, lvl);
					item._iCreateInfo = lvl | CF_WITCH;
					item._iIdentified = true;
					bookCount++;
					continue;
				}
			}
		}

		if (i >= itemCount) {
			item.clear();
			continue;
		}

		do {
			item = {};
			item._iSeed = AdvanceRndSeed();
			SetRndSeed(item._iSeed);
			_item_indexes itemData = RndWitchItem(*MyPlayer, lvl);
			// The ilvl BEFORE GetItemAttrs: a book picks its spell in there, and the band gate reads the
			// ilvl to decide which spells this depth may offer.
			oracool::StampVendorItemLevel(item, lvl);
			GetItemAttrs(item, itemData, lvl);
			oracool::ApplyVendorTier(item, lvl, item._iSeed, maxValue);
			int maxlvl = -1;
			if (GenerateRnd(100) <= 5)
				maxlvl = 2 * lvl;
			if (maxlvl == -1 && item._iMiscId == IMISC_STAFF)
				maxlvl = 2 * lvl;
			if (maxlvl != -1)
				GetItemBonus(*MyPlayer, item, maxlvl / 2, maxlvl, true, true);
		} while (item._iIvalue > maxValue);

		item._iCreateInfo = lvl | CF_WITCH;
		item._iIdentified = true;
	}

	StockSalvageCharms(witchitem, WITCH_ITEMS, lvl, CF_WITCH);

	// Adria deals in the magical, so she carries the SOCKETABLES rather than Griswold's gear (user,
	// 2026-08-27). Gems, runes and jewels are what a spellcaster's shelf should have, and they were
	// reachable only as drops before this.
	StockOracoolFixedItems(witchitem, WITCH_ITEMS, lvl, WitchOracoolCount,
	    [](std::underlying_type_t<_item_indexes> i) {
		    return IsOracoolGemIdx(i) || IsOracoolRuneIdx(i) || IsOracoolJewelIdx(i);
	    });
	// And, for a Necromancer, a shrunken head or two: his alone, so no other hero sees them here (oracool/necro_items.h).
	if (oracool::NecroHeadsMayDrop()) {
		StockOracoolFixedItems(witchitem, WITCH_ITEMS, lvl, 2,
		    [](std::underlying_type_t<_item_indexes> i) { return oracool::IsNecroHeadIdx(i); });
	}

	// The two things a spellcaster comes to Adria for, guaranteed rather than rolled for.
	//
	// Books take no affixes - a book IS its spell - so they are stocked with the plain roll and
	// `onlygood` would buy nothing. Staves take the ordinary one. The rare staves force the tier
	// with `onlygood` set, which is what makes a forced tier actually land rather than silently
	// not happen a large share of the time.
	StockVendorTypedItems(witchitem, WITCH_ITEMS, lvl, WitchBookCount, ItemType::Misc, IMISC_BOOK,
	    /*onlygood=*/false, std::nullopt);
	StockVendorTypedItems(witchitem, WITCH_ITEMS, lvl, WitchStaffCount, ItemType::Staff, -1,
	    /*onlygood=*/false, std::nullopt);
	StockVendorTypedItems(witchitem, WITCH_ITEMS, lvl, WitchRareStaffCount, ItemType::Staff, -1,
	    /*onlygood=*/true, OracoolItemTier::Rare);

	SortVendor(witchitem + PinnedItemCount, WITCH_ITEMS - PinnedItemCount);
}

/**
 * @brief One roll of Wirt's table into @p out - the body SpawnBoy always had, against the one-item
 * global; since 2026-09-20 his Shop tab is BOY_ITEMS of these (SpawnBoy below).
 */
void RollBoyItem(Item &out, int lvl)
{
	Item &boyitem = out; // the body was written against the global of that name
	int ivalue = 0;
	bool keepgoing = false;
	int count = 0;

	Player &myPlayer = *MyPlayer;

	HeroClass pc = myPlayer._pClass;
	int strength = std::max(myPlayer.GetMaximumAttributeValue(CharacterAttribute::Strength), myPlayer._pStrength);
	int dexterity = std::max(myPlayer.GetMaximumAttributeValue(CharacterAttribute::Dexterity), myPlayer._pDexterity);
	int magic = std::max(myPlayer.GetMaximumAttributeValue(CharacterAttribute::Magic), myPlayer._pMagic);
	strength += strength / 5;
	dexterity += dexterity / 5;
	magic += magic / 5;

	do {
		keepgoing = false;
		boyitem = {};
		boyitem._iSeed = AdvanceRndSeed();
		SetRndSeed(boyitem._iSeed);
		_item_indexes itype = RndBoyItem(*MyPlayer, lvl);
		oracool::StampVendorItemLevel(boyitem, lvl);
		GetItemAttrs(boyitem, itype, lvl);
		oracool::ApplyVendorTier(boyitem, lvl, boyitem._iSeed, gbIsHellfire ? MaxBoyValueHf : MaxBoyValue);
		GetItemBonus(*MyPlayer, boyitem, lvl, 2 * lvl, true, true);

		if (!gbIsHellfire) {
			if (boyitem._iIvalue > MaxBoyValue) {
				keepgoing = true; // prevent breaking the do/while loop too early by failing hellfire's condition in while
				continue;
			}
			break;
		}

		ivalue = 0;

		ItemType itemType = boyitem._itype;

		switch (itemType) {
		case ItemType::LightArmor:
		case ItemType::MediumArmor:
		case ItemType::HeavyArmor: {
			const auto *const mostValuablePlayerArmor = myPlayer.GetMostValuableItem(
			    [](const Item &item) {
				    return IsAnyOf(item._itype, ItemType::LightArmor, ItemType::MediumArmor, ItemType::HeavyArmor);
			    });

			ivalue = mostValuablePlayerArmor == nullptr ? 0 : mostValuablePlayerArmor->_iIvalue;
			break;
		}
		case ItemType::Shield:
		case ItemType::Axe:
		case ItemType::Bow:
		case ItemType::Mace:
		case ItemType::Sword:
		case ItemType::Helm:
		case ItemType::Staff:
		case ItemType::Ring:
		case ItemType::Amulet: {
			const auto *const mostValuablePlayerItem = myPlayer.GetMostValuableItem(
			    [itemType](const Item &item) { return item._itype == itemType; });

			ivalue = mostValuablePlayerItem == nullptr ? 0 : mostValuablePlayerItem->_iIvalue;
			break;
		}
		default:
			app_fatal("Invalid item spawn");
		}
		ivalue = ivalue * 4 / 5; // avoids forced int > float > int conversion

		count++;

		if (count < 200) {
			switch (pc) {
			case HeroClass::Warrior:
				if (IsAnyOf(itemType, ItemType::Bow, ItemType::Staff))
					ivalue = INT_MAX;
				break;
			case HeroClass::Rogue:
				if (IsAnyOf(itemType, ItemType::Sword, ItemType::Staff, ItemType::Axe, ItemType::Mace, ItemType::Shield))
					ivalue = INT_MAX;
				break;
			case HeroClass::Sorcerer:
			case HeroClass::Necromancer:
				if (IsAnyOf(itemType, ItemType::Staff, ItemType::Axe, ItemType::Bow, ItemType::Mace))
					ivalue = INT_MAX;
				break;
			case HeroClass::Monk:
				if (IsAnyOf(itemType, ItemType::Bow, ItemType::MediumArmor, ItemType::Shield, ItemType::Mace))
					ivalue = INT_MAX;
				break;
			case HeroClass::Bard:
				if (IsAnyOf(itemType, ItemType::Axe, ItemType::Mace, ItemType::Staff))
					ivalue = INT_MAX;
				break;
			case HeroClass::Barbarian:
				if (IsAnyOf(itemType, ItemType::Bow, ItemType::Staff))
					ivalue = INT_MAX;
				break;
			}
		}
	} while (keepgoing
	    || ((
	            boyitem._iIvalue > MaxBoyValueHf
	            || boyitem._iMinStr > strength
	            || boyitem._iMinMag > magic
	            || boyitem._iMinDex > dexterity
	            || boyitem._iIvalue < ivalue)
	        && count < 250));
	// Oracool: CF_LEVEL is only 6 bits wide (max 63); clamp so a level 64-99 character's raw
	// level doesn't bleed into the adjacent flag bits of _iCreateInfo.
	boyitem._iCreateInfo = std::min(lvl, static_cast<int>(CF_LEVEL)) | CF_BOY;
}

void RollBoyShopSlot(Item &out, int slot, int lvl)
{
	// Every THIRD slot is Oracool gear (audit, 2026-09-24, user dev note: "make sure all vendors can
	// offer orcl items"). His table (RndBoyItem) is the droppable pool, which refuses every Oracool
	// base because the pool is the save format; the other shops reach the fork's gear through a
	// second pass, and Wirt never had one. Interleaved rather than a block at the end, because
	// TrimShopStockToOnePage cuts whatever does not fit the page and a block at the end would be
	// the first thing cut. Magic, like the rest of his stock; the same helper Griswold's Magic tab
	// uses, with its bare-level stamp (never CF_BOY - see StockOracoolVendorItems' header).
	if (oracool::IsSinglePlayer() && slot % 3 == 2) {
		out = {};
		if (StockOracoolMagicItems(&out, 1, lvl, 1) == 1)
			return;
	}
	// And every third slot a RARE (user, 2026-09-25 dev note: "make wirt offer a mix of blue and yellow items
	// in his shop"): the Rare shelf's own roller, so the base is drawn from the whole catalogue - the fork's
	// gear a third of the time - and the tier is forced, onlygood. A base that cannot carry a tier is a miss
	// and is rolled again; after twenty misses the slot falls back to his ordinary blue roll.
	if (oracool::IsSinglePlayer() && slot % 3 == 1) {
		for (int attempt = 0; attempt < 20; attempt++) {
			out = {};
			if (CreateRareVendorItem(*MyPlayer, out, lvl))
				return;
		}
	}
	RollBoyItem(out, lvl);
	out._iIdentified = true; // the Shop tab sells what it shows; the gamble is the other tab
}

namespace {

/** @brief The Gamble tab's slots, one base each (user, 2026-09-20). Rings and amulets dearest, as Diablo II's Gheed had it. */
constexpr ItemType GambleSlots[] = { // cycled by SpawnGambleStock: GAMBLE_ITEMS is the page, not the slot count (2026-09-20)
	ItemType::Helm, ItemType::LightArmor, ItemType::HeavyArmor, ItemType::Shield, ItemType::Sword, ItemType::Axe,
	ItemType::Mace, ItemType::Bow, ItemType::Staff, ItemType::Ring, ItemType::Amulet,
	// The fork's six worn slots (audit, 2026-09-24): Oracool bases only - the droppable pool has none.
	ItemType::Shoulders, ItemType::Bracers, ItemType::Gloves, ItemType::Belt, ItemType::Legs, ItemType::Boots
};

} // namespace

int GamblePriceFor(ItemType type, int lvl)
{
	int base = 200;
	switch (type) {
	case ItemType::Ring:
		base = 600;
		break;
	case ItemType::Amulet:
		base = 800;
		break;
	case ItemType::LightArmor:
	case ItemType::MediumArmor:
	case ItemType::HeavyArmor:
	case ItemType::Sword:
	case ItemType::Axe:
	case ItemType::Mace:
	case ItemType::Bow:
	case ItemType::Legs:
		base = 300;
		break;
	case ItemType::Shield:
	case ItemType::Staff:
	case ItemType::Shoulders:
		base = 250;
		break;
	default:
		break;
	}
	// A gold sink that scales with the hero (the roadmap's design): the price is the slot's base
	// times the level, so a ring at level 50 is 30,000 and at 99 nearly Diablo II's 50,000.
	const int level = std::clamp(lvl, 1, MaxCharacterLevel);
	int64_t price = static_cast<int64_t>(base) * level;
	// And for worn gear, times the value its base tier is expected to carry (user, 2026-09-27: "fix the decisions for
	// me too"). The roll can land on a Hell or Torment base, worth four to thirty times a Normal one, while the price
	// stayed a Normal item's: from about level 42 a gambled piece sold for more than it cost - a gold faucet, not a
	// sink. The roll's item level runs to the hero's + 4. Rings and amulets too: they DO carry a base tier
	// (CanCarryBaseTier refuses only belt and unequipable items), and their gamble sold for up to 2.4 times its price
	// from about level 30 - repeatable through Wirt's free Refresh (round 17 audit, v1.12.242).
	price = price * oracool::ExpectedTierValuePercent(level + 4) / 100;
	return static_cast<int>(std::min<int64_t>(price, std::numeric_limits<int>::max()));
}

void SpawnGambleStock(int lvl)
{
	const Player &player = *MyPlayer;
	for (int i = 0; i < GAMBLE_ITEMS; i++) {
		const ItemType type = GambleSlots[i % std::size(GambleSlots)];
		Item &item = gambleitems[i];
		item = {};
		// A base of the slot the hero could wear at this level, from the vendor pool. Nothing is
		// rolled yet - the roll is the purchase (RollGambleResult).
		// Oracool gear too (audit, 2026-09-24): the fork's six slots draw only from it, and a third of
		// the vanilla slots do, so a helm slot can be a Spectral Helm and not only a vanilla cap.
		_item_indexes idx = IDI_NONE;
		if (oracool::IsSinglePlayer() && (IsOracoolItemType(type) || GenerateRnd(3) == 0))
			idx = RndOracoolGearBaseOfType(std::max(lvl, 1), type);
		if (idx == IDI_NONE && !IsOracoolItemType(type)) {
			idx = GetItemIndexForDroppableItem(false, [&](const ItemData &data) {
				return data.itype == type && PremiumItemOk(player, data) && !IsUniqueExpansionBase(data) && PoolQlvl(data) <= std::max(lvl, 1);
			});
			// The pool never answers IDI_NONE: with nothing that fits it hands back a leftover index,
			// which put another type's base in this slot at this slot's price. Checked by type.
			if (idx != IDI_NONE && AllItemsList[idx].itype != type)
				idx = IDI_NONE;
		}
		if (idx == IDI_NONE)
			continue;
		item._iSeed = AdvanceRndSeed();
		SetRndSeed(item._iSeed);
		GetItemAttrs(item, idx, lvl);
		item._iIdentified = false;
		item._iIvalue = GamblePriceFor(type, lvl);
		// A bare level on an Oracool base, never a town stamp - see StockOracoolVendorItems' header.
		item._iCreateInfo = std::min(lvl, static_cast<int>(CF_LEVEL)) | (IsOracoolGearBase(idx) ? 0 : CF_BOY);
	}
}

void RollGambleResult(Item &out, _item_indexes base, int lvl)
{
	// The gamble (the roadmap's design): the base is known, the rest is the roll - at an item level
	// of the hero's own minus five to plus four, magic or better (onlygood), a unique one time in a
	// hundred (uper 1 is CheckUnique's percent), the fork's Rare, Set and Primal tiers by their own
	// odds inside SetupAllItems. Identified on the purchase, as a drop is.
	const int ilvl = std::clamp(lvl - 5 + GenerateRnd(10), 1, oracool::MaxAreaLevel); // the ladder's top, as every other source (round 34 audit)
	const uint32_t seed = AdvanceRndSeed();
	SetupAllItems(*MyPlayer, out, base, seed, ilvl, /*uper=*/1, /*onlygood=*/true, /*recreate=*/false, /*pregen=*/false);
	out._iIdentified = true;
	out._iDurability = out._iMaxDur; // bought whole, as every shelf's stock is - it came out worn like a drop (round 12)
	out._iCreateInfo = std::min(ilvl, static_cast<int>(CF_LEVEL)) | (IsOracoolGearBase(base) ? 0 : CF_BOY);
}

void SpawnBoy(int lvl)
{
	// Once per level tier, like the one-item table it replaces (user, 2026-09-20: "Shop to be full of
	// the type of items he is eligible to sell. Gamble to be full of items to gamble with for Gold").
	if (boylevel >= (lvl / 2) && !boyitems[0].isEmpty())
		return;
	for (int i = 0; i < BOY_ITEMS; i++)
		RollBoyShopSlot(boyitems[i], i, lvl);
	SpawnGambleStock(lvl);
	boylevel = lvl / 2;
}

void SpawnHealer(int lvl)
{
	constexpr int PinnedItemCount = 2;
	constexpr std::array<_item_indexes, PinnedItemCount + 1> PinnedItemTypes = { IDI_HEAL, IDI_FULLHEAL, IDI_RESURRECT };
	// Oracool: the same reservation the smith and the witch make - Pepin's twenty slots have to
	// leave room for HealerOracoolCount charms after the roll.
	const int itemCount = std::min(GenerateRnd(gbIsHellfire ? 10 : 8) + 10,
	    static_cast<int>(std::size(healitem)) - HealerOracoolCount);

	for (int i = 0; i < 20; i++) {
		Item &item = healitem[i];
		item = {};

		if (i < PinnedItemCount || (gbIsMultiplayer && i == PinnedItemCount)) {
			item._iSeed = AdvanceRndSeed();
			// Not a Town Portal scroll where the portal is built in (round 37 audit): a healing potion stands in.
			const _item_indexes pinned = PinnedItemTypes[i] == IDI_PORTAL && oracool::IsBuiltInPortalAbility(SpellID::TownPortal)
			    ? IDI_HEAL
			    : PinnedItemTypes[i];
			GetItemAttrs(item, pinned, 1);
			item._iCreateInfo = lvl;
			item._iStatFlag = true;
			continue;
		}

		if (i >= itemCount) {
			item.clear();
			continue;
		}

		item._iSeed = AdvanceRndSeed();
		SetRndSeed(item._iSeed);
		_item_indexes itype = RndHealerItem(*MyPlayer, lvl);
		oracool::StampVendorItemLevel(item, lvl);
		GetItemAttrs(item, itype, lvl);
		// Pepin sells potions, which CanCarryBaseTier declines - the call is here so the ilvl gets
		// stamped anyway, and so a future stocked item that IS gear picks the tier up for free.
		oracool::ApplyVendorTier(item, lvl, item._iSeed, gbIsHellfire ? MaxVendorValueHf : MaxVendorValue);
		item._iCreateInfo = lvl | CF_HEALER;
		item._iIdentified = true;
	}

	// Pepin keeps people standing up, so his Oracool line is the stat CHARMS - the same kind of
	// steady, always-on help his potions give (user, 2026-08-27).
	StockOracoolFixedItems(healitem, static_cast<int>(std::size(healitem)), lvl, HealerOracoolCount,
	// Never the three encounter charms: they are the named encounters' rewards, and only their levels
	// (20/24/28, above the vendor cap of 16) kept them off this shelf until now (audit, 2026-09-24).
	    [](std::underlying_type_t<_item_indexes> i) { return IsOracoolCharmIdx(i) && !IsOracoolEncounterCharmIdx(i); });

	SortVendor(healitem + PinnedItemCount, static_cast<int>(std::size(healitem)) - PinnedItemCount);
}

void MakeGoldStack(Item &goldItem, int value)
{
	InitializeItem(goldItem, IDI_GOLD);
	GenerateNewSeed(goldItem);
	goldItem._iStatFlag = true;
	goldItem._ivalue = value;
	SetPlrHandGoldCurs(goldItem);
}

int ItemNoFlippy()
{
	int r = ActiveItems[ActiveItemCount - 1];
	Items[r].AnimInfo.currentFrame = Items[r].AnimInfo.numberOfFrames - 1;
	Items[r]._iAnimFlag = false;
	Items[r]._iSelFlag = 1;

	return r;
}

int SpellBookDropLevel(SpellID ispell)
{
	if (!gbIsHellfire)
		return currlevel;

	const int vanilla = GetSpellBookLevel(ispell) + 1;
	if (vanilla < 1)
		return vanilla; // no book for this spell at all - the caller drops nothing

	// AND deep enough for the fork's own band gate (user report, 2026-09-02: "i clicked on a Slain
	// Hero on level 9 and game crashed" - it hung, in CreateSpellBook's roll loop).
	//
	// GetBookSpell refuses a spell whose oracool::SpellBookItemLevel is above the item's ilvl, and
	// vanilla's sBookLvl - which is all this used to ask - is an unrelated number. The Slain Hero
	// wants Lightning: sBookLvl 4, so ilvl 5, against a band of 6. Six is greater than five, so every
	// roll refused it and the loop spun for as long as the process lived. Na-Krul's Apocalypse was
	// worse and had never been reported: ilvl 20 against a band of 52.
	//
	// The ilvl is RAISED rather than the gate lowered, because the band is the deliberate design - it
	// is what stops a shallow book teaching a deep spell. What was wrong was asking for the book at a
	// depth the band forbids.
	//
	// Its own function so the audit test can ask what CreateSpellBook will actually roll at, instead
	// of keeping a second copy of this arithmetic that would agree with it only until one changed.
	return std::max(vanilla, oracool::SpellBookItemLevel(ispell));
}

void CreateSpellBook(Point position, SpellID ispell, bool sendmsg, bool delta)
{
	const int lvl = SpellBookDropLevel(ispell);
	if (lvl < 1)
		return;

	_item_indexes idx = RndTypeItems(ItemType::Misc, IMISC_BOOK, lvl);
	if (ActiveItemCount >= MAXITEMS)
		return;

	int ii = AllocateItem();
	auto &item = Items[ii];

	// BOUNDED. The loop above was `while (true)`, and an unbounded roll-until-it-matches is a hang
	// waiting for the day its target becomes unrollable - which is exactly what happened. The level
	// fix makes the target reachable again; this makes the next such mismatch a dropped item and a
	// log line instead of a frozen game. Generous enough that a legitimate roll never reaches it:
	// the book pool is small and the target is in it, so a match arrives in a handful of draws.
	bool rolled = false;
	for (int attempt = 0; attempt < 10000; attempt++) {
		item = {};
		SetupAllItems(*MyPlayer, item, idx, AdvanceRndSeed(), 2 * lvl, 1, true, false, delta,
		    /*allowTieredRoll=*/true, std::nullopt, /*itemLevel=*/lvl);
		if (item._iMiscId == IMISC_BOOK && item._iSpell == ispell) {
			rolled = true;
			break;
		}
	}
	if (!rolled) {
		LogError("CreateSpellBook: no roll produced spell {} at ilvl {} - dropping nothing rather than hanging",
		    static_cast<int>(ispell), lvl);
		Items[ii] = {};
		ActiveItemCount--;
		return;
	}
	GetSuperItemSpace(position, ii);

	if (sendmsg)
		NetSendCmdPItem(false, CMD_DROPITEM, item.position, item);
	if (delta)
		DeltaAddItem(ii);
}

void CreateMagicArmor(Point position, ItemType itemType, int icurs, bool sendmsg, bool delta)
{
	int lvl = ItemsGetCurrlevel();
	CreateMagicItem(position, lvl, itemType, IMISC_NONE, icurs, sendmsg, delta);
}

void CreateAmulet(Point position, int lvl, bool sendmsg, bool delta, bool spawn /*= false*/)
{
	CreateMagicItem(position, lvl, ItemType::Amulet, IMISC_AMULET, ICURS_AMULET, sendmsg, delta, spawn);
}

void CreateMagicWeapon(Point position, ItemType itemType, int icurs, bool sendmsg, bool delta)
{
	int imid = IMISC_NONE;
	if (itemType == ItemType::Staff)
		imid = IMISC_STAFF;

	int curlv = ItemsGetCurrlevel();

	CreateMagicItem(position, curlv, itemType, imid, icurs, sendmsg, delta);
}

bool GetItemRecord(uint32_t nSeed, uint16_t wCI, int nIndex)
{
	uint32_t ticks = SDL_GetTicks();

	for (int i = 0; i < gnNumGetRecords; i++) {
		if (ticks - itemrecord[i].dwTimestamp > 6000) {
			// BUGFIX: loot actions for multiple quest items with same seed (e.g. blood stone) performed within less than 6 seconds will be ignored.
			NextItemRecord(i);
			i--;
		} else if (nSeed == itemrecord[i].nSeed && wCI == itemrecord[i].wCI && nIndex == itemrecord[i].nIndex) {
			return false;
		}
	}

	return true;
}

void SetItemRecord(uint32_t nSeed, uint16_t wCI, int nIndex)
{
	uint32_t ticks = SDL_GetTicks();

	if (gnNumGetRecords == MAXITEMS) {
		return;
	}

	itemrecord[gnNumGetRecords].dwTimestamp = ticks;
	itemrecord[gnNumGetRecords].nSeed = nSeed;
	itemrecord[gnNumGetRecords].wCI = wCI;
	itemrecord[gnNumGetRecords].nIndex = nIndex;
	gnNumGetRecords++;
}

void PutItemRecord(uint32_t nSeed, uint16_t wCI, int nIndex)
{
	uint32_t ticks = SDL_GetTicks();

	for (int i = 0; i < gnNumGetRecords; i++) {
		if (ticks - itemrecord[i].dwTimestamp > 6000) {
			NextItemRecord(i);
			i--;
		} else if (nSeed == itemrecord[i].nSeed && wCI == itemrecord[i].wCI && nIndex == itemrecord[i].nIndex) {
			NextItemRecord(i);
			break;
		}
	}
}

#ifdef _DEBUG
// Seeded (audit, 2026-09-19): default-constructed it was the fixed 5489, so the first random spawn after
// every launch was the same item - the fixture the random spawners were written to remove.
std::mt19937 BetterRng { std::random_device {}() };

// The "drop" debug command picks a uniformly random 1-63 "monster level" purely to select
// which item to generate (RndItemForMonsterLevel) and never checks whether that value could
// plausibly have come from a real monster or dungeon level. That raw value is stamped into
// _iCreateInfo and shipped to the network layer unchanged, but OnPutItem/OnDropItem's loopback
// validation (IsPItemValid/IsDungeonItemValid in msg.cpp) rejects any dungeon item whose level
// doesn't exactly match a real monster's level and also exceeds the ~30/34 dungeon-depth
// fallback ceiling - true for roughly half of the 1-63 range. A rejected item is placed in the
// world locally (so it's visible, can be picked up, equipped, etc.) but vanishes with "sent an
// invalid packet" the moment it's actually dropped back onto the ground, since the loopback
// rejection silently discards it instead of ever calling PlaceItemInWorld. This mirrors just
// enough of IsPItemValid to predict that outcome ahead of time.
//
// 2026-09-12: IsPItemValid now returns early in single player, so in a single-player game nothing
// is rejected any more and this predicate is PESSIMISTIC rather than wrong - it can only make the
// `drop` command reroll a level that would in fact have been accepted. Deliberately left mirroring
// the RULES rather than the early-out: it is the rules this is a predicate about, it is the rules
// that must be taught the area ladder if multiplayer ever returns, and the tests in items_test.cpp
// use it to pin exactly that.
bool WouldSurviveNetworkValidation(const Item &item, _item_indexes idx)
{
	if (idx != IDI_GOLD && !IsCreationFlagComboValid(item._iCreateInfo))
		return false;
	if ((item._iCreateInfo & CF_TOWN) != 0)
		return IsTownItemValid(item._iCreateInfo);
	if ((item._iCreateInfo & CF_USEFUL) == CF_UPER15)
		return IsUniqueMonsterItemValid(item._iCreateInfo, item.dwBuff);
	if ((item.dwBuff & CF_HELLFIRE) != 0 && AllItemsList[idx].iMiscId == IMISC_BOOK)
		return true; // reconstructed unconditionally by RecreateHellfireSpellBook, see msg.cpp
	return IsDungeonItemValid(item._iCreateInfo, item.dwBuff);
}

/**
 * @brief Oracool: does @p testItem answer to a debug console `{name}` query?
 *
 * Matches the item's OWN name or its BASE type's name, and the base half is the point. Until
 * v1.11.101 a magic item was called "Amber Helm of harmony", so `drop helm` found one by finding
 * "helm" inside that. Magic and tiered items are named out of the pool now - "Rotting Bane" - and
 * carry their base type nowhere in the string, so matching `_iIName` alone would leave `drop helm`
 * and every other base-type query finding nothing but plain, unrolled items.
 *
 * @param lowerQuery Already lowercased by the caller, as both search loops do once up front.
 */
bool DebugItemNameMatches(const Item &testItem, _item_indexes idx, const std::string &lowerQuery)
{
	if (AsciiStrToLower(string_view(testItem._iIName)).find(lowerQuery) != std::string::npos)
		return true;
	return AsciiStrToLower(string_view(_(AllItemsList[idx].iName))).find(lowerQuery) != std::string::npos;
}

std::string DebugSpawnItem(std::string itemName)
{
	if (ActiveItemCount >= MAXITEMS)
		return "No space to generate the item!";

	AsciiStrToLower(itemName);

	// Oracool bug fix: user report - "are you sure about this command" for `drop Iron Helm`. The
	// random-reroll search below can NEVER find any of our own items: they are IDROP_NEVER on
	// purpose (see the AllItemsList comment above IDI_ORACOOL_SHOULDERS), and
	// GetItemIndexForDroppableItem - the shared chokepoint RndItemForMonsterLevel calls into -
	// unconditionally skips IDROP_NEVER entries. The six worn types have a separate way in
	// (givebset and friends, via FirstBaseItemForEquipLocation's IsOracoolEquipLocation bypass),
	// but that bypass is scoped to their six new ILOC_* locations and does nothing for
	// IDI_ORACOOL_HELM, which sits at the ordinary, already-occupied ILOC_HELM - so it had no path
	// in at all.
	//
	// Deliberately a FALLBACK, tried only once the search below has already given up - not a
	// first check. Several of our names (a substring like "helm") legitimately overlap vanilla
	// item names, and checking first would make `drop helm` steal every future query for our
	// single Iron Helm instead of the vanilla Helm/Full Helm/Great Helm the search below already
	// finds correctly. Running after preserves that existing behaviour exactly - our items only
	// get a look in on a query the search below could never have satisfied anyway.
	const auto trySpawnOracoolItem = [&itemName]() -> std::optional<std::string> {
		for (std::underlying_type_t<_item_indexes> idx = IDI_ORACOOL_SHOULDERS; idx <= IDI_ORACOOL_SPECTRAL_HELM; idx++) {
			std::string candidateName = AsciiStrToLower(_(AllItemsList[idx].iName));
			if (candidateName.find(itemName) == std::string::npos)
				continue;

			const int lvl = std::clamp(static_cast<int>(MyPlayer->_pLevel), 1, 30);
			Item item = {};
			GetItemAttrs(item, static_cast<_item_indexes>(idx), lvl);
			item._iCreateInfo = lvl;
			item._iSeed = AdvanceRndSeed();
			SetupItem(item);
			item._iIdentified = true;

			const int ii = AllocateItem();
			Items[ii] = item.pop();
			Point pos = MyPlayer->position.tile;
			GetSuperItemSpace(pos, ii);
			NetSendCmdPItem(false, CMD_SPAWNITEM, Items[ii].position, Items[ii]);
			return "Item generated successfully.";
		}
		return std::nullopt;
	};

	const int max_time = 3000;
	const int max_iter = 1000000;

	Item testItem;

	uint32_t begin = SDL_GetTicks();
	int i = 0;
	for (;; i++) {
		// using a better rng here to seed the item to prevent getting stuck repeating same values using old one
		std::uniform_int_distribution<int32_t> dist(0, INT_MAX);
		SetRndSeed(dist(BetterRng));
		if (SDL_GetTicks() - begin > max_time) {
			if (std::optional<std::string> result = trySpawnOracoolItem())
				return *result;
			return StrCat("Item not found in ", max_time / 1000, " seconds!");
		}

		if (i > max_iter) {
			if (std::optional<std::string> result = trySpawnOracoolItem())
				return *result;
			return StrCat("Item not found in ", max_iter, " tries!");
		}

		const int8_t monsterLevel = dist(BetterRng) % CF_LEVEL + 1;
		_item_indexes idx = RndItemForMonsterLevel(monsterLevel);
		if (IsAnyOf(idx, IDI_NONE, IDI_GOLD))
			continue;

		testItem = {};
		SetupAllItems(*MyPlayer, testItem, idx, AdvanceRndSeed(), monsterLevel, 1, false, false, false);

		if (!DebugItemNameMatches(testItem, idx, itemName))
			continue;
		if (!WouldSurviveNetworkValidation(testItem, idx))
			continue;
		break;
	}

	int ii = AllocateItem();
	auto &item = Items[ii];
	item = testItem.pop();
	item._iIdentified = true;
	Point pos = MyPlayer->position.tile;
	GetSuperItemSpace(pos, ii);
	NetSendCmdPItem(false, CMD_SPAWNITEM, item.position, item);
	return StrCat("Item generated successfully - iterations: ", i);
}

/**
 * @brief Oracool: user request - giverare/giveunique/giveprimal debug commands. Same random-base-item
 * search loop as DebugSpawnItem, but forces the requested Oracool tier via SetupAllItems's
 * forcedTier parameter instead of leaving it to chance, retrying with a different base item
 * whenever the picked one can't carry tiered affixes at all (e.g. potions, scrolls, gold).
 */
std::string DebugSpawnTieredItem(std::string itemName, OracoolItemTier tier)
{
	if (ActiveItemCount >= MAXITEMS)
		return "No space to generate the item!";

	const int max_time = 3000;
	const int max_iter = 1000000;

	AsciiStrToLower(itemName);

	Item testItem;

	uint32_t begin = SDL_GetTicks();
	int i = 0;
	for (;; i++) {
		std::uniform_int_distribution<int32_t> dist(0, INT_MAX);
		SetRndSeed(dist(BetterRng));
		if (SDL_GetTicks() - begin > max_time)
			return StrCat("Item not found in ", max_time / 1000, " seconds!");

		if (i > max_iter)
			return StrCat("Item not found in ", max_iter, " tries!");

		const int8_t monsterLevel = dist(BetterRng) % CF_LEVEL + 1;
		_item_indexes idx = RndItemForMonsterLevel(monsterLevel);
		if (IsAnyOf(idx, IDI_NONE, IDI_GOLD))
			continue;

		testItem = {};
		SetupAllItems(*MyPlayer, testItem, idx, AdvanceRndSeed(), monsterLevel, 1, false, false, false, /*allowTieredRoll=*/true, tier);

		if (testItem._iOracoolTier != tier)
			continue; // this base item type can't carry tiered affixes - try another

		if (!DebugItemNameMatches(testItem, idx, itemName))
			continue;
		if (!WouldSurviveNetworkValidation(testItem, idx))
			continue;
		break;
	}

	int ii = AllocateItem();
	auto &item = Items[ii];
	item = testItem.pop();
	item._iIdentified = true;
	Point pos = MyPlayer->position.tile;
	GetSuperItemSpace(pos, ii);
	NetSendCmdPItem(false, CMD_SPAWNITEM, item.position, item);
	return StrCat("Item generated successfully - iterations: ", i);
}

/**
 * @brief Oracool (2026-09-19): one random item of a plain quality - `givebasic`, `givemagic`.
 *
 * DebugSpawnTieredItem's search loop with a different acceptance test: the roll must land on
 * exactly @p quality, on no Oracool tier and not ethereal, and the base must be something worn or
 * wielded - a potion is ITEM_QUALITY_NORMAL too, and is not what "a basic item" means to anyone.
 * Asked for so that every quality has ONE command that drops ONE random item of it; `drop` alone
 * rolls whatever the dungeon would, and the *set commands drop thirteen.
 */
std::string DebugSpawnQualityItem(std::string itemName, item_quality quality)
{
	if (ActiveItemCount >= MAXITEMS)
		return "No space to generate the item!";

	const int max_time = 3000;
	const int max_iter = 1000000;

	AsciiStrToLower(itemName);

	Item testItem;

	uint32_t begin = SDL_GetTicks();
	int i = 0;
	for (;; i++) {
		std::uniform_int_distribution<int32_t> dist(0, INT_MAX);
		SetRndSeed(dist(BetterRng));
		if (SDL_GetTicks() - begin > max_time)
			return StrCat("Item not found in ", max_time / 1000, " seconds!");

		if (i > max_iter)
			return StrCat("Item not found in ", max_iter, " tries!");

		const int8_t monsterLevel = dist(BetterRng) % CF_LEVEL + 1;
		_item_indexes idx = RndItemForMonsterLevel(monsterLevel);
		if (IsAnyOf(idx, IDI_NONE, IDI_GOLD))
			continue;
		if (IsAnyOf(AllItemsList[idx].iLoc, ILOC_UNEQUIPABLE, ILOC_BELT))
			continue;

		testItem = {};
		SetupAllItems(*MyPlayer, testItem, idx, AdvanceRndSeed(), monsterLevel, 1, false, false, false);

		if (testItem._iMagical != quality || testItem._iOracoolTier != OracoolItemTier::None || testItem._iOracoolEthereal)
			continue;
		if (!DebugItemNameMatches(testItem, idx, itemName))
			continue;
		if (!WouldSurviveNetworkValidation(testItem, idx))
			continue;
		break;
	}

	int ii = AllocateItem();
	auto &item = Items[ii];
	item = testItem.pop();
	item._iIdentified = true;
	Point pos = MyPlayer->position.tile;
	GetSuperItemSpace(pos, ii);
	NetSendCmdPItem(false, CMD_SPAWNITEM, item.position, item);
	return StrCat("Item generated successfully - iterations: ", i);
}

/**
 * @brief Oracool (2026-09-19): one random piece of a named set - `giveset ({name})`.
 *
 * The name matches the piece or its set, so `giveset ashen` is any Ashen piece. Built the way
 * givesset builds its thirteen (InitializeItem + MakeSetItem), dropped at the feet rather than
 * placed in the pack, and through FinishOracoolDrop because nothing here ran SetupAllItems.
 */
std::string DebugSpawnSetPiece(string_view parameter)
{
	if (ActiveItemCount >= MAXITEMS)
		return "No space to generate the item!";

	const std::string wanted = AsciiStrToLower(std::string { parameter });
	std::vector<size_t> candidates;
	for (size_t s = 0; s < oracool::ItemSetCount; s++) {
		const oracool::ItemSetDefinition &set = oracool::ItemSets[s];
		const std::string setName = AsciiStrToLower(std::string { set.name });
		for (int i = 0; i < set.itemCount; i++) {
			const size_t pieceIdx = static_cast<size_t>(set.firstItem + i);
			const oracool::SetItemDefinition &def = oracool::ItemSetItems[pieceIdx];
			if (oracool::BaseItemForSetPiece(def) < 0)
				continue; // no base for that slot in this fork - giveitemset reports these
			if (!wanted.empty()) {
				const std::string pieceName = AsciiStrToLower(std::string { def.name });
				if (pieceName.find(wanted) == std::string::npos && setName.find(wanted) == std::string::npos)
					continue;
			}
			candidates.push_back(pieceIdx);
		}
	}
	if (candidates.empty())
		return wanted.empty() ? "No set piece has a base item." : "No set or set piece matching that name.";

	std::uniform_int_distribution<size_t> pick(0, candidates.size() - 1);
	const oracool::SetItemDefinition &def = oracool::ItemSetItems[candidates[pick(BetterRng)]];

	Item item {};
	InitializeItem(item, static_cast<_item_indexes>(oracool::BaseItemForSetPiece(def)));
	oracool::MakeSetItem(item, def);
	FinalizeSetPiece(item, std::max<int>(def.requiredLevel, MyPlayer->_pLevel), /*allowEtherealRoll=*/false); // round 13 audit
	item._iIdentified = true;

	const int ii = AllocateItem();
	Items[ii] = item.pop();
	FinishOracoolDrop(ii, MyPlayer->position.tile);
	NetSendCmdPItem(false, CMD_SPAWNITEM, Items[ii].position, Items[ii]);
	return StrCat("Dropped ", Items[ii]._iIName, ".");
}

/**
 * @brief Oracool (2026-09-19): one formed runeword on a random fitting base - `giverw ({word})`.
 *
 * Picks a word (any, or one whose name contains @p parameter), then a plain base of the word's
 * host type that can hold that many sockets, seats the runes in the word's order and lets
 * TryCompleteRuneword name it - the same check the socket UI runs, so what drops here is exactly
 * what a player would have built rune by rune.
 */
std::string DebugSpawnRuneword(string_view parameter)
{
	if (ActiveItemCount >= MAXITEMS)
		return "No space to generate the item!";

	const std::string wanted = AsciiStrToLower(std::string { parameter });
	std::vector<const oracool::RunewordDefinition *> words;
	for (size_t w = 0; w < oracool::RunewordCount(); w++) {
		const oracool::RunewordDefinition *word = oracool::RunewordAt(w);
		if (!wanted.empty() && AsciiStrToLower(std::string { word->name }).find(wanted) == std::string::npos)
			continue;
		words.push_back(word);
	}
	if (words.empty())
		return "No runeword matching that name.";
	std::uniform_int_distribution<size_t> pickWord(0, words.size() - 1);
	const oracool::RunewordDefinition &word = *words[pickWord(BetterRng)];
	const int runeCount = word.runeCount;

	std::vector<_item_indexes> bases;
	for (std::underlying_type_t<_item_indexes> i = IDI_GOLD; i <= IDI_LAST; ++i) {
		if (!IsItemAvailable(i))
			continue;
		// Not a quest unique's base row (The Undead Crown, Griswold's Edge...): those are IMISC_UNIQUE
		// and would come out as a plain-quality helm wearing a runeword's name (audit, 2026-09-19).
		if (AllItemsList[i].iMiscId == IMISC_UNIQUE)
			continue;
		if (static_cast<uint8_t>(oracool::RunewordHostForItemType(AllItemsList[i].itype)) != word.host)
			continue;
		Item probe;
		GetItemAttrs(probe, static_cast<_item_indexes>(i), 1);
		probe._iCreateInfo = 1;
		probe._iSeed = AdvanceRndSeed();
		SetupItem(probe);
		if (!oracool::CanItemHaveSockets(probe) || oracool::MaxSocketsForItem(probe) < runeCount)
			continue;
		bases.push_back(static_cast<_item_indexes>(i));
	}
	if (bases.empty())
		return StrCat("No base can hold ", word.name, "'s ", runeCount, " runes.");
	std::uniform_int_distribution<size_t> pickBase(0, bases.size() - 1);

	Item item;
	GetItemAttrs(item, bases[pickBase(BetterRng)], 1);
	item._iCreateInfo = 1;
	item._iSeed = AdvanceRndSeed();
	SetupItem(item);
	item._iSocketCount = static_cast<uint8_t>(runeCount);
	for (int r = 0; r < runeCount; r++)
		item._iSocketed[r] = word.runes[r];
	if (!oracool::TryCompleteRuneword(item))
		return StrCat("The runes did not form ", word.name, " on ", item._iIName, ".");

	const int ii = AllocateItem();
	Items[ii] = item.pop();
	FinishOracoolDrop(ii, MyPlayer->position.tile);
	NetSendCmdPItem(false, CMD_SPAWNITEM, Items[ii].position, Items[ii]);
	return StrCat("Dropped ", Items[ii]._iIName, " (", runeCount, " runes).");
}

/**
 * @brief Oracool: user request - give{b,m,r,u,p}set. One item for every equipment slot at once.
 *
 * Picks a base item per slot by walking AllItemsList for the first entry with the right iLoc,
 * rather than hardcoding thirteen indices: those indices are positional and shift whenever the
 * table gains a row, which is exactly what this feature has been doing to it.
 *
 * IDROP_NEVER entries are skipped so a slot does not land on a quest or unique base (ILOC_HELM's
 * first match is The Undead Crown) - except for the six worn types, which are all IDROP_NEVER on
 * purpose right now and would otherwise be unreachable from here.
 */
_item_indexes FirstBaseItemForEquipLocation(item_equip_type loc, string_view namePrefix)
{
	if (!namePrefix.empty()) {
		// Tier-prefix mode (user report: "all assets seem to be of the same type" - correct,
		// first-in-table could only ever surface the leather tier). Every tiered item is
		// IDROP_NEVER by design, so the drop-rate skip in the plain path below must not apply
		// here; the prefix itself is the selector instead.
		//
		// Two passes, Oracool items first, because item names are NOT unique across the table:
		// IDI_ORACOOL_LEATHER_ARMOR is called "Leather Armor" and so is vanilla's own index 59,
		// which sits far earlier - a single pass silently handed back the vanilla item, i.e. the
		// old icon, for `give*set leather` (caught by items_test, not by reading the code). These
		// commands exist to exercise Oracool's own items, so ours win any name collision; the
		// second pass keeps vanilla-only materials reachable.
		for (const bool oracoolOnly : { true, false }) {
			for (std::underlying_type_t<_item_indexes> i = IDI_GOLD; i <= IDI_LAST; i++) {
				if (!IsItemAvailable(i))
					continue;
				if (oracoolOnly && !IsOracoolItemIdx(i))
					continue;
				const ItemData &data = AllItemsList[i];
				if (data.iLoc != loc)
					continue;
				const std::string name = AsciiStrToLower(_(data.iName));
				if (name.compare(0, namePrefix.size(), namePrefix) != 0)
					continue;
				return static_cast<_item_indexes>(i);
			}
		}
		return IDI_NONE;
	}

	for (std::underlying_type_t<_item_indexes> i = IDI_GOLD; i <= IDI_LAST; i++) {
		if (!IsItemAvailable(i))
			continue;
		const ItemData &data = AllItemsList[i];
		if (data.iLoc != loc)
			continue;
		if (data.iRnd == IDROP_NEVER && !IsOracoolEquipLocation(loc))
			continue;
		return static_cast<_item_indexes>(i);
	}
	return IDI_NONE;
}


/**
 * @brief Drops one plain item per index in [first, last] that @p keep accepts, at the player's feet.
 *
 * Oracool: user request (2026-08-19) - "we have introduced a lot of items lately - socketed items,
 * gems, runes... i need debug commands to spawn them to test them." Runes, gems and charms are all
 * ordinary AllItemsList rows in contiguous-ish index islands, so one loop with a predicate serves
 * all three rather than three near-identical functions.
 *
 * By INDEX, not by name. `drop {name}` searches by rerolling random drops until one matches, which
 * cannot reach an item reliably and cannot reach an IDROP_NEVER row at all; these families need to
 * be spawnable on demand, in full, to be worth testing against.
 *
 * Plain attributes with no affix roll - the same path givebset uses for its basic set. A gem or rune
 * IS its index; there is nothing to roll.
 */
std::string DebugSpawnByIndex(int first, int last, tl::function_ref<bool(int)> keep, string_view what)
{
	int spawned = 0;
	int skippedNoSpace = 0;
	for (int i = first; i <= last; i++) {
		if (!keep(i))
			continue;
		const auto idx = static_cast<_item_indexes>(i);
		if (!IsItemAvailable(i))
			continue;
		if (ActiveItemCount >= MAXITEMS) {
			skippedNoSpace++;
			continue;
		}

		Item item;
		GetItemAttrs(item, idx, 1);
		item._iCreateInfo = 1;
		item._iSeed = AdvanceRndSeed();
		SetupItem(item);

		const int ii = AllocateItem();
		Items[ii] = item.pop();
		Items[ii]._iIdentified = true;
		Point pos = MyPlayer->position.tile;
		GetSuperItemSpace(pos, ii);
		NetSendCmdPItem(false, CMD_SPAWNITEM, Items[ii].position, Items[ii]);
		spawned++;
	}

	if (spawned == 0)
		return StrCat("No ", what, " could be spawned.");
	if (skippedNoSpace > 0)
		return StrCat("Dropped ", spawned, " ", what, "; ", skippedNoSpace, " skipped - item table full.");
	return StrCat("Dropped ", spawned, " ", what, ".");
}

std::string DebugSpawnRunes()
{
	// Both islands: the five that shipped in 1.7.8 keep their original indices and the other 28 are
	// appended at the end, so the predicate is what defines the family rather than a single range.
	return DebugSpawnByIndex(IDI_ORACOOL_RUNE_EL, IDI_ORACOOL_RUNE_ZOD,
	    [](int i) { return IsOracoolRuneIdx(i); }, "runes");
}

std::string DebugSpawnGems(string_view quality)
{
	// No argument: every gem at every quality, which is what you want when checking the socket
	// effects table. With one, a quality-name prefix ("givegems perfect") filters to that rung.
	std::string wanted { quality };
	AsciiStrToLower(wanted);
	return DebugSpawnByIndex(IDI_ORACOOL_GEM_RUBY, IDI_ORACOOL_GEM_SKULL_PERFECT,
	    [&wanted](int i) {
		    if (!IsOracoolGemIdx(i))
			    return false;
		    if (wanted.empty())
			    return true;
		    std::string name = AsciiStrToLower(std::string { AllItemsList[i].iName });
		    return name.find(wanted) != std::string::npos;
	    },
	    wanted.empty() ? "gems" : "gems of that quality");
}

std::string DebugSpawnCharms()
{
	// IDI_LAST, not IDI_ORACOOL_CHARM_GREED: the Charms of Salvaging sit past GREED, and a debug
	// command that quietly stops at an old boundary is how a new family goes untested.
	return DebugSpawnByIndex(IDI_ORACOOL_CHARM_VIGOR, IDI_LAST,
	    [](int i) { return IsOracoolCharmIdx(i); }, "charms");
}


/**
 * @brief Spawns one fresh base item carrying @p count empty sockets.
 *
 * Oracool: user decision (2026-08-19) - "spawn a fresh base with N sockets", rather than socketing
 * whatever sits under the cursor. A fresh base is the reproducible half of the pair: the item's
 * quality, affixes and footprint are known, so a runeword that fails to form is the runeword's
 * fault and not the host's.
 *
 * BASIC quality on purpose. CanItemHaveSockets refuses anything above basic for everything except
 * jewelry - that is the socketing rule, not an accident of this command - so rolling affixes here
 * would produce items the socket system would then decline to accept.
 *
 * The base is chosen by footprint: the first available row that can hold sockets AND whose own
 * ceiling reaches @p count, since MaxSocketsForItem is the item's 28x28 cell count and a 1x1 ring
 * can never hold six. An optional name prefix narrows the search when a particular host is wanted -
 * "givesockets 4 great sword" for a runeword that needs a specific base.
 */
std::string DebugSpawnSocketedBase(string_view parameter)
{
	if (ActiveItemCount >= MAXITEMS)
		return "No space to generate the item!";

	// "{count} {optional name}". A missing or unparsable count means "as many as the base allows",
	// which is the common case when the point is to test the socket UI rather than a recipe.
	int wantedCount = Item::MaxItemSockets;
	std::string namePrefix;
	{
		std::string arg { parameter };
		const size_t space = arg.find(' ');
		const std::string first = arg.substr(0, space);
		if (!first.empty() && std::all_of(first.begin(), first.end(), [](unsigned char c) { return std::isdigit(c) != 0; })) {
			wantedCount = std::atoi(first.c_str());
			if (space != std::string::npos)
				namePrefix = arg.substr(space + 1);
		} else {
			namePrefix = arg;
		}
	}
	AsciiStrToLower(namePrefix);
	wantedCount = std::clamp(wantedCount, 1, static_cast<int>(Item::MaxItemSockets));

	// A RANDOM candidate, not the first in the table (2026-09-19) - see DebugSpawnEthereal.
	std::vector<_item_indexes> candidates;
	for (std::underlying_type_t<_item_indexes> i = IDI_GOLD; i <= IDI_LAST; ++i) {
		if (!IsItemAvailable(i))
			continue;
		if (AllItemsList[i].iMiscId == IMISC_UNIQUE)
			continue; // a quest unique's base row, not a base (audit, 2026-09-19)
		if (!namePrefix.empty()) {
			std::string name = AsciiStrToLower(std::string { AllItemsList[i].iName });
			if (name.find(namePrefix) == std::string::npos)
				continue;
		}
		Item probe;
		GetItemAttrs(probe, static_cast<_item_indexes>(i), 1);
		probe._iCreateInfo = 1;
		probe._iSeed = AdvanceRndSeed();
		SetupItem(probe);
		if (!oracool::CanItemHaveSockets(probe) || oracool::MaxSocketsForItem(probe) < wantedCount)
			continue;
		candidates.push_back(static_cast<_item_indexes>(i));
	}

	if (!candidates.empty()) {
		std::uniform_int_distribution<size_t> pick(0, candidates.size() - 1);
		Item item;
		GetItemAttrs(item, candidates[pick(BetterRng)], 1);
		item._iCreateInfo = 1;
		item._iSeed = AdvanceRndSeed();
		SetupItem(item);
		item._iSocketCount = static_cast<uint8_t>(wantedCount);
		item._iIdentified = true;

		const int ii = AllocateItem();
		Items[ii] = item.pop();
		Point pos = MyPlayer->position.tile;
		GetSuperItemSpace(pos, ii);
		NetSendCmdPItem(false, CMD_SPAWNITEM, Items[ii].position, Items[ii]);
		return StrCat("Dropped ", Items[ii]._iIName, " with ", wantedCount, " sockets.");
	}

	// Says which constraint failed. "No base found" alone would send you looking for a typo when
	// the real answer is that nothing 1x1 can hold four.
	if (!namePrefix.empty())
		return StrCat("No socketable base matching that name holds ", wantedCount, " sockets.");
	return StrCat("No socketable base holds ", wantedCount, " sockets.");
}


/**
 * @brief Spawns one ethereal item, optionally the first base whose name contains @p parameter.
 *
 * Oracool: user request (2026-08-20). Ethereal is a 5% roll on ordinary drops, so waiting for one
 * to test the repair refusal, the damage bonus and the halved durability is not a plan.
 *
 * Goes through MakeItemEthereal, the same function the drop path calls, so this cannot produce an
 * item the game could not - a potion cannot be made ethereal here any more than it can out there,
 * and the 135% comes from one place.
 *
 * Basic quality, matching givesockets: the point is to see the ethereal bargain applied to known
 * numbers. An affix roll on top would make the damage line harder to read, not more useful.
 */
std::string DebugSpawnEthereal(string_view parameter)
{
	if (ActiveItemCount >= MAXITEMS)
		return "No space to generate the item!";

	std::string wanted { parameter };
	AsciiStrToLower(wanted);

	// A RANDOM candidate, not the first in the table (2026-09-19): with no name this dropped the
	// same base every time, which made it a fixture rather than a spawner. Every base that can be
	// ethereal (and matches the name, if any) is a candidate; one is drawn.
	std::vector<_item_indexes> candidates;
	for (std::underlying_type_t<_item_indexes> i = IDI_GOLD; i <= IDI_LAST; ++i) {
		if (!IsItemAvailable(i))
			continue;
		if (AllItemsList[i].iMiscId == IMISC_UNIQUE)
			continue; // a quest unique's base row, not a base (audit, 2026-09-19)
		if (!wanted.empty()) {
			std::string name = AsciiStrToLower(std::string { AllItemsList[i].iName });
			if (name.find(wanted) == std::string::npos)
				continue;
		}
		Item probe;
		GetItemAttrs(probe, static_cast<_item_indexes>(i), 1);
		probe._iCreateInfo = 1;
		probe._iSeed = AdvanceRndSeed();
		SetupItem(probe);
		if (MakeItemEthereal(probe))
			candidates.push_back(static_cast<_item_indexes>(i));
	}
	if (candidates.empty()) {
		if (!wanted.empty())
			return "No durable weapon or armour matching that name - ethereal needs one of those.";
		return "No durable weapon or armour available.";
	}

	std::uniform_int_distribution<size_t> pick(0, candidates.size() - 1);
	Item item;
	GetItemAttrs(item, candidates[pick(BetterRng)], 1);
	item._iCreateInfo = 1;
	item._iSeed = AdvanceRndSeed();
	SetupItem(item);
	MakeItemEthereal(item);
	item._iIdentified = true;

	const int ii = AllocateItem();
	Items[ii] = item.pop();
	Point pos = MyPlayer->position.tile;
	GetSuperItemSpace(pos, ii);
	NetSendCmdPItem(false, CMD_SPAWNITEM, Items[ii].position, Items[ii]);
	return StrCat("Dropped ethereal ", Items[ii]._iIName, ".");
}

std::string DebugSpawnEquipmentSet(std::optional<OracoolItemTier> tier, bool magical, string_view namePrefix, bool ethereal)
{
	// One per slot. Rings are the only slot pair sharing an item location, so ILOC_RING appears
	// twice - the set is thirteen items, matching the thirteen paperdoll slots, not thirteen
	// distinct item locations.
	static const item_equip_type SlotLocations[] = {
		ILOC_HELM, ILOC_AMULET, ILOC_ARMOR, ILOC_ONEHAND, ILOC_ONEHAND,
		ILOC_RING, ILOC_RING,
		ILOC_SHOULDERS, ILOC_BRACERS, ILOC_GLOVES, ILOC_WAIST, ILOC_LEGS, ILOC_BOOTS
	};
	constexpr int SlotCount = sizeof(SlotLocations) / sizeof(SlotLocations[0]);

	const std::string prefixLower = AsciiStrToLower(namePrefix);

	if (ActiveItemCount + SlotCount > MAXITEMS)
		return "Not enough free item slots on this level for a whole set.";

	// Oracool bug fix: user report - every spawn produced "sent an invalid packet" spam. The spawn
	// message loops back through the network layer even in single player, and IsDungeonItemValid
	// rejects any item level above 30 that matches no monster's level - which the raw _pLevel of
	// any high-level character is. 30 is the highest level valid everywhere.
	const int lvl = std::clamp(static_cast<int>(MyPlayer->_pLevel), 1, 30);
	int spawned = 0;
	int missingBase = 0;

	for (item_equip_type loc : SlotLocations) {
		const _item_indexes idx = FirstBaseItemForEquipLocation(loc, prefixLower);
		if (idx == IDI_NONE) {
			missingBase++;
			continue;
		}

		Item item;
		if (!magical && !tier) {
			// Plain: base attributes only, no affix roll at all. SetupAllItems always rolls
			// something, so the basic set skips it and finishes the item by hand instead.
			GetItemAttrs(item, idx, lvl);
			item._iCreateInfo = lvl;
			item._iSeed = AdvanceRndSeed();
			SetupItem(item);
		} else {
			SetupAllItems(*MyPlayer, item, idx, AdvanceRndSeed(), lvl, 1, /*onlygood=*/true,
			    /*recreate=*/false, /*pregen=*/false, /*allowTieredRoll=*/true,
			    tier ? tier : std::optional<OracoolItemTier> { OracoolItemTier::None });
		}

		// Ethereal is a stamp on top of a finished item, not a quality of its own, so it goes on
		// LAST - the same order the drop path uses, where TryMakeDroppedItemEthereal runs after
		// SetupAllItems. MakeItemEthereal carries the whole bargain (+35% AC or max damage, max
		// durability halved), so nothing here has to know what ethereal means, and it declines the
		// items that cannot be ethereal on its own.
		if (ethereal)
			MakeItemEthereal(item);

		const int ii = AllocateItem();
		Items[ii] = item.pop();
		Items[ii]._iIdentified = true;
		Point pos = MyPlayer->position.tile;
		GetSuperItemSpace(pos, ii);
		NetSendCmdPItem(false, CMD_SPAWNITEM, Items[ii].position, Items[ii]);
		spawned++;
	}

	// Reported rather than assumed. GetSuperItemSpace widens its search until it finds a free
	// tile, so thirteen items do fit around the player in the open - but a corridor or a doorway
	// is a different matter, and silently dropping nine of thirteen would be worse than saying so.
	//
	// The message names the material, or says "default" and lists the alternatives when none was
	// given: without that, a plain `giveuset` and a `giveuset diamond` that silently fell back
	// look identical on screen - which is exactly how the missing prefix went unnoticed through a
	// whole play session (user report: "everytime the same set of items drops").
	const std::string what = prefixLower.empty()
	    ? std::string("default (leather) tier - try: givebset iron|steel|crusader|bone|royal|obsidian|infernal|diamond")
	    : StrCat(prefixLower, " tier");
	if (missingBase > 0)
		return StrCat("Spawned ", spawned, " of ", SlotCount, ", ", what, " - ", missingBase, " slot(s) have no item at that tier.");
	return StrCat("Spawned ", spawned, " items, ", what, ".");
}

std::string DebugSpawnUniqueItem(std::string itemName)
{
	if (ActiveItemCount >= MAXITEMS)
		return "No space to generate the item!";

	AsciiStrToLower(itemName);

	// No name means a RANDOM unique (2026-09-19). The empty string matched the first entry of the
	// table, so `dropu` alone dropped the same item every time - a fixture, not a spawner.
	UniqueItem uniqueItem;
	bool foundUnique = false;
	int uniqueIndex = 0;
	if (itemName.empty()) {
		std::vector<int> available;
		for (int j = 0; UniqueItems[j].UIItemId != UITYPE_INVALID; j++) {
			if (!IsUniqueAvailable(j))
				break;
			available.push_back(j);
		}
		if (available.empty())
			return "No unique found!";
		std::uniform_int_distribution<size_t> pick(0, available.size() - 1);
		// The drawn INDEX, not its name through the substring search below: "Black Meridian" is
		// inside "Black Meridian Robe" and "The Long Vigil" inside "Hood of the Long Vigil", so a
		// name would resolve to the wrong unique (audit, 2026-09-19).
		uniqueIndex = available[pick(BetterRng)];
		uniqueItem = UniqueItems[uniqueIndex];
		itemName = AsciiStrToLower(uniqueItem.UIName);
		foundUnique = true;
	}
	for (int j = 0; !foundUnique && UniqueItems[j].UIItemId != UITYPE_INVALID; j++) {
		if (!IsUniqueAvailable(j))
			break;

		const std::string tmp = AsciiStrToLower(UniqueItems[j].UIName);
		if (tmp.find(itemName) != std::string::npos) {
			itemName = tmp;
			uniqueItem = UniqueItems[j];
			uniqueIndex = j;
			foundUnique = true;
			break;
		}
	}
	if (!foundUnique)
		return "No unique found!";

	_item_indexes uniqueBaseIndex = IDI_GOLD;
	for (std::underlying_type_t<_item_indexes> j = IDI_GOLD; j <= IDI_LAST; j++) {
		if (!IsItemAvailable(j))
			continue;
		if (AllItemsList[j].iItemId == uniqueItem.UIItemId) {
			uniqueBaseIndex = static_cast<_item_indexes>(j);
			break;
		}
	}

	if (uniqueBaseIndex == IDI_GOLD)
		return "Base item not available";

	auto &baseItemData = AllItemsList[static_cast<size_t>(uniqueBaseIndex)];

	Item testItem;

	int i = 0;
	for (uint32_t begin = SDL_GetTicks();; i++) {
		constexpr int max_time = 3000;
		if (SDL_GetTicks() - begin > max_time)
			return StrCat("Item not found in ", max_time / 1000, " seconds!");

		constexpr int max_iter = 1000000;
		if (i > max_iter)
			return StrCat("Item not found in ", max_iter, " tries!");

		testItem = {};
		testItem._iMiscId = baseItemData.iMiscId;
		std::uniform_int_distribution<int32_t> dist(0, INT_MAX);
		SetRndSeed(dist(BetterRng));
		// The game's record of found uniques is kept (round 13 audit): it was cleared to all-false after every try.
		std::array<bool, MaxUniqueItems> uniquesBefore;
		std::copy(std::begin(UniqueItemFlags), std::end(UniqueItemFlags), uniquesBefore.begin());
		for (auto &flag : UniqueItemFlags)
			flag = true;
		UniqueItemFlags[uniqueIndex] = false;
		SetupAllItems(*MyPlayer, testItem, uniqueBaseIndex, testItem._iMiscId == IMISC_UNIQUE ? uniqueIndex : AdvanceRndSeed(), uniqueItem.UIMinLvl, 1, false, false, false);
		std::copy(uniquesBefore.begin(), uniquesBefore.end(), std::begin(UniqueItemFlags));
		if (testItem._iMagical == ITEM_QUALITY_UNIQUE)
			UniqueItemFlags[uniqueIndex] = true;

		if (testItem._iMagical != ITEM_QUALITY_UNIQUE)
			continue;

		const std::string tmp = AsciiStrToLower(testItem._iIName);
		if (tmp.find(itemName) != std::string::npos)
			break;
		return "Impossible to generate!";
	}

	int ii = AllocateItem();
	auto &item = Items[ii];
	item = testItem.pop();
	Point pos = MyPlayer->position.tile;
	GetSuperItemSpace(pos, ii);
	item._iIdentified = true;
	NetSendCmdPItem(false, CMD_SPAWNITEM, item.position, item);
	return StrCat("Item generated successfully - iterations: ", i);
}
#endif

bool Item::isUsable() const
{
	if (IDidx == IDI_SPECELIX && Quests[Q_MUSHROOM]._qactive != QUEST_DONE)
		return false;
	return AllItemsList[IDidx].iUsable;
}

void Item::setNewAnimation(bool showAnimation)
{
	int8_t it = GetItemDropAnimIndexFor(*this);
	int8_t numberOfFrames = ItemAnimLs[it];
	OptionalClxSpriteList sprite = itemanims[it] ? OptionalClxSpriteList { *itemanims[static_cast<size_t>(it)] } : std::nullopt;
	if (_iCurs != ICURS_MAGIC_ROCK)
		AnimInfo.setNewAnimation(sprite, numberOfFrames, 1, AnimationDistributionFlags::ProcessAnimationPending, 0, numberOfFrames);
	else
		AnimInfo.setNewAnimation(sprite, numberOfFrames, 1);
	_iPostDraw = false;
	_iRequest = false;
	if (showAnimation) {
		_iAnimFlag = true;
		_iSelFlag = 0;
	} else {
		AnimInfo.currentFrame = AnimInfo.numberOfFrames - 1;
		_iAnimFlag = false;
		_iSelFlag = 1;
	}
}

void Item::updateRequiredStatsCacheForPlayer(const Player &player)
{
	if (_itype == ItemType::Misc && _iMiscId == IMISC_BOOK) {
		_iMinMag = GetSpellData(_iSpell).minInt;
		int8_t spellLevel = player._pSplLvl[static_cast<int16_t>(_iSpell)];
		while (spellLevel != 0) {
			_iMinMag += 20 * _iMinMag / 100;
			spellLevel--;
			if (_iMinMag + 20 * _iMinMag / 100 > 255) {
				_iMinMag = 255;
				spellLevel = 0;
			}
		}
		// Red when the Rule of Rangs refuses the next rank, as the read refuses it: a book asks no level through
		// CanUseItem, so an unreadable one looked usable until clicked (round 13 audit, v1.12.238).
		const int nextLevel = player._pSplLvl[static_cast<int16_t>(_iSpell)] + 1;
		_iStatFlag = player.CanUseItem(*this) && oracool::CanReadSpellBookTo(player, _iSpell, nextLevel);
		return;
	}
	_iStatFlag = player.CanUseItem(*this);
}

StringOrView Item::getName() const
{
	if (isEmpty()) {
		return string_view("");
	} else if (_iOracoolTier == OracoolItemTier::Set) {
		// A set piece's name is the one MakeSetItem wrote, full stop.
		//
		// Bug (fixed 2026-08-16, user report: a complete Vestments of the Ashen Saint showing as
		// "Leather Armor", "Short Sword", "Iron Helm"...). Without this branch a set item fell into
		// the next one and got GetTranslatedItemName - the BASE name - because MakeSetItem left
		// _iCreateInfo at 0, and this function reads a zero there as "there is no real name here".
		// That is true of an item the roller never touched; it is not true of a named object.
		//
		// Checked before the _iCreateInfo test rather than after, so a set item's name never depends
		// on how it happened to be created.
		return string_view(_iIName);
	} else if (!_iIdentified || _iCreateInfo == 0 || _iMagical == ITEM_QUALITY_NORMAL) {
		// Phase 1 audit fix (2026-08-16): a completed runeword writes its name into _iIName, but
		// this branch returns the BASE name for NORMAL quality - the one quality runewords form
		// on, so "Steel" showed as "Short Sword" everywhere. The word's name wins.
		if (_iMagical == ITEM_QUALITY_NORMAL && oracool::GetActiveRuneword(*this) != nullptr)
			return string_view(_iIName);
		return GetTranslatedItemName(*this);
	} else if (_iMagical == ITEM_QUALITY_UNIQUE) {
		return _(UniqueItems[_iUid].UIName);
	} else {
		// Oracool bug fix: user report - the popup description window (DrawUniqueInfo) and this
		// function used to disagree on a magic item's name (e.g. popup: "Ruby Amulet", HUD:
		// "Amulet of the Tiger", for the exact same single Resist Fire affix). The popup reads
		// _iIName directly - the name cached once at generation time from the item's *real*
		// rolled affixes (see GetItemPower/GetStaffPower/GetTieredItemAffixes). This
		// function instead called GetTranslatedItemNameMagical, which recomputes the name by
		// replaying the RNG from the item's saved seed - a replay that assumes a fixed sequence
		// of random calls no longer matched once Oracool's own tier-roll checks
		// (primalItemDropChance/buffedUniqueItemDropChance/rareItemDropChance in SetupAllItems)
		// started consuming extra randomness before falling through to a plain magic item's
		// real affix roll. That shifted the replay onto the wrong prefix/suffix row - sometimes
		// inventing a suffix that was never rolled, sometimes losing one that was - and, since
		// the replay has no concept of Oracool tiers at all, it also overrode a Rare/Buffed
		// Unique/Primal item's correct tiered name with a fabricated vanilla-style one. Rather
		// than keep two independent name-generation paths in sync forever, just return the same
		// already-correct _iIName the popup uses - matching how the Unique branch above already
		// works (cached, never replayed).
		return string_view(_iIName);
	}
}

bool CornerStoneStruct::isAvailable()
{
	return currlevel == 21 && !gbIsMultiplayer;
}

void initItemGetRecords()
{
	memset(itemrecord, 0, sizeof(itemrecord));
	gnNumGetRecords = 0;
}

void RepairItem(Item &item, int lvl)
{
	// Phase 1 ethereal: the Repair skill/spell is still a smith's hand - it declines too.
	if (item._iOracoolEthereal)
		return;
	// Zod's stamp (durability indestructible, the maximum kept) is not wear: the loop below cut the maximum and wrote
	// the durability back over the stamp, so the rune stayed socketed on a sword that wore down again (round 11 audit).
	if (item._iDurability == item._iMaxDur || item._iDurability == DUR_INDESTRUCTIBLE) {
		return;
	}

	if (item._iMaxDur <= 0) {
		item.clear();
		return;
	}

	int rep = 0;
	do {
		rep += lvl + GenerateRnd(lvl);
		item._iMaxDur -= std::max(item._iMaxDur / (lvl + 9), 1);
		if (item._iMaxDur == 0) {
			item.clear();
			return;
		}
	} while (rep + item._iDurability < item._iMaxDur);

	item._iDurability = std::min<int>(item._iDurability + rep, item._iMaxDur);
	item._iOracoolBroken = false; // mended, so no longer the emptied item left equipped
}

void RechargeItem(Item &item, Player &player)
{
	if (item._itype != ItemType::Staff || !IsValidSpell(item._iSpell))
		return;

	if (item._iCharges == item._iMaxCharges)
		return;

	int r = GetSpellStaffLevel(item._iSpell);
	r = GenerateRnd(player._pLevel / r) + 1;

	do {
		item._iMaxCharges--;
		if (item._iMaxCharges == 0) {
			return;
		}
		item._iCharges += r;
	} while (item._iCharges < item._iMaxCharges);

	item._iCharges = std::min(item._iCharges, item._iMaxCharges);

	if (&player != MyPlayer)
		return;
	if (&item == &player.InvBody[INVLOC_HAND_LEFT]) {
		NetSendCmdChItem(true, INVLOC_HAND_LEFT);
		return;
	}
	if (&item == &player.InvBody[INVLOC_HAND_RIGHT]) {
		NetSendCmdChItem(true, INVLOC_HAND_RIGHT);
		return;
	}
	for (int i = 0; i < player._pNumInv; i++) {
		if (&item == &player.InvList[i]) {
			NetSyncInvItem(player, i);
			break;
		}
	}
}

bool HasOilToSpend(const Player &player)
{
	for (int i = 0; i < player._pNumInv; i++) {
		if (!player.InvList[i].isEmpty() && player.InvList[i]._iMiscId == player._pOilType)
			return true;
	}
	for (int t = 0; t < Player::NumExtraInventoryTabs; t++) {
		for (int i = 0; i < player._pNumInvTab[t]; i++) {
			if (!player.InvTabList[t][i].isEmpty() && player.InvTabList[t][i]._iMiscId == player._pOilType)
				return true;
		}
	}
	for (const Item &belt : player.SpdList) {
		if (!belt.isEmpty() && belt._iMiscId == player._pOilType)
			return true;
	}
	if (&player == MyPlayer) {
		for (const Item &stashed : Stash.stashList) {
			if (!stashed.isEmpty() && stashed._iMiscId == player._pOilType)
				return true;
		}
	}
	return false;
}

void SpendOneOil(Player &player)
{
	for (int i = 0; i < player._pNumInv; i++) {
		if (!player.InvList[i].isEmpty() && player.InvList[i]._iMiscId == player._pOilType) {
			DecrementOrRemoveInvItem(player, i);
			return;
		}
	}
	for (int t = 0; t < Player::NumExtraInventoryTabs; t++) {
		for (int i = 0; i < player._pNumInvTab[t]; i++) {
			if (!player.InvTabList[t][i].isEmpty() && player.InvTabList[t][i]._iMiscId == player._pOilType) {
				DecrementOrRemoveInvItem(player, i, t);
				return;
			}
		}
	}
	for (int i = 0; i < MaxBeltItems; i++) {
		if (!player.SpdList[i].isEmpty() && player.SpdList[i]._iMiscId == player._pOilType) {
			DecrementOrRemoveSpdBarItem(player, i);
			return;
		}
	}
	if (&player != MyPlayer)
		return;
	for (size_t i = 0; i < Stash.stashList.size(); i++) {
		Item &stashed = Stash.stashList[i];
		if (stashed.isEmpty() || stashed._iMiscId != player._pOilType)
			continue;
		if (stashed.isStackableConsumable() && stashed.stackCount() > 1)
			stashed.setStackCount(stashed.stackCount() - 1);
		else
			Stash.RemoveStashItem(static_cast<StashStruct::StashCell>(i));
		Stash.dirty = true;
		oracool::ScheduleAutoSaveForStashChange(); // as every stash change (round 44 audit)
		return;
	}
}

bool ApplyOilToItem(Item &item, Player &player)
{
	int r;

	if (item._iClass == ICLASS_MISC) {
		return false;
	}
	// Ethereal cannot be repaired, and the two durability oils were a repair: refused, and the oil stays on the cursor
	// (round 5 audit, v1.12.230). Mend at the Cube is the one way back.
	// Oil of Permanence too: on a worn-down ethereal it was a free repair to whole and indestructible (round 21 audit).
	if (item._iOracoolEthereal && IsAnyOf(player._pOilType, IMISC_OILBSMTH, IMISC_OILFORT, IMISC_OILPERM)) {
		player.SaySpecific(HeroSpeech::ICantDoThat); // the refusal said, not silent (round 22 audit)
		return false;
	}
	if (item._iClass == ICLASS_GOLD) {
		return false;
	}
	if (item._iClass == ICLASS_QUEST) {
		return false;
	}

	switch (player._pOilType) {
	case IMISC_OILACC:
	case IMISC_OILMAST:
	case IMISC_OILSHARP:
		if (item._iClass == ICLASS_ARMOR) {
			return false;
		}
		break;
	case IMISC_OILDEATH:
		if (item._iClass == ICLASS_ARMOR) {
			return false;
		}
		if (item._itype == ItemType::Bow) {
			return false;
		}
		break;
	case IMISC_OILHARD:
	case IMISC_OILIMP:
		if (item._iClass == ICLASS_WEAPON) {
			return false;
		}
		break;
	default:
		break;
	}

	switch (player._pOilType) {
	case IMISC_OILACC:
		if (item._iPLToHit < 50) {
			item._iPLToHit += GenerateRnd(2) + 1;
		}
		break;
	case IMISC_OILMAST:
		if (item._iPLToHit < 100) {
			item._iPLToHit += GenerateRnd(3) + 3;
		}
		break;
	case IMISC_OILSHARP:
		if (item._iMaxDam - item._iMinDam < 30 && item._iMaxDam < 255) {
			item._iMaxDam = item._iMaxDam + 1;
		}
		break;
	case IMISC_OILDEATH:
		if (item._iMaxDam - item._iMinDam < 30 && item._iMaxDam < 254) {
			item._iMinDam = item._iMinDam + 1;
			item._iMaxDam = item._iMaxDam + 2;
		}
		break;
	case IMISC_OILSKILL:
		r = GenerateRnd(6) + 5;
		item._iMinStr = std::max(0, item._iMinStr - r);
		item._iMinMag = std::max(0, item._iMinMag - r);
		item._iMinDex = std::max(0, item._iMinDex - r);
		break;
	case IMISC_OILBSMTH:
		if (item._iMaxDur == DUR_INDESTRUCTIBLE || item._iDurability == DUR_INDESTRUCTIBLE)
			return true; // nothing to mend; Zod's stamp is not overwritten (round 11 audit)
		if (item._iDurability < item._iMaxDur) {
			item._iDurability = (item._iMaxDur + 4) / 5 + item._iDurability;
			item._iDurability = std::min<int>(item._iDurability, item._iMaxDur);
		} else {
			if (item._iMaxDur >= 100) {
				return true;
			}
			item._iMaxDur++;
			item._iDurability = item._iMaxDur;
		}
		break;
	case IMISC_OILFORT:
		// Not on a Zod host: 255 + r left the stamp and made the item destructible again (round 11 audit).
		if (item._iMaxDur != DUR_INDESTRUCTIBLE && item._iDurability != DUR_INDESTRUCTIBLE && item._iMaxDur < 200) {
			r = GenerateRnd(41) + 10;
			item._iMaxDur += r;
			item._iDurability += r;
		}
		break;
	case IMISC_OILPERM:
		item._iDurability = DUR_INDESTRUCTIBLE;
		item._iMaxDur = DUR_INDESTRUCTIBLE;
		break;
	case IMISC_OILHARD:
		if (item._iAC < 60) {
			const int added = GenerateRnd(2) + 1;
			item._iAC += added;
			if (item._iOracoolOilAC >= 0)
				item._iOracoolOilAC = static_cast<int16_t>(std::min(item._iOracoolOilAC + added, static_cast<int>(INT16_MAX)));
		}
		break;
	case IMISC_OILIMP:
		if (item._iAC < 120) {
			const int added = GenerateRnd(3) + 3;
			item._iAC += added;
			if (item._iOracoolOilAC >= 0)
				item._iOracoolOilAC = static_cast<int16_t>(std::min(item._iOracoolOilAC + added, static_cast<int>(INT16_MAX)));
		}
		break;
	default:
		return false;
	}
	return true;
}

void UpdateHellfireFlag(Item &item, const char *identifiedItemName)
{
	// DevilutionX support vanilla and hellfire items in one save file and for that introduced CF_HELLFIRE
	// But vanilla hellfire items don't have CF_HELLFIRE set in Item::dwBuff
	// This functions tries to set this flag for vanilla hellfire items based on the item name
	// This ensures that Item::getName() returns the correct translated item name
	if (item.dwBuff & CF_HELLFIRE)
		return; // Item is already a hellfire item
	if (item._iMagical != ITEM_QUALITY_MAGIC)
		return; // Only magic item's name can differ between diablo and hellfire
	if (gbIsMultiplayer)
		return; // Vanilla hellfire multiplayer is not supported in devilutionX, so there can't be items with missing dwBuff from there
	// We need to test both short and long name, cause StringInPanel can return a different result (other font and some bugfixes)
	std::string diabloItemNameShort = GetTranslatedItemNameMagical(item, false, false, false);
	if (diabloItemNameShort == identifiedItemName)
		return; // Diablo item name is identical => not a hellfire specific item
	std::string diabloItemNameLong = GetTranslatedItemNameMagical(item, false, false, true);
	if (diabloItemNameLong == identifiedItemName)
		return; // Diablo item name is identical => not a hellfire specific item
	std::string hellfireItemNameShort = GetTranslatedItemNameMagical(item, true, false, false);
	std::string hellfireItemNameLong = GetTranslatedItemNameMagical(item, true, false, true);
	if (hellfireItemNameShort == identifiedItemName || hellfireItemNameLong == identifiedItemName) {
		// This item should be a vanilla hellfire item that has CF_HELLFIRE missing, cause only then the item name matches
		item.dwBuff |= CF_HELLFIRE;
	}
}

namespace {

/** @brief Weapons, armour, rings and amulets - what Smart Loot aims. Mirrors SmartLootIsEquipmentBase. */
bool IsSmartLootEquipmentData(const ItemData &item)
{
	return item.iClass == ICLASS_ARMOR || item.iClass == ICLASS_WEAPON
	    || item.itype == ItemType::Ring || item.itype == ItemType::Amulet;
}

} // namespace

_item_indexes RndEquipmentForMonsterLevel(int8_t monsterLevel, item_equip_type slot)
{
	// RndItemForMonsterLevel's own walk - drop rate weighting and qlvl ceiling - with its two early
	// exits removed. Those exits return nothing three times in five and gold three times in four of
	// the rest, which is right for deciding WHETHER a monster drops equipment and useless for
	// choosing WHICH equipment: Smart Loot only asks this once the answer is already yes.
	return GetItemIndexForDroppableItem(true, [&monsterLevel, slot](const ItemData &item) {
		return PoolQlvl(item) <= monsterLevel && IsSmartLootEquipmentData(item) && (slot == ILOC_INVALID || item.iLoc == slot);
	});
}

_item_indexes RndEquipmentForCurrentLevel(item_equip_type slot)
{
	// RndAllItems' walk, which gives gold three times in four, without the gold.
	const int itemMaxLevel = ItemsGetCurrlevel() * 2;
	return GetItemIndexForDroppableItem(false, [&itemMaxLevel, slot](const ItemData &item) {
		return PoolQlvl(item) <= itemMaxLevel && IsSmartLootEquipmentData(item) && (slot == ILOC_INVALID || item.iLoc == slot);
	});
}

DropOddsTally SimulateMonsterDropOdds(int dropLevel, int itemRollLevel, int itemLevel, bool uniqueMonster, int kills, uint32_t seed)
{
	DropOddsTally tally;
	tally.kills = kills;
	const Player &player = *MyPlayer;
	const auto level = static_cast<int8_t>(std::clamp(dropLevel, 0, 127));
	for (int k = 0; k < kills; k++) {
		// A fresh stream per kill: SetupAllItems re-seeds the global engine with the item's own seed, so
		// without this the next kill's base would be decided by the last item's seed.
		SetRndSeed(seed + static_cast<uint32_t>(k) * 2654435761U);
		_item_indexes idx;
		if (uniqueMonster) {
			// RndUItem's pool for a unique monster, at its ItemLevelOfMonster.
			idx = GetItemIndexForDroppableItem(false, [level](const ItemData &item) {
				if (item.itype == ItemType::Misc && item.iMiscId == IMISC_BOOK)
					return true;
				if (level < PoolQlvl(item))
					return false;
				return !IsAnyOf(item.itype, ItemType::Gold, ItemType::Misc);
			});
		} else {
			idx = RndItemForMonsterLevel(level);
		}
		if (idx == IDI_NONE) {
			tally.nothing++;
			continue;
		}
		if (idx == IDI_GOLD) {
			tally.gold++;
			continue;
		}
		ClearUniqueItemFlags();
		Item item;
		SetupAllItems(player, item, idx, AdvanceRndSeed(), itemRollLevel, uniqueMonster ? 15 : 1, uniqueMonster, false, false,
		    /*allowTieredRoll=*/true, std::nullopt, itemLevel);
		if (item.isEmpty())
			tally.nothing++;
		else if (item._iMagical == ITEM_QUALITY_UNIQUE)
			tally.unique++;
		else if (item._iOracoolTier == OracoolItemTier::Primal)
			tally.primal++;
		else if (item._iOracoolTier == OracoolItemTier::BuffedUnique)
			tally.buffedUnique++;
		else if (item._iOracoolTier == OracoolItemTier::Rare)
			tally.rare++;
		else if (item._iMagical == ITEM_QUALITY_MAGIC)
			tally.magic++;
		else if (GetAffixItemTypeForItem(item) == AffixItemType::None)
			tally.consumable++;
		else
			tally.basic++;
	}
	ClearUniqueItemFlags();
	return tally;
}

int OracoolPoolAffixMinLevel(item_effect_type type, int param1, int param2)
{
	int best = -1;
	int fallback = -1;
	for (const OracoolPoolRow &row : OracoolPoolRows) {
		if (row.row.power.type != type)
			continue;
		const int level = row.row.PLMinLvl;
		if (fallback < 0 || level < fallback)
			fallback = level;
		if (param1 >= row.row.power.param1 && param2 <= row.row.power.param2 && (best < 0 || level < best))
			best = level;
	}
	return best >= 0 ? best : fallback;
}

int ApplyOracoolItemPower(const Player &player, Item &item, ItemPower &power)
{
	return SaveItemPower(player, item, power);
}

} // namespace devilution
