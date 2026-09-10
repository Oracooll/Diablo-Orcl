/**
 * @file misdat.h
 *
 * Interface of data related to missiles.
 */
#pragma once

#include <cstdint>
#include <type_traits>
#include <vector>

#include "effects.h"
#include "engine.h"
#include "engine/clx_sprite.hpp"
#include "spelldat.h"
#include "utils/enum_traits.h"
#include "utils/stdcompat/cstddef.hpp"
#include "utils/stdcompat/string_view.hpp"

namespace devilution {

enum mienemy_type : uint8_t {
	TARGET_MONSTERS,
	TARGET_PLAYERS,
	TARGET_BOTH,
};

enum class DamageType : uint8_t {
	Physical,
	Fire,
	Lightning,
	Magic,
	Acid,
	/**
	 * Oracool, Round 1 of the inert-skill plan (2026-09-03): the fifth element.
	 *
	 * APPENDED, not inserted, and that is load-bearing twice over. MissileDataFlags packs a
	 * DamageType into its low three bits, so every existing missile's flags value has to keep its
	 * number; and DamageType::Acid is 4, so Cold is 5 and the mask (0b111) still holds it.
	 *
	 * Thirteen inert skill rows were waiting on this one missing noun - ten of the Sorceress's and
	 * three of the Rogue's arrows - which is why it is the first round rather than the tidiest.
	 */
	Cold,
};

enum class MissileGraphicID : uint8_t {
	Arrow,
	Fireball,
	Guardian,
	Lightning,
	FireWall,
	MagmaBallExplosion,
	TownPortal,
	FlashBottom,
	FlashTop,
	ManaShield,
	BloodHit,
	BoneHit,
	MetalHit,
	FireArrow,
	DoomSerpents,
	Golem,
	Spurt,
	ApocalypseBoom,
	StoneCurseShatter,
	BigExplosion,
	Inferno,
	ThinLightning,
	BloodStar,
	BloodStarExplosion,
	MagmaBall,
	Krull,
	ChargedBolt,
	HolyBolt,
	HolyBoltExplosion,
	LightningArrow,
	FireArrowExplosion,
	Acid,
	AcidSplat,
	AcidPuddle,
	Etherealize,
	Elemental,
	Resurrect,
	BoneSpirit,
	RedPortal,
	DiabloApocalypseBoom,
	BloodStarBlue,
	BloodStarBlueExplosion,
	BloodStarYellow,
	BloodStarYellowExplosion,
	BloodStarRed,
	BloodStarRedExplosion,
	HorkSpawn,
	Reflect,
	OrangeFlare,
	BlueFlare,
	RedFlare,
	YellowFlare,
	Rune,
	YellowFlareExplosion,
	BlueFlareExplosion,
	RedFlareExplosion,
	BlueFlare2,
	OrangeFlareExplosion,
	BlueFlareExplosion2,
	/**
	 * Oracool, the Cold pack (2026-09-03). Appended before None, because MissileSpriteData is indexed
	 * by this enum positionally and every existing value has to keep its number.
	 *
	 * The art is missiles\ice_bolt.png and missiles\ice_impact.png - PNG sheets rather than CL2s,
	 * which MissileFileData::LoadGFX learned to read in v1.9.181.
	 */
	IceBolt,
	IceImpact,
	// Round 2: the rest of the pack. ice_ground.png and freezing_burst.png are in the archive but
	// not here yet - the ground patch has no spell to leave it, and the burst is Freezing Arrow's,
	// which is Round 3's.
	IceBlast,
	GlacialSpike,
	GlacialShatter,
	FrostNova,
	BlizzardShard,
	FrozenOrb,
	IceArmorShell,
	IceArmorBreak,
	// Round 3: the Rogue's cold arrow and Freezing Arrow's landing. Last two of the thirteen.
	FrostArrow,
	FreezingBurst,
	/**
	 * Oracool (2026-09-11): Blessed Hammer's own sprite - missileslessed_hammer_spin.png, sixteen
	 * 48x48 frames of one hammer turning clockwise 22.5 degrees a frame, generated art. Until now the
	 * hammer wore the mace item's drop tumble painted gold.
	 */
	BlessedHammerSpin,
	/**
	 * Oracool (2026-09-11): the rest of the skill sheets from Resources\ORCL-skill-asset-briefs.md,
	 * registered before their art exists. Each is PngOnly: while its PNG is not in the archive the
	 * slot stays empty and the skill keeps the sprite it borrowed, so a delivered batch lands with a
	 * copy and a repack and no code change. MissileArtLoaded is the question every user asks.
	 */
	FistOfHeavensBolt,
	BlessedShieldSpin,
	HolySpark,
	MagicArrowLight,
	GuidedArrowGold,
	WarcryRing,
	HitFire,
	HitLightning,
	HitCold,
	None,
};

/**
 * @brief Specifies what if and how movement distribution is applied
 */
enum class MissileMovementDistribution : uint8_t {
	/**
	 * @brief No movement distribution is calculated. Normally this means the missile doesn't move at all.
	 */
	Disabled,
	/**
	 * @brief The missile moves and if it hits a enemey it stops (for example firebolt)
	 */
	Blockable,
	/**
	 * @brief The missile moves and even it hits a enemy it keeps moving (for example flame wave)
	 */
	Unblockable,
};

struct Missile;
struct AddMissileParameter;

enum class MissileDataFlags : uint8_t {
	// The lower 3 bytes are used to store DamageType.
	Physical = static_cast<uint8_t>(DamageType::Physical),
	Fire = static_cast<uint8_t>(DamageType::Fire),
	Lightning = static_cast<uint8_t>(DamageType::Lightning),
	Magic = static_cast<uint8_t>(DamageType::Magic),
	Acid = static_cast<uint8_t>(DamageType::Acid),
	Cold = static_cast<uint8_t>(DamageType::Cold),
	Arrow = 1 << 4,
	Invisible = 1 << 5,
};
use_enum_as_flags(MissileDataFlags);

struct MissileData {
	void (*mAddProc)(Missile &, AddMissileParameter &);
	void (*mProc)(Missile &);
	_sfx_id mlSFX;
	_sfx_id miSFX;
	MissileGraphicID mFileNum;
	MissileDataFlags flags;
	MissileMovementDistribution movementDistribution;

