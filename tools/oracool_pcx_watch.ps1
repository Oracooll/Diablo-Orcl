<#
.SYNOPSIS
    Watches a folder and converts every .pcx that appears (or is already there) into
    a .jpg, then deletes the original .pcx.

.DESCRIPTION
    Oracool screenshot workflow: the in-game screenshot key saves .pcx files into
    Saved_Games (Source/capture.cpp), a format nothing on Windows opens natively.
    This keeps that folder permanently in .jpg so screenshots are viewable the
    instant they're taken.

    Decodes the same format oracool_pcx_to_png.ps1 handles - 128-byte header,
    RLE-compressed 8bpp scanlines, trailing 769-byte VGA palette (marker 0x0C +
    768 bytes RGB) - but builds the image via LockBits instead of per-pixel
    SetPixel, which is roughly two orders of magnitude faster (a 960x720 shot goes
    from ~30s to well under a second).

    Safety: the source .pcx is deleted only after the .jpg has been written AND
    verified non-empty on disk. Files still being written by the game are skipped
    until their size stops changing and they can be opened exclusively, so a
    screenshot is never converted half-finished. Use -KeepOriginal to disable
    deletion entirely.

.PARAMETER Path
    Folder to watch. Defaults to the Debug build's Screenshots folder (the game creates it on the
    first screenshot; before v1.0.74 screenshots went to Saved_Games instead).

.PARAMETER Quality
    JPEG quality, 1-100. Defaults to 92.

.PARAMETER IntervalMs
    Polling interval in milliseconds. Defaults to 500.

.PARAMETER KeepOriginal
    Convert but do NOT delete the source .pcx.

.PARAMETER Once
    Convert everything currently in the folder, then exit instead of watching.

.EXAMPLE
    .\oracool_pcx_watch.ps1
    Watches the default Saved_Games folder until you press Ctrl+C.

.EXAMPLE
    .\oracool_pcx_watch.ps1 -Once -KeepOriginal
    One-off batch conversion of whatever is already there, keeping the .pcx files.
#>
param(
    [string]$Path = "$PSScriptRoot\..\build\x64-Debug\Screenshots",
    [ValidateRange(1, 100)][int]$Quality = 92,
    [int]$IntervalMs = 500,
    [switch]$KeepOriginal,
    [switch]$Once
)

Add-Type -AssemblyName System.Drawing

if (-not (Test-Path -LiteralPath $Path)) {
    Write-Error "Folder not found: $Path"
    exit 1
}
$Path = (Resolve-Path -LiteralPath $Path).Path

