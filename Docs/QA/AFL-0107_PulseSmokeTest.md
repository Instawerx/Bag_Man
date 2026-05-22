# AFL-0107 — Pulse Smoke Test

> Sprint 1 end-gate smoke test for the Pulse weapon. Validates that the
> AFL-0106 client-authoritative hitscan ability (`UAFLAG_Laser_Pulse`)
> activates on input, deals 18 damage via `UGE_AFL_Damage_Pulse` routed
> through `UAFLDamageExecCalc`, and emits the expected diagnostic log
> lines on both the listen-server host and remote clients with simulated
> 80 ms RTT.
>
> Authored from `Docs/QA/AFL_TEST_PLAN_TEMPLATE.md`.

---

## 1. Identification

| Field | Value |
| --- | --- |
| Plan ID | `AFL-0107_PulseSmokeTest` |
| Linked ticket | AFL-0107 (Sprint 1 PIE smoke test sign-off) |
| Authoring ticket | AFL-0111 (QA test plan template + Pulse smoke test) |
| Sprint | S1 |
| Discipline owner | QA |
| Engine | UE 5.6 |
| Branch under test | `yolo/afl-0111-qa-plan-pulse` (template); future runs use whatever branch carries the change being smoke-tested. |
| Build configuration | LyraEditor Win64 Development (PIE) |

## 2. Scope and intent

The Pulse weapon is the first AFL ability that exercises the full
client-authoritative hitscan pipeline (master doc §7 L1–L3). This plan
defends against regressions in three independently-failing layers:
input → ability activation → damage application. It is the gate that
closes Sprint 1.

- **In scope:**
  - Pulse ability activation via `InputTag.Weapon.Fire` (bound by
    AFL-0107).
  - Local-client trace → `ServerSetReplicatedTargetData` → server
    `ServerApplyTargetData` → `GE_AFL_Damage_Pulse` application.
  - Health attribute decrement on the hit target by exactly 18.
  - Diagnostic log emission for the activation + the (current,
    log-only) telemetry reject path.
  - Behaviour on a 4-client listen-server with 80 ms RTT.

- **Out of scope:**
  - Lag compensation (AFL-0211 — `UAFLLagCompensationWorldSubsystem`
    is not landed yet; tests here use stationary target dummies so a
    server snapshot equals the client view).
  - Real angular-velocity reject behaviour (AFL-0213 turns the log
    line into a drop+counter; today it is log-only).
  - Cosmetic FX / audio (AFL-0201..0205).
  - Damage multipliers (AFL-0411 headshot matrix).
  - Heat / cost (AFL-0207).

- **Defended risks:**
  - Pulse activates but applies no damage (input/ability/effect path
    silently disconnects between sprints).
  - Damage applies on the host but not on remote clients
    (target-data RPC regression).
  - `AFLCombatCheats` extension fails to register, so future cheat-
    gated regression matrices give false greens.

## 3. Preconditions

- [ ] Working tree clean or only-intended-changes for the feature
      under test.
- [ ] `Build.bat LyraEditor Win64 Development C:\Dev\Bag_Man\Bag_Man.uproject -Plugin=AFLCombat.uplugin` succeeds with no warnings (warnings-as-errors).
- [ ] AFL-0215 lint clean: `python Tools/AFL_Yolo/lint_afl_0215.py`.
- [ ] AFL-0107 merged (`IA_AFL_Fire` + `IC_AFL_Default` mapping live).
- [ ] AFL-0106 merged (`UAFLAG_Laser_Pulse` + `UGE_AFL_Damage_Pulse` +
      `FAFLAbilityTargetData_Hitscan` live).
- [ ] `UAFLCombatCheats` cheat extension auto-registers at PIE start —
      look for the `AFL.Combat.Damage` and `AFL.Combat.EnergyGain`
      console commands existing (use `~ AFL.Combat.` tab-complete).
- [ ] No prior PIE session running.

## 4. Setup

1. Open `C:\Dev\Bag_Man\Bag_Man.uproject` in UnrealEditor (UE 5.6).
   Wait for shader compilation to settle (status bar idle).
2. Open the Output Log (`Window → Output Log`). Add a category
   filter that includes at least: `LogAFLCombat`, `LogAFLCore`,
   `LogCheatManager`. Leave verbosity at Verbose so `Display` and
   `Log` lines both show.
3. Load PIE map: `L_ShooterGym` (per memory
   `project_afl0110_redirect_shootergym` — L_ShooterGym is Sprint 1's
   testbed; `B_LyraExperience_AFL_Arena_Test` is the experience).
4. Set PIE mode for the chosen variant:
   - Variant A (single-host): PIE `Number of Players = 1`, `Net Mode = Play As Listen Server`.
   - Variant B (host + 3 clients): PIE `Number of Players = 4`, `Net Mode = Play As Listen Server`.
5. Place (or confirm placed) at least one stationary target dummy
   actor with an ability system component within easy aim distance
   of the host spawn point. Any Lyra `B_Hero_ShooterMannequin`-style
   actor that owns a `UAbilitySystemComponent` is acceptable for the
   smoke pass; a dedicated `BP_AFL_TargetDummy` is preferable when it
   lands.
