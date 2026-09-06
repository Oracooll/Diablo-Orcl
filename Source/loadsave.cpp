/**
 * @file loadsave.cpp
 *
 * Implementation of save game functionality.
 */
#include "loadsave.h"

#include <climits>
#include <cstdint>
#include <cstring>
#include <numeric>
#include <unordered_map>

#include <SDL.h>
#include <fmt/core.h>

#include "automap.h"
#include "codec.h"
#include "control.h"
#include "cursor.h"
#include "dead.h"
#include "doom.h"
#include "engine.h"
#include "engine/point.hpp"
#include "engine/random.hpp"
#include "init.h"
#include "inv.h"
#include "lighting.h"
#include "menu.h"
#include "missiles.h"
#include "monster.h"
#include "mpq/mpq_common.hpp"
#include "oracool/save_status.h"
#include "oracool/auto_save.h"
#include "oracool/item_tiers.h"
#include "oracool/mystic_orbs.h"
#include "oracool/readied_spells.h"
#include "pfile.h"
#include "playerdat.hpp"
#include "plrmsg.h"
#include "qol/stash.h"
#include "quests.h"
#include "stores.h"
#include "utils/endian_read.hpp"
#include "utils/language.h"
#include "utils/stdcompat/algorithm.hpp"

namespace devilution {

bool gbIsHellfireSaveGame;
uint8_t giNumberOfLevels;

namespace {

constexpr size_t MaxMissilesForSaveGame = 125;

uint8_t giNumberQuests;
uint8_t giNumberOfSmithPremiumItems;

template <class T>
T SwapLE(T in)
{
	switch (sizeof(T)) {
	case 2:
		return SDL_SwapLE16(in);
	case 4:
		return SDL_SwapLE32(in);
	case 8:
		return SDL_SwapLE64(in);
	default:
		return in;
	}
}

template <class T>
T SwapBE(T in)
{
	switch (sizeof(T)) {
	case 2:
		return SDL_SwapBE16(in);
	case 4:
		return SDL_SwapBE32(in);
	case 8:
		return static_cast<T>(SDL_SwapBE64(in));
	default:
		return in;
	}
}

class LoadHelper {
	std::unique_ptr<byte[]> m_buffer_;
	size_t m_cur_ = 0;
	size_t m_size_;

	template <class T>
	T Next()
	{
		const auto size = sizeof(T);
		if (!IsValid(size))
			return 0;

		T value;
		memcpy(&value, &m_buffer_[m_cur_], size);
		m_cur_ += size;

		return value;
	}

public:
	LoadHelper(std::optional<SaveReader> archive, const char *szFileName)
	{
		if (archive)
			m_buffer_ = ReadArchive(*archive, szFileName, &m_size_);
		else
			m_buffer_ = nullptr;
	}

	bool IsValid(size_t size = 1)
	{
		return m_buffer_ != nullptr
		    && m_size_ >= (m_cur_ + size);
	}

	size_t Size()
	{
		return m_size_;
	}

	template <typename T>
	constexpr void Skip(size_t count = 1)
	{
		Skip(sizeof(T) * count);
	}

	void Skip(size_t size)
	{
		m_cur_ += size;
	}

	void NextBytes(void *bytes, size_t size)
	{
		if (!IsValid(size))
			return;

		memcpy(bytes, &m_buffer_[m_cur_], size);
		m_cur_ += size;
	}

	template <class T>
	T NextLE()
	{
		return SwapLE(Next<T>());
	}

	template <class T>
	T NextBE()
	{
		return SwapBE(Next<T>());
	}

	template <class TSource, class TDesired>
	TDesired NextLENarrow(TSource modifier = 0)
	{
		static_assert(sizeof(TSource) > sizeof(TDesired), "Can only narrow to a smaller type");
		TSource value = SwapLE(Next<TSource>()) + modifier;
		return static_cast<TDesired>(clamp<TSource>(value, std::numeric_limits<TDesired>::min(), std::numeric_limits<TDesired>::max()));
	}

	bool NextBool8()
	{
		return Next<uint8_t>() != 0;
	}

	bool NextBool32()
	{
		return Next<uint32_t>() != 0;
	}
};

class SaveHelper {
	SaveWriter &m_mpqWriter;
	const char *m_szFileName_;
	std::unique_ptr<byte[]> m_buffer_;
	size_t m_cur_ = 0;
	size_t m_capacity_;
	/** @brief Set when a write was dropped for want of room. See WriteBytes and the destructor. */
	bool m_overran_ = false;

public:
	SaveHelper(SaveWriter &mpqWriter, const char *szFileName, size_t bufferLen)
	    : m_mpqWriter(mpqWriter)
	    , m_szFileName_(szFileName)
	    , m_buffer_(new byte[codec_get_encoded_len(bufferLen)])
	    , m_capacity_(bufferLen)
	{
	}

	bool IsValid(size_t len = 1)
	{
		return m_buffer_ != nullptr
		    && m_capacity_ >= (m_cur_ + len);
	}

	template <typename T>
	constexpr void Skip(size_t count = 1)
	{
		Skip(sizeof(T) * count);
	}

	void Skip(size_t len)
	{
		std::memset(&m_buffer_[m_cur_], 0, len);
		m_cur_ += len;
	}

	void WriteBytes(const void *bytes, size_t len)
	{
		if (!IsValid(len)) {
			// SILENT TRUNCATION, until 2026-08-22. This returned without a word, so a caller that
			// under-declared its buffer wrote a short file and found out much later and somewhere
			// else - loadsave's own round-trip test HUNG rather than failing, and the cause turned
			// out to be that OracoolItemExtensionSaveSize had not counted v8's gold-find bytes for
			// four versions.
			//
			// An overrun is never intentional: every caller declares a size it computed from what
			// it is about to write. Recording it lets the destructor say so.
			m_overran_ = true;
			return;
		}

		memcpy(&m_buffer_[m_cur_], bytes, len);
		m_cur_ += len;
	}

	template <class T>
	void WriteLE(T value)
	{
		value = SwapLE(value);
		WriteBytes(&value, sizeof(value));
	}

	template <class T>
	void WriteBE(T value)
	{
		value = SwapBE(value);
		WriteBytes(&value, sizeof(value));
	}

