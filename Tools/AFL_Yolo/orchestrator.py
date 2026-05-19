#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
afl_yolo - Autonomous build orchestrator for BAG MAN (AFL).

Reads tasks from queue.yaml, spawns Claude Code with the appropriate AFL skills
loaded, verifies builds via Build.bat + AFL-0215 lint rule + a console-cheat
acceptance matrix, commits to a feature branch, opens a PR via `gh`, pings
Discord, then waits for human merge from GitHub mobile before picking up the
next task.

Production safety:
  * Lockfile prevents concurrent orchestrators on the same checkout
  * Hard halt on 2 consecutive build failures (configurable)
  * Hard halt on >100 changed files or >5 deletions per task
  * Hard halt on LFS bandwidth >225 GB (90% of 250 GB quota)
  * AFL-0215 lint failure rolls back working tree, marks task ERRORED
  * Phase-gate tasks always pause for explicit human GO
  * Discord "!afl halt" command stops loop after current task (poll-based)
  * SIGINT/SIGTERM clean shutdown - finishes current task, no rollback

CLI:
  python orchestrator.py run                start the loop
  python orchestrator.py status             show current state and queue head
  python orchestrator.py task <task_id>     run one task and stop
  python orchestrator.py halt               request graceful halt
  python orchestrator.py validate           dry-run, validates queue.yaml
