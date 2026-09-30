/**
 * @file player.h
 *
 * Interface of player functionality, leveling, actions, creation, loading, etc.
 */
#pragma once

#include <cstdint>
#include <vector>

#include <algorithm>
#include <array>
#include <memory>

#include "diablo.h"
#include "engine.h"
#include "engine/actor_position.hpp"
#include "engine/animationinfo.h"
#include "engine/clx_sprite.hpp"
#include "engine/path.h"
#include "engine/point.hpp"
#include "interfac.h"
#include "items.h"
#include "items/validation.h"
#include "levels/gendung.h"
#include "multi.h"
#include "spelldat.h"
#include "utils/attributes.h"
#include "utils/enum_traits.h"
#include "utils/stdcompat/algorithm.hpp"

namespace devilution {

namespace oracool {
struct SpriteColours;
} // namespace oracool

struct Player;
struct Monster; // PlayerStrikesMonster
namespace oracool {
/**
 * @brief Heavenly Strength's grip (2026-09-11): whether @p player holds @p item, a two-handed weapon, in ONE
 * hand - the Paladin's passive slotted. Defined in oracool/class_tree.cpp; asked by GetItemLocation.
 */
bool HeavenlyStrengthGrips(const Player &player, const Item &item);
/** @brief Heavenly Strength's price (dev note, 2026-09-27): -20% damage, to hit and attack speed while the grip is used. */
constexpr int HeavenlyStrengthPenaltyPercent = 20;
/** @brief Whether @p player is using the grip right now: a two-hander held in one hand beside a shield. */
bool HeavenlyStrengthInUse(const Player &player);
/** @brief Extra ticks a swing takes under Heavenly Strength: 20% slower is a quarter more of the attack's frames. */
int HeavenlyStrengthSwingDelayFrames(const Player &player);
/**
 * @brief Parts a two-handed weapon from whatever shares its hands when the rules say it needs both - the
 * other item to the backpack, or the ground at the hero's feet (2026-09-11). Run on every level load: the
 * bow beside a shield that three builds allowed is not a pair a hero keeps once bows need both hands again.
 */
void EnforceTwoHandedGrip(Player &player);
} // namespace oracool

/**
 * @brief Backpack capacity, one entry per grid cell.
 *
 * Oracool V1: raised from vanilla's 40 (10x4) to 70 (10x7) with the new 320x660 inventory
 * window - see oracool/inventory_layout.h. This is a SAVE-BREAKING change: PlayerPack embeds
 * InvList and InvGrid at this size and pfile.cpp's ReadHero validates it with a strict
 * sizeof(), so pre-1.0.85 characters are rejected rather than silently misread. V1 always
 * starts a New Game, so there is nothing to migrate.
 */
constexpr int InventoryGridCells = 70;
constexpr int MaxBeltItems = 8;
constexpr int MaxResistance = 75;
/** @brief Oracool: raised from vanilla's 50 to allow post-Hell/Torment progression. */
constexpr int MaxCharacterLevel = 99;
// THIRTY (user, 2026-08-31), down from 98 and up from vanilla's 15.
//
// 98 was exactly the pool a character can earn, so it said "everything you have" rather than being a
// ceiling - and ScaleSpellEffect grows by 9/8 PER LEVEL, so a spell at 98 dealt around ten million
// damage against monsters with hit points in the thousands. The curve was written for vanilla's 15.
//
// Books share the cap with invested points (user's call, same day): one number governs how deep any
// one ability goes, whoever taught it. Books past the cap are vendor trash, which is the ordinary
// shape of this genre and is what keeps the curve bounded. See oracool/skill_points.h.
constexpr uint8_t MaxSpellLevel = 30;
constexpr int PlayerNameLength = 32;

constexpr size_t NumHotkeys = 12;
constexpr int BaseHitChance = 50;
/**
 * @brief Oracool EXPERIMENT (user, 2026-09-11: "Let's switch to D2 style for a while. I want to see how
 * overpower this would make sorcerers"): a player's spell never misses a monster, as in Diablo II. Arrows
 * and other weapon missiles keep their roll against armour. Set false for Diablo's own roll, Magic + 50
 * (+20 Sorcerer, +10 Bard) - 2 x monster level - distance, kept to 5-95%.
 */
constexpr bool SpellsNeverMiss = true;

/** Walking directions */
enum {
	// clang-format off
	WALK_NE   =  1,
	WALK_NW   =  2,
	WALK_SE   =  3,
	WALK_SW   =  4,
	WALK_N    =  5,
	WALK_E    =  6,
	WALK_S    =  7,
	WALK_W    =  8,
	WALK_NONE = -1,
	// clang-format on
};

enum class HeroClass : uint8_t {
	Warrior,
	Rogue,
	Sorcerer,
	Monk,
	Bard,
	Barbarian,
	/** Oracool, 2026-09-17: a class of his own (D1), on the Sorcerer's body (D2). The Bard keeps slot 4, hidden. */
	Necromancer,

	LAST = Necromancer
};

enum class CharacterAttribute : uint8_t {
	Strength,
	Magic,
	Dexterity,
	Vitality,

	FIRST = Strength,
	LAST = Vitality
};

// Logical equipment locations
//
// Oracool: user request - six worn slots added after the original seven. SAVE-BREAKING: PlayerPack
// embeds InvBody[NUM_INVLOC] at a fixed size and pfile.cpp gates loading on an exact sizeof(), so
// an old save is rejected cleanly rather than misread. Acceptable under V1's always-new-game rule.
//
// Appended rather than inserted so the existing seven keep their numbers - the same reasoning as
// ItemType's new values. Anything that loops 0..NUM_INVLOC (CalcPlrItemVals via
// EquippedPlayerItemsRange, durability, the save round-trip) picks the new slots up automatically;
// anything that names a slot explicitly - notably the armour graphic, which reads INVLOC_CHEST and
// only INVLOC_CHEST - keeps working unchanged. That is deliberate: the new slots must not require
// new player animations.
enum inv_body_loc : uint8_t {
	INVLOC_HEAD,
	INVLOC_RING_LEFT,
	INVLOC_RING_RIGHT,
	INVLOC_AMULET,
	INVLOC_HAND_LEFT,
	INVLOC_HAND_RIGHT,
	INVLOC_CHEST,
	INVLOC_SHOULDERS,
	INVLOC_BRACERS,
	INVLOC_GLOVES,
	INVLOC_WAIST,
	INVLOC_LEGS,
	INVLOC_BOOTS,
	NUM_INVLOC,
};

/** @brief Whether @p loc is one of the six worn slots Oracool added. */
constexpr bool IsOracoolBodyLocation(inv_body_loc loc)
{
	return loc >= INVLOC_SHOULDERS && loc <= INVLOC_BOOTS;
}

enum class player_graphic : uint8_t {
	Stand,
	Walk,
	Attack,
	Hit,
	Lightning,
	Fire,
	Magic,
	Death,
	Block,
	/**
	 * @brief The unarmed-with-shield attack sheet ("u" + "at"), whatever is in hand: Shield Bash and Aegis Slam strike
	 * with the shield (dev note, 2026-09-27). Loaded for the Paladin only; the armour tier follows what is worn.
	 */
	ShieldAttack,

