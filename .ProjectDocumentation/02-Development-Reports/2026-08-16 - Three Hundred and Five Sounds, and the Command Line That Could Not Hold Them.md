# Three Hundred and Five Sounds, and the Command Line That Could Not Hold Them

**Version:** 1.7.55
**Date:** 2026-08-16
**Files:** `Source/oracool/skill_sounds.{h,cpp}` (new), `skill_sounds_data.inc` (generated), `tools/GenSkillSounds.ps1` (new), `tools/oracool_mpq_pack.cpp`, `tools/build_oracool_mpq.cmd`, `class_tree.cpp`, `items.cpp`, `diablo.cpp`, `Source/CMakeLists.txt`

---

## The sweep

Two archives in the MPQ root that had not been consumed:

- **`class-skill-sounds.zip`** — 304 engine-ready WAVs, one or more per node of all six class trees, with `audio-manifest.json`/`.csv`, an integration contract, a sound-design note, and SHA256 sums.
- **`set-complete-sound.zip`** — one shared equipment-set completion stinger with its own trigger contract.

A third, `unique-item-expansion-250.zip`, landed **during** the sweep (23:33, while the sound work was mid-build). It is filed and characterised at the end of this report; it is not integrated.

---

## The join

The sound package covers "all 161 nodes in the supplied skill trees". This fork's tree is now **163** — Hammer of Faith and Blessed Shield were appended after those design sheets were supplied. Matching the manifest's skill names against the tree:

```
tree skills:      163 rows, 161 distinct names
manifest skills:  159 distinct names
tree skills with NO sound:   Blessed Shield, Hammer of Faith
sounds with NO tree skill:   (none)
```

Not one orphan in the other direction. So the join is on **(class, skill name)** — the only key the two sides share, since the package knows nothing about `ClassTreeSkill` values and the tree knows nothing about sound ids. The pair, not the name alone, because two classes can carry the same skill name and a name-only lookup would hand one class the other's cues.

`tools/GenSkillSounds.ps1` does that join at build time and emits `skill_sounds_data.inc`. A manifest row matching no tree row is a **hard error** there, not a cue that silently never plays. It also reads the enum and the table in one pass and checks their lengths against each other, because they are paired positionally and a positional pairing that drifts is silent.

---

## What the contract asked for, and where I departed from it

The package ships an unusually precise integration contract. Followed:

- **Fail softly.** Every entry point returns quietly on a miss. A skill with no cue is normal — see the two above.
- **Atomic aura replacement**, in the stated order: old loop down, old stop cue, new start cue, new loop up.
- **Release the loop on** replacement, level transition, and leaving the game.
- **Mix defaults** — one-shots −2 dB, learn/stop −4 dB, loops −12 dB — in the engine's hundredths-of-a-dB units, where `VOLUME_MIN` is −1600.

Departed from, deliberately:

- **"Key each loop by (player, skill, activation generation)."** Collapsed to **one** module-level handle. This fork stores the active aura as a single value on the player (`_pOracoolActiveAura`), so there cannot be two live loops to tell apart, and a generation counter would be machinery guarding an impossible state. Written down in the header so it grows back correctly if auras ever stack.
- **Voice limits and per-caster caps**: not implemented. `snd_play_snd` already drops any retrigger inside 80 ms, which subsumes the contract's 70 ms floor for multi-hit skills, and the mixer caps duplicates itself.
- **A `Resume` that is not a `Start`.** The contract says loops are "reconstructed from validated active state after load". Reconstruction is not activation — the aura never went out — so `ResumeClassAuraLoop` re-attaches the loop without replaying the start cue. A level change should not sound like lighting an aura you already had lit.

---

## Where the cues fire

| Cue | Trigger | Notes |
|---|---|---|
| `learn` | `InvestClassTreePoint` | Only passives/masteries have one; an active's confirmation is its first cast, so most skills fall through silently |
| `start` / `loop` / `stop` | `ToggleClassAura` | 24 implemented auras |
| `ui.set.complete` | end of `CalcPlrInv` | rising edge only |
| `cast` / `impact` / `arrive` | **not yet wired** | see below |

**The set stinger's edge detection** was the fussiest part, because the contract is emphatic about what must *not* ring it: a recalculation, a load, a preview, a cancelled move, or swapping one valid piece for another. So it compares a remembered 15-bit mask rather than asking "is anything complete right now", and it sits at the **end** of `CalcPlrInv` — after stat flags are recomputed and unusable items demoted. A set whose last piece the wearer cannot actually use is not complete, and asking any earlier would have rung for it. `ArmSetCompletionBaseline` records without ringing, at both ends of a level load.

**`cast`/`impact` are not wired yet.** 27 of the tree's actives are implemented, but their cast sites live in five separate Paladin modules with their own hit-resolution paths, and the contract is specific that an area skill plays *one* impact per resolved cast rather than one per monster. Wiring that correctly is its own unit of work; wiring it hastily would produce exactly the per-target machine-gun the contract warns about. The table already carries all 92 cast and 64 impact rows, so it is a call-site job, not a data job.

---

## The build-tool bug

Packing failed with:

```
The input line is too long.
The syntax of the command is incorrect.
```

`build_oracool_mpq.cmd` accumulated every asset path into one `FILES` variable and passed it on the command line. 79 assets fit; **384 did not** — Windows' ~8191-character limit, reported by `cmd.exe` before the packer was ever reached, naming neither the cause nor the culprit.

The packer now accepts a response file (`@listfile`) and the `.cmd` writes one. Worth noting that the fix is in the **tool**, not the invocation: the asset count only goes up from here.

The list reader strips trailing `\r` explicitly, because the `.cmd` writes the file with `echo` and a stray carriage return would have become part of the archive path — an asset that packs successfully and is then unfindable at runtime.

---

## Verification

- Debug build clean at 1.7.55; `skill_sounds.cpp.obj` confirmed built and linked rather than silently absent from the CMake list.
- Generator: 304 manifest rows, 163 enum entries, 163 table rows, **0 unmatched**.
- MPQ repacked: **384 files, 34.0 MB**. Archive paths spot-checked against the generated table — `sfx\skills\barbarian\combat-skills\bash-cast.wav` on both sides.
- Full suite **441 of 443**; the two failures are the standing baseline pair.

Not verified: how any of it actually *sounds*. That needs the game running, which is yours.

---

## Filed

The root is down to `README.md`. Everything else went to `02-source-art/`:

- `skill-sounds/` — both audio archives
- `unique-items/` — the new 250-item package
- `item-sets/` — 15 root copies removed after hash-comparing each against the filed copy (identical; the generator reads the filed ones)
- `04-references/` — the upstream devilutionx font pack

---

## The package that arrived mid-sweep

`unique-item-expansion-250.zip` — **250 original uniques** for the 13-slot model, as data only (no sprites, deliberately). It carries a JSON schema, a CSV import table, an affix registry, a catalog, a balance/drop policy, and an implementation classification per item:

| Tier | Items | What it needs |
|---|---:|---|
| `native_table` | 94 | stock bases, stock powers — nothing new |
| `extended_slot_stats` | 67 | the new equipment slots, which this fork already has |
| `reusable_hooks` | 89 | bounded hook IDs from the affix registry |

The package states that no item requires a bespoke code path, which — if it holds up — makes the first 161 largely a data-import job of the same shape as the item sets. Two things to check before believing it: 23 of the items sit in `relic` and `cloak` slots, which this fork has **not** built (the same gap that leaves 21 set items unspawnable), and the 89 `reusable_hooks` items need their registry audited against real channels the way the set keywords were — that audit is what turned up 45 empty bonus tiers yesterday.
