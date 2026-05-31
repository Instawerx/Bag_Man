# AIK Briefing — Laser/Beam, the proper way

Paste-ready material for NeoStack AIK (Claude Code agent, AFL profiles). This both
**informs AIK of the correct method** and **removes the old one**. Use the AFL prompt
formula: *[what] + [Lyra base class] + [behavior] + [AFL conventions] + [platform]*.

## 0. Session bootstrap — make the agent aware of Tools/ and Tools/skills (run FIRST)

Paste this as the first message of an AIK session (the agent runs Claude Code with the
project at `C:\Dev\Bag_Man`). It indexes the dropped tools/skills and proves awareness
before any work:

```
Before doing anything else, become system-aware of this project's tooling.

1. Read C:\Dev\Bag_Man\CLAUDE.md (project memory / doctrine).
2. List C:\Dev\Bag_Man\Tools and C:\Dev\Bag_Man\Tools\skills.
3. For every <skill>\SKILL.md under Tools\skills, read its YAML frontmatter (name +
   description) so you know when each triggers.
4. Read Tools\skills\afl-laser-beam-system\SKILL.md IN FULL, plus its references\
   integration-architecture.md, full-weapon-feature-map.md, and beam-replacement-plan.md.

Then confirm awareness by replying with, and nothing else yet:
  (a) the list of skills you found under Tools\skills (name — one-line purpose),
  (b) the laser/weapon doctrine in your own words (the cosmetic↔authoritative boundary,
      GameplayCue rule, cooldown-as-GE, ✅=PIE), and
  (c) the keep/replace/retire stance: Pulse Carbine stays (AAA), the second beam gets
      re-skinned via the imported library, all beam Niagara is hand-authored = retired.

From now on, for ANY laser / beam / weapon / VFX task, read afl-laser-beam-system first
and follow it. Treat C:\Dev\Bag_Man\Tools\skills as the source of truth for AFL workflows.
Do not start implementation in this message — only confirm awareness.
```

If AIK's agent can't see `Tools\` (working dir scoped elsewhere), add the folder to its
allowed dirs, or set the Claude Code env so it loads CLAUDE.md from there — but the simplest
fix is to keep `CLAUDE.md` at the repo root (it auto-loads) and let its `@Tools/skills/...`
imports pull the rest.

## 0b. Authorization to reorganize ONLY the laser-skill files (run after §0 confirms awareness)

Use this when the laser-skill files were dropped flattened/scattered and need assembling.
It is an **addition and enhancement, not a takeaway** — no other skill is touched.

```
You are authorized to MOVE and RENAME files to assemble the afl-laser-beam-system skill
into its correct layout, and ONLY these files. Goal layout:

  C:\Dev\Bag_Man\CLAUDE.md                                  (the project memory; auto-loads)
  C:\Dev\Bag_Man\Tools\skills\afl-laser-beam-system\SKILL.md
  C:\Dev\Bag_Man\Tools\skills\afl-laser-beam-system\references\*.md (+ rename_manifest.csv)
  C:\Dev\Bag_Man\Tools\skills\afl-laser-beam-system\scripts\audit_laser_pack.py

ALLOW-LIST (you may move/rename/consolidate these):
  - Tools\skills\SKILL.md   (the afl-laser-beam-system SKILL.md sitting loose)
  - Tools\Bag_Man_CLAUDE.md (-> copy to C:\Dev\Bag_Man\CLAUDE.md)
  - Tools\integration-architecture.md, Tools\full-weapon-feature-map.md,
    Tools\import-and-rebrand.md, Tools\pack-inventory.md, Tools\aik-briefing.md,
    Tools\rename_manifest.csv, Tools\beam-replacement-plan.md   (the references)
  - Tools\skills\Laser VFX.md, Tools\skills\VFX_Laser_Skill.md  (loose laser drafts)

DENY-LIST (never move, rename, delete, or edit):
  - Any other skill folder: afl-cpp-lyra-developer, lyra-ue5-build-discipline,
    unreal-engine-expert, afl-sprint-planner, afl-asset-pipeline, and any other <name>\SKILL.md
  - The raw bundles expert-game-designer.skill, lyra-skin-builder-marketplace.skill
  - Anything outside Tools\ (no Content\, Source\, Plugins\, Config\ changes here)

