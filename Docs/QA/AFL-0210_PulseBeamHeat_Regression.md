# AFL-0210 — Pulse + Beam + Heat Regression Matrix

> Sprint 2 closeout regression plan for the combined laser-weapon surface:
> Pulse (AFL-0104/0105/0106/0107), Beam (AFL-0206/0208), and the shared
> Heat attribute (AFL-0207). Authored from
> `Docs/QA/AFL_TEST_PLAN_TEMPLATE.md` and structured to extend the
> AFL-0111 Pulse smoke test (`AFL-0107_PulseSmokeTest.md`) into a full
> per-scenario matrix.
>
> **This document ships the plan, not the run.** A human QA pass fills
> the *Actual* and *Pass/Fail* columns in §5. Dismemberment regression
> is owned by AFL-0411 and is out of scope here.

---

## 1. Identification

| Field | Value |
| --- | --- |
| Plan ID | `AFL-0210_PulseBeamHeat_Regression` |
| Linked ticket | AFL-0210 |
| Authoring ticket | AFL-0210 (Sprint 2 regression matrix) |
| Sprint | S2 |
| Discipline owner | QA |
| Engine | UE 5.6 |
| Branch under test | `yolo/afl-0210-pulse-beam-heat-regression` for the plan itself; future runs use the branch carrying the change under test. |
| Build configuration | LyraEditor Win64 Development (PIE) |
| Predecessor plan | `Docs/QA/AFL-0107_PulseSmokeTest.md` (AFL-0111 template applied to Pulse-only) |

## 2. Scope and intent

The Pulse + Beam + Heat surface is the first place where three
independently-shipped abilities share a single AttributeSet
(`UAFLAttributeSet_Combat`) and a single ASC tag bag
(`Cooldown.Weapon.*`, `State.Overheated`). Each ability's smoke test
passed in isolation; this matrix defends the **interaction surface**
between them, which is the easiest place for a sprint-closeout
regression to slip in.

- **In scope:**
  - Pulse activation, damage, cooldown, miss/no-hit, body-zone scoring
    (P01–P05).
  - Beam channel activation, per-tick damage cadence, cooldown on
    release, drift behaviour, range rejection (B01–B04).
  - Heat accumulation to overheat, decay back below the venting gate,
    cheat-driven overheat, Pulse exemption from heat (H01–H04).
  - 4-client listen-server replication agreement for Pulse damage,
    Beam tick alignment, and Heat state transitions (R01–R03).

- **Out of scope:**
  - Actual execution of the matrix — a human QA tester runs the
    sequence and fills in §5's *Actual* and *Pass/Fail* columns. This
    document only ships the plan.
  - Dismemberment / headshot damage multiplier matrix — owned by
    AFL-0411.
  - Lag compensation behaviour (AFL-0211) — variants here use
    stationary target dummies so a server snapshot equals client view.
  - Real angular-velocity reject (AFL-0213) — today
    `AFL_TELEMETRY: hitscan_reject reason=ang` is log-only.
  - Cosmetic-only FX/SFX polish (AFL-0201..0205, AFL-0208).

- **Defended risks (why this plan exists):**
  - **Shared AttributeSet drift.** Pulse and Beam both target
    `UAFLAttributeSet_Combat`. A change to clamp logic or replication
    condition that fixed one ability can silently break the other.
  - **Tag-bag collisions.** `Cooldown.Weapon.Pulse`,
    `Cooldown.Weapon.Beam`, and `State.Overheated` are all granted by
    GEs on the same ASC; an `ActivationBlockedTags` regression would
    let one weapon fire when it shouldn't.
  - **Heat affecting Pulse.** Master doc §5 states Pulse is
    heat-free. A future heat-cost refactor that puts heat on Pulse
    must trip H04 before it ships.
  - **Beam tick desync under net emulation.** Beam fires N target-data
    submissions per channel; an off-by-one or rounding regression
    silently produces 4-or-6 ticks instead of 5 over 0.5s. R02 is the
    gate.
  - **Cheat-matrix token drift.** AFL-0215's verify gate consumes
    `AFLCombatCheats: OK <Name>` substrings. If a cheat is renamed or
    its log token changes, every regression matrix downstream goes
    green for the wrong reason. P01/B02/H03 each touch a different
    cheat to cover this.

## 3. Preconditions

- [ ] Working tree on the branch under test; no unintended modifications.
- [ ] `Build.bat LyraEditor Win64 Development C:\Dev\Bag_Man\Bag_Man.uproject -Plugin=AFLCombat.uplugin` succeeds with **no new warnings** (warnings-as-errors).
- [ ] `Build.bat LyraEditor Win64 Development C:\Dev\Bag_Man\Bag_Man.uproject -Plugin=AFLAttributes.uplugin` succeeds (if the plugin is in the workspace; skip silently if not present).
- [ ] AFL-0215 lint clean: `python Tools/AFL_Yolo/lint_afl_0215.py`.
- [ ] Required upstream tickets merged:
      AFL-0104, AFL-0105, AFL-0106, AFL-0107 (Pulse stack);
      AFL-0206 (Beam channel); AFL-0207 (Heat); AFL-0208 placeholder VFX
      acceptable as a stand-in.
