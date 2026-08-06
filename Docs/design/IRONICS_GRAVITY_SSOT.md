# IRONICS — Gravity & Shrink Systems SSOT

> **Status: APPROVED SSOT — locked 2026-06-22 (operator-confirmed).** Both plugins fully read (OmniWalk
> source-verified; MGG `BP_MGG_Character` read via the reference-mount). All six decisions are **law** (§7);
> **no open PROPOSED items** (only tune-at-build numbers remain). Covers the three gravity/scale systems +
> the access/map-gating model. Grounded in the **real OmniWalk source** + the **real MGG mechanic** (read, §3.1)
> + the **proven AFLMovement / AFL_GRAB stack.** Invents no plugin capability OmniWalk lacks; no MGG internals guessed.
>
> **Grounds in:** OmniWalk (UE5.6 C++ plugin, **source-read**) · MGG (UE5.3 BP pack, **`BP_MGG_Character` read**) ·
> `AFLCharacterMovementComponent`/`AFLDashMovementComponent`/`AFLClimbMovementComponent` + **AFL_GRAB** ·
> `IRONICS_WEAPONS_SSOT` · `IRONICS_PRICING_SCARCITY_SSOT` · `IRONICS_MAP_MODE_SPEC` · `IRONICS_ECONOMY_SPEC`.
> **No OmniWalk game-install, no build, no shipping MGG; the `/MGG/` reference-mount is KEPT through the gravity-gun build, removed after.**

---

## 0. PROJECT-SIDE STATE (read-only, confirmed 2026-06-22)

