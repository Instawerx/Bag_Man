# 60 — Equipment & FX: Equipment Attachment, Jiggle Physics, Footstep Dispatcher, Vehicle Deformation

## Inventory Equipment Item Attachment (Data-Driven)

Topology:
```
[UI/Inventory Data Table] -> [BPC_EquipmentManager] -> (Spawns) -> [BP_Equippable_Base]
                                    |
                    (Queries Sockets & IK Offsets)
                                    v
                     [Character AnimInstance / Control Rig]
```

### FEquippableItemData (struct)
- `Item_Mesh` (Soft Obj Ref | Static/Skeletal Mesh)
- `Attachment_Socket` (Name): character skeleton target (e.g. `Spine_03_Holster`, `Hand_R_Grip`)
- `LeftHand_IK_Socket_Name` (Name): weapon-relative socket for left-hand IK (e.g. `IK_LeftHand_Target`)
- `Grip_Transform_Offset` (Transform): fine-tune weapon-to-hand alignment

```
Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CustomEvent Name="Event_EquipItemToSlot"
   CustomFunctionName="Execute_EquipItemToSlot"
   Pins(0)=(PinName="Then",PinType=(PinCategory="exec"),Direction="EGPD_Output")
   Pins(1)=(PinName="ItemData",PinType=(PinCategory="struct",PinSubCategory="FEquippableItemData"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_SpawnActorFromClass"
   FunctionReference=(Name="SpawnActorFromClass")
   Pins(0)=(PinName="execute",PinType=(PinCategory="exec"),Direction="EGPD_Input")
   Pins(1)=(PinName="Class",PinType=(PinCategory="class",PinSubCategoryClass=/Script/Engine.Actor),Direction="EGPD_Input")
   Pins(2)=(PinName="SpawnTransform",PinType=(PinCategory="struct",PinSubCategory="Transform"),Direction="EGPD_Input")
   Pins(3)=(PinName="ReturnValue",PinType=(PinCategory="object",PinSubCategoryClass=/Script/Engine.Actor),Direction="EGPD_Output")
   Pins(4)=(PinName="then",PinType=(PinCategory="exec"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_K2_AttachToComponent"
   FunctionReference=(Name="K2_AttachToComponent")
   Pins(0)=(PinName="execute",PinType=(PinCategory="exec"),Direction="EGPD_Input")
   Pins(1)=(PinName="Target",PinType=(PinCategory="object",PinSubCategoryClass=/Script/Engine.SceneComponent"),Direction="EGPD_Input")
   Pins(2)=(PinName="Parent",PinType=(PinCategory="object",PinSubCategoryClass=/Script/Engine.SceneComponent"),Direction="EGPD_Input")
   Pins(3)=(PinName="SocketName",PinType=(PinCategory="name"),Direction="EGPD_Input")
   Pins(4)=(PinName="LocationRule",PinType=(PinCategory="byte",PinSubCategory="/Script/Engine.EAttachmentRule"),Direction="EGPD_Input",DefaultValue="SnapToTarget")
   Pins(5)=(PinName="RotationRule",PinType=(PinCategory="byte",PinSubCategory="/Script/Engine.EAttachmentRule"),Direction="EGPD_Input",DefaultValue="SnapToTarget")
   Pins(6)=(PinName="ScaleRule",PinType=(PinCategory="byte",PinSubCategory="/Script/Engine.EAttachmentRule"),Direction="EGPD_Input",DefaultValue="KeepWorld")
   Pins(7)=(PinName="bWeldSimulatedBodies",PinType=(PinCategory="bool"),Direction="EGPD_Input",DefaultValue="true")
   Pins(8)=(PinName="then",PinType=(PinCategory="exec"),Direction="EGPD_Output")
End Object
```

Pipeline:
1. **Clear Existing Slot**: if `Current_Equipped_Actor` is valid → `DestroyActor`.
2. **Async Loading**: `Async Load Asset` on the `Item_Mesh` soft pointer — no frame hiccups during fast inventory swaps.
3. **Spawn & Glue**: `SpawnActorFromClass` → `K2_AttachToComponent` to the skeletal mesh at `Attachment_Socket`.
4. **Expose to IK**: cache `LeftHand_IK_Socket_Name`; AnimInstance reads it to drive the left-hand Control Rig constraint.

## Procedural Soft-Body Jiggle Physics (Pouches/Holsters/Capes/Straps)

Spring-mass solver responding to character acceleration, replacing keyed secondary animation.

