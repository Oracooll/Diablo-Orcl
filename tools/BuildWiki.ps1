# BuildWiki.ps1 - generates the Diablo Orcl V1 wiki from the game's own source data.
#
# Run it after any change to the data tables and the wiki is current again:
#
#     powershell -ExecutionPolicy Bypass -File tools\BuildWiki.ps1
#     powershell -ExecutionPolicy Bypass -File tools\BuildWiki.ps1 -NoBundle
#
# Everything the wiki states about items, spells, skills, monsters and the loot mechanics is PARSED
# from Source/, never re-typed here. That is the whole design: a hand-written wiki is out of date the
# first time someone edits a table, and a wiki nobody trusts is worse than none. Prose that cannot be
# derived (what a mechanic is FOR, why a rule exists) lives in this script beside the data it
# annotates, so it travels with the thing it describes.

param(
    # Skip the single-file bundle. See the note beside the call at the end of this script.
    [switch] $NoBundle
)

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
# The rune rows live in generated includes rather than inline (tools/GenRunes.ps1 owns all 33), so
# they are concatenated here - otherwise the wiki's item count silently drops by the five that
# moved out and never gains the 28 that arrived, which is exactly what happened on the first run.
foreach ($inc in @('oracool\runes_shipped_data.inc', 'oracool\runes_data.inc')) {
    $itemdat += "`n" + (Get-Content (Join-Path $src $inc) -Raw -Encoding UTF8)
}
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
# Character classes and the experience ladder - Source/playerdat.cpp
# ---------------------------------------------------------------------------------------------

# The life and mana columns are in the engine's 1/64 fixed point, written either as an explicit
# "(18 << 6)" or as "static_cast<int>(5.5F * 64)". Both are converted to whole points here, which is
# the unit a player counts in and the unit the character sheet shows.
function ConvertFixed([string]$raw) {
    $text = $raw.Trim()
    if ($text -match '^\(?\s*(-?\d+)\s*<<\s*6\s*\)?$') { return [double]$matches[1] }
    if ($text -match '^([0-9.]+)x64$') { return [double]$matches[1] }
    if ($text -match '^\(?\s*(-?\d+)\s*\)?$') { return [double]$matches[1] / 64.0 }
    return 0
}

$playerdat = Read-SourceFile 'playerdat.cpp'
$classes = New-Object System.Collections.ArrayList
# Bounded to the PlayersData table. The sprite table below it repeats the same "/* HeroClass::X */"
# comment on every row, so an unbounded walk found each class twice - twelve classes for six.
$classBody = $playerdat.Substring($playerdat.IndexOf('const PlayerData PlayersData[]'))
$classBody = $classBody.Substring(0, $classBody.IndexOf('};'))
foreach ($line in ($classBody -split "`n")) {
    if ($line -notmatch '^/\* HeroClass::(\w+)\s*\*/\s*\{') { continue }
    $enum = $matches[1]
    $name = ''
    if ($line -match 'N_\("([^"]*)"\)') { $name = $matches[1] }
    $flat = $line -replace 'N_\("[^"]*"\)', 'NAME'
    $flat = $flat -replace 'static_cast<int>\(([0-9.]+)F \* 64\)', '$1x64'
    $parts = $flat.Substring($flat.IndexOf('{') + 1) -split ','
    for ($i = 0; $i -lt $parts.Count; $i++) { $parts[$i] = $parts[$i].Trim() }
    if ($parts.Count -lt 20) { continue }

    [void]$classes.Add([ordered]@{
            enum      = $enum
            name      = $name
            # Positional from className: 0 name, 1 sprite path, 2 str, 3 mag, 4 dex, 5 vit,
            # 6-9 the maxima, 10 block bonus, 11-18 the life and mana curves, 19 the class skill.
            baseStr   = [int]$parts[2]
            baseMag   = [int]$parts[3]
            baseDex   = [int]$parts[4]
            baseVit   = [int]$parts[5]
            maxStr    = [int]$parts[6]
            maxMag    = [int]$parts[7]
            maxDex    = [int]$parts[8]
            maxVit    = [int]$parts[9]
            blockBonus = [int]$parts[10]
            startLife = ConvertFixed $parts[11]
            startMana = ConvertFixed $parts[12]
            lifePerLevel = ConvertFixed $parts[13]
            manaPerLevel = ConvertFixed $parts[14]
            lifePerVit   = ConvertFixed $parts[15]
            manaPerMag   = ConvertFixed $parts[16]
            skill     = ($parts[19] -replace 'SpellID::', '' -replace '\s*\}.*$', '')
        })
}

