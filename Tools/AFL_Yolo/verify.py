#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
verify.py - Build + acceptance verification for afl_yolo tasks.

Verification modes:
  compile               Build.bat LyraEditor compiles cleanly for each --plugin
  compile+test          compile, then run the project's automation tests via UAT
  compile+cheat-matrix  compile, then launch headless editor and run a sequence
                        of console commands (the AFL cheat matrix from master
                        doc Sec. 8.3 T1-T6). Each cheat is expected to log a
                        success token; any missing token fails the build.

Exit codes:
  0   verification passed
  1   compile failure
  2   automation test failure
  3   cheat-matrix expectation missed
  4   environment / tool invocation failure
"""

from __future__ import annotations

import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile
import time
from pathlib import Path


SUCCESS_TOKEN_RE = re.compile(r"\bAFLCombatCheats\s*:\s*OK\b", re.IGNORECASE)
COMPILE_FAILED_TOKENS = [
    "BUILD FAILED",
    "ERROR : ",
    "fatal error",
    "LogInit: Error",
]

# UBT named-mutex collisions look like this in stdout/stderr. They happen when
# Claude Code runs Build.bat in-session and the orchestrator's verify.py
# kicks off Build.bat seconds later — the previous UBT process's named mutex
# is still owned by a different Windows session, so we get
# UnauthorizedAccessException acquiring the global mutex. The first invocation
# usually drops the mutex within 10-30s of exiting, so a single retry after
# a short sleep recovers. We do NOT treat this as a code compile failure.
UBT_MUTEX_COLLISION_TOKENS = [
    "UnauthorizedAccessException",
    "UnrealBuildTool_Mutex",
    "Access to the path",
]
UBT_MUTEX_RETRY_SLEEP_S = 30
TEST_FAILED_TOKENS = [
    "Test Failed",
    "Tests Failed",
    "FAIL ",
]


def stream_subprocess(cmd: list[str], cwd: Path, timeout: int, log_path: Path) -> int:
    """Run cmd, stream to console and a logfile, return exit code."""
    log_path.parent.mkdir(parents=True, exist_ok=True)
    print(f"[verify] $ {' '.join(cmd)}", flush=True)
    start = time.time()
    with log_path.open("w", encoding="utf-8", errors="replace") as logf:
        proc = subprocess.Popen(
            cmd,
            cwd=str(cwd),
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
        )
        try:
            assert proc.stdout is not None
            for line in proc.stdout:
                sys.stdout.write(line)
                logf.write(line)
                if time.time() - start > timeout:
                    proc.kill()
                    print(f"[verify] TIMEOUT after {timeout}s", flush=True)
                    return 124
            proc.wait(timeout=30)
        except KeyboardInterrupt:
            proc.kill()
            raise
    return proc.returncode if proc.returncode is not None else 1


def detect_compile_failure(log_path: Path) -> tuple[bool, str]:
    if not log_path.exists():
        return False, ""
    text = log_path.read_text(encoding="utf-8", errors="ignore")
    for token in COMPILE_FAILED_TOKENS:
        if token in text:
            # Extract a window around the first hit
            i = text.index(token)
            window = text[max(0, i - 400): i + 400]
            return True, window
    return False, ""


def detect_ubt_mutex_collision(log_path: Path) -> bool:
    """Return True if the build log shows the UBT global-mutex collision
    pattern (a leftover Build.bat from a Claude in-session compile still
    holding the named mutex). This is a transient orchestration race, NOT
    a code compile failure — retry is the correct response."""
    if not log_path.exists():
        return False
    text = log_path.read_text(encoding="utf-8", errors="ignore")
    return all(token in text for token in UBT_MUTEX_COLLISION_TOKENS)


def build_plugin(engine_root: Path, uproject: Path, plugin: str, timeout: int, work: Path) -> int:
    """Build LyraEditor with -Plugin=<plugin.uplugin> on Win64 Development.

    This matches the AFL canonical pattern recorded against AFL-0101/0103 in
    the master doc: Build.bat LyraEditor -Plugin=<.uplugin>.
    """
    build_bat = engine_root / "Engine" / "Build" / "BatchFiles" / "Build.bat"
    if not build_bat.exists():
        print(f"[verify] FATAL: {build_bat} not found", file=sys.stderr)
        return 4

    plugin_path = _resolve_plugin(uproject, plugin)
    if plugin_path is None:
        print(f"[verify] FATAL: plugin {plugin} not found under {uproject.parent}", file=sys.stderr)
        return 4

    cmd = [
        str(build_bat),
        "LyraEditor",
        "Win64",
        "Development",
        str(uproject),
        f"-Plugin={plugin_path}",
        "-waitmutex",
        "-NoHotReloadFromIDE",
    ]
    log = work / f"build_{plugin}.log"
    rc = stream_subprocess(cmd, uproject.parent, timeout=timeout, log_path=log)

    # Retry once on the UBT mutex-collision race. This trips when Claude Code
    # ran Build.bat in-session during its authoring loop, then we kicked off
    # Build.bat again here ~5s after the session ended — the prior UBT
    # process's Global\UnrealBuildTool_Mutex_<hash> hasn't been released yet
    # in a way that the second process can acquire (named-mutex ownership is
    # per-creating-session on Windows). The prior process drops the mutex
    # within 10-30s of exiting; one retry after a short sleep recovers.
    # We do this regardless of rc != 0 because UBT can exit non-zero from
    # the collision even before getting to a meaningful compile step.
    if rc != 0 and detect_ubt_mutex_collision(log):
        print(
            f"[verify] UBT mutex collision detected (leftover from a prior Build.bat); "
            f"sleeping {UBT_MUTEX_RETRY_SLEEP_S}s and retrying once.",
            flush=True,
        )
        time.sleep(UBT_MUTEX_RETRY_SLEEP_S)
        log = work / f"build_{plugin}_retry.log"
        rc = stream_subprocess(cmd, uproject.parent, timeout=timeout, log_path=log)
        if rc == 0 and not detect_compile_failure(log)[0]:
            print(f"[verify] {plugin} compiled OK on retry", flush=True)
            return 0
        # Fall through to standard failure handling.

    if rc != 0:
        return 1
    failed, window = detect_compile_failure(log)
    if failed:
        print(f"[verify] Compile log contains failure marker:\n{window}", file=sys.stderr)
        return 1
    print(f"[verify] {plugin} compiled OK", flush=True)
    return 0


def _resolve_plugin(uproject: Path, plugin: str) -> Path | None:
    """Find a plugin file by name, searching Plugins/ recursively."""
    if plugin.endswith(".uplugin"):
        candidate = Path(plugin)
        if candidate.exists():
            return candidate.resolve()
        base = plugin.rsplit(".", 1)[0]
    else:
        base = plugin
    for p in uproject.parent.rglob(f"{base}.uplugin"):
        return p
    return None


def run_automation_tests(engine_root: Path, uproject: Path, suite: str, timeout: int, work: Path) -> int:
    """Run UE automation tests via UnrealEditor-Cmd."""
    editor_cmd = engine_root / "Engine" / "Binaries" / "Win64" / "UnrealEditor-Cmd.exe"
    if not editor_cmd.exists():
        print(f"[verify] FATAL: {editor_cmd} not found", file=sys.stderr)
        return 4
    cmd = [
        str(editor_cmd),
        str(uproject),
        "-ExecCmds=Automation RunTests " + suite + "; Quit",
        "-TestExit=Automation Test Queue Empty",
        "-ReportOutputPath=" + str(work / "AutomationReport"),
        "-nullrhi",
        "-unattended",
        "-nopause",
        "-NoSplash",
        "-stdout",
        "-FullStdOutLogOutput",
    ]
    log = work / "automation.log"
    rc = stream_subprocess(cmd, uproject.parent, timeout=timeout, log_path=log)
    if rc != 0:
        return 2
    text = log.read_text(encoding="utf-8", errors="ignore") if log.exists() else ""
    for token in TEST_FAILED_TOKENS:
        if token in text:
            return 2
    return 0


def run_cheat_matrix(engine_root: Path, uproject: Path, cheats: list[str], timeout: int, work: Path) -> int:
    """Run a sequence of console commands headlessly and check for success tokens.

    The expectation is that the AFL cheat manager (UAFLCombatCheats from Sprint 1
    Block A) logs `AFLCombatCheats: OK <CheatName>` when each cheat completes
    successfully. We require one success token per cheat.
    """
    if not cheats:
        return 0
    editor_cmd = engine_root / "Engine" / "Binaries" / "Win64" / "UnrealEditor-Cmd.exe"
    if not editor_cmd.exists():
        print(f"[verify] FATAL: {editor_cmd} not found", file=sys.stderr)
        return 4

    # Build the ExecCmds string. UE's ParseExecCmds (Engine/Source/Runtime/
    # Engine/Private/ParseExecCommands.cpp) splits on commas; semicolons stay
    # part of the command string and the console treats everything after the
    # first verb as a single argument list, so only the first cheat would run.
    sep = ", "
    sequence = sep.join(cheats) + sep + "Quit"
    cmd = [
        str(editor_cmd),
        str(uproject),
        "-ExecCmds=" + sequence,
        "-game",          # run the standalone game module rather than the editor UI
        "-nullrhi",
        "-unattended",
        "-nopause",
        "-NoSplash",
        "-stdout",
        "-FullStdOutLogOutput",
    ]
    log = work / "cheat_matrix.log"
    rc = stream_subprocess(cmd, uproject.parent, timeout=timeout, log_path=log)
    if rc not in (0, 124):
        return 4 if rc == 4 else 3
    text = log.read_text(encoding="utf-8", errors="ignore") if log.exists() else ""
    ok_count = len(SUCCESS_TOKEN_RE.findall(text))
    if ok_count < len(cheats):
        print(
            f"[verify] cheat matrix: expected {len(cheats)} OK tokens, got {ok_count}. "
            "Confirm UAFLCombatCheats logs `AFLCombatCheats: OK <Name>` per cheat.",
            file=sys.stderr,
        )
        return 3
    print(f"[verify] cheat matrix passed ({ok_count}/{len(cheats)})", flush=True)
    return 0


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(description="afl_yolo build/test/cheat-matrix verifier")
    p.add_argument("--project-root", required=True, type=Path)
    p.add_argument("--engine-root", required=True, type=Path)
    p.add_argument("--uproject", required=True, type=Path)
    p.add_argument("--mode", required=True,
                   choices=["compile", "compile+test", "compile+cheat-matrix", "none"])
    p.add_argument("--plugin", action="append", default=[],
                   help="Plugin name or path to .uplugin; repeat for multiple")
    p.add_argument("--cheat", action="append", default=[],
                   help="Console command to run in cheat matrix; repeat for multiple")
    p.add_argument("--test-suite", default="AFL",
                   help="Automation test filter, default 'AFL'")
    p.add_argument("--build-timeout", type=int, default=1800)
    p.add_argument("--test-timeout", type=int, default=1200)
    p.add_argument("--workdir", type=Path, default=None)
    args = p.parse_args(argv)

    if args.mode == "none":
        return 0

    work = args.workdir or (args.project_root / ".state" / "verify" / time.strftime("%Y%m%d_%H%M%S"))
    work.mkdir(parents=True, exist_ok=True)

    # 1. Compile every plugin
    plugins = args.plugin or _detect_default_plugins(args.uproject)
    if not plugins:
        print("[verify] no plugins requested and none auto-detected; skipping compile", flush=True)
    for plugin in plugins:
        rc = build_plugin(args.engine_root, args.uproject, plugin, args.build_timeout, work)
        if rc != 0:
            return rc

    if args.mode == "compile":
        return 0
    if args.mode == "compile+test":
        return run_automation_tests(args.engine_root, args.uproject, args.test_suite, args.test_timeout, work)
    if args.mode == "compile+cheat-matrix":
        return run_cheat_matrix(args.engine_root, args.uproject, args.cheat, args.test_timeout, work)
    return 4


def _detect_default_plugins(uproject: Path) -> list[str]:
    """Default to AFLCore + AFLCombat if they exist (matches Sprint 1 Block A)."""
    out: list[str] = []
    for name in ("AFLCore", "AFLCombat"):
        if _resolve_plugin(uproject, name) is not None:
            out.append(name)
    return out


if __name__ == "__main__":
    raise SystemExit(main())
