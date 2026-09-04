# Oracool asset pipeline: cuts the bottom HUD - ui\middle_hud.png, ui\health_orb.png, ui\mana_orb.png
# and the two ui\*_orb_liquid.png - from ONE painted master, and writes the geometry header the layout
# is built from.
#
#     powershell -ExecutionPolicy Bypass -File tools\CutHudPlate.ps1
#
# ## The fifth HUD (2026-09-05: "use 03-transparent-slot-visual-draft.png as hud", then, the same
# ## night, "use 02-raised-stone-slot-design.png as hud")
#
# The GPT pack in Oracool.MPQ\02-source-art\delivered-packs\diablo-bottom-hud-v1. The master is the
# file the user chose: 02-raised-stone-slot-design.png, a 24-bit render with a CHECKERBOARD painted
# where transparency would be, and belt cells that are PAINTED raised stone rather than holes. So the
# transparency is recovered here: near-white and light-grey NEUTRAL pixels forming large connected
# regions are keyed out (the ground), while small bright neutral specks - the highlights on glass and
# gold - are kept. The belt cells cannot be found by alpha on this master; they are the hand-measured
# rects below, which the script falls back to when it finds no holes. (Design 03, the same layout with
# holes for cells, was the HUD for v1.9.216, and its holes measured the same.)
#
# The orbs are PAINTED in this master. The game draws its own liquid (v1.9.214), so each sphere is
# split: its interior goes to a liquid file, opaque and alone, and is made transparent in the cradle
# file, whose rim then sits on the liquid's edge. Above the fill line the glass is empty.
#
# One master, five files. The health cradle, the plate and the mana cradle are cut from a single
# resampled band at screen scale, at vertical lines, so the three rects the layout butts together
# reassemble to the painting exactly.
#
# THE SCALE. 0.32 would make the belt holes exactly a 28px potion sprite - and cradles whose spheres
# rise above the line the side panels' content stops at (ornate_border.h's SidePanelContentBottom,
# 624), which the stash's seventeen saved rows and both Abilities pages cannot yield. At 0.288 the
# spheres all but clear it and only the arches' tips cross - scrollrt.cpp clips those while a side
# panel is open - and the plate comes out ~352 wide, where the old 356 was. The holes are 25px; a
# potion's own art is narrower than its 28px cell.

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$root = Split-Path -Parent $PSScriptRoot
$pack = Join-Path (Split-Path -Parent $root) 'Oracool.MPQ\02-source-art\delivered-packs\diablo-bottom-hud-v1\designs'
$source = Join-Path $pack '02-raised-stone-slot-design.png'
$outDir = Join-Path $root 'Packaging\resources\oracool_assets\ui'
$header = Join-Path $root 'Source\oracool\hud_plate_skin.h'
$scratch = Join-Path $env:TEMP 'CutHudPlate'
if (-not (Test-Path $source)) { throw "missing master: $source" }
New-Item -ItemType Directory -Force $scratch | Out-Null

# ---- measured off the 1942x809 master (2026-09-05) --------------------------------------------
$expectBand = @(3, 214, 1934, 382)          # the silhouette's bounding box after keying, +-3 (the true-alpha sibling's is 4,215 1931x380: keying keeps 1-3px of anti-aliased edge)
$scale = 0.3168   # 0.288 x 1.10 (user, 2026-09-05: "scale the hud up 10%")
$cutLeft = 352; $cutRight = 1576            # MASTER-space x where the cradles hand over to the plate
# BAND-LOCAL master pixels, the dark stone inside each well's rim (checked on the overlay this
# script writes to %TEMP%\CutHudPlate\overlay.png):
$lmbWell = @(385, 182, 169, 166)
$rmbWell = @(1389, 185, 172, 166)
# The spheres, BAND-LOCAL master pixels: centre x, centre y, and one radius for both. Measured on
# the overlay (the painted spheres' edges at 3x), not detected: a hue detector was tried twice and
# took the sphere's reflection on the stone, then the arch's blue highlights, for the sphere.
$healthSphere = @(205, 165); $manaSphere = @(1732, 168); $sphereRadiusMaster = 123
# The six belt cells' openings, BAND-LOCAL master pixels, for a master whose cells are painted rather
# than cut through: measured as the holes of design 03, which shares the layout (x 594-682 ... y
# 461-556 in master space). Used only when the alpha finds no holes.
$beltCellsMaster = @( @(591, 247, 88, 96), @(724, 247, 88, 96), @(856, 247, 88, 96), @(989, 247, 88, 96), @(1121, 247, 88, 96), @(1255, 247, 88, 96) )
# The top edge of the painted belt bar (band-local master y): the silhouette rises from the cells' run
# to the wells at x 580..1360, and its top row there is 425 in master space. The XP bar sits just
# above this line.
$beltBarTopMaster = 211

