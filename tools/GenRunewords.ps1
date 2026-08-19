# GenRunewords.ps1 - the runeword pool, generated from a scheme and then tuned.
#
#     powershell -ExecutionPolicy Bypass -File tools\GenRunewords.ps1
#
# Emits Source/oracool/runewords_data.inc - several hundred words covering all ten non-jewelry
# equipment slots.
#
# Why generated: hand-authoring several hundred words is how a table becomes inconsistent - the
# fiftieth word gets numbers that do not line up with the fifth, and nobody notices until a build
# is unbalanced. The same call was made for the 73 item-set rungs and the 143 uniques. What is
# hand-authored here is the SCHEME: which hosts exist, how long a word may be, how a word's grant
# scales with the runes it costs, and the name pools.
#
# The rules the scheme encodes, all of them Diablo II's:
#   - A word forms only in a PLAIN, fully-socketed item of the right host, with the runes in order.
#   - Word length must equal the host's socket count EXACTLY, so the long words are automatically
#     the rare ones: only a 2x3 host (body armour, two-handers, staves) can hold six.
#   - A word's tier is its DEEPEST rune, and its grant scales with the total depth of its runes -
#     so a word always beats socketing the same runes loose, which is the whole reason to build one.

$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$out = Join-Path $root 'Source\oracool'

# The 33 runes in ladder order, read from the generator that owns them rather than retyped.
$orderInc = Get-Content (Join-Path $out 'runes_order.inc') -Raw -Encoding UTF8
$ladder = @([regex]::Matches($orderInc, 'IDI_ORACOOL_RUNE_(\w+),') | ForEach-Object { $_.Groups[1].Value })
if ($ladder.Count -ne 33) { throw "expected 33 runes in the ladder, found $($ladder.Count)" }

# The ten non-jewelry equipment slots, with the longest word each can hold. The cap is the slot's
# inventory footprint in 28x28 cells - the same number Sockets v2 uses for the socket ceiling, so a
# word can never demand more sockets than its host could ever have.
$hosts = @(
    @{ id = 'Weapon'; label = 'weapons'; maxLen = 6 },
    @{ id = 'Shield'; label = 'shields'; maxLen = 4 },
    @{ id = 'Body'; label = 'body armour'; maxLen = 6 },
    @{ id = 'Helm'; label = 'helms'; maxLen = 4 },
    @{ id = 'Shoulders'; label = 'shoulders'; maxLen = 4 },
    @{ id = 'Bracers'; label = 'bracers'; maxLen = 4 },
    @{ id = 'Gloves'; label = 'gloves'; maxLen = 4 },
    @{ id = 'Belt'; label = 'belts'; maxLen = 2 },
    @{ id = 'Legs'; label = 'legs'; maxLen = 4 },
    @{ id = 'Boots'; label = 'boots'; maxLen = 4 }
)

