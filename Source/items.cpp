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
#include "inv_iterators.hpp"
#include "items/validation.h"
#include "levels/town.h"
#include "lighting.h"
#include "minitext.h"
#include "missiles.h"
#include "options.h"
#include "oracool/item_tiers.h"
#include "oracool/area_level.h"
#include "oracool/auto_save.h"
#include "oracool/class_skills.h"
#include "oracool/spell_ranks.h"
#include "oracool/event_log.h"
#include "oracool/gradual_healing.h"
#include "oracool/charms.h"
#include "oracool/gems.h"
#include "oracool/item_sets.h"
#include "oracool/oracool.h"
#include "oracool/runewords.h"
#include "oracool/skill_sounds.h"
#include "oracool/stat_sheet.h"
#include "panels/info_box.hpp"
#include "panels/ui_panels.hpp"
#include "player.h"
#include "playerdat.hpp"
#include "qol/stash.h"
#include "spells.h"
#include "stores.h"
#include "utils/format_int.hpp"
#include "utils/language.h"
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
};

/**
 * @brief Ground-drop animation index for an item graphic, safe for any cursor id.
 *
 * Oracool: ItemCAnimTbl is a flat array with one entry per VANILLA item graphic, indexed by
 * _iCurs with no bounds check. Our own icons live past its end, so reading it directly for one of
 * them is an out-of-bounds read that happens to work until it doesn't.
 *
 * The six new types deliberately borrow existing drop animations rather than shipping six more
 * ground CELs. The tumbling object on the floor is small, and this keeps an entire art dependency
 * off the critical path - swapping in dedicated animations later is a one-line change here plus
 * the new entries in ItemDropNames/ItemAnimLs.
 */
