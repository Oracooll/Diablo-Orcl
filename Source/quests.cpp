/**
 * @file quests.cpp
 *
 * Implementation of functionality for handling quests.
 */
#include "quests.h"

#include <array>
#include <cstdint>

#include <fmt/format.h>

#include "DiabloUI/ui_flags.hpp"
#include "control.h"
#include "cursor.h"
#include "engine/load_file.hpp"
#include "engine/random.hpp"
#include "engine/render/clx_render.hpp"
#include "engine/render/primitive_render.hpp" // DrawHalfTransparentRectTo
#include "engine/render/text_render.hpp"
#include "engine/world_tile.hpp"
#include "init.h"
#include "levels/gendung.h"
#include "levels/town.h"
#include "levels/trigs.h"
#include "minitext.h"
#include "missiles.h"
#include "monster.h"
#include "options.h"
#include "oracool/event_log.h"
#include "oracool/hud_art.h"
#include "oracool/rift.h"
#include "oracool/stonegate.h"
#include "oracool/ornate_border.h"
#include "panels/ui_panels.hpp"
#include "stores.h"
#include "towners.h"
#include "utils/language.h"
#include "utils/utf8.hpp"

namespace devilution {

bool QuestLogIsOpen;
OptionalOwnedClxSpriteList pQLogCel;
/** Contains the quests of the current game. */
Quest Quests[MAXQUESTS];
Point ReturnLvlPosition;
dungeon_type ReturnLevelType;
int ReturnLevel;

namespace {
// Oracool: tracks each quest's _qlog state as of the last SyncQuestLogState()/CheckQuests() call,
// so a false->true transition can be detected and logged exactly once - see CheckQuests() and
// SyncQuestLogState() below.
std::array<bool, MAXQUESTS> PreviouslyLoggedQuestState {};
} // namespace

void SyncQuestLogState()
{
	for (int i = 0; i < MAXQUESTS; i++)
		PreviouslyLoggedQuestState[i] = Quests[i]._qlog;
}

/** Contains the data related to each quest_id. */
QuestData QuestsData[] = {
	// clang-format off
	// _qdlvl,  _qdmultlvl, _qlvlt,          bookOrder,   _qdrnd, _qslvl,          isSinglePlayerOnly, _qdmsg,        _qlstr
	{       5,          -1, DTYPE_NONE,          5,      100,    SL_NONE,         true,               TEXT_INFRA5,   N_( /* TRANSLATORS: Quest Name Block */ "The Magic Rock")           },
	{       9,          -1, DTYPE_NONE,         10,      100,    SL_NONE,         true,               TEXT_MUSH8,    N_("Black Mushroom")           },
	{       4,          -1, DTYPE_NONE,          3,      100,    SL_NONE,         true,               TEXT_GARBUD1,  N_("Gharbad The Weak")         },
	{       8,          -1, DTYPE_NONE,          9,      100,    SL_NONE,         true,               TEXT_ZHAR1,    N_("Zhar the Mad")             },
	{      14,          -1, DTYPE_NONE,         21,      100,    SL_NONE,         true,               TEXT_VEIL9,    N_("Lachdanan")                },
	{      15,          -1, DTYPE_NONE,         23,      100,    SL_NONE,         false,              TEXT_VILE3,    N_("Diablo")                   },
	{       2,           2, DTYPE_NONE,          0,      100,    SL_NONE,         false,              TEXT_BUTCH9,   N_("The Butcher")              },
	{       4,          -1, DTYPE_NONE,          4,      100,    SL_NONE,         true,               TEXT_BANNER2,  N_("Ogden's Sign")             },
	{       7,          -1, DTYPE_NONE,          8,      100,    SL_NONE,         true,               TEXT_BLINDING, N_("Halls of the Blind")       },
	{       5,          -1, DTYPE_NONE,          6,      100,    SL_NONE,         true,               TEXT_BLOODY,   N_("Valor")                    },
	{      10,          -1, DTYPE_NONE,         11,      100,    SL_NONE,         true,               TEXT_ANVIL5,   N_("Anvil of Fury")            },
	{      13,          -1, DTYPE_NONE,         20,      100,    SL_NONE,         true,               TEXT_BLOODWAR, N_("Warlord of Blood")         },
	{       3,           3, DTYPE_CATHEDRAL,     2,      100,    SL_SKELKING,     false,              TEXT_KING2,    N_("The Curse of King Leoric") },
	{       2,          -1, DTYPE_CAVES,         1,      100,    SL_POISONWATER,  true,               TEXT_POISON3,  N_("Poisoned Water Supply")    },
	{       6,          -1, DTYPE_CATACOMBS,     7,      100,    SL_BONECHAMB,    true,               TEXT_BONER,    N_("The Chamber of Bone")      },
	{      15,          15, DTYPE_CATHEDRAL,    22,      100,    SL_VILEBETRAYER, false,              TEXT_VILE1,    N_("Archbishop Lazarus")       },
	{      17,          17, DTYPE_NONE,         17,      100,    SL_NONE,         false,              TEXT_GRAVE7,   N_("Grave Matters")            },
	{      9,            9, DTYPE_NONE,         12,      100,    SL_NONE,         false,              TEXT_FARMER1,  N_("Farmer's Orchard")         },
	{      17,          -1, DTYPE_NONE,         14,      100,    SL_NONE,         true,               TEXT_GIRL2,    N_("Little Girl")              },
	{      19,          -1, DTYPE_NONE,         16,      100,    SL_NONE,         true,               TEXT_TRADER,   N_("Wandering Trader")         },
	{      17,          17, DTYPE_NONE,         15,      100,    SL_NONE,         false,              TEXT_DEFILER1, N_("The Defiler")              },
	{      21,          21, DTYPE_NONE,         19,      100,    SL_NONE,         false,              TEXT_NAKRUL1,  N_("Na-Krul")                  },
	{      21,          -1, DTYPE_NONE,         18,      100,    SL_NONE,         true,               TEXT_CORNSTN,  N_("Cornerstone of the World") },
	{       9,           9, DTYPE_NONE,         13,      100,    SL_NONE,         false,              TEXT_JERSEY4,  N_( /* TRANSLATORS: Quest Name Block end*/ "The Jersey's Jersey")      },
	// clang-format on
};

namespace {

int WaterDone;

/** Indices of quests to display in quest log window. `FirstFinishedQuest` are active quests the rest are completed */
quest_id EncounteredQuests[MAXQUESTS];
/** Overall number of EncounteredQuests entries */
int EncounteredQuestCount;
/** First (nonselectable) finished quest in list */
int FirstFinishedQuest;
/** Currently selected quest list item */
int SelectedQuest;

// Oracool V1: the quest log wears the same treatment as the waypoint list and the event log - a
// half-transparent fill under the ornate textbox_frame00 bevel, with an outlined FontSize30 title
// over a separator rule. Geometry mirrors oracool/waypoint_menu.cpp deliberately, so the two
// windows are interchangeable at a glance:
//
//   0..24     top margin
//   24..74    label band, "QUESTS"
//   74..77    separator rule
//   77..101   gap below the rule
//   101..696  entry list
//   696..720  bottom margin
//
// Top margin, gap under the rule and bottom margin are all one QuestPanelMargin.
//
// The window owns its own rect rather than GetLeftPanel()'s. That rect is 320x352 and shared with
// the character sheet; a 720-tall log would drag it along. Flush top-left, like the waypoint list.
constexpr Size QuestPanelSize { 340, 720 };
constexpr int QuestPanelMargin = 24;
constexpr int QuestLabelHeight = 50;
constexpr int QuestSeparatorGap = QuestPanelMargin;
constexpr int QuestListTop = QuestPanelMargin + QuestLabelHeight + oracool::OrnateBorderWidth + QuestSeparatorGap;

/**
 * @brief The entry list's area, in SCREEN coordinates - the panel is flush at the origin.
 *
 * This one rect drives all three of: the spacing StartQuestlog computes to fill it, where
 * DrawQuestLog puts the rows, and what QuestLogMouseToEntry hit-tests. They were already tied
 * together through the old 280x300 inner rect; re-basing it is what moves the whole log.
 */
// Ends at the health orb's top edge, not at the panel's own height minus a margin. The orb is
// anchored to the screen's bottom-left corner and draws OVER this panel, so the last quest used to
// render underneath it (user, 2026-08-16: "move it up so the last Diablo quest lands above the
// line"). StartQuestlog derives its line spacing from this rect's height and centres the list in
// it, so shortening the rect is the whole fix - every entry tightens up and rises together.
constexpr Rectangle InnerPanel { { QuestPanelMargin, QuestListTop },
	{ QuestPanelSize.width - 2 * QuestPanelMargin, oracool::SidePanelContentBottom - QuestListTop } };
static_assert(InnerPanel.position.y + InnerPanel.size.height == oracool::SidePanelContentBottom,
    "Quest log list no longer ends at the orb clearance line");
static_assert(InnerPanel.size.height > 0, "Quest log list has no room left");

/**
 * @brief The panel's top-left on screen. Vertically centred (user, 2026-08-30).
 *
 * The window used to be flush at the origin, which on a 720-tall screen is also centred and so cost
 * nothing - but on anything taller it hung from the top edge with all the slack below it. Centring
 * is one expression, and every other rect in this file is expressed relative to the panel, so they
 * all follow from QuestListRect below rather than each carrying its own offset.
 *
 * x stays 0: this is a left-docked window and only the vertical was asked about.
 */
Point QuestPanelOrigin()
{
	return { 0, std::max(0, (static_cast<int>(gnScreenHeight) - QuestPanelSize.height) / 2) };
}

/** @brief InnerPanel moved to where the panel actually is. The rows and the hit-test share it. */
Rectangle QuestListRect()
{
	return { InnerPanel.position + Displacement { QuestPanelOrigin().x, QuestPanelOrigin().y }, InnerPanel.size };
}

constexpr int LineHeight = 12;
constexpr int MaxSpacing = LineHeight * 2;
int ListYOffset;
int LineSpacing;
/** The number of pixels to move finished quest, to seperate them from the active ones */
int FinishedQuestOffset;

const char *const QuestTriggerNames[5] = {
	N_(/* TRANSLATORS: Quest Map*/ "King Leoric's Tomb"),
	N_(/* TRANSLATORS: Quest Map*/ "The Chamber of Bone"),
	N_(/* TRANSLATORS: Quest Map*/ "Maze"),
	N_(/* TRANSLATORS: Quest Map*/ "A Dark Passage"),
	N_(/* TRANSLATORS: Quest Map*/ "Unholy Altar")
};
/**
 * A quest group containing the three quests the Butcher,
 * Ogden's Sign and Gharbad the Weak, which ensures that exactly
 * two of these three quests appear in any single player game.
 */
int QuestGroup1[3] = { Q_BUTCHER, Q_LTBANNER, Q_GARBUD };
/**
 * A quest group containing the three quests Halls of the Blind,
 * the Magic Rock and Valor, which ensures that exactly two of
 * these three quests appear in any single player game.
 */
int QuestGroup2[3] = { Q_BLIND, Q_ROCK, Q_BLOOD };
/**
 * A quest group containing the three quests Black Mushroom,
 * Zhar the Mad and Anvil of Fury, which ensures that exactly
 * two of these three quests appear in any single player game.
 */
int QuestGroup3[3] = { Q_MUSHROOM, Q_ZHAR, Q_ANVIL };
/**
 * A quest group containing the two quests Lachdanan and Warlord
 * of Blood, which ensures that exactly one of these two quests
 * appears in any single player game.
 */
int QuestGroup4[2] = { Q_VEIL, Q_WARLORD };

/**
 * @brief There is no reason to run this, the room has already had a proper sector assigned
 */
void DrawButcher()
{
	Point position = SetPiece.position.megaToWorld() + Displacement { 3, 3 };
	DRLG_RectTrans({ position, { 7, 7 } });
}

void DrawSkelKing(quest_id q, Point position)
{
	Quests[q].position = position.megaToWorld() + Displacement { 12, 7 };
}

void DrawWarLord(Point position)
{
	auto dunData = LoadFileInMem<uint16_t>("levels\\l4data\\warlord2.dun");

	SetPiece = { position, WorldTileSize(SDL_SwapLE16(dunData[0]), SDL_SwapLE16(dunData[1])) };

	PlaceDunTiles(dunData.get(), position, 6);
}

void DrawSChamber(quest_id q, Point position)
{
	auto dunData = LoadFileInMem<uint16_t>("levels\\l2data\\bonestr1.dun");

	SetPiece = { position, WorldTileSize(SDL_SwapLE16(dunData[0]), SDL_SwapLE16(dunData[1])) };

	PlaceDunTiles(dunData.get(), position, 3);

	Quests[q].position = position.megaToWorld() + Displacement { 6, 7 };
}

void DrawLTBanner(Point position)
{
	auto dunData = LoadFileInMem<uint16_t>("levels\\l1data\\banner1.dun");

	int width = SDL_SwapLE16(dunData[0]);
	int height = SDL_SwapLE16(dunData[1]);

	SetPiece = { position, WorldTileSize(SDL_SwapLE16(dunData[0]), SDL_SwapLE16(dunData[1])) };

	const uint16_t *tileLayer = &dunData[2];

	for (int j = 0; j < height; j++) {
		for (int i = 0; i < width; i++) {
			auto tileId = static_cast<uint8_t>(SDL_SwapLE16(tileLayer[j * width + i]));
			if (tileId != 0) {
				pdungeon[position.x + i][position.y + j] = tileId;
			}
		}
	}
}

/**
 * Close outer wall
 */
void DrawBlind(Point position)
{
	dungeon[position.x][position.y + 1] = 154;
	dungeon[position.x + 10][position.y + 8] = 154;
}

void DrawBlood(Point position)
{
	auto dunData = LoadFileInMem<uint16_t>("levels\\l2data\\blood2.dun");

	SetPiece = { position, WorldTileSize(SDL_SwapLE16(dunData[0]), SDL_SwapLE16(dunData[1])) };

	PlaceDunTiles(dunData.get(), position, 0);
}

int QuestLogMouseToEntry()
{
	// InnerPanel is already in screen coordinates - the window is flush at the origin, so there is
	// no GetLeftPanel() offset to add any more.
	if (!QuestListRect().contains(MousePosition) || (EncounteredQuestCount == 0))
		return -1;
	int y = MousePosition.y - QuestListRect().position.y;
	for (int i = 0; i < FirstFinishedQuest; i++) {
		if ((y >= ListYOffset + i * LineSpacing)
		    && (y < ListYOffset + i * LineSpacing + LineHeight)) {
			return i;
		}
	}
	return -1;
}

void PrintQLString(const Surface &out, int x, int y, string_view str, bool marked, bool disabled = false)
{
	// Screen coordinates throughout now: the window is flush at the origin, so GetPanelPosition's
	// UiPanels::Quest offset no longer applies. Centred within the list's own width rather than the
	// old parchment's fixed 257.
	const int listWidth = InnerPanel.size.width;
	int width = GetLineWidth(str);
	x += std::max((listWidth - width) / 2, 0);
	if (marked) {
		ClxDraw(out, Point { x - 20, y + 13 }, (*pSPentSpn2Cels)[PentSpn2Spin()]);
	}
	DrawString(out, str, { Point { x, y }, { listWidth, 0 } }, { (disabled ? UiFlags::ColorWhitegold : UiFlags::ColorWhite) | UiFlags::Shadowed });
	if (marked) {
		ClxDraw(out, Point { x + width + 7, y + 13 }, (*pSPentSpn2Cels)[PentSpn2Spin()]);
	}
}

void StartPWaterPurify()
{
	PlaySfxLoc(IS_QUESTDN, MyPlayer->position.tile);
	LoadPalette("levels\\l3data\\l3pwater.pal", false);
	UpdatePWaterPalette();
	WaterDone = 32;
}

} // namespace

void InitQuests()
{
	QuestDialogTable[TOWN_HEALER][Q_MUSHROOM] = TEXT_NONE;
	QuestDialogTable[TOWN_WITCH][Q_MUSHROOM] = TEXT_MUSH9;

	QuestLogIsOpen = false;
	WaterDone = 0;

	int q = 0;
	for (auto &quest : Quests) {
		quest._qidx = static_cast<quest_id>(q);
		auto &questData = QuestsData[q];
		q++;

		quest._qactive = QUEST_NOTAVAIL;
		quest.position = { 0, 0 };
		quest._qlvltype = questData._qlvlt;
		quest._qslvl = questData._qslvl;
		quest._qvar1 = 0;
		quest._qvar2 = 0;
		quest._qlog = false;
		quest._qmsg = questData._qdmsg;

		if (!UseMultiplayerQuests()) {
			quest._qlevel = questData._qdlvl;
			quest._qactive = QUEST_INIT;
		} else if (!questData.isSinglePlayerOnly) {
			quest._qlevel = questData._qdmultlvl;
			quest._qactive = QUEST_INIT;
		}
	}

	// Oracool: user request - Farmer's Orchard and The Jersey's Jersey are chosen here, from a
	// three-way setting: Random, Always Farmer, Always Cow.
	//
	// They are the one pair in the game that CANNOT both be on: the cow quest replaces Lester the
	// farmer with the Complete Nut, which is why Farmer's own gate reads `bCowQuest == 0`
	// (towners.cpp). Every other quest is on for good, so this is the only choice left in the set.
	//
	// Random is seeded from glSeedTbl[15], the level-16 seed, for the same reason
	// InitialiseQuestPools uses it: it is fixed for a given game and it is SAVED. That matters more
	// here than it looks. Neither sgGameInitInfo.bCowQuest nor the quests themselves are persisted -
	// InitQuests re-runs on load and rebuilds both from scratch (see loadsave.cpp) - so anything not
	// derived from the save's own seed would re-roll on every load, and the farmer standing in town
	// could change identity underneath a quest already in progress.
	//
	// Before the pool block below, deliberately: SetRndSeed there restarts the same stream from the
	// same seed, so this draw cannot shift which quests that would cull if it is ever switched on.
	//
	// Single-player only. In multiplayer sgGameInitInfo comes off the wire from the host and every
	// client must agree with it, so resolving locally there would desync the town.
	if (!gbIsMultiplayer && gbIsHellfire) {
		switch (*sgOptions.Gameplay.farmerQuest) {
		case FarmerQuestMode::AlwaysFarmer:
			sgGameInitInfo.bCowQuest = 0;
			break;
		case FarmerQuestMode::AlwaysCow:
			sgGameInitInfo.bCowQuest = 1;
			break;
		case FarmerQuestMode::Random:
			SetRndSeed(glSeedTbl[15]);
			sgGameInitInfo.bCowQuest = static_cast<uint8_t>(GenerateRnd(2) != 0 ? 1 : 0);
			break;
		}
	}

	if (!UseMultiplayerQuests() && *sgOptions.Gameplay.randomizeQuests) {
		// Quests are set from the seed used to generate level 16.
		InitialiseQuestPools(glSeedTbl[15], Quests);
	}

	if (gbIsSpawn) {
		for (auto &quest : Quests) {
			quest._qactive = QUEST_NOTAVAIL;
		}
	}

	if (Quests[Q_SKELKING]._qactive == QUEST_NOTAVAIL)
		Quests[Q_SKELKING]._qvar2 = 2;
	if (Quests[Q_ROCK]._qactive == QUEST_NOTAVAIL)
		Quests[Q_ROCK]._qvar2 = 2;
	Quests[Q_LTBANNER]._qvar1 = 1;
	if (UseMultiplayerQuests())
		Quests[Q_BETRAYER]._qvar1 = 2;
	// In multiplayer items spawn during level generation to avoid desyncs
	if (gbIsMultiplayer && Quests[Q_MUSHROOM]._qactive == QUEST_INIT)
		Quests[Q_MUSHROOM]._qvar1 = QS_TOMESPAWNED;

	// Oracool: every quest's _qlog is false at this point (set above), so this just seeds a clean
	// baseline for the new-quest-added log detector in CheckQuests().
	SyncQuestLogState();
}

void InitialiseQuestPools(uint32_t seed, Quest quests[])
{
	SetRndSeed(seed);
	quests[PickRandomlyAmong({ Q_SKELKING, Q_PWATER })]._qactive = QUEST_NOTAVAIL;

	// using int and not size_t here to detect negative values from GenerateRnd
	int randomIndex = GenerateRnd(sizeof(QuestGroup1) / sizeof(*QuestGroup1));

	if (randomIndex >= 0)
		quests[QuestGroup1[randomIndex]]._qactive = QUEST_NOTAVAIL;

	randomIndex = GenerateRnd(sizeof(QuestGroup2) / sizeof(*QuestGroup2));
	if (randomIndex >= 0)
		quests[QuestGroup2[randomIndex]]._qactive = QUEST_NOTAVAIL;

	randomIndex = GenerateRnd(sizeof(QuestGroup3) / sizeof(*QuestGroup3));
	if (randomIndex >= 0)
		quests[QuestGroup3[randomIndex]]._qactive = QUEST_NOTAVAIL;

	randomIndex = GenerateRnd(sizeof(QuestGroup4) / sizeof(*QuestGroup4));

	// always true, QuestGroup4 has two members
	if (randomIndex >= 0)
		quests[QuestGroup4[randomIndex]]._qactive = QUEST_NOTAVAIL;
}

void CheckQuests()
{
	if (gbIsSpawn)
		return;

	// Oracool: user request - "when i interact with quest item which introduces a certain quest in
	// my quest log put a log message." Rather than instrument the ~25 scattered call sites that
	// set _qlog = true directly (one per quest, spread across quests.cpp/towners.cpp/objects.cpp/
	// monster.cpp/player.cpp), this generically detects the false->true transition every tick -
	// CheckQuests() already runs every game tick regardless of what triggered the change, so no
	// existing quest-activation code needed to change at all. PreviouslyLoggedQuestState is kept
	// in sync with reality (without logging) by SyncQuestLogState(), called once whenever a game
	// actually starts or a save actually loads - see that function's own callers for why.
	for (int i = 0; i < MAXQUESTS; i++) {
		if (Quests[i]._qlog && !PreviouslyLoggedQuestState[i]) {
			oracool::LogEvent(fmt::format("Quest \"{:s}\" was added to the quest log", _(QuestsData[Quests[i]._qidx]._qlstr)));
			PreviouslyLoggedQuestState[i] = true;
		}
	}

	auto &quest = Quests[Q_BETRAYER];
	if (quest.IsAvailable() && UseMultiplayerQuests() && quest._qvar1 == 2) {
		AddObject(OBJ_ALTBOY, SetPiece.position.megaToWorld() + Displacement { 4, 6 });
		quest._qvar1 = 3;
		NetSendCmdQuest(true, quest);
	}

	if (UseMultiplayerQuests()) {
		return;
	}

	if (currlevel == quest._qlevel
	    && !setlevel
	    && quest._qvar1 >= 2
	    && (quest._qactive == QUEST_ACTIVE || quest._qactive == QUEST_DONE)
	    && (quest._qvar2 == 0 || quest._qvar2 == 2)) {
		// Spawn a portal at the quest trigger location
		AddMissile(quest.position, quest.position, Direction::South, MissileID::RedPortal, TARGET_MONSTERS, MyPlayerId, 0, 0);
		quest._qvar2 = 1;
		if (quest._qactive == QUEST_ACTIVE && quest._qvar1 == 2) {
			quest._qvar1 = 3;
		}
	}

	if (quest._qactive == QUEST_DONE
	    && setlevel
	    && setlvlnum == SL_VILEBETRAYER
	    && quest._qvar2 == 4) {
		Point portalLocation { 35, 32 };
		AddMissile(portalLocation, portalLocation, Direction::South, MissileID::RedPortal, TARGET_MONSTERS, MyPlayerId, 0, 0);
		quest._qvar2 = 3;
	}

	if (setlevel) {
		// Hostiles left, not slots in use (audit, 2026-09-27): the four golem slots are always taken, and the hero's own
		// minions and companions take more - a Necromancer with his army out cleared the level and the water never cleared.
		const auto hostilesLeft = [] {
			for (size_t i = 0; i < ActiveMonsterCount; i++) {
				const Monster &monster = Monsters[ActiveMonsters[i]];
				if (ActiveMonsters[i] >= MAX_PLRS && !monster.isPlayerMinion() && (monster.hitPoints >> 6) > 0)
					return true;
			}
			return false;
		};
		Quest &poisonWater = Quests[Q_PWATER];
		if (setlvlnum == poisonWater._qslvl
		    && poisonWater._qactive != QUEST_INIT
		    && leveltype == poisonWater._qlvltype
		    && !hostilesLeft()
		    && poisonWater._qactive != QUEST_DONE) {
			poisonWater._qactive = QUEST_DONE;
			poisonWater._qlog = true; // even if the player skips talking to Pepin completely they should at least notice the water being purified once they cleanse the level
			NetSendCmdQuest(true, poisonWater);
			StartPWaterPurify();
		}
	} else if (MyPlayer->_pmode == PM_STAND) {
		for (auto &quest : Quests) {
			if (currlevel == quest._qlevel
			    && quest._qslvl != 0
			    && quest._qactive != QUEST_NOTAVAIL
			    && MyPlayer->position.tile == quest.position
			    && (quest._qidx != Q_BETRAYER || quest._qvar1 >= 3)) {
				if (quest._qlvltype != DTYPE_NONE) {
					setlvltype = quest._qlvltype;
				}
				StartNewLvl(*MyPlayer, WM_DIABSETLVL, quest._qslvl);
			}
		}
	}
}

bool ForceQuests()
{
	if (gbIsSpawn)
		return false;

	if (UseMultiplayerQuests()) {
		return false;
	}

	for (auto &quest : Quests) {
		if (quest._qidx != Q_BETRAYER && currlevel == quest._qlevel && quest._qslvl != 0) {
			int ql = quest._qslvl - 1;

			if (EntranceBoundaryContains(quest.position, cursPosition)) {
				InfoString = fmt::format(fmt::runtime(_(/* TRANSLATORS: Used for Quest Portals. {:s} is a Map Name */ "To {:s}")), _(QuestTriggerNames[ql]));
				cursPosition = quest.position;
				return true;
			}
		}
	}

	return false;
}

void CheckQuestKill(const Monster &monster, bool sendmsg)
{
	if (gbIsSpawn)
		return;
	// A rift's guardian is the Skeleton King, the Butcher, Diablo or Na-Krul in body only: killing
	// him there settles no quest (oracool/rift.h).
	if (oracool::InRift())
		return;

	Player &myPlayer = *MyPlayer;

	if (monster.type().type == MT_SKING) {
		auto &quest = Quests[Q_SKELKING];
		quest._qactive = QUEST_DONE;
		myPlayer.Say(HeroSpeech::RestWellLeoricIllFindYourSon, 30);
		if (sendmsg)
			NetSendCmdQuest(true, quest);

	} else if (monster.type().type == MT_CLEAVER) {
		auto &quest = Quests[Q_BUTCHER];
		quest._qactive = QUEST_DONE;
		myPlayer.Say(HeroSpeech::TheSpiritsOfTheDeadAreNowAvenged, 30);
		if (sendmsg)
			NetSendCmdQuest(true, quest);
	} else if (monster.uniqueType == UniqueMonsterType::Garbud) { //"Gharbad the Weak"
		Quests[Q_GARBUD]._qactive = QUEST_DONE;
		NetSendCmdQuest(true, Quests[Q_GARBUD]);
		myPlayer.Say(HeroSpeech::ImNotImpressed, 30);
	} else if (monster.uniqueType == UniqueMonsterType::Zhar) { //"Zhar the Mad"
		Quests[Q_ZHAR]._qactive = QUEST_DONE;
		NetSendCmdQuest(true, Quests[Q_ZHAR]);
		myPlayer.Say(HeroSpeech::ImSorryDidIBreakYourConcentration, 30);
	} else if (monster.uniqueType == UniqueMonsterType::Lazarus) { //"Arch-Bishop Lazarus"
		auto &betrayerQuest = Quests[Q_BETRAYER];
		betrayerQuest._qactive = QUEST_DONE;
		myPlayer.Say(HeroSpeech::YourMadnessEndsHereBetrayer, 30);
		betrayerQuest._qvar1 = 7;
		auto &diabloQuest = Quests[Q_DIABLO];
		diabloQuest._qactive = QUEST_ACTIVE;

		if (UseMultiplayerQuests()) {
			for (WorldTileCoord j = 0; j < MAXDUNY; j++) {
				for (WorldTileCoord i = 0; i < MAXDUNX; i++) {
					if (dPiece[i][j] == 369) {
						trigs[numtrigs].position = { i, j };
						trigs[numtrigs]._tmsg = WM_DIABNEXTLVL;
						numtrigs++;
					}
				}
			}
		} else {
			InitVPTriggers();
			betrayerQuest._qvar2 = 4;
			AddMissile({ 35, 32 }, { 35, 32 }, Direction::South, MissileID::RedPortal, TARGET_MONSTERS, MyPlayerId, 0, 0);
		}
		if (sendmsg) {
			NetSendCmdQuest(true, betrayerQuest);
			NetSendCmdQuest(true, diabloQuest);
		}
	} else if (monster.uniqueType == UniqueMonsterType::WarlordOfBlood) {
		Quests[Q_WARLORD]._qactive = QUEST_DONE;
		NetSendCmdQuest(true, Quests[Q_WARLORD]);
		myPlayer.Say(HeroSpeech::YourReignOfPainHasEnded, 30);
	}
}

void DRLG_CheckQuests(Point position)
{
	for (auto &quest : Quests) {
		if (quest.IsAvailable()) {
			switch (quest._qidx) {
			case Q_BUTCHER:
				DrawButcher();
				break;
			case Q_LTBANNER:
				DrawLTBanner(position);
				break;
			case Q_BLIND:
				DrawBlind(position);
				break;
			case Q_BLOOD:
				DrawBlood(position);
				break;
			case Q_WARLORD:
				DrawWarLord(position);
				break;
			case Q_SKELKING:
				DrawSkelKing(quest._qidx, position);
				break;
			case Q_SCHAMB:
				DrawSChamber(quest._qidx, position);
				break;
			default:
				break;
			}
		}
	}
}

int GetMapReturnLevel()
{
	switch (setlvlnum) {
	case SL_SKELKING:
		return Quests[Q_SKELKING]._qlevel;
	case SL_BONECHAMB:
		return Quests[Q_SCHAMB]._qlevel;
	case SL_POISONWATER:
		return Quests[Q_PWATER]._qlevel;
	case SL_VILEBETRAYER:
		return Quests[Q_BETRAYER]._qlevel;
	default:
		return 0;
	}
}

Point GetMapReturnPosition()
{
	// A RIFT COMES BACK TO THE MONUMENT (user, 2026-09-22: "when i take the exit portal i spawn next
	// to farhnam. spawn me next to the nephalem/guardian rift monument").
	//
	// It came back to Farnham because a rift is a set level whose number is none of the four below,
	// so it fell to the default - and the default is the drunk, which is where the Poisoned Water
	// and the rest of vanilla's set pieces put you. Nothing was wrong with that until a set level
	// existed that the player walks into from somewhere else entirely.
	//
	// Asked of the rift rather than added as another `case`, because the rift's level NUMBER is
	// chosen per kind (RiftLevelFor) and a case list would have to be kept in step with it.
	//
	// Asked of the LEVEL NUMBER since 2026-09-24. It asked InRift() and the gate object, and both are
	// wrong by the time this runs: the WM_DIABRTNLVL handler clears `setlevel` before LoadGameLevel
	// asks, so InRift() is false, and the gate object belongs to a town that is not built yet.
	// setlvlnum still names the level being left, and the monument's tile is remembered from the last
	// town (RiftReturnTile).
	if (oracool::IsRiftLevel(setlvlnum))
		return oracool::RiftReturnTile();
	// A Sealed Map's arena returns to the middle of town, the new game's spawn, not beside Farnham in the far corner -
	// the complaint the rifts were fixed for (round 10 audit, v1.12.235).
	if (IsArenaLevel(setlvlnum))
		return { 57, 67 };

	switch (setlvlnum) {
	case SL_SKELKING:
		return Quests[Q_SKELKING].position + Direction::SouthEast;
	case SL_BONECHAMB:
		return Quests[Q_SCHAMB].position + Direction::SouthEast;
	case SL_POISONWATER:
		return Quests[Q_PWATER].position + Direction::SouthWest;
	case SL_VILEBETRAYER:
		return Quests[Q_BETRAYER].position + Direction::South;
	default:
		return GetTowner(TOWN_DRUNK)->position + Direction::SouthEast;
	}
}

void LoadPWaterPalette()
{
	if (!setlevel || setlvlnum != Quests[Q_PWATER]._qslvl || Quests[Q_PWATER]._qactive == QUEST_INIT || leveltype != Quests[Q_PWATER]._qlvltype)
		return;

	if (Quests[Q_PWATER]._qactive == QUEST_DONE)
		LoadPalette("levels\\l3data\\l3pwater.pal");
	else
		LoadPalette("levels\\l3data\\l3pfoul.pal");
}

void UpdatePWaterPalette()
{
	if (WaterDone > 0) {
		palette_update_quest_palette(WaterDone);
		WaterDone--;
		return;
	}
	palette_update_caves();
}

void ResyncMPQuests()
{
	if (gbIsSpawn)
		return;

	auto &kingQuest = Quests[Q_SKELKING];
	if (kingQuest._qactive == QUEST_INIT
	    && currlevel >= kingQuest._qlevel - 1
	    && currlevel <= kingQuest._qlevel + 1) {
		kingQuest._qactive = QUEST_ACTIVE;
		NetSendCmdQuest(true, kingQuest);
	}

	auto &butcherQuest = Quests[Q_BUTCHER];
	if (butcherQuest._qactive == QUEST_INIT
	    && currlevel >= butcherQuest._qlevel - 1
	    && currlevel <= butcherQuest._qlevel + 1) {
		butcherQuest._qactive = QUEST_ACTIVE;
		NetSendCmdQuest(true, butcherQuest);
	}

	auto &betrayerQuest = Quests[Q_BETRAYER];
	if (betrayerQuest._qactive == QUEST_INIT && currlevel == betrayerQuest._qlevel - 1) {
		betrayerQuest._qactive = QUEST_ACTIVE;
		NetSendCmdQuest(true, betrayerQuest);
	}
	if (betrayerQuest.IsAvailable())
		AddObject(OBJ_ALTBOY, SetPiece.position.megaToWorld() + Displacement { 4, 6 });

	auto &cryptQuest = Quests[Q_GRAVE];
	if (cryptQuest._qactive == QUEST_INIT && currlevel == cryptQuest._qlevel - 1) {
		cryptQuest._qactive = QUEST_ACTIVE;
		NetSendCmdQuest(true, cryptQuest);
	}

	auto &defilerQuest = Quests[Q_DEFILER];
	if (defilerQuest._qactive == QUEST_INIT && currlevel == defilerQuest._qlevel - 1) {
		defilerQuest._qactive = QUEST_ACTIVE;
		NetSendCmdQuest(true, defilerQuest);
	}

	auto &nakrulQuest = Quests[Q_NAKRUL];
	if (nakrulQuest._qactive == QUEST_INIT && currlevel == nakrulQuest._qlevel - 1) {
		nakrulQuest._qactive = QUEST_ACTIVE;
		NetSendCmdQuest(true, nakrulQuest);
	}
}

void ResyncQuests()
{
	if (gbIsSpawn)
		return;

	LoadingMapObjects = true;

	if (Quests[Q_LTBANNER].IsAvailable()) {
		Monster *snotSpill = FindUniqueMonster(UniqueMonsterType::SnotSpill);
		if (Quests[Q_LTBANNER]._qvar1 == 1) {
			ObjChangeMapResync(
			    SetPiece.position.x + SetPiece.size.width - 2,
			    SetPiece.position.y + SetPiece.size.height - 2,
			    SetPiece.position.x + SetPiece.size.width + 1,
			    SetPiece.position.y + SetPiece.size.height + 1);
		}
		if (Quests[Q_LTBANNER]._qvar1 == 2) {
			ObjChangeMapResync(
			    SetPiece.position.x + SetPiece.size.width - 2,
			    SetPiece.position.y + SetPiece.size.height - 2,
			    SetPiece.position.x + SetPiece.size.width + 1,
			    SetPiece.position.y + SetPiece.size.height + 1);
			ObjChangeMapResync(SetPiece.position.x, SetPiece.position.y, SetPiece.position.x + (SetPiece.size.width / 2) + 2, SetPiece.position.y + (SetPiece.size.height / 2) - 2);
			for (int i = 0; i < ActiveObjectCount; i++)
				SyncObjectAnim(Objects[ActiveObjects[i]]);
			auto tren = TransVal;
			TransVal = 9;
			DRLG_MRectTrans({ SetPiece.position, WorldTileSize(SetPiece.size.width / 2 + 4, SetPiece.size.height / 2) });
			TransVal = tren;
			if (gbIsMultiplayer && snotSpill != nullptr && snotSpill->talkMsg != TEXT_BANNER12) {
				snotSpill->goal = MonsterGoal::Inquiring;
				snotSpill->talkMsg = Quests[Q_LTBANNER]._qactive == QUEST_DONE ? TEXT_BANNER12 : TEXT_BANNER11;
				snotSpill->flags |= MFLAG_QUEST_COMPLETE;
			}
		}
		if (Quests[Q_LTBANNER]._qvar1 == 3) {
			ObjChangeMapResync(SetPiece.position.x, SetPiece.position.y, SetPiece.position.x + SetPiece.size.width + 1, SetPiece.position.y + SetPiece.size.height + 1);
			for (int i = 0; i < ActiveObjectCount; i++)
				SyncObjectAnim(Objects[ActiveObjects[i]]);
			auto tren = TransVal;
			TransVal = 9;
			DRLG_MRectTrans({ SetPiece.position, WorldTileSize(SetPiece.size.width / 2 + 4, SetPiece.size.height / 2) });
			TransVal = tren;
			if (gbIsMultiplayer && snotSpill != nullptr) {
				snotSpill->goal = MonsterGoal::Normal;
				snotSpill->flags |= MFLAG_QUEST_COMPLETE;
				snotSpill->talkMsg = TEXT_NONE;
				snotSpill->activeForTicks = UINT8_MAX;
				RedoPlayerVision();
			}
		}
	}
	if (currlevel == Quests[Q_MUSHROOM]._qlevel && !setlevel) {
		if (Quests[Q_MUSHROOM]._qactive == QUEST_INIT && Quests[Q_MUSHROOM]._qvar1 == QS_INIT) {
			SpawnQuestItem(IDI_FUNGALTM, { 0, 0 }, 5, 1, true);
			Quests[Q_MUSHROOM]._qvar1 = QS_TOMESPAWNED;
			NetSendCmdQuest(true, Quests[Q_MUSHROOM]);
		} else {
			if (Quests[Q_MUSHROOM]._qactive == QUEST_ACTIVE) {
				// The Brain first: the later stage was never reached behind QS_MUSHGIVEN (a lower value), and Pepin asked for
				// the Brain again after taking it (round 26 audit; upstream's order).
				if (Quests[Q_MUSHROOM]._qvar1 >= QS_BRAINGIVEN) {
					QuestDialogTable[TOWN_HEALER][Q_MUSHROOM] = TEXT_NONE;
				} else if (Quests[Q_MUSHROOM]._qvar1 >= QS_MUSHGIVEN) {
					QuestDialogTable[TOWN_WITCH][Q_MUSHROOM] = TEXT_NONE;
					QuestDialogTable[TOWN_HEALER][Q_MUSHROOM] = TEXT_MUSH3;
				}
			}
		}
	}
	if (currlevel == Quests[Q_VEIL]._qlevel + 1 && Quests[Q_VEIL]._qactive == QUEST_ACTIVE && Quests[Q_VEIL]._qvar1 == 0 && !gbIsMultiplayer) {
		Quests[Q_VEIL]._qvar1 = 1;
		MakeRoomForGuaranteedReward(); // Lachdanan's elixir, not lost on a full floor (round 26 audit)
		SpawnQuestItem(IDI_GLDNELIX, { 0, 0 }, 5, 1, true);
		NetSendCmdQuest(true, Quests[Q_VEIL]);
	}
	if (setlevel && setlvlnum == SL_VILEBETRAYER) {
		if (Quests[Q_BETRAYER]._qvar1 >= 4)
			ObjChangeMapResync(1, 11, 20, 18);
		if (Quests[Q_BETRAYER]._qvar1 >= 6) {
			ObjChangeMapResync(1, 18, 20, 24);
			if (gbIsMultiplayer) {
				Monster *lazarus = FindUniqueMonster(UniqueMonsterType::Lazarus);
				if (lazarus != nullptr) {
					// Ensure lazarus starts attacking again after returning to the level
					lazarus->goal = MonsterGoal::Normal;
					lazarus->talkMsg = TEXT_NONE;
				}
			}
		}
		if (Quests[Q_BETRAYER]._qvar1 >= 7)
			InitVPTriggers();
		for (int i = 0; i < ActiveObjectCount; i++)
			SyncObjectAnim(Objects[ActiveObjects[i]]);
	}
	if (currlevel == Quests[Q_BETRAYER]._qlevel
	    && !setlevel
	    && (Quests[Q_BETRAYER]._qvar2 == 1 || Quests[Q_BETRAYER]._qvar2 >= 3)
	    && (Quests[Q_BETRAYER]._qactive == QUEST_ACTIVE || Quests[Q_BETRAYER]._qactive == QUEST_DONE)) {
		Quests[Q_BETRAYER]._qvar2 = 2;
		NetSendCmdQuest(true, Quests[Q_BETRAYER]);
	}
	if (currlevel == Quests[Q_DIABLO]._qlevel
	    && !setlevel
	    && Quests[Q_DIABLO]._qactive == QUEST_ACTIVE
	    && gbIsMultiplayer) {
		Point posPentagram = Quests[Q_DIABLO].position;
		ObjChangeMapResync(posPentagram.x, posPentagram.y, posPentagram.x + 5, posPentagram.y + 5);
		InitL4Triggers();
	}
	if (currlevel == 0
	    && Quests[Q_PWATER]._qactive == QUEST_DONE
	    && gbIsMultiplayer) {
		CleanTownFountain();
	}
	if (Quests[Q_GARBUD].IsAvailable() && gbIsMultiplayer) {
		Monster *garbud = FindUniqueMonster(UniqueMonsterType::Garbud);
		if (garbud != nullptr && Quests[Q_GARBUD]._qvar1 != QS_GHARBAD_INIT) {
			switch (Quests[Q_GARBUD]._qvar1) {
			case QS_GHARBAD_FIRST_ITEM_READY:
				garbud->goal = MonsterGoal::Inquiring;
				break;
			case QS_GHARBAD_FIRST_ITEM_SPAWNED:
				garbud->talkMsg = TEXT_GARBUD2;
				garbud->flags |= MFLAG_QUEST_COMPLETE;
				garbud->goal = MonsterGoal::Talking;
				break;
			case QS_GHARBAD_SECOND_ITEM_NEARLY_DONE:
				garbud->talkMsg = TEXT_GARBUD3;
				garbud->flags |= MFLAG_QUEST_COMPLETE;
				garbud->goal = MonsterGoal::Inquiring;
				break;
			case QS_GHARBAD_SECOND_ITEM_READY:
				garbud->talkMsg = TEXT_GARBUD4;
				garbud->flags |= MFLAG_QUEST_COMPLETE;
				garbud->goal = MonsterGoal::Inquiring;
				break;
			case QS_GHARBAD_ATTACKING:
				garbud->talkMsg = TEXT_NONE;
				garbud->flags |= MFLAG_QUEST_COMPLETE;
				garbud->goal = MonsterGoal::Normal;
				garbud->activeForTicks = UINT8_MAX;
				break;
			}
		}
	}
	if (Quests[Q_ZHAR].IsAvailable() && gbIsMultiplayer) {
		Monster *zhar = FindUniqueMonster(UniqueMonsterType::Zhar);
		if (zhar != nullptr && Quests[Q_ZHAR]._qvar1 != QS_ZHAR_INIT) {
			zhar->flags |= MFLAG_QUEST_COMPLETE;

			switch (Quests[Q_ZHAR]._qvar1) {
			case QS_ZHAR_ITEM_SPAWNED:
				zhar->goal = MonsterGoal::Talking;
				break;
			case QS_ZHAR_ANGRY:
				zhar->talkMsg = TEXT_ZHAR2;
				zhar->goal = MonsterGoal::Inquiring;
				break;
			case QS_ZHAR_ATTACKING:
				zhar->talkMsg = TEXT_NONE;
				zhar->goal = MonsterGoal::Normal;
				zhar->activeForTicks = UINT8_MAX;
				break;
			}
		}
	}
	if (Quests[Q_WARLORD].IsAvailable() && gbIsMultiplayer) {
		Monster *warlord = FindUniqueMonster(UniqueMonsterType::WarlordOfBlood);
		if (warlord != nullptr && Quests[Q_WARLORD]._qvar1 == QS_WARLORD_ATTACKING) {
			warlord->activeForTicks = UINT8_MAX;
			warlord->talkMsg = TEXT_NONE;
			warlord->goal = MonsterGoal::Normal;
		}
	}
	if (Quests[Q_VEIL].IsAvailable() && gbIsMultiplayer) {
		Monster *lachdan = FindUniqueMonster(UniqueMonsterType::Lachdan);
		if (lachdan != nullptr) {
			switch (Quests[Q_VEIL]._qvar2) {
			case QS_VEIL_EARLY_RETURN:
				lachdan->talkMsg = TEXT_VEIL10;
				lachdan->goal = MonsterGoal::Inquiring;
				break;
			case QS_VEIL_ITEM_SPAWNED:
				if (lachdan->talkMsg == TEXT_VEIL11)
					break;
				lachdan->talkMsg = TEXT_VEIL11;
				lachdan->flags |= MFLAG_QUEST_COMPLETE;
				lachdan->goal = MonsterGoal::Inquiring;
				break;
			}
		}
	}

	LoadingMapObjects = false;
}

Rectangle GetQuestLogPanelRect()
{
	return { QuestPanelOrigin(), QuestPanelSize };
}

void DrawQuestLog(const Surface &out)
{
	int l = QuestLogMouseToEntry();
	if (l >= 0) {
		SelectedQuest = l;
	}
	const auto x = QuestListRect().position.x;

	// Oracool V1: same chrome as the waypoint list and the event log. The parchment CEL (pQLogCel)
	// it used to draw here is still loaded - minitext and the waypoint list's fallback both want it
	// - it is just no longer this window's background.
	// Oracool (2026-08-16): the painted side-panel background, shared with the stash, inventory,
	// character sheet and waypoint list - all five are the same 340x720 window, so one piece of art
	// serves them. The procedural fill and bevel stay as the fallback, so the art is droppable.
	//
	// The rule under the title went with it: the background brings its own header framing, so the
	// separator was a second line drawn across the first.
	const Rectangle panel { QuestPanelOrigin(), QuestPanelSize };
	if (oracool::HasSidePanelArt()) {
		oracool::DrawSidePanelArt(out, panel.position);
	} else {
		DrawHalfTransparentRectTo(out, panel.position.x, panel.position.y, panel.size.width, panel.size.height);
		oracool::DrawOrnateBorder(out, panel);
	}

	const Rectangle labelArea { panel.position + Displacement { QuestPanelMargin, oracool::PanelTitleTop },
		{ QuestPanelSize.width - 2 * QuestPanelMargin, oracool::PanelTitleHeight } };
	oracool::DrawOutlinedString(out, _("QUESTS"), labelArea,
	    UiFlags::ColorWhitegold | UiFlags::FontSize30 | UiFlags::AlignCenter | UiFlags::VerticalCenter);

	int y = QuestListRect().position.y + ListYOffset;
	for (int i = 0; i < EncounteredQuestCount; i++) {
		if (i == FirstFinishedQuest) {
			y += FinishedQuestOffset;
		}
		PrintQLString(out, x, y, _(QuestsData[EncounteredQuests[i]]._qlstr), i == SelectedQuest, i >= FirstFinishedQuest);
		y += LineSpacing;
	}
}

void StartQuestlog()
{

	auto sortQuestIdx = [](int a, int b) {
		return QuestsData[a].questBookOrder < QuestsData[b].questBookOrder;
	};

	// Oracool: user request - with Quest Log Reveal All on (single-player only), every quest
	// that's part of this game (i.e. not excluded by quest-pool randomization, _qactive !=
	// QUEST_NOTAVAIL) shows up in the log immediately, including ones the player hasn't
	// discovered yet (still QUEST_INIT). Deliberately does NOT touch _qactive/_qlog themselves -
	// every quest's actual trigger (finding the right object/NPC, reaching the right level,
	// spawning its quest item) still runs exactly as vanilla intends, unaffected by this. Some
	// quests (e.g. the Mushroom quest's tome spawn in ResyncQuests()) gate a real mechanical step
	// on _qactive still being QUEST_INIT, so forcing that state early would silently break them -
	// this only widens what the log *displays*.
	const bool revealUndiscovered = !gbIsMultiplayer && *sgOptions.Oracool.questLogRevealAll;

	EncounteredQuestCount = 0;
	for (auto &quest : Quests) {
		// Revealed: every quest this game can hold that is not finished - INIT, ACTIVE with or without its log flag, and the
		// Jersey's tease states. Testing INIT alone dropped a quest the moment vanilla set it ACTIVE without logging it (the
		// lair entered before Ogden, Lazarus killed before Cain) and hid the Jersey while he teased; and it listed quests
		// that cannot exist here - the Wandering Trader, the absent one of cow and farmer (round 8 audit, v1.12.233).
		const bool revealed = revealUndiscovered && IsQuestEnabledInThisGame(quest)
		    && IsNoneOf(quest._qactive, QUEST_NOTAVAIL, QUEST_DONE, QUEST_HIVE_DONE);
		if ((quest._qactive == QUEST_ACTIVE && quest._qlog) || revealed) {
			EncounteredQuests[EncounteredQuestCount] = quest._qidx;
			EncounteredQuestCount++;
		}
	}
	FirstFinishedQuest = EncounteredQuestCount;
	for (auto &quest : Quests) {
		if (quest._qactive == QUEST_DONE || quest._qactive == QUEST_HIVE_DONE) {
			EncounteredQuests[EncounteredQuestCount] = quest._qidx;
			EncounteredQuestCount++;
		}
	}

	std::sort(&EncounteredQuests[0], &EncounteredQuests[FirstFinishedQuest], sortQuestIdx);
	std::sort(&EncounteredQuests[FirstFinishedQuest], &EncounteredQuests[EncounteredQuestCount], sortQuestIdx);

	bool twoBlocks = FirstFinishedQuest != 0 && FirstFinishedQuest < EncounteredQuestCount;

	ListYOffset = 0;
	FinishedQuestOffset = !twoBlocks ? 0 : LineHeight / 2;

	int overallMinHeight = EncounteredQuestCount * LineHeight + FinishedQuestOffset;
	int space = InnerPanel.size.height;

	if (EncounteredQuestCount > 0) {
		int additionalSpace = space - overallMinHeight;
		int addLineSpacing = additionalSpace / EncounteredQuestCount;
		addLineSpacing = std::min(MaxSpacing - LineHeight, addLineSpacing);
		LineSpacing = LineHeight + addLineSpacing;
		if (twoBlocks) {
			int additionalSepSpace = additionalSpace - (addLineSpacing * EncounteredQuestCount);
			additionalSepSpace = std::min(LineHeight, additionalSepSpace);
			FinishedQuestOffset = std::max(4, additionalSepSpace);
		}

		int overallHeight = EncounteredQuestCount * LineSpacing + FinishedQuestOffset;
		ListYOffset += (space - overallHeight) / 2;
	}

	SelectedQuest = FirstFinishedQuest == 0 ? -1 : 0;
	// This closed nothing, so opening the log with the character sheet up left it invisible behind
	// the sheet - the same fault as the stash's (user report, 2026-08-31).
	if (!TakeLeftPanelSlot(LeftPanelContent::QuestLog))
		return;
	QuestLogIsOpen = true;
}

void QuestlogUp()
{
	if (FirstFinishedQuest == 0) {
		SelectedQuest = -1;
	} else {
		SelectedQuest--;
		if (SelectedQuest < 0) {
			SelectedQuest = FirstFinishedQuest - 1;
		}
		PlaySFX(IS_TITLEMOV);
	}
}

void QuestlogDown()
{
	if (FirstFinishedQuest == 0) {
		SelectedQuest = -1;
	} else {
		SelectedQuest++;
		if (SelectedQuest == FirstFinishedQuest) {
			SelectedQuest = 0;
		}
		PlaySFX(IS_TITLEMOV);
	}
}

void QuestlogEnter()
{
	PlaySFX(IS_TITLSLCT);
	if (EncounteredQuestCount != 0 && SelectedQuest >= 0 && SelectedQuest < FirstFinishedQuest)
		InitQTextMsg(Quests[EncounteredQuests[SelectedQuest]]._qmsg);
	QuestLogIsOpen = false;
}

void QuestlogESC()
{
	int l = QuestLogMouseToEntry();
	if (l != -1) {
		QuestlogEnter();
	}
}

void SetMultiQuest(int q, quest_state s, bool log, int v1, int v2, int16_t qmsg)
{
	if (gbIsSpawn)
		return;

	auto &quest = Quests[q];
	quest_state oldQuestState = quest._qactive;
	if (quest._qactive != QUEST_DONE) {
		if (s > quest._qactive || (IsAnyOf(s, QUEST_ACTIVE, QUEST_DONE) && IsAnyOf(quest._qactive, QUEST_HIVE_TEASE1, QUEST_HIVE_TEASE2, QUEST_HIVE_ACTIVE)))
			quest._qactive = s;
		if (log)
			quest._qlog = true;
	}
	if (v1 > quest._qvar1)
		quest._qvar1 = v1;
	quest._qvar2 = v2;
	quest._qmsg = static_cast<_speech_id>(qmsg);
	if (!UseMultiplayerQuests()) {
		// Ensure that changes on another client is also updated on our own
		ResyncQuests();

		bool questGotCompleted = oldQuestState != QUEST_DONE && quest._qactive == QUEST_DONE;
		// Ensure that water also changes for remote players
		if (quest._qidx == Q_PWATER && questGotCompleted && MyPlayer->isOnLevel(quest._qslvl))
			StartPWaterPurify();
		if (quest._qidx == Q_GIRL && questGotCompleted && MyPlayer->isOnLevel(0))
			UpdateGirlAnimAfterQuestComplete();
		if (quest._qidx == Q_JERSEY && questGotCompleted && MyPlayer->isOnLevel(0))
			UpdateCowFarmerAnimAfterQuestComplete();
	}
}

bool UseMultiplayerQuests()
{
	return sgGameInitInfo.fullQuests == 0;
}

bool Quest::IsAvailable()
{
	if (setlevel)
		return false;
	if (currlevel != _qlevel)
		return false;
	if (_qactive == QUEST_NOTAVAIL)
		return false;
	if (QuestsData[_qidx].isSinglePlayerOnly && UseMultiplayerQuests())
		return false;

	return true;
}

} // namespace devilution
