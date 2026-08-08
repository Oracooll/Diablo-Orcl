#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <forward_list>
#include <unordered_map>

#include <SDL_version.h>

#include "controls/controller.h"
#include "controls/controller_buttons.h"
#include "controls/game_controls.h"
#include "engine/sound_defs.hpp"
#include "pack.h"
#include "utils/enum_traits.h"
#include "utils/stdcompat/optional.hpp"
#include "utils/stdcompat/string_view.hpp"

namespace devilution {

enum class StartUpGameMode : uint8_t {
	/** @brief If hellfire is present, asks the user what game they want to start. */
	Ask = 0,
	Hellfire = 1,
	Diablo = 2,
};

enum class StartUpIntro : uint8_t {
	Off = 0,
	Once = 1,
	On = 2,
};

/** @brief Defines what splash screen should be shown at startup. */
enum class StartUpSplash : uint8_t {
	/** @brief Show no splash screen. */
	None = 0,
	/** @brief Show only TitleDialog. */
	TitleDialog = 1,
	/** @brief Show Logo and TitleDialog. */
	LogoAndTitleDialog = 2,
};

enum class ScalingQuality : uint8_t {
	NearestPixel,
	BilinearFiltering,
	AnisotropicFiltering,
};

enum class FrameRateControl : uint8_t {
	None = 0,
#ifndef USE_SDL1
	VerticalSync = 1,
#endif
	CPUSleep = 2,
};

enum class Resampler : uint8_t {
#ifdef DEVILUTIONX_RESAMPLER_SPEEX
	Speex = 0,
#endif
#ifdef DVL_AULIB_SUPPORTS_SDL_RESAMPLER
	SDL,
#endif
};

string_view ResamplerToString(Resampler resampler);
std::optional<Resampler> ResamplerFromString(string_view resampler);

enum class FloatingNumbers : uint8_t {
	/** @brief Show no floating numbers. */
	Off = 0,
	/** @brief Show floating numbers at random angles. */
	Random = 1,
	/** @brief Show floating numbers vertically only. */
	Vertical = 2,
};

enum class OptionEntryType : uint8_t {
	Boolean,
	List,
	Key,
	PadButton,
};

enum class OptionEntryFlags : uint8_t {
	/** @brief No special logic. */
	None = 0,
	/** @brief Shouldn't be shown in settings dialog. */
	Invisible = 1 << 0,
	/** @brief Need to restart the current running game (single- or multiplayer) to take effect. */
	CantChangeInGame = 1 << 1,
	/** @brief Need to restart the current running multiplayer game to take effect. */
	CantChangeInMultiPlayer = 1 << 2,
	/** @brief Option is only relevant for Hellfire. */
	OnlyHellfire = 1 << 3,
	/** @brief Option is only relevant for Diablo. */
	OnlyDiablo = 1 << 4,
	/** @brief After option is changed the UI needs to be recreated. */
	RecreateUI = 1 << 5,
	/** @brief diablo.mpq must be present. */
	NeedDiabloMpq = 1 << 6,
	/** @brief hellfire.mpq must be present. */
	NeedHellfireMpq = 1 << 7,
};
use_enum_as_flags(OptionEntryFlags);

class OptionEntryBase {
public:
	OptionEntryBase(string_view key, OptionEntryFlags flags, const char *name, const char *description)
	    : flags(flags)
	    , key(key)
	    , name(name)
	    , description(description)
	{
	}
	[[nodiscard]] virtual string_view GetName() const;
	[[nodiscard]] string_view GetDescription() const;
	[[nodiscard]] virtual OptionEntryType GetType() const = 0;
	[[nodiscard]] OptionEntryFlags GetFlags() const;

	void SetValueChangedCallback(std::function<void()> callback);

	[[nodiscard]] virtual string_view GetValueDescription() const = 0;
	virtual void LoadFromIni(string_view category) = 0;
	virtual void SaveToIni(string_view category) const = 0;

	OptionEntryFlags flags;

protected:
	string_view key;
	const char *name;
	const char *description;
	void NotifyValueChanged();

private:
	std::function<void()> callback;
};

class OptionEntryBoolean : public OptionEntryBase {
public:
	OptionEntryBoolean(string_view key, OptionEntryFlags flags, const char *name, const char *description, bool defaultValue)
	    : OptionEntryBase(key, flags, name, description)
	    , defaultValue(defaultValue)
	    , value(defaultValue)
	{
	}
	[[nodiscard]] bool operator*() const
	{
		return value;
	}
	void SetValue(bool value);

