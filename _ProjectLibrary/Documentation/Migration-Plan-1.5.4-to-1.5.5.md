# DevilutionX 1.5.4 to 1.5.5 Migration Plan

## Source of truth

The earlier ChatGPT development task `Diablo DevilutionX Oracool Edition` and its recorded successful user tests are the historical specification. The original 1.5.4 working tree and patch attachments are not locally available, so implementation is reconstructed from the accepted behavior and adapted directly to the 1.5.5 codebase.

## Porting order

1. Foundation: portable paths, Oracool option category, shared single-player guard.
2. World and items: unique multiplier, entrances, infravision, auto-identification, auto-pickup range.
3. Character: stat limits, over-cap persistence/display, deterministic reset, free Town Portal.
4. Griswold basic services: Refresh, Refresh Until, universal buying, consumables, recharge, restoration, sorting.
5. Griswold advanced generation: unique shop and premium limit controls.
6. Auto-save triggers and safety logic.
7. Complete generated INI documentation and full regression test.

## Progress

- Steps 1 and 2: implemented, build-verified, and accepted by the user.
- Step 3: implemented, build-verified, and accepted by the user on 2026-08-02.
- Step 4: Premium Refresh, Refresh Until, restoration, Buy Consumables, Recharge Staves, Buy All, stable sell sorting, and cursor restoration are user-accepted.
- Step 5: Unique shop is user-accepted. Premium limit controls are build-verified, with high-level boundary testing deferred.
- Step 6: Auto Save is implemented, build-verified, and user-accepted.
- Step 7: complete. Generated INI documentation and the final migration audit are complete; all 213 automated tests pass.
- Post-migration OE-007 redesign is build-verified and awaiting user acceptance: four infinite Pepin potions appear before Adria's stock, with no other Pepin merchandise included.

## Final migration verification

- Every feature recorded in the 1.5.4 feature catalogue has a corresponding 1.5.5 implementation.
- The complete Debug build succeeds.
- The complete automated test suite passes: 213 of 213 tests on 2026-08-02.
- User acceptance is complete for all migrated features except the Premium limit controls' high-level boundary cases, which remain deliberately deferred until a sufficiently high-level character is available.
- The migration introduces no save-format changes.
- The 1.5.4-to-1.5.5 migration is complete. Further work belongs to the separately recorded post-migration roadmap.

## Commit policy

Each independently testable feature or tightly coupled feature group receives its own commit after compilation and focused verification. User acceptance testing is recorded separately from developer build verification.

## Historical implementation hazards to avoid

- Never expose the internal `GetUniqueItem` symbol directly; the prototype required a separately named public wrapper to avoid ambiguous lookup.
- Griswold recharge confirmation must explicitly handle the Griswold/Smith recharge dialog route.
- Translated runtime format strings must use `fmt::runtime` where required by the bundled fmt version.
- Reset Stats must use the deterministic level-based budget; inferring refundable history from current stats caused point duplication and loss.
- Over-cap base stats must not be destructively clamped when the option is disabled.
- Auto-save public hooks must not be duplicated across anonymous and normal namespaces.

## Acceptance corrections

- 2026-08-02: The initial Buy All Items migration exposed `Sell all` but retained Griswold's vanilla equipment-only filter. It was corrected to expand Griswold's individual and bulk sell eligibility to all positive-value non-gold, non-quest items, including Adria categories.
- 2026-08-02: Expanding Griswold's menu moved the vanilla entries upward, while legacy Back/Escape handlers retained their original line numbers and returned the selector one entry too low. All Griswold menu positions are now resolved through one shared layout function.
- 2026-08-02: The three Refresh Until INI keys were standardized with a `Griswold` prefix, and save ordering was changed so `[Oracool Edition]` is always the final INI section.
- 2026-08-02: The complete Oracool INI catalogue was organized into named groups, alphabetized within each group, and restored with the detailed setting explanations developed for the 1.5.4 project. A dedicated serializer now preserves this canonical documented layout on every options save.
- 2026-08-02: All Boolean Oracool modifications were standardized as enabled by default for new configurations. Numeric tuning settings retain their established baseline values.
- 2026-08-02: Refresh Until was user-accepted after successfully locating an Obsidian Ring.
- 2026-08-02: User testing of the Premium limit controls was deferred because the available character is level 30; the controls remain build-verified rather than user-accepted.
- 2026-08-02: The Unique Shop purchase and no-replacement behavior passed user testing. Purchased uniques were initially hidden from Griswold's Sell Items list when their base ID fell in the quest range; shop merchandise is now explicitly marked and permitted for resale while actual quest items remain protected.
- 2026-08-02: The Unique Shop resale correction passed user testing, completing acceptance of the feature.
- 2026-08-02: All Auto Save rules passed user testing, including periodic, level-change, non-gold pickup, store-purchase, debounce, and gold-exclusion behavior.
