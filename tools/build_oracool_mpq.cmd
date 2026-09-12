@echo off
REM Builds oracool.mpq - Oracool Edition's own asset archive - from
REM Packaging\resources\oracool_assets\ and drops it next to the game binary.
REM
REM The archive is searched BEFORE every other MPQ (see FindMpqFile in Source/engine/assets.cpp),
REM so anything in here overrides the original game data without diabdat.mpq ever being touched.
REM It is optional: if the file is missing the game runs exactly as before.
REM
REM Usage:  tools\build_oracool_mpq.cmd [build-directory]
REM Run from the repository root.
REM
REM The build directory defaults to build\x64-Debug and can be overridden by the first argument:
REM     tools\build_oracool_mpq.cmd build\x64-Release
REM
REM It used to be hardcoded to the Debug tree (audit, 2026-08-26), which is the wrong default in the
REM one situation that matters most: CMake's package step reads oracool.mpq from the build directory
REM being packaged, so cutting a Release either failed the install guard or - before that guard
REM existed - shipped whatever happened to be sitting in the Debug folder, however old.

setlocal enabledelayedexpansion
set SRC=Packaging\resources\oracool_assets
set BUILD=%~1
REM The Debug tree lives OUTSIDE OneDrive since 2026-09-06 (user: "move only the x64-Debug folder
REM out of OneDrive"); an in-tree Debug folder, if someone still builds one, is preferred.
if "%BUILD%"=="" if exist "build\x64-Debug\CMakeCache.txt" set BUILD=build\x64-Debug
if "%BUILD%"=="" set BUILD=C:\Diablo Orcl\x64-Debug
set PACKER=%BUILD%\oracool_mpq_pack.exe
set OUT=%BUILD%\oracool.mpq

if not exist "%SRC%" (
  echo ERROR: asset source folder not found: %SRC%
  exit /b 1
)

REM The packer is a CMake target that is EXCLUDE_FROM_ALL, so it is not built by a normal build.
REM cmake is only on PATH inside a developer shell, so this does not try to invoke it - build the
REM target once and this script reuses it thereafter.
if not exist "%PACKER%" (
  echo ERROR: packer not found at %PACKER%
  echo Build it once from a developer shell:
  echo     cmake --build %BUILD% --config Debug --target oracool_mpq_pack
  exit /b 1
)

REM Collect every file under the source folder as a path relative to it, into a RESPONSE FILE.
REM
REM Not onto the command line: the skill-sound library took the asset count past 300 and the
REM accumulated string hit Windows' ~8191-character limit, which cmd.exe reports as "The input line
REM is too long" without naming a cause. A list file has no such ceiling.
REM Scoped to this process, not a fixed name (external audit of v1.9.92, finding 9): two invocations
REM on one machine shared %TEMP%\oracool_mpq_files.txt, so one could truncate the list the other was
REM feeding the packer.
REM EXCLUSIVELY OWNED, not merely improbable. It used to be %RANDOM%_%TIME:~9,2% - 32,768 values and
REM a clock in hundredths - so two invocations could share one list and one could truncate the list
REM the other was feeding the packer (external audit of v1.9.97, finding 4).
REM
REM cmd.exe has no exclusive file create and no $$, but `mkdir` IS atomic and fails when the
REM directory already exists, so a directory claimed that way is genuinely this run's. The retry
REM loop is what turns "probably free" into "provably mine": whoever's mkdir succeeds owns it.
set LISTDIR=
for /l %%A in (1,1,20) do (
  if not defined LISTDIR (
    set "TRYDIR=%TEMP%\oracool_mpq_%RANDOM%%RANDOM%"
    mkdir "!TRYDIR!" 2>nul && set "LISTDIR=!TRYDIR!"
  )
)
if not defined LISTDIR (
  echo ERROR: could not claim a temporary directory under %TEMP%
  exit /b 1
)
set LIST=%LISTDIR%\files.txt
pushd "%SRC%"
for /r %%F in (*) do (
  set "P=%%F"
  set "P=!P:%CD%\=!"
  echo(!P!>>"%LIST%"
)
popd

echo Packing...
REM The exit code is CAPTURED and the temporary is cleaned up on both paths. `|| exit /b 1` used to
REM leave here directly on failure, so the response file and its directory survived every failed run
REM (external audit of v1.9.97, finding 4) - and a failure reported 1 rather than what the packer
REM actually returned.
"%PACKER%" "%SRC%" "%OUT%" "@%LIST%"
set PACKRC=%ERRORLEVEL%
rmdir /s /q "%LISTDIR%" 2>nul
if not "%PACKRC%"=="0" (
  echo ERROR: the packer failed with exit code %PACKRC%
  exit /b %PACKRC%
)

echo.
echo oracool.mpq written to %OUT%

REM The PRIVATE archive is GONE (user, 2026-09-12: "Delete Private Assets folder, there are no
REM private assets. All goes online as we agreed it is a non-profit mod"). Its twenty assets - the
REM nine cutscene paintings, the three object sprites and the eight UI panels - moved into
REM Packaging\resources\oracool_assets and now ship in oracool.mpq like everything else, so there is
REM one archive again and nothing to keep out of the repository.
REM
REM The engine still MOUNTS oracool_private.mpq if one is present (see init.cpp); it is simply never
REM built now. Delete any stale copy beside the binary - it is mounted AHEAD of oracool.mpq and would
REM shadow these files with their old versions.
endlocal
