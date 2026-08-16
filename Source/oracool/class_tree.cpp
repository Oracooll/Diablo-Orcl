#include "oracool/class_tree.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <optional>

#include <fmt/format.h>

#include "engine/backbuffer_state.hpp"
#include "oracool/aura_field.h"
#include "oracool/event_log.h"
#include "oracool/paladin_skills.h"
#include "oracool/skill_points.h"
#include "oracool/skill_sounds.h"
#include "oracool/stat_sheet.h"
#include "player.h"
#include "utils/language.h"

namespace devilution::oracool {

namespace {

using Skill = ClassTreeSkill;
using Kind = ClassTreeKind;

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
	    Pal, 0, 0, 1, Kind::Active, SpellID::Invalid, true },
	{ N_("Holy Bolt"), N_("A bolt of holy energy that sears the undead. Points raise this engine's own Holy Bolt."),
	    Pal, 0, 0, 2, Kind::Active, SpellID::HolyBolt, true },
	{ N_("Zeal"), N_("Strike several times in one furious burst. Each invested pair of points adds a strike, up to five."),
	    Pal, 0, 1, 0, Kind::Active, SpellID::Invalid, true },
	{ N_("Charge"), N_("Rush an enemy and land a running blow."),
	    Pal, 0, 1, 1, Kind::Active, SpellID::Invalid, true },
	{ N_("Vengeance"), N_("Adds fire, lightning and cold damage to your attack. Not yet built; this engine also has no cold."),
	    Pal, 0, 2, 0, Kind::Active, SpellID::Invalid, false },
	{ N_("Blessed Hammer"), N_("Looses a spinning hammer that wheels outward through anything in its path."),
	    Pal, 0, 3, 2, Kind::Active, SpellID::Invalid, true },
	// Corrected 2026-08-16: this row used to claim "no charmed-monster state exists", which was
	// simply wrong - this engine's Berserk sets MFLAG_GOLEM on the target, making it fight for the
	// player, which IS conversion. Found while wiring the Bard's Charm onto the same spell.
	{ N_("Conversion"), N_("Turns an enemy to your side. Rides this engine's Berserk, which does exactly that."),
	    Pal, 0, 4, 1, Kind::Active, SpellID::Berserk, true },
	{ N_("Fist of the Heavens"), N_("Calls down a bolt from the sky, which bursts into holy energy where it lands."),
	    Pal, 0, 5, 2, Kind::Active, SpellID::Invalid, true },
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
	    Pal, 0, 2, 1, Kind::Active, SpellID::Invalid, true },
	{ N_("Blessed Shield"), N_("Hurls your shield at a crowd, striking several of them before it returns. A shield is mandatory."),
	    Pal, 0, 3, 0, Kind::Active, SpellID::Invalid, true },

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
	{ N_("Lightning Mastery"), N_("Deepens every lightning spell. Inert: there is no per-element channel here."), Sor, 1, 5, 2, Kind::Passive, SpellID::Invalid, false },
	// --- Fire Spells ---
	{ N_("Fire Bolt"), N_("A bolt of flame. Points raise this engine's Fire Bolt."), Sor, 2, 0, 0, Kind::Active, SpellID::Firebolt, true },
	{ N_("Warmth"), N_("Your mana returns of its own accord."), Sor, 2, 0, 1, Kind::Passive, SpellID::Invalid, true },
	{ N_("Inferno"), N_("A gout of flame from your hands. Points raise this engine's Inferno."), Sor, 2, 1, 0, Kind::Active, SpellID::Inferno, true },
	{ N_("Blaze"), N_("Leaves fire in your wake. Mapped onto this engine's Flame Wave, the nearest rolling fire it has."), Sor, 2, 2, 0, Kind::Active, SpellID::FlameWave, true },
	{ N_("Fire Ball"), N_("A bursting ball of flame. Points raise this engine's Fireball."), Sor, 2, 2, 1, Kind::Active, SpellID::Fireball, true },
	{ N_("Fire Wall"), N_("A wall of flame across the ground. Points raise this engine's Fire Wall."), Sor, 2, 3, 0, Kind::Active, SpellID::FireWall, true },
	{ N_("Enchant"), N_("Sets a weapon alight. Inert: no analogue exists here."), Sor, 2, 3, 1, Kind::Active, SpellID::Invalid, false },
	{ N_("Meteor"), N_("Calls a burning rock down from the sky. Inert: no analogue exists here."), Sor, 2, 4, 0, Kind::Active, SpellID::Invalid, false },
	{ N_("Fire Mastery"), N_("Deepens every fire spell. Inert: there is no per-element channel here."), Sor, 2, 5, 2, Kind::Passive, SpellID::Invalid, false },
	{ N_("Hydra"), N_("Sets a fire-breathing head to guard a spot. Mapped onto this engine's Guardian, which is the same idea."), Sor, 2, 5, 0, Kind::Active, SpellID::Guardian, true },

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

