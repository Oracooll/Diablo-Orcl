# The Rift Monument menu acts on the release, not the press (v1.12.103)

**Date:** 2026-09-21 · **Version:** v1.12.103 · **Tests:** 832/832

User: "apply to Rift Monument menu same click mechanic" - Griswold's CONFIRM / CANCEL rule of v1.12.102.

`CheckStonegateMenuClick`'s row branch now only sinks the face and sounds. `ReleaseStonegateMenuButton`, the window's
LeftMouseUp hook, decides: if the release lands inside the button that was pressed, that row runs - a Nephalem Rift
opens, the best keystone turns, or Leave closes the menu - and if it lands anywhere else, nothing runs and the menu
stands. The Guardian row's "you need a keystone" refusal moved to the release with it, so a press no longer speaks
for a button the player may still slide off.

The close X and the click-away keep acting on the press: they are the window's own conventions, not the menu's
answer buttons.
