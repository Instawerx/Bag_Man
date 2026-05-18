# AFL YOLO Playbook

> Operating manual for `afl_yolo`, the autonomous build orchestrator that
> drives BAG MAN forward while you sleep, commute, or do anything that isn't
> sitting in front of UE 5.6 yourself.

This is the source of truth for how the system is supposed to behave. If
the code disagrees with the playbook, the code wins and the playbook gets
patched in the same PR.

**Companion docs:**

- `Tools/AFL_Yolo/README.md` — 10-minute quickstart
- `Tools/AFL_Yolo/queue.yaml` — task definitions (the actual work)
- `Tools/AFL_Yolo/config.example.toml` — annotated config reference

---

## 1. What it is

`afl_yolo` is a Python daemon that lives in `Tools/AFL_Yolo/` and runs on the
studio box (`C:\Dev\Bag_Man`). It reads a queue of AFL tasks and, for each
one, it:

1. Picks the next eligible task (respecting `depends_on` and `autonomy`).
2. Cuts a `yolo/<task-id>-<slug>` branch off `main`.
3. Spawns Claude Code with a tailored Session Contract — discipline skills
   pre-loaded, hard rails baked in, master-doc section references inline.
4. Runs the **AFL-0215 architectural lint** on changed C++ files.
5. Compiles affected plugins via `Build.bat LyraEditor Win64 Development`.
6. Runs the task's **cheat acceptance matrix** if defined (headless
   `UnrealEditor-Cmd` with console cheats, parsing the log for
   `AFLCombatCheats: OK <Name>` tokens).
7. Validates safety caps (changed files, deletions, LFS bandwidth).
8. Commits, pushes, opens a PR with the `yolo` label, pings Discord.
9. Polls the PR every 60s until you tap **Merge** from your phone.
10. Loops to step 1.

You drive it from three apps on your phone: **GitHub mobile** (the merge
button), **Discord** (status pings and the `!afl halt` command), and the
**Claude mobile app** (co-reviewer for tricky PRs).

---

## 2. Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    queue.yaml (the plan)                    │
└─────────────────────────────────┬───────────────────────────┘
                                  │ pick next
                                  ▼
┌─────────────────────────────────────────────────────────────┐
│        afl_yolo daemon  (Tools/AFL_Yolo/orchestrator.py)    │
│  ├─ Lockfile + state.json                                   │
│  ├─ Branch off main                                         │
│  ├─ Build Session Contract (aik_session.py)                 │
│  └─ Spawn `claude` with --dangerously-skip-permissions      │
└─────────────────────────────────┬───────────────────────────┘
                                  │
                                  ▼
┌─────────────────────────────────────────────────────────────┐
│   verify.py: AFL-0215 lint  →  Build.bat  →  cheat matrix   │──┐
└─────────────────────────────────┬───────────────────────────┘  │ FAIL
                                  │ PASS                         │
                                  ▼                              ▼
┌─────────────────────────────────────────────────────────────┐  ┌──────────────────┐
│   Safety caps  →  commit  →  push  →  gh pr create          │  │  Rollback,       │
└─────────────────────────────────┬───────────────────────────┘  │  mark errored,   │
                                  │                              │  bump fail count │
                                  ▼                              └────────┬─────────┘
┌─────────────────────────────────────────────────────────────┐           │
│  notify.py → Discord webhook → ping your phone              │           │
└─────────────────────────────────┬───────────────────────────┘           │
                                  │                                       │
                                  ▼                                       │
┌─────────────────────────────────────────────────────────────┐           │
│  Poll PR state every 60s (gh pr view --json state,mergedAt) │           │
└─────────────────────────────────┬───────────────────────────┘           │
                                  │ you merge from phone                  │
                                  ▼                                       ▼
                          loop to next task                    if ≥2 in a row → HALT