	LAST = ShieldAttack
};

enum class PlayerWeaponGraphic : uint8_t {
	Unarmed,
	UnarmedShield,
	Sword,
	SwordShield,
	Bow,
	Axe,
	Mace,
	MaceShield,
	Staff,
};

namespace oracool {
/**
 * @brief The weapon graphic whose BLOCK sheet stands in for @p weapon's when the archive has none (2026-09-11):
 * the shield-carrying sheet nearest it - the axe, the staff and the mace the mace-and-shield's, the sword the
 * sword-and-shield's, the bow and the empty hand the empty-hand-and-shield's. A shield graphic is its own.
 */
PlayerWeaponGraphic BlockSheetFallback(PlayerWeaponGraphic weapon);
} // namespace oracool

enum PLR_MODE : uint8_t {
	PM_STAND,
	PM_WALK_NORTHWARDS,
	PM_WALK_SOUTHWARDS,
	PM_WALK_SIDEWAYS,
	PM_ATTACK,
	PM_RATTACK,
	PM_BLOCK,
	PM_GOTHIT,
	PM_DEATH,
	PM_SPELL,
	PM_NEWLVL,
	PM_QUIT,
};

enum action_id : int8_t {
	// clang-format off
	ACTION_WALK        = -2, // Automatic walk when using gamepad
	ACTION_NONE        = -1,
	ACTION_ATTACK      = 9,
	ACTION_RATTACK     = 10,
	ACTION_SPELL       = 12,
	ACTION_OPERATE     = 13,
	ACTION_DISARM      = 14,
	ACTION_PICKUPITEM  = 15, // put item in hand (inventory screen open)
	ACTION_PICKUPAITEM = 16, // put item in inventory
	ACTION_TALK        = 17,
	ACTION_OPERATETK   = 18, // operate via telekinesis
	ACTION_ATTACKMON   = 20,
	ACTION_ATTACKPLR   = 21,
	ACTION_RATTACKMON  = 22,
	ACTION_RATTACKPLR  = 23,
	ACTION_SPELLMON    = 24,
	ACTION_SPELLPLR    = 25,
	ACTION_SPELLWALL   = 26,
	// clang-format on
};

enum class SpellFlag : uint8_t {
	// clang-format off
	None         = 0,
	Etherealize  = 1 << 0,
	RageActive   = 1 << 1,
	RageCooldown = 1 << 2,
	// bits 3-7 are unused
	// clang-format on
};
use_enum_as_flags(SpellFlag);

/* @brief When the player dies, what is the reason/source why? */
enum class DeathReason {
	/* @brief Monster or Trap (dungeon) */
	MonsterOrTrap,
	/* @brief Other player or selfkill (for example firewall) */
	Player,
	/* @brief HP is zero but we don't know when or where this happend */
	Unknown,
};

/** Maps from armor animation to letter used in graphic files. */
constexpr std::array<char, 4> ArmourChar = {
	'l', // light
	'm', // medium
	'h', // heavy
};
/** Maps from weapon animation to letter used in graphic files. */
constexpr std::array<char, 9> WepChar = {
	'n', // unarmed
	'u', // no weapon + shield
	's', // sword + no shield
	'd', // sword + shield
	'b', // bow
	'a', // axe
	'm', // blunt + no shield
	'h', // blunt + shield
	't', // staff
};

/** Maps from player class to letter used in graphic files. */
constexpr std::array<char, 7> CharChar = {
	'w', // warrior
	'r', // rogue
	's', // sorcerer
	'm', // monk
	'b',
	'c',
	's', // necromancer: the Sorcerer's sheets (GetPlayerSpriteClass sends him there first; this is the fallback)
};

/**
 * @brief Contains Data (CelSprites) for a player graphic (player_graphic)
 */
struct PlayerAnimationData {
	/**
	 * @brief Sprite lists for each of the 8 directions.
	 */
	OptionalOwnedClxSpriteSheet sprites;

	/**
	 * @brief Oracool: what this sheet's indices mean as colours, or null for "the level palette, as always".
	 * Set by LoadPlrGFX for a dyed class or an imported PNG sheet; drawn through by DrawPlayer. See
	 * oracool/sprite_colours.h.
	 */
	std::shared_ptr<const oracool::SpriteColours> colours;

	/**
	 * @brief Oracool: the key of the mixed sheet being built in the background for this animation, or empty.
	 * The plain sheet is worn meanwhile and swapped when it arrives - see PumpPlayerSheetMixer.
	 */
	std::string pendingSheetKey;

	/**
	 * @brief The sprite list for @p direction, or nullopt when this animation was never loaded.
	 *
	 * Oracool audit (2026-08-16): this used to dereference `sprites` unconditionally, while its
	 * exact twin for monsters - AnimStruct::spritesForDirection in monster.h - returns nullopt.
	 * The two were written to different standards and the player's was the wrong one, because
	 * LoadPlrGFX declines four graphics outright (Attack and Hit in town, Block without the block
	 * flag, and - until v1.7.21 - Death with a weapon held), so an unloaded animation is a REACHABLE
	 * state rather than a programming error.
	 *
	 * This is the deref behind the v1.6.24 crash. That fix removed one path to it (Shield Bash
	 * selecting the block graphic without the sheet); the deref itself stayed, and callers that
	 * checked for an empty result were checking downstream of an assert that had already fired.
	 */
	[[nodiscard]] OptionalClxSpriteList spritesForDirection(Direction direction) const
	{
		if (!sprites)
			return std::nullopt;
		return (*sprites)[static_cast<size_t>(direction)];
	}
};

struct SpellCastInfo {
	SpellID spellId;
	SpellType spellType;
	/* @brief Inventory location for scrolls */
	int8_t spellFrom;
	/* @brief Used for spell level */
	int spellLevel;
};

struct Player {
	Player() = default;
	Player(Player &&) noexcept = default;
	Player &operator=(Player &&) noexcept = default;

