# Builds the Diablo Orcl V1 skills/spells/auras reference workbook via Excel COM.
#
# Every number is TRANSCRIBED FROM SOURCE, never estimated:
#   inventory        - wiki/data.js, which tools/BuildWiki.ps1 parses from class_tree.cpp and spelldat.cpp
#   per-rank effects - ApplyAura / ApplyPassive / ProcessClassTreeTick, Source/oracool/class_tree.cpp
#   aura radius      - AuraRadiusForPoints, Source/oracool/aura_field.cpp
#   spell damage     - GetDamageAmtAtLevel, Source/missiles.cpp
#   spell mana       - GetManaAmount, Source/spells.cpp
# Where the code has no per-rank formula, the sheet says so rather than inventing one.

$ErrorActionPreference = 'Stop'
$repo = 'C:\Users\hroga\OneDrive\2. Personal Files\Software\Diablo\Diablo Orcl V1'
$out  = Join-Path $repo 'Diablo Orcl V1 - Skills, Spells and Auras.xlsx'

$txt = [System.IO.File]::ReadAllText((Join-Path $repo 'wiki\data.js'))
$s = $txt.IndexOf('{'); $e = $txt.LastIndexOf('}')
$W = $txt.Substring($s, $e - $s + 1) | ConvertFrom-Json
$version = $W.version

