---
title: V0 to V1 Fork
tags: [overview]
summary: Why Diablo Oracool Edition split into a vanilla-faithful V0 line and a UI-overhaul V1 line, and what each owns going forward.
---

# V0 to V1 Fork

## The split

Diablo Oracool Edition was originally one project (`Diablo Ocrl V0`), a DevilutionX-based quality-of-life mod that stayed faithful to vanilla Diablo's look and behavior. On 2026-08-08, the project split into two independently versioned lines:

- **V0** (`Diablo Ocrl V0`, `0.x.xxx`) — stays vanilla-faithful. QoL and bugfix features only; no departure from vanilla's UI, resolution behavior, or visual identity.
- **V1** (`Diablo Orcl V1`, `1.x.xxx`, this repository) — where deliberately vanilla-*departing* work lives, starting with a 960×720-floor UI overhaul.

## How the fork happened

V1 was branched from V0 at commit `f2a347e` ("Add CEL decoder and MPQ extractor tools for the UI-overhaul asset pipeline"), via commit `40a1ce0` ("Start Oracool Edition V1 - the 960x720 UI-overhaul line (v1.0.0)"), which only bumped the `ORACOOL_VERSION` file from V0's line to `1.0.0` — no source changes.

Shortly after the fork point, V0 committed `264c72b` ("Revert Furious Charge and curated resolution list; course-correct to vanilla-faithful (v0.5.0)"), which removed two features from V0 that had briefly existed there: the Furious Charge skill rework, and a curated/floored resolution list (960×720 minimum, replacing SDL's raw display-mode enumeration). That revert commit's message explicitly states the intent: those UI/gameplay-departing features belong in the new V1 fork instead, not in V0.

**Practical consequence:** because V1 branched *before* that revert, V1's `Source/options.cpp` already contained the curated resolution list in full — it was never actually missing from V1, just unverified. See [[2026-08-09 - Resolution List Investigation]] for how this was confirmed.

## What each line owns

| | V0 | V1 |
|---|---|---|
| Version line | `0.x.xxx` | `1.x.xxx` |
| Visual identity | Vanilla Diablo | UI overhaul, starting at 960×720 |
| Furious Charge rework | Removed (reverted) | N/A — never carried over, not currently planned |
| Curated resolution list | Removed (reverted) | Present, see [[2026-08-09 - Resolution List Sort Order]] |
| Remote | `Diablo-Oracool-Edition` on GitHub | None configured — local-only as of the fork |

## Related

- [[2026-08-08 - Fork to Oracool Edition V1]]
- [[2026-08-09 - Resolution List Investigation]]
- [[Project-Scope]]
