# Design Decisions

Record durable technical and gameplay decisions here.

## DD-001: Preserve the upstream repository layout

The DevilutionX source already exists directly under `C:\DiabloDOE`. It remains in place to avoid breaking source and build paths.

## DD-002: Use a dedicated project library

All Oracool-specific documentation and supporting project materials are stored under `C:\DiabloDOE\_ProjectLibrary`. Its standard sections are `Documentation`, `Research`, `Screenshots`, `Builds`, and `Releases`.

## DD-003: Separate runtime branding from build identifiers

The user-facing runtime name is `Diablo Oracool Edition`. Existing CMake targets, executable names, and internal build identifiers retain their upstream DevilutionX names to minimize unnecessary build-system changes.
