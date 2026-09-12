#include "floatingnumbers.h"

#include <cstdint>
#include <ctime>
#include <deque>
#include <fmt/format.h>
#include <string>

#include "engine/render/text_render.hpp"
#include "options.h"
#include "panels/charpanel.hpp" // DamageTypeColor - one table for what an element looks like
#include "utils/str_cat.hpp"

namespace devilution {

namespace {

struct FloatingNumber {
	Point startPos;
	Displacement startOffset;
	Displacement endOffset;
	std::string text;
	uint32_t time;
	uint32_t lastMerge;
	UiFlags style;
	DamageType type;
	int value;
	int index;
	bool reverseDirection;
};

std::deque<FloatingNumber> FloatingQueue;

void ClearExpiredNumbers()
{
	while (!FloatingQueue.empty()) {
		FloatingNumber &num = FloatingQueue.front();
		if (num.time > SDL_GetTicks())
			break;

		FloatingQueue.pop_front();
	}
}

GameFontTables GetGameFontSizeByDamage(int value)
{
	value >>= 6;
	if (value >= 300)
		return GameFont30;
	if (value >= 100)
		return GameFont24;
	return GameFont12;
}

UiFlags GetFontSizeByDamage(int value)
{
	value >>= 6;
	if (value >= 300)
		return UiFlags::FontSize30;
	if (value >= 100)
		return UiFlags::FontSize24;
	return UiFlags::FontSize12;
}

void UpdateFloatingData(FloatingNumber &num)
{
	if (num.value > 0 && num.value < 64) {
		num.text = fmt::format("{:.2f}", num.value / 64.0);
	} else {
		num.text = StrCat(num.value >> 6);
	}

	num.style &= ~(UiFlags::FontSize12 | UiFlags::FontSize24 | UiFlags::FontSize30);
	num.style |= GetFontSizeByDamage(num.value);
	num.style |= DamageTextColor(num.type);
}

void AddFloatingNumber(Point pos, Displacement offset, DamageType type, int value, int index, bool damageToPlayer)
{
	// 45 deg angles to avoid jitter caused by px alignment
	Displacement goodAngles[] = {
		{ 0, -140 },
		{ 100, -100 },
		{ -100, -100 },
	};

	Displacement endOffset;
	if (*sgOptions.Gameplay.enableFloatingNumbers == FloatingNumbers::Random) {
		endOffset = goodAngles[rand() % 3];
	} else if (*sgOptions.Gameplay.enableFloatingNumbers == FloatingNumbers::Vertical) {
		endOffset = goodAngles[0];
	}

	if (damageToPlayer)
		endOffset = -endOffset;

	for (auto &num : FloatingQueue) {
		if (num.reverseDirection == damageToPlayer && num.type == type && num.index == index && (SDL_GetTicks() - static_cast<int>(num.lastMerge)) <= 100) {
			num.value += value;
			num.lastMerge = SDL_GetTicks();
			UpdateFloatingData(num);
			return;
		}
	}
	FloatingNumber num {
		pos, offset, endOffset, "", SDL_GetTicks() + 2500, SDL_GetTicks(), UiFlags::Outlined, type, value, index, damageToPlayer
	};
	UpdateFloatingData(num);
	FloatingQueue.push_back(num);
}

} // namespace

UiFlags DamageTextColor(DamageType type)
{
	// Physical is gold here and white on the sheet - the one deliberate divergence. Gold is
	// vanilla's damage number and by far the most common thing on screen; white would put the
	// loudest number in the game in the same ink as cold.
	if (type == DamageType::Physical)
		return UiFlags::ColorGold;

	// Acid is monster-only (nothing the player casts carries it), and on the sheet it falls through
	// to white. Here white is COLD's, so acid keeps its own yellow rather than borrowing one.
	if (type == DamageType::Acid)
		return UiFlags::ColorYellow;

	// Everything else - fire, lightning, magic, cold - defers to the character sheet's table, so an
	// element cannot be described two ways in one game. Before 1.11.080 this switch held its own
	// opinions and all three of the user's named elements were wrong:
	//
	//   fire      ColorUiSilver, commented "appears dark red ingame". True of the old indexed path;
	//             renderer stage 4 (v1.11.010) turned the .trn files into RGB values and
	//             ColorInGameUiSilver became a plain grey ramp, 0xF3F3F3 down to 0x111111. Fire
	//             damage had been drawing GREY ever since, with the comment still promising red -
	//             a colour picked by a NAME whose meaning moved underneath it.
	//   lightning ColorBlue, while the sheet says yellow - the user's explicit 2026-08-31 call,
	//             after a first attempt rendered "dark blue instead of yellow".
	//   magic     ColorOrange, while the sheet had said ColorMagicDamage (208,98,98) since
	//             2026-09-11 - so a Blessed Hammer hit named one colour in the panel and wore
	//             another over the monster.
	//
	// Auras reach this through AuraStrike -> ApplyMonsterDamage, which has always carried the
	// element; nothing needed rewiring, the table was simply lying.
	return DamageTypeColor(type);
}

void AddFloatingNumber(DamageType damageType, const Monster &monster, int damage)
{
	if (*sgOptions.Gameplay.enableFloatingNumbers == FloatingNumbers::Off)
		return;

	Displacement offset = {};
	if (monster.isWalking()) {
		offset = GetOffsetForWalking(monster.animInfo, monster.direction);
		if (monster.mode == MonsterMode::MoveSideways) {
			if (monster.direction == Direction::West)
				offset -= Displacement { 64, 0 };
			else
				offset += Displacement { 64, 0 };
		}
	}
	if (monster.animInfo.sprites) {
		const ClxSprite sprite = monster.animInfo.currentSprite();
		offset.deltaY -= sprite.height() / 2;
	}

	AddFloatingNumber(monster.position.tile, offset, damageType, damage, monster.getId(), false);
}

void AddFloatingNumber(DamageType damageType, const Player &player, int damage)
{
	if (*sgOptions.Gameplay.enableFloatingNumbers == FloatingNumbers::Off)
		return;

	Displacement offset = {};
	if (player.isWalking()) {
		offset = GetOffsetForWalking(player.AnimInfo, player._pdir);
		if (player._pmode == PM_WALK_SIDEWAYS) {
			if (player._pdir == Direction::West)
				offset -= Displacement { 64, 0 };
			else
				offset += Displacement { 64, 0 };
		}
	}

	AddFloatingNumber(player.position.tile, offset, damageType, damage, player.getId(), true);
}

void DrawFloatingNumbers(const Surface &out, Point viewPosition, Displacement offset)
{
	if (*sgOptions.Gameplay.enableFloatingNumbers == FloatingNumbers::Off)
		return;

	for (auto &floatingNum : FloatingQueue) {
		Displacement worldOffset = viewPosition - floatingNum.startPos;
		worldOffset = worldOffset.worldToScreen() + offset + Displacement { TILE_WIDTH / 2, -TILE_HEIGHT / 2 } + floatingNum.startOffset;

		worldOffset *= *sgOptions.Oracool.dungeonZoomLevel;

		Point screenPosition { worldOffset.deltaX, worldOffset.deltaY };

		int lineWidth = GetLineWidth(floatingNum.text, GetGameFontSizeByDamage(floatingNum.value));
		screenPosition.x -= lineWidth / 2;
		uint32_t timeLeft = floatingNum.time - SDL_GetTicks();
		float mul = 1 - (timeLeft / 2500.0f);
		screenPosition += floatingNum.endOffset * mul;

		DrawString(out, floatingNum.text, Rectangle { screenPosition, { lineWidth, 0 } }, { floatingNum.style });
	}

	ClearExpiredNumbers();
}

void ClearFloatingNumbers()
{
	srand(time(nullptr));

	FloatingQueue.clear();
}

} // namespace devilution