	~SaveHelper()
	{
		if (m_overran_) {
			// Loud, and on the way OUT rather than at the dropped write, because by then the file
			// is already short and the next thing that happens is a load reading past its end. A
			// save that quietly lost bytes is worse than one that refused: the player keeps
			// playing, and the damage surfaces as a corrupt character much later.
			app_fatal(StrCat("Save buffer for \"", m_szFileName_,
			    "\" was too small - the record grew without its size constant. This is a bug; "
			    "please report it rather than continuing, as the save is incomplete."));
		}
		const auto encodedLen = codec_get_encoded_len(m_cur_);
		const char *const password = pfile_get_password();
		codec_encode(m_buffer_.get(), m_cur_, encodedLen, password);
		// Oracool (audit, 2026-08-26): the result is no longer discarded. A destructor cannot
		// report a failure to its caller, so it is recorded where the save path can see it - see
		// oracool::NoteSaveWriteFailed. The archive itself now keeps the previous record when a
		// write fails (MpqWriter::WriteFile), so this is about TELLING somebody rather than about
		// the data; a save that quietly did not happen is how a player loses an evening.
		if (!m_mpqWriter.WriteFile(m_szFileName_, m_buffer_.get(), encodedLen))
			oracool::NoteSaveWriteFailed(m_szFileName_);
	}
};

struct MonsterConversionData {
	int8_t monsterLevel;
	uint16_t experience;
	uint8_t toHit;
	uint8_t toHitSpecial;
};

struct LevelConversionData {
	MonsterConversionData monsterConversionData[MaxMonsters];
};

/**
 * @brief Oracool item tier/affix data format version (v0.2.0+).
 *
 * Rare/Buffed Unique/Primal item tier identity, up to three prefixes and three suffixes each,
 * and a perfect-roll flag are folded directly into SaveItem/LoadItemData's fixed-size item
 * record, so every container that already calls those two functions - the backpack, belt,
 * equipped slots, the Stash, dropped ground items on every level, and Tabbed Inventory's extra
 * tabs - carries tier data with no per-container wiring to remember. This single version byte,
 * checked wherever a container's own leading header is read (SaveHeroItems/LoadHeroItems,
 * SaveStash/LoadStash, SaveInventoryTabs/LoadInventoryTabs), exists purely to reject a
 * pre-v0.2.0 save cleanly: LoadItemData's record grew when this data was added, so blindly
 * reading an old, shorter record here would silently misalign every subsequent field instead
 * of failing loudly.
 */
// Version 3 added the socket fields; version 4 the ethereal flag - see SaveItem/LoadItemData.
// Version 7 (Sockets v2) widened the socket block from 3 slots to 6, because socket allowance is
// now an item's inventory footprint and the largest footprint is 2x3. The record grew 6 bytes, so
// this is a real break rather than a tail extension - an older save is rejected here.
// Version 8 appends _iPLGoldFind, so an item can carry the gold bonus that ItemBonusTotals,
// Player::_pGoldFind and the drop tail were all already able to consume. A tail extension rather
// than a widening, but the version still moves: the record grew, and a v7 reader would run off the
// end of every item.
// Version 9 (D2MXL-to-ORCL Phase 1) appends TWO fields: _iPLMagicFind, the twin of v8's gold-find
// channel, and _iOracoolOrbCount.
//
// The orb count is the first per-item value in this fork that is not derived from something. The
// base tier, the ethereal roll and every affix come out of the item's own seed; a monster variant
// and a boss trait come out of the monster's. A count of Mystic Orbs is a PLAYER DECISION, and a
// decision has nowhere to be recomputed from - so it is stored, and storing it is what moves the
// version. Paid once, deliberately: the growing charms in Phase 3 want the same byte.
constexpr uint8_t OracoolItemFormatVersion = 9;

bool IsOracoolAffixTypeValid(item_effect_type type)
{
	// The bound moves with every appended power, and forgetting it is how a new power would load
	// back as IPL_INVALID on every existing item - silently, and only after a save/load round trip.
	return type == IPL_INVALID || (type >= 0 && type <= IPL_MAGICFIND);
}

void LoadItemData(LoadHelper &file, Item &item)
{
	item._iSeed = file.NextLE<uint32_t>();
	item._iCreateInfo = file.NextLE<uint16_t>();
	file.Skip(2); // Alignment
	item._itype = static_cast<ItemType>(file.NextLE<uint32_t>());
	item.position.x = file.NextLE<int32_t>();
	item.position.y = file.NextLE<int32_t>();
	item._iAnimFlag = file.NextBool32();
	file.Skip(4); // Skip pointer _iAnimData
	item.AnimInfo = {};
	item.AnimInfo.numberOfFrames = file.NextLENarrow<int32_t, int8_t>();
	item.AnimInfo.currentFrame = file.NextLENarrow<int32_t, int8_t>(-1);
	file.Skip(8); // Skip _iAnimWidth and _iAnimWidth2
	file.Skip(4); // Unused since 1.02
	item._iSelFlag = file.NextLE<uint8_t>();
	file.Skip(3); // Alignment
	item._iPostDraw = file.NextBool32();
	item._iIdentified = file.NextBool32();
	item._iMagical = static_cast<item_quality>(file.NextLE<int8_t>());
	file.NextBytes(item._iName, 64);
	file.NextBytes(item._iIName, 64);
	item._iLoc = static_cast<item_equip_type>(file.NextLE<int8_t>());
	item._iClass = static_cast<item_class>(file.NextLE<uint8_t>());
	file.Skip(1); // Alignment
	item._iCurs = file.NextLE<int32_t>();
	item._ivalue = file.NextLE<int32_t>();
	item._iIvalue = file.NextLE<int32_t>();
	item._iMinDam = file.NextLE<int32_t>();
	item._iMaxDam = file.NextLE<int32_t>();
	item._iAC = file.NextLE<int32_t>();
	item._iFlags = static_cast<ItemSpecialEffect>(file.NextLE<uint32_t>());
	item._iMiscId = static_cast<item_misc_id>(file.NextLE<int32_t>());
	item._iSpell = static_cast<SpellID>(file.NextLE<int32_t>());
	item._iCharges = file.NextLE<int32_t>();
	item._iMaxCharges = file.NextLE<int32_t>();
	item._iDurability = file.NextLE<int32_t>();
	item._iMaxDur = file.NextLE<int32_t>();
	item._iPLDam = file.NextLE<int32_t>();
	item._iPLToHit = file.NextLE<int32_t>();
	item._iPLAC = file.NextLE<int32_t>();
	item._iPLStr = file.NextLE<int32_t>();
	item._iPLMag = file.NextLE<int32_t>();
	item._iPLDex = file.NextLE<int32_t>();
	item._iPLVit = file.NextLE<int32_t>();
	item._iPLFR = file.NextLE<int32_t>();
	item._iPLLR = file.NextLE<int32_t>();
	item._iPLMR = file.NextLE<int32_t>();
	item._iPLMana = file.NextLE<int32_t>();
	item._iPLHP = file.NextLE<int32_t>();
	item._iPLDamMod = file.NextLE<int32_t>();
	item._iPLGetHit = file.NextLE<int32_t>();
	item._iPLLight = file.NextLE<int32_t>();
	item._iSplLvlAdd = file.NextLE<int8_t>();
	item._iRequest = file.NextBool8();
	file.Skip(2); // Alignment
	item._iUid = file.NextLE<int32_t>();
	item._iFMinDam = file.NextLE<int32_t>();
	item._iFMaxDam = file.NextLE<int32_t>();
	item._iLMinDam = file.NextLE<int32_t>();
	item._iLMaxDam = file.NextLE<int32_t>();
	item._iPLEnAc = file.NextLE<int32_t>();
	item._iPrePower = static_cast<item_effect_type>(file.NextLE<int8_t>());
	item._iSufPower = static_cast<item_effect_type>(file.NextLE<int8_t>());
	file.Skip(2); // Alignment
	item._iVAdd1 = file.NextLE<int32_t>();
	item._iVMult1 = file.NextLE<int32_t>();
	item._iVAdd2 = file.NextLE<int32_t>();
	item._iVMult2 = file.NextLE<int32_t>();
	item._iMinStr = file.NextLE<int8_t>();
	item._iMinMag = file.NextLE<uint8_t>();
	item._iMinDex = file.NextLE<int8_t>();
	file.Skip(1); // Alignment
	item._iStatFlag = file.NextBool32();
	item.IDidx = static_cast<_item_indexes>(file.NextLE<int32_t>());
	if (gbIsSpawn) {
		item.IDidx = RemapItemIdxFromSpawn(item.IDidx);
	}
	if (!gbIsHellfireSaveGame) {
		item.IDidx = RemapItemIdxFromDiablo(item.IDidx);
	}
	item.dwBuff = file.NextLE<uint32_t>();
	if (gbIsHellfireSaveGame)
		item._iDamAcFlags = static_cast<ItemSpecialEffectHf>(file.NextLE<uint32_t>());
	else
		item._iDamAcFlags = ItemSpecialEffectHf::None;
	UpdateHellfireFlag(item, item._iIName);

	// Oracool item tier/affix data (v0.2.0+); see the matching write in SaveItem. Callers
	// that need to reject a pre-v0.2.0 save outright (where this data simply isn't present
	// at this offset) do so before ever reaching here - see OracoolItemFormatVersion.
	const uint8_t rawTier = file.NextLE<uint8_t>();
	// Clamped against the LAST tier, not a named one.
	//
	// Bug (fixed 2026-08-16, user report: "all set item i acquired with debug commands turned into
	// uniques with suspicious stats"). This read `<= Primal`, which was every tier when it was
	// written. OracoolItemTier::Set arrived at 4 the same day, so a set item saved its tier
	// correctly and then had it silently reset to None on the way back in - "turned into", after a
	// round trip through the save.
	//
	// The result reads as garbage rather than as a plain item, which is why it looked alarming.
	// MakeSetItem marks a set piece ITEM_QUALITY_UNIQUE, so with the tier gone the description
	// falls through to the vanilla unique branch and prints UniqueItems[_iUid] - and _iUid is 0 on
	// a set item, which is The Butcher's Cleaver. Its three powers are exactly what the screenshot
	// showed: "+0 to strength" (IPL_STR, printed from the item's own _iPLStr, which the Cleaver's
	// powers never touched), "unusual item damage" (IPL_SETDAM) and "altered durability"
	// (IPL_SETDUR).
	//
	// OracoolItemTier::LAST is now what this compares against, so the next tier added cannot
	// reintroduce it by being forgotten here.
	item._iOracoolTier = rawTier <= static_cast<uint8_t>(OracoolItemTier::LAST)
	    ? static_cast<OracoolItemTier>(rawTier)
	    : OracoolItemTier::None;
	item._iOracoolPerfectRoll = file.NextLE<uint8_t>() != 0;
	item._iOracoolBroken = file.NextLE<uint8_t>() != 0;
	const uint8_t prefixCount = file.NextLE<uint8_t>();
	const uint8_t suffixCount = file.NextLE<uint8_t>();
	item._iOracoolPrefixCount = std::min<uint8_t>(prefixCount, Item::MaxOracoolAffixesPerSlot);
	item._iOracoolSuffixCount = std::min<uint8_t>(suffixCount, Item::MaxOracoolAffixesPerSlot);
	for (OracoolAffix &affix : item._iOracoolPrefixes) {
		const auto type = static_cast<item_effect_type>(file.NextLE<int8_t>());
		affix.type = IsOracoolAffixTypeValid(type) ? type : IPL_INVALID;
		affix.param1 = file.NextLE<int32_t>();
		affix.param2 = file.NextLE<int32_t>();
	}
	for (OracoolAffix &affix : item._iOracoolSuffixes) {
		const auto type = static_cast<item_effect_type>(file.NextLE<int8_t>());
		affix.type = IsOracoolAffixTypeValid(type) ? type : IPL_INVALID;
		affix.param1 = file.NextLE<int32_t>();
		affix.param2 = file.NextLE<int32_t>();
	}

	// Megaplan Phase 1 sockets (OracoolItemFormatVersion 3+): count plus one gem/rune base-item
	// index per slot, EmptySocket for a hole. Clamped on read like every other extension field.
	item._iSocketCount = std::min<uint8_t>(file.NextLE<uint8_t>(), Item::MaxItemSockets);
	for (uint16_t &socket : item._iSocketed) {
		const uint16_t stored = file.NextLE<uint16_t>();
		// The COUNT was clamped and the CONTENTS were not, which is the half that mattered: a socket
		// holds a base-item index and several readers use it to subscript AllItemsList directly -
		// the hover line (gems.cpp), the stone overlay (socket_overlay.cpp), extraction. A corrupt
		// or crafted value indexed off the end of that table (external audit, 2026-08-25, P0).
		//
		// Only the three families a socket can actually hold are accepted; anything else becomes an
		// empty socket. A player who loses a gem to a damaged save is annoyed; a player whose game
		// reads past a global array is not necessarily even told.
		const bool socketable = IsOracoolGemIdx(stored) || IsOracoolRuneIdx(stored)
		    || IsOracoolJewelIdx(stored);
		socket = (stored == Item::EmptySocket || socketable) ? stored : Item::EmptySocket;
	}
	// Version 4: the ethereal flag.
	item._iOracoolEthereal = file.NextLE<uint8_t>() != 0;
	// Version 5: ilvl (oracool/area_level.h). 0 on anything generated before it existed, which prints
	// no ilvl line rather than an invented one.
	item._iOracoolItemLevel = file.NextLE<uint8_t>();
	// Version 6: the base tier. 0 is Normal, which is also what everything made before it existed was.
	item._iOracoolBaseTier = std::min<uint8_t>(file.NextLE<uint8_t>(),
	    static_cast<uint8_t>(oracool::BaseItemTier::LAST));
	// Version 8: the gold-find bonus this item carries.
	item._iPLGoldFind = file.NextLE<int32_t>();
	// Version 9: the magic-find twin, and the Mystic Orb count.
	item._iPLMagicFind = file.NextLE<int32_t>();
	// Clamped on read like every other extension field - a corrupted byte must cost the player an
	// orb or two, never the ability to apply any at all.
	item._iOracoolOrbCount = std::min<uint8_t>(file.NextLE<uint8_t>(), oracool::MaxOrbsPerItem);

	// Self-healing for negative durability (user, 2026-08-27: "i have magic oracool items with
	// negative durability"). Until WearDurabilityPoint landed, gear that broke while equipped kept
	// being decremented every wear tick, so characters made on earlier builds are carrying items
	// reading "Dur: -37/60". Nothing downstream expects a negative - the repair price, the
	// durability bar and the smith's stock line all take the number at face value - and there is no
	// information in how far past zero it went.
	//
	// HERE and not in LoadAndValidateItemData, because the worn slots do not go through that:
	// LoadMatchingItems reads them with LoadItemData directly.
	if (item._iDurability < 0)
		item._iDurability = 0;
}

void LoadAndValidateItemData(LoadHelper &file, Item &item)
{
	LoadItemData(file, item);
	RemoveInvalidItem(item);
	// Oracool: called for every item in every container (inventory, belt, stash, extra tabs,
	// ground on every level) - the single choke point to self-heal any Rare/Buffed Unique/Primal
	// item that was generated before the v0.3.42 affix-value fix, without needing a new save
	// format or a player-facing repair action.
	RepairOracoolAffixesIfCorrupted(item);
}

void LoadPlayer(LoadHelper &file, Player &player)
{
	player._pmode = static_cast<PLR_MODE>(file.NextLE<int32_t>());

	for (int8_t &step : player.walkpath) {
		step = file.NextLE<int8_t>();
	}
	player.plractive = file.NextBool8();
	file.Skip(2); // Alignment
	player.destAction = static_cast<action_id>(file.NextLE<int32_t>());
	player.destParam1 = file.NextLE<int32_t>();
	player.destParam2 = file.NextLE<int32_t>();
	player.destParam3 = file.NextLE<int32_t>();
	player.destParam4 = file.NextLE<int32_t>();
	player.setLevel(file.NextLE<uint32_t>());
	player.position.tile.x = file.NextLE<int32_t>();
	player.position.tile.y = file.NextLE<int32_t>();
	player.position.future.x = file.NextLE<int32_t>();
	player.position.future.y = file.NextLE<int32_t>();
	file.Skip<uint32_t>(2); // Skip _ptargx and _ptargy
	player.position.last.x = file.NextLE<int32_t>();
	player.position.last.y = file.NextLE<int32_t>();
	player.position.old.x = file.NextLE<int32_t>();
	player.position.old.y = file.NextLE<int32_t>();
	file.Skip<int32_t>(4); // Skip offset and velocity
	player._pdir = static_cast<Direction>(file.NextLE<int32_t>());
	file.Skip(4); // Unused
	player._pgfxnum = file.NextLENarrow<uint32_t, uint8_t>();
	file.Skip<uint32_t>(); // Skip pointer pData
	player.AnimInfo = {};
	player.AnimInfo.ticksPerFrame = file.NextLENarrow<int32_t, int8_t>(1);
	player.AnimInfo.tickCounterOfCurrentFrame = file.NextLENarrow<int32_t, int8_t>();
	player.AnimInfo.numberOfFrames = file.NextLENarrow<int32_t, int8_t>();
	player.AnimInfo.currentFrame = file.NextLENarrow<int32_t, int8_t>(-1);
	file.Skip<uint32_t>(3); // Skip _pAnimWidth, _pAnimWidth2, _peflag
	player.lightId = file.NextLE<int32_t>();
	file.Skip<int32_t>(); // _pvid

	player.queuedSpell.spellId = static_cast<SpellID>(file.NextLE<int32_t>());
	player.queuedSpell.spellType = static_cast<SpellType>(file.NextLE<int8_t>());
	auto spellFrom = file.NextLE<int8_t>();
	if (!IsValidSpellFrom(spellFrom))
		spellFrom = 0;
	player.spellFrom = spellFrom;
	player.queuedSpell.spellFrom = spellFrom;
	file.Skip(2); // Alignment
	player.inventorySpell = static_cast<SpellID>(file.NextLE<int32_t>());
	// Oracool: user request (2026-08-15) - the LEFT button's readied spell, in the byte vanilla used
	// for _pTSplType and devilutionX has only ever skipped. The right button's pair is already saved
	// a few lines below; this keeps the two consistent instead of one of them vanishing on a reload.
	// Decoded further down, once _pAblSpells and _pMemSpells are in - see pack.h's pReadiedSpellRight
	// for the encoding and why an unused byte was used rather than a new field.
	const uint8_t packedLeftSpell = file.NextLE<uint8_t>();
	file.Skip(3); // Alignment
	player._pRSpell = static_cast<SpellID>(file.NextLE<int32_t>());
	player._pRSplType = static_cast<SpellType>(file.NextLE<int8_t>());
	file.Skip(3); // Alignment
	player._pSBkSpell = static_cast<SpellID>(file.NextLE<int32_t>());
	file.Skip<int8_t>(); // Skip _pSBkSplType
	for (uint8_t &spellLevel : player._pSplLvl)
		spellLevel = file.NextLE<uint8_t>();
	file.Skip(7); // Alignment
	player._pMemSpells = file.NextLE<uint64_t>();
	player._pAblSpells = file.NextLE<uint64_t>();
	player._pScrlSpells = file.NextLE<uint64_t>();
	player._pSpellFlags = static_cast<SpellFlag>(file.NextLE<uint8_t>());
	file.Skip(3); // Alignment

	oracool::UnpackReadiedSpell(player, packedLeftSpell, player._pLRSpell, player._pLRSplType);

	// Extra hotkeys: to keep single player save compatibility, read only 4 hotkeys here, rely on LoadHotkeys for the rest
	for (size_t i = 0; i < 4; i++) {
		player._pSplHotKey[i] = static_cast<SpellID>(file.NextLE<int32_t>());
	}
	for (size_t i = 0; i < 4; i++) {
		player._pSplTHotKey[i] = static_cast<SpellType>(file.NextLE<uint8_t>());
	}

	file.Skip<int32_t>(); // Skip _pwtype
	player._pBlockFlag = file.NextBool8();
	player._pInvincible = file.NextBool8();
	player._pLightRad = file.NextLE<int8_t>();
	player._pLvlChanging = file.NextBool8();

	file.NextBytes(player._pName, PlayerNameLength);
	player._pClass = static_cast<HeroClass>(file.NextLE<int8_t>());
	file.Skip(3); // Alignment
	player._pStrength = file.NextLE<int32_t>();
	player._pBaseStr = file.NextLE<int32_t>();
	player._pMagic = file.NextLE<int32_t>();
	player._pBaseMag = file.NextLE<int32_t>();
	player._pDexterity = file.NextLE<int32_t>();
	player._pBaseDex = file.NextLE<int32_t>();
	player._pVitality = file.NextLE<int32_t>();
	player._pBaseVit = file.NextLE<int32_t>();
	player._pStatPts = file.NextLE<int32_t>();
	player._pDamageMod = file.NextLE<int32_t>();
	player._pBaseToBlk = file.NextLE<int32_t>();
	if (player._pBaseToBlk == 0)
		player._pBaseToBlk = PlayersData[static_cast<std::size_t>(player._pClass)].blockBonus;
	player._pHPBase = file.NextLE<int32_t>();
	player._pMaxHPBase = file.NextLE<int32_t>();
	player._pHitPoints = file.NextLE<int32_t>();
	player._pMaxHP = file.NextLE<int32_t>();
	file.Skip<int32_t>(); // Skip _pHPPer - always derived from hp and maxHP.
	player._pManaBase = file.NextLE<int32_t>();
	player._pMaxManaBase = file.NextLE<int32_t>();
	player._pMana = file.NextLE<int32_t>();
	player._pMaxMana = file.NextLE<int32_t>();
	file.Skip<int32_t>(); // Skip _pManaPer - always derived from mana and maxMana
	player._pLevel = file.NextLE<int8_t>();
	player._pMaxLvl = file.NextLE<int8_t>();
	file.Skip(2); // Alignment
	/** @brief Oracool: widened to uint64_t - the extended level-99 curve exceeds UINT32_MAX. */
	player._pExperience = file.NextLE<uint64_t>();
	file.Skip<uint32_t>();                        // Skip _pMaxExp - unused
	player._pNextExper = file.NextLE<uint64_t>(); // This can be calculated based on pLevel (which in turn could be calculated based on pExperience)
	player._pArmorClass = file.NextLE<int8_t>();
	player._pMagResist = file.NextLE<int8_t>();
	player._pFireResist = file.NextLE<int8_t>();
	player._pLghtResist = file.NextLE<int8_t>();
	player._pGold = file.NextLE<int32_t>();
	player._pInfraFlag = file.NextBool32();

	int32_t tempPositionX = file.NextLE<int32_t>();
	int32_t tempPositionY = file.NextLE<int32_t>();
	if (player._pmode == PM_WALK_NORTHWARDS) {
		// These values are saved as offsets to remain consistent with old savefiles
		tempPositionX += player.position.tile.x;
		tempPositionY += player.position.tile.y;
	}
	player.position.temp.x = static_cast<WorldTileCoord>(tempPositionX);
	player.position.temp.y = static_cast<WorldTileCoord>(tempPositionY);

	player.tempDirection = static_cast<Direction>(file.NextLE<int32_t>());
	player.queuedSpell.spellLevel = file.NextLE<int32_t>();
	file.Skip<uint32_t>(); // skip _pVar5, was used for storing position of a tile which should have its HorizontalMovingPlayer flag removed after walking
	file.Skip<int32_t>(2); // skip offset2;
	file.Skip<uint32_t>(); // Skip actionFrame

	for (uint8_t i = 0; i < giNumberOfLevels; i++)
		player._pLvlVisited[i] = file.NextBool8();

	for (uint8_t i = 0; i < giNumberOfLevels; i++)
		player._pSLvlVisited[i] = file.NextBool8();

	file.Skip(2);           // Alignment
	file.Skip<uint32_t>();  // skip _pGFXLoad
	file.Skip<uint32_t>(8); // Skip pointers _pNAnim
	player._pNFrames = file.NextLENarrow<int32_t, int8_t>();
	file.Skip<uint32_t>();  // skip _pNWidth
	file.Skip<uint32_t>(8); // Skip pointers _pWAnim
	player._pWFrames = file.NextLENarrow<int32_t, int8_t>();
	file.Skip<uint32_t>();  // skip _pWWidth
	file.Skip<uint32_t>(8); // Skip pointers _pAAnim
	player._pAFrames = file.NextLENarrow<int32_t, int8_t>();
	file.Skip<uint32_t>(); // skip _pAWidth
	player._pAFNum = file.NextLENarrow<int32_t, int8_t>();
	file.Skip<uint32_t>(8); // Skip pointers _pLAnim
	file.Skip<uint32_t>(8); // Skip pointers _pFAnim
	file.Skip<uint32_t>(8); // Skip pointers _pTAnim
	player._pSFrames = file.NextLENarrow<int32_t, int8_t>();
	file.Skip<uint32_t>(); // skip _pSWidth
	player._pSFNum = file.NextLENarrow<int32_t, int8_t>();
	file.Skip<uint32_t>(8); // Skip pointers _pHAnim
	player._pHFrames = file.NextLENarrow<int32_t, int8_t>();
	file.Skip<uint32_t>();  // skip _pHWidth
	file.Skip<uint32_t>(8); // Skip pointers _pDAnim
	player._pDFrames = file.NextLENarrow<int32_t, int8_t>();
	file.Skip<uint32_t>();  // skip _pDWidth
	file.Skip<uint32_t>(8); // Skip pointers _pBAnim
	player._pBFrames = file.NextLENarrow<int32_t, int8_t>();
	file.Skip<uint32_t>(); // skip _pBWidth

	for (Item &item : player.InvBody)
		LoadAndValidateItemData(file, item);

	for (Item &item : player.InvList)
		LoadAndValidateItemData(file, item);

	player._pNumInv = file.NextLE<int32_t>();

	for (int8_t &cell : player.InvGrid)
		cell = file.NextLE<int8_t>();

	for (Item &item : player.SpdList)
		LoadAndValidateItemData(file, item);

	LoadAndValidateItemData(file, player.HoldItem);

	player._pIMinDam = file.NextLE<int32_t>();
	player._pIMaxDam = file.NextLE<int32_t>();
	player._pIAC = file.NextLE<int32_t>();
	player._pIBonusDam = file.NextLE<int32_t>();
	player._pIBonusToHit = file.NextLE<int32_t>();
	player._pIBonusAC = file.NextLE<int32_t>();
	player._pIBonusDamMod = file.NextLE<int32_t>();
	file.Skip(4); // Alignment

	player._pISpells = file.NextLE<uint64_t>();
	player._pIFlags = static_cast<ItemSpecialEffect>(file.NextLE<int32_t>());
	player._pIGetHit = file.NextLE<int32_t>();
	player._pISplLvlAdd = file.NextLE<int8_t>();
	file.Skip(1);         // Unused
	file.Skip(2);         // Alignment
	file.Skip<int32_t>(); // _pISplDur
	player._pIEnAc = file.NextLE<int32_t>();
	player._pIFMinDam = file.NextLE<int32_t>();
	player._pIFMaxDam = file.NextLE<int32_t>();
	player._pILMinDam = file.NextLE<int32_t>();
	player._pILMaxDam = file.NextLE<int32_t>();
	player._pOilType = static_cast<item_misc_id>(file.NextLE<int32_t>());
	player.pTownWarps = file.NextLE<uint8_t>();
	player.pDungMsgs = file.NextLE<uint8_t>();
	player.pLvlLoad = file.NextLE<uint8_t>();

	if (gbIsHellfireSaveGame) {
		player.pDungMsgs2 = file.NextLE<uint8_t>();
	} else {
		player.pDungMsgs2 = 0;
		file.Skip(1); // pBattleNet
	}
	player.pManaShield = file.NextBool8();
	if (gbIsHellfireSaveGame) {
		player.pOriginalCathedral = file.NextBool8();
	} else {
		file.Skip(1);
		player.pOriginalCathedral = true;
	}
	file.Skip(2); // Available bytes
	player.wReflections = file.NextLE<uint16_t>();
	file.Skip(14); // Available bytes

	player.pDiabloKillLevel = file.NextLE<uint32_t>();
	sgGameInitInfo.nDifficulty = static_cast<_difficulty>(file.NextLE<uint32_t>());
	player.pDamAcFlags = static_cast<ItemSpecialEffectHf>(file.NextLE<uint32_t>());
	// Oracool Reset Stats: repurposes 16 of these 20 previously-inert bytes (always zero-filled by
	// SaveHelper::Skip on every prior build) to track manually-spent stat points; see player.h.
	player._pStatPtsSpentStr = file.NextLE<int32_t>();
	player._pStatPtsSpentMag = file.NextLE<int32_t>();
	player._pStatPtsSpentDex = file.NextLE<int32_t>();
	player._pStatPtsSpentVit = file.NextLE<int32_t>();
	file.Skip(4); // Available bytes
	CalcPlrInv(player, false);

	player.executedSpell = player.queuedSpell; // Ensures backwards compatibility

	// Oracool: user request - per-difficulty waypoint unlock table, appended after every vanilla
	// field rather than slotted into the "Available bytes" padding above (not enough of it left -
	// see the Reset Stats comment). Safe for saves made before this field existed: LoadHelper::Next
	// returns 0 (unlocked=false) once the read cursor runs past the end of the buffer, so an old
	// save just loads with every waypoint but Tristram locked, no special-casing needed.
	for (auto &difficultyRow : player._pWaypointUnlocked) {
		for (bool &unlocked : difficultyRow)
			unlocked = file.NextBool8();
	}

	// Omit pointer _pNData
	// Omit pointer _pWData
	// Omit pointer _pAData
	// Omit pointer _pLData
	// Omit pointer _pFData
	// Omit pointer  _pTData
	// Omit pointer _pHData
	// Omit pointer _pDData
	// Omit pointer _pBData
	// Omit pointer pReserved

	// Ensure plrIsOnSetLevel and plrlevel is correctly initialized, cause in vanilla sometimes plrlevel is not updated to setlvlnum
	if (setlevel)
		player.setLevel(setlvlnum);
	else
		player.setLevel(currlevel);
}

bool gbSkipSync = false;

void LoadMonster(LoadHelper *file, Monster &monster, MonsterConversionData *monsterConversionData = nullptr)
{
	monster.levelType = file->NextLE<int32_t>();
	monster.mode = static_cast<MonsterMode>(file->NextLE<int32_t>());
	monster.goal = static_cast<MonsterGoal>(file->NextLE<uint8_t>());
	file->Skip(3); // Alignment
	monster.goalVar1 = file->NextLENarrow<int32_t, int16_t>();
	monster.goalVar2 = file->NextLENarrow<int32_t, int8_t>();
	monster.goalVar3 = file->NextLENarrow<int32_t, int8_t>();
	// Oracool: the champion modifier, in the first of the four bytes this block has always written as
	// unused. No save-format change, and a save written before lesser uniques existed reads zero here
	// - which is LesserUniqueAffix::None, exactly what a monster from such a save should have.
	monster.lesserAffix = static_cast<LesserUniqueAffix>(file->NextLE<uint8_t>());
	// And two more of the same run for the name/tint roll - see Monster::lesserNameSeed for why that
	// could not just be read off aiSeed. One byte of the original four is still spare.
	monster.lesserNameSeed = file->NextLE<uint16_t>();
	file->Skip(1); // Unused
	monster.pathCount = file->NextLE<uint8_t>();
	file->Skip(3); // Alignment
	monster.position.tile.x = file->NextLE<int32_t>();
	monster.position.tile.y = file->NextLE<int32_t>();
	monster.position.future.x = file->NextLE<int32_t>();
	monster.position.future.y = file->NextLE<int32_t>();
	monster.position.old.x = file->NextLE<int32_t>();
	monster.position.old.y = file->NextLE<int32_t>();
	file->Skip<int32_t>(4); // Skip offset and velocity
	monster.direction = static_cast<Direction>(file->NextLE<int32_t>());
	monster.enemy = file->NextLE<int32_t>();
	monster.enemyPosition.x = file->NextLE<uint8_t>();
	monster.enemyPosition.y = file->NextLE<uint8_t>();
	file->Skip(2); // Unused

	file->Skip(4); // Skip pointer _mAnimData
	monster.animInfo = {};
	monster.animInfo.ticksPerFrame = file->NextLENarrow<int32_t, int8_t>();
	// Ensure that we can increase the tickCounterOfCurrentFrame at least once without overflow (needed for backwards compatibility for sitting gargoyles)
	monster.animInfo.tickCounterOfCurrentFrame = file->NextLENarrow<int32_t, int8_t>(1) - 1;
	monster.animInfo.numberOfFrames = file->NextLENarrow<int32_t, int8_t>();
	monster.animInfo.currentFrame = file->NextLENarrow<int32_t, int8_t>(-1);
	file->Skip(4); // Skip _meflag
	monster.isInvalid = file->NextBool32();
	monster.var1 = file->NextLENarrow<int32_t, int16_t>();
	monster.var2 = file->NextLENarrow<int32_t, int16_t>();
	monster.var3 = file->NextLENarrow<int32_t, int8_t>();
	monster.position.temp.x = file->NextLENarrow<int32_t, WorldTileCoord>();
	monster.position.temp.y = file->NextLENarrow<int32_t, WorldTileCoord>();
	file->Skip<int32_t>(2); // skip offset2;
	file->Skip(4);          // Skip actionFrame
	monster.maxHitPoints = file->NextLE<int32_t>();
	monster.hitPoints = file->NextLE<int32_t>();

	monster.ai = static_cast<MonsterAIID>(file->NextLE<uint8_t>());
	monster.intelligence = file->NextLE<uint8_t>();
	file->Skip(2); // Alignment
	monster.flags = file->NextLE<uint32_t>();
	monster.activeForTicks = file->NextLE<uint8_t>();
	file->Skip(3); // Alignment
	file->Skip(4); // Unused
	monster.position.last.x = file->NextLE<int32_t>();
	monster.position.last.y = file->NextLE<int32_t>();
	monster.rndItemSeed = file->NextLE<uint32_t>();
	monster.aiSeed = file->NextLE<uint32_t>();
	file->Skip(4); // Unused

	monster.uniqueType = static_cast<UniqueMonsterType>(file->NextLE<uint8_t>() - 1);
	monster.uniqTrans = file->NextLE<uint8_t>();
	monster.corpseId = file->NextLE<int8_t>();

	monster.whoHit = file->NextLE<int8_t>();
	if (monsterConversionData != nullptr)
		monsterConversionData->monsterLevel = file->NextLE<int8_t>();
	else
		file->Skip(1); // Skip level - now calculated on the fly
	file->Skip(1);     // Alignment
	if (monsterConversionData != nullptr)
		monsterConversionData->experience = file->NextLE<uint16_t>();
	else
		file->Skip(2); // Skip exp - now calculated from monstdat when the monster dies

	if (monsterConversionData != nullptr)
		monsterConversionData->toHit = file->NextLE<uint8_t>();
	else if (monster.isPlayerMinion()) // Don't skip for golems
		monster.golemToHit = file->NextLE<uint8_t>();
	else
		file->Skip(1); // Skip toHit - now calculated on the fly
	monster.minDamage = file->NextLE<uint8_t>();
	monster.maxDamage = file->NextLE<uint8_t>();
	if (monsterConversionData != nullptr)
		monsterConversionData->toHitSpecial = file->NextLE<uint8_t>();
	else
		file->Skip(1); // Skip toHitSpecial - now calculated on the fly
	monster.minDamageSpecial = file->NextLE<uint8_t>();
	monster.maxDamageSpecial = file->NextLE<uint8_t>();
	monster.armorClass = file->NextLE<uint8_t>();
	file->Skip(1); // Alignment
	monster.resistance = file->NextLE<uint16_t>();
	file->Skip(2); // Alignment

	monster.talkMsg = static_cast<_speech_id>(file->NextLE<int32_t>());
	if (monster.talkMsg == TEXT_KING1) // Fix original bad mapping of NONE for monsters
		monster.talkMsg = TEXT_NONE;
	monster.leader = file->NextLE<uint8_t>();
	if (monster.leader == 0)
		monster.leader = Monster::NoLeader; // Golems shouldn't be leaders of other monsters
	monster.leaderRelation = static_cast<LeaderRelation>(file->NextLE<uint8_t>());
	monster.packSize = file->NextLE<uint8_t>();
	monster.lightId = file->NextLE<int8_t>();
	if (monster.lightId == 0)
		monster.lightId = NO_LIGHT; // Correct incorect values in old saves

	// Omit pointer name;

	if (monster.mode == MonsterMode::Petrified)
		monster.animInfo.isPetrified = true;
}

/**
 * @brief Recalculate the pack size of monster group that may have underflown
 */
void SyncPackSize(Monster &leader)
{
	if (!leader.isUnique())
		return;
	if (leader.ai != MonsterAIID::Scavenger)
		return;

	leader.packSize = 0;

	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		auto &minion = Monsters[ActiveMonsters[i]];
		if (minion.leaderRelation == LeaderRelation::Leashed && minion.getLeader() == &leader)
			leader.packSize++;
	}
}

void LoadMissile(LoadHelper *file)
{
	Missile missile = {};
	missile._mitype = static_cast<MissileID>(file->NextLE<int32_t>());
	missile.position.tile.x = file->NextLE<int32_t>();
	missile.position.tile.y = file->NextLE<int32_t>();
	missile.position.offset.deltaX = file->NextLE<int32_t>();
	missile.position.offset.deltaY = file->NextLE<int32_t>();
	missile.position.velocity.deltaX = file->NextLE<int32_t>();
	missile.position.velocity.deltaY = file->NextLE<int32_t>();
	missile.position.start.x = file->NextLE<int32_t>();
	missile.position.start.y = file->NextLE<int32_t>();
	missile.position.traveled.deltaX = file->NextLE<int32_t>();
	missile.position.traveled.deltaY = file->NextLE<int32_t>();
	missile._mimfnum = file->NextLE<int32_t>();
	missile._mispllvl = file->NextLE<int32_t>();
	missile._miDelFlag = file->NextBool32();
	missile._miAnimType = static_cast<MissileGraphicID>(file->NextLE<uint8_t>());
	file->Skip(3); // Alignment
	missile._miAnimFlags = static_cast<MissileGraphicsFlags>(file->NextLE<int32_t>());
	file->Skip(4); // Skip pointer _miAnimData
	missile._miAnimDelay = file->NextLE<int32_t>();
	missile._miAnimLen = file->NextLE<int32_t>();
	missile._miAnimWidth = file->NextLE<int32_t>();
	missile._miAnimWidth2 = file->NextLE<int32_t>();
	missile._miAnimCnt = file->NextLE<int32_t>();
	missile._miAnimAdd = file->NextLE<int32_t>();
	missile._miAnimFrame = file->NextLE<int32_t>();
	missile._miDrawFlag = file->NextBool32();
	missile._miLightFlag = file->NextBool32();
	missile._miPreFlag = file->NextBool32();
	missile._miUniqTrans = file->NextLE<uint32_t>();
	missile._mirange = file->NextLE<int32_t>();
	missile._misource = file->NextLE<int32_t>();
	missile._micaster = static_cast<mienemy_type>(file->NextLE<int32_t>());
	missile._midam = file->NextLE<int32_t>();
	missile._miHitFlag = file->NextBool32();
	missile._midist = file->NextLE<int32_t>();
	missile._mlid = file->NextLE<int32_t>();
	missile._mirnd = file->NextLE<int32_t>();
	missile.var1 = file->NextLE<int32_t>();
	missile.var2 = file->NextLE<int32_t>();
	missile.var3 = file->NextLE<int32_t>();
	missile.var4 = file->NextLE<int32_t>();
	missile.var5 = file->NextLE<int32_t>();
	missile.var6 = file->NextLE<int32_t>();
	missile.var7 = file->NextLE<int32_t>();
	missile.limitReached = file->NextBool32();
	missile.lastCollisionTargetHash = 0;
	if (Missiles.size() < Missiles.max_size()) {
		Missiles.push_back(missile);
	}
}

_object_id ConvertFromHellfireObject(_object_id type)
{
	if (leveltype == DTYPE_NEST) {
		switch (type) {
		case OBJ_BARREL:
			return OBJ_POD;
		case OBJ_BARRELEX:
			return OBJ_PODEX;
		default:
			break;
		}
	}

	if (leveltype == DTYPE_CRYPT) {
		switch (type) {
		case OBJ_BARREL:
			return OBJ_URN;
		case OBJ_BARRELEX:
			return OBJ_URNEX;
		case OBJ_STORYBOOK:
			return OBJ_L5BOOKS;
		case OBJ_STORYCANDLE:
			return OBJ_L5CANDLE;
		case OBJ_L1LDOOR:
			return OBJ_L5LDOOR;
		case OBJ_L1RDOOR:
			return OBJ_L5RDOOR;
		case OBJ_LEVER:
			return OBJ_L5LEVER;
		case OBJ_SARC:
			return OBJ_L5SARC;
		default:
			break;
		}
	}

	return type;
}

void LoadObject(LoadHelper &file, Object &object)
{
	object._otype = ConvertFromHellfireObject(static_cast<_object_id>(file.NextLE<int32_t>()));
	object.position.x = file.NextLE<int32_t>();
	object.position.y = file.NextLE<int32_t>();
	object.applyLighting = file.NextBool32();
	object._oAnimFlag = file.NextBool32();
	file.Skip(4); // Skip pointer _oAnimData
	object._oAnimDelay = file.NextLE<int32_t>();
	object._oAnimCnt = file.NextLE<int32_t>();
	object._oAnimLen = file.NextLE<uint32_t>();
	object._oAnimFrame = file.NextLE<uint32_t>();
	object._oAnimWidth = static_cast<uint16_t>(file.NextLE<int32_t>());
	file.Skip(4); // Skip _oAnimWidth2
	object._oDelFlag = file.NextBool32();
	object._oBreak = file.NextLE<int8_t>();
	file.Skip(3); // Alignment
	object._oSolidFlag = file.NextBool32();
	object._oMissFlag = file.NextBool32();

	object._oSelFlag = file.NextLE<int8_t>();
	file.Skip(3); // Alignment
	object._oPreFlag = file.NextBool32();
	object._oTrapFlag = file.NextBool32();
	object._oDoorFlag = file.NextBool32();
	object._olid = file.NextLE<int32_t>();
	object._oRndSeed = file.NextLE<uint32_t>();
	object._oVar1 = file.NextLE<int32_t>();
	object._oVar2 = file.NextLE<int32_t>();
	object._oVar3 = file.NextLE<int32_t>();
	object._oVar4 = file.NextLE<int32_t>();
	object._oVar5 = file.NextLE<int32_t>();
	object._oVar6 = file.NextLE<uint32_t>();
	object.bookMessage = static_cast<_speech_id>(file.NextLE<int32_t>());
	object._oVar8 = file.NextLE<int32_t>();
}

void LoadItem(LoadHelper &file, Item &item)
{
	LoadAndValidateItemData(file, item);
	GetItemFrm(item);
}

void LoadPremium(LoadHelper &file, int i)
{
	LoadAndValidateItemData(file, premiumitems[i]);
}

void LoadQuest(LoadHelper *file, int i)
{
	auto &quest = Quests[i];

	quest._qlevel = file->NextLE<uint8_t>();
	file->Skip<uint8_t>(); // _qtype, identical to _qidx
	quest._qactive = static_cast<quest_state>(file->NextLE<uint8_t>());
	quest._qlvltype = static_cast<dungeon_type>(file->NextLE<uint8_t>());
	quest.position.x = file->NextLE<int32_t>();
	quest.position.y = file->NextLE<int32_t>();
	quest._qslvl = static_cast<_setlevels>(file->NextLE<uint8_t>());
	quest._qidx = static_cast<quest_id>(file->NextLE<uint8_t>());
	if (gbIsHellfireSaveGame) {
		file->Skip(2); // Alignment
		quest._qmsg = static_cast<_speech_id>(file->NextLE<int32_t>());
	} else {
		quest._qmsg = static_cast<_speech_id>(file->NextLE<uint8_t>());
	}
	quest._qvar1 = file->NextLE<uint8_t>();
	quest._qvar2 = file->NextLE<uint8_t>();
	file->Skip(2); // Alignment
	if (!gbIsHellfireSaveGame)
		file->Skip(1); // Alignment
	quest._qlog = file->NextBool32();

	ReturnLvlPosition.x = file->NextBE<int32_t>();
	ReturnLvlPosition.y = file->NextBE<int32_t>();
	ReturnLevel = file->NextBE<int32_t>();
	ReturnLevelType = static_cast<dungeon_type>(file->NextBE<int32_t>());
	file->Skip(sizeof(int32_t)); // Skip DoomQuestState
}

void LoadLighting(LoadHelper *file, Light *pLight)
{
	pLight->position.tile.x = file->NextLE<int32_t>();
	pLight->position.tile.y = file->NextLE<int32_t>();
	pLight->radius = file->NextLE<int32_t>();
	file->Skip<int32_t>(); // _lid
	pLight->isInvalid = file->NextBool32();
	pLight->hasChanged = file->NextBool32();
	file->Skip(4); // Unused
	pLight->position.old.x = file->NextLE<int32_t>();
	pLight->position.old.y = file->NextLE<int32_t>();
	pLight->oldRadius = file->NextLE<int32_t>();
	pLight->position.offset.deltaX = file->NextLE<int32_t>();
	pLight->position.offset.deltaY = file->NextLE<int32_t>();
	file->Skip<uint32_t>(); // _lflags
}

void LoadPortal(LoadHelper *file, int i)
{
	Portal *pPortal = &Portals[i];

	pPortal->open = file->NextBool32();
	pPortal->position.x = file->NextLE<int32_t>();
	pPortal->position.y = file->NextLE<int32_t>();
	pPortal->level = file->NextLE<int32_t>();
	pPortal->ltype = static_cast<dungeon_type>(file->NextLE<int32_t>());
	pPortal->setlvl = file->NextBool32();
	if (!pPortal->setlvl)
		pPortal->ltype = GetLevelType(pPortal->level);
}

void GetLevelNames(string_view prefix, char *out)
{
	char suf;
	uint8_t num;
	if (setlevel) {
		suf = 's';
		num = static_cast<uint8_t>(setlvlnum);
	} else {
		suf = 'l';
		num = currlevel;
	}
	*fmt::format_to(out, "{}{}{:02d}", prefix, suf, num) = '\0';
}

void GetTempLevelNames(char *szTemp)
{
	return GetLevelNames("temp", szTemp);
}

void GetPermLevelNames(char *szPerm)
{
	return GetLevelNames("perm", szPerm);
}

bool LevelFileExists(SaveWriter &archive)
{
	char szName[MaxMpqPathSize];

	GetTempLevelNames(szName);
	if (archive.HasFile(szName))
		return true;

	GetPermLevelNames(szName);
	return archive.HasFile(szName);
}

/**
 * @brief Reads the fully-stored item records over the seed-replayed ones from the packed hero.
 *
 * ## Single-player takes the stored record, full stop
 *
 * It used to take it only if the packed item had ALREADY been rebuilt into something with a matching
 * seed - three `continue`s that each threw the stored record away and kept the reconstruction. That
 * is backwards, and it made the packed path a GATEKEEPER for the authoritative one: an item whose
 * index had become unavailable, or whose replay produced an empty slot, silently lost its complete
 * record and left the slot empty. The item was on disk the whole time.
 *
 * The evidence that the gate was vestigial is next door: LoadInventoryTabs reads the extra backpack
 * pages straight from storage with no seed check at all, so the same item loaded one way in tab 3
 * and another way in the main backpack.
 *
 * That gate is also the reason "the droppable pool is the save format" has shaped so much of this
 * fork - the pool walk is what a replay re-derives an index from, so touching the pool re-routed
 * every seeded item. In single-player it no longer decides anything: the record on disk does.
 * (User, 2026-08-27: "i dont care about preserving sdave. i care about robust coding.")
 *
 * MULTIPLAYER is untouched and still validates, because there the packed record is what arrived over
 * the wire and the local file is the thing that has to be checked against it. V1 is single-player
 * (oracool::MultiplayerEnabled), so that branch is dead code kept honest rather than a live path.
 */
void LoadMatchingItems(LoadHelper &file, const Player &player, const int n, Item *pItem)
{
	Item heroItem;

	for (int i = 0; i < n; i++) {
		Item &unpackedItem = pItem[i];
		// Read unconditionally, before any skip: the record is fixed-size and positional, so a
		// `continue` that skipped the READ would misalign every item after it.
		LoadItemData(file, heroItem);

		// An ear carries its owner's name in fields the item record has no room for, and UnPackItem
		// rebuilds it from those. It is the one item the packed copy knows more about than the
		// stored one, so the packed copy wins - in both modes.
		if (heroItem.IDidx == IDI_EAR)
			continue;

		if (!gbIsMultiplayer) {
			unpackedItem = heroItem;
			continue;
		}

		if (unpackedItem.isEmpty() || heroItem.isEmpty())
			continue;
		if (unpackedItem._iSeed != heroItem._iSeed)
			continue;
		{
			// Ensure that the unpacked item was regenerated using the appropriate
			// game's item generation logic before attempting to use it for validation
			if ((heroItem.dwBuff & CF_HELLFIRE) != (unpackedItem.dwBuff & CF_HELLFIRE)) {
				unpackedItem = {};
				RecreateItem(player, unpackedItem, heroItem.IDidx, heroItem._iCreateInfo, heroItem._iSeed, heroItem._ivalue, (heroItem.dwBuff & CF_HELLFIRE) != 0);
				unpackedItem._iIdentified = heroItem._iIdentified;
				unpackedItem._iMaxDur = heroItem._iMaxDur;
				unpackedItem._iDurability = ClampDurability(unpackedItem, heroItem._iDurability);
				unpackedItem._iMaxCharges = clamp<int>(heroItem._iMaxCharges, 0, unpackedItem._iMaxCharges);
				unpackedItem._iCharges = clamp<int>(heroItem._iCharges, 0, unpackedItem._iMaxCharges);
			}
			if (gbIsHellfire) {
				unpackedItem._iPLToHit = ClampToHit(unpackedItem, heroItem._iPLToHit); // Oil of Accuracy
				unpackedItem._iMaxDam = ClampMaxDam(unpackedItem, heroItem._iMaxDam);  // Oil of Sharpness
			}
		}
	}
}


/**
 * @brief Loads items on the current dungeon floor
 * @param file interface to the save file
 * @param savedItemCount how many items to read from the save file
 */
void LoadDroppedItems(LoadHelper &file, size_t savedItemCount)
{
	// Skip loading ActiveItems and AvailableItems, the indices are initialised below based on the number of valid items
	file.Skip<uint8_t>(MAXITEMS * 2);

	// Reset ActiveItems, the Items array will be populated from the start
	std::iota(ActiveItems, ActiveItems + MAXITEMS, 0);
	ActiveItemCount = 0;
	// Clear dItem so we can populate valid drop locations
	memset(dItem, 0, sizeof(dItem));

	for (size_t i = 0; i < savedItemCount; i++) {
		Item &item = Items[ActiveItemCount];
		LoadItem(file, item);

		if (!item.isEmpty()) {
			// Loaded a valid item
			ActiveItemCount++;
			// populate its location in the lookup table with the offset in the Items array + 1 (so 0 can be used for "no item")
			dItem[item.position.x][item.position.y] = ActiveItemCount;
		}
	}
}

int getHellfireLevelType(int type)
{
	if (type == DTYPE_CRYPT)
		return DTYPE_CATHEDRAL;

	if (type == DTYPE_NEST)
		return DTYPE_CAVES;

	return type;
}

void SaveItem(SaveHelper &file, const Item &item)
{
	auto idx = item.IDidx;
	if (!gbIsHellfire)
		idx = RemapItemIdxToDiablo(idx);
	if (gbIsSpawn)
		idx = RemapItemIdxToSpawn(idx);
	ItemType iType = item._itype;
	if (idx == -1) {
		idx = _item_indexes::IDI_GOLD;
		iType = ItemType::None;
	}

	file.WriteLE<uint32_t>(item._iSeed);
	file.WriteLE<int16_t>(item._iCreateInfo);
	file.Skip(2); // Alignment
	file.WriteLE<int32_t>(static_cast<int32_t>(iType));
	file.WriteLE<int32_t>(item.position.x);
	file.WriteLE<int32_t>(item.position.y);
	file.WriteLE<uint32_t>(item._iAnimFlag ? 1 : 0);
	file.Skip(4); // Skip pointer _iAnimData
	file.WriteLE<int32_t>(item.AnimInfo.numberOfFrames);
	file.WriteLE<int32_t>(item.AnimInfo.currentFrame + 1);
	// write _iAnimWidth for vanilla compatibility
	file.WriteLE<int32_t>(ItemAnimWidth);
	// write _iAnimWidth2 for vanilla compatibility
	file.WriteLE<int32_t>(CalculateWidth2(ItemAnimWidth));
	file.Skip<uint32_t>(); // _delFlag, unused since 1.02
	file.WriteLE<uint8_t>(item._iSelFlag);
	file.Skip(3); // Alignment
	file.WriteLE<uint32_t>(item._iPostDraw ? 1 : 0);
	file.WriteLE<uint32_t>(item._iIdentified ? 1 : 0);
	file.WriteLE<int8_t>(item._iMagical);
	file.WriteBytes(item._iName, 64);
	file.WriteBytes(item._iIName, 64);
	file.WriteLE<int8_t>(item._iLoc);
	file.WriteLE<uint8_t>(item._iClass);
	file.Skip(1); // Alignment
	file.WriteLE<int32_t>(item._iCurs);
	file.WriteLE<int32_t>(item._ivalue);
	file.WriteLE<int32_t>(item._iIvalue);
	file.WriteLE<int32_t>(item._iMinDam);
	file.WriteLE<int32_t>(item._iMaxDam);
	file.WriteLE<int32_t>(item._iAC);
	file.WriteLE<uint32_t>(static_cast<uint32_t>(item._iFlags));
	file.WriteLE<int32_t>(item._iMiscId);
	file.WriteLE<int32_t>(static_cast<int8_t>(item._iSpell));
	file.WriteLE<int32_t>(item._iCharges);
	file.WriteLE<int32_t>(item._iMaxCharges);
	file.WriteLE<int32_t>(item._iDurability);
	file.WriteLE<int32_t>(item._iMaxDur);
	file.WriteLE<int32_t>(item._iPLDam);
	file.WriteLE<int32_t>(item._iPLToHit);
	file.WriteLE<int32_t>(item._iPLAC);
	file.WriteLE<int32_t>(item._iPLStr);
	file.WriteLE<int32_t>(item._iPLMag);
	file.WriteLE<int32_t>(item._iPLDex);
	file.WriteLE<int32_t>(item._iPLVit);
	file.WriteLE<int32_t>(item._iPLFR);
	file.WriteLE<int32_t>(item._iPLLR);
	file.WriteLE<int32_t>(item._iPLMR);
	file.WriteLE<int32_t>(item._iPLMana);
	file.WriteLE<int32_t>(item._iPLHP);
	file.WriteLE<int32_t>(item._iPLDamMod);
	file.WriteLE<int32_t>(item._iPLGetHit);
	file.WriteLE<int32_t>(item._iPLLight);
	file.WriteLE<int8_t>(item._iSplLvlAdd);
	file.WriteLE<int8_t>(item._iRequest ? 1 : 0);
	file.Skip(2); // Alignment
	file.WriteLE<int32_t>(item._iUid);
	file.WriteLE<int32_t>(item._iFMinDam);
	file.WriteLE<int32_t>(item._iFMaxDam);
	file.WriteLE<int32_t>(item._iLMinDam);
	file.WriteLE<int32_t>(item._iLMaxDam);
	file.WriteLE<int32_t>(item._iPLEnAc);
	file.WriteLE<int8_t>(item._iPrePower);
	file.WriteLE<int8_t>(item._iSufPower);
	file.Skip(2); // Alignment
	file.WriteLE<int32_t>(item._iVAdd1);
	file.WriteLE<int32_t>(item._iVMult1);
	file.WriteLE<int32_t>(item._iVAdd2);
	file.WriteLE<int32_t>(item._iVMult2);
	file.WriteLE<int8_t>(item._iMinStr);
	file.WriteLE<uint8_t>(item._iMinMag);
	file.WriteLE<int8_t>(item._iMinDex);
	file.Skip(1); // Alignment
	file.WriteLE<uint32_t>(item._iStatFlag ? 1 : 0);
	file.WriteLE<int32_t>(idx);
	file.WriteLE<uint32_t>(item.dwBuff);
	if (gbIsHellfire)
		file.WriteLE<uint32_t>(static_cast<uint32_t>(item._iDamAcFlags));

	// Oracool item tier/affix data (v0.2.0+): folded directly into the primary item record
	// instead of a separate "heroitemsext"-style sidecar file, so every container that
	// already calls SaveItem - backpack, belt, equipped slots, stash, dropped ground items
	// on every level, and Tabbed Inventory's extra tabs - carries tier data automatically
	// with no per-container wiring to remember. Every slot is written unconditionally
	// (regardless of the item's actual prefix/suffix count) since this is a fixed-size
	// positional record; a variable-length record here would misalign every subsequent item.
	file.WriteLE<uint8_t>(static_cast<uint8_t>(item._iOracoolTier));
	file.WriteLE<uint8_t>(item._iOracoolPerfectRoll ? 1 : 0);
	file.WriteLE<uint8_t>(item._iOracoolBroken ? 1 : 0);
	file.WriteLE<uint8_t>(item._iOracoolPrefixCount);
	file.WriteLE<uint8_t>(item._iOracoolSuffixCount);
	for (const OracoolAffix &affix : item._iOracoolPrefixes) {
		file.WriteLE<int8_t>(static_cast<int8_t>(affix.type));
		file.WriteLE<int32_t>(affix.param1);
		file.WriteLE<int32_t>(affix.param2);
	}
	for (const OracoolAffix &affix : item._iOracoolSuffixes) {
		file.WriteLE<int8_t>(static_cast<int8_t>(affix.type));
		file.WriteLE<int32_t>(affix.param1);
		file.WriteLE<int32_t>(affix.param2);
	}

	// Megaplan Phase 1 sockets - see the matching read in LoadItemData.
	file.WriteLE<uint8_t>(item._iSocketCount);
	for (const uint16_t socket : item._iSocketed)
		file.WriteLE<uint16_t>(socket);
	// Version 4: the ethereal flag.
	file.WriteLE<uint8_t>(item._iOracoolEthereal ? 1 : 0);
	// Version 5: ilvl - see the matching read.
	file.WriteLE<uint8_t>(item._iOracoolItemLevel);
	// Version 6: the base tier.
	file.WriteLE<uint8_t>(item._iOracoolBaseTier);
	// Version 8: the gold-find bonus.
	file.WriteLE<int32_t>(item._iPLGoldFind);
	// Version 9: the magic-find twin, and the Mystic Orb count.
	file.WriteLE<int32_t>(item._iPLMagicFind);
	file.WriteLE<uint8_t>(item._iOracoolOrbCount);
}

void SavePlayer(SaveHelper &file, const Player &player)
{
	file.WriteLE<int32_t>(player._pmode);
	for (int8_t step : player.walkpath)
		file.WriteLE<int8_t>(step);
	file.WriteLE<uint8_t>(player.plractive ? 1 : 0);
	file.Skip(2); // Alignment
	file.WriteLE<int32_t>(player.destAction);
	file.WriteLE<int32_t>(player.destParam1);
	file.WriteLE<int32_t>(player.destParam2);
	file.WriteLE<int32_t>(static_cast<int32_t>(player.destParam3));
	file.WriteLE<int32_t>(player.destParam4);
	file.WriteLE<uint32_t>(player.plrlevel);
	file.WriteLE<int32_t>(player.position.tile.x);
	file.WriteLE<int32_t>(player.position.tile.y);
	file.WriteLE<int32_t>(player.position.future.x);
	file.WriteLE<int32_t>(player.position.future.y);

	// For backwards compatibility
	const Point target = player.GetTargetPosition();
	file.WriteLE<int32_t>(target.x);
	file.WriteLE<int32_t>(target.y);

	file.WriteLE<int32_t>(player.position.last.x);
	file.WriteLE<int32_t>(player.position.last.y);
	file.WriteLE<int32_t>(player.position.old.x);
	file.WriteLE<int32_t>(player.position.old.y);
	DisplacementOf<int16_t> offset = {};
	DisplacementOf<int16_t> offset2 = {};
	DisplacementOf<int16_t> velocity = {};
	if (player.isWalking()) {
		offset = player.position.CalculateWalkingOffset(player._pdir, player.AnimInfo);
		offset2 = player.position.CalculateWalkingOffsetShifted8(player._pdir, player.AnimInfo);
		velocity = player.position.GetWalkingVelocityShifted8(player._pdir, player.AnimInfo);
	}
	file.WriteLE<int32_t>(offset.deltaX);
	file.WriteLE<int32_t>(offset.deltaY);
	file.WriteLE<int32_t>(velocity.deltaX);
	file.WriteLE<int32_t>(velocity.deltaY);
	file.WriteLE<int32_t>(static_cast<int32_t>(player._pdir));
	file.Skip(4); // Unused
	file.WriteLE<uint32_t>(player._pgfxnum);
	file.Skip(4); // Skip pointer _pAnimData
	file.WriteLE<int32_t>(std::max(0, player.AnimInfo.ticksPerFrame - 1));
	file.WriteLE<int32_t>(player.AnimInfo.tickCounterOfCurrentFrame);
	file.WriteLE<int32_t>(player.AnimInfo.numberOfFrames);
	file.WriteLE<int32_t>(player.AnimInfo.currentFrame + 1);
	// write _pAnimWidth for vanilla compatibility
	const int animWidth = player.getSpriteWidth();
	file.WriteLE<int32_t>(animWidth);
	// write _pAnimWidth2 for vanilla compatibility
	file.WriteLE<int32_t>(CalculateWidth2(animWidth));
	file.Skip<uint32_t>(); // Skip _peflag
	file.WriteLE<int32_t>(player.lightId);
	file.WriteLE<int32_t>(1); // _pvid

	file.WriteLE<int32_t>(static_cast<int8_t>(player.queuedSpell.spellId));
	file.WriteLE<int8_t>(static_cast<int8_t>(player.queuedSpell.spellType));
	file.WriteLE<int8_t>(player.queuedSpell.spellFrom);
	file.Skip(2); // Alignment
	file.WriteLE<int32_t>(static_cast<int8_t>(player.inventorySpell));
	file.WriteLE<uint8_t>(oracool::PackReadiedSpell(player._pLRSpell)); // was _pTSplType, see LoadPlayer
	file.Skip(3);                                                       // Alignment
	file.WriteLE<int32_t>(static_cast<int8_t>(player._pRSpell));
	file.WriteLE<int8_t>(static_cast<uint8_t>(player._pRSplType));
	file.Skip(3); // Alignment
	file.WriteLE<int32_t>(static_cast<int8_t>(player._pSBkSpell));
	file.Skip<int8_t>(); // Skip _pSBkSplType

	for (uint8_t spellLevel : player._pSplLvl)
		file.WriteLE<uint8_t>(spellLevel);

	file.Skip(7); // Alignment
	// The LOW word only, on all four - the layout is vanilla's and it stays. The high word is never
	// persisted and does not need to be: book spells all sit below 64, and innate, item and scroll
	// spells are rebuilt from investment and inventory on load. See SpellMask.
	file.WriteLE<uint64_t>(player._pMemSpells.low);
	file.WriteLE<uint64_t>(player._pAblSpells.low);
	file.WriteLE<uint64_t>(player._pScrlSpells.low);
	file.WriteLE<uint8_t>(static_cast<uint8_t>(player._pSpellFlags));
	file.Skip(3); // Alignment

	// Extra hotkeys: to keep single player save compatibility, write only 4 hotkeys here, rely on SaveHotkeys for the rest
	for (size_t i = 0; i < 4; i++) {
		file.WriteLE<int32_t>(static_cast<int8_t>(player._pSplHotKey[i]));
	}
	for (size_t i = 0; i < 4; i++) {
		file.WriteLE<uint8_t>(static_cast<uint8_t>(player._pSplTHotKey[i]));
	}

	file.WriteLE<int32_t>(player.UsesRangedWeapon() ? 1 : 0);
	file.WriteLE<uint8_t>(player._pBlockFlag ? 1 : 0);
	file.WriteLE<uint8_t>(player._pInvincible ? 1 : 0);
	file.WriteLE<int8_t>(player._pLightRad);
	file.WriteLE<uint8_t>(player._pLvlChanging ? 1 : 0);

	file.WriteBytes(player._pName, PlayerNameLength);
	file.WriteLE<int8_t>(static_cast<int8_t>(player._pClass));
	file.Skip(3); // Alignment
	file.WriteLE<int32_t>(player._pStrength);
	file.WriteLE<int32_t>(player._pBaseStr);
	file.WriteLE<int32_t>(player._pMagic);
	file.WriteLE<int32_t>(player._pBaseMag);
	file.WriteLE<int32_t>(player._pDexterity);
	file.WriteLE<int32_t>(player._pBaseDex);
	file.WriteLE<int32_t>(player._pVitality);
	file.WriteLE<int32_t>(player._pBaseVit);
	file.WriteLE<int32_t>(player._pStatPts);
	file.WriteLE<int32_t>(player._pDamageMod);

	file.WriteLE<int32_t>(player._pBaseToBlk);
	file.WriteLE<int32_t>(player._pHPBase);
	file.WriteLE<int32_t>(player._pMaxHPBase);
	file.WriteLE<int32_t>(player._pHitPoints);
	file.WriteLE<int32_t>(player._pMaxHP);
	file.Skip<int32_t>(); // Skip _pHPPer
	file.WriteLE<int32_t>(player._pManaBase);
	file.WriteLE<int32_t>(player._pMaxManaBase);
	file.WriteLE<int32_t>(player._pMana);
	file.WriteLE<int32_t>(player._pMaxMana);
	file.Skip<int32_t>(); // Skip _pManaPer
	file.WriteLE<int8_t>(player._pLevel);
	file.WriteLE<int8_t>(player._pMaxLvl);
	file.Skip(2); // Alignment
	file.WriteLE<uint64_t>(player._pExperience);
	file.Skip<uint32_t>(); // Skip _pMaxExp
	file.WriteLE<uint64_t>(player._pNextExper);
	file.WriteLE<int8_t>(player._pArmorClass);
	file.WriteLE<int8_t>(player._pMagResist);
	file.WriteLE<int8_t>(player._pFireResist);
	file.WriteLE<int8_t>(player._pLghtResist);
	file.WriteLE<int32_t>(player._pGold);
	file.WriteLE<uint32_t>(player._pInfraFlag ? 1 : 0);

	int32_t tempPositionX = player.position.temp.x;
	int32_t tempPositionY = player.position.temp.y;
	if (player._pmode == PM_WALK_NORTHWARDS) {
		// For backwards compatibility, save this as an offset
		tempPositionX -= player.position.tile.x;
		tempPositionY -= player.position.tile.y;
	}
	file.WriteLE<int32_t>(tempPositionX);
	file.WriteLE<int32_t>(tempPositionY);

	file.WriteLE<int32_t>(static_cast<int32_t>(player.tempDirection));
	file.WriteLE<int32_t>(player.queuedSpell.spellLevel);
	file.Skip<int32_t>(); // skip _pVar5, was used for storing position of a tile which should have its HorizontalMovingPlayer flag removed after walking
	file.WriteLE<int32_t>(offset2.deltaX);
	file.WriteLE<int32_t>(offset2.deltaY);
	file.Skip<int32_t>(); // Skip _pVar8
	for (uint8_t i = 0; i < giNumberOfLevels; i++)
		file.WriteLE<uint8_t>(player._pLvlVisited[i] ? 1 : 0);
	for (uint8_t i = 0; i < giNumberOfLevels; i++)
		file.WriteLE<uint8_t>(player._pSLvlVisited[i] ? 1 : 0); // only 10 used

	file.Skip(2); // Alignment

	file.Skip<int32_t>();   // Skip _pGFXLoad
	file.Skip<uint32_t>(8); // Skip pointers _pNAnim
	file.WriteLE<int32_t>(player._pNFrames);
	file.Skip<uint32_t>();  // Skip _pNWidth
	file.Skip<uint32_t>(8); // Skip pointers _pWAnim
	file.WriteLE<int32_t>(player._pWFrames);
	file.Skip<uint32_t>();  // Skip _pWWidth
	file.Skip<uint32_t>(8); // Skip pointers _pAAnim
	file.WriteLE<int32_t>(player._pAFrames);
	file.Skip<uint32_t>(); // Skip _pAWidth
	file.WriteLE<int32_t>(player._pAFNum);
	file.Skip<uint32_t>(8); // Skip pointers _pLAnim
	file.Skip<uint32_t>(8); // Skip pointers _pFAnim
	file.Skip<uint32_t>(8); // Skip pointers _pTAnim
	file.WriteLE<int32_t>(player._pSFrames);
	file.Skip<uint32_t>(); // Skip _pSWidth
	file.WriteLE<int32_t>(player._pSFNum);
	file.Skip<uint32_t>(8); // Skip pointers _pHAnim
	file.WriteLE<int32_t>(player._pHFrames);
	file.Skip<uint32_t>();  // Skip _pHWidth
	file.Skip<uint32_t>(8); // Skip pointers _pDAnim
	file.WriteLE<int32_t>(player._pDFrames);
	file.Skip<uint32_t>();  // Skip _pDWidth
	file.Skip<uint32_t>(8); // Skip pointers _pBAnim
	file.WriteLE<int32_t>(player._pBFrames);
	file.Skip<uint32_t>(); // Skip _pBWidth

	for (const Item &item : player.InvBody)
		SaveItem(file, item);

	for (const Item &item : player.InvList)
		SaveItem(file, item);

	file.WriteLE<int32_t>(player._pNumInv);

	for (int8_t cell : player.InvGrid)
		file.WriteLE<int8_t>(cell);

	for (const Item &item : player.SpdList)
		SaveItem(file, item);

	SaveItem(file, player.HoldItem);

	file.WriteLE<int32_t>(player._pIMinDam);
	file.WriteLE<int32_t>(player._pIMaxDam);
	file.WriteLE<int32_t>(player._pIAC);
	file.WriteLE<int32_t>(player._pIBonusDam);
	file.WriteLE<int32_t>(player._pIBonusToHit);
	file.WriteLE<int32_t>(player._pIBonusAC);
	file.WriteLE<int32_t>(player._pIBonusDamMod);
	file.Skip(4); // Alignment

	file.WriteLE<uint64_t>(player._pISpells.low);
	file.WriteLE<int32_t>(static_cast<int32_t>(player._pIFlags));
	file.WriteLE<int32_t>(player._pIGetHit);

	file.WriteLE<int8_t>(player._pISplLvlAdd);
	file.Skip<uint8_t>(); // Skip _pISplCost
	file.Skip(2);         // Alignment
	file.Skip<int32_t>(); // _pISplDur
	file.WriteLE<int32_t>(player._pIEnAc);
	file.WriteLE<int32_t>(player._pIFMinDam);
	file.WriteLE<int32_t>(player._pIFMaxDam);
	file.WriteLE<int32_t>(player._pILMinDam);
	file.WriteLE<int32_t>(player._pILMaxDam);
	file.WriteLE<int32_t>(player._pOilType);
	file.WriteLE<uint8_t>(player.pTownWarps);
	file.WriteLE<uint8_t>(player.pDungMsgs);
	file.WriteLE<uint8_t>(player.pLvlLoad);
	if (gbIsHellfire)
		file.WriteLE<uint8_t>(player.pDungMsgs2);
	else
		file.WriteLE<uint8_t>(0);
	file.WriteLE<uint8_t>(player.pManaShield ? 1 : 0);
	file.WriteLE<uint8_t>(player.pOriginalCathedral ? 1 : 0);
	file.Skip(2); // Available bytes
	file.WriteLE<uint16_t>(player.wReflections);
	file.Skip(14); // Available bytes

	file.WriteLE<uint32_t>(player.pDiabloKillLevel);
	file.WriteLE<uint32_t>(sgGameInitInfo.nDifficulty);
	file.WriteLE<uint32_t>(static_cast<uint32_t>(player.pDamAcFlags));
	// Oracool Reset Stats: see the matching LoadPlayer comment above.
	file.WriteLE<int32_t>(player._pStatPtsSpentStr);
	file.WriteLE<int32_t>(player._pStatPtsSpentMag);
	file.WriteLE<int32_t>(player._pStatPtsSpentDex);
	file.WriteLE<int32_t>(player._pStatPtsSpentVit);
	file.Skip(4); // Available bytes

	// Oracool: see the matching LoadPlayer comment above.
	for (const auto &difficultyRow : player._pWaypointUnlocked) {
		for (bool unlocked : difficultyRow)
			file.WriteLE<uint8_t>(unlocked ? 1 : 0);
	}

	// Omit pointer _pNData
	// Omit pointer _pWData
	// Omit pointer _pAData
	// Omit pointer _pLData
	// Omit pointer _pFData
	// Omit pointer  _pTData
	// Omit pointer _pHData
	// Omit pointer _pDData
	// Omit pointer _pBData
	// Omit pointer pReserved
}

void SaveMonster(SaveHelper *file, Monster &monster, MonsterConversionData *monsterConversionData = nullptr)
{
	file->WriteLE<int32_t>(monster.levelType);
	file->WriteLE<int32_t>(static_cast<int>(monster.mode));
	file->WriteLE<uint8_t>(static_cast<uint8_t>(monster.goal));
	file->Skip(3); // Alignment
	file->WriteLE<int32_t>(monster.goalVar1);
	file->WriteLE<int32_t>(monster.goalVar2);
	file->WriteLE<int32_t>(monster.goalVar3);
	file->WriteLE<uint8_t>(static_cast<uint8_t>(monster.lesserAffix)); // was the first Unused byte
	file->WriteLE<uint16_t>(monster.lesserNameSeed);                   // was the second and third
	file->Skip(1);                                                     // Unused
	file->WriteLE<uint8_t>(monster.pathCount);
	file->Skip(3); // Alignment
	file->WriteLE<int32_t>(monster.position.tile.x);
	file->WriteLE<int32_t>(monster.position.tile.y);
	file->WriteLE<int32_t>(monster.position.future.x);
	file->WriteLE<int32_t>(monster.position.future.y);
	file->WriteLE<int32_t>(monster.position.old.x);
	file->WriteLE<int32_t>(monster.position.old.y);
	DisplacementOf<int16_t> offset = {};
	DisplacementOf<int16_t> offset2 = {};
	DisplacementOf<int16_t> velocity = {};
	if (monster.isWalking()) {
		offset = monster.position.CalculateWalkingOffset(monster.direction, monster.animInfo);
		offset2 = monster.position.CalculateWalkingOffsetShifted4(monster.direction, monster.animInfo);
		velocity = monster.position.GetWalkingVelocityShifted4(monster.direction, monster.animInfo);
	}
	file->WriteLE<int32_t>(offset.deltaX);
	file->WriteLE<int32_t>(offset.deltaY);
	file->WriteLE<int32_t>(velocity.deltaX);
	file->WriteLE<int32_t>(velocity.deltaY);
	file->WriteLE<int32_t>(static_cast<int32_t>(monster.direction));
	file->WriteLE<int32_t>(monster.enemy);
	file->WriteLE<uint8_t>(monster.enemyPosition.x);
	file->WriteLE<uint8_t>(monster.enemyPosition.y);
	file->Skip(2); // Unused

	file->Skip(4); // Skip pointer _mAnimData
	file->WriteLE<int32_t>(monster.animInfo.ticksPerFrame);
	file->WriteLE<int32_t>(monster.animInfo.tickCounterOfCurrentFrame);
	file->WriteLE<int32_t>(monster.animInfo.numberOfFrames);
	file->WriteLE<int32_t>(monster.animInfo.currentFrame + 1);
	file->Skip<uint32_t>(); // Skip _meflag
	file->WriteLE<uint32_t>(monster.isInvalid ? 1 : 0);
	file->WriteLE<int32_t>(monster.var1);
	file->WriteLE<int32_t>(monster.var2);
	file->WriteLE<int32_t>(monster.var3);
	file->WriteLE<int32_t>(monster.position.temp.x);
	file->WriteLE<int32_t>(monster.position.temp.y);
	file->WriteLE<int32_t>(offset2.deltaX);
	file->WriteLE<int32_t>(offset2.deltaY);
	file->Skip<int32_t>(); // Skip _mVar8
	file->WriteLE<int32_t>(monster.maxHitPoints);
	file->WriteLE<int32_t>(monster.hitPoints);

	file->WriteLE<uint8_t>(static_cast<int8_t>(monster.ai));
	file->WriteLE<uint8_t>(monster.intelligence);
	file->Skip(2); // Alignment
	file->WriteLE<uint32_t>(monster.flags);
	file->WriteLE<uint8_t>(monster.activeForTicks);
	file->Skip(3); // Alignment
	file->Skip(4); // Unused
	file->WriteLE<int32_t>(monster.position.last.x);
	file->WriteLE<int32_t>(monster.position.last.y);
	file->WriteLE<uint32_t>(monster.rndItemSeed);
	file->WriteLE<uint32_t>(monster.aiSeed);
	file->Skip(4); // Unused

	file->WriteLE<uint8_t>(static_cast<uint8_t>(monster.uniqueType) + 1);
	file->WriteLE<uint8_t>(monster.uniqTrans);
	file->WriteLE<int8_t>(monster.corpseId);

	file->WriteLE<int8_t>(monster.whoHit);
	if (monsterConversionData != nullptr)
		file->WriteLE<int8_t>(monsterConversionData->monsterLevel);
	else
		file->WriteLE<int8_t>(static_cast<int8_t>(monster.level(sgGameInitInfo.nDifficulty)));
	file->Skip(1); // Alignment
	if (monsterConversionData != nullptr)
		file->WriteLE<uint16_t>(monsterConversionData->experience);
	else
		file->WriteLE<uint16_t>(static_cast<uint16_t>(std::min<unsigned>(std::numeric_limits<uint16_t>::max(), monster.exp(sgGameInitInfo.nDifficulty))));

	if (monsterConversionData != nullptr)
		file->WriteLE<uint8_t>(monsterConversionData->toHit);
	else
		file->WriteLE<uint8_t>(static_cast<uint8_t>(std::min<uint16_t>(monster.toHit(sgGameInitInfo.nDifficulty), std::numeric_limits<uint8_t>::max()))); // For backwards compatibility
	file->WriteLE<uint8_t>(monster.minDamage);
	file->WriteLE<uint8_t>(monster.maxDamage);
	if (monsterConversionData != nullptr)
		file->WriteLE<uint8_t>(monsterConversionData->toHitSpecial);
	else
		file->WriteLE<uint8_t>(static_cast<uint8_t>(std::min<uint16_t>(monster.toHitSpecial(sgGameInitInfo.nDifficulty), std::numeric_limits<uint8_t>::max()))); // For backwards compatibility
	file->WriteLE<uint8_t>(monster.minDamageSpecial);
	file->WriteLE<uint8_t>(monster.maxDamageSpecial);
	file->WriteLE<uint8_t>(monster.armorClass);
	file->Skip(1); // Alignment
	file->WriteLE<uint16_t>(monster.resistance);
	file->Skip(2); // Alignment

	file->WriteLE<int32_t>(monster.talkMsg == TEXT_NONE ? 0 : monster.talkMsg);       // Replicate original bad mapping of none for monsters
	file->WriteLE<uint8_t>(monster.leader == Monster::NoLeader ? 0 : monster.leader); // Vanilla uses 0 as the default leader which corresponds to player 0s golem
	file->WriteLE<uint8_t>(static_cast<std::uint8_t>(monster.leaderRelation));
	file->WriteLE<uint8_t>(monster.packSize);
	// vanilla compatibility
	if (monster.lightId == NO_LIGHT)
		file->WriteLE<int8_t>(0);
	else
		file->WriteLE<int8_t>(monster.lightId);

	// Omit pointer name;
}

void SaveMissile(SaveHelper *file, const Missile &missile)
{
	file->WriteLE<int32_t>(static_cast<int16_t>(missile._mitype));
	file->WriteLE<int32_t>(missile.position.tile.x);
	file->WriteLE<int32_t>(missile.position.tile.y);
	file->WriteLE<int32_t>(missile.position.offset.deltaX);
	file->WriteLE<int32_t>(missile.position.offset.deltaY);
	file->WriteLE<int32_t>(missile.position.velocity.deltaX);
	file->WriteLE<int32_t>(missile.position.velocity.deltaY);
	file->WriteLE<int32_t>(missile.position.start.x);
	file->WriteLE<int32_t>(missile.position.start.y);
	file->WriteLE<int32_t>(missile.position.traveled.deltaX);
	file->WriteLE<int32_t>(missile.position.traveled.deltaY);
	file->WriteLE<int32_t>(missile._mimfnum);
	file->WriteLE<int32_t>(missile._mispllvl);
	file->WriteLE<uint32_t>(missile._miDelFlag ? 1 : 0);
	file->WriteLE<uint8_t>(static_cast<uint8_t>(missile._miAnimType));
	file->Skip(3); // Alignment
	file->WriteLE<int32_t>(static_cast<int32_t>(missile._miAnimFlags));
	file->Skip(4); // Skip pointer _miAnimData
	file->WriteLE<int32_t>(missile._miAnimDelay);
	file->WriteLE<int32_t>(missile._miAnimLen);
	file->WriteLE<int32_t>(missile._miAnimWidth);
	file->WriteLE<int32_t>(missile._miAnimWidth2);
	file->WriteLE<int32_t>(missile._miAnimCnt);
	file->WriteLE<int32_t>(missile._miAnimAdd);
	file->WriteLE<int32_t>(missile._miAnimFrame);
	file->WriteLE<uint32_t>(missile._miDrawFlag ? 1 : 0);
	file->WriteLE<uint32_t>(missile._miLightFlag ? 1 : 0);
	file->WriteLE<uint32_t>(missile._miPreFlag ? 1 : 0);
	file->WriteLE<uint32_t>(missile._miUniqTrans);
	file->WriteLE<int32_t>(missile._mirange);
	file->WriteLE<int32_t>(missile._misource);
	file->WriteLE<int32_t>(missile._micaster);
	file->WriteLE<int32_t>(missile._midam);
	file->WriteLE<uint32_t>(missile._miHitFlag ? 1 : 0);
	file->WriteLE<int32_t>(missile._midist);
	file->WriteLE<int32_t>(missile._mlid);
	file->WriteLE<int32_t>(missile._mirnd);
	file->WriteLE<int32_t>(missile.var1);
	file->WriteLE<int32_t>(missile.var2);
	file->WriteLE<int32_t>(missile.var3);
	file->WriteLE<int32_t>(missile.var4);
	file->WriteLE<int32_t>(missile.var5);
	file->WriteLE<int32_t>(missile.var6);
	file->WriteLE<int32_t>(missile.var7);
	file->WriteLE<uint32_t>(missile.limitReached ? 1 : 0);
}

_object_id ConvertToHellfireObject(_object_id type)
{
	if (leveltype == DTYPE_NEST) {
		switch (type) {
		case OBJ_POD:
			return OBJ_BARREL;
		case OBJ_PODEX:
			return OBJ_BARRELEX;
		default:
			break;
		}
	}

	if (leveltype == DTYPE_CRYPT) {
		switch (type) {
		case OBJ_URN:
			return OBJ_BARREL;
		case OBJ_URNEX:
			return OBJ_BARRELEX;
		case OBJ_L5BOOKS:
			return OBJ_STORYBOOK;
		case OBJ_L5CANDLE:
			return OBJ_STORYCANDLE;
		case OBJ_L5LDOOR:
			return OBJ_L1LDOOR;
		case OBJ_L5RDOOR:
			return OBJ_L1RDOOR;
		case OBJ_L5LEVER:
			return OBJ_LEVER;
		case OBJ_L5SARC:
			return OBJ_SARC;
		default:
			break;
		}
	}

	return type;
}

void SaveObject(SaveHelper &file, const Object &object)
{
	file.WriteLE<int32_t>(ConvertToHellfireObject(object._otype));
	file.WriteLE<int32_t>(object.position.x);
	file.WriteLE<int32_t>(object.position.y);
	file.WriteLE<uint32_t>(object.applyLighting ? 1 : 0);
	file.WriteLE<uint32_t>(object._oAnimFlag ? 1 : 0);
	file.Skip(4); // Skip pointer _oAnimData
	file.WriteLE<int32_t>(object._oAnimDelay);
	file.WriteLE<int32_t>(object._oAnimCnt);
	file.WriteLE<uint32_t>(object._oAnimLen);
	file.WriteLE<uint32_t>(object._oAnimFrame);
	file.WriteLE<int32_t>(object._oAnimWidth);
	file.WriteLE<int32_t>(CalculateWidth2(static_cast<int>(object._oAnimWidth))); // Write _oAnimWidth2 for vanilla compatibility
	file.WriteLE<uint32_t>(object._oDelFlag ? 1 : 0);
	file.WriteLE<int8_t>(object._oBreak);
	file.Skip(3); // Alignment
	file.WriteLE<uint32_t>(object._oSolidFlag ? 1 : 0);
	file.WriteLE<uint32_t>(object._oMissFlag ? 1 : 0);

	file.WriteLE<int8_t>(object._oSelFlag);
	file.Skip(3); // Alignment
	file.WriteLE<uint32_t>(object._oPreFlag ? 1 : 0);
	file.WriteLE<uint32_t>(object._oTrapFlag ? 1 : 0);
	file.WriteLE<uint32_t>(object._oDoorFlag ? 1 : 0);
	file.WriteLE<int32_t>(object._olid);
	file.WriteLE<uint32_t>(object._oRndSeed);

	/* Make dynamic light sources unseen when saving level data for level change */
	int32_t var1 = object._oVar1;
	switch (object._otype) {
	case OBJ_L1LIGHT:
	case OBJ_SKFIRE:
	case OBJ_CANDLE1:
	case OBJ_CANDLE2:
	case OBJ_BOOKCANDLE:
	case OBJ_STORYCANDLE:
	case OBJ_L5CANDLE:
	case OBJ_TORCHL:
	case OBJ_TORCHR:
	case OBJ_TORCHL2:
	case OBJ_TORCHR2:
	case OBJ_BCROSS:
	case OBJ_TBCROSS:
		if (var1 != -1)
			var1 = 0;
		break;
	default:
		break;
	}
	file.WriteLE<int32_t>(var1);
	file.WriteLE<int32_t>(object._oVar2);
	file.WriteLE<int32_t>(object._oVar3);
	file.WriteLE<int32_t>(object._oVar4);
	file.WriteLE<int32_t>(object._oVar5);
	file.WriteLE<uint32_t>(object._oVar6);
	file.WriteLE<int32_t>(object.bookMessage);
	file.WriteLE<int32_t>(object._oVar8);
}

void SaveQuest(SaveHelper *file, int i)
{
	auto &quest = Quests[i];

	file->WriteLE<uint8_t>(quest._qlevel);
	file->WriteLE<uint8_t>(quest._qidx); // _qtype for compatability, used in DRLG_CheckQuests
	file->WriteLE<uint8_t>(quest._qactive);
	file->WriteLE<uint8_t>(quest._qlvltype);
	file->WriteLE<int32_t>(quest.position.x);
	file->WriteLE<int32_t>(quest.position.y);
	file->WriteLE<uint8_t>(quest._qslvl);
	file->WriteLE<uint8_t>(quest._qidx);
	if (gbIsHellfire) {
		file->Skip(2); // Alignment
		file->WriteLE<int32_t>(quest._qmsg);
	} else {
		file->WriteLE<uint8_t>(quest._qmsg);
	}
	file->WriteLE<uint8_t>(quest._qvar1);
	file->WriteLE<uint8_t>(quest._qvar2);
	file->Skip(2); // Alignment
	if (!gbIsHellfire)
		file->Skip(1); // Alignment
	file->WriteLE<uint32_t>(quest._qlog ? 1 : 0);

	file->WriteBE<int32_t>(ReturnLvlPosition.x);
	file->WriteBE<int32_t>(ReturnLvlPosition.y);
	file->WriteBE<int32_t>(ReturnLevel);
	file->WriteBE<int32_t>(ReturnLevelType);
	file->Skip(sizeof(int32_t)); // Skip DoomQuestState
}

void SaveLighting(SaveHelper *file, Light *pLight, bool vision = false)
{
	file->WriteLE<int32_t>(pLight->position.tile.x);
	file->WriteLE<int32_t>(pLight->position.tile.y);
	file->WriteLE<int32_t>(pLight->radius);
	file->WriteLE<int32_t>(vision ? 1 : 0); // _lid
	file->WriteLE<uint32_t>(pLight->isInvalid ? 1 : 0);
	file->WriteLE<uint32_t>(pLight->hasChanged ? 1 : 0);
	file->Skip(4); // Unused
	file->WriteLE<int32_t>(pLight->position.old.x);
	file->WriteLE<int32_t>(pLight->position.old.y);
	file->WriteLE<int32_t>(pLight->oldRadius);
	file->WriteLE<int32_t>(pLight->position.offset.deltaX);
	file->WriteLE<int32_t>(pLight->position.offset.deltaY);
	file->WriteLE<uint32_t>(vision ? 1 : 0);
}

void SavePortal(SaveHelper *file, int i)
{
	Portal *pPortal = &Portals[i];

	file->WriteLE<uint32_t>(pPortal->open ? 1 : 0);
	file->WriteLE<int32_t>(pPortal->position.x);
	file->WriteLE<int32_t>(pPortal->position.y);
	file->WriteLE<int32_t>(pPortal->level);
	file->WriteLE<int32_t>(pPortal->setlvl ? pPortal->ltype : getHellfireLevelType(pPortal->ltype));
	file->WriteLE<uint32_t>(pPortal->setlvl ? 1 : 0);
}

/**
 * @brief Saves items on the current dungeon floor
 * @param file interface to the save file
 * @return a map converting from runtime item indexes to the relative position in the save file, used by SaveDroppedItemLocations
 * @see SaveDroppedItemLocations
 */
std::unordered_map<uint8_t, uint8_t> SaveDroppedItems(SaveHelper &file)
{
	// Vanilla Diablo/Hellfire initialise the ActiveItems and AvailableItems arrays based on saved data, so write valid values for compatibility
	for (uint8_t i = 0; i < MAXITEMS; i++)
		file.WriteLE<uint8_t>(i); // Strictly speaking everything from ActiveItemCount onwards is unused but no harm writing non-zero values here.
	for (uint8_t i = 0; i < MAXITEMS; i++)
		file.WriteLE<uint8_t>((i + ActiveItemCount) % MAXITEMS);

	std::unordered_map<uint8_t, uint8_t> itemIndexes = { { 0, 0 } };
	for (uint8_t i = 0; i < ActiveItemCount; i++) {
		itemIndexes[ActiveItems[i] + 1] = i + 1;
		SaveItem(file, Items[ActiveItems[i]]);
	}
	return itemIndexes;
}

/**
 * @brief Saves the position of dropped items (in dItem)
 * @param file interface to the save file
 * @param itemIndexes a map converting from runtime item indexes to the relative position in the save file
 */
void SaveDroppedItemLocations(SaveHelper &file, const std::unordered_map<uint8_t, uint8_t> &itemIndexes)
{
	for (int j = 0; j < MAXDUNY; j++) {
		for (int i = 0; i < MAXDUNX; i++) // NOLINT(modernize-loop-convert)
			file.WriteLE<uint8_t>(itemIndexes.at(dItem[i][j]));
	}
}

constexpr uint32_t VersionAdditionalMissiles = 0;

void SaveAdditionalMissiles(SaveWriter &saveWriter)
{
	constexpr size_t BytesWrittenBySaveMissile = 180;
	uint32_t missileCountAdditional = (Missiles.size() > MaxMissilesForSaveGame) ? static_cast<uint32_t>(Missiles.size() - MaxMissilesForSaveGame) : 0;
	SaveHelper file(saveWriter, "additionalMissiles", sizeof(uint32_t) + sizeof(uint32_t) + (missileCountAdditional * BytesWrittenBySaveMissile));

	file.WriteLE<uint32_t>(VersionAdditionalMissiles);
	file.WriteLE<uint32_t>(missileCountAdditional);

	if (missileCountAdditional > 0) {
		auto it = Missiles.cbegin();
		// std::list::const_iterator doesn't provide operator+() :/ using std::advance to get past the missiles we've already saved
		std::advance(it, MaxMissilesForSaveGame);
		for (; it != Missiles.cend(); it++) {
			SaveMissile(&file, *it);
		}
	}
}

void LoadAdditionalMissiles()
{
	LoadHelper file(OpenSaveArchive(gSaveNumber), "additionalMissiles");

	if (!file.IsValid()) {
		// no addtional Missiles saved
		return;
	}

	auto loadedVersion = file.NextLE<uint32_t>();
	if (loadedVersion > VersionAdditionalMissiles) {
		// unknown version
		return;
	}
	auto missileCountAdditional = file.NextLE<uint32_t>();
	for (uint32_t i = 0U; i < missileCountAdditional; i++) {
		LoadMissile(&file);
	}
}

void SaveLevel(SaveWriter &saveWriter, LevelConversionData *levelConversionData)
{
	Player &myPlayer = *MyPlayer;

	DoUnVision(myPlayer.position.tile, myPlayer._pLightRad); // fix for vision staying on the level

	if (leveltype == DTYPE_TOWN)
		glSeedTbl[0] = AdvanceRndSeed();

	char szName[MaxMpqPathSize];
	GetTempLevelNames(szName);
	SaveHelper file(saveWriter, szName, 256 * 1024);

	if (leveltype != DTYPE_TOWN) {
		for (int j = 0; j < MAXDUNY; j++) {
			for (int i = 0; i < MAXDUNX; i++) // NOLINT(modernize-loop-convert)
				file.WriteLE<int8_t>(dCorpse[i][j]);
		}
	}

	file.WriteBE<int32_t>(ActiveMonsterCount);
	file.WriteBE<int32_t>(ActiveItemCount);
	file.WriteBE<int32_t>(ActiveObjectCount);

	if (leveltype != DTYPE_TOWN) {
		for (int monsterId : ActiveMonsters)
			file.WriteBE<int32_t>(monsterId);
		for (size_t i = 0; i < ActiveMonsterCount; i++) {
			MonsterConversionData *monsterConversionData = nullptr;
			if (levelConversionData != nullptr)
				monsterConversionData = &levelConversionData->monsterConversionData[ActiveMonsters[i]];
			SaveMonster(&file, Monsters[ActiveMonsters[i]], monsterConversionData);
		}
		for (int objectId : ActiveObjects)
			file.WriteLE<int8_t>(objectId);
		for (int objectId : AvailableObjects)
			file.WriteLE<int8_t>(objectId);
		for (int i = 0; i < ActiveObjectCount; i++) {
			SaveObject(file, Objects[ActiveObjects[i]]);
		}
	}

	auto itemIndexes = SaveDroppedItems(file);

	for (int j = 0; j < MAXDUNY; j++) {
		for (int i = 0; i < MAXDUNX; i++) // NOLINT(modernize-loop-convert)
			file.WriteLE<uint8_t>(static_cast<uint8_t>(dFlags[i][j] & DungeonFlag::SavedFlags));
	}
	SaveDroppedItemLocations(file, itemIndexes);

	if (leveltype != DTYPE_TOWN) {
		for (int j = 0; j < MAXDUNY; j++) {
			for (int i = 0; i < MAXDUNX; i++) // NOLINT(modernize-loop-convert)
				file.WriteBE<int32_t>(dMonster[i][j]);
		}
		for (int j = 0; j < MAXDUNY; j++) {
			for (int i = 0; i < MAXDUNX; i++) // NOLINT(modernize-loop-convert)
				file.WriteLE<int8_t>(dObject[i][j]);
		}
		for (int j = 0; j < MAXDUNY; j++) {
			for (int i = 0; i < MAXDUNX; i++) // NOLINT(modernize-loop-convert)
				file.WriteLE<uint8_t>(dLight[i][j]);
		}
		for (int j = 0; j < MAXDUNY; j++) {
			for (int i = 0; i < MAXDUNX; i++) // NOLINT(modernize-loop-convert)
				file.WriteLE<uint8_t>(dPreLight[i][j]);
		}
		for (int j = 0; j < DMAXY; j++) {
			for (int i = 0; i < DMAXX; i++) // NOLINT(modernize-loop-convert)
				file.WriteLE<uint8_t>(AutomapView[i][j]);
		}
	}

	if (!setlevel)
		myPlayer._pLvlVisited[currlevel] = true;
	else
		myPlayer._pSLvlVisited[setlvlnum] = true;
}

void LoadLevel(LevelConversionData *levelConversionData)
{
	char szName[MaxMpqPathSize];
	std::optional<SaveReader> archive = OpenSaveArchive(gSaveNumber);
	GetTempLevelNames(szName);
	if (!archive || !archive->HasFile(szName))
		GetPermLevelNames(szName);
	LoadHelper file(std::move(archive), szName);
	if (!file.IsValid())
		app_fatal(_("Unable to open save file archive"));

	if (leveltype != DTYPE_TOWN) {
		for (int j = 0; j < MAXDUNY; j++) {
			for (int i = 0; i < MAXDUNX; i++) // NOLINT(modernize-loop-convert)
				dCorpse[i][j] = file.NextLE<int8_t>();
		}
		MoveLightsToCorpses();
	}

	ActiveMonsterCount = file.NextBE<int32_t>();
	auto savedItemCount = file.NextBE<uint32_t>();
	ActiveObjectCount = file.NextBE<int32_t>();

	if (leveltype != DTYPE_TOWN) {
		for (int &monsterId : ActiveMonsters)
			monsterId = file.NextBE<int32_t>();
		for (size_t i = 0; i < ActiveMonsterCount; i++) {
			Monster &monster = Monsters[ActiveMonsters[i]];
			MonsterConversionData *monsterConversionData = nullptr;
			if (levelConversionData != nullptr)
				monsterConversionData = &levelConversionData->monsterConversionData[ActiveMonsters[i]];
			LoadMonster(&file, monster, monsterConversionData);
			if (monster.isUnique() && monster.lightId != NO_LIGHT)
				Lights[monster.lightId].isInvalid = false;
		}
		if (!gbSkipSync) {
			for (size_t i = 0; i < ActiveMonsterCount; i++)
				SyncMonsterAnim(Monsters[ActiveMonsters[i]]);
		}
		for (int &objectId : ActiveObjects)
			objectId = file.NextLE<int8_t>();
		for (int &objectId : AvailableObjects)
			objectId = file.NextLE<int8_t>();
		for (int i = 0; i < ActiveObjectCount; i++)
			LoadObject(file, Objects[ActiveObjects[i]]);
		if (!gbSkipSync) {
			for (int i = 0; i < ActiveObjectCount; i++)
				SyncObjectAnim(Objects[ActiveObjects[i]]);
		}
	}

	LoadDroppedItems(file, savedItemCount);

	for (int j = 0; j < MAXDUNY; j++) {
		for (int i = 0; i < MAXDUNX; i++) // NOLINT(modernize-loop-convert)
			dFlags[i][j] = static_cast<DungeonFlag>(file.NextLE<uint8_t>()) & DungeonFlag::LoadedFlags;
	}

	// skip dItem indexes, this gets populated in LoadDroppedItems
	file.Skip<uint8_t>(MAXDUNX * MAXDUNY);

	if (leveltype != DTYPE_TOWN) {
		for (int j = 0; j < MAXDUNY; j++) {
			for (int i = 0; i < MAXDUNX; i++) // NOLINT(modernize-loop-convert)
				dMonster[i][j] = file.NextBE<int32_t>();
		}
		for (int j = 0; j < MAXDUNY; j++) {
			for (int i = 0; i < MAXDUNX; i++) // NOLINT(modernize-loop-convert)
				dObject[i][j] = file.NextLE<int8_t>();
		}
		file.Skip<uint8_t>(MAXDUNY * MAXDUNX); // dLight
		for (int j = 0; j < MAXDUNY; j++) {
			for (int i = 0; i < MAXDUNX; i++) // NOLINT(modernize-loop-convert)
				dPreLight[i][j] = file.NextLE<uint8_t>();
		}
		for (int j = 0; j < DMAXY; j++) {
			for (int i = 0; i < DMAXX; i++) { // NOLINT(modernize-loop-convert)
				const auto automapView = static_cast<MapExplorationType>(file.NextLE<uint8_t>());
				AutomapView[i][j] = automapView == MAP_EXP_OLD ? MAP_EXP_SELF : automapView;
			}
		}

		// No need to load dLight, we can recreate it accurately from LightList
		memcpy(dLight, dPreLight, sizeof(dLight));                                     // resets the light on entering a level to get rid of incorrect light
		ChangeLightXY(Players[MyPlayerId].lightId, Players[MyPlayerId].position.tile); // forces player light refresh
	} else {
		memset(dLight, 0, sizeof(dLight));
	}

	if (!gbSkipSync) {
		AutomapZoomReset();
		ResyncQuests();
		RedoMissileFlags();
		UpdateLighting = true;
	}

	for (Player &player : Players) {
		if (player.plractive && player.isOnActiveLevel())
			Lights[player.lightId].hasChanged = true;
	}
}

// The Oracool extension folded into SaveItem/LoadItemData's fixed-size item record.
//
// THIS CONSTANT SIZES THE SAVE BUFFER (see the SaveHelper in SaveHeroItems), so it is not
// documentation - it is load-bearing, and every field SaveItem writes has to be counted here or the
// writer runs off the end of the buffer it was given.
//
// It had already drifted before v9 found it: version 8 appended _iPLGoldFind's four bytes in
// August 2026 and this sum was never updated, so every hero save since has been written into a
// buffer four bytes per item too small. The symptom is not a clean failure - the loadsave round-trip
// test HANGS - which is exactly why the terms below are now spelled one per line with the version
// that added them. A sum of bare numbers is a sum nobody re-derives when they add a field.
constexpr int OracoolItemExtensionSaveSize =
    // v1: tier, perfect-roll, broken, prefix count, suffix count.
    5
    // v1: three prefixes and three suffixes, each a type byte plus two int32 params.
    + (Item::MaxOracoolAffixesPerSlot * 2) * (1 + 4 + 4)
    // v3: the socket block - one count byte plus one uint16 base index per slot.
    + 1 + Item::MaxItemSockets * 2
    // v4: the ethereal flag.
    + 1
    // v5: the item level.
    + 1
    // v6: the base tier.
    + 1
    // v8: the gold-find bonus. MISSING from this sum until v9 caught it.
    + 4
    // v9: the magic-find twin, and the Mystic Orb count.
    + 4 + 1;
const int DiabloItemSaveSize = 368 + OracoolItemExtensionSaveSize;
const int HellfireItemSaveSize = 372 + OracoolItemExtensionSaveSize;

bool IsStashSizeValid(size_t stashSize, uint8_t version, uint32_t pages, uint32_t itemCount)
{
	const size_t itemSize = (gbIsHellfire ? HellfireItemSaveSize : DiabloItemSaveSize);

	// Version 6 grew the header by the embedded item-format byte; version 5 has only its own byte.
	const size_t headerSize = version >= 6 ? 2 * sizeof(uint8_t) : sizeof(uint8_t);
	const size_t expectedSize = headerSize
	    + sizeof(uint32_t)
	    + sizeof(uint32_t)
	    + (sizeof(uint32_t) + StashGridColumns * StashGridRows * sizeof(uint16_t)) * pages
	    + sizeof(uint32_t)
	    + itemSize * itemCount
	    + sizeof(uint32_t);

	return stashSize == expectedSize;
}

} // namespace

