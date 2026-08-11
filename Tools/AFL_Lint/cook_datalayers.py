#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
cook_datalayers.py - AFL DataLayer cook coverage (AFL-DL).

THE DEFECT THIS EXISTS TO PREVENT
---------------------------------
World Partition DataLayer packages are not emitted consistently. Measured across six full cooks on
2026-08-10 (manifests in Docs/reference/cook-manifests): the `.umap` files never moved, but their
DataLayer packages did, in both directions, across three different maps -- and every one of those
cooks reported `Success - 0 error(s)`.

A missing data layer is a map whose streaming is silently wrong in a packaged build and CORRECT IN
PIE, where every asset is on disk. That is the same shape as every cook-refs bug in DefaultGame.ini's
DirectoriesToAlwaysCook block, and it is invisible in the exit code, the warnings, and the totals.

The root cause was never found. Two hypotheses were tested and both failed: an aborted cook is not
the trigger (reproduced deliberately, nothing dropped), and clearing the DerivedDataCache changes the
outcome without stabilising it. So the fix does not depend on the cause -- the two DataLayer roots are
force-cooked, which makes their packages DECLARED rather than discovered.

WHY THIS SCRIPT, GIVEN THE FIX IS ALREADY IN
--------------------------------------------
Because the fix covers the folders that existed on the day it was written. A NEW World Partition map
drops its layers into a NEW folder, and unless somebody remembers to extend DirectoriesToAlwaysCook,
that map re-enters the lottery -- silently, exactly like the first time. Remembering is not a control.

This compares what is ON DISK to what is IN A COOK TREE and fails on any gap. It is deliberately not
part of cook_soft_refs.py: that tool answers "did anyone name an asset in a way the cooker cannot
follow", which is a source-scanning question. This one is a pure artifact comparison and shares none
of its machinery.

Usage:
    python Tools/AFL_Lint/cook_datalayers.py --root . --cooked-dir Saved/Cooked/Windows
    python Tools/AFL_Lint/cook_datalayers.py --root . --self-test

Exit codes:
    0  every DataLayer on disk is present in the cook
    1  at least one is missing
    2  usage / arg error
"""

from __future__ import annotations

import argparse
import sys
import tempfile
from pathlib import Path

# Where DataLayer assets live. A folder literally named DataLayers, anywhere under a content root --
# matched by SHAPE rather than by an enumerated list, because an enumerated list is the thing that goes
# stale when a new map lands, which is the whole failure this script exists to catch.
DATALAYER_DIR = "DataLayers"

# Content roots to scan. /Game plus every GameFeature plugin's Content.
CONTENT_GLOBS = ("Content", "Plugins/**/Content")


def find_source_layers(root: Path) -> dict[str, set[str]]:
    """{map_folder: {layer_name, ...}} for every DataLayers/<Map>/*.uasset on disk."""
    out: dict[str, set[str]] = {}
    for pattern in CONTENT_GLOBS:
        for content_root in root.glob(pattern):
            if not content_root.is_dir():
                continue
            for layer in content_root.rglob(f"{DATALAYER_DIR}/*/*.uasset"):
                out.setdefault(layer.parent.name, set()).add(layer.stem)
    return out


def find_cooked_layers(cooked_dir: Path) -> dict[str, set[str]]:
    """The same shape, read out of a cook tree."""
    out: dict[str, set[str]] = {}
    for layer in cooked_dir.rglob(f"{DATALAYER_DIR}/*/*.uasset"):
        out.setdefault(layer.parent.name, set()).add(layer.stem)
    return out


def compare(source: dict[str, set[str]], cooked: dict[str, set[str]]) -> list[str]:
    """Missing entries as `Map/Layer`, sorted. Extra cooked layers are NOT an error."""
    missing: list[str] = []
    for map_name, layers in source.items():
        for layer in layers - cooked.get(map_name, set()):
            missing.append(f"{map_name}/{layer}")
    return sorted(missing)


def run_self_test() -> int:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp) / "repo"
        cooked = Path(tmp) / "cooked"

        # Two maps, one in /Game and one in a plugin -- the plugin case is the one an enumerated list
        # forgets, and it is where L_Expanse_Blockout actually lives.
        for rel in ("Content/Maps/DataLayers/L_A/Gameplay.uasset",
                    "Content/Maps/DataLayers/L_A/Lighting.uasset",
                    "Plugins/GameFeatures/Thing/Content/Maps/DataLayers/L_B/Layout.uasset"):
            p = root / rel
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_text("x", encoding="utf-8")

        # A cook missing exactly one of them.
        for rel in ("Proj/Content/Maps/DataLayers/L_A/Gameplay.uasset",
                    "Thing/Content/Maps/DataLayers/L_B/Layout.uasset"):
            p = cooked / rel
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_text("x", encoding="utf-8")

        src = find_source_layers(root)
        if set(src) != {"L_A", "L_B"}:
            print(f"FAIL: source scan found {sorted(src)}, expected L_A and L_B")
            return 1
        if src["L_A"] != {"Gameplay", "Lighting"}:
            print(f"FAIL: source scan missed a layer: {sorted(src['L_A'])}")
            return 1

        missing = compare(src, find_cooked_layers(cooked))
        if missing != ["L_A/Lighting"]:
            print(f"FAIL: expected exactly L_A/Lighting missing, got {missing}")
            return 1

        # An EXTRA cooked layer must not fail -- the cook legitimately emits generated packages that
        # have no source file, and treating those as errors would make this unusable.
        extra = cooked / "Proj/Content/Maps/DataLayers/L_A/Generated.uasset"
        extra.write_text("x", encoding="utf-8")
        if compare(src, find_cooked_layers(cooked)) != ["L_A/Lighting"]:
            print("FAIL: an extra cooked layer was treated as a problem")
            return 1

    print("AFL-DL self-test: PASS - source scan, plugin roots, missing detection, extras ignored.")
    return 0


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(
        prog="cook_datalayers",
        description="AFL DataLayer cook coverage (AFL-DL). Every DataLayer on disk must be in the cook.")
    p.add_argument("--root", type=Path, default=Path("."), help="Repo root (default: cwd).")
    p.add_argument("--cooked-dir", type=Path, default=None, help="Cook tree, e.g. Saved/Cooked/Windows.")
    p.add_argument("--self-test", action="store_true", help="Regression guard; exits 0 if healthy.")
    args = p.parse_args(argv)

    if args.self_test:
        return run_self_test()
    if args.cooked_dir is None:
        p.error("--cooked-dir is required (this rule compares a cook tree to disk)")
    if not args.cooked_dir.is_dir():
        print(f"AFL-DL: no cook tree at {args.cooked_dir}")
        return 2

    source = find_source_layers(args.root)
    cooked = find_cooked_layers(args.cooked_dir)
    missing = compare(source, cooked)

    total = sum(len(v) for v in source.values())
    if missing:
        print(f"\nAFL-DL: {len(missing)} DataLayer package(s) ON DISK but ABSENT from {args.cooked_dir}.")
        print("  A missing data layer is a map whose streaming is silently wrong in a packaged build")
        print("  and correct in PIE. This does not appear in the cook's exit code or warnings.\n")
        for m in missing:
            print(f"  {m}")
        roots = sorted({m.split('/')[0] for m in missing})
        print(f"\n  remedy: cover the affected map folder(s) with +DirectoriesToAlwaysCook in "
              f"Config/DefaultGame.ini -- {', '.join(roots)}")
        return 1

    print(f"AFL-DL: clean - {total} DataLayer package(s) across {len(source)} map(s), all present in the cook.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
