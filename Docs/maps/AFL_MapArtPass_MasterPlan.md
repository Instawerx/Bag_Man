# AFL Map AAA Art-Pass — Master Plan & Work Order

**Goal:** turn our validated greybox maps into stylized **AAA, visually-storytelling** battle spaces — every cube/box/platform/wall/light replaced with modular-kit art and hero story props (crashed UFOs, damaged vehicles/aircraft, tanks, wreckage) — **without losing the design/science data** (layout, spawns, POIs, objectives, nav, sightlines, flow). One map at a time. **First map: L_Arena_04 (ARCANEON).**

## Governing principles
1. **Greybox = the spec.** The greybox layout, dimensions, and gameplay actors are the design/science data. Art replaces the *look*, never the *design*. Snapshot + lock it before touching anything.
2. **1 map at a time, phase-gated.** Finish ARCANEON fully before the next map.
3. **AAA from kits first.** Harvest our modular kits (already on disk); reserve genAI (Rodin) for bespoke hero props not in any kit (e.g. a signature crashed UFO). genAI is wrong for tiling architecture ([[project-ironics-base-building]]).
4. **AFL asset standards** (naming/import/LOD/collision/platform) per the asset-pipeline skill — PC + Console + Mobile.
5. **Stability discipline** (hard-won this project): restart editor before heavy level ops; verify active `.umap` before writes ([[feedback-verify-active-level-before-writes]]); no bridge level create/duplicate ([[reference-bp-ism-prefab-and-editor-stability]]); nav-first re-validation; commit each phase to Git-LFS.

## The per-map phase pipeline (repeatable)
| Phase | Name | Output |
|---|---|---|
| **P0** | Design-Data Snapshot & Lock | JSON/table of every greybox actor: position, size, role, + all spawns/POIs/objectives/navlinks. The immutable spec. |
| **P1** | Art Direction & Kit Palette | Theme + the specific kits/props chosen per role. Moodboard/screens. Operator sign-off. |
| **P2** | Kit Harvest & Conform | Blender-bridge AAA-clean the chosen kit pieces: scale to character, LODs, UCX collision, sockets, reimport manifest ([[reference-bridge-ue-origin-fbx-roundtrip]], afl-blender-bridge). |
| **P3** | Structural Replacement | Swap floor / walls / platforms / ramps / bridges / rings → kit art, matched 1:1 to greybox transforms & footprints. |
| **P4** | Cover & Hero Story Props | Replace every cover/obstruction greybox with art props at the same collision footprint (vehicles, tanks, wrecks); place the hero centerpiece. |
| **P5** | Set Dressing & Storytelling | Non-blocking detail: debris, decals, foliage, signage, scatter — the narrative layer. Must not alter nav. |
| **P6** | Lighting & Atmosphere | Replace default lighting: key/fill, emissive, fog/atmosphere, post-process volume, time-of-day mood. |
| **P7** | Nav & Collision Re-Validation | Art meshes get correct collision; rebuild nav; re-run coverage + spawn→objective pathing; **diff against P0 spec — zero gameplay regression**. |
| **P8** | Perf & Platform Optimization | Nanite/LODs/HISM/draw-call budgets; mobile texture overrides; profile GPU/CPU per platform (skill LOD & platform tables). |
| **P9** | Playtest, Verify & Sign-off | HOST→tile PIE (both GEs where applicable); confirm fun + navigable + design intact; commit; update registry. |

## Roles / division of labor
- **Me (bridge automation):** P0 snapshot, P2 harvest/conform (Blender bridge), P3/P4 transform-matched placement, P7 nav re-validation, P8 audits, per-phase commits.
- **AIK (in-editor):** material instances, Blueprint props, lighting build, anything the bridge can't author (per [[afl-neostack-task-writer]] pattern).
- **Operator:** P1 art direction + kit/theme approval, hero-prop taste calls, final PIE sign-off.

---

# WORK ORDER — L_Arena_04 (ARCANEON), first map

**Proposed theme:** **downed-craft crash/wreckage arena** — a signature **crashed UFO** as the central Anvil focal point (story: the arena formed around a crash), militarized wreckage (tanks, damaged vehicles, a downed aircraft) as cover, industrial/sci-fi kit for structure. Fits your art vocabulary + our kits (SpaceshipInterior/CyberPunk for the UFO, CyberCar/Helicopter/containers for wreckage, DeepWaterStation/ModularSciFiEnv_H/HighTechPack1 for structure).

**Per-actor treatment map (greybox role → AAA art → kit source):**

