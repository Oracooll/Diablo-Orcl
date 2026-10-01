/**
 * @file debug.cpp
 *
 * Implementation of debug functions.
 */

#ifdef _DEBUG

#include <algorithm> // std::clamp - the keystone command's tier
#include <cstdint>
#include <cstdio>
#include <set>

#include "debug.h"

#include "automap.h"
#include "control.h"
#include "cursor.h"
#include "engine/backbuffer_state.hpp"
#include "engine/events.hpp"
#include "engine/load_cel.hpp"
#include "engine/point.hpp"
#include "error.h"
#include "inv.h"
#include "levels/setmaps.h"
#include "lighting.h"
#include "levels/gendung.h"
#include "monstdat.h"
#include "monster.h"
#include "oracool/hud_art.h"
#include "oracool/minions.h"
#include "oracool/rift.h"
#include "oracool/item_sets.h"
#include "oracool/telemetry.h"
#include "oracool/waypoint_menu.h"
#include "oracool/runeword_book.h"
#include "pack.h"
#include "plrmsg.h"
#include "quests.h"
#include "spells.h"
#include "towners.h"
#include <fmt/format.h>

#include "utils/endian_stream.hpp"
#include "utils/file_util.h"
#include "utils/paths.h"
#include "utils/language.h"
#include "utils/log.hpp"
#include "utils/str_case.hpp"
#include "utils/str_cat.hpp"
#include "utils/str_split.hpp"

namespace devilution {

std::string TestMapPath;
OptionalOwnedClxSpriteList pSquareCel;
bool DebugToggle = false;
bool DebugGodMode = false;
bool DebugVision = false;
bool DebugPath = false;
bool DebugGrid = false;
bool DebugHideUi = false;
bool DebugClearUi = false;
std::unordered_map<int, Point> DebugCoordsMap;
bool DebugScrollViewEnabled = false;
std::string debugTRN;

// Used for debugging level generation
uint32_t glMid1Seed[NUMLEVELS];
uint32_t glMid2Seed[NUMLEVELS];
uint32_t glMid3Seed[NUMLEVELS];
uint32_t glEndSeed[NUMLEVELS];

namespace {

enum class DebugGridTextItem : uint16_t {
	None,
	dPiece,
	dTransVal,
	dLight,
	dPreLight,
	dFlags,
	dPlayer,
	dMonster,
	dCorpse,
	dObject,
	dItem,
	dSpecial,

	coords,
	cursorcoords,
	objectindex,

	// take dPiece as index
	Solid,
	Transparent,
	Trap,

	// megatiles
	AutomapView,
	dungeon,
	pdungeon,
	Protected,
};

DebugGridTextItem SelectedDebugGridTextItem;

int DebugMonsterId;

std::vector<std::string> SearchMonsters;
std::vector<std::string> SearchItems;
std::vector<std::string> SearchObjects;

void PrintDebugMonster(const Monster &monster)
{
	EventPlrMsg(StrCat(
	                "Monster ", static_cast<int>(monster.getId()), " = ", monster.name(),
	                "\nX = ", monster.position.tile.x, ", Y = ", monster.position.tile.y,
	                "\nEnemy = ", monster.enemy, ", HP = ", monster.hitPoints,
	                "\nMode = ", static_cast<int>(monster.mode), ", Var1 = ", monster.var1),
	    UiFlags::ColorWhite);

	bool bActive = false;

	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		if (&Monsters[ActiveMonsters[i]] == &monster) {
			bActive = true;
			break;
		}
	}

