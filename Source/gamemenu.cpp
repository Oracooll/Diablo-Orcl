/**
 * @file gamemenu.cpp
 *
 * Implementation of the in-game menu functions.
 */
#include "gamemenu.h"

#include "cursor.h"
#include "engine/backbuffer_state.hpp"
#include "engine/events.hpp"
#include "engine/sound.h"
#include "engine/sound_defs.hpp"
#include "error.h"
#include "gmenu.h"
#include "init.h"
#include "loadsave.h"
#include "options.h"
#include "oracool/event_log.h"
#include "oracool/oracool.h"
#include "pfile.h"
#include "player.h"
#include "qol/floatingnumbers.h"
#include "utils/language.h"

namespace devilution {
namespace {

// Forward-declare menu handlers, used by the global menu structs below.
void GamemenuPrevious(bool bActivate);
void GamemenuNewGame(bool bActivate);
void GamemenuRestartTown(bool bActivate);
void GamemenuRespawnInTown(bool bActivate);
void GamemenuOptions(bool bActivate);
void GamemenuMusicVolume(bool bActivate);
void GamemenuSoundVolume(bool bActivate);
void GamemenuGamma(bool bActivate);
void GamemenuSpeed(bool bActivate);

/**
 * @brief Oracool: user request - the pause menu is trimmed to exactly these five entries while
 * the player is alive. "Main Menu"/"Exit Game" reuse the same handlers the menu already had
 * under their vanilla names ("New Game"/"Quit Game") - both already do exactly what their new
 * names say (abandon the current game and return to the main menu; abandon the current game and
 * close the application), so only the label changed.
 */
TMenuItem sgSingleMenu[] = {
	// clang-format off
	// dwFlags,      pszStr,                 fnMenu
	{ GMENU_ENABLED, N_("Save Game"),       &gamemenu_save_game    },
	{ GMENU_ENABLED, N_("Load Game"),       &gamemenu_load_game    },
	{ GMENU_ENABLED, N_("Main Menu"),       &GamemenuNewGame       },
	{ GMENU_ENABLED, N_("Options"),         &GamemenuOptions       },
	{ GMENU_ENABLED, N_("Exit Game"),       &gamemenu_quit_game    },
	{ GMENU_ENABLED, nullptr,               nullptr                }
	// clang-format on
};
/**
 * @brief Oracool: user request - "Respawn In Town" should only ever be visible, not merely
 * enabled, while the character is dead - the underlying menu system draws every item between the
 * array's start and its nullptr terminator unconditionally (enabling/disabling an item only
 * changes its color, not whether it's shown), so a separate array is the only way to actually
 * remove it from the list the rest of the time. Replaces "Save Game" in this slot, which was
 * already effectively non-functional while dead (gamemenu_save_game no-ops in that state).
 */
TMenuItem sgSingleMenuOnDeath[] = {
	// clang-format off
	// dwFlags,      pszStr,                 fnMenu
	{ GMENU_ENABLED, N_("Respawn In Town"), &GamemenuRespawnInTown },
	{ GMENU_ENABLED, N_("Load Game"),       &gamemenu_load_game    },
	{ GMENU_ENABLED, N_("Main Menu"),       &GamemenuNewGame       },
	{ GMENU_ENABLED, N_("Options"),         &GamemenuOptions       },
	{ GMENU_ENABLED, N_("Exit Game"),       &gamemenu_quit_game    },
	{ GMENU_ENABLED, nullptr,               nullptr                }
	// clang-format on
};
/** Contains the game menu items of the multi player menu. */
TMenuItem sgMultiMenu[] = {
	// clang-format off
	// dwFlags,      pszStr,                fnMenu
	{ GMENU_ENABLED, N_("Options"),         &GamemenuOptions     },
	{ GMENU_ENABLED, N_("New Game"),        &GamemenuNewGame     },
	{ GMENU_ENABLED, N_("Restart In Town"), &GamemenuRestartTown },
	{ GMENU_ENABLED, N_("Quit Game"),       &gamemenu_quit_game  },
	{ GMENU_ENABLED, nullptr,               nullptr              },
	// clang-format on
};
TMenuItem sgOptionsMenu[] = {
	// clang-format off
	// dwFlags,                     pszStr,              fnMenu
	{ GMENU_ENABLED | GMENU_SLIDER, nullptr,             &GamemenuMusicVolume  },
	{ GMENU_ENABLED | GMENU_SLIDER, nullptr,             &GamemenuSoundVolume  },
	{ GMENU_ENABLED | GMENU_SLIDER, N_("Gamma"),         &GamemenuGamma        },
	{ GMENU_ENABLED | GMENU_SLIDER, N_("Speed"),         &GamemenuSpeed        },
	{ GMENU_ENABLED               , N_("Previous Menu"), &GamemenuPrevious     },
	{ GMENU_ENABLED               , nullptr,             nullptr               },
	// clang-format on
};
/** Specifies the menu names for music enabled and disabled. */
const char *const MusicToggleNames[] = {
	N_("Music"),
	N_("Music Disabled"),
};
/** Specifies the menu names for sound enabled and disabled. */
const char *const SoundToggleNames[] = {
	N_("Sound"),
	N_("Sound Disabled"),
};

void GamemenuUpdateSingle()
{
	// Oracool: index 1 (Load Game) is the same slot in both sgSingleMenu and
	// sgSingleMenuOnDeath - whichever array gamemenu_on() picked, this still applies correctly.
	sgSingleMenu[1].setEnabled(gbValidSaveFile);
	sgSingleMenuOnDeath[1].setEnabled(gbValidSaveFile);
}

void GamemenuUpdateMulti()
{
	sgMultiMenu[2].setEnabled(MyPlayerIsDead);
}

void GamemenuPrevious(bool /*bActivate*/)
{
	gamemenu_on();
}

void GamemenuNewGame(bool /*bActivate*/)
{
	for (Player &player : Players) {
		player._pmode = PM_QUIT;
		player._pInvincible = true;
	}

	MyPlayerIsDead = false;
	if (!HeadlessMode) {
		RedrawEverything();
		scrollrt_draw_game_screen();
	}
	CornerStone.activated = false;
	gbRunGame = false;
	gamemenu_off();
}

void GamemenuRestartTown(bool /*bActivate*/)
{
	NetSendCmd(true, CMD_RETOWN);
}

void GamemenuRespawnInTown(bool /*bActivate*/)
{
	if (!MyPlayerIsDead)
		return;

	MyPlayerIsDead = false;
	gamemenu_off();
	RestartTownLvl(*MyPlayer);
}

void GamemenuSoundMusicToggle(const char *const *names, TMenuItem *menuItem, int volume)
{
	if (gbSndInited) {
		menuItem->addFlags(GMENU_ENABLED | GMENU_SLIDER);
		menuItem->pszStr = names[0];
		gmenu_slider_steps(menuItem, VOLUME_STEPS);
		gmenu_slider_set(menuItem, VOLUME_MIN, VOLUME_MAX, volume);
		return;
	}

	menuItem->removeFlags(GMENU_ENABLED | GMENU_SLIDER);
	menuItem->pszStr = names[1];
}

int GamemenuSliderMusicSound(TMenuItem *menuItem)
{
	return gmenu_slider_get(menuItem, VOLUME_MIN, VOLUME_MAX);
}

void GamemenuGetMusic()
{
	GamemenuSoundMusicToggle(MusicToggleNames, sgOptionsMenu, sound_get_or_set_music_volume(1));
}

void GamemenuGetSound()
{
	GamemenuSoundMusicToggle(SoundToggleNames, &sgOptionsMenu[1], sound_get_or_set_sound_volume(1));
}

void GamemenuGetGamma()
{
	gmenu_slider_steps(&sgOptionsMenu[2], 15);
	gmenu_slider_set(&sgOptionsMenu[2], 30, 100, UpdateGamma(0));
}

void GamemenuGetSpeed()
{
	if (gbIsMultiplayer) {
		sgOptionsMenu[3].removeFlags(GMENU_ENABLED | GMENU_SLIDER);
		if (sgGameInitInfo.nTickRate >= 50)
			sgOptionsMenu[3].pszStr = _("Speed: Fastest").data();
		else if (sgGameInitInfo.nTickRate >= 40)
			sgOptionsMenu[3].pszStr = _("Speed: Faster").data();
		else if (sgGameInitInfo.nTickRate >= 30)
			sgOptionsMenu[3].pszStr = _("Speed: Fast").data();
		else if (sgGameInitInfo.nTickRate == 20)
			sgOptionsMenu[3].pszStr = _("Speed: Normal").data();
		return;
	}

	sgOptionsMenu[3].addFlags(GMENU_ENABLED | GMENU_SLIDER);

	sgOptionsMenu[3].pszStr = _("Speed").data();
	gmenu_slider_steps(&sgOptionsMenu[3], 46);
	gmenu_slider_set(&sgOptionsMenu[3], 20, 50, sgGameInitInfo.nTickRate);
}

int GamemenuSliderGamma()
{
	return gmenu_slider_get(&sgOptionsMenu[2], 30, 100);
}

void GamemenuOptions(bool /*bActivate*/)
{
	GamemenuGetMusic();
	GamemenuGetSound();
	GamemenuGetGamma();
	GamemenuGetSpeed();
	gmenu_set_items(sgOptionsMenu, nullptr);
}

void GamemenuMusicVolume(bool bActivate)
{
	if (bActivate) {
		if (gbMusicOn) {
			gbMusicOn = false;
			music_stop();
			sound_get_or_set_music_volume(VOLUME_MIN);
		} else {
			gbMusicOn = true;
			sound_get_or_set_music_volume(VOLUME_MAX);
			music_start(GetLevelMusic(leveltype));
		}
	} else {
		int volume = GamemenuSliderMusicSound(&sgOptionsMenu[0]);
		sound_get_or_set_music_volume(volume);
		if (volume == VOLUME_MIN) {
			if (gbMusicOn) {
				gbMusicOn = false;
				music_stop();
			}
		} else if (!gbMusicOn) {
			gbMusicOn = true;
			music_start(GetLevelMusic(leveltype));
		}
	}

	GamemenuGetMusic();
}

void GamemenuSoundVolume(bool bActivate)
{
	if (bActivate) {
		if (gbSoundOn) {
			gbSoundOn = false;
			sound_stop();
			sound_get_or_set_sound_volume(VOLUME_MIN);
		} else {
			gbSoundOn = true;
			sound_get_or_set_sound_volume(VOLUME_MAX);
		}
	} else {
		int volume = GamemenuSliderMusicSound(&sgOptionsMenu[1]);
		sound_get_or_set_sound_volume(volume);
		if (volume == VOLUME_MIN) {
			if (gbSoundOn) {
				gbSoundOn = false;
				sound_stop();
			}
		} else if (!gbSoundOn) {
			gbSoundOn = true;
		}
	}
	PlaySFX(IS_TITLEMOV);
	GamemenuGetSound();
}

void GamemenuGamma(bool bActivate)
{
	int gamma;
	if (bActivate) {
		gamma = UpdateGamma(0);
		if (gamma == 30)
			gamma = 100;
		else
			gamma = 30;
	} else {
		gamma = GamemenuSliderGamma();
	}

	UpdateGamma(gamma);
	GamemenuGetGamma();
}

void GamemenuSpeed(bool bActivate)
{
	if (bActivate) {
		if (sgGameInitInfo.nTickRate != 20)
			sgGameInitInfo.nTickRate = 20;
		else
			sgGameInitInfo.nTickRate = 50;
		gmenu_slider_set(&sgOptionsMenu[3], 20, 50, sgGameInitInfo.nTickRate);
	} else {
		sgGameInitInfo.nTickRate = gmenu_slider_get(&sgOptionsMenu[3], 20, 50);
	}

	sgOptions.Gameplay.tickRate.SetValue(sgGameInitInfo.nTickRate);
	gnTickDelay = 1000 / sgGameInitInfo.nTickRate;
}

} // namespace

void gamemenu_quit_game(bool bActivate)
{
	GamemenuNewGame(bActivate);
#ifndef NOEXIT
	gbRunGameResult = false;
#else
	ReturnToMainMenu = true;
#endif
}

void gamemenu_load_game(bool /*bActivate*/)
{
	EventHandler saveProc = SetEventHandler(DisableInputEventHandler);
	gamemenu_off();
	ClearFloatingNumbers();
	NewCursor(CURSOR_NONE);
	InitDiabloMsg(EMSG_LOADING);
	RedrawEverything();
	DrawAndBlit();
	LoadGame(false);
	ClrDiabloMsg();
	CornerStone.activated = false;
	PaletteFadeOut(8);
	MyPlayerIsDead = false;
	RedrawEverything();
	DrawAndBlit();
	LoadPWaterPalette();
	PaletteFadeIn(8);
	NewCursor(CURSOR_HAND);
	interface_msg_pump();
	SetEventHandler(saveProc);
}

void gamemenu_save_game(bool /*bActivate*/)
{
	if (pcurs != CURSOR_HAND) {
		return;
	}

	if (MyPlayer->_pmode == PM_DEATH || MyPlayerIsDead) {
		gamemenu_off();
		return;
	}

	EventHandler saveProc = SetEventHandler(DisableInputEventHandler);
	NewCursor(CURSOR_NONE);
	gamemenu_off();
	InitDiabloMsg(EMSG_SAVING);
	RedrawEverything();
	DrawAndBlit();
	SaveGame();
	oracool::LogEvent("Game saved");
	ClrDiabloMsg();
	InitDiabloMsg(EMSG_GAME_SAVED, 1000);
	RedrawEverything();
	NewCursor(CURSOR_HAND);
	if (CornerStone.activated) {
		CornerstoneSave();
		SaveOptions();
	}
	interface_msg_pump();
	SetEventHandler(saveProc);
}

void gamemenu_on()
{
	if (!gbIsMultiplayer) {
		// Oracool: user request - pick the death-only variant (with "Respawn In Town" in place
		// of "Save Game") once here, when the menu is actually opened, rather than every frame -
		// the game simulation (and therefore MyPlayerIsDead/_pmode) is frozen for as long as this
		// menu stays open, so there's no case where the right choice could change mid-display.
		const bool isDead = MyPlayerIsDead || MyPlayer->_pmode == PM_DEATH;
		gmenu_set_items(isDead ? sgSingleMenuOnDeath : sgSingleMenu, GamemenuUpdateSingle);
	} else {
		gmenu_set_items(sgMultiMenu, GamemenuUpdateMulti);
	}
	PressEscKey();
}

void gamemenu_off()
{
	gmenu_set_items(nullptr, nullptr);
}

void gamemenu_handle_previous()
{
	if (gmenu_is_active())
		gamemenu_off();
	else
		gamemenu_on();
}

} // namespace devilution
