<#
.SYNOPSIS
    Assembles the Windows x64 release zip, and refuses to produce a broken one.

.DESCRIPTION
    Written after v1.9.88 shipped without its `assets\` folder and without a README (2026-08-27).
    The package had been put together by hand from a remembered file list, and the only check run
    against it was the negative one - "no Blizzard archive is in here". That passed, and passing it
    was mistaken for the package being right. A game that cannot start shipped anyway.

    So this script's job is not really "make a zip". It is to hold the list in one place and to fail
    LOUDLY rather than quietly produce something that does not run. Every check below exists because
    its absence has already cost something:

      - the file manifest is fixed here, so nothing can be forgotten by being remembered wrong
      - `assets\` is required and counted, because that is what was missing
      - the exe's stamped version must equal ORACOOL_VERSION, which catches a stale binary
      - oracool.mpq must be newer than every asset that feeds it, which catches the OTHER thing that
        went wrong this session: the Release archive was a day stale and would have shipped without
        the new skill icons
      - the staged folder is swept for all seven Blizzard archives, which is the rule that must never
        be broken and is therefore the one check that was already being done

.PARAMETER BuildDir
    The build tree to package. Defaults to build\x64-Release.

.PARAMETER OutDir
    Where to write the zip. Defaults to the repository root, which is where .gitignore's existing
    `DiabloOrcl-*-win64.zip` rule already expects to find it.

    NOT `dist\`, and that is a trap worth naming: CMakeLists.txt line 16 treats the mere EXISTENCE of
    a `dist` folder at the repository root as "this is a source distribution", sets SRC_DIST, and
    add_subdirectory(dist)'s it. Writing packages there broke the CMake configure step outright - the
    SRC_DIST branch reaches an install() for a devilutionx.mpq that is not built on this machine.

.EXAMPLE
    powershell -ExecutionPolicy Bypass -File tools\BuildReleasePackage.ps1

.NOTES
    Run from the repository root. Build the Release target and repack the Release oracool.mpq first:

        cmake --build build/x64-Release --target devilutionx --parallel
        tools\build_oracool_mpq.cmd build\x64-Release
#>
[CmdletBinding()]
param(
    [string]$BuildDir = 'build\x64-Release',
    [string]$OutDir = '.'
)

$ErrorActionPreference = 'Stop'

function Fail($message) {
    Write-Host ''
    Write-Host "PACKAGING FAILED: $message" -ForegroundColor Red
    Write-Host ''
    exit 1
}

# --- Version -----------------------------------------------------------------------------------

if (-not (Test-Path 'ORACOOL_VERSION')) { Fail 'ORACOOL_VERSION not found - run this from the repository root.' }
$version = (Get-Content 'ORACOOL_VERSION' -Raw).Trim()
if ($version -notmatch '^\d+\.\d+\.\d+$') { Fail "ORACOOL_VERSION is not a version number: '$version'" }

Write-Host "Packaging Diablo Orcl V1 - Oracool Edition v$version" -ForegroundColor Cyan
Write-Host "  build tree: $BuildDir"

if (-not (Test-Path $BuildDir)) { Fail "build directory not found: $BuildDir" }

# --- The manifest ------------------------------------------------------------------------------
#
# One list, in one place. Everything a fresh installation needs and nothing it does not.

$requiredFiles = @(
    'DiabloOrcl.exe',
    'oracool.mpq',
    'SDL2.dll',
    'SDL2_image.dll',
    'bz2.dll',
    'fmt.dll',
    'libpng16.dll',
    'libsodium.dll',
    'zlib1.dll',
    'discord_game_sdk.dll'
)

# DevilutionX's own fonts, interface art and level data. The game does not start without it, which
# is exactly what shipping v1.9.88 without it demonstrated.
#
# It must arrive as devilutionx.mpq. The game would also accept it loose in an `assets` folder -
# FindAsset searches the MPQ archives first and that directory after them (Source/engine/assets.cpp) -
# and this script briefly accepted either for that reason.
#
# It does not any more. User, 2026-08-27: "i dont want any loose folders in my release." That is a
# packaging decision rather than an engine one, and it is a reasonable one: a release is 12 files now,
# every one of which is either the binary, a library, a README or an archive. A fallback that quietly
# shipped 188 loose files instead would be the same silent substitution this whole script exists to
# prevent, so an absent archive is an ERROR that names the command to build it.
$engineAssetsMpq = 'devilutionx.mpq'
$engineAssetsDir = 'assets'

# Never redistributable. Blizzard's commercial data, not ours to hand out.
$forbidden = @(
    'diabdat.mpq', 'hellfire.mpq', 'hfmonk.mpq',
    'hfmusic.mpq', 'hfvoice.mpq', 'hfbard.mpq', 'hfbarb.mpq'
)

# --- Pre-flight checks -------------------------------------------------------------------------

Write-Host ''
Write-Host 'Checking the build tree...'

foreach ($f in $requiredFiles) {
    if (-not (Test-Path (Join-Path $BuildDir $f))) { Fail "required file missing from the build tree: $f" }
}
$engineMpqPath = Join-Path $BuildDir $engineAssetsMpq
if (-not (Test-Path $engineMpqPath)) {
    Fail ("$engineAssetsMpq is missing - the game will not start without the engine's own assets.`n" +
          "    tools\build_devilutionx_mpq.cmd $BuildDir")
}
# The archive must be newer than the deployed assets it was built from, for the same reason
# oracool.mpq must: neither is rebuilt by an ordinary build, so an asset added after the last pack
# ships as though it had never been added.
$engineSrc = Join-Path $BuildDir $engineAssetsDir
if (Test-Path $engineSrc) {
    $newestAsset = Get-ChildItem $engineSrc -Recurse -File | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
    if ($newestAsset -and $newestAsset.LastWriteTimeUtc -gt (Get-Item $engineMpqPath).LastWriteTimeUtc) {
        Fail ("$engineAssetsMpq is older than '$($newestAsset.Name)'. Rebuild it:`n" +
              "    tools\build_devilutionx_mpq.cmd $BuildDir")
    }
}
Write-Host "  $engineAssetsMpq : current"

