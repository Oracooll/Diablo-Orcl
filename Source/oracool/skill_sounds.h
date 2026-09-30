/**
 * @file oracool/skill_sounds.h
 *
 * The six-class skill sound library, and the set-completion stinger.
 *
 * VANILLA SINCE 2026-09-27 (user: "remove all chatgpt sounds from the game. they are no good. replace with vanilla sounds per
 * your decision"). Every cue below is a file in the player's diabdat.mpq, chosen per class page and event by
 * tools/GenVanillaSkillSounds.js; the delivered package this header describes is out of the game, its cue slots kept
 * (tools/skill_sound_slots.csv). Aura loops are silent. The history below is the package's.
 *
 * 306 WAVs covering every node of all six class trees (304 delivered; RfA-02 added two), delivered as data with an authoritative
 * manifest (Resources/02. Oracooll Assets/skill-sounds/class-skill-sounds.zip). The manifest is read at
 * BUILD time by tools/GenVanillaSkillSounds.js, which joins it to the class tree on (class, skill name)
 * and emits skill_sounds_data.inc. Nothing here is hand-typed, and a sound whose skill name matches
 * no tree row is a generator error rather than a cue that silently never plays.
 *
 * The package ships its own integration contract. Where this follows it, and where it deliberately
 * does not:
 *
 *   - "Missing sound IDs must fail softly and never cancel skill execution." Every function here
 *     returns quietly on a miss. A skill with no cue is normal. The two Paladin skills added to the
 *     tree after the package's design sheets (Hammer of Faith, Blessed Shield) had none until
 *     2026-09-11, when RfA-02's four WAVs arrived through tools/skill_sounds_extra.csv.
 *   - "Key each loop by (source player/entity, skill ID, activation generation)." Collapsed here to
 *     ONE module-level handle, because this fork stores the active aura as a single value on the
 *     player (Player::_pOracoolActiveAura). There cannot be two live loops to tell apart, so a
 *     generation counter would be machinery guarding an impossible state. If auras ever become
 *     stackable this has to grow back - see StopClassAuraLoop.
 *   - Voice limits and per-caster caps are NOT implemented. The engine's own mixer already caps
 *     duplicate sounds, and snd_play_snd drops any retrigger inside 80ms, which subsumes the
 *     contract's 70ms floor for multi-hit skills.
 *
 * Gains follow the contract's mix defaults, in the engine's hundredths-of-a-dB units.
 */
#pragma once

#include <cstdint>

#include "oracool/class_tree.h"
#include "utils/attributes.h"

