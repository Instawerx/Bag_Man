---
name: afl-laser-beam-system
description: >
  The single source of truth for building laser and beam weapon FX in the AFL /
  BAG MAN Lyra UE5 project. Covers adopting the marketplace "Laser VFX" Niagara +
  material library, wiring it to the authoritative GAS combat pipeline through
  GameplayCues (NOT through per-tick Blueprint traces), creating mobile material
  variants, and retiring the old hand-authored beam methodology (NS_AFL_PulseBeam,
  M_AFL_NeonMaster, and any client-tick-trace beam Blueprint).

  Use this skill whenever anyone on AFL asks about: making, adjusting, upgrading,
  or "doing properly" a laser, beam, pulse, prism, or orb weapon effect; importing
  or rebranding the Laser VFX pack; wiring a beam's visual endpoint to a hit/impact
  point; writing the GameplayCue for a beam; authoring AIK prompts for laser FX;
  replacing the previous laser/beam approach; or driving a Niagara beam's User.Beam
  End / User.Color parameters. Trigger it even when the request is vague ("make our
  laser better", "fix the beam", "the laser endpoint is wrong") — this is the
  canonical method and it overrides any earlier per-weapon Niagara authoring plan.
---

# AFL Laser & Beam Weapon System

This skill encodes the **one correct way** to build laser/beam weapon FX in BAG MAN.
It replaces every earlier approach (hand-authoring per-weapon Niagara from scratch,
and the marketplace demo's per-tick line-trace Blueprint).

## The one rule everything else follows

> **Gameplay owns the trace and the damage. Cosmetics own the beam. They meet at a
> GameplayCue, and the only thing that crosses the boundary is a world-space point
> and a color.**

The AFL GAS combat C++ that already exists (Pulse hitscan predict-and-send, the
lag-compensated server re-trace, `UAFLDamageExecCalc`, heat, telemetry) is the
**authoritative half** and stays exactly as written. The marketplace Laser VFX pack
is the **cosmetic half**. Wire them together with a GameplayCue and nothing else.

Why this split is non-negotiable: it is Lyra/GAS-canonical, it is the thing the
STEP-0 reset doctrine demands ("stay Lyra-canonical / one layer at a time"), and it
is the only design that survives multiplayer — the demo's tick-trace is client-only
and unreplicated and will fight your server trace if you ship it.

## What the marketplace pack actually gives you

A UE **5.2** VFX library (built on the First-Person template). Read
`references/pack-inventory.md` for the full extracted manifest. The parts that matter:

- **31 beam/orb Niagara systems** — each exposes exactly two integration parameters:
  - `User.Beam End` — `Position`/Vector, world-space. The beam stretches from the
    component origin to this point.
  - `User.Color` — `LinearColor`. Beam tint.
- **18 master materials + 11 instances** (emissive/translucent beam materials, panning
  noise via `Speed`/`U_Speed`/`V_Speed`, driven by fractal-noise textures).
- **A `BP_Laser` demo Blueprint that is REFERENCE-ONLY.** It does its own
  `LineTraceSingle` every tick and calls `SetVariableVec3("Beam End", ImpactPoint)`.
  **Never import or ship this.** The trace belongs in the ability; the BP just shows
  you which Niagara param to drive.

## Workflow — follow in order

This is feature work. Under STEP-0 doctrine it is gated behind the Control Gate
(BM-0013, a controllable pawn) and the Pulse reattachment (BM-0101). Do not mark any
step ✅ until it is **demonstrated in PIE** — a beam you watched stretch to a real hit
point on a moving target, not a thing that compiled.

1. **Import & rebrand the pack** → `references/import-and-rebrand.md`
   License check, 5.2→5.6 upgrade, exclude the Demo/, land it in a content-only
   `AFLVFXLibrary` plugin, apply the rename manifest, fix redirectors, confirm LFS.
   Generate the manifest with `scripts/audit_laser_pack.py` (read-only).

2. **Build the cosmetic layer** → `references/integration-architecture.md`
   Author the GameplayCue notifies (`GameplayCue.Weapon.Laser.Beam` looping,
   `GameplayCue.Weapon.Laser.Impact` burst), the muzzle-socket attach, and the
   `User.Beam End` / `User.Color` drive. Create the `_Mobile` material variants.

3. **Connect to the existing ability** → `references/integration-architecture.md`
   The ability triggers the cue with the **client-predicted** impact point for
   instant feel; damage still uses the server-validated trace. No Niagara code in
   the ability; no attribute code in the cue.

4. **Retire the old methodology** → `references/import-and-rebrand.md` (§Removal)
   Delete/cancel the hand-authored beam plan and any tick-trace beam BP. Update the
   AIK VFX profile so the agent never hand-rolls a beam Niagara again.

5. **Drive AIK to execute** → `references/aik-briefing.md`
   The updated AIK profile + the exact sequenced prompts to paste, in order.

6. **Verify in PIE** → `references/integration-architecture.md` (§PIE acceptance)
   The acceptance matrix that earns the ✅.

### Full weapon, not just the beam

A beam VFX is one slice. For the complete laser-rifle feature set — HUD slot, input,
weapon switching, the Lyra weapon definition, mesh/material, cooldown GE, camera shakes,
**automatic vs charge fire modes**, the charge timeline, and cancel/cleanup — every
feature is mapped to its Lyra-canonical home in `references/full-weapon-feature-map.md`,
with the additional AIK prompts (8–14) and PIE checks. Read it whenever the request is
about the weapon as a whole, fire modes, charging, weapon switching, or cooldown — not
just the beam visual.

### Making the agents system-aware

So Claude Code and AIK *know* these rules without being re-told each session:
- **Claude Code:** commit a project-root `CLAUDE.md` (auto-loaded every session,
  high-priority, survives `/compact`). A ready BAG MAN one ships with this skill
  (`references/Bag_Man_CLAUDE.md`) — it encodes the doctrine, the skill index, and the
  laser/weapon contract, and `@`-imports this `SKILL.md` from `Tools/skills/`. Place the
  skill folder at `C:\Dev\Bag_Man\Tools\skills\afl-laser-beam-system\` so the import
  resolves; copy `Bag_Man_CLAUDE.md` to the repo root as `C:\Dev\Bag_Man\CLAUDE.md`.
- **AIK:** run the §0 session-bootstrap prompt in `references/aik-briefing.md` first (it
  indexes `Tools\` + `Tools\skills` and makes the agent confirm awareness), then paste the
  §A profile block into the VFX profile so the in-editor agent carries the same rules.

## Hard constraints (carry these into every prompt and review)

- **Never** drive a beam from a Blueprint/actor that runs its own trace on Tick.
- **Never** spawn the Niagara directly inside the GameplayAbility. Cosmetics go
  through the cue so they are predicted + replicated + net-decoupled for free.
- **Always** make a PC master + a `_Mobile` instance for every beam material used in
  a shipped weapon (AFL platform rule: PC/Console/Mobile).
- **Always** attach the beam's start to the weapon **muzzle socket**, set the end to
  the **confirmed impact point**, never to a hardcoded length.
- The ✅ is a beam watched working in PIE on a controllable pawn. Compiling is not done.

## Install layout (on disk — assemble exactly like this)

This skill is a **folder**, not loose files. It must land as:

```
C:\Dev\Bag_Man\
  CLAUDE.md                              <- copy of references\Bag_Man_CLAUDE.md (auto-loads)
  Tools\skills\afl-laser-beam-system\
    SKILL.md
    references\  (integration-architecture.md, full-weapon-feature-map.md,
                  beam-replacement-plan.md, build-runbook.md, import-and-rebrand.md, aik-briefing.md,
                  pack-inventory.md, rename_manifest.csv, Bag_Man_CLAUDE.md)
    scripts\     (audit_laser_pack.py)
```

If you instead see a loose `Tools\skills\SKILL.md` with references scattered at `Tools\`,
the drop was flattened — reassemble it into the folder above before relying on it. The
`@import` in the root `CLAUDE.md` (`@Tools/skills/afl-laser-beam-system/SKILL.md`) only
resolves when the folder exists.

## Files in this skill

| File | Read it when |
|---|---|
| `references/integration-architecture.md` | Building the cue, the param drive, the ability hook, PIE acceptance |
| `references/full-weapon-feature-map.md` | The whole laser rifle: HUD, input, switching, fire modes, charge, shakes, cleanup |
| `references/beam-replacement-plan.md` | Removing/replacing existing beams: keep Pulse (AAA), fix the non-AAA second beam, standardize the rest |
| `references/build-runbook.md` | Driving it all by prompt in the Unreal terminal: place files → import → build AFLVFX module → wire cues → audit → fix the beam, with the human PIE/editor checkpoints marked |
| `references/import-and-rebrand.md` | Importing the pack, 5.2→5.6, naming, LFS, and removing the old method |
| `references/aik-briefing.md` | Driving AIK — profile update + paste-ready prompt sequence |
| `references/pack-inventory.md` | The real extracted asset list + the Niagara/material param surface |
| `references/Bag_Man_CLAUDE.md` | The project-root CLAUDE.md that makes Claude Code system-aware |
| `scripts/audit_laser_pack.py` | Regenerate the inventory + rename manifest from the unzipped pack |
