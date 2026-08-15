---
date: 2026-08-15
version: 1.6.2
area: Monsters / lesser uniques
---

# A Thousand Names From Seventy-Six Words

Two requests, both about the same thing: what happens when a floor wants more champions than it has
distinct ones to give.

> "no problem to use same monsters. you can rename them randomly. make a thousand random names
> namebook and draw from there."
> "you can just recolor them a little bit."

## The problem being solved

1.6.1 allowed a champion identity to repeat with a different modifier, because Torment asking for six
packs on dungeon level 2 was simply getting fewer — a floor loads four monster types, and only some of
those have champions written for them, so the pool ran dry and placement stopped.

That fix left a visible seam. Two packs on one floor, both called **Rotfeast the Hungry**, both the
same shade of green. Correct, and it reads as a duplication bug.

## The namebook: 1,300 names, 76 strings

A literal list of a thousand names is a thousand strings to write, a thousand to translate, and a
thousand to maintain. **50 given names × 26 epithets = 1,300 combinations from 76 entries** — the same
variety at a fifteenth of the size, and it stays editable: adding one given name adds twenty-six
names, not one.

```cpp
const uint32_t seed = monster.aiSeed;
const char *given   = GivenNames[seed % GivenNameCount];
const char *epithet = Epithets[(seed / GivenNameCount) % EpithetCount];
return StrCat(given, " ", _(epithet));
```

The division matters. Both draws come from one seed, but they read different parts of the number, so
the given name and the epithet don't march in lockstep as the seed increments.

### The seed is the whole trick

`aiSeed` is **already per-monster and already saved**. Deriving the name from it rather than storing it
means a champion keeps its name across a save and reload without costing a field, a string table, or a
third conversation about the save format. This is the same instinct that got the affix into an
existing `Unused` byte at 1.6.0 — look at what the format already carries before adding to it.

### Translation, split down the middle

Given names are not marked for translation; epithets are. "Malgrith" is Malgrith in every language, and
asking a translator to render fifty invented proper nouns produces fifty entries of busywork that come
back unchanged. "the Marrowdrinker" is an English phrase and reads as one, so only those 26 carry
`N_()`.

## The recolour: a step along its own ramp

`TintLesserUnique` shifts each entry of the champion's TRN **−2 to +2 within its own 16-shade ramp**,
derived from the same seed:

```cpp
const int ramp   = mapped & 0xF0;
const int within = (mapped & 0x0F) + shift;
```

Three decisions inside four lines:

**Along the ramp, not across it.** The palette's six ramps are contiguous 16-shade runs. Moving within
one makes the champion paler or darker; moving between them would change its hue, and the game's
per-unique palettes are hand-picked — a green Rotfeast is green on purpose.

**Entries below 128 are skipped.** That half of the palette is level-specific and differs per dungeon
type, so a shift there means one colour in the Cathedral and a different one in the Caves.

**Clamped by skipping, not saturating.** A colour already at the end of its ramp stays put. Saturating
would pile several distinct shades onto one index and flatten the sprite's shading into a block.

A shift of 0 is a valid roll and returns early — one champion in five is simply its authored colour,
which is right: the tint is variation, not a uniform.

## What the health bar says now

The borrowed champion's name is **dropped entirely**. Not "Warded Rotfeast the Hungry" — **"Warded
Malgrith the Unclean"**.

A lesser unique borrows a sprite and a stat line, not an identity. Keeping the original name would
tell the player they'd met the floor's real champion when they hadn't, and it's what made a repeat
read as a bug in the first place. The sprite still recurs on a floor that loads four monster types —
that's unavoidable — but the character never does.

## State

**354/356.** `Drlg_l1.CreateL5Dungeon_diablo_3_844660068` and `Timedemo.WarriorLevel1to2`, both red
before any of this work began.

Not tested in-game — the user runs the game. Worth watching first: whether the longer generated names
still fit the health bar box (they average about the same length as the champion names they replace,
but "Marrowdrinker" is the long tail), and whether a ±2 tint is visible enough to tell two packs apart
at a glance or wants widening to ±3.