$expTable = New-Object System.Collections.ArrayList
$expBody = $playerdat.Substring($playerdat.IndexOf('const uint64_t ExpLvlsTbl'))
$expBody = $expBody.Substring($expBody.IndexOf('{') + 1)
$expBody = $expBody.Substring(0, $expBody.IndexOf('};'))
$level = 1
foreach ($m in [regex]::Matches($expBody, '(\d[\d]*)')) {
    [void]$expTable.Add([ordered]@{ level = $level; toNext = [int64]$m.Groups[1].Value })
    $level++
}

# ---------------------------------------------------------------------------------------------
# Affixes - Source/itemdat.cpp's prefix and suffix tables
# ---------------------------------------------------------------------------------------------
$affixes = New-Object System.Collections.ArrayList
foreach ($table in @('ItemPrefixes', 'ItemSuffixes')) {
    $kind = if ($table -eq 'ItemPrefixes') { 'prefix' } else { 'suffix' }
    $body = $itemdat.Substring($itemdat.IndexOf("const PLStruct $table[]"))
    $body = $body.Substring(0, $body.IndexOf('};'))
    # One row per line, read positionally after the two brace groups. A single regex over the whole
    # row was tried first and could not be trusted: PLIType is an OR-chain of AffixItemType flags
    # padded with runs of spaces, so a lazy match ran past the field and a greedy one swallowed the
    # next row. Splitting the line at its known landmarks is duller and correct.
    foreach ($line in ($body -split "`n")) {
        if ($line -notmatch '^\{\s*N_\("([^"]*)"\),\s*\{\s*(IPL_\w+),\s*(-?\d+),\s*(-?\d+)\s*\},\s*(-?\d+),\s*(.+)$') { continue }
        # Copied out BEFORE the second -match runs. $matches is a single automatic variable that each
        # match overwrites, so reading the first pattern's groups afterwards silently returns the
        # second pattern's - which is how "false" ended up being cast to an int here.
        $affixName = $matches[1]
        $affixPower = $matches[2] -replace '^IPL_', ''
        $affixMin = [int]$matches[3]
        $affixMax = [int]$matches[4]
        $affixMinLvl = [int]$matches[5]
        $tail = $matches[6]
        $types = ($tail -split ',\s*GOE_')[0]

        $align = ''; $double = $false; $good = $false; $minVal = 0; $maxVal = 0; $mult = 0
        if ($tail -match 'GOE_(\w+),\s*(true|false),\s*(true|false),\s*(-?\d+),\s*(-?\d+),\s*(-?\d+)') {
            $align = $matches[1]
            $double = ($matches[2] -eq 'true')
            $good = ($matches[3] -eq 'true')
            $minVal = [int]$matches[4]
            $maxVal = [int]$matches[5]
            $mult = [int]$matches[6]
        }

        [void]$affixes.Add([ordered]@{
                kind   = $kind
                name   = $affixName
                power  = $affixPower
                min    = $affixMin
                max    = $affixMax
                minLvl = $affixMinLvl
                types  = (($types -replace 'AffixItemType::', '') -replace '\s+', ' ').Trim()
                align  = $align
                double = $double
                good   = $good
                minVal = $minVal
                maxVal = $maxVal
                mult   = $mult
            })
    }
}

# ---------------------------------------------------------------------------------------------
# Unique items - the generated include
# ---------------------------------------------------------------------------------------------
$uniques = New-Object System.Collections.ArrayList
$uniquePath = Join-Path $src 'oracool/unique_items_data.inc'
if (Test-Path $uniquePath) {
    foreach ($line in (Get-Content $uniquePath -Encoding UTF8)) {
        if ($line -notmatch '\{\s*N_\("([^"]*)"\),\s*(UITYPE_\w+),\s*(\d+),\s*(\d+),\s*(\d+),') { continue }
        $powers = New-Object System.Collections.ArrayList
        foreach ($p in [regex]::Matches($line, '\{\s*(IPL_\w+),\s*(-?\d+),\s*(-?\d+)\s*\}')) {
            $type = $p.Groups[1].Value
            if ($type -eq 'IPL_INVALID' -or $type -eq 'IPL_INVCURS') { continue }
            [void]$powers.Add(($type -replace '^IPL_', '') + ' ' + $p.Groups[2].Value)
        }
        [void]$uniques.Add([ordered]@{
                name    = $matches[1]
                base    = ($matches[2] -replace '^UITYPE_', '')
                minLvl  = [int]$matches[3]
                powerCount = [int]$matches[4]
                value   = [int]$matches[5]
                powers  = ($powers -join ', ')
            })
    }
}