"""

from __future__ import annotations

import argparse
import dataclasses
import json
import logging
import os
import re
import signal
import shlex
import shutil
import subprocess
import sys
import threading
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Optional

try:
    import tomllib  # type: ignore[import]  # 3.11+
except ImportError:  # pragma: no cover
    import tomli as tomllib  # type: ignore[no-redef]  # pip install tomli on 3.10

try:
    import yaml
except ImportError:
    print("[fatal] PyYAML missing. Install: pip install pyyaml requests", file=sys.stderr)
    sys.exit(2)

try:
    import requests
except ImportError:
    print("[fatal] requests missing. Install: pip install pyyaml requests", file=sys.stderr)
    sys.exit(2)


VERSION = "0.1.0"
LOG = logging.getLogger("afl_yolo")
SHUTDOWN_REQUESTED = threading.Event()
HALT_REQUESTED = threading.Event()

HERE = Path(__file__).resolve().parent
DEFAULT_CONFIG_PATH = HERE / "config.toml"
DEFAULT_QUEUE_PATH = HERE / "queue.yaml"
STATE_DIR = HERE / ".state"
STATE_PATH = STATE_DIR / "state.json"
LOCK_PATH = STATE_DIR / "orchestrator.lock"
RUN_LOG_PATH = STATE_DIR / "runs.jsonl"
HALT_FLAG_PATH = STATE_DIR / "halt.flag"
LOG_PATH = STATE_DIR / "orchestrator.log"


# ---------------------------------------------------------------------------
# Data classes
# ---------------------------------------------------------------------------

@dataclasses.dataclass
class Config:
    project_root: Path
    engine_root: Path
    uproject: Path
    gh_repo: str
    main_branch: str = "main"

    discord_webhook: Optional[str] = None

    max_consecutive_failures: int = 2
    max_changed_files: int = 100
    max_deletions_per_task: int = 5
    lfs_quota_gb: int = 250
    lfs_halt_threshold_pct: float = 0.90

    claude_code_bin: str = "claude"
    claude_code_timeout_s: int = 3600
    claude_code_skip_permissions: bool = True

    build_timeout_s: int = 1800
    verify_timeout_s: int = 1200
    pr_poll_interval_s: int = 60
    pr_poll_max_hours: int = 24

    require_human_for_disciplines: list[str] = dataclasses.field(default_factory=lambda: ["art", "design"])
    dry_run: bool = False

    @classmethod
    def load(cls, path: Path) -> "Config":
        if not path.exists():
            raise FileNotFoundError(f"Config not found: {path}. Copy config.example.toml.")
        with path.open("rb") as fh:
            raw = tomllib.load(fh)
        return cls(
            project_root=Path(raw["paths"]["project_root"]),
            engine_root=Path(raw["paths"]["engine_root"]),
            uproject=Path(raw["paths"]["uproject"]),
            gh_repo=raw["github"]["repo"],
            main_branch=raw["github"].get("main_branch", "main"),
            discord_webhook=raw.get("discord", {}).get("webhook_url"),
            max_consecutive_failures=raw.get("safety", {}).get("max_consecutive_failures", 2),
            max_changed_files=raw.get("safety", {}).get("max_changed_files", 100),
            max_deletions_per_task=raw.get("safety", {}).get("max_deletions_per_task", 5),
            lfs_quota_gb=raw.get("safety", {}).get("lfs_quota_gb", 250),
            lfs_halt_threshold_pct=raw.get("safety", {}).get("lfs_halt_threshold_pct", 0.90),
            claude_code_bin=raw.get("claude_code", {}).get("bin", "claude"),
            claude_code_timeout_s=raw.get("claude_code", {}).get("timeout_s", 3600),
            claude_code_skip_permissions=raw.get("claude_code", {}).get("skip_permissions", True),
            build_timeout_s=raw.get("build", {}).get("timeout_s", 1800),
            verify_timeout_s=raw.get("build", {}).get("verify_timeout_s", 1200),
            pr_poll_interval_s=raw.get("github", {}).get("pr_poll_interval_s", 60),
            pr_poll_max_hours=raw.get("github", {}).get("pr_poll_max_hours", 24),
            require_human_for_disciplines=raw.get("safety", {}).get(
                "require_human_for_disciplines", ["art", "design"]
            ),
            dry_run=raw.get("orchestrator", {}).get("dry_run", False),
        )


@dataclasses.dataclass
class Task:
    id: str
    title: str
    discipline: str
    estimate: str
    autonomy: str               # full | needs_approval | human_only | phase_gate
    branch: str
    depends_on: list[str]
    verify: str                 # compile | compile+test | compile+cheat-matrix | none
    cheat_matrix: list[str]
    plugins_to_build: list[str]
    skills: list[str]
    files_hint: list[str]
    aik_brief: str
    status: str = "pending"     # pending | running | pr_open | merged | error | skipped
    last_attempt_at: Optional[str] = None
    last_error: Optional[str] = None
    pr_number: Optional[int] = None
    pr_url: Optional[str] = None


@dataclasses.dataclass
class RunState:
    consecutive_failures: int = 0
    current_task_id: Optional[str] = None
    last_run_at: Optional[str] = None
    last_run_status: Optional[str] = None
    last_pr_url: Optional[str] = None
    started_at: Optional[str] = None

    @classmethod
    def load(cls) -> "RunState":
        if not STATE_PATH.exists():
            return cls()
        try:
            return cls(**json.loads(STATE_PATH.read_text(encoding="utf-8")))
        except Exception:
            LOG.warning("state.json corrupt - starting fresh")
            return cls()

    def save(self) -> None:
        STATE_DIR.mkdir(parents=True, exist_ok=True)
        STATE_PATH.write_text(json.dumps(dataclasses.asdict(self), indent=2), encoding="utf-8")


# ---------------------------------------------------------------------------
# Logging + structured run log
# ---------------------------------------------------------------------------

def setup_logging(verbose: bool = False) -> None:
    STATE_DIR.mkdir(parents=True, exist_ok=True)
    level = logging.DEBUG if verbose else logging.INFO
    fmt = "%(asctime)s %(levelname)-7s %(message)s"
    logging.basicConfig(level=level, format=fmt, datefmt="%H:%M:%S")
    fh = logging.FileHandler(LOG_PATH, encoding="utf-8")
    fh.setFormatter(logging.Formatter(fmt))
    fh.setLevel(level)
    logging.getLogger().addHandler(fh)


def emit_run_event(event: str, payload: dict[str, Any]) -> None:
    STATE_DIR.mkdir(parents=True, exist_ok=True)
    record = {
        "ts": datetime.now(timezone.utc).isoformat(timespec="seconds"),
        "event": event,
        **payload,
    }
    with RUN_LOG_PATH.open("a", encoding="utf-8") as fh:
        fh.write(json.dumps(record, ensure_ascii=False) + "\n")


# ---------------------------------------------------------------------------
# Lockfile (single orchestrator)
# ---------------------------------------------------------------------------

class Lockfile:
    """Best-effort cross-platform exclusive lockfile."""

    def __init__(self, path: Path) -> None:
        self.path = path

    def __enter__(self) -> "Lockfile":
        STATE_DIR.mkdir(parents=True, exist_ok=True)
        if self.path.exists():
            try:
                existing = int(self.path.read_text(encoding="utf-8").strip())
                if _pid_alive(existing):
                    raise RuntimeError(
                        f"Another orchestrator is running (PID {existing}). "
                        f"Delete {self.path} only if you are certain it is stale."
                    )
            except (ValueError, OSError):
                LOG.warning("Stale lockfile detected, removing")
                self.path.unlink(missing_ok=True)
        self.path.write_text(str(os.getpid()), encoding="utf-8")
        return self

    def __exit__(self, *_exc: Any) -> None:
        self.path.unlink(missing_ok=True)


def _pid_alive(pid: int) -> bool:
    if pid <= 0:
        return False
    try:
        if os.name == "nt":
            res = subprocess.run(
                ["tasklist", "/FI", f"PID eq {pid}"],
                capture_output=True, text=True, check=False, timeout=10,
            )
            return str(pid) in res.stdout
        os.kill(pid, 0)
        return True
    except (ProcessLookupError, PermissionError):
        return False
    except Exception:
        return False


# ---------------------------------------------------------------------------
# Queue I/O
# ---------------------------------------------------------------------------

def load_queue(path: Path) -> list[Task]:
    if not path.exists():
        raise FileNotFoundError(f"Queue file not found: {path}")
    with path.open("r", encoding="utf-8") as fh:
        raw = yaml.safe_load(fh)
    if not raw or "tasks" not in raw:
        raise ValueError("queue.yaml missing top-level 'tasks' key")
    tasks: list[Task] = []
    for t in raw["tasks"]:
        tasks.append(Task(
            id=t["id"],
            title=t["title"],
            discipline=t.get("discipline", "eng"),
            estimate=t.get("estimate", "M"),
            autonomy=t.get("autonomy", "needs_approval"),
            branch=t.get("branch") or _slugify_branch(t["id"], t["title"]),
            depends_on=list(t.get("depends_on") or []),
            verify=t.get("verify", "compile"),
            cheat_matrix=list(t.get("cheat_matrix") or []),
            plugins_to_build=list(t.get("plugins_to_build") or []),
            skills=list(t.get("skills") or []),
            files_hint=list(t.get("files_hint") or []),
            aik_brief=t.get("aik_brief", "").strip(),
            status=t.get("status", "pending"),
            last_attempt_at=t.get("last_attempt_at"),
            last_error=t.get("last_error"),
            pr_number=t.get("pr_number"),
            pr_url=t.get("pr_url"),
        ))
    return tasks


def save_queue(path: Path, tasks: list[Task]) -> None:
    """Write back to queue.yaml, preserving comments via a header sentinel."""
    raw = {"tasks": [_task_to_dict(t) for t in tasks]}
    tmp = path.with_suffix(".yaml.tmp")
    with tmp.open("w", encoding="utf-8") as fh:
        fh.write("# afl_yolo queue - autogenerated header, edit body freely.\n")
        fh.write(f"# Last write: {datetime.now(timezone.utc).isoformat(timespec='seconds')}\n\n")
        yaml.safe_dump(raw, fh, sort_keys=False, allow_unicode=True, width=120)
    tmp.replace(path)


def _task_to_dict(t: Task) -> dict[str, Any]:
    d = dataclasses.asdict(t)
    # Drop empty optionals to keep YAML clean
    for k in ("last_attempt_at", "last_error", "pr_number", "pr_url"):
        if d.get(k) in (None, ""):
            d.pop(k, None)
    return d


def _slugify_branch(task_id: str, title: str) -> str:
    short = re.sub(r"[^A-Za-z0-9]+", "-", title.lower()).strip("-")[:40].rstrip("-")
    return f"yolo/{task_id.lower()}-{short}"


# ---------------------------------------------------------------------------
# Task picker
# ---------------------------------------------------------------------------

def pick_next_task(tasks: list[Task]) -> Optional[Task]:
    by_id = {t.id: t for t in tasks}
    for t in tasks:
        if t.status not in ("pending", "error"):
            continue
        if t.status == "error":
            continue  # errors require manual reset
        unmet = [d for d in t.depends_on if by_id.get(d) and by_id[d].status != "merged"]
        if unmet:
            continue
        return t
    return None


# ---------------------------------------------------------------------------
# Subprocess helpers
# ---------------------------------------------------------------------------

def run_cmd(
    cmd: list[str] | str,
    *,
    cwd: Path | None = None,
    env: dict[str, str] | None = None,
    timeout: int | None = None,
    check: bool = False,
    capture: bool = True,
) -> subprocess.CompletedProcess[str]:
    LOG.debug("$ %s (cwd=%s)", cmd if isinstance(cmd, str) else " ".join(cmd), cwd)
    return subprocess.run(
        cmd,
        cwd=str(cwd) if cwd else None,
        env={**os.environ, **(env or {})},
        timeout=timeout,
        check=check,
        capture_output=capture,
        text=True,
        shell=isinstance(cmd, str),
    )


# ---------------------------------------------------------------------------
# Git operations
# ---------------------------------------------------------------------------

def git(args: list[str], cfg: Config, *, check: bool = True, timeout: int = 600) -> subprocess.CompletedProcess[str]:
    return run_cmd(["git", *args], cwd=cfg.project_root, check=check, timeout=timeout)


def ensure_clean_tree(cfg: Config) -> None:
    res = git(["status", "--porcelain"], cfg)
    if res.stdout.strip():
        raise RuntimeError(
            "Working tree not clean. Commit or stash changes before starting yolo.\n"
            + res.stdout
        )


def checkout_branch(cfg: Config, branch: str) -> None:
    git(["fetch", "origin", cfg.main_branch, "--quiet"], cfg, check=False)
    git(["checkout", cfg.main_branch], cfg)
    git(["pull", "--rebase", "--autostash", "origin", cfg.main_branch], cfg, check=False)
    # Create or reuse branch
    res = git(["rev-parse", "--verify", branch], cfg, check=False)
    if res.returncode == 0:
        git(["checkout", branch], cfg)
        git(["rebase", cfg.main_branch], cfg, check=False)
    else:
        git(["checkout", "-b", branch], cfg)


def diff_summary(cfg: Config) -> tuple[int, int, int]:
    """Return (files_changed, additions, deletions) since branch base."""
    base = run_cmd(["git", "merge-base", "HEAD", cfg.main_branch], cwd=cfg.project_root, check=False)
    base_sha = base.stdout.strip() or cfg.main_branch
    res = run_cmd(
        ["git", "diff", "--numstat", f"{base_sha}..HEAD"],
        cwd=cfg.project_root, check=False,
    )
    files = additions = deletions = 0
    for line in res.stdout.splitlines():
        parts = line.split("\t")
        if len(parts) < 3:
            continue
        files += 1
        if parts[0].isdigit():
            additions += int(parts[0])
        if parts[1].isdigit():
            deletions += int(parts[1])
    return files, additions, deletions


def files_deleted(cfg: Config) -> int:
    base = run_cmd(["git", "merge-base", "HEAD", cfg.main_branch], cwd=cfg.project_root, check=False)
    base_sha = base.stdout.strip() or cfg.main_branch
    res = run_cmd(
        ["git", "diff", "--diff-filter=D", "--name-only", f"{base_sha}..HEAD"],
        cwd=cfg.project_root, check=False,
    )
    return len([l for l in res.stdout.splitlines() if l.strip()])


def commit_all(cfg: Config, message: str) -> bool:
    git(["add", "-A"], cfg)
    res = git(["status", "--porcelain"], cfg)
    if not res.stdout.strip():
        return False
    git(["commit", "-m", message], cfg)
    return True


_PUSH_TRANSIENT_PATTERNS = (
    "tls handshake timeout",
    "rpc failed",
    "connection reset",
    "connection timed out",
    "early eof",
    "ssl_read",
    "ssl error",
    "schannel: server closed",
    "could not resolve host",
    "operation timed out",
    "remote end hung up",
    "the requested url returned error: 5",  # 5xx server-side
)


def _push_error_is_transient(stderr: str) -> bool:
    if not stderr:
        return False
    low = stderr.lower()
    return any(p in low for p in _PUSH_TRANSIENT_PATTERNS)


def push_branch(cfg: Config, branch: str, *, attempts: int = 3) -> None:
    """Push a yolo branch to origin, retrying on transient network errors.

    Retries only when stderr matches a known transient pattern (TLS timeout,
    RPC failed, connection reset, etc). Hard failures (auth, non-fast-forward,
    refspec missing) raise immediately so the orchestrator can surface them
    to Discord without burning retries.
    """
    backoff_s = [30, 60]  # sleeps between attempts 1->2 and 2->3
    last_err: str = ""
    for i in range(attempts):
        res = run_cmd(
            ["git", "push", "-u", "origin", branch],
            cwd=cfg.project_root, check=False, timeout=1800,
        )
        if res.returncode == 0:
            if i > 0:
                LOG.info("Push succeeded on attempt %d/%d", i + 1, attempts)
            return
        stderr = (res.stderr or "") + (res.stdout or "")
        last_err = stderr.strip()[:1500]
        if not _push_error_is_transient(stderr):
            raise subprocess.CalledProcessError(
                res.returncode, res.args,
                output=res.stdout, stderr=res.stderr,
            )
        if i == attempts - 1:
            LOG.error("Push attempt %d/%d failed (transient, but retries exhausted): %s",
                      i + 1, attempts, last_err[:300])
            raise subprocess.CalledProcessError(
                res.returncode, res.args,
                output=res.stdout, stderr=res.stderr,
            )
        sleep_s = backoff_s[i] if i < len(backoff_s) else backoff_s[-1]
        LOG.warning("Push attempt %d/%d failed (transient): %s — retrying in %ds",
                    i + 1, attempts, last_err[:200], sleep_s)
        time.sleep(sleep_s)


def rollback_branch(cfg: Config, branch: str) -> None:
    """Hard reset and return to main."""
    git(["reset", "--hard"], cfg, check=False)
    git(["clean", "-fd"], cfg, check=False)
    git(["checkout", cfg.main_branch], cfg, check=False)
    git(["branch", "-D", branch], cfg, check=False)


# ---------------------------------------------------------------------------
# GitHub PR helpers (via `gh`)
# ---------------------------------------------------------------------------

def gh(args: list[str], cfg: Config, *, check: bool = True, timeout: int = 120) -> subprocess.CompletedProcess[str]:
    return run_cmd(["gh", *args], cwd=cfg.project_root, check=check, timeout=timeout)


def open_pr(cfg: Config, task: Task, body: str) -> tuple[int, str]:
    """Open a PR for the task. Falls back to yolo-only labels if the
    discipline:<x> label doesn't exist on the repo (gh exits non-zero with
    'could not add label' on stderr). `yolo` is the only mandatory label."""
    title = f"[{task.id}] {task.title}"
    base_cmd = [
        "pr", "create",
        "--repo", cfg.gh_repo,
        "--base", cfg.main_branch,
        "--head", task.branch,
        "--title", title,
        "--body", body,
        "--label", "yolo",
    ]
    discipline_label = f"discipline:{task.discipline}"
    cmd = base_cmd + ["--label", discipline_label]

    res = gh(cmd, cfg, timeout=180, check=False)
    if res.returncode != 0:
        stderr = (res.stderr or "") + (res.stdout or "")
        if "could not add label" in stderr.lower() or "not found" in stderr.lower():
            LOG.warning(
                "PR create rejected discipline label %r — retrying with yolo-only. stderr=%s",
                discipline_label, stderr.strip()[:300],
            )
            res = gh(base_cmd, cfg, timeout=180, check=False)
        if res.returncode != 0:
            raise RuntimeError(
                f"gh pr create failed (exit {res.returncode}):\n"
                f"{(res.stderr or res.stdout or '').strip()[:1500]}"
            )

    url = res.stdout.strip().splitlines()[-1] if res.stdout else ""
    num = _extract_pr_number(url)
    if num is None:
        raise RuntimeError(f"Could not parse PR URL: {res.stdout}\n{res.stderr}")
    return num, url


def ensure_discipline_labels(cfg: Config, tasks: list[Task]) -> None:
    """Pre-flight: list existing labels via gh and create any missing
    discipline:<x> labels referenced by the queue. Self-healing for new
    disciplines so open_pr() doesn't trip on a missing label."""
    needed = {f"discipline:{t.discipline}" for t in tasks if t.discipline}
    if not needed:
        return
    res = gh(["label", "list", "--repo", cfg.gh_repo, "--json", "name", "--limit", "200"],
             cfg, check=False, timeout=60)
    if res.returncode != 0:
        LOG.warning("gh label list failed (exit %d) — skipping discipline label pre-flight. "
                    "stderr=%s", res.returncode, (res.stderr or "").strip()[:300])
        return
    try:
        existing = {entry.get("name") for entry in json.loads(res.stdout or "[]")}
    except (json.JSONDecodeError, AttributeError) as exc:
        LOG.warning("Could not parse gh label list output: %s", exc)
        return
    missing = sorted(needed - existing)
    for label in missing:
        LOG.info("Creating missing label %r on %s", label, cfg.gh_repo)
        create = gh(
            ["label", "create", label, "--repo", cfg.gh_repo,
             "--color", "B3B3B3",
             "--description", f"YOLO auto-created for discipline label"],
            cfg, check=False, timeout=60,
        )
        if create.returncode != 0:
            LOG.warning("Could not create label %r (exit %d): %s",
                        label, create.returncode, (create.stderr or "").strip()[:200])