```

The daemon is single-threaded by design. One task at a time. No parallelism
in the loop because the bottleneck is the human merge, not the studio box.

---

## 3. Why this design

**Why PR-per-task instead of batched commits?**
Because you need to be able to revert one failed task without losing the
ones around it. Every PR is the unit of work and the unit of recovery.

**Why poll for merges instead of webhook?**
Polling is dumber, simpler, and survives the studio box losing internet for
a few minutes. The cost is one `gh pr view` per minute per open PR —
negligible.

**Why `--dangerously-skip-permissions` on Claude Code?**
Because if every file write requires a phone tap, the loop's bottleneck
becomes you, defeating the point. Safety is provided by the orchestrator's
post-execution rails (AFL-0215 lint, build verify, file caps, cheat
matrix), not by Claude Code's per-action prompts. The rails are
deterministic and reviewable; an interactive permission stream is neither.

**Why halt after 2 failures, not 5 or 10?**
Three failures in a row is almost always a systemic issue (build env
broken, queue.yaml task too ambiguous, model regression). Better to wake
the human at 2 than waste an hour grinding through identical failures.

**Why is `art` and `design` flagged `human_only` by default?**
Because Claude Code can't pick your game's color palette or write a design
spec from one sentence. Once you have detailed specs in the queue items
themselves, you can remove the disciplines from
`safety.require_human_for_disciplines` in `config.toml`.

---

## 4. Safety rails (the rules of the game)

Every rail is configurable in `config.toml`. The defaults are deliberately
tight — loosen them only with a written justification in the playbook.

| Rail                          | Default          | Trigger                              | Action            |
|-------------------------------|------------------|--------------------------------------|-------------------|
| Consecutive failures          | 2                | Two tasks errored in a row           | Hard halt         |
| Max changed files / task      | 100              | `git diff --stat` count > cap        | Rollback + errored|
| Max deletions / task          | 5                | Net file deletions > cap             | Rollback + errored|
| LFS bandwidth                 | 90% of 250 GB    | Pushed LFS bytes ≥ threshold         | Hard halt         |
| AFL-0215 lint                 | Always on        | Any regex match in changed C++       | Rollback + errored|
| Build compile                 | Always on        | Build.bat exit ≠ 0                   | Rollback + errored|
| Cheat matrix (if defined)     | Per task         | Missing `AFLCombatCheats: OK <X>`    | Rollback + errored|
| `autonomy: human_only`        | Per task         | Daemon would auto-start the task     | Skip in `run`     |
| `autonomy: phase_gate`        | Per task         | Phase boundary task                  | Pause for GO      |
| PR merge timeout              | 24 h             | No merge inside window               | Mark `timed_out`  |
| `dry_run` flag                | False (off)      | Set in config.toml                   | All side effects suppressed |

**Hard halt** = loop exits, daemon stops, state preserved, must be
restarted manually. **Rollback + errored** = working tree reset to `main`,
task marked errored in `queue.yaml`, failure counter incremented, loop
moves to the next eligible task (or halts if the counter trips).

---

## 5. AFL-0215 — the architectural guard

This is the only architectural lint that runs in the YOLO loop. It exists
because Lyra's mistakes are silent — a direct `GiveAbility` call compiles
fine, ships fine, and quietly defeats the entire ability-set architecture.
The lint catches the three mistakes that cost the most to unwind later.

| Pattern matched                                              | Why it's a violation                                                                 |
|--------------------------------------------------------------|--------------------------------------------------------------------------------------|
| `AbilitySystemComponent->GiveAbility`                        | Abilities must come from `ULyraPawnData` ability sets, not be granted ad-hoc.       |
| `GetAttributeSet<UAFLAttributeSet_Combat>...->SetHealth`     | Damage must route through `UAFLDamageExecCalc` so DR / armor / energy hooks fire.   |
| `GetPlayerViewPoint` (anywhere)                              | Hitscan uses **client TargetData** per AFL-0106. Server viewpoint is a desync bug.  |

The rule is implemented twice on purpose:

- **Locally**, in `Tools/AFL_Yolo/orchestrator.py` → `LINT_RULES` (Python
  regex). Catches it before commit.
- **In CI**, in `.github/workflows/afl-yolo-pr.yml` (PowerShell regex).
  Catches it before merge as an independent re-check.

**If you change one, change both.** The CI workflow comments call this out.

**False positives** are rare but possible (e.g., calling `GetPlayerViewPoint`
in an editor-only debug path). The escape hatch is a same-line marker
comment:

```cpp
const FVector EyeLoc = PC->GetPlayerViewPoint(...);  // afl-0215-ignore: editor-only debug overlay
```

The current lint does **not** honor `afl-0215-ignore` yet (the regex would
match anyway). Adding the ignore-list support is **AFL-0216** and lives in
the backlog; until then, restructure the code or land an explicit playbook
addendum justifying the violation.

---

## 6. queue.yaml schema

```yaml
- id: AFL-0104                    # required, unique, matches tracker ID
  title: "Stand up UAFLAG_Laser_Pulse base class"
  discipline: eng                  # eng | devops | art | vfx | ui | audio | animation | qa | design
  estimate: XL                     # S | M | L | XL  (rough effort)
  autonomy: yolo                   # yolo | human_only | phase_gate
  branch: yolo/afl-0104-ag-laser-pulse
  status: pending                  # pending | in_progress | merged | errored | timed_out
  depends_on: []                   # list of task IDs that must be `merged` first
  verify: compile                  # none | compile | compile+test | compile+cheat-matrix
  plugins_to_build: [AFLCombat]    # passed to verify.py
  cheat_matrix:                    # optional, used when verify includes cheat-matrix
    - "AFL.Combat.Damage 18"
    - "AFL.Combat.EnergyGain 10"
  skills:                          # AFL skills loaded into Claude Code's context
    - afl-cpp-lyra-developer
    - unreal-engine-expert
  files_hint:                      # advisory list, helps Claude Code scope edits
    - Plugins/AFLCombat/Source/AFLCombat/Public/Abilities/AFLGA_Laser_Pulse.h
    - Plugins/AFLCombat/Source/AFLCombat/Private/Abilities/AFLGA_Laser_Pulse.cpp
  aik_brief: |                     # the task prompt body (multiline)
    Create UAFLAG_Laser_Pulse extending UGameplayAbility...
    Reference: master doc Sec. 6.2, Sec. 7.