# Diablo II's own runeword names come first, as homage, spread across the hosts that suit them.
# After those run out the generator names words from two pools - the same two-part shape the 143
# uniques used, which produced a thousand usable names from seventy-six words.
# Diablo II's OWN runewords, with their OWN recipes - name, host, runes in order. These are
# authored, not generated: putting D2's names on generated sequences (which the first cut did) is
# worse than inventing names, because anyone who knows D2 reads "Steel" and expects Tir El.
#
# Words whose D2 recipe repeats a rune (Sanctuary's Ko Ko Mal, Last Wish, Infinity, Phoenix, Bone)
# are dropped by the filter below rather than altered - a repeated rune is legal in D2 but signals
# a stride bug in the generated half, and one rule for the whole table is worth more than a few
# extra homages.
$d2Words = @(
    @{ name = 'Steel'; host = 'Weapon'; runes = @('Tir', 'El') },
    @{ name = 'Nadir'; host = 'Helm'; runes = @('Nef', 'Tir') },
    @{ name = 'Malice'; host = 'Weapon'; runes = @('Ith', 'El', 'Eth') },
    @{ name = 'Stealth'; host = 'Body'; runes = @('Tal', 'Eth') },
    @{ name = 'Leaf'; host = 'Weapon'; runes = @('Tir', 'Ral') },
    @{ name = 'Zephyr'; host = 'Weapon'; runes = @('Ort', 'Eth') },
    @{ name = "Ancient's Pledge"; host = 'Shield'; runes = @('Ral', 'Ort', 'Tal') },
    @{ name = "King's Grace"; host = 'Weapon'; runes = @('Amn', 'Ral', 'Thul') },
    @{ name = 'Edge'; host = 'Weapon'; runes = @('Tir', 'Tal', 'Amn') },
    @{ name = 'Radiance'; host = 'Helm'; runes = @('Nef', 'Sol', 'Ith') },
    @{ name = 'Lore'; host = 'Helm'; runes = @('Ort', 'Sol') },
    @{ name = 'Rhyme'; host = 'Shield'; runes = @('Shael', 'Eth') },
    @{ name = 'Peace'; host = 'Body'; runes = @('Shael', 'Thul', 'Amn') },
    @{ name = 'Myth'; host = 'Body'; runes = @('Hel', 'Amn', 'Nef') },
    @{ name = 'Black'; host = 'Weapon'; runes = @('Thul', 'Io', 'Nef') },
    @{ name = 'White'; host = 'Weapon'; runes = @('Dol', 'Io') },
    @{ name = 'Smoke'; host = 'Body'; runes = @('Nef', 'Lum') },
    @{ name = 'Splendor'; host = 'Shield'; runes = @('Eth', 'Lum') },
    @{ name = 'Lionheart'; host = 'Body'; runes = @('Hel', 'Lum', 'Fal') },
    @{ name = 'Melody'; host = 'Weapon'; runes = @('Shael', 'Ko', 'Nef') },
    @{ name = 'Lawbringer'; host = 'Weapon'; runes = @('Amn', 'Lem', 'Ko') },
    @{ name = 'Duress'; host = 'Body'; runes = @('Shael', 'Um', 'Thul') },
    @{ name = 'Gloom'; host = 'Body'; runes = @('Fal', 'Um', 'Pul') },
    @{ name = 'Prudence'; host = 'Body'; runes = @('Mal', 'Tir') },
    @{ name = 'Wealth'; host = 'Body'; runes = @('Lem', 'Ko', 'Tir') },
    @{ name = 'Bramble'; host = 'Body'; runes = @('Ral', 'Ohm', 'Sur', 'Eth') },
    @{ name = 'Enigma'; host = 'Body'; runes = @('Jah', 'Ith', 'Ber') },
    @{ name = 'Principle'; host = 'Body'; runes = @('Ral', 'Gul', 'Eld') },
    @{ name = 'Passion'; host = 'Weapon'; runes = @('Dol', 'Ort', 'Eld', 'Lem') },
    @{ name = 'Chaos'; host = 'Weapon'; runes = @('Fal', 'Ohm', 'Um') },
    @{ name = 'Delirium'; host = 'Helm'; runes = @('Lem', 'Ist', 'Io') },
    @{ name = 'Fury'; host = 'Weapon'; runes = @('Jah', 'Gul', 'Eth') },
    @{ name = 'Kingslayer'; host = 'Weapon'; runes = @('Mal', 'Um', 'Gul', 'Fal') },
    @{ name = 'Rift'; host = 'Weapon'; runes = @('Hel', 'Ko', 'Lem', 'Gul') },
    @{ name = 'Oath'; host = 'Weapon'; runes = @('Shael', 'Pul', 'Mal', 'Lum') },
    @{ name = 'Spirit'; host = 'Shield'; runes = @('Tal', 'Thul', 'Ort', 'Amn') },
    @{ name = 'Beast'; host = 'Weapon'; runes = @('Ber', 'Tir', 'Um', 'Mal', 'Lum') },
    @{ name = 'Enlightenment'; host = 'Body'; runes = @('Pul', 'Ral', 'Sol') },
    @{ name = 'Obedience'; host = 'Weapon'; runes = @('Hel', 'Ko', 'Thul', 'Eth', 'Fal') },
    @{ name = 'Venom'; host = 'Weapon'; runes = @('Tal', 'Dol', 'Mal') },
    @{ name = 'Wrath'; host = 'Weapon'; runes = @('Pul', 'Lum', 'Ber', 'Mal') },
    @{ name = 'Exile'; host = 'Shield'; runes = @('Vex', 'Ohm', 'Ist', 'Dol') },
    @{ name = 'Famine'; host = 'Weapon'; runes = @('Fal', 'Ohm', 'Ort', 'Jah') },
    @{ name = 'Gospel'; host = 'Body'; runes = @('Lem', 'Ko', 'El') },
    @{ name = 'Hand of Justice'; host = 'Weapon'; runes = @('Sur', 'Cham', 'Amn', 'Lo') },
    @{ name = 'Heart of the Oak'; host = 'Weapon'; runes = @('Ko', 'Vex', 'Pul', 'Thul') },
    @{ name = 'Pride'; host = 'Weapon'; runes = @('Cham', 'Sur', 'Io', 'Lo') },
    @{ name = 'Dragon'; host = 'Body'; runes = @('Sur', 'Lo', 'Sol') },
    @{ name = 'Dream'; host = 'Helm'; runes = @('Io', 'Jah', 'Pul') },
    @{ name = 'Insight'; host = 'Weapon'; runes = @('Ral', 'Tir', 'Tal', 'Sol') },
    @{ name = 'Harmony'; host = 'Weapon'; runes = @('Tir', 'Ith', 'Sol', 'Ko') },
    @{ name = 'Ice'; host = 'Weapon'; runes = @('Amn', 'Shael', 'Jah', 'Lo') },
    @{ name = 'Faith'; host = 'Weapon'; runes = @('Ohm', 'Jah', 'Lem', 'Eld') },
    @{ name = 'Destruction'; host = 'Weapon'; runes = @('Vex', 'Lo', 'Ber', 'Jah', 'Ko') },
    @{ name = 'Doom'; host = 'Weapon'; runes = @('Hel', 'Ohm', 'Um', 'Lo', 'Cham') },
    @{ name = 'Call to Arms'; host = 'Weapon'; runes = @('Amn', 'Ral', 'Mal', 'Ist', 'Ohm') },
    @{ name = 'Brand'; host = 'Weapon'; runes = @('Jah', 'Lo', 'Mal', 'Gul') },
    @{ name = 'Death'; host = 'Weapon'; runes = @('Hel', 'El', 'Vex', 'Ort', 'Gul') },
    @{ name = 'Grief'; host = 'Weapon'; runes = @('Eth', 'Tir', 'Lo', 'Mal', 'Ral') },
    @{ name = 'Fortitude'; host = 'Body'; runes = @('El', 'Sol', 'Dol', 'Lo') },
    @{ name = 'Breath of the Dying'; host = 'Weapon'; runes = @('Vex', 'Hel', 'El', 'Eld', 'Zod', 'Eth') }
)
$d2Words = @($d2Words | Where-Object { ($_.runes | Select-Object -Unique).Count -eq $_.runes.Count })

