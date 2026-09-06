# Oracool asset pipeline: cuts the bottom HUD - ui\middle_hud.png, ui\health_orb.png, ui\mana_orb.png
# and the two ui\*_orb_liquid.png - from GPT's sixth HUD master, and writes the geometry header the
# layout is built from.
#
#     powershell -ExecutionPolicy Bypass -File tools\CutHudPlate.ps1
#
# ## The sixth HUD - sunken wells (2026-09-05, user: "there is a new hud in oracool.mpq. use it")
#
# Resources\02-source-art\delivered-packs\oracool-hud-v6-sunken-wells\hud-v6.png: 1839x324, TRUE
# alpha, built by GPT to the v6 brief (.ProjectDocumentation\01-Project-Overview\Asset Brief - HUD v6)
# at exactly 3x of the 613x108 it is shown at. Its layout-manifest.json carries every rect, and the
# numbers below are that manifest divided by three - not re-measured, because the pack's own README
# says why not: the wells' inward shadows partially occupy the openings, so alpha thresholding would
# find the wrong edges.
#
# WHAT IS NEW IN IT. The orb cradles are shorter (108 on screen, from 121). The two skill wells are
# OPENINGS: 56x56 of true transparency, with the frame casting a shadow INWARD as an alpha gradient
# - black, 255 at the rim falling to 0 nine master pixels in (three on screen). The game draws the
# vanilla 56px spell plate and the icon under the plate, so the shadow falls on them; the net area
# the icon owns is the 50x50 that shadow leaves. The spheres are empty glass; the liquid is its own
# file (hud-v6-liquid.png: two opaque discs, radius 102 at the sphere centres).
#
# THE SHADOW AND THE PALETTE. The engine can blend at one level - 50%, through the palette's own
# table - so a three-pixel gradient cannot ship as such. hud_art.cpp keys alpha >= 128 as opaque and
# blends [40, 128) at half; this script shapes the resampled shadow to that: inside each well
# opening, alpha >= 160 becomes 255 (the rim's own black), [30, 160) becomes 100 (the half layer),
# and below 30 nothing. On screen that is one black pixel and two half-dark ones - three of shadow,
# which is what was asked. Outside the wells the alpha is binarised at 128 as every other asset is.

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$root = Split-Path -Parent $PSScriptRoot
$pack = Join-Path (Split-Path -Parent $root) 'Resources\02-source-art\delivered-packs\oracool-hud-v6-matched-belt'   # the LARGE-BELT compact revision (2026-09-05, "apply these" / "apply this when done"): 580 wide, six 34px belt holes at a 35px pitch behind a 4px outline that repeats the wells' carved border; `-large-belt` is the same geometry with the plain outline
$source = Join-Path $pack 'hud-v6.png'
$liquidSource = Join-Path $pack 'hud-v6-liquid.png'
$outDir = Join-Path $root 'Packaging\resources\oracool_assets\ui'
$header = Join-Path $root 'Source\oracool\hud_plate_skin.h'
foreach ($f in @($source, $liquidSource)) { if (-not (Test-Path $f)) { throw "missing: $f" } }

# ---- the manifest, in master pixels ----------------------------------------------------------
$masterW = 1740; $masterH = 324                      # the COMPACT layout in the pack's manifest (580x108 native)
$scaleDiv = 3                                       # master / 3 = screen, exactly
$plateCrop = @(348, 120, 1059, 204)                # x y w h - belt-shadow's band plus the 120 master px (40 native) the belt grew
$wells = @( @(366, 138, 168, 168), @(1221, 138, 168, 168) )
$beltX = @(564, 669, 774, 879, 984, 1089); $beltY = 210; $beltW = 102; $beltH = 102   # 34x34 holes at a 35px pitch, each with a 2px inward alpha shadow
$beltBarTop = 198                                  # the belt strip's top edge
$orbs = @( @(195, 156, 102), @(1548, 159, 102) )   # cx cy r

function Scl($v) { return [int][math]::Round($v / $scaleDiv) }

$W = Scl $masterW; $Hh = Scl $masterH
$xl = Scl $plateCrop[0]; $xr = Scl ($plateCrop[0] + $plateCrop[2])

# ---- resample --------------------------------------------------------------------------------
function Resample([string]$path) {
  $src = New-Object System.Drawing.Bitmap $path
  if ($src.Width -ne $masterW -or $src.Height -ne $masterH) { throw "$path is $($src.Width)x$($src.Height), expected ${masterW}x${masterH}" }
  $bmp = New-Object System.Drawing.Bitmap $W, $Hh, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.InterpolationMode = 'HighQualityBicubic'; $g.PixelOffsetMode = 'HighQuality'; $g.CompositingMode = 'SourceCopy'
  $g.DrawImage($src, (New-Object System.Drawing.Rectangle -ArgumentList 0, 0, $W, $Hh), (New-Object System.Drawing.Rectangle -ArgumentList 0, 0, $masterW, $masterH), 'Pixel')
  $g.Dispose(); $src.Dispose()
  return $bmp
}

$s = Resample $source
$ls = Resample $liquidSource

