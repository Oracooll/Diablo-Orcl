# Oracool asset pipeline: stamps the user's redesigned class-tree glyphs (2026-10-03) into a class's icon strip.
#
# The user draws them from the green-screen reference cards (OracoolPreview.DISABLED_NecromancerSheetGlyphs, with
# ORCL_SHEETS_CLASS for the other classes) and delivers one folder per Abilities sheet in
# Resources\02. Oracooll Assets\<Class> Skills:
#   "<Sheet> Glyphs Redesign\Individual 42x42\NN_<slug>.png" + validation.txt ("NN <Skill Name>: PASS ...").
# Each glyph is 42x42: pure white (255,255,255) and its shadow pure black (0,0,0), 2px left and down, on chroma green
# (0,255,0). The Necromancer came first; the Rogue and the Monk followed the same day ("i redesigned monk and rogue skill
# glyphs as well. apply them as well").
#
# The game's strip is different on purpose, so every glyph is converted, not copied:
#   - 56x56 cells, one per class row in class_tree.cpp's Skills table order (the strip's frame order IS that order -
#     see BuildGlyphStrips.ps1);
#   - the glyph centred 1:1 on its cell (seven clear pixels round it, so it sits centred on the gold plate - the user:
#     "don't upscale to 56x56. center the 42x42 glyphs onto the gold backings"), green made transparent;
#   - white -> (243,243,243) and black -> (12,7,7), the two colours IsGlyphFrame (hud_art.cpp) recognises - anything
#     else and the game would draw it as a picture, without the plate under it.
# The join is by NAME (validation.txt's name against the table's), so a sheet's order can never put a glyph on the
# wrong skill. A retired row (it holds its frame but sits on no page) gets no delivered glyph and keeps the frame it
# has. A name that matches no row, a sheet row left without a glyph, or a pixel that is neither white, black nor green,
# stops the script before anything is written.
#
# Usage: powershell -ExecutionPolicy Bypass -File tools\ApplyNecroGlyphRedesign.ps1 [-Class Rogue|Monk]   (repo root)
# Then repack: tools\build_oracool_mpq.cmd
param([ValidateSet('Necromancer', 'Rogue', 'Monk')][string]$Class = 'Necromancer')
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$configs = @{
  Necromancer = @{ Tag = 'Nec'; Strip = 'necro_tree_icons.png'; Sheets = @('Summoning', 'Poison & Bone', 'Curses', 'Passive Skills') }
  Rogue       = @{ Tag = 'Rog'; Strip = 'rogue_tree_icons.png'; Sheets = @('Bow & Crossbow', 'Passive & Magic', 'Javelin & Spear', 'Passive Skills') }
  Monk        = @{ Tag = 'Monk'; Strip = 'monk_tree_icons.png'; Sheets = @('Way of the Staff', 'Way of the Body', 'Way of the Spirit', 'Passive Skills') }
}
$config = $configs[$Class]
$root = Split-Path -Parent $PSScriptRoot
$deliveries = Join-Path (Split-Path -Parent $root) "Resources\02. Oracooll Assets\$Class Skills"
$stripPath = Join-Path $root "Packaging\resources\oracool_assets\ui\$($config.Strip)"
$ICON = 56
$GLYPH = 42
$PAD = ($ICON - $GLYPH) / 2
# The delivery's names for rows the table names otherwise. "Bone Spires" is the Necromancer's Poison & Bone 07 - tier 2,
# column 1 - which the table (and the sheet in game) calls Bone Spikes.
$aliases = @{ 'bone spires' = 'bone spikes' }

# ---- the class's rows, in strip order (BuildGlyphStrips.ps1's regex) ----------------------------------------------
$src = Get-Content (Join-Path $root 'Source\oracool\class_tree.cpp') -Raw
$tableStart = $src.IndexOf('const ClassTreeSkillData Skills[ClassTreeSkillCount] = {')
$tableEnd = $src.IndexOf("`n};", $tableStart)
$table = $src.Substring($tableStart, $tableEnd - $tableStart)
$rowRe = [regex]'\{\s*N_\("((?:[^"\\]|\\.)*)"\),\s*N_\("(?:[^"\\]|\\.)*"\),\s*(Pal|Bar|Sor|Rog|Bard|Monk|Nec),\s*(\d+|RetiredFromTreePage),'
$frameOf = @{}
$rows = 0
foreach ($m in $rowRe.Matches($table)) {
  if ($m.Groups[2].Value -ne $config.Tag) { continue }
  # A retired row is on no sheet, and its name may repeat a live one's, so it never takes a delivered glyph.
  if ($m.Groups[3].Value -ne 'RetiredFromTreePage') { $frameOf[$m.Groups[1].Value.ToLowerInvariant()] = $rows }
  $rows++
}
if ($rows -lt 72) { throw "parsed only $rows $Class rows from class_tree.cpp" }