# The binary must BE the version we are stamping on the box.
$exePath = Join-Path $BuildDir 'DiabloOrcl.exe'
$stamped = (Select-String -Path $exePath -Pattern '\d+\.\d+\.\d+' -Encoding Ascii -AllMatches).Matches.Value |
    Sort-Object -Unique
if ($stamped -notcontains $version) {
    Fail "DiabloOrcl.exe does not carry version $version - it is a stale build. Rebuild the Release target."
}
Write-Host "  DiabloOrcl.exe : v$version"

# oracool.mpq must be newer than everything that feeds it. A normal build does NOT repack the
# archive, so a source asset edited after the last pack ships as if it had never been added.
$mpqPath = Join-Path $BuildDir 'oracool.mpq'
$mpqTime = (Get-Item $mpqPath).LastWriteTimeUtc
$assetSrc = 'Packaging\resources\oracool_assets'
if (Test-Path $assetSrc) {
    $newest = Get-ChildItem $assetSrc -Recurse -File | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
    if ($newest -and $newest.LastWriteTimeUtc -gt $mpqTime) {
        Fail ("oracool.mpq is older than '$($newest.Name)'. Repack it:`n" +
              "    tools\build_oracool_mpq.cmd $BuildDir")
    }
}
Write-Host "  oracool.mpq    : current"

# --- Stage -------------------------------------------------------------------------------------

$name = "DiabloOrcl-v$version-win64"
$stage = Join-Path ([System.IO.Path]::GetTempPath()) "oracool-package-$version"
if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
$target = Join-Path $stage $name
New-Item -ItemType Directory -Path $target -Force | Out-Null

Write-Host ''
Write-Host 'Staging...'
foreach ($f in $requiredFiles) { Copy-Item (Join-Path $BuildDir $f) $target }
Copy-Item $engineMpqPath $target

# The README, with the version stamped in rather than typed in.
#
# This is the SAME file CMakeLists.txt's install() step ships (line 583), not a second copy of it. I
# added a duplicate under tools\ first, which was worse than no template at all: two READMEs, one
# updated and one not, with no way to tell from either which the release had used. Its version line
# was stale by fifty-seven versions when this was written, which is exactly what a hand-typed
# version number does.
$templatePath = 'Packaging\windows\RELEASE_README.txt'
if (-not (Test-Path $templatePath)) { Fail "README template not found: $templatePath" }
(Get-Content $templatePath -Raw).Replace('{{VERSION}}', $version) |
    Set-Content (Join-Path $target 'README.txt') -Encoding ascii -NoNewline
Write-Host '  README.txt stamped'

# --- Post-stage checks -------------------------------------------------------------------------
#
# Run against what will ACTUALLY be zipped, not against the intent. The two can differ - a recursive
# copy brings whatever is in the source folder.

Write-Host ''
Write-Host 'Checking the staged package...'

$staged = Get-ChildItem $target -Recurse -File
foreach ($m in $forbidden) {
    $hit = $staged | Where-Object { $_.Name -ieq $m }
    if ($hit) { Fail "commercial game data staged for release: $($hit.FullName)" }
}
Write-Host "  no commercial game data ($($forbidden.Count) archives checked)"

$mustBeStaged = @($requiredFiles + 'README.txt' + $engineAssetsMpq)
foreach ($f in $mustBeStaged) {
    # Checked HERE as well as before staging, because "the copy ran" and "the files arrived" are
    # different claims.
    if (-not (Test-Path (Join-Path $target $f))) { Fail "staged package is missing $f" }
}
Write-Host "  all $($mustBeStaged.Count) files present"

# NO LOOSE FOLDERS (user, 2026-08-27). Everything ships as a file at the top level, so any directory
# in the staged tree is something that was not meant to be there - a stray copy, or an asset folder
# that came along with one.
$strayDirs = @(Get-ChildItem $target -Directory)
if ($strayDirs.Count -gt 0) {
    Fail "the package contains loose folders, which it must not: $($strayDirs.Name -join ', ')"
}
Write-Host "  no loose folders"
Write-Host "  $($staged.Count) files total"

# --- Zip ---------------------------------------------------------------------------------------

if (-not (Test-Path $OutDir)) { New-Item -ItemType Directory -Path $OutDir -Force | Out-Null }
$zip = Join-Path $OutDir "$name.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }
Compress-Archive -Path $target -DestinationPath $zip -Force

$mb = [math]::Round((Get-Item $zip).Length / 1MB, 1)
Remove-Item $stage -Recurse -Force

Write-Host ''
Write-Host "OK  $zip  ($mb MB)" -ForegroundColor Green
Write-Host ''
Write-Host 'Upload with:' -ForegroundColor Cyan
Write-Host "    gh release create v$version --title `"Oracool Edition v$version`" --notes-file <notes.md> `"$zip`""
Write-Host "  or, to replace the asset on an existing release:"
Write-Host "    gh release upload v$version `"$zip`" --clobber"
