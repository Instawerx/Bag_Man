# IRONICS_VR_SSOT.md
**Programme: IRONICS VR (PCVR) — Single Source of Truth**
Status: **LIVE** · Created 2026-09-02 · Supersedes: nothing (first VR doc)
Authority: This document is authoritative for all VR-lane work. Newest SSOT supersedes older docs; where silent, prior SSOTs fill gaps (`IRONICS_LOBBY_HUB_SSOT.md`, `IRONICS_CHARACTER_CREATOR_SSOT.md`, `IRONICS_PRICING_SSOT.md`).

---

## 1. Purpose

Deliver a fully playable PCVR (OpenXR/SteamVR) release of IRONICS with full cross-platform sync: one identity, one economy, one server fleet, one matchmaking pipeline shared between flat and VR clients. VR is an **entrance to the existing game, not a rewrite** — the same doctrine that governs the Lobby Hub.

---

## 2. Rulings (operator, 2026-09-02 — locked)

| ID | Ruling | Consequence |
|----|--------|-------------|
| **R-VR-1** | **PCVR first.** OpenXR on the existing Win64 client. Quest standalone = separate future programme, decided after PCVR ships. PSVR2 out of scope. | One build, one cook, one backend. No Android target, no Meta cert. |
| **R-VR-2** | **Mixed pools.** VR and flat clients share all queues, including staked cells. | Zero matchmaking work required for V1. VR clients submit identical FlexMatch tickets. An input-modality attribute is logged for telemetry only (no routing effect). |
| **R-VR-3** | **Lumen-off acceptable but not preferred.** Decision is **measured, not assumed**: VR-0 captures in-headset GPU timings on min-spec in both configs. Log is the arbiter. Default VR tier if Lumen cannot hold 90Hz: Lumen off, baked GI + emissive + bloom parity pass. | See L-VR-5. Renderer never forks (deferred only). |
| **R-VR-4** | **Full motion embodiment.** Motion-controller aim, tracked hands, HMD camera. No "flat game in headset" mode. | Aim-source seam, pose replication, upper-body IK all in scope. |
| **R-VR-5** | **VR is the active programme now.** All prior programme upgrades complete. The 2-client replication carry-forward debt is paid inside VR-3 (hard prerequisite for pose-replication proof). | VR-0 starts immediately. |
| **R-VR-6** *(default so nothing blocks — operator may override)* | **Min-spec:** RTX 3070 / Ryzen 5 class @ 90Hz. Reference HMDs: Quest 3 via Link (primary), any SteamVR OpenXR headset (secondary). | All perf gates measure against this target. |

---

## 3. Laws (L-VR-1 … L-VR-8)

- **L-VR-1 — The HMD owns the camera.** In VR the LyraCameraMode stack is bypassed entirely; nothing ever writes the camera transform against the HMD. The VR camera path is a parallel path, not a modification of the stack (flat clients keep the stack untouched).
- **L-VR-2 — P-CONTROLS doctrine extends to VR.** VR mechanics attach via GameFeature-added `UActorComponent`s reading the **stock CMC** via `GetCharacterMovement()`. No CMC subclass, no reparent. (`UAFLCharacterMovementComponent`/`AAFLCharacter` remain dead code.)
- **L-VR-3 — Net-serialization law applies.** Every new net-serialized VR struct (head pose, hand poses, aim source) lives in an **always-loaded non-GameFeature module**. GameFeature-resident structs desync `FNetSerializeScriptStructCache` indices and drop connections; single-client PIE never catches this.
- **L-VR-4 — Aim-source seam.** Abilities consume an aim-source abstraction (`IAFLAimSource`): camera/control rotation for flat, right-motion-controller muzzle transform for VR. Post-seam, **no ability reads camera rotation directly.** Server-side shot validation accepts both origins with sanity bounds (origin within tolerance of replicated hand pose).
- **L-VR-5 — Single deferred renderer.** VR differs from flat by **scalability tier only** (device profile / CVar tier). Forward+MSAA fork is permanently rejected: it forks every material, forecloses Lumen, and doubles the visual maintenance surface for a solo builder. TAA/TSR beam ghosting is mitigated in-tier (responsive AA flags, TSR tuning), verified visually at VR-1.
- **L-VR-6 — Nothing regresses the proven seams.** `#43` ServerSetCosmeticSelection, ClientRequestPurchase (the one purchase path), `IAFLCosmeticPersistence`, the FlexMatch pipeline, EOS identity: **untouched**. VR clients traverse these seams identically to flat clients. Any VR task whose diff touches a seam is quarantined, not patched.
- **L-VR-7 — Proof standard extends to VR.** ✅ = demonstrated **in-headset, operator-watched**. Replication claims require **flat client + VR client on a dedicated server** (a VR client watching a VR client proves nothing about cross-modality). "Compiles" is never proof. Disk is the arbiter; each assertion cites its own log line.
- **L-VR-8 — Comfort law.** No forced camera translation/rotation the player did not input. Snap turn + smooth locomotion + comfort vignette ship as options from VR-1. **MotionWarping abilities (climb/mantle) are disabled in VR until they receive a VR treatment** (fade-through or camera decouple) and pass an in-headset comfort watch. WarpToLedgeTop translation warp yanking the HMD is a hard fail.

---

## 4. What is already solved (do not rebuild)

