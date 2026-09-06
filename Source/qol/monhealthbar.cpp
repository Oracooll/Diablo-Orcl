/**
 * @file monhealthbar.cpp
 *
 * Adds monster health bar QoL feature
 */
#include "monhealthbar.h"

#include <cstdint>
#include <string>

#include <fmt/format.h>

#include "control.h"
#include "cursor.h"
#include "engine/clx_sprite.hpp"
#include "engine/load_clx.hpp"
#include "engine/render/clx_render.hpp"
#include "engine/render/primitive_render.hpp"
#include "multi.h" // sgGameInitInfo.nDifficulty - the XP a kill pays here
#include "options.h"
#include "oracool/aura_field.h"
#include "oracool/lesser_uniques.h"
#include "player.h" // KillExperienceFor
#include "utils/language.h"
#include "utils/str_cat.hpp"

namespace devilution {
namespace {

OptionalOwnedClxSpriteList healthBox;
OptionalOwnedClxSpriteList resistance;
OptionalOwnedClxSpriteList health;
OptionalOwnedClxSpriteList healthBlue;
OptionalOwnedClxSpriteList playerExpTags;

} // namespace

void InitMonsterHealthBar()
{
	if (!*sgOptions.Gameplay.enemyHealthBar)
		return;

	healthBox = LoadClx("data\\healthbox.clx");
	health = LoadClx("data\\health.clx");
	resistance = LoadClx("data\\resistance.clx");
	playerExpTags = LoadClx("data\\monstertags.clx");

	std::array<uint8_t, 256> healthBlueTrn;
	healthBlueTrn[234] = 185;
	healthBlueTrn[235] = 186;
	healthBlueTrn[236] = 187;
	healthBlue = health->clone();
	ClxApplyTrans(*healthBlue, healthBlueTrn.data());
}

void FreeMonsterHealthBar()
{
	healthBlue = std::nullopt;
	playerExpTags = std::nullopt;
	resistance = std::nullopt;
	health = std::nullopt;
	healthBox = std::nullopt;
}

void DrawMonsterHealthBar(const Surface &out)
{
	if (!*sgOptions.Gameplay.enemyHealthBar)
		return;

	if (leveltype == DTYPE_TOWN)
		return;
	if (pcursmonst == -1)
		return;

	const Monster &monster = Monsters[pcursmonst];

	const int width = (*healthBox)[0].width();
	const int barWidth = (*health)[0].width();
	const int height = (*healthBox)[0].height();
	Point position = { (gnScreenWidth - width) / 2, 18 };

	if (CanPanelsCoverView()) {
		if (IsRightPanelOpen())
			position.x -= SidePanelSize.width / 2;
		if (IsLeftPanelOpen())
			position.x += SidePanelSize.width / 2;
	}

	const int border = 3;

	int multiplier = 0;
	int currLife = monster.hitPoints;
	// lifestealing monsters can reach HP exceeding their max
	if (monster.hitPoints > monster.maxHitPoints) {
		multiplier = monster.hitPoints / monster.maxHitPoints;
		currLife = monster.hitPoints - monster.maxHitPoints * multiplier;
		if (currLife == 0 && multiplier > 0) {
			multiplier--;
			currLife = monster.maxHitPoints;
		}
	}

	RenderClxSprite(out, (*healthBox)[0], position);
	DrawHalfTransparentRectTo(out, position.x + border, position.y + border, width - (border * 2), height - (border * 2));
	int barProgress = (barWidth * currLife) / monster.maxHitPoints;
	if (barProgress != 0) {
		RenderClxSprite(
		    out.subregion(position.x + border + 1, position.y + border + 1, barProgress, height - (border * 2) - 2),
		    (*(multiplier > 0 ? healthBlue : health))[0], { 0, 0 });
	}

	constexpr auto GetBorderColor = [](MonsterClass monsterClass) {
		switch (monsterClass) {
		case MonsterClass::Undead:
			return 248;

		case MonsterClass::Demon:
			return 232;

		case MonsterClass::Animal:
			return 150;

		default:
			app_fatal(StrCat("Invalid monster class: ", static_cast<int>(monsterClass)));
		}
	};

	if (*sgOptions.Gameplay.showMonsterType) {
		Uint8 borderColor = GetBorderColor(monster.data().monsterClass);
		int borderWidth = width - (border * 2);
		UnsafeDrawHorizontalLine(out, { position.x + border, position.y + border }, borderWidth, borderColor);
		UnsafeDrawHorizontalLine(out, { position.x + border, position.y + height - border - 1 }, borderWidth, borderColor);
		int borderHeight = height - (border * 2) - 2;
		UnsafeDrawVerticalLine(out, { position.x + border, position.y + border + 1 }, borderHeight, borderColor);
		UnsafeDrawVerticalLine(out, { position.x + width - border - 1, position.y + border + 1 }, borderHeight, borderColor);
	}

	UiFlags style = UiFlags::AlignCenter | UiFlags::VerticalCenter;

	// Oracool: a lesser unique wears its modifier in front of a name of its OWN - "Warded Malgrith
	// the Unclean", not "Warded Rotfeast the Hungry". The borrowed champion's name is dropped
	// entirely, because a lesser unique borrows a sprite and a stat line, not an identity; keeping
	// the original name would tell the player they had met the floor's real champion when they had
	// not, and would make a repeated identity read as a duplication bug.
	//
	// This bar is the ONLY place a monster's name reaches the player (the cursor tooltip stopped
	// carrying monsters on an earlier request), so it is the only place either the modifier or the
	// name can be learned - and learning them is the whole point: a champion the player cannot read
	// is just a monster that unaccountably takes longer to kill.
	//
	// It is also what stands in for the "shrunken in size" the user asked for. There is no scale
	// parameter anywhere in the CLX renderer - see the design doc - and legibility, not literal size,
	// is what that request was after.
	// Oracool Phase 3.4: and the aura a champion beside it is lending, if any. A pack bonus the
	// player cannot read is just a monster that unaccountably hits harder - the same argument that
	// put the champion's own modifier on this bar in the first place.
	std::string displayName = oracool::GetMonsterDisplayName(monster);
	if (const char *lent = oracool::PackAuraName(monster); lent[0] != '\0')
		StrAppend(displayName, " (", _(lent), ")");
	const string_view name = displayName;

	DrawString(out, name, { position + Displacement { -1, 1 }, { width, height } }, { style | UiFlags::ColorBlack });
	if (monster.isUnique())
		style |= UiFlags::ColorWhitegold;
	else if (monster.leader != Monster::NoLeader)
		style |= UiFlags::ColorBlue;
	else
		style |= UiFlags::ColorWhite;
	DrawString(out, name, { position, { width, height } }, { style });

	if (multiplier > 0)
		DrawString(out, StrCat("x", multiplier), { position, { width - 2, height } }, { UiFlags::ColorWhite | UiFlags::AlignRight | UiFlags::VerticalCenter });

	// mlvl, BELOW the bar rather than inside it (user, 2026-08-26: "mlvl text should be below the
	// healthbar of the monsters, so it is more clearly visible. now it is overlaping with the
	// healthbar").
	//
	// The original request (2026-08-19) asked for it "somewhere under their health bar. maybe in the
	// right corner below bar", and `height - 13` put it in the bar's bottom-right corner instead -
	// inside the frame, on top of the bar art and whatever colour the health fill happened to be
	// behind it. It was legible against an empty bar and vanished against a full one.
	//
	// Now it sits clear of the frame entirely, still right-aligned, with the same black offset the
	// name above it uses - because outside the frame it is drawn over the dungeon floor, which is
	// noisier than any bar.
	//
	// This is the LOOT level - the number that decides what the kill can drop (oracool/area_level.h)
	// - not Monster::level(), which is the combat curve and a different question. A player reading
	// "mlvl 61" is reading what the corpse is worth.
	const std::string monsterLevelText = StrCat("mlvl ", ItemLevelOfMonster(monster));
	const Rectangle monsterLevelRect { position + Displacement { 0, height + 1 }, { width - 5, 12 } };
	DrawString(out, monsterLevelText,
	    { monsterLevelRect.position + Displacement { -1, 1 }, monsterLevelRect.size },
	    { UiFlags::ColorBlack | UiFlags::AlignRight });
	DrawString(out, monsterLevelText, monsterLevelRect,
	    { UiFlags::ColorUiSilverDark | UiFlags::AlignRight });

	// THE READOUT (user, 2026-09-05: "monsters Class/Hit Points/DMG/XP to be displayed under their
	// healthbar. Font White/RED/White/Gold"). Four lines down the left under the bar, each with the
	// same black offset the name and the mlvl wear, since this is drawn over the dungeon floor.
	// Hit points in whole points (the engine keeps them in 64ths), damage as the melee range the
	// monster rolls, XP as this difficulty pays for the kill - the same call the kill itself makes.
	//
	// AUDITED 2026-09-07 (user: "make sure monster stats are correct ... not some basic stats that
	// are wrong in uniques, minions and lesser uniques cases"). Hit points and damage are the
	// monster's OWN fields, which every tier writes at spawn: PrepareUniqueMonst (uniques, from
	// their own table), the minion scaling, the lesser-unique stat line, Hollow/Feral, and the
	// difficulty multipliers. Two things were NOT in the fields and are read here now: a champion's
	// Might, which MonsterAttack adds at swing time (PackAdjustedDamage), and the XP clamp, which
	// runs on the monster's LEVEL against the player's - a unique's level is double its table
	// level, so the raw table XP was not what the kill paid.
	{
		const auto className = [](MonsterClass monsterClass) -> string_view {
			switch (monsterClass) {
			case MonsterClass::Undead:
				return _("Undead");
			case MonsterClass::Demon:
				return _("Demon");
			case MonsterClass::Animal:
				return _("Animal");
			}
			return "";
		};
		const std::string lines[] = {
			fmt::format(fmt::runtime(_("Class: {:s}")), className(monster.data().monsterClass)),
			fmt::format(fmt::runtime(_("Hit Points: {:d} / {:d}")), monster.hitPoints >> 6, monster.maxHitPoints >> 6),
			fmt::format(fmt::runtime(_("Damage: {:d} - {:d}")), oracool::PackAdjustedDamage(monster, monster.minDamage), oracool::PackAdjustedDamage(monster, monster.maxDamage)),
			fmt::format(fmt::runtime(_("XP: {:d}")), KillExperienceFor(*MyPlayer, static_cast<int>(monster.level(sgGameInitInfo.nDifficulty)), static_cast<int>(monster.exp(sgGameInitInfo.nDifficulty)))),
		};
		const UiFlags colors[] = { UiFlags::ColorWhite, UiFlags::ColorRed, UiFlags::ColorWhite, UiFlags::ColorGold };
		constexpr int ReadoutLineHeight = 12;
		for (int i = 0; i < 4; i++) {
			const Rectangle line { position + Displacement { 5, height + 1 + i * ReadoutLineHeight }, { width - 10, ReadoutLineHeight } };
			DrawString(out, lines[i], { line.position + Displacement { -1, 1 }, line.size }, { UiFlags::ColorBlack });
			DrawString(out, lines[i], line, { colors[i] });
		}
	}
	if (monster.isUnique() || MonsterKillCounts[monster.type().type] >= 15) {
		monster_resistance immunes[] = { IMMUNE_MAGIC, IMMUNE_FIRE, IMMUNE_LIGHTNING };
		monster_resistance resists[] = { RESIST_MAGIC, RESIST_FIRE, RESIST_LIGHTNING };

		// Oracool bug fix (2026-08-16): these read monster.resistance directly, so a Paladin
		// standing in his own lit Conviction watched the bar keep showing an immunity he had just
		// broken. EffectiveResistances is the authority the damage path uses; the bar must agree
		// with it, or the aura's whole payoff is invisible.
		const uint16_t shown = oracool::EffectiveResistances(monster);
		// LEFT of the frame, not under its left corner (user, 2026-09-07: "move the shields left of
		// frame"). They hung from height-6 into the readout that arrived on 2026-09-05, across the
		// Class and Hit Points lines. Now they run leftward from a 5px gap at the frame's left edge,
		// vertically centred on it, still magic-fire-lightning left to right - which means laying
		// them from the right, lightning first.
		const int iconWidth = (*resistance)[0].width();
		const int iconY = (height - (*resistance)[0].height()) / 2;
		int resX = position.x - 5;
		for (size_t i = 3; i-- > 0;) {
			OptionalClxSprite icon;
			if ((shown & immunes[i]) != 0)
				icon = (*resistance)[i * 2 + 1];
			else if ((shown & resists[i]) != 0)
				icon = (*resistance)[i * 2];
			if (!icon)
				continue;
			resX -= iconWidth;
			RenderClxSprite(out, *icon, { resX, position.y + iconY });
			resX -= 2;
		}
	}

	if (Players.size() > 1) {
		int tagOffset = 5;
		for (size_t i = 0; i < Players.size(); i++) {
			if (((1U << i) & monster.whoHit) != 0) {
				RenderClxSprite(out, (*playerExpTags)[i + 1], position + Displacement { tagOffset, height - 31 });
			} else if (Players[i].plractive) {
				RenderClxSprite(out, (*playerExpTags)[0], position + Displacement { tagOffset, height - 31 });
			}
			tagOffset += (*playerExpTags)[0].width();
		}
	}
}

} // namespace devilution
