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
    Where to write the zip. Defaults to the repository root's `dist` folder.

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
    [string]$OutDir = 'dist'
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

# DevilutionX's own fonts, interface art and level data - the loose form of devilutionx.mpq. The
# game does not start without it, which is exactly what shipping v1.9.88 without it demonstrated.
$requiredDirs = @('assets')

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
foreach ($d in $requiredDirs) {
    $path = Join-Path $BuildDir $d
    if (-not (Test-Path $path)) { Fail "required folder missing from the build tree: $d" }
    $count = @(Get-ChildItem $path -Recurse -File).Count
    # A sanity floor, not an exact count - the set grows. Zero or a handful means something copied
    # an empty tree, which is the failure that looks like success.
    if ($count -lt 50) { Fail "'$d' holds only $count files - that is not a complete asset tree." }
    Write-Host "  $d : $count files"
}

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
foreach ($d in $requiredDirs) { Copy-Item (Join-Path $BuildDir $d) (Join-Path $target $d) -Recurse }

# The README, with the version stamped in rather than typed in.
# Beside this script rather than under Packaging\, because .gitignore's `[Rr]elease/` rule - meant
# for build configurations - swallows any path with a `release` folder in it, and a template that is
# silently not committed is the same class of failure this whole script exists to prevent.
$templatePath = 'tools\release_README.template.txt'
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

foreach ($f in @($requiredFiles + 'README.txt')) {
    if (-not (Test-Path (Join-Path $target $f))) { Fail "staged package is missing $f" }
}
Write-Host "  all $($requiredFiles.Count + 1) top-level files present"
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
