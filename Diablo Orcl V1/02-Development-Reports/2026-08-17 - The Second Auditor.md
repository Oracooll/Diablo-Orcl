# The Second Auditor

**Version:** 1.7.69
**Date:** 2026-08-17
**Scope:** the external audit's findings - a risk-based pass over 355 commits and 1,035 changed
files against the DevilutionX 1.5.5 baseline, run by a separate process while my own four internal
rounds were closing. Every claim was re-verified against the source before anything was changed;
this report is the verdict sheet.

Where my rounds audited what shipped THIS session, the outside pass went wide - and it found real
things mine never looked at, because they lived in code untouched for weeks. The score, honestly
kept: nine claims confirmed and fixed, two confirmed and hardened, one half-misread, three cleared
with reasons, two known-and-deferred, one unlocatable (hardened anyway).

---

## FIXED: the infinite skill-point loop (blocker 1 - confirmed, worse than claimed)

`TotalInvestedSkillPoints` summed only `_pSkillInvestment` - the slotted store. The class tree's
slotless passives, masteries and auras keep their ranks in `_pClassTreeInvestment`, invisible to
the ledger. Three consequences, one worse than the audit stated:

- `EnsureRetroactiveSkillPoints` (every game start) computed `owed - have` with `have` blind to
  passive investment - **invest in passives, relog, and the points come back**. Repeatable
  forever.
- Adria's respec undercharged (cost = 500 x a total that missed the passives).
- `RefundAllSkillPoints` zeroed only the slotted store: passives kept their ranks AND the next
  start re-granted their cost. The respec was itself a duplication engine.

The ledger now sums both stores; the refund empties both, puts out the active aura (any aura is
rank 0 after a full refund - the same teardown `RefundClassTreePoint` applies one rank at a
time), and the respec recalculates the player before the store screen returns. Pinned by
`OracoolSkillPoints.LedgerCountsBothStoresAndRefundsBoth`, which self-calibrates against
`EnsureRetroactiveSkillPoints` so it cannot drift from `SkillPointsPerLevel`.

## FIXED: gems double-dipped their quality (major 6 - confirmed, textbook)

`ApplyGemToTotals` wrapped every field but one in `at(at(...))` - the quality percentage applied
twice, squaring the scale, while `GemSocketLine`'s tooltip applied it once. A Perfect gem's 200%
was mechanically 400%: a ruby *displaying* 4-12 fire damage *granting* 8-24. The single-wrapped
`dexterity` field is what proved the doubling a copy-paste accident rather than a design. One
`at()` per number now, exactly the tooltip's arithmetic; pinned by
`OracoolGems.QualityScalesEffectsOnceNotTwice` (weapon and armor hosts, against an independently
restated `AtQuality`).

## FIXED: the remapper would destroy gems, charms and runes (blocker 2 - confirmed)

The Diablo/Spawn save remaps passed Oracool ids through as themselves via `IsOracoolItemIdx` -
a range ending at the tier armor. The Phase 1 gems, charms and runes were appended PAST that
range, so `RemapItemIdxToDiablo` sent every one of them into the "Hellfire exclusive" band and
out as -1: **the empty-slot marker, the exact bug the passthrough was built to fix**, re-shipped
for the newer ranges because a hand-bounded range was outgrown by appending.

The remaps now test `IsOracoolAddedIdx` - the whole fork-owned tail, `IDI_ORACOOL_SHOULDERS`
through `IDI_LAST`, which appending cannot outgrow. (Reachability required Diablo mode; see the
next item - fixed anyway, because a save format must not destroy items under ANY flag state.)

## DECIDED: Diablo mode is now explicitly impossible (blocker 8)

The audit offered two exits: support both modes, or disable Diablo mode explicitly. Everything
this fork is - class-tree spell ids past the Diablo cutoff, expansion uniques past index 89,
25 waypoints, Nest and Crypt - sits behind `gbIsHellfire`. A Diablo boot (reachable via
`--diablo` or a stale `gameMode=Diablo` ini value) was a half-broken game nobody intended.