	char _pName[PlayerNameLength];
	Item InvBody[NUM_INVLOC];
	Item InvList[InventoryGridCells];
	Item SpdList[MaxBeltItems];
	Item HoldItem;

	/**
	 * @brief Oracool Tabbed Inventory: 9 additional backpack pages beyond the original InvList
	 * (tab 1), each the same size and shape as InvList/InvGrid. Deliberately not part of the
	 * on-disk PlayerPack/ItemNetPack save structs (those are validated with a strict sizeof()
	 * check, so any resize breaks every existing save) - persisted instead through a separate,
	 * skip-if-empty save sub-file, the same pattern already used for tiered-item extension data.
	 */
	static constexpr int NumExtraInventoryTabs = 9;
	std::array<std::array<Item, InventoryGridCells>, NumExtraInventoryTabs> InvTabList {};
	std::array<std::array<int8_t, InventoryGridCells>, NumExtraInventoryTabs> InvTabGrid {};
	std::array<int, NumExtraInventoryTabs> _pNumInvTab {};

	int lightId;

	int _pNumInv;
	int _pStrength;
	int _pBaseStr;
	int _pMagic;
	int _pBaseMag;
	int _pDexterity;
	int _pBaseDex;
	int _pVitality;
	int _pBaseVit;
	int _pStatPts;
	/**
	 * @brief Oracool Reset Stats: points manually allocated to each attribute via the character
	 * panel's "+" buttons (CMD_ADDSTR/MAG/DEX/VIT), tracked separately from _pBaseStr/Mag/Dex/Vit
	 * so ResetPlayerStats() can undo exactly the player's own spending without touching permanent
	 * bonuses granted by quests, shrines, or items (which modify the same base stats through a
	 * different call path and must survive a reset).
	 */
	int _pStatPtsSpentStr = 0;
	int _pStatPtsSpentMag = 0;
	int _pStatPtsSpentDex = 0;
	int _pStatPtsSpentVit = 0;
	int _pDamageMod;
	int _pBaseToBlk;
	int _pHPBase;
	int _pMaxHPBase;
	int _pHitPoints;
	int _pMaxHP;
	int _pHPPer;
	int _pManaBase;
	int _pMaxManaBase;
	int _pMana;
	int _pMaxMana;
	int _pManaPer;
	/** The Barbarian's Rage, whole points - see oracool/rage.h. Transient: every level starts at 0. */
	int _pRage;
	/** Ticks since Rage was last gained or spent; the out-of-combat drain starts from it. */
	int _pRageIdleTicks;
	/** The Necromancer's Essence in 1/64 points - see oracool/essence.h. Transient: a hero enters the game with none. */
	int _pEssence;
	int _pIMinDam;
	int _pIMaxDam;
	int _pIAC;
	int _pIBonusDam;
	int _pIBonusToHit;
	int _pIBonusAC;
	int _pIBonusDamMod;
	int _pIGetHit;
	int _pIEnAc;
	int _pIFMinDam;
	int _pIFMaxDam;
	int _pILMinDam;
	int _pILMaxDam;
	/** @brief Oracool: widened to uint64_t - the extended level-99 curve exceeds UINT32_MAX. */
	uint64_t _pExperience;
	uint64_t _pNextExper;
	PLR_MODE _pmode;
	int8_t walkpath[MaxPathLength];
	bool plractive;
	action_id destAction;
	int destParam1;
	int destParam2;
	int destParam3;
	int destParam4;
	int _pGold;

	/**
	 * @brief Contains Information for current Animation
	 */
	AnimationInfo AnimInfo;
	/**
	 * @brief Contains a optional preview ClxSprite that is displayed until the current command is handled by the game logic
	 */
	OptionalClxSprite previewCelSprite;
	/**
	 * @brief Contains the progress to next game tick when previewCelSprite was set
	 */
	int8_t progressToNextGameTickWhenPreviewWasSet;
	/** @brief Bitmask using item_special_effect */
	ItemSpecialEffect _pIFlags;
	/**
	 * @brief Contains Data (Sprites) for the different Animations
	 */
	std::array<PlayerAnimationData, enum_size<player_graphic>::value> AnimationData;
	int8_t _pNFrames;
	int8_t _pWFrames;
	int8_t _pAFrames;
	int8_t _pAFNum;
	int8_t _pSFrames;
	int8_t _pSFNum;
	int8_t _pHFrames;
	int8_t _pDFrames;
	int8_t _pBFrames;
	int8_t InvGrid[InventoryGridCells];

	uint8_t plrlevel;
	bool plrIsOnSetLevel;
	ActorPosition position;
	Direction _pdir; // Direction faced by player (direction enum)
	HeroClass _pClass;
	int8_t _pLevel;
	int8_t _pMaxLvl;
	/** @brief Oracool: oracool::GearLookCode as of the last sheet load - a look that changes reloads the sheets. Transient. */
	uint8_t _pGearLook = 0;
	uint8_t _pgfxnum; // Bitmask indicating what variant of the sprite the player is using. The 3 lower bits define weapon (PlayerWeaponGraphic) and the higher bits define armour (starting with PlayerArmorGraphic)
	int8_t _pISplLvlAdd;
	/** @brief Specifies whether players are in non-PvP mode. */
	bool friendlyMode = true;

