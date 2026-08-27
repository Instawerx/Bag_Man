# IRONICS_HUB_MAP_BUILD_SPEC

**Status:** NEW. The build order for the thing this phase *is*: take the **Military Mega Base Pack
demo map**, make it ours, and turn it into **Outpost Earth MWR** — the walkable, social base whose
buildings and tents are the doors to every IRONICS system.
**Date:** 2026-08-27
**Operator intent (fixed, from `Lobby_Upgrade_Doc.docx`):** "Main Lobby map we are to take, and upgrade
for our use — Military Mega Base Pack Demo Map — and rename for our Game." Walkable and interactable;
chat; show players and friends; wear accessories and carry weapons; no fire except the Shooting Range;
base has destinations that transport you to experiences: PX Store, Lab = Robo Labs, Patriot Café, EM
Lounge, Deployments (Staked / League), tents/zones → mini map / partition map / Tournament / experience.
**Companions:** `IRONICS_LOBBY_HUB_SSOT.md` (architecture) · `IRONICS_LOBBY_HUB_ROADMAP.html` (H1 is this
spec) · `IRONICS_LOBBY_HUB_TASKS.md` (AFL-34xx below) · `IRONICS_LOBBY_HUB_CLAUDE_CODE_BRIEF.md` (lanes).
**Lane:** almost all of this is **CC-E** — Claude Code on the editor checkout through the direct editor
connection, one session at a time, PIE watched by the operator. Code rows are CC-W and are small.

---

## 0 · What "done" looks like for the map

You stand at the spawn plaza on Outpost Earth as your own robot, wearing what you own, carrying your
loadout. Another player walks past; you see their build. You walk into the PX and the racks are lit
with real catalog items; you walk into Robo Labs and the creator station is there; the mess hall is
the Patriot Café; the range door takes you to the range and back; the deployment gate opens the
League door you already have. Nothing fires in the base. The whole thing is one map, one server,
no loading screens between zones. That is the H1/H2 proof, and everything below exists to get there.

---

## 1 · Import — get the pack into the project the right way

The pack is a Fab product; "Add to Project" from the Epic Launcher/Fab drops it into `Content/<Pack>/`.
That is `/Game`, which is where new AFL content must **not** live. So the import is two steps: the
operator adds it, Claude Code migrates it.

| Step | Who | What | Proof |
|---|---|---|---|
| 1.1 | OP | Fab → Add to Project (`Bag_Man`, UE 5.6). Record the pack's version and size before adding. **LFS budget check:** 250 GB quota; note the pack's on-disk size against current usage | Launcher shows it in the project; size recorded on the tracker |
| 1.2 | CC-E | Open the editor. Content Browser census of `Content/<Pack>/`: the demo map asset name, map type (single level / sublevels / World Partition), static-mesh count, material count, any Blueprints, sequences, Niagara, sounds. Written to `Docs/Hub/HUB-MAP-CENSUS.md` | Census committed (doc) |
| 1.3 | CC-E | **Migrate** `Content/<Pack>/` → `Plugins/GameFeatures/AFLHub/Content/MegaBase/` (Asset Tools migrate, then fix-up redirectors, then delete the `/Game` copy). The pack's own folder structure is kept under `MegaBase/` so future pack updates diff cleanly | Zero assets left under `/Game/<Pack>`; zero broken references (Reference Viewer on the map) |
| 1.4 | CC-E | Duplicate the demo map to `AFLHub/Content/Maps/L_AFL_OutpostEarth`. The pack's original demo map stays untouched under `MegaBase/` as the reference copy | Both maps open |
| 1.5 | OP + CC-W | Content commit: the migrated pack (LFS) as **one** commit with the count stated first; the census doc as its own commit | Pushed, triple-hash |

`[VERIFY at 1.2]` — the pack's UE version (5.3/5.4 packs need a load-and-save pass under 5.6; note any
material/Nanite warnings), whether the demo map is World Partition, and whether it ships its own
lighting scenario. These three facts set the §6 optimisation path.

---

## 2 · Sanitise and rename — make it ours

