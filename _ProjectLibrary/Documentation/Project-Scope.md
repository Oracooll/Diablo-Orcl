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

1. Gold Stacks Buff: completed and user-accepted at the existing save format's exact maximum of 65,535 per inventory stack.
2. Fixed Pepin potion section in Griswold's `Buy consumables`: exactly four infinite potions (Healing, Full Healing, Rejuvenation, and Full Rejuvenation) appear in that order before Adria's stock; implementation and build verification are complete, with user acceptance pending.
3. Add `RESPAWN IN TOWN` to the post-death menu. Selecting it must revive the character in town with all equipped gear and carried items retained; death must not drop any of the character's items. The implementation must preserve the existing death-menu choices and avoid changing save compatibility.
