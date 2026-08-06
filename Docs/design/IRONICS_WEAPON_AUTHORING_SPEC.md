# IRONICS — AFL WEAPON AUTHORING SPEC (conform-to authority)

**Purpose.** The single conform-to reference a NEW AFL weapon (esp. a power weapon for world
spawners) must satisfy. The **proven weapons are the authority, not prose docs** — every value
here is `[DISK]` (verified against current source/asset this session) or `[MEM]` (project memory,
flagged; re-verify in-editor before tuning). Doctrine: **✅ = watched in PIE** (2-client listen
server for anything networked), never "compiled/committed." Configure/harvest, don't reinvent.

Recorded 2026-07-19 from a live-editor readback + a disk source sweep. Supersedes scattered
weapon comments (several were stale — see §8).

---

## 1. The full asset + code chain (disk-verified end to end)

```
ID_<Name>   ULyraInventoryItemDefinition                                              [DISK]
  Fragments (the proven 5): InventoryFragment_EquippableItem, _QuickBarIcon,
                            _SetStats, _PickupIcon, _ReticleConfig
  └ EquippableItem.EquipmentDefinition ─► WID_<Name>
WID_<Name>  ULyraEquipmentDefinition (a BP subclass, class name WID_<Name>_C)          [DISK]
  ├ ActorsToSpawn[0].ActorToSpawn    = B_AFL_<Name>   (display actor, lives in /Game/) [DISK]
  ├ ActorsToSpawn[0].AttachSocket    = weapon_r                                        [DISK]
  ├ ActorsToSpawn[0].AttachTransform = Loc(0,0,0) · Rot(0,-90,0) · Scale(1,1,1)        [DISK base weapons]
  ├ InstanceType                     = B_WeaponInstance_AFL_<Name>                     [DISK]
  └ AbilitySetsToGrant[]             = AbilitySet_AFL_<Fire>                           [DISK]
       └ grants the fire GA on InputTag.Weapon.Fire (single) / .FireAuto (held)        [DISK]
B_WeaponInstance_AFL_<Name>  ─ BP child of ShooterCore B_WeaponInstance_Rifle|_Shotgun
                               ─► ULyraRangedWeaponInstance (C++)                       [DISK]
  ├ EquippedAnimSet   = INHERITED from the ShooterCore parent (do NOT re-author)        [MEM; parent chain DISK]
  └ implements IAFLLaserVisualProvider (per-weapon beam color feed to the cues)         [DISK]
B_AFL_<Name>  ─ BP child of /Game/Weapons/B_Weapon                                      [DISK]
  ├ SkeletalMesh component   ◄── THE MESH SWAP POINT (inherited-component override)     [DISK]
  ├ "Muzzle" socket (barrel tip) → beam/tracer cosmetic origin                          [DISK]
  ├ OverrideMaterials[0] = MI_AFL_Weapon_<Name> (child of M_AFL_Weapon_Master)          [DISK]
  └ beam weapons add an AFLBeamVisualComponent "BeamVisual", bReplicates=true           [DISK]
Loadout: ID_<Name> ∈ GA_AFL_GrantLoadout.Weapons[] → the hero spawns holding it         [DISK field]
```

**Two coexisting models — do not conflate (§8 Flag 4):**
- **Own-WID full chain** (a distinct GAMEPLAY weapon): the ItemDef equips *its own* WID →
  own instance + own AbilitySet + own mesh. This is PulseCarbine / ShotgunBeam / Pistol.
  **A NEW POWER WEAPON USES THIS MODEL.**
- **Cosmetic shared-base skin**: the ItemDef equips the *shared* `WID_AFL_ShotgunBeam`, and the
  visual identity comes off a `DA_AFL_Weapon_<Name>` cosmetic axis. This is Arclight/Voltaic/
  Ioncaster (their own WIDs exist but their ItemDefs bypass them — a mis-wire, not the target model).

---

## 2. The fire ability (the mechanically-distinct part)

