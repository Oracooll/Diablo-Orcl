# Tooltip audit: five fixes — v1.12.177

2026-09-25

> i want you to make an artefact with all possible item tiers/types and make a legend for each of
> them which explains what rows of text its tooltip screen should contain.
> — Fix all.

The "Orcl Item Tooltips" page (https://claude.ai/artifact/FwFKLxRUmEe7ypNnNrScjT) was built by
reading every tooltip printer. Five places did not do what their own code intended; all five fixed.

1. **Signet, Guardian Keystone, Sealed Map showed only their name.** Their rows live in
   `PrintItemOil`, reached only through `PrintItemMisc`'s `isOil` gate, and their misc ids sit outside
   every range the gate tests. Added to the gate: they now print their description rows (the
   signet's stat point and lifetime count, the keystone's tier and clock, the map's encounter and
   reward) and "Right-click to use".
2. **Movement speed / faster cast rate could print twice.** The stand-alone rows (for a magic item's
   pool affixes, kept in the Orcl record) printed even when a prefix, suffix, vanilla unique power
   or the rerolled-magic record branch had already printed the stat. `alreadyPrinted(a, b)` checks
   those three sources first.
3. **The shop printed the identified layout for unidentified stock** (Wirt's Gamble bases): quality,
   tier and item-level rows over a base name. The shop hover now prints `PrintItemDur` for an
   unidentified item, as the backpack and stash always did.
4. **`TierNamePrefix` (Jagged / Cruel / Primeval) was dead code.** Tier words were taken out of names
   on purpose (they broke magic names — the comment in `ApplyBaseTier` records why); the function was
   left with no caller. Removed; the comment now says so.
5. **"Not Identified" in two colours.** Blue on weapons and armour, default white on rings and
   amulets. Blue everywhere.

The page was republished with the new rows and the findings marked fixed.

Debug build and tests clean.
