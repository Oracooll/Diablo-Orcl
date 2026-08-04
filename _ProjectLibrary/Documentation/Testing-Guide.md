# Oracool Edition Testing Guide

## Test states

For every toggleable feature, test both enabled and disabled states after restarting the game. Unless noted otherwise, also confirm multiplayer retains vanilla behavior.

## Final regression record

- 2026-08-02: the complete Debug build succeeded.
- 2026-08-02: all 213 automated tests passed.
- 2026-08-02: after adding Gold Stacks Buff and its save-boundary test, all 214 automated tests passed.
- 2026-08-02: after correcting the validation clamp and adding lossless-withdrawal coverage, all 215 automated tests passed.
- 2026-08-02: the user completed broad practical testing of the corrected Gold Stacks Buff and reported no remaining problems.
- 2026-08-02: after the initial combined Adria/Pepin-stock implementation at Griswold, all 216 automated tests passed.
- 2026-08-02: after replacing that combined section with four fixed, infinite Pepin potions before Adria's stock, all 216 automated tests passed.
- Every catalogued 1.5.4 feature was matched to its 1.5.5 implementation during the final source audit.
- 2026-08-03: after adding Respawn In Town (OE-014) and its `ShouldDropGoldOnDeath` test coverage, all 218 automated tests passed. Release branding and version display (OE-015) required no new automated coverage.
- 2026-08-03: the user tested Respawn In Town (OE-014) and the single-line version display (OE-015) in-game and accepted both.
- 2026-08-03: OE-015 revised at the user's request into a two-row main-menu label (`DevilutionX 1.5.5` / `Oracool Edition v0.1.0`, "Diablo" omitted from row 2 at the user's request). Tested and accepted by the user.
- 2026-08-04: after implementing Stackable Consumables (OE-012) across seven phases (data model, save round-trip fix, pickup/purchase merge, quantity overlay, decrement-on-consume plus a Succubus steal-potion fix, manual drag/drop merge and split, stack-aware store pricing), all 243 automated tests passed.
- 2026-08-04: the user tested Stackable Consumables (OE-012) and found a critical bug: buying certain consumables while owning none of that item spent gold but placed nothing. Root-caused to `isStackableConsumable()` not checking `isEmpty()` first, letting stale leftover data in a cleared slot look like a valid merge target. Fixed, with a regression test reproducing the exact scenario plus two `Item`-level unit tests; all 247 automated tests passed.
- 2026-08-04: after implementing Belt Mod (OE-013) — belt slots hold their own capped stock and auto-refill from inventory once emptied, with the legacy `autoRefillBelt` redirect bypassed and the hotkey-number overlay suppressed while active — all 252 automated tests passed. Implemented and automated-verified overnight while the user was asleep, per their explicit "keep working" instruction; in-game acceptance is still pending (checklist below).
- 2026-08-04: after implementing the shared extended item-data and save foundation (OE-016, roadmap section 7) — new `Item` tier/affix fields and a versioned `"heroitemsext"`/stash-equivalent save extension, with no player-visible feature yet — all 259 automated tests passed, including a full character-save round trip of a fully-populated six-affix tiered item and a stash round trip. Also implemented and automated-verified overnight per the user's "keep working" instruction; this phase has nothing to manually test yet (see OE-016 in Gameplay-Changes.md), acceptance explicitly deferred to Rare Items (OE-008).
- 2026-08-04: after implementing Rare Items (OE-008) and generalizing the floating item-statistics popup — procedurally generated 1-4 affix items drawn from vanilla's own affix tables, an adjustable drop-chance option, and a z-order fix so the popup (Rare and vanilla Unique alike) renders above every other panel — a regression was found and fixed before the suite went green: the new drop-chance roll was consuming an extra random number even when *reconstructing* a previously-generated item from its saved seed (`UnPackItem`/`RecreateItem`), which would have silently changed existing items' rolled stats on every load. Fixed by adding an `allowRareRoll` parameter to `SetupAllItems`, false only for the reconstruction call site. All 266 automated tests passed afterward. Implemented and automated-verified per the user's continued "keep working" authorization, this time with the user awake to answer the open design questions (affix distribution, duplicate/Good-Evil rules, identification state, rarity mechanism) before implementation began.
- 2026-08-04: the user caught a real bug in Rare Items right after it shipped: identification was hardcoded to always-on for Rare items, but the user's actual intent (stated as a general project rule) is that *no* item type should force its own identification state — every item, tiered or not, must follow the single shared `Auto Identify Drops` toggle uniformly. Fixed by removing the hardcoded override; Rare items now correctly go unidentified when that option is off.
- 2026-08-04: after implementing Buffed Uniques (OE-009) — sharing almost the entire affix/popup/pricing/color engine already built for Rare Items, generalized via a new shared `GetTieredItemAffixes` function, plus a new independent `Buffed Unique Item Drop Chance` option checked before Rare in the quality-roll fork — all 271 automated tests passed. The user also stated a new standing project rule mid-session: every new feature should default to being INI-toggle-able unless that's genuinely impractical (saved to memory as `feedback_toggleable_features`); Buffed Uniques already followed this by design.
- 2026-08-04: the user tested Belt Mod, Rare Items, and Buffed Uniques together in-game and confirmed "all seems OK," clearing the standing test-before-next-feature gate for Primal Items.
- 2026-08-04: before starting Primal Items (OE-010), discovered the roadmap's literal "cyan" color requirement wasn't achievable without real risk — `UiFlags` (the bitflag type selecting item text color) is a fully-packed 32-bit enum with zero free bits, and no cyan font recolor (`.trn`) asset exists in the game's data. Asked the user, who chose Orange instead (matching Diablo 3's own Primal Ancient convention) over investing in a 64-bit `UiFlags` widening plus an unverifiable new art asset.
- 2026-08-04: after implementing Primal Items (OE-010) — always exactly 3+3 affixes, every one forced to its maximum roll via a new `ForcePerfectAffixRoll` flag in the shared `RndPL` primitive, full durability, a new `Primal Item Drop Chance` option checked before Buffed Unique and Rare — all 277 automated tests passed. `ItemPrefixes[]`/`ItemSuffixes[]` needed `DVL_API_FOR_TEST` added (same class of link issue hit earlier for `dPlayer`/`gSaveNumber`) so tests could cross-check every Primal affix against the vanilla table's `maxVal`.
- 2026-08-04: the user tested Rare, Buffed Unique, and Primal item tiers extensively in-game using level-60 developer test characters and the debug console's `drop rare`/`drop unique`/`drop primal` commands. Rare items worked as intended with no issues. Three bugs were found: (1) Buffed Unique items showed "rare item" in the description panel instead of "unique item"; (2) Primal items showed "rare item" instead of "primal item" in the same panel; (3) some Primal items dropped with fewer than 6 total affixes (though the affixes present were confirmed correctly max-rolled), contradicting the guaranteed-3+3 design.
- 2026-08-04: fixed all three bugs. (1)/(2): added a new `GetOracoolTierPanelLabel` helper returning the correct per-tier wording, wired into `PrintItemDetails` in place of a hardcoded ternary that always defaulted to Rare's wording for any tiered item. (3): `GetTieredItemAffixes` now forces `ignoreLevelLimits = true` whenever `perfectRoll = true` (alongside the existing `onlygood = true` forcing), since a narrow/low level window was starving the forced-minimum affix candidate pool for Primal specifically. Added a `GetOracoolTierPanelLabel.ReturnsDistinctWordingPerTier` test and a `GetPrimalItemAffixes_AlwaysProducesExactlyThreePrefixesAndThreeSuffixesInNarrowLevelWindow` test (200 trials at minlvl=maxlvl=1, the narrowest possible window) reproducing bug 3 and confirming the fix. All 279 automated tests passed. Version bumped to 0.1.5 for the user's retest.
- 2026-08-04: while retesting the v0.1.5 corrections, the user found a further bug and asked for the affix-count limits on Rare, Buffed Unique, and Primal to be re-checked against the roadmap: a Buffed Unique ring dropped with only 1 affix total, well under the guaranteed 2 prefixes + 2 suffixes (4 total) minimum. Root cause was the same mechanism as the v0.1.5 Primal fix (a narrow level window starving the forced-minimum affix candidate pool) but that fix only forced `ignoreLevelLimits` for `perfectRoll` items (Primal) — Rare and Buffed Unique remained exposed, and jewelry (rings/amulets, using `AffixItemType::Misc`) has a much smaller affix pool than weapons/armor, making the starvation far more likely to actually surface there.
- 2026-08-04: fixed by making the forced-minimum affix loop in `GetTieredItemAffixes` always ignore the level window, for every tier unconditionally — the minimum count is a stated design guarantee, not a best-effort, per every tier's roadmap wording ("Always at least..."). The optional bonus-affix rolls above the minimum still respect the level window, since those are explicitly probabilistic extras. Added `GetRareItemAffixes_AlwaysProducesAtLeastOnePrefixAndSuffixForJewelryInNarrowLevelWindow` and `GetBuffedUniqueItemAffixes_AlwaysProducesAtLeastTwoPrefixesAndTwoSuffixesForJewelryInNarrowLevelWindow` tests (200 trials each, jewelry item type, minlvl=maxlvl=1) reproducing the exact scenario and confirming the fix. All 281 automated tests passed. Version bumped to 0.1.6 for the user's retest.
- 2026-08-04: the user tested and approved the v0.1.6 affix-count guarantee fix, clearing the way to discuss further ideas before Torment Difficulty (OE-011). First idea: Tabbed Inventory - 9 extra backpack pages, switchable via tab buttons above the inventory grid, kept inside the existing gap between the ring row and the grid since the surrounding artwork can't be moved (it's original, non-editable game art). Before implementing, the user asked whether such a large inventory (10 pages x 40 slots = 400 total) is actually handleable by the engine, since InvGrid's `int8_t` index encoding and PlayerPack's `uint8_t _pNumInv` both have hard ceilings well below 400 total items, and PlayerPack/ItemNetPack are validated by a strict `sizeof()` check that breaks every save if resized. The user also required that a character stay exactly one file (unlike the Stash, which is deliberately a separate `stash.sv`).
- 2026-08-04: designed and implemented Tabbed Inventory (OE-017) around those constraints: 9 independent 40-cell pages (each individually safe under the `int8_t` ceiling, since 40 never comes close to overflowing ±127) stored in a new sub-file *inside* the same character archive (`"heroinvtabs"`, mirroring the `"heroitemsext"` precedent) rather than growing `PlayerPack`. Tab 1 stays the untouched original `InvList`/`InvGrid`/`_pNumInv`. All 287 automated tests passed, including a caught-and-fixed real bug: `LoadOracoolItemExtensions` originally ran before `LoadInventoryTabs`, so a tiered item stored in an extra tab silently lost its tier data on load (the extension-matching scan ran before the tab's items even existed in memory) - an automated round-trip test caught this before it ever reached a real save file. Version bumped to 0.1.7 for the user's first test of this feature.
- 2026-08-04: the user tested Tabbed Inventory and reported Primal items specifically could not be placed into an extra tab, while regular Griswold items (white/blue/vanilla-Unique) worked fine. Extensive automated reproduction through the real click-handling entry point (plain item, fully-generated Primal item, Primal item with a realistic multi-cell weapon icon) could not reproduce a placement failure - all three succeeded. The user's further testing found a real, confirmed bug in the same area: hovering an item stored in an extra tab showed no name or stats anywhere, since `CheckInvHLight()` only ever read the real InvGrid/InvList (tab 1). Fixed to read through the same active-tab accessors used elsewhere - very likely the actual explanation, since Primal items are the tier most worth double-checking after a move, and hover showing nothing gave no way to confirm a placement had actually succeeded. New `TabbedInventory_HoverInExtraTabShowsItemInfo` test added; all 290 automated tests passed. Version bumped to 0.1.8 for the user to retest with hover now working.
- 2026-08-04: the user found two more Tabbed Inventory bugs after the v0.1.8 hover fix: (1) buying from a vendor while tab 1 was full still failed with "no room", ignoring the 9 empty extra tabs entirely; (2) hovering an item in an extra tab showed the floating popup for tiered items but the classic under-belt panel still showed nothing for magic/basic items and consumables. Root cause 1: `StoreAutoPlace` (every vendor purchase path) never knew extra tabs existed. Root cause 2: `DrawInfoBox` clears the panel whenever `pcursinvitem == -1`, and that's exactly what an extra-tab hover deliberately leaves it at - the v0.1.8 fix populated the text but had no way to tell `DrawInfoBox` "don't clear this one." Fixed with a new `AutoPlaceItemInExtraTabs` purchase fallback and a new `ActiveTabItemHovered` flag that `DrawInfoBox` now also checks. All 292 automated tests passed. Version bumped to 0.1.9.
- 2026-08-04: the user found three more Tabbed Inventory issues after v0.1.9: (1) an item in an extra tab could reappear after being dropped and the game saved twice - a real, confirmed bug: `SaveInventoryTabs` skipped writing the sub-file when all tabs were empty, but MPQ archives are updated in place, so a *stale* entry from an earlier save (when a tab had something in it) stayed behind and got read back on the next load. Fixed by explicitly deleting the archive entry instead of just skipping the write, verified with a test that reproduces the exact save-remove-save-reload sequence and was confirmed to fail without the fix. (2) Ground pickup, auto-pickup, and Stash withdrawal still only checked tab 1, unlike vendor purchases. Fixed by moving the extra-tab fallback into `AutoPlaceItemInInventory` itself, the single function all of those already funnel through - gold and quest items still correctly excluded. (3) Griswold's and the Witch's Sell Items lists didn't include anything stored in an extra tab. Fixed by extending both sell-list scans and adding a new `RemoveExtraTabItem` removal path. All 296 automated tests passed. Version bumped to 0.1.10.
- 2026-08-04: the user retested v0.1.10 and reported the "sent an invalid packet" drop bug again with a much sharper repro: ~50% of items generated via the debug console's `drop {name}` command vanished specifically on being dropped to the ground, despite behaving normally otherwise, and it wasn't tied to any particular tier. Root cause: `DebugSpawnItem` stamps a uniformly random 1-63 "level" into the generated item with no regard for whether `IsPItemValid`/`IsDungeonItemValid` (which run even in single-player, over loopback) would ever accept it - true for only about half that range. Fixed by having the debug command predict and retry past any level that wouldn't survive a real drop (`WouldSurviveNetworkValidation`). Not tier-specific and never affected real monster/dungeon drops, whose levels always come from a legitimate source. Also fixed in the same round: Belt Mod's refill only ever scanned the real backpack, never the 9 Tabbed Inventory extra tabs. All 6 directly-touched regression suites (44 items, 40 inv, 6 stores, 8 loadsave, 4 player, 58 pack — 160 tests) pass at 100%. Version bumped to 0.1.11 for the user's retest.
- Also investigated, not yet resolved: re-entering Griswold's "Buy Basic Items" after buying out the stock backs the player out of the store - confirmed pre-existing vanilla DevilutionX behavior (`StoreESC()` fires whenever `smithitem[]` is empty), not something Tabbed Inventory touches; awaiting a decision on whether to change it.
- Process note (2026-08-04): after shipping v0.1.11, the user reported "none of the bugs seem repaired." The fixes were correct and already test-verified, but `devilutionx.exe` itself (the actual game the user launches, confirmed to live in `build/x64-Debug/` alongside the live `diablo.ini`/`Saved_Games`) had never been relinked - only the test binaries had been rebuilt. Separately, editing the `ORACOOL_VERSION` text file alone doesn't trigger CMake to regenerate `Source/config.h` (which bakes in the version string shown on the main menu) unless `cmake .` is explicitly re-run first; `ninja` alone never notices that file changed. Both are now fixed for this and future rounds: `cmake .` then `ninja devilutionx` before ever handing a build back for testing.
- 2026-08-04: the user reported a further bug after v0.1.11 - most uniques purchased from Griswold's Unique Items shop (OE-004) also vanished with "sent an invalid packet" when dropped, a different root cause from the debug-drop bug above. `CreateUniqueVendorItem` marks its stock with both `CF_UNIQUE` and `CF_SMITH` in `_iCreateInfo` (so `stores.cpp` can recognize it as Smith-sourced for resale), but `IsCreationFlagComboValid` rejects any town-flagged item carrying more than one flag - a combination vanilla never produces and never anticipated. This ran first, unconditionally, for every dropped item, so it rejected 100% of Unique Shop purchases (not partial like the debug-drop bug). Fixed by excluding `CF_UNIQUE` before applying the "one towner flag" rule; confirmed the real `UniqueItems` table's `UIMinLvl` (max 27) is comfortably under the unrelated 30-level town-item ceiling the very next check applies, so no further fix was needed there. All 163 tests across the six directly-touched suites pass. Version bumped to 0.1.12.
- 2026-08-04: the user confirmed the Tabbed Inventory feature (OE-017) itself is now fully working across every prior bug round, and reported a new, separate issue: a batch of Rare/Buffed Unique/Primal items left on the ground, then saved and reloaded, sometimes reverted to plain magic items (name/stats/durability intact, tier color and affixes gone). Root cause: the shared item-extension save mechanism (OE-016, `"heroitemsext"`) only ever scanned player-carried containers - it never scanned the dungeon floor's own `Items`/`ActiveItems` arrays, so a ground item's tier/affix data was never written or restored at all, unconditionally (the "sometimes some" in the report most likely just reflects incomplete manual checking, not a partial effect). Fixed by adding matching ground-item collect/apply functions to the same pipeline, applied from `LoadGame` right after `LoadDroppedItems` repopulates the level's items (too late for `LoadOracoolItemExtensions`'s own, earlier call site to help directly). Known gap: only covers the floor the player is standing on at save time, not other levels' dropped items (their own separate per-level save file has no equivalent hook yet) - flagged, not fixed, pending whether it matters in practice. All 165 tests across the six directly-touched suites pass. Version bumped to 0.1.13.
- Environment note (2026-08-04, unrelated to any Oracool feature): a full `ninja` (all targets) currently fails to compile `missiles_test.cpp`'s pre-existing, already-committed-uncommitted vanilla-baseline test code (`TestArrowRotatesUniformly`/`TestAnimatedMissileRotatesUniformly`, present since the DevilutionX 1.5.5 baseline) with a gmock template-matcher error (`UnorderedElementsAre`/`AllOf(Gt, Lt)` over an `unordered_map<int, unsigned int>`), most likely from a Visual Studio/MSVC toolchain update since this was last verified. This is not caused by, and does not affect, any of this session's changes - confirmed by building and passing the six specific regression targets these fixes touch directly - but it currently blocks a full `ninja && ctest` run and needs its own look before the next full-suite baseline can be recorded.
- Deferred manual validation: high-level boundary testing of the Premium limit controls (requires a character above the user's currently available level 30 character); user retest of the affix-count guarantee fix above, across all three tiers and multiple item classes (weapon, armor, jewelry); in-game acceptance of Tabbed Inventory (OE-017, checklist below), specifically re-confirming Primal item placement, vendor purchases with tab 1 full, the under-belt panel for magic/basic/consumable items in an extra tab, the stale-save-entry fix, ground/auto-pickup and Stash withdrawal spilling into extra tabs, and selling an extra-tab item to Griswold/the Witch; the new debug-drop network-safety fix and Belt Mod extra-tab refill fix (both checklists above).
- 2026-08-04: the user asked for a full "rethink and polish" of every Oracool-specific modification made so far, explicitly accepting a save-compatibility break, plus a review of artificially narrow integer types and the font-color ceiling, with an eye toward future Diablo 2/3-style features (nothing from that list built now). Implemented as OE-018 (v0.2.0 "Foundations Pass") - see Gameplay-Changes.md for the full breakdown. Headline change: the OE-016 item-tier sidecar save mechanism (root cause of the entire v0.1.11-v0.1.13 bug streak) is gone, folded directly into `SaveItem`/`LoadItemData` so every container - including, as a side effect, ground items on every level, closing the last v0.1.13 gap - shares one save path. This is **not save-compatible**; a pre-v0.2.0 save now fails to load with a clear message instead of a crash or silent corruption. Also in this pass: the Gold Stacks Buff cap raised to 100,000,000 (the old 65,535 figure was more conservative than the actual save path required); `UiFlags` widened to 64 bits (removes the "0 free bits" ceiling, though a genuinely new font color turned out to need a palette edit that's out of scope, not just a free bit - Primal stays orange); the Griswold empty-stock bounce-out bug fixed; a real item-duplication bug in the two-handed-weapon unequip path fixed (`RemoveMatchingInventoryOrExtraTabItem`); `MergeStackableItemIntoInventory` now also merges into a matching stack sitting in an extra tab; Griswold's and the Witch's sell-list logic unified into one shared `PopulateSellList` helper (the Witch's sell list is now price-sortable too, matching Griswold's); several smaller consistency fixes in stores.cpp/options.cpp/demomode.cpp. All 168 automated tests across the six regression suites pass. Version bumped to 0.2.0. **Not yet packaged as a release ZIP** - awaiting the user's own manual playthrough first.
- 2026-08-04: before any manual test, the user asked to classify all 37 Oracool options as either a permanent always-on feature or a kept INI toggle, one option at a time rather than by inferred grouping, and to fold the result into the still-unreleased v0.2.0. 11 became always-on (see the "Always-on feature conversion" checklist item above and Gameplay-Changes.md's OE-018 "Toggle classification" section for the full list and rationale); the other 26 stay exactly as configurable as before. Every removed option's field, INI key, menu entry, and call site was deleted rather than left as a dead always-true toggle. Three inv_test cases that specifically exercised a now-impossible "disabled" branch were removed, and two others (player_test's Respawn In Town tests, stores_test's Witch price-sort test) were simplified/renamed to match. Rebuilt `devilutionx.exe` and all affected test targets from a clean `cmake .` + `ninja`; 165 automated tests across the six regression suites pass (168 minus the three deleted dead-branch tests), plus `missiles_test` (5 tests, touched incidentally since its setup referenced the now-removed `stackableConsumables`). Still version 0.2.0 - still not packaged, still awaiting the user's manual playthrough.
- 2026-08-04: the user confirmed v0.2.0 worked after clearing the incompatible old saves, and asked to release it. Built an optimized x64 Release `devilutionx.exe` (with Discord SDK integration to match v0.1.0's package), assembled and SHA-256-verified the ZIP (15 files, byte-for-byte match), and published it as a GitHub Release (tag `v0.2.0`) on the user's newly created private repo `Oracooll/Diablo-Oracool-Edition`, which this working copy is now linked to as `origin` (all prior history pushed to `main`). Immediately after, the user flagged one more thing before calling v0.2.0 truly complete: vanilla DevilutionX's six potion auto-pickup options (Heal/Full Heal/Mana/Full Mana/Rejuvenation/Full Rejuvenation) each capped auto-pickup at 16 pieces, which no longer made sense now that Stackable Consumables (OE-012, always-on since OE-018) lets a slot hold up to 99. Converted all six from a numeric cap to a plain on/off switch (see OE-019 checklist above), matching how Elixir/Oil auto-pickup already worked. All 170 tests (165 across the six regression suites + missiles_test's 5) pass. Version bumped to 0.2.1 - not yet re-released as a new ZIP/GitHub Release, pending the user's decision on whether this warrants its own release or folds into a later one.
- 2026-08-04: the user asked to release "this package" as v0.2.0, but the working tree was already at v0.2.1 (auto-pickup fix) with an existing, differently-content v0.2.0 tag already published - flagged the mismatch and released as v0.2.1 instead, matching what the built exe actually reports. Built, packaged, SHA-256-verified (15/15 files), tagged, and published as a GitHub Release, save-compatible with v0.2.0 (no save-format change in this release).
- 2026-08-04: the user shared a `diablo.ini` they'd hand-tuned across their installs and asked for those settings to become the new code defaults for future releases. Diffed every key against the live `options.cpp` defaults; confirmed scope with the user on the handful of ambiguous cases (declined baking in muted audio, approved the 900x600 resolution and Hardware Cursor For Items as new defaults). Applied as OE-020 (v0.2.2): ~22 vanilla `[Game]` defaults and ~10 `[Oracool Edition]` defaults changed, plus the two `[Graphics]` ones. While verifying, found and fixed a genuine pre-existing bug this exposed: `CheckUnique`'s uniqueness roll used the *live* `uniqueItemDropMultiplier` even when reconstructing an already-saved item (hero-select preview, `UnPackItem`/`RecreateItem`), so raising the multiplier's default from 1 to 25 could flip a previously-Magic item into Unique on reconstruction - a real bug, unmasked by (not caused by) the default change, now fixed by pinning the multiplier to 1 during reconstruction (see OE-020 in Gameplay-Changes.md for the full mechanism). Also fixed a test-harness-only crash: `showItemGraphicsInStores` defaulting to true made every store-listing test assert on a null sprite, since test binaries never load real item graphics - disabled it globally in `test/main.cpp`, the same way the hardware cursor already is. All 170 tests pass. Version bumped to 0.2.2 - not yet packaged/released, pending the user's decision.
- 2026-08-04: the user reported a genuine bug: Health/Mana potions sometimes wouldn't stack with an existing belt stack "from time to time." Traced to a real, previously-undiscovered issue: `AllItemsList` carries two separate `_item_indexes` for several potions (one for vendor stock/starting gear, one for actual monster/floor drops) that display identically but have different `IDidx` - and `canStackWith` compared `IDidx`. Confirmed concretely via Pepin's/Adria's shop code and new-character belt setup, all of which use the vendor-reserved index directly; Rejuvenation potions were spared because their lookup happens to go through a helper that finds the drop-pool index instead. Fixed as OE-021 (v0.2.3) by switching `canStackWith` to compare `_iMiscId` (+ `_iSpell` for scrolls, to avoid merging different spells) instead of `IDidx`. Two new regression tests added (`items_test`, now 49 tests). All 172 tests across the six regression suites plus `missiles_test` pass. Version bumped to 0.2.3 - not yet packaged/released.
- 2026-08-04: the user asked to develop the rest of the backlog ideas before releasing again, picked the Reset Stats button reposition/icon to build first, and asked for design-only research passes (no code) on the gold-to-Stash and durability-inactive ideas in parallel. Repositioned the Reset Stats button next to "Points to distribute" and swapped its "R" for a circular-arrow Unicode glyph (unverified rendering - no screenshot tooling available here, confirmed the renderer safely falls back to "?" rather than breaking if the glyph is missing). Mid-task, the user separately flagged that Rare items show Magic's blue inventory background instead of their own - fixed `InvDrawSlotBack` to check the Oracool tier before falling back to vanilla `_iMagical`, scoped to Rare only as reported. Bundled both as OE-022 (v0.2.4). All 172 tests still pass (both changes are cosmetic, no new test coverage needed). Version bumped to 0.2.4 - not yet packaged/released. Design-pass research (gold-to-Stash, durability-inactive) is still in progress separately.

## Migration batches

### Batch A: Foundation

- Confirm `diablo.ini` is beside `devilutionx.exe`.
- Create or save a character and confirm files appear under `Saved_Games` beside the executable.
- Confirm the game starts with no existing local INI or save folder.

### Batch B: World, items, and character

- Exercise both values of every option.
- Confirm player-dropped unidentified items remain unidentified when Auto Identify Drops is enabled.
- Test all relevant Diablo and Hellfire town entrances.
- Validate stat colors, the 255 cap, save/reload, toggle transitions, and repeated Reset Stats operations.
- Validate Town Portal on a new character, an existing character, and a character with a spell level above 1.

#### Character acceptance checklist

- With `Remove Stat Limits=1`, allocate every base attribute beyond its class cap and verify allocation stops at 255; 255 is gold.
- Save and reload an over-cap character. With the option changed to `0`, verify the real over-cap value remains intact, is red, and has no `+` button.
- With `Reset Stats Button=1`, open the local character panel and click the silver `R`; it turns red while pressed.
- Verify class starting base attributes are restored and available points equal exactly `5 × (level − 1)`. Click `R` again and verify the result is unchanged.
- Confirm multiplayer shows no reset control and retains vanilla stat limits.
- With `Permanent Free Town Portal=1`, verify Town Portal appears as a memorized level-1 spell on new and existing characters, costs zero mana, and does not reduce an existing higher spell level.
- Disable the portal option and verify ordinary mana cost returns; the already-granted learned spell may remain.

### Batch C: Griswold

- Test every menu route, Back route, insufficient-gold case, full-inventory case, purchase removal, and persistence behavior.
- Verify Premium Refresh and Refresh Until do not mutate unique-shop stock.
- Test unique-shop counts 1 and 8 and multiple price multipliers.
- Test stable descending sell sorting, including equal prices and belt items.

#### Basic services acceptance checklist

- Enable `Griswold Premium Refresh`, open Premium Items, and verify a `Refresh` footer action appears above `Back`.
- Activate Refresh several times and verify the complete premium inventory changes without charging gold.
- Buy an item after refreshing and verify the correct selected item is purchased and removed.
- Disable the option and verify the Refresh action disappears; confirm it is also absent in multiplayer.
- Test `Griswold Restore Health` and `Griswold Restore Mana` separately, then together. Enter Griswold's main menu with depleted resources and verify only enabled resources silently refill.
- Confirm restoration produces no spell sound or extra menu entry, and remains inactive in multiplayer.

#### Refresh Until acceptance checklist

- Under the final `[Oracool Edition]` section, set `Griswold Refresh Until Button=1`, choose `Griswold Refresh Until Timeout Seconds`, and set `Griswold Refresh Until Item Names` to one or more exact displayed names separated by semicolons—for example `King's Sword of Haste; Awesome Plate`.
- Verify leading/trailing spaces around each semicolon-separated target are ignored and matching is case-insensitive but requires the complete item name.
- Activate `Refresh until` and verify success feedback reports the matching item and attempt count; confirm the matching premium inventory remains available for purchase.
- Use an empty target setting and verify clear feedback appears without changing stock.
- Search for an impossible name with a short nonzero timeout and verify timeout feedback appears while the last generated stock remains.
- Set timeout to `0`, search for an impossible name, and verify the operation stops at the 100,000-generation hard limit.
- Enter multiple targets and verify finding any one succeeds. Confirm the action is absent in multiplayer and when its toggle is disabled.

#### Consumables and recharge acceptance checklist

- Set `Griswold Sell Consumables=1` and `Griswold Recharge Staves=1`, then verify Griswold's expanded menu shows `Buy consumables` and `Recharge staves`.
- Open Buy Consumables and verify the first four entries are, exactly in order: Potion of Healing, Potion of Full Healing, Potion of Rejuvenation, and Potion of Full Rejuvenation.
- Verify Adria's normal stock begins immediately after those four entries and that no other Pepin-generated merchandise appears.
- Buy each of the four fixed potions and verify gold is deducted, the item is placed normally, and the same potion remains in stock after every purchase.
- Buy one replenishing and, if available, one non-replenishing Adria entry; verify each retains Adria's normal stock behavior.
- Scroll through the complete list immediately after opening and verify all four fixed potions and all Adria entries are reachable.
- Test insufficient gold and full inventory, then verify confirmation cancellation, `Back`, and Escape return to Griswold—not Adria.
- Test an equipped staff and an inventory staff with missing charges. Verify eligibility, price, confirmation, gold deduction, and restored charges match Adria.
- Confirm a fully charged or otherwise ineligible staff is absent, and all recharge dialog paths return to Griswold.
- Disable each option separately and verify only its corresponding menu entry disappears. Confirm both services remain absent in multiplayer.

#### Bulk sale and sorting acceptance checklist

- (As of v0.2.0, `Griswold Buy All Items` and `Griswold Sort Sell Items by Price` are permanent, non-optional parts of Oracool Edition — there is no INI toggle for either anymore.)
- Verify `Sell all` appears above `Back` in Griswold's Sell Items screen.
- Prepare equipment, Adria-sellable consumables, and staves in both inventory and belt, together with gold and any quest item. Verify the expanded individual sell list contains every positive-value non-quest item, then activate `Sell all` and verify those items are removed and paid for while gold and quest items remain.
- Verify the resulting gold total equals the sum of the individual displayed sale prices.
- Test with nearly full inventory/gold capacity and verify the operation stops safely if proceeds cannot fit.
- Verify displayed prices descend from highest to lowest across inventory and belt.
- Include two equal-price items and verify their original relative order remains stable.
- Sell items from the beginning, middle, and end of the sorted list and verify the selected original item—not a neighboring inventory item—is removed.
- Confirm neither feature is active in multiplayer.
- With the expanded Griswold menu active, open and close each service using both `Back` and Escape; verify the selector returns to the exact service that was closed.

#### Unique shop acceptance checklist

- With `Griswold Sell Unique Items=1`, start a new single-player game and verify Griswold shows `Buy unique items`; confirm the entry is absent when disabled and in multiplayer.
- Verify the shop contains the number configured by `Griswold Unique Shop Items`, contains no duplicate unique names, and every item is identified.
- Compare an item's displayed purchase price using at least two `Griswold Unique Item Price Multiplier` values in separate new games.
- Buy an item and verify the correct item enters inventory, the correct gold is deducted, and the purchased entry disappears without replacement.
- Test confirmation cancellation, insufficient gold, full inventory, `Back`, and Escape. Every return route must restore the selector to `Buy unique items`.
- Use Premium Refresh and Refresh Until, then reopen the unique shop and verify its stock is unchanged.
- Leave and return to town during the same game and verify the remaining unique stock is unchanged.

### Batch D: Auto-save

- Periodic save at one minute.
- Level transition in both directions.
- One non-gold pickup and rapid multiple pickups.
- Gold-only pickup does not save.
- Successful purchase saves; cancelled and failed purchases do not.
- Notification off still saves.
- Master option off disables every trigger.

### Post-migration: Gold Stacks Buff

- (As of v0.2.0, this is a permanent, non-optional part of Oracool Edition — there is no INI toggle anymore. The cap itself was also raised from 65,535 to 100,000,000 in the same release; see the v0.2.0 Foundations Pass checklist below for that specific check.)
- Merge or collect more than 5,000 gold into one inventory square and verify the same stack continues growing.
- Split a large stack, drop part of it, pick it back up, and verify both the stack values and large-gold cursor remain correct.
- Save, exit completely, reload the character, and verify a large stack is preserved exactly.
- Buy an item costing more than 5,000 and verify payment is deducted correctly from the enlarged stack.
- Move gold to and from the stash and verify enlarged inventory stacks merge correctly.
- Repeat a withdrawal with inventory space for only part of the requested gold and verify the unplaced remainder stays in the stash.
- Confirm multiplayer retains vanilla gold-stack behavior.

### Post-migration: Respawn In Town (OE-014)

- (As of v0.2.0, this is a permanent, non-optional part of Oracool Edition — there is no INI toggle anymore.)
- Equip gear, carry inventory items, belt consumables, and gold, then die to a monster or trap. Verify nothing is dropped on the ground and the death menu shows `Respawn In Town` between `New Game` and `Load Game`.
- Select `Respawn In Town` and verify the character revives in town with every equipped item, inventory item, belt item, and all gold intact.
- Verify `Save Game`, `Options`, `New Game`, `Load Game`, and `Quit Game` still work normally from the death menu.
- Confirm multiplayer is unaffected: its own `Restart In Town` entry and vanilla drop-on-death behavior are unchanged regardless of the single-player setting.

### Post-migration: Release branding and version display (OE-015)

- Launch the game and confirm the main menu shows two stacked lines: `DevilutionX 1.5.5` above `Oracool Edition v0.1.0` (the Oracool version is its own line, not appended to the engine version).
- Attempt to join or host a multiplayer game and confirm version-compatibility checks still behave normally (unaffected by this change).

### Post-migration: Stackable Consumables (OE-012)

- (As of v0.2.0, this is a permanent, non-optional part of Oracool Edition — there is no INI toggle anymore.)
- Pick up several identical identified potions and verify they merge into one inventory stack with a small quantity number over the icon, instead of taking one slot each.
- Pick up enough of the same potion to exceed 99 and verify the stack stops at 99 with the remainder placed in a new slot.
- Pick up an unidentified potion of the same base type as an identified stack and verify it does NOT merge (stays separate) until both share the same identified state.
- Drink/use a stacked potion repeatedly and verify the quantity decreases by one each time, and the slot only disappears once it reaches zero.
- Use a stacked scroll the same way; verify it behaves identically to potions.
- Drag a held stack onto a matching stack in the inventory or belt and verify it merges, with any overflow beyond 99 staying on the cursor as a separate stack.
- Shift+right-click a stack of more than one and verify a split dialog opens; enter a quantity and verify the source stack decreases by that amount while the entered amount appears on the cursor.
- Buy potions from Griswold, Adria, or Pepin into an inventory that already has a matching stack and verify they merge instead of taking a new slot.
- Sell a full stack of a stacked consumable at Griswold and verify the payout equals the per-unit price times the stack quantity.
- Have a Succubus (or similar steal-potion effect) target a character carrying a large stack of a base-tier potion (Heal/Mana) and verify only one unit is removed, not the whole stack. Repeat with a stack of an upgraded-tier potion (Full Heal/Full Mana/Rejuvenation/Full Rejuvenation) and verify the source stack loses only one unit while a single downgraded potion appears elsewhere on the belt (or is lost if the belt is full).
- Load a character saved before this feature existed and verify their unstacked potions/scrolls behave normally, then merge naturally the next time a matching item is picked up or bought.
- Confirm multiplayer is unaffected regardless of the single-player setting.

### Post-migration: Belt Mod (OE-013)

- (As of v0.2.0, this is a permanent, non-optional part of Oracool Edition — there is no INI toggle anymore.)
- Put a large stack (e.g. 40+) of a potion in your inventory and a single unit of the same potion on a belt slot. Use the belt potion and verify the slot silently refills — up to 99 — from the inventory stack, and the inventory stack shrinks (or disappears entirely if it was smaller than what the belt slot could take).
- Confirm no hotkey number ("1" through "8") is ever shown on an occupied belt slot — only the OE-012 quantity overlay.
- Use a belt potion down to its last unit with no matching potion anywhere in inventory, and verify the slot goes fully empty (not grayed out or reserved) and is immediately usable for a different item.
- Drag a potion onto/off a belt slot and reorder belt slots; verify this still works exactly as before.
- Buy or pick up more of a potion type already resident on the belt and verify it still tops off the belt slot first (existing OE-012 behavior), rather than going to inventory.
- Confirm multiplayer is unaffected regardless of the single-player setting.

### Post-migration: Rare Items (OE-008)

- With `Rare Item Drop Chance` at its default (8) and `Auto Identify Drops=1`, play until a Rare item drops (killing monsters and opening chests at a level with magic-eligible drops; raising the drop chance temporarily, e.g. to 50-100, makes this much faster to trigger for testing). Confirm the item's name shows in yellow as `Rare {item name}` and that it appears already identified.
- Set `Auto Identify Drops=0`, restart, and get another Rare item to drop. Confirm it now drops unidentified (needs a scroll or Cain), the same as any other magic item — Rare items must not force identification on their own.
- Hover the Rare item in your inventory and confirm the floating stat popup lists every one of its affixes (1-4 lines), matching the same visual style as a vanilla Unique item's popup.
- Specifically get a Rare ring or amulet to drop (jewelry has a much smaller affix pool than weapons/armor, the exact scenario a past bug was found in) and confirm it still always has at least 1 prefix + 1 suffix, never fewer.
- With the popup open, also open your character panel, then your inventory, then a store — confirm the popup still renders on top of all of them in every combination, and confirm hovering a vanilla Unique item's popup is also now always on top (the same z-order fix applies to both).
- Move the mouse off the item and confirm the popup disappears.
- Take the Rare item to Griswold (or the appropriate vendor) and confirm its store listing shows a bonus line summarizing its affixes, and that selling it, repairing it, and (if it has charges) recharging it all work normally.
- Set `Rare Item Drop Chance=0`, restart, and confirm no new Rare items generate, while any Rare item already on the character (inventory, belt, equipped, stash) remains fully intact, correctly named/colored, and usable.
- Load a save from before this feature existed (or a save with no Rare items) and confirm it loads normally with no errors.
- Confirm multiplayer never generates a Rare item regardless of the option's value.
- Drop a Rare (or Buffed Unique, or Primal) item on the ground where you're currently standing, save the game, and reload. Confirm the item on the ground still shows its tier color and full affix list, not a plain magic-item appearance. Try a small batch (a few of each tier) at once and check every single one, not just the first you notice. (Known remaining gap: an item left on a *different* level, then revisited later, isn't covered by this fix yet.)

### Post-migration: Buffed Uniques (OE-009)

- With `Buffed Unique Item Drop Chance` at its default (3), play until a Buffed Unique drops (raising the setting temporarily, e.g. to 50-100, makes this much faster to trigger). Confirm the item's name shows in gold as `Unique {item name}` — not "Buffed Unique" — matching vanilla Unique item coloring.
- Hover it and confirm the description panel under the belt row reads "unique item" (fixed from a bug where it incorrectly read "rare item").
- Hover it and confirm the floating popup lists 4-6 affix lines total (at least 2 prefixes + 2 suffixes), reusing the same popup mechanism already verified for Rare items.
- Specifically get a Buffed Unique ring or amulet to drop (the exact item class a past bug was found on) and confirm it still always has at least 2 prefixes + 2 suffixes, never fewer.
- Confirm identification also follows `Auto Identify Drops` for Buffed Uniques, the same as Rare items and everything else — no per-tier special casing.
- Confirm a real vanilla Unique item can still drop normally and looks/behaves exactly as before (same gold color, same static name, same popup) — Buffed Uniques must never replace or reduce vanilla Unique frequency.
- Take a Buffed Unique to a vendor and confirm its store listing, selling, repair, and recharge all work the same way Rare items' did.
- Set `Buffed Unique Item Drop Chance=0`, restart, and confirm no new Buffed Uniques generate while any already on the character remain intact and usable.
- Confirm multiplayer never generates a Buffed Unique regardless of the option's value.

### Post-migration: Primal Items (OE-010)

- With `Primal Item Drop Chance` at its default (1) (raise it temporarily, e.g. to 20-30, to trigger one faster for testing), play until a Primal item drops. Confirm the item's name shows in orange as `Primal {item name}` — distinct from White (normal), Blue (magic), Yellow (Rare), and Gold (Unique/Buffed Unique).
- Hover it and confirm the floating popup lists exactly 6 affix lines (3 prefixes + 3 suffixes) every time — never fewer, unlike Rare/Buffed Unique's variable counts. Also confirm the description panel under the belt row reads "primal item" (fixed from a bug where it incorrectly read "rare item").
- If you can compare against a Rare or Buffed Unique item with the same affix (e.g. both rolled a to-hit bonus), confirm the Primal's value is clearly at the top end of that affix's range — every Primal affix should read as the best possible roll for its type.
- Confirm the Primal item shows full/undamaged durability the moment it drops.
- Confirm identification also follows `Auto Identify Drops`, the same as every other tier.
- Take a Primal item to a vendor and confirm its store listing, selling, repair, and recharge all work the same way Rare/Buffed Unique items' did.
- Confirm a real vanilla Unique and a Buffed Unique can each still drop normally alongside Primal being enabled — none of the three tiered rolls should crowd out the others.
- Set `Primal Item Drop Chance=0`, restart, and confirm no new Primal items generate while any already on the character remain intact and usable.
- Confirm multiplayer never generates a Primal item regardless of the option's value.

### Post-migration: Tabbed Inventory (OE-017)

- (As of v0.2.0, this is a permanent, non-optional part of Oracool Edition — there is no INI toggle anymore.)
- Open your inventory and confirm 10 small numbered tab buttons appear in the gap between the ring row and the backpack grid, one above each grid column. Tab 1 should be gold/enlarged by default (it's the currently selected tab); tabs 2-10 should be gray.
- Click tab 3 (or any other) and confirm the backpack grid's contents change to show that page (empty, the first time), the clicked tab turns gold and grows slightly, and the previously selected tab returns to gray/normal size.
- Place an item into tab 3, then switch to tab 1 and confirm the item is not there (and tab 1's original items are all still exactly where they were), then switch back to tab 3 and confirm the item is still there.
- Confirm items can be equipped directly from an extra tab via shift-click (auto-equip) or dragged onto an equip slot, exactly like from the original backpack.
- Confirm a stack of potions/scrolls placed in an extra tab still merges correctly with another matching stack in that same tab (Stackable Consumables behavior carries over).
- Confirm a multi-cell item (e.g. a 2x2 or 2x3 piece of armor) can be placed in an extra tab and correctly blocks the cells it covers - no other item should be placeable on top of it until it's moved.
- Try to place gold or a quest item into an extra tab and confirm the paste is rejected (the item stays on the cursor) — both must stay in the original backpack.
- Move a Rare/Buffed Unique/Primal item into an extra tab, save and reload (or exit and reopen the game), and confirm it's still there with its full tier coloring, name, and affix list intact.
- Hover a Rare/Buffed Unique/Primal item in an extra tab and confirm both the floating stat popup AND the classic description panel under the belt row show correctly (fixed from a bug where only the popup worked). Hover a magic, basic (white), or consumable item in an extra tab too, and confirm the under-belt panel shows its name/damage/armor/durability/charges as appropriate — this previously showed nothing for anything in an extra tab.
- Fill tab 1 completely, then buy an item from any vendor (Griswold, Adria, Pepin, Wirt, Cain) and confirm the purchase succeeds and the item lands in the first extra tab with room, rather than being rejected with a "no room" message (fixed from a bug where vendors only ever checked tab 1).
- Fill tab 1 completely, then pick up an item from the ground (both by walking over it with auto-pickup enabled, and by manually clicking it) and confirm it lands in an extra tab instead of the pickup silently failing.
- Fill tab 1 completely, then withdraw an item from the Stash and confirm it lands in an extra tab instead of failing with "no room."
- Move an item into an extra tab, then open Griswold's or the Witch's Sell Items list and confirm it appears there and can be sold normally (gold received, item removed from the tab). Also confirm a quest item and gold still cannot be moved into an extra tab even when tab 1 is completely full (pickup/purchase should just fail for those, not redirect them).
- Move an item into an extra tab, drop it on the ground (or otherwise remove it), save, then save again (or wait for an auto-save) and reload — confirm the item does not reappear in the tab (fixed from a bug where a stale save-file entry could resurrect it).
- Close and reopen the inventory panel and confirm it always reopens on tab 1, regardless of which tab was selected before closing.
- Confirm multiplayer never shows the tab buttons or activates the feature.
- Confirm the character's `Saved_Games` folder still contains only the usual single save file per character — no new files should appear.
- Put a matching stack of a consumable only in an extra tab (none in tab 1 or the belt), place a single unit of that consumable on the belt, and consume it — confirm the slot refills from the extra tab instead of staying empty. Also try a case where tab 1 has a small matching stack and an extra tab has more of the same item, and confirm tab 1 drains first before the extra tab is touched.

### Post-migration: Debug drop command network-safety fix

- (Debug/`_DEBUG` builds only.) Use the console's `drop {name}` command many times in a row (20+) for a few different item names, including some that resolve to Rare/Buffed Unique/Primal items. Confirm every single one can be picked up, viewed, equipped, AND dropped back onto the ground without ever producing a "sent an invalid packet" message or the item vanishing — this used to fail roughly half the time.

### Post-migration: v0.2.0 Foundations Pass (OE-018)

**Save compatibility** — this is the one thing that must NOT work: confirm a save from any v0.1.x build fails to load with a clear "incompatible version" message rather than a crash, silently missing items, or corrupted data. Starting a brand-new character on v0.2.0 should work completely normally.

**Item tier persistence, the core of what changed** — for each of Rare, Buffed Unique, and Primal, get at least one item and confirm its tier color, name, and full affix list survive a save/reload while:
- sitting in your backpack (tab 1), equipped, and on the belt (matches prior behavior, shouldn't have changed)
- sitting in a Tabbed Inventory extra tab
- sitting in the Stash
- **left on the ground** — this is the specific case that used to break in v0.1.13's gap. Drop items of every tier on your current floor, save, reload, confirm they're still tiered. Then, as a stretch check (not expected to fully work — see the known remaining gap in Gameplay-Changes.md OE-018): drop a tiered item, walk to a different level, save, reload, and see whether it kept its tier when you go back to check on it.

**Other fixes in this pass**:
- Buy out Griswold's entire basic-items stock, then reopen "Buy Basic Items" — confirm it shows an empty list normally instead of bouncing you back to the store menu.
- Sell a mix of differently-priced items to the Witch, and confirm her sell list is sorted highest-price-first (previously only Griswold's list sorted).
- Pick up (or buy) a stackable consumable while a matching stack already sits in one of the extra tabs — confirm it merges into that stack instead of creating a new one in the backpack.
- Equip a two-handed weapon from your inventory while both hands are already occupied by one-handed items AND your backpack (tab 1) is completely full but at least one extra tab has room — confirm the swap either succeeds cleanly or correctly fails without losing, duplicating, or swapping in the wrong item. This is a narrow, hard-to-hit scenario; if you can't reproduce the exact setup, a normal two-handed-weapon swap with room to spare is still worth confirming works as before.
- Try selling and buying at both Griswold and the Witch generally (backpack, belt, and extra-tab items) to confirm the shared sell-list rewrite didn't change anything about normal buy/sell behavior.
- Sanity-check a gold stack well above the old 5,000/65,535 figures still displays and splits correctly.

**Always-on feature conversion** — after going through all 37 Oracool options one by one, 11 of them lost their INI toggle and are now permanent: uncapped base attributes (255 cap), Respawn In Town, Gold Stacks Buff, Stackable Consumables, Belt Mod, Tabbed Inventory, permanent free Town Portal, Griswold buying every item (with Sell All), Griswold selling consumables, Griswold recharging staves, and Griswold/Witch sell-list price sorting. Confirm each of these behaves as always-on with no corresponding entry left in the Oracool Edition options menu, and that editing `diablo.ini` to try to turn any of them off has no effect (the key is simply ignored if still present from an old config).

### Post-migration: v0.2.1 Unlimited Potion Auto-Pickup (OE-019)

- Turn on `Heal Potion Pickup` in the vanilla Gameplay settings, get a stack of a Healing potion up near 99 (e.g. buy them from Pepin/Griswold), then walk over more Healing potions on the ground — confirm they keep getting auto-picked-up past the old 16-piece ceiling, growing the stack (or starting a new one if the first is full) all the way toward 99.
- Repeat briefly for at least one of the other five (Full Heal, Mana, Full Mana, Rejuvenation, Full Rejuvenation) to confirm the same fix applies to all six.
- Turn `Heal Potion Pickup` back off, confirm auto-pickup for that potion type stops entirely (it's a plain on/off now, no partial/numeric state).
- If you have a `diablo.ini` from before this fix with one of these six options set to a nonzero number (e.g. `8`), confirm it now behaves as simply "on" after upgrading, and one previously left at `0` still behaves as "off."
- Confirm Elixir and Oil auto-pickup (already-existing boolean options) are unaffected by this change.

### Post-migration: v0.2.2 Curated Default Settings (OE-020)

- Rename or move aside any existing `diablo.ini`, launch the game fresh, and let it generate a brand-new one. Confirm the new defaults took effect: Run in Town on, Experience Bar/Enemy Health Bar/Show Monster Type/Show Item Labels/health-and-mana-globe-values/item-graphics-in-stores all on, floating combat numbers on (vertical), Auto Refill Belt on, Disable Crippling Shrines on, Auto Gold/Elixir Pickup and all six potion types and Auto Pickup in Town on, Randomize Quests off, Auto Equip Weapons off, window opens at 900x600, Hardware Cursor For Items on.
- In the same fresh `diablo.ini`, confirm the Oracool Edition section: Auto Pickup Range is 5, Rare/Buffed Unique/Primal drop chances and the Unique Item Drop Multiplier are all at their new higher values, Griswold's Unique Shop is off (but its item count/price multiplier are still saved as 8/20x for later), Griswold Refresh Until Button is off, and Auto Save Notification is off.
- Restore a `diablo.ini` from before this change (any version) and confirm every setting in it is preserved exactly as saved — none of the new defaults should override an existing value.
- **Item reconstruction bug fix**: get or create a Magic (blue) item, note its name precisely, save and exit to the main menu (not fully quitting), then look at that character in the hero-select list. Confirm it still shows as the same Magic item, not a Unique. This is the scenario the underlying `CheckUnique` fix targets — most likely to show up (if it were still broken) specifically because Unique Item Drop Multiplier now defaults much higher than before.

### Post-migration: v0.2.3 Fixed potions that silently refused to stack (OE-021)

- Buy a Potion of Healing from Pepin (Griswold's Buy Consumables), then find or drop a plain Potion of Healing on the ground and pick it up. Confirm they merge into one stack — this exact combination used to silently fail to stack.
- Buy a Potion of Mana from Adria/the Witch, then pick up a dungeon-dropped Potion of Mana. Confirm they merge.
- Start a brand-new character, and without using either starting belt potion, pick up a matching potion from the ground (Heal or Mana depending on Diablo/Hellfire). Confirm it merges into the starting belt slot instead of taking a new slot.
- Sanity-check Full Healing and Full Mana the same way (bought from Pepin/the Witch respectively, then merged with a ground-found one) — these had the identical underlying issue.
- Confirm Rejuvenation and Full Rejuvenation potions still merge correctly regardless of source (these were never actually broken, just to be sure nothing regressed).
- Pick up two different scroll types (e.g. Scroll of Firebolt and Scroll of Identify) into the same general inventory area and confirm they still occupy separate slots — do NOT merge with each other, even though both are technically the same "misc" scroll category internally.

### Post-migration: v0.2.4 Small UI fixes (OE-022)

- Open the character panel and confirm the Reset Stats button sits right next to the "Points to distribute" number instead of off to the side.
- Look closely at the Reset Stats button's icon. **This is the key check** — confirm whether it shows a circular-arrow symbol or a "?" character. Either is a valid, non-broken outcome to report back; a "?" just means that glyph needs to be swapped for a different one.
- Get or drop a Rare item and place it in your backpack, an extra tab, and the Stash. Confirm its slot background is yellow, not blue, in all three places. Compare side-by-side with a plain Magic item (blue) and a Unique/Buffed Unique/Primal item (also yellow, unchanged) to confirm Rare is now visually distinct from Magic specifically.
- Equip a Rare item and confirm its equipped-slot background is also yellow (the head/weapon/armor/ring/amulet slots use the same fix, not just the grid).
