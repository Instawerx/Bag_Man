# IRONICS_VR_TASKS.md — AFL-34xx ticket set
Status: **LIVE** · 2026-09-02 · Governed by `IRONICS_VR_SSOT.md` (rulings R-VR-1…6, laws L-VR-1…8)
Milestone: **IRONICS VR (PCVR)** · Phases VR-0 → VR-5. No phase opens until the prior gate is ✅ on the tracker.

Keystone tickets are written in full AFL format; conforming tickets are condensed. Every acceptance criterion cites a disk fact or a law ID. ✅ = per L-VR-7 (in-headset, operator-watched; replication = flat client + VR client on dedicated server).

---

## Phase VR-0 — Capability probe

```
[AFL-3400] Enable OpenXR and capture the two-config perf probe
Type:        Research
Discipline:  Engineering
Priority:    P0
Estimate:    S (config) + operator watch session
Sprint:      VR-0
Milestone:   IRONICS VR
Branch:      feature/vr0-openxr-probe
Depends On:  None
Blocks:      Everything (VR-0 gate)

## Context
Grounds the entire programme in measured fact. Resolves R-VR-3 (Lumen in the
VR tier) from a GPU timing log instead of assumption.

## Acceptance Criteria
- [ ] Bag_Man.uproject enables the OpenXR plugin (diff shown; operator commits)
- [ ] VR Preview PIE renders Arena_01 to the HMD (operator-watched)
- [ ] Config A (as-shipped, Lumen on) GPU timings captured to log on min-spec (R-VR-6)
- [ ] Config B (r.DynamicGlobalIlluminationMethod 0, r.ReflectionMethod 0) captured same session
- [ ] R-VR-3 verdict written to the tracker citing the two log excerpts
- [ ] No rendering-config change is committed in VR-0 beyond plugin enable

## Technical Notes
Config-only worktree task. No C++. Probe checklist doc authored for the operator
(stat gpu / stat unit / profilegpu capture points, both configs, same vantage).
Logs read AFTER PIE closes per durable-PIE workflow.

## Definition of Done
- [ ] Tracker row VR-0 written with evidence lines + lane attribution
- [ ] Plugin-enable diff committed by operator (code-only commit, no content)
```

---

## Phase VR-1 — VR pawn, camera, locomotion, comfort

```
[AFL-3410] VR camera path — HMD-owned camera bypassing LyraCameraMode (L-VR-1)
Type:        Feature
Discipline:  Engineering
Priority:    P0
Estimate:    L
Branch:      feature/vr1-hmd-camera
Depends On:  AFL-3400
Blocks:      AFL-3411..3415

## Context
Lyra's camera-mode stack must never fight the HMD. Flat clients keep the stack
untouched; VR clients take a parallel camera path.

## Acceptance Criteria
- [ ] In-headset: head tracking drives the view 1:1, zero stack interference (operator-watched)
- [ ] Flat PIE client in the same build shows zero camera behavior change (non-regression watch)
- [ ] No modification to stock camera-mode classes (diff inspection)

## Technical Notes
GameFeature-attached VR component layer per L-VR-2 (Transient+Replicated via
GameFeatureAction_AddComponents, same OOB-correct pattern as the MotionWarping
component). Stock CMC untouched.
```

```
[AFL-3411] Motion controller components + tracked hands on the VR pawn
Type:        Feature
Discipline:  Engineering
Priority:    P0
Estimate:    L
Branch:      feature/vr1-motion-controllers
Depends On:  AFL-3410

## Acceptance Criteria
- [ ] Both controllers tracked, visible hand meshes follow 1:1 in-headset
- [ ] Enhanced Input actions fire from controller inputs through the existing
      ULyraInputConfig path (log-verified), no parallel input system invented
- [ ] Hand transforms exposed for AFL-3430 (aim) and AFL-3440 (replication) consumers
```

Condensed VR-1 set:

| ID | Title | D | E | Notes |
|----|-------|---|---|-------|
| AFL-3412 | Smooth locomotion + snap turn on stock CMC via VR component (L-VR-2) | eng | M | HMD-relative or controller-relative move dir = player option |
| AFL-3413 | Comfort vignette (loco-triggered), exposed as player option | eng | M | L-VR-8 |
| AFL-3414 | Gate MotionWarping abilities (climb/mantle, dash review) OFF in VR pending VR treatment | eng | S | L-VR-8 hard requirement; tag-gated per-modality |
| AFL-3415 | VR-1 gate watch: in-headset pawn acceptance (camera, loco, comfort, gating) | qa | M | Writes VR-1 tracker row |

---

## Phase VR-2 — Aim-source seam

```
[AFL-3420] IAFLAimSource seam — abstract ability aim origin (L-VR-4)
Type:        Feature
Discipline:  Engineering
Priority:    P0
Estimate:    L
Branch:      feature/vr2-aim-source
Depends On:  AFL-3411
Blocks:      AFL-3421, AFL-3422

## Context
Abilities currently derive targeting from camera/control rotation. VR aims from
the right motion controller. One seam serves both; no ability reads the camera
directly post-seam.

## Acceptance Criteria
- [ ] IAFLAimSource lives in an always-loaded non-GameFeature module (L-VR-3 by
      construction if any struct nets); path cited on disk
- [ ] Flat client: aim behavior byte-identical to pre-seam (regression fixture
      drives the real changed code path, not an inline probe)
- [ ] All shipped ability targeting paths route through the seam (grep-verified:
      zero direct camera-rotation reads remain in ability code)
```