	EventPlrMsg(StrCat("Active List = ", bActive ? 1 : 0, ", Squelch = ", monster.activeForTicks), UiFlags::ColorWhite);
}

struct DebugCmdItem {
	const string_view text;
	const string_view description;
	const string_view requiredParameter;
	std::string (*actionProc)(const string_view);
};

extern std::vector<DebugCmdItem> DebugCmdList;

std::string DebugCmdHelp(const string_view parameter)
{
	if (parameter.empty()) {
		std::string ret = "Available Debug Commands: ";
		bool first = true;
		for (const auto &dbgCmd : DebugCmdList) {
			if (first)
				first = false;
			else
				ret.append(" - ");
			ret.append(std::string(dbgCmd.text));
		}
		return ret;
	}
	auto debugCmdIterator = std::find_if(DebugCmdList.begin(), DebugCmdList.end(), [&](const DebugCmdItem &elem) { return elem.text == parameter; });
	if (debugCmdIterator == DebugCmdList.end())
		return StrCat("Debug command ", parameter, " wasn't found");
	auto &dbgCmdItem = *debugCmdIterator;
	if (dbgCmdItem.requiredParameter.empty())
		return StrCat("Description: ", dbgCmdItem.description, "\nParameters: No additional parameter needed.");
	return StrCat("Description: ", dbgCmdItem.description, "\nParameters: ", dbgCmdItem.requiredParameter);
}

std::string DebugCmdGiveGoldCheat(const string_view parameter)
{
	Player &myPlayer = *MyPlayer;

	for (int8_t &itemIndex : myPlayer.InvGrid) {
		if (itemIndex != 0)
			continue;

		Item &goldItem = myPlayer.InvList[myPlayer._pNumInv];
		MakeGoldStack(goldItem, GOLD_MAX_LIMIT);
		myPlayer._pNumInv++;
		itemIndex = myPlayer._pNumInv;

		myPlayer._pGold += goldItem._ivalue;
	}
	CalcPlrInv(myPlayer, true);

	return "You are now rich! If only this was as easy in real life...";
}

std::string DebugCmdTakeGoldCheat(const string_view parameter)
{
	Player &myPlayer = *MyPlayer;

	for (auto itemIndex : myPlayer.InvGrid) {
		itemIndex -= 1;

		if (itemIndex < 0)
			continue;
		if (myPlayer.InvList[itemIndex]._itype != ItemType::Gold)
			continue;

		myPlayer.RemoveInvItem(itemIndex);
	}

	myPlayer._pGold = 0;

	return "You are poor...";
}

std::string DebugCmdWarpToLevel(const string_view parameter)
{
	Player &myPlayer = *MyPlayer;
	auto level = atoi(parameter.data());
	if (level < 0 || level > (gbIsHellfire ? 24 : 16))
		return StrCat("Level ", level, " is not known. Do you want to write a mod?");
	if (!setlevel && myPlayer.isOnLevel(level))
		return StrCat("I did nothing but fulfilled your wish. You are already at level ", level, ".");

	StartNewLvl(myPlayer, (level != 21) ? interface_mode::WM_DIABNEXTLVL : interface_mode::WM_DIABTOWNWARP, level);
	return StrCat("Welcome to level ", level, ".");
}

std::string DebugCmdLoadQuestMap(const string_view parameter)
{
	if (parameter.empty()) {
		std::string ret = "What mapid do you want to visit?";
		for (auto &quest : Quests) {
			if (quest._qslvl <= 0)
				continue;
			StrAppend(ret, " ", quest._qslvl, " (", QuestLevelNames[quest._qslvl], ")");
		}
		return ret;
	}

	auto level = atoi(parameter.data());
	if (level < 1)
		return "Map id must be 1 or higher";
	if (setlevel && setlvlnum == level)
		return StrCat("I did nothing but fulfilled your wish. You are already at mapid .", level);

	for (auto &quest : Quests) {
		if (level != quest._qslvl)
			continue;

		setlvltype = quest._qlvltype;
		StartNewLvl(*MyPlayer, WM_DIABSETLVL, level);

		return StrCat("Welcome to ", QuestLevelNames[level], ".");
	}

	return StrCat("Mapid ", level, " is not known. Do you want to write a mod?");
}

/**
 * @brief Opens a rift at the gate and walks straight in (oracool/rift.h): "rift nephalem", "rift
 * guardian 40". Town only, like the gate itself.
 */
std::string DebugCmdKeystone(const string_view parameter)
{
	// A Guardian Keystone at the hero's feet (user, 2026-09-20: "i need a debug command for Guardian
	// Rift keys"): the tier as given, or the deepest floor's tier - what a Nephalem Rift would open
	// at - when none is. Dropped, not placed in the pack, so it goes through the same tumble and
	// pickup a guardian's drop does.
	Player &myPlayer = *MyPlayer;
	int tier = parameter.empty() ? 0 : atoi(parameter.data());
	if (tier <= 0)
		tier = oracool::NephalemRiftTierFor(myPlayer);
	tier = std::clamp(tier, 1, 255);
	const int before = ActiveItemCount;
	oracool::DropGuardianKeystone(myPlayer.position.tile, tier);
	if (ActiveItemCount <= before)
		return "No room for a keystone here.";
	return StrCat("A Guardian Keystone of tier ", tier, " lies at your feet.");
}

std::string DebugCmdRift(const string_view parameter)
{
	Player &myPlayer = *MyPlayer;
	if (!myPlayer.isOnLevel(0) || setlevel)
		return "The rifts open from town.";
	std::string kind;
	int tier = 0;
	{
		const size_t space = parameter.find(' ');
		kind = std::string(parameter.substr(0, space));
		if (space != string_view::npos)
			tier = atoi(parameter.substr(space + 1).data());
	}
	if (kind == "guardian") {
		if (tier <= 0)
			tier = oracool::NephalemRiftTierFor(myPlayer);
		tier = std::clamp(tier, 1, 255); // a keystone's own range: past it the scaling overflowed (round 13 audit)
		if (!oracool::OpenGuardianRift(myPlayer, tier))
			return "Could not open a Guardian Rift here.";
	} else if (kind == "nephalem" || kind.empty()) {
		if (!oracool::OpenNephalemRift(myPlayer))
			return "Could not open a Nephalem Rift here.";
	} else {
		return "rift {nephalem|guardian} [tier]";
	}
	if (!oracool::EnterRift(myPlayer))
		return "The rift would not open.";
	return StrCat("Entering a ", oracool::RiftKindName(oracool::ActiveRift()), " at tier ", oracool::RiftTier(),
	    "; ", oracool::RiftGuardianName(oracool::RiftGuardian()), " waits at 100%.");
}

std::string DebugCmdLoadMap(const string_view parameter)
{
	TestMapPath.clear();
	int mapType = 0;
	Point spawn = {};

	int count = 0;
	for (string_view arg : SplitByChar(parameter, ' ')) {
		switch (count) {
		case 0:
			TestMapPath = StrCat(arg, ".dun");
			break;
		case 1:
			mapType = atoi(std::string(arg).c_str());
			break;
		case 2:
			spawn.x = atoi(std::string(arg).c_str());
			break;
		case 3:
			spawn.y = atoi(std::string(arg).c_str());
			break;
		}
		count++;
	}

	if (TestMapPath.empty() || mapType < DTYPE_CATHEDRAL || mapType > DTYPE_LAST || !InDungeonBounds(spawn))
		return "Directions not understood";

	setlvltype = static_cast<dungeon_type>(mapType);
	ViewPosition = spawn;

	StartNewLvl(*MyPlayer, WM_DIABSETLVL, SL_NONE);

	return "Welcome to this unique place.";
}

std::string ExportDun(const string_view parameter)
{
	std::string levelName = StrCat(currlevel, "-", glSeedTbl[currlevel], ".dun");

	// "wb", not "ab" - a second export of a level appended a second map into one file - and refused when it cannot
	// be opened, where writing through the null handle crashed (round 14 audit, v1.12.239).
	FILE *dunFile = OpenFile(levelName.c_str(), "wb");
	if (dunFile == nullptr)
		return StrCat("Could not open ", levelName, " for writing.");

	WriteLE16(dunFile, DMAXX);
	WriteLE16(dunFile, DMAXY);

	/** Tiles. */
	for (int y = 0; y < DMAXY; y++) {
		for (int x = 0; x < DMAXX; x++) {
			WriteLE16(dunFile, dungeon[x][y]);
		}
	}

	/** Padding */
	for (int y = 16; y < MAXDUNY - 16; y++) {
		for (int x = 16; x < MAXDUNX - 16; x++) {
			WriteLE16(dunFile, 0);
		}
	}

	/** Monsters */
	for (int y = 16; y < MAXDUNY - 16; y++) {
		for (int x = 16; x < MAXDUNX - 16; x++) {
			uint16_t monsterId = 0;
			if (dMonster[x][y] > 0) {
				for (int i = 0; i < 157; i++) {
					if (MonstConvTbl[i] == Monsters[abs(dMonster[x][y]) - 1].type().type) {
						monsterId = i + 1;
						break;
					}
				}
			}
			WriteLE16(dunFile, monsterId);
		}
	}

	/** Objects */
	for (int y = 16; y < MAXDUNY - 16; y++) {
		for (int x = 16; x < MAXDUNX - 16; x++) {
			uint16_t objectId = 0;
			Object *object = FindObjectAtPosition({ x, y }, false);
			if (object != nullptr) {
				for (int i = 0; i < 147; i++) {
					if (ObjTypeConv[i] == object->_otype) {
						objectId = i;
						break;
					}
				}
			}
			WriteLE16(dunFile, objectId);
		}
	}

	/** Transparency */
	for (int y = 16; y < MAXDUNY - 16; y++) {
		for (int x = 16; x < MAXDUNX - 16; x++) {
			WriteLE16(dunFile, dTransVal[x][y]);
		}
	}
	std::fclose(dunFile);

	return StrCat(levelName, " saved. Happy mapping!");
}

std::unordered_map<string_view, _talker_id> TownerShortNameToTownerId = {
	{ "griswold", _talker_id::TOWN_SMITH },
	{ "pepin", _talker_id::TOWN_HEALER },
	{ "ogden", _talker_id::TOWN_TAVERN },
	{ "cain", _talker_id::TOWN_STORY },
	{ "farnham", _talker_id::TOWN_DRUNK },
	{ "adria", _talker_id::TOWN_WITCH },
	{ "gillian", _talker_id::TOWN_BMAID },
	{ "wirt", _talker_id ::TOWN_PEGBOY },
	{ "lester", _talker_id ::TOWN_FARMER },
	{ "girl", _talker_id ::TOWN_GIRL },
	{ "nut", _talker_id::TOWN_COWFARM },
};

std::string DebugCmdVisitTowner(const string_view parameter)
{
	Player &myPlayer = *MyPlayer;

	if (setlevel || !myPlayer.isOnLevel(0))
		return "What kind of friends do you have in dungeons?";

	if (parameter.empty()) {
		std::string ret;
		ret = "Who? ";
		for (auto &entry : TownerShortNameToTownerId) {
			ret.append(" ");
			ret.append(std::string(entry.first));
		}
		return ret;
	}

	auto it = TownerShortNameToTownerId.find(parameter);
	if (it == TownerShortNameToTownerId.end())
		return StrCat(parameter, " is unknown. Perhaps he is a ninja?");

	for (auto &towner : Towners) {
		if (towner._ttype != it->second)
			continue;

		CastSpell(
		    MyPlayerId,
		    SpellID::Teleport,
		    myPlayer.position.tile.x,
		    myPlayer.position.tile.y,
		    towner.position.x,
		    towner.position.y,
		    1);

		return StrCat("Say hello to ", parameter, " from me.");
	}

	return StrCat("Couldn't find ", parameter, ".");
}

std::string DebugCmdResetLevel(const string_view parameter)
{
	Player &myPlayer = *MyPlayer;

	auto args = SplitByChar(parameter, ' ');
	auto it = args.begin();
	if (it == args.end())
		return "What level do you want to visit?";
	auto level = atoi(std::string(*it).c_str());
	if (level < 0 || level > (gbIsHellfire ? 24 : 16))
		return StrCat("Level ", level, " is not known. Do you want to write an extension mod?");
	// Refused BEFORE anything is cleared: the level was wiped and then refused, and its floor items were lost on the way
	// back (round 13 audit, v1.12.238).
	if (myPlayer.isOnLevel(level))
		return StrCat("Level ", level, " can't be cleaned, cause you still occupy it!");
	uint32_t seed = 0;
	const bool hasSeed = ++it != args.end();
	if (hasSeed) {
		// strtoul with an end check: std::stoul threw on "x" or an overlong number and ended the game (round 13 audit).
		const std::string seedText(*it);
		char *end = nullptr;
		const unsigned long parsed = std::strtoul(seedText.c_str(), &end, 10);
		if (seedText.empty() || end == nullptr || *end != '\0' || parsed > UINT32_MAX)
			return "The seed must be a number from 0 to 4294967295.";
		seed = static_cast<uint32_t>(parsed);
	}
	myPlayer._pLvlVisited[level] = false;
	DeltaClearLevel(level);
	if (hasSeed)
		glSeedTbl[level] = seed;
	return StrCat("Level ", level, " was restored and looks fabulous.");
}

/**
 * @brief Oracool, phase N4 of the Necromancer: a debug army, so the crowd can be walked through a corridor before
 * any skill raises one. "army" fills every group to its cap (27 bodies); "army 12" raises twelve skeleton-bodied
 * minions across the groups in order; "army 0" dismisses it.
 */
std::string DebugCmdArmy(const string_view parameter)
{
	if (leveltype == DTYPE_TOWN)
		return "An army needs a dungeon.";
	Player &myPlayer = *MyPlayer;
	const std::string text(parameter);
	const bool hasNumber = !text.empty();
	const int wanted = hasNumber ? atoi(text.c_str()) : 1000;
	if (hasNumber && wanted <= 0) {
		oracool::DismissMinions(myPlayer);
		return "The army is dismissed.";
	}
	const int level = std::max<int>(myPlayer._pLevel, 1);
	int raised = 0;
	for (size_t g = 0; g < oracool::MinionGroupCount && raised < wanted; g++) {
		const auto group = static_cast<oracool::MinionGroup>(g);
		oracool::MinionSpec spec {};
		spec.group = group;
		spec.type = group == oracool::MinionGroup::Mage ? MT_XSKELBW : (group == oracool::MinionGroup::Golem ? MT_GOLEM : (group == oracool::MinionGroup::Revived ? MT_TSKELAX : MT_WSKELAX));
		if (group == oracool::MinionGroup::Mage)
			spec.missile = MissileID::Firebolt;
		if (group == oracool::MinionGroup::Golem) {
			spec.golem = oracool::GolemKind::Iron;
			spec.ramp = 240;
		}
		spec.life = (group == oracool::MinionGroup::Golem ? 120 : 30) + 8 * level;
		spec.minDamage = 2 + level / 2;
		spec.maxDamage = 6 + level;
		spec.toHit = 60 + 2 * level;
		spec.armorClass = 10 + level;
		while (raised < wanted && oracool::SummonMinion(myPlayer, spec, myPlayer.position.tile))
			raised++;
	}
	return StrCat("Raised ", raised, ". The army stands at ", oracool::MinionCount(myPlayer), ".");
}

std::string DebugCmdGodMode(const string_view parameter)
{
	DebugGodMode = !DebugGodMode;
	if (DebugGodMode)
		return "A god descended.";
	return "You are mortal, beware of the darkness.";
}

std::string DebugCmdLighting(const string_view parameter)
{
	ToggleLighting();

	return "All raindrops are the same.";
}

std::string DebugCmdMapReveal(const string_view parameter)
{
	for (int x = 0; x < DMAXX; x++)
		for (int y = 0; y < DMAXY; y++)
			UpdateAutomapExplorer({ x, y }, MAP_EXP_SHRINE);

	return "The way is made clear when viewed from above";
}

std::string DebugCmdMapHide(const string_view parameter)
{
	for (int x = 0; x < DMAXX; x++)
		for (int y = 0; y < DMAXY; y++)
			AutomapView[x][y] = MAP_EXP_NONE;

	return "The way is made unclear when viewed from below";
}

std::string DebugCmdVision(const string_view parameter)
{
	DebugVision = !DebugVision;
	if (DebugVision)
		return "You see as I do.";

	return "My path is set.";
}

std::string DebugCmdPath(const string_view parameter)
{
	DebugPath = !DebugPath;
	if (DebugPath)
		return "The mushroom trail.";

	return "The path is hidden.";
}

std::string DebugCmdQuest(const string_view parameter)
{
	if (parameter.empty()) {
		std::string ret = "You must provide an id. This could be: all";
		for (auto &quest : Quests) {
			if (IsNoneOf(quest._qactive, QUEST_NOTAVAIL, QUEST_INIT))
				continue;
			StrAppend(ret, ", ", quest._qidx, " (", QuestsData[quest._qidx]._qlstr, ")");
		}
		return ret;
	}

	if (parameter.compare("all") == 0) {
		for (auto &quest : Quests) {
			if (IsNoneOf(quest._qactive, QUEST_NOTAVAIL, QUEST_INIT))
				continue;

			quest._qactive = QUEST_ACTIVE;
			quest._qlog = true;
		}

		return "Happy questing";
	}

	int questId = atoi(parameter.data());

	// Negative too: -1 wrote before the quest array (round 13 audit, v1.12.238).
	if (questId < 0 || questId >= MAXQUESTS)
		return StrCat("Quest ", questId, " is not known. Do you want to write a mod?");
	auto &quest = Quests[questId];

	if (IsNoneOf(quest._qactive, QUEST_NOTAVAIL, QUEST_INIT))
		return StrCat(QuestsData[questId]._qlstr, " was already given.");

	quest._qactive = QUEST_ACTIVE;
	quest._qlog = true;

	return StrCat(QuestsData[questId]._qlstr, " enabled.");
}

std::string DebugCmdLevelUp(const string_view parameter)
{
	int levels = std::max(1, atoi(parameter.data()));
	for (int i = 0; i < levels; i++)
		NetSendCmd(true, CMD_CHEAT_EXPERIENCE);
	return "New experience leads to new insights.";
}

std::string DebugCmdGiveWaypoints(const string_view parameter)
{
	// Oracool: user request - unlocks every waypoint (1-16; Tristram/0 is always unlocked
	// already) on the current difficulty, so all 17 travel-list entries can be tried without
	// having to actually find and activate every sigil first.
	for (int i = 1; i <= 24; i++) // the Hellfire act too (round 9 audit)
		oracool::UnlockWaypoint(i);
	return "The way is open.";
}

std::string DebugCmdMaxStats(const string_view parameter)
{
	Player &myPlayer = *MyPlayer;
	// To the fork's ceiling, 255, not the class row's vanilla maximum (round 44 audit: it LOWERED a base past the row - a
	// Paladin's 200 Magic fell to 50). ModifyPlr* clamps at the ceiling.
	ModifyPlrStr(myPlayer, 255 - myPlayer._pBaseStr);
	ModifyPlrMag(myPlayer, 255 - myPlayer._pBaseMag);
	ModifyPlrDex(myPlayer, 255 - myPlayer._pBaseDex);
	ModifyPlrVit(myPlayer, 255 - myPlayer._pBaseVit);
	return "Who needs elixirs anyway?";
}

std::string DebugCmdMinStats(const string_view parameter)
{
	Player &myPlayer = *MyPlayer;
	ModifyPlrStr(myPlayer, -myPlayer._pBaseStr);
	ModifyPlrMag(myPlayer, -myPlayer._pBaseMag);
	ModifyPlrDex(myPlayer, -myPlayer._pBaseDex);
	ModifyPlrVit(myPlayer, -myPlayer._pBaseVit);
	return "From hero to zero.";
}

std::string DebugCmdSetSpellsLevel(const string_view parameter)
{
	// Oracool: this both LEARNS and levels - CMD_CHANGE_SPELL_LEVEL's handler sets _pMemSpells as
	// well as _pSplLvl (see OnChangeSpellLevel in msg.cpp), so one call is the whole "teach me
	// everything" button.
	//
	// Its reach is "every spell with a book", which as of 2026-08-15 is every spell in the game bar
	// the six class skills - and those are granted to every class at birth now anyway
	// (oracool::AllClassSkillsBitmask), so between the two nothing castable is left out. Before that
	// day fifteen spells had no book and could not be handed over by any means at all.
	if (parameter.empty())
		return "Which spell level? setspells 0 forgets every book spell.";
	const uint8_t level = static_cast<uint8_t>(std::clamp(atoi(parameter.data()), 0, static_cast<int>(MaxSpellLevel)));
	// Forgetting is done here, at once: the queued CMD_CHANGE_SPELL_LEVEL learns as it sets the level, so every book
	// spell ended known at level 0 - and the clear below it ran first (round 13 audit, v1.12.238).
	if (level == 0) {
		for (int i = static_cast<int>(SpellID::Firebolt); i < MAX_SPELLS; i++) {
			if (GetSpellBookLevel(static_cast<SpellID>(i)) == -1)
				continue;
			MyPlayer->_pSplLvl[static_cast<size_t>(i)] = 0;
			MyPlayer->_pMemSpells &= ~GetSpellBitmask(static_cast<SpellID>(i));
		}
		return "Knowledge is power - and forgotten.";
	}
	// An int, not the uint8_t this was (audit, 2026-09-27): MAX_SPELLS is 290, and a uint8_t wraps at 255 before it gets
	// there - the command never returned and sent CMD_CHANGE_SPELL_LEVEL forever.
	for (int i = static_cast<int>(SpellID::Firebolt); i < MAX_SPELLS; i++) {
		if (GetSpellBookLevel(static_cast<SpellID>(i)) != -1) {
			NetSendCmdParam2(true, CMD_CHANGE_SPELL_LEVEL, static_cast<uint16_t>(i), level);
		}
	}
	return "Knowledge is power.";
}

std::string DebugCmdRefillHealthMana(const string_view parameter)
{
	Player &myPlayer = *MyPlayer;
	myPlayer.RestoreFullLife();
	myPlayer.RestoreFullMana();
	RedrawComponent(PanelDrawComponent::Health);
	RedrawComponent(PanelDrawComponent::Mana);

	return "Ready for more.";
}

std::string DebugCmdChangeHealth(const string_view parameter)
{
	Player &myPlayer = *MyPlayer;
	int change = -1;

	if (!parameter.empty())
		change = atoi(parameter.data());

	if (change == 0)
		return "Health hasn't changed.";

	int newHealth = myPlayer._pHitPoints + (change * 64);
	SetPlayerHitPoints(myPlayer, newHealth);
	if (newHealth <= 0)
		SyncPlrKill(myPlayer, DeathReason::MonsterOrTrap);

	return "Health has changed.";
}

std::string DebugCmdChangeMana(const string_view parameter)
{
	Player &myPlayer = *MyPlayer;
	int change = -1;

	if (!parameter.empty())
		change = atoi(parameter.data());

	if (change == 0)
		return "Mana hasn't changed.";

	int newMana = myPlayer._pMana + (change * 64);
	myPlayer._pMana = newMana;
	myPlayer._pManaBase = myPlayer._pMana + myPlayer._pMaxManaBase - myPlayer._pMaxMana;
	RedrawComponent(PanelDrawComponent::Mana);

	return "Mana has changed.";
}

std::string DebugCmdGenerateUniqueItem(const string_view parameter)
{
	return DebugSpawnUniqueItem(parameter.data());
}

std::string DebugCmdGenerateItem(const string_view parameter)
{
	return DebugSpawnItem(parameter.data());
}

std::string DebugCmdGenerateRareItem(const string_view parameter)
{
	return DebugSpawnTieredItem(parameter.data(), OracoolItemTier::Rare);
}

std::string DebugCmdGenerateBuffedUniqueItem(const string_view parameter)
{
	return DebugSpawnTieredItem(parameter.data(), OracoolItemTier::BuffedUnique);
}

std::string DebugCmdGeneratePrimalItem(const string_view parameter)
{
	return DebugSpawnTieredItem(parameter.data(), OracoolItemTier::Primal);
}

// Oracool: user request - one item for every equipment slot at once, at a chosen quality. Mainly
// for exercising the worn slots and the tiered set items, which have no loot-table presence and
// are otherwise only reachable one at a time by name through drop/givemagic and friends.
//
// The optional parameter is a material-tier name prefix ("givebset steel", "givemset diamond"):
// without it each slot spawns its first-in-table base (the leather tier), which is also all it
// COULD spawn before the prefix existed - the eight-tier set items all sit later in the table
// (user report: "all assets seem to be of the same type" - they were, structurally).
// Oracool: user request (2026-08-19) - the item families added since the tier work have no spawn
// path at all. `drop {name}` reaches an item by rerolling random drops until one matches, which is
// unreliable for a specific item and impossible for the IDROP_NEVER rows; these three go by index.
std::string DebugCmdGiveRunes(const string_view parameter)
{
	return DebugSpawnRunes();
}

std::string DebugCmdGiveGems(const string_view parameter)
{
	return DebugSpawnGems(parameter);
}

std::string DebugCmdGiveCharms(const string_view parameter)
{
	return DebugSpawnCharms();
}

// Oracool: a second way in, deliberately. When "the key does nothing" the question is whether the
// key never fired or the window never drew, and one command that toggles it directly splits that in
// two without a rebuild.
std::string DebugCmdRunewordBook(const string_view parameter)
{
	oracool::ToggleRunewordBook();
	return oracool::IsRunewordBookOpen() ? "Runeword book opened." : "Runeword book closed.";
}

std::string DebugCmdGiveEthereal(const string_view parameter)
{
	return DebugSpawnEthereal(parameter);
}

// Oracool (2026-09-19): one command per item type that drops ONE random item of it. Rare, Buffed
// Unique and Primal had theirs; these are the four that were missing.
std::string DebugCmdGiveBasic(const string_view parameter)
{
	return DebugSpawnQualityItem(std::string(parameter), ITEM_QUALITY_NORMAL);
}

std::string DebugCmdGiveMagic(const string_view parameter)
{
	return DebugSpawnQualityItem(std::string(parameter), ITEM_QUALITY_MAGIC);
}

std::string DebugCmdGiveSetPiece(const string_view parameter)
{
	return DebugSpawnSetPiece(parameter);
}

std::string DebugCmdGiveRuneword(const string_view parameter)
{
	return DebugSpawnRuneword(parameter);
}

std::string DebugCmdGiveSockets(const string_view parameter)
{
	return DebugSpawnSocketedBase(parameter);
}

std::string DebugCmdGiveBasicSet(const string_view parameter)
{
	return DebugSpawnEquipmentSet(std::nullopt, /*magical=*/false, parameter);
}

std::string DebugCmdGiveMagicSet(const string_view parameter)
{
	return DebugSpawnEquipmentSet(std::nullopt, /*magical=*/true, parameter);
}

std::string DebugCmdGiveRareSet(const string_view parameter)
{
	return DebugSpawnEquipmentSet(OracoolItemTier::Rare, /*magical=*/true, parameter);
}

std::string DebugCmdGiveBuffedUniqueSet(const string_view parameter)
{
	return DebugSpawnEquipmentSet(OracoolItemTier::BuffedUnique, /*magical=*/true, parameter);
}

std::string DebugCmdGivePrimalSet(const string_view parameter)
{
	return DebugSpawnEquipmentSet(OracoolItemTier::Primal, /*magical=*/true, parameter);
}

/**
 * @brief `giveitemset N` - every spawnable piece of item set N (1-15) into the backpack.
 *
 * The sets are not in the drop tables yet, so this is currently the ONLY way to see one. It reports
 * what it could not give as well as what it could: thirty of the ninety-four items sit on slots this
 * fork has no base item for (amulet, ring, relic, cloak), and silently handing over four pieces of a
 * six-piece set would look like a bug rather than a boundary.
 */
std::string DebugCmdGiveItemSet(const string_view parameter)
{
	int index = 0;
	if (!parameter.empty())
		index = atoi(std::string(parameter).c_str());
	if (index < 1 || index > static_cast<int>(oracool::ItemSetCount))
		return fmt::format("Pick a set from 1 to {:d}.", oracool::ItemSetCount);

	const oracool::ItemSetDefinition &set = oracool::ItemSets[index - 1];
	Player &myPlayer = *MyPlayer;
	int given = 0;
	int noBase = 0;
	int noRoom = 0;
	for (int i = 0; i < set.itemCount; i++) {
		const oracool::SetItemDefinition &def = oracool::ItemSetItems[set.firstItem + i];
		const int base = oracool::BaseItemForSetPiece(def);
		if (base < 0) {
			noBase++;
			continue;
		}
		Item item {};
		InitializeItem(item, static_cast<_item_indexes>(base));
		oracool::MakeSetItem(item, def);
		// The finish every real set piece gets - a seed, an item level, a base tier (round 13 audit: seed 0 made two
		// pieces on one base the same item to the pickup filter).
		FinalizeSetPiece(item, std::max<int>(def.requiredLevel, myPlayer._pLevel), /*allowEtherealRoll=*/false);
		item._iStatFlag = myPlayer.CanUseItem(item);
		if (!AutoPlaceItemInInventory(myPlayer, item, true)) {
			noRoom++;
			continue;
		}
		given++;
	}
	std::string result = fmt::format("{:s}: gave {:d} of {:d}", _(set.name), given, set.itemCount);
	if (noBase > 0)
		result += fmt::format(", {:d} have no base item for their slot", noBase);
	if (noRoom > 0)
		result += fmt::format(", {:d} did not fit", noRoom);
	return result;
}

/**
 * @brief `givesset` - one SET item per equipment slot, drawn from across the fifteen named sets.
 *
 * User request, 2026-08-20: "give as full as possible set of set and etherial items so i can test
 * them."
 *
 * Deliberately NOT DebugSpawnEquipmentSet(OracoolItemTier::Set, ...) like its five siblings. Those
 * ask the affix roller for a tier, and Set is the one tier that is not a roll - a set piece is a
 * specific named object with a fixed stat list (see OracoolItemTier::Set's own comment). Asking the
 * roller for one would produce a Set-coloured item that belongs to no set, which is worse than
 * useless for testing sets.
 *
 * So it walks the real table and takes the FIRST piece that fits each slot. The result is a mongrel
 * - thirteen pieces from up to thirteen different sets - which is exactly right for testing that
 * every slot renders, colours and equips, and exactly wrong for testing set BONUSES. `giveitemset N`
 * stays the command for one coherent set.
 */
std::string DebugCmdGiveSetSet(const string_view parameter)
{
	Player &myPlayer = *MyPlayer;
	int given = 0;
	int noRoom = 0;
	// Keyed on the slot WORD, because SetItemDefinition::slot is the design's string ("helm",
	// "main_hand", ...) and not an engine enum - see its declaration.
	std::set<std::string> covered;

	for (size_t s = 0; s < oracool::ItemSetCount; s++) {
		const oracool::ItemSetDefinition &set = oracool::ItemSets[s];
		for (int i = 0; i < set.itemCount; i++) {
			const oracool::SetItemDefinition &def = oracool::ItemSetItems[set.firstItem + i];
			// One per slot: the first set that has a piece for it wins, so the walk is stable and
			// a second Helm never displaces the first.
			if (covered.count(def.slot) > 0)
				continue;
			const int base = oracool::BaseItemForSetPiece(def);
			if (base < 0)
				continue; // no base item for that slot in this fork - giveitemset reports these
			Item item {};
			InitializeItem(item, static_cast<_item_indexes>(base));
			oracool::MakeSetItem(item, def);
			FinalizeSetPiece(item, std::max<int>(def.requiredLevel, myPlayer._pLevel), /*allowEtherealRoll=*/false); // round 13
			item._iStatFlag = myPlayer.CanUseItem(item);
			if (!AutoPlaceItemInInventory(myPlayer, item, true)) {
				noRoom++;
				continue;
			}
			covered.insert(def.slot);
			given++;
		}
	}

	std::string result = fmt::format("Gave {:d} set items, one per slot, from across the {:d} sets",
	    given, static_cast<int>(oracool::ItemSetCount));
	if (noRoom > 0)
		result += fmt::format(" - {:d} did not fit", noRoom);
	result += ". These are from DIFFERENT sets: use giveitemset N for one whole set.";
	return result;
}

/**
 * @brief `giveeset` - one ETHEREAL item per equipment slot.
 *
 * Ethereal is not a quality, it is a stamp applied on top of one - so this spawns the ordinary
 * per-slot set and then makes each piece ethereal, which is the same order the drop path uses.
 * MakeItemEthereal carries the whole bargain (+35% AC or max damage, max durability halved), so
 * nothing here has to know what ethereal means.
 *
 * Magic-quality bases rather than plain: an ethereal white has almost nothing to show, and the
 * point of the command is to see the stamp against real numbers.
 */
std::string DebugCmdGiveEtherealSet(const string_view parameter)
{
	return DebugSpawnEquipmentSet(std::nullopt, /*magical=*/true, parameter, /*ethereal=*/true);
}

// Oracool: Megaplan Phase 0.8 - the tile-matrix export half of the zone iteration loop. The
// engine dumps WHAT the generator laid out (dPiece indices); the offline tileset tools composite
// HOW it looks. Together they let a generated zone be inspected without anyone launching a client.
std::string DebugCmdDumpDungeon(const string_view parameter)
{
	const std::string path = paths::PrefPath() + fmt::format("dungeon_dump_l{:02d}.csv", currlevel);
	FILE *file = OpenFile(path.c_str(), "wb"); // UTF-8 pref path (round 4 audit)
	if (file == nullptr)
		return "Could not open the dump file.";
	const std::string header = fmt::format("# level {:d} type {:d} size {:d}x{:d}\n",
	    currlevel, static_cast<int>(leveltype), MAXDUNX, MAXDUNY);
	std::fwrite(header.data(), header.size(), 1, file);
	for (int y = 0; y < MAXDUNY; y++) {
		std::string row;
		for (int x = 0; x < MAXDUNX; x++) {
			if (x > 0)
				row += ',';
			row += fmt::format("{:d}", dPiece[x][y]);
		}
		row += '\n';
		std::fwrite(row.data(), row.size(), 1, file);
	}
	std::fclose(file);
	return fmt::format("Dumped to dungeon_dump_l{:02d}.csv", currlevel);
}

// Oracool: Megaplan Phase 0.8 - the other half of the art iteration loop: edit a ui\*.png, run
// this, see the change. See oracool::ResetHudArtCaches.
std::string DebugCmdReloadAssets(const string_view parameter)
{
	oracool::ResetHudArtCaches();
	return "HUD art caches dropped - every ui\\*.png reloads on its next draw.";
}

std::string DebugCmdExit(const string_view parameter)
{
	gbRunGame = false;
	gbRunGameResult = false;
	return "See you again my Lord.";
}

std::string DebugCmdArrow(const string_view parameter)
{
	Player &myPlayer = *MyPlayer;

	myPlayer._pIFlags &= ~ItemSpecialEffect::FireArrows;
	myPlayer._pIFlags &= ~ItemSpecialEffect::LightningArrows;

	if (parameter == "normal") {
		// we removed the parameter at the top
	} else if (parameter == "fire") {
		myPlayer._pIFlags |= ItemSpecialEffect::FireArrows;
	} else if (parameter == "lightning") {
		myPlayer._pIFlags |= ItemSpecialEffect::LightningArrows;
	} else if (parameter == "explosion") {
		myPlayer._pIFlags |= (ItemSpecialEffect::FireArrows | ItemSpecialEffect::LightningArrows);
	} else {
		return "Unknown is sometimes similar to nothing (unkown effect).";
	}

	return "I can shoot any arrow.";
}

std::string DebugCmdTalkToTowner(const string_view parameter)
{
	if (DebugTalkToTowner(parameter.data())) {
		return "Hello from the other side.";
	}
	return "NPC not found.";
}

std::string DebugCmdShowGrid(const string_view parameter)
{
	DebugGrid = !DebugGrid;
	if (DebugGrid)
		return "A basket full of rectangles and mushrooms.";

	return "Back to boring.";
}

std::string DebugCmdHideUi(const string_view parameter)
{
	DebugHideUi = !DebugHideUi;
	RedrawEverything();
	if (DebugHideUi)
		return "HUD hidden - dungeon view only.";

	return "HUD restored.";
}

std::string DebugCmdClearUi(const string_view parameter)
{
	DebugClearUi = !DebugClearUi;
	RedrawEverything();
	if (DebugClearUi)
		return "Everything's hidden - clean shot.";

	return "HUD restored.";
}

std::string DebugCmdSpawnUniqueMonster(const string_view parameter)
{
	if (leveltype == DTYPE_TOWN)
		return "Do you want to kill the towners?!?";

	std::string name;
	int count = 1;
	for (string_view arg : SplitByChar(parameter, ' ')) {
		const int num = atoi(std::string(arg).c_str());
		if (num > 0) {
			count = num;
			break;
		}
		AppendStrView(name, arg);
		name += ' ';
	}
	if (name.empty())
		return "Monster name cannot be empty. Duh.";

	name.pop_back(); // remove last space
	AsciiStrToLower(name);

	int mtype = -1;
	UniqueMonsterType uniqueIndex = UniqueMonsterType::None;
	for (size_t i = 0; UniqueMonstersData[i].mtype != MT_INVALID; i++) {
		auto mondata = UniqueMonstersData[i];
		const std::string monsterName = AsciiStrToLower(mondata.mName);
		if (monsterName.find(name) == std::string::npos)
			continue;
		mtype = mondata.mtype;
		uniqueIndex = static_cast<UniqueMonsterType>(i);
		if (monsterName == name) // to support partial name matching but always choose the correct monster if full name is given
			break;
	}

	if (mtype == -1)
		return "Monster not found!";

	size_t id = MaxLvlMTypes - 1;
	bool found = false;

	for (size_t i = 0; i < LevelMonsterTypeCount; i++) {
		if (LevelMonsterTypes[i].type == mtype) {
			id = i;
			found = true;
			break;
		}
	}

	if (!found) {
		CMonster &monsterType = LevelMonsterTypes[id];
		monsterType.type = static_cast<_monster_id>(mtype);
		InitMonsterGFX(monsterType);
		InitMonsterSND(monsterType);
		monsterType.placeFlags |= PLACE_SCATTER;
		monsterType.corpseId = 1;
	}

	Player &myPlayer = *MyPlayer;

	int spawnedMonster = 0;

	auto ret = Crawl(0, MaxCrawlRadius, [&](Displacement displacement) -> std::optional<std::string> {
		Point pos = myPlayer.position.tile + displacement;
		if (dPlayer[pos.x][pos.y] != 0 || dMonster[pos.x][pos.y] != 0)
			return {};
		if (!IsTileWalkable(pos))
			return {};

		Monster *monster = AddMonster(pos, myPlayer._pdir, id, true);
		if (monster == nullptr)
			return StrCat("I could only summon ", spawnedMonster, " Monsters. The rest strike for shorter working hours.");
		PrepareUniqueMonst(*monster, uniqueIndex, 0, 0, UniqueMonstersData[static_cast<size_t>(uniqueIndex)]);
		monster->corpseId = 1;
		spawnedMonster += 1;

		if (spawnedMonster >= count)
			return "Let the fighting begin!";

		return {};
	});

	if (!ret)
		ret = StrCat("I could only summon ", spawnedMonster, " Monsters. The rest strike for shorter working hours.");
	return *ret;
}

std::string DebugCmdSpawnMonster(const string_view parameter)
{
	if (leveltype == DTYPE_TOWN)
		return "Do you want to kill the towners?!?";

	std::string name;
	int count = 1;
	for (string_view arg : SplitByChar(parameter, ' ')) {
		const int num = atoi(std::string(arg).c_str());
		if (num > 0) {
			count = num;
			break;
		}
		AppendStrView(name, arg);
		name += ' ';
	}
	if (name.empty())
		return "Monster name cannot be empty. Duh.";

	name.pop_back(); // remove last space
	AsciiStrToLower(name);

	int mtype = -1;

	for (int i = 0; i < NUM_MTYPES; i++) {
		auto mondata = MonstersData[i];
		const std::string monsterName = AsciiStrToLower(mondata.name);
		if (monsterName.find(name) == std::string::npos)
			continue;
		mtype = i;
		if (monsterName == name) // to support partial name matching but always choose the correct monster if full name is given
			break;
	}

	if (mtype == -1)
		return "Monster not found!";

	size_t id = MaxLvlMTypes - 1;
	bool found = false;

	for (size_t i = 0; i < LevelMonsterTypeCount; i++) {
		if (LevelMonsterTypes[i].type == mtype) {
			id = i;
			found = true;
			break;
		}
	}

	if (!found) {
		CMonster &monsterType = LevelMonsterTypes[id];
		monsterType.type = static_cast<_monster_id>(mtype);
		InitMonsterGFX(monsterType);
		InitMonsterSND(monsterType);
		monsterType.placeFlags |= PLACE_SCATTER;
		monsterType.corpseId = 1;
	}

	Player &myPlayer = *MyPlayer;

	int spawnedMonster = 0;

	auto ret = Crawl(0, MaxCrawlRadius, [&](Displacement displacement) -> std::optional<std::string> {
		Point pos = myPlayer.position.tile + displacement;
		if (dPlayer[pos.x][pos.y] != 0 || dMonster[pos.x][pos.y] != 0)
			return {};
		if (!IsTileWalkable(pos))
			return {};

		if (AddMonster(pos, myPlayer._pdir, id, true) == nullptr)
			return StrCat("I could only summon ", spawnedMonster, " Monsters. The rest strike for shorter working hours.");
		spawnedMonster += 1;

		if (spawnedMonster >= count)
			return "Let the fighting begin!";

		return {};
	});

	if (!ret)
		ret = StrCat("I could only summon ", spawnedMonster, " Monsters. The rest strike for shorter working hours.");
	return *ret;
}

std::string DebugCmdShowTileData(const string_view parameter)
{
	std::string paramList[] = {
		"dPiece",
		"dTransVal",
		"dLight",
		"dPreLight",
		"dFlags",
		"dPlayer",
		"dMonster",
		"dCorpse",
		"dObject",
		"dItem",
		"dSpecial",
		"coords",
		"cursorcoords",
		"objectindex",
		"solid",
		"transparent",
		"trap",
		"AutomapView",
		"dungeon",
		"pdungeon",
		"Protected",
	};

	if (parameter == "clear") {
		SelectedDebugGridTextItem = DebugGridTextItem::None;
		return "Tile data cleared!";
	}
	if (parameter == "") {
		std::string list = "clear";
		for (const auto &param : paramList) {
			list += " / " + param;
		}
		return list;
	}
	bool found = false;
	int index = 0;
	for (const auto &param : paramList) {
		index++;
		if (parameter != param)
			continue;
		found = true;
		auto newGridText = static_cast<DebugGridTextItem>(index);
		if (newGridText == SelectedDebugGridTextItem) {
			SelectedDebugGridTextItem = DebugGridTextItem::None;
			return "Tile data toggled... now you see nothing.";
		}
		SelectedDebugGridTextItem = newGridText;
		break;
	}
	if (!found)
		return "Invalid name. Check names using tiledata command.";

	return "Special powers activated.";
}

std::string DebugCmdScrollView(const string_view parameter)
{
	DebugScrollViewEnabled = !DebugScrollViewEnabled;
	if (DebugScrollViewEnabled)
		return "You can see as far as an eagle.";
	InitMultiView();
	return "If you want to see the world, you need to explore it yourself.";
}

std::string DebugCmdItemInfo(const string_view parameter)
{
	Player &myPlayer = *MyPlayer;
	Item *pItem = nullptr;
	if (!myPlayer.HoldItem.isEmpty()) {
		pItem = &myPlayer.HoldItem;
	} else if (pcursinvitem != -1) {
		if (pcursinvitem <= INVITEM_INV_LAST)
			pItem = &myPlayer.InvList[pcursinvitem - INVITEM_INV_FIRST];
		else
			pItem = &myPlayer.SpdList[pcursinvitem - INVITEM_BELT_FIRST];
	} else if (pcursitem != -1) {
		pItem = &Items[pcursitem];
	}
	if (pItem != nullptr) {
		string_view netPackValidation { "N/A" };
		if (gbIsMultiplayer) {
			ItemNetPack itemPack;
			Item unpacked;
			PackNetItem(*pItem, itemPack);
			netPackValidation = UnPackNetItem(myPlayer, itemPack, unpacked) ? "Success" : "Failure";
		}
		return StrCat("Name: ", pItem->_iIName,
		    "\nIDidx: ", pItem->IDidx, " (", AllItemsList[pItem->IDidx].iName, ")",
		    "\nSeed: ", pItem->_iSeed,
		    "\nCreateInfo: ", pItem->_iCreateInfo,
		    "\nLevel: ", pItem->_iCreateInfo & CF_LEVEL,
		    "\nOnly Good: ", ((pItem->_iCreateInfo & CF_ONLYGOOD) == 0) ? "False" : "True",
		    "\nUnique Monster: ", ((pItem->_iCreateInfo & CF_UPER15) == 0) ? "False" : "True",
		    "\nDungeon Item: ", ((pItem->_iCreateInfo & CF_UPER1) == 0) ? "False" : "True",
		    "\nUnique Item: ", ((pItem->_iCreateInfo & CF_UNIQUE) == 0) ? "False" : "True",
		    "\nSmith: ", ((pItem->_iCreateInfo & CF_SMITH) == 0) ? "False" : "True",
		    "\nSmith Premium: ", ((pItem->_iCreateInfo & CF_SMITHPREMIUM) == 0) ? "False" : "True",
		    "\nBoy: ", ((pItem->_iCreateInfo & CF_BOY) == 0) ? "False" : "True",
		    "\nWitch: ", ((pItem->_iCreateInfo & CF_WITCH) == 0) ? "False" : "True",
		    "\nHealer: ", ((pItem->_iCreateInfo & CF_HEALER) == 0) ? "False" : "True",
		    "\nPregen: ", ((pItem->_iCreateInfo & CF_PREGEN) == 0) ? "False" : "True",
		    "\nNet Validation: ", netPackValidation);
	}
	return StrCat("Numitems: ", ActiveItemCount);
}

std::string DebugCmdQuestInfo(const string_view parameter)
{
	if (parameter.empty()) {
		std::string ret = "You must provide an id. This could be:";
		for (auto &quest : Quests) {
			if (IsNoneOf(quest._qactive, QUEST_NOTAVAIL, QUEST_INIT))
				continue;
			StrAppend(ret, ", ", quest._qidx, " (", QuestsData[quest._qidx]._qlstr, ")");
		}
		return ret;
	}

	int questId = atoi(parameter.data());

	if (questId < 0 || questId >= MAXQUESTS) // negative too (round 13 audit)
		return StrCat("Quest ", questId, " is not known. Do you want to write a mod?");
	auto &quest = Quests[questId];
	return StrCat("\nQuest: ", QuestsData[quest._qidx]._qlstr, "\nActive: ", quest._qactive, " Var1: ", quest._qvar1, " Var2: ", quest._qvar2);
}

std::string DebugCmdPlayerInfo(const string_view parameter)
{
	int playerId = atoi(parameter.data());
	if (static_cast<size_t>(playerId) >= Players.size())
		return "My friend, we need a valid playerId.";
	Player &player = Players[playerId];
	if (!player.plractive)
		return "Player is not active";

	const Point target = player.GetTargetPosition();
	return StrCat("Plr ", playerId, " is ", player._pName,
	    "\nLvl: ", player.plrlevel, " Changing: ", player._pLvlChanging,
	    "\nTile.x: ", player.position.tile.x, " Tile.y: ", player.position.tile.y, " Target.x: ", target.x, " Target.y: ", target.y,
	    "\nMode: ", player._pmode, " destAction: ", player.destAction, " walkpath[0]: ", player.walkpath[0],
	    "\nInvincible:", player._pInvincible ? 1 : 0, " HitPoints:", player._pHitPoints);
}

std::string DebugCmdToggleFPS(const string_view parameter)
{
	frameflag = !frameflag;
	return "";
}

std::string DebugCmdChangeTRN(const string_view parameter)
{
	std::string out;
	const auto parts = SplitByChar(parameter, ' ');
	auto it = parts.begin();
	if (it != parts.end()) {
		const string_view first = *it;
		if (++it != parts.end()) {
			const string_view second = *it;
			string_view prefix;
			if (first == "mon") {
				prefix = "monsters\\monsters\\";
			} else if (first == "plr") {
				prefix = "plrgfx\\";
			}
			debugTRN = StrCat(prefix, second, ".trn");
		} else {
			debugTRN = StrCat(first, ".trn");
		}
		out = fmt::format("I am a pretty butterfly. \n(Loading TRN: {:s})", debugTRN);
	} else {
		debugTRN = "";
		out = "I am a big brown potato.";
	}
	auto &player = *MyPlayer;
	InitPlayerGFX(player);
	StartStand(player, player._pdir);
	return out;
}

std::string DebugCmdSearchMonster(const string_view parameter)
{
	if (parameter.empty()) {
		std::string ret = "What should I search? I'm too lazy to search for everything... you must provide a monster name!";
		return ret;
	}

	std::string name;
	AppendStrView(name, parameter);
	AsciiStrToLower(name);
	SearchMonsters.push_back(name);

	return "We will find this bastard!";
}

std::string DebugCmdSearchItem(const string_view parameter)
{
	if (parameter.empty()) {
		std::string ret = "What should I search? I'm too lazy to search for everything... you must provide a item name!";
		return ret;
	}

	std::string name;
	AppendStrView(name, parameter);
	AsciiStrToLower(name);
	SearchItems.push_back(name);

	return "Are you greedy? Anyway I will help you.";
}

std::string DebugCmdSearchObject(const string_view parameter)
{
	if (parameter.empty()) {
		std::string ret = "What should I search? I'm too lazy to search for everything... you must provide a object name!";
		return ret;
	}

	std::string name;
	AppendStrView(name, parameter);
	AsciiStrToLower(name);
	SearchObjects.push_back(name);

	return "I will look for the pyramids. Oh sorry, I'm looking for what you want, of course.";
}

std::string DebugCmdClearSearch(const string_view parameter)
{
	SearchMonsters.clear();
	SearchItems.clear();
	SearchObjects.clear();

	return "Now you have to find it yourself.";
}

std::vector<DebugCmdItem> DebugCmdList = {
	{ "help", "Prints help overview or help for a specific command.", "({command})", &DebugCmdHelp },
	{ "givegold", "Fills the inventory with gold.", "", &DebugCmdGiveGoldCheat },
	{ "givexp", "Levels the player up (min 1 level or {levels}).", "({levels})", &DebugCmdLevelUp },
	{ "givewp", "Unlocks every waypoint (1-24) on the current difficulty.", "", &DebugCmdGiveWaypoints },
	{ "maxstats", "Sets all stat values to maximum.", "", &DebugCmdMaxStats },
	{ "minstats", "Sets all stat values to minimum.", "", &DebugCmdMinStats },
	// Oracool: reworded 2026-08-15. It said "Set spell level to {level} for all spells", which
	// undersold it - OnChangeSpellLevel also sets _pMemSpells, so this LEARNS every spell it touches
	// rather than only levelling ones already known. The user went looking for a separate "teach me
	// all spells" command because the help did not say so.
	{ "setspells", "Learn every spell that has a book, at spell level {level}. 0 forgets them all again.", "{level}", &DebugCmdSetSpellsLevel },
	{ "takegold", "Removes all gold from inventory.", "", &DebugCmdTakeGoldCheat },
	{ "givequest", "Enable a given quest.", "({id})", &DebugCmdQuest },
	{ "givemap", "Reveal the map.", "", &DebugCmdMapReveal },
	{ "takemap", "Hide the map.", "", &DebugCmdMapHide },
	{ "goto", "Moves to specifided {level} (use 0 for town).", "{level}", &DebugCmdWarpToLevel },
	{ "questmap", "Load a quest level {level}.", "{level}", &DebugCmdLoadQuestMap },
	{ "map", "Load custom level from a given {path}.dun.", "{path} {type} {x} {y}", &DebugCmdLoadMap },
	{ "exportdun", "Save the current level as a dun-file.", "", &ExportDun },
	{ "visit", "Visit a towner.", "{towner}", &DebugCmdVisitTowner },
	{ "restart", "Resets specified {level}.", "{level} ({seed})", &DebugCmdResetLevel },
	{ "god", "Toggles godmode.", "", &DebugCmdGodMode },
	{ "army", "Raises a debug army of minions; a number raises that many, 0 dismisses it.", "({count})", &DebugCmdArmy },
	{ "drawvision", "Toggles vision debug rendering.", "", &DebugCmdVision },
	{ "drawpath", "Toggles path debug rendering.", "", &DebugCmdPath },
	{ "fullbright", "Toggles whether light shading is in effect.", "", &DebugCmdLighting },
	{ "fill", "Refills health and mana.", "", &DebugCmdRefillHealthMana },
	{ "changehp", "Changes health by {value} (Use a negative value to remove health).", "{value}", &DebugCmdChangeHealth },
	{ "changemp", "Changes mana by {value} (Use a negative value to remove mana).", "{value}", &DebugCmdChangeMana },
	{ "dropu", "Attempts to generate unique item {name}; no name is a random unique.", "({name})", &DebugCmdGenerateUniqueItem },
	{ "drop", "Attempts to generate item {name}.", "{name}", &DebugCmdGenerateItem },
	{ "givebasic", "Generates one random Basic (white) worn or wielded item, optionally matching {name}.", "({name})", &DebugCmdGiveBasic },
	{ "givemagic", "Generates one random Magic item with no Orcl tier, optionally matching {name}.", "({name})", &DebugCmdGiveMagic },
	{ "giverare", "Attempts to generate a Rare-tier item, optionally matching {name}.", "({name})", &DebugCmdGenerateRareItem },
	{ "giveunique", "Attempts to generate a Buffed Unique-tier item, optionally matching {name}.", "({name})", &DebugCmdGenerateBuffedUniqueItem },
	{ "giveprimal", "Attempts to generate a Primal-tier item, optionally matching {name}.", "({name})", &DebugCmdGeneratePrimalItem },
	{ "givebset", "Drops a Basic item for each of the 13 equipment slots, optionally of material {tier} (leather/iron/steel/crusader/bone/royal/obsidian/infernal/diamond).", "({tier})", &DebugCmdGiveBasicSet },
	{ "givemset", "Drops a Magic item for each of the 13 equipment slots, optionally of material {tier}.", "({tier})", &DebugCmdGiveMagicSet },
	{ "giverset", "Drops a Rare item for each of the 13 equipment slots, optionally of material {tier}.", "({tier})", &DebugCmdGiveRareSet },
	{ "giveuset", "Drops a Buffed Unique item for each of the 13 equipment slots, optionally of material {tier}.", "({tier})", &DebugCmdGiveBuffedUniqueSet },
	{ "givepset", "Drops a Primal item for each of the 13 equipment slots, optionally of material {tier}.", "({tier})", &DebugCmdGivePrimalSet },
	{ "givesset", "Gives one SET item per equipment slot, taken from across the 15 named sets - use giveitemset {n} for one whole set.", "", &DebugCmdGiveSetSet },
	{ "giveeset", "Drops an ETHEREAL magic item for each of the 13 equipment slots, optionally of material {tier}.", "({tier})", &DebugCmdGiveEtherealSet },
	{ "giverunes", "Drops all 33 runes.", "", &DebugCmdGiveRunes },
	{ "givegems", "Drops every gem, or only quality {q} (chipped/flawed/normal/flawless/perfect).", "({q})", &DebugCmdGiveGems },
	{ "runewords", "Toggles the runeword book.", "", &DebugCmdRunewordBook },
	{ "giveethereal", "Spawns one random ethereal item, optionally matching {name}.", "({name})", &DebugCmdGiveEthereal },
	{ "givesockets", "Spawns one random basic base with {n} empty sockets (default: as many as it holds), optionally named {name}.", "({n}) ({name})", &DebugCmdGiveSockets },
	{ "giveset", "Drops one random piece of a named set, optionally matching a piece or set {name} - giveitemset {n} for a whole set.", "({name})", &DebugCmdGiveSetPiece },
	{ "giverw", "Drops one formed runeword on a random fitting base, optionally the word matching {name}.", "({name})", &DebugCmdGiveRuneword },
	{ "givecharms", "Drops every charm.", "", &DebugCmdGiveCharms },
	{ "giveitemset", "Gives every spawnable piece of named item set {n} (1-15).", "{n}", &DebugCmdGiveItemSet },
	{ "talkto", "Interacts with a NPC whose name contains {name}.", "{name}", &DebugCmdTalkToTowner },
	{ "exit", "Exits the game.", "", &DebugCmdExit },
	{ "dumpdungeon", "Writes the current level's tile matrix to a CSV beside the saves.", "", &DebugCmdDumpDungeon },
	{ "reloadassets", "Drops cached ui\\*.png art so edits show without restarting.", "", &DebugCmdReloadAssets },
	{ "arrow", "Changes arrow effect (normal, fire, lightning, explosion).", "{effect}", &DebugCmdArrow },
	{ "grid", "Toggles showing grid.", "", &DebugCmdShowGrid },
	{ "hideui", "Toggles hiding the main HUD (panel, orbs, belt, buttons, XP bar) for clean screenshots.", "", &DebugCmdHideUi },
	{ "clearui", "Toggles hiding every HUD element (panel, mini-map, overlays, cursor, etc.) for a fully clean screenshot.", "", &DebugCmdClearUi },
	{ "spawnu", "Spawns unique monster {name}.", "{name} ({count})", &DebugCmdSpawnUniqueMonster },
	{ "spawn", "Spawns monster {name}.", "{name} ({count})", &DebugCmdSpawnMonster },
	{ "tiledata", "Toggles showing tile data {name} (leave name empty to see a list).", "{name}", &DebugCmdShowTileData },
	{ "scrollview", "Toggles scroll view feature (with shift+mouse).", "", &DebugCmdScrollView },
	{ "iteminfo", "Shows info of currently selected item.", "", &DebugCmdItemInfo },
	{ "questinfo", "Shows info of quests.", "{id}", &DebugCmdQuestInfo },
	{ "playerinfo", "Shows info of player.", "{playerid}", &DebugCmdPlayerInfo },
	{ "fps", "Toggles displaying FPS", "", &DebugCmdToggleFPS },
	{ "trn", "Makes player use TRN {trn} - Write 'plr' before it to look in plrgfx\\ or 'mon' to look in monsters\\monsters\\ - example: trn plr infra is equal to 'plrgfx\\infra.trn'", "{trn}", &DebugCmdChangeTRN },
	{ "searchmonster", "Searches the automap for {monster}", "{monster}", &DebugCmdSearchMonster },
	{ "searchitem", "Searches the automap for {item}", "{item}", &DebugCmdSearchItem },
	{ "searchobject", "Searches the automap for {object}", "{object}", &DebugCmdSearchObject },
	{ "clearsearch", "Search in the auto map is cleared", "", &DebugCmdClearSearch },
	{ "rift", "Opens a rift at the Rift Monument and enters it: nephalem (free, the deepest floor's tier) or guardian at {tier}.", "{nephalem|guardian} ({tier})", &DebugCmdRift },
	{ "keystone", "Drops a Guardian Keystone at your feet: of {tier}, or of the deepest floor's tier when none is given.", "({tier})", &DebugCmdKeystone },
};

} // namespace

