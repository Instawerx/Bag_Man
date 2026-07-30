# 20 — Locomotion & Feet: Foot Placement IK, Pivot/Stop Blending, Aim Offset, Foot Lock, Edge Balancing

## Procedural Foot Placement IK (Slopes / Stairs)

### Pre-Evaluation Line Traces (AnimInstance Event Graph)

Per foot bone (`foot_l`, `foot_r`): query world location → Start Trace = location + (0,0,50); End Trace = location − (0,0,100) → line trace down → on hit, delta = `Hit_Location - Predicted_Animation_Foot_Location` → push to Control Rig inputs `Foot_Offset_L` / `Foot_Offset_R`.

```
Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CustomEvent Name="Event_EvaluateFootPlacement"
   CustomFunctionName="Execute_EvaluateFootPlacement"
   Pins(0)=(PinName="Then",PinType=(PinCategory="exec"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_GetSocketLocation"
   FunctionReference=(Name="GetSocketLocation")
   Pins(0)=(PinName="Target",PinType=(PinCategory="object",PinSubCategoryClass=/Script/Engine.SkeletalMeshComponent"),Direction="EGPD_Input")
   Pins(1)=(PinName="InSocketName",PinType=(PinCategory="name"),Direction="EGPD_Input")
   Pins(2)=(PinName="ReturnValue",PinType=(PinCategory="struct",PinSubCategory="Vector"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_MathVectorAdd"
   FunctionReference=(Name="Add_VectorVector")
   Pins(0)=(PinName="A",PinType=(PinCategory="struct",PinSubCategory="Vector"),Direction="EGPD_Input")
   Pins(1)=(PinName="B",PinType=(PinCategory="struct",PinSubCategory="Vector"),Direction="EGPD_Input")
   Pins(2)=(PinName="ReturnValue",PinType=(PinCategory="struct",PinSubCategory="Vector"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_MathVectorSubtract"
   FunctionReference=(Name="Subtract_VectorVector")
   Pins(0)=(PinName="A",PinType=(PinCategory="struct",PinSubCategory="Vector"),Direction="EGPD_Input")
   Pins(1)=(PinName="B",PinType=(PinCategory="struct",PinSubCategory="Vector"),Direction="EGPD_Input")
   Pins(2)=(PinName="ReturnValue",PinType=(PinCategory="struct",PinSubCategory="Vector"),Direction="EGPD_Output")
End Object
```

### RigVM Foot Evaluation Layer

```
Begin Object Class=/Script/ControlRig.RigVMGraph Name="RigVMGraph_FootIKPlacement"
   # Step 1: Pelvis (hip) drop adjustment so knees bend properly on low steps
   Begin Node Type=RigVMModelFunctionNode Name="Function_AdjustPelvis"
      FunctionName="MathFloatMin"
      Pins(0)=(Name="A",Direction=Input,Value="Foot_Offset_L")
      Pins(1)=(Name="B",Direction=Input,Value="Foot_Offset_R")
      Pins(2)=(Name="Result",Direction=Output)
   End Node

   # Step 2: Offset pelvis down by lowest stepping value
   Begin Node Type=RigVMModelFunctionNode Name="Function_SetPelvisBone"
      FunctionName="SetBoneTransform"
      Pins(0)=(Name="Bone",Direction=Input,DefaultValue="pelvis")
      Pins(1)=(Name="Space",Direction=Input,DefaultValue="EBoneControlTransformSpace::LocalSpace")
   End Node

   # Step 3: FBIK solver for left/right leg chains
   Begin Node Type=RigVMModelFunctionNode Name="Function_FBIKSolver"
      FunctionName="FullBodyIK"
      Pins(0)=(Name="Root",Direction=Input,DefaultValue="pelvis")
      Pins(1)=(Name="Effectors",Direction=Input)
         # Effector 0: left foot target aligned to trace floor height
         # Effector 1: right foot target aligned to trace floor height
   End Node
End Object
```

**Polish**:
- **Pelvis Z-Smoothing Gate**: never pass instant trace adjustments to the pelvis. Interp speed 7.0 on sudden down-steps, 15.0 stepping up — mimics joint compression curves.
- **Foot-Rotation Matching**: extract `HitNormal` from the trace, convert via `MakeRotFromX`, pass to Control Rig to angle boot soles flat against slopes.

