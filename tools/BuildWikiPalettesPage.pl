# Builds wiki/palettes.html: every palette the game can have active, one 16x16 grid each, drawn as
# LoadPalette leaves it (the fork's green ramp injected at 152-159 on every in-game palette).
# Sources: the .pal files pulled out of diabdat.mpq / hellfire.mpq into <paldir>, and the front-end
# palette from Packaging. Usage: perl wikipalettes.pl <paldir> <repo>
use strict; use warnings;
my ($paldir, $repo) = @ARGV;
die 'usage: wikipalettes.pl <paldir> <repo>' unless $paldir && $repo;
my @green = ([140,190,140],[100,160,100],[62,130,62],[34,110,34],[24,90,24],[16,70,16],[10,50,10],[4,28,4]);
sub slurp { my $f = shift; open my $F, '<:raw', $f or die "$f: $!"; local $/; my $d = <$F>; close $F; return $d; }
sub pal { my ($f, $ingame) = @_; my $d = slurp($f); my @p = map { [unpack('C3', substr($d, $_ * 3, 3))] } 0 .. 255; if ($ingame) { $p[152 + $_] = $green[$_] for 0 .. 7; } return \@p; }
sub grid {
	my ($p, $mark) = @_;
	my $h = '<div class="pal">';
	for my $i (0 .. 255) {
		# The VALUES are Blizzard's (IP audit, 2026-09-08: a page carrying all 256 RGB entries of each
		# .pal is the file in another encoding). Only the fork's own eight injected entries are drawn
		# in colour; every other cell shows its index and the half it belongs to.
		my $cls = ($i >= 128) ? ' s' : '';
		if ($mark && $i >= 152 && $i < 160) {
			my ($r, $g, $b) = @{ $p->[$i] };
			$h .= sprintf('<i class="c%s g" style="background:#%02x%02x%02x" title="%d: #%02x%02x%02x (the fork\'s green)"></i>', $cls, $r, $g, $b, $i, $r, $g, $b);
		} else {
			$h .= sprintf('<i class="c%s" style="background:%s" title="%d"></i>', $cls, $i >= 128 ? '#3a3631' : '#1e1b18', $i);
		}
	}
	return $h . '</div>';
}
my @groups = (
	['Town', 'levels\towndata\town.pal, loaded whenever the level type is town', [['town.pal', "$paldir/levels/towndata/town.pal"]]],
	['Cathedral (levels 1-4)', 'one of four, rolled when the level is built: levels\l1data\l1_N.pal', [map { ["l1_$_.pal", "$paldir/levels/l1data/l1_$_.pal"] } 1 .. 4]],
	['Catacombs (levels 5-8)', 'levels\l2data\l2_N.pal', [map { ["l2_$_.pal", "$paldir/levels/l2data/l2_$_.pal"] } 1 .. 4]],
	['Caves (levels 9-12)', 'levels\l3data\l3_N.pal', [map { ["l3_$_.pal", "$paldir/levels/l3data/l3_$_.pal"] } 1 .. 4]],
	['Hell (levels 13-16)', 'levels\l4data\l4_N.pal', [map { ["l4_$_.pal", "$paldir/levels/l4data/l4_$_.pal"] } 1 .. 4]],
	['Crypt (Hellfire)', 'nlevels\l5data\l5base.pal, always the one', [['l5base.pal', "$paldir/nlevels/l5data/l5base.pal"]]],
	['Nest (Hellfire)', 'nlevels\l6data\l6baseN.pal; the roll skips base1 unless Alternate Nest Art is on', [map { ["l6base$_.pal", "$paldir/nlevels/l6data/l6base$_.pal"] } 1 .. 5]],
);
my $body = '';
for my $g (@groups) {
	my ($title, $note, $files) = @$g;
	$body .= qq{<h2>$title</h2>\n<p class="lede">$note</p>\n<div class="pals">\n};
	for my $f (@$files) {
		my $p = pal($f->[1], 1);
		$body .= qq{<figure class="shot"><figcaption><b>$f->[0]</b><span>as loaded: green injected at 152-159</span></figcaption>} . grid($p, 1) . qq{</figure>\n};
	}
	$body .= "</div>\n";
}
my $menu = pal("$repo/Packaging/resources/assets/ui_art/diablo.pal", 0);
$body .= qq{<h2>The menus</h2>\n<p class="lede">ui_art\\diablo.pal, the front end's own. Not an in-game palette, so no green is injected - and its shared-half ramps sit one row (16 indices) above a level's, which is why a text colour made for the menus is off by a ramp in play.</p>\n<div class="pals"><figure class="shot"><figcaption><b>diablo.pal</b><span>as shipped</span></figcaption>} . grid($menu, 0) . qq{</figure></div>\n};

