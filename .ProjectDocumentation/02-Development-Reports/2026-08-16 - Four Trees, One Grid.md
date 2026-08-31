---
date: 2026-08-16
version: 1.7.15
area: MPQ drop-zone intake, units 2-5 - the class skill trees generalized to all four classes
---

# Four Trees, One Grid

Units 2 through 5 of the drop-zone plan, done together because they are one change: the Paladin's
tree machinery became everyone's, and the Barbarian, Sorceress and Rogue sheets filled it.

## The cut, second attempt

The self-measuring script abandoned last session was replaced by the two-step the plan prescribed:
a throwaway scan printed each sheet's emblem bands and the column spans inside them, and those
numbers went into a table-driven `tools/CutClassTree.ps1` modelled on the Paladin's.

Two things the scan had to solve. The **skill names are printed under every emblem** and are not
wanted, so only the tall emblem bands are cropped - titles and labels are both short, which
separates them by height alone. And on the Barbarian's first row **Bash and Leap physically
touch**: Leap's motion lines run into Bash's impact burst, so no gap threshold could split them.
The boundary was found by scanning for the emptiest column between them - the seam between two
emblems is where the fewest pixels are, even when it is not empty. One pair on the Rogue's second
row needed the same treatment. 90 icons, 30 per class.

## One enum, four classes

`oracool/paladin_tree` became `oracool/class_tree`. Every tree skill in the game - 119 of them - is
one value of a single enum, each row carrying the class it belongs to, so investment, persistence
and the whole page UI still work on one index type instead of four parallel ones. A class's block
is contiguous, and a skill's position within it is also its position in that class's icon strip,
so art and table cannot drift.

The Abilities window's three tree sheets are now numbered rather than named, because *which* tree
page 1 is depends on the class: the Paladin's Offensive Auras, the Barbarian's Combat Masteries,
the Sorceress's Lightning Spells, the Rogue's Passive & Magic. The titles come from the tree.

Investment kept its two-store rule and generalized with it: a skill with a spell slot stores points
in `_pSkillInvestment` keyed by SpellID (so they reach `GetSpellLevel` and every ladder for free),
everything else in `_pClassTreeInvestment`, now thirty entries indexed by position-within-class.
That re-indexing retired chunk tag 5 in favour of tag 6 — **but tag 5 is still read and migrated**
onto the new slots, because the Paladin's auras sat at positions 9-28, so a hero saved between the
two builds keeps the points it paid for. Golden hero hash re-baselined as documented change #8.

## What each class actually got

**Sorceress — the best fit, as predicted.** Fourteen of thirty act, because this engine already has
the spells: Fire Bolt, Fireball, Fire Wall, Inferno, Charged Bolt, Lightning, Chain Lightning,
Nova, Teleport and Telekinesis map one-to-one, and investing raises the engine's own spell level.
Two are deliberate adaptations, stated in their own rows: **Energy Shield** is Mana Shield (mana
takes the damage life would — the same idea), and **Hydra** is Guardian (a stationary
fire-breathing summon guarding a spot — also the same idea). **Blaze** rides Flame Wave, the
nearest rolling fire this engine has. **Warmth** is a real passive: mana returns on the per-tick
hook. The entire Cold page is inert — there is no cold damage channel and no chill, so ten skills
are listed, described and honest about it rather than invented.

**Barbarian — seven act.** Four masteries (Sword, Axe, Mace, and Pole Arm mapped onto the staff,
this engine's nearest pole arm) sharpen aim and blow *only while that weapon is actually held*,
which is what a mastery means and what the provider's condition hook is for. Iron Skin, Natural
Resistance and Increased Speed round it out — Increased Speed being the fourth consumer of the run
toggle's frame skip. Throwing and Spear Mastery are inert because neither weapon type exists here;
the warcries want a monster-facing pass and the combat skills want movement work.

**Rogue — three act, the weakest fit and last for that reason.** Penetrate sharpens aim, Critical
Strike raises damage (this engine has no critical roll, so the expected value is spent flat, and
its row says so), and Valkyrie rides Golem — a summoned warrior fighting beside you. The bow
skills all want missile work; the cold and poison ones have no channel at all.

## Verified

**408 tests, the usual two pre-existing failures.** The grid invariant test now walks all four
trees rather than only the Paladin's — every skill on exactly one page, no two sharing a cell,
119 accounted for. That is the generalization's own proof.

## What is left

The drop zone is empty: every file the user has put in the MPQ root is now consumed. The inert
rows are the honest backlog — the monster-facing pass (Conviction, Sanctuary, the warcries), the
cold-damage channel (the whole Sorceress Cold page, Holy Freeze, the Rogue's ice arrows), missile
work (the bow skills), and movement work (Leap, Whirlwind).
