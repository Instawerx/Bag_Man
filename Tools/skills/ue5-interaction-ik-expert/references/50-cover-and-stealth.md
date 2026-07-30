# 50 — Cover & Stealth: Cover Peeking, Blind Firing, Corner Slipping, Wall Lean

## Procedural Cover-Peeking & Corner-Leaning

Variables: `Cover_Edge_Normal` (Vector — lean direction); `Cover_Type_Enum` (Byte: Low_Cover / High_Cover); `Lean_Amount_Alpha` (Float 0–1, from thumbstick extension).

```
Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CustomEvent Name="Event_ScanCoverEnvironment"
   CustomFunctionName="Execute_CoverEnvironmentScan"
   Pins(0)=(PinName="Then",PinType=(PinCategory="exec"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_CoverCapsuleSweep"
   FunctionReference=(Name="CapsuleTraceSingle")
   Pins(0)=(PinName="execute",PinType=(PinCategory="exec"),Direction="EGPD_Input")
   Pins(1)=(PinName="Start",PinType=(PinCategory="struct",PinSubCategory="Vector"),Direction="EGPD_Input")
   Pins(2)=(PinName="End",PinType=(PinCategory="struct",PinSubCategory="Vector"),Direction="EGPD_Input")
   Pins(3)=(PinName="Radius",PinType=(PinCategory="float"),Direction="EGPD_Input",DefaultValue="35.0")
   Pins(4)=(PinName="HalfHeight",PinType=(PinCategory="float"),Direction="EGPD_Input",DefaultValue="80.0")
   Pins(5)=(PinName="TraceChannel",PinType=(PinCategory="byte",PinSubCategory="/Script/Engine.ECollisionChannel"),Direction="EGPD_Input",DefaultValue="ECC_Visibility")
   Pins(6)=(PinName="OutHit",PinType=(PinCategory="struct",PinSubCategory="HitResult"),Direction="EGPD_Output")
   Pins(7)=(PinName="ReturnValue",PinType=(PinCategory="bool"),Direction="EGPD_Output")
   Pins(8)=(PinName="then",PinType=(PinCategory="exec"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_EvaluateCoverHeight"
   FunctionReference=(Name="BreakVector")
   Pins(0)=(PinName="InVec",PinType=(PinCategory="struct",PinSubCategory="Vector"),Direction="EGPD_Input")
   Pins(1)=(PinName="X",PinType=(PinCategory="float"),Direction="EGPD_Output")
   Pins(2)=(PinName="Y",PinType=(PinCategory="float"),Direction="EGPD_Output")
   Pins(3)=(PinName="Z",PinType=(PinCategory="float"),Direction="EGPD_Output")
End Object
```

Flow: approach barrier → forward capsule sweep; hit actor implements the interaction interface → snap into resting cover state → break `HitResult.ImpactPoint`: obstacle max Z below chest line → `Low_Cover`; above → scan laterally for nearest left/right edge → on aim/stick input update `Lean_Amount_Alpha`:
- **Low Cover**: vertical translation offset (**+Z up to 35.0 cm**) on `spine_01` to rise over the barricade.
- **High Cover**: rotation offset (up to **25.0° Roll/Yaw**) on `spine_02` along `Cover_Edge_Normal` to lean around the corner, legs hidden.

**Polish — Camera Spring Arm Handshake**: pull the spring arm opposite the lean via `SpringInterpTo` driven by `Lean_Amount_Alpha` — smooth over-the-shoulder framing, no camera crashing into walls.

## Stealth Cover-Slipping & Blind Firing

Variables: `Target_Slip_Point` (Vector — junction corner node); `Blind_Fire_Alpha` (Float 0–1); `Is_Slipping_Active` (Bool — locks locomotion during corner execution).

```
Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CustomEvent Name="Event_PredictCornerSlipVector"
   CustomFunctionName="Execute_CornerSlipPrediction"
   Pins(0)=(PinName="Then",PinType=(PinCategory="exec"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_LateralWallTrace"
   FunctionReference=(Name="LineTraceSingle")
   Pins(0)=(PinName="execute",PinType=(PinCategory="exec"),Direction="EGPD_Input")
   Pins(1)=(PinName="Start",PinType=(PinCategory="struct",PinSubCategory="Vector"),Direction="EGPD_Input")
   Pins(2)=(PinName="End",PinType=(PinCategory="struct",PinSubCategory="Vector"),Direction="EGPD_Input")
   Pins(3)=(PinName="OutHit",PinType=(PinCategory="struct",PinSubCategory="HitResult"),Direction="EGPD_Output")
   Pins(4)=(PinName="then",PinType=(PinCategory="exec"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_RegisterSlipWarpTarget"
   FunctionReference=(Name="AddOrUpdateWarpTargetFromLocationAndRotation")
   Pins(0)=(PinName="execute",PinType=(PinCategory="exec"),Direction="EGPD_Input")
   Pins(1)=(PinName="Target",PinType=(PinCategory="object",PinSubCategoryClass=/Script/MotionWarping.MotionWarpingComponent"),Direction="EGPD_Input")
   Pins(2)=(PinName="WarpTargetName",PinType=(PinCategory="name"),Direction="EGPD_Input",DefaultValue="Warp_Cover_Slip")
   Pins(3)=(PinName="TargetLocation",PinType=(PinCategory="struct",PinSubCategory="Vector"),Direction="EGPD_Input")
   Pins(4)=(PinName="TargetRotation",PinType=(PinCategory="struct",PinSubCategory="Rotator"),Direction="EGPD_Input")
End Object
```

