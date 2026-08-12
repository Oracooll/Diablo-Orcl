# Oracool asset pipeline: cuts the modular stone panel/frame kit out of the user's green-screen
# sheet into individual transparent PNGs under ui\.
#
# The sheet is a KIT SHEET, not a single asset: three labelled sections (1 background texture,
# 2 border elements, 3 assembled examples) plus "HOW TO BUILD" diagrams and a tiling demo. Only
# sections 1 and 2 are real assets - the examples and diagrams are documentation of how to
# combine them, and shipping those would just be shipping pictures of instructions.
#
# Element rects come from connected-component detection on the sheet, not from eyeballing:
# see the dev report. Two caveats recorded there and worth repeating here:
#   - The corners block yields FIVE components, not the four a "corners" label suggests, and one
#     of them (62x99) is visibly larger than its neighbours - it may be two shapes that touch.
#     They are therefore shipped as corner_1..corner_5 with NEUTRAL names rather than
#     top-left/top-right/etc: this script cannot verify an orientation, and a wrong orientation
#     label is worse than no label when the panel gets assembled later.
#   - Bars come in 3 horizontal and 5 vertical lengths; they are ornamental variants, not a
#     progression, so they keep index names too.
#
# Usage: powershell -ExecutionPolicy Bypass -File tools\CutPanelFrameKit.ps1
# Run from the repository root.
$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$src = "..\Oracool.MPQ\02-source-art\inventory\panel-frame-kit-modular-greenscreen.png"
$GreenCut = 25   # same green test as the item-icon pipeline

if (-not (Test-Path $src)) { throw "source sheet not found: $src" }
$bmp = [System.Drawing.Bitmap]::FromFile((Resolve-Path $src))
$w = $bmp.Width; $h = $bmp.Height
$data = $bmp.LockBits((New-Object System.Drawing.Rectangle(0,0,$w,$h)), [System.Drawing.Imaging.ImageLockMode]::ReadOnly, [System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
$stride = $data.Stride
$buf = New-Object byte[] ($stride * $h)
[System.Runtime.InteropServices.Marshal]::Copy($data.Scan0, $buf, 0, $buf.Length)
$bmp.UnlockBits($data); $bmp.Dispose()

$elements = @(
    @{ n="panel_frame_bg";       x=16;  y=53;  w=442; h=463 }   # section 1, the tileable fill
    @{ n="panel_frame_hbar_1";   x=493; y=89;  w=281; h=24  }
    @{ n="panel_frame_hbar_2";   x=492; y=134; w=282; h=27  }
    @{ n="panel_frame_hbar_3";   x=492; y=184; w=283; h=30  }
    @{ n="panel_frame_vbar_1";   x=496; y=270; w=24;  h=217 }
    @{ n="panel_frame_vbar_2";   x=562; y=271; w=24;  h=243 }
    @{ n="panel_frame_vbar_3";   x=626; y=270; w=24;  h=217 }
    @{ n="panel_frame_vbar_4";   x=690; y=271; w=24;  h=242 }
    @{ n="panel_frame_vbar_5";   x=746; y=270; w=24;  h=244 }
    @{ n="panel_frame_corner_1"; x=814; y=86;  w=62;  h=99  }
    @{ n="panel_frame_corner_2"; x=908; y=86;  w=52;  h=59  }
    @{ n="panel_frame_corner_3"; x=908; y=161; w=64;  h=56  }
    @{ n="panel_frame_corner_4"; x=814; y=212; w=65;  h=65  }
    @{ n="panel_frame_corner_5"; x=908; y=233; w=59;  h=52  }
    @{ n="panel_frame_tee_1";    x=826; y=337; w=50;  h=47  }
    @{ n="panel_frame_tee_2";    x=909; y=337; w=51;  h=47  }
    @{ n="panel_frame_cross_1";  x=819; y=439; w=65;  h=72  }
    @{ n="panel_frame_cross_2";  x=908; y=439; w=65;  h=72  }
)

$dirs = @("Packaging\resources\oracool_assets\ui", "Packaging\resources\assets\ui", "build\x64-Debug\assets\ui")
foreach ($d in $dirs) { if (Test-Path (Split-Path $d -Parent)) { New-Item -ItemType Directory -Force -Path $d | Out-Null } }

$written = 0
foreach ($e in $elements) {
    $img = New-Object System.Drawing.Bitmap $e.w, $e.h, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $kept = 0
    for ($yy = 0; $yy -lt $e.h; $yy++) {
        for ($xx = 0; $xx -lt $e.w; $xx++) {
            $i = ($e.y + $yy) * $stride + ($e.x + $xx) * 4
            $b = $buf[$i]; $g = $buf[$i+1]; $r = $buf[$i+2]
            if (($g - [Math]::Max($r, $b)) -ge $GreenCut) {
                $img.SetPixel($xx, $yy, [System.Drawing.Color]::FromArgb(0,0,0,0))
            } else {
                $img.SetPixel($xx, $yy, [System.Drawing.Color]::FromArgb(255, $r, $g, $b))
                $kept++
            }
        }
    }
    foreach ($d in $dirs) {
        if (Test-Path $d) { $img.Save((Join-Path (Resolve-Path $d) "$($e.n).png"), [System.Drawing.Imaging.ImageFormat]::Png) }
    }
    $img.Dispose()
    Write-Host ("  {0,-24} {1,4}x{2,-4} {3,7} opaque px" -f $e.n, $e.w, $e.h, $kept)
    $written++
}
Write-Host "wrote $written elements into: $($dirs -join ', ')"