## Procedural Locomotion Pivot / Stop Blending

Tracks sudden divergence between acceleration vector and velocity vector to trigger directional pivot and quick-stop animations.

Variables: `Is_Pivot_Active` (Bool); `Pivot_Angle` (Float — relative turnaround angle, e.g. 180° U-turn); `Distance_To_Stop` (Float — predicted braking distance).

```
Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CustomEvent Name="Event_AnalyzeLocomotionVectors"
   CustomFunctionName="Execute_AnalyzeLocomotionVectors"
   Pins(0)=(PinName="Then",PinType=(PinCategory="exec"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_GetVelocity"
   FunctionReference=(Name="GetVelocity")
   Pins(0)=(PinName="Target",PinType=(PinCategory="object",PinSubCategoryClass=/Script/Engine.Actor),Direction="EGPD_Input")
   Pins(1)=(PinName="ReturnValue",PinType=(PinCategory="struct",PinSubCategory="Vector"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_GetCurrentAcceleration"
   FunctionReference=(Name="GetCurrentAcceleration")
   Pins(0)=(PinName="Target",PinType=(PinCategory="object",PinSubCategoryClass=/Script/Engine.CharacterMovementComponent"),Direction="EGPD_Input")
   Pins(1)=(PinName="ReturnValue",PinType=(PinCategory="struct",PinSubCategory="Vector"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_CalculateDotProduct"
   FunctionReference=(Name="Dot_VectorVector")
   Pins(0)=(PinName="A",PinType=(PinCategory="struct",PinSubCategory="Vector"),Direction="EGPD_Input")
   Pins(1)=(PinName="B",PinType=(PinCategory="struct",PinSubCategory="Vector"),Direction="EGPD_Input")
   Pins(2)=(PinName="ReturnValue",PinType=(PinCategory="float"),Direction="EGPD_Output")
End Object
```

Flow: each frame, dot(normalized Velocity, Acceleration). Dot < **-0.2** → stick pushed opposite momentum → compute `Pivot_Angle`, set `Is_Pivot_Active`. When acceleration drops to zero at speed: braking distance = `(Velocity²) / (2 × BrakingDeceleration)` → push into Orientation/Stride Warping node in the AnimGraph to match foot strides to the braking path — eliminates foot sliding.

**Polish — Dynamic Lean & Weight**: drive root-bone offsets in the Control Rig from directional vector deltas; lean spine/pelvis into high-momentum turns via Spring Interpolation for grounded inertia.

## Procedural Aim-Offset Look-At Tracking (Multi-Spine Distribution)

Distributes a look-at rotation across a chain (`spine_01..03, neck_01, head`) for a natural torso bend.

Inputs: `Aim_Target_World` (Vector); `Aim_Weight` (Float 0–1); `Spine_Distribution_Array` (Bone Name array, default `[spine_01, spine_02, spine_03, neck_01, head]`).

```
Begin Object Class=/Script/ControlRig.RigVMGraph Name="RigVMGraph_ProceduralAimOffset"
   # Step 1: Look direction from chest origin to target
   Begin Node Type=RigVMModelFunctionNode Name="Function_AimAimItem"
      FunctionName="AimToTarget"
      Pins(0)=(Name="Item",Direction=Input,DefaultValue="spine_03")
      Pins(1)=(Name="Target",Direction=Input,Value="Aim_Target_World")
      Pins(2)=(Name="AimAxis",Direction=Input,DefaultValue="(X=1.0,Y=0.0,Z=0.0)")
      Pins(3)=(Name="UpAxis",Direction=Input,DefaultValue="(X=0.0,Y=0.0,Z=1.0)")
      Pins(4)=(Name="ResultRotation",Direction=Output)
   End Node

   # Step 2: Distribute rotation across the spinal chain
   Begin Node Type=RigVMModelFunctionNode Name="Function_DistributeRotation"
      FunctionName="DistributeRotation"
      Pins(0)=(Name="Bones",Direction=Input,Value="Spine_Distribution_Array")
      Pins(1)=(Name="Rotation",Direction=Input) # Connected to ResultRotation
      Pins(2)=(Name="Weight",Direction=Input,Value="Aim_Weight")
      Pins(3)=(Name="CompComponentClass",Direction=Input,DefaultValue="EControlRigRotationOrder::XYZ")
   End Node
End Object
```