void LoadDebugGFX()
{
	pSquareCel = LoadCel("data\\square", 64);
}

void FreeDebugGFX()
{
	pSquareCel = std::nullopt;
}

void GetDebugMonster()
{
	int monsterIndex = pcursmonst;
	if (monsterIndex == -1)
		monsterIndex = abs(dMonster[cursPosition.x][cursPosition.y]) - 1;

	if (monsterIndex == -1)
		monsterIndex = DebugMonsterId;

	PrintDebugMonster(Monsters[monsterIndex]);
}

void NextDebugMonster()
{
	DebugMonsterId++;
	if (DebugMonsterId == MaxMonsters)
		DebugMonsterId = 0;

	EventPlrMsg(StrCat("Current debug monster = ", DebugMonsterId), UiFlags::ColorWhite);
}

void SetDebugLevelSeedInfos(uint32_t mid1Seed, uint32_t mid2Seed, uint32_t mid3Seed, uint32_t endSeed)
{
	glMid1Seed[currlevel] = mid1Seed;
	glMid2Seed[currlevel] = mid2Seed;
	glMid3Seed[currlevel] = mid3Seed;
	glEndSeed[currlevel] = endSeed;
}

bool CheckDebugTextCommand(const string_view text)
{
	auto debugCmdIterator = std::find_if(DebugCmdList.begin(), DebugCmdList.end(), [&](const DebugCmdItem &elem) { return text.find(elem.text) == 0 && (text.length() == elem.text.length() || text[elem.text.length()] == ' '); });
	if (debugCmdIterator == DebugCmdList.end())
		return false;

	auto &dbgCmd = *debugCmdIterator;
	string_view parameter = "";
	if (text.length() > (dbgCmd.text.length() + 1))
		parameter = text.substr(dbgCmd.text.length() + 1);
	// Telemetry: a session that used the console is not a session about BALANCE, and the analysis
	// has to be able to tell (2026-08-21). The first read-back of the CSV could not: it showed 39
	// Primal pickups against 16 Rare, which inverts the rarity ladder and looks alarming until you
	// remember the session had run givepset and giveitemset. Debug spawns and real drops were the
	// same row.
	//
	// Recorded BEFORE the command runs, so a command that crashes still leaves its mark - which is
	// exactly the session whose data you would most want to discard.
	oracool::TelemetryRecordDebugCommand(std::string(dbgCmd.text));
	const auto result = dbgCmd.actionProc(parameter);
	Log("DebugCmd: {} Result: {}", text, result);
	if (result != "")
		EventPlrMsg(result, UiFlags::ColorRed);
	return true;
}

