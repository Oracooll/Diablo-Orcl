---
date: 2026-08-16
version: 1.7.39
tags: [abilities-window, class-tree, paladin, ui, bugfix]
---

# Seven Sheets, One of Them Imaginary

> "these are screenshots of all sheets of the abilities window. for my paladin... they seem too many
> and full of issues. analize them and fix everything you can identify as an issue."

Seven screenshots. One of them was of a sheet that does not exist.

## The blank one

`AbilitySheet` has six members and ends `LAST = ClassTree2`. `AbilitySheetCount` was the literal `7`.

Nothing crashed and nothing indexed out of bounds — `ScrollOffset` was sized from the same wrong
number — so the phantom simply fell through every switch that consumed it. `GetSheetTitle` returned
an empty string, `GetRowCount` returned 0, and `IsSheetAvailable` had no case for it and so said yes.
The arrows dutifully cycled you onto a blank page with no name and nothing on it.

It is derived now:

```cpp
constexpr size_t AbilitySheetCount = static_cast<size_t>(AbilitySheet::LAST) + 1;
```

## The one that mattered

Five rows of the Paladin's **Combat Skills** tree are not implementations. They are second faces on
skills `oracool/paladin_skills.h` owns — Smite is Shield Bash, and Zeal, Charge, Blessed Hammer and
Fist of the Heavens are themselves. They share that module's spell slot, its investment store and its
mana price.

They did not share its **availability**.

`IsClassTreeSkillUnlocked` stopped at the row's tier level. The skill itself gates on its own
`minLevel` and, for two of the seven, on a shield actually being held. They disagreed four times out
of five:

| Tree row | Tier says | The skill says |
|---|---|---|
| Smite (Shield Bash) | level 1 | level 8 **and a shield** |
| Charge | level 6 | level 12 |
| Blessed Hammer | level 18 | level 16 |
| Fist of the Heavens | level 30 | level 24 |

The visible half is Smite, and it is visible in the user's own screenshot: the Combat Skills page
lights it green at level 1 with nothing in hand, lets it be clicked onto a mouse button — and the
button then does nothing, because `CanUsePaladinSkill` asks the other question. One arrow away, the
Skills sheet greys the same skill out correctly.

Fixed by giving the borrowed set a single list, `BorrowedPaladinSkill()`, which both the slot lookup
and the unlock test now read. A row that borrows a skill answers that skill's requirements — all of
them, shield included.

That left the tree quietly *stricter* than the Skills sheet for the two rows whose tier sits above
the skill's own level. Both of those numbers were placeholders, and said so in their own file's
header ("none of them is load-bearing until the skill has mechanics to gate"), so they moved up to
their tiers: Blessed Hammer 16 → 18, Fist of the Heavens 24 → 30. Every sheet now tells one story.

Pinned by `TreeAndSkillsSheetAgreeOnBorrowedPaladinSkills`, which walks all five skills across
levels 1–40 both bare-handed and holding a shield, and was verified to go red against the old code.

## The window itself

**It had no background.** Five side panels — stash, inventory, character, quests, waypoints — moved
to `ui/stash_background.png` earlier today. The Abilities window was the sixth and was missed, so it
was still a half-transparent wash and the town showed straight through it in every screenshot. It
now takes the same art, with the procedural fill kept as the fallback.

**The title band held three things and had room for one.** The sheet name (centred), the unspent
points (right-aligned) and the right-hand arrow were all drawn into one 292px rect. On any long name
they overlapped — and "Points: 3" rendered over the arrow as `Points▶3`, the colon swallowed. The
band belongs to the title alone now, at the shared `PanelTitleTop` so it lines up with the other five
windows. The arrows and the points count moved to a nav row below it, which is the stash's page row
in all but name.

**The list ran off the bottom of the frame.** It was sized "panel height less a margin", reaching
y=696, straight across the background's bottom ornament. It ends at
`oracool::SidePanelContentBottom` now, like everything else. That is the same mistake the character
sheet and quest log made against the health orb, which is why the constant exists.

**Tree pages advertised a tier they did not have.** `TotalListHeight` returned
`ClassTreeTierCount * pitch` unconditionally, so every six-tier page — which is every class but the
Monk — grew a scrollbar to reach an empty seventh row. It measures its own deepest tier now, and the
row gap tightened 12 → 6 so a six-tier page still fits the (now shorter) list unscrolled. A
`static_assert` says so rather than a comment.

**Every tree page was speckled with zeroes.** The counter under each icon was drawn unconditionally,
so a page showed a `0` under every skill on it — and most of a tree is locked at any given level, so
most of those sat under grey plates that already said "not yours yet". It speaks only when there is
something to say: gold `+ N` when you can invest, white `N` when you already have, nothing otherwise.

**The Skills sheet's invest controls were stranded.** That sheet is icon-only by request, so between
its 38px icon and the panel's right edge — where the `+` and the sunk-points count were anchored —
lay 200px of nothing. In the screenshot they read as belonging to no row at all. They follow the icon
now, into the column the text used to occupy. Draw and hit-test share one helper, so they moved
together.

**The Spells sheet opened with the only three entries that do nothing.** Doom Serpents, Blood Ritual
and Invisibility are the original developers' name-only stubs: `MissileID::Null` in both slots, no
mana, no book price — and `minInt` 0. The list sorts on `minInt`, so that placeholder zero put all
three ahead of every real spell. They sort last now, until they are given substance.

Also removed: `RowHeightAt`, which rebuilt the entire Skills sheet to return a constant and was
called once per row from `TotalListHeight`.

## Left alone, deliberately

**The horizontal margins.** The painted frame's interior is about x=41..298 and the rows run 24..323,
so icons and the scrollbar overlap the pilasters. But all six side panels use a 24px margin and the
user has been living with the other five, so this is one coordinated decision about the whole theme,
not something to change in one window and make it the odd one out.

**The `0` glyph rendering as `⌀`.** That is the font.

## Still open — the user's call

The Skills sheet lists all seven Paladin skills; five of them are also on the Combat Skills tree
page, sharing the same investment. That is the real substance of "too many". It cannot be resolved by
deleting the duplicates, because **Hammer of Faith and Blessed Shield have no tree row at all** —
they would simply vanish. Giving them the free slots at tier 2 and tier 4 would fix it, but it moves
their level gates onto the tier ladder, which is a balance decision rather than a defect.

## Shipped

`ORACOOL_VERSION` 1.7.39. 423 tests, the two long-standing failures unchanged
(`Drlg_l1.CreateL5Dungeon_diablo_3_844660068`, `Timedemo.WarriorLevel1to2`).

## Worth noting

Two of today's bugs are the same bug: a number restated instead of derived. The stash's auto-place
scan restated the page height as `10`; the Abilities window restated its sheet count as `7`. Both
were correct when written and both were left behind by a change elsewhere, and in both cases nothing
failed loudly — the stash quietly used fewer rows, the window quietly gained a blank page.
