# BuildWiki.ps1 - generates the Diablo Orcl V1 wiki from the game's own source data.
#
# Run it after any change to the data tables and the wiki is current again:
#
#     powershell -ExecutionPolicy Bypass -File tools\BuildWiki.ps1
#
# Everything the wiki states about items, spells, skills, monsters and the loot mechanics is PARSED
# from Source/, never re-typed here. That is the whole design: a hand-written wiki is out of date the
# first time someone edits a table, and a wiki nobody trusts is worse than none. Prose that cannot be
# derived (what a mechanic is FOR, why a rule exists) lives in this script beside the data it
# annotates, so it travels with the thing it describes.

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$src = Join-Path $root 'Source'
$out = Join-Path $root 'wiki'

function Read-SourceFile([string]$relative) {
    return Get-Content (Join-Path $src $relative) -Raw -Encoding UTF8
}

# ---------------------------------------------------------------------------------------------
# Version and build identity
# ---------------------------------------------------------------------------------------------
$version = (Get-Content (Join-Path $root 'ORACOOL_VERSION') -Raw).Trim()
$generated = (Get-Date).ToString('yyyy-MM-dd HH:mm')

# ---------------------------------------------------------------------------------------------
# Base items - Source/itemdat.cpp
# ---------------------------------------------------------------------------------------------
$itemdat = Read-SourceFile 'itemdat.cpp'
$items = New-Object System.Collections.ArrayList

foreach ($line in ($itemdat -split "`n")) {
    if ($line -notmatch '^\s*/\*([^*]*)\*/\s*\{\s*(IDROP_\w+),') { continue }
    $enumName = $matches[1].Trim()
    $body = $line.Substring($line.IndexOf('{') + 1)
    $body = $body.Substring(0, $body.LastIndexOf('}'))

    # The two name columns are the only fields that can hold a comma-free quoted string; pull them
    # out first so the naive comma split below cannot trip over their contents.
    $name = ''
    if ($body -match 'N_\("([^"]*)"\)') { $name = $matches[1] }

    $parts = $body -split ','
    if ($parts.Count -lt 22) { continue } # 22 columns in ItemData; fewer means a row we cannot trust
    for ($i = 0; $i -lt $parts.Count; $i++) { $parts[$i] = $parts[$i].Trim() }

    $item = [ordered]@{
        id       = $enumName
        drop     = $parts[0] -replace 'IDROP_', ''
        class    = $parts[1] -replace 'ICLASS_', ''
        loc      = $parts[2] -replace 'ILOC_', ''
        curs     = $parts[3] -replace 'ICURS_', ''
        type     = $parts[4] -replace 'ItemType::', ''
        uitype   = $parts[5] -replace 'UITYPE_', ''
        name     = $name
        qlvl     = [int]$parts[8]
        dur      = [int]$parts[9]
        minDam   = [int]$parts[10]
        maxDam   = [int]$parts[11]
        minAC    = [int]$parts[12]
        maxAC    = [int]$parts[13]
        reqStr   = [int]$parts[14]
        reqMag   = [int]$parts[15]
        reqDex   = [int]$parts[16]
        misc     = ($parts[18] -replace 'IMISC_', '')
        spell    = ($parts[19] -replace 'SpellID::', '')
        usable   = ($parts[20] -eq 'true')
        value    = [int]($parts[21])
    }
    [void]$items.Add($item)
}

