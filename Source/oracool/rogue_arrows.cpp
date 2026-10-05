#include "oracool/rogue_arrows.h"

#include <algorithm>
#include <cstdlib>
#include <vector>

#include "control.h"
#include "effects.h" // PlaySfxLoc, PS_BFIRE - the Strafe spin's shots
#include "lighting.h" // AddLight - each skill arrow's own light
#include "engine/backbuffer_state.hpp"
#include "engine/random.hpp"
#include "missiles.h"
#include "monster.h"
#include "oracool/paladin_skills.h" // MissilePoolHasRoom
#include "oracool/passives.h"
#include "oracool/rfa12_actives.h" // SightLineClear - Strafe's targets
#include "player.h"
#include "spells.h"
#include "utils/language.h"
#include <fmt/format.h>
#include "oracool/cold.h"

namespace devilution::oracool {

namespace {

/** The latch - see the header, and paladin_melee.cpp's ArmedSkill, which this mirrors. */
std::optional<RogueArrow> ArmedArrow;
std::optional<SpellID> ArmedBowSpell;
/** A quick-cast armed the bow latch: it is one shot's, and goes after it (round 95 audit). */
bool BowLatchOneShot = false;
/** What the held button had armed before the key's shot: put back after it (round 96 audit). */
std::optional<RogueArrow> HeldArrowBeforeKey;
std::optional<SpellID> HeldBowSpellBeforeKey;
std::optional<SpellID> ArmedThrow;
Point ArmedThrowTarget;

/** @brief The rank bonus an elemental arrow adds on top of the bow: two a rank, plus two. */
int ElementalBonus(int spellLevel)
{
	return 2 + 2 * std::max(spellLevel, 0);
}

/** @brief How many arrows Multiple Shot fans out: two, plus one every two ranks, six at most. */
int MultipleShotCount(int spellLevel)
{
	return std::min(2 + std::max(spellLevel, 0) / 2, 6);
}

/** @brief How many enemies Strafe answers: three, plus one every two ranks, eight at most. */
int StrafeCount(int spellLevel)
{
	return std::min(3 + std::max(spellLevel, 0) / 2, 8);
}

/** @brief The missile a skill's arrow flies as. The element is the missile's; the behaviour is var5's. */
MissileID MissileFor(RogueArrow arrow)
{
	switch (arrow) {
	case RogueArrow::MagicArrow:
		return MissileID::MagicArrow;
	case RogueArrow::FireArrow:
	case RogueArrow::ExplodingArrow:
	case RogueArrow::ImmolationArrow:
		return MissileID::FlameArrow;
	case RogueArrow::ColdArrow:
	case RogueArrow::IceArrow:
	case RogueArrow::FreezingArrow:
		return MissileID::FrostArrow;
	case RogueArrow::GuidedArrow:
		return MissileID::GuidedArrow;
	case RogueArrow::MultipleShot:
	case RogueArrow::Strafe:
		break;
	}
	return MissileID::SkillArrow;
}

/** @brief One arrow, carrying its skill in var5 and its rank in _mispllvl. */
void Loose(Player &player, RogueArrow arrow, Point target, int spellLevel)
{
	Missile *missile = AddMissile(player.position.tile, target, player._pdir, MissileFor(arrow),
	    TARGET_MONSTERS, static_cast<int>(player.getId()), 0, spellLevel);
	if (missile == nullptr)
		return;
	missile->var5 = static_cast<int>(arrow);
	// The vanilla arrow in the skill's colour, with a light of its own carried along (the Rogue Skill Cards page,
	// 2026-10-04): AddRogueArrow flies every one as the plain arrow, and ProcessRogueArrow walks the light with it.
	const RogueArrowLook look = RogueArrowLookOf(arrow);
	missile->oracoolTint = look.tint;
	missile->oracoolTintRgb = look.rgb;
	if (look.light > 0 && !missile->_miDelFlag) {
		if (missile->_mlid != NO_LIGHT)
			AddUnLight(missile->_mlid);
		missile->_mlid = AddLight(missile->position.tile, look.light);
	}
}

/** @brief A unit step at right angles to the shot, for fanning arrows out beside the target. */
Displacement Perpendicular(Point from, Point to)
{
	const int dx = to.x - from.x;
	const int dy = to.y - from.y;
	// The longer axis decides which way "sideways" is, so a fan is always wide across the shot
	// rather than along it.
	if (std::abs(dx) >= std::abs(dy))
		return { 0, 1 };
	return { 1, 0 };
}

} // namespace

std::optional<RogueArrow> RogueArrowForSpell(SpellID spell)
{
	switch (spell) {
	case SpellID::MagicArrow:
		return RogueArrow::MagicArrow;
	case SpellID::FireArrow:
		return RogueArrow::FireArrow;
	case SpellID::ColdArrow:
		return RogueArrow::ColdArrow;
	case SpellID::MultipleShot:
		return RogueArrow::MultipleShot;
	case SpellID::ExplodingArrow:
		return RogueArrow::ExplodingArrow;
	case SpellID::IceArrow:
		return RogueArrow::IceArrow;
	case SpellID::GuidedArrow:
		return RogueArrow::GuidedArrow;
	case SpellID::Strafe:
		return RogueArrow::Strafe;
	case SpellID::ImmolationArrow:
		return RogueArrow::ImmolationArrow;
	case SpellID::FreezingArrow:
		return RogueArrow::FreezingArrow;
	default:
		return std::nullopt;
	}
}

SpellID RogueArrowSpell(RogueArrow arrow)
{
	switch (arrow) {
	case RogueArrow::MagicArrow:
		return SpellID::MagicArrow;
	case RogueArrow::FireArrow:
		return SpellID::FireArrow;
	case RogueArrow::ColdArrow:
		return SpellID::ColdArrow;
	case RogueArrow::MultipleShot:
		return SpellID::MultipleShot;
	case RogueArrow::ExplodingArrow:
		return SpellID::ExplodingArrow;
	case RogueArrow::IceArrow:
		return SpellID::IceArrow;
	case RogueArrow::GuidedArrow:
		return SpellID::GuidedArrow;
	case RogueArrow::Strafe:
		return SpellID::Strafe;
	case RogueArrow::ImmolationArrow:
		return SpellID::ImmolationArrow;
	case RogueArrow::FreezingArrow:
		return SpellID::FreezingArrow;
	}
	return SpellID::Invalid;
}

void ArmArrowSkill(std::optional<RogueArrow> arrow)
{
	BowLatchOneShot = false;
	ArmedArrow = arrow;
	ArmedBowSpell = std::nullopt;
	ArmedThrow = std::nullopt;
}

std::optional<RogueArrow> ArmedArrowSkill()
{
	return ArmedArrow;
}

bool IsJavelinStrike(SpellID spell)
{
	return IsAnyOf(spell, SpellID::Jab, SpellID::PowerStrike, SpellID::Impale, SpellID::ChargedStrike, SpellID::Fend, SpellID::LightningStrike,
	    SpellID::Sweep, SpellID::ReapingPoint, SpellID::TurningPike);
}

bool IsJavelinThrow(SpellID spell)
{
	return IsAnyOf(spell, SpellID::PoisonJavelin, SpellID::PlagueJavelin, SpellID::Harpoon, SpellID::AnchorJavelin, SpellID::ValkyriesSpear);
}

int JavelinThrowRange(SpellID spell)
{
	return IsAnyOf(spell, SpellID::Harpoon, SpellID::ValkyriesSpear) ? 8 : 6;
}

void ArmJavelinThrow(std::optional<SpellID> spell, Point target)
{
	ArmedArrow = std::nullopt;
	ArmedBowSpell = std::nullopt;
	ArmedThrow = spell;
	ArmedThrowTarget = target;
}

std::optional<SpellID> ArmedJavelinThrow()
{
	return ArmedThrow;
}

Point ArmedJavelinThrowTarget()
{
	return ArmedThrowTarget;
}

void ArmBowSkillSpell(SpellID spell)
{
	BowLatchOneShot = false;
	ArmedArrow = std::nullopt;
	ArmedThrow = std::nullopt;
	ArmedBowSpell = spell;
}

std::optional<SpellID> ArmedBowSkillSpell()
{
	return ArmedBowSpell;
}

void MarkBowLatchOneShot(std::optional<RogueArrow> heldArrow, std::optional<SpellID> heldBowSpell)
{
	if (!ArmedArrow.has_value() && !ArmedBowSpell.has_value())
		return;
	BowLatchOneShot = true;
	HeldArrowBeforeKey = heldArrow;
	HeldBowSpellBeforeKey = heldBowSpell;
}

bool IsBowLatchOneShot()
{
	return BowLatchOneShot;
}

void DropOneShotBowLatch()
{
	if (!BowLatchOneShot)
		return;
	BowLatchOneShot = false;
	// The held button's own skill comes back for the rest of the hold (round 96 audit: a held Multishot went on as plain arrows).
	ArmedArrow = HeldArrowBeforeKey;
	ArmedBowSpell = ArmedArrow.has_value() ? std::nullopt : HeldBowSpellBeforeKey;
	HeldArrowBeforeKey = std::nullopt;
	HeldBowSpellBeforeKey = std::nullopt;
}

RogueArrowLook RogueArrowLookOf(RogueArrow arrow)
{
	// The page's words per card: Magic Arrow "tinted pink, light radius 2"; Fire, Exploding, Immolation and Guided Arrow
	// "tinted fire, colorcycling to imitate burning, light radius 2"; Cold, Ice and Freezing Arrow "tinted ice-cold,
	// colorcycling to imitate ice reflection, lightradius 2"; Multiple Shot "use regular arrows".
	switch (arrow) {
	case RogueArrow::MagicArrow:
		return { Tint::Hue, hue::ArrowPink, 2 };
	case RogueArrow::FireArrow:
	case RogueArrow::ExplodingArrow:
	case RogueArrow::ImmolationArrow:
	case RogueArrow::GuidedArrow:
		return { Tint::Glint, hue::FireOrange, 2 };
	case RogueArrow::ColdArrow:
	case RogueArrow::IceArrow:
	case RogueArrow::FreezingArrow:
		return { Tint::Glint, hue::IceBlue, 2 };
	case RogueArrow::MultipleShot:
	case RogueArrow::Strafe:
		break;
	}
	return {};
}

int StrafeAimPercent(int spellLevel)
{
	return std::min(60 + 5 * (std::max(spellLevel, 1) - 1), 95);
}

void LooseStrafeShot(Player &player)
{
	const int spellLevel = player.GetSpellLevel(SpellID::Strafe);
	std::vector<Point> inView;
	for (size_t i = 0; i < ActiveMonsterCount; i++) {
		const Monster &monster = Monsters[ActiveMonsters[i]];
		if (monster.hitPoints >> 6 <= 0 || monster.isPlayerMinion() || (monster.flags & MFLAG_HIDDEN) != 0 || !monster.isPossibleToHit())
			continue;
		if (player.position.tile.WalkingDistance(monster.position.tile) > 8 || !SightLineClear(player.position.tile, monster.position.tile))
			continue;
		inView.push_back(monster.position.tile);
	}
	Point target;
	if (!inView.empty() && GenerateRnd(100) < StrafeAimPercent(spellLevel)) {
		target = inView[static_cast<size_t>(GenerateRnd(static_cast<int>(inView.size())))];
	} else {
		// Off into the room: any of the eight ways, five tiles out - the spin's arrows fly all round her.
		const auto way = static_cast<Direction>(GenerateRnd(8));
		target = player.position.tile + Displacement(way) * 5;
	}
	if (target == player.position.tile)
		return;
	Loose(player, RogueArrow::Strafe, target, spellLevel);
	PlaySfxLoc(PS_BFIRE, player.position.tile);
}

void FireArrowSkill(Player &player, RogueArrow arrow, Point target)
{
	const SpellID spell = RogueArrowSpell(arrow);
	const int spellLevel = player.GetSpellLevel(spell);

	// The price, re-checked here rather than trusted from the click: this is the one place mana
	// leaves the player for a bow skill, and hold-to-fire reaches it many times per click.
	const int cost = GetManaAmount(player, spell);
	if (player._pMana < cost) {
		// Degrade, do not jam: a plain arrow, and the latch dropped so the next shot does not ask.
		ArmArrowSkill(std::nullopt);
		AddMissile(player.position.tile, target, player._pdir, MissileID::Arrow, TARGET_MONSTERS,
		    static_cast<int>(player.getId()), 4, 0);
		if (&player == MyPlayer)
			player.Say(HeroSpeech::NotEnoughMana);
		return;
	}
	// Room for the arrows before the mana, as the Paladin's casts ask: a full missile pool took the cost for nothing
	// (round 6 audit, v1.12.231).
	if (!MissilePoolHasRoom())
		return;
	player._pMana -= cost;
	player._pManaBase -= cost;
	oracool::OnPassiveManaSpent(player, cost);
	RedrawComponent(PanelDrawComponent::Mana);

	switch (arrow) {
	case RogueArrow::MultipleShot: {
		const int count = MultipleShotCount(spellLevel);
		const Displacement side = Perpendicular(player.position.tile, target);
		// Centred on the target: for five arrows, offsets -2..2; for four, -2..1 with the fan
		// leaning one step, which is invisible at bow range and keeps the arithmetic to one line.
		for (int i = 0; i < count; i++) {
			const int offset = i - count / 2;
			Loose(player, arrow, target + side * offset, spellLevel);
		}
		return;
	}
	case RogueArrow::Strafe: {
		// Every enemy in view, nearest first, up to the count. "In view" is a clear line and eight
		// tiles - the bow's own reach - and a monster already down or on your side is not a target.
		std::vector<std::pair<int, Point>> targets;
		for (size_t i = 0; i < ActiveMonsterCount; i++) {
			const Monster &monster = Monsters[ActiveMonsters[i]];
			// Nor one no arrow can hit - a talker, a charging or retreating counselor: the arrow passed through and counted
			// against the volley (round 22 audit).
			if (monster.hitPoints >> 6 <= 0 || monster.isPlayerMinion() || (monster.flags & MFLAG_HIDDEN) != 0 || !monster.isPossibleToHit())
				continue;
			const int distance = player.position.tile.WalkingDistance(monster.position.tile);
			if (distance > 8 || !SightLineClear(player.position.tile, monster.position.tile)) // nor past a closed door (round 80)
				continue;
			targets.emplace_back(distance, monster.position.tile);
		}
		std::sort(targets.begin(), targets.end(), [](const auto &a, const auto &b) { return a.first < b.first; });
		const int count = StrafeCount(spellLevel);
		if (targets.empty()) {
			Loose(player, arrow, target, spellLevel); // nothing in view: the shot still goes where it was aimed
			return;
		}
		for (int i = 0; i < count && i < static_cast<int>(targets.size()); i++)
			Loose(player, arrow, targets[static_cast<size_t>(i)].second, spellLevel);
		return;
	}
	default:
		Loose(player, arrow, target, spellLevel);
		return;
	}
}

void RogueArrowDamage(const Player &player, SpellID spell, int spellLevel, int &minDamage, int &maxDamage)
{
	minDamage = -1;
	maxDamage = -1;
	const std::optional<RogueArrow> arrow = RogueArrowForSpell(spell);
	if (!arrow.has_value())
		return;
	// The bow's own range, which the sheet already knows as the character's ranged damage.
	minDamage = player._pIMinDam;
	maxDamage = player._pIMaxDam;
	switch (*arrow) {
	case RogueArrow::MagicArrow:
	case RogueArrow::FireArrow:
	case RogueArrow::ColdArrow:
	case RogueArrow::ExplodingArrow:
	case RogueArrow::IceArrow:
	case RogueArrow::ImmolationArrow:
	case RogueArrow::FreezingArrow: {
		// The bow's WHOLE damage, as the sheet shows it: the missile path adds the +% and the Strength part to a PHYSICAL
		// hit only, so these arrows landed at the bare weapon dice - well under a plain arrow (round 8 audit, v1.12.233).
		const int statShare = player._pClass == HeroClass::Rogue || !player.UsesRangedWeapon() ? 100 : 50; // the pool's stat share, as the arrow takes it (round 80)
		minDamage = PooledWeaponDamage(player, minDamage, 0, statShare);
		maxDamage = PooledWeaponDamage(player, maxDamage, 0, statShare);
		minDamage += ElementalBonus(spellLevel);
		maxDamage += ElementalBonus(spellLevel);
		break;
	}
	case RogueArrow::MultipleShot:
	case RogueArrow::GuidedArrow:
	case RogueArrow::Strafe: {
		// Physical arrows: MonsterMHit adds the +%, the flat bonus and the Strength part to them, so the slot quotes the
		// same sum - it read the bare weapon dice (round 12 audit, v1.12.237).
		// Glass Cannon in the pool, as the arrow's hit and the sheet's Damage line add it (round 58 audit).
		const int statShare = player._pClass == HeroClass::Rogue || !player.UsesRangedWeapon() ? 100 : 50; // the pool's stat share, as the arrow takes it (round 80)
		const int always = PassiveUnconditionalDamagePercent(player);
		minDamage = PooledWeaponDamage(player, minDamage, always, statShare);
		maxDamage = PooledWeaponDamage(player, maxDamage, always, statShare);
		break;
	}
	}
}

const char *RogueArrowDescription(SpellID spell)
{
	switch (spell) {
	case SpellID::MagicArrow:
		return N_("An arrow of pure force: your bow's damage as magic, plus a little a rank.");
	case SpellID::FireArrow:
		return N_("An arrow wrapped in flame: your bow's damage as fire, plus a little a rank.");
	case SpellID::ColdArrow:
		return N_("An arrow sheathed in frost: your bow's damage as cold, and it chills what it hits.");
	case SpellID::MultipleShot:
		return N_("Looses a fan of arrows at once - two, and one more every two ranks, six at most.");
	case SpellID::ExplodingArrow:
		return N_("A fire arrow that bursts where it stops, burning the tiles around it.");
	case SpellID::IceArrow:
		return N_("A frost arrow that freezes what it hits solid for a moment.");
	case SpellID::GuidedArrow:
		return N_("An arrow that cannot miss.");
	case SpellID::Strafe:
		return N_("Hold the mouse button: she spins and glides toward the cursor, loosing an arrow every quarter second, mostly at enemies in view. Costs its mana every second.");
	case SpellID::ImmolationArrow:
		return N_("A fire arrow that leaves a wall of flame burning where it stops.");
	case SpellID::FreezingArrow:
		return N_("A frost arrow that freezes everything around where it stops.");
	default:
		return "";
	}
}

std::string RogueArrowFactsAt(RogueArrow arrow, int spellLevel)
{
	const int level = std::max(spellLevel, 0);
	std::string out;
	const auto line = [&out](const std::string &s) {
		if (!out.empty())
			out += '\n';
		out += s;
	};
	switch (arrow) {
	case RogueArrow::MagicArrow:
	case RogueArrow::FireArrow:
	case RogueArrow::ExplodingArrow:
	case RogueArrow::ImmolationArrow:
	case RogueArrow::ColdArrow:
	case RogueArrow::IceArrow:
	case RogueArrow::FreezingArrow:
		line(fmt::format(fmt::runtime(_("Elemental damage: +{:d}")), ElementalBonus(level)));
		break;
	default:
		break;
	}
	switch (arrow) {
	case RogueArrow::MultipleShot:
		line(fmt::format(fmt::runtime(_("Arrows: {:d}")), MultipleShotCount(level)));
		break;
	case RogueArrow::Strafe:
		// The held spin (dev note, 2026-10-03): an arrow every quarter second, mostly at enemies in view.
		line(std::string(_("Hold: spin and glide, an arrow every 0.25 s")));
		line(fmt::format(fmt::runtime(_("Aimed at enemies within 8 tiles: {:d}%")), StrafeAimPercent(level)));
		line(std::string(_("Mana: its cost every second")));
		break;
	case RogueArrow::GuidedArrow:
		line(std::string(_("Cannot miss")));
		break;
	case RogueArrow::ColdArrow:
		line(fmt::format(fmt::runtime(_("Chill: {:.1f} s")), ChillSecondsTenths(level) / 10.0));
		break;
	case RogueArrow::IceArrow:
	case RogueArrow::FreezingArrow:
		line(fmt::format(fmt::runtime(_("Freeze: {:.1f} s")), FreezeSecondsTenths(level) / 10.0));
		break;
	default:
		break;
	}
	return out;
}

} // namespace devilution::oracool
