#!/usr/bin/perl
# Audit: does each skill's SENTENCE agree with the FACTS its module rolls? The sentence says
# "+150% damage, +20% per rank"; the facts at rank 1 and 2 say "Damage: +150%" then "+170%".
# Compares the numbers, flags disagreement.
use strict; use warnings;
my ($treeFile, $tsv) = @ARGV;
open my $in, '<', $treeFile or die; my $src = do { local $/; <$in> }; close $in;
my ($table) = $src =~ /const ClassTreeSkillData Skills\[ClassTreeSkillCount\] = \{(.*?)\n\};/s or die;
my %desc;
while ($table =~ /\{\s*N_\("((?:[^"\\]|\\.)*)"\),\s*N_\("((?:[^"\\]|\\.)*)"\),\s*(Pal|Bar|Sor|Rog|Bard|Monk),\s*(\d+),/sg) {
  $desc{"$3|$4|$1"} = $2;
}
my %cls = (Paladin => 'Pal', Barbarian => 'Bar', Sorceress => 'Sor', Rogue => 'Rog', Bard => 'Bard', Monk => 'Monk');
my %pageIdx = (
  'COMBAT SKILLS' => 0, 'OFFENSIVE AURAS' => 1, 'DEFENSIVE AURAS' => 2, 'PASSIVE SKILLS' => 3,
  'COMBAT MASTERIES' => 1, 'WARCRIES' => 2, 'COLD SPELLS' => 0, 'LIGHTNING SPELLS' => 1, 'FIRE SPELLS' => 2,
  'BOW & CROSSBOW' => 0, 'PASSIVE & MAGIC' => 1, 'JAVELIN & SPEAR' => 2, 'MELODY' => 0, 'HARMONY' => 1, 'POETRY' => 2,
  'WAY OF THE STAFF' => 0, 'WAY OF THE BODY' => 1, 'WAY OF THE SPIRIT' => 2,
);
my %facts;
open my $fh, '<', $tsv or die; while (<$fh>) { chomp; my ($c, $p, $n, $r, $l) = split /\t/; $facts{"$cls{$c}|$pageIdx{$p}|$n"}{$r} = $l; } close $fh;

my ($checked, $flagged) = (0, 0);
for my $key (sort keys %facts) {
  my $d = $desc{$key}; next unless defined $d;
  my $f1 = $facts{$key}{1} // ''; my $f2 = $facts{$key}{2} // '';
  my @notes;
  # damage bonus: sentence "+X% damage" and "+Y% per rank"
  if ($f1 =~ /Damage: \+(\d+)%/) {
    my $r1 = $1; my ($r2) = $f2 =~ /Damage: \+(\d+)%/;
    my ($sX) = $d =~ /\+(\d+)% damage/; my ($sY) = $d =~ /\+(\d+)% per rank/;
    push @notes, "sentence has no +X% damage (facts +$r1%)" unless defined $sX;
    push @notes, "sentence +$sX% vs facts +$r1%" if defined $sX && $sX != $r1;
    if (defined $r2) { my $delta = $r2 - $r1; push @notes, "sentence +${sY}% per rank vs facts +$delta" if defined $sY && $sY != $delta; push @notes, "facts climb +$delta per rank, sentence says nothing per rank" if !defined $sY && $delta != 0; }
  }
  # strikes
  if ($f1 =~ /Strikes: (\d+)/) { my $n = $1; push @notes, "sentence never says how many blows (facts: $n)" unless $d =~ /\b(two|three|four|several|\d+)\b/i; }
  # durations
  if ($f1 =~ /Duration: (\d+) s/) { my $s = $1; my ($ds) = $d =~ /(\d+) seconds/; push @notes, "sentence $ds s vs facts $s s" if defined $ds && $ds != $s; push @notes, "facts last $s s, sentence has no duration" unless defined $ds; }
  # chance
  if ($f1 =~ /Chance: (\d+)%/) { my $c = $1; my ($dc) = $d =~ /(\d+)% of the time/; push @notes, "sentence $dc% vs facts $c%" if defined $dc && $dc != $c; }
  # sweep share
  if ($f1 =~ /around you at (\d+)% damage/) { my $s = $1; my ($ds) = $d =~ /at (\d+)% damage/; push @notes, "sentence $ds% vs facts $s%" if defined $ds && $ds != $s; }
  $checked++;
  if (@notes) { $flagged++; print "$key\n  sentence: $d\n  rank1: $f1\n  rank2: $f2\n  ", join("\n  ", @notes), "\n"; }
}
print "checked $checked skills with facts, flagged $flagged\n";
