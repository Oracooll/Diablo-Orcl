# ScaleLevskiCube.ps1 - the user's own Levski's Cube painting (Resources\02. Oracooll Assets\01. Used\Levski's Cube.png, 1254x1254 with
# real alpha, 2026-09-20) scaled to the town object's 96-pixel width, bottom-centre anchored, into
# Resources\02. Oracooll Assets\01. Used\levski-cube-user\levski_cube.png for tools\build_levski_cube_cel.cmd.
# The content box (x 0..1244, y 35..1253) is cropped first so the frame is the painting and nothing else.
param(
    [string]$Source = "..\Resources\02. Oracooll Assets\01. Used\Levski's Cube.png",
    [string]$OutDir = "..\Resources\02. Oracooll Assets\01. Used\levski-cube-user",
    [int]$Width = 96
)
Add-Type -AssemblyName System.Drawing
$ErrorActionPreference = "Stop"
New-Item -ItemType Directory -Force $OutDir | Out-Null
$src = [System.Drawing.Bitmap]::FromFile((Resolve-Path $Source))
# Content box: the first and last rows and columns with any alpha.
$minx = $src.Width; $maxx = -1; $miny = $src.Height; $maxy = -1
for ($y = 0; $y -lt $src.Height; $y++) { for ($x = 0; $x -lt $src.Width; $x++) { if ($src.GetPixel($x, $y).A -gt 0) { if ($x -lt $minx) { $minx = $x }; if ($x -gt $maxx) { $maxx = $x }; if ($y -lt $miny) { $miny = $y }; if ($y -gt $maxy) { $maxy = $y } } } }
$cw = $maxx - $minx + 1; $ch = $maxy - $miny + 1
$h = [int][Math]::Round($ch * $Width / $cw)
$out = New-Object System.Drawing.Bitmap $Width, $h, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$g = [System.Drawing.Graphics]::FromImage($out)
$g.Clear([System.Drawing.Color]::FromArgb(0, 0, 0, 0))
$g.InterpolationMode = 'HighQualityBicubic'; $g.SmoothingMode = 'HighQuality'; $g.PixelOffsetMode = 'HighQuality'; $g.CompositingMode = 'SourceCopy'
$g.DrawImage($src, (New-Object System.Drawing.Rectangle 0, 0, $Width, $h), (New-Object System.Drawing.Rectangle $minx, $miny, $cw, $ch), [System.Drawing.GraphicsUnit]::Pixel)
$g.Dispose()
$out.Save((Join-Path (Resolve-Path $OutDir) "levski_cube.png"), [System.Drawing.Imaging.ImageFormat]::Png)
Write-Host ("levski_cube.png {0}x{1} from a {2}x{3} content box" -f $Width, $h, $cw, $ch)
$out.Dispose(); $src.Dispose()
