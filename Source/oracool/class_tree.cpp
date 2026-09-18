#include "oracool/class_tree.h"

#include "oracool/class_skills.h" // RefreshInnateSpells - a point spent or refunded changes what the character HAS

#include <algorithm>
#include <array>
#include <cassert>
#include <optional>

#include <fmt/format.h>

#include "engine/backbuffer_state.hpp"
#include "inv.h"
#include "itemdat.h" // AllItemsList - Brace asks whether the weapon is a spear or pike
#include "oracool/auto_save.h"
#include "oracool/aura_field.h"
#include "oracool/event_log.h"
#include "oracool/paladin_skills.h"
#include "oracool/passives.h"
#include "oracool/curses.h"
#include "oracool/essence.h"
#include "oracool/rage.h"
#include "oracool/rfa12_effects.h"
#include "oracool/skill_facts.h"
#include "oracool/skill_points.h"
#include "oracool/skill_sounds.h"
#include "oracool/spell_ranks.h" // the Rule of Rangs
#include "oracool/stat_sheet.h"
#include "oracool/ui_sound.h"
#include "oracool/warcries.h"
#include "missiles.h" // GetDamageAmtAtLevel - an active's rank is its spell level
#include "spells.h"   // GetManaAmountAtLevel
#include "player.h"
#include "plrmsg.h" // EventPlrMsg - a slot change refused for want of backpack room says so
#include "utils/language.h"

