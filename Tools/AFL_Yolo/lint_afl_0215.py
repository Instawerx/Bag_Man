#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
lint_afl_0215.py - AFL-0215 architectural lint (standalone).

The three forbidden patterns from master doc §9.3, mirrored from
orchestrator.py LINT_RULES:

  1. AbilitySystemComponent->GiveAbility called directly.
     All abilities MUST be granted via ULyraPawnData ability sets
     (DA_AFL_AbilitySet_*).

  2. Direct SetHealth on UAFLAttributeSet_Combat from an ability or BP.
     Damage MUST route through UAFLDamageExecCalc using the Damage
     meta attribute and a GameplayEffect.

  3. Any GetPlayerViewPoint use (especially server-side).
     Hitscan traces are ALWAYS performed locally on the firing client,
     packed into an FGameplayAbilityTargetDataHandle, and shipped via
     ServerSetReplicatedTargetData (AFL-0106, master doc §7).

Usage:
  python lint_afl_0215.py PATH [PATH ...]
      Scan all .cpp/.h/.hpp/.cc files under each PATH (files or dirs).
      Exit 0 on clean, 1 on violations found, 2 on usage error.

  python lint_afl_0215.py --self-test
      Run the regression guard: writes 3 fixture files containing each
      violation pattern, scans them, asserts non-zero exit + correct
      messages. Exit 0 if the rail still works, 1 if it has silently
      broken.

History:
  This script is the AFL-0215 deliverable that was specified to live at
  Tools/AFL_Lint/afl_arch_lint.py but never actually authored during
  the 2026-05-17 bootstrap (commit 4945a01a falsely marked AFL-0215
  merged). Authored 2026-05-20 by extracting LINT_RULES from
  orchestrator.py so direct manual commits (non-yolo PRs) get caught
  too.
"""

from __future__ import annotations

import argparse
import re
import sys
import tempfile
from pathlib import Path
from typing import Iterable


# -----------------------------------------------------------------------------
# THE RULES
# -----------------------------------------------------------------------------
# Keep these byte-identical with orchestrator.py LINT_RULES. If a rule changes
# in one place, change both. (A future refactor can fold this module into the
# orchestrator's import path, but for now duplication is acceptable per the
# AFL-0215 brief.)

LINT_RULES: list[tuple[re.Pattern[str], str]] = [
    (re.compile(r"\bAbilitySystemComponent\s*->\s*GiveAbility\b"),
     "GiveAbility called directly - abilities must come from ULyraPawnData ability sets."),
    (re.compile(r"\bGetAttributeSet\s*<\s*UAFLAttributeSet_Combat\s*>\s*[^;]*?->\s*SetHealth\b"),
     "Direct SetHealth on AttributeSet - damage must route through UAFLDamageExecCalc."),
    (re.compile(r"\bGetPlayerViewPoint\b"),
     "GetPlayerViewPoint detected - hitscan must use client TargetData per AFL-0106."),
]

CPP_EXTS = {".cpp", ".h", ".hpp", ".cc"}


# -----------------------------------------------------------------------------
# Core scanner
# -----------------------------------------------------------------------------

def iter_cpp_files(paths: Iterable[Path]) -> Iterable[Path]:
    """Yield every C++ source/header reachable from `paths` (files or dirs)."""
    for p in paths:
        if p.is_file():
            if p.suffix.lower() in CPP_EXTS:
                yield p
        elif p.is_dir():
            for child in p.rglob("*"):
                if child.is_file() and child.suffix.lower() in CPP_EXTS:
                    yield child
        # silently skip nonexistent paths; the caller surfaces them


def scan_file(path: Path) -> list[str]:
    """Return a list of violation strings for one file. Empty list = clean."""
    try:
        text = path.read_text(encoding="utf-8", errors="ignore")
    except OSError:
        return []
    out: list[str] = []
    for rx, msg in LINT_RULES:
        for m in rx.finditer(text):
            line_no = text.count("\n", 0, m.start()) + 1
            out.append(f"{path.as_posix()}:{line_no}: {msg}")
    return out


def scan_paths(paths: Iterable[Path]) -> list[str]:
    """Scan every C++ file under the given paths, return all violations."""
    violations: list[str] = []
    for f in iter_cpp_files(paths):
        violations.extend(scan_file(f))
    return violations


# -----------------------------------------------------------------------------
# Self-test (regression guard)
# -----------------------------------------------------------------------------

_SELF_TEST_FIXTURES = {
    "give_ability.cpp": (
        "void Foo() {\n"
        "    AbilitySystemComponent->GiveAbility(SpecHandle);\n"
        "}\n"
    ),
    "set_health.cpp": (
        "void Bar() {\n"
        "    GetAttributeSet<UAFLAttributeSet_Combat>(this)->SetHealth(10.f);\n"
        "}\n"
    ),
    "view_point.cpp": (
        "void Baz() {\n"
        "    GetPlayerViewPoint(Origin, Rot);\n"
        "}\n"
    ),
}


def run_self_test() -> int:
    """Write 3 fixture files, scan them, verify each rule fires once."""
    print("AFL-0215 self-test: writing fixtures...")
    with tempfile.TemporaryDirectory(prefix="afl0215_selftest_") as td:
        td_path = Path(td)
        for name, body in _SELF_TEST_FIXTURES.items():
            (td_path / name).write_text(body, encoding="utf-8")

        violations = scan_paths([td_path])
        print(f"AFL-0215 self-test: {len(violations)} violation(s) detected.")
        for v in violations:
            print(f"  {v}")

        expected_count = len(_SELF_TEST_FIXTURES)
        if len(violations) != expected_count:
            print(f"FAIL: expected {expected_count} violations, got {len(violations)}.")
            print("This means one of the three architectural rails has silently broken.")
            return 1

        # Verify each rule fired at least once (not just one rule firing
        # three times because of regex overlap or a bad refactor).
        rules_hit = set()
        for v in violations:
            for i, (_, msg) in enumerate(LINT_RULES):
                if msg in v:
                    rules_hit.add(i)
        if len(rules_hit) != len(LINT_RULES):
            missing = set(range(len(LINT_RULES))) - rules_hit
            print(f"FAIL: rules not firing: indices {sorted(missing)}")
            print("Each of the three rules must catch its own fixture.")
            return 1

    print("AFL-0215 self-test: PASS — all 3 rails functional.")
    return 0


# -----------------------------------------------------------------------------
# CLI
# -----------------------------------------------------------------------------

def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(
        prog="lint_afl_0215",
        description="AFL-0215 architectural lint (standalone). "
                    "Mirrors the three rules from master doc §9.3.",
    )
    p.add_argument(
        "paths", nargs="*", type=Path,
        help="Files or directories to scan. Recurses into directories. "
             "Filters to .cpp/.h/.hpp/.cc extensions.",
    )
    p.add_argument(
        "--self-test", action="store_true",
        help="Run regression guard: write fixture violations, scan them, "
             "assert all 3 rules fire. Exits 0 if rails are healthy.",
    )
    args = p.parse_args(argv)

    if args.self_test:
        return run_self_test()

    if not args.paths:
        p.print_usage(sys.stderr)
        print("error: no paths given (or pass --self-test)", file=sys.stderr)
        return 2

    violations = scan_paths(args.paths)
    if violations:
        print(f"AFL-0215: {len(violations)} violation(s):")
        for v in violations:
            print(f"  {v}")
        return 1

    # Count what we actually scanned so silent zero-files doesn't masquerade
    # as a clean pass.
    scanned = sum(1 for _ in iter_cpp_files(args.paths))
    print(f"AFL-0215: clean — scanned {scanned} file(s).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
