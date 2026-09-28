# 2026-09-28 - The Paladin's holy bursts (v1.12.215)

**Date:** 2026-09-28. Debug only. The user: "check my paladin comments on Diablo Orcl Visual FX Schedule. Apply in game and replace animations in artefact with what i have chosen."

## The comments

The page's db collection `comments` held six. Each one asks for vanilla's Holy Bolt explosion (holyexpl) in place of a ChatGPT sheet:

| Card | Size | Tint | Where it draws |
|---|---|---|---|
| Aegis Slam | 50% | gold | on what the shield struck (was an arc along the facing) |
| Crusade Sweep | 50% | gold | on the Paladin: the sweep is all round him |
| Heaven's Descent | 100% | gold | where he lands |
| Judgment Strike | 50% | blue | on the struck body |
| Oathbrand Strike | 50% | blue | on the struck body |
| Votive Strike | 50% | red | on the struck body. Already done in v1.12.214 as infrared red, Rgb(255, 56, 32), so left as it is. |

## Code

`oracool/rfa12_actives.cpp`: v1.12.214's `VotiveBurst` became `HolyBurst(player, tile, half, rgb)`.
- **How it draws:** it places holyexpl through `Art`, tints it with `Tint::Hue`, and for half size swaps in the cached 50% list (`ScaleClxList`), lifted so its centre stays where the full burst's is.
- **Return value:** false when holyexpl is not loaded. Heaven's Descent then keeps its ring fallback.
- **Colours:**
  - `BurstGold` is the Paladin's gold, Rgb(244, 204, 96), the same as `RingHueForClass`.
  - `BurstBlue` is Rgb(96, 150, 255).
  - `BurstInfrared` is Rgb(255, 56, 32).
- **Callers:** `SwingArt` calls it for Aegis Slam, Crusade, Votive Strike, Judgment and Oathbrand; `TickLanding` for Heaven's Descent.

The five ChatGPT sheets stay in misdat and the archive but are no longer drawn.

## The page

- **Exporter:** `DISABLED_ExportVisualFx` renders the four bursts (gold 50/100, blue 50, red 50) through the game's `TintedTable` and `ScaleClxList`. All four share the full burst's cell, so a half burst reads as half.
- **The six cards:** they show the bursts and keep their keys, so the comments stay attached. The ChatGPT sheets are on the page's retired list.

## Test

Debug build and ctest: 878/878 passed.
