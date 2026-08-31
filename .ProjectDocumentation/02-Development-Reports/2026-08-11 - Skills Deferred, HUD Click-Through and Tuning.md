---
title: 2026-08-11 - Skills Deferred, HUD Click-Through and Tuning
date: 2026-08-11
tags: [dev-report]
summary: Furious Charge and Paladin splash damage leave the settings list to become future acquirable skills; the inventory sort control becomes standard; the dead zone around the new HUD becomes clickable world again; menu entries and pixel nudges tuned.
---

# Skills Deferred, HUD Click-Through and Tuning

## Three options leave the INI

The user drew a line between *settings* and *progression*:

- **Furious Charge** and **Paladin splash damage** are not preferences - they are skills to be earned once the Skills system exists. Both options are removed; both gates now return `false` unconditionally, with the mechanics left fully intact behind them. The Paladin's free slot returns to vanilla **Item Repair**, "a skill he is gifted at birth". Re-enabling later means pointing the gate at whatever the Skills system uses to record what the player has learned - a one-line change in each.
- **Inventory sort button** is a standard V1 feature, not a toggle. Always present in single-player.

Their tests were rewritten rather than deleted: they now assert the gates are closed, and the dash/cooldown state-machine coverage is retained untouched, since that code is what a learned skill will reuse verbatim. All six affected tests pass.

## The HUD's dead zone is world again

The click handler still gated on the *old* 640x128 main panel rect, so a wide band across the bottom of the screen silently swallowed clicks even though the new HUD is a 356px plate. Clicks there now walk the player like any other patch of ground. Only three regions still absorb clicks as UI: the plate itself, the XP counter's strip, and the chat panel while it is open.

## Menu and tuning

- Menu popup: **Chat** and **Friendly Fire** removed (chat keeps its Enter keybind; friendly fire is a multiplayer concept and this is a single-player HUD - note this leaves it with no UI at all, deliberately). **Skill Book** added as a placeholder, showing a "not available yet" message, so the entry's slot is reserved before the feature exists. Ten entries.
- XP counter moved 14px lower.
- Belt item sprites nudged 4px right; the RMB spell icon 2px right and 3px down. Both sprites carry asymmetric internal padding, so geometric centring alone reads off-centre - these are eye-tuned against the art, and documented as such so they are not mistaken for derived values.

## Town Portal scrolls and staves stop spawning (v1.0.70)

The user confirmed the open question: they should stop generating too.

**Scrolls** are filtered in `GetItemIndexForDroppableItem` - the single chokepoint every generation path funnels through (loot drops, all four vendors, unique base lookup, `RndTypeItems`/`RndUItem`/`RndAllItems`). The new line sits directly beneath, and mirrors, the existing single-player Resurrect/HealOther exclusion.

**Staves** roll their spell separately in `GetStaffSpell`, so they get the same predicate-based skip already used for books - by predicate rather than by jumping the enum index, so it does not depend on enum adjacency.

**Deliberately not used: `IsItemAvailable`.** That would have been the tidier-looking single switch, but it also gates `RemoveInvalidItem` (loadsave.cpp), `pack.cpp`, and the network item validators - so flipping it would have *deleted* Town Portal scrolls from any character already carrying them, and rejected them over the wire. The filter is generation-only by design; items already in a save stay valid and usable.

Item and store test suites pass (59 and 13 tests).

## Crash from the click-through change (fixed, v1.0.72)

The user crashed by clicking *near* the XP counter - in the newly-freed band around the plate - and correctly guessed the click-through work was responsible.

`LeftMouseCmd` opened with `assert(!GetMainPanel().contains(MousePosition))`. That invariant held for as long as the old 640x128 panel swallowed every click inside its rect, so nothing could reach world handling from there. Opening that band up is precisely what the user asked for, which made the assert false the first time anyone clicked in it - and in a Debug build a failed assert aborts.

Removed rather than relaxed: the caller already decides what counts as HUD, and `CheckCursMove` computes `cursPosition` for panel-area coordinates too (it only clamps `sy` while *scrolling*), so the target tile is valid. Checked for sibling asserts elsewhere - there are none.

Worth noting the reported symptom was a font-loading message, which does not match an assertion dialog; the font assets are all present and other white text renders fine. The assert is a definite, reproducible bug on that exact action, so it is fixed - but if a font error recurs, the exact wording would help, since that would be a second, separate problem.

## Verification

Debug build clean, all touched tests pass, `ORACOOL_VERSION` 1.0.70. Needs a play-test pass on: clicking the area either side of the plate (should walk), the Paladin's slot showing Item Repair again, the sort button appearing without an INI entry, the three nudges, and Adria's stock no longer listing Town Portal scrolls.

## Related

- [[2026-08-11 - HUD Follow-ups and Town Portal as Built-In Ability]]
