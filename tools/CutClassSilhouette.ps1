# Oracool asset pipeline: cuts ui\silhouette_paladin.png out of the class silhouette reference
# sheet, for the inventory window's equipment area.
#
# The silhouette used to be baked into ui\inventory_panel.png. The shared theme replaced that
# composition with a procedural fill and bevel, and the silhouette went with it - this brings it
# back as its own asset so it survives future restyles.
#
# The sheet is six labelled cards in a 3x2 grid, each a dark figure on grey stone inside a dark
# ornate frame. Thresholding the whole cell picks up that FRAME as well as the figure (measured:
# it returns a 389x452 box in a 429x612 cell - essentially the whole card), so the search area is
# inset past the frame and the bottom label band before keying.
#
# The key is by luminance, not chroma: card stone measures ~59 mean, the figure bottoms out at ~5,
# so the midpoint cleanly separates them. Output alpha is BINARY because hud_art's QuantizeAsset
# thresholds at a<128 - soft edges would be clipped to hard ones anyway, so they are made hard
# here where the cutoff is visible rather than silently downstream.
#
# Usage: powershell -ExecutionPolicy Bypass -File tools\CutClassSilhouette.ps1
# Run from the repository root.
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$sheet = "..\Oracool.MPQ\04-references\class-layouts\class-silhouette-reference-sheet-v2.png"
if (-not (Test-Path $sheet)) { throw "silhouette sheet not found: $sheet" }
$src = [System.Drawing.Bitmap]::FromFile((Resolve-Path $sheet))

# Grid position of the class to cut. Row 0: Barbarian, Paladin, Warrior. Row 1: Archer, Sorcerer,
# Necromancer.
$COL = 1; $ROW = 0; $NAME = "paladin"
$cw = [int]($src.Width / 3); $ch = [int]($src.Height / 2)
$cx = $COL * $cw; $cy = $ROW * $ch

# Inset past the card's frame, and past the label strip along the bottom.
$INSET_X = 50; $INSET_TOP = 45; $INSET_BOTTOM = 115
$x0 = $cx + $INSET_X; $x1 = $cx + $cw - $INSET_X
$y0 = $cy + $INSET_TOP; $y1 = $cy + $ch - $INSET_BOTTOM

function Luma([System.Drawing.Color]$c) { return 0.299*$c.R + 0.587*$c.G + 0.114*$c.B }

# Card level from the interior's corners, figure floor from the darkest interior pixel.
# Coordinates computed into variables first: PowerShell parses arithmetic inside a nested @()
# literal as an operation on the ARRAY, not on its elements, and fails with op_Addition.
$sx0 = $x0 + 6; $sx1 = $x1 - 6; $sy0 = $y0 + 6; $sy1 = $y1 - 6
$pts = @(,@($sx0,$sy0)); $pts += ,@($sx1,$sy0); $pts += ,@($sx0,$sy1); $pts += ,@($sx1,$sy1)
$corner = @()
foreach ($p in $pts) {
    $corner += (Luma $src.GetPixel($p[0], $p[1]))
}
$card = ($corner | Measure-Object -Average).Average
$floor = 255.0
for ($y = $y0; $y -lt $y1; $y += 2) {
    for ($x = $x0; $x -lt $x1; $x += 2) {
        $l = Luma $src.GetPixel($x, $y)
        if ($l -lt $floor) { $floor = $l }
    }
}
$thr = ($card + $floor) / 2.0
Write-Host ("  card {0:N1}, figure floor {1:N1}, threshold {2:N1}" -f $card, $floor, $thr)

# Tight-crop the figure inside the inset area.
$mnX = 999999; $mxX = -1; $mnY = 999999; $mxY = -1
for ($y = $y0; $y -lt $y1; $y++) {
    for ($x = $x0; $x -lt $x1; $x++) {
        if ((Luma $src.GetPixel($x, $y)) -ge $thr) { continue }
        if ($x -lt $mnX) { $mnX = $x }; if ($x -gt $mxX) { $mxX = $x }
        if ($y -lt $mnY) { $mnY = $y }; if ($y -gt $mxY) { $mxY = $y }
    }
}
if ($mxX -lt 0) { throw "no figure found inside the inset area - the inset or threshold is wrong" }
$fw = $mxX - $mnX + 1; $fh = $mxY - $mnY + 1
Write-Host ("  figure at sheet ({0},{1}) {2}x{3}" -f $mnX, $mnY, $fw, $fh)

# Scale to the height the inventory's equipment area gives it.
$TARGET_H = 370
$scale = $TARGET_H / [double]$fh
$ow = [int][Math]::Round($fw * $scale); $oh = $TARGET_H

# Key at source resolution first, then scale the MASK - scaling the sheet directly would blend
# card grey into the figure's edge and thicken it.
$mask = New-Object System.Drawing.Bitmap $fw, $fh, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)