	/** @brief The next queued spell */
	SpellCastInfo queuedSpell;
	/** @brief The spell that is currently being cast */
	SpellCastInfo executedSpell;
	/* @brief Which spell should be executed with CURSOR_TELEPORT */
	SpellID inventorySpell;
	/* @brief Inventory location for scrolls with CURSOR_TELEPORT */
	int8_t spellFrom;
	SpellID _pRSpell;
	SpellType _pRSplType;
	/**
	 * Oracool: user request (2026-08-15) - "All skills and spells must be able to be set on both
	 * places and cast with both mouse buttons." Vanilla has ONE readied spell, the pair above, and
	 * the left button always swings. This is the left button's own pair.
	 *
	 * SpellID::Invalid means "left click attacks", which is vanilla behaviour and the default, so a
	 * player who never assigns anything here notices no change.
	 *
	 * Persisted alongside _pRSpell as of 1.5.59 - see pack.h's pReadiedSpellRight. Neither pair used
	 * to survive a New Game with an existing hero, because neither was in PlayerPack at all.
	 */
	SpellID _pLRSpell;
	SpellType _pLRSplType;
	SpellID _pSBkSpell;
	uint8_t _pSplLvl[64];
	/**
	 * @brief Oracool: Megaplan Phase 2's skill points, persisted from day one via the hero file's
	 * chunk tail (oracool/hero_chunks.h, HeroChunkSkillPoints) so the feature can land without a
	 * save break. Nothing reads these yet; they round-trip through every save until Phase 2 gives
	 * them a UI and rules.
	 */
	uint16_t _pUnspentSkillPoints = 0;
	uint8_t _pSkillInvestment[MAX_SPELLS] = {};
	/**
	 * @brief Oracool: the burning Paladin aura, as the raw oracool::PaladinTreeSkill byte
	 * (0xFF = none) so this header does not need oracool/paladin_tree.h. Persisted via the
	 * HeroChunkActiveAura chunk; applied through the "aura" bonus provider in stat_sheet.cpp.
	 */
	/**
	 * @brief The burning aura as an ABSOLUTE oracool::ClassTreeSkill, or 0xFFFF for none.
	 *
	 * uint16_t since 2026-08-25, when the Passive Skills page took the tree past 255 rows and the
	 * enum itself had to widen. Persisted by HeroChunkActiveAura, which now writes two bytes
	 * little-endian - so an older build, which reads only the first, still recovers any aura below
	 * 256, and every aura row is.
	 *
	 * Absolute, which is the weakness: the value shifts whenever a class earlier in the enum gains
	 * rows. GetActiveClassAura guards it by checking the row belongs to this character's class.
	 */
	uint16_t _pOracoolActiveAura = 0xFFFF;
	/**
	 * @brief F1-F8 bound to AURAS, as absolute oracool::ClassTreeSkill ordinals. 0xFFFF for none.
	 *
	 * A separate array because an aura cannot live in the two SpellID hotkey arrays: aura rows carry
	 * SpellID::Invalid by construction - an aura is a toggle, not a cast, and has no spell slot to
	 * be named by. That is why F-keys refused them (user, 2026-08-31: "i cant set them on auras").
	 *
	 * Same width and same sentinel as _pOracoolActiveAura above, and absolute for the same reason
	 * and with the same weakness: the ordinal shifts if a class earlier in the enum gains rows. The
	 * readers guard it the way GetActiveClassAura does, by checking the row belongs to this
	 * character's class before honouring it.
	 *
	 * Persisted by HeroChunkAuraHotkeys. NumHotkeys, not AbilityFKeyCount, so the array matches its
	 * two SpellID siblings and the extra slots exist if the reserved keys ever widen again.
	 */
	uint16_t _pAuraHotKey[NumHotkeys] = { 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
		0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF };
	/**
	 * @brief Points sunk into each class-tree skill that has no spell slot - every aura, every
	 * passive, and the actives whose mechanics are not built yet - indexed by the skill's position
	 * within its own class (oracool::ClassTreeIconIndex). The tree's castable skills store their
	 * points in _pSkillInvestment instead, keyed by SpellID, so GetSpellLevel picks them up.
	 * Persisted via the HeroChunkClassTree chunk.
	 *
	 * 64, grown from 32 on 2026-08-25 with oracool::MaxSkillsPerClass, which indexes it - the
	 * Passive Skills page took three classes to 49 skills apiece. 32 was itself grown from 30 on
	 * 2026-08-16. Widening it is backward compatible both ways: the chunk carries its own length
	 * byte and ApplyClassTree clamps to the smaller of that and this array, so an old 32-entry tail
	 * loads into the first 32 slots with the rest zeroed, and an older build reading a 64-entry tail
	 * keeps the first 32 and drops the rest.
	 */
	uint8_t _pClassTreeInvestment[96] = {};
	/**
	 * @brief The four Passive Skills slots, as CLASS-RELATIVE skill indices. 0xFF is empty.
	 *
	 * Class-relative deliberately, and not the absolute ClassTreeSkill that _pOracoolActiveAura
	 * stores: an absolute id shifts whenever a class earlier in the enum gains rows, which is
	 * exactly what cost Bard and Monk heroes their lit aura at 1.9.45. Same index space as
	 * _pClassTreeInvestment above, so oracool::ClassTreeIconIndex converts both ways.
	 *
	 * Persisted by HeroChunkPassiveSlots. A slot holding a skill the character cannot have - the
	 * wrong class, or a level they no longer meet - reads as empty rather than as that skill.
	 */
	uint8_t _pPassiveSlots[4] = { 0xFF, 0xFF, 0xFF, 0xFF };
	/** @brief Phase 1 Magic/Gold Find: derived each CalcPlrItemVals from the bonus providers
	 * (charms carry them today), never saved. Consumed by the drop tail in items.cpp. */
	int _pMagicFind = 0;
	int _pGoldFind = 0;
	/** @brief Oracool: Movement Speed +X%, derived by CalcPlrItemVals from the worn affixes and the burning aura (2026-09-07). */
	int _pIMoveSpeed = 0;
	/** @brief Oracool: Faster Cast Rate +X%, derived by CalcPlrItemVals from the worn affixes (2026-09-11). */
	int _pIFastCast = 0;
	/** @brief Bitmask of staff spell */
	SpellMask _pISpells;
	/** @brief Bitmask of learned spells */
	SpellMask _pMemSpells;
	/** @brief Bitmask of abilities */
	SpellMask _pAblSpells;
	/** @brief Bitmask of spells available via scrolls */
	SpellMask _pScrlSpells;
	SpellFlag _pSpellFlags;
	SpellID _pSplHotKey[NumHotkeys];
	SpellType _pSplTHotKey[NumHotkeys];
	/**
	 * Oracool: the LEFT-button half of the same hotkeys (user request, 2026-08-18). F1-F8 bind to
	 * the right button; LShift+F1-F8 bind the same keys to the left. Two arrays rather than one
	 * array of pairs, because the vanilla pair above is load-bearing everywhere ToggleSpell runs
	 * and widening it would have touched every one of those sites to say "the right one".
	 *
	 * Persisted by its own hero chunk, HeroChunkSpellHotkeysLeft - see oracool/hero_chunks.cpp.
	 */
	SpellID _pSplLHotKey[NumHotkeys];
	SpellType _pSplLTHotKey[NumHotkeys];
	bool _pBlockFlag;
	bool _pInvincible;
	int8_t _pLightRad;
	/** @brief True when the player is transitioning between levels */
	bool _pLvlChanging;

