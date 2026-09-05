# Oracool audit: every frame of every class strip and the attack strip is either a GLYPH (nothing but
# white 243 / shadow 12,7,7 on transparency, visible bbox inside x8..47 y8..47, non-empty) or a
# LEGACY coloured frame (anything else, non-empty). Reports counts per strip and any empty frame.
# Reads the same Skills table as BuildGlyphStrips.ps1 to name each frame, and says which rows are
# still legacy so the list can be checked against the 18 book-spell rows the brief excluded.
#
# Usage: powershell -ExecutionPolicy Bypass -File tools\AuditGlyphStrips.ps1
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
$root = Split-Path -Parent $PSScriptRoot
$ui = Join-Path $root 'Packaging\resources\oracool_assets\ui'
$ICON = 56

$src = Get-Content (Join-Path $root 'Source\oracool\class_tree.cpp') -Raw
$tableStart = $src.IndexOf('const ClassTreeSkillData Skills[ClassTreeSkillCount] = {')
$tableEnd = $src.IndexOf("`n};", $tableStart)
$table = $src.Substring($tableStart, $tableEnd - $tableStart)
$rowRe = [regex]'\{\s*N_\("((?:[^"\\]|\\.)*)"\),\s*N_\("(?:[^"\\]|\\.)*"\),\s*(Pal|Bar|Sor|Rog|Bard|Monk),\s*(\d+),'
$rows = @()
foreach ($m in $rowRe.Matches($table)) { $rows += [pscustomobject]@{ Name = $m.Groups[1].Value; Cls = $m.Groups[2].Value; Page = [int]$m.Groups[3].Value } }
$stripFile = @{ Pal = 'paladin_tree_icons.png'; Bar = 'barb_tree_icons.png'; Sor = 'sorc_tree_icons.png'; Rog = 'rogue_tree_icons.png'; Bard = 'bard_tree_icons.png'; Monk = 'monk_tree_icons.png' }

function ClassifyFrame([System.Drawing.Bitmap]$bmp, [int]$frame) {
  $visible = 0; $glyph = $true; $minX = 99; $minY = 99; $maxX = -1; $maxY = -1
  for ($y = 0; $y -lt $ICON; $y++) {
    for ($x = 0; $x -lt $ICON; $x++) {
      $c = $bmp.GetPixel($frame * $ICON + $x, $y)
      if ($c.A -eq 0) { continue }
      $visible++
      if ($x -lt $minX) { $minX = $x }; if ($x -gt $maxX) { $maxX = $x }; if ($y -lt $minY) { $minY = $y }; if ($y -gt $maxY) { $maxY = $y }
      if ($c.A -ne 255) { $glyph = $false; continue }
      $white = ($c.R -eq 243 -and $c.G -eq 243 -and $c.B -eq 243)
      $shadow = ($c.R -eq 12 -and $c.G -eq 7 -and $c.B -eq 7)
      if (-not ($white -or $shadow)) { $glyph = $false }
    }
  }
  if ($visible -eq 0) { return 'EMPTY' }
  if ($glyph) {
    if ($minX -lt 8 -or $minY -lt 8 -or $maxX -gt 47 -or $maxY -gt 47) { return 'GLYPH-OUT-OF-BOUNDS' }
    return 'glyph'
  }
  return 'legacy'
}

$problems = 0
foreach ($cls in @('Pal', 'Bar', 'Sor', 'Rog', 'Bard', 'Monk')) {
  $mine = @($rows | Where-Object { $_.Cls -eq $cls })
  $path = Join-Path $ui $stripFile[$cls]
  $bmp = [System.Drawing.Bitmap]::FromFile($path)
  $frames = [int]($bmp.Width / $ICON)
  if ($frames -ne $mine.Count) { Write-Host ("{0}: strip has {1} frames, table has {2} rows - MISMATCH" -f $stripFile[$cls], $frames, $mine.Count); $problems++ }
  $counts = @{ glyph = 0; legacy = 0; EMPTY = 0; 'GLYPH-OUT-OF-BOUNDS' = 0 }
  $legacyNames = @()
  for ($i = 0; $i -lt [Math]::Min($frames, $mine.Count); $i++) {
    $k = ClassifyFrame $bmp $i
    $counts[$k]++
    if ($k -eq 'legacy') { $legacyNames += $mine[$i].Name }
    if ($k -eq 'EMPTY' -or $k -eq 'GLYPH-OUT-OF-BOUNDS') { Write-Host ("  {0} frame {1} ({2}): {3}" -f $stripFile[$cls], $i, $mine[$i].Name, $k); $problems++ }
  }
  $bmp.Dispose()
  Write-Host ("{0,-24} {1,3} frames: {2,3} glyph, {3,2} legacy, {4} empty" -f $stripFile[$cls], $frames, $counts.glyph, $counts.legacy, $counts.EMPTY)
  if ($legacyNames.Count -gt 0) { Write-Host ("    legacy: " + ($legacyNames -join ', ')) }
}
$attack = [System.Drawing.Bitmap]::FromFile((Join-Path $ui 'attack_icons.png'))
$af = [int]($attack.Width / $ICON)
for ($i = 0; $i -lt $af; $i++) { $k = ClassifyFrame $attack $i; Write-Host ("attack_icons.png frame {0}: {1}" -f $i, $k); if ($k -ne 'glyph') { $problems++ } }
$attack.Dispose()
Write-Host ("problems: {0}" -f $problems)
