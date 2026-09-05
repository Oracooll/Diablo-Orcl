#!/usr/bin/perl
# Generates the hover matrix artifact from the source tables: every class tree row (class_tree.cpp)
# and every book spell (spell_book.cpp's SpellPages, spelldat.cpp names, spell_descriptions.cpp).
use strict; use warnings;
my $root = '.';
my $outPath = shift @ARGV or die "out path";

sub slurp { my $f = shift; open my $h, '<', $f or die "$f: $!"; local $/; my $s = <$h>; close $h; $s }

# ---- tree rows -------------------------------------------------------------------------------
my $tree = slurp("$root/Source/oracool/class_tree.cpp");
my ($table) = $tree =~ /const ClassTreeSkillData Skills\[ClassTreeSkillCount\] = \{(.*?)\n\};/s or die "no Skills table";
my @rows;
while ($table =~ /\{\s*N_\("((?:[^"\\]|\\.)*)"\),\s*N_\("((?:[^"\\]|\\.)*)"\),\s*(Pal|Bar|Sor|Rog|Bard|Monk),\s*(\d+),\s*(\d+),\s*(\d+),\s*Kind::(\w+),\s*SpellID::(\w+),\s*(true|false)/sg) {
  push @rows, { name => $1, desc => $2, cls => $3, page => $4, tier => $5, col => $6, kind => $7, spell => $8, impl => $9 eq 'true' };
}
die "only ".scalar(@rows)." rows" if @rows < 150;

my %className = (Pal => 'Paladin', Bar => 'Barbarian', Sor => 'Sorceress', Rog => 'Rogue', Bard => 'Bard', Monk => 'Monk');
my %pages = (
  Pal => ['Combat Skills', 'Offensive Auras', 'Defensive Auras', 'Passive Skills'],
  Bar => ['Combat Skills', 'Combat Masteries', 'Warcries', 'Passive Skills'],
  Sor => ['Cold Spells', 'Lightning Spells', 'Fire Spells', 'Passive Skills'],
  Rog => ['Bow & Crossbow', 'Passive & Magic', 'Javelin & Spear', 'Passive Skills'],
  Bard => ['Melody', 'Harmony', 'Poetry', 'Passive Skills'],
  Monk => ['Way of the Staff', 'Way of the Body', 'Way of the Spirit', 'Passive Skills'],
);

# ---- book spells -----------------------------------------------------------------------------
my $book = slurp("$root/Source/panels/spell_book.cpp");
my ($pagesTable) = $book =~ /const SpellID SpellPages\[SpellBookPages\]\[SpellBookPageEntries\] = \{(.*?)\n\};/s or die "no SpellPages";
my @bookIds;
while ($pagesTable =~ /SpellID::(\w+)/g) { push @bookIds, $1 unless $1 eq 'Invalid' || $1 eq 'TownPortal' || $1 eq 'Null'; }
my $dat = slurp("$root/Source/spelldat.cpp");
my %spellName;
while ($dat =~ m{/\*SpellID::(\w+)\*/\s*\{\s*P_\("spell",\s*"([^"]*)"\)}g) { $spellName{$1} = $2; }
my $descs = slurp("$root/Source/oracool/spell_descriptions.cpp");
my %spellDesc;
while ($descs =~ m{/\*\s*(\w+)\s*\*/\s*N_\("((?:[^"\\]|\\.)*)"\)}g) { $spellDesc{$1} = $2; }

# ---- html helpers ----------------------------------------------------------------------------
sub esc { my $s = shift; $s =~ s/&/&amp;/g; $s =~ s/</&lt;/g; $s =~ s/>/&gt;/g; $s }
sub pop_ { my ($lines) = @_; '<pre class="pop">' . join("\n", @$lines) . '</pre>' }
sub gold { '<b>' . esc(shift) . '</b>' }
sub rt { '<i>' . esc(shift) . '</i>' }
sub none { '<span class="none">not hoverable here</span>' }

my $curTree = gold('Current Skill Level: {p}');
my $nextTree = gold('Next Level') . "\n" . 'Requires level ' . rt('{L}');
my $unlearned = 'Not learned' . "\n\n" . gold('First Level') . "\n" . 'Requires level ' . rt('{L}');

sub activeLines { my $spell = shift; my @l = ('Damage: ' . rt('{min}') . ' - ' . rt('{max}') . '  ' . rt('(if the spell reports damage)'), 'Mana Cost: ' . rt('{mana}')); @l }
sub auraLines { my @l = (rt('{each bonus this rank grants, one per line, e.g. +20% damage}'), 'Radius: ' . rt('{r}') . ' tiles'); @l }
sub passiveLines { my @l = (rt('{each bonus this rank grants, one per line}')); @l }

sub abilitiesCell {
  my $r = shift;
  my @l = (gold($r->{name}), esc($r->{desc}), '');
  if ($r->{page} == 3) {
    push @l, 'Learned  ' . rt('(or: Learned at level {N})'), 'Active - slot ' . rt('{n}') . '  ' . rt('(or: Inactive - not in a slot)');
    push @l, 'No effect yet' unless $r->{impl};
    return pop_(\@l);
  }
  my @rank = $r->{kind} eq 'Active' ? ($r->{spell} ne 'Invalid' ? activeLines($r->{spell}) : ()) : $r->{kind} eq 'Aura' ? auraLines() : passiveLines();
  @rank = () unless $r->{impl} || $r->{kind} eq 'Active';
  push @l, $curTree, @rank, '', $nextTree, @rank, rt('(or: Fully invested)');
  push @l, 'No effect yet' unless $r->{impl};
  push @l, 'Breaks immunities at ' . rt('{N}') . ' points' if $r->{name} eq 'Conviction';
  pop_(\@l);
}
sub pickerCell {
  my $r = shift;
  return none() if $r->{page} == 3 || $r->{kind} eq 'Passive';
  my @l = (gold($r->{name}), rt('Right button only - click to light it') . '  ' . rt('(only while dimmed)'));
  if ($r->{kind} eq 'Active' && $r->{spell} ne 'Invalid') {
    push @l, 'Mana Cost: ' . rt('{mana}'), 'Damage: ' . rt('{min}') . ' - ' . rt('{max}') . '  ' . rt('(if reported)');
  } else {
    push @l, $curTree, auraLines();
    push @l, 'No effect yet' unless $r->{impl};
  }
  pop_(\@l);
}
sub lmbCell {
  my $r = shift;
  return none() unless $r->{kind} eq 'Active' && $r->{spell} ne 'Invalid';
  pop_([gold($r->{name}), 'Left click to use', 'Click here for abilities']);
}
sub rmbCell {
  my $r = shift;
  return pop_([gold('Select current spell button'), esc($r->{name}) . ' Aura', 'Burning']) if $r->{kind} eq 'Aura';
  return none() unless $r->{kind} eq 'Active' && $r->{spell} ne 'Invalid';
  pop_([gold('Select current spell button'), esc($r->{name}) . ' Skill']);
}

my $html = <<'HEAD';
<title>Skill Hover Matrix</title>
<link rel="stylesheet" href="https://fonts.googleapis.com/css2?family=Cinzel:wght@500;700&family=Source+Serif+4:ital,wght@0,400;0,600;1,400&family=IBM+Plex+Mono:ital,wght@0,400;1,400&display=swap">
<style>
  :root { --ground:#f1e9d6; --panel:#faf5ea; --ink:#2a241c; --ink-soft:#6b6152; --rule:#d6c9ad; --gold:#8a6a1c; --gold-soft:#c9b071; --popup:#efe5cf; --popup-ink:#2a241c; --slot:#e3d7bb; }
  @media (prefers-color-scheme: dark) { :root:not([data-theme="light"]) { --ground:#1c1815; --panel:#241f1a; --ink:#e6d9b8; --ink-soft:#9d917a; --rule:#3a332b; --gold:#d2ab4e; --gold-soft:#7c6532; --popup:#0e0c0a; --popup-ink:#e9ddbd; --slot:#2e2822; } }
  :root[data-theme="dark"] { --ground:#1c1815; --panel:#241f1a; --ink:#e6d9b8; --ink-soft:#9d917a; --rule:#3a332b; --gold:#d2ab4e; --gold-soft:#7c6532; --popup:#0e0c0a; --popup-ink:#e9ddbd; --slot:#2e2822; }
  body { background:var(--ground); color:var(--ink); font-family:"Source Serif 4",Georgia,serif; font-size:15px; line-height:1.45; margin:0; padding:32px 28px 56px; }
  header { max-width:76ch; margin-bottom:22px; }
  h1 { font-family:Cinzel,Georgia,serif; font-weight:700; font-size:30px; letter-spacing:.04em; color:var(--gold); margin:0 0 10px; text-wrap:balance; }
  h2 { font-family:Cinzel,Georgia,serif; font-weight:700; font-size:20px; letter-spacing:.05em; color:var(--gold); margin:36px 0 6px; }
  h3 { font-family:Cinzel,Georgia,serif; font-weight:500; font-size:14px; letter-spacing:.08em; color:var(--ink-soft); margin:18px 0 8px; text-transform:uppercase; }
  header p, .note { margin:0 0 8px; color:var(--ink-soft); }
  nav { display:flex; flex-wrap:wrap; gap:6px 16px; font-size:13.5px; margin-top:12px; }
  nav a { color:var(--gold); text-decoration:none; border-bottom:1px solid var(--gold-soft); }
  .legend { display:flex; gap:22px; flex-wrap:wrap; margin-top:12px; font-size:13px; color:var(--ink-soft); }
  .wrap { overflow-x:auto; border:1px solid var(--rule); background:var(--panel); }
  table { border-collapse:collapse; min-width:1180px; width:100%; }
  th, td { border-bottom:1px solid var(--rule); border-right:1px solid var(--rule); vertical-align:top; text-align:left; }
  th:last-child, td:last-child { border-right:0; }
  thead th { background:var(--slot); padding:10px 12px; font-family:Cinzel,Georgia,serif; font-weight:500; font-size:12.5px; letter-spacing:.06em; color:var(--gold); position:sticky; top:0; }
  thead th small { display:block; font-family:"Source Serif 4",Georgia,serif; letter-spacing:0; font-size:12.5px; color:var(--ink-soft); margin-top:3px; }
  tbody th { background:var(--slot); padding:10px 12px; font-family:Cinzel,Georgia,serif; font-weight:700; font-size:13.5px; letter-spacing:.03em; white-space:nowrap; position:sticky; left:0; }
  tbody th small { display:block; font-family:"Source Serif 4",Georgia,serif; font-weight:400; letter-spacing:0; color:var(--ink-soft); font-size:12px; margin-top:3px; }
  td { padding:8px 10px; }
  .pop { font-family:"IBM Plex Mono",Consolas,monospace; font-size:12px; line-height:1.5; white-space:pre-wrap; background:var(--popup); color:var(--popup-ink); border:1px solid var(--rule); padding:7px 9px; margin:0; max-width:46ch; }
  .pop b { font-weight:400; color:var(--gold); }
  .pop i { font-style:italic; color:var(--gold-soft); }
  td.c1 .pop { max-width:56ch; }
  .none { color:var(--ink-soft); font-style:italic; font-size:13px; }
  .col-w { width:34%; } .col-n { width:22%; } .col-s { width:15%; }
  footer { margin-top:30px; max-width:80ch; color:var(--ink-soft); font-size:13.5px; }
</style>
<header>
  <h1>Skill Hover Matrix</h1>
  <p>Every skill, aura, passive and book spell in Diablo Orcl, and what the hover popup prints in each of the four places it can be hovered. Generated from the source tables at v1.9.259: the class tree table, the spell book pages, the spell names and the spell descriptions.</p>
  <div class="legend"><span>Gold lines are drawn gold in the game.</span><span><i>{braces}</i> are filled in at runtime from the character.</span></div>
HEAD

my @classes = qw(Pal Bar Sor Rog Bard Monk);
$html .= '<nav>' . join('', map { '<a href="#c-' . $_ . '">' . $className{$_} . '</a>' } @classes) . '<a href="#spells">Book spells</a></nav></header>';

my $thead = '<thead><tr><th>Row</th><th>1 · Abilities window<small>the tree page cell, hover panel</small></th><th>2 · Skill picker<small>quick list above LMB/RMB, cursor tooltip</small></th><th>3 · LMB well<small>readied on the left button</small></th><th>4 · RMB well<small>readied or burning on the right</small></th></tr></thead>';
my $cols = '<colgroup><col><col class="col-w"><col class="col-n"><col class="col-s"><col class="col-s"></colgroup>';

for my $c (@classes) {
  $html .= '<h2 id="c-' . $c . '">' . $className{$c} . '</h2>';
  for my $p (0 .. 3) {
    my @in = grep { $_->{cls} eq $c && $_->{page} == $p } @rows;
    next unless @in;
    @in = sort { $a->{tier} <=> $b->{tier} || $a->{col} <=> $b->{col} } @in;
    $html .= '<h3>' . $pages{$c}[$p] . ' · ' . scalar(@in) . ' rows</h3><div class="wrap"><table>' . $cols . $thead . '<tbody>';
    for my $r (@in) {
      my $sub = ($r->{page} == 3 ? 'passive page' : 'tier ' . ($r->{tier} + 1)) . ' · ' . lc($r->{kind}) . ($r->{impl} ? '' : ' · not built');
      $html .= '<tr><th>' . esc($r->{name}) . '<small>' . $sub . '</small></th>'
        . '<td class="c1">' . abilitiesCell($r) . '</td><td>' . pickerCell($r) . '</td><td>' . lmbCell($r) . '</td><td>' . rmbCell($r) . '</td></tr>';
    }
    $html .= '</tbody></table></div>';
  }
}

# book spells
$html .= '<h2 id="spells">Book spells</h2><h3>The Spells sheet · ' . scalar(@bookIds) . ' rows</h3><p class="note">Hovered on the Abilities window\'s Spells sheet rather than a tree page; the picker, LMB and RMB texts follow the spell path.</p><div class="wrap"><table>' . $cols
  . '<thead><tr><th>Spell</th><th>1 · Abilities window<small>Spells sheet row, hover panel</small></th><th>2 · Skill picker<small>Spells / Scrolls / Staff sections</small></th><th>3 · LMB well</th><th>4 · RMB well</th></tr></thead><tbody>';
my $curSpell = gold('Current Spell Level: {L}');
for my $id (@bookIds) {
  my $name = $spellName{$id} // $id;
  my $desc = $spellDesc{$id} // '';
  my @stat = ('Mana Cost: ' . rt('{mana}'), 'Damage: ' . rt('{min}') . ' - ' . rt('{max}') . '  ' . rt('(if the spell reports damage; Heals: for the two heals)'));
  my $c1 = pop_([gold($name), esc($desc), '', rt('Requires {N} Magic') . '  ' . rt('(only while short of it)'), $curSpell . '  ' . rt('(or: Not learned)'), @stat, '', gold('Next Level') . '  ' . rt('(First Level when unlearned)'), @stat]);
  my $c2 = pop_([gold($name), rt('Right button only - click to light it') . '  ' . rt('(only while dimmed)'), $curSpell . '  ' . rt('(or: Not learned)'), @stat]);
  my $c3 = pop_([gold($name), 'Left click to use', 'Click here for abilities']);
  my $c4 = pop_([gold('Select current spell button'), esc($name) . ' Spell', 'Spell Level ' . rt('{L}') . '  ' . rt('(or: Spell Level 0 - Unusable)'), rt('(a scroll: "Scroll of ' . $name . '" and its count; a staff: "Staff of ' . $name . '" and its charges)')]);
  $html .= '<tr><th>' . esc($name) . '<small>book spell</small></th><td class="c1">' . $c1 . '</td><td>' . $c2 . '</td><td>' . $c3 . '</td><td>' . $c4 . '</td></tr>';
}
$html .= '</tbody></table></div>';

$html .= <<'FOOT';
<footer>
  <p><strong>Where each column comes from.</strong> Column 1 is the row's sentence plus the Diablo II block: Current Skill Level with this rank's numbers, then Next Level with the next rank's and its level requirement. An active's numbers are the spell side's (damage, mana at that rank); an aura's or passive's are what ApplyAura / ApplyPassive grant at that rank, one bonus per line, plus an aura's radius. Column 2 is the picker's cursor tooltip: name and the current rank only. Columns 3 and 4 are the fixed well lines in control.cpp. Passive-page rows report Learned / slot state instead of ranks and appear nowhere but the Abilities window.</p>
  <p><strong>Not drawn.</strong> A row marked "not built" carries "No effect yet" and no rank numbers. Seven Paladin combat skills have a second sentence in paladin_skills.cpp that no surface prints.</p>
</footer>
FOOT

open my $o, '>:encoding(UTF-8)', $outPath or die $!; print $o $html; close $o;
printf "rows %d, book spells %d, names %d, descs %d -> %s\n", scalar(@rows), scalar(@bookIds), scalar(keys %spellName), scalar(keys %spellDesc), $outPath;
