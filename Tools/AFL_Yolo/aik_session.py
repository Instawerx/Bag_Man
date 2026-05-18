#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
aik_session.py - Build a Claude Code Session Contract per task.

The Session Contract is the prompt format established in the AIK Session
Starter doc (1,029 lines, 7 stages). It tells Claude Code:

  1. Project identity and paths
  2. Which AFL skills to load
  3. The task and acceptance criteria
  4. Hard rails (no direct SetHealth, no GiveAbility outside AbilitySet, no
     server-side GetPlayerViewPoint, follow AFL-XXXX commit format)
  5. The verify mode and cheat matrix the orchestrator will run after
  6. What "done" looks like

This module is consumed by orchestrator.py and is also unit-testable on its own.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, TYPE_CHECKING

if TYPE_CHECKING:  # pragma: no cover
    from orchestrator import Config, Task


DISCIPLINE_DEFAULT_SKILLS: dict[str, list[str]] = {
    "eng":       ["afl-cpp-lyra-developer", "afl-build-operator", "unreal-engine-expert"],
    "devops":    ["afl-build-operator", "afl-qa-build-recovery"],
    "art":       ["afl-asset-pipeline", "afl-blender-bridge", "expert-game-designer"],
    "vfx":       ["afl-asset-pipeline", "expert-game-designer"],
    "ui":        ["afl-ui-hud-design", "expert-game-designer"],
    "audio":     ["afl-asset-pipeline"],
    "animation": ["afl-asset-pipeline", "lyra-skin-builder-marketplace"],
    "qa":        ["afl-qa-build-recovery"],
    "design":    ["afl-sprint-planner", "expert-game-designer"],
}


HARD_RAILS = """
HARD RAILS — non-negotiable, enforced by post-task lint (AFL-0215):

1. NEVER call AbilitySystemComponent->GiveAbility from BeginPlay or in code.
   All abilities are granted via ULyraPawnData ability sets (DA_AFL_AbilitySet_*).

2. NEVER call SetHealth (or any Health setter) directly from an ability or
   Blueprint. Damage MUST route through UAFLDamageExecCalc using the Damage
   meta attribute and a GameplayEffect.

3. NEVER use server-side GetPlayerViewPoint for hitscan. Hitscan traces are
   ALWAYS performed locally on the firing client, packed into an
   FGameplayAbilityTargetDataHandle, and shipped via ServerSetReplicatedTargetData
   (AFL-0106, master doc Sec. 7).

4. NEVER widen module exports beyond what the task requires. Keep AFL prefix
   on all classes, structs, and enums.

5. NEVER modify Lyra engine source. Extend via subclass or component.

6. ALL new GameplayTags must be added to AFLCoreTags.ini under the appropriate
   plugin's Config/Tags/ folder using the GameplayTagList= form (no + prefix at
   the plugin level — see master doc Sec. 14.5 for the gotcha that cost us a
   compile cycle).

7. Commit format: single-line summary `[AFL-XXXX] short title` followed by
   blank line and body. Do NOT amend already-pushed commits.
"""


WHAT_DONE_LOOKS_LIKE = """
DEFINITION OF DONE for this task:

* Code compiles cleanly via `Build.bat LyraEditor Win64 Development
  <uproject> -Plugin=<.uplugin>` for every plugin listed in plugins_to_build.

* No new compile warnings introduced (treat warnings as errors).

* If verify mode is `compile+cheat-matrix`, the cheats listed below must each
  produce a `AFLCombatCheats: OK <CheatName>` log line when executed by
  UnrealEditor-Cmd in `-game -nullrhi -unattended` mode.

* All AFL-0215 hard rails above are satisfied. The orchestrator will run lint
  after you finish; violations cause an automatic rollback.

* Files outside the task's `files_hint` scope SHOULD NOT be modified unless
  strictly necessary. Justify each out-of-scope edit in your final summary.

* When you are finished, print a final summary that:
    - Lists every file you touched (`Source/...`, `Content/...`, `Config/...`)
    - Names new public types you introduced
    - Notes any deferred work or follow-ups the orchestrator should file
"""


