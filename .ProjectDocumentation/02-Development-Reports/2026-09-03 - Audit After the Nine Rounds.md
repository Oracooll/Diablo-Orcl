# Audit after the nine rounds (v1.9.192)

Nine rounds of the inert-skill plan went in against unit tests only. This pass read the new code
against the project's known failure modes — statics outliving the game, table/enum drift, masks
that stopped at 64, hook ordering, lifetime of timed state — and the hero-file format.

## Findings, fixed

**1. No non-Paladin tree skill could be readied (blocker).** `InnateSpellsBitmask` granted the
Paladin's seven skills and nothing else. Every castable row the plan added for the other five
classes — the cold page, the bow and melee pages, the cries, the javelins, some ninety rows — was
selectable only if it sat in a mask, and nothing put it in one. A Barbarian could buy Bash, read
its sentence, and never ready it: the picker, the speedbook and the wells all list what the masks
say. Fixed by widening the Paladin's rule to every class: an implemented Active row with a SpellID,
unlocked, with a point in it, is innate — except a row that rides a book spell, which stays a book
spell. Test: `OracoolClassSkills.AnInvestedTreeRowIsInnateForEveryClass`.

**2. The speedbook stopped at the 64th spell.** `spell_list.cpp` walked a `uint64_t` bit with
`spl <<= 1`, which is zero from the 65th spell on, so no spell past id 64 could appear in the
quick list or be given an F-key from it. The masks widened to 128 bits in Round 2; this walk was
never converted. Now asks `GetSpellBitmask`.

**3. A cry's buff was wiped at every level change without a sheet recompute.** `ClearWarcries`
ran from `InitMonsters` (once per level) and emptied the caster's buffs along with the monsters'
debuffs — so a Shout's +50% armour stayed baked into the sheet until something else recomputed
it, and the buff itself was lost on the stairs. Split: the per-level clear now takes only the
monsters' side (debuffs, conversions, wards); `ClearWarcryBuffs(player)` runs where a new game
clears the cold armour, and recomputes the sheet if a sheet buff was live. Buffs now walk down the
stairs with the caster and cannot carry to the next character. The passive clocks (the cheat-death
cooldown among them) got the same per-player clear at the same site.

**4. `SpellsData` had no size pin.** It is indexed by SpellID positionally and grew by hand five
times today. One row short or over and every later spell wears the wrong name, price and missile
with no error at the point it is caused. `static_assert` added.

**5. A stale test comment** called Holy Freeze, Sanctuary and Redemption "auras with no channel";
they work off the sheet now. Reworded; the assertion (nothing on the totals) is still right.

## Checked and sound

- The hero file: `PlayerPack::pSplLvl` is fixed at 37 (+10 Hellfire) and `ReadHero` demands an
  exact size, so growing MAX_SPELLS did not touch the fixed struct. The MAX_SPELLS-sized array is
  the network pack, never persisted. The investment chunk is count-prefixed and clamps on read.
- The five `SpellsData` insertions are in enum order; the pin now proves it every build.
- `SpellID` is `int8_t`; 126 fits, and the compiler refuses an enumerator past 127.
- No new warnings from the eleven new or touched files; every warning in the build log predates
  the plan.
- Damage-taken passives are applied before the floating number is shown and before Mana Shield.
- The cheat-death save returns before `SyncPlrKill`; the per-level clear no longer resets it.

## Still unverified

Everything above is unit-tested; none of it has been seen on screen. The things most likely to
surprise in a play session: the melee extra blows through an empty front tile, Howl and Grim Ward
against a pack with a leader, and a sheet buff recomputing the hero sheet mid-fight.
