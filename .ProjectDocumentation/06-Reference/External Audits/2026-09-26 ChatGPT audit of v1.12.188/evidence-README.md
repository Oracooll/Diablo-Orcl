# Audit evidence

Target: `acc970335a9c0ecd5287f236fb601f2c8d7a8601` (v1.12.188).

- `audit-v188-ctest.log`: unedited existing-suite output; 838 passed, one skipped, one disabled.
- `audit-v188-production-probes.cpp`: final external probe source; eight enabled regression tests, one explicitly disabled inconclusive workshop fixture.
- `audit-v188-build-probes.cjs`: compiles the external source using the installed Debug build's flags/import libraries. Requires MSVC and the paths used on the audit machine.
- `audit-v188-final-probes.log`: final eight-test run. All eight fail their intended assertions; no enabled test encounters SEH. Two tests cover the same Essence defect.
- `audit-v188-workshop-probes.log`: **exploratory/inconclusive** bench-return SEH plus a valid UI-predicate failure. The SEH is not a product finding and is why that fixture is disabled in the final source.
- `audit-v188-models.cjs` and `.log`: actual Node packer boundary/engine-reader checks and independent stash packing model. Running the script regenerates synthetic files under the working directory.
- `audit-v188-installed-archive.log`: engine-reader verification of 935 current archive files against the source manifest.
- `tooltip_sweep_report.md`: existing suite's generated 4,148-item report; warnings are not automatically confirmed findings.

The harness paths refer to the original audit workspace and existing build. Use writable scratch storage when reproducing and adjust machine-specific paths. No source patch, compiled game, proprietary asset or player save is included.
