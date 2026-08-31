# Audit of the whole folder, and the name that lost its V1 (v1.9.143–144)

**Date:** 2026-08-31
**Versions:** 1.9.143, 1.9.144
**Tests:** 594/595 (the standing `Drlg_l1.CreateL5Dungeon_diablo_3_844660068`)

## The ask

> i am away. in the mean time audit and optimize the whole Diablo Orcl V1 folder. also remove the V1
> from the folder name. the project will be from now on known as Diablo Orcl. also reaudit
> Oracool.MPQ folder in onedrive and optimize if necesary.

---

## 1. The rename

Every product-facing surface now says **Diablo Orcl**: the Windows version resource (`ProductName`,
`FileDescription`), the release README, the packaging banner, all 25 wiki page titles and the wiki
brand, the tool headers, and the skills workbook — file included, now
`Diablo Orcl - Skills, Spells and Auras.xlsx`.

Historical dev reports and the ChatGPT audit files keep the old name. They are dated records of what
was written at the time; rewriting them would make the archive lie about itself. `V0 to V1 Fork.md`
keeps its name too — it is about the fork lineage — with a note that the folder dropped the suffix.

Two scripts carried the folder name in an **absolute path**. Both now derive it from `$PSScriptRoot`:
`ServeWiki.ps1` (which would have 404'd at request time, not at start-up) and
`BuildSkillsWorkbook.ps1`. Only `tools/OracoolPcxWatcher.cfg` still holds a literal path, because a
config file has nowhere to compute one from; it was updated to the new name.

### The folder itself is NOT renamed yet

Windows refuses to rename a directory that is any running process's current directory, and this
session's own is exactly that. The attempt failed with *"the process cannot access the file because
it is being used by another process"*, from a shell that had already left the folder — so the holder
is the agent session host and/or OneDrive, neither of which can release it from in here.

`tools/RenameProjectFolder.ps1` does the whole job from outside:

```
powershell -ExecutionPolicy Bypass -File "tools\RenameProjectFolder.ps1"
```

It refuses if a `Diablo Orcl` already exists, leaves the folder before touching it, and — because
`CMakeCache.txt` records absolute source and binary directories — clears the stale cache and prints
the configure line that reproduces the current settings. Everything else in the repo is already
rename-safe: `CMakeSettings.json` uses `${workspaceRoot}`, and no tracked file references the old
absolute path any more.

The agent memory folder under `~/.claude/projects/` was copied to the new project key ahead of time,
so the 27 memories survive the rename.

---

## 2. Three wiki readers that had been pointed at nothing

The most valuable finding, and all three the same shape: **a reader that finds no files and says
nothing about it.**

| Reader | Was reading | Shipped | Should ship |
|---|---|---|---|
| Dev reports | `<repo>/Diablo Orcl V1/02-Development-Reports` | **0** | **312** |
| Backlog pipeline | `<repo>/Diablo Orcl V1/07-Backlog/Pipeline.md` | **0** | **39** |
| Art gallery | `Packaging/resources/assets` | **5** | **104** |

The first two looked for the documentation vault *beside* the repo under the project's folder name.
The vault lives **inside** the repo as `.ProjectDocumentation`, and has for a long time — so the
Version history page and the Pipeline page have both been publishing empty tables, silently, every
time the wiki was rebuilt.

The third is worse than empty. `Packaging/resources/assets` is DevilutionX's **stock** resource
folder — five `ui_art` buttons. This fork's own art is `oracool_assets`, the 104 PNGs CMake packs
into `oracool.mpq`. Pointed at the stock folder, the gallery listed five vanilla buttons and called
them the art assets, while `wiki/sprites/ui` kept a **frozen copy of the real art** from before the
path changed: images no page referenced any more, still being inlined into every published bundle.

Fixed by naming the vault once (`$vault`, with a warning when it is missing), pointing the gallery at
`oracool_assets`, and clearing `wiki/sprites` before each copy so a deleted asset also leaves the
wiki. Verified in the browser, not by the build log: **312 reports, 39 pipeline entries, 104 sprites,
0 broken images.**

---

## 3. Four windows that outlived their game

The sweep for `ResetLevskiRoarForNewGame`'s siblings (the 2026-08-30 audit fixed two; nobody checked
the rest). The crafting book, the burger row, the skill picker and the waypoint menu all keep their
open flag in a file-local static, and nothing on the way out of a game closes them — so the next
character in the same session started with whatever the last one left up.