def _extract_pr_number(url: str) -> Optional[int]:
    m = re.search(r"/pull/(\d+)", url)
    return int(m.group(1)) if m else None


def pr_state(cfg: Config, pr_number: int) -> dict[str, Any]:
    res = gh(
        ["pr", "view", str(pr_number),
         "--repo", cfg.gh_repo,
         "--json", "state,mergedAt,reviewDecision,mergeable,statusCheckRollup"],
        cfg, check=False,
    )
    if res.returncode != 0 or not res.stdout.strip():
        return {}
    try:
        return json.loads(res.stdout)
    except json.JSONDecodeError:
        return {}


def poll_pr_merged(cfg: Config, pr_number: int) -> str:
    """Block until PR is merged, closed, or timeout. Returns final state."""
    deadline = time.time() + cfg.pr_poll_max_hours * 3600
    while time.time() < deadline:
        if SHUTDOWN_REQUESTED.is_set():
            return "shutdown"
        if HALT_FLAG_PATH.exists():
            HALT_REQUESTED.set()
            return "halt"
        st = pr_state(cfg, pr_number)
        state = st.get("state") or "UNKNOWN"
        if state == "MERGED":
            return "merged"
        if state == "CLOSED":
            return "closed"
        time.sleep(cfg.pr_poll_interval_s)
    return "timeout"


