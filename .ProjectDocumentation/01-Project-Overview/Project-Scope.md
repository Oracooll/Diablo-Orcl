# Diablo Orcl — current project scope

Reviewed against source v1.12.347 on 2 October 2026.

Diablo Orcl is the V1 overhaul line of Diablo Oracool Edition, built on DevilutionX. Its current development branch is `renderer-32bit`; Windows is the primary development platform. V1 is single-player only, and multiplayer is not supported.

The current scope covers six classes (Paladin, Rogue, Sorcerer, Monk, Barbarian and Necromancer), class skill trees, Rage and Essence, expanded items and sets, runewords and crafting, Torment and rifts, the three-Act waypoint menu, and a redesigned interface with a 32-bit renderer. See the [root README](../../README.md) for the current feature overview and installation requirements.

Development continues. Source changes are not automatically released packages, and compatibility between development saves is not guaranteed. Original Diablo and Hellfire data remains user-supplied.

## Working rules

1. Make focused changes and verify the affected behaviour.
2. Document gameplay, compatibility and build effects.
3. Keep generated builds, original game archives, credentials and local settings out of source history.
4. Keep project documentation under `.ProjectDocumentation`; use relative paths rather than a particular computer's project root.
5. Check current source definitions when old plans or reports disagree with the implementation.

## Documentation and history

[Development reports](../02-Development-Reports) record individual changes. [The historical changelog](../04-Changelog/CHANGELOG.md), dated release notes and older design plans preserve the project’s evolution and may describe superseded behaviour. [V0 to V1 Fork](V0%20to%20V1%20Fork.md) explains the earlier split.

Feature counts and current release availability are summarized in the root README. Do not treat an old prototype default, a historical milestone or the V0 scope as a statement of current V1 support.
