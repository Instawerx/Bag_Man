# 10 — Weapons & Aiming: Procedural Recoil, ADS Sight Alignment, Weapon Wall Collision

## Procedural Recoil Control Rig Graph

Applies structural offsets directly to hand/weapon bones inside RigVM — bypasses static keyframe animation for dynamic, variable-driven kickback.

Topology:
```
[Camera/Input Vector] -> [BPC_InteractionHandler] --(Line Trace Interface Check)--> [Valid Object Sockets]
                               |
                        (Triggers Weapon Fire)
                               v
                       [AnimInstance Runtime] --(Pushes Delta Offsets)--> [Control Rig Recoil Node]
```

Variables (Control Rig): `Recoil_Target_Offset` (Transform | Input — raw kickback translation/rotation from firing Blueprint); `Recoil_Current_Offset` (Transform | Internal — current interpolated state); `Recoil_Recovery_Speed` (Float | Input | 12.0).

```
Begin Object Class=/Script/ControlRig.RigVMGraph Name="RigVMGraph_ProceduralRecoil"
   # Step 1: Interpolate current offset to target using Delta Time (frame-rate independent)
   Begin Node Type=RigVMModelFunctionNode Name="Function_InterpolateRecoil"
      FunctionName="MathTransformInterpTo"
      Pins(0)=(Name="Current",Direction=Input,Value="Recoil_Current_Offset")
      Pins(1)=(Name="Target",Direction=Input,Value="Recoil_Target_Offset")
      Pins(2)=(Name="DeltaTime",Direction=Input,Value="AbsoluteDeltaTime")
      Pins(3)=(Name="InterpSpeed",Direction=Input,Value="Recoil_Recovery_Speed")
      Pins(4)=(Name="Result",Direction=Output,Value="Recoil_Current_Offset")
   End Node

   # Step 2: Extract current bone transform of the main weapon grip (right hand)
   Begin Node Type=RigVMModelFunctionNode Name="Function_GetHandTransform"
      FunctionName="GetBoneTransform"
      Pins(0)=(Name="Bone",Direction=Input,DefaultValue="hand_r")
      Pins(1)=(Name="Space",Direction=Input,DefaultValue="EBoneControlTransformSpace::LocalSpace")
      Pins(2)=(Name="Transform",Direction=Output)
   End Node

   # Step 3: Accumulate interpolated recoil offset onto the base hand animation
   Begin Node Type=RigVMModelFunctionNode Name="Function_AccumulateRecoil"
      FunctionName="MathTransformMultiply"
      Pins(0)=(Name="A",Direction=Input)
      Pins(1)=(Name="B",Direction=Input,Value="Recoil_Current_Offset")
      Pins(2)=(Name="Result",Direction=Output)
   End Node

   # Step 4: Re-inject calculated transform back into the hierarchy
   Begin Node Type=RigVMModelFunctionNode Name="Function_SetRecoilTransform"
      FunctionName="SetBoneTransform"
      Pins(0)=(Name="Bone",Direction=Input,DefaultValue="hand_r")
      Pins(1)=(Name="Transform",Direction=Input)
      Pins(2)=(Name="Space",Direction=Input,DefaultValue="EBoneControlTransformSpace::LocalSpace")
      Pins(3)=(Name="PropagateToChildren",Direction=Input,DefaultValue="true")
   End Node
End Object
```

**Polish — Decoupled Camera Shake Shunt**: never rely on camera shake to simulate gun power. Feed a small fraction of `Recoil_Target_Offset` pitch directly into the controller's Aim Pitch Input for clean, physics-driven aim compensation mirroring the weapon mesh motion.

## Procedural Dynamic ADS Sight Alignment

Eliminates static aiming animations: reads the world transform of the physical optic socket and uses an inverse transform to align the optic to camera-viewport center.

Topology:
```
[Camera Forward Line View] -> [BPC_AimAlignmentComponent] ---> [AnimInstance Thread-Safe Engine]
                                            |
                             (Calculates Camera-to-Optic Delta)
                                            v
                                [Control Rig Optic Target]
```

Config: `Optic_Aim_Socket` — socket centered in the scope lens, oriented +X along barrel. `ADS_Blend_Alpha` (Float | Input): 0.0 hipfire → 1.0 fully aimed.

```
Begin Object Class=/Script/ControlRig.RigVMGraph Name="RigVMGraph_ProceduralADS"
   # Step 1: Query exact current world transform of the physical optic socket
   Begin Node Type=RigVMModelFunctionNode Name="Function_GetOpticWorldTransform"
      FunctionName="GetBoneTransform"
      Pins(0)=(Name="Bone",Direction=Input,DefaultValue="Optic_Aim_Socket")
      Pins(1)=(Name="Space",Direction=Input,DefaultValue="EBoneControlTransformSpace::WorldSpace")
      Pins(2)=(Name="Transform",Direction=Output)
   End Node

   # Step 2: Delta between camera view center and physical optic
   Begin Node Type=RigVMModelFunctionNode Name="Function_CalculateADSOffset"
      FunctionName="MathTransformInverse"
      Pins(0)=(Name="Value",Direction=Input,Value="Camera_Center_World_Transform")
      Pins(1)=(Name="Parent",Direction=Input) # Connected to Optic World Transform output
      Pins(2)=(Name="Result",Direction=Output)
   End Node

   # Step 3: Inject offset into the right-hand weapon-holder chain
   Begin Node Type=RigVMModelFunctionNode Name="Function_ApplyADSOffset"
      FunctionName="SetBoneTransform"
      Pins(0)=(Name="Bone",Direction=Input,DefaultValue="hand_r")
      Pins(1)=(Name="Transform",Direction=Input) # Blended by ADS_Blend_Alpha
      Pins(2)=(Name="Space",Direction=Input,DefaultValue="EBoneControlTransformSpace::WorldSpace")
      Pins(3)=(Name="Weight",Direction=Input,Value="ADS_Blend_Alpha")
   End Node
End Object
```

