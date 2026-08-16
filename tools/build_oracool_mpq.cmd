@echo off
REM Builds oracool.mpq - Oracool Edition's own asset archive - from
REM Packaging\resources\oracool_assets\ and drops it next to the game binary.
REM
REM The archive is searched BEFORE every other MPQ (see FindMpqFile in Source/engine/assets.cpp),
REM so anything in here overrides the original game data without diabdat.mpq ever being touched.
REM It is optional: if the file is missing the game runs exactly as before.
REM
REM Usage:  tools\build_oracool_mpq.cmd
REM Run from the repository root.

setlocal enabledelayedexpansion
set SRC=Packaging\resources\oracool_assets
set BUILD=build\x64-Debug
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
set LIST=%TEMP%\oracool_mpq_files.txt
if exist "%LIST%" del "%LIST%"
pushd "%SRC%"
for /r %%F in (*) do (
  set "P=%%F"
  set "P=!P:%CD%\=!"
  echo(!P!>>"%LIST%"
)
popd

echo Packing...
"%PACKER%" "%SRC%" "%OUT%" "@%LIST%" || exit /b 1
del "%LIST%"

echo.
echo oracool.mpq written to %OUT%
endlocal