	[[nodiscard]] OptionEntryType GetType() const override;
	[[nodiscard]] string_view GetValueDescription() const override;
	void LoadFromIni(string_view category) override;
	void SaveToIni(string_view category) const override;

private:
	bool defaultValue;
	bool value;
};

class OptionEntryListBase : public OptionEntryBase {
public:
	[[nodiscard]] virtual size_t GetListSize() const = 0;
	[[nodiscard]] virtual string_view GetListDescription(size_t index) const = 0;
	[[nodiscard]] virtual size_t GetActiveListIndex() const = 0;
	virtual void SetActiveListIndex(size_t index) = 0;

	[[nodiscard]] OptionEntryType GetType() const override;
	[[nodiscard]] string_view GetValueDescription() const override;

protected:
	OptionEntryListBase(string_view key, OptionEntryFlags flags, const char *name, const char *description)
	    : OptionEntryBase(key, flags, name, description)
	{
	}
};

class OptionEntryEnumBase : public OptionEntryListBase {
public:
	void LoadFromIni(string_view category) override;
	void SaveToIni(string_view category) const override;

	[[nodiscard]] size_t GetListSize() const override;
	[[nodiscard]] string_view GetListDescription(size_t index) const override;
	[[nodiscard]] size_t GetActiveListIndex() const override;
	void SetActiveListIndex(size_t index) override;

protected:
	OptionEntryEnumBase(string_view key, OptionEntryFlags flags, const char *name, const char *description, int defaultValue)
	    : OptionEntryListBase(key, flags, name, description)
	    , defaultValue(defaultValue)
	    , value(defaultValue)
	{
	}

	[[nodiscard]] int GetValueInternal() const
	{
		return value;
	}
	void SetValueInternal(int value);

	void AddEntry(int value, string_view name);

private:
	int defaultValue;
	int value;
	std::vector<string_view> entryNames;
	std::vector<int> entryValues;
};

template <typename T>
class OptionEntryEnum : public OptionEntryEnumBase {
public:
	OptionEntryEnum(string_view key, OptionEntryFlags flags, const char *name, const char *description, T defaultValue, std::initializer_list<std::pair<T, string_view>> entries)
	    : OptionEntryEnumBase(key, flags, name, description, static_cast<int>(defaultValue))
	{
		for (auto entry : entries) {
			AddEntry(static_cast<int>(entry.first), entry.second);
		}
	}
	[[nodiscard]] T operator*() const
	{
		return static_cast<T>(GetValueInternal());
	}
	void SetValue(T value)
	{
		SetValueInternal(static_cast<int>(value));
	}
};

class OptionEntryIntBase : public OptionEntryListBase {
public:
	void LoadFromIni(string_view category) override;
	void SaveToIni(string_view category) const override;

	[[nodiscard]] size_t GetListSize() const override;
	[[nodiscard]] string_view GetListDescription(size_t index) const override;
	[[nodiscard]] size_t GetActiveListIndex() const override;
	void SetActiveListIndex(size_t index) override;

protected:
	OptionEntryIntBase(string_view key, OptionEntryFlags flags, const char *name, const char *description, int defaultValue)
	    : OptionEntryListBase(key, flags, name, description)
	    , defaultValue(defaultValue)
	    , value(defaultValue)
	{
	}

	[[nodiscard]] int GetValueInternal() const
	{
		return value;
	}
	void SetValueInternal(int value);

	void AddEntry(int value);

	/**
	 * @brief Oracool: lets a subclass reformat a specific entry's displayed text (see
	 * OptionEntryTormentMultiplier) without needing to duplicate GetListSize/AddEntry's own
	 * bookkeeping of which values are actually present, in what order.
	 */
	[[nodiscard]] int GetEntryValue(size_t index) const
	{
		return entryValues[index];
	}

private:
	int defaultValue;
	int value;
	mutable std::vector<std::string> entryNames;
	std::vector<int> entryValues;
};

template <typename T>
class OptionEntryInt : public OptionEntryIntBase {
public:
	OptionEntryInt(string_view key, OptionEntryFlags flags, const char *name, const char *description, T defaultValue, std::initializer_list<T> entries)
	    : OptionEntryIntBase(key, flags, name, description, static_cast<int>(defaultValue))
	{
		for (auto entry : entries) {
			AddEntry(static_cast<int>(entry));
		}
	}
	OptionEntryInt(string_view key, OptionEntryFlags flags, const char *name, const char *description, T defaultValue)
	    : OptionEntryInt(key, flags, name, description, defaultValue, { defaultValue })
	{
	}
	[[nodiscard]] T operator*() const
	{
		return static_cast<T>(GetValueInternal());
	}
	void SetValue(T value)
	{
		SetValueInternal(static_cast<int>(value));
	}
};

/**
 * @brief Oracool: an int-backed slider (like OptionEntryInt) whose stored value is tenths of the
 * actual setting, displayed and read back as a one-decimal float - used for Torment Difficulty
 * Multiplier's 1.1-5.0-in-0.1-steps range, which OptionEntryInt's plain integer display can't
 * represent without this reformatting.
 */
class OptionEntryTormentMultiplier : public OptionEntryIntBase {
public:
	OptionEntryTormentMultiplier(string_view key, OptionEntryFlags flags, const char *name, const char *description, int defaultValueTenths, std::initializer_list<int> entriesTenths)
	    : OptionEntryIntBase(key, flags, name, description, defaultValueTenths)
	{
		for (auto entry : entriesTenths)
			AddEntry(entry);
	}