Three of those are cosmetic. The waypoint menu is not: it also holds an **unconsumed spawn request**,
set one line before `StartNewLvl` and consumed when the destination level places its sigil. Quit
between the two halves and the flag is still true when the *next* character loads their first level —
which consumes it and drops them on that level's waypoint.

`ResetWaypointMenuForNewGame` clears all three of its statics; the other three windows get their
existing closers called from `FreeGame`. The test asserts through `ConsumeWaypointSpawnRequest`, the
function the level loader itself calls.

---

## 4. Disk

**Freed now: 231 MB.**

- `build/x64-Debug/CMakeFiles/CMakeConfigureLog.yaml` — **210 MB**. A configure log grown across
  hundreds of reconfigures, re-synced by OneDrive every time it changed. Release's copy was 21 MB.
- Ten stray build/test outputs in the repo root (`*.obj` from the MPQ tooling, three
  `Test_FileUtil_*.tmp`). All git-ignored, none referenced.

**Not done, needs a decision:**

- `build/x64-Release` is **1.82 GB** and nine versions stale (last built at 1.9.133; the standing rule
  is Debug-only). Deleting it was blocked by the sandbox. `Saved_Games` inside it holds 16 MB of
  screenshots worth keeping, so the command is:

  ```
  Get-ChildItem "build\x64-Release" -Force | Where-Object { $_.Name -ne 'Saved_Games' } | Remove-Item -Recurse -Force
  ```

- The two build trees each hold their own copy of the game data — `diabdat.mpq` alone is 493 MB
  ×2 — which is ~1.3 GB of Blizzard archives inside a synced OneDrive folder. Debug's copy has to
  stay where it is; Release's goes with the command above.
- `~/.claude/projects/…/35a61f60….jsonl` is **681 MB**. Outside this repo, but it is the biggest
  single file the machine is carrying for this project.

The strongest remaining optimisation is not deleting anything: it is **excluding `build/` from
OneDrive sync**, which is a per-folder setting in the OneDrive client and cannot be scripted safely
from here. Every incremental build rewrites hundreds of megabytes of `.pdb`/`.ilk` that are pure
regenerable output.

---

## 5. Oracool.MPQ — re-audited, nothing to do

1,723 files, 1.78 GB, hashed in full.

- **The root is clean.** Only `README.md`, as the drop-zone rule requires. The 2026-08-31 sweep
  recorded in the README holds.
- **36 duplicate groups, 28.7 MB reclaimable — 1.6% of the folder.** Almost all of it is *structural*
  rather than waste: a file that ships inside `delivered-packs/<pack>/` and also lives in its
  category folder is a pack being archived **as shipped**, which is the stated purpose of that
  directory. Deleting either copy would break that property to save a few megabytes. Left alone
  deliberately.
- Two genuine accidents, both trivial and both in `03-concepts`/`999-design-lab` (exploration
  material that never ships): `anim-rogue-v1.mp4` and `anim-rogue-v2.mp4` are byte-identical
  (2.36 MB), and `999-design-lab/inventory-screen/panel-texture-white-marble.png` duplicates the
  copy in `02-source-art/textures/`. Reported rather than removed — 5 MB is not worth touching the
  user's art tree unasked.
- Every literal MPQ path in `tools/*.ps1` still resolves. The cutters can all still find their
  sources — the "a cutter that is never run never fails" failure mode the README warns about is not
  present today.

**Conclusion: the art folder is in good order and needs no optimisation.**

---

## 6. One flake worth knowing about

`Writehero.HeroSurvivesAWriteAndReadsBack` failed once in a full parallel run and passed both in
isolation and on the next full run. The suite writes real save files inside a OneDrive-synced tree,
so a sync worker touching a temp save mid-test is the obvious suspect. Not chased; noted because a
one-off red in this suite is more likely to be this than a regression.

---

## What to check

1. Run `tools\RenameProjectFolder.ps1` with this session closed, then reconfigure as it prints.
2. Open the wiki's **Version history**, **Pipeline** and **Art assets** pages — all three were
   effectively blank and now carry 312, 39 and 104 rows.
3. Start a game, open the Crafting book, leave to the main menu, start another character: the book
   should be closed.
