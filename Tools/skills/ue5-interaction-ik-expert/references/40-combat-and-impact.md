# 40 — Combat & Impact: Melee Sweeps, Parry, Ragdoll Blending, Hit Reactions, Dismemberment

## Procedural Melee Weapon Collision (Anim-Notify-Gated Capsule Sweeps)

Avoids continuous trace ticks: an Anim Notify State activates shape sweeps only during active swing frames.

Config: weapon sockets `Base_Socket` (blade base) + `Tip_Socket` (tip); `SphereTraceMultiByChannel` or `CapsuleTraceMultiByChannel` on a dedicated channel (e.g. `ECC_GameTraceChannel_Weapon`).

```
Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CustomEvent Name="Event_ExecuteWeaponSweep"
   CustomFunctionName="Execute_WeaponSweep"
   Pins(0)=(PinName="Then",PinType=(PinCategory="exec"),Direction="EGPD_Output")
   Pins(1)=(PinName="WeaponMeshComponent",PinType=(PinCategory="object",PinSubCategoryClass=/Script/Engine.MeshComponent"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_GetSocketLocationBase"
   FunctionReference=(Name="GetSocketLocation")
   Pins(0)=(PinName="Target",PinType=(PinCategory="object",PinSubCategoryClass=/Script/Engine.SceneComponent"),Direction="EGPD_Input")
   Pins(1)=(PinName="InSocketName",PinType=(PinCategory="name"),Direction="EGPD_Input",DefaultValue="Base_Socket")
   Pins(2)=(PinName="ReturnValue",PinType=(PinCategory="struct",PinSubCategory="Vector"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_GetSocketLocationTip"
   FunctionReference=(Name="GetSocketLocation")
   Pins(0)=(PinName="Target",PinType=(PinCategory="object",PinSubCategoryClass=/Script/Engine.SceneComponent"),Direction="EGPD_Input")
   Pins(1)=(PinName="InSocketName",PinType=(PinCategory="name"),Direction="EGPD_Input",DefaultValue="Tip_Socket")
   Pins(2)=(PinName="ReturnValue",PinType=(PinCategory="struct",PinSubCategory="Vector"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_CapsuleTraceMultiByChannel"
   FunctionReference=(Name="CapsuleTraceMulti")
   Pins(0)=(PinName="execute",PinType=(PinCategory="exec"),Direction="EGPD_Input")
   Pins(1)=(PinName="Start",PinType=(PinCategory="struct",PinSubCategory="Vector"),Direction="EGPD_Input")
   Pins(2)=(PinName="End",PinType=(PinCategory="struct",PinSubCategory="Vector"),Direction="EGPD_Input")
   Pins(3)=(PinName="Radius",PinType=(PinCategory="float"),Direction="EGPD_Input",DefaultValue="12.0")
   Pins(4)=(PinName="HalfHeight",PinType=(PinCategory="float"),Direction="EGPD_Input",DefaultValue="45.0")
   Pins(5)=(PinName="TraceChannel",PinType=(PinCategory="byte",PinSubCategory="/Script/Engine.ECollisionChannel"),Direction="EGPD_Input",DefaultValue="ECC_WorldDynamic")
   Pins(6)=(PinName="OutHits",PinType=(PinCategory="struct",PinSubCategory="HitResult"),Direction="EGPD_Output")
   Pins(7)=(PinName="ReturnValue",PinType=(PinCategory="bool"),Direction="EGPD_Output")
   Pins(8)=(PinName="then",PinType=(PinCategory="exec"),Direction="EGPD_Output")
End Object
```

Flow: swing montage reaches active frames → Notify State calls `Execute_WeaponSweep` → query Base/Tip sockets → per-frame capsule volume traces the exact motion path → **Parry Evaluation Gate**: hit opponent weapon capsule while both in active attack/parry state → stop both swing montages, play synchronized weapon-rebound montages.

**Polish — Parry Time-Dilation Spark**: `SetGlobalTimeDilation(0.1)` for **0.05 s** + fast camera shake on a successful parry — physical friction and impact.

## IK-Driven Shield & Parry Alignment

Reads incoming strike angle, overrides default block animation to position the shield directly into the threat vector.

Variables: `Incoming_Strike_Direction` (Vector | Input — normalized character→attack direction); `Shield_IK_Alpha` (Float | 0.0 baseline block → 1.0 reactive parry override).

