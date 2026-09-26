# RfA-30: the last four skill effects — v1.12.186

2026-09-26

> "put a 2min monitor for rfa30 and process when found"

The watch found `batch-60-four-effects` and its delivery note. Checked independently:

| Sheet | Size | Colours | Identical frames | Binary alpha | Ends empty |
|---|---|---|---|---|---|
| army_of_the_dead | 160x128 x12 | 11 | 0 | yes | yes |
| bone_prison | 96x128 x12 | 11 | 0 | yes | yes |
| wave_of_light | 96x160 x12 | 7 | 0 | yes | yes |
| ancestral_court | 192x128 x12 | 7 | 0 | yes | yes |

Anchors match RfA-30's table and are given this time. By eye they now show what they are: hands and ribcages clawing
out of the ground; a cage of bone spikes rising and crumbling; a golden bell dropping with a ring of light; three
armoured shades striking inward. Same procedural style as RfA-29's accepted 86.

All four placed in `oracool_assets/missiles`; the code was already wired (fail-soft), so no code changed. All 90 of
RfA-27's effect slots now have art. Build v1.12.186 packed oracool.mpq with 935 files; the skill asset tests pass.
