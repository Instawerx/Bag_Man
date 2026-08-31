# HUB CLUB VENUES — pack census (pre-harvest SSOT)

Committed ALONE before any asset motion, per the OutpostEarth precedent
(`HUB-MAP-CENSUS.md` / spec §1.2). Operator rulings baked in (2026-08-30/31 session):

- **Officers Club = the Patriot Cafe destination row** (display rename; row id `PatriotCafe`
  stays — shipped ids never rename, NM5). Venue map harvested from **GentlemensClubDemo**.
- **EM Lounge = SkyClub_Demo** harvest.
- **Model: TRAVEL venues (separate maps), superseding the spec §5.4 in-lobby JoinClub sketch.**
  The EOS sub-lobby social layer ([AFL-3043], gated EOS-AUTH-C2) layers on later; the seam-rule
  trade (a load between hub and club) is operator-accepted by the choice of demo-map venues.
- **Full discard this time**: harvest → operator PIE-verifies the clone → two-way zero-ref
  proof → delete the source packs from index AND disk. (OutpostEarth's own source, the 7.2 GB
  `MegaBase`, was KEPT as reference — that deletion never happened; club packs will not repeat
  that. MegaBase pruning is a separate, larger win, not in this arc.)

## License gate (import-and-rebrand §0)

Both packs arrived at bootstrap baseline `f841d361` from the studio's marketplace library.
**Operator owns the license confirmation** — no ledger entry exists for either pack; recorded
here as the standing gate note rather than a blocker (packs are already in-repo).

## Pack censuses (asset registry, 2026-08-31)

Both packs are **fully self-contained** (dependency walk from each demo map: zero external
/Game refs) and **not World Partition** (no `__ExternalActors__`; monolithic .umap packages).

### SkyClub — `/Game/SkyClub` (360 MB, 427 packages) → EM LOUNGE  [CANARY: runs first]
| Class | Count |
|---|---|
| StaticMesh | 191 |
| Texture2D | 149 |
| MaterialInstanceConstant | 64 |
| Material | 9 |
| Blueprint | 6 |
| World | 2 (`SkyClub_Demo`, `SkyClub_Overview`) |
| MapBuildDataRegistry | 2 (72.4 MB + 29.1 MB) |
| SkeletalMesh / Skeleton / PhysicsAsset / AnimSequence | 1 each |

- Referenced by `SkyClub_Demo` (harvest set): **312 packages**.
- Unreferenced (delete set): **115 packages** incl. `SkyClub_Overview` + its BuiltData.
- `SkyClub_Demo_BuiltData` (72 MB) dies at sanitize (§6.6 dynamic-lighting ruling).

### GentlemensNightClub — `/Game/GentlemensNightClub` (1.5 GB, 1202 packages) → OFFICERS CLUB
8 sub-packs (GentlemensClub 908 MB, ClassicInteriorProps 246 MB, InteriorProps 120 MB,
BarProps 89 MB, DoorsPack 68 MB, Restroom 62 MB, Electronics 25 MB, StageEquipment 14 MB).
| Class | Count |
|---|---|
| Texture2D | 573 |
| StaticMesh | 351 |
| MaterialInstanceConstant | 186 |
| Material | 58 |
| Blueprint | 25 |
| World | 2 (`GentlemensClubDemo`, `ModularInterior_Overview`) |
| MapBuildDataRegistry | 2 (323.1 MB + 54.5 MB) |
| ParticleSystem | 1 |
| SkeletalMesh / Skeleton / PhysicsAsset / AnimSequence | 1 each |

- Referenced by `GentlemensClubDemo` (harvest set): **1055 packages** (the demo showcases
  nearly the whole bundle).
- Unreferenced (delete set): **147 packages** incl. `ModularInterior_Overview` + BuiltData.
- `GentlemensClubDemo_BuiltData` (323 MB) dies at sanitize.

## Dispositions

| Set | Disposition |
|---|---|
| Demo maps (2) | DUPLICATE → `L_AFL_OfficersClub`, `L_AFL_EMLounge` under `/AFLHub/Clubs/`; sources discarded after clone proven |
| Referenced prop/material/texture sets | MIGRATE to `/AFLHub/Clubs/<Pack>/` preserving vendor structure (spec §1.3 batching: plain ×30, BP/SK/World singly + GC, maps last) |
| `*_Overview` maps + all 4 `*_BuiltData` | DELETE (showcase grids + invalidated bakes; §6.6 rules the clones fully dynamic) |
| Unreferenced tails (115 + 147 pkgs) | DELETE with the pack prune (`613f63a9` Wild_West pattern) |
| Vendor-branded screens (`M_ScreenClub`, `T_ScreenClub1`, signage) | REBRAND per spec §5.7 (no vendor string player-visible, §2.3 rule) |
| Vendor Blueprints (6 + 25) | Census at migrate: keep inert decor/door BPs, strip demo rigs/cinematics/GameMode overrides |

## Known code work this harvest unlocks (door backend half)

- `ExecuteDoorAction` implements only OpenScreen; **Travel case must be written** (payload
  already spec'd as the map id).
- **LATENT DEFECT (pre-existing)**: no `/AFLHub` path is registered for `PrimaryAssetType="Map"`
  in `Config/DefaultGame.ini` — even `L_AFL_OutpostEarth` is invisible to the AssetManager.
  Registration lands with the Travel backend.
- Each club map needs a WorldSettings/experience treatment + a RETURN door back to
  `L_AFL_OutpostEarth`.
