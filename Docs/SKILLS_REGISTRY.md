# AFL Skill Registry

**Generated** by `Tools/AFL_Lint/skill_registry.py --generate`. Do not edit
by hand - edit the `SKILLS` tuple in that script and re-run the generator.

This file is the authoritative list of skill slugs that may be referenced
in AFL source, configs, and docs. The CI job `afl-skill-registry` (AFL-0218)
rejects PRs that introduce a reference to a slug starting with `afl-` that
is not in this list.

Source of truth: master doc Sec. 15.1.

## Skills

| # | Slug | Discipline | Summary |
|---|---|---|---|
| 1 | `afl-cpp-lyra-developer` | C++ Engineering | Lyra-correct C++ extension: GAS abilities, GameFeature plugins, AttributeSets, ExecCalcs. Enforces master doc Sec. 7/8/9. |
| 2 | `afl-neostack-task-writer` | Engineering / AI Workflow | Writes high-quality AIK prompts that produce AFL-conformant Blueprints, Materials, Behavior Trees. |
| 3 | `afl-sprint-planner` | Production / Project Mgmt | Decomposes features into AFL-XXXX tasks, writes sprint briefs, estimates effort, tracks blockers. |
| 4 | `afl-build-operator` | DevOps / Build | UAT BuildCookRun pipelines, GitHub Actions, multi-platform packaging, dedicated server builds. |
| 5 | `afl-asset-pipeline` | Tech Art / Pipeline | DCC tool export settings, FBX/USD import, LOD generation, texture compression per platform. |
| 6 | `afl-qa-build-recovery` | QA / DevOps | Crash triage, broken-build recovery, regression matrices, platform cert (TRC/XR). |
| 7 | `afl-ui-hud-design` | UI Engineering | Lyra CommonUI stack, UMG widgets, activatable widgets, multi-platform input routing. |
| 8 | `afl-blender-bridge` | Tech Art / 3D | Blender<->UE5 round-trip via blender_mcp. Kitbash, retexture, dress, audit, AAA-clean modular assets. Composes with NeoStack genAI (Tripo, Meshy). Image-to-level blockouts. Heightmap landscapes. |
| 9 | `lyra-skin-builder-marketplace` | Character / Cosmetics | Lyra-foundation reskinning pipeline. Mesh swap, IK retargeting to SK_Mannequin, modular character parts, in-game cosmetic marketplace, GameFeature plugins for live-ops skin drops, server-authoritative entitlement. |
| 10 | `expert-game-designer` | Game Design / Visual Direction | Apple-Glass-inspired UI aesthetic, level/environment design direction, character/creature concept, Midjourney + NeoStack AIK prompt pipelines, design systems, palettes, typography. |
| 11 | `unreal-engine-expert` | C++ Engineering (general UE5) | AAA-level general UE5 expertise: rendering (Lumen, Nanite, Niagara), gameplay systems, AI, networking, optimization, animation. Pairs with afl-cpp-lyra-developer for AFL-specific work. |

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
