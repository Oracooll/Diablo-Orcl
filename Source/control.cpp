/**
 * @file control.cpp
 *
 * Implementation of the character and main control panels
 */
#include "control.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <fmt/format.h>

#include "DiabloUI/text_input.hpp"
#include "automap.h"
#include "controls/modifier_hints.h"
#include "controls/plrctrls.h"
#include "help.h"         // HelpFlag - a modal over the world
#include "oracool/runeword_book.h" // CloseRunewordBook - the full-screen book
#include "qol/chatlog.h" // ChatLogFlag - the same
#include "cursor.h"
#include "engine/backbuffer_state.hpp"
#include "engine/clx_sprite.hpp"
#include "engine/load_cel.hpp"
#include "engine/render/clx_render.hpp"
#include "engine/render/text_render.hpp"
#include "engine/trn.hpp"
#include "effects.h"
#include "error.h"
#include "gamemenu.h"
#include "init.h"
#include "inv.h"
#include "inv_iterators.hpp"
#include "levels/setmaps.h"
#include "levels/trigs.h"
#include "lighting.h"
#include "minitext.h"
#include "missiles.h"
#include "options.h"
#include "oracool/cursor_tooltip.h" // ShowPanelStringsAsHintCard - the HUD controls' card
#include "oracool/attack_skills.h"
#include "oracool/combat_odds.h"
#include "oracool/auto_save.h"
#include "oracool/dev_notes.h" // LogDevelopmentNote: the /dev chat command
#include "oracool/skill_picker.h"
#include "oracool/class_tree.h" // the burning aura is what the RMB well holds
#include "oracool/event_log.h"
#include "oracool/furious_charge.h"
#include "oracool/hud_art.h"
#include "oracool/hud_layout.h"
#include "oracool/run_toggle.h" // IsRunEnabled - the belt toggle's hover hint names the mode
#include "oracool/hud_menu.h"
#include "oracool/inventory_layout.h"
#include "engine/render/primitive_render.hpp"
#include "oracool/ornate_border.h"
#include "oracool/levski_roar.h"
#include "oracool/workshop.h"
#include "oracool/shop_grid.h"
#include "oracool/shop_tabs.h" // IsShopTab - the Salvage tab is a tab without being a grid
#include "oracool/crafting_menu.h"
#include "oracool/advanced_stats.h" // the right-hand slot's third window
#include "oracool/ui_sound.h"
#include "oracool/waypoint_menu.h"
#include "oracool/xp_counter.h"
#include "panels/charpanel.hpp"
#include "quests.h" // GetQuestLogPanelRect
#include "panels/mainpanel.hpp"
#include "panels/spell_book.hpp"
#include "panels/spell_icons.hpp"
#include "panels/spell_list.hpp"
#include "playerdat.hpp"
#include "qol/itemlabels.h"
#include "qol/stash.h"
#include "qol/xpbar.h"
#include "stores.h"
#include "towners.h"
#include "utils/format_int.hpp"
#include "utils/language.h"
#include "utils/log.hpp"
#include "utils/sdl_geometry.h"
#include "utils/stdcompat/optional.hpp"
#include "utils/str_case.hpp"
#include "utils/str_cat.hpp"
#include "utils/string_or_view.hpp"
#include "utils/utf8.hpp"

#ifdef _DEBUG
#include "debug.h"
#endif