Variables: `Jiggle_Damping` (Float | 25.0); `Jiggle_Stiffness` (Float | 150.0); `Jiggle_Bone_Chain` (Bone Name array, e.g. `[backpack_strap_01, backpack_strap_02]`).

```
Begin Object Class=/Script/AnimGraph.AnimNode_AnimDynamics Name="AnimNode_JigglePhysics_Equipment"
   SourceRigClass=/Script/Engine.AnimDynamicsClass
   Pins(0)=(PinName="Source",PinType=(PinCategory="struct",PinSubCategory="PoseLink"),Direction="EGPD_Input")
   Pins(1)=(PinName="ExternalForce",PinType=(PinCategory="struct",PinSubCategory="Vector"),Direction="EGPD_Input")
   Pins(2)=(PinName="Damping",PinType=(PinCategory="float"),Direction="EGPD_Input",Value="Jiggle_Damping")
   Pins(3)=(PinName="Stiffness",PinType=(PinCategory="float"),Direction="EGPD_Input",Value="Jiggle_Stiffness")
   Pins(4)=(PinName="Result",PinType=(PinCategory="struct",PinSubCategory="PoseLink"),Direction="EGPD_Output")
   bFrameRateIndependent=true
   bUsePlanarLimit=true
   SimulationSpace=ComponentSpace
End Object
```

(Native AnimDynamics or KawaiiPhysics nodes — physics evaluates on worker threads, off the game thread.)

Flow: character transform delta feeds the node → each `Jiggle_Bone_Chain` joint treated as a local spring-mass anchor (swing under momentum on jumps/decelerations) → **planar collision limits** on the node stop straps clipping through body armor.

**Polish — Velocity-Based Inertial Scalers**: hook `Jiggle_Stiffness`/damping to velocity magnitude; at a full stop, scale damping up so equipment settles instantly — no distracting wobble in idle/dialogue shots.

## Procedural Footstep Audio + Particle Surface Dispatcher

Reads contact events from anim notifies, queries physical surface material, dispatches the right SFX/VFX/decal — no hard asset refs in character classes.

### S_SurfaceImpactFX (Data Table row)
- `Surface_Type` (EPhysicalSurface): Concrete, Mud, Wood, Water, …
- `Impact_Audio` (Soft Obj | MetaSound Source / Sound Base)
- `Footprint_Decal` (Soft Obj | Material Interface)
- `Splash_Particles` (Soft Obj | Niagara System)

```
Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CustomEvent Name="Event_OnFootstepTriggered"
   CustomFunctionName="Execute_FootstepDispatcher"
   Pins(0)=(PinName="Then",PinType=(PinCategory="exec"),Direction="EGPD_Output")
   Pins(1)=(PinName="FootBoneName",PinType=(PinCategory="name"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_SurfaceDownTrace"
   FunctionReference=(Name="LineTraceSingle")
   Pins(0)=(PinName="execute",PinType=(PinCategory="exec"),Direction="EGPD_Input")
   Pins(1)=(PinName="Start",PinType=(PinCategory="struct",PinSubCategory="Vector"),Direction="EGPD_Input")
   Pins(2)=(PinName="End",PinType=(PinCategory="struct",PinSubCategory="Vector"),Direction="EGPD_Input")
   Pins(3)=(PinName="TraceChannel",PinType=(PinCategory="byte",PinSubCategory="/Script/Engine.ECollisionChannel"),Direction="EGPD_Input",DefaultValue="ECC_Visibility")
   Pins(4)=(PinName="bReturnPhysicalMaterial",PinType=(PinCategory="bool"),Direction="EGPD_Input",DefaultValue="true")
   Pins(5)=(PinName="OutHit",PinType=(PinCategory="struct",PinSubCategory="HitResult"),Direction="EGPD_Output")
   Pins(6)=(PinName="then",PinType=(PinCategory="exec"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_GetPhysicalMaterialSurface"
   FunctionReference=(Name="GetSurfaceType")
   Pins(0)=(PinName="PhysMat",PinType=(PinCategory="object",PinSubCategoryClass=/Script/Engine.PhysicalMaterial"),Direction="EGPD_Input")
   Pins(1)=(PinName="ReturnValue",PinType=(PinCategory="byte",PinSubCategory="/Script/Engine.EPhysicalSurface"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_GetDataTableRow"
   FunctionReference=(Name="GetDataTableRow")
   Pins(0)=(PinName="execute",PinType=(PinCategory="exec"),Direction="EGPD_Input")
   Pins(1)=(PinName="DataTable",PinType=(PinCategory="object",PinSubCategoryClass=/Script/Engine.DataTable"),Direction="EGPD_Input")
   Pins(2)=(PinName="RowName",PinType=(PinCategory="name"),Direction="EGPD_Input")
   Pins(3)=(PinName="OutRow",PinType=(PinCategory="struct"),Direction="EGPD_Output")
   Pins(4)=(PinName="then",PinType=(PinCategory="exec"),Direction="EGPD_Output")
End Object
```

