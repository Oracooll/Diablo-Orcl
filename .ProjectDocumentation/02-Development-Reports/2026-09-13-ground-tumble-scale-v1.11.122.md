# Fork ground tumbles drawn at a chosen scale per sheet

2026-09-13 — v1.11.122

## Why

> "we need to reduce size of ground assets of oracool items. compare px sizes of vanilla ground assets and
> compare to oracool ground assets. give me a comparison artifact and lets decide per each asset."

Measured on the resting frame (the one an item lies on), the fork's 21 sheets cover two to fifteen times the pixel
area of the vanilla drop nearest their shape — gloves 40×29 against the helm's 13×15, the amulet 52×23 against
the ring's 7×7, the War Lute 48×41 against the mace's 35×16.

## The decision

A comparison page — *Ground Tumble Scale* — set each fork sheet beside its vanilla analogue at 3× on the game's
floor grey, with a per-sheet picker over 40 / 50 / 60 / 70 / 75 / 80 / 90 / 100 % and a suggestion (area ≈ 1.4×
the analogue). The user picked nine; the other twelve rows previewed the suggestion and were left there, so the
suggestion is what is applied for them.

| Sheet | Scale | | Sheet | Scale | |
|---|---|---|---|---|---|
| amuletflip | **40** | picked | gemflip | 90 | suggested |
| signetflip | **50** | picked | runeflip | 90 | suggested |
| luteflip | **50** | picked | salvageflip | 90 | suggested |
| beltflip | **60** | picked | jewelflip | 80 | suggested |
| cloakflip | **60** | picked | mapflip | 75 | suggested |
| focusflip | **60** | picked | bootflip | 70 | suggested |
| legflip | **60** | picked | orbflip | 60 | suggested |
| relicflip | **60** | picked | gloveflip | 60 | suggested |
| spearflip | **60** | picked | bracerflip | 60 | suggested |
| | | | shoulderflip | 60 | suggested |
| | | | quiverflip | 50 | suggested |
| | | | charmflip | 100 | suggested |

## How

- `OracoolDropAnimScale` (`items.cpp`): one entry per fork tumble, indexed from `FirstOracoolDropAnim`, with a
  `static_assert` tying its size to `ITEMTYPES` so a new sheet cannot ship without a scale.
- `InitItemGFX`: a fork PNG sheet that loads with its expected frame count is scaled once, with
  `oracool::ScaleClxList` — the engine's nearest-neighbour scaler, so the art stays hard-edged.
- **Frame counts are untouched** (13 each), so a saved item's frame number stays valid; no migration.
- Vanilla sheets, and the `larmor` fallback a fork tumble borrows when its PNG is absent, stay at 100 %.
- Everything that reads the sprites — the drop, the resting frame, the label's centring, the hover outline —
  reads the scaled frames, because they replace the originals in `itemanims`.

One visual consequence worth knowing: each sheet's cell scales too, so the empty rows under the resting item
shrink with it and a smaller item sits a few pixels lower on its tile than a 100 % one would.

## Tests

`OracoolAudit.ForkTumblesAreDrawnAtTheirChosenScale` — the nine picks by sheet name, vanilla sheets at 100 %, and
every scale inside the page's range.

## For the user to look at

Any floor with fork drops: amulets, lutes, belts and the worn gear noticeably smaller, sitting closer to the size
of vanilla drops. Say which of the twelve suggested scales to change and it is a one-number edit each.