**Base:** extend **`UAFLAG_Laser_Base`** (`UCLASS(Abstract)` : `ULyraGameplayAbility`) to inherit the
shared muzzle + tint resolver. The two live, proven leaves to copy from:
`UAFLAG_Laser_Pulse` (single-shot hitscan) and `UAFLAG_BeamChannel_v2` (held channel). **Never build
on the RETIRED `AFLAG_Laser_Beam`** (dead code, buggy static-only muzzle, stranded heat — §8 Flag 2).

**Required ctor setup (from `AFLAG_Laser_Pulse.cpp`, cite lines) `[DISK]`:**
- Net: `ReplicationPolicy=ReplicateNo`, `NetExecutionPolicy=LocalPredicted`,
  `InstancingPolicy=InstancedPerActor` (`:125-127`).
- **Bot-fire parity (mandatory):** add a `GameplayEvent` `AbilityTrigger` on `InputTag.Weapon.Fire`
  so AI (`BTS_Shoot`) can fire it (`:143-148`). Missing = bots can't shoot the weapon.
- Identity/owned/blocked tags (`:153-168`): `AbilityTags += Ability.Laser.*`,
  `ActivationOwnedTags += State.Firing.*`, `ActivationBlockedTags += State.Carrying`,
  `State.Weapon.ThrowRecovery`, `State.Match.Warmup`, `State.Match.Ended`.
- `DamageEffectClass = GE_AFL_Damage_*` (`:177`); `BaseDamage` per weapon (Pulse=18, `.h:75`).
- `TuningData` (recoil/spread DA) via `ConstructorHelpers` (`:185-190`) — **AFL bloom lives here,
  NOT in the instance's Lyra spread/heat** (§3 nuance).
- **`CharacterFireMontage` — MANDATORY, does NOT come free (`:196-201`, `.h:130`).** A custom AFL fire
  GA does not inherit stock `GA_Weapon_Fire`'s montage → without it the gun fires but the hero never
  pulls the trigger. Played **fire-and-forget** `ASC->PlayMontage(...)` in `OnTargetDataReadyCallback`
  (`:593-596`) — NOT `PlayMontageAndWait` (single-shot EndAbility would blend the kick out). Additive
  `AAT_ROTATION_OFFSET_MESH_SPACE` on `SK_Mannequin`; per class: `AM_MM_Rifle_Fire` /
  `AM_MM_Shotgun_Fire` / `AM_MM_Pistol_Fire`. Set on the fire-GA **BP child**.

**Muzzle (shared on the base) `[DISK]`:** `UAFLAG_Laser_Base::ResolveMuzzleLocation` walks attached
actors + character-mesh descendant **`UMeshComponent`s** (skeletal OR static — clones are skeletal) for
`MuzzleSocketCandidates = {Muzzle, Barrel, Slide}`, else falls back to `weapon_r` — **never world
origin**. Cosmetic only; trace/damage always fire from the camera.

**Fire path (LocalPredicted) `[DISK]`:** `ActivateAbility → CommitAbility →` bind
`AbilityTargetDataSetDelegate` → client `ClientPredictAndSend` (trace from camera, pack
`FAFLAbilityTargetData_Hitscan`, `CallServerSetReplicatedTargetData`) → server re-traces through
lag-comp (`ConfirmHit`) + applies `DamageEffectClass` via `UAFLDamageExecCalc`. **Server never reads
the client viewpoint.**

**Held-channel weapons** (charge/beam): `ActivationPolicy=WhileInputActive` + a `WaitInputRelease`
task, on `InputTag.Weapon.FireAuto`; teardown (RemoveGameplayCue + timers) in `EndAbility` for BOTH
end and cancel (§ the feature-map charge/cleanup contract).