Flow: read live camera world transform → `MathTransformInverse` yields the delta the gun must move to meet the eye vector → drive `hand_r` in world space; the left hand follows via its Two-Bone IK solver. Optic snaps to screen center regardless of weapon model or scope attachment height.

**Polish — Procedural Breathing & Sway**: pure eye-to-optic alignment looks rigid. Add a multi-octave Perlin noise block in the Control Rig, multiplied by current stamina, applied as a tiny floating positional offset on the weapon socket *before* the ADS inverse — organic sway mimicking breathing/exertion.

## Dynamic Weapon Wall-Collision (High/Low Ready)

Measures barrel-tip-to-obstacle distance; procedurally rotates the weapon up (High Ready) or down (Low Ready) into a compressed stance to stop barrel clipping.

Variables: `Weapon_Collision_Alpha` (Float | Internal, 0.0 unobstructed → 1.0 fully compressed); `Preferred_Stance_State` (Enum: High_Ready / Low_Ready); `Barrel_Clearance_Length` (Float | 85.0 — chest to barrel tip).

```
Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CustomEvent Name="Event_AnalyzeWeaponProximity"
   CustomFunctionName="Execute_WeaponProximityScan"
   Pins(0)=(PinName="Then",PinType=(PinCategory="exec"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_GetWeaponMuzzleTransform"
   FunctionReference=(Name="GetSocketTransform")
   Pins(0)=(PinName="Target",PinType=(PinCategory="object",PinSubCategoryClass=/Script/Engine.SceneComponent"),Direction="EGPD_Input")
   Pins(1)=(PinName="InSocketName",PinType=(PinCategory="name"),Direction="EGPD_Input",DefaultValue="Muzzle_Socket")
   Pins(2)=(PinName="ReturnValue",PinType=(PinCategory="struct",PinSubCategory="Transform"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_WeaponLineTrace"
   FunctionReference=(Name="LineTraceSingle")
   Pins(0)=(PinName="execute",PinType=(PinCategory="exec"),Direction="EGPD_Input")
   Pins(1)=(PinName="Start",PinType=(PinCategory="struct",PinSubCategory="Vector"),Direction="EGPD_Input")
   Pins(2)=(PinName="End",PinType=(PinCategory="struct",PinSubCategory="Vector"),Direction="EGPD_Input")
   Pins(3)=(PinName="TraceChannel",PinType=(PinCategory="byte",PinSubCategory="/Script/Engine.ECollisionChannel"),Direction="EGPD_Input",DefaultValue="ECC_Visibility")
   Pins(4)=(PinName="OutHit",PinType=(PinCategory="struct",PinSubCategory="HitResult"),Direction="EGPD_Output")
   Pins(5)=(PinName="then",PinType=(PinCategory="exec"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_NormalizeProximityRange"
   FunctionReference=(Name="MapRangeClamped")
   Pins(0)=(PinName="Value",PinType=(PinCategory="float"),Direction="EGPD_Input")
   Pins(1)=(PinName="InLeft",PinType=(PinCategory="float"),Direction="EGPD_Input",DefaultValue="85.0")  # Barrel clearance threshold
   Pins(2)=(PinName="InRight",PinType=(PinCategory="float"),Direction="EGPD_Input",DefaultValue="25.0") # Maximum compression distance
   Pins(3)=(PinName="OutLeft",PinType=(PinCategory="float"),Direction="EGPD_Input",DefaultValue="0.0")
   Pins(4)=(PinName="OutRight",PinType=(PinCategory="float"),Direction="EGPD_Input",DefaultValue="1.0")
   Pins(5)=(PinName="ReturnValue",PinType=(PinCategory="float"),Direction="EGPD_Output")
End Object
```

Flow: raycast forward from spine along aiming axis to `Barrel_Clearance_Length` → on hit, map `HitResult.Distance` through the clamped range → alpha 0→1. Control Rig: High_Ready adds upward rotation (up to -65.0° pitch) + backward translation (-20.0 cm local X) on weapon grip bones (`hand_r_IK`); left hand follows the handguard socket via Two-Bone IK so hands never clip the weapon.

**Polish — Contextual Aim-Sway Modification**: when `Weapon_Collision_Alpha > 0.5`, scale procedural aim sway to zero and lock firing via an interface flag — communicates physical constraint and prevents projectiles spawning inside walls.
