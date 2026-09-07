/**
 * @file init.h
 *
 * Interface of routines for initializing the environment, disable screen saver, load MPQ.
 */
#pragma once

#include "utils/attributes.h"
#include "utils/stdcompat/optional.hpp"

#ifdef UNPACKED_MPQS
#include <string>
#else
#include "mpq/mpq_reader.hpp"
#endif

#include <SDL.h>

namespace devilution {

extern bool gbActive;
extern DVL_API_FOR_TEST bool gbIsSpawn;
extern DVL_API_FOR_TEST bool gbIsHellfire;
extern DVL_API_FOR_TEST bool gbVanilla;
extern bool forceHellfire;

#ifdef UNPACKED_MPQS
extern DVL_API_FOR_TEST std::optional<std::string> spawn_data_path;
extern DVL_API_FOR_TEST std::optional<std::string> diabdat_data_path;
extern std::optional<std::string> hellfire_data_path;
extern std::optional<std::string> font_data_path;
extern std::optional<std::string> lang_data_path;
#else
/** A handle to the spawn.mpq archive. */
extern DVL_API_FOR_TEST std::optional<MpqArchive> spawn_mpq;
/** A handle to the diabdat.mpq archive. */
extern DVL_API_FOR_TEST std::optional<MpqArchive> diabdat_mpq;
/** A handle to an hellfire.mpq archive. */
extern std::optional<MpqArchive> hellfire_mpq;
extern std::optional<MpqArchive> hfmonk_mpq;
extern std::optional<MpqArchive> hfbard_mpq;
extern std::optional<MpqArchive> hfbarb_mpq;
extern std::optional<MpqArchive> hfmusic_mpq;
extern std::optional<MpqArchive> hfvoice_mpq;
extern std::optional<MpqArchive> font_mpq;
extern std::optional<MpqArchive> lang_mpq;
extern std::optional<MpqArchive> devilutionx_mpq;
/**
 * @brief Oracool Edition's own asset archive.
 *
 * Built from Packaging/resources/oracool_assets/ by tools/oracool_mpq_pack, and searched ahead of
 * every other archive (see FindMpqFile in engine/assets.cpp) so anything Oracool ships overrides
 * the original game data without modifying diabdat.mpq. Optional: the game runs normally when it
 * is absent, falling back to whatever the other archives provide.
 */
extern std::optional<MpqArchive> oracool_mpq;
/**
 * Oracool (IP audit, 2026-09-07): the PRIVATE archive - art the user made from reworked Blizzard
 * textures, which stays part of the mod on his machine and is never distributed. Packed from a
 * folder outside the repository; absent in a public build, where every lookup falls through to
 * oracool.mpq and the game's own art.
 */
extern std::optional<MpqArchive> oracool_private_mpq;
#endif

inline bool HaveSpawn()
{
#ifdef UNPACKED_MPQS
	return bool(spawn_data_path);
#else
	return bool(spawn_mpq);
#endif
}

inline bool HaveDiabdat()
{
#ifdef UNPACKED_MPQS
	return bool(diabdat_data_path);
#else
	return bool(diabdat_mpq);
#endif
}

inline bool HaveHellfire()
{
#ifdef UNPACKED_MPQS
	return bool(hellfire_data_path);
#else
	return bool(hellfire_mpq);
#endif
}

/**
 * @brief Whether the Monk's own data is present.
 *
 * Oracool: the Monk is the one class Hellfire actually drew. `hfmonk.mpq` carries a complete sprite
 * set at its own scale - 112px idles, 130px attacks, a 160px death frame, matching no other class -
 * where `hfbard.mpq` and `hfbarb.mpq` carry stats and voice for classes that wear the Rogue's and the
 * Warrior's art (see the classPath column in PlayersData). So the Monk is the one added class worth
 * offering on its own, without dragging Hellfire's quests, levels and monsters along with it.
 *
 * Separate from HaveHellfire on purpose: `gbIsHellfire` is set by hellfire.mpq and changes the whole
 * game, while this asks only "is there a Monk to draw".
 *
 * Unpacked builds have no separate monk path - everything Hellfire ships sits under one folder - so
 * there this necessarily means the same thing as HaveHellfire().
 */
inline bool HaveMonk()
{
#ifdef UNPACKED_MPQS
	return bool(hellfire_data_path);
#else
	return bool(hfmonk_mpq);
#endif
}

inline bool HaveExtraFonts()
{
#ifdef UNPACKED_MPQS
	return bool(font_data_path);
#else
	return bool(font_mpq);
#endif
}

#ifdef UNPACKED_MPQS
bool AreExtraFontsOutOfDate(const std::string &path);
#else
bool AreExtraFontsOutOfDate(MpqArchive &archive);
#endif

inline bool AreExtraFontsOutOfDate()
{
#ifdef UNPACKED_MPQS
	return font_data_path && AreExtraFontsOutOfDate(*font_data_path);
#else
	return font_mpq && AreExtraFontsOutOfDate(*font_mpq);
#endif
}

void init_cleanup();
void LoadCoreArchives();
void LoadLanguageArchive();
void LoadGameArchives();
void init_create_window();
void MainWndProc(const SDL_Event &event);

} // namespace devilution
