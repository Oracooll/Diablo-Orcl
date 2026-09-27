# CutBeltGlyphs.ps1 - builds the two belt-button glyph strips from the oracool-belt-glyphs-v1 pack.
#
# Source: Resources\02. Oracooll Assets\delivered-packs\oracool-belt-glyphs-v1 (2026-09-06): six 30x30
# RGBA glyphs, white (243,243,243) and shadow (12,7,7) on transparency, three states per button:
# idle, hover, pressed. The pressed state is the idle mask shifted (+1,+1) with no shadow, BY DESIGN
# - the pack's README says not to recentre any state by its bounds, and the game does not.
#
# Output: ui\belt_glyphs_tp.png and ui\belt_glyphs_menu.png, each 90x30 - idle, hover, pressed left
# to right - in Packaging\resources\oracool_assets\ui. The engine's IsGlyphFrame recognises the two
# colours and DrawTownPortalIcon / DrawBurgerMenuButton draw frame <state> 1:1, centred in the cell.
# Re-run tools\build_oracool_mpq.cmd afterwards: a normal build does NOT repack oracool.mpq.
#
# Usage: powershell -ExecutionPolicy Bypass -File tools\CutBeltGlyphs.ps1
Add-Type -AssemblyName System.Drawing
$root = Split-Path -Parent $PSScriptRoot
$pack = Join-Path (Split-Path -Parent $root) 'Resources\02. Oracooll Assets\delivered-packs\oracool-belt-glyphs-v1\glyphs\belt'
$outDir = Join-Path $root 'Packaging\resources\oracool_assets\ui'
$ICON = 30
$states = @('idle', 'hover', 'pressed')

function IsGlyphPixel([System.Drawing.Color]$c) {
  if ($c.A -eq 0) { return $true }
  if ($c.A -ne 255) { return $false }
  return (($c.R -eq 243 -and $c.G -eq 243 -and $c.B -eq 243) -or ($c.R -eq 12 -and $c.G -eq 7 -and $c.B -eq 7))
}

foreach ($button in @(@{ name = 'town_portal'; out = 'belt_glyphs_tp.png' }, @{ name = 'burger_menu'; out = 'belt_glyphs_menu.png' })) {
  $strip = New-Object System.Drawing.Bitmap ($ICON * $states.Count), $ICON, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
  $g = [System.Drawing.Graphics]::FromImage($strip)
  $g.Clear([System.Drawing.Color]::Transparent)
  for ($i = 0; $i -lt $states.Count; $i++) {
    $file = Join-Path $pack ("{0}_{1}.png" -f $button.name, $states[$i])
    if (-not (Test-Path $file)) { throw "missing $file" }
    $src = New-Object System.Drawing.Bitmap $file
    if ($src.Width -ne $ICON -or $src.Height -ne $ICON) { throw "$file is $($src.Width)x$($src.Height), not ${ICON}x${ICON}" }
    for ($y = 0; $y -lt $ICON; $y++) { for ($x = 0; $x -lt $ICON; $x++) {
      $c = $src.GetPixel($x, $y)
      if (-not (IsGlyphPixel $c)) { throw "$file has a non-glyph pixel at $x,$y : $c" }
      if ($c.A -eq 255) { $strip.SetPixel($i * $ICON + $x, $y, $c) }
    } }
    $src.Dispose()
  }
  $g.Dispose()
  $path = Join-Path $outDir $button.out
  $strip.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
  $strip.Dispose()
  Write-Host "wrote $path"
}
