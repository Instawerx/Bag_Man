# Bag_Man handoff — 2026-05-22 EOD (Sprint 2 ability/infra done, VFX/Audio next)

Picking this up tomorrow in a fresh Claude Code session, the UE AIK panel, or driving UE directly? Read this first.

## Where we are right now

- **18 of 18 active queue tasks merged.** Zero errors. Zero pending. The orchestrator queue is fully exhausted.
- **Origin/main HEAD:** `8c6a8838 docs(SSOT): tracker v2.5 → v2.6 reconcile — active queue exhausted`
- **Tracker:** `BAG_MAN_LIVE_TRACKER_v2_2.html` internal version v2.6, reflects all 18 merges + AFL-0110 closure + 4 open AFL-0210 followups
- **Sprint 2 status:** ability + infrastructure layer is **DONE**. Pulse → Beam → Heat → Hit-Confirm → Damage ExecCalc → AttributeSet → Lag Comp → Telemetry → AbilitySet → 4-rail AFL-0215 lint all shipped and in main
- **Sprint 2 remaining work:** VFX (4 tasks), Audio (1 task), Design tuning (1 task), Tech Art (2 tasks) — 8 tasks total, mostly UE-side authoring

## What shipped this session (2026-05-21 → 22)

9 PRs through the orchestrator + 4 orchestrator hardening commits + AFL-0110 closure + tracker reconciles + AFL-0215 standalone + asset-naming prefix fix + Sprint 2 queue authoring.

**The 9 PRs:**

| PR | Task | Discipline | Estimate | E2E |
|---|---|---|---|---|
| #9 | AFL-0109 asset naming | devops | M | ~28 min |
| #10 | AFL-0111 QA template | qa | S | ~7 min |
| #11 | AFL-0211 lag comp | eng | XL | ~12 min (3rd attempt) |
| #12 | AFL-0213 telemetry | eng | M | **5m 36s** |
| #13 | AFL-0218 skill registry | devops | S | ~12 min |
| #14 | AFL-0204 hit confirm | eng | M | ~12 min |
| #15 | AFL-0206 beam ability | eng | L | ~16 min |
| #16 | AFL-0207 heat system | eng | M | 23m 14s (heaviest) |
| #17 | AFL-0210 regression matrix | qa | M | 7m 45s |

**Orchestrator hardening (4 commits):**
- `851a76d8 fix(verify): retry once on UBT mutex collision` — 30s sleep + single retry, catches transient Build.bat leftovers
- `6d6f16f3 fix(asset_naming): allow Lyra-canonical prefixes` — added `B_/L_/LAS_/IMC_/InputData_` to the allowed-prefix list
- `4551ba35 fix(orchestrator): pre-flight UBT mutex check halts on Session 0 zombies` — process-list heuristic + helper script `Tools/AFL_Yolo/kill_ubt_zombies.ps1`
- Validated all 3 in production during AFL-0206 + AFL-0207 attempts

**Closures:**
- AFL-0110: WorldSettings UI fix + ShooterCore-actor cleanup (commits `6805ff97` + `68db4743`). The L_AFL_Arena_Test testbed is now genuinely project-owned with no cross-plugin refs.

## Sprint 2 VFX/Audio/Design slice — the analysis

8 tasks remain in Sprint 2 by the master doc. They split into three buckets by how the orchestrator can drive them:

### Bucket A — UE-required (5 tasks)

These need UE Editor authoring with full GUI access. The orchestrator can scaffold C++ around them but cannot author the actual asset graphs (Niagara modules, material nodes, MetaSounds graphs). You drive these in UE directly; Claude can pair with you on a per-task basis but cannot run autonomously.

| ID | Title | Why UE-required |
|---|---|---|
| **AFL-0201** | `NS_AFL_PulseBeam` Niagara | Niagara emitter graph is a visual-node editor. Headless Python access exists but the API surface is small and brittle. |
| **AFL-0202** | `NS_AFL_HitSpark` Niagara | Same. |
| **AFL-0203** | `M_AFL_NeonMaster` material | Material node graph editor. `Mobile` variant via Material Layers or a #if mobile branch on the master. |
| **AFL-0205** | SFX MetaSounds | MetaSounds is a graph editor like Niagara. |
| **AFL-0208** | `NS_AFL_PrismBeam` Niagara | Same as 0201. Plus heat-driven flicker means it consumes the Heat attribute from AFL-0207 via a User Parameter set per-frame from the ability. |

### Bucket B — Mixed (2 tasks)

C++ scaffold + data asset structure CAN be authored headless. The TUNING phase (actual values, iteration on feel) needs UE PIE.

