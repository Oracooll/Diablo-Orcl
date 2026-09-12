# Oracool asset pipeline: rebuilds the six class-tree icon strips (ui\<class>_tree_icons.png) and
# the attack strip (ui\attack_icons.png) from GPT's vanilla-style skill glyphs.
#
# Source: Resources\01-in-use-assets\delivered-packs\oracool-skill-glyphs-vanilla-v1 (2026-09-05):
# 257 glyphs, 56x56 RGBA, nothing but white (243,243,243) and shadow (12,7,7) on transparency,
# each in glyphs\<class>\<page>\<slug>.png and listed in manifest.json with its class, PAGE name and
# skill NAME. That triple is the join: the strip's frame order IS the ClassTreeSkill order, which
# this script reads out of Source\oracool\class_tree.cpp's Skills table (the same regex
# tools\GenerateHoverMatrix.pl uses) and matches by (class, page, name). Two names repeat across
# pages (Fanaticism aura / passive, Shout Barbarian / Bard); the page keeps them apart.
#
# A row with no glyph keeps the frame it has - the coloured set - so the 18 Sorceress book rows the
# brief excluded, and anything the pack missed, draw as before. Every replaced frame is checked
# for the two colours before it goes in, and every frame written is transparent outside the glyph:
# the game's IsGlyphFrame (hud_art.cpp) recognises exactly that and draws the plate under it.
#
# Usage: powershell -ExecutionPolicy Bypass -File tools\BuildGlyphStrips.ps1
# Run from the repository root. Re-runnable.
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$root = Split-Path -Parent $PSScriptRoot
$pack = Join-Path (Split-Path -Parent $root) 'Resources\01-in-use-assets\delivered-packs\oracool-skill-glyphs-vanilla-v1'
$outDir = Join-Path $root 'Packaging\resources\oracool_assets\ui'
$ICON = 56

$manifest = Get-Content (Join-Path $pack 'manifest.json') -Raw | ConvertFrom-Json
$byKey = @{}
foreach ($e in $manifest) { $byKey[("{0}|{1}|{2}" -f $e.class, $e.page, $e.name)] = $e }
# Later glyph packs in the same format (RfA-04 batch 13, 2026-09-11: the five rows the first pack
# missed). Each entry remembers its own pack folder; a key the first pack already has is an error
# rather than a silent override.
$extraPacks = @('batch-13-skill-glyphs')
foreach ($extra in $extraPacks) {
  $extraRoot = Join-Path (Split-Path -Parent $pack) $extra
  $extraManifest = Get-Content (Join-Path $extraRoot 'manifest.json') -Raw | ConvertFrom-Json
  foreach ($e in $extraManifest) {
    $k = "{0}|{1}|{2}" -f $e.class, $e.page, $e.name
    if ($byKey.ContainsKey($k)) { throw "$extra repeats a glyph the first pack already has: $k" }
    $e | Add-Member -NotePropertyName root -NotePropertyValue $extraRoot
    $byKey[$k] = $e
  }
}

# ---- the skill table, in strip order -------------------------------------------------------------
$src = Get-Content (Join-Path $root 'Source\oracool\class_tree.cpp') -Raw
$tableStart = $src.IndexOf('const ClassTreeSkillData Skills[ClassTreeSkillCount] = {')
$tableEnd = $src.IndexOf("`n};", $tableStart)
$table = $src.Substring($tableStart, $tableEnd - $tableStart)
$rowRe = [regex]'\{\s*N_\("((?:[^"\\]|\\.)*)"\),\s*N_\("(?:[^"\\]|\\.)*"\),\s*(Pal|Bar|Sor|Rog|Bard|Monk),\s*(\d+),'
$rows = @()
foreach ($m in $rowRe.Matches($table)) { $rows += [pscustomobject]@{ Name = $m.Groups[1].Value; Cls = $m.Groups[2].Value; Page = [int]$m.Groups[3].Value } }
if ($rows.Count -lt 150) { throw "only $($rows.Count) rows parsed from class_tree.cpp" }

$className = @{ Pal = 'Paladin'; Bar = 'Barbarian'; Sor = 'Sorceress'; Rog = 'Rogue'; Bard = 'Bard'; Monk = 'Monk' }
$stripFile = @{ Pal = 'paladin_tree_icons.png'; Bar = 'barb_tree_icons.png'; Sor = 'sorc_tree_icons.png'; Rog = 'rogue_tree_icons.png'; Bard = 'bard_tree_icons.png'; Monk = 'monk_tree_icons.png' }
# GetClassTreePageName, page 0..3 per class.
$pageName = @{
  Pal  = @('COMBAT SKILLS', 'OFFENSIVE AURAS', 'DEFENSIVE AURAS', 'PASSIVE SKILLS')
  Bar  = @('COMBAT SKILLS', 'COMBAT MASTERIES', 'WARCRIES', 'PASSIVE SKILLS')
  Sor  = @('COLD SPELLS', 'LIGHTNING SPELLS', 'FIRE SPELLS', 'PASSIVE SKILLS')
  Rog  = @('BOW & CROSSBOW', 'PASSIVE & MAGIC', 'JAVELIN & SPEAR', 'PASSIVE SKILLS')
  Bard = @('MELODY', 'HARMONY', 'POETRY', 'PASSIVE SKILLS')
  Monk = @('WAY OF THE STAFF', 'WAY OF THE BODY', 'WAY OF THE SPIRIT', 'PASSIVE SKILLS')
}

