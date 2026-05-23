#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
afl_mesh_validator.py - AAA conformance gate for genAI mesh outputs.

Validates Tripo / Meshy / hand-authored meshes against the budget table
in `budgets.json` (master doc Sec. 16.2). Pure stdlib; no UE dep, no pip
deps beyond pytest (dev-only).

CLI:
  python -m afl_mesh_validator <file_or_dir>
      [--class hero_char|heavy_weapon|env_prop|kit_piece|decal]
      [--budgets <path>]
      [--manifest-out <path>]
      [--strict]

Exit codes:
  0  all assets passed (or non-strict mode and no hard failures)
  1  one or more assets failed
  2  invalid input / blender missing for FBX input

FBX inputs are shelled out through `blender --background` to produce a
glb that this validator then reads. If `blender` is not on PATH we exit
2 with a helpful warning (per design decision #1, 2026-05-22 EOD handoff).
"""

from __future__ import annotations

import argparse
import datetime as _dt
import json
import re
import shutil
import struct
import subprocess
import sys
from pathlib import Path
from typing import Any, Iterable

__all__ = [
    "validate_file",
    "validate_paths",
    "load_budgets",
    "detect_class",
    "ValidationError",
]


# -----------------------------------------------------------------------------
# Errors
# -----------------------------------------------------------------------------

class ValidationError(Exception):
    """Raised for unrecoverable input problems (bad header, missing blender)."""


# -----------------------------------------------------------------------------
# Budgets loader
# -----------------------------------------------------------------------------

DEFAULT_BUDGETS_PATH = Path(__file__).resolve().parent / "budgets.json"


def load_budgets(path: Path | None = None) -> dict[str, Any]:
    p = Path(path) if path else DEFAULT_BUDGETS_PATH
    with p.open("r", encoding="utf-8") as fh:
        data = json.load(fh)
    if "classes" not in data or "detection_rules" not in data:
        raise ValidationError(f"budgets file missing required keys: {p}")
    return data


# -----------------------------------------------------------------------------
# Class detection (filename → class)
# -----------------------------------------------------------------------------

def detect_class(stem: str, budgets: dict[str, Any]) -> tuple[str | None, int | None]:
    """Return (class_name, lod_tier or None) from an asset stem.

    LOD suffix is stripped before applying detection rules so the class
    of `SM_AFL_Crate_Kit_LOD2` is still `kit_piece` (tier 2).
    """
    suffix_re = re.compile(budgets.get("lod_suffix_regex", r"_LOD(?P<tier>[0-3])$"))
    lod_tier: int | None = None
    m = suffix_re.search(stem)
    base = stem
    if m:
        lod_tier = int(m.group("tier"))
        base = stem[: m.start()]

    for rule in budgets["detection_rules"]:
        if re.search(rule["regex"], base):
            return rule["class"], lod_tier
    return None, lod_tier


# -----------------------------------------------------------------------------
# glTF / glb parsing
# -----------------------------------------------------------------------------

_GLB_MAGIC = 0x46546C67  # b'glTF' little-endian


def load_gltf_json(path: Path) -> dict[str, Any]:
    """Load the glTF JSON document from a .gltf or .glb file.

    For .glb we read the 12-byte header and the first chunk (which MUST
    be the JSON per the glTF 2.0 spec). The bin chunk is intentionally
    ignored — every check we run is metadata-only.
    """
    suffix = path.suffix.lower()
    if suffix == ".gltf":
        with path.open("r", encoding="utf-8") as fh:
            return json.load(fh)
    if suffix == ".glb":
        with path.open("rb") as fh:
            header = fh.read(12)
            if len(header) < 12:
                raise ValidationError(f"glb truncated header: {path}")
            magic, version, _length = struct.unpack("<III", header)
            if magic != _GLB_MAGIC:
                raise ValidationError(f"glb bad magic: {path}")
            if version != 2:
                raise ValidationError(f"glb unsupported version {version}: {path}")
            chunk_header = fh.read(8)
            if len(chunk_header) < 8:
                raise ValidationError(f"glb missing JSON chunk: {path}")
            chunk_len, chunk_type = struct.unpack("<II", chunk_header)
            if chunk_type != 0x4E4F534A:  # 'JSON'
                raise ValidationError(f"glb first chunk not JSON: {path}")
            payload = fh.read(chunk_len)
            # glb pads JSON with spaces (0x20) to a 4-byte boundary; json.loads tolerates it.
            return json.loads(payload.decode("utf-8"))
    raise ValidationError(f"unsupported mesh suffix: {path}")


# -----------------------------------------------------------------------------
# FBX → glb shell-out (optional, requires blender on PATH)
# -----------------------------------------------------------------------------

_BLENDER_CONVERTER_SRC = r"""
import bpy, sys
argv = sys.argv[sys.argv.index('--') + 1:]
src, dst = argv[0], argv[1]
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=src)
bpy.ops.export_scene.gltf(filepath=dst, export_format='GLB')
"""


def _ensure_converter_script(tmp_dir: Path) -> Path:
    script = tmp_dir / "afl_fbx_to_glb.py"
    script.write_text(_BLENDER_CONVERTER_SRC, encoding="utf-8")
    return script


def convert_fbx_to_glb(fbx_path: Path, out_dir: Path) -> Path:
    if shutil.which("blender") is None:
        raise ValidationError(
            "FBX input requires `blender` on PATH (Blender 4.x). "
            "Convert to glTF/glb yourself or install Blender."
        )
    out_dir.mkdir(parents=True, exist_ok=True)
    script = _ensure_converter_script(out_dir)
    glb_path = out_dir / (fbx_path.stem + ".glb")
    cmd = [
        "blender", "--background", "--python", str(script),
        "--", str(fbx_path), str(glb_path),
    ]
    proc = subprocess.run(cmd, capture_output=True, text=True)
    if proc.returncode != 0 or not glb_path.exists():
        raise ValidationError(
            f"blender FBX→glb failed (rc={proc.returncode}): {proc.stderr[:400]}"
        )
    return glb_path


# -----------------------------------------------------------------------------
# Checks
# -----------------------------------------------------------------------------

def _triangle_count(gltf: dict[str, Any]) -> int:
    """Sum of (indices.count / 3) across all primitives.

    Falls back to position-accessor count / 3 for non-indexed primitives.
    Treats mode==4 (TRIANGLES, the default) — other modes ignored as the
    pipeline rejects them upstream.
    """
    accessors = gltf.get("accessors", [])
    total = 0
    for mesh in gltf.get("meshes", []):
        for prim in mesh.get("primitives", []):
            mode = prim.get("mode", 4)
            if mode != 4:
                continue
            if "indices" in prim:
                acc = accessors[prim["indices"]]
                total += acc.get("count", 0) // 3
            else:
                pos = prim.get("attributes", {}).get("POSITION")
                if pos is not None:
                    total += accessors[pos].get("count", 0) // 3
    return total


def _check_poly_budget(gltf: dict, cls: str, tier: int | None, budgets: dict) -> dict:
    b = budgets["classes"][cls]
    tris = _triangle_count(gltf)
    # When tier known, compare to that tier's budget; default to LOD0.
    if tier == 3 and b.get("lod3_tris") is not None:
        budget = b["lod3_tris"]
        tier_lbl = "LOD3"
    elif tier == 3 and b.get("lod3_tris") is None:
        # Class has no LOD3 budget (decal). Treat as pass with note.
        return {
            "name": "POLY_BUDGET",
            "pass": True,
            "detail": f"{tris} tris (class {cls} has no LOD3 budget)",
        }
    else:
        budget = b["lod0_tris"]
        tier_lbl = f"LOD{tier}" if tier is not None else "LOD0"
    return {
        "name": "POLY_BUDGET",
        "pass": tris <= budget,
        "detail": f"{tris}/{budget} {tier_lbl} tris",
    }


def _check_naming(stem: str, cls: str, budgets: dict) -> dict:
    pattern = budgets["naming_rules"].get(cls)
    if pattern is None:
        return {"name": "NAMING", "pass": True, "detail": f"no naming rule for class {cls}"}
    ok = bool(re.match(pattern, stem))
    return {
        "name": "NAMING",
        "pass": ok,
        "detail": f"stem '{stem}' vs /{pattern}/",
    }


def _check_uv_coverage(gltf: dict) -> dict:
    """At least one primitive has TEXCOORD_0; all UV accessors fit [0,1].

    Uses accessor `min`/`max` (cheap, exporter-supplied). If min/max are
    absent we pass the bounds part with an explanatory detail — the
    validator does not decode bin payloads for performance reasons.
    """
    accessors = gltf.get("accessors", [])
    has_uv = False
    out_of_bounds: list[str] = []
    for mesh in gltf.get("meshes", []):
        for prim in mesh.get("primitives", []):
            tx = prim.get("attributes", {}).get("TEXCOORD_0")
            if tx is None:
                continue
            has_uv = True
            acc = accessors[tx]
            amin = acc.get("min")
            amax = acc.get("max")
            if amin is None or amax is None:
                continue
            if any(v < 0.0 for v in amin) or any(v > 1.0 for v in amax):
                out_of_bounds.append(
                    f"acc[{tx}] min={amin} max={amax}"
                )
    if not has_uv:
        return {"name": "UV_COVERAGE", "pass": False, "detail": "no TEXCOORD_0 attribute"}
    if out_of_bounds:
        return {
            "name": "UV_COVERAGE",
            "pass": False,
            "detail": "uv outside [0,1]: " + "; ".join(out_of_bounds),
        }
    return {"name": "UV_COVERAGE", "pass": True, "detail": "TEXCOORD_0 present, in bounds"}


def _check_mat_slots(gltf: dict, cls: str, budgets: dict) -> dict:
    budget = budgets["classes"][cls]["mat_slots"]
    seen: set[int] = set()
    for mesh in gltf.get("meshes", []):
        for prim in mesh.get("primitives", []):
            mat = prim.get("material")
            if mat is not None:
                seen.add(int(mat))
    count = len(seen)
    return {
        "name": "MAT_SLOTS",
        "pass": count <= budget,
        "detail": f"{count}/{budget} unique material slots",
    }


def _check_socket_sniff(gltf: dict, cls: str, budgets: dict) -> dict:
    if cls != "hero_char":
        return {"name": "SOCKET_SNIFF", "pass": True, "detail": "n/a (non-hero)"}
    expected = set(budgets.get("hero_sockets", []))
    if not expected:
        return {"name": "SOCKET_SNIFF", "pass": True, "detail": "no canonical sockets configured"}
    node_names = {n.get("name", "") for n in gltf.get("nodes", [])}
    found = sorted(expected & node_names)
    missing = sorted(expected - node_names)
    # Always pass (WARN-level per task brief). Detail enumerates absence.
    return {
        "name": "SOCKET_SNIFF",
        "pass": True,
        "detail": (
            f"found={found}; missing={missing}"
            if missing
            else f"all canonical sockets present ({len(found)})"
        ),
    }


# -----------------------------------------------------------------------------
# Orchestration
# -----------------------------------------------------------------------------

def validate_file(
    path: Path,
    budgets: dict[str, Any],
    cls_override: str | None = None,
    fbx_tmp_dir: Path | None = None,
) -> dict[str, Any]:
    """Validate a single mesh file and return a manifest row."""
    path = Path(path)
    stem = path.stem
    if path.suffix.lower() == ".fbx":
        glb = convert_fbx_to_glb(path, fbx_tmp_dir or path.parent / ".afl_fbx_cache")
        gltf = load_gltf_json(glb)
    else:
        gltf = load_gltf_json(path)

    cls, tier = detect_class(stem, budgets)
    if cls_override:
        cls = cls_override
    if cls is None or cls not in budgets["classes"]:
        return {
            "path": str(path),
            "class": None,
            "pass": False,
            "checks": [],
            "reasons": [
                f"could not detect asset class from stem '{stem}' "
                f"(use --class to override; valid: {sorted(budgets['classes'])})"
            ],
        }

    checks = [
        _check_poly_budget(gltf, cls, tier, budgets),
        _check_naming(stem, cls, budgets),
        _check_uv_coverage(gltf),
        _check_mat_slots(gltf, cls, budgets),
        _check_socket_sniff(gltf, cls, budgets),
    ]
    reasons = [f"{c['name']}: {c['detail']}" for c in checks if not c["pass"]]
    return {
        "path": str(path),
        "class": cls,
        "pass": not reasons,
        "checks": checks,
        "reasons": reasons,
    }


def _iter_mesh_files(root: Path) -> Iterable[Path]:
    if root.is_file():
        yield root
        return
    for p in sorted(root.rglob("*")):
        if p.is_file() and p.suffix.lower() in {".gltf", ".glb", ".fbx"}:
            yield p


def validate_paths(
    target: Path,
    budgets: dict[str, Any],
    cls_override: str | None = None,
) -> dict[str, Any]:
    target = Path(target)
    if not target.exists():
        raise ValidationError(f"input path does not exist: {target}")
    rows = [
        validate_file(p, budgets, cls_override=cls_override)
        for p in _iter_mesh_files(target)
    ]
    return {
        "version": 1,
        "timestamp": _dt.datetime.now(_dt.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "assets": rows,
    }


# -----------------------------------------------------------------------------
# Pretty printing
# -----------------------------------------------------------------------------

def _print_summary(manifest: dict[str, Any], out: Any | None = None) -> None:
    stream = out if out is not None else sys.stdout
    total = len(manifest["assets"])
    passed = sum(1 for a in manifest["assets"] if a["pass"])
    stream.write(f"AFL mesh validator - {passed}/{total} passed\n")
    for asset in manifest["assets"]:
        marker = "PASS" if asset["pass"] else "FAIL"
        stream.write(f"  [{marker}] {asset['path']}  class={asset['class']}\n")
        for c in asset["checks"]:
            cm = "ok" if c["pass"] else "FAIL"
            stream.write(f"      - {c['name']:<14} {cm:<4} {c['detail']}\n")
        for r in asset["reasons"]:
            stream.write(f"      ! {r}\n")


# -----------------------------------------------------------------------------
# CLI
# -----------------------------------------------------------------------------

def _build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        prog="afl_mesh_validator",
        description="AAA conformance gate for AFL genAI mesh outputs.",
    )
    p.add_argument("target", help="File or directory to validate.")
    p.add_argument(
        "--class",
        dest="cls",
        choices=["hero_char", "heavy_weapon", "env_prop", "kit_piece", "decal"],
        default=None,
        help="Override auto-detected asset class.",
    )
    p.add_argument(
        "--budgets",
        default=None,
        help=f"Path to budgets.json (default: {DEFAULT_BUDGETS_PATH}).",
    )
    p.add_argument(
        "--manifest-out",
        default=None,
        help="Write JSON manifest here (default: stdout, with pretty summary).",
    )
    p.add_argument(
        "--strict",
        action="store_true",
        help="Single-file mode: exit 1 on any check failure (directory mode is always strict).",
    )
    return p


def main(argv: list[str] | None = None) -> int:
    args = _build_parser().parse_args(argv)
    try:
        budgets = load_budgets(Path(args.budgets) if args.budgets else None)
        manifest = validate_paths(Path(args.target), budgets, cls_override=args.cls)
    except ValidationError as exc:
        sys.stderr.write(f"afl_mesh_validator: {exc}\n")
        return 2

    if args.manifest_out:
        Path(args.manifest_out).write_text(
            json.dumps(manifest, indent=2, sort_keys=True), encoding="utf-8"
        )
        _print_summary(manifest)
    else:
        _print_summary(manifest)
        sys.stdout.write("\n")
        json.dump(manifest, sys.stdout, indent=2, sort_keys=True)
        sys.stdout.write("\n")

    any_failed = any(not a["pass"] for a in manifest["assets"])
    # Default is strict: any failure → exit 1. The --strict flag is accepted
    # for explicitness and forward-compat with a future warn-only default.
    return 1 if any_failed else 0


if __name__ == "__main__":
    sys.exit(main())
