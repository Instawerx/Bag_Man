# AFL QA Test Plan — Template

> Reusable template for AFL Bag_Man manual QA test plans. Copy this file to
> `Docs/QA/AFL-XXXX_<ShortName>.md`, then fill in every section. Delete the
> instructional block-quotes (like this one) once the plan is authored.
>
> Owning discipline: QA Lead. See `Docs/BAG_MAN_MASTER_BUILD_v2.0.md`
> §17 for the broader QA process; this template is the per-ticket artifact
> referenced from the sprint end-gates.

---

## 1. Identification

| Field | Value |
| --- | --- |
| Plan ID | `AFL-XXXX_<ShortName>` |
| Linked ticket | AFL-XXXX |
| Sprint | S<n> |
| Discipline owner | QA |
| Author | <name> |
| Created | YYYY-MM-DD |
| Last updated | YYYY-MM-DD |
| Engine | UE 5.6 |
| Branch under test | `<branch>` |
| Build configuration | LyraEditor Win64 Development (PIE) unless stated |

## 2. Scope and intent

> One short paragraph: what feature is exercised, what risk this plan is
> defending against, and what is explicitly **out of scope**. Out-of-scope
> matters as much as in-scope — it tells the next QA pass what *not* to retest.

- **In scope:**
- **Out of scope:**
- **Defended risks (why this plan exists):**

## 3. Preconditions

> List every state the world must be in before the plan can run. The reader
> should be able to reproduce the precondition list without reading code.

- [ ] Working tree on `<branch>`; uncommitted changes documented or absent.
- [ ] `Build.bat LyraEditor Win64 Development C:\Dev\Bag_Man\Bag_Man.uproject -Plugin=<plugin>.uplugin` succeeded for every plugin touched by the feature.
- [ ] AFL-0215 lint clean (`python Tools/AFL_Yolo/lint_afl_0215.py`).
- [ ] Required upstream tickets merged: AFL-XXXX, AFL-YYYY.
- [ ] Required cheat manager extensions present (`UAFLCombatCheats`, etc.).
- [ ] No prior PIE session bleeding over (close-all-PIE before starting).

## 4. Setup

> The minimum sequence to get from a clean editor open to "ready to press
> Play." Number the steps; another QA tester should be able to drive this
> without asking questions.

1. Open `C:\Dev\Bag_Man\Bag_Man.uproject` in UnrealEditor (UE 5.6).
2. Confirm Output Log filter category includes `LogAFLCombat` (and any
   other AFL log categories the feature uses).
3. Load PIE map: `<map path, e.g. /Game/System/DefaultMap or L_ShooterGym>`.
4. Set PIE mode: `<Standalone | ListenServer | Client(N) | DedicatedServer>`.
5. Cheat manager: confirm `AFLCombatCheats` extension is registered
   (Output Log shows `LogCheatManager: ... AFLCombatCheats` at PIE start;
   if absent, the test fails preconditions).
6. Net emulation (only when the plan requires it):
   `~ NetEmulation.PktLag=40` for 80 ms RTT (40 ms each way).

## 5. Test steps

> Numbered, atomic steps. Each step has a single observable result. If a
> step has multiple sub-observations (e.g. "fire and check both damage and
> log"), split it. The orchestrator's regression runner reads this list.

| # | Action | Expected immediate observation |
|---|---|---|
| 1 | <action> | <observable> |
| 2 | <action> | <observable> |
| 3 | <action> | <observable> |

## 6. Expected log output

> Exact `UE_LOG` lines (or substrings, when timestamps/prediction keys
> vary) the plan expects to see. Each line lists the category, verbosity,
> and the matcher. Use substring matchers (not regex) unless regex is
> required — substring is what the orchestrator's grep-based gates use.

| Log category | Verbosity | Matcher | Source | Required? |
|---|---|---|---|---|
| `LogAFLCombat` | `Log` | `AFL_PULSE: Activate` | `UAFLAG_Laser_Pulse::ActivateAbility` | yes |
| `LogAFLCombat` | `Log` | `AFL_TELEMETRY: hitscan_reject` | `UAFLAG_Laser_Pulse::ServerApplyTargetData` | conditional — see §7 |
| `LogAFLCombat` | `Display` | `AFLCombatCheats: OK <CheatName>` | `Tools/AFL_Yolo/verify.py` cheat matrix gate | yes (when verify mode = compile+cheat-matrix) |

## 7. Conditional behaviours

> Some log lines or behaviours only fire under specific conditions. List
> them here so future QA can disambiguate "expected absence" from "bug."

- `AFL_TELEMETRY: hitscan_reject` fires **only** when the inbound
  hitscan target data carries `AimAngularVelocityDegPerSec` above the
  ability's `MaxAimAngularVelocityDegPerSec` budget. AFL-0213 will turn
  this into a real reject (drops the shot, increments counter); until
  then it is log-only and damage still applies (see comment in
  `AFLAG_Laser_Pulse.cpp:299`).

## 8. Pass / fail criteria

> Boolean criteria. The plan **passes** only if every "Required" matcher
> in §6 fires AND every step's observation in §5 matched. Any single
> failure fails the plan.

- [ ] All required log matchers in §6 observed.
- [ ] All step observations in §5 matched.
- [ ] No new ensure/check failures in the Output Log.
- [ ] No new warnings in `LogAFLCombat`, `LogAFLMovement`, `LogAFLCore`,
      or other AFL categories the feature touches.
- [ ] PIE session closes cleanly (no shutdown crash, no leaked actors).

## 9. Multi-client / replication variants (delete if irrelevant)

> Replication-sensitive features need their own per-topology pass.
> Document the variant matrix; reuse §5/§6 steps where possible.

| Variant | PIE mode | Clients | Net emulation | Notes |
|---|---|---|---|---|
| A | ListenServer | 1 host | none | Baseline. |
| B | ListenServer | 1 host + 3 clients | `NetEmulation.PktLag=40` (80 ms RTT) | Per master doc §7 L3 target. |
| C | DedicatedServer | 4 clients | `NetEmulation.PktLag=40` | Optional; only if dedi behaviour differs. |

For each variant: rerun §5 steps, record the §6 matchers per-client, and
note any divergence (e.g. host sees damage immediately, remote client
sees it after one RTT).

## 10. Artifacts to capture

- [ ] Full PIE Output Log (`File → Output Log → Save…`) → attach to the
      ticket with filename `AFL-XXXX_<variant>_outputlog.txt`.
- [ ] Screenshot of the §6 matcher hits in the Output Log filter.
- [ ] If a failure: short clip + `~ stat unit` capture for frame-time
      and `~ NetProfile` if replication is involved.

## 11. Known-good baseline

> The commit / PR where this plan last passed end-to-end. Update on every
> successful run so regressions can git-bisect cheaply.

- Last pass: `<commit hash>` on `<date>` by `<tester>`.

## 12. Sign-off

| Role | Name | Date | Signature / PR link |
|---|---|---|---|
| QA Lead | | | |
| Sprint Owner | | | |

## 13. Revision history

| Date | Author | Change |
|---|---|---|
| YYYY-MM-DD | <name> | Initial authoring. |
