# IRONICS Map Repurposing Plan — Asset-Pack Maps + Modular Containment Barriers

**Goal:** repurpose bought/imported asset-pack showcase levels (e.g. `Sci_FI_Valley_Village` → GOLD BANKS) into IRONICS match maps quickly and safely, using a **pre-made containment-barrier kit** to define the play space instead of hand-building perimeter walls per map.

> Merge-conflict resolution is **operator-owned** (operator has solutions). This plan covers the map/barrier pipeline, not conflict merging.

## Repurpose pipeline (per map)
1. **Duplicate** the pack's showcase level → `/Game/Maps/L_<Name>` (verify active level before any edit — see [[feedback-verify-active-level-before-writes]]).
2. **Drag in** the pre-made **Cyber Barrier** of the target match size; position it so it contains the desired play area (it's ONE actor — moves/duplicates/deletes as a unit).
3. **Delete excess** outside the barrier (barrier defines the keep-boundary; makes culling the rest fast).
4. **Nav-first**: dynamic RecastNavMesh over the contained area; validate coverage + spawn→objective pathing BEFORE art investment (the GOLD BANKS / METATRON lesson: [[project-metatron-abandoned]]).
5. **Gameplay layer**: side-tagged spawns, loot/objective POIs (INFINEON spawner system), no-fall/no-dead-zone treatment (catch-tiles for internal voids).
6. **Dual-GE wiring** (Haywire + ProMod) + roster tile — the in-editor AIK step ([[project-ironics-dual-ge-requirement]]).

## Modular Containment Barrier — "Military Industrial Cyber Fencing"
Reusable draggable barriers, one baked mesh per size (drag one actor, contains perfectly, delete excess easily). Aesthetic: blast-wall base + I-beam posts + cyber/energy fence infill + emissive top strip (greybox now, art-pass later). Prefix `SM_BagMan_` ([[project_bag_man_asset_prefix]]).

**Size table (liberal — oversize so operator drags inward + trims):**

| Tier | Match sizes | Footprint (drag-to-fit) | Height |
|---|---|---|---|
| **BR** (build first) | Battle Royale (BR_18 / BR_36) | **1500 × 1500 m** | 100 m |
| Warfare | 8v8 – 12v12 | 700 × 700 m | 70 m |
| Arena | 4v4 – 6v6 | 300 × 300 m | 50 m |
| Skirmish | 3v3 | 180 × 180 m | 40 m |
| Duel | 1v1 – 2v2 | 90 × 90 m | 30 m |

- Square rings (easy to align/rotate); operator scales/positions to taste. Height is liberal to clear tall terrain; scale Z per map if needed.
- Also ship the single **`SM_BagMan_CyberFence_Panel`** (20 m module) for custom-shaped runs.
- Barrier collision = blocking (containment); can extend an invisible collision cap above the visible mesh if a map needs more height.

## Status
- **BR barrier** — building first (this pass).
- Warfare / Arena / Skirmish / Duel — after BR look + size are approved.