	[[nodiscard]] string_view GetListDescription(size_t index) const override;

	[[nodiscard]] float operator*() const
	{
		return GetValueInternal() / 10.0f;
	}

	/** @brief The raw stored tenths value (e.g. 20 for 2.0x) - for the INI writer, which needs the
	 * exact integer rather than a float round-trip through *this that could round imprecisely. */
	[[nodiscard]] int ValueTenths() const
	{
		return GetValueInternal();
	}

	/** @param tenths the raw tenths value to store (e.g. 20 for 2.0x), not the float multiplier. */
	void SetValue(int tenths)
	{
		SetValueInternal(tenths);
	}

private:
	/**
	 * @brief Oracool: caches every entry's formatted text, one std::string per index, so the
	 * string_views GetListDescription hands back all stay valid simultaneously - settingsmenu.cpp
	 * calls GetListDescription(i) for every i in a loop before displaying any of them, so a single
	 * shared/reused buffer (the previous approach) would make every entry after the loop show
	 * whatever the *last* call formatted (a real bug this fixed - every entry but the first showed
	 * the same value).
	 */
	mutable std::vector<std::string> descriptionCache;
};

/**
 * @brief Oracool: an int-backed list (like OptionEntryInt) whose value 0 displays as "OFF"
 * instead of the digit "0" - used for Monster Range Highlight's OFF/1/2/3/4/5 range.
 */
class OptionEntryRangeOrOff : public OptionEntryIntBase {
public:
	OptionEntryRangeOrOff(string_view key, OptionEntryFlags flags, const char *name, const char *description, int defaultValue, std::initializer_list<int> entries)
	    : OptionEntryIntBase(key, flags, name, description, defaultValue)
	{
		for (auto entry : entries)
			AddEntry(entry);
	}

	[[nodiscard]] string_view GetListDescription(size_t index) const override;

	[[nodiscard]] int operator*() const
	{
		return GetValueInternal();
	}
	void SetValue(int value)
	{
		SetValueInternal(value);
	}

private:
	/** @brief Oracool: see OptionEntryTormentMultiplier::descriptionCache for why this exists. */
	mutable std::vector<std::string> descriptionCache;
};

/**
 * @brief Oracool: an int-backed value (like OptionEntryTormentMultiplier) storing tenths of the
 * dungeon-view zoom factor - 10 to 20, i.e. 1.0x (zoomed out, today's normal view) to 2.0x
 * (zoomed in, matching the old binary Graphics.zoom's exact 2x). Adjusted live via mouse wheel /
 * middle-click, not through the settings dialog - always constructed with Invisible so it never
 * appears there, but it still persists through the normal GetEntries()-driven ini load/save.
 */
class OptionEntryDungeonZoom : public OptionEntryIntBase {
public:
	OptionEntryDungeonZoom(string_view key, OptionEntryFlags flags, const char *name, const char *description, int defaultValueTenths)
	    : OptionEntryIntBase(key, flags, name, description, defaultValueTenths)
	{
		for (int tenths = 10; tenths <= 20; tenths++)
			AddEntry(tenths);
	}

	[[nodiscard]] float operator*() const
	{
		return GetValueInternal() / 10.0f;
	}

	/** @brief The raw stored tenths value (10-20) - for the INI writer / wheel step logic. */
	[[nodiscard]] int ValueTenths() const
	{
		return GetValueInternal();
	}

	/** @param tenths the raw tenths value to store (10-20, i.e. 1.0x-2.0x), not the float factor. */
	void SetValue(int tenths)
	{
		SetValueInternal(std::clamp(tenths, 10, 20));
	}
};

class OptionEntryLanguageCode : public OptionEntryListBase {
public:
	OptionEntryLanguageCode();

	void LoadFromIni(string_view category) override;
	void SaveToIni(string_view category) const override;

	[[nodiscard]] size_t GetListSize() const override;
	[[nodiscard]] string_view GetListDescription(size_t index) const override;
	[[nodiscard]] size_t GetActiveListIndex() const override;
	void SetActiveListIndex(size_t index) override;

	string_view operator*() const
	{
		return szCode;
	}

