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
		$rows .= qq{<tr><td class="id">$code-$k</td><td class="sample"><img src="$uri" alt="Color$name$k"></td><td class="name">Color$name$k$usedHtml</td><td class="idx">$lo-$hi <span class="sw">$sw</span></td><td class="recipe">$recipe</td></tr>\n};
	}
	my $full = join '', map { qq{<i style="background:@{[hex6($base + $_)]}"></i>} } 0 .. $len - 1;
	$blocks .= <<"X";
<section class="ramp">
<header><h2>$name</h2><span class="meta">palette $base-@{[$base+$len-1]} · $note · $usable usable of @{[$len>=15?$len-3:8]}</span><span class="sw full">$full</span></header>
<div class="scroll"><table>
<thead><tr><th>ID</th><th>Sample (Font 12, 2x)</th><th>Name in code</th><th>Glyph lands on</th><th>.trn band 192-207 →</th></tr></thead>
<tbody>$rows</tbody></table></div>
</section>
X
}

# ---- the working set: what item text uses today, one row each, in the real files
my @today = (
	['ColorWhite', 'white.trn', '≈ GR-2', 'plain items, most panel text'],
	['ColorWhitegold', 'whitegold.trn', '≈ GD-2', 'unique items; the fallback for an unrecognised flag'],
	['ColorGold', 'gamedialogyellow.trn', 'GD-3', 'labels, the raw glyph; same look as the six dialog names'],
	['ColorYellow3', 'oracool_yellow3.trn', 'YL-3', 'rare items and rejuvenation potions (was ColorYellow, YL-1, until 2026-09-07)'],
	['ColorRed', 'red.trn', '≈ RD-3', 'unmet requirements, fire damage, a slow on the sheet'],
	['ColorBlue', 'blue.trn', 'BL-0', 'magic items, magic damage, a bonus on the sheet'],
	['ColorBeige2', 'oracool_beige2.trn', 'BE-2', 'Primal items, name and slot backing (was ColorOrange, OR-1)'],
	['ColorGray5', 'oracool_gray5.trn', 'GR-5', 'socketed plain drops on the floor, the socket count on any drop, the Sockets row'],
	['ColorGray7', 'oracool_gray7.trn', 'GR-7', 'ethereal plain items and the Ethereal row; beats the socketed gray'],
	['ColorBrightRed3', 'oracool_brightred3.trn', 'BR-3', 'health potions'],
	['ColorBrightBlue3', 'oracool_brightblue3.trn', 'BB-3', 'mana potions'],
	['ColorGold6', 'oracool_gold6.trn', 'GD-6', 'books'],
	['ColorOrange7', 'oracool_orange7.trn', 'OR-7', 'runes'],
	['ColorOracoolGreen', 'yellow.trn', 'GN-1', 'set items, healing (yellow.trn shifted onto the green minis at load)'],
	['ColorBlack', 'black.trn', 'BK', 'the shadow under HUD text'],
);
my $work = '';
for my $t (@today) {
	my ($name, $file, $id, $use) = @$t;
	my $trn = trnfile($file);
	if ($name eq 'ColorOracoolGreen') { $_ = ($_ >= 144 && $_ < 152) ? $_ - 144 + 152 : $_ for @$trn; }
	if ($name eq 'ColorOrange') { $_ = ($_ >= 152 && $_ < 160) ? $_ - 152 + 208 : $_ for @$trn; }
	my $uri = render("This is $name", $trn);
	$work .= qq{<tr><td class="id">$id</td><td class="sample"><img src="$uri" alt="$name"></td><td class="name">$name</td><td class="file">fonts\\$file</td><td>$use</td></tr>\n};
}

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
.sw i{display:inline-block;width:10px;height:14px;vertical-align:middle;border-right:1px solid #000}
.sw.full i{width:12px}
.steps{max-width:70ch}
.steps ol{padding-left:22px;margin:6px 0}
.steps li{margin:4px 0}
code{font-family:"JetBrains Mono",Consolas,monospace;font-size:12.5px;color:var(--gold)}
.warn{border-left:3px solid var(--hot);padding:6px 12px;color:var(--ink);max-width:70ch;margin:10px 0 0}
</style>
<h1>Orcl Font Colour Legend</h1>
<p class="lede">Every colour a string can be drawn in during play, rendered with the game's own Font 12 glyphs on the runtime palette. <b>Part 1</b> is the nine colours item text uses today. <b>Part 2</b> is the full pool: ten palette ramps, keeping only the passes where the glyph still has its bevel. Every shade has an <b>ID</b>: two letters for the ramp, a number for the pass, GD-3 or YL-1. Refer to a colour by its ID; a name in Part 2 is only a proposal for what it would be called in code once added, and the .trn recipe beside it is the whole file. BK is black, the one colour with no ramp.</p>

<section class="panel"><header><h2>1 · In use today</h2><span class="meta">nine looks behind 22 enum names; the file is what matters</span></header>
<div class="scroll"><table><thead><tr><th>ID</th><th>Sample</th><th>Name in code</th><th>File</th><th>Used for</th></tr></thead><tbody>$work</tbody></table></div>
</section>

<section class="panel"><header><h2>2 · The pool</h2><span class="meta">66 usable shades; a swatch shows the entries the glyph's 13 levels land on, the band column is the .trn's entries 192-207</span></header>
$blocks
</section>

<section class="panel steps"><header><h2>Adding a colour</h2></header>
<ol>
<li>Write the 256-byte .trn: identity everywhere except entries 192-207, which take the band column above. <code>tools/MakeYellowFontTrn.ps1</code> is the pattern. Put it in <code>Packaging/resources/oracool_assets/fonts</code>.</li>
<li>Add the name to <code>text_color</code> in <code>engine/render/text_render.hpp</code> (append; the enum indexes <code>ColorTranslations</code> positionally) and the file to <code>ColorTranslations</code> in <code>text_render.cpp</code>, growing both array sizes.</li>
<li>Add a <code>UiFlags::Color…</code> bit in <code>DiabloUI/ui_flags.hpp</code> (the widened 64-bit range) and a line in <code>GetColorFromFlags</code>, above the Whitegold fallback.</li>
<li>Repack: <code>tools\\build_oracool_mpq.cmd</code>. A normal build does not rebuild the MPQ.</li>
</ol>
<p class="warn">Front-end screens run on a different palette. Entries 128-135 are blue in play and yellow in the menus; 176-191 are steel blue in play and gold in the menus; 224-239 are red in play and gray in the menus. A colour picked here is for in-game text. Check it on a menu separately before reusing the name there.</p>
</section>
X
my $out = "$repo/.ProjectDocumentation/06-Reference/Font-Colour-Legend.html";
open my $O, '>', $out or die; print $O $page; close $O;
print "ok ", length($page), "\n";
