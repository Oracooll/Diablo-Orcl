/**
 * @file spells.cpp
 *
 * Implementation of functionality for casting player spells.
 */
#include "spells.h"
#include "oracool/class_tree.h"
#include "oracool/oracool.h" // IsBuiltInPortalAbility
#include "oracool/essence.h"
#include "oracool/paladin_ranged.h"
#include "oracool/paladin_skills.h"
#include "oracool/passives.h"
#include "oracool/rage.h"
#include "oracool/event_log.h"
#include "oracool/rift.h" // RiftForbidsTownPortal (plan r9)
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
#include "oracool/whirlwind.h" // RightButtonOnly

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
 * @brief A tree skill paid for through oracool/rage.h's facade rather than in mana: every skill of a Rage
 * user, and a Necromancer's Essence-priced rows (external audit of v1.12.188, SKL-01: the curses, the two
 * corpse explosions and Revive checked and paid mana here, so their Essence price was never asked).
 */
bool SkillPaysThroughFacade(const Player &player, SpellID spell)
{
	return oracool::UsesRage(player) || (oracool::EssenceCost(spell) > 0 && oracool::UsesEssence(player));
}

/**
 * @brief Gets a value indicating whether the player's current readied spell is a valid spell. Readied spells can be
 * invalidaded in a few scenarios where the spell comes from items, for example (like dropping the only scroll that
 * provided the spell).
 * @param player The player whose readied spell is to be checked.
 * @return 'true' when the readied spell is currently valid, and 'false' otherwise.
 */
bool IsReadiedPairValid(const Player &player, SpellID spell, SpellType type)
{
	switch (type) {
	case SpellType::Skill:
	case SpellType::Spell:
	case SpellType::Invalid:
		return true;

	case SpellType::Charges:
		return (player._pISpells & GetSpellBitmask(spell)) != 0;

	case SpellType::Scroll:
		return (player._pScrlSpells & GetSpellBitmask(spell)) != 0;

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

	if (gbIsHellfire && IsAnyOf(player._pClass, HeroClass::Sorcerer, HeroClass::Necromancer)) {
		ma /= 2;
	} else if (player._pClass == HeroClass::Rogue || player._pClass == HeroClass::Monk || player._pClass == HeroClass::Bard) {
		ma -= ma / 4;
	}

	if (GetSpellData(sn).sMinMana > ma >> 6) {
		ma = GetSpellData(sn).sMinMana << 6;
	}

	// Chant of Resonance (2026-09-14): the Monk's mantras, cheaper. After the floor, not before: under it the floor gave
	// the saving back - Commander of the Risen Dead took nothing off Raise Skeleton (round 13 audit, v1.12.238).
	ma += ma * oracool::PassiveManaCostPercent(player, sn) / 100;

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
		if (SkillPaysThroughFacade(player, sn)) {
			// A cast generator reaches here only when it struck (Backhand's cast fizzles on an empty
			// tile), so it landed one blow. A spender's cast is not counted as a blow. An Essence row
			// pays its Essence and nothing else.
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

bool HeroHasBinding(const Player &player, SpellID spell, SpellType type)
{
	if (!IsValidSpell(spell))
		return false;
	const SpellMask bit = GetSpellBitmask(spell);
	switch (type) {
	case SpellType::Skill:
		return (player._pAblSpells & bit) != 0;
	case SpellType::Spell:
		return (player._pMemSpells & bit) != 0;
	case SpellType::Scroll:
		return (player._pScrlSpells & bit) != 0;
	case SpellType::Charges:
		return (player._pISpells & bit) != 0;
	case SpellType::Invalid:
		break;
	}
	return false;
}

void EnsureValidReadiedSpell(Player &player)
{
	if (!IsReadiedPairValid(player, player._pRSpell, player._pRSplType)) {
		ClearReadiedSpell(player);
	}
	// The left button too (audit, 2026-09-27): only the right was checked, so a left-button scroll read to the last one,
	// or a staff put away, left a binding that silently refused every left click on an enemy - no swing either.
	if (!IsReadiedPairValid(player, player._pLRSpell, player._pLRSplType)) {
		player._pLRSpell = SpellID::Invalid;
		player._pLRSplType = SpellType::Invalid;
	}
	// A right-button-only skill never sits on the left (audit, 2026-09-29): a hero saved before v1.12.224 could carry
	// Leap, Rend or a cry there - drawn red on the well, and still cast by a left click - or on a left F-key that the
	// key then refuses.
	if (oracool::RightButtonOnly(player._pLRSpell)) {
		player._pLRSpell = SpellID::Invalid;
		player._pLRSplType = SpellType::Invalid;
	}
	for (size_t slot = 0; slot < NumHotkeys; slot++) {
		if (oracool::RightButtonOnly(player._pSplLHotKey[slot])) {
			player._pSplLHotKey[slot] = SpellID::Invalid;
			player._pSplLTHotKey[slot] = SpellType::Invalid;
		}
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

	// No town portal inside a Guardian Rift (plan r9): the only way out is the gate's own, after the
	// guardian. Refused before any cost, from spell, scroll or staff alike.
	if (sn == SpellID::TownPortal && oracool::RiftForbidsTownPortal()) {
		// Said on a cast, not on the wells' per-frame plate check (manaonly): it counted "(xN)" up every frame (round 23).
		if (&player == MyPlayer && !manaonly)
			oracool::LogEvent("No portal opens inside a Guardian Rift - only the guardian's fall does.", UiFlags::ColorRed);
		return SpellCheckResult::Fail_Level0;
	}

	if (st == SpellType::Skill) {
		// Oracool, Round 2 (2026-09-03): a TREE skill with a mana price pays it. Vanilla's skills
		// (Repair, Identify...) cost nothing and this branch was written for them; the fork's tree
		// rows are SpellType::Skill because they are earned rather than read from a book, and until
		// this line Ice Bolt was free. The Paladin's seven pay their own way in CheckPlrSpell, and are
		// left to it - see SkillPaysMana. The Barbarian's skills ask for Rage, not mana (oracool/rage.h).
		if (SkillPaysThroughFacade(player, sn))
			return oracool::CanPaySkill(player, sn) ? SpellCheckResult::Success : SpellCheckResult::Fail_NoMana;
		if (SkillPaysMana(sn) && player._pMana < GetManaAmount(player, sn))
			return SpellCheckResult::Fail_NoMana;
		return SpellCheckResult::Success;
	}

	// The built-in Portal is always there (oracool.h): -spell-level gear (Crackrust, Bovine Plate) took it to level 0, and
	// the Portal button and T did nothing, silently (round 24 audit, v1.12.249).
	if (player.GetSpellLevel(sn) <= 0 && !oracool::IsBuiltInPortalAbility(sn)) {
		return SpellCheckResult::Fail_Level0;
	}

	if (player._pMana < GetManaAmount(player, sn)) {
		return SpellCheckResult::Fail_NoMana;
	}

	return SpellCheckResult::Success;
}

int ChargedBoltCount(int spellLevel)
{
	return (spellLevel / 2) + 4;
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
		// The table's missile above is the first bolt; these are the rest.
		for (int i = ChargedBoltCount(spllvl) - 1; i > 0; i--) {
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
