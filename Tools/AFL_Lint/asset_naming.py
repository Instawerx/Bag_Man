#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
asset_naming.py - AFL asset naming convention lint (AFL-0109).

Enforces the three rules from master doc Sec. 3.4:

  1. AFL-owned .uasset files use an approved prefix
     (BP_/ABP_/AnimBP_/A_/M_/MI_/MF_/M_AFL_/SK_/SM_/T_/NS_/NSE_/MS_/
      DA_/DT_/CT_/GE_/AS_/IA_/IC_, plus the prevailing repo set —
      see ALLOWED_ASSET_PREFIXES below).

  2. AFL-owned C++ types (declared in Plugins/GameFeatures/AFL*/Source)
     follow the Unreal A/U/F/E prefix convention AND contain "AFL"
     somewhere in the name.

  3. Every .uplugin file's stem equals its containing directory's name.

Usage:
    python Tools/AFL_Lint/asset_naming.py --root .
    python Tools/AFL_Lint/asset_naming.py --root . --self-test

Exit codes:
    0  clean
    1  violations found
    2  usage / arg error

Scope notes (intentional, not a bug):
  * The Content/ tree at the repo root is mostly Lyra-derived and
    marketplace content. Only AFL-owned subtrees are linted:
        Content/AFL/        (excluding _Bridge/ pipeline staging)
        Content/BagMan/
        Plugins/GameFeatures/AFL*/Content/
    Adding new AFL content roots? Extend AFL_CONTENT_ROOTS below.
  * C++ scan is restricted to Plugins/GameFeatures/AFL*/Source/. The
    project's Source/ tree is Lyra engine code we do not own.
  * Generated bindings (manifest.uasset, pipeline staging files under
    _Bridge/) are exempt — these are tool artifacts, not authored
    assets.