# Scaled(p, base, perPoint) = base + perPoint * (p - 1). base is what the FIRST point buys;
# perPoint is what each one after adds. base=0/per=0 marks a flag or a one-rank row: it does
# not scale, and its rank cells read "-" rather than a fabricated curve.
$SCALED = @{
  'Might'            = @(@('Damage bonus (%)',20,10,''))
  'Holy Fire'        = @(@('Fire damage, min',2,1,''), @('Fire damage, max',6,4,''))
  'Blessed Aim'      = @(@('To-hit bonus',15,7,''))
  'Concentration'    = @(@('Damage bonus (%)',15,8,''), @('Fastest hit recovery',0,0,'flag - on from the first point, does not scale'))
  'Holy Shock'       = @(@('Lightning damage, min',1,0,'flat +1 at any rank'), @('Lightning damage, max',10,6,''))
  'Fanaticism'       = @(@('To-hit bonus',10,5,''), @('Damage bonus (%)',10,5,''), @('Fast attack',0,0,'flag - on from the first point, does not scale'))
  'Resist Fire'      = @(@('Fire resistance (%)',15,4,''))
  'Defiance'         = @(@('Armour class',25,12,''))
  'Resist Cold'      = @(@('Magic resistance (%)',15,4,'cold maps to magic - this engine has no cold channel'))
  'Resist Lightning' = @(@('Lightning resistance (%)',15,4,''))
  'Salvation'        = @(@('Fire resistance (%)',10,3,''), @('Lightning resistance (%)',10,3,''), @('Magic resistance (%)',10,3,''))
  'Prayer'           = @(@('Life restored per tick',2,2,'ProcessClassTreeTick - only while below full life'))
  'Meditation'       = @(@('Mana restored per tick',2,2,'ProcessClassTreeTick - only while below full mana'))
  'Thorns'           = @(@('Thorns',0,0,'flag - on from the first point, does not scale'))
  'Vigor'            = @(@('Run speed',0,0,'skips a walk frame - on from the first point, does not scale'))
  'Sanctuary'        = @(@('Repels undead',0,0,'within the aura radius; non-unique undead only'))
  'Conviction'       = @(@('Strips monster resistances',0,0,'breaks IMMUNITIES from 5 points (ConvictionBreaksImmunityAt)'))
  'Battle Hymn'      = @(@('To-hit bonus',12,6,''), @('Damage bonus (%)',12,6,''))
  'Song of Swiftness'= @(@('Fast attack',0,0,'flag, plus a skipped run frame - does not scale'))
  'Song of Fortitude'= @(@('Armour class',20,10,''), @('Fire resistance (%)',8,3,''), @('Lightning resistance (%)',8,3,''), @('Magic resistance (%)',8,3,''))
  'Tale of Heroes'   = @(@('Strength',4,2,''), @('Dexterity',4,2,''))
  'Melody of Life'   = @(@('Life restored per tick',2,2,'ProcessClassTreeTick - only while below full life'))
  'Inspiration'      = @(@('Mana restored per tick',2,2,'ProcessClassTreeTick - only while below full mana'))
  'Healing Mantra'   = @(@('Life restored per tick',2,2,'ProcessClassTreeTick - only while below full life'))
  'Sword Mastery'    = @(@('To-hit bonus',10,5,'only while a sword is held'), @('Damage bonus (%)',10,6,'only while a sword is held'))
  'Axe Mastery'      = @(@('To-hit bonus',10,5,'only while an axe is held'), @('Damage bonus (%)',10,6,'only while an axe is held'))
  'Mace Mastery'     = @(@('To-hit bonus',10,5,'only while a mace is held'), @('Damage bonus (%)',10,6,'only while a mace is held'))
  'Pole Arm Mastery' = @(@('To-hit bonus',10,5,'only while a staff is held'), @('Damage bonus (%)',10,6,'only while a staff is held'))
  'Iron Skin'        = @(@('Armour class',20,10,''))
  'Natural Resistance'= @(@('Fire resistance (%)',8,3,''), @('Lightning resistance (%)',8,3,''), @('Magic resistance (%)',8,3,''))
  'Increased Speed'  = @(@('Run speed',0,0,'skips a walk frame - on from the first point, does not scale'))
  'Critical Strike'  = @(@('Damage bonus (%)',12,6,'flat, in place of D2''s crit roll - no crit exists in this engine'))
  'Penetrate'        = @(@('To-hit bonus',12,6,''))
  'Warmth'           = @(@('Mana restored per tick',2,2,'ProcessClassTreeTick - only while below full mana'))
  'Enchant'          = @(@('Fire damage, min',2,1,''), @('Fire damage, max',5,3,''))
  'Fire Mastery'     = @(@('Fire damage, min',3,2,''), @('Fire damage, max',7,4,''), @('Fire resistance (%)',5,2,''))
  'Lightning Mastery'= @(@('Lightning damage, min',1,1,''), @('Lightning damage, max',10,6,''), @('Lightning resistance (%)',5,2,''))
  'Master of the Long Staff' = @(@('Damage bonus (%)',10,0,'one-rank row: flat, only while a staff is held'), @('To-hit bonus',15,0,'one-rank row: flat, only while a staff is held'))
  'Flowing Step'     = @(@('Run speed',0,0,'skips a walk frame; the D2 evade half has no channel in this engine'))
  'Iron Robe'        = @(@('Damage taken reduction',3,3,'3 per rank, CAPPED AT 20; halved in light armour, off entirely in medium or heavy'))
  'Perfect Vessel'   = @(@('Life',0,0,'one-rank row: +10% of base max life, plus fast hit recovery'))
  'Enlightenment'    = @(@('Mana',0,0,'one-rank row: +10% of base max mana, plus +10 to all three resistances'))
}