bool IsDebugGridTextNeeded()
{
	return SelectedDebugGridTextItem != DebugGridTextItem::None;
}

bool IsDebugGridInMegatiles()
{
	switch (SelectedDebugGridTextItem) {
	case DebugGridTextItem::AutomapView:
	case DebugGridTextItem::dungeon:
	case DebugGridTextItem::pdungeon:
	case DebugGridTextItem::Protected:
		return true;
	default:
		return false;
	}
}

bool GetDebugGridText(Point dungeonCoords, char *debugGridTextBuffer)
{
	int info = 0;
	int blankValue = 0;
	Point megaCoords = dungeonCoords.worldToMega();
	switch (SelectedDebugGridTextItem) {
	case DebugGridTextItem::coords:
		*BufCopy(debugGridTextBuffer, dungeonCoords.x, ":", dungeonCoords.y) = '\0';
		return true;
	case DebugGridTextItem::cursorcoords:
		if (dungeonCoords != cursPosition)
			return false;
		*BufCopy(debugGridTextBuffer, dungeonCoords.x, ":", dungeonCoords.y) = '\0';
		return true;
	case DebugGridTextItem::objectindex: {
		info = 0;
		Object *object = FindObjectAtPosition(dungeonCoords);
		if (object != nullptr) {
			info = static_cast<int>(object->_otype);
		}
		break;
	}
	case DebugGridTextItem::dPiece:
		info = dPiece[dungeonCoords.x][dungeonCoords.y];
		break;
	case DebugGridTextItem::dTransVal:
		info = dTransVal[dungeonCoords.x][dungeonCoords.y];
		break;
	case DebugGridTextItem::dLight:
		info = dLight[dungeonCoords.x][dungeonCoords.y];
		blankValue = LightsMax;
		break;
	case DebugGridTextItem::dPreLight:
		info = dPreLight[dungeonCoords.x][dungeonCoords.y];
		blankValue = LightsMax;
		break;
	case DebugGridTextItem::dFlags:
		info = static_cast<int>(dFlags[dungeonCoords.x][dungeonCoords.y]);
		break;
	case DebugGridTextItem::dPlayer:
		info = dPlayer[dungeonCoords.x][dungeonCoords.y];
		break;
	case DebugGridTextItem::dMonster:
		info = dMonster[dungeonCoords.x][dungeonCoords.y];
		break;
	case DebugGridTextItem::dCorpse:
		info = dCorpse[dungeonCoords.x][dungeonCoords.y];
		break;
	case DebugGridTextItem::dItem:
		info = dItem[dungeonCoords.x][dungeonCoords.y];
		break;
	case DebugGridTextItem::dSpecial:
		info = dSpecial[dungeonCoords.x][dungeonCoords.y];
		break;
	case DebugGridTextItem::dObject:
		info = dObject[dungeonCoords.x][dungeonCoords.y];
		break;
	case DebugGridTextItem::Solid:
		info = TileHasAny(dPiece[dungeonCoords.x][dungeonCoords.y], TileProperties::Solid) << 0 | TileHasAny(dPiece[dungeonCoords.x][dungeonCoords.y], TileProperties::BlockLight) << 1 | TileHasAny(dPiece[dungeonCoords.x][dungeonCoords.y], TileProperties::BlockMissile) << 2;
		break;
	case DebugGridTextItem::Transparent:
		info = TileHasAny(dPiece[dungeonCoords.x][dungeonCoords.y], TileProperties::Transparent) << 0 | TileHasAny(dPiece[dungeonCoords.x][dungeonCoords.y], TileProperties::TransparentLeft) << 1 | TileHasAny(dPiece[dungeonCoords.x][dungeonCoords.y], TileProperties::TransparentRight) << 2;
		break;
	case DebugGridTextItem::Trap:
		info = TileHasAny(dPiece[dungeonCoords.x][dungeonCoords.y], TileProperties::Trap);
		break;
	case DebugGridTextItem::AutomapView:
		info = AutomapView[megaCoords.x][megaCoords.y];
		break;
	case DebugGridTextItem::dungeon:
		info = dungeon[megaCoords.x][megaCoords.y];
		break;
	case DebugGridTextItem::pdungeon:
		info = pdungeon[megaCoords.x][megaCoords.y];
		break;
	case DebugGridTextItem::Protected:
		info = Protected.test(megaCoords.x, megaCoords.y);
		break;
	case DebugGridTextItem::None:
		return false;
	}
	if (info == blankValue)
		return false;
	*BufCopy(debugGridTextBuffer, info) = '\0';
	return true;
}

