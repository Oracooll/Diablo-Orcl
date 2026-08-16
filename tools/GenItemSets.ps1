# Generates Source/oracool/item_sets_data.inc from the fifteen delivered set-data.json files.
#
# The sets are CONTENT, delivered as data (Oracool.MPQ/02-source-art/item-sets). Ninety-four items
# with names, slots, base types, requirements and stats, plus tiered set bonuses, is far too much to
# retype into C++ by hand and keep correct - and retyping would also make the JSON a stale copy of
# the truth rather than the source of it.
#
# So the table is GENERATED, and this script is the only place the two representations meet. Re-run
# it whenever a set-data.json changes:
#
#     pwsh -File tools\GenItemSets.ps1
#
# Stat keywords are resolved against Source/oracool/item_set_stats.cpp - that table decides what a
# keyword means, and this script only decides how its VALUE is packed into an ItemPower. A keyword
# the table does not know is a hard error here rather than a silent omission.

param(
    [string]$SetsRoot = "..\Oracool.MPQ\02-source-art\item-sets",
    [string]$OutFile  = "Source\oracool\item_sets_data.inc",
    [string]$StatTable = "Source\oracool\item_set_stats.cpp"
)

$ErrorActionPreference = "Stop"

# --- read the keyword -> IPL mapping straight out of the C++ table, so the two cannot drift -------
$mapping = @{}
foreach ($line in Get-Content $StatTable) {
    if ($line -match '^\s*\{\s*"([a-z_]+)",\s*(Power|Approx|Inert),\s*(IPL_[A-Z0-9_]+)') {
        $mapping[$Matches[1]] = @{ Fidelity = $Matches[2]; Power = $Matches[3] }
    }
}
Write-Host "stat table: $($mapping.Count) keywords"
if ($mapping.Count -lt 100) { throw "only $($mapping.Count) keywords parsed from $StatTable - the row format changed?" }

# --- the archives are zips; work from whatever is extracted beside them, else extract to temp -----
$work = Join-Path ([System.IO.Path]::GetTempPath()) "oracool-item-sets"
if (Test-Path $work) { Remove-Item -Recurse -Force $work }
New-Item -ItemType Directory -Force $work | Out-Null
foreach ($zip in Get-ChildItem (Join-Path $SetsRoot "set-*.zip") | Sort-Object Name) {
    Expand-Archive -Path $zip.FullName -DestinationPath $work -Force
}

# --- value packing ------------------------------------------------------------------------------
# The data writes values five ways: "+8", "-1", "+20%", "[1,3]" and "true". Everything lands in an
# ItemPower's two int parameters.
function ConvertTo-Params([string]$raw, [string]$power) {
    $v = $raw.Trim()
    if ($v -eq "true")  { return @(1, 1) }
    if ($v -eq "false") { return @(0, 0) }
    # The speed stats are declared BOTH ways in the delivered data - "+10%" on some items and the
    # engine's own tier word ("quick", "fast") on others. The word is the more faithful of the two,
    # since these really are discrete tiers, so it is taken literally.
    if ($power -eq "IPL_FASTATTACK") {
        switch ($v) { "quick" { return @(1, 0) } "fast" { return @(2, 0) } "faster" { return @(3, 0) } "fastest" { return @(4, 0) } }
    }
    if ($power -eq "IPL_FASTRECOVER") {
        switch ($v) { "fast" { return @(1, 0) } "faster" { return @(2, 0) } "fastest" { return @(3, 0) } }
    }
    if ($power -eq "IPL_FASTBLOCK" -or $power -eq "IPL_ONEHAND") {
        # Neither takes a magnitude: the flag's presence IS the effect.
        return @(1, 0)
    }
    if ($v -match '^\[\s*(-?\d+)\s*,\s*(-?\d+)\s*\]$') { return @([int]$Matches[1], [int]$Matches[2]) }
    if ($v -match '^([+-]?\d+)\s*%?$') {
        $n = [int]$Matches[1]
        # IPL_FASTATTACK and IPL_FASTRECOVER carry a discrete TIER, not a percentage: SaveItemPower
        # reads param1 as 1..4 (Quick/Fast/Faster/Fastest) and 1..3 respectively. A declared "+10%"
        # is not ten of anything here - it is the gentlest tier.
        if ($power -eq "IPL_FASTATTACK")  { return @([Math]::Max(1, [Math]::Min(4, [int][Math]::Round($n / 10.0))), 0) }
        if ($power -eq "IPL_FASTRECOVER") { return @([Math]::Max(1, [Math]::Min(3, [int][Math]::Round($n / 10.0))), 0) }
        # IPL_GETHIT SUBTRACTS its parameter (items.cpp: `_iPLGetHit -= r`), so the data's negative
        # "damage_taken_flat:-1" means "reduce by one" and has to arrive positive.
        if ($power -eq "IPL_GETHIT") { return @([Math]::Abs($n), [Math]::Abs($n)) }
        return @($n, $n)
    }
    return $null   # a name, e.g. proc:cinderbrand - only ever reached on inert keywords
}

