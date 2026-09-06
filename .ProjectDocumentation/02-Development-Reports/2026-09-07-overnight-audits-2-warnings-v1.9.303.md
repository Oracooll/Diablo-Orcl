# Overnight audits, batch 2: compiler warnings in the Oracool sources (v1.9.303)

**Date:** 2026-09-07

Every object under Source/oracool was deleted and recompiled with the output logged. Five warnings, all one shape: `paladin_ranged.cpp` passed `player.getId()` (size_t) straight into `AddMissile`'s `int id`. Cast as every other caller does (`static_cast<int>(player.getId())`). The Oracool sources are warning-free now.

Also: the Testing Guide named the old in-tree Debug folder; it names `C:\Diablo Orcl\x64-Debug` now.

Suite 635/636, the standing dungeon-generation failure only.