	[[nodiscard]] bool isDrawn() const
	{
		return !HasAnyOf(flags, MissileDataFlags::Invisible);
	}

	[[nodiscard]] bool isArrow() const
	{
		return HasAnyOf(flags, MissileDataFlags::Arrow);
	}

	[[nodiscard]] DamageType damageType() const
	{
		return static_cast<DamageType>(static_cast<std::underlying_type<MissileDataFlags>::type>(flags) & 0b111U);
	}
};

enum class MissileGraphicsFlags : uint8_t {
	// clang-format off
	None         = 0,
	MonsterOwned = 1 << 0,
	NotAnimated  = 1 << 1,
	// Oracool: art this fork ships as a PNG and nothing else. No PNG, no sprite - never a CL2 lookup
	// for a file that has never existed. See MissileArtLoaded.
	PngOnly      = 1 << 2,
	// clang-format on
};

struct MissileFileData {
	OptionalOwnedClxSpriteListOrSheet sprites;
	uint16_t animWidth;
	int8_t animWidth2;
	/**
	 * The art's file name, without extension.
	 *
	 * NINE bytes until 2026-09-03, which is 8.3 - every original missile is a DOS-era CL2 called
	 * "fireba" or "magblos". The Cold pack's sheets are named for what they are ("ice_impact",
	 * "glacial_shatter"), and there is no reason for art this fork ships to obey a filename limit
	 * from 1996. Twenty holds the longest of the thirteen with room, and costs eleven bytes a row in
	 * a table of about a hundred.
	 */
	// Twenty-four since 2026-09-11: "fist_of_heavens_bolt" is twenty on its own.
	char name[24];
	uint8_t animFAmt;
	MissileGraphicsFlags flags;
	uint8_t animDelayIdx;
	uint8_t animLenIdx;

	[[nodiscard]] uint8_t animDelay(uint8_t dir) const;
	[[nodiscard]] uint8_t animLen(uint8_t dir) const;

	void LoadGFX();

	void FreeGFX()
	{
		sprites = std::nullopt;
	}

	/**
	 * @brief Returns the sprite list for a given direction.
	 *
	 * @param direction One of the 16 directions. Valid range: [0, 15].
	 * @return OptionalClxSpriteList
	 */
	[[nodiscard]] OptionalClxSpriteList spritesForDirection(size_t direction) const
	{
		if (!sprites)
			return std::nullopt;
		return sprites->isSheet() ? sprites->sheet()[direction] : sprites->list();
	}
};

extern const MissileData MissilesData[];

inline const MissileData &GetMissileData(MissileID missileId)
{
	return MissilesData[static_cast<std::underlying_type<MissileID>::type>(missileId)];
}

extern MissileFileData MissileSpriteData[];

inline MissileFileData &GetMissileSpriteData(MissileGraphicID graphicId)
{
	return MissileSpriteData[static_cast<std::underlying_type<MissileGraphicID>::type>(graphicId)];
}

/**
 * @brief Oracool: whether a graphic's art is actually loaded - false for a PngOnly slot whose sheet
 * has not been delivered yet, and for everything in headless mode. The skills that wait on new art
 * ask this and keep their borrowed sprite until it says yes.
 */
inline bool MissileArtLoaded(MissileGraphicID graphicId)
{
	return graphicId < MissileGraphicID::None && GetMissileSpriteData(graphicId).sprites.has_value();
}

void InitMissileGFX(bool loadHellfireGraphics = false);
void FreeMissileGFX();

} // namespace devilution