int8_t GetItemDropAnimIndex(uint16_t curs)
{
	if (curs < ICURS_ORACOOL_FIRST)
		return ItemCAnimTbl[curs];

	// 14 is "larmor" - light armour, a soft cloth/leather tumble. The closest existing match for
	// gloves, boots, bracers, shoulders and legs; the belt borrows it too rather than the noisier
	// metal-armour drop.
	constexpr int8_t OracoolWornDropAnim = 14;
	return OracoolWornDropAnim;
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
};
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
};
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
};
/** Maps from Griswold premium item number to a quality level delta as added to the base quality level. */
int premiumlvladd[] = {
	// clang-format off
	-1,
	-1,
	 0,
	 0,
	 1,
	 2,
	// clang-format on
};
/** Maps from Griswold premium item number to a quality level delta as added to the base quality level. */
int premiumLvlAddHellfire[] = {
	// clang-format off
	-1,
	-1,
	-1,
	 0,
	 0,
	 0,
	 0,
	 1,
	 1,
	 1,
	 1,
	 2,
	 2,
	 3,
	 3,
	// clang-format on
};

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
	int sa = 0;
	int ma = 0;
	int da = 0;

	// first iteration is used for collecting stat bonuses from items
	for (Item &equipment : EquippedPlayerItemsRange(player)) {
		// A broken (0-durability, left equipped rather than destroyed - see
		// BreakOrRemoveEquipment) item contributes nothing at all, the same as if it had
		// been removed. Checked here, before stat bonuses are ever added, rather than only
		// in the invalidation pass below, since that pass only ever removes an already-added
		// bonus - a broken item's bonus must never be added in the first place.
		equipment._iStatFlag = !equipment._iOracoolBroken;
		if (equipment._iStatFlag && equipment._iIdentified) {
			sa += equipment._iPLStr;
			ma += equipment._iPLMag;
			da += equipment._iPLDex;
		}
	}

	bool changeflag;
	do {
		// cap stats to 0
		const int currstr = std::max(0, sa + player._pBaseStr);
		const int currmag = std::max(0, ma + player._pBaseMag);
		const int currdex = std::max(0, da + player._pBaseDex);

		changeflag = false;
		// Iterate over equipped items and remove stat bonuses if they are not valid
		for (Item &equipment : EquippedPlayerItemsRange(player)) {
			if (!equipment._iStatFlag)
				continue;

			bool isValid = IsItemValid(equipment);

			if (currstr < equipment._iMinStr
			    || currmag < equipment._iMinMag
			    || currdex < equipment._iMinDex)
				isValid = false;

			if (isValid)
				continue;

			changeflag = true;
			equipment._iStatFlag = false;
			if (equipment._iIdentified) {
				sa -= equipment._iPLStr;
				ma -= equipment._iPLMag;
				da -= equipment._iPLDex;
			}
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

	int s = static_cast<int8_t>(SpellID::Firebolt);
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
	switch (level) {
	case -50:
		return -RndPL(6, 10);
	case -25:
		return -RndPL(1, 5);
	case 20:
		return RndPL(1, 5);
	case 36:
		return RndPL(6, 10);
	case 51:
		return RndPL(11, 15);
	case 66:
		return RndPL(16, 20);
	case 81:
		return RndPL(21, 30);
	case 96:
		return RndPL(31, 40);
	case 111:
		return RndPL(41, 50);
	case 126:
		return RndPL(51, 75);
	case 151:
		return RndPL(76, 100);
	default:
		app_fatal("Unknown to hit bonus");
	}
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
	case IPL_ALLRES:
		item._iPLFR = std::max(item._iPLFR + r, 0);
		item._iPLLR = std::max(item._iPLLR + r, 0);
		item._iPLMR = std::max(item._iPLMR + r, 0);
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
		int bonus = r * item._iMaxDur / 100;
		item._iMaxDur += bonus;
		item._iDurability += bonus;
	} break;
	case IPL_CRYSTALLINE:
		item._iPLDam += 140 + r * 2;
		[[fallthrough]];
	case IPL_DUR_CURSE:
		item._iMaxDur -= r * item._iMaxDur / 100;
		item._iMaxDur = std::max<uint8_t>(item._iMaxDur, 1);
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

void SaveItemAffix(const Player &player, Item &item, const PLStruct &affix)
{
	auto power = affix.power;
	int value = SaveItemPower(player, item, power);

	value = PLVal(value, power.param1, power.param2, affix.minVal, affix.maxVal);
	if (item._iVAdd1 != 0 || item._iVMult1 != 0) {
		item._iVAdd2 = value;
		item._iVMult2 = affix.multVal;
	} else {
		item._iVAdd1 = value;
		item._iVMult1 = affix.multVal;
	}
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
	if (!StringInPanel(name.c_str())) {
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
	if (forceNameLengthCheck ? *forceNameLengthCheck : !StringInPanel(identifiedName.c_str())) {
		string_view shortName = translate ? _(baseItemData.iSName) : baseItemData.iSName;
		identifiedName = fmt::format(fmt::runtime(magicFmt), prefixName, shortName, spellName);
	}
	return identifiedName;
}

void GetStaffPower(const Player &player, Item &item, int lvl, SpellID bs, bool onlygood)
{
	int preidx = GetStaffPrefixId(lvl, onlygood, gbIsHellfire);
	if (preidx != -1) {
		item._iMagical = ITEM_QUALITY_MAGIC;
		SaveItemAffix(player, item, ItemPrefixes[preidx]);
		item._iPrePower = ItemPrefixes[preidx].power.type;
	}

	const ItemData &baseItemData = AllItemsList[item.IDidx];
	std::string staffName = GenerateStaffName(baseItemData, item._iSpell, false);

	CopyUtf8(item._iName, staffName, sizeof(item._iName));
	if (preidx != -1) {
		std::string staffNameMagical = GenerateStaffNameMagical(baseItemData, item._iSpell, preidx, false, std::nullopt);
		CopyUtf8(item._iIName, staffNameMagical, sizeof(item._iIName));
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
			if (!ignoreLevelLimits && (ItemPrefixes[j].PLMinLvl < minlvl || ItemPrefixes[j].PLMinLvl > maxlvl))
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
			    && (ignoreLevelLimits || (ItemSuffixes[j].PLMinLvl >= minlvl && ItemSuffixes[j].PLMinLvl <= maxlvl))
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

void GetItemPower(const Player &player, Item &item, int minlvl, int maxlvl, AffixItemType flgs, bool onlygood, bool ignoreLevelLimits = false)
{
	const PLStruct *pPrefix = nullptr;
	const PLStruct *pSufix = nullptr;
	GetItemPowerPrefixAndSuffix(
	    minlvl, maxlvl, flgs, onlygood, gbIsHellfire,
	    [&item, &player, &pPrefix](const PLStruct &prefix) {
		    item._iMagical = ITEM_QUALITY_MAGIC;
		    SaveItemAffix(player, item, prefix);
		    item._iPrePower = prefix.power.type;
		    pPrefix = &prefix;
	    },
	    [&item, &player, &pSufix](const PLStruct &suffix) {
		    item._iMagical = ITEM_QUALITY_MAGIC;
		    SaveItemAffix(player, item, suffix);
		    item._iSufPower = suffix.power.type;
		    pSufix = &suffix;
	    },
	    ignoreLevelLimits);

	CopyUtf8(item._iIName, GenerateMagicItemName(item._iName, pPrefix, pSufix, false), sizeof(item._iIName));
	if (!StringInPanel(item._iIName)) {
		CopyUtf8(item._iIName, GenerateMagicItemName(AllItemsList[item.IDidx].iSName, pPrefix, pSufix, false), sizeof(item._iIName));
	}
	if (pPrefix != nullptr || pSufix != nullptr)
		CalcItemValue(item);
}

namespace {

/**
 * @brief Picks one eligible prefix for an Oracool-tiered item (Rare, Buffed Unique, ...),
 * excluding any affix type already rolled on this item and applying the running Good/Evil
 * exclusion cumulatively (unlike vanilla's single-prefix-then-single-suffix
 * GetItemPowerPrefixAndSuffix, a tiered item may already have picked up to
 * 2*Item::MaxOracoolAffixesPerSlot - 1 other affixes by the time this runs).
 *
 * @return Index into ItemPrefixes[], or -1 if nothing eligible remains.
 */
int SelectRarePrefixCandidate(int minlvl, int maxlvl, AffixItemType flgs, bool onlygood, bool hellfireItem,
    bool ignoreLevelLimits, const std::array<item_effect_type, Item::MaxOracoolAffixesPerSlot * 2> &pickedTypes, int pickedCount, goodorevil goe)
{
	int l[256];
	int nt = 0;
	for (int j = 0; ItemPrefixes[j].power.type != IPL_INVALID; j++) {
		if (!IsPrefixValidForItemType(j, flgs, hellfireItem))
			continue;
		if (!ignoreLevelLimits && (ItemPrefixes[j].PLMinLvl < minlvl || ItemPrefixes[j].PLMinLvl > maxlvl))
			continue;
		if (onlygood && !ItemPrefixes[j].PLOk)
			continue;
		if (HasAnyOf(flgs, AffixItemType::Staff) && ItemPrefixes[j].power.type == IPL_CHARGES)
			continue;
		if ((goe == GOE_GOOD && ItemPrefixes[j].PLGOE == GOE_EVIL) || (goe == GOE_EVIL && ItemPrefixes[j].PLGOE == GOE_GOOD))
			continue;
		bool alreadyPicked = false;
		for (int k = 0; k < pickedCount; k++) {
			if (pickedTypes[k] == ItemPrefixes[j].power.type) {
				alreadyPicked = true;
				break;
			}
		}
		if (alreadyPicked)
			continue;
		l[nt] = j;
		nt++;
		if (ItemPrefixes[j].PLDouble) {
			l[nt] = j;
			nt++;
		}
	}
	if (nt == 0)
		return -1;
	return l[GenerateRnd(nt)];
}

/** @brief Suffix equivalent of SelectRarePrefixCandidate; see that function for the shared rules. */
int SelectRareSuffixCandidate(int minlvl, int maxlvl, AffixItemType flgs, bool onlygood, bool hellfireItem,
    bool ignoreLevelLimits, const std::array<item_effect_type, Item::MaxOracoolAffixesPerSlot * 2> &pickedTypes, int pickedCount, goodorevil goe)
{
	int l[256];
	int nt = 0;
	for (int j = 0; ItemSuffixes[j].power.type != IPL_INVALID; j++) {
		if (!IsSuffixValidForItemType(j, flgs, hellfireItem))
			continue;
		if (!ignoreLevelLimits && (ItemSuffixes[j].PLMinLvl < minlvl || ItemSuffixes[j].PLMinLvl > maxlvl))
			continue;
		if (onlygood && !ItemSuffixes[j].PLOk)
			continue;
		if ((goe == GOE_GOOD && ItemSuffixes[j].PLGOE == GOE_EVIL) || (goe == GOE_EVIL && ItemSuffixes[j].PLGOE == GOE_GOOD))
			continue;
		bool alreadyPicked = false;
		for (int k = 0; k < pickedCount; k++) {
			if (pickedTypes[k] == ItemSuffixes[j].power.type) {
				alreadyPicked = true;
				break;
			}
		}
		if (alreadyPicked)
			continue;
		l[nt] = j;
		nt++;
	}
	if (nt == 0)
		return -1;
	return l[GenerateRnd(nt)];
}

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

	int s = static_cast<int8_t>(SpellID::Firebolt);
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
		if (!gbIsMultiplayer && s == static_cast<int8_t>(SpellID::Resurrect))
			s = static_cast<int8_t>(SpellID::Telekinesis);
		if (!gbIsMultiplayer && s == static_cast<int8_t>(SpellID::HealOther))
			s = static_cast<int8_t>(SpellID::BloodStar);
		if (s == maxSpells)
			s = static_cast<int8_t>(SpellID::Firebolt);
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
 * below. Returns AffixItemType::None for types that never carry prefix/suffix affixes.
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
		// accepts them on the wire) but NOT through this pool - because this pool is part of the
		// save format. UnPackItem recreates a dungeon item's INDEX by replaying its seed through
		// this exact walk, so growing the list re-routes every seeded recreation: the first attempt
		// put them here and pack_test watched a Jade Great Helm come back as Jade Leggings. Set
		// items drop through their own hook instead - see TrySpawnOracoolSetItem.
		if (IsOracoolItemIdx(i))
			continue;
		// Phase 1: gems, charms and runes obey the same pool-is-save-format rule; own hooks drop them.
		if (IsOracoolGemIdx(i) || IsOracoolCharmIdx(i) || IsOracoolRuneIdx(i))
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
		if (itemMaxLevel < item.iMinMLvl)
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
		if (itemMaxLevel < item.iMinMLvl)
			return false;
		return true;
	});
}

_item_indexes RndTypeItems(ItemType itemType, int imid, int lvl)
{
	int itemMaxLevel = lvl * 2;
	return GetItemIndexForDroppableItem(false, [&itemMaxLevel, &itemType, &imid](const ItemData &item) {
		if (itemMaxLevel < item.iMinMLvl)
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
	}
	if (GenerateRnd(100) > uniqueRollUpperBound)
		return UITEM_INVALID;

	int numu = 0;
	for (int j = 0; UniqueItems[j].UIItemId != UITYPE_INVALID; j++) {
		if (!IsUniqueAvailable(j))
			break;
		if (UniqueItems[j].UIItemId == AllItemsList[item.IDidx].iItemId
		    && lvl >= UniqueItems[j].UIMinLvl
		    && (recreate || !UniqueItemFlags[j] || gbIsMultiplayer)) {
			uok[j] = true;
			numu++;
		}
	}

	if (numu == 0)
		return UITEM_INVALID;

	DiscardRandomValues(1);
	// uint16_t and MaxUniqueItems, both of which were a uint8_t and a literal 128. With 230 uniques
	// in the table the old pair could neither represent an id past 255 nor walk past index 127, so
	// every unique the expansion added was unreachable even before the bitset overran.
	uint16_t itemData = 0;
	while (numu > 0) {
		if (uok[itemData])
			numu--;
		if (numu > 0)
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
	}

	CopyUtf8(item._iIName, UniqueItems[uid].UIName, sizeof(item._iIName));
	item._iIvalue = UniqueItems[uid].UIValue;

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
		oracool::ApplyBaseTier(item, oracool::TierForItem(item._iOracoolItemLevel, iseed));

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
			} else if (tieredRollEligible
			    && GenerateRnd(1000) < oracool::QualityChancePerMille(OracoolItemTier::Primal, item._iOracoolItemLevel, *sgOptions.Oracool.primalItemDropChance)
			    && tieredFlgs != AffixItemType::None) {
				// Primal is checked before Buffed Unique and Rare: it's the rarest and most
				// powerful tier, so it gets first crack at the item. Identification is
				// deliberately NOT forced here - every item, tiered or not, follows the
				// single shared Auto Identify Drops toggle checked below.
				GetPrimalItemAffixes(player, item, iblvl / 2, iblvl, tieredFlgs, onlygood);
			} else if (tieredRollEligible
			    && GenerateRnd(1000) < oracool::QualityChancePerMille(OracoolItemTier::BuffedUnique, item._iOracoolItemLevel, *sgOptions.Oracool.buffedUniqueItemDropChance)
			    && tieredFlgs != AffixItemType::None) {
				// Buffed Unique is checked before Rare: it's meant to be the rarer of the
				// two tiers, so the rarer roll gets first crack at the item before a more
				// common tier claims it.
				GetBuffedUniqueItemAffixes(player, item, iblvl / 2, iblvl, tieredFlgs, onlygood);
			} else if (tieredRollEligible
			    && GenerateRnd(1000) < oracool::QualityChancePerMille(OracoolItemTier::Rare, item._iOracoolItemLevel, *sgOptions.Oracool.rareItemDropChance)
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

	if (sendmsg)
		NetSendCmdPItem(false, CMD_DROPITEM, item.position, item);
	if (delta)
		DeltaAddItem(ii);
	if (spawn)
		NetSendCmdPItem(false, CMD_SPAWNITEM, item.position, item);
}

void SetupAllUseful(Item &item, int iseed, int lvl)
{
	item._iSeed = iseed;
	SetRndSeed(iseed);

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

	GetItemAttrs(item, idx, lvl);
	item._iCreateInfo = lvl | CF_USEFUL;
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

void PrintItemOil(char iDidx)
{
	switch (iDidx) {
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
	}
}

void printItemMiscKBM(const Item &item, const bool isOil, const bool isCastOnTarget)
{
	if (item._iMiscId == IMISC_MAPOFDOOM) {
		AddPanelString(_("Right-click to view"));
	} else if (isOil) {
		PrintItemOil(item._iMiscId);
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
		PrintItemOil(item._iMiscId);
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
		PrintItemOil(item._iMiscId);
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
	    || item._iMiscId == IMISC_ARENAPOT;
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
}

/**
 * @brief Oracool: the hover panel's colour scheme, per user specification.
 *
 * The name takes the item's own tier colour (Item::getTextColor), and everything below it splits
 * into two kinds: what the item IS - its damage or armour, durability, charges, stat requirements -
 * which is white, and what has been ADDED to it - its prefixes and suffixes - which is blue. The
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
	uint8_t str = item._iMinStr;
	uint8_t dex = item._iMinDex;
	uint8_t mag = item._iMinMag;
	if (str != 0 || mag != 0 || dex != 0) {
		std::string text = std::string(_("Required:"));
		if (str != 0)
			text.append(fmt::format(fmt::runtime(_(" {:d} Str")), str));
		if (mag != 0)
			text.append(fmt::format(fmt::runtime(_(" {:d} Mag")), mag));
		if (dex != 0)
			text.append(fmt::format(fmt::runtime(_(" {:d} Dex")), dex));
		// Oracool: requirements are base information about the item, so white - see the colour
		// scheme note in PrintItemDetails.
		AddPanelString(std::move(text), ItemBaseStatColor);
	}
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
		if (item.iMinMLvl < minlvl || item.iMinMLvl > maxlvl)
			return false;
		return true;
	});
}

_item_indexes RndSmithItem(const Player &player, int lvl)
{
	return RndVendorItem<SmithItemOk, true>(player, 0, lvl);
}

void SortVendor(Item *itemList)
{
	int count = 1;
	while (!itemList[count].isEmpty())
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
		GetItemAttrs(premiumItem, itemType, plvl);
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
		if (item.iMiscId == IMISC_ELIXSTR)
			return !gbIsHellfire || player._pBaseStr < player.GetMaximumAttributeValue(CharacterAttribute::Strength);
		if (item.iMiscId == IMISC_ELIXMAG)
			return !gbIsHellfire || player._pBaseMag < player.GetMaximumAttributeValue(CharacterAttribute::Magic);
		if (item.iMiscId == IMISC_ELIXDEX)
			return !gbIsHellfire || player._pBaseDex < player.GetMaximumAttributeValue(CharacterAttribute::Dexterity);
		if (item.iMiscId == IMISC_ELIXVIT)
			return !gbIsHellfire || player._pBaseVit < player.GetMaximumAttributeValue(CharacterAttribute::Vitality);
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

	while (true) {
		item = {};
		SetupAllItems(*MyPlayer, item, idx, AdvanceRndSeed(), 2 * lvl, 1, true, false, delta,
		    /*allowTieredRoll=*/true, std::nullopt, /*itemLevel=*/lvl);
		if (item._iCurs == icurs)
			break;

		idx = RndTypeItems(itemType, imid, lvl);
	}
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
		return oracool::BandedQlvl(item.iMinMLvl) <= monsterLevel;
	});
}

