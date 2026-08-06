# AFL — PRO MOD CHARACTER BRIEF (dev build doc)

**Status:** BUILD DOC — operator-approved 2026-07-30. How to build a Pro Mod character (pawn + movement + true
FBIK + the 8-color axis). Extends `AFL_IDENTITY_PRODUCTION_LINE.md` (data-only character build) and
`AFL_COLOR_REGISTRY_MIGRATION_PLAN.md` (color system). Parent: `IRONICS_GAME_MODES_SSOT.md`.
First build: **1 pilot character** (prove end-to-end, then expand).

## 1. Pro pawn variant

- **`B_Hero_BagMan_Pro`** — child/duplicate of `Plugins/GameFeatures/AFLBagMan/Content/Characters/B_Hero_BagMan.uasset`
  with the `AFLDismemberComponent` **removed** from its component list (kills the gib layer structurally). Everything
  else — mesh, ASC, input, weapon wiring — stays.
- **`DA_AFL_PawnData_Hero_Pro`** — duplicate of `DA_AFL_PawnData_Hero_Default`: `PawnClass → B_Hero_BagMan_Pro`;
  AbilitySets += `DA_AFL_AbilitySet_Movement_Dash/Climb/Interaction`.

## 2. Enhanced movement ("faster/stronger/smoother")

Reuse `UAFLCharacterMovementComponent` (Config=Game, subclass of `ULyraCharacterMovementComponent`) +
the `AFLOverdriveComponent` speed-multiplier pattern. Expose as tunables on a thin **`DA_AFL_MoveProfile_Pro`**
(or the Pro MoveComp CDO). Starting targets (tune in playtest):

| Prop | Base | Pro start |
|---|---|---|
| `MaxWalkSpeed` | ~500 | 650–720 |
| `MaxAcceleration` | ~2048 | 3000–3600 |
| `BrakingDecelerationWalking` | ~2000 | 2600+ |
| `GroundFriction` | 8 | 8–10 |
| Overdrive `SpeedMultiplier` | 1.0 | 1.15–1.3 |

Baseline = passive (bake into the Pro MoveComp defaults). Reserve `AFLOverdriveComponent` for a sprint/dash buff.

## 3. True FBIK — NEW (the core Pro-Mod upgrade)

**Status: MISSING today.** The project has only per-limb Two-Bone IK + Aim + Lyra trace foot-placement
(`CR_AFL_IRONICS`, aliased "CR_AFL_CoreIK") + a left-hand support channel (`UAFLWeaponIKComponent`). `IK_Mannequin`
exists but is retarget-time only.

**Build:**
1. **`CR_ProMod_FBIK`** — a new Control Rig with a **Full-Body IK (PBIK)** solver over the shared `SK_Mannequin`
   hierarchy: pelvis + `spine_01..` + `upperarm/lowerarm/hand_l/r` + `thigh/calf/foot_l/r` effectors + foot
   planting. One rig serves every Pro Mod character (they share the skeleton).
2. **FBIK delivery component** (C++) — mirror `UAFLWeaponIKComponent`'s `ResolveOwnerControlRig` +
   `SetControlValue` push; expose pelvis/spine/foot effector controls; `TG_PostPhysics`.
3. **Wiring** — evaluate `CR_ProMod_FBIK` as an **added** Control Rig node *after* the existing solve (post-process
   or an extra anim-graph node). Add rig + component to Pro pawns via the Experience `GameFeatureAction_AddComponents`
   (the same pattern that adds `UAFLWeaponIKComponent`).
4. **Isolation rule (load-bearing):** do **NOT** modify `CR_AFL_IRONICS` or re-pin the shared `ABP_Mannequin_Base`
   graph — its controls drive weapon/grab IK on **every** character; a regression breaks all of them. Keep FBIK
   additive + gated to Pro/Melee. The existing weapon/hand IK stays; FBIK adds the full-body pose + foot solve on top.