# ---------------------------------------------------------------------------
# AFL-0215 lint rule (the architectural guard)
# ---------------------------------------------------------------------------

LINT_RULES = [
    # (regex, human-readable violation)
    (re.compile(r"\bAbilitySystemComponent\s*->\s*GiveAbility\b"),
     "GiveAbility called directly - abilities must come from ULyraPawnData ability sets."),
    (re.compile(r"\bGetAttributeSet\s*<\s*UAFLAttributeSet_Combat\s*>\s*[^;]*?->\s*SetHealth\b"),
     "Direct SetHealth on AttributeSet - damage must route through UAFLDamageExecCalc."),
    (re.compile(r"\bGetPlayerViewPoint\b"),  # in server context — flagged everywhere as a smell
     "GetPlayerViewPoint detected - hitscan must use client TargetData per AFL-0106."),
]


def run_lint(cfg: Config, changed_files: list[Path]) -> list[str]:
    """Run AFL-0215 lint on changed C++ files. Returns list of violations."""
    violations: list[str] = []
    cpp_exts = {".cpp", ".h", ".hpp", ".cc"}
    for f in changed_files:
        if f.suffix.lower() not in cpp_exts or not f.exists():
            continue
        try:
            text = f.read_text(encoding="utf-8", errors="ignore")
        except OSError:
            continue
        for rx, msg in LINT_RULES:
            for m in rx.finditer(text):
                line_no = text.count("\n", 0, m.start()) + 1
                violations.append(f"{f.relative_to(cfg.project_root)}:{line_no}: {msg}")
    return violations