bool IsDebugAutomapHighlightNeeded()
{
	return SearchMonsters.size() > 0 || SearchItems.size() > 0 || SearchObjects.size() > 0;
}

bool ShouldHighlightDebugAutomapTile(Point position)
{
	auto matchesSearched = [](const string_view name, const std::vector<std::string> &searchedNames) {
		const std::string lowercaseName = AsciiStrToLower(name);
		for (const auto &searchedName : searchedNames) {
			if (lowercaseName.find(searchedName) != std::string::npos) {
				return true;
			}
		}
		return false;
	};

	if (SearchMonsters.size() > 0 && dMonster[position.x][position.y] != 0) {
		const int mi = abs(dMonster[position.x][position.y]) - 1;
		const Monster &monster = Monsters[mi];
		if (matchesSearched(monster.name(), SearchMonsters))
			return true;
	}

	if (SearchItems.size() > 0 && dItem[position.x][position.y] != 0) {
		const int itemId = abs(dItem[position.x][position.y]) - 1;
		const Item &item = Items[itemId];
		if (matchesSearched(item.getName(), SearchItems))
			return true;
	}

	if (SearchObjects.size() > 0 && IsObjectAtPosition(position)) {
		const Object &object = ObjectAtPosition(position);
		if (matchesSearched(object.name(), SearchObjects))
			return true;
	}

	return false;
}

} // namespace devilution

#endif