# ---- pixel work in C#: keying, despeckling, sphere finding ------------------------------------
$cs = @"
using System;
using System.Drawing;
using System.Drawing.Imaging;
using System.Collections.Generic;
public static class HudKey {
  public static int[] Load(Bitmap b) {
    var d = b.LockBits(new Rectangle(0,0,b.Width,b.Height), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
    var px = new int[b.Width*b.Height]; System.Runtime.InteropServices.Marshal.Copy(d.Scan0, px, 0, px.Length); b.UnlockBits(d); return px;
  }
  public static Bitmap Store(int[] px, int w, int h) {
    var b = new Bitmap(w, h, PixelFormat.Format32bppArgb);
    var d = b.LockBits(new Rectangle(0,0,w,h), ImageLockMode.WriteOnly, PixelFormat.Format32bppArgb);
    System.Runtime.InteropServices.Marshal.Copy(px, 0, d.Scan0, px.Length); b.UnlockBits(d); return b;
  }
  // Near-white / light-grey neutral pixels in connected regions of at least minArea become alpha 0.
  public static int[] Key(int[] px, int w, int h, int minArea) {
    var cand = new bool[px.Length];
    for (int i = 0; i < px.Length; i++) {
      int r = (px[i] >> 16) & 255, g = (px[i] >> 8) & 255, b = px[i] & 255;
      int mx = Math.Max(r, Math.Max(g, b)), mn = Math.Min(r, Math.Min(g, b));
      cand[i] = (mx - mn) <= 6 && mn >= 222;
    }
    var label = new int[px.Length]; int next = 0; var stack = new Stack<int>();
    var outPx = new int[px.Length];
    for (int i = 0; i < px.Length; i++) outPx[i] = px[i] | unchecked((int)0xFF000000);
    for (int i = 0; i < px.Length; i++) {
      if (!cand[i] || label[i] != 0) continue;
      next++; var members = new List<int>(); stack.Push(i); label[i] = next;
      while (stack.Count > 0) {
        int p = stack.Pop(); members.Add(p); int x = p % w, y = p / w;
        if (x > 0 && cand[p-1] && label[p-1] == 0) { label[p-1] = next; stack.Push(p-1); }
        if (x < w-1 && cand[p+1] && label[p+1] == 0) { label[p+1] = next; stack.Push(p+1); }
        if (y > 0 && cand[p-w] && label[p-w] == 0) { label[p-w] = next; stack.Push(p-w); }
        if (y < h-1 && cand[p+w] && label[p+w] == 0) { label[p+w] = next; stack.Push(p+w); }
      }
      if (members.Count >= minArea) foreach (int m in members) outPx[m] = 0;
    }
    return outPx;
  }
  // Opaque pixels with fewer than three opaque 8-neighbours become transparent: the checker's
  // anti-aliased seams leave single stray pixels that would otherwise set the bounding box.
  public static int[] Despeckle(int[] px, int w, int h) {
    var o = (int[])px.Clone();
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
      int i = y*w + x; if (((px[i] >> 24) & 255) < 128) continue;
      int n = 0;
      for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
        if (dx == 0 && dy == 0) continue; int xx = x+dx, yy = y+dy;
        if (xx < 0 || yy < 0 || xx >= w || yy >= h) continue;
        if (((px[yy*w+xx] >> 24) & 255) >= 128) n++;
      }
      if (n < 3) o[i] = 0;
    }
    return o;
  }
  // Opaque connected components smaller than minArea become transparent: fragments of the checker's
  // anti-aliased seams that survived keying inside the holes. The glass highlights are safe - they
  // touch the sphere and so belong to a large component.
  public static int[] DropIslands(int[] px, int w, int h, int minArea) {
    var o = (int[])px.Clone(); var label = new int[px.Length]; int next = 0; var stack = new Stack<int>();
    for (int i = 0; i < px.Length; i++) {
      if (((px[i] >> 24) & 255) < 128 || label[i] != 0) continue;
      next++; var members = new List<int>(); stack.Push(i); label[i] = next;
      while (stack.Count > 0) {
        int p = stack.Pop(); members.Add(p); int x = p % w, y = p / w;
        if (x > 0 && ((px[p-1] >> 24) & 255) >= 128 && label[p-1] == 0) { label[p-1] = next; stack.Push(p-1); }
        if (x < w-1 && ((px[p+1] >> 24) & 255) >= 128 && label[p+1] == 0) { label[p+1] = next; stack.Push(p+1); }
        if (y > 0 && ((px[p-w] >> 24) & 255) >= 128 && label[p-w] == 0) { label[p-w] = next; stack.Push(p-w); }
        if (y < h-1 && ((px[p+w] >> 24) & 255) >= 128 && label[p+w] == 0) { label[p+w] = next; stack.Push(p+w); }
      }
      if (members.Count < minArea) foreach (int m in members) o[m] = 0;
    }
    return o;
  }
  // Bright neutral pixels that touch transparency are the checker's seams clinging to the frame's
  // edge (they survived keying by being connected to nothing large). Two passes eat them.
  public static int[] EatSeams(int[] px, int w, int h) {
    var o = (int[])px.Clone();
    for (int pass = 0; pass < 2; pass++) {
      var src = (int[])o.Clone();
      for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
        int i = y*w + x; int p = src[i]; if (((p >> 24) & 255) < 128) continue;
        int r = (p >> 16) & 255, g = (p >> 8) & 255, b = p & 255;
        int mx = Math.Max(r, Math.Max(g, b)), mn = Math.Min(r, Math.Min(g, b));
        if ((mx - mn) > 10 || mn < 200) continue;
        bool touches = false;
        for (int dy = -1; dy <= 1 && !touches; dy++) for (int dx = -1; dx <= 1; dx++) {
          int xx = x+dx, yy = y+dy; if (xx < 0 || yy < 0 || xx >= w || yy >= h) continue;
          if (((src[yy*w+xx] >> 24) & 255) < 128) { touches = true; break; } }
        if (touches) o[i] = 0;
      }
    }
    return o;
  }
  public static int[] Bbox(int[] px, int w, int h) {
    int minX = w, minY = h, maxX = -1, maxY = -1;
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) if (((px[y*w+x] >> 24) & 255) >= 128) {
      if (x < minX) minX = x; if (x > maxX) maxX = x; if (y < minY) minY = y; if (y > maxY) maxY = y; }
    return new int[] { minX, minY, maxX, maxY };
  }
  // Bounding box of strongly red (hue 0) or strongly blue (hue 1) opaque pixels inside a search box.
  public static int[] SphereBox(int[] px, int w, int h, int hue, int x0, int y0, int x1, int y1) {
    int minX = w, minY = h, maxX = -1, maxY = -1;
    for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) {
      int p = px[y*w+x]; if (((p >> 24) & 255) < 128) continue;
      int r = (p >> 16) & 255, g = (p >> 8) & 255, b = p & 255;
      bool hit = hue == 0 ? (r > 60 && r > 2*g && r > 2*b) : (b > 60 && b > 2*r && b*2 > 3*g);
      if (!hit) continue;
      if (x < minX) minX = x; if (x > maxX) maxX = x; if (y < minY) minY = y; if (y > maxY) maxY = y; }
    return new int[] { minX, minY, maxX, maxY };
  }
  // Splits px into (cradle with the two sphere interiors transparent, liquid = those interiors alone).
  public static int[][] SplitSpheres(int[] px, int w, int h, double hx, double hy, double mx, double my, double r) {
    var frame = (int[])px.Clone(); var liquid = new int[px.Length];
    double rIn = (r - 1) * (r - 1), rOut = (r + 2) * (r + 2);
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
      int i = y*w + x; if (((px[i] >> 24) & 255) < 128) continue;
      double dh = (x-hx)*(x-hx) + (y-hy)*(y-hy), dm = (x-mx)*(x-mx) + (y-my)*(y-my);
      double d = Math.Min(dh, dm);
      if (d <= rOut) liquid[i] = px[i] | unchecked((int)0xFF000000);
      if (d <= rIn) frame[i] = 0;
    }
    return new int[][] { frame, liquid };
  }
}
"@
Add-Type -TypeDefinition $cs -ReferencedAssemblies System.Drawing