```

**Status transitions** are written back to `queue.yaml` by the daemon:
`pending → in_progress → (merged | errored | timed_out)`. Don't hand-edit
status while the daemon is running — it'll round-trip clobber your edit on
the next save.

**Adding a new task**: append to `queue.yaml`, run `python orchestrator.py
validate`, commit the change manually (outside the YOLO loop), then either
let the daemon pick it up on its next iteration or invoke it directly with
`python orchestrator.py task <ID>`.

---

## 7. The Session Contract (what Claude Code sees)

`aik_session.py` builds a single prompt per task with this structure:

1. **Identity** — "You are working on BAG MAN (AFL), a UE5 Lyra-based game."
2. **Skills loaded** — the discipline's default skills plus any task-specific
   skills from `queue.yaml` (`skills:`).
3. **HARD RAILS** — the 7 immutable rules:
   1. Abilities come from `ULyraPawnData` ability sets, never direct
      `GiveAbility`.
   2. Damage routes through `UAFLDamageExecCalc`, never direct
      `SetHealth`.
   3. Hitscan uses client-side `TargetData` per AFL-0106, never server
      `GetPlayerViewPoint`.
   4. All AFL classes use the `AFL` prefix (`UAFL*`, `AAFL*`, `FAFL*`).
   5. No modifications to Lyra engine source — extend, don't fork.
   6. Gameplay tags declared in the form
      `GameplayTagList=(Tag="...",DevComment="...")` per master doc Sec. 14.5.
   7. Commit messages start with the task ID (`AFL-0104: ...`).
4. **The task** — the `aik_brief` from `queue.yaml`.
5. **WHAT DONE LOOKS LIKE** — the verify mode, cheat matrix entries, file
   hints, and a reminder that the orchestrator will re-check everything
   post-edit.

If you find Claude Code drifting from a rail, the fix is almost always to
strengthen the brief, not to add a new rail. Rails are for things that
must hold for every task forever.

---

## 8. Discord integration

The daemon posts rich embeds via webhook. Color coding:

| Event           | Color   | Meaning                                                |
|-----------------|---------|--------------------------------------------------------|
| `loop_start`    | Blurple | Daemon came up, starting first task                    |
| `pr_open`       | Gold    | PR opened, your turn                                   |
| `task_merged`   | Green   | You merged, daemon moving on                           |
| `task_failed`   | Red     | Task errored (rollback already happened)               |
| `loop_halt`     | Red     | Loop stopped (failure cap, halt flag, or fatal error)  |
| `needs_human`   | Amber   | `human_only` or `phase_gate` task, daemon paused       |

**The `!afl halt` command**: the daemon polls the local file
`.state/halt.flag` between tasks. To halt from Discord:

1. Set up a tiny Discord bot (any platform — a 20-line Cloudflare Worker
   works fine) that on `!afl halt` writes the halt flag to the studio box
   over SSH or a webhook.
2. The daemon notices on its next loop iteration and exits cleanly after
   the current task finishes.

A reference bot implementation is **out of scope** for the YOLO core (it
varies per studio infrastructure), but the protocol — "touch
`.state/halt.flag` and the daemon stops" — is stable.

If you don't want to wire a bot up yet, `python orchestrator.py halt` from
any SSH session to the studio box does the same thing.

---

## 9. Self-hosted runner provisioning

`.github/workflows/afl-yolo-pr.yml` requires a runner with the labels
`[self-hosted, windows, ue5]`. One-time setup:

1. Spin up or repurpose a Windows machine with UE 5.6 installed (at
   `C:\Program Files\Epic Games\UE_5.6`), Visual Studio 2022 with the
   Game Development with C++ workload, Python 3.11+, `git` + `git-lfs`,
   and `gh` CLI.
2. In the repo, **Settings → Actions → Runners → New self-hosted runner**.
   Follow the Windows install steps GitHub gives you.
3. When prompted for labels, add `windows ue5` (the `self-hosted` label is
   automatic).
4. Run the runner as a service:
   `.\svc.cmd install` then `.\svc.cmd start`.
5. Test: push any branch with the `yolo` label on a PR. The
   `afl-yolo/build` check should appear within ~30s.

**For the studio box doubling as the runner**: this works fine — the
daemon and the runner are isolated. The runner picks up jobs from
GitHub's queue; the daemon does its own local Build.bat. They don't
contend except for compile cache, which is harmless.

---

## 10. Troubleshooting

### "Lockfile present (PID NNN)"

```powershell
python orchestrator.py status
```

If the PID is dead, the status command will report the stale lock and
self-clean. If the PID is alive but the daemon is hung, kill it
(`Stop-Process -Id NNN`) then re-run `status` to clean up.

### Loop halted after 2 consecutive failures

1. Open `.state/orchestrator.log` and read the last task's transcript.
2. Common causes, in order:
   - **AFL-0215 lint hit**: the brief was too vague and Claude Code
     reached for a forbidden API. Tighten the `aik_brief` in
     `queue.yaml`, restart.
   - **Compile failure**: missing module include, mismatched UCLASS
     specifiers. Often the fix is a one-line `aik_brief` addition like
     "Add `UAFLCombat` to `PublicDependencyModuleNames`."
   - **Cheat matrix miss**: cheat command typo, or the ability didn't
     actually grant correctly. Test the cheat by hand in the editor.
3. Once the root cause is fixed, reset the failure counter manually:
   delete `state.json` or run `python orchestrator.py status` (it
   resets the counter on successful task completion automatically).
4. Restart: `python orchestrator.py run`.

### LFS quota halt

The 250 GB GitHub LFS allocation is a hard cap. If the daemon halts on
LFS:

```powershell
git lfs prune --verify-remote                 # remove local LFS objects
                                              # that are safe on remote
