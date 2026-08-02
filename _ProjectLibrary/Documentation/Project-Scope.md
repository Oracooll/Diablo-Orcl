# Diablo Oracool Edition

Diablo Oracool Edition is a controlled modification of DevilutionX 1.5.5.

## Baseline

- Upstream version: DevilutionX 1.5.5
- Working branch: `oracool-main`
- Primary platform: Windows
- Project root: `C:\DiabloDOE`
- Project library: `C:\DiabloDOE\_ProjectLibrary`

## Working rules

1. Preserve a reproducible, working 1.5.5 baseline before gameplay changes.
2. Make one focused feature change at a time.
3. Document gameplay, compatibility, and build effects.
4. Keep generated builds and release packages out of source history.
5. Test launch behavior before and after substantial changes.
6. Store Oracool documentation and supporting project materials under `_ProjectLibrary`.

## Milestones

- Milestone 0: Clean DevilutionX 1.5.5 build and launch verification. Completed.
- Milestone 1: Oracool Edition identity and core gameplay changes.

## Post-migration roadmap

1. Gold Stacks Buff: implemented at the existing save format's exact maximum of 65,535 per inventory stack; build verification complete and user acceptance pending.
2. Add Pepin's purchasable items to Griswold's `Buy consumables` store. Reuse Pepin's normal stock eligibility, pricing, replenishment, and purchase behavior while keeping all navigation and confirmation routes within Griswold's store interface.
3. Add `RESPAWN IN TOWN` to the post-death menu. Selecting it must revive the character in town with all equipped gear and carried items retained; death must not drop any of the character's items. The implementation must preserve the existing death-menu choices and avoid changing save compatibility.
