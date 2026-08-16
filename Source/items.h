/**
 * @file items.h
 *
 * Interface of item functionality.
 */
#pragma once

#include <array>
#include <cstdint>
#include <limits>

#include "DiabloUI/ui_flags.hpp"
#include "engine.h"
#include "engine/animationinfo.h"
#include "engine/point.hpp"
#include "itemdat.h"
#include "monster.h"
#include "utils/attributes.h"
#include "utils/stdcompat/optional.hpp"
#include "utils/string_or_view.hpp"

namespace devilution {

#define MAXITEMS 127
#define ITEMTYPES 43

#define GOLD_SMALL_LIMIT 1000
#define GOLD_MEDIUM_LIMIT 2500
#define GOLD_MAX_LIMIT 5000

/**
 * @brief Gold Stacks Buff's raised per-stack gold cap.
 *
 * Previously tied to std::numeric_limits<uint16_t>::max() (65,535) under the assumption that
 * the compact ItemPack.wValue save field (used only for the character-select preview and
 * multiplayer, both of which are irrelevant here since this buff is single-player-only) was
 * the real ceiling. Tracing the actual save path showed that's not true: single-player's
 * authoritative item data (SaveItem/LoadItemData, _ivalue as int32_t) already supports far
 * more, and LoadMatchingItems fully overwrites the compact/truncated copy with it on every
 * load - so 65,535 was never actually protecting anything. This value is chosen well below
 * INT_MAX instead: StoreGoldFit (stores.cpp) multiplies MaxGold by an item's inventory-grid
 * cell count (up to ~10 for the largest 2-handed items), so a value must stay comfortably
 * clear of signed 32-bit overflow at that multiplier - 100,000,000 leaves a wide margin while
 * still being far beyond anything a real single-player game would accumulate.
 */
constexpr int GoldStackSaveLimit = 100'000'000;

// Item indestructible durability
#define DUR_INDESTRUCTIBLE 255

constexpr int MaxVendorValue = 140000;
constexpr int MaxVendorValueHf = 200000;
constexpr int MaxBoyValue = 90000;
constexpr int MaxBoyValueHf = 200000;

enum item_quality : uint8_t {
	ITEM_QUALITY_NORMAL,
	ITEM_QUALITY_MAGIC,
	ITEM_QUALITY_UNIQUE,
};

/**
 * @brief Oracool item tier, layered on top of the vanilla item_quality system.
 *
 * A tiered item is still generated from (and mechanically behaves as) a heavily-rolled
 * magic item - its resolved stat bonuses live in the existing _iPL* fields exactly like
 * any other magic item, and those already round-trip through the unmodified save format.
 * This tier identity, together with the individual affix list below, is purely additional
 * identity/display data layered on top; losing it (e.g. an old save, or a corrupted
 * extension record) degrades an item back to looking/behaving like a plain magic item,
 * never destroys it or its stats.
 */
enum class OracoolItemTier : uint8_t {
	None = 0,
	Rare = 1,
	BuffedUnique = 2,
	Primal = 3,
};

/** @brief One named affix (prefix or suffix) contributing to an Oracool-tiered item. */
struct OracoolAffix {
	item_effect_type type = IPL_INVALID;
	int32_t param1 = 0;
	int32_t param2 = 0;
};

enum _unique_items : int8_t {
	UITEM_CLEAVER,
	UITEM_SKCROWN,
	UITEM_INFRARING,
	UITEM_OPTAMULET,
	UITEM_TRING,
	UITEM_HARCREST,
	UITEM_STEELVEIL,
	UITEM_ARMOFVAL,
	UITEM_GRISWOLD,
	UITEM_BOVINE,
	UITEM_RIFTBOW,
	UITEM_NEEDLER,
	UITEM_CELESTBOW,
	UITEM_DEADLYHUNT,
	UITEM_BOWOFDEAD,
	UITEM_BLKOAKBOW,
	UITEM_FLAMEDART,
	UITEM_FLESHSTING,
	UITEM_WINDFORCE,
	UITEM_EAGLEHORN,
	UITEM_GONNAGALDIRK,
	UITEM_DEFENDER,
	UITEM_GRYPHONCLAW,
	UITEM_BLACKRAZOR,
	UITEM_GIBBOUSMOON,
	UITEM_ICESHANK,
	UITEM_EXECUTIONER,
	UITEM_BONESAW,
	UITEM_SHADHAWK,
	UITEM_WIZSPIKE,
	UITEM_LGTSABRE,
	UITEM_FALCONTALON,
	UITEM_INFERNO,
	UITEM_DOOMBRINGER,
	UITEM_GRIZZLY,
	UITEM_GRANDFATHER,
	UITEM_MANGLER,
	UITEM_SHARPBEAK,
	UITEM_BLOODLSLAYER,
	UITEM_CELESTAXE,
	UITEM_WICKEDAXE,
	UITEM_STONECLEAV,
	UITEM_AGUHATCHET,
	UITEM_HELLSLAYER,
	UITEM_MESSERREAVER,
	UITEM_CRACKRUST,
	UITEM_JHOLMHAMM,
	UITEM_CIVERBS,
	UITEM_CELESTSTAR,
	UITEM_BARANSTAR,
	UITEM_GNARLROOT,
	UITEM_CRANBASH,
	UITEM_SCHAEFHAMM,
	UITEM_DREAMFLANGE,
	UITEM_STAFFOFSHAD,
	UITEM_IMMOLATOR,
	UITEM_STORMSPIRE,
	UITEM_GLEAMSONG,
	UITEM_THUNDERCALL,
	UITEM_PROTECTOR,
	UITEM_NAJPUZZLE,
	UITEM_MINDCRY,
	UITEM_RODOFONAN,
	UITEM_SPIRITSHELM,
	UITEM_THINKINGCAP,
	UITEM_OVERLORDHELM,
	UITEM_FOOLSCREST,
	UITEM_GOTTERDAM,
	UITEM_ROYCIRCLET,
	UITEM_TORNFLESH,
	UITEM_GLADBANE,
	UITEM_RAINCLOAK,
	UITEM_LEATHAUT,
	UITEM_WISDWRAP,
	UITEM_SPARKMAIL,
	UITEM_SCAVCARAP,
	UITEM_NIGHTSCAPE,
	UITEM_NAJPLATE,
	UITEM_DEMONSPIKE,
	UITEM_DEFLECTOR,
	UITEM_SKULLSHLD,
	UITEM_DRAGONBRCH,
	UITEM_BLKOAKSHLD,
	UITEM_HOLYDEF,
	UITEM_STORMSHLD,
	UITEM_BRAMBLE,
	UITEM_REGHA,
	UITEM_BLEEDER,
	UITEM_CONSTRICT,
	UITEM_ENGAGE,
	UITEM_INVALID = -1,
};

/*
CF_LEVEL: Item Level (6 bits; value ranges from 0-63)
CF_ONLYGOOD: Item is not able to have affixes with PLOK set to false
CF_UPER15: Item is from a Unique Monster and has 15% chance of being a Unique Item
CF_UPER1: Item is from the dungeon and has a 1% chance of being a Unique Item
CF_UNIQUE: Item is a Unique Item
CF_SMITH: Item is from Griswold (Basic)
CF_SMITHPREMIUM: Item is from Griswold (Premium)
CF_BOY: Item is from Wirt
CF_WITCH: Item is from Adria
CF_HEALER: Item is from Pepin
CF_PREGEN: Item is pre-generated, mostly associated with Quest items found in the dungeon or potions on the dungeon floor

Items that have both CF_UPER15 and CF_UPER1 are CF_USEFUL, which is used to generate Potions and Town Portal scrolls on the dungeon floor
Items that have any of CF_SMITH, CF_SMITHPREMIUM, CF_BOY, CF_WICTH, and CF_HEALER are CF_TOWN, indicating the item is sourced from an NPC
*/
enum icreateinfo_flag {
	// clang-format off
	CF_LEVEL        = (1 << 6) - 1,
	CF_ONLYGOOD     = 1 << 6,
	CF_UPER15       = 1 << 7,
	CF_UPER1        = 1 << 8,
	CF_UNIQUE       = 1 << 9,
	CF_SMITH        = 1 << 10,
	CF_SMITHPREMIUM = 1 << 11,
	CF_BOY          = 1 << 12,
	CF_WITCH        = 1 << 13,
	CF_HEALER       = 1 << 14,
	CF_PREGEN       = 1 << 15,