$firstWords = @(
    'Ashen', 'Bitter', 'Bleak', 'Burning', 'Cinder', 'Cold', 'Crimson', 'Dread', 'Dusk', 'Ember',
    'Fell', 'Grave', 'Grim', 'Hollow', 'Iron', 'Kindled', 'Lost', 'Molten', 'Pale', 'Quiet',
    'Ragged', 'Riven', 'Salt', 'Shattered', 'Silent', 'Sable', 'Storm', 'Sunless', 'Thorn',
    'Tidal', 'Vigil', 'Waking', 'Wan', 'Wither', 'Wrought'
)
$secondWords = @(
    'Accord', 'Anthem', 'Bargain', 'Bulwark', 'Cadence', 'Charter', 'Compass', 'Covenant', 'Creed',
    'Dirge', 'Ember', 'Errand', 'Fetter', 'Gambit', 'Harbour', 'Herald', 'Keening', 'Lantern',
    'Ledger', 'Litany', 'Mandate', 'Mercy', 'Omen', 'Pact', 'Parable', 'Reckoning', 'Refrain',
    'Sentinel', 'Signet', 'Sojourn', 'Tally', 'Tenet', 'Threnody', 'Verdict', 'Vigil', 'Warrant'
)

$usedNames = New-Object System.Collections.Generic.HashSet[string]
$nameQueue = New-Object System.Collections.Queue


function Get-Name([int]$seed) {
    if ($nameQueue.Count -gt 0) {
        $candidate = $nameQueue.Dequeue()
        if ($usedNames.Add($candidate)) { return $candidate }
    }
    # Deterministic two-part fallback: walk both pools with co-prime strides so the pairs do not
    # repeat until both pools have been exhausted against each other.
    for ($attempt = 0; $attempt -lt 4000; $attempt++) {
        $i = ($seed + $attempt * 7) % $firstWords.Count
        $j = ($seed + $attempt * 13) % $secondWords.Count
        $candidate = "$($firstWords[$i]) $($secondWords[$j])"
        if ($usedNames.Add($candidate)) { return $candidate }
    }
    throw 'ran out of runeword names - widen the pools'
}

# How many words each (host, length) pair gets. Short words are the common early finds, so there
# are more of them; a six-rune word is an endgame chase and there are few.
$countsByLength = @{ 2 = 12; 3 = 10; 4 = 9; 5 = 5; 6 = 4 }

$rows = New-Object System.Collections.ArrayList
$seed = 0
$total = 0
$takenSequences = New-Object System.Collections.Generic.HashSet[string]

