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
# Use-before-assignment is a silent empty string in PowerShell, and that is exactly how this script
# lost its per-run isolation: the oracool verification list was named "oracool_verify_$runId.txt"
# twenty-one lines BEFORE $runId was assigned, so every concurrent packaging job wrote and deleted
# one shared %TEMP%\oracool_verify_.txt - reintroducing the very race the run id was added to end
# (external audit BR-01, 2026-08-30). Strict mode turns that class of mistake into an immediate
# error instead of a shared temp path.
Set-StrictMode -Version Latest

# ONE identity for the whole run, established before anything is named after it. UNIQUE per run
# rather than per version (external audit of v1.9.92, finding 9): a fixed
# `oracool-package-$version` path is recursively deleted before staging, so two packaging jobs for
# the same version - a CI run and a shell, or two shells - would delete each other's staging tree
# mid-build. The run id also makes an interrupted run's leftovers identifiable rather than shared.
$runId = [guid]::NewGuid().ToString('N').Substring(0, 12)

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

Write-Host "Packaging Diablo Orcl - Oracool Edition v$version" -ForegroundColor Cyan
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
    'zlib1.dll'
    # discord_game_sdk.dll left this list on 2026-09-07: DISCORD_INTEGRATION is OFF in every tree,
    # neither binary imports it (checked by string), and the v1.10.001 Release tree had none - it was a
    # leftover of an older configuration that the first manifest froze in. The exe's own imports are
    # the seven above.
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

# The binary must BE the version we are stamping on the box - ASKED, not guessed at.
#
# This used to scan the executable's ASCII strings and accept it if the expected number appeared
# anywhere (external audit of v1.9.92, finding 10). That proves only that those bytes occur
# somewhere: a changelog line, a resource path or dead data would satisfy it just as well as the
# real stamp, so a stale binary could pass. `--version` now prints ORACOOL_VERSION on its own line,
# so the check is equality against what the binary says it is.
# Read from the exe's VERSIONINFO resource, which CMake stamps from ORACOOL_VERSION. Not from
# `--version`: printInConsole goes through WriteConsole, which writes nothing when stdout is a pipe,
# so a script cannot capture it.
$exePath = Join-Path $BuildDir 'DiabloOrcl.exe'
$exeVersion = (Get-Item $exePath).VersionInfo.ProductVersion
if ([string]::IsNullOrWhiteSpace($exeVersion)) {
    Fail ("$exePath carries no version resource. It predates the VERSIONINFO stamp - rebuild it, and`n" +
          "    if it is still empty, check that Packaging\windows\oracool_version.rc.in is in the target.")
}
if ($exeVersion.Trim() -ne $version) {
    Fail "DiabloOrcl.exe reports v$($exeVersion.Trim()) but ORACOOL_VERSION is $version - it is a stale build. Rebuild the Release target."
}
Write-Host "  DiabloOrcl.exe : resource says v$exeVersion"

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

# --- Archive CONTENTS, not archive timestamps ----------------------------------------------------
#
# Everything above this line compares modification times, and a modification time proves when a file
# was written and nothing about what is in it (external audit of v1.9.97, finding 5). A truncated,
# corrupt or simply wrong archive passes every check so far by being recent.
#
# So both archives are opened and every manifest entry is read back out and compared byte for byte
# with its source, using the packer's own --verify mode - the same code that validates a freshly
# packed archive, so there is one definition of "this archive is correct" rather than two.
#
# Timestamps stay above as an incremental-build convenience. They are not evidence.
$packer = Join-Path $BuildDir 'oracool_mpq_pack.exe'
if (-not (Test-Path $packer)) {
    Fail ("the archive verifier is missing: $packer`n" +
          "    cmake --build $BuildDir --target oracool_mpq_pack")
}

function Verify-Archive([string]$label, [string]$archivePath, [string]$sourceDir, [string]$listFile) {
    if (-not (Test-Path $listFile)) {
        Fail ("no manifest to verify $label against: $listFile`n" +
              "    Configure and build once so CMake generates it.")
    }
    & $packer '--verify' $sourceDir $archivePath "@$listFile" | Out-Null
    if ($LASTEXITCODE -ne 0) {
        Fail "$label does not match its sources - repack it. The verifier's output is above."
    }
    Write-Host "  $label : contents verified"
}

# The engine archive is packed from the BUILD TREE's assets using CMake's generated manifest.
Verify-Archive $engineAssetsMpq $engineMpqPath $engineSrc (Join-Path $BuildDir 'devilutionx_mpq_files.txt')

# oracool.mpq is packed from the source asset folder, and its file list is the folder walk
# build_oracool_mpq.cmd does - rebuilt here so the verifier is checking every file that should be in
# it rather than every file that happens to be.
$oracoolList = Join-Path ([System.IO.Path]::GetTempPath()) ("oracool_verify_$runId.txt")
$assetRoot = (Resolve-Path $assetSrc).Path
# WriteAllLines with a BOM-less encoder, not `Set-Content -Encoding utf8`: Windows PowerShell 5.1
# writes a BOM, which becomes part of the first entry name in the list the verifier reads.
$listBytes = New-Object System.Text.UTF8Encoding $false
[System.IO.File]::WriteAllLines($oracoolList, [string[]]@(
    Get-ChildItem $assetRoot -Recurse -File | ForEach-Object { $_.FullName.Substring($assetRoot.Length + 1) }
), $listBytes)
try {
    Verify-Archive 'oracool.mpq' $mpqPath $assetRoot $oracoolList
} finally {
    Remove-Item $oracoolList -Force -ErrorAction SilentlyContinue
}

