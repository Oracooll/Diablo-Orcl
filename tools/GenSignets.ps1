# Generates the Signet of Learning - D2MXL-to-ORCL Phase 2b.
#
# ONE ITEM, AND STILL A GENERATOR. That looks like ceremony for a single row, and it is not: the
# ids, the AllItemsList row, the ICURS id, the CEL frame size and the icon cut spec all have to
# agree, and a CEL stores no names and no sizes - a frame's POSITION in the file is the only thing
# tying it to an id. Hand-adding one item to five places is exactly how the sixth place gets missed,
# and the failure is silent: every icon after the mismatch is wrong.
#
# It is also what makes the next signet-like item free.
#
#     pwsh -File tools\GenSignets.ps1
param(
    [string]$OutDir = "Source\oracool",
    # Where the PROCEDURAL placeholder is drawn. Stays in %TEMP%: this folder is WIPED recursively
    # below, so pointing it at Resources would delete the delivered art. Checked in this script
    # rather than assumed from the others (2026-09-12).
    [string]$ArtDir = (Join-Path ([System.IO.Path]::GetTempPath()) "oracool-signets"),
    # Where the SPEC points: real art since batch 27 (RfA-10, 2026-09-12). Batch 25 was rejected for
    # having no visible ring hole; 27 passes that check with 113 enclosed transparent pixels.
    # Per-file, with a fallback to the placeholder. Relative to the repository root, where
    # build_item_icons.cmd runs.
    [string]$SpecArtDir = "..\Resources\02. Oracooll Assets\01. Used\items",
    # One past the last Mystic Orb. cursor.cpp static_asserts the adjacency.
    [int]$FirstCursorId = 485
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$repo = Split-Path -Parent $PSScriptRoot
$out = if ([System.IO.Path]::IsPathRooted($OutDir)) { $OutDir } else { Join-Path $repo $OutDir }
$artDir = $ArtDir
if (Test-Path $artDir) { Remove-Item -Recurse -Force $artDir }
New-Item -ItemType Directory -Force $artDir | Out-Null

$cell = 28

$realCount = 0
# Real art if present, else the placeholder. Existence is checked against the resolved path; the
# SPEC keeps the relative one, so no machine-specific path lands in a tracked file.
function Resolve-SpecPath([string]$slug, [string]$placeholder) {
    $rel = Join-Path $SpecArtDir ("$slug.png")
    if (Test-Path (Join-Path $repo $rel)) {
        $script:realCount++
        return $rel
    }
    return $placeholder
}

# Qlvl 8 rather than something deep: a signet is capped for life, so making it a late find would
# mean the pool only opens once most of the levelling is behind you - which is the half of the game
# the points are least useful in.
$signets = @(
    @{ Id = 'LEARNING'; Name = 'Signet of Learning'; Qlvl = 8; Value = 5000 }
)

<#
.SYNOPSIS
Draws a signet: a gold ring seen face-on with a dark seal stone.

A RING silhouette, deliberately unlike the orbs' spheres and the jewels' lozenges sitting beside it
in the same 28x28 cell. At this size shape reads before colour, and three families of small round
things would be three families a player has to squint at.
#>
function New-Signet() {
    $bmp = New-Object System.Drawing.Bitmap $cell, $cell, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias

    $gold     = [System.Drawing.Color]::FromArgb(255, 206, 168,  66)
    $goldLit  = [System.Drawing.Color]::FromArgb(255, 246, 220, 140)
    $goldDark = [System.Drawing.Color]::FromArgb(255, 124,  92,  28)
    $seal     = [System.Drawing.Color]::FromArgb(255,  52,  38,  72)
    $sealLit  = [System.Drawing.Color]::FromArgb(255, 118,  92, 172)
    $edge     = [System.Drawing.Color]::FromArgb(255,  10,   9,  12)

    # The band: a thick ring, lit from the top left like everything else on this sheet.
    $inset = 5.0
    $d = $cell - $inset * 2
    $bandDark = New-Object System.Drawing.Pen $goldDark, 5
    $g.DrawEllipse($bandDark, $inset, $inset, $d, $d)
    $band = New-Object System.Drawing.Pen $gold, 3.4
    $g.DrawEllipse($band, $inset, $inset, $d, $d)
    $bandLit = New-Object System.Drawing.Pen $goldLit, 1.4
    $g.DrawArc($bandLit, $inset, $inset, $d, $d, 170, 110)
    $rim = New-Object System.Drawing.Pen $edge, 1
    $g.DrawEllipse($rim, ($inset - 2.5), ($inset - 2.5), ($d + 5), ($d + 5))

    # The seal stone, top-centre, where a signet's face sits.
    $sealW = 12.0
    $sealH = 9.0
    $sealX = ($cell - $sealW) / 2.0
    $sealY = 1.5
    $sealBrush = New-Object System.Drawing.SolidBrush $seal
    $g.FillEllipse($sealBrush, $sealX, $sealY, $sealW, $sealH)
    $sealEdge = New-Object System.Drawing.Pen $goldLit, 1.2
    $g.DrawEllipse($sealEdge, $sealX, $sealY, $sealW, $sealH)
    $spark = New-Object System.Drawing.SolidBrush $sealLit
    $g.FillEllipse($spark, ($sealX + 3), ($sealY + 2), 3.5, 2.6)

    $spark.Dispose(); $sealEdge.Dispose(); $sealBrush.Dispose()
    $rim.Dispose(); $bandLit.Dispose(); $band.Dispose(); $bandDark.Dispose(); $g.Dispose()
    return $bmp
}

$enumLines = @(); $dataLines = @(); $cursLines = @()
$widthLines = @(); $heightLines = @(); $specLines = @()
$cursor = $FirstCursorId

foreach ($s in $signets) {
    $idi = "IDI_ORACOOL_SIGNET_$($s.Id)"
    $icurs = "ICURS_ORACOOL_SIGNET_$($s.Id)"
    $slug = "signet_" + $s.Id.ToLowerInvariant()
    $png = Join-Path $artDir ("$slug.png")

    $bmp = New-Signet
    try { $bmp.Save($png, [System.Drawing.Imaging.ImageFormat]::Png) } finally { $bmp.Dispose() }

    $enumLines += "`t$idi,"
    # IMISC_ORACOOL_SIGNET and iUsable TRUE - the two fields that make UseItem's switch reach it.
    # Everything else is a rune's row, which is what makes signets stack and sort like one.
    $dataLines += "/*$idi*/ { IDROP_REGULAR, ICLASS_MISC, ILOC_UNEQUIPABLE, $icurs, ItemType::Misc, UITYPE_NONE, N_(`"$($s.Name)`"), N_(`"Signet`"), $($s.Qlvl), 0, 0, 0, 0, 0, 0, 0, 0, ItemSpecialEffect::None, IMISC_ORACOOL_SIGNET, SpellID::Null, true, $($s.Value) },"
    $cursLines += "`t$icurs,"
    $widthLines += "`t$cell, // $($s.Name)"
    $heightLines += "`t$cell, // $($s.Name)"
    $specLines += "$(Resolve-SpecPath $slug $png),0,0,$cell,$cell,$cell,$cell,$slug,30,false,asis"

    $cursor++
}