# ---------------------------------------------------------------------------------------------
# Item sets - the generated include
# ---------------------------------------------------------------------------------------------
$setItems = New-Object System.Collections.ArrayList
$setsPath = Join-Path $src 'oracool/item_sets_data.inc'
if (Test-Path $setsPath) {
    foreach ($line in (Get-Content $setsPath -Encoding UTF8)) {
        if ($line -notmatch '\{\s*"(SET_\w+)",\s*N_\("([^"]*)"\),\s*"(\w+)",\s*"(\w+)",\s*(\d+),\s*(\d+),\s*(\d+),') { continue }
        $powers = New-Object System.Collections.ArrayList
        foreach ($p in [regex]::Matches($line, '\{\s*(IPL_\w+),\s*(-?\d+),\s*(-?\d+)\s*\}')) {
            if ($p.Groups[1].Value -eq 'IPL_INVALID') { continue }
            [void]$powers.Add(($p.Groups[1].Value -replace '^IPL_', '') + ' ' + $p.Groups[2].Value)
        }
        # The set's display name is the item name's possessive prefix - "Vhal's Blackened Halo" is a
        # piece of Vhal's set - so it is derived rather than restated in a second table.
        $setName = $matches[1] -replace '^SET_', '' -replace '_', ' '
        [void]$setItems.Add([ordered]@{
                set     = (Get-Culture).TextInfo.ToTitleCase($setName.ToLower())
                name    = $matches[2]
                slot    = $matches[3]
                base    = ($matches[4] -replace '_', ' ')
                qlvl    = [int]$matches[5]
                ac      = [int]$matches[6]
                dur     = [int]$matches[7]
                powers  = ($powers -join ', ')
            })
    }
}

# ---------------------------------------------------------------------------------------------
# Quests and shrines
# ---------------------------------------------------------------------------------------------
$questsCpp = Read-SourceFile 'quests.cpp'
$quests = New-Object System.Collections.ArrayList
$questBody = $questsCpp.Substring($questsCpp.IndexOf('QuestData QuestsData[]'))
$questBody = $questBody.Substring(0, $questBody.IndexOf('};'))
foreach ($m in [regex]::Matches($questBody, '\{\s*(-?\d+),\s*(-?\d+),\s*(\w+),\s*(-?\d+),\s*(\d+),\s*(\w+),\s*(true|false),\s*(\w+),\s*N_\(\s*(?:/\*[^*]*\*/\s*)?"([^"]*)"\s*\)')) {
    [void]$quests.Add([ordered]@{
            level      = [int]$m.Groups[1].Value
            levelType  = ($m.Groups[3].Value -replace 'DTYPE_', '')
            setLevel   = ($m.Groups[6].Value -replace 'SL_', '')
            singlePlayer = ($m.Groups[7].Value -eq 'true')
            name       = $m.Groups[9].Value
        })
}

