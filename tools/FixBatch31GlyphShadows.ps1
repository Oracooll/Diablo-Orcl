# Oracool asset pipeline: turns RfA-13's batch-31 glyphs into house-style glyphs for BuildGlyphStrips.
#
# Source: batch-31-new-skill-glyphs\artist-verified-export-2026-09-13\glyphs - the artist's checked snapshot,
# never modified here. Output: batch-31-new-skill-glyphs\glyphs, which BuildGlyphStrips reads. The output is
# rebuilt from the source on every run, so it depends only on the export and re-running changes nothing.
# (The first delivery was fixed in place while the artist was still exporting into the same folder; the two
# processes rewrote each other's files. Reading one folder and writing another is the cure.)
#
# Two corrections, both the brief's fault, not the artist's:
# 1. Shadow. Every glyph already in the game (oracool-skill-glyphs-vanilla-v1, batch-13, batch-26) draws its
#    shadow two pixels LEFT of the white and one pixel UP: (-2,-1). RfA-13 said "down and right", and the
#    export follows it at (+2,+2). The shadow is rebuilt from the white shape, the part drawn by hand:
#    shadow = white moved by (-2,-1), wherever that lands on clear.
# 2. Safe box. Every glyph in the game keeps white and shadow inside 8..47 on both axes (AuditGlyphStrips'
#    GLYPH-OUT-OF-BOUNDS). A glyph wider or taller than 40 loses its outermost shadow-only column (left) or
#    row (top); then it moves the least distance that puts it inside, so the artist's placement is kept
#    wherever it already fits.
# Two colours and binary alpha are kept, so BuildGlyphStrips' pixel check still holds.
#
# Usage: powershell -ExecutionPolicy Bypass -File tools\FixBatch31GlyphShadows.ps1
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$root = Split-Path -Parent $PSScriptRoot
$pack = Join-Path (Split-Path -Parent $root) 'Resources\01-in-use-assets\delivered-packs\batch-31-new-skill-glyphs'
$source = Join-Path $pack 'artist-verified-export-2026-09-13\glyphs'
$dest = Join-Path $pack 'glyphs'
$ICON = 56
$DX = -2
$DY = -1
$LO = 8
$HI = 47
$white = [System.Drawing.Color]::FromArgb(255, 243, 243, 243)
$shadow = [System.Drawing.Color]::FromArgb(255, 12, 7, 7)

$written = 0; $moved = 0; $trimmed = 0; $unfit = @()
foreach ($file in Get-ChildItem $source -Recurse -Filter *.png) {
  $rel = $file.FullName.Substring($source.Length + 1)
  $isWhite = New-Object 'bool[,]' $ICON, $ICON
  $bmp = [System.Drawing.Bitmap]::FromFile($file.FullName)
  try {
    if ($bmp.Width -ne $ICON -or $bmp.Height -ne $ICON) { throw "$rel is $($bmp.Width)x$($bmp.Height), not ${ICON}x${ICON}" }
    for ($y = 0; $y -lt $ICON; $y++) {
      for ($x = 0; $x -lt $ICON; $x++) {
        $c = $bmp.GetPixel($x, $y)
        if ($c.A -eq 0) { continue }
        if ($c.A -ne 255) { throw "$rel has partial alpha at $x,$y" }
        if ($c.R -eq 243 -and $c.G -eq 243 -and $c.B -eq 243) { $isWhite[$x, $y] = $true }
        elseif (-not ($c.R -eq 12 -and $c.G -eq 7 -and $c.B -eq 7)) { throw "$rel has a third colour at $x,$y" }
      }
    }
  } finally { $bmp.Dispose() }

  # 0 clear, 1 white, 2 shadow. A shadow pixel is a clear one whose white sits at (x - DX, y - DY).
  $px = New-Object 'int[,]' $ICON, $ICON
  $minX = $ICON; $minY = $ICON; $maxX = -1; $maxY = -1
  for ($y = 0; $y -lt $ICON; $y++) {
    for ($x = 0; $x -lt $ICON; $x++) {
      $wx = $x - $DX; $wy = $y - $DY
      if ($isWhite[$x, $y]) { $px[$x, $y] = 1 }
      elseif ($wx -ge 0 -and $wx -lt $ICON -and $wy -ge 0 -and $wy -lt $ICON -and $isWhite[$wx, $wy]) { $px[$x, $y] = 2 }
      else { continue }
      if ($x -lt $minX) { $minX = $x }; if ($x -gt $maxX) { $maxX = $x }
      if ($y -lt $minY) { $minY = $y }; if ($y -gt $maxY) { $maxY = $y }
    }
  }
  if ($maxX -lt 0) { throw "$rel is empty" }

  $size = $HI - $LO + 1
  $cut = $false
  while (($maxX - $minX + 1) -gt $size) {
    $onlyShadow = $true
    for ($y = 0; $y -lt $ICON; $y++) { if ($px[$minX, $y] -eq 1) { $onlyShadow = $false; break } }
    if (-not $onlyShadow) { break }
    for ($y = 0; $y -lt $ICON; $y++) { $px[$minX, $y] = 0 }
    $minX++; $cut = $true
  }
  while (($maxY - $minY + 1) -gt $size) {
    $onlyShadow = $true
    for ($x = 0; $x -lt $ICON; $x++) { if ($px[$x, $minY] -eq 1) { $onlyShadow = $false; break } }
    if (-not $onlyShadow) { break }
    for ($x = 0; $x -lt $ICON; $x++) { $px[$x, $minY] = 0 }
    $minY++; $cut = $true
  }

  $sx = 0; $sy = 0
  if (($maxX - $minX + 1) -gt $size -or ($maxY - $minY + 1) -gt $size) {
    $unfit += "$rel ($($maxX - $minX + 1)x$($maxY - $minY + 1), left in place)"
  } else {
    if ($minX -lt $LO) { $sx = $LO - $minX } elseif ($maxX -gt $HI) { $sx = $HI - $maxX }
    if ($minY -lt $LO) { $sy = $LO - $minY } elseif ($maxY -gt $HI) { $sy = $HI - $maxY }
  }
  if ($sx -ne 0 -or $sy -ne 0) { $moved++ }
  if ($cut) { $trimmed++ }

  $outPath = Join-Path $dest $rel
  New-Item -ItemType Directory -Force (Split-Path -Parent $outPath) | Out-Null
  $out = New-Object System.Drawing.Bitmap -ArgumentList $ICON, $ICON, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  try {
    for ($y = $minY; $y -le $maxY; $y++) {
      for ($x = $minX; $x -le $maxX; $x++) {
        if ($px[$x, $y] -eq 1) { $out.SetPixel($x + $sx, $y + $sy, $white) }
        elseif ($px[$x, $y] -eq 2) { $out.SetPixel($x + $sx, $y + $sy, $shadow) }
      }
    }
    $out.Save($outPath, [System.Drawing.Imaging.ImageFormat]::Png)
  } finally { $out.Dispose() }
  $written++
}

Write-Host "glyphs written from the verified export, shadow at ($DX,$DY): $written"
Write-Host "moved inside the ${LO}..${HI} box: $moved; lost an outer shadow row or column: $trimmed"
if ($unfit.Count -gt 0) { Write-Host "white alone is wider or taller than the box:"; $unfit | ForEach-Object { Write-Host "  $_" } }