# --- Stage -------------------------------------------------------------------------------------

$name = "DiabloOrcl-v$version-win64"
$stage = Join-Path ([System.IO.Path]::GetTempPath()) "oracool-package-$version-$runId"
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
# Built under a unique name and moved into place, so the requested output is either the previous
# zip or a complete new one and never a half-written file wearing the right name. Same reasoning as
# the packer's own temp-then-replace, and the same finding.
# Windows PowerShell's Compress-Archive refuses any extension but .zip (found the first time this ran
# under 5.1, 2026-09-07), so the temporary name keeps the extension and carries the run id before it.
$zipTemp = Join-Path $OutDir "$name.$runId.partial.zip"
if (Test-Path $zipTemp) { Remove-Item $zipTemp -Force }
Compress-Archive -Path $target -DestinationPath $zipTemp -Force

# The finished zip is OPENED and its entry list compared with what was staged, before it is allowed
# to become the published one. Compress-Archive reporting success is not the same claim (external
# audit of v1.9.97, finding 5's reasoning, applied here).
$zipEntries = $null
try {
    Add-Type -AssemblyName System.IO.Compression.FileSystem -ErrorAction Stop
    $archive = [System.IO.Compression.ZipFile]::OpenRead($zipTemp)
    try { $zipEntries = @($archive.Entries | Where-Object { $_.Name -ne '' } | ForEach-Object { $_.FullName }) }
    finally { $archive.Dispose() }
} catch {
    Remove-Item $zipTemp -Force -ErrorAction SilentlyContinue
    Fail "the zip that was just written cannot be opened: $($_.Exception.Message)"
}
# The NAMES, not just how many of them (external audit BR-02, 2026-08-30). The comment above has
# claimed since v1.9.97 that the entry list is compared with staging; only the count ever was. A zip
# missing one file and carrying one unexpected file has the right count and passed - so did a
# duplicated entry paired with a missing one, and so did a wrong top-level prefix.
#
# Compress-Archive on a directory prefixes every entry with that directory's name, so the expected
# FullName is "$name/<relative path>". Separators are normalised because the zip uses '/' and
# Get-ChildItem reports '\'.
$stagedEntries = @($staged | ForEach-Object {
    ($name + '/' + $_.FullName.Substring($target.Length + 1)) -replace '\\', '/'
})
$zipNormalised = @($zipEntries | ForEach-Object { $_ -replace '\\', '/' })

$missing = @($stagedEntries | Where-Object { $zipNormalised -notcontains $_ })
$unexpected = @($zipNormalised | Where-Object { $stagedEntries -notcontains $_ })
$duplicates = @($zipNormalised | Group-Object | Where-Object { $_.Count -gt 1 } | ForEach-Object { $_.Name })

if ($missing.Count -gt 0 -or $unexpected.Count -gt 0 -or $duplicates.Count -gt 0) {
    Remove-Item $zipTemp -Force -ErrorAction SilentlyContinue
    $detail = ''
    if ($missing.Count -gt 0) { $detail += "`n  missing ($($missing.Count)): " + (($missing | Select-Object -First 10) -join ', ') }
    if ($unexpected.Count -gt 0) { $detail += "`n  unexpected ($($unexpected.Count)): " + (($unexpected | Select-Object -First 10) -join ', ') }
    if ($duplicates.Count -gt 0) { $detail += "`n  duplicated ($($duplicates.Count)): " + (($duplicates | Select-Object -First 10) -join ', ') }
    Fail "the zip's entries do not match what was staged:$detail"
}
if ($zipEntries.Count -ne $staged.Count) {
    Remove-Item $zipTemp -Force -ErrorAction SilentlyContinue
    Fail "the zip holds $($zipEntries.Count) files but $($staged.Count) were staged"
}
Write-Host "  zip entries match staging by name ($($stagedEntries.Count) files)"

# ATOMIC REPLACE, not delete-then-move. The old shape deleted the published zip and then moved the
# partial onto its name, and the comment above called that "either the previous zip or a complete
# new one" - which is false for the interval between the two, where there is NO zip at all
# (external audit of v1.9.97, finding 6). A crash, a permission error, an antivirus lock or a
# OneDrive race in that window loses the last known-good package.
#
# [System.IO.File]::Replace is a genuine same-volume replace primitive and keeps the old file as a
# backup until it succeeds. It requires the destination to exist, so a first-ever publish still
# takes the plain move - which is safe, because there is nothing there to lose.
if (Test-Path $zip) {
    $zipBackup = Join-Path $OutDir "$name.$runId.previous"
    try {
        [System.IO.File]::Replace((Resolve-Path $zipTemp).Path, (Resolve-Path $zip).Path, $zipBackup)
    } catch {
        Remove-Item $zipTemp -Force -ErrorAction SilentlyContinue
        Fail "could not replace the published zip - the previous one is untouched: $($_.Exception.Message)"
    }
    Remove-Item $zipBackup -Force -ErrorAction SilentlyContinue
} else {
    Move-Item $zipTemp $zip
}

$mb = [math]::Round((Get-Item $zip).Length / 1MB, 1)
Remove-Item $stage -Recurse -Force

Write-Host ''
Write-Host "OK  $zip  ($mb MB)" -ForegroundColor Green
Write-Host ''
Write-Host 'Upload with:' -ForegroundColor Cyan
Write-Host "    gh release create v$version --title `"Oracool Edition v$version`" --notes-file <notes.md> `"$zip`""
Write-Host "  or, to replace the asset on an existing release:"
Write-Host "    gh release upload v$version `"$zip`" --clobber"