- [ ] `UAFLCombatCheats` extension auto-registers at PIE start. Confirm
      by tab-completing `AFL.Combat.` in the console — at minimum
      `Damage`, `EnergyGain`, `GrantBeam`, `Heat`, `ForceOverheat`,
      `ResetHeat`, `DumpCombatAttributes` must appear.
- [ ] No prior PIE session bleeding over (close all PIE windows before
      starting a variant).
- [ ] Output Log filter includes: `LogAFLCombat`, `LogAFLCore`,
      `LogCheatManager`. Verbosity set to **Verbose** so `Display`,
      `Log`, and `Verbose` lines all show (the hit-confirm log fires at
      `Verbose` — see §7).

## 4. Setup

1. Open `C:\Dev\Bag_Man\Bag_Man.uproject` in UnrealEditor 5.6. Wait
   for shader compilation to settle (status bar idle).
2. Open the Output Log and apply the category filter from §3.
3. Load PIE map: `L_ShooterGym` (per memory
   `project_afl0110_redirect_shootergym` — Sprint 1/2's testbed; the
   experience is `B_LyraExperience_AFL_Arena_Test`).
4. PIE mode per variant:
   - **Variant A (single-host baseline):** `Number of Players = 1`,
     `Net Mode = Play As Listen Server`.
   - **Variant B (4-client listen-server):** `Number of Players = 4`,
     `Net Mode = Play As Listen Server`.
