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
- Step 4: Premium Refresh, restoration, Buy Consumables, Recharge Staves, Buy All, stable sell sorting, and cursor restoration are user-accepted. Refresh Until is implemented and build-verified.
- Steps 5 through 7: pending.

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