| ID | Title | Headless slice | UE slice |
|---|---|---|---|
| **AFL-0209** | Recoil/spread tuning on Pulse | C++ struct `FAFLPulseTuning`, `DA_AFL_PulseTuning_Default` data asset class declaration, integration into UAFLAG_Laser_Pulse to read from the asset, console cheats to override at runtime | Tuning values themselves (recoil curve, spread cone, recovery time) — design iteration via PIE |
| **AFL-0216** | Blender MCP bridge bootstrap | Python connector install + manifest, FBX import settings preset, a per-asset import-spec JSON shape, the round-trip Python harness | The actual Blender-side connector verification (Blender is a separate app; UE confirms reimport worked) |

### Bucket C — Mostly headless (1 task)

| ID | Title | Notes |
|---|---|---|
| **AFL-0217** | GenAI mesh validator | `tools/afl_mesh_validator.py`. Pure Python: ingest Tripo/Meshy FBX outputs, run AFL-conformant checks (poly budget per LOD, naming convention, materials present, collision sized correctly, sockets named per skeleton), output a pass/fail manifest. No UE dependency for the validator script itself — it operates on FBX files directly via `pyfbx` or similar. AFL-0216's import pipeline would consume the manifest. |

## Recommended starting point

**Run AFL-0217 first** — pure Python, no UE dependency, no mutex/network/zombie risk, orchestrator-ready. Sets up the validator that AFL-0216 will use. ~30 min Claude session.

Then **AFL-0209 (headless slice)** — C++ struct + data asset class + cheat manager hooks. Tuning values left as TODOs in the brief for the in-UE pass.

After those two, you have a meaningful headless slice shipped and the remaining work is honestly UE-side. Then drive AFL-0203 (material — easiest UE-side, lots of Lyra reference material to inherit from) in UE Editor, with Claude pairing on the C++ side (Material expression helpers if needed) but not driving autonomously.

## Concrete launch commands for tomorrow

### Step 1 — Verify clean state on session start

```powershell
cd C:\Dev\Bag_Man
git status                                          # expect: clean, on main
git log --oneline -3                                # expect: 8c6a8838 at top
python C:\Dev\Bag_Man\Tools\AFL_Yolo\orchestrator.py status   # expect: 18/0/0
```

### Step 2 — Authoring decisions to make before queue.yaml additions

Three open questions worth deciding before adding to queue.yaml:

1. **AFL-0217 deliverable shape:** Tripo and Meshy both produce FBX. Do we validate FBX bytes directly (via `pyfbx` SDK — adds a dep) OR do we shell out to Blender headless to convert FBX→glTF and validate from glTF (simpler bytes, no extra Python dep)? Lean toward Blender shell-out because we'll have Blender anyway for AFL-0216.

2. **AFL-0209 cheat command pattern:** existing `UAFLCombatCheats` uses `AFL.Combat.X` console commands. New `AFL.Combat.SpreadAngle <degrees>` / `AFL.Combat.RecoilAmp <pct>` would follow that. OR a single `AFL.Combat.LoadTuning <DA_AssetPath>` that swaps the whole DA. First option is simpler; second is more powerful but harder to live-edit.

3. **AFL-0216 Blender MCP scope:** the master doc §16.1 says "validate Blender↔Claude Code round-trip with a test asset (kitbash one Lyra prop, modify in Blender via MCP, reimport to UE5 with locked FBX settings)." That's three separate sub-tasks. Worth splitting into AFL-0216a (install + connector verify), AFL-0216b (round-trip a fixture), AFL-0216c (lock FBX settings preset)?

### Step 3 — Add chosen tasks to queue.yaml

Same pattern as the AFL-0210 / AFL-0207 / AFL-0206 / AFL-0204 / AFL-0212 entries from this session. Each entry needs `id`, `title`, `discipline`, `estimate`, `autonomy: full`, `branch`, `depends_on`, `verify`, `cheat_matrix`, `plugins_to_build`, `skills`, `files_hint`, `aik_brief`, `status: pending`. Reference the Sprint 2 slice in `queue.yaml` lines 356-485 for the format.

### Step 4 — Launch (orchestrator path)

```powershell
# Pre-flight check before any verify:compile task — catches Session 0 zombies
Get-Process dotnet -ErrorAction SilentlyContinue | Where-Object { $_.SessionId -eq 0 } | Format-Table

# Launch
python C:\Dev\Bag_Man\Tools\AFL_Yolo\orchestrator.py -v task AFL-XXXX
```

If the pre-flight catches a zombie, the orchestrator will halt cleanly with a Discord ping. Recovery:
```powershell
# Win+X → Terminal (Admin)
cd C:\Dev\Bag_Man
.\Tools\AFL_Yolo\kill_ubt_zombies.ps1
```
Then re-launch.