	OptionEntryLanguageCode &operator=(string_view code)
	{
		assert(code.size() < 6);
		memcpy(szCode, code.data(), code.size());
		szCode[code.size()] = '\0';
		return *this;
	}

private:
	/** @brief Language code (ISO-15897) for text. */
	char szCode[6];
	mutable std::vector<std::pair<std::string, std::string>> languages;

	void CheckLanguagesAreInitialized() const;
};

class OptionEntryResolution : public OptionEntryListBase {
public:
	OptionEntryResolution();

	void LoadFromIni(string_view category) override;
	void SaveToIni(string_view category) const override;

	[[nodiscard]] size_t GetListSize() const override;
	[[nodiscard]] string_view GetListDescription(size_t index) const override;
	[[nodiscard]] size_t GetActiveListIndex() const override;
	void SetActiveListIndex(size_t index) override;
	void InvalidateList();

	Size operator*() const
	{
		return size;
	}

private:
	/** @brief View size. */
	Size size;
	mutable std::vector<std::pair<Size, std::string>> resolutions;

	void CheckResolutionsAreInitialized() const;
};

class OptionEntryResampler : public OptionEntryListBase {
public:
	OptionEntryResampler();

	void LoadFromIni(string_view category) override;
	void SaveToIni(string_view category) const override;

	[[nodiscard]] size_t GetListSize() const override;
	[[nodiscard]] string_view GetListDescription(size_t index) const override;
	[[nodiscard]] size_t GetActiveListIndex() const override;
	void SetActiveListIndex(size_t index) override;

	Resampler operator*() const
	{
		return resampler_;
	}

private:
	void UpdateDependentOptions() const;

	Resampler resampler_;
};

class OptionEntryAudioDevice : public OptionEntryListBase {
public:
	OptionEntryAudioDevice();

	void LoadFromIni(string_view category) override;
	void SaveToIni(string_view category) const override;

	[[nodiscard]] size_t GetListSize() const override;
	[[nodiscard]] string_view GetListDescription(size_t index) const override;
	[[nodiscard]] size_t GetActiveListIndex() const override;
	void SetActiveListIndex(size_t index) override;

	std::string operator*() const
	{
		for (size_t i = 0; i < GetListSize(); i++) {
			string_view deviceName = GetDeviceName(i);
			if (deviceName == deviceName_)
				return deviceName_;
		}
		return "";
	}

private:
	string_view GetDeviceName(size_t index) const;

	std::string deviceName_;
};

struct OptionCategoryBase {
	OptionCategoryBase(string_view key, const char *name, const char *description)
	    : key(key)
	    , name(name)
	    , description(description)
	{
	}

	[[nodiscard]] string_view GetKey() const;
	[[nodiscard]] string_view GetName() const;
	[[nodiscard]] string_view GetDescription() const;

	virtual std::vector<OptionEntryBase *> GetEntries() = 0;

protected:
	string_view key;
	const char *name;
	const char *description;
};

struct GameModeOptions : OptionCategoryBase {
	GameModeOptions();
	std::vector<OptionEntryBase *> GetEntries() override;

	OptionEntryEnum<StartUpGameMode> gameMode;
	OptionEntryBoolean shareware;
};

struct StartUpOptions : OptionCategoryBase {
	StartUpOptions();
	std::vector<OptionEntryBase *> GetEntries() override;

	/** @brief Play game intro video on diablo startup. */
	OptionEntryEnum<StartUpIntro> diabloIntro;
	/** @brief Play game intro video on hellfire startup. */
	OptionEntryEnum<StartUpIntro> hellfireIntro;
	OptionEntryEnum<StartUpSplash> splash;
};

struct DiabloOptions : OptionCategoryBase {
	DiabloOptions();
	std::vector<OptionEntryBase *> GetEntries() override;

	/** @brief Remembers what singleplayer hero/save was last used. */
	OptionEntryInt<std::uint32_t> lastSinglePlayerHero;
	/** @brief Remembers what multiplayer hero/save was last used. */
	OptionEntryInt<std::uint32_t> lastMultiplayerHero;
};

struct HellfireOptions : OptionCategoryBase {
	HellfireOptions();
	std::vector<OptionEntryBase *> GetEntries() override;

	/** @brief Cornerstone of the world item. */
	char szItem[sizeof(ItemPack) * 2 + 1];
	/** @brief Remembers what singleplayer hero/save was last used. */
	OptionEntryInt<std::uint32_t> lastSinglePlayerHero;
	/** @brief Remembers what multiplayer hero/save was last used. */
	OptionEntryInt<std::uint32_t> lastMultiplayerHero;
};

struct AudioOptions : OptionCategoryBase {
	AudioOptions();
	std::vector<OptionEntryBase *> GetEntries() override;

