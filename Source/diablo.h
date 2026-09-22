/**
 * @file diablo.h
 *
 * Interface of the main game initialization functions.
 */
#pragma once

#include <cstdint>

#ifdef _DEBUG
#include "monstdat.h"
#include "spelldat.h" // SpellID/SpellType, for LastMouseButtonSpell below
#endif
#include "init.h"
#include "levels/gendung.h"
#include "utils/attributes.h"
#include "utils/endian_read.hpp"

namespace devilution {

/**
 * @brief Declared as well as included, because the include above is not always effective.
 *
 * There is a cycle: spelldat.h pulls in effects.h, which reaches player.h, which includes THIS
 * header - so a translation unit that enters spelldat.h first arrives here with spelldat.h still in
 * progress. Its include guard then hands line 12 an empty file, and the two externs below name types
 * that do not exist yet.
 *
 * Both are scoped enums with fixed underlying types, so declaring them is enough for an extern
 * declaration and costs nothing when the include did work. Breaking the cycle properly is a job of
 * its own; this is what it takes to build Release, which is where the cycle first bites.
 */
enum class SpellID : int16_t;
enum class SpellType : uint8_t;

constexpr uint32_t GameIdDiabloFull = LoadBE32("DRTL");
constexpr uint32_t GameIdDiabloSpawn = LoadBE32("DSHR");
constexpr uint32_t GameIdHellfireFull = LoadBE32("HRTL");
constexpr uint32_t GameIdHellfireSpawn = LoadBE32("HSHR");
#define GAME_ID (gbIsHellfire ? (gbIsSpawn ? GameIdHellfireSpawn : GameIdHellfireFull) : (gbIsSpawn ? GameIdDiabloSpawn : GameIdDiabloFull))

#define NUMLEVELS 25

enum clicktype : int8_t {
	CLICK_NONE,
	CLICK_LEFT,
	CLICK_RIGHT,
};

/**
 * @brief Specifices what game logic step is currently executed
 */
enum class GameLogicStep : uint8_t {
	None,
	ProcessPlayers,
	ProcessMonsters,
	ProcessObjects,
	ProcessMissiles,
	ProcessItems,
	ProcessTowners,
	ProcessItemsTown,
	ProcessMissilesTown,
};

enum class MouseActionType : uint8_t {
	None,
	Walk,
	Spell,
	SpellMonsterTarget,
	SpellPlayerTarget,
	Attack,
	AttackMonsterTarget,
	AttackPlayerTarget,
	OperateObject,
};

extern uint32_t glSeedTbl[NUMLEVELS];
extern DVL_API_FOR_TEST Point MousePosition;
extern DVL_API_FOR_TEST bool gbRunGame;
extern bool gbRunGameResult;
extern bool ReturnToMainMenu;
extern bool gbProcessPlayers;
extern DVL_API_FOR_TEST bool gbLoadGame;
extern bool cineflag;
/* These are defined in fonts.h */
extern void FontsCleanup();
extern DVL_API_FOR_TEST int PauseMode;
extern bool gbBard;
extern bool gbBarbarian;
/**
 * @brief Don't load UI or show Messageboxes or other user-interaction. Needed for UnitTests.
 */
extern DVL_API_FOR_TEST bool HeadlessMode;
extern clicktype sgbMouseDown;
extern uint16_t gnTickDelay;
extern char gszProductName[128];
extern char gszMainMenuVersionText[192];

extern MouseActionType LastMouseButtonAction;

/**
 * @brief The spell the last mouse action actually cast, for the hold-to-repeat path.
 *
 * Vanilla's RepeatMouseAction re-cast through CheckPlrSpell's DEFAULT arguments, which are
 * MyPlayer->_pRSpell / _pRSplType - correct in a game where only the right button can hold a spell,
 * and wrong the moment this fork let the LEFT button hold one too. Holding the left button after
 * casting a left-button skill repeated the RIGHT button's spell instead (user report, 2026-08-20:
 * "when i cast fist of the heavens game also casts teleport... is it casting rmb skill by itself?"
 * - it was).
 *
 * Recorded beside LastMouseButtonAction and by the same code, so the action and the spell it stands
 * for cannot describe different casts.
 */
extern SpellID LastMouseButtonSpell;
extern SpellType LastMouseButtonSpellType;

void InitKeymapActions();

/**
 * @brief Closes every window in the game - the space bar's master closer.
 *
 * Exported 2026-08-20 for the runeword book, which is 944 wide on a 960 screen and so owns the
 * screen while it is up. Deliberately THIS rather than ClosePanels(): it is documented as the list
 * every new window must be added to, so a caller cannot fall behind as windows are added.
 */
/**
 * @brief Shuts every shop surface - a store, the stash, an artisan's window, the Cube.
 *
 * Called by an OPENER before it opens, so only one of them is ever up (user, 2026-09-22). Each part
 * is a no-op when its own surface is shut, so a caller need not exempt itself; the two that can
 * refuse (a bench with items it cannot hand back) still refuse, by design.
 */
void CloseOtherShopSurfaces();

void CloseAllWindows();
void SetCursorPos(Point position);
void FreeGameMem();
bool StartGame(bool bNewGame, bool bSinglePlayer);
[[noreturn]] void diablo_quit(int exitStatus);
int DiabloMain(int argc, char **argv);
bool TryIconCurs();
void diablo_pause_game();
bool diablo_is_focused();
void diablo_focus_pause();
void diablo_focus_unpause();
bool PressEscKey();
void DisableInputEventHandler(const SDL_Event &event, uint16_t modState);
void LoadGameLevel(bool firstflag, lvl_entry lvldir);
bool IsDiabloAlive(bool playSFX);
void PrintScreen(SDL_Keycode vkey);

/**
 * @param bStartup Process additional ticks before returning
 */
bool game_loop(bool bStartup);
void diablo_color_cyc_logic();

/* rdata */

#ifdef _DEBUG
extern bool DebugDisableNetworkTimeout;
#endif

struct QuickMessage {
	/** Config variable names for quick message */
	const char *const key;
	/** Default quick message */
	const char *const message;
};

constexpr size_t QUICK_MESSAGE_OPTIONS = 4;
extern QuickMessage QuickMessages[QUICK_MESSAGE_OPTIONS];
/**
 * @brief Specifices what game logic step is currently executed
 */
extern GameLogicStep gGameLogicStep;

#ifdef __UWP__
void setOnInitialized(void (*)());
#endif

} // namespace devilution