```

If you're close to the org cap, contact GitHub support to bump the
allocation, or audit what's stored with `git lfs ls-files` and either
move heavy assets to a separate Drive/S3 stash or git-rm objects you no
longer need (rare and risky — only if you're sure).

### Build timeouts

Default is 1800s (30m) for compile, 1200s (20m) for verify. If you're
legitimately running an XL task that needs more, bump
`[build].timeout_s` in `config.toml`. If it's hanging, kill the
process tree manually (`Get-Process UnrealHeader* | Stop-Process`)
and the daemon will report the timeout and roll back.

### PR never merges → task `timed_out`

Default merge window is 24h. Three options:

- Merge it now and let the daemon discover the merge on its next loop.
- Manually close the PR; the branch will linger but the daemon won't
  retry the task — bump its status to `pending` in `queue.yaml` if you
  want it re-attempted.
- Bump `[github].pr_poll_max_hours` if you're going on vacation.

### AFL-0215 false positive

If a violation is legitimately unavoidable:

1. Add a `// afl-0215-ignore: <reason>` comment on the offending line.
2. Until **AFL-0216** ships (ignore-list support), the lint will still
   trip. Workaround: restructure the code, **or** push the change
   manually outside the YOLO loop with a written justification on the
   PR.

### Discord pings stopped

