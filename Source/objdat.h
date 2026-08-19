/**
 * @file objdat.h
 *
 * Interface of all object data.
 */
#pragma once

#include <cstdint>

#include "levels/gendung.h"
#include "utils/enum_traits.h"

namespace devilution {

enum theme_id : int8_t {
	THEME_BARREL,
	THEME_SHRINE,
	THEME_MONSTPIT,
	THEME_SKELROOM,
	THEME_TREASURE,
	THEME_LIBRARY,
	THEME_TORTURE,
	THEME_BLOODFOUNTAIN,
	THEME_DECAPITATED,
	THEME_PURIFYINGFOUNTAIN,
	THEME_ARMORSTAND,
	THEME_GOATSHRINE,
	THEME_CAULDRON,
	THEME_MURKYFOUNTAIN,
	THEME_TEARFOUNTAIN,
	THEME_BRNCROSS,
	THEME_WEAPONRACK,
	THEME_NONE = -1,
};

enum object_graphic_id : int8_t {
	OFILE_L1BRAZ,
	OFILE_L1DOORS,
	OFILE_LEVER,
	OFILE_CHEST1,
	OFILE_CHEST2,
	OFILE_BANNER,
	OFILE_SKULPILE,
	OFILE_SKULFIRE,
	OFILE_SKULSTIK,
	OFILE_CRUXSK1,
	OFILE_CRUXSK2,
	OFILE_CRUXSK3,
	OFILE_BOOK1,
	OFILE_BOOK2,
	OFILE_ROCKSTAN,
	OFILE_ANGEL,
	OFILE_CHEST3,
	OFILE_BURNCROS,
	OFILE_CANDLE2,
	OFILE_NUDE2,
	OFILE_SWITCH4,
	OFILE_TNUDEM,
	OFILE_TNUDEW,
	OFILE_TSOUL,
	OFILE_L2DOORS,
	OFILE_WTORCH4,
	OFILE_WTORCH3,
	OFILE_SARC,
	OFILE_FLAME1,
	OFILE_PRSRPLT1,
	OFILE_TRAPHOLE,
	OFILE_MINIWATR,
	OFILE_WTORCH2,
	OFILE_WTORCH1,
	OFILE_BCASE,
	OFILE_BSHELF,
	OFILE_WEAPSTND,
	OFILE_BARREL,
	OFILE_BARRELEX,
	OFILE_LSHRINEG,
	OFILE_RSHRINEG,
	OFILE_BLOODFNT,
	OFILE_DECAP,
	OFILE_PEDISTL,
	OFILE_L3DOORS,
	OFILE_PFOUNTN,
	OFILE_ARMSTAND,
	OFILE_GOATSHRN,
	OFILE_CAULDREN,
	OFILE_MFOUNTN,
	OFILE_TFOUNTN,
	OFILE_ALTBOY,
	OFILE_MCIRL,
	OFILE_BKSLBRNT,
	OFILE_MUSHPTCH,
	OFILE_LZSTAND,
	OFILE_POD,
	OFILE_PODEX,
	OFILE_L5DOORS,
	OFILE_L5LEVER,
	OFILE_L5CANDLE,
	OFILE_L5SARC,
	OFILE_URN,
	OFILE_URNEX,
	OFILE_L5BOOKS,
	/**
	 * Oracool: user request - the waypoint's own graphic (objects\orclwayp.cel, shipped in
	 * oracool.mpq, built by tools/WaypointCel.cs from the user's two-state painting).
	 *
	 * Deliberately its own file rather than a replacement for OFILE_MCIRL, which is what
	 * OBJ_WAYPOINT borrowed while it had no art: mcirl.cel is also OBJ_MCIRCLE1/OBJ_MCIRCLE2, the
	 * two magic circles in the Archbishop Lazarus quest, and shadowing it would silently turn those
	 * into waypoint platforms too.
	 *
	 * Note this sits AFTER OFILE_L5BOOKS, which is the upper bound of LoadLevelObjects' loop. That
	 * is intentional and not an oversight: the waypoint is registered explicitly by
	 * AddWaypointSigilObject/EnsureWaypointGraphicsLoaded on every level rather than by matching a
	 * level type, since it is the one object that appears in town AND on all 16 dungeon levels.
	 */
	OFILE_ORCLWAYP,
	/**
	 * Oracool: the town Stash Chest's own art - the Grand Reliquary (objects\orclstash.cel, shipped
	 * in oracool.mpq). Six 76x70 frames: 1/2/3 and 4/5/6 are two identical closed/opening/open
	 * trios, mirroring chest3.cel's two-variant convention, so the existing "closed is frame 4, open
	 * is frame 6" logic carries over unchanged.
	 *
	 * Like OFILE_ORCLWAYP this sits after OFILE_L5BOOKS and is registered explicitly rather than by
	 * a level scan (EnsureObjectGraphicsLoaded, from AddStashChestObject). Unlike the waypoint, it
	 * does NOT belong to an object type: the Stash Chest stays an ordinary OBJ_CHEST3 - see
	 * ApplyStashChestGraphics in objects.cpp for why, and for the two places that must both apply it.
	 */
	OFILE_ORCLSTASH,
	/**
	 * Oracool: Levski's Roar, the town monument (objects\orclroar.cel, shipped in oracool.mpq,
	 * built by tools/MonumentCel.cs from the user's painting).
	 *
	 * One frame: OBJ_STAND is static (animLen 0, no Animated flag), so there is nothing to step
	 * through. Until 2026-08-20 the monument wore OFILE_ROCKSTAN, the Anvil of Fury's rock stand,
	 * as an openly-labelled placeholder.
	 *
	 * Like the two above it this sits after OFILE_L5BOOKS and is registered explicitly rather than
	 * by a level scan, and like OFILE_ORCLSTASH it does NOT belong to an object type: the monument
	 * stays an ordinary OBJ_STAND wearing its own art, so the Caves' rock stands are untouched. See
	 * ApplyLevskiRoarGraphics in objects.cpp for the two places that must both apply it.
	 */
	OFILE_ORCLROAR,
	OFILE_NULL = -1,
};

/** @brief Number of entries in object_graphic_id, i.e. the size every filesWidths[] array needs. */
constexpr int NumObjectGraphicFiles = OFILE_ORCLROAR + 1;

/**
 * @brief Oracool: orclstash.cel's frame width. CEL stores no width, so LoadCel must be told; a
 * wrong one splits this sprite's RLE scanlines mid-row and renders it as garbage.
 *
 * Not the delivered pack's 160: at full size the reliquary spanned two and a half floor tiles and
 * read as a building rather than a chest (user, 2026-08-18 - "this is too big"). The sprite is
 * re-cut at half scale from the RGBA masters by tools\build_reliquary_cel.cmd. That tool can also
 * paint a contact shadow, which grows the frame to 82x80; it is parked off (see EnableShadow there)
 * and this width is the unshadowed one. The tool prints the width it produced; if its output ever
 * changes, this constant changes with it, in the same commit.
 */
constexpr uint16_t OracoolStashChestAnimWidth = 76;

/**
 * @brief Oracool: orclroar.cel's frame width. Same contract as the constant above - CEL stores no
 * width, and a wrong one renders the monument as garbage rather than failing.
 *
 * Three town tiles. The painting is 971px wide once padded to its floor anchor; the scale is chosen
 * so the plaza reads as a plaza rather than as a piece of furniture, and so the statue stands about
 * twice a player's height. tools\build_levski_roar_cel.cmd passes this number in and the tool prints
 * back what it produced; if either moves, both move, in the same commit.
 */
constexpr uint16_t OracoolLevskiRoarAnimWidth = 192;

enum _object_id : int8_t {
	OBJ_L1LIGHT,
	OBJ_L1LDOOR,
	OBJ_L1RDOOR,
	OBJ_SKFIRE,
	OBJ_LEVER,
	OBJ_CHEST1,
	OBJ_CHEST2,
	OBJ_CHEST3,
	OBJ_CANDLE1,
	OBJ_CANDLE2,
	OBJ_CANDLEO,
	OBJ_BANNERL,
	OBJ_BANNERM,
	OBJ_BANNERR,
	OBJ_SKPILE,
	OBJ_SKSTICK1,
	OBJ_SKSTICK2,
	OBJ_SKSTICK3,
	OBJ_SKSTICK4,
	OBJ_SKSTICK5,
	OBJ_CRUX1,
	OBJ_CRUX2,
	OBJ_CRUX3,
	OBJ_STAND,
	OBJ_ANGEL,
	OBJ_BOOK2L,
	OBJ_BCROSS,
	OBJ_NUDEW2R,
	OBJ_SWITCHSKL,
	OBJ_TNUDEM1,
	OBJ_TNUDEM2,
	OBJ_TNUDEM3,
	OBJ_TNUDEM4,
	OBJ_TNUDEW1,
	OBJ_TNUDEW2,
	OBJ_TNUDEW3,
	OBJ_TORTURE1,
	OBJ_TORTURE2,
	OBJ_TORTURE3,
	OBJ_TORTURE4,
	OBJ_TORTURE5,
	OBJ_BOOK2R,
	OBJ_L2LDOOR,
	OBJ_L2RDOOR,
	OBJ_TORCHL,
	OBJ_TORCHR,
	OBJ_TORCHL2,
	OBJ_TORCHR2,
	OBJ_SARC,
	OBJ_FLAMEHOLE,
	OBJ_FLAMELVR,
	OBJ_WATER,
	OBJ_BOOKLVR,
	OBJ_TRAPL,
	OBJ_TRAPR,
	OBJ_BOOKSHELF,
	OBJ_WEAPRACK,
	OBJ_BARREL,
	OBJ_BARRELEX,
	OBJ_SHRINEL,
	OBJ_SHRINER,
	OBJ_SKELBOOK,
	OBJ_BOOKCASEL,
	OBJ_BOOKCASER,
	OBJ_BOOKSTAND,
	OBJ_BOOKCANDLE,
	OBJ_BLOODFTN,
	OBJ_DECAP,
	OBJ_TCHEST1,
	OBJ_TCHEST2,
	OBJ_TCHEST3,
	OBJ_BLINDBOOK,
	OBJ_BLOODBOOK,
	OBJ_PEDESTAL,
	OBJ_L3LDOOR,
	OBJ_L3RDOOR,
	OBJ_PURIFYINGFTN,
	OBJ_ARMORSTAND,
	OBJ_ARMORSTANDN,
	OBJ_GOATSHRINE,
	OBJ_CAULDRON,
	OBJ_MURKYFTN,
	OBJ_TEARFTN,
	OBJ_ALTBOY,
	OBJ_MCIRCLE1,
	OBJ_MCIRCLE2,
	OBJ_STORYBOOK,
	OBJ_STORYCANDLE,
	OBJ_STEELTOME,
	OBJ_WARARMOR,
	OBJ_WARWEAP,
	OBJ_TBCROSS,
	OBJ_WEAPONRACK,
	OBJ_WEAPONRACKN,
	OBJ_MUSHPATCH,
	OBJ_LAZSTAND,
	OBJ_SLAINHERO,
	OBJ_SIGNCHEST,
	OBJ_BOOKSHELFR,
	OBJ_POD,
	OBJ_PODEX,
	OBJ_URN,
	OBJ_URNEX,
	OBJ_L5BOOKS,
	OBJ_L5CANDLE,
	OBJ_L5LDOOR,
	OBJ_L5RDOOR,
	OBJ_L5LEVER,
	OBJ_L5SARC,
	// Oracool: user request - Waypoints. Started life borrowing the mcirl magic-circle graphic as a
	// placeholder; now draws its own two-state art (see OFILE_ORCLWAYP). Frame 1 is the dormant
	// platform, frame 2 the lit one, set directly rather than by an animation.
	OBJ_WAYPOINT,
	OBJ_NULL = -1,
};

enum quest_id : int8_t {
	Q_ROCK,
	Q_MUSHROOM,
	Q_GARBUD,
	Q_ZHAR,
	Q_VEIL,
	Q_DIABLO,
	Q_BUTCHER,
	Q_LTBANNER,
	Q_BLIND,
	Q_BLOOD,
	Q_ANVIL,
	Q_WARLORD,
	Q_SKELKING,
	Q_PWATER,
	Q_SCHAMB,
	Q_BETRAYER,
	Q_GRAVE,
	Q_FARMER,
	Q_GIRL,
	Q_TRADER,
	Q_DEFILER,
	Q_NAKRUL,
	Q_CORNSTN,
	Q_JERSEY,
	Q_INVALID = -1,
};

enum class ObjectDataFlags : uint8_t {
	Animated = 1U,
	Solid = 1U << 1,
	MissilesPassThrough = 1U << 2,
	Light = 1U << 3,
	Trap = 1U << 4,
	Breakable = 1U << 5,
};
use_enum_as_flags(ObjectDataFlags);

struct ObjectData {
	object_graphic_id ofindex;
	int8_t minlvl;
	int8_t maxlvl;
	dungeon_type olvltype;
	theme_id otheme;
	quest_id oquest;
	ObjectDataFlags flags;
	uint8_t animDelay; // Tick length of each frame in the current animation
	uint8_t animLen;   // Number of frames in current animation
	uint8_t animWidth;
	int8_t selFlag; // TODO Create enum

	[[nodiscard]] bool isAnimated() const
	{
		return HasAnyOf(flags, ObjectDataFlags::Animated);
	}

	[[nodiscard]] bool isSolid() const
	{
		return HasAnyOf(flags, ObjectDataFlags::Solid);
	}

	[[nodiscard]] bool missilesPassThrough() const
	{
		return HasAnyOf(flags, ObjectDataFlags::MissilesPassThrough);
	}

	[[nodiscard]] bool applyLighting() const
	{
		return HasAnyOf(flags, ObjectDataFlags::Light);
	}

	[[nodiscard]] bool isTrap() const
	{
		return HasAnyOf(flags, ObjectDataFlags::Trap);
	}

	[[nodiscard]] bool isBreakable() const
	{
		return HasAnyOf(flags, ObjectDataFlags::Breakable);
	}
};

extern const _object_id ObjTypeConv[];
extern const ObjectData AllObjects[110];
extern const char *const ObjMasterLoadList[];

} // namespace devilution
