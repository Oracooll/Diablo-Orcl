---
date: 2026-08-15
version: 1.5.59
area: Save format / readied spells
---

# Two Bytes That Were Already There

> make the readied spells persist across saves

I had told the user this would cost them every hero they own. It didn't, and the reason is worth
writing down, because the same reasoning applies to the next field that wants space in `PlayerPack`.

## What I said, and why it was wrong

When the left mouse button gained its own readied spell at 1.5.58 I closed with a warning:

> Making it persist means changing `sizeof(PlayerPack)`, and `ReadHero` accepts only an exact size
> match, so it would invalidate every existing hero.

That is true of **appending** to the struct, which is what I had in mind. It is not true of the
change itself. `pfile.cpp`'s `ReadHero` only cares about the total size; it has no opinion about
what the bytes inside mean. So the question was never "can this be persisted", it was "is there a
byte inside the struct that nobody is using". There were two.

## The two bytes

| Byte | Was | Who wrote it |
|---|---|---|
| `PlayerPack::pReadiedSpellRight` | `pBattleNet` | `PackPlayer` memsets it and never writes it; `loadsave.cpp` only `file.Skip(1)`s past it. The sole writer that ever set it was the original 1.09 game, whose `.sv` heroes this fork stopped being able to read when it moved to Hellfire's `.hsv`. |
| `PlayerPack::pReadiedSpellLeft` | `reserved // For future use` | Nobody. This was the future use. |

Both were already zero in every hero file this fork has ever written, which is what makes the
change free rather than merely cheap: **zero is the encoding for "nothing readied"**, so a hero
saved last week decodes correctly without a migration, a version stamp, or a fallback path.

`Writehero.pfile_write_hero` hashes the whole written hero blob against a golden SHA-256 and it
passed **unchanged**. The on-disk format is byte-identical. That is the proof, not my assurance.

## Why the spell type is not stored

The obvious encoding is four bytes: spell and type, twice. I only had two, so the type had to go —
and losing it turned out to be an improvement rather than a compromise.

A `SpellType` is one of Skill, Spell, Scroll or Charges. The last two name an **item**. Restoring
"Scroll of Town Portal is on your right button" into a session where the scroll has been read is
restoring a binding that silently does nothing. Skill and Spell are the two kinds that belong to the
character rather than to their bags, and those are exactly the two that can be re-derived from
`_pAblSpells` and `_pMemSpells` on load, for free.

So `oracool/readied_spells.cpp` stores the spell id plus one — `+1` so that `SpellID::Null` (0) and
"nothing readied" stay distinguishable — and derives the type. A spell the character no longer has
(an unequipped staff, a skill not yet unlocked at this level) resolves to `Invalid` and is dropped
rather than restored as an uncastable binding. That also means a garbage byte, if one ever arrived
from an unexpected writer, cannot produce a live binding.

## Ordering, which is the whole of the bug that didn't happen

`UnPackPlayer` decodes the two bindings **last**, after `CalcPlrInv`. Three separate reasons, all of
which would have been a bug if I had put the call anywhere earlier:

1. The type is derived from `_pAblSpells`, which `InitPlayer` builds, and `_pMemSpells`, read
   further down. Decoding before either yields `Invalid` for everything.
2. `CalcPlrInv` is what auto-readies an equipped staff's charged spell when nothing else is readied
   (`player.cpp:1882`). Decoding before it means the staff overwrites the player's saved choice.
3. And for the same reason the decoder **leaves both pairs untouched** when the byte is zero,
   instead of writing `Invalid`. A hero saved with nothing readied must still get their staff.

## The game save too

`loadsave.cpp` already stored `_pRSpell` and its type; it had no idea `_pLRSpell` existed. Persisting
only half of a pair means the left binding evaporates on any reload, which is a worse bug than not
persisting at all — it looks like the feature is broken rather than absent. The same trick applied:
vanilla's `_pTSplType` byte, which devilutionX has only ever `Skip`ped on both read and write, now
carries the left binding.

## One thing that was quietly wrong

`InitPlayer(player, firstTime: true)` resets `_pRSpell` to `Invalid` and never touched the left pair,
so value-initialising a `Player` left `_pLRSpell` at `SpellID::Null` (0), not `Invalid` (-1). Nothing
misbehaved — `IsValidSpell` rejects `Null` too, so the left button still swung — but "left click
attacks" now had two spellings, and only one of them was checked for anywhere else. Reset added.

## State

352/354, the standing baseline (`Drlg_l1.CreateL5Dungeon_diablo_3_844660068` and
`Timedemo.WarriorLevel1to2`, both failing before this change). `Writehero` and the `Pack` fixtures
pass untouched.

Not tested in-game — the user runs the game.