```
Begin Object Class=/Script/ControlRig.RigVMGraph Name="RigVMGraph_DynamicShieldParry"
   # Step 1: Directional space local to character orientation
   Begin Node Type=RigVMModelFunctionNode Name="Function_TransformToLocalSpace"
      FunctionName="MathTransformInverse"
      Pins(0)=(Name="Value",Direction=Input,Value="Incoming_Strike_Direction")
      Pins(1)=(Name="Parent",Direction=Input,DefaultValue="pelvis")
      Pins(2)=(Name="Result",Direction=Output)
   End Node

   # Step 2: Offset hand control toward localized threat vector
   Begin Node Type=RigVMModelFunctionNode Name="Function_CalculateShieldEffector"
      FunctionName="MathVectorScale"
      Pins(0)=(Name="Value",Direction=Input)
      Pins(1)=(Name="Factor",Direction=Input,DefaultValue="45.0") # Extends shield outward from body
      Pins(2)=(Name="Result",Direction=Output)
   End Node

   # Step 3: Two-Bone IK driving the shield limb
   Begin Node Type=RigVMModelFunctionNode Name="Function_ShieldTwoBoneIK"
      FunctionName="TwoBoneIK"
      Pins(0)=(Name="Item",Direction=Input,DefaultValue="hand_l")
      Pins(1)=(Name="Target",Direction=Input)
      Pins(2)=(Name="PrimaryAxis",Direction=Input,DefaultValue="(X=1.0,Y=0.0,Z=0.0)")
      Pins(3)=(Name="SecondaryAxis",Direction=Input,DefaultValue="(X=0.0,Y=1.0,Z=0.0)")
      Pins(4)=(Name="PoleVector",Direction=Input,DefaultValue="ctrl_elbow_l_Position")
      Pins(5)=(Name="Weight",Direction=Input,Value="Shield_IK_Alpha")
   End Node
End Object
```

**Polish — Parry Kinetic Energy Handoff**: read the delta between baseline shield rest pose and impact angle; feed the scalar into a custom Anim Notify altering post-process exposure limits and scaling camera rumble by reach distance.

## Procedural Ragdoll Physical Animation Blending

Mixes keyframed animation with real-time physics: knockback with ragdoll while limbs still drive toward the pose.

Prereqs: tuned Physics Asset; **Physical Animation Profile `Knockback_Profile`** with high resting Skeletal Strength (Linear/Angular Drive Strength ≈ 1000.0/1000.0) so the character stays rigid normally.

```
Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CustomEvent Name="Event_ApplyPhysicalKnockback"
   CustomFunctionName="Execute_PhysicalKnockback"
   Pins(0)=(PinName="Then",PinType=(PinCategory="exec"),Direction="EGPD_Output")
   Pins(1)=(PinName="ImpactForceVector",PinType=(PinCategory="struct",PinSubCategory="Vector"),Direction="EGPD_Output")
   Pins(2)=(PinName="HitBoneName",PinType=(PinCategory="name"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_SetPhysicalAnimationProfile"
   FunctionReference=(Name="SetPhysicalAnimationProfileByName")
   Pins(0)=(PinName="execute",PinType=(PinCategory="exec"),Direction="EGPD_Input")
   Pins(1)=(PinName="Target",PinType=(PinCategory="object",PinSubCategoryClass=/Script/Engine.PhysicalAnimationComponent"),Direction="EGPD_Input")
   Pins(2)=(PinName="BodyName",PinType=(PinCategory="name"),Direction="EGPD_Input",DefaultValue="pelvis")
   Pins(3)=(PinName="ProfileName",PinType=(PinCategory="name"),Direction="EGPD_Input",DefaultValue="Knockback_Profile")
   Pins(4)=(PinName="bIncludeChildren",PinType=(PinCategory="bool"),Direction="EGPD_Input",DefaultValue="true")
   Pins(5)=(PinName="then",PinType=(PinCategory="exec"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_SetAllBodiesBelowSimulatePhysics"
   FunctionReference=(Name="SetAllBodiesBelowSimulatePhysics")
   Pins(0)=(PinName="execute",PinType=(PinCategory="exec"),Direction="EGPD_Input")
   Pins(1)=(PinName="Target",PinType=(PinCategory="object",PinSubCategoryClass=/Script/Engine.SkeletalMeshComponent"),Direction="EGPD_Input")
   Pins(2)=(PinName="InBoneName",PinType=(PinCategory="name"),Direction="EGPD_Input")
   Pins(3)=(PinName="bNewSimulate",PinType=(PinCategory="bool"),Direction="EGPD_Input",DefaultValue="true")
   Pins(4)=(PinName="bIncludeSelf",PinType=(PinCategory="bool"),Direction="EGPD_Input",DefaultValue="true")
   Pins(5)=(PinName="then",PinType=(PinCategory="exec"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_AddImpulseToAllBodiesBelow"
   FunctionReference=(Name="AddImpulseToAllBodiesBelow")
   Pins(0)=(PinName="execute",PinType=(PinCategory="exec"),Direction="EGPD_Input")
   Pins(1)=(PinName="Target",PinType=(PinCategory="object",PinSubCategoryClass=/Script/Engine.SkeletalMeshComponent"),Direction="EGPD_Input")
   Pins(2)=(PinName="InBoneName",PinType=(PinCategory="name"),Direction="EGPD_Input")
   Pins(3)=(PinName="Impulse",PinType=(PinCategory="struct",PinSubCategory="Vector"),Direction="EGPD_Input")
   Pins(4)=(PinName="bVelocityChange",PinType=(PinCategory="bool"),Direction="EGPD_Input",DefaultValue="true")
   Pins(5)=(PinName="then",PinType=(PinCategory="exec"),Direction="EGPD_Output")
End Object
```