def build_session_contract(task: "Task", cfg: "Config") -> str:
    """Compose the full prompt body that ships to `claude -p ...`."""
    skills = task.skills or DISCIPLINE_DEFAULT_SKILLS.get(task.discipline, [])
    skills_lines = "\n".join(f"  * {s}" for s in skills) or "  (none specific)"

    files_lines = "\n".join(f"  * {p}" for p in task.files_hint) or "  (no hints — discover from context)"

    plugins_lines = "\n".join(f"  * {p}" for p in task.plugins_to_build) or "  (none)"

    cheats_block = ""
    if task.cheat_matrix:
        cheats_block = (
            "\nCHEAT MATRIX — the orchestrator will execute each of these after you "
            "finish, headless in `UnrealEditor-Cmd -game -nullrhi -unattended`. "
            "Each cheat MUST log `AFLCombatCheats: OK <CheatName>` on success:\n"
            + "\n".join(f"  * {c}" for c in task.cheat_matrix)
            + "\n"
        )

    return f"""You are operating inside the BAG MAN project (codename AFL), a Lyra Starter
Game-based UE5 project owned by C12 AI Gaming. The studio is using `afl_yolo`
to run continuous autonomous development. You are the executing agent for one
task. After you finish, an orchestrator will run AFL-0215 lint, Build.bat for
each plugin, and a cheat matrix; any failure rolls back your changes.

PROJECT IDENTITY
----------------
  uproject:       {cfg.uproject}
  project_root:   {cfg.project_root}
  engine_root:    {cfg.engine_root}
  main_branch:    {cfg.main_branch}
  repo:           {cfg.gh_repo}
  current_branch: {task.branch}

LOAD THESE SKILLS BEFORE STARTING (read each SKILL.md and follow its rules):
{skills_lines}

TASK
----
ID:           {task.id}
Title:        {task.title}
Discipline:   {task.discipline}
Estimate:     {task.estimate}
Verify mode:  {task.verify}

Brief (this is what the human wants):
{task.aik_brief or '(no brief — derive scope from the title and your loaded skills)'}

Files in scope (hints — not exhaustive, but stay close):
{files_lines}

Plugins to compile after you finish:
{plugins_lines}
{cheats_block}
{HARD_RAILS}
{WHAT_DONE_LOOKS_LIKE}

EXECUTION PROTOCOL
------------------
1. Read every SKILL.md listed above. They contain architectural constraints
   and naming conventions specific to AFL.
2. Inspect the relevant existing source under `Plugins/GameFeatures/` and
   `Source/` to understand the current state. Match the prevailing style.
3. Implement the task. Prefer the smallest correct diff. Add comments only
   where intent is non-obvious.
4. Run `Build.bat LyraEditor Win64 Development {cfg.uproject} -Plugin=<...>` for
   each plugin in plugins_to_build to confirm compile before you finish.
5. Do NOT commit. Do NOT push. The orchestrator handles git.
6. Do NOT open a PR. The orchestrator handles `gh pr create`.
7. End by printing the final summary specified under DEFINITION OF DONE.

Begin.
""".strip()


# ---------------------------------------------------------------------------
# Self-test helpers
# ---------------------------------------------------------------------------

def _self_test() -> int:
    """Minimal sanity check so this module can be invoked directly."""
    from types import SimpleNamespace
    fake_cfg = SimpleNamespace(
        uproject=Path("C:/Dev/Bag_Man/Bag_Man.uproject"),
        project_root=Path("C:/Dev/Bag_Man"),
        engine_root=Path("C:/UE_5.6"),
        main_branch="main",
        gh_repo="C12-Ai-Gaming/Bag_Man",
    )
    fake_task = SimpleNamespace(
        id="AFL-0104",
        title="Stand up UAFLAG_Laser_Pulse",
        discipline="eng",
        estimate="XL",
        autonomy="full",
        branch="yolo/afl-0104-laser-pulse",
        verify="compile+cheat-matrix",
        cheat_matrix=["AFL.Combat.Damage 50", "AFL.Combat.Overkill 250"],
        plugins_to_build=["AFLCombat"],
        skills=["afl-cpp-lyra-developer", "unreal-engine-expert"],
        files_hint=["Plugins/GameFeatures/AFLCombat/Source/AFLCombat/Public/Abilities/AFLAG_Laser_Pulse.h"],
        aik_brief="Implement client-predicted TargetData hitscan ability extending ULyraGameplayAbility.",
    )
    prompt = build_session_contract(fake_task, fake_cfg)
    print(prompt)
    return 0


if __name__ == "__main__":
    raise SystemExit(_self_test())