`DiabloParseFlags` now forces `forceHellfire = true` unconditionally (before the archives load,
so init.cpp's existing missing-hellfire dialog fires on a bad install), logs and ignores
`--diablo`, and DiabloInit migrates a stored `Diablo` ini value to `Hellfire` the same way it
already migrated `Ask`. The audit's spells.cpp/items.cpp cutoff concerns are moot by
construction - no code path can see `gbIsHellfire == false` in a running game.

## FIXED: sidecar files now carry the item schema they were written with (blocker 3)

The stash comments record the lesson twice: *"every item embedded is subject to
OracoolItemFormatVersion... the two versions must move together."* The rule was applied to the
stash (versions 4 and 5) and MISSED for `heroinvtabs` - still advertising container version 2
while the item record grew twice (sockets, the ethereal flag). A pre-socket tab file passed the
equality gate and misparsed at today's byte boundaries: the "+11126% fire resist" class of
corruption, waiting.

A rule held by memory fails; version 3 of the tab file and version 6 of the stash **embed
`OracoolItemFormatVersion` as a second header byte, checked on load** - a future item-format
bump orphans stale sidecars by itself, with no one needing to remember anything. The current
build's own pre-audit output (tab v2, stash v5) is still accepted and parsed with today's format
- which is the format those files were actually written in, since autosave rewrites both
constantly - so nobody's tabs or stash vanish on upgrade; the next save rewrites them in the
new form. `IsStashSizeValid` learned the version-dependent header size.

## FIXED: a poisoned tab file could reach an out-of-bounds write (blocker 4)

A crafted (or torn) `heroinvtabs` could declare 70 items over an empty grid. The loader accepted
it; the first paste into the apparently-free tab then wrote `InvTabList[t][70]` - one past the
array. Three layers now:

- **Loader consistency:** every declared item must own a positive anchor cell in the grid it was
  saved beside (a written item always has exactly one). A record inconsistent with itself has its
  tab emptied - after its item records are consumed, because they are variable-length and
  skipping them blind would misalign every tab after.
- **Post-validation scrub:** cells referencing an item `LoadAndValidateItemData` cleared are
  zeroed - the tab equivalent of `RemoveEmptyInventory`.
- **Sink guards:** the paste append and `AutoPlaceItemInExtraTabSlot` both refuse to grow a list
  already at `InventoryGridCells`, whatever the grid claims.

## FIXED: every hero preview wore the selected hero's items (blocker 5)

`LoadHeroItems` and `LoadInventoryTabs` opened `gSaveNumber` - the global - while the
hero-select loop iterates slot `i`. Every preview showed the SELECTED slot's equipment, tabs and
sockets. And the loop never read the extension chunks, so previews also lacked skill points and
tree passives. Both loaders now take the save slot as a parameter (the real load path passes the
same number the global holds - unchanged behavior, now by contract rather than luck), and the
preview loop applies the hero's chunk tail before `CalcPlrInv`, the same sequence
`pfile_read_player_from_save` runs.

## FIXED: the smith repaired equipped ethereals; new slots never wore out (part of 9)

`SmithRepairOk` turned ghosts away from the inventory walk while the four body-slot adds had no
ethereal check - an equipped ethereal was repairable at the smith, contradicting its own
"cannot be repaired" line and the Repair spell's decline. And the six Oracool worn slots never
took durability damage at all, which made durability - and ethereal's half-lifespan bargain -
meaningless there.

One table now, `RepairableBodySlots`, read by both `StartSmithRepair` and `SmithRepairItem` (the
encoding is -(index+1), preserving the historical -1..-4): all ten durability-bearing slots
repairable, ethereals refused everywhere. `DamageArmor` spreads its wear uniformly across every
worn, non-indestructible armor piece instead of head/chest alone - one point per trigger, same
total wear rate, more gear sharing it; an indestructible chest no longer shields the rest of the
outfit from ever wearing. (`BreakOrRemoveEquipment` was already slot-generic; in single-player
it marks broken-in-place.)

## FIXED: the F-key edges (rest of 9)

- **Charged spells.** The binding stored identity as Skill-or-Spell only; a staff-only spell
  typed as Spell cast down the memorized path and died on the `Fail_Level0` gate while the staff
  sat charged in hand. The identity derivation now has all three kinds - innate Skill, memorized
  Spell, staff Charges - at bind time AND at load (`ReadiedSpellType` gained the Charges branch;
  the masks it reads at load are freshly rebuilt, so a still-equipped staff is a live fact, not
  a stale stored type; an unequipped one drops the binding as before).
- **SHIFT clearing.** SHIFT+FX only cleared while hovering the exact bound ability and silently
  swallowed the press everywhere else - an "unassign" that mostly did not. With the sheet open,
  SHIFT+FX now clears slot X unconditionally, hover or no hover.

## HARDENED: dormant but dangerous (build/security section)

- **`recv_plrinfo`** memcpy'd wire-supplied offset+length into a fixed `PlayerNetPack` with no
  size check - a remote out-of-bounds write. Both figures must now prove
  `offset + bytes <= sizeof(pack)` before anything happens. Multiplayer being hidden from V1's
  UI made this dormant, not gone.
- **The wire version** advertised DevilutionX 1.5.5 while the wire structs grew past vanilla's
  shape - a stock client could version-match and then corrupt. The wire identity is now the
  Oracool version (split into numeric components in CMake; `IsGameCompatible` compares the same
  numbers, and the mismatch message names the Orcl version).
- **The hero-chunk walk** used additive bounds checks (`offset + 6 + chunkLen > len`) that wrap
  on a 32-bit `size_t` for near-UINT32_MAX lengths - an attacker-steered loop over a hostile
  hero file. Subtraction-form checks now, which cannot overflow, and the advance is widened.
- **The MPQ packer** (the audit said "extractor"; no extractor exists in this tree - the packer
  is the closest thing) accepted `..` and absolute paths from its listfile, which would read
  files from anywhere on disk into the archive. Refused now, both input modes.
- **Packaging** ships `oracool.mpq` when present: `install(FILES ... OPTIONAL)` beside
  `devilutionx.mpq` in both the Windows and Linux CPack blocks. Without it a packaged game was
  missing most of its UI and content.

## CLEARED, with reasons

- **"Rank-zero skills are assignable."** Deliberate. A known ability at rank 0 can be bound; the
  cast lands on the engine's own `Fail_Level0` gate with feedback, and the binding starts
  working the moment a point goes in. Pre-binding is a feature, and the same reasoning cleared
  refund-to-zero in my round one.
- **"Unimplemented skills are assignable."** They are not: the tree's hover gate is
  `data.implemented && IsValidSpell(slot) && IsSpellKnown(slot)` - the same gate the
  click-to-ready path applies. Nothing without a real, known SpellID can become
  `HoveredAbilitySpell`.
- **"The set drop hook never selects a named set definition"** - half a misread. The hook at
  `TrySpawnOracoolSetItem` is the TIER-item drop path (the eight product lines), working as
  designed. The NAMED item-sets having no drop path is true and is the standing, documented
  deferral ("no drops, no gold mechanic - don't resume unasked"). One real nuance folded into
  the record: `BaseItemForSetSlot` maps every `main_hand` to a one-handed sword base, so a
  set piece flavored as a bow would be mechanically a sword - part of the same "missing bases"
  gap (amulet/ring/relic/cloak, and now weapon families), for the day sets resume.

## DOCUMENTED, deliberately not churned

- **CI workflows target `master`** and Windows packaging scripts expect `devilutionx.exe`. True,
  and now an explicit decision instead of an accident: this fork's process is local Debug builds
  with the full suite per bump - 446 tests on every version - and the upstream matrix (3DS, Vita,
  Amiga...) would burn Actions minutes failing on assets it cannot have. The workflows stay
  dormant. If release automation is ever wanted, that is its own unit of work, and the exe name
  goes into it.
- **The `install(TARGETS)` rules** already ship the right exe name (they use the target, which
  carries `OUTPUT_NAME DiabloOrcl`).

## Verification

Debug build clean at **1.7.69**. Suite **444 of 446** - two tests added this round
(`QualityScalesEffectsOnceNotTwice`, `LedgerCountsBothStoresAndRefundsBoth`), the two failures
the standing baseline pair (`Drlg_l1.CreateL5Dungeon_diablo_3_844660068`,
`Timedemo.WarriorLevel1to2`). `Writehero` still green - the chunk-tail format is unchanged, only
its validation arithmetic hardened. `pack_test` green - the remap fix touches only the
non-Hellfire branch, which no longer exists at runtime.

**In-game checks worth a minute when convenient:** armor wear now lands on gloves/boots/etc. and
the smith lists them; an ethereal equipped piece no longer appears in the repair list; SHIFT+F1
clears the binding with the sheet open wherever the cursor is; a staff spell bound to an F-key
casts from charges; hero-select previews show each hero's own gear; Adria's respec empties tree
passives too and the points arrive once, not once per relog.