StringOrView GetTranslatedItemName(const Item &item)
{
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
		if (forceNameLengthCheck ? *forceNameLengthCheck : !StringInPanel(identifiedName.c_str())) {
			identifiedName = GenerateMagicItemName(_(baseItemData.iSName), pPrefix, pSufix, translate);
		}
	}

	SetRndSeed(currentSeed);
	return identifiedName;
}

} // namespace

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
 * @brief Generates an Oracool-tiered item's affixes: forces exactly minAffixesPerSlot prefixes
 * and minAffixesPerSlot suffixes (the tier's minimum identity requirement - "Always at least..."
 * per the roadmap, an unconditional guarantee, not a common case), then independently a further
 * bonusAffixChancePercent chance each for one more prefix and one more suffix, capped at
 * Item::MaxOracoolAffixesPerSlot - weighted toward fewer total affixes by design.
 * Shared engine behind GetRareItemAffixes (minAffixesPerSlot=1), GetBuffedUniqueItemAffixes
 * (minAffixesPerSlot=2), and GetPrimalItemAffixes (minAffixesPerSlot=3, perfectRoll=true).
 * Reuses vanilla's exact roll-and-apply primitives (SaveItemPower/PLVal) so the real _iPL*
 * stat bonuses are identical in kind to an ordinary magic item's; only the identity bookkeeping
 * (which affixes, at what rolled value) goes into Item::_iOracoolPrefixes/_iOracoolSuffixes
 * instead of the vanilla _iVAdd/_iVMult fields, which only have room for one of each.
 *
 * The minAffixesPerSlot loop always ignores the caller's level window (minlvl/maxlvl), regardless
 * of the ignoreLevelLimits argument or tier: the level window is by far the dominant cause of a
 * candidate pool running dry (a low-level drop, or a narrow-pool item class like jewelry, can
 * exhaust every eligible entry once a few affixes are already excluded as duplicates), and the
 * minimum count is a stated guarantee, not a best-effort. The duplicate-type and Good/Evil
 * exclusions above are never relaxed - only the level restriction is. The optional bonus-affix
 * rolls below still respect the ignoreLevelLimits argument as passed, since going over the
 * guaranteed minimum is explicitly probabilistic "extra," not a promise.
 *
 * @param perfectRoll When true, every affix's magnitude is forced to the maximum end of its
 * declared range (via RndPL, see ForcePerfectAffixRoll) instead of being randomly rolled, and
 * only beneficial (PLOk) affixes are ever considered regardless of the onlygood argument - a
 * maxed-out curse/drawback affix would contradict "perfect roll" being an unambiguous upgrade.
 * It also forces ignoreLevelLimits for the bonus-affix rolls (moot for Primal today, since its
 * minAffixesPerSlot already equals the hard cap and the bonus rolls never fire).
 */
void GetTieredItemAffixes(const Player &player, Item &item, int minlvl, int maxlvl, AffixItemType flgs, bool onlygood, OracoolItemTier tier, int minAffixesPerSlot, int bonusAffixChancePercent, bool ignoreLevelLimits, bool perfectRoll = false)
{
	if (perfectRoll) {
		onlygood = true;
		ignoreLevelLimits = true;
	}

	std::array<item_effect_type, Item::MaxOracoolAffixesPerSlot * 2> pickedTypes {};
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

	auto applyPrefix = [&](int idx) {
		const PLStruct &affix = ItemPrefixes[idx];
		ItemPower power = affix.power;
		int raw = SaveItemPower(player, item, power);
		int value = PLVal(raw, power.param1, power.param2, affix.minVal, affix.maxVal);
		item._iOracoolPrefixes[item._iOracoolPrefixCount] = OracoolAffix { affix.power.type, raw, affix.multVal };
		item._iOracoolPrefixCount++;
		pickedTypes[pickedCount++] = affix.power.type;
		priceAddTotal += value;
		priceMultTotal += affix.multVal;
		if (affix.PLGOE != GOE_ANY)
			goe = affix.PLGOE;
	};
	auto applySuffix = [&](int idx) {
		const PLStruct &affix = ItemSuffixes[idx];
		ItemPower power = affix.power;
		int raw = SaveItemPower(player, item, power);
		int value = PLVal(raw, power.param1, power.param2, affix.minVal, affix.maxVal);
		item._iOracoolSuffixes[item._iOracoolSuffixCount] = OracoolAffix { affix.power.type, raw, affix.multVal };
		item._iOracoolSuffixCount++;
		pickedTypes[pickedCount++] = affix.power.type;
		priceAddTotal += value;
		priceMultTotal += affix.multVal;
		if (affix.PLGOE != GOE_ANY)
			goe = affix.PLGOE;
	};

	const bool previousForcePerfectAffixRoll = ForcePerfectAffixRoll;
	ForcePerfectAffixRoll = perfectRoll;

	for (int i = 0; i < minAffixesPerSlot; i++) {
		int idx = SelectRarePrefixCandidate(minlvl, maxlvl, flgs, onlygood, gbIsHellfire, /*ignoreLevelLimits=*/true, pickedTypes, pickedCount, goe);
		if (idx != -1)
			applyPrefix(idx);
	}
	for (int i = 0; i < minAffixesPerSlot; i++) {
		int idx = SelectRareSuffixCandidate(minlvl, maxlvl, flgs, onlygood, gbIsHellfire, /*ignoreLevelLimits=*/true, pickedTypes, pickedCount, goe);
		if (idx != -1)
			applySuffix(idx);
	}

	if (item._iOracoolPrefixCount > 0 && item._iOracoolPrefixCount < Item::MaxOracoolAffixesPerSlot && GenerateRnd(100) < bonusAffixChancePercent) {
		int idx = SelectRarePrefixCandidate(minlvl, maxlvl, flgs, onlygood, gbIsHellfire, ignoreLevelLimits, pickedTypes, pickedCount, goe);
		if (idx != -1)
			applyPrefix(idx);
	}
	if (item._iOracoolSuffixCount > 0 && item._iOracoolSuffixCount < Item::MaxOracoolAffixesPerSlot && GenerateRnd(100) < bonusAffixChancePercent) {
		int idx = SelectRareSuffixCandidate(minlvl, maxlvl, flgs, onlygood, gbIsHellfire, ignoreLevelLimits, pickedTypes, pickedCount, goe);
		if (idx != -1)
			applySuffix(idx);
	}

	ForcePerfectAffixRoll = previousForcePerfectAffixRoll;

	CalcOracoolTieredItemValue(item, priceAddTotal, priceMultTotal);

	item._iMagical = ITEM_QUALITY_MAGIC;
	item._iOracoolTier = tier;
	item._iOracoolPerfectRoll = perfectRoll;

	const string_view tierLabel = GetOracoolTierLabel(tier);
	std::string tieredName = fmt::format(fmt::runtime(_("{0} {1}")), tierLabel, item._iName);
	if (!StringInPanel(tieredName.c_str()))
		tieredName = fmt::format(fmt::runtime(_("{0} {1}")), tierLabel, AllItemsList[item.IDidx].iSName);
	CopyUtf8(item._iIName, tieredName, sizeof(item._iIName));
}

void GetRareItemAffixes(const Player &player, Item &item, int minlvl, int maxlvl, AffixItemType flgs, bool onlygood, bool ignoreLevelLimits)
{
	constexpr int BonusAffixChancePercent = 30;
	GetTieredItemAffixes(player, item, minlvl, maxlvl, flgs, onlygood, OracoolItemTier::Rare, 1, BonusAffixChancePercent, ignoreLevelLimits);
}

