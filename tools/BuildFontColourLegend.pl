use strict; use warnings;
# BuildFontColourLegend.pl - the Font Colour Legend as a REGISTRY of the colours in use.
#
# Rewritten for renderer stage 3 (v1.11.009, 2026-09-07). Until then the legend was a menu of what
# the 256-entry palette allowed: ten ramps, their passes, a pool of 66 shades to pick from. With
# text drawn through RGB tables on a 32-bit screen a colour is a value, so the pool is every colour
# there is and listing it is pointless (user: "the legend now only needs to include colors we
# actually use"). What remains worth keeping is the register: which colours the game draws, what
# each is called in code, the hex it lands on, where it is used. The samples are still the game's
# own Font 12 glyphs on the level palette, so a row shows exactly what play shows.
#
#   perl tools/BuildFontColourLegend.pl
# writes .ProjectDocumentation/06-Reference/Font-Colour-Legend.html; tools/BuildWikiColoursPage.pl
# folds it into the wiki.
use Compress::Zlib;
use MIME::Base64;
my $repo = 'C:/Users/hroga/OneDrive/2. Personal Files/Software/Diablo/Diablo Orcl';
my $palf = 'C:/Users/hroga/OneDrive/2. Personal Files/Software/Diablo/Resources/02-source-art/delivered-packs/oracool-stash-tab-button-pack/assets/source/town-patched-runtime.pal';
my $fonts = "$repo/Packaging/resources/assets/fonts";
my $ofonts = "$repo/Packaging/resources/oracool_assets/fonts";
sub slurp { my $f = shift; open my $F, '<:raw', $f or die "$f: $!"; local $/; my $d = <$F>; close $F; return $d; }
my $pal = slurp($palf);
my @pal = map { [unpack('C3', substr($pal, $_ * 3, 3))] } 0 .. 255;
sub hex6 { sprintf('#%02x%02x%02x', @{ $_[0] }) }

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

# A colour is a TABLE: index -> [r,g,b], exactly what the engine's TextColorRgbTable holds.
# From a file: the palette through the .trn. From a value: the band shaded like the gold ramp
# (text_render.cpp, BakeRgbTable), everything else its own palette colour.
sub lum { my $c = shift; int((299 * $c->[0] + 587 * $c->[1] + 114 * $c->[2]) / 1000) }
sub table_from_trn { my $trn = shift; return [map { $pal[$trn->[$_]] } 0 .. 255]; }
sub table_from_value {
	my $rgb = shift;
	my @t = map { $pal[$_] } 0 .. 255;
	my ($r, $g, $b) = (($rgb >> 16) & 255, ($rgb >> 8) & 255, $rgb & 255);
	my $top = lum($pal[192]) || 1;
	for my $j (0 .. 15) {
		my $l = lum($pal[192 + $j]); $l = $top if $l > $top;
		$t[192 + $j] = [int($r * $l / $top), int($g * $l / $top), int($b * $l / $top)];
	}
	return \@t;
}
sub render {
	my ($text, $table) = @_;
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
	for my $row (@canvas) { $raw .= "\0"; for my $ix (@$row) { if (!$ix) { $raw .= "\0\0\0\0"; next; } $raw .= pack('C4', @{ $table->[$ix] }, 255); } }
	my $png = "\x89PNG\r\n\x1a\n" . chunk('IHDR', pack('NNCCCCC', $w, $h, 8, 6, 0, 0, 0)) . chunk('IDAT', compress($raw)) . chunk('IEND', '');
	return "data:image/png;base64," . encode_base64($png, '');
}
sub trnfile { my $d = slurp($_[0] =~ /^oracool_/ ? "$ofonts/$_[0]" : "$fonts/$_[0]"); return [map { ord substr($d, $_, 1) } 0 .. 255]; }

# ---- how often live code asks for each colour flag (everything but the two files that define them)
my %uses;
{
	my @files;
	my $walk; $walk = sub { my $d = shift; opendir my $D, $d or return; for my $e (readdir $D) { next if $e =~ /^\./; my $p = "$d/$e"; if (-d $p) { $walk->($p) } elsif ($p =~ /\.(cpp|h|hpp)$/ && $p !~ /ui_flags\.hpp$|text_render\.cpp$/) { push @files, $p } } closedir $D; };
	$walk->("$repo/Source");
	for my $f (@files) { my $s = slurp($f); $uses{$1}++ while $s =~ /UiFlags::(Color\w+)/g; }
}

