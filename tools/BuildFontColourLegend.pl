use strict; use warnings;
use Compress::Zlib;
use MIME::Base64;
my $repo = 'C:/Users/hroga/OneDrive/2. Personal Files/Software/Diablo/Diablo Orcl';
my $palf = 'C:/Users/hroga/OneDrive/2. Personal Files/Software/Diablo/Resources/02-source-art/delivered-packs/oracool-stash-tab-button-pack/assets/source/town-patched-runtime.pal';
my $fonts = "$repo/Packaging/resources/assets/fonts";
my $ofonts = "$repo/Packaging/resources/oracool_assets/fonts";
sub slurp { my $f = shift; open my $F, '<:raw', $f or die "$f: $!"; local $/; my $d = <$F>; close $F; return $d; }
my $pal = slurp($palf);
my @pal = map { [unpack('C3', substr($pal, $_ * 3, 3))] } 0 .. 255;
sub hex6 { sprintf('#%02x%02x%02x', @{ $pal[$_[0]] }) }

my $sheet = slurp("$fonts/12-00.clx");
my $nframes = unpack('V', $sheet);
my %glyph;
sub glyph {
	my $c = shift;
	return $glyph{$c} if $glyph{$c};
	my $o = unpack('V', substr($sheet, 4 + 4 * $c, 4));
	my $end = ($c + 1 < $nframes) ? unpack('V', substr($sheet, 4 + 4 * ($c + 1), 4)) : length $sheet;
	my ($hs, $w, $h) = unpack('vvv', substr($sheet, $o, 6));
	my $p = $o + $hs; my @rows; my @row; my $bound = 0;
	while ($p < $end && @rows < $h && ++$bound < 100000) {
		my $k = ord substr($sheet, $p++, 1);
		if ($k < 0x80) { push @row, (0) x $k; }
		elsif ($k <= 0xBE) { my $wd = 0xBF - $k; my $ix = ord substr($sheet, $p++, 1); push @row, ($ix) x $wd; }
		else { my $wd = 256 - $k; for (1 .. $wd) { push @row, ord substr($sheet, $p++, 1); } }
		while (@row >= $w) { push @rows, [splice @row, 0, $w]; }
	}
	push @rows, [(0) x $w] while @rows < $h;
	@rows = reverse @rows;
	return $glyph{$c} = { w => $w, h => $h, rows => \@rows };
}
sub crc { pack('N', crc32($_[0])) }
sub chunk { my ($t, $s) = @_; pack('N', length $s) . $t . $s . crc($t . $s) }
sub render {
	my ($text, $trn) = @_;
	my @gl = map { glyph(ord $_) } split //, $text;
	my $w = 0; $w += $_->{w} + 1 for @gl; $w -= 1;
	my $h = 20;
	my @canvas = map { [(0) x $w] } 1 .. $h;
	my $x = 0;
	for my $g (@gl) {
		for my $y (0 .. $g->{h} - 1) { my $r = $g->{rows}[$y]; for my $i (0 .. $g->{w} - 1) { $canvas[$y][$x + $i] = $r->[$i] if $r->[$i]; } }
		$x += $g->{w} + 1;
	}
	my $raw = '';
	for my $row (@canvas) { $raw .= "\0"; for my $ix (@$row) { if (!$ix) { $raw .= "\0\0\0\0"; next; } $raw .= pack('C4', @{ $pal[$trn->[$ix]] }, 255); } }
	my $png = "\x89PNG\r\n\x1a\n" . chunk('IHDR', pack('NNCCCCC', $w, $h, 8, 6, 0, 0, 0)) . chunk('IDAT', compress($raw)) . chunk('IEND', '');
	return "data:image/png;base64," . encode_base64($png, '');
}
sub trnfile { my $d = slurp($_[0] =~ /^oracool_/ ? "$ofonts/$_[0]" : "$fonts/$_[0]"); return [map { ord substr($d, $_, 1) } 0 .. 255]; }

