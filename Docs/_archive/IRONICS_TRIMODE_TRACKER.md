# Tri-Mode System — Haywire / Pro Mod / Melee — Build Plan & Tracker

## Progress log (live) — as of 2026-07-30

Senior-dev takeover of the tri-mode build. Verified against a headless UE-Python asset inspection (Python Editor Script Plugin enabled in `Bag_Man.uproject` for deterministic commandlet-driven asset ops).

**Phase 0 — DONE & verified.** `B_Experience_BagMan` is now an **ObjectRedirector** → `B_Experience_Haywire` (correct rename, *not* a duplicate). Haywire GFE keeps `AFLDismember`.

**Phase 1 — mode-plumbing DONE & PIE-verified.** ProMod slice is structurally correct:
- `B_Experience_ProMod` — GFE `[ShooterCore, AFLCore, AFLCombat, AFLMovement]` (**AFLDismember dropped**); DefaultPawnData = `HeroData_BagMan_Pro`.
- **Naming note:** AIK used the project's real convention — Pro PawnData is **`HeroData_BagMan_Pro`** (not the plan's placeholder `DA_AFL_PawnData_Hero_Pro`); default is **`HeroData_BagMan`**. Treat `HeroData_BagMan_Pro` as canonical going forward.
- `B_Hero_BagMan_Pro` spawns under ProMod (PIE-confirmed).

