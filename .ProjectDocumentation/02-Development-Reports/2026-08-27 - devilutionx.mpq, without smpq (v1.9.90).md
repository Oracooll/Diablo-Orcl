# devilutionx.mpq, without smpq (v1.9.90)

Date: 2026-08-27
Version: 1.9.90
Tests: 564/566 serially (the two standing baseline failures)

## smpq cannot be installed here, and that is not a shortcut

Asked to install it. It is not installable on this machine in any ordinary sense:

- **No package.** `winget search smpq` and `winget search stormlib` both return nothing. It is not
  in vcpkg's manifest for this project either. smpq is a Launchpad project packaged for Debian and
  the BSDs.
- **The repo's own installer is not for Windows.** `tools/build_and_install_smpq.sh` says
  "Compatible with Linux, *BSD, and macOS", uses `getconf _NPROCESSORS_ONLN`, patches the source with
  `sed -i`, and ends in `sudo cmake --build ... --target install`.

Getting it here means porting a Launchpad tarball and StormLib to MSVC. That is a port, not an
install, and it would add a third-party build dependency to a project that had already decided
against exactly that.

**It had decided against it in writing.** `tools/oracool_mpq_pack.cpp`, which builds `oracool.mpq`
every day, opens with:

> Deliberately built on the engine's own MpqWriter rather than the external `smpq` tool that
> devilutionx.mpq uses. The game already writes MPQs (save files are MPQ archives), so the code is
> right there, and depending on it means the archive can be rebuilt on any machine that can build the
> game - not true of the smpq path, which is absent here, and is why devilutionx's own assets
> currently load loose from a directory instead of an archive.

So the answer was already written down. I extended that decision to the one archive it had not yet
covered.

## `tools\build_devilutionx_mpq.cmd`

The sibling of `build_oracool_mpq.cmd`, over DevilutionX's own assets. Same packer, same response-file
handling, same "build the EXCLUDE_FROM_ALL target once" note.

**It packs the BUILD TREE's `assets\`, not `Packaging\resources\assets\`** — and getting that wrong
first is what makes it worth stating. Those are different sets:

| | files | size |
|---|---|---|
| `Packaging\resources\assets` (source) | 258 | 20 MB |
| `build\...\assets` (what CMake deploys) | 188 | 6.8 MB |

`devilutionx_assets` in `CMake/Assets.cmake` is an explicit list, and 70 of the source files are this
fork's own art that belongs in `oracool.mpq` instead. Packing the source tree produced a **17 MB**
archive full of things this archive is never asked for. Packing what CMake deployed produces **4.1
MB**, compressed from 6.6.

The build tree is the authority on what ships. That is the whole reason it exists.

## Verified without launching the game

I cannot start the game to check the archive loads, so I checked what can be checked: every one of
the 188 files was extracted back out with `tools/oracool_mpq_extract.exe` — which reads through
`MpqArchive`, the same class `LoadMPQ` uses — and byte-compared against its source.

**188 extracted, 188 identical, 0 mismatches.**

That establishes the archive is structurally sound and readable by the engine's own reader. It does
not establish that the game starts from it, which needs one launch.

## The packager takes either form

`FindAsset` searches the MPQ archives first and the loose `assets` directory after them, so the game
accepts either. The release script now does the same: it prefers `devilutionx.mpq` and falls back to
`assets\`, failing only when neither is there.

A packager stricter than the thing it packages for would reject working builds. The fallback is also
what makes this safe to ship before the archive has been confirmed in play: delete the mpq from the
build tree and the next run reverts to the folder that shipped in v1.9.88's corrected zip.

Package size is unchanged in practice — 41.8 MB against 41.2 MB — because a compressed archive
inside a zip cannot be compressed twice. What changes is that it is **12 files instead of 199**, and
a single archive cannot be half-copied.

## To confirm

One launch from a copy of the built package. If the main menu draws with its fonts, the archive is
being read. If it does not, delete `devilutionx.mpq` from the build tree and repackage — the loose
folder is still the proven path.