# ---------------------------------------------------------------------------------------------
# Spells - Source/spelldat.cpp, banded by Source/oracool/spell_ranks.cpp
# ---------------------------------------------------------------------------------------------
$spelldat = Read-SourceFile 'spelldat.cpp'
$spells = New-Object System.Collections.ArrayList
foreach ($line in ($spelldat -split "`n")) {
    if ($line -notmatch '^/\*SpellID::(\w+)\*/\s*\{') { continue }
    $id = $matches[1]
    $name = ''
    if ($line -match 'P_\("spell",\s*"([^"]*)"\)') { $name = $matches[1] }
    $body = $line.Substring($line.IndexOf('{') + 1)
    # Collapse P_("spell", "Name"): its comma would throw every positional column below off by one.
    # Strip the missile sub-array so the comma split lines up with the flat columns.
    $body = $body -replace 'P_\("[^"]*",\s*"[^"]*"\)', 'NAME'
    $body = $body -replace '\{[^{}]*\}', 'MISSILES'
    $parts = $body -split ','
    for ($i = 0; $i -lt $parts.Count; $i++) { $parts[$i] = $parts[$i].Trim() }
    if ($parts.Count -lt 14) { continue }
    $missiles = ''
    # The trailing comma inside the pair is real - "{ MissileID::Firebolt, MissileID::Null, }" - and
    # the first version of this pattern did not allow for it, so every spell showed no missiles.
    if ($line -match '\{\s*(MissileID::\w+),\s*(MissileID::\w+),?\s*\}') {
        $missiles = ($matches[1] -replace 'MissileID::', '') + ' / ' + ($matches[2] -replace 'MissileID::', '')
    }
    [void]$spells.Add([ordered]@{
            id       = $id
            name     = $name
            mana     = [int]$parts[4]
            type     = ($parts[5] -replace 'SpellDataFlags::', '')
            bookLvl  = [int]$parts[6]
            staffLvl = [int]$parts[7]
            minInt   = [int]$parts[8]
            missiles = $missiles
            manaAdj  = [int]$parts[10]
            minMana  = [int]$parts[11]
            bookCost = ([int]$parts[2]) * 10
        })
}

# The band table, read from the module that owns it rather than restated.
$ranks = Read-SourceFile 'oracool/spell_ranks.cpp'
$bandById = @{}
foreach ($line in ($ranks -split "`n")) {
    if ($line -match '^\s*(\d+),\s*//\s*(.+?)\s*$') {
        $bandById[$matches[2].Trim()] = [int]$matches[1]
    }
}
foreach ($s in $spells) {
    $band = 0
    foreach ($key in $bandById.Keys) {
        if ($key -eq $s.name -or $key -replace ' ', '' -eq $s.id) { $band = $bandById[$key]; break }
    }
    $s['band'] = $band
}

# Band 0 covers three different things, and calling them all "not a book spell" would be a half-truth.
# The class skills are RETIRED - they exist in the enum and are granted to nobody - while the Paladin
# skills are earned by character level and Town Portal is a belt ability. The reader needs the
# difference, so it is recorded here rather than guessed at in the page.
$retired = @('ItemRepair', 'TrapDisarm', 'StaffRecharge', 'Search', 'Identify', 'Rage')
$paladinSkills = @('Charge', 'Zeal', 'HammerOfFaith', 'BlessedShield', 'FistOfTheHeavens', 'ShieldBash', 'BlessedHammer')
foreach ($s in $spells) {
    if ($retired -contains $s.id) { $s['role'] = 'retired' }
    elseif ($paladinSkills -contains $s.id) { $s['role'] = 'class skill' }
    elseif ($s.id -eq 'TownPortal') { $s['role'] = 'belt ability' }
    elseif ($s.band -gt 0) { $s['role'] = 'book spell' }
    else { $s['role'] = 'unused' }
}

# ---------------------------------------------------------------------------------------------
# Class-tree skills - Source/oracool/class_tree.cpp
# ---------------------------------------------------------------------------------------------
$tree = Read-SourceFile 'oracool/class_tree.cpp'
$skills = New-Object System.Collections.ArrayList
$classMap = @{ 'Pal' = 'Paladin'; 'Bar' = 'Barbarian'; 'Sor' = 'Sorcerer'; 'Rog' = 'Rogue'; 'Bar_' = 'Barbarian'; 'Brd' = 'Bard'; 'Mnk' = 'Monk' }
$treeBody = $tree.Substring($tree.IndexOf('const ClassTreeSkillData Skills['))
$rowPattern = '\{\s*N_\("([^"]*)"\),\s*N_\("([^"]*)"\),\s*(\w+),\s*(\d+),\s*(\d+),\s*(\d+),\s*Kind::(\w+),\s*SpellID::(\w+),\s*(true|false)'
foreach ($m in [regex]::Matches($treeBody, $rowPattern)) {
    $cls = $m.Groups[3].Value
    $className = $cls
    if ($classMap.ContainsKey($cls)) { $className = $classMap[$cls] }
    [void]$skills.Add([ordered]@{
            name        = $m.Groups[1].Value
            description = $m.Groups[2].Value
            class       = $className
            page        = [int]$m.Groups[4].Value
            tier        = [int]$m.Groups[5].Value
            column      = [int]$m.Groups[6].Value
            kind        = $m.Groups[7].Value
            spell       = $m.Groups[8].Value
            implemented = ($m.Groups[9].Value -eq 'true')
        })
}