Flow: heavy hit → `SetPhysicalAnimationProfileByName` (lowers motor resistance) → `SetAllBodiesBelowSimulatePhysics` from `pelvis` (full-body) or `spine_01` (legs locked) → `AddImpulseToAllBodiesBelow` with **bVelocityChange = True** (bypasses mass for consistent knockback distance) → recovery: timeline ramps motor strengths back, pulling ragdoll bones into alignment with the standing state machine.

**Polish — Physics-to-Keyframe Standup Handshake**: at recovery, read simulated pelvis forward vector vs world to determine face-up/face-down; pick the matching recovery montage and blend from the ragdoll's last simulated pose.

## Procedural Hit-Reaction / Flinch Rig Graph

Lighter, non-ragdoll impacts: bend specific joints away from incoming attack vectors.

Inputs: `Hit_Direction_World` (Vector); `Hit_Intensity` (Float — maps to max bend angle); `Target_Impact_Bone` (Name — closest joint, e.g. `spine_03`, `clavicle_l`).

```
Begin Object Class=/Script/ControlRig.RigVMGraph Name="RigVMGraph_ProceduralFlinch"
   # Step 1: World hit vector -> local coordinate system of the hit bone
   Begin Node Type=RigVMModelFunctionNode Name="Function_TransformVectorToLocal"
      FunctionName="MathTransformInverse"
      Pins(0)=(Name="Value",Direction=Input,Value="Hit_Direction_World")
      Pins(1)=(Name="Parent",Direction=Input) # target bone global transform
      Pins(2)=(Name="Result",Direction=Output)
   End Node

   # Step 2: Localized offset -> Rotator (x Hit_Intensity)
   Begin Node Type=RigVMModelFunctionNode Name="Function_CreateFlinchRotator"
      FunctionName="MathRotatorMakeFromEuler"
      Pins(0)=(Name="Value",Direction=Input)
      Pins(1)=(Name="Result",Direction=Output)
   End Node

   # Step 3: Apply rotational displacement to the target bone
   Begin Node Type=RigVMModelFunctionNode Name="Function_ApplyProceduralFlinch"
      FunctionName="SetBoneTransform"
      Pins(0)=(Name="Bone",Direction=Input,Value="Target_Impact_Bone")
      Pins(1)=(Name="Transform",Direction=Input)
      Pins(2)=(Name="Space",Direction=Input,DefaultValue="EBoneControlTransformSpace::LocalSpace")
      Pins(3)=(Name="Weight",Direction=Input,Value="Flinch_Blend_Weight")
   End Node
End Object
```

**Polish — Asymmetric Spring Recovery**: never clear flinch offsets with `FInterpTo`. Spring Interp with stiffness **45.0**, damping **0.4** → immediate snap away from impact, organic settling bounce regaining stance.

## Procedural Dismemberment & Gore

### Data (FDismembermentGoreRow / FHeavyImpactGoreProfile)
- `Target_Bone_Branch` / `Severed_Joint_Bone` (Name): limb root (e.g. `calf_l`, `upperarm_r`, `thigh_r`, `neck`).
- `Bone_Cap_Mesh` (Soft Obj | StaticMesh): severed cross-section cap. `Bone_Cap_Skeletal_Index` (Int) variant for indexed wound geometry.
- `Bleed_Niagara_System` (Soft Obj | Niagara): looping arterial spray.
- `Blood_Decal_Material` (Soft Obj | Material Interface). `Fluid_Splat_Velocity_Scale` (Float | 2.5).

