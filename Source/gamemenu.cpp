/**
 * @file gamemenu.cpp
 *
 * Implementation of the in-game menu functions.
 */
#include "gamemenu.h"

#include <algorithm>

#include "cursor.h"
#include "engine/backbuffer_state.hpp"
#include "engine/events.hpp"
#include "engine/sound.h"
#include "engine/sound_defs.hpp"
#include "gmenu.h"
#include "init.h"
#include "options.h"
#include "oracool/auto_save.h"
#include "oracool/game_speed.h" // MinGameSpeed, MaxGameSpeed - the Speed slider's range
#include "oracool/levski_roar.h" // the monument's grid is handed back before the exit save
#include "oracool/workshop.h" // and the workshop's bench and craft grid
#include "player.h"
#include "utils/language.h"

namespace devilution {
namespace {

// Forward-declare menu handlers, used by the global menu structs below.
void GamemenuPrevious(bool bActivate);
void GamemenuReturnToGame(bool bActivate);
void GamemenuNewGame(bool bActivate);
void GamemenuReturnToMainMenu(bool bActivate);
void GamemenuRestartTown(bool bActivate);
void GamemenuRespawnInTown(bool bActivate);
void GamemenuOptions(bool bActivate);
void GamemenuMusicVolume(bool bActivate);
void GamemenuSoundVolume(bool bActivate);
void GamemenuGamma(bool bActivate);
void GamemenuSpeed(bool bActivate);

/**
 * @brief Oracool: user request - the pause menu is trimmed to exactly these three entries while
 * the player is alive. "Save Game"/"Load Game" were removed entirely as part of the move to
 * continuous autosave (see oracool/auto_save.h) - with every meaningful change persisted
 * instantly, a manual save/load concept no longer applies, matching Diablo 3's menus. "Exit Game"
 * reuses the vanilla "Quit Game" handler, which already does exactly what the new name says
 * (abandon the current game and close the application). "Main Menu" originally reused vanilla's
 * "New Game" handler too, but that only abandons the current game back to the hero/character-select
 * screen, not the actual title screen - a separate bug report ("MAIN MENU option ... to take me to
 * main game menu, not character selection menu") confirmed the mismatch. It now uses
 * GamemenuReturnToMainMenu instead.
 */
TMenuItem sgSingleMenu[] = {
	// clang-format off
	// dwFlags,      pszStr,                 fnMenu
	{ GMENU_ENABLED, N_("Return to Game"),  &GamemenuReturnToGame     },
	{ GMENU_ENABLED, N_("Main Menu"),       &GamemenuReturnToMainMenu },
	{ GMENU_ENABLED, N_("Options"),         &GamemenuOptions          },
	{ GMENU_ENABLED, N_("Exit Game"),       &gamemenu_quit_game       },
	{ GMENU_ENABLED, nullptr,               nullptr                   }
	// clang-format on
};
/**
 * @brief Oracool: user request - "Respawn In Town" should only ever be visible, not merely
 * enabled, while the character is dead - the underlying menu system draws every item between the
 * array's start and its nullptr terminator unconditionally (enabling/disabling an item only
 * changes its color, not whether it's shown), so a separate array is the only way to actually
 * remove it from the list the rest of the time. "Load Game" was removed entirely along with
 * sgSingleMenu's copy - see the comment above.
 */
TMenuItem sgSingleMenuOnDeath[] = {
	// clang-format off
	// dwFlags,      pszStr,                 fnMenu
	{ GMENU_ENABLED, N_("Respawn In Town"), &GamemenuRespawnInTown    },
	{ GMENU_ENABLED, N_("Return to Game"),  &GamemenuReturnToGame     },
	{ GMENU_ENABLED, N_("Main Menu"),       &GamemenuReturnToMainMenu },
	{ GMENU_ENABLED, N_("Options"),         &GamemenuOptions          },
	{ GMENU_ENABLED, N_("Exit Game"),       &gamemenu_quit_game       },
	{ GMENU_ENABLED, nullptr,               nullptr                   }
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

void GamemenuUpdateMulti()
{
	sgMultiMenu[2].setEnabled(MyPlayerIsDead);
}

void GamemenuPrevious(bool /*bActivate*/)
{
	gamemenu_on();
}

/**
 * @brief Oracool: user request - an explicit way out of the pause menu. Escape and the HUD's Menu
 * button both already dismiss it, but neither is visible from inside the menu, so the only listed
 * options all led away from the current game.
 */
void GamemenuReturnToGame(bool /*bActivate*/)
{
	gamemenu_off();
}

void GamemenuNewGame(bool /*bActivate*/)
{
	// Oracool: user request - persist the character on the way out, so the last thing you did
	// before quitting is never lost. Both "Main Menu" (GamemenuReturnToMainMenu) and "Exit Game"
	// (gamemenu_quit_game) funnel through here, so this one call covers both.
	//
	// It must come first: the loop below sets every player's _pmode to PM_QUIT and clears
	// MyPlayerIsDead, and saving after that would persist a quitting player rather than the one
	// who was just playing. SaveOnExit() is single-player-only; multiplayer keeps its existing
	// exit-path save in diablo.cpp.
	// BEFORE the save, and before the loop below: the monument's grid is not save state, so whatever
	// is sitting in it when the player leaves is gone. Closing here hands back everything the
	// backpack has room for, and THAT is then persisted by the save on the next line - so leaving
	// the game no longer costs the player items they had merely staged (audit, 2026-08-30).
	//
	// It may still refuse when the pack is full; FreeGame's ResetLevskiRoarForNewGame is the
	// backstop that stops the remainder leaking into the next character.
	oracool::CloseLevskiRoar();
	// The artisans' workshop is the same kind of container (external audit of v1.12.188, SAVE-01): the
	// bench and the craft grid are not save state, and nothing closed them on the way out.
	oracool::CloseWorkshop();

	oracool::SaveOnExit();

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

/**
 * @brief Oracool: user bug report - the ESC menu's "Main Menu" entry was landing back on the
 * hero/character-select screen instead of the actual title screen (Single Player/Multiplayer/
 * Options/Exit Diablo). That's because it reused GamemenuNewGame verbatim, which only abandons
 * the current game and lets StartGame's own loop (diablo.cpp) re-run hero selection - vanilla's
 * "New Game" always worked this way, since it exists to let you pick a different character, not
 * to leave the game entirely. Reaching the real title screen needs the separate ReturnToMainMenu
 * flag (diablo.h) - already used by the NOEXIT build's "Quit Game" for the same reason - which
 * StartGame checks right after its game loop returns, short-circuiting past its own hero-select
 * retry loop straight back to mainmenu_loop (menu.cpp).
 */
void GamemenuReturnToMainMenu(bool bActivate)
{
	GamemenuNewGame(bActivate);
	ReturnToMainMenu = true;
}

void GamemenuRestartTown(bool /*bActivate*/)
{
	NetSendCmd(true, CMD_RETOWN);
}

void GamemenuRespawnInTown(bool /*bActivate*/)
{
	// The death menu shows while the hero is still falling (PM_DEATH), and the open menu freezes the tick that would set
	// MyPlayerIsDead - so Respawn chosen there did nothing (round 6 audit, v1.12.231).
	if (!MyPlayerIsDead && MyPlayer->_pmode != PM_DEATH)
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
	// The whole range F9/F10 can set (audit, 2026-09-29): built for 20-50, a speed of 60 put the slider past its last
	// step, and Right then climbed on until the tick rate wrapped to 0 - a division by zero.
	gmenu_slider_steps(&sgOptionsMenu[3], oracool::MaxGameSpeed - oracool::MinGameSpeed + 1);
	gmenu_slider_set(&sgOptionsMenu[3], oracool::MinGameSpeed, oracool::MaxGameSpeed, sgGameInitInfo.nTickRate);
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
		gmenu_slider_set(&sgOptionsMenu[3], oracool::MinGameSpeed, oracool::MaxGameSpeed, sgGameInitInfo.nTickRate);
	} else {
		sgGameInitInfo.nTickRate = static_cast<uint8_t>(std::clamp(gmenu_slider_get(&sgOptionsMenu[3], oracool::MinGameSpeed, oracool::MaxGameSpeed),
		    oracool::MinGameSpeed, oracool::MaxGameSpeed));
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

void gamemenu_on()
{
	if (!gbIsMultiplayer) {
		// Oracool: user request - pick the death-only variant (with "Respawn In Town" in place
		// of "Main Menu") once here, when the menu is actually opened, rather than every frame -
		// the game simulation (and therefore MyPlayerIsDead/_pmode) is frozen for as long as this
		// menu stays open, so there's no case where the right choice could change mid-display.
		const bool isDead = MyPlayerIsDead || MyPlayer->_pmode == PM_DEATH;
		gmenu_set_items(isDead ? sgSingleMenuOnDeath : sgSingleMenu, nullptr);
	} else {
		gmenu_set_items(sgMultiMenu, GamemenuUpdateMulti);
	}
	// Every window, not the top one only: Escape closes one a press since 2026-09-30, and the menu (and death) wants them all.
	CloseWindowsForGameMenu();
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