	/** @brief Movie and SFX volume. */
	OptionEntryInt<int> soundVolume;
	/** @brief Music volume. */
	OptionEntryInt<int> musicVolume;
	/** @brief Player emits sound when walking. */
	OptionEntryBoolean walkingSound;
	/** @brief Automatically equipping items on pickup emits the equipment sound. */
	OptionEntryBoolean autoEquipSound;
	/** @brief Picking up items emits the items pickup sound. */
	OptionEntryBoolean itemPickupSound;

	/** @brief Output sample rate (Hz). */
	OptionEntryInt<std::uint32_t> sampleRate;
	/** @brief The number of output channels (1 or 2) */
	OptionEntryInt<std::uint8_t> channels;
	/** @brief Buffer size (number of frames per channel) */
	OptionEntryInt<std::uint32_t> bufferSize;
	/** @brief Resampler implementation. */
	OptionEntryResampler resampler;
	/** @brief Quality of the resampler, from 0 (lowest) to 10 (highest). Available for the speex resampler only. */
	OptionEntryInt<std::uint8_t> resamplingQuality;
	/** @brief Audio device. */
	OptionEntryAudioDevice device;
};

struct GraphicsOptions : OptionCategoryBase {
	GraphicsOptions();
	std::vector<OptionEntryBase *> GetEntries() override;

	OptionEntryResolution resolution;
	/** @brief Run in fullscreen or windowed mode. */
	OptionEntryBoolean fullscreen;
#if !defined(USE_SDL1) || defined(__3DS__)
	/** @brief Expand the aspect ratio to match the screen. */
	OptionEntryBoolean fitToScreen;
#endif
#ifndef USE_SDL1
	/** @brief Scale the image after rendering. */
	OptionEntryBoolean upscale;
	/** @brief See SDL_HINT_RENDER_SCALE_QUALITY. */
	OptionEntryEnum<ScalingQuality> scaleQuality;
	/** @brief Only scale by values divisible by the width and height. */
	OptionEntryBoolean integerScaling;
#endif
	/** @brief Limit frame rate either for vsync or CPU load. */
	OptionEntryEnum<FrameRateControl> frameRateControl;
	/** @brief Gamma correction level. */
	OptionEntryInt<int> gammaCorrection;
	/** @brief Enable color cycling animations. */
	OptionEntryBoolean colorCycling;
	/** @brief Use alternate nest palette. */
	OptionEntryBoolean alternateNestArt;
#if SDL_VERSION_ATLEAST(2, 0, 0)
	/** @brief Use a hardware cursor (SDL2 only). */
	OptionEntryBoolean hardwareCursor;
	/** @brief Use a hardware cursor for items. */
	OptionEntryBoolean hardwareCursorForItems;
	/** @brief Maximum width / height for the hardware cursor. Larger cursors fall back to software. */
	OptionEntryInt<int> hardwareCursorMaxSize;
#endif
	/** @brief Show FPS, even without the -f command line flag. */
	OptionEntryBoolean showFPS;
};

struct GameplayOptions : OptionCategoryBase {
	GameplayOptions();
	std::vector<OptionEntryBase *> GetEntries() override;