# ---- the deliveries --------------------------------------------------------------------------------------------
$plan = @{}
foreach ($sheet in $config.Sheets) {
  $folder = Join-Path $deliveries "$sheet Glyphs Redesign"
  $validation = Join-Path $folder 'validation.txt'
  if (-not (Test-Path $validation)) { throw "no validation.txt in $folder" }
  foreach ($line in Get-Content $validation) {
    if ($line -notmatch '^(\d{2}) (.+?): (\w+)') { continue }
    $nn = $Matches[1]; $name = $Matches[2]; $verdict = $Matches[3]
    if ($verdict -ne 'PASS') { throw "$sheet $nn $name is marked $verdict in validation.txt" }
    $key = $name.ToLowerInvariant()
    if ($aliases.ContainsKey($key)) { $key = $aliases[$key] }
    if (-not $frameOf.ContainsKey($key)) { throw "$sheet glyph '$name' matches no $Class row in class_tree.cpp" }
    $file = Get-ChildItem (Join-Path $folder 'Individual 42x42') -Filter "${nn}_*.png" | Select-Object -First 1
    if ($null -eq $file) { throw "$sheet $nn $name has no Individual 42x42\${nn}_*.png" }
    if ($plan.ContainsKey($frameOf[$key])) { throw "two glyphs for '$name'" }
    $plan[$frameOf[$key]] = [pscustomobject]@{ Sheet = $sheet; Name = $name; Path = $file.FullName }
  }
}
if ($plan.Count -ne $frameOf.Count) { throw "$($plan.Count) glyphs delivered for $($frameOf.Count) rows on the sheets" }

# ---- convert and stamp ------------------------------------------------------------------------------------------
$white = [System.Drawing.Color]::FromArgb(255, 243, 243, 243)
$shadow = [System.Drawing.Color]::FromArgb(255, 12, 7, 7)
$clear = [System.Drawing.Color]::FromArgb(0, 0, 0, 0)
$old = [System.Drawing.Bitmap]::FromFile($stripPath)
if ($old.Width -ne $rows * $ICON -or $old.Height -ne $ICON) { throw "$($config.Strip) is $($old.Width)x$($old.Height), not $rows frames of ${ICON}px" }
$strip = New-Object System.Drawing.Bitmap ($rows * $ICON), $ICON, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$kept = 0
try {
  foreach ($frame in 0..($rows - 1)) {
    $entry = $plan[$frame]
    if ($null -eq $entry) {
      # No sheet delivers this frame (a retired row): copied over unchanged.
      for ($y = 0; $y -lt $ICON; $y++) { for ($x = 0; $x -lt $ICON; $x++) { $strip.SetPixel($frame * $ICON + $x, $y, $old.GetPixel($frame * $ICON + $x, $y)) } }
      $kept++
      continue
    }
    $g = [System.Drawing.Bitmap]::FromFile($entry.Path)
    try {
      if ($g.Width -ne $GLYPH -or $g.Height -ne $GLYPH) { throw "$($entry.Path) is $($g.Width)x$($g.Height), not ${GLYPH}x${GLYPH}" }
      for ($y = 0; $y -lt $ICON; $y++) { for ($x = 0; $x -lt $ICON; $x++) { $strip.SetPixel($frame * $ICON + $x, $y, $clear) } }
      $visible = 0
      for ($y = 0; $y -lt $GLYPH; $y++) {
        for ($x = 0; $x -lt $GLYPH; $x++) {
          $c = $g.GetPixel($x, $y)
          if ($c.R -eq 0 -and $c.G -eq 255 -and $c.B -eq 0) { continue }
          if ($c.R -eq 255 -and $c.G -eq 255 -and $c.B -eq 255) { $out = $white }
          elseif ($c.R -eq 0 -and $c.G -eq 0 -and $c.B -eq 0) { $out = $shadow }
          else { throw "$($entry.Path) has a pixel that is neither white, black nor green at $x,$y : $c" }
          $strip.SetPixel($frame * $ICON + $PAD + $x, $PAD + $y, $out)
          $visible++
        }
      }
      if ($visible -eq 0) { throw "$($entry.Path) is empty" }
    } finally { $g.Dispose() }
  }
} finally { $old.Dispose() }
$strip.Save($stripPath, [System.Drawing.Imaging.ImageFormat]::Png)
$strip.Dispose()
Write-Host "$($config.Strip): $rows frames of ${ICON}px - $($plan.Count) from the 2026-10-03 redesign ($($config.Sheets -join ', ')), $kept kept"
