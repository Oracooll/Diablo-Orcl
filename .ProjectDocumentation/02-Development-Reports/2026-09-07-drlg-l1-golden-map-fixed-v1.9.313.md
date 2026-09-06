# The level-3 golden map test, fixed: the suite is all green (v1.9.313)

**Date:** 2026-09-07. The user: "fix the drlg_l1 golden map test too."

## What it was

`Drlg_l1.CreateL5Dungeon_diablo_3_844660068` had failed every run for weeks and was carried as "the standing dungeon-generation failure". A new `DRLG_TEST_DUMP=<file>` hook in the harness writes what the generator produced in the fixture's own .dun layout; diffing that against the golden showed 1314 of 1600 tiles different and both view positions off - the whole map, not a set piece - so the generator's random stream had diverged from the start. Level 3 is the Skeleton King's level, and `LoadQuestSetPieces` loads his set piece when the quest is available, which changes everything generated after it.

The cause was one default. Upstream ships "Randomize Quests" ON, which culls one of Skeleton King / Poisoned Water (and one quest per group) from the level-16 seed before any level is built; the goldens were generated that way. This fork ships it OFF - every quest is on for good (InitQuests, v1.5.44) - so the test built level 3 with the Skeleton King available and his set piece in it. The generator itself is untouched (drlg_l1.cpp has no fork commits).

## The fix

The harness pins upstream's value (`randomizeQuests` ON) in `TestInitGame`: the goldens encode the generator, not the fork's quest default, and the fork's quest behaviour has its own tests. Not a regenerated golden: that would have hidden a real generator change behind a new fixture.

All four dungeon binaries pass (19, 8, 9, 7), the shuffle lane is 35/35, and the whole suite is 683/683 with one intentional skip - the first fully green run of the project in this record.