**Cooldown `[DISK]`:** single-shot = `CooldownGameplayEffectClass` + `Cooldown.Weapon.*` on the GA CDO;
channel = `ReleaseCooldownEffectClass` applied in `EndAbility` (`BeamChannel_v2.cpp:474-499`). Same
shape as `GE_AFL_DashCooldown`. It is NOT granted via the AbilitySet.

---

## 3. The weapon instance

**No AFL C++ subclass exists** — AFL instances are **BP children of ShooterCore
`B_WeaponInstance_Rifle` / `_Shotgun`** → `ULyraRangedWeaponInstance` (C++) `[DISK]`. The C++ base
supplies spread (`SpreadExponent`, `HeatToSpreadCurve`, first-shot-accuracy, aim/move multipliers),
heat, `BulletsPerCartridge`, `MaxDamageRange=25000`, `DistanceDamageFalloff`, `MaterialDamageMultiplier`,
and the `ILyraAbilitySourceInterface` attenuation hooks.

**Conform-to nuance `[DISK-code]`:** AFL abilities do **not** consume the instance's Lyra spread/heat —
Pulse runs its own bloom from `DA_AFLPulseTuning`, beam traces raw from camera. So a new weapon's
**accuracy/recoil is tuned in the ability's `TuningData`**, not the instance curves. The instance's real
jobs: (a) carry the inherited `EquippedAnimSet`, (b) implement `IAFLLaserVisualProvider` (beam color).
Per-weapon **damage/fire-feel is tuned on the ability (BaseDamage / DamageEffect / fire cadence)**, not
the instance.

---

## 4. Sizing · Alignment · IK grip · Animation (the conform-to values)

**Grip / attach (identical on all 3 base weapons `[DISK]`):**
- Socket **`weapon_r`** (hero right-hand).
- AttachTransform **Loc (0,0,0) · Rot (0, −90, 0) · Scale (1,1,1)**. The −90° yaw aligns the weapon's
  local forward to the socket; because of it, translation axes do NOT map to world forward/up — **grip
  seating is operator-directed visual tuning, not agent-calculated** (ask "forward/up/left", tune one
  axis, converge; or gizmo-place and read back).
- **SIZE knob = `WID.ActorsToSpawn[].AttachTransform.Scale3D`** — engine-verified: `ULyraEquipmentInstance
  ::SpawnEquipmentActors` does `SetActorRelativeTransform(AttachTransform)` then attaches
  (`LyraEquipmentInstance.cpp:87-88`), **overwriting the display actor's root scale** → scaling
  `DefaultSceneRoot` is editor-preview-only, dead at runtime (the trap).
- **Origin-grip convention (the proven low-risk path):** stock Lyra meshes author the grip AT the mesh
  origin, so `AttachTransform = weapon_r + (−90 yaw) only`, **zero translation, scale 1** → correct by
  construction, no tuning. The deprecated Tripo carbine needed `Loc(25,0,-5)+Scale 0.7` (grip 25 cm
  off-origin — a multi-pass saga). **A new power weapon should reuse a stock/shared-skeleton origin-grip
  mesh so the grip is correct with no tuning.**
- Muzzle: barrel-tip socket named **`Muzzle`** (SK_Rifle ≈ +69.8 cm `[MEM]`); raw pack meshes w/o
  sockets fall back to `weapon_r` — add a `Muzzle` socket via the mesh asset API.

**Animation:**
- Equipped anim layer = `EquippedAnimSet` on the **instance, INHERITED** from the ShooterCore parent
  (`_Rifle → ABP_RifleAnimLayers`, `_Shotgun → shotgun layer`). A child storing no override = inherit,
  NOT empty — **do not re-author.** Targets `SK_Mannequin` (the robot body is CharacterParts leader-posed
  to it, so it matches).
- Trigger-pull/recoil = the ability's `CharacterFireMontage` (§2), not the layer.