void ConvertLevels(SaveWriter &saveWriter)
{
	// Backup current level state
	bool tmpSetlevel = setlevel;
	_setlevels tmpSetlvlnum = setlvlnum;
	int tmpCurrlevel = currlevel;
	dungeon_type tmpLeveltype = leveltype;

	gbSkipSync = true;

	setlevel = false; // Convert regular levels
	for (int i = 0; i < giNumberOfLevels; i++) {
		currlevel = i;
		if (!LevelFileExists(saveWriter))
			continue;

		leveltype = GetLevelType(currlevel);

		LevelConversionData levelConversionData;
		LoadLevel(&levelConversionData);
		SaveLevel(saveWriter, &levelConversionData);
	}

	setlevel = true; // Convert quest levels
	for (auto &quest : Quests) {
		if (quest._qactive == QUEST_NOTAVAIL) {
			continue;
		}

		leveltype = quest._qlvltype;
		if (leveltype == DTYPE_NONE) {
			continue;
		}

		setlvlnum = quest._qslvl;
		if (!LevelFileExists(saveWriter))
			continue;

		LevelConversionData levelConversionData;
		LoadLevel(&levelConversionData);
		SaveLevel(saveWriter, &levelConversionData);
	}

	gbSkipSync = false;

	// Restore current level state
	setlevel = tmpSetlevel;
	setlvlnum = tmpSetlvlnum;
	currlevel = tmpCurrlevel;
	leveltype = tmpLeveltype;
}

