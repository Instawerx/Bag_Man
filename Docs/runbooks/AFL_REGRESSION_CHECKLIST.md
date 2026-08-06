# AFL Regression Checklist — Living Index

> Top-level index of every per-ticket regression test plan under
> `Docs/runbooks/`. Each entry points at a single regression matrix; the
> matrix itself owns scope, setup, scenarios, matchers, pass/fail, and
> sign-off (see `AFL_TEST_PLAN_TEMPLATE.md`).
>
> When you author a new regression matrix, add a one-line bullet here
> in chronological order under its sprint. Keep this file short — it
> is an index, not a doc.

---

## How to use this index

1. Find the sprint you're closing out.
2. Run every matrix listed under that sprint in the order they appear.
3. If a row fails, reopen the matrix's linked ticket and attach the
   artifacts that matrix's §10 lists.
4. After a successful pass, update the matrix's §11 "Known-good
   baseline" row with the commit hash and date — that's what future
   git-bisect runs lean on.

New plans should be authored from
[`AFL_TEST_PLAN_TEMPLATE.md`](AFL_TEST_PLAN_TEMPLATE.md). Copy the
template, fill in every section, then add a bullet here.

---

## Sprint 1 — Pulse stack

- [AFL-0107 — Pulse smoke test](AFL-0107_PulseSmokeTest.md) — Sprint 1
  end-gate smoke: Pulse activation → `UGE_AFL_Damage_Pulse` via
  `UAFLDamageExecCalc` → 18 damage applied, on 4-client listen-server
  with 80 ms RTT. Authored under AFL-0111.

## Sprint 2 — Beam channel, Heat, and combined-surface regression

- [AFL-0210 — Pulse + Beam + Heat regression matrix](AFL-0210_PulseBeamHeat_Regression.md)
  — Sprint 2 closeout. Pulse scenarios P01–P05, Beam B01–B04, Heat
  H01–H04, replication R01–R03. Defends the shared-AttributeSet /
  shared-tag-bag interaction surface, and protects the AFL-0215 cheat-
  matrix tokens (`AFLCombatCheats: OK <Name>`). Documents known gaps
  between the brief and the live build (see its §7) — file follow-ups
  before running for sign-off.

## Future sprints

Add new matrices here as they land. Suggested next entries (not yet
authored):

- AFL-0411 — Dismemberment / headshot damage multiplier regression
  (Sprint 4 surface, called out as out-of-scope in AFL-0210 §2).
- AFL-0211 — Lag compensation regression (once
  `UAFLLagCompensationWorldSubsystem` lands).
- AFL-0213 — Angular-velocity hitscan reject regression (when the
  `reason=ang` path moves from log-only to real drop+counter).

---

## Related documents

- `AFL_TEST_PLAN_TEMPLATE.md` — the per-matrix template; copy this
  when authoring a new regression plan.
- `Docs/BAG_MAN_MASTER_BUILD_v2.0.md` §17 — broader QA process; the
  matrices listed above are the per-ticket artifacts referenced from
  the sprint end-gates.
- `Tools/AFL_Yolo/verify.py` — the orchestrator's cheat-matrix gate;
  every `AFLCombatCheats: OK <Name>` substring listed in a matrix's
  §6 must remain stable, or this gate goes green for the wrong
  reason.