| ID | Title | D | E | Notes |
|----|-------|---|---|-------|
| AFL-3421 | Server validation: accept controller-space shot origins with tolerance vs replicated hand pose | eng | M | Anti-cheat sanity bounds; G4 doctrine |
| AFL-3422 | VR-2 gate: in-headset Pulse fire from controller aim, hit via normal GAS/damage path; flat client confirms server acceptance | qa | M | Writes VR-2 tracker row |

---

## Phase VR-3 — Pose replication, IK, and the carry-forward debt

```
[AFL-3430] Replicated head/hand pose — always-loaded module (L-VR-3)
Type:        Feature
Discipline:  Engineering
Priority:    P0
Estimate:    L
Branch:      feature/vr3-pose-replication
Depends On:  AFL-3411
Blocks:      AFL-3431, AFL-3435

## Context
Other players must see a VR player's head/hands. Net-serialized pose structs are
exactly the class of struct the net-serialization law exists for.

## Acceptance Criteria
- [ ] Pose structs live in an always-loaded non-GameFeature module — module path
      + Build.cs cited on disk BEFORE any client test is trusted
- [ ] Quantization/send-rate chosen and documented (bandwidth line in evidence)
- [ ] Dedicated server + flat client + VR client: flat client receives live pose
      updates (log-verified marker cadence)
```

```
[AFL-3431] Upper-body IK from replicated poses on ABP_IRONICS
Type:        Feature
Discipline:  Animation
Priority:    P0
Estimate:    XL
Branch:      feature/vr3-upperbody-ik
Depends On:  AFL-3430

## Context
ABP_IRONICS (owned fork, FOUNDATION-4b) is the substrate; Control Rig hand-IK
doctrine per ue5-interaction-ik-expert and locked decisions K–P.

## Acceptance Criteria
- [ ] Flat client watches VR client: head aim + both hands track plausibly, no
      candy-wrapper deformation, locomotion lower body unaffected (operator-watched)
- [ ] Editor-connection capability probe for Control Rig authoring recorded; AIK
      invoked as fallback ONLY on probe failure, named on this ticket if so
```

Carry-forward debt (each item earns its own evidence line — never fused):

| ID | Title | D | E |
|----|-------|---|---|
| AFL-3432 | Lag-comp proof under real latency (2-client dedicated, netem or region latency) | eng | L |
| AFL-3433 | Player death via PawnExt-fallback — replicated, both clients observe | eng | M |
| AFL-3434 | Cue replication + beam endpoint correctness on remote client | eng | M |
| AFL-3435 | VR-3 gate watch: full 3-way session (server + flat + VR), all rows written individually | qa | L |

---

## Phase VR-4 — UI in VR

```
[AFL-3440] UI host capability probe + mechanism ruling
Type:        Research
Discipline:  Engineering
Priority:    P0
Estimate:    M
Branch:      feature/vr4-ui-probe
Depends On:  VR-1 gate
Blocks:      AFL-3441

## Context
The activatable CommonUI stack is screen-space. Decision: world-space
UWidgetComponent host vs stereo layer, judged on CommonUI input routing
(analog cursor / focus navigation) actually working, not on rendering alone.

## Acceptance Criteria
- [ ] One existing activatable screen hosted and INTERACTABLE in-headset via
      motion controller (operator-watched) — the probe checks capability, not presence
- [ ] Mechanism ruling recorded in the SSOT §8 with the probe evidence
```

| ID | Title | D | E | Notes |
|----|-------|---|---|-------|
| AFL-3441 | VR UI host component — one codebase, activatable stack rendered per ruling | eng/ui | XL | No widget forks; flat path untouched |
| AFL-3442 | In-match HUD VR treatment (health/heat/hit-marker as head-locked quad or diegetic) | ui | L | Hit-confirm message path (Event.Damage.Confirmed) reused, presentation-only |
| AFL-3443 | VR-4 gate: full purchase chain in-headset (buy→deduct→grant→entitle→equip→render), seams unmodified (diff inspection = zero seam files touched) | qa | M | L-VR-6 enforcement; writes VR-4 tracker row |

---

## Phase VR-5 — Performance lock + release

| ID | Title | D | E | Notes |
|----|-------|---|---|-------|
| AFL-3450 | VR scalability tier locked per R-VR-3 verdict; device profile/CVar tier committed | eng | M | L-VR-5: tier, never a renderer fork |
| AFL-3451 | Beam/translucency AA mitigation in VR tier (responsive AA, TSR tuning), visual watch in-headset | vfx | M | Thin-bright-beam ghosting is the named risk |
| AFL-3452 | 90Hz hold on min-spec through full match loop; Insights capture on disk | eng | L | VR-5 gate evidence |
| AFL-3453 | FlexMatch input-modality telemetry attribute (logging only, zero routing effect per R-VR-2) | eng | S | Backend repo; SSM-config doctrine applies |
| AFL-3454 | Steam VR store flag + launch config; cook + launch verified from the shipped build | devops | M | Ship-cook already green; this is flag/config |
| AFL-3455 | VR-5 gate watch + programme retrospective row | qa | M | Writes VR-5 tracker row |

---

## Dependency spine (critical path)

AFL-3400 → 3410 → 3411 → { 3420 → 3421 → 3422 } and { 3430 → 3431 → 3435 } → 3440 → 3441 → 3443 → 3450 → 3452 → 3454

Locomotion/comfort (3412–3414) parallel after 3410. Carry-forward debt (3432–3434) parallel inside VR-3 once the 3-way session harness exists. HUD (3442) parallel with 3441.
