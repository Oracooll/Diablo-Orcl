# Wraps the font colour legend (06-Reference/Font-Colour-Legend.html) in the wiki's page shell as
# wiki/colours.html. The legend's own styles are scoped under .legend so nothing leaks into the
# bundle's other sections, and its body/root rules are dropped in favour of the wiki's.
use strict; use warnings;
my $repo = 'C:/Users/hroga/OneDrive/2. Personal Files/Software/Diablo/Diablo Orcl';
my $src = "$repo/.ProjectDocumentation/06-Reference/Font-Colour-Legend.html";
open my $I, '<:raw', $src or die "$src: $!"; local $/; my $h = <$I>; close $I;

sub slurp_src { open my $F, '<:raw', $_[0] or die "$_[0]: $!"; local $/; my $d = <$F>; close $F; return $d; }

my ($css) = $h =~ m{<style>(.*?)</style>}s or die 'no style';
my ($body) = $h =~ m{</style>\s*(.*)\z}s or die 'no body';

# scope the CSS: drop :root and body rules, prefix the rest with .legend
my @rules = split /\}/, $css;
my $scoped = '';
for my $r (@rules) {
	next unless $r =~ /\S/;
	my ($sel, $decl) = split /\{/, $r, 2;
	next unless defined $decl;
	$sel =~ s/^\s+|\s+$//g;
	next if $sel eq 'body' || $sel eq ':root';
	my @sels = map { my $s = $_; $s =~ s/^\s+|\s+$//g; ".legend $s" } split /,/, $sel;
	$scoped .= join(',', @sels) . '{' . $decl . "}\n";
}
# the legend's tokens, on the wrapper rather than :root
my ($tokens) = $css =~ m/:root\{([^}]*)\}/ or die 'no tokens';
$scoped = ".legend{$tokens}\n" . $scoped;

# drop the legend's own h1 (the page has one) and its lede's first sentence is fine as is
$body =~ s{<h1>.*?</h1>\s*}{}s;

# ---- the damage palette: which colour each element is written in.
#
# A player learns this from the floating numbers, and until now the wiki only said it sideways, in
# the "Used for" column of the table below - which is exactly how it went stale when the palette was
# settled at v1.11.082 (physical took white, cold took blue back off magic, acid moved to poison
# green). So it is stated once, here, and DERIVED: charpanel's DamageTypeColor is the engine's only
# table for this, and the hex comes from the legend's own rows, so neither half can drift from the
# game without this page changing with it.
my %hexOf;
while ($body =~ m{class="name">(Color\w+)<.*?class="hex">(#[0-9a-f]{6})}gs) { $hexOf{$1} = $2; }

my ($fn) = slurp_src("$repo/Source/panels/charpanel.cpp") =~ m{UiFlags DamageTypeColor\(DamageType type\)\s*\{(.*?)\n\}}s
	or die 'no DamageTypeColor';
my ($fallback) = $fn =~ m{\}\s*return UiFlags::(Color\w+);}s or die 'no DamageTypeColor fallback';
my %colorOf;
my @order;
while ($fn =~ m{case DamageType::(\w+):(.*?)(?=case DamageType::|\n\t\})}gs) {
	my ($elem, $arm) = ($1, $2);
	# an arm that breaks instead of returning takes the fallback after the switch - that is how
	# physical is written, because white is the one element that wants the default
	my ($c) = $arm =~ m{return UiFlags::(Color\w+);};
	push @order, $elem;
	$colorOf{$elem} = $c // $fallback;
}
die 'DamageTypeColor: expected six elements, got ' . scalar(@order) unless @order == 6;
{	# a collision here means two elements share an ink, which the engine asserts against
	my %seen; my @dup = grep { $seen{ $colorOf{$_} }++ } @order;
	die 'DamageTypeColor: colour shared by ' . join(', ', @dup) if @dup;
}
my $damageRows = join '', map {
	my $c = $colorOf{$_};
	my $hex = $hexOf{$c} // '';
	my $sw = $hex ? qq{<i style="display:inline-block;width:11px;height:11px;vertical-align:-1px;margin-right:6px;background:$hex"></i>} : '';
	qq{<tr><td><b>$_</b></td><td>$sw<code>$hex</code></td><td class="mono">$c</td></tr>\n};
} @order;

my $page = <<"X";
<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Text colours - Diablo Orcl</title>
<link rel="stylesheet" href="wiki.css">
</head>
<body>
<div class="shell">
<nav class="side"></nav>
<main>
<h1>Text colours</h1>
<p class="lede">Every colour a string can wear in play, and the ID to ask for it by. The samples are
the game's own Font 12 glyphs on the runtime palette; the recipes are the .trn files that make them.
A colour is a 256-entry remap: the glyphs are authored on the gold band 192-207 and the .trn moves
that band onto one of the palette's ten ramps.</p>

<h2>The damage palette</h2>
<p>Every number that floats off a monster is written in its element's colour, and the character
sheet colours its damage rows the same way - one table serves both, so a number over a head and a
row on the sheet always agree. The scheme is Diablo II's, taken literally.</p>
<div class="tablewrap"><table>
<thead><tr><th>Element</th><th>Colour</th><th>Name in code</th></tr></thead>
<tbody>
$damageRows</tbody></table></div>
<div class="note"><b>Blue does double duty.</b> On the character sheet blue also marks a buffed or
active row. That is an accepted ambiguity rather than an oversight: on the sheet a blue damage row
<i>is</i> the row being coloured by its element, and over a monster's head there is no buffed row
for it to be confused with. Acid is monster-only - no player spell carries it - so it never appears
on the sheet, but it still needs an ink of its own for the floating numbers.</div>

<div class="legend">
<!-- styles inside main on purpose: the bundle keeps only <main>, so a <head> style would be lost -->
<style>
$scoped</style>
$body
</div>
<footer></footer>
</main>
</div>
<script src="data.js"></script>
<script src="wiki.js"></script>
</body>
</html>
X
open my $O, '>:raw', "$repo/wiki/colours.html" or die; print $O $page; close $O;
print "ok ", length($page), "\n";