RULES — lose no ground:
  1. Before deleting/superseding any loose laser draft (Laser VFX.md, VFX_Laser_Skill.md),
     read it and fold anything it contains that ISN'T already in the assembled skill into the
     right reference file. Only then remove the redundant loose copy.
  2. If a target file already exists with NEWER/different content than the loose copy, keep
     the better content — diff and merge, don't blindly overwrite. The canonical set has 8
     references incl. beam-replacement-plan.md; if a loose SKILL.md's file table is older
     (lists import-and-rebrand.md in the replacement-plan slot), prefer the newer SKILL.md.
  3. Make NO engine/asset/code changes in this step. Filesystem assembly only.
  4. When done, re-run the §0 enumeration and report the final tree + anything you merged or
     couldn't reconcile. Do not start implementation.
```



## A. Replace the "AFL VFX & Materials" profile Custom Instructions

Open AIK Settings → Profiles → **AFL VFX & Materials** → Custom Instructions, and
replace with:

```
This is the AFL / BAG MAN project on Lyra Starter Game, UE 5.6, targeting PC + Console + Mobile.

LASER/BEAM WORK — THE ONLY APPROVED METHOD:
- Beam VISUALS come from the imported library under /AFLVFXLibrary/Laser/ (Niagara NS_AFL_Laser_* / NS_AFL_OrbLaser_*). NEVER hand-author a beam Niagara system from scratch. If a new look is needed, instance an existing system or its material, do not rebuild.
- A beam's only inputs are two Niagara user params: "Beam End" (Vector, world-space impact point) and "Color" (LinearColor). Drive those; nothing else.
- Beams are played through GameplayCues in the AFLVFX module: AAFLCueNotify_LaserBeam (looping, channeled), plus burst cues for BeamFlash/Impact/Muzzle. NEVER spawn beam Niagara directly inside a GameplayAbility. NEVER drive a beam endpoint from a per-tick LineTrace in an actor/BP — the authoritative trace already lives in the AFL ability + lag-comp subsystem; the cue does a cosmetic-only endpoint trace if needed.
- The weapon supplies its own look via IAFLLaserVisualProvider (GetBeamSystem/GetMuzzleSystem/GetBeamColor/GetMuzzleSocketName/GetCosmeticRange). Cues read it from cue SourceObject. Look is data, not code.
- Every shipped beam material needs a PC master + a _Mobile instance (drop scene-color/distortion, cap samples). One Niagara system, quality-switched — never fork per platform.

GENERAL:
- Subclass Lyra classes; never edit Lyra base. New code in AFLVFX module or a GameFeature plugin.
- Niagara naming NS_AFL_, materials M_AFL_*_Master, instances MI_AFL_. GameplayCue tags GameplayCue.Weapon.Laser.*.
- ✅ means demonstrated in PIE on a controllable pawn, never "compiles". Re-verify base movement after any change.

DO NOT create or reference: NS_AFL_PulseBeam, NS_AFL_PrismBeam, NS_AFL_HitSpark, or M_AFL_NeonMaster as a beam material — these are RETIRED. Do not import or use BP_Laser / BP_Laser_Orb from the marketplace pack; they are reference-only tick-trace demos.
```

## B. Sequenced prompts (run in order, attach context where noted)

### Prompt 1 — tags
```
Add these GameplayCue tags to the AFL plugin-local tags table in
Config/Tags/ (same file family as the existing 51-tag bundle, GameplayTagList=
no '+' prefix at plugin level):
  GameplayCue.Weapon.Laser.Beam
  GameplayCue.Weapon.Laser.BeamFlash
  GameplayCue.Weapon.Laser.Impact
  GameplayCue.Weapon.Laser.Muzzle
Then add matching native tag declarations to AFLGameplayTags (NativeGameplayTags
pattern). Show the diff and the PrintReplicationIndices count after.
```

### Prompt 2 — the visual-provider interface
```
Create a UInterface IAFLLaserVisualProvider in a new module AFLVFX
(Plugins/AFLVFX/, runtime module, deps: Niagara, GameplayAbilities, GameplayTags,
the AFLCombat module). Methods exactly:
  UNiagaraSystem* GetBeamSystem() const
  UNiagaraSystem* GetMuzzleSystem() const
  FLinearColor    GetBeamColor() const
  FName           GetMuzzleSocketName() const
  float           GetCosmeticRange() const
