# Nine delivered sounds that never once played

2026-09-12 — v1.11.087

## How it was found

The post-RfA asset sweep, run under the standing rule. It was looking for *missing* assets and found
the opposite: nine sounds that ship in the archive, are referenced by code, and have never been
heard.

## The bug

`Source/oracool/skill_sounds.cpp` — `UiEventPaths[]`, written with **single** backslashes from the
day the table was added:

```cpp
"sfx\ui\salvage.wav",
```

In C++ `\u` opens a universal-character-name (ill-formed without four hex digits) and `\s` is not an
escape at all. MSVC warns — C4429 and C4129 — and drops both backslashes.

Not inferred. Compiled that exact literal and printed it:

```
[sfxuisalvage.wav] len=16
```

Sixteen characters, no separators, a filename no archive has ever contained.

## Why nobody noticed for fifty versions

Because the design is forgiving **by intent**. `PlayUiEventSound` returns false when a file will not
load, and every caller then plays the vanilla sound it used before — `inv.cpp:713` and `:739`,
`levski_roar.cpp:1082` and `:1143`. Their comments even say so: *"the old stand-in, if the salvage
sound is not in the archive"*.

So salvaging clanged like a shield, absorbing an orb sounded like a spell, and four events that were
supposed to gain a voice in v1.11.036 stayed silent. Nothing sounded broken enough to look at. The
graceful fallback that made the feature safe to ship is exactly what hid its failure.

`SetCompletePath` a few lines above was written `"sfx\\ui\\set-complete.wav"` and is the only fork UI
sound that has ever played.

Affected, all nine: salvage, transmute, orb-absorb, socket, runeword-complete, milestone,
encounter-cleared, map-unseal, signet-use. Introduced with the first five at v1.11.032 (RfA-03) and
extended by four at v1.11.036 (RfA-04).

## The test, and why reading the source could not do this

`OracoolAudit.EveryUiEventSoundPathSurvivedTheCompiler` asks what the strings **became**, which is
the only place the bug is visible — the backslash is eaten by the compiler, not by the eye. It
checks the `sfx\ui\` prefix, the `.wav` suffix, that no path collapsed to `sfxui…`, that an
out-of-range value answers `nullptr`, and then mounts the archive and asserts `FindAsset` finds all
nine. That last half proves the fix rather than the spelling.

`UiEventSoundPath` and `UiEventSoundCount` are exported for it. The header says why: a wrong path
here is invisible at runtime by design.

**My own test had the same bug on its first run.** The heredoc writing it collapsed `"sfx\\ui\\"` to
`"sfx\ui\\"`, so it searched for `sfxui\` and failed — the sixth time today that backslash
collapsing has bitten, and the memory note has said to use the Edit tool or `\x5c` since the first.
It failing loudly is the argument for the test existing.

## Swept for the same class of bug elsewhere

A regex over every string literal in `Source/**` for a single backslash followed by anything that is
not a valid escape: **zero other hits.** This was the only occurrence.

## Verification

- The failure reproduced in isolation and the fix confirmed by the archive lookup in the test.
- No C4429 or C4129 remains in the build.
- **724/724 tests pass** (up from 723).
- Both archives repacked; Debug and Release build clean; RTM refreshed.

## What to listen for in play

Nine events that have never made their own sound now should: salvaging an item and transmuting at
Levski's Roar, absorbing a Mystic Orb, socketing a gem, completing a runeword, hitting a milestone,
clearing a named encounter, unsealing a map, and using a Signet of Learning. If any still sounds like
the old stand-in, that one file is genuinely missing from the archive rather than misspelled.