# the openings on screen - the two wells and the six belt holes - for the shadow shaping
$wellsScreen = $wells | ForEach-Object { ,@((Scl $_[0]), (Scl $_[1]), (Scl $_[2]), (Scl $_[3])) }
$openings = @($wellsScreen) + @($beltX | ForEach-Object { ,@((Scl $_), (Scl $beltY), (Scl $beltW), (Scl $beltH)) })
function InWell($x, $y) { foreach ($w in $openings) { if ($x -ge $w[0] -and $x -lt $w[0] + $w[2] -and $y -ge $w[1] -and $y -lt $w[1] + $w[3]) { return $true } }; return $false }

for ($y = 0; $y -lt $Hh; $y++) { for ($x = 0; $x -lt $W; $x++) {
  $c = $s.GetPixel($x, $y)
  if (InWell $x $y) {
    if ($c.A -ge 160) { $s.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(255, 0, 0, 0)) }
    elseif ($c.A -ge 30) { $s.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(100, 0, 0, 0)) }
    else { $s.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(0, 0, 0, 0)) }
  } elseif ($c.A -lt 128) { $s.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(0, 0, 0, 0)) }
  elseif ($c.A -lt 255) { $s.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(255, $c.R, $c.G, $c.B)) }
  $l = $ls.GetPixel($x, $y)
  if ($l.A -lt 128) { $ls.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(0, 0, 0, 0)) }
  elseif ($l.A -lt 255) { $ls.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(255, $l.R, $l.G, $l.B)) }
} }
Write-Host ("composite {0}x{1}  plate x {2}..{3}" -f $W, $Hh, $xl, $xr)

# ---- the five pieces --------------------------------------------------------------------------
function Piece([System.Drawing.Bitmap]$from, $x0, $x1, $name) {
  $p = $from.Clone((New-Object System.Drawing.Rectangle -ArgumentList $x0, 0, ($x1 - $x0), $Hh), $from.PixelFormat)
  $p.Save((Join-Path $outDir $name), [System.Drawing.Imaging.ImageFormat]::Png); $p.Dispose()
  Write-Host ("{0,-22} x {1}..{2}  {3}x{4}" -f $name, $x0, $x1, ($x1 - $x0), $Hh)
}
Piece $s 0 $xl 'health_orb.png'
Piece $s $xl $xr 'middle_hud.png'
Piece $s $xr $W 'mana_orb.png'
Piece $ls 0 $xl 'health_orb_liquid.png'
Piece $ls $xr $W 'mana_orb_liquid.png'
$s.Dispose(); $ls.Dispose()

# ---- the header -------------------------------------------------------------------------------
$plateW = $xr - $xl
$cells = ($beltX | ForEach-Object { (Scl $_) - $xl }) -join ', '
$lw = $wellsScreen[0]; $rw = $wellsScreen[1]
$hc = @((Scl $orbs[0][0]), (Scl $orbs[0][1])); $mc = @((Scl $orbs[1][0]), (Scl $orbs[1][1])); $rs = Scl $orbs[0][2]
$h = @"
/**
 * @file oracool/hud_plate_skin.h
 *
 * GENERATED by tools/CutHudPlate.ps1 - do not edit. Change the numbers at the top of that script
 * and re-run it; the five PNGs it writes and these come from the same pass.
 *
 * The sixth bottom HUD (Resources/02-source-art/delivered-packs/oracool-hud-v6-sunken-wells,
 * hud-v6.png at exactly one third). Every number is in SCREEN pixels; the plate's rects are
 * PLATE-local, the orbs' are local to their own piece. The three pieces share one bottom edge and
 * butt together left to right: health cradle, plate, mana cradle.
 *
 * The wells are OPENINGS with a three-pixel inward shadow; the well rects are the openings, and
 * SkillWellNetSize (50) is what the shadow leaves inside them.
 */
#pragma once

#include "engine/point.hpp"
#include "engine/rectangle.hpp"
#include "engine/size.hpp"

namespace devilution::oracool::hud_skin {

constexpr Size PlateSize { $plateW, $Hh };

/** The wells' openings - transparent, the spell plate drawn beneath - plate-local. */
constexpr Rectangle LmbWell { { $($lw[0] - $xl), $($lw[1]) }, { $($lw[2]), $($lw[3]) } };
constexpr Rectangle RmbWell { { $($rw[0] - $xl), $($rw[1]) }, { $($rw[2]), $($rw[3]) } };

/** The six belt cells: the painted raised floors, from the pack's manifest. */
constexpr int BeltCellX[6] = { $cells };
constexpr int BeltCellY = $(Scl $beltY);
constexpr Size BeltCellSize { $(Scl $beltW), $(Scl $beltH) };
/** Plate-local y of the belt bar's top edge - what the XP bar sits above. */
constexpr int BeltBarTop = $((Scl $beltBarTop) - (Scl $plateCrop[1]) + (Scl $plateCrop[1]));

constexpr Size HealthOrbSize { $xl, $Hh };
constexpr Size ManaOrbSize { $($W - $xr), $Hh };
constexpr Point HealthSphereCenter { $($hc[0]), $($hc[1]) };
constexpr Point ManaSphereCenter { $($mc[0] - $xr), $($mc[1]) };
constexpr int SphereRadius = $rs;

} // namespace devilution::oracool::hud_skin
"@
[System.IO.File]::WriteAllText($header, $h.Replace("`r`n", "`n"), (New-Object System.Text.UTF8Encoding $false))
Write-Host "wrote $header"