function Write-Inc([string]$file, [string]$what, [string[]]$lines) {
    $header = @(
        "// GENERATED by tools/GenSignets.ps1 - do not edit by hand.",
        "// $what",
        ""
    )
    [System.IO.File]::WriteAllLines((Join-Path $out $file), ($header + $lines), (New-Object System.Text.UTF8Encoding $false))
    Write-Host ("  $file  ($($lines.Count) lines)")
}

Write-Inc "signets_enum.inc"   "The Signet ids."                                    $enumLines
Write-Inc "signets_data.inc"   "Their AllItemsList rows, in enum order."            $dataLines
Write-Inc "signets_curs.inc"   "Their ICURS ids - appended after the Mystic Orbs."  $cursLines
Write-Inc "signets_curs_widths.inc"  "Frame widths, in the same order."             $widthLines
Write-Inc "signets_curs_heights.inc" "Frame heights, in the same order."            $heightLines

[System.IO.File]::WriteAllLines((Join-Path $out "signets_icon_specs.txt"), $specLines, (New-Object System.Text.UTF8Encoding $false))
Write-Host ("  signets_icon_specs.txt  ($($specLines.Count) specs)")
Write-Host ""
Write-Host ("$($enumLines.Count) signet(s), cursor ids $FirstCursorId..$($cursor - 1)")
Write-Host ("  real art: $realCount of $($specLines.Count) specs, from $SpecArtDir")
Write-Host "Now: tools\build_item_icons.cmd, then tools\build_oracool_mpq.cmd."