Helper §8 "Actor Sanitation", done first because nothing can be placed on a map still running someone
else's demo.

| Step | Who | What | Proof |
|---|---|---|---|
| 2.1 | CC-E | From the census: delete every demo AI Blueprint, ticking target actor, level sequence / cinematic trigger, physics prop, demo HUD/widget spawner, demo GameMode override. Count before, count after, per class | Counts on the tracker; PIE opens with no log lines from deleted classes |
| 2.2 | CC-E | `WorldSettings` → `ALyraWorldSettings::DefaultGameplayExperience = B_AFL_Experience_Hub` (the hub experience asset from AFL-3013). Remove any pack GameMode override | Map PIEs on the hub experience, hero pawn spawns |
| 2.3 | CC-E | Rename pass: map = `L_AFL_OutpostEarth`; player-facing name "Outpost Earth MWR" in the map's display-name/loading metadata; any pack-branded signage textures noted for replacement in §5.7 | No "Mega Base" string is player-visible |
| 2.4 | CC-E | Kill-zone / out-of-bounds: `KillZ` set; a blocking volume ring at the base perimeter so nobody walks off the demo's edge | PIE: cannot leave the base |

---

## 3 · Assign the base — which building is which zone

This is the step that turns the pack into *your* base, and it is a **ruling, not an agent's call**.
Claude Code walks the map, photographs every distinct structure (PIE screenshots), and proposes the
assignment; the operator ratifies or reassigns; only then is anything placed.

Default heuristic (military-base feature → IRONICS zone) — the proposal starts here and is corrected
by what the map actually has:

| IRONICS zone | Look for on the map | Why |
|---|---|---|
| **Spawn plaza (Main Zone)** | Parade ground / central yard with sight lines to the other zones | Players spawn where they can *see* the doors |
| **PX Store** | The largest interior with open floor — a hangar, warehouse, or supply depot | Racks, pedestals, jewellery counter, mirrors need floor and wall |
| **Robo Labs** | Tech/comms/HQ building, or a clean-room-looking interior | The creator station wants a lit, enclosed space with a mirror wall |
| **Loadout Barracks** | The barracks block (bunks/lockers) or the armoury | Named in your doc; lockers read as "your kit" |
| **Patriot Café** | Mess hall / canteen | Seating clusters = social club |
| **EM Lounge** | A second social interior — rec room, bar, briefing room | Second club flavour; must not share a wall with the Café (visibility mask is per interior) |
| **Deployments** | Main gate, helipad, or motor pool — a threshold you *leave through* | The League/Staked door lives at the way out |
| **Shooting Range door** | The pack's range, if it has one, becomes the door location; the range itself is its own map (§5.6) | Operator ruling: range is a separate map |
| **Tournament / Mini Game / Assigned Match tents** | Actual tents or the pack's field structures at the base edge | Your doc: "player enters tent/zone → transports" |
| **Landing backdrop camera** | The single best establishing shot of the base — likely from the gate looking in, or a tower | Landing Page "3D Map Shot" |

Output: `Docs/Hub/HUB-ZONE-ASSIGNMENT.md` — one row per zone: structure name on the map, world
location, interior/exterior, door location(s), screenshot, ratified Y/N. Everything in §4–§5 places
against this table by location, never by guess.

---

## 4 · Skeleton — spawn, zones, doors (H1 ↔ H2)

| Step | Who | What | Proof |
|---|---|---|---|
| 4.1 | CC-E | PlayerStarts (≥ 16, staggered) on the spawn plaza; a `NavMeshBoundsVolume` covering only the plaza for now | 2 clients spawn without overlap |
| 4.2 | CC-E | One `AAFLHubZoneVolume` per interior zone (PX, Labs, Barracks, Café, EM Lounge, Deployments plaza), sized to the structure from §3, tag per zone. Exterior = Main by default (no volume) | `AFL_HUB[Zone]` markers on entry/exit, both clients |
| 4.3 | CC-E | One `AAFLHubDestinationVolume` at each real doorway / tent flap / gate from §3, `DestinationId` per row of `DA_AFL_HubDestinations`; a world prompt at each (zone prompt widget). Unproven backends read `Disabled` | Walk to each door → correct prompt |
| 4.4 | CC-E | Streaming/relevancy check: with 2 clients in opposite zones, confirm the net profile's per-zone cull is doing what §6.4 expects | Replicated-actor count per client on the tracker |