	CF_USEFUL = CF_UPER15 | CF_UPER1,
	CF_TOWN   = CF_SMITH | CF_SMITHPREMIUM | CF_BOY | CF_WITCH | CF_HEALER,
	// clang-format on
};

enum icreateinfo_flag2 {
	// clang-format off
	CF_HELLFIRE = 1,
	// clang-format on
};

// All item animation frames have this width.
constexpr int ItemAnimWidth = 96;

// Defined in player.h, forward declared here to allow for functions which operate in the context of a player.
struct Player;

struct Item {
	/** Randomly generated identifier */
	uint32_t _iSeed = 0;
	uint16_t _iCreateInfo = 0;
	ItemType _itype = ItemType::None;
	bool _iAnimFlag = false;
	Point position = { 0, 0 };
	/*
	 * @brief Contains Information for current Animation
	 */
	AnimationInfo AnimInfo;
	bool _iDelFlag = false; // set when item is flagged for deletion, deprecated in 1.02
	uint8_t _iSelFlag = 0;
	bool _iPostDraw = false;
	bool _iIdentified = false;
	item_quality _iMagical = ITEM_QUALITY_NORMAL;
	char _iName[64] = {};
	char _iIName[64] = {};
	item_equip_type _iLoc = ILOC_NONE;
	item_class _iClass = ICLASS_NONE;
	/** @brief Item graphic id. uint16_t, not uint8_t - see item_cursor_graphic. */
	uint16_t _iCurs = 0;
	int _ivalue = 0;
	int _iIvalue = 0;
	uint8_t _iMinDam = 0;
	uint8_t _iMaxDam = 0;
	int16_t _iAC = 0;
	ItemSpecialEffect _iFlags = ItemSpecialEffect::None;
	item_misc_id _iMiscId = IMISC_NONE;
	SpellID _iSpell = SpellID::Null;
	_item_indexes IDidx = IDI_NONE;
	int _iCharges = 0;
	int _iMaxCharges = 0;
	int _iDurability = 0;
	int _iMaxDur = 0;
	int16_t _iPLDam = 0;
	int16_t _iPLToHit = 0;
	int16_t _iPLAC = 0;
	int16_t _iPLStr = 0;
	int16_t _iPLMag = 0;
	int16_t _iPLDex = 0;
	int16_t _iPLVit = 0;
	int16_t _iPLFR = 0;
	int16_t _iPLLR = 0;
	int16_t _iPLMR = 0;
	int16_t _iPLMana = 0;
	int16_t _iPLHP = 0;
	int16_t _iPLDamMod = 0;
	int16_t _iPLGetHit = 0;
	int16_t _iPLLight = 0;
	int8_t _iSplLvlAdd = 0;
	bool _iRequest = false;
	/** Unique item ID, used as an index into UniqueItemList */
	int _iUid = 0;
	int16_t _iFMinDam = 0;
	int16_t _iFMaxDam = 0;
	int16_t _iLMinDam = 0;
	int16_t _iLMaxDam = 0;
	int16_t _iPLEnAc = 0;
	enum item_effect_type _iPrePower = IPL_INVALID;
	enum item_effect_type _iSufPower = IPL_INVALID;
	int _iVAdd1 = 0;
	int _iVMult1 = 0;
	int _iVAdd2 = 0;
	int _iVMult2 = 0;
	int8_t _iMinStr = 0;
	uint8_t _iMinMag = 0;
	int8_t _iMinDex = 0;
	bool _iStatFlag = false;
	ItemSpecialEffectHf _iDamAcFlags = ItemSpecialEffectHf::None;
	uint32_t dwBuff = 0;

