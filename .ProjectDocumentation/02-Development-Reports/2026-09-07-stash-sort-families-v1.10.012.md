# Stash SORT: consumables by family, each family on rows of its own (v1.10.012)

**Date:** 2026-09-07
**Request:** "still issues with books sorting in stash" (with a screenshot of the materials page: a book at the top-left, potions, more books wrapped into rows 2 and 3).

## The cause

Books are stackable consumables, so they go to the consumables layout on the materials page, not the ordinary sort the 2026-09-07 morning fix touched. That layout ordered kinds by misc id, in which IMISC_BOOK is one more number after the rejuvenation potions, and then filled free cells in row order with no regard to where a kind ended - so the books sat in the middle of the potion run and wrapped across rows with everything else.

## The fix

The consumables are laid out by FAMILY, and every family starts on a fresh row: potions (belt order), elixirs, scrolls by spell, books by spell, oils, the Hellfire trap runes, anything else. A family is a visual block now, which is what a sorted page is for.

## Tests

A mixed deposit of three books and three potions: after SORT the books share one row, that row is below the potions', and nothing else is on it. Suite 691/691.
