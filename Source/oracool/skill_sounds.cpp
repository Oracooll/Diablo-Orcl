#include "oracool/skill_sounds.h"

#include <cstddef>
#include <memory>

#include "engine/sound.h"
#include "engine/sound_defs.hpp"
#include "oracool/item_sets.h"
#include "player.h"

namespace devilution::oracool {

namespace {

using Skill = ClassTreeSkill;

// The table itself. GENERATED - see tools/GenSkillSounds.ps1 and the note in skill_sounds.h.
#include "oracool/skill_sounds_data.inc"

constexpr size_t SkillSoundCount = sizeof(SkillSounds) / sizeof(SkillSounds[0]);

/** @brief The set-completion stinger. Not in the generated table - it belongs to no skill. */
constexpr char SetCompletePath[] = "sfx\\ui\\set-complete.wav";

// The contract's mix defaults, in the engine's units: VOLUME_MIN is -1600 for -16 dB, so one
// hundredth of a decibel each. These are starting points to be tuned in play against weapons,
// monsters and music - not baked into the waveforms, which are all normalised to -1.5 dBFS.
constexpr int VolumeOneShot = -200; // -2 dB
constexpr int VolumeLearnStop = -400; // -4 dB
constexpr int VolumeLoop = -1200; // -12 dB

/**
 * @brief Lazily loaded, then kept.
 *
 * One slot per generated row, indexed by the row itself, so there is no map and no hashing. 304
 * pointers is a rounding error next to the samples they point at, and a skill's cue is loaded the
 * first time it actually fires rather than all 13 MB at startup.
 */
std::unique_ptr<TSnd> SoundCache[SkillSoundCount];
std::unique_ptr<TSnd> SetCompleteSound;

/** @brief The running aura loop, and which skill owns it. See the header on why one is enough. */
std::unique_ptr<TSnd> AuraLoop;
Skill AuraLoopSkill = Skill::None;

/** @brief Which sets were complete last time we looked. Bit i is ItemSets[i]. */
uint16_t CompletedSetsMask = 0;
static_assert(ItemSetCount <= 16, "CompletedSetsMask needs a wider type");
/**
 * @brief Whether CompletedSetsMask means anything yet.
 *
 * Bug (fixed 2026-08-17, user: "dont play set completed sound on game load"). The FIRST CalcPlrInv
 * of a character's life runs from the player-file load - before LoadGameLevel, where the baseline
 * used to be armed - so a character loaded wearing a complete set compared it against the static-
 * init mask of zero and rang. Until armed, a check RECORDS instead of ringing, whichever code path
 * gets there first; the flag drops on leaving the game so the next character starts mute too.
 */
bool BaselineArmed = false;

/** @brief Index into SkillSounds, or SkillSoundCount for "no such cue". */
size_t FindSound(Skill skill, SkillSoundEvent event)
{
	// Linear over 304 rows, on events that happen at human speed - a cast, a hit, a point spent.
	// Sorted by class then skill then event, so a binary search would be possible; it would also be
	// a second thing to keep true, for a lookup that never runs in a hot loop.
	for (size_t i = 0; i < SkillSoundCount; i++) {
		if (SkillSounds[i].skill == skill && SkillSounds[i].event == event)
			return i;
	}
	return SkillSoundCount;
}

TSnd *LoadCached(std::unique_ptr<TSnd> &slot, const char *path)
{
	if (slot == nullptr)
		slot = sound_file_load(path);
	// sound_file_load returns an unloaded TSnd for a missing file rather than nullptr, and
	// snd_play_snd checks IsLoaded itself - so a missing WAV is already a silent no-op. Returning it
	// anyway keeps the "fail softly" rule in one place instead of two.
	return slot.get();
}

} // namespace

void PlaySkillSound(Skill skill, SkillSoundEvent event)
{
	const size_t index = FindSound(skill, event);
	if (index == SkillSoundCount)
		return; // no cue for this pair; normal, and never an error

	const int volume = event == SkillSoundEvent::Learn || event == SkillSoundEvent::Stop
	    ? VolumeLearnStop
	    : VolumeOneShot;
	snd_play_snd(LoadCached(SoundCache[index], SkillSounds[index].path), volume, 0);
}

void StartClassAuraLoop(Skill skill)
{
	// Atomic replacement, in the contract's order: the old loop and its stop cue go first, so two
	// auras are never audible at once even for a frame.
	StopClassAuraLoop();
	PlaySkillSound(skill, SkillSoundEvent::Start);
	ResumeClassAuraLoop(skill);
}

void ResumeClassAuraLoop(Skill skill)
{
	SilenceClassAuraLoop();

	const size_t index = FindSound(skill, SkillSoundEvent::Loop);
	if (index == SkillSoundCount)
		return; // a persistent skill with a start but no loop is fine - the start just rings once

	AuraLoop = sound_file_load(SkillSounds[index].path);
	if (AuraLoop == nullptr || !AuraLoop->DSB.IsLoaded()) {
		AuraLoop = nullptr;
		return;
	}
	// Not through snd_play_snd: that plays exactly one iteration and can hand the sample off to a
	// duplicate when it is already playing, which would leave nothing here to stop later. A loop
	// needs an owned handle, so it is driven directly.
	AuraLoop->DSB.SetVolume(VolumeLoop, VOLUME_MIN, VOLUME_MAX);
	AuraLoop->DSB.Play(0); // 0 iterations means loop forever
	AuraLoopSkill = skill;
}

void StopClassAuraLoop()
{
	if (AuraLoop == nullptr && AuraLoopSkill == Skill::None)
		return;
	const Skill stopping = AuraLoopSkill;
	SilenceClassAuraLoop();
	if (stopping != Skill::None)
		PlaySkillSound(stopping, SkillSoundEvent::Stop);
}

void SilenceClassAuraLoop()
{
	if (AuraLoop != nullptr) {
		AuraLoop->DSB.Stop();
		AuraLoop = nullptr;
	}
	AuraLoopSkill = Skill::None;
}

void PlaySetCompleteSound()
{
	snd_play_snd(LoadCached(SetCompleteSound, SetCompletePath), VolumeOneShot, 0);
}

namespace {

uint16_t CompletedSets(const Player &player)
{
	uint16_t mask = 0;
	for (size_t i = 0; i < ItemSetCount; i++) {
		if (WornSetPieces(player, ItemSets[i]) >= ItemSets[i].itemCount)
			mask |= static_cast<uint16_t>(1U << i);
	}
	return mask;
}

} // namespace

void CheckSetCompletionTransition(const Player &player)
{
	if (&player != MyPlayer)
		return; // local UI feedback for the owning player; peers get no replicated cue

	// Nothing counts until LoadGameLevel has armed the baseline. The first version of this guard
	// self-armed on the first check instead - and still rang (user, 2026-08-17: "still chime on
	// load"), because the load pipeline calls CalcPlrInv MORE THAN ONCE while equipment is landing
	// piece by piece: the first call recorded a half-dressed character and the next saw the set
	// "become" complete. Only the arm at the END of LoadGameLevel is the character's real settled
	// state, so until that runs, a check is a no-op rather than a recording.
	if (!BaselineArmed)
		return;
	const uint16_t now = CompletedSets(player);
	// Rising edges only. Removing a piece clears its bit and so REARMS the set, which is what the
	// contract asks for; swapping one valid piece for another inside a single transaction never
	// drops the bit in the first place, so it cannot manufacture a false edge.
	const uint16_t newlyComplete = static_cast<uint16_t>(now & ~CompletedSetsMask);
	CompletedSetsMask = now;
	if (newlyComplete != 0)
		PlaySetCompleteSound();
}

void ArmSetCompletionBaseline(const Player &player)
{
	if (&player != MyPlayer)
		return;
	// Records without ringing: loading a character who is already wearing a complete set is not an
	// achievement they just earned.
	CompletedSetsMask = CompletedSets(player);
	BaselineArmed = true;
}

void ResetSetCompletionBaseline()
{
	// Leaving the game: the mask describes a character who is no longer here, and the next one's
	// first check must record rather than ring against it.
	CompletedSetsMask = 0;
	BaselineArmed = false;
}

} // namespace devilution::oracool