void GetBuffedUniqueItemAffixes(const Player &player, Item &item, int minlvl, int maxlvl, AffixItemType flgs, bool onlygood, bool ignoreLevelLimits)
{
	constexpr int BonusAffixChancePercent = 30;
	GetTieredItemAffixes(player, item, minlvl, maxlvl, flgs, onlygood, OracoolItemTier::BuffedUnique, 2, BonusAffixChancePercent, ignoreLevelLimits);
}

void GetPrimalItemAffixes(const Player &player, Item &item, int minlvl, int maxlvl, AffixItemType flgs, bool onlygood, bool ignoreLevelLimits)
{
	// minAffixesPerSlot=3 already equals Item::MaxOracoolAffixesPerSlot, so the bonus-affix
	// roll inside GetTieredItemAffixes can never fire (the count-below-cap guard blocks it) -
	// bonusAffixChancePercent is passed as 0 here purely for clarity, not because it matters.
	GetTieredItemAffixes(player, item, minlvl, maxlvl, flgs, onlygood, OracoolItemTier::Primal, Item::MaxOracoolAffixesPerSlot, 0, ignoreLevelLimits, /*perfectRoll=*/true);
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

/**
 * @brief Oracool bug-repair helper (v0.3.42): given one OracoolAffix and the static table it was
 * rolled from (ItemPrefixes for a prefix, ItemSuffixes for a suffix), detects and corrects the
 * "price value stored instead of the real roll" bug - see GetTieredItemAffixes. Items generated
 * before that fix have the wrong value baked permanently into their save data; this lets them
 * self-heal the next time they're loaded or picked up, instead of staying wrong forever.
 * @return true if a correction was made.
 */
static bool RepairOracoolAffixValue(OracoolAffix &affix, const PLStruct *table)
{
	// If the stored value already looks like a plausible roll for some row of this type, leave it
	// alone. Deliberately conservative: on the rare boundary where a price range and a small roll
	// range happen to overlap numerically for the same type, treating it as "already valid" is far
	// safer than "fixing" a value that didn't need it.
	for (int j = 0; table[j].power.type != IPL_INVALID; j++) {
		if (table[j].power.type != affix.type)
			continue;
		int lo = std::min<int>(table[j].power.param1, table[j].power.param2);
		int hi = std::max<int>(table[j].power.param1, table[j].power.param2);
		if (affix.param1 >= lo && affix.param1 <= hi)
			return false;
	}

	// Otherwise, see if it matches some row's price range instead - the bug's exact signature.
	// Price ranges don't overlap between tiers of the same affix type, so a match (when one
	// exists) unambiguously identifies which row was actually rolled.
	for (int j = 0; table[j].power.type != IPL_INVALID; j++) {
		if (table[j].power.type != affix.type)
			continue;
		int priceLo = std::min(table[j].minVal, table[j].maxVal);
		int priceHi = std::max(table[j].minVal, table[j].maxVal);
		if (affix.param1 < priceLo || affix.param1 > priceHi)
			continue;

		int p1 = table[j].power.param1;
		int p2 = table[j].power.param2;
		int raw = (priceHi == priceLo || p1 == p2)
		    ? p2
		    : p1 + (p2 - p1) * (affix.param1 - priceLo) / (priceHi - priceLo);
		affix.param1 = raw;
		return true;
	}

	return false;
}

bool RepairOracoolAffixesIfCorrupted(Item &item)
{
	if (!item.hasOracoolTier())
		return false;

	bool repaired = false;
	for (int i = 0; i < item._iOracoolPrefixCount; i++) {
		if (RepairOracoolAffixValue(item._iOracoolPrefixes[i], ItemPrefixes))
			repaired = true;
	}
	for (int i = 0; i < item._iOracoolSuffixCount; i++) {
		if (RepairOracoolAffixValue(item._iOracoolSuffixes[i], ItemSuffixes))
			repaired = true;
	}

	if (repaired)
		oracool::LogEvent(fmt::format(fmt::runtime(_("Corrected stats on {:s}")), item._iIName));

	return repaired;
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

	_item_indexes baseItemIndex = IDI_GOLD;
	for (std::underlying_type_t<_item_indexes> i = IDI_GOLD; i <= IDI_LAST; ++i) {
		if (IsItemAvailable(i) && AllItemsList[i].iItemId == UniqueItems[uid].UIItemId) {
			baseItemIndex = static_cast<_item_indexes>(i);
			break;
		}
	}
	if (baseItemIndex == IDI_GOLD)
		return false;

	item = {};
	item._iSeed = AdvanceRndSeed();
	SetRndSeed(item._iSeed);
	GetItemAttrs(item, baseItemIndex, UniqueItems[uid].UIMinLvl);
	item._iCreateInfo = std::max<int>(UniqueItems[uid].UIMinLvl, 1) | CF_UNIQUE | CF_SMITH;
	const bool wasGenerated = UniqueItemFlags[uid];
	GetUniqueItem(player, item, uid);
	UniqueItemFlags[uid] = wasGenerated;
	item._iIdentified = true;
	item._iStatFlag = player.CanUseItem(item);
	return true;
}

void ClearUniqueItemFlags()
{
	memset(UniqueItemFlags, 0, sizeof(UniqueItemFlags));
}

void InitItemGFX()
{
	char arglist[64];

	int itemTypes = gbIsHellfire ? ITEMTYPES : 35;
	for (int i = 0; i < itemTypes; i++) {
		*BufCopy(arglist, "items\\", ItemDropNames[i]) = '\0';
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
	uint64_t spl = totals.spells;
	int fr = totals.fireResist;
	int lr = totals.lightningResist;
	int mr = totals.magicResist;
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
	player._pIAC = tac;
	player._pIBonusDam = bdam;
	player._pIBonusToHit = btohit;
	player._pIBonusAC = bac;
	player._pIFlags = iflgs;
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

	if (player._pClass == HeroClass::Rogue) {
		player._pDamageMod = player._pLevel * (player._pStrength + player._pDexterity) / 200;
	} else if (player._pClass == HeroClass::Monk) {
		// A broken (0-durability, left equipped rather than destroyed) weapon no longer
		// counts as "holding" anything for these class-specific checks, matching how
		// CalcSelfItems already excludes it from stat bonuses via _iStatFlag.
		const bool leftIsFunctionalNonStaff = !player.InvBody[INVLOC_HAND_LEFT].isEmpty() && player.InvBody[INVLOC_HAND_LEFT]._iStatFlag && player.InvBody[INVLOC_HAND_LEFT]._itype != ItemType::Staff;
		const bool rightIsFunctionalNonStaff = !player.InvBody[INVLOC_HAND_RIGHT].isEmpty() && player.InvBody[INVLOC_HAND_RIGHT]._iStatFlag && player.InvBody[INVLOC_HAND_RIGHT]._itype != ItemType::Staff;
		player._pDamageMod = player._pLevel * (player._pStrength + player._pDexterity) / 150;
		if (leftIsFunctionalNonStaff || rightIsFunctionalNonStaff)
			player._pDamageMod /= 2; // Monks get half the normal damage bonus if they're holding a non-staff weapon
	} else if (player._pClass == HeroClass::Bard) {
		const bool leftSword = player.InvBody[INVLOC_HAND_LEFT]._itype == ItemType::Sword && player.InvBody[INVLOC_HAND_LEFT]._iStatFlag;
		const bool rightSword = player.InvBody[INVLOC_HAND_RIGHT]._itype == ItemType::Sword && player.InvBody[INVLOC_HAND_RIGHT]._iStatFlag;
		const bool leftBow = player.InvBody[INVLOC_HAND_LEFT]._itype == ItemType::Bow && player.InvBody[INVLOC_HAND_LEFT]._iStatFlag;
		const bool rightBow = player.InvBody[INVLOC_HAND_RIGHT]._itype == ItemType::Bow && player.InvBody[INVLOC_HAND_RIGHT]._iStatFlag;
		if (leftSword || rightSword)
			player._pDamageMod = player._pLevel * (player._pStrength + player._pDexterity) / 150;
		else if (leftBow || rightBow) {
			player._pDamageMod = player._pLevel * (player._pStrength + player._pDexterity) / 250;
		} else {
			player._pDamageMod = player._pLevel * player._pStrength / 100;
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
			player._pDamageMod = player._pLevel * player._pStrength / 75;
		} else if (leftMace || rightMace) {
			player._pDamageMod = player._pLevel * player._pStrength / 75;
		} else if (leftBow || rightBow) {
			player._pDamageMod = player._pLevel * player._pStrength / 300;
		} else {
			player._pDamageMod = player._pLevel * player._pStrength / 100;
		}

		if (leftShield || rightShield) {
			if (leftShield)
				player._pIAC -= player.InvBody[INVLOC_HAND_LEFT]._iAC / 2;
			else if (rightShield)
				player._pIAC -= player.InvBody[INVLOC_HAND_RIGHT]._iAC / 2;
		} else if (!leftStaffOrBow && !rightStaffOrBow) {
			player._pDamageMod += player._pLevel * player._pVitality / 100;
		}
		player._pIAC += player._pLevel / 4;
	} else {
		player._pDamageMod = player._pLevel * player._pStrength / 100;
	}

	player._pISpells = spl;

	EnsureValidReadiedSpell(player);

	player._pISplLvlAdd = spllvladd;
	player._pIEnAc = enac;

	if (player._pClass == HeroClass::Barbarian) {
		mr += player._pLevel;
		fr += player._pLevel;
		lr += player._pLevel;
	}

	if (HasAnyOf(player._pSpellFlags, SpellFlag::RageCooldown)) {
		mr -= player._pLevel;
		fr -= player._pLevel;
		lr -= player._pLevel;
	}

	if (HasAnyOf(iflgs, ItemSpecialEffect::ZeroResistance)) {
		// reset resistances to zero if the respective special effect is active
		mr = 0;
		fr = 0;
		lr = 0;
	}

	player._pMagResist = clamp(mr, 0, MaxResistance);
	player._pFireResist = clamp(fr, 0, MaxResistance);
	player._pLghtResist = clamp(lr, 0, MaxResistance);

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

	PlayerWeaponGraphic animWeaponId = holdsShield ? PlayerWeaponGraphic::UnarmedShield : PlayerWeaponGraphic::Unarmed;
	switch (weaponItemType) {
	case ItemType::Sword:
		animWeaponId = holdsShield ? PlayerWeaponGraphic::SwordShield : PlayerWeaponGraphic::Sword;
		break;
	case ItemType::Axe:
		animWeaponId = PlayerWeaponGraphic::Axe;
		break;
	case ItemType::Bow:
		animWeaponId = PlayerWeaponGraphic::Bow;
		break;
	case ItemType::Mace:
		animWeaponId = holdsShield ? PlayerWeaponGraphic::MaceShield : PlayerWeaponGraphic::Mace;
		break;
	case ItemType::Staff:
		animWeaponId = PlayerWeaponGraphic::Staff;
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
	if (player._pgfxnum != gfxNum && loadgfx) {
		player._pgfxnum = gfxNum;
		ResetPlayerGFX(player);
		SetPlrAnims(player);
		player.previewCelSprite = std::nullopt;
		player_graphic graphic = player.getGraphic();
		int8_t numberOfFrames;
		int8_t ticksPerFrame;
		player.getAnimationFramesAndTicksPerFrame(graphic, numberOfFrames, ticksPerFrame);
		LoadPlrGFX(player, graphic);
		OptionalClxSpriteList sprites;
		if (!HeadlessMode)
			sprites = player.AnimationData[static_cast<size_t>(graphic)].spritesForDirection(player._pdir);
		player.AnimInfo.changeAnimationData(sprites, numberOfFrames, ticksPerFrame);
	} else {
		player._pgfxnum = gfxNum;
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
	item._iPrePower = IPL_INVALID;
	item._iSufPower = IPL_INVALID;
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
	case HeroClass::Barbarian:
		InitializeItem(player.InvBody[INVLOC_HAND_LEFT], IDI_BARBARIAN);
		GenerateNewSeed(player.InvBody[INVLOC_HAND_LEFT]);

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
	player.InvGrid[30] = player._pNumInv;

	player._pGold = goldItem._ivalue;

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
	item._iPrePower = IPL_INVALID;
	item._iSufPower = IPL_INVALID;

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
	case DIFF_NIGHTMARE:
		rndv = 5 * (itemlevel + 16) + GenerateRnd(10 * (itemlevel + 16));
		break;
	case DIFF_HELL:
		rndv = 5 * (itemlevel + 32) + GenerateRnd(10 * (itemlevel + 32));
		break;
	case DIFF_TORMENT:
		// Oracool: Hell's own formula, scaled further by the adjustable Torment multiplier.
		rndv = static_cast<int>((5 * (itemlevel + 32) + GenerateRnd(10 * (itemlevel + 32))) * GetTormentDifficultyMultiplier());
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
	switch (leveltype) {
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
	if (ActiveItemCount >= MAXITEMS)
		return nullptr;

	int ii = AllocateItem();
	auto &item = Items[ii];
	if (exactPosition && CanPut(position)) {
		item.position = position;
		dItem[position.x][position.y] = ii + 1;
	} else {
		GetSuperItemSpace(position, ii);
	}
	int curlv = ItemsGetCurrlevel();

	std::underlying_type_t<_item_indexes> idx = 0;
	while (AllItemsList[idx].iItemId != UniqueItems[uid].UIItemId)
		idx++;

	if (sgGameInitInfo.nDifficulty == DIFF_NORMAL) {
		GetItemAttrs(item, static_cast<_item_indexes>(idx), curlv);
		GetUniqueItem(*MyPlayer, item, uid);
		SetupItem(item);
	} else {
		if (level)
			curlv = *level;
		const ItemData &uniqueItemData = AllItemsList[idx];
		_item_indexes idx = GetItemIndexForDroppableItem(false, [&uniqueItemData](const ItemData &item) {
			return item.itype == uniqueItemData.itype;
		});
		SetupAllItems(*MyPlayer, item, idx, AdvanceRndSeed(), curlv * 2, 15, true, false, false,
		    /*allowTieredRoll=*/true, std::nullopt, /*itemLevel=*/curlv);
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
	if (monster.isUnique())
		level += 3;
	else if (monster.lesserAffix != LesserUniqueAffix::None)
		level += 2;
	return std::min(level, oracool::MaxAreaLevel);
}

void SpawnItem(Monster &monster, Point position, bool sendmsg, bool spawn /*= false*/)
{
	_item_indexes idx;
	bool onlygood = true;

	bool dropsSpecialTreasure = (monster.data().treasure & T_UNIQ) != 0;
	bool dropBrain = Quests[Q_MUSHROOM]._qactive == QUEST_ACTIVE && Quests[Q_MUSHROOM]._qvar1 == QS_MUSHGIVEN;

	if (dropsSpecialTreasure && !UseMultiplayerQuests()) {
		Item *uniqueItem = SpawnUnique(static_cast<_unique_items>(monster.data().treasure & T_MASK), position, std::nullopt, false);
		if (uniqueItem != nullptr && sendmsg)
			NetSendCmdPItem(false, CMD_DROPITEM, uniqueItem->position, *uniqueItem);
		return;
	} else if (monster.isUnique() || dropsSpecialTreasure) {
		// Unqiue monster is killed => use better item base (for example no gold)
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
		idx = RndItemForMonsterLevel(static_cast<int8_t>(std::min(ItemLevelOfMonster(monster), 127)));
	}

	if (idx == IDI_NONE)
		return;

	if (ActiveItemCount >= MAXITEMS)
		return;

	int ii = AllocateItem();
	auto &item = Items[ii];
	GetSuperItemSpace(position, ii);
	int uper = monster.isUnique() ? 15 : 1;

	int8_t mLevel = monster.data().level;
	if (!gbIsHellfire && monster.type().type == MT_DIABLO)
		mLevel -= 15;

	SetupAllItems(*MyPlayer, item, idx, AdvanceRndSeed(), mLevel, uper, onlygood, false, false);
	// Phase 1: the drop tail - Magic/Gold Find first (an upgraded item then correctly skips the
	// socket roll), then sockets, then ethereal. All AFTER setup and outside the seed replay -
	// see TryAddSocketsToDroppedItem's comment for why none of this may move into SetupAllItems.
	ApplyMagicAndGoldFindToDrop(item, mLevel);
	TryAddSocketsToDroppedItem(item);
	TryMakeDroppedItemEthereal(item);
	LogNoteworthyItemDrop(item);

	if (sendmsg)
		NetSendCmdPItem(false, CMD_DROPITEM, item.position, item);
	if (spawn)
		NetSendCmdPItem(false, CMD_SPAWNITEM, item.position, item);
}

void CreateRndItem(Point position, bool onlygood, bool sendmsg, bool delta)
{
	_item_indexes idx = onlygood ? RndUItem(nullptr) : RndAllItems();

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
	ItemPack pkSItem;

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

	if (strlen(sgOptions.Hellfire.szItem) < sizeof(ItemPack) * 2)
		return;

	Hex2bin(sgOptions.Hellfire.szItem, sizeof(ItemPack), reinterpret_cast<uint8_t *>(&pkSItem));

	int ii = AllocateItem();
	auto &item = Items[ii];

	dItem[position.x][position.y] = ii + 1;

	UnPackItem(pkSItem, *MyPlayer, item, (pkSItem.dwBuff & CF_HELLFIRE) != 0);
	item.position = position;
	RespawnItem(item, false);
	CornerStone.item = item;
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

void SpawnRewardItem(_item_indexes itemid, Point position, bool sendmsg)
{
	if (ActiveItemCount >= MAXITEMS)
		return;

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

void RespawnItem(Item &item, bool flipFlag)
{
	int it = GetItemDropAnimIndex(item._iCurs);
	item.setNewAnimation(flipFlag);
	item._iRequest = false;

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
				PlaySfxLoc(ItemDropSnds[GetItemDropAnimIndex(item._iCurs)], item.position);

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
	int it = GetItemDropAnimIndex(item._iCurs);
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

	// The same construction the debug set commands use, magic roll and tier ladder included; the
	// level clamp is the same 30 that keeps IsDungeonItemValid satisfied on the loopback.
	const int lvl = std::clamp(mlvl, 1, 30);
	Item item;
	SetupAllItems(*MyPlayer, item, idx, AdvanceRndSeed(), lvl, 1, /*onlygood=*/false,
	    /*recreate=*/false, /*pregen=*/false, /*allowTieredRoll=*/true);

	const int ii = AllocateItem();
	Items[ii] = item.pop();
	Point position = monster.position.tile;
	GetSuperItemSpace(position, ii);
	if (sendmsg)
		NetSendCmdPItem(false, CMD_SPAWNITEM, Items[ii].position, Items[ii]);
}

void TrySpawnOracoolGem(const Monster &monster, bool sendmsg)
{
	// Megaplan Phase 1: the gems' own drop path - a hook rather than a pool seat, for exactly the
	// reason TrySpawnOracoolSetItem's comment records: the droppable pool is save format.
	if (!oracool::IsSinglePlayer())
		return;

	// Rarer than set pieces: a gem is permanent power the moment it lands in a socket, and the
	// telemetry (Phase 0.9) exists to tune these numbers against real sessions. One draw covers
	// all three socket-economy families: 3 in 100 a gem, the next 1 a charm, the next 2 a rune.
	constexpr int GemDropPercent = 3;
	constexpr int CharmDropPercent = 1;
	constexpr int RuneDropPercent = 2;
	const int roll = GenerateRnd(100);
	if (roll >= GemDropPercent + CharmDropPercent + RuneDropPercent)
		return;

	const int mlvl = ItemLevelOfMonster(monster);
	_item_indexes idx;

	if (roll < GemDropPercent) {
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
		int first = IDI_ORACOOL_RUNE_EL;
		int last = IDI_ORACOOL_RUNE_SOL;
		bool charmsOnly = false;
		if (roll < GemDropPercent + CharmDropPercent) {
			// The charms live in two enum islands (the MF/GF pair was appended after the runes -
			// positional indices), so the walk spans both and filters by the range check.
			first = IDI_ORACOOL_CHARM_VIGOR;
			last = IDI_ORACOOL_CHARM_GREED;
			charmsOnly = true;
		}
		_item_indexes candidates[12];
		int candidateCount = 0;
		for (int i = first; i <= last; i++) {
			if (charmsOnly && !IsOracoolCharmIdx(i))
				continue;
			if (oracool::BandedQlvl(AllItemsList[i].iMinMLvl) <= mlvl)
				candidates[candidateCount++] = static_cast<_item_indexes>(i);
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
	Point position = monster.position.tile;
	GetSuperItemSpace(position, ii);
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
	// look forever, rare enough that a 3-socket roll (1 in 40 drops) still lands as an event.
	if (GenerateRnd(100) >= 25)
		return;
	const int roll = GenerateRnd(100);
	item._iSocketCount = roll < 60 ? 1 : (roll < 90 ? 2 : 3);
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

	const int magicFind = MyPlayer->_pMagicFind;
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

void TryMakeDroppedItemEthereal(Item &item)
{
	// Phase 1 ethereal: a ghost of an item - more of everything, half the lifespan, and no smith
	// can touch it. Rolled on the drop paths for any quality of durable equipment; the bargain is
	// stamped into the item's own stats here, so nothing downstream computes anything.
	if (!oracool::IsSinglePlayer() || item.isEmpty())
		return;
	if (item._iClass != ICLASS_WEAPON && item._iClass != ICLASS_ARMOR)
		return;
	if (item._iMaxDur == 0 || item._iMaxDur == DUR_INDESTRUCTIBLE)
		return;
	if (GenerateRnd(100) >= 5)
		return;

	item._iOracoolEthereal = true;
	if (item._iClass == ICLASS_WEAPON) {
		item._iMinDam = item._iMinDam * 135 / 100;
		item._iMaxDam = std::max<int>(item._iMaxDam * 135 / 100, item._iMinDam);
	} else {
		item._iAC = std::max<int>(item._iAC * 135 / 100, item._iAC + 1);
	}
	item._iMaxDur = std::max<int>(1, item._iMaxDur / 2);
	item._iDurability = std::min<int>(item._iDurability, item._iMaxDur);
}

void GetItemStr(Item &item)
{
	if (item._itype != ItemType::Gold) {
		// Oracool: the name carries the item's tier colour, and SetPanelString records that as line
		// 0's colour so the stat lines below it can be coloured independently.
		SetPanelString(item.getName(), item.getTextColor());
	} else {
		int nGold = item._ivalue;
		InfoString = fmt::format(fmt::runtime(ngettext("{:s} gold piece", "{:s} gold pieces", nGold)), FormatInteger(nGold));
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
		if (item._iPLFR < MaxResistance)
			return fmt::format(fmt::runtime(_("Resist Fire: {:+d}%")), item._iPLFR);
		else
			return fmt::format(fmt::runtime(_("Resist Fire: {:+d}% MAX")), MaxResistance);
	case IPL_LIGHTRES:
	case IPL_LIGHTRES_CURSE:
		if (item._iPLLR < MaxResistance)
			return fmt::format(fmt::runtime(_("Resist Lightning: {:+d}%")), item._iPLLR);
		else
			return fmt::format(fmt::runtime(_("Resist Lightning: {:+d}% MAX")), MaxResistance);
	case IPL_MAGICRES:
	case IPL_MAGICRES_CURSE:
		if (item._iPLMR < MaxResistance)
			return fmt::format(fmt::runtime(_("Resist Magic: {:+d}%")), item._iPLMR);
		else
			return fmt::format(fmt::runtime(_("Resist Magic: {:+d}% MAX")), MaxResistance);
	case IPL_ALLRES:
		if (item._iPLFR < MaxResistance)
			return fmt::format(fmt::runtime(_("Resist All: {:+d}%")), item._iPLFR);
		else
			return fmt::format(fmt::runtime(_("Resist All: {:+d}% MAX")), MaxResistance);
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
		return fmt::format(fmt::runtime(_(/*xgettext:no-c-format*/ "+{:d}% light radius")), 10 * item._iPLLight);
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
		if (HasAnyOf(item._iFlags, ItemSpecialEffect::QuickAttack))
			return _("quick attack");
		if (HasAnyOf(item._iFlags, ItemSpecialEffect::FastAttack))
			return _("fast attack");
		if (HasAnyOf(item._iFlags, ItemSpecialEffect::FasterAttack))
			return _("faster attack");
		if (HasAnyOf(item._iFlags, ItemSpecialEffect::FastestAttack))
			return _("fastest attack");
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
		return _("one handed sword");
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
		return fmt::format(fmt::runtime(_("to hit: {:+d}%, {:+d}% damage")), item._iPLToHit, item._iPLDam);
	case IPL_ACDEMON:
		return _("extra AC vs demons");
	case IPL_ACUNDEAD:
		return _("extra AC vs undead");
	case IPL_MANATOLIFE:
		return _("50% Mana moved to Health");
	case IPL_LIFETOMANA:
		return _("40% Health moved to Mana");
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
	case IPL_LIFE:
		return fmt::format(fmt::runtime(_("Hit Points: {:+d}")), affix.param1);
	case IPL_LIFE_CURSE:
		return fmt::format(fmt::runtime(_("Hit Points: {:+d}")), -affix.param1);
	case IPL_MANA:
		return fmt::format(fmt::runtime(_("Mana: {:+d}")), affix.param1);
	case IPL_MANA_CURSE:
		return fmt::format(fmt::runtime(_("Mana: {:+d}")), -affix.param1);
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
	case IPL_FIRERES:
		return fmt::format(fmt::runtime(_(/*xgettext:no-c-format*/ "{:+d}% fire resist")), power.param1);
	case IPL_LIGHTRES:
		return fmt::format(fmt::runtime(_(/*xgettext:no-c-format*/ "{:+d}% lightning resist")), power.param1);
	case IPL_MAGICRES:
		return fmt::format(fmt::runtime(_(/*xgettext:no-c-format*/ "{:+d}% magic resist")), power.param1);
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
		case 1: return std::string(_("quick attack"));
		case 2: return std::string(_("fast attack"));
		case 3: return std::string(_("faster attack"));
		default: return std::string(_("fastest attack"));
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
		return fmt::format(fmt::runtime(_(/*xgettext:no-c-format*/ "{:d}% life stolen per hit")), power.param1);
	case IPL_STEALMANA:
		return fmt::format(fmt::runtime(_(/*xgettext:no-c-format*/ "{:d}% mana stolen per hit")), power.param1);
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
 * Magic items are absent here on purpose: PrintItemDetails already prints their prefix and suffix
 * powers, and the old box was showing them a second time in a different place.
 */
void AddItemPowerPanelStrings(const Item &item)
{
	// A set piece's stats come from its own definition, not from a roll.
	//
	// Bug (fixed 2026-08-16, user report: "i dont see any affixes" on a complete set). Set is an
	// OracoolItemTier, so it fell into the branch below and printed _iOracoolPrefixes /
	// _iOracoolSuffixes - the arrays the affix ROLLER fills. A set item is never rolled: MakeSetItem
	// applies its declared powers straight into the _iPL* fields, so those arrays are empty and the
	// description had nothing to say.
	//
	// PrintItemPower reads the item's own accumulated fields, which is exactly right here: a set
	// piece has one source for each stat, so there is no accumulation to disentangle - the very
	// problem PrintOracoolAffixPower exists to solve for multi-affix tiered items.
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
			AddPanelString(PrintItemPower(power.type, item), ItemAffixColor);
		}

		const oracool::ItemSetDefinition *set = oracool::FindItemSetOwning(def->id);
		if (set == nullptr)
			return;
		const int worn = oracool::WornSetPieces(*MyPlayer, *set);
		AddPanelString(fmt::format(fmt::runtime(_("{:s} ({:d}/{:d})")), _(set->name), worn, set->itemCount),
		    UiFlags::ColorOracoolGreen);

		// Every piece of the set, worn ones green and missing ones red, each followed by its slot in
		// white brackets. The white tail is a two-run line - see AddPanelStringSplit and the tail
		// handling in oracool::DrawCursorTooltip; one colour per line could not say this.
		for (int i = 0; i < set->itemCount; i++) {
			const oracool::SetItemDefinition &piece = oracool::ItemSetItems[set->firstItem + i];
			std::string name = StrCat("  ", _(piece.name));
			// The offset is taken BEFORE the bracket is appended, so it is the byte the white run
			// starts at whatever the translated name's length turns out to be.
			const size_t tailStart = name.size();
			name = StrCat(name, " (", _(oracool::SetSlotDisplayName(piece.slot)), ")");
			AddPanelStringSplit(std::move(name),
			    oracool::IsSetPieceWorn(*MyPlayer, piece) ? UiFlags::ColorOracoolGreen : UiFlags::ColorRed,
			    tailStart);
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
		// the item instance itself (up to 3 prefixes + 3 suffixes), so the vanilla
		// UniqueItems[uid].powers[] table isn't involved at all here.
		for (int i = 0; i < item._iOracoolPrefixCount; i++)
			AddPanelString(PrintOracoolAffixPower(item._iOracoolPrefixes[i], item), ItemAffixColor);
		for (int i = 0; i < item._iOracoolSuffixCount; i++)
			AddPanelString(PrintOracoolAffixPower(item._iOracoolSuffixes[i], item), ItemAffixColor);
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
 * whether it has the property anywhere: a unique's powers are only listed for uniques, and an
 * Oracool tier's affixes only for tiered items, so an item can be indestructible with nothing below
 * to say so. Those keep the word on the base line, which is the only place it would appear.
 */
bool AffixStatesIndestructible(const Item &item)
{
	if (item._iPrePower == IPL_INDESTRUCTIBLE || item._iSufPower == IPL_INDESTRUCTIBLE)
		return true;
	if (item.hasOracoolTier()) {
		for (int i = 0; i < item._iOracoolPrefixCount; i++) {
			if (item._iOracoolPrefixes[i].type == IPL_INDESTRUCTIBLE)
				return true;
		}
		for (int i = 0; i < item._iOracoolSuffixCount; i++) {
			if (item._iOracoolSuffixes[i].type == IPL_INDESTRUCTIBLE)
				return true;
		}
	} else if (item._iMagical == ITEM_QUALITY_UNIQUE) {
		for (const auto &power : UniqueItems[item._iUid].powers) {
			if (power.type == IPL_INVALID)
				break;
			if (power.type == IPL_INDESTRUCTIBLE)
				return true;
		}
	}
	return false;
}

void PrintItemDetails(const Item &item)
{
	if (HeadlessMode)
		return;

	const bool indestructible = item._iMaxDur == DUR_INDESTRUCTIBLE;
	// Only suppressed when something below will actually print the word - see the helper.
	const bool affixSaysIndestructible = indestructible && AffixStatesIndestructible(item);

	// Oracool: user request (2026-08-16) - the tier line sits directly BELOW THE NAME and above the
	// damage stats, for every quality: "basic item" and "magic item" say so out loud now, not just by
	// the absence of colour, and the tiered labels moved up here from below the affix block. Each
	// line wears the name's own colour, so the word and the colour teach each other. Equipment only
	// (anything with a worn slot) - a potion calling itself a basic item would be noise, not
	// information.
	if (item._iLoc != ILOC_NONE && item._iLoc != ILOC_UNEQUIPABLE && item._iLoc != ILOC_BELT) {
		if (item.hasOracoolTier())
			AddPanelString(GetOracoolTierPanelLabel(item._iOracoolTier), item.getTextColor());
		else if (item._iMagical == ITEM_QUALITY_UNIQUE)
			AddPanelString(_("unique item"), item.getTextColor());
		else if (item._iMagical == ITEM_QUALITY_MAGIC)
			AddPanelString(_("magic item"), item.getTextColor());
		else
			AddPanelString(_("basic item"), item.getTextColor());
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
		AddPanelString(fmt::format(fmt::runtime(_("Charges: {:d}/{:d}")), item._iCharges, item._iMaxCharges), ItemBaseStatColor);
	}
	if (item._iPrePower != -1) {
		AddPanelString(PrintItemPower(item._iPrePower, item), ItemAffixColor);
	}
	if (item._iSufPower != -1) {
		AddPanelString(PrintItemPower(item._iSufPower, item), ItemAffixColor);
	}
	// The tier label used to print here, between the affixes and the power list; it now leads the
	// panel instead (user request, 2026-08-16 - "just below their name and above the dmg stats").
	if (item.hasOracoolTier() || item._iMagical == ITEM_QUALITY_UNIQUE) {
		AddItemPowerPanelStrings(item);
	}
	// Phase 1 ethereal: the whole bargain in one line, directly under the tier - the buffed stats
	// already show in the numbers above, so what the line carries is the PRICE.
	if (item._iOracoolEthereal)
		AddPanelString(_("Ethereal (cannot be repaired)"), ItemBaseStatColor);
	// Phase 1 charms: the effect, and the rule that governs it - the description is where the
	// active-cap system explains itself.
	if (IsOracoolCharmIdx(item.IDidx)) {
		AddPanelString(oracool::CharmEffectLine(static_cast<uint16_t>(item.IDidx)), ItemAffixColor);
		AddPanelString(fmt::format(fmt::runtime(_("only your first {:d} charms are active")), oracool::CharmActiveCap), ItemBaseStatColor);
	}
	// Phase 1 runes: every rune teaches the runewords it belongs to - the recipes drop WITH the
	// runes, which is the whole "discoverable in-game" improvement over D2's wiki homework.
	if (IsOracoolRuneIdx(item.IDidx)) {
		const std::string teaching = oracool::RuneTeachingLines(static_cast<uint16_t>(item.IDidx));
		size_t start = 0;
		while (start < teaching.size()) {
			size_t end = teaching.find('\n', start);
			if (end == std::string::npos)
				end = teaching.size();
			AddPanelString(teaching.substr(start, end - start), ItemAffixColor);
			start = end + 1;
		}
	}
	// A completed runeword announces itself above the stats, in the name's own gold.
	if (const oracool::RunewordDefinition *word = oracool::GetActiveRuneword(item); word != nullptr)
		AddPanelString(fmt::format(fmt::runtime(_("Runeword: {:s}")), _(word->name)), UiFlags::ColorWhitegold);
	// Phase 1 sockets: the socket line and one line per set gem, each in the gem economy's own
	// voice. The empty-socket count is the item's pitch - "Sockets: 1/3" is an invitation.
	if (item._iSocketCount > 0) {
		AddPanelString(fmt::format(fmt::runtime(_("Sockets: {:d}/{:d}")), item.socketedCount(), item._iSocketCount), ItemBaseStatColor);
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

	// Oracool: the unidentified view shows only base stats, so it is white throughout. "Not
	// Identified" takes the affix colour because it stands in for the affix lines that are being
	// withheld - it is a statement about the rolls, not about the base item.
	if (item._iClass == ICLASS_WEAPON) {
		if (item._iMinDam == item._iMaxDam) {
			if (item._iMaxDur == DUR_INDESTRUCTIBLE)
				AddPanelString(fmt::format(fmt::runtime(_("damage: {:d}  Indestructible")), item._iMinDam), ItemBaseStatColor);
			else
				AddPanelString(fmt::format(fmt::runtime(_("damage: {:d}  Dur: {:d}/{:d}")), item._iMinDam, item._iDurability, item._iMaxDur), ItemBaseStatColor);
		} else {
			if (item._iMaxDur == DUR_INDESTRUCTIBLE)
				AddPanelString(fmt::format(fmt::runtime(_("damage: {:d}-{:d}  Indestructible")), item._iMinDam, item._iMaxDam), ItemBaseStatColor);
			else
				AddPanelString(fmt::format(fmt::runtime(_("damage: {:d}-{:d}  Dur: {:d}/{:d}")), item._iMinDam, item._iMaxDam, item._iDurability, item._iMaxDur), ItemBaseStatColor);
		}
		if (item._iMiscId == IMISC_STAFF && item._iMaxCharges > 0) {
			AddPanelString(fmt::format(fmt::runtime(_("Charges: {:d}/{:d}")), item._iCharges, item._iMaxCharges), ItemBaseStatColor);
		}
		if (item._iMagical != ITEM_QUALITY_NORMAL)
			AddPanelString(_("Not Identified"), ItemAffixColor);
	}
	if (item._iClass == ICLASS_ARMOR) {
		if (item._iMaxDur == DUR_INDESTRUCTIBLE)
			AddPanelString(fmt::format(fmt::runtime(_("armor: {:d}  Indestructible")), item._iAC), ItemBaseStatColor);
		else
			AddPanelString(fmt::format(fmt::runtime(_("armor: {:d}  Dur: {:d}/{:d}")), item._iAC, item._iDurability, item._iMaxDur), ItemBaseStatColor);
		if (item._iMagical != ITEM_QUALITY_NORMAL)
			AddPanelString(_("Not Identified"), ItemAffixColor);
		if (item._iMiscId == IMISC_STAFF && item._iMaxCharges > 0) {
			AddPanelString(fmt::format(fmt::runtime(_("Charges: {:d}/{:d}")), item._iCharges, item._iMaxCharges), ItemBaseStatColor);
		}
	}
	if (IsAnyOf(item._itype, ItemType::Ring, ItemType::Amulet))
		AddPanelString(_("Not Identified"));
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
			NetSendCmdLocParam3(true, CMD_SPELLXY, target, static_cast<int8_t>(spellID), static_cast<uint8_t>(SpellType::Scroll), static_cast<uint16_t>(spellFrom));
		}
		break;
	case IMISC_BOOK: {
		uint8_t newSpellLevel = player._pSplLvl[static_cast<int8_t>(spellID)] + 1;
		// The level band and the Rule of Rangs, asked as one question (user, 2026-08-19: "apply lvl
		// req rule to books as well"). A book that would raise the spell past what the reader's level
		// allows is refused outright - it is not consumed, no mana is granted, nothing happens.
		//
		// The same answer updateRequiredStatsCacheForPlayer gives, so a book the inventory draws as
		// unusable is a book this path also refuses. One rule, asked in two places.
		if (!oracool::CanReadSpellBookTo(player, spellID, newSpellLevel))
			return; // the caller refuses first; this is the backstop, and it consumes nothing
		if (newSpellLevel <= MaxSpellLevel) {
			player._pSplLvl[static_cast<int8_t>(spellID)] = newSpellLevel;
			NetSendCmdParam2(true, CMD_CHANGE_SPELL_LEVEL, static_cast<uint16_t>(spellID), newSpellLevel);
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

void SpawnSmith(int lvl)
{
	constexpr int PinnedItemCount = 0;

	int maxValue = MaxVendorValue;
	int maxItems = 20;
	if (gbIsHellfire) {
		maxValue = MaxVendorValueHf;
		maxItems = 25;
	}

	int iCnt = GenerateRnd(maxItems - 10) + 10;
	for (int i = 0; i < iCnt; i++) {
		Item &newItem = smithitem[i];

		do {
			newItem = {};
			newItem._iSeed = AdvanceRndSeed();
			SetRndSeed(newItem._iSeed);
			_item_indexes itemData = RndSmithItem(*MyPlayer, lvl);
			GetItemAttrs(newItem, itemData, lvl);
		} while (newItem._iIvalue > maxValue);

		newItem._iCreateInfo = lvl | CF_SMITH;
		newItem._iIdentified = true;
	}
	for (int i = iCnt; i < SMITH_ITEMS; i++)
		smithitem[i].clear();

	SortVendor(smithitem + PinnedItemCount);
}

void SpawnPremium(const Player &player)
{
	int8_t lvl = player._pLevel;
	int maxItems = gbIsHellfire ? SMITH_PREMIUM_ITEMS : 6;
	if (numpremium < maxItems) {
		for (int i = 0; i < maxItems; i++) {
			if (premiumitems[i].isEmpty()) {
				int plvl = premiumlevel + (gbIsHellfire ? premiumLvlAddHellfire[i] : premiumlvladd[i]);
				SpawnOnePremium(premiumitems[i], plvl, player);
			}
		}
		numpremium = maxItems;
	}
	while (premiumlevel < lvl) {
		premiumlevel++;
		if (gbIsHellfire) {
			// Discard first 3 items and shift next 10
			std::move(&premiumitems[3], &premiumitems[12] + 1, &premiumitems[0]);
			SpawnOnePremium(premiumitems[10], premiumlevel + premiumLvlAddHellfire[10], player);
			premiumitems[11] = premiumitems[13];
			SpawnOnePremium(premiumitems[12], premiumlevel + premiumLvlAddHellfire[12], player);
			premiumitems[13] = premiumitems[14];
			SpawnOnePremium(premiumitems[14], premiumlevel + premiumLvlAddHellfire[14], player);
		} else {
			// Discard first 2 items and shift next 3
			std::move(&premiumitems[2], &premiumitems[4] + 1, &premiumitems[0]);
			SpawnOnePremium(premiumitems[3], premiumlevel + premiumlvladd[3], player);
			premiumitems[4] = premiumitems[5];
			SpawnOnePremium(premiumitems[5], premiumlevel + premiumlvladd[5], player);
		}
	}
}

void SpawnWitch(int lvl)
{
	constexpr int PinnedItemCount = 3;
	constexpr std::array<_item_indexes, PinnedItemCount> PinnedItemTypes = { IDI_MANA, IDI_FULLMANA, IDI_PORTAL };
	constexpr int MaxPinnedBookCount = 4;
	constexpr std::array<_item_indexes, MaxPinnedBookCount> PinnedBookTypes = { IDI_BOOK1, IDI_BOOK2, IDI_BOOK3, IDI_BOOK4 };

	int bookCount = 0;
	const int pinnedBookCount = gbIsHellfire ? GenerateRnd(MaxPinnedBookCount) : 0;
	const int reservedItems = gbIsHellfire ? 10 : 17;
	const int itemCount = GenerateRnd(WITCH_ITEMS - reservedItems) + 10;
	const int maxValue = gbIsHellfire ? MaxVendorValueHf : MaxVendorValue;

	for (int i = 0; i < WITCH_ITEMS; i++) {
		Item &item = witchitem[i];
		item = {};

		if (i < PinnedItemCount) {
			item._iSeed = AdvanceRndSeed();
			GetItemAttrs(item, PinnedItemTypes[i], 1);
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
			GetItemAttrs(item, itemData, lvl);
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

	SortVendor(witchitem + PinnedItemCount);
}

void SpawnBoy(int lvl)
{
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

	if (boylevel >= (lvl / 2) && !boyitem.isEmpty())
		return;
	do {
		keepgoing = false;
		boyitem = {};
		boyitem._iSeed = AdvanceRndSeed();
		SetRndSeed(boyitem._iSeed);
		_item_indexes itype = RndBoyItem(*MyPlayer, lvl);
		GetItemAttrs(boyitem, itype, lvl);
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
	// Megaplan Phase 1 gambling (2026-08-16): Wirt is the gambler now. The item is UNIDENTIFIED
	// on his table - you buy the base and the roll reveals itself in your pack (BoyBuyItem flips
	// the flag). The magic roll happened above exactly as it always did, seeded and validation-
	// safe (CF_BOY recreation replays the same stream); only the RELEASE of the knowledge moved
	// to the purchase. That IS the gamble, and it is exactly how Gheed sold it in Diablo II.
	boyitem._iIdentified = false;
	boylevel = lvl / 2;
}

void SpawnHealer(int lvl)
{
	constexpr int PinnedItemCount = 2;
	constexpr std::array<_item_indexes, PinnedItemCount + 1> PinnedItemTypes = { IDI_HEAL, IDI_FULLHEAL, IDI_RESURRECT };
	const int itemCount = GenerateRnd(gbIsHellfire ? 10 : 8) + 10;

	for (int i = 0; i < 20; i++) {
		Item &item = healitem[i];
		item = {};

		if (i < PinnedItemCount || (gbIsMultiplayer && i == PinnedItemCount)) {
			item._iSeed = AdvanceRndSeed();
			GetItemAttrs(item, PinnedItemTypes[i], 1);
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
		GetItemAttrs(item, itype, lvl);
		item._iCreateInfo = lvl | CF_HEALER;
		item._iIdentified = true;
	}

	SortVendor(healitem + PinnedItemCount);
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

void CreateSpellBook(Point position, SpellID ispell, bool sendmsg, bool delta)
{
	int lvl = currlevel;

	if (gbIsHellfire) {
		lvl = GetSpellBookLevel(ispell) + 1;
		if (lvl < 1) {
			return;
		}
	}

	_item_indexes idx = RndTypeItems(ItemType::Misc, IMISC_BOOK, lvl);
	if (ActiveItemCount >= MAXITEMS)
		return;

	int ii = AllocateItem();
	auto &item = Items[ii];

	while (true) {
		item = {};
		SetupAllItems(*MyPlayer, item, idx, AdvanceRndSeed(), 2 * lvl, 1, true, false, delta,
		    /*allowTieredRoll=*/true, std::nullopt, /*itemLevel=*/lvl);
		if (item._iMiscId == IMISC_BOOK && item._iSpell == ispell)
			break;
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
std::mt19937 BetterRng;

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

		std::string tmp = AsciiStrToLower(testItem._iIName);
		if (tmp.find(itemName) == std::string::npos)
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

		std::string tmp = AsciiStrToLower(testItem._iIName);
		if (tmp.find(itemName) == std::string::npos)
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

std::string DebugSpawnEquipmentSet(std::optional<OracoolItemTier> tier, bool magical, string_view namePrefix)
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
	UniqueItem uniqueItem;
	bool foundUnique = false;
	int uniqueIndex = 0;
	for (int j = 0; UniqueItems[j].UIItemId != UITYPE_INVALID; j++) {
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
		for (auto &flag : UniqueItemFlags)
			flag = true;
		UniqueItemFlags[uniqueIndex] = false;
		SetupAllItems(*MyPlayer, testItem, uniqueBaseIndex, testItem._iMiscId == IMISC_UNIQUE ? uniqueIndex : AdvanceRndSeed(), uniqueItem.UIMinLvl, 1, false, false, false);
		for (auto &flag : UniqueItemFlags)
			flag = false;

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
	int8_t it = GetItemDropAnimIndex(_iCurs);
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
		int8_t spellLevel = player._pSplLvl[static_cast<int8_t>(_iSpell)];
		while (spellLevel != 0) {
			_iMinMag += 20 * _iMinMag / 100;
			spellLevel--;
			if (_iMinMag + 20 * _iMinMag / 100 > 255) {
				_iMinMag = 255;
				spellLevel = 0;
			}
		}
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
		// rolled prefix/suffix (see GetItemPower/GetStaffPower/GetTieredItemAffixes). This
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
	if (item._iDurability == item._iMaxDur) {
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

bool ApplyOilToItem(Item &item, Player &player)
{
	int r;

	if (item._iClass == ICLASS_MISC) {
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
		if (item._iMaxDur == DUR_INDESTRUCTIBLE)
			return true;
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
		if (item._iMaxDur != DUR_INDESTRUCTIBLE && item._iMaxDur < 200) {
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
			item._iAC += GenerateRnd(2) + 1;
		}
		break;
	case IMISC_OILIMP:
		if (item._iAC < 120) {
			item._iAC += GenerateRnd(3) + 3;
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

} // namespace devilution
