# Persistence and options audit: v1.9.122

**Audited commit:** 2c3e8d15d0fdc475562024bfc9b2feee9eb8852a  
**Date:** 2026-08-30

## PO-01 — High — The canonical Oracool serializer silently deletes six registered options

### Observation

The Oracool category has a custom SaveOptions path so its INI section can be grouped and heavily
commented. That path skips the generic serializer, deletes the complete “Oracool Edition” section,
and reconstructs it by hand. Six entries returned by OracoolOptions::GetEntries are never written
back.

This includes the two v1.9.118 slots whose entire purpose is to remember the last LMB/RMB choices
across characters and game launches.

### Static comparison result

The audit compared member names in OracoolOptions::GetEntries with sgOptions.Oracool members used by
SaveOptions:

~~~text
registered entries: 67
members referenced by the manual Oracool serializer: 62

registered but not serialized:
  dungeonZoomLevel
  gameSpeedReadout
  lastReadiedSpellLeft
  lastReadiedSpellRight
  panelDocking
  vendorTieredStockChance

serializer-only non-entry:
  refreshUntilItemNames
~~~

refreshUntilItemNames is an intentional character buffer rather than an OptionEntry. It does not
explain any of the six missing registered entries.

### Evidence

- Source/options.cpp:359-365 loads every category by iterating GetEntries.
- Source/options.cpp:392-397 deliberately skips the entire Oracool category in the generic
  SaveToIni loop.
- Source/options.cpp:414-419 deletes the complete existing “Oracool Edition” section.
- Source/options.cpp:420-572 manually writes the canonical section but never writes the six names
  above.
- Source/options.cpp:1452-1453 declares Last Readied Spell Left and Right.
- Source/options.cpp:1470-1474 declares Panel Docking.
- Source/options.cpp:1498 declares Vendor Tiered Stock Chance.
- Source/options.cpp:1502-1507 declares Game Speed Readout.
- Source/options.cpp:1515 declares Dungeon Zoom Level.
- Source/options.cpp:1536-1588 includes all six in GetEntries.
- Source/diablo.cpp:3201 loads options, then Source/diablo.cpp:3206 and 3215 calls SaveOptions during
  startup. Therefore the destructive rewrite happens on every launch, not only after a user opens
  Settings.

### User-visible behavior

For Panel Docking, Vendor Tiered Stock Chance, Game Speed Readout, and Dungeon Zoom Level:

1. A non-default INI value is loaded into memory on launch A.
2. Startup SaveOptions deletes that key without replacing it.
3. The non-default value can appear to work during launch A because the in-memory value survived.
4. On launch B the key is gone, so the constructor default returns.

For Last Readied Spell Left/Right:

1. RememberReadiedSpells updates only the in-memory OptionEntry values at
   Source/oracool/readied_spells.cpp:65-73.
2. No assignment-time SaveOptions call writes them.
3. Even when SaveOptions later runs, the manual serializer omits them.
4. They can affect a character created in the same process, but do not reliably survive restart.

This is a silent preference-loss bug. There is no error message because the INI rewrite succeeds.

### Recommended repair

Preferred long-term shape:

- Stop maintaining a second hand-written list of Oracool members.
- Let every registered OptionEntry serialize through one path.
- If canonical order and comments are required, attach section/group/comment metadata to the
  entries or generate the canonical writer from a single descriptor table used by GetEntries.

Minimal safe fix:

- Add explicit writes for all six missing entries before SaveIni.
- Ensure remembered readied spells reach disk at a defined point. Either persist the two entries
  when RememberReadiedSpells changes them, or add a guaranteed clean-shutdown SaveOptions call and
  document crash semantics.

### Regression tests

Add a temp-config round-trip test:

1. Set every Oracool OptionEntry to a non-default valid value.
2. SaveOptions.
3. Reset Options to constructor defaults.
4. LoadOptions.
5. Assert all 67 registered entries round-trip.

Also parse the resulting section and assert that every registered key appears exactly once. This
would catch both omissions and duplicate manual keys. Add a focused two-process-style test for the
remembered LMB/RMB pair.

## PO-02 — Medium — Balance Telemetry is an unreachable, permanently enabled “option”

### Observation

Balance Telemetry is declared as a normal visible OptionEntryBoolean and is consumed by the
telemetry writer. It is absent from OracoolOptions::GetEntries and absent from the manual
serializer.

GetEntries is used for loading and for the settings category. Consequently:

- the setting is not shown in Settings;
- an INI key with that name is never loaded;
- SaveOptions never writes it;
- the constructor default true is the only value the running game can obtain.

The description promises a local-only CSV, but the player has no working control over whether that
CSV is produced.

### Evidence

- Source/options.cpp:1497 declares Balance Telemetry with default true and OptionEntryFlags::None.
- Source/options.h:905 stores the member.
- Source/options.cpp:1519-1589 returns the category's registered entries; balanceTelemetry is not
  in the vector.
- Source/options.cpp:420-572 does not manually serialize “Balance Telemetry.”
- Source/oracool/telemetry.cpp:31 returns the current balanceTelemetry value to gate telemetry.
- Source/oracool/telemetry.h documents the option as the feature gate.

### Recommended repair

Add balanceTelemetry to GetEntries and to the canonical serializer, then decide whether default-on
is still desired. Add a test that:

- confirms it appears in the Oracool category;
- loads a false value;
- confirms telemetry is disabled;
- saves and reloads false without losing it.

## Root cause and prevention

Both findings share one root cause: the set of declared options, the set returned by GetEntries,
and the set manually serialized are independent lists.

A compile-time or unit-test invariant should compare these sets. The strongest repair is to make
one descriptor list generate all three so drift is structurally impossible.

## Lifecycle concern investigated and ruled out

The new remembered-spell application is not erased permanently by InitPlayer:

- Source/player.cpp:2515-2520 applies remembered values during CreatePlayer.
- Source/pack.cpp:247-248 packs the two button spells into the new hero.
- Source/pack.cpp:512-513 unpacks them after InitPlayer has established defaults.
- Source/pfile.cpp:938-950 creates, packs, and commits the new hero record.

The actual failure boundary is the option-file round trip described above.
