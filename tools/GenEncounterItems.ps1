# Generates the named-encounter items - D2MXL-to-ORCL Phase 4.
#
# Six items, in two families that arrive together because neither is useful without the other:
#
#   - three SEALED MAPS, each opening one encounter. Consumed on use, so how often an encounter can
#     be run is bounded by drops rather than by a cooldown nobody can see. The map's own description
#     is where "a known reward" lives - it says what it opens and what it pays, which is cheaper and
#     clearer than wiring the quest log and means a player who has never seen one still knows
#     before they go.
#
#   - three REWARD CHARMS, one per encounter, guaranteed on the kill. Charms because the active cap
#     is three, so three signature charms make that cap a decision rather than an inventory rule.
#     Each is strong in ONE stat and narrow, so the choice against a growing charm stays live.
#
# ONE WALK, ONE ORDER - see any other generator here for why a CEL's frame POSITION is the only
# thing tying it to an id.
#
#     pwsh -File tools\GenEncounterItems.ps1
param(
    [string]$OutDir = "Source\oracool",
    # Where the PROCEDURAL placeholders are drawn. Stays in %TEMP%: this folder is WIPED recursively
    # below, so pointing it at Resources would delete the delivered batch-24 icons. Checked in this
    # script rather than assumed from the others (2026-09-12).
    [string]$ArtDir = (Join-Path ([System.IO.Path]::GetTempPath()) "oracool-encounters"),
    # Where the SPECS point: real art since batch 24 (RfA-09, 2026-09-12). Per-file, with a fallback
    # to the placeholder. Relative to the repository root, where build_item_icons.cmd runs.
    [string]$SpecArtDir = "..\Resources\01-in-use-assets\items\encounters",
    # One past the last growing charm. cursor.cpp static_asserts the adjacency.
    [int]$FirstCursorId = 489
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
# Real art if present, else that slug's placeholder. Existence checked against the resolved path;
# the SPEC keeps the relative one.
function Resolve-SpecPath([string]$slug, [string]$placeholder) {
    $rel = Join-Path $SpecArtDir ("$slug.png")
    if (Test-Path (Join-Path $repo $rel)) {
        $script:realCount++
        return $rel
    }
    return $placeholder
}

# Qlvl is where the MAP starts dropping. All three are deep, because a Dread boss is what drops them
# and Dread bosses are guaranteed only from the first Hell floor.
$encounters = @(
    @{ Id = 'CHAPEL';   Enc = 'SunkenChapel';   MapName = 'Map of the Sunken Chapel'
       Charm = 'Chapel Reliquary'; Line = '+60 life while in your backpack'
       Qlvl = 20; Colour = @(120, 148, 190); CharmColour = @(150, 178, 214) }
    @{ Id = 'MOURNING'; Enc = 'RingOfMourning'; MapName = 'Map of the Ring of Mourning'
       Charm = 'Mourning Token'; Line = '+18% to all resistances while in your backpack'
       Qlvl = 24; Colour = @(126, 108, 176); CharmColour = @(160, 140, 206) }
    @{ Id = 'VAULT';    Enc = 'EmberVault';     MapName = 'Map of the Ember Vault'
       Charm = 'Ember Seal'; Line = '+40% better chance of magic items'
       Qlvl = 28; Colour = @(196,  96,  48); CharmColour = @(216, 132,  70) }
)

<#
.SYNOPSIS
Draws a rolled map: a scroll seen end-on with a wax seal.
#>
function New-SealedMap([int[]]$rgb) {
    $bmp = New-Object System.Drawing.Bitmap $cell, $cell, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $parch     = [System.Drawing.Color]::FromArgb(255, 214, 200, 164)
    $parchLit  = [System.Drawing.Color]::FromArgb(255, 240, 230, 202)
    $parchDark = [System.Drawing.Color]::FromArgb(255, 150, 136, 104)
    $edge      = [System.Drawing.Color]::FromArgb(255, 12, 10, 14)
    $sealCol   = [System.Drawing.Color]::FromArgb(255, $rgb[0], $rgb[1], $rgb[2])

    # The rolled body, a vertical cylinder.
    $x = 7.0; $y = 3.0; $w = $cell - 14.0; $h = $cell - 6.0
    $brush = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
        (New-Object System.Drawing.PointF($x, $y)),
        (New-Object System.Drawing.PointF(($x + $w), $y)), $parchLit, $parchDark)
    $g.FillRectangle($brush, $x, $y, $w, $h)
    $pen = New-Object System.Drawing.Pen $edge, 1
    $g.DrawRectangle($pen, $x, $y, $w, $h)

    # The two rolled ends, so it reads as paper rather than a plank.
    $capBrush = New-Object System.Drawing.SolidBrush $parch
    $g.FillEllipse($capBrush, ($x - 2), $y, ($w + 4), 5)
    $g.DrawEllipse($pen, ($x - 2), $y, ($w + 4), 5)
    $g.FillEllipse($capBrush, ($x - 2), ($y + $h - 5), ($w + 4), 5)
    $g.DrawEllipse($pen, ($x - 2), ($y + $h - 5), ($w + 4), 5)

    # The wax seal, which is the only per-encounter colour.
    $sealBrush = New-Object System.Drawing.SolidBrush $sealCol
    $g.FillEllipse($sealBrush, ($cell / 2.0 - 4.5), ($cell / 2.0 - 4.5), 9, 9)
    $sealPen = New-Object System.Drawing.Pen $edge, 1
    $g.DrawEllipse($sealPen, ($cell / 2.0 - 4.5), ($cell / 2.0 - 4.5), 9, 9)

    $sealPen.Dispose(); $sealBrush.Dispose(); $capBrush.Dispose(); $pen.Dispose(); $brush.Dispose(); $g.Dispose()
    return $bmp
}

