# HUB-MAP-CENSUS — Military Base Megapack (AFL-3401)

**Source:** `Military Base Megapack 5.0.zip` — operator's Google Drive (8,667,828,666 B ≈ 8.07 GiB),
downloaded 2026-08-27. The Fab library did not contain the pack; Drive is the canonical source.
The zip is a complete **UE 5.0-era project** (`Military_Base/`, configs dated 2022-05/06); only its
`Content/US_Military/` subtree was ingested — 1,750 files, 8,533 MB on disk.
⚠ Tooling note: Info-ZIP `unzip` silently extracts NOTHING from this zip64 archive (exit 0, dir entry
only) — `tar.exe` (bsdtar) extracted it correctly; counts verified against the manifest.
**Ingest placement:** `Content/US_Military/` — the pack's authored root, so every internal
`/Game/US_Military/...` package reference resolves unchanged. Migration to
`Plugins/GameFeatures/AFLHub/Content/MegaBase/` is the next step of AFL-3401 (in-editor move +
redirector fixup; the disk placement is deliberately at the authored path first).

## Demo maps (`World` ×35)
| Map | Role |
|---|---|
| **`Levels/Showreel_Scene`** (14.4 MB) | **The base scene — the `L_AFL_OutpostEarth` source (proposed; operator confirms at M3)** |
| `Levels/Overview` | Asset-overview grid (the pack's own default map) |
| `Levels/Vehicles_Demo_Scene`, `Vehicles_Drivable_Demo` | Vehicle demos — reference only |
| `Levels/SubLevels/Base_Lighting`, `Main_Lighting`, `Camera01–25_Lighting*` (~31) | **Streaming sublevels** — lighting + per-camera set-dressing |

- **Map type: persistent level + streaming sublevels (Level Streaming, NOT World Partition)** —
  5.0-era authoring. §6.4 streaming decision starts from measured load/memory, not WP conversion.
- **Lighting: BAKED** — `MapBuildDataRegistry` ×27 (per-level built lighting ships with the pack).
  §6.6 default = keep the baked scenario unless Lumen-era relight is measured necessary.
- Landscape-based terrain: `LandscapeLayerInfoObject` ×4, `FoliageType_InstancedStaticMesh` ×7.

## Asset census (asset registry, `/Game/US_Military`, total = 1,750)
| Class | Count | | Class | Count |
|---|---|---|---|---|
| Texture2D | 818 | | Material | 29 |
| StaticMesh | 446 | | MaterialFunction | 15 |
| MaterialInstanceConstant | 260 | | SoundCue / SoundWave | 16 / 37 |
| World | 35 | | Skeleton / SkeletalMesh / PhysicsAsset | 3 / 2 / 2 |
| Blueprint | 42 | | AnimBlueprint | 2 (`ABP_FMTV`, `ABP_Turret_Veh`) |
| MapBuildDataRegistry | 27 | | TextureCube / SubsurfaceProfile / TireConfig / MPC / SoundAttenuation | 1 each |

**Zero** LevelSequence, Niagara, or Cascade assets — no sequence/VFX sanitation needed.

## Blueprints (42) — the AFL-3402 sanitise targets, grouped
- **Drivable vehicles + physics (DELETE/disable at M2):** `BP_FMTV_Drivable`, `BP_TurretVeh_Drivable`,
  `BPSM_Drone_Drivable2`, `BP_Drone`, `BP_FMTV`, `BP_TurretVehicle_Front/Rear`, `BP_Chaos_Front/Rear`,
  wheels ×4, tires ×2, `TireConfig`, the two AnimBPs, vehicle SoundCues (16) — ChaosVehicles-era.
- **Obstacle-course interactables (operator call at M3 — could become hub flavour):**
  `BPI_Climbing_Ropes`, `BPI_Crawling_Wires_a/b`, `BPI_JumpingTire_field01/02`, `BPI_Vaulting_Fence_Double`.
- **Static dressing BPs (keep):** T-wall family ×10, `BPI_SandBarrier_a/b`, `BPI_MilitaryBase_WiredFence_Gate_Double`, `BP_Basic_01–04`, `BP_Base`.
- **Camera shakes (delete with the showreel rig):** `CameraShake_SequenceCamera01/02`.

## 5.0 → 5.6 conversion facts ([VERIFY at 1.2] answered)
- Pack authored under UE **5.0** (project configs 2022-05; HoloLens config era). Assets load under
  5.6 via versioned serialization; they get **re-saved to 5.6 format during the migrate's fixup
  pass** (every asset is rewritten at its new path — the load-and-save pass happens implicitly).
- Chaos vehicle BPs are the most version-sensitive items and are M2 deletion targets anyway.
- No Nanite anywhere in the pack (5.0 authoring) — §6.5 Nanite enablement is a later measured pass.

## Sizes / budget (AFL-3400 record)
| | MB |
|---|---|
| Pre-ingest: `Content/` / LFS store / C: free | 64,165 / 63,114 / 118,610 |
| Pack on disk (`Content/US_Military`) | **8,533** (1,750 files) |
| Post-ingest C: free | 99,309 |
| LFS quota headroom | 250 GB quota; ~63 GB used pre-pack → ample |
