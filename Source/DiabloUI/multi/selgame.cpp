#include "DiabloUI/multi/selgame.h"

#include <cstdint>
#include <cstring>
#include <string>

#include <fmt/format.h>

#include "DiabloUI/diabloui.h"
#include "DiabloUI/dialogs.h"
#include "DiabloUI/hero/hero_layout.h"
#include "DiabloUI/hero/selhero.h"
#include "DiabloUI/scrollbar.h"
#include "DiabloUI/selok.h"
#include "config.h"
#include "control.h"
#include "menu.h"
#include "options.h"
#include "oracool/ui_backgrounds.h"
#include "storm/storm_net.hpp"
#include "utils/language.h"
#include "utils/str_cat.hpp"
#include "utils/utf8.hpp"

namespace devilution {

char selgame_Label[32];
char selgame_Ip[129] = "";
char selgame_Password[16] = "";
char selgame_Description[512];
std::string selgame_Title;
bool selgame_enteringGame;
int selgame_selectedGame;
bool selgame_endMenu;
int *gdwPlayerId;
_difficulty nDifficulty;
int nTickRate;
/**
 * @brief The level the difficulty gate measures against: the level of the character that was JUST
 * chosen, and nothing else.
 *
 * Oracool, user report (2026-09-03): "the level gate to higher difficulties has a hero levelcheck
 * problem [...] i can get around it when i select different level heroes a few times."
 *
 * It was found by scanning the save list for gSaveNumber - and in single-player gSaveNumber is not
 * written until the character dialog RETURNS, which happens after this screen has been shown and
 * chosen. So the gate measured the PREVIOUSLY played character: pick a level 45 hero, back out, pick
 * a level 3 hero, and the level 3 hero was offered Torment. The character screen now hands its level
 * forward the moment a character is picked or created - see selgame_SetHeroLevel and its caller in
 * SelheroLoadSelect.
 */
int heroLevel;

static GameData *m_game_data;
extern int provider;

#define DESCRIPTION_WIDTH 205

namespace {

const char *title = "";

std::vector<std::unique_ptr<UiListItem>> vecSelGameDlgItems;
std::vector<std::unique_ptr<UiItemBase>> vecSelGameDialog;
std::vector<GameInfo> Gamelist;
uint32_t firstPublicGameInfoRequestSend = 0;
unsigned HighlightedItem;

/**
 * @brief The difficulty screen's four wrapped blurbs and its locked rows' requirement lines.
 *
 * Held here rather than built inline because `UiArtText` stores a pointer: the text has to outlive the
 * item. Rewritten only in selgame_Difficulty_Init, which runs after selgame_FreeVectors has destroyed
 * whatever was pointing at them.
 */
std::string selgame_DifficultyBlurb[4];
std::string selgame_DifficultyRequirement[4];

void selgame_FreeVectors()
{
	vecSelGameDlgItems.clear();

	vecSelGameDialog.clear();
}

void selgame_Init()
{
	LoadBackgroundArt("ui_art\\selgame");
	LoadScrollBar();
}

void selgame_Free()
{
	ArtBackground = std::nullopt;
	UnloadScrollBar();
	selgame_FreeVectors();
}

bool IsGameCompatible(const GameData &data)
{
	// The Oracool version, matching what InitGameInfo advertises - see the note there. The wire
	// bytes are uint8, so the patch component compares modulo 256 on both sides identically.
	return (data.versionMajor == static_cast<uint8_t>(ORACOOL_VERSION_MAJOR)
	    && data.versionMinor == static_cast<uint8_t>(ORACOOL_VERSION_MINOR)
	    && data.versionPatch == static_cast<uint8_t>(ORACOOL_VERSION_PATCH)
	    && data.programid == GAME_ID);
}

static std::string GetErrorMessageIncompatibility(const GameData &data)
{
	if (data.programid != GAME_ID) {
		string_view gameMode;
		switch (data.programid) {
		case GameIdDiabloFull:
			gameMode = _("Diablo");
			break;
		case GameIdDiabloSpawn:
			gameMode = _("Diablo Shareware");
			break;
		case GameIdHellfireFull:
			gameMode = _("Hellfire");
			break;
		case GameIdHellfireSpawn:
			gameMode = _("Hellfire Shareware");
			break;
		default:
			return std::string(_("The host is running a different game than you."));
		}
		return fmt::format(fmt::runtime(_("The host is running a different game mode ({:s}) than you.")), gameMode);
	} else {
		return fmt::format(fmt::runtime(_(/* TRANSLATORS: Error message when somebody tries to join a game running another version. */ "Your version {:s} does not match the host {:d}.{:d}.{:d}.")), ORACOOL_VERSION, data.versionMajor, data.versionMinor, data.versionPatch);
	}
}

void UiInitGameSelectionList(string_view search)
{
	selgame_enteringGame = false;
	selgame_selectedGame = 0;

	if (provider == SELCONN_LOOPBACK) {
		selgame_enteringGame = true;
		selgame_GameSelection_Select(0);
		return;
	}

	if (provider == SELCONN_ZT) {
		CopyUtf8(selgame_Ip, sgOptions.Network.szPreviousZTGame, sizeof(selgame_Ip));
	} else {
		CopyUtf8(selgame_Ip, sgOptions.Network.szPreviousHost, sizeof(selgame_Ip));
	}

	selgame_FreeVectors();

	UiAddBackground(&vecSelGameDialog);
	UiAddLogo(&vecSelGameDialog);

	const Point uiPosition = GetUIRectangle().position;

	SDL_Rect rectScrollbar = { (Sint16)(uiPosition.x + 590), (Sint16)(uiPosition.y + 244), 25, 178 };
	vecSelGameDialog.push_back(std::make_unique<UiScrollbar>((*ArtScrollBarBackground)[0], (*ArtScrollBarThumb)[0], *ArtScrollBarArrow, rectScrollbar));

	SDL_Rect rect1 = { (Sint16)(uiPosition.x + 24), (Sint16)(uiPosition.y + 161), 590, 35 };
	vecSelGameDialog.push_back(std::make_unique<UiArtText>(_(ConnectionNames[provider]).data(), rect1, UiFlags::AlignCenter | UiFlags::FontSize30 | UiFlags::ColorUiSilver, 3));

	SDL_Rect rect2 = { (Sint16)(uiPosition.x + 35), (Sint16)(uiPosition.y + 211), 205, 192 };
	vecSelGameDialog.push_back(std::make_unique<UiArtText>(_("Description:").data(), rect2, UiFlags::FontSize24 | UiFlags::ColorUiSilver));

	SDL_Rect rect3 = { (Sint16)(uiPosition.x + 35), (Sint16)(uiPosition.y + 256), DESCRIPTION_WIDTH, 192 };
	vecSelGameDialog.push_back(std::make_unique<UiArtText>(selgame_Description, rect3, UiFlags::FontSize12 | UiFlags::ColorUiSilverDark, 1, 16));

	SDL_Rect rect4 = { (Sint16)(uiPosition.x + 300), (Sint16)(uiPosition.y + 211), 295, 33 };
	vecSelGameDialog.push_back(std::make_unique<UiArtText>(_("Select Action").data(), rect4, UiFlags::AlignCenter | UiFlags::FontSize30 | UiFlags::ColorUiSilver, 3));

#ifdef PACKET_ENCRYPTION
	vecSelGameDlgItems.push_back(std::make_unique<UiListItem>(_("Create Game"), 0, UiFlags::ColorUiGold));
#endif
	vecSelGameDlgItems.push_back(std::make_unique<UiListItem>(_("Create Public Game"), 1, UiFlags::ColorUiGold));
	vecSelGameDlgItems.push_back(std::make_unique<UiListItem>(_("Join Game"), 2, UiFlags::ColorUiGold));

	if (provider == SELCONN_ZT) {
		vecSelGameDlgItems.push_back(std::make_unique<UiListItem>("", -1, UiFlags::ElementDisabled));
		vecSelGameDlgItems.push_back(std::make_unique<UiListItem>(_("Public Games"), -1, UiFlags::ElementDisabled | UiFlags::ColorWhitegold));

		if (Gamelist.empty()) {
			// We expect the game list to be received after 3 seconds
			if (firstPublicGameInfoRequestSend == 0 || (SDL_GetTicks() - firstPublicGameInfoRequestSend) < 2000)
				vecSelGameDlgItems.push_back(std::make_unique<UiListItem>(_("Loading..."), -1, UiFlags::ElementDisabled | UiFlags::ColorUiSilver));
			else
				vecSelGameDlgItems.push_back(std::make_unique<UiListItem>(_("None"), -1, UiFlags::ElementDisabled | UiFlags::ColorUiSilver));
		} else {
			for (unsigned i = 0; i < Gamelist.size(); i++) {
				vecSelGameDlgItems.push_back(std::make_unique<UiListItem>(Gamelist[i].name, i + 3, UiFlags::ColorUiGold));
			}
		}
	}

	vecSelGameDialog.push_back(std::make_unique<UiList>(vecSelGameDlgItems, 6, uiPosition.x + 305, (uiPosition.y + 255), 285, 26, UiFlags::AlignCenter | UiFlags::FontSize24));

	SDL_Rect rect5 = { (Sint16)(uiPosition.x + 299), (Sint16)(uiPosition.y + 427), 140, 35 };
	vecSelGameDialog.push_back(std::make_unique<UiArtTextButton>(_("OK"), &UiFocusNavigationSelect, rect5, UiFlags::AlignCenter | UiFlags::VerticalCenter | UiFlags::FontSize30 | UiFlags::ColorUiGold));

	SDL_Rect rect6 = { (Sint16)(uiPosition.x + 449), (Sint16)(uiPosition.y + 427), 140, 35 };
	vecSelGameDialog.push_back(std::make_unique<UiArtTextButton>(_("CANCEL"), &UiFocusNavigationEsc, rect6, UiFlags::AlignCenter | UiFlags::VerticalCenter | UiFlags::FontSize30 | UiFlags::ColorUiGold));

	auto selectFn = [](int index) {
		// UiListItem::m_value could be different from
		// the index if packet encryption is disabled
		int itemValue = vecSelGameDlgItems[index]->m_value;
		selgame_GameSelection_Select(itemValue);
	};

	if (!search.empty()) {
		for (unsigned i = 0; i < vecSelGameDlgItems.size(); i++) {
			int gameIndex = vecSelGameDlgItems[i]->m_value - 3;
			if (gameIndex < 0)
				continue;
			if (search == Gamelist[gameIndex].name)
				HighlightedItem = i;
		}
	}

	if (HighlightedItem >= vecSelGameDlgItems.size()) {
		HighlightedItem = vecSelGameDlgItems.size() - 1;
	}

	UiInitList(selgame_GameSelection_Focus, selectFn, selgame_GameSelection_Esc, vecSelGameDialog, true, nullptr, nullptr, HighlightedItem);
}

} // namespace

void selgame_GameSelection_Init()
{
	UiInitGameSelectionList("");
}

void selgame_GameSelection_Focus(int value)
{
	const auto index = static_cast<unsigned>(value);
	HighlightedItem = index;
	const UiListItem &item = *vecSelGameDlgItems[index];
	switch (item.m_value) {
	case 0:
		CopyUtf8(selgame_Description, _("Create a new game with a difficulty setting of your choice."), sizeof(selgame_Description));
		break;
	case 1:
		CopyUtf8(selgame_Description, _("Create a new public game that anyone can join with a difficulty setting of your choice."), sizeof(selgame_Description));
		break;
	case 2:
		if (provider == SELCONN_ZT) {
			CopyUtf8(selgame_Description, _("Enter Game ID to join a game already in progress."), sizeof(selgame_Description));
		} else {
			CopyUtf8(selgame_Description, _("Enter an IP or a hostname to join a game already in progress."), sizeof(selgame_Description));
		}
		break;
	default:
		const GameInfo &gameInfo = Gamelist[item.m_value - 3];
		std::string infoString = std::string(_("Join the public game already in progress."));
		infoString.append("\n\n");
		if (IsGameCompatible(gameInfo.gameData)) {
			string_view difficulty;
			switch (gameInfo.gameData.nDifficulty) {
			case DIFF_NORMAL:
				difficulty = _("Normal");
				break;
			case DIFF_NIGHTMARE:
				difficulty = _("Nightmare");
				break;
			case DIFF_HELL:
				difficulty = _("Hell");
				break;
			case DIFF_TORMENT:
				difficulty = _("Torment");
				break;
			}
			infoString.append(fmt::format(fmt::runtime(_(/* TRANSLATORS: {:s} means: Game Difficulty. */ "Difficulty: {:s}")), difficulty));
			infoString += '\n';
			switch (gameInfo.gameData.nTickRate) {
			case 20:
				AppendStrView(infoString, _("Speed: Normal"));
				break;
			case 25:
				AppendStrView(infoString, _("Speed: Fast"));
				break;
			case 30:
				AppendStrView(infoString, _("Speed: Faster"));
				break;
			case 35:
				AppendStrView(infoString, _("Speed: Fastest"));
				break;
			default:
				// This should not occure, so no translations is needed
				infoString.append(StrCat("Speed: ", gameInfo.gameData.nTickRate));
				break;
			}
			infoString += '\n';
			AppendStrView(infoString, _("Players: "));
			for (auto &playerName : gameInfo.players) {
				infoString.append(playerName);
				infoString += ' ';
			}
		} else {
			infoString.append(GetErrorMessageIncompatibility(gameInfo.gameData));
		}
		CopyUtf8(selgame_Description, infoString, sizeof(selgame_Description));
		break;
	}
	CopyUtf8(selgame_Description, WordWrapString(selgame_Description, DESCRIPTION_WIDTH), sizeof(selgame_Description));
}

/**
 * @brief Load the current hero level from save file
 * @param pInfo Hero info
 * @return always true
 */
bool UpdateHeroLevel(_uiheroinfo *pInfo)
{
	if (pInfo->saveNumber == gSaveNumber)
		heroLevel = pInfo->level;

	return true;
}

/**
 * @brief heroLevel from the save list, for gSaveNumber's character. Zero if there is no such save.
 *
 * Zeroed BEFORE the scan, which is the other half of the report above: a scan that matched nothing -
 * a character just created, a save just deleted - used to leave whatever the previous scan found, so
 * a stale level survived even where the scan was the right question. Zero locks everything but
 * Normal, which is the safe way to be wrong.
 *
 * Multiplayer's path only: there gSaveNumber IS the chosen character by the time a difficulty is
 * picked, because that character dialog has already returned.
 */
void RefreshHeroLevelFromSaves()
{
	heroLevel = 0;
	gfnHeroInfo(UpdateHeroLevel);
}

// Defined further down, beside the level gates they share their thresholds with.
void selgame_Difficulty_Init();
string_view DifficultyName(int value);
const char *DifficultyDescription(int value);
int DifficultyLevelRequirement(int value);

void selgame_GameSelection_Select(int value)
{
	selgame_enteringGame = true;
	selgame_selectedGame = value;

	// THE LEVEL CHECK'S INPUT - see heroLevel for why this is not a scan in single-player. The
	// character screen has already handed its level forward through selgame_SetHeroLevel, and
	// gSaveNumber is still the previously played character until that dialog returns. Multiplayer
	// reaches here with gSaveNumber current, so it asks the saves.
	//
	// Deliberately not a one-shot handoff: a click on a locked row comes back through this function
	// (see selgame_Diff_Select), and re-reading the wrong save at that point would hand the gate the
	// wrong level on the second try - which is the shape of the bug being fixed.
	if (selhero_isMultiPlayer)
		RefreshHeroLevelFromSaves();

	selgame_FreeVectors();

	if (value > 2) {
		CopyUtf8(selgame_Ip, Gamelist[value - 3].name, sizeof(selgame_Ip));
		selgame_Password_Select(value);
		return;
	}

	// Oracool: user request - the difficulty picker gets its own painting. Values 0 and 1 are that
	// screen (single-player reaches it as 0, straight after choosing a character); 2 is the multiplayer
	// game list, which keeps the stock plate. The palette is already loaded by the LoadBackgroundArt
	// that brought us here, which is what AddUiBackground quantizes against.
	if (value > 1 || !oracool::AddUiBackground(&vecSelGameDialog, oracool::UiBackground::Difficulty))
		UiAddBackground(&vecSelGameDialog);

	if (value <= 1) {
		selgame_Difficulty_Init();
		return;
	}

	UiAddLogo(&vecSelGameDialog);

	const Point uiPosition = GetUIRectangle().position;

	SDL_Rect rect1 = { (Sint16)(uiPosition.x + 24), (Sint16)(uiPosition.y + 161), 590, 35 };
	vecSelGameDialog.push_back(std::make_unique<UiArtText>(&title, rect1, UiFlags::AlignCenter | UiFlags::FontSize30 | UiFlags::ColorUiSilver, 3));

	SDL_Rect rect2 = { (Sint16)(uiPosition.x + 34), (Sint16)(uiPosition.y + 211), 205, 33 };
	vecSelGameDialog.push_back(std::make_unique<UiArtText>(selgame_Label, rect2, UiFlags::AlignCenter | UiFlags::FontSize30 | UiFlags::ColorUiSilver, 3));

	SDL_Rect rect3 = { (Sint16)(uiPosition.x + 35), (Sint16)(uiPosition.y + 256), DESCRIPTION_WIDTH, 192 };
	vecSelGameDialog.push_back(std::make_unique<UiArtText>(selgame_Description, rect3, UiFlags::FontSize12 | UiFlags::ColorUiSilverDark, 1, 16));

	switch (value) {
	case 2: {
		selgame_Title = fmt::format(fmt::runtime(_("Join {:s} Games")), _(ConnectionNames[provider]));
		title = selgame_Title.c_str();

		const char *inputHint;
		if (provider == SELCONN_ZT) {
			inputHint = _("Enter Game ID").data();
		} else {
			inputHint = _("Enter address").data();
		}

		SDL_Rect rect4 = { (Sint16)(uiPosition.x + 305), (Sint16)(uiPosition.y + 211), 285, 33 };
		vecSelGameDialog.push_back(std::make_unique<UiArtText>(inputHint, rect4, UiFlags::AlignCenter | UiFlags::FontSize30 | UiFlags::ColorUiSilver, 3));

		SDL_Rect rect5 = { (Sint16)(uiPosition.x + 305), (Sint16)(uiPosition.y + 314), 285, 33 };
		vecSelGameDialog.push_back(std::make_unique<UiEdit>(inputHint, selgame_Ip, 128, false, rect5, UiFlags::FontSize24 | UiFlags::ColorUiGold));

		SDL_Rect rect6 = { (Sint16)(uiPosition.x + 299), (Sint16)(uiPosition.y + 427), 140, 35 };
		vecSelGameDialog.push_back(std::make_unique<UiArtTextButton>(_("OK"), &UiFocusNavigationSelect, rect6, UiFlags::AlignCenter | UiFlags::VerticalCenter | UiFlags::FontSize30 | UiFlags::ColorUiGold));

		SDL_Rect rect7 = { (Sint16)(uiPosition.x + 449), (Sint16)(uiPosition.y + 427), 140, 35 };
		vecSelGameDialog.push_back(std::make_unique<UiArtTextButton>(_("CANCEL"), &UiFocusNavigationEsc, rect7, UiFlags::AlignCenter | UiFlags::VerticalCenter | UiFlags::FontSize30 | UiFlags::ColorUiGold));

		HighlightedItem = 0;

#ifdef PACKET_ENCRYPTION
		UiInitList(nullptr, selgame_Password_Init, selgame_GameSelection_Init, vecSelGameDialog);
#else
		UiInitList(nullptr, selgame_Password_Select, selgame_GameSelection_Init, vecSelGameDialog);
#endif
		break;
	}
	}
}

/**
 * @brief Oracool: user request - the difficulty picker rebuilt around its own painting.
 *
 * The art is four horizontal bands, one per difficulty, and the bands ARE the rows: each carries its
 * blurb on the left and its name on the right. All four blurbs stay on screen, so the four can be
 * compared without moving the selection (user's call).
 *
 * The band positions are measured off `ui\difficulty_bg.png` rather than assumed to be quarters - they
 * compress toward the bottom (204/196/181/164 rows of 821). A UiList can only space its rows evenly,
 * so the pitch runs between the FIRST and LAST measured centres; the two middle rows land within about
 * 7px of theirs, which is nothing against a 180px band. Using the list rather than hand-rolled
 * hit-testing was the user's call too, and it is what makes the keyboard, the mouse and the amber focus
 * glow all work here for free.
 *
 * Note this screen serves single-player as well - see SelheroLoadSelect's note on the difficulty hack.
 */
void selgame_Difficulty_Init()
{
	/** Measured band centres, as per-mille of the painting's height. */
	constexpr int BandCentrePerMille[] = { 134, 386, 619, 861 };
	constexpr int TitleTop = 12;
	/**
	 * The description column, the gap, then the name column, on an 800px line centred on the screen.
	 *
	 * It borrowed HeroButtonRowWidth for this, back when the action row was itself an 800px centred
	 * line. The zone rule made the row the whole window, so the constant is gone and the figure is
	 * written out here - it was only ever this block's width, and the sharing was a coincidence.
	 */
	constexpr int BlockWidth = 800;
	constexpr int DescriptionWidth = 380;
	constexpr int NameOffset = 440;
	constexpr int NameWidth = 300;
	/** The FontSize24 line height, for the requirement line tucked under a locked name. */
	constexpr int RequirementHeight = 26;
	/**
	 * Lower than the other screens' 50. This one has no logo (user's call), so the bottom of the screen
	 * is free - and Torment's band is both the shortest and the lowest, so its name needs the room.
	 */
	constexpr int ButtonRowBottomMargin = 16;

	const int blockLeft = (gnScreenWidth - BlockWidth) / 2;
	const int descriptionX = blockLeft;
	const int nameX = blockLeft + NameOffset;
	const int firstCentre = gnScreenHeight * BandCentrePerMille[0] / 1000;
	const int lastCentre = gnScreenHeight * BandCentrePerMille[3] / 1000;
	const int pitch = (lastCentre - firstCentre) / 3;
	const int rowsTop = firstCentre - pitch / 2;
	const int buttonRowTop = gnScreenHeight - ButtonRowBottomMargin - HeroButtonRowHeight;

	title = _("Create Game").data();

	// Oracool: user report - "super dim, unreadable". Outlined rather than recoloured: this title was
	// already ColorUiSilver, the brightest silver there is, and it still washed out. The trouble is
	// not the ink, it is that it sits on a photographic sky whose own values run right through the
	// text's - so there is no colour that separates from all of it. UiFlags::Outlined rings each glyph
	// in palette index 0, which is black in every palette, and an edge does what a brighter fill
	// cannot: it works against a light background and a dark one at the same time.
	vecSelGameDialog.push_back(std::make_unique<UiArtText>(_("Select Difficulty").data(),
	    MakeSdlRect(0, TitleTop, static_cast<Uint16>(gnScreenWidth), HeroTitleHeight),
	    UiFlags::AlignCenter | HeroTitleFontSize | UiFlags::ColorUiSilver | UiFlags::Outlined, 3));

	// Oracool: Torment is single-player-only, so it must never appear as a choice when creating a
	// multiplayer game.
	const int difficultyCount = selhero_isMultiPlayer ? 3 : 4;
	for (int i = 0; i < difficultyCount; i++) {
		const int required = DifficultyLevelRequirement(i);
		const bool locked = heroLevel < required;

		// Wrapped here rather than at draw time because UiArtText holds a pointer: the strings must
		// outlive the items, and these are refilled only after selgame_FreeVectors has destroyed them.
		//
		// Wrapped at the font it is DRAWN in. These three - the wrap's font, the size flag and the
		// line height - have to move together or the column breaks at the wrong width, which is how
		// the OK dialog ended up wrapping 160px short of its own box (see that screen's note).
		selgame_DifficultyBlurb[i] = WordWrapString(DifficultyDescription(i), DescriptionWidth, GameFont12);
		vecSelGameDialog.push_back(std::make_unique<UiArtText>(selgame_DifficultyBlurb[i].c_str(),
		    MakeSdlRect(static_cast<Sint16>(descriptionX), static_cast<Sint16>(rowsTop + i * pitch),
		        DescriptionWidth, static_cast<Uint16>(pitch)),
		    // Smaller on the user's call, and back to the size the old screen drew this text at. The
		    // colour is the title's, also on the user's call: the dark silver these started in tops out
		    // at 204 against ColorUiSilver's 243, and over paintings this dark that was not readable.
		    UiFlags::AlignCenter | UiFlags::VerticalCenter | UiFlags::FontSize12 | UiFlags::ColorUiSilver, 1, 16));

		// A locked row says so on the screen instead of only in the popup that follows a click on it
		// (user's call). The popup still works - this just means you can see it coming.
		//
		// Oracool: user report - this was the worst of the two. It was ColorUiSilverDark, which tops
		// out at 204 against ColorUiSilver's 243, printed small over the darkest bands on the screen:
		// on Hell's and Torment's rows it was very nearly invisible. Brightened AND outlined, because
		// the blurbs above went through exactly this once already and only the colour was fixed then -
		// which was enough for a line of body text and not for four words in a corner.
		if (locked) {
			selgame_DifficultyRequirement[i] = fmt::format(fmt::runtime(_("requires level {:d}")), required);
			vecSelGameDialog.push_back(std::make_unique<UiArtText>(selgame_DifficultyRequirement[i].c_str(),
			    MakeSdlRect(static_cast<Sint16>(nameX),
			        static_cast<Sint16>(rowsTop + i * pitch + pitch / 2 + HeroTitleHeight / 2),
			        NameWidth, RequirementHeight),
			    UiFlags::AlignCenter | UiFlags::FontSize24 | UiFlags::ColorUiSilver | UiFlags::Outlined));
		}

		// ElementDisabled both dims the name and makes UiFocus step over it, so the arrow keys walk
		// only what this character has earned.
		vecSelGameDlgItems.push_back(std::make_unique<UiListItem>(DifficultyName(i), i,
		    locked ? UiFlags::ColorUiSilverDark | UiFlags::ElementDisabled : UiFlags::ColorUiGold));
	}

	// BEFORE the list, deliberately. The last row's rect reaches down past the button line - a row is
	// a whole band tall - and UiItemMouseEvents takes the FIRST item whose rect contains the click, so
	// whichever is pushed first wins the overlap. The buttons have to.
	vecSelGameDialog.push_back(std::make_unique<UiArtTextButton>(_("OK"), &UiFocusNavigationSelect,
	    MakeSdlRect(static_cast<Sint16>(HeroButtonRect(OkButtonIndex).x), static_cast<Sint16>(buttonRowTop),
	        static_cast<Uint16>(HeroButtonRect(OkButtonIndex).w), HeroButtonRowHeight),
	    HeroButtonFlags));
	vecSelGameDialog.push_back(std::make_unique<UiArtTextButton>(_("CANCEL"), &UiFocusNavigationEsc,
	    MakeSdlRect(static_cast<Sint16>(HeroButtonRect(CancelButtonIndex).x), static_cast<Sint16>(buttonRowTop),
	        static_cast<Uint16>(HeroButtonRect(CancelButtonIndex).w), HeroButtonRowHeight),
	    HeroButtonFlags));

	vecSelGameDialog.push_back(std::make_unique<UiList>(vecSelGameDlgItems, vecSelGameDlgItems.size(),
	    static_cast<Sint16>(nameX), static_cast<Sint16>(rowsTop), NameWidth, static_cast<Uint16>(pitch),
	    UiFlags::AlignCenter | UiFlags::VerticalCenter | HeroButtonFontSize | UiFlags::ColorUiGold));

	// No focus callback: the blurbs are all on screen at once, so there is nothing left to rewrite as
	// the selection moves.
	UiInitList(nullptr, selgame_Diff_Select, selgame_Diff_Esc, vecSelGameDialog, true);
}

void selgame_GameSelection_Esc()
{
	UiInitList_clear();
	selgame_enteringGame = false;
	selgame_endMenu = true;
}

string_view DifficultyName(int value)
{
	switch (value) {
	case DIFF_NIGHTMARE:
		return _("Nightmare");
	case DIFF_HELL:
		return _("Hell");
	case DIFF_TORMENT:
		return _("Torment");
	default:
		return _("Normal");
	}
}

/**
 * @brief The difficulty's blurb WITHOUT its leading "<Name> Difficulty" line.
 *
 * Oracool: each band shows its name as its own label now, so that first line would print the name
 * twice. Skipping past the newline reuses the existing translated strings rather than duplicating
 * them, and the pointer is still a valid C string - the body runs to the same terminator.
 */
const char *DifficultyDescription(int value)
{
	string_view full;
	switch (value) {
	case DIFF_NIGHTMARE:
		full = _("Nightmare Difficulty\nThe denizens of the Labyrinth have been bolstered and will prove to be a greater challenge. This is recommended for experienced characters only.");
		break;
	case DIFF_HELL:
		full = _("Hell Difficulty\nThe most powerful of the underworld's creatures lurk at the gateway into Hell. Only the most experienced characters should venture in this realm.");
		break;
	case DIFF_TORMENT:
		full = _("Torment Difficulty\nBeyond Hell lies a still crueler realm. Every foe here is stronger still, and only those who have already conquered Hell should attempt it.");
		break;
	default:
		full = _("Normal Difficulty\nThis is where a starting character should begin the quest to defeat Diablo.");
		break;
	}
	const char *body = std::strchr(full.data(), '\n');
	return body != nullptr ? body + 1 : full.data();
}

/**
 * @brief The character level @p value needs, or 0 if it is open to anyone.
 *
 * The single place the thresholds live. Both gate functions below read it, and so does the screen -
 * which needs the answer WITHOUT the popup those two raise, to know which rows to grey out.
 * Multiplayer keeps its own (lower) pair; single-player's is behind its own toggle.
 */
void selgame_SetHeroLevel(int level)
{
	heroLevel = level;
}

int DifficultyLevelRequirement(int value)
{
	if (selhero_isMultiPlayer) {
		if (value == 1)
			return 20;
		if (value == 2)
			return 30;
		return 0;
	}
	if (!*sgOptions.Oracool.difficultyLevelGate)
		return 0;
	if (value == 1)
		return 15;
	if (value == 2)
		return 30;
	if (value == 3)
		return 40;
	return 0;
}

bool IsDifficultyUnlocked(int value)
{
	return heroLevel >= DifficultyLevelRequirement(value);
}

bool IsDifficultyAllowed(int value)
{
	if (IsDifficultyUnlocked(value))
		return true;

	selgame_Free();

	if (value == 1)
		UiSelOkDialog(title, _("Your character must reach level 20 before you can enter a multiplayer game of Nightmare difficulty.").data(), false);
	if (value == 2)
		UiSelOkDialog(title, _("Your character must reach level 30 before you can enter a multiplayer game of Hell difficulty.").data(), false);

	selgame_Init();

	return false;
}

/**
 * @brief Oracool: single-player equivalent of IsDifficultyAllowed, with its own (higher)
 * thresholds and its own toggle - single-player difficulty selection had no level gate at all
 * before this, unlike multiplayer's existing level-20/level-30 gate for Nightmare/Hell.
 */
bool IsSinglePlayerDifficultyAllowed(int value)
{
	if (IsDifficultyUnlocked(value))
		return true;

	selgame_Free();

	if (value == 1)
		UiSelOkDialog(title, _("Your character must reach level 15 before starting a game on Nightmare difficulty.").data(), false);
	else if (value == 2)
		UiSelOkDialog(title, _("Your character must reach level 30 before starting a game on Hell difficulty.").data(), false);
	else if (value == 3)
		UiSelOkDialog(title, _("Your character must reach level 40 before starting a game on Torment difficulty.").data(), false);

	selgame_Init();

	return false;
}

void selgame_Diff_Select(int value)
{
	if (selhero_isMultiPlayer && !IsDifficultyAllowed(vecSelGameDlgItems[value]->m_value)) {
		selgame_GameSelection_Select(0);
		return;
	}
	if (!selhero_isMultiPlayer && !IsSinglePlayerDifficultyAllowed(vecSelGameDlgItems[value]->m_value)) {
		selgame_GameSelection_Select(0);
		return;
	}

	nDifficulty = (_difficulty)vecSelGameDlgItems[value]->m_value;

	if (!selhero_isMultiPlayer) {
		// This is part of a dangerous hack to enable difficulty selection in single-player.
		// FIXME: Dialogs should not refer to each other's variables.

		// We're in the selhero loop instead of the selgame one.
		// Free the selgame data and flag the end of the selhero loop.
		selhero_endMenu = true;

		// We only call FreeVectors because ArtBackground.Unload()
		// will be called by selheroFree().
		selgame_FreeVectors();

		// We must clear the InitList because selhero's loop will perform
		// one more iteration after this function exits.
		UiInitList_clear();

		return;
	}

	selgame_GameSpeedSelection();
}

void selgame_Diff_Esc()
{
	if (!selhero_isMultiPlayer) {
		selgame_Free();

		selhero_Init();
		selhero_List_Init();
		return;
	}

	if (provider == SELCONN_LOOPBACK) {
		selgame_GameSelection_Esc();
		return;
	}

	HighlightedItem = 0;
	selgame_GameSelection_Init();
}

void selgame_GameSpeedSelection()
{
	RefreshHeroLevelFromSaves();

	selgame_FreeVectors();

	UiAddBackground(&vecSelGameDialog);
	UiAddLogo(&vecSelGameDialog);

	const Point uiPosition = GetUIRectangle().position;

	SDL_Rect rect1 = { (Sint16)(uiPosition.x + 24), (Sint16)(uiPosition.y + 161), 590, 35 };
	vecSelGameDialog.push_back(std::make_unique<UiArtText>(_("Create Game").data(), rect1, UiFlags::AlignCenter | UiFlags::FontSize30 | UiFlags::ColorUiSilver, 3));

	SDL_Rect rect2 = { (Sint16)(uiPosition.x + 34), (Sint16)(uiPosition.y + 211), 205, 33 };
	vecSelGameDialog.push_back(std::make_unique<UiArtText>(selgame_Label, rect2, UiFlags::AlignCenter | UiFlags::FontSize30 | UiFlags::ColorUiSilver, 3));

	SDL_Rect rect3 = { (Sint16)(uiPosition.x + 35), (Sint16)(uiPosition.y + 256), DESCRIPTION_WIDTH, 192 };
	vecSelGameDialog.push_back(std::make_unique<UiArtText>(selgame_Description, rect3, UiFlags::FontSize12 | UiFlags::ColorUiSilverDark, 1, 16));

	SDL_Rect rect4 = { (Sint16)(uiPosition.x + 299), (Sint16)(uiPosition.y + 211), 295, 35 };
	vecSelGameDialog.push_back(std::make_unique<UiArtText>(_("Select Game Speed").data(), rect4, UiFlags::AlignCenter | UiFlags::FontSize30 | UiFlags::ColorUiSilver, 3));

	vecSelGameDlgItems.push_back(std::make_unique<UiListItem>(_("Normal"), 20));
	vecSelGameDlgItems.push_back(std::make_unique<UiListItem>(_("Fast"), 30));
	vecSelGameDlgItems.push_back(std::make_unique<UiListItem>(_("Faster"), 40));
	vecSelGameDlgItems.push_back(std::make_unique<UiListItem>(_("Fastest"), 50));

	vecSelGameDialog.push_back(std::make_unique<UiList>(vecSelGameDlgItems, vecSelGameDlgItems.size(), uiPosition.x + 300, (uiPosition.y + 279), 295, 26, UiFlags::AlignCenter | UiFlags::FontSize24 | UiFlags::ColorUiGold));

	SDL_Rect rect5 = { (Sint16)(uiPosition.x + 299), (Sint16)(uiPosition.y + 427), 140, 35 };
	vecSelGameDialog.push_back(std::make_unique<UiArtTextButton>(_("OK"), &UiFocusNavigationSelect, rect5, UiFlags::AlignCenter | UiFlags::VerticalCenter | UiFlags::FontSize30 | UiFlags::ColorUiGold));

	SDL_Rect rect6 = { (Sint16)(uiPosition.x + 449), (Sint16)(uiPosition.y + 427), 140, 35 };
	vecSelGameDialog.push_back(std::make_unique<UiArtTextButton>(_("CANCEL"), &UiFocusNavigationEsc, rect6, UiFlags::AlignCenter | UiFlags::VerticalCenter | UiFlags::FontSize30 | UiFlags::ColorUiGold));

	UiInitList(selgame_Speed_Focus, selgame_Speed_Select, selgame_Speed_Esc, vecSelGameDialog, true);
}

void selgame_Speed_Focus(int value)
{
	switch (vecSelGameDlgItems[value]->m_value) {
	case 20:
		CopyUtf8(selgame_Label, _("Normal"), sizeof(selgame_Label));
		CopyUtf8(selgame_Description, _("Normal Speed\nThis is where a starting character should begin the quest to defeat Diablo."), sizeof(selgame_Description));
		break;
	case 30:
		CopyUtf8(selgame_Label, _("Fast"), sizeof(selgame_Label));
		CopyUtf8(selgame_Description, _("Fast Speed\nThe denizens of the Labyrinth have been hastened and will prove to be a greater challenge. This is recommended for experienced characters only."), sizeof(selgame_Description));
		break;
	case 40:
		CopyUtf8(selgame_Label, _("Faster"), sizeof(selgame_Label));
		CopyUtf8(selgame_Description, _("Faster Speed\nMost monsters of the dungeon will seek you out quicker than ever before. Only an experienced champion should try their luck at this speed."), sizeof(selgame_Description));
		break;
	case 50:
		CopyUtf8(selgame_Label, _("Fastest"), sizeof(selgame_Label));
		CopyUtf8(selgame_Description, _("Fastest Speed\nThe minions of the underworld will rush to attack without hesitation. Only a true speed demon should enter at this pace."), sizeof(selgame_Description));
		break;
	}
	CopyUtf8(selgame_Description, WordWrapString(selgame_Description, DESCRIPTION_WIDTH), sizeof(selgame_Description));
}

void selgame_Speed_Esc()
{
	selgame_GameSelection_Select(0);
}

void selgame_Speed_Select(int value)
{
	nTickRate = vecSelGameDlgItems[value]->m_value;

	if (provider == SELCONN_LOOPBACK || selgame_selectedGame == 1) {
		selgame_Password_Select(0);
		return;
	}

	selgame_Password_Init(0);
}

void selgame_Password_Init(int /*value*/)
{
	memset(&selgame_Password, 0, sizeof(selgame_Password));

	selgame_FreeVectors();

	UiAddBackground(&vecSelGameDialog);
	UiAddLogo(&vecSelGameDialog);

	const Point uiPosition = GetUIRectangle().position;

	SDL_Rect rect1 = { (Sint16)(uiPosition.x + 24), (Sint16)(uiPosition.y + 161), 590, 35 };
	vecSelGameDialog.push_back(std::make_unique<UiArtText>(_(ConnectionNames[provider]).data(), rect1, UiFlags::AlignCenter | UiFlags::FontSize30 | UiFlags::ColorUiSilver, 3));

	SDL_Rect rect2 = { (Sint16)(uiPosition.x + 35), (Sint16)(uiPosition.y + 211), 205, 192 };
	vecSelGameDialog.push_back(std::make_unique<UiArtText>(_("Description:").data(), rect2, UiFlags::FontSize24 | UiFlags::ColorUiSilver));

	SDL_Rect rect3 = { (Sint16)(uiPosition.x + 35), (Sint16)(uiPosition.y + 256), DESCRIPTION_WIDTH, 192 };
	vecSelGameDialog.push_back(std::make_unique<UiArtText>(selgame_Description, rect3, UiFlags::FontSize12 | UiFlags::ColorUiSilverDark, 1, 16));

	SDL_Rect rect4 = { (Sint16)(uiPosition.x + 305), (Sint16)(uiPosition.y + 211), 285, 33 };
	vecSelGameDialog.push_back(std::make_unique<UiArtText>(_("Enter Password").data(), rect4, UiFlags::AlignCenter | UiFlags::FontSize30 | UiFlags::ColorUiSilver, 3));

	// Allow password to be empty only when joining games
	bool allowEmpty = selgame_selectedGame == 2;
	SDL_Rect rect5 = { (Sint16)(uiPosition.x + 305), (Sint16)(uiPosition.y + 314), 285, 33 };
	vecSelGameDialog.push_back(std::make_unique<UiEdit>(_("Enter Password"), selgame_Password, 15, allowEmpty, rect5, UiFlags::FontSize24 | UiFlags::ColorUiGold));

	SDL_Rect rect6 = { (Sint16)(uiPosition.x + 299), (Sint16)(uiPosition.y + 427), 140, 35 };
	vecSelGameDialog.push_back(std::make_unique<UiArtTextButton>(_("OK"), &UiFocusNavigationSelect, rect6, UiFlags::AlignCenter | UiFlags::VerticalCenter | UiFlags::FontSize30 | UiFlags::ColorUiGold));

	SDL_Rect rect7 = { (Sint16)(uiPosition.x + 449), (Sint16)(uiPosition.y + 427), 140, 35 };
	vecSelGameDialog.push_back(std::make_unique<UiArtTextButton>(_("CANCEL"), &UiFocusNavigationEsc, rect7, UiFlags::AlignCenter | UiFlags::VerticalCenter | UiFlags::FontSize30 | UiFlags::ColorUiGold));

	UiInitList(nullptr, selgame_Password_Select, selgame_Password_Esc, vecSelGameDialog);
}

static bool IsGameCompatibleWithErrorMessage(const GameData &data)
{
	if (IsGameCompatible(data))
		return IsDifficultyAllowed(data.nDifficulty);

	selgame_Free();

	std::string errorMessage = GetErrorMessageIncompatibility(data);
	UiSelOkDialog(title, errorMessage.c_str(), false);

	selgame_Init();

	return false;
}

void selgame_Password_Select(int /*value*/)
{
	char *gamePassword = nullptr;
	if (selgame_selectedGame == 0)
		gamePassword = selgame_Password;
	if (selgame_selectedGame == 2 && strlen(selgame_Password) > 0)
		gamePassword = selgame_Password;

	// If there is an error, the error message won't necessarily be set.
	// Clear the error so that we display "Unknown network error"
	// instead of an arbitrary message in that case.
	SDL_ClearError();

	if (selgame_selectedGame > 1) {
		bool allowJoin = true;
		if (selgame_selectedGame > 2)
			allowJoin = IsGameCompatible(Gamelist[selgame_selectedGame - 3].gameData);
		if (provider == SELCONN_ZT) {
			for (unsigned int i = 0; i < (sizeof(selgame_Ip) / sizeof(selgame_Ip[0])); i++) {
				selgame_Ip[i] = (selgame_Ip[i] >= 'A' && selgame_Ip[i] <= 'Z') ? selgame_Ip[i] + 'a' - 'A' : selgame_Ip[i];
			}
			strcpy(sgOptions.Network.szPreviousZTGame, selgame_Ip);
		} else {
			strcpy(sgOptions.Network.szPreviousHost, selgame_Ip);
		}
		if (allowJoin && SNetJoinGame(selgame_Ip, gamePassword, gdwPlayerId)) {
			if (!IsGameCompatibleWithErrorMessage(*m_game_data)) {
				InitGameInfo();
				selgame_GameSelection_Select(1);
				return;
			}

			UiInitList_clear();
			selgame_endMenu = true;
		} else {
			InitGameInfo();
			selgame_Free();
			std::string error;
			if (!allowJoin)
				error = GetErrorMessageIncompatibility(Gamelist[selgame_selectedGame - 3].gameData);
			else
				error = SDL_GetError();
			if (error.empty())
				error = "Unknown network error";
			UiSelOkDialog(_("Multi Player Game").data(), error.c_str(), false);
			selgame_Init();
			if (selgame_selectedGame == 2)
				selgame_Password_Init(selgame_selectedGame);
			else
				UiInitGameSelectionList("");
		}
		return;
	}

	m_game_data->nDifficulty = nDifficulty;
	m_game_data->nTickRate = nTickRate;
	m_game_data->bRunInTown = *sgOptions.Gameplay.runInTown ? 1 : 0;
	m_game_data->bTheoQuest = *sgOptions.Gameplay.theoQuest ? 1 : 0;
	// Random resolves to the farmer - see the matching note in multi.cpp.
	m_game_data->bCowQuest = (*sgOptions.Gameplay.farmerQuest == FarmerQuestMode::AlwaysCow) ? 1 : 0;

	GameData gameInitInfo = *m_game_data;
	gameInitInfo.swapLE();
	if (SNetCreateGame(nullptr, gamePassword, reinterpret_cast<char *>(&gameInitInfo), sizeof(gameInitInfo), gdwPlayerId)) {
		UiInitList_clear();
		selgame_endMenu = true;
	} else {
		selgame_Free();
		std::string error = SDL_GetError();
		if (error.empty())
			error = "Unknown network error";
		UiSelOkDialog(_("Multi Player Game").data(), error.c_str(), false);
		selgame_Init();
		selgame_Password_Init(0);
	}
}

void selgame_Password_Esc()
{
	if (selgame_selectedGame == 2)
		selgame_GameSelection_Select(2);
	else
		selgame_GameSpeedSelection();
}

void RefreshGameList()
{
	static uint32_t lastRequest = 0;
	static uint32_t lastUpdate = 0;

	if (selgame_enteringGame)
		return;

	uint32_t currentTime = SDL_GetTicks();

	if ((lastRequest == 0 || currentTime - lastRequest > 30000) && DvlNet_SendInfoRequest()) {
		lastRequest = currentTime;
		lastUpdate = currentTime - 3000; // Give 2 sec for responses, but don't wait 5
		if (firstPublicGameInfoRequestSend == 0)
			firstPublicGameInfoRequestSend = currentTime;
	}

	if (lastUpdate == 0 || currentTime - lastUpdate > 5000) {
		int gameIndex = vecSelGameDlgItems[HighlightedItem]->m_value - 3;
		std::string gameSearch = gameIndex >= 0 ? Gamelist[gameIndex].name : "";
		std::vector<GameInfo> gamelist = DvlNet_GetGamelist();
		Gamelist.clear();
		for (unsigned i = 0; i < gamelist.size(); i++) {
			Gamelist.push_back(gamelist[i]);
		}
		UiInitGameSelectionList(gameSearch);
		lastUpdate = currentTime;
	}
}

bool UiSelectGame(GameData *gameData, int *playerId)
{
	firstPublicGameInfoRequestSend = 0;
	gdwPlayerId = playerId;
	m_game_data = gameData;
	selgame_Init();
	HighlightedItem = 0;
	selgame_GameSelection_Init();

	selgame_endMenu = false;

	DvlNet_ClearPassword();
	DvlNet_ClearGamelist();

	while (!selgame_endMenu) {
		UiClearScreen();
		UiPollAndRender();
		if (provider == SELCONN_ZT)
			RefreshGameList();
	}
	selgame_Free();

	return selgame_enteringGame;
}
} // namespace devilution