namespace devilution {

bool DropGoldFlag;
TextInputCursorState GoldDropCursor;
char GoldDropText[21];
namespace {
int8_t GoldDropInvIndex;
/**
 * WHICH CONTAINER THE PROMPT IS SPLITTING (2026-09-22).
 *
 * GoldDropInvIndex alone meant "backpack list index, or belt index if it is past
 * INVITEM_INV_LAST" - which is the whole of vanilla's world and only a third of this one. The
 * backpack has ten pages and the stash has its own list, and the amount prompt could reach neither:
 * a shift-right-click in an extra tab read the item at that index in TAB ONE, and in the stash it
 * did nothing at all.
 *
 * The tab and the page are RECORDED rather than read at commit time, because the prompt does not
 * capture the mouse: the player can turn to another page while it is open, and the removal would
 * then compact the wrong list.
 */
int GoldDropInvTab;
uint16_t GoldDropStashIndex = StashStruct::EmptyCell;
unsigned GoldDropStashPage;
std::optional<NumberInputState> GoldDropInputState;

/** @brief The stack the prompt is splitting, or nullptr if it has gone since the prompt opened. */
Item *GoldDropSourceItem()
{
	if (GoldDropStashIndex != StashStruct::EmptyCell) {
		// The page has to be the one it was opened on, or RemoveStashItem would clear cell
		// references on whichever page is showing instead - see StashStruct::RemoveStashItem, which
		// walks GetCurrentGrid().
		if (Stash.GetPage() != GoldDropStashPage || GoldDropStashIndex >= Stash.stashList.size())
			return nullptr;
		return &Stash.stashList[GoldDropStashIndex];
	}
	Player &player = *MyPlayer;
	if (GoldDropInvIndex > INVITEM_INV_LAST)
		return &player.SpdList[GoldDropInvIndex - INVITEM_BELT_FIRST];
	const int index = GoldDropInvIndex - INVITEM_INV_FIRST;
	if (index < 0)
		return nullptr;
	if (GoldDropInvTab == 0)
		return &player.InvList[index];
	return &player.InvTabList[GoldDropInvTab - 1][index];
}
} // namespace

bool chrbtn[4];
bool chrDecBtn[4];
bool lvlbtndown;
bool chrbtnactive;
bool resetStatsButtonDown;
UiFlags InfoColor;
std::vector<UiFlags> InfoStringLineColors;
std::vector<uint16_t> InfoStringLineTailStart;
std::vector<std::vector<PanelLineRun>> InfoStringLineRuns;
bool talkflag;
bool sbookflag;
bool chrflag;
PanelInfoText InfoString;

PanelInfoText &PanelInfoText::operator=(StringOrView str)
{
	// The whole point of the type - see control.h. A bare assignment means "one hover, one colour",
	// and leaving the previous hover's per-line colours behind is what made that a crash.
	text = std::move(str);
	InfoStringLineColors.clear();
	InfoStringLineTailStart.clear();
	InfoStringLineRuns.clear();
	return *this;
}
bool panelflag;
bool spselflag;
Rectangle MainPanel;
Rectangle LeftPanel;
Rectangle RightPanel;
std::optional<OwnedSurface> pBtmBuff;
OptionalOwnedClxSpriteList pGBoxBuff;

const Rectangle &GetMainPanel()
{
	return MainPanel;
}
const Rectangle &GetLeftPanel()
{
	return LeftPanel;
}
const Rectangle &GetRightPanel()
{
	return RightPanel;
}
bool IsLeftPanelOpen()
{
	return GetLeftPanelContent() != LeftPanelContent::None;
}
LeftPanelContent GetLeftPanelContent()
{
	// Oracool bug fix: user report - with the waypoint list and the Character panel both open,
	// clicking the Character panel teleported the player. The draw chain and the click chain each
	// had their own hand-written precedence and they disagreed: the drawing said Character wins and
	// showed it, while the click handling tested the waypoint list first and routed the click
	// there. Both now read this one function, so they cannot drift apart again.
	//
	// The order is the one the drawing already used, which is also the sensible one: whatever the
	// player opened most recently is what they see, and the panels close each other rather than
	// stacking.
	if (chrflag)
		return LeftPanelContent::Character;
	if (QuestLogIsOpen)
		return LeftPanelContent::QuestLog;
	if (IsStashOpen)
		return LeftPanelContent::Stash;
	if (oracool::IsWaypointMenuOpen())
		return LeftPanelContent::WaypointMenu;
	if (oracool::IsCraftingMenuOpen())
		return LeftPanelContent::Crafting;
	return LeftPanelContent::None;
}
Rectangle GetLeftPanelContentRect()
{
	// Oracool bug fix: user rule - "area of UI screens should in no case permit clicks on the
	// ground". Routing used GetLeftPanel(), the vanilla 320x352, but three of the four contents
	// have since grown to their own 340x720 windows. Every click below y=352 or right of x=320
	// therefore missed the panel entirely and fell through to the world: the character sheet's
	// RESET button was unclickable, its + buttons responded only left of x=320, waypoint entries
	// 8-16 walked the player instead of warping, and the same for quest log rows.
	//
	// Rect and content now come from the same switch, so a window cannot be drawn in one place and
	// hit-tested in another. A window that grows must export its rect and be added here.
	switch (GetLeftPanelContent()) {
	case LeftPanelContent::Character:
		return GetCharacterPanelRect();
	case LeftPanelContent::QuestLog:
		return GetQuestLogPanelRect();
	case LeftPanelContent::WaypointMenu:
		return oracool::GetWaypointMenuRect();
	case LeftPanelContent::Stash:
		return GetStashPanelRect();
	case LeftPanelContent::Crafting:
		return oracool::GetCraftingMenuRect();
	case LeftPanelContent::None:
		break;
	}
	return { { 0, 0 }, { 0, 0 } };
}
bool IsOverLeftPanel(Point position)
{
	return GetLeftPanelContent() != LeftPanelContent::None
	    && GetLeftPanelContentRect().contains(position);
}
void CloseLeftPanelContent()
{
	switch (GetLeftPanelContent()) {
	case LeftPanelContent::Character:
		CloseCharPanel();
		break;
	case LeftPanelContent::QuestLog:
		QuestLogIsOpen = false;
		break;
	case LeftPanelContent::Stash:
		// CloseStash(), not IsStashOpen = false: it also returns a held item and releases the
		// chest object, both of which leak if the flag is cleared behind its back.
		CloseStash();
		break;
	case LeftPanelContent::WaypointMenu:
		oracool::CloseWaypointMenu();
		break;
	case LeftPanelContent::Crafting:
		oracool::CloseCraftingMenu();
		break;
	case LeftPanelContent::None:
		break;
	}
}
bool TakeLeftPanelSlot(LeftPanelContent content)
{
	// User report, 2026-08-31: "if hero stats screen is on and i open the stash the stash window is
	// not display or it is but under the hero stats."
	//
	// Five windows share one slot and GetLeftPanelContent picks ONE of them by a fixed precedence,
	// with Character first. Opening a window therefore does not make it the visible one - taking the
	// slot does, and taking the slot means closing whatever else holds it. Four of the five openers
	// were doing that by hand and getting it wrong in different ways: OpenStash and StartQuestlog
	// closed nothing at all, OpenCharPanel closed three of the four siblings, and the waypoint menu
	// closed the two ABOVE it in precedence but not the stash, which is also above it.
	//
	// Every miss has the same symptom and it is a confusing one: the window you just opened is
	// invisible, its clicks route to the window covering it, and it appears later when that one is
	// closed - as if it had been waiting behind it. Which it had.
	//
	// So the openers stop listing siblings. This closes everything that is not @p content, which
	// cannot fall behind as contents are added: a new LeftPanelContent enumerator makes this switch
	// fail to compile until it is handled.
	if (content != LeftPanelContent::Character)
		CloseCharPanel();
	if (content != LeftPanelContent::QuestLog)
		QuestLogIsOpen = false;
	if (content != LeftPanelContent::Stash) {
		// CloseStash(), not IsStashOpen = false, for the reason CloseLeftPanelContent gives: it also
		// returns a held item and releases the chest object.
		CloseGoldWithdraw();
		CloseStash();
	}
	if (content != LeftPanelContent::WaypointMenu)
		oracool::CloseWaypointMenu();
	if (content != LeftPanelContent::Crafting)
		oracool::CloseCraftingMenu();
	// The runeword book is the whole screen: C and Q opened their window hidden under it (round 20 audit, v1.12.245).
	oracool::CloseRunewordBook();
	// And the workshop and the Cube, which share the rect though they are not left-panel contents: the sheet and the quest
	// log opened invisible under them (round 16 audit, v1.12.241). Either may refuse with items it cannot give back.
	oracool::CloseWorkshop();
	oracool::CloseLevskiRoar();
	// A refused close keeps the slot: the caller backs out rather than opening invisible under it (round 17 audit).
	return !oracool::IsWorkshopOpen() && !oracool::IsLevskiRoarOpen();
}

bool IsModalPromptOpen()
{
	// The same three ReleaseKey already treats as modal (diablo.cpp), asked in one place so the
	// keyboard and the mouse cannot disagree about who owns input.
	return DropGoldFlag || IsWithdrawGoldOpen || IsRefreshUntilPromptOpen;
}

bool IsOverAnyInterface(Point position)
{
	// Order is by cost, not by importance - every one of these is authoritative for its own
	// surface, so the answer does not depend on which is asked first.
	if (IsOverLeftPanel(position))
		return true;
	if (IsOverRightPanel(position))
		return true;
	if (ChatLogFlag || HelpFlag) // modal: nothing behind them takes a click (round 20 audit)
		return true;
	if (oracool::IsPointOverHudChrome(position))
		return true;
	if (oracool::IsPointOverFloatingWindow(position))
		return true;
	// The shop is a panel rather than a modal screen (see LeftMouseDown), so only the shop's own
	// rect counts as interface - the rest of the screen stays clickable, which is what lets an item
	// be dragged out of the inventory to sell it.
	// Every shop TAB, not just the grid ones (2026-09-21) - the Salvage tab is a store screen that
	// draws a page in this same rect, and a page that does not count as interface is a page the world
	// can be clicked through.
	if (stextflag != TalkID::None && oracool::IsShopTab(stextflag)
	    && oracool::IsPointOverShop(position))
		return true;
	return false;
}

bool IsRightPanelOpen()
{
	// The Advanced Stats window (2026-09-26) takes the same slot, so it counts: the corner HUD hides
	// behind it, the view shifts for it, and every "is the right side covered" question agrees.
	return invflag || sbookflag || oracool::IsAdvancedStatsOpen();
}
bool IsOverRightPanel(Point position)
{
	// The inventory and the spellbook no longer share a rect - see the header. Both have since
	// grown into their own 340x720 windows, so neither may be tested against RightPanel's vanilla
	// 320x352: a window hit-tested smaller than it draws lets clicks reach the ground beneath it.
	if (invflag && oracool::GetInventoryPanelRect().contains(position))
		return true;
	// The Advanced Stats window by its own rect - never two windows in the slot at once, see
	// oracool/advanced_stats.h, so the order against the two above does not matter.
	if (oracool::IsAdvancedStatsOpen() && oracool::GetAdvancedStatsRect().contains(position))
		return true;
	return sbookflag && GetSpellBookPanelRect().contains(position);
}

constexpr Size IncrementAttributeButtonSize { 41, 22 };
/** Maps from attribute_id to the rectangle on screen used for attribute increment buttons. */
Rectangle ChrBtnsRect[4] = {
	{ { 137, 138 }, IncrementAttributeButtonSize },
	{ { 137, 166 }, IncrementAttributeButtonSize },
	{ { 137, 195 }, IncrementAttributeButtonSize },
	{ { 137, 223 }, IncrementAttributeButtonSize }
};
Rectangle ChrDecBtnsRect[4] {};

namespace {

OptionalOwnedClxSpriteList pDurIcons;

/**
 * @brief How long a line typed into the chat box may be.
 *
 * Was MAX_SEND_STR_LEN, the network packet's 80 bytes. The box is single-player now and its long
 * lines are /dev notes (user, 2026-09-24 dev note: "dev notes are too short for me to express
 * myself. make it take longer text"); NetSendCmdString copies with CopyUtf8 into its own 80, so a
 * line that ever did go out would be cut there and nowhere else. The drawn box is still the real
 * limit - see DrawTalkPan, which grows it for a note.
 */
constexpr size_t ChatInputMaxBytes = 640;
char TalkSave[8][ChatInputMaxBytes];
uint8_t TalkSaveIndex;
uint8_t NextTalkSave;
char TalkMessage[ChatInputMaxBytes];
/** @brief Oracool: whether the event log was open when chat opened, so it can be put back. */
bool LogWasOpenBeforeChat = false;
int sgbPlrTalkTbl;
bool WhisperList[MAX_PLRS];

TextInputCursorState ChatCursor;
std::optional<TextInputState> ChatInputState;

int DrawDurIcon4Item(const Surface &out, Item &pItem, int x, int c)
{
	const int durabilityThresholdGold = 5;
	const int durabilityThresholdRed = 2;

	if (pItem.isEmpty())
		return x;
	if (pItem._iDurability > durabilityThresholdGold)
		return x;
	if (c == 0) {
		switch (pItem._itype) {
		case ItemType::Sword:
			c = 1;
			break;
		case ItemType::Axe:
			c = 5;
			break;
		case ItemType::Bow:
			c = 6;
			break;
		case ItemType::Mace:
			c = 4;
			break;
		case ItemType::Staff:
			c = 7;
			break;
		case ItemType::Shield:
		default:
			c = 0;
			break;
		}
	}

	// Calculate how much of the icon should be gold and red
	int height = (*pDurIcons)[c].height(); // Height of durability icon CEL
	int partition = 0;
	if (pItem._iDurability > durabilityThresholdRed) {
		int current = pItem._iDurability - durabilityThresholdRed;
		partition = (height * current) / (durabilityThresholdGold - durabilityThresholdRed);
	}

	// Draw icon
	int y = -17 + GetMainPanel().position.y;
	if (partition > 0) {
		const Surface stenciledBuffer = out.subregionY(y - partition, partition);
		ClxDraw(stenciledBuffer, { x, partition }, (*pDurIcons)[c + 8]); // Gold icon
	}
	if (partition != height) {
		const Surface stenciledBuffer = out.subregionY(y - height, height - partition);
		ClxDraw(stenciledBuffer, { x, height }, (*pDurIcons)[c]); // Red icon
	}

	// Oracool: a broken (0 durability) item also gets a red X stamped over its durability icon here,
	// in addition to the one DrawItem/DrawItem2 already stamp over the item's inventory/equipped icon.
	if (pItem._iOracoolBroken) {
		const int width = static_cast<int>((*pDurIcons)[c].width());
		DrawBrokenItemMarker(out, { x, y - height }, width, height);
	}

	return x - (*pDurIcons)[c].height() - 8; // Add in spacing for the next durability icon
}

struct TextCmdItem {
	const std::string text;
	const std::string description;
	const std::string requiredParameter;
	std::string (*actionProc)(const string_view);
};

extern std::vector<TextCmdItem> TextCmdList;

std::string TextCmdHelp(const string_view parameter)
{
	if (parameter.empty()) {
		std::string ret;
		StrAppend(ret, _("Available Commands:"));
		for (const TextCmdItem &textCmd : TextCmdList) {
			StrAppend(ret, " ", _(textCmd.text));
		}
		return ret;
	}
	auto textCmdIterator = std::find_if(TextCmdList.begin(), TextCmdList.end(), [&](const TextCmdItem &elem) { return elem.text == parameter; });
	if (textCmdIterator == TextCmdList.end())
		return StrCat(_("Command "), parameter, _(" is unkown."));
	auto &textCmdItem = *textCmdIterator;
	if (textCmdItem.requiredParameter.empty())
		return StrCat(_("Description: "), _(textCmdItem.description), _("\nParameters: No additional parameter needed."));
	return StrCat(_("Description: "), _(textCmdItem.description), _("\nParameters: "), _(textCmdItem.requiredParameter));
}

void AppendArenaOverview(std::string &ret)
{
	for (int arena = SL_FIRST_ARENA; arena <= SL_LAST_ARENA; arena++) {
		StrAppend(ret, "\n", arena - SL_FIRST_ARENA + 1, " (", QuestLevelNames[arena], ")");
	}
}

const dungeon_type DungeonTypeForArena[] = {
	dungeon_type::DTYPE_CATHEDRAL, // SL_ARENA_CHURCH
	dungeon_type::DTYPE_HELL,      // SL_ARENA_HELL
	dungeon_type::DTYPE_HELL,      // SL_ARENA_CIRCLE_OF_LIFE
};

std::string TextCmdArena(const string_view parameter)
{
	std::string ret;
	if (!gbIsMultiplayer) {
		StrAppend(ret, _("Arenas are only supported in multiplayer."));
		return ret;
	}

	if (parameter.empty()) {
		StrAppend(ret, _("What arena do you want to visit?"));
		AppendArenaOverview(ret);
		return ret;
	}

	int arenaNumber = atoi(parameter.data());
	_setlevels arenaLevel = static_cast<_setlevels>(arenaNumber - 1 + SL_FIRST_ARENA);
	if (arenaNumber < 0 || !IsArenaLevel(arenaLevel)) {
		StrAppend(ret, _("Invalid arena-number. Valid numbers are:"));
		AppendArenaOverview(ret);
		return ret;
	}

	if (!MyPlayer->isOnLevel(0) && !MyPlayer->isOnArenaLevel()) {
		StrAppend(ret, _("To enter a arena, you need to be in town or another arena."));
		return ret;
	}

	setlvltype = DungeonTypeForArena[arenaLevel - SL_FIRST_ARENA];
	StartNewLvl(*MyPlayer, WM_DIABSETLVL, arenaLevel);
	return ret;
}

std::string TextCmdArenaPot(const string_view parameter)
{
	std::string ret;
	if (!gbIsMultiplayer) {
		StrAppend(ret, _("Arenas are only supported in multiplayer."));
		return ret;
	}

	Player &myPlayer = *MyPlayer;

	for (int potNumber = std::max(1, atoi(parameter.data())); potNumber > 0; potNumber--) {
		Item item {};
		InitializeItem(item, IDI_ARENAPOT);
		GenerateNewSeed(item);
		item.updateRequiredStatsCacheForPlayer(myPlayer);

		if (!AutoPlaceItemInBelt(myPlayer, item, true) && !AutoPlaceItemInInventory(myPlayer, item, true)) {
			break; // inventory is full
		}
	}

	return ret;
}

std::string TextCmdInspect(const string_view parameter)
{
	std::string ret;
	if (!gbIsMultiplayer) {
		StrAppend(ret, _("Inspecting only supported in multiplayer."));
		return ret;
	}

	if (parameter.empty()) {
		StrAppend(ret, _("Stopped inspecting players."));
		InspectPlayer = MyPlayer;
		return ret;
	}

	const std::string param = AsciiStrToLower(parameter);
	for (auto &player : Players) {
		const std::string playerName = AsciiStrToLower(player._pName);
		if (playerName.find(param) != std::string::npos) {
			InspectPlayer = &player;
			StrAppend(ret, _("Inspecting player: "));
			StrAppend(ret, player._pName);
			OpenCharPanel();
			if (!sbookflag)
				invflag = true;
			RedrawEverything();
			return ret;
		}
	}
	StrAppend(ret, _("No players found with such a name"));
	return ret;
}

bool IsQuestEnabled(const Quest &quest)
{
	switch (quest._qidx) {
	case Q_FARMER:
		return gbIsHellfire && !sgGameInitInfo.bCowQuest;
	case Q_JERSEY:
		return gbIsHellfire && sgGameInitInfo.bCowQuest;
	case Q_GIRL:
		return gbIsHellfire && sgGameInitInfo.bTheoQuest;
	case Q_CORNSTN:
		return gbIsHellfire && !gbIsMultiplayer;
	case Q_GRAVE:
	case Q_DEFILER:
	case Q_NAKRUL:
		return gbIsHellfire;
	case Q_TRADER:
		return false;
	default:
		return quest._qactive != QUEST_NOTAVAIL;
	}
}

std::string TextCmdLevelSeed(const string_view parameter)
{
	string_view levelType = setlevel ? "set level" : "dungeon level";

	char gameId[] = {
		static_cast<char>((sgGameInitInfo.programid >> 24) & 0xFF),
		static_cast<char>((sgGameInitInfo.programid >> 16) & 0xFF),
		static_cast<char>((sgGameInitInfo.programid >> 8) & 0xFF),
		static_cast<char>(sgGameInitInfo.programid & 0xFF),
		'\0'
	};

	string_view mode = gbIsMultiplayer ? "MP" : "SP";
	string_view questPool = UseMultiplayerQuests() ? "MP" : "Full";

	uint32_t questFlags = 0;
	for (const Quest &quest : Quests) {
		questFlags <<= 1;
		if (IsQuestEnabled(quest))
			questFlags |= 1;
	}

	return StrCat(
	    "Seedinfo for ", levelType, " ", currlevel, "\n",
	    "seed: ", glSeedTbl[currlevel], "\n",
#ifdef _DEBUG
	    "Mid1: ", glMid1Seed[currlevel], "\n",
	    "Mid2: ", glMid2Seed[currlevel], "\n",
	    "Mid3: ", glMid3Seed[currlevel], "\n",
	    "End: ", glEndSeed[currlevel], "\n",
#endif
	    "\n",
	    gameId, " ", mode, "\n",
	    questPool, " quests: ", questFlags, "\n",
	    "Storybook: ", glSeedTbl[16]);
}

/**
 * @brief /dev <note> - files a development note (user, 2026-09-23: "Type /dev in the game").
 *
 * In THIS list rather than debug.cpp's, which only exists under _DEBUG and speaks through the red
 * cheat-console channel. A note is not a cheat. See oracool/dev_notes.h.
 */
std::string TextCmdDev(const string_view parameter)
{
	return oracool::LogDevelopmentNote(parameter);
}

std::vector<TextCmdItem> TextCmdList = {
	{ N_("/help"), N_("Prints help overview or help for a specific command."), N_("[command]"), &TextCmdHelp },
	{ N_("/dev"), N_("Files a development note in development.md, stamped with the time, the build and where you stand."), N_("<note>"), &TextCmdDev },
	{ N_("/arena"), N_("Enter a PvP Arena."), N_("<arena-number>"), &TextCmdArena },
	{ N_("/arenapot"), N_("Gives Arena Potions."), N_("<number>"), &TextCmdArenaPot },
	{ N_("/inspect"), N_("Inspects stats and equipment of another player."), N_("<player name>"), &TextCmdInspect },
	{ N_("/seedinfo"), N_("Show seed infos for current level."), "", &TextCmdLevelSeed },
};

bool CheckTextCommand(const string_view text)
{
	if (text.size() < 1 || text[0] != '/')
		return false;

	auto textCmdIterator = std::find_if(TextCmdList.begin(), TextCmdList.end(), [&](const TextCmdItem &elem) { return text.find(elem.text) == 0 && (text.length() == elem.text.length() || text[elem.text.length()] == ' '); });
	if (textCmdIterator == TextCmdList.end()) {
		InitDiabloMsg(StrCat(_("Command \""), text, "\" is unknown."));
		return true;
	}

	TextCmdItem &textCmd = *textCmdIterator;
	string_view parameter = "";
	if (text.length() > (textCmd.text.length() + 1))
		parameter = text.substr(textCmd.text.length() + 1);
	const std::string result = textCmd.actionProc(parameter);
	if (result != "")
		InitDiabloMsg(result);
	return true;
}

void ResetTalkMsg()
{
#ifdef _DEBUG
	if (CheckDebugTextCommand(TalkMessage))
		return;
#endif
	if (CheckTextCommand(TalkMessage))
		return;

	uint32_t pmask = 0;

	for (size_t i = 0; i < Players.size(); i++) {
		if (WhisperList[i])
			pmask |= 1 << i;
	}

	NetSendCmdString(pmask, TalkMessage);
}

void ControlPressEnter()
{
	if (TalkMessage[0] != 0) {
		ResetTalkMsg();
		uint8_t i = 0;
		for (; i < 8; i++) {
			if (strcmp(TalkSave[i], TalkMessage) == 0)
				break;
		}
		if (i >= 8) {
			strcpy(TalkSave[NextTalkSave], TalkMessage);
			NextTalkSave++;
			NextTalkSave &= 7;
		} else {
			uint8_t talkSave = NextTalkSave - 1;
			talkSave &= 7;
			if (i != talkSave) {
				strcpy(TalkSave[i], TalkSave[talkSave]);
				*BufCopy(TalkSave[talkSave], ChatInputState->value()) = '\0';
			}
		}
		TalkMessage[0] = '\0';
		TalkSaveIndex = NextTalkSave;
	}
	control_reset_talk();
}

void ControlUpDown(int v)
{
	for (int i = 0; i < 8; i++) {
		TalkSaveIndex = (v + TalkSaveIndex) & 7;
		if (TalkSave[TalkSaveIndex][0] != 0) {
			ChatInputState->assign(TalkSave[TalkSaveIndex]);
			return;
		}
	}
}

void RemoveGold(Player &player, int goldIndex, int amount)
{
	const int gi = goldIndex - INVITEM_INV_FIRST;
	player.InvList[gi]._ivalue -= amount;
	if (player.InvList[gi]._ivalue > 0) {
		SetPlrHandGoldCurs(player.InvList[gi]);
		NetSyncInvItem(player, gi);
	} else {
		player.RemoveInvItem(gi);
	}

	MakeGoldStack(player.HoldItem, amount);
	NewCursor(player.HoldItem);

	player._pGold = CalculateGold(player);
}

/**
 * @brief Splits `amount` units off the stack the prompt was opened on, onto the cursor.
 *
 * FOUR CONTAINERS since 2026-09-22 (user: "shift+right click to move part of a stack should work
 * for stacks in every tab in stash and in inventory grid"): the backpack's first page, its nine
 * extra pages, the belt and the stash. It was the first page and the belt, because it indexed
 * straight into InvList - which is why a split in an extra tab took a slice out of whatever
 * happened to sit at that index in page one, and a split in the stash was never wired at all.
 *
 * Every container's own remover is used rather than a shared one: the backpack compacts its list
 * and fixes up its grid references, the belt resyncs the panel, and the stash walks its page's
 * cells. They are not interchangeable and this is the only place that has to know it.
 */
void RemoveStackSplit(Player &player, int amount)
{
	Item *source = GoldDropSourceItem();
	if (source == nullptr || source->isEmpty() || amount <= 0 || amount > source->stackCount())
		return;

	player.HoldItem = *source;
	player.HoldItem.setStackCount(amount);
	const bool emptied = source->stackCount() - amount <= 0;

	if (GoldDropStashIndex != StashStruct::EmptyCell) {
		if (emptied) {
			Stash.RemoveStashItem(GoldDropStashIndex);
		} else {
			source->setStackCount(source->stackCount() - amount);
		}
		Stash.dirty = true;
		if (&player == MyPlayer)
			oracool::ScheduleAutoSaveForStashChange();
	} else if (GoldDropInvIndex > INVITEM_INV_LAST) {
		const int index = GoldDropInvIndex - INVITEM_BELT_FIRST;
		if (emptied) {
			player.RemoveSpdBarItem(index);
		} else {
			source->setStackCount(source->stackCount() - amount);
			player.CalcScrolls();
			RedrawComponent(PanelDrawComponent::Belt);
			if (&player == MyPlayer)
				NetSendCmdChBeltItem(false, index);
		}
	} else {
		const int index = GoldDropInvIndex - INVITEM_INV_FIRST;
		if (emptied) {
			// The page the prompt was opened on, not the one on screen. RemoveExtraTabItem takes the
			// tab; Player::RemoveInvItem is page one's own and is what tab 0 must still use, because
			// it is the only one that network-syncs.
			if (GoldDropInvTab == 0)
				player.RemoveInvItem(index);
			else
				RemoveExtraTabItem(player, GoldDropInvTab - 1, index);
		} else {
			source->setStackCount(source->stackCount() - amount);
			// No packet format exists for an extra tab - single-player only, as everything else
			// that touches InvTabList already notes.
			if (GoldDropInvTab == 0 && &player == MyPlayer)
				NetSyncInvItem(player, index);
		}
	}

	NewCursor(player.HoldItem);
}

bool IsLevelUpButtonVisible()
{
	// NOT hidden by an open window any more (user, 2026-08-28: "dont hide exp counter and hero stats
	// button when windows are open. leave them on as you leave skill points button on").
	//
	// Vanilla hid it behind chrflag, the store, the stash and the quest log, because in vanilla it
	// sat in the top-left corner where the character sheet and the quest log both open. It does not
	// live there any more: GetLevelUpIconRect anchors it above the LMB skill button, in the middle of
	// the main panel, which no window covers. The skill-point frame beside it (DrawUnspentPointsFrame)
	// never had those gates and has never been in anyone's way, which is the evidence that the gates
	// were about the old position rather than about the button.
	//
	// The two that remain are not about windows: the spell-select overlay draws over this exact
	// spot, and the virtual gamepad has its own controls.
	if (spselflag || MyPlayer->_pStatPts == 0) {
		return false;
	}
	if (ControlMode == ControlTypes::VirtualGamepad) {
		return false;
	}

	return true;
}

} // namespace

bool IsQuestEnabledInThisGame(const Quest &quest)
{
	return IsQuestEnabled(quest);
}

bool IsLevelUpIconShown()
{
	return MyPlayer != nullptr && IsLevelUpButtonVisible();
}

void CalculatePanelAreas()
{
	constexpr Size MainPanelSize { 640, 128 };

	MainPanel = {
		{ (gnScreenWidth - MainPanelSize.width) / 2, gnScreenHeight - MainPanelSize.height },
		MainPanelSize
	};
	LeftPanel = {
		{ 0, 0 },
		SidePanelSize
	};
	RightPanel = {
		{ 0, 0 },
		SidePanelSize
	};

	if (ControlMode == ControlTypes::VirtualGamepad) {
		LeftPanel.position.x = gnScreenWidth / 2 - LeftPanel.size.width;
	} else {
		if (gnScreenWidth - LeftPanel.size.width - RightPanel.size.width > MainPanel.size.width) {
			LeftPanel.position.x = (gnScreenWidth - LeftPanel.size.width - RightPanel.size.width - MainPanel.size.width) / 2;
		}
	}
	LeftPanel.position.y = (gnScreenHeight - LeftPanel.size.height - MainPanel.size.height) / 2;

	if (ControlMode == ControlTypes::VirtualGamepad) {
		RightPanel.position.x = gnScreenWidth / 2;
	} else {
		RightPanel.position.x = gnScreenWidth - RightPanel.size.width - LeftPanel.position.x;
	}
	RightPanel.position.y = LeftPanel.position.y;

	gnViewportHeight = gnScreenHeight;
	if (gnScreenWidth <= MainPanel.size.width) {
		// Part of the screen is fully obscured by the UI
		gnViewportHeight -= MainPanel.size.height;
	}
}

bool IsChatAvailable()
{
#ifdef _DEBUG
	return true;
#else
	return gbIsMultiplayer;
#endif
}

void FocusOnCharInfo()
{
	Player &myPlayer = *MyPlayer;

	if (invflag || myPlayer._pStatPts <= 0)
		return;

	// Find the first incrementable stat.
	int stat = -1;
	for (auto attribute : enum_values<CharacterAttribute>()) {
		const int maximum = 255;
		if (myPlayer.GetBaseAttributeValue(attribute) >= maximum)
			continue;
		stat = static_cast<int>(attribute);
	}
	if (stat == -1)
		return;

	SetCursorPos(ChrBtnsRect[stat].Center());
}

void OpenCharPanel()
{
	// The sheet is roughly twice its window's height now, so opening it should always show the top
	// rather than wherever it was left last time.
	ResetCharacterSheetScroll();
	// Was three hand-listed closes, which missed the waypoint menu and the crafting book. They are
	// below the sheet in precedence so the sheet still won, but they stayed open behind it and
	// reappeared when it closed.
	if (!TakeLeftPanelSlot(LeftPanelContent::Character))
		return;
	chrflag = true;
}

void CloseCharPanel()
{
	// The Advanced Stats window is the sheet's own extension (its button lives on the sheet), so it
	// goes with it - putting back whatever it covered, as its ordinary close does. A no-op when shut.
	oracool::CloseAdvancedStats();
	chrflag = false;
	if (IsInspectingPlayer()) {
		InspectPlayer = MyPlayer;
		RedrawEverything();
		InitDiabloMsg(_("Stopped inspecting players."));
	}
}

void ToggleCharPanel()
{
	if (chrflag)
		CloseCharPanel();
	else
		OpenCharPanel();
}

namespace {

/**
 * @brief Records @p color once per LINE of @p str, keeping InfoStringLineColors parallel to
 * InfoString's lines.
 *
 * Per line, not per call: several callers pass strings with an embedded newline ("Right-click to
 * read, then\nleft-click to target"), so one call can produce two lines. Counting calls instead
 * would slide every colour after the first such string one row up.
 */
void PushLineColors(string_view str, UiFlags color)
{
	size_t lines = 1;
	for (const char c : str) {
		if (c == '\n')
			lines++;
	}
	InfoStringLineColors.insert(InfoStringLineColors.end(), lines, color);
	// The tail array walks in lockstep with the colour array whether or not anybody uses it, so a
	// consumer can index either with the same line number. 0 = no tail, the state every existing
	// caller wants.
	InfoStringLineTailStart.insert(InfoStringLineTailStart.end(), lines, 0);
	InfoStringLineRuns.insert(InfoStringLineRuns.end(), lines, {});
}

/**
 * @brief Records InfoColor for any InfoString lines that were assigned bare, before appending more.
 *
 * Oracool: crash fix (2026-08-15, user screenshot - the assert in cursor_tooltip.cpp fired on a
 * shrine hover). `InfoString = name; AddPanelString(description);` is a pattern vanilla uses all
 * over - GetObjectStr's shrines, the speedbook's selected entry, the player hover - and it was
 * perfectly legal until the per-line-colour tooltip made an unrecorded line an error. The first
 * attempt fixed individual call sites and missed these; this honours the pattern instead: lines
 * that existed before an append are credited with InfoColor, which is exactly the colour the
 * single-colour path would have painted them.
 */
void BackfillLineColors()
{
	if (InfoString.empty())
		return;
	size_t lines = 1;
	for (const char c : InfoString.str()) {
		if (c == '\n')
			lines++;
	}
	if (InfoStringLineColors.size() < lines)
		InfoStringLineColors.insert(InfoStringLineColors.end(), lines - InfoStringLineColors.size(), InfoColor);
	if (InfoStringLineTailStart.size() < lines)
		InfoStringLineTailStart.insert(InfoStringLineTailStart.end(), lines - InfoStringLineTailStart.size(), 0);
	if (InfoStringLineRuns.size() < lines)
		InfoStringLineRuns.insert(InfoStringLineRuns.end(), lines - InfoStringLineRuns.size(), {});
}

} // namespace

void AddPanelString(string_view str)
{
	AddPanelString(str, UiFlags::ColorWhite);
}

void AddPanelString(std::string &&str)
{
	AddPanelString(std::move(str), UiFlags::ColorWhite);
}

void AddPanelString(string_view str, UiFlags color)
{
	BackfillLineColors();
	if (InfoString.empty())
		InfoString.AssignKeepingLineColors(str);
	else
		InfoString.AssignKeepingLineColors(StrCat(InfoString.str(), "\n", str));
	PushLineColors(str, color);
}

void AddPanelString(std::string &&str, UiFlags color)
{
	BackfillLineColors();
	PushLineColors(str, color);
	if (InfoString.empty())
		InfoString.AssignKeepingLineColors(std::move(str));
	else
		InfoString.AssignKeepingLineColors(StrCat(InfoString.str(), "\n", str));
}

void AddPanelStringSplit(std::string &&str, UiFlags color, size_t tailStart)
{
	// The split only makes sense on a single line, and every caller passes one; a multi-line string
	// would give its tail offset to the first row and leave the rest bare, which is worse than
	// ignoring it. Clamped rather than asserted because a translation could legitimately shorten
	// the string past the offset the caller computed from the untranslated form.
	const size_t clamped = std::min(tailStart, str.size());
	AddPanelString(std::move(str), color);
	if (!InfoStringLineTailStart.empty())
		InfoStringLineTailStart.back() = static_cast<uint16_t>(clamped);
}

void AddPanelStringRuns(std::string &&str, UiFlags color, std::vector<PanelLineRun> runs)
{
	// One line only, like the split; runs past the end are dropped rather than asserted, for the
	// same translation reason.
	const size_t size = str.size();
	AddPanelString(std::move(str), color);
	if (InfoStringLineRuns.empty())
		return;
	std::vector<PanelLineRun> kept;
	for (const PanelLineRun &run : runs) {
		if (run.start < size)
			kept.push_back(run);
	}
	InfoStringLineRuns.back() = std::move(kept);
}

void SetPanelString(StringOrView str, UiFlags color)
{
	InfoString.AssignKeepingLineColors(std::move(str));
	InfoColor = color;
	InfoStringLineColors.clear();
	InfoStringLineTailStart.clear();
	InfoStringLineRuns.clear();
	PushLineColors(InfoString.str(), color);
}

void ClearPanelStrings()
{
	InfoString.AssignKeepingLineColors(StringOrView {});
	InfoStringLineColors.clear();
	InfoStringLineTailStart.clear();
	InfoStringLineRuns.clear();
}

Point GetPanelPosition(UiPanels panel, Point offset)
{
	Displacement displacement { offset.x, offset.y };

	switch (panel) {
	case UiPanels::Main:
		return GetMainPanel().position + displacement;
	case UiPanels::Character:
		// Oracool V1: the character sheet has its own 340x720 top-left rect, matching the waypoint
		// list and quest log. This returns its CONTENT origin, not the panel's, so every caller -
		// the draw, the stat buttons and their hit-testing - moves as one.
		return GetCharacterContentOrigin() + displacement;
	case UiPanels::Quest:
	case UiPanels::Stash:
		// Oracool V1: the stash has its own 340x720 top-left rect - see qol/stash.h.
		return GetStashPanelRect().position + displacement;
	case UiPanels::Inventory:
		// Oracool V1: the inventory has its own 320x660 top-right rect and no longer shares
		// RightPanel with the spellbook - see oracool/inventory_layout.h.
		return oracool::GetInventoryPanelRect().position + displacement;
	case UiPanels::Spell:
		// Oracool V1: the book has its own 340x720 top-right rect, like the inventory - see
		// panels/spell_book.hpp.
		return GetSpellBookPanelRect().position + displacement;
	default:
		return GetMainPanel().position + displacement;
	}
}

void DrawPanelBox(const Surface &out, SDL_Rect srcRect, Point targetPosition)
{
	out.BlitFrom(*pBtmBuff, srcRect, targetPosition);
}

/**
 * @brief Draws "current/max" with the SLASH centred on @p pos.
 *
 * Oracool: user request - "make the health and mana points slash symbol to be dead center in the
 * orbs". @p pos is the orb's sphere centre (scrollrt passes GetHealthOrbSphereCenterLocal and its
 * mana twin), and the slash is the right thing to centre on it rather than the string as a whole:
 * the two numbers either side change width independently - 9/10 against 100/100 - so centring the
 * whole line would let the divider wander off the middle as the values moved.
 *
 * It was off on both axes. Horizontally the slash's LEFT EDGE sat on the centre, putting the glyph
 * half its own width to the right. Vertically `pos` was passed straight to the Point overload of
 * DrawString, whose y is the TOP of the line, so the whole readout hung a half line-height below the
 * middle of the sphere. Both are corrected here rather than by nudging the numbers scrollrt passes,
 * so anything else that ever draws a pair of values gets the same centring.
 */
void DrawFlaskValues(const Surface &out, Point pos, int currValue, int maxValue)
{
	DrawFlaskValuesInColor(out, pos, currValue, maxValue, currValue > 0 ? (currValue == maxValue ? UiFlags::ColorGold : UiFlags::ColorWhite) : UiFlags::ColorRed);
}

void DrawFlaskValuesInColor(const Surface &out, Point pos, int currValue, int maxValue, UiFlags color)
{
	auto drawStringWithShadow = [out, color](string_view text, Point pos) {
		DrawString(out, text, pos + Displacement { -1, -1 }, { UiFlags::ColorBlack | UiFlags::KerningFitSpacing, 0 });
		DrawString(out, text, pos, { color | UiFlags::KerningFitSpacing, 0 });
	};

	const int slashWidth = GetLineWidth("/", GameFont12);
	const int lineHeight = GetLineHeight("/", GameFont12);
	// The slash's own top-left, such that its centre lands on pos.
	const Point slashPos { pos.x - slashWidth / 2, pos.y - lineHeight / 2 };

	std::string currText = StrCat(currValue);
	drawStringWithShadow(currText, slashPos - Displacement { GetLineWidth(currText, GameFont12) + 1, 0 });
	drawStringWithShadow("/", slashPos);
	drawStringWithShadow(StrCat(maxValue), slashPos + Displacement { slashWidth + 1, 0 });
}

void control_update_life_mana()
{
	MyPlayer->UpdateManaPercentage();
	MyPlayer->UpdateHitPointPercentage();
}

void InitControlPan()
{
	if (!HeadlessMode) {
		// Oracool: HUD art pass - pBtmBuff (the vanilla panel8 image) survives only as the chat
		// panel's backdrop (DrawTalkPan/DrawPanelBox); the flasks it used to feed are gone,
		// replaced by the corner orb compositions (oracool/hud_art.cpp).
		pBtmBuff.emplace(GetMainPanel().size.width, (GetMainPanel().size.height + 16) * (IsChatAvailable() ? 2 : 1));

		LoadCharPanel();
		LoadLargeSpellIcons();
		{
			const OwnedClxSpriteList sprite = LoadCel("ctrlpan\\panel8", GetMainPanel().size.width);
			ClxDraw(*pBtmBuff, { 0, (GetMainPanel().size.height + 16) - 1 }, sprite[0]);
		}
	}
	talkflag = false;
	ChatInputState = std::nullopt;
	if (IsChatAvailable()) {
		if (!HeadlessMode) {
			// Oracool (2026-08-18): ctrlpan\talkpanl still goes into pBtmBuff - other panel drawing
			// reads that buffer - but ctrlpan\talkbutt is no longer loaded. It held the VOICE button
			// frames, and nothing draws them since the chat box became a plain bordered rectangle.
			const OwnedClxSpriteList sprite = LoadCel("ctrlpan\\talkpanl", GetMainPanel().size.width);
			ClxDraw(*pBtmBuff, { 0, (GetMainPanel().size.height + 16) * 2 - 1 }, sprite[0]);
		}
		sgbPlrTalkTbl = 0;
		TalkMessage[0] = '\0';
		for (bool &whisper : WhisperList)
			whisper = true;
	}
	panelflag = false;
	lvlbtndown = false;
	if (!HeadlessMode) {
		LoadMainPanel();

		static const uint16_t CharButtonsFrameWidths[9] { 95, 41, 41, 41, 41, 41, 41, 41, 41 };
		pChrButtons = LoadCel("data\\charbut", CharButtonsFrameWidths);
	}
	if (!HeadlessMode)
		pDurIcons = LoadCel("items\\duricons", 32);
	for (bool &buttonEnabled : chrbtn)
		buttonEnabled = false;
	for (bool &buttonEnabled : chrDecBtn)
		buttonEnabled = false;
	chrbtnactive = false;
	oracool::ClearCombatOdds(); // the sheet's odds bars start blank for every character (combat_odds.h)
	ClearPanelStrings();
	RedrawComponent(PanelDrawComponent::Health);
	RedrawComponent(PanelDrawComponent::Mana);
	// A new game starts with the Advanced Stats window shut and nothing remembered under it - its
	// state is file-local and would otherwise reach the next character. First, with restoring off, so
	// CloseCharPanel's own close below has nothing left to put back.
	oracool::CloseAdvancedStats(/*restoreCovered=*/false);
	CloseCharPanel();
	spselflag = false;
	// sbooktab is gone with the book's six tabs - it is one scrolling list now, and its scroll
	// position is reset by ResetSpellBookScroll() at every open rather than once at game start.
	ResetSpellBookScroll();
	sbookflag = false;

	if (!HeadlessMode) {
		InitSpellBook();
		pQLogCel = LoadCel("data\\quest", static_cast<uint16_t>(SidePanelSize.width));
		pGBoxBuff = LoadCel("ctrlpan\\golddrop", 261);
	}
	CloseGoldDrop();
	CalculatePanelAreas();

	if (!HeadlessMode)
		InitModifierHints();
}

// Oracool: HUD overhaul - the old panel background blit (DrawCtrlPan), the 8 panel buttons
// (DrawCtrlBtns/ClearPanBtn/CheckBtnUp/control_check_btn_press and their PanelButtons/PanBtnPos
// state), and the fixed info box are all gone. Their actions live on in the belt's Menu popup
// (oracool/hud_menu.cpp) and the cursor tooltip (oracool/cursor_tooltip.cpp). DoPanBtn below kept
// its name but now only handles the one click target left on the main panel that isn't the belt:
// the RMB skill button (readied-spell/speedbook slot).
void DoPanBtn()
{
	if (!spselflag && oracool::GetRmbSkillButtonRect().contains(MousePosition)) {
		oracool::PressHudWell(/*leftWell=*/false); // the icon sinks until the release (2026-09-20)
		if ((SDL_GetModState() & KMOD_SHIFT) != 0) {
			ClearReadiedSpell(*MyPlayer);
			// Same omission the skill picker had (audit, 2026-08-26): the shortcut that CLEARS the
			// button was the one route to a readied-skill change that never asked to be saved.
			oracool::ScheduleAutoSaveForSkillChange();
			oracool::PlayUiMoveSound();
			return;
		}
		// The quick list, not the Abilities window (user, 2026-08-18): clicking a well is how the
		// basic attack goes onto that button. The Abilities window keeps the S key and the burger
		// menu, which is where everything that has to be EARNED is chosen.
		oracool::OpenSkillPicker(/*forLeftButton=*/false);
		oracool::PlayUiMoveSound();
		gamemenu_off();
	}
}

void DoAutoMap()
{
	// Oracool: back to vanilla's plain on/off toggle - the mini-map is no longer part of this
	// cycle at all, it's an independent always-on overlay controlled solely by the Mini-Map
	// option (see DrawView, scrollrt.cpp) and simply hides itself while this full map is active.
	if (!AutomapActive) {
		StartAutomap();
	} else {
		AutomapActive = false;
	}
}

void CheckPanelInfo()
{
	panelflag = false;
	// Through ClearPanelStrings, not a bare assignment: this runs at the top of the hover pass
	// every frame, so it is the point that guarantees last frame's per-line colours cannot survive
	// into this one's text.
	ClearPanelStrings();

	// Oracool: the burger menu's icon row names whichever icon is hovered - the icons are
	// wordless, so this is the only thing telling the player what each one does.
	const int hoveredMenuIcon = oracool::HitTestHudMenuIcon(MousePosition);
	if (hoveredMenuIcon >= 0) {
		InfoString = oracool::GetHudMenuEntryLabel(hoveredMenuIcon);
		InfoColor = UiFlags::ColorWhite;
		oracool::ShowPanelStringsAsHintCard(); // the vendors' gold card on every HUD control (dev note, 2026-09-28)
		panelflag = true;
		return;
	}

	// Oracool: user request (2026-08-11) - hover hints for the HUD's own controls. These three
	// carry no affordance of their own: the Menu and Portal cells are painted into the plate art,
	// and the XP counter is bare text, so nothing about them says "clickable" without this.
	if (oracool::GetBeltSlotRect(oracool::BeltMenuSlotIndex).contains(MousePosition)) {
		SetPanelString(_("Menu Bar"), UiFlags::ColorWhite);
		AddPanelString(_("Click to open/close"));
		InfoColor = UiFlags::ColorWhite;
		oracool::ShowPanelStringsAsHintCard(); // the vendors' gold card on every HUD control (dev note, 2026-09-28)
		panelflag = true;
		return;
	}
	if (oracool::GetBeltSlotRect(oracool::BeltTownPortalSlotIndex).contains(MousePosition)) {
		SetPanelString(_("Town Portal"), UiFlags::ColorWhite);
		AddPanelString(_("Click to open."));
		InfoColor = UiFlags::ColorWhite;
		oracool::ShowPanelStringsAsHintCard(); // the vendors' gold card on every HUD control (dev note, 2026-09-28)
		panelflag = true;
		return;
	}
	if (oracool::GetBeltSlotRect(oracool::BeltRunToggleSlotIndex).contains(MousePosition)) {
		// Names the state it is in, then what one step of it costs, then what a click does - the
		// glyph already shows the first, and the hint is where the key binding gets mentioned at all.
		const bool running = oracool::IsRunEnabled();
		SetPanelString(running ? _("Running") : _("Walking"), UiFlags::ColorWhite);

		// What a step ACTUALLY costs right now, derived the way StartWalkAnimation derives it rather
		// than quoting the plain-walk constants: the walk animation is 8 frames and its length is
		// 8 less the skipped frames, the run floors that skip at 2, and a plain walk floors it at -2.
		// Movement Speed from items, Vigor and slows moves the number, so a fixed "10 ticks" would be
		// wrong for most characters (user, 2026-09-12: "add elabotation text saying how many frames
		// or ticks is one move").
		//
		// The carry is LOCAL and discarded. StrideTicksFor consumes it to spread a fractional tick
		// across successive strides, and a hover that spent the player's carry would make merely
		// looking at the button alter the next step.
		int hoverCarry = 0;
		const int strideTicks = oracool::StrideTicksFor(oracool::MovementSpeedPercent(*MyPlayer), hoverCarry);
		// The feet's own branches (StartWalkAnimation): no -2 floor on the walk since 2026-09-27, and the slowed run
		// (round 20 audit, v1.12.245).
		const int walkSkip = 8 - strideTicks;
		const int skippedFrames = !running                              ? walkSkip
		    : oracool::PlayerSlowPercent(*MyPlayer) > 0 ? std::min(walkSkip + 4, std::max(2, walkSkip))
		                                                : std::max(2, walkSkip);
		const int stepTicks = 8 - skippedFrames;
		AddPanelString(fmt::format(fmt::runtime(_("One step: {:d} ticks, 8 frames")), stepTicks));
		AddPanelString(fmt::format(fmt::runtime(_("Movement speed {:d}%")),
		    oracool::MovementSpeedPercent(*MyPlayer)));
		AddPanelString(_("Click to switch. Also the R key."));
		InfoColor = UiFlags::ColorWhite;
		oracool::ShowPanelStringsAsHintCard(); // the vendors' gold card on every HUD control (dev note, 2026-09-28)
		panelflag = true;
		return;
	}
	// The XP bar's hover text ("Experience Meter / Click for more.") is gone (user, 2026-09-05:
	// "remove the pop-up message when hovering"). The bar still ends the search here, so nothing
	// under it names itself; the counter it reveals is its own explanation.
	if (oracool::IsPointOverXpCounter(MousePosition))
		return;

	// Oracool: the LMB well now holds the basic attack rather than nothing, so it has something to
	// say. It is also the plate's other opener for the Abilities window (diablo.cpp's LeftMouseDown),
	// which was previously findable only by clicking the empty socket and seeing what happened.
	if (oracool::GetLmbSkillButtonRect().contains(MousePosition)) {
		// What is actually ON the button (user, 2026-08-19: "when i assign a skill to LMB it lands but
		// when i hover over LMB popup says Regular Attack"). This reported the basic attack
		// unconditionally, from back when the left button could hold nothing else; the well has drawn
		// the readied skill for versions, so the hover was the last place still saying otherwise.
		const Player &myPlayer = *MyPlayer;
		const SpellID leftSpell = myPlayer._pLRSpell;
		if (IsValidSpell(leftSpell)) {
			SetPanelString(oracool::GetSpellDisplayName(leftSpell), UiFlags::ColorWhite);
			if (myPlayer._pLRSplType == SpellType::Spell) {
				const int spellLevel = myPlayer.GetSpellLevel(leftSpell);
				AddPanelString(spellLevel == 0 ? _("Spell Level 0 - Unusable")
				                               : fmt::format(fmt::runtime(_("Spell Level {:d}")), spellLevel));
			}
			AddPanelString(_("Left click to use"));
		} else {
			SetPanelString(_(oracool::AttackIconName(oracool::BasicAttackIcon(myPlayer))), UiFlags::ColorWhite);
			AddPanelString(_("Left click to attack"));
		}
		AddPanelString(_("Click here for abilities"));
		InfoColor = UiFlags::ColorWhite;
		oracool::ShowPanelStringsAsHintCard(); // the vendors' gold card on every HUD control (dev note, 2026-09-28)
		panelflag = true;
		return;
	}

	if (!spselflag && oracool::GetRmbSkillButtonRect().contains(MousePosition)) {
		SetPanelString(_("Select current spell button"), UiFlags::ColorWhite);
		InfoColor = UiFlags::ColorWhite;
		panelflag = true;
		// No "Hotkey: 's'" line (user, 2026-09-05: "hot key of either of the three to be omitted in
		// hover descriptions of skills/spells/auras") - the key is the player's to set in Keymapping.
		Player &myPlayer = *MyPlayer;
		const SpellID spellId = myPlayer._pRSpell;
		if (const oracool::ClassTreeSkill aura = oracool::GetActiveClassAura(myPlayer);
		    aura != oracool::ClassTreeSkill::None) {
			// A burning aura IS this button's setting - it clears any readied spell, so it is checked
			// first (user, 2026-08-19: "i can successfully assign them but when i hover over RMB it
			// says Regular Attack"). It carries no SpellID, so nothing below could ever name it.
			AddPanelString(fmt::format(fmt::runtime(_("{:s} Aura")),
			    _(oracool::GetClassTreeSkillData(aura).name)));
			AddPanelString(_("Burning"));
		} else if (!IsValidSpell(spellId)) {
			// Nothing readied - which is the basic attack, and the icon in the well says so. Name it
			// here too, so hovering never reports an empty slot for a slot that does something.
			AddPanelString(_(oracool::AttackIconName(oracool::BasicAttackIcon(myPlayer))));
		} else {
			switch (myPlayer._pRSplType) {
			case SpellType::Skill:
				AddPanelString(fmt::format(fmt::runtime(_("{:s} Skill")), oracool::GetSpellDisplayName(spellId)));
				break;
			case SpellType::Spell: {
				AddPanelString(fmt::format(fmt::runtime(_("{:s} Spell")), oracool::GetSpellDisplayName(spellId)));
				const int spellLevel = myPlayer.GetSpellLevel(spellId);
				AddPanelString(spellLevel == 0 ? _("Spell Level 0 - Unusable") : fmt::format(fmt::runtime(_("Spell Level {:d}")), spellLevel));
			} break;
			case SpellType::Scroll: {
				AddPanelString(fmt::format(fmt::runtime(_("Scroll of {:s}")), oracool::GetSpellDisplayName(spellId)));
				const InventoryAndBeltPlayerItemsRange items { myPlayer };
				const int scrollCount = std::count_if(items.begin(), items.end(), [spellId](const Item &item) {
					return item.isScrollOf(spellId);
				});
				AddPanelString(fmt::format(fmt::runtime(ngettext("{:d} Scroll", "{:d} Scrolls", scrollCount)), scrollCount));
			} break;
			case SpellType::Charges:
				AddPanelString(fmt::format(fmt::runtime(_("Staff of {:s}")), oracool::GetSpellDisplayName(spellId)));
				AddPanelString(fmt::format(fmt::runtime(ngettext("{:d} Charge", "{:d} Charges", myPlayer.InvBody[INVLOC_HAND_LEFT]._iCharges)), myPlayer.InvBody[INVLOC_HAND_LEFT]._iCharges));
				break;
			case SpellType::Invalid:
				break;
			}
		}
		// RETURN, exactly as the LMB well above does (user, 2026-08-28: "show a tooltip when
		// hovering over rmb slot showing which is the skill loaded in that slot").
		//
		// The tooltip was already being built - every line of it, naming the aura, the skill, the
		// spell and its level, the scroll count, the staff charges. It was then thrown away one
		// statement later: the RMB well is inside GetMiddleHudRect, so the belt hover pass below
		// ran over the same pixel, and CheckInvHLight opens by calling ClearPanelStrings. The LMB
		// well returns and so keeps its tooltip; this one fell through and lost it every frame.
		//
		// Which is why the report reads as "there is no tooltip" rather than "the tooltip is wrong".
		oracool::ShowPanelStringsAsHintCard(); // the vendors' gold card on every HUD control (dev note, 2026-09-28)
		panelflag = true;
		return;
	}
	if (oracool::GetMiddleHudRect().contains(MousePosition))
		pcursinvitem = CheckInvHLight();

	if (CheckXPBarInfo()) {
		panelflag = true;
	}
}

void FreeControlPan()
{
	pBtmBuff = std::nullopt;
	FreeLargeSpellIcons();
	FreeSpellBook();
	pChrButtons = std::nullopt;
	pDurIcons = std::nullopt;
	pQLogCel = std::nullopt;
	pGBoxBuff = std::nullopt;
	FreeMainPanel();
	FreeCharPanel();
	FreeModifierHints();
}

void UpdateInfoString()
{
	// The shop grid answers first and stops. Its panel covers the world, so every other producer
	// below is describing something the player cannot see or reach while it is open.
	if (oracool::SetShopHoverInfoString())
		return;

	// Levski's grid, on the same terms and for the same reason (user, 2026-09-03). Its window floats
	// over the world, so while the cursor is on one of its items nothing behind it is hoverable -
	// and the producers below would otherwise describe whatever is under the window.
	if (oracool::SetWorkshopHoverInfoString())
		return;
	if (oracool::SetLevskiHoverInfoString())
		return;
	// The hero sheet's Armor class and To hit boxes name the monster their odds bars measure against (2026-09-27).
	if (SetCharacterSheetHoverInfoString())
		return;

	if (!panelflag && !trigflag && pcursinvitem == -1 && pcursstashitem == StashStruct::EmptyCell && !ActiveTabItemHovered && !spselflag) {
		ClearPanelStrings();
		InfoColor = UiFlags::ColorWhite;
	}
	Player &myPlayer = *MyPlayer;
	if (spselflag || trigflag) {
		InfoColor = UiFlags::ColorWhite;
	} else if (!myPlayer.HoldItem.isEmpty()) {
		// All three through SetPanelString (user, 2026-09-03: the assert fired while carrying an
		// unsocketed ring back to the inventory). The held-item branch runs while the cursor is over
		// an inventory or stash slot, which is exactly when the block above does NOT clear - so these
		// two one-liners were inheriting the colour list of whatever multi-line item the player had
		// been hovering a moment earlier.
		//
		// The assignment operator now clears that list by itself, so these are no longer load-bearing
		// - but they are what the rule says to write, and the third repeat of this bug is a poor
		// argument for leaving two more examples of the pattern that caused it.
		if (myPlayer.HoldItem._itype == ItemType::Gold) {
			int nGold = myPlayer.HoldItem._ivalue;
			SetPanelString(fmt::format(fmt::runtime(ngettext("{:s} gold piece", "{:s} gold pieces", nGold)), FormatInteger(nGold)), UiFlags::ColorWhite);
		} else if (!myPlayer.CanUseItem(myPlayer.HoldItem)) {
			SetPanelString(_("Requirements not met"), UiFlags::ColorRed);
		} else {
			// One line, so the colour list is trivially in step - but through SetPanelString
			// anyway, so that "an item's name is set this way" holds without exception.
			SetPanelString(myPlayer.HoldItem.getName(), myPlayer.HoldItem.getTextColor());
		}
	} else {
		// Oracool: user request - with item labels on, every item on the floor is already named
		// where it lies, so the hover panel over one of them is the same information a second time
		// ("i already see what an item is"). Ground items only: labels do not cover the inventory,
		// the stash or the belt, which all keep their panel.
		//
		// Consequence worth knowing: a magic or unique item on the floor now shows only its name
		// until it is picked up, since the label carries the name and nothing else.
		if (pcursitem != -1) {
			if (!IsHighlightingLabelsEnabled())
				GetItemStr(Items[pcursitem]);
		} else if (ObjectUnderCursor != nullptr)
			GetObjectStr(*ObjectUnderCursor);
		if (pcursmonst != -1) {
			if (leveltype != DTYPE_TOWN) {
				// Oracool: user request - monsters no longer populate the cursor tooltip. The
				// name, which is all of this the player actually needed, is already on the
				// health bar across the top of the screen the moment a monster is hovered, and
				// having the same name plus a type/kill-count readout follow the cursor around
				// was pure distraction in the middle of a fight.
				//
				// This branch was the only caller of monster.cpp's PrintMonstHistory and
				// PrintUniqueHistory, which have since been deleted. Restoring monster hover text
				// means writing them again, not just re-adding a call.
			} else if (pcursitem == -1) {
				InfoString = string_view(Towners[pcursmonst].name);
			}
		}
		if (pcursplr != -1) {
			InfoColor = UiFlags::ColorWhitegold;
			auto &target = Players[pcursplr];
			InfoString = string_view(target._pName);
			AddPanelString(fmt::format(fmt::runtime(_("{:s}, Level: {:d}")), _(PlayersData[static_cast<std::size_t>(target._pClass)].className), target._pLevel));
			AddPanelString(fmt::format(fmt::runtime(_("Hit Points {:d} of {:d}")), target._pHitPoints >> 6, target._pMaxHP >> 6));
		}
	}
}

void CheckLvlBtn()
{
	if (!IsLevelUpButtonVisible()) {
		return;
	}

	// Oracool: the indicator moved to under the clock, so both hit-tests read the one rect that
	// also drives the drawing - the old hardcoded offsets against the main panel had already drifted
	// once, which is exactly what a shared rect prevents.
	if (!lvlbtndown && oracool::GetLevelUpIconRect().contains(MousePosition))
		lvlbtndown = true;
}

void ReleaseLvlBtn()
{
	if (oracool::GetLevelUpIconRect().contains(MousePosition)) {
		OpenCharPanel();
	}
	lvlbtndown = false;
}

/**
 * Oracool: user request - the level-up indicator moved from beside the old bottom panel to under
 * the game clock in the top-left, and got its own artwork instead of borrowing frame 1/2 of the
 * character sheet's "+" button. The label sits beneath the icon in gold.
 */
/**
 * @brief The numbered points frame - the delivered art with @p count painted into its well.
 *
 * Shared by BOTH pools since 2026-08-27 (user: "i like the icon that pops up when skill points are
 * available. use it also for stat points instead of the + icon. Now it will show how many stat
 * points are available for distribution"). The stat pool used to show the vanilla level-up plus,
 * which said only THAT there was something to spend; this says how much.
 *
 * @return False if the art is missing, so the caller can draw its own fallback.
 */
bool DrawPointsFrame(const Surface &out, Rectangle frame, int count, bool lit)
{
	if (!oracool::DrawUnspentPointsIcon(out, frame.position, count, lit))
		return false;
	// Clamped at 99 because that is the number the user asked to see, and because three digits do
	// not fit the well the art gives us.
	const int shown = std::min<int>(count, 99);
	// Red, outlined (user, 2026-08-20). The engine has no bold weight - the font ships as five
	// fixed sizes and size IS the weight - so Outlined is what stands in for one: it thickens each
	// glyph with a dark edge, which also stops red-on-near-black from sinking into the frame's own
	// dark well. FontSize42 would be genuinely heavier and would clip a 39px band.
	DrawString(out, StrCat(shown), oracool::SkillPointsNumberRect(frame.position),
	    { UiFlags::ColorRed | UiFlags::Outlined | UiFlags::FontSize30 | UiFlags::AlignCenter
	        | UiFlags::VerticalCenter });
	return true;
}

bool IsUnspentPointsFrameVisible()
{
	return !IsInspectingPlayer() && MyPlayer->_pUnspentSkillPoints > 0;
}

Rectangle GetUnspentPointsFrameRect()
{
	const Rectangle rmb = oracool::GetRmbSkillButtonRect();
	constexpr int GapAboveWell = 6;
	// The numbered icons' own 64px canvas - the same canvas the user's level-up art sits on, which
	// is what "as big as the level up icon" means in practice (LevelUpIconSize's 60x61 is that art
	// minus the canvas's edge padding).
	return { { rmb.position.x + (rmb.size.width - oracool::PointsIconSize.width) / 2,
		         rmb.position.y - oracool::PointsIconSize.height - GapAboveWell },
		oracool::PointsIconSize };
}

bool CheckUnspentPointsFrameClick(Point position)
{
	// Clicking the pool opens the window where the points are spent (user, 2026-08-30: "clicking
	// skill points button to open abilities screen"). It was a readout with no click at all.
	//
	// The rect is shared with the draw rather than restated, which is what stops the hit box from
	// drifting off the picture the way the burger menu's once did.
	if (!IsUnspentPointsFrameVisible() || !GetUnspentPointsFrameRect().contains(position))
		return false;
	ToggleAbilitiesWindow();
	// Here, not in ToggleAbilitiesWindow: the burger menu's entry reaches the same toggle and has
	// already sounded by the time it returns.
	if (sbookflag)
		oracool::PlayUiSelectSound();
	else
		oracool::PlayUiMoveSound();
	return true;
}

void DrawUnspentPointsFrame(const Surface &out)
{
	// The unspent skill pool, above the RMB well (user, 2026-08-17: "Available skill point to go in
	// a placeholder frame above the rmb button, the size of Level Up button"). This replaced the
	// "Points: N" line in the Abilities window's nav row, which existed only while that window was
	// open - the pool is a standing prompt and belongs on the HUD.
	//
	// The art has arrived (ui\skill_points.png, drawn by DrawPointsFrame below). The themed fill and
	// border that stood in for it are now only the fallback; they were sized exactly as the promised
	// picture (LevelUpIconSize), so the swap was a draw-call change and no geometry moved.
	if (!IsUnspentPointsFrameVisible())
		return;
	const Rectangle frame = GetUnspentPointsFrameRect();
	// The delivered frame, with the count drawn into its well (user, 2026-08-20: "replace the
	// current skill point indicator above rmb... in its center area in 40x39px area dead center in
	// the icon i want you to draw with your own font the number of skill point available up to 99").
	//
	// Two draws rather than one picture per value: the old art was a pair of 99-frame strips with
	// the numeral baked in, which could not have shown a hundredth point and had to be recut every
	// time the cap moved. Clamped at 99 here because that is the number the user asked to see, and
	// because three digits do not fit the well the art gives us.
	if (DrawPointsFrame(out, frame, MyPlayer->_pUnspentSkillPoints, frame.contains(MousePosition)))
		return;
	// The pre-art placeholder, kept only as the fallback for a build without ui\skill_points.png.
	DrawHalfTransparentRectTo(out, frame.position.x, frame.position.y, frame.size.width, frame.size.height);
	DrawHalfTransparentRectTo(out, frame.position.x, frame.position.y, frame.size.width, frame.size.height);
	UnsafeDrawBorder2px(out, frame, oracool::ThemeEdgeColor);
	// Same treatment as the real path above, so a build running without the art still shows the
	// count the way the art build does.
	DrawString(out, StrCat(std::min<int>(MyPlayer->_pUnspentSkillPoints, 99)), frame,
	    { UiFlags::ColorRed | UiFlags::Outlined | UiFlags::FontSize30 | UiFlags::AlignCenter
	        | UiFlags::VerticalCenter });
}

void DrawLevelUpIcon(const Surface &out)
{
	if (!IsLevelUpButtonVisible())
		return;

	const Rectangle rect = oracool::GetLevelUpIconRect();

	int state = 0;
	if (lvlbtndown)
		state = 2;
	else if (rect.contains(MousePosition))
		state = 1;

	// The same numbered frame the skill pool wears, carrying the STAT count (user, 2026-08-27). The
	// vanilla plus said only that something was waiting; the number says how much, which is the
	// difference between a reminder and an answer.
	if (DrawPointsFrame(out, rect, MyPlayer->_pStatPts, state != 0))
		return;

	// The vanilla level-up art, demoted to the fallback for a build whose points frame is missing.
	// Centred in the rect, which is now the frame's 64px rather than this art's own 60x61.
	// No label. The icon carries the meaning on its own, and the words competed with the clock
	// directly above them for the same small corner.
	oracool::DrawLevelUpIconArt(out, state);
}

void CheckChrBtns()
{
	Player &myPlayer = *MyPlayer;

	if (chrbtnactive)
		return;

	// Oracool V1: the sheet scrolls, so a + or RESET button can be sitting outside the window -
	// scrolled up under the title band, say. Their rects move with the scroll (charpanel.cpp's
	// PlaceWidgets), which keeps the visible ones correct, but a rect that has moved off the top
	// would still contain a click on the title. Gating every press on the scrolling area closes
	// that: a point outside it can never be inside a rect that is also outside it.
	if (!GetCharacterContentRect().contains(MousePosition))
		return;

	// The grouped sheet's ADVANCED STATS button (2026-09-26) - it presses here and acts on the release
	// through ReleaseCharacterSheetAdvancedButton in LeftMouseUp. False on the list sheet.
	if (PressCharacterSheetAdvancedButton(MousePosition))
		return;

	// The list sheet only: the grouped sheet has no RESET since 2026-09-27 ("remove the reset button. the arrow buttons
	// made it obsolete") - its left-pointing triangles take points back one stat at a time.
	if (*sgOptions.Oracool.resetStatsButton && !gbIsMultiplayer && !*sgOptions.Oracool.heroSheetGrouped) {
		// GetResetStatsButtonSize, not ResetStatsButtonSize: one place answers the button's size.
		Rectangle resetButton { GetPanelPosition(UiPanels::Character, GetResetStatsButtonPosition()), GetResetStatsButtonSize() };
		if (resetButton.contains(MousePosition)) {
			resetStatsButtonDown = true;
			chrbtnactive = true;
			// The list's word-label RESET is silent at the press; the armour-drop clunk sounds on the release.
			return;
		}
	}

	// The grouped sheet's triangles (user, 2026-09-26: "They are always active alowing fine tuning stats at all
	// times"): both press whenever they are clicked - with no point to spend or none to take back they sink,
	// click and do nothing - so they never appear and vanish with the points. Single player, like RESET.
	if (*sgOptions.Oracool.heroSheetGrouped) {
		if (gbIsMultiplayer)
			return;
		for (auto attribute : enum_values<CharacterAttribute>()) {
			const auto buttonId = static_cast<size_t>(attribute);
			const Rectangle decrease { GetPanelPosition(UiPanels::Character, ChrDecBtnsRect[buttonId].position), ChrDecBtnsRect[buttonId].size };
			const Rectangle increase { GetPanelPosition(UiPanels::Character, ChrBtnsRect[buttonId].position), ChrBtnsRect[buttonId].size };
			bool *pressed = decrease.contains(MousePosition) ? &chrDecBtn[buttonId] : increase.contains(MousePosition) ? &chrbtn[buttonId] : nullptr;
			if (pressed != nullptr) {
				*pressed = true;
				chrbtnactive = true;
				oracool::PlayUiMoveSound();
				return;
			}
		}
		return;
	}

	if (myPlayer._pStatPts == 0)
		return;

	for (auto attribute : enum_values<CharacterAttribute>()) {
		const int maximum = 255;
		if (myPlayer.GetBaseAttributeValue(attribute) >= maximum)
			continue;
		auto buttonId = static_cast<size_t>(attribute);
		Rectangle button = ChrBtnsRect[buttonId];
		button.position = GetPanelPosition(UiPanels::Character, button.position);
		if (button.contains(MousePosition)) {
			chrbtn[buttonId] = true;
			chrbtnactive = true;
		}
	}
}

void ReleaseChrBtns(bool addAllStatPoints, bool addFive)
{
	chrbtnactive = false;
	// Same scroll gate as CheckChrBtns - a button that scrolled out from under the cursor between
	// press and release must not still act on the release. Nor on a sheet closed in between (audit, 2026-09-27): press
	// -, close the sheet with C or Escape still holding it, release - and a stat came back from a sheet no longer there.
	if (!chrflag || !GetCharacterContentRect().contains(MousePosition)) {
		resetStatsButtonDown = false;
		for (bool &pressed : chrbtn)
			pressed = false;
		for (bool &pressed : chrDecBtn)
			pressed = false;
		return;
	}
	// 1 point a click, 5 with ctrl and 10 with shift, either way - PointsPerClick, the rule every point button shares
	// (user, 2026-09-27). The list sheet's + used to spend every point on shift; aligned with the rest the same day.
	for (auto attribute : enum_values<CharacterAttribute>()) {
		const auto buttonId = static_cast<size_t>(attribute);
		if (!chrDecBtn[buttonId])
			continue;
		chrDecBtn[buttonId] = false;
		const Rectangle button { GetPanelPosition(UiPanels::Character, ChrDecBtnsRect[buttonId].position), ChrDecBtnsRect[buttonId].size };
		if (button.contains(MousePosition)) {
			RefundStatPoints(*MyPlayer, attribute, PointsPerClick(addAllStatPoints, addFive));
		}
		return;
	}
	if (resetStatsButtonDown) {
		resetStatsButtonDown = false;
		Rectangle resetButton { GetPanelPosition(UiPanels::Character, GetResetStatsButtonPosition()), GetResetStatsButtonSize() };
		if (resetButton.contains(MousePosition)) {
			ResetPlayerStats(*MyPlayer);
			// Oracool: user request - reuses the armor-drop sound for a satisfying "clunk"
			// confirming the reset actually happened.
			PlaySfxLoc(IS_FHARM, MyPlayer->position.tile);
		}
		return;
	}
	for (auto attribute : enum_values<CharacterAttribute>()) {
		auto buttonId = static_cast<size_t>(attribute);
		if (!chrbtn[buttonId])
			continue;

		chrbtn[buttonId] = false;
		Rectangle button = ChrBtnsRect[buttonId];
		button.position = GetPanelPosition(UiPanels::Character, button.position);
		if (button.contains(MousePosition)) {
			Player &myPlayer = *MyPlayer;
			// Every path through StatPointsToSpend, the plain click included: the grouped + presses with no points to
			// spend, and its "1" used to go through unchecked (user, 2026-09-27 dev note: "stat points just increase
			// negativly").
			const int statPointsToAdd = StatPointsToSpend(myPlayer, attribute, PointsPerClick(addAllStatPoints, addFive));
			if (statPointsToAdd <= 0)
				continue;
			switch (attribute) {
			case CharacterAttribute::Strength:
				NetSendCmdParam1(true, CMD_ADDSTR, statPointsToAdd);
				myPlayer._pStatPts -= statPointsToAdd;
				break;
			case CharacterAttribute::Magic:
				NetSendCmdParam1(true, CMD_ADDMAG, statPointsToAdd);
				myPlayer._pStatPts -= statPointsToAdd;
				break;
			case CharacterAttribute::Dexterity:
				NetSendCmdParam1(true, CMD_ADDDEX, statPointsToAdd);
				myPlayer._pStatPts -= statPointsToAdd;
				break;
			case CharacterAttribute::Vitality:
				NetSendCmdParam1(true, CMD_ADDVIT, statPointsToAdd);
				myPlayer._pStatPts -= statPointsToAdd;
				break;
			}
		}
	}
}

void DrawDurIcon(const Surface &out)
{
	bool hasRoomBetweenPanels = RightPanel.position.x - (LeftPanel.position.x + LeftPanel.size.width) >= 16 + (32 + 8 + 32 + 8 + 32 + 8 + 32) + 16;
	bool hasRoomUnderPanels = MainPanel.position.y - (RightPanel.position.y + RightPanel.size.height) >= 16 + 32 + 16;

	if (!hasRoomBetweenPanels && !hasRoomUnderPanels) {
		if (IsLeftPanelOpen() && IsRightPanelOpen())
			return;
	}

	int x = MainPanel.position.x + MainPanel.size.width - 32 - 16;
	if (!hasRoomUnderPanels) {
		if (IsRightPanelOpen() && MainPanel.position.x + MainPanel.size.width > RightPanel.position.x)
			x -= MainPanel.position.x + MainPanel.size.width - RightPanel.position.x;
	}

	Player &myPlayer = *MyPlayer;
	x = DrawDurIcon4Item(out, myPlayer.InvBody[INVLOC_HEAD], x, 3);
	x = DrawDurIcon4Item(out, myPlayer.InvBody[INVLOC_CHEST], x, 2);
	x = DrawDurIcon4Item(out, myPlayer.InvBody[INVLOC_HAND_LEFT], x, 0);
	DrawDurIcon4Item(out, myPlayer.InvBody[INVLOC_HAND_RIGHT], x, 0);
}

void RedBack(const Surface &out)
{
	if (!out.isIndexed()) {
		// v1.11: the pause tint on a frame of colours. pause.trn is an index map; its effect - the
		// scene drained toward a dim red-grey - is done here in RGB: luminance, three quarters
		// strength, the red kept a little ahead. Hell's lava exemption (indices below 32) has no
		// colour equivalent and is not reproduced.
		for (int y = 0; y < gnViewportHeight; y++) {
			uint32_t *row = out.at<uint32_t>(0, y);
			for (int x = 0; x < gnScreenWidth; x++) {
				const uint32_t c = row[x];
				const uint32_t lum = (((c >> 16) & 0xFF) * 77 + ((c >> 8) & 0xFF) * 150 + (c & 0xFF) * 29) >> 8;
				const uint32_t r = std::min<uint32_t>(lum * 3 / 4 + lum / 8, 255);
				const uint32_t g = lum / 2;
				row[x] = (r << 16) | (g << 8) | g;
			}
		}
		return;
	}
	uint8_t *dst = out.begin();
	uint8_t *tbl = GetPauseTRN();
	for (int h = gnViewportHeight; h != 0; h--, dst += out.pitch() - gnScreenWidth) {
		for (int w = gnScreenWidth; w != 0; w--) {
			if (leveltype != DTYPE_HELL || *dst >= 32)
				*dst = tbl[*dst];
			dst++;
		}
	}
}

namespace {

/**
 * @brief Where the split / gold-drop box is drawn: the point its frame is drawn from (its bottom-left), which used to
 * be fixed at the inventory's (30, 178). Set when the box opens, next to the pointer (dev note, 2026-09-27: "the
 * split stack pop-up window - pop-it close to the cursor").
 */
Point GoldDropBoxAnchor;

/** @brief Offsets inside the box, from GoldDropBoxAnchor - the text and the typed amount, where vanilla put them. */
constexpr Displacement GoldDropTextOffset { 31, 75 - 178 };
constexpr Displacement GoldDropAmountOffset { 37, 128 - 178 };

/**
 * @brief Puts the box just below and right of the pointer, flipped to the other side where it would leave the screen
 * and clamped onto it, and moves the text-input rect (the on-screen keyboard's hint) with it.
 */
void PlaceGoldDropBox()
{
	const int w = pGBoxBuff ? (*pGBoxBuff)[0].width() : 261;
	const int h = pGBoxBuff ? (*pGBoxBuff)[0].height() : 128;
	constexpr int Gap = 12;
	int left = MousePosition.x + Gap;
	int top = MousePosition.y + Gap;
	if (left + w > gnScreenWidth)
		left = MousePosition.x - Gap - w;
	if (top + h > gnScreenHeight)
		top = MousePosition.y - Gap - h;
	left = std::clamp(left, 0, std::max(0, gnScreenWidth - w));
	top = std::clamp(top, 0, std::max(0, gnScreenHeight - h));
	GoldDropBoxAnchor = { left, top + h - 1 };
	const Point input = GoldDropBoxAnchor + GoldDropAmountOffset;
	SDL_Rect rect = MakeSdlRect(input.x, input.y, 180, 20);
	SDL_SetTextInputRect(&rect);
}

} // namespace

void DrawGoldSplit(const Surface &out)
{
	ClxDraw(out, GoldDropBoxAnchor, (*pGBoxBuff)[0]);

	const string_view amountText = GoldDropText;
	const TextInputCursorState &cursor = GoldDropCursor;
	const int max = GoldDropInputState->max();

	const Item *source = GoldDropSourceItem();
	if (source == nullptr)
		return; // the stack went while the prompt was open - Enter will decline for the same reason
	const Item &sourceItem = *source;

	std::string description;
	if (sourceItem._itype == ItemType::Gold) {
		description = fmt::format(
		    fmt::runtime(ngettext(
		        /* TRANSLATORS: {:s} is a number with separators. Dialog is shown when splitting a stash of Gold.*/
		        "You have {:s} gold piece. How many do you want to remove?",
		        "You have {:s} gold pieces. How many do you want to remove?",
		        max)),
		    FormatInteger(max));
	} else {
		description = fmt::format(
		    /* TRANSLATORS: {:s} is a number, {:s} is an item name. Dialog is shown when splitting a stack of consumables. */
		    fmt::runtime(_("You have {:s} {:s}. How many do you want to split off?")),
		    FormatInteger(max), sourceItem.getName().str());
	}

	// Pre-wrap the string at spaces, otherwise DrawString would hard wrap in the middle of words
	const std::string wrapped = WordWrapString(description, 200);

	// The split gold dialog is roughly 4 lines high, but we need at least one line for the player to input an amount.
	// Using a clipping region 50 units high (approx 3 lines with a lineheight of 17) to ensure there is enough room left
	//  for the text entered by the player.
	DrawString(out, wrapped, { GoldDropBoxAnchor + GoldDropTextOffset, { 200, 50 } },
	    { UiFlags::ColorWhitegold | UiFlags::AlignCenter, 1, 17 });

	// Even a ten digit amount of gold only takes up about half a line. There's no need to wrap or clip text here so we
	// use the Point form of DrawString.
	DrawString(out, amountText, GoldDropBoxAnchor + GoldDropAmountOffset,
	    TextRenderOptions {
	        /*flags=*/UiFlags::ColorWhite | UiFlags::PentaCursor,
	        /*spacing=*/1,
	        /*lineHeight=*/-1,
	        /*cursorPosition=*/static_cast<int>(cursor.position),
	        /*highlightRange=*/ { static_cast<int>(cursor.selection.begin), static_cast<int>(cursor.selection.end) },
	    });
}

void control_drop_gold(SDL_Keycode vkey)
{
	Player &myPlayer = *MyPlayer;

	if (myPlayer._pHitPoints >> 6 <= 0) {
		CloseGoldDrop();
		return;
	}

	switch (vkey) {
	case SDLK_RETURN:
	case SDLK_KP_ENTER: {
		const int value = GoldDropInputState->value();
		if (const Item *sourceItem = GoldDropSourceItem(); value != 0 && sourceItem != nullptr) {
			// Gold is still the backpack's alone: RemoveGold indexes InvList directly, and gold
			// never reaches this prompt from anywhere else - StartGoldDrop reads pcursinvitem, which
			// is only ever set on page one, and the stash keeps its gold as a number rather than as
			// an item in a cell.
			if (sourceItem->_itype == ItemType::Gold)
				RemoveGold(myPlayer, GoldDropInvIndex, value);
			else
				RemoveStackSplit(myPlayer, value);
		}
		CloseGoldDrop();
	} break;
	case SDLK_ESCAPE:
		CloseGoldDrop();
		break;
	default:
		break;
	}
}

void DrawTalkPan(const Surface &out)
{
	if (!talkflag)
		return;

	const Point mainPanelPosition = GetMainPanel().position;

	// Oracool (2026-08-18): a plain bordered black box, drawn by us.
	//
	// What was here was six DrawPanelBox blits out of the vanilla ctrlpan\talkpanl composite - a top
	// cap, two tapered bevel loops, the black inset, and a 310x55 lower plate that had three VOICE
	// buttons baked into it by LoadMainPanel. Those are Diablo's multiplayer voice-chat controls,
	// which this single-player fork has no use for, and the panel edging around them belonged to a
	// HUD that no longer exists (user: "there are some remnants of the vanilla hud and some VOICE
	// buttons. To be gone. Leave only the black rectangle which fits 66 zeroes").
	//
	// The text rect below is UNCHANGED and must stay so: DrawString returns how much fitted and
	// ChatInputState->truncate(len) cuts the input to it, so this rect IS the length limit. Its
	// 250x39 at lineHeight 13 is the three rows that hold 66 zeroes.
	const int x = mainPanelPosition.x + 200;
	constexpr int LineHeight = 13;
	constexpr int ChatRows = 3;
	// A /dev note GROWS the box upward, a row ahead of the text (user, 2026-09-24 dev note: "dev
	// notes are too short for me to express myself"). The font wraps per character, so the typed
	// width over the box width is the rows used; one spare row is always open, so typing never hits
	// the truncate below until the cap. The bottom edge stays where the three-row box's is.
	constexpr int DevNoteMaxRows = 18;
	int rows = ChatRows;
	if (string_view(TalkMessage).substr(0, 4) == "/dev")
		rows = std::clamp(GetLineWidth(TalkMessage, GameFont12, 1) / 250 + 2, ChatRows, DevNoteMaxRows);
	const int y = mainPanelPosition.y + 10 - (rows - ChatRows) * LineHeight;
	const Size TextSize { 250, rows * LineHeight };

	// The box is the text rect plus a margin, and the frame goes OUTSIDE that - so the border never
	// eats into the space the text measured itself against.
	constexpr int TextMargin = 6;
	const Rectangle box { { x - TextMargin, y - TextMargin },
		{ TextSize.width + 2 * TextMargin, TextSize.height + 2 * TextMargin } };
	FillRect(out, box.position.x, box.position.y, box.size.width, box.size.height, 0);
	oracool::DrawOrnateBorderOutside(out, box);

	const uint32_t len = DrawString(out, TalkMessage, { { x, y }, TextSize },
	    TextRenderOptions {
	        /*flags=*/UiFlags::ColorWhite | UiFlags::PentaCursor,
	        /*spacing=*/1,
	        /*lineHeight=*/13,
	        /*cursorPosition=*/static_cast<int>(ChatCursor.position),
	        /*highlightRange=*/ { static_cast<int>(ChatCursor.selection.begin), static_cast<int>(ChatCursor.selection.end) },
	    });
	ChatInputState->truncate(len);
}

/**
 * @brief Retired with the VOICE buttons (Oracool, 2026-08-18).
 *
 * These hit-tested the three whisper toggles that used to sit on the chat panel's lower plate, at
 * hardcoded rows 69..123 and columns 172..233 of the vanilla art. That art and those buttons are
 * gone - they were Diablo's multiplayer voice-chat controls, and this fork is single-player - so
 * there is nothing there to press.
 *
 * Kept as no-ops rather than deleted outright because both are called from diablo.cpp's mouse
 * handlers, where they sit ahead of every other click test; removing them means touching that
 * ordering, and the ordering is the part that is easy to get subtly wrong.
 */
bool control_check_talk_btn()
{
	return false;
}

void control_release_talk_btn()
{
}

void control_type_message()
{
	if (!IsChatAvailable())
		return;

	talkflag = true;
	ChatInputState.emplace(TextInputState::Options {
	    /*value=*/TalkMessage,
	    /*cursor=*/&ChatCursor,
	    /*maxLength=*/sizeof(TalkMessage) - 1 });
	SDL_Rect rect = MakeSdlRect(GetMainPanel().position.x + 200, GetMainPanel().position.y + 22, 0, 27);
	SDL_SetTextInputRect(&rect);
	TalkMessage[0] = '\0';

	// The log shares this column with the message history, so it stands aside while chatting (user,
	// 2026-08-18: "when the Messages History window pops-up, the Log to close temporary"). Remembered
	// rather than simply closed, so "temporary" is honoured - control_reset_talk puts it back.
	LogWasOpenBeforeChat = oracool::IsEventLogOpen();
	if (LogWasOpenBeforeChat)
		oracool::ToggleEventLog();
	sgbPlrTalkTbl = GetMainPanel().size.height + 16;
	RedrawEverything();
	TalkSaveIndex = NextTalkSave;
	SDL_StartTextInput();
}

void control_reset_talk()
{
	talkflag = false;
	if (LogWasOpenBeforeChat && !oracool::IsEventLogOpen())
		oracool::ToggleEventLog();
	LogWasOpenBeforeChat = false;
	SDL_StopTextInput();
	ChatInputState = std::nullopt;
	sgbPlrTalkTbl = 0;
	RedrawEverything();
}

bool IsTalkActive()
{
	if (!IsChatAvailable())
		return false;

	if (!talkflag)
		return false;

	return true;
}

template <typename InputStateType>
bool HandleInputEvent(const SDL_Event &event, std::optional<InputStateType> &inputState)
{
	if (!inputState) {
		return false; // No input state to handle
	}

	if constexpr (std::is_same_v<InputStateType, TextInputState>) {
		return HandleTextInputEvent(event, *inputState);
	} else if constexpr (std::is_same_v<InputStateType, NumberInputState>) {
		return HandleNumberInputEvent(event, *inputState);
	}

	return false; // Unknown input state type
}

bool HandleTalkTextInputEvent(const SDL_Event &event)
{
	return HandleInputEvent(event, ChatInputState);
}

bool control_presskeys(SDL_Keycode vkey)
{
	if (!IsChatAvailable())
		return false;
	if (!talkflag)
		return false;

	switch (vkey) {
	case SDLK_ESCAPE:
		control_reset_talk();
		return true;
	case SDLK_RETURN:
	case SDLK_KP_ENTER:
		ControlPressEnter();
		return true;
	case SDLK_BACKSPACE:
		TalkMessage[FindLastUtf8Symbols(TalkMessage)] = '\0';
		return true;
	case SDLK_DOWN:
		ControlUpDown(1);
		return true;
	case SDLK_UP:
		ControlUpDown(-1);
		return true;
	default:
		return vkey >= SDLK_SPACE && vkey <= SDLK_z;
	}
}

void DiabloHotkeyMsg(uint32_t dwMsg)
{
	if (!IsChatAvailable()) {
		return;
	}

	assert(dwMsg < QUICK_MESSAGE_OPTIONS);

	for (auto &msg : sgOptions.Chat.szHotKeyMsgs[dwMsg]) {

#ifdef _DEBUG
		if (CheckDebugTextCommand(msg))
			continue;
#endif
		if (CheckTextCommand(msg))
			continue;
		char charMsg[MAX_SEND_STR_LEN];
		CopyUtf8(charMsg, msg, sizeof(charMsg));
		NetSendCmdString(0xFFFFFF, charMsg);
	}
}

void OpenGoldDrop(int8_t invIndex, int max)
{
	DropGoldFlag = true;
	GoldDropInvIndex = invIndex;
	// The page the stack is ON, captured now: the prompt does not hold the mouse, so by the time
	// the player presses Enter the inventory may be showing a different tab entirely.
	GoldDropInvTab = ActiveInventoryTab;
	GoldDropStashIndex = StashStruct::EmptyCell;
	GoldDropText[0] = '\0';
	GoldDropInputState.emplace(NumberInputState::Options {
	    /*textOptions*/ {
	        /*value=*/GoldDropText,
	        /*cursor=*/&GoldDropCursor,
	        /*maxLength=*/sizeof(GoldDropText) - 1,
	    },
	    /*min=*/0,
	    /*max=*/max,
	});
	PlaceGoldDropBox();
	SDL_StartTextInput();
}

/**
 * @brief The same prompt, for a stack in the STASH.
 *
 * Its own opener rather than an overload of the one above, because the stash indexes nothing like
 * the backpack: @p stashIndex is a position in Stash.stashList and the page it is drawn on is a
 * separate fact, which RemoveStashItem needs and cannot recover.
 */
void OpenStashStackSplit(uint16_t stashIndex, int max)
{
	DropGoldFlag = true;
	GoldDropInvIndex = 0;
	GoldDropInvTab = 0;
	GoldDropStashIndex = stashIndex;
	GoldDropStashPage = Stash.GetPage();
	GoldDropText[0] = '\0';
	GoldDropInputState.emplace(NumberInputState::Options {
	    /*textOptions*/ {
	        /*value=*/GoldDropText,
	        /*cursor=*/&GoldDropCursor,
	        /*maxLength=*/sizeof(GoldDropText) - 1,
	    },
	    /*min=*/0,
	    /*max=*/max,
	});
	PlaceGoldDropBox();
	SDL_StartTextInput();
}

void CloseGoldDrop()
{
	if (!DropGoldFlag)
		return;
	SDL_StopTextInput();
	DropGoldFlag = false;
	GoldDropInputState = std::nullopt;
	GoldDropInvIndex = 0;
	GoldDropInvTab = 0;
	GoldDropStashIndex = StashStruct::EmptyCell;
	GoldDropStashPage = 0;
}

bool HandleGoldDropTextInputEvent(const SDL_Event &event)
{
	return HandleInputEvent(event, GoldDropInputState);
}

} // namespace devilution