bool IsClassTreeSkillUnlocked(const Player &player, Skill skill)
{
	if (skill > Skill::LAST)
		return false;
	const ClassTreeSkillData &data = GetClassTreeSkillData(skill);
	if (data.heroClass != player._pClass)
		return false;
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

bool CanInvestClassTreePoint(const Player &player, Skill skill)
{
	return player._pUnspentSkillPoints > 0
	    && IsClassTreeSkillUnlocked(player, skill)
	    && ClassTreeInvestment(player, skill) < ClassTreeMaxRank(skill);
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
	if (&player == MyPlayer) {
		LogEvent(fmt::format("{:s} raised to {:d}", std::string(_(GetClassTreeSkillData(skill).name)),
		             ClassTreeInvestment(player, skill)),
		    UiFlags::ColorWhitegold);
		// The `learn` cue. Only the passives and masteries have one - an active's confirmation is its
		// first cast - so most skills fall through PlaySkillSound silently, which is intended.
		PlaySkillSound(skill, SkillSoundEvent::Learn);
	}
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
		player._pOracoolActiveAura = static_cast<uint8_t>(Skill::None);
		if (&player == MyPlayer)
			StopClassAuraLoop();
	}

	if (&player == MyPlayer) {
		LogEvent(fmt::format("{:s} lowered to {:d}", std::string(_(GetClassTreeSkillData(skill).name)),
		             ClassTreeInvestment(player, skill)),
		    UiFlags::ColorWhitegold);
	}
	return true;
}

Skill GetActiveClassAura(const Player &player)
{
	const auto skill = static_cast<Skill>(player._pOracoolActiveAura);
	if (skill > Skill::LAST)
		return Skill::None;
	if (GetClassTreeSkillData(skill).kind != Kind::Aura)
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
	player._pOracoolActiveAura = static_cast<uint8_t>(switchingOff ? Skill::None : skill);
	if (&player == MyPlayer) {
		const char *name = GetClassTreeSkillData(skill).name;
		LogEvent(switchingOff ? fmt::format("{:s} fades", std::string(_(name)))
		                      : fmt::format("{:s} burns", std::string(_(name))),
		    UiFlags::ColorWhitegold);
		// The persistent cue. StartClassAuraLoop stops whatever was running first, so switching
		// straight from one aura to another is atomic in the order the sound package asks for: old
		// loop down, old stop cue, new start cue, new loop up.
		if (switchingOff)
			StopClassAuraLoop();
		else
			StartClassAuraLoop(skill);
	}
	return true;
}

void ApplyClassTreeToTotals(const Player &player, ItemBonusTotals &totals)
{
	if (!ClassHasTree(player._pClass))
		return;

	if (const Skill aura = GetActiveClassAura(player);
	    aura != Skill::None && IsClassTreeSkillUnlocked(player, aura)) {
		ApplyAura(aura, ClassTreeInvestment(player, aura), totals);
	}

	// Passives are always on once bought - no activation, no slot, just the points.
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

	// The Sorceress's Warmth is a passive, so it needs no activation - the points alone.
	if (player._pClass == HeroClass::Sorcerer && IsClassTreeSkillUnlocked(player, Skill::Warmth)
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
				if (data.page == page && data.tier == tier && data.column == column)
					out[count++] = static_cast<Skill>(index);
			}
		}
	}
	return count;
}

std::string ClassTreeEffectLine(const Player &player, Skill skill)
{
	const ClassTreeSkillData &data = GetClassTreeSkillData(skill);
	const int p = ClassTreeInvestment(player, skill);
	std::string out = fmt::format(fmt::runtime(_("Points: {:d} of {:d}")), p, ClassTreeMaxRank(skill));
	out += "\n" + fmt::format(fmt::runtime(_("Requires level {:d}")), ClassTreeTierMinLevel(data.tier));
	if (!data.implemented)
		out += "\n" + std::string(_("No effect yet"));
	else if (data.kind == Kind::Aura && p == 0)
		out += "\n" + std::string(_("Invest a point to light it"));
	else if (data.kind == Kind::Passive && p == 0)
		out += "\n" + std::string(_("Invest a point to gain it"));
	return out;
}

} // namespace devilution::oracool
