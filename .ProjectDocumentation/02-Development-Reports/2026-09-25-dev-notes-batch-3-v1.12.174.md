# Third /dev batch: one towner layout, stash sort, the Nephalem clock — v1.12.174

2026-09-25

> check dev report and process.

Three notes, coded in full and built once.

## Every towner's dialog on one grid

`stores.cpp` gained `TownerTitleLine` 2, `TownerPromptLine` 9, `TownerTalkLine` 12, `TownerDoorLine` 14,
`TownerSecondLine` 16 and `TownerLeaveLine` 18, used by all eight Start* functions, their Enter
handlers and every back path that seats `stextsel` (Wirt's two tab backs, Adria's respec
`stextlhold`). `WitchShopDoorLine` is now `TownerDoorLine`. Griswold's list-driven menu keeps its
entries-run for Talk and the door, with the leave entry pinned to `TownerLeaveLine` in both the draw
(`StartSmith`) and the dispatch (`SmithEnter`, `SmithMenuLine(None)`).

Wording: "Leave <name>" for all eight (was "Leave the shop", "Leave the shack", "Leave Healer's
home", "Say goodbye" ×2, "Say Goodbye"), single-line titles (the "Welcome to the" first line dropped
from Griswold, Pepin and Ogden), Pepin's door "Enter Shop".

## Stash SORT: non-worn items after the gear

The user saw consumables ahead of gear. Signets, Sealed Maps and Guardian Keystones do not stack,
so they missed the consumables partition and were sorted as Misc gear of the Plain tier — page one,
with the white gear, before the magic/rare/unique pages. The partition now also takes any
`ItemType::Misc` that is not a charm; they land in the consumables' "anything else" family.

## A cleared Nephalem Rift ends only on its clock

`RiftNoteReturnHome` no longer marks a cleared Nephalem Rift as returned home, so walking out does
not end it and the monument portal re-enters it; `ProcessRift`'s close clock is the only end. The
in-rift portal hover reads "The rift closes in Ns". Guardian Rifts are unchanged.

Debug build and tests at the end of the batch.
