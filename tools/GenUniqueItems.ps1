# Generates Source/oracool/unique_items_data.inc from the 250-unique expansion package.
#
# The package (Oracool.MPQ/02-source-art/unique-items/unique-item-expansion-250.zip) is data only -
# names, bases, requirements, drop bands, lore, visual briefs, exact affix tokens. Sprites are
# deliberately outside it and are still to come, which is why nothing here allocates an icon: a
# unique keeps its BASE item's sprite until the art arrives. That also means no CEL frame order to
# get wrong, unlike the item sets.
#
# Identity rides the vanilla UniqueItem machinery. Appending rows to UniqueItems[] gets naming,
# drop-rolling, description and _iUid persistence for free - _iUid is already an int and already
# saved - so 250 new uniques cost no save-format change and no new item field.
#
# Affix tokens are resolved through Source/oracool/unique_affixes.cpp, NOT through the package's own
# `enginePower` column. The package is mostly right and wrong in one place; see that file's header.
# This script reports every disagreement rather than silently preferring one side.
#
#     pwsh -File tools\GenUniqueItems.ps1

param(
    [string]$Package    = "..\Oracool.MPQ\02-source-art\unique-items\unique-item-expansion-250.zip",
    [string]$AffixTable = "Source\oracool\unique_affixes.cpp",
    [string]$BaseEnum   = "Source\itemdat.h",
    [string]$OutFile    = "Source\oracool\unique_items_data.inc"
)

$ErrorActionPreference = "Stop"

# --- the package -----------------------------------------------------------------------------------
$work = Join-Path ([System.IO.Path]::GetTempPath()) "oracool-uniques"
if (Test-Path $work) { Remove-Item -Recurse -Force $work }
Expand-Archive -Path $Package -DestinationPath $work -Force
$jsonPath = Get-ChildItem $work -Recurse -Filter "unique-items.json" | Select-Object -First 1
if (-not $jsonPath) { throw "no unique-items.json inside $Package" }
$pkg = Get-Content $jsonPath.FullName -Raw | ConvertFrom-Json
Write-Host "package: $($pkg.items.Count) items"

# --- the affix mapping, read out of the C++ table so the two cannot drift --------------------------
$affix = @{}
foreach ($line in Get-Content $AffixTable) {
    if ($line -match '^\s*\{\s*"([a-z_]+)",\s*(Power|Approx|Inert),\s*(IPL_[A-Z0-9_]+)') {
        $affix[$Matches[1]] = @{ Fidelity = $Matches[2]; Power = $Matches[3] }
    }
}
Write-Host "affix table: $($affix.Count) tokens"
if ($affix.Count -lt 40) { throw "only $($affix.Count) tokens parsed from $AffixTable - the row format changed?" }

# --- which UITYPE_ values the engine actually has --------------------------------------------------
$engineBases = @{}
$inEnum = $false
foreach ($line in Get-Content $BaseEnum) {
    if ($line -match '^enum unique_base_item') { $inEnum = $true; continue }
    if ($inEnum) {
        if ($line -match '^\};') { break }
        if ($line -match '^\s*(UITYPE_[A-Z0-9_]+)') { $engineBases[$Matches[1]] = $true }
    }
}
Write-Host "engine bases: $($engineBases.Count) UITYPE_ values"

# --- walk the items --------------------------------------------------------------------------------
$rows = @()
$skippedNoBase = @{}
$disagreements = @()
$liveAffixes = 0
$inertAffixes = 0
$emptyItems = @()

