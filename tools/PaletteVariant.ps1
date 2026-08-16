<#
.SYNOPSIS
  Oracool Megaplan Phase 0.7 - the recolor-zone palette tool.

  Reads a Diablo 768-byte .pal (256 x RGB), applies a colour transform, and writes a variant .pal
  plus a swatch-grid preview PNG. Together with the zone registry's paletteOverride field this is
  the whole recolor-zone mechanism: Hellfire's Crypt-is-a-recolored-Cathedral trick, as a tool.

  The transform deliberately SKIPS the injected green ramp's donor slots (PAL8_GREEN 152-159) so a
  variant palette never fights LoadPalette's in-game green injection, and skips index 0 (unused /
  transparency by convention).

.EXAMPLE
  # A frost variant of the Cathedral's first palette: pull every colour 45% toward steel blue.
  .\PaletteVariant.ps1 -In l1_1.pal -Out l1_frost.pal -TintR 110 -TintG 140 -TintB 190 -Strength 45

.EXAMPLE
  # A gloom variant: darken 30%, no hue change.
  .\PaletteVariant.ps1 -In l2_1.pal -Out l2_gloom.pal -Brightness 70
#>
param(
    [Parameter(Mandatory = $true)][string]$In,
    [Parameter(Mandatory = $true)][string]$Out,
    [int]$TintR = -1,
    [int]$TintG = -1,
    [int]$TintB = -1,
    [ValidateRange(0, 100)][int]$Strength = 50,
    [ValidateRange(10, 200)][int]$Brightness = 100,
    [string]$PreviewPng = ""
)

$bytes = [System.IO.File]::ReadAllBytes($In)
if ($bytes.Length -ne 768) { throw "Expected a 768-byte .pal, got $($bytes.Length) bytes." }

$hasTint = ($TintR -ge 0 -and $TintG -ge 0 -and $TintB -ge 0)
$greenRampFirst = 152  # PAL8_GREEN - see engine/palette.h
$greenRampLast = 159

for ($i = 0; $i -lt 256; $i++) {
    if ($i -eq 0) { continue }                                        # transparency convention
    if ($i -ge $greenRampFirst -and $i -le $greenRampLast) { continue } # the injected green ramp's home
    $r = [int]$bytes[$i * 3]; $g = [int]$bytes[$i * 3 + 1]; $b = [int]$bytes[$i * 3 + 2]
    if ($hasTint) {
        # Luminance-preserving pull toward the tint colour: the tint is scaled by the entry's own
        # brightness first, so dark stones stay dark and highlights stay bright - this is what keeps
        # a variant reading as "the same dungeon, different mood" instead of a flat colour wash.
        $lum = (2 * $r + 4 * $g + 3 * $b) / 9.0 / 255.0
        $tr = $TintR * $lum; $tg = $TintG * $lum; $tb = $TintB * $lum
        $r = $r + ($tr - $r) * $Strength / 100.0
        $g = $g + ($tg - $g) * $Strength / 100.0
        $b = $b + ($tb - $b) * $Strength / 100.0
    }
    $r = $r * $Brightness / 100.0; $g = $g * $Brightness / 100.0; $b = $b * $Brightness / 100.0
    $bytes[$i * 3] = [byte][Math]::Min(255, [Math]::Max(0, [Math]::Round($r)))
    $bytes[$i * 3 + 1] = [byte][Math]::Min(255, [Math]::Max(0, [Math]::Round($g)))
    $bytes[$i * 3 + 2] = [byte][Math]::Min(255, [Math]::Max(0, [Math]::Round($b)))
}

[System.IO.File]::WriteAllBytes($Out, $bytes)
Write-Host "Wrote $Out"

if ($PreviewPng -eq "") { $PreviewPng = [System.IO.Path]::ChangeExtension($Out, ".preview.png") }
Add-Type -AssemblyName System.Drawing
$cell = 20
$size = 16 * $cell
$bmp = New-Object System.Drawing.Bitmap -ArgumentList $size, $size
$gfx = [System.Drawing.Graphics]::FromImage($bmp)
for ($i = 0; $i -lt 256; $i++) {
    $color = [System.Drawing.Color]::FromArgb(255, [int]$bytes[$i * 3], [int]$bytes[$i * 3 + 1], [int]$bytes[$i * 3 + 2])
    $brush = New-Object System.Drawing.SolidBrush -ArgumentList $color
    $x = ($i % 16) * $cell
    $y = [int][Math]::Floor($i / 16) * $cell
    $gfx.FillRectangle($brush, $x, $y, $cell, $cell)
    $brush.Dispose()
}
$gfx.Dispose()
$bmp.Save($PreviewPng, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
Write-Host "Wrote $PreviewPng"