void RemoveInvalidItem(Item &item)
{
	bool isInvalid = !IsItemAvailable(item.IDidx) || !IsUniqueAvailable(item._iUid);

	if (!gbIsHellfire) {
		isInvalid = isInvalid || (item._itype == ItemType::Staff && GetSpellStaffLevel(item._iSpell) == -1);
		isInvalid = isInvalid || (item._iMiscId == IMISC_BOOK && GetSpellBookLevel(item._iSpell) == -1);
		isInvalid = isInvalid || item._iDamAcFlags != ItemSpecialEffectHf::None;
		isInvalid = isInvalid || item._iPrePower > IPL_LASTDIABLO;
		isInvalid = isInvalid || item._iSufPower > IPL_LASTDIABLO;
	}

	if (isInvalid) {
		item.clear();
	}
}

/**
 * Oracool bug fix: user report - "all new type items are gone after starting a new game."
 *
 * The six worn types (IDI_ORACOOL_SHOULDERS..IDI_ORACOOL_BOOTS, 168-173) sit past IDI_ARENAPOT,
 * squarely inside the `i >= 161` band RemapItemIdxToDiablo below writes off as "Hellfire
 * exclusive, does not exist in Diablo" - so in a Diablo game, PackItem/SaveItem mapped each one to
 * -1, which is the EMPTY SLOT marker. The items were destroyed at save time; New Game merely
 * showed the result. The vanilla seven survived because their ids sit below the band.
 *
 * They are not Hellfire items - they exist in both games - so both directions now pass the range
 * through unchanged, ahead of every shift. Claiming 168-173 in the Diablo save numbering is
 * provably collision-free: the Diablo format compresses all 168 Hellfire ids into 0-155 (minus 4
 * oils, 1 scroll, 7 runes/quest ids), with 166 special-cased for the Sorcerer staff, so no
 * legitimate vanilla save ever contains a value above 166.
 *
 * IDI_ORACOOL_HELM (174) extends the same range on the same reasoning - it is not one of the six
 * worn types (ILOC_HELM is an ordinary vanilla slot), but it is just as new an id and just as
 * exposed to the same remap-to-empty-slot bug if left out here.
 *
 * IsOracoolItemIdx itself now lives in itemdat.h - items.cpp needs the same range for a second,
 * unrelated reason (letting debug spawn commands past the IDROP_NEVER filter), and two files each
 * hand-maintaining the same id range is exactly the kind of drift that caused the bug this
 * function exists to fix in the first place.
 */