# ---------------------------------------------------------------------------------------------
# Monsters - Source/monstdat.cpp
# ---------------------------------------------------------------------------------------------
$mon = Read-SourceFile 'monstdat.cpp'
$monsters = New-Object System.Collections.ArrayList
$monBody = $mon.Substring($mon.IndexOf('const MonsterData MonstersData[]'))
$monBody = $monBody.Substring(0, $monBody.IndexOf('const UniqueMonsterData'))
foreach ($line in ($monBody -split "`n")) {
    if ($line -notmatch '^/\*\s*(MT_\w+)\s*\*/\s*\{') { continue }
    $id = $matches[1]
    $name = ''
    if ($line -match 'P_\("monster",\s*"([^"]*)"\)') { $name = $matches[1] }
    # Same two collapses as the spell table, for the same positional reason.
    $flat = $line -replace 'P_\("[^"]*",\s*"[^"]*"\)', 'NAME'
    $flat = $flat -replace '\{[^{}]*\}', 'ARR'
    $parts = $flat.Substring($flat.IndexOf('{') + 1) -split ','
    for ($i = 0; $i -lt $parts.Count; $i++) { $parts[$i] = $parts[$i].Trim() }
    if ($parts.Count -lt 30) { continue }
    [void]$monsters.Add([ordered]@{
            id       = $id
            name     = $name
            minLvl   = [int]$parts[11]
            maxLvl   = [int]$parts[12]
            level    = [int]$parts[13]
            hpMin    = [int]$parts[14]
            hpMax    = [int]$parts[15]
            toHit    = [int]$parts[19]
            minDam   = [int]$parts[21]
            maxDam   = [int]$parts[22]
            ac       = [int]$parts[27]
            class    = ($parts[28] -replace 'MonsterClass::', '')
            # The last column, taken by regex rather than by index. The split leaves the closing brace
            # on the final element ("54 }"), and casting that to int silently produced 0 - a whole
            # column of zeroes on the monsters page that looked entirely plausible until it was read.
            exp      = [int]$(if ($line -match ',\s*(\d+)\s*\},?\s*$') { $matches[1] } else { 0 })
        })
}

# ---------------------------------------------------------------------------------------------
# Mechanics constants, read from the modules that define them
# ---------------------------------------------------------------------------------------------
function Get-Constant([string]$text, [string]$pattern) {
    if ($text -match $pattern) { return $matches[1] }
    return '?'
}

$tiersCpp = Read-SourceFile 'oracool/item_tiers.cpp'
$areaH = Read-SourceFile 'oracool/area_level.h'
$ranksH = Read-SourceFile 'oracool/spell_ranks.h'
$pointsH = Read-SourceFile 'oracool/skill_points.h'
$playerH = Read-SourceFile 'player.h'

$mechanics = [ordered]@{
    maxAreaLevel      = 96
    floorsPerArea     = [int](Get-Constant $areaH 'constexpr int FloorsPerArea = (\d+)')
    areaCount         = [int](Get-Constant $areaH 'constexpr int AreaCount = (\d+)')
    areaFloorCount    = [int](Get-Constant $areaH 'constexpr int AreaFloorCount = (\d+)')
    maxSpellLevel     = [int](Get-Constant $playerH 'constexpr uint8_t MaxSpellLevel = (\d+)')
    maxCharacterLevel = [int](Get-Constant $playerH 'constexpr int MaxCharacterLevel = (\d+)')
    maxInvestment     = [int](Get-Constant $pointsH 'constexpr int MaxSkillInvestment = (\d+)')
    spellBands        = @(1, 6, 12, 18, 24, 30)
    tierScales        = @()
    tierWeights       = @()
}

foreach ($m in [regex]::Matches($tiersCpp, '\{\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+)\s*\},\s*//\s*(\w+)')) {
    $mechanics.tierScales += , [ordered]@{
        tier       = $m.Groups[5].Value
        power      = [int]$m.Groups[1].Value
        require    = [int]$m.Groups[2].Value
        durability = [int]$m.Groups[3].Value
        value      = [int]$m.Groups[4].Value
    }
}
if ($tiersCpp -match 'TierWeights\[BaseItemTierCount\] = \{\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+)') {
    $mechanics.tierWeights = @([int]$matches[1], [int]$matches[2], [int]$matches[3], [int]$matches[4])
}