| Concern | Status | Why |
|---|---|---|
| Identity | ✅ solved | EOS/Epic OAuth is client-agnostic. VR client logs in identically. |
| Economy / entitlements / cosmetics | ✅ solved | PlayFab behind `IIronicsIdentity` + `IAFLCosmeticPersistence`. Buy flat, wear in VR — zero new backend. |
| Servers | ✅ solved | GameLift dedicated servers are client-blind. VR client = another Lyra client. |
| Matchmaking | ✅ solved (per R-VR-2) | Mixed pools = identical tickets. FlexMatch standalone has no queue cap; telemetry attribute is additive config. |
| Cross-platform sync | ✅ solved by construction | One account, one wallet, one fleet. "Fully syncs" is a property of the existing architecture, not new work. |

---

## 5. New work — system impact map

| # | System | Work | Size | Phase |
|---|--------|------|------|-------|
| 1 | OpenXR bring-up | Plugin enable, VR Preview functional, perf probe both Lumen configs | S (config) | VR-0 |
| 2 | VR pawn & camera | HMD camera path (L-VR-1), motion controller components, GameFeature-attached VR component layer (L-VR-2) | XL | VR-1 |
| 3 | Locomotion & comfort | Smooth loco + snap turn + vignette on stock CMC; MotionWarping VR gating (L-VR-8) | L | VR-1 |
| 4 | Aim-source seam | `IAFLAimSource` + refactor of ability targeting; server validation of controller-space origins | L | VR-2 |
| 5 | Pose replication + IK | Head/hand pose structs (always-loaded module, L-VR-3) → upper-body IK on ABP_IRONICS (Control Rig, per `ue5-interaction-ik-expert` doctrine) | L–XL | VR-3 |
| 6 | 2-client carry-forward debt | Lag-comp, death via PawnExt-fallback, cue replication, beam endpoint — paid here | L | VR-3 |
| 7 | UI in VR | Out-of-match: activatable CommonUI stack hosted on a world-space/stereo-layer quad with input routing (one UI codebase preserved). In-match HUD: head-locked quad or diegetic elements | XL | VR-4 |
| 8 | VR scalability tier | Locked CVar tier per R-VR-3 measurement; beam-material AA mitigation; 90Hz hold on min-spec | L | VR-5 |
| 9 | Matchmaking telemetry attribute + Steam VR store flag | Config only | S | VR-5 |

**Explicitly out of scope for V1:** Quest standalone, PSVR2, VR spectating/replay, VR-exclusive cosmetics, hand-physics toys in the hub, any seam modification.

---

## 6. Phase gates (canary-before-scaling; each gate operator-watched, tracker-recorded)

| Gate | Proof required (✅ definition) |
|------|-------------------------------|
| **VR-0** | HMD renders Arena_01 in VR Preview PIE. GPU timing logs captured on min-spec for config A (Lumen as-shipped) and config B (Lumen off). R-VR-3 resolved from the log. |
| **VR-1** | Operator, in headset, on a controllable VR pawn: HMD camera correct, smooth loco + snap turn + vignette functional, no camera fights, no comfort fails on flat ground. MotionWarping abilities confirmed gated off in VR. |
| **VR-2** | Operator fires Pulse from motion-controller aim at a target in-headset; hit registers via the normal GAS/damage path; a flat PIE client on the same session confirms server accepted the controller-origin shot. |
| **VR-3** | Dedicated server + flat client + VR client: flat client watches the VR client's head and hands animate live. Net-serialization module placement verified on disk. Carry-forward debt items each individually ✅ with their own evidence lines. |
| **VR-4** | Operator, in headset, completes one full store purchase (buy→deduct→grant→entitle→equip→render) through the VR-hosted UI, traversing the proven seams unmodified. |
| **VR-5** | 90Hz held on min-spec through a full match loop (matchmake → travel → fight → extract → return to hub) in-headset. Insights capture on disk. Steam build with VR flag cooks and launches. |

Each gate writes its verdict to `BAG_MAN_LIVE_TRACKER.html` with evidence lines and lane attribution before the next phase opens.

---

## 7. Lane assignments

- **Claude Code (primary):** all C++/config via worktrees (sparse-checkout, LFS skip-smudge, no editor on worktrees); ABP/Control Rig and UI asset work via direct editor connection (one session at a time); read-only subagent fan-out for read passes.
- **AIK (fallback):** only on a failed capability probe (candidate: Control Rig internals for hand IK if the editor connection cannot author them). Named per ticket if invoked.
- **Operator:** UBT regen + builds (editor closed), all in-headset watching, commits/tags (per-file staging, triple-hash verify), rulings.
- **Claude:** orchestration, proof standards, paste-ready blocks, this SSOT.

Engine note: all VR editor/PIE work runs on **C: LAUNCHER** (5.6.1). Any D: SOURCE session (dedicated-server builds for VR-3) ends with the C: launcher `LyraEditor` rebuild before the editor reopens — standing rule, doubly load-bearing here because VR-3 alternates between server builds and in-headset PIE.

---

## 8. Open items

- R-VR-6 min-spec confirmation (default stands unless overridden).
- UI host mechanism selection (world-space `UWidgetComponent` vs stereo layer) — decided at VR-4 open, after a Claude Code capability probe on CommonUI input routing to a world-space host.
- MotionWarping VR treatment design (fade vs decouple) — designed during VR-1, gated by L-VR-8.
- Steam store VR entrance/metadata — VR-5.
