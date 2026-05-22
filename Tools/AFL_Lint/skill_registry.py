#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
skill_registry.py - AFL-0218 skill registry CI check.

The skill registry is the authoritative list of AFL skills documented
in master doc Sec. 15.1. A PR is rejected if it references a skill
slug not in the registry - this catches typos like "afl-cpp-developer"
vs the real "afl-cpp-lyra-developer".

Two modes:

  python Tools/AFL_Lint/skill_registry.py --generate
      Regenerate Docs/SKILLS_REGISTRY.md from the hard-coded list
      below. Exit 0.

  python Tools/AFL_Lint/skill_registry.py --root .
      Scan .py/.md/.yaml/.yml files for `afl-<slug>` patterns and
      verify each match is a known AFL skill. Exit 0 on clean,
      1 on unknown-slug references.

  python Tools/AFL_Lint/skill_registry.py --self-test
      Regression guard. Builds a fixture tree with one good and one
      bad reference, asserts the bad one is flagged.

Scope notes:
  * The eleven skills in master doc Sec. 15.1 include three that do
    NOT carry the `afl-` prefix (lyra-skin-builder-marketplace,
    expert-game-designer, unreal-engine-expert). The registry lists
    all eleven, but the scanner only enforces slugs that match
    `afl-<lowercase-slug>` - the non-prefixed three are out of scope
    for the typo check.
  * The scanner is context-aware - a naive `afl-<slug>` regex would
    false-positive on CI job names (`afl-build/win64-dev`), workflow
    filenames (`afl-yolo-pr.yml`), git repo names (`afl-game`), and
    tool subdirectory names (`afl-yolo`). Only patterns that look
    like skill-name *references* are flagged:
      - YAML: items under a `skills:` list.
      - Markdown: backtick-wrapped identifiers (``afl-...``).
      - Python: string literals ("afl-..." or 'afl-...').
    This matches how skills are actually referenced in the master
    doc (Sec. 15.1 backticked-name conventions) and in queue.yaml
    (`skills:` block per task).
  * The registry doc and this script itself are exempt from the
    scan, since they enumerate every known slug as data.