```powershell
python notify.py loop_start --message "wiring test"
```

If that doesn't deliver, the webhook is dead. Regenerate it in
Discord's server settings and update `[discord].webhook_url` in
`config.toml`. No daemon restart needed — config is re-read per event.

---

## 11. Phone workflow — the merge dance

Once the daemon is running, your phone workflow is:

1. **Ping arrives in Discord**: gold embed, "PR #N opened: AFL-0104".
2. **Tap the PR link**: opens in GitHub mobile.
3. **Review the diff**: scroll the Files Changed tab. The CI check
   `afl-yolo/build` should be green (you can wait for it if it isn't).
4. **If anything smells off**: copy the diff (GitHub mobile → Files
   Changed → menu → Copy diff), paste it into the Claude app, ask
   "review this for AFL hard rails." Opus 4.7 will catch what the lint
   doesn't.
5. **Tap Merge**: the daemon notices within 60s, posts the green
   `task_merged` ping, picks up the next task.

That's the whole loop. Done from a phone in a coffee shop. The studio
box does the actual UE5 work.

---

## 12. When to stop the loop and think

The YOLO loop is great at small, well-defined tasks. It's **not** great
at:

- Designing new systems from scratch (use the `expert-game-designer`
  skill in a normal Claude conversation, lock down a design, then
  decompose into YOLO tasks).
- Resolving cross-cutting refactors (touch too many files, will hit the
  100-file cap).
- Art and audio polish (subjective, no test signal).
- Performance tuning (needs Unreal Insights traces, profiler runs, a
  human eye on flame graphs).

Those tasks get marked `autonomy: human_only` in `queue.yaml` and live
outside the loop. The daemon will skip them and surface them in
`status` so you know what's queued for your manual attention.

---

## 13. Bootstrap — first PR is special

The very first YOLO PR will land the orchestrator itself
(`Tools/AFL_Yolo/`), the CI workflow (`.github/workflows/afl-yolo-pr.yml`),
the playbook (this file), and the initial `queue.yaml`. That PR **also**
satisfies:

- **AFL-0108** (GitHub Actions Win64 Dev build on PR) — the workflow file
  IS the deliverable.
- **AFL-0215** (CI lint rule for direct ability grants / Health writes /
  server viewpoint) — the workflow's lint step IS the deliverable.

Mark both `merged` in `queue.yaml` immediately after the bootstrap PR
merges. The daemon will then start from AFL-0104.

---

## 14. Version history

| Date       | Change                                                            |
|------------|-------------------------------------------------------------------|
| 2026-05-17 | v0.1.0 — initial system: orchestrator, verify, lint, CI, playbook |

Future changes go here. The version number in `orchestrator.py`
(`VERSION = "0.1.0"`) and the version row above must match.
