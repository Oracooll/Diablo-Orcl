# Renames the project folder from "Diablo Orcl V1" to "Diablo Orcl" (user, 2026-08-31: the project
# is now known as Diablo Orcl) and leaves the build tree ready to reconfigure.
#
# WHY THIS IS A SCRIPT AND NOT SOMETHING THAT ALREADY HAPPENED. A directory that is the current
# directory of a running process cannot be renamed on Windows, and every agent session, terminal,
# editor and OneDrive sync worker that has the folder open is such a process. The rename has to come
# from OUTSIDE the folder, with nothing holding it - which means running this from a shell whose
# working directory is somewhere else, after closing whatever else is pointed at it.
#
#   powershell -ExecutionPolicy Bypass -File "<path to this file>"
#
# Run it from the copy in the folder you are renaming; it resolves its own location and never needs
# the old name typed anywhere.
#
# AFTER THE RENAME the CMake build tree is stale: CMakeCache.txt records the absolute source and
# binary directories, so the first build under the new name refuses with "does not match the
# generated" rather than doing anything dangerous. This script clears the cache (not the whole tree -
# the 700 MB of game archives beside it are expensive to re-stage) so the next configure is clean,
# and prints the configure line that reproduces the current settings.

$ErrorActionPreference = 'Stop'

$repo = Split-Path -Parent $PSScriptRoot
$parent = Split-Path -Parent $repo
$target = Join-Path $parent 'Diablo Orcl'

if ((Split-Path -Leaf $repo) -eq 'Diablo Orcl') {
    Write-Host "Already named 'Diablo Orcl' - nothing to do." -ForegroundColor Green
    return
}
if (Test-Path $target) {
    throw "A folder named 'Diablo Orcl' already exists beside this one. Move or remove it first."
}

# Leave the folder before touching it: this script's own process must not be the thing that blocks
# the rename it is performing.
Set-Location $parent

Write-Host "renaming:" -ForegroundColor Cyan
Write-Host "  $repo"
Write-Host "  -> $target"
try {
    Rename-Item -LiteralPath $repo -NewName 'Diablo Orcl' -ErrorAction Stop
} catch {
    Write-Host ""
    Write-Host "RENAME FAILED: $($_.Exception.Message)" -ForegroundColor Red
    Write-Host "Something still has the folder open. Close editors, agent sessions and terminals"
    Write-Host "pointed at it, pause OneDrive sync, then run this again."
    Write-Host "To find the holder:  Get-Process | Where-Object { `$_.Path -like '*Diablo Orcl V1*' }"
    return
}
Write-Host "renamed." -ForegroundColor Green

# The stale CMake cache. Removed rather than rewritten: a cache edited by hand to point somewhere
# else is a cache nobody can trust afterwards.
$cache = Join-Path $target 'build\x64-Debug\CMakeCache.txt'
$cmakeFiles = Join-Path $target 'build\x64-Debug\CMakeFiles'
if (Test-Path $cache) { Remove-Item $cache -Force; Write-Host "cleared stale CMakeCache.txt" }
if (Test-Path $cmakeFiles) { Remove-Item $cmakeFiles -Recurse -Force; Write-Host "cleared stale CMakeFiles" }

Write-Host ""
Write-Host "Reconfigure with the settings the old cache carried:" -ForegroundColor Cyan
Write-Host @"
  cmake -S "$target" -B "$target\build\x64-Debug" -G Ninja ``
    -DCMAKE_BUILD_TYPE=Debug ``
    -DCMAKE_TOOLCHAIN_FILE="C:/Program Files/Microsoft Visual Studio/18/Community/VC/vcpkg/scripts/buildsystems/vcpkg.cmake" ``
    -DVCPKG_TARGET_TRIPLET=x64-windows ``
    -DDISCORD_INTEGRATION=OFF
"@
Write-Host "(or just open the folder in Visual Studio - CMakeSettings.json uses `${workspaceRoot} and"
Write-Host " configures itself.)"