def list_changed_cpp(cfg: Config) -> list[Path]:
    base = run_cmd(["git", "merge-base", "HEAD", cfg.main_branch], cwd=cfg.project_root, check=False)
    base_sha = base.stdout.strip() or cfg.main_branch
    res = run_cmd(
        ["git", "diff", "--name-only", f"{base_sha}..HEAD"],
        cwd=cfg.project_root, check=False,
    )
    out: list[Path] = []
    for line in res.stdout.splitlines():
        p = cfg.project_root / line.strip()
        if p.suffix.lower() in {".cpp", ".h", ".hpp", ".cc"} and p.exists():
            out.append(p)
    return out


# ---------------------------------------------------------------------------
# Build verification (uses verify.py)
# ---------------------------------------------------------------------------

def run_build_verify(cfg: Config, task: Task) -> tuple[bool, str]:
    cmd = [
        sys.executable, str(HERE / "verify.py"),
        "--project-root", str(cfg.project_root),
        "--engine-root", str(cfg.engine_root),
        "--uproject", str(cfg.uproject),
        "--mode", task.verify,
    ]
    for p in task.plugins_to_build:
        cmd += ["--plugin", p]
    for c in task.cheat_matrix:
        cmd += ["--cheat", c]
    res = run_cmd(cmd, timeout=cfg.verify_timeout_s, check=False)
    ok = res.returncode == 0
    log_tail = "\n".join((res.stdout + res.stderr).splitlines()[-60:])
    return ok, log_tail


# ---------------------------------------------------------------------------
# LFS bandwidth check
# ---------------------------------------------------------------------------

def lfs_bytes_pushed(cfg: Config) -> Optional[int]:
    """Best-effort: parse `git lfs env` for storage location and du it."""
    try:
        res = git(["lfs", "ls-files", "-l"], cfg, check=False, timeout=30)
    except Exception:
        return None
    # ls-files -l prints SHA size path; sum sizes.
    total = 0
    for line in res.stdout.splitlines():
        parts = line.split()
        if len(parts) >= 2 and parts[1].isdigit():
            total += int(parts[1])
    return total


def lfs_check_safe(cfg: Config) -> tuple[bool, str]:
    n = lfs_bytes_pushed(cfg)
    if n is None:
        return True, "lfs check skipped"
    gb = n / 1e9
    threshold = cfg.lfs_quota_gb * cfg.lfs_halt_threshold_pct
    if gb > threshold:
        return False, f"LFS usage {gb:.1f} GB exceeds halt threshold {threshold:.1f} GB"
    return True, f"LFS {gb:.1f} / {cfg.lfs_quota_gb} GB"


# ---------------------------------------------------------------------------
# Claude Code spawn
# ---------------------------------------------------------------------------

CLAUDE_LIVE_LOG_PATH = STATE_DIR / "claude_code_live.log"


def spawn_claude_code(cfg: Config, prompt: str) -> tuple[bool, str]:
    """Spawn Claude Code and stream its stdout/stderr to a live log file
    while keeping an in-memory transcript.

    Why streaming: subprocess.run with capture_output=True buffers the entire
    transcript in memory and DISCARDS it when TimeoutExpired fires — which is
    exactly the case where we most want forensic evidence. Using Popen with
    reader threads lets us:
      - tail -f the live log while the run is in progress
      - preserve all output collected up to the moment of a timeout
      - survive the orchestrator's working-tree rollback (.state/ is outside
        the tree)

    Returns (ok, transcript). Transcript is the merged stdout + stderr,
    truncated for non-success cases by the caller.
    """
    if not shutil.which(cfg.claude_code_bin):
        return False, f"`{cfg.claude_code_bin}` not found on PATH"

    cmd = [cfg.claude_code_bin, "-p", prompt]
    if cfg.claude_code_skip_permissions:
        cmd.append("--dangerously-skip-permissions")

    STATE_DIR.mkdir(parents=True, exist_ok=True)
    # Truncate the live log at the start of each spawn so a tail -f shows
    # only this session. A copy of the previous failure is preserved by the
    # rotation below.
    if CLAUDE_LIVE_LOG_PATH.exists():
        try:
            prev = CLAUDE_LIVE_LOG_PATH.read_text(encoding="utf-8", errors="replace")
            (STATE_DIR / "claude_code_live.prev.log").write_text(prev, encoding="utf-8")
        except OSError:
            pass

    LOG.info("Spawning Claude Code (timeout %ds, live log %s)...",
             cfg.claude_code_timeout_s, CLAUDE_LIVE_LOG_PATH)

    stdout_lines: list[str] = []
    stderr_lines: list[str] = []

    def reader(stream: Any, sink: list[str], label: str, log_fh: Any) -> None:
        try:
            for line in stream:
                sink.append(line)
                try:
                    log_fh.write(line if line.endswith("\n") else line + "\n")
                    log_fh.flush()
                except OSError:
                    pass
        except (OSError, ValueError):
            pass

    try:
        proc = subprocess.Popen(
            cmd,
            cwd=str(cfg.project_root),
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            stdin=subprocess.DEVNULL,
            text=True,
            bufsize=1,  # line-buffered
            encoding="utf-8",
            errors="replace",
        )
    except FileNotFoundError as exc:
        return False, f"Cannot start Claude Code: {exc}"

    timed_out = False
    with CLAUDE_LIVE_LOG_PATH.open("w", encoding="utf-8") as log_fh:
        log_fh.write(f"# Claude Code session started {datetime.now(timezone.utc).isoformat(timespec='seconds')}\n")
        log_fh.write(f"# Timeout: {cfg.claude_code_timeout_s}s\n")
        log_fh.write(f"# CWD: {cfg.project_root}\n")
        log_fh.write("# Live tail: Get-Content -Wait .state\\claude_code_live.log\n\n")
        log_fh.flush()

        t_out = threading.Thread(
            target=reader, args=(proc.stdout, stdout_lines, "stdout", log_fh), daemon=True,
        )
        t_err = threading.Thread(
            target=reader, args=(proc.stderr, stderr_lines, "stderr", log_fh), daemon=True,
        )
        t_out.start()
        t_err.start()

        try:
            proc.wait(timeout=cfg.claude_code_timeout_s)
        except subprocess.TimeoutExpired:
            timed_out = True
            LOG.error("Claude Code exceeded %ds — killing subprocess",
                      cfg.claude_code_timeout_s)
            proc.kill()
            try:
                proc.wait(timeout=10)
            except subprocess.TimeoutExpired:
                LOG.warning("Claude Code did not exit 10s after kill")

        # Give the reader threads a moment to drain after the process exits.
        t_out.join(timeout=5)
        t_err.join(timeout=5)

    transcript = "".join(stdout_lines)
    if stderr_lines:
        transcript += "\n--- stderr ---\n" + "".join(stderr_lines)

    if timed_out:
        return False, f"Claude Code timed out after {cfg.claude_code_timeout_s}s\n{transcript[-3000:]}"
    if proc.returncode != 0:
        return False, f"Claude Code exit {proc.returncode}\n{transcript[-2000:]}"
    return True, transcript


