# HUB-ZONE-ASSIGNMENT — Outpost Earth MWR (AFL-3403, M3)

**Status:** **RATIFIED — ALL ROWS** (operator, 2026-08-27): "ratified, all rows … Approved we can modify later if necessary." Café/EM Lounge approved as proposed with an explicit modify-later allowance. Method: structure census over 6,155 placed actors +
28 vantage photographs (5 aerials + the pack's own 23 surviving showreel camera positions).
Shots: `Docs/Hub/ZoneShots/M3_*.png` (all 28 originals in `Saved/Screenshots/WindowsEditor/R_M3_*`).
**Rulings recorded this session:** hub pawn data = **`HeroData_BagMan_Pro`** (operator, 2026-08-27) ·
interactivity-first sanitise (vehicles/obstacles kept).
**Map frame:** landscape x −25 200…12 600, y −18 900…18 900 (~378 m × 378 m), walled perimeter,
watchtowers, two security-booth gates. Base layout: large multi-room building complex WEST, tent
grid CENTER, obstacle-course bowl NORTH-CENTER, drone apron + range lanes SOUTH, motor pool +
container yards EAST/SOUTH-WEST, aircraft strip on the far WEST edge.

| # | IRONICS zone (flow node) | Proposed structure | World location (x, y) | In/Out | Door(s) | Shot | Confidence | Ratified |
|---|---|---|---|---|---|---|---|---|
| 1 | **Spawn plaza — Main** (N4) | Central crossroads between tent grid, west complex road and east road; open ground, sight lines to every zone | (−4 000, −2 000) | Exterior | — (spawns + nav) | `M3_A_TopDown` | HIGH | ☑ 2026-08-27 |
| 2 | **PX Store** (N4g) | The **quartermaster store** wing of the west complex — floor-to-ceiling stocked shelving aisles, ready for `AAFLDisplayRack` slots + jewellery counter + mirrors | (−17 000, −2 000) | Interior | East door into complex corridor | `M3_Camera_07` | HIGH | ☑ 2026-08-27 |
| 3 | **Robo Labs** (N4a) | The **ops/office room** of the west complex — lit, enclosed, tech-dressed (desks, PCs, map boards); creator station + mirror wall fit naturally | (−14 000, −2 300) | Interior | Corridor door (red door visible in shot) | `M3_Camera_09` | HIGH | ☑ 2026-08-27 |
| 4 | **Loadout Barracks** (N4d) | The **central tent grid** (3 blocks, ~24 tents) — the literal billeting quarter; owned-gear racks along tent rows | (−4 800…−1 000, −1 000…2 000) | Semi-interior (tents) | Tent flaps N + S | `M3_A_TopDown` | MEDIUM | ☑ 2026-08-27 |
| 5 | **Patriot Café** (N4b) | **North tent row** — separate social tent cluster at the base's north edge | (−9 000, 12 500) | Semi-interior | South flap to plaza road | `M3_A_TopDown` (top-left) | LOW — interior undressed; confirm on your fly-through | ☑ 2026-08-27 |
| 6 | **EM Lounge** (N4c) | The **standalone building** east of the main complex (dark interior pending lighting rebuild — separate structure, so the club visibility mask stays per-interior) | (−11 100, −1 200) | Interior | West + east doors | `M3_Camera_25` | LOW — unlit; re-shoot after lighting pass | ☑ 2026-08-27 |
| 7 | **Deployments** (N4h) | The **walled drone apron** south — Reaper-style UAV on a marked pad; reads "deploy"; League/Staked door at its gate; Track-S board space on the wall | (−4 300, −8 600) | Exterior (walled) | North gate | `M3_Camera_21` | HIGH | ☑ 2026-08-27 |
| 8 | **Shooting Range door** (N4f) | The **SW target-lane field** by the south security booth — the pack's own range lanes; the door lives here, the range itself becomes `L_AFL_ShootingRange` (M6) from these pieces | (−9 000, −9 500) | Exterior | Booth-side threshold | `M3_A_TopDown` (bottom-left), `M3_A_SW` | MEDIUM | ☑ 2026-08-27 |
| 9 | **Tournaments / Mini Games tents** (N4e) | The **obstacle-course bowl** north-center — vaults, crawl wires, tire fields, stairs; the kept `BPI_*` interactables live here; tent-flap doors at the rim | (−2 000, 6 300) | Exterior (sunken bowl) | Rim entries E + W | `M3_Camera_24` | HIGH | ☑ 2026-08-27 |
| 10 | **Assigned Match door** (N7 entry) | The **east convoy road** — truck column along the container corridor; "ship out" gate | (5 100, 7 300) | Exterior | Road gate E | `M3_Camera_20` | MEDIUM | ☑ 2026-08-27 |
| 11 | **Landing backdrop camera** (N1) | The pack's own hero establishing shot — showreel `Camera_23` (south aerial over the whole base); alt `Camera_22` (north aerial). Both still placed as CineCameraActors — reuse as-is | (−14 087, −15 810, z 3 790) | — | — | `M3_Camera_23` | HIGH | ☑ 2026-08-27 |
| 12 | *(reserved, not an IRONICS zone)* Motor pool → **AFL-3411 hub-drivables bay** | Vehicle canopies + parked FMTV row | (−12 200, −5 100) | Exterior | — | `M3_Camera_16` | — | ☑ 2026-08-27 |

## Notes for ratification
- **Café/EM Lounge are the two LOW-confidence rows** — the pack has no obvious mess hall; my
  proposals use the north social tents and the standalone east building. Both are cheap to re-dress
  either way, and the spec's only hard constraint is that the two clubs be **separate structures**
  (per-interior visibility mask) — both proposals satisfy it. Reassign freely.
- **Lighting:** interiors currently show "Preview" watermarks / darkness — the streaming trim
  removed the showreel's per-camera lighting sublevels and the duplicate map has no `_BuiltData`;
  a lighting build (or Lumen decision, §6.6) is queued after zone placement so the dressing pass
  works in final light.
- **The west aircraft strip** (C-130 + helicopter visible on `M3_A_TopDown` far left) is outside
  every proposed zone — candidate flavour for Deployments-adjacent dressing or a future event stage.
- Placement (AFL-3404: ≥16 PlayerStarts on the plaza, `AAFLHubZoneVolume` per interior zone, nav)
  starts only after every row above is ratified.