**P1-8 clean-health gate — ✅ DONE & PIE-VERIFIED (2026-07-30).** Two-layer C++, built two-engine (D: source + C: launcher):
- `AFLDamageExecCalc.cpp` — target carrying `State.Mode.NoDismember` forces `bIsZoneRouted=false` → single body-health. *(Also carries AIK's `raw=0.0` fix + `AFL_DMG` outcome instrumentation.)*
- **Tag application (final mechanism):** `UAFLMatchPhaseComponent::OnExperienceLoaded_ApplyModeTags` stamps `State.Mode.NoDismember` on every combatant ASC (PlayerState ASCs + pawns, deduped) **when the active experience omits `AFLDismember`**. Registered via `CallOrRegister_OnExperienceLoaded`. Auto-scoped (Haywire untouched); covers the dummy.
- **⚠ Crash + fix (learned):** first cut read the experience at `BeginPlay` via `GetCurrentExperienceChecked()` → assert `LoadState==Loaded`. This component's `BeginPlay` runs DURING AFLCombat game-feature activation, BEFORE the experience finalizes loading. Fix = defer the read to the `OnExperienceLoaded` delegate. **Rule: never read the experience from a GameFeature-added component's BeginPlay; defer to `CallOrRegister_OnExperienceLoaded`.**
- **PIE PROOF (tag live):** ProMod — torso `LANDED`; **`zone=2 bone=hand_l → LANDED health=8.0`** (was `ZONE_CONSUMED`); dummy **died from accumulated body health** (`AFL_DEATH … out of Health`) — no sever. Clean single-health confirmed. Haywire regression check owed (limb must still `ZONE_CONSUMED`).

**P1-6/P1-7 movement — ✅ DONE & verified (2026-07-30).** AIK already tuned it, matching plan targets: Pro CMC (`LyraCharacterMovementComponent` on `B_Hero_BagMan_Pro`) = MaxWalkSpeed **700** (vs default 600), MaxAcceleration **3200** (vs 1200), BrakingDecelWalking **2600** (vs 1400), GroundFriction **9** (vs 8). `HeroData_BagMan_Pro` grants Dash + Climb + Interaction(Grab) + ShooterHero/EMP/Loadout. Weapon IK live (PIE). **Plan's `DA_AFL_MoveProfile_Pro` NOT needed** — numbers live on the pawn CMC directly. **Phase 1 mode-plumbing slice = COMPLETE.**

**Still open on Pro Mod:** **Phase 1B — true FBIK** (Pro pawn still resolves `rig=CR_AFL_IRONICS`, Two-Bone/Aim/foot-trace only; no PBIK full-body solve). The single largest new-authoring item and the operator's core Pro-Mod ask. Minor: spawn-race warning persists (weapon fires once ASC arrives — parked); "routing to match play on map" UX friction (mode-select, Phase 4).

## Context

Character-mod work kept colliding with the **dismemberment game feature** (the zone-HP + gib system baked into the core combat/pawn). Rather than rework the character system to accommodate mods, we resolve it as a **major upgrade**: split the game into **modes**, where dismemberment becomes a *per-mode rule* instead of an always-on coupling.

The key insight from research: **a "game mode" in this Lyra project is just a `ULyraExperienceDefinition` asset** that lists which GameFeatures activate + which components/abilities are granted. So the meshes, weapons, hand cannons, cosmetics, movement, IK, scoring, and economy are **mode-agnostic and carry over untouched** — only a thin *rules layer* (one Experience per mode + one small C++ gate + one small ranking component) diverges. This is why the divergence is mostly **assembly of existing systems**, not a rewrite.

Three modes:
- **Haywire Mode** — the current dismemberment gameplay, forward-face renamed. No behavior change.
- **Pro Mod** — no dismemberment, clean single-health, FBIK-enhanced faster/stronger/smoother movement, new AAA characters, the hand-cannon "1 base + N colors" methodology applied to characters (8 colors).
- **Melee Mode** — instant respawn on kill; ranking = kills + assists − deaths; a durable cross-match ladder.

## Locked decisions (operator)

1. **Pro Mod damage = clean single-health via a real C++ gate** (removes both the gib layer *and* the zone-HP model → conventional health). This is the one genuinely-new combat code change; it makes the mode split fully data-driven thereafter.
2. **Melee = instant-respawn deathmatch, ANY weapon** (not melee-weapon restricted). Keep the name "Melee Mode" as the operator's label for the fast-respawn/KDA arcade ruleset; document it as **weapon-agnostic** so nobody scopes a melee-only weapon set. Melee shares the clean-health gate.
3. **Ranking = durable cross-match ladder** (the target). Ship a **session-local KDA MVP** as the intermediate milestone so the mode is playable well before the backend ladder + MMR land.
4. **Pro Mod roster = 1 pilot character first** (Rodin body + 8-color axis on the shared `SK_Mannequin`), prove the mode end-to-end, then expand.

## Mode architecture

A game mode == one `ULyraExperienceDefinition` in `Plugins/GameFeatures/AFLBagMan/Content/Experiences/` (scanned via `Config/DefaultGame.ini` → `/AFLBagMan/Experiences`). Precedent for a no-dismember, non-BagMan Experience already exists on disk: `B_LyraExperience_AFL_Arena_Test` — use it as the structural model.

| Feature | Haywire | Pro Mod | Melee |
|---|---|---|---|
| Experience asset | `B_Experience_Haywire` (rename of `B_Experience_BagMan`) | `B_Experience_ProMod` (new) | `B_Experience_Melee` (new) |
| Pawn BP | `B_Hero_BagMan` | `B_Hero_BagMan_Pro` (no dismember comp) | `B_Hero_BagMan_Pro` |
| PawnData | `DA_AFL_PawnData_Hero_Default` | `DA_AFL_PawnData_Hero_Pro` | `DA_AFL_PawnData_Hero_Pro` |
| `AFLDismember` GameFeature | enabled | **dropped** | **dropped** |
| Zone-HP model (Layer 1) + gibs (Layer 2) | on | **gated off (clean health)** | **gated off** |
| Weapon/hand IK (`UAFLWeaponIKComponent`) | as-is | as-is | as-is |
| **True FBIK (full-body)** — none exists today | ❌ | ✅ new `CR_ProMod_FBIK` | ✅ new `CR_ProMod_FBIK` |
| Enhanced movement | base | faster/snappier | faster/snappier |
| Dash / Climb / Grab sets | optional | granted | granted |
| New characters + 8-color axis | no | yes (1 pilot first) | shared with Pro |
| Ranking authority (K+A−D) | no | no | `UAFLDeathmatchRankComponent` (new) |
| Respawn | round-gated | round default | **instant** (0 delay, mid-round) |

Mode selection surfaces exactly as today (per `Docs/IRONICS_MAP_MODE_SPEC.md §5`, mode == Experience): map `WorldSettings.DefaultGameplayExperience`, `?Experience=B_Experience_ProMod` URL for PIE, and playlist rows / front-end mode buttons mapping 1:1 to the three Experience primary asset IDs.

## The dismemberment gate (chosen: clean single-health)

Dismemberment is **two decoupled layers**:
- **Layer 1 — zone-HP routing**, `Plugins/GameFeatures/AFLCombat/Source/AFLCombat/Private/AbilitySystem/AFLDamageExecCalc.cpp` (~271–385). Always-on, no gate; routes hits to head/arm/leg HP attributes and broadcasts `Event.Dismember.Sever.AFL`. **Trap:** an unseeded/≤0 zone HP makes limb/head shots deal *0* body damage — so you can't disable it by simply not seeding zone HP.
- **Layer 2 — cosmetic sever/consequences**, `Plugins/GameFeatures/AFLDismember/Source/AFLDismember/Private/AFLDismemberComponent.cpp` (`OnSever`). This component lives **directly on the pawn BP** `B_Hero_BagMan`, not added by a GameFeature.

**Implementation (Option 3 — the gate):** a single gameplay tag `State.Mode.NoDismember` (applied to the pawn ASC per Pro/Melee Experience) checked in **both**:
- `AFLDamageExecCalc.cpp` — wrap the zone-routed block in `if (!bDismemberDisabled)`; the `else` routes all damage to the base **Health** attribute (conventional single-health; sidesteps the zero-damage trap by construction).
- `AFLDismemberComponent::OnSever` — early-return on the same gate.

Set per-Experience via a `GameFeatureAction` that applies `State.Mode.NoDismember` on ProMod/Melee. Belt-and-suspenders: Pro/Melee also use the dismember-free `B_Hero_BagMan_Pro` pawn and drop `AFLDismember` from `GameFeaturesToEnable`, so nothing dismember loads at all.

## Pro Mod build

- **Pawn:** `B_Hero_BagMan_Pro` — child of `B_Hero_BagMan` minus `AFLDismemberComponent`. `DA_AFL_PawnData_Hero_Pro` points at it and grants the movement/interaction ability sets.
- **Movement (reuse `UAFLCharacterMovementComponent` + `AFLOverdriveComponent` pattern):** expose tunables on a thin `DA_AFL_MoveProfile_Pro` — starting targets `MaxWalkSpeed 650–720`, `MaxAcceleration 3000–3600`, snappier braking, optional passive Overdrive `SpeedMultiplier 1.15–1.3`. Baseline passive; reserve Overdrive for a sprint/dash buff. Enhanced-move abilities already exist: `GA_AFL_Dash/Climb/Grab` + `DA_AFL_AbilitySet_Movement_Dash/Climb/Interaction`.
- **True FBIK (NEW — currently MISSING; the operator's core Pro-Mod ask):** investigation verdict — the project has **no full-body IK**. `CR_AFL_CoreIK` is a spec alias for the real rig `Content/Characters/Heroes/IRONICS/Animations/CR_AFL_IRONICS.uasset`, which contains only per-limb **Two-Bone IK** + Aim + Lyra **trace-based foot placement** — no pelvis/spine coupled solve, no PBIK/Full-Body-IK node. `UAFLWeaponIKComponent` drives only the **left-hand support** channel. `IK_Mannequin` exists (`Content/Characters/Heroes/Mannequin/Rig/IK_Mannequin.uasset`) but is a **retarget-time** IK Rig, not a runtime FBIK vehicle.
  - **Author a new `CR_ProMod_FBIK`** — a Control Rig with a **Full-Body IK (PBIK)** solver over the shared `SK_Mannequin` hierarchy (pelvis + spine_01.. + upperarm/lowerarm/hand_l/r + thigh/calf/foot_l/r effectors). One rig covers every Pro Mod character (shared skeleton).
  - **Delivery:** mirror `UAFLWeaponIKComponent`'s proven `ResolveOwnerControlRig` + `SetControlValue` push in a new sibling component (or extend), adding pelvis/spine/foot effector controls. Layer the FBIK rig as an **additional** anim-graph / post-process Control Rig node **after** the existing solve — **do NOT modify the shipped `CR_AFL_IRONICS`** (its controls are load-bearing for weapon/grab IK on every character).
  - **Gate:** add the FBIK rig + component to Pro Mod pawns only, via the Experience `GameFeatureAction_AddComponents` (same pattern that adds `UAFLWeaponIKComponent`), so stock/Haywire characters are untouched.
  - The existing `UAFLWeaponIKComponent` weapon/hand IK **stays** for weapon hold; FBIK adds the full-body pose + foot solve on top.
- **Character (data-only, per `Docs/AFL_IDENTITY_PRODUCTION_LINE.md` 7-point checklist):** `Cosmetic.Brand.<NAME>` tag + `DA_AFL_CharacterPartMap` row + `B_AFL_Robot_<NAME>` BP (child of `AAFLCharacterPartActor`) + neutralized body/limbs MI + logo texture + `DA_AFL_CosmeticCatalog` rows; body gen via `Content/AFL/_Bridge/Rodin/rodin.py` reskinned onto shared `SK_Mannequin` (`bUniqueBodyUVs`). Resolution spine reused as-is: `UAFLCharacterPartSelectorComponent → UAFLCharacterPartMap → AAFLCharacterPartActor → UAFLSkinColorControllerComponent`.
- **8-color axis (base + Standard 6 + Neon Orange):** one `FAFLColorIdentity` row + one `Cosmetic.Identity.*` tag per color in `Content/BagMan/Cosmetics/DA_AFL_ColorIdentityRegistry.uasset` (applied via `FAFLSkinFinish::FindToneForParam`, `NeonColor→EdgeGlowColor`, through `AAFLCharacterPartActor::ApplySkinColor`). **Neon Orange = `Cosmetic.Identity.Solar` #FF7300 exists but has NO edge-ramp/emissive preset — author it** so it renders like the other 7.
- **Tint caveat (do not put on the slice critical path):** the color-registry SKIN/BODY migration is spec-locked/unbuilt — bodies still read baked `UAFLSkinColorAsset` finishes, and multi-part Rodin bodies won't tint from a single MID until reskinned to a master. For the pilot: reskin to a single-MID master (or ship baked finishes); track the full migration as breadth (P4-3).

## Melee Mode

- **New ranking component `UAFLDeathmatchRankComponent`** (mirror the confirmed template `UAFLRoundManagerComponent`: `UGameStateComponent` + `IAFLRoundRestartPolicy`, server FSM, replicated state). New files: `Plugins/GameFeatures/AFLCombat/Source/AFLCombat/Public/Round/AFLDeathmatchRankComponent.h` + `Private/Round/AFLDeathmatchRankComponent.cpp`.
  - **Score source = existing replicated StatTags** on `ULyraPlayerState`: `ShooterGame.Score.{Eliminations,Deaths,Assists}` (already read at `AFLCombat/.../UI/AFLW_MatchScoreboard.cpp:159-178`). **No new scoring pipeline.**
  - `Rank = Eliminations + Assists − Deaths`, server-authoritative; replicate a sorted leaderboard (playerId, K, A, D, rank) + kill-target/time-limit win condition.
  - **Win/end → reuse `UAFLMatchPhaseComponent.ConcludeMatch()`** (do not reimplement match end).
  - `ShouldBlockRestart() → false` (never suppress → instant respawn).
- **Instant respawn (reuse):** `GA_AFL_AutoRespawn` with delay 0 (a `GA_AFL_AutoRespawn_Instant` variant) + `bAllowMidRoundRespawn = true` + never call `SetRoundRespawnSuppressed(true)`; `AAFLGameMode::ControllerCanRestart` already consults the policy seam → **zero game-mode edits**.
- **Any-weapon:** Melee runs with the existing arsenal (per decision 2) — no melee weapon set required.
- **Ranking persistence:** session-local leaderboard on the component first (zero backend, ships with the slice); **durable ladder** writes final K/A/D + rank to PlayFab on `ConcludeMatch()` (a new, undemonstrated write shape) and a true skill ladder needs the unbuilt **Glicko-2 MMR (AFL-2201)** — both sequenced after the playable MVP.

## Phased tracker

Size: S(≤½d) M(1–2d) L(3d+). R=reuse, N=new.

**Phase 0 — Forward-face (no risk)**
- P0-1 (S,R) Rename `B_Experience_BagMan` → `B_Experience_Haywire` (+ redirector; update `DefaultGameplayExperience` refs).
- P0-2 (S,R) Front-end: relabel current mode "Haywire Mode".

**Phase 1 — Pro Mod vertical slice (end-to-end, clean-health)**
- P1-1 (S,R) Duplicate Haywire → `B_Experience_ProMod`.
- P1-2 (S,N) `B_Hero_BagMan_Pro` (strip `AFLDismemberComponent`).
- P1-3 (S,R) `DA_AFL_PawnData_Hero_Pro` → Pro pawn.
- P1-4 (S,R) ProMod Experience: drop `AFLDismember` GF + LegPenalty AddComponents; set Pro PawnData.
- P1-5 (S,R) Add `UAFLWeaponIKComponent` (existing weapon/hand IK) to the Pro pawn.
- P1-6 (M,R) Enhanced-move numbers via `DA_AFL_MoveProfile_Pro` / Pro MoveComp.
- P1-7 (S,R) Grant Dash/Climb/Grab ability sets on Pro PawnData.
- **P1-8 (M,N) Clean-health GATE** — `State.Mode.NoDismember` in `AFLDamageExecCalc.cpp` + `AFLDismemberComponent::OnSever`; apply per-Experience. *(Committed damage model, not optional.)*
- P1-9 (M) PIE `?Experience=B_Experience_ProMod`: verify no gore, single-health damage, movement feel, weapon IK. **Mode-plumbing slice done** (FBIK lands in Phase 1B).

**Phase 1B — True FBIK (the core Pro-Mod upgrade; NEW — nothing exists today)**
- P1B-1 (L,N-asset) Author `CR_ProMod_FBIK` — Full-Body IK (PBIK) solver over shared `SK_Mannequin` (pelvis + spine + 4 limb effectors + foot placement). Isolated; does NOT touch `CR_AFL_IRONICS`.
- P1B-2 (M,N-C++) FBIK delivery component — mirror `UAFLWeaponIKComponent`'s `ResolveOwnerControlRig` + `SetControlValue` pattern; push pelvis/spine/foot effector controls.
- P1B-3 (M,N) Wire `CR_ProMod_FBIK` as an added anim-graph / post-process Control Rig node (after the existing solve); add rig+component to Pro pawns via `GameFeatureAction_AddComponents` (gated to Pro/Melee).
- P1B-4 (M) PIE: verify full-body pose + foot planting on uneven ground on the Pro pawn, weapon/hand IK still correct, stock/Haywire characters unaffected. **FBIK slice done.**

**Phase 2 — First Pro Mod character + 8-color axis (1 pilot)**
- P2-1 (L,N-data) Author 1st `B_AFL_Robot_<NAME>` via the 7-point checklist.
- P2-2 (L,R-pipe) Rodin body gen + reskin to `SK_Mannequin` (single-MID master).
- P2-3 (M,N-data) 6 Standard neon registry rows + `Cosmetic.Identity.*` tags.
- P2-4 (M,N-asset) Author the **Solar (Neon Orange) edge-ramp/emissive preset**.
- P2-5 (S) Editor relaunch → resolve tags; verify all 8 colors render on the pilot.

**Phase 3 — Melee Mode (playable, session-local ranking)**
- P3-1 (S,R) Duplicate ProMod → `B_Experience_Melee`.
- P3-2 (L,N-C++) `UAFLDeathmatchRankComponent` (mirror RoundManager; K+A−D from StatTags).
- P3-3 (M,R) Wire via AddComponents; `ConcludeMatch()` on win.
- P3-4 (M,R) Instant respawn: `GA_AFL_AutoRespawn_Instant` (0 delay) + allow mid-round + no suppression.
- P3-5 (M,R) Session-local leaderboard replication + HUD readout (reuse scoreboard read path).
- P3-6 (M) PIE: kill→instant respawn, rank updates live, match ends on target. **Melee MVP done.**

**Phase 4 — Breadth / durability / hardening**
- P4-1 (L,N) Durable Melee persistence: PlayFab write shape on `ConcludeMatch()` *(undemonstrated — de-risk carefully)*.
- P4-2 (L,N-future) Glicko-2 MMR skill ladder (AFL-2201).
- P4-3 (L,N) Color-registry SKIN/BODY migration (single-MID tint for multi-part bodies).
- P4-4 (L,N-data) Remaining Pro Mod characters (repeat Phase 2).
- P4-5 (M,R) Playlists + front-end 3-mode select.

## Dev build docs to produce (when implementation begins)

Slot beside the existing `IRONICS_*` SSOTs in `Docs/`:
- `IRONICS_GAME_MODES_SSOT.md` — master mode SSOT: the 3 Experiences, the feature matrix, selection surfaces, the dismember gate. Extends `IRONICS_MAP_MODE_SPEC.md §5`.
- `AFL_PRO_MOD_CHARACTER_BRIEF.md` — Pro pawn variant, move tunables, FBIK, 8-color axis, Solar edge-ramp spec, tint caveat. Extends `AFL_IDENTITY_PRODUCTION_LINE.md` + `AFL_COLOR_REGISTRY_MIGRATION_PLAN.md`.
- `IRONICS_MELEE_RULESET_SSOT.md` — K+A−D formula, `UAFLDeathmatchRankComponent` contract, instant-respawn wiring, session→durable ranking, Glicko-2 hook. Extends `IRONICS_LEAGUE_ADVANCEMENT_SSOT.md §3.2/§7`.

## Verification (end-to-end)

- **Haywire regression:** boot `B_Experience_Haywire` in PIE — dismemberment, leg penalty, gibs unchanged. (Rename must not alter behavior.)
- **Pro Mod slice:** `?Experience=B_Experience_ProMod` — confirm (a) no gibs on limb/head kills, (b) **single-health** damage (headshots do normal damage, not survivable-decap; limbs don't zero), (c) faster/snappier movement + Dash/Climb/Grab, (d) weapon/hand IK holding cleanly.
- **True FBIK (Phase 1B):** on the Pro pawn, confirm full-body pose solve + foot planting on slopes/steps, weapon/hand IK still correct on top, and that stock/Haywire characters show **no** change (the FBIK rig is gated and does not touch `CR_AFL_IRONICS`).
- **8-color pilot:** cycle the pilot through all 8 identities (base + Standard 6 + Solar) via the cosmetic loadout/cheat; every surface (body, edge, emblem, visor) tints, including Neon Orange.
- **Melee MVP:** `?Experience=B_Experience_Melee` — kill → instant respawn (no timer), leaderboard updates `K+A−D` live, match ends at kill-target via `ConcludeMatch()`; no `State.Round.NoRespawn` suppression.
- **Build discipline:** the clean-health gate + `UAFLDeathmatchRankComponent` are C++ — build editor-closed, two-engine (D: source LyraEditor then C: launcher rebuild), per the standing rule. New Rodin bodies follow the import discipline (fresh skeleton, no phantom bones) before acceptance.

## Risks

1. **Zone-damage trap (F.1):** unseeded zone HP → 0 damage. The gate's `else`-to-single-Health path avoids it; verify damage in PIE for any no-dismember pawn.
2. **Color-registry skin path unbuilt (F.2):** multi-part Rodin bodies won't single-MID tint until reskin-to-master; use a single-MID master (or baked finishes) for the pilot; full migration is P4-3, off the slice critical path.
3. **Durable persistence undemonstrated (F.3):** the PlayFab rank write is a new write shape; session-local MVP first, durable ladder + Glicko-2 as Phase 4.
4. **Editor-relaunch for tags (F.4):** new `Cosmetic.Identity.*` / `Cosmetic.Brand.*` / `State.Mode.NoDismember` tags resolve only after an editor relaunch; author tags before wiring assets; budget the relaunch (P2-5).
5. **Phantom-bone / import discipline (F.5):** new bodies reskinned onto shared `SK_Mannequin` must respect `bUniqueBodyUVs` + skeleton hierarchy or they break the shared skeleton and the FBIK Control Rig; enforce import discipline before accepting a body.
6. **FBIK is net-new + rides a frozen shared AnimBP (F.6):** true full-body IK does not exist yet — `CR_ProMod_FBIK` (PBIK) must be authored from scratch and layered as an *added* Control Rig node **without** re-pinning the shared `ABP_Mannequin_Base` graph or altering the load-bearing `CR_AFL_IRONICS` controls; a regression here would break weapon/grab IK on **every** character. Keep FBIK isolated + gated to Pro/Melee; PIE-verify stock characters are unchanged. FBIK is the single largest new-authoring item in Pro Mod (Phase 1B).

## Critical files
- `Plugins/GameFeatures/AFLBagMan/Content/Experiences/B_Experience_BagMan.uasset` — rename to Haywire; template for ProMod/Melee (`B_LyraExperience_AFL_Arena_Test` = no-dismember precedent).
- `Plugins/GameFeatures/AFLCombat/Source/AFLCombat/Private/AbilitySystem/AFLDamageExecCalc.cpp` + `Plugins/GameFeatures/AFLDismember/Source/AFLDismember/Private/AFLDismemberComponent.cpp` — the clean-health gate (both layers).
- `Plugins/GameFeatures/AFLBagMan/Content/Characters/B_Hero_BagMan.uasset` — → `_Pro` variant.
- `Plugins/GameFeatures/AFLCombat/Source/AFLCombat/Public/Round/AFLRoundManagerComponent.h` — mirror template for `UAFLDeathmatchRankComponent`.
- `Content/BagMan/Cosmetics/DA_AFL_ColorIdentityRegistry.uasset` — 8-color axis + Solar edge-ramp.
- FBIK: `Content/Characters/Heroes/IRONICS/Animations/CR_AFL_IRONICS.uasset` (the real "CoreIK" rig — Two-Bone/Aim/foot-trace only; do NOT modify), `Content/Characters/Heroes/Mannequin/Animations/ABP_Mannequin_Base.uasset` (the shared Control-Rig evaluation point), `Content/Characters/Heroes/Mannequin/Rig/IK_Mannequin.uasset` (retarget-time only, not runtime FBIK), `Plugins/GameFeatures/AFLMovement/Source/AFLMovement/.../Interaction/AFLWeaponIKComponent.{h,cpp}` (the `SetControlValue` delivery pattern to mirror for `CR_ProMod_FBIK`), `SK_Mannequin` (the shared skeleton the FBIK rig targets).
- `Docs/AFL_IDENTITY_PRODUCTION_LINE.md` — the data-only character build; `Docs/IRONICS_MAP_MODE_SPEC.md §5` — mode==Experience.
</content>
