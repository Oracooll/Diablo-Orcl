#include "oracool/class_tree.h"

#include "oracool/class_skills.h" // RefreshInnateSpells - a point spent or refunded changes what the character HAS

#include <algorithm>
#include <array>
#include <cassert>
#include <optional>

#include <fmt/format.h>

#include "engine/backbuffer_state.hpp"
#include "inv.h"
#include "oracool/auto_save.h"
#include "oracool/aura_field.h"
#include "oracool/event_log.h"
#include "oracool/paladin_skills.h"
#include "oracool/skill_points.h"
#include "oracool/skill_sounds.h"
#include "oracool/spell_ranks.h" // the Rule of Rangs
#include "oracool/stat_sheet.h"
#include "player.h"
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
	{ N_("Sacrifice"), N_("Strike for heavy bonus damage and wound yourself for a share of it. Not yet built."),
	    Pal, 0, 0, 0, Kind::Active, SpellID::Invalid, false },
	{ N_("Smite"), N_("Bash with your shield: it always connects and briefly stuns. A shield is mandatory."),
	    Pal, 0, 0, 1, Kind::Active, SpellID::ShieldBash, true },
	{ N_("Holy Bolt"), N_("A bolt of holy energy that sears the undead. Withdrawn: it collided with this engine's own Holy Bolt spell."),
	    Pal, 0, 0, 2, Kind::Active, SpellID::Invalid, false },
	// Matches paladin_skills.cpp word for word on the numbers, deliberately: two windows describing
	// one skill differently is worse than either being wrong alone, and this row has drifted twice.
	{ N_("Zeal"), N_("Strike several times in one furious burst. Skill levels 1, 3 and 5 each add a strike, and every skill level adds +1% chance to hit."),
	    Pal, 0, 1, 0, Kind::Active, SpellID::Zeal, true },
	{ N_("Charge"), N_("Rush an enemy and land a running blow."),
	    Pal, 0, 1, 1, Kind::Active, SpellID::Charge, true },
	{ N_("Vengeance"), N_("Adds fire, lightning and cold damage to your attack. Not yet built; this engine also has no cold."),
	    Pal, 0, 2, 0, Kind::Active, SpellID::Invalid, false },
	{ N_("Blessed Hammer"), N_("Looses a spinning hammer that wheels outward through anything in its path."),
	    Pal, 0, 3, 2, Kind::Active, SpellID::BlessedHammer, true },
	// Corrected 2026-08-16: this row used to claim "no charmed-monster state exists", which was
	// simply wrong - this engine's Berserk sets MFLAG_GOLEM on the target, making it fight for the
	// player, which IS conversion. Found while wiring the Bard's Charm onto the same spell.
	{ N_("Conversion"), N_("Turns an enemy to your side. Withdrawn pending design work: its Berserk behaviour was wrong."),
	    Pal, 0, 4, 1, Kind::Active, SpellID::Invalid, false },
	{ N_("Fist of the Heavens"), N_("Calls down a bolt from the sky, which bursts into holy energy where it lands."),
	    Pal, 0, 5, 2, Kind::Active, SpellID::FistOfTheHeavens, true },
	// --- Offensive Auras ---
	{ N_("Might"), N_("Increases the damage you deal."), Pal, 1, 0, 0, Kind::Aura, SpellID::Invalid, true },
	{ N_("Holy Fire"), N_("Wreathes your weapon in flame, adding fire damage to every blow."),
	    Pal, 1, 1, 1, Kind::Aura, SpellID::Invalid, true },
	{ N_("Thorns"), N_("Returns damage to whatever strikes you. This engine's thorns is a flat return, so points light it rather than growing it."),
	    Pal, 1, 1, 2, Kind::Aura, SpellID::Invalid, true },
	{ N_("Blessed Aim"), N_("Steadies your hand, raising your chance to hit."),
	    Pal, 1, 2, 0, Kind::Aura, SpellID::Invalid, true },
	{ N_("Concentration"), N_("Raises damage and steadies you against interruption."),
	    Pal, 1, 3, 0, Kind::Aura, SpellID::Invalid, true },
	{ N_("Holy Freeze"), N_("Chills nearby enemies and adds cold damage. Inert: this engine has no cold and no slow."),
	    Pal, 1, 3, 1, Kind::Aura, SpellID::Invalid, false },
	{ N_("Holy Shock"), N_("Charges your weapon, adding lightning damage to every blow."),
	    Pal, 1, 4, 1, Kind::Aura, SpellID::Invalid, true },
	{ N_("Sanctuary"), N_("Hallows the ground you stand on: nearby undead break and flee from you. Champions are too proud to run."),
	    Pal, 1, 4, 2, Kind::Aura, SpellID::Invalid, true },
	{ N_("Fanaticism"), N_("Drives you to strike faster, harder and truer."),
	    Pal, 1, 5, 0, Kind::Aura, SpellID::Invalid, true },
	{ N_("Conviction"), N_("Strips the resistances of every enemy near you, and at five points begins to break their immunities down into mere resistances."),
	    Pal, 1, 5, 2, Kind::Aura, SpellID::Invalid, true },
	// --- Defensive Auras ---
	{ N_("Prayer"), N_("Mends your wounds steadily as you walk."), Pal, 2, 0, 0, Kind::Aura, SpellID::Invalid, true },
	{ N_("Resist Fire"), N_("Hardens you against fire."), Pal, 2, 0, 1, Kind::Aura, SpellID::Invalid, true },
	{ N_("Defiance"), N_("Raises your armour class."), Pal, 2, 1, 0, Kind::Aura, SpellID::Invalid, true },
	{ N_("Resist Cold"), N_("Hardens you against cold. No cold exists here, so it wards against magic instead."),
	    Pal, 2, 1, 1, Kind::Aura, SpellID::Invalid, true },
	{ N_("Cleansing"), N_("Shortens poison and curses. Inert: this engine tracks no duration for either."),
	    Pal, 2, 2, 2, Kind::Aura, SpellID::Invalid, false },
	{ N_("Resist Lightning"), N_("Hardens you against lightning."), Pal, 2, 2, 1, Kind::Aura, SpellID::Invalid, true },
	{ N_("Vigor"), N_("Quickens your stride: you run instead of walking, wherever you are."),
	    Pal, 2, 3, 0, Kind::Aura, SpellID::Invalid, true },
	{ N_("Meditation"), N_("Restores your mana steadily as you walk."), Pal, 2, 4, 0, Kind::Aura, SpellID::Invalid, true },
	{ N_("Redemption"), N_("Consumes the fallen for life and mana. Inert: it needs the corpse-handling pass."),
	    Pal, 2, 5, 1, Kind::Aura, SpellID::Invalid, false },
	{ N_("Salvation"), N_("Wards you against fire, lightning and magic alike."),
	    Pal, 2, 5, 2, Kind::Aura, SpellID::Invalid, true },
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
	{ N_("Blessed Shield"), N_("Hurls your shield at a crowd, striking several of them before it returns. A shield is mandatory."),
	    Pal, 0, 3, 0, Kind::Active, SpellID::BlessedShield, true },

	// ---- Passive Skills (page 3) ----
	{ N_("Heavenly Strength"), N_("Bear a two-handed weapon in your main hand and a shield in the other. Not yet built."),
	    Pal, 3, 0, 0, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Fervor"), N_("One-handed weapons swing faster and your cooldowns come round sooner. Not yet built."),
	    Pal, 3, 0, 1, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Vigilant"), N_("Your wounds close faster and every blow that is not steel hurts less. Not yet built."),
	    Pal, 3, 0, 2, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Righteousness"), N_("Your opening strikes build wrath faster, and you hold more of it. Not yet built."),
	    Pal, 3, 1, 0, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Insurmountable"), N_("Every blow you turn aside feeds your wrath. Not yet built."),
	    Pal, 3, 1, 1, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Fanaticism"), N_("Your simplest attacks land faster than a measured swing would. Not yet built."),
	    Pal, 3, 1, 2, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Indestructible"), N_("Once a minute a killing blow leaves you standing, stronger and drinking life. Not yet built."),
	    Pal, 3, 2, 0, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Holy Cause"), N_("Your weapon bites deeper, and holy damage mends you as it burns. Not yet built."),
	    Pal, 3, 2, 1, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Wrathful"), N_("Spent wrath returns to you as life. Not yet built."),
	    Pal, 3, 2, 2, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Divine Fortress"), N_("The shield you hide behind becomes armour you wear. Not yet built."),
	    Pal, 3, 3, 0, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Lord Commander"), N_("Your mount, your bombardment and your phalanx all answer sooner and hit harder. Not yet built."),
	    Pal, 3, 3, 1, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Hold Your Ground"), N_("You no longer dodge at all, and block far more. Not yet built."),
	    Pal, 3, 3, 2, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Long Arm of the Law"), N_("Every law you declare holds its power longer. Not yet built."),
	    Pal, 3, 4, 0, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Iron Maiden"), N_("What strikes you is returned with far greater interest. Not yet built."),
	    Pal, 3, 4, 1, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Renewal"), N_("Each blow turned aside returns a measure of life. Not yet built."),
	    Pal, 3, 4, 2, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Finery"), N_("Every gem set into your gear lends you strength. Not yet built."),
	    Pal, 3, 5, 0, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Blunt"), N_("Justice and the blessed hammer fall heavier. Not yet built."),
	    Pal, 3, 5, 1, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Towering Shield"), N_("Every skill worked through your shield strikes harder and readies sooner. Not yet built."),
	    Pal, 3, 5, 2, Kind::Passive, SpellID::Invalid, false, 1 },
	// ======================= BARBARIAN =======================
	// --- Combat Skills ---
	{ N_("Bash"), N_("A heavy blow that knocks the target back. Not yet built."), Bar, 0, 0, 0, Kind::Active, SpellID::Invalid, false },
	{ N_("Leap"), N_("Vault over anything in the way. Not yet built: it needs new movement work."), Bar, 0, 1, 0, Kind::Active, SpellID::Invalid, false },
	{ N_("Double Swing"), N_("Strike with both weapons at once. Not yet built."), Bar, 0, 1, 1, Kind::Active, SpellID::Invalid, false },
	{ N_("Stun"), N_("A blow that leaves the target reeling. Not yet built."), Bar, 0, 2, 0, Kind::Active, SpellID::Invalid, false },
	{ N_("Double Throw"), N_("Hurl both thrown weapons at once. Inert: this engine has no thrown weapons."), Bar, 0, 2, 1, Kind::Active, SpellID::Invalid, false },
	{ N_("Leap Attack"), N_("Leap onto a distant enemy and strike on landing. Not yet built."), Bar, 0, 3, 0, Kind::Active, SpellID::Invalid, false },
	{ N_("Concentrate"), N_("A focused blow you cannot be jolted out of. Not yet built."), Bar, 0, 3, 1, Kind::Active, SpellID::Invalid, false },
	{ N_("Frenzy"), N_("Each kill drives the next blow faster. Not yet built."), Bar, 0, 4, 0, Kind::Active, SpellID::Invalid, false },
	{ N_("Whirlwind"), N_("Spin through a crowd striking everything. Not yet built: it needs new movement work."), Bar, 0, 5, 0, Kind::Active, SpellID::Invalid, false },
	{ N_("Berserk"), N_("Trade all defence for a devastating magical blow. Not yet built."), Bar, 0, 5, 1, Kind::Active, SpellID::Invalid, false },
	// --- Combat Masteries ---
	{ N_("Sword Mastery"), N_("Sharpens your aim and your blow with any sword held."), Bar, 1, 0, 0, Kind::Passive, SpellID::Invalid, true },
	{ N_("Axe Mastery"), N_("Sharpens your aim and your blow with any axe held."), Bar, 1, 0, 1, Kind::Passive, SpellID::Invalid, true },
	{ N_("Mace Mastery"), N_("Sharpens your aim and your blow with any mace or club held."), Bar, 1, 0, 2, Kind::Passive, SpellID::Invalid, true },
	{ N_("Pole Arm Mastery"), N_("Sharpens your aim and your blow with a staff - this engine's nearest pole arm."),
	    Bar, 1, 1, 0, Kind::Passive, SpellID::Invalid, true },
	{ N_("Throwing Mastery"), N_("Mastery of thrown weapons. Inert: this engine has none."), Bar, 1, 1, 1, Kind::Passive, SpellID::Invalid, false },
	{ N_("Spear Mastery"), N_("Mastery of spears. Inert: this engine has no spear type."), Bar, 1, 1, 2, Kind::Passive, SpellID::Invalid, false },
	{ N_("Increased Stamina"), N_("Lengthens your wind. Inert: this engine tracks no stamina."), Bar, 1, 2, 0, Kind::Passive, SpellID::Invalid, false },
	{ N_("Iron Skin"), N_("Toughens your hide, raising armour class."), Bar, 1, 3, 0, Kind::Passive, SpellID::Invalid, true },
	{ N_("Increased Speed"), N_("You run rather than walk, wherever you are."), Bar, 1, 4, 0, Kind::Passive, SpellID::Invalid, true },
	{ N_("Natural Resistance"), N_("Hardens you against fire, lightning and magic alike."), Bar, 1, 5, 0, Kind::Passive, SpellID::Invalid, true },
	// --- Warcries ---
	{ N_("Howl"), N_("Sends nearby enemies fleeing. Inert: it needs the monster-facing pass."), Bar, 2, 0, 0, Kind::Active, SpellID::Invalid, false },
	{ N_("Find Potion"), N_("Searches a corpse for a potion. Inert: it needs the corpse-handling pass."), Bar, 2, 0, 1, Kind::Active, SpellID::Invalid, false },
	{ N_("Taunt"), N_("Goads an enemy into charging you. Inert: it needs the monster-facing pass."), Bar, 2, 1, 0, Kind::Active, SpellID::Invalid, false },
	{ N_("Shout"), N_("A bellow that hardens you. Inert: buffs with a duration have no home here yet."), Bar, 2, 1, 1, Kind::Active, SpellID::Invalid, false },
	{ N_("Find Item"), N_("Searches a corpse for loot. Inert: it needs the corpse-handling pass."), Bar, 2, 2, 0, Kind::Active, SpellID::Invalid, false },
	{ N_("Battle Cry"), N_("A cry that weakens what hears it. Inert: it needs the monster-facing pass."), Bar, 2, 3, 0, Kind::Active, SpellID::Invalid, false },
	{ N_("Battle Orders"), N_("A shout that swells life and mana. Inert: buffs with a duration have no home here yet."), Bar, 2, 4, 0, Kind::Active, SpellID::Invalid, false },
	{ N_("Grim Ward"), N_("Raises a corpse as a totem of terror. Inert: it needs the corpse-handling pass."), Bar, 2, 4, 1, Kind::Active, SpellID::Invalid, false },
	{ N_("War Cry"), N_("A shout that stuns everything near. Inert: it needs the monster-facing pass."), Bar, 2, 5, 0, Kind::Active, SpellID::Invalid, false },
	{ N_("Battle Command"), N_("A command that deepens every other skill. Inert: buffs with a duration have no home here yet."), Bar, 2, 5, 1, Kind::Active, SpellID::Invalid, false },

	// ---- Passive Skills (page 3) ----
	{ N_("Pound of Flesh"), N_("Healing taken from the fallen leaves you mending and quickened, and it stacks. Not yet built."),
	    Bar, 3, 0, 0, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Ruthless"), N_("You fall far harder on the wounded. Not yet built."),
	    Bar, 3, 0, 1, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Nerves of Steel"), N_("A killing blow leaves you barely standing but briefly untouchable. Not yet built."),
	    Bar, 3, 0, 2, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Weapons Master"), N_("Each family of weapon lends its own gift - damage, precision, speed or fury. Not yet built."),
	    Bar, 3, 1, 0, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Inspiring Presence"), N_("Your shouts hold twice as long and leave everyone near you mending. Not yet built."),
	    Bar, 3, 1, 1, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Berserker Rage"), N_("Near the height of your fury you strike far harder. Not yet built."),
	    Bar, 3, 1, 2, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Bloodthirst"), N_("Every point of fury you spend is paid back in life. Not yet built."),
	    Bar, 3, 2, 0, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Animosity"), N_("Fury comes faster and you can hold more of it. Not yet built."),
	    Bar, 3, 2, 1, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Superstition"), N_("Magic and missiles hurt less, and being struck by them stokes your fury. Not yet built."),
	    Bar, 3, 2, 2, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Tough as Nails"), N_("Your armour and the harm you return are both greatly increased. Not yet built."),
	    Bar, 3, 3, 0, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("No Escape"), N_("What you throw and what you hurl lands harder on the distant. Not yet built."),
	    Bar, 3, 3, 1, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Relentless"), N_("Badly wounded, your skills cost half, your healing doubles and blows land softer. Not yet built."),
	    Bar, 3, 3, 2, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Brawler"), N_("Surrounded by three or more, everything you do hurts more. Not yet built."),
	    Bar, 3, 4, 0, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Juggernaut"), N_("What would hold you fast holds you half as long, and may give you back your life. Not yet built."),
	    Bar, 3, 4, 1, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Unforgiving"), N_("Your fury no longer ebbs when the fighting stops - it rises. Not yet built."),
	    Bar, 3, 4, 2, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Boon of Bul-Kathos"), N_("Your earthquake, your ancients and your berserking all return far sooner. Not yet built."),
	    Bar, 3, 5, 0, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Earthen Might"), N_("Splitting the ground fills you with fury. Not yet built."),
	    Bar, 3, 5, 1, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Sword and Board"), N_("Behind a shield you take far less harm and spend far less fury. Not yet built."),
	    Bar, 3, 5, 2, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Rampage"), N_("Every kill lends you strength, and it stacks high. Not yet built."),
	    Bar, 3, 6, 0, Kind::Passive, SpellID::Invalid, false, 1 },
	// ======================= SORCERESS =======================
	// --- Cold Spells: inert as a page. This engine has no cold damage channel and no chill, so
	//     every one of these would have to be invented rather than adapted. Listed and described.
	{ N_("Ice Bolt"), N_("A shard of ice that chills what it hits. Inert: this engine has no cold damage."), Sor, 0, 0, 0, Kind::Active, SpellID::Invalid, false },
	{ N_("Frozen Armor"), N_("Armour of ice that freezes attackers. Inert: no cold, no freeze."), Sor, 0, 0, 1, Kind::Active, SpellID::Invalid, false },
	{ N_("Frost Nova"), N_("A ring of ice bursting outward. Inert: no cold damage."), Sor, 0, 1, 0, Kind::Active, SpellID::Invalid, false },
	{ N_("Ice Blast"), N_("A shard that freezes its target solid. Inert: no cold, no freeze."), Sor, 0, 1, 1, Kind::Active, SpellID::Invalid, false },
	{ N_("Shiver Armor"), N_("Armour that answers blows with ice. Inert: no cold damage."), Sor, 0, 2, 1, Kind::Active, SpellID::Invalid, false },
	{ N_("Glacial Spike"), N_("A spike that shatters into freezing shards. Inert: no cold damage."), Sor, 0, 3, 0, Kind::Active, SpellID::Invalid, false },
	{ N_("Blizzard"), N_("Ice falls across a wide area. Inert: no cold damage."), Sor, 0, 4, 0, Kind::Active, SpellID::Invalid, false },
	{ N_("Chilling Armor"), N_("Armour that answers ranged attacks in kind. Inert: no cold damage."), Sor, 0, 4, 1, Kind::Active, SpellID::Invalid, false },
	{ N_("Frozen Orb"), N_("An orb that wanders, shedding ice. Inert: no cold damage."), Sor, 0, 5, 0, Kind::Active, SpellID::Invalid, false },
	{ N_("Cold Mastery"), N_("Pierces cold resistance. Inert: there is no cold to master."), Sor, 0, 5, 2, Kind::Passive, SpellID::Invalid, false },
	// --- Lightning Spells: most of this page is a wiring job - the engine already has the spells.
	{ N_("Charged Bolt"), N_("Looses a spray of erratic bolts. Points raise this engine's Charged Bolt."), Sor, 1, 0, 0, Kind::Active, SpellID::ChargedBolt, true },
	{ N_("Static Field"), N_("Strips a share of the life from everything near. Inert: no analogue exists here."), Sor, 1, 1, 0, Kind::Active, SpellID::Invalid, false },
	{ N_("Telekinesis"), N_("Works objects and gathers items at a distance. Points raise this engine's Telekinesis."), Sor, 1, 1, 1, Kind::Active, SpellID::Telekinesis, true },
	{ N_("Nova"), N_("A ring of lightning bursting outward. Points raise this engine's Nova."), Sor, 1, 2, 0, Kind::Active, SpellID::Nova, true },
	{ N_("Lightning"), N_("A bolt that strikes in a line. Points raise this engine's Lightning."), Sor, 1, 2, 1, Kind::Active, SpellID::Lightning, true },
	{ N_("Chain Lightning"), N_("A bolt that leaps between enemies. Points raise this engine's Chain Lightning."), Sor, 1, 3, 0, Kind::Active, SpellID::ChainLightning, true },
	{ N_("Teleport"), N_("Step instantly to a place you can see. Points raise this engine's Teleport."), Sor, 1, 3, 1, Kind::Active, SpellID::Teleport, true },
	{ N_("Thunder Storm"), N_("A storm that strikes on its own as you fight. Inert: no analogue exists here."), Sor, 1, 4, 0, Kind::Active, SpellID::Invalid, false },
	{ N_("Energy Shield"), N_("Mana takes the damage your life would. Points raise this engine's Mana Shield."), Sor, 1, 4, 1, Kind::Active, SpellID::ManaShield, true },
	{ N_("Lightning Mastery"), N_("Your blows carry lightning, and lightning troubles you less. Not D2's spell scaling: this engine deepens a spell by its LEVEL, and has no per-element channel to raise."), Sor, 1, 5, 2, Kind::Passive, SpellID::Invalid, true },
	// --- Fire Spells ---
	{ N_("Fire Bolt"), N_("A bolt of flame. Points raise this engine's Fire Bolt."), Sor, 2, 0, 0, Kind::Active, SpellID::Firebolt, true },
	{ N_("Warmth"), N_("Your mana returns of its own accord."), Sor, 2, 0, 1, Kind::Passive, SpellID::Invalid, true },
	{ N_("Inferno"), N_("A gout of flame from your hands. Points raise this engine's Inferno."), Sor, 2, 1, 0, Kind::Active, SpellID::Inferno, true },
	{ N_("Blaze"), N_("Leaves fire in your wake. Mapped onto this engine's Flame Wave, the nearest rolling fire it has."), Sor, 2, 2, 0, Kind::Active, SpellID::FlameWave, true },
	{ N_("Fire Ball"), N_("A bursting ball of flame. Points raise this engine's Fireball."), Sor, 2, 2, 1, Kind::Active, SpellID::Fireball, true },
	{ N_("Fire Wall"), N_("A wall of flame across the ground. Points raise this engine's Fire Wall."), Sor, 2, 3, 0, Kind::Active, SpellID::FireWall, true },
	{ N_("Enchant"), N_("Your weapon burns: every blow carries fire. A passive rather than a cast buff, since a tree skill with no spell slot has no way to be cast."), Sor, 2, 3, 1, Kind::Passive, SpellID::Invalid, true },
	{ N_("Meteor"), N_("Calls a burning rock down from the sky. Inert: no analogue exists here."), Sor, 2, 4, 0, Kind::Active, SpellID::Invalid, false },
	{ N_("Fire Mastery"), N_("Fire burns for you and less against you. Not D2's spell scaling: this engine deepens a spell by its LEVEL, and has no per-element channel to raise."), Sor, 2, 5, 2, Kind::Passive, SpellID::Invalid, true },
	{ N_("Hydra"), N_("Sets a fire-breathing head to guard a spot. Mapped onto this engine's Guardian, which is the same idea."), Sor, 2, 5, 0, Kind::Active, SpellID::Guardian, true },

	// ---- Passive Skills (page 3) ----
	{ N_("Power Hungry"), N_("You deal far more harm to what is far away. Not yet built."),
	    Sor, 3, 0, 0, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Blur"), N_("Everything that strikes you strikes softer. Not yet built."),
	    Sor, 3, 0, 1, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Evocation"), N_("Every cooldown you carry comes round sooner. Not yet built."),
	    Sor, 3, 0, 2, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Glass Cannon"), N_("You hit much harder and are much easier to hit back. Not yet built."),
	    Sor, 3, 1, 0, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Prodigy"), N_("Your simplest spells give back arcane power as you cast them. Not yet built."),
	    Sor, 3, 1, 1, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Astral Presence"), N_("You hold more arcane power and recover it faster. Not yet built."),
	    Sor, 3, 1, 2, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Illusionist"), N_("A heavy blow resets your escapes and speeds your step. Not yet built."),
	    Sor, 3, 2, 0, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Cold Blooded"), N_("What you have chilled takes more harm from every source. Not yet built."),
	    Sor, 3, 2, 1, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Conflagration"), N_("What you set alight becomes easier to strike truly. Not yet built."),
	    Sor, 3, 2, 2, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Paralysis"), N_("Your lightning may stun everything it touches. Not yet built."),
	    Sor, 3, 3, 0, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Galvanizing Ward"), N_("Go unharmed a moment and a ward forms around you. Not yet built."),
	    Sor, 3, 3, 1, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Temporal Flux"), N_("Arcane harm slows what it touches to a crawl. Not yet built."),
	    Sor, 3, 3, 2, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Dominance"), N_("Every kill lays another shell of shielding over you. Not yet built."),
	    Sor, 3, 4, 0, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Arcane Dynamo"), N_("Five simple spells charge the next great one. Not yet built."),
	    Sor, 3, 4, 1, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Unstable Anomaly"), N_("A killing blow throws up a vast ward and scatters what stands near. Not yet built."),
	    Sor, 3, 4, 2, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Unwavering Will"), N_("Stand still a moment and your armour, your wards and your damage all rise. Not yet built."),
	    Sor, 3, 5, 0, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Audacity"), N_("You deal far more harm to whatever is close enough to touch. Not yet built."),
	    Sor, 3, 5, 1, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Elemental Exposure"), N_("Striking with a new element leaves the target more open to all of them. Not yet built."),
	    Sor, 3, 5, 2, Kind::Passive, SpellID::Invalid, false, 1 },
	// ======================= ROGUE =======================
	// --- Bow & Crossbow: the bow skills all want missile work this engine has not been given yet.
	{ N_("Magic Arrow"), N_("An arrow of pure force that costs no ammunition. Not yet built."), Rog, 0, 0, 0, Kind::Active, SpellID::Invalid, false },
	{ N_("Fire Arrow"), N_("An arrow wrapped in flame. Not yet built."), Rog, 0, 0, 1, Kind::Active, SpellID::Invalid, false },
	{ N_("Cold Arrow"), N_("An arrow that chills. Inert: this engine has no cold damage."), Rog, 0, 1, 0, Kind::Active, SpellID::Invalid, false },
	{ N_("Multiple Shot"), N_("Looses a fan of arrows at once. Not yet built."), Rog, 0, 1, 1, Kind::Active, SpellID::Invalid, false },
	{ N_("Exploding Arrow"), N_("An arrow that bursts where it lands. Not yet built."), Rog, 0, 2, 0, Kind::Active, SpellID::Invalid, false },
	{ N_("Ice Arrow"), N_("An arrow that freezes its target. Inert: no cold, no freeze."), Rog, 0, 2, 1, Kind::Active, SpellID::Invalid, false },
	{ N_("Guided Arrow"), N_("An arrow that hunts its target. Not yet built."), Rog, 0, 3, 0, Kind::Active, SpellID::Invalid, false },
	{ N_("Strafe"), N_("Looses at every enemy in view in turn. Not yet built."), Rog, 0, 4, 0, Kind::Active, SpellID::Invalid, false },
	{ N_("Immolation Arrow"), N_("An arrow that leaves a burning pool. Not yet built."), Rog, 0, 4, 1, Kind::Active, SpellID::Invalid, false },
	{ N_("Freezing Arrow"), N_("An arrow that freezes everything near where it lands. Inert: no cold."), Rog, 0, 5, 0, Kind::Active, SpellID::Invalid, false },
	// --- Passive & Magic ---
	{ N_("Inner Sight"), N_("Lights nearby enemies and strips their defence. Inert: it needs the monster-facing pass."), Rog, 1, 0, 0, Kind::Active, SpellID::Invalid, false },
	{ N_("Critical Strike"), N_("A chance to strike for double. This engine has no critical roll, so it raises your damage instead."),
	    Rog, 1, 0, 1, Kind::Passive, SpellID::Invalid, true },
	{ N_("Dodge"), N_("A chance to slip a blow while standing. Inert: no avoidance roll exists here."), Rog, 1, 1, 0, Kind::Passive, SpellID::Invalid, false },
	{ N_("Slow Missiles"), N_("Slows what is thrown at you. Inert: it needs the monster-facing pass."), Rog, 1, 2, 0, Kind::Active, SpellID::Invalid, false },
	{ N_("Avoid"), N_("A chance to slip a missile. Inert: no avoidance roll exists here."), Rog, 1, 2, 1, Kind::Passive, SpellID::Invalid, false },
	{ N_("Penetrate"), N_("Sharpens your aim with anything you wield."), Rog, 1, 3, 0, Kind::Passive, SpellID::Invalid, true },
	{ N_("Decoy"), N_("A double of yourself to draw fire. Not yet built."), Rog, 1, 3, 1, Kind::Active, SpellID::Invalid, false },
	{ N_("Evade"), N_("A chance to slip a blow while moving. Inert: no avoidance roll exists here."), Rog, 1, 4, 0, Kind::Passive, SpellID::Invalid, false },
	{ N_("Valkyrie"), N_("Calls a warrior to fight beside you. Mapped onto this engine's Golem, which is the same idea."),
	    Rog, 1, 5, 0, Kind::Active, SpellID::Golem, true },
	{ N_("Pierce"), N_("Your missiles carry on through. Not yet built."), Rog, 1, 5, 1, Kind::Passive, SpellID::Invalid, false },
	// --- Javelin & Spear ---
	{ N_("Jab"), N_("A rapid flurry of thrusts. Not yet built."), Rog, 2, 0, 0, Kind::Active, SpellID::Invalid, false },
	{ N_("Power Strike"), N_("A thrust charged with lightning. Not yet built."), Rog, 2, 1, 0, Kind::Active, SpellID::Invalid, false },
	{ N_("Poison Javelin"), N_("A javelin trailing venom. Inert: this engine has no poison."), Rog, 2, 1, 1, Kind::Active, SpellID::Invalid, false },
	{ N_("Impale"), N_("A savage thrust that wears the weapon. Not yet built."), Rog, 2, 2, 0, Kind::Active, SpellID::Invalid, false },
	{ N_("Charged Strike"), N_("A thrust that throws off charged bolts. Not yet built."), Rog, 2, 2, 1, Kind::Active, SpellID::Invalid, false },
	{ N_("Lightning Bolt"), N_("Turns a thrown javelin into a bolt. Not yet built."), Rog, 2, 3, 0, Kind::Active, SpellID::Invalid, false },
	{ N_("Plague Javelin"), N_("A javelin trailing a cloud of pestilence. Inert: this engine has no poison."), Rog, 2, 3, 1, Kind::Active, SpellID::Invalid, false },
	{ N_("Fend"), N_("Strikes every enemy around you in one motion. Not yet built."), Rog, 2, 4, 0, Kind::Active, SpellID::Invalid, false },
	{ N_("Lightning Strike"), N_("A thrust whose lightning leaps onward. Not yet built."), Rog, 2, 5, 0, Kind::Active, SpellID::Invalid, false },
	{ N_("Lightning Fury"), N_("A javelin that bursts into many bolts. Not yet built."), Rog, 2, 5, 1, Kind::Active, SpellID::Invalid, false },

	// ---- Passive Skills (page 3) ----
	{ N_("Thrill of the Hunt"), N_("What your heavier shots strike is slowed almost to a stop. Not yet built."),
	    Rog, 3, 0, 0, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Tactical Advantage"), N_("Every evasion leaves you running far faster. Not yet built."),
	    Rog, 3, 0, 1, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Blood Vengeance"), N_("You hold more hatred, and the fallen restore both hatred and discipline. Not yet built."),
	    Rog, 3, 0, 2, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Steady Aim"), N_("With nothing close to you, everything you do hurts more. Not yet built."),
	    Rog, 3, 1, 0, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Cull the Weak"), N_("You fall harder on anything already slowed. Not yet built."),
	    Rog, 3, 1, 1, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Night Stalker"), N_("Your opening shots build hatred faster. Not yet built."),
	    Rog, 3, 1, 2, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Brooding"), N_("Stand still and your wounds close faster and faster. Not yet built."),
	    Rog, 3, 2, 0, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Hot Pursuit"), N_("Landing a blow leaves you moving faster. Not yet built."),
	    Rog, 3, 2, 1, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Archery"), N_("Each kind of bow lends its own gift - damage, precision or hatred. Not yet built."),
	    Rog, 3, 2, 2, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Numbing Traps"), N_("Anything you have slowed strikes back far weaker. Not yet built."),
	    Rog, 3, 3, 0, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Perfectionist"), N_("Your discipline goes further and your armour and wards are stronger. Not yet built."),
	    Rog, 3, 3, 1, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Custom Engineering"), N_("Your traps and sentries last twice as long and you may set more. Not yet built."),
	    Rog, 3, 3, 2, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Grenadier"), N_("Your grenades hit harder, burst wider, and one falls when you do. Not yet built."),
	    Rog, 3, 4, 0, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Sharpshooter"), N_("Every moment you do not land a telling blow makes the next one likelier. Not yet built."),
	    Rog, 3, 4, 1, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Ballistics"), N_("Your rockets hit twice as hard and sometimes seek their mark. Not yet built."),
	    Rog, 3, 4, 2, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Leech"), N_("Every blow you land returns life. Not yet built."),
	    Rog, 3, 5, 0, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Ambush"), N_("You fall far harder on the unwounded. Not yet built."),
	    Rog, 3, 5, 1, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Awareness"), N_("Once a minute a killing blow makes you vanish and mends you instead. Not yet built."),
	    Rog, 3, 5, 2, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Single Out"), N_("Anything that has strayed from its fellows is far easier to strike truly. Not yet built."),
	    Rog, 3, 6, 0, Kind::Passive, SpellID::Invalid, false, 1 },
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
	{ N_("Dirge of Dread"), N_("Weakens enemies and sends them fleeing. Inert: it needs the monster-facing pass."),
	    Bard, 0, 2, 0, Kind::Aura, SpellID::Invalid, false },
	{ N_("Lullaby"), N_("Puts enemies to sleep. Inert: this engine has no sleep state."), Bard, 0, 2, 1, Kind::Active, SpellID::Invalid, false },
	{ N_("Epic Solo"), N_("Mastery that empowers every Melody song. Inert: there is no per-page channel here."),
	    Bard, 0, 5, 1, Kind::Passive, SpellID::Invalid, false },
	// --- Harmony ---
	{ N_("Sound Shock"), N_("A burst of sonic force in front of you. Not yet built."), Bard, 1, 0, 0, Kind::Active, SpellID::Invalid, false },
	{ N_("Shout"), N_("A shout that stuns. Inert: it needs the monster-facing pass."), Bard, 1, 0, 1, Kind::Active, SpellID::Invalid, false },
	{ N_("Sonic Barrier"), N_("A barrier that drinks the damage meant for you. Points raise this engine's Mana Shield."),
	    Bard, 1, 1, 0, Kind::Active, SpellID::ManaShield, true },
	{ N_("Discord"), N_("Strips enemy defence. Inert: it needs the monster-facing pass."), Bard, 1, 1, 1, Kind::Aura, SpellID::Invalid, false },
	{ N_("Resonance"), N_("Your blows amplify your next song. Inert: no such carry-over exists here."),
	    Bard, 1, 2, 0, Kind::Passive, SpellID::Invalid, false },
	{ N_("Echoing Song"), N_("Your songs reach further and last longer. Inert: songs here have neither range nor duration."),
	    Bard, 1, 2, 1, Kind::Passive, SpellID::Invalid, false },
	{ N_("Perfect Harmony"), N_("Mastery that empowers every Harmony skill. Inert: there is no per-page channel here."),
	    Bard, 1, 5, 1, Kind::Passive, SpellID::Invalid, false },
	// --- Poetry ---
	{ N_("Daze"), N_("Sets an enemy wandering and striking at random. Inert: it needs the monster-facing pass."),
	    Bard, 2, 0, 0, Kind::Active, SpellID::Invalid, false },
	{ N_("Charm"), N_("Turns a monster to your side. Rides this engine's Berserk, which does exactly that."),
	    Bard, 2, 0, 1, Kind::Active, SpellID::Berserk, true },
	{ N_("Inspiration"), N_("A verse that returns your mana as it plays."), Bard, 2, 1, 0, Kind::Aura, SpellID::Invalid, true },
	{ N_("Tale of Heroes"), N_("A verse that lends you a hero's strength and grace."), Bard, 2, 1, 1, Kind::Aura, SpellID::Invalid, true },
	{ N_("Weaken"), N_("Blunts enemy aim and slows their step. Inert: it needs the monster-facing pass."),
	    Bard, 2, 2, 0, Kind::Aura, SpellID::Invalid, false },
	{ N_("Ode to Glory"), N_("Raises a fallen ally to fight on. Inert: it needs the corpse-handling pass."),
	    Bard, 2, 2, 1, Kind::Active, SpellID::Invalid, false },
	{ N_("Legendary Ballad"), N_("Mastery that empowers every Poetry skill. Inert: there is no per-page channel here."),
	    Bard, 2, 5, 1, Kind::Passive, SpellID::Invalid, false },

	// ---- Passive Skills (page 3) ----
	{ N_("Perfect Pitch"), N_("A song held without a wrong note strikes truer the longer it runs. Not yet built."),
	    Bard, 3, 0, 0, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Crescendo"), N_("Each verse of a song hits harder than the one before it, and it stacks. Not yet built."),
	    Bard, 3, 0, 1, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Sustain"), N_("Your songs hold their power well after you stop playing them. Not yet built."),
	    Bard, 3, 0, 2, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Countermelody"), N_("A second song may play beneath the first at half its strength. Not yet built."),
	    Bard, 3, 1, 0, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Rhythm"), N_("Striking in time with your song quickens your hand. Not yet built."),
	    Bard, 3, 1, 1, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Refrain"), N_("A song that has run its course begins again at no cost. Not yet built."),
	    Bard, 3, 1, 2, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Encore"), N_("Falling silent leaves the last song ringing a while longer. Not yet built."),
	    Bard, 3, 2, 0, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Cadence"), N_("Every third blow lands on the beat and hits far harder. Not yet built."),
	    Bard, 3, 2, 1, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Timbre"), N_("Your songs reach far further from you. Not yet built."),
	    Bard, 3, 2, 2, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Virtuoso"), N_("Your songs cost far less to hold. Not yet built."),
	    Bard, 3, 3, 0, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Dissonance"), N_("What your songs touch strikes back weaker. Not yet built."),
	    Bard, 3, 3, 1, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Improvisation"), N_("Switching songs costs nothing and briefly grants both. Not yet built."),
	    Bard, 3, 3, 2, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Chorus"), N_("Every ally within earshot lends your songs strength. Not yet built."),
	    Bard, 3, 4, 0, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Overture"), N_("The first song of a fight begins at its full power. Not yet built."),
	    Bard, 3, 4, 1, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Reverberation"), N_("Your songs echo, striking a second time for less. Not yet built."),
	    Bard, 3, 4, 2, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Stagecraft"), N_("Being struck while playing does not break the song. Not yet built."),
	    Bard, 3, 5, 0, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Requiem"), N_("Each enemy that falls near you mends you a little. Not yet built."),
	    Bard, 3, 5, 1, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Magnum Opus"), N_("Hold one song long enough and it becomes something greater. Not yet built."),
	    Bard, 3, 5, 2, Kind::Passive, SpellID::Invalid, false, 1 },
	// ======================= MONK =======================
	// The user's design doc (MONK_SKILL_TREE.md in the package) is the specification, including
	// the two things this tree did not previously support: a SEVENTH tier at character level 36,
	// and per-skill rank caps - five ranks for skills 1-6, one for each branch capstone.
	//
	// Every skill sits in the middle column because each branch is a ladder: seven skills, one per
	// tier, each requiring the one below it.
	// --- Way of the Staff ---
	{ N_("Sweeping Reed"), N_("Sweep your staff through enemies in a wide arc. Not yet built: it needs the multi-tile melee arc."),
	    Monk, 0, 0, 1, Kind::Active, SpellID::Invalid, false, 5 },
	{ N_("Breaking Current"), N_("A focused strike that breaks armour and interrupts. Inert: it needs the monster-facing pass."),
	    Monk, 0, 1, 1, Kind::Active, SpellID::Invalid, false, 5 },
	{ N_("Reed in the Wind"), N_("Staff blocks carry you aside. Inert: this engine exposes no block-chance channel."),
	    Monk, 0, 2, 1, Kind::Passive, SpellID::Invalid, false, 5 },
	{ N_("Vaulting Strike"), N_("Vault over danger onto a distant foe. Not yet built: it needs new movement work."),
	    Monk, 0, 3, 1, Kind::Active, SpellID::Invalid, false, 5 },
	{ N_("Wheel of Heaven"), N_("Spin your staff, striking all around you. Not yet built."),
	    Monk, 0, 4, 1, Kind::Active, SpellID::Invalid, false, 5 },
	{ N_("Seven Reeds"), N_("A rapid chain of staff blows. Not yet built; the Paladin's Zeal burst is the nearest machinery."),
	    Monk, 0, 5, 1, Kind::Active, SpellID::Invalid, false, 5 },
	{ N_("Master of the Long Staff"), N_("Your mastery of the staff empowers every Way of the Staff skill. With a staff in hand: +10% damage and a sharper aim."),
	    Monk, 0, 6, 1, Kind::Passive, SpellID::Invalid, true, 1 },
	// --- Way of the Body ---
	{ N_("Open Palm"), N_("An open-hand strike that drives the enemy back. Not yet built."),
	    Monk, 1, 0, 1, Kind::Active, SpellID::Invalid, false, 5 },
	{ N_("Flowing Step"), N_("Move through battle with greater speed. One point makes you run rather than walk; the evade half needs an avoidance roll this engine has not got."),
	    Monk, 1, 1, 1, Kind::Passive, SpellID::Invalid, true, 5 },
	{ N_("Iron Robe"), N_("Discipline hardens your body while you wear light armour or none at all. Unarmoured: armour class by level, and blows land lighter. Light armour keeps half. Mail and plate switch it off."),
	    Monk, 1, 2, 1, Kind::Passive, SpellID::Invalid, true, 5 },
	{ N_("Counterstroke"), N_("A block empowers your next blow. Inert: nothing here reports a block to build on."),
	    Monk, 1, 3, 1, Kind::Passive, SpellID::Invalid, false, 5 },
	{ N_("Purifying Breath"), N_("Centre yourself against the elements. Inert: the cleansing half needs status effects this engine has not got."),
	    Monk, 1, 4, 1, Kind::Active, SpellID::Invalid, false, 5 },
	{ N_("Hundred Fists"), N_("A storm of unarmed strikes on one enemy. Not yet built."),
	    Monk, 1, 5, 1, Kind::Active, SpellID::Invalid, false, 5 },
	{ N_("Perfect Vessel"), N_("Your mastery of the body empowers every Way of the Body skill: a tenth more life, and you shake off hits faster."),
	    Monk, 1, 6, 1, Kind::Passive, SpellID::Invalid, true, 1 },
	// --- Way of the Spirit ---
	{ N_("Inner Sight"), N_("Reveal nearby objects, traps and treasure. Deepens the Monk's own Search: every point holds the sight longer."),
	    Monk, 2, 0, 1, Kind::Active, SpellID::Search, true, 5 },
	{ N_("Healing Mantra"), N_("Restore life to yourself over time. Held like an aura rather than cast, so it mends you for as long as it plays."),
	    Monk, 2, 1, 1, Kind::Aura, SpellID::Invalid, true, 5 },
	{ N_("Temple Bell"), N_("A tone that staggers and repels the undead. Inert: it needs the monster-facing pass."),
	    Monk, 2, 2, 1, Kind::Active, SpellID::Invalid, false, 5 },
	{ N_("Spirit Ward"), N_("Surround yourself with a barrier against magic. Rides this engine's Mana Shield, which drinks the blow into your mana; every point makes it drink deeper."),
	    Monk, 2, 3, 1, Kind::Active, SpellID::ManaShield, true, 5 },
	{ N_("Radiant Palm"), N_("Marks an enemy to erupt when it falls. Not yet built."),
	    Monk, 2, 4, 1, Kind::Active, SpellID::Invalid, false, 5 },
	{ N_("Tranquility"), N_("A sanctuary that slows enemies and restores allies. Inert: it needs a ground-effect pass."),
	    Monk, 2, 5, 1, Kind::Active, SpellID::Invalid, false, 5 },
	{ N_("Enlightenment"), N_("Your mastery of spirit empowers every Way of the Spirit skill: a tenth more mana, and ten points of every resistance."),
	    Monk, 2, 6, 1, Kind::Passive, SpellID::Invalid, true, 1 },
	// ---- Passive Skills (page 3) ----
	{ N_("Resolve"), N_("What you strike strikes back weaker for a while. Not yet built."),
	    Monk, 3, 0, 0, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Fleet Footed"), N_("You move faster at all times. Not yet built."),
	    Monk, 3, 0, 1, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Exalted Soul"), N_("You hold more spirit and recover it faster. Not yet built."),
	    Monk, 3, 0, 2, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Transcendence"), N_("Every point of spirit you spend returns as life. Not yet built."),
	    Monk, 3, 1, 0, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Chant of Resonance"), N_("Your mantras cost far less to invoke. Not yet built."),
	    Monk, 3, 1, 1, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Seize the Initiative"), N_("Striking the unwounded quickens your hand. Not yet built."),
	    Monk, 3, 1, 2, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("The Guardian's Path"), N_("Two weapons lend you evasion; one great staff lends you spirit. Not yet built."),
	    Monk, 3, 2, 0, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Sixth Sense"), N_("Everything that is not steel hurts you far less. Not yet built."),
	    Monk, 3, 2, 1, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Determination"), N_("Every enemy pressing close makes you hit harder. Not yet built."),
	    Monk, 3, 2, 2, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Relentless Assault"), N_("You fall harder on anything blinded, frozen or reeling. Not yet built."),
	    Monk, 3, 3, 0, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Beacon of Ytar"), N_("Every cooldown you carry comes round sooner. Not yet built."),
	    Monk, 3, 3, 1, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Alacrity"), N_("Your spirit-building strikes come faster. Not yet built."),
	    Monk, 3, 3, 2, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Harmony"), N_("A ward against one element becomes a lesser ward against all of them. Not yet built."),
	    Monk, 3, 4, 0, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Combination Strike"), N_("Rotating your strikes makes each of them stronger. Not yet built."),
	    Monk, 3, 4, 1, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Near Death Experience"), N_("Once a minute a killing blow restores your life and spirit instead. Not yet built."),
	    Monk, 3, 4, 2, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Unity"), N_("Every ally under your mantra lends you strength. Not yet built."),
	    Monk, 3, 5, 0, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Momentum"), N_("Cover enough ground and your next blows land far harder. Not yet built."),
	    Monk, 3, 5, 1, Kind::Passive, SpellID::Invalid, false, 1 },
	{ N_("Mythic Rhythm"), N_("Every third building strike charges the spender that follows. Not yet built."),
	    Monk, 3, 5, 2, Kind::Passive, SpellID::Invalid, false, 1 },
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
		totals.fireMin += Scaled(p, 2, 1);
		totals.fireMax += Scaled(p, 6, 4);
		break;
	case Skill::Thorns:
		totals.flags |= ItemSpecialEffect::Thorns;
		break;
	case Skill::BlessedAim:
		totals.bonusToHit += Scaled(p, 15, 7);
		break;
	case Skill::Concentration:
		totals.bonusDamage += Scaled(p, 15, 8);
		totals.flags |= ItemSpecialEffect::FastestHitRecovery;
		break;
	case Skill::HolyShock:
		totals.lightningMin += 1;
		totals.lightningMax += Scaled(p, 10, 6);
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
	default:
		// Prayer, Meditation, Vigor, Melody of Life and Inspiration act elsewhere (the per-tick
		// hook and the walk animation); the rest are inert - see their rows.
		break;
	}
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
	return static_cast<int>(skill) - static_cast<int>(first);
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
		// first cast - so most skills fall through PlaySkillSound silently, which is intended.
		PlaySkillSound(skill, SkillSoundEvent::Learn);
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
	const HeroClass heroClass = GetClassTreeSkillData(skill).heroClass;
	const Skill first = FirstSkillOf(heroClass);
	if (first == Skill::None)
		return -1;
	int index = 0;
	for (size_t i = static_cast<size_t>(first); i < ClassTreeSkillCount; i++) {
		const auto candidate = static_cast<Skill>(i);
		if (Skills[i].heroClass != heroClass)
			break;
		if (candidate == skill)
			return index;
		if (Skills[i].page == PassiveSkillsPage)
			index++;
	}
	return -1;
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
	// One every even level: the first at 2, the nth at 2n+2. Nineteen passives reach level 38, well
	// inside the character cap, so no class outruns its own page.
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
	}

	// Two kinds of passive, and they turn on for different reasons.
	//
	// A Diablo II passive - the Barbarian's masteries, the Rogue's Passive & Magic page - is always
	// on once BOUGHT, and scales with the points in it.
	//
	// A Passive Skills page row is bought with nothing and scales with nothing. It is on if and only
	// if it sits in one of the four slots, which is the whole of that page's choice. Every one of
	// them is inert today, so this gate changes no number yet; it is here so that the first one
	// built cannot accidentally apply from the grid.
	const Skill first = FirstSkillOf(player._pClass);
	for (size_t i = 0; i < MaxSkillsPerClass; i++) {
		const auto skill = static_cast<Skill>(static_cast<size_t>(first) + i);
		if (skill > Skill::LAST)
			break;
		const ClassTreeSkillData &data = GetClassTreeSkillData(skill);
		if (data.heroClass != player._pClass)
			break;
		if (data.kind != Kind::Passive || !data.implemented)
			continue;
		if (!IsClassTreeSkillUnlocked(player, skill))
			continue;
		if (IsPassiveSkillRow(skill)) {
			if (PassiveSlotOf(player, skill) >= 0)
				ApplyPassive(player, skill, 1, totals);
			continue;
		}
		const int points = ClassTreeInvestment(player, skill);
		if (points > 0)
			ApplyPassive(player, skill, points, totals);
	}
}

