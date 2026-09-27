# CleanCubeLabel.ps1 - paints out the bright "LEVSKI" label ChatGPT baked under the cube in every frame
# of batch 43a (user, 2026-09-20: "remove the levski text in the red box. leave the pure asset"). The
# carving on the plinth's front face stays. Writes the cleaned frames to 02. Oracooll Assets\01. Used, leaving the
# delivered pack untouched; tools\build_levski_cube_cel.cmd packs them.
#
# The label sits on the cube's dark lower band, rows 105-112 of the 96x160 frame, x 38-64: light grey
# pixels (luminance > 60, saturation < 60) there are the letters. Each is replaced by the mean of the
# band's own dark pixels on the same row, so the band reads as it does beside the letters.
param(
    [string]$Source = "..\Resources\02. Oracooll Assets\delivered-packs\batch-43-levskis-cube\object",
    [string]$OutDir = "..\Resources\02. Oracooll Assets\01. Used\levski-cube"
)
Add-Type -AssemblyName System.Drawing
$ErrorActionPreference = "Stop"
New-Item -ItemType Directory -Force $OutDir | Out-Null
$x0 = 38; $x1 = 64; $y0 = 105; $y1 = 112
foreach ($file in Get-ChildItem (Join-Path $Source "cube_*.png") | Where-Object { $_.Name -notlike "*preview*" }) {
    $src = [System.Drawing.Bitmap]::FromFile($file.FullName)
    $bmp = New-Object System.Drawing.Bitmap $src.Width, $src.Height, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp); $g.DrawImage($src, 0, 0, $src.Width, $src.Height); $g.Dispose(); $src.Dispose()
    $changed = 0
    for ($y = $y0; $y -le $y1; $y++) {
        # The row's dark band colour: the mean of the non-letter, opaque pixels in the window.
        $r = 0; $gg = 0; $b = 0; $n = 0
        for ($x = $x0; $x -le $x1; $x++) {
            $c = $bmp.GetPixel($x, $y)
            if ($c.A -eq 0) { continue }
            $l = 0.3 * $c.R + 0.59 * $c.G + 0.11 * $c.B
            $sat = [Math]::Max($c.R, [Math]::Max($c.G, $c.B)) - [Math]::Min($c.R, [Math]::Min($c.G, $c.B))
            if ($l -gt 60 -and $sat -lt 60) { continue }
            $r += $c.R; $gg += $c.G; $b += $c.B; $n++
        }
        if ($n -eq 0) { continue }
        $fill = [System.Drawing.Color]::FromArgb(255, [int]($r / $n), [int]($gg / $n), [int]($b / $n))
        for ($x = $x0; $x -le $x1; $x++) {
            $c = $bmp.GetPixel($x, $y)
            if ($c.A -eq 0) { continue }
            $l = 0.3 * $c.R + 0.59 * $c.G + 0.11 * $c.B
            $sat = [Math]::Max($c.R, [Math]::Max($c.G, $c.B)) - [Math]::Min($c.R, [Math]::Min($c.G, $c.B))
            if ($l -gt 60 -and $sat -lt 60) { $bmp.SetPixel($x, $y, $fill); $changed++ }
        }
    }
    $bmp.Save((Join-Path $OutDir $file.Name), [System.Drawing.Imaging.ImageFormat]::Png)
    $bmp.Dispose()
    Write-Host ("{0}: {1} label pixels painted over" -f $file.Name, $changed)
}