_item_indexes RemapItemIdxFromDiablo(_item_indexes i)
{
	constexpr auto GetItemIdValue = [](int i) -> int {
		if (IsOracoolAddedIdx(i)) {
			// Every Oracool-appended id - worn types, gems, charms, runes - stored as itself, see
			// above. This used to test only IsOracoolItemIdx, so the Phase 1 gems/charms/runes
			// (appended past that range) fell through into the "Hellfire exclusive" band and were
			// remapped to the empty-slot marker in Diablo-mode saves (external audit, 2026-08-17).
			return i;
		}
		if (i == IDI_SORCERER) {
			return IDI_SORCERER_DIABLO;
		}
		if (i >= 156) {
			i += 5; // Hellfire exclusive items
		}
		if (i >= 88) {
			i += 1; // Scroll of Search
		}
		if (i >= 83) {
			i += 4; // Oils
		}

		return i;
	};

	return static_cast<_item_indexes>(GetItemIdValue(i));
}

_item_indexes RemapItemIdxToDiablo(_item_indexes i)
{
	constexpr auto GetItemIdValue = [](int i) -> int {
		if (IsOracoolAddedIdx(i)) {
			return i; // ALL Oracool-appended ids - NOT Hellfire-exclusive; see RemapItemIdxFromDiablo
		}
		if (i == IDI_SORCERER_DIABLO) {
			return IDI_SORCERER;
		}
		if ((i >= 83 && i <= 86) || i == 92 || i >= 161) {
			return -1; // Hellfire exclusive items
		}
		if (i >= 93) {
			i -= 1; // Scroll of Search
		}
		if (i >= 87) {
			i -= 4; // Oils
		}

		return i;
	};

	return static_cast<_item_indexes>(GetItemIdValue(i));
}

