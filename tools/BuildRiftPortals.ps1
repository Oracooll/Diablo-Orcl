# BuildRiftPortals.ps1 - the rift portals are VANILLA's town-portal animation, recoloured (user,
# 2026-09-20: "for nephalem and guardian rifts i want to use vanilla portal animation ... recoloured
# according to the colors i specified earlier" - golden for Nephalem, purple for Guardian).
#
# Reads the exported strip Resources/01. Blizzard Assets/Animated Items 2/portal.png (16 frames of 96x256,
# side by side - the layout oracool::LoadPngMissileSheet reads) and writes
#   Packaging/resources/oracool_assets/missiles/portal_gold.png
#   Packaging/resources/oracool_assets/missiles/portal_purple.png
# Every coloured pixel keeps its lightness and alpha and takes the target hue; grey, white and black
# pixels (the core's flash, the rim's shadow) are left alone, so the animation's shape and timing are
# vanilla's to the frame. The palette has no purple ramp, which is why this is done in RGB on the
# 32-bit renderer and never as a .trn.
#
# Usage: powershell -NoProfile -File tools\BuildRiftPortals.ps1   (from the repo root)
param(
    [string]$Source = "..\Resources\01. Blizzard Assets\Animated Items 2\portal.png",
    [string]$RedSource = "..\Resources\01. Blizzard Assets\Animated Items 2\rportal.png",
    [string]$Cl2Frames = "..\Resources\01. Blizzard Assets\Animated Items 2\portal_cl2_frames.png",
    [string]$OutDir = "Packaging\resources\oracool_assets\missiles",
    # 90 since 2026-09-20 (user: "portal asset behind rift monument - scale down to 90%"): each 96x128 frame
    # is resampled to 86x115 after the recolour and fill, so misdat's rows read animWidth 86 and
    # animWidth2 11 (which keeps the oval centred where the 96-wide frame had it).
    [int]$ScalePercent = 90
)
Add-Type -AssemblyName System.Drawing

# $hue -1 leaves the colours as they are (the town's own portal, 2026-09-20); $stripShadow drops the
# ground shadow under the oval's foot - the desaturated blue-grey pixels (25,30,45 / 88,99,141 /
# 67,76,111 / 78,88,125 / 13,17,27) in the frame's bottom band, y >= 90 of 128; the ring's own blues
# and its near-black rim shading are not touched (user: "remove its shadow").
# The shadow vanilla's blue portal draws along the oval's foot: its desaturated blue-grey near-blacks in the frame's
# bottom band (y >= 90 of 128) - 25,30,45 / 13,17,27 / 67,76,111 / 78,88,125 / 88,99,141.
function IsPortalShadow([int]$py, [int]$r8, [int]$g8, [int]$b8) {
    return ($py -ge 90 -and [Math]::Abs($r8 - $g8) -lt 14 -and $b8 -ge $r8 + 8 -and $b8 -lt $r8 + 60 -and $g8 -ge 10)
}

# THE GROUND SHADOW (dev note, 2026-10-03: "blue vanilla town portals have black shadow. copy this shadow to all other
# portals in the game incl. rifts portals"). The CL2 draws a solid black shadow on the floor at the oval's lower left; the
# exported strip ($Source) lost it - the exporter wrote its palette index as clear - so every sheet built from the strip had
# none. $Cl2Frames is the CL2 as the game draws it (OracoolPreview.DISABLED_Dev370Looks writes portal_cl2_frames.png:
# both rows in 96x128 cells, magenta for clear), one pixel left of the strip. The shadow is its pure black pixels that lie
# OUTSIDE the strip's own pixels on their row. Returns a byte per strip pixel, 1 where the shadow is.
function ReadGroundShadow([string]$cl2Path, [int]$width, [int]$height, [byte[]]$stripBytes, [int]$stride) {
    $c = [System.Drawing.Bitmap]::FromFile((Resolve-Path $cl2Path))
    $mask = New-Object byte[] ($width * $height)
    $count = 0
    for ($y = 0; $y -lt $height; $y++) {
        for ($f = 0; $f -lt 16; $f++) {
            $left = -1; $right = -1
            for ($x = 0; $x -lt 96; $x++) { if ($stripBytes[$y * $stride + ($f * 96 + $x) * 4 + 3] -ne 0) { if ($left -lt 0) { $left = $x }; $right = $x } }
            for ($x = 0; $x -lt 95; $x++) {
                $p = $c.GetPixel($f * 96 + $x, $y)
                if ($p.R -ne 0 -or $p.G -ne 0 -or $p.B -ne 0) { continue }
                $sx = $x + 1 # the strip sits a pixel right of the CL2's draw
                if ($left -ge 0 -and $sx -ge $left -and $sx -le $right) { continue } # the oval's own black centre
                $mask[$y * $width + $f * 96 + $sx] = 1
                $count++
            }
        }
    }
    $c.Dispose()
    Write-Host ("  ground shadow: {0} pixels from {1}" -f $count, $cl2Path)
    return , $mask
}