```
Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CustomEvent Name="Event_ExecuteLimbSeverance"
   CustomFunctionName="Execute_LimbSeverance"
   Pins(0)=(PinName="Then",PinType=(PinCategory="exec"),Direction="EGPD_Output")
   Pins(1)=(PinName="SeverBoneName",PinType=(PinCategory="name"),Direction="EGPD_Output")
   Pins(2)=(PinName="HitImpulseVector",PinType=(PinCategory="struct",PinSubCategory="Vector"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_HideBoneByName"
   FunctionReference=(Name="HideBoneByName")
   Pins(0)=(PinName="execute",PinType=(PinCategory="exec"),Direction="EGPD_Input")
   Pins(1)=(PinName="Target",PinType=(PinCategory="object",PinSubCategoryClass=/Script/Engine.SkeletalMeshComponent"),Direction="EGPD_Input")
   Pins(2)=(PinName="BoneName",PinType=(PinCategory="name"),Direction="EGPD_Input")
   Pins(3)=(PinName="PhysBodyOption",PinType=(PinCategory="byte",PinSubCategory="/Script/Engine.EPhysBodyOp"),Direction="EGPD_Input",DefaultValue="PBO_Disable")
   Pins(4)=(PinName="then",PinType=(PinCategory="exec"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_AttachBoneCapMesh"
   FunctionReference=(Name="AddComponentByClass")
   Pins(0)=(PinName="execute",PinType=(PinCategory="exec"),Direction="EGPD_Input")
   Pins(1)=(PinName="Class",PinType=(PinCategory="class",PinSubCategoryClass=/Script/Engine.StaticMeshComponent"),Direction="EGPD_Input",DefaultObject=/Script/Engine.StaticMeshComponent)
   Pins(2)=(PinName="then",PinType=(PinCategory="exec"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_SpawnNiagaraArterialSpray"
   FunctionReference=(Name="SpawnSystemAtLocation")
   Pins(0)=(PinName="execute",PinType=(PinCategory="exec"),Direction="EGPD_Input")
   Pins(1)=(PinName="SystemTemplate",PinType=(PinCategory="object",PinSubCategoryClass=/Script/Niagara.NiagaraSystem"),Direction="EGPD_Input")
   Pins(2)=(PinName="Location",PinType=(PinCategory="struct",PinSubCategory="Vector"),Direction="EGPD_Input")
   Pins(3)=(PinName="Rotation",PinType=(PinCategory="struct",PinSubCategory="Rotator"),Direction="EGPD_Input")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_LineTraceBloodDecal"
   FunctionReference=(Name="LineTraceSingle")
   Pins(0)=(PinName="execute",PinType=(PinCategory="exec"),Direction="EGPD_Input")
   Pins(1)=(PinName="Start",PinType=(PinCategory="struct",PinSubCategory="Vector"),Direction="EGPD_Input")
   Pins(2)=(PinName="End",PinType=(PinCategory="struct",PinSubCategory="Vector"),Direction="EGPD_Input")
   Pins(3)=(PinName="OutHit",PinType=(PinCategory="struct",PinSubCategory="HitResult"),Direction="EGPD_Output")
   Pins(4)=(PinName="then",PinType=(PinCategory="exec"),Direction="EGPD_Output")
End Object
```

Flow & PhysBodyOption semantics:
- **Cosmetic severance**: `HideBoneByName` with **PBO_Disable** — eliminates the hidden child limb's collision shapes, preventing ghost collisions.
- **Physical detachment**: **PBO_Term** — breaks the physics constraint at the joint, un-welding the severed body so it drops into the world as a detached physical ragdoll limb.
- Attach `Bone_Cap_Mesh` to remaining parent socket (e.g., `clavicle_l`) Snap-to-Target (async-load first).
- Spawn arterial Niagara at the cap; pipe `HitImpulseVector`/`HitVelocityVector` into the Niagara `User.VelocityVector` parameter so spray matches impact physics; multi-line trace cluster along impulse direction stamps `SpawnDecalAtLocation` blood splats on environment hits.

**Polish**:
- **Decoupled Ragdoll Limb Prop**: at the severance frame, spawn a separate physics-enabled skeletal prop matching the hidden limb and apply the original impulse — the severed limb flies with realistic momentum.
- **Blood-Drip Ground Decal Buffering**: attach a Niagara Collision Export Interface module to the spray; collision callbacks feed an asynchronous pool dispatcher stamping random splat decals — persistent gore at near-zero cost.