function Escape-Cpp([string]$s) { return $s.Replace('\', '\\').Replace('"', '\"') }

# Twenty of the ninety-four items declare `"durability": "indestructible"` rather than a number.
# The engine already has a sentinel for exactly this - items.h's DUR_INDESTRUCTIBLE, 255 - which
# every durability path already understands, so it needs no new concept.
function ConvertTo-Durability($raw) {
    if ($raw -is [string] -and $raw.Trim() -eq "indestructible") { return 255 }
    return [int]$raw
}

# --- walk the sets -------------------------------------------------------------------------------
$setRows = @()
$itemRows = @()
$bonusRows = @()
$itemIndex = 0
$bonusIndex = 0
$inertSeen = 0
$liveSeen = 0

foreach ($dir in Get-ChildItem $work -Directory | Sort-Object Name) {
    $json = Get-Content (Join-Path $dir.FullName "set-data.json") -Raw | ConvertFrom-Json
    $firstItem = $itemIndex
    $firstBonus = $bonusIndex

    foreach ($it in $json.items) {
        $powers = @()
        foreach ($stat in $it.stats) {
            $parts = $stat -split ':', 2
            $kw = $parts[0]
            $val = if ($parts.Count -gt 1) { $parts[1] } else { "" }
            if (-not $mapping.ContainsKey($kw)) { throw "item $($it.id): unknown stat keyword '$kw'" }
            $m = $mapping[$kw]
            if ($m.Fidelity -eq "Inert") { $inertSeen++; continue }
            $p = ConvertTo-Params $val $m.Power
            if ($null -eq $p) { throw "item $($it.id): stat '$stat' maps to $($m.Power) but its value is not a number" }
            $liveSeen++
            $powers += "{ $($m.Power), $($p[0]), $($p[1]) }"
        }
        # ItemPower slots are fixed-width; six is what UniqueItem carries and no delivered item
        # declares more than five LIVE stats.
        if ($powers.Count -gt 6) { throw "item $($it.id) has $($powers.Count) live powers, more than the six slots" }
        $powerList = ($powers -join ", ")
        while ($powers.Count -lt 6) { $powers += "{ IPL_INVALID, 0, 0 }" }

        $dmgMin = 0; $dmgMax = 0; $acMin = 0; $acMax = 0
        if ($it.damage) { $dmgMin = $it.damage[0]; $dmgMax = $it.damage[1] }
        if ($it.armor)  { $acMin  = $it.armor[0];  $acMax  = $it.armor[1] }

        $itemRows += ("`t{ `"$(Escape-Cpp $it.id)`", N_(`"$(Escape-Cpp $it.name)`"), `"$(Escape-Cpp $it.slot)`", `"$(Escape-Cpp $it.baseType)`", " +
            "$([int]$it.requiredLevel), $([int]$it.requiredStrength), $(ConvertTo-Durability $it.durability), " +
            "{ $([int]$it.grid[0]), $([int]$it.grid[1]) }, $dmgMin, $dmgMax, $acMin, $acMax, " +
            "{ $($powers -join ', ') } },")
        $itemIndex++
    }

    foreach ($b in $json.bonuses) {
        $powers = @()
        foreach ($stat in $b.stats) {
            $parts = $stat -split ':', 2
            $kw = $parts[0]
            $val = if ($parts.Count -gt 1) { $parts[1] } else { "" }
            if (-not $mapping.ContainsKey($kw)) { throw "bonus $($b.name): unknown stat keyword '$kw'" }
            $m = $mapping[$kw]
            if ($m.Fidelity -eq "Inert") { $inertSeen++; continue }
            $p = ConvertTo-Params $val $m.Power
            if ($null -eq $p) { throw "bonus $($b.name): stat '$stat' maps to $($m.Power) but its value is not a number" }
            $liveSeen++
            $powers += "{ $($m.Power), $($p[0]), $($p[1]) }"
        }
        if ($powers.Count -gt 4) { throw "bonus $($b.name) has $($powers.Count) live powers, more than the four slots" }
        while ($powers.Count -lt 4) { $powers += "{ IPL_INVALID, 0, 0 }" }
        $bonusRows += "`t{ $([int]$b.pieces), N_(`"$(Escape-Cpp $b.name)`"), { $($powers -join ', ') } },"
        $bonusIndex++
    }

    $setRows += ("`t{ `"$(Escape-Cpp $json.id)`", N_(`"$(Escape-Cpp $json.name)`"), $([int]$json.requiredLevel), " +
        "$firstItem, $($itemIndex - $firstItem), $firstBonus, $($bonusIndex - $firstBonus) },")
}

# --- emit ----------------------------------------------------------------------------------------
$out = @()
$out += "// GENERATED by tools/GenItemSets.ps1 - do not edit."
$out += "//"
$out += "// Source: the fifteen set-data.json files under Oracool.MPQ/02-source-art/item-sets."
$out += "// Keyword meanings come from oracool/item_set_stats.cpp; this file only carries the values."
$out += "//"
$out += "// $($setRows.Count) sets, $($itemRows.Count) items, $($bonusRows.Count) bonus tiers."
$out += "// $liveSeen stat lines compiled to a power; $inertSeen were inert and are deliberately absent."
$out += ""
$out += "// clang-format off"
$out += "const SetItemDefinition ItemSetItems[] = {"
$out += $itemRows
$out += "};"
$out += ""
$out += "const SetBonusDefinition ItemSetBonuses[] = {"
$out += $bonusRows
$out += "};"
$out += ""
$out += "const ItemSetDefinition ItemSets[] = {"
$out += $setRows
$out += "};"
$out += "// clang-format on"

Set-Content -Path $OutFile -Value $out -Encoding utf8
Write-Host "wrote $OutFile : $($setRows.Count) sets, $($itemRows.Count) items, $($bonusRows.Count) bonuses"
Write-Host "stat lines: $liveSeen live, $inertSeen inert"