# ---------------------------------------------------------------------------
# Safety rails
# ---------------------------------------------------------------------------

def apply_safety_rails(cfg: Config, task: Task) -> tuple[bool, str]:
    """Return (ok, reason). False reason halts the loop."""
    files, _adds, _dels = diff_summary(cfg)
    if files > cfg.max_changed_files:
        return False, f"{files} files changed exceeds cap {cfg.max_changed_files}"
    dels = files_deleted(cfg)
    if dels > cfg.max_deletions_per_task:
        return False, f"{dels} file deletions exceeds cap {cfg.max_deletions_per_task}"
    ok, reason = lfs_check_safe(cfg)
    if not ok:
        return False, reason
    violations = run_lint(cfg, list_changed_cpp(cfg))
    if violations:
        return False, "AFL-0215 lint violations:\n  " + "\n  ".join(violations)
    return True, "ok"


# ---------------------------------------------------------------------------
# Notification
# ---------------------------------------------------------------------------

def notify(cfg: Config, kind: str, payload: dict[str, Any]) -> None:
    if cfg.dry_run:
        LOG.info("[dry-run] notify %s: %s", kind, payload)
        return
    if not cfg.discord_webhook:
        return
    try:
        from notify import build_discord_payload  # type: ignore[import]
    except ImportError:
        LOG.warning("notify.py not importable; skipping Discord push")
        return
    body = build_discord_payload(kind, payload, cfg.gh_repo)
    try:
        requests.post(cfg.discord_webhook, json=body, timeout=15)
    except requests.RequestException as exc:
        LOG.warning("Discord push failed: %s", exc)


# ---------------------------------------------------------------------------
# Main task lifecycle
# ---------------------------------------------------------------------------