```
Begin Object Class=/Script/ControlRig.RigVMGraph Name="RigVMGraph_CoverBlindFiring"
   # Step 1: Offset weapon shoulder control up/outward along barrier edge vector
   Begin Node Type=RigVMModelFunctionNode Name="Function_OffsetWeapGrip"
      FunctionName="MathVectorAdd"
      Pins(0)=(Name="A",Direction=Input) # Baseline arm control location
      Pins(1)=(Name="B",Direction=Input) # Positional offset from Blind_Fire_Alpha
      Pins(2)=(Name="Result",Direction=Output)
   End Node

   # Step 2: Rotate hand/shoulder bones toward camera crosshair position
   Begin Node Type=RigVMModelFunctionNode Name="Function_BlindFireAimSolver"
      FunctionName="AimToTarget"
      Pins(0)=(Name="Item",Direction=Input,DefaultValue="hand_r")
      Pins(1)=(Name="Target",Direction=Input,Value="Camera_Crosshair_World_Position")
      Pins(2)=(Name="Weight",Direction=Input,Value="Blind_Fire_Alpha")
   End Node
End Object
```

Flow:
1. **Corner Slipping Prediction**: at a wall edge with continued stick input toward it, fire a predictive corner trace loop reading the perpendicular wall normal to locate the safe landing corner.
2. **Slipping Warp**: register destination under `Warp_Cover_Slip` → play corner-slip montage, root path warps around the junction while keeping stealth stance.
3. **Blind Firing**: fire trigger without aim-peek → `Blind_Fire_Alpha` → 1.0; rig shifts right arm (`shoulder_r`/`hand_r`) up over low cover or outward past high cover (capsule stays protected), Aim Solver locks the barrel onto the camera crosshair — no static blind-fire animations needed.

**Polish — Tactile Suppression Camera Lag**: while blind firing, apply asymmetric noise to the camera spring arm — heavier, unstable kickback and subtle lag, mechanically rewarding aimed peeks over un-aimed blind suppression.

## Dynamic Wall Lean & Hand Placement (Narrow Corridors)

Variables: `Wall_Hit_Location` (Vector); `Wall_Surface_Normal` (Vector); `Wall_Proximity_Alpha` (Float, 0.0 open space → 1.0 max proximity).

```
Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CustomEvent Name="Event_ScanNarrowCorridors"
   CustomFunctionName="Execute_NarrowCorridorScan"
   Pins(0)=(PinName="Then",PinType=(PinCategory="exec"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_LineTraceLeftAndRight"
   FunctionReference=(Name="LineTraceSingle")
   Pins(0)=(PinName="execute",PinType=(PinCategory="exec"),Direction="EGPD_Input")
   Pins(1)=(PinName="Start",PinType=(PinCategory="struct",PinSubCategory="Vector"),Direction="EGPD_Input")
   Pins(2)=(PinName="End",PinType=(PinCategory="struct",PinSubCategory="Vector"),Direction="EGPD_Input")
   Pins(3)=(PinName="TraceChannel",PinType=(PinCategory="byte",PinSubCategory="/Script/Engine.ECollisionChannel"),Direction="EGPD_Input",DefaultValue="ECC_Visibility")
   Pins(4)=(PinName="OutHit",PinType=(PinCategory="struct",PinSubCategory="HitResult"),Direction="EGPD_Output")
   Pins(5)=(PinName="ReturnValue",PinType=(PinCategory="bool"),Direction="EGPD_Output")
   Pins(6)=(PinName="then",PinType=(PinCategory="exec"),Direction="EGPD_Output")
End Object

Begin Object Class=/Script/BlueprintGraph.EdGraphNode_CallFunction Name="Call_CalculateProximityAlpha"
   FunctionReference=(Name="MapRangeClamped")
   Pins(0)=(PinName="Value",PinType=(PinCategory="float"),Direction="EGPD_Input")
   Pins(1)=(PinName="InLeft",PinType=(PinCategory="float"),Direction="EGPD_Input",DefaultValue="90.0")  # Start interaction distance
   Pins(2)=(PinName="InRight",PinType=(PinCategory="float"),Direction="EGPD_Input",DefaultValue="45.0") # Hand flat wall distance
   Pins(3)=(PinName="OutLeft",PinType=(PinCategory="float"),Direction="EGPD_Input",DefaultValue="0.0")
   Pins(4)=(PinName="OutRight",PinType=(PinCategory="float"),Direction="EGPD_Input",DefaultValue="1.0")
   Pins(5)=(PinName="ReturnValue",PinType=(PinCategory="float"),Direction="EGPD_Output")
End Object
```

Flow: two concurrent lateral traces outward from the pelvis (left/right local axes, **100.0 cm**) → on static-wall hit, map distance; below **90.0 cm** the alpha ramps toward 1.0 → Control Rig:
- **Hand IK**: Basic IK pins the palm flush against the wall; hand rotation matched to the wall normal so fingers conform.
- **Procedural Spine Lean**: `Set Bone Transform` on `spine_02` tilts the torso up to **12.0°** away from the wall — natural squeezing posture.

**Polish — Hand-Over-Surface Sliding**: feed forward velocity as a cyclical offset to the hand target plus an Offset Vector Noise node — the palm slides fluidly along bricks/cracks instead of freezing rigidly.