**IK grip — DORMANT by doctrine.** A static two-handed hold = the **baked/authored hand POSE, not runtime
IK**. `bEnableLeftHandWeaponIK = false`, shipped off; both TwoBoneIK (elbow flip — axes were an un-mirrored
clone of the right arm) and FABRIK (over-stretch) failed the do-no-harm oracle. **A new weapon inherits the
baked hold — do NOT wire per-weapon left-hand IK.** If ever needed: a small blended correction to a
*reachable-near* dynamic target only, gated `targetDist ≤ 0.9 × armLen` (chain root `upperarm_l`), never a
persistent far-socket solve. Dormant left socket = `GripPoint_L`.

---

## 5. Authoring responsibility (who does what)

| Chain part | Owner | How |
|---|---|---|
| Fire ability **class** (a new mechanic) | **C++ (Claude Code)** | new `UAFLAG_Laser_Base` subclass; copy the Pulse/BeamChannel_v2 predict-and-send skeleton |
| Damage GE / Cooldown GE | **C++** (exist) or BP child | `GE_AFL_Damage_*`, `GE_AFL_Cooldown_*` |
| Fire-GA **BP child** (montage/tuning override) | Asset data | set `CharacterFireMontage`, `DamageEffectClass`, `TuningData` |
| `ID_`, `WID_`, `AbilitySet_AFL_<Fire>`, `WeaponPickupData_`, `DA_AFL_Weapon_` | Asset data (bridge/editor) | Lyra data assets; WID `AbilitySetsToGrant`+`ActorsToSpawn`+`InstanceType` |
| `B_WeaponInstance_AFL_<Name>` | Asset data (editor) | BP child of ShooterCore `_Rifle`/`_Shotgun`; inherit `EquippedAnimSet` |
| `B_AFL_<Name>` display actor (mesh/mat/sockets/BeamVisual) | **BP editor GUI (NOT bridge)** | mesh binding is an inherited-component override — `duplicate_asset` drops it, byte-grep is blind; set in the BP editor, verify in PIE after relaunch (§8 Flag 10) |
| The mesh + `Muzzle` socket | **Blender/Tripo, or harvest a stock skeletal mesh** | prefer origin-grip stock/shared-skeleton mesh; add `Muzzle` socket |
| Anim layer | **inherited — no authoring** | from the instance parent |
| Left-hand IK / grip pose | **no authoring** | baked pose inherited; IK dormant |
| Loadout membership | Asset data | (power weapons are NOT in the loadout — spawner-only) |

---

## 6. Conform-to CHECKLIST (a new weapon must satisfy ALL)

1. `ID_<Name>` has the **5-fragment** pattern (EquippableItem, QuickBarIcon, SetStats, PickupIcon, ReticleConfig).
2. `ID_` equips **its OWN** `WID_<Name>` (own-WID full-chain model, not the shared ShotgunBeam WID).
3. `WID` → own `InstanceType`, own `AbilitySetsToGrant`, `ActorsToSpawn = B_AFL_<Name> @ weapon_r`,
   AttachTransform Rot −90 / Scale set so the grip seats (origin-grip mesh ⇒ zero-translation, scale 1).
4. Fire GA extends `UAFLAG_Laser_Base` (or copies Pulse/BeamChannel_v2); has `CharacterFireMontage`,
   `DamageEffectClass`, LocalPredicted, the **bot-fire GameplayEvent trigger**, the blocked-tag set, and a
   cooldown GE. NOT built on `AFLAG_Laser_Beam`.
4b. Held/charge weapons: `WhileInputActive` + `WaitInputRelease`, all teardown in `EndAbility` (end+cancel).
5. Instance is a BP child of ShooterCore `_Rifle`/`_Shotgun`; `EquippedAnimSet` inherited (not re-authored).
6. Display actor mesh has a `Muzzle` socket; grip authored at origin; material = `MI_AFL_Weapon_*`.
7. No per-weapon left-hand IK; baked two-handed hold inherited.
8. `_Mobile` material variant exists (AFL PC/Console/Mobile rule).
9. **PIE-proven** on a controllable pawn (2-client if networked): equips, hero pulls trigger (montage),
   fires from the muzzle cosmetically + camera for damage, bots can fire it, cooldown gates, cleans up.

