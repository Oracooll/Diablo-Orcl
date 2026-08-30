# The play-test list of fourteen (v1.9.108-1.9.121)

**Date:** 2026-08-30
**Versions:** 1.9.108-1.9.121
**Trigger:** user play-test list of fourteen items, plus one HUD experiment that ran alongside it.

Fourteen items, none of them a crash. Four were nerfs or rule changes, six were UI geometry, and
four were bindings and descriptions. Each is below with what was actually wrong rather than what it
looked like from the outside.

## 1. The skill-points button opens the Abilities window

A button that showed a number and did nothing when clicked. Routed to the same opener the burger
menu and the two HUD skill buttons use, so the three cannot drift apart in what they close on the
way in.

## 2. F1-F8 bind from the quick lists (v1.9.121)

The user's framing: *"we are making F1-F8 hotkeys assignable from the quicklists, not from the
abilities windows."*

Almost none of this was new machinery. The per-button hotkey arrays (`_pSplLHotKey` for the left
button, `_pSplHotKey` for the right), the one-key-one-skill sweep, and the bind/unbind toggle have
been in `HandleAbilityFKey` since 2026-08-18. What changed is where the hovered skill and the target
button come from.

While a quick list is open the **open list names the button**, so shift stops choosing it. That is
not a detail: shift previously meant "the left button", and holding it over the LMB list would have
written the RIGHT button's array - a silent wrong-slot binding, the worst outcome available here.

The picker reads its own hover out of its draw, the way the Abilities window does, because the cell
rects are laid out in the draw and a second copy of that walk is a second place for the two to
disagree. Attacks and auras carry no `SpellID` and so report nothing hovered, which makes them
unbindable with no special case at the call site.

Each cell now shows the F-key it sits on **for that list's button only**. The Abilities window puts
both buttons' keys in its two corners; a quick list binds one button, and showing the other's key
here would invite pressing it in the wrong window.

## 3. The gold XP bar (v1.9.119)

`Source/qol/xpbar.cpp` had been a set of no-op stubs since the HUD art pass retired the vanilla
bar - the entry points were left wired to their call sites rather than unpicked, on the argument
that bringing a bar back should be a one-file change. It was: nothing outside that file moved.

8px tall, gold, rounded ends, a rounded notch every tenth. The rect is **derived, not placed** -
top from the XP counter's own draw rect, bottom from the belt's first cell - so "dead in the
vertical middle between the exp counter and the top edge of the belt" survives either of them
moving, and both moved twice this week. Width is the belt run rather than the plate, because the
belt is what the request anchors to.

## 4. Aura rings stop growing with level

The rings were sized from the aura's radius, so every point spent made them larger. They are now
held at their smallest size regardless of rank.

## 5. Unique-monster drops nerfed (v1.9.117)

Two new options, both defaulted rather than hardcoded: `Unique Drop Chance Percent` (50) narrows
`CheckUnique`, and `Champion Extra Drop Chance` (25) gates a champion's second `SpawnItem`, which
was previously unconditional - the "two at a time" in the report.

`uper` was left alone, deliberately. It does double duty: it is the unique-roll window **and** the
high-quality marker, where `uper == 15` sets `CF_UPER15` and pushes `GetItemBLevel` up by four.
Lowering it to nerf drops would have quietly nerfed item quality across the game.

## 6. Socketed whites survive the auto-salvage - the rest of it

This was reported as already-asked, and it was. The protection had been written for Levski's
buttons; `TrySalvageOnPickup`, the automatic charm path, asked `IsSalvageable`/`SalvageTierOf`
itself and never saw it. Both now go through one `SalvageMatches(item, tier)`.

The recurring shape of this week: a fix correct in isolation and incomplete in context.

## 7. Gems in jewelry (v1.9.120)

The behaviour was already right. `SocketHostForItemType` has three cases, and Ring and Amulet fall
to the `default:` - which is `SocketHost::Armor`, so a gem in a ring has always given its armour
effect. Only the label lied: "In armor", on a line that also governs rings and amulets.