	/**
	 * @brief Clears this item and returns the old value
	 */
	Item pop() &
	{
		Item temp = std::move(*this);
		clear();
		return temp;
	}

	/**
	 * @brief Resets the item so isEmpty() returns true without needing to reinitialise the whole object
	 */
	DVL_REINITIALIZES void clear()
	{
		this->_itype = ItemType::None;
	}

	/**
	 * @brief Checks whether this item is empty or not.
	 * @return 'True' in case the item is empty and 'False' otherwise.
	 */
	bool isEmpty() const
	{
		return this->_itype == ItemType::None;
	}

	/**
	 * @brief Checks whether this item is an equipment.
	 * @return 'True' in case the item is an equipment and 'False' otherwise.
	 */
	bool isEquipment() const
	{
		if (this->isEmpty()) {
			return false;
		}

		switch (this->_iLoc) {
		case ILOC_AMULET:
		case ILOC_ARMOR:
		case ILOC_HELM:
		case ILOC_ONEHAND:
		case ILOC_RING:
		case ILOC_TWOHAND:
		// Oracool: the six new worn locations. This predicate gates CanEquip, so leaving them out
		// would make every one of the new items permanently unequippable, silently.
		case ILOC_SHOULDERS:
		case ILOC_BRACERS:
		case ILOC_GLOVES:
		case ILOC_WAIST:
		case ILOC_LEGS:
		case ILOC_BOOTS:
			return true;

		default:
			return false;
		}
	}

	/**
	 * @brief Whether this is one of the six worn types Oracool added (shoulders, bracers, gloves,
	 * belt, legs, boots).
	 *
	 * Deliberately separate from isArmor() rather than folded into it: isArmor() also drives the
	 * gamepad's "where does this go" logic in plrctrls.cpp, which assumes an armour is body armour
	 * bound for INVLOC_CHEST. Widening it there would send boots to the chest slot.
	 */
	bool isOracoolWorn() const
	{
		return !this->isEmpty() && IsOracoolItemType(this->_itype);
	}

	/**
	 * @brief Checks whether this item is a weapon.
	 * @return 'True' in case the item is a weapon and 'False' otherwise.
	 */
	bool isWeapon() const
	{
		if (this->isEmpty()) {
			return false;
		}

		switch (this->_itype) {
		case ItemType::Axe:
		case ItemType::Bow:
		case ItemType::Mace:
		case ItemType::Staff:
		case ItemType::Sword:
			return true;

		default:
			return false;
		}
	}

	/**
	 * @brief Checks whether this item is an armor.
	 * @return 'True' in case the item is an armor and 'False' otherwise.
	 */
	bool isArmor() const
	{
		if (this->isEmpty()) {
			return false;
		}

		switch (this->_itype) {
		case ItemType::HeavyArmor:
		case ItemType::LightArmor:
		case ItemType::MediumArmor:
			return true;

		default:
			return false;
		}
	}

	/**
	 * @brief Checks whether this item is a helm.
	 * @return 'True' in case the item is a helm and 'False' otherwise.
	 */
	bool isHelm() const
	{
		return !this->isEmpty() && this->_itype == ItemType::Helm;
	}

	/**
	 * @brief Checks whether this item is a shield.
	 * @return 'True' in case the item is a shield and 'False' otherwise.
	 */
	bool isShield() const
	{
		return !this->isEmpty() && this->_itype == ItemType::Shield;
	}

	/**
	 * @brief Checks whether this item is a jewelry.
	 * @return 'True' in case the item is a jewelry and 'False' otherwise.
	 */
	bool isJewelry() const
	{
		if (this->isEmpty()) {
			return false;
		}

		switch (this->_itype) {
		case ItemType::Amulet:
		case ItemType::Ring:
			return true;

		default:
			return false;
		}
	}