6. Confirm cheat manager wiring in the Output Log:
   `LogCheatManager:` line referencing `UAFLCombatCheats` during PIE
   start. If absent, **stop** — preconditions failed.
7. For Variant B only, after PIE has spawned all clients, focus the
   host window and run in the console:
   `NetEmulation.PktLag 40` — this adds 40 ms each way ⇒ 80 ms RTT
   (matches master doc §7 L3 target).

## 5. Test steps

> Steps assume Variant A unless noted; rerun the same steps for
> Variant B (§9), recording per-client matchers.

| # | Action | Expected immediate observation |
|---|---|---|
| 1 | After PIE start, focus the host viewport. | Pulse is equipped by default via the AbilitySet on `PawnData` (no manual equip step needed — AFL-0214 will formalise the equip flow; for the smoke pass the AbilitySet grant is sufficient). |
| 2 | Aim crosshair at the target dummy. Confirm a clear line-of-sight (no intervening collision). | No diagnostic log change yet. |
| 3 | Console: `AFL.Combat.DumpCombatAttributes` on the target dummy's controller, OR aim at the host pawn and run the same cheat to capture **starting health**. | Output Log: `LogAFLCombat: Display: [Combat] H=<x>/<x> S=… …` — record the starting `H=` value as `H₀`. |
| 4 | Press the bound Pulse fire input (`InputTag.Weapon.Fire`, default LMB per `IA_AFL_Fire`). | Output Log: `LogAFLCombat: Log: AFL_PULSE: Activate`. |
| 5 | Re-run `AFL.Combat.DumpCombatAttributes` for the target dummy. | Output Log: a second `[Combat]` line where the new `H` equals `H₀ - 18.0` (subject to clamping at `MaxHealth` ceiling and 0 floor). |
| 6 | As a cheat-matrix substitute path, run `AFL.Combat.Damage 18` on the host. | Output Log: `LogAFLCombat: Display: AFLCombatCheats: OK Damage (Amount=18.0)`. Confirms `GE_AFL_Damage_Pulse` is the same effect that the cheat path applies — the live ability path and the cheat path share `UGE_AFL_Damage_Pulse`, so this is a sanity link, not a duplicate of step 5. |
| 7 | Stop PIE. | Editor returns cleanly; no shutdown ensure/check. |

> **Note on `AFL.Combat.ShowHealth`:** the AFL-0111 brief references a
> `AFL.Combat.ShowHealth` cheat. The shipping cheat that prints health
> (and the rest of the combat attribute set) today is
> `AFL.Combat.DumpCombatAttributes` on `UAFLCombatCheats` (see
> `Plugins/GameFeatures/AFLCombat/Source/AFLCombat/Private/Test/AFLCombatCheats.cpp:258`).
> If a dedicated `ShowHealth` cheat is later authored, swap the
> command name in step 3/5 — the matcher in §6 should be updated to
> match its log format.

## 6. Expected log output

| Log category | Verbosity | Matcher (substring) | Source | Required? |
|---|---|---|---|---|
| `LogAFLCombat` | `Log` | `AFL_PULSE: Activate` | `UAFLAG_Laser_Pulse::ActivateAbility` (`Plugins/GameFeatures/AFLCombat/Source/AFLCombat/Private/Abilities/AFLAG_Laser_Pulse.cpp:79`) | yes — every shot |
| `LogAFLCombat` | `Display` | `[Combat] H=` | `UAFLCombatCheats::DumpCombatAttributes` (`…/AFLCombatCheats.cpp:270`) | yes — pre and post |
| `LogAFLCombat` | `Display` | `AFLCombatCheats: OK Damage` | `HandleAFLCombatDamage` (`…/AFLCombatCheats.cpp:213`) | yes — step 6 |
| `LogAFLCombat` | `Log` | `AFL_TELEMETRY: hitscan_reject` | `UAFLAG_Laser_Pulse::ServerApplyTargetData` (`…/AFLAG_Laser_Pulse.cpp:305`) | conditional — see §7 |
| `LogAFLCombat` | `Verbose` | `AFL_PULSE: server skipped non-hitscan target data` | server validation skip-path (`…/AFLAG_Laser_Pulse.cpp:291`) | conditional — must **not** fire in a clean run |

## 7. Conditional behaviours