_item_indexes RemapItemIdxFromSpawn(_item_indexes i)
{
	constexpr auto GetItemIdValue = [](int i) {
		if (IsOracoolAddedIdx(i)) {
			return i; // all Oracool-appended ids - same identity mapping as the Diablo remap
		}
		if (i >= 62) {
			i += 9; // Medium and heavy armors
		}
		if (i >= 96) {
			i += 1; // Scroll of Stone Curse
		}
		if (i >= 98) {
			i += 1; // Scroll of Guardian
		}
		if (i >= 99) {
			i += 1; // Scroll of ...
		}
		if (i >= 101) {
			i += 1; // Scroll of Golem
		}
		if (i >= 102) {
			i += 1; // Scroll of None
		}
		if (i >= 104) {
			i += 1; // Scroll of Apocalypse
		}

		return i;
	};

	return static_cast<_item_indexes>(GetItemIdValue(i));
}

_item_indexes RemapItemIdxToSpawn(_item_indexes i)
{
	constexpr auto GetItemIdValue = [](int i) {
		if (IsOracoolAddedIdx(i)) {
			return i; // all Oracool-appended ids - same identity mapping as the Diablo remap
		}
		if (i >= 104) {
			i -= 1; // Scroll of Apocalypse
		}
		if (i >= 102) {
			i -= 1; // Scroll of None
		}
		if (i >= 101) {
			i -= 1; // Scroll of Golem
		}
		if (i >= 99) {
			i -= 1; // Scroll of ...
		}
		if (i >= 98) {
			i -= 1; // Scroll of Guardian
		}
		if (i >= 96) {
			i -= 1; // Scroll of Stone Curse
		}
		if (i >= 71) {
			i -= 9; // Medium and heavy armors
		}

		return i;
	};

	return static_cast<_item_indexes>(GetItemIdValue(i));
}

bool IsHeaderValid(uint32_t magicNumber)
{
	gbIsHellfireSaveGame = false;
	if (magicNumber == LoadLE32("SHAR")) {
		return true;
	}
	if (magicNumber == LoadLE32("SHLF")) {
		gbIsHellfireSaveGame = true;
		return true;
	}
	if (!gbIsSpawn && magicNumber == LoadLE32("RETL")) {
		return true;
	}
	if (!gbIsSpawn && magicNumber == LoadLE32("HELF")) {
		gbIsHellfireSaveGame = true;
		return true;
	}

	return false;
}

// Returns the size of the hotkeys file with the number of hotkeys passed and if a header with the number of hotkeys is present in the file
size_t HotkeysSize(size_t nHotkeys = NumHotkeys)
{
	//     header            spells                         spell types                    active spell      active spell type
	return sizeof(uint8_t) + (nHotkeys * sizeof(int32_t)) + (nHotkeys * sizeof(uint8_t)) + sizeof(int32_t) + sizeof(uint8_t);
}

/**
 * @brief The size of a hotkeys chunk that also carries the LEFT button's readied spell.
 *
 * Vanilla has one readied spell - the right button's - so its chunk ends there. This fork has two,
 * and the left one was never written, which is the whole of the bug below.
 *
 * Appended AFTER vanilla's trailer rather than woven in, so a file written before this still reads:
 * LoadHotkeys checks for the extra pair and simply does not find it.
 */
size_t HotkeysSizeWithLeft(size_t nHotkeys = NumHotkeys)
{
	return HotkeysSize(nHotkeys) + sizeof(int32_t) + sizeof(uint8_t);
}

void LoadHotkeys()
{
	LoadHelper file(OpenSaveArchive(gSaveNumber), "hotkeys");
	if (!file.IsValid())
		return;

	Player &myPlayer = *MyPlayer;
	size_t nHotkeys = 4; // Defaults to old save format number

	// THE FILL IS GONE (user, 2026-09-02: "make hotkeys remembered over games. i dont want to set
	// hotkeys every new game. this is PER CHARACTER setting.").
	//
	// It is vanilla's, and vanilla is right about it: there, this chunk is the only record of the
	// bindings, so clearing the array before reading it is just how you load an array. This fork
	// carries them in the HERO file instead - HeroChunkSpellHotkeys, its Left counterpart and
	// HeroChunkAuraHotkeys, all three applied by ApplyHeroChunks before this function runs - and
	// against that the fill is destructive: it wiped the bindings the hero file had just supplied
	// and then refilled them from the GAME save, which V1 never continues from and which therefore
	// holds whichever session last happened to write it. Every new game the F-keys came back as some
	// older set, or as nothing.
	//
	// So this chunk is now a FALLBACK rather than the truth, exactly like the readied pair below: it
	// fills slots the hero file left empty and overwrites none that it filled. Same reasoning, same
	// session, same bug - see the note on the readied spell further down.

	// Checking if the save file has the old format with only 4 hotkeys and no header
	if (file.IsValid(HotkeysSize(nHotkeys))) {
		// The file contains a header byte and at least 4 entries, so we can assume it's a new format save
		nHotkeys = file.NextLE<uint8_t>();
	}

	// Read all hotkeys in the file. Buffered rather than written straight into the player, because
	// the spells and their types arrive in two separate runs and a slot has to be judged on the pair:
	// applying the spell in the first loop would commit to a binding whose type has not been read
	// yet, and the second loop would then have to remember which slots the first one took.
	SpellID savedSpell[NumHotkeys];
	SpellType savedType[NumHotkeys];
	std::fill(std::begin(savedSpell), std::end(savedSpell), SpellID::Invalid);
	std::fill(std::begin(savedType), std::end(savedType), SpellType::Invalid);
	for (size_t i = 0; i < nHotkeys; i++) {
		// Do not load hotkeys past the size of the spell types array, discard the rest
		if (i < NumHotkeys) {
			savedSpell[i] = static_cast<SpellID>(file.NextLE<int32_t>());
		} else {
			file.Skip<int32_t>();
		}
	}
	for (size_t i = 0; i < nHotkeys; i++) {
		// Do not load hotkeys past the size of the spells array, discard the rest
		if (i < NumHotkeys) {
			savedType[i] = static_cast<SpellType>(file.NextLE<uint8_t>());
		} else {
			file.Skip<uint8_t>();
		}
	}
	for (size_t i = 0; i < NumHotkeys; i++) {
		// An empty slot takes the offer; a bound one keeps what the hero file gave it.
		if (IsValidSpell(myPlayer._pSplHotKey[i]) || !IsValidSpell(savedSpell[i]))
			continue;
		myPlayer._pSplHotKey[i] = savedSpell[i];
		myPlayer._pSplTHotKey[i] = savedType[i];
	}

	// Load the selected spell last.
	//
	// Applied only when the slot is not ALREADY holding a real spell (2026-09-02). This chunk belongs
	// to the GAME save, and V1 always starts a new game - so what it holds is the readied pair of
	// some previous session, while the hero file's pair has just been decoded into these fields by
	// pfile_read_player_from_save. Overwriting the second with the first is how the newer answer got
	// replaced by the older one. A genuinely empty slot still takes whatever this chunk offers.
	const SpellID savedRight = static_cast<SpellID>(file.NextLE<int32_t>());
	const auto savedRightType = static_cast<SpellType>(file.NextLE<uint8_t>());
	if (!IsValidSpell(myPlayer._pRSpell)) {
		myPlayer._pRSpell = savedRight;
		myPlayer._pRSplType = savedRightType;
	}

	// AND THE LEFT BUTTON'S (user, 2026-09-02: "lmb still doesnt remember the skill i assigned to
	// it"). This chunk is vanilla's, and vanilla has exactly one readied spell - the right button's,
	// read directly above. The fork added a second button and never extended this.
	//
	// That alone would have been harmless, because pack.cpp restores BOTH readied slots from the
	// hero file. What made it a bug is the order: InitPlayer(firstTime) clears both slots, and then
	// this function runs and puts only the right one back. So the left button's skill was loaded
	// correctly, wiped, and never restored - which is exactly the asymmetry that was reported, the
	// right button remembering and the left forgetting.
	//
	// Guarded on the chunk actually being long enough, so a hero saved before this reads cleanly and
	// simply keeps whatever the pack path established.
	if (file.IsValid(HotkeysSizeWithLeft(nHotkeys))) {
		const SpellID savedLeft = static_cast<SpellID>(file.NextLE<int32_t>());
		const auto savedLeftType = static_cast<SpellType>(file.NextLE<uint8_t>());
		if (!IsValidSpell(myPlayer._pLRSpell)) {
			myPlayer._pLRSpell = savedLeft;
			myPlayer._pLRSplType = savedLeftType;
		}
	}
}

