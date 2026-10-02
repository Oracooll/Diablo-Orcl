---
title: Diablo Oracool Edition V1 — Documentation Home
tags: [moc, home]
---

# Diablo Oracool Edition V1 — Project Documentation

This is the documentation vault for **Diablo Orcl**, the class, loot, progression and 32-bit interface overhaul line of Diablo Oracool Edition. It is a fork of Diablo Oracool Edition V0 (which continues on its own vanilla-faithful, quality-of-life-only path) — see [[V0 to V1 Fork]] for the full story.

Open this folder (`.ProjectDocumentation/`) as an Obsidian vault to get backlinks, the graph view, and clickable `[[wikilinks]]` between notes.

If you're new here, read in this order: [[About This Documentation]] → [[V0 to V1 Fork]] → [[Project-Scope]] → the latest entries in [[#Development Reports]] below.

## Project Overview

- [[Project-Scope]] — what this project is and isn't
- [[Feature-Catalogue]] — features shipped so far
- [[Design-Decisions]] — standing design choices and the reasoning behind them
- [[V0 to V1 Fork]] — why the project split into two version lines, and what each one owns

## Current development

The default branch is `renderer-32bit`. See [the current README](../README.md) and [development reports](02-Development-Reports) for current status. Latest reviewed source: v1.12.347, 2 October 2026. V1 is single-player only. The older report index below is historical and is not a complete current report list.

## Development Reports

Dated, atomic reports — one per unit of work — written as the work happens. This is the primary "what happened and why" record for a third party auditing the project's history. Historical index (newer reports are in the folder linked above):

- [[2026-08-11 - Panel Darkening and HUD Recolour]]
- [[2026-08-11 - Asset Studio]]
- [[2026-08-11 - Save On Exit]]
- [[2026-08-11 - Inventory Window Live]]
- [[2026-08-11 - Inventory Panel Geometry]]
- [[2026-08-11 - Burger Menu Icon Row]]
- [[2026-08-11 - Skills Deferred, HUD Click-Through and Tuning]]
- [[2026-08-11 - HUD Follow-ups and Town Portal as Built-In Ability]]
- [[2026-08-11 - Fixed Mini-Map Frame Size]]
- [[2026-08-10 - Orb Art with Sphere Drain Effect]]
- [[2026-08-10 - Compact Middle HUD Redesign]]
- [[2026-08-10 - Middle HUD Art Asset Pipeline]]
- [[2026-08-10 - HUD Position Tuning from First Play-Test]]
- [[2026-08-10 - HUD Overhaul Phase 1 - Structural Skeleton]]
- [[2026-08-10 - Waypoint System Complete]]
- [[2026-08-10 - All 16 Dungeon Waypoints and givewp Debug Command]]
- [[2026-08-10 - Scoped Black Flash Fix to Dungeon Only]]
- [[2026-08-10 - Revert Lighting Force-Recompute - Corrupted Town]]
- [[2026-08-10 - Waypoint Unlock Survives New Game]]
- [[2026-08-10 - Waypoint Spawn Black Flash Fix]]
- [[2026-08-10 - Waypoint Crash Root Cause and Spawn Timing Fix]]
- [[2026-08-10 - Waypoint Spawn Crash - Move Reposition Off the Load Path]]
- [[2026-08-10 - Stale Waypoint Spawn Flag Bug]]
- [[2026-08-10 - Persisted Per-Difficulty Waypoint Unlock Table]]
- [[2026-08-10 - Waypoint Spawn Position and Town Object Persistence]]
- [[2026-08-09 - ClearUI Debug Command]]
- [[2026-08-09 - Mini-Map Waypoint and Portal Markers]]
- [[2026-08-09 - Waypoint Unlock State and Real Warping]]
- [[2026-08-09 - Give Rare, Unique, and Primal Debug Commands]]
- [[2026-08-09 - Autosave on Waypoint Activation]]
- [[2026-08-09 - Autosave Cheat Command and Wider Event Coverage]]
- [[2026-08-09 - Autosave Trigger Coverage Gaps]]
- [[2026-08-09 - Remove Auto Save Item Delay]]
- [[2026-08-09 - Quest Log Reveal All]]
- [[2026-08-09 - Character-Only Persistence, No Continue]]
- [[2026-08-09 - Autosave-Only Play, Part 1]]
- [[2026-08-09 - Rename Warrior to Paladin]]
- [[2026-08-09 - Idea Backlog Reordered by Difficulty]]
- [[2026-08-09 - Town, Shops, Classes, and Save-Load Backlog Additions]]
- [[2026-08-09 - Diablo 2 and 3 Mechanics Brainstorm]]
- [[2026-08-09 - Project Documentation Overhaul]]
- [[2026-08-09 - Version Bump on Rebuild Policy]]
- [[2026-08-09 - Fit to Screen Duplicate Resolution Cleanup]]
- [[2026-08-09 - Resolution List Sort Order]]
- [[2026-08-09 - Resolution List Investigation]]
- [[2026-08-09 - Rename Executable to DiabloOrcl]]
- [[2026-08-08 - Fork to Oracool Edition V1]]

See [[Development Report Template]] for the format new reports should follow.

## Releases

Player-facing release records, inherited from the V0 line up through the fork point:

[[v0.1.0]] · [[v0.1.0-Player-README]] · [[v0.2.0]] · [[v0.2.0-Player-README]] · [[v0.2.1]] · [[v0.2.6]] · [[v0.2.8]] · [[v0.2.10]] · [[v0.3.0]] · [[v0.4.0]]

V1 has not yet cut a release under its own `1.x.xxx` version line — see [[V0 to V1 Fork]] for the versioning split.

## Changelog

- [[CHANGELOG]] — full feature-by-feature changelog inherited from V0 (covers `v0.1.0` through the fork point; V1-only changes are tracked instead in [[#Development Reports]] until a V1 changelog format is agreed)

## Gameplay & Features

- [[Gameplay-Changes]] — the detailed, feature-by-feature gameplay change log inherited from V0
- [[Migration-Plan-1.5.4-to-1.5.5]] — the DevilutionX engine base migration this project was built on top of

## Reference

- [[Build-Instructions]]
- [[Testing-Guide]]
- [[Future-Development-Roadmap]] (companion PDF alongside it)

## Backlog

- [[Idea-Backlog]]

## Implementation Reports

- [[Oracool-Edition-v0.1.0-Implementation-Report]] (companion PDF alongside it)

## Meta

- [[About This Documentation]] — structure, conventions, and how to keep this vault up to date