# ---- the pass -> band map, shared with the earlier survey
sub passmap {
	my ($base, $len, $k) = @_;
	my $trn = [0 .. 255];
	for my $j (0 .. 15) {
		my $v = $len >= 15 ? $j - 3 + $k : int($j / 2) - 1 + $k;
		$v = 0 if $v < 0; $v = $len - 1 if $v > $len - 1;
		$trn->[192 + $j] = $base + $v;
	}
	return $trn;
}

# ---- the ramps and their usable passes
my @ramps = (
	['Gold', 192, 16, 8, 'gold ramp', 'GD'], ['Gray', 240, 15, 8, 'gray ramp (240-254; 255 is pure white and stays out)', 'GR'], ['Red', 224, 16, 8, 'red ramp', 'RD'], ['Orange', 208, 16, 8, 'orange ramp', 'OR'],
	['Blue', 176, 16, 8, 'steel-blue ramp', 'BL'], ['Beige', 160, 16, 8, 'beige ramp; rose in most level palettes', 'BE'],
	['Yellow', 144, 8, 4, 'bright yellow minis', 'YL'], ['Green', 152, 8, 4, 'green minis, injected by the fork over the orange minis', 'GN'],
	['BrightRed', 136, 8, 4, 'bright red minis', 'BR'], ['BrightBlue', 128, 8, 4, 'bright blue minis', 'BB'],
);
# where the named colours in use today sit (nearest pass, by the band's third entry)
my %today = (
	'Gold:3' => 'ColorGold (and the six dialog names)', 'Gold:2' => 'ColorWhitegold - unique items, the default',
	'Gray:2' => 'ColorWhite - plain item text', 'Red:3' => 'ColorRed - unmet requirements, fire',
	'Blue:0' => 'ColorBlue - magic items', 'Orange:1' => 'ColorOrange - Primal items',
	'Yellow:1' => 'ColorYellow - lightning on the sheet (rare items until 2026-09-07)', 'Yellow:3' => 'ColorYellow3 - rare items, rejuvenation potions', 'Beige:2' => 'ColorBeige2 - Primal items', 'Gray:5' => 'ColorGray5 - socketed drops and the Sockets row', 'Gray:7' => 'ColorGray7 - ethereal items and the Ethereal row', 'BrightRed:3' => 'ColorBrightRed3 - health potions', 'BrightBlue:3' => 'ColorBrightBlue3 - mana potions', 'Gold:6' => 'ColorGold6 - books', 'Orange:7' => 'ColorOrange7 - runes', 'Green:1' => 'ColorOracoolGreen - set items',
	'Blue:0 ' => '', 'Red:2' => 'ColorUiSilver (front-end name; red in play)', 'Blue:0  ' => '',
	'Gray:0' => '(pure white 255 is one step lighter than this ramp)',
);

