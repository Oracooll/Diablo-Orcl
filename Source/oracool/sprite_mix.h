#pragma once
/**
 * @file oracool/sprite_mix.h
 *
 * Oracool: a hero's sheets ASSEMBLED from the game's own - a shield or a sword from another armour tier
 * on this tier's body (user, 2026-09-17: "introduce Shield from another tier and Weapon from another
 * tier and ... assign Shield and Sword to particular item types").
 *
 * Every player sheet is one flat render, so nothing can be taken off it by asking. What makes this
 * possible is that inside one armour tier the sheets of the one-handed family - sword, mace, axe, staff,
 * and the two with a shield - are THE SAME BODY RENDER with different things in its hands (measured
 * 2026-09-17: 65-78% of pixels identical, the rest weapon, arm and dither). So:
 *
 *  - a SHIELD is what differs between "sword and shield" and "sword": subtract, keep the piece by the arm;
 *  - a BODY WITH EMPTY HANDS is a vote, pixel by pixel, across sword, mace, axe and staff - whatever at
 *    least two agree on is body, because no two of them hold the same thing;
 *  - a SWORD is what the sword sheet shows that the vote does not.
 *
 * Do that in the heavy tier and in the body's own tier, and the heavy pieces go where the own pieces
 * were. It is done at LOAD time, from the archive's CL2s, in palette-index space - no art is shipped and
 * drawing costs nothing - for every class with sheets of its own (Warrior, Rogue, Sorcerer, Monk; the Barbarian
 * and the Bard wear the first two). Both halves are INI options: "Shields Sprites Swap", "Swords Sprites Swap".
 *
 * Known limits, all seen in the proofs and none hidden: each tier holds its sword at its own angle, so a
 * heavy sword sits near a light hand rather than in it; heavy armour is bulkier, so a shield lifted from
 * it leaves a pixel or two of gap against a slimmer body; the BLOCK animation has no shieldless twin to
 * subtract from and keeps its own tier's shield; unarmed and bow are other poses and are left alone.
 */

#include <cstdint>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "oracool/sprite_import.h"
#include "player.h"

namespace devilution::oracool {

/**
 * @brief Which armour tier's sheets a piece of the look is lifted from. Own = whatever the body armour is, which
 * is also what an item this table does not know gets. The three named tiers are ArmourChar order.
 */
enum class LookTier : uint8_t {
	Own,
	Light,
	Medium,
	Heavy,
};

/** @brief The ArmourChar index of @p tier, or -1 for Own. */
constexpr int LookTierIndex(LookTier tier)
{
	return static_cast<int>(tier) - 1;
}

struct GearLook {
	LookTier shield = LookTier::Own;
	LookTier sword = LookTier::Own;
};

/**
 * @brief What @p player's hands say about the look, decided by the BASE item so a unique's own cursor changes
 * nothing. Each armour tier was drawn with a shield of its own, and since v1.12.027 the shield follows the ITEM
 * rather than the armour: Buckler and Small Shield are the light tier's round buckler, Large and Kite Shield the
 * medium tier's steel heater with the cross, Tower and Gothic Shield the heavy tier's blue heater with the lion -
 * on any body, so a knight in plate can carry a buckler. The big swords (Long, Broad, Bastard, Two-Handed, Great)
 * are the heavy tier's longsword. Anything else keeps its tier's look.
 */
GearLook GearLookFor(const Player &player);

/** @brief The look as one byte, for "did it change" - CalcPlrItemVals reloads the sheets when it does. */
uint8_t GearLookCode(const Player &player);

/** @brief Everything a derived sheet depends on - captured on the main thread, used on either. */
struct PlayerSheetRequest {
	HeroClass spriteClass = HeroClass::Warrior;
	uint8_t armour = 0; // ArmourChar index
	PlayerWeaponGraphic weapon = PlayerWeaponGraphic::Unarmed;
	GearLook look;
	std::string cel;
	uint16_t frameWidth = 0;
	/** The size of the class, applied after the mix so the cache holds the sheet as it is drawn. */
	int scalePercent = 100;
	/** The colours of the class, or null; pieces brought in are moved off the indices it dyes. */
	std::shared_ptr<const SpriteColours> dye;
	uint8_t dyeId = 0;

	/** @brief The cache key - also the file name on disk, so it is letters, digits and dashes only. */
	[[nodiscard]] std::string Key() const;
};

PlayerSheetRequest MakePlayerSheetRequest(const Player &player, HeroClass spriteClass, PlayerWeaponGraphic weapon, const char *szCel, uint16_t frameWidth);

/** @brief Cheap: whether the look asks for anything on this body, weapon class and armour at all. */
bool WantsMixedSheet(const PlayerSheetRequest &request);

/**
 * @brief Builds the sheet HERE AND NOW, reading the archive as it goes. For the export tool, which has no game
 * loop to spread the work over. The game never calls this: it is the lag the cache and the worker exist to remove.
 */
std::optional<ColouredSpriteSheet> MixPlayerSheetNow(const PlayerSheetRequest &request);

enum class CachedSheetState : uint8_t {
	/** Built before, this session or any other: @p out holds a copy. */
	Ready,
	/** Asked before, and the mixer declined (the sheets are no twins): load the plain sheet, ask no more. */
	Nothing,
	/** Never built: load the plain sheet for now and RequestPlayerSheet. */
	Unknown,
};

/**
 * @brief Whether this look is known to be unwearable: its standing or its walking sheet was declined. A look is worn
 * whole or not at all - see the definition. TakeCachedPlayerSheet and TakeFinishedPlayerSheets both apply it.
 */
bool LookDeclinedByCore(const PlayerSheetRequest &request);

/** @brief Memory first, then `<prefs>/sprite_cache/<key>.osm`. One small file read at worst; never a mix. */
CachedSheetState TakeCachedPlayerSheet(const PlayerSheetRequest &request, std::optional<ColouredSpriteSheet> &out);

/** @brief Queues the sheet to be built in the background. Asking twice for the same key is asking once. */
void RequestPlayerSheet(const PlayerSheetRequest &request);

/** @brief Once a game tick, main thread: reads ONE archive sheet for the job in hand, and hands a fully read job to the worker. */
void PumpSpriteMixer();

struct FinishedPlayerSheet {
	std::string key;
	bool nothing = false;
	/** A fresh copy of the sheet for one hero. */
	std::function<ColouredSpriteSheet()> make;
};

/** @brief What the worker finished since the last call, for the caller to put on whoever is waiting for it. */
std::vector<FinishedPlayerSheet> TakeFinishedPlayerSheets();

/** @brief Stops and JOINS the worker. Called on the way out of the game. */
void ShutdownSpriteMixer();

} // namespace devilution::oracool