$objectsCpp = Read-SourceFile 'objects.cpp'
$shrines = New-Object System.Collections.ArrayList
if ($objectsCpp -match '(?s)const char \*const ShrineNames\[\] = \{(.*?)\};') {
    $shrineBody = $matches[1]
    $shrineIndex = 0
    foreach ($m in [regex]::Matches($shrineBody, 'N_\("([^"]*)"\)')) {
        [void]$shrines.Add([ordered]@{ index = $shrineIndex; name = $m.Groups[1].Value })
        $shrineIndex++
    }
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
$resistH = Read-SourceFile 'oracool/player_resistance.h'

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

# The resistance curve, DERIVED rather than typed - the wiki page states these numbers, and the
# standing complaint about this wiki is hand-typed values that drift from the source. Changing the
# constants in player_resistance.h is enough; nothing here needs editing with them.
$mechanics.resistance = [ordered]@{
    softCap   = [int](Get-Constant $resistH 'constexpr int ResistanceHardCap = (\d+)')
    hardCap   = [int](Get-Constant $resistH 'constexpr int ResistanceHardCap = (\d+)')
    divisor   = [int](Get-Constant $resistH 'constexpr int ResistanceSoftCapDivisor = (\d+)')
    penalties = @()
}
# The soft cap is written as `= MaxResistance`, so it comes from player.h rather than from its own
# literal - following the alias instead of re-typing 75 here.
$mechanics.resistance.softCap = [int](Get-Constant $playerH 'constexpr int MaxResistance = (\d+)')
if ($resistH -match 'ResistancePenaltyPerDifficulty\[\]\s*=\s*\{([^}]*)\}') {
    $mechanics.resistance.penalties = @($matches[1] -split ',' | ForEach-Object { [int]$_.Trim() })
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


# The band -> book-ilvl map. Fourth hand-typed table found inside the generated wiki (after the
# quality curves, the crafting recipes and the runeword grid size), so it is parsed now: the values
# happened to be right, but "happened to be right" is not the standard this generator exists for.
$ranksCpp = Read-SourceFile 'oracool/spell_ranks.cpp'
$mechanics.bookItemLevel = [ordered]@{}
if ($ranksCpp -match '(?s)int SpellBookItemLevel\(SpellID spell\)(.*?)\n\}') {
    foreach ($m in [regex]::Matches($matches[1], 'case (\d+):\s*\r?\n\s*return (\d+);')) {
        $mechanics.bookItemLevel[$m.Groups[1].Value] = [int]$m.Groups[2].Value
    }
}

# The per-band quality scales. These were hand-copied into affixes.html's JS, which is exactly the
# drift the reaudit is meant to catch - a retune in item_tiers.cpp would have left the wiki quoting
# the old curve with no way to notice.
$mechanics.qualityBands = [ordered]@{}
foreach ($name in @('Rare', 'BuffedUnique', 'Primal')) {
    if ($tiersCpp -match ("{0}ByBand\[BaseItemTierCount\] = \{{\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+)" -f $name)) {
        $mechanics.qualityBands[$name] = @([int]$matches[1], [int]$matches[2], [int]$matches[3], [int]$matches[4])
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
# The socket economy - Source/oracool/gems.cpp, charms.cpp, runewords.cpp, and the drop hook
# ---------------------------------------------------------------------------------------------
# Gems and runes share one effect table in gems.cpp, told apart by the index constant's own name.
# The rows use designated initialisers, which is exactly why they can be parsed at all: every
# number arrives labelled, so nothing here depends on column order.
$gemsCpp = Read-SourceFile 'oracool/gems.cpp'
$charmsCpp = Read-SourceFile 'oracool/charms.cpp'
$wordsCpp = Read-SourceFile 'oracool/runewords.cpp'
$itemsCpp2 = Read-SourceFile 'items.cpp'

function Get-PrettyName([string]$constant) {
    $bare = $constant -replace '^IDI_ORACOOL_(GEM|RUNE|CHARM)_', ''
    $bare = $bare -replace '_(CHIPPED|FLAWED|NORMAL|FLAWLESS|PERFECT)$', ''
    return ($bare.Substring(0, 1) + $bare.Substring(1).ToLower())
}

$gems = New-Object System.Collections.ArrayList
$runes = New-Object System.Collections.ArrayList
if ($gemsCpp -match '(?s)constexpr GemData Gems\[\] = \{(.*?)\n\};') {
    $tableBody = $matches[1]
    foreach ($row in [regex]::Matches($tableBody, '(?s)\{\s*\.idx = (IDI_ORACOOL_\w+)(.*?)\}')) {
        $idx = $row.Groups[1].Value
        $fieldText = $row.Groups[2].Value
        $fields = [ordered]@{}
        foreach ($f in [regex]::Matches($fieldText, '\.(\w+) = (-?\d+|true|false)')) {
            $raw = $f.Groups[2].Value
            if ($raw -eq 'true') { $fields[$f.Groups[1].Value] = $true }
            elseif ($raw -eq 'false') { $fields[$f.Groups[1].Value] = $false }
            else { $fields[$f.Groups[1].Value] = [int]$raw }
        }
        $entry = [ordered]@{ name = (Get-PrettyName $idx); constant = $idx; fields = $fields }
        if ($idx -like '*_RUNE_*') { [void]$runes.Add($entry) } else { [void]$gems.Add($entry) }
    }
}


# The other 28 runes live in their own generated include rather than inline in gems.cpp, so the
# table walk above cannot see them - it stops at the file's own rows. Same row shape, same parse.
$runesEffectsInc = Get-Content (Join-Path $src 'oracool\runes_effects.inc') -Raw -Encoding UTF8
foreach ($row in [regex]::Matches($runesEffectsInc, '(?s)\{\s*\.idx = (IDI_ORACOOL_\w+)(.*?)\}')) {
    $idx = $row.Groups[1].Value
    $fields = [ordered]@{}
    foreach ($f in [regex]::Matches($row.Groups[2].Value, '\.(\w+) = (-?\d+|true|false|ItemSpecialEffect::\w+)')) {
        $raw = $f.Groups[2].Value
        if ($raw -eq 'true') { $fields[$f.Groups[1].Value] = $true }
        elseif ($raw -eq 'false') { $fields[$f.Groups[1].Value] = $false }
        elseif ($raw -like 'ItemSpecialEffect::*') { $fields[$f.Groups[1].Value] = $raw.Substring('ItemSpecialEffect::'.Length) }
        else { $fields[$f.Groups[1].Value] = [int]$raw }
    }
    [void]$runes.Add([ordered]@{ name = (Get-PrettyName $idx); constant = $idx; fields = $fields })
}

# Depth and price, off the generated data rows - both files, so the five shipped runes and the 28
# appended ones read from the same place the game does.
$runeQlvl = @{}
foreach ($file in @('runes_shipped_data.inc', 'runes_data.inc')) {
    $text = Get-Content (Join-Path $src "oracool\$file") -Raw -Encoding UTF8
    foreach ($m in [regex]::Matches($text, 'IDI_ORACOOL_RUNE_(\w+)\*/.*?N_\("Rune"\),\s*(\d+),.*?false,\s*(\d+)')) {
        $key = $m.Groups[1].Value
        $runeQlvl[$key] = @{ qlvl = [int]$m.Groups[2].Value; value = [int]$m.Groups[3].Value }
    }
}
foreach ($rune in $runes) {
    $key = $rune.constant -replace '^IDI_ORACOOL_RUNE_', ''
    if ($runeQlvl.ContainsKey($key)) {
        $rune.qlvl = $runeQlvl[$key].qlvl
        $rune.value = $runeQlvl[$key].value
    }
}

# The ladder order - the enum interleaves runes with charms and the gem ladder, so this is the only
# correct sequence, and it is what the wiki must present them in.
$orderInc = Get-Content (Join-Path $src 'oracool\runes_order.inc') -Raw -Encoding UTF8
$runeOrder = @([regex]::Matches($orderInc, 'IDI_ORACOOL_RUNE_(\w+),') | ForEach-Object { $_.Groups[1].Value })
$ordered = New-Object System.Collections.ArrayList
foreach ($key in $runeOrder) {
    $match = $runes | Where-Object { ($_.constant -replace '^IDI_ORACOOL_RUNE_', '') -eq $key }
    if ($match) { [void]$ordered.Add($match) }
}
$runes = $ordered

$gemQualities = New-Object System.Collections.ArrayList
foreach ($q in [regex]::Matches($gemsCpp, 'case GemQuality::(\w+):\s*\r?\n\s*return (\d+);')) {
    [void]$gemQualities.Add([ordered]@{ name = $q.Groups[1].Value; percent = [int]$q.Groups[2].Value })
}

$charms = New-Object System.Collections.ArrayList
if ($charmsCpp -match '(?s)constexpr CharmData Charms\[\] = \{(.*?)\n\};') {
    foreach ($row in [regex]::Matches($matches[1], '\{\s*(IDI_ORACOOL_CHARM_\w+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+),\s*(\d+)\s*\}')) {
        [void]$charms.Add([ordered]@{
                name          = (Get-PrettyName $row.Groups[1].Value)
                life          = [int]$row.Groups[2].Value
                fireResist    = [int]$row.Groups[3].Value
                lightResist   = [int]$row.Groups[4].Value
                toHit         = [int]$row.Groups[5].Value
                magicFind     = [int]$row.Groups[6].Value
                goldFind      = [int]$row.Groups[7].Value
            })
    }
}
$charmCap = [int](Get-Constant (Read-SourceFile 'oracool/charms.h') 'CharmActiveCap = (\d+)')

$runewords = New-Object System.Collections.ArrayList
if ($wordsCpp -match '(?s)constexpr RunewordDefinition Runewords\[\] = \{(.*?)\n\};') {
    $pattern = '(?s)N_\("([^"]+)"\),\s*static_cast<uint8_t>\(SocketHost::(\w+)\),\s*(\d+),\s*\{([^}]*)\},\s*([-\d,\s]*?)\}'
    foreach ($row in [regex]::Matches($matches[1], $pattern)) {
        $runeList = @()
        foreach ($r in [regex]::Matches($row.Groups[4].Value, 'IDI_ORACOOL_RUNE_(\w+)')) {
            $runeList += ($r.Groups[1].Value.Substring(0, 1) + $r.Groups[1].Value.Substring(1).ToLower())
        }
        $nums = @($row.Groups[5].Value -split ',' | ForEach-Object { $_.Trim() } | Where-Object { $_ -match '^-?\d+$' } | ForEach-Object { [int]$_ })
        while ($nums.Count -lt 8) { $nums += 0 }
        [void]$runewords.Add([ordered]@{
                name        = $row.Groups[1].Value
                host        = $row.Groups[2].Value
                sockets     = [int]$row.Groups[3].Value
                runes       = ($runeList -join ' ')
                damagePct   = $nums[0]
                damageMod   = $nums[1]
                toHit       = $nums[2]
                allResist   = $nums[3]
                armor       = $nums[4]
                spellLevels = $nums[5]
                mana        = $nums[6]
                life        = $nums[7]
            })
    }
}

# The drop hook's own numbers, read out of items.cpp so tuning them updates the wiki with them.
$socketRules = [ordered]@{
    gemDropPercent    = [int](Get-Constant $itemsCpp2 'constexpr int GemDropPercent = (\d+)')
    charmDropPercent  = [int](Get-Constant $itemsCpp2 'constexpr int CharmDropPercent = (\d+)')
    runeDropPercent   = [int](Get-Constant $itemsCpp2 'constexpr int RuneDropPercent = (\d+)')
    socketedPercent   = [int](Get-Constant $itemsCpp2 '(?s)void TryAddSocketsToDroppedItem.*?GenerateRnd\(100\) >= (\d+)')
    socketWeights     = @()
    qualityWeights    = @(40, 30, 18, 9, 3)
    etherealPercent   = [int](Get-Constant $itemsCpp2 '(?s)void TryMakeDroppedItemEthereal.*?GenerateRnd\(100\) >= (\d+)')
    etherealBonusPct  = 135
    charmActiveCap    = $charmCap
    levskiGridColumns = [int](Get-Constant (Read-SourceFile 'oracool/levski_roar.h') 'LevskiGridColumns = (\d+)')
    levskiGridRows    = [int](Get-Constant (Read-SourceFile 'oracool/levski_roar.h') 'LevskiGridRows = (\d+)')
    recipes           = @()
}


# The recipes, parsed from crafting.cpp's own name and input tables rather than retyped - the
# hand-written list said three recipes and "Sol is the top of the ladder" long after neither was
# true, which is precisely the drift this generator exists to prevent.
$craftingCpp = Read-SourceFile 'oracool/crafting.cpp'
$recipeNames = @{}
$recipeInputs = @{}
if ($craftingCpp -match '(?s)const char \*CraftingRecipeName\(int index\)(.*?)\n\}') {
    foreach ($m in [regex]::Matches($matches[1], 'case (\d+):\s*\r?\n\s*return N_\("([^"]+)"\)')) {
        $recipeNames[[int]$m.Groups[1].Value] = $m.Groups[2].Value
    }
}
if ($craftingCpp -match '(?s)const char \*CraftingRecipeInputs\(int index\)(.*?)\n\}') {
    foreach ($m in [regex]::Matches($matches[1], 'case (\d+):\s*\r?\n\s*return N_\("([^"]+)"\)')) {
        $recipeInputs[[int]$m.Groups[1].Value] = $m.Groups[2].Value
    }
}
$recipeRows = New-Object System.Collections.ArrayList
foreach ($key in ($recipeNames.Keys | Sort-Object)) {
    [void]$recipeRows.Add([ordered]@{ name = $recipeNames[$key]; formula = $recipeInputs[$key] })
}
$socketRules.recipes = $recipeRows

# The socket-count weights, read out of the drop hook so retuning them retunes the wiki.
if ($itemsCpp2 -match 'constexpr int SocketWeights\[Item::MaxItemSockets\] = \{([^}]*)\}') {
    $socketRules.socketWeights = @($matches[1] -split ',' | ForEach-Object { $_.Trim() } |
        Where-Object { $_ -match '^\d+$' } | ForEach-Object { [int]$_ })
}


# ---------------------------------------------------------------------------------------------
# Runewords - Source/oracool/runewords_data.inc (GENERATED; see tools/GenRunewords.ps1)
# ---------------------------------------------------------------------------------------------
$runewordsInc = Get-Content (Join-Path $src 'oracool\runewords_data.inc') -Raw -Encoding UTF8
$runewords = New-Object System.Collections.ArrayList
$runeNameByConst = @{}
foreach ($rune in $runes) { $runeNameByConst[$rune.constant] = $rune.name }

$pattern = '(?s)N_\("([^"]+)"\),\s*static_cast<uint8_t>\(RunewordHost::(\w+)\),\s*(\d+),\s*\{([^}]*)\},\s*([-\d,\s]*?)\}'
foreach ($m in [regex]::Matches($runewordsInc, $pattern)) {
    $sequence = @()
    foreach ($r in [regex]::Matches($m.Groups[4].Value, 'IDI_ORACOOL_RUNE_\w+')) {
        $name = $runeNameByConst[$r.Value]
        if ($name) { $sequence += $name }
    }
    $nums = @($m.Groups[5].Value -split ',' | ForEach-Object { $_.Trim() } |
        Where-Object { $_ -match '^-?\d+$' } | ForEach-Object { [int]$_ })
    while ($nums.Count -lt 8) { $nums += 0 }

    $grants = @()
    if ($nums[0]) { $grants += "+$($nums[0])% damage" }
    if ($nums[1]) { $grants += "+$($nums[1]) damage" }
    if ($nums[2]) { $grants += "+$($nums[2])% to hit" }
    if ($nums[3]) { $grants += "+$($nums[3])% all resists" }
    if ($nums[4]) { $grants += "+$($nums[4]) armour" }
    if ($nums[5]) { $grants += "+$($nums[5]) to spell levels" }
    if ($nums[6]) { $grants += "+$($nums[6]) mana" }
    if ($nums[7]) { $grants += "+$($nums[7]) life" }

    # The word's tier is its deepest rune - that is what gates when it can be built at all.
    $deepest = ''
    $deepestRung = -1
    foreach ($runeName in $sequence) {
        $rung = [array]::IndexOf(@($runes | ForEach-Object { $_.name }), $runeName)
        if ($rung -gt $deepestRung) { $deepestRung = $rung; $deepest = $runeName }
    }

    [void]$runewords.Add([ordered]@{
            name    = $m.Groups[1].Value
            host    = $m.Groups[2].Value
            sockets = [int]$m.Groups[3].Value
            runes   = ($sequence -join ' ')
            deepest = $deepest
            grants  = ($grants -join ', ')
        })
}

# ---------------------------------------------------------------------------------------------
# Pipeline - the vault's own backlog table (Diablo Orcl V1/07-Backlog/Pipeline.md)
# ---------------------------------------------------------------------------------------------
# Parsed rather than retyped, and the vault file is the source: the user edits it in Obsidian and
# the page follows. The old Idea-Backlog.md is NOT read - a dozen of its "not started" entries had
# shipped without being moved, which is exactly the staleness this page exists to avoid.
$pipeline = New-Object System.Collections.ArrayList
$pipelinePath = Join-Path $root 'Diablo Orcl V1\07-Backlog\Pipeline.md'
if (Test-Path $pipelinePath) {
    foreach ($line in (Get-Content $pipelinePath -Encoding UTF8)) {
        if ($line -notmatch '^\|') { continue }
        if ($line -match '^\|\s*-{2,}' -or $line -match '^\|\s*Name\s*\|') { continue }
        $cells = @($line.Trim('|') -split '\|' | ForEach-Object { $_.Trim() })
        if ($cells.Count -lt 6) { continue }
        [void]$pipeline.Add([ordered]@{
                name    = $cells[0]
                group   = $cells[1]
                size    = $cells[2]
                save    = $cells[3]
                blocked = $cells[4]
                summary = $cells[5]
            })
    }
}
# ---------------------------------------------------------------------------------------------
# Debug console commands - Source/debug.cpp
# ---------------------------------------------------------------------------------------------
$debugCpp = Read-SourceFile 'debug.cpp'
$debugCmds = New-Object System.Collections.ArrayList
if ($debugCpp -match '(?s)std::vector<DebugCmdItem> DebugCmdList = \{(.*?)\n\};') {
    foreach ($row in [regex]::Matches($matches[1], '\{\s*"(\w+)",\s*"([^"]*)",\s*"([^"]*)",\s*&\w+\s*\}')) {
        [void]$debugCmds.Add([ordered]@{
                command = $row.Groups[1].Value
                args    = $row.Groups[3].Value
                summary = $row.Groups[2].Value
            })
    }
}

# ---------------------------------------------------------------------------------------------
# Autosave triggers - the declarations in Source/oracool/auto_save.h are the list
# ---------------------------------------------------------------------------------------------
$autoSaveH = Read-SourceFile 'oracool/auto_save.h'
$autoSaveTriggers = New-Object System.Collections.ArrayList
foreach ($t in [regex]::Matches($autoSaveH, 'void ScheduleAutoSaveFor(\w+)\(\);')) {
    # "ForStorePurchase" -> "Store purchase": the declaration names ARE the trigger list, so this
    # cannot drift from the code the way a hand-typed list would.
    $words = [regex]::Replace($t.Groups[1].Value, '(?<!^)([A-Z])', ' $1')
    [void]$autoSaveTriggers.Add(($words.Substring(0, 1) + $words.Substring(1).ToLower()))
}

# ---------------------------------------------------------------------------------------------
# Emit
# ---------------------------------------------------------------------------------------------
if (-not (Test-Path $out)) { New-Item -ItemType Directory -Path $out | Out-Null }

$data = [ordered]@{
    version   = $version
    generated = $generated
    classes   = $classes
    expTable  = $expTable
    affixes   = $affixes
    uniques   = $uniques
    setItems  = $setItems
    quests    = $quests
    shrines   = $shrines
    items     = $items
    spells    = $spells
    skills    = $skills
    monsters  = $monsters
    mechanics = $mechanics
    options   = $options
    gems      = $gems
    runes     = $runes
    gemQualities = $gemQualities
    charms    = $charms

    runewords = $runewords
    pipeline  = $pipeline
    socketRules = $socketRules
    debugCmds = $debugCmds
    autoSaveTriggers = $autoSaveTriggers
    assets    = $assets
    reports   = $reports
}

$json = $data | ConvertTo-Json -Depth 8 -Compress
Set-Content -Path (Join-Path $out 'data.js') -Value ("const WIKI = " + $json + ";") -Encoding UTF8

Write-Host ("           {0} runewords, {1} pipeline entries" -f $runewords.Count, $pipeline.Count)
Write-Host ("wiki data: {0} items, {1} spells, {2} skills, {3} monsters, {4} options, {5} assets, {6} reports" -f `
        $items.Count, $spells.Count, $skills.Count, $monsters.Count, $options.Count, $assets.Count, $reports.Count)
Write-Host ("           {0} classes, {1} exp rows, {2} affixes, {3} uniques, {4} set items, {5} quests, {6} shrines" -f `
        $classes.Count, $expTable.Count, $affixes.Count, $uniques.Count, $setItems.Count, $quests.Count, $shrines.Count)

# The bundle is part of the wiki, not a separate deliverable, so building one without the other is
# never what anyone wanted. On 2026-08-19 the multi-page wiki was rebuilt alone and the single-file
# bundle kept serving the previous Pipeline text - no error, no warning, nothing to notice.
#
# -NoBundle exists for the case that actually needs it: BundleWiki.ps1 inlines megabytes of sprites,
# so a run that only wants data.js refreshed can skip it. Anyone who does is choosing the drift, and
# tools\BundleWiki.ps1 -Verify will say so.
if ($NoBundle) {
    Write-Host "wiki bundle: SKIPPED (-NoBundle). tools\BundleWiki.ps1 -Verify will report it stale."
} else {
    & (Join-Path $PSScriptRoot 'BundleWiki.ps1')
}
