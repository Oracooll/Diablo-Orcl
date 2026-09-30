#include "oracool/quest_marks.h"

#include <SDL.h>

#include "DiabloUI/ui_flags.hpp"
#include "engine/render/text_render.hpp"
#include "inv.h"
#include "player.h"
#include "quests.h"
#include "towners.h"

namespace devilution::oracool {

namespace {

/** @brief The mark itself. Not translated: it is a punctuation mark, and every language draws it. */
constexpr string_view QuestMark = "!";

bool HasItem(Player &player, _item_indexes id)
{
	return HasInventoryItemWithId(player, id);
}

/** @brief Ogden - TalkToBarOwner, towners.cpp:298. The Skeleton King, then Ogden's Sign. */
bool BarOwnerHasNews(Player &player)
{
	// A speech the click queued and nobody has heard yet: the click moved the quest on, so the tests below go quiet at once
	// while the news still waits under "Talk to Ogden" (round 8 audit, v1.12.233).
	if (HasOgdenQuestText())
		return true;
	const Quest &king = Quests[Q_SKELKING];
	if (king._qactive != QUEST_NOTAVAIL && (player._pLvlVisited[2] || player._pLvlVisited[3] || player._pLvlVisited[4])) {
		if (king._qvar2 == 0)
			return true;
		if (king._qactive == QUEST_DONE && king._qvar2 == 1)
			return true;
	}
	const Quest &banner = Quests[Q_LTBANNER];
	if (banner._qactive != QUEST_NOTAVAIL && banner._qactive != QUEST_DONE
	    && (player._pLvlVisited[3] || player._pLvlVisited[4])) {
		if (banner._qvar2 == 0)
			return true;
		if (banner._qvar2 == 1 && HasItem(player, IDI_BANNER))
			return true;
	}
	return false;
}

/** @brief Griswold - TalkToBlackSmith, towners.cpp:378. The Magic Rock, then the Anvil of Fury. */
bool BlackSmithHasNews(Player &player)
{
	const Quest &rock = Quests[Q_ROCK];
	if (rock._qactive != QUEST_NOTAVAIL && rock._qactive != QUEST_DONE
	    && (player._pLvlVisited[4] || player._pLvlVisited[5])) {
		if (rock._qvar2 == 0)
			return true;
		if (rock._qvar2 == 1 && HasItem(player, IDI_ROCK))
			return true;
	}
	const Quest &anvil = Quests[Q_ANVIL];
	if (IsNoneOf(anvil._qactive, QUEST_NOTAVAIL, QUEST_DONE)) {
		if (anvil._qvar2 == 0 && (player._pLvlVisited[9] || player._pLvlVisited[10]))
			return true;
		if (anvil._qvar2 == 1 && HasItem(player, IDI_ANVIL))
			return true;
	}
	return false;
}

/** @brief Adria - TalkToWitch, towners.cpp:427. The whole Black Mushroom chain but the brain, which is Pepin's. */
bool WitchHasNews(Player &player)
{
	const Quest &mushroom = Quests[Q_MUSHROOM];
	if (mushroom._qactive == QUEST_NOTAVAIL)
		return false;
	if (mushroom._qactive == QUEST_INIT)
		return HasItem(player, IDI_FUNGALTM);
	if (mushroom._qactive != QUEST_ACTIVE)
		return false;
	if (mushroom._qvar1 >= QS_TOMEGIVEN && mushroom._qvar1 < QS_MUSHGIVEN) {
		// She takes the mushroom, or says the one line she has not said yet.
		return HasItem(player, IDI_MUSHROOM) || mushroom._qmsg != TEXT_MUSH9;
	}
	// The Brain only until she has said its line: she repeats MUSH11 on every click (round 26 audit).
	if (mushroom._qvar1 >= QS_MUSHGIVEN)
		return (HasItem(player, IDI_BRAIN) && mushroom._qmsg != TEXT_MUSH11) || HasInventoryOrBeltItemWithId(player, IDI_SPECELIX);
	return false;
}

/** @brief Pepin - TalkToHealer, towners.cpp:498. Poisoned Water, and the brain for the elixir. */
bool HealerHasNews(Player &player)
{
	const Quest &water = Quests[Q_PWATER];
	if (water._qactive != QUEST_NOTAVAIL) {
		if (water._qactive == QUEST_INIT && (player._pLvlVisited[1] || player._pLvlVisited[5]))
			return true;
		if (water._qactive == QUEST_ACTIVE && !water._qlog)
			return true;
		if (water._qactive == QUEST_DONE && water._qvar1 != 2)
			return true;
	}
	const Quest &mushroom = Quests[Q_MUSHROOM];
	return mushroom._qactive == QUEST_ACTIVE && mushroom._qvar1 >= QS_MUSHGIVEN
	    && mushroom._qvar1 < QS_BRAINGIVEN && HasItem(player, IDI_BRAIN);
}

/** @brief Cain - TalkToStoryteller, towners.cpp:542. The Staff of Lazarus, and the word after Lazarus falls. */
bool StorytellerHasNews(Player &player)
{
	const Quest &betrayer = Quests[Q_BETRAYER];
	if (!UseMultiplayerQuests()) {
		if (betrayer._qactive == QUEST_INIT && HasItem(player, IDI_LAZSTAFF))
			return true;
	} else if (betrayer._qactive == QUEST_ACTIVE && !betrayer._qlog) {
		return true;
	}
	return betrayer._qactive == QUEST_DONE && betrayer._qvar1 == 7;
}

/** @brief Lester - TalkToFarmer, towners.cpp:608. He hands out the rune bomb, and pays for the hive. */
bool FarmerHasNews(Player &player)
{
	const Quest &quest = Quests[Q_FARMER];
	switch (quest._qactive) {
	case QUEST_NOTAVAIL:
	case QUEST_INIT:
		// Without the bomb he only chats until the Caves are behind you or level 15.
		return HasItem(player, IDI_RUNEBOMB) || player._pLvlVisited[9] || player._pLevel >= 15;
	case QUEST_DONE:
		return true; // the Auric Amulet is waiting
	default:
		return false;
	}
}

/** @brief The complete nut - TalkToCowFarmer, towners.cpp:664. The suits, the bomb, and his three teases. */
bool CowFarmerHasNews(Player &player)
{
	// The bomb is news only before his quest is under way: once ACTIVE he only repeats himself, and the mark stayed lit for
	// as long as the bomb was carried (round 8 audit).
	if (HasItem(player, IDI_GREYSUIT) || HasItem(player, IDI_BROWNSUIT)
	    || (HasItem(player, IDI_RUNEBOMB) && Quests[Q_JERSEY]._qactive != QUEST_ACTIVE))
		return true;
	const Quest &quest = Quests[Q_JERSEY];
	switch (quest._qactive) {
	case QUEST_NOTAVAIL:
	case QUEST_INIT:
	case QUEST_HIVE_TEASE1:
	case QUEST_HIVE_TEASE2:
		return true;
	case QUEST_HIVE_ACTIVE:
		return player._pLvlVisited[9] || player._pLevel >= 15;
	default:
		return false;
	}
}

/** @brief Celia - TalkToGirl, towners.cpp:748. She asks for Theodore, and takes him. */
bool GirlHasNews(Player &player)
{
	const Quest &quest = Quests[Q_GIRL];
	if (quest._qactive != QUEST_DONE && HasItem(player, IDI_THEODORE))
		return true;
	return IsAnyOf(quest._qactive, QUEST_NOTAVAIL, QUEST_INIT);
}

} // namespace

bool TownerHasQuestNews(const Towner &towner)
{
	if (MyPlayer == nullptr)
		return false;
	Player &player = *MyPlayer; // the inventory checks below are non-const, as HasInventoryItemWithId is
	switch (towner._ttype) {
	case TOWN_TAVERN:
		return BarOwnerHasNews(player);
	case TOWN_DEADGUY:
		// TalkToDeadguy, towners.cpp:359: he tells you about the Butcher once, and repeats nothing.
		return Quests[Q_BUTCHER]._qactive != QUEST_DONE && Quests[Q_BUTCHER]._qvar1 != 1;
	case TOWN_SMITH:
		return BlackSmithHasNews(player);
	case TOWN_WITCH:
		return WitchHasNews(player);
	case TOWN_BMAID:
		// TalkToBarmaid, towners.cpp:477: the Cathedral Map, before the Crypt is entered.
		return !player._pLvlVisited[21] && HasItem(player, IDI_MAPOFDOOM)
		    && Quests[Q_GRAVE]._qmsg != TEXT_GRAVE8;
	case TOWN_HEALER:
		return HealerHasNews(player);
	case TOWN_STORY:
		return StorytellerHasNews(player);
	case TOWN_FARMER:
		return FarmerHasNews(player);
	case TOWN_COWFARM:
		return CowFarmerHasNews(player);
	case TOWN_GIRL:
		return GirlHasNews(player);
	// Wirt sells, Farnham drinks and the cow is a cow. None of them carries a quest.
	case TOWN_PEGBOY:
	case TOWN_DRUNK:
	case TOWN_COW:
	case NUM_TOWNER_TYPES:
		return false;
	}
	return false;
}

void DrawTownerQuestMark(const Surface &out, const Towner &towner, Point position, ClxSprite sprite)
{
	if (!TownerHasQuestNews(towner))
		return;

	// The pulse: a four-step triangle about every half second, brightening the gold and lifting the mark
	// a pixel with it. Two colours and one pixel are enough to read as a glow, and neither needs art.
	const int step = static_cast<int>((SDL_GetTicks() / 120) % 8);
	const int bright = step < 4 ? step : 7 - step;
	const UiFlags color = bright >= 2 ? UiFlags::ColorWhitegold : UiFlags::ColorGold;

	const int width = GetLineWidth(QuestMark, GameFont24);
	const Point mark { position.x + static_cast<int>(sprite.width()) / 2 - width / 2,
		position.y - static_cast<int>(sprite.height()) - 10 - bright };
	DrawString(out, QuestMark, Rectangle { mark, { width, 0 } },
	    { color | UiFlags::FontSize24 | UiFlags::Shadowed });
}

} // namespace devilution::oracool