	int8_t _pArmorClass;
	int8_t _pMagResist;
	int8_t _pFireResist;
	int8_t _pLghtResist;
	/** Oracool (2026-09-26): cold resistance, a stat of its own as in Diablo II. Cold hits were resisted by magic until then. */
	int8_t _pColdResist = 0;
	bool _pInfraFlag;
	/** Player's direction when ending movement. Also used for casting direction of SpellID::FireWall. */
	Direction tempDirection;

	bool _pLvlVisited[NUMLEVELS];
	bool _pSLvlVisited[NUMLEVELS]; // only 10 used

	/**
	 * @brief Waypoint list slots stored per difficulty: index 0 is Tristram, 1-24 the dungeon
	 * levels; a plain Diablo game simply never sets slots past 16 (oracool::VisibleWaypointCount).
	 *
	 * WIDENED 25 -> 64 for Megaplan Phase 0.2 (2026-08-16): each future dungeon ZONE brings its own
	 * waypoints, and this constant was re-widened twice already (16 -> 24 -> 25). 64 is the full
	 * width of the HeroChunkWaypoints64 mask (oracool/hero_chunks.h), so it never moves again.
	 * PlayerPack's fixed u32 masks keep carrying slots 1-32 for chunkless heroes; the chunk carries
	 * all 63 storable slots.
	 */
	static constexpr size_t MaxWaypointSlots = 64;
	/**
	 * @brief Oracool: user request - per-difficulty waypoint unlock table. Indexed
	 * [difficulty][waypoint list index] (0 = Tristram, 1-24 = that dungeon level, matching
	 * currlevel numbering) - see oracool::IsWaypointUnlocked/UnlockWaypoint
	 * (oracool/waypoint_menu.h), the only readers/writers. Unlocking a waypoint on one difficulty
	 * deliberately doesn't unlock it on another, matching the user's explicit request. Index 0 is
	 * always treated as unlocked regardless of what's stored here - see IsWaypointUnlocked.
	 */
	bool _pWaypointUnlocked[4][MaxWaypointSlots] = {};

	item_misc_id _pOilType;
	uint8_t pTownWarps;
	uint8_t pDungMsgs;
	uint8_t pLvlLoad;
	bool pManaShield;
	uint8_t pDungMsgs2;
	bool pOriginalCathedral;
	uint8_t pDiabloKillLevel;
	uint16_t wReflections;
	ItemSpecialEffectHf pDamAcFlags;

	void CalcScrolls();

	bool CanUseItem(const Item &item) const;

	/**
	 * @brief Remove an item from player inventory
	 * @param iv invList index of item to be removed
	 * @param calcScrolls If true, CalcScrolls() gets called after removing item
	 */
	void RemoveInvItem(int iv, bool calcScrolls = true);

	/**
	 * @brief Returns the network identifier for this player
	 */
	[[nodiscard]] size_t getId() const;

	void RemoveSpdBarItem(int iv);

	/**
	 * @brief Gets the most valuable item out of all the player's items that match the given predicate.
	 * @param itemPredicate The predicate used to match the items.
	 * @return The most valuable item out of all the player's items that match the given predicate, or 'nullptr' in case no
	 * matching items were found.
	 */
	template <typename TPredicate>
	const Item *GetMostValuableItem(const TPredicate &itemPredicate) const
	{
		const auto getMostValuableItem = [&itemPredicate](const Item *begin, const Item *end, const Item *mostValuableItem = nullptr) {
			for (const auto *item = begin; item < end; item++) {
				if (item->isEmpty() || !itemPredicate(*item)) {
					continue;
				}

				if (mostValuableItem == nullptr || item->_iIvalue > mostValuableItem->_iIvalue) {
					mostValuableItem = item;
				}
			}

			return mostValuableItem;
		};

		const Item *mostValuableItem = getMostValuableItem(SpdList, SpdList + MaxBeltItems);
		mostValuableItem = getMostValuableItem(InvBody, InvBody + inv_body_loc::NUM_INVLOC, mostValuableItem);
		mostValuableItem = getMostValuableItem(InvList, InvList + _pNumInv, mostValuableItem);

		return mostValuableItem;
	}

	/**
	 * @brief Gets the base value of the player's specified attribute.
	 * @param attribute The attribute to retrieve the base value for
	 * @return The base value for the requested attribute.
	 */
	int GetBaseAttributeValue(CharacterAttribute attribute) const;

	/**
	 * @brief Gets the current value of the player's specified attribute.
	 * @param attribute The attribute to retrieve the current value for
	 * @return The current value for the requested attribute.
	 */
	int GetCurrentAttributeValue(CharacterAttribute attribute) const;

	/**
	 * @brief Gets the maximum value of the player's specified attribute.
	 * @param attribute The attribute to retrieve the maximum value for
	 * @return The maximum value for the requested attribute.
	 */
	int GetMaximumAttributeValue(CharacterAttribute attribute) const;

	/**
	 * @brief Get the tile coordinates a player is moving to (if not moving, then it corresponds to current position).
	 */
	Point GetTargetPosition() const;

	/**
	 * @brief Check if position is in player's path.
	 */
	bool IsPositionInPath(Point position);

	/**
	 * @brief Says a speech line.
	 * @todo BUGFIX Prevent more than one speech to be played at a time (reject new requests).
	 */
	void Say(HeroSpeech speechId) const;
	/**
	 * @brief Says a speech line after a given delay.
	 * @param speechId The speech ID to say.
	 * @param delay Multiple of 50ms wait before starting the speech
	 */
	void Say(HeroSpeech speechId, int delay) const;
	/**
	 * @brief Says a speech line, without random variants.
	 */
	void SaySpecific(HeroSpeech speechId) const;

	/**
	 * @brief Attempts to stop the player from performing any queued up action. If the player is currently walking, his walking will
	 * stop as soon as he reaches the next tile. If any action was queued with the previous command (like targeting a monster,
	 * opening a chest, picking an item up, etc) this action will also be cancelled.
	 */
	void Stop();

	/**
	 * @brief Is the player currently walking?
	 */
	bool isWalking() const;