	[[nodiscard]] bool isScroll() const
	{
		return IsAnyOf(_iMiscId, IMISC_SCROLL, IMISC_SCROLLT);
	}

	[[nodiscard]] bool isScrollOf(SpellID spellId) const
	{
		return isScroll() && _iSpell == spellId;
	}

	[[nodiscard]] bool isRune() const
	{
		return _iMiscId > IMISC_RUNEFIRST && _iMiscId < IMISC_RUNELAST;
	}

	[[nodiscard]] bool isRuneOf(SpellID spellId) const
	{
		if (!isRune())
			return false;
		switch (_iMiscId) {
		case IMISC_RUNEF:
			return spellId == SpellID::RuneOfFire;
		case IMISC_RUNEL:
			return spellId == SpellID::RuneOfLight;
		case IMISC_GR_RUNEL:
			return spellId == SpellID::RuneOfNova;
		case IMISC_GR_RUNEF:
			return spellId == SpellID::RuneOfImmolation;
		case IMISC_RUNES:
			return spellId == SpellID::RuneOfStone;
		default:
			return false;
		}
	}

	/** @brief Maximum number of units a stackable consumable may hold in one Item. */
	static constexpr int MaxStackCount = 99;

	/**
	 * @brief Number of units represented by this Item, for stackable consumables.
	 * Stored in dwBuff bits 1-7 (bit 0 is CF_HELLFIRE). A decoded value of 0 (every
	 * legacy item, and every item that has never called setStackCount) reads as 1.
	 */
	[[nodiscard]] int stackCount() const
	{
		int count = static_cast<int>((dwBuff & 0xFEu) >> 1);
		return count == 0 ? 1 : count;
	}

	void setStackCount(int count)
	{
		if (count < 1)
			count = 1;
		if (count > MaxStackCount)
			count = MaxStackCount;
		dwBuff = (dwBuff & ~static_cast<uint32_t>(0xFE)) | (static_cast<uint32_t>(count) << 1);
	}

	[[nodiscard]] bool isStackableConsumable() const
	{
		// Item::clear() only resets _itype (isEmpty()'s check) and deliberately leaves
		// every other field as stale leftover data from whatever previously occupied
		// this slot, since vanilla code always fully overwrites a cleared slot via
		// assignment rather than reading its other fields. isStackableConsumable() must
		// not treat that leftover _iMiscId/_iClass/IDidx as real, or a cleared slot can
		// look like a valid merge target for an unrelated incoming item.
		if (isEmpty())
			return false;
		if (_iClass == ICLASS_QUEST)
			return false;
		if (_iMiscId == IMISC_ARENAPOT)
			return false;
		if (_iMiscId > IMISC_USEFIRST && _iMiscId < IMISC_USELAST)
			return true; // potions and elixirs, including the Special Elixir's siblings
		if (isScroll())
			return true;
		if (_iMiscId == IMISC_BOOK)
			return true;
		if (_iMiscId == IMISC_SPECELIX)
			return true;
		if (_iMiscId > IMISC_OILFIRST && _iMiscId < IMISC_OILLAST)
			return true; // Hellfire oils
		// Oracool (user request, 2026-08-16): gems, skulls and runes stack like any other
		// consumable. They ARE consumables - a socketing material is spent the moment it goes into
		// an item, exactly as a potion is - and they drop in the quantities that make an unstacked
		// backpack unusable: seven gem types across five qualities, plus 33 runes.
		//
		// Identified by base-item INDEX rather than by _iMiscId, which is what socketing already
		// does (see oracool/gems.h): these carry no misc id of their own, and the index is the
		// whole of their identity - which is also why they merge safely, since two gems with the
		// same index are genuinely interchangeable.
		if (IsOracoolGemIdx(IDidx) || IsOracoolRuneIdx(IDidx))
			return true;
		return false;
	}

	[[nodiscard]] bool canStackWith(const Item &other) const
	{
		// Deliberately compares _iMiscId (+ _iSpell for scrolls, where _iMiscId is the
		// generic IMISC_SCROLL for every spell) rather than IDidx. Diablo's own item table
		// carries two separate _item_indexes for several potions - one reserved for vendor
		// stock and starting gear (Pepin's Heal/Full Heal, Adria's Mana/Full Mana, a new
		// character's two starting belt potions), another that monster/floor drops actually
		// use - both displaying as the exact same potion with no visible difference. IDidx
		// would treat those as different items and refuse to stack them; _iMiscId is shared
		// by both underlying indices, since it's what actually determines the potion's kind.
		//
		// Oracool: _iIdentified is deliberately NOT compared here (it used to be, and that was
		// itself a bug). Every stackable consumable (potions, scrolls, books, oils) is always
		// ITEM_QUALITY_NORMAL, and normal-quality items are already treated as identified for
		// every display/behavior purpose regardless of this flag's actual value (see the
		// _iMagical == ITEM_QUALITY_NORMAL checks elsewhere in this file). But the flag's raw
		// value is NOT consistent across sources: SetupItem() (items.cpp) unconditionally sets
		// it false for every monster/floor drop, only flipped true afterward if Auto Identify
		// Drops is on, while vendor stock and starting gear set it true directly. A
		// dungeon-dropped mana potion with the option off could therefore have _iIdentified=false
		// while an otherwise-identical one bought from Adria has it true - comparing this field
		// made two visually and functionally identical potions silently refuse to stack.
		if (!isStackableConsumable() || !other.isStackableConsumable())
			return false;

		// Oracool (2026-08-16): the _iMiscId rule above is right for POTIONS, whose duplicate
		// indices are the problem it was written for. It is dangerously wrong for gems and runes,
		// which carry no misc id at all - every one of them is IMISC_NONE, so _iMiscId alone would
		// happily stack a Ruby into an Emerald and a Tir into an El.
		//
		// Caught by OracoolCrafting.AscendRunesConsumesPairAndProducesNextRung the moment these
		// became stackable: the crafted Tir merged into the El pile it was made from and the recipe
		// produced nothing. For these two families the base-item INDEX is the whole identity - the
		// same rule socketing already uses - so it is what has to match.
		const bool eitherIsMaterial = IsOracoolGemIdx(IDidx) || IsOracoolRuneIdx(IDidx)
		    || IsOracoolGemIdx(other.IDidx) || IsOracoolRuneIdx(other.IDidx);
		if (eitherIsMaterial)
			return IDidx == other.IDidx;

		return _iMiscId == other._iMiscId && _iSpell == other._iSpell;
	}