function IsGlyphPixel([System.Drawing.Color]$c) {
  if ($c.A -eq 0) { return $true }
  if ($c.A -ne 255) { return $false }
  return (($c.R -eq 243 -and $c.G -eq 243 -and $c.B -eq 243) -or ($c.R -eq 12 -and $c.G -eq 7 -and $c.B -eq 7))
}

# Copies a glyph file into frame $frame of $strip, pixel-exact, refusing anything but the two colours.
function StampGlyph([System.Drawing.Bitmap]$strip, [int]$frame, [string]$path) {
  $g = [System.Drawing.Bitmap]::FromFile($path)
  try {
    if ($g.Width -ne $ICON -or $g.Height -ne $ICON) { throw "$path is $($g.Width)x$($g.Height), not ${ICON}" }
    $visible = 0
    for ($y = 0; $y -lt $ICON; $y++) {
      for ($x = 0; $x -lt $ICON; $x++) {
        $c = $g.GetPixel($x, $y)
        if (-not (IsGlyphPixel $c)) { throw "$path has a pixel that is neither white, shadow nor clear at $x,$y : $c" }
        if ($c.A -ne 0) { $visible++ }
        $strip.SetPixel($frame * $ICON + $x, $y, $c)
      }
    }
    if ($visible -eq 0) { throw "$path is empty" }
  } finally { $g.Dispose() }
}

function LoadStripEditable([string]$path, [int]$frames) {
  $bmp = New-Object System.Drawing.Bitmap -ArgumentList ($frames * $ICON), $ICON, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  if (Test-Path $path) {
    $old = [System.Drawing.Bitmap]::FromFile($path)
    try {
      $gr = [System.Drawing.Graphics]::FromImage($bmp)
      $gr.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
      $gr.DrawImage($old, 0, 0, $old.Width, $old.Height)
      $gr.Dispose()
    } finally { $old.Dispose() }
  }
  return $bmp
}

$report = @()
$missing = @()
$used = @{}
foreach ($cls in @('Pal', 'Bar', 'Sor', 'Rog', 'Bard', 'Monk')) {
  $mine = @($rows | Where-Object { $_.Cls -eq $cls })
  $path = Join-Path $outDir $stripFile[$cls]
  $strip = LoadStripEditable $path $mine.Count
  $stamped = 0
  for ($i = 0; $i -lt $mine.Count; $i++) {
    $row = $mine[$i]
    $key = "{0}|{1}|{2}" -f $className[$cls], $pageName[$cls][$row.Page], $row.Name
    $entry = $byKey[$key]
    if ($null -eq $entry) { $missing += $key; continue }
    $glyphRoot = if ($entry.PSObject.Properties['root']) { $entry.root } else { $pack }
    StampGlyph $strip $i (Join-Path $glyphRoot $entry.file)
    $used[$key] = $true
    $stamped++
  }
  $strip.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
  $strip.Dispose()
  $report += ("{0,-10} {1,3} rows, {2,3} glyphs stamped -> {3}" -f $className[$cls], $mine.Count, $stamped, $stripFile[$cls])
}

# ---- the attack strip: the two basic attacks, in AttackIcon order (Regular, Fist) ---------------
$attacks = @(
  @{ Name = 'Regular Attack'; Frame = 0 },
  @{ Name = 'Fist'; Frame = 1 }
)
$common = @($manifest | Where-Object { $_.class -eq 'Common' })
$attackPath = Join-Path $outDir 'attack_icons.png'
$attackStrip = New-Object System.Drawing.Bitmap -ArgumentList (2 * $ICON), $ICON, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$attackStamped = 0
foreach ($a in $attacks) {
  $entry = $common | Where-Object { $_.name -eq $a.Name } | Select-Object -First 1
  if ($null -eq $entry) {
    # The pack's second basic attack may be named otherwise; take the remaining Common entry.
    $entry = $common | Where-Object { -not $used.ContainsKey("Common|" + $_.name) -and $_.name -ne 'Regular Attack' } | Select-Object -First 1
  }
  if ($null -eq $entry) { $missing += ("Common|Basic Attacks|" + $a.Name); continue }
  StampGlyph $attackStrip $a.Frame (Join-Path $pack $entry.file)
  $used["Common|" + $entry.name] = $true
  $attackStamped++
  Write-Host ("attack frame {0}: {1} <- {2}" -f $a.Frame, $a.Name, $entry.file)
}
if ($attackStamped -eq 2) { $attackStrip.Save($attackPath, [System.Drawing.Imaging.ImageFormat]::Png) } else { Write-Host "attack strip NOT written ($attackStamped of 2)" }
$attackStrip.Dispose()

$report | ForEach-Object { Write-Host $_ }
Write-Host ("manifest entries: {0}, used: {1}" -f $manifest.Count, $used.Count)
if ($missing.Count -gt 0) { Write-Host "rows with NO glyph (kept their old frame):"; $missing | ForEach-Object { Write-Host "  $_" } }
$unused = $manifest | Where-Object { -not $used.ContainsKey(("{0}|{1}|{2}" -f $_.class, $_.page, $_.name)) -and $_.class -ne 'Common' }
if ($unused) { Write-Host "manifest entries matching NO row:"; $unused | ForEach-Object { Write-Host ("  {0}|{1}|{2}" -f $_.class, $_.page, $_.name) } }
