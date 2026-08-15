# MakeYellowFontTrn.ps1
#
# Oracool: generates the two font translation tables behind UiFlags::ColorOracoolYellow and
# ColorOracoolYellowDark - the focus glow's colour.
#
# User request (2026-08-14): the selector should be a bright yellow GLOW, not a box. A glow works by
# redrawing the text in a colour, and a text colour here is a .trn: a 256-byte table remapping the
# font's ink indices onto a ramp inside the current palette. None of the shipped tables reaches a
# real yellow in ui_art\diablo.pal - measured, `yellow.trn` lands on 144-151, which in THIS palette
# is pink (232,202,202 at the top), because that table was authored for the level palette.
#
# The palette does carry a yellow ramp, at 128-135, the same shape as gold (176-191) and silver
# (224-239):
#
#     128 (255,253,159) lum 243   <- brighter than gold's brightest (255,227,164, lum 228)
#     129 (255,252, 87) lum 234
#     130 (254,251, 36) lum 227
#     131 (240,236,  0) lum 210
#     132 (195,195,  0) lum 173
#     133 (134,134,  0) lum 119
#     134 ( 87, 85,  0) lum  76
#     135 ( 25, 25,  0) lum  22
#
# The font's ink occupies source indices 192-207 - sixteen shades, established by diffing
# goldui.trn against grayui.trn and seeing exactly which entries disagree. Eight ramp steps under
# sixteen ink shades means each ramp entry is used twice, which is what whitegold.trn already does
# at its bright end (193 193 194 194 ...).
#
# Two tables, because the glow needs a bright ring and a dim companion, exactly as goldui.trn pairs
# with golduis.trn:
#   oracool_yellow.trn   - the full ramp, 128 at the top: the inner ring and the lit text.
#   oracool_yellows.trn  - the same ramp entered two steps down and bottomed out early, so the outer
#                          rings top out below the inner one and fade into the background.
#
# Everything outside 192-207 is identity, so the table cannot disturb anything that is not font ink.

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
$InkFirst = 192
$InkLast  = 207

# Ramp positions per ink shade, brightest first. Doubled: sixteen ink shades over eight ramp steps.
$brightRamp = @(128,128,129,129,130,130,131,131,132,132,133,133,134,134,135,135)
# Two steps dimmer and clamped at the ramp's floor - the compressed companion.
$darkRamp   = @(130,130,131,131,132,132,133,133,134,134,135,135,135,135,135,135)

function Write-Trn([string]$name, [int[]]$ramp) {
    if ($ramp.Count -ne ($InkLast - $InkFirst + 1)) { throw "$name : ramp must cover ink $InkFirst..$InkLast" }
    $bytes = New-Object byte[] 256
    for ($i = 0; $i -lt 256; $i++) { $bytes[$i] = [byte]$i }   # identity everywhere else
    for ($i = 0; $i -lt $ramp.Count; $i++) { $bytes[$InkFirst + $i] = [byte]$ramp[$i] }

    # oracool_assets ONLY. This used to write to Packaging\resources\assets as well, and that second
    # copy was pure dead weight: the stock assets tree only reaches the game through the explicit
    # devilutionx_assets list in CMake\Assets.cmake, these were never in it, so that copy never
    # deployed anywhere. The live pair is the one packed into oracool.mpq by
    # tools\build_oracool_mpq.cmd, which is what ColorTranslations actually resolves against.
    $path = Join-Path $repoRoot "Packaging\resources\oracool_assets\fonts\$name"
    $parent = Split-Path -Parent $path
    if (-not (Test-Path $parent)) { New-Item -ItemType Directory -Force -Path $parent | Out-Null }
    [System.IO.File]::WriteAllBytes($path, $bytes)
    Write-Host ("wrote {0} ({1} bytes)" -f $path, $bytes.Length)
}

Write-Trn 'oracool_yellow.trn'  $brightRamp
Write-Trn 'oracool_yellows.trn' $darkRamp

# Show what the two actually resolve to, so the ramp is verified against the palette rather than assumed.
$pal = [System.IO.File]::ReadAllBytes((Join-Path $repoRoot 'Packaging\resources\assets\ui_art\diablo.pal'))
foreach ($pair in @(@('oracool_yellow.trn', $brightRamp), @('oracool_yellows.trn', $darkRamp))) {
    $line = ""
    foreach ($idx in ($pair[1] | Select-Object -Unique)) {
        $line += ("{0}=({1},{2},{3}) " -f $idx, $pal[$idx*3], $pal[$idx*3+1], $pal[$idx*3+2])
    }
    Write-Host ("{0,-22} {1}" -f $pair[0], $line)
}
