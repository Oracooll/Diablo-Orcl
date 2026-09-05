# Runewords, the Second Half (v1.9.276)

**Date:** 2026-09-05 · **Request:** "show each rune's socket effect under the word in the book. also add more additional affixes. probe for suggestions from the internet. inspire yourself by diablo 2 vanilla and by runewords in d2 mods."

## What was true

Every one of the 370 words carried exactly three of its definition's eight fields, filled by one template per host family. The runes' own socket effects applied on top but the book never showed them.

## The book

Under each word's bonus lines the book now lists every rune's socket effect for that word's host ("Shael: faster attack", "Amn: life steal"), in the runes' orange, through the same `GemSocketLine` the item panel uses. Entry heights grow with the lines.

## The definition

`RunewordDefinition` gained a second half: special-effect flags (attack speed, life and mana steal, knockback, thorns, triple demon damage, fast block, hit recovery), the four attributes, magic and gold find, flat damage reduction, light radius and single resistances. `ApplyRunewordToTotals` maps them through the same totals the socket effects use, so nothing new was invented on the player side. One describer, `RunewordBonusLines`, feeds both the item panel and the book.

## The numbers, and where they came from

The research: Blizzard's original Arreat Summit runeword list and a Diablo II wiki's full list for the 1.10 words, plus Median XL's runeword guide for how far a mod goes. Median XL's words are class-locked procs and synergies this engine has no channels for, so they were read as inspiration only; nothing of theirs was copied. Project Diablo 2's wiki refused the fetch.

On legality: a word's name and its recipe are game facts and this table already used D2's; a mod's authored stat lines are the mod's, and reproducing them wholesale is neither needed nor done. What the generator carries is a SIGNATURE table for the 60 D2-named words, translating what each word was for into this engine's channels with this fork's own numbers - Steel's attack speed, Nadir's -33% gold, Rhyme's gold and magic find, Lionheart's four attributes, Enigma's strength, life and magic find, Grief's flat damage - and two extras for every other word, drawn from a pool by host family and the word's deepest rune, scaled with the word's depth. The "% chance to cast", "crushing blow" and aura-granting lines have no channel here and became flat or percentage damage, as the runes' own effects already had.

| Extra | Words carrying it |
|---|---|
| any flag | 141 of 370 |
| faster attack / fast attack | 35 / 7 |
| faster hit recovery | 47 |
| thorns | 23 |
| life steal / mana steal | 22 / 9 |
| knockback | 13 |
| triple demon damage | 12 |
| fast block | 11 |

## Verification

Debug build clean; 625/626 with the standing `Drlg_l1` failure. `tools/GenRunewords.ps1` regenerated the table (370 words). In the book, Steel reads "Damage +3%, Damage +1, To Hit +2%, Light Radius +1, Faster attack" and then "Tir: ..., El: ..." underneath.

## v1.9.277: the matrix carries the words

"update the hover matrix artifact with the new runeword lines." `DiabloOrcl.exe --runeword-lines <file>` writes every word's name, host, recipe, bonus lines and rune lines as the book and the item panel print them (the same `RunewordBonusLines` and `GemSocketLine`); `tools/GenerateHoverMatrix.pl` takes it as its third argument and adds a Runewords section, 370 rows by host. The dump is kept at `.ProjectDocumentation/01-Project-Overview/runeword-lines.tsv`.