my $page = <<"X";
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Palettes - Diablo Orcl</title>
<link rel="stylesheet" href="wiki.css">
</head>
<body>
<div class="shell">
<nav class="side"></nav>
<main>
<style>
.pals{display:flex;flex-wrap:wrap;gap:14px;margin:10px 0 6px}
.pal{display:grid;grid-template-columns:repeat(16,14px);grid-auto-rows:14px;gap:1px;background:#000;padding:2px;border:1px solid var(--edge)}
.pal i.c{display:block}
.pal i.s{outline:1px solid rgba(255,255,255,.06);outline-offset:-1px}
.pal i.g{box-shadow:inset 0 0 0 2px #fff}
figure.shot{width:auto}
figure.shot figcaption{margin-bottom:6px}
.key{display:flex;gap:18px;flex-wrap:wrap;font-size:13px;color:var(--ink-dim);margin:8px 0 0}
.key i{display:inline-block;width:12px;height:12px;vertical-align:middle;margin-right:5px;border:1px solid #555}
</style>
<h1>Palettes</h1>
<p class="lede">Every palette the game can have active, as it looks once LoadPalette has finished with it.
Each grid is the LAYOUT of one .pal file, index 0 top-left and reading across, so row 9 begins at
index 128. The colour values themselves are Blizzard's and are not reproduced here; the cells show
their index and which half they belong to. The top half, 0-127, is the level's own scenery colours and differs from file to file. The
bottom half, 128-255, is the shared half: identical in every in-game palette, laid out as ramps of one
hue running light to dark, and the only half that monsters, items, cursors and text may use. Hover a
cell for its index.</p>
<div class="note"><b>The fork's one change.</b> On every in-game palette the eight entries at 152-159,
vanilla's orange minis, are rewritten as a forest-green ramp at load. The cells with a white inset ring
are those eight. No .pal file was edited; the green exists only in memory. The current zone table names
no palette override, so every level type loads what is listed here.</div>
<div class="key"><span><i style="background:#3e823e;box-shadow:inset 0 0 0 2px #fff"></i>injected green (152-159)</span><span><i style="background:#888;outline:1px solid rgba(255,255,255,.3)"></i>shared half (128-255)</span></div>
$body
<h2>Where the shared half's ramps sit</h2>
<div class="tablewrap"><table>
<thead><tr><th class="num">Indices</th><th>Ramp</th><th class="num">Entries</th><th>Legend ID</th><th>Note</th></tr></thead>
<tbody>
<tr><td class="num">128-135</td><td>bright blue minis</td><td class="num">8</td><td>BB</td><td></td></tr>
<tr><td class="num">136-143</td><td>bright red minis</td><td class="num">8</td><td>BR</td><td></td></tr>
<tr><td class="num">144-151</td><td>bright yellow minis</td><td class="num">8</td><td>YL</td><td></td></tr>
<tr><td class="num">152-159</td><td>green minis</td><td class="num">8</td><td>GN</td><td>injected by the fork; vanilla's orange minis</td></tr>
<tr><td class="num">160-175</td><td>beige</td><td class="num">16</td><td>BE</td><td>rose in most level files</td></tr>
<tr><td class="num">176-191</td><td>steel blue</td><td class="num">16</td><td>BL</td><td></td></tr>
<tr><td class="num">192-207</td><td>gold</td><td class="num">16</td><td>GD</td><td>the band the font glyphs are painted on</td></tr>
<tr><td class="num">208-223</td><td>orange</td><td class="num">16</td><td>OR</td><td></td></tr>
<tr><td class="num">224-239</td><td>red</td><td class="num">16</td><td>RD</td><td></td></tr>
<tr><td class="num">240-254</td><td>gray</td><td class="num">15</td><td>GR</td><td>255 is pure white, not part of the ramp</td></tr>
</tbody></table></div>
<p>In the menu palette the same ramps sit sixteen lower: gold at 176, orange at 192, red at 208,
gray at 224, and the bright yellow minis at 128. See <a href="colours.html">Text colours</a> for what
that does to a .trn.</p>
<footer></footer>
</main>
</div>
<script src="data.js"></script>
<script src="wiki.js"></script>
</body>
</html>
X
open my $O, '>:raw', "$repo/wiki/palettes.html" or die; print $O $page; close $O;
print "ok ", length($page), "\n";