### Step 5 — UE-side authoring (Bucket A tasks)

For Niagara / Material / MetaSounds work:
1. Open `C:\Dev\Bag_Man\Bag_Man.uproject` in UE Editor
2. Work in UE GUI directly
3. Save the asset
4. **Close UE before any subsequent `git add` of .uasset files** (UE write-lock prevents clean staging)
5. `git add Plugins/GameFeatures/AFLCombat/Content/...` (or wherever)
6. Commit + push manually (orchestrator can't validate the asset graph contents anyway — it would just compile)

For pairing with Claude during UE-side work: ask Claude to read the relevant C++ (e.g., the User Parameter binding for AFL-0208's heat-driven flicker is on `UAFLAG_Laser_Beam`) and dictate exactly which Niagara user-parameter to expose. Claude can edit the C++ side while you author the .uasset in UE.

## Open hardening followups (not blocking, defer to future pass)

From this session's commit messages:

1. **`subprocess.TimeoutExpired` crash on `check=False` git fetch.** During AFL-0206 attempt 2, the orchestrator hit a 10-minute git fetch hang and crashed via uncaught `TimeoutExpired`. The `check=False` semantic doesn't catch this exception class. Fix: wrap the fetch in `try/except subprocess.TimeoutExpired`, log + downgrade to "proceed with stale main."
2. **Pre-flight halt path leaves a queue.yaml scribble.** When `pre_flight_check_ubt_mutex` halts a task, it writes `status: error` + `last_error` to queue.yaml but never commits (pre-flight exits before commit). Next task launch then halts on `ensure_clean_tree`. Fix: pre-flight halt path should `git checkout HEAD -- Tools/AFL_Yolo/queue.yaml` before exiting.
3. **AFL-0212 retroactive entry note:** AFL-0212's queue.yaml entry was added during this session as `status: merged` for completeness (it shipped early in Sprint 1 Block A via commit `68df114b`). The `aik_brief` is a backfill description, not a generated brief. Future audits should know this entry was a retroactive log, not a yolo PR.

## Open AFL-0210 followups (worth filing as their own tickets)

From the AFL-0210 transcript — Claude went above the brief and identified 4 brief-vs-live-build gaps that need code-side resolution:

1. **Add `AFL_PULSE: NoHit` log line** to `UAFLAG_Laser_Pulse::ServerApplyTargetData` no-target branch. The AFL-0210 P04 scenario expects a positive matcher, not "absence of hit_confirmed."
2. **Emit `AFL_TELEMETRY: hitscan_reject reason=range`** from Pulse and Beam ability paths when the trace's `MaxRange` cap was the reason it returned no hit. AFL-0210 P03 + B04 expect this. Today only `reason=ang` and `reason=beam_tick` are emitted.
3. **Wire `GE_AFL_Heat_DecayPause`** if the master-doc-specified "0.5s gate" before heat decay is a hard requirement. AFL-0207 implemented gating via `State.Combat.CoolingGate` tag check, but the timing of the gate vs the actual decay isn't strictly the same shape. AFL-0210 H02 will need disambiguation here.
4. **Confirm `State.Overheated` replication condition** — verify it's NOT `COND_OwnerOnly` so non-owning clients can observe it. AFL-0210 R03 will fail silently if the condition is wrong.

These are all small (S-estimate) code changes. Reasonable Sprint 2 cleanup before declaring it truly done.

## Files / paths cheat sheet

| Path | What |
|---|---|
| `BAG_MAN_LIVE_TRACKER_v2_2.html` | SSOT tracker, internal v2.6. Amendments banner = running narrative. |
| `Docs/HANDOFF_2026-05-20.md` | Prior session handoff. |
| `Docs/HANDOFF_2026-05-22_EOD.md` | **This document.** |
| `Docs/BAG_MAN_MASTER_BUILD_v2.0.md` | Master spec. Sprint structure, hard rails, architectural decisions. |
| `Docs/AFL_YOLO_PLAYBOOK.md` | Orchestrator operating manual. |
| `Docs/QA/AFL_REGRESSION_CHECKLIST.md` | Living index of per-task regression docs (AFL-0107 smoke + AFL-0210 matrix). |
| `Docs/QA/AFL_TEST_PLAN_TEMPLATE.md` | QA test plan template (AFL-0111 deliverable). |
| `Docs/QA/AFL-0107_PulseSmokeTest.md` | Sprint 1 Pulse smoke test instance. |
| `Docs/QA/AFL-0210_PulseBeamHeat_Regression.md` | Sprint 2 full regression matrix (14 scenarios). |
| `Tools/AFL_Yolo/orchestrator.py` | The orchestrator (~1400 lines). |
| `Tools/AFL_Yolo/verify.py` | UBT mutex detection + retry + Build.bat invocation. |
| `Tools/AFL_Yolo/kill_ubt_zombies.ps1` | Admin-elevated recovery script for Session 0 zombies. |
| `Tools/AFL_Yolo/queue.yaml` | Task queue. 18/18 merged. Adding new tasks tomorrow. |
| `Tools/AFL_Yolo/config.toml` | Live Discord webhook + paths. **Gitignored.** Don't commit. |
| `Tools/AFL_Yolo/lint_afl_0215.py` | Standalone architectural lint. 4 rails. |
| `Tools/AFL_Lint/asset_naming.py` | Asset prefix validator (AFL-0109). |
| `Tools/AFL_Lint/skill_registry.py` | Context-aware skill registry scanner (AFL-0218). |
| `Tools/AFL_Yolo/.state/orchestrator.log` | Orchestrator's own log. Always available. |
| `Plugins/GameFeatures/AFLCombat/Source/AFLCombat/` | Where all the Sprint 2 ability/infra code lives. |
| `Content/AFL/Maps/L_AFL_Arena_Test.umap` | Sprint 1+2 combat testbed (Lyra L_ShooterGym derivative). |
| `Content/AFL/Experiences/B_LyraExperience_AFL_Arena_Test.uasset` | The Experience that L_AFL_Arena_Test boots. |

## Gotchas (carryover + new)

From prior handoffs (still relevant):
1. **UE Editor write-locks .uassets.** Close UE before any task that touches uassets.
2. **`http.version HTTP/1.1`** already globally set — don't re-apply.
3. **`http.postBuffer = 524288000`** already set.
4. **The orchestrator's `git add -A` sweeps mid-task queue.yaml writes into the PR.** Every PR ships with stale `status: running`; cleanup commit pattern resolves it.
5. **`gh pr merge --squash --auto`** is the working substitute for phone-merge when network's flaky.

New this session:
6. **The pre-flight UBT mutex check catches Session 0 zombies.** When it halts: admin PowerShell + `kill_ubt_zombies.ps1` + retry. Memory: `feedback_ue_open_locks_uassets.md`.
7. **Do NOT use real Build.bat probes for mutex detection** — they leak zombies via cmd.exe → dotnet.exe orphan chains. Process-list heuristics only. Documented in `verify.py:list_suspect_ubt_zombies`.
8. **`unreal.set_editor_property()` refuses to write EditDefaultsOnly properties.** AFL-0110 hit this on `WorldSettings.DefaultGameplayExperience`. Fix is UI click or `FProperty` reflection helper. Memory: `feedback_lyra_worldsettings_editonly_gate.md`.
9. **AssetValidator_AssetReferenceRestrictions blocks /Game/AFL/ maps from referencing ShooterCore plugin assets.** If you duplicate any Lyra plugin map to /Game/AFL/, audit and remove all placed actors whose asset path starts with `/Plugins/GameFeatures/ShooterCore/`. Otherwise save fails.
10. **`ConvertTo-Json -AsArray` is PowerShell 7+ only.** Windows PowerShell 5.1 errors silently. Use the standard `ConvertTo-Json -Compress` and normalize single-vs-list in the consumer.

## Suggested resume sequence

1. **Verify clean state** (Step 1 above).
2. **Decide on the 3 authoring questions** (Step 2 above) — write the answers somewhere durable (memory entry or addendum to this doc).
3. **Add AFL-0217 + AFL-0209 to queue.yaml** with chosen specs.
4. **Launch AFL-0217 via orchestrator.** Pure Python, no UE dependency. Most likely to ship clean on first try.
5. **Launch AFL-0209 (headless slice) via orchestrator.** C++ struct + data asset + cheat hooks.
6. **Decide on Bucket A approach:** drive AFL-0203 next (easiest UE-side task; lots of Lyra master materials to learn from) OR pause Sprint 2 VFX and start Sprint 3 ability work that's headless-feasible (AFL-0301 stand up `AFLMovement` plugin is a 5-min scaffold task).
7. **If pursuing AFL-0203:** open UE, copy Lyra's `M_LyraDefaultMaterial` (or similar) to `M_AFL_NeonMaster`, branch with Material Layers for the Mobile variant, save, audit, commit, push.

---

*Generated 2026-05-22 EOD. Session shipped 9 PRs + 4 orchestrator hardening commits + AFL-0110 closure + 2 tracker reconciles. Active queue is fully exhausted. Sprint 2 ability+infrastructure complete. Next session picks up VFX/Audio/Design slice with the headless tasks first.*