bool IsClassTreeRunActive(const Player &player)
{
	if (!ClassHasTree(player._pClass))
		return false;
	if (player._pClass == HeroClass::Warrior) {
		const Skill aura = GetActiveClassAura(player);
		return aura == Skill::Vigor && ClassTreeInvestment(player, aura) > 0
		    && IsClassTreeSkillUnlocked(player, aura);
	}
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
		    && IsClassTreeSkillUnlocked(player, Skill::FlowingStep);
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
				// A row whose slot is a BOOK spell is not a tree skill any more (user rule,
				// 2026-08-20: "Spells cant be affected by skill points, only by books"). It is
				// filtered out here rather than deleted from the table, because
				// ClassTreeIconIndex is simultaneously the icon-strip position AND the
				// _pClassTreeInvestment index - deleting rows would drift the art and misalign
				// every existing save. The grid is addressed by page/tier/column, so a filtered
				// row simply leaves its cell empty and shifts nothing.
				if (IsClassTreeRowRetiredAsSpell(static_cast<Skill>(index)))
					continue;
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

std::string ClassTreeEffectLine(const Player &player, Skill skill)
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
	const int p = ClassTreeInvestment(player, skill);
	const int maxRank = ClassTreeMaxRank(skill);
	std::string out = fmt::format(fmt::runtime(_("Points: {:d} of {:d}")), p, maxRank);
	// THE NEXT RANK'S requirement, which climbs with every point spent (user, 2026-08-28: "update
	// lvl requrements everytime a skill point is added to reflect truthfully the req lvl bump with
	// each skill lvl").
	//
	// This printed ClassTreeTierMinLevel - the tier's floor, a constant - while the gate that
	// actually refuses the point is RankRequiredLevel(tierLevel, invested + 1), the Rule of Rangs
	// (see CanInvestClassTreePoint). So a tier-1 skill with nine points in it still advertised
	// "Requires level 1" while silently demanding level 10 for the tenth, and the button did nothing
	// with no explanation anywhere on the tooltip.
	//
	// Phrased as the NEXT point's price rather than the current rank's, because that is the only
	// number a player standing in front of the button can act on. At full rank there is no next
	// point and the line would be a number for a purchase that cannot be made.
	if (p >= maxRank) {
		out += "\n" + std::string(_("Fully invested"));
	} else {
		const int nextLevel = RankRequiredLevel(ClassTreeTierMinLevel(data.tier), p + 1);
		out += "\n" + fmt::format(fmt::runtime(_("Next point requires level {:d}")), nextLevel);
	}
	if (!data.implemented)
		out += "\n" + std::string(_("No effect yet"));
	else if (data.kind == Kind::Aura && p == 0)
		out += "\n" + std::string(_("Invest a point to light it"));
	else if (data.kind == Kind::Passive && p == 0)
		out += "\n" + std::string(_("Invest a point to gain it"));

	// WHAT THE POINTS BUY, at this rank and at the next (user, 2026-08-28: "i want more information
	// in the hover opoups of skills/spells/auras - include the benefits/bonuses current level is
	// providing and the bonuses/benefits the next level will provide").
	//
	// Only what is actually MODELLED gets a number. Two things are, and the honest scope of this
	// line is those two:
	//
	//   - Every aura's radius, from AuraRadiusForPoints - the one quantity every aura in the tree
	//     scales by, and the one the player can see on the floor.
	//   - Conviction's immunity break, which is a threshold rather than a curve and so is stated as
	//     the rank it happens at.
	//
	// The remaining rows have bespoke effects with no per-rank formula behind them - several are
	// still `implemented == false` - and inventing a number for those would be worse than the
	// silence: a tooltip that quotes a bonus the code does not apply is a bug that reads as a
	// feature. Those rows keep their description, which is what states the effect today.
	if (data.implemented && data.kind == Kind::Aura && p > 0) {
		const int radius = AuraRadiusForPoints(p);
		out += "\n" + fmt::format(fmt::runtime(_("Radius: {:d} tiles")), radius);
		if (p < maxRank) {
			const int nextRadius = AuraRadiusForPoints(p + 1);
			// Silent when the next point does not move it - the radius steps every second point and
			// caps at eight, so "Next point: 8 tiles" beside "Radius: 8 tiles" is noise that makes
			// the player look twice for a difference that is not there.
			if (nextRadius != radius)
				out += "\n" + fmt::format(fmt::runtime(_("Next point: {:d} tiles")), nextRadius);
		}
	}
	if (skill == Skill::Conviction && data.implemented) {
		out += "\n"
		    + (p >= ConvictionBreaksImmunityAt
		            ? std::string(_("Breaks immunities"))
		            : fmt::format(fmt::runtime(_("Breaks immunities at {:d} points")), ConvictionBreaksImmunityAt));
	}
	return out;
}

} // namespace devilution::oracool