# The figure's own shading is preserved, not flattened. The first version filled every kept pixel
# with one flat colour, which threw away the armour, cloak and shield modelling the source actually
# has - it came out as a featureless blob. Alpha still has to be binary (hud_art's QuantizeAsset
# thresholds at a<128), so the detail is carried in the COLOUR instead: the source luminance is
# remapped across a narrow dark band, so darker parts of the figure stay darker and the modelling
# survives.
#
# The band is dark on purpose. This is blitted through paletteTransparencyLookup, which darkens
# what is behind it, so a lighter band would wash the silhouette out against the panel.
#
# Raised on request to make the figure read more clearly. This blit AVERAGES with the panel behind
# it, so the band's position relative to the panel's own tone is what decides the effect: below it
# the figure darkens, above it the figure lifts. The old 12..74 sat entirely below, giving a shadow
# so faint it barely registered. 22..112 straddles the panel, so the shadowed parts still darken
# while the lit edges - pauldrons, shield boss, mace head - now come up out of it.
$DARKEST = 22    # deepest shadow in the figure
$LIGHTEST = 112  # its brightest lit edge
$kept = 0
for ($y = 0; $y -lt $fh; $y++) {
    for ($x = 0; $x -lt $fw; $x++) {
        $l = Luma $src.GetPixel($mnX + $x, $mnY + $y)
        if ($l -ge $thr) { $mask.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(0,0,0,0)); continue }
        # 0 at the figure's darkest, 1 at the keying threshold.
        $t = ($l - $floor) / [Math]::Max(1.0, ($thr - $floor))
        $v = [int][Math]::Round($DARKEST + $t * ($LIGHTEST - $DARKEST))
        if ($v -lt 0) { $v = 0 } elseif ($v -gt 255) { $v = 255 }
        # Very slightly warm, so it sits with the gold bevel rather than reading as flat grey.
        $r2 = [int][Math]::Min(255, $v * 1.10)
        $b2 = [int][Math]::Max(0, $v * 0.88)
        $mask.SetPixel($x, $y, [System.Drawing.Color]::FromArgb(255, $r2, $v, $b2))
        $kept++
    }
}
$src.Dispose()

# Despeckle: keep only the figure. The card's stone texture has a few dark flecks that fall under
# the threshold and survive as loose dots around the silhouette. Same connected-component filter
# the item-icon cutter uses for bleed - label every opaque region, keep those at least 2% of the
# largest, erase the rest. 2% is far below the figure (which is one region carrying essentially all
# the pixels) and far above a fleck.
$lab = New-Object 'int[,]' $fw, $fh
$sizes = @{}
$next = 1
for ($sy = 0; $sy -lt $fh; $sy++) {
    for ($sx = 0; $sx -lt $fw; $sx++) {
        if ($lab[$sx,$sy] -ne 0) { continue }
        if ($mask.GetPixel($sx,$sy).A -eq 0) { continue }
        $id = $next; $next++
        $n = 0
        $q = New-Object System.Collections.Generic.Queue[int]
        $q.Enqueue($sy*$fw+$sx); $lab[$sx,$sy] = $id
        while ($q.Count -gt 0) {
            $cur = $q.Dequeue(); $x = $cur % $fw; $y = [int][Math]::Floor($cur / $fw); $n++
            foreach ($d in @(@(1,0),@(-1,0),@(0,1),@(0,-1))) {
                $nx = $x + $d[0]; $ny = $y + $d[1]
                if ($nx -lt 0 -or $ny -lt 0 -or $nx -ge $fw -or $ny -ge $fh) { continue }
                if ($lab[$nx,$ny] -ne 0) { continue }
                if ($mask.GetPixel($nx,$ny).A -eq 0) { continue }
                $lab[$nx,$ny] = $id; $q.Enqueue($ny*$fw+$nx)
            }
        }
        $sizes[$id] = $n
    }
}
$largest = 0
foreach ($v in $sizes.Values) { if ($v -gt $largest) { $largest = $v } }
$cut = [int]($largest * 0.02)
$dropped = 0
for ($y = 0; $y -lt $fh; $y++) {
    for ($x = 0; $x -lt $fw; $x++) {
        $id = $lab[$x,$y]
        if ($id -eq 0) { continue }
        if ($sizes[$id] -lt $cut) { $mask.SetPixel($x,$y,[System.Drawing.Color]::FromArgb(0,0,0,0)); $dropped++ }
    }
}
Write-Host ("  despeckle: {0} regions, largest {1}px, dropped {2}px below {3}px" -f $sizes.Count, $largest, $dropped, $cut)

$out = New-Object System.Drawing.Bitmap $ow, $oh, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$g = [System.Drawing.Graphics]::FromImage($out)
$g.Clear([System.Drawing.Color]::FromArgb(0,0,0,0))
$g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
$g.DrawImage($mask, 0, 0, $ow, $oh)
$g.Dispose(); $mask.Dispose()

foreach ($d in @("Packaging\resources\oracool_assets\ui","Packaging\resources\assets\ui","build\x64-Debug\assets\ui")) {
    if (Test-Path $d) {
        $out.Save((Join-Path (Resolve-Path $d) "silhouette_$NAME.png"), [System.Drawing.Imaging.ImageFormat]::Png)
        Write-Host "  wrote $d\silhouette_$NAME.png"
    }
}
$out.Dispose()
Write-Host "done - ${ow}x${oh} ($kept opaque source px)"