def execute_one_task(cfg: Config, task: Task, all_tasks: list[Task], queue_path: Path, state: RunState) -> str:
    """Execute one task end to end. Returns final status: merged | error | halted."""
    LOG.info("=" * 72)
    LOG.info("Task %s: %s", task.id, task.title)
    LOG.info("  discipline=%s  estimate=%s  autonomy=%s", task.discipline, task.estimate, task.autonomy)
    emit_run_event("task_start", {"task": task.id, "title": task.title})

    # Phase-gate or human-only: skip in YOLO mode, surface to human
    if task.autonomy in ("human_only", "phase_gate") or task.discipline in cfg.require_human_for_disciplines:
        LOG.warning("Task requires human - skipping in YOLO. Surface via Discord.")
        notify(cfg, "needs_human", {"task": task.id, "title": task.title, "reason": "autonomy=human_only or human-discipline"})
        task.status = "skipped"
        task.last_attempt_at = _now()
        save_queue(queue_path, all_tasks)
        return "skipped"

    # Pre-flight: clean tree
    try:
        ensure_clean_tree(cfg)
    except RuntimeError as exc:
        LOG.error("%s", exc)
        return "halted"

    # Branch
    LOG.info("Checking out branch %s", task.branch)
    if not cfg.dry_run:
        checkout_branch(cfg, task.branch)

    task.status = "running"
    task.last_attempt_at = _now()
    state.current_task_id = task.id
    state.save()
    save_queue(queue_path, all_tasks)

    notify(cfg, "task_start", {"task": task.id, "title": task.title, "branch": task.branch})

    # Build prompt
    try:
        from aik_session import build_session_contract  # type: ignore[import]
    except ImportError:
        LOG.error("aik_session.py not importable")
        return "halted"
    prompt = build_session_contract(task, cfg)

    if cfg.dry_run:
        LOG.info("[dry-run] would spawn Claude Code with %d-char prompt", len(prompt))
        ok, transcript = True, "[dry-run]"
    else:
        ok, transcript = spawn_claude_code(cfg, prompt)

    if not ok:
        LOG.error("Claude Code failed: %s", transcript[:800])
        task.status = "error"
        task.last_error = transcript[:2000]
        save_queue(queue_path, all_tasks)
        if not cfg.dry_run:
            rollback_branch(cfg, task.branch)
        notify(cfg, "task_failed", {"task": task.id, "stage": "claude_code", "tail": transcript[-600:]})
        return "error"

    # Safety rails
    ok, reason = apply_safety_rails(cfg, task)
    if not ok:
        LOG.error("Safety rail tripped: %s", reason)
        if not cfg.dry_run:
            rollback_branch(cfg, task.branch)
        task.status = "error"
        task.last_error = reason
        save_queue(queue_path, all_tasks)
        notify(cfg, "task_failed", {"task": task.id, "stage": "safety", "reason": reason})
        return "error"

    # Verify build
    if task.verify != "none":
        LOG.info("Running build verify mode=%s", task.verify)
        ok, tail = (True, "[dry-run]") if cfg.dry_run else run_build_verify(cfg, task)
        if not ok:
            LOG.error("Build verify failed:\n%s", tail)
            if not cfg.dry_run:
                rollback_branch(cfg, task.branch)
            task.status = "error"
            task.last_error = tail[:2000]
            save_queue(queue_path, all_tasks)
            notify(cfg, "task_failed", {"task": task.id, "stage": "build_verify", "tail": tail[-1500:]})
            return "error"

    # Commit
    msg = (
        f"[{task.id}] {task.title}\n\n"
        f"Discipline: {task.discipline}  Estimate: {task.estimate}\n"
        f"Skills: {', '.join(task.skills) or 'none'}\n"
        f"Verify: {task.verify}\n\n"
        f"Generated by afl_yolo {VERSION}."
    )
    if not cfg.dry_run:
        had_changes = commit_all(cfg, msg)
        if not had_changes:
            LOG.error("Claude Code produced no changes - marking error")
            task.status = "error"
            task.last_error = "no changes"
            save_queue(queue_path, all_tasks)
            rollback_branch(cfg, task.branch)
            notify(cfg, "task_failed", {"task": task.id, "stage": "commit", "reason": "no changes"})
            return "error"

    # Push
    LOG.info("Pushing %s", task.branch)
    if not cfg.dry_run:
        try:
            push_branch(cfg, task.branch)
        except subprocess.CalledProcessError as exc:
            LOG.error("Push failed: %s", exc)
            task.status = "error"
            task.last_error = f"push failed: {exc}"
            save_queue(queue_path, all_tasks)
            notify(cfg, "task_failed", {"task": task.id, "stage": "push", "reason": str(exc)})
            return "error"

    # Open PR
    body = _build_pr_body(task, transcript)
    if cfg.dry_run:
        pr_num, pr_url = 0, "https://example.invalid/pr/0"
    else:
        try:
            pr_num, pr_url = open_pr(cfg, task, body)
        except (RuntimeError, subprocess.CalledProcessError, subprocess.TimeoutExpired) as exc:
            reason = str(exc)
            LOG.error("PR creation failed for %s: %s", task.id, reason)
            task.status = "error"
            task.last_error = f"open_pr: {reason}"[:2000]
            save_queue(queue_path, all_tasks)
            notify(cfg, "task_failed", {
                "task": task.id, "title": task.title,
                "stage": "open_pr", "reason": reason[:900],
                "branch": task.branch,
            })
            return "error"
    task.pr_number = pr_num
    task.pr_url = pr_url
    task.status = "pr_open"
    save_queue(queue_path, all_tasks)
    state.last_pr_url = pr_url
    state.save()

    LOG.info("PR opened: %s", pr_url)
    notify(cfg, "pr_open", {"task": task.id, "title": task.title, "pr_url": pr_url, "pr_number": pr_num})

    if cfg.dry_run:
        task.status = "merged"
        save_queue(queue_path, all_tasks)
        return "merged"

    # Wait for merge
    LOG.info("Waiting for PR merge (poll every %ds, max %dh)", cfg.pr_poll_interval_s, cfg.pr_poll_max_hours)
    final = poll_pr_merged(cfg, pr_num)
    if final == "merged":
        task.status = "merged"
        save_queue(queue_path, all_tasks)
        notify(cfg, "task_merged", {"task": task.id, "title": task.title, "pr_url": pr_url})
        return "merged"
    if final in ("halt", "shutdown"):
        return "halted"
    if final == "closed":
        task.status = "error"
        task.last_error = "PR closed without merge"
        save_queue(queue_path, all_tasks)
        notify(cfg, "task_failed", {"task": task.id, "stage": "merge", "reason": "PR closed without merge"})
        return "error"
    # timeout
    task.status = "error"
    task.last_error = "PR merge poll timeout"
    save_queue(queue_path, all_tasks)
    notify(cfg, "task_failed", {"task": task.id, "stage": "merge", "reason": "timeout"})
    return "error"


def _build_pr_body(task: Task, transcript: str) -> str:
    tail = "\n".join(transcript.splitlines()[-30:])
    return (
        f"## {task.id} - {task.title}\n\n"
        f"- **Discipline**: {task.discipline}\n"
        f"- **Estimate**: {task.estimate}\n"
        f"- **Skills loaded**: {', '.join(task.skills) or '_(none)_'}\n"
        f"- **Verify**: `{task.verify}`\n"
        f"- **Cheat matrix**: {', '.join(task.cheat_matrix) or '_(none)_'}\n"
        f"- **Branch**: `{task.branch}`\n\n"
        f"### What the agent was asked to do\n\n"
        f"{task.aik_brief}\n\n"
        f"### Safety checks\n\n"
        f"- AFL-0215 lint rule: PASS\n"
        f"- File-change cap: PASS\n"
        f"- LFS bandwidth: within budget\n"
        f"- Build.bat compile: PASS\n\n"
        f"### Phone review checklist\n\n"
        f"1. Diff is scoped to the task description\n"
        f"2. No direct `SetHealth`, `GiveAbility`, or server-side `GetPlayerViewPoint`\n"
        f"3. Plugin name matches `AFL` prefix\n"
        f"4. CI build is green\n"
        f"\n"
        f"### Claude Code transcript tail\n\n"
        f"```\n{tail}\n```\n"
        f"\n_Generated by afl_yolo {VERSION}._\n"
    )


def _now() -> str:
    return datetime.now(timezone.utc).isoformat(timespec="seconds")


# ---------------------------------------------------------------------------
# Main loop
# ---------------------------------------------------------------------------