---

## 5 · Dress each zone — the build list per destination

Each zone is built **canary-first**: one rack, one pedestal, one mirror, one seat cluster, proven, then
scaled. Names in `code` are the classes from the SSOT; placement is CC-E; the classes are CC-W.

### 5.1 PX Store
- `AAFLDisplayRack` ×4 to start — **weapons wall**, **mask wall**, **jewellery counter**, **robots** —
  each with `SpawnSlots` laid along the pack's existing shelving/crates so pedestals sit where a
  shopper expects them. Filter per rack from the catalog; pedestals spawn at BeginPlay from catalog
  rows, never hand-placed (the proven weapon-spawner pattern, recoloured to brand).
- Pad MI: base `#222A3A`, emissive `#1E5AFF`; `#FF00D5` pad variant on premium/staked rows.
- `AAFLHubPreviewAnchor` (Portrait + CombatRange) per rack front; `AAFLHubMirror` ×2 (jewellery
  counter, mask wall) — capture off until the local player steps in front.
- Sticker rack position reserved; reads `Disabled` until CC-7.
- Shelving meshes → `ECC_HubRetail` object channel (helper §8 collision optimisation).

### 5.2 Robo Labs
- The **creator station**: a `AAFLHubPreviewAnchor` pair facing a mirror wall (two `AAFLHubMirror`,
  one wide), a floor marker, the Labs door row → `AFLW_Creator` (Track C3; `Disabled` until then).
- Signage: ROBO LABS (§5.7).

### 5.3 Loadout Barracks
- `AAFLDisplayRack` ×2 with filter `OwnedByLocalPlayer` (weapons, cosmetics) along the pack's lockers;
  one anchor + one mirror; door row → `AFLW_Loadout` (Track C4).

### 5.4 Patriot Café / EM Lounge
- Seating clusters from the pack's mess/rec props (no new meshes); a club door
  (`AAFLHubDestinationVolume`, action `JoinClub`, flavour per lounge); a chat/nameplate-friendly open
  floor. Interiors are separate structures (visibility mask is per interior).

### 5.5 Deployments
- The gate/helipad/motor-pool threshold from §3; door row → the existing League/Staked door widget
  (H2.2); space reserved for the Track S leaderboard board (a `Disabled` row until S).

### 5.6 Shooting Range — separate map
- `L_AFL_ShootingRange`: built from the pack's range pieces (or greybox if the pack has none), own
  experience with no `NoFire` GE, existing target dummies, an exit door back to the hub. The hub's
  range door is an `ExperienceTravel` row. This is the H2 travel canary.

### 5.7 Signage and branding
- Every zone gets a sign: **PX**, **ROBO LABS**, **BARRACKS**, **PATRIOT CAFÉ**, **EM LOUNGE**,
  **DEPLOYMENTS**, **RANGE** — Orbitron on `surface-card` with an accent emissive rim; decal or
  mesh via the `afl-blender-bridge` signage flow. Pack-branded textures replaced. "Outpost Earth
  MWR" at the gate. No new brand values (SSOT brand lock).

### 5.8 Landing backdrop
- A `CineCameraActor` at the §3 establishing-shot location; Landing Page either streams
  `L_AFL_OutpostEarth_Backdrop` (a low-LOD sublevel of the base, no pawns) or shows a captured still —
  chosen by measured cold-load time (< 3 s).

---

## 6 · Optimise — the pack was a demo, the base is a server map (H5)

Helper §8, in the order that measurement justifies:

| Step | What | Trigger |
|---|---|---|
| 6.1 | **HISM conversion** — Merge Actors on prop clusters (crates, beams, wall decor, lockers) per zone | Draw-call count from the census over budget; numbers before/after on the tracker |
| 6.2 | **NavMesh restriction** — bounds shrunk to the walkable paths between spawn plaza, PX, Labs, Barracks, lounges, Deployments | Always (after §4) |
| 6.3 | **Collision** — retail shelving on `ECC_HubRetail`; decorative props to no-collision where players can't reach | Always |
| 6.4 | **Streaming** — if the map is single-level: measure load and memory first; convert to World Partition only if it fails budget. If it is already WP: cell size tuned to zone footprints (SEAM RULE: stream within the hub, never load) | Measured |
| 6.5 | **Nanite / LOD** — enable Nanite on qualifying pack static meshes per `afl-asset-pipeline`; LOD groups on the rest | Census warnings |
| 6.6 | **Lighting** — keep the pack's lighting scenario if it holds under Lumen at target frame; otherwise one baked pass. Sky/time-of-day fixed (no dynamic cycle in v1) | Measured |
| 6.7 | **Server-side** — the dedicated server cooks the same map; confirm no client-only actors tick on the server (mirrors, anchors, preview instances are client-side by construction) | Server log ledger at H1.7 |

---

## 7 · Sequence and gates

```
M1 Import + migrate + census          (H1, first thing after the tree is clean)
M2 Sanitise + rename + hub experience  (H1)
M3 Zone assignment → operator ratifies (H1)   ← the ruling
M4 Spawn plaza + zone volumes          (H1)   → H1 gate: 2 clients on Outpost Earth, cosmetics, no fire
M5 Doors at real doorways              (H2)
M6 Range map + travel canary           (H2)   → H2 gate: full loop
M7 PX / Labs / Barracks / lounges dressed (H3, canary-first; opens on Track C3)
M8 Signage + landing backdrop          (H3–H6)
M9 Optimisation pass                   (H5, measured)
```

M1–M4 are the first four CC-E sessions of the programme, in that order, before any retail or social
code lands. They are the critical path and they are not interleaved with creator work.

---

## 8 · Tasks (AFL format, `AFL-34xx`)