function Resample([int[]]$px, [int]$w, [int]$h, [System.Drawing.Rectangle]$crop, [int]$ow, [int]$oh) {
  $srcBmp = [HudKey]::Store($px, $w, $h)
  $bmp = New-Object System.Drawing.Bitmap $ow, $oh, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $g = [System.Drawing.Graphics]::FromImage($bmp)
  $g.InterpolationMode = 'HighQualityBicubic'; $g.PixelOffsetMode = 'HighQuality'; $g.CompositingMode = 'SourceCopy'
  $g.DrawImage($srcBmp, (New-Object System.Drawing.Rectangle -ArgumentList 0, 0, $ow, $oh), $crop, 'Pixel')
  $g.Dispose(); $srcBmp.Dispose()
  # binary alpha: the game keys on >=128, and a feathered edge would carry premultiplied fringe colour
  for ($y=0;$y -lt $oh;$y++){ for($x=0;$x -lt $ow;$x++){ $c=$bmp.GetPixel($x,$y); if ($c.A -lt 128) { $bmp.SetPixel($x,$y,[System.Drawing.Color]::FromArgb(0,0,0,0)) } elseif ($c.A -lt 255) { $bmp.SetPixel($x,$y,[System.Drawing.Color]::FromArgb(255,$c.R,$c.G,$c.B)) } } }
  return $bmp
}
function Scl($v) { return [int][math]::Round($v * $scale) }

