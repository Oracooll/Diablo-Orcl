# 2026-10-05 - Audit of the user's five decisions (v1.12.410)

**Date:** 2026-10-05. Debug only. All 954 tests pass. The Debug `diablo.ini` was unchanged by ctest (md5 b3392ce4).

The user asked for an audit of v1.12.408-409. Two read-only reviews found no P1, three P2s and several P3s, all fixed here.

## Fixed (P2)
1. **The "once per spell" gate was one slot per monster for every spell.** A Fire Wall's or Inferno's ticks took the next Lightning bolt's Paralysis roll. Static Field and Thunder Storm blocked a third of every second. The slot is now kept per damage type, so other elements cannot use it up.
2. **Item knockback doubled a skill's own push, or cancelled its pull.**
   - **Harpoon:** with a Knockback item, its pull toward the hero did nothing.
   - **Dragon Tail Sweep, Whirling Kick and Aegis Slam:** they moved the first target two tiles.
   - **The fix:** these four strike with no item knockback (`SkillMovesTarget`), one push a blow, as Bash and Open Palm.
3. **The Counselor family's Firebolt carried only the byte roll.** `AddFirebolt` recomputes only a zero damage, so the overflow percent was missed. Every third cast of a Counselor-type rift boss, or a Torment Counselor champion, hit at a fraction of his damage. The roll now carries `RiftBossDamagePercent`.

## Fixed (P3)
4. **Javelin throws and skill weapon blows** are each their own hit for the per-hit rules (`OnPassiveMissileHit` `ownHit`). Two throws within a second had counted once for Hot Pursuit and Momentum.
5. **Skill-blow steals never heal a dead hero.** An Exploding Palm burst can fire after his death.
6. **Only physical skill blows take the item powers,** as arrows and plain swings do. Phantom Volley's magic and Poison Javelin's acid were tripled against demons.
7. **A minion body starts at overflow 100.** It took its type's Torment overflow on top of its record's bytes, so a revived champion or variant came back weaker.
8. **The Thunderous champion's death nova** carries its real damage, not the byte.
9. **A boss killed in an older arena** no longer clears a newer arena's free re-entry.
10. **The `JavelinThrowRange` comment** says 8.

## Open, unchanged (design notes)
- **Might and Berserk** still clamp per byte, so their bonus is lost on the max end of a monster with overflow. This was lost before too.
- **Free re-entry needs a second map of the kind.** The first map is spent on the first entry, so a hero with a single map who dies can return only by a portal still open into the arena.
- **Momentum's charge on a lingering spell** is now spent once a second per monster, while its +25% applies to every tick.
- **Doppelganger rolls its 10%** on every Earthquake pulse and every Thousand Reeds target.
- **The Paladin's weapon casts** (Blessed Hammer) take no item on-hit powers.

## Checked and fine
- **Units:** both skill strike paths are in 1/64ths, as `PlrHitMonst` is.
- **No double application** with `PlrHitMonst`.
- **Knockback** respects Unyielding, stone and the dead.
- **The overflow percent** is applied exactly once on every damage path: melee, specials, charges, projectiles, live recomputes, the rift signatures and the health bar.
- **Save and load** use exactly the two spare bytes, and no golden depends on them.
- **The arena** is saved on a death and restored wounded.
- **Javelin range:** nothing else assumes 6.
