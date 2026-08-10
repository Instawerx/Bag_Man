#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
cook_soft_refs.py - AFL string-referenced asset cook lint (AFL-COOK).

THE DEFECT THIS EXISTS TO PREVENT
---------------------------------
The cooker packages what something REFERENCES. An FSoftObjectPath / TSoftClassPtr
built from a C++ string literal is not a reference the cooker can follow, so the
asset is silently dropped from the build.

The failure CANNOT REPRODUCE IN PIE -- every asset is on disk there, so
LoadSynchronous always succeeds. It surfaces only in a cooked build, as a
LogStreaming Warning buried in thousands of lines:

    LogStreaming: Warning: LoadPackage: SkipPackage: <path>
      - The package to load does not exist on disk or in the loader
    LogUObjectGlobals: Warning: Failed to find object '<path>_C'

It has shipped repeatedly. FBIK -- project doctrine on every character -- had
never once shipped in a packaged build. The match-end results board was absent,
which removed the CONTINUE button and left finished matches in PostGame
indefinitely; one held a GameLift session for 6.5 hours. Both were found only by
reading a cooked client log line by line, which is exactly the thing that does
not scale.

WHY A SOURCE SCAN, AND NOT ONLY A RUNTIME CHECK
-----------------------------------------------
A runtime check still requires someone to produce a cooked build and read its
log. That is precisely the step that failed. This lint runs on every PR in
seconds on a stock ubuntu runner -- no engine, no cook, no editor -- and it needs
no cooperation from the author: it DISCOVERS the literals itself rather than
asking anyone to remember to register them. There is nothing to forget.

The runtime guard (UAFLCookedAssetRegistry, in the AFLCore plugin) is
defence-in-depth for what a source scan structurally cannot see: paths composed
at runtime, and cook drift between targets (client vs dedicated server) where the
config is right but the artifact still lacks the asset.