# One row builder, so an authored word and a generated one cannot end up shaped differently.
function New-Row($name, $hostId, $positions) {
    $deepest = ($positions | Measure-Object -Maximum).Maximum
    $depth = ($positions | Measure-Object -Sum).Sum
    $runeList = @($positions | ForEach-Object { "IDI_ORACOOL_RUNE_$($script:ladder[$_].ToUpper())" })
    $len = $runeList.Count
    while ($runeList.Count -lt 6) { $runeList += '0' }

    $unit = [Math]::Max(1, [int][Math]::Round($depth / 4.0))
    $dmgPct = 0; $dmgMod = 0; $toHit = 0; $res = 0; $ac = 0; $spell = 0; $mana = 0; $life = 0
    switch ($hostId) {
        'Weapon' { $dmgPct = $unit * 3; $dmgMod = $unit; $toHit = $unit * 2 }
        'Shield' { $res = [Math]::Min(40, $unit); $ac = $unit * 3; $life = $unit * 2 }
        'Body' { $ac = $unit * 4; $res = [Math]::Min(35, [int]($unit * 0.8)); $life = $unit * 3 }
        'Helm' { $ac = $unit * 2; $mana = $unit * 3; $spell = [int]($deepest / 16) }
        'Shoulders' { $ac = $unit * 2; $life = $unit * 2; $res = [Math]::Min(25, [int]($unit * 0.6)) }
        'Bracers' { $toHit = $unit * 2; $ac = $unit; $mana = $unit * 2 }
        'Gloves' { $toHit = $unit * 3; $dmgMod = $unit; $ac = $unit }
        'Belt' { $life = $unit * 4; $ac = $unit }
        'Legs' { $ac = $unit * 3; $life = $unit * 2 }
        'Boots' { $ac = $unit * 2; $life = $unit; $res = [Math]::Min(25, [int]($unit * 0.6)) }
    }
    return ("`t{{ N_(`"{0}`"), static_cast<uint8_t>(RunewordHost::{1}), {2}, {{ {3} }}, {4}, {5}, {6}, {7}, {8}, {9}, {10}, {11} }}, // deepest {12}" -f `
            $name, $hostId, $len, ($runeList -join ', '), $dmgPct, $dmgMod, $toHit, $res, $ac, $spell, $mana, $life, $script:ladder[$deepest])
}

function Get-SequenceKey($hostId, $positions) { return "$hostId|" + ($positions -join ',') }

# Diablo II's own words first, with their own recipes, so a player who knows D2 finds what they
# expect. Anything the generator later produces that would collide with one of these is skipped.
foreach ($word in $d2Words) {
    # The ladder is captured from the generated include, where the names are UPPERCASE; the
    # authored list above is written the way a player reads them.
    $positions = @($word.runes | ForEach-Object { [array]::IndexOf($ladder, $_.ToUpper()) })
    if ($positions -contains -1) { throw "unknown rune in $($word.name)" }
    if ($positions.Count -gt 6) { continue }  # longer than any host can hold
    $key = Get-SequenceKey $word.host $positions
    if (-not $takenSequences.Add($key)) { continue }
    [void]$usedNames.Add($word.name)
    [void]$rows.Add((New-Row $word.name $word.host $positions))
    $total++
}

foreach ($slot in $hosts) {
    for ($len = 2; $len -le $slot.maxLen; $len++) {
        $count = $countsByLength[$len]
        for ($k = 0; $k -lt $count; $k++) {
            # The stride must be CO-PRIME with 33, or the walk revisits ladder positions inside one
            # word: a stride of 11 has order 3, which made every six-rune word an X Y Z X Y Z
            # repeat. 7, 8, 10, 13 and 14 share no factor with 33 (= 3 x 11).
            $stride = @{ 2 = 7; 3 = 8; 4 = 10; 5 = 13; 6 = 14 }[$len]
            $anchor = [int](($k * 33.0) / $count)
            $positions = @()
            for ($r = 0; $r -lt $len; $r++) {
                $positions += (($anchor + $r * $stride) % 33)
            }
            $key = Get-SequenceKey $slot.id $positions
            if (-not $takenSequences.Add($key)) { continue }
            $name = Get-Name $seed
            $seed++
            [void]$rows.Add((New-Row $name $slot.id $positions))
            $total++
        }
    }
}
$header = @(
    '// GENERATED by tools/GenRunewords.ps1 - do not edit by hand.',
    "// $total runewords across the ten non-jewelry equipment slots.",
    '//',
    '// Word length equals the host socket count exactly (Diablo II''s rule), so a six-rune word can',
    '// only form in a 2x3 host and the long words are the rare ones by construction. Each word''s',
    '// grant scales with the total ladder depth of its runes, which is what makes building a word',
    '// worth more than socketing the same runes loose.',
    ''
)
Set-Content -Path (Join-Path $out 'runewords_data.inc') -Value ($header + $rows) -Encoding UTF8
Write-Host ("generated {0} runewords across {1} hosts" -f $total, $hosts.Count)