# The file a pool ID already has - a vanilla file where one lands exactly on the pass, an Orcl file
# where one was made - or the name it would get (user, 2026-09-07: "put a file column in The Pool").
my %fileFor = (
	'BL-0' => 'fonts\blue.trn', 'YL-1' => 'fonts\yellow.trn', 'OR-1' => 'fonts\oracool_orange1.trn',
	'GD-3' => 'none - the raw glyph (ColorGold)', 'GN-1' => 'fonts\oracool_green1.trn',
	'GR-2' => '≈ fonts\white.trn (hand-tuned, not a clean pass)', 'GD-2' => '≈ fonts\whitegold.trn (hand-tuned)', 'RD-3' => '≈ fonts\red.trn (hand-tuned)',
	'GR-5' => 'fonts\oracool_gray5.trn', 'BE-2' => 'fonts\oracool_beige2.trn', 'YL-3' => 'fonts\oracool_yellow3.trn',
	'BR-3' => 'fonts\oracool_brightred3.trn', 'BB-3' => 'fonts\oracool_brightblue3.trn', 'GD-6' => 'fonts\oracool_gold6.trn',
	'OR-7' => 'fonts\oracool_orange7.trn', 'GR-7' => 'fonts\oracool_gray7.trn',
);
my $blocks = '';
for my $r (@ramps) {
	my ($name, $base, $len, $usable, $note, $code) = @$r;
	my $rows = '';
	for my $k (0 .. $usable - 1) {
		my $trn = passmap($base, $len, $k);
		my $uri = render("This is Color$name$k", $trn);
		my @win = map { $trn->[192 + $_] } 0 .. 15;
		my ($lo, $hi) = ($trn->[195], $trn->[207]);
		my $sw = join '', map { qq{<i style="background:@{[hex6($_)]}"></i>} } $lo .. $hi;
		my $recipe = join ' ', @win;
		my $used = $today{"$name:$k"} // '';
		my $usedHtml = $used ? qq{<span class="today">$used</span>} : '';
		my $fileHtml = exists $fileFor{"$code-$k"}
		    ? $fileFor{"$code-$k"}
		    : qq{<i>oracool_@{[lc $name]}$k.trn</i> <span class="notyet">not made yet</span>};
		$rows .= qq{<tr><td class="id">$code-$k</td><td class="sample"><img src="$uri" alt="Color$name$k"></td><td class="name">Color$name$k$usedHtml</td><td class="file">$fileHtml</td><td class="idx">$lo-$hi <span class="sw">$sw</span></td><td class="recipe">$recipe</td></tr>\n};
	}
	my $full = join '', map { qq{<i style="background:@{[hex6($base + $_)]}"></i>} } 0 .. $len - 1;
	$blocks .= <<"X";
<section class="ramp">
<header><h2>$name</h2><span class="meta">palette $base-@{[$base+$len-1]} · $note · $usable usable of @{[$len>=15?$len-3:8]}</span><span class="sw full">$full</span></header>
<div class="scroll"><table>
<thead><tr><th>ID</th><th>Sample (Font 12, 2x)</th><th>Name in code</th><th>File</th><th>Glyph lands on</th><th>.trn band 192-207 →</th></tr></thead>
<tbody>$rows</tbody></table></div>
</section>
X
}