	/**
	 * @brief Returns item location taking into consideration barbarian's ability to hold two-handed maces and clubs in one hand.
	 *
	 * Oracool: and the Paladin's Heavenly Strength, which grants it for EVERY two-handed weapon while it is
	 * slotted (2026-09-11).
	 * Every equip rule asks this rather than the item, so the shield slot follows it with no other change.
	 */
	item_equip_type GetItemLocation(const Item &item) const
	{
		if (_pClass == HeroClass::Barbarian && item._iLoc == ILOC_TWOHAND && IsAnyOf(item._itype, ItemType::Sword, ItemType::Mace))
			return ILOC_ONEHAND;
		if (oracool::HeavenlyStrengthGrips(*this, item))
			return ILOC_ONEHAND;
		return item._iLoc;
	}

	/**
	 * @brief Return player's armor value
	 */
	int GetArmor() const
	{
		return _pIBonusAC + _pIAC + _pDexterity / 5;
	}

	/**
	 * @brief Return player's melee to hit value
	 */
	int GetMeleeToHit() const
	{
		int hper = _pLevel + _pDexterity / 2 + _pIBonusToHit + BaseHitChance;
		if (_pClass == HeroClass::Warrior)
			hper += 20;
		return hper;
	}

	/**
	 * @brief Return player's melee to hit value, including armor piercing
	 */
	int GetMeleePiercingToHit() const
	{
		int hper = GetMeleeToHit();
		// in hellfire armor piercing ignores % of enemy armor instead, no way to include it here
		if (!gbIsHellfire)
			hper += _pIEnAc;
		return hper;
	}

	/**
	 * @brief Return player's ranged to hit value
	 */
	int GetRangedToHit() const
	{
		int hper = _pLevel + _pDexterity + _pIBonusToHit + BaseHitChance;
		if (_pClass == HeroClass::Rogue)
			hper += 20;
		else if (_pClass == HeroClass::Warrior || _pClass == HeroClass::Bard)
			hper += 10;
		return hper;
	}

	int GetRangedPiercingToHit() const
	{
		int hper = GetRangedToHit();
		// in hellfire armor piercing ignores % of enemy armor instead, no way to include it here
		if (!gbIsHellfire)
			hper += _pIEnAc;
		return hper;
	}

	/**
	 * @brief Return magic hit chance
	 */
	int GetMagicToHit() const
	{
		int hper = _pMagic + BaseHitChance;
		if (IsAnyOf(_pClass, HeroClass::Sorcerer, HeroClass::Necromancer))
			hper += 20;
		else if (_pClass == HeroClass::Bard)
			hper += 10;
		return hper;
	}

	/**
	 * @brief Return block chance
	 * @param useLevel - indicate if player's level should be added to block chance (the only case where it isn't is blocking a trap)
	 */
	int GetBlockChance(bool useLevel = true) const
	{
		int blkper = _pDexterity + _pBaseToBlk;
		if (useLevel)
			blkper += _pLevel * 2;
		return blkper;
	}

	/**
	 * @brief Return reciprocal of the factor for calculating damage reduction due to Mana Shield.
	 *
	 * Valid only for players with Mana Shield spell level greater than zero.
	 */
	int GetManaShieldDamageReduction();

	/**
	 * @brief Gets the effective spell level for the player, considering item bonuses
	 * @param spell SpellID enum member identifying the spell
	 * @return effective spell level
	 */
	int GetSpellLevel(SpellID spell) const
	{
		// Two stores with two extents: the legacy BOOK levels are 64 wide (a save-format fact), the
		// tree investment is MAX_SPELLS wide. The bound used to be the book array's, so every skill
		// with an id of 64 or more - Frozen Orb, the cold armours, the Rogue's arrows, the class
		// melee skills, the warcries and songs - reported rank 0 to every mechanic while the tree
		// showed the points spent (external audit, 2026-09-06, P1: SKL-01).
		const std::size_t index = static_cast<std::size_t>(spell);
		if (spell == SpellID::Invalid || index >= MAX_SPELLS) {
			return 0;
		}
		const int bookLevel = index < std::size(_pSplLvl) ? _pSplLvl[index] : 0;

		// Oracool Phase 2.1: invested skill points deepen every ladder through this one seam -
		// anything that already scales with spell level scales with investment automatically.
		return std::max<int>(_pISplLvlAdd + bookLevel + _pSkillInvestment[index], 0);
	}

	/**
	 * @brief Return monster armor value after including player's armor piercing % (hellfire only)
	 * @param monsterArmor - monster armor before applying % armor pierce
	 * @param isMelee - indicates if it's melee or ranged combat
	 */
	int CalculateArmorPierce(int monsterArmor, bool isMelee) const
	{
		int tmac = monsterArmor;
		if (_pIEnAc > 0) {
			if (gbIsHellfire) {
				int pIEnAc = _pIEnAc - 1;
				if (pIEnAc > 0)
					tmac >>= pIEnAc;
				else
					tmac -= tmac / 4;
			}
			if (isMelee && _pClass == HeroClass::Barbarian) {
				tmac -= monsterArmor / 8;
			}
		}
		if (tmac < 0)
			tmac = 0;

		return tmac;
	}

	/**
	 * @brief Calculates the players current Hit Points as a percentage of their max HP and stores it for later reference
	 *
	 * The stored value is unused...
	 * @see _pHPPer
	 * @return The players current hit points as a percentage of their maximum (from 0 to 80%)
	 */
	int UpdateHitPointPercentage()
	{
		if (_pMaxHP <= 0) { // divide by zero guard
			_pHPPer = 0;
		} else {
			// Maximum achievable HP is approximately 1200. Diablo uses fixed point integers where the last 6 bits are
			// fractional values. This means that we will never overflow HP values normally by doing this multiplication
			// as the max value is representable in 17 bits and the multiplication result will be at most 23 bits
			_pHPPer = clamp(_pHitPoints * 80 / _pMaxHP, 0, 80); // hp should never be greater than maxHP but just in case
		}

		return _pHPPer;
	}

	int UpdateManaPercentage()
	{
		if (_pMaxMana <= 0) {
			_pManaPer = 0;
		} else {
			_pManaPer = clamp(_pMana * 80 / _pMaxMana, 0, 80);
		}

		return _pManaPer;
	}

	/**
	 * @brief Restores between 1/8 (inclusive) and 1/4 (exclusive) of the players max HP (further adjusted by class).
	 *
	 * This determines a random amount of non-fractional life points to restore then scales the value based on the
	 *  player class. Warriors/barbarians get between 1/4 and 1/2 life restored per potion, rogue/monk/bard get 3/16
	 *  to 3/8, and sorcerers get the base amount.
	 */
	void RestorePartialLife();

