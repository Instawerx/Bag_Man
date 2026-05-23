#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Reconcile BAG_MAN_LIVE_TRACKER (internal v2.6 -> v2.7) in place.

WHY THIS EXISTS:
  Windows PowerShell 5.1 `Get-Content -Raw` decodes UTF-8 files with the ANSI
  codepage, so a `-replace` against a clean U+00B7 separator silently no-ops.
  Python reads/writes explicit UTF-8, so the real characters match.

SAFETY:
  Each edit asserts its OLD string is present EXACTLY the expected count BEFORE
  any replacement happens. If any anchor is missing or ambiguous, the script
  aborts and changes NOTHING. A silent no-op is impossible.

SCOPE OF v2.7 DELTA (vs v2.6 @ 68db4743):
  - AFL-0217 GenAI mesh validator landed (work dddf43ae, PR #18 = 1f7d692a)
  - queue status reconcile commit b5fb9824 on top
  Everything else in v2.6 was already correct.

DOES NOT TOUCH: STORAGE_KEY (bumping it would wipe everyone's saved checkboxes).
"""

import sys
import io

FILE = "BAG_MAN_LIVE_TRACKER_v2_2.html"  # filename still v2_2 on disk; internal version is what we bump

# Read as explicit UTF-8, preserve existing line endings (no translation).
try:
    with io.open(FILE, "r", encoding="utf-8", newline="") as fh:
        text = fh.read()
except FileNotFoundError:
    print(f"ABORT: {FILE} not found. Run from C:\\Dev\\Bag_Man.")
    sys.exit(2)

nl = "\r\n" if "\r\n" in text else "\n"
indent = "      "  # 6 spaces, matches the banner body's existing indentation

# ---- AFL-0217 banner paragraph (prepended before the AFL-0206 block) ----
afl217_para = (
    "<strong>\U0001f9ec AFL-0217 \u2705 GenAI mesh validator shipped</strong> via PR #18 "
    "(work commit dddf43ae, merge 1f7d692a). Tools/AFL_MeshValidator/afl_mesh_validator.py "
    "\u2014 pure-Python conformance gate between genAI mesh outputs (Tripo/Meshy) and the "
    "afl-blender-bridge AAA-clean pipeline. 5 checks: POLY_BUDGET, NAMING, UV_COVERAGE, "
    "MAT_SLOTS, SOCKET_SNIFF (hero warn-only). FBX\u2192glTF via Blender headless shell-out, "
    "glTF-direct fallback when blender is off PATH. budgets.json mirrors master \u00a716.2. "
    "Notable recovery arc: the orchestrator authored + committed the work, but its git push "
    "timed out on a network stall and the queue row stuck at status:running. The work was "
    "never lost \u2014 it was safe at dddf43ae on the branch; once the network returned it was "
    "manually pushed, PR #18 merged, and the stale queue status reconciled to merged "
    "(b5fb9824). No re-run, no burned cost. First task to validate the new TimeoutExpired "
    "catch in the orchestrator's git() path (70880797)."
    f"{nl}{indent}<br><br>{nl}{indent}"
    "<strong>\U0001f3af AFL-0206 \u2705 Beam ability shipped</strong>"
)

# Each entry: (label, old, new, expected_count)
EDITS = [
    (
        "R1 title",
        "<title>BAG MAN :: LIVE BUILD TRACKER v2.6</title>",
        "<title>BAG MAN :: LIVE BUILD TRACKER v2.7</title>",
        1,
    ),
    (
        "R2 brand-sub",
        "// Live Build Tracker \u00b7 v2.6 \u00b7 SSOT \u00b7 Sprint 1+2 ability/infra done \u00b7 18/18 yolo merged \u00b7 queue exhausted",
        "// Live Build Tracker \u00b7 v2.7 \u00b7 SSOT \u00b7 Sprint 1+2 ability/infra done \u00b7 19/19 yolo merged \u00b7 queue exhausted",
        1,
    ),
    (
        "R3 banner comment",
        "<!-- v2.6 PROGRESS BANNER (reconciled against origin/main @ 68db4743 on 2026-05-22 late) -->",
        "<!-- v2.7 PROGRESS BANNER (reconciled against origin/main @ 1f7d692a \u2014 PR #18 AFL-0217 merge; queue-status fix b5fb9824 on top) -->",
        1,
    ),
    (
        "R4 banner-tag",
        "v2.6 \u00b7 18/18 yolo merged \u00b7 Sprint 2 ability layer + Heat economy + QA matrix + AFL-0110 closure all landed \u00b7 active queue exhausted",
        "v2.7 \u00b7 19/19 yolo merged \u00b7 AFL-0217 GenAI mesh validator landed (PR #18) \u00b7 active queue exhausted",
        1,
    ),
    (
        "R5 AFL-0217 task row",
        "{id:'AFL-0217', title:'\U0001f3a8 GenAI mesh validation pipeline \u2014 afl_mesh_validator.py for Tripo/Meshy outputs, conformance budgets per asset class, pass/fail manifest (\u00a716.2)', d:'art', e:'L'}",
        "{id:'AFL-0217', title:'\u2705 GenAI mesh validator shipped \u2014 Tools/AFL_MeshValidator/afl_mesh_validator.py (475 lines, pure Python, zero pip deps beyond pytest). 5 checks: POLY_BUDGET / NAMING / UV_COVERAGE / MAT_SLOTS / SOCKET_SNIFF (hero warn-only). FBX\u2192glTF via Blender headless shell-out; glTF-direct fallback if blender not on PATH. budgets.json mirrors master \u00a716.2. Work dddf43ae \u2192 merged PR #18 (1f7d692a). Recovery arc: orchestrator push timed out, work stayed safe on branch, manually pushed + merged, queue status reconciled (b5fb9824) \u2014 no re-run. (\u00a716.2)', d:'devops', e:'L'}",
        1,
    ),
    (
        "R6 banner body lead",
        "<strong>Origin/main HEAD <code>68db4743</code>. Sprint 2 ability + infrastructure layer is now COMPLETE. The active 18-task yolo queue is fully closed. Since v2.5 reconcile (4 new PR merges + AFL-0110 closure):</strong>",
        "<strong>v2.7 reconcile \u2014 origin/main HEAD <code>1f7d692a</code> (PR #18 merge; queue-status fix b5fb9824 on top). Sprint 2 ability + infrastructure layer COMPLETE. The 19-task yolo queue is now fully closed \u2014 AFL-0217 GenAI mesh validator landed since v2.6:</strong>",
        1,
    ),
    (
        "R7 prepend AFL-0217 paragraph",
        "<strong>\U0001f3af AFL-0206 \u2705 Beam ability shipped</strong>",
        afl217_para,
        1,
    ),
    (
        "R8 queue-state line",
        "18 of 18 yolo-queue tasks merged. 0 errors. 0 pending. <code>Next: (none eligible)</code>. Last PR: #17.",
        "19 of 19 yolo-queue tasks merged. 0 errors. 0 pending. <code>Next: (none eligible)</code>. Last PR: #18.",
        1,
    ),
]

# ---- VERIFY ALL ANCHORS FIRST (no mutation yet) ----
problems = []
for label, old, _new, expected in EDITS:
    found = text.count(old)
    if found != expected:
        problems.append(f"  [{label}] expected {expected} match(es), found {found}")

if problems:
    print("ABORT: anchor verification failed. NOTHING was changed.")
    print(nl.join(problems))
    print()
    print("Likely cause: the canonical file differs from what this script expects.")
    print("Do NOT force it. Re-read the file and adjust the script.")
    sys.exit(1)

# ---- APPLY (all anchors confirmed unique) ----
for label, old, new, _expected in EDITS:
    text = text.replace(old, new, 1)

with io.open(FILE, "w", encoding="utf-8", newline="") as fh:
    fh.write(text)

print("OK: v2.7 reconcile applied. 8 edits, all anchors matched exactly once.")
print("  - internal version 2.6 -> 2.7 (title, brand-sub, banner-tag, comment)")
print("  - queue count 18/18 -> 19/19; Last PR #17 -> #18")
print("  - AFL-0217 task row: planned art -> shipped devops")
print("  - AFL-0217 banner paragraph added (recovery arc documented)")
print("  - STORAGE_KEY untouched (bag_man_tracker_v4)")
print()
print("Next: git diff --stat ; review ; git mv to drop the v2_2 filename ; commit.")
