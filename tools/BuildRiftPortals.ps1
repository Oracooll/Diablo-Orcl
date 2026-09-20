# BuildRiftPortals.ps1 - the rift portals are VANILLA's town-portal animation, recoloured (user,
# 2026-09-20: "for nephalem and guardian rifts i want to use vanilla portal animation ... recoloured
# according to the colors i specified earlier" - golden for Nephalem, purple for Guardian).
#
# Reads the exported strip Resources/00-original-game-art/missiles/portal.png (16 frames of 96x256,
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
    [string]$Source = "..\Resources\00-original-game-art\missiles\portal.png",
    [string]$OutDir = "Packaging\resources\oracool_assets\missiles"
)
Add-Type -AssemblyName System.Drawing

function HueShift([string]$inPath, [string]$outPath, [double]$hue, [double]$satBoost, [double]$lightGain) {
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
    [System.Runtime.InteropServices.Marshal]::Copy($bytes, 0, $data.Scan0, $n)
    $bmp.UnlockBits($data)
    $bmp.Save($outPath, [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    Write-Host ("{0} <- {1} at hue {2}" -f $outPath, $inPath, $hue)
}

$out = (Resolve-Path $OutDir).Path
# Gold: the yellow-orange of the game's gold text; a touch more saturation so the flame reads as metal, not straw.
HueShift $Source (Join-Path $out "portal_gold.png") 42 1.25 1.05
# Purple: violet, the blue's own lightness - the ramp the palette never had.
HueShift $Source (Join-Path $out "portal_purple.png") 278 1.15 1.0
