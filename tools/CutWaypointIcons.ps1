# CutWaypointIcons.ps1
#
# Oracool: cuts ui\waypoint_icons.png - the waypoint list's per-row pad - from the master painting
# in the art vault.
#
# User request (2026-08-14): "delete the existing 30x30 asset and make new one 43x43 from the master
# asset", and "i want shrunk down detailed better than enlarged small res asset." So this always
# reduces from the 1536x1024 master; nothing is ever scaled up from the old sheet.
#
# The master holds two states side by side, each 768x1024: the dormant stone sigil on the left, the
# lit blue one on the right. Its background is already transparent - what looks like black around the
# sigil in a viewer is the alpha channel rendered on black - so the cut keys off ALPHA and there is
# no colour to chroma-key.
#
# THE ONE THING THIS SCRIPT EXISTS TO GET RIGHT: both cells are cut with the SAME crop rectangle.
# The lit state's glow reaches roughly 110px further up and down than the dormant stone does
# (measured: dormant y 116..861, lit y 73..927), so measuring each state's own bounding box would
# scale them differently and the pad would visibly jump in size the moment a waypoint was activated.
# The dormant stone is the reference; the lit state gets the identical rect and its glow is clipped
# at the cell edge, which is what "the sigil's diameter is tangent to the row borders" requires.
#
# Run from the repo root:
#     powershell -ExecutionPolicy Bypass -File tools\CutWaypointIcons.ps1

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$repoRoot = Split-Path -Parent $PSScriptRoot
# The vault sits beside the repo root, not inside it.
$master = Join-Path (Split-Path -Parent $repoRoot) 'Oracool.MPQ\02-source-art\hud-icons\waypoint-sigil-topdown-2-states.png'
if (-not (Test-Path $master)) { throw "master art not found: $master" }

# One row is 43px tall and the pad is tangent to its top and bottom borders, so the cell is 43x43.
# hud_art.cpp derives the cell width as sheetWidth/2 and the height as the sheet height, so shipping
# an 86x43 sheet is all that is needed to change the on-screen size - no code change.
$CellSize = 43

$destinations = @(
    (Join-Path $repoRoot 'Packaging\resources\assets\ui\waypoint_icons.png'),
    (Join-Path $repoRoot 'Packaging\resources\oracool_assets\ui\waypoint_icons.png')
)

$src = [System.Drawing.Bitmap]::FromFile($master)
try {
    $w = [int]$src.Width
    $h = [int]$src.Height
    if ($w % 2 -ne 0) { throw "master width $w is not two equal states" }
    $halfWidth = [int]($w / 2)

    $rect = New-Object System.Drawing.Rectangle 0, 0, $w, $h
    $data = $src.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::ReadOnly, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $buf = New-Object byte[] ($data.Stride * $h)
    [System.Runtime.InteropServices.Marshal]::Copy($data.Scan0, $buf, 0, $buf.Length)
    $stride = [int]$data.Stride
    $src.UnlockBits($data)

    # Measured over the dormant half only, and stopping short of the midline: the lit sigil's glow
    # bleeds a little across x=768 (its columns there still carry alpha), and letting that into the
    # dormant measurement would drag the crop sideways.
    $scanRight = $halfWidth - 13
    $minX = [int]::MaxValue; $maxX = -1; $minY = [int]::MaxValue; $maxY = -1
    for ($y = 0; $y -lt $h; $y++) {
        $row = $y * $stride
        for ($x = 0; $x -le $scanRight; $x++) {
            if ($buf[$row + $x * 4 + 3] -ge 128) {
                if ($x -lt $minX) { $minX = $x }
                if ($x -gt $maxX) { $maxX = $x }
                if ($y -lt $minY) { $minY = $y }
                if ($y -gt $maxY) { $maxY = $y }
            }
        }
    }
    if ($maxX -lt 0) { throw "no opaque pixels found in the dormant half" }

    $boxW = $maxX - $minX + 1
    $boxH = $maxY - $minY + 1
    Write-Host ("dormant stone: x {0}..{1} (w={2})  y {3}..{4} (h={5})" -f $minX, $maxX, $boxW, $minY, $maxY, $boxH)

    # A square around the sigil, centred on the stone, so the round pad is not squashed on either
    # axis. Side = the larger of the two extents, which is what makes the wider axis tangent.
    $side = [Math]::Max($boxW, $boxH)
    $centreX = ($minX + $maxX) / 2.0
    $centreY = ($minY + $maxY) / 2.0
    $cropX = [int][Math]::Round($centreX - $side / 2.0)
    $cropY = [int][Math]::Round($centreY - $side / 2.0)
    Write-Host ("shared crop: {0}x{0} at ({1},{2}) of each half  ->  {3}x{3} per cell" -f $side, $cropX, $cropY, $CellSize)

    # Premultiplied destination: straight-alpha bicubic reduction blends the RGB of fully
    # transparent pixels into the edge and leaves a dark fringe around the pad.
    $sheet = New-Object System.Drawing.Bitmap ($CellSize * 2), $CellSize, ([System.Drawing.Imaging.PixelFormat]::Format32bppPArgb)
    try {
        $g = [System.Drawing.Graphics]::FromImage($sheet)
        try {
            $g.Clear([System.Drawing.Color]::Transparent)
            $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
            $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
            $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::HighQuality
            $g.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality

            for ($state = 0; $state -lt 2; $state++) {
                $srcRect = New-Object System.Drawing.Rectangle ($cropX + $state * $halfWidth), $cropY, $side, $side
                $dstRect = New-Object System.Drawing.Rectangle ($state * $CellSize), 0, $CellSize, $CellSize
                $g.DrawImage($src, $dstRect, $srcRect, [System.Drawing.GraphicsUnit]::Pixel)
            }
        } finally { $g.Dispose() }

        # Back to straight ARGB for the PNG so the engine's loader sees the same format as before.
        $out = New-Object System.Drawing.Bitmap ($CellSize * 2), $CellSize, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        try {
            $g2 = [System.Drawing.Graphics]::FromImage($out)
            try {
                $g2.Clear([System.Drawing.Color]::Transparent)
                $g2.DrawImage($sheet, 0, 0)
            } finally { $g2.Dispose() }

            foreach ($dest in $destinations) {
                $dir = Split-Path -Parent $dest
                if (-not (Test-Path $dir)) { throw "asset folder missing: $dir" }
                if (Test-Path $dest) { Remove-Item -LiteralPath $dest -Force }
                $out.Save($dest, [System.Drawing.Imaging.ImageFormat]::Png)
                Write-Host ("wrote {0}  ({1}x{2})" -f $dest, $out.Width, $out.Height)
            }
        } finally { $out.Dispose() }
    } finally { $sheet.Dispose() }
} finally { $src.Dispose() }