# ---- the master: key, despeckle, assert the band ----------------------------------------------
$img = New-Object System.Drawing.Bitmap $source
if ($img.Width -ne 1942 -or $img.Height -ne 809) { throw "master is $($img.Width)x$($img.Height), expected 1942x809" }
$mw = $img.Width; $mh = $img.Height
$px = [HudKey]::Load($img); $img.Dispose()
$px = [HudKey]::Key($px, $mw, $mh, 1500)
$px = [HudKey]::Despeckle($px, $mw, $mh)
$px = [HudKey]::DropIslands($px, $mw, $mh, 200)
$px = [HudKey]::EatSeams($px, $mw, $mh)
$bb = [HudKey]::Bbox($px, $mw, $mh)
$bandX = $bb[0]; $bandY = $bb[1]; $bandW = $bb[2] - $bb[0] + 1; $bandH = $bb[3] - $bb[1] + 1
Write-Host ("band {0}x{1} at ({2},{3})" -f $bandW, $bandH, $bandX, $bandY)
foreach ($i in 0..3) { $got = @($bandX, $bandY, $bandW, $bandH)[$i]; if ([math]::Abs($got - $expectBand[$i]) -gt 3) { throw "band moved: got $bandX,$bandY ${bandW}x${bandH}, expected $($expectBand -join ' ') (+-3) - re-measure the wells and cut lines before cutting" } }

# ---- the spheres ------------------------------------------------------------------------------
$hc = @( ($healthSphere[0] + $bandX), ($healthSphere[1] + $bandY) ); $mc = @( ($manaSphere[0] + $bandX), ($manaSphere[1] + $bandY) )
$sphereRadius = $sphereRadiusMaster
Write-Host ("spheres: health centre {0},{1}  mana {2},{3}  radius {4}  (master px)" -f $hc[0], $hc[1], $mc[0], $mc[1], $sphereRadius)
$split = [HudKey]::SplitSpheres($px, $mw, $mh, $hc[0], $hc[1], $mc[0], $mc[1], $sphereRadius)
$framePx = $split[0]; $liquidPx = $split[1]