# ---- the register. One row per name in code that live code draws with, in field-index order.
# [flag name, legend ID, field index, file drawn in play, file drawn in menus (if different), used for]
my @rows = (
	['ColorWhitegold', 'GD-2', 0, 'whitegold.trn', undef, 'unique items; the fallback for an unrecognised flag'],
	['ColorUiGold', 'GD-3', 1, 'oracool_uigold.trn', 'goldui.trn', 'front-end text; in play the panel labels'],
	['ColorUiSilver', 'GR-3', 2, 'oracool_uisilver.trn', 'grayui.trn', 'front-end text; in play floating fire damage'],
	['ColorUiGoldDark', 'GD-5', 3, 'oracool_uigolddark.trn', 'golduis.trn', 'front-end text, dark'],
	['ColorUiSilverDark', 'GR-5', 4, 'oracool_uisilverdark.trn', 'grayuis.trn', 'front-end text, dark; the mlvl line under a monster bar'],
	['ColorDialogWhite', '≈ GR-2', 5, 'oracool_dialogwhite.trn', 'white.trn', 'dialog text'],
	['ColorYellow', 'YL-1', 8, 'yellow.trn', undef, 'lightning damage on the sheet and the floating numbers'],
	['ColorGold', 'GD-3', 9, '', undef, 'the raw glyph, no file: labels, the readied-slot rows, gold text everywhere'],
	['ColorBlack', 'BK', 10, 'black.trn', undef, 'the shadow under HUD text'],
	['ColorWhite', '≈ GR-2', 11, 'white.trn', undef, 'plain items, most panel and HUD text'],
	['ColorRed', '≈ RD-3', 13, 'red.trn', undef, 'unmet requirements, fire damage, a slow on the sheet'],
	['ColorBlue', 'BL-0', 14, 'blue.trn', undef, 'magic items, magic damage, a bonus on the sheet'],
	['ColorOrange', 'OR-1', 15, 'oracool_orange1.trn', undef, 'the runeword book\'s rune lines, floating magic damage'],
	['ColorUiYellow', 'YL-1', 18, 'oracool_uiyellow.trn', 'oracool_menuyellow.trn', 'the front-end focus glow'],
	['ColorOracoolGreen', 'GN-1', 20, 'oracool_green1.trn', undef, 'set items, healing'],
	['ColorGray5', 'GR-5', 21, 'oracool_gray5.trn', undef, 'socketed plain drops on the floor, the socket count on any drop, the Sockets row'],
	['ColorBeige2', 'BE-2', 22, 'oracool_beige2.trn', undef, 'Primal items, name and slot backing'],
	['ColorYellow3', 'YL-3', 23, 'oracool_yellow3.trn', undef, 'rare items, rejuvenation potions'],
	['ColorBrightRed3', 'BR-3', 24, 'oracool_brightred3.trn', undef, 'health potions'],
	['ColorBrightBlue3', 'BB-3', 25, 'oracool_brightblue3.trn', undef, 'mana potions'],
	['ColorGold6', 'GD-6', 26, 'oracool_gold6.trn', undef, 'books'],
	['ColorOrange7', 'OR-7', 27, 'oracool_orange7.trn', undef, 'runes'],
	['ColorGray7', 'GR-7', 28, 'oracool_gray7.trn', undef, 'ethereal plain items and the Ethereal row; beats the socketed gray'],
);
# Colours defined by VALUE (text_render.cpp, RgbDefinedColors): flag name -> 0xRRGGBB. None yet.
my %valueOf = ();

my $body = '';
my $inUse = 0;
for my $r (@rows) {
	my ($name, $id, $field, $file, $menuFile, $use) = @$r;
	my $count = $uses{$name} // 0;
	next if $count == 0 && $name ne 'ColorGold';
	$inUse++;
	my $table = exists $valueOf{$name} ? table_from_value($valueOf{$name}) : $file eq '' ? table_from_trn([0 .. 255]) : table_from_trn(trnfile($file));
	my $uri = render("This is $name", $table);
	# the value: what the glyph's brightest level lands on, and the shades its bevel walks through
	my $top = hex6($table->[192]);
	my %seen; my @shades = grep { !$seen{$_}++ } map { hex6($table->[$_]) } 195 .. 207;
	my $sw = join '', map { qq{<i style="background:$_" title="$_"></i>} } @shades;
	my $src = exists $valueOf{$name} ? sprintf('value 0x%06X', $valueOf{$name}) : $file eq '' ? '<i>none, the raw glyph</i>' : "fonts\\$file";
	$src .= qq{<br><span class="menu">menus: fonts\\$menuFile</span>} if $menuFile;
	$body .= qq{<tr><td class="id">$id</td><td class="sample"><img src="$uri" alt="$name"></td><td class="name">$name</td><td class="idx">$field</td><td class="hex">$top <span class="sw">$sw</span></td><td class="file">$src</td><td class="idx">$count</td><td>$use</td></tr>\n};
}

