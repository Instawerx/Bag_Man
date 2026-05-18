#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
notify.py - Discord webhook payload builder for afl_yolo.

Each event kind produces a Discord embed with a distinct color so a glance at
the phone tells you the status: teal=normal progress, green=merged, amber=needs
human, red=halt. The PR URL is always front-and-center when present so the
single tap from notification -> PR diff is preserved.

Use as a library: orchestrator imports build_discord_payload().
Use as a CLI: `python notify.py --webhook <url> --kind task_start ...`
"""

from __future__ import annotations

import argparse
import json
import sys
from typing import Any


COLORS = {
    "loop_start":    0x5865F2,   # blurple
    "loop_stop":     0x4F545C,   # gray
    "loop_halt":     0xED4245,   # red
    "task_start":    0x00B8D9,   # cyan/teal
    "pr_open":       0xF2C94C,   # gold (needs your eyes)
    "task_merged":   0x57F287,   # green
    "task_failed":   0xED4245,   # red
    "needs_human":   0xFAA61A,   # amber
    "queue_empty":   0x4F545C,   # gray
}


TITLES = {
    "loop_start":    "AFL YOLO loop started",
    "loop_stop":     "AFL YOLO loop stopped",
    "loop_halt":     "AFL YOLO HALTED",
    "task_start":    "Task started",
    "pr_open":       "PR open — review on phone",
    "task_merged":   "Task merged",
    "task_failed":   "Task failed",
    "needs_human":   "Needs human attention",
    "queue_empty":   "Queue empty",
}


def build_discord_payload(kind: str, payload: dict[str, Any], repo: str) -> dict[str, Any]:
    color = COLORS.get(kind, 0x99AAB5)
    title = TITLES.get(kind, kind)

    fields: list[dict[str, Any]] = []

    task_id = payload.get("task")
    task_title = payload.get("title")
    if task_id:
        line = f"**{task_id}**"
        if task_title:
            line += f"  ·  {_truncate(task_title, 80)}"
        fields.append({"name": "Task", "value": line, "inline": False})

    branch = payload.get("branch")
    if branch:
        fields.append({"name": "Branch", "value": f"`{branch}`", "inline": True})

    pr_url = payload.get("pr_url")
    pr_number = payload.get("pr_number")
    if pr_url:
        label = f"PR #{pr_number}" if pr_number else "PR"
        fields.append({"name": label, "value": pr_url, "inline": False})

    stage = payload.get("stage")
    if stage:
        fields.append({"name": "Stage", "value": stage, "inline": True})

    reason = payload.get("reason")
    if reason:
        fields.append({"name": "Reason", "value": _truncate(reason, 900), "inline": False})

    tail = payload.get("tail")
    if tail:
        fields.append({"name": "Log tail", "value": _fenced(_truncate(tail, 900)), "inline": False})

    if kind == "loop_start":
        fields.append({"name": "Repo", "value": repo, "inline": True})
        if payload.get("dry_run"):
            fields.append({"name": "Mode", "value": "dry-run", "inline": True})
        if payload.get("version"):
            fields.append({"name": "Version", "value": payload["version"], "inline": True})

    if kind == "loop_halt":
        count = payload.get("count")
        if count is not None:
            fields.append({"name": "Failure count", "value": str(count), "inline": True})

    if kind == "queue_empty":
        fields.append({
            "name": "Action",
            "value": "Add tasks to queue.yaml or mark errored tasks back to `pending`.",
            "inline": False,
        })

    if kind == "pr_open":
        fields.append({
            "name": "Phone workflow",
            "value": (
                "1. Open the PR link above in GitHub mobile\n"
                "2. Skim the diff and CI status\n"
                "3. (Optional) Drop the diff into Claude phone for a second pair of eyes\n"
                "4. Tap **Merge** when you're happy — the loop picks up the next task automatically"
            ),
            "inline": False,
        })

    embed = {
        "title": title,
        "color": color,
        "fields": fields,
        "footer": {"text": f"afl_yolo · {repo}"},
    }

    return {"embeds": [embed]}


def _truncate(value: str, n: int) -> str:
    value = str(value)
    if len(value) <= n:
        return value
    return value[: n - 1] + "\u2026"


def _fenced(value: str) -> str:
    if "```" in value:
        value = value.replace("```", "ʼʼʼ")
    return "```\n" + value + "\n```"


def main(argv: list[str] | None = None) -> int:
    import urllib.request

    p = argparse.ArgumentParser(description="Send a Discord notification for afl_yolo")
    p.add_argument("--webhook", required=True)
    p.add_argument("--kind", required=True, choices=sorted(TITLES.keys()))
    p.add_argument("--repo", default="C12-Ai-Gaming/Bag_Man")
    p.add_argument("--task", default=None)
    p.add_argument("--title", default=None)
    p.add_argument("--branch", default=None)
    p.add_argument("--pr-url", default=None)
    p.add_argument("--pr-number", type=int, default=None)
    p.add_argument("--stage", default=None)
    p.add_argument("--reason", default=None)
    p.add_argument("--tail", default=None)
    args = p.parse_args(argv)

    payload: dict[str, Any] = {}
    for k in ("task", "title", "branch", "stage", "reason", "tail"):
        v = getattr(args, k, None)
        if v:
            payload[k] = v
    if args.pr_url:
        payload["pr_url"] = args.pr_url
    if args.pr_number:
        payload["pr_number"] = args.pr_number

    body = build_discord_payload(args.kind, payload, args.repo)
    data = json.dumps(body).encode("utf-8")
    req = urllib.request.Request(args.webhook, data=data, headers={"Content-Type": "application/json"})
    try:
        urllib.request.urlopen(req, timeout=15)
    except Exception as exc:  # noqa: BLE001
        print(f"[notify] failed: {exc}", file=sys.stderr)
        return 1
    print("[notify] sent")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
