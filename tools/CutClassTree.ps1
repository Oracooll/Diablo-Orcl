# Oracool asset pipeline: cuts a class skill-tree sheet into ui\<name>_tree_icons.png - one
# horizontal strip the game indexes by tree skill.
#
# Table-driven, like tools/CutPaladinTree.ps1 which it is modelled on. The rectangles below were
# MEASURED (a throwaway scan printing the y-bands of non-green rows, then the x-spans inside each
# band) rather than read off by eye, and are recorded here so a re-cut is reproducible.
#
# Two things the measurement had to handle and the numbers already account for:
#   - The skill names are printed under each emblem and are not wanted: the tree draws names as
#     text, and a baked name can never be translated. So the crops are the EMBLEM bands only, which
#     are the tall ones - titles and labels are both short.
#   - On the Barbarian's first row Bash and Leap physically TOUCH (Leap's motion lines run into
#     Bash's impact burst), so no gap threshold separates them; the boundary was found by scanning
#     for the emptiest column between them. Same for one pair on the Rogue's second row.
#
# Green is unmixed rather than cut: these emblems are white with a soft drop shadow, so a binary key
# leaves a green halo one pixel wide around every one of them, which reads as a glow on the window's
# dark fill. Partially-green pixels get proportional alpha AND have their green channel pulled back
# to max(R,B) - standard spill suppression.
#
# Strip order is left-to-right within a band, bands top to bottom, which IS the order each class's
# skill table declares - so the art and the code cannot drift.
#
# Usage: powershell -ExecutionPolicy Bypass -File tools\CutClassTree.ps1 -Class barb
# Run from the repository root.
param(
  [Parameter(Mandatory = $true)][ValidateSet("barb", "sorc", "rogue", "bard")][string]$Class,
  [int]$Icon = 56
)
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

# sheet file, then one line per band: "y0 y1 : x0-x1 x0-x1 ..."
$sheets = @{
  "barb"  = @{
    File  = "Barb Skill Tree.png"
    Bands = @(
      "101 264 : 34-157 158-285 315-461 492-590 612-721 748-860 893-1011 1026-1150 1166-1277 1295-1423",
      "447 596 : 34-145 173-282 320-429 458-562 594-716 739-859 893-1008 1034-1143 1160-1287 1308-1407",
      "770 940 : 36-170 190-279 309-424 448-573 594-708 735-869 903-992 1032-1121 1144-1269 1292-1414"
    )
  }
  "sorc"  = @{
    File  = "Sorc Skill Tree.png"
    Bands = @(
      "103 252 : 35-151 181-299 322-442 468-580 604-720 749-845 879-998 1022-1141 1161-1275 1295-1419",
      "443 596 : 33-147 171-298 318-435 458-582 612-705 738-859 885-994 1019-1130 1153-1274 1295-1419",
      "776 931 : 35-145 177-279 327-418 444-585 601-709 729-856 893-993 1018-1131 1162-1272 1296-1420"
    )
  }
  "rogue" = @{
    File  = "Rogue Skill Tree.png"
    Bands = @(
      "92 246 : 20-152 179-296 316-446 478-577 600-733 751-866 896-1016 1040-1154 1165-1274 1303-1429",
      "459 607 : 26-147 170-301 310-412 437-552 587-703 704-869 888-989 1021-1144 1154-1276 1295-1430",
      "795 942 : 27-166 180-300 320-439 456-583 597-709 738-860 869-991 1004-1142 1166-1265 1295-1429"
    )
  }
  # The Bard's sheet is laid out differently from the other four: its icon strip sits in THREE
  # green panels (one per page) each holding two rows - four icons then three - and every icon has
  # its name printed underneath IN the same white as the icon, so no colour test separates them.
  # The rows below are the two emblem bands only, read off a vertical ink profile; the label bands
  # fall in the gaps between them and are excluded by construction. Six band lines rather than
  # three, ordered Melody 1-7, Harmony 1-7, Poetry 1-7, which is the enum's order.
  "bard"  = @{
    File  = "Bard Skill Trees.png"
    Bands = @(
      "707 803 : 37-112 146-247 277-368 403-487",
      "868 969 : 48-127 187-270 327-423",
      "707 803 : 530-627 653-748 779-871 909-989",
      "868 969 : 563-642 695-787 831-924",
      "707 803 : 1038-1122 1159-1239 1276-1364 1395-1502",
      "868 969 : 1085-1164 1217-1310 1355-1465"
    )
  }
}

