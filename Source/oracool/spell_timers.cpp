#include "oracool/spell_timers.h"

#include <array>
#include <vector>

#include <fmt/format.h>

#include "DiabloUI/ui_flags.hpp"
#include "automap.h"
#include "engine/palette.h"
#include "engine/rectangle.hpp"
#include "engine/render/primitive_render.hpp"
#include "engine/render/text_render.hpp"
#include "missiles.h"
#include "oracool/cold.h"
#include "oracool/hud_art.h"
#include "oracool/rfa12_actives.h"
#include "oracool/warcries.h"
#include "player.h"
#include "spelldat.h"

namespace devilution::oracool {

namespace {

/** @brief The user's numbers: a 28px square, 6px off the mini-map's left edge, rows 6px apart. */
constexpr int IconSize = 28;
constexpr int Gap = 6;
/** @brief The seconds sit left of the icon, right-aligned against it with this much air. */
constexpr int TextGap = 4;
constexpr int TextWidth = 40;
/** @brief The clock every timed effect counts on: 20 ticks to a game second. */
constexpr int TicksPerSecond = 20;
/** @brief The square's blue, and the palette's blue ramp for a screen without colour values. */
constexpr uint32_t BlueBacking = 0x24408C;
constexpr uint8_t BlueBackingIndex = PAL16_BLUE + 4;

struct Timer {
	SpellID spell;
	int ticks;
};

/** @brief The seven cries that leave a timed buff on the caster (warcries.h). */
constexpr std::array<SpellID, 7> WarcryBuffs {
	SpellID::Shout, SpellID::BattleOrders, SpellID::BattleCommand, SpellID::PurifyingBreath,
	SpellID::Vengeance, SpellID::SlowMissiles, SpellID::Tranquility
};

/**
 * @brief The seventeen RfA-12 casts that leave a timed effect on the caster (rfa12_actives.h) - the Necromancer's
 * Bone Armor, Poison Dagger and Bone Storm since 2026-09-26. Their icons are the letter plate until his strip lands.
 */
constexpr std::array<SpellID, 17> Rfa12Buffs {
	SpellID::RallyingCry, SpellID::IronWill, SpellID::Bloodcall, SpellID::StaticCharge, SpellID::Conduit,
	SpellID::Immolate, SpellID::ChordOfWarding, SpellID::Feedback, SpellID::MusicOfTheSpheres, SpellID::Saga,
	SpellID::MantraOfClarity, SpellID::MantraOfEvasion, SpellID::MantraOfRetribution, SpellID::AstralProjection,
	SpellID::BoneArmor, SpellID::PoisonDagger, SpellID::BoneStorm
};

/**
 * @brief Every timed effect @p player carries, in a fixed order so a row never jumps when another
 * starts or ends ahead of it: the engine's own spells first, then the cries, then the RfA-12 buffs.
 *
 * The engine's timed spells keep their clock on the MISSILE that carries them (_mirange counts down
 * to the end), so those are found by walking the player's missiles. Rage counts only while it is
 * active - the same missile then runs the cooldown, which is not a spell the hero is under.
 */
std::vector<Timer> ActiveTimers(const Player &player)
{
	std::vector<Timer> timers;
	std::array<int, 8> missileTicks {}; // Infravision, Etherealize, Search, Rage, Blizzard, Guardian, Fire Wall, Lightning Wall
	for (Missile &missile : Missiles) {
		if (missile._miDelFlag || missile.sourceType() != MissileSource::Player || missile._misource != static_cast<int>(player.getId()))
			continue;
		switch (missile._mitype) {
		case MissileID::Infravision:
			missileTicks[0] = std::max(missileTicks[0], missile._mirange);
			break;
		case MissileID::Etherealize:
			missileTicks[1] = std::max(missileTicks[1], missile._mirange);
			break;
		case MissileID::Search:
			missileTicks[2] = std::max(missileTicks[2], missile._mirange);
			break;
		case MissileID::Rage:
			if (HasAnyOf(player._pSpellFlags, SpellFlag::RageActive))
				missileTicks[3] = std::max(missileTicks[3], missile._mirange);
			break;
		// The spells whose missile stands on the ground for their duration (dev note, 2026-10-01: "skill with runtime to
		// have countdown timer"); a wall's segments each count, the longest shows.
		case MissileID::Blizzard:
			missileTicks[4] = std::max(missileTicks[4], missile._mirange);
			break;
		case MissileID::Guardian:
			missileTicks[5] = std::max(missileTicks[5], missile._mirange);
			break;
		case MissileID::FireWall:
			missileTicks[6] = std::max(missileTicks[6], missile._mirange);
			break;
		case MissileID::LightningWall:
			missileTicks[7] = std::max(missileTicks[7], missile._mirange);
			break;
		default:
			break;
		}
	}
	const std::array<SpellID, 8> missileSpells { SpellID::Infravision, SpellID::Etherealize, SpellID::Search, SpellID::Rage,
		SpellID::Blizzard, SpellID::Guardian, SpellID::FireWall, SpellID::LightningWall };
	for (size_t i = 0; i < missileSpells.size(); i++) {
		if (missileTicks[i] > 0)
			timers.push_back({ missileSpells[i], missileTicks[i] });
	}
	for (SpellID spell : WarcryBuffs) {
		if (const int ticks = WarcryBuffTicks(player, spell); ticks > 0)
			timers.push_back({ spell, ticks });
	}
	for (SpellID spell : Rfa12Buffs) {
		if (const int ticks = Rfa12BuffTicks(player, spell); ticks > 0)
			timers.push_back({ spell, ticks });
	}
	// The ice armours (dev note, 2026-10-01), then the ground effects that last 5 s or more.
	for (SpellID spell : { SpellID::FrozenArmor, SpellID::ShiverArmor, SpellID::ChillingArmor }) {
		if (const int ticks = ColdArmourTicks(player, spell); ticks > 0)
			timers.push_back({ spell, ticks });
	}
	for (const auto &[spell, ticks] : Rfa12FieldTimers(player))
		timers.push_back({ spell, ticks });
	return timers;
}

} // namespace

void DrawSpellTimers(const Surface &out)
{
	if (MyPlayer == nullptr)
		return;
	const Player &player = *MyPlayer;
	const std::vector<Timer> timers = ActiveTimers(player);
	if (timers.empty())
		return;

	const Rectangle miniMap = GetMiniMapScreenRect();
	const int iconX = miniMap.position.x - Gap - IconSize;
	int y = miniMap.position.y;
	for (const Timer &timer : timers) {
		const Rectangle icon { { iconX, y }, { IconSize, IconSize } };
		FillRectRgb(out, icon.position.x, icon.position.y, IconSize, IconSize, BlueBacking, BlueBackingIndex);
		DrawTimedSpellIcon(out, icon, player._pClass, timer.spell);

		// Whole seconds, rounded up: the last second reads 1 until the effect is gone, never 0.
		const int seconds = (timer.ticks + TicksPerSecond - 1) / TicksPerSecond;
		const Rectangle text { { iconX - TextGap - TextWidth, y }, { TextWidth, IconSize } };
		DrawString(out, fmt::format("{:d}", seconds), text,
		    { UiFlags::AlignRight | UiFlags::VerticalCenter | UiFlags::FontSize12 | UiFlags::ColorWhite | UiFlags::Shadowed });

		y += IconSize + Gap;
	}
}

} // namespace devilution::oracool