<#
.SYNOPSIS
Draws a reward charm: a bordered plaque, heavier than the growing charms' tablet.
#>
function New-RewardCharm([int[]]$rgb) {
    $bmp = New-Object System.Drawing.Bitmap $cell, $cell, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias
    $clamp = { param($v) [Math]::Max(0, [Math]::Min(255, [int]$v)) }
    $mid  = [System.Drawing.Color]::FromArgb(255, $rgb[0], $rgb[1], $rgb[2])
    $lit  = [System.Drawing.Color]::FromArgb(255, (& $clamp ($rgb[0] + 66)), (& $clamp ($rgb[1] + 66)), (& $clamp ($rgb[2] + 66)))
    $dark = [System.Drawing.Color]::FromArgb(255, (& $clamp ($rgb[0] - 66)), (& $clamp ($rgb[1] - 66)), (& $clamp ($rgb[2] - 66)))
    $gold = [System.Drawing.Color]::FromArgb(255, 226, 190, 96)
    $edge = [System.Drawing.Color]::FromArgb(255, 10, 9, 12)

    $x = 4.0; $y = 5.0; $w = $cell - 8.0; $h = $cell - 10.0
    $brush = New-Object System.Drawing.Drawing2D.LinearGradientBrush(
        (New-Object System.Drawing.PointF($x, $y)),
        (New-Object System.Drawing.PointF(($x + $w), ($y + $h))), $lit, $dark)
    $g.FillRectangle($brush, $x, $y, $w, $h)
    # A gold frame - the cue that this is a reward rather than a drop.
    $frame = New-Object System.Drawing.Pen $gold, 2
    $g.DrawRectangle($frame, $x, $y, $w, $h)
    $pen = New-Object System.Drawing.Pen $edge, 1
    $g.DrawRectangle($pen, ($x - 1), ($y - 1), ($w + 2), ($h + 2))
    # A single centred mark.
    $markBrush = New-Object System.Drawing.SolidBrush $mid
    $g.FillEllipse($markBrush, ($cell / 2.0 - 3.5), ($cell / 2.0 - 3.5), 7, 7)
    $markPen = New-Object System.Drawing.Pen $gold, 1
    $g.DrawEllipse($markPen, ($cell / 2.0 - 3.5), ($cell / 2.0 - 3.5), 7, 7)

    $markPen.Dispose(); $markBrush.Dispose(); $pen.Dispose(); $frame.Dispose(); $brush.Dispose(); $g.Dispose()
    return $bmp
}

$enumLines = @(); $dataLines = @(); $cursLines = @()
$widthLines = @(); $heightLines = @(); $specLines = @(); $tableLines = @(); $charmLines = @()
$cursor = $FirstCursorId