5. Place (or confirm placed) at least two stationary target dummies
   with `UAbilitySystemComponent`:
   - `Dummy_Near` — directly in front of host spawn, < 20 m away.
   - `Dummy_Far` — past 80 m from host spawn (or wherever the level
     geometry allows a clear line-of-sight beyond the Pulse/Beam
     `MaxRange = 8000 uu`; see §7 note on the brief's "50 m" wording).
   A dedicated `BP_AFL_TargetDummy` is preferable when it lands; until
   then any Lyra-derived pawn that owns an ASC is acceptable.
6. Confirm cheat manager wiring in the Output Log — look for a
   `LogCheatManager:` line referencing `UAFLCombatCheats` during PIE
   start. If absent, **stop**: preconditions failed.
7. For Variant B only, after PIE has spawned all clients, focus the
   host window and run:
   `NetEmulation.PktLag 40` — 40 ms each way ⇒ 80 ms RTT (master
   doc §7 L3 target).

## 5. Test steps — scenario matrix

> One row per scenario. *Actual* and *Pass/Fail* are filled in by the
> human tester at run time; *Notes* captures anything surprising.
> Re-run the **whole table** for each variant in §9 (A, B). For
> R01–R03 only Variant B applies — those rows hard-require 4 clients.

### Pulse scenarios (P01–P05)

| ID | Setup | Steps | Expected | Actual | Pass/Fail | Notes |
|---|---|---|---|---|---|---|
| **P01** | Variant A. Host pawn equipped with Pulse via PawnData AbilitySet. `Dummy_Near` at point-blank range with healthy `H=100`. Output Log idle. | 1. `AFL.Combat.DumpCombatAttributes` to capture `H₀`. 2. Aim at `Dummy_Near` torso. 3. Fire one Pulse shot (`InputTag.Weapon.Fire`, default LMB). 4. `AFL.Combat.DumpCombatAttributes` again. | `LogAFLCombat: Log: AFL_PULSE: Activate` fires once. Damage of exactly **18.0** applied via `UAFLDamageExecCalc` (post-shot `H = H₀ - 18`, clamped at 0). Hit-confirm: `LogAFLCombat: Verbose: hit_confirmed damage=18.0 zone=<bone> headshot=0 distance=<cm>` AND `WBP_AFL_HitMarker` pulses on the host viewport. | | | Brief calls the matcher "AFL_PULSE log fires"; the live token is `AFL_PULSE: Activate` at `Plugins/GameFeatures/AFLCombat/Source/AFLCombat/Private/Abilities/AFLAG_Laser_Pulse.cpp:80`. |
| **P02** | Continuation of P01 (host still aimed at `Dummy_Near`). | 1. Fire Pulse. 2. Immediately fire Pulse a second time (≤ 0.2 s gap). 3. Wait 1.1 s. 4. Fire Pulse a third time. | Shot 1: `AFL_PULSE: Activate` + 18 damage. Shot 2: blocked — no second `AFL_PULSE: Activate` line, no damage change on the dummy, `Cooldown.Weapon.Pulse` is present on the host ASC for the cooldown window. Shot 3 (after 1 s): activates normally. | | | `Cooldown.Weapon.Pulse` is the consumed tag; verify via `AbilitySystem.DebugTag Cooldown.Weapon.Pulse` if needed. |
| **P03** | Variant A. Aim **past** any dummy at empty sky / out-of-range geometry beyond `MaxRange = 8000 uu` (≈ 80 m). | 1. Aim past `Dummy_Far` into open space (or stand back from `Dummy_Far` so the dummy is past `MaxRange`). 2. Fire Pulse. | `AFL_PULSE: Activate` fires once (activation is local, miss-or-hit). **No** damage on any dummy. Brief expects a `AFL_TELEMETRY: hitscan_reject reason=range` line; **the live build does not emit a `reason=range` reject** (current telemetry only emits `reason=ang` / `reason=beam_tick`). See §7 — this row currently passes if the activation log fires and no damage is applied; the `reason=range` matcher is **aspirational** until AFL-0213/0214 wires a real range reject. | | | Brief uses ">50m" prose; the actual `MaxRange` constant is `8000 uu` (= 80 m) on `UAFLAG_Laser_Pulse.h:67`. Use 80 m as the practical threshold. |
| **P04** | Variant A. Aim **past** `Dummy_Near` so the trace continues through empty space (no body) and hits world geometry, OR aim into empty air with no collider in range. | 1. Aim into open sky (no actor along the trace). 2. Fire Pulse. | `AFL_PULSE: Activate` fires. No damage applied (no target). Brief expects `AFL_PULSE: NoHit`; **the live build does not emit a `NoHit` line today** — `AFLAG_Laser_Pulse.cpp` has no `NoHit` token. This row currently passes if the activation log fires, no damage is applied, and `hit_confirmed` is **absent**. The `NoHit` matcher is **aspirational** — file a follow-up to add the log line if this gate is required for downstream automation. | | | See §7 — the closest existing signal is the absence of `hit_confirmed`. Treat that absence as the assertion for now. |
| **P05** | Variant A. `Dummy_Near` placed with clear line-of-sight to its head bone (default Lyra mannequin: `head` / `neck_01`). | 1. Aim at the dummy's head. 2. Fire Pulse. | `AFL_PULSE: Activate` fires. Hit-confirm log: `LogAFLCombat: Verbose: hit_confirmed damage=<n> zone=head headshot=1 distance=<cm>` (the component infers headshot from `HeadshotBoneFragments` — see `AFLHitConfirmComponent.cpp:104` / `:124`). The on-screen `WBP_AFL_HitMarker` shows the **gold-tint** variant. | | | Brief says "zone=Head"; the live log is lower-case (`zone=head`) because `FName::ToString` preserves source casing. Substring-match `zone=head` and `headshot=1` together to disambiguate. Damage scaling for headshots is AFL-0411 (out of scope). |

### Beam scenarios (B01–B04)

| ID | Setup | Steps | Expected | Actual | Pass/Fail | Notes |
|---|---|---|---|---|---|---|
| **B01** | Variant A. Host equipped with Beam (via PawnData AbilitySet, OR via `AFL.Combat.GrantBeam` if equip is gated). `Dummy_Near` at point-blank with `H=100`. | 1. Hold Beam fire input continuously for **0.5 s**, then release. | `LogAFLCombat: Log: AFL_BEAM: Activate` fires once on press. Five tick lines `LogAFLCombat: Log: AFL_LOG: beam_tick damage=1.20 target=<actor>` fire over the hold (tick cadence is 100 ms; 0.5 s ≥ 5 ticks with a 50 ms scheduling grace — acceptable range **5..6** ticks). `LogAFLCombat: Log: AFL_BEAM: Release` fires on release. Dummy `H` decreased by `5 × 1.2 = 6.0` (or `6 × 1.2 = 7.2` if 6 ticks landed in the grace window). | | | Tick log is at `AFLAG_Laser_Beam.cpp:467`; format string uses `%.2f` ⇒ literal `damage=1.20`. Brief says `damage=1.2` — substring-match on `damage=1.2` to tolerate either. |
| **B02** | Continuation of B01 (Beam just released). | 1. Immediately attempt to re-activate Beam (press fire again ≤ 0.2 s after release). 2. Wait 3.1 s. 3. Attempt to re-activate Beam. | Attempt 1: blocked, no `AFL_BEAM: Activate` line, `Cooldown.Weapon.Beam` is present on the host ASC. Attempt 2 (after 3 s): activates normally and `AFL_BEAM: Activate` fires. | | | `GE_AFL_Cooldown_Beam` duration is **3.0 s** (`GE_AFL_Cooldown_Beam.cpp:19`). |
| **B03** | Variant A. Host aiming at `Dummy_Near` but sweeping the aim horizontally across the dummy's torso during the channel. Use a consistent sweep speed (eyeball ~30°/s). | 1. Hold Beam fire for ~1.0 s while sweeping aim across the dummy. 2. Repeat the run **50 times** in a row, resetting heat with `AFL.Combat.ResetHeat` between runs if needed to stay below overheat. | Each tick that intersects the dummy applies `1.20` damage. Across 50 trials the host log shows the same number of `AFL_LOG: beam_tick damage=1.20 target=<dummy>` lines on every trial within ±1 (tick-count drift only inside the 50 ms scheduling grace). **No** `ensure` / `check` callstacks. **No** `AFL_BEAM: server skipped non-hitscan target data` (it should never fire in a clean run). | | | Multi-client desync check belongs to R02. This row catches host-side flakes only. |
| **B04** | Variant A. Stand back so all targets are beyond `MaxRange = 8000 uu` (≈ 80 m); aim into open sky beyond range. | 1. Hold Beam fire for 0.5 s into out-of-range space. | `AFL_BEAM: Activate` fires. **No** damage applied. Brief expects `AFL_TELEMETRY: hitscan_reject reason=range` per tick; **the live build emits `AFL_TELEMETRY: hitscan_reject reason=beam_tick`** for rejected beam ticks (`AFLAG_Laser_Beam.cpp:40`, `:406`). Match on `hitscan_reject` (substring) and `source=` mentioning the Beam class. Same caveat as P03: a dedicated `reason=range` token is aspirational. | | | See §7. |

### Heat scenarios (H01–H04)

| ID | Setup | Steps | Expected | Actual | Pass/Fail | Notes |
|---|---|---|---|---|---|---|
| **H01** | Variant A. Host equipped with Beam. Heat = 0 (run `AFL.Combat.ResetHeat` to confirm). | 1. Hold Beam fire continuously without releasing. | After **~1.25 s** (12.5 ticks at 100 ms × 8 heat/tick — see `GE_AFL_Heat_BeamTick.h:16`), `Heat` reaches `MaxHeat = 100`. `State.Overheated` is granted on the host ASC. `LogAFLCombat: Log: AFL_LOG: heat_overheat` fires once (`AFLAttributeSet_Combat.cpp:118`). The Beam ability ends itself — `LogAFLCombat: Log: AFL_BEAM: overheat — ending channel` (`AFLAG_Laser_Beam.cpp:386`) and then `AFL_BEAM: Release`. Damage tick logs stop. | | | The brief says "~1.25s"; this is the as-coded value. The exact boundary depends on whether the tick that crosses MaxHeat counts — substring-match on `heat_overheat` and accept ±1 tick of timing variance. |
| **H02** | Continuation of H01 (host is now Overheated, Beam released). | 1. Stop firing. 2. Wait, watching the Output Log. 3. Once `LogAFLCombat: Log: AFL_LOG: heat_vented` fires (`AFLAttributeSet_Combat.cpp:149`), attempt to re-activate Beam. | After Heat decays below the venting gate (`MaxHeat * 0.3 = 30`), `State.Overheated` clears, `AFL_LOG: heat_vented` fires once, and `Event.Combat.HeatVentingComplete` broadcasts. From the moment overheat triggered to the moment Heat drops to 30 should be **~3.5 s** (decay = `HeatDecayRate × 0.1` per 100 ms = `2 heat/100 ms` = 20/s; 70 heat lost ≈ 3.5 s — but the brief's stated "0.5 s gate" is the *cooldown delay before decay starts*, which is **not modelled in the current GE chain**; see §7). Re-firing Beam after vent must produce a fresh `AFL_BEAM: Activate`. | | | Brief mentions "Heat decay after 0.5s gate." The current `GE_AFL_Heat_Decay` is granted at ability *activate* and runs continuously — there is no explicit 0.5 s post-overheat gate in the live build. Treat the gate as aspirational; the regression assertion is "Heat reaches <30 within ~3.5 s of overheat and beam re-fires." |
| **H03** | Variant A. Host equipped with Beam. Heat = 0. | 1. Run `AFL.Combat.ForceOverheat` in the host console. 2. Immediately attempt to activate Beam. | `LogAFLCombat: Display: AFLCombatCheats: OK ForceOverheat` fires (`AFLCombatCheats.cpp:343`). `Heat` is pinned to `MaxHeat = 100` and `State.Overheated` is granted (`GE_AFL_Heat_SetByCaller`). Beam activation attempt is **blocked** — no `AFL_BEAM: Activate` line. Run `AFL.Combat.ResetHeat` after the scenario to clean up (`AFLCombatCheats.cpp:359` ⇒ `AFLCombatCheats: OK ResetHeat`). | | | This row also protects the AFL-0215 cheat-matrix token — `AFLCombatCheats: OK ForceOverheat` is the substring `Tools/AFL_Yolo/verify.py` looks for. |
| **H04** | Variant A. Host equipped with **both** Pulse and Beam. Run `AFL.Combat.ForceOverheat` first to put the host in `State.Overheated`. `Dummy_Near` at point-blank. | 1. With `State.Overheated` still active, fire Pulse once at `Dummy_Near`. 2. `AFL.Combat.DumpCombatAttributes` to confirm damage. 3. `AFL.Combat.ResetHeat` to clean up. | Pulse fires normally — `AFL_PULSE: Activate` fires, 18 damage applies, hit-confirm log fires. Pulse is **heat-free** per master doc Sec. 5; Pulse's GA must not list `State.Overheated` in `ActivationBlockedTags` and must not consume Heat. **No** `AFL_LOG: heat_overheat` line should fire during the Pulse shot itself (Heat is already at MaxHeat from the cheat — confirm the value did not change). | | | This is the most important Heat regression gate: any future heat-cost refactor that puts heat on Pulse must trip this row before it ships. |

### Replication scenarios (R01–R03)

> All replication rows require **Variant B** (4-client listen-server,
> `NetEmulation.PktLag 40` ⇒ 80 ms RTT). Skip these rows on Variant A.

| ID | Setup | Steps | Expected | Actual | Pass/Fail | Notes |
|---|---|---|---|---|---|---|
| **R01** | Variant B. All 4 PIE windows arranged so the same `Dummy_Near` is visible from each. `H₀` captured on every client via `AFL.Combat.DumpCombatAttributes`. | 1. From each client in turn (Host, Client 1, Client 2, Client 3), fire one Pulse at `Dummy_Near`. 2. After all 4 shots, run `AFL.Combat.DumpCombatAttributes` from each window. | Every client's view of the dummy converges to `H₀ - 72` (4 shots × 18). Per-shot damage delta is `-18` exactly on every client's reading (zero frames of disagreement once net updates flush — convergence ≤ 200 ms after the shot). Each firing client emits **exactly one** `AFL_PULSE: Activate` line in **its own** log; remote clients emit **none** for shots they did not fire (Pulse activation is locally-predicted; remote sims see only the replicated EndAbility — see `AFL-0107_PulseSmokeTest.md` §7). | | | "Within 0 frames" in the brief means once net updates flush, all clients agree on the final number — not that the update is instantaneous. Record the convergence delay; flag > 500 ms. |
| **R02** | Variant B. Same dummy visible to all 4 clients. Reset heat on all clients (`AFL.Combat.ResetHeat`) so no run hits overheat. | 1. Host fires a single Beam channel for 0.5 s. 2. Repeat from each of the other 3 clients. | Each firing client's log shows 5 (±1) `AFL_LOG: beam_tick` lines. Per-tick application on the authoritative copy replicates to all observers; the dummy's `H` value as read from each client converges to the same total within **≤ 100 ms** of the firing client's last tick. **No** remote client emits `AFL_LOG: beam_tick` for shots it did not fire (same locally-predicted contract as Pulse). | | | This is the desync gate. If any non-firing client sees a different per-shot decrement from the firing client by > 100 ms, file a target-data RPC regression bug. |
| **R03** | Variant B. Host has Heat = 0 confirmed via `DumpCombatAttributes` from every client. | 1. Host holds Beam to overheat (~1.25 s as in H01). 2. From each non-owning client window, inspect the host pawn for `State.Overheated` (use `AbilitySystem.DebugTag State.Overheated` if available, or fire any Beam ability from the host's perspective and observe block). | Host emits `AFL_LOG: heat_overheat` once. `State.Overheated` is set on the host's ASC and replicates to all non-owning observers — each client sees `State.Overheated` on the owner pawn's ASC tag bag (`COND_OwnerOnly` does **not** apply to `MinimalReplicationTags` — confirm the tag is in the replicated minimal bag, not owner-only). Heat **attribute** itself replicates `COND_OwnerOnly` (`AFLAttributeSet_Combat.cpp:45`), so non-owners can't read the numeric value, but the tag must replicate. | | | If non-owners do **not** see `State.Overheated` on the host, the replication-condition on the tag is wrong — file a bug; the visual cue (overheat HUD/SFX on observers) depends on this. |

## 6. Expected log output (master matcher table)

> Substring matchers — not regex. The AFL-0215 cheat-matrix gate and
> the orchestrator's grep-based regression runner consume these exact
> tokens. If you rename one of these, update this table and the AFL-0215
> verify script in the same PR.

| Log category | Verbosity | Matcher (substring) | Source | Fires in row(s) | Required? |
|---|---|---|---|---|---|
| `LogAFLCombat` | `Log` | `AFL_PULSE: Activate` | `UAFLAG_Laser_Pulse::ActivateAbility` (`AFLAG_Laser_Pulse.cpp:80`) | P01, P02, P03, P04, P05, H04, R01 | yes |
| `LogAFLCombat` | `Verbose` | `hit_confirmed damage=` | `UAFLHitConfirmComponent::HandleHitConfirmed` (`AFLHitConfirmComponent.cpp:104`) | P01, P05, H04 | yes when a hit lands |
| `LogAFLCombat` | `Verbose` | `zone=head headshot=1` | same as above | P05 only | yes for P05 |
| `LogAFLCombat` | `Log` | `AFL_BEAM: Activate` | `UAFLAG_Laser_Beam::ActivateAbility` (`AFLAG_Laser_Beam.cpp:94`) | B01, B02 (after cooldown), B03, B04, R02 | yes |
| `LogAFLCombat` | `Log` | `AFL_BEAM: Release` | `UAFLAG_Laser_Beam::EndAbility` (`AFLAG_Laser_Beam.cpp:278`) | B01, B02, B03, B04, H01 (via overheat), R02 | yes |
| `LogAFLCombat` | `Log` | `AFL_LOG: beam_tick damage=` | `UAFLAG_Laser_Beam::OnBeamTick` (`AFLAG_Laser_Beam.cpp:467`) | B01, B03, R02 | yes per tick |
| `LogAFLCombat` | `Log` | `AFL_BEAM: overheat — ending channel` | `UAFLAG_Laser_Beam` overheat exit (`AFLAG_Laser_Beam.cpp:386`) | H01 only | yes for H01 |
| `LogAFLCombat` | `Log` | `AFL_LOG: heat_overheat` | `UAFLAttributeSet_Combat::PostGameplayEffectExecute` (`AFLAttributeSet_Combat.cpp:118`) | H01, R03 | yes |
| `LogAFLCombat` | `Log` | `AFL_LOG: heat_vented` | `UAFLAttributeSet_Combat::PostAttributeChange` (`AFLAttributeSet_Combat.cpp:149`) | H02 | yes for H02 |
| `LogAFLCombat` | `Log` | `AFL_TELEMETRY: hitscan_reject` | `Plugins/.../Telemetry/AFLCombatTelemetry.cpp:21` (substring catches both `reason=ang` and `reason=beam_tick`) | B04 (per tick), P03 (conditional — see §7) | conditional |
| `LogAFLCombat` | `Display` | `AFLCombatCheats: OK Damage` | `HandleAFLCombatDamage` (`AFLCombatCheats.cpp:222`) | (link to AFL-0107 smoke) | yes — protects cheat-matrix token |
| `LogAFLCombat` | `Display` | `AFLCombatCheats: OK GrantBeam` | `HandleAFLCombatGrantBeam` (`AFLCombatCheats.cpp:287`) | B01–B04 setup if equip-gated | conditional |
| `LogAFLCombat` | `Display` | `AFLCombatCheats: OK Heat` | `HandleAFLCombatHeat` (`AFLCombatCheats.cpp:325`) | H02 sanity (optional) | conditional |
| `LogAFLCombat` | `Display` | `AFLCombatCheats: OK ForceOverheat` | `HandleAFLCombatForceOverheat` (`AFLCombatCheats.cpp:343`) | H03, H04 setup | yes — protects cheat-matrix token |
| `LogAFLCombat` | `Display` | `AFLCombatCheats: OK ResetHeat` | `HandleAFLCombatResetHeat` (`AFLCombatCheats.cpp:359`) | H02/H03/H04 cleanup | yes — protects cheat-matrix token |
| `LogAFLCombat` | `Display` | `[Combat] H=` | `UAFLCombatCheats::DumpCombatAttributes` (`AFLCombatCheats.cpp:270`) | every row that asserts a damage value | yes when a row reads health |
| `LogAFLCombat` | `Verbose` | `AFL_PULSE: server skipped non-hitscan target data` | server validation skip-path (`AFLAG_Laser_Pulse.cpp:291`) | none in a clean run | **must NOT fire** in a clean run |

## 7. Conditional behaviours and known gaps between brief and live build

Documented here so future QA can disambiguate "expected absence" from
"bug" and so the gaps can be filed as follow-ups instead of being
silently re-discovered next sprint.

- **`AFL_PULSE: NoHit` (P04).** The brief specifies a `NoHit` log line
  for Pulse shots that hit no actor. The current `AFLAG_Laser_Pulse.cpp`
  has **no `NoHit` token** — there is no equivalent line emitted on a
  whiff. The closest existing assertion is the *absence* of
  `hit_confirmed`. **Follow-up:** file an orchestrator ticket to add
  `UE_LOG(LogAFLCombat, Log, TEXT("AFL_PULSE: NoHit"))` to the
  no-target branch of `UAFLAG_Laser_Pulse::ServerApplyTargetData`. Until
  then P04 passes on "activation log + no hit_confirmed + no damage."
- **`AFL_TELEMETRY: hitscan_reject reason=range` (P03, B04).** The
  brief expects a `reason=range` reject token. The current telemetry
  helper at `Plugins/.../Telemetry/AFLCombatTelemetry.cpp:21` emits
  `reason=<arbitrary string>`; the only reasons live in the codebase
  today are `reason=ang` (AFL-0213 ang anomaly path, log-only) and
  `reason=beam_tick` (`AFLAG_Laser_Beam.cpp:40`). There is **no
  `reason=range`** call site. **Follow-up:** file an orchestrator
  ticket to emit `AFL_TELEMETRY: hitscan_reject reason=range source=...`
  from the Pulse and Beam ability paths when the trace `MaxRange` cap
  is the reason the trace returned no hit. Until then, B04 substring-
  matches on `hitscan_reject` (any reason) and P03 currently has no
  positive matcher — it asserts only "no damage."
- **`AFL_TELEMETRY: hitscan_reject reason=ang` (Pulse).** This is
  log-only today (AFL-0213 will turn it into a real reject). It fires
  only when `AimAngularVelocityDegPerSec` in inbound target data
  exceeds `MaxAimAngularVelocityDegPerSec`. The client always sends
  `0.0` today, so the matcher should *not* fire in a baseline pass.
  To deliberately provoke it for matcher validation, temporarily set
  `MaxAimAngularVelocityDegPerSec = -1.0f` on the Pulse CDO, fire once,
  then revert. Damage **must still apply** (log-only reject — see
  `AFLAG_Laser_Pulse.cpp:299`).
- **`AFL_PULSE: server skipped non-hitscan target data`.** Should
  **never** fire in a baseline pass. Its presence signals that another
  ability is feeding foreign target-data into the Pulse delegate, or
  that the target-data struct registration drifted. Treat as a fail.
- **Hit-confirm log verbosity (P01, P05, H04).** The log fires at
  `Verbose`, not `Log`, and the literal token is `hit_confirmed
  damage=...` — there is **no** `AFL_LOG:` prefix on this line
  (`AFLHitConfirmComponent.cpp:104`). The brief writes
  `AFL_LOG: hit_confirmed`; substring-match on `hit_confirmed damage=`
  to be robust to the prefix difference.
- **Headshot zone casing (P05).** The brief writes "zone=Head" but
  `FName::ToString` preserves the source bone name's casing — Lyra's
  mannequin uses lower-case `head` / `neck_01`. Substring-match
  `zone=head` AND `headshot=1` together.
- **Beam-tick log format (B01, B03, R02).** The format string is
  `%.2f` ⇒ literal `damage=1.20`. Brief writes `damage=1.2`. Match on
  `damage=1.2` (substring) to tolerate either.
- **Heat decay "0.5 s gate" (H02).** The brief specifies a 0.5 s gate
  before Heat begins decaying after overheat. The current GE chain
  (`GE_AFL_Heat_Decay`) is granted at Beam activation and runs
  continuously; there is **no explicit 0.5 s post-overheat gate** in
  the live build. The regression assertion has been written as
  "Heat reaches < 30 within ~3.5 s of overheat" — that's what the
  current decay rate produces (`HeatDecayRate = 20/s`, 70 heat ⇒
  3.5 s). **Follow-up:** if the 0.5 s gate is required, file a ticket
  to wire a duration-gated `GE_AFL_Heat_DecayPause` between overheat
  and decay.
- **Pulse MaxRange (P03, B04).** The brief uses ">50 m" prose; the
  actual `MaxRange` constant is **8000 uu (= 80 m)** on both Pulse
  (`AFLAG_Laser_Pulse.h:67`) and Beam (`AFLAG_Laser_Beam.h:118`). Use
  80 m as the practical out-of-range distance.
- **Variant B activation log scope.** On a multi-client listen-server,
  the activation log fires **once per fire** on the firing client *and*
  on the host (the listen-server also runs the local-prediction path
  because `IsLocallyControlled` is true on its own pawn). Remote sim
  proxies do **not** emit `AFL_PULSE: Activate` or `AFL_BEAM: Activate`
  for shots they did not fire — they see only the replicated
  `EndAbility`. R01/R02 explicitly assert this.

## 8. Pass / fail criteria

The matrix **passes** for a given variant only if **every** row's
expected observation in §5 matched AND every Required matcher in §6
fired. Any single row failure fails the variant.

- [ ] All required §6 matchers observed for every row that requires them.
- [ ] Every "Actual" cell in §5 is filled and matches the "Expected" cell.
- [ ] Every "Pass/Fail" cell is filled with `PASS` or `FAIL`.
- [ ] No new ensure/check failures in the Output Log.
- [ ] No new warnings in `LogAFLCombat`, `LogAFLCore`, `LogAFLMovement`.
- [ ] PIE session closes cleanly (no shutdown crash, no leaked actors).
- [ ] All cheat-matrix `AFLCombatCheats: OK <Name>` substrings used
      across §5 are observed verbatim (protects the AFL-0215 verify
      contract).
- [ ] Conditional matchers in §7 either fired in the documented
      provoke setup, or are explicitly noted as "not exercised this
      pass" in the row's Notes column — they must not be silently
      skipped.

Any single failure fails the matrix; reopen the regression ticket and
attach the §10 artifacts.

## 9. Variant matrix

| Variant | PIE mode | Clients | Net emulation | Required? | Notes |
|---|---|---|---|---|---|
| A | ListenServer | 1 host | none | required | Baseline. P01–P05, B01–B04, H01–H04 all run here. R01–R03 are skipped on A. |
| B | ListenServer | 1 host + 3 clients | `NetEmulation.PktLag 40` (80 ms RTT) | required | Master doc §7 L3 target. R01–R03 run here. P01/B01/H01 should also be re-run as a sanity sweep on B — record any divergence in row Notes. |
| C | DedicatedServer | 4 clients | `NetEmulation.PktLag 40` | optional | Only if dedi behaviour materially differs from listen-server (e.g. a future AFL-04xx ticket changes the authoritative path). Not required for AFL-0210 sign-off. |

### Variant B procedure

1. After PIE starts with 4 players, focus the **host** window and
   run `NetEmulation.PktLag 40`.
2. For P01/B01/H01 (sanity sweep on B): each non-host client fires
   once at `Dummy_Near`; record convergence delay (host attribute
   value visible on remote clients) per row.
3. For R01–R03: follow the row-specific Steps column in §5.
4. Across all Variant B rows, per-client validation: run
   `AFL.Combat.DumpCombatAttributes` targeting the dummy after each
   shot. Every client window should converge to the same `H` value
   once net updates flush (≤ 200 ms after the shot — record the
   convergence delay; flag anything > 500 ms).

### What "fail" looks like on Variant B

- Pulse damage applies on the firing client but not on the host →
  target-data RPC regression
  (`AFLAG_Laser_Pulse.cpp:240` `CallServerSetReplicatedTargetData`).
- Damage applies on the host but not on non-firing clients →
  attribute replication regression
  (`UAFLAttributeSet_Combat` rep notifies).
- Host emits `AFL_PULSE: Activate` for shots fired by remote clients →
  server is incorrectly running the local-prediction path; check
  `IsLocallyControlled` gating in `AFLAG_Laser_Pulse.cpp:93`.
- Beam tick counts diverge between firing client and host by more than
  the 50 ms scheduling grace → beam scheduling / RPC timing
  regression.
- `State.Overheated` set on host but not visible to non-owning
  clients → replication condition on the overheat tag is wrong
  (must be in the minimal replicated tag bag, not `COND_OwnerOnly`).

## 10. Artifacts to capture

- [ ] Per-variant Output Log saved as
      `AFL-0210_VariantA_outputlog.txt` and
      `AFL-0210_VariantB_outputlog.txt`, attached to the run's tracker
      entry.
- [ ] Screenshot of the §5 table with all Actual / Pass-Fail cells
      filled, one screenshot per variant.
- [ ] For Variant B: one screenshot per client window showing the
      converged dummy `H` value after R01 (Pulse) and R02 (Beam) runs.
- [ ] For H01: a screenshot of the Output Log showing the
      `heat_overheat` line adjacent to the `AFL_BEAM: overheat — ending
      channel` line.
- [ ] If any check/ensure fired: the full callstack and the PIE
      session's `.log` from `Saved/Logs/`.
- [ ] Any follow-up tickets filed against the §7 gaps — link them
      here so the next pass picks them up.

## 11. Known-good baseline

| Last pass | Commit | Tester | Notes |
|---|---|---|---|
| — | — | — | Plan freshly authored on `yolo/afl-0210-pulse-beam-heat-regression`; first execution will populate this row. |

## 12. Sign-off

| Role | Name | Date | Signature / PR link |
|---|---|---|---|
| QA Lead | | | |
| Sprint 2 Owner | | | |

## 13. Revision history

| Date | Author | Change |
|---|---|---|
| 2026-05-22 | AFL-0210 (yolo) | Initial authoring. Extends the AFL-0111 Pulse smoke test (`AFL-0107_PulseSmokeTest.md`) into a full P/B/H/R regression matrix. §7 documents gaps between the brief (`NoHit`, `reason=range`, 0.5 s decay gate) and the live build; follow-ups should be filed before the matrix is run for sign-off. |
