# Contributing to Diablo Orcl

Diablo Orcl is a development fork of DevilutionX. Target the default `renderer-32bit` branch, and use [this project's issues](https://github.com/Oracooll/Diablo-Orcl/issues) for mod bugs and proposals. V1 is single-player only; do not assume upstream multiplayer or platform support applies to this fork.

## Build and review

The CMake build requests C++20. Use [the root README](../README.md#building-from-source) and the checked-in build configuration to set up a toolchain; Windows configuration paths need adapting to your machine. Keep changes focused, describe their player-facing effect, and verify affected behaviour and relevant tests. Report save-format or asset dependencies explicitly.

Do not commit original Diablo/Hellfire game archives, credentials, local settings, generated builds or save files. Respect the [license](../LICENSE.md) and existing dependency and asset notices.

## Style and documentation

Follow the repository's formatting and surrounding C++ conventions. The [upstream style guide](https://github.com/diasurgical/devilutionX/wiki/Code-Style) provides background. Record mod development and compatibility changes under [`.ProjectDocumentation`](../.ProjectDocumentation). Preserve dated reports as history and update current documentation when behaviour changes.
