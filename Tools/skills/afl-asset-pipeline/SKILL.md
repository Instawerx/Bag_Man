---
name: afl-asset-pipeline
description: >
  AFL asset pipeline engineer skill for the Lyra-derived UE5 project
  (filesystem Bag_Man, code prefix AFL, launch identity Ironics - Beta
  Lands V1.0). Covers the full art-to-engine workflow: DCC tool export
  settings (Maya, Blender, Substance), FBX/USD import into UE5,
  Lyra-compatible skeletal mesh setup, LOD generation, texture
  compression per platform (PC/Console/Mobile), AFL asset naming
  conventions, Git LFS management for large binary assets, cook
  validation, redirector fixup for content relocation, and asset audit
  workflows. Use whenever the work involves importing meshes or
  textures, DCC export settings, skeleton retargeting for Lyra's
  mannequin, LOD setup, texture compression settings per platform, Git
  LFS configuration, missing assets in cook, asset redirectors,
  content shape decisions (relocating /Game/ content into a plugin per
  the AssetReferenceRestrictions validator), Material Instance setup
  from source art, Nanite enablement criteria, or any workflow
  bridging DCC tools to the project's UE5 build. Paired with
  lyra-ue5-build-discipline (the rebuild's methodology and 22-trap
  catalog) and afl-cpp-lyra-developer (which carries the architecture
  rules that asset-shape decisions enforce).
---

# AFL Asset Pipeline Skill

AFL asset pipeline engineer. All assets must meet AFL's quality bar, be
correctly configured for **PC + Console + Mobile** targets, and be
compatible with **Lyra's skeleton and animation system**.

**Identity Map**: see `lyra-ue5-build-discipline/SKILL.md` for the
canonical Bag_Man (filesystem) / AFL (code prefix) / Ironics - Beta
Lands V1.0 (launch identity) disambiguation. Asset paths in this skill
use `Bag_Man` filesystem identity and `AFL` code prefix; project
display fields use the full launch identity.

This skill pairs with `afl-cpp-lyra-developer` (which carries the
architecture rules that asset-shape decisions enforce — particularly
rule 7 on content shape) and `lyra-ue5-build-discipline` (the
methodology + 22-trap catalog). Read the discipline catalog's traps #5
(.uasset cosmetic churn), #6 (cube-mesh path subtlety), and #13
(editor-locks-uassets) before any sustained asset workflow.

---

## AFL Asset Naming Conventions (illustrative)

This table defines the **naming patterns** for each asset type. Examples
are illustrative — they show the pattern, not the live asset list. For
the actual assets on disk, see the **Project Asset Registry** section
below.

| Asset Type | Prefix | Suffix Convention | Pattern | Example |
|---|---|---|---|---|
| Static Mesh | `SM_` | `_LOD0` (source) | `SM_<system>_<name>_<variant>` | `SM_AFL_Crate_01` |
| Skeletal Mesh | `SK_` | | `SK_<character>_<bodypart>` | `SK_BagMan_Body` |
| Physics Asset | `PHYS_` | | `PHYS_<character>_<bodypart>` | `PHYS_BagMan_Body` |
| Animation Sequence | `AS_` | | `AS_<character>_<action>_<dir>` | `AS_BagMan_Run_Fwd` |
| Anim Montage | `AM_` | | `AM_<character>_<action>` | `AM_BagMan_MeleeLight` |
| Anim Blueprint | `ABP_` | | `ABP_<character>` | `ABP_BagMan` |
| Material | `M_` | `_Master` for masters | `M_<system>_<purpose>_Master` | `M_AFL_Metal_Master` |
| Material Instance | `MI_` | | `MI_<system>_<variant>` | `MI_AFL_Metal_Rusty` |
| Texture (Diffuse) | `T_` | `_D` | `T_<system>_<surface>_D` | `T_AFL_Metal_D` |
| Texture (Normal) | `T_` | `_N` | `T_<system>_<surface>_N` | `T_AFL_Metal_N` |
| Texture (ORM) | `T_` | `_ORM` | `T_<system>_<surface>_ORM` | `T_AFL_Metal_ORM` |
| Niagara System | `NS_` | | `NS_<system>_<effect>` | `NS_AFL_Explosion` |
| Sound Wave | `SW_` | | `SW_<system>_<sound>_<variant>` | `SW_AFL_Footstep_Concrete` |
| Sound Cue | `SC_` | | `SC_<system>_<event>` | `SC_AFL_Footstep` |
| Data Asset | `DA_` | | `DA_<system>_<purpose>` | `DA_AFL_WeaponStats` |
| Blueprint | `B_` or `BP_` | `_BagMan` for player-specific | `B_<system>_<name>` or `BP_<name>_BagMan` | `B_Experience_BagMan` |
| Pawn Data | `HeroData_` | | `HeroData_<character>` | `HeroData_BagMan` |
| Input Config | `DA_AFL_` | `_InputConfig` | `DA_AFL_<character>_InputConfig` | `DA_AFL_BagMan_InputConfig` |

