/**
 * @file spells.cpp
 *
 * Implementation of functionality for casting player spells.
 */
#include "spells.h"
#include "oracool/class_tree.h"
#include "oracool/paladin_ranged.h"
#include "oracool/paladin_skills.h"
#include "oracool/passives.h"
#include "oracool/rage.h"
#include "oracool/skill_sounds.h"

#include "control.h"
#include "cursor.h"
#ifdef _DEBUG
#include "debug.h"
#endif
#include "engine/backbuffer_state.hpp"
#include "engine/point.hpp"
#include "engine/random.hpp"
#include "gamemenu.h"
#include "inv.h"
#include "missiles.h"
#include "options.h"

namespace devilution {

namespace {

/**
 * @brief Whether a SpellType::Skill cast of @p spell costs its listed mana.
 *
 * Oracool, Round 2. Three kinds of thing are SpellType::Skill: vanilla's class skills, priced at
 * zero; the Paladin's seven, which pay through SpendPaladinSkillMana on their own path and never
 * reach CastSpell; and the tree rows this fork adds, which have a price in SpellsData and, until
 * this, nobody to collect it. So: a price, and not a Paladin skill.
 */
bool SkillPaysMana(SpellID spell)
{
	return GetSpellData(spell).sManaCost > 0 && !oracool::PaladinSkillForSpell(spell).has_value();
}

/**
 * @brief Gets a value indicating whether the player's current readied spell is a valid spell. Readied spells can be
 * invalidaded in a few scenarios where the spell comes from items, for example (like dropping the only scroll that
 * provided the spell).
 * @param player The player whose readied spell is to be checked.
 * @return 'true' when the readied spell is currently valid, and 'false' otherwise.
 */
bool IsReadiedSpellValid(const Player &player)
{
	switch (player._pRSplType) {
	case SpellType::Skill:
	case SpellType::Spell:
	case SpellType::Invalid:
		return true;

	case SpellType::Charges:
		return (player._pISpells & GetSpellBitmask(player._pRSpell)) != 0;

	case SpellType::Scroll:
		return (player._pScrlSpells & GetSpellBitmask(player._pRSpell)) != 0;

	default:
		return false;
	}
}

} // namespace

// Oracool: lifted out of the anonymous namespace above. "No spell readied" is not just an internal
// housekeeping state any more - it is the Regular Attack the Abilities window lists and the HUD's
// skill wells draw, so more than one place needs to be able to put the player back into it. Two of
// them were already writing the same pair of fields by hand (control.cpp's shift-click on the RMB
// well); one named function is what stops a third from doing it slightly differently.
void ClearReadiedSpell(Player &player)
{
	if (player._pRSpell != SpellID::Invalid) {
		player._pRSpell = SpellID::Invalid;
		RedrawEverything();
	}

	if (player._pRSplType != SpellType::Invalid) {
		player._pRSplType = SpellType::Invalid;
		RedrawEverything();
	}
}

bool IsValidSpell(SpellID spl)
{
	return spl > SpellID::Null
	    && spl <= SpellID::LAST
	    && (spl <= SpellID::LastDiablo || gbIsHellfire);
}

bool IsValidSpellFrom(int spellFrom)
{
	if (spellFrom == 0)
		return true;
	if (spellFrom >= INVITEM_INV_FIRST && spellFrom <= INVITEM_INV_LAST)
		return true;
	if (spellFrom >= INVITEM_BELT_FIRST && spellFrom <= INVITEM_BELT_LAST)
		return true;
	return false;
}

bool IsWallSpell(SpellID spl)
{
	return spl == SpellID::FireWall || spl == SpellID::LightningWall;
}

bool TargetsMonster(SpellID id)
{
	return id == SpellID::Fireball
	    || id == SpellID::FireWall
	    || id == SpellID::Inferno
	    || id == SpellID::Lightning
	    || id == SpellID::StoneCurse
	    || id == SpellID::FlameWave;
}

int GetManaAmountAtLevel(const Player &player, SpellID sn, int spellLevel)
{
	if (sn == SpellID::TownPortal && !gbIsMultiplayer)
		return 0;

	int ma; // mana amount

	// mana adjust
	int adj = 0;

	// spell level
	// Oracool: the LEVEL IS A PARAMETER (2026-08-31). This body is vanilla's, unchanged, except that
	// it took the level from player.GetSpellLevel(sn) itself - which made "what would this cost one
	// level from now" unaskable. The Abilities panel asks it, because mana FALLS as a spell levels
	// and a player deciding where to put a point should be able to see that.
	//
	// GetManaAmount below passes the player's own level, so every existing caller is bit-for-bit
	// unchanged.
	int sl = std::max(spellLevel - 1, 0);

	if (sl > 0) {
		adj = sl * GetSpellData(sn).sManaAdj;
	}
	if (sn == SpellID::Firebolt) {
		adj /= 2;
	}
	if (sn == SpellID::Resurrect && sl > 0) {
		adj = sl * (GetSpellData(SpellID::Resurrect).sManaCost / 8);
	}

	if (sn == SpellID::Healing || sn == SpellID::HealOther) {
		ma = (GetSpellData(SpellID::Healing).sManaCost + 2 * player._pLevel - adj);
	} else if (GetSpellData(sn).sManaCost == 255) {
		ma = (player._pMaxManaBase >> 6) - adj;
	} else {
		ma = (GetSpellData(sn).sManaCost - adj);
	}

	ma = std::max(ma, 0);
	ma <<= 6;

	if (gbIsHellfire && player._pClass == HeroClass::Sorcerer) {
		ma /= 2;
	} else if (player._pClass == HeroClass::Rogue || player._pClass == HeroClass::Monk || player._pClass == HeroClass::Bard) {
		ma -= ma / 4;
	}

	// Chant of Resonance (2026-09-14): the Monk's mantras, cheaper.
	ma += ma * oracool::PassiveManaCostPercent(player, sn) / 100;

	if (GetSpellData(sn).sMinMana > ma >> 6) {
		ma = GetSpellData(sn).sMinMana << 6;
	}

	return ma;
}

int GetManaAmount(const Player &player, SpellID sn)
{
	return GetManaAmountAtLevel(player, sn, player.GetSpellLevel(sn));
}

void ConsumeSpell(Player &player, SpellID sn)
{
	switch (player.executedSpell.spellType) {
	case SpellType::Skill:
		// The tree skills' price - see CheckSpell. Same subtraction as the Spell case below, kept
		// apart from it so vanilla's free skills stay free without a second condition down there.
		// The Barbarian settles in Rage instead (2026-09-13): a spender pays, a generator fills - and
		// only now, past the fizzle check, so a Backhand that struck nothing earns nothing.
		if (oracool::UsesRage(player)) {
			// A cast generator reaches here only when it struck (Backhand's cast fizzles on an empty
			// tile), so it landed one blow. A spender's cast is not counted as a blow.
			oracool::SettleSkill(player, sn, oracool::RageGain(sn) > 0 ? 1 : 0);
		} else if (SkillPaysMana(sn)) {
			const int ma = GetManaAmount(player, sn);
			player._pMana -= ma;
			player._pManaBase -= ma;
			oracool::OnPassiveManaSpent(player, ma);
			RedrawComponent(PanelDrawComponent::Mana);
		}
		break;
	case SpellType::Invalid:
		break;
	case SpellType::Scroll:
		ConsumeScroll(player);
		break;
	case SpellType::Charges:
		ConsumeStaffCharge(player);
		break;
	case SpellType::Spell:
#ifdef _DEBUG
		if (DebugGodMode)
			break;
#endif
		int ma = GetManaAmount(player, sn);
		player._pMana -= ma;
		player._pManaBase -= ma;
		oracool::OnPassiveManaSpent(player, ma);
		RedrawComponent(PanelDrawComponent::Mana);
		break;
	}
	if (sn == SpellID::BloodStar) {
		ApplyPlrDamage(DamageType::Physical, player, 5);
	}
	if (sn == SpellID::BoneSpirit) {
		ApplyPlrDamage(DamageType::Physical, player, 6);
	}
}

void EnsureValidReadiedSpell(Player &player)
{
	if (!IsReadiedSpellValid(player)) {
		ClearReadiedSpell(player);
	}
}

SpellCheckResult CheckSpell(const Player &player, SpellID sn, SpellType st, bool manaonly)
{
#ifdef _DEBUG
	if (DebugGodMode)
		return SpellCheckResult::Success;
#endif

	if (!manaonly && pcurs != CURSOR_HAND) {
		return SpellCheckResult::Fail_Busy;
	}

	if (st == SpellType::Skill) {
		// Oracool, Round 2 (2026-09-03): a TREE skill with a mana price pays it. Vanilla's skills
		// (Repair, Identify...) cost nothing and this branch was written for them; the fork's tree
		// rows are SpellType::Skill because they are earned rather than read from a book, and until
		// this line Ice Bolt was free. The Paladin's seven pay their own way in CheckPlrSpell, and are
		// left to it - see SkillPaysMana. The Barbarian's skills ask for Rage, not mana (oracool/rage.h).
		if (oracool::UsesRage(player))
			return oracool::CanPaySkill(player, sn) ? SpellCheckResult::Success : SpellCheckResult::Fail_NoMana;
		if (SkillPaysMana(sn) && player._pMana < GetManaAmount(player, sn))
			return SpellCheckResult::Fail_NoMana;
		return SpellCheckResult::Success;
	}

	if (player.GetSpellLevel(sn) <= 0) {
		return SpellCheckResult::Fail_Level0;
	}

	if (player._pMana < GetManaAmount(player, sn)) {
		return SpellCheckResult::Fail_NoMana;
	}

	return SpellCheckResult::Success;
}

void CastSpell(int id, SpellID spl, int sx, int sy, int dx, int dy, int spllvl)
{
	Player &player = Players[id];

	// Oracool: the Paladin's three cast skills (2026-09-11) arrive here at the cast frame like any spell
	// and go to their own module rather than to the missile table, where their rows hold
	// MissileID::Null. CastRangedPaladinSkill pays its own mana, so ConsumeSpell is not reached - it
	// would charge nothing for them anyway (SkillPaysMana).
	if (const std::optional<oracool::PaladinSkill> skill = oracool::PaladinSkillForSpell(spl); skill.has_value() && oracool::IsCastPaladinSkill(*skill)) {
		oracool::CastRangedPaladinSkill(player, *skill, { dx, dy });
		return;
	}

	Direction dir = player._pdir;
	if (IsWallSpell(spl)) {
		dir = player.tempDirection;
	}

	// Oracool: everything AddMissile makes between here and the end of this function belongs to this
	// skill. CastSpell is the right place rather than StartSpell, which only begins the animation -
	// the missiles are created later, when the animation reaches its action frame and lands here.
	oracool::BeginSkillCast(oracool::ClassTreeSkillForSpell(player._pClass, spl));

	bool fizzled = false;
	const SpellData &spellData = GetSpellData(spl);
	for (size_t i = 0; i < sizeof(spellData.sMissiles) / sizeof(spellData.sMissiles[0]) && spellData.sMissiles[i] != MissileID::Null; i++) {
		Missile *missile = AddMissile({ sx, sy }, { dx, dy }, dir, spellData.sMissiles[i], TARGET_MONSTERS, id, 0, spllvl);
		fizzled |= (missile == nullptr);
	}
	if (spl == SpellID::ChargedBolt) {
		for (int i = (spllvl / 2) + 3; i > 0; i--) {
			Missile *missile = AddMissile({ sx, sy }, { dx, dy }, dir, MissileID::ChargedBolt, TARGET_MONSTERS, id, 0, spllvl);
			fizzled |= (missile == nullptr);
		}
	}
	oracool::EndSkillCast();

	if (!fizzled) {
		ConsumeSpell(player, spl);
	}
}

void DoResurrect(size_t pnum, Player &target)
{
	if (pnum >= Players.size()) {
		return;
	}

	AddMissile(target.position.tile, target.position.tile, Direction::South, MissileID::ResurrectBeam, TARGET_MONSTERS, pnum, 0, 0);

	if (target._pHitPoints != 0)
		return;

	if (&target == MyPlayer) {
		MyPlayerIsDead = false;
		gamemenu_off();
		RedrawComponent(PanelDrawComponent::Health);
		RedrawComponent(PanelDrawComponent::Mana);
	}

	ClrPlrPath(target);
	target.destAction = ACTION_NONE;
	target._pInvincible = false;
	SyncInitPlrPos(target);

	int hp = 10 << 6;
	if (target._pMaxHPBase < (10 << 6)) {
		hp = target._pMaxHPBase;
	}
	SetPlayerHitPoints(target, hp);

	target._pHPBase = target._pHitPoints + (target._pMaxHPBase - target._pMaxHP); // CODEFIX: does the same stuff as SetPlayerHitPoints above, can be removed
	target._pMana = 0;
	target._pManaBase = target._pMana + (target._pMaxManaBase - target._pMaxMana);

	target._pmode = PM_STAND;

	CalcPlrInv(target, true);

	if (target.isOnActiveLevel()) {
		StartStand(target, target._pdir);
	}
}

void DoHealOther(const Player &caster, Player &target)
{
	if ((target._pHitPoints >> 6) <= 0) {
		return;
	}

	int hp = (GenerateRnd(10) + 1) << 6;
	for (int i = 0; i < caster._pLevel; i++) {
		hp += (GenerateRnd(4) + 1) << 6;
	}
	for (int i = 0; i < caster.GetSpellLevel(SpellID::HealOther); i++) {
		hp += (GenerateRnd(6) + 1) << 6;
	}

	if (caster._pClass == HeroClass::Warrior || caster._pClass == HeroClass::Barbarian) {
		hp *= 2;
	} else if (caster._pClass == HeroClass::Rogue || caster._pClass == HeroClass::Bard) {
		hp += hp / 2;
	} else if (caster._pClass == HeroClass::Monk) {
		hp *= 3;
	}

	target._pHitPoints = std::min(target._pHitPoints + hp, target._pMaxHP);
	target._pHPBase = std::min(target._pHPBase + hp, target._pMaxHPBase);

	if (&target == MyPlayer) {
		RedrawComponent(PanelDrawComponent::Health);
	}
}

int GetSpellBookLevel(SpellID s)
{
	if (gbIsSpawn) {
		switch (s) {
		case SpellID::StoneCurse:
		case SpellID::Guardian:
		case SpellID::Golem:
		case SpellID::Elemental:
		case SpellID::BloodStar:
		case SpellID::BoneSpirit:
			return -1;
		default:
			break;
		}
	}

	if (!gbIsHellfire) {
		switch (s) {
		case SpellID::Nova:
		case SpellID::Apocalypse:
			return -1;
		default:
			if (s > SpellID::LastDiablo)
				return -1;
			break;
		}
	}

	return GetSpellData(s).sBookLvl;
}

int GetSpellStaffLevel(SpellID s)
{
	if (gbIsSpawn) {
		switch (s) {
		case SpellID::StoneCurse:
		case SpellID::Guardian:
		case SpellID::Golem:
		case SpellID::Apocalypse:
		case SpellID::Elemental:
		case SpellID::BloodStar:
		case SpellID::BoneSpirit:
			return -1;
		default:
			break;
		}
	}

	if (!gbIsHellfire && s > SpellID::LastDiablo)
		return -1;

	return GetSpellData(s).sStaffLvl;
}

} // namespace devilution