# ---- every .trn the game ships, in two inventories: the 16 vanilla files and the 10 Orcl ones
# (user, 2026-09-07: "put all of them in the artifact/wiki. make a column to state used/not-used.
# put them in two separate categories - Vanilla Colours, Orcl Colors"). "Used" is whether any live
# code asks for the colour; the counts were taken from a grep of UiFlags::Color* on 2026-09-07.
# A file with no enum reader is still loaded into the translation table and costs nothing else.
my %fieldIndex = (
	ColorUiGold => 1, ColorUiSilver => 2, ColorUiGoldDark => 3, ColorUiSilverDark => 4, ColorDialogWhite => 5,
	ColorDialogYellow => 6, ColorDialogRed => 7, ColorYellow => 8, ColorGold => 9, ColorBlack => 10, ColorWhite => 11,
	ColorWhitegold => 12, ColorRed => 13, ColorBlue => 14, ColorOrange => 15, ColorButtonface => 16, ColorButtonpushed => 17,
	ColorOracoolYellow => 18, ColorOracoolYellowDark => 19, ColorOracoolGreen => 20, ColorGray5 => 21, ColorBeige2 => 22,
	ColorYellow3 => 23, ColorBrightRed3 => 24, ColorBrightBlue3 => 25, ColorGold6 => 26, ColorOrange7 => 27, ColorGray7 => 28,
);
my @vanilla = (
	['white.trn', 'ColorWhite', '≈ GR-2', 1, 'plain items, most panel and HUD text'],
	['whitegold.trn', 'ColorWhitegold', '≈ GD-2', 1, 'unique items; the fallback for an unrecognised flag'],
	['yellow.trn', 'ColorYellow', 'YL-1', 1, 'lightning damage on the sheet and the floating numbers (rare items until 2026-09-07)'],
	['red.trn', 'ColorRed', '≈ RD-3', 1, 'unmet requirements, fire damage, a slow on the sheet'],
	['blue.trn', 'ColorBlue', 'BL-0', 1, 'magic items, magic damage, a bonus on the sheet'],
	['orange.trn', '(none)', 'OR-1 on the menu palette', 0, 'vanilla orange, which points at the minis the green ramp took; the in-game orange is oracool_orange1.trn now'],
	['black.trn', 'ColorBlack', 'BK', 1, 'the shadow under HUD text'],
	['goldui.trn', 'ColorUiGold', 'BL ramp in play', 1, 'front-end text (gold on the menu palette, steel blue in a level)'],
	['golduis.trn', 'ColorUiGoldDark', 'BL ramp in play', 1, 'front-end text, dark'],
	['grayui.trn', 'ColorUiSilver', 'RD ramp in play', 1, 'front-end text (gray on the menu palette, red in a level); floating fire damage'],
	['grayuis.trn', 'ColorUiSilverDark', 'RD ramp in play', 1, 'the mlvl line under a monster bar - which is why it reads red-brown there'],
	['gamedialogwhite.trn', 'ColorDialogWhite', 'GD-3', 1, 'in-game dialog text; identity on the glyph band, so the raw gold glyph'],
	['gamedialogyellow.trn', 'ColorDialogYellow', 'GD-3', 0, 'vanilla dialog yellow; no caller since the dialogs were rebuilt'],
	['gamedialogred.trn', 'ColorDialogRed', 'GD-3', 0, 'vanilla dialog red; no caller'],
	['buttonface.trn', 'ColorButtonface', 'gray/gold mix', 0, 'vanilla menu buttons; no caller'],
	['buttonpushed.trn', 'ColorButtonpushed', 'gray/gold mix', 0, 'vanilla menu buttons, pressed; no caller'],
	['', 'ColorGold', 'GD-3', 1, 'NO FILE, by rule the only one: an empty table slot draws the raw gold glyph. Labels, the readied-slot rows, the gold text everywhere'],
);
my @orcl = (
	['oracool_yellow.trn', 'ColorOracoolYellow', 'menu palette only', 1, 'the front-end focus glow (2026-08-15); points at 128-135, which is bright blue in a level, so never on items'],
	['oracool_yellows.trn', 'ColorOracoolYellowDark', 'menu palette only', 0, 'the glow\'s dark twin; no caller'],
	['oracool_green1.trn', 'ColorOracoolGreen', 'GN-1', 1, 'set items, healing (was yellow.trn shifted in memory until 2026-09-07)'],
	['oracool_orange1.trn', 'ColorOrange', 'OR-1', 1, 'the runeword book\'s rune lines, floating magic damage (was vanilla orange.trn re-pointed in memory)'],
	['oracool_gray5.trn', 'ColorGray5', 'GR-5', 1, 'socketed plain drops on the floor, the socket count on any drop, the Sockets row'],
	['oracool_beige2.trn', 'ColorBeige2', 'BE-2', 1, 'Primal items, name and slot backing'],
	['oracool_yellow3.trn', 'ColorYellow3', 'YL-3', 1, 'rare items, rejuvenation potions'],
	['oracool_brightred3.trn', 'ColorBrightRed3', 'BR-3', 1, 'health potions'],
	['oracool_brightblue3.trn', 'ColorBrightBlue3', 'BB-3', 1, 'mana potions'],
	['oracool_gold6.trn', 'ColorGold6', 'GD-6', 1, 'books'],
	['oracool_orange7.trn', 'ColorOrange7', 'OR-7', 1, 'runes'],
	['oracool_gray7.trn', 'ColorGray7', 'GR-7', 1, 'ethereal plain items and the Ethereal row; beats the socketed gray'],
);
sub inventory {
	my @rows;
	for my $t (@_) {
		my ($file, $name, $id, $used, $use) = @$t;
		my $trn;
		if ($file eq '') {
			$trn = [0 .. 255];
		} else {
			$trn = trnfile($file);
		}
		my $uri = render("This is $name", $trn);
		my $fileHtml = $file eq '' ? '<i>none</i>' : "fonts\\$file";
		my $usedHtml = $used ? '<span class="used">used</span>' : '<span class="unused">not used</span>';
		my $fi = $fieldIndex{$name} // '-';
		push @rows, qq{<tr><td class="id">$id</td><td class="sample"><img src="$uri" alt="$name"></td><td class="name">$name</td><td class="idx">$fi</td><td class="file">$fileHtml</td><td class="used">$usedHtml</td><td>$use</td></tr>\n};
	}
	return join '', @rows;
}
my $vanillaRows = inventory(@vanilla);
my $orclRows = inventory(@orcl);
my $vanillaUsed = scalar(grep { $_->[3] } @vanilla);
my $orclUsed = scalar(grep { $_->[3] } @orcl);