| Greybox (design data) | Count | AAA treatment | Kit source |
|---|---|---|---|
| `Floor_Pit` | 1 | Battle-scarred floor / crater deck | ModularSciFiEnv_H / DeepWaterStation floor tiles (HISM) |
| `Wall_N/S/E/W` | 4 | Industrial perimeter walls | DeepWaterStation `SM_MetalWall*` / our CyberBarrier kit |
| `Anvil_Platform` (center) | 1 | ⭐ **Hero: crashed UFO** centerpiece | SpaceshipInterior hull + Rodin bespoke UFO if needed |
| `Ring_N/S/E/W` + `Parapet_Ring_*` | 6 | Upper catwalks + railings | ModularSciFiEnv_H catwalk / DeepWaterStation `SM_Railings01` |
| `Bridge_A/B` | 2 | Metal gantry bridges | SciFi/industrial gantry pieces |
| `Ramp_Pit_*` + `Ramp_Bridge_*` | 8 | Ramps / stairs (walkable) | kit ramps/stairs |
| `Vault_Lane` | 10 | Vault-height cover (vehicles/barriers you vault) | CyberCar, containers, barriers — waist-high, vault-matched |
| `Cover_Pit` | 6 | Hero cover: damaged tanks/vehicles/wreck | CyberCar/Helicopter wreck, Rodin tank |
| `CQB_Mouth` | 6 | Doorways / breach openings | kit doorway/arch pieces |
| `Div_W/E_*` | 4 | Half-wall dividers | kit low walls |
| `ClimbFace_N/S` | 2 | Marked climbable surfaces | kit wall + climb decal |
| `SM_SkySphere` | 1 | Replace with atmosphere/sky for theme | Sky/atmosphere pass (P6) |

**PRESERVE VERBATIM (never move/delete):** 16 `LyraPlayerStart`, 15 `B_WeaponSpawner`, 3 `B_AbilitySpawner`, 3 `B_AFL_ExtractionZone`, 10 `NavLinkProxy`. Vault mechanic is ProMod-gated ([[project-afl-lefthand-weapon-ik]] / VAULT) — keep Vault_Lane heights vault-accurate.

**Execution order for ARCANEON:** P0 snapshot → P1 approve theme/palette → P2 harvest the ARCANEON kit subset → P3 structure → P4 cover + crashed-UFO hero → P5 dressing → P6 lighting → P7 nav re-validate (diff vs P0) → P8 perf → P9 PIE both-GE sign-off + commit.

---

# EXTRACTOR ART SYSTEM (reusable across all maps)

The `B_AFL_ExtractionZone` objectives are currently art-less — the one hole in placement. Build a **branded, AAA, hero Extractor** system: a modular Blueprint that skins the extraction volume, with **light-up pulse + audio cue**, that the player can **walk into to extract**, and that can be **coupled/blended into other map features** (inside a tunnel, back of an army truck, under a UFO, etc.).

## Core: `B_AFL_Extractor` (modular BP wrapping the extraction volume)
- **Skin slot** — swappable hero mesh (variant meshes below). Enterable interior/pad.
- **Pulse FX** — emissive material driven by a sine/timeline (idle glow → fast pulse when extraction active/available); Niagara energy accents.
- **Audio** — looping idle hum (`SC_AFL_Extractor_Idle`) + activation/extract cue (`SC_AFL_Extractor_Activate`), attenuated 3D.
- **Branding** — IRONICS/BagMan logo decal + signature NeonColor ([[reference-standard6-flagship-colors]] / [[reference-finish-color-dump-method]]).
- **Enter/Exit** — walk-in trigger = the extraction volume; clear ingress (ramp/door/pad).
- **Modular coupling** — attach socket + "blend mode" so it nests into a tunnel mouth, a truck bed, a UFO underbelly, etc. (composable with map hero props).

## Variants (start with 2)
1. **SPACESHIP Extractor** — a landed dropship/UFO you board (ramp/hatch); hull pulses; engine glow; dust/heat Niagara. Mesh: SpaceshipInterior kit + **Rodin bespoke dropship/UFO**.
2. **TELEPORTER Extractor** — a pad + portal ring; vertical energy column that pulses/beams on extract; you stand on the pad. Mesh: **Rodin portal ring/pad** + Niagara beam.

## Build division
- **Rodin gens** — bespoke spaceship (dropship/UFO) + teleporter (portal ring/pad) hero meshes ([[reference-rodin-gen-pipeline]]).
- **AIK (in-editor)** — the pulse material (emissive + time node), Niagara FX, audio cues, and the `B_AFL_Extractor` BP graph (timeline, audio, branding decal, socket) — material/timeline/audio graphs are AIK's domain ([[afl-neostack-task-writer]]).
- **Bridge (me)** — place `B_AFL_Extractor` on each map's extraction zones, swap in Rodin meshes, wire the extraction volume, screenshot/verify.

## Status
- ARCANEON P0–P7 done + committed `ce1c6b44`. **Remaining:** Extractor art (this system, 2 variants) + EX2 PIE-verify + vault/UFO refine + P6 lighting + P8 perf + P9 sign-off.
