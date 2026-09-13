# RfA-15: aura rings for the new auras and songs

2026-09-13 — no build (request for assets only)

## Why

> "you need to issue an rfa for assets for new auras we have introduced. they dont seem to have rings under the hero."

## Audit

A ring is drawn when `oracool/aura_ground.cpp`'s `AuraFiles` table has a row for the lit skill **and**
`ui\aura_<id>.png` loads. A missing file is only logged at verbose level ("that aura burns without a ring"),
so nothing warned.

- `Kind::Aura` rows in `class_tree.cpp`: **57**.
- `AuraFiles` rows and ring PNGs in `Packaging/resources/oracool_assets/ui/`: **30** (20 Paladin, 9 Bard, Healing Mantra).
- Without a ring: **27**, every one from RfA-12 (v1.11.108–112):
  - **16 Paladin auras**: Valor, Radiance, Bane of Evil, Condemnation, Tithe of Ash, Retaliation, Doom Procession,
    Dominion, Steadfast, Resist Magic, Immovable, Warding Light, Mercy, Aura of Protection, Endurance, Sanctity.
  - **11 Bard songs**: Minstrel's Tune, Ballad of Resilience, Hunter's Chant, Serenade of Steel, Song of Plenty,
    Nocturne, Anthem of Valor, Siren's Call, Hymn of Renewal, Symphony of War, Sovereign Measure.
- The Monk's RfA-12 mantras are timed actives, not auras, so they need no ring.

## The request

`Resources/ChatGPT RfA/RfA-15 - Aura Rings for the New Auras and Songs.md`, delivered as `batch-33-aura-rings`.

- 27 rings at 512×256 with soft alpha, the same format as batch 8 and the Paladin brief.
- Each ring has its file id, a description of what the skill does, the colour ramps allowed and a suggested motif.
- The green ramp is forbidden, because the palette no longer holds green.

`RfA-15-reference/` holds the Paladin brief, the 30 existing rings and batch 8's notes as an example.

## On delivery

1. Add 27 `AuraFiles` rows (30 → 57).
2. Copy the files to Packaging, then repack.
3. Add ledger rows.
4. Add an audit test that every `Kind::Aura` row has a file, so an aura added later cannot lose its ring silently again.
