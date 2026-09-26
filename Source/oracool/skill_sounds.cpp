#include "oracool/skill_sounds.h"

#include <cstddef>
#include <iterator>
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
 * One slot per generated row, indexed by the row itself, so there is no map and no hashing. 306
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
	// Linear over 306 rows, on events that happen at human speed - a cast, a hit, a point spent.
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

bool PlaySkillSound(Skill skill, SkillSoundEvent event)
{
	// No audio device, no sound - the same gate PlaySFX has. Without it a mounted archive and an
	// uninitialised mixer met in a divide by zero (external audit, 2026-09-06: QA-01, the class
	// tree tests after any test that mounted the archives).
	if (!gbSndInited)
		return false;
	const size_t index = FindSound(skill, event);
	if (index == SkillSoundCount)
		return false; // no cue for this pair; normal, and never an error

	const int volume = event == SkillSoundEvent::Learn || event == SkillSoundEvent::Stop
	    ? VolumeLearnStop
	    : VolumeOneShot;
	snd_play_snd(LoadCached(SoundCache[index], SkillSounds[index].path), volume, 0);
	return true;
}

const char *SkillSoundPath(Skill skill, SkillSoundEvent event)
{
	const size_t index = FindSound(skill, event);
	return index == SkillSoundCount ? nullptr : SkillSounds[index].path;
}

bool HasSkillSound(Skill skill, SkillSoundEvent event)
{
	return FindSound(skill, event) != SkillSoundCount;
}

void StartClassAuraLoop(Skill skill)
{
	// Atomic replacement, in the contract's order: the old loop and its stop cue go first, so two
	// auras are never audible at once even for a frame.
	StopClassAuraLoop();
	// An aura delivered with only a CAST cue (Static Field; the Bard's Discord, Tale of Heroes, Weaken) was
	// silent when lit, because this asked for Start alone (asset audit, 2026-09-26). Its cast is its start.
	if (!PlaySkillSound(skill, SkillSoundEvent::Start))
		PlaySkillSound(skill, SkillSoundEvent::Cast);
	ResumeClassAuraLoop(skill);
}

void ResumeClassAuraLoop(Skill skill)
{
	SilenceClassAuraLoop();
	if (!gbSndInited)
		return; // see PlaySkillSound

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

Skill CastingSkill = Skill::None;

void BeginSkillCast(Skill skill)
{
	CastingSkill = skill;
}

void EndSkillCast()
{
	CastingSkill = Skill::None;
}

Skill CurrentCastSkill()
{
	return CastingSkill;
}

namespace {

/**
 * @brief The nine fork UI event sounds, by UiEventSound order.
 *
 * DOUBLE backslashes. Every one of these was written with a single backslash from the day the table
 * was added (v1.11.032) until 2026-09-12, which meant **none of the nine ever played**:
 *
 *     "sfx\ui\salvage.wav"   ->  sfxuisalvage.wav
 *
 * In C++ `\u` opens a universal-character-name and `\s` is not an escape at all, so MSVC warns
 * (C4429, C4129) and drops both backslashes - verified by compiling that exact literal and printing
 * it, which gives `sfxuisalvage.wav`, 16 characters. The WAVs were in the archive the whole time
 * under their real names; the paths asking for them were the problem.
 *
 * It stayed invisible because the design is deliberately forgiving: PlayUiEventSound returns false
 * when a file will not load, and every caller then plays the vanilla sound it used before
 * (inv.cpp, levski_roar.cpp). So nine delivered assets were silently substituted for ~50 versions
 * and nothing ever looked broken. SetCompletePath above was written correctly and is the only fork
 * UI sound that has ever been heard.
 *
 * The test pins the shape of these strings for exactly that reason: a stripped backslash cannot be
 * seen by reading the line, only by asking what the compiler made of it.
 */
constexpr const char *UiEventPaths[] = {
	"sfx\\ui\\salvage.wav",
	"sfx\\ui\\transmute.wav",
	"sfx\\ui\\imbue.wav", // RfA-18 batch 41 (2026-09-19); orb-absorb.wav until then
	"sfx\\ui\\socket.wav",
	"sfx\\ui\\runeword-complete.wav",
	"sfx\\ui\\milestone.wav",
	"sfx\\ui\\encounter-cleared.wav",
	"sfx\\ui\\map-unseal.wav",
	"sfx\\ui\\signet-use.wav",
	"sfx\\ui\\rift_open.wav",  // RfA-19 batch 42 (2026-09-20)
	"sfx\\ui\\rift_close.wav", // RfA-19 batch 42 (2026-09-20)
	"sfx\\ui\\cube_open.wav",      // RfA-20 batch 43d (2026-09-20)
	"sfx\\ui\\cube_transmute.wav", // RfA-20 batch 43d (2026-09-20)
};
std::unique_ptr<TSnd> UiEventCache[std::size(UiEventPaths)];

} // namespace

const char *UiEventSoundPath(UiEventSound sound)
{
	const size_t i = static_cast<size_t>(sound);
	if (i >= std::size(UiEventPaths))
		return nullptr;
	return UiEventPaths[i];
}

bool PlayUiEventSound(UiEventSound sound)
{
	if (!gbSndInited)
		return false;
	const size_t i = static_cast<size_t>(sound);
	if (i >= std::size(UiEventPaths))
		return false;
	TSnd *snd = LoadCached(UiEventCache[i], UiEventPaths[i]);
#ifndef NOSOUND
	// A missing WAV loads as an unloaded TSnd, not nullptr - ask, so the caller can fall back.
	if (snd == nullptr || !snd->DSB.IsLoaded())
		return false;
#endif
	snd_play_snd(snd, VolumeOneShot, 0);
	return true;
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
