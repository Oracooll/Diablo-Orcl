# Oracool asset pipeline: cuts ui\paladin_tree_icons.png - the 29 icons of Diablo II's Paladin
# skill tree - out of the reference sheet, as one horizontal strip the game indexes by tree skill.
#
# The sheet is three labelled sections (Combat Skills, Offensive Auras, Defensive Auras) of white
# emblems on a flat green screen, with the skill's name printed under each emblem. The names are NOT
# wanted in the icons: the tree draws them as text, and baking them in would give two names in two
# fonts and freeze them where a translation could never reach.
#
# So the crop rectangles are per-ROW y-bands (measured by scanning for rows that contain any
# non-green pixel, which separates emblem bands from label bands cleanly) crossed with per-COLUMN
# x-spans measured INSIDE each band. That is why the columns are listed per row rather than as one
# grid: the three sections have different counts (9 / 10 / 5+5) and none of them share a pitch.
#
# Strip order IS oracool::PaladinTreeSkill order - see paladin_tree.h, where the enum is documented
# as following this sheet - so the two cannot drift.
#
# Usage: powershell -ExecutionPolicy Bypass -File tools\CutPaladinTree.ps1
# Run from the repository root.
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$sheet = "..\Resources\02. Oracooll Assets\02. Unused\class-trees\Paladin Skill Tree.png"
if (-not (Test-Path $sheet)) { throw "skill tree sheet not found: $sheet" }

# 56px: the tree lays out three columns inside the Abilities window's ~300px content width, so the
# icon is as large as a three-wide grid affords once the connector gutters are taken out.
$ICON = 56

$src = [System.Drawing.Bitmap]::FromFile((Resolve-Path $sheet))
Write-Host ("sheet {0}x{1}" -f $src.Width, $src.Height)

# Measured bands and spans. Each entry: name, x0, x1, y0, y1.
$cells = @(
  # --- Combat Skills (y 78..238) ---
  @("sacrifice",          23, 147,  78, 238),
  @("smite",             180, 303,  78, 238),
  @("holy_bolt",         336, 468,  78, 238),
  @("zeal",              501, 618,  78, 238),
  @("charge",            647, 793,  78, 238),
  @("vengeance",         816, 944,  78, 238),
  @("blessed_hammer",    987,1108,  78, 238),
  @("conversion",       1148,1275,  78, 238),
  @("fist_of_heavens",  1312,1404,  78, 238),
  # --- Offensive Auras (y 365..527) ---
  @("might",              24, 144, 365, 527),
  @("holy_fire",         166, 286, 365, 527),
  @("thorns",            312, 441, 365, 527),
  @("blessed_aim",       470, 585, 365, 527),
  @("concentration",     612, 731, 365, 527),
  @("holy_freeze",       763, 867, 365, 527),
  @("holy_shock",        896, 989, 365, 527),
  @("sanctuary",        1014,1125, 365, 527),
  @("fanaticism",       1149,1285, 365, 527),
  @("conviction",       1301,1422, 365, 527),
  # --- Defensive Auras, first row (y 667..806) ---
  @("prayer",            212, 327, 667, 806),
  @("resist_fire",       418, 542, 667, 806),
  @("defiance",          648, 765, 667, 806),
  @("resist_cold",       871, 994, 667, 806),
  @("cleansing",        1084,1193, 667, 806),
  # --- Defensive Auras, second row (y 869..1007) ---
  @("resist_lightning",  200, 335, 869,1007),
  @("vigor",             406, 550, 869,1007),
  @("meditation",        635, 766, 869,1007),
  @("redemption",        871,1001, 869,1007),
  @("salvation",        1065,1220, 869,1007)
)