Spine bones take smaller rotation percentages; neck/head take larger — anatomy-mimicking. Clamp angles so the spine never twists backward when the target moves behind.

**Polish — Velocity-Gated Eye Tracking**: when the target moves fast across screen, head tracks quickly while slower torso joints lag behind via weight modulation.

## IK-Driven Foot Lock (Anti-Slide During Sharp Turns)

Variables: `Left_Foot_Lock_Location` / `Right_Foot_Lock_Location` (Vector, cached); `Is_Left_Foot_Planted` (Bool — driven by animation foot-down curves).

```
Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CustomEvent Name="Event_EvaluateFootLockState"
   CustomFunctionName="Execute_EvaluateFootLockState"
   Pins(0)=(PinName="Then",PinType=(PinCategory="exec"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_Branch Name="Branch_CheckFootCurve"
   Pins(0)=(PinName="Condition",PinType=(PinCategory="bool"),Direction="EGPD_Input")
   Pins(1)=(PinName="True",PinType=(PinCategory="exec"),Direction="EGPD_Output")
   Pins(2)=(PinName="False",PinType=(PinCategory="exec"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_GetAnimCurveValue"
   FunctionReference=(Name="GetCurveValue")
   Pins(0)=(PinName="Target",PinType=(PinCategory="object",PinSubCategoryClass=/Script/Engine.AnimInstance"),Direction="EGPD_Input")
   Pins(1)=(PinName="CurveName",PinType=(PinCategory="name"),Direction="EGPD_Input",DefaultValue="Foot_Down_L")
   Pins(2)=(PinName="ReturnValue",PinType=(PinCategory="float"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_StoreFootWorldCoordinates"
   FunctionReference=(Name="GetSocketLocation")
   Pins(0)=(PinName="Target",PinType=(PinCategory="object",PinSubCategoryClass=/Script/Engine.SkeletalMeshComponent"),Direction="EGPD_Input")
   Pins(1)=(PinName="InSocketName",PinType=(PinCategory="name"),Direction="EGPD_Input",DefaultValue="foot_l")
   Pins(2)=(PinName="ReturnValue",PinType=(PinCategory="struct",PinSubCategory="Vector"),Direction="EGPD_Output")
End Object
```

Flow: read `Foot_Down_L`/`Foot_Down_R` curves each frame; value > **0.5** = foot planted → first planted frame, cache world position → during capsule rotation, compute delta between moving bone and stored lock → feed into Control Rig leg solver pulling the foot back to the anchor → curve drops below 0.5 → release.

**Polish — Anatomical Knee Protection**: dot product between capsule forward and locked foot bone; angle > **45.0°** → auto-release the lock and take a quick procedural adjustment step, protecting knee-twist visuals.

## Cliff-Edge Balancing & Fall Recovery

Predictive scanning: each frame, look-ahead point = position + velocity × 0.25s, trace down 250 cm from it. No hit (or rapid normal change) = steep ledge → update `Edge_Proximity_Alpha`, cache `Ledge_Normal_Vector`, push to AnimInstance thread.

Variables: `Edge_Proximity_Alpha` (0 flat → 1 at ledge); `Ledge_Normal_Vector` (Vector); `Drop_Severity_Depth` (Float — step-down vs lethal cliff).