**Content prefixes:**
- `BagMan` suffix on assets specific to the player character or
  player-specific content (Blueprints, animations, character meshes).
- Generic AFL systems use `AFL` only (`SM_AFL_Crate_01`, `DA_AFL_WeaponStats`,
  weapons, environment, abilities-as-data).

See `afl-cpp-lyra-developer` for C++ naming conventions
(`UAFL*Component`, `UAFLAG_*` abilities, `UAFLAttributeSet_*`).

**When you add a named asset to the project, update the Project Asset
Registry below as part of the same sprint commit.**

---

## Project Asset Registry (LIVE — updated per sprint)

**This section reflects the actual assets on disk.** It is updated per
sprint as new assets land — see Definition of Done in
`afl-sprint-planner` (authored at D.0d). If you add a named asset, add
it here in the same commit; the registry drifts in sync with reality.

Last inventoried: D.0b commit (when this section was first populated).

### BagMan (player character)
*Assets specific to the BagMan player pawn — meshes, animations,
input config, experience definition, ability sets, pawn data. Lives in
`/Game/BagMan/` (project content — subject to D Layer 1 relocation per
the content-shape rule, since several of these reference `/ShooterCore/`
plugin assets).*

- **Characters/**
  - `B_BagMan_AssignCharacterPart`
  - `B_Hero_BagMan`
  - `HeroData_BagMan`
- **CombatTest/**
  - `B_AFL_DamageTarget`
- **Environments/Decals/**
  - `SM_BagMan_Decal_OpsSign`
- **Equipment/**
  - `AbilitySet_BagMan_NoFire`
  - `ID_BagMan_PulseCarbine`
  - `WID_BagMan_PulseCarbine`
- **Experiences/**
  - `B_Experience_BagMan`
- **Maps/**
  - `L_BagMan_Greybox` (.umap)
- **Materials/Decals/**
  - `MI_BagMan_Decal_OpsSign`
- **Materials/Master/**
  - `M_BagMan_Decal_Master`
- **Textures/Decals/**
  - `T_BagMan_Decal_OpsSign_BC`

### AFLCombat (combat plugin)
*Combat-system assets — ability sets, gameplay effects, damage data,
combat test scaffolding, Pulse tuning. Lives in `/AFLCombat/` (plugin
mount).*

- `AFLCombat` (GameFeatureData)
- **Abilities/**
  - `BP_GA_AFL_Damage_Test`
- **Effects/**
  - `GE_AFL_Combat_InitData`
  - `GE_AFL_Damage_Instant`
- **Sets/**
  - `DA_AFL_Combat_AbilitySet`
- **Tests/**
  - `B_AFL_Test_Experience`
  - `DA_AFLTestPawnData`
- **Tuning/**
  - `DA_AFLPulseTuning`

### AFLCore (core plugin)
*Core systems. Code-heavy plugin — only the GameFeatureData asset on
disk; tags and shared types live in C++/ini, not content. Lives in
`/AFLCore/` (plugin mount).*

- `AFLCore` (GameFeatureData)

### AFLMovement (movement plugin)
*Movement-system assets — dash ability/effects, movement pawn data,
input mappings, validation map. Lives in `/AFLMovement/` (plugin mount).*

- `AFLMovement` (GameFeatureData)
- **Abilities/**
  - `GA_AFL_Dash`
- **Data/**
  - `DA_AFL_PawnData_Hero_Default`
- **Effects/**
  - `GE_AFL_DashCooldown`
  - `GE_AFL_Dash_Active`
- **Experiences/**
  - `LAS_AFL_HeroComponents` ⚠️ *the standing-hazard action set — inert on disk, NEVER add to BagMan's experience chain; see `afl-cpp-lyra-developer` Standing Hazard section + `project_uaflherocomponent_standing_hazard` memory*
- **Input/**
  - `IMC_AFL_Movement`
  - `InputData_AFL_Hero`
- **Sets/**
  - `DA_AFL_AbilitySet_Movement_Dash`
- **Tests/**
  - `L_AFL_Movement_Validate` (.umap)

### ShooterCore (Lyra-stock plugin content)
*`Content/ShooterCore/` **does not exist** — ShooterCore is a plugin
(`Plugins/GameFeatures/ShooterCore/`), so its content lives at the
`/ShooterCore/` plugin mount, NOT a `/Game/ShooterCore/` path. The
BagMan→ShooterCore references found in Phase A (B_Hero_ShooterMannequin,
AbilitySet_ShooterHero, the Pistol equipment chain, the LAS_ShooterGame_*
action sets) resolve to that plugin mount. D Layer 1's relocation moves
BagMan content into a plugin so those plugin→plugin references become
legal — it does NOT touch ShooterCore's own content. Not inventoried
here (Lyra-stock, not project-authored).*

*(directory not present at `Content/ShooterCore` — see note above)*

---

## Content Shape — the BM-DEBT-001 Pattern

The `AssetReferenceRestrictions` validator forbids `/Game/` content
from referencing plugin content. The fix: put content that needs to
reference plugin assets *inside a plugin*. **Relocate the content,
don't sever the references.** Plugin → plugin references are legal;
the validator only flags `/Game/` → plugin.

This is `afl-cpp-lyra-developer` rule 7, applied to assets. The
canonical UE5 mechanic for performing the relocation cleanly is the
redirector fixup commandlet (see "Asset Validation" section below).

When you relocate an asset:
1. Move the asset's source file (`git mv` if tracked, preserving LFS
   pointers and git history).
2. UE5 leaves a redirector at the old location pointing at the new
   location — references to the old path keep working transiently.
3. Run the redirector-fixup commandlet to update every reference to
   point at the new location directly, then delete the redirectors.
4. Asset-audit cook for missing references; verify clean.
5. **Update the Project Asset Registry** above to reflect the new
   asset homes.

The redirector fixup is what makes content relocation tractable at
scale — without it, references stay broken until every consumer is
hand-edited. With it, the commandlet does the editing.

---

## DCC Export Settings

### Maya / Blender → FBX
```
Scale:              1.0 (UE uses cm; set DCC scene to cm OR use Scale Factor)
Axis:               Forward = -Y, Up = Z (UE convention)
Smoothing Groups:   ON
Tangents/Binormals: OFF (let UE recompute -- more reliable)
Triangulate:        ON before export
LODs:               Export as single FBX with LOD groups OR separate files
Sockets:            Export as bones prefixed with SOCKET_ (UE auto-detects)
Collision:          Name convex hulls UCX_MeshName_01, UCX_MeshName_02...
```

### Substance Painter → UE5 Texture Export
```
Export Preset:  Unreal Engine 4 (Packed)  -- preset name unchanged in UE5
Channel Packing:
  _D   = BaseColor (RGB)
  _N   = Normal (RGB, DirectX -- UE uses DirectX convention)
  _ORM = Occlusion(R) + Roughness(G) + Metallic(B)
  _E   = Emissive (RGB, if needed)
Bit Depth:  8-bit for _D, _ORM, _E | 16-bit for _N
Resolution: 4096 master, export 2048 for Mobile
```

---

## UE5 Import Settings — AFL Standards

### Static Mesh Import
```
Generate Lightmap UVs:   true (channel 1)
Build Nanite:            true if poly count > 50k AND not foliage with alpha
Auto Generate Collision: false -- hand-author UCX_ convex hulls
Import Mesh LODs:        true (import FBX LOD groups)
Combine Meshes:          false (keep separate for occlusion culling)
```

### Skeletal Mesh Import (Lyra Skeleton)
```
Skeleton:             SK_Mannequin (Lyra's skeleton asset)
                      Path: /LyraStarterGame/Characters/Heroes/Mannequin/Meshes/
Import Morph Targets: true if facial / blend shapes exist
Use T0AsRefPose:      true
Import Animations:    false on mesh import -- import animations separately
```

### Texture Import
```
# Master settings per texture type
_D (BaseColor):
  Compression: DXT1 (no alpha) / DXT5 (with alpha)
  sRGB: true
  Mip Gen: FromTextureGroup

_N (Normal):
  Compression: BC5 (best quality) or NormalMap
  sRGB: false
  Flip Green Channel: false (DirectX normals -- correct for UE)

_ORM:
  Compression: DXT5 (packed channel, lossless-ish)
  sRGB: false
  Mip Gen: FromTextureGroup

_E (Emissive):
  Compression: DXT1
  sRGB: true
```

### ⚠️ Cube-Mesh Path Subtlety (trap #6)

When importing or referencing the Engine's default cube mesh, **note
which path you're using.** `/Engine/BasicShapes/Cube` and
`/Engine/EditorMeshes/EditorCube` differ in size (1.12cm vs 100cm
edge-equivalent), and `/Game/Effects/...` cubes from older asset packs
can introduce a third size. Using the wrong cube in a measurement-
critical context (collision bounds, hit-test debug visuals, scale
references) produces silently wrong results.

See `lyra-ue5-build-discipline` trap #6 for the diagnostic signature.
Defensive practice: always log the cube's actual extents in any code
path that relies on its size.

---

## Platform Texture Compression Overrides

```
# In Texture asset > Platform Overrides tab

PC / Console:
  Win64:  DXT1/DXT5/BC5 (default -- no override needed)
  PS5:    GNF format handled automatically by PS5 target
  XSX:    DDS with XBC compression -- auto via target platform

Mobile:
  Android: ASTC (recommended for all Mali/Adreno/PowerVR)
           Override: TextureGroup = Mobile, Compression = ASTC_8x8
  iOS:     ASTC or PVRTC4 (ASTC preferred for A-series 2018+)
```

Set these per-texture or via `DefaultDeviceProfiles.ini`:
```ini
[/Script/Engine.TextureLODSettings]
@TextureGroup=TEXTUREGROUP_Mobile
MinLODSize=16
MaxLODSize=512   ; Mobile max texture = 512 or 1024
MipGenSettings=TMGS_SimpleAverage
```

---

## Git LFS Setup for Binary Assets

```bash
# .gitattributes -- AFL binary asset tracking
*.uasset filter=lfs diff=lfs merge=lfs -text
*.umap   filter=lfs diff=lfs merge=lfs -text
*.fbx    filter=lfs diff=lfs merge=lfs -text
*.psd    filter=lfs diff=lfs merge=lfs -text
*.png    filter=lfs diff=lfs merge=lfs -text
*.tga    filter=lfs diff=lfs merge=lfs -text
*.wav    filter=lfs diff=lfs merge=lfs -text
*.mp3    filter=lfs diff=lfs merge=lfs -text
*.mp4    filter=lfs diff=lfs merge=lfs -text
*.zip    filter=lfs diff=lfs merge=lfs -text
*.dll    filter=lfs diff=lfs merge=lfs -text
*.exe    filter=lfs diff=lfs merge=lfs -text
```

```bash
# Initial LFS setup (already configured on this project)
git lfs install
git lfs track "*.uasset"
git lfs track "*.umap"
git add .gitattributes
git commit -m "chore: configure Git LFS for UE binary assets"
```

**AFL LFS rules:**
- All `.uasset` and `.umap` → LFS, no exceptions
- Source art (PSD, TGA, FBX) → LFS
- Never commit `DerivedDataCache/`, `Binaries/`, `Intermediate/`,
  `Saved/` to Git

### ⚠️ Cosmetic .uasset Churn (trap #5)

UE5 sometimes modifies .uasset files cosmetically (touch timestamps,
serialization order shifts) when assets are merely opened, even
without intentional edits. These show up as `M` in `git status` but
have no semantic change. The trap is committing this churn:

1. It pollutes git history with no-op diffs
2. It contaminates intentional commits with unrelated cosmetic noise
3. It can cause spurious LFS uploads (the byte content changed even if
   semantics didn't)

**Mitigation**: before staging any .uasset, verify the modification
was intentional. If unsure, `git diff` the binary may not help — but
checking the editor's status (was this asset actually opened and
edited, or just touched?) usually clarifies. If purely cosmetic,
revert with `git checkout -- path/to/file.uasset` rather than commit.

See trap #5 for the full diagnostic.

### ⚠️ Editor Locks .uasset Files (trap #13)

While the UE5 editor is open, .uasset files for loaded assets are
locked — git operations on them fail with permission errors. Some
implications:
- Asset relocation (`git mv`) requires the editor closed or the asset
  unloaded.
- The redirector-fixup commandlet (next section) requires the editor
  closed (it runs as its own process and would conflict with an
  editor mutex on shared assets).
- For mid-editor-session asset edits where MCP / Python is the
  authoring path, the editor stays open and the workflow uses
  `compile_blueprint` + `save_asset` per trap #18 (BP-CDO persistence).

The constraint inverts depending on whether you need MCP (editor
stays open, no UBT build) or a commandlet/build (editor closes, no
MCP). See `lyra-ue5-build-discipline` trap #13's MCP-access corollary.

---

## Asset Validation — Cook Audit + Redirector Fixup

Run before every release branch cook, AND after any content
relocation per the BM-DEBT-001 pattern:

```bash
# UE5 editor commandlet -- audit all assets for missing references
"<UE5 root>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\Dev\Bag_Man\Bag_Man.uproject" ^
  -run=AssetRegistry ^
  -checkforerrors ^
  -unattended ^
  -stdout

# Fix up redirectors (after asset renames or relocation)
"<UE5 root>\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "C:\Dev\Bag_Man\Bag_Man.uproject" ^
  -run=ResavePackages ^
  -fixupredirects ^
  -autocheckout ^
  -unattended
```

Note: the executable is `UnrealEditor-Cmd.exe` for UE5 (not
`UE4Editor-Cmd.exe` — that's the UE4 name). Project file is
`Bag_Man.uproject` per the project's filesystem identity.

The redirector-fixup commandlet is the load-bearing mechanic for
content relocation (rule 7 in `afl-cpp-lyra-developer`, BM-DEBT-001's
resolution). After relocating an asset, run the fixup, then delete
the redirector stubs UE5 left behind.

**AFL Cook Checklist:**
- [ ] No broken redirectors (`Edit > Fix Up Redirectors in Folder` on
      `/Game/` AND on any plugin's `/Content/` folder that received
      relocated assets per the BM-DEBT-001 pattern)
- [ ] No missing textures (Asset Audit → filter by missing)
- [ ] All Nanite meshes have valid LOD0
- [ ] Mobile texture overrides set on all character textures
- [ ] Sound assets have correct attenuation settings
- [ ] All DataTables have valid row struct references
- [ ] Project Asset Registry (in this skill) reflects current disk state

---

## LOD Generation Guidelines

| Mesh Category | LOD0 | LOD1 | LOD2 | LOD3 | Nanite? |
|---|---|---|---|---|---|
| Hero character | 80k tris | 40k | 15k | 5k | No |
| Environment props (large) | 50k+ | auto | auto | auto | Yes |
| Environment props (small) | <10k | auto | auto | — | Optional |
| Weapons (FPP) | 20k | 10k | — | — | No |
| Foliage | 5k | 2k | 500 | — | No (alpha) |
| Buildings / architecture | 100k+ | — | — | — | Yes |

Auto-generate LODs via: Asset Details > LOD Settings > Auto LOD >
Generate. Hand-author critical LODs (hero characters) in DCC.

---

## When to Reach for the Discipline Catalog

`lyra-ue5-build-discipline` carries 22 catalogued anti-patterns.
Consult the catalog when:

- Importing any new asset (traps #5 cosmetic churn, #6 cube paths)
- Editing .uassets via MCP / Python (traps #1, #18)
- Relocating content (`/Game/` → plugin, rule 7) — trap #13 editor
  locks during the redirector-fixup pass
- Building / running cook (traps #4 PowerShell encoding, #12 UBT
  mutex, #13 editor locks)
- Doing git operations on binary assets (trap #21 git output mediation
  — the wrapper/PowerShell may lie about push success)

---

## Cross-references

- **`lyra-ue5-build-discipline`** (Tools/skills/) — paired methodology
  and 22-trap catalog. Carries the canonical Identity Map referenced
  in this skill's opening.
- **`afl-cpp-lyra-developer`** (Tools/skills/) — paired architecture
  rules. Rule 7 (content shape: relocate-not-sever) is the C++ side of
  the relocation pattern this skill provides the tooling for.
- **`afl-sprint-planner`** (Tools/skills/, *authored at D.0d — see git log*) —
  task format and estimation guide. The Project Asset Registry DoD
  item is added there at D.0d.
- **`unreal-engine-expert`** (Tools/skills/, *authored at D.0c — see git log*) —
  broader AAA UE5 patterns.
- **Master Build Document** (Docs/) — the project's SSOT and forward
  roadmap.
- **`BAG_MAN_LIVE_TRACKER.html`** (project root) — the live tracker;
  reconciled to git reality at HEAD `b93bd0a9` (post-D.0a).