# ---------------------------------------------------------------------------------------------
# INI options - Source/options.cpp's Oracool block
# ---------------------------------------------------------------------------------------------
$optionsCpp = Read-SourceFile 'options.cpp'
$options = New-Object System.Collections.ArrayList
foreach ($m in [regex]::Matches($optionsCpp, '\n\s*,\s*(\w+)\("([^"]+)",\s*OptionEntryFlags::\w+,\s*N_\("([^"]+)"\),\s*N_\("([^"]+)"\),\s*([^,\)]+)')) {
    [void]$options.Add([ordered]@{
            field       = $m.Groups[1].Value
            key         = $m.Groups[2].Value
            label       = $m.Groups[3].Value
            description = $m.Groups[4].Value
            default     = $m.Groups[5].Value.Trim()
        })
}

# ---------------------------------------------------------------------------------------------
# Art assets - what actually ships, measured rather than assumed
# ---------------------------------------------------------------------------------------------
Add-Type -AssemblyName System.Drawing
$assets = New-Object System.Collections.ArrayList
$assetRoot = Join-Path $root 'Packaging\resources\assets'
if (Test-Path $assetRoot) {
    foreach ($file in (Get-ChildItem $assetRoot -Recurse -Include *.png -ErrorAction SilentlyContinue)) {
        $w = 0; $h = 0
        try {
            $img = [System.Drawing.Image]::FromFile($file.FullName)
            $w = $img.Width; $h = $img.Height
            $img.Dispose()
        } catch { }
        $relative = $file.FullName.Substring($assetRoot.Length + 1).Replace('\', '/')
        [void]$assets.Add([ordered]@{
                path   = $relative
                width  = $w
                height = $h
                kb     = [math]::Round($file.Length / 1024, 1)
            })

        # COPIED into the wiki rather than linked across the repo. The wiki is served from its own
        # folder (and is meant to survive being zipped and sent somewhere), so a relative path out to
        # Packaging/ would resolve for exactly one way of opening it and break for the rest.
        $spriteDir = Join-Path $out 'sprites'
        $target = Join-Path $spriteDir $relative.Replace('/', '\')
        $targetDir = Split-Path -Parent $target
        if (-not (Test-Path $targetDir)) { New-Item -ItemType Directory -Path $targetDir -Force | Out-Null }
        Copy-Item $file.FullName $target -Force
    }
}

# ---------------------------------------------------------------------------------------------
# Dev reports - the project's own history
# ---------------------------------------------------------------------------------------------
$reports = New-Object System.Collections.ArrayList
$reportDir = Join-Path $root 'Diablo Orcl V1\02-Development-Reports'
if (Test-Path $reportDir) {
    foreach ($file in (Get-ChildItem $reportDir -Filter *.md | Sort-Object Name -Descending)) {
        $text = Get-Content $file.FullName -Raw -Encoding UTF8
        $ver = ''
        if ($text -match '\*\*Version:\*\*\s*([0-9.]+)') { $ver = $matches[1] }
        $title = $file.BaseName
        # An explicit order index. The files are named by date, and several land on the same day, so
        # sorting a table by the date column alone shuffles same-day reports into an arbitrary order -
        # which put 1.8.5 above 1.8.7 on the history page. This preserves the filename sort.
        [void]$reports.Add([ordered]@{ order = $reports.Count; title = $title; version = $ver; file = $file.Name })
    }
}

# ---------------------------------------------------------------------------------------------
# Emit
# ---------------------------------------------------------------------------------------------
if (-not (Test-Path $out)) { New-Item -ItemType Directory -Path $out | Out-Null }

$data = [ordered]@{
    version   = $version
    generated = $generated
    items     = $items
    spells    = $spells
    skills    = $skills
    monsters  = $monsters
    mechanics = $mechanics
    options   = $options
    assets    = $assets
    reports   = $reports
}

$json = $data | ConvertTo-Json -Depth 8 -Compress
Set-Content -Path (Join-Path $out 'data.js') -Value ("const WIKI = " + $json + ";") -Encoding UTF8

Write-Host ("wiki data: {0} items, {1} spells, {2} skills, {3} monsters, {4} options, {5} assets, {6} reports" -f `
        $items.Count, $spells.Count, $skills.Count, $monsters.Count, $options.Count, $assets.Count, $reports.Count)