# ---- resample both layers to screen scale -------------------------------------------------------
$W = Scl $bandW; $Hh = Scl $bandH
$crop = New-Object System.Drawing.Rectangle -ArgumentList $bandX, $bandY, $bandW, $bandH
$s = Resample $framePx $mw $mh $crop $W $Hh
$ls = Resample $liquidPx $mw $mh $crop $W $Hh
Write-Host ("composite {0}x{1}  scale {2}" -f $W, $Hh, $scale)

# ---- the belt holes, by alpha, in the plate's span --------------------------------------------
$xl = Scl $cutLeft; $xr = Scl $cutRight
$midY = Scl (508 - $bandY)
$runs=@(); $in=$false
for($x=0;$x -lt $W;$x++){ $o = $s.GetPixel($x,$midY).A -ge 128; if(-not $o -and -not $in){$start=$x;$in=$true}; if($o -and $in){$runs += @{ X=$start; W=$x-$start };$in=$false} }
$holes = @($runs | Where-Object { $_.W -gt 10 -and $_.W -lt 60 -and $_.X -ge $xl -and ($_.X + $_.W) -le $xr })
if ($holes.Count -eq 6) {
  $cx = [int]($holes[0].X + $holes[0].W / 2)
  $vr=@(); $in=$false
  for($y=0;$y -lt $Hh;$y++){ $o = $s.GetPixel($cx,$y).A -ge 128; if(-not $o -and -not $in){$start=$y;$in=$true}; if($o -and $in){$vr += @{ Y=$start; H=$y-$start };$in=$false} }
  $hole = @($vr | Where-Object { $_.H -gt 10 -and $_.H -lt 60 })[0]
  $cellW = ($holes | ForEach-Object { $_.W } | Measure-Object -Minimum).Minimum
  Write-Host ("belt holes by alpha, y {0} h {1}: {2}" -f $hole.Y, $hole.H, (($holes | ForEach-Object { "x$($_.X) w$($_.W)" }) -join ' '))
} elseif ($holes.Count -eq 0) {
  $holes = @($beltCellsMaster | ForEach-Object { @{ X = (Scl $_[0]); W = (Scl $_[2]) } })
  $hole = @{ Y = (Scl $beltCellsMaster[0][1]); H = (Scl $beltCellsMaster[0][3]) }
  $cellW = ($holes | ForEach-Object { $_.W } | Measure-Object -Minimum).Minimum
  Write-Host ("belt cells painted - hand rects, y {0} h {1}: {2}" -f $hole.Y, $hole.H, (($holes | ForEach-Object { "x$($_.X) w$($_.W)" }) -join ' '))
} else { throw "found $($holes.Count) belt holes, expected 6 or none" }

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