$jpegCodec = [System.Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() | Where-Object { $_.MimeType -eq 'image/jpeg' }
$encoderParams = New-Object System.Drawing.Imaging.EncoderParameters(1)
$encoderParams.Param[0] = New-Object System.Drawing.Imaging.EncoderParameter([System.Drawing.Imaging.Encoder]::Quality, [int64]$Quality)

function Convert-Pcx {
    param([string]$InputPath, [string]$OutputPath)

    $bytes = [System.IO.File]::ReadAllBytes($InputPath)
    if ($bytes.Length -lt 900) { throw "File too small to be a valid PCX ($($bytes.Length) bytes)" }

    $xmin = [int]$bytes[4] + ([int]$bytes[5] * 256)
    $ymin = [int]$bytes[6] + ([int]$bytes[7] * 256)
    $xmax = [int]$bytes[8] + ([int]$bytes[9] * 256)
    $ymax = [int]$bytes[10] + ([int]$bytes[11] * 256)
    $bitsPerPixel = $bytes[3]
    $nPlanes = $bytes[65]
    $bytesPerLine = [int]$bytes[66] + ([int]$bytes[67] * 256)

    $width = $xmax - $xmin + 1
    $height = $ymax - $ymin + 1
    if ($bitsPerPixel -ne 8 -or $nPlanes -ne 1) {
        throw "Unsupported PCX (bpp=$bitsPerPixel planes=$nPlanes); only 8bpp/1-plane is handled"
    }
    if ($width -le 0 -or $height -le 0) { throw "Bad dimensions ${width}x${height}" }

    # RLE decode
    $rowBytes = $nPlanes * $bytesPerLine
    $total = $rowBytes * $height
    $decoded = New-Object byte[] $total
    $srcPos = 128
    $dstPos = 0
    $limit = $bytes.Length
    while ($dstPos -lt $total -and $srcPos -lt $limit) {
        $b = $bytes[$srcPos]; $srcPos++
        if (($b -band 0xC0) -eq 0xC0) {
            $count = $b -band 0x3F
            if ($srcPos -ge $limit) { break }
            $val = $bytes[$srcPos]; $srcPos++
            for ($i = 0; $i -lt $count -and $dstPos -lt $total; $i++) {
                $decoded[$dstPos] = $val; $dstPos++
            }
        } else {
            $decoded[$dstPos] = $b; $dstPos++
        }
    }

    # 8bpp indexed bitmap + VGA palette from the trailing 769 bytes
    $bmp = New-Object System.Drawing.Bitmap($width, $height, [System.Drawing.Imaging.PixelFormat]::Format8bppIndexed)
    try {
        $palOffset = $bytes.Length - 769
        $pal = $bmp.Palette
        if ($palOffset -gt 0 -and $bytes[$palOffset] -eq 0x0C) {
            for ($i = 0; $i -lt 256; $i++) {
                $pal.Entries[$i] = [System.Drawing.Color]::FromArgb(
                    $bytes[$palOffset + 1 + $i * 3],
                    $bytes[$palOffset + 2 + $i * 3],
                    $bytes[$palOffset + 3 + $i * 3])
            }
        } else {
            for ($i = 0; $i -lt 256; $i++) { $pal.Entries[$i] = [System.Drawing.Color]::FromArgb($i, $i, $i) }
        }
        $bmp.Palette = $pal

        # Row-wise copy into the locked bits (stride and bytesPerLine rarely match)
        $rect = New-Object System.Drawing.Rectangle(0, 0, $width, $height)
        $data = $bmp.LockBits($rect, [System.Drawing.Imaging.ImageLockMode]::WriteOnly, [System.Drawing.Imaging.PixelFormat]::Format8bppIndexed)
        try {
            for ($y = 0; $y -lt $height; $y++) {
                $dstPtr = [IntPtr]::Add($data.Scan0, $y * $data.Stride)
                [System.Runtime.InteropServices.Marshal]::Copy($decoded, $y * $rowBytes, $dstPtr, $width)
            }
        } finally {
            $bmp.UnlockBits($data)
        }

        # JPEG needs a non-indexed surface
        $flat = New-Object System.Drawing.Bitmap($width, $height, [System.Drawing.Imaging.PixelFormat]::Format24bppRgb)
        try {
            $g = [System.Drawing.Graphics]::FromImage($flat)
            try { $g.DrawImageUnscaled($bmp, 0, 0) } finally { $g.Dispose() }
            $flat.Save($OutputPath, $script:jpegCodec, $script:encoderParams)
        } finally {
            $flat.Dispose()
        }
    } finally {
        $bmp.Dispose()
    }

    return "${width}x${height}"
}

# Tracks sizes between sweeps so a file mid-write is skipped until it settles.
$pending = @{}

function Invoke-Sweep {
    foreach ($file in @(Get-ChildItem -LiteralPath $Path -Filter *.pcx -File -ErrorAction SilentlyContinue)) {
        $key = $file.FullName
        $size = $file.Length

        # Require a stable size across two sweeps before touching the file.
        if (-not $pending.ContainsKey($key) -or $pending[$key] -ne $size) {
            $pending[$key] = $size
            continue
        }

        # And require an exclusive open, so the game isn't still holding it.
        try {
            $stream = [System.IO.File]::Open($key, 'Open', 'Read', 'None')
            $stream.Close()
        } catch {
            continue
        }

        $outPath = [System.IO.Path]::ChangeExtension($key, '.jpg')
        try {
            $dims = Convert-Pcx -InputPath $key -OutputPath $outPath
        } catch {
            Write-Host ("[{0}] FAILED {1}: {2}" -f (Get-Date -Format 'HH:mm:ss'), $file.Name, $_.Exception.Message) -ForegroundColor Red
            $pending.Remove($key)
            continue
        }

        # Only remove the source once the .jpg is verifiably on disk and non-empty.
        $ok = (Test-Path -LiteralPath $outPath) -and ((Get-Item -LiteralPath $outPath).Length -gt 0)
        if ($ok -and -not $KeepOriginal) {
            try {
                Remove-Item -LiteralPath $key -Force -ErrorAction Stop
                $note = "converted + removed source"
            } catch {
                $note = "converted; source NOT removed ($($_.Exception.Message))"
            }
        } elseif ($ok) {
            $note = "converted (source kept)"
        } else {
            $note = "conversion produced no output - source kept"
        }

        Write-Host ("[{0}] {1} -> {2} [{3}] {4}" -f (Get-Date -Format 'HH:mm:ss'), $file.Name, (Split-Path $outPath -Leaf), $dims, $note) -ForegroundColor Green
        $pending.Remove($key)
    }

    # Forget entries whose files are gone, so the table can't grow forever.
    foreach ($stale in @($pending.Keys | Where-Object { -not (Test-Path -LiteralPath $_) })) {
        $pending.Remove($stale)
    }
}

if ($Once) {
    # No settle-delay needed for a batch run over files that are already at rest:
    # prime the size table, then convert in a second pass.
    Invoke-Sweep
    Invoke-Sweep
    Write-Host "Done." -ForegroundColor Cyan
    return
}

Write-Host "Watching: $Path" -ForegroundColor Cyan
Write-Host ("JPEG quality {0} | poll {1}ms | source .pcx {2}" -f $Quality, $IntervalMs, $(if ($KeepOriginal) { 'kept' } else { 'deleted after conversion' })) -ForegroundColor DarkGray
Write-Host "Press Ctrl+C to stop." -ForegroundColor DarkGray

while ($true) {
    Invoke-Sweep
    Start-Sleep -Milliseconds $IntervalMs
}