5. **Verify:** foot planting on slopes/steps + full-body pose on the Pro pawn; weapon IK still correct; stock/Haywire
   characters unchanged.

Critical files: `Content/Characters/Heroes/IRONICS/Animations/CR_AFL_IRONICS.uasset` (do not modify),
`Content/Characters/Heroes/Mannequin/Animations/ABP_Mannequin_Base.uasset` (evaluation point),
`Plugins/GameFeatures/AFLMovement/.../Interaction/AFLWeaponIKComponent.{h,cpp}` (delivery pattern to mirror).

## 4. Character production (data-only — per `AFL_IDENTITY_PRODUCTION_LINE.md` 7-point checklist)

Per new Pro Mod character: (1) `Cosmetic.Brand.<NAME>` tag in `Config/DefaultGameplayTags.ini`; (2) `DA_AFL_CharacterPartMap`
row `AFL.Character.<Name>`/`AFL.Team.<Name>` → `B_AFL_Robot_<NAME>_C`; (3) `DA_AFL_BrandEdgeMap` signature-finish row;
(4) `B_AFL_Robot_<NAME>` BP (clone of `B_AFL_Robot_IRONICS`, parent `AAFLCharacterPartActor`); (5) neutralized
body/limbs MI; (6) logo texture; (7) `DA_AFL_CosmeticCatalog` rows. Body mesh gen via
`Content/AFL/_Bridge/Rodin/rodin.py` reskinned onto shared `SK_Mannequin` (`bUniqueBodyUVs=true`). Resolution spine
reused as-is: `UAFLCharacterPartSelectorComponent → UAFLCharacterPartMap → AAFLCharacterPartActor → UAFLSkinColorControllerComponent`.

## 5. The 8-color axis (base + Standard 6 + Neon Orange)

Same "1 base + N colors" methodology as the hand cannons. 8 = base(1) + Standard 6 + Neon Orange.

- **Standard 6** registry rows (already defined): NeonBlue `#006BFF` · NeonGreen `#00FF40` · NeonYellow `#FFE61A` ·
  NeonRed `#FF0D0D` · NeonPink `#FF1A99` · NeonPurple `#9900FF`.
- **Neon Orange = `Cosmetic.Identity.Solar` `#FF7300`** (PrimaryColor 1.00,0.45,0.00) — the tag + Primary/Accent
  exist, **but there is NO edge-ramp/emissive preset for Solar**. → **Author the Solar edge-ramp/emissive preset**
  so it renders like the other 7 (this is the one net-new color asset).
- Mechanism: one `FAFLColorIdentity` row + one `Cosmetic.Identity.*` tag per color in
  `Content/BagMan/Cosmetics/DA_AFL_ColorIdentityRegistry.uasset`. Applied via `FAFLSkinFinish::FindToneForParam`
  (`NeonColor → EdgeGlowColor`) through `AAFLCharacterPartActor::ApplySkinColor` (owned-MID push). No per-surface
  asset edits. **Tags resolve only after an editor relaunch** — author tags first.

## 6. Tint caveat (do not put on the pilot critical path)

The color-registry **SKIN/BODY** migration is spec-locked/unbuilt (`AFL_COLOR_REGISTRY_MIGRATION_PLAN.md`) — bodies
still read baked `UAFLSkinColorAsset` finishes, and a **multi-part** Rodin body won't tint from a single MID until
reskinned to a master. For the pilot: **reskin to a single-MID master** (recommended) or ship 8 baked finishes.
Track the full registry→skin migration as breadth (tracker P4-3), off the pilot path.

## 7. Risks
- FBIK regression on the shared rig (§3.4) — keep additive + PIE-verify stock characters.
- Editor relaunch needed for new tags (§5).
- Phantom-bone/import discipline on new Rodin bodies (shared `SK_Mannequin`, `bUniqueBodyUVs`, no extra bones) — the
  FBIK rig depends on a clean shared skeleton.
</content>
