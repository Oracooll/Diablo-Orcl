/**
 * @file oracool/skill_sounds.h
 *
 * The six-class skill sound library, and the set-completion stinger.
 *
 * 304 WAVs covering every node of all six class trees, delivered as data with an authoritative
 * manifest (Oracool.MPQ/02-source-art/skill-sounds/class-skill-sounds.zip). The manifest is read at
 * BUILD time by tools/GenSkillSounds.ps1, which joins it to the class tree on (class, skill name)
 * and emits skill_sounds_data.inc. Nothing here is hand-typed, and a sound whose skill name matches
 * no tree row is a generator error rather than a cue that silently never plays.
 *
 * The package ships its own integration contract. Where this follows it, and where it deliberately
 * does not:
 *
 *   - "Missing sound IDs must fail softly and never cancel skill execution." Every function here
 *     returns quietly on a miss. A skill with no cue is normal: the two borrowed Paladin skills
 *     (Hammer of Faith, Blessed Shield) were added to the tree after the design sheets the package
 *     was built from, so they have no sounds at all.
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
 * cast hook in StartSpell plays the vanilla sSFX only when the tree row has nothing of its own, and
 * cannot ask "did that ring?" any other way without repeating the lookup.
 */
bool PlaySkillSound(ClassTreeSkill skill, SkillSoundEvent event);

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

/** @brief The shared equipment-set completion stinger, `ui.set.complete`. */
void PlaySetCompleteSound();

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

/** @brief Forgets the baseline entirely - leaving the game. The next check records, never rings. */
void ResetSetCompletionBaseline();

} // namespace oracool
} // namespace devilution