# GetDamageAmtAtLevel, missiles.cpp. sl = spell level, clvl = character level, mag = magic.
# Scale(x, sl) = ScaleSpellEffect: adds x/8 once per level, ~+12.5% compounding with integer
# truncation. An empty second field means SPELL LEVEL DOES NOT CHANGE THE DAMAGE.
$DAMAGE = @{
  'Firebolt'   = @('min = mag/8 + sl + 1;  max = min + 9','+1 min and +1 max per level')
  'Healing'    = @('min = clvl + sl + 1;  max = 4*clvl + 6*sl + 10','+1 min and +6 max per level')
  'HealOther'  = @('min = clvl + sl + 1;  max = 4*clvl + 6*sl + 10','+1 min and +6 max per level')
  'Flash'      = @('min = Scale(clvl, sl) * 1.5;  max = min * 2','x1.125 compounding per level')
  'Fireball'   = @('base = 2*clvl + 4;  min = Scale(base, sl);  max = Scale(base + 36, sl)','x1.125 compounding per level')
  'RuneOfFire' = @('base = 2*clvl + 4;  min = Scale(base, sl);  max = Scale(base + 36, sl)','x1.125 compounding per level')
  'Guardian'   = @('base = clvl/2 + 1;  min = Scale(base, sl);  max = Scale(base + 9, sl)','x1.125 compounding per level')
  'Nova'       = @('min = Scale((clvl+5)/2, sl) * 5;  max = Scale((clvl+30)/2, sl) * 5','x1.125 compounding per level')
  'Immolation' = @('min = Scale((clvl+5)/2, sl) * 5;  max = Scale((clvl+30)/2, sl) * 5','x1.125 compounding per level')
  'RuneOfImmolation' = @('min = Scale((clvl+5)/2, sl) * 5;  max = Scale((clvl+30)/2, sl) * 5','x1.125 compounding per level')
  'RuneOfNova' = @('min = Scale((clvl+5)/2, sl) * 5;  max = Scale((clvl+30)/2, sl) * 5','x1.125 compounding per level')
  'Elemental'  = @('min = Scale(2*clvl + 4, sl);  max = Scale(2*clvl + 40, sl)','x1.125 compounding per level')
  'BloodStar'  = @('min = max = mag/2 + 3*sl - mag/8','+3 per level')
  'Lightning'  = @('min = 2;  max = 2 + clvl','')
  'RuneOfLight'= @('min = 2;  max = 2 + clvl','')
  'FireWall'   = @('min = 2*clvl + 4;  max = min + 36','')
  'LightningWall' = @('min = 2*clvl + 4;  max = min + 36','')
  'RingOfFire' = @('min = 2*clvl + 4;  max = min + 36','')
  'ChainLightning' = @('min = 4;  max = 4 + 2*clvl','')
  'FlameWave'  = @('min = 6*(clvl + 1);  max = min + 54','')
  'Inferno'    = @('min = 3;  max = (clvl + 4) * 1.5','')
  'Golem'      = @('min = 11;  max = 17','')
  'Apocalypse' = @('min = clvl;  max = 6*clvl','')
  'ChargedBolt'= @('min = 1;  max = 1 + mag/4','')
  'HolyBolt'   = @('min = clvl + 9;  max = min + 9','')
  'BoneSpirit' = @('one third of the target''s current health','')
}

$TIER = @(1,6,12,18,24,30,36)   # class_tree.cpp TierLevels
$RANKS = @(1,2,3,5,10,20)

$xl = New-Object -ComObject Excel.Application
$xl.Visible = $false
$xl.DisplayAlerts = $false
$wb = $xl.Workbooks.Add()
while ($wb.Sheets.Count -gt 1) { $wb.Sheets.Item($wb.Sheets.Count).Delete() }

function Set-Header($ws, $row, $names, $widths) {
  for ($i = 0; $i -lt $names.Count; $i++) {
    $c = $ws.Cells.Item($row, $i + 1)
    $c.Value2 = $names[$i]
    $ws.Columns.Item($i + 1).ColumnWidth = $widths[$i]
  }
  $rng = $ws.Range($ws.Cells.Item($row,1), $ws.Cells.Item($row,$names.Count))
  $rng.Interior.Color = 0x64381F      # BGR: dark navy
  $rng.Font.Color = 0xFFFFFF
  $rng.Font.Bold = $true
  $rng.WrapText = $true
  $rng.HorizontalAlignment = -4108
  $rng.VerticalAlignment = -4108
  $ws.Rows.Item($row).RowHeight = 32
  $ws.Activate(); $ws.Application.ActiveWindow.FreezePanes = $false
  $ws.Cells.Item($row + 1, 1).Select()
  $ws.Application.ActiveWindow.FreezePanes = $true
}

function Write-Block($ws, $top, $rows, $cols) {
  if ($rows.Count -eq 0) { return }
  $arr = New-Object 'object[,]' $rows.Count, $cols
  for ($r = 0; $r -lt $rows.Count; $r++) {
    for ($c = 0; $c -lt $cols; $c++) { $arr[$r, $c] = $rows[$r][$c] }
  }
  # .Formula, not .Value2: it always parses US English syntax, so a leading "=" becomes a real
  # formula whatever the machine's locale separator is.
  $ws.Range($ws.Cells.Item($top,1), $ws.Cells.Item($top + $rows.Count - 1, $cols)).Formula = $arr
}

