# CLAUDE.md — BAG MAN (AFL) project memory

Auto-loaded by Claude Code every session. Keep it lean and authoritative. Heavier
detail lives in the skills referenced below — open those on demand, don't inline them.

## Project

- **Game:** BAG MAN. Studio: C12 AI Gaming. Internal prefix: **AFL**.
- **Engine:** UE **5.6**, built on **Lyra Starter Game**. `.uproject`: `Bag_Man.uproject`.
- **Path:** `C:\Dev\Bag_Man`. Repo: `C12-Ai-Gaming/Bag_Man` (Git LFS, 250 GB).
- **C++ prefix:** `AFL` (modules `AFLCore`, `AFLCombat`, `AFLMovement`, `AFLVFX`; content plugin `AFLVFXLibrary`).
- **AI in editor:** NeoStack AIK v1.0.74 (Claude Code agent). Bridge: `Content/AFL/_Bridge/Blender/`.
- **Agent tooling & skills:** `C:\Dev\Bag_Man\Tools` (tools) and `C:\Dev\Bag_Man\Tools\skills` (AFL skill suite, auto-imported below).

## Doctrine — non-negotiable (this is why the project was reset once already)

1. **✅ means demonstrated in PIE** on a controllable pawn — watched moving/firing on
   screen. Never "compiles," never "committed." A green checkbox is a thing you saw work.
2. **Stay Lyra-canonical.** Characters and weapons are **Blueprint child + data assets**
   (PawnData, EquipmentDefinition, AbilitySet, InputConfig). **Never** assemble a hero or
   weapon from raw C++. The original failure was an empty `DefaultInputMappings` from a
   raw-C++ hero component — do not repeat it.
3. **Abilities are granted via AbilitySets** on PawnData/EquipmentDefinition — never
   `GiveAbility` in `BeginPlay`/code.
4. **Cosmetics go through GameplayCues; gameplay never touches Niagara; cosmetics never
   touch attributes.** Cooldowns are Cooldown **GameplayEffects**, not float variables.
5. **One layer at a time. Re-verify base control after each change.** The live PIE / live
   terminal is the only source of truth — status marks and summaries are reports, not proof.
6. **Multi-platform:** PC + PS5/XSX + iOS/Android. Every shipped material needs a PC master
   and a `_Mobile` instance. Guard platform code with `#if PLATFORM_*`.

## Build / verify

```bat
:: Generate project files / build a plugin (pattern used across AFL modules)
Build.bat LyraEditor Win64 Development -Project="C:\Dev\Bag_Man\Bag_Man.uproject" -Plugin=AFLCombat.uplugin
```
Never mark work done off a successful build alone — open PIE (listen server + 2 clients for
anything networked) and watch it.

## Skills — consult before acting in these domains

The skills live under `C:\Dev\Bag_Man\Tools\skills`. **Disk is truth** (Pillar 5): at
session start, enumerate `Tools\skills\*\SKILL.md` and read the frontmatter of each that's
actually present — do not assume a skill exists because it's named here. Read the relevant
`SKILL.md` IN FULL before writing code or creating assets in its domain.

- **Laser / beam / any weapon FX work → `afl-laser-beam-system`. Mandatory.**
- Methodology / trap catalog (PIE-✅, foundation-first, read-before-author) → `lyra-ue5-build-discipline`.
- C++ on Lyra (GAS, GameFeatures, equipment) → `afl-cpp-lyra-developer`.
- General AAA UE5 → `unreal-engine-expert`. Asset import / LFS / cook → `afl-asset-pipeline`.
- Sprint / task planning → `afl-sprint-planner`.

Studio suite that MAY also be dropped here (flag as missing if a task needs one and it
isn't on disk — do not invent it): `afl-neostack-task-writer`, `afl-ui-hud-design`,
`afl-build-operator`, `afl-qa-build-recovery`, `afl-blender-bridge`,
`lyra-skin-builder-marketplace`, `expert-game-designer`. Raw `.skill` bundles or loose
`*.md` in `Tools\skills` are NOT installed skills until assembled into a `<name>\SKILL.md`
folder.

@Tools/skills/afl-laser-beam-system/SKILL.md

## Laser & weapon system — the one approved method

Full detail in the skill above; the rules Claude Code must always honor:

- **Beam visuals** come from the imported `/AFLVFXLibrary/Laser/` library
  (`NS_AFL_Laser_*` / `NS_AFL_OrbLaser_*`). **Never hand-author a beam Niagara from
  scratch.** A beam's only inputs are `User.Beam End` (Vector) and `User.Color`.
- **Beams/charge/impact/shake play through GameplayCues** (`AFLVFX` module:
  `AAFLCueNotify_LaserBeam` looping, plus burst cues `BeamFlash`/`Impact`/`Muzzle`/`Charge`).
  Never spawn beam Niagara inside an ability. Never drive a beam endpoint from a per-tick
  `LineTrace` in an actor — the authoritative trace lives in the AFL ability + lag-comp
  subsystem; the cue does a cosmetic-only endpoint trace.
- **Weapon = Lyra data**, not a hand-built actor: `ID_AFL_*` (ItemDefinition) →
  `InventoryFragment_EquippableItem` → `ED_AFL_*` (EquipmentDefinition) →
  `B_WeaponInstance_AFL_*` (child of `ULyraRangedWeaponInstance`) + an AbilitySet.
- **Fire modes** = GameplayAbilities: `UAFLAG_Laser_AutoFire` (hitscan, LocalPredicted,
  ≈ existing Pulse) and `UAFLAG_Laser_Charge` (charge curve, SetByCaller-scaled damage,
  looping charge cue). Toggle via `InputTag.Weapon.FireMode` + `State.Weapon.Mode.*`.
- **Cooldown** = `GE_AFL_*_Cooldown` granting `Cooldown.Weapon.*` (same shape as
  `GE_AFL_DashCooldown`), set as the ability's `CooldownGameplayEffectClass`.
- **Cleanup** lives entirely in `EndAbility` (end + cancel): `RemoveGameplayCue` for every
  looping cue + cancel timers/tasks. The looping cue `OnRemove` is the one place Niagara/
  audio/shake actually stop.
- **RETIRED — never create or reference:** `NS_AFL_PulseBeam`, `NS_AFL_PrismBeam`,
  `NS_AFL_HitSpark`, `M_AFL_NeonMaster` (as a beam material), and the marketplace
  `BP_Laser`/`BP_Laser_Orb` (reference-only tick-trace demos).

## Naming

`AFLAG_`/`UAFLAG_` abilities · `GE_AFL_*` effects · `NS_AFL_*` Niagara · `M_AFL_*_Master`
materials · `MI_AFL_*` instances · `DA_AFL_*` data assets · `ED_AFL_*` equipment defs ·
`ID_AFL_*` item defs · GameplayCue tags `GameplayCue.Weapon.Laser.*` · state tags
`State.*` · input tags `InputTag.*` · cooldown tags `Cooldown.*`.
