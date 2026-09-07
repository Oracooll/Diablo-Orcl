# Wraps the font colour legend (06-Reference/Font-Colour-Legend.html) in the wiki's page shell as
# wiki/colours.html. The legend's own styles are scoped under .legend so nothing leaks into the
# bundle's other sections, and its body/root rules are dropped in favour of the wiki's.
use strict; use warnings;
my $repo = 'C:/Users/hroga/OneDrive/2. Personal Files/Software/Diablo/Diablo Orcl';
my $src = "$repo/.ProjectDocumentation/06-Reference/Font-Colour-Legend.html";
open my $I, '<:raw', $src or die "$src: $!"; local $/; my $h = <$I>; close $I;

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