namespace devilution::oracool {

namespace {

using Skill = ClassTreeSkill;
using Kind = ClassTreeKind;

/**
 * @brief The aura whose loop is currently PLAYING, or None.
 *
 * One owner for the audio, at file scope, because two of them is what went wrong. Audit
 * finding, 2026-08-26 - and this one was mine, introduced with the death guard at v1.9.50.
 *
 * ToggleClassAura started the loop directly, and ProcessClassTreeTick kept its own separate
 * static of what it thought was playing. So the tick saw the same transition a second time
 * and started the loop AGAIN - and StartClassAuraLoop stops whatever is running first, so
 * lighting an aura produced start, then stop-and-start a tick later. An audible stutter on
 * every activation.
 *
 * Now every path goes through SetAuraLoop, which is idempotent against this variable, so the
 * tick can re-assert the correct state as often as it likes and only a real CHANGE is heard.
 */
Skill LoopedAura = Skill::None;

/** @brief Makes the aura loop match @p skill. Does nothing if it already does. */
void SetAuraLoop(Skill skill)
{
	if (skill == LoopedAura)
		return;
	if (skill == Skill::None)
		StopClassAuraLoop();
	else
		StartClassAuraLoop(skill);
	LoopedAura = skill;
}

} // namespace (reopened below)

void SilenceAuraLoopForTransition()
{
	// Audit finding, 2026-08-26. Level loading and FreeGame called SilenceClassAuraLoop and
	// ResumeClassAuraLoop straight through to the sound layer, which left LoopedAura describing a
	// loop that was no longer playing - and SetAuraLoop believes LoopedAura.
	//
	// Both directions of that lie are audible. Silence without clearing the tracker means the next
	// tick sees "already playing" and never re-starts the loop, so an aura that is lit goes quiet
	// for good. Resume without setting it means the next tick sees a change that did not happen,
	// stops the loop it just resumed and plays the transition cues over the top of a level load.
	SilenceClassAuraLoop();
	LoopedAura = Skill::None;
}

void ResumeAuraLoopAfterTransition(Skill skill)
{
	if (skill == Skill::None) {
		SilenceAuraLoopForTransition();
		return;
	}
	ResumeClassAuraLoop(skill);
	// The whole point: the tracker now agrees with the audio, so the tick that follows sees no
	// change and stays quiet.
	LoopedAura = skill;
}

namespace {

constexpr HeroClass Pal = HeroClass::Warrior; // Oracool displays the Warrior as "Paladin"
constexpr HeroClass Bar = HeroClass::Barbarian;
constexpr HeroClass Sor = HeroClass::Sorcerer;
constexpr HeroClass Rog = HeroClass::Rogue;
constexpr HeroClass Bard = HeroClass::Bard;
constexpr HeroClass Monk = HeroClass::Monk;
constexpr HeroClass Nec = HeroClass::Necromancer;

// Diablo II's own tier requirements, plus a seventh the Monk's design doc adds (its branches are
// seven sequential skills, one per tier). The first six are unchanged, so no existing skill moved.
constexpr int TierLevels[] = { 1, 6, 12, 18, 24, 30, 36 };

/**
 * @brief The four trees, in icon-strip order within each class.
 *
 * Descriptions state D2's effect first, then this engine's adaptation where the two differ. The
 * `implemented` flag is the honest one: false means the row is listed and described but does
 * nothing, which the UI shows rather than hides.
 *
 * Five Paladin actives carry SpellID::Invalid here and get their real slot from
 * oracool/paladin_skills.h at call time - see ClassTreeSpellId for why that is not stored.
 */
const ClassTreeSkillData Skills[ClassTreeSkillCount] = {
	// ======================= PALADIN =======================
	// --- Combat Skills ---
	{ N_("Sacrifice"), N_("A blow at +150% damage, +20% per rank, that costs you 8% of the damage it dealt in life. It cannot take your last point of life."),
	    Pal, 0, 0, 0, Kind::Active, SpellID::Sacrifice, true },
	{ N_("Smite"), N_("Bash with your shield: it always connects, lands +15% damage per level, and stuns - though not uniques or bosses. A shield is mandatory."),
	    Pal, 0, 0, 1, Kind::Active, SpellID::ShieldBash, true },
	// Holy Bolt stood here at column 2 until 2026-09-06 (user: "remove paladin Holy Bolt skill. There is a
	// spell like this already in the game" - the book's). Its SpellID::HolyBoltSkill row in spelldat stays, unreferenced.
	// Matches paladin_skills.cpp word for word on the numbers, deliberately: two windows describing
	// one skill differently is worse than either being wrong alone, and this row has drifted twice.
	{ N_("Zeal"), N_("Strike several times in one furious burst. Skill levels 1, 3 and 5 each add a strike, and every skill level adds +1% chance to hit."),
	    Pal, 0, 1, 0, Kind::Active, SpellID::Zeal, true },
	{ N_("Charge"), N_("Rush an enemy and land a running blow, +20% damage per level."),
	    Pal, 0, 1, 1, Kind::Active, SpellID::Charge, true },
	{ N_("Vengeance"), N_("Your blows burn and crackle for 30 seconds, +5 per rank: fire and lightning on every hit. Cold has no place on the weapon sheet, so it is not added."),
	    Pal, 0, 2, 0, Kind::Active, SpellID::Vengeance, true },
	{ N_("Blessed Hammer"), N_("Looses a spinning hammer that wheels outward through anything in its path."),
	    Pal, 0, 3, 1, Kind::Active, SpellID::BlessedHammer, true },
	// Corrected 2026-08-16: this row used to claim "no charmed-monster state exists", which was
	// simply wrong - this engine's Berserk sets MFLAG_GOLEM on the target, making it fight for the
	// player, which IS conversion. Found while wiring the Bard's Charm onto the same spell.
	{ N_("Conversion"), N_("Turns one enemy near the cursor to your side for 20 seconds, +2 per rank. Uniques and the magic-immune refuse."),
	    Pal, 0, 4, 0, Kind::Active, SpellID::Conversion, true },
	{ N_("Fist of the Heavens"), N_("Calls down a bolt from the sky, which bursts into holy energy where it lands."),
	    Pal, 0, 5, 0, Kind::Active, SpellID::FistOfTheHeavens, true },
	// --- Offensive Auras ---
	{ N_("Might"), N_("Increases the damage you deal."), Pal, 1, 0, 0, Kind::Aura, SpellID::Invalid, true },
	{ N_("Holy Fire"), N_("Every 3 seconds holy fire strikes everything around you. Damage and reach grow every level; the reach stops at 10 tiles."),
	    Pal, 1, 1, 0, Kind::Aura, SpellID::Invalid, true },
	{ N_("Thorns"), N_("Returns 25% of the melee damage you take to whatever struck you, +10% per level."),
	    Pal, 1, 1, 1, Kind::Aura, SpellID::Invalid, true },
	{ N_("Blessed Aim"), N_("Steadies your hand, raising your chance to hit."),
	    Pal, 1, 2, 0, Kind::Aura, SpellID::Invalid, true },
	{ N_("Concentration"), N_("Raises damage and steadies you against interruption."),
	    Pal, 1, 3, 0, Kind::Aura, SpellID::Invalid, true },
	{ N_("Holy Freeze"), N_("Every 3 seconds holy cold strikes and chills everything around you. Damage and reach grow every level; the reach stops at 10 tiles."),
	    Pal, 1, 3, 1, Kind::Aura, SpellID::Invalid, true },
	{ N_("Holy Shock"), N_("Every 3 seconds holy lightning strikes everything around you. Damage and reach grow every level; the reach stops at 10 tiles."),
	    Pal, 1, 4, 0, Kind::Aura, SpellID::Invalid, true },
	{ N_("Sanctuary"), N_("Hallows the ground you stand on: nearby undead break and flee, and burn for magic damage every second. Champions are too proud to run, but they burn."),
	    Pal, 1, 4, 1, Kind::Aura, SpellID::Invalid, true },
	{ N_("Fanaticism"), N_("Drives you to strike faster, harder and truer."),
	    Pal, 1, 5, 0, Kind::Aura, SpellID::Invalid, true },
	{ N_("Conviction"), N_("Strips the resistances of every enemy near you and lowers their armour 3% per level, to 60%. At five points it begins to break immunities down into mere resistances."),
	    Pal, 1, 5, 1, Kind::Aura, SpellID::Invalid, true },
	// --- Defensive Auras ---
	{ N_("Prayer"), N_("Mends your wounds steadily as you walk."), Pal, 2, 0, 0, Kind::Aura, SpellID::Invalid, true },
	{ N_("Resist Fire"), N_("Hardens you against fire."), Pal, 2, 0, 1, Kind::Aura, SpellID::Invalid, true },
	{ N_("Defiance"), N_("Raises your armour class."), Pal, 2, 1, 0, Kind::Aura, SpellID::Invalid, true },
	{ N_("Resist Cold"), N_("Hardens you against cold. No cold exists here, so it wards against magic instead."),
	    Pal, 2, 1, 1, Kind::Aura, SpellID::Invalid, true },
	{ N_("Cleansing"), N_("Slows and chills on you wear off 20% sooner, +5% per level."),
	    Pal, 2, 2, 1, Kind::Aura, SpellID::Invalid, true },
	{ N_("Resist Lightning"), N_("Hardens you against lightning."), Pal, 2, 2, 0, Kind::Aura, SpellID::Invalid, true },
	{ N_("Vigor"), N_("Quickens your stride: +5% movement speed per level, anywhere, and every level counts. Items with +movement speed stack with it."),
	    Pal, 2, 3, 0, Kind::Aura, SpellID::Invalid, true },
	{ N_("Meditation"), N_("Restores your mana steadily as you walk."), Pal, 2, 4, 0, Kind::Aura, SpellID::Invalid, true },
	{ N_("Redemption"), N_("Once a second the nearest corpse in the field is consumed for a fiftieth of your life and mana, a hundredth more a point."),
	    Pal, 2, 5, 0, Kind::Aura, SpellID::Invalid, true },
	{ N_("Salvation"), N_("Wards you against fire, lightning and magic alike."),
	    Pal, 2, 5, 1, Kind::Aura, SpellID::Invalid, true },
	// --- Combat Skills, appended out of page order (2026-08-16) ---
	//
	// These two are this fork's own, with no Diablo II row to sit in, and until now they had no tree
	// home at all - they were reachable only from the Skills sheet, which the user has since removed.
	// They are page 0 and draw among the other Combat Skills; only their position in THIS table is
	// unusual, and it has to be, because a Paladin skill's index here is its icon frame and its save
	// slot. See the note beside HammerOfFaith in class_tree.h.
	//
	// Tiers chosen as the smallest move from each skill's existing level: Hammer of Faith was 10 and
	// tier 2 is 12; Blessed Shield was 20 and tier 3 is 18. paladin_skills.cpp now carries those two
	// numbers, and IsClassTreeSkillUnlocked defers to it, so tier and skill agree.
	//
	// Columns fill the gaps their tiers had: Vengeance holds (2,0) and Blessed Hammer (3,2).
	{ N_("Hammer of Faith"), N_("A heavy swing whose force splashes over everything around your target."),
	    Pal, 0, 2, 1, Kind::Active, SpellID::HammerOfFaith, true },
	{ N_("Blessed Shield"), N_("Hurls your shield at a monster. It bounces to the nearest monster, then to a third, striking each for less. A shield is mandatory."),
	    Pal, 0, 3, 0, Kind::Active, SpellID::BlessedShield, true },

	// ---- Passive Skills (page 3) ----
	// Built 2026-09-11 (user: "build Heavenly Strength passive skill"): the Barbarian's own grip, widened to
	// every two-handed weapon - see HeavenlyStrengthGrips - so the other hand is free for a shield.
	{ N_("Heavenly Strength"), N_("Bear a two-handed axe, sword, mace or staff in one hand and a shield in the other."),
	    Pal, 3, 0, 0, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Fervor"), N_("With a one-handed weapon in hand you swing faster."),
	    Pal, 3, 0, 1, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Vigilant"), N_("Every blow that is not steel - fire, lightning, magic - deals -20% damage to you."),
	    Pal, 3, 0, 2, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Righteousness"), N_("You hold 20 more mana, and every blow you land in melee restores 1."),
	    Pal, 3, 1, 0, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Insurmountable"), N_("Every blow you block restores 5 mana."),
	    Pal, 3, 1, 1, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Fanaticism"), N_("Your simplest attacks swing faster."),
	    Pal, 3, 1, 2, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Indestructible"), N_("Once a minute a killing blow leaves you standing at 33% of your life instead."),
	    Pal, 3, 2, 0, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Holy Cause"), N_("+10% weapon damage."),
	    Pal, 3, 2, 1, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Wrathful"), N_("30% of every point of mana you spend returns as life."),
	    Pal, 3, 2, 2, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Divine Fortress"), N_("Behind a shield your armour is +25%."),
	    Pal, 3, 3, 0, Kind::Passive, SpellID::Invalid, true, 1 },
	// Replaced 2026-09-14 (user note: "Invent a new passive skill which works with current engine capabilities") -
	// Lord Commander asked for a mount, a bombardment and a phalanx this engine does not have.
	{ N_("Crusader's Stride"), N_("While an aura burns, you move 15% faster."),
	    Pal, 3, 3, 1, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Hold Your Ground"), N_("+20% chance to block."),
	    Pal, 3, 3, 2, Kind::Passive, SpellID::Invalid, true, 1 },
	// Replaced 2026-09-14 (user note) - Long Arm of the Law lengthened aura durations, and auras here have none.
	{ N_("Sanctified"), N_("+20% damage against the undead and demons."),
	    Pal, 3, 4, 0, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Iron Maiden"), N_("What strikes you in melee takes 50% of the blow back, on top of Thorns."),
	    Pal, 3, 4, 1, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Renewal"), N_("Every blow you block heals 3% of your life."),
	    Pal, 3, 4, 2, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Finery"), N_("+2 Strength for every gem set into the gear you wear."),
	    Pal, 3, 5, 0, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Blunt"), N_("Blessed Hammer strikes 25% harder."),
	    Pal, 3, 5, 1, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Towering Shield"), N_("Smite and Blessed Shield strike 25% harder."),
	    Pal, 3, 5, 2, Kind::Passive, SpellID::Invalid, true, 1 },
	// ---- RfA-12 skills (2026-09-13): the empty cells of the three class pages, from the final list ----
	{ N_("Votive Strike"), N_("A blow at +30% damage, +8% per level. An enemy it kills leaves no corpse to raise."),
	    Pal, 0, 0, 2, Kind::Active, SpellID::VotiveStrike, true },
	{ N_("Judgment"), N_("A blow at +10% damage, +3% per level, that marks the target for 4 seconds: it takes +15% damage from everything, +1% per level, to 40%."),
	    Pal, 0, 1, 2, Kind::Active, SpellID::Judgment, true },
	{ N_("Oathbrand"), N_("A blow at +10% damage, +3% per level, that brands the target for 6 seconds: your next three blows on it each add 4-8 magic damage, +2-3 per level."),
	    Pal, 0, 2, 2, Kind::Active, SpellID::Oathbrand, true },
	{ N_("Holy Lance"), N_("A thrust that also strikes the two tiles beyond your target with light, for 80% of a blow, +5% per level."),
	    Pal, 0, 3, 2, Kind::Active, SpellID::HolyLance, true },
	{ N_("Crusade"), N_("A blow that also strikes up to three other enemies beside you, each for 75% of a blow, +5% per level."),
	    Pal, 0, 4, 1, Kind::Active, SpellID::Crusade, true },
	{ N_("Aegis Slam"), N_("A shield slam across the three tiles ahead: everything there is knocked back and stunned for 1 second, and the two beside the target take 60% of a blow, +5% per level. Needs a shield."),
	    Pal, 0, 4, 2, Kind::Active, SpellID::AegisSlam, true },
	{ N_("Heaven's Descent"), N_("Leap up to 6 tiles to the cursor and land in a holy explosion: 8-16 magic damage to everything beside you, +4-6 per level."),
	    Pal, 0, 5, 1, Kind::Active, SpellID::HeavensDescent, true },
	{ N_("Wrath of the Heavens"), N_("Five pillars of light fall over 3 seconds, each on an enemy within 5 tiles: 10-20 magic damage, +4-7 per level."),
	    Pal, 0, 5, 2, Kind::Active, SpellID::WrathOfTheHeavens, true },
	{ N_("Valor"), N_("Adds 3 damage to every blow, +2 per level."),
	    Pal, 1, 0, 1, Kind::Aura, SpellID::Invalid, true },
	{ N_("Radiance"), N_("Every 2 seconds, undead in reach take 3-6 magic damage, +1-2 per level."),
	    Pal, 1, 0, 2, Kind::Aura, SpellID::Invalid, true },
	{ N_("Bane of Evil"), N_("+25% damage against demons and undead, +5% per level, to 150%."),
	    Pal, 1, 1, 2, Kind::Aura, SpellID::Invalid, true },
	{ N_("Condemnation"), N_("Enemies in reach lose 10% of their armour, +2% per level, to 50%."),
	    Pal, 1, 2, 1, Kind::Aura, SpellID::Invalid, true },
	{ N_("Tithe of Ash"), N_("A kill in reach restores 2 mana, +1 per level, and consumes the corpse."),
	    Pal, 1, 2, 2, Kind::Aura, SpellID::Invalid, true },
	{ N_("Retaliation"), N_("Each blow you take adds +10% damage to your next blow, +3% per level, stacking three times."),
	    Pal, 1, 3, 2, Kind::Aura, SpellID::Invalid, true },
	{ N_("Doom Procession"), N_("After 2 seconds on the move, each tile you leave burns for 3 seconds; the first enemy to step on it takes 4-8 magic damage, +2-3 per level."),
	    Pal, 1, 4, 2, Kind::Aura, SpellID::Invalid, true },
	{ N_("Dominion"), N_("Enemies in reach deal 10% less damage and take 10% more, +1% per level, to 35%."),
	    Pal, 1, 5, 2, Kind::Aura, SpellID::Invalid, true },
	{ N_("Steadfast"), N_("Blows barely interrupt you: faster hit recovery, and the fastest from level 10."),
	    Pal, 2, 0, 2, Kind::Aura, SpellID::Invalid, true },
	{ N_("Resist Magic"), N_("Hardens you against magic: +15% magic resistance, +4% per level."),
	    Pal, 2, 1, 2, Kind::Aura, SpellID::Invalid, true },
	{ N_("Immovable"), N_("Knockback cannot move you while this aura burns; the damage still lands."),
	    Pal, 2, 2, 2, Kind::Aura, SpellID::Invalid, true },
	{ N_("Warding Light"), N_("Every blow you take deals 2 less damage, +1 per level."),
	    Pal, 2, 3, 1, Kind::Aura, SpellID::Invalid, true },
	{ N_("Mercy"), N_("Falling below 30% life heals 20% of your life, +1% per level to 50%, once every 20 seconds."),
	    Pal, 2, 3, 2, Kind::Aura, SpellID::Invalid, true },
	{ N_("Aura of Protection"), N_("+20% armour, +6% per level."),
	    Pal, 2, 4, 1, Kind::Aura, SpellID::Invalid, true },
	{ N_("Endurance"), N_("+10% maximum life, +2% per level, to 60%."),
	    Pal, 2, 4, 2, Kind::Aura, SpellID::Invalid, true },
	{ N_("Sanctity"), N_("+8% to every resistance, +2% per level, and slows on you wear off 50% sooner."),
	    Pal, 2, 5, 2, Kind::Aura, SpellID::Invalid, true },
	// ======================= BARBARIAN =======================
	// --- Combat Skills ---
	{ N_("Bash"), N_("A heavy blow at +30% damage, +10% per rank, that knocks the target back."), Bar, 0, 0, 0, Kind::Active, SpellID::Bash, true },
	{ N_("Leap"), N_("Vault to the spot under the cursor, over anything in the way - four tiles, a tile further every three ranks."), Bar, 0, 1, 0, Kind::Active, SpellID::Leap, true },
	{ N_("Double Swing"), N_("Two blows in one swing, the second at 75% damage, +5% per rank."), Bar, 0, 1, 1, Kind::Active, SpellID::DoubleSwing, true },
	{ N_("Stun"), N_("A blow that leaves the target reeling for 1.5 seconds, +20% longer per rank. Uniques shrug it off."), Bar, 0, 2, 0, Kind::Active, SpellID::Stun, true },
	// User note, 2026-09-14: "We remove Double Throw ... but we need to keep single weapon throw ... using normal attack animation."
	{ N_("Weapon Throw"), N_("Hurl the sword or axe in your hand at the enemy under the cursor for its full damage, with your ordinary attack; it is back in your grip for the next blow."), Bar, 0, 2, 1, Kind::Active, SpellID::WeaponThrow, true },
	{ N_("Leap Attack"), N_("Leap onto a distant enemy; the blow you land there is at +50% damage, +10% per rank."), Bar, 0, 3, 0, Kind::Active, SpellID::LeapAttack, true },
	{ N_("Concentrate"), N_("A focused blow at +50% damage, +10% per rank. The steadiness half is not built yet."), Bar, 0, 3, 1, Kind::Active, SpellID::Concentrate, true },
	{ N_("Frenzy"), N_("Two blows in one swing, both at 100% damage, +10% per rank."), Bar, 0, 4, 0, Kind::Active, SpellID::Frenzy, true },
	{ N_("Whirlwind"), N_("Every swing strikes everything around you at 66% damage, +5% per rank. You stand your ground rather than travelling."), Bar, 0, 5, 0, Kind::Active, SpellID::Whirlwind, true },
	{ N_("Berserk"), N_("A blow at +100% damage, +20% per rank. The defence you would trade for it is not taken yet."), Bar, 0, 5, 1, Kind::Active, SpellID::BerserkBlow, true },
	// --- Combat Masteries ---
	{ N_("Sword Mastery"), N_("Sharpens your aim and your blow with any sword held."), Bar, 1, 0, 0, Kind::Passive, SpellID::Invalid, true },
	{ N_("Axe Mastery"), N_("Sharpens your aim and your blow with any axe held."), Bar, 1, 0, 1, Kind::Passive, SpellID::Invalid, true },
	{ N_("Mace Mastery"), N_("Sharpens your aim and your blow with any mace or club held."), Bar, 1, 0, 2, Kind::Passive, SpellID::Invalid, true },
	{ N_("Pole Arm Mastery"), N_("Sharpens your aim and your blow with a staff - this engine's nearest pole arm."),
	    Bar, 1, 1, 0, Kind::Passive, SpellID::Invalid, true },
	{ N_("Throwing Mastery"), N_("Mastery of the thrown weapon: Weapon Throw lands +10% damage, +6% per level."), Bar, 1, 1, 1, Kind::Passive, SpellID::Invalid, true },
	// Built 2026-09-14: the engine has no spear ItemType, but the Spear and Pike BASES have been told apart
	// since RfA-12's Brace and Long Reach (WieldingSpearOrPike), so the old "no spear type" reason no longer held.
	{ N_("Spear Mastery"), N_("Mastery of spears and pikes: +10% chance to hit, +5% per level, and +10% damage, +6% per level, while one is held."), Bar, 1, 1, 2, Kind::Passive, SpellID::Invalid, true },
	// Replaced 2026-09-14 (user note) - Increased Stamina lengthened a wind this engine does not track.
	{ N_("Toughness"), N_("+5 Vitality, +2 per level."), Bar, 1, 2, 0, Kind::Passive, SpellID::Invalid, true },
	{ N_("Iron Skin"), N_("Toughens your hide, raising armour class."), Bar, 1, 3, 0, Kind::Passive, SpellID::Invalid, true },
	{ N_("Increased Speed"), N_("You run rather than walk, wherever you are."), Bar, 1, 4, 0, Kind::Passive, SpellID::Invalid, true },
	{ N_("Natural Resistance"), N_("Hardens you against fire, lightning and magic alike."), Bar, 1, 5, 0, Kind::Passive, SpellID::Invalid, true },
	// --- Warcries ---
	{ N_("Howl"), N_("A howl that sends everything in earshot running, four tiles and a tile more a rank. Uniques hold their ground."), Bar, 2, 0, 0, Kind::Active, SpellID::Howl, true },
	{ N_("Find Potion"), N_("Search a corpse near the cursor. 50% of the time, +5% per rank, it yields a potion - rarely a full one. The corpse is used up."), Bar, 2, 0, 1, Kind::Active, SpellID::FindPotion, true },
	{ N_("Taunt"), N_("A goad that wakes everything in earshot and turns it on you."), Bar, 2, 1, 0, Kind::Active, SpellID::Taunt, true },
	{ N_("Shout"), N_("A bellow that hardens you: +50% armour, +10% per rank, for 40 seconds, +5 per rank."), Bar, 2, 1, 1, Kind::Active, SpellID::Shout, true },
	{ N_("Find Item"), N_("Search a corpse near the cursor. 25% of the time, +5% per rank, it yields an item. The corpse is used up."), Bar, 2, 2, 0, Kind::Active, SpellID::FindItem, true },
	{ N_("Battle Cry"), N_("A cry that leaves what hears it at -25% damage and -25% armour for 24 seconds."), Bar, 2, 3, 0, Kind::Active, SpellID::BattleCry, true },
	{ N_("Battle Orders"), N_("A shout that swells your life by +20, +10 per rank, for 40 seconds, +5 per rank."), Bar, 2, 4, 0, Kind::Active, SpellID::BattleOrders, true },
	{ N_("Grim Ward"), N_("Raise a corpse near the cursor as a totem of terror: for 20 seconds, +2 per rank, everything but the uniques that comes near it runs."), Bar, 2, 4, 1, Kind::Active, SpellID::GrimWard, true },
	{ N_("War Cry"), N_("A shout that strikes everything in earshot for four to eight a rank and leaves it reeling for two seconds. Uniques shrug off the reeling."), Bar, 2, 5, 0, Kind::Active, SpellID::WarCry, true },
	{ N_("Battle Command"), N_("A command that deepens every skill you have by a rank for 30 seconds, +5 per rank."), Bar, 2, 5, 1, Kind::Active, SpellID::BattleCommand, true },

	// ---- Passive Skills (page 3) ----
	{ N_("Pound of Flesh"), N_("Every kill heals 3% of your life."),
	    Bar, 3, 0, 0, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Ruthless"), N_("+40% damage against anything below 33% of its life."),
	    Bar, 3, 0, 1, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Nerves of Steel"), N_("Once a minute a killing blow leaves you standing at 33% of your life instead."),
	    Bar, 3, 0, 2, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Weapons Master"), N_("Your weapon lends its gift: a sword +15% damage, an axe +15% chance to hit, a staff a faster attack, a mace 1 Rage for every blow that lands."),
	    Bar, 3, 1, 0, Kind::Passive, SpellID::Invalid, true, 1 },
	// Swapped cells with Unforgiving (user, 2026-09-14: "move unforgiving passive to lvl10 slot"): its
	// row stays here, its cell is Unforgiving's old one at level 30.
	{ N_("Inspiring Presence"), N_("Your warcries' blessings last twice as long, and while one is on you, 1% of your life returns every second."),
	    Bar, 3, 4, 2, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Berserker Rage"), N_("+25% damage while your Rage is at half or more."),
	    Bar, 3, 1, 2, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Bloodthirst"), N_("50% of every point of Rage you spend returns as life."),
	    Bar, 3, 2, 0, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Animosity"), N_("You hold twenty more Rage."),
	    Bar, 3, 2, 1, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Superstition"), N_("+10 to fire, lightning and magic resistance."),
	    Bar, 3, 2, 2, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Tough as Nails"), N_("+25% armour."),
	    Bar, 3, 3, 0, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("No Escape"), N_("+25% damage to enemies 5 or more tiles away."),
	    Bar, 3, 3, 1, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Relentless"), N_("Below 33% of your life, every blow deals -25% damage to you."),
	    Bar, 3, 3, 2, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Brawler"), N_("With three or more enemies pressing close, +20% damage to everything you do."),
	    Bar, 3, 4, 0, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Juggernaut"), N_("Slows hold you half as long and half the blows that would stagger you do not. A blow that does has a 30% chance to heal 20% of your life, once every 10 seconds."),
	    Bar, 3, 4, 1, Kind::Passive, SpellID::Invalid, true, 1 },
	// The level-10 cell since 2026-09-14 (user: "move unforgiving passive to lvl10 slot and develop it").
	// Built in oracool/rage.cpp, ProcessRageTick.
	{ N_("Unforgiving"), N_("Your Rage no longer drains when the fighting stops - it rises, 2 a second once the last battle's 5 seconds of fury have passed."),
	    Bar, 3, 1, 1, Kind::Passive, SpellID::Invalid, true, 1 },
	// Off the page (user, 2026-09-12: "i dont want 19th (lvl 36) skill"). A full 3x6 page holds 18, so
	// this unbuilt passive left it and the three after it moved up a cell - Rampage into the last one, at
	// level 36. Kept in the table: a row's position is its identity (icon strip, saves). See
	// RetiredFromTreePage.
	{ N_("Boon of Bul-Kathos"), N_("Your earthquake, your ancients and your berserking all return far sooner. Not yet built."),
	    Bar, RetiredFromTreePage, 5, 0, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Earthen Might"), N_("Ground Stomp, Seismic Slam and Earthquake give 3 Rage for every enemy they strike."),
	    Bar, 3, 5, 0, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Sword and Board"), N_("Behind a shield you take -30% damage."),
	    Bar, 3, 5, 1, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Rampage"), N_("Every kill lends +5% damage for 5 seconds, stacking five high."),
	    Bar, 3, 5, 2, Kind::Passive, SpellID::Invalid, true, 1 },
	// ---- RfA-12 skills (2026-09-13): the empty cells of the three class pages, from the final list ----
	{ N_("Cleave"), N_("A swing that also strikes the enemies beside you, each for 70% of a blow, +5% per level."),
	    Bar, 0, 0, 1, Kind::Active, SpellID::Cleave, true },
	{ N_("Backhand"), N_("Strike the enemy behind you without turning, for 100% of a blow, +8% per level."),
	    Bar, 0, 0, 2, Kind::Active, SpellID::Backhand, true },
	{ N_("Ground Stomp"), N_("Stuns everything beside you for 1.5 seconds, +0.2 per level. Uniques shrug it off."),
	    Bar, 0, 1, 2, Kind::Active, SpellID::GroundStomp, true },
	{ N_("Rend"), N_("A tearing blow at +20% damage, +5% per level, that makes the target bleed for 4 seconds: 3 damage a second, +2 per level."),
	    Bar, 0, 2, 2, Kind::Active, SpellID::Rend, true },
	{ N_("Hammer of the Ancients"), N_("One huge blow at +150% damage, +15% per level."),
	    Bar, 0, 3, 2, Kind::Active, SpellID::HammerOfTheAncients, true },
	{ N_("Seismic Slam"), N_("A shockwave rolls 5 tiles toward the cursor, striking everything on its path for 80% of a blow, +10% per level."),
	    Bar, 0, 4, 1, Kind::Active, SpellID::SeismicSlam, true },
	{ N_("Clasp of Ruin"), N_("A seizing blow at +20% damage, +5% per level, that holds an ordinary enemy in place for 1 second."),
	    Bar, 0, 4, 2, Kind::Active, SpellID::ClaspOfRuin, true },
	{ N_("Earthquake"), N_("The ground shakes for 4 seconds: every second, everything within 3 tiles takes 30% of a blow, +5% per level."),
	    Bar, 0, 5, 2, Kind::Active, SpellID::Earthquake, true },
	{ N_("Grip of Iron"), N_("With only one enemy beside you, its blows cannot interrupt your swing."),
	    Bar, 1, 2, 1, Kind::Passive, SpellID::Invalid, true },
	{ N_("Deep Wounds"), N_("Melee blows have a 10% chance, +2% per level to 50%, to make the target bleed for 3 seconds: 3 damage a second, +1 per level."),
	    Bar, 1, 2, 2, Kind::Passive, SpellID::Invalid, true },
	{ N_("Heavy Foot"), N_("Knockback cannot move you while you wield a two-handed weapon."),
	    Bar, 1, 3, 1, Kind::Passive, SpellID::Invalid, true },
	{ N_("Battle Hardened"), N_("Below half life, fire, lightning and magic deal 10% less damage to you, +2% per level to 40%."),
	    Bar, 1, 3, 2, Kind::Passive, SpellID::Invalid, true },
	{ N_("Bloodlust"), N_("Melee blows return 2% of their damage as life, +1% every 5 levels."),
	    Bar, 1, 4, 1, Kind::Passive, SpellID::Invalid, true },
	{ N_("Long Reach"), N_("With a staff, spear or pike, a swing at an empty tile strikes the enemy standing beyond it."),
	    Bar, 1, 4, 2, Kind::Passive, SpellID::Invalid, true },
	{ N_("Unfinished Business"), N_("Killing an enemy that struck you in the last 5 seconds heals 3% of your life, +1% every 3 levels."),
	    Bar, 1, 5, 1, Kind::Passive, SpellID::Invalid, true },
	{ N_("Lasting Wounds"), N_("Enemies you strike in melee cannot regenerate life for 4 seconds."),
	    Bar, 1, 5, 2, Kind::Passive, SpellID::Invalid, true },
	{ N_("Threatening Shout"), N_("Enemies in earshot deal 15% less damage, +1% per level to 40%, for 10 seconds, +1 per level."),
	    Bar, 2, 0, 2, Kind::Active, SpellID::ThreateningShout, true },
	{ N_("Rallying Cry"), N_("Heals 20% of your life over 5 seconds, +2% per level to 50%."),
	    Bar, 2, 1, 2, Kind::Active, SpellID::RallyingCry, true },
	{ N_("Intimidate"), N_("Enemies in earshot lose 15% of their armour, +2% per level to 60%, for 10 seconds, +1 per level."),
	    Bar, 2, 2, 1, Kind::Active, SpellID::Intimidate, true },
	{ N_("Split Ranks"), N_("A shout that shoves the enemies ahead of you within 3 tiles aside, opening a lane."),
	    Bar, 2, 2, 2, Kind::Active, SpellID::SplitRanks, true },
	{ N_("Iron Will"), N_("+15% to every resistance, +3% per level, for 20 seconds, +2 per level."),
	    Bar, 2, 3, 1, Kind::Active, SpellID::IronWill, true },
	{ N_("Bloodcall"), N_("For 10 seconds, every kill restores 3 life and 2 Rage, +1 each per level."),
	    Bar, 2, 3, 2, Kind::Active, SpellID::Bloodcall, true },
	{ N_("Ancestral Call"), N_("Calls the three Ancients for 20 seconds, +2 per level: Korlic leaps into the fray, Talic whirls, and Madawc hurls his hammer. Each strikes for 35% of your damage, +3% per level."),
	    Bar, 2, 4, 2, Kind::Active, SpellID::AncestralCall, true },
	{ N_("Earthshaker Cry"), N_("A roar that strikes everything within 8 tiles for 5-10 magic damage, +3-5 per level, and stuns it for 2 seconds. Uniques shrug off the stun."),
	    Bar, 2, 5, 2, Kind::Active, SpellID::EarthshakerCry, true },
	// ======================= SORCERESS =======================
	// --- Cold Spells: inert as a page. This engine has no cold damage channel and no chill, so
	//     every one of these would have to be invented rather than adapted. Listed and described.
	// LIVE since 2026-09-03, Round 1 of the inert-skill plan. The description no longer has to
	// apologise: the engine has cold damage now, and the chill is the point of it.
	{ N_("Ice Bolt"), N_("A shard of ice that damages and chills what it hits, halving its speed for two seconds."), Sor, 0, 0, 0, Kind::Active, SpellID::IceBolt, true },
	{ N_("Frozen Armor"), N_("Armour of ice for 24 seconds, +4 per rank: whatever strikes you in melee is frozen in place."), Sor, 0, 0, 1, Kind::Active, SpellID::FrozenArmor, true },
	{ N_("Frost Nova"), N_("A ring of ice bursting out from you, chilling and damaging everything near."), Sor, 0, 1, 0, Kind::Active, SpellID::FrostNova, true },
	{ N_("Ice Blast"), N_("A heavier shard that freezes its target solid for a moment. Uniques are chilled instead."), Sor, 0, 1, 1, Kind::Active, SpellID::IceBlast, true },
	{ N_("Shiver Armor"), N_("Armour of ice for 24 seconds, +4 per rank: whatever strikes you in melee is chilled and cut by cold."), Sor, 0, 2, 0, Kind::Active, SpellID::ShiverArmor, true },
	{ N_("Glacial Spike"), N_("A spear of ice that freezes what it strikes and shatters, chilling everything beside it."), Sor, 0, 3, 0, Kind::Active, SpellID::GlacialSpike, true },
	{ N_("Blizzard"), N_("Ice falls over an area for a few seconds, chilling and damaging whatever stands in it."), Sor, 0, 4, 0, Kind::Active, SpellID::Blizzard, true },
	{ N_("Chilling Armor"), N_("Armour of ice for 24 seconds, +4 per rank: whatever hits you - near or far - is chilled and answered with an ice bolt."), Sor, 0, 4, 1, Kind::Active, SpellID::ChillingArmor, true },
	{ N_("Frozen Orb"), N_("An orb that drifts toward its mark shedding ice bolts, then bursts into a ring of them."), Sor, 0, 5, 0, Kind::Active, SpellID::FrozenOrb, true },
	{ N_("Cold Mastery"), N_("Every rank adds 6% to all cold damage. From rank 3 a resisting monster keeps only 50% of its protection; from rank 6, none."), Sor, 0, 5, 1, Kind::Passive, SpellID::Invalid, true },
	// --- Lightning Spells: most of this page is a wiring job - the engine already has the spells.
	{ N_("Charged Bolt"), N_("Looses a spray of erratic bolts. This engine's Charged Bolt, raised by its books rather than by skill points."), Sor, 1, 0, 0, Kind::Active, SpellID::ChargedBolt, true },
	// User note, 2026-09-14: "this should work like DMG aura of Paladin. Similar to Holy Fire." - an aura, pulsing in aura_field.cpp.
	{ N_("Static Field"), N_("An aura. Every second and a half the air around you cracks: everything within reach loses 4% of its remaining life as lightning, +1% per level, to 20%. Uniques lose half as much."), Sor, 1, 1, 0, Kind::Aura, SpellID::Invalid, true },
	{ N_("Telekinesis"), N_("Works objects and gathers items at a distance. This engine's Telekinesis, raised by its books rather than by skill points."), Sor, 1, 1, 1, Kind::Active, SpellID::Telekinesis, true },
	{ N_("Nova"), N_("A ring of lightning bursting outward. This engine's Nova, raised by its books rather than by skill points."), Sor, 1, 2, 0, Kind::Active, SpellID::Nova, true },
	{ N_("Lightning"), N_("A bolt that strikes in a line. This engine's Lightning, raised by its books rather than by skill points."), Sor, 1, 2, 1, Kind::Active, SpellID::Lightning, true },
	{ N_("Chain Lightning"), N_("A bolt that leaps between enemies. This engine's Chain Lightning, raised by its books rather than by skill points."), Sor, 1, 3, 0, Kind::Active, SpellID::ChainLightning, true },
	{ N_("Teleport"), N_("Step instantly to a place you can see. This engine's Teleport, raised by its books rather than by skill points."), Sor, 1, 3, 1, Kind::Active, SpellID::Teleport, true },
	// User note, 2026-09-14: an aura like Holy Fire - the storm strikes on its own while it burns.
	{ N_("Thunder Storm"), N_("An aura. Every second and a half a bolt falls on one enemy within 6 tiles of you for 1-20 lightning damage, +10 per level."), Sor, 1, 4, 0, Kind::Aura, SpellID::Invalid, true },
	{ N_("Energy Shield"), N_("Mana takes the damage your life would. This engine's Mana Shield, raised by its books rather than by skill points."), Sor, 1, 4, 1, Kind::Active, SpellID::ManaShield, true },
	{ N_("Lightning Mastery"), N_("Your blows carry lightning, and lightning troubles you less. Not D2's spell scaling: this engine deepens a spell by its LEVEL, and has no per-element channel to raise."), Sor, 1, 5, 0, Kind::Passive, SpellID::Invalid, true },
	// --- Fire Spells ---
	{ N_("Fire Bolt"), N_("A bolt of flame. This engine's Fire Bolt, raised by its books rather than by skill points."), Sor, 2, 0, 0, Kind::Active, SpellID::Firebolt, true },
	{ N_("Warmth"), N_("Your mana returns of its own accord."), Sor, 2, 0, 1, Kind::Passive, SpellID::Invalid, true },
	{ N_("Inferno"), N_("A gout of flame from your hands. This engine's Inferno, raised by its books rather than by skill points."), Sor, 2, 1, 0, Kind::Active, SpellID::Inferno, true },
	{ N_("Blaze"), N_("Leaves fire in your wake. Mapped onto this engine's Flame Wave, the nearest rolling fire it has."), Sor, 2, 2, 0, Kind::Active, SpellID::FlameWave, true },
	{ N_("Fire Ball"), N_("A bursting ball of flame. This engine's Fireball, raised by its books rather than by skill points."), Sor, 2, 2, 1, Kind::Active, SpellID::Fireball, true },
	{ N_("Fire Wall"), N_("A wall of flame across the ground. This engine's Fire Wall, raised by its books rather than by skill points."), Sor, 2, 3, 0, Kind::Active, SpellID::FireWall, true },
	{ N_("Enchant"), N_("Your weapon burns: every blow carries fire. A passive rather than a cast buff, since a tree skill with no spell slot has no way to be cast."), Sor, 2, 3, 1, Kind::Passive, SpellID::Invalid, true },
	{ N_("Meteor"), N_("A burning rock falls on the cursor a second after the cast: 20-40 fire damage, +8-12 per level, to everything within 2 tiles, and the ground burns for 3 seconds."), Sor, 2, 4, 0, Kind::Active, SpellID::Meteor, true },
	{ N_("Fire Mastery"), N_("Fire burns for you and less against you. Not D2's spell scaling: this engine deepens a spell by its LEVEL, and has no per-element channel to raise."), Sor, 2, 5, 1, Kind::Passive, SpellID::Invalid, true },
	{ N_("Hydra"), N_("Sets a fire-breathing head to guard a spot. Mapped onto this engine's Guardian, which is the same idea."), Sor, 2, 5, 0, Kind::Active, SpellID::Guardian, true },

	// ---- Passive Skills (page 3) ----
	{ N_("Power Hungry"), N_("+20% damage to anything five tiles away or further."),
	    Sor, 3, 0, 0, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Blur"), N_("Everything that strikes you deals -17% damage."),
	    Sor, 3, 0, 1, Kind::Passive, SpellID::Invalid, true, 1 },
	// Replaced 2026-09-14 (user note) - Evocation shortened cooldowns, and skills here have none.
	{ N_("Mana Attunement"), N_("+15% spell damage while your mana is above half."),
	    Sor, 3, 0, 2, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Glass Cannon"), N_("+15% damage and -10% armour."),
	    Sor, 3, 1, 0, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Prodigy"), N_("Spells costing 6 mana or less give 3 of it back."),
	    Sor, 3, 1, 1, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Astral Presence"), N_("You hold twenty more mana."),
	    Sor, 3, 1, 2, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Illusionist"), N_("A blow that takes 15% of your life or more leaves you running 50% faster for 3 seconds."),
	    Sor, 3, 2, 0, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Cold Blooded"), N_("+10% damage to anything chilled or frozen."),
	    Sor, 3, 2, 1, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Conflagration"), N_("Enemies your fire has struck in the last 3 seconds take +10% damage."),
	    Sor, 3, 2, 2, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Paralysis"), N_("Your lightning has a 15% chance to stun what it strikes for 1 second. Uniques shrug it off."),
	    Sor, 3, 3, 0, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Galvanizing Ward"), N_("Go 5 seconds unharmed and the next blow you take is halved."),
	    Sor, 3, 3, 1, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Temporal Flux"), N_("Your magic damage slows what it strikes for 2 seconds."),
	    Sor, 3, 3, 2, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Dominance"), N_("Every kill hardens you: 4% less damage taken for 5 seconds, stacking five high."),
	    Sor, 3, 4, 0, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Arcane Dynamo"), N_("Five spells costing 6 mana or less charge the next costlier spell: +60% spell damage for 3 seconds."),
	    Sor, 3, 4, 1, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Unstable Anomaly"), N_("Once a minute a killing blow leaves you standing at 33% of your life instead, and throws back everything within 2 tiles."),
	    Sor, 3, 4, 2, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Unwavering Will"), N_("Stand still a moment and blows deal -20% damage to you while yours deal +10%."),
	    Sor, 3, 5, 0, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Audacity"), N_("+15% damage to anything within two tiles."),
	    Sor, 3, 5, 1, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Elemental Exposure"), N_("Every element that has struck an enemy in the last 5 seconds - physical, fire, lightning, magic, cold - adds +5% damage against it."),
	    Sor, 3, 5, 2, Kind::Passive, SpellID::Invalid, true, 1 },
	// ---- RfA-12 skills (2026-09-13): the empty cells of the three class pages, from the final list ----
	{ N_("Chill Touch"), N_("A cone of frost through the three tiles ahead: 3-6 cold damage, +2-3 per level, and a 2-second chill."),
	    Sor, 0, 0, 2, Kind::Active, SpellID::ChillTouch, true },
	{ N_("Ice Needle"), N_("A needle of ice through the first enemy on its line into the second: 5-9 cold damage each, +3-4 per level, and a chill."),
	    Sor, 0, 1, 2, Kind::Active, SpellID::IceNeedle, true },
	{ N_("Frostbite"), N_("Curse: for 6 seconds an enemy near the cursor is chilled and takes 20% more cold damage, +2% per level to 60%."),
	    Sor, 0, 2, 1, Kind::Active, SpellID::Frostbite, true },
	{ N_("Ice Lance"), N_("A lance of ice through every enemy on an 8-tile line: 6-11 cold damage, +3-5 per level, and a chill."),
	    Sor, 0, 2, 2, Kind::Active, SpellID::IceLance, true },
	{ N_("Brittle Ground"), N_("Freezes a two-tile strip for 6 seconds: an enemy walking across takes 4-8 cold damage, +2-3 per level, at most once a second."),
	    Sor, 0, 3, 1, Kind::Active, SpellID::BrittleGround, true },
	{ N_("Frozen Sentinel"), N_("Raises an ice sentinel for 15 seconds that fires an Ice Bolt at the nearest enemy every 1.5 seconds."),
	    Sor, 0, 3, 2, Kind::Active, SpellID::FrozenSentinel, true },
	{ N_("Whiteout"), N_("A three-tile wall of snow rolls 8 tiles ahead, striking each enemy once for 5-10 cold damage, +3-4 per level, and chilling it."),
	    Sor, 0, 4, 2, Kind::Active, SpellID::Whiteout, true },
	{ N_("Absolute Zero"), N_("Everything within 8 tiles takes 8-16 cold damage, +4-6 per level, and freezes solid for 2 seconds. Uniques are chilled instead."),
	    Sor, 0, 5, 2, Kind::Active, SpellID::AbsoluteZero, true },
	{ N_("Arc"), N_("A bolt that strikes an enemy near the cursor for 2-12 lightning damage, +2-5 per level, then leaps to two more within 3 tiles for less each time."),
	    Sor, 1, 0, 1, Kind::Active, SpellID::Arc, true },
	{ N_("Static Charge"), N_("For 20 seconds, +2 per level, every enemy that strikes you in melee takes 2-10 lightning damage, +1-3 per level."),
	    Sor, 1, 0, 2, Kind::Active, SpellID::StaticCharge, true },
	{ N_("Ball Lightning"), N_("A ball of lightning rolls 8 tiles ahead, throwing off a charged bolt at every tile."),
	    Sor, 1, 1, 2, Kind::Active, SpellID::BallLightning, true },
	{ N_("Conduit"), N_("For 10 seconds: +20% faster cast rate, +2% per level to 60%, and your mana flows back."),
	    Sor, 1, 2, 2, Kind::Active, SpellID::Conduit, true },
	{ N_("Lightning Rod"), N_("Plants a rod for 12 seconds that swallows the first enemy lightning missile to come near it and bursts: 4-14 lightning damage within 2 tiles, +2-5 per level."),
	    Sor, 1, 3, 2, Kind::Active, SpellID::LightningRod, true },
	{ N_("Faraday Ring"), N_("A ring for 4 seconds that destroys every enemy missile within 2 tiles of it."),
	    Sor, 1, 4, 2, Kind::Active, SpellID::FaradayRing, true },
	{ N_("Storm Crucible"), N_("Cast once to place a conductor and again within 8 seconds to place its pair: lightning runs between them three times, 4-16 damage, +2-6 per level."),
	    Sor, 1, 5, 1, Kind::Active, SpellID::StormCrucible, true },
	{ N_("Ride the Lightning"), N_("Ride a bolt up to 6 tiles to the cursor, striking everything on the way for 3-12 lightning damage, +2-4 per level."),
	    Sor, 1, 5, 2, Kind::Active, SpellID::RideTheLightning, true },
	{ N_("Cinder Touch"), N_("A touch that sets the enemy ahead burning for 3 seconds: 3 fire damage a second, +1 per level."),
	    Sor, 2, 0, 2, Kind::Active, SpellID::CinderTouch, true },
	{ N_("Ember Mine"), N_("Leaves an ember that bursts when an enemy steps on it within 20 seconds: 6-12 fire damage beside it, +3-5 per level."),
	    Sor, 2, 1, 1, Kind::Active, SpellID::EmberMine, true },
	{ N_("Flame Ring"), N_("A ring of fire bursts around you: 4-8 fire damage within 2 tiles, +2-3 per level."),
	    Sor, 2, 1, 2, Kind::Active, SpellID::FlameRing, true },
	{ N_("Ashen Brand"), N_("Brands an enemy for 4 seconds; if it dies, it bursts for 6-12 fire damage beside it, +3-5 per level."),
	    Sor, 2, 2, 2, Kind::Active, SpellID::AshenBrand, true },
	{ N_("Furnace Mouth"), N_("Opens a vent that spits flame three tiles ahead once a second for 3 seconds: 4-9 fire damage, +2-4 per level."),
	    Sor, 2, 3, 2, Kind::Active, SpellID::FurnaceMouth, true },
	{ N_("Firestorm"), N_("Fireballs rain around the cursor for 4 seconds."),
	    Sor, 2, 4, 1, Kind::Active, SpellID::Firestorm, true },
	{ N_("Immolate"), N_("For 10 seconds you burn everything beside you: 3-6 fire damage a second, +1-2 per level."),
	    Sor, 2, 4, 2, Kind::Active, SpellID::Immolate, true },
	{ N_("Funeral Star"), N_("Stand still for 2 seconds and a star bursts at the cursor: 15-30 fire damage within 3 tiles, +6-10 per level. Moving cancels it."),
	    Sor, 2, 5, 2, Kind::Active, SpellID::FuneralStar, true },
	// ======================= ROGUE =======================
	// --- Bow & Crossbow: the bow skills all want missile work this engine has not been given yet.
	{ N_("Magic Arrow"), N_("An arrow of pure force: your bow damage as magic, plus a little a rank."), Rog, 0, 0, 0, Kind::Active, SpellID::MagicArrow, true },
	{ N_("Fire Arrow"), N_("An arrow wrapped in flame: your bow damage as fire, plus a little a rank."), Rog, 0, 0, 1, Kind::Active, SpellID::FireArrow, true },
	{ N_("Cold Arrow"), N_("An arrow sheathed in frost: your bow damage as cold, and it chills what it hits."), Rog, 0, 1, 0, Kind::Active, SpellID::ColdArrow, true },
	{ N_("Multiple Shot"), N_("Looses a fan of arrows at once - two, and one more every two ranks."), Rog, 0, 1, 1, Kind::Active, SpellID::MultipleShot, true },
	{ N_("Exploding Arrow"), N_("A fire arrow that bursts where it stops, burning the tiles around it."), Rog, 0, 2, 0, Kind::Active, SpellID::ExplodingArrow, true },
	{ N_("Ice Arrow"), N_("A frost arrow that freezes what it hits solid for a moment."), Rog, 0, 2, 1, Kind::Active, SpellID::IceArrow, true },
	{ N_("Guided Arrow"), N_("An arrow that cannot miss."), Rog, 0, 3, 0, Kind::Active, SpellID::GuidedArrow, true },
	{ N_("Strafe"), N_("One arrow at each enemy in view, nearest first - three, and one more every two ranks."), Rog, 0, 4, 0, Kind::Active, SpellID::Strafe, true },
	{ N_("Immolation Arrow"), N_("A fire arrow that leaves a wall of flame burning where it stops."), Rog, 0, 4, 1, Kind::Active, SpellID::ImmolationArrow, true },
	{ N_("Freezing Arrow"), N_("A frost arrow that freezes everything around where it stops."), Rog, 0, 5, 0, Kind::Active, SpellID::FreezingArrow, true },
	// --- Passive & Magic ---
	{ N_("Inner Sight"), N_("Reveals the weak points of everything in earshot: -33% armour, -2% more per rank, for 20 seconds."), Rog, 1, 0, 0, Kind::Active, SpellID::InnerSight, true },
	{ N_("Critical Strike"), N_("A chance to strike at +100% damage. This engine has no critical roll, so it raises your damage instead."),
	    Rog, 1, 0, 1, Kind::Passive, SpellID::Invalid, true },
	{ N_("Dodge"), N_("A chance to slip a blow while standing: 10%, +4% per rank, 40% at most."), Rog, 1, 1, 0, Kind::Passive, SpellID::Invalid, true },
	{ N_("Slow Missiles"), N_("For 20 seconds, +4 per rank, 50% of the arrows aimed at you turn aside, +5% per rank."), Rog, 1, 2, 0, Kind::Active, SpellID::SlowMissiles, true },
	{ N_("Avoid"), N_("A chance to slip an arrow: 10%, +4% per rank, 40% at most."), Rog, 1, 2, 1, Kind::Passive, SpellID::Invalid, true },
	{ N_("Penetrate"), N_("Sharpens your aim with anything you wield."), Rog, 1, 3, 0, Kind::Passive, SpellID::Invalid, true },
	// User note, 2026-09-14: "Summon recolored clone. Use Golem mechanic to control it." The Golem slot, disarmed.
	{ N_("Decoy"), N_("A blue ghost of yourself stands at the cursor for 15 seconds, +1 per level. It strikes no one, but everything near it attacks it instead of you."), Rog, 1, 3, 1, Kind::Active, SpellID::Decoy, true },
	{ N_("Evade"), N_("A chance to slip a blow while moving: 10%, +4% per rank, 40% at most."), Rog, 1, 4, 0, Kind::Passive, SpellID::Invalid, true },
	// User, 2026-09-14: it asked for a book and wore the Golem's red icon, because it rode SpellID::Golem - a book
	// spell. Its own id now: skill points and the Valkyrie glyph. The body is still the Golem.
	{ N_("Valkyrie"), N_("Calls a Valkyrie archer to guard you for 30 seconds, +5 per level. She keeps close and shoots what you strike, or the enemy nearest you, for 50% of your damage, +5% per level, with a volley every 8 seconds. 150 life, rising to 1000 at level 20; her resistances reach 90%."),
	    Rog, 1, 5, 0, Kind::Active, SpellID::Valkyrie, true },
	{ N_("Pierce"), N_("Your arrows may carry on through what they strike: 15%, +5% per rank, 60% at most."), Rog, 1, 5, 1, Kind::Passive, SpellID::Invalid, true },
	// --- Javelin & Spear ---
	{ N_("Jab"), N_("Three quick thrusts in one motion, the second and third at 50% damage, +5% per rank."), Rog, 2, 0, 0, Kind::Active, SpellID::Jab, true },
	{ N_("Power Strike"), N_("A thrust at +30% damage, +5% per rank, with 1-4 lightning damage per rank on top of it."), Rog, 2, 1, 0, Kind::Active, SpellID::PowerStrike, true },
	// User note, 2026-09-14: "This engine has Acid, so use Acid."
	{ N_("Poison Javelin"), N_("A javelin of acid: the first enemy in its path takes 60% of your weapon's damage as acid, +5% per level, and the acid eats at the ground under it for 3 seconds."), Rog, 2, 1, 1, Kind::Active, SpellID::PoisonJavelin, true },
	{ N_("Impale"), N_("A savage thrust at +100% damage, +20% per rank."), Rog, 2, 2, 0, Kind::Active, SpellID::Impale, true },
	{ N_("Charged Strike"), N_("A thrust at +20% damage, +5% per rank, that throws off two charged bolts toward the target, one more every two ranks."), Rog, 2, 2, 1, Kind::Active, SpellID::ChargedStrike, true },
	{ N_("Lightning Bolt"), N_("Hurl a bolt of lightning that races along the ground toward the target, at the rank. No javelin exists here; the bolt carries itself."), Rog, 2, 3, 0, Kind::Active, SpellID::LightningBoltSkill, true },
	{ N_("Plague Javelin"), N_("A javelin that bursts into a cloud of acid where it strikes: 4-8 acid damage, +2-3 per level, every second for 5 seconds to everything within 2 tiles."), Rog, 2, 3, 1, Kind::Active, SpellID::PlagueJavelin, true },
	{ N_("Fend"), N_("Every swing also strikes everything around you at 80% damage, +5% per rank."), Rog, 2, 4, 0, Kind::Active, SpellID::Fend, true },
	{ N_("Lightning Strike"), N_("A thrust at +20% damage, +5% per rank, whose lightning leaps on from the target to the next enemy, and the next."), Rog, 2, 5, 0, Kind::Active, SpellID::LightningStrike, true },
	{ N_("Lightning Fury"), N_("Hurl lightning that bursts outward in every direction at once, at the rank."), Rog, 2, 5, 1, Kind::Active, SpellID::LightningFury, true },

	// ---- Passive Skills (page 3) ----
	{ N_("Thrill of the Hunt"), N_("Your arrows have a 20% chance to slow what they strike for 2 seconds."),
	    Rog, 3, 0, 0, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Tactical Advantage"), N_("Dodging, evading or avoiding a blow leaves you running 40% faster for 3 seconds."),
	    Rog, 3, 0, 1, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Blood Vengeance"), N_("You hold 20 more mana, and every kill restores 3."),
	    Rog, 3, 0, 2, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Steady Aim"), N_("With nothing within three tiles of you, +20% damage to everything you do."),
	    Rog, 3, 1, 0, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Cull the Weak"), N_("+20% damage against anything chilled or frozen."),
	    Rog, 3, 1, 1, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Night Stalker"), N_("Every arrow that strikes restores 1 mana."),
	    Rog, 3, 1, 2, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Brooding"), N_("Stand still a moment and your wounds close, a hundredth of your life a second."),
	    Rog, 3, 2, 0, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Hot Pursuit"), N_("Landing a blow leaves you running 20% faster for 2 seconds."),
	    Rog, 3, 2, 1, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Archery"), N_("Your bow lends its gift: a short or hunter's bow +15% chance to hit, a long or war bow +15% damage, a composite or battle bow 1 mana for every arrow that strikes."),
	    Rog, 3, 2, 2, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Numbing Traps"), N_("Anything chilled or frozen strikes you 25% weaker."),
	    Rog, 3, 3, 0, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Perfectionist"), N_("+10% armour and +10 to every resistance."),
	    Rog, 3, 3, 1, Kind::Passive, SpellID::Invalid, true, 1 },
	// User note, 2026-09-14: "Diablo Hellfire introduces trap runes. Use their mechanics."
	{ N_("Custom Engineering"), N_("The rune traps you set strike as if three levels stronger, and half the time the rune is not used up."),
	    Rog, 3, 3, 2, Kind::Passive, SpellID::Invalid, true, 1 },
	// User note, 2026-09-14: "Use Magic Star mechanics for granades." A lobbed fire burst after every fourth arrow.
	{ N_("Grenadier"), N_("Every fourth arrow you loose is followed by a grenade that bursts in flame where it lands."),
	    Rog, 3, 4, 0, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Sharpshooter"), N_("Every second without a critical blow adds 4% to the chance of one; a critical blow deals double and starts the count again."),
	    Rog, 3, 4, 1, Kind::Passive, SpellID::Invalid, true, 1 },
	// Off the page (user, 2026-09-12: "i dont want 19th (lvl 36) skill"), as the Barbarian's Boon of
	// Bul-Kathos: this unbuilt passive left, and the four after it moved up a cell - Leech to level 30,
	// Single Out into the last cell at 36. Kept in the table; see RetiredFromTreePage.
	{ N_("Ballistics"), N_("Your rockets hit at +100% damage and sometimes seek their mark. Not yet built."),
	    Rog, RetiredFromTreePage, 4, 2, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Leech"), N_("Every blow you land returns three hundredths of its damage as life."),
	    Rog, 3, 4, 2, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Ambush"), N_("+40% damage against anything above 75% of its life."),
	    Rog, 3, 5, 0, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Awareness"), N_("Once a minute a killing blow leaves you standing at 33% of your life instead."),
	    Rog, 3, 5, 1, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Single Out"), N_("+25% damage against anything with no fellow within two tiles."),
	    Rog, 3, 5, 2, Kind::Passive, SpellID::Invalid, true, 1 },
	// ---- RfA-12 skills (2026-09-13): the empty cells of the three class pages, from the final list ----
	{ N_("Barbed Shaft"), N_("An arrow at 100% damage, +5% per level, that makes its target bleed for 3 seconds: 3 damage a second, +1 per level. Needs a bow."),
	    Rog, 0, 0, 2, Kind::Active, SpellID::BarbedShaft, true },
	{ N_("Shock Arrow"), N_("An arrow whose lightning arcs to one more enemy within 3 tiles: 1-6 lightning damage, +1-3 per level. Needs a bow."),
	    Rog, 0, 1, 2, Kind::Active, SpellID::ShockArrow, true },
	{ N_("Piercing Shot"), N_("An arrow through every enemy on a 10-tile line, each for 80% damage, +5% per level. Needs a bow."),
	    Rog, 0, 2, 2, Kind::Active, SpellID::PiercingShot, true },
	{ N_("Rain of Arrows"), N_("A volley on the cursor: everything within 2 tiles takes 50% of an arrow, +4% per level. Needs a bow."),
	    Rog, 0, 3, 1, Kind::Active, SpellID::RainOfArrows, true },
	{ N_("Crippling Shot"), N_("An arrow that slows its target to half speed for 4 seconds. Needs a bow."),
	    Rog, 0, 3, 2, Kind::Active, SpellID::CripplingShot, true },
	{ N_("Hunter's Mark"), N_("Marks an enemy for 10 seconds, +1 per level: your arrows deal it 20% more damage, +2% per level to 60%. Needs a bow."),
	    Rog, 0, 4, 2, Kind::Active, SpellID::HuntersMark, true },
	{ N_("Barrage"), N_("Five arrows at one target, each for 40% damage, +3% per level. Needs a bow."),
	    Rog, 0, 5, 1, Kind::Active, SpellID::Barrage, true },
	{ N_("Phantom Volley"), N_("Spectral arrows strike every enemy within 6 tiles for 40% of an arrow as magic damage, +3% per level. Needs a bow."),
	    Rog, 0, 5, 2, Kind::Active, SpellID::PhantomVolley, true },
	{ N_("Soft Tread"), N_("After 3 seconds walking without attacking, monsters notice you only within two thirds of your sight."),
	    Rog, 1, 0, 2, Kind::Passive, SpellID::Invalid, true },
	{ N_("Swiftness"), N_("+5% movement speed, +1% per level, to 35%."),
	    Rog, 1, 1, 1, Kind::Passive, SpellID::Invalid, true },
	{ N_("Scent of Blood"), N_("An enemy you have wounded stays visible for 2 seconds after it leaves the light."),
	    Rog, 1, 1, 2, Kind::Passive, SpellID::Invalid, true },
	{ N_("Sharpen"), N_("Adds 2 damage to every blow and arrow, +1 per level."),
	    Rog, 1, 2, 2, Kind::Passive, SpellID::Invalid, true },
	{ N_("Dead Ground"), N_("Your first ranged hit on an enemy that has stood still for 2 seconds deals +25% damage, +2% per level; once every 6 seconds per enemy."),
	    Rog, 1, 3, 2, Kind::Passive, SpellID::Invalid, true },
	{ N_("Deadeye"), N_("Ranged hits have a 10% chance to deal +50% damage, +5% per level."),
	    Rog, 1, 4, 1, Kind::Passive, SpellID::Invalid, true },
	{ N_("Shadow Step"), N_("Step through the shadows to the far side of an enemy near the cursor."),
	    Rog, 1, 4, 2, Kind::Active, SpellID::ShadowStep, true },
	{ N_("Hunter's Claim"), N_("Claims a unique or boss near the cursor for 8 seconds: your arrows pass by every ordinary monster in between."),
	    Rog, 1, 5, 2, Kind::Active, SpellID::HuntersClaim, true },
	{ N_("Sweep"), N_("A sweep that also strikes the enemies beside you, each for 80% of a blow, +5% per level."),
	    Rog, 2, 0, 1, Kind::Active, SpellID::Sweep, true },
	{ N_("Brace"), N_("Holding a spear or pike: +10% armour, +3% per level, and you can block - 5% of blows, +1% per level to 30%."),
	    Rog, 2, 0, 2, Kind::Passive, SpellID::Invalid, true },
	{ N_("Harpoon"), N_("A thrown spear at the first enemy on an 8-tile line: 60% of a blow, +5% per level, and it drags the enemy a step toward you."),
	    Rog, 2, 1, 2, Kind::Active, SpellID::Harpoon, true },
	{ N_("Vault"), N_("Pole-vault up to 3 tiles to the cursor, a tile further every 5 levels, to 6."),
	    Rog, 2, 2, 2, Kind::Active, SpellID::Vault, true },
	{ N_("Reaping Point"), N_("A thrust at +15% damage, +4% per level, that runs on into the enemy behind your target for 100% of a blow, +5% per level."),
	    Rog, 2, 3, 2, Kind::Active, SpellID::ReapingPoint, true },
	{ N_("Anchor Javelin"), N_("A javelin at the first enemy on an 8-tile line: 70% of a blow, +5% per level, and it is pinned for 2 seconds. Uniques only take the damage."),
	    Rog, 2, 4, 1, Kind::Active, SpellID::AnchorJavelin, true },
	{ N_("Turning Pike"), N_("A strike at +20% damage, +5% per level, and you pivot to a free tile beside the target."),
	    Rog, 2, 4, 2, Kind::Active, SpellID::TurningPike, true },
	{ N_("Valkyrie's Spear"), N_("A great spear hurled at the cursor bursts for 150% of a blow, +15% per level, on everything within a tile."),
	    Rog, 2, 5, 2, Kind::Active, SpellID::ValkyriesSpear, true },
	// ======================= BARD =======================
	// The user's own design rather than Diablo II's: seven songs per discipline, described on the
	// sheet itself. The working songs are AURAS, which is both what they are - a bard plays one
	// song at a time - and free: the one-at-a-time machinery the Paladin's auras use is already
	// generic over ClassTreeSkill. Their descriptions say "and allies"; this is single-player, so
	// in practice that means you.
	// --- Melody ---
	{ N_("Melody of Life"), N_("A song that mends your wounds as it plays."), Bard, 0, 0, 0, Kind::Aura, SpellID::Invalid, true },
	{ N_("Battle Hymn"), N_("A song that sharpens your aim and your blow."), Bard, 0, 0, 1, Kind::Aura, SpellID::Invalid, true },
	{ N_("Song of Swiftness"), N_("A song that quickens your strikes and your stride - you run rather than walk."),
	    Bard, 0, 1, 0, Kind::Aura, SpellID::Invalid, true },
	{ N_("Song of Fortitude"), N_("A song that hardens your guard and your wards."), Bard, 0, 1, 1, Kind::Aura, SpellID::Invalid, true },
	{ N_("Dirge of Dread"), N_("A dirge that leaves what hears it at -15% damage, -2% more per point, and sends all but the uniques fleeing."),
	    Bard, 0, 2, 0, Kind::Aura, SpellID::Invalid, true },
	{ N_("Lullaby"), N_("A song that leaves everything in earshot asleep on its feet for 4 seconds, +0.5 per rank, until it is struck. Uniques do not sleep."), Bard, 0, 2, 1, Kind::Active, SpellID::Lullaby, true },
	{ N_("Epic Solo"), N_("Mastery that empowers every Melody song. Inert: there is no per-page channel here."),
	    Bard, 0, 5, 0, Kind::Passive, SpellID::Invalid, false },
	// --- Harmony ---
	{ N_("Sound Shock"), N_("A burst of sound through the three tiles ahead, for four to ten and two to four more a rank, that staggers what it strikes."), Bard, 1, 0, 0, Kind::Active, SpellID::SoundShock, true },
	{ N_("Shout"), N_("A shout that leaves everything within three tiles reeling for 1 second, +20% per rank. Uniques shrug it off."), Bard, 1, 0, 1, Kind::Active, SpellID::BardShout, true },
	{ N_("Sonic Barrier"), N_("A barrier that drinks the damage meant for you. This engine's Mana Shield, raised by its books rather than by skill points."),
	    Bard, 1, 1, 0, Kind::Active, SpellID::ManaShield, true },
	{ N_("Discord"), N_("A discord that strips -20% armour from what hears it, -2% more per point."), Bard, 1, 1, 1, Kind::Aura, SpellID::Invalid, true },
	{ N_("Resonance"), N_("Your blows amplify your next song. Inert: no such carry-over exists here."),
	    Bard, 1, 2, 0, Kind::Passive, SpellID::Invalid, false },
	{ N_("Echoing Song"), N_("Your songs reach further and last longer. Inert: songs here have neither range nor duration."),
	    Bard, 1, 2, 1, Kind::Passive, SpellID::Invalid, false },
	{ N_("Perfect Harmony"), N_("Mastery that empowers every Harmony skill. Inert: there is no per-page channel here."),
	    Bard, 1, 5, 0, Kind::Passive, SpellID::Invalid, false },
	// --- Poetry ---
	{ N_("Daze"), N_("A verse that sends everything in earshot stumbling off in a direction of its own. Uniques keep their feet."),
	    Bard, 2, 0, 0, Kind::Active, SpellID::Daze, true },
	{ N_("Charm"), N_("Turns a monster to your side. Rides this engine's Berserk, which does exactly that."),
	    Bard, 2, 0, 1, Kind::Active, SpellID::Berserk, true },
	{ N_("Inspiration"), N_("A verse that returns your mana as it plays."), Bard, 2, 1, 0, Kind::Aura, SpellID::Invalid, true },
	{ N_("Tale of Heroes"), N_("A verse that lends you a hero's strength and grace."), Bard, 2, 1, 1, Kind::Aura, SpellID::Invalid, true },
	{ N_("Weaken"), N_("A drone that blunts the aim of what hears it by twenty, two more a point, and slows its step."),
	    Bard, 2, 2, 0, Kind::Aura, SpellID::Invalid, true },
	{ N_("Ode to Glory"), N_("Raises a fallen ally to fight on. Inert: it needs the corpse-handling pass."),
	    Bard, 2, 2, 1, Kind::Active, SpellID::Invalid, false },
	{ N_("Legendary Ballad"), N_("Mastery that empowers every Poetry skill. Inert: there is no per-page channel here."),
	    Bard, 2, 5, 0, Kind::Passive, SpellID::Invalid, false },

	// ---- Passive Skills (page 3) ----
	{ N_("Perfect Pitch"), N_("Hold a song and your blows grow stronger: +2% damage every 5 seconds it plays, up to +20%."),
	    Bard, 3, 0, 0, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Crescendo"), N_("While a song plays, every blow you land adds +1% damage, stacking to +15%, until the song changes."),
	    Bard, 3, 0, 1, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Sustain"), N_("Your songs hold their power well after you stop playing them. Not yet built: a song stops the moment another is chosen; nothing lingers."),
	    Bard, 3, 0, 2, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Countermelody"), N_("A second song may play beneath the first at 50% strength. Not yet built: only one aura or song can play at a time."),
	    Bard, 3, 1, 0, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Rhythm"), N_("While a song plays, you attack faster."),
	    Bard, 3, 1, 1, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Refrain"), N_("A song that has run its course begins again at no cost. Not yet built: songs here have no duration or cost to renew."),
	    Bard, 3, 1, 2, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Encore"), N_("Falling silent leaves the last song ringing a while longer. Not yet built: a song stops the moment another is chosen; nothing lingers."),
	    Bard, 3, 2, 0, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Cadence"), N_("Every third blow lands on the beat at +50% damage."),
	    Bard, 3, 2, 1, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Timbre"), N_("Your songs reach far further from you. Not yet built: songs here work on you and have no range to extend."),
	    Bard, 3, 2, 2, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Virtuoso"), N_("Your songs cost far less to hold. Not yet built: songs here cost nothing to hold."),
	    Bard, 3, 3, 0, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Dissonance"), N_("While a song plays, enemies within 3 tiles strike 20% weaker."),
	    Bard, 3, 3, 1, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Improvisation"), N_("Switching songs costs nothing and briefly grants both. Not yet built: switching songs already costs nothing, and two cannot play at once."),
	    Bard, 3, 3, 2, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Chorus"), N_("Every ally of yours within 5 tiles lends +10% damage, up to +30%."),
	    Bard, 3, 4, 0, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Overture"), N_("The first song of a fight begins at its full power. Not yet built: songs here start at full power already."),
	    Bard, 3, 4, 1, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Reverberation"), N_("Your songs echo, striking a second time for less. Not yet built: songs here do not strike, so there is nothing to echo."),
	    Bard, 3, 4, 2, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Stagecraft"), N_("While a song plays, blows do not stagger you."),
	    Bard, 3, 5, 0, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Requiem"), N_("Each enemy that falls within four tiles mends a fiftieth of your life."),
	    Bard, 3, 5, 1, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Magnum Opus"), N_("Hold one song for 30 seconds and it becomes greater: +15% damage and 10% less damage taken while it plays on."),
	    Bard, 3, 5, 2, Kind::Passive, SpellID::Invalid, true, 1 },
	// ---- RfA-12 skills (2026-09-13): the empty cells of the three class pages, from the final list ----
	{ N_("Minstrel's Tune"), N_("A tune that restores your mana as it plays."),
	    Bard, 0, 0, 2, Kind::Aura, SpellID::Invalid, true },
	{ N_("Ballad of Resilience"), N_("+10% to every resistance while it plays, +3% per level."),
	    Bard, 0, 1, 2, Kind::Aura, SpellID::Invalid, true },
	{ N_("Hunter's Chant"), N_("+15% chance to hit while it plays, +5% per level."),
	    Bard, 0, 2, 2, Kind::Aura, SpellID::Invalid, true },
	{ N_("Serenade of Steel"), N_("+20% armour while it plays, +8% per level."),
	    Bard, 0, 3, 0, Kind::Aura, SpellID::Invalid, true },
	{ N_("Song of Plenty"), N_("+15% gold find and +8% magic find while it plays, +4% and +2% per level."),
	    Bard, 0, 3, 1, Kind::Aura, SpellID::Invalid, true },
	{ N_("Nocturne"), N_("While it plays, monsters notice you only within half your sight."),
	    Bard, 0, 3, 2, Kind::Aura, SpellID::Invalid, true },
	{ N_("Anthem of Valor"), N_("While it plays, blows do not interrupt you."),
	    Bard, 0, 4, 0, Kind::Aura, SpellID::Invalid, true },
	{ N_("Siren's Call"), N_("Once a second, enemies in reach turn on you and come at half speed."),
	    Bard, 0, 4, 1, Kind::Aura, SpellID::Invalid, true },
	{ N_("Hymn of Renewal"), N_("A hymn that restores both life and mana as it plays."),
	    Bard, 0, 4, 2, Kind::Aura, SpellID::Invalid, true },
	{ N_("Symphony of War"), N_("While it plays, every other Melody song you have learned lends half its strength."),
	    Bard, 0, 5, 1, Kind::Aura, SpellID::Invalid, true },
	{ N_("Sovereign Measure"), N_("With exactly one enemy within four tiles, every 4th blow on it deals 6-10 magic damage, +2-3 per level."),
	    Bard, 0, 5, 2, Kind::Aura, SpellID::Invalid, true },
	{ N_("Plucked Needle"), N_("A narrow note that strikes the first enemy on an 8-tile line for 4-8 magic damage, +2-4 per level."),
	    Bard, 1, 0, 2, Kind::Active, SpellID::PluckedNeedle, true },
	{ N_("Shatter Note"), N_("A note through every enemy on an 8-tile line: 3-7 magic damage, +2-3 per level, and 15% of its armour gone for 8 seconds, +1% per level."),
	    Bard, 1, 1, 2, Kind::Active, SpellID::ShatterNote, true },
	{ N_("Tuning Fork"), N_("Plants a fork that hums for 12 seconds, striking everything within 2 tiles once a second for 2-5 magic damage, +1-2 per level."),
	    Bard, 1, 2, 2, Kind::Active, SpellID::TuningFork, true },
	{ N_("Thunderclap"), N_("A clap that strikes everything within 2 tiles for 1-4 lightning damage, +1-2 per level, and stuns it for 1.5 seconds."),
	    Bard, 1, 3, 0, Kind::Active, SpellID::Thunderclap, true },
	{ N_("Dissonant Thread"), N_("Threads two enemies near the cursor together for 8 seconds; if they move more than 3 tiles apart it snaps, striking both for 6-12 magic damage, +3-5 per level."),
	    Bard, 1, 3, 1, Kind::Active, SpellID::DissonantThread, true },
	{ N_("Sound Wave"), N_("A wave through every enemy on a 12-tile line: 4-9 magic damage, +2-4 per level."),
	    Bard, 1, 3, 2, Kind::Active, SpellID::SoundWave, true },
	{ N_("Deafening Roar"), N_("Holds every monster in earshot silent and still for 2 seconds. Uniques shrug it off."),
	    Bard, 1, 4, 0, Kind::Active, SpellID::DeafeningRoar, true },
	{ N_("Chord of Warding"), N_("A ward of sound for 15 seconds that absorbs the next 20 damage you take, +8 per level."),
	    Bard, 1, 4, 1, Kind::Active, SpellID::ChordOfWarding, true },
	{ N_("Feedback"), N_("For 6 seconds, missiles and spells that strike you return 30% of their damage to the monster that cast them, +2% per level to 70%."),
	    Bard, 1, 4, 2, Kind::Active, SpellID::Feedback, true },
	{ N_("Grand Finale"), N_("A burst within 3 tiles: 6-12 magic damage, +3-5 per level - half again as much while a song is playing."),
	    Bard, 1, 5, 1, Kind::Active, SpellID::GrandFinale, true },
	{ N_("Music of the Spheres"), N_("For 15 seconds, notes orbit you and strike everything within 2 tiles twice a second for 1-3 magic damage, +1 per level."),
	    Bard, 1, 5, 2, Kind::Active, SpellID::MusicOfTheSpheres, true },
	{ N_("Bitter Couplet"), N_("Curse: an enemy near the cursor cannot regenerate life for 4 seconds."),
	    Bard, 2, 0, 2, Kind::Active, SpellID::BitterCouplet, true },
	{ N_("Mocking Rhyme"), N_("Curse: an enemy near the cursor deals 20% less damage for 10 seconds, +2% per level to 50%."),
	    Bard, 2, 1, 2, Kind::Active, SpellID::MockingRhyme, true },
	{ N_("Epitaph"), N_("A corpse near the cursor bursts: 6-12 magic damage within 2 tiles, +3-5 per level."),
	    Bard, 2, 2, 2, Kind::Active, SpellID::Epitaph, true },
	{ N_("Elegy"), N_("Curse: an enemy near the cursor takes 3 magic damage a second for 6 seconds, +1 per level."),
	    Bard, 2, 3, 0, Kind::Active, SpellID::Elegy, true },
	{ N_("Sonnet of Sight"), N_("Reveals the map for 12 tiles around you, and every monster there stays visible for 5 seconds."),
	    Bard, 2, 3, 1, Kind::Active, SpellID::SonnetOfSight, true },
	{ N_("Satire"), N_("Curse: enemies near the cursor lose their resistances for 8 seconds. Immunities hold."),
	    Bard, 2, 3, 2, Kind::Active, SpellID::Satire, true },
	{ N_("Verse of Binding"), N_("Roots every enemy within 2 tiles of the cursor for 3 seconds. Uniques shrug it off."),
	    Bard, 2, 4, 0, Kind::Active, SpellID::VerseOfBinding, true },
	{ N_("Heroic Couplet"), N_("The next Poetry verse you speak takes effect twice."),
	    Bard, 2, 4, 1, Kind::Active, SpellID::HeroicCouplet, true },
	{ N_("Tragedy"), N_("Curse: for 8 seconds, damage you deal an enemy near the cursor is also dealt to the enemies beside it at 25%, +1% per level to 50%."),
	    Bard, 2, 4, 2, Kind::Active, SpellID::Tragedy, true },
	{ N_("Saga"), N_("For 20 seconds, +2 to every skill you have."),
	    Bard, 2, 5, 1, Kind::Active, SpellID::Saga, true },
	{ N_("Last Word"), N_("An ordinary enemy near the cursor below 20% of its life dies outright."),
	    Bard, 2, 5, 2, Kind::Active, SpellID::LastWord, true },
	// ======================= MONK =======================
	// The user's design doc (MONK_SKILL_TREE.md in the package) is the specification, including
	// the two things this tree did not previously support: a SEVENTH tier at character level 36,
	// and per-skill rank caps - five ranks for skills 1-6, one for each branch capstone.
	//
	// Every skill sits in the middle column because each branch is a ladder: seven skills, one per
	// tier, each requiring the one below it.
	// --- Way of the Staff ---
	{ N_("Sweeping Reed"), N_("Sweep your staff through the three tiles ahead - the target and both beside it - at full force."),
	    Monk, 0, 0, 0, Kind::Active, SpellID::SweepingReed, true, 5 },
	{ N_("Breaking Current"), N_("A focused strike at +33% damage that leaves the target reeling for 1 second. Uniques shrug the stagger off."),
	    Monk, 0, 1, 0, Kind::Active, SpellID::BreakingCurrent, true, 5 },
	{ N_("Reed in the Wind"), N_("While holding a staff, +10% chance to block, +2% per level."),
	    Monk, 0, 2, 0, Kind::Passive, SpellID::Invalid, true, 5 },
	{ N_("Vaulting Strike"), N_("Vault onto a distant foe; the blow you land there is at +50% damage, +10% per rank."),
	    Monk, 0, 3, 0, Kind::Active, SpellID::VaultingStrike, true, 5 },
	{ N_("Wheel of Heaven"), N_("Every swing strikes everything around you at 66% damage, +5% per rank."),
	    Monk, 0, 4, 0, Kind::Active, SpellID::WheelOfHeaven, true, 5 },
	{ N_("Seven Reeds"), N_("Three blows in one swing, one more every three ranks up to seven, each at 60% damage."),
	    Monk, 0, 5, 0, Kind::Active, SpellID::SevenReeds, true, 5 },
	{ N_("Master of the Long Staff"), N_("Your mastery of the staff empowers every Way of the Staff skill. With a staff in hand: +10% damage and a sharper aim."),
	    Monk, 0, 5, 1, Kind::Passive, SpellID::Invalid, true, 1 },
	// --- Way of the Body ---
	{ N_("Open Palm"), N_("An open-hand strike at +20% damage, +10% per rank, that drives the enemy back a tile."),
	    Monk, 1, 0, 0, Kind::Active, SpellID::OpenPalm, true, 5 },
	{ N_("Flowing Step"), N_("Move through battle with greater speed. One point makes you run rather than walk; the evade half needs an avoidance roll this engine has not got."),
	    Monk, 1, 1, 0, Kind::Passive, SpellID::Invalid, true, 5 },
	{ N_("Iron Robe"), N_("Discipline hardens your body while you wear light armour or none at all. Unarmoured: armour class by level, and blows land lighter. Light armour keeps 50% of it. Mail and plate switch it off."),
	    Monk, 1, 2, 0, Kind::Passive, SpellID::Invalid, true, 5 },
	{ N_("Counterstroke"), N_("A block empowers your next melee blow within 3 seconds: +30% damage, +10% per level."),
	    Monk, 1, 3, 0, Kind::Passive, SpellID::Invalid, true, 5 },
	{ N_("Purifying Breath"), N_("Centre yourself: +20 to every resistance, +5 per rank, for 30 seconds, +5 per rank."),
	    Monk, 1, 4, 0, Kind::Active, SpellID::PurifyingBreath, true, 5 },
	{ N_("Hundred Fists"), N_("Four blows in one swing, one more every two ranks up to seven, each at 50% damage."),
	    Monk, 1, 5, 0, Kind::Active, SpellID::HundredFists, true, 5 },
	{ N_("Perfect Vessel"), N_("Your mastery of the body empowers every Way of the Body skill: +10% life, and you shake off hits faster."),
	    Monk, 1, 5, 1, Kind::Passive, SpellID::Invalid, true, 1 },
	// --- Way of the Spirit ---
	{ N_("Inner Sight"), N_("Reveal nearby objects, traps and treasure. Deepens the Monk's own Search: every point holds the sight longer."),
	    Monk, 2, 0, 0, Kind::Active, SpellID::Search, true, 5 },
	{ N_("Healing Mantra"), N_("Restore life to yourself over time. Held like an aura rather than cast, so it mends you for as long as it plays."),
	    Monk, 2, 1, 0, Kind::Aura, SpellID::Invalid, true, 5 },
	{ N_("Temple Bell"), N_("A tone that strikes every undead in earshot for three to six a rank, staggers it and drives it back."),
	    Monk, 2, 2, 0, Kind::Active, SpellID::TempleBell, true, 5 },
	{ N_("Spirit Ward"), N_("Surround yourself with a barrier against magic. Rides this engine's Mana Shield, which drinks the blow into your mana; every point makes it drink deeper."),
	    Monk, 2, 3, 0, Kind::Active, SpellID::ManaShield, true, 5 },
	{ N_("Radiant Palm"), N_("A strike at +20% damage, +10% per rank; an enemy it kills erupts, dealing the blow again to everything beside it."),
	    Monk, 2, 4, 0, Kind::Active, SpellID::RadiantPalm, true, 5 },
	{ N_("Tranquility"), N_("A sanctuary about you for 13 seconds, +1 per rank: what stands beside you is slowed, and 2% of your life returns each second."),
	    Monk, 2, 5, 0, Kind::Active, SpellID::Tranquility, true, 5 },
	{ N_("Enlightenment"), N_("Your mastery of spirit empowers every Way of the Spirit skill: +10% mana, and +10 to every resistance."),
	    Monk, 2, 5, 1, Kind::Passive, SpellID::Invalid, true, 1 },
	// ---- Passive Skills (page 3) ----
	{ N_("Resolve"), N_("What you strike deals 20% less damage for 3 seconds."),
	    Monk, 3, 0, 0, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Fleet Footed"), N_("You run at all times."),
	    Monk, 3, 0, 1, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Exalted Soul"), N_("You hold twenty more mana."),
	    Monk, 3, 0, 2, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Transcendence"), N_("50% of every point of mana you spend returns as life."),
	    Monk, 3, 1, 0, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Chant of Resonance"), N_("Your mantras cost 50% less mana."),
	    Monk, 3, 1, 1, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Seize the Initiative"), N_("+30% damage against enemies still at full life."),
	    Monk, 3, 1, 2, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("The Guardian's Path"), N_("With a weapon in each hand, 15% of melee blows miss you; with a two-handed staff, you hold 20 more mana."),
	    Monk, 3, 2, 0, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Sixth Sense"), N_("Everything that is not steel - fire, lightning, magic - deals -25% damage to you."),
	    Monk, 3, 2, 1, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Determination"), N_("Every enemy pressing close gives you +5% damage, up to +20%."),
	    Monk, 3, 2, 2, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Relentless Assault"), N_("+30% damage against anything frozen or reeling."),
	    Monk, 3, 3, 0, Kind::Passive, SpellID::Invalid, true, 1 },
	// Replaced 2026-09-14 (user note) - Beacon of Ytar shortened cooldowns, and skills here have none.
	{ N_("Serene Mind"), N_("Stand still for a moment and 2% of your mana returns every second."),
	    Monk, 3, 3, 1, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Alacrity"), N_("You attack faster."),
	    Monk, 3, 3, 2, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Harmony"), N_("Every resistance fifteen points higher."),
	    Monk, 3, 4, 0, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Combination Strike"), N_("Every different melee skill you have struck with in the last 3 seconds adds +10% damage, up to +30%."),
	    Monk, 3, 4, 1, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Near Death Experience"), N_("Once a minute a killing blow restores 33% of your life and mana instead."),
	    Monk, 3, 4, 2, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Unity"), N_("Every ally of yours within 5 tiles lends +10% damage, up to +30%."),
	    Monk, 3, 5, 0, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Momentum"), N_("Keep moving for 2 seconds and your next 3 blows land +25% harder."),
	    Monk, 3, 5, 1, Kind::Passive, SpellID::Invalid, true, 1 },
	{ N_("Mythic Rhythm"), N_("Every third melee skill blow charges your spells: +40% spell damage for 3 seconds."),
	    Monk, 3, 5, 2, Kind::Passive, SpellID::Invalid, true, 1 },
	// ---- RfA-12 skills (2026-09-13): the empty cells of the three class pages, from the final list ----
	{ N_("Staff Parry"), N_("Holding a staff: +5% block chance, +1% per level, to 30%."),
	    Monk, 0, 0, 1, Kind::Passive, SpellID::Invalid, true },
	{ N_("Long Thrust"), N_("A thrust that reaches the first enemy within 2 tiles for 100% of a blow, +8% per level."),
	    Monk, 0, 0, 2, Kind::Active, SpellID::LongThrust, true },
	{ N_("Low Branch"), N_("A sweep at the legs that slows the target to half speed for 3 seconds without staggering it."),
	    Monk, 0, 1, 1, Kind::Active, SpellID::LowBranch, true },
	{ N_("Rearward Reach"), N_("Strike the enemy behind you with the staff's end, without turning, for 100% of a blow, +8% per level."),
	    Monk, 0, 1, 2, Kind::Active, SpellID::RearwardReach, true },
	{ N_("Mountain Pole"), N_("A ground slam: everything beside you takes 50% of a blow, +5% per level, and is stunned for 1 second."),
	    Monk, 0, 2, 1, Kind::Active, SpellID::MountainPole, true },
	{ N_("Bamboo Rain"), N_("A flurry at up to three enemies within 2 tiles, each for 70% of a blow, +5% per level."),
	    Monk, 0, 2, 2, Kind::Active, SpellID::BambooRain, true },
	{ N_("Dragon Tail Sweep"), N_("A low sweep that strikes everything beside you for 60% of a blow, +5% per level, and knocks it back."),
	    Monk, 0, 3, 1, Kind::Active, SpellID::DragonTailSweep, true },
	{ N_("Staff of Echoes"), N_("A blow that lands again a second later for 60% of its damage, +4% per level."),
	    Monk, 0, 3, 2, Kind::Active, SpellID::StaffOfEchoes, true },
	{ N_("River Stance"), N_("Holding a staff: +5% movement speed and +5% armour, +1% and +2% per level."),
	    Monk, 0, 4, 1, Kind::Passive, SpellID::Invalid, true },
	{ N_("Heaven Splitter"), N_("An overhead blow whose shockwave strikes everything on a 4-tile line for 120% of a blow, +10% per level."),
	    Monk, 0, 4, 2, Kind::Active, SpellID::HeavenSplitter, true },
	{ N_("Thousand Reeds"), N_("A flurry across every enemy within 6 tiles, each for 40% of a blow, +3% per level."),
	    Monk, 0, 5, 2, Kind::Active, SpellID::ThousandReeds, true },
	{ N_("Tiger Claw"), N_("Raking strikes at +10% damage, +3% per level, that make the target bleed for 3 seconds: 3 damage a second, +1 per level."),
	    Monk, 1, 0, 1, Kind::Active, SpellID::TigerClaw, true },
	{ N_("Deep Breath"), N_("Your life slowly regenerates."),
	    Monk, 1, 0, 2, Kind::Passive, SpellID::Invalid, true },
	{ N_("Leaping Crane"), N_("A flying kick up to 4 tiles to the cursor, a tile further every 4 levels to 7, striking the enemy you land beside for 80% of a blow, +5% per level."),
	    Monk, 1, 1, 1, Kind::Active, SpellID::LeapingCrane, true },
	{ N_("Pressure Point"), N_("A strike that slows the target to half speed for 3 seconds and strips 20% of its armour for 6 seconds, +2% per level to 60%."),
	    Monk, 1, 1, 2, Kind::Active, SpellID::PressurePoint, true },
	{ N_("Iron Fist"), N_("Unarmed or holding a staff: +2 damage to every blow, +1 per level."),
	    Monk, 1, 2, 1, Kind::Passive, SpellID::Invalid, true },
	{ N_("Whirling Kick"), N_("A spinning kick that strikes everything beside you for 70% of a blow, +5% per level, and knocks it back."),
	    Monk, 1, 2, 2, Kind::Active, SpellID::WhirlingKick, true },
	{ N_("Shoulder Gate"), N_("Rush up to 2 tiles toward the cursor and stop the enemy you meet dead for 1 second."),
	    Monk, 1, 3, 1, Kind::Active, SpellID::ShoulderGate, true },
	{ N_("Seven-Sided Strike"), N_("Strike up to three enemies within 4 tiles of the cursor, one more every 3 levels to seven, each for 60% of a blow, +4% per level."),
	    Monk, 1, 3, 2, Kind::Active, SpellID::SevenSidedStrike, true },
	{ N_("Mountain Stance"), N_("Every blow you take deals 1 less damage, +1 per level, to 20."),
	    Monk, 1, 4, 1, Kind::Passive, SpellID::Invalid, true },
	{ N_("Exploding Palm"), N_("A strike at +10% damage, +3% per level, that makes the target bleed for 6 seconds; if it dies meanwhile it bursts for 50% of a blow on everything beside it, +5% per level."),
	    Monk, 1, 4, 2, Kind::Active, SpellID::ExplodingPalm, true },
	{ N_("Dragon's Wrath"), N_("A wave of force down an 8-tile line, striking everything on it for 100% of a blow, +8% per level."),
	    Monk, 1, 5, 2, Kind::Active, SpellID::DragonsWrath, true },
	{ N_("Mantra of Clarity"), N_("For 30 seconds your mana flows back faster."),
	    Monk, 2, 0, 1, Kind::Active, SpellID::MantraOfClarity, true },
	{ N_("Mantra of Evasion"), N_("For 30 seconds, 10% of melee blows miss you, +2% per level to 40%."),
	    Monk, 2, 0, 2, Kind::Active, SpellID::MantraOfEvasion, true },
	{ N_("Chi Wave"), N_("A wave that bounces through up to four enemies near the cursor, each taking 4-8 magic damage, +2-3 per level."),
	    Monk, 2, 1, 1, Kind::Active, SpellID::ChiWave, true },
	{ N_("Blinding Flash"), N_("Enemies within 3 tiles are blinded and wander off. Uniques shrug it off."),
	    Monk, 2, 1, 2, Kind::Active, SpellID::BlindingFlash, true },
	{ N_("Mantra of Retribution"), N_("For 30 seconds, every enemy that strikes you in melee takes 3-6 magic damage, +1-2 per level."),
	    Monk, 2, 2, 1, Kind::Active, SpellID::MantraOfRetribution, true },
	{ N_("Serenity"), N_("Every slow and chill on you ends at once."),
	    Monk, 2, 2, 2, Kind::Active, SpellID::Serenity, true },
	{ N_("Spirit Guardian"), N_("A spirit guardian stands at your side for 30 seconds, +5 per level. It strikes for 30% of your damage, +3% per level, holds the enemies that come near it, and every 8 seconds taunts all around it."),
	    Monk, 2, 3, 1, Kind::Active, SpellID::SpiritGuardian, true },
	{ N_("Wave of Light"), N_("A spectral bell crashes down on the cursor: 7-14 magic damage within 2 tiles, +3-5 per level."),
	    Monk, 2, 3, 2, Kind::Active, SpellID::WaveOfLight, true },
	{ N_("Inner Fire"), N_("Your blows carry 1-3 fire damage, +1-2 per level."),
	    Monk, 2, 4, 1, Kind::Passive, SpellID::Invalid, true },
	{ N_("Astral Projection"), N_("For 6 seconds: +50% movement speed, and monsters that have not seen you do not notice you."),
	    Monk, 2, 4, 2, Kind::Active, SpellID::AstralProjection, true },
	{ N_("Ancestral Court"), N_("Three ancestral shades gather at the cursor and strike inward after a second: three strikes of 5-9 magic damage within 2 tiles, +2-4 per level."),
	    Monk, 2, 5, 2, Kind::Active, SpellID::AncestralCourt, true },
	// ======================= NECROMANCER =======================
	// Phase N1 of "Plan - The Necromancer" (2026-09-17): the rows exist so the pages can be read and edited; every one
	// is inert and says what it is waiting for ("Not yet built" is the phrase the tests hold inert rows to). Essence-priced rows say so in words until the pay path exists (N2).
	// --- Summoning --- (built at N5, 2026-09-18: oracool/necro_summoning) ---
	{ N_("Raise Skeleton"), N_("Raise a skeleton warrior from a corpse to fight for you. One skeleton at rank 1, one more every three ranks, eight at most."),
	    Nec, 0, 0, 0, Kind::Active, SpellID::RaiseSkeleton, true },
	{ N_("Skeleton Mastery"), N_("Your skeletons and skeletal mages gain life and damage with every rank. Applies to the next ones you raise."),
	    Nec, 0, 0, 1, Kind::Passive, SpellID::Invalid, true },
	{ N_("Command the Dead"), N_("Point at an enemy and every minion you own turns on it."),
	    Nec, 0, 0, 2, Kind::Active, SpellID::CommandTheDead, true },
	{ N_("Clay Golem"), N_("Shape a golem of clay: slow, tough, and its blows slow what it strikes. You keep one golem of any kind."),
	    Nec, 0, 1, 0, Kind::Active, SpellID::ClayGolem, true },
	{ N_("Golem Mastery"), N_("Your golem gains life and chance to hit with every rank. Applies to the next one you shape."),
	    Nec, 0, 1, 1, Kind::Passive, SpellID::Invalid, true },
	{ N_("Gather the Dead"), N_("Call every minion to your side at once, through walls and across the floor."),
	    Nec, 0, 1, 2, Kind::Active, SpellID::GatherTheDead, true },
	{ N_("Raise Skeletal Mage"), N_("Raise a skeletal mage from a corpse; it throws fire, lightning, poison or bone from behind your line. One at rank 1, one more every three ranks, eight at most."),
	    Nec, 0, 2, 0, Kind::Active, SpellID::RaiseSkeletalMage, true },
	{ N_("Summon Resist"), N_("Everything you have raised resists fire, lightning and magic, more with every rank."),
	    Nec, 0, 2, 1, Kind::Passive, SpellID::Invalid, true },
	{ N_("Dark Mending"), N_("Knit your minions back together: every minion near you is healed at once."),
	    Nec, 0, 2, 2, Kind::Active, SpellID::DarkMending, true },
	{ N_("Blood Golem"), N_("A golem of blood bound to your own life: what it takes from its enemies heals you both."),
	    Nec, 0, 3, 0, Kind::Active, SpellID::BloodGolem, true },
	{ N_("Bone Plating"), N_("Your minions wear bone: armour for every one of them, more with every rank. Applies to the next ones you raise."),
	    Nec, 0, 3, 1, Kind::Passive, SpellID::Invalid, true },
	{ N_("Frenzy of the Dead"), N_("For ten seconds every minion strikes harder and hurries after its prey."),
	    Nec, 0, 3, 2, Kind::Active, SpellID::FrenzyOfTheDead, true },
	{ N_("Iron Golem"), N_("A golem of iron that returns a share of every blow it takes to the one that struck it."),
	    Nec, 0, 4, 0, Kind::Active, SpellID::IronGolem, true },
	{ N_("Lasting Bond"), N_("The Revived serve longer before they fall apart, more with every rank."),
	    Nec, 0, 4, 1, Kind::Passive, SpellID::Invalid, true },
	{ N_("Unholy Offering"), N_("Unmake one of your minions to heal yourself for a share of its life."),
	    Nec, 0, 4, 2, Kind::Active, SpellID::UnholyOffering, true },
	{ N_("Fire Golem"), N_("A golem of fire: it burns what stands near it and is healed by fire."),
	    Nec, 0, 5, 0, Kind::Active, SpellID::FireGolem, true },
	{ N_("Revive"), N_("Return a dead monster to life to fight for you as it was, for three minutes. One at rank 1, ten at most. Paid in Essence."),
	    Nec, 0, 5, 1, Kind::Active, SpellID::NecroRevive, true },
	{ N_("Army of the Dead"), N_("The dead erupt at the cursor and tear at everything within two tiles, six times over three seconds."),
	    Nec, 0, 5, 2, Kind::Active, SpellID::ArmyOfTheDead, true },
	// --- Poison & Bone --- (built at N6, 2026-09-18: rfa12_actives, the bone and poison cases) ---
	{ N_("Teeth"), N_("A fan of barbed teeth, magic damage, one more tooth with every rank."),
	    Nec, 1, 0, 0, Kind::Active, SpellID::Teeth, true },
	{ N_("Bone Armor"), N_("A shell of bone that absorbs damage until it is spent, or for a minute."),
	    Nec, 1, 0, 1, Kind::Active, SpellID::BoneArmor, true },
	{ N_("Poison Dagger"), N_("For twenty seconds every blow you land with a weapon poisons what it strikes."),
	    Nec, 1, 0, 2, Kind::Active, SpellID::PoisonDagger, true },
	{ N_("Corpse Explosion"), N_("Burst a corpse: everything near it takes a share of the dead monster's life as damage. Paid in Essence."),
	    Nec, 1, 1, 0, Kind::Active, SpellID::CorpseExplosion, true },
	{ N_("Bone Splinters"), N_("Three quick splinters of bone in a narrow spread."),
	    Nec, 1, 1, 1, Kind::Active, SpellID::BoneSplinters, true },
	{ N_("Blight"), N_("A bolt that bursts into a pool of poison where it lands."),
	    Nec, 1, 1, 2, Kind::Active, SpellID::Blight, true },
	{ N_("Bone Wall"), N_("Raise a line of bone across the cursor for eight seconds. A monster that steps into it is cut and thrown back."),
	    Nec, 1, 2, 0, Kind::Active, SpellID::BoneWall, true },
	{ N_("Bone Spikes"), N_("Spikes erupt under the cursor, magic damage, and hold what they hit for a moment."),
	    Nec, 1, 2, 1, Kind::Active, SpellID::BoneSpikes, true },
	{ N_("Poison Explosion"), N_("Burst a corpse into a cloud of poison. Paid in Essence."),
	    Nec, 1, 2, 2, Kind::Active, SpellID::PoisonExplosion, true },
	{ N_("Bone Spear"), N_("A spear of bone that passes through everything in its line, magic damage."),
	    Nec, 1, 3, 0, Kind::Active, SpellID::BoneSpear, true },
	{ N_("Decompose"), N_("Rot an enemy where it stands: heavy poison for five seconds."),
	    Nec, 1, 3, 1, Kind::Active, SpellID::Decompose, true },
	{ N_("Marrow"), N_("Every bone skill deals more damage, more with every rank."),
	    Nec, 1, 3, 2, Kind::Passive, SpellID::Invalid, true },
	{ N_("Bone Prison"), N_("Bones close around the target and hold it for three seconds, cutting it as it struggles."),
	    Nec, 1, 4, 0, Kind::Active, SpellID::BonePrison, true },
	{ N_("Bone Storm"), N_("For eight seconds a storm of bone shards follows you and cuts everything within two tiles, twice a second."),
	    Nec, 1, 4, 1, Kind::Active, SpellID::BoneStorm, true },
	{ N_("Virulence"), N_("Your poisons last longer and bite deeper, more with every rank."),
	    Nec, 1, 4, 2, Kind::Passive, SpellID::Invalid, true },
	{ N_("Bone Spirit"), N_("A spirit of bone that hunts the nearest enemy and strikes it for heavy magic damage."),
	    Nec, 1, 5, 0, Kind::Active, SpellID::NecroBoneSpirit, true },
	{ N_("Poison Nova"), N_("A ring of poison bolts in every direction."),
	    Nec, 1, 5, 1, Kind::Active, SpellID::PoisonNova, true },
	{ N_("Death Nova"), N_("A burst of bone and blight around you: magic damage now, poison after."),
	    Nec, 1, 5, 2, Kind::Active, SpellID::DeathNova, true },
	// --- Curses --- (built at N7, 2026-09-18: oracool/curses) ---
	{ N_("Amplify Damage"), N_("Cursed monsters take half again as much physical damage, more with every rank. One curse to a monster; a new one replaces the old."),
	    Nec, 2, 0, 0, Kind::Active, SpellID::AmplifyDamage, true },
	{ N_("Curse Mastery"), N_("Your curses last longer, more with every rank."),
	    Nec, 2, 0, 1, Kind::Passive, SpellID::Invalid, true },
	{ N_("Essence Tap"), N_("A cursed monster that dies returns Essence to you, more with every rank."),
	    Nec, 2, 0, 2, Kind::Passive, SpellID::Invalid, true },
	{ N_("Dim Vision"), N_("Cursed monsters cannot see you until you are beside them. Paid in Essence."),
	    Nec, 2, 1, 0, Kind::Active, SpellID::DimVision, true },
	{ N_("Weaken"), N_("Cursed monsters deal a third less damage. Paid in Essence."),
	    Nec, 2, 1, 1, Kind::Active, SpellID::NecroWeaken, true },
	{ N_("Frailty"), N_("A cursed monster that falls below a tenth of its life, a little more each rank, simply dies. Uniques do not."),
	    Nec, 2, 1, 2, Kind::Active, SpellID::Frailty, true },
	{ N_("Iron Maiden"), N_("A cursed monster takes back every blow it lands on you or your minions, and more with every rank."),
	    Nec, 2, 2, 0, Kind::Active, SpellID::NecroIronMaiden, true },
	{ N_("Terror"), N_("Cursed monsters run from you while the curse holds. Uniques do not."),
	    Nec, 2, 2, 1, Kind::Active, SpellID::Terror, true },
	{ N_("Bane"), N_("Cursed monsters rot: poison damage for as long as the curse holds. Paid in Essence."),
	    Nec, 2, 2, 2, Kind::Active, SpellID::Bane, true },
	{ N_("Confuse"), N_("Cursed monsters turn on whatever is nearest, friend or foe, while the curse holds. Uniques do not."),
	    Nec, 2, 3, 0, Kind::Active, SpellID::Confuse, true },
	{ N_("Life Tap"), N_("Blows landed on a cursed monster heal the one who struck - you and your minions alike. Paid in Essence."),
	    Nec, 2, 3, 1, Kind::Active, SpellID::LifeTap, true },
	{ N_("Wide Malice"), N_("Your curses cover more ground, more with every rank."),
	    Nec, 2, 3, 2, Kind::Passive, SpellID::Invalid, true },
	{ N_("Attract"), N_("The cursed monster becomes the target of every monster near it. Paid in Essence."),
	    Nec, 2, 4, 0, Kind::Active, SpellID::Attract, true },
	{ N_("Decrepify"), N_("Cursed monsters are slowed, deal a quarter less damage and take a fifth more."),
	    Nec, 2, 4, 1, Kind::Active, SpellID::Decrepify, true },
	{ N_("Death Mark"), N_("Mark one monster: when it dies, it bursts as a Corpse Explosion of your rank. Paid in Essence."),
	    Nec, 2, 4, 2, Kind::Active, SpellID::DeathMark, true },
	{ N_("Lower Resist"), N_("Cursed monsters lose resistance to fire, lightning, magic and poison. Paid in Essence."),
	    Nec, 2, 5, 0, Kind::Active, SpellID::LowerResist, true },
	{ N_("Soul Harvest"), N_("Tear at every cursed monster within six tiles: magic damage to each, and five Essence to you for each."),
	    Nec, 2, 5, 1, Kind::Active, SpellID::SoulHarvest, true },
	{ N_("Doom"), N_("Cursed monsters take more damage from every source, and the curse cannot be replaced by a weaker one. Paid in Essence."),
	    Nec, 2, 5, 2, Kind::Active, SpellID::Doom, true },
	// --- Passive Skills ---
	{ N_("Life from Death"), N_("Monsters that die near you may leave a health globe. Not yet built: awaits the system it modifies."),
	    Nec, 3, 0, 0, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Fueled by Death"), N_("Each corpse you consume quickens your step for a few seconds. Not yet built: awaits the system it modifies."),
	    Nec, 3, 0, 1, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Stand Alone"), N_("More armour while you have no minions, less for each one you keep. Not yet built: awaits the system it modifies."),
	    Nec, 3, 0, 2, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Swift Harvesting"), N_("Faster attacks with a scythe or wand. Not yet built: awaits the system it modifies."),
	    Nec, 3, 1, 0, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Commander of the Risen Dead"), N_("Raising skeletons and mages costs less mana. Not yet built: awaits the system it modifies."),
	    Nec, 3, 1, 1, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Extended Servitude"), N_("Timed minions last a quarter longer. Not yet built: awaits the system it modifies."),
	    Nec, 3, 1, 2, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Rigor Mortis"), N_("Your bone skills slow what they hit. Not yet built: awaits the system it modifies."),
	    Nec, 3, 2, 0, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Overwhelming Essence"), N_("Your Essence pool is larger by a fifth. Not yet built: awaits the system it modifies."),
	    Nec, 3, 2, 1, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Dark Reaping"), N_("Your blows return a little Essence and a little mana. Not yet built: awaits the system it modifies."),
	    Nec, 3, 2, 2, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Spreading Malediction"), N_("You deal more damage for each cursed monster near you. Not yet built: awaits the system it modifies."),
	    Nec, 3, 3, 0, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Eternal Torment"), N_("Your curses never run out; they end when the monster does. Not yet built: awaits the system it modifies."),
	    Nec, 3, 3, 1, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Final Service"), N_("A blow that would kill you unmakes your minions instead, once a floor. Not yet built: awaits the system it modifies."),
	    Nec, 3, 3, 2, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Grisly Tribute"), N_("A share of the damage your minions deal heals you. Not yet built: awaits the system it modifies."),
	    Nec, 3, 4, 0, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Draw Life"), N_("You regenerate life faster for each monster near you. Not yet built: awaits the system it modifies."),
	    Nec, 3, 4, 1, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Serration"), N_("Your bone skills deal more damage the farther they have flown. Not yet built: awaits the system it modifies."),
	    Nec, 3, 4, 2, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Aberrant Animator"), N_("Your minions return a share of the blows they take. Not yet built: awaits the system it modifies."),
	    Nec, 3, 5, 0, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Blood is Power"), N_("Losing life shortens the wait on your skills. Not yet built: awaits the system it modifies."),
	    Nec, 3, 5, 1, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Rathma's Shield"), N_("When your life falls low, nothing can harm you for a few seconds. Once a floor. Not yet built: awaits the system it modifies."),
	    Nec, 3, 5, 2, Kind::Passive, SpellID::Invalid, false, 1 },
};

/** @brief The first skill of @p heroClass's block, or None if the class has no tree. */
Skill FirstSkillOf(HeroClass heroClass)
{
	switch (heroClass) {
	case HeroClass::Warrior:
		return Skill::PALADIN_FIRST;
	case HeroClass::Barbarian:
		return Skill::BARBARIAN_FIRST;
	case HeroClass::Sorcerer:
		return Skill::SORCERER_FIRST;
	case HeroClass::Rogue:
		return Skill::ROGUE_FIRST;
	case HeroClass::Bard:
		return Skill::BARD_FIRST;
	case HeroClass::Monk:
		return Skill::MONK_FIRST;
	case HeroClass::Necromancer:
		return Skill::NECROMANCER_FIRST;
	default:
		return Skill::None;
	}
}

/**
 * @brief The shape every scaled effect uses: the first point buys @p base, each one after adds
 * @p perPoint. Zero for nothing invested, which is what keeps an unpaid skill inert.
 */
int Scaled(int points, int base, int perPoint)
{
	if (points <= 0)
		return 0;
	return base + perPoint * (points - 1);
}

/** @brief Whether @p player is wielding @p type in either hand, for the Barbarian's masteries. */
bool WieldingType(const Player &player, ItemType type)
{
	for (const Item &item : { player.InvBody[INVLOC_HAND_LEFT], player.InvBody[INVLOC_HAND_RIGHT] }) {
		if (!item.isEmpty() && item._iStatFlag && item._itype == type)
			return true;
	}
	return false;
}

/** @brief Whether @p player holds a spear or a pike - the two RfA-12 polearm bases. For Brace. */
bool WieldingSpearOrPike(const Player &player)
{
	for (const Item &item : { player.InvBody[INVLOC_HAND_LEFT], player.InvBody[INVLOC_HAND_RIGHT] }) {
		if (item.isEmpty() || !item._iStatFlag || item.IDidx < 0 || item.IDidx > IDI_LAST)
			continue;
		const unique_base_item base = AllItemsList[static_cast<size_t>(item.IDidx)].iItemId;
		if (base == UITYPE_SPEAR || base == UITYPE_PIKE)
			return true;
	}
	return false;
}

/** @brief Whether @p player's hands hold no weapon. For Iron Fist. */
bool WieldingNoWeapon(const Player &player)
{
	for (const Item &item : { player.InvBody[INVLOC_HAND_LEFT], player.InvBody[INVLOC_HAND_RIGHT] }) {
		if (!item.isEmpty() && item._iStatFlag && item._iClass == ICLASS_WEAPON)
			return false;
	}
	return true;
}

/** @brief Applies one paid-for passive. Auras go through ApplyAura below. */
void ApplyPassive(const Player &player, Skill skill, int points, ItemBonusTotals &totals)
{
	switch (skill) {
	// --- Barbarian masteries: only while the matching weapon is actually held, which is the
	//     whole point of a mastery and the reason the provider has a condition hook.
	case Skill::SwordMastery:
		if (WieldingType(player, ItemType::Sword)) {
			totals.bonusToHit += Scaled(points, 10, 5);
			totals.bonusDamage += Scaled(points, 10, 6);
		}
		break;
	case Skill::AxeMastery:
		if (WieldingType(player, ItemType::Axe)) {
			totals.bonusToHit += Scaled(points, 10, 5);
			totals.bonusDamage += Scaled(points, 10, 6);
		}
		break;
	case Skill::MaceMastery:
		if (WieldingType(player, ItemType::Mace)) {
			totals.bonusToHit += Scaled(points, 10, 5);
			totals.bonusDamage += Scaled(points, 10, 6);
		}
		break;
	case Skill::PoleArmMastery:
		if (WieldingType(player, ItemType::Staff)) {
			totals.bonusToHit += Scaled(points, 10, 5);
			totals.bonusDamage += Scaled(points, 10, 6);
		}
		break;
	case Skill::SpearMastery:
		if (WieldingSpearOrPike(player)) {
			totals.bonusToHit += Scaled(points, 10, 5);
			totals.bonusDamage += Scaled(points, 10, 6);
		}
		break;
	// Weapons Master (2026-09-14): each family its own gift. The mace's - Rage per landed blow - is a
	// rule, not a number, and lives in passives.cpp's OnPassiveHit.
	case Skill::WeaponsMaster:
		if (WieldingType(player, ItemType::Sword))
			totals.bonusDamage += 15;
		if (WieldingType(player, ItemType::Axe))
			totals.bonusToHit += 15;
		if (WieldingType(player, ItemType::Staff))
			totals.flags |= ItemSpecialEffect::FastAttack;
		break;
	// --- Round 5 (2026-09-03): the passives that are a NUMBER on the sheet. The ones that are a
	//     rule - a chance, a condition read at the moment of a blow - live in oracool/passives.cpp.
	//     Passive Skills page rows are one rank, so no Scaled(): the number is the number.
	case Skill::DivineFortress:
		if (WieldingType(player, ItemType::Shield))
			totals.bonusArmor += 25;
		break;
	case Skill::ToughAsNails:
		totals.bonusArmor += 25;
		break;
	case Skill::Perfectionist:
		totals.bonusArmor += 10;
		totals.fireResist += 10;
		totals.lightningResist += 10;
		totals.magicResist += 10;
		break;
	case Skill::MonkHarmony:
		totals.fireResist += 15;
		totals.lightningResist += 15;
		totals.magicResist += 15;
		break;
	case Skill::Superstition:
		totals.fireResist += 10;
		totals.lightningResist += 10;
		totals.magicResist += 10;
		break;
	case Skill::GlassCannon:
		totals.bonusDamage += 15;
		totals.bonusArmor -= 10;
		break;
	case Skill::HolyCause:
		totals.bonusDamage += 10;
		break;
	case Skill::Animosity:
		// Twenty more RAGE, not mana (2026-09-13) - MaxRage asks for this passive itself.
		break;
	case Skill::AstralPresence:
	case Skill::ExaltedSoul:
	case Skill::Righteousness:
	case Skill::BloodVengeance:
		// The engine's mana IS this fork's fury, arcane power, wrath, hatred and spirit; twenty points
		// of it - in the 1/64 units mana is kept in (2026-09-14: this was a bare 20, a third of a point).
		totals.mana += 20 << 6;
		break;
	case Skill::Toughness:
		totals.vitality += Scaled(points, 5, 2);
		break;
	// ---- the all-heroes sweep (2026-09-14): the sheet halves of the new passives ----
	case Skill::TheGuardiansPath:
		for (const Item &item : { player.InvBody[INVLOC_HAND_LEFT], player.InvBody[INVLOC_HAND_RIGHT] }) {
			if (!item.isEmpty() && item._iStatFlag && item._itype == ItemType::Staff && item._iLoc == ILOC_TWOHAND) {
				totals.mana += 20 << 6;
				break;
			}
		}
		break;
	case Skill::Finery:
		for (const Item &item : player.InvBody) {
			if (item.isEmpty() || !item._iStatFlag)
				continue;
			for (int s = 0; s < std::min<int>(item._iSocketCount, Item::MaxItemSockets); s++) {
				if (item._iSocketed[s] != Item::EmptySocket && IsOracoolGemIdx(item._iSocketed[s]))
					totals.strength += 2;
			}
		}
		break;
	case Skill::Archery:
		for (const Item &item : { player.InvBody[INVLOC_HAND_LEFT], player.InvBody[INVLOC_HAND_RIGHT] }) {
			if (item.isEmpty() || !item._iStatFlag || item._itype != ItemType::Bow || item.IDidx < 0 || item.IDidx > IDI_LAST)
				continue;
			const unique_base_item bow = AllItemsList[static_cast<size_t>(item.IDidx)].iItemId;
			if (bow == UITYPE_SHORTBOW || bow == UITYPE_HUNTBOW)
				totals.bonusToHit += 15;
			if (bow == UITYPE_LONGBOW || bow == UITYPE_WARBOW)
				totals.bonusDamage += 15;
			break; // the composite and battle bows' gift is a rule - passives.cpp
		}
		break;
	case Skill::Rhythm:
		// ToggleClassAura recalculates the sheet, so a song starting or stopping takes this with it.
		if (GetActiveClassAura(player) != Skill::None)
			totals.flags |= ItemSpecialEffect::FastAttack;
		break;
	case Skill::Alacrity:
		totals.flags |= ItemSpecialEffect::FastAttack;
		break;
	case Skill::CrusaderFanaticism:
		totals.flags |= ItemSpecialEffect::FastAttack;
		break;
	case Skill::Fervor:
		// One-handed: a weapon in hand that is not a two-hander.
		for (const Item &item : { player.InvBody[INVLOC_HAND_LEFT], player.InvBody[INVLOC_HAND_RIGHT] }) {
			if (!item.isEmpty() && item._iStatFlag && item._iClass == ICLASS_WEAPON && item._iLoc == ILOC_ONEHAND) {
				totals.flags |= ItemSpecialEffect::QuickAttack;
				break;
			}
		}
		break;
	case Skill::IronSkin:
		totals.bonusArmor += Scaled(points, 20, 10);
		break;
	case Skill::NaturalResistance:
		totals.fireResist += Scaled(points, 8, 3);
		totals.lightningResist += Scaled(points, 8, 3);
		totals.magicResist += Scaled(points, 8, 3);
		break;
	case Skill::CriticalStrike:
		// D2 rolls a chance to double the blow; this engine has no critical roll, so the expected
		// value is spent as flat damage instead - stated in the row's own description.
		totals.bonusDamage += Scaled(points, 12, 6);
		break;
	case Skill::Penetrate:
		totals.bonusToHit += Scaled(points, 12, 6);
		break;

	// --- Sorceress: the three rows the engine can actually pay (2026-08-21) -------------------
	//
	// She was left with ONE working row when the books rule retired her thirteen castables, so her
	// page was a grid of struck-out cells with nowhere to put a point. These three are the only
	// others this engine has a channel for, and each is fitted to a real one rather than
	// approximated into the nearest spell.
	//
	// The ten cold rows stay inert and always will: there is no cold damage type at all, so there
	// is nothing to scale, resist or pierce. Static Field, Lightning Storm, Meteor and Enchant's
	// two neighbours stay inert for the same honest reason - no analogue exists.
	case Skill::Enchant:
		// The closest thing in the game to what Enchant IS: your weapon burns. D2's version is a
		// cast buff with a duration; here the investment is the buff, which is the only shape a
		// slotless tree skill can take. No approximation in the effect itself - fireMin/fireMax are
		// exactly "this attack also does fire damage".
		totals.fireMin += Scaled(points, 2, 1);
		totals.fireMax += Scaled(points, 5, 3);
		break;
	case Skill::FireMastery:
		// NOT "deepens every fire spell" - this engine scales spells by spell level, and there is no
		// per-element channel to raise. Mastery is read as command OF fire instead: it burns for you
		// and less against you. Both halves are real channels, and the row's description says so
		// rather than letting a player infer the D2 meaning.
		totals.fireMin += Scaled(points, 3, 2);
		totals.fireMax += Scaled(points, 7, 4);
		totals.fireResist += Scaled(points, 5, 2);
		break;
	case Skill::LightningMastery:
		// The mirror of Fire Mastery, and deliberately the same shape so the two read as a pair.
		// Lightning's spread is wider than fire's in this engine (see the Topaz gem and the Ort
		// rune, both 1..N rather than a tight band), so the minimum stays low and the maximum runs.
		totals.lightningMin += Scaled(points, 1, 1);
		totals.lightningMax += Scaled(points, 10, 6);
		totals.lightningResist += Scaled(points, 5, 2);
		break;

	// --- Monk. The capstones are one-rank rows, so they take no per-point step at all; the
	//     numbers below are the design doc's, mapped onto the channels this engine actually has.
	case Skill::MasterOfTheLongStaff:
		// A mastery, so it wants the weapon in hand. The engine files quarterstaves under
		// ItemType::Staff, which is the type the Barbarian's Pole Arm mastery already tests.
		// bonusDamage IS a percentage of weapon damage, so +10% is the doc's number exactly;
		// bonusToHit is a flat attack-rating add, which is the nearest thing to its +15%.
		if (WieldingType(player, ItemType::Staff)) {
			totals.bonusDamage += 10;
			totals.bonusToHit += 15;
		}
		break;
	case Skill::IronRobe:
		// Discipline in place of plate. Medium and heavy armour switch it off entirely; light
		// armour keeps half the damage reduction but not the armour class, exactly as the doc has
		// it - the Monk's level-based AC is innate to the class and does not need buying twice.
		{
			const Item &chest = player.InvBody[INVLOC_CHEST];
			const bool unarmoured = chest.isEmpty();
			const bool light = !chest.isEmpty() && chest._iStatFlag && chest._itype == ItemType::LightArmor;
			if (unarmoured || light) {
				// getHit is a flat subtraction from damage taken, not a percentage, so the doc's
				// "3% per rank capped at 20%" is spent as points off each blow instead.
				const int reduction = std::min(3 * points, 20);
				totals.getHit -= unarmoured ? reduction : reduction / 2;
				if (unarmoured)
					totals.bonusArmor += 2 * player._pLevel;
			}
		}
		break;
	case Skill::PerfectVessel:
		// A true tenth of the character's own life: _pMaxHPBase is the level-and-vitality base
		// that totals.hitPoints is added TO, so reading it here is not circular.
		totals.hitPoints += player._pMaxHPBase / 10;
		totals.flags |= ItemSpecialEffect::FastHitRecovery;
		break;
	case Skill::Enlightenment:
		totals.mana += player._pMaxManaBase / 10;
		totals.fireResist += 10;
		totals.lightningResist += 10;
		totals.magicResist += 10;
		break;
	// --- RfA-12 (2026-09-13): the new passives that are a number on the sheet. The rules - bleeding,
	//     blocking, reach, noticing - are oracool/rfa12_effects.cpp's.
	case Skill::Swiftness:
		totals.moveSpeed += std::min(Scaled(points, 5, 1), 35);
		break;
	case Skill::Sharpen:
		totals.damageMod += Scaled(points, 2, 1);
		break;
	case Skill::Brace:
		if (WieldingSpearOrPike(player))
			totals.bonusArmor += Scaled(points, 10, 3);
		break;
	case Skill::RiverStance:
		if (WieldingType(player, ItemType::Staff)) {
			totals.moveSpeed += Scaled(points, 5, 1);
			totals.bonusArmor += Scaled(points, 5, 2);
		}
		break;
	case Skill::IronFist:
		if (WieldingNoWeapon(player) || WieldingType(player, ItemType::Staff))
			totals.damageMod += Scaled(points, 2, 1);
		break;
	case Skill::MountainStance:
		totals.getHit -= std::min(Scaled(points, 1, 1), 20);
		break;
	case Skill::InnerFire:
		totals.fireMin += Scaled(points, 1, 1);
		totals.fireMax += Scaled(points, 3, 2);
		break;
	default:
		// Increased Speed, Warmth and Flowing Step act elsewhere (the walk animation and the
		// per-tick hook); everything else on a passive row is inert and says so.
		break;
	}
}

/** @brief Applies the Paladin's burning aura. */
void ApplyAura(Skill aura, int p, ItemBonusTotals &totals)
{
	switch (aura) {
	case Skill::Might:
		totals.bonusDamage += Scaled(p, 20, 10);
		break;
	case Skill::HolyFire:
	case Skill::HolyShock:
	case Skill::Thorns:
		// Off the sheet since 2026-09-12. Holy Fire and Holy Shock (and Holy Freeze) are pulses on the
		// monsters around you - aura_field.cpp's ProcessOutwardAura - and Thorns returns a share of every
		// blow taken, read where the blow lands (ThornsReturnPercent). Nothing here, so the tooltip cannot
		// quote weapon damage the aura no longer adds.
		break;
	case Skill::BlessedAim:
		totals.bonusToHit += Scaled(p, 15, 7);
		break;
	case Skill::Concentration:
		totals.bonusDamage += Scaled(p, 15, 8);
		totals.flags |= ItemSpecialEffect::FastestHitRecovery;
		break;
	case Skill::Fanaticism:
		totals.flags |= ItemSpecialEffect::FastAttack;
		totals.bonusToHit += Scaled(p, 10, 5);
		totals.bonusDamage += Scaled(p, 10, 5);
		break;
	case Skill::ResistFire:
		totals.fireResist += Scaled(p, 15, 4);
		break;
	case Skill::Defiance:
		totals.bonusArmor += Scaled(p, 25, 12);
		break;
	case Skill::ResistCold:
		totals.magicResist += Scaled(p, 15, 4);
		break;
	case Skill::ResistLightning:
		totals.lightningResist += Scaled(p, 15, 4);
		break;
	case Skill::Salvation:
		totals.fireResist += Scaled(p, 10, 3);
		totals.lightningResist += Scaled(p, 10, 3);
		totals.magicResist += Scaled(p, 10, 3);
		break;
	// --- the Bard's songs. One plays at a time, which is what a bard does and what the aura
	//     machinery already enforces.
	case Skill::BattleHymn:
		totals.bonusToHit += Scaled(p, 12, 6);
		totals.bonusDamage += Scaled(p, 12, 6);
		break;
	case Skill::Vigor:
		// A percentage per rank since 2026-09-07 (user: "vigor should make it clear in the description how
		// many % it increases movement with each level"). It rode the binary run frame skip before; now
		// it is a number on the sheet that item affixes add to, and WalkFrameSkipFor turns the total
		// into strides - every percent of it since 2026-09-12 (StrideTicksFor), +5 a level.
		totals.moveSpeed += p * VigorMoveSpeedPerRank;
		break;
	case Skill::SongOfSwiftness:
		// The stride half is the run frame skip - see IsClassTreeRunActive.
		totals.flags |= ItemSpecialEffect::FastAttack;
		break;
	case Skill::SongOfFortitude:
		totals.bonusArmor += Scaled(p, 20, 10);
		totals.fireResist += Scaled(p, 8, 3);
		totals.lightningResist += Scaled(p, 8, 3);
		totals.magicResist += Scaled(p, 8, 3);
		break;
	case Skill::TaleOfHeroes:
		totals.strength += Scaled(p, 4, 2);
		totals.dexterity += Scaled(p, 4, 2);
		break;
	// --- RfA-12 (2026-09-13): the new auras and songs that are a number on the sheet. What they do to
	//     monsters, and the regeneration, is oracool/rfa12_effects.cpp's.
	case Skill::Valor:
		totals.damageMod += Scaled(p, 3, 2);
		break;
	case Skill::Steadfast:
		totals.flags |= p >= 10 ? ItemSpecialEffect::FastestHitRecovery : ItemSpecialEffect::FasterHitRecovery;
		break;
	case Skill::ResistMagic:
		totals.magicResist += Scaled(p, 15, 4);
		break;
	case Skill::WardingLight:
		totals.getHit -= Scaled(p, 2, 1);
		break;
	case Skill::AuraOfProtection:
		totals.bonusArmor += Scaled(p, 20, 6);
		break;
	case Skill::Sanctity:
		totals.fireResist += Scaled(p, 8, 2);
		totals.lightningResist += Scaled(p, 8, 2);
		totals.magicResist += Scaled(p, 8, 2);
		break;
	case Skill::BalladOfResilience:
		totals.fireResist += Scaled(p, 10, 3);
		totals.lightningResist += Scaled(p, 10, 3);
		totals.magicResist += Scaled(p, 10, 3);
		break;
	case Skill::HuntersChant:
		totals.bonusToHit += Scaled(p, 15, 5);
		break;
	case Skill::SerenadeOfSteel:
		totals.bonusArmor += Scaled(p, 20, 8);
		break;
	case Skill::SongOfPlenty:
		totals.goldFind += Scaled(p, 15, 4);
		totals.magicFind += Scaled(p, 8, 2);
		break;
	default:
		// Prayer, Meditation, Vigor, Melody of Life and Inspiration act elsewhere (the per-tick
		// hook and the walk animation); the rest are inert - see their rows.
		break;
	}
}


/**
 * @brief The character stat an RfA-12 skill grants for every rank in it (2026-09-13).
 *
 * The brief gave every one of the 162 skills two halves: one main skill, and one "level-up stat" - a
 * single character stat that grows with each rank. This is the second half, as data, so the sheet and
 * the tooltip read the same numbers and no skill can describe a stat it does not grant.
 */
enum class StatChannel : uint8_t {
	Strength,
	Magic,
	Dexterity,
	Vitality,
	Life,
	Mana,
	ArmorFlat,
	ArmorPercent,
	ToHitPercent,
	DamagePercent,
	DamageFlat,
	FireResist,
	LightningResist,
	MagicResist,
	DamageTaken,
	LightRadius,
	MagicFind,
	GoldFind,
	MoveSpeed,
	FastCast,
	FireDamage,
	LightningDamage,
};

struct LevelUpStat {
	Skill skill;
	StatChannel channel;
	/** The value at rank 1 (the low end, for a damage range). */
	int base;
	/** Added every `everyRanks` ranks after the first (the low end, for a damage range). */
	int perRank;
	/** A damage range's high end at rank 1, and what each step adds to it. Zero for everything else. */
	int baseMax;
	int perRankMax;
	int everyRanks;
};

// Generated from "RfA-12 - Final Skill List.csv"; the trailing comment is the list's own wording.
constexpr LevelUpStat LevelUpStats[] = {
	{ Skill::VotiveStrike, StatChannel::Strength, 1, 1, 0, 0, 1 }, // PAL-1-T1C3 Strength +1 / +1
	{ Skill::Judgment, StatChannel::ToHitPercent, 5, 3, 0, 0, 1 }, // PAL-1-T2C3 Chance to hit +5% / +3%
	{ Skill::Oathbrand, StatChannel::DamagePercent, 3, 1, 0, 0, 1 }, // PAL-1-T3C3 Damage +3% / +1%
	{ Skill::HolyLance, StatChannel::Dexterity, 2, 1, 0, 0, 1 }, // PAL-1-T4C3 Dexterity +2 / +1
	{ Skill::Crusade, StatChannel::DamagePercent, 8, 4, 0, 0, 1 }, // PAL-1-T5C2 Damage +8% / +4%
	{ Skill::AegisSlam, StatChannel::ArmorPercent, 6, 3, 0, 0, 1 }, // PAL-1-T5C3 Armor +6% / +3%
	{ Skill::HeavensDescent, StatChannel::Vitality, 3, 1, 0, 0, 1 }, // PAL-1-T6C2 Vitality +3 / +1
	{ Skill::WrathOfTheHeavens, StatChannel::Mana, 10, 3, 0, 0, 1 }, // PAL-1-T6C3 Mana +10 / +3
	{ Skill::Valor, StatChannel::DamageFlat, 2, 1, 0, 0, 1 }, // PAL-2-T1C2 Damage flat +2 / +1
	{ Skill::Radiance, StatChannel::LightRadius, 1, 1, 0, 0, 5 }, // PAL-2-T1C3 Light radius +1 / +1 per 5 ranks
	{ Skill::BaneOfEvil, StatChannel::Magic, 2, 1, 0, 0, 1 }, // PAL-2-T2C3 Magic +2 / +1
	{ Skill::Condemnation, StatChannel::ToHitPercent, 4, 2, 0, 0, 1 }, // PAL-2-T3C2 Chance to hit +4% / +2%
	{ Skill::TitheOfAsh, StatChannel::FireResist, 3, 1, 0, 0, 1 }, // PAL-2-T3C3 Fire resistance +3% / +1%
	{ Skill::Retaliation, StatChannel::Strength, 2, 1, 0, 0, 1 }, // PAL-2-T4C3 Strength +2 / +1
	{ Skill::DoomProcession, StatChannel::MoveSpeed, 2, 1, 0, 0, 1 }, // PAL-2-T5C3 Movement Speed +2% / +1%
	{ Skill::Dominion, StatChannel::Vitality, 3, 1, 0, 0, 1 }, // PAL-2-T6C3 Vitality +3 / +1
	{ Skill::Steadfast, StatChannel::ArmorFlat, 10, 3, 0, 0, 1 }, // PAL-3-T1C3 Armor +10 / +3
	{ Skill::ResistMagic, StatChannel::Life, 8, 3, 0, 0, 1 }, // PAL-3-T2C3 Life +8 / +3
	{ Skill::Immovable, StatChannel::ArmorFlat, 5, 2, 0, 0, 1 }, // PAL-3-T3C3 Armor flat +5 / +2
	{ Skill::WardingLight, StatChannel::Vitality, 2, 1, 0, 0, 1 }, // PAL-3-T4C2 Vitality +2 / +1
	{ Skill::Mercy, StatChannel::Life, 10, 4, 0, 0, 1 }, // PAL-3-T4C3 Life +10 / +4
	{ Skill::AuraOfProtection, StatChannel::MagicResist, 3, 1, 0, 0, 1 }, // PAL-3-T5C2 Magic resistance +3% / +1%
	{ Skill::Endurance, StatChannel::Vitality, 3, 1, 0, 0, 1 }, // PAL-3-T5C3 Vitality +3 / +1
	{ Skill::Sanctity, StatChannel::Mana, 10, 3, 0, 0, 1 }, // PAL-3-T6C3 Mana +10 / +3
	{ Skill::Cleave, StatChannel::DamagePercent, 6, 3, 0, 0, 1 }, // BAR-1-T1C2 Damage +6% / +3%
	{ Skill::Backhand, StatChannel::DamageFlat, 1, 1, 0, 0, 1 }, // BAR-1-T1C3 Damage flat +1 / +1
	{ Skill::GroundStomp, StatChannel::Vitality, 2, 1, 0, 0, 1 }, // BAR-1-T2C3 Vitality +2 / +1
	{ Skill::Rend, StatChannel::Strength, 2, 1, 0, 0, 1 }, // BAR-1-T3C3 Strength +2 / +1
	{ Skill::HammerOfTheAncients, StatChannel::DamagePercent, 8, 4, 0, 0, 1 }, // BAR-1-T4C3 Damage +8% / +4%
	{ Skill::SeismicSlam, StatChannel::Life, 15, 5, 0, 0, 1 }, // BAR-1-T5C2 Life +15 / +5
	{ Skill::ClaspOfRuin, StatChannel::Vitality, 2, 1, 0, 0, 1 }, // BAR-1-T5C3 Vitality +2 / +1
	{ Skill::Earthquake, StatChannel::Strength, 4, 2, 0, 0, 1 }, // BAR-1-T6C3 Strength +4 / +2
	{ Skill::GripOfIron, StatChannel::Strength, 2, 1, 0, 0, 1 }, // BAR-2-T3C2 Strength +2 / +1
	{ Skill::DeepWounds, StatChannel::DamageFlat, 2, 1, 0, 0, 1 }, // BAR-2-T3C3 Damage flat +2 / +1
	{ Skill::HeavyFoot, StatChannel::ArmorFlat, 6, 2, 0, 0, 1 }, // BAR-2-T4C2 Armor flat +6 / +2
	{ Skill::BattleHardened, StatChannel::MagicResist, 3, 1, 0, 0, 1 }, // BAR-2-T4C3 Magic resistance +3% / +1%
	{ Skill::Bloodlust, StatChannel::Vitality, 2, 1, 0, 0, 1 }, // BAR-2-T5C2 Vitality +2 / +1
	{ Skill::LongReach, StatChannel::ToHitPercent, 4, 1, 0, 0, 1 }, // BAR-2-T5C3 Chance to hit +4% / +1%
	{ Skill::UnfinishedBusiness, StatChannel::DamagePercent, 4, 1, 0, 0, 1 }, // BAR-2-T6C2 Damage +4% / +1%
	{ Skill::LastingWounds, StatChannel::DamageFlat, 2, 1, 0, 0, 1 }, // BAR-2-T6C3 Damage flat +2 / +1
	{ Skill::ThreateningShout, StatChannel::ArmorFlat, 10, 4, 0, 0, 1 }, // BAR-3-T1C3 Armor +10 / +4
	{ Skill::RallyingCry, StatChannel::Life, 10, 4, 0, 0, 1 }, // BAR-3-T2C3 Life +10 / +4
	{ Skill::Intimidate, StatChannel::ToHitPercent, 4, 2, 0, 0, 1 }, // BAR-3-T3C2 Chance to hit +4% / +2%
	{ Skill::SplitRanks, StatChannel::Strength, 2, 1, 0, 0, 1 }, // BAR-3-T3C3 Strength +2 / +1
	{ Skill::IronWill, StatChannel::MagicResist, 3, 1, 0, 0, 1 }, // BAR-3-T4C2 Magic resistance +3% / +1%
	{ Skill::Bloodcall, StatChannel::Life, 8, 3, 0, 0, 1 }, // BAR-3-T4C3 Life +8 / +3 (was Mana; the Barbarian has none, 2026-09-13)
	{ Skill::AncestralCall, StatChannel::Strength, 2, 1, 0, 0, 1 }, // BAR-3-T5C3 Strength +2 / +1
	{ Skill::EarthshakerCry, StatChannel::Vitality, 4, 2, 0, 0, 1 }, // BAR-3-T6C3 Vitality +4 / +2
	{ Skill::ChillTouch, StatChannel::Mana, 8, 3, 0, 0, 1 }, // SOR-1-T1C3 Mana +8 / +3
	{ Skill::IceNeedle, StatChannel::Magic, 1, 1, 0, 0, 1 }, // SOR-1-T2C3 Magic +1 / +1
	{ Skill::Frostbite, StatChannel::Magic, 2, 1, 0, 0, 1 }, // SOR-1-T3C2 Magic +2 / +1
	{ Skill::IceLance, StatChannel::FastCast, 3, 1, 0, 0, 1 }, // SOR-1-T3C3 Faster Cast Rate +3% / +1%
	{ Skill::BrittleGround, StatChannel::DamageTaken, 1, 1, 0, 0, 1 }, // SOR-1-T4C2 Damage taken reduced flat +1 / +1
	{ Skill::FrozenSentinel, StatChannel::Mana, 10, 4, 0, 0, 1 }, // SOR-1-T4C3 Mana +10 / +4
	{ Skill::Whiteout, StatChannel::FastCast, 2, 1, 0, 0, 1 }, // SOR-1-T5C3 Faster Cast Rate +2% / +1%
	{ Skill::AbsoluteZero, StatChannel::Magic, 4, 2, 0, 0, 1 }, // SOR-1-T6C3 Magic +4 / +2
	{ Skill::Arc, StatChannel::LightningDamage, 1, 0, 3, 1, 1 }, // SOR-2-T1C2 Lightning damage +1-3 / +1 max
	{ Skill::StaticCharge, StatChannel::LightningResist, 3, 1, 0, 0, 1 }, // SOR-2-T1C3 Lightning resistance +3% / +1%
	{ Skill::BallLightning, StatChannel::Mana, 8, 3, 0, 0, 1 }, // SOR-2-T2C3 Mana +8 / +3
	{ Skill::Conduit, StatChannel::FastCast, 3, 1, 0, 0, 1 }, // SOR-2-T3C3 Faster Cast Rate +3% / +1%
	{ Skill::LightningRod, StatChannel::LightningResist, 4, 1, 0, 0, 1 }, // SOR-2-T4C3 Lightning resistance +4% / +1%
	{ Skill::FaradayRing, StatChannel::ArmorFlat, 6, 2, 0, 0, 1 }, // SOR-2-T5C3 Armor flat +6 / +2
	{ Skill::StormCrucible, StatChannel::Magic, 3, 1, 0, 0, 1 }, // SOR-2-T6C2 Magic +3 / +1
	{ Skill::RideTheLightning, StatChannel::MoveSpeed, 3, 1, 0, 0, 1 }, // SOR-2-T6C3 Movement Speed +3% / +1%
	{ Skill::CinderTouch, StatChannel::FireResist, 2, 1, 0, 0, 1 }, // SOR-3-T1C3 Fire resistance +2% / +1%
	{ Skill::EmberMine, StatChannel::Magic, 1, 1, 0, 0, 1 }, // SOR-3-T2C2 Magic +1 / +1
	{ Skill::FlameRing, StatChannel::Life, 8, 3, 0, 0, 1 }, // SOR-3-T2C3 Life +8 / +3
	{ Skill::AshenBrand, StatChannel::FireDamage, 1, 1, 2, 1, 1 }, // SOR-3-T3C3 Fire damage +1-2 / +1 each end
	{ Skill::FurnaceMouth, StatChannel::ArmorFlat, 5, 2, 0, 0, 1 }, // SOR-3-T4C3 Armor flat +5 / +2
	{ Skill::Firestorm, StatChannel::Mana, 10, 4, 0, 0, 1 }, // SOR-3-T5C2 Mana +10 / +4
	{ Skill::Immolate, StatChannel::Vitality, 2, 1, 0, 0, 1 }, // SOR-3-T5C3 Vitality +2 / +1
	{ Skill::FuneralStar, StatChannel::Magic, 3, 1, 0, 0, 1 }, // SOR-3-T6C3 Magic +3 / +1
	{ Skill::BarbedShaft, StatChannel::Dexterity, 1, 1, 0, 0, 1 }, // ROG-1-T1C3 Dexterity +1 / +1
	{ Skill::ShockArrow, StatChannel::LightningDamage, 1, 0, 3, 1, 1 }, // ROG-1-T2C3 Lightning damage +1-3 / +1 max
	{ Skill::PiercingShot, StatChannel::ToHitPercent, 4, 2, 0, 0, 1 }, // ROG-1-T3C3 Chance to hit +4% / +2%
	{ Skill::RainOfArrows, StatChannel::DamagePercent, 6, 3, 0, 0, 1 }, // ROG-1-T4C2 Damage +6% / +3%
	{ Skill::CripplingShot, StatChannel::MoveSpeed, 3, 1, 0, 0, 1 }, // ROG-1-T4C3 Movement Speed +3% / +1%
	{ Skill::HuntersMark, StatChannel::Dexterity, 2, 1, 0, 0, 1 }, // ROG-1-T5C3 Dexterity +2 / +1
	{ Skill::Barrage, StatChannel::DamagePercent, 6, 3, 0, 0, 1 }, // ROG-1-T6C2 Damage +6% / +3%
	{ Skill::PhantomVolley, StatChannel::Mana, 10, 3, 0, 0, 1 }, // ROG-1-T6C3 Mana +10 / +3
	{ Skill::SoftTread, StatChannel::Dexterity, 1, 1, 0, 0, 1 }, // ROG-2-T1C3 Dexterity +1 / +1
	{ Skill::Swiftness, StatChannel::Dexterity, 2, 1, 0, 0, 1 }, // ROG-2-T2C2 Dexterity +2 / +1
	{ Skill::ScentOfBlood, StatChannel::ToHitPercent, 2, 1, 0, 0, 1 }, // ROG-2-T2C3 Chance to hit +2% / +1%
	{ Skill::Sharpen, StatChannel::DamageFlat, 2, 1, 0, 0, 1 }, // ROG-2-T3C3 Damage flat +2 / +1
	{ Skill::DeadGround, StatChannel::DamageFlat, 1, 1, 0, 0, 1 }, // ROG-2-T4C3 Damage flat +1 / +1
	{ Skill::Deadeye, StatChannel::ToHitPercent, 4, 2, 0, 0, 1 }, // ROG-2-T5C2 Chance to hit +4% / +2%
	{ Skill::ShadowStep, StatChannel::Dexterity, 2, 1, 0, 0, 1 }, // ROG-2-T5C3 Dexterity +2 / +1
	{ Skill::HuntersClaim, StatChannel::DamagePercent, 4, 1, 0, 0, 1 }, // ROG-2-T6C3 Damage +4% / +1%
	{ Skill::Sweep, StatChannel::DamagePercent, 6, 3, 0, 0, 1 }, // ROG-3-T1C2 Damage +6% / +3%
	{ Skill::Brace, StatChannel::ArmorFlat, 8, 3, 0, 0, 1 }, // ROG-3-T1C3 Armor +8 / +3
	{ Skill::Harpoon, StatChannel::Strength, 2, 1, 0, 0, 1 }, // ROG-3-T2C3 Strength +2 / +1
	{ Skill::Vault, StatChannel::MoveSpeed, 3, 1, 0, 0, 1 }, // ROG-3-T3C3 Movement Speed +3% / +1%
	{ Skill::ReapingPoint, StatChannel::DamagePercent, 3, 1, 0, 0, 1 }, // ROG-3-T4C3 Damage +3% / +1%
	{ Skill::AnchorJavelin, StatChannel::Life, 8, 3, 0, 0, 1 }, // ROG-3-T5C2 Life +8 / +3
	{ Skill::TurningPike, StatChannel::MoveSpeed, 2, 1, 0, 0, 1 }, // ROG-3-T5C3 Movement Speed +2% / +1%
	{ Skill::ValkyriesSpear, StatChannel::Strength, 4, 2, 0, 0, 1 }, // ROG-3-T6C3 Strength +4 / +2
	{ Skill::MinstrelsTune, StatChannel::Mana, 8, 3, 0, 0, 1 }, // BRD-1-T1C3 Mana +8 / +3
	{ Skill::BalladOfResilience, StatChannel::MagicResist, 3, 1, 0, 0, 1 }, // BRD-1-T2C3 Magic resistance +3% / +1%
	{ Skill::HuntersChant, StatChannel::Dexterity, 2, 1, 0, 0, 1 }, // BRD-1-T3C3 Dexterity +2 / +1
	{ Skill::SerenadeOfSteel, StatChannel::ArmorFlat, 10, 4, 0, 0, 1 }, // BRD-1-T4C1 Armor +10 / +4
	{ Skill::SongOfPlenty, StatChannel::GoldFind, 5, 2, 0, 0, 1 }, // BRD-1-T4C2 Gold Find +5% / +2%
	{ Skill::Nocturne, StatChannel::Vitality, 2, 1, 0, 0, 1 }, // BRD-1-T4C3 Vitality +2 / +1
	{ Skill::AnthemOfValor, StatChannel::Life, 15, 5, 0, 0, 1 }, // BRD-1-T5C1 Life +15 / +5
	{ Skill::SirensCall, StatChannel::ArmorPercent, 4, 2, 0, 0, 1 }, // BRD-1-T5C2 Armor +4% / +2%
	{ Skill::HymnOfRenewal, StatChannel::Life, 10, 4, 0, 0, 1 }, // BRD-1-T5C3 Life +10 / +4
	{ Skill::SymphonyOfWar, StatChannel::Magic, 4, 2, 0, 0, 1 }, // BRD-1-T6C2 Magic +4 / +2
	{ Skill::SovereignMeasure, StatChannel::Magic, 3, 1, 0, 0, 1 }, // BRD-1-T6C3 Magic +3 / +1
	{ Skill::PluckedNeedle, StatChannel::Magic, 1, 1, 0, 0, 1 }, // BRD-2-T1C3 Magic +1 / +1
	{ Skill::ShatterNote, StatChannel::ToHitPercent, 4, 2, 0, 0, 1 }, // BRD-2-T2C3 Chance to hit +4% / +2%
	{ Skill::TuningFork, StatChannel::Mana, 8, 3, 0, 0, 1 }, // BRD-2-T3C3 Mana +8 / +3
	{ Skill::Thunderclap, StatChannel::LightningDamage, 1, 0, 3, 1, 1 }, // BRD-2-T4C1 Lightning damage +1-3 / +1 max
	{ Skill::DissonantThread, StatChannel::DamagePercent, 3, 1, 0, 0, 1 }, // BRD-2-T4C2 Damage +3% / +1%
	{ Skill::SoundWave, StatChannel::FastCast, 3, 1, 0, 0, 1 }, // BRD-2-T4C3 Faster Cast Rate +3% / +1%
	{ Skill::DeafeningRoar, StatChannel::MagicResist, 3, 1, 0, 0, 1 }, // BRD-2-T5C1 Magic resistance +3% / +1%
	{ Skill::ChordOfWarding, StatChannel::Life, 12, 4, 0, 0, 1 }, // BRD-2-T5C2 Life +12 / +4
	{ Skill::Feedback, StatChannel::LightningResist, 3, 1, 0, 0, 1 }, // BRD-2-T5C3 Lightning resistance +3% / +1%
	{ Skill::GrandFinale, StatChannel::DamagePercent, 6, 3, 0, 0, 1 }, // BRD-2-T6C2 Damage +6% / +3%
	{ Skill::MusicOfTheSpheres, StatChannel::Mana, 10, 4, 0, 0, 1 }, // BRD-2-T6C3 Mana +10 / +4
	{ Skill::BitterCouplet, StatChannel::Magic, 1, 1, 0, 0, 1 }, // BRD-3-T1C3 Magic +1 / +1
	{ Skill::MockingRhyme, StatChannel::Dexterity, 2, 1, 0, 0, 1 }, // BRD-3-T2C3 Dexterity +2 / +1
	{ Skill::Epitaph, StatChannel::Magic, 2, 1, 0, 0, 1 }, // BRD-3-T3C3 Magic +2 / +1
	{ Skill::Elegy, StatChannel::Mana, 8, 3, 0, 0, 1 }, // BRD-3-T4C1 Mana +8 / +3
	{ Skill::SonnetOfSight, StatChannel::LightRadius, 1, 1, 0, 0, 5 }, // BRD-3-T4C2 Light radius +1 / +1 per 5 ranks
	{ Skill::Satire, StatChannel::MagicResist, 3, 1, 0, 0, 1 }, // BRD-3-T4C3 Magic resistance +3% / +1%
	{ Skill::VerseOfBinding, StatChannel::Vitality, 2, 1, 0, 0, 1 }, // BRD-3-T5C1 Vitality +2 / +1
	{ Skill::HeroicCouplet, StatChannel::FastCast, 3, 1, 0, 0, 1 }, // BRD-3-T5C2 Faster Cast Rate +3% / +1%
	{ Skill::Tragedy, StatChannel::Life, 12, 4, 0, 0, 1 }, // BRD-3-T5C3 Life +12 / +4
	{ Skill::Saga, StatChannel::Magic, 4, 2, 0, 0, 1 }, // BRD-3-T6C2 Magic +4 / +2
	{ Skill::LastWord, StatChannel::DamagePercent, 6, 3, 0, 0, 1 }, // BRD-3-T6C3 Damage +6% / +3%
	{ Skill::StaffParry, StatChannel::ArmorFlat, 8, 3, 0, 0, 1 }, // MON-1-T1C2 Armor +8 / +3
	{ Skill::LongThrust, StatChannel::ToHitPercent, 4, 2, 0, 0, 1 }, // MON-1-T1C3 Chance to hit +4% / +2%
	{ Skill::LowBranch, StatChannel::ToHitPercent, 3, 1, 0, 0, 1 }, // MON-1-T2C2 Chance to hit +3% / +1%
	{ Skill::RearwardReach, StatChannel::DamageFlat, 1, 1, 0, 0, 1 }, // MON-1-T2C3 Damage flat +1 / +1
	{ Skill::MountainPole, StatChannel::Strength, 2, 1, 0, 0, 1 }, // MON-1-T3C2 Strength +2 / +1
	{ Skill::BambooRain, StatChannel::DamagePercent, 6, 3, 0, 0, 1 }, // MON-1-T3C3 Damage +6% / +3%
	{ Skill::DragonTailSweep, StatChannel::Vitality, 2, 1, 0, 0, 1 }, // MON-1-T4C2 Vitality +2 / +1
	{ Skill::StaffOfEchoes, StatChannel::Mana, 8, 3, 0, 0, 1 }, // MON-1-T4C3 Mana +8 / +3
	{ Skill::RiverStance, StatChannel::MoveSpeed, 3, 1, 0, 0, 1 }, // MON-1-T5C2 Movement Speed +3% / +1%
	{ Skill::HeavenSplitter, StatChannel::DamagePercent, 8, 4, 0, 0, 1 }, // MON-1-T5C3 Damage +8% / +4%
	{ Skill::ThousandReeds, StatChannel::Dexterity, 4, 2, 0, 0, 1 }, // MON-1-T6C3 Dexterity +4 / +2
	{ Skill::TigerClaw, StatChannel::DamagePercent, 6, 3, 0, 0, 1 }, // MON-2-T1C2 Damage +6% / +3%
	{ Skill::DeepBreath, StatChannel::Life, 8, 3, 0, 0, 1 }, // MON-2-T1C3 Life +8 / +3
	{ Skill::LeapingCrane, StatChannel::MoveSpeed, 3, 1, 0, 0, 1 }, // MON-2-T2C2 Movement Speed +3% / +1%
	{ Skill::PressurePoint, StatChannel::ToHitPercent, 4, 2, 0, 0, 1 }, // MON-2-T2C3 Chance to hit +4% / +2%
	{ Skill::IronFist, StatChannel::DamageFlat, 2, 1, 0, 0, 1 }, // MON-2-T3C2 Damage flat +2 / +1
	{ Skill::WhirlingKick, StatChannel::Dexterity, 2, 1, 0, 0, 1 }, // MON-2-T3C3 Dexterity +2 / +1
	{ Skill::ShoulderGate, StatChannel::Vitality, 2, 1, 0, 0, 1 }, // MON-2-T4C2 Vitality +2 / +1
	{ Skill::SevenSidedStrike, StatChannel::DamagePercent, 6, 3, 0, 0, 1 }, // MON-2-T4C3 Damage +6% / +3%
	{ Skill::MountainStance, StatChannel::ArmorPercent, 4, 2, 0, 0, 1 }, // MON-2-T5C2 Armor +4% / +2%
	{ Skill::ExplodingPalm, StatChannel::Strength, 2, 1, 0, 0, 1 }, // MON-2-T5C3 Strength +2 / +1
	{ Skill::DragonsWrath, StatChannel::Strength, 4, 2, 0, 0, 1 }, // MON-2-T6C3 Strength +4 / +2
	{ Skill::MantraOfClarity, StatChannel::Mana, 8, 3, 0, 0, 1 }, // MON-3-T1C2 Mana +8 / +3
	{ Skill::MantraOfEvasion, StatChannel::Dexterity, 2, 1, 0, 0, 1 }, // MON-3-T1C3 Dexterity +2 / +1
	{ Skill::ChiWave, StatChannel::Magic, 2, 1, 0, 0, 1 }, // MON-3-T2C2 Magic +2 / +1
	{ Skill::BlindingFlash, StatChannel::MagicResist, 3, 1, 0, 0, 1 }, // MON-3-T2C3 Magic resistance +3% / +1%
	{ Skill::MantraOfRetribution, StatChannel::ArmorFlat, 10, 4, 0, 0, 1 }, // MON-3-T3C2 Armor +10 / +4
	{ Skill::Serenity, StatChannel::Life, 10, 4, 0, 0, 1 }, // MON-3-T3C3 Life +10 / +4
	{ Skill::SpiritGuardian, StatChannel::Vitality, 2, 1, 0, 0, 1 }, // MON-3-T4C2 Vitality +2 / +1
	{ Skill::WaveOfLight, StatChannel::DamagePercent, 6, 3, 0, 0, 1 }, // MON-3-T4C3 Damage +6% / +3%
	{ Skill::InnerFire, StatChannel::FireDamage, 1, 0, 3, 1, 1 }, // MON-3-T5C2 Fire damage +1-3 / +1 max
	{ Skill::AstralProjection, StatChannel::MoveSpeed, 3, 1, 0, 0, 1 }, // MON-3-T5C3 Movement Speed +3% / +1%
	{ Skill::AncestralCourt, StatChannel::Magic, 3, 1, 0, 0, 1 }, // MON-3-T6C3 Magic +3 / +1
};

const LevelUpStat *LevelUpStatOf(Skill skill)
{
	for (const LevelUpStat &stat : LevelUpStats) {
		if (stat.skill == skill)
			return &stat;
	}
	return nullptr;
}

/** @brief @p base, plus @p perStep for every @p everyRanks ranks past the first. Zero with no points. */
int LevelUpValueAt(int points, int base, int perStep, int everyRanks)
{
	if (points <= 0)
		return 0;
	return base + perStep * ((points - 1) / std::max(everyRanks, 1));
}

/** @brief Adds @p skill's level-up stat at @p points to @p totals. Nothing for a skill without one. */
void ApplyLevelUpStat(Skill skill, int points, ItemBonusTotals &totals)
{
	const LevelUpStat *stat = LevelUpStatOf(skill);
	if (stat == nullptr || points <= 0)
		return;
	const int v = LevelUpValueAt(points, stat->base, stat->perRank, stat->everyRanks);
	switch (stat->channel) {
	case StatChannel::Strength:
		totals.strength += v;
		break;
	case StatChannel::Magic:
		totals.magic += v;
		break;
	case StatChannel::Dexterity:
		totals.dexterity += v;
		break;
	case StatChannel::Vitality:
		totals.vitality += v;
		break;
	case StatChannel::Life:
		// Life and mana are kept in 1/64 units - the same shift Battle Orders applies.
		totals.hitPoints += v << 6;
		break;
	case StatChannel::Mana:
		totals.mana += v << 6;
		break;
	case StatChannel::ArmorFlat:
		totals.armor += v;
		break;
	case StatChannel::ArmorPercent:
		totals.bonusArmor += v;
		break;
	case StatChannel::ToHitPercent:
		totals.bonusToHit += v;
		break;
	case StatChannel::DamagePercent:
		totals.bonusDamage += v;
		break;
	case StatChannel::DamageFlat:
		totals.damageMod += v;
		break;
	case StatChannel::FireResist:
		totals.fireResist += v;
		break;
	case StatChannel::LightningResist:
		totals.lightningResist += v;
		break;
	case StatChannel::MagicResist:
		totals.magicResist += v;
		break;
	case StatChannel::DamageTaken:
		// getHit is added to every blow taken, so less damage is a negative number.
		totals.getHit -= v;
		break;
	case StatChannel::LightRadius:
		totals.lightRadius += v;
		break;
	case StatChannel::MagicFind:
		totals.magicFind += v;
		break;
	case StatChannel::GoldFind:
		totals.goldFind += v;
		break;
	case StatChannel::MoveSpeed:
		totals.moveSpeed += v;
		break;
	case StatChannel::FastCast:
		totals.fastCast += v;
		break;
	case StatChannel::FireDamage:
		totals.fireMin += v;
		totals.fireMax += LevelUpValueAt(points, stat->baseMax, stat->perRankMax, stat->everyRanks);
		break;
	case StatChannel::LightningDamage:
		totals.lightningMin += v;
		totals.lightningMax += LevelUpValueAt(points, stat->baseMax, stat->perRankMax, stat->everyRanks);
		break;
	}
}

/**
 * @brief @p skill's level-up stat at @p points as one tooltip line, or empty.
 *
 * Its own words rather than DescribeBonusTotals', because that prints life and mana in the 1/64 units
 * the totals keep them in - "+640 life" for ten points.
 */
std::string LevelUpStatLine(Skill skill, int points)
{
	const LevelUpStat *stat = LevelUpStatOf(skill);
	if (stat == nullptr || points <= 0)
		return {};
	const int v = LevelUpValueAt(points, stat->base, stat->perRank, stat->everyRanks);
	const int vMax = LevelUpValueAt(points, stat->baseMax, stat->perRankMax, stat->everyRanks);
	switch (stat->channel) {
	case StatChannel::Strength:
		return fmt::format(fmt::runtime(_("+{:d} strength")), v);
	case StatChannel::Magic:
		return fmt::format(fmt::runtime(_("+{:d} magic")), v);
	case StatChannel::Dexterity:
		return fmt::format(fmt::runtime(_("+{:d} dexterity")), v);
	case StatChannel::Vitality:
		return fmt::format(fmt::runtime(_("+{:d} vitality")), v);
	case StatChannel::Life:
		return fmt::format(fmt::runtime(_("+{:d} life")), v);
	case StatChannel::Mana:
		return fmt::format(fmt::runtime(_("+{:d} mana")), v);
	case StatChannel::ArmorFlat:
		return fmt::format(fmt::runtime(_("+{:d} armour")), v);
	case StatChannel::ArmorPercent:
		return fmt::format(fmt::runtime(_("+{:d}% armour")), v);
	case StatChannel::ToHitPercent:
		return fmt::format(fmt::runtime(_("+{:d}% to hit")), v);
	case StatChannel::DamagePercent:
		return fmt::format(fmt::runtime(_("+{:d}% damage")), v);
	case StatChannel::DamageFlat:
		return fmt::format(fmt::runtime(_("+{:d} damage")), v);
	case StatChannel::FireResist:
		return fmt::format(fmt::runtime(_("+{:d}% fire resist")), v);
	case StatChannel::LightningResist:
		return fmt::format(fmt::runtime(_("+{:d}% lightning resist")), v);
	case StatChannel::MagicResist:
		return fmt::format(fmt::runtime(_("+{:d}% magic resist")), v);
	case StatChannel::DamageTaken:
		return fmt::format(fmt::runtime(_("-{:d} damage taken")), v);
	case StatChannel::LightRadius:
		return fmt::format(fmt::runtime(_("+{:d} light radius")), v);
	case StatChannel::MagicFind:
		return fmt::format(fmt::runtime(_("+{:d}% magic find")), v);
	case StatChannel::GoldFind:
		return fmt::format(fmt::runtime(_("+{:d}% gold find")), v);
	case StatChannel::MoveSpeed:
		return fmt::format(fmt::runtime(_("+{:d}% movement speed")), v);
	case StatChannel::FastCast:
		return fmt::format(fmt::runtime(_("+{:d}% faster cast rate")), v);
	case StatChannel::FireDamage:
		return fmt::format(fmt::runtime(_("+{:d}-{:d} fire damage")), v, vMax);
	case StatChannel::LightningDamage:
		return fmt::format(fmt::runtime(_("+{:d}-{:d} lightning damage")), v, vMax);
	}
	return {};
}

} // namespace

const ClassTreeSkillData &GetClassTreeSkillData(Skill skill)
{
	const auto index = static_cast<size_t>(skill);
	assert(index < ClassTreeSkillCount);
	return Skills[index];
}

/**
 * @brief The oracool/paladin_skills.h skill a tree row borrows, if it borrows one.
 *
 * Five Combat Skills rows are not implementations at all - they are second faces on skills that
 * module owns. It keeps their spell slot, their level gate, their shield requirement and their mana
 * price; this table keeps only their name, description, tier and column.
 *
 * ONE list, read by everything that has to know: the slot lookup below and the unlock test further
 * down both used to answer for themselves, and they disagreed - see IsClassTreeSkillUnlocked.
 */
std::optional<PaladinSkill> BorrowedPaladinSkill(Skill skill)
{
	switch (skill) {
	case Skill::Smite:
		return PaladinSkill::ShieldBash;
	case Skill::Zeal:
		return PaladinSkill::Zeal;
	case Skill::Charge:
		return PaladinSkill::Charge;
	case Skill::BlessedHammer:
		return PaladinSkill::BlessedHammer;
	case Skill::FistOfTheHeavens:
		return PaladinSkill::FistOfTheHeavens;
	case Skill::HammerOfFaith:
		return PaladinSkill::HammerOfFaith;
	case Skill::BlessedShield:
		return PaladinSkill::BlessedShield;
	default:
		return std::nullopt;
	}
}

SpellID ClassTreeSpellId(Skill skill)
{
	// Resolved HERE rather than stored in the table: the borrowed slots are owned by
	// oracool/paladin_skills.h, and reading that module's table during this one's static
	// initialisation would be an initialisation-order gamble across translation units.
	if (const std::optional<PaladinSkill> borrowed = BorrowedPaladinSkill(skill); borrowed.has_value())
		return GetPaladinSkillData(*borrowed).spellId;
	return skill > Skill::LAST ? SpellID::Invalid : Skills[static_cast<size_t>(skill)].spellId;
}

Skill ClassTreeSkillForSpell(HeroClass heroClass, SpellID spell)
{
	if (spell == SpellID::Invalid)
		return Skill::None;
	// Linear over 163 rows, called once or twice a frame to pick a HUD icon. A lookup table would
	// have to be per class and rebuilt whenever the borrowed-slot resolution above changes, which is
	// a lot of machinery to save a scan that never shows up in a profile.
	for (size_t i = 0; i < ClassTreeSkillCount; i++) {
		const Skill skill = static_cast<Skill>(i);
		if (Skills[i].heroClass != heroClass)
			continue;
		if (ClassTreeSpellId(skill) == spell)
			return skill;
	}
	return Skill::None;
}

int ClassTreeMaxRank(Skill skill)
{
	if (skill > Skill::LAST)
		return MaxTreeInvestment;
	const int declared = Skills[static_cast<size_t>(skill)].maxRank;
	return declared > 0 ? declared : MaxTreeInvestment;
}

int ClassTreeTierMinLevel(int tier)
{
	if (tier < 0 || tier >= static_cast<int>(std::size(TierLevels)))
		return 1;
	return TierLevels[tier];
}

string_view GetClassTreePageName(HeroClass heroClass, int page)
{
	// The fourth page is the same page for everyone, so it is answered before the per-class switch
	// rather than repeated in six branches. It also has to come FIRST: every branch below reads
	// "page == 1 ? a : b", which would hand page 3 the third page's name.
	//
	// The Rogue's second page is Diablo II's own "PASSIVE & MAGIC", so she now carries two pages
	// with "passive" in the name. Left alone: renaming a D2 sheet to tidy up a D3 one would be the
	// wrong thing to give way.
	if (page == 3)
		return _("PASSIVE SKILLS");
	switch (heroClass) {
	case HeroClass::Warrior:
		if (page == 0)
			return _("COMBAT SKILLS");
		return page == 1 ? _("OFFENSIVE AURAS") : _("DEFENSIVE AURAS");
	case HeroClass::Barbarian:
		if (page == 0)
			return _("COMBAT SKILLS");
		return page == 1 ? _("COMBAT MASTERIES") : _("WARCRIES");
	case HeroClass::Sorcerer:
		if (page == 0)
			return _("COLD SPELLS");
		return page == 1 ? _("LIGHTNING SPELLS") : _("FIRE SPELLS");
	case HeroClass::Rogue:
		if (page == 0)
			return _("BOW & CROSSBOW");
		return page == 1 ? _("PASSIVE & MAGIC") : _("JAVELIN & SPEAR");
	case HeroClass::Bard:
		if (page == 0)
			return _("MELODY");
		return page == 1 ? _("HARMONY") : _("POETRY");
	case HeroClass::Monk:
		if (page == 0)
			return _("WAY OF THE STAFF");
		return page == 1 ? _("WAY OF THE BODY") : _("WAY OF THE SPIRIT");
	case HeroClass::Necromancer:
		if (page == 0)
			return _("SUMMONING");
		return page == 1 ? _("POISON & BONE") : _("CURSES");
	default:
		return {};
	}
}

bool ClassHasTree(HeroClass heroClass)
{
	return FirstSkillOf(heroClass) != Skill::None;
}

int ClassTreeIconIndex(Skill skill)
{
	if (skill > Skill::LAST)
		return 0;
	const Skill first = FirstSkillOf(GetClassTreeSkillData(skill).heroClass);
	const int index = static_cast<int>(skill) - static_cast<int>(first);
	// Clamped at the source rather than at each caller (audit, 2026-08-31). The two callers that
	// WRITE - InvestClassTreePoint and RefundClassTreePoint - indexed _pClassTreeInvestment with
	// this result unchecked, while every reader guarded it. That array sits mid-struct in Player, so
	// an out-of-range index would not fault; it would write into the next field and then save it.
	//
	// class_tree.h's static_asserts make this unreachable by construction - no class can outgrow
	// MaxSkillsPerClass without failing the build - so this is the belt to that pair of braces, and
	// it returns 0 the same way the LAST guard above does rather than inventing a second failure
	// mode for callers to handle.
	if (index < 0 || index >= static_cast<int>(MaxSkillsPerClass))
		return 0;
	return index;
}

std::optional<ClassTreeSkill> ClassTreeSkillAtIndex(HeroClass heroClass, int index)
{
	if (index < 0 || index >= static_cast<int>(MaxSkillsPerClass))
		return std::nullopt;
	const Skill first = FirstSkillOf(heroClass);
	if (first == Skill::None)
		return std::nullopt;
	const size_t absolute = static_cast<size_t>(first) + static_cast<size_t>(index);
	if (absolute >= ClassTreeSkillCount)
		return std::nullopt;
	const auto skill = static_cast<Skill>(absolute);
	// The index must land inside the SAME class, or it is not that class's skill at all -
	// which is the whole property this representation exists to guarantee.
	if (GetClassTreeSkillData(skill).heroClass != heroClass)
		return std::nullopt;
	return skill;
}

bool IsClassTreeSkillUnlocked(const Player &player, Skill skill)
{
	if (skill > Skill::LAST)
		return false;
	const ClassTreeSkillData &data = GetClassTreeSkillData(skill);
	if (data.heroClass != player._pClass)
		return false;
	// A Passive Skills row is gated by its OWN level, not by its tier. Its tier is only where it
	// sits on the grid: three rows to a tier, but one unlock every even level, so the three cells of
	// a tier open at 2, 4 and 6 rather than together. Nothing else on the page is bought at all.
	if (IsPassiveSkillRow(skill))
		return player._pLevel >= PassiveSkillRequiredLevel(skill);
	if (player._pLevel < ClassTreeTierMinLevel(data.tier))
		return false;

	// A row that BORROWS a skill must also answer that skill's own requirements, or the tree offers
	// something the game will then refuse.
	//
	// Bug (fixed 2026-08-16, found while auditing the Abilities window on the user's report that the
	// sheets were "full of issues"). This function used to stop at the tier level, and the tier level
	// is not what decides whether a borrowed skill fires - CanUsePaladinSkill asks
	// IsPaladinSkillUnlocked, which has its own minLevel AND a shield requirement. They disagreed
	// four times out of five:
	//
	//   Smite (Shield Bash)   tier 0 = level 1  vs  level 8 and a shield
	//   Charge                tier 1 = level 6  vs  level 12
	//   Blessed Hammer        tier 3 = level 18 vs  level 18   (aligned in paladin_skills.cpp)
	//   Fist of the Heavens   tier 5 = level 30 vs  level 30   (aligned in paladin_skills.cpp)
	//
	// The visible half was Smite: the Combat Skills page lit it at level 1 with no shield in hand,
	// let it be clicked onto a mouse button, and the button then did nothing - while the Skills
	// sheet, one arrow away, correctly showed the same skill greyed out.
	if (const std::optional<PaladinSkill> borrowed = BorrowedPaladinSkill(skill); borrowed.has_value())
		return IsPaladinSkillUnlocked(player, *borrowed);
	return true;
}

int ClassTreeInvestment(const Player &player, Skill skill)
{
	if (skill > Skill::LAST)
		return 0;
	// A skill with a slot stores its points where GetSpellLevel will find them; only the rest
	// need the tree's own array. See the file comment.
	if (const SpellID slot = ClassTreeSpellId(skill); slot != SpellID::Invalid)
		return player._pSkillInvestment[static_cast<size_t>(slot)];
	const int index = ClassTreeIconIndex(skill);
	if (index < 0 || index >= static_cast<int>(MaxSkillsPerClass))
		return 0;
	return player._pClassTreeInvestment[index];
}

bool IsClassTreeRowRetiredAsSpell(Skill skill)
{
	return SpellHasBook(ClassTreeSpellId(skill));
}

bool CanInvestClassTreePoint(const Player &player, Skill skill)
{
	// Belt and braces with the filter in BuildClassTreePage: the page no longer offers these rows,
	// and this refuses them even if some other path reaches one.
	if (IsClassTreeRowRetiredAsSpell(skill))
		return false;
	// A Passive Skills row costs nothing and cannot be bought (user, 2026-08-25). It arrives on its
	// own at its level; what a player DECIDES about it is which four to slot. Refused here as well
	// as at the UI so no other path can put a point somewhere it can never be spent or refunded.
	if (IsPassiveSkillRow(skill))
		return false;
	if (player._pUnspentSkillPoints <= 0 || !IsClassTreeSkillUnlocked(player, skill))
		return false;
	const int invested = ClassTreeInvestment(player, skill);
	if (invested >= ClassTreeMaxRank(skill))
		return false;
	// The Rule of Rangs (user, 2026-08-19): each rank costs one character level more than the rank
	// before it, counting from the skill's own tier. A tier-1 skill takes its second point at level
	// 2 and its tenth at level 10; a tier-30 skill takes its tenth at 39.
	//
	// Applied here rather than inside IsClassTreeSkillUnlocked, which answers "is this row yours at
	// all" - the tier gate - and is asked by the assignment and drawing paths too. Depth is a
	// separate question from ownership.
	return player._pLevel >= RankRequiredLevel(ClassTreeTierMinLevel(GetClassTreeSkillData(skill).tier), invested + 1);
}

bool InvestClassTreePoint(Player &player, Skill skill)
{
	if (!CanInvestClassTreePoint(player, skill))
		return false;
	player._pUnspentSkillPoints--;
	if (const SpellID slot = ClassTreeSpellId(skill); slot != SpellID::Invalid) {
		player._pSkillInvestment[static_cast<size_t>(slot)]++;
	} else {
		player._pClassTreeInvestment[ClassTreeIconIndex(skill)]++;
	}
	// The other half of the refund's refresh: a row's FIRST point is what puts it into _pAblSpells,
	// so without this a newly bought skill would not be selectable until the next load recomputed
	// the mask.
	RefreshInnateSpells(player);

	if (&player == MyPlayer) {
		LogEvent(fmt::format("{:s} raised to {:d}", std::string(_(GetClassTreeSkillData(skill).name)),
		             ClassTreeInvestment(player, skill)),
		    UiFlags::ColorWhitegold);
		// The `learn` cue. Only the passives and masteries have one - an active's confirmation is its
		// first cast. The rest get the interface click instead, so a point spent is never silent.
		if (!PlaySkillSound(skill, SkillSoundEvent::Learn))
			PlayUiSelectSound();
	}
	ScheduleAutoSaveForSkillChange();
	return true;
}

bool CanRefundClassTreePoint(const Player &player, Skill skill)
{
	return ClassTreeInvestment(player, skill) > 0;
}

bool RefundClassTreePoint(Player &player, Skill skill)
{
	if (!CanRefundClassTreePoint(player, skill))
		return false;
	player._pUnspentSkillPoints++;
	const SpellID slot = ClassTreeSpellId(skill);
	if (slot != SpellID::Invalid) {
		player._pSkillInvestment[static_cast<size_t>(slot)]--;
	} else {
		player._pClassTreeInvestment[ClassTreeIconIndex(skill)]--;
	}

	// An aura at zero has no strength left to give, and ToggleClassAura already refuses to LIGHT one
	// in that state - so leaving it burning would be the one way to hold an aura the rules say you
	// cannot have. Put out here rather than guarded at every reader.
	if (GetClassTreeSkillData(skill).kind == Kind::Aura
	    && GetActiveClassAura(player) == skill
	    && ClassTreeInvestment(player, skill) <= 0) {
		player._pOracoolActiveAura = static_cast<uint16_t>(Skill::None);
		if (&player == MyPlayer)
			SetAuraLoop(Skill::None);
	}

	// The mask and the buttons follow the points. A row refunded to zero is no longer something the
	// character HAS, so it must leave _pAblSpells and let go of any slot holding it.
	RefreshInnateSpells(player);

	if (&player == MyPlayer) {
		LogEvent(fmt::format("{:s} lowered to {:d}", std::string(_(GetClassTreeSkillData(skill).name)),
		             ClassTreeInvestment(player, skill)),
		    UiFlags::ColorWhitegold);
	}
	ScheduleAutoSaveForSkillChange();
	return true;
}

// ---------------------------------------------------------------------------------------------
// The Passive Skills page: free, automatic, and slotted (user, 2026-08-25).
//
// Three rules, and the third is what makes the page a choice rather than a list. Passives cost no
// skill points. They unlock on their own, one every even character level. And a passive only DOES
// anything while it sits in one of four slots, which open at levels 1, 10, 20 and 30 - so a
// character at the cap has every passive available and may run four of them.
// ---------------------------------------------------------------------------------------------

bool IsPassiveSkillRow(Skill skill)
{
	if (skill > Skill::LAST)
		return false;
	return GetClassTreeSkillData(skill).page == PassiveSkillsPage;
}

namespace {

/**
 * @brief @p skill's position among its class's passives, counting from zero, or -1.
 *
 * Counted rather than derived from a stored base. The passives are contiguous at the end of each
 * class block today, and an arithmetic shortcut would quietly stop being true the first time that
 * changes - which is precisely the class of bug this file keeps recording.
 */
int PassiveIndexOnPage(Skill skill)
{
	if (!IsPassiveSkillRow(skill))
		return -1;
	// The GRID cell, reading order - the same tier and column BuildClassTreePage places it by.
	// It used to count rows in table order, which agreed only while the table happened to be in grid
	// order; a passive could not move to another cell without moving its row, and a row's position is
	// its identity (icon strip, investment index, slot bytes). Moving Unforgiving to the level-10 cell
	// (user, 2026-09-14) is a change of tier and column alone.
	const ClassTreeSkillData &data = GetClassTreeSkillData(skill);
	return data.tier * 3 + data.column;
}

/** @brief The class-relative index stored in a slot byte, or -1 for empty. */
int SlotByteToIconIndex(uint8_t value)
{
	return value == 0xFF ? -1 : static_cast<int>(value);
}

} // namespace

int PassiveSkillRequiredLevel(Skill skill)
{
	const int index = PassiveIndexOnPage(skill);
	if (index < 0)
		return 0;
	// One every even level: the first at 2, the nth at 2n+2. Eighteen passives - a full 3x6 page, the
	// most any class has since 2026-09-12 - reach level 36, well inside the character cap.
	return 2 * (index + 1);
}

int PassiveSlotRequiredLevel(int slot)
{
	// 1, 10, 20, 30 (user, 2026-08-25). Slot one opens immediately even though the first passive
	// does not arrive until level 2 - an empty slot on a level-1 character is the page explaining
	// itself, not a gap.
	constexpr int Levels[PassiveSlotCount] = { 1, 10, 20, 30 };
	if (slot < 0 || slot >= static_cast<int>(PassiveSlotCount))
		return 0;
	return Levels[slot];
}

int UnlockedPassiveSlotCount(const Player &player)
{
	int count = 0;
	for (int slot = 0; slot < static_cast<int>(PassiveSlotCount); slot++) {
		if (player._pLevel >= PassiveSlotRequiredLevel(slot))
			count++;
	}
	return count;
}

Skill PassiveInSlot(const Player &player, int slot)
{
	if (slot < 0 || slot >= static_cast<int>(PassiveSlotCount))
		return Skill::None;
	if (player._pLevel < PassiveSlotRequiredLevel(slot))
		return Skill::None;
	const int index = SlotByteToIconIndex(player._pPassiveSlots[slot]);
	if (index < 0)
		return Skill::None;
	const Skill first = FirstSkillOf(player._pClass);
	if (first == Skill::None)
		return Skill::None;
	const size_t absolute = static_cast<size_t>(first) + static_cast<size_t>(index);
	if (absolute >= ClassTreeSkillCount)
		return Skill::None;
	const auto skill = static_cast<Skill>(absolute);
	// Validated on the way OUT rather than trusted from the save. A slot byte is a class-relative
	// index, and a character who somehow carries one that is not this class's passive, or is a
	// passive they no longer meet the level for, reads as an empty slot - never as a live skill
	// they have not earned.
	if (!IsPassiveSkillRow(skill) || !IsClassTreeSkillUnlocked(player, skill))
		return Skill::None;
	return skill;
}

int PassiveSlotOf(const Player &player, Skill skill)
{
	if (!IsPassiveSkillRow(skill))
		return -1;
	for (int slot = 0; slot < static_cast<int>(PassiveSlotCount); slot++) {
		if (PassiveInSlot(player, slot) == skill)
			return slot;
	}
	return -1;
}

namespace {

/**
 * @brief Heavenly Strength leaving its slot takes its grip with it: a shield held beside a two-hander goes to
 * the backpack, or to the ground at the hero's feet when the backpack is full (user, 2026-09-11: "shield must
 * go to inventory of drop on ground if inventory is full"). The slot change itself is never refused.
 */
void ReleaseHeavenlyGrip(Player &player)
{
	Item &left = player.InvBody[INVLOC_HAND_LEFT];
	Item &right = player.InvBody[INVLOC_HAND_RIGHT];
	if (left.isEmpty() || right.isEmpty())
		return;
	const bool leftTwoHanded = left._iClass == ICLASS_WEAPON && left._iLoc == ILOC_TWOHAND;
	const bool rightTwoHanded = right._iClass == ICLASS_WEAPON && right._iLoc == ILOC_TWOHAND;
	if (!leftTwoHanded && !rightTwoHanded)
		return;
	Item &offHand = leftTwoHanded ? right : left;
	if (!AutoPlaceItemInInventory(player, offHand, /*persistItem=*/true)) {
		DropItemBesidePlayer(player, offHand);
		EventPlrMsg(_("Your backpack is full - the shield is on the ground at your feet."), UiFlags::ColorWhitegold);
	}
	offHand.clear();
	// With the sprites: the body is now a two-hander without a shield. The Abilities window's own recalc
	// after the slot change is stats-only.
	CalcPlrInv(player, true);
}

} // namespace

bool HeavenlyStrengthGrips(const Player &player, const Item &item)
{
	// Every two-handed weapon but a bow (user, 2026-09-11: "works for Great Sword, but doesnt work for Great
	// Axe, Bows, Staff", then "lets make bows always require 2 hands. makes no sense to have a bow and shield.
	// but it is possible ... to wear heavy axe or big staff with one hand and shield in the other"). The first
	// cut took only the Barbarian's two, swords and maces, because theirs are the body sprites with a shield
	// variant; an axe, a pike or a staff held with a shield wears the mace-and-shield body (CalcPlrItemVals).
	//
	// The other hand still takes only a shield: the paste rules put a second weapon in place of the first
	// for every class but the Bard (inv.cpp's pasteIntoSelectedHand), so this opens no dual-wield.
	if (item._iLoc != ILOC_TWOHAND || item._iClass != ICLASS_WEAPON || item._itype == ItemType::Bow)
		return false;
	return PassiveSlotOf(player, Skill::HeavenlyStrength) >= 0;
}

void EnforceTwoHandedGrip(Player &player)
{
	// A weapon that needs both hands - GetItemLocation says so, so Heavenly Strength and the Barbarian's
	// grip are already answered - with anything in the other hand is a pair this hero may not keep. It
	// arises only when a rule changes under an equipped pair: the bow beside a shield that v1.11.047-049
	// allowed. The other hand's item leaves the way it does when the passive is taken out.
	const Item &left = player.InvBody[INVLOC_HAND_LEFT];
	const Item &right = player.InvBody[INVLOC_HAND_RIGHT];
	if (left.isEmpty() || right.isEmpty())
		return;
	const bool leftNeedsBoth = left._iClass == ICLASS_WEAPON && player.GetItemLocation(left) == ILOC_TWOHAND;
	const bool rightNeedsBoth = right._iClass == ICLASS_WEAPON && player.GetItemLocation(right) == ILOC_TWOHAND;
	if (leftNeedsBoth || rightNeedsBoth)
		ReleaseHeavenlyGrip(player);
}

bool SetPassiveSlot(Player &player, int slot, Skill skill)
{
	if (slot < 0 || slot >= static_cast<int>(PassiveSlotCount))
		return false;
	if (player._pLevel < PassiveSlotRequiredLevel(slot))
		return false;
	if (!IsPassiveSkillRow(skill))
		return false;
	if (GetClassTreeSkillData(skill).heroClass != player._pClass)
		return false;
	if (!IsClassTreeSkillUnlocked(player, skill))
		return false;
	// Already somewhere else: refused rather than moved. Silently emptying another slot is the kind
	// of helpfulness that reads as a bug when the slot you were not looking at goes dark.
	const int existing = PassiveSlotOf(player, skill);
	if (existing >= 0 && existing != slot)
		return false;
	// 0xFF is the empty sentinel, so an index that reached it would read back as "no passive here".
	// Cannot happen at 64 skills per class; asserted so it cannot start happening quietly either.
	static_assert(MaxSkillsPerClass < 0xFF,
	    "a class-relative index can now collide with the empty-slot sentinel");
	// Heavenly Strength leaving this slot takes its grip with it - see ReleaseHeavenlyGrip.
	if (PassiveInSlot(player, slot) == Skill::HeavenlyStrength && skill != Skill::HeavenlyStrength)
		ReleaseHeavenlyGrip(player);
	player._pPassiveSlots[slot] = static_cast<uint8_t>(ClassTreeIconIndex(skill));
	ScheduleAutoSaveForSkillChange();
	return true;
}

bool ClearPassiveSlot(Player &player, int slot)
{
	if (slot < 0 || slot >= static_cast<int>(PassiveSlotCount))
		return false;
	if (player._pPassiveSlots[slot] == 0xFF)
		return false;
	// Heavenly Strength leaving takes its grip with it - see ReleaseHeavenlyGrip.
	if (PassiveInSlot(player, slot) == Skill::HeavenlyStrength)
		ReleaseHeavenlyGrip(player);
	player._pPassiveSlots[slot] = 0xFF;
	ScheduleAutoSaveForSkillChange();
	return true;
}

Skill GetActiveClassAura(const Player &player)
{
	const auto skill = static_cast<Skill>(player._pOracoolActiveAura);
	if (skill > Skill::LAST)
		return Skill::None;
	const ClassTreeSkillData &data = GetClassTreeSkillData(skill);
	if (data.kind != Kind::Aura)
		return Skill::None;
	// A CORPSE HAS NO AURA. Audit finding, 2026-08-26: the class-tree tick runs unconditionally, so
	// Prayer, Melody of Life and the Healing Mantra went on regenerating a dead player's zero hit
	// points while they lay in PM_DEATH, and Sanctuary, Conviction, the ground ring and the looping
	// audio all stayed live over the body.
	//
	// Answered HERE rather than at each of those, which is the point: this is the one function all
	// of them ask, so one guard suspends the whole aura - its bonuses, its field, its picture and
	// its sound - for exactly as long as the player is down. Nothing is cleared, so it comes back
	// by itself on revival; the aura is suspended, not forgotten.
	// Death MODE is the unambiguous signal and comes first. The zero-health test is a second net for
	// the tick or two between the blow landing and StartPlayerKill running - and it is qualified by
	// _pMaxHP, because "no health left" and "health never set up" are not the same state and only
	// one of them is death. Reading them as the same is what a first cut of this guard did, and it
	// declared every test fixture a corpse.
	if (player._pmode == PM_DEATH)
		return Skill::None;
	if (player._pMaxHP > 0 && player._pHitPoints <= 0)
		return Skill::None;
	// The burning aura is persisted as an ABSOLUTE ClassTreeSkill value (hero_chunks writes the raw
	// byte), and absolute values move whenever a class EARLIER in the enum gains rows. The Passive
	// Skills page did exactly that on 2026-08-25, so a Bard or Monk saved with a song or a mantra
	// lit now decodes to some other class's row at that number.
	//
	// This is the guard that makes that safe rather than wrong: an aura that is not this character's
	// is no aura at all. The cost is that such a hero comes back with the aura OUT, which is one
	// click to restore; without the guard they would come back with somebody else's aura burning.
	//
	// The real fix is to persist the class-RELATIVE index, which is stable under exactly this kind
	// of growth. That is a chunk change and is on the pipeline; this is not a substitute for it.
	if (data.heroClass != player._pClass)
		return Skill::None;
	return skill;
}

bool ToggleClassAura(Player &player, Skill skill)
{
	if (skill > Skill::LAST || GetClassTreeSkillData(skill).kind != Kind::Aura)
		return false;
	if (!IsClassTreeSkillUnlocked(player, skill))
		return false;
	const bool switchingOff = GetActiveClassAura(player) == skill;
	// An aura with nothing invested has no strength to give, so lighting it would be a no-op that
	// LOOKED like it worked. Switching one off is always allowed.
	if (!switchingOff && ClassTreeInvestment(player, skill) <= 0)
		return false;
	player._pOracoolActiveAura = static_cast<uint16_t>(switchingOff ? Skill::None : skill);
	// The aura IS the right button's setting, so lighting one clears whatever skill was readied
	// there. See ClearClassAuraForRightButton for the other half and the reasoning.
	if (!switchingOff) {
		player._pRSpell = SpellID::Invalid;
		player._pRSplType = SpellType::Invalid;
	}
	if (&player == MyPlayer) {
		const char *name = GetClassTreeSkillData(skill).name;
		LogEvent(switchingOff ? fmt::format("{:s} fades", std::string(_(name)))
		                      : fmt::format("{:s} burns", std::string(_(name))),
		    UiFlags::ColorWhitegold);
		// The persistent cue. StartClassAuraLoop stops whatever was running first, so switching
		// straight from one aura to another is atomic in the order the sound package asks for: old
		// loop down, old stop cue, new start cue, new loop up.
		SetAuraLoop(switchingOff ? Skill::None : skill);
	}
	// Same responsibility as ClearClassAuraForRightButton: lighting or dousing an aura changes what
	// the character's totals should be, so this function makes that true rather than trusting the
	// caller. The skill picker's own CalcPlrInv is now redundant and harmless - left where it is,
	// because removing a correct recalculation to save a few microseconds is how the asymmetry that
	// caused this bug gets recreated.
	oracool::ScheduleAutoSaveForSkillChange();
	CalcPlrInv(player, false);
	return true;
}

void ClearClassAuraForRightButton(Player &player)
{
	if (GetActiveClassAura(player) == Skill::None)
		return;
	player._pOracoolActiveAura = static_cast<uint16_t>(Skill::None);
	if (&player == MyPlayer)
		SetAuraLoop(Skill::None);
	// The bonuses go out with the light. Audit finding, 2026-08-26: this cleared the STATE and the
	// SOUND and left the cached totals alone, and all four callers - the skill picker, the
	// Abilities window, the spell list and the hotkey path - forgot to recalculate. So readying
	// Firebolt over a lit Might put the ring out, stopped the hum, and left the damage bonus
	// running until something unrelated happened to recalculate.
	//
	// Done HERE rather than at the four call sites, which is the whole lesson: ToggleClassAura's
	// one caller remembered and these four did not, and the next path added would have been a coin
	// flip. A function that puts an aura out is responsible for the aura being out.
	oracool::ScheduleAutoSaveForSkillChange();
	CalcPlrInv(player, false);
	// No "fades" line here. The player is looking at the skill they just readied, and the aura going
	// out is the visible half of that one action rather than a second event.
}

void ApplyClassTreeToTotals(const Player &player, ItemBonusTotals &totals)
{
	if (!ClassHasTree(player._pClass))
		return;

	if (const Skill aura = GetActiveClassAura(player);
	    aura != Skill::None && IsClassTreeSkillUnlocked(player, aura)) {
		ApplyAura(aura, ClassTreeInvestment(player, aura), totals);
		// An aura's level-up stat burns with it, and goes out with it.
		if (GetClassTreeSkillData(aura).implemented)
			ApplyLevelUpStat(aura, ClassTreeInvestment(player, aura), totals);
		// The two RfA-12 auras whose number is the character's own: Endurance is a share of the base life,
		// and Symphony of War lends half of every other Melody song the Bard has learned.
		if (aura == Skill::Endurance && GetClassTreeSkillData(aura).implemented) {
			const int p = ClassTreeInvestment(player, aura);
			totals.hitPoints += player._pMaxHPBase * std::min(10 + 2 * (p - 1), 60) / 100;
		}
		if (aura == Skill::SymphonyOfWar && GetClassTreeSkillData(aura).implemented) {
			const Skill first = FirstSkillOf(player._pClass);
			for (size_t i = 0; i < MaxSkillsPerClass; i++) {
				const auto song = static_cast<Skill>(static_cast<size_t>(first) + i);
				if (song > Skill::LAST || song == Skill::SymphonyOfWar)
					continue;
				const ClassTreeSkillData &data = GetClassTreeSkillData(song);
				if (data.heroClass != player._pClass)
					break;
				if (data.page != 0 || data.kind != Kind::Aura || !data.implemented || !IsClassTreeSkillUnlocked(player, song))
					continue;
				if (const int q = ClassTreeInvestment(player, song); q > 0)
					ApplyAura(song, (q + 1) / 2, totals);
			}
		}
	}

	// Two kinds of passive, and they turn on for different reasons.
	//
	// A Diablo II passive - the Barbarian's masteries, the Rogue's Passive & Magic page - is always
	// on once BOUGHT, and scales with the points in it.
	//
	// A Passive Skills page row is bought with nothing and scales with nothing. It is on if and only
	// if it sits in one of the four slots, which is the whole of that page's choice. 42 of the 110 are
	// built now and the rest are inert; this gate is what keeps a built one from applying from the
	// grid - only a slotted row counts.
	const Skill first = FirstSkillOf(player._pClass);
	for (size_t i = 0; i < MaxSkillsPerClass; i++) {
		const auto skill = static_cast<Skill>(static_cast<size_t>(first) + i);
		if (skill > Skill::LAST)
			break;
		const ClassTreeSkillData &data = GetClassTreeSkillData(skill);
		if (data.heroClass != player._pClass)
			break;
		if (!data.implemented || !IsClassTreeSkillUnlocked(player, skill))
			continue;
		// An ACTIVE's level-up stat (RfA-12) is the character's for as long as points sit in it - it is
		// what learning the skill made of you, not a buff that runs while the skill is cast.
		if (data.kind == Kind::Active) {
			ApplyLevelUpStat(skill, ClassTreeInvestment(player, skill), totals);
			continue;
		}
		if (data.kind != Kind::Passive)
			continue;
		if (IsPassiveSkillRow(skill)) {
			if (PassiveSlotOf(player, skill) >= 0)
				ApplyPassive(player, skill, 1, totals);
			continue;
		}
		const int points = ClassTreeInvestment(player, skill);
		if (points > 0) {
			ApplyPassive(player, skill, points, totals);
			ApplyLevelUpStat(skill, points, totals);
		}
	}
}

namespace {

/** A slow on a player: the deeper of two overlapping ones wins, and it runs down by the tick. */
struct MovementSlow {
	int ticksLeft = 0;
	int percent = 0;
};
std::array<MovementSlow, MAX_PLRS> MovementSlows;

/**
 * @brief The part of a tick each player's last stride could not show, in thousandths - StrideTicksFor
 * carries it into the next stride so the average pace is the exact percentage (2026-09-12).
 */
std::array<int, MAX_PLRS> StrideCarry {};

} // namespace

int MovementSpeedBonusPercent(const Player &player)
{
	// Signed since the curse (2026-09-07): a cursed ring's -15 is a bonus of -15, and the sheet
	// shows 85%. Plus the passives' bursts of speed (2026-09-14), which come and go with no recalc.
	return player._pIMoveSpeed + PassiveMoveSpeedBonus(player);
}

int ThornsReturnPercentAt(int points)
{
	return points <= 0 ? 0 : 25 + 10 * (points - 1);
}

int ThornsReturnPercent(const Player &player)
{
	// Iron Maiden (2026-09-14) returns its share with or without the aura.
	const int maiden = PassiveThornsPercent(player);
	if (GetActiveClassAura(player) != Skill::Thorns || !IsClassTreeSkillUnlocked(player, Skill::Thorns))
		return maiden;
	return ThornsReturnPercentAt(ClassTreeInvestment(player, Skill::Thorns)) + maiden;
}

int CleansingShortenPercentAt(int points)
{
	return points <= 0 ? 0 : std::min(20 + 5 * (points - 1), 90);
}

int CleansingShortenPercent(const Player &player)
{
	if (GetActiveClassAura(player) != Skill::Cleansing || !IsClassTreeSkillUnlocked(player, Skill::Cleansing))
		return 0;
	return CleansingShortenPercentAt(ClassTreeInvestment(player, Skill::Cleansing));
}

int PlayerSlowPercent(const Player &player)
{
	const MovementSlow &slow = MovementSlows[player.getId()];
	return slow.ticksLeft > 0 ? slow.percent : 0;
}

void SlowPlayer(const Player &player, int ticks, int percent)
{
	// Cleansing (2026-09-12): under it, a slow or a chill wears off sooner.
	// Juggernaut (2026-09-14): the Barbarian's own half.
	ticks = ticks * (100 - std::max({ CleansingShortenPercent(player), Rfa12SlowShortenPercent(player), PassiveSlowShortenPercent(player) })) / 100;
	if (ticks <= 0)
		return;
	MovementSlow &slow = MovementSlows[player.getId()];
	slow.ticksLeft = std::max(slow.ticksLeft, ticks);
	slow.percent = std::max(slow.percent, std::clamp(percent, 0, 90));
}

void TickMovementSlow(const Player &player)
{
	MovementSlow &slow = MovementSlows[player.getId()];
	if (slow.ticksLeft > 0 && --slow.ticksLeft == 0)
		slow.percent = 0;
}

void ClearMovementSlows()
{
	MovementSlows.fill(MovementSlow {});
	StrideCarry.fill(0);
}

void ClearPlayerSlow(const Player &player)
{
	MovementSlows[player.getId()] = MovementSlow {};
}

int MovementSpeedPercent(const Player &player)
{
	// 100 is a plain walk. Abilities and items add to it; a slow (cold, a curse) takes from it.
	// Floored at 10 so a stack of slows never reads as standing still - the feet floor separately.
	return std::max(100 + MovementSpeedBonusPercent(player) - PlayerSlowPercent(player), 10);
}

int StrideTicksFor(int percent, int &carryMilliTicks)
{
	// A plain walk is 10 ticks a stride, so a stride at P% is 1000 / P ticks. Whole ticks only reach the
	// feet, so the fraction is carried into the next stride: over any run of strides the pace is the
	// exact percentage (user, 2026-09-12: "make vigor +5% faster walk per level for every level"). It
	// stepped by thresholds before - +10, 125, 140, 160 - and the run was the ceiling.
	const int total = 1000000 / std::max(percent, 10) + carryMilliTicks;
	int ticks = total / 1000;
	carryMilliTicks = total % 1000;
	if (ticks < MinStrideTicks || ticks > MaxStrideTicks) {
		ticks = std::clamp(ticks, MinStrideTicks, MaxStrideTicks);
		carryMilliTicks = 0; // a clamped stride owes nothing to the next
	}
	return ticks;
}

int8_t WalkFrameSkipFor(const Player &player)
{
	// The walk animation runs its 8 frames over 10 ticks (StartWalkAnimation's -2); each skipped frame
	// is one tick off the stride, so the skip is 8 less the stride's ticks: -2 is the walk, 2 the run,
	// 4 the fastest stride (250%, every other frame shown). Called once per stride, which is what
	// lets it carry the fraction.
	return static_cast<int8_t>(8 - StrideTicksFor(MovementSpeedPercent(player), StrideCarry[player.getId()]));
}

int CastFrameSkip(int castFrame, int fasterCastPercent)
{
	if (castFrame <= 1 || fasterCastPercent <= 0)
		return 0;
	// castFrame * 100 / (100 + X), rounded to the nearest tick.
	const int divisor = 100 + fasterCastPercent;
	const int ticks = (2 * castFrame * 100 + divisor) / (2 * divisor);
	return std::clamp(castFrame - ticks, 0, castFrame - 1);
}

bool IsClassTreeRunActive(const Player &player)
{
	if (!ClassHasTree(player._pClass))
		return false;
	// The Paladin's Vigor left this list on 2026-09-07: it is a movement-speed PERCENTAGE now, in the
	// aura totals, and WalkFrameSkipFor reads it with the items' affixes.
	if (player._pClass == HeroClass::Barbarian) {
		return ClassTreeInvestment(player, Skill::IncreasedSpeed) > 0
		    && IsClassTreeSkillUnlocked(player, Skill::IncreasedSpeed);
	}
	if (player._pClass == HeroClass::Bard) {
		const Skill song = GetActiveClassAura(player);
		return song == Skill::SongOfSwiftness && ClassTreeInvestment(player, song) > 0
		    && IsClassTreeSkillUnlocked(player, song);
	}
	if (player._pClass == HeroClass::Monk) {
		// Flowing Step is a passive like the Barbarian's Increased Speed, not a held stance, so the
		// points alone carry it. Its other half - a chance to evade a blow outright - is the part
		// this engine has no roll for, and the row says so.
		return ClassTreeInvestment(player, Skill::FlowingStep) > 0
		    && IsClassTreeSkillUnlocked(player, Skill::FlowingStep)
		    || PassiveRunActive(player);
	}
	return false;
}

void ProcessClassTreeTick(Player &player)
{
	if (!ClassHasTree(player._pClass))
		return;

	// The aura's SOUND follows its live state, which death now suspends (see GetActiveClassAura).
	// Without this the loop would keep playing over the body: the guard there makes the aura report
	// as None, and a thing that is already None never asks anybody to stop its loop.
	//
	// Edge-triggered against the last state rather than called every tick, or restarting the loop
	// would retrigger the cue sixty times a second.
	if (&player == MyPlayer) {
		// Re-asserted every tick; SetAuraLoop is idempotent, so only a real change is heard.
		SetAuraLoop(GetActiveClassAura(player));
	}

	// Both regenerations are whole points per tick against the <<6 fixed point the life and mana
	// fields use, so one invested point is a trickle rather than a heal button.
	const Skill aura = GetActiveClassAura(player);
	if (aura != Skill::None && IsClassTreeSkillUnlocked(player, aura)) {
		const int p = ClassTreeInvestment(player, aura);
		// Melody of Life and Inspiration are the Bard's counterparts of Prayer and Meditation -
		// same channel, same trickle, so they share the branch rather than repeating it.
		const bool healing = aura == Skill::Prayer || aura == Skill::MelodyOfLife
		    || aura == Skill::HealingMantra;
		const bool restoring = aura == Skill::Meditation || aura == Skill::Inspiration;
		if (p > 0 && healing && player._pHitPoints < player._pMaxHP) {
			const int heal = Scaled(p, 2, 2);
			player._pHitPoints = std::min(player._pHitPoints + heal, player._pMaxHP);
			player._pHPBase = std::min(player._pHPBase + heal, player._pMaxHPBase);
			RedrawComponent(PanelDrawComponent::Health);
		}
		if (p > 0 && restoring && player._pMana < player._pMaxMana
		    && HasNoneOf(player._pIFlags, ItemSpecialEffect::NoMana)) {
			const int gain = Scaled(p, 2, 2);
			player._pMana = std::min(player._pMana + gain, player._pMaxMana);
			player._pManaBase = std::min(player._pManaBase + gain, player._pMaxManaBase);
			RedrawComponent(PanelDrawComponent::Mana);
		}
	}

	// Phase 3.4: the outward half of the aura system. Auras that must PUSH run here; auras a monster
	// can simply be ASKED about (Conviction) are answered at the point of use instead, because a
	// query cannot go stale. See oracool/aura_field.h.
	ProcessOutwardAura(player);
	ProcessPassivesTick(player);
	ProcessRageTick(player);
	ProcessEssenceTick(player);
	ProcessCursesTick(player);
	ProcessRfa12Tick(player);
	ProcessWarcriesTick(player);
	TickMovementSlow(player);

	// The Sorceress's Warmth is a passive, so it needs no activation - the points alone. But a
	// corpse regenerates nothing (audit, 2026-08-26): the aura guard in GetActiveClassAura does not
	// reach this branch, because Warmth is not an aura and never asks it, so Warmth alone went on
	// refilling a dead Sorceress's mana.
	if (player._pHitPoints > 0 && player._pmode != PM_DEATH
	    && player._pClass == HeroClass::Sorcerer && IsClassTreeSkillUnlocked(player, Skill::Warmth)
	    && player._pMana < player._pMaxMana && HasNoneOf(player._pIFlags, ItemSpecialEffect::NoMana)) {
		const int p = ClassTreeInvestment(player, Skill::Warmth);
		if (p > 0) {
			const int gain = Scaled(p, 2, 2);
			player._pMana = std::min(player._pMana + gain, player._pMaxMana);
			player._pManaBase = std::min(player._pManaBase + gain, player._pMaxManaBase);
			RedrawComponent(PanelDrawComponent::Mana);
		}
	}
}

size_t BuildClassTreePage(HeroClass heroClass, int page, Skill *out)
{
	const Skill first = FirstSkillOf(heroClass);
	if (first == Skill::None)
		return 0;
	size_t count = 0;
	for (int tier = 0; tier < static_cast<int>(std::size(TierLevels)); tier++) {
		for (int column = 0; column < 3; column++) {
			for (size_t i = 0; i < MaxSkillsPerClass; i++) {
				const size_t index = static_cast<size_t>(first) + i;
				if (index >= ClassTreeSkillCount)
					break;
				const ClassTreeSkillData &data = Skills[index];
				if (data.heroClass != heroClass)
					break;
				// A row whose slot is a BOOK spell takes no points (user rule, 2026-08-20:
				// "Spells cant be affected by skill points, only by books"), and it used to be
				// filtered out here entirely - which left the Sorceress's Lightning and Fire
				// pages nearly empty, since thirteen of her rows are book spells. The user's
				// word for the result was "orphaned" (2026-09-03), and the ask was to
				// "duplicate coresponding legacy spells also throughout these screens".
				//
				// So they are LISTED again, and still take no points: the page shows the whole
				// arsenal, says which of it you have learned and at what level, and lets you
				// bind it to a button, while the book stays the only thing that raises it. See
				// DrawTreeCell for the state those cells draw and the click handler for what a
				// click on one says.
				if (data.page == page && data.tier == tier && data.column == column)
					out[count++] = static_cast<Skill>(index);
			}
		}
	}
	return count;
}


std::string ClassTreeLockReason(const Player &player, Skill skill)
{
	if (skill > Skill::LAST)
		return {};
	const ClassTreeSkillData &data = GetClassTreeSkillData(skill);
	if (data.heroClass != player._pClass)
		return {};
	if (IsClassTreeSkillUnlocked(player, skill))
		return {};

	// A Passive Skills row is gated by its OWN level, never by its tier, so answering from the tier
	// here would be wrong in both directions - and worse than wrong, SILENT. A tier-0 passive needs
	// level 2 while its tier needs 1, so `_pLevel < tierLevel` is false and the function used to
	// fall through and return nothing at all: a locked row that refuses a click and says why it
	// refused it, except it does not. That is the exact failure this function was written for.
	if (IsPassiveSkillRow(skill)) {
		return fmt::format(fmt::runtime(_("{:s} is learned at level {:d}.")),
		    _(data.name), PassiveSkillRequiredLevel(skill));
	}

	// The borrowed rows first, because their requirements are the ones that surprise: they are NOT
	// the tier's, and the tier is what the page's own layout implies. See IsClassTreeSkillUnlocked.
	if (const std::optional<PaladinSkill> borrowed = BorrowedPaladinSkill(skill); borrowed.has_value()) {
		const PaladinSkillData &pal = GetPaladinSkillData(*borrowed);
		const bool needsLevel = player._pLevel < pal.minLevel;
		const bool needsShield = pal.requiresShield && !HasShieldEquipped(player);
		if (needsLevel && needsShield)
			return fmt::format(fmt::runtime(_("{:s} needs level {:d} and a shield.")), _(data.name), pal.minLevel);
		if (needsShield)
			return fmt::format(fmt::runtime(_("{:s} needs a shield.")), _(data.name));
		if (needsLevel)
			return fmt::format(fmt::runtime(_("{:s} needs level {:d}.")), _(data.name), pal.minLevel);
		// Unlocked by both and still locked means the class check above - which cannot happen here.
		return {};
	}

	const int tierLevel = ClassTreeTierMinLevel(data.tier);
	if (player._pLevel < tierLevel)
		return fmt::format(fmt::runtime(_("{:s} needs level {:d}.")), _(data.name), tierLevel);
	return {};
}

std::string ClassTreeEffectLine(const Player &player, Skill skill, bool withNext)
{
	const ClassTreeSkillData &data = GetClassTreeSkillData(skill);
	// A Passive Skills row has no points and no rank, so the usual two lines would both be lies -
	// "Points: 0 of 1" invites a click that is refused, and the tier is not its gate.
	if (IsPassiveSkillRow(skill)) {
		const int required = PassiveSkillRequiredLevel(skill);
		std::string out = player._pLevel >= required
		    ? std::string(_("Learned"))
		    : fmt::format(fmt::runtime(_("Learned at level {:d}")), required);
		const int slot = PassiveSlotOf(player, skill);
		if (slot >= 0)
			out += "\n" + fmt::format(fmt::runtime(_("Active - slot {:d}")), slot + 1);
		else if (player._pLevel >= required)
			out += "\n" + std::string(_("Inactive - not in a slot"));
		if (!data.implemented)
			out += "\n" + std::string(_("No effect yet"));
		return out;
	}
	// A BOOK row reports the book's level, not a point count. "Points: 0 of 20" on Fire Bolt would
	// invite a click that is always refused and describe a store this row does not use.
	if (IsClassTreeRowRetiredAsSpell(skill)) {
		const SpellID spell = ClassTreeSpellId(skill);
		const int level = player.GetSpellLevel(spell);
		std::string out = level > 0
		    ? fmt::format(fmt::runtime(_("Spell level {:d}")), level)
		    : std::string(_("Not learned"));
		out += "\n" + std::string(_("Raised by books, not by skill points"));
		return out;
	}
	const int p = ClassTreeInvestment(player, skill);
	const int maxRank = ClassTreeMaxRank(skill);

	// THE DIABLO II SHAPE (user, 2026-09-05: "look at diablo 2 description theme. we want same theme
	// when hovering over a skill in the abilities windows"): a "Current Skill Level: N" heading with
	// this rank's numbers under it, a gap, then "Next Level" with the next rank's numbers - the same
	// lines, so the eye compares them row for row. Before this the block was "Points: 1 of 20 / Next
	// point requires level 12 / Now: ... / Next point: ...", which said the same things in a shape
	// no player had seen before.
	//
	// WHAT A RANK GRANTS, derived by RUNNING the effect, never by a second table (user, 2026-08-31:
	// "compare yours to D2. D2 is more informative"). An aura or passive goes through the same
	// ApplyAura / ApplyPassive the game runs, and DescribeBonusTotals names every field that moved -
	// so the tooltip cannot promise a bonus the code does not apply. That failure has happened three
	// times in this project (see oracool/unique_affixes.h) and a tooltip is the worst place for it.
	// An ACTIVE carries a SpellID, and its rank is its spell level (Player::GetSpellLevel folds the
	// investment in), so its numbers are the spell side's: damage and mana at that level, from the
	// same two functions the Spells sheet quotes.
	//
	// Rows whose effect the struct cannot carry - flags, procs, bespoke behaviour - produce no lines
	// and keep their authored sentence. Silence stays the honest answer.
	const auto rankLines = [&](int points) {
		std::string text;
		const auto line = [&text](const std::string &s) {
			if (!text.empty())
				text += '\n';
			text += s;
		};
		if (data.kind == Kind::Active) {
			const SpellID spell = ClassTreeSpellId(skill);
			if (IsValidSpell(spell)) {
				const int at = std::max(points, 1);
				int min = -1;
				int max = -1;
				GetDamageAmtAtLevel(spell, at, &min, &max);
				if (min != -1)
					line(fmt::format(fmt::runtime(_("Damage: {:d} - {:d}")), min, max));
				if (const std::string resource = SkillResourceLine(player, spell, at); !resource.empty())
					line(resource); // Mana Cost, or the Barbarian's Rage Cost / Generates
				const std::string facts = SkillFactsAt(spell, at); // strikes, range, duration, stun, chance - the module's own numbers
				if (!facts.empty())
					line(facts);
			}
			if (data.implemented) {
				const std::string stat = LevelUpStatLine(skill, std::max(points, 1));
				if (!stat.empty())
					line(stat);
			}
			return text;
		}
		if (!data.implemented)
			return text;
		ItemBonusTotals totals {};
		if (data.kind == Kind::Aura)
			ApplyAura(skill, points, totals);
		else
			ApplyPassive(player, skill, points, totals);
		const std::string bonuses = DescribeBonusTotals(totals, "\n");
		if (!bonuses.empty())
			line(bonuses);
		const std::string stat = LevelUpStatLine(skill, points);
		if (!stat.empty())
			line(stat);
		if (data.kind == Kind::Aura) {
			// What an aura does OFF the sheet - a pulse, a return, a shortening - which the totals cannot
			// carry (2026-09-12).
			const std::string field = AuraFieldFactsAt(skill, points);
			if (!field.empty())
				line(field);
			// The reach only where it reaches something: it used to sit on Might and the resists too, where
			// it meant nothing (audit, 2026-09-12).
			if (AuraReachesMonsters(skill))
				line(fmt::format(fmt::runtime(_("Radius: {:d} tiles")), AuraFieldRadius(skill, points)));
		}
		return text;
	};

	std::string out;
	const auto add = [&out](const std::string &s) {
		if (!out.empty())
			out += '\n';
		out += s;
	};
	if (p > 0) {
		add(fmt::format(fmt::runtime(_("Current Skill Level: {:d}")), p));
		const std::string now = rankLines(p);
		if (!now.empty())
			add(now);
	} else {
		add(std::string(_("Not learned")));
	}
	// The next rank, when asked for (the Abilities window asks; the picker shows the current rank
	// only - user, 2026-09-05: "the other places can be truncated to Name or Name, Current Level
	// Stats"). Its level requirement is the NEXT point's, which climbs with every point spent - the
	// Rule of Rangs (see CanInvestClassTreePoint) - so it is the one number a player standing in
	// front of the button can act on.
	if (withNext) {
		if (p >= maxRank) {
			add(std::string(_("Fully invested")));
		} else {
			out += "\n"; // the gap D2 leaves between the two blocks
			add(std::string(p == 0 ? _("First Level") : _("Next Level")));
			add(fmt::format(fmt::runtime(_("Requires level {:d}")), RankRequiredLevel(ClassTreeTierMinLevel(data.tier), p + 1)));
			const std::string next = rankLines(p + 1);
			if (!next.empty())
				add(next);
		}
	}
	if (!data.implemented)
		add(std::string(_("No effect yet")));
	if (skill == Skill::Conviction && data.implemented) {
		add(p >= ConvictionBreaksImmunityAt
		        ? std::string(_("Breaks immunities"))
		        : fmt::format(fmt::runtime(_("Breaks immunities at {:d} points")), ConvictionBreaksImmunityAt));
	}
	return out;
}

} // namespace devilution::oracool
