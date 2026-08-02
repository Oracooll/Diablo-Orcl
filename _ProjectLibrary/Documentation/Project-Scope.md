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

1. Increase the inventory gold-stack limit beyond 5,000 to the highest value that DevilutionX's runtime arithmetic and existing save format can safely represent. The implementation must prevent overflow, preserve saved characters, and update every gold placement, splitting, cursor, display, and validation path consistently.
2. Add Pepin's purchasable items to Griswold's `Buy consumables` store. Reuse Pepin's normal stock eligibility, pricing, replenishment, and purchase behavior while keeping all navigation and confirmation routes within Griswold's store interface.
3. Add `RESPAWN IN TOWN` to the post-death menu. Selecting it must revive the character in town with all equipped gear and carried items retained; death must not drop any of the character's items. The implementation must preserve the existing death-menu choices and avoid changing save compatibility.