# ---- shipped and not drawn: the files with no caller, so nobody wonders where they went
my @parked = (
	['ColorDialogYellow', 'oracool_dialogyellow.trn', 'in-game dialog yellow; no caller yet'],
	['ColorDialogRed', 'oracool_dialogred.trn', 'in-game dialog red; no caller yet'],
	['ColorUiYellowDark', 'oracool_uiyellowdark.trn (oracool_menuyellowdark.trn in menus)', 'the focus glow\'s dark twin; no caller'],
	['ColorButtonface', 'buttonface.trn', 'vanilla menu buttons; no caller'],
	['ColorButtonpushed', 'buttonpushed.trn', 'vanilla menu buttons, pressed; no caller'],
	['(none)', 'orange.trn, gamedialogwhite.trn, gamedialogyellow.trn, gamedialogred.trn', 'vanilla files the fork no longer reads (each has an Orcl file of its own)'],
);
my $parkedRows = join '', map { qq{<tr><td class="name">$_->[0]</td><td class="file">$_->[1]</td><td>$_->[2]</td></tr>\n} } @parked;

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
td.file .menu{color:#6f6558}
td.idx{font-family:"JetBrains Mono",Consolas,monospace;font-size:12px}
td.hex{font-family:"JetBrains Mono",Consolas,monospace;font-size:12px;color:var(--ink)}
.sw i{display:inline-block;width:10px;height:14px;vertical-align:middle;border-right:1px solid #000}
.steps{max-width:70ch}
.steps ol{padding-left:22px;margin:6px 0}
.steps li{margin:4px 0}
code{font-family:"JetBrains Mono",Consolas,monospace;font-size:12.5px;color:var(--gold)}
.warn{border-left:3px solid var(--hot);padding:6px 12px;color:var(--ink);max-width:70ch;margin:10px 0 0}
</style>
<h1>Orcl Font Colour Legend</h1>
<p class="lede">The colours the game draws text in, one row each, rendered with the game's own Font 12 glyphs. <b>Since renderer stage 3 (v1.11.009)</b> a text colour is a value: on the 32-bit screen a glyph is drawn through a table of RGB values, not through a .trn and the 256-entry palette. The files still exist and still define the colours in use, baked exactly, so nothing in play moved; but a new colour needs no file and no palette entry, and the old pool of ramps and passes is gone from this page because every colour is now possible. Refer to a colour by its <b>ID</b> (the ramp-and-pass name it kept) or by its <b>hex</b>. The <b>field index</b> is its number in the 12-bit colour field of the draw flags.</p>

<section class="panel"><header><h2>Colours in use</h2><span class="meta">$inUse names live code draws with · samples on the level palette · the hex is where the glyph's brightest level lands, the swatches are the shades its bevel walks through · "callers" counts UiFlags::Color… in Source</span></header>
<div class="scroll"><table><thead><tr><th>ID</th><th>Sample (Font 12, 2x)</th><th>Name in code</th><th>Field</th><th>Hex</th><th>Defined by</th><th>Callers</th><th>Used for</th></tr></thead><tbody>$body</tbody></table></div>
</section>

<section class="panel"><header><h2>Shipped, not drawn</h2><span class="meta">names and files with no caller; loaded, cost nothing, kept for the day something asks</span></header>
<div class="scroll"><table><thead><tr><th>Name in code</th><th>File</th><th>Note</th></tr></thead><tbody>$parkedRows</tbody></table></div>
</section>

<section class="panel steps"><header><h2>Adding a colour</h2><span class="meta">no .trn since stage 3</span></header>
<ol>
<li>Append the name to <code>text_color</code> in <code>engine/render/text_render.hpp</code> and a <code>nullptr</code> to <code>ColorTranslations</code> in <code>text_render.cpp</code> (the enum indexes it positionally; grow both array sizes).</li>
<li>Add <code>{ ColorName, 0xRRGGBB }</code> to <code>RgbDefinedColors</code> in <code>text_render.cpp</code>. That is the colour. Its bevel shades follow the font's own ramp automatically.</li>
<li>Add a <code>UiFlags::Color…</code> name in <code>DiabloUI/ui_flags.hpp</code> with the next free field index and a <code>case</code> for it in <code>GetColorFromFlags</code>.</li>
<li>Add the row here (<code>tools/BuildFontColourLegend.pl</code>, <code>\@rows</code> and <code>%valueOf</code>) and rebuild the legend and the wiki. No MPQ repack: there is no file.</li>
</ol>
<p class="warn">Offscreen 8-bit surfaces and the golden tests still draw through the .trn, so a colour defined only by value has no look there; that is by design, nothing in play draws to one.</p>
</section>
X
my $out = "$repo/.ProjectDocumentation/06-Reference/Font-Colour-Legend.html";
open my $O, '>', $out or die; print $O $page; close $O;
print "ok ", length($page), " bytes, $inUse colours in use\n";
