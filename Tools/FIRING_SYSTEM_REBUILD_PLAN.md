# AFL Firing System — Rebuild-to-Spec Plan

**Status:** PLAN ONLY. No code, no build, no revert performed in authoring this. For a fresh-session, full-energy execution.
**Authored:** 2026-06-02 (end of a long debugging session — deliberately NOT started tonight).
**Checkpoint / rollback floor:** `3bf573b3` (HEAD = origin/main, synced, pushed). Everything below sits on top of this proven commit.

---

## WHY THIS REBUILD (the case, not a reflex)

The firing/weapon/cue layer produced surprise after surprise this session — each a *real* bug, each *hard to locate*:
1. Weapon mesh invisible on proxies → `bReplicates=false` on the display actor (fixed `1c3cb0f9`).
2. Beam stuck/thrashing → `WhileInputActive` + redundant `WaitInputRelease` re-cycling ~6×/sec (fixed `3bf573b3`).
3. Cue Niagara missing on the 2nd-screen proxy for BOTH Pulse and Beam → unlocatable by inspection; lives in the shared cue→proxy delivery path.

When the BUGS are this hard to locate, the **substrate** is the problem, not the individual bugs. AND — decisively — this is **NOT a blind rewrite**: a documented, canonical, proven spec EXISTS (the 12 tutorial frames + the `afl-laser-beam-system` skill). Rebuild-to-spec ≠ rewrite-and-pray. The current firing layer isn't reliably working; a known-good target is in hand.

**The core architectural divergence (the root of the instability):**
- **Current model:** the cue (`AAFLCueNotify_LaserBeam`) **spawns a Niagara system per fire** (`SpawnSystemAtLocation`, `bAutoDestroy=false`) and **destroys it on cue-remove**. A `Self/Infinite` beam emitter under spawn-and-destroy runs forever if the destroy mis-fires — fragile exactly where every bug landed.
- **Tutorial model (the target):** a **persistent `Niagara_Laser` COMPONENT on the weapon**, `Auto-Activate = OFF`, **Activated** on fire / **Deactivated** on stop, with `Beam End` driven each tick. Deactivate is reliable in a way that destroy-per-fire is not. **Cannot stick, cannot stack.**

---

## KEEP — the proven foundation (DO NOT rebuild; committed + green at `3bf573b3`)

These are hard-won, PIE-proven this session. Re-fighting them would be the real waste.