	/** @brief Maximum number of prefix (or suffix) affixes an Oracool-tiered item may carry. */
	static constexpr int MaxOracoolAffixesPerSlot = 3;

	OracoolItemTier _iOracoolTier = OracoolItemTier::None;
	bool _iOracoolPerfectRoll = false;
	uint8_t _iOracoolPrefixCount = 0;
	uint8_t _iOracoolSuffixCount = 0;
	std::array<OracoolAffix, MaxOracoolAffixesPerSlot> _iOracoolPrefixes;
	std::array<OracoolAffix, MaxOracoolAffixesPerSlot> _iOracoolSuffixes;

	/**
	 * @brief Single-player only: true once this equipped item's durability reached 0 and was
	 * left in place, inactive, instead of being destroyed (see CalcSelfItems, which clears
	 * _iStatFlag for a broken item the same way it already does for invalid/unmet-requirement
	 * items). Cleared by repairing at Griswold. Local-save-only, matching _iOracoolTier - not
	 * synced to multiplayer or hero export, since the always-destroy behavior is untouched there.
	 */
	bool _iOracoolBroken = false;

	/** @brief Megaplan Phase 1: sockets. The cap is 3 - D1 items are smaller than D2's and the
	 * economy tighter; three is enough for every planned runeword. */
	static constexpr int MaxItemSockets = 3;
	/** @brief The sentinel in _iSocketed for an empty socket (no _item_indexes uses it). */
	static constexpr uint16_t EmptySocket = 0xFFFF;
	/**
	 * @brief Phase 1 sockets: how many sockets this item was born with (rolled only on
	 * NORMAL-quality tierless equipment - which is what makes plain "basic items" the raw
	 * material of the socket economy, exactly as in Diablo II), and what sits in each.
	 *
	 * A socketed gem or rune is identified by its base-item index alone (_item_indexes as
	 * uint16_t): gems are fixed-effect, non-magical items, so the index fully describes one.
	 * Persisted in the fixed item extension record (loadsave.cpp, OracoolItemFormatVersion 3);
	 * like the tier data, this survives via the full-record heroitems/stash paths, not the
	 * seed-replay ItemPack.
	 */
	uint8_t _iSocketCount = 0;
	uint16_t _iSocketed[MaxItemSockets] = { EmptySocket, EmptySocket, EmptySocket };

	/**
	 * @brief Phase 1 ethereal: rolled at drop time on equipment of any quality (5%). The bargain
	 * is stamped into the item's own stats when it rolls (+35% AC or max damage, max durability
	 * halved) and persisted through the full-record paths like everything else; this flag's job
	 * is the REFUSALS - repair paths decline it - and the description line. Version 4 of the item
	 * extension record.
	 */
	bool _iOracoolEthereal = false;

	/** @brief Filled-socket count, derived. */
	[[nodiscard]] int socketedCount() const
	{
		int filled = 0;
		for (const uint16_t idx : _iSocketed) {
			if (idx != EmptySocket)
				filled++;
		}
		return filled;
	}

	/** @brief Whether this item has at least one empty socket. */
	[[nodiscard]] bool hasOpenSocket() const
	{
		return _iSocketCount > 0 && socketedCount() < _iSocketCount;
	}

	/**
	 * @brief Whether this item genuinely carries an Oracool tier (Rare/Buffed Unique/Primal).
	 *
	 * Item::clear() only resets _itype (isEmpty()'s check) and deliberately leaves every
	 * other field - including _iOracoolTier - as stale leftover data from whatever
	 * previously occupied this slot. Callers must never trust _iOracoolTier on a cleared
	 * item; this accessor guards against that the same way isStackableConsumable() does.
	 */
	[[nodiscard]] bool hasOracoolTier() const
	{
		return !isEmpty() && _iOracoolTier != OracoolItemTier::None;
	}

	[[nodiscard]] bool isUsable() const;

	[[nodiscard]] bool keyAttributesMatch(uint32_t seed, _item_indexes itemIndex, uint16_t createInfo) const
	{
		return _iSeed == seed && IDidx == itemIndex && _iCreateInfo == createInfo;
	}

