# Session scripts (kept so a new machine has them)

These lived in Claude Code's per-session scratchpad, which does not move with the repository. Copied here on 2026-09-18 for the move to another computer.

| Folder | What | How it is used |
|---|---|---|
| census/ | rows.js, gen_all_heroes.js, all_heroes.template.html | Regenerates the Orcl Skill Census page from Source/oracool/class_tree.cpp: `node rows.js <class_tree.cpp> rows.json` then `node gen_all_heroes.js rows.json out.html <version>`; publish the page with the Artifact tool using the census artifact's URL. |
| necromancer/ | necro_rows.js (the 72 rows' source), necro_rfa17.js (writes the RfA-17 draft), road-to-necromancy.html + patch_ledger.js (the Road to Necromancy page) | The Road page is republished with its URL; its data (decisions, phases, notes, skills) lives in the artifact's own db, not in the file. |
| roadmap/ | orcl-roadmap.html, seed.js | The Orcl Roadmap page; ideas live in its db (collection `ideas`). Do not reseed. |

The artifact URLs are in Claude's memory notes (reference_*_artifact.md) and in .ProjectDocumentation.