Flow: walk/run anim notify fires `Execute_FootstepDispatcher` with the bone name → vertical trace down **50.0 cm** with **Return Physical Material = True** → surface type enum → name → Data Table row → asynchronously load + spawn: `SpawnSoundAtLocation`, `SpawnSystemAtLocation` (Niagara), `SpawnDecalAtLocation` aligned to the trace hit normal.

**Polish — Velocity-Proportional Impact Scalers**: read velocity at the footstep event; multiply MetaSound params and Niagara spawn rates by the normalized scalar (volume ≈ 0.2 crouched → 1.2 sprinting) — scuffs vs heavy sprint steps sound distinct.

## Procedural Vehicle Damage & Debris Deformation

Real-time impact evaluation modifying vertex offsets / localized material masks — no pre-baked damage meshes.

### FVehicleDamageProfile (struct)
- `Deformation_Stiffness` (Float | 500.0): panel resistance to crushing.
- `Max_Deformation_Radius` (Float | 120.0): max area per high-impact puncture.
- `Debris_Class` (Soft Class | Niagara System or StaticMesh).

```
Begin Object Class=/Script/BlueprintGraph.EdGraphNode_ComponentBoundEvent Name="Event_OnVehicleHit"
   ComponentPropertyClass=/Script/Engine.PrimitiveComponent
   ComponentPropertyName="VehicleMesh"
   DelegatePropertyName="OnComponentHit"
   Pins(0)=(PinName="Then",PinType=(PinCategory="exec"),Direction="EGPD_Output")
   Pins(1)=(PinName="HitResult",PinType=(PinCategory="struct",PinSubCategory="HitResult"),Direction="EGPD_Output")
   Pins(2)=(PinName="NormalImpulse",PinType=(PinCategory="struct",PinSubCategory="Vector"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_CalculateImpactForce"
   FunctionReference=(Name="VSize")
   Pins(0)=(PinName="A",PinType=(PinCategory="struct",PinSubCategory="Vector"),Direction="EGPD_Input") # NormalImpulse
   Pins(1)=(PinName="ReturnValue",PinType=(PinCategory="float"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_TransformHitToLocalMeshSpace"
   FunctionReference=(Name="InverseTransformPoint")
   Pins(0)=(PinName="T",PinType=(PinCategory="struct",PinSubCategory="Transform"),Direction="EGPD_Input") # Vehicle mesh world transform
   Pins(1)=(PinName="InPoint",PinType=(PinCategory="struct",PinSubCategory="Vector"),Direction="EGPD_Input") # HitResult.ImpactPoint
   Pins(2)=(PinName="ReturnValue",PinType=(PinCategory="struct",PinSubCategory="Vector"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_SetMaterialDamageParameter"
   FunctionReference=(Name="SetScalarParameterValue")
   Pins(0)=(PinName="execute",PinType=(PinCategory="exec"),Direction="EGPD_Input")
   Pins(1)=(PinName="ParameterName",PinType=(PinCategory="name"),Direction="EGPD_Input",DefaultValue="DamageMaskRadius")
   Pins(2)=(PinName="Value",PinType=(PinCategory="float"),Direction="EGPD_Input")
   Pins(3)=(PinName="then",PinType=(PinCategory="exec"),Direction="EGPD_Output")
End Object
```

Flow: `OnComponentHit` → `VSize(NormalImpulse)`; > threshold (e.g. **100000.0**) → deformation sequence → `InverseTransformPoint` converts impact point to mesh-local space → drive dynamic material instance + vertex offset array (vertices shift inward along impact direction, torn-metal/scraped-paint mask blends in) → simultaneously `SpawnSystemAtLocation` debris burst (metal fragments, glass) oriented to `HitResult.ImpactNormal`.

**Polish — Deformation Momentum Preservation**: at extreme speeds, read pre/post-collision velocity delta → camera shake sequence + brief Radial Force Impulse pushing nearby light props/trash/dust away from the crash epicenter.