	/**
	 * @brief Oracool: the same random amount RestorePartialLife() would apply, without applying
	 * it - lets Gradual Healing queue the amount for a gradual drip instead of an instant grant.
	 */
	int CalcPartialLifeRestoreAmount() const;

	/**
	 * @brief Resets hp to maxHp
	 */
	void RestoreFullLife()
	{
		_pHitPoints = _pMaxHP;
		_pHPBase = _pMaxHPBase;
	}

	/**
	 * @brief Restores between 1/8 (inclusive) and 1/4 (exclusive) of the players max Mana (further adjusted by class).
	 *
	 * This determines a random amount of non-fractional mana points to restore then scales the value based on the
	 *  player class. Sorcerers get between 1/4 and 1/2 mana restored per potion, rogue/monk/bard get 3/16 to 3/8,
	 *  and warrior/barbarian get the base amount. However if the player can't use magic due to an equipped item then
	 *  they get nothing.
	 */
	void RestorePartialMana();

	/**
	 * @brief Oracool: the same random amount RestorePartialMana() would apply, without applying
	 * it - lets Gradual Healing queue the amount for a gradual drip instead of an instant grant.
	 */
	int CalcPartialManaRestoreAmount() const;

	/**
	 * @brief Resets mana to maxMana (if the player can use magic)
	 */
	void RestoreFullMana()
	{
		if (HasNoneOf(_pIFlags, ItemSpecialEffect::NoMana)) {
			_pMana = _pMaxMana;
			_pManaBase = _pMaxManaBase;
		}
	}
	/**
	 * @brief Sets the readied spell to the spell in the specified equipment slot. Does nothing if the item does not have a valid spell.
	 * @param bodyLocation - the body location whose item will be checked for the spell.
	 * @param forceSpell - if true, always change active spell, if false, only when current spell slot is empty
	 */
	void ReadySpellFromEquipment(inv_body_loc bodyLocation, bool forceSpell);

	/**
	 * @brief Does the player currently have a ranged weapon equipped?
	 */
	bool UsesRangedWeapon() const
	{
		return static_cast<PlayerWeaponGraphic>(_pgfxnum & 0xF) == PlayerWeaponGraphic::Bow;
	}

	bool CanChangeAction()
	{
		if (_pmode == PM_STAND)
			return true;
		if (_pmode == PM_ATTACK && AnimInfo.currentFrame >= _pAFNum)
			return true;
		if (_pmode == PM_RATTACK && AnimInfo.currentFrame >= _pAFNum)
			return true;
		if (_pmode == PM_SPELL && AnimInfo.currentFrame >= _pSFNum)
			return true;
		if (isWalking() && AnimInfo.isLastFrame())
			return true;
		return false;
	}

	[[nodiscard]] player_graphic getGraphic() const;

	[[nodiscard]] uint16_t getSpriteWidth() const;

	void getAnimationFramesAndTicksPerFrame(player_graphic graphics, int8_t &numberOfFrames, int8_t &ticksPerFrame) const;

	/**
	 * @brief Updates previewCelSprite according to new requested command
	 * @param cmdId What command is requested
	 * @param point Point for the command
	 * @param wParam1 First Parameter
	 * @param wParam2 Second Parameter
	 */
	void UpdatePreviewCelSprite(_cmd_id cmdId, Point point, uint16_t wParam1, uint16_t wParam2);

	/** @brief Checks if the player is on the same level as the local player (MyPlayer). */
	bool isOnActiveLevel() const
	{
		if (setlevel)
			return isOnLevel(setlvlnum);
		return isOnLevel(currlevel);
	}

	/** @brief Checks if the player is on the corresponding level. */
	bool isOnLevel(uint8_t level) const
	{
		return !this->plrIsOnSetLevel && this->plrlevel == level;
	}
	/** @brief Checks if the player is on the corresponding level. */
	bool isOnLevel(_setlevels level) const
	{
		return this->plrIsOnSetLevel && this->plrlevel == static_cast<uint8_t>(level);
	}
	/** @brief Checks if the player is on a arena level. */
	bool isOnArenaLevel() const
	{
		return plrIsOnSetLevel && IsArenaLevel(static_cast<_setlevels>(plrlevel));
	}
	void setLevel(uint8_t level)
	{
		this->plrlevel = level;
		this->plrIsOnSetLevel = false;
	}
	void setLevel(_setlevels level)
	{
		this->plrlevel = static_cast<uint8_t>(level);
		this->plrIsOnSetLevel = true;
	}

	/** @brief Returns a character's life based on starting life, character level, and base vitality. */
	int32_t calculateBaseLife() const;