$def = $sheets[$Class]
$path = Join-Path "..\Oracool.MPQ" $def.File
if (-not (Test-Path $path)) { throw "sheet not found: $path" }
$src = [System.Drawing.Bitmap]::FromFile((Resolve-Path $path))
Write-Host ("sheet {0}: {1}x{2}" -f $def.File, $src.Width, $src.Height)

# Excess of green over the stronger of red/blue; at or above this a pixel is pure backdrop.
$KEY_FULL = 60

# Flatten the tables into one ordered list of "x0 x1 y0 y1" strings. Strings rather than nested
# arrays on purpose - PowerShell's handling of arrays-of-arrays is what broke an earlier attempt
# at this script, in a way that surfaced as an unrelated arithmetic error.
$cells = New-Object System.Collections.ArrayList
foreach ($bandLine in $def.Bands) {
  $halves = $bandLine -split ' : '
  $yParts = $halves[0].Trim() -split '\s+'
  $y0 = [int]$yParts[0]; $y1 = [int]$yParts[1]
  foreach ($span in ($halves[1].Trim() -split '\s+')) {
    $xParts = $span -split '-'
    [void]$cells.Add("$([int]$xParts[0]) $([int]$xParts[1]) $y0 $y1")
  }
}
Write-Host ("cells: " + $cells.Count)

$strip = New-Object System.Drawing.Bitmap -ArgumentList ($Icon * $cells.Count), $Icon
$g = [System.Drawing.Graphics]::FromImage($strip)
$g.Clear([System.Drawing.Color]::FromArgb(0, 0, 0, 0))
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality

$i = 0
foreach ($cellStr in $cells) {
  $p4 = $cellStr -split '\s+'
  $x0 = [int]$p4[0]; $x1 = [int]$p4[1]; $cy0 = [int]$p4[2]; $cy1 = [int]$p4[3]
  $w = $x1 - $x0 + 1; $h = $cy1 - $cy0 + 1

  $cell = New-Object System.Drawing.Bitmap -ArgumentList $w, $h
  $minX = $w; $maxX = -1; $minY = $h; $maxY = -1
  for ($y = 0; $y -lt $h; $y++) {
    for ($x = 0; $x -lt $w; $x++) {
      $c = $src.GetPixel($x0 + $x, $cy0 + $y)
      $cr = [int]$c.R; $cg = [int]$c.G; $cb = [int]$c.B
      $mx = $cr; if ($cb -gt $mx) { $mx = $cb }
      $ex = $cg - $mx
      if ($ex -ge $KEY_FULL) { $cell.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(0, 0, 0, 0)); continue }
      $a = 255; $outG = $cg
      if ($ex -gt 0) {
        $a = [int](255 * (1.0 - ($ex / $KEY_FULL)))
        $outG = $mx   # spill suppression: no green left in the soft edge
      }
      if ($a -le 8) { $cell.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(0, 0, 0, 0)); continue }
      $cell.SetPixel($x, $y, [System.Drawing.Color]::FromArgb($a, $cr, $outG, $cb))
      if ($x -lt $minX) { $minX = $x }
      if ($x -gt $maxX) { $maxX = $x }
      if ($y -lt $minY) { $minY = $y }
      if ($y -gt $maxY) { $maxY = $y }
    }
  }
  if ($maxX -lt 0) { throw "cell $i is entirely background" }

  $cw = $maxX - $minX + 1; $ch = $maxY - $minY + 1
  # Uniform scale, centred: the emblems read as one set rather than each stretched to its own shape.
  $scale = [Math]::Min(($Icon / $cw), ($Icon / $ch))
  $dw = [int][Math]::Round($cw * $scale); $dh = [int][Math]::Round($ch * $scale)
  $dx = ($i * $Icon) + [int](($Icon - $dw) / 2)
  $dy = [int](($Icon - $dh) / 2)
  $rect = New-Object System.Drawing.Rectangle $dx, $dy, $dw, $dh
  $g.DrawImage($cell, $rect, $minX, $minY, $cw, $ch, [System.Drawing.GraphicsUnit]::Pixel)
  $cell.Dispose()
  $i++
}

$g.Dispose()
$src.Dispose()

$outPath = "Packaging\resources\oracool_assets\ui\${Class}_tree_icons.png"
New-Item -ItemType Directory -Force -Path (Split-Path $outPath) | Out-Null
$strip.Save($outPath, [System.Drawing.Imaging.ImageFormat]::Png)
$strip.Dispose()
Write-Host ("wrote {0} ({1} icons at {2}px)" -f $outPath, $cells.Count, $Icon)
