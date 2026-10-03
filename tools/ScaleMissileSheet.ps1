# ScaleMissileSheet.ps1 - shrinks (or grows) every frame of a PNG missile sheet about its cell's centre, so the missile
# keeps its place on the tile and only its size changes. The cell size stays, so misdat's row is untouched.
#
# First use (dev notes, 2026-10-03: "reduce size of magic arrow by 50%", "reduce size of guided arrow. to have size similar
# to other arrows"): magic_arrow.png at 50 and guided_arrow.png at 62, both ~38px long like frost_arrow.png.
#
# Usage (from the repo root):
#   powershell -NoProfile -File tools\ScaleMissileSheet.ps1 -Sheet Packaging\resources\oracool_assets\missiles\magic_arrow.png -Percent 50
# The sheet is rewritten in place; git holds the original.
param(
    [Parameter(Mandatory = $true)][string]$Sheet,
    [Parameter(Mandatory = $true)][int]$Percent,
    [int]$Cell = 96
)
Add-Type -AssemblyName System.Drawing

$path = (Resolve-Path $Sheet).Path
$src = [System.Drawing.Bitmap]::FromFile($path)
$copy = New-Object System.Drawing.Bitmap $src.Width, $src.Height, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$cg = [System.Drawing.Graphics]::FromImage($copy); $cg.DrawImage($src, 0, 0, $src.Width, $src.Height); $cg.Dispose(); $src.Dispose()

$out = New-Object System.Drawing.Bitmap $copy.Width, $copy.Height, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$g = [System.Drawing.Graphics]::FromImage($out)
$g.Clear([System.Drawing.Color]::FromArgb(0, 0, 0, 0))
$g.InterpolationMode = 'HighQualityBicubic'; $g.PixelOffsetMode = 'HighQuality'; $g.CompositingMode = 'SourceCopy'
$size = [int][Math]::Round($Cell * $Percent / 100.0)
$inset = [int][Math]::Floor(($Cell - $size) / 2)
$cols = [int]($copy.Width / $Cell); $rows = [int]($copy.Height / $Cell)
for ($r = 0; $r -lt $rows; $r++) {
    for ($c = 0; $c -lt $cols; $c++) {
        # Frame by frame, so no frame bleeds into its neighbour at the resample.
        $dst = New-Object System.Drawing.Rectangle ($c * $Cell + $inset), ($r * $Cell + $inset), $size, $size
        $from = New-Object System.Drawing.Rectangle ($c * $Cell), ($r * $Cell), $Cell, $Cell
        $g.DrawImage($copy, $dst, $from, [System.Drawing.GraphicsUnit]::Pixel)
    }
}
$g.Dispose(); $copy.Dispose()
$out.Save($path, [System.Drawing.Imaging.ImageFormat]::Png)
$out.Dispose()
Write-Host ("{0}: {1}x{2} frames of {3}px scaled to {4}%" -f $path, $cols, $rows, $Cell, $Percent)