namespace devilution {

struct Player;

namespace oracool {

/** @brief When a cue fires. The package's own event vocabulary, one-to-one. */
enum class SkillSoundEvent : uint8_t {
	/** Accepted skill activation, at the caster. */
	Cast,
	/** Resolved hit or area packet. One per resolved cast, not one per monster. */
	Impact,
	/** A summon was successfully placed. Suppressed when placement fails. */
	Arrive,
	/** A persistent state became active. Precedes the loop. */
	Start,
	/** The body of a persistent state. Exactly 2.0s and seamless at the wrap. */
	Loop,
	/** A persistent state ended or was replaced. Follows the loop's stop. */
	Stop,
	/** Allocation confirmed, in the UI. Never spatial, never in combat. */
	Learn,
};

struct SkillSound {
	ClassTreeSkill skill;
	SkillSoundEvent event;
	/** @brief MPQ-relative, backslash-separated, as every other asset path in this engine. */
	const char *path;
};

/**
 * @brief Plays @p event for @p skill once, or does nothing if the pair has no cue.
 *
 * Non-spatial: these are the local player's own skills, and the contract asks for `learn` to be
 * non-spatial anyway. A monster-side impact one day would want a pan derived from the hit tile.
 *
 * @return whether a cue existed and was played. Callers that have a FALLBACK sound need this - the
 * cast hook in StartSpell plays the row's Cast cue IN PLACE of the generic IS_CAST2 (user, 2026-09-26), and
 * the vanilla sound only when the cue is missing.
 */
bool PlaySkillSound(ClassTreeSkill skill, SkillSoundEvent event);

/** @brief The file behind @p event for @p skill, or nullptr when the row has no such cue. For tests and fallbacks. */
DVL_API_FOR_TEST const char *SkillSoundPath(ClassTreeSkill skill, SkillSoundEvent event);

/** @brief Whether @p skill has a cue for @p event - asked before choosing between it and a vanilla sound. */
bool HasSkillSound(ClassTreeSkill skill, SkillSoundEvent event);

/**
 * @brief Begins @p skill's persistent loop: the start cue, then the looping body beneath it.
 *
 * Stops whatever loop was running first, so aura replacement is atomic in the order the contract
 * asks for - old stop, then new start.
 */
void StartClassAuraLoop(ClassTreeSkill skill);

/**
 * @brief Re-attaches @p skill's loop WITHOUT its start cue.
 *
 * The contract's "loops are reconstructed from validated active state after load". Reconstruction is
 * not activation: the aura was already burning before the level changed, and replaying its start cue
 * would announce something that did not just happen.
 */
void ResumeClassAuraLoop(ClassTreeSkill skill);

/** @brief Stops the running aura loop and plays its stop cue. Safe when nothing is running. */
void StopClassAuraLoop();

/**
 * @brief Releases the aura loop WITHOUT its stop cue.
 *
 * For the transitions that are not the player switching an aura off: death, level change, loading a
 * different character, leaving the game. The contract wants the handle released on all of them, but
 * a stop cue on a level transition would be a sound with no cause the player can see.
 */
void SilenceClassAuraLoop();

/**
 * @brief Drops every loaded cue - the skills', the UI events', the set chime and a running loop - before the sound
 * device goes (audit, 2026-09-29). Called from effects_cleanup_sfx, beside vanilla's own: a sample-rate change restarts
 * the device, and cues decoded for the old one would play at the wrong pitch. The lit aura is still remembered.
 */
void FreeSkillSounds();

/**
 * @brief The tree row whose cast is currently creating missiles, or None.
 *
 * A missile outlives the call that made it by many ticks, so the skill that fired it has to travel
 * WITH it rather than be looked up at the moment it lands - by then the player has moved on, may
 * have readied something else, and `CastSpell`'s arguments are long gone. `AddMissile` stamps this
 * onto every missile it creates, and the impact cue reads it back off the missile.
 *
 * A scoped global rather than a parameter because `AddMissile` has eleven call sites in this engine
 * that have nothing to do with class skills - traps, monster attacks, town portals - and threading a
 * skill id through all of them to serve the few that care would be a worse trade than one value set
 * for the length of one call.
 */
DVL_API_FOR_TEST void BeginSkillCast(ClassTreeSkill skill);

/** @brief Ends the window opened by BeginSkillCast. Missiles made after this carry no skill. */
DVL_API_FOR_TEST void EndSkillCast();

/** @brief The skill missiles created right now belong to, or None outside a cast. */
DVL_API_FOR_TEST ClassTreeSkill CurrentCastSkill();

/** @brief The shared equipment-set completion stinger, `ui.set.complete`. */
void PlaySetCompleteSound();

/**
 * @brief Oracool (RfA-03 batch 9, 2026-09-11): the fork's own item-event sounds, which used to borrow
 * vanilla ones that meant something else (a shield slotting in for a salvage, a spell for an orb).
 */
enum class UiEventSound : uint8_t {
	Salvage,
	Transmute,
	ShardImbue, // OrbAbsorb until 2026-09-19
	Socket,
	RunewordComplete,
	// RfA-04 batch 14 (2026-09-11): four events that made no sound at all.
	Milestone,
	EncounterCleared,
	MapUnseal,
	SignetUse,
	// RfA-19 batch 42 (2026-09-20): the Stonegate's portals opening and closing.
	RiftOpen,
	RiftClose,
	// RfA-20 batch 43d (2026-09-20): Levski's Cube - the lid parting, and a transmute on its book.
	CubeOpen,
	CubeTransmute,
};

/**
 * @brief Plays @p sound (sfx\ui\<name>.wav) once. False - so the caller plays the vanilla sound it
 * used before - when there is no audio device or the file is not in the archive.
 */
bool PlayUiEventSound(UiEventSound sound);

/** @brief How many UiEventSound values there are, so a test can walk all of them. */
constexpr size_t UiEventSoundCount = static_cast<size_t>(UiEventSound::CubeTransmute) + 1;

/**
 * @brief The archive path @p sound is loaded from, or nullptr if @p sound is out of range.
 *
 * Exported because a wrong path here is INVISIBLE: PlayUiEventSound returns false for a file that
 * will not load and every caller falls back to the vanilla sound it used before, so nine of these
 * were misspelled for fifty versions and nothing ever sounded broken. Reading the source line does
 * not help either - the bug was single backslashes, which the compiler silently eats. A test has to
 * ask what the string actually became.
 */
DVL_API_FOR_TEST const char *UiEventSoundPath(UiEventSound sound);

/**
 * @brief Fires the set-completion stinger when a set crosses from incomplete to complete.
 *
 * Call after any equipment transaction has fully settled. The contract is strict about what must
 * NOT ring it - a recalculation, a load, a preview, a cancelled move, or swapping one valid piece
 * for another - so this compares against a remembered mask rather than against "is anything
 * complete right now".
 */
void CheckSetCompletionTransition(const Player &player);

/**
 * @brief Re-reads which sets are complete WITHOUT ringing for any of them.
 *
 * The suppression half of the above: called when a character is loaded or entered, so an already-
 * complete set is the baseline rather than an achievement the player just earned.
 *
 * Belt and braces: CheckSetCompletionTransition also records silently on its FIRST call after a
 * reset, because the first CalcPlrInv of a character's life runs from the player-file load - before
 * any code that knows to arm this. See BaselineArmed in the .cpp.
 */
void ArmSetCompletionBaseline(const Player &player);
/** @brief Whether LoadGameLevel has armed the baseline - the character is settled in a running game, not loading. */
bool IsSetCompletionBaselineArmed();

/** @brief Forgets the baseline entirely - leaving the game. The next check records, never rings. */
void ResetSetCompletionBaseline();

} // namespace oracool
} // namespace devilution
