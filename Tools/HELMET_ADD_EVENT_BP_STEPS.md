# Helmet add-event — operator GUI node steps (S-ECON-CAT 4b, Step B)

**Target asset:** `B_BagMan_AssignCharacterPart` (Content/BagMan/Characters/) — it's a Blueprint child of
`ULyraControllerComponent_CharacterParts`, attached to the player Controller via the experience's
`GameFeatureAction_AddComponents` (entry [0]). This is the **canonical part-selector seam** — the robot
body is already added here via its `CharacterParts` CDO default (1 static entry). The helmet rides the
same seam, but **selection-driven** (read from the catalog) instead of a static default.

**Why operator-GUI and not MCP:** this is BP-graph *authoring* (adding + wiring new nodes), the MCP-walled
zone. The C++ half is done + committed (Build 1, `202a6242`): `ResolveCosmeticAsset` (BP-callable on the
catalog subsystem), `UAFLHelmetAsset` (the resolve target), `GetSelection()`/`HelmetId` (BP-readable on the
loadout component). You wire the graph below; everything it calls already exists.

**Prereqs that must exist before this graph works** (authored separately, MCP, when the mesh lands):
- `SM_AFL_Helmet_Visor_v1` (Tripo mesh) — Step A, network-gated.
- A helmet **part actor BP** (e.g. `B_AFL_Helmet_Visor`) — a plain `AActor` with a `StaticMeshComponent`
  carrying the visor mesh. This is `FLyraCharacterPart.PartClass`.
- `DA_AFL_Helmet_Visor01` (a `UAFLHelmetAsset` instance) — `PartClass` → `B_AFL_Helmet_Visor`,
  `SocketName` → `head`.
- Catalog row `AFL.Helmet.Visor01` / `Type=Helmet` / `Asset` → `DA_AFL_Helmet_Visor01`.

---

## The graph — node by node

Open `B_BagMan_AssignCharacterPart` → Event Graph. Build this off **Event BeginPlay** (the component's
BeginPlay; authority-gated so only the server adds — matches how `AddCharacterPart` is `BlueprintAuthorityOnly`).