## 8. The quest window centred, with shadows

Vertically centred via a `QuestPanelOrigin()` the list rect is then derived from. Rows carry the new
`UiFlags::Shadowed`.

That flag came out of the same day's shadow work: vanilla's offset is `Displacement{-2, 2}` -
down and **left** by two - which was read back out of the DevilutionX 1.5.5 baseline in git history
rather than guessed. The first attempt used (+1,+1) and looked wrong for a reason nobody could name
until the real number turned up.

## 9. The mouse buttons remember their skills (v1.9.118)

Two invisible options, `lastReadiedSpellLeft`/`Right`, written by
`ScheduleAutoSaveForSkillChange` and applied in `CreatePlayer` after the class defaults. New game,
same two skills on the same two buttons.

## 10. The crafting book gets the runeword book's frame (v1.9.120)

It was a 340-wide box sized to its recipe count and pinned to the screen's top-left corner. It is
now 944x616, centred, top flush with the mini-map - and because the frame is **fixed rather than
grown from the list**, the list inside it can now outrun the panel, which is what makes scrolling
mean anything at all.

Wheel notches over the window are consumed whether or not they move the list, so the dungeon zoom
behind it never sees them. Clicks and hovers needed no new routing: the window is a
`LeftPanelContent`, and both `GetLeftPanelContentRect` and `CheckCursMove` already ask
`GetCraftingMenuRect()`, so the bigger rect absorbs the bigger window for free. The scroll offset
does have to come back out of the row hit test, or every click below the fold lands on the recipe
that used to be at that height. Added the red X, which the window never had.

## 11. Zeal capped at four strikes

`MaxZealStrikes = 4`, one strike per level, and each point past the cap buys **+1% to hit** instead
(`ZealToHitBonus`, added to `hper` in `PlrHitMonst` before the clamp). Three tests pinned the old
ladder and failed correctly; their tables were rewritten to `{0,2},{1,3},{2,4},{4,4},{6,4},{20,4}`.

## 12. Three-line spell rows

Name / mana / damage range, the last from `GetDamageAmtAtLevel` at the known level, or level 1 when
the spell is not yet learned. A `static_assert` holds `SpellRowHeight >= 3 * AbilitiesLineHeight`,
so a future row-height change cannot silently clip the third line.

## 13. The stash chest moved one tile SW

`{55,67}` to `{56,67}`. Worth recording: the test failure this produced was **not** a placement
failure. `Test/oracool_town_objects_test.cpp` pins the tile as a constant, so the fix was to move
the pin, not to chase the placement.

## 14. Row two names the item type

Row two said "MAGIC ITEM". It now says "MAGIC RING", "RARE BELT", "UNIQUE AMULET". Two helpers -
`GetItemTypeNoun(const Item&)` and `GetOracoolTierQualityWord(tier)` - and the row is
`"{quality} {noun}"`.

## The HUD experiment, alongside

Ran in parallel with the list and is worth its own note because it ended in a revert.

The plate art was made optional (`hudPlateArt`), which exposed the LMB/RMB wells and the belt with
no backing. The stats/skills-points frame was reused as a backing for both: native 64x64 behind the
two mouse wells, and scaled to 45x46 per belt cell with a `static_assert` holding the well opening
at exactly 28x28 so potions still fit. The plateless row is `2*64 + 6*45 = 398` wide.

v1.9.109 scaled the mouse-well backing to fit its opening exactly and was reverted the same day -
*"the new size look awful. rollback to what it was before 109"* - so the wells are native 64x64,
centred. The belt scaling stayed.

## Verification

575/577 across every version above. The two failures are the standing baseline pair,
`Drlg_l1.CreateL5Dungeon_diablo_3_844660068` and `Timedemo.WarriorLevel1to2`, unchanged.

Three of the fourteen are geometry and one is a badge, and none of those four is proven by a test.
They need a screenshot.