	/** @brief Gameplay ticks per second. */
	OptionEntryInt<int> tickRate;
	/** @brief Enable double walk speed when in town. */
	OptionEntryBoolean runInTown;
	/** @brief Do not let the mouse leave the application window. */
	OptionEntryBoolean grabInput;
	/** @brief Enable the Theo quest. */
	OptionEntryBoolean theoQuest;
	/** @brief Enable the cow quest. */
	OptionEntryBoolean cowQuest;
	/** @brief Will players still damage other players in non-PvP mode. */
	OptionEntryBoolean friendlyFire;
	/** @brief Enables the full/uncut singleplayer version of quests. */
	OptionEntryBoolean multiplayerFullQuests;
	/** @brief Enable the bard hero class. */
	OptionEntryBoolean testBard;
	/** @brief Enable the babarian hero class. */
	OptionEntryBoolean testBarbarian;
	/** @brief Show the current level progress. */
	OptionEntryBoolean experienceBar;
	/** @brief Show item graphics to the left of item descriptions in store menus. */
	OptionEntryBoolean showItemGraphicsInStores;
	/** @brief Display current/max health values on health globe. */
	OptionEntryBoolean showHealthValues;
	/** @brief Display current/max mana values on mana globe. */
	OptionEntryBoolean showManaValues;
	/** @brief Show enemy health at the top of the screen. */
	OptionEntryBoolean enemyHealthBar;
	/** @brief Automatically pick up gold when walking over it. */
	OptionEntryBoolean autoGoldPickup;
	/** @brief Auto-pickup elixirs */
	OptionEntryBoolean autoElixirPickup;
	/** @brief Auto-pickup oils */
	OptionEntryBoolean autoOilPickup;
	/** @brief Enable or Disable auto-pickup in town */
	OptionEntryBoolean autoPickupInTown;
	/** @brief Recover mana when talking to Adria. */
	OptionEntryBoolean adriaRefillsMana;
	/** @brief Automatically attempt to equip weapon-type items when picking them up. */
	OptionEntryBoolean autoEquipWeapons;
	/** @brief Automatically attempt to equip armor-type items when picking them up. */
	OptionEntryBoolean autoEquipArmor;
	/** @brief Automatically attempt to equip helm-type items when picking them up. */
	OptionEntryBoolean autoEquipHelms;
	/** @brief Automatically attempt to equip shield-type items when picking them up. */
	OptionEntryBoolean autoEquipShields;
	/** @brief Automatically attempt to equip jewelry-type items when picking them up. */
	OptionEntryBoolean autoEquipJewelry;
	/** @brief Only enable 2/3 quests in each game session */
	OptionEntryBoolean randomizeQuests;
	/** @brief Indicates whether or not monster type (Animal, Demon, Undead) is shown along with other monster information. */
	OptionEntryBoolean showMonsterType;
	/** @brief Displays item labels for items on the ground.  */
	OptionEntryBoolean showItemLabels;
	/** @brief Refill belt from inventory, or rather, use potions/scrolls from inventory first when belt item is consumed.  */
	OptionEntryBoolean autoRefillBelt;
	/** @brief Locally disable clicking on shrines which permanently cripple character. */
	OptionEntryBoolean disableCripplingShrines;
	/** @brief Spell hotkeys instantly cast the spell. */
	OptionEntryBoolean quickCast;
	/** @brief Healing potions are automatically collected when in close proximity to the player. */
	OptionEntryBoolean numHealPotionPickup;
	/** @brief Full Healing potions are automatically collected when in close proximity to the player. */
	OptionEntryBoolean numFullHealPotionPickup;
	/** @brief Mana potions are automatically collected when in close proximity to the player. */
	OptionEntryBoolean numManaPotionPickup;
	/** @brief Full Mana potions are automatically collected when in close proximity to the player. */
	OptionEntryBoolean numFullManaPotionPickup;
	/** @brief Rejuvenation potions are automatically collected when in close proximity to the player. */
	OptionEntryBoolean numRejuPotionPickup;
	/** @brief Full Rejuvenation potions are automatically collected when in close proximity to the player. */
	OptionEntryBoolean numFullRejuPotionPickup;
	/** @brief Enable floating numbers. */
	OptionEntryEnum<FloatingNumbers> enableFloatingNumbers;
};

struct OracoolOptions : OptionCategoryBase {
	OracoolOptions();
	std::vector<OptionEntryBase *> GetEntries() override;

	OptionEntryInt<int> uniqueItemDropMultiplier;
	OptionEntryBoolean unlockAllTownEntrances;
	OptionEntryBoolean permanentInfravision;
	OptionEntryBoolean autoIdentifyDrops;
	OptionEntryBoolean resetStatsButton;
	OptionEntryBoolean inventorySortButton;
	OptionEntryInt<int> autoPickupRange;
	OptionEntryBoolean autoScrollPickup;
	OptionEntryInt<int> rareItemDropChance;
	OptionEntryInt<int> buffedUniqueItemDropChance;
	OptionEntryInt<int> primalItemDropChance;
	OptionEntryBoolean griswoldPremiumRefresh;
	OptionEntryBoolean refreshUntilButton;
	OptionEntryInt<int> refreshUntilTimeoutSeconds;
	char refreshUntilItemNames[512];
	OptionEntryBoolean griswoldRestoreHealth;
	OptionEntryBoolean griswoldRestoreMana;
	OptionEntryBoolean griswoldSellUniqueItems;
	OptionEntryInt<int> griswoldUniqueShopItems;
	OptionEntryInt<int> griswoldUniqueItemPriceMultiplier;
	OptionEntryBoolean griswoldPremiumIgnoreAffixLevelLimits;
	OptionEntryBoolean griswoldPremiumIgnorePriceLimits;
	OptionEntryBoolean griswoldSellIgnoresBelt;
	OptionEntryBoolean autoSave;
	OptionEntryInt<int> autoSaveIntervalMinutes;
	OptionEntryBoolean autoSaveOnLevelChange;
	OptionEntryBoolean autoSaveOnItemPickup;
	OptionEntryBoolean autoSaveOnStorePurchase;
	OptionEntryInt<int> autoSaveItemDelaySeconds;
	OptionEntryBoolean autoSaveNotification;
	OptionEntryBoolean difficultyLevelGate;
	OptionEntryTormentMultiplier tormentDifficultyMultiplier;
	OptionEntryBoolean miniMapEnabled;
	OptionEntryBoolean eventLog;
	OptionEntryBoolean furiousCharge;
	OptionEntryBoolean gameClock;
	OptionEntryBoolean gameClock12HourFormat;
	OptionEntryBoolean gradualHealing;
	OptionEntryBoolean xpCounter;
	OptionEntryBoolean xpGainIndicator;
	OptionEntryBoolean remainingMonsterXpButton;
	OptionEntryRangeOrOff monsterRangeHighlight;
	OptionEntryBoolean monsterWallOutline;
	OptionEntryRangeOrOff warriorSplashDamageRange;
	/** @brief Continuous dungeon-view zoom level, in tenths (10-20 = 1.0x-2.0x). Invisible - set
	 * live via mouse wheel / middle-click, not through the settings dialog. */
	OptionEntryDungeonZoom dungeonZoomLevel;
};

struct ControllerOptions : OptionCategoryBase {
	ControllerOptions();
	std::vector<OptionEntryBase *> GetEntries() override;

