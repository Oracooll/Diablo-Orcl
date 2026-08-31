# A fourth tree page, from Diablo III (v1.9.45)

Every class now has a **Passive Skills** sheet: 110 rows across six classes, named, described, and
deliberately inert. The user asked for placeholders with names and empty icons, and that is exactly
what shipped - but getting there ran into a hard limit and one genuine defect that only a test saw.

## The research, and the one class that had none

Five of the six map onto a Diablo III class without argument, and each takes its counterpart's list
verbatim from Blizzard's own class pages rather than from memory:

| Oracool | Diablo III | Passives |
|---|---|---|
| Paladin (Warrior) | Crusader | 18 |
| Barbarian | Barbarian | 19 |
| Sorceress | Wizard | 18 |
| Rogue | Demon Hunter | 19 |
| Monk | Monk | 18 |
| Bard | *none exists* | 18, authored |

The Bard was the only real decision, and it went to the user. D3 has no bard, and borrowing the
Witch Doctor's or the Necromancer's list would have put a witch doctor's passives on a lute. Hers
are authored in D3's idiom instead - Perfect Pitch, Crescendo, Sustain, Countermelody, Cadence,
Encore - which is what her three song pages already are: `class_tree.h` calls Melody/Harmony/Poetry
"the user's own design, not Diablo II's".

D3's own unlock levels (10 to 70) are **not** reproduced. The rows are laid out three to a tier down
this game's existing ladder, in D3's order. Same reasoning the file already gives for not rebuilding
D2's prerequisite graph: borrow the shape, do not invent the numbers and present them as someone
else's.

## Placeholders for free

The three things a placeholder needs were already in the engine, which is why the visible half of
this cost nothing:

- `implemented = false` makes a row inert. The UI draws it greyed with a red X.
- `spell_book.cpp` refuses to invest in an unimplemented row, so **no point can be sunk into one**.
- `DrawStripIcon` returns silently when the frame index is past the end of the strip, and the plate
  is drawn separately - so a passive with no art yet draws an **empty plate**, which is precisely
  the "empty icon" that was asked for. No art was needed to ship this.

`maxRank` is 1 on every one of them. A D3 passive is binary: you have it or you do not. The table's
default of 0 means "the usual cap", which here would have meant 98 ranks of nothing.

## The limit nobody had hit

`ClassTreeSkill` was `enum class : uint8_t` with `None = 0xFF`. 163 rows plus 110 is 273, so the
enum overflowed its own type and **0xFF became a real Rogue skill**. The compiler said so:

```
warning C4369: enumerator value '256' cannot be represented as 'unsigned char'
```

The enum, `Player::_pOracoolActiveAura` and `Missile::oracoolSkill` all had to widen together, and
`HeroChunkActiveAura` with them - one byte to two, little-endian. The endianness is load-bearing
rather than incidental: the chunk reader's own comment already said *"First byte only; anything
after it belongs to a newer build's larger payload"*, and byte 0 of a little-endian u16 is the low
byte, so an **older build still recovers any aura below 256** - and every aura row is. That is a
clean round trip, not merely a safe failure.

`sizeof(PlayerPack)` is untouched. Both format changes are tail chunks, which is the whole reason
the tail exists.

## The defect a test caught

`OracoolAudit.AMissileCarriesNoSkillUnlessACastGaveItOne` failed, and it was right to.
`Missile::oracoolSkill` was a `uint8_t` defaulting to `0xFF`. The moment `None` became `0xFFFF`,
every missile nobody cast - traps, monster attacks, town portals - carried skill id 255, which is
now a real Rogue passive, and would have rung its impact cue.

The test's own comment had predicted this failure mode exactly: *"a default of 0 rather than None
would make every one of them ring the impact cue of whichever skill happens to sit first in the tree
enum."* It was written against the wrong end of the range and caught the right bug anyway.

## What this does cost: a lit aura on a Bard or a Monk

The burning aura persists as an **absolute** `ClassTreeSkill`, so its meaning shifts whenever a class
earlier in the enum gains rows - which four of them just did. A Bard or Monk saved with a song or a
mantra lit now decodes to some other class's row at that number.

`GetActiveClassAura` grew a class check, so the outcome is **no aura** rather than **somebody else's
aura**, which is one click to restore. That guard is a seatbelt, not the fix; storing the
class-relative index is, and it is now a pipeline row of its own.

The passives themselves are appended at the END of each class block, which is what keeps every
existing skill's icon frame and investment slot exactly where it was - the same rule Hammer of Faith
and Blessed Shield followed for the same reason. A new test pins those boundaries.

## Verification

527/529, the two standing baseline failures. Three tests added, and the aura guard was **verified by
reintroducing the bug**: with the class check disabled, `AnAuraThatIsNotThisCharactersDoesNotBurn`
fails with "a Paladin is burning one of the Bard's songs". A test that has never failed has not been
tested.

The enum and the table were generated from one source file rather than hand-written twice. Index IS
the save slot here, so the two drifting apart would have silently reassigned live characters' points
- and 110 rows written out twice is exactly how that happens.

Not verified: how the page LOOKS. Six new sheets of empty plates is a screenshot question, and the
standing note that a screenshot is the only verification for anything drawn over the world applies
to windows too.
