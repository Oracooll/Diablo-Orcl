# RfA-28 reference frames: crops chosen frames out of the exported class sheets, writes each at 1x and 6x
# (nearest neighbour, so every source pixel is a crisp 6x6 block), and lays each armour tier's four frames on
# one plate. A manifest records where every frame came from and where it sits, so a returned plate can be
# sampled back to source pixels (block centres).
param(
    [string]$Scratch,
    [string]$OutDir
)
Add-Type -AssemblyName System.Drawing
$ErrorActionPreference = 'Stop'
$Scale = 6
$Gap = 48

# Direction rows in the exported sheets: 0 S, 1 SW, 2 W, 3 NW, 4 N, 5 NE, 6 E, 7 SE.
$classes = @(
    @{ key = 'barbarian'; source = 'warrior'; widths = @{ stand = 96; walk = 96; attack = 128; magic = 96 };
       frames = @(
         @{ tag = 'stand';        weapon = 'axe';          anim = 'stand';  dir = 0; frame = 0 },
         @{ tag = 'walk';         weapon = 'axe';          anim = 'walk';   dir = 1; frame = 3 },
         @{ tag = 'attack';       weapon = 'axe';          anim = 'attack'; dir = 7; frame = 7 },
         @{ tag = 'shield-stand'; weapon = 'sword-shield'; anim = 'stand';  dir = 2; frame = 0 }) },
    @{ key = 'necromancer'; source = 'sorceror'; widths = @{ stand = 96; walk = 96; attack = 128; magic = 128 };
       frames = @(
         @{ tag = 'stand';        weapon = 'staff';        anim = 'stand';  dir = 0; frame = 0 },
         @{ tag = 'walk';         weapon = 'staff';        anim = 'walk';   dir = 1; frame = 3 },
         @{ tag = 'attack';       weapon = 'staff';        anim = 'attack'; dir = 7; frame = 7 },
         @{ tag = 'shield-stand'; weapon = 'mace-shield';  anim = 'stand';  dir = 7; frame = 0 }) }
)
$tiers = @('light', 'medium', 'heavy')

function Get-OpaqueBounds([System.Drawing.Bitmap]$bmp, [int]$x0, [int]$y0, [int]$w, [int]$h) {
    $minX = [int]::MaxValue; $minY = [int]::MaxValue; $maxX = -1; $maxY = -1
    for ($y = 0; $y -lt $h; $y++) {
        for ($x = 0; $x -lt $w; $x++) {
            if ($bmp.GetPixel($x0 + $x, $y0 + $y).A -ne 0) {
                if ($x -lt $minX) { $minX = $x }; if ($x -gt $maxX) { $maxX = $x }
                if ($y -lt $minY) { $minY = $y }; if ($y -gt $maxY) { $maxY = $y }
            }
        }
    }
    if ($maxX -lt 0) { throw "empty frame" }
    return @{ x = $minX; y = $minY; w = $maxX - $minX + 1; h = $maxY - $minY + 1 }
}

function Scale-Nearest([System.Drawing.Bitmap]$src, [int]$s) {
    $dst = New-Object System.Drawing.Bitmap ($src.Width * $s), ($src.Height * $s), ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    for ($y = 0; $y -lt $src.Height; $y++) {
        for ($x = 0; $x -lt $src.Width; $x++) {
            $c = $src.GetPixel($x, $y)
            for ($dy = 0; $dy -lt $s; $dy++) { for ($dx = 0; $dx -lt $s; $dx++) { $dst.SetPixel($x * $s + $dx, $y * $s + $dy, $c) } }
        }
    }
    return $dst
}

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$OneX = Join-Path $OutDir 'original-1x'
New-Item -ItemType Directory -Force -Path $OneX | Out-Null
$manifest = @()
foreach ($cls in $classes) {
    foreach ($tier in $tiers) {
        $crops = @()
        foreach ($f in $cls.frames) {
            $sheetPath = Join-Path $Scratch "$($cls.source)\$tier-$($f.weapon)-$($f.anim).png"
            if (-not (Test-Path $sheetPath)) { throw "missing sheet $sheetPath" }
            $sheet = [System.Drawing.Bitmap]::FromFile($sheetPath)
            $cellW = $cls.widths[$f.anim]
            $cellH = [int]($sheet.Height / 8)
            $x0 = $f.frame * $cellW; $y0 = $f.dir * $cellH
            $b = Get-OpaqueBounds $sheet $x0 $y0 $cellW $cellH
            $pad = 2
            $cx = [Math]::Max(0, $b.x - $pad); $cy = [Math]::Max(0, $b.y - $pad)
            $cw = [Math]::Min($cellW - $cx, $b.w + 2 * $pad); $ch = [Math]::Min($cellH - $cy, $b.h + 2 * $pad)
            $crop = New-Object System.Drawing.Bitmap $cw, $ch, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
            for ($y = 0; $y -lt $ch; $y++) { for ($x = 0; $x -lt $cw; $x++) { $crop.SetPixel($x, $y, $sheet.GetPixel($x0 + $cx + $x, $y0 + $cy + $y)) } }
            $sheet.Dispose()
            $name = "$($cls.key)-$tier-$($f.tag)"
            $crop.Save((Join-Path $OneX "$name.png"), [System.Drawing.Imaging.ImageFormat]::Png)
            $crops += @{ name = $name; bmp = $crop; f = $f; sheet = "$tier-$($f.weapon)-$($f.anim)"; cellX = $cx; cellY = $cy }
        }
        # The plate: the four frames side by side at 6x, bottoms aligned, on transparency.
        $plateW = $Gap; $plateH = 0
        foreach ($c in $crops) { $plateW += $c.bmp.Width * $Scale + $Gap; $plateH = [Math]::Max($plateH, $c.bmp.Height * $Scale) }
        $plateH += 2 * $Gap
        $plate = New-Object System.Drawing.Bitmap $plateW, $plateH, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        $g = [System.Drawing.Graphics]::FromImage($plate)
        $g.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::NearestNeighbor
        $g.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::Half
        $x = $Gap
        foreach ($c in $crops) {
            $big = Scale-Nearest $c.bmp $Scale
            $y = $plateH - $Gap - $big.Height
            $g.DrawImageUnscaled($big, $x, $y)
            $manifest += [pscustomobject]@{
                plate = "$($cls.key)-$tier-plate.png"; frame = $c.name; sourceSheet = "plrgfx\$($cls.source) $($c.sheet)";
                direction = $c.f.dir; frameIndex = $c.f.frame; cropInCell = @($c.cellX, $c.cellY, $c.bmp.Width, $c.bmp.Height);
                plateX = $x; plateY = $y; scale = $Scale }
            $x += $big.Width + $Gap
            $big.Dispose()
        }
        $g.Dispose()
        $plate.Save((Join-Path $OutDir "$($cls.key)-$tier-plate.png"), [System.Drawing.Imaging.ImageFormat]::Png)
        $plate.Dispose()
        foreach ($c in $crops) { $c.bmp.Dispose() }
    }
}
$manifest | ConvertTo-Json -Depth 4 | Set-Content -Path (Join-Path $OutDir 'frames-manifest.json') -Encoding utf8
"plates: " + (Get-ChildItem $OutDir -Filter '*-plate.png').Count
