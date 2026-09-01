/**
 * @file options.cpp
 *
 * Load and save options from the diablo.ini file.
 */

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>

#include <fmt/format.h>

#define SI_NO_CONVERSION
#include <SimpleIni.h>

#include "control.h"
#include "controls/controller.h"
#include "controls/game_controls.h"
#include "controls/plrctrls.h"
#include "discord/discord.h"
#include "engine/demomode.h"
#include "engine/sound_defs.hpp"
#include "hwcursor.hpp"
#include "options.h"
#include "platform/locale.hpp"
#include "qol/monhealthbar.h"
#include "qol/xpbar.h"
#include "utils/display.h"
#include "utils/file_util.h"
#include "utils/language.h"
#include "utils/log.hpp"
#include "utils/paths.h"
#include "utils/stdcompat/algorithm.hpp"
#include "utils/str_cat.hpp"
#include "utils/str_split.hpp"
#include "utils/utf8.hpp"

namespace devilution {

#ifndef DEFAULT_WIDTH
// Oracool: user request - 960x720 is now the supported minimum resolution (see
// CuratedResolutions below), so the default must be one of the curated entries rather than
// the old sub-minimum 900x600. 1280x720 is the most broadly compatible modern default (any
// display that can do 720p or better handles it) while the old default no longer would.
#define DEFAULT_WIDTH 1280
#endif
#ifndef DEFAULT_HEIGHT
#define DEFAULT_HEIGHT 720
#endif
#ifndef DEFAULT_AUDIO_SAMPLE_RATE
#define DEFAULT_AUDIO_SAMPLE_RATE 22050
#endif
#ifndef DEFAULT_AUDIO_CHANNELS
#define DEFAULT_AUDIO_CHANNELS 2
#endif
#ifndef DEFAULT_AUDIO_BUFFER_SIZE
#define DEFAULT_AUDIO_BUFFER_SIZE 2048
#endif
#ifndef DEFAULT_AUDIO_RESAMPLING_QUALITY
#define DEFAULT_AUDIO_RESAMPLING_QUALITY 3
#endif

namespace {

#if defined(__ANDROID__) || (defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE == 1)
constexpr OptionEntryFlags OnlyIfSupportsWindowed = OptionEntryFlags::Invisible;
#else
constexpr OptionEntryFlags OnlyIfSupportsWindowed = OptionEntryFlags::None;
#endif

constexpr size_t NumResamplers =
#ifdef DEVILUTIONX_RESAMPLER_SPEEX
    1 +
#endif
#ifdef DVL_AULIB_SUPPORTS_SDL_RESAMPLER
    1 +
#endif
    0;

std::string GetIniPath()
{
	auto path = paths::ConfigPath() + std::string("diablo.ini");
	return path;
}

CSimpleIni &GetIni()
{
	static CSimpleIni ini;
	static bool isIniLoaded = false;
	if (!isIniLoaded) {
		auto path = GetIniPath();
		FILE *file = OpenFile(path.c_str(), "rb");
		ini.SetSpaces(false);
		ini.SetMultiKey();
		if (file != nullptr) {
			ini.LoadFile(file);
			std::fclose(file);
		}
		isIniLoaded = true;
	}
	return ini;
}

bool IniChanged = false;

/**
 * @brief Checks if a ini entry is changed by comparing value before and after
 */
class IniChangedChecker {
public:
	IniChangedChecker(const char *sectionName, const char *keyName)
	{
		this->sectionName_ = sectionName;
		this->keyName_ = keyName;
		oldValue_ = GetValue();
		if (!oldValue_) {
			// No entry found in original ini => new entry => changed
			IniChanged = true;
		}
	}
	~IniChangedChecker()
	{
		auto newValue = GetValue();
		if (oldValue_ != newValue)
			IniChanged = true;
	}

private:
	std::optional<std::string> GetValue()
	{
		std::list<CSimpleIni::Entry> values;
		if (!GetIni().GetAllValues(sectionName_, keyName_, values))
			return std::nullopt;
		std::string ret;
		for (auto &entry : values) {
			if (entry.pItem != nullptr)
				ret.append(entry.pItem);
			ret.append("\n");
		}
		return ret;
	}