	/** @brief Returns a character's mana based on starting mana, character level, and base magic. */
	int32_t calculateBaseMana() const;
};

extern DVL_API_FOR_TEST size_t MyPlayerId;
extern DVL_API_FOR_TEST Player *MyPlayer;
extern DVL_API_FOR_TEST std::vector<Player> Players;
/** @brief What Player items and stats should be displayed? Normally this is identical to MyPlayer but can differ when /inspect was used. */
extern DVL_API_FOR_TEST Player *InspectPlayer;
/** @brief Do we currently inspect a remote player (/inspect was used)? In this case the (remote) players items and stats can't be modified. */
inline bool IsInspectingPlayer()
{
	return MyPlayer != InspectPlayer;
}
extern bool MyPlayerIsDead;

Player *PlayerAtPosition(Point position);

void LoadPlrGFX(Player &player, player_graphic graphic);
void InitPlayerGFX(Player &player);
void ResetPlayerGFX(Player &player);
/** @brief Oracool: asks the background mixer for every animation of the look @p player now wears. */
void PrewarmPlayerLook(Player &player);
/** @brief Oracool: once a game tick - feeds the background mixer and puts finished sheets on the heroes waiting for them. */
void PumpPlayerSheetMixer();

/**
 * @brief Sets the new Player Animation with all relevant information for rendering
 * @param player The player to set the animation for
 * @param graphic What player animation should be displayed
 * @param dir Direction of the animation
 * @param numberOfFrames Number of Frames in Animation
 * @param delayLen Delay after each Animation sequence
 * @param flags Specifies what special logics are applied to this Animation
 * @param numSkippedFrames Number of Frames that will be skipped (for example with modifier "faster attack")
 * @param distributeFramesBeforeFrame Distribute the numSkippedFrames only before this frame
 */
void NewPlrAnim(Player &player, player_graphic graphic, Direction dir, AnimationDistributionFlags flags = AnimationDistributionFlags::None, int8_t numSkippedFrames = 0, int8_t distributeFramesBeforeFrame = 0);
void SetPlrAnims(Player &player);
/**
 * @brief Player::GetManaShieldDamageReduction at an explicit Mana Shield level: the shield takes
 * 1/N off every blow before mana soaks it. The member and the tooltip both read this.
 */
int ManaShieldDamageReductionAtLevel(int spellLevel);
void CreatePlayer(Player &player, HeroClass c);
int CalcStatDiff(Player &player);
#ifdef _DEBUG
void NextPlrLevel(Player &player);
#endif
void AddPlrExperience(Player &player, int lvl, int exp);

/**
 * @brief Oracool: the experience @p player would actually receive for a kill of a monster of level
 * @p monsterLevel worth @p monsterExp - the level-difference clamp and, in multiplayer, the
 * power-levelling cap, exactly as AddPlrExperience applies them. Factored out on 2026-09-07 so the
 * monster health bar's "XP" line and the XP counter quote the number the kill pays, not the raw
 * table value (user: "make sure monster stats are correct ... not some basic stats that are wrong
 * in uniques, minions and lesser uniques cases" - a unique's level is not its kind's, and the clamp
 * runs on the level).
 */
uint64_t KillExperienceFor(const Player &player, int monsterLevel, int monsterExp);
void AddPlrMonstExper(int lvl, int exp, char pmask);
void ApplyPlrDamage(DamageType damageType, Player &player, int dam, int minHP = 0, int frac = 0, DeathReason deathReason = DeathReason::MonsterOrTrap);
void InitPlayer(Player &player, bool FirstTime);
void InitMultiView();
void PlrClrTrans(Point position);
void PlrDoTrans(Point position);
void SetPlayerOld(Player &player);
void FixPlayerLocation(Player &player, Direction bDir);
void StartStand(Player &player, Direction dir);
void StartPlrBlock(Player &player, Direction dir);
void FixPlrWalkTags(const Player &player);
void StartPlrHit(Player &player, int dam, bool forcehit);
void StartPlayerKill(Player &player, DeathReason deathReason);
/**
 * @brief Spends one point of an equipped item's durability, breaking it if that was the last one.
 *
 * The single wear primitive behind DamageWeapon, DamageParryItem and DamageArmor. Declared here
 * rather than kept file-local so the runaway it was written to end can be tested directly.
 *
 * @return true if this call BROKE the item.
 */
bool WearDurabilityPoint(Player &player, inv_body_loc slot);
/**
 * @brief Strip the top off gold piles that are larger than MaxGold
 */
void StripTopGold(Player &player);
void SyncPlrKill(Player &player, DeathReason deathReason);
void RemovePlrMissiles(const Player &player);
void StartNewLvl(Player &player, interface_mode fom, int lvl);
void RestartTownLvl(Player &player);
void StartWarpLvl(Player &player, size_t pidx);
void ProcessPlayers();
void ClrPlrPath(Player &player);
bool PosOkPlayer(const Player &player, Point position);
/**
 * @brief Oracool: one melee blow of @p player at @p monster through the swing's own hit path (to-hit roll, damage, the
 * armed skill's bonus, knockback) without a swing - Whirlwind's spin (oracool/whirlwind.h). True when it landed.
 */
bool PlayerStrikesMonster(Player &player, Monster &monster);
void MakePlrPath(Player &player, Point targetPosition, bool endspace);
void CalcPlrStaff(Player &player);
void CheckPlrSpell(bool isShiftHeld, SpellID spellID = MyPlayer->_pRSpell, SpellType spellType = MyPlayer->_pRSplType);
void SyncPlrAnim(Player &player);
/**
 * @brief Oracool: puts @p item on the ground beside @p player - the nearest free tile, found the way a dying
 * hero's items are (DeadItem). For an item that must leave the hands with nowhere else to go (2026-09-11).
 */
void DropItemBesidePlayer(Player &player, Item item);
void SyncInitPlrPos(Player &player);
void SyncInitPlr(Player &player);
void CheckStats(Player &player);
void ResetPlayerStats(Player &player);
/**
 * @brief Takes up to @p count points back out of @p attribute and returns them to the unspent pool - RESET for one
 * stat, a point at a time (user, 2026-09-26: the grouped sheet's left-pointing triangle, "reduces its designated
 * stat ... allowing fine tuning stats at all times"). Only points the player spent on that stat come back
 * (_pStatPtsSpent*): quest, shrine and elixir gains are never refunded, which is ResetPlayerStats' rule too.
 * Single player, the local hero only. @return how many points came back (0 when there were none to take).
 */
int RefundStatPoints(Player &player, CharacterAttribute attribute, int count);
/**
 * @brief How many of @p requested stat points a + click may put into @p attribute: never more than the hero has
 * unspent (_pStatPts) and never past the base cap of 255. 0 when there is nothing to spend - the grouped sheet's +
 * is always pressable, and a plain click used to spend 1 regardless, taking _pStatPts below zero (user, 2026-09-27
 * dev note: "stat points just increase negativly").
 */
int StatPointsToSpend(const Player &player, CharacterAttribute attribute, int requested);
void ModifyPlrStr(Player &player, int l);
void ModifyPlrMag(Player &player, int l);
void ModifyPlrDex(Player &player, int l);
void ModifyPlrVit(Player &player, int l);
void SetPlayerHitPoints(Player &player, int val);
/**
 * @brief CalcPlrInv for a bonus that ends on its own - a war cry's buff running out, an aura put out by readying another
 * skill. The bonus comes off with its life, as it went on with it, but never below 1 life: at low life the cut took it
 * to 0 and SyncPlrKill killed the hero (round 7 audit, v1.12.232). Keeping the whole life, as it did until v1.12.241,
 * made every off/on a free heal (round 16). Taking an ITEM off keeps vanilla's plain CalcPlrInv.
 */
void CalcPlrInvKeepingLife(Player &player);
void SetPlrStr(Player &player, int v);
void SetPlrMag(Player &player, int v);
void SetPlrDex(Player &player, int v);
void SetPlrVit(Player &player, int v);
void InitDungMsgs(Player &player);
void PlayDungMsgs();

} // namespace devilution