void SaveHotkeys(SaveWriter &saveWriter, const Player &player)
{
	SaveHelper file(saveWriter, "hotkeys", HotkeysSizeWithLeft());

	// Write the number of spell hotkeys
	file.WriteLE<uint8_t>(static_cast<uint8_t>(NumHotkeys));

	// Write the spell hotkeys
	for (auto &spellId : player._pSplHotKey) {
		file.WriteLE<int32_t>(static_cast<int8_t>(spellId));
	}
	for (auto &spellType : player._pSplTHotKey) {
		file.WriteLE<uint8_t>(static_cast<uint8_t>(spellType));
	}

	// Write the selected spell last
	file.WriteLE<int32_t>(static_cast<int8_t>(player._pRSpell));
	file.WriteLE<uint8_t>(static_cast<uint8_t>(player._pRSplType));

	// The left button's, appended after vanilla's trailer - see HotkeysSizeWithLeft. Without this
	// pair the load path above has nothing to restore, and InitPlayer's reset is the last word on
	// what the left button holds.
	file.WriteLE<int32_t>(static_cast<int8_t>(player._pLRSpell));
	file.WriteLE<uint8_t>(static_cast<uint8_t>(player._pLRSplType));
}

void LoadHeroItems(Player &player, uint32_t saveNumber)
{
	// The slot is a PARAMETER, not the gSaveNumber global (external audit, 2026-08-17): the
	// hero-select preview loop iterates every slot, and reading the global here meant every
	// hero's preview wore the SELECTED slot's equipment. The real load path passes the same
	// number the global holds, so it is unchanged in behavior - but now by contract, not luck.
	LoadHelper file(OpenSaveArchive(saveNumber), "heroitems");
	if (!file.IsValid())
		return;

	gbIsHellfireSaveGame = file.NextBool8();

	if (file.NextLE<uint8_t>() != OracoolItemFormatVersion) {
		// The fixed-size item record grew when Oracool tier/affix data was folded directly
		// into it; reading an older, shorter record with today's field layout would silently
		// misalign every item after this point rather than failing cleanly.
		app_fatal(_("This save is from an incompatible version of Diablo Orcl and cannot be loaded. Please start a new character."));
	}

	LoadMatchingItems(file, player, NUM_INVLOC, player.InvBody);
	LoadMatchingItems(file, player, InventoryGridCells, player.InvList);
	LoadMatchingItems(file, player, MaxBeltItems, player.SpdList);

	gbIsHellfireSaveGame = gbIsHellfire;
}

// Oracool bug fix: user report - old Rare/Buffed Unique items pulled from the Stash showed wildly
// wrong affix magnitudes (e.g. "+11126% fire resist") while their actual applied bonus (verified
// by unequipping/re-equipping) stayed correct. Root cause: StashVersion tracked only the Stash
// file's own page/grid layout, but every item embedded in it is also subject to
// OracoolItemFormatVersion (loadsave.cpp), which has grown more than once (e.g. when
// _iOracoolBroken was added) without StashVersion ever being bumped alongside it - so a Stash
// file saved before one of those per-item growths got silently read with today's longer per-item
// layout, byte-shifting every field after the divergence point (which is exactly why only some
// affixes - the ones whose fields land after the shift - showed garbage, while the item's name
// and base stats, read before it, stayed intact). Bumped to 1 and switched to an exact-match
// check (matching heroitems/heroinvtabs's existing pattern) so a stale-format Stash is rejected
// cleanly instead of silently misread.
// Oracool V1: bumped to 2 when the stash page grew from 10x10 to 10x17 (StashGridRows). A version
// 1 file has a different cell count per page, so it cannot be read into the new grid - LoadStash
// rejects it and says so, which is the supported path rather than silent corruption. Wiping was an
// explicit call: "we are developing a product, saves are not important."
// Oracool V1: bumped to 3 when the page lost its last row to the health orb, 10x17 -> 10x16. Same
// reasoning as the bump before it - the cell count per page changed, so a version 2 stash is
// rejected rather than read at the wrong stride. Existing stashes are lost, which is the known cost
// of that call.
// Version 4 (Megaplan Phase 1): bumped ALONGSIDE OracoolItemFormatVersion 3 (the socket fields),
// per the lesson recorded above - every item embedded in the stash file is subject to the item
// record's layout, so the two versions must move together. Existing stashes are lost; known cost.
// Version 6 (external audit, 2026-08-17): "the two versions must move together" was a rule held
// by memory, and heroinvtabs proved memory fails - so version 6 EMBEDS OracoolItemFormatVersion
// as a second header byte, checked on load, and a future item-format bump orphans stale stashes
// by itself. A version-5 file (the current build's own output until this change) is still
// accepted and parsed with today's item format, which is the format it was written in; the next
// save rewrites it as version 6.
constexpr uint8_t StashVersion = 6;

void LoadStash()
{
	const char *filename;
	if (!gbIsMultiplayer)
		filename = "spstashitems";
	else
		filename = "mpstashitems";

	Stash = {};

	LoadHelper file(OpenStashArchive(), filename);
	if (!file.IsValid())
		return;

	auto version = file.NextLE<uint8_t>();
	if (version != StashVersion && version != 5) {
		EventPlrMsg(_("This save's Stash is from an incompatible version of Diablo Orcl and cannot be loaded. Items already in the Stash could not be recovered; new items placed in the Stash will be saved correctly from now on."), UiFlags::ColorRed);
		return;
	}
	// Version 6 carries the item schema its records were written with; version 5 is the current
	// build's own pre-audit output, parsed with today's format. See the StashVersion note.
	if (version == StashVersion && file.NextLE<uint8_t>() != OracoolItemFormatVersion) {
		EventPlrMsg(_("This save's Stash is from an incompatible version of Diablo Orcl and cannot be loaded. Items already in the Stash could not be recovered; new items placed in the Stash will be saved correctly from now on."), UiFlags::ColorRed);
		return;
	}

	Stash.gold = file.NextLE<uint32_t>();

	auto pages = file.NextLE<uint32_t>();
	// Self-audit (2026-08-15): bound `pages` BEFORE the loop below trusts it. IsStashSizeValid
	// rejects a torn file exactly, but it runs after the grids are read - so a corrupted count in
	// the billions would spin this loop against an exhausted stream for minutes before the
	// validation ever saw it. The file itself is the cheapest ceiling: a page costs at least its
	// grid, so more pages than the whole file could hold is corruption, answered the same way the
	// exact check answers it. This matters more here than in vanilla, whose stash is written once
	// per session - this fork's autosave rewrites it constantly, so a torn write is a matter of time.
	constexpr size_t PageSaveSize = sizeof(uint32_t) + StashGridColumns * StashGridRows * sizeof(uint16_t);
	if (pages > file.Size() / PageSaveSize) {
		Stash = {};
		EventPlrMsg(_("Stash size invalid. If you attempt to access your stash, data will be overwritten!!"), UiFlags::ColorRed);
		return;
	}
	for (unsigned i = 0; i < pages; i++) {
		auto page = file.NextLE<uint32_t>();
		for (auto &row : Stash.stashGrids[page]) {
			for (uint16_t &cell : row) {
				cell = file.NextLE<uint16_t>();
			}
		}
	}

	auto itemCount = file.NextLE<uint32_t>();
	if (!IsStashSizeValid(file.Size(), version, pages, itemCount)) {
		Stash = {};
		EventPlrMsg(_("Stash size invalid. If you attempt to access your stash, data will be overwritten!!"), UiFlags::ColorRed);
		return;
	}
	Stash.stashList.resize(itemCount);
	for (unsigned i = 0; i < itemCount; i++) {
		LoadAndValidateItemData(file, Stash.stashList[i]);
	}

	// And the grids must agree with the list they were saved beside (self-audit, 2026-08-15): a cell
	// is an index+1 into stashList, and GetItemIdAtPosition feeds it to a std::vector unchecked, so a
	// cell past itemCount is an out-of-bounds read waiting on a hover. Same guard, same reasoning as
	// LoadInventoryTabs' - the byte-size check above proves the file's SHAPE, not that its references
	// point inside each other. A bad cell becomes an empty one; the items themselves are kept.
	for (auto &[pageNumber, grid] : Stash.stashGrids) {
		for (auto &column : grid) {
			for (uint16_t &cell : column) {
				if (cell > itemCount)
					cell = 0;
			}
		}
	}

	Stash.SetPage(file.NextLE<uint32_t>());
}

/**
 * @brief Oracool Tabbed Inventory (2026-08-04): 9 extra backpack pages beyond the original
 * InvList/InvGrid/_pNumInv (tab 1, left completely untouched here so every existing save/equip/
 * quest/network code path is unaffected). PlayerPack/ItemNetPack are validated with a strict
 * sizeof() equality check, so these 9 extra grids live in their own new, separately-versioned
 * archive sub-file rather than growing InventoryGridCells (which would also overflow InvGrid's
 * int8_t index encoding well before a meaningfully larger inventory could ever be reached).
 * Old saves simply lack this sub-file; every extra tab defaults to empty, matching Player's own
 * in-memory default. Each tab item's Oracool tier/affix data is carried for free via the same
 * SaveItem/LoadItemData calls this file already makes per item - see OracoolItemFormatVersion.
 */
// Bumped to 1 for v0.2.0: LoadItem's underlying per-item record grew when Oracool tier/affix
// data was folded directly into SaveItem/LoadItemData, so a pre-v0.2.0 "heroinvtabs" file's
// items are no longer at the byte offsets this build expects. Checked for exact equality
// (not just "is this newer than what I understand") so an old-format file is rejected the
// same safe way an unrecognized future one already was - every extra tab just stays empty,
// matching the existing "absent = default" pattern; nothing is destroyed or misaligned.
//
// Version 3 (external audit, 2026-08-17): the version byte above guarded the CONTAINER's layout
// but not the item records inside it - the exact trap the StashVersion comment below records
// ("every item embedded is subject to OracoolItemFormatVersion... the two versions must move
// together"), applied to the stash and missed here: the item record grew twice (sockets, the
// ethereal flag) while this file kept advertising 2. Rather than trust anyone to remember next
// time, version 3 EMBEDS OracoolItemFormatVersion as a second header byte, checked on load - a
// future item-format bump orphans old tab files automatically instead of misreading them.
// A version-2 file is still accepted, parsed with TODAY's item format: this file is rewritten by
// every autosave, so any live hero's copy was written by the current build and reads correctly;
// a genuinely stale one misreads no worse than it already did, once, and the per-item validation
// plus the grid guards below bound the damage until the next save rewrites it as version 3.
constexpr uint8_t OracoolInvTabsVersion = 3;

void LoadInventoryTabs(Player &player, uint32_t saveNumber)
{
	player.InvTabList = {};
	player.InvTabGrid = {};
	player._pNumInvTab = {};

	// Parameterized for the same reason as LoadHeroItems: the preview loop reads OTHER slots.
	LoadHelper file(OpenSaveArchive(saveNumber), "heroinvtabs");
	if (!file.IsValid())
		return; // no extra-tab data: an old save, or one where nothing was ever stored there

	const uint8_t version = file.NextLE<uint8_t>();
	// Version 2 is no longer accepted (third audit, 2026-08-19). It was allowed on the reasoning that
	// this file is rewritten by every autosave, so a live hero's copy is always current - true, but it
	// was parsed with TODAY's item format and had no total-size guard, only per-item validation. The
	// item record has since grown twice more (the ilvl byte, the base-tier byte), so a genuinely stale
	// v2 file would now drift two bytes per item through the stream and read later tabs as garbage,
	// silently. The stash rejects its equivalent outright and says so; this now does the same, and
	// unlike the stash it costs nothing - an unreadable tab file simply leaves the tabs empty.
	if (version != OracoolInvTabsVersion)
		return; // unrecognized or outgrown format; every extra tab stays empty
	if (file.NextLE<uint8_t>() != OracoolItemFormatVersion)
		return; // the embedded item schema disagrees with this build's; see the version-3 note above

	for (int t = 0; t < Player::NumExtraInventoryTabs; t++) {
		if (!file.IsValid())
			return; // corrupt/truncated stream; tabs already loaded stay loaded

		for (int8_t &cell : player.InvTabGrid[t])
			cell = file.NextLE<int8_t>();

		const uint8_t itemCount = file.NextLE<uint8_t>();
		if (itemCount > InventoryGridCells)
			return; // implausible count; stop here, tabs already loaded stay loaded

		// Self-audit (2026-08-15): the grid must agree with the count it was saved beside. A cell is
		// an index+1 into InvTabList (negatives mark a multi-cell item's continuation), and a cell
		// pointing past itemCount indexes items this record never wrote - values up to 127 would read
		// past the 70-item array entirely. The main InvGrid shares this trust model, but it is written
		// once per save slot where this file is rewritten on every autosave, so a torn write is a
		// plausibility here, not a hypothetical. A bad cell becomes an empty one; the item list itself
		// is untouched, so at worst an item loses its grid spot rather than the tab losing its items.
		for (int8_t &cell : player.InvTabGrid[t]) {
			if (abs(cell) > itemCount)
				cell = 0;
		}

		// The count must also agree with the grid (external audit, 2026-08-17): a written item
		// always owns exactly one positive anchor cell, so a count the grid cannot account for is a
		// record inconsistent with itself - and the count is what the in-game append paths index by,
		// so accepting it would park the tab one paste away from writing past the list. The item
		// records are still consumed below either way (they are variable-length; skipping them
		// blind would misalign every tab after this one) - the tab is emptied after the read.
		bool anchorsConsistent = true;
		for (int idx = 1; idx <= itemCount; idx++) {
			bool anchored = false;
			for (const int8_t cell : player.InvTabGrid[t]) {
				if (cell == idx) {
					anchored = true;
					break;
				}
			}
			if (!anchored) {
				anchorsConsistent = false;
				break;
			}
		}

		player._pNumInvTab[t] = itemCount;
		for (uint8_t i = 0; i < itemCount; i++) {
			if (!file.IsValid())
				return;
			LoadAndValidateItemData(file, player.InvTabList[t][i]);
		}

		if (!anchorsConsistent) {
			player.InvTabList[t] = {};
			player.InvTabGrid[t] = {};
			player._pNumInvTab[t] = 0;
			continue;
		}

		// An item LoadAndValidateItemData cleared (failed validation) keeps its list slot but must
		// lose its grid cells - the tab equivalent of RemoveEmptyInventory's sweep, so a hover or
		// paste never resolves a cell onto an empty record.
		for (int8_t &cell : player.InvTabGrid[t]) {
			if (cell != 0 && player.InvTabList[t][abs(cell) - 1].isEmpty())
				cell = 0;
		}
	}
}

void RemoveEmptyInventory(Player &player)
{
	for (int i = InventoryGridCells; i > 0; i--) {
		int8_t idx = player.InvGrid[i - 1];
		if (idx > 0 && player.InvList[idx - 1].isEmpty()) {
			player.RemoveInvItem(idx - 1);
		}
	}
}

