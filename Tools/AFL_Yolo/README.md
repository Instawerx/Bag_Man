# afl_yolo — Autonomous Build Orchestrator

`afl_yolo` runs the BAG MAN build forward without you sitting in front of it.
It picks the next task from `queue.yaml`, spawns Claude Code with the right
AFL skills loaded, compiles + lints + runs the cheat acceptance matrix,
opens a PR, pings Discord, then waits for you to tap **Merge** on GitHub
mobile before moving to the next task.

You drive it from your phone. The studio box does the work.

For the architecture overview, safety-rail reference, queue schema, runner
provisioning, and troubleshooting, see **`Docs/AFL_YOLO_PLAYBOOK.md`**.
This file is the 10-minute quickstart.

---

## Requirements

On the studio box (Windows, where `C:\Dev\Bag_Man` lives):

- Python 3.11+ (3.10 works if you `pip install tomli`)
- `pip install pyyaml requests`
- `git` and `git-lfs` in PATH
- `gh` CLI authenticated against the `C12-Ai-Gaming` org
  (run `gh auth login` once, scope: `repo`)
- Claude Code installed and on PATH (the `claude` shim)
- UE 5.6 installed at `C:\Program Files\Epic Games\UE_5.6`
- A clean working tree on `main` (the daemon refuses to start otherwise)

On your phone:

- GitHub mobile app (for one-tap PR merges)
- Discord app (for status pings; optional but recommended)
- Claude mobile app (for paste-the-diff co-review on tricky PRs)

---

## First-run wiring check (5 minutes)

```powershell
cd C:\Dev\Bag_Man\Tools\AFL_Yolo

# 1. Configure
copy config.example.toml config.toml
notepad config.toml
#   - Fill in [discord].webhook_url (or leave blank to disable Discord)
#   - Confirm engine_root matches your install
#   - Set [orchestrator].dry_run = true for this first run

# 2. Validate config + queue schema (no side effects)
python orchestrator.py validate

# 3. Show the queue head and current state
python orchestrator.py status
```

`validate` will exit 0 if `config.toml` parses, `queue.yaml` is structurally
correct, and every `depends_on` reference resolves. `status` prints the next
task that would run, current consecutive-failure count, and whether a halt
flag is set.

---

## Smoke test on one task (15 minutes)

With `dry_run = true` in `config.toml`:

```powershell
python orchestrator.py task AFL-0104
```

This walks every state transition (branch creation, Discord ping, PR open,
PR poll, merge wait, advance) but skips the real Claude Code spawn, real
Build.bat, real git push, and real `gh pr create`. You'll see the full event
trace in `.state/orchestrator.log` and structured records in
`.state/runs.jsonl`. Discord will receive real pings if the webhook is
configured — useful for verifying mobile alerts work.

Once the dry run looks clean, set `dry_run = false` and run a real one:

```powershell
python orchestrator.py task AFL-0104
```

When the PR pings your phone, review the diff in GitHub mobile, tap Merge.
The daemon exits cleanly after one task in `task` mode.

---

## Going YOLO

```powershell
python orchestrator.py run
```

That's it. The daemon will:

1. Pick the next eligible task from `queue.yaml` (respects `depends_on`,
   skips tasks with `autonomy: human_only` unless invoked with `task <id>`).
2. Create branch `yolo/<task-id-lowercase>-<slug>`.
3. Spawn Claude Code with the Session Contract for that task — discipline
   skills auto-loaded, AFL hard rails baked into the prompt, master-doc
   section references inline.
4. Run the AFL-0215 lint rule on changed C++ files. Any violation rolls
   back the working tree, marks the task `errored`, increments the
   consecutive-failure counter.
5. Compile the affected plugins (`AFLCore`, `AFLCombat`, …) via Build.bat.
6. If the task has a `cheat_matrix:`, launch `UnrealEditor-Cmd` headless
   and assert every cheat emits its `AFLCombatCheats: OK <Name>` token.
7. Check safety caps (≤100 files changed, ≤5 deletions, LFS bandwidth
   under 90% of the 250 GB quota).
8. Commit, push, open a PR with the `yolo` label, ping Discord.
9. Poll the PR every 60 s until you merge from your phone, then loop.

Run `python orchestrator.py halt` (or send `!afl halt` in Discord — the
daemon polls for it) to stop the loop **after the current task** finishes
cleanly. Ctrl-C does the same.

---

## Daily commands

```powershell
python orchestrator.py status              # what's running / queued
python orchestrator.py task <ID>           # one-shot, ignore the loop
python orchestrator.py halt                # graceful stop after current task
python orchestrator.py run --max-tasks 3   # run only N tasks then stop
python orchestrator.py validate            # CI-safe queue lint
```

State is in `Tools/AFL_Yolo/.state/`:

- `state.json` — last run state (consecutive failures, last task, halt flag)
- `runs.jsonl` — append-only event log, one JSON object per line
- `orchestrator.log` — human-readable rolling log
- `orchestrator.lock` — PID lockfile, stale-cleaned on next start

That whole directory is safe to delete to reset state. The queue and config
are untouched.

---

## Mobile workflow

| Where         | What you do                                                |
|---------------|------------------------------------------------------------|
| Discord       | Glance at status pings, type `!afl halt` to stop the loop  |
| GitHub mobile | Open the PR notification, review the diff, tap **Merge**   |
| Claude app    | For a PR that smells off: paste the diff, ask Opus 4.7 to  |
|               | review it before you merge                                 |

The daemon assumes you'll merge within `pr_poll_max_hours` (default 24).
After that it marks the task `timed_out`, moves to the next one, and the
PR stays open for whenever you get back.

---

## When something goes wrong

| Symptom                                  | First check                                                   |
|------------------------------------------|---------------------------------------------------------------|
| "Lockfile present (PID NNN)"             | `orchestrator status` — if PID is dead it'll self-clean       |
| Build fails twice in a row → loop halts  | Read `.state/orchestrator.log`, fix root cause, restart `run` |
| LFS halt at 90%                          | Bump quota or run `git lfs prune` on the studio box           |
| Discord pings stop arriving              | Re-test webhook: `python notify.py loop_start --message test` |
| PR never merges → task `timed_out`       | Merge it manually from GitHub mobile; daemon picks up on next |
| AFL-0215 false positive                  | Add a `# afl-0215-ignore` comment on the line; see playbook   |

Full troubleshooting tree, error code reference, and recovery procedures
live in **`Docs/AFL_YOLO_PLAYBOOK.md`**.

---

## What it will NOT do

`afl_yolo` is deliberately conservative. It will not:

- Run tasks with `autonomy: human_only` (greybox art, design specs)
- Modify branches other than its own `yolo/...` branches
- Merge its own PRs (that's the human's only required step)
- Push to `main` directly
- Delete more than `max_deletions_per_task` files in one task
- Continue after `max_consecutive_failures` (default 2) in a row
- Push when LFS usage is over the configured threshold
- Spawn Claude Code without `--dangerously-skip-permissions` unless
  `[claude_code].skip_permissions = false` in the config

Every one of those rails is configurable in `config.toml`. None of them
should ever be loosened without writing down why in the playbook.