| System | Files | Why it stays |
|---|---|---|
| **AFLNetTypes** (net-serialized hitscan struct in an always-loaded module) | `Plugins/AFLNetTypes/...AFLAbilityTargetData_Hitscan.{h,cpp}` + module | The GAS `FNetSerializeScriptStructCache` desync fix. Deep, correct, networked-proven. [[reference_weapon_actor_must_replicate_for_proxies]] sibling. |
| **Death spine** | `AFLCombat/.../Combat/AFLDeathComponent.{h,cpp}` | AFL-native trigger → Lyra replicated `StartDeath`/`FinishDeath`. PIE-proven ragdoll. |
| **Damage pipeline** | `AFLCombat/.../AbilitySystem/AFLDamageExecCalc.{h,cpp}` + `Attributes/AFLAttributeSet_Combat.{h,cpp}` | ExecCalc + Source.Damage seed + Health/Heat attrs. Runtime-verified. |
| **Lag-comp** | `AFLCombat/.../LagComp/AFLLagCompensationWorldSubsystem.{h,cpp}` + `AFLPawnHitboxHistoryComponent.{h,cpp}` | Rewind + ConfirmHit. Non-zero-dt demonstrated 2-client. |
| **Heat GAMEPLAY** | inside `AFLAttributeSet_Combat` (Heat/MaxHeat, State.Overheated) | GAS owns it. NEVER moves to Niagara (renderer can't own gameplay state, no authority/replication). |
| **Lyra base** | GAS/ASC, `ULyraEquipmentManagerComponent` (pawn), `ULyraQuickBarComponent` (controller), experience/pawn data | Equipment + experience substrate. Unmodified-stock = upstream-mergeable. |

**Also KEEP (correct, reusable in the rebuild):**
- **`AFLNetTypes` hitscan target-data** — the published-trace payload; the new abilities reuse it.
- **The `bReplicates=true` lesson** for weapon display actors ([[reference_weapon_actor_must_replicate_for_proxies]]).
- **The `WhileInputActive`-alone lesson** for channels ([[reference_whileinputactive_no_waitinputrelease]]) — NO `WaitInputRelease`.
- **FireAuto trigger-less = hold; Fire+Pressed = per-press** ([[reference_channeled_ability_needs_fireauto_held_trigger]]).

---

## TUTORIAL SPEC — the canonical firing setup (the stable target)

From the 12 frames + `afl-laser-beam-system` skill. This does NOT drift; it is the reference.

### Beam delivery (the model that replaces cue-spawn)
- **A persistent `Niagara_Laser` COMPONENT** lives on the weapon (the tutorial puts it on `BP_Weapon_Component`), **`Auto-Activate = OFF`**.
- Firing = **Activate** the component (+ drive `Beam End`); stop = **Deactivate**. The component LIVES the whole time; it is only toggled.
- `Beam End` (Vector, world-space) driven each tick from the trace impact point. `User.Color` for per-weapon recolor.

### NS asset (`NS_AFL_Laser_Twist` — ALREADY matches the spec, verified)
- Beam-ribbon emitter `DynamicBeam002` = **`Self/Infinite`** (persistent line, has `Update Beam`).
- Spark emitter `sparks` = **self-expiring, `Initialize Particle` Lifetime ~0.1–0.4** (the crackle / "reset cycle").
- Plus `Glow` + `ShockWave_Cast` (richer than the minimal tutorial — superset, fine).
- User params: `Beam End` + Color. **THE ASSET IS DONE — no NS authoring needed.** (Verified via `info()` + emitter screenshot this session.)

### Pulse / Tracer delivery
- **Pulse = per-press hitscan** (`Fire`/`InputTriggerPressed`); one-shot tracer/muzzle/impact.
- **The 2nd-screen proxy regression is the open question for the rebuild** — Pulse's one-shot `K2_ExecuteGameplayCue` and Beam's persistent component must BOTH render on simulated proxies. The rebuild must establish the **canonical cue→proxy delivery** as a first-class requirement, verified in 2-client before cutover (see Clean-Room §).

### The doctrine boundary (unchanged, governs the whole rebuild)
> Gameplay owns the trace + damage. Cosmetics own the beam. They meet at a world-space point + color. Gameplay never touches Niagara; cosmetics never touch attributes. Cooldown = GE. Cleanup in EndAbility.

---

## REBUILD SCOPE — exactly what gets replaced

**REPLACE (the firing/cue layer — where every bug lived):**

| File | Current model | Rebuild to |
|---|---|---|
| `AFLVFX/.../AFLCueNotify_LaserBeam.{h,cpp}` | cue spawns/destroys NS per fire | persistent-component toggle model (or retired if delivery moves weapon-side) |
| `AFLVFX/.../AFLCueNotify_LaserBeamFlash.{h,cpp}` | burst cue | re-evaluate vs unified delivery |
| `AFLVFX/.../AFLCueNotify_LaserImpact.{h,cpp}` | burst cue | re-evaluate vs unified delivery |
| `AFLCombat/.../Abilities/AFLAG_Laser_Beam.{h,cpp}` | channel ability + cue-add | unified channel fire-mode (drives the toggled component) |
| `AFLCombat/.../Abilities/AFLAG_Laser_Pulse.{h,cpp}` | per-press hitscan + execute-cue | unified per-press fire-mode |
| `AFLCombat/.../Beam/AFLBeamChannelComponent.{h,cpp}` | published-value bridge (replicated endpoint) | **likely KEEP/adapt** — the published endpoint is doctrine-correct; the persistent NS component reads it |

**KEEP from the firing layer (correct pieces):**
- `AFLBeamChannelComponent` (the published-value bridge) — the replicated endpoint is the right boundary; the rebuild's persistent component reads it the same way. Adapt, don't discard.
- `AFLLaserVisualProvider` / `AFLLaserVisualData` (the interface + data asset) — the recolor/visual-data seam.
- `AFLAbilityTargetData_Hitscan` (in AFLNetTypes) — the trace payload.

---

## UNIFIED SYSTEM SHAPE — one framework, fire-mode variants (the AAA deliverable)

The rebuild produces the unified firing system the operator wants — not just a beam patch.

- **One weapon framework** (Lyra `ULyraRangedWeaponInstance` child) parameterized by **fire mode**:
  - **Per-press hitscan** (handgun / Pulse): `Fire`/Pressed, one-shot cue.
  - **Sustained channel** (beam / rifle-auto): `FireAuto`/trigger-less, `WhileInputActive`-alone, toggled persistent NS component.
  - **Projectile** (grenade-launcher): per-press, spawns a replicated projectile (new — future fire-mode).
- **Shared cosmetic delivery** that works on owner AND proxy (the regression's permanent fix): one canonical path for muzzle→target visuals, recolorable per weapon via `User.Color`.
- **Per-weapon = Lyra data** (ID → EquipmentDefinition → WeaponInstance + AbilitySet), so handgun/rifle/launcher are *data variants* of the one framework, stamped fast once the framework is proven.

---

## CLEAN-ROOM APPROACH — verifiable, not big-bang

1. **Build the new firing layer ALONGSIDE** the current one (new classes/module; e.g. a 3rd weapon as the clean-room canary), so the proven `3bf573b3` weapons keep working as the control.
2. **Prove the new layer in 2-client PIE against the spec** BEFORE touching the existing weapons:
   - Persistent component toggles cleanly (sustain on hold, stop on release, no stick/stack).
   - **Cue/visual shows on the CLIENT proxy** (mesh + beam + tracer + impact + death/ragdoll) — the regression's acceptance gate.
   - Heat builds continuously (one channel, not N re-applies).
   - Recolors per weapon (`User.Color`).
3. **Cut over** the existing weapons to the proven framework only AFTER the clean-room canary passes 2-client. Retire the old cue-spawn classes in the same cutover.
4. **Then stamp** handgun/rifle/grenade-launcher as data variants.

**Acceptance gate for the whole rebuild:** 2-client listen-server, watch the CLIENT proxy — both fire modes show mesh + cue + hit + death, beam sustains-not-sticks, heat sane. The thing this session could never confirm becomes the cutover requirement.

---

## SKILLS GOVERNING EACH PART
- **`afl-cpp-lyra-developer`** → the Lyra-canonical ability/equipment/cue C++ patterns; the unified weapon framework + fire-mode abilities.
- **`afl-laser-beam-system`** → the tutorial spec (persistent toggled component, NS contract, the one-rule boundary); the beam delivery.
- **`unreal-engine-expert`** → GAS/Niagara/replication correctness; the cue→proxy delivery fix; per-pawn cue instancing.
- **`lyra-ue5-build-discipline`** → PIE-✅ doctrine, one-layer-at-a-time, the clean-room/cutover sequencing.

---

## RESOLVED DECISIONS (operator, 2026-06-02 AM — record the reasoning)

### Q1 = (b) DOCTRINE TRACE — SETTLED, non-negotiable
The beam endpoint comes from the **authoritative server-side trace**, published via the
replicated `AFLBeamChannelComponent`; the cosmetic component READS the point. Rejected the
tutorial-literal (a) BP/weapon `Break Hit Result` trace: a client-side cosmetic trace does NOT
replicate and violates the GAS authority boundary. **The tutorial is single-player demo code; it
does not carry our multiplayer constraint.** Doctrine wins.

### Q2 = (a) PERSISTENT NS COMPONENT ON THE REPLICATED WEAPON ACTOR — SETTLED (load-bearing)
This is THE load-bearing decision of the rebuild. Reasoning to record exactly:
- The **cue→proxy regression from last night was NEVER root-caused** — confirmed visually,
  config + committed code ruled out, but the actual mechanism never pinned. It is an UNSOLVED bug
  in the cue→proxy delivery path.
- Option (b) (spawn-once-by-cue-then-toggle) would keep building on **that same unsolved path.**
- Option (a) routes the visual through the **WEAPON ACTOR'S replication** — which we PROVED reaches
  proxies (commit `1c3cb0f9`, the `bReplicates=true` fix, 2-client-verified). So (a) is chosen
  because it STOPS building on the unsolved cue→proxy bug and rides the replication channel we
  already proved works. **The permanent fix by construction.**

**CRITICAL CAVEAT (do NOT let (a) become a false win):** a Niagara component existing on a
replicated actor does NOT automatically mean its ACTIVATION/TOGGLE state replicates to proxies the
way we need. The actor replicating is **necessary, not obviously sufficient.** WHEN and HOW the
component toggles on the proxy must be **explicitly designed AND proven on the 2nd screen — not
assumed.** So (a) is the architecture; its payoff is CONDITIONAL on the 2-client proxy watch
passing. **The proxy watch is the gate that confirms (a) actually FIXED the regression vs just
MOVED it.** Design the toggle-replication deliberately (e.g., the published `bBeamActive` on the
replicated `AFLBeamChannelComponent` drives the component's Activate/Deactivate via OnRep on every
machine — so the toggle rides the same replicated value, not a client-local guess) and prove it.

### Q3 = RESOLVED (2026-06-02 AM): uncommitted edits cleared
Working tree cleaned to `3bf573b3` + 2 deliberate keepers (IRONICS `DefaultGame.ini`, the map).
The discardable churn (`DefaultEngine.ini` cook-setting + carried `NS_*` mods) is in `stash@{0}`
(recoverable). `NS_AFL_Laser_Twist` already at `3bf573b3`-good. Tree is clean; no floating edits.

---

## CANARY FIRST-SLICE — clean-room, proves the architecture end-to-end (SCOPE ONLY, no build)

The smallest clean-room weapon that exercises the EXACT thing that must work. Built ALONGSIDE the
existing layer (new classes; existing weapons stay as the control at `3bf573b3`).

### What the canary is
- **One weapon, channel fire-mode (beam)** — the harder case (sustained + toggle), so passing it
  validates the model the per-press case trivially inherits.
- **NEW clean-room classes** (do not touch `AFLAG_Laser_Beam` / `AFLCueNotify_LaserBeam` yet):
  - A new channel ability (e.g. `UAFLAG_BeamChannel_v2`) — `WhileInputActive`-ALONE (NO
    `WaitInputRelease` — the thrash lesson), `FireAuto` trigger-less binding, doctrine trace →
    publishes endpoint + `bBeamActive` to `AFLBeamChannelComponent` (Q1=b, KEEP/adapt the bridge).
  - A new weapon display actor (child of the canonical `B_Weapon`, or `bReplicates=true` set) that
    **carries a persistent `Niagara_Laser` component, Auto-Activate OFF** (Q2=a).
  - The component's **Activate/Deactivate driven by the replicated `bBeamActive`** (the deliberate
    toggle-replication design the caveat demands). SEE THE TWO SHARP EDGES BELOW — they are the
    spots the proxy watch tests hardest; design them in BEFORE the build.
  - Reuse the verified `NS_AFL_Laser_Twist` asset (already spec-compliant: Self/Infinite ribbon +
    self-expiring sparks + Beam End/Color). NO NS authoring.

### TWO SHARP EDGES — design these in before the build (the proxy watch will test them hardest)

**EDGE 1 — OnRep does NOT fire on the server; the listen-host is a watched player.**
`OnRep_bBeamActive` fires ONLY on remote clients receiving the change — NOT on the server that
set the value. In the listen-server test, the host is server + a watched player. If
Activate/Deactivate lived ONLY in OnRep, the beam would toggle on the client screen but NOT on the
host's own → a FALSE "works on client, not host" partial-regression at the watch. **Required
design (set-and-call-locally + OnRep-for-remotes):**
- A SHARED function `ApplyBeamActiveState(bool)` does the actual component Activate/Deactivate +
  tick-enable. NEITHER path inlines the logic — both CALL this one function.
- **Server/authority:** when the ability sets `bBeamActive`, it ALSO calls `ApplyBeamActiveState()`
  locally immediately (so the host-as-player's own screen toggles).
- **Remote client:** `OnRep_bBeamActive` calls the SAME `ApplyBeamActiveState()` (so proxies toggle).
- Result: host-as-player, owning client, AND simulated proxy all run identical toggle logic. The
  classic OnRep-only "works on client not host" bug is designed OUT.

**EDGE 2 — `Beam End` is a SEPARATE replicated value (per-tick vector); cadence ≠ framerate.**
The toggle is a bool (flips twice → OnRep is the right tool). The endpoint is a VECTOR moving every
tick while firing — a different replication problem:
- `BeamImpactPoint` is a replicated `FVector_NetQuantize` UPROPERTY on `AFLBeamChannelComponent`
  (already exists). It replicates via NORMAL property replication, gated by `NetUpdateFrequency` +
  bandwidth — **NOT per-frame.** Expected proxy cadence ≈ NetUpdateFrequency (~10–30 Hz), not render fps.
- **EXPECTED at the watch:** if the beam APPEARS on the proxy and TOGGLES OFF correctly but the
  ENDPOINT visibly lags/steps behind the host's smooth endpoint → that is EXPECTED-AND-TUNABLE
  (raise NetUpdateFrequency, or client-side interpolate the endpoint, or accept it — cosmetic
  polish). NOT a toggle failure. `FVector_NetQuantize` already keeps the payload tight; cadence is
  the only knob.
- **DO NOT CONFLATE:** toggle (bool/OnRep) = CORRECTNESS, must work crisply (beam appears + toggles
  off). Endpoint (vector/property-rep) = SMOOTHNESS, cadence-bound, tunable. Endpoint stutter is NOT
  a toggle bug. Beam-absent or toggle-stuck IS (Edge 1).
- **Per-weapon recolor** via the NS `User.Color` (data, not code).

### ACCEPTANCE GATE — explicit, the whole point of the canary
**2-client listen-server proxy watch.** Host fires the canary beam; on the CLIENT screen, watching
the HOST's pawn (proxy):
1. Weapon **mesh** shows on the proxy (rides weapon-actor replication).
2. Firing **beam Niagara** shows on the proxy, tracking muzzle→target (toggle-replication works).
3. The beam **toggles OFF** on release/end on the proxy (OnRep drives Deactivate everywhere).
4. (Plus: sustains one continuous channel, not thrash; heat builds sane; damage/death intact.)

- **PASS** → the architecture is PROVEN (Q2=a's caveat cleared) → cut the existing weapons over to
  the v2 framework, retire the old cue-spawn classes, then stamp variants (handgun/rifle/launcher
  as data variants).
- **FAIL** → caught in ONE clean-room weapon, not across the system. Debug the toggle-replication
  THEN (before cutover). The existing weapons stay untouched at `3bf573b3` as the control.

### Skills governing the canary
- `afl-cpp-lyra-developer` → the new channel ability + weapon-display-actor C++ (Lyra ability/
  equipment patterns, the `WhileInputActive`-alone + FireAuto wiring).
- `afl-laser-beam-system` → the spec: persistent toggled component, the NS contract, the one-rule
  boundary (gameplay traces, cosmetic reads).
- `unreal-engine-expert` → the toggle-replication design (OnRep `bBeamActive` → Activate/Deactivate
  on all machines), GAS/Niagara/replication correctness — the part the caveat hinges on.
- `lyra-ue5-build-discipline` → PIE-✅ (the proxy-watch gate), one-layer-at-a-time, clean-room/cutover.

---

## CHECKPOINT (stated explicitly)
- **Rollback floor: `3bf573b3`** — committed AND pushed to origin/main, synced (0 ahead). The proven foundation. Any rebuild misstep recovers here.
- **Recoverable:** the broken heat-edited beam NS is backed up at `%TEMP%\NS_AFL_Laser_Twist_HEAT_BROKEN.uasset` (operator's earlier solo save) if any heat idea is worth salvaging.
- **Nothing destructive done in authoring this plan.** No revert, no stash, no build.
