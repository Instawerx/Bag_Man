# afl_mesh_validator (AFL-0217)

AAA conformance gate for genAI mesh outputs (Tripo, Meshy) feeding the
`afl-blender-bridge` AAA-clean pipeline. Pure Python, no UE dependency,
no pip deps beyond `pytest` for development.

The validator is the contract between "generated mesh" and "shippable
asset" — it refuses anything that would blow our poly budgets, break the
asset-naming SSOT, or arrive with UV coordinates outside `[0,1]` (a
common Tripo failure mode).

## Install

No `pip install` needed for runtime. Only requires:

- Python 3.10+ (3.13 is what the studio box runs on).
- *Optional:* Blender 4.x on `PATH` to validate `.fbx` inputs. Without
  Blender the validator still handles `.gltf` and `.glb` directly.

For tests:

```powershell
pip install --user pytest
```

## Usage

```powershell
# Validate a single file (auto-detects class from filename prefix)
python -m afl_mesh_validator Path\To\SM_AFL_Crate_Kit.glb

# Force a class when auto-detection can't read the filename
python -m afl_mesh_validator weird_name.glb --class kit_piece

# Validate an entire directory and write a manifest
python -m afl_mesh_validator Content\AFL\Meshes --manifest-out report.json

# Use a non-default budgets table
python -m afl_mesh_validator asset.gltf --budgets Tools\AFL_MeshValidator\budgets.json
```

Exit codes:

| code | meaning |
| ---- | ------- |
| 0    | all assets passed |
| 1    | one or more assets failed a check |
| 2    | invalid input or `blender` missing for `.fbx` input |

## What gets checked

| check         | scope                       | pass criterion |
| ------------- | --------------------------- | -------------- |
| POLY_BUDGET   | all classes                 | `sum(indices.count / 3)` <= class LOD budget (LOD tier inferred from `_LOD<N>` filename suffix) |
| NAMING        | all classes                 | filename stem matches the per-class regex in `budgets.json` |
| UV_COVERAGE   | all classes                 | at least one primitive has `TEXCOORD_0`; UV accessor `min`/`max` inside `[0,1]` |
| MAT_SLOTS     | all classes                 | distinct material indices across primitives <= class budget |
| SOCKET_SNIFF  | `hero_char` only (warn-only) | reports presence of the canonical AFL hero sockets (`head`, `hand_l`, `hand_r`, `foot_l`, `foot_r`, `weapon_socket`) without failing the asset |

`SOCKET_SNIFF` is deliberately WARN-level — generated mesh outputs
rarely include rigging data; the gate exists to flag missing sockets
for the rigger (AFL-0216 follow-up) rather than reject the mesh.

## Budget table

Mirrors master doc §16.2 (lines 2105-2114). Single source of truth lives
in `budgets.json`. Reproduced here for quick reference:

| class          | LOD0 tris | LOD3 tris | tex max | mat slots |
| -------------- | --------- | --------- | ------- | --------- |
| `hero_char`    | 80000     | 8000      | 4096    | 8         |
| `heavy_weapon` | 25000     | 3000      | 2048    | 4         |
| `env_prop`     | 15000     | 1500      | 2048    | 4         |
| `kit_piece`    | 5000      | 500       | 1024    | 2         |
| `decal`        | 200       | (none)    | 1024    | 1         |

Texture-size enforcement (`tex_max`) is recorded in the budget table for
parity with the master doc, but the current validator does not load
texture binaries — that's a follow-up once `afl_reimport.py` lands and
we have a place to fetch the source PNGs.

## Class detection

Detection rules live in `budgets.json` under `detection_rules`. They run
in order, first match wins, against the filename stem with any
`_LOD<0..3>` suffix stripped:

| stem matches             | class          |
| ------------------------ | -------------- |
| `^SKM_AFL_.*Hero`        | `hero_char`    |
| `^SM_AFL_.*Weapon`       | `heavy_weapon` |
| `^SM_AFL_.*Decal`        | `decal`        |
| `^SM_AFL_.*Kit`          | `kit_piece`    |
| `^SM_AFL_`               | `env_prop`     (fallback) |

When auto-detection can't classify a file, pass `--class` explicitly
or rename the file to the canonical AFL convention.

## Test fixtures

`tests/fixtures/` ships three hand-authored glTF JSON files (each under
5KB, no real geometry — just enough accessor / mesh structure to
exercise each check):

- `SM_AFL_Crate_Kit.gltf` — passing kit piece (600 tris, in-budget).
- `SKM_AFL_Pilot_Hero.gltf` — hero mesh authored at 90k tris to fail POLY_BUDGET.
- `SM_AFL_thing_Kit_extra_bad_segments.gltf` — kit-classified by prefix but fails NAMING.

Run them with:

```powershell
cd Tools\AFL_MeshValidator
pytest tests
```

## Manifest format

```jsonc
{
  "version": 1,
  "timestamp": "2026-05-22T19:12:33Z",
  "assets": [
    {
      "path": "Content/AFL/Meshes/SM_AFL_Crate_Kit.glb",
      "class": "kit_piece",
      "pass": true,
      "checks": [
        { "name": "POLY_BUDGET", "pass": true, "detail": "600/5000 LOD0 tris" },
        { "name": "NAMING", "pass": true, "detail": "..." }
        /* ... */
      ],
      "reasons": []
    }
  ]
}
```

## TODO / §16.2 follow-ups

- **DT_AFL_MeshBudgets.uasset** — UE-side DataTable wrapper around
  `budgets.json` so designers can edit budgets without leaving the
  editor. Authoring the .uasset is a separate UE-side ticket; this tool
  is the headless source of truth in the meantime.
- **AFL-0216 (a/b/c)** — `afl-blender-bridge` integration. The bridge
  will call this validator before any auto-import, surfacing failures
  back to the asset's source in Blender.
- **`afl_reimport.py`** — bulk reimport pipeline. Will gate on this
  validator and additionally enforce `tex_max` once it has the actual
  PNG paths.
- **CI integration** — Standalone for now. A future PR can wire this
  into the GitHub Actions workflow as a gate on any `Content/AFL/Meshes/**`
  diff.