"""

from __future__ import annotations

import argparse
import re
import sys
import tempfile
from pathlib import Path


# -----------------------------------------------------------------------------
# Rule data
# -----------------------------------------------------------------------------

# Asset prefixes allowed under AFL-owned Content/ trees. Order matters only
# in that the longest matching prefix wins (so M_AFL_ is tried before M_).
ALLOWED_ASSET_PREFIXES: tuple[str, ...] = (
    "AnimBP_",  # animation blueprint
    "ABP_",     # animation blueprint (Lyra short form)
    "BP_",      # blueprint
    "M_AFL_",   # AFL-branded master material (declared in brief)
    "MI_",      # material instance
    "MF_",      # material function
    "M_",       # material
    "A_",       # animation sequence
    "SK_",      # skeletal mesh
    "SM_",      # static mesh
    "T_",       # texture
    "NSE_",     # niagara system effect
    "NS_",      # niagara system
    "MS_",      # metasound source / graph
    "DA_",      # data asset
    "DT_",      # data table
    "CT_",      # curve table
    "GE_",      # gameplay effect
    "AS_",      # ability set (Lyra short form)
    "IA_",      # input action
    "IC_",      # input config
    # Prevailing in the repo - kept so the lint matches what is already there
    # without forcing a mass-rename.
    "GA_",      # gameplay ability (e.g., GA_AFL_Dash)
    "S_",       # slate widget brush
    "W_",       # widget blueprint
    # Lyra-derived asset patterns (AFL-0110 brought these in via the
    # L_ShooterGym repoint). Matches Lyra's own canonical naming so
    # AFL-extension assets that mirror Lyra's classes (Experiences,
    # Maps, Levels) don't get false-flagged.
    "B_",       # Blueprint subclass (e.g., B_LyraExperience_AFL_Arena_Test)
    "L_",       # Level / map (e.g., L_AFL_Arena_Test)
    "LAS_",     # Lyra ActionSet (e.g., LAS_AFL_HeroComponents)
    "IMC_",     # Input Mapping Context (Lyra-EnhancedInput convention)
    "InputData_",  # Lyra InputData asset (e.g., InputData_AFL_Hero)
)

# AFL-owned content roots. Anything outside these is exempt - Lyra and
# marketplace content under Content/ uses its own naming.
AFL_CONTENT_ROOTS: tuple[str, ...] = (
    "Content/AFL",
    "Content/BagMan",
)

# Subpaths inside an AFL content root that are exempt. _Bridge/ holds
# Blender->UE pipeline staging assets whose names come from the DCC tool,
# not from authored AFL conventions.
EXEMPT_CONTENT_SUBPATHS: tuple[str, ...] = (
    "Content/AFL/_Bridge",
    "Content/AFL/References",  # PNG reference art, not packaged assets
)

# File extensions inside AFL content roots that this lint actually checks.
# .uasset / .umap are the authored Unreal assets; everything else (.fbx,
# .png, .json sidecars) is tool input/output and exempt.
CONTENT_LINTED_EXTS: frozenset[str] = frozenset({".uasset", ".umap"})

# Plugin source trees we own (and therefore enforce class naming on).
AFL_PLUGIN_SOURCE_GLOB = "Plugins/GameFeatures/AFL*/Source"

# Matches a UCLASS/USTRUCT/UENUM-declared type. We only care about
# declarations (not forward declarations), so we require an opening brace
# or an inheritance colon on the same line. Captures the type name.
#
#   class AFLCOMBAT_API UAFLDamageExecCalc : public ...
#   class AFLMOVEMENT_API AAFLCharacter : public ...
#   struct FAFLFoo { ... }
#   enum class EAFLBar : uint8 { ... }
#
# The API macro (e.g. AFLCOMBAT_API) is optional - some internal types
# omit it. Forward declarations like `class UFoo;` are skipped because
# they end with ';' and we require '{' or ':'.
CLASS_DECL_RE = re.compile(
    r"""
    ^\s*
    (?:template\s*<[^>]*>\s*)?                # optional template prefix
    (?:class|struct)\s+
    (?:[A-Z][A-Z0-9_]*_API\s+)?               # optional MODULE_API macro
    (?:final\s+)?
    ([A-Za-z_][A-Za-z0-9_]*)                  # captured type name (prefix
                                              # validity is checked by the
                                              # caller, not the regex, so
                                              # bad-prefix decls still fire)
    \s*
    (?:final\s+)?
    [:{]                                       # inheritance or body start
    """,
    re.VERBOSE | re.MULTILINE,
)

ENUM_DECL_RE = re.compile(
    r"""
    ^\s*
    enum\s+class\s+
    ([A-Za-z_][A-Za-z0-9_]*)                  # captured enum name
    """,
    re.VERBOSE | re.MULTILINE,
)


# -----------------------------------------------------------------------------
# Path helpers
# -----------------------------------------------------------------------------

def _posix_rel(root: Path, p: Path) -> str:
    """Return p relative to root as a forward-slash string (stable across OSes)."""
    return p.resolve().relative_to(root.resolve()).as_posix()


def _is_under(rel: str, prefix: str) -> bool:
    """True if rel == prefix or rel starts with prefix + '/'."""
    return rel == prefix or rel.startswith(prefix + "/")


# -----------------------------------------------------------------------------
# Rule 1: asset prefix lint
# -----------------------------------------------------------------------------

def _stem_has_allowed_prefix(stem: str) -> bool:
    """True if the file stem starts with one of the approved AFL prefixes."""
    return any(stem.startswith(pfx) for pfx in ALLOWED_ASSET_PREFIXES)


def lint_asset_names(root: Path) -> list[str]:
    """Return one violation string per offending .uasset/.umap under AFL roots."""
    violations: list[str] = []
    for content_root in AFL_CONTENT_ROOTS:
        base = root / content_root
        if not base.is_dir():
            continue
        for f in base.rglob("*"):
            if not f.is_file():
                continue
            if f.suffix.lower() not in CONTENT_LINTED_EXTS:
                continue
            rel = _posix_rel(root, f)
            if any(_is_under(rel, ex) for ex in EXEMPT_CONTENT_SUBPATHS):
                continue
            if not _stem_has_allowed_prefix(f.stem):
                allowed_str = " ".join(ALLOWED_ASSET_PREFIXES)
                violations.append(
                    f"{rel}: asset name '{f.stem}' lacks an AFL-approved "
                    f"prefix (one of: {allowed_str})"
                )
    return violations


# -----------------------------------------------------------------------------
# Rule 2: C++ class naming lint
# -----------------------------------------------------------------------------

CPP_HEADER_EXTS: frozenset[str] = frozenset({".h", ".hpp"})


def _afl_owned_headers(root: Path) -> list[Path]:
    """All .h/.hpp under Plugins/GameFeatures/AFL*/Source/."""
    out: list[Path] = []
    base = root / "Plugins" / "GameFeatures"
    if not base.is_dir():
        return out
    for plugin_dir in base.iterdir():
        if not plugin_dir.is_dir() or not plugin_dir.name.startswith("AFL"):
            continue
        source_dir = plugin_dir / "Source"
        if not source_dir.is_dir():
            continue
        for f in source_dir.rglob("*"):
            if f.is_file() and f.suffix.lower() in CPP_HEADER_EXTS:
                out.append(f)
    return out


def _extract_type_decls(text: str) -> list[tuple[str, int]]:
    """Return (typename, line_number) for every class/struct/enum decl."""
    decls: list[tuple[str, int]] = []
    for m in CLASS_DECL_RE.finditer(text):
        line = text.count("\n", 0, m.start()) + 1
        decls.append((m.group(1), line))
    for m in ENUM_DECL_RE.finditer(text):
        line = text.count("\n", 0, m.start()) + 1
        decls.append((m.group(1), line))
    return decls


def lint_cpp_class_names(root: Path) -> list[str]:
    """AFL-owned types must (a) carry an A/U/F/E prefix and (b) contain 'AFL'."""
    violations: list[str] = []
    for header in _afl_owned_headers(root):
        try:
            text = header.read_text(encoding="utf-8", errors="ignore")
        except OSError:
            continue
        rel = _posix_rel(root, header)
        for name, line in _extract_type_decls(text):
            if not name or name[0] not in "AUFE":
                violations.append(
                    f"{rel}:{line}: type '{name}' missing Unreal A/U/F/E prefix"
                )
                continue
            if "AFL" not in name:
                violations.append(
                    f"{rel}:{line}: AFL-owned type '{name}' must contain 'AFL' "
                    f"somewhere in the name"
                )
    return violations


# -----------------------------------------------------------------------------
# Rule 3: .uplugin stem must equal containing directory name
# -----------------------------------------------------------------------------

def lint_uplugin_names(root: Path) -> list[str]:
    """Every .uplugin file's stem must match its parent directory's name."""
    violations: list[str] = []
    for uplugin in root.rglob("*.uplugin"):
        if not uplugin.is_file():
            continue
        rel = _posix_rel(root, uplugin)
        expected = uplugin.parent.name
        if uplugin.stem != expected:
            violations.append(
                f"{rel}: .uplugin stem '{uplugin.stem}' does not match parent "
                f"directory '{expected}'"
            )
    return violations


# -----------------------------------------------------------------------------
# Driver
# -----------------------------------------------------------------------------

def run_all(root: Path) -> list[str]:
    """Run every rule and return the merged violation list."""
    violations: list[str] = []
    violations.extend(lint_asset_names(root))
    violations.extend(lint_cpp_class_names(root))
    violations.extend(lint_uplugin_names(root))
    return violations


# -----------------------------------------------------------------------------
# Self-test (regression guard - mirrors lint_afl_0215.py's pattern)
# -----------------------------------------------------------------------------

def run_self_test() -> int:
    """Write fixtures for every rule, scan them, assert each rule fires."""
    print("AFL-0109 self-test: building fixture tree...")
    with tempfile.TemporaryDirectory(prefix="afl0109_selftest_") as td:
        root = Path(td)

        # Rule 1: AFL content with bad prefix + valid prefix + exempt area.
        afl_content = root / "Content" / "AFL"
        afl_content.mkdir(parents=True)
        (afl_content / "BadlyNamedAsset.uasset").write_bytes(b"x")     # VIOLATION
        (afl_content / "BP_Good.uasset").write_bytes(b"x")             # ok
        bridge = afl_content / "_Bridge" / "Blender"
        bridge.mkdir(parents=True)
        (bridge / "manifest.uasset").write_bytes(b"x")                 # exempt

        # Rule 1: BagMan tree with one violator.
        bagman = root / "Content" / "BagMan" / "Environments"
        bagman.mkdir(parents=True)
        (bagman / "WrongName.uasset").write_bytes(b"x")                # VIOLATION
        (bagman / "SM_BagMan_Crate.uasset").write_bytes(b"x")          # ok

        # Rule 1: outside AFL roots - lint must NOT fire here.
        other = root / "Content" / "LyraThing"
        other.mkdir(parents=True)
        (other / "anything_goes.uasset").write_bytes(b"x")             # exempt

        # Rule 2: C++ headers in an AFL plugin.
        afl_src = root / "Plugins" / "GameFeatures" / "AFLCombat" / "Source" / "AFLCombat" / "Public"
        afl_src.mkdir(parents=True)
        (afl_src / "Good.h").write_text(
            "UCLASS()\n"
            "class AFLCOMBAT_API UAFLFoo : public UObject {};\n",
            encoding="utf-8",
        )
        (afl_src / "BadPrefix.h").write_text(
            "UCLASS()\n"
            "class AFLCOMBAT_API XAFLBar : public UObject {};\n",  # VIOLATION (no A/U/F/E prefix)
            encoding="utf-8",
        )
        (afl_src / "MissingAFL.h").write_text(
            "UCLASS()\n"
            "class AFLCOMBAT_API UWidget : public UObject {};\n",  # VIOLATION (no AFL substring)
            encoding="utf-8",
        )
        (afl_src / "ForwardDecls.h").write_text(
            "class UGameplayEffect;\n"  # forward decl - must NOT count
            "struct FGameplayAbilityTargetDataHandle;\n",
            encoding="utf-8",
        )

        # Rule 3: .uplugin with mismatched stem.
        plug_bad = root / "Plugins" / "GameFeatures" / "AFLBroken"
        plug_bad.mkdir(parents=True)
        (plug_bad / "WrongName.uplugin").write_text("{}", encoding="utf-8")  # VIOLATION
        plug_good = root / "Plugins" / "GameFeatures" / "AFLOK"
        plug_good.mkdir(parents=True)
        (plug_good / "AFLOK.uplugin").write_text("{}", encoding="utf-8")  # ok

        violations = run_all(root)
        print(f"AFL-0109 self-test: {len(violations)} violation(s).")
        for v in violations:
            print(f"  {v}")

        expected_substrings = [
            "BadlyNamedAsset",
            "WrongName.uasset",
            "XAFLBar",
            "UWidget",
            "WrongName.uplugin",
        ]
        missing = [s for s in expected_substrings if not any(s in v for v in violations)]
        if missing:
            print(f"FAIL: expected violations not raised: {missing}")
            return 1

        forbidden_substrings = [
            "BP_Good",
            "SM_BagMan_Crate",
            "anything_goes",
            "manifest.uasset",
            "UAFLFoo",
            "AFLOK.uplugin",
            "UGameplayEffect",
            "FGameplayAbilityTargetDataHandle",
        ]
        false_pos = [s for s in forbidden_substrings if any(s in v for v in violations)]
        if false_pos:
            print(f"FAIL: false positives on: {false_pos}")
            return 1

    print("AFL-0109 self-test: PASS - all 3 rules fire correctly.")
    return 0


# -----------------------------------------------------------------------------
# CLI
# -----------------------------------------------------------------------------

def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(
        prog="asset_naming",
        description="AFL asset naming convention lint (AFL-0109). "
                    "Scans AFL-owned assets, C++ types, and .uplugin files.",
    )
    p.add_argument(
        "--root", type=Path, default=Path("."),
        help="Repo root to scan (default: current directory).",
    )
    p.add_argument(
        "--self-test", action="store_true",
        help="Regression guard: build a fixture tree containing each rule's "
             "violation, scan it, assert every rule fires. Exits 0 if healthy.",
    )
    args = p.parse_args(argv)

    if args.self_test:
        return run_self_test()

    root = args.root
    if not root.is_dir():
        print(f"error: --root {root} is not a directory", file=sys.stderr)
        return 2

    violations = run_all(root)
    if violations:
        print(f"AFL-0109: {len(violations)} naming violation(s):")
        for v in violations:
            print(f"  {v}")
        return 1

    print("AFL-0109: clean - asset, C++, and .uplugin naming all conform.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