	/** @brief SDL Controller mapping, see SDL_GameControllerDB. */
	char szMapping[1024];
	/** @brief Configure gamepad joysticks deadzone */
	float fDeadzone;
#ifdef __vita__
	/** @brief Enable input via rear touchpad */
	bool bRearTouch;
#endif
};

struct NetworkOptions : OptionCategoryBase {
	NetworkOptions();
	std::vector<OptionEntryBase *> GetEntries() override;

	/** @brief Optionally bind to a specific network interface. */
	char szBindAddress[129];
	/** @brief Most recently entered ZeroTier Game ID. */
	char szPreviousZTGame[129];
	/** @brief Most recently entered Hostname in join dialog. */
	char szPreviousHost[129];
	/** @brief What network port to use. */
	OptionEntryInt<uint16_t> port;
};

struct ChatOptions : OptionCategoryBase {
	ChatOptions();
	std::vector<OptionEntryBase *> GetEntries() override;

	/** @brief Quick chat messages. */
	std::vector<std::string> szHotKeyMsgs[QUICK_MESSAGE_OPTIONS];
};

struct LanguageOptions : OptionCategoryBase {
	LanguageOptions();
	std::vector<OptionEntryBase *> GetEntries() override;

	OptionEntryLanguageCode code;
};

constexpr uint32_t KeymapperMouseButtonMask = 1 << 31;
constexpr uint32_t MouseScrollUpButton = 65536 | KeymapperMouseButtonMask;
constexpr uint32_t MouseScrollDownButton = 65537 | KeymapperMouseButtonMask;
constexpr uint32_t MouseScrollLeftButton = 65538 | KeymapperMouseButtonMask;
constexpr uint32_t MouseScrollRightButton = 65539 | KeymapperMouseButtonMask;

/** The Keymapper maps keys to actions. */
struct KeymapperOptions : OptionCategoryBase {
	/**
	 * Action represents an action that can be triggered using a keyboard
	 * shortcut.
	 */
	class Action final : public OptionEntryBase {
	public:
		// OptionEntryBase::key may be referencing Action::dynamicKey.
		// The implicit copy constructor would copy that reference instead of referencing the copy.
		Action(const Action &) = delete;

		Action(string_view key, const char *name, const char *description, uint32_t defaultKey, std::function<void()> actionPressed, std::function<void()> actionReleased, std::function<bool()> enable, unsigned index);

		[[nodiscard]] string_view GetName() const override;
		[[nodiscard]] OptionEntryType GetType() const override
		{
			return OptionEntryType::Key;
		}

		void LoadFromIni(string_view category) override;
		void SaveToIni(string_view category) const override;

		[[nodiscard]] string_view GetValueDescription() const override;

		bool SetValue(int value);

	private:
		uint32_t defaultKey;
		std::function<void()> actionPressed;
		std::function<void()> actionReleased;
		std::function<bool()> enable;
		uint32_t boundKey = SDLK_UNKNOWN;
		unsigned dynamicIndex;
		std::string dynamicKey;
		mutable std::string dynamicName;

		friend struct KeymapperOptions;
	};

	KeymapperOptions();
	std::vector<OptionEntryBase *> GetEntries() override;

	void AddAction(
	    string_view key, const char *name, const char *description, uint32_t defaultKey,
	    std::function<void()> actionPressed,
	    std::function<void()> actionReleased = nullptr,
	    std::function<bool()> enable = nullptr,
	    unsigned index = 0);
	void CommitActions();
	void KeyPressed(uint32_t key) const;
	void KeyReleased(SDL_Keycode key) const;
	bool IsTextEntryKey(SDL_Keycode vkey) const;
	bool IsNumberEntryKey(SDL_Keycode vkey) const;
	string_view KeyNameForAction(string_view actionName) const;
	uint32_t KeyForAction(string_view actionName) const;

private:
	std::forward_list<Action> actions;
	std::unordered_map<uint32_t, std::reference_wrapper<Action>> keyIDToAction;
	std::unordered_map<uint32_t, std::string> keyIDToKeyName;
	std::unordered_map<std::string, uint32_t> keyNameToKeyID;
};

/** The Padmapper maps gamepad buttons to actions. */
struct PadmapperOptions : OptionCategoryBase {
	/**
	 * Action represents an action that can be triggered using a gamepad
	 * button combo.
	 */
	class Action final : public OptionEntryBase {
	public:
		Action(string_view key, const char *name, const char *description, ControllerButtonCombo defaultInput, std::function<void()> actionPressed, std::function<void()> actionReleased, std::function<bool()> enable, unsigned index);