"""

from __future__ import annotations

import argparse
import re
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path


# -----------------------------------------------------------------------------
# THE REGISTRY - hard-coded curation of master doc Sec. 15.1
# -----------------------------------------------------------------------------

@dataclass(frozen=True)
class Skill:
    slug: str
    discipline: str
    summary: str


# Eleven skills, in the same order as master doc Sec. 15.1. Edit this list
# and re-run `--generate` to update Docs/SKILLS_REGISTRY.md. Do not let the
# generated doc drift from this list by hand - the doc is a build output.
SKILLS: tuple[Skill, ...] = (
    Skill(
        "afl-cpp-lyra-developer",
        "C++ Engineering",
        "Lyra-correct C++ extension: GAS abilities, GameFeature plugins, "
        "AttributeSets, ExecCalcs. Enforces master doc Sec. 7/8/9.",
    ),
    Skill(
        "afl-neostack-task-writer",
        "Engineering / AI Workflow",
        "Writes high-quality AIK prompts that produce AFL-conformant "
        "Blueprints, Materials, Behavior Trees.",
    ),
    Skill(
        "afl-sprint-planner",
        "Production / Project Mgmt",
        "Decomposes features into AFL-XXXX tasks, writes sprint briefs, "
        "estimates effort, tracks blockers.",
    ),
    Skill(
        "afl-build-operator",
        "DevOps / Build",
        "UAT BuildCookRun pipelines, GitHub Actions, multi-platform "
        "packaging, dedicated server builds.",
    ),
    Skill(
        "afl-asset-pipeline",
        "Tech Art / Pipeline",
        "DCC tool export settings, FBX/USD import, LOD generation, "
        "texture compression per platform.",
    ),
    Skill(
        "afl-qa-build-recovery",
        "QA / DevOps",
        "Crash triage, broken-build recovery, regression matrices, "
        "platform cert (TRC/XR).",
    ),
    Skill(
        "afl-ui-hud-design",
        "UI Engineering",
        "Lyra CommonUI stack, UMG widgets, activatable widgets, "
        "multi-platform input routing.",
    ),
    Skill(
        "afl-blender-bridge",
        "Tech Art / 3D",
        "Blender<->UE5 round-trip via blender_mcp. Kitbash, retexture, "
        "dress, audit, AAA-clean modular assets. Composes with NeoStack "
        "genAI (Tripo, Meshy). Image-to-level blockouts. Heightmap "
        "landscapes.",
    ),
    Skill(
        "lyra-skin-builder-marketplace",
        "Character / Cosmetics",
        "Lyra-foundation reskinning pipeline. Mesh swap, IK retargeting "
        "to SK_Mannequin, modular character parts, in-game cosmetic "
        "marketplace, GameFeature plugins for live-ops skin drops, "
        "server-authoritative entitlement.",
    ),
    Skill(
        "expert-game-designer",
        "Game Design / Visual Direction",
        "Apple-Glass-inspired UI aesthetic, level/environment design "
        "direction, character/creature concept, Midjourney + NeoStack "
        "AIK prompt pipelines, design systems, palettes, typography.",
    ),
    Skill(
        "unreal-engine-expert",
        "C++ Engineering (general UE5)",
        "AAA-level general UE5 expertise: rendering (Lumen, Nanite, "
        "Niagara), gameplay systems, AI, networking, optimization, "
        "animation. Pairs with afl-cpp-lyra-developer for AFL-specific "
        "work.",
    ),
)


def _afl_slugs() -> frozenset[str]:
    """Subset of registry slugs that start with `afl-` (scan enforces these)."""
    return frozenset(s.slug for s in SKILLS if s.slug.startswith("afl-"))


# -----------------------------------------------------------------------------
# Scanner
# -----------------------------------------------------------------------------

# Bare slug pattern: `afl-` followed by a LETTER (so `afl-0218-...` task-ID
# branch names do not match) then more lowercase letters / digits / hyphens.
# Case-sensitive so `AFL-0218` task IDs are not false positives. The pattern
# is wrapped per file type below to scope each match to a real reference
# context.
SLUG_CORE = r"afl-[a-z][a-z0-9-]*[a-z0-9]"

# Markdown: skills are referenced via backticks, e.g. ``afl-cpp-lyra-developer``
# in master doc Sec. 15.1. Plain-text mentions ("afl-yolo orchestrator")
# are intentionally NOT flagged - those are tool/repo names, not skill refs.
MD_BACKTICK_RE = re.compile(r"`(" + SLUG_CORE + r")`")

# Python: string literals only. Catches "afl-cpp-developer" typos in code
# without firing on identifier names, docstring prose, or comments.
PY_STRLIT_RE = re.compile(r"""(['"])(""" + SLUG_CORE + r""")\1""")

# YAML: `skills:` block. Recognized by a `skills:` key followed by indented
# `- <slug>` lines. The block ends at the first non-blank line that is not
# indented deeper than the key. queue.yaml is the canonical case.
YAML_SKILLS_KEY_RE = re.compile(r"^(?P<indent>\s*)skills\s*:\s*$")
YAML_LIST_ITEM_RE = re.compile(r"^\s*-\s*['\"]?(" + SLUG_CORE + r")['\"]?\s*$")

# Files we scan. Anything else (.cpp/.h, .ini, .uasset, build logs) is out of
# scope - skill references live in code/config/docs, not engine assets.
SCAN_EXTS: frozenset[str] = frozenset({".py", ".md", ".yaml", ".yml"})

# Directories we never descend into - generated bytecode, build output, the
# engine's own source if it ever shows up.
EXEMPT_DIRS: frozenset[str] = frozenset({
    "__pycache__", ".git", "Binaries", "Intermediate", "Saved", "DerivedDataCache",
})

# Specific files that legitimately enumerate every slug (this script, the
# generated doc, the master doc's §15.1 skills table) - scanning them would
# be circular or noisy. The master doc is the source of truth so its
# backtick-enumeration of every skill is data, not a reference.
EXEMPT_FILES: tuple[str, ...] = (
    "Tools/AFL_Lint/skill_registry.py",
    "Docs/SKILLS_REGISTRY.md",
    "Docs/BAG_MAN_MASTER_BUILD_v2.0.md",
)

# Identifiers that share the `afl-` convention but are NOT skill names. These
# come from CI workflow file/job/check naming, the afl_yolo tooling, and the
# project's GitHub repos. They are listed here so that legitimate references
# to them (in code/config/docs) do not trip the typo check.
#
# Add to this list when a new CI job or tool acquires an `afl-` prefix.
# If you ever rename one of these to collide with a real skill slug, the
# registry takes precedence and the scanner will flag the collision.
KNOWN_NON_SKILL_IDENTIFIERS: frozenset[str] = frozenset({
    # GitHub Actions workflow names / files / status-check job names
    "afl-pr-build",
    "afl-yolo-pr",
    "afl-naming-lint",
    "afl-arch-lint",
    "afl-skill-registry",
    "afl-build",                # `afl-build/win64-dev` check name root
    # afl_yolo orchestrator tooling
    "afl-yolo",
    # Project GitHub repos
    "afl-game",
    "afl-game-assets",
    # Legacy `forbidden patterns` lint marker from older docs
    "afl-forbidden-patterns",
})


def _posix_rel(root: Path, p: Path) -> str:
    return p.resolve().relative_to(root.resolve()).as_posix()


def _iter_scan_files(root: Path):
    """Yield Paths under root with a SCAN_EXTS suffix, skipping exempt areas."""
    for p in root.rglob("*"):
        if not p.is_file():
            continue
        if p.suffix.lower() not in SCAN_EXTS:
            continue
        parts = set(p.parts)
        if parts & EXEMPT_DIRS:
            continue
        rel = _posix_rel(root, p)
        if rel in EXEMPT_FILES:
            continue
        yield p


def _scan_md(text: str) -> list[tuple[str, int]]:
    """Return (slug, line_number) for every ``afl-...`` backtick reference."""
    hits: list[tuple[str, int]] = []
    for m in MD_BACKTICK_RE.finditer(text):
        line = text.count("\n", 0, m.start()) + 1
        hits.append((m.group(1), line))
    return hits


def _scan_py(text: str) -> list[tuple[str, int]]:
    """Return (slug, line_number) for every `'afl-...'` / `"afl-..."` literal."""
    hits: list[tuple[str, int]] = []
    for m in PY_STRLIT_RE.finditer(text):
        line = text.count("\n", 0, m.start()) + 1
        hits.append((m.group(2), line))
    return hits


def _scan_yaml(text: str) -> list[tuple[str, int]]:
    """Return (slug, line_number) for every list item under a `skills:` block."""
    hits: list[tuple[str, int]] = []
    lines = text.splitlines()
    i = 0
    while i < len(lines):
        m = YAML_SKILLS_KEY_RE.match(lines[i])
        if not m:
            i += 1
            continue
        key_indent = len(m.group("indent"))
        j = i + 1
        while j < len(lines):
            line = lines[j]
            stripped = line.strip()
            if not stripped:
                j += 1
                continue
            cur_indent = len(line) - len(line.lstrip())
            # End of block: indentation drops to <= the `skills:` key's level
            # AND the line is not a list item (a list item at key+0 indent is
            # still in the block in some yaml styles, but we accept only items
            # that are MORE indented than the key to keep parsing simple).
            if cur_indent <= key_indent:
                break
            item_m = YAML_LIST_ITEM_RE.match(line)
            if item_m:
                hits.append((item_m.group(1), j + 1))
            else:
                # A non-list-item line inside the block ends the block - YAML
                # block scalars or sibling keys would not be valid skill refs.
                break
            j += 1
        i = j
    return hits


def scan_for_unknown_slugs(root: Path) -> list[str]:
    """Return one violation string per unknown afl-<slug> reference."""
    known = _afl_slugs()
    violations: list[str] = []
    for f in _iter_scan_files(root):
        try:
            text = f.read_text(encoding="utf-8", errors="ignore")
        except OSError:
            continue
        rel = _posix_rel(root, f)
        ext = f.suffix.lower()
        if ext == ".md":
            hits = _scan_md(text)
        elif ext == ".py":
            hits = _scan_py(text)
        elif ext in (".yaml", ".yml"):
            hits = _scan_yaml(text)
        else:
            continue
        for slug, line in hits:
            if slug in known:
                continue
            if slug in KNOWN_NON_SKILL_IDENTIFIERS:
                continue
            violations.append(
                f"{rel}:{line}: unknown skill '{slug}' - not in registry "
                f"(Docs/SKILLS_REGISTRY.md). Did you mean one of: "
                f"{', '.join(sorted(known))}?"
            )
    return violations


# -----------------------------------------------------------------------------
# Generator
# -----------------------------------------------------------------------------

REGISTRY_DOC_PATH = "Docs/SKILLS_REGISTRY.md"

_DOC_HEADER = """\
# AFL Skill Registry

**Generated** by `Tools/AFL_Lint/skill_registry.py --generate`. Do not edit
by hand - edit the `SKILLS` tuple in that script and re-run the generator.

This file is the authoritative list of skill slugs that may be referenced
in AFL source, configs, and docs. The CI job `afl-skill-registry` (AFL-0218)
rejects PRs that introduce a reference to a slug starting with `afl-` that
is not in this list.

Source of truth: master doc Sec. 15.1.

"""

_DOC_TABLE_HEADER = """\
## Skills

| # | Slug | Discipline | Summary |
|---|---|---|---|
"""

_DOC_FOOTER = """\

## Scope of the CI check

The scanner walks `.py`, `.md`, `.yaml`, and `.yml` files in the repo and
matches references to slugs starting with `afl-`. Only slugs prefixed
`afl-` are enforced - the three non-prefixed skills above
(`lyra-skin-builder-marketplace`, `expert-game-designer`,
`unreal-engine-expert`) are listed for completeness but are out of scope
for the typo check.

To avoid false positives on identifiers that share the `afl-` convention
without being skill references, the scanner matches only in
skill-reference contexts:

- **YAML**: items under a `skills:` list (the canonical orchestrator case
  in `Tools/AFL_Yolo/queue.yaml`).
- **Markdown**: backtick-wrapped identifiers such as
  `` `afl-cpp-lyra-developer` ``.
- **Python**: string literals such as `"afl-cpp-lyra-developer"`.

Additionally, a small allow-list of known non-skill `afl-`-prefixed
identifiers (CI job names like `afl-naming-lint`, the `afl-yolo`
orchestrator package, repo names like `afl-game`) is excluded from
the typo check - see `KNOWN_NON_SKILL_IDENTIFIERS` in the scanner.

Task IDs like `AFL-0218` are uppercase and do not match the scanner's
case-sensitive pattern. Lowercase branch names of the form
`afl-NNNN-...` are excluded because the scanner requires the first
character after `afl-` to be a letter.

## Adding or renaming a skill

1. Edit `SKILLS` in `Tools/AFL_Lint/skill_registry.py`.
2. Run `python Tools/AFL_Lint/skill_registry.py --generate` to refresh
   this file.
3. Commit both changes together. The CI check will then accept the new
   slug everywhere in the repo.
"""


def render_registry_md() -> str:
    """Build the SKILLS_REGISTRY.md contents from the SKILLS tuple."""
    rows: list[str] = []
    for i, s in enumerate(SKILLS, start=1):
        summary = s.summary.replace("|", "\\|")
        rows.append(f"| {i} | `{s.slug}` | {s.discipline} | {summary} |")
    return _DOC_HEADER + _DOC_TABLE_HEADER + "\n".join(rows) + "\n" + _DOC_FOOTER


def write_registry_doc(root: Path) -> Path:
    """Write Docs/SKILLS_REGISTRY.md under root. Returns the written path."""
    out = root / REGISTRY_DOC_PATH
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(render_registry_md(), encoding="utf-8")
    return out


# -----------------------------------------------------------------------------
# Self-test
# -----------------------------------------------------------------------------

def run_self_test() -> int:
    print("AFL-0218 self-test: building fixture tree...")
    with tempfile.TemporaryDirectory(prefix="afl0218_selftest_") as td:
        root = Path(td)

        # YAML scope: queue.yaml-style skills: block with one known slug, one
        # typo (must fire), and one unrelated `branch:` key with an afl-NNNN
        # branch name that must NOT fire (outside skills: block).
        (root / "Tools").mkdir()
        (root / "Tools" / "queue.yaml").write_text(
            "tasks:\n"
            "- id: AFL-9999\n"
            "  branch: yolo/afl-9999-some-task\n"   # NOT in skills: block - ignore
            "  skills:\n"
            "    - afl-cpp-lyra-developer\n"        # ok
            "    - afl-cpp-developer\n"             # VIOLATION (typo)
            "    - afl-build-operator\n"            # ok
            "  files_hint:\n"
            "    - some/path.cpp\n",                # post-block, ignore
            encoding="utf-8",
        )

        # Markdown scope: only backticked references are flagged. Backticked
        # identifiers that are known non-skill (CI jobs, repos, tools) are
        # exempt via KNOWN_NON_SKILL_IDENTIFIERS so they do not false-positive.
        (root / "Docs").mkdir()
        (root / "Docs" / "notes.md").write_text(
            "## Build pipeline\n"
            "Job `afl-naming-lint` runs in workflow `afl-pr-build`.\n"   # exempt
            "Required check: afl-build/win64-dev.\n"
            "Repo: github.com/example/afl-game (also see `afl-game`).\n" # exempt
            "Task AFL-0218 on branch yolo/afl-0218-skill-registry.\n"
            "\n"
            "## Skill assignments\n"
            "Owner: `afl-build-operator`.\n"           # ok (known skill)
            "Reviewer: `afl-cpp-developer`.\n",        # VIOLATION (typo)
            encoding="utf-8",
        )

        # Python scope: only string literals are flagged. Comments and
        # identifier names are exempt by construction. The string literal
        # 'afl-yolo' is a known-non-skill identifier so it must NOT fire.
        (root / "agent.py").write_text(
            "# afl-yolo is the orchestrator package - not a skill\n"
            "SKILL_NAME = 'afl-cpp-lyra-developer'\n"   # ok (known skill)
            "FALLBACK   = \"afl-ghost-skill\"\n"        # VIOLATION (unknown)
            "PACKAGE    = 'afl-yolo'\n",                # ok (known non-skill)
            encoding="utf-8",
        )

        # Exempt files: even an exact typo here must NOT fire.
        (root / "Docs" / "SKILLS_REGISTRY.md").write_text(
            "Generated. Mentions `afl-cpp-developer` literally as data.\n",
            encoding="utf-8",
        )

        violations = scan_for_unknown_slugs(root)
        print(f"AFL-0218 self-test: {len(violations)} violation(s).")
        for v in violations:
            print(f"  {v}")

        # Expected hits - each must appear in the violation list.
        must_flag = {
            "Tools/queue.yaml": "afl-cpp-developer",       # yaml skills: typo
            "Docs/notes.md":    "afl-cpp-developer",       # md backtick typo
            "agent.py":         "afl-ghost-skill",         # py string literal
        }
        for src, slug in must_flag.items():
            if not any(src in v and slug in v for v in violations):
                print(f"FAIL: expected violation not raised for {src} -> {slug}")
                return 1

        # Expected non-hits - none of these should appear as a violation SOURCE.
        sources = [v.split(":", 2)[0] for v in violations]
        forbidden_substrings = [
            "AFL-9999",                  # uppercase task ID in yaml
            "afl-9999",                  # lowercase branch name (NOT in skills:)
            "afl-naming-lint",           # md plain-text job name (no backticks)
            "afl-pr-build.yml",          # md filename mention
            "afl-build/win64-dev",       # md plain-text status check
            "afl-game",                  # md plain-text repo name
            "afl-0218-skill-registry",   # md plain-text branch name
        ]
        for v in violations:
            for s in forbidden_substrings:
                if s in v and "afl-cpp-developer" not in v and "afl-ghost-skill" not in v:
                    print(f"FAIL: unexpected violation contains '{s}': {v}")
                    return 1
        if "Docs/SKILLS_REGISTRY.md" in sources:
            print("FAIL: exempt file Docs/SKILLS_REGISTRY.md was scanned")
            return 1

        # Render sanity: the doc must list every slug, in order, and the
        # generator must be idempotent (two runs produce the same text).
        out1 = render_registry_md()
        out2 = render_registry_md()
        if out1 != out2:
            print("FAIL: render_registry_md is not deterministic")
            return 1
        for s in SKILLS:
            if f"`{s.slug}`" not in out1:
                print(f"FAIL: rendered doc missing slug '{s.slug}'")
                return 1

    print("AFL-0218 self-test: PASS - scanner and generator both healthy.")
    return 0


# -----------------------------------------------------------------------------
# CLI
# -----------------------------------------------------------------------------

def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(
        prog="skill_registry",
        description="AFL-0218 skill registry CI check. Scans the repo for "
                    "`afl-<slug>` references and verifies every slug appears "
                    "in the curated registry. Use --generate to refresh "
                    "Docs/SKILLS_REGISTRY.md from the hard-coded list.",
    )
    p.add_argument(
        "--root", type=Path, default=Path("."),
        help="Repo root to scan or write into (default: current directory).",
    )
    p.add_argument(
        "--generate", action="store_true",
        help="Write Docs/SKILLS_REGISTRY.md from the hard-coded SKILLS list "
             "and exit. Does not run the scan.",
    )
    p.add_argument(
        "--self-test", action="store_true",
        help="Regression guard: build a fixture tree containing one valid "
             "reference, one typo, and one exempt-file mention; assert the "
             "scanner flags the typo and only the typo.",
    )
    args = p.parse_args(argv)

    if args.self_test:
        return run_self_test()

    root: Path = args.root
    if not root.is_dir():
        print(f"error: --root {root} is not a directory", file=sys.stderr)
        return 2

    if args.generate:
        out = write_registry_doc(root)
        print(f"AFL-0218: regenerated {_posix_rel(root, out)} "
              f"({len(SKILLS)} skills).")
        return 0

    violations = scan_for_unknown_slugs(root)
    if violations:
        print(f"AFL-0218: {len(violations)} unknown skill reference(s):")
        for v in violations:
            print(f"  {v}")
        return 1

    print(f"AFL-0218: clean - all afl-<slug> references map to a known skill "
          f"({len(_afl_slugs())} afl-prefixed slugs in registry).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
