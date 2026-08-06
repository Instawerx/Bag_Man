# Arena_04 — Per-Map Design Brief

**Status:** ✅ **APPROVED + GREYBOX BLOCKOUT COMPLETE (flow-complete) 2026-08-02/03** (operator ruling — greybox authorized per `IRONICS_MAP_MODE_SPEC` §11). Level `/Game/Maps/L_Arena_04` built via the crash-safe bridge `create_level` (after an initial `new_level` attempt crashed the editor — no data lost; see [[feedback-ue-bridge-new-level-crash]]). All §3–§6 flow elements are in and nav-verified. **PIE sign-off (the map's `✅ watched in PIE`) still pending** — needs the operator Experience/playlist wiring (both GEs per [[project-ironics-dual-ge-requirement]]) + the §6 telemetry playtest. (Originally DRAFT, 2026-08-02.)

### Greybox build record (2026-08-02, via `unreal-editor` bridge)
- **Level:** `/Game/Maps/L_Arena_04` (standard level; **WP conversion deferred to pre-art** — WP matters for 16-player streaming/perf, not flow validation).
- **Footprint built:** 141 × 91 m × 20 m vertical; **3 tiers at Z 0 / +8 / +16 m** (Pit / Bridges / Ring) per §2/§3.
- **Actors (final census):** **51 StaticMesh masses** — pit floor, 4 perimeter walls, 2 bridges, 4-segment ring, anvil dais, 8 walkable ramps, **4 lane dividers** (3 lanes/side per §3), **16 cover pieces** (6 pit vault + 10 lane vault + 6 CQB choke-cover per §5 bands) + 2 ring parapets, **2 climb faces** onto the Ring (§4.1); **16 `LyraPlayerStart` (8/8, tagged `AFL.Spawn.Side.0`/`.1` W/E — verified)**; **3 `B_AFL_ExtractionZone`** (Pit / Bridge / Ring, one per tier per §6); **18 POI spawners** — **15 `B_WeaponSpawner`** (WeaponIntent-tagged) + **3 `B_AbilitySpawner`** (heal pads, wired); **1 `BP_AFL_TestGrabbable`** grab-shard beside the Bridge extract (§4 GRAB — placeholder for the neon-glass shard); **10 `NavLinkProxy`** (Pit↔Bridge ×4, Bridge↔Ring ×4, climb ×2), `NavMeshBoundsVolume` + `RecastNavMesh`, template lighting/sky/fog. All objectives nav-reachable from spawn through the lanes (Pit 63 m, Bridge 78 m, Ring 162 m, non-partial).
- **Nav verified (the §7 goalvalid-disambiguator property):** RecastNavMesh built; bot pathing from a West spawn to **all three tier extracts is non-partial** (Pit 63 m, Bridge 79 m, Ring 162 m) and West↔East spawn is non-partial (126 m). Two connectivity bugs were caught and fixed during the pass: a too-tall anvil (nav island → lowered to walkable dais) and open ring corners (→ Ring E/W extended to meet N/S).
- **Completed this pass (flow-complete blockout):**
  - **Tiers + loop:** 3 tiers (Pit 0 / Bridges +8 / Ring +16 m), perimeter, walkable ramps (34.8° Pit→Bridge, 36.5° Bridge→Ring — flush-connecting, NavLinks as redundancy).
  - **Lanes (§3/§5):** 4 dividers → 3 lanes/side, base→pit sprint arteries; cover bands per §5 (10 lane vault + 6 CQB choke cover + 6 pit vault + 2 ring parapets).
  - **Traversal (§4):** 2 **climb faces** onto the Ring (the exposed counter-route) + climb NavLinks; vault cover on Pit/Ring/lanes; **grab shard** (`BP_AFL_TestGrabbable`) beside the Bridge extract.
  - **Spawns/extracts:** 16 starts (8/8 side-tagged, verified) · 3 extraction zones (one per tier).
  - **POIs (established INFINEON system):** 15 `B_WeaponSpawner` with `AFL.Spawner.Weapon.*` `WeaponIntent` tags (registry resolves tag→pickup at runtime, per `AFLWeaponSpawner.h`); 3 `B_AbilitySpawner` heal pads with direct `WeaponPickupData_*` refs (always-loaded stock → legal direct ref). **No C++/AFLCombat change** — the ability-intent roster was investigated and found unnecessary.
  - **Nav:** RecastNavMesh + 10 NavLinks; all objectives reachable from spawn through the lanes (non-partial).
  - **Dual-GE registry:** verified `AFLGFA_WeaponSpawns` present in **both** ProMod and Haywire GEs (Haywire was missing it → fixed by adding `LAS_AFL_ExtractionMatch`, [[project-ironics-dual-ge-requirement]]).
- **Remaining (operator/next steps):** ① **Experience/playlist wiring for both GEs** — `EXP_Arena04_<size>_Haywire`/`_ProMod` (+ `DA_AFL_*` playlists); the `GameFeatureAction` ComponentList edit is operator in-editor (not bridge-editable), same as INFINEON/T1. ② **WP conversion** (pre-art; matters for 16-player streaming/perf, not flow). ③ **§6 telemetry playtest** → the §11 exit windows → art. ④ economy tuning: exact weapon-per-POI spread + neon-glass-shard art to replace the grab placeholder. ⑤ ramp geometry is walkable but rough — polish at art.
**Tier:** C (Large-Team, 5v5–8v8 up to 16) · **Source:** tracker (Arena_04, AFL-1801, "energy storms" → **re-cast by operator as the neon-glass showcase arena**)
**Purpose:** the FIRST Tier-C map and **the map that establishes the IRONICS house look** — neon glass, volumetric neon cloud, ultra laser-tag feel, tiered-and-looping. It proves the loop at 16-player scale AND sets the visual identity that **BR_36 inherits later rather than inventing.** Where Arena_01 proved the loop bare and Arena_02 adds the first exotic mechanic, Arena_04 proves the loop **at Tier-C density with the flagship aesthetic**.

> Grounded in `IRONICS_MAP_MODE_SPEC` §2 (extraction primitive), §3 Tier C (density/footprint), §6 (telemetry loop), §11 (this template). All numeric targets are **greybox-validate, not fixed law** (§3). Conforms to the shape of the approved `Docs/maps/Arena_01_DESIGN.md`.

> **Reference substrate: L_Expanse — MEASURED LIVE via the `unreal-editor` bridge (full all-cells WP descriptor pass; studied, NOT referenced).** Bounds read from all **3,011 World-Partition actor descriptors** (loaded + unloaded cells, via `get_actor_descs()`) in the live editor, 2026-08-02. Measured facts (these correct the prior docs where noted):
> - **Geometry footprint ≈ 133 m × 50 m** (all-cells static-mesh X-span 132.8 m, Y-span 50.0 m — a **long/narrow ~2.6:1 two-base lane map**). Gameplay-actor envelope ≈ 116 × 43 m. ⚠ **Corrects the "~94 m" figure** — that was only the loaded PlayerStart X-extent, not the geometry.
> - **Playable vertical ≈ 23–28 m across 3–4 tiers** (gameplay Z −16…+7 m; static-mesh Z −17.8…+10.4 m; tier clusters ~−13 / ~−4 / ~0 / ~+6 m). Real, measured tiering — inter-tier deltas ~6–9 m.
> - **20 `LyraPlayerStart`, 10 West / 10 East** (X ±46 m, Y ±16 m). The `INFINEON_Expanse_DESIGN.md` figure of 20 was **correct** — an earlier disk `grep` of "40" was a per-package string-count artifact, now retracted. Surplus (unloaded-cell) starts sit in an **`ExtraSpawn` data layer**, the proven way L_Expanse scales starts by player count.
> - **4 extraction zones** distributed across ~13 m of height and the full X range; **23 weapon/ability spawners**.
> - **Geometry is a modular-recolor kit, baked:** 2,413 `StaticMeshActor` + 2,386 `BakedStaticMeshActor_C` + 274 `GeneratedDynamicMeshActor`, overwhelmingly **one mesh family (`Mesh_A_2`) recolored across a wide hex palette** — assembled from a `BakedGeneratedMeshSystem` + modular kit tools (**190 windows, 72 panels, 9 stairs**).
> - **Verticality/rotation furniture:** **9 stair tools, 4 up-launchers (jump pads), 4 push-launchers, 6 teleporters, 5 ability spawners** — L_Expanse buys its tier movement with launchers + teleporters + stairs.
> - **Playable-space shaping:** **198 `BlockingVolume`**, and nav authored with **1 `NavMeshBoundsVolume` + 13 `NavModifierVolume` + 5 `NavLinkProxy`**.
> - **Economy/POI spread:** ~**36 weapon/ability spawners** (18 `AFLWeaponSpawner` + 18 `B_WeaponSpawner` + 5 ability) distributed across the map; **4 `AFLExtractionZone`** (the INFINEON wash).
> - **Lighting rig:** 26 `RectLight`, `SkyLight`, `SkyAtmosphere`, **`ExponentialHeightFog` (cheap fog — NOT volumetric)**, 2 `PostProcessVolume`, 3 `SphereReflectionCapture`. Organized into **data layers: `LayoutModelA` / `Gameplay` / `ExtraSpawn` / `Lighting`.**
>
> Arena_04 **keeps** the two-base-symmetric, tiered, looping, 16-player skeleton, the **data-layer organization** (Layout / Gameplay / ExtraSpawn / Lighting), and the **BlockingVolume + NavModifier + NavLinkProxy** authoring discipline; **scales up** footprint and verticality (flanks + launchers → three explicit tiers); **changes** the loop to vertical-between-tiers, the look to neon-glass, and — critically — **the fog model from cheap `ExponentialHeightFog` to true volumetric** (which is exactly why §8's perf budget is a real delta, not an aspiration — L_Expanse deliberately avoided volumetric). **No AFL asset references `/ShooterMaps/**` — AssetReferenceRestrictions.**

> **Traversal is load-bearing, not an appendix.** Vault, Climb, Grab, and Sprint live in §4 (power positions) and §5 (sightlines). Tier-C verticality is their natural home: climbing IS the exposed counter-route between tiers, vaulting IS how you cross an exposed tier under fire, sprinting IS the tempo decision across the long lower lanes. That they unblock three deferred AI abilities is a consequence of the map's design — **any section that reads as existing FOR THE BOTS is rewritten.**

---

## 1. Identity
- **Name:** Arena_04 · **Tier:** C · **UE5 level:** one level, **World Partition** (mandatory at ~150–180 m — spec §5 "WP for large Tier C").
- **Sizes hosted:** 5v5 → 8v8 (up to 16). Adjacent-band share (7v7↔8v8) per spec §4; non-adjacent never share.
- **`LyraExperienceDefinition` variants** (spec §5, `map × size × ruleset`). **⚠ DUAL-GE REQUIREMENT (operator, 2026-08-03, [[project-ironics-dual-ge-requirement]]):** this map — like all maps unless stated — must be playable in **BOTH the Haywire GE and the ProMod GE**. So each size hosts a variant per GE:
  - `EXP_Arena04_5v5_Haywire` · `EXP_Arena04_5v5_ProMod`
  - `EXP_Arena04_8v8_Haywire` · `EXP_Arena04_8v8_ProMod`
  - (naming TBD against the existing GE assets; 6v6 / 7v7 fold onto the nearest via density telemetry, spec §4 / §10.3 — not pre-committed here.)
  - Offline = the same Experiences with Lyra bot-fill, no matchmaking ticket (spec §1, §5).
  - Greybox flow, spawns, extraction, and nav must satisfy **both** rulesets; the operator Experience-wiring step wires both GEs.
- **Signature identity:** **NEON-GLASS SHOWCASE** — neon glass walls, volumetric neon cloud, ultra laser-tag feel, tiered-and-looping with MC-Escher flavor (see §8, §10). This is the flagship look; BR_36 inherits it.
- **Source:** tracker Arena_04 (AFL-1801), re-cast from "energy storms" to the neon-glass showcase per operator ruling 2026-08-02.

## 2. Footprint & density
- **Footprint (calibrated to measured L_Expanse):** target **~140 m long × ~90 m cross, ~20–24 m vertical across 3 tiers** (well inside the spec §3 Tier C ~100–200 m band). The reference venue measured **~133 × 50 m geometry × ~23–28 m vertical** in the full all-cells pass (gameplay envelope ~116 × 43 m) — a *long, narrow* ~2.6:1 two-base lane map. Arena_04 **deliberately changes the plan aspect**: from L_Expanse's ~2.6:1 lengthwise form to a **squarer ~1.5:1 plan**, because a rotational tiered *loop* (§9) reads as a continuous circuit only if it isn't stretched into a single long lane. It is **only modestly longer than L_Expanse (133 → ~140 m) but much wider (50 → ~90 m) and comparably tall** — the added Tier-C room goes into the cross-axis and the tier stack, not lengthwise sprawl. Sized for 8v8; holds 5v5 at lower density (the §11 dead-zone metric guards against 5v5 feeling searchy).
- **Tier heights (calibrated):** measured L_Expanse inter-tier deltas are ~6–9 m; Arena_04 adopts **~8 m tier-to-tier** (Pit 0 → Bridges +8 → Ring +16, ±tuning) so a climb/vault reads as one storey and the vertical duels (§4) sit at a legible, proven height gap.
- **Density:** medium (spec §3 Tier C). The three-tier stack keeps effective density up by layering players vertically over a smaller floor-plan footprint than a flat 180 m map would need.
- **TTFC target:** ~12–25 s for all sizes (spec §3 Tier C). 8v8 trends low, 5v5 high. The long lower lanes (§5 sprint) are sized so first contact lands inside this window; a lane that pushes median TTFC past 25 s gets shortened or a mid-cover objective added (§11).

## 3. Flow
- **Layout:** **three vertical tiers**, looping — this is the Tier-C strength the spec §3 asks for ("multi-lane + meaningful verticality … rotation depth so a team can't lock the whole map"):
  - **Lower — The Pit:** the ground-floor central arena; three lower lanes feed it; holds the primary contested extract.
  - **Mid — The Bridges:** a rotational pair of mid-height spans + landings connecting the lanes and stairs; the rotation carriers.
  - **Upper — The Ring:** an elevated catwalk ring overlooking the Pit; the strongest vertical angle and the third extract.
- **The loop is VERTICAL, not just horizontal:** you rotate Pit→Bridge→Ring→Pit via stairs, climbs, and drops, AND laterally around each tier. No single team can lock the map because holding a tier costs you the tier above and below (each tier is overlooked by the next).
- **No dead-ends** (spec §6): every tier has ≥2 up-routes and ≥2 down-routes; losing the grand stair never severs a tier (the climb routes + the opposite-side stair carry it).
- **Tier connectors (design choice vs. the measured reference):** L_Expanse connects its layers with **launchers (jump pads) + teleporters + stairs** (census: 8 launchers, 6 teleporters, 9 stairs). Arena_04 **deliberately favors the traversal kit — climb / vault / stairs / drops** — as the connectors, because those are *skill-expressed, counterable* routes (a climb is exposed; a teleporter is instant and safe). **Up-launchers MAY be retained** as one fast, readable connector to the Ring where a climb would be too slow, but teleporters are declined for a ranked-integrity map (instant, un-counterable repositioning undermines the tier-control fight). This keeps verticality a movement-skill contest, which is also why the movement kit lives here honestly (§4/§5).
- **Loop diagram (greybox intent — side elevation + plan; 180° rotational):**
  ```
  SIDE ELEVATION (the vertical loop; heights calibrated to measured L_Expanse ~8m tiers):
   +16m [==== UPPER: THE RING (catwalk, extract-3) ====]
          |  \climb/     |grand stair|     \drop/  |
    +8m [== MID: BRIDGES (rotational pair, extract-2) ==]
          |  \climb/     |grand stair|     \drop/  |
     0m [======= LOWER: THE PIT (extract-1) =========]

  PLAN (lower tier, 180° rotational — base A rotates onto base B):
        [Base-A spawns]                      [Base-B spawns]
              \                                    /
           lane-1 --\                        /-- lane-1'
           lane-2 ------ [ THE PIT ] ------ lane-2'
           lane-3 --/          |           \-- lane-3'
                          (Bridges + Ring stack above the Pit)
  ```

## 4. Power positions (each with its flank/counter — no uncontested map-spanning sightline)
**Traversal counter-routes specified inline — this is where Climb, Vault, and Grab live by design.**

1. **The Ring (Upper)** — the elevated catwalk overlooking the Pit; the map's strongest vertical angle and the site of extract-3.
   - **Counter:** rotationally self-countering — each Ring segment is seen by the opposite segment across the Pit gap; and the Ring cannot see into a spawn. A team on the Ring dominates the Pit **only until** the Pit team takes the Bridges beneath and contests the up-routes.
   - **CLIMB (designed here):** the fast way onto the Ring is an **exposed climbable glass face, ≤80 cm approach**, versus the slow, safe grand stair. **Climbing IS the counter-route** — it's how a pressured team retakes the Ring quickly at the cost of exposure. Without it, Ring control is too cheap to hold; the climb keeps the top tier contestable every fight.
   - **VAULT (designed here):** **40–130 cm neon-glass parapets (≤100 cm approach)** ring the catwalk so a player under fire from the opposite segment **vaults behind cover to break the cross-Pit LOS** — vault-to-survive the vertical duel.
2. **The Pit (Lower-Central)** — the ground arena holding the primary contested extract; the map's focal fight.
   - **Counter:** **too exposed to camp** — it is overlooked by both Bridges and the whole Ring, so holding it means winning the tiers above, not sitting in it. The volumetric cloud gives *transient cosmetic concealment but is NOT cover* (§8 — a shot passes through it).
   - **VAULT (designed here):** staggered **40–130 cm glass cover clusters (≤100 cm approach)** across the Pit floor let a team **vault between cover to cross the open floor** while the Ring watches — vault-to-cross under a vertical threat.
3. **The Bridges (Mid, rotational pair)** — the mid-height spans, the rotation carriers between lanes and tiers.
   - **Counter:** each Bridge is **undercut by the Pit and overlooked by the Ring** (a two-way vertical squeeze), and losing one Bridge never severs the loop (the other Bridge + the grand stairs carry rotation). No Bridge holds a line into a spawn.

- **GRAB (designed here):** a single **grabbable neon-glass shard** sits at the Mid-tier extract-2. The Bridge extract is exposed to the Ring directly above; a channeling team can **grab-and-drag the shard to plug that vertical angle** for the duration of the bank. It is placed **with a reason bound to the win-tension beat** (channeling exposed from above, spec §2/§6) — not scattered as a physics toy. One at greybox; contest-rate telemetry (§11) decides a second.
- **SPRINT** is a §5 tempo decision, sited on the lower lanes — see §5.
- **Hard rule honored (spec §6):** no sightline spans the map uncontested; the tiered stack means the longest angles are vertical (Ring→Pit), each countered by a climb flank onto the shooter's tier.

## 5. Sightline bands (exercise the full laser roster) + SPRINT
- **CQB:** the Ring catwalks, extract interiors, stair landings, Bridge mouths, and the transient behind-glass pockets — pulse/auto-fire territory.
- **Mid:** the Bridges, tier-to-tier verticals, and Pit crossings — the bread-and-butter pulse range. **The defining Tier-C character: engagements happen ACROSS tiers (up/down), not only horizontally.**
- **Long:** the **Pit is the one long-sightline space**, but it is **self-limited** — tiered glass cover breaks it into segments and the (cosmetic) volumetric cloud caps *practical* engagement range without blocking shots (§8). The one controlled long angle is Ring→Pit, countered by the climb flanks (§4.1). This rewards the **charge/beam** range without an uncounterable perch.
- **SPRINT (designed here):** the three lower lanes are sized long (~50–60 m base→Pit) so **sprinting between tiers/lanes is a real decision at Tier-C scale, not decoration** — sprint to reach the Pit or a stair first, at the cost of arriving unable to immediately fire (sprint→ADS transition). The long lane length is what makes the TTFC window 12–25 s (spec §3) and what makes "sprint the safe outer lane vs. fight the short central approach" a genuine tempo choice. Sprint distance is tuned so time-saved ≈ the window to contest a tier before it's locked.
- Layered cover at CQB/mid/long is a spec §6 fundamental — every band present so the whole weapon roster has a home.

## 6. Extraction (the universal primitive — spec §2 Large-Team = 2–3 distributed zones)
Three **distributed** zones, one per tier band, to enable simultaneous team objectives + flanking denial (spec §2):
- **Extract-1 — Pit (Lower, central, contested):** highest Watts payout. Channeling here is the win-tension beat (spec §2) — exposed from both Bridges and the whole Ring while you hold it. The Pit vault cover (§4.2) is the counterplay.
- **Extract-2 — Bridge (Mid, peripheral, safer/slower):** lower payout, the counterplay zone (spec §2 "optional peripheral safer-but-slower"). Exposed to the Ring above — the grabbable shard (§4) is its designed counter.
- **Extract-3 — Ring (Upper, elevated, high-risk-to-reach):** high payout for the exposure of banking on the top tier; reaching it fast means the climb (§4.1), i.e. banking here is a commitment.
- **5v5 ships 2 zones** (Pit + one of Bridge/Ring) to keep the smaller team's objectives concentrated; **8v8 ships all 3** (spec §2 scales zones with count). Distribution lets two objectives run at once so no team locks the map.
- **Payout follows risk** (spec §2) — Pit ≈ Ring > Bridge. Exact Watts owned by the economy spec, not here.
- BR collapse interaction: **N/A** (Tier C, no zone collapse).

## 7. Spawns (authored deliberately — the goalvalid disambiguator)
Arena_04 is a **goalvalid disambiguator**: round-1 `goalvalid` = 72 % on `L_BagMan_Greybox` (candidate cause spawn geometry or navmesh coverage, unconfirmed). Multi-tier nav makes this map the sharpest test — vertical objectives are exactly where incomplete nav strands bots.

- **Two rotationally-placed base spawn areas** (180°), team-aware selection (spec §6 anti-spawn-camp). **Reuse the proven L_Arena_01 side-tag pattern** — `AFL.Spawn.Side.0` / `AFL.Spawn.Side.1` on the starts, read by the size-agnostic selector (`AFLPlayerSpawningManagerComponent`), spatial split by base. No new selector.
- **Spawn count:** ≥ 16 starts, **8/side** for 8v8 (covers 5v5–7v7). In line with proven practice — the L_Expanse all-cells pass measured **20 starts (10/side)** on the reference venue; Arena_04 wants 16+ for a clean 8/side. **Adopt L_Expanse's proven `ExtraSpawn` data-layer pattern:** a base set of starts always loaded, with the surplus (for the higher bands) in an `ExtraSpawn` data layer toggled per Experience — the measured way L_Expanse scales starts across sizes. **No untagged starts** (an untagged side falls back to ALL starts and defeats separation — the documented L_Expanse footgun).
- **No enemy-LOS spawn** (spec §3): a fresh spawn never has line into an enemy. Spawn exits feed **≥2 of the three lower lanes** so a single choke can't spawn-trap. Spawns are **lower-tier only** — no spawn on the Bridges or Ring (fresh players enter the vertical loop from the ground, never dropped into an exposed high tier).
- **`Gameplay.DamageImmunity` ≈ 2.75 s** on spawn (GE_SpawnIn, montage-tied, removed on spawn-montage complete — spec §7).
- **Side/team swap each round (or at half)** per spec §1.1 — side balance tracked in telemetry (§11 integrity).
- **NAVMESH COVERAGE (stated, so it can be evidence):**
  - Full RecastNavMesh coverage of **100 % of the walkable area on ALL THREE TIERS** — both bases, all lower lanes, the Pit, both Bridges, and the full Ring. No unreachable islands, no unnavigated tier.
  - **Every vertical route carries a NavLinkProxy** — grand stairs (walkable ramp nav), climb faces, vault cover, and one-way drops — so **bots can path the whole vertical loop.** This is the load-bearing part: the most likely systemic cause of a low goalvalid on a tiered map is bots that cannot reach an upper-tier objective. NavLinks make the tiers bot-reachable by design.
  - **The volumetric cloud is NOT a nav or collision modifier** — it never affects pathing (§8, cosmetic).
  - **Authoring set (matches measured L_Expanse practice):** shape playable space with `BlockingVolume` (L_Expanse: 198), tune bot pathing with `NavModifierVolume` (L_Expanse: 13), bridge tiers with `NavLinkProxy` (L_Expanse: 5), inside a single `NavMeshBoundsVolume`. This is the proven venue's exact nav toolkit — Arena_04 uses more `NavLinkProxy` because it has more vertical routes to bridge.
  - **WP note:** nav is built per-cell; verify the nav is complete with all objective-holding cells loaded (the documented L_Expanse WP inspection discipline).
  - **Exit evidence:** with authored spawns + full multi-tier nav, round-1 `goalvalid` must recover to the §11 window. Recovery ⇒ the greybox 72 % was greybox-specific (thin nav / 4-prop scene). A miss on a fully-navmeshed 3-tier map ⇒ the cause is systemic (spawn/objective wiring) and escalates beyond map design.

## 8. Signature mechanic — NEON-GLASS SHOWCASE (authoritative-vs-cosmetic split + perf constraint, per spec §11.8)
The operator states the identity is **largely rendering**. So this section states **plainly what is authoritative gameplay and what is cosmetic** — because a hidden gameplay element dressed as art, or art mistaken for cover, is a fairness bug.

- **The gameplay signature is the TIERED-LOOPING VERTICALITY itself** (§3/§4/§5) — that is the mechanic, and it is **static geometry**, not replicated state.
- **Neon glass walls — GAMEPLAY (static):** solid, LOS-blocking, opaque-to-weapons collision geometry that is *visually* translucent-neon. **A glass wall stops a shot and blocks sight exactly like an opaque wall** — the translucency is cosmetic; the neon edge-framing (§10) makes it read as solid so no player wastes a shot trying to fire through it. **No glass destruction at greybox** (destruction = replicated mid-match state → deferred; if ever added, see the AFLNetTypes rule below).
- **Volumetric neon cloud — COSMETIC ONLY:** atmosphere. **It does NOT block shots, does NOT block LOS for gameplay, does NOT provide cover, and does NOT affect navigation.** A tracer and a hitscan pass through it unchanged. This is stated so it is never a hidden gameplay element and never confers advantage.
- **Server-authority / replication note (the `FNetSerializeScriptStructCache` category):**
  - **Arena_04 introduces NO new net-serialized gameplay mechanic.** Verticality is static geometry; glass is static collision; the cloud is client-side rendering; extraction uses the existing base-loop replication. So there is **no new `FNetSerializeScriptStructCache` surface** — the honest §11.8 answer, not a mechanic bolted on to have one.
  - **Pre-committed rule (stands even though nothing triggers it yet):** if any dynamic element is later added — destructible glass, gameplay-affecting cloud density, animated tiers — **its net-serialized struct MUST live in `AFLNetTypes` (always-loaded), NEVER in a GameFeature module**, and MUST be validated in a 2+-client networked PIE (single-client cannot catch the desync category). Recording it now so a future addition inherits the rule.
- **PERF CONSTRAINT — volumetric cloud at ~150–180 m across 16 players (stated as a constraint, not an aspiration):**
  - **Measured delta:** the L_Expanse census shows the proven venue ships **`ExponentialHeightFog` (cheap, ~free) and NO volumetric fog** at this scale. Arena_04's volumetric cloud is therefore a **deliberate cost the reference map declined to pay** — it is on us to prove it fits the budget, which is why it is validated *during greybox*, not assumed.
  - **Hard GPU budget:** the volumetric fog stays inside a fixed ms ceiling at target spec (starting target ≤ ~1.5 ms GPU; froxel/volumetric-fog grid resolution capped; **no per-dynamic-light volumetric scattering on the 16 players** — the cloud is a bounded, largely static atmospheric volume, not 16 moving lit volumetrics). Measured with `stat GPU` / `profilegpu` (instruments that exist).
  - **Scalability without gameplay change:** because the cloud is cosmetic (see above), low-spec scalability may **reduce or drop** it freely — and doing so **confers no competitive advantage**, because reducing cosmetic fog does not change LOS, cover, or nav. This is the fairness reason the cloud is cosmetic: a player who turns fog down must not thereby see an enemy the fog was "hiding," because the fog never hid anyone for gameplay.
  - **Neon readability survives the fog (a constraint, not a hope):** the cloud **density ceiling is set by "a weapon beam must stay legible across the longest sightline (Ring→Pit),"** not by art preference. Weapon beams/tracers use high-intensity emissive + bloom tuned to read *through* the volumetric; environment + cloud hue are reserved **off** the weapon-beam hues (no beam-on-cloud color collision); rim light on silhouettes so robots read against the fog. If a density that looks good hides a tracer at range, the density loses — legibility is the gate.

## 9. Symmetry
- **ROTATIONAL (180°)** — permitted for Tier C (spec §3 "mirror or rotational"). Chosen over mirror because **the identity is looping**, and 180° rotation makes the tiered loop read as one continuous circuit rather than two reflected halves; it also spreads the two bases across the vertical stack more naturally at 16-player scale. Rotational removes side-advantage by construction; residual bias is measured and corrected by the §1.1 side/team-swap + telemetry (§11 integrity). (Mirror was the right call for the ranked Tier-B baselines; rotational is the right call for the looping Tier-C showcase.)

## 10. Readability (beam + silhouette + the Escher constraint — spec §11.10)
- **Legibility is the hard constraint, above the aesthetic.** The operator's rule: MC Escher is **flavor, never the organizing principle** — impossible-looking stairs and tiers are a **visual read only.** **Every playable path is a real, walkable, obviously-connected route.** The illusion lives in ornament, backdrop geometry, and non-playable framing — **never in the route a player must read to survive.** If a stair looks impossible AND a player can't tell where it goes, it's re-cut. A player never dies to confusion about whether a surface is walkable.
- **Neon glass reads as solid:** translucent glass walls (§8) get bright neon edge-framing so players read them as cover/walls, not shoot-through openings — the cosmetic transparency never causes a wasted shot or a false "I can see them so I can shoot them."
- **Beams legible through the cloud** (the §8 density-ceiling constraint): weapon beams/tracers stay high-intensity and hue-reserved so they never wash out in the volumetric neon; the cloud hue stays off the weapon hues.
- **Low ambient + rim light** so emissive reads and robot **silhouettes** (the proven skin/edge/body identities) pop against a neon-saturated environment — the risk on a neon-glass map is silhouette-lost-in-neon; rim light is the mitigation.
- This map **sets** the house readability standard (BR_36 inherits it); the art pass must not regress silhouette or beam readability, and must hold the Escher-flavor-not-at-the-cost-of-legibility line.

## 11. Telemetry hooks + greybox exit criteria (spec §6) — instruments that EXIST only
**Instruments used (all real): the `AFL-0213` telemetry substrate (map-coordinate events), `MoveProbe` (traversal-ability + movement-state capture), and standard UE profiling (`stat unit` / `stat GPU` / `profilegpu`).** No criterion below needs an unbuilt instrument.

**Heatmaps / captures:**
- **Kill/death density** — per lane + per tier + per power position (telemetry substrate) — overpowered-angle + dominant-tier detection.
- **Traversal density** — per region **and per tier** (telemetry substrate) — cold/dead-zone AND dead-tier detection.
- **TTFC distribution** — per size (telemetry substrate).
- **Extract outcomes** — per zone: contest rate, hold-vs-deny, channel-success (telemetry substrate).
- **Traversal-decision usage** — vault-at-Pit/Ring cover, tier-climb-under-contest, grab-shard-at-extract, sprint-lane commit (**MoveProbe**). Captured as **design validation** — proof the vertical counter-routes are live decisions, not dead geometry.
- **Perf** — frame time + volumetric GPU cost at 16 players (`stat unit` / `stat GPU`).

**Greybox EXIT criteria (starting windows — tune per spec §3/§6; the `✅ watched in PIE` gate). No art pass until ALL are hit (spec §6.5):**

| Metric | Target window | Instrument |
|---|---|---|
| Median TTFC (5v5 and 8v8) | inside 12–25 s | telemetry |
| Single power-position kill share | ≤ ~35 % of kills (no dominant angle) | telemetry |
| **Per-tier occupancy** | **no tier below a min-traffic floor** (no dead tier — the vertical loop is used) | telemetry |
| Dead-zone traversal | no playable region below a min-traffic floor | telemetry |
| Pit-extract contest rate | ≈ 40–70 % contested | telemetry |
| Hold-vs-deny on contested extract | ≈ 50/50 after tuning | telemetry |
| Simultaneous-objective rate | measurable share of rounds with 2 zones contested at once (proves distribution prevents map-lock) | telemetry |
| Side/team win-rate balance (post-swap) | within ~±5 % | telemetry |
| **Tier-climb-under-contest usage** | climb counter-route taken in a meaningful share of Ring/Bridge retakes | MoveProbe |
| **Vault-at-cover usage** | above a min floor at Pit + Ring cover (vault-to-cross / vault-to-survive is used) | MoveProbe |
| **Sprint-lane commit** | sprint used on the lower lanes above a min floor (the tempo decision is live) | MoveProbe |
| **Volumetric GPU cost @ 16p** | inside the §8 ms ceiling (≤ ~1.5 ms target) across the map | stat GPU / profilegpu |
| **Frame time @ 16p** | inside the platform frame budget (no volumetric-driven hitching) | stat unit |
| **Round-1 `goalvalid`** | ≥ ~95 % (the disambiguator target — recovers from the greybox 72 %; a miss on a fully-navmeshed 3-tier map escalates as systemic, §7) | telemetry |

---

## What this brief commits the map to host (the proven + newly-exercised pillars)
- **Laser roster** → the CQB/mid/long + **vertical** sightline bands (§5), legible through the cloud (§8/§10).
- **Part-token loot + carry-value** → the three-zone extraction real estate (§6) + the carried-value HUD already proven.
- **Dismember / death** → the combat the arena frames, now across three tiers.
- **Skin / edge / body identity** → the readability section (§10), stress-tested against a neon-saturated environment.
- **The round → extract → bank loop** → inherited proven from Arena_01; Arena_04 stresses it at **Tier-C 16-player density**.
- **Movement kit (Vault / Climb / Grab / Sprint)** → designed into §4/§5 as vertical counter-routes and tempo, unblocking three deferred AI abilities as a consequence of the tiered design, not as a test harness.
- **The IRONICS house look** → neon glass + volumetric cloud + Escher-flavor tiers; **the flagship aesthetic BR_36 inherits** (§8/§10).

## Gate
Per spec §11: **this brief → operator approval → greybox → telemetry (§6) → balance → art → PIE sign-off.** A re-sent brief is not approval; disk state is verified before build. **On approval**, greybox step 3 (the Blender blockout) ships as a ready-to-run prompt using the proven gib-extraction FBX export settings as the bridge contract; UE import/placement + the multi-tier WP nav + spawn authoring run on the editor bridge. **Do not build geometry before approval — that is the §11 gate.** WP + 16-player perf mean the volumetric budget (§8) is validated during greybox, not deferred to art.