```
[AFL-3400] Fab import + LFS budget
Type: Pipeline · Discipline: Art · Priority: P0 · Estimate: S · Sprint: HUB-H1 · Lane: OP
- [ ] Pack added to Bag_Man via Fab; version + on-disk size recorded; LFS usage before/after on the tracker
```
```
[AFL-3401] Pack census + migrate into AFLHub GameFeature
Type: Pipeline · Discipline: Art · Priority: P0 · Estimate: M · Sprint: HUB-H1 · Lane: CC-E · Depends On: AFL-3400, AFL-3010
- [ ] Docs/Hub/HUB-MAP-CENSUS.md: demo map name, map type (single/sublevel/WP), pack UE version, counts by asset class, lighting scenario, every BP/sequence/Niagara/sound listed
- [ ] Migrated to Plugins/GameFeatures/AFLHub/Content/MegaBase/; redirectors fixed; /Game copy removed; Reference Viewer clean on the map
- [ ] L_AFL_OutpostEarth duplicated from the demo map; original kept under MegaBase/
- [ ] Content commit (count stated first) + doc commit, separate
```
```
[AFL-3402] Sanitise + rename + hub experience on WorldSettings
Type: Pipeline · Discipline: Art · Priority: P0 · Estimate: M · Sprint: HUB-H1 · Lane: CC-E · Depends On: AFL-3401, AFL-3013
- [ ] Deletions per class with before/after counts; PIE log clean of deleted classes
- [ ] ALyraWorldSettings::DefaultGameplayExperience = B_AFL_Experience_Hub; pack GameMode override removed
- [ ] KillZ + perimeter blocking volume; no player-visible "Mega Base" string
```
```
[AFL-3403] Zone assignment proposal → operator ratification
Type: Research · Discipline: Design · Priority: P0 · Estimate: M · Sprint: HUB-H1 · Lane: CC-E (propose) · OP (rule) · Depends On: AFL-3402
- [ ] Docs/Hub/HUB-ZONE-ASSIGNMENT.md: one row per §3 zone — structure, world location, interior/exterior, door locations, PIE screenshot, proposed/ratified
- [ ] Operator ratifies or reassigns every row before AFL-3404 starts
```
```
[AFL-3404] Spawn plaza + zone volumes + nav
Type: Feature · Discipline: Art · Priority: P0 · Estimate: M · Sprint: HUB-H1 · Lane: CC-E · Depends On: AFL-3403, AFL-3015
- [ ] ≥16 staggered PlayerStarts on the plaza; NavMeshBounds on the plaza
- [ ] AAFLHubZoneVolume per interior zone from the ratified table; tags per zone
- [ ] Proof: 2 clients spawn, walk zone to zone, AFL_HUB[Zone] markers on both; feeds AFL-3017 (H1 gate)
```
```
[AFL-3405] Doors at real doorways
Type: Feature · Discipline: Art · Priority: P0 · Estimate: M · Sprint: HUB-H2 · Lane: CC-E · Depends On: AFL-3404, AFL-3020
- [ ] AAFLHubDestinationVolume at every door/tent/gate from the ratified table; DA rows per §4.3; prompts show; unproven rows Disabled
```
```
[AFL-3406] Shooting Range map from pack pieces   (see AFL-3025)
Type: Feature · Discipline: Art · Priority: P0 · Estimate: L · Sprint: HUB-H2 · Lane: CC-E · Depends On: AFL-3405
- [ ] L_AFL_ShootingRange built from the pack's range assets (or greybox); own experience; dummies; exit door; the H2 travel canary
```
```
[AFL-3407] PX Store dressing — canary first
Type: Feature · Discipline: Art · Priority: P0 · Estimate: L · Sprint: HUB-H3 · Lane: CC-E · Depends On: AFL-3030, AFL-3036, AFL-3231 (C3)
- [ ] One rack + one pedestal (Flag.Japan) + one anchor pair + one mirror proven; then weapons wall, mask wall, jewellery counter, robots racks; shelving on ECC_HubRetail; sticker rack reserved/Disabled
```
```
[AFL-3408] Robo Labs station · Barracks racks · Café + EM Lounge interiors · Deployments threshold
Type: Feature · Discipline: Art · Priority: P0 · Estimate: L · Sprint: HUB-H3 · Lane: CC-E · Depends On: AFL-3407
- [ ] Per §5.2–§5.5; each zone's door row switched from Disabled to its widget only when that widget is proven (C3/C4/C5, H2.2)
```
```
[AFL-3409] Signage + branding + landing backdrop camera
Type: Feature · Discipline: Art · Priority: P1 · Estimate: M · Sprint: HUB-H3 · Lane: CC-E (+ afl-blender-bridge for sign meshes/decals) · Depends On: AFL-3403
- [ ] Seven zone signs + gate lockup in brand tokens; pack-branded textures replaced; CineCameraActor at the ratified establishing shot; backdrop method chosen by measured load (<3 s)
```
```
[AFL-3410] Optimisation pass — measured
Type: Polish · Discipline: Art · Priority: P1 · Estimate: L · Sprint: HUB-H5 · Lane: CC-E + OP PIE · Depends On: AFL-3408
- [ ] §6.1–§6.7 each with a before/after number; WP conversion only if load/memory fails budget; server log confirms no client-only actors tick on the dedicated server
```

---

## 9 · Rules specific to the map

1. **The pack is a foundation, not a template to fight.** Use its buildings, props, lighting and range
   pieces as they are; add IRONICS on top (signs, racks, stations, doors). Don't re-art the base.
2. **Zone assignment is the operator's.** No volume or door is placed before `HUB-ZONE-ASSIGNMENT.md`
   is ratified.
3. **Content lives in `AFLHub`.** Nothing from the pack stays under `/Game`.
4. **Canary-first in every zone.** One rack proven before four; one mirror before three.
5. **Doors are honest.** A door whose backend is not proven reads `Disabled` with a prompt, never a
   stub.
6. **One map, one server, no loads between zones.** Streaming is by measurement; travel only at
   the hub↔range/match seam.
7. **Optimise from numbers.** Draw calls, load time, memory, replicated-actor counts — before/after
   on the tracker, never "feels fine".