my $page = <<"X";
<title>Orcl Font Colour Legend</title>
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Cinzel:wght\@600&family=Source+Sans+3:wght\@400;600&family=JetBrains+Mono:wght\@400&display=swap">
<style>
:root{--ground:#0e0c0a;--panel:#171412;--rule:#3a3126;--ink:#d8d0c0;--muted:#8f8577;--gold:#e6d29a;--hot:#c9a24d}
body{background:var(--ground);color:var(--ink);font-family:"Source Sans 3",Segoe UI,system-ui,sans-serif;font-size:15px;line-height:1.5;margin:0;padding:32px 28px 60px}
h1{font-family:Cinzel,Georgia,serif;font-weight:600;color:var(--gold);font-size:30px;letter-spacing:.02em;margin:0 0 4px;text-wrap:balance}
h2{font-family:Cinzel,Georgia,serif;font-weight:600;font-size:18px;color:var(--gold);margin:0}
.lede{max-width:66ch;color:var(--muted);margin:0 0 28px}
.lede b{color:var(--ink);font-weight:600}
section{margin:0 0 26px}
.panel{background:var(--panel);border:1px solid var(--rule);padding:14px 18px 8px}
header{display:flex;align-items:baseline;gap:16px;flex-wrap:wrap;margin-bottom:8px}
.meta{color:var(--muted);font-size:13px}
.scroll{overflow-x:auto}
table{border-collapse:collapse;width:100%;font-variant-numeric:tabular-nums}
th{text-align:left;font-size:11px;letter-spacing:.08em;text-transform:uppercase;color:var(--muted);font-weight:600;padding:6px 12px 6px 0;border-bottom:1px solid var(--rule)}
td{padding:5px 12px 5px 0;border-bottom:1px solid #221d18;vertical-align:middle;white-space:nowrap}
td.sample{background:#000;padding:4px 10px;width:330px}
td.sample img{image-rendering:pixelated;height:40px;display:block}
td.id{font-family:"JetBrains Mono",Consolas,monospace;font-size:14px;color:var(--gold);font-weight:600;width:56px}
td.name{font-family:"JetBrains Mono",Consolas,monospace;font-size:13px;color:var(--ink)}
td.file{font-family:"JetBrains Mono",Consolas,monospace;font-size:12px;color:var(--muted)}
td.idx{font-family:"JetBrains Mono",Consolas,monospace;font-size:12px}
td.recipe{font-family:"JetBrains Mono",Consolas,monospace;font-size:11px;color:var(--muted)}
.today{display:block;font-family:"Source Sans 3",sans-serif;font-size:12px;color:var(--hot)}
td.used{white-space:nowrap}
span.used{color:#8fbf6a;font-weight:600}
span.unused{color:#b06a4a;font-weight:600}
span.notyet{font-family:"Source Sans 3",sans-serif;font-size:11px;color:var(--muted);margin-left:6px}
.sw i{display:inline-block;width:10px;height:14px;vertical-align:middle;border-right:1px solid #000}
.sw.full i{width:12px}
.steps{max-width:70ch}
.steps ol{padding-left:22px;margin:6px 0}
.steps li{margin:4px 0}
code{font-family:"JetBrains Mono",Consolas,monospace;font-size:12.5px;color:var(--gold)}
.warn{border-left:3px solid var(--hot);padding:6px 12px;color:var(--ink);max-width:70ch;margin:10px 0 0}
</style>
<h1>Orcl Font Colour Legend</h1>
<p class="lede">Every colour a string can be drawn in during play, rendered with the game's own Font 12 glyphs on the runtime palette. In the engine a colour is a 12-bit field index in the draw flags (v1.10.014), shown beside each name below. <b>The rule (2026-09-07):</b> every colour has its own .trn file, named for its legend ID; nothing is borrowed or edited after loading; only the raw gold, ColorGold, has no file. <b>Parts 1 and 2</b> are every colour file the game ships, vanilla and Orcl, with whether live code still asks for it. <b>Part 3</b> is the full pool: ten palette ramps, keeping only the passes where the glyph still has its bevel. Every shade has an <b>ID</b>: two letters for the ramp, a number for the pass, GD-3 or YL-1. Refer to a colour by its ID; a name in Part 2 is only a proposal for what it would be called in code once added, and the .trn recipe beside it is the whole file. BK is black, the one colour with no ramp.</p>

<section class="panel"><header><h2>1 · Vanilla Colours</h2><span class="meta">the 16 .trn files DevilutionX ships, plus ColorGold which has none · @{[$vanillaUsed - 1]} of 16 files used</span></header>
<div class="scroll"><table><thead><tr><th>ID</th><th>Sample (as drawn in play)</th><th>Name in code</th><th>Field index</th><th>File</th><th>Used</th><th>Used for</th></tr></thead><tbody>$vanillaRows</tbody></table></div>
</section>

<section class="panel"><header><h2>2 · Orcl Colours</h2><span class="meta">the 12 .trn files this fork added · @{[$orclUsed]} of 12 used</span></header>
<div class="scroll"><table><thead><tr><th>ID</th><th>Sample</th><th>Name in code</th><th>Field index</th><th>File</th><th>Used</th><th>Used for</th></tr></thead><tbody>$orclRows</tbody></table></div>
</section>

<section class="panel"><header><h2>3 · The pool</h2><span class="meta">66 usable shades; a swatch shows the entries the glyph's 13 levels land on, the band column is the .trn's entries 192-207</span></header>
$blocks
</section>

<section class="panel steps"><header><h2>Adding a colour</h2></header>
<ol>
<li>Write the 256-byte .trn: identity everywhere except entries 192-207, which take the band column above. <code>tools/MakeYellowFontTrn.ps1</code> is the pattern. Put it in <code>Packaging/resources/oracool_assets/fonts</code>.</li>
<li>Add the name to <code>text_color</code> in <code>engine/render/text_render.hpp</code> (append; the enum indexes <code>ColorTranslations</code> positionally) and the file to <code>ColorTranslations</code> in <code>text_render.cpp</code>, growing both array sizes.</li>
<li>Add a <code>UiFlags::Color…</code> name in <code>DiabloUI/ui_flags.hpp</code> with the next free <b>field index</b> (<code>29ULL &lt;&lt; UiFlagsColorShift</code> and so on; the field is 12 bits, 4096 values, since v1.10.014) and a <code>case</code> for it in <code>GetColorFromFlags</code>. Nothing else: the outline pass clears the whole field, so there is no mask to extend.</li>
<li>Repack: <code>tools\\build_oracool_mpq.cmd</code>. A normal build does not rebuild the MPQ.</li>
</ol>
<p class="warn">Front-end screens run on a different palette. Entries 128-135 are blue in play and yellow in the menus; 176-191 are steel blue in play and gold in the menus; 224-239 are red in play and gray in the menus. A colour picked here is for in-game text. Check it on a menu separately before reusing the name there.</p>
</section>
X
my $out = "$repo/.ProjectDocumentation/06-Reference/Font-Colour-Legend.html";
open my $O, '>', $out or die; print $O $page; close $O;
print "ok ", length($page), "\n";
