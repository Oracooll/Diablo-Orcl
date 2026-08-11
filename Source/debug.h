/**
 * @file debug.h
 *
 * Interface of debug functions.
 */
#pragma once

#include <cstdint>
#include <unordered_map>

#include "diablo.h"
#include "engine.h"
#include "engine/clx_sprite.hpp"
#include "utils/stdcompat/string_view.hpp"

namespace devilution {

extern std::string TestMapPath;
extern OptionalOwnedClxSpriteList pSquareCel;
extern bool DebugToggle;
extern bool DebugGodMode;
extern bool DebugVision;
extern bool DebugPath;
extern bool DebugGrid;
/**
 * @brief Oracool: user request - hides the always-on main HUD (bottom panel, health/mana orbs,
 * belt, control buttons, XP bar) for clean screenshots. Toggled by the "hideui" debug command;
 * checked in engine/render/scrollrt.cpp's DrawAndBlit(). Doesn't touch the dungeon view itself or
 * any side panel (inventory, character, etc.) - those already have their own close controls.
 */
extern bool DebugHideUi;
/**
 * @brief Oracool: user request - a more thorough version of DebugHideUi for a genuinely clean
 * screenshot: also hides the bottom HUD panel (like DebugHideUi), the mini-map, the Event Log
 * button/window, the Game Clock, the XP Counter and its gain indicator, the durability warning
 * icon, item name labels, monster health bars, floating damage numbers, monster wall outlines,
 * the FPS counter, and the mouse cursor. Toggled by the "clearui" debug command; checked in
 * engine/render/scrollrt.cpp's DrawView()/DrawAndBlit(). Deliberately independent of DebugHideUi
 * so the two can still be toggled separately. Doesn't force-close anything the player opened
 * themselves (inventory, character panel, quest log, spellbook, Stash, store dialog, the
 * full-screen map) - those already have their own close controls and aren't "ambient" HUD.
 */
extern bool DebugClearUi;
extern std::unordered_map<int, Point> DebugCoordsMap;
extern bool DebugScrollViewEnabled;
extern std::string debugTRN;
extern uint32_t glMid1Seed[NUMLEVELS];
extern uint32_t glMid2Seed[NUMLEVELS];
extern uint32_t glMid3Seed[NUMLEVELS];
extern uint32_t glEndSeed[NUMLEVELS];

void FreeDebugGFX();
void LoadDebugGFX();
void GetDebugMonster();
void NextDebugMonster();
void SetDebugLevelSeedInfos(uint32_t mid1Seed, uint32_t mid2Seed, uint32_t mid3Seed, uint32_t endSeed);
bool CheckDebugTextCommand(const string_view text);
bool IsDebugGridTextNeeded();
bool IsDebugGridInMegatiles();
bool GetDebugGridText(Point dungeonCoords, char *debugGridTextBuffer);
bool IsDebugAutomapHighlightNeeded();
bool ShouldHighlightDebugAutomapTile(Point position);

} // namespace devilution