```
Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CustomEvent Name="Event_ScanTerrainEdge"
   CustomFunctionName="Execute_TerrainEdgeScan"
   Pins(0)=(PinName="Then",PinType=(PinCategory="exec"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_PredictiveForwardTrace"
   FunctionReference=(Name="LineTraceSingle")
   Pins(0)=(PinName="execute",PinType=(PinCategory="exec"),Direction="EGPD_Input")
   Pins(1)=(PinName="Start",PinType=(PinCategory="struct",PinSubCategory="Vector"),Direction="EGPD_Input") # Pelvis + (Velocity * 0.25)
   Pins(2)=(PinName="End",PinType=(PinCategory="struct",PinSubCategory="Vector"),Direction="EGPD_Input")   # Start - (0, 0, 250)
   Pins(3)=(PinName="TraceChannel",PinType=(PinCategory="byte",PinSubCategory="/Script/Engine.ECollisionChannel"),Direction="EGPD_Input",DefaultValue="ECC_Visibility")
   Pins(4)=(PinName="OutHit",PinType=(PinCategory="struct",PinSubCategory="HitResult"),Direction="EGPD_Output")
   Pins(5)=(PinName="then",PinType=(PinCategory="exec"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_MapProximityRange"
   FunctionReference=(Name="MapRangeClamped")
   Pins(0)=(PinName="Value",PinType=(PinCategory="float"),Direction="EGPD_Input")
   Pins(1)=(PinName="InLeft",PinType=(PinCategory="float"),Direction="EGPD_Input",DefaultValue="75.0")  # Warning threshold
   Pins(2)=(PinName="InRight",PinType=(PinCategory="float"),Direction="EGPD_Input",DefaultValue="15.0") # Foot over ledge limit
   Pins(3)=(PinName="OutLeft",PinType=(PinCategory="float"),Direction="EGPD_Input",DefaultValue="0.0")
   Pins(4)=(PinName="OutRight",PinType=(PinCategory="float"),Direction="EGPD_Input",DefaultValue="1.0")
   Pins(5)=(PinName="ReturnValue",PinType=(PinCategory="float"),Direction="EGPD_Output")
End Object
```

```
Begin Object Class=/Script/ControlRig.RigVMGraph Name="RigVMGraph_LedgeEdgeBalancing"
   # Step 1: Lower center of gravity
   Begin Node Type=RigVMModelFunctionNode Name="Function_LowerCenterOfMass"
      FunctionName="SetBoneTransform"
      Pins(0)=(Name="Bone",Direction=Input,DefaultValue="pelvis")
      Pins(1)=(Name="Transform",Direction=Input) # Down Z (up to -25.0cm) x Edge_Proximity_Alpha
      Pins(2)=(Name="Space",Direction=Input,DefaultValue="EBoneControlTransformSpace::LocalSpace")
      Pins(3)=(Name="Weight",Direction=Input,Value="Edge_Proximity_Alpha")
   End Node

   # Step 2: Tilt spine backward away from the ledge normal
   Begin Node Type=RigVMModelFunctionNode Name="Function_TiltSpineBackwards"
      FunctionName="SetBoneTransform"
      Pins(0)=(Name="Bone",Direction=Input,DefaultValue="spine_02")
      Pins(1)=(Name="Transform",Direction=Input) # Pitch/Roll along inverted Ledge_Normal_Vector
      Pins(2)=(Name="Space",Direction=Input,DefaultValue="EBoneControlTransformSpace::WorldSpace")
      Pins(3)=(Name="Weight",Direction=Input,Value="Edge_Proximity_Alpha")
   End Node

   # Step 3: FBIK — clamp rear supporting foot, let cliff-side foot adjust
   Begin Node Type=RigVMModelFunctionNode Name="Function_LedgeFBIKSolver"
      FunctionName="FullBodyIK"
      Pins(0)=(Name="Root",Direction=Input,DefaultValue="pelvis")
      Pins(1)=(Name="Effectors",Direction=Input)
         # Effector 0: supporting_foot -> clamped to safe terrain hit location
         # Effector 1: cliff_edge_foot -> rotated slightly downward, balancing over ledge
      Pins(2)=(Name="Weight",Direction=Input,Value="Edge_Proximity_Alpha")
   End Node
End Object
```

**Polish**:
- **Asymmetric Spring-Driven Windmilling**: at alpha = 1.0 while stationary, pipe low-frequency high-amplitude Perlin noise into clavicle/hand controls — subtle arm-flailing equilibrium fight.
- **Velocity Intercept Recovery Lockout**: sprinting toward the edge past the safe-stop window → override input, lock controls 0.85 s, trigger hard-braking slide-to-stop or edge-catch recovery montage.