# MAPS FIRST, then charms - two contiguous islands, so each gets a single range predicate rather
# than an alternating "is it odd" test nobody would trust.
foreach ($e in $encounters) {
    $idi = "IDI_ORACOOL_MAP_$($e.Id)"
    $icurs = "ICURS_ORACOOL_MAP_$($e.Id)"
    $slug = "map_" + $e.Id.ToLowerInvariant()
    $png = Join-Path $artDir ("$slug.png")
    $bmp = New-SealedMap $e.Colour
    try { $bmp.Save($png, [System.Drawing.Imaging.ImageFormat]::Png) } finally { $bmp.Dispose() }

    $enumLines += "`t$idi,"
    # iUsable TRUE and its own misc id - the two fields that make UseItem's switch reach it.
    $dataLines += "/*$idi*/ { IDROP_REGULAR, ICLASS_MISC, ILOC_UNEQUIPABLE, $icurs, ItemType::Misc, UITYPE_NONE, N_(`"$($e.MapName)`"), N_(`"Sealed Map`"), $($e.Qlvl), 0, 0, 0, 0, 0, 0, 0, 0, ItemSpecialEffect::None, IMISC_ORACOOL_MAP, SpellID::Null, true, 12000 },"
    $cursLines += "`t$icurs,"
    $widthLines += "`t$cell, // $($e.MapName)"
    $heightLines += "`t$cell, // $($e.MapName)"
    $specLines += "$(Resolve-SpecPath $slug $png),0,0,$cell,$cell,$cell,$cell,$slug,30,false,asis"
    $cursor++
}

foreach ($e in $encounters) {
    $idi = "IDI_ORACOOL_CHARM_$($e.Id)"
    $icurs = "ICURS_ORACOOL_CHARM_$($e.Id)"
    $slug = "rcharm_" + $e.Id.ToLowerInvariant()
    $png = Join-Path $artDir ("$slug.png")
    $bmp = New-RewardCharm $e.CharmColour
    try { $bmp.Save($png, [System.Drawing.Imaging.ImageFormat]::Png) } finally { $bmp.Dispose() }

    $enumLines += "`t$idi,"
    $dataLines += "/*$idi*/ { IDROP_REGULAR, ICLASS_MISC, ILOC_UNEQUIPABLE, $icurs, ItemType::Misc, UITYPE_NONE, N_(`"$($e.Charm)`"), N_(`"Charm`"), $($e.Qlvl), 0, 0, 0, 0, 0, 0, 0, 0, ItemSpecialEffect::None, IMISC_NONE, SpellID::Null, false, 40000 },"
    $cursLines += "`t$icurs,"
    $widthLines += "`t$cell, // $($e.Charm)"
    $heightLines += "`t$cell, // $($e.Charm)"
    $specLines += "$(Resolve-SpecPath $slug $png),0,0,$cell,$cell,$cell,$cell,$slug,30,false,asis"
    $cursor++

    $charmLines += "`t{ IDI_ORACOOL_CHARM_$($e.Id), N_(`"$($e.Line)`") }, // $($e.Charm)"
    $tableLines += "`t{ NamedEncounter::$($e.Enc), IDI_ORACOOL_MAP_$($e.Id), IDI_ORACOOL_CHARM_$($e.Id) },"
}

function Write-Inc([string]$file, [string]$what, [string[]]$lines) {
    $header = @(
        "// GENERATED by tools/GenEncounterItems.ps1 - do not edit by hand.",
        "// $what",
        ""
    )
    [System.IO.File]::WriteAllLines((Join-Path $out $file), ($header + $lines), (New-Object System.Text.UTF8Encoding $false))
    Write-Host ("  $file  ($($lines.Count) lines)")
}

Write-Inc "encounter_items_enum.inc"   "The Sealed Maps, then the reward charms - two islands."  $enumLines
Write-Inc "encounter_items_data.inc"   "Their AllItemsList rows, in enum order."                 $dataLines
Write-Inc "encounter_items_curs.inc"   "Their ICURS ids - appended after the growing charms."    $cursLines
Write-Inc "encounter_items_curs_widths.inc"  "Frame widths, in the same order."                  $widthLines
Write-Inc "encounter_items_curs_heights.inc" "Frame heights, in the same order."                 $heightLines
Write-Inc "encounter_items_table.inc"  "encounter -> map item -> reward charm."                  $tableLines
Write-Inc "encounter_charms.inc"       "The reward charms' description lines."                   $charmLines

[System.IO.File]::WriteAllLines((Join-Path $out "encounter_items_icon_specs.txt"), $specLines, (New-Object System.Text.UTF8Encoding $false))
Write-Host ("  encounter_items_icon_specs.txt  ($($specLines.Count) specs)")
Write-Host ""
Write-Host ("$($enumLines.Count) items, cursor ids $FirstCursorId..$($cursor - 1)")
Write-Host ("  real art: $realCount of $($specLines.Count) specs, from $SpecArtDir")
Write-Host ("  placeholders drawn into $artDir (fallback only)")
Write-Host "Now: tools\build_item_icons.cmd, then tools\build_oracool_mpq.cmd."
