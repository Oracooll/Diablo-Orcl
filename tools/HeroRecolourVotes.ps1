# Pipeline (RfA-28): tools/MakeRecolourFrames.ps1 cuts the reference plates from oracool_sprite_export sheets ->
# ChatGPT recolours them -> this script pairs the plates into votes (copy votes.json to tools/hero_recolour_votes.json,
# keeping only colour/share/pixels) -> node tools/GenHeroRecolour.js writes Source/oracool/hero_recolour_data.inc.
# RfA-28 intake: pairs every 6x6 block of each original plate with the same block of its recolour, maps the
# original colour back to its town-palette index, and measures how well "one colour per palette index" (per hero,
# per armour tier) reproduces the hand recolour. Writes votes.json for the table generator.
param([string]$Frames, [string]$Plates, [string]$Palette, [string]$Out)
Add-Type -AssemblyName System.Drawing
$ErrorActionPreference = 'Stop'

# town palette: "idx r g b" per line
$rgbToIndex = @{}
foreach ($line in Get-Content $Palette) {
    $p = ($line.Trim() -split '\s+')
    if ($p.Count -lt 4) { continue }
    $i = [int]$p[0]; $key = "{0},{1},{2}" -f $p[1], $p[2], $p[3]
    # player sprites live in 128-255; prefer those when a colour repeats
    if (-not $rgbToIndex.ContainsKey($key) -or $i -ge 128) { $rgbToIndex[$key] = $i }
}

$result = @{}
$summary = @()
foreach ($hero in 'barbarian', 'necromancer') {
    foreach ($tier in 'light', 'medium', 'heavy') {
        $orig = [System.Drawing.Bitmap]::FromFile((Join-Path $Frames "$hero-$tier-plate.png"))
        $rec = [System.Drawing.Bitmap]::FromFile((Join-Path $Plates "$hero-$tier-plate-recolour.png"))
        if ($orig.Width -ne $rec.Width -or $orig.Height -ne $rec.Height) { throw "$hero-$tier size differs" }
        $votes = @{}   # index -> @{ rgb -> count }
        $blocks = 0; $alphaMismatch = 0; $notFlat = 0; $unknown = 0
        for ($by = 0; $by -lt [int]($orig.Height / 6); $by++) {
            for ($bx = 0; $bx -lt [int]($orig.Width / 6); $bx++) {
                $o = $orig.GetPixel($bx * 6 + 3, $by * 6 + 3)
                $r = $rec.GetPixel($bx * 6 + 3, $by * 6 + 3)
                if (($o.A -eq 0) -ne ($r.A -eq 0)) { $alphaMismatch++; continue }
                if ($o.A -eq 0) { continue }
                $r2 = $rec.GetPixel($bx * 6, $by * 6)
                if ($r2.ToArgb() -ne $r.ToArgb()) { $notFlat++ }
                $blocks++
                $key = "{0},{1},{2}" -f $o.R, $o.G, $o.B
                if (-not $rgbToIndex.ContainsKey($key)) { $unknown++; continue }
                $idx = $rgbToIndex[$key]
                $target = '#{0:X2}{1:X2}{2:X2}' -f $r.R, $r.G, $r.B
                if (-not $votes.ContainsKey($idx)) { $votes[$idx] = @{} }
                $votes[$idx][$target] = 1 + [int]$votes[$idx][$target]
            }
        }
        $orig.Dispose(); $rec.Dispose()
        # how much the majority colour per index reproduces
        $agree = 0; $total = 0; $table = @{}
        foreach ($idx in $votes.Keys) {
            $best = $null; $bestN = 0; $n = 0
            foreach ($t in $votes[$idx].Keys) { $c = $votes[$idx][$t]; $n += $c; if ($c -gt $bestN) { $bestN = $c; $best = $t } }
            $agree += $bestN; $total += $n
            $table["$idx"] = @{ colour = $best; share = [math]::Round($bestN / $n, 3); pixels = $n; alternatives = $votes[$idx] }
        }
        $result["$hero-$tier"] = $table
        $summary += [pscustomobject]@{ plate = "$hero-$tier"; blocks = $blocks; indices = $votes.Count;
            tableReproduces = ('{0:P1}' -f ($agree / [math]::Max($total, 1))); alphaMismatch = $alphaMismatch; notFlat = $notFlat; unknownColour = $unknown }
    }
}
$result | ConvertTo-Json -Depth 6 | Set-Content -Path (Join-Path $Out 'votes.json') -Encoding utf8
$summary | Format-Table -AutoSize | Out-String -Width 200
