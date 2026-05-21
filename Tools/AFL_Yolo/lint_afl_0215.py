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

# Constructor-context rule. RequestGameplayTag inside a UObject ctor runs at
# CDO construction time, which races against the per-plugin Tags/*.ini scan.
# Crashed the editor on 2026-05-20 when AFLCombat's CDO loaded before
# AFLCombat's own ini did. Use UE_DEFINE_GAMEPLAY_TAG_STATIC instead — native
# tags register at module init, strictly before any CDO is constructed.
# This rule needs scope awareness (brace depth), so it's a separate pass
# from the regex LINT_RULES above. See scan_file_constructor_tag_lookups().
CTOR_TAG_LOOKUP_RULE = (
    re.compile(r"\bFGameplayTag\s*::\s*RequestGameplayTag\b"),
    "RequestGameplayTag inside a UObject constructor body - the tag ini hasn't "
    "loaded yet at CDO construction time. Declare as a native tag with "
    "UE_DEFINE_GAMEPLAY_TAG_STATIC at file scope and reference the static instead. "
    "See AFLAG_Laser_Pulse.cpp for the post-2026-05-20-crash fix pattern.",
)

# Matches `ClassName::ClassName(` — the start of a C++ constructor definition.
# We accept either `Class::Class(...)` or `Class::Class(...)` followed (after
# possible initializer list) by `{`. The opening brace is found by the brace
# tracker, not the regex.
_CTOR_SIG_RE = re.compile(r"\b(?P<cls>[A-Za-z_]\w*)\s*::\s*(?P=cls)\s*\(")

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


def _strip_cpp_comments(text: str) -> str:
    """Replace C++ comments with spaces so byte offsets are preserved (line
    numbers remain accurate). Doesn't try to be perfect about strings inside
    comments or vice versa — good enough for the heuristics we run."""
    # Block comments first, then line comments. Both replaced with spaces of
    # the same length so all subsequent offsets are unchanged.
    def _spaces(m: "re.Match[str]") -> str:
        return "".join("\n" if c == "\n" else " " for c in m.group(0))
    text = re.sub(r"/\*.*?\*/", _spaces, text, flags=re.DOTALL)
    text = re.sub(r"//[^\n]*", _spaces, text)
    return text


def scan_file_constructor_tag_lookups(path: Path, text: str) -> list[str]:
    """Flag FGameplayTag::RequestGameplayTag inside any C++ constructor body.

    Approach: scan forward, find each ClassName::ClassName( signature, locate
    the opening brace of its body, then walk the brace depth until it returns
    to zero. While inside, any RequestGameplayTag match is a violation. Cheap
    heuristic — doesn't understand templates, attributes, or string-embedded
    braces; good enough for the AFL plugin sources.
    """
    rx_call, msg = CTOR_TAG_LOOKUP_RULE
    code = _strip_cpp_comments(text)
    out: list[str] = []
    pos = 0
    while True:
        m = _CTOR_SIG_RE.search(code, pos)
        if not m:
            break
        # From the end of the signature, find the matching `)` then the next `{`.
        # If there's no `{` before the next `;` or class boundary, this is a
        # declaration (not a definition) — skip it.
        i = m.end()
        # Find matching close-paren of the constructor argument list.
        paren_depth = 1
        while i < len(code) and paren_depth > 0:
            ch = code[i]
            if ch == "(":
                paren_depth += 1
            elif ch == ")":
                paren_depth -= 1
            i += 1
        if i >= len(code):
            break
        # Now skip through any initializer list to the opening brace.
        # If we hit a `;` first, it was a declaration only.
        brace_start = -1
        j = i
        while j < len(code):
            if code[j] == "{":
                brace_start = j
                break
            if code[j] == ";":
                break
            j += 1
        if brace_start < 0:
            pos = i
            continue
        # Walk braces to find the matching close.
        depth = 1
        k = brace_start + 1
        while k < len(code) and depth > 0:
            ch = code[k]
            if ch == "{":
                depth += 1
            elif ch == "}":
                depth -= 1
            k += 1
        body_end = k  # one past the matching `}`
        # Scan body for the forbidden call.
        body = code[brace_start:body_end]
        for hit in rx_call.finditer(body):
            abs_off = brace_start + hit.start()
            line_no = text.count("\n", 0, abs_off) + 1
            out.append(f"{path.as_posix()}:{line_no}: {msg}")
        pos = body_end
    return out


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
    out.extend(scan_file_constructor_tag_lookups(path, text))
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

# Each fixture must trigger exactly ONE violation when scanned.
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
    # The crash that motivated this rule on 2026-05-20: RequestGameplayTag at
    # CDO construction time. The constructor scope tracker must catch it.
    "ctor_tag_lookup.cpp": (
        "UFoo::UFoo()\n"
        "{\n"
        "    AbilityTags.AddTag(FGameplayTag::RequestGameplayTag(TEXT(\"State.Bar\")));\n"
        "}\n"
        "// Outside a constructor — this should NOT trigger (negative case):\n"
        "void UFoo::Activate() {\n"
        "    Spec.SetSetByCallerMagnitude(FGameplayTag::RequestGameplayTag(TEXT(\"Data.X\"), false), 1.0f);\n"
        "}\n"
    ),
}


def run_self_test() -> int:
    """Write fixture files, scan them, verify each rule fires exactly its
    expected number of times."""
    print("AFL-0215 self-test: writing fixtures...")
    with tempfile.TemporaryDirectory(prefix="afl0215_selftest_") as td:
        td_path = Path(td)
        for name, body in _SELF_TEST_FIXTURES.items():
            (td_path / name).write_text(body, encoding="utf-8")

        violations = scan_paths([td_path])
        print(f"AFL-0215 self-test: {len(violations)} violation(s) detected.")
        for v in violations:
            print(f"  {v}")

        # Each of the 4 fixtures is designed to produce exactly ONE violation.
        # If the ctor scope tracker is broken in the false-positive direction,
        # the negative case in ctor_tag_lookup.cpp will inflate this count.
        expected_count = len(_SELF_TEST_FIXTURES)
        if len(violations) != expected_count:
            print(f"FAIL: expected {expected_count} violations, got {len(violations)}.")
            print("Either a rail is missing a hit, or the ctor scope tracker "
                  "is producing false positives outside constructors.")
            return 1

        # Verify each rule fired at least once. The 4th rule (ctor scope) lives
        # in CTOR_TAG_LOOKUP_RULE rather than LINT_RULES, so we check it
        # separately.
        all_messages = [msg for _, msg in LINT_RULES] + [CTOR_TAG_LOOKUP_RULE[1]]
        rules_hit = set()
        for v in violations:
            for i, msg in enumerate(all_messages):
                if msg in v:
                    rules_hit.add(i)
        if len(rules_hit) != len(all_messages):
            missing = set(range(len(all_messages))) - rules_hit
            print(f"FAIL: rules not firing: indices {sorted(missing)}")
            print("Each of the four rules must catch its own fixture.")
            return 1

    print(f"AFL-0215 self-test: PASS — all {len(all_messages)} rails functional.")
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