# ============================================================ Sheet 1: Skills and Auras
$ws = $wb.Sheets.Item(1)
$ws.Name = 'Skills and Auras'
$ws.Cells.Item(1,1).Value2 = "Diablo Orcl V1 v$version - class-tree skills and auras that are BUILT, one row per effect channel"
$ws.Cells.Item(1,1).Font.Bold = $true
$ws.Cells.Item(1,1).Font.Size = 13
$ws.Cells.Item(2,1).Value2 = "Rank columns are live formulas - base + per-rank x (rank - 1), the Scaled() shape from class_tree.cpp. Edit Base or Per extra rank and they recalculate."
$ws.Cells.Item(2,1).Font.Italic = $true

$names  = @('Skill','Class','Kind','Tier','Unlocks at level','Effect channel','Base (rank 1)','Per extra rank')
$widths = @(24,11,9,5,9,24,10,10)
foreach ($n in $RANKS) { $names += "Rank $n"; $widths += 8 }
$names += 'Aura radius (tiles)'; $widths += 30
$names += 'Conditions and caveats'; $widths += 55
Set-Header $ws 4 $names $widths

$rows = @()
foreach ($sk in $W.skills) {
  if (-not $sk.implemented) { continue }
  if ($sk.kind -ne 'Aura' -and $sk.kind -ne 'Passive') { continue }
  if (-not $SCALED.ContainsKey($sk.name)) { continue }
  # PowerShell COLLAPSES @(@(...)) with a single inner array, so a one-channel skill arrives as
  # a flat @('name',base,per,cond) and a naive foreach then iterates its four FIELDS as if each
  # were a channel. Re-wrap when the first element is not itself an array.
  $chans = @($SCALED[$sk.name])
  if ($chans[0] -isnot [System.Array]) { $chans = @(, $chans) }
  foreach ($ch in $chans) {
    $chan = $ch[0]; $base = [int]$ch[1]; $per = [int]$ch[2]; $cond = $ch[3]
    $flat = ($base -eq 0 -and $per -eq 0)
    $rowIdx = 5 + $rows.Count
    $row = @($sk.name, $sk.class, $sk.kind, ($sk.tier + 1), $TIER[$sk.tier], $chan)
    $row += if ($flat) { 'n/a' } else { $base }
    $row += if ($flat) { 'n/a' } else { $per }
    foreach ($n in $RANKS) {
      $row += if ($flat) { '-' } else { "=`$G$rowIdx+`$H$rowIdx*($n-1)" }
    }
    $row += if ($sk.kind -eq 'Aura') { '4 at 1 pt, +1 every 2 pts, caps at 8 (at 9 pts)' } else { '-' }
    $row += $cond
    $rows += ,$row
  }
}
Write-Block $ws 5 $rows $names.Count
$skillRows = $rows.Count

# ==================================================================== Sheet 2: Spells
$ws2 = $wb.Sheets.Add([System.Reflection.Missing]::Value, $wb.Sheets.Item($wb.Sheets.Count))
$ws2.Name = 'Spells'
$ws2.Cells.Item(1,1).Value2 = "Diablo Orcl V1 v$version - every spell in spelldat.cpp, and what a spell level actually changes"
$ws2.Cells.Item(1,1).Font.Bold = $true
$ws2.Cells.Item(1,1).Font.Size = 13
$ws2.Cells.Item(2,1).Value2 = "Damage formulas are GetDamageAmtAtLevel (missiles.cpp) verbatim. 'does not scale with spell level' means extra levels cut the mana cost and nothing else."
$ws2.Cells.Item(2,1).Font.Italic = $true
$n2 = @('Spell','Role','Book level band','Magic required','Mana at level 1','Mana saved per level','Mana floor','Damage formula','Damage per spell level','Missiles')
$w2 = @(22,13,9,9,10,10,8,48,28,22)
Set-Header $ws2 4 $n2 $w2