### 1. Authority gate
- **Event BeginPlay** → drag off the exec.
- Add **`Has Authority`** (target: Self / the component's owning actor) → **Branch**.
  - *(Self is the controller component; `Has Authority` on `GetOwner` works. If `Has Authority` isn't
    directly available on the component, use `Get Owner` → `Has Authority`.)*
- Branch **True** → continue. **False** → do nothing (clients converge via the part's own replication).

### 2. Reach the loadout component → read HelmetId
- **`Get Owner`** (Self) → returns the **Controller**.
- From the Controller, **`Get Player State`** (or drag a `Get` on the Controller's `PlayerState`).
- From PlayerState, **`Find Component By Class`** → Component Class = **`AFLCosmeticLoadoutComponent`**.
  → returns the loadout component (promote to a local var `Loadout` if you like; null-check it).
- **`Is Valid`** on `Loadout` → Branch. Invalid → End (no loadout yet → nothing to add).
- From `Loadout`, call **`Get Selection`** (BlueprintPure; returns `FAFLCosmeticSelection` by value).
- From the returned struct, **`Break FAFLCosmeticSelection`** (or drag the `HelmetId` pin) → get **`HelmetId`** (FName).

### 3. Guard: only proceed if a helmet is selected
- **`Not Equal (Name)`**: `HelmetId` ≠ `None` → Branch. (An unset helmet → End; no helmet to add.)
  - *(In BP: `HelmetId` → `!= ` → default/empty Name. Or use `Is None`/compare to `None`.)*

### 4. Resolve the helmet asset via the CATALOG
- **`Get Game Instance Subsystem`** → Class = **`AFLCosmeticCatalogSubsystem`**.
  → returns the catalog subsystem. **`Is Valid`** → Branch (invalid/not-ready → End).
- From the subsystem, call **`Resolve Cosmetic Asset`** (the `ResolveCosmeticAsset` UFUNCTION):
  - **Cosmetic Id** = `HelmetId` (from step 2).
  - → returns a `UPrimaryDataAsset*`.
- **`Cast to UAFLHelmetAsset`** on the result.
  - Cast **Failed** → End (the id resolved to the wrong type / didn't resolve — a real miss; with the
    stopgap retired this is the loud failure, which is correct).
  - Cast **Succeeded** → `HelmetAsset`.

### 5. Build the FLyraCharacterPart from the helmet asset
- From `HelmetAsset`, get **`Part Class`** (TSoftClassPtr<AActor>) → **`Resolve Soft Class`** /
  **`Load Asset Blocking`** (a soft *class* → use the soft-class-resolve node to get a hard
  `Class (Actor)` reference). Promote to `ResolvedPartClass`.
  - **`Is Valid Class`** → Branch (unset/failed-load → End).
- From `HelmetAsset`, get **`Socket Name`** (FName) → `ResolvedSocketName` (will be `head`).
- **`Make FLyraCharacterPart`**:
  - **PartClass** = `ResolvedPartClass`.
  - **SocketName** = `ResolvedSocketName`.
  - **CollisionMode** = leave default (`NoCollision`) — a cosmetic helmet, no collision (matches the
    body-part pattern; a colliding head part would fight the capsule).

### 6. Add the part
- Call **`Add Character Part`** (on **Self** — it's a method on this component;
  `BlueprintCallable, BlueprintAuthorityOnly`):
  - **New Part** = the `Make FLyraCharacterPart` output.
- That's the add. The parent component spawns the part actor, attaches it to the `head` socket, and (if
  the part is a colored `AAFLCharacterPartActor`-style part) the proven #38a/#43 color path applies on
  the part's BeginPlay. For Step B's proof we use a plain visor part (mesh only) — color is the next axis.

---

## Compile + save
Compile the Blueprint, Save. (This persists the graph — the BP-node author the operator does in-GUI.)

## Then PIE-prove (the helmet bar)
With `afl.SkinDiag 1`:
1. `afl.Cosmetic.Resolve AFL.Helmet.Visor01` → must log `resolveVia=catalog` + `asset=DA_AFL_Helmet_Visor01`
   (proves the catalog-resolution half, type-independent).
2. `afl.Cosmetic.SetHelmet Visor01` → issues the selection RPC; on the next part-resolve (re-possess /
   respawn, or fire BeginPlay path) the add-event runs.
3. **Watch:** the visor spawns on the robot's **head socket**, sits correctly (socket-fit — proven
   standalone in Step A, so a bad fit here would be the graph/transform, not the mesh).

**Pass = visor visible on the head via the catalog, resolveVia=catalog, socket-fit clean.**

## Notes / gotchas
- **Timing:** BeginPlay may fire before the loadout's `HelmetId` has replicated to the server-side
  PlayerState. The `Is Valid`/`!= None` guards make it a clean no-op in that case. If the helmet doesn't
  appear on first spawn but does after a `SetHelmet` + re-possess, that's the timing window — the same
  dual-path shape #43 solved. If it needs a re-drive, the cleanest is to ALSO call this add-logic from
  the loadout component's existing `NudgeControllerReapply`/possession path (a C++ follow-up), but try
  the BeginPlay version first — it may be sufficient for the proof.
- **`AddCharacterPart` is `BlueprintAuthorityOnly`** — it silently no-ops off-authority, which is why the
  step-1 authority gate matters (it's belt-and-suspenders, but keeps the graph honest).
- **SocketName = `head`** is the canonical UE5 Mannequin head bone (to be visually confirmed in Step A's
  standalone fit). If the visor sits at the wrong spot, the fix is the SocketName (try `neck_01`, or a
  dedicated socket added to SK_Mannequin) + the part actor's mesh transform — NOT the catalog/graph.
