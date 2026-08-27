@echo off
REM Builds devilutionx.mpq - DevilutionX's OWN fonts, interface art and level data - from
REM Packaging\resources\assets\ and drops it next to the game binary.
REM
REM Usage:  tools\build_devilutionx_mpq.cmd [build-directory]
REM Run from the repository root. Defaults to build\x64-Debug.
REM
REM
REM WHY THIS EXISTS RATHER THAN smpq
REM
REM CMake builds devilutionx.mpq only when it can find `smpq`, an external tool (CMakeLists.txt:190).
REM smpq is packaged for Linux and the BSDs; there is no Windows build, nothing in winget or vcpkg
REM offers it, and this repository's own tools\build_and_install_smpq.sh is a Linux/macOS script that
REM ends in `sudo make install`. Getting it here means porting a Launchpad tarball to MSVC, which is a
REM port rather than an install.
REM
REM So the same decision is taken here that tools\oracool_mpq_pack.cpp already took and documented for
REM oracool.mpq: THE GAME ALREADY WRITES MPQ ARCHIVES. Save files are MPQs, so MpqWriter is right
REM there in the engine, and an archive built with it can be rebuilt on any machine that can build the
REM game - which is exactly what is not true of the smpq path.
REM
REM The practical consequence, and the reason this was worth doing (2026-08-27): without
REM devilutionx.mpq the release has to ship those 258 files LOOSE in an assets\ folder, and a package
REM that forgets the folder is a package that cannot start. v1.9.88 shipped exactly that way.

setlocal enabledelayedexpansion
set BUILD=%~1
if "%BUILD%"=="" set BUILD=build\x64-Debug
REM The BUILD TREE's assets folder, not Packaging\resources\assets.
REM
REM Those are different sets and the difference is not small: the source folder holds 258 files and
REM CMake deploys 188 of them (`devilutionx_assets` in CMake\Assets.cmake is an explicit list, and 70
REM of the source files are this fork's own art that lives in oracool.mpq instead). Packing the source
REM tree produced a 17 MB archive of 20 MB of files, most of which the game never asks this archive
REM for; packing what CMake actually deployed produces the same 6.8 MB the loose folder ships.
REM
REM The build tree is the authority on what ships. That is the whole reason it exists.
set SRC=%BUILD%\assets
set PACKER=%BUILD%\oracool_mpq_pack.exe
set OUT=%BUILD%\devilutionx.mpq

if not exist "%SRC%" (
  echo ERROR: deployed asset folder not found: %SRC%
  echo Build the game first - CMake copies the assets into the build tree.
  exit /b 1
)

REM The packer is EXCLUDE_FROM_ALL, so a normal build does not produce it. Same note as
REM build_oracool_mpq.cmd: build the target once and both scripts reuse it.
if not exist "%PACKER%" (
  echo ERROR: packer not found at %PACKER%
  echo Build it once from a developer shell:
  echo     cmake --build %BUILD% --target oracool_mpq_pack
  exit /b 1
)

REM THE MANIFEST CMAKE GENERATED, not a walk of the build tree (external audit of v1.9.92, finding 6).
REM
REM This used to enumerate every file under %SRC% recursively. That directory is only ever ADDED to -
REM CMake copies each current `devilutionx_assets` entry into it and never removes one that has left
REM the list - so an asset deleted from the manifest stayed on disk and was packed into the next
REM archive. A clean tree and an incremental tree could produce different archives from one commit.
REM
REM Note that an ordinary `cmake --build` now packs this archive itself, from the same list. This
REM script remains for a manual repack and must not disagree with it.
set LIST=%BUILD%\devilutionx_mpq_files.txt
if not exist "%LIST%" (
  echo ERROR: manifest not found: %LIST%
  echo Configure/build once so CMake generates it:
  echo     cmake --build %BUILD% --target oracool_devilutionx_mpq
  exit /b 1
)

echo Packing...
REM The manifest is CMake's, not a temporary this script owns - it is not deleted afterwards.
"%PACKER%" "%SRC%" "%OUT%" "@%LIST%" || exit /b 1

echo.
echo devilutionx.mpq written to %OUT%