$rows2 = @()
foreach ($sp in ($W.spells | Sort-Object @{e={$_.role -ne 'book spell'}}, @{e={if ($_.name) {$_.name} else {$_.id}}})) {
  $d = if ($DAMAGE.ContainsKey($sp.id)) { $DAMAGE[$sp.id] } else { @('no damage','') }
  $rows2 += ,@(
    $(if ($sp.name) { $sp.name } else { $sp.id }),
    $sp.role,
    $(if ($sp.band) { $sp.band } else { '-' }),
    $sp.minInt,
    $(if ($sp.mana -eq 255) { 'all mana' } else { $sp.mana }),
    $(if ($sp.manaAdj) { $sp.manaAdj } else { '-' }),
    $sp.minMana,
    $d[0],
    $(if ($d[1]) { $d[1] } else { 'does not scale with spell level' }),
    $(if ($sp.missiles) { $sp.missiles } else { '-' })
  )
}
Write-Block $ws2 5 $rows2 $n2.Count
$spellRows = $rows2.Count

# ============================================================ Sheet 3: Full inventory
$ws3 = $wb.Sheets.Add([System.Reflection.Missing]::Value, $wb.Sheets.Item($wb.Sheets.Count))
$ws3.Name = 'Full inventory'
$ws3.Cells.Item(1,1).Value2 = "Every class-tree row in the game at v$version - built and unbuilt"
$ws3.Cells.Item(1,1).Font.Bold = $true
$ws3.Cells.Item(1,1).Font.Size = 13
$ws3.Cells.Item(2,1).Value2 = "'Built' is the implemented flag in class_tree.cpp. An unbuilt row is listed and described in game and takes no points - it is not a gap in this sheet."
$ws3.Cells.Item(2,1).Font.Italic = $true
$n3 = @('Skill','Class','Kind','Page','Tier','Unlocks at level','Built','Spell slot','Description')
$w3 = @(26,11,9,5,5,9,7,18,90)
Set-Header $ws3 4 $n3 $w3
$rows3 = @()
foreach ($sk in ($W.skills | Sort-Object class, page, tier, column)) {
  $rows3 += ,@($sk.name, $sk.class, $sk.kind, ($sk.page + 1), ($sk.tier + 1), $TIER[$sk.tier],
               $(if ($sk.implemented) { 'yes' } else { 'no' }),
               $(if ($sk.spell -eq 'Invalid') { '-' } else { $sk.spell }),
               $sk.description)
}
Write-Block $ws3 5 $rows3 $n3.Count
$invRows = $rows3.Count