# Green is keyed by EXCESS over the stronger of red/blue rather than by a flat threshold, and the
# fringe is unmixed rather than cut: these emblems are white with a soft drop shadow, so a binary
# key leaves a green halo one pixel wide all the way round every icon, which reads as a glow on the
# window's dark fill. Partially-green pixels instead get proportional alpha AND have their green
# channel pulled back down to max(R,B) - standard spill suppression - so the antialiased edge stays
# soft but stops being green.
$KEY_FULL = 60   # excess at or above this is pure backdrop
function GreenExcess($c) { return $c.G - [Math]::Max($c.R, $c.B) }

$strip = New-Object System.Drawing.Bitmap -ArgumentList ($ICON * $cells.Count), $ICON
$g = [System.Drawing.Graphics]::FromImage($strip)
$g.Clear([System.Drawing.Color]::FromArgb(0, 0, 0, 0))
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality

for ($i = 0; $i -lt $cells.Count; $i++) {
  $name = $cells[$i][0]
  $x0 = $cells[$i][1]; $x1 = $cells[$i][2]; $y0 = $cells[$i][3]; $y1 = $cells[$i][4]
  $w = $x1 - $x0 + 1; $h = $y1 - $y0 + 1

  # Green out, alpha in - and tighten to the emblem's real bounds inside the measured cell, so an
  # emblem that does not fill its band is not scaled down by its neighbour's whitespace.
  $cell = New-Object System.Drawing.Bitmap -ArgumentList $w, $h
  $minX = $w; $maxX = -1; $minY = $h; $maxY = -1
  for ($y = 0; $y -lt $h; $y++) {
    for ($x = 0; $x -lt $w; $x++) {
      $p = $src.GetPixel($x0 + $x, $y0 + $y)
      $ex = GreenExcess $p
      if ($ex -ge $KEY_FULL) {
        $cell.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(0, 0, 0, 0))
        continue
      }
      $a = 255; $r = $p.R; $gg = $p.G; $b = $p.B
      if ($ex -gt 0) {
        $a = [int](255 * (1.0 - ($ex / $KEY_FULL)))
        $gg = [Math]::Max($p.R, $p.B)   # spill suppression: no green left in the soft edge
      }
      if ($a -le 8) {
        $cell.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(0, 0, 0, 0))
        continue
      }
      $cell.SetPixel($x, $y, [System.Drawing.Color]::FromArgb($a, $r, $gg, $b))
      if ($x -lt $minX) { $minX = $x }; if ($x -gt $maxX) { $maxX = $x }
      if ($y -lt $minY) { $minY = $y }; if ($y -gt $maxY) { $maxY = $y }
    }
  }
  if ($maxX -lt 0) { throw "cell $name is entirely background" }

  $cw = $maxX - $minX + 1; $ch = $maxY - $minY + 1
  # Fit inside the square preserving aspect, centred - a uniform scale keeps the emblems reading as
  # one set rather than each being stretched to its own proportions.
  $scale = [Math]::Min($ICON / $cw, $ICON / $ch)
  $dw = [int][Math]::Round($cw * $scale); $dh = [int][Math]::Round($ch * $scale)
  $dx = $i * $ICON + [int](($ICON - $dw) / 2)
  $dy = [int](($ICON - $dh) / 2)
  $g.DrawImage($cell, (New-Object System.Drawing.Rectangle $dx, $dy, $dw, $dh),
      $minX, $minY, $cw, $ch, [System.Drawing.GraphicsUnit]::Pixel)
  $cell.Dispose()
  Write-Host ("  {0,2}: {1,-18} {2}x{3} -> {4}x{5}" -f $i, $name, $cw, $ch, $dw, $dh)
}

$g.Dispose()
$src.Dispose()

$out = "Packaging\resources\oracool_assets\ui\paladin_tree_icons.png"
New-Item -ItemType Directory -Force -Path (Split-Path $out) | Out-Null
$strip.Save($out, [System.Drawing.Imaging.ImageFormat]::Png)
$strip.Dispose()
Write-Host ("wrote {0} ({1} icons at {2}px)" -f $out, $cells.Count, $ICON)