---

## 7. Current inventory (live readback 2026-07-19)

**16 `WID_` weapons, each with own mesh `B_AFL_*` + instance `B_WeaponInstance_AFL_*` @ `weapon_r`**,
grouped by **3 fire AbilitySets** (only 3 distinct mechanics exist):

| Fire AbilitySet | Weapons | ItemDef? | Loadout? |
|---|---|---|---|
| `AbilitySet_AFL_PulseFire` | PulseCarbine | ✅ own WID | ✅ |
| `AbilitySet_AFL_PulseFire_Pistol` | Pistol, DE42, Judge45, NFP16 | Pistol ✅; others ✗ | Pistol ✅ |
| `AbilitySet_AFL_BeamFire_Shotgun` | ShotgunBeam, Arclight, Voltaic, Ioncaster, Tempest, Vanguard, Breacher, GTM, PP9, N90, Remore | ShotgunBeam ✅ own; Arclight/Voltaic/Ioncaster ✅ but equip the SHARED ShotgunBeam WID; rest ✗ | ShotgunBeam ✅ |

Loadout (`GA_AFL_GrantLoadout.Weapons`): **PulseCarbine, ShotgunBeam, Pistol** (ActiveSlot 0).
So **13 non-loadout weapons have distinct meshes but NO distinct fire mechanic** — the "power weapon"
gap is a distinct FIRE ABILITY, not a mesh.

---

## 8. Flags — stale / trap findings (carry into every weapon task)

1. **`*Beam_v2*` assets DO NOT EXIST** (glob=0). The C++ `UAFLAG_BeamChannel_v2` is real; the live beam
   chain is `ID_AFL_ShotgunBeam → WID_AFL_ShotgunBeam → AbilitySet_AFL_BeamFire_Shotgun → GA_AFL_Beam_Shotgun`
   (BP child of `UAFLAG_BeamChannel_v2`). The loadout header comment "[PulseCarbine, Beam_v2]" and the
   factory doc "Prism Beam (Beam_v2)" are BOTH stale.
2. **`AFLAG_Laser_Beam` = RETIRED dead code** (buggy static-only muzzle, stranded heat). Never build on it.
3. **Instance parent varies:** ShotgunBeam parents to `_Shotgun`, Pulse to `_Rifle`. Verify per weapon;
   the anim layer follows the parent.
4. **Own-WID vs cosmetic-shared model** — a power weapon uses the own-WID full chain (§1).
5. **Input tags split:** single = `InputTag.Weapon.Fire`; held = `InputTag.Weapon.FireAuto`.
6. **`DA_AFL_LaserVisual_PrismBeam`** = retired-model asset; live beam color = `AFLBeamVisualComponent
   .BeamColorOverride` on the display actor. Don't reference the DA.
7. **Heat/overheat economy is stranded on the retired beam** (`GE_AFL_Heat_*`, `State.Overheated`).
   `BeamChannel_v2` has no heat. A power weapon wanting overheat must port it.
8. **Use `B_AFL_Carbine`** (SK_Rifle, origin grip) — the deprecated `B_AFL_PulseCarbine` (Tripo, off-origin,
   `GripPoint_L`) is on disk but not spawned.
9. **Cooldown is on the ability CDO**, not the AbilitySet (§2).
10. **Mesh-binding authoring trap:** the display-actor mesh is an inherited-component override — bridge
    `duplicate_asset` DROPS it, byte-grep is blind. Author mesh/material in the **BP editor GUI**; verify
    only after an editor relaunch, in PIE.
11. **`AttachTransform` numeric values are `[MEM]` for translation/rotation** (binary struct); only
    `weapon_r` + the −90 yaw were readback-confirmed this session. Re-read the WID transform in-editor
    before any grip tuning. The Scale3D-is-the-size-knob mechanism is engine-verified.