# Lays the ground shadow onto a strip-sized sheet's bytes, opaque black - after the centre fill, so the fill's row
# enclosure never reaches out to it.
function ApplyGroundShadow([byte[]]$bytes, [int]$stride, [int]$width, [int]$height, [byte[]]$mask) {
    for ($y = 0; $y -lt $height; $y++) {
        for ($x = 0; $x -lt $width; $x++) {
            if ($mask[$y * $width + $x] -eq 0) { continue }
            $i = $y * $stride + $x * 4
            if ($bytes[$i + 3] -ne 0) { continue }
            $bytes[$i] = 0; $bytes[$i + 1] = 0; $bytes[$i + 2] = 0; $bytes[$i + 3] = 255
        }
    }
}

# The red portal (dev note, 2026-10-03): vanilla's rportal is the blue portal's animation recoloured red, pixel for
# pixel, so its shadow is red too. This writes rportal.png - the red sheet with every pixel that is shadow in the BLUE
# sheet taken from the blue sheet, and the oval's hollow centre filled opaque black as the CL2 draws it - at full size,
# 96x128 frames, which the engine loads in place of the CL2 (misdat.cpp: a PNG beside a missile's name wins).
function CopyShadowOntoRed([string]$bluePath, [string]$redPath, [string]$outPath) {
    $load = {
        param($p)
        $s = [System.Drawing.Bitmap]::FromFile((Resolve-Path $p))
        $b = New-Object System.Drawing.Bitmap $s.Width, $s.Height, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $gg = [System.Drawing.Graphics]::FromImage($b); $gg.DrawImage($s, 0, 0, $s.Width, $s.Height); $gg.Dispose(); $s.Dispose()
        return $b
    }
    $blue = & $load $bluePath
    $red = & $load $redPath
    $rect = New-Object System.Drawing.Rectangle 0, 0, $red.Width, $red.Height
    $bd = $blue.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $rd = $red.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadWrite, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $n = $rd.Stride * $red.Height
    $bb = New-Object byte[] $n; $rb = New-Object byte[] $n
    [System.Runtime.InteropServices.Marshal]::Copy($bd.Scan0, $bb, 0, $n)
    [System.Runtime.InteropServices.Marshal]::Copy($rd.Scan0, $rb, 0, $n)
    $copied = 0
    for ($i = 0; $i -lt $n; $i += 4) {
        if ($bb[$i + 3] -eq 0) { continue }
        $py = [int](($i / $rd.Stride) % 128)
        if (IsPortalShadow $py $bb[$i + 2] $bb[$i + 1] $bb[$i]) {
            $rb[$i] = $bb[$i]; $rb[$i + 1] = $bb[$i + 1]; $rb[$i + 2] = $bb[$i + 2]; $rb[$i + 3] = $bb[$i + 3]
            $copied++
        }
    }
    # The hollow centre, black, as HueShift fills the town sheet's.
    $stride = $rd.Stride
    for ($y = 0; $y -lt $red.Height; $y++) {
        for ($f = 0; $f -lt 16; $f++) {
            $x0 = $f * 96; $x1 = $x0 + 95
            $left = -1; $right = -1
            for ($x = $x0; $x -le $x1; $x++) { if ($rb[$y * $stride + $x * 4 + 3] -ne 0) { if ($left -lt 0) { $left = $x }; $right = $x } }
            if ($left -lt 0 -or $right - $left -lt 4) { continue }
            for ($x = $left + 1; $x -lt $right; $x++) {
                $j = $y * $stride + $x * 4
                if ($rb[$j + 3] -ne 0) { continue }
                $rb[$j] = 0; $rb[$j + 1] = 0; $rb[$j + 2] = 0; $rb[$j + 3] = 255
            }
        }
    }
    ApplyGroundShadow $rb $stride $red.Width $red.Height $script:GroundShadowMask
    [System.Runtime.InteropServices.Marshal]::Copy($rb, 0, $rd.Scan0, $n)
    $blue.UnlockBits($bd); $red.UnlockBits($rd); $blue.Dispose()
    $red.Save($outPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $red.Dispose()
    Write-Host ("{0} <- {1} with {2} shadow pixels from {3}" -f $outPath, $redPath, $copied, $bluePath)
}

function HueShift([string]$inPath, [string]$outPath, [double]$hue, [double]$satBoost, [double]$lightGain, [int[]]$fillRgb, [bool]$stripShadow = $false, [bool]$keepShadow = $false) {
    $src = [System.Drawing.Bitmap]::FromFile((Resolve-Path $inPath))
    $bmp = New-Object System.Drawing.Bitmap $src.Width, $src.Height, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp); $g.DrawImage($src, 0, 0, $src.Width, $src.Height); $g.Dispose(); $src.Dispose()
    $rect = New-Object System.Drawing.Rectangle 0, 0, $bmp.Width, $bmp.Height
    $data = $bmp.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadWrite, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $n = $data.Stride * $bmp.Height
    $bytes = New-Object byte[] $n
    [System.Runtime.InteropServices.Marshal]::Copy($data.Scan0, $bytes, 0, $n)
    $h = $hue / 360.0
    for ($i = 0; $i -lt $n; $i += 4) {
        $a = $bytes[$i + 3]
        if ($a -eq 0) { continue }
        if ($stripShadow -or $keepShadow) {
            $py = [int](($i / $data.Stride) % 128)
            $r8 = [int]$bytes[$i + 2]; $g8 = [int]$bytes[$i + 1]; $b8 = [int]$bytes[$i]
            if (IsPortalShadow $py $r8 $g8 $b8) {
                # $keepShadow: vanilla's own shadow, pixel for pixel - not hue-shifted, not greyed (dev note, 2026-10-03:
                # "blue vanilla town portals have black shadow. copy this shadow to all other portals in the game incl. rifts
                # portals"; v1.12.271's gold portal had it at half brightness in grey).
                if ($stripShadow) { $bytes[$i + 3] = 0; $bytes[$i] = 0; $bytes[$i + 1] = 0; $bytes[$i + 2] = 0 }
                continue
            }
        }
        if ($hue -lt 0) { continue } # the colours as painted
        $b = $bytes[$i] / 255.0; $gch = $bytes[$i + 1] / 255.0; $r = $bytes[$i + 2] / 255.0
        $max = [Math]::Max($r, [Math]::Max($gch, $b)); $min = [Math]::Min($r, [Math]::Min($gch, $b))
        $l = ($max + $min) / 2.0
        $d = $max - $min
        if ($d -lt 0.08) { continue } # grey, white, black: untouched
        $s = if ($l -gt 0.5) { $d / (2.0 - $max - $min) } else { $d / ($max + $min) }
        $s = [Math]::Min(1.0, $s * $satBoost)
        $l = [Math]::Min(1.0, $l * $lightGain)
        # HSL -> RGB at the new hue
        $q = if ($l -lt 0.5) { $l * (1 + $s) } else { $l + $s - $l * $s }
        $p = 2 * $l - $q
        $rgb = @(0.0, 0.0, 0.0)
        $ts = @(($h + 1.0 / 3.0), $h, ($h - 1.0 / 3.0))
        for ($k = 0; $k -lt 3; $k++) {
            $t = $ts[$k]
            if ($t -lt 0) { $t += 1 }; if ($t -gt 1) { $t -= 1 }
            if ($t -lt 1.0 / 6.0) { $rgb[$k] = $p + ($q - $p) * 6 * $t }
            elseif ($t -lt 0.5) { $rgb[$k] = $q }
            elseif ($t -lt 2.0 / 3.0) { $rgb[$k] = $p + ($q - $p) * (2.0 / 3.0 - $t) * 6 }
            else { $rgb[$k] = $p }
        }
        $bytes[$i + 2] = [byte][Math]::Round($rgb[0] * 255)
        $bytes[$i + 1] = [byte][Math]::Round($rgb[1] * 255)
        $bytes[$i] = [byte][Math]::Round($rgb[2] * 255)
    }
    # The centre (user, 2026-09-20: "fill the gold/purple portals center with gold/purple similar to
    # the vanilla blue portal"). The exported strip's oval is hollow - its interior pixels are index 0,
    # which the exporter writes as transparent - so every transparent pixel enclosed between the
    # ring's opaque pixels on its row is painted the kind's own dark tone, frame by frame; the thin
    # frames of the opening blossom enclose nothing and stay as they are.
    if ($fillRgb -ne $null) {
        $stride = $data.Stride
        $width = $bmp.Width
        for ($y = 0; $y -lt $bmp.Height; $y++) {
            $frameH = 128
            for ($f = 0; $f -lt 16; $f++) {
                $x0 = $f * 96; $x1 = $x0 + 95
                $left = -1; $right = -1
                for ($x = $x0; $x -le $x1; $x++) { if ($bytes[$y * $stride + $x * 4 + 3] -ne 0) { if ($left -lt 0) { $left = $x }; $right = $x } }
                if ($left -lt 0 -or $right - $left -lt 4) { continue }
                for ($x = $left + 1; $x -lt $right; $x++) {
                    $i = $y * $stride + $x * 4
                    if ($bytes[$i + 3] -ne 0) { continue }
                    $bytes[$i] = [byte]$fillRgb[2]; $bytes[$i + 1] = [byte]$fillRgb[1]; $bytes[$i + 2] = [byte]$fillRgb[0]; $bytes[$i + 3] = 255
                }
            }
        }
    }
    if (-not $stripShadow) { ApplyGroundShadow $bytes $data.Stride $bmp.Width $bmp.Height $script:GroundShadowMask }
    [System.Runtime.InteropServices.Marshal]::Copy($bytes, 0, $data.Scan0, $n)
    $bmp.UnlockBits($data)
    if ($ScalePercent -ne 100) {
        $fw = [int][Math]::Floor(96 * $ScalePercent / 100); $fh = [int][Math]::Floor(128 * $ScalePercent / 100)
        $rows = [int]($bmp.Height / 128)
        $small = New-Object System.Drawing.Bitmap (16 * $fw), ($rows * $fh), ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $sg = [System.Drawing.Graphics]::FromImage($small)
        $sg.Clear([System.Drawing.Color]::FromArgb(0, 0, 0, 0))
        $sg.InterpolationMode = 'HighQualityBicubic'; $sg.PixelOffsetMode = 'HighQuality'; $sg.CompositingMode = 'SourceCopy'
        # Frame by frame, so no frame bleeds into its neighbour at the resample.
        for ($row = 0; $row -lt $rows; $row++) {
            for ($f = 0; $f -lt 16; $f++) {
                $sg.DrawImage($bmp, (New-Object System.Drawing.Rectangle ($f * $fw), ($row * $fh), $fw, $fh), (New-Object System.Drawing.Rectangle ($f * 96), ($row * 128), 96, 128), [System.Drawing.GraphicsUnit]::Pixel)
            }
        }
        $sg.Dispose()
        $bmp.Dispose()
        $bmp = $small
        Write-Host ("  frames resampled to {0}x{1} ({2}%)" -f $fw, $fh, $ScalePercent)
    }
    $bmp.Save($outPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    Write-Host ("{0} <- {1} at hue {2}" -f $outPath, $inPath, $hue)
}

$out = (Resolve-Path $OutDir).Path
# The ground shadow, read once against the unmodified strip.
$strip = [System.Drawing.Bitmap]::FromFile((Resolve-Path $Source))
$stripArgb = New-Object System.Drawing.Bitmap $strip.Width, $strip.Height, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$sg0 = [System.Drawing.Graphics]::FromImage($stripArgb); $sg0.DrawImage($strip, 0, 0, $strip.Width, $strip.Height); $sg0.Dispose(); $strip.Dispose()
$sd = $stripArgb.LockBits((New-Object System.Drawing.Rectangle 0, 0, $stripArgb.Width, $stripArgb.Height), [System.Drawing.Imaging.ImageLockMode]::ReadOnly, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$stripBytes = New-Object byte[] ($sd.Stride * $stripArgb.Height)
[System.Runtime.InteropServices.Marshal]::Copy($sd.Scan0, $stripBytes, 0, $stripBytes.Length)
$script:GroundShadowMask = ReadGroundShadow $Cl2Frames $stripArgb.Width $stripArgb.Height $stripBytes $sd.Stride
$stripArgb.UnlockBits($sd); $stripArgb.Dispose()
# Gold: the yellow-orange of the game's gold text; a touch more saturation so the flame reads as metal, not straw.
# The core a very dark gold (dev note, 2026-09-30: "very dark gold core to match the darker tones in its animation";
# was 96,66,8), and the rim's shadow kept as painted.
HueShift $Source (Join-Path $out "portal_gold.png") 42 1.25 1.05 @(40, 27, 3) $false $true
# Purple: NOT hue-shifted here any more (2026-09-20). The sheet is quantised to the palette on load and
# the palette has no violet ramp, so the shifted sheet came out blue in the game. It stays vanilla's
# BLUE (the PAL8_BLUE ramp) with a dark blue centre fill, and the game draws the Guardian portal through
# oracool::GuardianPortalRgbTable, which sends that ramp to violet values (rift.cpp).
HueShift $Source (Join-Path $out "portal_purple.png") -1 1.0 1.0 @(0, 0, 60)
# The TOWN's own portal (user, 2026-09-20: "shrink the town portal asset in-town only, not in dungeons
# to 90% and remove its shadow"): vanilla's colours, the ground shadow stripped, 90% like the rift
# portals, and the oval's interior filled BLACK - the exported strip's hollow centre is index 0, which the
# CL2 draws as opaque black but the PNG loader reads as transparent (user, 2026-09-20: "the main blue town
# portal asset lost its black center ... Now there is a transparent hole in the center"); opaque black
# quantises to a near-black index above 128, never to the transparent 0. MissileGraphicID::TownPortalInTown;
# the dungeon-side portal keeps the CL2.
# The shadow was back on 2026-10-03 (dev note: "copy this shadow to all other portals in the game incl. rifts portals"),
# and is gone again since 2026-10-04 (RTM dev note: "dont cast portal shadows over monuments - we have two in town, so - no
# portal shadows in town"): this sheet is only ever worn in town.
HueShift $Source (Join-Path $out "portal_town.png") -1 1.0 1.0 @(0, 0, 0) $true $false
# The Rift Monument's two portals as they stand in town, shadowless for the same note (MissileGraphicID::RiftPortalGoldInTown,
# RiftPortalPurpleInTown); a rift's own exit keeps the shadowed sheets above.
HueShift $Source (Join-Path $out "portal_gold_town.png") 42 1.25 1.05 @(40, 27, 3) $true $false
HueShift $Source (Join-Path $out "portal_purple_town.png") -1 1.0 1.0 @(0, 0, 60) $true $false
# The red portal (Lazarus's, the quest levels'): vanilla's red sheet with the blue portal's shadow (2026-10-03).
CopyShadowOntoRed $Source $RedSource (Join-Path $out "rportal.png")
