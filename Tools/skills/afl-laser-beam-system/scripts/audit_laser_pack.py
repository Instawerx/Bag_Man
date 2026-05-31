#!/usr/bin/env python3
"""
audit_laser_pack.py — Inventory + rebrand-manifest generator for the
marketplace "Laser VFX" pack being adopted into the AFL / BAG MAN project.

WHY THIS EXISTS
---------------
Bringing a third-party UE marketplace pack into a Lyra project by hand is
where projects rot: stray Demo content gets cooked, names collide with the
AFL convention, and nobody has a record of what the pack's Niagara systems
actually expose. This script reads the *unzipped* pack on disk and emits:

  1. inventory.json   — every asset, classified, with size
  2. rename_manifest.csv — old /Game path  ->  proposed AFL path + name
  3. A human summary to stdout

It is read-only. It never touches the UE project. The rename manifest is a
PROPOSAL for a human (or AIK) to execute inside the editor with redirectors,
then "Fix Up Redirectors". Treat its output as a plan, not an action.

USAGE
-----
    python3 audit_laser_pack.py <unzipped_pack_root> [--out <dir>]

<unzipped_pack_root> is the folder that contains "Content/LaserFX_BP/...".
"""

import argparse
import csv
import json
import os
import sys
from pathlib import Path

# ---- AFL naming convention --------------------------------------------------
# Prefix is preserved from the source; the body gets an AFL_ namespace so the
# library can never collide with hand-authored or other-vendor assets.
PREFIX_RULES = {
    # Lossless: keep the vendor's descriptive body, insert an AFL_ namespace
    # right after the type prefix. No token stripping -> no collisions, no
    # accidental double-words, and the original asset is always recognisable.
    "NS_": "NS_AFL_",   # Niagara systems
    "MI_": "MI_AFL_",   # Material instances (check BEFORE M_)
    "M_":  "M_AFL_",    # Materials (masters get a _Master suffix)
    "MF_": "MF_AFL_",   # Material functions
    "T_":  "T_AFL_",    # Textures
    "SM_": "SM_AFL_",   # Meshes
    "CC_": "CC_AFL_",   # Color curves
    "CA_": "CA_AFL_",   # Curve atlases
    "BP_": "BP_AFL_",   # Blueprints (REFERENCE ONLY)
}

# Folders whose contents are demo/showcase scaffolding, never shipped.
EXCLUDE_DIR_TOKENS = ("/Demo/", "/Map/", "/Cinematic/")

# Blueprints from the pack are reference-only: their tick-trace logic is the
# exact anti-pattern we are replacing. Flag, never import as gameplay assets.
REFERENCE_ONLY_PREFIXES = ("BP_",)


def classify(rel_path: str):
    p = rel_path.replace("\\", "/")
    name = os.path.splitext(os.path.basename(p))[0]
    ext = os.path.splitext(p)[1].lower()

    if any(tok in p for tok in EXCLUDE_DIR_TOKENS):
        return "EXCLUDE", name, ext
    if ext == ".umap":
        return "EXCLUDE", name, ext
    if ext not in (".uasset",):
        return "CONFIG/OTHER", name, ext

    for pre in REFERENCE_ONLY_PREFIXES:
        if name.startswith(pre):
            return "REFERENCE_ONLY", name, ext

    if name.startswith("NS_"):
        return "NIAGARA", name, ext
    if name.startswith("MI_"):
        return "MATERIAL_INSTANCE", name, ext
    if name.startswith("MF_"):
        return "MATERIAL_FUNCTION", name, ext
    if name.startswith("M_"):
        return "MATERIAL", name, ext
    if name.startswith("T_"):
        return "TEXTURE", name, ext
    if name.startswith("SM_"):
        return "MESH", name, ext
    if name.startswith(("CC_", "CA_")):
        return "CURVE", name, ext
    return "OTHER_UASSET", name, ext


def propose_name(name: str) -> str:
    # MI_ must be tested before M_ (prefix overlap).
    for pre in ("NS_", "MI_", "M_", "MF_", "T_", "SM_", "CC_", "CA_", "BP_"):
        if name.startswith(pre):
            new = PREFIX_RULES[pre] + name[len(pre):]
            # Base materials (not instances) get a _Master suffix for clarity.
            if pre == "M_" and not new.endswith("_Master"):
                new = new + "_Master"
            return new
    return name


def target_folder(category: str) -> str:
    base = "/AFLVFXLibrary/Laser"  # content-only plugin mount point
    return {
        "NIAGARA":            f"{base}/Niagara",
        "MATERIAL":           f"{base}/Materials",
        "MATERIAL_INSTANCE":  f"{base}/Materials",
        "MATERIAL_FUNCTION":  f"{base}/Materials/Functions",
        "TEXTURE":            f"{base}/Textures",
        "MESH":               f"{base}/Meshes",
        "CURVE":              f"{base}/Curves",
    }.get(category, f"{base}/_Unsorted")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("root", help="Unzipped pack root (contains Content/LaserFX_BP/)")
    ap.add_argument("--out", default=".", help="Output directory")
    args = ap.parse_args()

    root = Path(args.root)
    if not root.exists():
        sys.exit(f"Path not found: {root}")

    inventory = []
    counts = {}
    for dirpath, _dirs, files in os.walk(root):
        for f in files:
            full = Path(dirpath) / f
            rel = str(full.relative_to(root))
            cat, name, ext = classify(rel)
            counts[cat] = counts.get(cat, 0) + 1
            entry = {
                "category": cat,
                "name": name,
                "ext": ext,
                "rel_path": rel.replace("\\", "/"),
                "size_bytes": full.stat().st_size,
            }
            if cat in ("NIAGARA", "MATERIAL", "MATERIAL_INSTANCE",
                       "MATERIAL_FUNCTION", "TEXTURE", "MESH", "CURVE"):
                entry["proposed_name"] = propose_name(name)
                entry["proposed_folder"] = target_folder(cat)
            inventory.append(entry)

    outdir = Path(args.out)
    outdir.mkdir(parents=True, exist_ok=True)

    with open(outdir / "inventory.json", "w") as fh:
        json.dump({"counts": counts, "assets": inventory}, fh, indent=2)

    rename_rows = [a for a in inventory if "proposed_name" in a]
    with open(outdir / "rename_manifest.csv", "w", newline="") as fh:
        w = csv.writer(fh)
        w.writerow(["category", "old_name", "proposed_name",
                    "proposed_folder", "size_bytes", "source_rel_path"])
        for a in sorted(rename_rows, key=lambda x: (x["category"], x["name"])):
            w.writerow([a["category"], a["name"], a["proposed_name"],
                        a["proposed_folder"], a["size_bytes"], a["rel_path"]])

    # ---- stdout summary ----
    print("LASER PACK AUDIT")
    print("=" * 60)
    for cat in sorted(counts):
        print(f"  {cat:20s} {counts[cat]:>4d}")
    print("-" * 60)
    importable = sum(counts.get(c, 0) for c in
                     ("NIAGARA", "MATERIAL", "MATERIAL_INSTANCE",
                      "MATERIAL_FUNCTION", "TEXTURE", "MESH", "CURVE"))
    print(f"  IMPORTABLE (shipped) : {importable}")
    print(f"  EXCLUDE (demo/maps)  : {counts.get('EXCLUDE', 0)}")
    print(f"  REFERENCE_ONLY (BPs) : {counts.get('REFERENCE_ONLY', 0)}")
    print("=" * 60)
    print(f"Wrote {outdir/'inventory.json'} and {outdir/'rename_manifest.csv'}")


if __name__ == "__main__":
    main()