# ============================================================ Sheet 4: How to read this
$ws4 = $wb.Sheets.Add([System.Reflection.Missing]::Value, $wb.Sheets.Item($wb.Sheets.Count))
$ws4.Name = 'How to read this'
$ws4.Cells.Item(1,1).Value2 = 'How to read this workbook'
$ws4.Cells.Item(1,1).Font.Bold = $true
$ws4.Cells.Item(1,1).Font.Size = 13
$ws4.Columns.Item(1).ColumnWidth = 32
$ws4.Columns.Item(2).ColumnWidth = 115
$notes = @(
  @('Where the numbers come from','Nothing here is estimated. The inventory is parsed out of class_tree.cpp and spelldat.cpp (via wiki/data.js, which tools/BuildWiki.ps1 generates). The per-rank effects are transcribed from ApplyAura, ApplyPassive and ProcessClassTreeTick in Source/oracool/class_tree.cpp. Spell damage is GetDamageAmtAtLevel in Source/missiles.cpp; mana is GetManaAmount in Source/spells.cpp; the aura radius is AuraRadiusForPoints in Source/oracool/aura_field.cpp.'),
  @('The rank formula','Scaled(points, base, perPoint) = base + perPoint x (points - 1). The FIRST point buys the base; each point after it adds perPoint. Nothing invested means the skill is inert.'),
  @('Aura radius','AuraRadiusForPoints(p) = min(4 + (p-1)/2, 8), integer division. Four tiles at one point, one more tile every SECOND point, capped at eight - which is reached at nine points. Every aura shares this; it is the only quantity all of them scale by.'),
  @('The level requirement climbs','A skill''s tier sets its floor (tier 1 = level 1, then 6, 12, 18, 24, 30, 36). Each rank after the first costs one more character level: RankRequiredLevel(tierLevel, rank) = tierLevel + rank - 1. A tier-1 skill takes its tenth point at level 10, and a tier-7 skill takes its tenth at 45.'),
  @('Spell mana','Mana falls by the spell''s own manaAdj for every level above 1, down to the floor in the Mana floor column. Firebolt halves that saving. Rogue, Monk and Bard pay 25% less overall; a Hellfire Sorcerer pays half.'),
  @('ScaleSpellEffect','ScaleSpellEffect(base, sl) adds base/8 once per spell level - about +12.5% compounding, with integer truncation at every step. Only the spells whose formula shows Scale(...) grow with spell level at all.'),
  @('Spells that do NOT scale with spell level','Lightning, Rune of Light, Fire Wall, Lightning Wall, Ring of Fire, Chain Lightning, Flame Wave, Inferno, Golem, Apocalypse, Charged Bolt and Holy Bolt take their damage from the CHARACTER''s level or magic alone. Extra spell levels still cut their mana cost, but not their damage. This is inherited engine behaviour, recorded here rather than changed.'),
  @('What is missing, and why','206 of the 273 class-tree rows are not built yet - see the Built column on Full inventory. They are listed and described in game and take no points. They have no per-rank numbers here because there is no code behind them; inventing some for this sheet would be worse than the blank, so the blank stands.'),
  @('Flag effects','Some ranks buy a flag rather than a number - fast attack, fastest hit recovery, thorns, a skipped run frame. A flag is on from the first point and does not grow, so its rank columns read "-" rather than a made-up curve.'),
  @('Conditional effects','A Barbarian mastery pays only while the matching weapon is held; Iron Robe pays only in light or no armour, at half in light. The Conditions column names every such gate.')
)
$r = 3
foreach ($n in $notes) {
  $ws4.Cells.Item($r,1).Value2 = $n[0]
  $ws4.Cells.Item($r,1).Font.Bold = $true
  $ws4.Cells.Item($r,2).Value2 = $n[1]
  $ws4.Range($ws4.Cells.Item($r,1), $ws4.Cells.Item($r,2)).WrapText = $true
  $ws4.Range($ws4.Cells.Item($r,1), $ws4.Cells.Item($r,2)).VerticalAlignment = -4160
  $ws4.Rows.Item($r).RowHeight = 52
  $r++
}

# --------------------------------------------------------------------------- finishing
foreach ($sheet in @($ws, $ws2, $ws3, $ws4)) {
  $sheet.Cells.Font.Name = 'Arial'
  $sheet.Cells.Font.Size = 10
  $sheet.Cells.Item(1,1).Font.Size = 13
  $sheet.Cells.Item(1,1).Font.Bold = $true
}
foreach ($pair in @(@($ws,$skillRows,$names.Count), @($ws2,$spellRows,$n2.Count), @($ws3,$invRows,$n3.Count))) {
  $sh = $pair[0]; $cnt = $pair[1]; $nc = $pair[2]
  $body = $sh.Range($sh.Cells.Item(5,1), $sh.Cells.Item(4 + $cnt, $nc))
  $body.VerticalAlignment = -4160
  $body.WrapText = $true
  for ($b = 7; $b -le 12; $b++) { $body.Borders.Item($b).LineStyle = 1; $body.Borders.Item($b).Color = 0xBFBFBF }
  $sh.Range($sh.Cells.Item(4,1), $sh.Cells.Item(4 + $cnt, $nc)).AutoFilter() | Out-Null
  $sh.Rows("5:$(4 + $cnt)").AutoFit() | Out-Null
}
$ws.Activate()
$ws.Cells.Item(5,1).Select()

$wb.SaveAs($out, 51)   # 51 = xlOpenXMLWorkbook (.xlsx)
$wb.Close($false)
$xl.Quit()
[void][Runtime.InteropServices.Marshal]::ReleaseComObject($xl)

Write-Host "wrote $out"
Write-Host "  Skills and Auras : $skillRows rows"
Write-Host "  Spells           : $spellRows rows"
Write-Host "  Full inventory   : $invRows rows"
