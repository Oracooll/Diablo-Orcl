# The first Release build, and the two bugs only it could find (v1.9.31)

**Date:** 2026-08-24
**Version:** 1.9.31 (unchanged — see below)
**Tests:** 512/514 Debug — the two standing baseline failures.
**Status:** Release built, packaged, published as a private GitHub release. Not played.

Asked for a Release build, a zip, and an upload. Standing instruction had been Debug only; this
supersedes it.

## Release would not compile, and Debug never would have told us

Two errors, same root cause, and both are header hygiene rather than anything to do with
optimisation. Debug hid them because a translation unit only fails if it reaches the header by a
path where the type is not yet defined, and the two configurations do not compile the same set of
units in the same order.

**`inv.h` named `SpellID` without saying where it comes from.** `CanUseScroll` and `CanUseStaff`
take one. Every Debug translation unit that reached `inv.h` happened to have seen `spelldat.h`
already; Release had one that had not.

Including `spelldat.h` fixes the symptom and creates a cycle — `spelldat.h` → `effects.h` →
`player.h` → back round — so on the second entry the include guard hands `spelldat.h` an empty file
and its *own* `SpellID` goes undefined. The build got worse, not better. A forward declaration is
the right answer: a scoped enum with a fixed underlying type is complete once declared, which is all
a by-value parameter needs, and it adds no edge to the include graph.

**`diablo.h` had the same problem while already including the fix.** It includes `spelldat.h` at
line 12 and still could not see `SpellID` at line 99 — because the cycle above means a unit entering
`spelldat.h` first arrives at `diablo.h` with `spelldat.h` still in progress. Same forward
declarations, kept alongside the include.

The cycle itself is inherited from DevilutionX and is not fixed here. Breaking it properly is its
own job; these two declarations are what it takes to build Release, which is where it first bites.

Debug rebuilt and the full suite re-run afterwards — 512/514, unchanged. The declarations are inert
where the includes already worked.

## What went in the zip, and what deliberately did not

`DiabloOrcl-v1.9.31-win64.zip`, 38.3 MB, 200 entries.

The build directory contains **493 MB of `diabdat.mpq`** and four Hellfire archives. None of it is
ours, and none of it is in the package. The zip carries the game binary, the seven DLLs the
executable actually imports, the fork's own `assets/`, and `oracool.mpq` — Oracool's own art, which
the asset loader searches ahead of every other archive so it overrides the original game without
touching it.

`discord_game_sdk.dll` ships in the build directory but the executable does not import it, so it is
not in the zip either — 3.3 MB for nothing.

A `README.txt` says plainly that `diabdat.mpq` is missing on purpose, that the player supplies it
from a copy of Diablo they own, and that saves live in `%APPDATA%` so a build can be replaced in
place. The zip is checked after packing for any of the five commercial archives; that check is part
of the packaging, not a one-off.

## The version was NOT bumped

Standing rule is to bump the patch on every build. Not here: the Release executable is stamped
1.9.31, the repository at the tagged commit says 1.9.31, and the tag is `v1.9.31`. A bump would have
made the shipped binary disagree with the source it was cut from, which is worse than a skipped
increment. The rule resumes on the next development build.

## What is in this release that nobody has played

All of it, and that is worth saying loudly: v1.9.26–1.9.31 rebuilt the shop from a text list into a
paged icon grid with drag-to-sell, a buyback tab, and icon services. Three audit passes found seven
defects and fixed them, but no one has clicked a single button. This is a build to test, not a build
to trust.
