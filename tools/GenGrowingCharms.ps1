# Generates the growing charms - D2MXL-to-ORCL Phase 3.
#
# WHAT THEY GROW WITH, AND WHY THAT IS NOT STORED
#
# Median XL's charms gain stats as you complete challenges. The plan for this phase assumed that
# needed per-item state and budgeted a save format bump for it, riding the byte Phase 1 paid for.
#
# It does not. These grow with the character's CLAIMED MILESTONES, which already live in the hero
# chunk tail (oracool/signets.h, shipped v1.9.20) - so the charm's power is a pure function of
# something already persisted, needs no field on the item at all, and costs no version bump.
#
# The trade is real and worth stating: every copy of a growing charm on one character is worth the
# same, because the growth belongs to the CHARACTER rather than to the object. That is arguably the
# better reading anyway - the charm is a record of what you have done, not a thing with a private
# history you cannot see - and it is what ties this phase to the milestones instead of merely
# shipping after them.
#
# ONE WALK, ONE ORDER - see any other generator here for why.
#
#     pwsh -File tools\GenGrowingCharms.ps1
param(
    [string]$OutDir = "Source\oracool",
    [string]$ArtDir = (Join-Path ([System.IO.Path]::GetTempPath()) "oracool-growing-charms"),
    # One past the Signet of Learning. cursor.cpp static_asserts the adjacency.
    [int]$FirstCursorId = 486
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$repo = Split-Path -Parent $PSScriptRoot
$out = if ([System.IO.Path]::IsPathRooted($OutDir)) { $OutDir } else { Join-Path $repo $OutDir }
$artDir = $ArtDir
if (Test-Path $artDir) { Remove-Item -Recurse -Force $artDir }
New-Item -ItemType Directory -Force $artDir | Out-Null

$cell = 28

# The three, one per stat a fixed charm already covers - so a player choosing between a Charm of
# Vigor and a Charm of Trials is choosing "good now" against "better later", which is a real
# decision rather than a strictly-better item.
#
# Base is what it is worth with NO milestones claimed, deliberately below the fixed charm's value;
# PerMilestone is what each claimed milestone adds. At eight milestones the growing charm passes its
# fixed cousin, which is the point at which having played the character starts to pay.
$charms = @(
    @{ Id = 'TRIALS'; Name = 'Charm of Trials'; Field = 'hitPoints';  Base =  8; Per = 3; Colour = @(176,  58,  58)
       Line = 'life' }
    @{ Id = 'DEEDS';  Name = 'Charm of Deeds';  Field = 'allRes';     Base =  4; Per = 2; Colour = @(120, 152, 196)
       Line = 'to all resistances' }
    @{ Id = 'LEGEND'; Name = 'Charm of Legend'; Field = 'magicFind';  Base =  5; Per = 2; Colour = @(198, 178,  92)
       Line = '% better chance of magic items' }
)

<#
.SYNOPSIS
Draws a charm: a rounded tablet with a rising notch, marked.

Deliberately a TABLET rather than the orbs' spheres, the jewels' lozenges or the signet's ring - the
28x28 cell now holds five small families and silhouette is what tells them apart before colour does.
The three notches read as a rising bar, which is the one visual cue that these grow.
#>
function New-GrowingCharm([int[]]$rgb) {
    $bmp = New-Object System.Drawing.Bitmap $cell, $cell, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $clamp = { param($v) [Math]::Max(0, [Math]::Min(255, [int]$v)) }
    $mid  = [System.Drawing.Color]::FromArgb(255, $rgb[0], $rgb[1], $rgb[2])
    $lit  = [System.Drawing.Color]::FromArgb(255, (& $clamp ($rgb[0] + 62)), (& $clamp ($rgb[1] + 62)), (& $clamp ($rgb[2] + 62)))
    $dark = [System.Drawing.Color]::FromArgb(255, (& $clamp ($rgb[0] - 58)), (& $clamp ($rgb[1] - 58)), (& $clamp ($rgb[2] - 58)))
    $edge = [System.Drawing.Color]::FromArgb(255, 10, 9, 12)

    $x = 6.0; $y = 3.0; $w = $cell - 12.0; $h = $cell - 6.0
    $body = New-Object System.Drawing.Drawing2D.GraphicsPath
    $r = 4.0
    $body.AddArc($x, $y, $r * 2, $r * 2, 180, 90)
    $body.AddArc($x + $w - $r * 2, $y, $r * 2, $r * 2, 270, 90)
    $body.AddArc($x + $w - $r * 2, $y + $h - $r * 2, $r * 2, $r * 2, 0, 90)
    $body.AddArc($x, $y + $h - $r * 2, $r * 2, $r * 2, 90, 90)
    $body.CloseFigure()

    $brush = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
        (New-Object System.Drawing.PointF($x, $y)),
        (New-Object System.Drawing.PointF(($x + $w), ($y + $h))), $lit, $dark)
    $g.FillPath($brush, $body)
    $pen = New-Object System.Drawing.Pen $edge, 1
    $g.DrawPath($pen, $body)

    # Three rising notches - the growth cue.
    $markBrush = New-Object System.Drawing.SolidBrush $mid
    $markLit = New-Object System.Drawing.SolidBrush $lit
    for ($i = 0; $i -lt 3; $i++) {
        $bh = 4.0 + $i * 3.5
        $bx = $x + 3.0 + $i * 4.0
        $by = $y + $h - 4.0 - $bh
        $g.FillRectangle($markBrush, $bx, $by, 3.0, $bh)
        $g.FillRectangle($markLit, $bx, $by, 3.0, 1.4)
    }

    $markLit.Dispose(); $markBrush.Dispose(); $pen.Dispose(); $brush.Dispose(); $body.Dispose(); $g.Dispose()
    return $bmp
}