	UiFlags getTextColor() const
	{
		// Oracool: user correction (2026-08-15) - the material lines (IsOracoolItemIdx) are "more
		// like a line of products than a set in diablo's meaning", so they carry NO colour of their
		// own: a rare Steel Helm is yellow, a unique one gold, exactly like any other item. The
		// green (UiFlags::ColorOracoolGreen) is reserved for the REAL set system - items granting
		// bonuses when a whole set is worn - which is not built yet.
		if (hasOracoolTier()) {
			switch (_iOracoolTier) {
			case OracoolItemTier::Rare:
				return UiFlags::ColorYellow;
			case OracoolItemTier::BuffedUnique:
				return UiFlags::ColorWhitegold;
			case OracoolItemTier::Primal:
				// True cyan isn't available: UiFlags is a fully-packed 32-bit flag enum with
				// no free bit, and no cyan font asset (.trn) exists in the game's data files.
				// Orange was chosen instead - it's distinct from every other item quality and
				// happens to match Diablo 3's own convention for Primal Ancient items.
				return UiFlags::ColorOrange;
			case OracoolItemTier::None:
				break;
			}
		}
		switch (_iMagical) {
		case ITEM_QUALITY_MAGIC:
			return UiFlags::ColorBlue;
		case ITEM_QUALITY_UNIQUE:
			return UiFlags::ColorWhitegold;
		default:
			return UiFlags::ColorWhite;
		}
	}

	UiFlags getTextColorWithStatCheck() const
	{
		if (!_iStatFlag)
			return UiFlags::ColorRed;
		return getTextColor();
	}

	/**
	 * @brief Sets the current Animation for the Item
	 * @param showAnimation Definies if the Animation (Flipping) is shown or if only the final Frame (item on the ground) is shown
	 */
	void setNewAnimation(bool showAnimation);

	/**
	 * @brief If this item is a spell book, calculates the magic requirement to learn a new level, then for all items sets _iStatFlag
	 * @param player Player to compare stats against requirements
	 */
	void updateRequiredStatsCacheForPlayer(const Player &player);