Use UPROPERTY/UFUNCTION as appropriate for BlueprintNativeEvent so BP weapons can
implement it. Follow AFL conventions.
```

### Prompt 3 — the looping beam cue  (attach: nothing; this is new C++)
```
In AFLVFX, create AAFLCueNotify_LaserBeam extending AGameplayCueNotify_Actor.
On OnActive: read IAFLLaserVisualProvider from Params.SourceObject; spawn the beam
NiagaraSystem attached to the weapon mesh at GetMuzzleSocketName via
UNiagaraFunctionLibrary::SpawnSystemAttached (bAutoDestroy false); set the "Color"
Niagara user param from GetBeamColor.
On Tick: trace from the muzzle socket forward by GetCosmeticRange on ECC_Visibility,
ignoring the instigator; set the "Beam End" Niagara Vec3 user param to the hit
ImpactPoint (or trace end if no hit). This trace is COSMETIC ONLY and must never apply
damage or a GameplayEffect.
On OnRemove: Deactivate the component, set AutoDestroy true, disable tick.
Expose BeamEndParam ("Beam End"), ColorParam ("Color"), and the cosmetic trace channel
as EditDefaultsOnly UPROPERTYs. Production-grade, fully commented.
```

### Prompt 4 — burst cues
```
In AFLVFX create AAFLCueNotify_LaserImpact (burst): on execute, spawn the impact spark
Niagara at Params.Location oriented to Params.Normal; optional decal.
And AAFLCueNotify_LaserBeamFlash (burst): spawn the beam system, set "Beam End" =
Params.Location and "Color" from the SourceObject's IAFLLaserVisualProvider, auto-
destroy after 0.08s. Both read everything from cue params / SourceObject — no tick.
```

### Prompt 5 — implement the provider on the weapon  (attach: the AFL laser weapon EquipmentInstance / its cosmetic data asset)
```
Make the attached AFL laser weapon implement IAFLLaserVisualProvider. Back the values
with a data asset DA_AFL_LaserVisual_<Weapon> (EditAnywhere): BeamSystem
(NS_AFL_Laser_*), MuzzleSystem (optional NS_AFL_OrbLaser_Center_*), BeamColor,
MuzzleSocketName (default "Muzzle"), CosmeticRange (default = the weapon's max range).
For the Pulse Carbine point BeamSystem at NS_AFL_Laser_Basic; for the Prism Beam point
it at NS_AFL_Laser_Twist. Do not hardcode systems in C++.
```

### Prompt 6 — trigger from the abilities  (attach: UAFLAG_Laser_Pulse and UAFLAG_Laser_Beam)
```
In the attached abilities, add ONLY GameplayCue triggers — do not move any trace or
damage logic:
- _Beam (channeled): on activation after CommitAbility, ASC->AddGameplayCue(
  GameplayCue.Weapon.Laser.Beam) with FGameplayCueParameters.SourceObject = the laser
  weapon instance. On ability end, RemoveGameplayCue the same tag.
- _Pulse (hitscan): per confirmed shot, build FGameplayCueParameters with SourceObject =
  weapon, Location = the CLIENT-PREDICTED impact point, Normal = predicted normal, then
  ExecuteGameplayCue BeamFlash and ExecuteGameplayCue Impact.
Damage continues to use the server-validated trace result unchanged. Show the diff and
confirm no Niagara include was added to the ability.
```

### Prompt 7 — mobile variants
```
For each beam master material a shipped weapon uses (start with M_AFL_Beam_Master and
M_AFL_Laser_Twist_Master), create a _Mobile material instance: disable scene-color /
refraction / distortion, cap texture samples, prefer additive blend where the look
survives. Keep one Niagara system per beam, quality-switched — do not fork per platform.
```

## C. AIK troubleshooting specific to this work

| Symptom | Fix |
|---|---|
| Beam endpoint stuck at origin | The Niagara user param name isn't "Beam End" — verify exact name in the system; UE shows it as `User.Beam End` |
| Beam not visible on other clients | Triggered with `Execute`/`Add` on a non-replicating path — ensure it's the ASC cue API, not a raw spawn |
| Color ignored | Param name mismatch ("Color" vs "User.Color") or driving the component before it spawned |
| Agent regenerates a beam Niagara | Profile Custom Instructions not saved/active — re-paste section A |
| Beam fights the damage hit | Someone fed the cosmetic trace into damage — they must be separate; only the ability's server trace damages |


## D. Full-weapon prompts (charge, cooldown, shake, HUD/input, switching)

The full laser-rifle prompt sequence (Prompts 8–14: charge + fire-mode tags, the Lyra
weapon definition, cooldown GE, auto-fire ability, charge ability, charge cue + camera
shakes, fire-mode toggle + HUD/input) lives with its design in
`full-weapon-feature-map.md` so the prompts sit next to the architecture they implement.
Run them after Prompts 1–7 here. Keep one source — do not duplicate them into this file.