void LoadGame(bool firstflag)
{
	FreeGameMem();

	LoadHelper file(OpenSaveArchive(gSaveNumber), "game");
	if (!file.IsValid())
		app_fatal(_("Unable to open save file archive"));

	if (!IsHeaderValid(file.NextLE<uint32_t>()))
		app_fatal(_("Invalid save file"));

	if (gbIsHellfireSaveGame) {
		giNumberOfLevels = 25;
		giNumberQuests = 24;
	} else {
		// Todo initialize additional levels and quests if we are running Hellfire
		giNumberOfLevels = 17;
		giNumberQuests = 16;
	}
	// Oracool: the premium stock is SMITH_PREMIUM_ITEMS long in both games now (the shop grid holds
	// it), so the save carries all of it either way. This is a GAME-state format change, not the
	// hero file's - a save written before v1.9.28 will not load. V1 always starts a new game.
	giNumberOfSmithPremiumItems = SMITH_PREMIUM_ITEMS;

	pfile_remove_temp_files();

	setlevel = file.NextBool8();
	setlvlnum = static_cast<_setlevels>(file.NextBE<uint32_t>());
	currlevel = file.NextBE<uint32_t>();
	leveltype = static_cast<dungeon_type>(file.NextBE<uint32_t>());
	if (!setlevel)
		leveltype = GetLevelType(currlevel);
	int viewX = file.NextBE<int32_t>();
	int viewY = file.NextBE<int32_t>();
	invflag = file.NextBool8();
	chrflag = file.NextBool8();
	int tmpNummonsters = file.NextBE<int32_t>();
	auto savedItemCount = file.NextBE<uint32_t>();
	int tmpNummissiles = file.NextBE<int32_t>();
	int tmpNobjects = file.NextBE<int32_t>();

	if (!gbIsHellfire && IsAnyOf(leveltype, DTYPE_NEST, DTYPE_CRYPT))
		app_fatal(_("Player is on a Hellfire only level"));

	for (uint8_t i = 0; i < giNumberOfLevels; i++) {
		glSeedTbl[i] = file.NextBE<uint32_t>();
		file.Skip(4); // Skip loading gnLevelTypeTbl
	}

	Player &myPlayer = *MyPlayer;

	LoadPlayer(file, myPlayer);

	if (sgGameInitInfo.nDifficulty < DIFF_NORMAL || sgGameInitInfo.nDifficulty > DIFF_LAST)
		sgGameInitInfo.nDifficulty = DIFF_NORMAL;

	for (int i = 0; i < giNumberQuests; i++)
		LoadQuest(&file, i);
	// Oracool: user request - quest progression is intentionally session-only and never persisted
	// across a load. The loop above still reads every quest's bytes so the file cursor lands
	// correctly for LoadPortal and everything that follows, but InitQuests() immediately discards
	// whatever was just read and reinitializes every quest to the same fresh state a brand-new
	// character starts with (including this save's own quest-pool randomization, since glSeedTbl
	// is already loaded by this point). This also resyncs the new-quest-added log detector, same
	// as the SyncQuestLogState() call this replaces.
	InitQuests();
	for (int i = 0; i < MAXPORTAL; i++)
		LoadPortal(&file, i);

	if (gbIsHellfireSaveGame != gbIsHellfire) {
		pfile_convert_levels();
		RemoveEmptyInventory(myPlayer);
	}

	LoadGameLevel(firstflag, ENTRY_LOAD);
	SetPlrAnims(myPlayer);
	SyncPlrAnim(myPlayer);

	ViewPosition = { viewX, viewY };
	ActiveMonsterCount = tmpNummonsters;
	ActiveObjectCount = tmpNobjects;

	for (int &monstkill : MonsterKillCounts)
		monstkill = file.NextBE<int32_t>();

	// skip ahead for vanilla save compatibility (Related to bugfix where MonsterKillCounts[MaxMonsters] was changed to MonsterKillCounts[NUM_MTYPES]
	file.Skip(4 * (MaxMonsters - NUM_MTYPES));
	if (leveltype != DTYPE_TOWN) {
		for (int &monsterId : ActiveMonsters)
			monsterId = file.NextBE<int32_t>();
		for (size_t i = 0; i < ActiveMonsterCount; i++)
			LoadMonster(&file, Monsters[ActiveMonsters[i]]);
		for (size_t i = 0; i < ActiveMonsterCount; i++)
			SyncPackSize(Monsters[ActiveMonsters[i]]);
		// Skip ActiveMissiles
		file.Skip<int8_t>(MaxMissilesForSaveGame);
		// Skip AvailableMissiles
		file.Skip<int8_t>(MaxMissilesForSaveGame);
		for (int i = 0; i < tmpNummissiles; i++)
			LoadMissile(&file);
		// For petrified monsters, the data in missile.var1 must be used to
		// load the appropriate animation data for the monster in missile.var2
		for (size_t i = 0; i < ActiveMonsterCount; i++)
			SyncMonsterAnim(Monsters[ActiveMonsters[i]]);
		for (int &objectId : ActiveObjects)
			objectId = file.NextLE<int8_t>();
		for (int &objectId : AvailableObjects)
			objectId = file.NextLE<int8_t>();
		for (int i = 0; i < ActiveObjectCount; i++)
			LoadObject(file, Objects[ActiveObjects[i]]);
		for (int i = 0; i < ActiveObjectCount; i++)
			SyncObjectAnim(Objects[ActiveObjects[i]]);

		ActiveLightCount = file.NextBE<int32_t>();

		for (uint8_t &lightId : ActiveLights)
			lightId = file.NextLE<uint8_t>();
		for (int i = 0; i < ActiveLightCount; i++)
			LoadLighting(&file, &Lights[ActiveLights[i]]);

		file.Skip<int32_t>(); // VisionId
		int visionCount = file.NextBE<int32_t>();

		for (int i = 0; i < visionCount; i++) {
			LoadLighting(&file, &VisionList[i]);
			VisionActive[i] = true;
		}
	}

	LoadDroppedItems(file, savedItemCount);

	LoadAdditionalMissiles();

	for (bool &uniqueItemFlag : UniqueItemFlags)
		uniqueItemFlag = file.NextBool8();

	file.Skip<uint8_t>(MAXDUNY * MAXDUNX); // dLight
	for (int j = 0; j < MAXDUNY; j++) {
		for (int i = 0; i < MAXDUNX; i++) // NOLINT(modernize-loop-convert)
			dFlags[i][j] = static_cast<DungeonFlag>(file.NextLE<uint8_t>()) & DungeonFlag::LoadedFlags;
	}
	for (int j = 0; j < MAXDUNY; j++) {
		for (int i = 0; i < MAXDUNX; i++) // NOLINT(modernize-loop-convert)
			dPlayer[i][j] = file.NextLE<int8_t>();
	}

	// skip dItem indexes, this gets populated in LoadDroppedItems
	file.Skip<uint8_t>(MAXDUNX * MAXDUNY);

	if (leveltype != DTYPE_TOWN) {
		for (int j = 0; j < MAXDUNY; j++) {
			for (int i = 0; i < MAXDUNX; i++) // NOLINT(modernize-loop-convert)
				dMonster[i][j] = file.NextBE<int32_t>();
		}
		for (int j = 0; j < MAXDUNY; j++) {
			for (int i = 0; i < MAXDUNX; i++) // NOLINT(modernize-loop-convert)
				dCorpse[i][j] = file.NextLE<int8_t>();
		}
		for (int j = 0; j < MAXDUNY; j++) {
			for (int i = 0; i < MAXDUNX; i++) // NOLINT(modernize-loop-convert)
				dObject[i][j] = file.NextLE<int8_t>();
		}
		file.Skip<uint8_t>(MAXDUNY * MAXDUNX); // dLight
		for (int j = 0; j < MAXDUNY; j++) {
			for (int i = 0; i < MAXDUNX; i++) // NOLINT(modernize-loop-convert)
				dPreLight[i][j] = file.NextLE<uint8_t>();
		}
		for (int j = 0; j < DMAXY; j++) {
			for (int i = 0; i < DMAXX; i++) { // NOLINT(modernize-loop-convert)
				const auto automapView = static_cast<MapExplorationType>(file.NextLE<uint8_t>());
				AutomapView[i][j] = automapView == MAP_EXP_OLD ? MAP_EXP_SELF : automapView;
			}
		}
		file.Skip(MAXDUNX * MAXDUNY); // dMissile

		// No need to load dLight, we can recreate it accurately from LightList
		memcpy(dLight, dPreLight, sizeof(dLight));               // resets the light on entering a level to get rid of incorrect light
		ChangeLightXY(myPlayer.lightId, myPlayer.position.tile); // forces player light refresh
	} else {
		memset(dLight, 0, sizeof(dLight));
	}

	numpremium = file.NextBE<int32_t>();
	premiumlevel = file.NextBE<int32_t>();

	for (int i = 0; i < giNumberOfSmithPremiumItems; i++)
		LoadPremium(file, i);
	if (gbIsHellfire && !gbIsHellfireSaveGame)
		SpawnPremium(myPlayer);

	AutomapActive = file.NextBool8();
	AutoMapScale = file.NextBE<int32_t>();
	AutomapZoomReset();
	ResyncQuests();

	if (leveltype != DTYPE_TOWN) {
		RedoPlayerVision();
		ProcessVisionList();
		ProcessLightList();
	}

	// convert stray manashield missiles into pManaShield flag
	for (auto &missile : Missiles) {
		if (missile._mitype == MissileID::ManaShield && !missile._miDelFlag) {
			Players[missile._misource].pManaShield = true;
			missile._miDelFlag = true;
		}
	}

	missiles_process_charge();
	RedoMissileFlags();
	NewCursor(CURSOR_HAND);
	gbProcessPlayers = IsDiabloAlive(!firstflag);

	if (gbIsHellfireSaveGame != gbIsHellfire) {
		SaveGame();
	}

	gbIsHellfireSaveGame = gbIsHellfire;
}

void SaveHeroItems(SaveWriter &saveWriter, Player &player)
{
	size_t itemCount = static_cast<size_t>(NUM_INVLOC) + InventoryGridCells + MaxBeltItems;
	SaveHelper file(saveWriter, "heroitems", itemCount * (gbIsHellfire ? HellfireItemSaveSize : DiabloItemSaveSize) + sizeof(uint8_t) * 2);

	file.WriteLE<uint8_t>(gbIsHellfire ? 1 : 0);
	file.WriteLE<uint8_t>(OracoolItemFormatVersion);

	for (const Item &item : player.InvBody)
		SaveItem(file, item);
	for (const Item &item : player.InvList)
		SaveItem(file, item);
	for (const Item &item : player.SpdList)
		SaveItem(file, item);
}

void SaveStash(SaveWriter &stashWriter)
{
	const char *filename;
	if (!gbIsMultiplayer)
		filename = "spstashitems";
	else
		filename = "mpstashitems";

	const int itemSize = (gbIsHellfire ? HellfireItemSaveSize : DiabloItemSaveSize);

	SaveHelper file(
	    stashWriter,
	    filename,
	    2 * sizeof(uint8_t) // container version + embedded item-format version
	        + sizeof(uint32_t)
	        + sizeof(uint32_t)
	        // From the constants, not a literal 10 * 10. This was still saying 10x10 while the page
	        // was 10x17, so the figure has been wrong since that change - harmless, because it only
	        // sizes the write buffer, but it is the same divergence StashGridRows lives in stash.h to
	        // prevent and there was no reason for it to be exempt.
	        + (sizeof(uint32_t) + StashGridColumns * StashGridRows * sizeof(uint16_t)) * Stash.stashGrids.size()
	        + sizeof(uint32_t)
	        + itemSize * Stash.stashList.size()
	        + sizeof(uint32_t));

	file.WriteLE<uint8_t>(StashVersion);
	// The embedded item schema - see the StashVersion note. LoadStash checks this on version 6+.
	file.WriteLE<uint8_t>(OracoolItemFormatVersion);

	file.WriteLE<uint32_t>(Stash.gold);

	std::vector<unsigned> pagesToSave;
	for (const auto &stashPage : Stash.stashGrids) {
		if (std::any_of(stashPage.second.cbegin(), stashPage.second.cend(), [](const auto &row) {
			    return std::any_of(row.cbegin(), row.cend(), [](auto cell) {
				    return cell > 0;
			    });
		    })) {
			// found a page that contains at least one item
			pagesToSave.push_back(stashPage.first);
		}
	};

	// Current stash size is 100 pages. Will definitely fit in a 32 bit value.
	file.WriteLE<uint32_t>(static_cast<uint32_t>(pagesToSave.size()));
	for (const auto &page : pagesToSave) {
		file.WriteLE<uint32_t>(page);
		for (const auto &row : Stash.stashGrids[page]) {
			for (uint16_t cell : row) {
				file.WriteLE<uint16_t>(cell);
			}
		}
	}

	// 100 pages of 100 items is still only 10 000, as with the page count will definitely fit in 32 bits even in the worst case.
	file.WriteLE<uint32_t>(static_cast<uint32_t>(Stash.stashList.size()));
	for (const Item &item : Stash.stashList) {
		SaveItem(file, item);
	}

	file.WriteLE<uint32_t>(static_cast<uint32_t>(Stash.GetPage()));
}

/** @brief Saves the Oracool Tabbed Inventory's 9 extra backpack pages; see LoadInventoryTabs. */
void SaveInventoryTabs(SaveWriter &saveWriter, const Player &player)
{
	bool anyItems = false;
	for (int numInTab : player._pNumInvTab) {
		if (numInTab > 0) {
			anyItems = true;
			break;
		}
	}
	if (!anyItems) {
		// Nothing stored in any extra tab right now. This sub-file is positional/count-based
		// and trusted wholesale on load - a stale "heroinvtabs" entry left over from an
		// earlier save (when a tab still had something in it) would resurrect that old
		// content on the next load, even though the tabs are genuinely empty now. MPQ
		// archives are updated in place, not rewritten from scratch each save, so skipping
		// the write here would leave that stale entry behind; it must be explicitly removed
		// instead.
		saveWriter.RemoveHashEntry("heroinvtabs");
		return;
	}

	const size_t itemSize = (gbIsHellfire ? HellfireItemSaveSize : DiabloItemSaveSize);

	size_t bufferSize = 2 * sizeof(uint8_t); // container version + embedded item-format version
	for (int numInTab : player._pNumInvTab)
		bufferSize += InventoryGridCells * sizeof(int8_t) + sizeof(uint8_t) + itemSize * static_cast<size_t>(numInTab);

	SaveHelper file(saveWriter, "heroinvtabs", bufferSize);
	file.WriteLE<uint8_t>(OracoolInvTabsVersion);
	// The item schema these records were written with, checked on load - see the version-3 note at
	// OracoolInvTabsVersion. Bumping OracoolItemFormatVersion now orphans this file by itself.
	file.WriteLE<uint8_t>(OracoolItemFormatVersion);
	for (int t = 0; t < Player::NumExtraInventoryTabs; t++) {
		for (int8_t cell : player.InvTabGrid[t])
			file.WriteLE<int8_t>(cell);
		file.WriteLE<uint8_t>(static_cast<uint8_t>(player._pNumInvTab[t]));
		for (int i = 0; i < player._pNumInvTab[t]; i++)
			SaveItem(file, player.InvTabList[t][i]);
	}
}

void SaveGameData(SaveWriter &saveWriter)
{
	SaveHelper file(saveWriter, "game", 320 * 1024);

	if (gbIsSpawn && !gbIsHellfire)
		file.WriteLE<uint32_t>(LoadLE32("SHAR"));
	else if (gbIsSpawn && gbIsHellfire)
		file.WriteLE<uint32_t>(LoadLE32("SHLF"));
	else if (!gbIsSpawn && gbIsHellfire)
		file.WriteLE<uint32_t>(LoadLE32("HELF"));
	else if (!gbIsSpawn && !gbIsHellfire)
		file.WriteLE<uint32_t>(LoadLE32("RETL"));
	else
		app_fatal(_("Invalid game state"));

	if (gbIsHellfire) {
		giNumberOfLevels = 25;
		giNumberQuests = 24;
	} else {
		giNumberOfLevels = 17;
		giNumberQuests = 16;
	}
	// Must match the load path's width exactly - see the note there.
	giNumberOfSmithPremiumItems = SMITH_PREMIUM_ITEMS;

	file.WriteLE<uint8_t>(setlevel ? 1 : 0);
	file.WriteBE<uint32_t>(setlvlnum);
	file.WriteBE<uint32_t>(currlevel);
	file.WriteBE<uint32_t>(getHellfireLevelType(leveltype));
	file.WriteBE<int32_t>(ViewPosition.x);
	file.WriteBE<int32_t>(ViewPosition.y);
	file.WriteLE<uint8_t>(invflag ? 1 : 0);
	file.WriteLE<uint8_t>(chrflag ? 1 : 0);
	file.WriteBE<int32_t>(ActiveMonsterCount);
	file.WriteBE<int32_t>(ActiveItemCount);
	// ActiveMissileCount will be a value from 0-125 (for vanilla compatibility). Writing an unsigned value here to avoid
	// warnings about casting from unsigned to signed, but there's no sign extension issues when reading this as a signed
	// value later so it doesn't have to match in LoadGameData().
	file.WriteBE<uint32_t>(static_cast<uint32_t>(std::min(Missiles.size(), MaxMissilesForSaveGame)));
	file.WriteBE<int32_t>(ActiveObjectCount);

	for (uint8_t i = 0; i < giNumberOfLevels; i++) {
		file.WriteBE<uint32_t>(glSeedTbl[i]);
		file.WriteBE<int32_t>(getHellfireLevelType(GetLevelType(i)));
	}

	Player &myPlayer = *MyPlayer;
	SavePlayer(file, myPlayer);

	for (int i = 0; i < giNumberQuests; i++)
		SaveQuest(&file, i);
	for (int i = 0; i < MAXPORTAL; i++)
		SavePortal(&file, i);
	for (int monstkill : MonsterKillCounts)
		file.WriteBE<int32_t>(monstkill);
	// add padding for vanilla save compatibility (Related to bugfix where MonsterKillCounts[MaxMonsters] was changed to MonsterKillCounts[NUM_MTYPES]
	file.Skip(4 * (MaxMonsters - NUM_MTYPES));

	if (leveltype != DTYPE_TOWN) {
		for (int monsterId : ActiveMonsters)
			file.WriteBE<int32_t>(monsterId);
		for (size_t i = 0; i < ActiveMonsterCount; i++)
			SaveMonster(&file, Monsters[ActiveMonsters[i]]);
		// Write ActiveMissiles
		for (uint8_t activeMissile = 0; activeMissile < MaxMissilesForSaveGame; activeMissile++)
			file.WriteLE<uint8_t>(activeMissile);
		// Write AvailableMissiles
		for (size_t availableMissiles = Missiles.size(); availableMissiles < MaxMissilesForSaveGame; availableMissiles++)
			file.WriteLE(static_cast<uint8_t>(availableMissiles));
		const size_t savedMissiles = std::min(Missiles.size(), MaxMissilesForSaveGame);
		file.Skip<uint8_t>(savedMissiles);
		// Write Missile Data
		{
			auto missilesEnd = Missiles.cbegin();
			std::advance(missilesEnd, savedMissiles);
			for (auto it = Missiles.cbegin(); it != missilesEnd; it++) {
				SaveMissile(&file, *it);
			}
		}
		for (int objectId : ActiveObjects)
			file.WriteLE(static_cast<int8_t>(objectId));
		for (int objectId : AvailableObjects)
			file.WriteLE(static_cast<int8_t>(objectId));
		for (int i = 0; i < ActiveObjectCount; i++)
			SaveObject(file, Objects[ActiveObjects[i]]);

		file.WriteBE<int32_t>(ActiveLightCount);

		for (uint8_t lightId : ActiveLights)
			file.WriteLE<uint8_t>(lightId);
		for (int i = 0; i < ActiveLightCount; i++)
			SaveLighting(&file, &Lights[ActiveLights[i]]);

		int visionCount = Players.size();
		file.WriteBE<int32_t>(visionCount + 1); // VisionId
		file.WriteBE<int32_t>(visionCount);

		for (const Player &player : Players)
			SaveLighting(&file, &VisionList[player.getId()], true);
	}

	auto itemIndexes = SaveDroppedItems(file);

	for (bool uniqueItemFlag : UniqueItemFlags)
		file.WriteLE<uint8_t>(uniqueItemFlag ? 1 : 0);

	for (int j = 0; j < MAXDUNY; j++) {
		for (int i = 0; i < MAXDUNX; i++) // NOLINT(modernize-loop-convert)
			file.WriteLE<uint8_t>(dLight[i][j]);
	}
	for (int j = 0; j < MAXDUNY; j++) {
		for (int i = 0; i < MAXDUNX; i++) // NOLINT(modernize-loop-convert)
			file.WriteLE<uint8_t>(static_cast<uint8_t>(dFlags[i][j] & DungeonFlag::SavedFlags));
	}
	for (int j = 0; j < MAXDUNY; j++) {
		for (int i = 0; i < MAXDUNX; i++) // NOLINT(modernize-loop-convert)
			file.WriteLE<int8_t>(dPlayer[i][j]);
	}

	SaveDroppedItemLocations(file, itemIndexes);

	if (leveltype != DTYPE_TOWN) {
		for (int j = 0; j < MAXDUNY; j++) {
			for (int i = 0; i < MAXDUNX; i++) // NOLINT(modernize-loop-convert)
				file.WriteBE<int32_t>(dMonster[i][j]);
		}
		for (int j = 0; j < MAXDUNY; j++) {
			for (int i = 0; i < MAXDUNX; i++) // NOLINT(modernize-loop-convert)
				file.WriteLE<int8_t>(dCorpse[i][j]);
		}
		for (int j = 0; j < MAXDUNY; j++) {
			for (int i = 0; i < MAXDUNX; i++) // NOLINT(modernize-loop-convert)
				file.WriteLE<int8_t>(dObject[i][j]);
		}
		for (int j = 0; j < MAXDUNY; j++) {
			for (int i = 0; i < MAXDUNX; i++)        // NOLINT(modernize-loop-convert)
				file.WriteLE<uint8_t>(dLight[i][j]); // BUGFIX: dLight got saved already
		}
		for (int j = 0; j < MAXDUNY; j++) {
			for (int i = 0; i < MAXDUNX; i++) // NOLINT(modernize-loop-convert)
				file.WriteLE<uint8_t>(dPreLight[i][j]);
		}
		for (int j = 0; j < DMAXY; j++) {
			for (int i = 0; i < DMAXX; i++) // NOLINT(modernize-loop-convert)
				file.WriteLE<uint8_t>(AutomapView[i][j]);
		}
		for (int j = 0; j < MAXDUNY; j++) {
			for (int i = 0; i < MAXDUNX; i++)                                 // NOLINT(modernize-loop-convert)
				file.WriteLE<int8_t>(TileContainsMissile({ i, j }) ? -1 : 0); // For backwards compatability
		}
	}

	file.WriteBE<int32_t>(numpremium);
	file.WriteBE<int32_t>(premiumlevel);

	for (int i = 0; i < giNumberOfSmithPremiumItems; i++)
		SaveItem(file, premiumitems[i]);

	file.WriteLE<uint8_t>(AutomapActive ? 1 : 0);
	file.WriteBE<int32_t>(AutoMapScale);

	SaveAdditionalMissiles(saveWriter);
}

void SaveGame()
{
	gbValidSaveFile = true;
	SaveHeroAndStash(/*writeGameData=*/true);
	oracool::NotifyGameSaved();
}

void SaveLevel(SaveWriter &saveWriter)
{
	SaveLevel(saveWriter, nullptr);
}

void LoadLevel()
{
	LoadLevel(nullptr);
}

} // namespace devilution
