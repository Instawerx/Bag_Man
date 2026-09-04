# IRONICS release ledger

This is a **release ledger**, not a distribution store. The multi-GB build artifacts (the `.zip`,
paks, exe) live on `D:\BagMan\releases\...` and, once uploaded, in the private S3 releases bucket
(BUILD_PRUNE_DEPLOY_PLAN.md §11). Only the small immutable records are committed here:

- `manifest.json` -- version, size, sha256, source commit, engine, provenance verdict.
- `IRONICS-<semver>-Win64.zip.sha256` -- integrity hash.

Every release MUST pass `Tools/verify_ship_provenance.ps1` (BPD §11 step 0; `Docs/ENGINE_DOCTRINE.md` §3)
before packaging/upload. The `.gitignore` here blocks the large artifacts from ever being committed.

`0.1.0-beta` is a PLACEHOLDER version pending the operator's real release semver.