	std::optional<std::string> oldValue_;
	const char *sectionName_;
	const char *keyName_;
};

int GetIniInt(const char *keyname, const char *valuename, int defaultValue)
{
	return GetIni().GetLongValue(keyname, valuename, defaultValue);
}

bool GetIniBool(const char *sectionName, const char *keyName, bool defaultValue)
{
	return GetIni().GetBoolValue(sectionName, keyName, defaultValue);
}

float GetIniFloat(const char *sectionName, const char *keyName, float defaultValue)
{
	return (float)GetIni().GetDoubleValue(sectionName, keyName, defaultValue);
}

bool GetIniValue(string_view sectionName, string_view keyName, char *string, int stringSize, const char *defaultString = "")
{
	std::string sectionNameStr { sectionName };
	std::string keyNameStr { keyName };
	const char *value = GetIni().GetValue(sectionNameStr.c_str(), keyNameStr.c_str());
	if (value == nullptr) {
		CopyUtf8(string, defaultString, stringSize);
		return false;
	}
	CopyUtf8(string, value, stringSize);
	return true;
}

bool GetIniStringVector(const char *sectionName, const char *keyName, std::vector<std::string> &stringValues)
{
	std::list<CSimpleIni::Entry> values;
	if (!GetIni().GetAllValues(sectionName, keyName, values)) {
		return false;
	}
	for (auto &entry : values) {
		stringValues.emplace_back(entry.pItem);
	}
	return true;
}

void SetIniValue(const char *keyname, const char *valuename, int value)
{
	IniChangedChecker changedChecker(keyname, valuename);
	GetIni().SetLongValue(keyname, valuename, value, nullptr, false, true);
}

void SetIniValue(const char *keyname, const char *valuename, bool value)
{
	IniChangedChecker changedChecker(keyname, valuename);
	GetIni().SetLongValue(keyname, valuename, value ? 1 : 0, nullptr, false, true);
}

void SetIniValue(const char *keyname, const char *valuename, float value)
{
	IniChangedChecker changedChecker(keyname, valuename);
	GetIni().SetDoubleValue(keyname, valuename, value, nullptr, true);
}

void SetIniValue(const char *sectionName, const char *keyName, const char *value)
{
	IniChangedChecker changedChecker(sectionName, keyName);
	auto &ini = GetIni();
	ini.SetValue(sectionName, keyName, value, nullptr, true);
}

void SetIniValue(string_view sectionName, string_view keyName, string_view value)
{
	std::string sectionNameStr { sectionName };
	std::string keyNameStr { keyName };
	std::string valueStr { value };
	SetIniValue(sectionNameStr.c_str(), keyNameStr.c_str(), valueStr.c_str());
}

void SetIniValue(const char *keyname, const char *valuename, const std::vector<std::string> &stringValues)
{
	IniChangedChecker changedChecker(keyname, valuename);
	bool firstSet = true;
	for (auto &value : stringValues) {
		GetIni().SetValue(keyname, valuename, value.c_str(), nullptr, firstSet);
		firstSet = false;
	}
	if (firstSet)
		GetIni().SetValue(keyname, valuename, "", nullptr, true);
}

void SaveIni()
{
	if (!IniChanged)
		return;
	RecursivelyCreateDir(paths::ConfigPath().c_str());
	const std::string iniPath = GetIniPath();
	FILE *file = OpenFile(iniPath.c_str(), "wb");
	if (file != nullptr) {
		GetIni().SaveFile(file, true);
		std::fclose(file);
	} else {
		LogError("Failed to write ini file to {}: {}", iniPath, std::strerror(errno));
	}
	IniChanged = false;
}

#if SDL_VERSION_ATLEAST(2, 0, 0)
bool HardwareCursorDefault()
{
#if defined(__ANDROID__) || (defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE == 1)
	// See https://github.com/diasurgical/devilutionX/issues/2502
	return false;
#else
	return HardwareCursorSupported();
#endif
}
#endif

void OptionGrabInputChanged()
{
#ifdef USE_SDL1
	SDL_WM_GrabInput(*sgOptions.Gameplay.grabInput ? SDL_GRAB_ON : SDL_GRAB_OFF);
#else
	if (ghMainWnd != nullptr)
		SDL_SetWindowGrab(ghMainWnd, *sgOptions.Gameplay.grabInput ? SDL_TRUE : SDL_FALSE);
#endif
}

void OptionExperienceBarChanged()
{
	if (!gbRunGame)
		return;
	if (*sgOptions.Gameplay.experienceBar)
		InitXPBar();
	else
		FreeXPBar();
}

void OptionEnemyHealthBarChanged()
{
	if (!gbRunGame)
		return;
	if (*sgOptions.Gameplay.enemyHealthBar)
		InitMonsterHealthBar();
	else
		FreeMonsterHealthBar();
}

#if !defined(USE_SDL1) || defined(__3DS__)
void ResizeWindowAndUpdateResolutionOptions()
{
	ResizeWindow();
#ifndef __3DS__
	sgOptions.Graphics.resolution.InvalidateList();
#endif
}
#endif

void OptionShowFPSChanged()
{
	if (*sgOptions.Graphics.showFPS)
		EnableFrameCount();
	else
		frameflag = false;
}

void OptionLanguageCodeChanged()
{
	UnloadFonts();
	LanguageInitialize();
	LoadLanguageArchive();
}

void OptionGameModeChanged()
{
	gbIsHellfire = *sgOptions.GameMode.gameMode == StartUpGameMode::Hellfire;
	discord_manager::UpdateMenu(true);
}

void OptionSharewareChanged()
{
	gbIsSpawn = *sgOptions.GameMode.shareware;
}

void OptionAudioChanged()
{
	effects_cleanup_sfx();
	music_stop();
	snd_deinit();
	snd_init();
	music_start(TMUSIC_INTRO);
	if (gbRunGame)
		sound_init();
	else
		ui_sound_init();
}

} // namespace

/** Game options */
Options sgOptions;

#if SDL_VERSION_ATLEAST(2, 0, 0)
bool HardwareCursorSupported()
{
#if (defined(TARGET_OS_IPHONE) && TARGET_OS_IPHONE == 1)
	return false;
#else
	SDL_version v;
	SDL_GetVersion(&v);
	return SDL_VERSIONNUM(v.major, v.minor, v.patch) >= SDL_VERSIONNUM(2, 0, 12);
#endif
}
#endif

void LoadOptions()
{
	for (OptionCategoryBase *pCategory : sgOptions.GetCategories()) {
		for (OptionEntryBase *pEntry : pCategory->GetEntries()) {
			pEntry->LoadFromIni(pCategory->GetKey());
		}
	}

	GetIniValue("Hellfire", "SItem", sgOptions.Hellfire.szItem, sizeof(sgOptions.Hellfire.szItem), "");
	GetIniValue("Oracool Edition", "Griswold Refresh Until Item Names", sgOptions.Oracool.refreshUntilItemNames, sizeof(sgOptions.Oracool.refreshUntilItemNames), "");

	GetIniValue("Network", "Bind Address", sgOptions.Network.szBindAddress, sizeof(sgOptions.Network.szBindAddress), "0.0.0.0");
	GetIniValue("Network", "Previous Game ID", sgOptions.Network.szPreviousZTGame, sizeof(sgOptions.Network.szPreviousZTGame), "");
	GetIniValue("Network", "Previous Host", sgOptions.Network.szPreviousHost, sizeof(sgOptions.Network.szPreviousHost), "");

	for (size_t i = 0; i < QUICK_MESSAGE_OPTIONS; i++)
		GetIniStringVector("NetMsg", QuickMessages[i].key, sgOptions.Chat.szHotKeyMsgs[i]);

	GetIniValue("Controller", "Mapping", sgOptions.Controller.szMapping, sizeof(sgOptions.Controller.szMapping), "");
	sgOptions.Controller.fDeadzone = GetIniFloat("Controller", "deadzone", 0.07F);
#ifdef __vita__
	sgOptions.Controller.bRearTouch = GetIniBool("Controller", "Enable Rear Touchpad", true);
#endif

	if (demo::IsRunning())
		demo::OverrideOptions();
}

void SaveOptions()
{
	if (demo::IsRunning())
		return;

	for (OptionCategoryBase *pCategory : sgOptions.GetCategories()) {
		if (pCategory == &sgOptions.Oracool)
			continue;
		for (OptionEntryBase *pEntry : pCategory->GetEntries()) {
			pEntry->SaveToIni(pCategory->GetKey());
		}
	}

	SetIniValue("Hellfire", "SItem", sgOptions.Hellfire.szItem);
	SetIniValue("Network", "Bind Address", sgOptions.Network.szBindAddress);
	SetIniValue("Network", "Previous Game ID", sgOptions.Network.szPreviousZTGame);
	SetIniValue("Network", "Previous Host", sgOptions.Network.szPreviousHost);

	for (size_t i = 0; i < QUICK_MESSAGE_OPTIONS; i++)
		SetIniValue("NetMsg", QuickMessages[i].key, sgOptions.Chat.szHotKeyMsgs[i]);

	SetIniValue("Controller", "Mapping", sgOptions.Controller.szMapping);
	SetIniValue("Controller", "deadzone", sgOptions.Controller.fDeadzone);
#ifdef __vita__
	SetIniValue("Controller", "Enable Rear Touchpad", sgOptions.Controller.bRearTouch);
#endif

	// Keep a canonical, grouped, fully documented Oracool section at the bottom.
	// Entries are alphabetized inside each feature group.
	constexpr const char *Section = "Oracool Edition";
	auto &ini = GetIni();
	ini.Delete(Section, nullptr);
	IniChanged = true;
	auto setBoolean = [&](const char *key, bool value, const char *comment) {
		ini.SetLongValue(Section, key, value ? 1 : 0, comment, false, true);
	};
	auto setInteger = [&](const char *key, int value, const char *comment) {
		ini.SetLongValue(Section, key, value, comment, false, true);
	};
	auto setString = [&](const char *key, const char *value, const char *comment) {
		ini.SetValue(Section, key, value, comment, true);
	};

	setBoolean("Auto Save", *sgOptions.Oracool.autoSave,
	    "; =============================================================================\n; DEVILUTIONX ORACOOL EDITION OPTIONS\n; =============================================================================\n; Boolean options use 0 = Disabled and 1 = Enabled. Unless stated otherwise,\n; these gameplay options affect single-player games only. Restart the game after\n; changing settings. Options are grouped and alphabetized within each group.\n; =============================================================================\n\n; ----- AUTOMATIC SAVING -------------------------------------------------------\n; Enables all Oracool automatic-save triggers in single-player.\n; Set to 0 to disable every automatic save described below.");
	setInteger("Auto Save Interval Minutes", *sgOptions.Oracool.autoSaveIntervalMinutes,
	    "; Time between periodic saves, in minutes.\n; Available values: 1, 2, 3, 5, 10, 15, 30, and 60.");
	setBoolean("Auto Save Notification", *sgOptions.Oracool.autoSaveNotification,
	    "; Displays the normal brief \"Game Saved\" notification after an automatic save.\n; Saving still occurs silently when this setting is disabled.");
	setBoolean("Auto Save on Item Pickup", *sgOptions.Oracool.autoSaveOnItemPickup,
	    "; Schedules a save after successfully picking up an item or gold.\n; Failed pickup attempts do not trigger a save.");
	setBoolean("Auto Save on Level Change", *sgOptions.Oracool.autoSaveOnLevelChange,
	    "; Saves after entering another dungeon level or returning to town.");
	setBoolean("Auto Save on Store Purchase", *sgOptions.Oracool.autoSaveOnStorePurchase,
	    "; Schedules a save after a successful store purchase. Cancelled and failed\n; purchases do not trigger a save.");
	setBoolean("Auto Save on Experience Gain", *sgOptions.Oracool.autoSaveOnExperienceGain,
	    "; Saves instantly whenever your character gains experience, matching Diablo 3's\n; always-persisted progress.");
	setBoolean("Auto Save on Stat Point Spent", *sgOptions.Oracool.autoSaveOnStatPointSpent,
	    "; Saves instantly whenever a stat point is spent on the character panel.");
	setBoolean("Auto Save on Skill Change", *sgOptions.Oracool.autoSaveOnSkillChange,
	    "; Saves instantly whenever a skill point, passive slot, aura or readied skill changes.");
	setBoolean("Auto Save on Equipment Change", *sgOptions.Oracool.autoSaveOnEquipmentChange,
	    "; Saves instantly whenever equipment is worn or removed.");
	setBoolean("Auto Save on Item Drop", *sgOptions.Oracool.autoSaveOnItemDrop,
	    "; Saves instantly whenever an item is dropped on the ground.");
	setBoolean("Auto Save on Stash Change", *sgOptions.Oracool.autoSaveOnStashChange,
	    "; Saves instantly whenever the Stash's contents change: depositing or withdrawing an item\n; or gold, reading/drinking an item straight from the Stash, or sorting it.");
	setBoolean("Auto Save on Store Transaction", *sgOptions.Oracool.autoSaveOnStoreTransaction,
	    "; Saves instantly after selling, repairing, recharging, or identifying an item at a shop.\n; Purchases are covered separately by Auto Save on Store Purchase.");
	setBoolean("Auto Save on Shrine Activation", *sgOptions.Oracool.autoSaveOnShrineActivation,
	    "; Saves instantly after activating a Shrine, Goat Shrine, Cauldron, or Fountain.");
	setBoolean("Auto Save on Book Read", *sgOptions.Oracool.autoSaveOnBookRead,
	    "; Saves instantly after reading a spell book.");
	setBoolean("Auto Save on Item Break", *sgOptions.Oracool.autoSaveOnItemBreak,
	    "; Saves instantly when a weapon or piece of armor breaks (durability reaches 0). Ordinary\n; durability loss from combat does not trigger a save on its own - only the break itself.");
	setBoolean("Auto Save on Waypoint Activation", *sgOptions.Oracool.autoSaveOnWaypointActivation,
	    "; Saves instantly whenever a waypoint sigil's travel menu is opened.");

	setBoolean("Difficulty Level Gate", *sgOptions.Oracool.difficultyLevelGate,
	    "; ----- DIFFICULTY -------------------------------------------------------------\n; Requires a minimum character level to start a single-player game on Nightmare\n; (15), Hell (30), or Torment (40). Disabling this lets any level start any\n; difficulty, matching how Nightmare/Hell already work without this setting.");
	setInteger("Torment Difficulty Multiplier", sgOptions.Oracool.tormentDifficultyMultiplier.ValueTenths(),
	    "; How much harder Torment is than Hell (applied on top of Hell's own monster\n; and treasure scaling), stored as tenths - 20 means 2.0x. Valid range 11-50\n; (1.1x-5.0x) in steps of 1 (0.1x).");

	setBoolean("Mini-Map", *sgOptions.Oracool.miniMapEnabled,
	    "; ----- MINI-MAP ----------------------------------------------------------------\n; Shows an always-on mini-map in the top-right corner during gameplay.\n; Independent of TAB, which still opens/closes the normal full-screen map exactly\n; as in vanilla; the mini-map simply hides while the full map is open and\n; reappears once it's closed. This is the only way to turn the mini-map off.");

	setBoolean("HUD Plate Art", *sgOptions.Oracool.hudPlateArt,
	    "; ----- HUD PLATE ------------------------------------------------------------------\n; Draws the stone plate behind the belt and the two skill wells. Turn it off to see\n; the HUD without it: the belt items, the skill icons, the XP readout and both orbs\n; stay exactly where they are, and what goes is the plate plus everything painted\n; into the picture rather than drawn separately - the belt cell frames, the two well\n; rims, and the Menu and Portal button faces.");

	setBoolean("Event Log", *sgOptions.Oracool.eventLog,
	    "; ----- EVENT LOG -----------------------------------------------------------------\n; Shows a small \"LOG\" button above the durability-warning icons that expands into a\n; timestamped log of noteworthy session events (game saves, boss kills, special item\n; drops, deaths). Session-only - not saved to disk.");

	setBoolean("Naked Heroes", *sgOptions.Oracool.nakedHeroes,
	    "; ----- NAKED HEROES -------------------------------------------------------------\n; New heroes start with nothing: no weapon, no shield, no armour, no potions and no\n; gold, with both mouse buttons on the bare fist. Read ONCE, when a character is\n; created - turning it off later re-equips nobody, and turning it on strips nobody.");

	setBoolean("Game Clock", *sgOptions.Oracool.gameClock,
	    "; ----- GAME CLOCK -----------------------------------------------------------------\n; Shows the current real-world time in the screen's top-left corner.");

	setBoolean("Game Clock 12 Hour Format", *sgOptions.Oracool.gameClock12HourFormat,
	    "; If true, the Game Clock shows 12-hour time with an AM/PM suffix (e.g. \"2:45 PM\")\n; instead of the default 24-hour format (e.g. \"14:45\").");

	setBoolean("Gradual Healing", *sgOptions.Oracool.gradualHealing,
	    "; ----- GRADUAL HEALING -----------------------------------------------------------------\n; Potion of Healing and Potion of Mana restore their usual random amount gradually over a few\n; seconds instead of instantly, matching Diablo 2's Healing/Mana Potions. Full Healing/Full Mana\n; Potions stay instant either way, matching Diablo 2's Rejuvenation Potions. Single-player only.");

	setBoolean("XP Counter", *sgOptions.Oracool.xpCounter,
	    "; ----- XP COUNTER -----------------------------------------------------------------\n; Shows the experience remaining until your next level just below the mini-map,\n; centered between the Game Clock and the LOG button. Hidden at max level.");
	setBoolean("XP Gain Indicator", *sgOptions.Oracool.xpGainIndicator,
	    "; Briefly flashes \"+N\" just below the XP Counter for half a second whenever you gain\n; experience.");
	setBoolean("Remaining Monster XP Button", *sgOptions.Oracool.remainingMonsterXpButton,
	    "; Press and hold the XP Counter to see, in white, the total experience worth of every\n; monster still alive on this level (adjusted for your current character level, exactly like\n; a real kill would be). Releases back to the normal readout.");
	setInteger("Monster Range Highlight", *sgOptions.Oracool.monsterRangeHighlight,
	    "; Monsters within this many tiles of the player get the same red outline normally shown\n; only when hovering them. Values: 0 (OFF), 1-5.");
	setBoolean("Monster Wall Outline", *sgOptions.Oracool.monsterWallOutline,
	    "; Draws a red outline on top of walls and other architecture for any monster that would\n; otherwise be hidden behind them, so you can tell something is there.");
	setBoolean("Quest Log Reveal All", *sgOptions.Oracool.questLogRevealAll,
	    "; ----- QUEST LOG -----------------------------------------------------------------\n; Every quest available this session shows up in the quest log immediately, instead of\n; only after you discover it through normal exploration/dialogue. Quest mechanics\n; (finding the trigger, talking to the right NPC, item spawns) are unaffected - this\n; only changes what the log shows you upfront. Single-player only.");
	setBoolean("Reset Stats Button", *sgOptions.Oracool.resetStatsButton,
	    "; ----- CHARACTER --------------------------------------------------------------\n; Shows a reset control on the character panel. Removes only the points you have\n; manually spent via the +/- buttons and returns them to distribute; permanent\n; bonuses from quests/shrines/items are untouched. Repeated use is safe.");
	setBoolean("Griswold Premium Ignore Affix Level Limits", *sgOptions.Oracool.griswoldPremiumIgnoreAffixLevelLimits,
	    "; ----- GRISWOLD: PREMIUM SHOP -------------------------------------------------\n; Allows compatible Premium prefixes and suffixes regardless of their normal\n; quality-level requirement. Item compatibility and good-affix rules remain.");
	setBoolean("Griswold Premium Ignore Price Limits", *sgOptions.Oracool.griswoldPremiumIgnorePriceLimits,
	    "; Prevents otherwise valid Premium Items from being rejected for exceeding the\n; normal price ceiling. The resulting item's calculated price remains unchanged.");
	setBoolean("Griswold Sell Ignores Belt", *sgOptions.Oracool.griswoldSellIgnoresBelt,
	    "; Griswold's and Adria's sell lists skip belt items entirely, listing only the\n; backpack (including any Tabbed Inventory extra tab).");
	setBoolean("Griswold Premium Refresh", *sgOptions.Oracool.griswoldPremiumRefresh,
	    "; Adds a free Refresh action to Premium Items, regenerating the complete stock\n; without requiring a new game.");
	setBoolean("Griswold Refresh Until Button", *sgOptions.Oracool.refreshUntilButton,
	    "; Adds Refresh Until to Premium Items. It regenerates stock internally until an\n; exact target is found, the timeout expires, or the safety limit is reached.");
	setString("Griswold Refresh Until Item Names", sgOptions.Oracool.refreshUntilItemNames,
	    "; Semicolon-separated exact displayed item names. Matching is case-insensitive\n; and ignores spaces around entries. Do not use quotation marks or commas.");
	setInteger("Griswold Refresh Until Timeout Seconds", *sgOptions.Oracool.refreshUntilTimeoutSeconds,
	    "; Maximum search duration in seconds. Zero disables the time stop, but the hard\n; safety limit of 100,000 premium generations remains active.");

	setBoolean("Griswold Restore Health", *sgOptions.Oracool.griswoldRestoreHealth,
	    "; ----- GRISWOLD: SERVICES -----------------------------------------------------\n; Silently restores current health to maximum whenever Griswold's main menu opens.\n; No additional menu entry, dialog, or sound appears.");
	setBoolean("Griswold Restore Mana", *sgOptions.Oracool.griswoldRestoreMana,
	    "; Silently restores current mana to maximum whenever Griswold's main menu opens.\n; No additional menu entry, dialog, or sound appears.");

	setBoolean("Griswold Sell Unique Items", *sgOptions.Oracool.griswoldSellUniqueItems,
	    "; ----- GRISWOLD: UNIQUE SHOP --------------------------------------------------\n; Adds a separate identified unique-item shop. Stock avoids duplicates, remains\n; independent of Premium refreshes, and does not immediately replace purchases.");
	setInteger("Griswold Unique Item Price Multiplier", *sgOptions.Oracool.griswoldUniqueItemPriceMultiplier,
	    "; Purchase-price multiplier applied to each unique item's normal sell value.\n; Example: 5 means five times its normal sell price.");

	setBoolean("Griswold Sell Rare Items", *sgOptions.Oracool.griswoldSellRareItems,
	    "; ----- GRISWOLD: RARE AND SET SHOPS -------------------------------------------\n; Adds a separate identified Rare-tier shop. The stock is rolled at the vendor's\n; own depth and, like the unique shelf, does not refill a purchase immediately.");
	setBoolean("Griswold Sell Set Items", *sgOptions.Oracool.griswoldSellSetItems,
	    "; Adds a separate named-set-piece shop. Only pieces your character level has\n; earned are offered, and no piece appears twice on the same shelf.");
	setBoolean("Shop Stock Refresh", *sgOptions.Oracool.shopStockRefresh,
	    "; Adds a free Refresh action to the Basic, Rare and Supplies shelves, matching\n; the one Premium Items already has. Regenerates that shelf's complete stock.");

	setBoolean("Auto Identify Drops", *sgOptions.Oracool.autoIdentifyDrops,
	    "; ----- ITEMS AND PICKUP -------------------------------------------------------\n; Identifies newly generated world drops immediately. Items deliberately dropped\n; by the player retain their existing identification state.");
	setInteger("Auto Pickup Range", *sgOptions.Oracool.autoPickupRange,
	    "; Search radius in tiles for DevilutionX's enabled automatic-pickup categories.\n; Values: 1-10. This does not enable categories disabled in normal game options.");
	setBoolean("Auto Pickup Scrolls", *sgOptions.Oracool.autoScrollPickup,
	    "; Automatically collects every kind of scroll (Identify, spell scrolls, Town Portal,\n; etc.) when in close proximity to the player, similar to the vanilla potion/elixir/oil options.");
	setBoolean("Auto Pickup Runes", *sgOptions.Oracool.autoRunePickup,
	    "; Automatically collects runes when in close proximity to the player. On by default:\n; a rune is never clutter, and walking back over one is the commonest way to lose it.");
	setBoolean("Auto Pickup Gems", *sgOptions.Oracool.autoGemPickup,
	    "; Automatically collects gems when in close proximity to the player. On by default,\n; on the same reasoning as runes.");
	setInteger("Rare Item Drop Chance", *sgOptions.Oracool.rareItemDropChance,
	    "; Percent chance that an item eligible for Magic quality becomes a Rare item\n; instead, checked after it has already failed its Unique roll. Zero disables Rares.");
	setInteger("Buffed Unique Item Drop Chance", *sgOptions.Oracool.buffedUniqueItemDropChance,
	    "; Percent chance that an item eligible for Magic quality becomes a Buffed Unique\n; instead, checked before Rare (right after failing its Unique roll). Zero disables\n; Buffed Uniques. Existing vanilla Unique items are never affected either way.");
	setInteger("Primal Item Drop Chance", *sgOptions.Oracool.primalItemDropChance,
	    "; Percent chance that an item eligible for Magic quality becomes a Primal item\n; instead, checked before Buffed Unique and Rare (right after failing its Unique\n; roll). Every affix on a Primal item is forced to its maximum roll. Zero disables Primals.");
	setInteger("Unique Drop Chance Percent", *sgOptions.Oracool.uniqueDropChancePercent,
	    "; Scales the chance an eligible drop becomes a unique item. 100 is vanilla's own window;\n; lower narrows it. This is the knob that NERFS - the multiplier below only ever widens.\n; Ignored while reconstructing a saved item, so lowering it cannot downgrade gear you\n; already own.");
	setInteger("Champion Extra Drop Chance", *sgOptions.Oracool.championExtraDropChance,
	    "; Percent chance a champion monster rolls a SECOND item when it dies. It used to be a\n; guaranteed second roll, which is why champions dropped two good items at a time.");

	setInteger("Unique Item Drop Multiplier", *sgOptions.Oracool.uniqueItemDropMultiplier,
	    "; Multiplies the chance that an eligible drop becomes unique. One is the normal\n; rate; higher values make uniques more common, with final probability capped.");

	setInteger("Monster Density", *sgOptions.Oracool.monsterDensityPercent,
	    "; How many monsters a dungeon level scatters, as a percentage of the vanilla count.\n; 100 is vanilla; 150, 200, 250 and 300 are one and a half to three times as many.\n; Quest monsters and the named uniques are placed by their own rules and ignore this.\n; The engine's own ceiling on live monsters still applies, so the densest levels\n; approach it rather than exceeding it.");

	setInteger("Monster Variant Chance", *sgOptions.Oracool.monsterVariantChancePercent,
	    "; How often an ordinary monster is a recoloured variant - a different palette, a different\n; name and one trait that changes the fight - as a percentage of the base rate. The base is\n; a ladder by difficulty: 15% on Normal rising to 28% on Torment, so 100 keeps that shape and\n; 200 doubles the whole curve. 0 turns variants off entirely. The result is capped, because a\n; floor where a third of the monsters are recoloured has made the recolour the default.");

	setInteger("Lesser Unique Density", *sgOptions.Oracool.lesserUniqueDensityPercent,
	    "; How many champion packs - underpowered versions of the game's named uniques, each with\n; minions - a dungeon level hosts. 100 is one pack; 300 is three. Each is drawn from the\n; champions written for monsters that ALREADY appear on that level, and its stats are\n; scaled to the floor rather than to the level the champion was originally written for.");

	setBoolean("Permanent Infravision", *sgOptions.Oracool.permanentInfravision,
	    "; ----- WORLD AND EXPLORATION --------------------------------------------------\n; Permanently reveals nearby monsters through walls as if infravision were active.");
	setBoolean("Unlock All Town Entrances", *sgOptions.Oracool.unlockAllTownEntrances,
	    "; Unlocks Catacombs, Caves, and Hell town entrances without normal level thresholds;\n; also Hive and Crypt in Hellfire. It does not complete quests or alter progress.");

	// Backstop: every registered entry the hand-written block above did not cover.
	//
	// External audit PO-01 (2026-08-30) found six of them - Dungeon Zoom Level, Game Speed Readout,
	// Panel Docking, Vendor Tiered Stock Chance and both Last Readied Spell slots. The section is
	// DELETED and rebuilt on every launch, and SaveOptions runs at startup, so an option that is
	// loaded but never rewritten is not merely unsaved: its key is destroyed on the next launch and
	// the value silently reverts to the constructor default. The remembered LMB/RMB spells could
	// therefore never survive a restart, which was the entire point of them.
	//
	// A loop rather than six more setBoolean lines. The hand-maintained list IS the defect - it has
	// to be extended by hand every time an option is added, and nothing failed when it was not.
	// Anything missed now lands here uncommented instead of being lost, and
	// OracoolOptions.EveryRegisteredEntrySurvivesASaveRoundTrip pins that.
	for (OptionEntryBase *pEntry : sgOptions.Oracool.GetEntries()) {
		const std::string key { pEntry->GetKey() };
		if (ini.GetValue(Section, key.c_str(), nullptr) != nullptr)
			continue;
		pEntry->SaveToIni(Section);
	}

	SaveIni();
}

string_view OptionEntryBase::GetName() const
{
	return _(name);
}
string_view OptionEntryBase::GetDescription() const
{
	return _(description);
}
string_view OptionEntryBase::GetKey() const
{
	return key;
}
OptionEntryFlags OptionEntryBase::GetFlags() const
{
	return flags;
}
void OptionEntryBase::SetValueChangedCallback(std::function<void()> callback)
{
	this->callback = std::move(callback);
}
void OptionEntryBase::NotifyValueChanged()
{
	if (callback)
		callback();
}

void OptionEntryBoolean::LoadFromIni(string_view category)
{
	value = GetIniBool(category.data(), key.data(), defaultValue);
}
void OptionEntryBoolean::SaveToIni(string_view category) const
{
	SetIniValue(category.data(), key.data(), value);
}
void OptionEntryBoolean::SetValue(bool value)
{
	this->value = value;
	this->NotifyValueChanged();
}
OptionEntryType OptionEntryBoolean::GetType() const
{
	return OptionEntryType::Boolean;
}
string_view OptionEntryBoolean::GetValueDescription() const
{
	return value ? _("ON") : _("OFF");
}

OptionEntryType OptionEntryListBase::GetType() const
{
	return OptionEntryType::List;
}
string_view OptionEntryListBase::GetValueDescription() const
{
	return GetListDescription(GetActiveListIndex());
}

void OptionEntryEnumBase::LoadFromIni(string_view category)
{
	value = GetIniInt(category.data(), key.data(), defaultValue);
}
void OptionEntryEnumBase::SaveToIni(string_view category) const
{
	SetIniValue(category.data(), key.data(), value);
}
void OptionEntryEnumBase::SetValueInternal(int value)
{
	this->value = value;
	this->NotifyValueChanged();
}
void OptionEntryEnumBase::AddEntry(int value, string_view name)
{
	entryValues.push_back(value);
	entryNames.push_back(name);
}
size_t OptionEntryEnumBase::GetListSize() const
{
	return entryValues.size();
}
string_view OptionEntryEnumBase::GetListDescription(size_t index) const
{
	return _(entryNames[index].data());
}
size_t OptionEntryEnumBase::GetActiveListIndex() const
{
	auto iterator = std::find(entryValues.begin(), entryValues.end(), value);
	if (iterator == entryValues.end())
		return 0;
	return std::distance(entryValues.begin(), iterator);
}
void OptionEntryEnumBase::SetActiveListIndex(size_t index)
{
	this->value = entryValues[index];
	this->NotifyValueChanged();
}

void OptionEntryIntBase::LoadFromIni(string_view category)
{
	value = GetIniInt(category.data(), key.data(), defaultValue);
	if (std::find(entryValues.begin(), entryValues.end(), value) == entryValues.end()) {
		entryValues.push_back(value);
		std::sort(entryValues.begin(), entryValues.end());
		entryNames.clear();
	}
}
void OptionEntryIntBase::SaveToIni(string_view category) const
{
	SetIniValue(category.data(), key.data(), value);
}
void OptionEntryIntBase::SetValueInternal(int value)
{
	this->value = value;
	this->NotifyValueChanged();
}
void OptionEntryIntBase::AddEntry(int value)
{
	entryValues.push_back(value);
}
size_t OptionEntryIntBase::GetListSize() const
{
	return entryValues.size();
}
string_view OptionEntryIntBase::GetListDescription(size_t index) const
{
	if (entryNames.empty()) {
		for (auto value : entryValues) {
			entryNames.push_back(StrCat(value));
		}
	}
	return entryNames[index].data();
}
size_t OptionEntryIntBase::GetActiveListIndex() const
{
	auto iterator = std::find(entryValues.begin(), entryValues.end(), value);
	if (iterator == entryValues.end())
		return 0;
	return std::distance(entryValues.begin(), iterator);
}
void OptionEntryIntBase::SetActiveListIndex(size_t index)
{
	this->value = entryValues[index];
	this->NotifyValueChanged();
}

string_view OptionCategoryBase::GetKey() const
{
	return key;
}
string_view OptionCategoryBase::GetName() const
{
	return _(name);
}
string_view OptionCategoryBase::GetDescription() const
{
	return _(description);
}

GameModeOptions::GameModeOptions()
    : OptionCategoryBase("GameMode", N_("Game Mode"), N_("Game Mode Settings"))
    // Oracool: user request - Hellfire is the default, and the game never asks. Vanilla's default
    // is Ask, which is what triggered the startup dialog; that dialog is gone and any .ini still
    // holding Ask is migrated on boot (see DiabloInit). Ask stays in the enum so an old .ini still
    // parses, and stays out of the list below so it cannot be chosen again.
    , gameMode("Game", OptionEntryFlags::NeedHellfireMpq | OptionEntryFlags::RecreateUI, N_("Game Mode"), N_("Play Diablo or Hellfire."), StartUpGameMode::Hellfire,
          {
              { StartUpGameMode::Diablo, N_("Diablo") },
              // Ask is missing, cause we want to hide it from UI-Settings.
              { StartUpGameMode::Hellfire, N_("Hellfire") },
          })
    , shareware("Shareware", OptionEntryFlags::NeedDiabloMpq | OptionEntryFlags::RecreateUI, N_("Restrict to Shareware"), N_("Makes the game compatible with the demo. Enables multiplayer with friends who don't own a full copy of Diablo."), false)

{
	gameMode.SetValueChangedCallback(OptionGameModeChanged);
	shareware.SetValueChangedCallback(OptionSharewareChanged);
}
std::vector<OptionEntryBase *> GameModeOptions::GetEntries()
{
	return {
		&gameMode,
		&shareware,
	};
}

StartUpOptions::StartUpOptions()
    : OptionCategoryBase("StartUp", N_("Start Up"), N_("Start Up Settings"))
    , diabloIntro("Diablo Intro", OptionEntryFlags::OnlyDiablo, N_("Intro"), N_("Shown Intro cinematic."), StartUpIntro::Once,
          {
              { StartUpIntro::Off, N_("OFF") },
              // Once is missing, cause we want to hide it from UI-Settings.
              { StartUpIntro::On, N_("ON") },
          })
    // Oracool default: Off. Diablo's intro still plays Once; Hellfire's does not, because this is a
    // Diablo fork that happens to require Hellfire's data - the expansion's own cinematic is not the
    // front door to this game.
    , hellfireIntro("Hellfire Intro", OptionEntryFlags::OnlyHellfire, N_("Intro"), N_("Shown Intro cinematic."), StartUpIntro::Off,
          {
              { StartUpIntro::Off, N_("OFF") },
              // Once is missing, cause we want to hide it from UI-Settings.
              { StartUpIntro::On, N_("ON") },
          })
    // Oracool default: straight to the title dialog. Adopted from the user's own diablo.ini
    // (2026-08-27, "make its setting the default in future releases") - this is a fork people
    // relaunch constantly while testing, and a logo between them and the menu every time is a cost
    // paid on every single run.
    , splash("Splash", OptionEntryFlags::None, N_("Splash"), N_("Shown splash screen."), StartUpSplash::None,
          {
              { StartUpSplash::LogoAndTitleDialog, N_("Logo and Title Screen") },
              { StartUpSplash::TitleDialog, N_("Title Screen") },
              { StartUpSplash::None, N_("None") },
          })
{
}
std::vector<OptionEntryBase *> StartUpOptions::GetEntries()
{
	return {
		&diabloIntro,
		&hellfireIntro,
		&splash,
	};
}

DiabloOptions::DiabloOptions()
    : OptionCategoryBase("Diablo", N_("Diablo"), N_("Diablo specific Settings"))
    , lastSinglePlayerHero("LastSinglePlayerHero", OptionEntryFlags::Invisible | OptionEntryFlags::OnlyDiablo, "Sample Rate", "Remembers what singleplayer hero/save was last used.", 0)
    , lastMultiplayerHero("LastMultiplayerHero", OptionEntryFlags::Invisible | OptionEntryFlags::OnlyDiablo, "Sample Rate", "Remembers what multiplayer hero/save was last used.", 0)
{
}
std::vector<OptionEntryBase *> DiabloOptions::GetEntries()
{
	return {
		&lastSinglePlayerHero,
		&lastMultiplayerHero,
	};
}

HellfireOptions::HellfireOptions()
    : OptionCategoryBase("Hellfire", N_("Hellfire"), N_("Hellfire specific Settings"))
    , lastSinglePlayerHero("LastSinglePlayerHero", OptionEntryFlags::Invisible | OptionEntryFlags::OnlyHellfire, "Sample Rate", "Remembers what singleplayer hero/save was last used.", 0)
    , lastMultiplayerHero("LastMultiplayerHero", OptionEntryFlags::Invisible | OptionEntryFlags::OnlyHellfire, "Sample Rate", "Remembers what multiplayer hero/save was last used.", 0)
{
}
std::vector<OptionEntryBase *> HellfireOptions::GetEntries()
{
	return {
		&lastSinglePlayerHero,
		&lastMultiplayerHero,
	};
}

AudioOptions::AudioOptions()
    : OptionCategoryBase("Audio", N_("Audio"), N_("Audio Settings"))
    , soundVolume("Sound Volume", OptionEntryFlags::Invisible, "Sound Volume", "Movie and SFX volume.", VOLUME_MAX)
    , musicVolume("Music Volume", OptionEntryFlags::Invisible, "Music Volume", "Music Volume.", VOLUME_MAX)
    , walkingSound("Walking Sound", OptionEntryFlags::None, N_("Walking Sound"), N_("Player emits sound when walking."), true)
    , autoEquipSound("Auto Equip Sound", OptionEntryFlags::None, N_("Auto Equip Sound"), N_("Automatically equipping items on pickup emits the equipment sound."), false)
    , itemPickupSound("Item Pickup Sound", OptionEntryFlags::None, N_("Item Pickup Sound"), N_("Picking up items emits the items pickup sound."), false)
    , sampleRate("Sample Rate", OptionEntryFlags::CantChangeInGame, N_("Sample Rate"), N_("Output sample rate (Hz)."), DEFAULT_AUDIO_SAMPLE_RATE, { 22050, 44100, 48000 })
    , channels("Channels", OptionEntryFlags::CantChangeInGame, N_("Channels"), N_("Number of output channels."), DEFAULT_AUDIO_CHANNELS, { 1, 2 })
    , bufferSize("Buffer Size", OptionEntryFlags::CantChangeInGame, N_("Buffer Size"), N_("Buffer size (number of frames per channel)."), DEFAULT_AUDIO_BUFFER_SIZE, { 1024, 2048, 5120 })
    , resamplingQuality("Resampling Quality", OptionEntryFlags::CantChangeInGame, N_("Resampling Quality"), N_("Quality of the resampler, from 0 (lowest) to 10 (highest)."), DEFAULT_AUDIO_RESAMPLING_QUALITY, { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 })
{
	sampleRate.SetValueChangedCallback(OptionAudioChanged);
	channels.SetValueChangedCallback(OptionAudioChanged);
	bufferSize.SetValueChangedCallback(OptionAudioChanged);
	resamplingQuality.SetValueChangedCallback(OptionAudioChanged);
	resampler.SetValueChangedCallback(OptionAudioChanged);
	device.SetValueChangedCallback(OptionAudioChanged);
}
std::vector<OptionEntryBase *> AudioOptions::GetEntries()
{
	// clang-format off
	return {
		&soundVolume,
		&musicVolume,
		&walkingSound,
		&autoEquipSound,
		&itemPickupSound,
		&sampleRate,
		&channels,
		&bufferSize,
		&resampler,
		&resamplingQuality,
#if SDL_VERSION_ATLEAST(2, 0, 0)
		&device,
#endif
	};
	// clang-format on
}

namespace {

struct CuratedResolution {
	Size size;
	const char *ratioLabel;
};

/**
 * Oracool: user request - replace the old monitor-detected resolution list (which depended on
 * what the display driver happened to report, and could include anything down to 640x480) with
 * a fixed, curated list spanning five aspect ratios, from a 960x720 floor up to a 1440-tall
 * ceiling. Real/recognizable resolutions are used where they exist (1024x768, 1920x1080,
 * 2560x1440, WXGA/WSXGA+/WUXGA, common ultrawide panels) rather than purely mathematically
 * generated ones, per the user's "meaningful" requirement. Listed here grouped by ratio for
 * readability, but CheckResolutionsAreInitialized() sorts the displayed list by height then
 * width (equivalent to ratio, for entries sharing a height) rather than using this array order.
 */
constexpr CuratedResolution CuratedResolutions[] = {
	// 4:3
	{ { 960, 720 }, "4:3" },
	{ { 1024, 768 }, "4:3" },
	{ { 1152, 864 }, "4:3" },
	{ { 1280, 960 }, "4:3" },
	{ { 1400, 1050 }, "4:3" },
	{ { 1600, 1200 }, "4:3" },
	{ { 1920, 1440 }, "4:3" },
	// 3:2
	{ { 1152, 768 }, "3:2" },
	{ { 1440, 960 }, "3:2" },
	{ { 1920, 1280 }, "3:2" },
	{ { 2160, 1440 }, "3:2" },
	// 16:10
	{ { 1280, 800 }, "16:10" },
	{ { 1440, 900 }, "16:10" },
	{ { 1680, 1050 }, "16:10" },
	{ { 1920, 1200 }, "16:10" },
	// 16:9
	{ { 1280, 720 }, "16:9" },
	{ { 1600, 900 }, "16:9" },
	{ { 1920, 1080 }, "16:9" },
	{ { 2560, 1440 }, "16:9" },
	// 21:9
	{ { 1680, 720 }, "21:9" },
	{ { 2560, 1080 }, "21:9" },
	{ { 3440, 1440 }, "21:9" },
};

/**
 * Oracool: snaps a loaded (possibly pre-curated-list, possibly hand-edited) ini resolution to
 * the closest entry in CuratedResolutions, so the resolution option always resolves to one of
 * the fixed list's entries - nothing below the 960x720 floor, and nothing off-list, can persist
 * across a load.
 */
Size SnapToNearestCuratedResolution(Size size)
{
	Size best = CuratedResolutions[0].size;
	int bestDistance = std::numeric_limits<int>::max();
	for (const CuratedResolution &entry : CuratedResolutions) {
		if (entry.size == size)
			return size;
		const int distance = std::abs(entry.size.width - size.width) + std::abs(entry.size.height - size.height);
		if (distance < bestDistance) {
			bestDistance = distance;
			best = entry.size;
		}
	}
	return best;
}

} // namespace

OptionEntryResolution::OptionEntryResolution()
    : OptionEntryListBase("", OptionEntryFlags::CantChangeInGame | OptionEntryFlags::RecreateUI, N_("Resolution"), N_("Affect the game's internal resolution and determine your view area. Note: This can differ from screen resolution, when Upscaling, Integer Scaling or Fit to Screen is used."))
{
}
void OptionEntryResolution::LoadFromIni(string_view category)
{
	const Size loaded { GetIniInt(category.data(), "Width", DEFAULT_WIDTH), GetIniInt(category.data(), "Height", DEFAULT_HEIGHT) };
	// Oracool: user request - 960x720 is the supported floor; snap anything saved below it (or
	// anything not matching one of the curated entries, e.g. from before this list existed) up
	// to the closest valid resolution instead of allowing an unsupported value to load.
	size = SnapToNearestCuratedResolution(loaded);
}
void OptionEntryResolution::SaveToIni(string_view category) const
{
	SetIniValue(category.data(), "Width", size.width);
	SetIniValue(category.data(), "Height", size.height);
}

void OptionEntryResolution::InvalidateList()
{
	resolutions.clear();
}

void OptionEntryResolution::CheckResolutionsAreInitialized() const
{
	if (!resolutions.empty())
		return;

	// Oracool: user request - determine the device's actual desktop resolution so curated
	// entries too large to ever display (bigger than the desktop, in either dimension) can be
	// dropped from the list. Falls back to offering the full curated list if the desktop size
	// can't be determined, or if filtering would remove every entry (e.g. an unusually small
	// display below even the 960x720 floor) - better to offer resolutions that may need
	// scaling than to leave nothing selectable.
	Size desktopSize { 0, 0 };
#ifdef USE_SDL1
	const SDL_VideoInfo *videoInfo = SDL_GetVideoInfo();
	if (videoInfo != nullptr)
		desktopSize = { videoInfo->current_w, videoInfo->current_h };
#else
	SDL_DisplayMode desktopMode;
	if (SDL_GetDesktopDisplayMode(0, &desktopMode) == 0)
		desktopSize = { desktopMode.w, desktopMode.h };
#endif

	std::vector<CuratedResolution> filtered;
	for (const CuratedResolution &entry : CuratedResolutions) {
		if (desktopSize.width > 0 && desktopSize.height > 0
		    && (entry.size.width > desktopSize.width || entry.size.height > desktopSize.height)) {
			continue;
		}
		filtered.push_back(entry);
	}
	if (filtered.empty()) {
		for (const CuratedResolution &entry : CuratedResolutions)
			filtered.push_back(entry);
	}

	// Ensures the ini-specified resolution is present even in the unexpected case that it isn't
	// already one of the curated entries (LoadFromIni snaps it to the nearest one via
	// SnapToNearestCuratedResolution, so this is a defensive fallback, not the normal path).
	const bool alreadyPresent = std::find_if(filtered.begin(), filtered.end(), [this](const CuratedResolution &entry) {
		return entry.size == this->size;
	}) != filtered.end();
	if (!alreadyPresent)
		filtered.push_back({ this->size, "custom" });

	// Oracool: user request - display order is by height first, then width (equivalent to
	// sorting by aspect ratio for entries that share a height), instead of the fixed
	// aspect-ratio blocks the CuratedResolutions array is written in above.
	std::sort(filtered.begin(), filtered.end(), [](const CuratedResolution &a, const CuratedResolution &b) {
		if (a.size.height != b.size.height)
			return a.size.height < b.size.height;
		return a.size.width < b.size.width;
	});

#ifndef USE_SDL1
	int lastFitToScreenHeight = -1;
#endif
	for (const CuratedResolution &entry : filtered) {
		Size size = entry.size;
#ifndef USE_SDL1
		if (*sgOptions.Graphics.fitToScreen) {
			// Fit to Screen stretches every entry's width to the desktop's own aspect ratio, so
			// the curated ratio label no longer describes what's actually displayed - keep the
			// original "XXXp" height-only labeling for this mode instead of the ratio hint.
			// Oracool: user request - the stretched width depends only on height and the
			// desktop's aspect ratio, so every curated entry sharing a height resolves to the
			// exact same final resolution here; filtered is sorted by height, so duplicates are
			// adjacent and skipping repeats collapses them to one "XXXp" entry per height
			// instead of listing the same effective resolution two to four times over.
			if (size.height == lastFitToScreenHeight)
				continue;
			lastFitToScreenHeight = size.height;
			if (desktopSize.width > 0 && desktopSize.height > 0)
				size.width = size.height * desktopSize.width / desktopSize.height;
			resolutions.emplace_back(size, StrCat(size.height, "p"));
			continue;
		}
#endif
		resolutions.emplace_back(size, StrCat(size.width, "x", size.height, " (", entry.ratioLabel, ")"));
	}
}

size_t OptionEntryResolution::GetListSize() const
{
	CheckResolutionsAreInitialized();
	return resolutions.size();
}
string_view OptionEntryResolution::GetListDescription(size_t index) const
{
	CheckResolutionsAreInitialized();
	return resolutions[index].second;
}
size_t OptionEntryResolution::GetActiveListIndex() const
{
	CheckResolutionsAreInitialized();
	auto found = std::find_if(resolutions.begin(), resolutions.end(), [this](const auto &x) { return x.first == this->size; });
	if (found != resolutions.end())
		return std::distance(resolutions.begin(), found);

	// No exact match. Audit finding, 2026-08-26: this used to answer 0, which silently moved the
	// player's selection to the shortest resolution in the list.
	//
	// It is reachable in Fit to Screen, where the stored size is a width DERIVED from the desktop's
	// aspect ratio rather than a curated one. Change the monitor and that width no longer matches
	// anything the list generates, so the menu jumped to its first entry - the setting had not
	// changed, but the menu said it had, and confirming the screen wrote the lie back.
	//
	// The nearest HEIGHT is the honest answer, because height is what the player chose: in Fit to
	// Screen the list is labelled by height alone ("720p") and the width follows from the desktop.
	size_t best = 0;
	int bestDistance = std::numeric_limits<int>::max();
	for (size_t i = 0; i < resolutions.size(); i++) {
		const int distance = std::abs(resolutions[i].first.height - this->size.height);
		if (distance < bestDistance) {
			bestDistance = distance;
			best = i;
		}
	}
	return best;
}
void OptionEntryResolution::SetActiveListIndex(size_t index)
{
	size = resolutions[index].first;
	NotifyValueChanged();
}

OptionEntryResampler::OptionEntryResampler()
    : OptionEntryListBase("Resampler", OptionEntryFlags::CantChangeInGame
              // When there are exactly 2 options there is no submenu, so we need to recreate the UI
              // to reflect the change in the "Resampling quality" setting visibility.
              | (NumResamplers == 2 ? OptionEntryFlags::RecreateUI : OptionEntryFlags::None),
          N_("Resampler"), N_("Audio resampler"))
{
}
void OptionEntryResampler::LoadFromIni(string_view category)
{
	char resamplerStr[32];
	if (GetIniValue(category, key, resamplerStr, sizeof(resamplerStr))) {
		std::optional<Resampler> resampler = ResamplerFromString(resamplerStr);
		if (resampler) {
			resampler_ = *resampler;
			UpdateDependentOptions();
			return;
		}
	}
	resampler_ = Resampler::DEVILUTIONX_DEFAULT_RESAMPLER;
	UpdateDependentOptions();
}

void OptionEntryResampler::SaveToIni(string_view category) const
{
	SetIniValue(category, key, ResamplerToString(resampler_));
}

size_t OptionEntryResampler::GetListSize() const
{
	return NumResamplers;
}

string_view OptionEntryResampler::GetListDescription(size_t index) const
{
	return ResamplerToString(static_cast<Resampler>(index));
}

size_t OptionEntryResampler::GetActiveListIndex() const
{
	return static_cast<size_t>(resampler_);
}

void OptionEntryResampler::SetActiveListIndex(size_t index)
{
	resampler_ = static_cast<Resampler>(index);
	UpdateDependentOptions();
	NotifyValueChanged();
}

void OptionEntryResampler::UpdateDependentOptions() const
{
#ifdef DEVILUTIONX_RESAMPLER_SPEEX
	if (resampler_ == Resampler::Speex) {
		sgOptions.Audio.resamplingQuality.flags &= ~OptionEntryFlags::Invisible;
	} else {
		sgOptions.Audio.resamplingQuality.flags |= OptionEntryFlags::Invisible;
	}
#endif
}

OptionEntryAudioDevice::OptionEntryAudioDevice()
    : OptionEntryListBase("Device", OptionEntryFlags::CantChangeInGame, N_("Device"), N_("Audio device"))
{
}
void OptionEntryAudioDevice::LoadFromIni(string_view category)
{
	char deviceStr[100];
	GetIniValue(category, key, deviceStr, sizeof(deviceStr), "");
	deviceName_ = deviceStr;
}

void OptionEntryAudioDevice::SaveToIni(string_view category) const
{
#if SDL_VERSION_ATLEAST(2, 0, 0)
	SetIniValue(category, key, deviceName_);
#endif
}

size_t OptionEntryAudioDevice::GetListSize() const
{
#if SDL_VERSION_ATLEAST(2, 0, 0)
	return SDL_GetNumAudioDevices(false) + 1;
#else
	return 1;
#endif
}

string_view OptionEntryAudioDevice::GetListDescription(size_t index) const
{
	constexpr int MaxWidth = 500;

	string_view deviceName = GetDeviceName(index);
	if (deviceName.empty())
		return "System Default";

	while (GetLineWidth(deviceName, GameFont24, 1) > MaxWidth) {
		size_t lastSymbolIndex = FindLastUtf8Symbols(deviceName);
		deviceName = string_view(deviceName.data(), lastSymbolIndex);
	}

	return deviceName;
}

size_t OptionEntryAudioDevice::GetActiveListIndex() const
{
	for (size_t i = 0; i < GetListSize(); i++) {
		string_view deviceName = GetDeviceName(i);
		if (deviceName == deviceName_)
			return i;
	}
	return 0;
}

void OptionEntryAudioDevice::SetActiveListIndex(size_t index)
{
	deviceName_ = std::string { GetDeviceName(index) };
	NotifyValueChanged();
}

string_view OptionEntryAudioDevice::GetDeviceName(size_t index) const
{
#if SDL_VERSION_ATLEAST(2, 0, 0)
	if (index != 0)
		return SDL_GetAudioDeviceName(index - 1, false);
#endif
	return "";
}

GraphicsOptions::GraphicsOptions()
    : OptionCategoryBase("Graphics", N_("Graphics"), N_("Graphics Settings"))
    , fullscreen("Fullscreen", OnlyIfSupportsWindowed | OptionEntryFlags::CantChangeInGame | OptionEntryFlags::RecreateUI, N_("Fullscreen"), N_("Display the game in windowed or fullscreen mode."), true)
#if !defined(USE_SDL1) || defined(__3DS__)
    , fitToScreen("Fit to Screen", OptionEntryFlags::CantChangeInGame | OptionEntryFlags::RecreateUI, N_("Fit to Screen"), N_("Automatically adjust the game window to your current desktop screen aspect ratio and resolution."), true)
#endif
#ifndef USE_SDL1
    , upscale("Upscale", OptionEntryFlags::Invisible | OptionEntryFlags::CantChangeInGame | OptionEntryFlags::RecreateUI, N_("Upscale"), N_("Enables image scaling from the game resolution to your monitor resolution. Prevents changing the monitor resolution and allows window resizing."),
#ifdef NXDK
          false
#else
          true
#endif
          )
    , scaleQuality("Scaling Quality", OptionEntryFlags::None, N_("Scaling Quality"), N_("Enables optional filters to the output image when upscaling."), ScalingQuality::AnisotropicFiltering,
          {
              { ScalingQuality::NearestPixel, N_("Nearest Pixel") },
              { ScalingQuality::BilinearFiltering, N_("Bilinear") },
              { ScalingQuality::AnisotropicFiltering, N_("Anisotropic") },
          })
    , integerScaling("Integer Scaling", OptionEntryFlags::CantChangeInGame | OptionEntryFlags::RecreateUI, N_("Integer Scaling"), N_("Scales the image using whole number pixel ratio."), false)
#endif
    , frameRateControl("Frame Rate Control",
          OptionEntryFlags::RecreateUI
#if defined(NXDK) || defined(__ANDROID__)
              | OptionEntryFlags::Invisible
#endif
          ,
          N_("Frame Rate Control"),
          N_("Manages frame rate to balance performance, reduce tearing, or save power."),
#if defined(NXDK) || defined(USE_SDL1)
          FrameRateControl::CPUSleep
#else
          FrameRateControl::VerticalSync
#endif
          ,
          {
              { FrameRateControl::None, N_("None") },
#ifndef USE_SDL1
              { FrameRateControl::VerticalSync, N_("Vertical Sync") },
#endif
              { FrameRateControl::CPUSleep, N_("Limit FPS") },
          })
    , gammaCorrection("Gamma Correction", OptionEntryFlags::Invisible, "Gamma Correction", "Gamma correction level.", 100)
    , colorCycling("Color Cycling", OptionEntryFlags::None, N_("Color Cycling"), N_("Color cycling effect used for water, lava, and acid animation."), true)
    , alternateNestArt("Alternate nest art", OptionEntryFlags::OnlyHellfire | OptionEntryFlags::CantChangeInGame, N_("Alternate nest art"), N_("The game will use an alternative palette for Hellfire’s nest tileset."), false)
#if SDL_VERSION_ATLEAST(2, 0, 0)
    , hardwareCursor("Hardware Cursor", OptionEntryFlags::CantChangeInGame | OptionEntryFlags::RecreateUI | (HardwareCursorSupported() ? OptionEntryFlags::None : OptionEntryFlags::Invisible), N_("Hardware Cursor"), N_("Use a hardware cursor"), HardwareCursorDefault())
    , hardwareCursorForItems("Hardware Cursor For Items", OptionEntryFlags::CantChangeInGame | (HardwareCursorSupported() ? OptionEntryFlags::None : OptionEntryFlags::Invisible), N_("Hardware Cursor For Items"), N_("Use a hardware cursor for items."), true)
    , hardwareCursorMaxSize("Hardware Cursor Maximum Size", OptionEntryFlags::CantChangeInGame | OptionEntryFlags::RecreateUI | (HardwareCursorSupported() ? OptionEntryFlags::None : OptionEntryFlags::Invisible), N_("Hardware Cursor Maximum Size"), N_("Maximum width / height for the hardware cursor. Larger cursors fall back to software."), 128, { 0, 64, 128, 256, 512 })
#endif
    , showFPS("Show FPS", OptionEntryFlags::None, N_("Show FPS"), N_("Displays the FPS in the upper left corner of the screen."), false)
{
	resolution.SetValueChangedCallback(ResizeWindow);
	fullscreen.SetValueChangedCallback(SetFullscreenMode);
#if !defined(USE_SDL1) || defined(__3DS__)
	fitToScreen.SetValueChangedCallback(ResizeWindowAndUpdateResolutionOptions);
#endif
#ifndef USE_SDL1
	scaleQuality.SetValueChangedCallback(ReinitializeTexture);
	integerScaling.SetValueChangedCallback(ReinitializeIntegerScale);
	frameRateControl.SetValueChangedCallback(ReinitializeRenderer);
#endif
	showFPS.SetValueChangedCallback(OptionShowFPSChanged);
}
std::vector<OptionEntryBase *> GraphicsOptions::GetEntries()
{
	// clang-format off
	return {
		&resolution,
#ifndef __vita__
		&fullscreen,
#endif
#if !defined(USE_SDL1) || defined(__3DS__)
		&fitToScreen,
#endif
#ifndef USE_SDL1
		&upscale,
		&scaleQuality,
		&integerScaling,
#endif
		&frameRateControl,
		&gammaCorrection,
		&showFPS,
		&colorCycling,
		&alternateNestArt,
#if SDL_VERSION_ATLEAST(2, 0, 0)
		&hardwareCursor,
		&hardwareCursorForItems,
		&hardwareCursorMaxSize,
#endif
	};
	// clang-format on
}

GameplayOptions::GameplayOptions()
    : OptionCategoryBase("Game", N_("Gameplay"), N_("Gameplay Settings"))
    , tickRate("Speed", OptionEntryFlags::Invisible, "Speed", "Gameplay ticks per second.", 20)
    , runInTown("Run in Town", OptionEntryFlags::CantChangeInMultiPlayer, N_("Run in Town"), N_("Enable jogging/fast walking in town for Diablo and Hellfire. This option was introduced in the expansion."), true)
    , grabInput("Grab Input", OptionEntryFlags::None, N_("Grab Input"), N_("When enabled mouse is locked to the game window."), false)
    // Oracool: user request - the Little Girl quest is on by default now. Same treatment as the Bard
    // and the Barbarian: vanilla hides a finished piece of content behind a switch that defaults off,
    // and flipping the default is the whole change - the option stays, so it can still be turned off.
    //
    // Safe to default on, unlike its neighbour below: this one ADDS a towner (Celia, in town once
    // level 17 has been visited - see towners.cpp) and takes nothing away. Nothing else changes.
    , theoQuest("Theo Quest", OptionEntryFlags::CantChangeInGame | OptionEntryFlags::OnlyHellfire, N_("Theo Quest"), N_("Enable Little Girl quest."), true)
    // Oracool: user request - three ways rather than two, because the interesting answer was the one
    // a boolean could not hold. Random is the default: the pair is mutually exclusive, so leaving it
    // to the seed is the only way a playthrough can meet either of them.
    //
    // The ini key stays "Cow Quest" although the option is no longer named that. Renaming it would
    // silently discard the setting out of every existing ini for nothing - the key is internal and
    // the enum's values are chosen to match what that key already held (see FarmerQuestMode).
    , farmerQuest("Cow Quest", OptionEntryFlags::CantChangeInGame | OptionEntryFlags::OnlyHellfire, N_("Farmer's Quest"), N_("Which farmer stands outside town. Lester offers Farmer's Orchard; the Complete Nut offers The Jersey's Jersey. They cannot both appear, so Random draws one per game."), FarmerQuestMode::Random,
          {
              { FarmerQuestMode::Random, N_("Random") },
              { FarmerQuestMode::AlwaysFarmer, N_("Always Farmer") },
              { FarmerQuestMode::AlwaysCow, N_("Always Cow") },
          })
    , friendlyFire("Friendly Fire", OptionEntryFlags::CantChangeInMultiPlayer, N_("Friendly Fire"), N_("Allow arrow/spell damage between players in multiplayer even when the friendly mode is on."), true)
    , multiplayerFullQuests("MultiplayerFullQuests", OptionEntryFlags::CantChangeInMultiPlayer, N_("Full quests in Multiplayer"), N_("Enables the full/uncut singleplayer version of quests."), false)
    // Oracool: user request - the full six-class roster is on offer, so the Bard is back, on by
    // default, with its switch visible in the settings menu again. Same treatment as the Barbarian
    // below: vanilla hides both behind these test switches, and flipping the default is the whole
    // change - the option stays, so the class can still be turned off.
    //
    // Turning it off is safe for a Bard that already exists: the class vanishes from the new-hero
    // list, but its starting Sword and Dagger stay available to IsItemAvailable() either way, so
    // they are not stripped off the character on load. See the note there.
    , testBard("Test Bard", OptionEntryFlags::CantChangeInGame, N_("Test Bard"), N_("Force the Bard character type to appear in the hero selection menu."), true)
    // Oracool: user request - the Barbarian is on by default. It is a Hellfire class that vanilla
    // hides behind this switch; Oracool wants it in the hero list from the start, and flipping the
    // default here is the whole change - the option stays, so it can still be turned off.
    , testBarbarian("Test Barbarian", OptionEntryFlags::CantChangeInGame, N_("Test Barbarian"), N_("Force the Barbarian character type to appear in the hero selection menu."), true)
    , experienceBar("Experience Bar", OptionEntryFlags::None, N_("Experience Bar"), N_("Experience Bar is added to the UI at the bottom of the screen."), true)
    , showItemGraphicsInStores("Show Item Graphics in Stores", OptionEntryFlags::None, N_("Show Item Graphics in Stores"), N_("Show item graphics to the left of item descriptions in store menus."), true)
    , showHealthValues("Show health values", OptionEntryFlags::None, N_("Show health values"), N_("Displays current / max health value on health globe."), true)
    , showManaValues("Show mana values", OptionEntryFlags::None, N_("Show mana values"), N_("Displays current / max mana value on mana globe."), true)
    , enemyHealthBar("Enemy Health Bar", OptionEntryFlags::None, N_("Enemy Health Bar"), N_("Enemy Health Bar is displayed at the top of the screen."), true)
    , autoGoldPickup("Auto Gold Pickup", OptionEntryFlags::None, N_("Auto Gold Pickup"), N_("Gold is automatically collected when in close proximity to the player."), true)
    , autoElixirPickup("Auto Elixir Pickup", OptionEntryFlags::None, N_("Auto Elixir Pickup"), N_("Elixirs are automatically collected when in close proximity to the player."), true)
    , autoOilPickup("Auto Oil Pickup", OptionEntryFlags::OnlyHellfire, N_("Auto Oil Pickup"), N_("Oils are automatically collected when in close proximity to the player."), true)
    , autoPickupInTown("Auto Pickup in Town", OptionEntryFlags::None, N_("Auto Pickup in Town"), N_("Automatically pickup items in town."), true)
    , adriaRefillsMana("Adria Refills Mana", OptionEntryFlags::None, N_("Adria Refills Mana"), N_("Adria will refill your mana when you visit her shop."), true)
    , autoEquipWeapons("Auto Equip Weapons", OptionEntryFlags::None, N_("Auto Equip Weapons"), N_("Weapons will be automatically equipped on pickup or purchase if enabled."), false)
    , autoEquipArmor("Auto Equip Armor", OptionEntryFlags::None, N_("Auto Equip Armor"), N_("Armor will be automatically equipped on pickup or purchase if enabled."), false)
    , autoEquipHelms("Auto Equip Helms", OptionEntryFlags::None, N_("Auto Equip Helms"), N_("Helms will be automatically equipped on pickup or purchase if enabled."), false)
    , autoEquipShields("Auto Equip Shields", OptionEntryFlags::None, N_("Auto Equip Shields"), N_("Shields will be automatically equipped on pickup or purchase if enabled."), false)
    , autoEquipJewelry("Auto Equip Jewelry", OptionEntryFlags::None, N_("Auto Equip Jewelry"), N_("Jewelry will be automatically equipped on pickup or purchase if enabled."), false)
    , randomizeQuests("Randomize Quests", OptionEntryFlags::CantChangeInGame, N_("Randomize Quests"), N_("Randomly selecting available quests for new games."), false)
    , showMonsterType("Show Monster Type", OptionEntryFlags::None, N_("Show Monster Type"), N_("Hovering over a monster will display the type of monster in the description box in the UI."), true)
    , showItemLabels("Show Item Labels", OptionEntryFlags::None, N_("Show Item Labels"), N_("Show labels for items on the ground when enabled."), true)
    , autoRefillBelt("Auto Refill Belt", OptionEntryFlags::None, N_("Auto Refill Belt"), N_("Refill belt from inventory when belt item is consumed."), true)
    , disableCripplingShrines("Disable Crippling Shrines", OptionEntryFlags::None, N_("Disable Crippling Shrines"), N_("When enabled Cauldrons, Fascinating Shrines, Goat Shrines, Ornate Shrines, Sacred Shrines and Murphy's Shrines are not able to be clicked on and labeled as disabled."), true)
    , quickCast("Quick Cast", OptionEntryFlags::None, N_("Quick Cast"), N_("Spell hotkeys instantly cast the spell, rather than switching the readied spell."), false)
    , numHealPotionPickup("Heal Potion Pickup", OptionEntryFlags::None, N_("Heal Potion Pickup"), N_("Healing potions are automatically collected when in close proximity to the player, regardless of how many you're already carrying."), true)
    , numFullHealPotionPickup("Full Heal Potion Pickup", OptionEntryFlags::None, N_("Full Heal Potion Pickup"), N_("Full Healing potions are automatically collected when in close proximity to the player, regardless of how many you're already carrying."), true)
    , numManaPotionPickup("Mana Potion Pickup", OptionEntryFlags::None, N_("Mana Potion Pickup"), N_("Mana potions are automatically collected when in close proximity to the player, regardless of how many you're already carrying."), true)
    , numFullManaPotionPickup("Full Mana Potion Pickup", OptionEntryFlags::None, N_("Full Mana Potion Pickup"), N_("Full Mana potions are automatically collected when in close proximity to the player, regardless of how many you're already carrying."), true)
    , numRejuPotionPickup("Rejuvenation Potion Pickup", OptionEntryFlags::None, N_("Rejuvenation Potion Pickup"), N_("Rejuvenation potions are automatically collected when in close proximity to the player, regardless of how many you're already carrying."), true)
    , numFullRejuPotionPickup("Full Rejuvenation Potion Pickup", OptionEntryFlags::None, N_("Full Rejuvenation Potion Pickup"), N_("Full Rejuvenation potions are automatically collected when in close proximity to the player, regardless of how many you're already carrying."), true)
    , enableFloatingNumbers("Enable floating numbers", OptionEntryFlags::None, N_("Enable floating numbers"), N_("Enables floating numbers on gaining XP / dealing damage etc."), FloatingNumbers::Vertical,
          {
              { FloatingNumbers::Off, N_("Off") },
              { FloatingNumbers::Random, N_("Random Angles") },
              { FloatingNumbers::Vertical, N_("Vertical Only") },
          })
{
	grabInput.SetValueChangedCallback(OptionGrabInputChanged);
	experienceBar.SetValueChangedCallback(OptionExperienceBarChanged);
	enemyHealthBar.SetValueChangedCallback(OptionEnemyHealthBarChanged);
}
std::vector<OptionEntryBase *> GameplayOptions::GetEntries()
{
	return {
		&tickRate,
		&friendlyFire,
		&multiplayerFullQuests,
		&randomizeQuests,
		&theoQuest,
		&farmerQuest,
		&runInTown,
		&quickCast,
		&testBard,
		&testBarbarian,
		&experienceBar,
		&showItemGraphicsInStores,
		&showHealthValues,
		&showManaValues,
		&enemyHealthBar,
		&showMonsterType,
		&showItemLabels,
		&enableFloatingNumbers,
		&autoRefillBelt,
		&autoEquipWeapons,
		&autoEquipArmor,
		&autoEquipHelms,
		&autoEquipShields,
		&autoEquipJewelry,
		&autoGoldPickup,
		&autoElixirPickup,
		&autoOilPickup,
		&numHealPotionPickup,
		&numFullHealPotionPickup,
		&numManaPotionPickup,
		&numFullManaPotionPickup,
		&numRejuPotionPickup,
		&numFullRejuPotionPickup,
		&autoPickupInTown,
		&disableCripplingShrines,
		&adriaRefillsMana,
		&grabInput,
	};
}

OracoolOptions::OracoolOptions()
    : OptionCategoryBase("Oracool Edition", N_("Oracool Edition"), N_("Optional single-player features for Diablo Oracool Edition."))
    /*
     * The tuned balance, adopted wholesale from the user's own diablo.ini (2026-08-27: "look at this
     * ini file and make its setting the default in future releases").
     *
     * These five move together and are one decision rather than five, which is why they are noted
     * here rather than one by one: THREE TIMES the monsters and champion packs, and special items
     * an order of magnitude rarer than the previous defaults made them. Density supplies the kills;
     * rarity is what keeps a kill worth having. The old defaults had both dials turned up, which is
     * the combination that makes a Rare item ordinary within an hour.
     */
    , uniqueItemDropMultiplier("Unique Item Drop Multiplier", OptionEntryFlags::None, N_("Unique Item Drop Multiplier"), N_("Multiplies the chance that an eligible item drop becomes unique."), 1, { 1, 2, 5, 10, 25, 50, 100 })
    , monsterDensityPercent("Monster Density", OptionEntryFlags::CantChangeInGame, N_("Monster Density"), N_("Multiplies how many monsters a dungeon level scatters. 100 is vanilla."), 300, { 100, 150, 200, 250, 300 })
    // "2-6 by difficulty", not "one": the base count stopped being a single pack at 1.6.1, when the
    // user asked for 2-3 packs on Normal rising to 5-6 on Torment. The text said "100 is one" for a
    // day longer than it was true (self-audit, 2026-08-15).
    , lesserUniqueDensityPercent("Lesser Unique Density", OptionEntryFlags::CantChangeInGame, N_("Lesser Unique Density"), N_("Multiplies the champion packs a dungeon level hosts. 100 is the base 2-6 by difficulty."), 300, { 100, 150, 200, 250, 300 })
    // Not CantChangeInGame, unlike its two siblings: they decide what a level is BUILT with and so
    // cannot move once it exists, while the variant is derived per monster from a seed the level
    // already carries. Changing this mid-game simply changes what the next monster rolls.
    , monsterVariantChancePercent("Monster Variant Chance", OptionEntryFlags::None, N_("Monster Variant Chance"), N_("Multiplies how often a monster is a recoloured variant. 100 is the base 15-28% by difficulty; 0 turns them off."), 100, { 0, 100, 150, 200, 250, 300 })
    , unlockAllTownEntrances("Unlock All Town Entrances", OptionEntryFlags::CantChangeInGame, N_("Unlock All Town Entrances"), N_("Unlocks later dungeon entrances in town without level requirements."), true)
    , permanentInfravision("Permanent Infravision", OptionEntryFlags::None, N_("Permanent Infravision"), N_("Continuously reveals nearby monsters through walls."), false)
    , autoIdentifyDrops("Auto Identify Drops", OptionEntryFlags::None, N_("Auto Identify Drops"), N_("Automatically identifies newly generated item drops."), true)
    , resetStatsButton("Reset Stats Button", OptionEntryFlags::None, N_("Reset Stats Button"), N_("Adds a reset control to the character panel."), true)
    , autoPickupRange("Auto Pickup Range", OptionEntryFlags::None, N_("Auto Pickup Range"), N_("Search radius for enabled automatic-pickup categories."), 3, { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 })
    , autoScrollPickup("Auto Pickup Scrolls", OptionEntryFlags::None, N_("Auto Pickup Scrolls"), N_("Scrolls of every kind are automatically collected when in close proximity to the player."), true)
    , autoRunePickup("Auto Pickup Runes", OptionEntryFlags::None, N_("Auto Pickup Runes"), N_("Runes are automatically collected when in close proximity to the player."), true)
    , autoGemPickup("Auto Pickup Gems", OptionEntryFlags::None, N_("Auto Pickup Gems"), N_("Gems are automatically collected when in close proximity to the player."), true)
    , rareItemDropChance("Rare Item Drop Chance", OptionEntryFlags::None, N_("Rare Item Drop Chance"), N_("Percent chance an eligible drop becomes a Rare item after failing its Unique roll."), 2, { 0, 2, 4, 6, 8, 10, 15, 20, 30, 50, 75, 100 })
    , uniqueDropChancePercent("Unique Drop Chance Percent", OptionEntryFlags::None, N_("Unique Drop Chance Percent"), N_("Scales the chance an eligible drop becomes a unique item. 100 is vanilla; lower narrows it."), 50, { 10, 25, 50, 75, 100 })
    , lastReadiedSpellLeft("Last Readied Spell Left", OptionEntryFlags::Invisible, "Last Readied Spell Left", "The left-button skill a new character starts with, remembered from the last one.", 0, { })
    , lastReadiedSpellRight("Last Readied Spell Right", OptionEntryFlags::Invisible, "Last Readied Spell Right", "The right-button skill a new character starts with, remembered from the last one.", 0, { })
    , championExtraDropChance("Champion Extra Drop Chance", OptionEntryFlags::None, N_("Champion Extra Drop Chance"), N_("Percent chance a champion monster rolls a SECOND item on death. 100 is always, which is what it used to be."), 25, { 0, 10, 25, 50, 75, 100 })
    , buffedUniqueItemDropChance("Buffed Unique Item Drop Chance", OptionEntryFlags::None, N_("Buffed Unique Item Drop Chance"), N_("Percent chance an eligible drop becomes a Buffed Unique, checked before Rare."), 1, { 0, 1, 2, 3, 4, 5, 8, 10, 15, 20, 30, 50 })
    , primalItemDropChance("Primal Item Drop Chance", OptionEntryFlags::None, N_("Primal Item Drop Chance"), N_("Percent chance an eligible drop becomes a Primal item, checked before Buffed Unique."), 1, { 0, 1, 2, 3, 4, 5, 8, 10, 15, 20, 30 })
    , griswoldPremiumRefresh("Griswold Premium Refresh", OptionEntryFlags::None, N_("Griswold Premium Refresh"), N_("Adds a free Refresh action to Griswold's Premium Items."), true)
    , refreshUntilButton("Griswold Refresh Until Button", OptionEntryFlags::None, N_("Griswold Refresh Until Button"), N_("Searches Griswold's Premium Items for configured item names."), false)
    , refreshUntilTimeoutSeconds("Griswold Refresh Until Timeout Seconds", OptionEntryFlags::None, N_("Griswold Refresh Until Timeout Seconds"), N_("Maximum search duration; zero relies on the hard iteration limit."), 5, { 0, 1, 2, 3, 5, 10, 15, 30, 60 })
    , griswoldRestoreHealth("Griswold Restore Health", OptionEntryFlags::None, N_("Griswold Restore Health"), N_("Silently restores health when Griswold's menu opens."), true)
    , griswoldRestoreMana("Griswold Restore Mana", OptionEntryFlags::None, N_("Griswold Restore Mana"), N_("Silently restores mana when Griswold's menu opens."), true)
    , griswoldSellUniqueItems("Griswold Sell Unique Items", OptionEntryFlags::None, N_("Griswold Sell Unique Items"), N_("Adds a separate unique-item shop to Griswold."), false)
    , griswoldUniqueItemPriceMultiplier("Griswold Unique Item Price Multiplier", OptionEntryFlags::None, N_("Griswold Unique Item Price Multiplier"), N_("Multiplier applied to a unique item's normal sell value."), 20, { 1, 2, 3, 4, 5, 10, 15, 20 })
    , griswoldSellRareItems("Griswold Sell Rare Items", OptionEntryFlags::None, N_("Griswold Sell Rare Items"), N_("Adds a separate rare-item shop to Griswold."), true)
    , griswoldSellSetItems("Griswold Sell Set Items", OptionEntryFlags::None, N_("Griswold Sell Set Items"), N_("Adds a separate named-set-item shop to Griswold."), false)
    , shopStockRefresh("Shop Stock Refresh", OptionEntryFlags::None, N_("Shop Stock Refresh"), N_("Adds a free Refresh action to the Basic, Rare and Supplies shelves."), true)
    , griswoldPremiumIgnoreAffixLevelLimits("Griswold Premium Ignore Affix Level Limits", OptionEntryFlags::None, N_("Griswold Premium Ignore Affix Level Limits"), N_("Allows compatible Premium affixes regardless of their normal quality-level requirement."), false)
    , griswoldPremiumIgnorePriceLimits("Griswold Premium Ignore Price Limits", OptionEntryFlags::None, N_("Griswold Premium Ignore Price Limits"), N_("Prevents valid Premium items from being rejected by the normal price ceiling."), false)
    , griswoldSellIgnoresBelt("Griswold Sell Ignores Belt", OptionEntryFlags::None, N_("Griswold Sell Ignores Belt"), N_("Griswold's and Adria's sell lists skip belt items - only the backpack is offered."), true)
    , panelDocking("Panel Docking", OptionEntryFlags::None, N_("Panel Docking"), N_("Where the inventory, character sheet and other side panels sit on a screen taller than they are."), PanelDocking::Middle,
          {
              { PanelDocking::Bottom, N_("Bottom") },
              { PanelDocking::Middle, N_("Middle") },
          })
    , autoSave("Auto Save", OptionEntryFlags::None, N_("Auto Save"), N_("Enables Oracool automatic saving in single-player."), true)
    , autoSaveIntervalMinutes("Auto Save Interval Minutes", OptionEntryFlags::None, N_("Auto Save Interval Minutes"), N_("Minutes between periodic automatic saves."), 5, { 1, 2, 3, 5, 10, 15, 30, 60 })
    , autoSaveOnLevelChange("Auto Save on Level Change", OptionEntryFlags::None, N_("Auto Save on Level Change"), N_("Saves after entering another dungeon level or returning to town."), true)
    , autoSaveOnItemPickup("Auto Save on Item Pickup", OptionEntryFlags::None, N_("Auto Save on Item Pickup"), N_("Schedules a save after an item or gold enters inventory."), true)
    , autoSaveOnStorePurchase("Auto Save on Store Purchase", OptionEntryFlags::None, N_("Auto Save on Store Purchase"), N_("Schedules a save after a successful store purchase."), true)
    , autoSaveOnExperienceGain("Auto Save on Experience Gain", OptionEntryFlags::None, N_("Auto Save on Experience Gain"), N_("Saves instantly whenever your character gains experience."), true)
    , autoSaveOnStatPointSpent("Auto Save on Stat Point Spent", OptionEntryFlags::None, N_("Auto Save on Stat Point Spent"), N_("Saves instantly whenever a stat point is spent on the character panel."), true)
    , autoSaveOnSkillChange("Auto Save on Skill Change", OptionEntryFlags::None, N_("Auto Save on Skill Change"), N_("Saves instantly whenever a skill point, passive slot, aura or readied skill changes."), true)
    , autoSaveOnEquipmentChange("Auto Save on Equipment Change", OptionEntryFlags::None, N_("Auto Save on Equipment Change"), N_("Saves instantly whenever equipment is worn or removed."), true)
    , autoSaveOnItemDrop("Auto Save on Item Drop", OptionEntryFlags::None, N_("Auto Save on Item Drop"), N_("Saves instantly whenever an item is dropped on the ground."), true)
    , autoSaveOnStashChange("Auto Save on Stash Change", OptionEntryFlags::None, N_("Auto Save on Stash Change"), N_("Saves instantly whenever the Stash's contents change."), true)
    , autoSaveOnStoreTransaction("Auto Save on Store Transaction", OptionEntryFlags::None, N_("Auto Save on Store Transaction"), N_("Saves instantly after selling, repairing, recharging, or identifying an item at a shop."), true)
    , autoSaveOnShrineActivation("Auto Save on Shrine Activation", OptionEntryFlags::None, N_("Auto Save on Shrine Activation"), N_("Saves instantly after activating a Shrine, Goat Shrine, Cauldron, or Fountain."), true)
    , autoSaveOnBookRead("Auto Save on Book Read", OptionEntryFlags::None, N_("Auto Save on Book Read"), N_("Saves instantly after reading a spell book."), true)
    , autoSaveOnItemBreak("Auto Save on Item Break", OptionEntryFlags::None, N_("Auto Save on Item Break"), N_("Saves instantly when a weapon or piece of armor breaks."), true)
    , autoSaveOnWaypointActivation("Auto Save on Waypoint Activation", OptionEntryFlags::None, N_("Auto Save on Waypoint Activation"), N_("Saves instantly whenever a waypoint sigil's travel menu is opened."), true)
    , autoSaveNotification("Auto Save Notification", OptionEntryFlags::None, N_("Auto Save Notification"), N_("Displays a brief \"Game Saved\" message after an automatic save."), true)
    , difficultyLevelGate("Difficulty Level Gate", OptionEntryFlags::None, N_("Difficulty Level Gate"), N_("Requires a minimum character level to start a game on Nightmare, Hell, or Torment."), true)
    , tormentDifficultyMultiplier("Torment Difficulty Multiplier", OptionEntryFlags::None, N_("Torment Difficulty Multiplier"), N_("How much harder Torment is than Hell, applied on top of Hell's own monster and treasure scaling."), 20, { 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50 })
    , miniMapEnabled("Mini-Map", OptionEntryFlags::None, N_("Mini-Map"), N_("Shows an always-on mini-map in the top-right corner during gameplay. Independent of TAB, which still opens/closes the normal full map."), true)
    , hudPlateArt("HUD Plate Art", OptionEntryFlags::None, N_("HUD Plate Art"), N_("Draws the stone plate behind the belt and the two skill wells. Off leaves the belt items, skill icons and both orbs in place and removes the plate and everything painted into it - the cell frames, the well rims and the Menu/Portal button faces."), false)
    , eventLog("Event Log", OptionEntryFlags::None, N_("Event Log"), N_("Shows a toggleable button above the durability-warning icons that opens a timestamped log of noteworthy session events."), true)
    , balanceTelemetry("Balance Telemetry", OptionEntryFlags::None, N_("Balance Telemetry"), N_("Appends kills, deaths and item pickups to balance_telemetry.csv beside your saves, as tuning data for balancing the mod. Local file only; nothing leaves your machine."), true)
    , vendorTieredStockChance("Vendor Tiered Stock Chance", OptionEntryFlags::None, N_("Vendor Tiered Stock Chance"), N_("Percent chance a vendor item is offered at a base tier above Normal. The tier follows the game difficulty."), 35, { 0, 5, 10, 15, 20, 25, 35, 50, 65, 80, 100 })
    , nakedHeroes("Naked Heroes", OptionEntryFlags::None, N_("Naked Heroes"), N_("New heroes start with no equipment, no potions and no gold. Read once, when the character is created."), true)
    , gameClock("Game Clock", OptionEntryFlags::None, N_("Game Clock"), N_("Shows the current real-world time just below the mini-map's left edge."), true)
    , gameClock12HourFormat("Game Clock 12 Hour Format", OptionEntryFlags::None, N_("Game Clock 12 Hour Format"), N_("Shows the Game Clock in 12-hour format with an AM/PM suffix instead of 24-hour format."), false)
    , gameSpeedReadout("Game Speed Readout", OptionEntryFlags::None, N_("Game Speed Readout"), N_("Whether the game speed is shown under the clock. Blink shows it for one second whenever F9 or F10 changes it."), GameSpeedReadout::Blink,
          {
              { GameSpeedReadout::Off, N_("Off") },
              { GameSpeedReadout::On, N_("On") },
              { GameSpeedReadout::Blink, N_("Blink") },
          })
    , gradualHealing("Gradual Healing", OptionEntryFlags::None, N_("Gradual Healing"), N_("Potion of Healing and Potion of Mana restore their amount gradually over a few seconds instead of instantly. Full Healing/Full Mana Potions are unaffected."), true)
    , xpCounter("XP Counter", OptionEntryFlags::None, N_("XP Counter"), N_("Shows the experience remaining until your next level just below the mini-map."), true)
    , xpGainIndicator("XP Gain Indicator", OptionEntryFlags::None, N_("XP Gain Indicator"), N_("Briefly flashes the experience gained just below the XP Counter."), true)
    , remainingMonsterXpButton("Remaining Monster XP Button", OptionEntryFlags::None, N_("Remaining Monster XP Button"), N_("Press and hold the XP Counter to see the total experience worth of every monster still alive on this level."), true)
    , monsterRangeHighlight("Monster Range Highlight", OptionEntryFlags::None, N_("Monster Range Highlight"), N_("Monsters within this many tiles get the same red outline shown when hovering them."), 0, { 0, 1, 2, 3, 4, 5 })
    , monsterWallOutline("Monster Wall Outline", OptionEntryFlags::None, N_("Monster Wall Outline"), N_("Draws a red outline on monsters hidden behind walls or other architecture, so you can tell they're there."), false)
    , questLogRevealAll("Quest Log Reveal All", OptionEntryFlags::None, N_("Quest Log Reveal All"), N_("Every quest available this session appears in the quest log from the start, instead of only after you discover it. Quests still work exactly as before - this just previews what's out there."), true)
    , dungeonZoomLevel("Dungeon Zoom Level", OptionEntryFlags::Invisible, "Dungeon Zoom Level", "Continuous dungeon-view zoom, in tenths (10-20 = 1.0x-2.0x). Set live via mouse wheel / middle-click.", 10)
{
}

std::vector<OptionEntryBase *> OracoolOptions::GetEntries()
{
	std::vector<OptionEntryBase *> entries = {
		&uniqueItemDropMultiplier,
		&monsterDensityPercent,
		&lesserUniqueDensityPercent,
		&monsterVariantChancePercent,
		&unlockAllTownEntrances,
		&permanentInfravision,
		&autoIdentifyDrops,
		&resetStatsButton,
		&autoPickupRange,
		&autoScrollPickup,
		&autoRunePickup,
		&autoGemPickup,
		&rareItemDropChance,
		&uniqueDropChancePercent,
		&championExtraDropChance,
		&lastReadiedSpellLeft,
		&lastReadiedSpellRight,
		&buffedUniqueItemDropChance,
		&primalItemDropChance,
		&griswoldPremiumRefresh,
		&refreshUntilButton,
		&refreshUntilTimeoutSeconds,
		&griswoldRestoreHealth,
		&griswoldRestoreMana,
		&griswoldSellUniqueItems,
		&griswoldUniqueItemPriceMultiplier,
		&griswoldSellRareItems,
		&griswoldSellSetItems,
		&shopStockRefresh,
		&griswoldPremiumIgnoreAffixLevelLimits,
		&griswoldPremiumIgnorePriceLimits,
		&griswoldSellIgnoresBelt,
		&panelDocking,
		&autoSave,
		&autoSaveIntervalMinutes,
		&autoSaveOnLevelChange,
		&autoSaveOnItemPickup,
		&autoSaveOnStorePurchase,
		&autoSaveOnExperienceGain,
		&autoSaveOnStatPointSpent,
		&autoSaveOnSkillChange,
		&autoSaveOnEquipmentChange,
		&autoSaveOnItemDrop,
		&autoSaveOnStashChange,
		&autoSaveOnStoreTransaction,
		&autoSaveOnShrineActivation,
		&autoSaveOnBookRead,
		&autoSaveOnItemBreak,
		&autoSaveOnWaypointActivation,
		&autoSaveNotification,
		&difficultyLevelGate,
		&tormentDifficultyMultiplier,
		&miniMapEnabled,
		&hudPlateArt,
		&eventLog,
		// External audit PO-02, 2026-08-30: declared and consumed (oracool/telemetry.cpp) but never
		// registered, so it was neither loaded nor saved nor listed in Settings - permanently stuck
		// on at its default with no way to turn it off. Registering it is the whole fix; loading
		// walks this list, and SaveOptions' backstop now writes anything the canonical section
		// misses.
		&balanceTelemetry,
		&vendorTieredStockChance,
		&nakedHeroes,
		&gameClock,
		&gameClock12HourFormat,
		&gameSpeedReadout,
		&gradualHealing,
		&xpCounter,
		&xpGainIndicator,
		&remainingMonsterXpButton,
		&monsterRangeHighlight,
		&monsterWallOutline,
		&questLogRevealAll,
		&dungeonZoomLevel,
	};

	// Oracool: user request - show the settings menu's Oracool Edition category alphabetically by
	// display name rather than the rough chronological-added order above (which reflects nothing
	// the player cares about and made a specific setting hard to find in a list of 30+ options).
	std::sort(entries.begin(), entries.end(), [](const OptionEntryBase *a, const OptionEntryBase *b) {
		return a->GetName() < b->GetName();
	});
	return entries;
}

string_view OptionEntryTormentMultiplier::GetListDescription(size_t index) const
{
	if (descriptionCache.empty()) {
		for (size_t i = 0; i < GetListSize(); i++)
			descriptionCache.push_back(fmt::format("{:.1f}", GetEntryValue(i) / 10.0f));
	}
	return descriptionCache[index];
}

string_view OptionEntryRangeOrOff::GetListDescription(size_t index) const
{
	if (descriptionCache.empty()) {
		for (size_t i = 0; i < GetListSize(); i++)
			descriptionCache.push_back(GetEntryValue(i) == 0 ? "OFF" : StrCat(GetEntryValue(i)));
	}
	return descriptionCache[index];
}

ControllerOptions::ControllerOptions()
    : OptionCategoryBase("Controller", N_("Controller"), N_("Controller Settings"))
{
}
std::vector<OptionEntryBase *> ControllerOptions::GetEntries()
{
	return {};
}

NetworkOptions::NetworkOptions()
    : OptionCategoryBase("Network", N_("Network"), N_("Network Settings"))
    , port("Port", OptionEntryFlags::Invisible, "Port", "What network port to use.", 6112)
{
}
std::vector<OptionEntryBase *> NetworkOptions::GetEntries()
{
	return {
		&port,
	};
}

ChatOptions::ChatOptions()
    : OptionCategoryBase("NetMsg", N_("Chat"), N_("Chat Settings"))
{
}
std::vector<OptionEntryBase *> ChatOptions::GetEntries()
{
	return {};
}

OptionEntryLanguageCode::OptionEntryLanguageCode()
    : OptionEntryListBase("Code", OptionEntryFlags::CantChangeInGame | OptionEntryFlags::RecreateUI, N_("Language"), N_("Define what language to use in game."))
{
}
void OptionEntryLanguageCode::LoadFromIni(string_view category)
{
	if (GetIniValue(category, key, szCode, sizeof(szCode))) {
		if (HasTranslation(szCode)) {
			// User preferred language is available
			return;
		}
	}

	// Might be a first run or the user has attempted to load a translation that doesn't exist via manual ini edit. Try
	//  find a best fit from the platform locale information.
	std::vector<std::string> locales = GetLocales();

	// So that the correct language is shown in the settings menu for users with US english set as a preferred language
	//  we need to replace the "en_US" locale code with the neutral string "en" as expected by the available options
	std::replace(locales.begin(), locales.end(), std::string { "en_US" }, std::string { "en" });

	// Insert non-regional locale codes after the last regional variation so we fallback to neutral translations if no
	//  regional translation exists that meets user preferences.
	for (auto localeIter = locales.rbegin(); localeIter != locales.rend(); localeIter++) {
		auto regionSeparator = localeIter->find('_');
		if (regionSeparator != std::string::npos) {
			std::string neutralLocale = localeIter->substr(0, regionSeparator);
			if (std::find(locales.rbegin(), localeIter, neutralLocale) == localeIter) {
				localeIter = std::make_reverse_iterator(locales.insert(localeIter.base(), neutralLocale));
			}
		}
	}

	LogVerbose("Found user preferred locales: {}", fmt::join(locales, ", "));

	for (const auto &locale : locales) {
		LogVerbose("Trying to load translation: {}", locale);
		if (HasTranslation(locale)) {
			LogVerbose("Best match locale: {}", locale);
			CopyUtf8(szCode, locale, sizeof(szCode));
			return;
		}
	}

	LogVerbose("No suitable translation found");
	strcpy(szCode, "en");
}
void OptionEntryLanguageCode::SaveToIni(string_view category) const
{
	SetIniValue(category, key, szCode);
}

void OptionEntryLanguageCode::CheckLanguagesAreInitialized() const
{
	if (!languages.empty())
		return;

	// Add well-known supported languages
	languages.emplace_back("bg", "Български");
	languages.emplace_back("cs", "Čeština");
	languages.emplace_back("da", "Dansk");
	languages.emplace_back("de", "Deutsch");
	languages.emplace_back("el", "Ελληνικά");
	languages.emplace_back("en", "English");
	languages.emplace_back("es", "Español");
	languages.emplace_back("fr", "Français");
	languages.emplace_back("hr", "Hrvatski");
	languages.emplace_back("hu", "Magyar");
	languages.emplace_back("it", "Italiano");

	if (HaveExtraFonts()) {
		languages.emplace_back("ja", "日本語");
		languages.emplace_back("ko", "한국어");
	}

	languages.emplace_back("pl", "Polski");
	languages.emplace_back("pt_BR", "Português do Brasil");
	languages.emplace_back("ro", "Română");
	languages.emplace_back("ru", "Русский");
	languages.emplace_back("sv", "Svenska");
	languages.emplace_back("tr", "Türkçe");
	languages.emplace_back("uk", "Українська");

	if (HaveExtraFonts()) {
		languages.emplace_back("zh_CN", "汉语");
		languages.emplace_back("zh_TW", "漢語");
	}

	// Ensures that the ini specified language is present in languages list even if unknown (for example if someone starts to translate a new language)
	if (std::find_if(languages.begin(), languages.end(), [this](const auto &x) { return x.first == this->szCode; }) == languages.end()) {
		languages.emplace_back(szCode, szCode);
	}
}

size_t OptionEntryLanguageCode::GetListSize() const
{
	CheckLanguagesAreInitialized();
	return languages.size();
}
string_view OptionEntryLanguageCode::GetListDescription(size_t index) const
{
	CheckLanguagesAreInitialized();
	return languages[index].second;
}
size_t OptionEntryLanguageCode::GetActiveListIndex() const
{
	CheckLanguagesAreInitialized();
	auto found = std::find_if(languages.begin(), languages.end(), [this](const auto &x) { return x.first == this->szCode; });
	if (found == languages.end())
		return 0;
	return std::distance(languages.begin(), found);
}
void OptionEntryLanguageCode::SetActiveListIndex(size_t index)
{
	CopyUtf8(szCode, languages[index].first, sizeof(szCode));
	NotifyValueChanged();
}

LanguageOptions::LanguageOptions()
    : OptionCategoryBase("Language", N_("Language"), N_("Language Settings"))
{
	code.SetValueChangedCallback(OptionLanguageCodeChanged);
}
std::vector<OptionEntryBase *> LanguageOptions::GetEntries()
{
	return {
		&code,
	};
}

KeymapperOptions::KeymapperOptions()
    : OptionCategoryBase("Keymapping", N_("Keymapping"), N_("Keymapping Settings"))
{
	// Insert all supported keys: a-z, 0-9 and F1-F24.
	keyIDToKeyName.reserve(('Z' - 'A' + 1) + ('9' - '0' + 1) + 12);
	for (char c = 'A'; c <= 'Z'; ++c) {
		keyIDToKeyName.emplace(c, std::string(1, c));
	}
	for (char c = '0'; c <= '9'; ++c) {
		keyIDToKeyName.emplace(c, std::string(1, c));
	}
	for (int i = 0; i < 12; ++i) {
		keyIDToKeyName.emplace(SDLK_F1 + i, StrCat("F", i + 1));
	}
	for (int i = 0; i < 12; ++i) {
		keyIDToKeyName.emplace(SDLK_F13 + i, StrCat("F", i + 13));
	}

	keyIDToKeyName.emplace(SDLK_LALT, "LALT");
	keyIDToKeyName.emplace(SDLK_RALT, "RALT");

	keyIDToKeyName.emplace(SDLK_SPACE, "SPACE");

	keyIDToKeyName.emplace(SDLK_RCTRL, "RCONTROL");
	keyIDToKeyName.emplace(SDLK_LCTRL, "LCONTROL");

	keyIDToKeyName.emplace(SDLK_PRINTSCREEN, "PRINT");
	keyIDToKeyName.emplace(SDLK_PAUSE, "PAUSE");
	keyIDToKeyName.emplace(SDLK_TAB, "TAB");
	keyIDToKeyName.emplace(SDL_BUTTON_MIDDLE | KeymapperMouseButtonMask, "MMOUSE");
	keyIDToKeyName.emplace(SDL_BUTTON_X1 | KeymapperMouseButtonMask, "X1MOUSE");
	keyIDToKeyName.emplace(SDL_BUTTON_X2 | KeymapperMouseButtonMask, "X2MOUSE");
	keyIDToKeyName.emplace(MouseScrollUpButton, "SCROLLUPMOUSE");
	keyIDToKeyName.emplace(MouseScrollDownButton, "SCROLLDOWNMOUSE");
	keyIDToKeyName.emplace(MouseScrollLeftButton, "SCROLLLEFTMOUSE");
	keyIDToKeyName.emplace(MouseScrollRightButton, "SCROLLRIGHTMOUSE");

	keyIDToKeyName.emplace(SDLK_BACKQUOTE, "`");
	keyIDToKeyName.emplace(SDLK_LEFTBRACKET, "[");
	keyIDToKeyName.emplace(SDLK_RIGHTBRACKET, "]");
	keyIDToKeyName.emplace(SDLK_BACKSLASH, "\\");
	keyIDToKeyName.emplace(SDLK_SEMICOLON, ";");
	keyIDToKeyName.emplace(SDLK_QUOTE, "'");
	keyIDToKeyName.emplace(SDLK_COMMA, ",");
	keyIDToKeyName.emplace(SDLK_PERIOD, ".");
	keyIDToKeyName.emplace(SDLK_SLASH, "/");

	keyIDToKeyName.emplace(SDLK_BACKSPACE, "BACKSPACE");
	keyIDToKeyName.emplace(SDLK_CAPSLOCK, "CAPSLOCK");
	keyIDToKeyName.emplace(SDLK_SCROLLLOCK, "SCROLLLOCK");
	keyIDToKeyName.emplace(SDLK_INSERT, "INSERT");
	keyIDToKeyName.emplace(SDLK_DELETE, "DELETE");
	keyIDToKeyName.emplace(SDLK_HOME, "HOME");
	keyIDToKeyName.emplace(SDLK_END, "END");

	keyIDToKeyName.emplace(SDLK_KP_DIVIDE, "KEYPAD /");
	keyIDToKeyName.emplace(SDLK_KP_MULTIPLY, "KEYPAD *");
	keyIDToKeyName.emplace(SDLK_KP_ENTER, "KEYPAD ENTER");
	keyIDToKeyName.emplace(SDLK_KP_PERIOD, "KEYPAD DECIMAL");

	keyNameToKeyID.reserve(keyIDToKeyName.size());
	for (const auto &kv : keyIDToKeyName) {
		keyNameToKeyID.emplace(kv.second, kv.first);
	}
}

std::vector<OptionEntryBase *> KeymapperOptions::GetEntries()
{
	std::vector<OptionEntryBase *> entries;
	for (Action &action : actions) {
		entries.push_back(&action);
	}
	return entries;
}

KeymapperOptions::Action::Action(string_view key, const char *name, const char *description, uint32_t defaultKey, std::function<void()> actionPressed, std::function<void()> actionReleased, std::function<bool()> enable, unsigned index)
    : OptionEntryBase(key, OptionEntryFlags::None, name, description)
    , defaultKey(defaultKey)
    , actionPressed(std::move(actionPressed))
    , actionReleased(std::move(actionReleased))
    , enable(std::move(enable))
    , dynamicIndex(index)
{
	if (index != 0) {
		dynamicKey = fmt::format(fmt::runtime(fmt::string_view(key.data(), key.size())), index);
		this->key = dynamicKey;
	}
}

string_view KeymapperOptions::Action::GetName() const
{
	if (dynamicIndex == 0)
		return _(name);
	dynamicName = fmt::format(fmt::runtime(_(name)), dynamicIndex);
	return dynamicName;
}

void KeymapperOptions::Action::LoadFromIni(string_view category)
{
	std::array<char, 64> result;
	if (!GetIniValue(category.data(), key.data(), result.data(), result.size())) {
		SetValue(defaultKey);
		return; // Use the default key if no key has been set.
	}

	std::string readKey = result.data();
	if (readKey.empty()) {
		SetValue(SDLK_UNKNOWN);
		return;
	}

	auto keyIt = sgOptions.Keymapper.keyNameToKeyID.find(readKey);
	if (keyIt == sgOptions.Keymapper.keyNameToKeyID.end()) {
		// Use the default key if the key is unknown.
		Log("Keymapper: unknown key '{}'", readKey);
		SetValue(defaultKey);
		return;
	}

	// Store the key in action.key and in the map so we can save() the
	// actions while keeping the same order as they have been added.
	SetValue(keyIt->second);
}
void KeymapperOptions::Action::SaveToIni(string_view category) const
{
	if (boundKey == SDLK_UNKNOWN) {
		// Just add an empty config entry if the action is unbound.
		SetIniValue(category.data(), key.data(), "");
	}
	auto keyNameIt = sgOptions.Keymapper.keyIDToKeyName.find(boundKey);
	if (keyNameIt == sgOptions.Keymapper.keyIDToKeyName.end()) {
		LogVerbose("Keymapper: no name found for key '{}'", key);
		return;
	}
	SetIniValue(category.data(), key.data(), keyNameIt->second.c_str());
}

string_view KeymapperOptions::Action::GetValueDescription() const
{
	if (boundKey == SDLK_UNKNOWN)
		return "";
	auto keyNameIt = sgOptions.Keymapper.keyIDToKeyName.find(boundKey);
	if (keyNameIt == sgOptions.Keymapper.keyIDToKeyName.end()) {
		return "";
	}
	return keyNameIt->second;
}

bool KeymapperOptions::Action::SetValue(int value)
{
	if (value != SDLK_UNKNOWN && sgOptions.Keymapper.keyIDToKeyName.find(value) == sgOptions.Keymapper.keyIDToKeyName.end()) {
		// Ignore invalid key values
		return false;
	}

	// Remove old key
	if (boundKey != SDLK_UNKNOWN) {
		sgOptions.Keymapper.keyIDToAction.erase(boundKey);
		boundKey = SDLK_UNKNOWN;
	}

	// Add new key
	if (value != SDLK_UNKNOWN) {
		auto it = sgOptions.Keymapper.keyIDToAction.find(value);
		if (it != sgOptions.Keymapper.keyIDToAction.end()) {
			// Warn about overwriting keys.
			Log("Keymapper: key '{}' is already bound to action '{}', overwriting", value, it->second.get().name);
			it->second.get().boundKey = SDLK_UNKNOWN;
		}

		sgOptions.Keymapper.keyIDToAction.insert_or_assign(value, *this);
		boundKey = value;
	}

	return true;
}

void KeymapperOptions::AddAction(string_view key, const char *name, const char *description, uint32_t defaultKey, std::function<void()> actionPressed, std::function<void()> actionReleased, std::function<bool()> enable, unsigned index)
{
	actions.emplace_front(key, name, description, defaultKey, std::move(actionPressed), std::move(actionReleased), std::move(enable), index);
}

void KeymapperOptions::CommitActions()
{
	actions.reverse();
}

void KeymapperOptions::KeyPressed(uint32_t key) const
{
	if (key >= SDLK_a && key <= SDLK_z) {
		key -= 'a' - 'A';
	}

	auto it = keyIDToAction.find(key);
	if (it == keyIDToAction.end())
		return; // Ignore unmapped keys.

	const Action &action = it->second.get();

	// Check that the action can be triggered and that the chat textbox is not
	// open.
	if (!action.actionPressed || (action.enable && !action.enable()) || talkflag)
		return;

	action.actionPressed();
}

void KeymapperOptions::KeyReleased(SDL_Keycode key) const
{
	if (key >= SDLK_a && key <= SDLK_z) {
		key = static_cast<SDL_Keycode>(static_cast<Sint32>(key) - ('a' - 'A'));
	}
	auto it = keyIDToAction.find(key);
	if (it == keyIDToAction.end())
		return; // Ignore unmapped keys.

	const Action &action = it->second.get();

	// Check that the action can be triggered and that the chat or gold textbox is not
	// open. If either of those textboxes are open, only return if the key can be used for entry into the box
	if (!action.actionReleased || (action.enable && !action.enable()) || ((talkflag && IsTextEntryKey(key)) || (DropGoldFlag && IsNumberEntryKey(key))))
		return;

	action.actionReleased();
}

bool KeymapperOptions::IsTextEntryKey(SDL_Keycode vkey) const
{
	return IsAnyOf(vkey, SDLK_ESCAPE, SDLK_RETURN, SDLK_KP_ENTER, SDLK_BACKSPACE, SDLK_DOWN, SDLK_UP) || (vkey >= SDLK_SPACE && vkey <= SDLK_z);
}

bool KeymapperOptions::IsNumberEntryKey(SDL_Keycode vkey) const
{
	return ((vkey >= SDLK_0 && vkey <= SDLK_9) || vkey == SDLK_BACKSPACE);
}

string_view KeymapperOptions::KeyNameForAction(string_view actionName) const
{
	for (const Action &action : actions) {
		if (action.key == actionName && action.boundKey != SDLK_UNKNOWN) {
			return action.GetValueDescription();
		}
	}
	return "";
}

uint32_t KeymapperOptions::KeyForAction(string_view actionName) const
{
	for (const Action &action : actions) {
		if (action.key == actionName && action.boundKey != SDLK_UNKNOWN) {
			return action.boundKey;
		}
	}
	return SDLK_UNKNOWN;
}

PadmapperOptions::PadmapperOptions()
    : OptionCategoryBase("Padmapping", N_("Padmapping"), N_("Padmapping Settings"))
    , buttonToButtonName { {
	      /*ControllerButton_NONE*/ {},
	      /*ControllerButton_IGNORE*/ {},
	      /*ControllerButton_AXIS_TRIGGERLEFT*/ "LT",
	      /*ControllerButton_AXIS_TRIGGERRIGHT*/ "RT",
	      /*ControllerButton_BUTTON_A*/ "A",
	      /*ControllerButton_BUTTON_B*/ "B",
	      /*ControllerButton_BUTTON_X*/ "X",
	      /*ControllerButton_BUTTON_Y*/ "Y",
	      /*ControllerButton_BUTTON_LEFTSTICK*/ "LS",
	      /*ControllerButton_BUTTON_RIGHTSTICK*/ "RS",
	      /*ControllerButton_BUTTON_LEFTSHOULDER*/ "LB",
	      /*ControllerButton_BUTTON_RIGHTSHOULDER*/ "RB",
	      /*ControllerButton_BUTTON_START*/ "Start",
	      /*ControllerButton_BUTTON_BACK*/ "Select",
	      /*ControllerButton_BUTTON_DPAD_UP*/ "Up",
	      /*ControllerButton_BUTTON_DPAD_DOWN*/ "Down",
	      /*ControllerButton_BUTTON_DPAD_LEFT*/ "Left",
	      /*ControllerButton_BUTTON_DPAD_RIGHT*/ "Right",
	  } }
{
	buttonNameToButton.reserve(buttonToButtonName.size());
	for (size_t i = 0; i < buttonToButtonName.size(); ++i) {
		buttonNameToButton.emplace(buttonToButtonName[i], static_cast<ControllerButton>(i));
	}
}

std::vector<OptionEntryBase *> PadmapperOptions::GetEntries()
{
	std::vector<OptionEntryBase *> entries;
	for (Action &action : actions) {
		entries.push_back(&action);
	}
	return entries;
}

PadmapperOptions::Action::Action(string_view key, const char *name, const char *description, ControllerButtonCombo defaultInput, std::function<void()> actionPressed, std::function<void()> actionReleased, std::function<bool()> enable, unsigned index)
    : OptionEntryBase(key, OptionEntryFlags::None, name, description)
    , defaultInput(defaultInput)
    , actionPressed(std::move(actionPressed))
    , actionReleased(std::move(actionReleased))
    , enable(std::move(enable))
    , dynamicIndex(index)
{
	if (index != 0) {
		dynamicKey = fmt::format(fmt::runtime(fmt::string_view(key.data(), key.size())), index);
		this->key = dynamicKey;
	}
}

string_view PadmapperOptions::Action::GetName() const
{
	if (dynamicIndex == 0)
		return _(name);
	dynamicName = fmt::format(fmt::runtime(_(name)), dynamicIndex);
	return dynamicName;
}

void PadmapperOptions::Action::LoadFromIni(string_view category)
{
	std::array<char, 64> result;
	if (!GetIniValue(category.data(), key.data(), result.data(), result.size())) {
		SetValue(defaultInput);
		return; // Use the default button combo if no mapping has been set.
	}

	std::string modName;
	std::string buttonName;
	auto parts = SplitByChar(result.data(), '+');
	auto it = parts.begin();
	if (it == parts.end()) {
		SetValue(ControllerButtonCombo {});
		return;
	}
	buttonName = std::string(*it);
	if (++it != parts.end()) {
		modName = std::move(buttonName);
		buttonName = std::string(*it);
	}

	ControllerButtonCombo input {};
	if (!modName.empty()) {
		auto modifierIt = sgOptions.Padmapper.buttonNameToButton.find(modName);
		if (modifierIt == sgOptions.Padmapper.buttonNameToButton.end()) {
			// Use the default button combo if the modifier name is unknown.
			LogWarn("Padmapper: unknown button '{}'", modName);
			SetValue(defaultInput);
			return;
		}
		input.modifier = modifierIt->second;
	}

	auto buttonIt = sgOptions.Padmapper.buttonNameToButton.find(buttonName);
	if (buttonIt == sgOptions.Padmapper.buttonNameToButton.end()) {
		// Use the default button combo if the button name is unknown.
		LogWarn("Padmapper: unknown button '{}'", buttonName);
		SetValue(defaultInput);
		return;
	}
	input.button = buttonIt->second;

	// Store the input in action.boundInput and in the map so we can save()
	// the actions while keeping the same order as they have been added.
	SetValue(input);
}
void PadmapperOptions::Action::SaveToIni(string_view category) const
{
	if (boundInput.button == ControllerButton_NONE) {
		// Just add an empty config entry if the action is unbound.
		SetIniValue(category.data(), key.data(), "");
		return;
	}
	std::string inputName = sgOptions.Padmapper.buttonToButtonName[static_cast<size_t>(boundInput.button)];
	if (inputName.empty()) {
		LogVerbose("Padmapper: no name found for key '{}'", key);
		return;
	}
	if (boundInput.modifier != ControllerButton_NONE) {
		const std::string &modifierName = sgOptions.Padmapper.buttonToButtonName[static_cast<size_t>(boundInput.modifier)];
		if (modifierName.empty()) {
			LogVerbose("Padmapper: no name found for key '{}'", key);
			return;
		}
		inputName = StrCat(modifierName, "+", inputName);
	}
	SetIniValue(category.data(), key.data(), inputName.data());
}

void PadmapperOptions::Action::UpdateValueDescription() const
{
	boundInputDescriptionType = GamepadType;
	if (boundInput.button == ControllerButton_NONE) {
		boundInputDescription = "";
		boundInputShortDescription = "";
		return;
	}
	string_view buttonName = ToString(boundInput.button);
	if (boundInput.modifier == ControllerButton_NONE) {
		boundInputDescription = std::string(buttonName);
		boundInputShortDescription = std::string(Shorten(buttonName));
		return;
	}
	string_view modifierName = ToString(boundInput.modifier);
	boundInputDescription = StrCat(modifierName, "+", buttonName);
	boundInputShortDescription = StrCat(Shorten(modifierName), "+", Shorten(buttonName));
}

string_view PadmapperOptions::Action::Shorten(string_view buttonName) const
{
	size_t index = 0;
	size_t chars = 0;
	while (index < buttonName.size()) {
		if (!IsTrailUtf8CodeUnit(buttonName[index]))
			chars++;
		if (chars == 3)
			break;
		index++;
	}
	return string_view(buttonName.data(), index);
}

string_view PadmapperOptions::Action::GetValueDescription() const
{
	return GetValueDescription(false);
}

string_view PadmapperOptions::Action::GetValueDescription(bool useShortName) const
{
	if (GamepadType != boundInputDescriptionType)
		UpdateValueDescription();
	return useShortName ? boundInputShortDescription : boundInputDescription;
}

bool PadmapperOptions::Action::SetValue(ControllerButtonCombo value)
{
	if (boundInput.button != ControllerButton_NONE)
		boundInput = {};
	if (value.button != ControllerButton_NONE)
		boundInput = value;
	UpdateValueDescription();
	return true;
}

void PadmapperOptions::AddAction(string_view key, const char *name, const char *description, ControllerButtonCombo defaultInput, std::function<void()> actionPressed, std::function<void()> actionReleased, std::function<bool()> enable, unsigned index)
{
	if (committed)
		return;
	actions.emplace_front(key, name, description, defaultInput, std::move(actionPressed), std::move(actionReleased), std::move(enable), index);
}

void PadmapperOptions::CommitActions()
{
	if (committed)
		return;
	actions.reverse();
	committed = true;
}

void PadmapperOptions::ButtonPressed(ControllerButton button)
{
	const Action *action = FindAction(button);
	if (action == nullptr)
		return;
	if (IsMovementHandlerActive() && CanDeferToMovementHandler(*action))
		return;
	if (action->actionPressed)
		action->actionPressed();
	SuppressedButton = action->boundInput.modifier;
	buttonToReleaseAction[static_cast<size_t>(button)] = action;
}

void PadmapperOptions::ButtonReleased(ControllerButton button, bool invokeAction)
{
	if (invokeAction) {
		const Action *action = buttonToReleaseAction[static_cast<size_t>(button)];
		if (action == nullptr)
			return; // Ignore unmapped buttons.

		// Check that the action can be triggered.
		if (action->actionReleased && (!action->enable || action->enable()))
			action->actionReleased();
	}
	buttonToReleaseAction[static_cast<size_t>(button)] = nullptr;
}

void PadmapperOptions::ReleaseAllActiveButtons()
{
	for (auto *action : buttonToReleaseAction) {
		if (action == nullptr)
			continue;
		ControllerButton button = action->boundInput.button;
		ButtonReleased(button, true);
	}
}

bool PadmapperOptions::IsActive(string_view actionName) const
{
	for (const Action &action : actions) {
		if (action.key != actionName)
			continue;
		const Action *releaseAction = buttonToReleaseAction[static_cast<size_t>(action.boundInput.button)];
		return releaseAction != nullptr && releaseAction->key == actionName;
	}
	return false;
}

string_view PadmapperOptions::ActionNameTriggeredByButtonEvent(ControllerButtonEvent ctrlEvent) const
{
	if (!gbRunGame)
		return "";

	if (!ctrlEvent.up) {
		const Action *pressAction = FindAction(ctrlEvent.button);
		return pressAction != nullptr ? pressAction->key : "";
	}
	const Action *releaseAction = buttonToReleaseAction[static_cast<size_t>(ctrlEvent.button)];
	if (releaseAction == nullptr)
		return "";
	return releaseAction->key;
}

string_view PadmapperOptions::InputNameForAction(string_view actionName, bool useShortName) const
{
	for (const Action &action : actions) {
		if (action.key == actionName && action.boundInput.button != ControllerButton_NONE) {
			return action.GetValueDescription(useShortName);
		}
	}
	return "";
}

ControllerButtonCombo PadmapperOptions::ButtonComboForAction(string_view actionName) const
{
	for (const auto &action : actions) {
		if (action.key == actionName && action.boundInput.button != ControllerButton_NONE) {
			return action.boundInput;
		}
	}
	return ControllerButton_NONE;
}

const PadmapperOptions::Action *PadmapperOptions::FindAction(ControllerButton button) const
{
	// To give preference to button combinations,
	// first pass ignores mappings where no modifier is bound
	for (const Action &action : actions) {
		ControllerButtonCombo combo = action.boundInput;
		if (combo.modifier == ControllerButton_NONE)
			continue;
		if (button != combo.button)
			continue;
		if (!IsControllerButtonPressed(combo.modifier))
			continue;
		if (action.enable && !action.enable())
			continue;
		return &action;
	}

	for (const Action &action : actions) {
		ControllerButtonCombo combo = action.boundInput;
		if (combo.modifier != ControllerButton_NONE)
			continue;
		if (button != combo.button)
			continue;
		if (action.enable && !action.enable())
			continue;
		return &action;
	}

	return nullptr;
}

bool PadmapperOptions::CanDeferToMovementHandler(const Action &action) const
{
	if (action.boundInput.modifier != ControllerButton_NONE)
		return false;

	if (spselflag) {
		const string_view prefix { "QuickSpell" };
		const string_view key { action.key };
		if (key.size() >= prefix.size()) {
			const string_view truncated { key.data(), prefix.size() };
			if (truncated == prefix)
				return false;
		}
	}

	return IsAnyOf(action.boundInput.button,
	    ControllerButton_BUTTON_DPAD_UP,
	    ControllerButton_BUTTON_DPAD_DOWN,
	    ControllerButton_BUTTON_DPAD_LEFT,
	    ControllerButton_BUTTON_DPAD_RIGHT);
}

namespace {
constexpr char ResamplerSpeex[] = "Speex";
constexpr char ResamplerSDL[] = "SDL";
} // namespace

string_view ResamplerToString(Resampler resampler)
{
	switch (resampler) {
#ifdef DEVILUTIONX_RESAMPLER_SPEEX
	case Resampler::Speex:
		return ResamplerSpeex;
#endif
#ifdef DVL_AULIB_SUPPORTS_SDL_RESAMPLER
	case Resampler::SDL:
		return ResamplerSDL;
#endif
	default:
		return "";
	}
}

std::optional<Resampler> ResamplerFromString(string_view resampler)
{
#ifdef DEVILUTIONX_RESAMPLER_SPEEX
	if (resampler == ResamplerSpeex)
		return Resampler::Speex;
#endif
#ifdef DVL_AULIB_SUPPORTS_SDL_RESAMPLER
	if (resampler == ResamplerSDL)
		return Resampler::SDL;
#endif
	return std::nullopt;
}

} // namespace devilution
