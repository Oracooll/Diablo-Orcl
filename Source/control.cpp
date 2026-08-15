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
#include "oracool/attack_skills.h"
#include "oracool/furious_charge.h"
#include "oracool/hud_art.h"
#include "oracool/hud_layout.h"
#include "oracool/hud_menu.h"
#include "oracool/inventory_layout.h"
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
std::optional<NumberInputState> GoldDropInputState;
} // namespace

bool chrbtn[4];
bool lvlbtndown;
bool chrbtnactive;
bool resetStatsButtonDown;
UiFlags InfoColor;
std::vector<UiFlags> InfoStringLineColors;
bool talkflag;
bool sbookflag;
bool chrflag;
StringOrView InfoString;
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
bool IsRightPanelOpen()
{
	return invflag || sbookflag;
}
bool IsOverRightPanel(Point position)
{
	// The inventory and the spellbook no longer share a rect - see the header. Both have since
	// grown into their own 340x720 windows, so neither may be tested against RightPanel's vanilla
	// 320x352: a window hit-tested smaller than it draws lets clicks reach the ground beneath it.
	if (invflag && oracool::GetInventoryPanelRect().contains(position))
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

namespace {

OptionalOwnedClxSpriteList talkButtons;
OptionalOwnedClxSpriteList pDurIcons;

char TalkSave[8][MAX_SEND_STR_LEN];
uint8_t TalkSaveIndex;
uint8_t NextTalkSave;
char TalkMessage[MAX_SEND_STR_LEN];
bool TalkButtonsDown[3];
int sgbPlrTalkTbl;
bool WhisperList[MAX_PLRS];

TextInputCursorState ChatCursor;
std::optional<TextInputState> ChatInputState;

int CapStatPointsToAdd(int remainingStatPoints, const Player &player, CharacterAttribute attribute)
{
	const int maximum = 255;
	int pointsToReachCap = maximum - player.GetBaseAttributeValue(attribute);

	return std::min(remainingStatPoints, pointsToReachCap);
}

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
	for (int arena = SL_FIRST_ARENA; arena <= SL_LAST; arena++) {
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

std::vector<TextCmdItem> TextCmdList = {
	{ N_("/help"), N_("Prints help overview or help for a specific command."), N_("[command]"), &TextCmdHelp },
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
 * @brief Splits `amount` units off a stackable consumable at inventory-or-belt index
 * `cii` (same indexing scheme as UseInvItem) onto the player's cursor.
 */
void RemoveStackSplit(Player &player, int cii, int amount)
{
	const bool isBeltIndex = cii > INVITEM_INV_LAST;
	const int index = isBeltIndex ? (cii - INVITEM_BELT_FIRST) : (cii - INVITEM_INV_FIRST);
	Item &source = isBeltIndex ? player.SpdList[index] : player.InvList[index];

	player.HoldItem = source;
	player.HoldItem.setStackCount(amount);

	if (source.stackCount() - amount <= 0) {
		if (isBeltIndex)
			player.RemoveSpdBarItem(index);
		else
			player.RemoveInvItem(index);
	} else {
		source.setStackCount(source.stackCount() - amount);
		if (isBeltIndex) {
			player.CalcScrolls();
			RedrawComponent(PanelDrawComponent::Belt);
			if (&player == MyPlayer)
				NetSendCmdChBeltItem(false, index);
		} else if (&player == MyPlayer) {
			NetSyncInvItem(player, index);
		}
	}

	NewCursor(player.HoldItem);
}

bool IsLevelUpButtonVisible()
{
	if (spselflag || chrflag || MyPlayer->_pStatPts == 0) {
		return false;
	}
	if (ControlMode == ControlTypes::VirtualGamepad) {
		return false;
	}
	if (stextflag != TalkID::None || IsStashOpen) {
		return false;
	}
	if (QuestLogIsOpen && GetLeftPanel().contains(GetMainPanel().position + Displacement { 0, -74 })) {
		return false;
	}

	return true;
}

} // namespace

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
	QuestLogIsOpen = false;
	CloseGoldWithdraw();
	CloseStash();
	chrflag = true;
}

void CloseCharPanel()
{
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
	if (InfoString.empty())
		InfoString = str;
	else
		InfoString = StrCat(InfoString, "\n", str);
	PushLineColors(str, color);
}

void AddPanelString(std::string &&str, UiFlags color)
{
	PushLineColors(str, color);
	if (InfoString.empty())
		InfoString = std::move(str);
	else
		InfoString = StrCat(InfoString, "\n", str);
}

void SetPanelString(StringOrView str, UiFlags color)
{
	InfoString = std::move(str);
	InfoColor = color;
	InfoStringLineColors.clear();
	PushLineColors(InfoString.str(), color);
}

void ClearPanelStrings()
{
	InfoString = {};
	InfoStringLineColors.clear();
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
	UiFlags color = (currValue > 0 ? (currValue == maxValue ? UiFlags::ColorGold : UiFlags::ColorWhite) : UiFlags::ColorRed);

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
			{
				const OwnedClxSpriteList sprite = LoadCel("ctrlpan\\talkpanl", GetMainPanel().size.width);
				ClxDraw(*pBtmBuff, { 0, (GetMainPanel().size.height + 16) * 2 - 1 }, sprite[0]);
			}
			talkButtons = LoadCel("ctrlpan\\talkbutt", 61);
		}
		sgbPlrTalkTbl = 0;
		TalkMessage[0] = '\0';
		for (bool &whisper : WhisperList)
			whisper = true;
		for (bool &talkButtonDown : TalkButtonsDown)
			talkButtonDown = false;
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
	chrbtnactive = false;
	ClearPanelStrings();
	RedrawComponent(PanelDrawComponent::Health);
	RedrawComponent(PanelDrawComponent::Mana);
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
		if ((SDL_GetModState() & KMOD_SHIFT) != 0) {
			ClearReadiedSpell(*MyPlayer);
			return;
		}
		DoSpeedBook();
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
		panelflag = true;
		return;
	}
	if (oracool::GetBeltSlotRect(oracool::BeltTownPortalSlotIndex).contains(MousePosition)) {
		SetPanelString(_("Town Portal"), UiFlags::ColorWhite);
		AddPanelString(_("Click to open."));
		InfoColor = UiFlags::ColorWhite;
		panelflag = true;
		return;
	}
	if (oracool::IsPointOverXpCounter(MousePosition)) {
		SetPanelString(_("Experience Meter"), UiFlags::ColorWhite);
		AddPanelString(_("Click for more."));
		InfoColor = UiFlags::ColorWhite;
		panelflag = true;
		return;
	}

	// Oracool: the LMB well now holds the basic attack rather than nothing, so it has something to
	// say. It is also the plate's other opener for the Abilities window (diablo.cpp's LeftMouseDown),
	// which was previously findable only by clicking the empty socket and seeing what happened.
	if (oracool::GetLmbSkillButtonRect().contains(MousePosition)) {
		const oracool::AttackIcon icon = oracool::BasicAttackIcon(*MyPlayer);
		SetPanelString(_(oracool::AttackIconName(icon)), UiFlags::ColorWhite);
		AddPanelString(_("Left click to attack"));
		AddPanelString(_("Click here for abilities"));
		InfoColor = UiFlags::ColorWhite;
		panelflag = true;
		return;
	}

	if (!spselflag && oracool::GetRmbSkillButtonRect().contains(MousePosition)) {
		SetPanelString(_("Select current spell button"), UiFlags::ColorWhite);
		InfoColor = UiFlags::ColorWhite;
		panelflag = true;
		AddPanelString(_("Hotkey: 's'"));
		Player &myPlayer = *MyPlayer;
		const SpellID spellId = myPlayer._pRSpell;
		if (!IsValidSpell(spellId)) {
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
	talkButtons = std::nullopt;
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
	if (!panelflag && !trigflag && pcursinvitem == -1 && pcursstashitem == StashStruct::EmptyCell && !ActiveTabItemHovered && !spselflag) {
		ClearPanelStrings();
		InfoColor = UiFlags::ColorWhite;
	}
	Player &myPlayer = *MyPlayer;
	if (spselflag || trigflag) {
		InfoColor = UiFlags::ColorWhite;
	} else if (!myPlayer.HoldItem.isEmpty()) {
		if (myPlayer.HoldItem._itype == ItemType::Gold) {
			int nGold = myPlayer.HoldItem._ivalue;
			InfoString = fmt::format(fmt::runtime(ngettext("{:s} gold piece", "{:s} gold pieces", nGold)), FormatInteger(nGold));
		} else if (!myPlayer.CanUseItem(myPlayer.HoldItem)) {
			InfoString = _("Requirements not met");
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

	if (*sgOptions.Oracool.resetStatsButton && !gbIsMultiplayer) {
		Rectangle resetButton { GetPanelPosition(UiPanels::Character, GetResetStatsButtonPosition()), ResetStatsButtonSize };
		if (resetButton.contains(MousePosition)) {
			resetStatsButtonDown = true;
			chrbtnactive = true;
			return;
		}
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

void ReleaseChrBtns(bool addAllStatPoints)
{
	chrbtnactive = false;
	// Same scroll gate as CheckChrBtns - a button that scrolled out from under the cursor between
	// press and release must not still act on the release.
	if (!GetCharacterContentRect().contains(MousePosition)) {
		resetStatsButtonDown = false;
		for (bool &pressed : chrbtn)
			pressed = false;
		return;
	}
	if (resetStatsButtonDown) {
		resetStatsButtonDown = false;
		Rectangle resetButton { GetPanelPosition(UiPanels::Character, GetResetStatsButtonPosition()), ResetStatsButtonSize };
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
			int statPointsToAdd = 1;
			if (addAllStatPoints)
				statPointsToAdd = CapStatPointsToAdd(myPlayer._pStatPts, myPlayer, attribute);
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

void DrawGoldSplit(const Surface &out)
{
	const int dialogX = 30;

	ClxDraw(out, GetPanelPosition(UiPanels::Inventory, { dialogX, 178 }), (*pGBoxBuff)[0]);

	const string_view amountText = GoldDropText;
	const TextInputCursorState &cursor = GoldDropCursor;
	const int max = GoldDropInputState->max();

	const bool splittingBeltItem = GoldDropInvIndex > INVITEM_INV_LAST;
	const Item &sourceItem = splittingBeltItem
	    ? MyPlayer->SpdList[GoldDropInvIndex - INVITEM_BELT_FIRST]
	    : MyPlayer->InvList[GoldDropInvIndex - INVITEM_INV_FIRST];

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
	DrawString(out, wrapped, { GetPanelPosition(UiPanels::Inventory, { dialogX + 31, 75 }), { 200, 50 } },
	    { UiFlags::ColorWhitegold | UiFlags::AlignCenter, 1, 17 });

	// Even a ten digit amount of gold only takes up about half a line. There's no need to wrap or clip text here so we
	// use the Point form of DrawString.
	DrawString(out, amountText, GetPanelPosition(UiPanels::Inventory, { dialogX + 37, 128 }),
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
		if (value != 0) {
			const bool splittingBeltItem = GoldDropInvIndex > INVITEM_INV_LAST;
			const Item &sourceItem = splittingBeltItem
			    ? myPlayer.SpdList[GoldDropInvIndex - INVITEM_BELT_FIRST]
			    : myPlayer.InvList[GoldDropInvIndex - INVITEM_INV_FIRST];
			if (sourceItem._itype == ItemType::Gold) {
				RemoveGold(myPlayer, GoldDropInvIndex, value);
			} else {
				RemoveStackSplit(myPlayer, GoldDropInvIndex, value);
			}
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

	DrawPanelBox(out, MakeSdlRect(175, sgbPlrTalkTbl + 20, 294, 5), mainPanelPosition + Displacement { 175, 4 });
	int off = 0;
	for (int i = 293; i > 283; off++, i--) {
		DrawPanelBox(out, MakeSdlRect((off / 2) + 175, sgbPlrTalkTbl + off + 25, i, 1), mainPanelPosition + Displacement { (off / 2) + 175, off + 9 });
	}
	DrawPanelBox(out, MakeSdlRect(185, sgbPlrTalkTbl + 35, 274, 30), mainPanelPosition + Displacement { 185, 19 });
	DrawPanelBox(out, MakeSdlRect(180, sgbPlrTalkTbl + 65, 284, 5), mainPanelPosition + Displacement { 180, 49 });
	for (int i = 0; i < 10; i++) {
		DrawPanelBox(out, MakeSdlRect(180, sgbPlrTalkTbl + i + 70, i + 284, 1), mainPanelPosition + Displacement { 180, i + 54 });
	}
	DrawPanelBox(out, MakeSdlRect(170, sgbPlrTalkTbl + 80, 310, 55), mainPanelPosition + Displacement { 170, 64 });

	int x = mainPanelPosition.x + 200;
	int y = mainPanelPosition.y + 10;

	const uint32_t len = DrawString(out, TalkMessage, { { x, y }, { 250, 39 } },
	    TextRenderOptions {
	        /*flags=*/UiFlags::ColorWhite | UiFlags::PentaCursor,
	        /*spacing=*/1,
	        /*lineHeight=*/13,
	        /*cursorPosition=*/static_cast<int>(ChatCursor.position),
	        /*highlightRange=*/ { static_cast<int>(ChatCursor.selection.begin), static_cast<int>(ChatCursor.selection.end) },
	    });
	ChatInputState->truncate(len);

	x += 46;
	int talkBtn = 0;
	for (size_t i = 0; i < Players.size(); i++) {
		Player &player = Players[i];
		if (&player == MyPlayer)
			continue;

		UiFlags color = player.friendlyMode ? UiFlags::ColorWhitegold : UiFlags::ColorRed;
		const Point talkPanPosition = mainPanelPosition + Displacement { 172, 84 + 18 * talkBtn };
		if (WhisperList[i]) {
			// the normal (unpressed) voice button is pre-rendered on the panel, only need to draw over it when the button is held
			if (TalkButtonsDown[talkBtn]) {
				unsigned spriteIndex = talkBtn == 0 ? 2 : 3; // the first button sprite includes a tip from the devils wing so is different to the rest.
				ClxDraw(out, talkPanPosition, (*talkButtons)[spriteIndex]);

				// Draw the translated string over the top of the default (english) button. This graphic is inset to avoid overlapping the wingtip, letting
				// the first button be treated the same as the other two further down the panel.
				RenderClxSprite(out, (*TalkButton)[2], talkPanPosition + Displacement { 4, -15 });
			}
		} else {
			unsigned spriteIndex = talkBtn == 0 ? 0 : 1; // the first button sprite includes a tip from the devils wing so is different to the rest.
			if (TalkButtonsDown[talkBtn])
				spriteIndex += 4; // held button sprites are at index 4 and 5 (with and without wingtip respectively)
			ClxDraw(out, talkPanPosition, (*talkButtons)[spriteIndex]);

			// Draw the translated string over the top of the default (english) button. This graphic is inset to avoid overlapping the wingtip, letting
			// the first button be treated the same as the other two further down the panel.
			RenderClxSprite(out, (*TalkButton)[TalkButtonsDown[talkBtn] ? 1 : 0], talkPanPosition + Displacement { 4, -15 });
		}
		if (player.plractive) {
			DrawString(out, player._pName, { { x, y + 60 + talkBtn * 18 }, { 204, 0 } }, { color });
		}

		talkBtn++;
	}
}

bool control_check_talk_btn()
{
	if (!talkflag)
		return false;

	const Point mainPanelPosition = GetMainPanel().position;

	if (MousePosition.x < 172 + mainPanelPosition.x)
		return false;
	if (MousePosition.y < 69 + mainPanelPosition.y)
		return false;
	if (MousePosition.x > 233 + mainPanelPosition.x)
		return false;
	if (MousePosition.y > 123 + mainPanelPosition.y)
		return false;

	for (bool &talkButtonDown : TalkButtonsDown) {
		talkButtonDown = false;
	}

	TalkButtonsDown[(MousePosition.y - (69 + mainPanelPosition.y)) / 18] = true;

	return true;
}

void control_release_talk_btn()
{
	if (!talkflag)
		return;

	for (bool &talkButtonDown : TalkButtonsDown)
		talkButtonDown = false;

	const Point mainPanelPosition = GetMainPanel().position;

	if (MousePosition.x < 172 + mainPanelPosition.x || MousePosition.y < 69 + mainPanelPosition.y || MousePosition.x > 233 + mainPanelPosition.x || MousePosition.y > 123 + mainPanelPosition.y)
		return;

	int off = (MousePosition.y - (69 + mainPanelPosition.y)) / 18;

	size_t playerId = 0;
	for (; playerId < Players.size() && off != -1; ++playerId) {
		if (playerId != MyPlayerId)
			off--;
	}
	if (playerId > 0 && playerId <= Players.size())
		WhisperList[playerId - 1] = !WhisperList[playerId - 1];
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
	for (bool &talkButtonDown : TalkButtonsDown) {
		talkButtonDown = false;
	}
	sgbPlrTalkTbl = GetMainPanel().size.height + 16;
	RedrawEverything();
	TalkSaveIndex = NextTalkSave;
	SDL_StartTextInput();
}

void control_reset_talk()
{
	talkflag = false;
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
}

bool HandleGoldDropTextInputEvent(const SDL_Event &event)
{
	return HandleInputEvent(event, GoldDropInputState);
}

} // namespace devilution