	/** @brief Returns the translated item name to display (respects identified flag) */
	StringOrView getName() const;
};

struct ItemGetRecordStruct {
	uint32_t nSeed;
	uint16_t wCI;
	int nIndex;
	uint32_t dwTimestamp;
};

struct CornerStoneStruct {
	Point position;
	bool activated;
	Item item;
	bool isAvailable();
};

/** Contains the items on ground in the current game. */
extern DVL_API_FOR_TEST Item Items[MAXITEMS + 1];
extern DVL_API_FOR_TEST uint8_t ActiveItems[MAXITEMS];
extern DVL_API_FOR_TEST uint8_t ActiveItemCount;
/** Contains the location of dropped items. */
extern int8_t dItem[MAXDUNX][MAXDUNY];
extern CornerStoneStruct CornerStone;
extern bool UniqueItemFlags[128];

uint8_t GetOutlineColor(const Item &item, bool checkReq);
/**
 * @brief Oracool: the price an item actually sells for at a vendor - identified magical/unique
 * items use their real value (_iIvalue), everything else uses the base value (_ivalue), both cut
 * to a quarter and floored at 1, multiplied by stack count for a stackable consumable. Matches the
 * formula `PopulateSellList` (stores.cpp) already computes per item when populating a sell list,
 * factored out here so Oracool's inventory sort button can rank items the same way Griswold prices
 * them without duplicating the formula.
 */
int GetItemSellValue(const Item &item);
bool IsItemAvailable(int i);
bool IsUniqueAvailable(int i);
bool CreateUniqueVendorItem(const Player &player, Item &item, _unique_items uid);
void ClearUniqueItemFlags();
void InitItemGFX();
void InitItems();
void CalcPlrItemVals(Player &player, bool Loadgfx);
void CalcPlrInv(Player &player, bool Loadgfx);
void InitializeItem(Item &item, _item_indexes itemData);
void GenerateNewSeed(Item &h);
int GetGoldCursor(int value);

/**
 * @brief Update the gold cursor on the given gold item
 * @param gold The item to update
 */
void SetPlrHandGoldCurs(Item &gold);
void CreatePlrItems(Player &player);
bool ItemSpaceOk(Point position);
int AllocateItem();
/**
 * @brief Moves the item onto the floor of the current dungeon level
 * @param item The source of the item data, should not be used after calling this function
 * @param position Coordinates of the tile to place the item on
 * @return The index assigned to the item
 */
uint8_t PlaceItemInWorld(Item &&item, WorldTilePosition position);
Point GetSuperItemLoc(Point position);
void GetItemAttrs(Item &item, _item_indexes itemData, int lvl);
void SetupItem(Item &item);
/**
 * @brief Rolls a Rare item's affixes (1-2 prefixes + 1-2 suffixes, at least one of each in
 * the common case) onto an already-base-initialized item, tagging it OracoolItemTier::Rare.
 * Exposed here (rather than kept file-local to items.cpp) so tests can exercise the affix
 * selection rules directly.
 */
void GetRareItemAffixes(const Player &player, Item &item, int minlvl, int maxlvl, AffixItemType flgs, bool onlygood, bool ignoreLevelLimits = false);
/**
 * @brief Rolls a Buffed Unique item's affixes (2-3 prefixes + 2-3 suffixes, at least two of
 * each in the common case) onto an already-base-initialized item, tagging it
 * OracoolItemTier::BuffedUnique. Same rules and identity bookkeeping as GetRareItemAffixes,
 * just with a higher minimum per slot.
 */
void GetBuffedUniqueItemAffixes(const Player &player, Item &item, int minlvl, int maxlvl, AffixItemType flgs, bool onlygood, bool ignoreLevelLimits = false);
/**
 * @brief Rolls a Primal item's affixes: always exactly 3 prefixes + 3 suffixes, every one a
 * "perfect roll" (forced to the maximum end of its declared range) and always beneficial-only,
 * tagging it OracoolItemTier::Primal and setting _iOracoolPerfectRoll.
 */
void GetPrimalItemAffixes(const Player &player, Item &item, int minlvl, int maxlvl, AffixItemType flgs, bool onlygood, bool ignoreLevelLimits = false);
/**
 * @brief Oracool-tiered-item equivalent of CalcItemValue. addTotal/multTotal are the caller's own
 * running sums of each rolled affix's PLVal price contribution and multVal - these can't be
 * re-derived later from the stored OracoolAffix entries (which hold the displayed stat magnitude,
 * not the price scaling - see GetTieredItemAffixes), so the caller must accumulate them while
 * generating the affixes and pass them in directly.
 */
void CalcOracoolTieredItemValue(Item &item, int addTotal, int multTotal);
/**
 * @brief Detects and corrects the v0.3.42 "price value stored instead of the real roll" bug
 * (see GetTieredItemAffixes) on a single item that may have been generated before that fix. A
 * no-op for items without an Oracool tier, or that are already correct. Call whenever an item is
 * loaded or picked up so already-affected items self-heal without needing a new save format or
 * player-facing repair action. Logs an Event Log entry (see oracool/event_log.h) whenever it
 * actually corrects something, so the change isn't silent.
 * @return true if a correction was made.
 */
bool RepairOracoolAffixesIfCorrupted(Item &item);
/** @brief The word placed before the base item name for a tiered item's display name, e.g. "Rare {base}". */
string_view GetOracoolTierLabel(OracoolItemTier tier);
/**
 * @brief The description-panel line shown under the belt row for a tiered item ("rare item" /
 * "unique item" / "primal item"). Exposed here so tests can verify the wording per tier directly.
 */
string_view GetOracoolTierPanelLabel(OracoolItemTier tier);
Item *SpawnUnique(_unique_items uid, Point position, std::optional<int> level = std::nullopt, bool sendmsg = true, bool exactPosition = false);
void SpawnItem(Monster &monster, Point position, bool sendmsg, bool spawn = false);
void CreateRndItem(Point position, bool onlygood, bool sendmsg, bool delta);
void CreateRndUseful(Point position, bool sendmsg);
void CreateTypeItem(Point position, bool onlygood, ItemType itemType, int imisc, bool sendmsg, bool delta, bool spawn = false);
void RecreateItem(const Player &player, Item &item, _item_indexes idx, uint16_t icreateinfo, uint32_t iseed, int ivalue, bool isHellfire);
void RecreateEar(Item &item, uint16_t ic, uint32_t iseed, uint8_t bCursval, string_view heroName);
void CornerstoneSave();
void CornerstoneLoad(Point position);
void SpawnQuestItem(_item_indexes itemid, Point position, int randarea, int selflag, bool sendmsg);
void SpawnRewardItem(_item_indexes itemid, Point position, bool sendmsg);
void SpawnMapOfDoom(Point position, bool sendmsg);
void SpawnRuneBomb(Point position, bool sendmsg);
void SpawnTheodore(Point position, bool sendmsg);
void RespawnItem(Item &item, bool FlipFlag);
void DeleteItem(int i);
void ProcessItems();
void FreeItemGFX();
void GetItemFrm(Item &item);

/**
 * @brief Oracool: the drop animation an item of @p animIndex tumbles to the ground with.
 *
 * These are the CELs named in ItemDropNames, loaded per level into itemanims. They are the only
 * animations in the game of an OBJECT falling and settling - every other sprite of a weapon is
 * either a static inventory icon or a character swinging one - which is what makes them worth
 * exposing: a missile that wants to look like a thrown shield or a falling mace has nowhere else to
 * get the frames.
 *
 * Empty before InitItems has run, and after FreeItemGFX; callers must tolerate that.
 */
OptionalClxSpriteList GetItemDropAnim(int8_t animIndex);

/** @brief Drop-animation index of the mace - a mace falling to the ground and settling. */
constexpr int8_t MaceDropAnimIndex = 6;
/** @brief Drop-animation index of the shield - a shield tumbling end over end. */
constexpr int8_t ShieldDropAnimIndex = 7;
void GetItemStr(Item &item);
/** @brief Oracool: the set items' own drop hook - see items.cpp. Called from SpawnLoot after the vanilla rolls. */
void TrySpawnOracoolSetItem(const Monster &monster, bool sendmsg);
/** @brief Phase 1: the gems' own drop hook, same pool-exclusion reasoning. Called from SpawnLoot. */
void TrySpawnOracoolGem(const Monster &monster, bool sendmsg);
/** @brief Phase 1: rolls sockets onto a freshly dropped item - only plain NORMAL-quality
 * equipment, only on the drop paths, never inside the seed-replayed SetupAllItems. */
void TryAddSocketsToDroppedItem(Item &item);
/** @brief Phase 1: the ethereal roll (5% of durable equipment, any quality): +35% primary stats,
 * half max durability, unrepairable. Drop paths only, same seed-replay rule as the sockets. */
void TryMakeDroppedItemEthereal(Item &item);
/** @brief Phase 1: Magic/Gold Find consumption - scales dropped gold by _pGoldFind and gives
 * plain equipment a _pMagicFind% chance to upgrade to a Rare-tier roll. Drop tail only. */
void ApplyMagicAndGoldFindToDrop(Item &item, int mLevel);
/**
 * @brief tabIdx selects an Oracool Tabbed Inventory extra tab (0-8) instead of the vanilla
 * InvBody/InvList encoding cii would otherwise resolve through - pass -1 (the default) for the
 * original behavior.
 */
void CheckIdentify(Player &player, int cii, int tabIdx = -1);
void DoRepair(Player &player, int cii, int tabIdx = -1);
void DoRecharge(Player &player, int cii, int tabIdx = -1);
bool DoOil(Player &player, int cii, int tabIdx = -1);
[[nodiscard]] StringOrView PrintItemPower(char plidx, const Item &item);
/** @brief Like PrintItemPower, but reads a Rare/Buffed Unique/Primal item's own per-affix value instead of the item's shared accumulated field - see the definition for why that distinction matters. */
[[nodiscard]] StringOrView PrintOracoolAffixPower(const OracoolAffix &affix, const Item &item);
void PrintItemDetails(const Item &item);
void PrintItemDur(const Item &item);
void UseItem(size_t pnum, item_misc_id Mid, SpellID spellID, int spellFrom);
bool UseItemOpensHive(const Item &item, Point position);
bool UseItemOpensGrave(const Item &item, Point position);
void SpawnSmith(int lvl);
void SpawnPremium(const Player &player);
void SpawnWitch(int lvl);
void SpawnBoy(int lvl);
void SpawnHealer(int lvl);
void MakeGoldStack(Item &goldItem, int value);
int ItemNoFlippy();
void CreateSpellBook(Point position, SpellID ispell, bool sendmsg, bool delta);
void CreateMagicArmor(Point position, ItemType itemType, int icurs, bool sendmsg, bool delta);
void CreateAmulet(Point position, int lvl, bool sendmsg, bool delta, bool spawn = false);
void CreateMagicWeapon(Point position, ItemType itemType, int icurs, bool sendmsg, bool delta);
bool GetItemRecord(uint32_t nSeed, uint16_t wCI, int nIndex);
void SetItemRecord(uint32_t nSeed, uint16_t wCI, int nIndex);
void PutItemRecord(uint32_t nSeed, uint16_t wCI, int nIndex);

/**
 * @brief Resets item get records.
 */
void initItemGetRecords();

void RepairItem(Item &item, int lvl);
void RechargeItem(Item &item, Player &player);
bool ApplyOilToItem(Item &item, Player &player);
/**
 * @brief Checks if the item is generated in vanilla hellfire. If yes it updates dwBuff to include CF_HELLFIRE.
 */
void UpdateHellfireFlag(Item &item, const char *identifiedItemName);

#ifdef _DEBUG
bool WouldSurviveNetworkValidation(const Item &item, _item_indexes idx);
std::string DebugSpawnItem(std::string itemName);
std::string DebugSpawnTieredItem(std::string itemName, OracoolItemTier tier);
/** @brief Oracool: the base item each give*set slot spawns - first-in-table for an empty prefix,
 * case-insensitive (pre-lowercased) name-prefix match otherwise. Declared here for items_test:
 * this selection is exactly what a user's "givebset bone" resolves through, and the test exists
 * because "it reads correct" was asserted once already and turned out to matter. */
_item_indexes FirstBaseItemForEquipLocation(item_equip_type loc, string_view namePrefix = {});
/** @brief Oracool: give{b,m,r,u,p}set - one item for every equipment slot at once. Pass no tier
 * and magical=false for the plain set; no tier and magical=true for the magic set.
 *
 * namePrefix, when non-empty, picks each slot's base item by case-insensitive item-name prefix
 * ("steel", "diamond", ...) instead of first-in-table - the way to reach the eight-tier set
 * items, which all sit behind the leather items in AllItemsList and are otherwise unreachable
 * from these commands (user report: "all assets seem to be of the same type"). */
std::string DebugSpawnEquipmentSet(std::optional<OracoolItemTier> tier, bool magical, string_view namePrefix = {});
std::string DebugSpawnUniqueItem(std::string itemName);
#endif
/* data */

extern DVL_API_FOR_TEST int MaxGold;

extern int8_t ItemCAnimTbl[];
/** @brief Ground-drop animation index for an item graphic. Use this rather than indexing
 * ItemCAnimTbl directly - Oracool's own icon ids sit past the end of that array. */
int8_t GetItemDropAnimIndex(uint16_t curs);
extern _sfx_id ItemInvSnds[];

} // namespace devilution
