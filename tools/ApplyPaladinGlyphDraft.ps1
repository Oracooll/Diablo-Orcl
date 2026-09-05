# Oracool asset pipeline: stamps the eleven Paladin Combat glyphs of the GPT/Codex style draft
# (Oracool.MPQ\03-concepts\skill-glyphs\paladin-combat-draft-01) onto the vanilla blank plate and
# writes them into ui\paladin_tree_icons.png at their ClassTreeSkill frames.
#
# The draft is an APPROVAL delivery: its README holds the standalone 56x56 exports back "pending
# style approval", and what it ships is two contact sheets. The native sheet, though, is the real
# buffers composited 1:1 onto a flat #747474 ground (build-review.cjs: glyph at nx+23, ny+35 with
# nx = (i%4)*180, ny = 80 + floor(i/4)*120), and every glyph pixel is exactly white (243,243,243)
# or shadow (12,7,7) - so the glyphs come back out of the sheet losslessly by colour, and the
# recovered white/shadow counts are checked against the draft's own pixel-checks.json.
#
# The glyphs go in on TRANSPARENCY, no plate: hud_art's IsGlyphFrame recognises a frame that is
# nothing but white (243) and shadow (12,7,7) and draws it 1:1 on the engine's own plate through
# ApplyPlateTint, which is what gives the new icons the plate colour coding (gold ready, light grey
# unspent, red locked). The first build (v1.9.250) baked spelicon frame 26 in, and every plate came
# out gold whatever the skill's state ("what is the color coding of this skill page?" - none).
#
# Usage: powershell -ExecutionPolicy Bypass -File tools\ApplyPaladinGlyphDraft.ps1
# Run from the repository root. Re-runnable: it overwrites only the eleven frames it owns.
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$root = Split-Path -Parent $PSScriptRoot
$mpq = Join-Path (Split-Path -Parent $root) 'Oracool.MPQ'
$draft = Join-Path $mpq '03-concepts\skill-glyphs\paladin-combat-draft-01'
$sheetPath = Join-Path $draft 'contact-sheet-native.png'
$checksPath = Join-Path $draft 'pixel-checks.json'
$stripPath = Join-Path $root 'Packaging\resources\oracool_assets\ui\paladin_tree_icons.png'
foreach ($f in @($sheetPath, $checksPath, $stripPath)) { if (-not (Test-Path $f)) { throw "missing: $f" } }

$ICON = 56
# Sheet order (build-review.cjs walks the config in order) -> ClassTreeSkill frame. Hammer of Faith
# and Blessed Shield live at the END of the Paladin block (class_tree.h: "Combat Skills, appended").
$frames = @(
  @('sacrifice', 0), @('smite', 1), @('holy-bolt', 2), @('zeal', 3), @('charge', 4), @('vengeance', 5),
  @('hammer-of-faith', 29), @('blessed-hammer', 6), @('blessed-shield', 30), @('conversion', 7), @('fist-of-the-heavens', 8)
)

$checks = Get-Content $checksPath -Raw | ConvertFrom-Json
$sheet = [System.Drawing.Bitmap]::FromFile((Resolve-Path $sheetPath))

# Load the strip into a fresh 32bpp bitmap so the file handle is released before we save over it.
$loaded = [System.Drawing.Bitmap]::FromFile((Resolve-Path $stripPath))
$strip = New-Object System.Drawing.Bitmap -ArgumentList $loaded.Width, $loaded.Height, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$gs = [System.Drawing.Graphics]::FromImage($strip)
$gs.DrawImage($loaded, 0, 0, $loaded.Width, $loaded.Height)
$gs.Dispose(); $loaded.Dispose()
$cells = [int]($strip.Width / $ICON)

$white = [System.Drawing.Color]::FromArgb(255, 243, 243, 243)
$shadow = [System.Drawing.Color]::FromArgb(255, 12, 7, 7)
$clear = [System.Drawing.Color]::FromArgb(0, 0, 0, 0)

for ($i = 0; $i -lt $frames.Count; $i++) {
  $slug = $frames[$i][0]; $frame = $frames[$i][1]
  if ($frame -ge $cells) { throw "frame $frame for $slug is past the strip's $cells cells" }
  $nx = ($i % 4) * 180 + 23; $ny = 80 + [Math]::Floor($i / 4) * 120 + 35
  $nWhite = 0; $nShadow = 0
  for ($y = 0; $y -lt $ICON; $y++) {
    for ($x = 0; $x -lt $ICON; $x++) {
      $sp = $sheet.GetPixel($nx + $x, $ny + $y)
      $out = $clear
      if ($sp.R -eq 243 -and $sp.G -eq 243 -and $sp.B -eq 243) { $out = $white; $nWhite++ }
      elseif ($sp.R -eq 12 -and $sp.G -eq 7 -and $sp.B -eq 7) { $out = $shadow; $nShadow++ }
      $strip.SetPixel($frame * $ICON + $x, $y, $out)
    }
  }
  $check = $checks | Where-Object { $_.file -eq ($slug + '.png') }
  if ($null -eq $check) { throw "no pixel check for $slug" }
  if ($nWhite -ne $check.whitePixels -or $nShadow -ne $check.shadowPixels) {
    throw ("{0}: recovered {1} white / {2} shadow, the draft says {3} / {4}" -f $slug, $nWhite, $nShadow, $check.whitePixels, $check.shadowPixels)
  }
  Write-Host ("  frame {0,2}: {1,-20} {2} white {3} shadow  (matches pixel-checks.json)" -f $frame, $slug, $nWhite, $nShadow)
}

$sheet.Dispose()
$strip.Save($stripPath, [System.Drawing.Imaging.ImageFormat]::Png)
$strip.Dispose()
Write-Host ("wrote {0} ({1} frames, {2} replaced)" -f $stripPath, $cells, $frames.Count)