| Thing | State | Note |
|---|---|---|
| **OmniWalk plugin** | **ON DISK (`Plugins/OmniWalk07f218f7e14eV2/`), source-verified; NOT enabled into the game** | Real C++ plugin: `OmniWalk.uplugin` (**5.6.0**, `Installed:true`, `CanContainContent:true`, modules **OmniWalk**[Runtime/Default] + **OmniWalkEditor**[Editor/PostEngineInit], dep **EnhancedInput**; PhysicsCore = a Build.cs dep), **prebuilt** (Intermediate UHT for Editor + Game Win64). API source-verified (§2.1). **Absent from the `.uproject` Plugins list** — game-enable is a gated future step (§8). By GregOrigin (Fab). |
| **MGG pack** | **MOUNTED (reference-only `/MGG/`) + READ 2026-06-22; mechanic resolved (§3.1)** | Content moved under `Plugins/MGG/Content/` + a throwaway `EnabledByDefault` `MGG.uplugin` (§8 #2) → **118 assets** at `/MGG/`. `BP_MGG_Character` read: **server-auth grab/push** (§3.1). **Reference-only, NOT in the `.uproject`, never shipped, removed after harvest.** |
| **Proven movement stack** | **On disk, proven** | `AFLCharacterMovementComponent` (custom CMC) · `AFLDashMovementComponent` · `AFLClimbMovementComponent` · dash/climb GAS abilities · **AFL_GRAB** (`AFLGrabbableComponent`/`AFLInteractionComponent`/`AFLGameplayAbility_Grab`/`_Throw`/`AFLLootRetrievalRouter`) — replicated grab/carry/throw, proven. |

---

## 1. THREE DISTINCT SYSTEMS

| # | System | What | Source | Type |
|---|---|---|---|---|
| 1 | **Antigravity-map locomotion** | walk on arbitrary surfaces — the player's **own** gravity | **OmniWalk** C++ plugin | **locomotion / map** |
| 2 | **Gravity gun** | a **weapon** that pulls/pushes **target** objects | **AFL-authored**, harvested from **MGG** | **weapon** |
| 3 | **Shrink gun + Shrink map** | a **weapon** that scales a **target** down | **AFL-authored** (net-new) | **weapon + map** |

> They **co-locate** (the gravity gun is common on the OmniWalk map) but are **separate systems**. The
> gravity/shrink **guns do NOT depend on OmniWalk** — they manipulate **targets**, not the player's own gravity (§3.4).

---

## 2. SYSTEM 1 — OmniWalk locomotion (the antigravity map)

### 2.1 What OmniWalk is (SOURCE-VERIFIED against the real headers)
A **locomotion** plugin — walk on arbitrary surfaces via UE5.6 native `CMC->SetGravityDirection`. The public surface (all source-read):
- **`UOmniWalkPro`** (`UActorComponent`, zero-config "God Component"): ticks; private `HijackAndFixCharacter`
  (**gated by `bAutoFixPawnSettings=true`**) + `UpdateSurfaceAdhesion(ACharacter*, dt)` + `ApplyInputCorrection(ACharacter*)`.
  Tunables **`TraceDistance=200 · AlignmentSpeed=12 · AdhesionForce=2500`**.
- **`UOmniWalkComponent`** (`UActorComponent`): **adhesion-only** (`UpdateSurfaceAdhesion`, `IsStuckToSurface()`)
  — **no hijack / no input-correction**. Tunables 200/10/500.
- **`UOmniWalkSubsystem`** (`UWorldSubsystem`): `OnWorldBeginPlay` → `InitializeTaggedActors` — **scans the
  world for actors tagged `OmniWalk.Enabled` and injects the component.** **World-scoped (per-level)** — clean for map-scoping.
- **`AOmniWalkPlayerController`** (`APlayerController`): `UpdateRotation` in "gravity space" (no camera flip on
  walls/ceilings) + `GetGravityRelativeDirection` (remaps 2D input into the plane ⊥ gravity); `CameraAdaptationSpeed=12`.
- **`UOmniWalkCameraModifier`** (`UCameraModifier`): `ModifyCamera` gravity-space camera, **decoupled** (applies
  to an existing controller — no controller swap needed); `CameraRotationSpeed=15`.
- **How it works:** trace actor-down → `Hit.Normal`=TargetUp → `CMC->SetGravityDirection(VInterpTo(-TargetUp))`
  → rotate to surface → `-TargetUp * AdhesionForce` impulse when falling.

### 2.2 ⚠ CMC coexistence — a DELIBERATE integration (LAW: map-scoped, `bAutoFixPawnSettings=false`)
`HijackAndFixCharacter` blind-mutates the CMC (`SetWalkableFloorAngle(90)`, `bOrientRotationToMovement=false`,
`bUseControllerRotationYaw=false`) and `ApplyInputCorrection` consumes/re-injects `ConsumeMovementInputVector()`
every tick — colliding with IRONICS's proven stack (`AFLCharacterMovementComponent`, `AFLDashMovementComponent`,
**`AFLClimbMovementComponent`** — *functional overlap, both do wall movement*, dash's input). **So the integration
is DELIBERATE, never auto-hijack:**
- **`bAutoFixPawnSettings=false` (LAW)** → **disables the auto-CMC-hijack.** AFL reconciles the CMC config
  **manually** (decide WalkableFloorAngle/orientation per the antigravity map vs the proven dash/climb config)
  instead of letting OmniWalk blind-mutate it.
- **Map-scoped (option (a), LAW)** → OmniWalk active **only on the antigravity map**, applied via the subsystem's
  **`OmniWalk.Enabled` tag** on that map's pawn (or a map-scoped experience component). Off-map, the proven stack is **untouched**.
- **Camera (LAW): use `UOmniWalkCameraModifier`, do NOT swap to `AOmniWalkPlayerController`** — swapping the
  controller drops Lyra/AFL controller features; instead add the **decoupled camera modifier** to the existing
  controller + port `GetGravityRelativeDirection` into the AFL input path.
- **Must be tested:** dash + OmniWalk together (the input-vector consume conflict); climb-vs-OmniWalk overlap
  (does OmniWalk **replace** climb on this map?).

### 2.3 ⚠ MULTIPLAYER NET RECONCILIATION — the CENTRAL engineering requirement (SOURCE-VERIFIED LAW)
**OmniWalk ships ZERO replication.** Source-verified across **all five classes** — `UOmniWalkPro`,
`UOmniWalkComponent`, `UOmniWalkSubsystem`, `AOmniWalkPlayerController`, `UOmniWalkCameraModifier` are plain
`UActorComponent`/`UWorldSubsystem`/`APlayerController`/`UCameraModifier` with **no `Replicated` UPROPERTY, no
`GetLifetimeReplicatedProps`, no Server RPCs** — **pure client-local per-tick logic.**

**Therefore — AFL OWNS THE ENTIRE NET-RECONCILIATION BURDEN.** This is **the central engineering requirement
of the OmniWalk integration, not a footnote.** In a competitive multiplayer extraction shooter, the **gravity
direction + surface adhesion MUST be server-reconciled** — a player's gravity-up is gameplay state other
clients must agree on; `SetGravityDirection` set client-side **will desync** what others see — or the game
accepts visible desync. OmniWalk provides **no** net code, so AFL must add it: **server authority over the
gravity-up + replication/prediction** (saved-move on the CMC gravity direction, or a server-driven gravity-up
replicated to clients). **MGG's net model (§3.1) — now READ — is a CONFIRMED sound server-auth physics pattern**
(Server RPC `S_*` events do the physics; state replicates via `Set with Notify`/`OnRep`): a concrete reference for
AFL's server-reconciliation shape, applicable in principle to the OmniWalk gravity-up.

### 2.4 Enable-into-the-game (gated future step — NOT this pass)
OmniWalk is **on disk but NOT enabled** in the `.uproject` (no game install). Enabling it (add to `.uproject`
Plugins) is **gated behind** the §2.2 deliberate-integration design + the §2.3 net-reconciliation requirement.
**No game-install, no build in this pass.**

---

## 3. SYSTEM 2 — Gravity Gun (AFL-authored weapon, harvested from MGG)

**AFL-authored**, on the **proven weapon spine** (`IRONICS_WEAPONS_SSOT`). **Harvest MGG's mechanic; do NOT
import its UE4-skeleton character / gamemode.**

### 3.1 The MGG donor — RESOLVED (`BP_MGG_Character` read in-editor 2026-06-22 via the reference-mount)
Parent `Character` (UE4 mannequin — **never shipped**); 339-node EventGraph; 31 vars, 13 components. The **real mechanic**:
- **GRAB:** `Line Trace By Channel` → `Break Hit Result` → **`Grab Component at Location`** (a **`PhysicsHandle`**
  component) → **`Set Target Location`** holds the target in front at **`GrabbingDistance`** (adjustable via
  `Increase/Decrease/InitGrabbingDistance`). Backed by a `GrabZone` + the **`Hit Actor`** (grabbed target).
- **🔑 PUSH = the GRAB primitive with the force FLIPPED (the key answer):** both polarities feed the **same
  `Add Force`** on the target — **`Get AttractionForce`** (draw toward) vs **`Get RepulsiveForce`** (push away),
  selected by **`GunMode`** / input (`IA_Shoot` attract vs `IA_ShootRepulsive` repulse). **Not two mechanics —
  one primitive, force value/sign flipped.** (Strongly validates the one-weapon-polarity-toggle shape, §3.2.)
- **THROW/RELEASE:** **`Release Component`** (PhysicsHandle release) — drops with momentum; **no distinct
  launch-on-release event.**
- **🔑 REPLICATION = SOUND SERVER-AUTHORITATIVE (reusable reference, NOT naive):** every action routes through a
  **Server RPC `S_*` event** — **`S_AttractObject`, `S_ResetGrab`, `S_StopGrabbing`, `S_FireStart/Stop`,
  `S_RedLaser/BlueLaser`** (+ movement `S_Jump/Crouch/PichReplication`). The **server** does the trace + grab +
  force, then sets **replicated** vars via **`Set with Notify`** (`IsGrabbing`, `Hit Actor`, `GunMode`,
  `GrabbingDistance`, `ObjectDistance`, `LaserColor`) → **`OnRep_*`** sync all clients. **A real server-auth
  model** — sound reference for both the gravity gun (§3.3) and the OmniWalk net layer (§2.3).
- **VFX/SFX:** `NS_Laser` (Niagara) + `LaserImpact` + replicated `LaserColor` — the beam; **retint to brand** (liquid-glass/neon).

### 3.2 The gravity-gun spec — CONFIRMED LAW
- **ONE WEAPON, pull/push POLARITY TOGGLE:** **left = ATTRACT/PULL**, **right = REPULSE/PUSH** — the **same
  `Add Force` primitive with the force sign flipped** (`AttractionForce` vs `RepulsiveForce` by `GunMode`, §3.1).
  **NOT two mechanics, NOT two SKUs.** **"Two play modes" = the two polarities** — *not* two game modes (no new mode scope).
- **NET MODEL:** server-authoritative — reauthored from MGG's confirmed **`S_*` Server-RPC + `Set with
  Notify`/`OnRep`** pattern (§3.1) onto the **proven AFL weapon spine + AFL_GRAB net model** (§3.3).
- **Access + map-gating + the special-weapon ban:** the two access models + the reusable ban flag — **§5**
  (the gravity gun is the first consumer of that general model).
- **VFX/SFX:** brand **liquid-glass / electric-neon + tracers**; `NE_Laser`/`S_Laser` = reference, **retint** (`User.Color`).

### 3.3 ⚠ Multiplayer authority — host on the PROVEN AFL_GRAB net model (the efficiency + safety win)
The gravity gun **moves physics objects other players see → MUST be server-authoritative.** **IRONICS already
has the proven path:** **AFL_GRAB** (`AFLGrabbableComponent` + `AFLGameplayAbility_Grab`/`_Throw`) is a
**proven replicated** grab/carry/throw (server-auth writes, `bReplicates`+`RepMovement`, authority-only
release/detach, carry-pose via ASC montage). **So the gravity gun = AFL_GRAB (proven replicated) + a RANGED
acquisition (line-trace grab at distance) + the PUSH polarity (force flipped).** **MGG (§3.1, now read) is a
CONFIRMED sound server-auth reference — and so is AFL_GRAB**, so the harvest is clean: **reauthor MGG's server-grab
pattern (`S_*` Server RPCs + replicated held-state) in C++/GAS on AFL_GRAB + the weapon spine** — never ship the
UE4-character pack. (AFL already owns this layer anyway — OmniWalk ships no net code, §2.3.)

### 3.4 Reconcile with OmniWalk (different systems)
**OmniWalk = the player's OWN gravity** (locomotion). **The gravity gun = a weapon effect on TARGETS.** The gun
**does NOT depend on OmniWalk** — it works on **any** map, merely **common** on the OmniWalk map and **rare elsewhere.**

---

## 4. SYSTEM 3 — Shrink Gun + Shrink Map (AFL-authored)

### 4.1 The shrink mechanic — mechanism = LAW (the numbers tune at build)
A **targeted scale effect** — a **GAS GameplayEffect** that **(a)** scales the target's **mesh + capsule**
down over a duration (**server-authoritative** → the target's scale replicates), then restores; **(b)** applies
**stat consequences** (*tune-at-build*): smaller **hitbox** (harder to hit) traded against **reduced damage/movement,
knockback-prone** — a temporary disadvantage to survive/escape. A **TARGET effect**, server-authoritative.

### 4.2 Shrink-gun hands — **RESOLVED: 2H (rifle-class)**
It inherits the **most-proven template** — two-handed **`ABP_RifleAnimLayers`** (validated by **both** Pulse +
Beam) → **lowest generation risk** (clones the Pulse spine; a 1H pistol is a larger divergence). As an
**ultra-rare statement weapon**, a substantial 2H silhouette + steadier two-handed aim suit a targeted-effect
weapon. *(1H viable; 2H is the safer, more-proven default. Operator may override.)*

### 4.3 Map-gated + brand
ULTRA-RARE, common on the **Shrink map**, top-rarity exclusive elsewhere (§5). VFX/SFX → liquid-glass/neon.

---

## 5. ACCESS MODELS + the SPECIAL-WEAPON BAN (CONFIRMED LAW)

Special weapons (gravity gun, shrink gun, future specials) have **two distinct access models** + a **studio
map-rule** that can disallow them. *(This supersedes the earlier "off-map ultra-rare / blocked-by-owner" reading
— that was a misread; the ban is a **map config**, not a per-item persistence thing.)*

### 5.1 Access model A — OWNED SKU (persistent, purchased)
- A **100-mint limited rare** = the **Surge** rarity tier (**$3 / 100-mint**, `IRONICS_PRICING_SCARCITY` ladder).
- A **persistent owned weapon SKU** — usable **ANYWHERE**, **except** on maps/events flagged *special-weapons-banned* (§5.3).
- ⚠ **PERSISTENCE-GATED:** the mint-cap / `MintedCount` enforcement + the persistent ownership ride the
  **FOUNDATIONAL persistence backend** (still open) — **can't be enforced as a true 100-mint owned item until persistence exists** (§7.1).

### 5.2 Access model B — MAP/MATCH GRANT (match-scoped, free in-match)
- On the gravity-gun **map/mode**, **EVERY player in the match is granted the gravity gun FREE for that match**
  — **match-scoped, not kept** after the match (no ownership, no purchase).
- **Mode access** is gated by **tier-3 unlock AND/OR a small `FLICKER`-rung cost** ($1–$2.50, Watts-only —
  accessible, earnable, not free, not expensive) to **ENTER** the mode. **Once in-match, the gun is free to all, period.**
- ✅ **BUILDABLE NOW:** the match-scoped grant = the proven Lyra **experience-adds-content** pattern + an entry gate — **no persistence dependency.**

### 5.3 The SPECIAL-WEAPON BAN — a general, reusable MAP/EXPERIENCE config flag (LAW)
- A **map/experience CONFIG property** (studio-controlled — **NOT a player feature, NOT persisted per-item**): a
  map/event flagged *special-weapons-banned* **disallows all special weapons** (gravity, shrink, any future special)
  — **even an owner's copy cannot be used there.**
- Spec **GENERAL + reusable**: e.g. a `bDisallowSpecialWeapons` experience flag (or a `Cosmetic.Weapon.Special` tag
  the experience checks **at equip/spawn**), covering gravity + shrink + future specials **uniformly** — **not a
  gravity-gun-specific hack.**
- ✅ **BUILDABLE NOW** — a config flag + an equip/spawn check; **no persistence dependency.** This removes the prior "blocked by owner" misread.

### 5.4 Two named maps for the map phase (cross-link `IRONICS_MAP_MODE_SPEC`)
- **The OmniWalk ANTIGRAVITY map** — walk-on-surfaces locomotion (§2); the **gravity gun** map/mode (access model B).
- **The SHRINK map** — the **shrink gun** map/mode (same access + ban model).
- Named entries in the tiered map roster (the map spec governs the ladder + per-map design-brief build gate).

---

## 6. UE5.3 → 5.6 + harvest-not-import note (confirmed)
- **MGG is BP content, harvested not imported** → the **UE4-skeleton/version risk is moot:** we **ship none of
  MGG's character/skeleton/gamemode**, only **reauthor its grab/push logic** onto the proven IRONICS rig +
  weapon spine + AFL_GRAB net model. The reference-mount (§8 #2) is **throwaway**, removed after harvest.
- **OmniWalk is a C++ plugin, prebuilt for 5.6** — its risk is the §2.2/§2.3 coexistence + net code, **not** the version.

---

## 7. CONFIRMED LAW + the build-readiness split (was PROPOSED — now locked)
All six decisions are **law** (no open PROPOSED): (1) gravity gun = **ONE weapon, pull/push polarity toggle**
("two play modes" = the two polarities, not game modes); (2) **server-auth** net model reauthored from MGG's
`S_*` pattern onto **AFL_GRAB**; (3) **two access models** — owned **Surge-100** SKU + **match grant**; (4) the
**general special-weapon-ban** map flag; (5) **shrink gun 2H** + same gating; (6) **OmniWalk coexistence (a)**
(map-scoped, `bAutoFixPawnSettings=false`, camera-modifier, AFL-owned net). Only **tune-at-build numbers** remain
(not blocking): gravity pull range / hold distance / push impulse; shrink scale factor / duration / stat consequences.

### 7.1 ⚠ Buildable-now vs persistence-gated (the honest split)
| Gravity-gun piece | Status |
|---|---|
| Polarity-toggle mechanic (one weapon, pull/push) | ✅ **buildable now** (no persistence) |
| Server-auth net model (AFL_GRAB + MGG `S_*` pattern) | ✅ buildable now |
| **Match grant** (access model B) + `FLICKER` mode-entry | ✅ buildable now (experience grant + entry gate) |
| **Special-weapon-ban** map flag (§5.3) | ✅ buildable now (config flag + equip check) |
| OmniWalk integration (§2.2, map-scoped) | ✅ buildable now (coexistence design + test; the net reconciliation is the real work) |
| **OWNED 100-mint Surge SKU** (access model A) | 🔴 **persistence-gated** — mint-cap / `MintedCount` + persistent ownership ride the FOUNDATIONAL persistence backend (still open) |

> **Most of the gravity gun is buildable independently of persistence** — only the *owned limited SKU* waits on it.

## 8. GATES — VERIFY-IN-EDITOR + the operator actions (no game-install / build / shipping MGG)

### #1 — OmniWalk (DONE this pass): API source-verified (§2.1) + the zero-replication finding (§2.3) made law. **Remaining gates before any game-enable:** the CMC-coexistence test (dash + OmniWalk; climb overlap; input-vector consume) + the multiplayer gravity-reconciliation build (§2.2/§2.3). **Not enabled into the `.uproject` this pass.**

### #2 — MGG reference-mount — ✅ **DONE 2026-06-22** (mounted at `/MGG/`, `BP_MGG_Character` read, §3.1 resolved; throwaway, NEVER shipped)
*Record of how it was mounted (reference-only — remove after harvest; NEVER a real IRONICS dependency, NEVER committed):*
MGG had no `.uplugin`, so the editor couldn't mount it. The fix that worked: move the content under `Content/` +
a minimal `EnabledByDefault` descriptor (below).

- **⚠ The dropped MGG content is NOT under a `Content/` folder** (assets live at `Plugins/MGG/Blueprints/`,
  `…/Demo/`, …). UE plugins mount **`Plugins/<name>/Content/`** as `/<name>/`, so the content **must be moved
  under `Plugins/MGG/Content/`** AND the descriptor needs **`EnabledByDefault: true`** to auto-mount without a
  `.uproject` entry. (The first-pass descriptor-only instruction was incomplete — corrected here.)
- **Descriptor** `C:\Dev\Bag_Man\Plugins\MGG\MGG.uplugin` (content-only, no modules):
  ```json
  { "FileVersion": 3, "Version": 1, "VersionName": "0.0-REFERENCE-ONLY",
    "FriendlyName": "MGG (REFERENCE-ONLY — harvest, do not ship)",
    "Description": "Throwaway mount for in-editor inspection. NOT a game dependency. Remove after harvest.",
    "Category": "Reference", "CanContainContent": true, "EnabledByDefault": true, "Installed": false, "Modules": [] }
  ```
- **Operator steps (editor closed):** **move** `Plugins/MGG/{Blueprints,Demo,…}` → `Plugins/MGG/Content/` →
  write the descriptor above → relaunch → tell me it's up. **Do NOT add MGG to the `.uproject` Plugins list**
  (reference mount only). Safe to move on disk because MGG is **not yet mounted/loaded** (no live references).
- **Then I (read-only via MCP):** open `/MGG/Blueprints/BP_MGG_Character`, read the grab/push/throw graph + the
  replication model, and report the **real** mechanic (§3.1) — resolving the PENDING flags. If it still won't
  read, I keep VERIFY-IN-EDITOR and report the blocker — **I will not invent the graph.**

## 9. Cross-links
- **`IRONICS_WEAPONS_SSOT.md`** — gravity + shrink = the **deferred weapon TYPES** (mechanics here); shrink-gun
  hands **resolved 2H** (closes that deferral).
- **`IRONICS_PRICING_SCARCITY_SSOT.md`** — the **owned** gravity-gun SKU = the **Surge** 100-mint rarity tier ($3);
  the **mode-entry** gate = the **FLICKER** rung ($1–$2.50). The match grant is free in-match (§5).
- **`IRONICS_MAP_MODE_SPEC.md`** — the **OmniWalk antigravity** + **Shrink** maps are named map-phase entries.
- **AFL_GRAB** — the proven replicated grab the gravity gun hosts on (§3.3). **OmniWalk** (on-disk, source-read) +
  **MGG** (harvested — `BP_MGG_Character` read, §3.1) = the two external donors.

---

*Finalized read-only 2026-06-22 — **APPROVED SSOT**, all six decisions law (§7): one-weapon polarity toggle ·
server-auth net (MGG `S_*` → AFL_GRAB) · two access models (Surge-100 owned SKU + match grant) · the general
special-weapon-ban map flag · shrink 2H · OmniWalk coexistence (a). Grounded in **both fully-read plugins**
(OmniWalk source-verified — zero replication, AFL owns net reconciliation §2.3; MGG mechanic read §3.1). The
buildable-now vs persistence-gated split is §7.1. **No open PROPOSED** (only tune-at-build numbers). No OmniWalk
game-install, no build, no shipping MGG; the `/MGG/` reference-mount is KEPT through the gravity-gun build.*