# ---- the overlay, for checking the hand-measured wells ----------------------------------------
$z = 3
$ov = New-Object System.Drawing.Bitmap ($W*$z), ($Hh*$z)
$g = [System.Drawing.Graphics]::FromImage($ov); $g.Clear([System.Drawing.Color]::FromArgb(255,40,120,40)); $g.InterpolationMode='NearestNeighbor'; $g.PixelOffsetMode='Half'
$g.DrawImage($ls, 0, 0, $W*$z, $Hh*$z); $g.DrawImage($s, 0, 0, $W*$z, $Hh*$z)
$lime = New-Object System.Drawing.Pen ([System.Drawing.Color]::Lime), 1; $cyan = New-Object System.Drawing.Pen ([System.Drawing.Color]::Cyan), 1; $yel = New-Object System.Drawing.Pen ([System.Drawing.Color]::Yellow), 1
foreach ($hole2 in $holes) { $g.DrawRectangle($cyan, $hole2.X*$z, $hole.Y*$z, $hole2.W*$z, $hole.H*$z) }
$g.DrawRectangle($lime, (Scl $lmbWell[0])*$z, (Scl $lmbWell[1])*$z, (Scl $lmbWell[2])*$z, (Scl $lmbWell[3])*$z)
$g.DrawRectangle($lime, (Scl $rmbWell[0])*$z, (Scl $rmbWell[1])*$z, (Scl $rmbWell[2])*$z, (Scl $rmbWell[3])*$z)
$hcs = @((Scl ($hc[0] - $bandX)), (Scl ($hc[1] - $bandY))); $mcs = @((Scl ($mc[0] - $bandX)), (Scl ($mc[1] - $bandY))); $rs = Scl $sphereRadius
$g.DrawEllipse($yel, ($hcs[0]-$rs)*$z, ($hcs[1]-$rs)*$z, 2*$rs*$z, 2*$rs*$z); $g.DrawEllipse($yel, ($mcs[0]-$rs)*$z, ($mcs[1]-$rs)*$z, 2*$rs*$z, 2*$rs*$z)
$g.DrawLine($yel, $xl*$z, 0, $xl*$z, $Hh*$z); $g.DrawLine($yel, $xr*$z, 0, $xr*$z, $Hh*$z)
$g.Dispose(); $ov.Save((Join-Path $scratch 'overlay.png')); $ov.Dispose()
$s.Dispose(); $ls.Dispose()

# ---- the header -------------------------------------------------------------------------------
$plateW = $xr - $xl
$cells = ($holes | ForEach-Object { $_.X - $xl }) -join ', '
$h = @"
/**
 * @file oracool/hud_plate_skin.h
 *
 * GENERATED by tools/CutHudPlate.ps1 - do not edit. Change the measurements at the top of that
 * script and re-run it; the five PNGs it writes and these numbers come from the same pass.
 *
 * The fifth bottom HUD (Oracool.MPQ/02-source-art/delivered-packs/diablo-bottom-hud-v1, cut from
 * 02-raised-stone-slot-design.png with its checkerboard keyed out, at scale $scale). Every
 * number is in SCREEN pixels; the plate's rects are PLATE-local, the orbs' are local to their own
 * piece. The three pieces share one bottom edge and butt together left to right: health cradle,
 * plate, mana cradle. The sphere centre and radius are measured from the painting's own spheres,
 * whose interiors are cut out of the cradles and shipped as the liquid files.
 */
#pragma once

#include "engine/point.hpp"
#include "engine/rectangle.hpp"
#include "engine/size.hpp"

namespace devilution::oracool::hud_skin {

constexpr Size PlateSize { $plateW, $Hh };

/** The wells' openings - the flat stone inside the rim - plate-local. */
constexpr Rectangle LmbWell { { $((Scl $lmbWell[0]) - $xl), $(Scl $lmbWell[1]) }, { $(Scl $lmbWell[2]), $(Scl $lmbWell[3]) } };
constexpr Rectangle RmbWell { { $((Scl $rmbWell[0]) - $xl), $(Scl $rmbWell[1]) }, { $(Scl $rmbWell[2]), $(Scl $rmbWell[3]) } };

/** The six belt cells: the painting's holes by alpha where it has them, its painted openings by hand where not. */
constexpr int BeltCellX[6] = { $cells };
constexpr int BeltCellY = $($hole.Y);
constexpr Size BeltCellSize { $cellW, $($hole.H) };
/** Plate-local y of the belt bar's top edge - what the XP bar sits above. */
constexpr int BeltBarTop = $(Scl $beltBarTopMaster);

constexpr Size HealthOrbSize { $xl, $Hh };
constexpr Size ManaOrbSize { $($W - $xr), $Hh };
constexpr Point HealthSphereCenter { $($hcs[0]), $($hcs[1]) };
constexpr Point ManaSphereCenter { $($mcs[0] - $xr), $($mcs[1]) };
constexpr int SphereRadius = $rs;

} // namespace devilution::oracool::hud_skin
"@
[System.IO.File]::WriteAllText($header, $h.Replace("`r`n", "`n"), (New-Object System.Text.UTF8Encoding $false))
Write-Host "wrote $header"
Write-Host "overlay: $(Join-Path $scratch 'overlay.png')"