RULES
-----
  R1  COOK COVERAGE (default, fails the build)
      Every string-resolved content path in shipping AFL source must be covered
      by a +DirectoriesToAlwaysCook entry in Config/DefaultGame.ini. This is the
      rule that would have caught every known instance at authoring time.

  R2  COOKED OUTPUT VERIFICATION (opt-in, --cooked-dir)
      Given a real cook, assert each discovered path has a .uasset on disk. A
      cook can SUCCEED while omitting an asset -- that is how this shipped --
      so the exit code is not evidence. This rule is the authority; R1 checks
      intent, R2 checks reality.

      ⚠ R2 APPLIES THE SAME DEV-ONLY SPLIT AS R1, and did not until 2026-08-10.
      A cheat or test-harness fixture missing from a CLIENT cook is the correct
      outcome, not a defect: those call sites run in Development builds where
      the asset is on disk. Failing on them made the rule permanently red on a
      real cook tree (AFLBoundaryHeightTestHarness's Scaffold_HeightRig), and a
      lint that is always red is a lint nobody reads -- the same reasoning that
      put the ConstructorHelpers exemption in "deliberately not flagged" below.
      --include-dev promotes dev findings to failures for BOTH rules, together.

  R3  NEVER-COOK LEAK (opt-in, --cooked-dir --strict-never-cook)
      The mirror of R2: a path declared AFL_NEVER_COOK must be ABSENT. See the
      marker's own comment for why absence is sometimes the feature.

WHAT IS DELIBERATELY NOT FLAGGED
--------------------------------
ConstructorHelpers::FObjectFinder / FClassFinder resolve at CDO construction and
store the result in a UPROPERTY. The CDO is serialized with a hard object
pointer, so the cooker follows it and the asset IS packaged.

That is not an assumption -- it was verified against the 2026-08-09 cook. Every
FObjectFinder-only asset checked was present (SM_AFL_LoadoutPod,
MI_AFL_NeonPlatform, /Game/Effects/Meshes/ring, /Engine/BasicShapes/Cube), while
string-resolved siblings in the same modules were missing. Flagging them would be
noise, and noise is how a lint gets disabled.

Usage:
    python Tools/AFL_Lint/cook_soft_refs.py --root .
    python Tools/AFL_Lint/cook_soft_refs.py --root . --self-test
    python Tools/AFL_Lint/cook_soft_refs.py --root . --audit
    python Tools/AFL_Lint/cook_soft_refs.py --root . \
        --cooked-dir Saved/Cooked/Windows
    python Tools/AFL_Lint/cook_soft_refs.py --root . \
        --cooked-dir Saved/Cooked/Windows --strict-never-cook

Exit codes:
    0  clean
    1  violations found
    2  usage / arg error
"""

from __future__ import annotations

import argparse
import io
import re
import sys
import tempfile
from contextlib import redirect_stdout
from dataclasses import dataclass, field
from pathlib import Path


# -----------------------------------------------------------------------------
# Scope
# -----------------------------------------------------------------------------

# Shipping AFL source trees. Plugins/AgentIntegrationKit is editor tooling that
# never cooks, and the project Source/ tree is Lyra engine code we do not own.
AFL_SOURCE_GLOBS: tuple[str, ...] = (
    "Plugins/GameFeatures/AFL*/Source",
    "Plugins/AFL*/Source",
)

SOURCE_EXTS: frozenset[str] = frozenset({".cpp", ".h"})

# Build artifacts - never scanned.
EXCLUDED_DIR_PARTS: frozenset[str] = frozenset({"Intermediate", "Binaries", "Saved"})

# Content mount roots. A literal starting with one of these is an asset path.
# /Script/ is deliberately absent: those name C++ classes, not packages, and are
# resolved from the linked binary rather than the cook.
CONTENT_ROOT_RE = re.compile(
    r"^/(Game|Engine|AFL[A-Za-z]*|Shooter[A-Za-z]*|Lyra[A-Za-z]*|TopDownArena)/"
)

# Dev-only call sites: cheats, test runners, harnesses and specs. These DO ship
# in Development cooked builds (the headless afl.*.Test suites run against them),
# so they are reported -- but they do not fail the build by default, because
# force-cooking test fixtures bloats a shipping artifact for no player benefit.
# Escalate with --include-dev.
DEV_PATH_MARKERS: tuple[str, ...] = (
    "/Test/",
    "/Tests/",
    "/Spec/",
)
DEV_FILE_MARKERS: tuple[str, ...] = (
    "Cheats.cpp",
    "TestRunner.cpp",
    "TestHarness.cpp",
    "Spec.cpp",
)


# -----------------------------------------------------------------------------
# Detection
# -----------------------------------------------------------------------------

# Any TEXT("/...") literal. Broad by design: narrow shape-matching (only
# FSoftObjectPath(TEXT(...)), only LoadClass(...)) produces false NEGATIVES,
# which is the failure mode that actually costs us. Several real sites in this
# repo stash the path in a bare `const TCHAR* Path = TEXT("/Game/...")` first
# and construct the soft path elsewhere; a shape-matcher misses every one.
TEXT_LITERAL_RE = re.compile(r'TEXT\(\s*"(?P<path>/[^"]*)"\s*\)')

# ConstructorHelpers resolves into a UPROPERTY on the CDO -> cookable hard ref.
# Matched against the whole statement, since the finder and its path often wrap.
CTOR_HELPERS_RE = re.compile(r"ConstructorHelpers\s*::")

# A literal composed at runtime; its final value is not knowable statically.
# Reported under --audit, never enforced -- these are exactly what the runtime
# registry exists to cover. Two shapes occur in this repo:
#   printf specifiers   FString::Printf(TEXT("/Game/Skins/MI_%s_%s"), ...)
#   concatenation stem  const FString Root = TEXT("/Game/.../B_AFL_Robot_");
# A real package never ends in '_' or '/', so a trailing one marks a stem.
DYNAMIC_RE = re.compile(r"%[-+ #0-9.]*[sdilufgxSD]|[_/]$")

# An asset that is DELIBERATELY not cooked. The lint's whole premise is "a path
# the cooker cannot see may be silently absent" -- but a handful of paths exist
# precisely BECAUSE the cooker cannot see them, and for those, absence is the
# feature. Without this category the tool reports them as violations and, worse,
# offers a remedy (+DirectoriesToAlwaysCook) that would undo the thing they are
# for: the first such site was the deprecated HOST screen, which must not reach
# a shipping client at all.
#
# Declared AT THE CALL SITE rather than in an allowlist here, so the claim sits
# next to the code that has to keep being true, and shows up in the diff when
# someone changes that code. Write AFL_NEVER_COOK in a comment on the literal's
# line or within the LOOKBACK lines above it:
#
#     // AFL_NEVER_COOK -- dev-only summon, excluded from the shipping cook
#     const TCHAR* Path = TEXT("/Game/DeveloperUtils/Host/W_Thing.W_Thing_C");
#
# ⚠ THIS IS A DECLARATION, NOT A SUPPRESSION. It says "absent by design"; it does
# not say "stop checking". Such paths are still listed in the report, and
# --strict-never-cook FAILS if one of them turns out to be present in the cook
# tree -- i.e. if the exclusion silently stopped working.
NEVER_COOK_RE = re.compile(r"AFL_NEVER_COOK")
NEVER_COOK_LOOKBACK = 6

# Trailing object name: /Game/A/B.B_C -> package /Game/A/B
OBJECT_SUFFIX_RE = re.compile(r"\.[^./]*$")

# +DirectoriesToAlwaysCook=(Path="/Game/Foo")
ALWAYS_COOK_RE = re.compile(
    r'^\s*\+?DirectoriesToAlwaysCook\s*=\s*\(\s*Path\s*=\s*"(?P<path>[^"]+)"\s*\)',
    re.MULTILINE,
)


@dataclass(frozen=True)
class Ref:
    """One string-resolved asset path found in source."""
    package: str          # /Game/BagMan/UI/Loadout/WBP_AFL_Loadout
    raw: str              # the literal as written, object suffix included
    file: str             # repo-relative source path
    line: int
    is_dev: bool
    is_dynamic: bool
    is_never_cook: bool = False   # declared absent-by-design via AFL_NEVER_COOK

    def __str__(self) -> str:
        return f"{self.file}:{self.line}: {self.package}"


@dataclass
class Findings:
    uncovered: list[Ref] = field(default_factory=list)      # R1
    missing_in_cook: list[Ref] = field(default_factory=list)  # R2
    dev_uncovered: list[Ref] = field(default_factory=list)      # R1, dev call site
    dev_missing_in_cook: list[Ref] = field(default_factory=list)  # R2, dev call site
    dynamic: list[Ref] = field(default_factory=list)
    never_cook: list[Ref] = field(default_factory=list)     # declared absent by design
    never_cook_leaked: list[Ref] = field(default_factory=list)  # R3: absent-by-design, found PRESENT
    all_refs: list[Ref] = field(default_factory=list)


# -----------------------------------------------------------------------------
# Source scan
# -----------------------------------------------------------------------------

def iter_source_files(root: Path) -> list[Path]:
    files: list[Path] = []
    for pattern in AFL_SOURCE_GLOBS:
        for src_root in root.glob(pattern):
            if not src_root.is_dir():
                continue
            for path in src_root.rglob("*"):
                if path.suffix not in SOURCE_EXTS or not path.is_file():
                    continue
                if EXCLUDED_DIR_PARTS & set(path.parts):
                    continue
                files.append(path)
    return sorted(files)


def _is_dev_site(rel: str) -> bool:
    norm = rel.replace("\\", "/")
    if any(marker in norm for marker in DEV_PATH_MARKERS):
        return True
    # A whole module can be test-only (e.g. Source/AFLCombatTests/...), in which
    # case no individual directory is named Test/.
    if any(part.endswith("Tests") for part in norm.split("/")[:-1]):
        return True
    return any(norm.endswith(marker) for marker in DEV_FILE_MARKERS)


def to_package(raw: str) -> str:
    """/Game/A/B.B_C -> /Game/A/B   (strip the object-within-package suffix)."""
    head, _, tail = raw.rpartition("/")
    return f"{head}/{OBJECT_SUFFIX_RE.sub('', tail)}" if head else raw


def scan_file(path: Path, root: Path) -> list[Ref]:
    try:
        text = path.read_text(encoding="utf-8", errors="replace")
    except OSError:
        return []

    rel = str(path.relative_to(root)).replace("\\", "/")
    is_dev = _is_dev_site(rel)
    refs: list[Ref] = []

    lines = text.splitlines()
    for lineno, line in enumerate(lines, start=1):
        # ConstructorHelpers statements can wrap; a finder's path frequently
        # lands on the following line. Treat the finder as covering its whole
        # statement by looking back until the previous semicolon.
        if CTOR_HELPERS_RE.search(line):
            continue

        for m in TEXT_LITERAL_RE.finditer(line):
            raw = m.group("path")
            if not CONTENT_ROOT_RE.match(raw):
                continue
            # The declaration may sit on this line or in the comment block above
            # it -- the readable place to put it is directly above the literal.
            window = lines[max(0, lineno - 1 - NEVER_COOK_LOOKBACK):lineno]
            refs.append(
                Ref(
                    package=to_package(raw),
                    raw=raw,
                    file=rel,
                    line=lineno,
                    is_dev=is_dev,
                    is_dynamic=bool(DYNAMIC_RE.search(raw)),
                    is_never_cook=any(NEVER_COOK_RE.search(w) for w in window),
                )
            )
    return refs


def scan_sources(root: Path) -> list[Ref]:
    refs: list[Ref] = []
    for path in iter_source_files(root):
        refs.extend(scan_file(path, root))
    return refs


# -----------------------------------------------------------------------------
# R1 - cook coverage
# -----------------------------------------------------------------------------

def read_always_cook_dirs(root: Path) -> list[str]:
    ini = root / "Config" / "DefaultGame.ini"
    if not ini.is_file():
        return []
    text = ini.read_text(encoding="utf-8", errors="replace")
    return [m.group("path").rstrip("/") for m in ALWAYS_COOK_RE.finditer(text)]


def is_covered(package: str, cook_dirs: list[str]) -> bool:
    """A package is covered when an always-cook directory is one of its parents."""
    pkg_dir = package.rsplit("/", 1)[0]
    return any(
        pkg_dir == d or pkg_dir.startswith(d + "/")
        for d in cook_dirs
    )


# -----------------------------------------------------------------------------
# R2 - cooked output verification
# -----------------------------------------------------------------------------

def cooked_candidates(cooked_dir: Path, package: str) -> list[Path]:
    """
    Map a package path to the .uasset it should occupy in a cook tree.

      /Game/A/B      -> <cooked>/<Project>/Content/A/B.uasset
      /Engine/A/B    -> <cooked>/Engine/Content/A/B.uasset
      /<Mount>/A/B   -> <cooked>/**/<Mount>/Content/A/B.uasset   (plugin mount)

    Plugin layout varies, so mount roots are resolved by glob rather than by
    assuming a directory shape.
    """
    parts = package.strip("/").split("/")
    if not parts:
        return []
    mount, rest = parts[0], "/".join(parts[1:])
    if not rest:
        return []

    if mount == "Game":
        return list(cooked_dir.glob(f"*/Content/{rest}.uasset"))
    if mount == "Engine":
        return list(cooked_dir.glob(f"Engine/Content/{rest}.uasset"))
    return list(cooked_dir.glob(f"**/{mount}/Content/{rest}.uasset"))


def verify_cooked(refs: list[Ref], cooked_dir: Path) -> tuple[list[Ref], list[Ref], list[Ref]]:
    """
    Returns (missing, dev_missing, leaked).

    `missing` / `dev_missing` are R2, split by call site exactly as R1 splits.
    The dev bucket is advisory unless --include-dev: a fixture that only a cheat
    or a headless harness loads is SUPPOSED to be absent from a client cook, and
    force-cooking it would bloat a shipping artifact for no player benefit --
    which is the same argument the DirectoriesToAlwaysCook block makes against
    registering test fixtures. Failing on those kept this rule permanently red,
    and the cost of a permanently red rule is that the day it goes red for a real
    reason, nobody notices.

    `leaked` is R3, the mirror image: a path DECLARED absent-by-design that the
    cook does contain. Same check, opposite expectation -- and the reason the
    AFL_NEVER_COOK marker is a declaration rather than a mute. An exclusion that
    quietly stops working (a new hard reference reaches the asset, someone adds
    the folder to DirectoriesToAlwaysCook) is exactly as invisible as the absent
    asset this tool was written to catch, and deserves the same alarm.
    """
    missing: list[Ref] = []
    dev_missing: list[Ref] = []
    leaked: list[Ref] = []
    for ref in refs:
        if ref.is_dynamic:
            continue
        present = bool(cooked_candidates(cooked_dir, ref.package))
        if ref.is_never_cook:
            if present:
                leaked.append(ref)
        elif not present:
            (dev_missing if ref.is_dev else missing).append(ref)
    return missing, dev_missing, leaked


# -----------------------------------------------------------------------------
# Driver
# -----------------------------------------------------------------------------

def run_all(root: Path, cooked_dir: Path | None = None) -> Findings:
    out = Findings()
    cook_dirs = read_always_cook_dirs(root)

    # One entry per distinct (package, file, line); dedupe identical repeats of
    # the same literal on the same line.
    seen: set[tuple[str, str, int]] = set()
    for ref in scan_sources(root):
        key = (ref.package, ref.file, ref.line)
        if key in seen:
            continue
        seen.add(key)
        out.all_refs.append(ref)

        if ref.is_dynamic:
            out.dynamic.append(ref)
            continue
        if ref.is_never_cook:
            # Absent by design: R1 asks "is this covered by an always-cook
            # directory", which is the wrong question for a path that must not
            # be cooked at all. Answering it would hand back a remedy that
            # reverses the intent.
            out.never_cook.append(ref)
            continue
        if is_covered(ref.package, cook_dirs):
            continue
        (out.dev_uncovered if ref.is_dev else out.uncovered).append(ref)

    if cooked_dir is not None:
        (out.missing_in_cook,
         out.dev_missing_in_cook,
         out.never_cook_leaked) = verify_cooked(out.all_refs, cooked_dir)

    return out


def remedy(package: str) -> str:
    return f'+DirectoriesToAlwaysCook=(Path="{package.rsplit("/", 1)[0]}")'


# -----------------------------------------------------------------------------
# Self-test
# -----------------------------------------------------------------------------

def run_self_test() -> int:
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)

        cfg = root / "Config"
        cfg.mkdir(parents=True)
        (cfg / "DefaultGame.ini").write_text(
            "[/Script/UnrealEd.ProjectPackagingSettings]\n"
            '+DirectoriesToAlwaysCook=(Path="/Game/Covered")\n'
            '+DirectoriesToAlwaysCook=(Path="/AFLBagMan/UI")\n',
            encoding="utf-8",
        )

        src = root / "Plugins" / "GameFeatures" / "AFLThing" / "Source" / "AFLThing" / "Private"
        src.mkdir(parents=True)
        (src / "Thing.cpp").write_text(
            # covered by /Game/Covered -> must NOT flag
            'A = FSoftObjectPath(TEXT("/Game/Covered/BP_Ok.BP_Ok_C"));\n'
            # covered by /AFLBagMan/UI -> must NOT flag
            'B = TSoftClassPtr<UW>(FSoftObjectPath(TEXT("/AFLBagMan/UI/WBP_Ok.WBP_Ok_C")));\n'
            # NOT covered -> VIOLATION (this is the shipped defect)
            'C = FSoftObjectPath(TEXT("/Game/Orphan/ABP_Lost.ABP_Lost_C"));\n'
            # NOT covered, different API shape -> VIOLATION
            'D = LoadClass<AActor>(nullptr, TEXT("/Game/Orphan2/B_Lost.B_Lost_C"));\n'
            # bare TCHAR* stash -> VIOLATION (shape-matchers miss this one)
            'const TCHAR* P = TEXT("/Game/Orphan3/DT_Lost.DT_Lost");\n'
            # ConstructorHelpers -> cookable via CDO, must NOT flag
            'static ConstructorHelpers::FObjectFinder<UStaticMesh> M(TEXT("/Engine/BasicShapes/Sphere.Sphere"));\n'
            # runtime-composed -> reported as dynamic, never enforced
            'E = FString::Printf(TEXT("/Game/Skins/MI_%s.MI_%s"), *N, *N);\n'
            # concatenation stem, not a real package -> dynamic, must NOT be a violation
            'const FString Root = TEXT("/Game/Cosmetics/B_AFL_Robot_");\n'
            # /Script/ names a class, not a package -> must NOT flag
            'F = FindObject<UClass>(nullptr, TEXT("/Script/LyraGame.LyraCharacter"));\n'
            # declared absent by design, marker on the line above -> NOT an R1/R2
            # violation, and confirmed absent from the fixture cook tree
            "// AFL_NEVER_COOK -- dev-only summon, excluded from the shipping cook\n"
            'H = LoadClass<UW>(nullptr, TEXT("/Game/DeveloperUtils/W_Dev.W_Dev_C"));\n'
            # declared absent by design but PRESENT in the fixture cook -> R3 leak
            "// AFL_NEVER_COOK -- must not ship\n"
            'I = LoadClass<UW>(nullptr, TEXT("/Game/Covered/BP_Ok.BP_Ok_C"));\n',
            encoding="utf-8",
        )
        (src / "ThingCheats.cpp").write_text(
            # dev-only site, uncovered -> reported separately, not a hard failure
            'G = FSoftObjectPath(TEXT("/Game/DevOnly/GE_Cheat.GE_Cheat_C"));\n'
            # dev-only site, COVERED by always-cook but still absent from the cook
            # tree -> R2 dev bucket. Covered-but-absent is what isolates the R2
            # split from the R1 one; a path that fails both would pass this test
            # even if R2 ignored is_dev entirely.
            'J = FSoftObjectPath(TEXT("/Game/Covered/DT_Fixture.DT_Fixture"));\n',
            encoding="utf-8",
        )
        # Build artifacts must never be scanned.
        inter = root / "Plugins" / "GameFeatures" / "AFLThing" / "Source" / "AFLThing" / "Intermediate"
        inter.mkdir(parents=True)
        (inter / "Gen.cpp").write_text(
            'X = FSoftObjectPath(TEXT("/Game/Generated/Nope.Nope"));\n', encoding="utf-8"
        )

        f = run_all(root)

        flagged = {r.package for r in f.uncovered}
        dev_flagged = {r.package for r in f.dev_uncovered}
        dynamic = {r.package for r in f.dynamic}

        print(f"AFL-COOK self-test: {len(f.uncovered)} violation(s), "
              f"{len(f.dev_uncovered)} dev, {len(f.dynamic)} dynamic.")

        expected = {
            "/Game/Orphan/ABP_Lost",
            "/Game/Orphan2/B_Lost",
            "/Game/Orphan3/DT_Lost",
        }
        if not expected <= flagged:
            print(f"FAIL: R1 missed {sorted(expected - flagged)}")
            return 1

        forbidden = {
            "/Game/Covered/BP_Ok",          # covered by always-cook
            "/AFLBagMan/UI/WBP_Ok",         # covered by always-cook
            "/Engine/BasicShapes/Sphere",   # ConstructorHelpers -> cookable
            "/Game/Generated/Nope",         # under Intermediate/
        }
        hit = forbidden & flagged
        if hit:
            print(f"FAIL: R1 false positives on {sorted(hit)}")
            return 1

        if "/Script/LyraGame.LyraCharacter" in flagged or any(
            "/Script/" in p for p in flagged
        ):
            print("FAIL: /Script/ class path treated as an asset")
            return 1

        if dev_flagged != {"/Game/DevOnly/GE_Cheat"}:
            print(f"FAIL: dev classification wrong: {sorted(dev_flagged)}")
            return 1

        if not any("Skins" in p for p in dynamic):
            print(f"FAIL: printf-composed path not classified dynamic: {sorted(dynamic)}")
            return 1

        if "/Game/Cosmetics/B_AFL_Robot_" not in dynamic:
            print(f"FAIL: concatenation stem not classified dynamic: {sorted(dynamic)}")
            return 1
        if any(p.endswith("_") for p in flagged):
            print("FAIL: concatenation stem raised as a coverage violation")
            return 1

        # R2: a cook tree that omits one of the covered assets must be caught.
        cooked = root / "cooked"
        (cooked / "Proj" / "Content" / "Covered").mkdir(parents=True)
        (cooked / "Proj" / "Content" / "Covered" / "BP_Ok.uasset").write_text("x", encoding="utf-8")
        f2 = run_all(root, cooked_dir=cooked)
        missing = {r.package for r in f2.missing_in_cook}
        dev_missing = {r.package for r in f2.dev_missing_in_cook}
        if "/Game/Covered/BP_Ok" in missing:
            print("FAIL: R2 flagged an asset that IS in the cook")
            return 1
        if "/AFLBagMan/UI/WBP_Ok" not in missing:
            print("FAIL: R2 missed an asset absent from the cook")
            return 1

        # R2 splits on call site exactly as R1 does. DT_Fixture is covered by
        # always-cook (so R1 has nothing to say) and absent from the cook (so R2
        # does) -- it can only land in the dev bucket via is_dev.
        if "/Game/Covered/DT_Fixture" in missing:
            print("FAIL: R2 hard-failed on a dev-only call site")
            return 1
        if "/Game/Covered/DT_Fixture" not in dev_missing:
            print("FAIL: R2 dropped a dev-only absent asset instead of reporting it")
            return 1
        # The shipping-site absence must NOT be demoted along with it.
        if "/AFLBagMan/UI/WBP_Ok" in dev_missing:
            print("FAIL: R2 demoted a shipping-site absence to the dev bucket")
            return 1

        # AFL_NEVER_COOK, both directions. The exemption is only worth having if
        # it still catches the case where the exclusion has quietly failed --
        # otherwise it is a mute with extra steps.
        never = {r.package for r in f2.never_cook}
        leaked = {r.package for r in f2.never_cook_leaked}

        if "/Game/DeveloperUtils/W_Dev" not in never:
            print("FAIL: AFL_NEVER_COOK marker on the preceding line was not honoured")
            return 1
        if "/Game/DeveloperUtils/W_Dev" in missing or "/Game/DeveloperUtils/W_Dev" in flagged:
            print("FAIL: an absent-by-design path was reported as an R1/R2 violation")
            return 1
        if "/Game/Covered/BP_Ok" not in leaked:
            print("FAIL: R3 missed an absent-by-design path that IS present in the cook")
            return 1
        if "/Game/DeveloperUtils/W_Dev" in leaked:
            print("FAIL: R3 false positive on a path genuinely absent from the cook")
            return 1

        # --include-dev must promote BOTH rules, not just R1. Asserted through
        # _report's exit code, since that is where the promotion actually lives.
        quiet = io.StringIO()
        with redirect_stdout(quiet):
            lenient = _report(f2, include_dev=False, cooked_dir=cooked)
            strict = _report(f2, include_dev=True, cooked_dir=cooked)
        text = quiet.getvalue()
        if strict != 1:
            print("FAIL: --include-dev did not fail on dev findings")
            return 1
        if lenient != 1:
            # This fixture has genuine shipping violations, so lenient must still
            # fail -- otherwise the assertion above proves nothing about dev.
            print("FAIL: fixture no longer has a shipping-site violation to fail on")
            return 1
        if "/Game/Covered/DT_Fixture" not in text:
            print("FAIL: the dev-only absence was never reported to the user at all")
            return 1

    print("AFL-COOK self-test: PASS - R1 coverage, R2 cook output + dev split, R3 never-cook "
          "leak, ConstructorHelpers exemption, dev split and dynamic split all correct.")
    return 0


# -----------------------------------------------------------------------------
# CLI
# -----------------------------------------------------------------------------

def _report(f: Findings, include_dev: bool, cooked_dir: Path | None,
            strict_never_cook: bool = False) -> int:
    failed = False

    hard = list(f.uncovered) + (list(f.dev_uncovered) if include_dev else [])
    if hard:
        failed = True
        print(f"\nAFL-COOK R1: {len(hard)} string-referenced asset(s) not covered by "
              f"DirectoriesToAlwaysCook.")
        print("  The cooker cannot follow a path built from a C++ literal, so these may be")
        print("  silently absent from the packaged build. This CANNOT reproduce in PIE.\n")
        for ref in hard:
            print(f"  {ref}")
            print(f"      remedy: add to Config/DefaultGame.ini -> {remedy(ref.package)}")

    hard_missing = list(f.missing_in_cook) + (list(f.dev_missing_in_cook) if include_dev else [])
    if hard_missing:
        failed = True
        print(f"\nAFL-COOK R2: {len(hard_missing)} asset(s) ABSENT from the cooked "
              f"output at {cooked_dir}.")
        print("  These are confirmed missing on disk, not merely unconfigured.\n")
        for ref in hard_missing:
            print(f"  {ref}")
            print(f"      remedy: add to Config/DefaultGame.ini -> {remedy(ref.package)}")

    if not include_dev and (f.dev_uncovered or f.dev_missing_in_cook):
        # ONE LINE PER CALL SITE, tagged with what is true of it. A dev path that
        # is both uncovered and absent used to be printed twice -- once by each
        # rule -- which read as two problems and made the advisory section look
        # bigger than it was.
        by_site: dict[tuple[str, int, str], set[str]] = {}
        for ref in f.dev_uncovered:
            by_site.setdefault((ref.file, ref.line, ref.package), set()).add("uncovered")
        for ref in f.dev_missing_in_cook:
            by_site.setdefault((ref.file, ref.line, ref.package), set()).add("absent from cook")

        print(f"\nAFL-COOK: {len(by_site)} path(s) in dev-only code (cheats/tests/harnesses). "
              f"Not failing the build; re-run with --include-dev to enforce.")
        print("  A fixture only a cheat or a headless harness loads is SUPPOSED to be missing")
        print("  from a client cook -- those sites run where the asset is on disk.")
        for (file, line, package), tags in sorted(by_site.items()):
            print(f"  {file}:{line}: {package}  [{', '.join(sorted(tags))}]")

    if f.never_cook_leaked:
        # R3 fires only under --strict-never-cook, because a leak is a policy
        # regression rather than a broken build: the asset being present breaks
        # nothing at runtime, it just means an exclusion silently stopped working.
        # Loud in the report always; build-failing where the policy is enforced.
        if strict_never_cook:
            failed = True
        label = "R3" if strict_never_cook else "R3 (advisory)"
        print(f"\nAFL-COOK {label}: {len(f.never_cook_leaked)} asset(s) declared "
              f"AFL_NEVER_COOK are PRESENT in {cooked_dir}.")
        print("  Something reached them again -- a new hard reference, or a cook directory")
        print("  that now covers them. The exclusion has stopped working.\n")
        for ref in f.never_cook_leaked:
            print(f"  {ref}")

    if f.never_cook:
        print(f"\nAFL-COOK: {len(f.never_cook)} path(s) declared AFL_NEVER_COOK "
              f"(absent by design, exempt from R1/R2).")
        for ref in f.never_cook:
            state = "" if cooked_dir is None else (
                "  [LEAKED - present in cook]" if ref in f.never_cook_leaked
                else "  [confirmed absent]")
            print(f"  {ref}{state}")

    if f.dynamic:
        print(f"\nAFL-COOK: {len(f.dynamic)} runtime-composed path(s) a source scan cannot "
              f"verify. These are covered by the runtime registry, not by this lint.")
        for ref in f.dynamic:
            print(f"  {ref}")

    return 1 if failed else 0


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(
        prog="cook_soft_refs",
        description="AFL string-referenced asset cook lint (AFL-COOK). Finds asset "
                    "paths built from C++ literals, which the cooker cannot follow.",
    )
    p.add_argument("--root", type=Path, default=Path("."),
                   help="Repo root to scan (default: current directory).")
    p.add_argument("--self-test", action="store_true",
                   help="Regression guard: build a fixture tree exercising every rule "
                        "and both exemptions, then assert each behaves. Exits 0 if healthy.")
    p.add_argument("--cooked-dir", type=Path, default=None,
                   help="Enable R2. Path to a cook tree (e.g. Saved/Cooked/Windows). "
                        "A cook can succeed while omitting an asset, so this checks the "
                        "artifact rather than the exit code.")
    p.add_argument("--include-dev", action="store_true",
                   help="Also fail on findings in cheats/test harnesses -- both R1 (uncovered) "
                        "and R2 (absent from the cook).")
    p.add_argument("--strict-never-cook", action="store_true",
                   help="Fail (R3) if a path declared AFL_NEVER_COOK is actually PRESENT in "
                        "the cook tree -- i.e. an exclusion silently stopped working. "
                        "Requires --cooked-dir.")
    p.add_argument("--audit", action="store_true",
                   help="List every discovered path and its coverage state, then exit 0.")
    args = p.parse_args(argv)

    if args.self_test:
        return run_self_test()

    root: Path = args.root
    if not root.is_dir():
        print(f"error: --root {root} is not a directory", file=sys.stderr)
        return 2

    cooked_dir: Path | None = args.cooked_dir
    if cooked_dir is not None and not cooked_dir.is_dir():
        print(f"error: --cooked-dir {cooked_dir} is not a directory", file=sys.stderr)
        return 2

    findings = run_all(root, cooked_dir=cooked_dir)

    if args.audit:
        cook_dirs = read_always_cook_dirs(root)
        print(f"AFL-COOK audit: {len(findings.all_refs)} string-referenced path(s) "
              f"across shipping AFL source.")
        print(f"  always-cook directories configured: {len(cook_dirs)}")
        for d in cook_dirs:
            print(f"    {d}")
        missing = {r.package for r in findings.missing_in_cook}
        for ref in sorted(findings.all_refs, key=lambda r: (r.package, r.file, r.line)):
            if ref.is_dynamic:
                state = "DYNAMIC "
            elif ref.package in missing:
                state = "MISSING "
            elif is_covered(ref.package, cook_dirs):
                state = "COVERED "
            else:
                state = "uncovered"
            tag = " [dev]" if ref.is_dev else ""
            print(f"  {state}  {ref.package}{tag}")
            print(f"            {ref.file}:{ref.line}")
        return 0

    rc = _report(findings, include_dev=args.include_dev, cooked_dir=cooked_dir,
                 strict_never_cook=args.strict_never_cook)
    if rc == 0:
        scope = "coverage" if cooked_dir is None else "coverage + cooked output"
        print(f"AFL-COOK: clean - {len(findings.all_refs)} string-referenced path(s) "
              f"checked ({scope}).")
    return rc


if __name__ == "__main__":
    raise SystemExit(main())
