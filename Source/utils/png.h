#pragma once

#include <SDL.h>

#include "engine/assets.hpp"

#ifdef __cplusplus
extern "C" {
#endif

const int IMG_INIT_PNG = 0x00000002;

int IMG_Init(int flags);
void IMG_Quit(void);
int IMG_isPNG(SDL_RWops *src);
SDL_Surface *IMG_LoadPNG_RW(SDL_RWops *src);
int IMG_SavePNG(SDL_Surface *surface, const char *file);
int IMG_SavePNG_RW(SDL_Surface *surface, SDL_RWops *dst, int freedst);

inline SDL_Surface *IMG_LoadPNG(const char *file)
{
	SDL_RWops *src = SDL_RWFromFile(file, "rb");
	return IMG_LoadPNG_RW(src);
}

#ifdef __cplusplus
}
#endif

namespace devilution {

inline int InitPNG()
{
	return IMG_Init(IMG_INIT_PNG);
}

inline void QuitPNG()
{
	IMG_Quit();
}

/**
 * @brief Loads @p file as a surface, or nullptr if it is not there.
 *
 * Oracool: the null guard is the fix for a crash, not defensive habit. OpenAssetAsSdlRwOps returns
 * null for an asset that does not exist, and both IMG_LoadPNG_RW and SDL_RWclose dereference their
 * argument - so a missing file was an access violation inside SDL rather than a failed load.
 *
 * Upstream never met it because every caller here passes a path that ships with the game
 * (ui_art\button.png, menu.png, directions.png). oracool/sprite_import.cpp broke that assumption:
 * LoadPlrGFX probes plrgfx\<class>\...png for every animation of every character, and those files
 * are ABSENT by design until someone supplies a converted sprite set. The fallback to the .cl2 was
 * written and could never be reached, because the probe died first.
 *
 * Diagnosed from the minidump: READ of address 0x20 with a null first argument, unwinding through
 * LoadPNG -> LoadPngSpriteSheet -> LoadPlrGFX -> SetPlrAnims -> InitPlayer, which is why it fired on
 * creating or entering a game with any character and never anywhere else.
 */
inline SDL_Surface *LoadPNG(const char *file)
{
	SDL_RWops *rwops = OpenAssetAsSdlRwOps(file);
	if (rwops == nullptr)
		return nullptr;
	SDL_Surface *surface = IMG_LoadPNG_RW(rwops);
	SDL_RWclose(rwops);
	return surface;
}

} // namespace devilution