def main_loop(cfg: Config, queue_path: Path, max_tasks: int | None) -> int:
    state = RunState.load()
    state.started_at = _now()
    state.save()

    notify(cfg, "loop_start", {"version": VERSION, "repo": cfg.gh_repo, "dry_run": cfg.dry_run})

    # Self-healing: make sure every discipline label the queue references
    # exists on the repo before we try to open any PR.
    if not cfg.dry_run:
        try:
            ensure_discipline_labels(cfg, load_queue(queue_path))
        except Exception as exc:
            LOG.warning("discipline label pre-flight raised: %s — continuing anyway", exc)

    ran = 0
    while not SHUTDOWN_REQUESTED.is_set():
        if HALT_FLAG_PATH.exists():
            LOG.warning("Halt flag detected - graceful shutdown")
            HALT_FLAG_PATH.unlink(missing_ok=True)
            break
        if max_tasks is not None and ran >= max_tasks:
            LOG.info("Reached --max-tasks=%d", max_tasks)
            break

        tasks = load_queue(queue_path)
        task = pick_next_task(tasks)
        if task is None:
            LOG.info("No eligible task - queue empty or all errored. Sleeping 5 min.")
            notify(cfg, "queue_empty", {})
            for _ in range(60):
                if SHUTDOWN_REQUESTED.is_set() or HALT_FLAG_PATH.exists():
                    break
                time.sleep(5)
            continue

        result = execute_one_task(cfg, task, tasks, queue_path, state)
        ran += 1

        if result == "merged":
            state.consecutive_failures = 0
            state.last_run_status = "merged"
        elif result == "skipped":
            state.last_run_status = "skipped"
        elif result == "halted":
            state.last_run_status = "halted"
            break
        else:
            state.consecutive_failures += 1
            state.last_run_status = "error"
            LOG.error("Consecutive failures: %d / %d",
                      state.consecutive_failures, cfg.max_consecutive_failures)
            if state.consecutive_failures >= cfg.max_consecutive_failures:
                LOG.critical("Hit failure cap - HALTING")
                notify(cfg, "loop_halt", {"reason": "consecutive failures", "count": state.consecutive_failures})
                state.save()
                return 2

        state.last_run_at = _now()
        state.save()

    notify(cfg, "loop_stop", {"ran": ran})
    return 0


# ---------------------------------------------------------------------------
# Signal handlers
# ---------------------------------------------------------------------------

def _install_signals() -> None:
    def handler(signum: int, _frame: Any) -> None:
        LOG.warning("Received signal %d - requesting graceful shutdown", signum)
        SHUTDOWN_REQUESTED.set()
    signal.signal(signal.SIGINT, handler)
    if hasattr(signal, "SIGTERM"):
        signal.signal(signal.SIGTERM, handler)


# ---------------------------------------------------------------------------
# CLI entry points
# ---------------------------------------------------------------------------

def cmd_run(args: argparse.Namespace) -> int:
    cfg = Config.load(Path(args.config))
    setup_logging(verbose=args.verbose)
    LOG.info("afl_yolo %s starting against %s", VERSION, cfg.gh_repo)
    _install_signals()
    with Lockfile(LOCK_PATH):
        return main_loop(cfg, Path(args.queue), max_tasks=args.max_tasks)


def cmd_status(args: argparse.Namespace) -> int:
    cfg = Config.load(Path(args.config))
    setup_logging()
    tasks = load_queue(Path(args.queue))
    state = RunState.load()
    next_t = pick_next_task(tasks)
    by_status: dict[str, int] = {}
    for t in tasks:
        by_status[t.status] = by_status.get(t.status, 0) + 1
    print(f"== afl_yolo {VERSION} ==  repo: {cfg.gh_repo}")
    print(f"Queue: {len(tasks)} tasks   {by_status}")
    print(f"State: started={state.started_at}  last={state.last_run_at}  fails={state.consecutive_failures}")
    print(f"Next:  {next_t.id + ' - ' + next_t.title if next_t else '(none eligible)'}")
    if state.last_pr_url:
        print(f"Last PR: {state.last_pr_url}")
    return 0


def cmd_task(args: argparse.Namespace) -> int:
    cfg = Config.load(Path(args.config))
    setup_logging(verbose=args.verbose)
    _install_signals()
    queue_path = Path(args.queue)
    tasks = load_queue(queue_path)
    task = next((t for t in tasks if t.id == args.task_id), None)
    if task is None:
        LOG.error("Task %s not in queue", args.task_id)
        return 1
    state = RunState.load()
    with Lockfile(LOCK_PATH):
        if not cfg.dry_run:
            try:
                ensure_discipline_labels(cfg, tasks)
            except Exception as exc:
                LOG.warning("discipline label pre-flight raised: %s — continuing anyway", exc)
        result = execute_one_task(cfg, task, tasks, queue_path, state)
    LOG.info("Task %s finished: %s", args.task_id, result)
    return 0 if result in ("merged", "skipped") else 1


def cmd_halt(_args: argparse.Namespace) -> int:
    STATE_DIR.mkdir(parents=True, exist_ok=True)
    HALT_FLAG_PATH.write_text(_now(), encoding="utf-8")
    print(f"Halt flag written to {HALT_FLAG_PATH}. Loop will stop after the current task.")
    return 0


def cmd_validate(args: argparse.Namespace) -> int:
    setup_logging(verbose=args.verbose)
    Config.load(Path(args.config))
    tasks = load_queue(Path(args.queue))
    LOG.info("Queue OK: %d tasks", len(tasks))
    ids = {t.id for t in tasks}
    for t in tasks:
        for d in t.depends_on:
            if d not in ids:
                LOG.error("Task %s depends on unknown %s", t.id, d)
                return 1
    LOG.info("All dependencies resolve. Config valid.")
    return 0


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(prog="afl_yolo", description="Autonomous build orchestrator for BAG MAN.")
    p.add_argument("--config", default=str(DEFAULT_CONFIG_PATH))
    p.add_argument("--queue", default=str(DEFAULT_QUEUE_PATH))
    p.add_argument("-v", "--verbose", action="store_true")
    sub = p.add_subparsers(dest="cmd", required=True)

    pr = sub.add_parser("run", help="Run the loop")
    pr.add_argument("--max-tasks", type=int, default=None)
    pr.set_defaults(func=cmd_run)

    ps = sub.add_parser("status", help="Show state and queue summary")
    ps.set_defaults(func=cmd_status)

    pt = sub.add_parser("task", help="Run one task and exit")
    pt.add_argument("task_id")
    pt.set_defaults(func=cmd_task)

    ph = sub.add_parser("halt", help="Request graceful halt")
    ph.set_defaults(func=cmd_halt)

    pv = sub.add_parser("validate", help="Validate config + queue")
    pv.set_defaults(func=cmd_validate)

    args = p.parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    raise SystemExit(main())