foreach ($it in $pkg.items) {
    # A base this engine has no UITYPE_ for cannot be rolled onto anything, so the item is SKIPPED
    # rather than emitted pointing at UITYPE_NONE - which would make it undroppable in a way that
    # looked like a live row. Counted and named at the end instead.
    if (-not $engineBases.ContainsKey($it.baseToken)) {
        if (-not $skippedNoBase.ContainsKey($it.baseToken)) { $skippedNoBase[$it.baseToken] = 0 }
        $skippedNoBase[$it.baseToken]++
        continue
    }

    $powers = @()
    foreach ($a in $it.affixes) {
        if (-not $affix.ContainsKey($a.token)) { throw "item $($it.id): unknown affix token '$($a.token)'" }
        $m = $affix[$a.token]

        # The package's own opinion, kept only as a cross-check. Recorded, never obeyed.
        if ($a.enginePower -and $m.Power -ne $a.enginePower -and $m.Fidelity -ne "Inert") {
            $disagreements += "$($a.token): package says $($a.enginePower), table says $($m.Power)"
        }

        if ($m.Fidelity -eq "Inert") { $inertAffixes++; continue }

        # The elemental powers declare a RANGE ([1,6]); everything else a scalar. ItemPower's two
        # parameters are exactly min and max, so a range needs no reshaping - but a scalar cast over
        # an array is the silent kind of wrong, so the two are separated here rather than coerced.
        $isRange = $a.value -is [System.Array]
        $v = if ($isRange) { [int]$a.value[0] } else { [int]$a.value }
        # items.cpp only raises a steal flag for exactly 3 and 5; every other value falls through
        # both ifs and does nothing at all.
        if ($m.Power -in @("IPL_STEALLIFE", "IPL_STEALMANA") -and $v -notin @(3, 5)) {
            throw "item $($it.id): '$($a.token)' is $v, but only 3 and 5 exist"
        }
        # The two speed powers carry a discrete TIER in param1, not a magnitude. The package already
        # declares 1..3, which is a tier - but clamp anyway, because a future batch declaring "+20%"
        # would otherwise become tier 20 and read as garbage out of SaveItemPower.
        if ($m.Power -eq "IPL_FASTATTACK")  { $v = [Math]::Max(1, [Math]::Min(4, $v)) }
        if ($m.Power -eq "IPL_FASTRECOVER") { $v = [Math]::Max(1, [Math]::Min(3, $v)) }
        # IPL_GETHIT SUBTRACTS its parameter, so a reduction has to arrive positive. No current token
        # maps to it, but the rule belongs with the other packing rules rather than in a future
        # reader's memory.
        if ($m.Power -eq "IPL_GETHIT") { $v = [Math]::Abs($v) }

        # param2 is the range's upper bound, or param1 again for a scalar.
        $p2 = if ($isRange) { [int]$a.value[1] } else { $v }
        $liveAffixes++
        $powers += "{ $($m.Power), $v, $p2 }"
    }

    # UniqueItem carries six power slots and UINumPL counts them.
    if ($powers.Count -gt 6) { throw "item $($it.id) has $($powers.Count) live affixes, more than the six slots" }
    # A unique that grants nothing is the failure this project already shipped once, in the item-set
    # bonus ladders. It is a build error here, not something to find in play.
    if ($powers.Count -eq 0) { $emptyItems += $it.id; continue }

    $numPl = $powers.Count
    while ($powers.Count -lt 6) { $powers += "{ IPL_INVALID, 0, 0 }" }

    # UIValue is the gold value the vanilla roller assigns. The package has no such field, so it is
    # derived from the drop band rather than invented per item: a late-band unique should not be
    # worth the same as an early one, and deriving it means retuning the bands retunes every price.
    $value = switch ($it.powerBand) { "early" { 6000 } "mid" { 20000 } "late" { 60000 } default { 20000 } }

    $name = $it.name -replace '\\', '\\\\' -replace '"', '\"'
    $rows += "`t{ N_(`"$name`"), $($it.baseToken), $([int]$it.dropLevel), $numPl, $value, { $($powers -join ', ') } },"
}

# --- report ----------------------------------------------------------------------------------------
if ($emptyItems.Count -gt 0) {
    throw "these items compile to NO working affix at all: $($emptyItems -join ', ')"
}
Write-Host "affixes: $liveAffixes live, $inertAffixes inert"
if ($disagreements.Count -gt 0) {
    Write-Host "package/table disagreements (table wins, by design):"
    $disagreements | Sort-Object -Unique | ForEach-Object { Write-Host "  $_" }
}
if ($skippedNoBase.Count -gt 0) {
    $total = ($skippedNoBase.Values | Measure-Object -Sum).Sum
    Write-Host "skipped $total items on $($skippedNoBase.Count) bases this engine has no UITYPE_ for:"
    $skippedNoBase.GetEnumerator() | Sort-Object Value -Descending | ForEach-Object { Write-Host "  $($_.Key) x$($_.Value)" }
}

$out = @()
$out += "// GENERATED by tools/GenUniqueItems.ps1 - do not edit."
$out += "//"
$out += "// Source: Oracool.MPQ/02-source-art/unique-items/unique-item-expansion-250.zip."
$out += "// Affix meanings come from oracool/unique_affixes.cpp, NOT the package's enginePower column."
$out += "//"
$out += "// $($rows.Count) of the package's $($pkg.items.Count) items. The rest are on bases this engine has"
$out += "// no unique_base_item value for; the generator names them when it runs."
$out += "//"
$out += "// No icons: sprites are still to come, so a unique wears its BASE item's sprite."
$out += ""
$out += "// clang-format off"
$out += $rows
$out += "// clang-format on"

Set-Content -Path $OutFile -Value $out -Encoding utf8
Write-Host "wrote $OutFile : $($rows.Count) uniques"