		// OptionEntryBase::key may be referencing Action::dynamicKey.
		// The implicit copy constructor would copy that reference instead of referencing the copy.
		Action(const Action &) = delete;

		[[nodiscard]] string_view GetName() const override;
		[[nodiscard]] OptionEntryType GetType() const override
		{
			return OptionEntryType::PadButton;
		}

		void LoadFromIni(string_view category) override;
		void SaveToIni(string_view category) const override;

		[[nodiscard]] string_view GetValueDescription() const override;
		[[nodiscard]] string_view GetValueDescription(bool useShortName) const;

		bool SetValue(ControllerButtonCombo value);

	private:
		ControllerButtonCombo defaultInput;
		std::function<void()> actionPressed;
		std::function<void()> actionReleased;
		std::function<bool()> enable;
		ControllerButtonCombo boundInput {};
		mutable GamepadLayout boundInputDescriptionType = GamepadLayout::Generic;
		mutable std::string boundInputDescription;
		mutable std::string boundInputShortDescription;
		unsigned dynamicIndex;
		std::string dynamicKey;
		mutable std::string dynamicName;

		void UpdateValueDescription() const;
		string_view Shorten(string_view buttonName) const;

		friend struct PadmapperOptions;
	};

	PadmapperOptions();
	std::vector<OptionEntryBase *> GetEntries() override;

	void AddAction(
	    string_view key, const char *name, const char *description, ControllerButtonCombo defaultInput,
	    std::function<void()> actionPressed,
	    std::function<void()> actionReleased = nullptr,
	    std::function<bool()> enable = nullptr,
	    unsigned index = 0);
	void CommitActions();
	void ButtonPressed(ControllerButton button);
	void ButtonReleased(ControllerButton button, bool invokeAction = true);
	void ReleaseAllActiveButtons();
	bool IsActive(string_view actionName) const;
	string_view ActionNameTriggeredByButtonEvent(ControllerButtonEvent ctrlEvent) const;
	string_view InputNameForAction(string_view actionName, bool useShortName = false) const;
	ControllerButtonCombo ButtonComboForAction(string_view actionName) const;

private:
	std::forward_list<Action> actions;
	std::array<const Action *, enum_size<ControllerButton>::value> buttonToReleaseAction;
	std::array<std::string, enum_size<ControllerButton>::value> buttonToButtonName;
	std::unordered_map<std::string, ControllerButton> buttonNameToButton;
	bool committed = false;

	const Action *FindAction(ControllerButton button) const;
	bool CanDeferToMovementHandler(const Action &action) const;
};

struct Options {
	GameModeOptions GameMode;
	StartUpOptions StartUp;
	DiabloOptions Diablo;
	HellfireOptions Hellfire;
	AudioOptions Audio;
	GameplayOptions Gameplay;
	OracoolOptions Oracool;
	GraphicsOptions Graphics;
	ControllerOptions Controller;
	NetworkOptions Network;
	ChatOptions Chat;
	LanguageOptions Language;
	KeymapperOptions Keymapper;
	PadmapperOptions Padmapper;

	[[nodiscard]] std::vector<OptionCategoryBase *> GetCategories()
	{
		return {
			&Language,
			&GameMode,
			&StartUp,
			&Graphics,
			&Audio,
			&Diablo,
			&Hellfire,
			&Gameplay,
			&Oracool,
			&Controller,
			&Network,
			&Chat,
			&Keymapper,
			&Padmapper,
		};
	}
};

extern DVL_API_FOR_TEST Options sgOptions;

/**
 * @brief Oracool: how much harder Torment is than Hell, applied on top of Hell's already-computed
 * monster/treasure formulas rather than as its own independent set of per-stat constants (see
 * monster.h/.cpp and items.cpp's gold-value switch).
 */
inline float GetTormentDifficultyMultiplier()
{
	return *sgOptions.Oracool.tormentDifficultyMultiplier;
}

bool HardwareCursorSupported();

/**
 * @brief Save game configurations to ini file
 */
void SaveOptions();

/**
 * @brief Load game configurations from ini file
 */
void LoadOptions();

} // namespace devilution