$enumLines = @(); $dataLines = @(); $cursLines = @()
$widthLines = @(); $heightLines = @(); $specLines = @(); $growthLines = @()
$cursor = $FirstCursorId

foreach ($c in $charms) {
    $idi = "IDI_ORACOOL_CHARM_$($c.Id)"
    $icurs = "ICURS_ORACOOL_CHARM_$($c.Id)"
    $slug = "charm_" + $c.Id.ToLowerInvariant()
    $png = Join-Path $artDir ("$slug.png")

    $bmp = New-GrowingCharm $c.Colour
    try { $bmp.Save($png, [System.Drawing.Imaging.ImageFormat]::Png) } finally { $bmp.Dispose() }

    $enumLines += "`t$idi,"
    # A charm's row exactly - ICLASS_MISC, ILOC_UNEQUIPABLE, IMISC_NONE - so these obey the active
    # cap, the drop walk and the stash sort through the code that already exists.
    $dataLines += "/*$idi*/ { IDROP_REGULAR, ICLASS_MISC, ILOC_UNEQUIPABLE, $icurs, ItemType::Misc, UITYPE_NONE, N_(`"$($c.Name)`"), N_(`"Charm`"), 10, 0, 0, 0, 0, 0, 0, 0, 0, ItemSpecialEffect::None, IMISC_NONE, SpellID::Null, false, 6000 },"
    $cursLines += "`t$icurs,"
    $widthLines += "`t$cell, // $($c.Name)"
    $heightLines += "`t$cell, // $($c.Name)"
    $specLines += "$png,0,0,$cell,$cell,$cell,$cell,$slug,30,false,asis"
    $growthLines += "`t{ $idi, GrowingStat::$($c.Id), $($c.Base), $($c.Per), N_(`"$($c.Line)`") }, // $($c.Name)"

    $cursor++
}

function Write-Inc([string]$file, [string]$what, [string[]]$lines) {
    $header = @(
        "// GENERATED by tools/GenGrowingCharms.ps1 - do not edit by hand.",
        "// $what",
        ""
    )
    [System.IO.File]::WriteAllLines((Join-Path $out $file), ($header + $lines), (New-Object System.Text.UTF8Encoding $false))
    Write-Host ("  $file  ($($lines.Count) lines)")
}

Write-Inc "growing_charms_enum.inc"   "The growing charm ids."                                $enumLines
Write-Inc "growing_charms_data.inc"   "Their AllItemsList rows, in enum order."               $dataLines
Write-Inc "growing_charms_curs.inc"   "Their ICURS ids - appended after the Signet."          $cursLines
Write-Inc "growing_charms_curs_widths.inc"  "Frame widths, in the same order."                $widthLines
Write-Inc "growing_charms_curs_heights.inc" "Frame heights, in the same order."               $heightLines
Write-Inc "growing_charms_growth.inc" "Base, per-milestone growth and description fragment."  $growthLines

[System.IO.File]::WriteAllLines((Join-Path $out "growing_charms_icon_specs.txt"), $specLines, (New-Object System.Text.UTF8Encoding $false))
Write-Host ("  growing_charms_icon_specs.txt  ($($specLines.Count) specs)")
Write-Host ""
Write-Host ("$($enumLines.Count) growing charms, cursor ids $FirstCursorId..$($cursor - 1), art in $artDir")
Write-Host "Now: tools\build_item_icons.cmd, then tools\build_oracool_mpq.cmd."
