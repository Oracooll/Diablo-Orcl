# Grouped hero sheet, Advanced Stats, and a real cold resistance

2026-09-26 — v1.12.194

## Why

The user asked to regroup the ~50-row hero sheet and pointed at Diablo II Resurrected's Character + Advanced Stats pair. They approved a mock drawn on their 340x720 canvas, then answered these questions:
- **Advanced Stats placement:** "right panel slot, overlap inventory/abilities screen if open".
- **Bonuses the hero lacks:** hidden.
- **Rollback:** the new sheet sits behind an option.
- **Gold:** not on the sheet.
- **Cold:** "cold is part of orcl mod", and the cold resistance should be "as real as it is in Diablo 2". That covers four things:
  - cold-dealing monsters (ice variants, cold missile casters, rift and endgame bosses);
  - a D2 chill on the hero;
  - every D2 source of cold resistance (affixes, all-resist, the Resist Cold aura and Salvation, Sapphire and Thul);
  - one build for everything.

## The sheet (option "Grouped Hero Sheet", `heroSheetGrouped`, default on)

- `panels/charpanel.cpp`: the grouped layout.
  - **Header:** name; title, level and class; an XP bar with "X of Y" and "N to level L".
  - **Attribute boxes:** current value in the box, base in a strip under it, the + beside it.
  - **Points and Reset:** a stat and skill points box, then RESET.
  - **Right column:** both mouse-button skills with damage, armour class, to hit, Life, Mana (Rage or Essence) with bars, resist magic / fire / lightning / **cold**, the difficulty penalty and cap.
  - **Aura** box, and an ADVANCED STATS button.
- **Widget rects** come from the active layout: `PlaceWidgets`/`EnsureLayout` branch on the option. Option off draws the old list exactly.
- `oracool/advanced_stats.{h,cpp}`: the new right-slot window.
  - **Rows:** grouped Offense / Defense / Recovery / Other. Only non-zero bonuses are listed, blue for bonuses and grey for facts.
  - **Scrolling:** it scrolls when the list is longer than the window.
  - **Right slot:** it puts away the inventory or Abilities and restores it on close. Opening either of those closes it.
  - **Checklist:** red X, Escape, Space (`CloseAllWindows`), click-through blocked, hover suppressed, corner HUD hidden.
- The sheet and window were written by a sub-agent against a written spec. I built, rendered and fixed them.

## Cold resistance

- **Item field:** `Item::_iPLCR`. The item format goes to 14, and the three item-file loaders still read 13, loading those items with no cold (`AcceptItemFormat`). The save record size grew by 4.
- **Powers:** `IPL_COLDRES`, `IPL_COLDRES_CURSE`. Printed as "Resist Cold". All-resistances includes cold.
- **Pool rows:** cold resist bands on armour and jewellery at the fire rows' levels and prices. **Not weapons**, so weapon seeds keep rebuilding their old rolls.
- **Hero:** `_pColdResist`, on the same curve, penalty and cap as the other three. The Barbarian's per-level resists, Rage Cooldown and Zero Resistance treat it like the others.
- **Sources:**
  - Resist Cold aura (now cold, not magic), Salvation, and every hand-written "all resistances" in the class tree, charms, shards, runewords, warcries and RfA-12 actives.
  - Sapphire's shield (+20, Perfect 40), Diamond's shield all-resist, Thul (armour 30, shield 35), Um.
- **Cold hits:** a cold missile on a hero uses cold resistance, not magic.
- **Monsters:**
  - New `MonsterVariant::Glacial`: a third of its blow is cold, and its missiles are cold. It is in every roster but the Cathedral's.
  - The Snow Witch's blood star is cold.
  - A rift guardian or endgame boss deals a quarter of its melee as cold.
- **Chill** (`oracool::ChillPlayer`): 3 seconds, shortened by cold resistance's percentage. The walk runs at half speed through the movement slow (the sheet's Move speed shows it). Attacks, casts, blocks and hit recovery lose every other tick. It is cleared on level entry.

## Tests

- **New:**
  - `OracoolColdResistance.*`: the affix and all-resist reach the item and totals; the chill length and half-speed attacks.
  - `OracoolPreview.DISABLED_HeroSheet`, which renders the sheet.
  - `PackTest.DISABLED_DumpChangedGoldenRows`, which prints changed golden rows.
- **Updated:**
  - The aura test now expects Resist Cold to give cold.
  - The pack golden corpus: 18 Diablo and 13 Hellfire rows re-rolled, because the pool changed.
  - Writehero's item-derived totals, plus a cold pin.
- The v1.12.194 Debug build is clean. The full suite passes, 849 of 849, and the audit suite passes three shuffled runs. The preview render found two labels that overflowed ("level-up points" → "stat points", and the AC breakdown), both fixed.
- Not seen in game yet.