- `AFL_TELEMETRY: hitscan_reject` fires **only** when
  `AimAngularVelocityDegPerSec` in the inbound target data exceeds
  the ability's `MaxAimAngularVelocityDegPerSec`. Today `AFL-0213`'s
  measurement is unwired (the client always sends `0.0`), so the
  matcher should *not* fire in a baseline pass. **To deliberately
  provoke it for matcher validation**, temporarily set
  `MaxAimAngularVelocityDegPerSec = -1.0f` on the Pulse CDO (or any
  negative value), fire once, then revert. The matcher must fire
  exactly once per shot and damage **must still apply** (log-only
  reject — see `AFLAG_Laser_Pulse.cpp:299`'s comment).
- `AFL_PULSE: server skipped non-hitscan target data` should never
  fire in a baseline pass — its presence signals that some other
  ability is feeding foreign target-data into the Pulse delegate, or
  that the target-data struct registration drifted. Treat as a fail.
- On Variant B (multi-client), the activation log fires **once per
  fire** on both the firing client *and* the host (the listen-server
  also runs the local-prediction path because `IsLocallyControlled`
  is true on its own pawn). Remote sim proxies do **not** emit
  `AFL_PULSE: Activate` for shots they did not fire — they only see
  the replicated `EndAbility`.

## 8. Pass / fail criteria

- [ ] §6 required matchers all observed in the correct order.
- [ ] Step 5 confirms health decreased by **exactly 18** from `H₀`
      (modulo clamping at `MaxHealth`/0 — if `H₀` < 18, the target's
      H goes to 0, not negative).
- [ ] Step 6 confirms the `AFLCombatCheats: OK Damage` token appears
      (same token the cheat matrix in `Tools/AFL_Yolo/verify.py`
      consumes — protects the regression gate's contract).
- [ ] No new ensure/check failures or callstacks.
- [ ] No new warnings in `LogAFLCombat`, `LogAFLCore`,
      `LogAFLMovement`.
- [ ] PIE shuts down cleanly.

Any single failure fails the plan; reopen AFL-0107 (or the regression
ticket that triggered the run) and attach the artifacts from §10.

## 9. Multi-client / replication variants

| Variant | PIE mode | Clients | Net emulation | Notes |
|---|---|---|---|---|
| A | ListenServer | 1 host | none | Baseline. Required. |
| B | ListenServer | 1 host + 3 clients | `NetEmulation.PktLag 40` (80 ms RTT) | Required for the AFL-0111 brief. Per master doc §7 L3 target. |

### Variant B procedure

1. After PIE start with 4 players, focus the **host** window and run
   `NetEmulation.PktLag 40` in the console.
2. From each client (Client 1, Client 2, Client 3 — host is its own
   "client" too): aim at the same target dummy and fire once.
3. Per-client expected behaviour:
   - Firing client: `AFL_PULSE: Activate` in the **client's** PIE
     window log (the activation runs locally-predicted).
   - Host: also receives the replicated target data and runs
     `ServerApplyTargetData`. Damage applies on the authoritative
     copy; replication delivers the `Health` attribute update back
     to all clients on the next net update (≈ 80 ms after the shot
     under PktLag=40).
   - Non-firing clients: see the health drop on the target dummy,
     but no `AFL_PULSE: Activate` in their own log.
4. Per-client validation: run `AFL.Combat.DumpCombatAttributes`
   targeting the dummy after each shot. Every client window should
   converge to the same `H` value once net updates flush
   (≤ 200 ms after the shot — record the convergence delay; flag
   anything > 500 ms).
5. Total damage after 4 shots (one per client): `H₀ - 72`, clamped
   at 0. If `H₀ < 72`, the target dies during the run — that's
   acceptable; record the shot index at which `H` hit 0.

### What "fail" looks like on Variant B

- Damage applies on the firing client's view of the dummy but not
  on the host's view → target-data RPC regression (re-check the
  `CallServerSetReplicatedTargetData` path in
  `AFLAG_Laser_Pulse.cpp:240`).
- Damage applies on the host but not on non-firing clients →
  attribute replication regression (re-check `UAFLAttributeSet_Combat`
  rep notifies).
- Host emits `AFL_PULSE: Activate` for shots fired by remote
  clients → server is incorrectly running the local-prediction
  path; check `IsLocallyControlled` gating in
  `AFLAG_Laser_Pulse.cpp:93`.

## 10. Artifacts to capture

- [ ] Per-variant Output Log saved as
      `AFL-0107_VariantA_outputlog.txt` and
      `AFL-0107_VariantB_outputlog.txt`, attached to the run's
      tracker entry.
- [ ] Screenshot showing the `[Combat] H=` before/after pair from
      step 3 / step 5.
- [ ] For Variant B: one screenshot per client window showing the
      converged `H` value after the 4-shot sequence.
- [ ] If any check / ensure fired: the full callstack and the PIE
      session's `.log` from `Saved/Logs/`.

## 11. Known-good baseline

| Last pass | Commit | Tester | Notes |
|---|---|---|---|
| — | — | — | Plan freshly authored on `yolo/afl-0111-qa-plan-pulse`; first execution will populate this row. |

## 12. Sign-off

| Role | Name | Date | Signature / PR link |
|---|---|---|---|
| QA Lead | | | |
| Sprint 1 Owner | | | |

## 13. Revision history

| Date | Author | Change |
|---|---|---|
| 2026-05-21 | AFL-0111 (yolo) | Initial authoring from `AFL_TEST_PLAN_TEMPLATE.md`. Notes the `AFL.Combat.ShowHealth` brief reference and the substitute `AFL.Combat.DumpCombatAttributes`. |
